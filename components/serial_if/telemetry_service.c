#include "telemetry_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "serial_if.h"       // Sadece ham satırı verir (kuyruğa)
#include "data_parser.h"     // Ham satırı anlamlı kayda çevirir
#include "data_sender.h"     // Kayıttan tek çerçeve oluşturup gönderir
#include "time_if.h"         // RTC/sistem saatini formatlamak için (hazır string)

// ---- Ayarlar ----
#define TELEMETRY_LINE_MAX_BYTES     1024
#define TELEMETRY_QUEUE_LENGTH       16
#define TELEMETRY_TASK_STACK_BYTES   4096
#define TELEMETRY_TASK_PRIORITY      5

static const char *LOG_TAG_TELEMETRY = "TELEMETRY";

// Bu servisin içindeki kuyruk (serial_if buraya satır basar)
static QueueHandle_t g_line_queue = NULL;

// Kaç kanal gönderileceği (pozisyonel)
static int g_total_channel_count = 10;


bool telemetry_send_test_frame(int total_channels,
                               const char *formatted_timestamp,
                               const float *channel_values,
                               int value_count)
{
    if (total_channels <= 0 || !formatted_timestamp || !formatted_timestamp[0]) {
        ESP_LOGE(LOG_TAG_TELEMETRY, "telemetry_send_test_frame: invalid args");
        return false;
    }

    hd32mt_data_t record = {0};
    // Pozisyonel: sensors[0] -> ch1, ...
    int copy_count = value_count;
    if (copy_count > total_channels) copy_count = total_channels;
    if (copy_count > (int)(sizeof(record.sensors)/sizeof(record.sensors[0])))
        copy_count = (int)(sizeof(record.sensors)/sizeof(record.sensors[0]));

    for (int i = 0; i < copy_count; ++i) {
        record.sensors[i] = channel_values ? channel_values[i] : 0.0f;
    }
    record.sensor_count = copy_count;

    bool ok = data_sender_send_frame_from_record(&record,
                                                 total_channels,
                                                 formatted_timestamp);
    if (!ok) {
        ESP_LOGW(LOG_TAG_TELEMETRY, "Test frame gonderimi basarisiz");
    }
    return ok;
}

// Zaman biçimi: "yy/mm/dd-HH:MM:SS" üret (RTC’den)
static void build_formatted_timestamp(char *out, size_t capacity)
{
    // RTC/saat katmanınızda böyle bir yardımcı varsa onu çağırın.
    // Yoksa örnek bir doldurma:
    // time_if_get_formatted(out, capacity); // önerilen
    time_t now = time(NULL);
    struct tm tm_now;
    if (now > 0) {
        localtime_r(&now, &tm_now);
        snprintf(out, capacity, "%02d/%02d/%02d-%02d:%02d:%02d",
                 tm_now.tm_year % 100,
                 tm_now.tm_mon + 1,
                 tm_now.tm_mday,
                 tm_now.tm_hour,
                 tm_now.tm_min,
                 tm_now.tm_sec);
    } else {
        snprintf(out, capacity, "11/10/05-00:00:00");
    }
}

// Kuyruktan satır al → parse → gönder
static void telemetry_pipeline_task(void *task_parameter)
{
    (void)task_parameter;

    char received_line[TELEMETRY_LINE_MAX_BYTES];

    for (;;) {
        // serial_if bu kuyruğa null-terminated satırlar koyacak
        if (xQueueReceive(g_line_queue, &received_line, portMAX_DELAY) == pdTRUE) {

            // 1) Ham satırı ayrıştır
            hd32mt_data_t parsed_record = {0};
            bool parse_ok = parse_hd32mt_record(received_line, &parsed_record);
            if (!parse_ok) {
                ESP_LOGW(LOG_TAG_TELEMETRY, "Satir parse edilemedi: %s", received_line);
                continue;
            }

            // 2) Zaman damgasını hazır biçimde üret (RTC’den)
            char formatted_timestamp[24];
            build_formatted_timestamp(formatted_timestamp, sizeof(formatted_timestamp));

            // 3) Gönder (tek TCP bağlantı, $SEND$ gelince kapat)
            bool send_ok = data_sender_send_frame_from_record(&parsed_record,
                                                              g_total_channel_count,
                                                              formatted_timestamp);
            if (!send_ok) {
                ESP_LOGW(LOG_TAG_TELEMETRY, "Gonderim basarisiz; baglanti yoksa yakinda SD’ye kuyruk eklenecek.");
                // TODO: Burada offline ise SD’ye append edilecek (bir sonraki aşama)
            } else {
                // TODO: Online olsa da SD’ye "arşiv" yazımı burada yapılacak (silmeden)
            }
        }
    }
}

bool telemetry_service_start(int total_channel_count)
{
    if (total_channel_count <= 0) return false;
    g_total_channel_count = total_channel_count;

    // 1) Kuyruğu oluştur
    if (!g_line_queue) {
        g_line_queue = xQueueCreate(TELEMETRY_QUEUE_LENGTH, TELEMETRY_LINE_MAX_BYTES);
        if (!g_line_queue) {
            ESP_LOGE(LOG_TAG_TELEMETRY, "Kuyruk olusturulamadi");
            return false;
        }
    }

    // 2) Serial’i bu kuyruğa yazacak şekilde başlat
    // Not: serial_if içinde "serial_start_and_bind_line_queue(QueueHandle_t)" gibi bir API varmış gibi
    // kullanıyoruz. Yoksa küçük bir ekleme yapacağız (aşağıdaki notlara bakınız).
    if (!serial_start_and_bind_line_queue(g_line_queue, TELEMETRY_LINE_MAX_BYTES)) {
        ESP_LOGE(LOG_TAG_TELEMETRY, "Serial baslatilamadi");
        return false;
    }

    // 3) İşleme görevini başlat
    BaseType_t ok = xTaskCreate(telemetry_pipeline_task,
                                "telemetry_pipeline",
                                TELEMETRY_TASK_STACK_BYTES,
                                NULL,
                                TELEMETRY_TASK_PRIORITY,
                                NULL);
    if (ok != pdPASS) {
        ESP_LOGE(LOG_TAG_TELEMETRY, "telemetry_pipeline gorevi olusmadi");
        return false;
    }

    ESP_LOGI(LOG_TAG_TELEMETRY, "Telemetri servisi basladi (kanal=%d)", g_total_channel_count);
    return true;
}
