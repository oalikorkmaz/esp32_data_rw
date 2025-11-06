#ifndef CFG_IF_H
#define CFG_IF_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* -------------------------------------------------------
 * Cihaz Konfigürasyon Yapısı
 * ------------------------------------------------------- */
typedef struct {
    char device_id[32];           // Benzersiz cihaz kimliği (ör: ESP32-AABBCCDD)
    char server_host[64];         // Sunucu adresi (IP veya domain)
    int32_t server_port;          // Sunucu portu
    int32_t send_interval_sec;    // Veri gönderim aralığı (saniye)
    int32_t net_mode;             // Ağ modu (0=Auto, 1=Eth, 2=WiFi, 3=GSM)
    char fw_version[16];          // Firmware versiyonu
    uint32_t production_date;     // Üretim tarihi (UNIX timestamp)
} device_cfg_t;

/* -------------------------------------------------------
 * Fonksiyon Prototipleri
 * ------------------------------------------------------- */

/**
 * @brief Konfigürasyon sistemini başlatır
 * 
 * İlk çalıştırmada fabrika ayarları yüklenir ve MAC adresinden
 * benzersiz bir device_id üretilir. Sonraki açılışlarda NVS'den
 * kayıtlı ayarlar yüklenir.
 * 
 * @return true başarılı, false hata
 */
bool cfg_init(void);

/**
 * @brief Aktif konfigürasyonu döndürür
 * 
 * @return Konfigürasyon yapısının pointer'ı (değiştirilemez)
 */
const device_cfg_t *cfg_get(void);

/**
 * @brief Konfigürasyonu NVS'ye kaydeder
 * 
 * Kaydetmeden önce doğrulama yapılır. Geçersiz değerler reddedilir.
 * 
 * @param cfg Kaydedilecek konfigürasyon
 * @return true başarılı, false hata
 */
bool cfg_save(const device_cfg_t *cfg);

/**
 * @brief Fabrika sıfırlama yapar
 * 
 * Tüm ayarları siler ve fabrika varsayılanlarına döner.
 * 
 * @return true başarılı, false hata
 */
bool cfg_factory_reset(void);

/**
 * @brief Konfigürasyonu JSON formatında dışa aktarır
 * 
 * BLE veya HTTP üzerinden ayarları göstermek için kullanılabilir.
 * 
 * @param output JSON string'inin yazılacağı buffer
 * @param max_len Buffer boyutu
 */
void cfg_export_json(char *output, size_t max_len);

/**
 * @brief Device ID'yi değiştirir (Üretim sırasında)
 * 
 * @param new_id Yeni device ID
 * @return true başarılı, false geçersiz ID
 */
bool cfg_set_device_id(const char *new_id);

/**
 * @brief Üretim tarihini kaydeder
 * 
 * @param unix_timestamp UNIX timestamp (saniye)
 * @return true başarılı, false hata
 */
bool cfg_set_production_date(uint32_t unix_timestamp);

#endif // CFG_IF_H