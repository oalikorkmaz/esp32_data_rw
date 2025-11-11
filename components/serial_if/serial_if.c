// main/serial_if.c

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "serial_if.h"

/* ----------------------------- Kullanıcıya açık ayarlar ----------------------------- */
/* Gerekirse bu pin/baud değerlerini projene göre değiştir. */
#define SERIAL_UART_PORT_NUMBER            (UART_NUM_1)
#define SERIAL_UART_TX_GPIO_NUMBER         (17)
#define SERIAL_UART_RX_GPIO_NUMBER         (16)
#define SERIAL_UART_BAUD_RATE              (1150200)

/* UART sürücüsünün dahili RX buffer'ı (bayt) */
#define SERIAL_UART_DRIVER_RX_BUFFER_BYTES (2048)

/* Okuma döngüsü zaman aşımı (ms) */
#define SERIAL_UART_READ_TIMEOUT_MS        (100)

/* Satır sonlandırıcı kabul edilen karakterler */
#define SERIAL_ACCEPT_CR                   (1)   /* '\r' */
#define SERIAL_ACCEPT_LF                   (1)   /* '\n' */
#define SERIAL_ACCEPT_AMPERSAND            (1)   /* '&' */

/* Task ayarları */
#define SERIAL_RECEIVER_TASK_NAME          "serial_receiver_task"
#define SERIAL_RECEIVER_TASK_STACK_BYTES   (4096)
#define SERIAL_RECEIVER_TASK_PRIORITY      (5)

/* ------------------------------------------------------------------------------------ */

static const char *LOG_TAG_SERIAL_IF = "SERIAL_IF";

/* Bu modülün iç durumu */
static QueueHandle_t target_line_queue_handle = NULL;   /* Dışarıdan bağlanan kuyruk */
static size_t        max_line_bytes_for_queue = 0;      /* Kuyruktaki öğe boyutu */
static TaskHandle_t  serial_receiver_task_handle = NULL;
static bool          uart_initialized = false;

/* Satır biriktirme tamponu (tek görev tarafından kullanılır) */
static char   line_accumulator_buffer[1024];
static size_t line_accumulator_length = 0;

/* ------------------------------- Yardımcı Fonksiyonlar ------------------------------- */

static inline bool is_line_terminator_character(char ch)
{
#if SERIAL_ACCEPT_CR
    if (ch == '\r') return true;
#endif
#if SERIAL_ACCEPT_LF
    if (ch == '\n') return true;
#endif
#if SERIAL_ACCEPT_AMPERSAND
    if (ch == '&')  return true;
#endif
    return false;
}

static void trim_line_in_place(char *line)
{
    if (!line) return;

    /* Baştaki boşlukları kırp */
    while (*line && isspace((unsigned char)*line)) {
        memmove(line, line + 1, strlen(line));  /* sola kaydır */
    }

    /* Sondaki boşlukları kırp */
    size_t length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
}

/* ----------------------------------- UART Kurulumu ---------------------------------- */

static esp_err_t initialize_uart_once(void)
{
    if (uart_initialized) {
        return ESP_OK;
    }

    const uart_config_t uart_configuration = {
        .baud_rate  = SERIAL_UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(SERIAL_UART_PORT_NUMBER, &uart_configuration));
    ESP_ERROR_CHECK(uart_set_pin(SERIAL_UART_PORT_NUMBER,
                                 SERIAL_UART_TX_GPIO_NUMBER,
                                 SERIAL_UART_RX_GPIO_NUMBER,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    /* Sadece RX buffer kullanıyoruz (TX için driver buffer ayırmıyoruz) */
    ESP_ERROR_CHECK(uart_driver_install(SERIAL_UART_PORT_NUMBER,
                                        SERIAL_UART_DRIVER_RX_BUFFER_BYTES,
                                        0, 0, NULL, 0));

    uart_initialized = true;

    ESP_LOGI(LOG_TAG_SERIAL_IF,
             "UART hazir: UART%ld TX=%d RX=%d Baud=%d",
             (long)SERIAL_UART_PORT_NUMBER,
             SERIAL_UART_TX_GPIO_NUMBER,
             SERIAL_UART_RX_GPIO_NUMBER,
             SERIAL_UART_BAUD_RATE);

    return ESP_OK;
}

/* ----------------------------------- Alıcı Görev ------------------------------------ */

static void serial_receiver_task(void *task_parameters)
{
    (void)task_parameters;

    /* Okuma buffer'ı (tek seferde UART'tan çekilen ham baytlar) */
    uint8_t uart_read_buffer[SERIAL_UART_DRIVER_RX_BUFFER_BYTES];

    /* Satır biriktirme başlangıcı */
    line_accumulator_length = 0;
    line_accumulator_buffer[0] = '\0';

    const TickType_t read_timeout_ticks = pdMS_TO_TICKS(SERIAL_UART_READ_TIMEOUT_MS);

    ESP_LOGI(LOG_TAG_SERIAL_IF, "Serial receiver task basladi.");

    for (;;) {
        /* UART'tan baytları oku */
        int bytes_read = uart_read_bytes(SERIAL_UART_PORT_NUMBER,
                                         uart_read_buffer,
                                         sizeof(uart_read_buffer),
                                         read_timeout_ticks);

        if (bytes_read <= 0) {
            /* Veri yok → küçük bekleme */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Gelen baytları satırlara böl */
        for (int i = 0; i < bytes_read; ++i) {
            char incoming_character = (char)uart_read_buffer[i];

            /* Satır sonuna kadar biriktir */
            if (!is_line_terminator_character(incoming_character)) {

                /* Biriktirme tamponu taşmasın */
                if (line_accumulator_length < (sizeof(line_accumulator_buffer) - 1)) {
                    line_accumulator_buffer[line_accumulator_length++] = incoming_character;
                    line_accumulator_buffer[line_accumulator_length]   = '\0';
                } else {
                    /* Satır aşırı uzadı → sıfırla ve uyar */
                    ESP_LOGW(LOG_TAG_SERIAL_IF,
                             "Satir uzunluk siniri asildi (%u). Satir temizleniyor.",
                             (unsigned)sizeof(line_accumulator_buffer));
                    line_accumulator_length = 0;
                    line_accumulator_buffer[0] = '\0';
                }
                continue;
            }

            /* Satır sonlandırıcı gördük → boş satırı geç */
            if (line_accumulator_length == 0) {
                continue;
            }

            /* Satırı temizle (baş/son boşluk) */
            trim_line_in_place(line_accumulator_buffer);

            /* Null sonlandirildigindan emin ol */
            line_accumulator_buffer[sizeof(line_accumulator_buffer) - 1] = '\0';

            /* Satır boyutu, kuyruk öğe boyutunu aşmamalı */
            size_t line_length_bytes = strlen(line_accumulator_buffer) + 1; /* '\0' dahil */
            if (line_length_bytes > max_line_bytes_for_queue) {
                ESP_LOGW(LOG_TAG_SERIAL_IF,
                         "Satir, kuyruk oge boyutunu asiyor (satir=%u, limit=%u). Satir atlandi.",
                         (unsigned)line_length_bytes,
                         (unsigned)max_line_bytes_for_queue);
            } else {
                /* Kuyruğa gönder (hedef kuyruğa bir kopya olarak) */
                BaseType_t enqueue_result = xQueueSend(
                    target_line_queue_handle,
                    (void *)line_accumulator_buffer,
                    0 /* bekleme yok; doluysa düşür */);

                if (enqueue_result != pdTRUE) {
                    ESP_LOGW(LOG_TAG_SERIAL_IF,
                             "Kuyruk dolu, satir dusuruldu: '%s'",
                             line_accumulator_buffer);
                }
            }

            /* Yeni satır için biriktirmeyi sıfırla */
            line_accumulator_length = 0;
            line_accumulator_buffer[0] = '\0';
        } /* for (bytes_read) */
    } /* for (;;) */
}

/* ------------------------------ Dışarıya Açık API ------------------------------ */

bool serial_start_and_bind_line_queue(QueueHandle_t target_queue, size_t max_line_bytes)
{
    if (target_queue == NULL || max_line_bytes == 0) {
        ESP_LOGE(LOG_TAG_SERIAL_IF, "Gecersiz parametre: queue veya max_line_bytes bos");
        return false;
    }

    /* UART’ı bir kez hazırla */
    if (initialize_uart_once() != ESP_OK) {
        ESP_LOGE(LOG_TAG_SERIAL_IF, "UART baslatilamadi");
        return false;
    }

    /* Hedef kuyruğu ve öğe boyutunu kaydet */
    target_line_queue_handle  = target_queue;
    max_line_bytes_for_queue  = max_line_bytes;

    /* Görev zaten varsa yeniden oluşturma */
    if (serial_receiver_task_handle != NULL) {
        vTaskDelete(serial_receiver_task_handle);
        serial_receiver_task_handle = NULL;
    }

    /* Alıcı görevini oluştur */
    BaseType_t task_ok = xTaskCreate(
        serial_receiver_task,
        SERIAL_RECEIVER_TASK_NAME,
        SERIAL_RECEIVER_TASK_STACK_BYTES,
        NULL,
        SERIAL_RECEIVER_TASK_PRIORITY,
        &serial_receiver_task_handle);

    if (task_ok != pdPASS) {
        ESP_LOGE(LOG_TAG_SERIAL_IF, "Serial receiver task olusmadi");
        return false;
    }

    ESP_LOGI(LOG_TAG_SERIAL_IF,
             "Serial baglandi: queue=%p, item_size=%u",
             (void *)target_line_queue_handle,
             (unsigned)max_line_bytes_for_queue);

    return true;
}
