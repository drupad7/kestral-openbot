# Scanning, public PDF reports and automatic synchronization

This phase replaces the report entry points and extends the existing report implementation. The earlier `SERBOT_MISSION_REPORTS.md` describes the initial private-storage/on-demand design; the workflow and transfer sections below supersede it.

## Audit and targeted changes

- The old entry points opened `MissionReportsPanel`, an AlertDialog combining recording, history, review and emergency controls. Reports now has its own full-content view with loading, empty, generating, ready and transfer states. The existing dialog is retained only for detection review/regeneration.
- PDF generation already used Android `PdfDocument` and real saved crops, including zero detections and repeated page headers. That implementation is reused.
- The existing `ReportClient` already provided correlated requests, private caching and chunked on-demand downloads. It had no automatic persistent transfer queue, verified receipt protocol, or Documents publication. These were added using the existing local TCP/Nearby JSON connection, separately from WebRTC video.
- Ending a report previously called a motor ownership fence and invalidated queued camera frames. Scanning now closes intake and drains accepted writes, with no motor command from Start/Stop Scanning.
- The report database was version 1. Version 2 adds durable command IDs, transfer jobs, local publications and report-date ordering without replacing existing missions/detections.

## User workflow

**Robot:** General → Reports. **Controller:** Menu → Reports. Both show saved PDFs newest first, date/time, Manual/Autonomous type, detection count, publication availability and transfer state. Open, Share and Export use that device's locally stored content URI. The Reports screen has a Back button and contains no Emergency Stop or scanning controls. Emergency Stop remains in the existing robot-control interfaces.

In Waste Classification, the existing controls now include **Start Scanning / Stop Scanning**, recording status, mission ID, unique detection count and PDF/transfer state. No new scanning mode is introduced. Choose Manual/Autonomous using the existing operation selector. Starting a scan does not start driving; stopping a scan does not stop motors. Navigation, manual leases and Emergency Stop remain independent.

The Controller does not optimistically change the button to a confirmed state. Commands carry a stable logical `command_id` and a transport `request_id`; the Robot allocates the mission UUID and acknowledges committed state. Repeated logical commands return the same mission, including after a restart. A preserved interrupted scan can be stopped to recover/finalize it before another scan begins.

The Robot Reports screen can reuse the existing phone connection with its camera start gate disabled. This supports report synchronization without selecting a camera mode. Existing controller/Nearby permissions must already be granted; otherwise enable them through the existing driving setup. No second camera or transport is opened.

## Recording and finalization

The live threshold remains configurable with default **0.65**; the existing decoder floor remains 0.35. Recording eligibility remains independently **>= 0.90**, before live filtering. The supplied model, preprocessing, decoding/NMS, camera transforms and video pipeline are unchanged. Free Roam retains its false inference gate.

Live-confidence configuration is available beside the Waste Classification scan controls; it is no longer mixed into report history. Its editor is dismissed with the camera/control mode. Saved PDFs retain their generated detection count; later review changes are marked as requiring regeneration rather than showing a new count against an old PDF.

The existing exact-frame reference ownership, full context/crop JPEG saving, bounded reporting queue, timestamp/GPS association and approximate appearance/overlap/temporal tracking are retained. Different same-class objects can have different records. Review allows correction/removal. Deduplication remains a heuristic, not verified permanent physical identity; revisits, occlusion and identical objects can merge or split tracks.

Stop closes frame intake synchronously. The report I/O actor finishes already accepted images and database transactions before committing ENDED and its end timestamp. The initial Stop acknowledgement can say **PDF Generating**. PDF work then runs on that actor, independently of connection availability. Completed PDF metadata and a PENDING transfer are persisted before public publication is attempted. A process interruption after Stop is recovered on the next Robot app start; ended missions with no PDF are regenerated, existing PDFs are queued/backfilled and interrupted transfers return to PENDING.

Existing pause/mode-exit/communication safety handling is preserved. Those events may leave an interrupted report for explicit recovery; they do not invent navigation capability. Autonomous scan reports retain the repository's existing unverified-navigation qualification. This phase does not enable physical coverage execution or issue new motor commands.

## PDF format, dates and storage

The existing A4 PDF contains:

```text
KESTRAL OPENBOT
Autonomous Waste Detection
Mission Detection Report
```

It includes generation time, mission ID/type, start/end, unique count and the required S.No / Type of Waste / Image / Location table with real captured photographs. GPS is explicitly the robot's capture position with accuracy, or **GPS unavailable**. No exact waste geolocation is inferred.

- Display dates: `dd/MM/yyyy (HH:mm)`.
- Public filenames: `dd-MM-yyyy (HH-mm).pdf`.
- Collisions add a mission-based unique suffix; existing reports are not overwritten.
- Public directory on **each device**: `Documents/Kestral OpenBot/Reports/`.

On Android 10+, `ReportDocuments` publishes through MediaStore.Files with RELATIVE_PATH, IS_PENDING and a content URI. It verifies the destination's size and SHA-256 before marking the local publication READY. Older Android devices, or providers that do not support publication, use a one-time **folder access** action in Reports: select the phone's Documents folder, then the app creates Kestral OpenBot/Reports using SAF. The URI permission is persisted. An explicit SAF choice is also supported on newer devices. These use [Android shared-storage APIs](https://developer.android.com/training/data-storage/shared/media) and [the Storage Access Framework](https://developer.android.com/training/data-storage/shared/documents-files).

If permission/storage/provider access is unavailable, the private PDF and pending queue remain intact and publication can be retried; the app does not claim a Files-visible copy exists. Public publications are journalled as WRITING before copying and READY only after verification. Stored metadata includes local content URI, filename, mission/Robot IDs, hash and size. URI metadata is **not copied from Robot to Controller**. Each phone creates/verifies its own publication.

The private immutable source/cache remains available for transfer/recovery. Source Robot PDFs are never automatically deleted after transfer. Correcting detections requires regenerating the reviewed PDF using Review → Regenerate; the new PDF version gets its own transfer ID and supersedes the old queued version. Older physical PDF versions are retained.

## Transfer protocol and offline recovery

`reportCmd` requests and `REPORT_STATUS` replies use the existing local connection. WebRTC video is unchanged; no PDF bytes are embedded in video frames. New actions include SCAN_START, SCAN_STOP, STATUS, REPORTS, LOCAL_PDF, SYNC, TRANSFER_BEGIN, TRANSFER_FAILED, RECEIPT and RETRY_TRANSFER. Existing actions remain compatible.

The Robot creates a durable transfer job with mission ID, transfer UUID, immutable asset name, file size, SHA-256 and PENDING state. SYNC pages through all current report jobs, not only the first report-history page. The Controller's connection-scoped `ReportAutoSync` worker automatically processes one PDF at a time. Controller restart/reboot recovers its SQLite jobs, cache checkpoints and public publication metadata when the normal connection is restored.

Each 6 KiB chunk is requested with transfer ID, sequence, byte offset and request ID. Replies must match those values, mission/source identity, total size and checksum. Correlated replies acknowledge each chunk; timeouts retry the same request, and duplicate/delayed replies cannot append it twice. Partial files and immutable expected hashes are retained for byte-offset continuation. The **complete** file must pass size and SHA-256 verification before use; a corrupt result is truncated/retried rather than marked complete. Temporary downloads remain private until validated, then are copied to the Controller's own Documents directory and verified again.

Only after successful local publication does the Controller send RECEIPT containing mission ID, transfer ID, size and hash. The Robot validates those against the current job and then marks TRANSFERRED. If that acknowledgement is lost, reconnect finds the already published verified copy and resends the receipt without downloading or creating another publication. A missing/corrupt public copy is repaired from a valid private cache or redownloaded. Source retention is independent of receipt success.

Temporary failures are persisted with attempts/error/next retry. Backoff grows up to 60 seconds with at most five automatic attempts per connection session; Reports offers Retry Transfer. A fresh connection starts a new bounded retry budget. Robot-side Retry also publishes a new retry token so the Controller can reset its local failed budget. Generating a PDF is separate from transferring it: a Robot report remains usable while waiting for Controller or after transfer failure.

Workers are tied to the existing application's active connection and are stopped/invalidated on disconnect and destruction. Controller backgrounding already disconnects the existing transport. No new background service/boot receiver is introduced, and **no transfer is promised while both apps are inactive**. Persistent jobs are resumed automatically when the apps start/reconnect and permissions/storage are available.

## Verification

Final validation on 5 November 2026:

| Check | Actual result |
|---|---|
| Robot full unit suite | **404 passed**, zero failures/errors/skips |
| Controller full unit suite | **45 passed**, zero failures/errors/skips |
| Total full-suite tests | **449 passed**; 17 new tests in this phase |
| Robot / Controller debug APKs | Both built successfully |
| Robot native test APK | Built successfully, including four new scan/Documents tests |
| Git whitespace check | Passed; existing Firebase line-ending advisory only |
| Supplied model / both Firebase JSON files | Task-start hashes unchanged |
| Installation / native tests for this phase | **Not completed**: I2202 disappeared from ADB before installation; mDNS/devices were empty and the last paired endpoint refused connection. No new native tests are counted as passed. |

The JVM tests use the production SQLite command/transfer/cache logic and real chunk encoding/checksums through a simulated message link. Public Documents publishing is represented by a test provider fixture there. Restart tests close/recreate repository/client objects using the same database/cache files; they are not claims of physical reboot validation. Coverage includes command acknowledgements/idempotency, Stop draining accepted writes without calling the motor fence, independent thresholds, same-class separation, missing GPS, filename formatting, schema migration, reconnect/receiver restart, corrupted chunks, explicit retry, lost chunk replies, lost receipts, corrupt public-copy repair, late-callback shutdown and confidence-dialog cleanup. The existing Free Roam/model/control tests also passed.

The compiled native tests in `ScanDocumentsDeviceTest` cover actual MediaStore Documents placement/readback, filename collisions, independent Robot/Controller publications, offline generation followed by automatic transfer, and Controller Start/Stop acknowledgements. Those remain **pending on a connected device**, along with rerunning the existing native PDF/model tests against this APK. No real waste-scene, outdoor GPS, two-phone network/reboot, or physical motor test was performed in this phase. The APKs are built but were **not installed on I2202 in this phase**.

The build retains existing compileSdk/AGP and D8 Navigation warnings; this feature does not migrate the toolchain. Earlier compilation issues during incremental implementation were fixed before the final passing build. Logs: `scan-sync-build.log`, `scan-sync-focused.log`, `scan-sync-final.log`, `scan-sync-native-build.log`.

After reconnecting I2202, install the Robot and native-test APKs with `install -r`, then run:

```powershell
adb -s <robot-device> shell am instrument -w -r -e class org.openbot.reports.ScanDocumentsDeviceTest,org.openbot.reports.MissionReportDeviceTest,org.openbot.app.robot.robot.waste.WasteModelDeviceTest org.openbot.app.robot.test/androidx.test.runner.AndroidJUnitRunner
```

Native fixtures use isolated mission databases/UUIDs and remove their own shared Documents publications in teardown. They never open a camera or command motors. Separately install the Controller APK on its phone and validate the actual TCP/Nearby link, disconnect/reconnect and reboot scenarios with real captures.

Build command:

```powershell
.\android\gradlew.bat -p android :robot:testDebugUnitTest :controller:testDebugUnitTest :robot:assembleDebug :controller:assembleDebug :robot:assembleDebugAndroidTest --offline --console=plain
```

APKs remain at `android/robot/build/outputs/apk/debug/robot-debug.apk` and `android/controller/build/outputs/apk/debug/controller-debug.apk`. Install with `adb -s <device> install -r <apk>` to preserve app data. Test with both phones foreground and their existing connection established. For SAF fallback, grant Documents folder access on each phone separately.

Field checks still required unless explicitly recorded as performed: real waste-scene photo clarity/deduplication, outdoor GPS accuracy, sustained WebRTC/manual-control performance while recording/transferring, two-physical-phone network failure/reboot scenarios and external viewer/share/SAF picker behaviour. Automated fixtures do not constitute physical robot or geographic accuracy validation.

## Exact file inventory for this phase

This list is relative to the task-start working-tree hash snapshot, including files that were already untracked. Earlier unrelated changes are preserved. This document is also newly created.

Change | File
--- | ---
Modified | `android/controller/src/main/java/org/openbot/controller/WasteModePanel.kt`
Modified | `android/reporting/src/main/java/org/openbot/reports/MissionReportsPanel.java`
Created | `android/reporting/src/main/java/org/openbot/reports/ReportAutoSync.java`
Modified | `android/reporting/src/main/java/org/openbot/reports/ReportClient.java`
Created | `android/reporting/src/main/java/org/openbot/reports/ReportDocuments.java`
Modified | `android/reporting/src/main/java/org/openbot/reports/ReportExportActivity.java`
Created | `android/reporting/src/main/java/org/openbot/reports/ReportFolderActivity.java`
Modified | `android/reporting/src/main/java/org/openbot/reports/ReportPdf.java`
Modified | `android/reporting/src/main/java/org/openbot/reports/ReportRepository.java`
Modified | `android/reporting/src/main/java/org/openbot/reports/ReportStore.java`
Created | `android/reporting/src/main/java/org/openbot/reports/ReportsScreen.java`
Created | `android/reporting/src/main/java/org/openbot/reports/ScanControls.java`
Modified | `android/robot/src/androidTest/java/org/openbot/reports/MissionReportDeviceTest.java`
Created | `android/robot/src/androidTest/java/org/openbot/reports/ScanDocumentsDeviceTest.java`
Created | `android/robot/src/main/java/org/openbot/app/robot/main/ReportsFragment.java`
Modified | `android/robot/src/main/java/org/openbot/app/robot/robot/WasteClassificationFragment.java`
Modified | `android/robot/src/main/java/org/openbot/app/robot/robot/waste/WasteReportRuntime.java`
Created | `android/robot/src/test/java/org/openbot/reports/AutomaticReportSyncTest.java`
Created | `android/robot/src/test/java/org/openbot/reports/ScanReportsUiTest.java`
Created | `android/robot/src/test/java/org/openbot/reports/ScanningLifecycleTest.java`
Created | `scan-sync-build.log`
Created | `scan-sync-final.log`
Created | `scan-sync-focused.log`
Created | `scan-sync-native-build.log`
Modified | `android/controller/src/main/AndroidManifest.xml`
Modified | `android/controller/src/main/java/org/openbot/controller/ControllerActivity.kt`
Modified | `android/controller/src/main/res/layout/activity_fullscreen.xml`
Modified | `android/robot/src/main/AndroidManifest.xml`
Modified | `android/robot/src/main/java/org/openbot/app/robot/common/FeatureList.java`
Modified | `android/robot/src/main/java/org/openbot/app/robot/env/PhoneController.java`
Modified | `android/robot/src/main/java/org/openbot/app/robot/main/MainActivity.java`
Modified | `android/robot/src/main/java/org/openbot/app/robot/main/MainFragment.java`
Modified | `android/robot/src/main/res/menu/menu_items.xml`
Modified | `android/robot/src/main/res/navigation/nav_graph.xml`
Modified | `android/robot/src/main/res/values/strings.xml`
