# Navigation, fullscreen and indoor-map audit

Scope: latest request (Back navigation, fullscreen orientation, New/Load polygon mapping). Existing report/review/PDF behaviour is preserved; the earlier interrupted request to change PDF finalization is not part of this narrowed implementation.

| Component | Actual implementation before this phase | Status / evidence |
|---|---|---|
| Robot navigation | `main/MainActivity`, `MainFragment`, `nav_graph.xml` | Implemented navigation; no app-level safe Back/fullscreen handling. MainActivity destruction disconnects USB; native behaviour not verified this phase. |
| Controller navigation | `ControllerActivity`, `ScreenSelector`, existing mission screen | Implemented; Autonomous Mission directly opens old mission UI. New/Load indoor maps missing. |
| Local communication | `PhoneController`, `ConnectionSelector`, NetworkServiceConnection/NearbyConnection, event buses | Implemented; previous unit tests exist. Actual two-phone reconnect not verified this phase. |
| Video | Robot `WebRtcServer`; Controller `VideoViewWebRTC` / `SessionVideoSink` | Existing capturer/peer/renderer pipeline. Preserve it; no AR pose/frame metadata channel. |
| Waste model | `WasteModelService`, `WasteLiveScanner`, TFLite detector/decoder | Implemented; existing tests and prior device smoke evidence. No new physical waste accuracy claim. |
| Capture / deduplication | `WasteReportRuntime`, `ReportRepository.frame`, `ReportPhoto`, `DetectionMatcher` | Implemented; exact owned inference frame, approximate temporal/appearance matching. Field deduplication unverified. |
| Scanning | `ScanControls`, Robot `SCAN_START`/`SCAN_STOP`, durable commands | Implemented; independent motor controls; existing unit tests. |
| Review | `MissionReportsPanel.review`: Confirm/correct, Remove, nine-class Spinner | Implemented. Existing workflow retained. |
| PDFs / synchronization | `ReportPdf`, `ReportDocuments`, `ReportAutoSync`, `ReportClient` | Implemented with public storage API and persistent transfer state; previous 449-unit-test result. New public-storage native checks remained pending after I2202 disconnected in the preceding phase. |
| Fullscreen | `CameraFullscreenController`, used by Free Roam/Waste, CameraFragment derivatives, Point Goal, legacy camera | Layout toggle implemented; Activity landscape and fullscreen-first Back missing. |
| Preview transforms | `PhysicalCameraOrientation`, `LandscapePreviewSink`, `LandscapeCameraLayout`, waste overlay | Implemented aspect-fit / physical-orientation path; preserve pixel preprocessing and frame rotation. Device transitions require validation. |
| Boundary / coverage | `BoundaryRecorder/Store`, `CoveragePlanningController`, `CoveragePlanner` | Metric planner and GPS-recorded-boundary preview implemented. Saved AR indoor polygons missing. |
| Autonomous coverage execution | `AutonomousScanController.PREVIEW_ONLY` | Physical executor unavailable; not a verified indoor autonomous system. Must remain disabled. |
| GPS / heading | `GpsLocationSource`, `GpsNavigator`, RouteLocationSource, legacy AR helper | Real GPS interfaces exist; indoor accuracy, phone-to-chassis transform, heading and relocalization are unverified. |
| Obstacle handling | Vehicle sonar freshness, MissionSafety, RouteMotionSafety, ObstacleRecovery | Implemented stop/failure interfaces and tests. One directional ultrasonic sensor is not complete swept-path sensing. Hardware coverage not verified. |
| ARCore | `pointGoalNavigation/ArCore.resume` creates `new Session(appContext)` | Legacy independent camera ownership. No SHARED_CAMERA integration, floor hit-test endpoint for Controller, stable map persistence/relocalization or video-frame-to-pose association. |
| Encoders / dimensions | Existing firmware/telemetry may expose wheel values | Not proof the current physical robot has calibrated encoders/dimensions. No new assumption is made. |

ARCore availability alone does not prove compatible simultaneous capture, useful tracking on a robot mount, metric boundary accuracy or persistent relocalization. The [official camera-sharing guide](https://developers.google.com/ar/develop/java/camera-sharing) requires coordinated Camera2 surfaces/callbacks; the existing opaque WebRTC capturer does not provide that integration. [Availability checks](https://developers.google.com/ar/develop/java/enable-arcore) are used without creating another camera session.

Implementation plan: (1) central fullscreen lifecycle and safe Back hierarchy; (2) capability endpoint and Robot-owned visual-draft storage; (3) Controller New/Load editor over the existing renderer, with normalized display coordinates explicitly separated from world coordinates; (4) geometry/persistence/lifecycle tests and builds. Saved visual drafts never become autonomous boundaries. True AR anchoring and physical execution remain blocked until a shared-camera frame/pose pipeline, map relocalization and chassis/sensor calibration are validated on hardware.

## Implemented behaviour

Robot Back now pops detail destinations; the root asks whether to stay or explicitly stop and exit. It does not pop the last destination and leave an empty navigation graph. Back during legacy active/paused navigation pauses first and requires Stop and return before leaving; Stay retains the existing in-memory route progress. This is not durable navigation execution across process death. Scanning data uses its existing persistent interruption/recovery behaviour; leaving Waste Classification still releases inference and pauses recording rather than running hidden inference on menu screens.

The shared fullscreen component requests sensor landscape only while fullscreen, returns to portrait on exit/Back and restores the existing controls. MainActivity and the legacy camera host handle the declared orientation/size changes by relaying view re-layout; their camera/peer/model objects are not recreated by this configuration path. Other configuration changes and process loss still use normal lifecycle safety. This does not promise uninterrupted background camera access. Motors stop on app pause, and explicit exit disconnects. Foreground navigation from the manual/waste view to camera-free menus, settings or reports can retain the existing WebRTC capture and peer while detaching the local preview. Switching to another camera-owning mode or backgrounding releases capture permission. Controller orientation remains landscape.

Controller Autonomous Mission opens New/Load. New borrows the existing WebRTC renderer and freezes one actual rendered frame before allowing drawing. A frame receipt time is recorded, **not** a Robot exposure timestamp or pose. Aspect-fit inversion rejects letterbox taps. Vertices can be added, dragged or deleted; Start and End are independent interior positions. Closed geometry, unique vertices, area, self-intersections and positions are validated before Save visual draft asks for a name. Save navigable map remains disabled. Emergency Stop stays in the fixed header. Entering the editor while an active navigation mission is reported is rejected. Existing GPS mission controls remain available through a clearly labelled separate action.

Load shows named drafts with vector previews and dates, selection, rename and delete. The original reference photograph is not persisted or replayed against a new camera view. All drafts show area/relocalization unavailable and cannot start navigation. There is no metric zigzag route for image-space polygons. The existing metric coverage planner is preserved separately.

## Map persistence and synchronization

Robot `IndoorMapRuntime` handles `mapCmd` on one background executor and replies using existing status transport as `MAP_STATUS`; it creates no additional connection, AR Session or camera. `CAPABILITIES` checks ARCore availability without installing/starting ARCore. Availability alone never enables navigation. `HIT_TEST`, `SAVE_METRIC_MAP` and `START_NAVIGATION` are rejected until the missing tracking integration exists.

Robot drafts live under internal `files/indoor_maps/<map-id>.json`, using atomic writes and backup recovery. IDs derive from Robot ID plus a unique save command ID; identical retries return the same record, conflicting retries fail. Records contain name, dates, revision, normalized vertices/Start/End, reference dimensions/ID, capability metadata and explicit non-navigation flags. Metric coordinates, physical dimensions and robot pose remain unavailable. Rename uses revisions; deletion writes a tombstone so an uncertain old save cannot resurrect a deleted map.

Controller `MapClient` correlates request IDs, bounds outstanding requests, retries at most three sends and times out after 15 seconds. The source-namespaced `files/indoor_map_cache/<robot-id>/` is read-only offline. Opening Load refreshes a four-record page from the authoritative Robot, including deletion revisions; successful saves/renames/deletes update the same cache. Reconnection allows normal refresh/retry, not an assertion that every cached page is immediately up to date. This is metadata synchronization, not continuous image transfer. The existing PDF queue is untouched.

## Exact source inventory for this phase

Existing files changed (some were already locally modified/untracked before this task):

- `android/robot/src/main/AndroidManifest.xml`
- `android/robot/src/main/java/org/openbot/app/robot/OpenBotApplication.java`
- `android/robot/src/main/java/org/openbot/app/robot/main/MainActivity.java`
- `android/robot/src/main/java/org/openbot/app/robot/main/ReportsFragment.java`
- `android/robot/src/main/java/org/openbot/app/robot/original/CameraActivity.java`
- `android/robot/src/main/java/org/openbot/app/robot/customview/CameraFullscreenController.java`
- `android/robot/src/main/java/org/openbot/app/robot/env/PhoneController.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/FreeRoamFragment.java`
- `android/controller/src/main/java/org/openbot/controller/ControllerActivity.kt`
- `android/controller/src/main/java/org/openbot/controller/customComponents/VideoViewWebRTC.kt`
- `android/controller/src/test/java/org/openbot/controller/ControllerNavigationTest.kt`

New files:

- `android/reporting/src/main/java/org/openbot/maps/VisualPolygon.java`
- `android/reporting/src/main/java/org/openbot/maps/IndoorMapStore.java`
- `android/reporting/src/main/java/org/openbot/maps/MapGateway.java`
- `android/reporting/src/main/java/org/openbot/maps/MapClient.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/coverage/IndoorMapRuntime.java`
- `android/controller/src/main/java/org/openbot/controller/IndoorMappingPanel.kt`
- `android/robot/src/test/java/org/openbot/maps/IndoorMapStoreTest.java`
- `android/robot/src/test/java/org/openbot/maps/MapClientTest.java`
- `android/robot/src/test/java/org/openbot/app/robot/main/RobotBackFullscreenTest.java`
- `android/robot/src/test/java/org/openbot/app/robot/robot/coverage/IndoorCapabilityTest.java`
- `android/controller/src/test/java/org/openbot/controller/IndoorMappingPanelTest.kt`
- `docs/KESTRAL_NAVIGATION_MAPPING_AUDIT.md`

Build log: `mapping-build.log`. Gradle also updates ignored build outputs. Existing unrelated changes/untracked files are retained. No commits, pushes, firmware changes, model changes or Firebase configuration edits were made.

## Device acceptance checks still required

No Android device was connected in ADB during this phase. Robolectric tests cannot establish physical preview orientation, native WebRTC continuity, real YOLO box alignment or AR tracking quality.

1. Install both debug APKs; Robot should open portrait and Controller landscape. Connect the phones and verify normal manual controls and Emergency Stop with wheels safely raised.
2. In Free Roam and Waste Classification, repeat fullscreen entry/exit and Back. Confirm landscape fullscreen, portrait restoration, panel state, camera aspect ratio and native connection continuity. Repeat other supported camera modes.
3. Start scanning in Waste Classification, rotate via fullscreen repeatedly, stop/review and confirm records are preserved. Check real boxes against photographed waste at both orientations. Free Roam should remain inference-free.
4. Navigate to menus, settings and Reports, then back to Waste Classification. Check the Controller session persists where permitted; recording pauses safely when leaving the inference mode. Background/exit must stop movement.
5. Controller New: freeze a real video frame, draw/edit a closed polygon and interior Start/End, name/save a visual draft. Load/preview/rename/delete; disconnect and verify offline previews have no mutation/navigation actions. Reconnect and refresh to reconcile Robot revisions.
6. Confirm visual drafts never acquire autonomous motor ownership, and map editing is blocked during an active navigation mission. Validate old GPS mission access separately without assuming indoor GPS accuracy.

Real floor anchoring remains future work: coordinate the existing capturer with ARCore SharedCamera (or replace acquisition only through a validated shared pipeline), calibrate stream crop/rotation/mirror and exposure-frame/pose association, implement floor hit tests and persistent map relocalization, validate phone-to-chassis extrinsics and localization/obstacle-stop accuracy on actual hardware. Only then can metric maps feed the existing planner and a separately validated executor.

## Final validation — 5 November 2026

Executed from the repository root:

```powershell
.\android\gradlew.bat -p android :robot:testDebugUnitTest :controller:testDebugUnitTest :robot:assembleDebug :controller:assembleDebug --offline --console=plain
```

Final run succeeded: **Robot 420 tests, Controller 49 tests, zero failures/errors; both debug APKs built**. New tests cover root/detail Back, repeated fullscreen request/exit and view identity across configuration handling, polygon validation/letterbox inversion, atomic persistence/restart recovery, command idempotency, cache reconciliation/deletion, offline restrictions, unavailable metric endpoints, editor safety/disabled save without a frame, active-mission edit rejection and continued legacy mission access. These are JVM/Robolectric and simulated-transport tests, not native video or hardware tests.

Earlier runs exposed and corrected the empty-root-navigation bug, a test resource-ID typo and the old direct-mission-screen expectation. Final results supersede those failed intermediate runs. Existing Navigation/D8 and deprecated-API/Gradle warnings remain; the Navigation warning also appears in the pre-task `scan-sync-final.log`. `git diff --check` passed. Both Firebase JSON files and `waste_model.tflite` retained their audited SHA-256 hashes.

APKs:

- `android/robot/build/outputs/apk/debug/robot-debug.apk`
- `android/controller/build/outputs/apk/debug/controller-debug.apk`

Install on the appropriate devices (replace serials from `adb devices -l`):

```powershell
adb -s ROBOT_SERIAL install -r android/robot/build/outputs/apk/debug/robot-debug.apk
adb -s CONTROLLER_SERIAL install -r android/controller/build/outputs/apk/debug/controller-debug.apk
```

ADB was checked twice and listed no devices. No installation, physical motor test, live orientation/overlay check, native two-phone connection test or ARCore device capability test was performed in this phase.
