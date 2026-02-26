#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/queue.h"
#include "now.h"

static const char *TAG = "NOW";

static QueueHandle_t nowQueue;

static const uint8_t ESPNOW_BROADCAST_MAC[ESP_NOW_ETH_ALEN] =
    {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ESP-NOW receive callback */
static void nowRecvCb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int data_len)
{
    t_payload pl;
    if (data_len > MAX_PAYLOAD_SIZE)
    {
        data_len = MAX_PAYLOAD_SIZE;
    }
    
    pl.len = data_len;
    memcpy(&pl.data,data,data_len);
    xQueueSend(nowQueue, (void *)&pl, 0);
}


void nowSend(const void *data, size_t len)
{
    //ESP_LOG_BUFFER_HEX_LEVEL("TX", data,len,ESP_LOG_INFO);
    esp_now_send(ESPNOW_BROADCAST_MAC, data, len);
}

bool nowReceive( t_payload * pl)
{
    // 1 frame ogni 20 ms circa, dopo 50ms sync perso
    return xQueueReceive(nowQueue, (void *)pl, 50 / portTICK_PERIOD_MS) == pdTRUE;
}


void nowInit(int channel, bool longRange,bool rxEnable)
{
    // Event loop
    esp_event_loop_create_default();
    // WiFi in modalità STA
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

    if (longRange)
    {
        esp_wifi_set_protocol(
            WIFI_IF_STA,
            WIFI_PROTOCOL_11B |
            WIFI_PROTOCOL_11G |
            WIFI_PROTOCOL_11N |
            WIFI_PROTOCOL_LR
        );
    }

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    if (rxEnable)
    {
        nowQueue = xQueueCreate(1, sizeof(t_payload));
        ESP_ERROR_CHECK(esp_now_register_recv_cb(nowRecvCb));
    }


    // Aggiungi peer broadcast
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, ESPNOW_BROADCAST_MAC, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = channel;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "READY");
}