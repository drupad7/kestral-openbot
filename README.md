# Kestral OpenBot

**Campus waste detection, remote inspection, and reviewed mission reports.**

Kestral OpenBot combines a two-wheel ESP32 robot with two Android applications. The **Robot App** runs on the robot-mounted phone and handles the camera, YOLO11s waste detection, hardware commands, and mission records. The **Controller App** runs on the operator's phone and provides driving controls, live video, mapping requests, detection review, and report access.

## Project documentation

Start with the setup guide, then open the guide for the feature you want to use.

| Guide | Contents |
| --- | --- |
| [Robot App](docs/kestral/robot-app/README.md) | Robot phone responsibilities, modes, orientation, and safety |
| [Controller App](docs/kestral/controller-app/README.md) | Remote driving, motor controls, live monitoring, and mode synchronization |
| [Hardware](docs/kestral/hardware/README.md) | ESP32, TB6612FNG, motors, HC-SR04, wiring, and power |
| [Setup and installation](docs/kestral/setup/README.md) | Build both apps, install APKs, configure firmware, and connect devices |
| [Waste detection and scanning](docs/kestral/waste-detection/README.md) | Nine waste categories, confidence thresholds, photo capture, and deduplication |
| [Mapping and route planning](docs/kestral/mapping/README.md) | AR boundary recording, saved maps, coverage previews, and autonomy limits |
| [Review, reports, and synchronization](docs/kestral/reports/README.md) | Confirm/Remove/Categories, PDF generation, storage, and offline recovery |
| [Troubleshooting](docs/kestral/troubleshooting/README.md) | Connection, motor, camera, mapping, and report problems |

## System overview

```text
Controller phone
  Remote driving, live monitoring, detection review
        |
        | Wi-Fi / Nearby commands and status
        | WebRTC camera stream
        v
Robot phone
  Camera, YOLO11s, safety checks, maps, mission records, PDFs
        |
        | USB OTG serial: 115200 baud, 8N1
        v
ESP32 Dev Module
  TB6612FNG -> Left and right DC motors
  HC-SR04  -> Forward ultrasonic readings
```

The Robot is the authoritative source of mission state, maps, detections, and reports. The Controller receives synchronized copies. Images and PDFs transfer separately from live video.

## Main features

- Manual differential driving with press-and-hold controls, release-to-stop, and Emergency Stop.
- Separate individual motor control through the existing motor ownership system.
- Free Roam with normal video and no waste inference.
- Waste Classification with the existing YOLO11s TensorFlow Lite model.
- Independent scanning controls: default live threshold **65%**, scanning photo eligibility **75% or higher**.
- Actual camera photographs, duplicate suppression, and available capture-location metadata.
- Detection review before approved PDF generation.
- Reports stored in `Documents/Kestral OpenBot/Reports/` on each device, with verified transfer and persistent pending jobs.
- Robot-side AR boundary drafts and dimension-aware zigzag route previews.

Robot UI is portrait normally; Full Screen enters landscape and Exit/Back restores portrait. Controller UI remains landscape.

## Current scope

The confirmed drive hardware is an ESP32 Dev Module, TB6612FNG, two DC motors, and HC-SR04. Optional encoder support is disabled by default; calibrated odometry is not assumed.

**Physical autonomous coverage remains disabled until localization, persistent relocalization, mount calibration, and hardware safety are validated.** A route preview does not establish safe autonomous movement. No automatic waste pickup mechanism is implemented.

The dated [AR and capture validation record](docs/KESTRAL_AR_MAPPING_75_CAPTURE.md) documents prior builds and device tests. Documentation changes do not imply new hardware testing.

## License

See [LICENSE](LICENSE) for source licensing and retained copyright notices. Third-party models, assets, and dependencies retain their respective terms.
