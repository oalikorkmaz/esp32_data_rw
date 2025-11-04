#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "ethernet_init.h"

extern esp_err_t register_eth_service(void);
extern void set_eth_handle(esp_eth_handle_t handle);

static const char *TAG = "ETH_INIT";

/* ----------------------------- PIN AYARLARI ----------------------------- */
#define ETH_SPI_HOST       SPI3_HOST
#define ETH_MISO_GPIO      13
#define ETH_MOSI_GPIO      11
#define ETH_SCLK_GPIO      12
#define ETH_CS_GPIO        10
#define ETH_RST_GPIO       8
#define ETH_INT_GPIO       9
#define ETH_SPI_CLOCK_MHZ  8
/* ----------------------------------------------------------------------- */

/* SPI Bus başlatma */
static void init_spi_bus(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_MISO_GPIO,
        .mosi_io_num = ETH_MOSI_GPIO,
        .sclk_io_num = ETH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

/* Donanım reset pini */
static void w5500_reset(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << ETH_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(ETH_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(ETH_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* Ana fonksiyon */
esp_err_t start_w5500_ethernet(void)
{
    ESP_LOGI(TAG, "Ethernet başlatılıyor...");

    /* A. Ağ ve olay sistemi */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* B. SPI veri yolu */
    init_spi_bus();
    w5500_reset();

    /* C. Ethernet netif oluştur */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    /* D. MAC / PHY konfigürasyonu */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ETH_RST_GPIO;

    /* E. SPI cihaz arayüzü */
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = ETH_CS_GPIO,
        .queue_size = 20,
    };

    /* ✅ F. W5500 konfigürasyonu - DÜZELTİLMİŞ */
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = ETH_INT_GPIO;

    /* G. MAC & PHY oluştur */
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    /* H. Ethernet sürücüsünü oluştur */
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* I. Netif'e bağla */
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    /* J. Event handler'ları kaydet */
    ESP_ERROR_CHECK(register_eth_service());
    
    /* J2. Ethernet handle'ı kaydet */
    set_eth_handle(eth_handle);

    /* K. Ethernet'i başlat */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "W5500 Ethernet başarıyla başlatıldı!");
    return ESP_OK;
}