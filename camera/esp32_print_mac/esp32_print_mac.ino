// Prints this ESP32's MAC address, so it can be used as RECEIVER_MAC in esp32_motion_sensor.
// Prints every 2 seconds so it's not missed when the Serial Monitor is opened late.

#include <esp_mac.h>

uint8_t staMac[6];
uint8_t apMac[6];

void setup() {
  Serial.begin(115200);
  esp_read_mac(staMac, ESP_MAC_WIFI_STA);
  esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP);
}

void loop() {
  Serial.println("----------------------------------------");
  Serial.printf("STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5]);
  Serial.printf("Paste into esp32_motion_sensor:\n");
  Serial.printf("uint8_t RECEIVER_MAC[6] = {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X};\n",
                staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5]);
  Serial.printf("(AP MAC, only if the receiver runs as an access point: %02X:%02X:%02X:%02X:%02X:%02X)\n",
                apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5]);
  delay(2000);
}
