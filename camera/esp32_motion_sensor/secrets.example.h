// Copy this file to secrets.h (which git ignores) and replace both keys.
// Both boards - esp32_motion_sensor and esp32_laser_receiver - must use the SAME keys.
// Generate random ones with:  python3 -c "import os; print(', '.join('0x%02X' % b for b in os.urandom(16)))"
#pragma once
#include <esp_now.h>

// Primary master key: encrypts the local master key over the air
const uint8_t ESPNOW_PMK[ESP_NOW_KEY_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// Local master key: encrypts the messages between the two boards
const uint8_t ESPNOW_LMK[ESP_NOW_KEY_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
