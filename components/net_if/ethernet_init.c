// ethernet_init.c
#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net_manager.h"
#include "spi_if.h"

extern esp_err_t register_eth_service(void);
extern void set_eth_handle(esp_eth_handle_t handle,
                           esp_eth_netif_glue_handle_t glue,
                           esp_netif_t *netif);

/* ------------------------- Donanım Pinleri ------------------------- */
/* SD ile aynı SPI hattı: SPI2 */
#define ETH_MISO        37
#define ETH_MOSI        35
#define ETH_SCLK        36
#define ETH_CS          48
#define ETH_RST         -1   // Reset hattı yok
#define ETH_INT         -1   // Interrupt hattı yok (polling kullanılacak)
#define ETH_HOST        SPI2_HOST
#define ETH_CLOCK_MHZ   8
/* ------------------------------------------------------------------ */

static const char *TAG = "ETH_INIT";

/* Global durum */
static esp_eth_handle_t            s_eth_handle   = NULL;
static esp_eth_netif_glue_handle_t s_glue         = NULL;
static esp_netif_t                *s_netif        = NULL;
static bool                        s_eth_running  = false;

/* Eğer ortak SPI yönetimi kullanıyorsan (SD ile paylaşım): sadece BUS init et.
   W5500 için ayrıyeten spi_device KAYDI YAPMIYORUZ (IDF sürücüsü kendi halleder). */
static esp_err_t ethernet_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num  = ETH_MISO,
        .mosi_io_num  = ETH_MOSI,
        .sclk_io_num  = ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // .max_transfer_sz = 4096, // gerekirse SD için artırılabilir
    };



    // Bus zaten başka bir yerde (ör. SD) init edildiyse spi_if_init ESP_ERR_INVALID_STATE döndürebilir; sorun değil.
    esp_err_t ret = spi_if_init(ETH_HOST, &buscfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init hatası: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✅ SPI bus hazır (HOST=%d)", ETH_HOST);
    return ESP_OK;
}

static void w5500_hardware_reset(void)
{
    if (ETH_RST < 0) {
        ESP_LOGI(TAG, "Reset pini yok, atlanıyor.");
        return;
    }

    gpio_config_t rst_conf = {
        .pin_bit_mask = 1ULL << ETH_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);

    gpio_set_level(ETH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ETH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "✅ W5500 donanım reseti tamam.");
}

esp_err_t start_w5500_ethernet(void)
{
    if (s_eth_running) {
        ESP_LOGW(TAG, "Ethernet zaten çalışıyor.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "🌐 Ethernet başlatılıyor...");

    // Netif & Event loop
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    // SPI bus
    ESP_ERROR_CHECK(ethernet_spi_init());

    // INT yoksa ISR servisine gerek yok; kuruluysa ESP_ERR_INVALID_STATE gelebilir.
    if (ETH_INT >= 0) {
        if (gpio_install_isr_service(0) != ESP_OK) {
            ESP_LOGD(TAG, "GPIO ISR servisi zaten kurulu.");
        }
    }

    w5500_hardware_reset();

    // Netif oluştur
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&cfg);
    if (!s_netif) {
        ESP_LOGE(TAG, "❌ esp_netif_new başarısız!");
        return ESP_FAIL;
    }

    // W5500 konfigürasyonu (IDF 5.5.x)
    static spi_device_interface_config_t s_w5500_dev_cfg = {
    .mode = 0,
    .clock_speed_hz = ETH_CLOCK_MHZ * 1000 * 1000,
    .spics_io_num = ETH_CS,
    .queue_size = 20,
    .command_bits = 0,   // W5500 için 0 olmalı
    .address_bits = 0,   // W5500 için 0 olmalı
    // .dummy_bits = 0,   // varsayılan yeterli
    };
    // DİKKAT: spi_device eklemiyoruz; sürücü kendi ekliyor.
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_HOST, &s_w5500_dev_cfg);
    w5500_config.int_gpio_num = ETH_INT;

    if (ETH_INT < 0) {
        // Interrupt yok → polling
        w5500_config.poll_period_ms = 100;   // 100 ms iyi bir başlangıç
    }

    // MAC/PHY konfig
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr       = 1;        // W5500 internal PHY addr
    phy_config.reset_gpio_num = ETH_RST;  // -1 ise sürücü reset atlamayı bilir

    // MAC/PHY oluştur
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "❌ MAC/PHY oluşturulamadı!");
        if (mac) mac->del(mac);
        if (phy) phy->del(phy);
        esp_netif_destroy(s_netif);
        s_netif = NULL;
        return ESP_FAIL;
    }

    // Driver kurulum
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_eth_driver_install: %s", esp_err_to_name(ret));
        mac->del(mac);
        phy->del(phy);
        esp_netif_destroy(s_netif);
        s_netif = NULL;
        return ret;
    }

    // (Opsiyonel) MAC adresi ata — benzersiz sabit MAC kullanmak istersen burayı düzenle
    uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MAC set edilemedi (%s). EFUSE/local MAC kullanılacaktır.", esp_err_to_name(ret));
    }

    // Netif'e bağla (glue)
    s_glue = esp_eth_new_netif_glue(s_eth_handle);
    ret = esp_netif_attach(s_netif, s_glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_netif_attach: %s", esp_err_to_name(ret));
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
        esp_netif_destroy(s_netif);
        s_netif = NULL;
        return ret;
    }

    // Event servisleri + handle paylaş
    ESP_ERROR_CHECK(register_eth_service());
    set_eth_handle(s_eth_handle, s_glue, s_netif);

    // DHCP
    ret = esp_netif_dhcpc_start(s_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGE(TAG, "❌ DHCP start: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start
    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ esp_eth_start: %s", esp_err_to_name(ret));
        return ret;
    }

    s_eth_running = true;
    ESP_LOGI(TAG, "✅ Ethernet başlatıldı, IP bekleniyor...");
    return ESP_OK;
}

void stop_w5500_ethernet(void)
{
    if (!s_eth_running) {
        ESP_LOGD(TAG, "Ethernet zaten durdurulmuş.");
        return;
    }

    ESP_LOGW(TAG, "⚠️  Ethernet durduruluyor...");
    
    // 1) Çalışmayı durdur
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // 2) Glue’yu kaldır (ref --)
    if (s_glue) {
        esp_eth_del_netif_glue(s_glue);
        s_glue = NULL;
    }
    
    // 3) Netif’i temizle
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }

    // 4) Sürücüyü kaldır
    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }

    // Reset hattı varsa güç tasarrufu için LOW çek
    if (ETH_RST >= 0) {
        gpio_set_level(ETH_RST, 0);
    }

    s_eth_running = false;
    ESP_LOGI(TAG, "✅ Ethernet tamamen durduruldu.");
}

bool is_ethernet_running(void)
{
    return s_eth_running;
}
