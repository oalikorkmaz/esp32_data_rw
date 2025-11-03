#ifndef BLE_BUTTON_IF_H
#define BLE_BUTTON_IF_H

#include "esp_err.h"

/**
 * @brief BLE buton kontrol modülünü başlatır.
 * 
 * Bu görev arka planda GPIO'yu okur, 3 saniyelik basılı tutma algılar
 * ve BLE’yi açıp kapatır.
 */
esp_err_t ble_button_init(void);

#endif // BLE_BUTTON_IF_H
