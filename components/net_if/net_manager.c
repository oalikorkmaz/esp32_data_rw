#include "esp_eth.h"
#include "net_manager.h"
#include "ethernet_init.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "NET_MANAGER";

static net_mode_t s_current_mode = NET_MODE_ETHERNET;
static net_mode_t s_last_mode = NET_MODE_ETHERNET;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static esp_netif_t *s_netif = NULL;
static bool s_connected = false;
static bool s_eth_link_up = false;


/* Handle bildirimi */
void net_manager_set_eth_handle(esp_eth_handle_t handle,
                                esp_eth_netif_glue_handle_t glue,
                                esp_netif_t *netif)
{
    s_eth_handle = handle;
    s_glue = glue;
    s_netif = netif;
}

/* Kapatma fonksiyonu — önceki sürümdekiyle aynı */
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

void net_manager_on_eth_event(bool link_up)
{
    s_eth_link_up = link_up;
    ESP_LOGI("NET_MANAGER", "Ethernet bağlantı durumu: %s", link_up ? "UP" : "DOWN");
}

static bool net_manager_check_connection(void)
{
    if (s_current_mode == NET_MODE_ETHERNET)
        return s_eth_link_up;
    return s_connected;
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
        start_w5500_ethernet();
        break;
    case NET_MODE_WIFI:
        ESP_LOGI(TAG, "WiFi başlatılıyor...");
        // start_wifi(); // gelecek adım
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
