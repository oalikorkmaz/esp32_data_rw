#include "cfg_if.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CFG_IF";
static device_cfg_t s_cfg;  // Bellekte tutulan aktif konfigürasyon
static bool s_initialized = false;

/* -------------------------------------------------------
 * MAC Adresi Tabanlı Benzersiz Device ID Üretici
 * ------------------------------------------------------- */
static void generate_device_id(char *device_id, size_t max_len)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Format: ESP32-AABBCCDD (son 4 byte MAC adresi)
    snprintf(device_id, max_len, "ESP32-%02X%02X%02X%02X",
             mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI(TAG, "Üretilen Device ID: %s", device_id);
}

/* -------------------------------------------------------
 * Fabrika Varsayılanları
 * ------------------------------------------------------- */
static void load_factory_defaults(void)
{
    // Benzersiz ID üret
    generate_device_id(s_cfg.device_id, sizeof(s_cfg.device_id));
    
    // Sunucu ayarları
    strncpy(s_cfg.server_host, "ats.com.tc", sizeof(s_cfg.server_host) - 1);
    s_cfg.server_port = 4545;
    
    // Veri gönderim aralığı
    s_cfg.send_interval_sec = 60;
    
    // Ağ modu (0=Auto, 1=Ethernet, 2=WiFi, 3=GSM)
    s_cfg.net_mode = 0;
    
    // Firmware versiyonu
    strncpy(s_cfg.fw_version, "1.0.0", sizeof(s_cfg.fw_version) - 1);
    
    // Üretim tarihi (UNIX timestamp olarak saklanabilir)
    s_cfg.production_date = 0;
    
    ESP_LOGI(TAG, "Fabrika varsayılanları yüklendi.");
}

/* -------------------------------------------------------
 * Konfigürasyon Doğrulama
 * ------------------------------------------------------- */
static bool validate_config(const device_cfg_t *cfg)
{
    // Device ID kontrolü
    if (strlen(cfg->device_id) < 5 || strlen(cfg->device_id) > 31) {
        ESP_LOGE(TAG, "Geçersiz device_id uzunluğu!");
        return false;
    }
    
    // Server host kontrolü
    if (strlen(cfg->server_host) < 3 || strlen(cfg->server_host) > 63) {
        ESP_LOGE(TAG, "Geçersiz server_host!");
        return false;
    }
    
    // Port kontrolü
    if (cfg->server_port < 1 || cfg->server_port > 65535) {
        ESP_LOGE(TAG, "Geçersiz port numarası: %d", cfg->server_port);
        return false;
    }
    
    // Interval kontrolü
    if (cfg->send_interval_sec < 1 || cfg->send_interval_sec > 3600) {
        ESP_LOGE(TAG, "Geçersiz interval: %d", cfg->send_interval_sec);
        return false;
    }
    
    // Net mode kontrolü
    if (cfg->net_mode > 3) {
        ESP_LOGE(TAG, "Geçersiz net_mode: %d", cfg->net_mode);
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------
 * NVS'den Konfigürasyon Yükleme
 * ------------------------------------------------------- */
static bool load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cfg", NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS'de kayıtlı konfigürasyon yok (ilk açılış).");
        return false;
    }
    
    // String değerleri oku
    size_t len;
    
    len = sizeof(s_cfg.device_id);
    nvs_get_str(handle, "device_id", s_cfg.device_id, &len);
    
    len = sizeof(s_cfg.server_host);
    nvs_get_str(handle, "server_host", s_cfg.server_host, &len);
    
    len = sizeof(s_cfg.fw_version);
    nvs_get_str(handle, "fw_version", s_cfg.fw_version, &len);
    
    // Integer değerleri oku
    nvs_get_i32(handle, "server_port", &s_cfg.server_port);
    nvs_get_i32(handle, "send_interval", &s_cfg.send_interval_sec);
    nvs_get_i32(handle, "net_mode", &s_cfg.net_mode);
    nvs_get_u32(handle, "prod_date", &s_cfg.production_date);
    
    nvs_close(handle);
    
    // Yüklenen config'i doğrula
    if (!validate_config(&s_cfg)) {
        ESP_LOGE(TAG, "NVS'deki konfigürasyon bozuk, varsayılanlara dönülüyor!");
        return false;
    }
    
    ESP_LOGI(TAG, "✓ Konfigürasyon NVS'den yüklendi:");
    ESP_LOGI(TAG, "  Device ID    : %s", s_cfg.device_id);
    ESP_LOGI(TAG, "  Server       : %s:%d", s_cfg.server_host, s_cfg.server_port);
    ESP_LOGI(TAG, "  Interval     : %d saniye", s_cfg.send_interval_sec);
    ESP_LOGI(TAG, "  Net Mode     : %d", s_cfg.net_mode);
    ESP_LOGI(TAG, "  FW Version   : %s", s_cfg.fw_version);
    
    return true;
}

/* -------------------------------------------------------
 * Konfigürasyon Başlatma (İlk Çalıştırma)
 * ------------------------------------------------------- */
bool cfg_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "cfg_init() zaten çağrıldı!");
        return true;
    }
    
    ESP_LOGI(TAG, "Konfigürasyon sistemi başlatılıyor...");
    
    // NVS başlat
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS sıfırlanıyor...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    // Fabrika varsayılanlarını yükle
    load_factory_defaults();
    
    // NVS'den kayıtlı değerleri oku (varsa)
    bool loaded = load_from_nvs();
    
    // İlk açılışsa, fabrika ayarlarını kaydet
    if (!loaded) {
        ESP_LOGW(TAG, "İlk açılış tespit edildi, fabrika ayarları kaydediliyor...");
        cfg_save(&s_cfg);
    }
    
    s_initialized = true;
    return true;
}

/* -------------------------------------------------------
 * Aktif Konfigürasyonu Getir
 * ------------------------------------------------------- */
const device_cfg_t *cfg_get(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "cfg_init() çağrılmamış!");
        return NULL;
    }
    return &s_cfg;
}

/* -------------------------------------------------------
 * Konfigürasyonu Kaydet
 * ------------------------------------------------------- */
bool cfg_save(const device_cfg_t *cfg)
{
    if (!cfg) {
        ESP_LOGE(TAG, "cfg NULL!");
        return false;
    }
    
    // Doğrulama
    if (!validate_config(cfg)) {
        ESP_LOGE(TAG, "Geçersiz konfigürasyon, kaydetme iptal edildi!");
        return false;
    }
    
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("cfg", NVS_READWRITE, &handle));
    
    // String değerleri kaydet
    nvs_set_str(handle, "device_id", cfg->device_id);
    nvs_set_str(handle, "server_host", cfg->server_host);
    nvs_set_str(handle, "fw_version", cfg->fw_version);
    
    // Integer değerleri kaydet
    nvs_set_i32(handle, "server_port", cfg->server_port);
    nvs_set_i32(handle, "send_interval", cfg->send_interval_sec);
    nvs_set_i32(handle, "net_mode", cfg->net_mode);
    nvs_set_u32(handle, "prod_date", cfg->production_date);
    
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    
    // RAM'deki kopyayı güncelle
    memcpy(&s_cfg, cfg, sizeof(device_cfg_t));
    
    ESP_LOGI(TAG, "✓ Yeni konfigürasyon kaydedildi!");
    return true;
}

/* -------------------------------------------------------
 * Fabrika Sıfırlama
 * ------------------------------------------------------- */
bool cfg_factory_reset(void)
{
    ESP_LOGW(TAG, "Fabrika sıfırlama başlatıldı!");
    
    // NVS'yi temizle
    nvs_handle_t handle;
    if (nvs_open("cfg", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    
    // WiFi ayarlarını da sil
    if (nvs_open("wifi_cfg", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    
    // Fabrika ayarlarını yükle
    load_factory_defaults();
    cfg_save(&s_cfg);
    
    ESP_LOGI(TAG, "✓ Fabrika sıfırlama tamamlandı!");
    return true;
}

/* -------------------------------------------------------
 * Konfigürasyon Dışa Aktar (JSON formatında)
 * ------------------------------------------------------- */
void cfg_export_json(char *output, size_t max_len)
{
    snprintf(output, max_len,
        "{\n"
        "  \"device_id\": \"%s\",\n"
        "  \"server_host\": \"%s\",\n"
        "  \"server_port\": %ld,\n"
        "  \"send_interval_sec\": %ld,\n"
        "  \"net_mode\": %ld,\n"
        "  \"fw_version\": \"%s\",\n"
        "  \"production_date\": %u\n"
        "}",
        s_cfg.device_id,
        s_cfg.server_host,
        (long)s_cfg.server_port,
        (long)s_cfg.send_interval_sec,
        (long)s_cfg.net_mode,
        s_cfg.fw_version,
        (unsigned int)s_cfg.production_date
    );
}

/* -------------------------------------------------------
 * Device ID Değiştirme (Üretim Sırasında Kullanılabilir)
 * ------------------------------------------------------- */
bool cfg_set_device_id(const char *new_id)
{
    if (!new_id || strlen(new_id) < 5 || strlen(new_id) > 31) {
        ESP_LOGE(TAG, "Geçersiz device_id!");
        return false;
    }
    
    strncpy(s_cfg.device_id, new_id, sizeof(s_cfg.device_id) - 1);
    return cfg_save(&s_cfg);
}

/* -------------------------------------------------------
 * Üretim Tarihi Kaydetme
 * ------------------------------------------------------- */
bool cfg_set_production_date(uint32_t unix_timestamp)
{
    s_cfg.production_date = unix_timestamp;
    return cfg_save(&s_cfg);
}