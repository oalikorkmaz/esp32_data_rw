#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "data_parser.h"

/**
 * Çoklu sensörü tek satır halinde gönderir:
 * $<device_id>$<yy/mm/dd-HH:MM:SS>$<total_channels>$<ch1>$...$<chN>\r\n
 *
 * @param record                Parser’dan gelen veri (pozisyonel diziler)
 * @param total_channels        Toplam kanal sayısı (N)
 * @param formatted_timestamp   "yy/mm/dd-HH:MM:SS" (RTC’den hazır biçim)
 * @return true                 Satır başarıyla gönderildiyse
 */
bool data_sender_send_frame_from_record(const hd32mt_data_t *record,
                                        int total_channels,
                                        const char *formatted_timestamp);
/** Test/manuel kullanım: cfg_if.device_id yerine bunu kullan. NULL ya da "" verirsen override kapanır. */
void data_sender_set_device_id_override(const char *device_id_override);