#include "ethernet_init.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* net_eth_service.c içindeki servisleri dahil et */
extern esp_err_t register_eth_service(void);
extern void set_eth_handle(esp_eth_handle_t handle);

static const char *TAG = "ETH_INIT";

/* Global netif */
esp_netif_t *eth_netif = NULL;

/* PIN tanımları */
#define PIN_MISO  13
#define PIN_MOSI  11
#define PIN_SCLK  12
#define PIN_CS    10
#define PIN_RST   8
#define PIN_INT   9
#define SPI_HOST  SPI3_HOST
#define SPI_CLOCK_MHZ 8

esp_err_t start_w5500_ethernet(void)
{
    ESP_LOGI(TAG, "Ethernet başlatılıyor...");

    /* Ağ ve olay sistemi */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* SPI bus başlat */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* Reset pini */
    gpio_config_t rst_conf = { .pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst_conf);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Netif oluştur */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);

    /* SPI cihaz ayarları */
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 20,
    };

    /* MAC & PHY oluştur */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = PIN_RST;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_INT;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    /* Ethernet sürücüsünü kur */
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* MAC adresi ata */
    uint8_t custom_mac[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, custom_mac));

    /* Netif’e bağla */
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    /* Servisleri kaydet (IP event handler, ping vs) */
    ESP_ERROR_CHECK(register_eth_service());
    set_eth_handle(eth_handle);

    /* DHCP başlat */
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

    /* Ethernet başlat */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Ethernet başlatıldı, IP bekleniyor...");
    return ESP_OK;
}
