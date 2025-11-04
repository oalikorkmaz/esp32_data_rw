#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_ping.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG_NET = "ETH_SERVICE";
static esp_eth_handle_t s_eth_handle = NULL;

/* --------------------- PING TASK --------------------- */
static void ping_task(void *pvParameters)
{
    /* 1️⃣ Ping konfigürasyonu */
    ip_addr_t target_addr;
    inet_pton(AF_INET, "8.8.8.8", &target_addr.u_addr.ip4);
    target_addr.type = IPADDR_TYPE_V4;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 4; // 4 paket
    ping_config.interval_ms = 1000;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = NULL,
        .on_ping_timeout = NULL,
        .on_ping_end = NULL,
    };

    ESP_LOGI(TAG_NET, "Ping 8.8.8.8 başlatılıyor...");
    esp_ping_handle_t ping;
    if (esp_ping_new_session(&ping_config, &cbs, &ping) == ESP_OK)
    {
        esp_ping_start(ping);
        vTaskDelay(pdMS_TO_TICKS(6000));
        esp_ping_stop(ping);
        esp_ping_delete_session(ping);
    }
    else
    {
        ESP_LOGE(TAG_NET, "Ping oturumu başlatılamadı!");
    }

    /* 2️⃣ DNS çözümleme testi */
    struct hostent *he = gethostbyname("google.com");
    if (he)
    {
        ESP_LOGI(TAG_NET, "DNS çözümleme başarılı, google.com IP: %s",
                 inet_ntoa(*(struct in_addr *)he->h_addr));
    }
    else
    {
        ESP_LOGE(TAG_NET, "DNS çözümleme başarısız!");
    }

    ESP_LOGI(TAG_NET, "Ping task tamamlandı.");
    vTaskDelete(NULL);
}

/* IP alındığında başlatmak için fonksiyon */
static void start_ping_task(void)
{
    xTaskCreate(ping_task, "ping_task", 4096, NULL, 5, NULL);
}

/* ----------------- Event handler ----------------- */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};

    if (event_base == ETH_EVENT)
    {
        switch (event_id)
        {
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG_NET, "Ethernet Başlatıldı");
            break;

        case ETHERNET_EVENT_CONNECTED:
            if (s_eth_handle != NULL)
            {
                esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
                ESP_LOGI(TAG_NET, "Ethernet Bağlandı");
                ESP_LOGI(TAG_NET, "HW Adresi: %02x:%02x:%02x:%02x:%02x:%02x",
                         mac_addr[0], mac_addr[1], mac_addr[2],
                         mac_addr[3], mac_addr[4], mac_addr[5]);
            }
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_NET, "Ethernet Bağlantısı Kesildi");
            break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG_NET, "Ethernet Durduruldu");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_ETH_GOT_IP:
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            const esp_netif_ip_info_t *ip_info = &event->ip_info;

            char ip[16], gw[16], netmask[16];
            esp_ip4addr_ntoa(&ip_info->ip, ip, sizeof(ip));
            esp_ip4addr_ntoa(&ip_info->gw, gw, sizeof(gw));
            esp_ip4addr_ntoa(&ip_info->netmask, netmask, sizeof(netmask));

            ESP_LOGI(TAG_NET, "Ethernet'ten IP Alındı");
            ESP_LOGI(TAG_NET, "~~~~~~~~~~~");
            ESP_LOGI(TAG_NET, "IP Adresi : %s", ip);
            ESP_LOGI(TAG_NET, "Ağ Maskesi: %s", netmask);
            ESP_LOGI(TAG_NET, "Ağ Geçidi : %s", gw);
            ESP_LOGI(TAG_NET, "~~~~~~~~~~~");

            /* 🔔 IP alındığında ping başlat */
            start_ping_task();
            break;
        }
        default:
            break;
        }
    }
}

/* ----------------- Servis Kayıtları ----------------- */
esp_err_t register_eth_service(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_event_handler, NULL));

    ESP_LOGI(TAG_NET, "Ethernet olay işleyicileri başarıyla kaydedildi.");
    return ESP_OK;
}

void set_eth_handle(esp_eth_handle_t handle)
{
    s_eth_handle = handle;
}
