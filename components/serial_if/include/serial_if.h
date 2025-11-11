#pragma once
#include <stdbool.h>
#include "freertos/queue.h"

/**
 * Seri okuma görevini başlatır ve tamamlanan satırları verilen kuyruğa yazar.
 * - Her satır NULL-terminated string olarak kuyruğa konur.
 * - Kuyruk oluştururken item size, göndereceğiniz maksimum satır uzunluğu + 1 olmalı.
 *
 * @param target_queue     xQueueCreate ile oluşturduğun kuyruk.
 * @param max_line_bytes   Kuyrukta her öğe için ayrılan maksimum byte (örn. 1024).
 * @return true            Başarılıysa.
 */
void serial_if_init(void);
void serial_if_start(void);
bool serial_start_and_bind_line_queue(QueueHandle_t target_queue, size_t max_line_bytes);
