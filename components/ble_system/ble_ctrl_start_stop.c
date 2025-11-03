#include "ble_ctrl_if.h"
#include "ble_cfg_if.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "BLE_CTRL";
static bool ble_enabled = false;

esp_err_t ble_ctrl_init(void)
{
    ble_enabled = false;
    return ESP_OK;
}

esp_err_t ble_ctrl_start(void)
{
    if (ble_enabled) {
        ESP_LOGW(TAG, "BLE already running.");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(nimble_port_init());
    ble_enabled = true;
    ESP_LOGI(TAG, "BLE started.");
    return ble_cfg_start();
}

esp_err_t ble_ctrl_stop(void)
{
    if (!ble_enabled) {
        ESP_LOGW(TAG, "BLE already stopped.");
        return ESP_OK;
    }

    ble_gap_adv_stop();
    nimble_port_stop();
    nimble_port_deinit();

    ble_enabled = false;
    ESP_LOGI(TAG, "BLE stopped.");
    return ESP_OK;
}

bool ble_ctrl_is_enabled(void)
{
    return ble_enabled;
}
