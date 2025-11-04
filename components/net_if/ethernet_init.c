#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "ETH_W5500_BASIC";

/* ------------------- PINLERİ KENDİ W5500 LITE KARTINA GÖRE AYARLA ------------------- */
#define PIN_MISO  13
#define PIN_MOSI  11
#define PIN_SCLK  12
#define PIN_CS    10
#define PIN_RST   8
#define PIN_INT   9
#define SPI_HOST  SPI3_HOST
#define SPI_CLOCK_MHZ 8
/* ----------------------------------------------------------------------------------- */

/* Ethernet olay işleyicisi */
static void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    uint8_t mac[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)data;

    switch (id) {
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
            ESP_LOGI(TAG, "Connected. MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Disconnected");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}

/* IP alındığında çağrılır */
static void got_ip_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
}

void app_main(void) {
    ESP_LOGI(TAG, "W5500 Ethernet örneği başlatılıyor...");

    /* 1. Ağ yığını başlat */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 2. SPI bus başlat */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* 3. W5500 reset */
    gpio_config_t rst_conf = { .pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst_conf);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 4. Netif oluştur */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    /* 5. SPI cihaz ayarları */
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 20,
    };

    /* 6. W5500 MAC ve PHY oluştur */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = PIN_RST;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_INT;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    /* 7. Ethernet sürücüsünü kur */
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* 8. Benzersiz bir MAC adresi ata */
    uint8_t custom_mac[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, custom_mac));

    /* 9. Netif'e bağla */
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    /* 10. Event handler kayıtları */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_handler, NULL));

    /* 11. DHCP başlat */
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

    /* 12. Ethernet başlat */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Ethernet başlatıldı, IP bekleniyor...");
}
