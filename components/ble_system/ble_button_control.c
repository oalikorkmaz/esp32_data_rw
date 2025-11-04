#include "ble_button_if.h"
#include "ble_ctrl_if.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define BTN_GPIO GPIO_NUM_21
#define HOLD_MS 3000

static const char *TAG = "BLE_BTN";

static void button_task(void *pv)
{
    uint32_t press_start = 0;
    bool pressed = false;

    while (1) {
        int level = gpio_get_level(BTN_GPIO);
        if (level == 0 && !pressed) {
            pressed = true;
            press_start = xTaskGetTickCount();
        } else if (level == 1 && pressed) {
            pressed = false;
        }

        if (pressed && 
            (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS >= HOLD_MS)
        {
            pressed = false;

            if (ble_ctrl_is_enabled()) {
                ESP_LOGI(TAG, "Button: stopping BLE...");
                ble_ctrl_stop();
            } else {
                ESP_LOGI(TAG, "Button: starting BLE...");
                ble_ctrl_start();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t ble_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    xTaskCreate(button_task, "ble_btn_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Button task initialized.");
    return ESP_OK;
}
