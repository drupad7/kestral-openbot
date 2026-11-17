# I2202 physical-phone validation — 6 November 2026

Only the connected physical I2202 was used. No emulator tests were run in this phase and no movement commands were issued. Existing UI tests hold the emergency latch during their Activity execution. Both existing Firebase JSON files and the waste model retain their previously audited hashes. No commits or pushes were made.

## Found and corrected

The phone's crash buffer contained repeated Robot `IllegalStateException: MediaStream has been disposed` failures when `MainActivity.onPause` disabled capture. The latest pre-fix entry was 6 November at 00:02:11 IST. `WebRtcServer.stopServer` disposed the peer before accessing its attached stream/tracks. WebRTC peer disposal also releases its attached streams; attempting to remove tracks afterward accessed disposed objects.

The correction detaches the local stream from its peer, removes its tracks and disposes that stream before disposing the peer. The server then disposes its explicitly owned tracks/sources as before. The existing camera pipeline, model and protocol are preserved. This follows the ownership described by upstream [PeerConnection](https://webrtc.googlesource.com/src/+/refs/heads/main/sdk/android/api/org/webrtc/PeerConnection.java) and [MediaStream](https://webrtc.googlesource.com/src/+/refs/heads/main/sdk/android/api/org/webrtc/MediaStream.java); the regression test uses the project's actual bundled native WebRTC library.

An older Controller socket crash dated 30 October also exists in historical logs. Its uninitialized-client path has already been replaced by nullable/idempotent cleanup in current source. It was not newly reproduced during these Robot tests, and the Controller APK was not reinstalled or physically tested in this phase.

## Actual results

- Device: I2202, Android 14 / API 34, connected using Wireless Debugging.
- Robot build, test APK build and **428 Robot JVM tests passed**, zero failures/errors. Existing Navigation/D8 and deprecated Gradle/API warnings remain. See `phone-validation-build.log`.
- Updated Robot and matching test APKs installed with `adb install -r`; app data was retained.
- **12 native tests passed** (`phone-native-validation.log`): actual bundled TFLite runtime/model execution on a synthetic input; native stream/track cleanup twice without a camera; ARCore availability; zero/one/multiple-detection PDF signatures/pages; fixture-photo pixels embedded in PDFs; review regeneration; public Documents publication, collision/URI checks; and report transfer through a simulated existing-message link, including caching/reconnection tests.
- **3 native UI tests passed** (`phone-ui-validation.log`): portrait Free Roam/Waste layout and fullscreen/panel toggles with Emergency Stop visible; actual central-handle system touch gestures; and bounding-box rendering against synthetic metadata at multiple rotations.
- At the end of these tests, the crash buffer still ended with the old 00:02:11 failure. No new OpenBot fatal crash appeared during these checks. Old crash logs were not cleared.
- `git diff --check` passed.

The actual ARCore availability response was **SUPPORTED_INSTALLED**. This confirms the availability API recognizes compatible installed ARCore support. It does **not** establish floor tracking accuracy, metric anchors, shared-camera integration or relocalization. No AR Session was opened and physical autonomous execution remains disabled.

## Limits

These tests did not establish two-phone live WebRTC continuity, real scene/YOLO box alignment during motion, physical waste detection accuracy, actual GPS accuracy, ultrasonic/motor behaviour, or real-world AR navigation. PDF photographs were real saved test-fixture images, not live photographed waste. Report transfer used the production codec/cache/storage through a simulated message link on the physical phone, not a live second Controller phone. UI rendering tests do not prove optical calibration.

## Files changed in this validation phase

- Modified: `android/robot/src/main/java/org/openbot/app/robot/env/WebRtcServer.java`
- Created: `android/robot/src/androidTest/java/org/openbot/app/robot/env/WebRtcCleanupDeviceTest.java`
- Created: `docs/KESTRAL_I2202_VALIDATION.md`
- Logs: `phone-validation-build.log`, `phone-native-validation.log`, `phone-ui-validation.log`

Installed Robot APK: `android/robot/build/outputs/apk/debug/robot-debug.apk`.
