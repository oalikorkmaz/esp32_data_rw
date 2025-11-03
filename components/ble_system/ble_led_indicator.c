#include "ble_led_if.h"
#include "esp_log.h"
#include "led_strip.h"

#define LED_GPIO 38

static const char *TAG = "BLE_LED";
static led_strip_handle_t led_strip = NULL;

// ------------------------------------------------------------
// LED başlatma
// ------------------------------------------------------------
esp_err_t ble_led_init(void)
{
    ESP_LOGI(TAG, "Configuring RGB LED on GPIO%d...", LED_GPIO);

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    if (led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip) != ESP_OK) {
        ESP_LOGE(TAG, "LED initialization failed!");
        return ESP_FAIL;
    }

    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "RGB LED initialized successfully.");
    return ESP_OK;
}

// ------------------------------------------------------------
// BLE aktif/pasif durumuna göre LED kontrolü
// ------------------------------------------------------------
void ble_led_set(bool on)
{
    if (!led_strip) return;

    if (on) {
        // BLE aktif → beyaz LED
        led_strip_set_pixel(led_strip, 0, 0, 0, 16);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "BLE aktif: LED beyaz yandı.");
    } else {
        // BLE pasif → LED kapalı
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
        ESP_LOGI(TAG, "BLE pasif: LED kapandı.");
    }
}
