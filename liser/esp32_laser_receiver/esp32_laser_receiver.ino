// Laser receiver (ESP32-S3)
// Receives ESP-NOW messages from esp32_motion_sensor and keeps the laser on
// from "motion start" until "motion end". While the laser is on, the SG90 servo
// it is mounted on sweeps left and right; when it turns off, the servo returns to center.
//
// Wiring:
//   Laser: signal -> GPIO2, GND -> GND (the same wire that went to GPIO26 on the Raspberry Pi)
//   SG90:  orange (signal) -> GPIO4, red -> 5V, brown -> GND

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ---------------- Settings ----------------
#define LASER_PIN 2
#define SERVO_PIN 4

// true = the laser module turns on when the pin is LOW
const bool LASER_ACTIVE_LOW = true;

const uint8_t ESPNOW_CHANNEL = 1;  // must match ESPNOW_CHANNEL in esp32_motion_sensor

// Safety: turn the laser off if "motion end" never arrives (e.g. the message was lost). 0 = never
const unsigned long LASER_MAX_ON_MS = 10UL * 60 * 1000;

// Servo sweep - small fast wiggle around the center
const int SERVO_CENTER = 90;
const int SWEEP_MIN_ANGLE = 85;
const int SWEEP_MAX_ANGLE = 95;
const int SWEEP_STEP_DEG = 12;           // bigger = faster sweep (>= the range: jump straight from end to end)
const unsigned long SWEEP_STEP_MS = 30;  // smaller = faster sweep (no point going below 20 - the servo updates every 20ms)

// Test: on every power-up, turn the laser on and wiggle the servo for this long. 0 = no test
const unsigned long BOOT_TEST_MS = 3000;

// SG90 pulse range (0..180 degrees). Narrow it if the servo buzzes at the ends.
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;
const int SERVO_FREQ_HZ = 50;
const int SERVO_RES_BITS = 14;

// Must be identical to the message in esp32_motion_sensor
enum : uint8_t { MSG_MOTION_END = 0, MSG_MOTION_START = 1 };

typedef struct __attribute__((packed)) {
  uint8_t type;  // MSG_MOTION_START / MSG_MOTION_END
  uint32_t motionCount;
  uint32_t bootCount;
} MotionMsg;

volatile bool gotMsg = false;
volatile uint8_t lastType = MSG_MOTION_END;

bool laserOn = false;
unsigned long laserOnSince = 0;
unsigned long laserOffSince = 0;
bool bootTest = false;
unsigned long bootTestStart = 0;

int servoAngle = SERVO_CENTER;
int sweepDir = 1;
unsigned long lastServoStep = 0;
bool servoActive = false;

// The receive callback signature changed in Arduino-ESP32 core 3.0.0
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(MotionMsg)) return;
  lastType = ((const MotionMsg *)data)->type;
  gotMsg = true;  // handled in loop(), not in the Wi-Fi task
}

void servoWrite(int angle) {
  uint32_t us = map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
  uint32_t periodUs = 1000000UL / SERVO_FREQ_HZ;
  ledcWrite(SERVO_PIN, us * ((1UL << SERVO_RES_BITS) - 1) / periodUs);
  servoActive = true;
}

void setLaser(bool on) {
  bool level = LASER_ACTIVE_LOW ? !on : on;
  digitalWrite(LASER_PIN, level ? HIGH : LOW);
  laserOn = on;
  if (on) {
    laserOnSince = millis();
  } else {
    laserOffSince = millis();
    servoAngle = SERVO_CENTER;
    servoWrite(servoAngle);
  }
}

void updateServo() {
  unsigned long now = millis();

  if (laserOn || bootTest) {
    if (now - lastServoStep < SWEEP_STEP_MS) return;
    lastServoStep = now;

    servoAngle += sweepDir * SWEEP_STEP_DEG;
    if (servoAngle >= SWEEP_MAX_ANGLE || servoAngle <= SWEEP_MIN_ANGLE) {
      servoAngle = constrain(servoAngle, SWEEP_MIN_ANGLE, SWEEP_MAX_ANGLE);
      sweepDir = -sweepDir;
    }
    servoWrite(servoAngle);
  } else if (servoActive && now - laserOffSince > 500) {
    // Back at center - stop the pulses so the servo doesn't buzz/jitter while idle
    ledcWrite(SERVO_PIN, 0);
    servoActive = false;
  }
}

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  ledcAttach(SERVO_PIN, SERVO_FREQ_HZ, SERVO_RES_BITS);
  setLaser(false);

  if (BOOT_TEST_MS > 0) {
    setLaser(true);
    bootTest = true;
    bootTestStart = millis();
  }

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  if (gotMsg) {
    gotMsg = false;
    bootTest = false;  // a real message takes over from the test
    setLaser(lastType == MSG_MOTION_START);
  }

  if (bootTest && millis() - bootTestStart > BOOT_TEST_MS) {
    bootTest = false;
    setLaser(false);
  }

  if (laserOn && LASER_MAX_ON_MS > 0 && millis() - laserOnSince > LASER_MAX_ON_MS) {
    setLaser(false);
  }

  updateServo();
  delay(10);
}
