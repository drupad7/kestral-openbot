# Two-phone installation and connection validation — 6 November 2026

## Devices and installation

- I2202 / Android 14: latest Controller APK installed and launched.
- Nothing A069 / Android 17: latest Robot APK installed and launched in Free Roam / Phone control mode.
- Both phones were on the same Wi-Fi subnet; a direct reachability check from A069 to I2202 returned both packets with zero loss.
- No emulator was used. No directional or individual-motor movement command was sent. Existing app data was not cleared, and no other app was uninstalled.

The initial fresh Controller install with automatic permission grants returned a blank failure on I2202. Retrying without automatic grants succeeded. The location permission was subsequently granted by the user, but the phone's system Location setting remained OFF. Wireless ADB briefly disconnected on both phones and then recovered.

## Root cause and correction

Controller `ConnectionSelector.isConnectedViaWifi` used `WifiManager.connectionInfo.networkId >= 0`. That identifier can be redacted when system Location is OFF. The Controller consequently cached the Nearby transport despite being connected to Wi-Fi. Its NSD dump contained no active OpenBot advertisement, while A069 was actively searching `_openbot._tcp` and found no service.

The correction uses `ConnectivityManager` network capabilities to recognize Wi-Fi, with the existing connected-Wi-Fi network-info fallback for older devices and local-only Wi-Fi. It does not enable system Location or require an unredacted SSID/network identifier to select LAN control. Existing hotspot detection, protocol, port, camera pipeline and motor policy remain unchanged.

After installing the correction, I2202 advertised `OPEN_BOT_CONTROLLER._openbot._tcp` on the existing port 19400. A069 connected, exchanged application messages and started its existing WebRTC camera pipeline. This before/after observation validates the transport-selection cause on the actual phones.

Android 17 on A069 did not require a new local-network permission for this target-SDK-36 build. The [Android local-network documentation](https://developer.android.com/privacy-and-security/local-network-permission) describes the target-SDK-dependent permission policy. No new SDK target or local-network permission was introduced as a speculative fix.

## Executed validation

1. Built both debug APKs: successful (`two-phone-build.log`).
2. Ran all Controller unit tests after the correction: **52 tests, zero failures/errors** (`two-phone-controller-fix.log`). Added a regression test with connected Wi-Fi and `networkId=-1`, proving the LAN transport is selected without opening sockets or peers.
3. Installed updated Controller on I2202 and Robot on A069; both launched successfully.
4. Verified real service registration/discovery and message exchange between the two phones.
5. Verified actual live video rendering on I2202: initially about 24 FPS, then about 25 FPS. Native logs showed 95–101 received/rendered frames per four-second interval, rather than the prior zero-frame intervals. A069 reported about 25 camera FPS.
6. Sent Home to the Controller, allowing its existing background disconnect behaviour, then reopened it. The Robot stopped the old camera session without the previously fixed stream-disposal crash. Both app PIDs remained unchanged and the video returned to about 25 FPS after reconnection.
7. No new fatal exception was observed in the inspected application logs during these checks. This is not a claim that all possible errors are excluded.
8. `git diff --check` passed. Existing model/Firebase hashes remain unchanged.

At completion the apps were left connected in Free Roam. Physical motor response, encoders, sonar, real waste accuracy, autonomous motion and AR tracking were not tested. Hardware-dependent controls can still remain disabled when the Robot has no valid hardware connection or safety prerequisites. This connection test does not authorize autonomous movement.

## Files changed for this correction

- Modified: `android/controller/src/main/java/org/openbot/controller/ConnectionSelector.kt`
- Added: `android/controller/src/test/java/org/openbot/controller/ConnectionSelectorTest.kt`
- Added: `docs/KESTRAL_TWO_PHONE_CONNECTION_FIX.md`

Diagnostic artifacts: `two-phone-build.log`, `two-phone-controller-fix.log`, `two-phone-controller-ui.xml`, `two-phone-robot-ui.xml`, `two-phone-robot-screen.png`. They are local investigation artifacts, not product assets. All unrelated local modifications and untracked files remain preserved. No commits or pushes were made.

APKs installed:

- `android/controller/build/outputs/apk/debug/controller-debug.apk`
- `android/robot/build/outputs/apk/debug/robot-debug.apk`
