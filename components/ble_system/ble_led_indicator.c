#include "ble_led_if.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "BLE_LED";

static led_strip_handle_t led_strip = NULL;
static int s_led_gpio = -1;

esp_err_t ble_led_init_gpio(int gpio_num)
{
    s_led_gpio = gpio_num;
    ESP_LOGI(TAG, "RGB LED başlatılıyor (GPIO%d)...", s_led_gpio);

    led_strip_config_t strip_config = {
        .strip_gpio_num = s_led_gpio,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED init başarısız! GPIO%d kontrol et.", s_led_gpio);
        led_strip = NULL;
        return err;
    }

    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "RGB LED GPIO%d ile başarıyla başlatıldı.", s_led_gpio);
    return ESP_OK;
}

void ble_led_set(bool on)
{
    if (!led_strip) {
        ESP_LOGW(TAG, "LED daha önce başlatılmadı, işlem iptal.");
        return;
    }

    if (on) {
        led_strip_set_pixel(led_strip, 0, 0, 0, 16);  // mavi
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
        led_strip_refresh(led_strip);
    }
}
