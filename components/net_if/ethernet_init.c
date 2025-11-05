#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "net_manager.h"

/* net_eth_service.c içindeki servisleri dahil et */
extern esp_err_t register_eth_service(void);
extern void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

/* ---------------------------------------------------- */
/* Donanım konfigürasyon tanımları - senin proje pinlerine göre ayarla */
#define PIN_MISO  13
#define PIN_MOSI  11
#define PIN_SCLK  12
#define PIN_CS    10
#define PIN_RST   8
#define PIN_INT   9
#define SPI_HOST  SPI3_HOST
#define SPI_CLOCK_MHZ 8
/* ---------------------------------------------------- */
extern void set_eth_handle(esp_eth_handle_t handle,
                           esp_eth_netif_glue_handle_t glue,
                           esp_netif_t *netif);


static const char *TAG = "ETH_INIT";

/* Global handle’lar (durdurma fonksiyonu için erişilebilir) */
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static esp_netif_t *s_netif = NULL;

/* ---------------------------------------------------- */
/* W5500 donanım tespiti için basit SPI okuma */
static bool w5500_is_present(void)
{
    spi_device_handle_t spi;
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 1 * 1000 * 1000,  // düşük hızla test
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };

    esp_err_t ret = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus zaten başlatılmış (test modu).");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus başlatılamadı (0x%x)", ret);
        return false;
    }

    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST, &devcfg, &spi));

    // W5500 Version Register adresi = 0x0039
    uint8_t tx[4] = {0x00, 0x39, 0x00, 0x00};
    uint8_t rx[4] = {0};

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t tr = spi_device_transmit(spi, &t);

    spi_bus_remove_device(spi);
    spi_bus_free(SPI_HOST);

    if (tr != ESP_OK) {
        ESP_LOGW(TAG, "SPI iletişimi başarısız (%s)", esp_err_to_name(tr));
        return false;
    }

    uint8_t version = rx[3];
    ESP_LOGI(TAG, "W5500 versiyon kaydı: 0x%02X", version);
    return (version == 0x03 || version == 0x04);
}

/* ---------------------------------------------------- */
/* Ethernet Başlatma */
esp_err_t start_w5500_ethernet(void)
{
    ESP_LOGI(TAG, "Ethernet başlatılıyor...");

    // 1️⃣ Donanım tespiti
    if (!w5500_is_present()) {
        ESP_LOGW(TAG, "W5500 modülü algılanamadı. Ethernet pasif hale getirildi.");
        net_manager_on_eth_event(false);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "W5500 modülü algılandı.");

    // 2️⃣ Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "Event loop zaten oluşturulmuş, atlanıyor.");
    else
        ESP_ERROR_CHECK(ret);

    // 3️⃣ SPI Bus yapılandırması
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t ret1 = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret1 == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "SPI bus zaten başlatılmış, atlanıyor.");
    else
        ESP_ERROR_CHECK(ret1);

    // 4️⃣ Reset pini ayarla
    gpio_config_t rst_conf = {.pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&rst_conf);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5️⃣ Netif oluştur
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&cfg);

    // 6️⃣ SPI device yapılandırması
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 20,
    };

    // 7️⃣ MAC & PHY
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = PIN_RST;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_INT;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    // 8️⃣ Sürücü kurulumu
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));

    uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    // 9️⃣ Netif bağla
    s_glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(s_netif, s_glue));

    //  🔟 Servisleri ve handler’ı bağla
    ESP_ERROR_CHECK(register_eth_service());
    set_eth_handle(s_eth_handle, s_glue, s_netif);

    // DHCP ve sürücü başlat
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(s_netif));
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    ESP_LOGI(TAG, "Ethernet başlatıldı, IP bekleniyor...");
    return ESP_OK;
}

/* ---------------------------------------------------- */
/* Ethernet Durdurma */
void stop_w5500_ethernet(void)
{
    ESP_LOGW(TAG, "Ethernet durduruluyor...");

    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_glue) {
        esp_eth_del_netif_glue(s_glue);
        s_glue = NULL;
    }

    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }

    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }

    spi_bus_free(SPI_HOST);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);

    ESP_LOGI(TAG, "Ethernet tamamen durduruldu.");
}
