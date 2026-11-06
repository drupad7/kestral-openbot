# Kestral OpenBot — Hardware README

> **Project:** Smartphone-based autonomous campus waste detection and mapping robot  
> **Platform:** Customized OpenBot (Android Robot App + Controller App + ESP32)  
> **Scope:** Hardware assembly, wiring checks, power, sensor setup, and AR mapping prerequisites. The robot **detects and maps waste; it does not collect waste**.

## 1. System overview

```text
Controller Android phone
    |  Wi-Fi / existing WebRTC and command link
    v
Robot Android phone (camera, YOLO TFLite, optional ARCore)
    |  Existing phone-to-controller-board connection (verify USB/Bluetooth implementation)
    v
ESP32 controller
    |--------------------|
    v                    v
Motor driver          HC-SR04 ultrasonic sensor
    |
Left/right DC motors

Battery -> correctly rated motor supply and regulated electronics supply
All connected controller/sensor/driver logic must share a common ground.
```

The Robot phone handles camera streaming, waste detection, and (on compatible devices) AR tracking. The ESP32 handles low-level motor commands and ultrasonic sensing. The Controller phone provides remote driving and mission controls.

## 2. Hardware checklist

| Component | Purpose | Notes |
|---|---|---|
| Android Robot phone | Camera, AI inference, optional ARCore, robot app | Check Android `minSdk`, ARCore device support, camera performance |
| Android Controller phone | Remote control, mapping UI, report review | Controller app runs landscape |
| ESP32 board | Motor/sensor control | Identify exact board and firmware pin definitions |
| Motor driver board | Drives DC motors | **Model not yet confirmed**; verify voltage/current ratings |
| DC geared motors and wheels | Locomotion | Match driver and battery ratings |
| Robot chassis and phone mount | Mechanical support | Secure, rigid phone mounting required for AR |
| HC-SR04 ultrasonic sensor (if this is the fitted model) | Forward obstacle detection | 5 V supply and ECHO level shifting needed for standard ESP32 |
| Battery, switch, regulators, wiring | Power | Provide fuse/protection and separate appropriately regulated rails |
| Resistors / level shifter | Protect ESP32 from 5 V ECHO | Example divider below |

**Not required for this build:** MPU6050, LiDAR, or additional sensors. Do not assume wheel encoders or other hardware are present without checking the physical robot.

## 3. Wiring — ultrasonic sensor (HC-SR04 example)

| HC-SR04 pin | Connection | Caution |
|---|---|---|
| VCC | Regulated 5 V | Check the exact sensor module rating |
| GND | ESP32/system GND | Common ground |
| TRIG | Free, output-capable ESP32 GPIO | **GPIO assignment must match firmware** |
| ECHO | Free, input-capable ESP32 GPIO **via level shifter/divider** | Do not feed 5 V directly into 3.3 V GPIO |

**Example only (not confirmed project wiring):** TRIG → GPIO 5; ECHO → GPIO 18 via divider. Check that these pins are unused and suitable for the actual ESP32 board before connecting them. Avoid changing existing firmware pin assignments without updating the wiring.

Example 5 V-to-3.3 V resistor divider:

```text
HC-SR04 ECHO ---[ 1 kΩ ]---+---- ESP32 ECHO input
                            |
                          [ 2 kΩ ]
                            |
                           GND
```

This gives approximately 3.33 V for a 5 V ECHO pulse. A suitable logic-level shifter is another option. ESP32 sends a roughly 10 µs trigger pulse and measures the ECHO duration to estimate distance. One forward-facing sensor does **not** provide all-around obstacle coverage.

## 4. Motor driver and power connections

**Do not guess motor-control GPIOs.** Open the existing ESP32 firmware and record the real assignments before wiring.

| Connection | Requirement |
|---|---|
| ESP32 → motor-driver inputs | Match firmware-defined direction/PWM/enable pins |
| Driver outputs → motors | Match left/right polarity; correct in firmware only after safe tests |
| Battery → driver motor supply | Correct voltage and adequate current for motors |
| Regulator → ESP32 supply | Correct regulated input for the actual ESP32 board |
| Supply → ultrasonic VCC | Regulated 5 V for standard HC-SR04 |
| ESP32 GND ↔ driver GND ↔ sensor GND | Shared reference ground |
| Emergency stop / power isolation | Ensure motors can be stopped safely |

Do not power motors directly from ESP32 GPIO. Do not connect a motor battery directly to the ESP32's 3.3 V pin. Confirm motor-driver current capacity, battery polarity, regulator heat, and wiring before power-up.

### Firmware pin checklist (fill from actual repository)

```text
ESP32 board model:        __________________
Motor driver model:       __________________
Left motor direction:     __________________
Right motor direction:    __________________
Motor PWM/enable pins:    __________________
Ultrasonic TRIG GPIO:     __________________
Ultrasonic ECHO GPIO:     __________________
Robot phone link type:    __________________
Battery voltage/type:     __________________
5 V regulator rating:     __________________
```

## 5. Phone installation and mounting

1. Install the existing **Robot App** on the robot-mounted Android phone and **Controller App** on the remote phone.
2. Verify that the Robot App's Android `minSdk` supports the phone OS; Android 9 is API 28, but compatibility also depends on app libraries and hardware.
3. Confirm the existing ESP32-to-Robot-phone connection method in the repository and test manual motor commands **with wheels raised off the floor first**.
4. Mount the Robot phone rigidly so its rear camera has a useful view of the floor and surrounding features. Avoid vibration and loose mounts.
5. Measure the camera's translation and rotation relative to the robot's reference point (typically the center between drive wheels). Record mounting height and tilt.
6. Check camera rotation, YOLO box alignment, and WebRTC continuity while switching the Robot App between portrait and landscape Full Screen.

## 6. AR mapping hardware requirements

The planned **Autonomous Mission → New Map** workflow is:

1. The Robot App runs AR tracking and sends video/status to the Controller.
2. The Controller shows manual driving controls plus **Start / Add Point / End**.
3. **Start** records the robot's current validated floor pose as the first vertex; it **does not** start autonomous driving.
4. Drive the robot manually to boundary corners; **Add Point** records the current robot pose at each corner.
5. **End** becomes available after Start plus at least two distinct added points. It records the final vertex and closes the polygon by connecting End to Start.
6. Validate the polygon and save it with a name. The straight closing edge is **not** a motor-driving command.

ARCore requires a supported phone/device configuration; Android version alone does not guarantee support. Check Google's [ARCore supported devices](https://developers.google.com/ar/devices). The camera pipeline must coordinate ARCore, WebRTC, and YOLO without conflicting camera ownership. The app must convert camera pose to **robot** pose using the measured mount transform. Tracking quality and coordinate accuracy require real-device validation.

**Safety:** A saved polygon is not automatically a navigable map. Keep autonomous motor commands disabled until real-world localization, map relocalization after restart, route clearance, ultrasonic stopping, and Emergency Stop have been validated. If tracking is unavailable, save only a **visual-only draft**.

## 7. Waste detection and reporting hardware behavior

- The existing YOLO11s TFLite model runs on the Robot phone.
- Live detections may be shown from **65% confidence**.
- While **Start Scanning** is active, qualifying detections at **75% confidence or above** should trigger saved camera photos, subject to deduplication.
- Controller review retains **Confirm / Remove / Categories**; only confirmed detections and their actual photos go into the finalized PDF.
- Robot stores the authoritative map and generated reports; Controller receives synchronized copies.
- Recommended map files location (implementation must be verified): app-specific `files/Maps/` on each phone. Report output target: `Documents/Kestral OpenBot/Reports/`.

## 8. Safe first-power-on checklist

- [ ] Identify ESP32 board, motor driver, and firmware GPIO mapping.
- [ ] Verify battery voltage, polarity, fuse, regulators, and common ground.
- [ ] Confirm ECHO level shifting before connecting ultrasonic sensor.
- [ ] Test ESP32 connection and ultrasonic distance with motors disabled.
- [ ] Test motor direction and Stop/Emergency Stop with wheels lifted.
- [ ] Confirm Robot/Controller connection and safe stop on disconnection.
- [ ] Test camera, WebRTC, YOLO detection, and 75% photo capture.
- [ ] Check phone mount calibration and ARCore compatibility.
- [ ] Validate AR pose accuracy and saved-map relocalization on actual hardware.
- [ ] Keep physical autonomous driving disabled until all relevant safety checks pass.

## 9. Known unknowns / project-specific verification

This README documents the **intended** hardware arrangement; it does **not** assert that the following have been inspected or hardware-tested:

- Exact ESP32 model and firmware pin assignments.
- Motor driver model, motor ratings, and power wiring.
- Actual phone-to-ESP32 transport.
- Robot phone's ARCore compatibility and camera-sharing capability.
- Measured camera-to-robot transform and mapping accuracy.
- Autonomous navigation and safety-stop performance.

Update the tables above using the actual robot, firmware, and tested wiring before treating this README as a final assembly guide.
