#include "ble_system_if.h"
#include "ble_ctrl_if.h"
#include "ble_cfg_if.h"
#include "ble_button_if.h"
#include "esp_log.h"

static const char *TAG = "BLE_SYSTEM";

esp_err_t ble_system_init(void)
{
    ESP_LOGI(TAG, "BLE System initializing...");

    ESP_ERROR_CHECK(ble_ctrl_init());
    ESP_ERROR_CHECK(ble_cfg_init());
    ESP_ERROR_CHECK(ble_button_init());

    ESP_LOGI(TAG, "BLE System initialized successfully.");
    return ESP_OK;
}
