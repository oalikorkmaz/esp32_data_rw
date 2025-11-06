#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "serial_if.h"
#include "data_parser.h"
#include "data_sender.h"

#define UART_PORT       UART_NUM_1
#define UART_TX_PIN     17
#define UART_RX_PIN     16
#define UART_BAUD_RATE  9600
#define BUF_SIZE        2048

static const char *TAG = "SERIAL_IF";

/* -------------------------------------------------------
 * UART Başlatma
 * ------------------------------------------------------- */
void serial_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "UART başlatıldı (TX=%d, RX=%d, %d baud)", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
}

/* -------------------------------------------------------
 * Ana Görev: RS232 okuma + parsing + yönlendirme
 * ------------------------------------------------------- */
static void serial_task(void *arg)
{
    uint8_t rx_buffer[BUF_SIZE];
    char line_buf[512];
    int line_pos = 0;

    ESP_LOGI(TAG, "Serial task başlatıldı.");

    while (1) {
        int len = uart_read_bytes(UART_PORT, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(200));

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)rx_buffer[i];

                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = '\0'; // satırı sonlandır
                        ESP_LOGD(TAG, "RS232 Satır: %s", line_buf);

                        // Parse işlemi
                        hd32mt_data_t record;
                        if (parse_hd32mt_record(line_buf, &record)) {
                            ESP_LOGI(TAG, "Parse OK: %s (%d sensör)",
                                     record.timestamp, record.sensor_count);

                            // Her sensör için data_sender’e gönderim
                            for (int s = 0; s < record.sensor_count; s++) {
                                const char *label = record.labels[s];
                                const char *unit = record.units[s] ? record.units[s] : "";
                                const char *timestamp = record.timestamp;
                                data_sender_send(record.labels[s], record.sensors[s], unit, timestamp);
                            }
                        }
                        line_pos = 0;
                    }
                } else if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = c;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* -------------------------------------------------------
 * Görev Oluşturucu
 * ------------------------------------------------------- */
void serial_start(void)
{
    xTaskCreate(serial_task, "serial_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Serial task oluşturuldu.");
}
