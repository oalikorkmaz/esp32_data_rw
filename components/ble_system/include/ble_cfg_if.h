#ifndef BLE_CFG_IF_H
#define BLE_CFG_IF_H

#include "esp_err.h"

/**
 * @brief BLE GAP / GATT servislerini başlatır.
 */
esp_err_t ble_cfg_init(void);

/**
 * @brief BLE reklamını başlatır.
 */
esp_err_t ble_cfg_start(void);

#endif // BLE_CFG_IF_H
