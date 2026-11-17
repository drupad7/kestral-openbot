# Kestral OpenBot

### Campus Waste Detection, Mapping and Mission Reporting

**Kestral OpenBot** is a mobile robotics project designed to help inspect campus and indoor environments for waste. It combines an ESP32-based two-wheel robot, a robot-mounted Android phone, and a separate Android Controller application. The robot phone detects waste using the project's trained YOLO11s model; the operator phone provides remote driving, live monitoring, detection review, and report access.

The project also includes indoor AR boundary recording and zigzag route previews. **Physical autonomous coverage remains disabled until localization, relocalization, and hardware safety are validated.**

[Quick Start](#quick-start) · [Hardware](#hardware) · [Build](#build-and-install) · [Documentation](#documentation)

## Project purpose

Kestral OpenBot brings the inspection workflow into one system: drive through an area, identify visible waste, capture photographic evidence, review the findings, and produce a mission report. Its intended use includes campus cleanliness surveys, classroom and corridor inspections, and documented waste-monitoring missions.

The project has three main parts:

1. **Kestral Robot App:** runs on the robot, processes the camera, communicates with the ESP32, and stores mission results.
2. **Kestral Controller App:** runs on the operator's phone for driving, monitoring, mapping requests, and reviewing results.
3. **Kestral robot hardware:** an ESP32, TB6612FNG driver, two DC motors, and HC-SR04 ultrasonic sensor.

The current focus is detection and reporting. No automatic waste pickup mechanism is implemented.

## How the system works

```text
OPERATOR PHONE                         ROBOT-MOUNTED PHONE
Controller Android App                 Robot Android App

Driving and Emergency Stop   commands  Motor command ownership and safety
Live camera and telemetry    <------>  Camera, YOLO inference and telemetry
Map and mission controls     status    AR mapping and authoritative storage
Review and report access               Detection review and PDF generation
          |                                     |
          +---- Wi-Fi / Nearby communication ---+
                  WebRTC live video             |
                                                | USB OTG serial
                                                | 115200 baud, 8N1
                                           ESP32 Dev Module
                                                |
                                      TB6612FNG motor driver
                                      Two DC motors + HC-SR04
```

The Controller sends requests to the Robot phone; it does not directly command the ESP32. The Robot is authoritative for mission state, detections, maps, and reports. PDF and image files transfer separately from the video stream.

## The two Android applications

| | Robot App | Controller App |
| --- | --- | --- |
| Installed on | Phone mounted on the robot | Operator's handheld phone |
| Main role | Camera, AI inference, hardware communication, mission storage | Remote driving, monitoring, review, and mission requests |
| Orientation | Portrait normally; camera Full Screen switches to landscape; Exit/Back returns to portrait | Landscape |
| Driving | Validates commands and forwards wheel output to the ESP32 | Forward, Backward, Left, Right, Stop, and separate individual motor controls |
| Video | Captures the camera and publishes WebRTC video | Displays the Robot's live stream |
| Waste detection | Runs the existing YOLO11s TFLite model | Displays synchronized detection information |
| Reports | Stores original records and generates reviewed PDFs | Receives records, images, and verified PDFs into local storage |

The project has been used with **Nothing A069 as the Robot phone** and **I2202 as the Controller phone**. These are the project devices, not a guarantee of compatibility with every Android phone. AR features additionally require supported, installed ARCore and suitable tracking conditions.

### Operating modes

- **Free Roam:** manual driving and normal live video, without waste inference.
- **Waste Classification - Manual:** remote driving with live waste detection. Start/Stop Scanning independently controls photo recording.
- **Waste Classification - Auto:** saved-map selection, coverage preview, approval, and navigation state controls. Physical autonomous movement remains blocked pending validation.
- **Motor Control:** separate individual wheel testing through the existing control architecture. It cannot share motor ownership with an active autonomous mission.

Additional camera modes, including Autopilot and Object Tracking, remain available in the Robot App. These modes still require capability-specific calibration and validation on the Kestral hardware.

## Hardware

| Component | Purpose |
| --- | --- |
| ESP32 Dev Module | Receives serial wheel commands and reads the ultrasonic sensor |
| TB6612FNG dual motor driver | Drives the left and right DC motors independently |
| Two DC motors / 2WD chassis | Differential-drive movement |
| HC-SR04 ultrasonic sensor | Measures obstacles in its mounted sensing direction |
| Robot Android phone and rigid mount | Camera, AI, WebRTC, mission storage, and supported AR tracking |
| Controller Android phone | Operator interface |
| Data-capable USB OTG connection | Connects the Robot phone to the ESP32 USB-to-serial interface |
| Suitable motor power supply and wiring | Powers the drive system within component ratings |

**Encoders are not assumed installed.** Optional encoder code exists, but `ENABLE_ENCODERS` is currently `false`. No LiDAR, depth camera, MPU6050, or calibrated wheel odometry is required or claimed by the current configuration. Motor-driver battery-voltage telemetry is unavailable without the appropriate sensing hardware.

### Configured wiring

These GPIO assignments match the existing `sample.ino` motor-and-sonar test and application firmware:

| Connection | ESP32 GPIO |
| --- | --- |
| Left motor AIN1 / AIN2 / PWMA | 27 / 26 / 25 |
| Right motor BIN1 / BIN2 / PWMB | 33 / 32 / 18 |
| TB6612FNG STBY | 14 |
| HC-SR04 TRIG / ECHO | 5 / 22 |

Use a shared ground and a suitable separate motor supply. Level-shift or divide the HC-SR04 ECHO signal before the ESP32 input. Do not power the motors from the phone's USB port. See the [hardware setup guide](docs/CAMPUS_HARDWARE_SETUP.md) for electrical details, optional encoder configuration, firmware timeouts, and supervised checks.

`sample.ino` is a standalone hardware test sketch. Use `firmware/openbot/openbot.ino` for application integration: its protocol differs from the sample's serial-monitor letter commands.

## Waste detection and scanning

The Robot uses the existing **YOLO11s `waste_model.tflite`** without retraining or replacing it.

| Setting | Value |
| --- | --- |
| Input | Float32 `[1, 3, 320, 320]` |
| Output | Float32 `[1, 13, 2100]` |
| Live display threshold | 65% by default, independently configurable |
| Scanning photo threshold | At least 75% confidence |

The nine categories are:

| ID | Category | ID | Category | ID | Category |
| --- | --- | --- | --- | --- | --- |
| 0 | Plastic bag & wrapper | 3 | Bottle cap | 6 | Carton |
| 1 | Cigarette | 4 | Can | 7 | Cup |
| 2 | Bottle | 5 | Other plastic | 8 | Other |

During an active scan, qualifying detections can save actual camera photographs, category, confidence, timestamp, bounding box, and available capture location. Existing matching and deduplication reduce repeated records; review is still needed because identical-looking objects and reappearances cannot always be resolved perfectly.

**Start Scanning does not start the motors. Stop Scanning does not act as Emergency Stop.** Driving/navigation and detection recording are separate operations.

## Review and PDF reports

The normal workflow is:

1. Open Waste Classification and select Manual or the available Auto preview workflow.
2. Press **Start Scanning** to begin recording eligible detections.
3. Press **Stop Scanning** to finish recording and prepare review.
4. In the existing review interface, use **Confirm**, **Remove**, or **Categories** to approve, reject, or correct detections.
5. Finalize the review. The Robot generates the final PDF using confirmed detections and corrected categories; removed detections are excluded.
6. Open reports from **Robot: General > Reports** or **Controller: Menu > Reports**.

The PDF retains the headings **KESTRAL OPENBOT**, **Autonomous Waste Detection**, and **Mission Detection Report**, with the table:

| S.No | Type of Waste | Image | Location / GPS Coordinates |
| --- | --- | --- | --- |

Reports are published on each device under:

```text
Documents/Kestral OpenBot/Reports/
```

Filenames use `dd-MM-yyyy (HH-mm).pdf`, with a unique suffix when necessary. Depending on Android storage/provider configuration, Reports may require a one-time folder selection. The apps use each device's local content URI for opening and sharing.

GPS identifies the **robot's capture position**, not an independently measured waste position. Missing fixes are shown as **GPS unavailable**. Available indoor AR estimates are labelled separately with their local reference frame and calibration limitations.

The Robot retains its report when the Controller is offline. Existing synchronization uses persistent pending transfers, bounded chunks, acknowledgements, size/checksum verification, and retries on reconnection. Transfers resume when the apps reconnect and Android permits execution; continuous transfer while both apps are inactive is not promised.

## Indoor mapping and route planning

From **Controller > Autonomous Mission > New / Load**, the operator can record or browse maps.

For supported AR mapping, prepare the Robot camera with measured camera-to-robot offsets. Start records the current floor position without driving. Manually drive to each boundary location, stop, and use Add Point. After at least three distinct vertices, End records the final position and closes the polygon. Validated geometry can be named and saved.

The Robot owns AR tracking and metric calculations. Screen pixels and delayed video taps are not treated as physical coordinates. Saved maps carry a reference frame, timestamps, and tracking metadata; visual-only drafts are explicitly distinguished from metric AR drafts.

The coverage planner previews lanes and connecting turns using supplied robot dimensions, lane spacing, clearance, and turning assumptions. Route approval approves the geometry only. Persistent relocalization, pose accuracy, braking, obstacle coverage, and actual route following still need validation before physical autonomy can be enabled. A single forward-facing HC-SR04 cannot establish complete obstacle clearance.

## Quick start

1. Build/install the **Robot APK** on the robot-mounted phone and the **Controller APK** on the operator phone.
2. Configure and flash the integration firmware for your confirmed ESP32 board and wiring. Begin hardware checks with the wheels lifted.
3. Connect the Robot phone to the ESP32 through USB OTG, grant USB access, and select USB hardware communication at **115200 baud**.
4. Put both phones on the same Wi-Fi network or a hotspot allowing device-to-device traffic and local discovery. Nearby Connections is the existing alternative where supported and permitted.
5. In Robot settings, select **WebRTC** video. Open **Free Roam** and select **Phone** control. Open the Controller and grant the requested permissions.
6. Confirm the live camera, link status, and changing sonar readings before attempting movement. Test Stop, command release, and disconnection behavior at low speed in a controlled area.
7. Select Waste Classification for AI detection, scanning, review, and reporting.

A USB-open indicator alone does not prove the firmware is exchanging valid telemetry. Missing sensor values should remain unavailable rather than being interpreted as a clear path.

## Build and install

### Development prerequisites

- Android Studio with **JDK 17**.
- Android SDK Platform **36** and Build Tools **36.0.0**, as configured in the app modules.
- Gradle **8.9**, matching the current wrapper configuration.
- Espressif ESP32 Arduino tools for firmware builds.
- Two physical Android phones for end-to-end validation; compatible ARCore is additionally needed for AR mapping.

The current Gradle wrapper references a developer-local Gradle ZIP in `android/gradle/wrapper/gradle-wrapper.properties`. On another computer, configure `distributionUrl` for an accessible Gradle 8.9 distribution before building. Configure your own Android SDK path through Android Studio or `android/local.properties`.

Keep the existing Firebase configuration intact. A separately configured deployment needs its own matching application configuration; do not overwrite the project's `google-services.json` files as a troubleshooting shortcut.

From the repository root, in PowerShell:

```powershell
# Build both applications
.\android\gradlew.bat -p android :robot:assembleDebug :controller:assembleDebug

# Run JVM unit tests for both applications
.\android\gradlew.bat -p android :robot:testDebugUnitTest :controller:testDebugUnitTest

# Compile firmware; upload separately to the confirmed board/port
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/openbot
```

Use `--offline` only when all required dependencies and downloaded assets are already cached.

| Application | Debug APK |
| --- | --- |
| Robot | `android/robot/build/outputs/apk/debug/robot-debug.apk` |
| Controller | `android/controller/build/outputs/apk/debug/controller-debug.apk` |

Install through Android Studio or ADB. Replace the device placeholders with serials shown by `adb devices -l`:

```powershell
adb -s <robot-device> install -r android/robot/build/outputs/apk/debug/robot-debug.apk
adb -s <controller-device> install -r android/controller/build/outputs/apk/debug/controller-debug.apk
```

Update both applications together. `-r` requests an update retaining app data; changing signing keys or uninstalling can affect data retention.

## Repository guide

```text
android/
  robot/          Robot Android application, camera, AI and hardware integration
  controller/     Controller Android application and remote operator UI
  reporting/      Shared report, synchronization and map-storage source code
firmware/openbot/ ESP32 application firmware
sample.ino        Standalone motor and ultrasonic test sketch
docs/             Hardware guides, architecture notes and validation records
```

The shared `android/reporting` sources are included by both Kestral app builds; they are not a third installable application.

## Validation and current limits

The recorded 6 November 2026 validation completed both APK builds, **494 JVM tests**, and **16 physical A069 instrumentation tests**. The device checks included stationary AR camera handover, model execution, UI regressions, PDF generation, and simulated report transfer. Those results do not establish surveyed map accuracy, real-world detection accuracy, or safe autonomous navigation. The updated Controller installation and a fresh two-phone AR workflow check were pending in that validation record.

This README update itself does not rerun builds or hardware tests. Refer to the dated validation documents for the exact checks and limitations.

## Documentation

- [Hardware, wiring, firmware, and connection setup](docs/CAMPUS_HARDWARE_SETUP.md)
- [AR boundary mapping, 75% photo capture, and validation](docs/KESTRAL_AR_MAPPING_75_CAPTURE.md)
- [Navigation and mapping audit](docs/KESTRAL_NAVIGATION_MAPPING_AUDIT.md)
- [Detection review and coverage planning](docs/KESTRAL_REVIEW_COVERAGE_PHASE2.md)
- [Scanning, PDF storage, and synchronization](docs/KESTRAL_SCAN_REPORT_SYNC.md)
- [Two-phone connection troubleshooting](docs/KESTRAL_TWO_PHONE_CONNECTION_FIX.md)

## License

See [LICENSE](LICENSE) for source licensing and retained copyright notices. Third-party models, assets, and dependencies remain subject to their respective terms.
