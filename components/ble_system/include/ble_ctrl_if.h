#ifndef BLE_CTRL_IF_H
#define BLE_CTRL_IF_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief BLE kontrol modülü başlatma
 */
esp_err_t ble_ctrl_init(void);

/**
 * @brief BLE’yi başlatır (advertising dahil)
 */
esp_err_t ble_ctrl_start(void);

/**
 * @brief BLE’yi durdurur (advertising durdurulur, NimBLE devre dışı bırakılır)
 */
esp_err_t ble_ctrl_stop(void);

/**
 * @brief BLE aktif mi?
 */
bool ble_ctrl_is_enabled(void);

#endif // BLE_CTRL_IF_H
