#include "net_manager.h"
#include "esp_eth.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "ethernet_init.h"
#include "wifi_init.h"


static const char *TAG = "NET_MANAGER";

static net_mode_t s_current_mode = NET_MODE_ETHERNET;
static net_mode_t s_last_mode = NET_MODE_ETHERNET;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static esp_netif_t *s_netif = NULL;
static bool s_connected = false;
static bool s_eth_link_up = false;
static bool s_eth_present = false;

/* Handle bildirimi */
void net_manager_set_eth_handle(esp_eth_handle_t handle,
                                esp_eth_netif_glue_handle_t glue,
                                esp_netif_t *netif)
{
    s_eth_handle = handle;
    s_glue = glue;
    s_netif = netif;
}

/* Kapatma fonksiyonu */
static void stop_current_connection(void)
{
    if (s_eth_handle) {
        ESP_LOGW(TAG, "Ethernet bağlantısı durduruluyor...");
        esp_eth_stop(s_eth_handle);
        esp_eth_del_netif_glue(s_glue);
        s_glue = NULL;
        esp_netif_destroy(s_netif);
        s_netif = NULL;
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
        spi_bus_free(SPI3_HOST);
        gpio_set_level(PIN_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PIN_RST, 1);
        ESP_LOGW(TAG, "Ethernet bağlantısı tamamen durduruldu.");
    }
}

/* Ethernet olay bildirimi */
void net_manager_on_eth_event(bool link_up)
{
    s_eth_link_up = link_up;
    ESP_LOGI(TAG, "Ethernet bağlantı durumu: %s", link_up ? "UP" : "DOWN");
}

/* Wi-Fi olay bildirimi */
void net_manager_on_wifi_event(bool connected)
{
    s_connected = connected;
    ESP_LOGI(TAG, "Wi-Fi bağlantı durumu: %s", connected ? "UP" : "DOWN");
}

/* Bağlantı durumu kontrolü */
static bool net_manager_check_connection(void)
{
    if (s_current_mode == NET_MODE_ETHERNET)
        return s_eth_link_up;
    return s_connected;
}

/* Ethernet donanım kontrolü (basit tespit) */
static bool check_w5500_presence(void)
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
        .clock_speed_hz = 1 * 1000 * 1000,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };

    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus başlatılamadı, W5500 tespit atlanıyor.");
        return false;
    }

    spi_bus_add_device(SPI3_HOST, &devcfg, &spi);

    // W5500 Version Register = 0x0039
    uint8_t tx[4] = {0x00, 0x39, 0x00, 0x00};
    uint8_t rx[4] = {0};
    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t tr = spi_device_transmit(spi, &t);

    spi_bus_remove_device(spi);
    spi_bus_free(SPI3_HOST);

    if (tr != ESP_OK) {
        ESP_LOGW(TAG, "W5500 SPI iletişimi başarısız (%s)", esp_err_to_name(tr));
        return false;
    }

    uint8_t version = rx[3];
    if (version == 0x03 || version == 0x04) {
        ESP_LOGI(TAG, "W5500 modülü tespit edildi (version=0x%02X).", version);
        return true;
    } else {
        ESP_LOGW(TAG, "W5500 modülü tespit edilemedi (version=0x%02X).", version);
        return false;
    }
}

/* Modu değiştir */
void net_manager_set_mode(net_mode_t mode)
{
    s_last_mode = s_current_mode;
    s_current_mode = mode;
}

/* Mod başlat */
void net_manager_start(void)
{
    switch (s_current_mode) {
    case NET_MODE_ETHERNET:
        ESP_LOGI(TAG, "Ethernet başlatılıyor...");

        // W5500 donanım kontrolü
        s_eth_present = check_w5500_presence();
        if (!s_eth_present) {
            ESP_LOGW(TAG, "Ethernet donanımı bulunamadı, WiFi moduna geçiliyor...");
            s_current_mode = NET_MODE_WIFI;
            net_manager_start();
            return;
        }

        start_w5500_ethernet();
        break;

    case NET_MODE_WIFI:
        ESP_LOGI(TAG, "WiFi başlatılıyor...");
        start_wifi_station();
        break;

    case NET_MODE_GSM:
        ESP_LOGI(TAG, "GSM başlatılıyor...");
        // start_gsm(); // gelecek adım
        break;

    default:
        break;
    }
}

/* Otomatik ağ yönetimi görevi */
static void net_manager_task(void *arg)
{
    while (1) {
        if (!net_manager_check_connection()) {
            ESP_LOGW(TAG, "Ağ bağlantısı yok, sonraki moda geçiliyor...");
            stop_current_connection();

            switch (s_current_mode) {
            case NET_MODE_ETHERNET:
                s_current_mode = NET_MODE_WIFI;
                break;
            case NET_MODE_WIFI:
                s_current_mode = NET_MODE_GSM;
                break;
            case NET_MODE_GSM:
                s_current_mode = NET_MODE_ETHERNET;
                break;
            default:
                s_current_mode = NET_MODE_ETHERNET;
                break;
            }

            ESP_LOGI(TAG, "Yeni mod: %d", s_current_mode);
            net_manager_start();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5 saniyede bir denetle
    }
}

/* Görevi başlat */
void net_manager_create_task(void)
{
    xTaskCreate(net_manager_task, "net_manager_task", 4096, NULL, 5, NULL);
}
