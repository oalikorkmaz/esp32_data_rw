#include "time_if.h"
#include "ds1302.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "TIME_IF";
static struct tm s_current_time;

static ds1302_t rtc = {
    .sclk_pin = 18,
    .io_pin = 19,
    .ce_pin = 21
};
static void rtc_update_task(void *arg)
{
    while (1) {
        if (ds1302_get_time(&rtc, &s_current_time) == ESP_OK) {
            ESP_LOGD(TAG, "RTC updated: %02d/%02d/%02d %02d:%02d:%02d",
                     s_current_time.tm_mday,
                     s_current_time.tm_mon + 1,
                     (s_current_time.tm_year + 1900) % 100,
                     s_current_time.tm_hour,
                     s_current_time.tm_min,
                     s_current_time.tm_sec);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 saniyede bir güncelle
    }
}

esp_err_t time_if_init(void)
{
    ESP_ERROR_CHECK(ds1302_init(&rtc));
    ds1302_get_time(&rtc, &s_current_time);

    xTaskCreate(rtc_update_task, "rtc_update_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "RTC Time Interface initialized");
    return ESP_OK;
}

struct tm time_if_get_current_time(void)
{
    return s_current_time;
}

void time_if_get_date(char *buffer, size_t len)
{
    //snprintf(buffer, len, "%02d/%02d/%04d",
    //         s_current_time.tm_mday,
    //         s_current_time.tm_mon + 1,
    //         s_current_time.tm_year + 1900);
    snprintf(buffer, len, "%02d/%02d/%04d",
             11,
             11,
            2025);

}

void time_if_get_time(char *buffer, size_t len)
{
    // snprintf(buffer, len, "%02d:%02d:%02d",
    //          s_current_time.tm_hour,
    //          s_current_time.tm_min,
    //          s_current_time.tm_sec);
    snprintf(buffer, len, "%02d:%02d:%02d",
             17,
             57,
             56);
}

void time_if_get_formatted_timestamp(char *buffer, size_t len)
{
    // snprintf(buffer, len, "%02d/%02d/%02d-%02d:%02d:%02d",
    //     s_current_time.tm_mday,
    //     s_current_time.tm_mon + 1,
    //     (s_current_time.tm_year + 1900) % 100,
    //     s_current_time.tm_hour,
    //     s_current_time.tm_min,
    //     s_current_time.tm_sec);

        snprintf(buffer, len, "%02d/%02d/%02d-%02d:%02d:%02d",
        11,
        11,
        25,
        17,
        57,
        56);
}
