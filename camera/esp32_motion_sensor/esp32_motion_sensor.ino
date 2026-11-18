// Battery PIR motion sensor (ESP32-S3)
// Sleeps in deep sleep, wakes up when the PIR output goes HIGH and again when it goes LOW,
// reports "motion start" / "motion end" over ESP-NOW and goes back to sleep.
//
// Wiring (HC-SR501):  VCC -> 5V   GND -> GND   OUT -> GPIO1
// Wiring (AM312):     VCC -> 3V3  GND -> GND   OUT -> GPIO1

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <driver/rtc_io.h>

// ESPNOW_PMK / ESPNOW_LMK - the shared encryption keys. Not in git: copy
// secrets.example.h to secrets.h and use the same keys as the laser receiver.
#include "secrets.h"

// ---------------- Settings ----------------
#define PIR_PIN GPIO_NUM_1  // PIR OUT wire. Must be an RTC GPIO (0-21) to wake from deep sleep

// MAC of the receiving ESP32 (laser)
uint8_t RECEIVER_MAC[6] = {0xD0, 0xCF, 0x13, 0x35, 0xF0, 0xE4};

// Must match the receiver's Wi-Fi channel (if the receiver is connected to a router - the router's channel)
const uint8_t ESPNOW_CHANNEL = 1;
const int SEND_RETRIES = 3;

// Message sent to the receiver - the receiver must define the same struct
enum : uint8_t { MSG_MOTION_END = 0, MSG_MOTION_START = 1 };

typedef struct __attribute__((packed)) {
  uint8_t type;  // MSG_MOTION_START / MSG_MOTION_END
  uint32_t motionCount;
  uint32_t bootCount;
} MotionMsg;

// ---------------- Kept across deep sleep ----------------
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t motionCount = 0;
RTC_DATA_ATTR bool waitingForMotionEnd = false;

volatile bool sendDone = false;
volatile bool sendOk = false;

// The send callback signature changed in Arduino-ESP32 core 3.3.0
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  sendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendDone = true;
}

void sendEspNow(uint8_t type) {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(onDataSent);
  if (esp_now_set_pmk(ESPNOW_PMK) != ESP_OK) return;

  // Encrypted: the receiver only accepts messages sealed with the shared key
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, RECEIVER_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = true;
  memcpy(peer.lmk, ESPNOW_LMK, ESP_NOW_KEY_LEN);
  if (esp_now_add_peer(&peer) != ESP_OK) return;

  MotionMsg msg = {type, motionCount, bootCount};
  for (int attempt = 1; attempt <= SEND_RETRIES; attempt++) {
    sendDone = false;
    if (esp_now_send(RECEIVER_MAC, (uint8_t *)&msg, sizeof(msg)) == ESP_OK) {
      unsigned long t0 = millis();
      while (!sendDone && millis() - t0 < 100) delay(1);
      if (sendDone && sendOk) return;
    }
    delay(20);
  }
}

void goToSleep() {
  // The PIR keeps OUT HIGH for a few seconds after motion. If it is still HIGH,
  // sleep until it goes LOW - otherwise we would wake up again immediately.
  waitingForMotionEnd = (digitalRead(PIR_PIN) == HIGH);
  int wakeLevel = waitingForMotionEnd ? 0 : 1;

  esp_sleep_enable_ext0_wakeup(PIR_PIN, wakeLevel);
  rtc_gpio_pullup_dis(PIR_PIN);
  rtc_gpio_pulldown_en(PIR_PIN);  // disconnected wire = "no motion"

  esp_deep_sleep_start();
}

void setup() {
  bootCount++;

  rtc_gpio_deinit(PIR_PIN);  // back to a normal GPIO after the ext0 wake-up
  pinMode(PIR_PIN, INPUT_PULLDOWN);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    if (waitingForMotionEnd) {
      sendEspNow(MSG_MOTION_END);
    } else {
      motionCount++;
      sendEspNow(MSG_MOTION_START);
    }
  }

  goToSleep();
}

void loop() {
  // Never reached - setup() always ends in deep sleep
}
