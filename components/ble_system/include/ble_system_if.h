#ifndef BLE_SYSTEM_IF_H
#define BLE_SYSTEM_IF_H

#include "esp_err.h"

/**
 * @brief BLE sistemini başlatır (button + ctrl + cfg modüllerini bir araya getirir)
 */
esp_err_t ble_system_init(void);

#endif // BLE_SYSTEM_IF_H
