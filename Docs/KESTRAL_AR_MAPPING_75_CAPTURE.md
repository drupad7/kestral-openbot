# Robot-side AR boundary drafts and 75% mission photographs

Date: 2026-11-06. Branch: `feature/phone-b-kestral-integration`.

## Audit and scope

The repository already contained extensive local modifications and untracked implementation files. They were retained. Neither Firebase JSON file nor the TFLite model was changed. No firmware, hardware, commits, pushes, or destructive Git operations were introduced in this phase.

| Component and actual implementation | Audited state / result in this phase |
| --- | --- |
| Robot `MainActivity`, `CameraFullscreenController`, manifest configuration handling | Existing portrait navigation, fullscreen landscape, fullscreen-first Back, and explicit root exit confirmation retained. Device UI regression passed on A069. Detail navigation deliberately stops/releases motor ownership where required. Retaining a connection does not authorize unsupervised motion. |
| Controller `ControllerActivity`, manifest | Existing landscape presentation retained; no orientation rewrite. New APK builds; updated Controller was not installed on I2202 because that device became unreachable. |
| `PhoneController`, `WebRtcServer`, `WasteAnalysisSink`, `WasteModelService` | Existing WebRTC/manual and mode-specific inference architecture reused. Model/preprocessing/decoding unchanged. Added explicit exclusive AR camera handover through the same native video source/tracks. |
| `ArCore` in legacy PointGoal navigation | Existing separate AR GL camera owner is unsuitable for simultaneous use with WebRTC; it was not started alongside either camera. |
| `IndoorMapRuntime`, shared `IndoorMapStore`, `MapClient`, Controller `IndoorMappingPanel` | Previously visual snapshot polygons only. Added Robot pose recording, metric drafts, catalog/preview support, and metric planning approval. Existing authoritative Robot storage, revision checks, Controller cache, rename/delete/tombstones retained. Physical mapping accuracy and two-phone mapping workflow are not hardware validated. |
| `CoveragePlanner`, `CoverageGeometry`, `CoveragePlanningController`, `AutonomousScanController` | Existing geometry/safety and preview-only state machine reused. Metric drafts now produce dimension-aware preview segments with explicit approval. Physical execution remains disabled. |
| `Vehicle`, `ControlModeSession`, `ManualDriveCoordinator`, `FreeRoamFragment` | Existing command transport, motor leases, emergency priority, release/timeout behavior reused. Mapping controls do not create a second motor system. Added tracking-loss authorization checks. No physical motor test performed. |
| Sonar, GPS, heading and route safety | Existing implementations retained. No new sensor capability assumed. One forward ultrasonic sensor does not establish clearance around the robot or through turns. GPS/heading cannot establish indoor AR relocalization. |
| `WasteReportRuntime.observed`, `ReportRepository.frame`, `ScanControls` | Existing scan lifecycle, camera-frame capture, GPS capture, deduplication and asynchronous persistence reused; mission photo threshold changed to 0.75 independently of default live threshold 0.65. |
| `MissionReportsPanel`, `ReportRepository` review/`FINALIZE_REVIEW`, `ReportPdf`, `ReportDocuments`, `ReportClient` | Existing Confirm/Remove/Categories review, approved-only finalization, PDF format, public Documents publication, bounded verified report/image transfers and persistent pending queue preserved. Device PDF tests use fixture detections/photos and simulated messaging, not new live waste observations or a two-phone file transfer. |

## AR camera and pose evidence

`ArMappingCapturer` becomes the sole camera owner only after an explicit AR Prepare operation. It creates one ARCore session and forwards its CPU YUV images to the existing WebRTC `VideoSource`. The normal Camera2 capturer is stopped/disposed first. On leaving mapping, the AR session/EGL resources are released before normal Camera2 resumes. The existing peer, tracks and inference consumers are reused; no extra TFLite interpreter or second concurrent camera session is created.

The raw frame retains sensor/display rotation metadata and passes through the existing physical mounting orientation correction. CPU plane copies respect row and pixel strides. Pose evidence requires the AR frame and CPU image timestamps to agree within 2 ms. Unsupported crops/formats and missing images cannot supply a boundary point. `ArPoseStore` keeps bounded frame-keyed evidence for photographs and refuses stale (>500 ms) or unstable positions. Recording requires at least five samples spanning 400 ms with no more than 2 cm spread in the recent stability window. These are filters, **not validated accuracy claims**.

Prepare requires three explicitly measured camera-to-robot-floor-reference offsets in optical camera axes (X right, Y up, Z backward), metres, plus a measurement acknowledgement. No zeros or robot dimensions are assumed. The camera pose composes that translation and projects it to a tracked upward-facing observed floor plane; floor-height agreement must be within 10 cm. An anchor supplies a session-local X/Z metric frame with a fresh reference UUID. Mount calibration remains `USER_MEASURED_UNVERIFIED`. This does not calibrate chassis heading, validate drift, or persist AR relocalization. The native diagnostic test intentionally supplies no offsets and therefore never publishes a robot position.

Controller commands record the Robot pose when received after the operator stops and waits. They do not turn a tap on delayed video into a floor point. Polygons/routes are drawn in a separate top-down metric view; there is no claimed perspective AR overlay on delayed WebRTC video. Thus no guessed screen-to-world or frame-latency compensation is used.

API references: [ARCore Session](https://developers.google.com/ar/reference/java/com/google/ar/core/Session), [Frame and CPU image timestamps](https://developers.google.com/ar/reference/java/com/google/ar/core/Frame), [Camera tracking/pose](https://developers.google.com/ar/reference/java/com/google/ar/core/Camera).

## Boundary workflow, storage and safety

Controller Autonomous Mission → New offers the existing live renderer, measured-mount setup, explicit Enable Manual Driving, press-and-hold directions, Stop, Emergency Stop, Start/Add Point/End/Undo/Save/Cancel. Blank panel areas absorb touches rather than passing them to obscured driving controls. Commands use the existing acknowledged gateway; recording commands have IDs to reject conflicting duplicate actions. Manual selection from Waste Auto waits for its acknowledged mode transition.

Start records the current Robot floor position only. Add Point requires a distinct position at least 10 cm from every retained vertex. End is enabled after Start plus two distinct added points, records the final observed position and closes the polygon to Start. An End at/near Start or the last vertex does not add a duplicate vertex. Invalid geometry leaves the draft open. Validation checks finite metric coordinates, 3–32 vertices, distinct points, intersections and positive sufficient area. UI shows distances, perimeter, area, Start/End and tracking status.

Save names an authoritative Robot `METRIC_AR_DRAFT` with vertices, observations, start/end, timestamps, reference UUID, mount/tracking metadata, geometry measurements and revision. Files under `files/indoor_maps` use existing atomic persistence. The active draft is separately atomic under `files/indoor_mapping_state/draft.json`. An interrupted open recording is recovered as `INTERRUPTED`, not silently resumed in a new world frame. Closed drafts remain reviewable. Saved maps use existing `MapClient` cache synchronization and revision-based rename/delete. Visual-only snapshot drafts remain available and never acquire fabricated metric coordinates.

Tracking loss issues Stop and prevents subsequent nonzero mapping commands under the old manual authorization. Manual must be explicitly enabled again after fresh tracking. Existing motor ownership, communication-loss behavior and emergency latch remain authoritative. Start/Stop Scanning do not start navigation or motor output. Leaving AR mapping restores the normal camera; pending drafts are preserved unless explicitly cancelled.

## Coverage preview and autonomy limitations

`PLAN` on a metric map reuses `CoveragePlanner` with explicitly entered width, length, spacing, boundary clearance, uncertainty, minimum turn radius, lane angle and pivot assumption. It validates metric geometry and returns existing lane/connector segments inset for the configured footprint and uncertainty. Invalid/unreachable geometry is rejected. A plan ID and map revision gate explicit `APPROVE_PLAN`; changing/deleting the map invalidates approval. Waste Auto can load an approved preview through existing `INDOOR_PREVIEW` and preview-only scan state transitions.

The boundary-recording Start/End are perimeter capture positions, not proven safe interior navigation goals. Preview proposes safe lane endpoints separately. An approach/exit path from the recorded perimeter positions is not validated or executed. Current robot pose is shown only when fresh evidence belongs to the same reference frame; otherwise it is absent and relocalization is required.

All maps/plans remain `nav_valid=false` / `executable=false`. Approval approves geometry only. Persistent relocalization, calibrated mount/chassis pose and heading, reliable drift bounds, obstacle coverage, braking distance, an indoor route follower, and actual low-speed safety validation remain missing. There is no supported physical Start bypass. Existing obstacle/tracking/disconnection safety must not be interpreted as complete autonomous avoidance. No MPU6050 or other hardware was added.

## Photographs, review and reports

Live display keeps its independent configurable default of 0.65. `ReportRepository.RECORD_THRESHOLD=0.75` is enforced both at the observation adapter and persistence boundary; 0.749 is rejected and 0.75 included. Only active scans accept photographs. Existing deduplication, best-photo selection, asynchronous image writes and image transfer are reused; different objects are not merged solely by category. Exact-frame AR capture metadata is optional; absent matching evidence stays absent. Best-photo bounding box and dimensions are stored separately from the latest tracking box. Resumed old missions retain a threshold-change audit when needed.

GPS remains an available robot capture fix, never an exact waste position. An estimated indoor AR capture is explicitly labelled with local reference and unverified mount; missing GPS stays `GPS unavailable`. No coordinates are invented. Deduplication remains imperfect; existing review is necessary.

Stop Scanning drains accepted writes and enters review. Confirm/Remove/Categories and review persistence are retained. Final PDF generation requires `FINALIZE_REVIEW`, excludes rejected records and uses corrected categories. Existing heading/table, captured photographs, public `Documents/Kestral OpenBot/Reports/`, `dd-MM-yyyy (HH-mm).pdf` collision handling, content URIs, verified chunk transfer, acknowledgements/retries and offline queue are unchanged. AR capture labels were added to the location formatter; the PDF layout was not redesigned.

## Exact implementation file inventory for this phase

Paths below are relative to `android/`. Files may already have been locally modified/untracked before this phase; this inventory does not claim all of their Git diff was authored now.

New production classes:

- `reporting/src/main/java/org/openbot/maps/MetricBoundary.java`
- `reporting/src/main/java/org/openbot/maps/MetricRecording.java`
- `robot/src/main/java/org/openbot/app/robot/robot/coverage/ArMappingCapturer.java`
- `robot/src/main/java/org/openbot/app/robot/robot/coverage/ArPoseStore.java`

Updated existing implementation:

- `reporting/src/main/java/org/openbot/maps/IndoorMapStore.java`
- `reporting/src/main/java/org/openbot/reports/ReportRepository.java`
- `reporting/src/main/java/org/openbot/reports/ReportPdf.java`
- `reporting/src/main/java/org/openbot/reports/ScanControls.java`
- `reporting/src/main/java/org/openbot/reports/MissionReportsPanel.java`
- `robot/src/main/java/org/openbot/app/robot/env/PhoneController.java`
- `robot/src/main/java/org/openbot/app/robot/env/WebRtcServer.java`
- `robot/src/main/java/org/openbot/app/robot/main/MainActivity.java`
- `robot/src/main/java/org/openbot/app/robot/robot/FreeRoamFragment.java`
- `robot/src/main/java/org/openbot/app/robot/robot/WasteClassificationFragment.java`
- `robot/src/main/java/org/openbot/app/robot/robot/coverage/IndoorMapRuntime.java`
- `robot/src/main/java/org/openbot/app/robot/robot/coverage/CoveragePlanningController.java`
- `robot/src/main/java/org/openbot/app/robot/robot/waste/WasteReportRuntime.java`
- `controller/src/main/java/org/openbot/controller/ControllerActivity.kt`
- `controller/src/main/java/org/openbot/controller/IndoorMappingPanel.kt`
- `controller/src/main/java/org/openbot/controller/WasteModePanel.kt`

New tests:

- `robot/src/test/java/org/openbot/maps/MetricRecordingTest.java`
- `robot/src/test/java/org/openbot/app/robot/robot/coverage/ArPoseStoreTest.java`
- `robot/src/test/java/org/openbot/app/robot/robot/coverage/ArPlaneCopyTest.java`
- `robot/src/test/java/org/openbot/app/robot/robot/coverage/MetricPlanRuntimeTest.java`
- `robot/src/androidTest/java/org/openbot/app/robot/robot/coverage/ArCameraDeviceTest.java`

Updated tests:

- `robot/src/test/java/org/openbot/reports/MissionReportsTest.java`
- `robot/src/test/java/org/openbot/reports/ScanReportsUiTest.java`
- `controller/src/test/java/org/openbot/controller/IndoorMappingPanelTest.kt`
- `robot/src/androidTest/java/org/openbot/reports/ScanDocumentsDeviceTest.java`

This document and `ar-*.log` validation outputs were created in the workspace. Other existing local files remain preserved.

## Executed validation

Gradle, offline, no emulator:

```
.\android\gradlew.bat -p android :robot:testDebugUnitTest :controller:testDebugUnitTest :robot:assembleDebug :controller:assembleDebug :robot:assembleDebugAndroidTest --offline --console=plain
```

Final full run: BUILD SUCCESSFUL; Robot **441** JVM tests, Controller **53**, no failures (494 total). Log: `ar-full-build.log`. Native-test-only rebuild after the SAF test correction also succeeded (`ar-native-test-build.log`). Existing Gradle/deprecation/Android toolchain warnings remain; this is not a claim of warning-free builds.

Physical A069 (Nothing), installed updated Robot and test APKs with `adb install -r`, preserving app data:

- New stationary AR camera test: passed. ARCore `SUPPORTED_INSTALLED`; actual CPU camera frames reached a native WebRTC video source, image/AR timestamps associated, release stopped callbacks, ordinary Camera2 resumed afterward. No assumed mount, floor map or movement.
- Full instrumentation rerun: **OK (16 tests)**, `ar-a069-native-final.log`. Includes native model execution, synthetic overlay/rotation and fullscreen/panel interactions, native stream cleanup, zero/one/multiple-detection PDFs with fixture photographs, Documents publication and simulated verified report synchronization.
- First broader run found four Documents tests assuming MediaStore even though the phone already had an SAF folder selection. Queries and fixture cleanup failed. Test assertions now check provider-appropriate document IDs or MediaStore paths, and use `DocumentsContract.deleteDocument` for SAF. Production storage behavior was unchanged. The corrected full rerun passed.
- No emulator, no motor commands, no real-world accuracy/deduplication/physical-navigation claim. Synthetic detections/fixtures in tests are not production telemetry.

I2202 was not reachable through ADB at final validation; an explicit connection attempt to its advertised wireless endpoint timed out. Consequently the updated Controller APK and a fresh two-phone end-to-end mapping/video/report-transfer check remain pending. Earlier two-phone results in other documents are historical, not validation of this AR change.

`git diff --check` passed. Both Firebase JSON SHA-256 values and model hash match their values recorded before this phase.

## Install and remaining device checks

APKs: `android/robot/build/outputs/apk/debug/robot-debug.apk` and `android/controller/build/outputs/apk/debug/controller-debug.apk`. Install Robot on A069 and Controller on I2202; use `adb -s <device> install -r <apk>` to preserve data. Robot is already updated. Reconnect I2202 before updating/checking Controller.

Before trusting physical maps, measure the rigid optical-camera offsets, test observed floor acquisition, manually record a surveyed boundary at stopped positions, measure drift/closure, exercise loss/occlusion/reconnect and repeated camera handover. Confirm two-phone stream rotation/boxes, manual release/EStop, acknowledged mode switches, actual >=75% waste photos and review/reconnect/PDF transfer. Validate chassis pose, relocalization, obstacle coverage and braking at controlled low speed before implementing/enabling physical autonomous navigation. A069 camera support alone does not satisfy these conditions.
