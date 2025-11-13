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
    bool pressed = false;
    TickType_t press_start = 0;

    while (1) {

        int level = gpio_get_level(BTN_GPIO);   // 1 = serbest, 0 = basılı

        if (level == 0 && !pressed) {
            // buton yeni basıldı
            pressed = true;
            press_start = xTaskGetTickCount();
            ESP_LOGI(TAG, "Button DOWN");
        }

        else if (level == 1 && pressed) {
            // butondan çekildi
            pressed = false;
            ESP_LOGI(TAG, "Button UP");
        }

        // 3 saniye basılı kaldı mı?
        if (pressed &&
            (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS >= HOLD_MS)
        {
            pressed = false;   // sadece bir kez tetiklensin
            ESP_LOGI(TAG, "Button held for 3 seconds!");
            // ➤ BURAYA BLE Aç/Kapat koyabilirsin
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // debounce + CPU tasarruf
    }
}

esp_err_t ble_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,     // Harici 10k olduğu için kapalı
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Button task initialized.");
    return ESP_OK;
}
