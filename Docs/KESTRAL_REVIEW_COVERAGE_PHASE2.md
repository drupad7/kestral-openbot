# Kestral review and coverage continuation

## Audit findings and implementation scope

The working model, WebRTC acquisition, nine waste classes, `WasteReportRuntime.observed`, image capture, deduplication and confidence thresholds are preserved. The model and both Firebase JSON files retained their Phase 1 SHA-256 hashes. Existing unrelated working-tree changes and untracked files are preserved; no commits, pushes, firmware or hardware changes were made.

| Area | Existing implementation and this phase |
|---|---|
| Geometry | `CoveragePlanner` already generates clipped lanes, interior connectors and pivot/arc turns with footprint, clearance, uncertainty and spacing validation. Extended the same planner to connect independent metric Start/End positions, including those connectors in turning validation. |
| Saved indoor maps | `IndoorMapStore` stores **VISUAL_ONLY** normalized camera-snapshot polygons. There is no metric scale, live Robot pose or relocalization provider. `IndoorMapRuntime` validates a selected polygon then returns an explicit BLOCKED planning result with null route/Robot position. These maps cannot be approved or executed as physical routes. |
| GPS boundary previews | `CoveragePlanningController`, Controller `CoveragePanel` and Robot `WasteScanPanel` retain the existing metric preview/approval flow. Optional `config.start` / `config.end` pairs are accepted in the same east/north metric frame. Existing UI without these fields retains lane endpoints. Current GPS accuracy does not prove indoor localization. |
| Navigation state/safety | `AutonomousScanController` already retains its selected plan on Pause, checks prerequisites on Resume, and models obstacle, disconnect, invalid position/sensor and emergency conditions. Production remains `PREVIEW_ONLY`, `executable=false`, `can_start=false`. No coverage executor or new motor loop is added. Existing manual/legacy mission safety is unchanged. |
| Mode selection | Previously Controller Waste Classification required selecting the mode on Robot manually. Added ID-correlated `robotModeCmd` / `MODE_CHANGE_STATUS` activation through the existing Robot Activity/navigation and communication bus, with bounded retry/timeout and safe-screen checks. |
| Manual / Auto | Existing `scanCmd` and qualified `SCAN_STATUS` select Manual/Auto. Auto opens the saved indoor map picker after the matching acknowledgement; selecting a visual map shows the Robot's planning blocker. Existing GPS-boundary preview remains separately accessible. Repeated command IDs cannot reset an already selected plan or repeat state transitions. |
| Recording | Existing `ScanControls` operates independently of motor commands. Added it to Controller manual tools while Waste Classification is active. Live default remains 0.65, recording remains independently 0.90. No new indoor coordinates are invented. |
| Review | Existing `MissionReportsPanel` Confirm/correct, Remove and category Spinner are retained. Added Finalize Review with authoritative all-record decision checks and revision validation. |
| PDF/storage/transfer | Existing `ReportPdf`, `ReportDocuments`, `ReportAutoSync` and chunk/checksum/retry transport are reused. Generation/transfer now follow explicit review finalization. Documents path, timestamp filenames/collision handling, photographs and layout are unchanged. |

## Recording and review lifecycle

Start Scanning allocates a durable Robot mission ID, enters ACTIVE and accepts qualifying frames. Pause/interruption uses existing PAUSED/INTERRUPTED recovery. Stop Scanning stops frame admission immediately, drains previously accepted image writes on the existing I/O actor, records the end time and enters **REVIEW_PENDING**. It generates no PDF and queues no new report transfer. Navigation Start/Pause/Resume/Stop remain separate commands; scanning does not directly command motors. An existing storage-error motor fence remains a safety exception.

Review Detections is accessible beside scanning controls and in Robot General → Reports / Controller Menu → Reports. Pending-review missions appear there even though they do not yet have PDFs. Remote details and images are fetched using existing paginated synchronization; previously fetched records remain cached offline. Review mutations require the Robot connection; disconnected edits are not silently claimed as saved.

The existing category selector and Confirm/Remove decisions write to the authoritative SQLite store. Finalize Review is enabled only when an online DETAIL reply reports zero pending decisions across the **entire mission**, including other pages. The Robot checks the current revision and decisions again. Zero-detection missions also require an explicit Finalize Review.

Finalization persists approval and ENDED state before calling the unchanged Android PDF generator. Corrected categories are used, REMOVED records are excluded, and the report count excludes removals. Duplicate finalization returns the same report; failed generation can be retried. If the process stops after approval but before generation, startup recovery can generate the approved report. Pending review survives restart without generating anything.

Reviewing an already finalized report reopens REVIEW_PENDING, invalidates the current PDF reference and supersedes outstanding transfers. Re-finalization is required before a replacement is generated. Historical immutable PDFs already exported into Documents remain user files; they are not automatically deleted. Previously existing reports from older versions remain accessible; an old ENDED mission with no PDF and no approval marker is recovered into pending review instead of silently generating an unreviewed report.

Public copies remain at `Documents/Kestral OpenBot/Reports/`, using `dd-MM-yyyy (HH-mm).pdf` and existing collision suffixes. Existing persistent transfer jobs, verified receipts, SHA-256 validation, bounded chunks/retries and reconnect recovery apply after approval. Robot and Controller use their own local content URIs. This does not promise background transfer while both apps are inactive.

## Mode and movement boundaries

Robot `MainActivity` accepts a Waste mode request only while resumed and on an eligible safe screen. A busy Free Roam mission or boundary/route recording prevents leaving. Before switching it commands zero output and uses the existing navigation destination. It acknowledges only after the Waste fragment resumes. Duplicate in-flight/completed requests do not create another camera fragment. Switching from another camera-owning AI mode is rejected instead of disrupting it silently.

Controller Manual/Auto actions still use the existing motor lease policy. The Controller waits for matching mode-session/epoch/request status; opening a menu or acknowledging a screen change never grants movement. Emergency Stop remains available in controls and scan-review dialogs. Navigation state commands are deduplicated within the active fragment; persisted scan commands continue using the existing SQLite command ledger.

No physical indoor route is generated from screen pixels. Auto requires a validated metric frame, relocalization and live calibrated pose before physical route selection can be meaningful. The present visual maps are rejected explicitly. The geometry planner's output remains a preview, even for GPS-recorded boundaries, and does not assert that unseen areas are obstacle-free or that one ultrasonic sensor covers the full swept path.

## Files changed in this phase

Existing source/UI:

- `android/reporting/src/main/java/org/openbot/reports/ReportRepository.java`
- `android/reporting/src/main/java/org/openbot/reports/ReportStore.java`
- `android/reporting/src/main/java/org/openbot/reports/ReportClient.java`
- `android/reporting/src/main/java/org/openbot/reports/ScanControls.java`
- `android/reporting/src/main/java/org/openbot/reports/MissionReportsPanel.java`
- `android/reporting/src/main/java/org/openbot/reports/ReportsScreen.java`
- `android/robot/src/main/java/org/openbot/app/robot/main/MainActivity.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/FreeRoamFragment.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/WasteClassificationFragment.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/coverage/CoveragePlanner.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/coverage/CoveragePlanningController.java`
- `android/robot/src/main/java/org/openbot/app/robot/robot/coverage/IndoorMapRuntime.java`
- `android/controller/src/main/java/org/openbot/controller/ControllerActivity.kt`
- `android/controller/src/main/java/org/openbot/controller/WasteModePanel.kt`
- `android/controller/src/main/java/org/openbot/controller/IndoorMappingPanel.kt`
- `android/controller/src/main/res/layout/manual_controls_overlay.xml`

Existing tests updated:

- `android/robot/src/test/java/org/openbot/reports/ScanningLifecycleTest.java`
- `android/robot/src/test/java/org/openbot/reports/MissionReportsPanelTest.java`
- `android/robot/src/test/java/org/openbot/app/robot/robot/coverage/CoveragePlannerTest.java`
- `android/robot/src/test/java/org/openbot/app/robot/robot/WasteScanHostTest.java`
- `android/controller/src/test/java/org/openbot/controller/WasteModePanelTest.kt`
- `android/robot/src/androidTest/java/org/openbot/reports/MissionReportDeviceTest.java`
- `android/robot/src/androidTest/java/org/openbot/reports/ScanDocumentsDeviceTest.java`

New files:

- `android/robot/src/test/java/org/openbot/reports/ReviewFinalizationTest.java`
- `android/robot/src/test/java/org/openbot/app/robot/robot/WasteCommandReplayTest.java`
- `docs/KESTRAL_REVIEW_COVERAGE_PHASE2.md`

Build/test logs: `phase2-build.log`, `phase2-tests.log`, `phase2-zero-test.log`, `phase2-final.log`, `phase2-native.log`. Ignored Gradle build outputs also change.

## Validation procedure

```powershell
.\android\gradlew.bat -p android :robot:testDebugUnitTest :controller:testDebugUnitTest :robot:assembleDebug :controller:assembleDebug :robot:assembleDebugAndroidTest --offline --console=plain
adb -s emulator-5554 install -r -g android/robot/build/outputs/apk/debug/robot-debug.apk
adb -s emulator-5554 install -r -g android/robot/build/outputs/apk/androidTest/debug/robot-debug-androidTest.apk
adb -s emulator-5554 shell am instrument -w -r -e class org.openbot.reports.MissionReportDeviceTest,org.openbot.reports.ScanDocumentsDeviceTest org.openbot.app.robot.test/androidx.test.runner.AndroidJUnitRunner
```

The JVM review tests inject a simulated PDF delegate to validate ordering, review gates, zero/many records, persistence and idempotency. Robolectric 4.12.1 here does not provide usable native Android `PdfDocument` generation. Real PDF signature, page count, photographed pixels, storage URIs and file transfer are covered by the separate native instrumentation tests; their actual execution results are recorded below after completion.

Physical validation is still required for both phones together: mode acknowledgements during connection loss/recovery; scan controls while driving; real YOLO photos/overlay alignment and deduplication; GPS availability; ultrasonic stopping distance/freshness; motor deadman behaviour; battery handling; and approved-PDF synchronization across an actual connection interruption. AR shared-camera integration, metric anchoring, persistent relocalization, phone-to-chassis calibration and a validated route follower remain blockers for physical indoor autonomy. No new sensor hardware is assumed.

## Current results and user-requested testing pause

Both complete JVM suites passed: **Robot 428 tests and Controller 51 tests, zero failures/errors**. Both debug APKs and the Robot instrumentation APK built successfully; `phase2-final.log` records the final successful run. `git diff --check` passed. Existing Gradle/API warnings remain.

Robot and test APK installation on the emulator succeeded, but the native instrumentation command returned no test results. No native PDF, photograph embedding, Documents storage, device-to-device transfer, camera or physical hardware result is claimed from that attempt. On the user's instruction, emulator testing was stopped; further device testing must use the user's connected mobile phone when available. The native commands above are a record of the attempted procedure, not an instruction to continue emulator testing.
