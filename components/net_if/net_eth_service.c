#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_err.h"
#include "ethernet_init.h"
#include "ping/ping_sock.h"   // ping test için
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"


static const char *TAG = "net_eth_service";
static esp_eth_handle_t *eth_handles = NULL;
static uint8_t eth_port_cnt = 0;
static bool eth_started = false;

/** Ethernet olaylarını dinler */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up - MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2],
                     mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}

/** IP alındığında çağrılır */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ip_info->ip));
}

/** Ethernet başlatma */
esp_err_t net_eth_start(void)
{
    if (eth_started) {
        ESP_LOGW(TAG, "Ethernet already started");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handles[0]);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
    eth_started = true;

    ESP_LOGI(TAG, "Ethernet service started");
    return ESP_OK;
}

/** Ethernet durdurma */
esp_err_t net_eth_stop(void)
{
    if (!eth_started) {
        ESP_LOGW(TAG, "Ethernet not running");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_eth_stop(eth_handles[0]));
    ESP_ERROR_CHECK(example_eth_deinit(eth_handles, eth_port_cnt));
    eth_started = false;
    ESP_LOGI(TAG, "Ethernet stopped");
    return ESP_OK;
}

/** Ping testi */
esp_err_t net_eth_ping_test(void)
{
    if (!eth_started) {
        ESP_LOGW(TAG, "Ethernet not started");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Pinging Google DNS (8.8.8.8)...");
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ip_addr_t target_addr;
    inet_pton(AF_INET, "8.8.8.8", &target_addr.u_addr.ip4);
    target_addr.type = IPADDR_TYPE_V4;
    ping_config.target_addr = target_addr;

    esp_ping_handle_t ping;
    esp_ping_callbacks_t cbs = { 0 };
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
    vTaskDelay(pdMS_TO_TICKS(5000)); // 5 saniye bekle
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);

    ESP_LOGI(TAG, "Ping test complete");
    return ESP_OK;
}
