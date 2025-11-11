#include "telemetry_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "serial_if.h"
#include "data_parser.h"
#include "data_sender.h"
#include "time_if.h"

#define TELEMETRY_LINE_MAX_BYTES     1024
#define TELEMETRY_QUEUE_LENGTH       16
#define TELEMETRY_TASK_STACK_BYTES   4096
#define TELEMETRY_TASK_PRIORITY      5

static const char *TAG = "TELEMETRY";
static QueueHandle_t g_line_queue = NULL;
static int g_total_channel_count = 10;

/* ----------------------------- TELEMETRY PIPELINE ----------------------------- */

static void telemetry_task(void *param)
{
    (void)param;
    char received_line[TELEMETRY_LINE_MAX_BYTES];

    for (;;) {
        if (xQueueReceive(g_line_queue, &received_line, portMAX_DELAY) == pdTRUE) {

            // 1️⃣ Satırı ayrıştır
            hd32mt_data_t record = {0};
            if (!parse_hd32mt_record(received_line, &record)) {
                ESP_LOGW(TAG, "Geçersiz satır: %s", received_line);
                continue;
            }

            // 2️⃣ Gönderim (internet varsa gönderir, yoksa SD'ye kaydeder)
            bool ok = data_sender_send_frame_from_record(&record,
                                                         g_total_channel_count,
                                                         NULL);
            ESP_LOGI(TAG, "Frame işlendi: %s", ok ? "OK" : "FAIL");
        }
    }
}

/* ----------------------------- SERVİS BAŞLATMA ----------------------------- */

bool telemetry_service_start(int total_channel_count)
{
    if (total_channel_count <= 0)
        total_channel_count = 10;

    g_total_channel_count = total_channel_count;

    if (!g_line_queue) {
        g_line_queue = xQueueCreate(TELEMETRY_QUEUE_LENGTH, TELEMETRY_LINE_MAX_BYTES);
        if (!g_line_queue) {
            ESP_LOGE(TAG, "Kuyruk oluşturulamadı");
            return false;
        }
    }

    if (!serial_start_and_bind_line_queue(g_line_queue, TELEMETRY_LINE_MAX_BYTES)) {
        ESP_LOGE(TAG, "Serial başlatılamadı");
        return false;
    }

    BaseType_t ok = xTaskCreate(telemetry_task,
                                "telemetry_task",
                                TELEMETRY_TASK_STACK_BYTES,
                                NULL,
                                TELEMETRY_TASK_PRIORITY,
                                NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "telemetry_task oluşturulamadı");
        return false;
    }

    ESP_LOGI(TAG, "Telemetri servisi başlatıldı (kanal sayısı=%d)", g_total_channel_count);
    return true;
}
