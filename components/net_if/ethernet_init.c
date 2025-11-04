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
extern void set_eth_handle(esp_eth_handle_t handle);
static const char *TAG = "ETH_INIT";

esp_err_t start_w5500_ethernet(void)
{
    ESP_LOGI(TAG, "Ethernet başlatılıyor...");

    /* Ağ sistemi */
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Event loop zaten oluşturulmuş, atlanıyor.");
    } else {
        ESP_ERROR_CHECK(ret);
    }

    /* SPI BUS */
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t ret1 = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret1 == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus zaten başlatılmış, atlanıyor.");
    } else {
        ESP_ERROR_CHECK(ret1);
    }

    /* Reset pini */
    gpio_config_t rst_conf = { .pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst_conf);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Netif */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    /* SPI device */
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 20,
    };

    /* MAC & PHY */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = PIN_RST;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_INT;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    /* Ethernet sürücüsü */
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* MAC adresi ata */
    uint8_t mac_addr[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr));

    /* Netif’e bağla */
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    /* Olay işleyicisi kaydet */
    ESP_ERROR_CHECK(register_eth_service());

    /* 🔹 Handle’ı net_manager’a bildir */
    net_manager_set_eth_handle(eth_handle, glue, eth_netif);

    /* DHCP */
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

    /* Ethernet başlat */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "Ethernet başlatıldı, IP bekleniyor...");
    return ESP_OK;
}

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT)
    {
        switch (event_id)
        {
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet bağlandı");
                net_manager_on_eth_event(true);  // 🔹 net_manager’a bildir
                break;

            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGW(TAG, "Ethernet bağlantısı kesildi");
                net_manager_on_eth_event(false); // 🔹 net_manager’a bildir
                break;

            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet başlatıldı");
                break;

            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet durduruldu");
                net_manager_on_eth_event(false);
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP Alındı: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

