#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "net_manager.h"
#include "spi_if.h"

extern esp_err_t register_eth_service(void);
extern void set_eth_handle(esp_eth_handle_t handle,
                           esp_eth_netif_glue_handle_t glue,
                           esp_netif_t *netif);

/* ---------------------------------------------------- */
/* Donanım pinleri */
#define PIN_MISO  13
#define PIN_MOSI  11
#define PIN_SCLK  12
#define PIN_CS    10
#define PIN_RST   8
#define PIN_INT   9
#define SPI_HOST  SPI3_HOST
#define SPI_CLOCK_MHZ 8
/* ---------------------------------------------------- */

static const char *TAG = "ETH_INIT";

/* Global değişkenler */
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static esp_netif_t *s_netif = NULL;
static bool s_eth_running = false;

/* ---------------------------------------------------- */
/* SPI Bus başlatma */
esp_err_t ethernet_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    return spi_if_init(SPI_HOST, &buscfg);
}

/* ---------------------------------------------------- */
/* Donanım reset */
static void w5500_hardware_reset(void)
{
    gpio_config_t rst_conf = {
        .pin_bit_mask = 1ULL << PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);

    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "W5500 reset tamamlandı.");
}

/* ---------------------------------------------------- */
/* Ethernet başlatma */
esp_err_t start_w5500_ethernet(void)
{
    if (s_eth_running) {
        ESP_LOGW(TAG, "Ethernet zaten çalışıyor!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Ethernet başlatılıyor...");

    // 1️⃣ Ağ sistemlerini başlat (zaten varsa hata verme)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    // 2️⃣ SPI başlat
    ESP_ERROR_CHECK(ethernet_spi_init());

    // ⚙️ ISR servisi kurulmadıysa kur
    if (gpio_install_isr_service(0) != ESP_OK) {
        ESP_LOGW(TAG, "GPIO ISR servisi zaten kurulu.");
    }

    // 3️⃣ Donanım reset
    w5500_hardware_reset();

    // 4️⃣ Netif oluştur
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&cfg);
    if (!s_netif) {
        ESP_LOGE(TAG, "Netif oluşturulamadı!");
        return ESP_FAIL;
    }

    // 5️⃣ SPI ayarları
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 20,
        // Burada command_bits/address_bits TANIMLANMAMALI!
    };

    // 6️⃣ W5500 config
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = PIN_INT;  // aktif interrupt pini

    // 7️⃣ MAC & PHY config
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;              // eski sürümde sabitti
    phy_config.reset_gpio_num = PIN_RST;  // donanım reset pini aktif

    // 8️⃣ MAC & PHY oluştur
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!mac || !phy) {
        ESP_LOGE(TAG, "MAC/PHY oluşturulamadı!");
        if (s_netif) esp_netif_destroy(s_netif);
        s_netif = NULL;
        return ESP_FAIL;
    }

    // 9️⃣ Ethernet driver kurulumu
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t ret1 = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (ret1 != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver kurulamadı: %s", esp_err_to_name(ret1));
        if (s_netif) esp_netif_destroy(s_netif);
        s_netif = NULL;
        return ret1;
    }

    // 🔟 MAC adresi ayarla
    uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    // 1️⃣1️⃣ Netif'e bağla
    s_glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(s_netif, s_glue));

    // 1️⃣2️⃣ Event servisleri kaydet
    ESP_ERROR_CHECK(register_eth_service());
    set_eth_handle(s_eth_handle, s_glue, s_netif);

    // 1️⃣3️⃣ DHCP & Başlat
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(s_netif));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    s_eth_running = true;
    ESP_LOGI(TAG, "✓ Ethernet başlatıldı, IP bekleniyor...");

    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Ethernet durdurma */
void stop_w5500_ethernet(void)
{
    if (!s_eth_running) {
        ESP_LOGD(TAG, "Ethernet zaten durdurulmuş.");
        return;
    }

    ESP_LOGW(TAG, "Ethernet durduruluyor...");

    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }

    if (s_glue) {
        esp_eth_del_netif_glue(s_glue);
        s_glue = NULL;
    }

    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }

    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    s_eth_running = false;
    ESP_LOGI(TAG, "✓ Ethernet tamamen durduruldu.");
}

/* ---------------------------------------------------- */
/* Durum sorgulama */
bool is_ethernet_running(void)
{
    return s_eth_running;
}
