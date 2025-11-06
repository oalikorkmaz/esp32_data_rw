#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "net_manager.h"

static const char *TAG_NET = "ETH_SERVICE";
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;
static esp_netif_t *s_netif = NULL;  // ✅ Pointer olmalı
static bool s_event_registered = false;

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
            if (s_eth_handle != NULL) {
                esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
                ESP_LOGI(TAG_NET, "Ethernet Bağlandı");
                ESP_LOGI(TAG_NET, "HW Adresi: %02x:%02x:%02x:%02x:%02x:%02x",
                         mac_addr[0], mac_addr[1], mac_addr[2],
                         mac_addr[3], mac_addr[4], mac_addr[5]);
            }
            
            // ✅ Net manager'a link UP bildir
            net_manager_on_eth_event(true);
            break;

        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_NET, "Ethernet Bağlantısı Kesildi");
            
            // ✅ Net manager'a link DOWN bildir
            net_manager_on_eth_event(false);
            break;

        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG_NET, "Ethernet Durduruldu");
            
            // ✅ Net manager'a link DOWN bildir
            net_manager_on_eth_event(false);
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
            
            // ✅ Net manager'a IP alındı bilgisi gönder
            net_manager_on_eth_got_ip();
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
    // ✅ Tekrar kayıt yapma
    if (s_event_registered) {
        ESP_LOGD(TAG_NET, "Event handler zaten kayıtlı.");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, 
                                               &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, 
                                               &eth_event_handler, NULL));

    s_event_registered = true;
    ESP_LOGI(TAG_NET, "Ethernet olay işleyicileri başarıyla kaydedildi.");
    return ESP_OK;
}

/* ----------------- Event Handler'ları Kaldır ----------------- */
void unregister_eth_service(void)
{
    if (!s_event_registered) {
        return;
    }

    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_event_handler);
    
    s_event_registered = false;
    ESP_LOGI(TAG_NET, "Ethernet olay işleyicileri kaldırıldı.");
}

/* ----------------- Handle Saklama ----------------- */
void set_eth_handle(esp_eth_handle_t handle,
                   esp_eth_netif_glue_handle_t glue,
                   esp_netif_t *netif)
{
    s_eth_handle = handle;
    s_glue = glue;
    s_netif = netif;
}