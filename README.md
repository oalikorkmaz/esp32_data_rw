# ESP32 Veri Okuma-Yazma Sistem Analizi ve Task Mimarisi

## 1. Proje Özeti

**Donanım:**
- ESP32-S3
- W5500 Ethernet (SPI3)
- Quectel M95-R GSM (UART)
- DS1302 RTC
- 2x16 LCD (seri)
- microSD (SPI)
- Bluetooth (NimBLE)
- Buton (BLE aç/kapa)
- RGB LED (GPIO38)

**Amaç:**
Cihaz, seçilebilir ağ arabirimlerinden (**Ethernet**, **Wi-Fi**, **GSM**) biriyle
sunucuya veri gönderecek; aynı zamanda SD karta veri kaydedecek.
İnternet kesilirse, veriler yerel olarak kaydedilecek;
bağlantı geri geldiğinde otomatik olarak gönderilecektir.
Tüm ağ seçimi BLE üzerinden yapılır.

---

## 2. Sistem Akışı

1. **Sistem Başlangıcı**
- `app_main()` temel donanım ve servisleri başlatır.  
- BLE, LED, RTC, SD vb. modüller başlatılır.  
- `net_manager_create_task()` çağrılarak ağ yönetim döngüsü başlatılır.

2. **Ağ Seçimi**
- Ethernet modülü mevcutsa, Ethernet başlatılır.  
- Yoksa Wi-Fi’ye geçilir.  
- Wi-Fi başarısızsa GSM devreye alınır.

3. **BLE ile Kullanıcı Etkileşimi**
- Kullanıcı BLE üzerinden mod değiştirir veya Wi-Fi SSID/şifre gönderir.

---

## 3. BLE Katmanı (`ble_cfg_service.c`)

| İşlev | Açıklama |
|--------|-----------|
| BLE advertising | "ESP CFG" adıyla yayın yapar. |
| `cfg_write_cb()` | BLE’den gelen komutları yorumlar. |
| `"0"`, `"1"`, `"2"` | Ağ modlarını değiştirir (Ethernet / Wi-Fi / GSM). |
| `"wifi:SSID,PASS"` | Wi-Fi bilgilerini NVS’ye kaydeder. |
| BLE LED & Button | BLE modunu açma/kapatma. |

**Amaç:**  
BLE, kullanıcıyla cihaz arasında konfigürasyon arayüzü oluşturur.

---

## 4. Network Manager Katmanı (`net_manager.c`)

| Mod | Açıklama | Fonksiyon |
|-----|-----------|-----------|
| Ethernet | W5500 üzerinden SPI bağlantısı | `start_w5500_ethernet()` |
| Wi-Fi | BLE’den alınan SSID/PASS ile STA bağlantı | `start_wifi_station()` |
| GSM | Quectel M95-R üzerinden PPP/TCP bağlantı (planlı) | `start_gsm()` |

### `net_manager_task()`
- Her **5 saniyede** bir bağlantı durumu kontrol edilir.  
- Bağlantı yoksa mevcut mod kapatılır ve bir sonrakine geçilir.  

### Event Callback’ler
- `net_manager_on_eth_event(bool up)`
- `net_manager_on_wifi_event(bool up)`

Bu fonksiyonlar, ağ sürücülerinden gelen olaylara yanıt verir.

---

## 5. Ethernet Katmanı (`ethernet_init.c`)

| Görev | Açıklama |
|--------|-----------|
| SPI3 yapılandırması | W5500’ün MISO/MOSI/SCLK/CS/RST pinlerini ayarlar. |
| Donanım tespiti | `w5500_is_present()` ile modül algılanır. |
| DHCP IP Alımı | Ethernet bağlantısı sonrası IP atanır. |
| Ping Testi | 8.8.8.8 ve `google.com` test edilir. |
| Event Handler | Bağlantı durumlarını `net_manager`’a bildirir. |

> 💡 Eğer W5500 modülü yoksa, sistem otomatik olarak Wi-Fi moduna geçer.

---

## 6. Wi-Fi Katmanı (`wifi_init.c`)

| İşlev | Açıklama |
|--------|-----------|
| `start_wifi_station()` | NVS’den SSID/PASS okur ve bağlanır. |
| `wifi_event_handler()` | Wi-Fi olaylarını işler. |
| `net_manager_on_wifi_event()` | IP alındığında `net_manager`’ı bilgilendirir. |
| `load_wifi_credentials()` | NVS’den Wi-Fi bilgilerini çeker. |

> Eğer NVS boşsa, şu log görünür:
> ```
> W (xxx) WIFI_INIT: Wi-Fi bilgisi bulunamadı (BLE üzerinden girilmeli).
> ```

---

## 7. FreeRTOS Görevleri

| Görev Adı | Dosya | Görev | Öncelik |
|------------|--------|--------|----------|
| `main_task` | `app_main.c` | Sistem başlangıcı | 1 |
| `net_manager_task` | `net_manager.c` | Ağ mod geçişi & izleme | 5 |
| `ble_host_task` | `ble_cfg_service.c` | NimBLE stack | 4 |
| `button_task` | `ble_btn.c` | BLE buton takibi | 3 |
| `eth_ping_task` | `net_eth_service.c` | IP test ve ping (ops.) | 2 |
| `gsm_task` | `gsm_service.c` | GSM internet (planlı) | 4 |

---

## 8. Veri Yönetimi (Planlı Modüller)

| Modül | İşlev |
|--------|-------|
| `sd_logger.c` | RS232 verisini SD karta kaydetme |
| `data_sender.c` | İnternet varsa verileri sunucuya gönderme |
| `data_buffer.c` | RAM/SD ara bellek yönetimi |
| `rtc_service.c` | DS1302’den tarih & saat alma |

> Ağ kesilse bile SD loglama görevi devam eder.

---

## 9. Olası Geliştirme Görevleri

| No | Görev | Modül | Durum |
|----|--------|--------|--------|
| 1 | BLE’den `"wifi:SSID,PASS"` alınca otomatik bağlanma | `ble_cfg_service.c` | ✅ |
| 2 | Wi-Fi IP aldıktan sonra ping doğrulama | `wifi_init.c` | ⏳ |
| 3 | GSM M95-R PPP/AT entegrasyonu | `gsm_service.c` | 🚧 |
| 4 | RS232 veri okuma + SD loglama | `sd_logger.c` | 🚧 |
| 5 | SD’den buffered veri gönderimi | `data_sender.c` | 🚧 |
| 6 | OTA güncelleme desteği | `ota_service.c` | 🚧 |
| 7 | BLE status characteristic (IP/sinyal bilgisi) | `ble_status_service.c` | 🚧 |
| 8 | LED renkleriyle durum bildirimi | `ble_led.c` | ✅ |

---

## 10. Veri Akış Diyagramı
```
[RS232 Sensor]
↓
[data_buffer]
↓
[SD Logger Task] ←→ [Internet Sender Task]
↓ ↑
(lokal kayıt) │
[Net Manager]
↙─────────────┼────────────↘
[Ethernet] [Wi-Fi] [GSM (M95R)]
```




---

## 11. Mevcut Durum (Kasım 2025)

| Katman | Durum | Açıklama |
|--------|--------|-----------|
| BLE | ✅ | Konfigürasyon aktif |
| Ethernet | ✅ | W5500 tespiti + DHCP + Ping |
| Wi-Fi | ✅ | BLE üzerinden SSID/PASS + bağlantı |
| GSM | 🚧 | Donanım planlanıyor |
| RS232 / SD | 🚧 | Sonraki sprint |
| OTA | 🚧 | Partition hazır, uygulama yok |

---

## 12. Geliştirme Planı

| Sprint | Hedef | Dosya / Modül |
|---------|--------|---------------|
| 1 | Wi-Fi ping & yeniden bağlanma | `wifi_init.c` |
| 2 | GSM M95-R bağlantısı | `gsm_service.c` |
| 3 | RS232 & SD loglama | `sd_logger.c` |
| 4 | SD → Server veri gönderimi | `data_sender.c` |
| 5 | BLE Status Service | `ble_status_service.c` |
| 6 | OTA update (HTTP OTA) | `ota_service.c` |

---

## 13. Teknik Notlar

- **Partition Table:** `factory` 1.5 MB (OTA geçişine uygun).  
- **Wi-Fi Bilgileri:** `wifi_cfg` namespace (NVS).  
- **BLE UUID:** Tek characteristic, custom string komutları.  
- **Net Manager:** 5 saniyelik bağlantı kontrol periyodu.  
- **SD & Data:** Bağlantı yoksa veriler lokal saklanır, bağlantı gelince upload edilir.

---

## 14. Genel Durum Özeti

| Özellik | Durum |
|----------|--------|
| Dinamik ağ geçişi (Ethernet → Wi-Fi → GSM) | ✅ |
| Donanım otomatik algılama (W5500 yoksa atla) | ✅ |
| BLE konfigürasyonu | ✅ |
| Wi-Fi kaydı & otomatik reconnect | ✅ |
| FreeRTOS görev yapısı | ✅ |
| GSM iletişimi | 🚧 |
| RS232 / SD loglama | 🚧 |
| OTA güncelleme | 🚧 |

---

## Özet

Bu sistem:
- **Çoklu ağ arabirimi yönetimi**,  
- **BLE üzerinden dinamik konfigürasyon**,  
- **Otomatik fallback (Wi-Fi ↔ GSM ↔ Ethernet)**  
mekanizmasını başarıyla çalıştırmaktadır.  

Bir sonraki hedef, **Wi-Fi bağlantı sonrası IP testleri** ve  
**GSM / SD / OTA modüllerinin entegrasyonudur.**
