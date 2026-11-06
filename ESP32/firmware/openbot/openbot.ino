/*
 * SerBot ESP32 firmware for the OpenBot Android app (serial protocol).
 *
 * Hardware: ESP32 dev board + TB6612FNG dual motor driver + one HC-SR04 ultrasonic sensor.
 * The phone (OpenBot "robot" app) is connected over USB-OTG and talks to this sketch through the
 * board's USB-to-serial chip at 115200 baud, 8N1. Messages are lines: <header char><body>\n
 *
 *   phone -> ESP32                           ESP32 -> phone
 *   c<left>,<right>   wheel commands         s<cm>            sonar distance (-1 = no valid echo)
 *                     -255..255              f<type>:s:       configuration announcement
 *   h<ms>             heartbeat
 *   s<ms>             sonar interval
 *   f                 request configuration
 *   w<ms>             encoder report interval (optional encoders are disabled by default)
 *
 * SAFETY BEHAVIOUR (all of it is in this file, none of it depends on the phone):
 *  - The motors stay OFF until the first heartbeat arrives, and are stopped again if the heartbeat
 *    stops for longer than its interval. A heartbeat timeout also CLEARS the wheel command, so a
 *    returning heartbeat can never restart an old command: a new "c" message is required.
 *  - Any positive wheel output is blocked on invalid/stale sonar or a distance at or below
 *    HW_STOP_DISTANCE_CM. This is only a last-resort backstop below the app's own 40 cm stop.
 *  - Malformed or over-long messages are ignored and never change the motors.
 *  - Outputs are put in a safe state (driver in standby, everything off) before anything else.
 *
 * NO SERIAL DEBUG PRINTS: everything printed on this port is read by the app as protocol data.
 *
 * EVERYTHING in the "USER CONFIGURATION" block below must match YOUR wiring. The motor defaults avoid the
 * ESP32 strapping pins (0, 2, 12, 15), the flash pins (6-11) and the input-only pins (34-39).
 * Motor and sonar pins, forward polarity, PWM frequency and coast stopping match the working
 * repository-root sample.ino. The app protocol and watchdogs still require on-robot validation.
 */

#include <Arduino.h>
#include <errno.h>

// =============================== USER CONFIGURATION ===========================================

// ---- TB6612FNG wiring (ESP32 GPIO numbers). Motor A drives the LEFT wheel, motor B the RIGHT. ----
static const uint8_t PIN_AIN1 = 27;  // KESTRAL: LEFT motor direction 1
static const uint8_t PIN_AIN2 = 26;  // KESTRAL: LEFT motor direction 2
static const uint8_t PIN_PWMA = 25;  // KESTRAL: LEFT motor speed (PWM)
static const uint8_t PIN_BIN1 = 33;  // KESTRAL: RIGHT motor direction 1
static const uint8_t PIN_BIN2 = 32;  // KESTRAL: RIGHT motor direction 2
static const uint8_t PIN_PWMB = 18;  // KESTRAL: RIGHT motor speed (PWM)
static const uint8_t PIN_STBY = 14;  // KESTRAL: driver enable: HIGH = running, LOW = standby

// ---- HC-SR04 wiring. The ECHO output is 5 V: it MUST go through a divider (for example 1 kohm in
// series + 2 kohm to ground) or a level shifter before it reaches the ESP32 pin (3.3 V only). ----
static const uint8_t PIN_TRIG = 5;   // Existing HC-SR04 wiring; GPIO5 is a boot strapping pin
static const uint8_t PIN_ECHO = 22;  // KESTRAL HC-SR04 ECHO; use a voltage divider/level shifter

// sample.ino and the confirmed hardware list have no encoder wiring.
// Enable only after physical quadrature encoders have been connected to the pins below.
static const bool ENABLE_ENCODERS = false;
// Quadrature encoders: A edges are counted on CHANGE (x2 decoding), B gives direction.
// GPIO34-39 require EXTERNAL pull-ups when the encoder output is open collector.
// All signals MUST be 3.3V. These pins are for classic ESP32 only.
static const uint8_t PIN_ENCODER_LEFT_A = 34;
static const uint8_t PIN_ENCODER_LEFT_B = 35;
static const uint8_t PIN_ENCODER_RIGHT_A = 36;
static const uint8_t PIN_ENCODER_RIGHT_B = 39;
static const bool INVERT_ENCODER_LEFT = false;
static const bool INVERT_ENCODER_RIGHT = false;
// Measured A CHANGE counts per WHEEL revolution, including gearbox. 0 = uncalibrated:
// raw measured counts are sent, RPM is unavailable (never invent a conversion).
static const uint32_t ENCODER_COUNTS_PER_WHEEL_REV = 0;

// ---- Direction and side corrections (change these if the robot drives the wrong way) ----
static const bool SWAP_SIDES = false;    // true: the "left" command drives motor B, "right" motor A
static const bool INVERT_LEFT = true;    // sample.ino forward: AIN1 LOW, AIN2 HIGH
static const bool INVERT_RIGHT = true;   // sample.ino forward: BIN1 LOW, BIN2 HIGH

// ---- Motor behaviour ----
static const bool STOP_WITH_BRAKE = false;    // matches sample.ino: PWM 0, both direction pins LOW
static const uint32_t PWM_FREQ_HZ = 1000;     // matches the working sample.ino
static const uint8_t PWM_RESOLUTION_BITS = 8; // duty range 0..255
static const int MAX_PWM = 255;               // upper limit of the duty cycle (0..255)
// Smallest duty at which your motors really turn. 0 = no compensation (the command value is used
// as it is). A value above 0 maps every non-zero command into [MOTOR_MIN_PWM, MAX_PWM] so that the
// small commands the app sends for slow approaches do not stall. Tune it with the wheels lifted.
static const int MOTOR_MIN_PWM = 0;

// ---- Serial link ----
static const uint32_t SERIAL_BAUD = 115200;   // must match the baud rate set in the OpenBot app

// ---- Sonar ----
static const int HW_STOP_DISTANCE_CM = 30;    // KESTRAL safety stop: block forward motion at/below 30 cm
static const int SONAR_MEDIAN_SAMPLES = 3;    // 1 = raw readings, 3 or 5 = median filter
// ============================= END OF USER CONFIGURATION ======================================

// ---- Protocol constants (they must agree with the OpenBot app and with the Batch 2 mission code) ----
static const char* const ROBOT_TYPE = "DIY";  // the app only uses it to pick an icon
static const int MAX_SONAR_CM = 300;          // exclusive upper measurement limit
static const uint32_t MAX_ECHO_US = 17500UL;  // an echo longer than this is beyond 300 cm
static const float US_TO_CM = 0.01715f;       // speed of sound, one way: cm per microsecond
static const uint32_t SONAR_TIMEOUT_US = 30000UL;
static const unsigned long SONAR_MIN_CYCLE_MS = 60UL;  // HC-SR04 needs about 60 ms between pings
static const unsigned long SONAR_MAX_INTERVAL_MS = 5000UL;
static const unsigned long DEFAULT_SONAR_INTERVAL_MS = 100UL;
static const unsigned long HEARTBEAT_MIN_MS = 100UL;
static const unsigned long HEARTBEAT_MAX_MS = 5000UL;
static const unsigned long ANNOUNCE_PERIOD_MS = 2000UL;
static const unsigned long PHONE_ABSENT_MS = 3000UL;
static const unsigned long COMMAND_TIMEOUT_MS = 1000UL;
static unsigned long lastCommandMs = 0;
static unsigned long lastEncoderReportMs = 0;
static unsigned long encoderIntervalMs = 250;
static volatile int32_t encoderLeft = 0;
static volatile int32_t encoderRight = 0;
static portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
static bool sonarValid = false;
static unsigned long lastValidSonarMs = 0;
static const size_t MSG_MAX = 32;
static const int MAX_COMMAND = 255;

static_assert(SONAR_MEDIAN_SAMPLES == 1 || SONAR_MEDIAN_SAMPLES == 3 || SONAR_MEDIAN_SAMPLES == 5,
              "SONAR_MEDIAN_SAMPLES must be 1, 3 or 5");
static_assert(MAX_PWM > 0 && MAX_PWM <= 255, "MAX_PWM must be 1..255");
static_assert(MOTOR_MIN_PWM >= 0 && MOTOR_MIN_PWM < MAX_PWM, "MOTOR_MIN_PWM must be below MAX_PWM");

// ESP32 Arduino core 3.x changed the PWM (ledc) API; core 2.x and 1.x use channels.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define SERBOT_PWM_BY_PIN 1
#else
#define SERBOT_PWM_BY_PIN 0
#endif
static const uint8_t PWM_CHANNEL_A = 0;
static const uint8_t PWM_CHANNEL_B = 1;

// =============================== state ========================================================

// Message assembly.
static char msgBuf[MSG_MAX + 1];
static size_t msgLen = 0;
static bool msgOverflow = false;

// Wheel commands and heartbeat.
static int ctrlLeft = 0;
static int ctrlRight = 0;
static bool heartbeatSeen = false;
static unsigned long heartbeatIntervalMs = 0;
static unsigned long lastHeartbeatMs = 0;

// Configuration announcement handshake.
static bool configAcked = false;
static unsigned long lastAnnounceMs = 0;

// Sonar.
enum SonarState { SONAR_IDLE, SONAR_WAITING };
static SonarState sonarState = SONAR_IDLE;
static unsigned long sonarIntervalMs = DEFAULT_SONAR_INTERVAL_MS;
static unsigned long lastPingMs = 0;
static uint32_t pingStartUs = 0;
static int sonarWindow[5];
static int sonarWindowCount = 0;
static int sonarWindowPos = 0;
static int distanceCm = -1;  // filtered distance; "no echo" until measured
static volatile uint32_t echoRiseUs = 0;
static volatile uint32_t echoFallUs = 0;
static volatile bool echoArmed = false;
static volatile bool echoRisen = false;
static volatile bool echoDone = false;

// Last values written to the driver (to avoid rewriting identical outputs).
static int appliedLeft = 100000;
static int appliedRight = 100000;

// =============================== output helpers ===============================================

static void sendLine(char header, long value) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%c%ld", header, value);
  Serial.println(buf);
}

static void sendConfig() {
  char buf[32];
  const char* format = !ENABLE_ENCODERS ? "f%s:s:"
      : (ENCODER_COUNTS_PER_WHEEL_REV > 0 ? "f%s:s:enc:wf:" : "f%s:s:enc:");
  snprintf(buf, sizeof(buf), format, ROBOT_TYPE);
  Serial.println(buf);
}

static void pwmBegin(uint8_t pin, uint8_t channel) {
#if SERBOT_PWM_BY_PIN
  (void)channel;
  ledcAttach(pin, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
#else
  ledcSetup(channel, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
  ledcAttachPin(pin, channel);
#endif
}

static void pwmWrite(uint8_t pin, uint8_t channel, uint32_t duty) {
#if SERBOT_PWM_BY_PIN
  (void)channel;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(channel, duty);
#endif
}

/** Drives one TB6612FNG channel. cmd is -255..255 (already corrected for polarity). */
static void driveMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin, uint8_t channel, int cmd) {
  if (cmd == 0) {
    if (STOP_WITH_BRAKE) {
      digitalWrite(in1, HIGH);  // IN1 = IN2 = HIGH with PWM high is the short brake
      digitalWrite(in2, HIGH);
      pwmWrite(pwmPin, channel, 255);
    } else {
      digitalWrite(in1, LOW);   // IN1 = IN2 = LOW stops the motor
      digitalWrite(in2, LOW);
      pwmWrite(pwmPin, channel, 0);
    }
    return;
  }
  int magnitude = cmd < 0 ? -cmd : cmd;  // 1..255
  int duty;
  if (MOTOR_MIN_PWM > 0) {
    duty = MOTOR_MIN_PWM + ((MAX_PWM - MOTOR_MIN_PWM) * magnitude) / MAX_COMMAND;
  } else {
    duty = (MAX_PWM * magnitude) / MAX_COMMAND;
  }
  if (duty > MAX_PWM) duty = MAX_PWM;
  if (duty < 0) duty = 0;
  if (cmd > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  pwmWrite(pwmPin, channel, (uint32_t)duty);
}

/** Writes both wheels. left/right are the logical wheel commands (-255..255). */
static void writeMotors(int left, int right) {
  if (INVERT_LEFT) left = -left;
  if (INVERT_RIGHT) right = -right;
  int a = SWAP_SIDES ? right : left;
  int b = SWAP_SIDES ? left : right;
  driveMotor(PIN_AIN1, PIN_AIN2, PIN_PWMA, PWM_CHANNEL_A, a);
  driveMotor(PIN_BIN1, PIN_BIN2, PIN_PWMB, PWM_CHANNEL_B, b);
}

/** Applies the wheel commands, after the hardware obstacle backstop. */
static void applyOutputs() {
  int left = ctrlLeft;
  int right = ctrlRight;
  // Last-resort backstop also covers forward arcs and pivots with a positive wheel.
  const bool sonarUnsafe = !sonarValid || millis() - lastValidSonarMs > 500UL
      || distanceCm <= HW_STOP_DISTANCE_CM;
  if ((left > 0 || right > 0) && sonarUnsafe) {
    // Latch a stop: a fresh command is required after the obstacle/sensor fault clears.
    ctrlLeft = 0;
    ctrlRight = 0;
    left = 0;
    right = 0;
  }
  if (left != appliedLeft || right != appliedRight) {
    writeMotors(left, right);
    appliedLeft = left;
    appliedRight = right;
  }
}

// =============================== message parsing ==============================================

/** Parses a whole decimal integer (optional sign). Returns false for anything else. */
static bool parseWholeLong(const char* text, long* out) {
  if (text == NULL || *text == '\0') return false;
  char* end = NULL;
  errno = 0;
  long value = strtol(text, &end, 10);
  if (errno == ERANGE) return false;
  if (end == text || *end != '\0') return false;
  *out = value;
  return true;
}

/** Parses "<int>,<int>". Returns false if the text is not exactly that. */
static bool parseTwoInts(const char* text, long* first, long* second) {
  if (text == NULL || *text == '\0') return false;
  char* end = NULL;
  errno = 0;
  long a = strtol(text, &end, 10);
  if (errno == ERANGE) return false;
  if (end == text || *end != ',') return false;
  const char* rest = end + 1;
  char* end2 = NULL;
  errno = 0;
  long b = strtol(rest, &end2, 10);
  if (errno == ERANGE) return false;
  if (end2 == rest || *end2 != '\0') return false;
  *first = a;
  *second = b;
  return true;
}

static int clampCommand(long value) {
  if (value > MAX_COMMAND) return MAX_COMMAND;
  if (value < -MAX_COMMAND) return -MAX_COMMAND;
  return (int)value;
}

static unsigned long clampUnsigned(long value, unsigned long low, unsigned long high) {
  if (value < (long)low) return low;
  if ((unsigned long)value > high) return high;
  return (unsigned long)value;
}

static void handleMessage(const char* line) {
  const char header = line[0];
  const char* body = line + 1;
  long a = 0;
  long b = 0;
  switch (header) {
    case 'c':
      if (parseTwoInts(body, &a, &b)) {
        if (!heartbeatSeen || millis() - lastHeartbeatMs >= heartbeatIntervalMs) break;
        lastCommandMs = millis();
        ctrlLeft = clampCommand(a);
        ctrlRight = clampCommand(b);
      }
      break;
    case 'h':
      if (parseWholeLong(body, &a) && a > 0) {
        heartbeatIntervalMs = clampUnsigned(a, HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS);
        lastHeartbeatMs = millis();
        heartbeatSeen = true;
      }
      break;
    case 's':
      if (parseWholeLong(body, &a) && a > 0) {
        sonarIntervalMs = clampUnsigned(a, SONAR_MIN_CYCLE_MS, SONAR_MAX_INTERVAL_MS);
        // The phone only sends this after it has processed our configuration announcement.
        configAcked = true;
      }
      break;
    case 'w':
      if (parseWholeLong(body, &a) && a > 0) {
        encoderIntervalMs = clampUnsigned(a, 100UL, 2000UL);
      }
      break;
    case 'f':
      sendConfig();
      break;
    default:
      break;  // unknown or unsupported message: ignore
  }
}

static void pollSerial() {
  int budget = 128;  // bounded work per pass so the loop stays responsive
  while (budget-- > 0 && Serial.available() > 0) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\n') {
      if (!msgOverflow && msgLen > 0) {
        msgBuf[msgLen] = '\0';
        handleMessage(msgBuf);
      }
      msgLen = 0;
      msgOverflow = false;
    } else if (ch == '\r') {
      // ignore carriage returns
    } else if (msgLen < MSG_MAX) {
      msgBuf[msgLen++] = (char)ch;
    } else {
      msgOverflow = true;  // over-long line: dropped completely when its end arrives
    }
  }
}

// =============================== sonar ========================================================

void IRAM_ATTR onEchoEdge() {
  const uint32_t now = micros();
  if (!echoArmed) return;
  if (digitalRead(PIN_ECHO)) {
    echoRiseUs = now;
    echoRisen = true;
  } else if (echoRisen) {
    echoFallUs = now;
    echoDone = true;
    echoArmed = false;
  }
}

static void startPing() {
  echoRisen = false;
  echoDone = false;
  echoArmed = true;  // armed BEFORE the trigger so no edge of this ping can be missed
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  pingStartUs = micros();
  lastPingMs = millis();
  sonarState = SONAR_WAITING;
}

static int medianOfWindow() {
  int tmp[5];
  const int n = sonarWindowCount;
  for (int i = 0; i < n; i++) tmp[i] = sonarWindow[i];
  for (int i = 1; i < n; i++) {  // insertion sort of at most 5 values
    int v = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > v) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = v;
  }
  return tmp[n / 2];
}

static void finishMeasurement(int cm) {
  sonarValid = cm > 0 && cm < MAX_SONAR_CM;
  if (!sonarValid) {
    distanceCm = -1;
    sonarWindowCount = 0;
    sonarWindowPos = 0;
    sendLine('s', -1);
    sonarState = SONAR_IDLE;
    return;
  }
  lastValidSonarMs = millis();
  if (cm < 1) cm = 1;  // 0 would look like an invalid reading to the app
  if (cm > MAX_SONAR_CM) cm = MAX_SONAR_CM;
  sonarWindow[sonarWindowPos] = cm;
  sonarWindowPos = (sonarWindowPos + 1) % SONAR_MEDIAN_SAMPLES;
  if (sonarWindowCount < SONAR_MEDIAN_SAMPLES) sonarWindowCount++;
  distanceCm = min(cm, medianOfWindow()); // a new close obstacle stops immediately
  sendLine('s', distanceCm);
  sonarState = SONAR_IDLE;
}

static void sonarStep(unsigned long nowMs, uint32_t nowUs) {
  if (sonarState == SONAR_IDLE) {
    if (nowMs - lastPingMs >= sonarIntervalMs) {
      startPing();
    }
    return;
  }
  if (echoDone) {
    const uint32_t duration = echoFallUs - echoRiseUs;
    if (duration > MAX_ECHO_US) {
      finishMeasurement(-1);  // beyond range
    } else {
      finishMeasurement((int)((float)duration * US_TO_CM + 0.5f));
    }
  } else if ((uint32_t)(nowUs - pingStartUs) > SONAR_TIMEOUT_US) {
    echoArmed = false;
    finishMeasurement(-1);  // no echo
  }
}

void IRAM_ATTR onLeftEncoder() {
  const int delta = (digitalRead(PIN_ENCODER_LEFT_A) == digitalRead(PIN_ENCODER_LEFT_B)) ? 1 : -1;
  portENTER_CRITICAL_ISR(&encoderMux);
  encoderLeft = (int32_t)((uint32_t)encoderLeft + (INVERT_ENCODER_LEFT ? -delta : delta));
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR onRightEncoder() {
  const int delta = (digitalRead(PIN_ENCODER_RIGHT_A) == digitalRead(PIN_ENCODER_RIGHT_B)) ? 1 : -1;
  portENTER_CRITICAL_ISR(&encoderMux);
  encoderRight = (int32_t)((uint32_t)encoderRight + (INVERT_ENCODER_RIGHT ? -delta : delta));
  portEXIT_CRITICAL_ISR(&encoderMux);
}

static void reportEncoders(unsigned long nowMs) {
  if (!ENABLE_ENCODERS) return;
  const unsigned long elapsed = nowMs - lastEncoderReportMs;
  if (elapsed < encoderIntervalMs) return;
  int32_t left, right;
  portENTER_CRITICAL(&encoderMux);
  left = encoderLeft;
  right = encoderRight;
  portEXIT_CRITICAL(&encoderMux);
  Serial.printf("e%ld,%ld\n", (long)left, (long)right);
  static int32_t previousLeft = 0, previousRight = 0;
  if (ENCODER_COUNTS_PER_WHEEL_REV > 0) {
    const float scale = 60000.0f / ((float)ENCODER_COUNTS_PER_WHEEL_REV * elapsed);
    Serial.printf("w%.2f,%.2f\n",
        (int32_t)((uint32_t)left - (uint32_t)previousLeft) * scale,
        (int32_t)((uint32_t)right - (uint32_t)previousRight) * scale);
  }
  previousLeft = left;
  previousRight = right;
  lastEncoderReportMs = nowMs;
}

// =============================== setup and loop ===============================================

void setup() {
  // 1. Safe outputs first: driver in standby, nothing driven.
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, LOW);
  const uint8_t dirPins[4] = {PIN_AIN1, PIN_AIN2, PIN_BIN1, PIN_BIN2};
  for (int i = 0; i < 4; i++) {
    pinMode(dirPins[i], OUTPUT);
    digitalWrite(dirPins[i], LOW);
  }
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT);
  pwmBegin(PIN_PWMA, PWM_CHANNEL_A);
  pwmBegin(PIN_PWMB, PWM_CHANNEL_B);
  pwmWrite(PIN_PWMA, PWM_CHANNEL_A, 0);
  pwmWrite(PIN_PWMB, PWM_CHANNEL_B, 0);

  // 2. Stopped state (brake or coast), then enable the driver.
  writeMotors(0, 0);
  appliedLeft = 0;
  appliedRight = 0;
  digitalWrite(PIN_STBY, HIGH);

  // 3. Sonar.
  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO), onEchoEdge, CHANGE);

  if (ENABLE_ENCODERS) {
    pinMode(PIN_ENCODER_LEFT_A, INPUT);
    pinMode(PIN_ENCODER_LEFT_B, INPUT);
    pinMode(PIN_ENCODER_RIGHT_A, INPUT);
    pinMode(PIN_ENCODER_RIGHT_B, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_LEFT_A), onLeftEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_RIGHT_A), onRightEncoder, CHANGE);
  }

  // 4. Serial link, and a first configuration announcement.
  Serial.begin(SERIAL_BAUD);
  lastPingMs = millis();
  lastAnnounceMs = millis();
  sendConfig();
}

void loop() {
  pollSerial();
  const unsigned long nowMs = millis();

  // No valid heartbeat: the wheel command is cleared, so a returning heartbeat cannot restart it.
  const bool heartbeatActive =
      heartbeatSeen && (unsigned long)(nowMs - lastHeartbeatMs) < heartbeatIntervalMs;
  if (!heartbeatActive || nowMs - lastCommandMs >= COMMAND_TIMEOUT_MS) {
    ctrlLeft = 0;
    ctrlRight = 0;
  }

  // If the phone has gone away, the next one needs our configuration announcement again.
  if (heartbeatSeen && (unsigned long)(nowMs - lastHeartbeatMs) > PHONE_ABSENT_MS) {
    configAcked = false;
  }

  sonarStep(nowMs, micros());
  applyOutputs();
  reportEncoders(nowMs);

  // Announce the configuration until the phone has acknowledged it (it answers with "s<ms>").
  // The app only learns that a sonar exists from this message, and a mission needs the sonar.
  if (!configAcked && (unsigned long)(nowMs - lastAnnounceMs) >= ANNOUNCE_PERIOD_MS) {
    lastAnnounceMs = nowMs;
    sendConfig();
  }
}