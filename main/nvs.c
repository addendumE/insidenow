#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

static nvs_handle_t nvs_handler;

static const char *TAG="NVS";

bool nvsOpenNs(bool write)
{
    esp_err_t err = nvs_open("storage", (write) ? NVS_READWRITE:NVS_READONLY, &nvs_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore apertura NVS (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool nvsSetNodeAddress(int address)
{
    bool ret = true;
    if (nvsOpenNs(true))
    {
        esp_err_t err = nvs_set_i32(nvs_handler, "nodeAddress", address);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore scrittura (%s)", esp_err_to_name(err));
            ret = false;
        }
        err = nvs_commit(nvs_handler);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore commit (%s)", esp_err_to_name(err));
            ret = false;
        }
        nvs_close(nvs_handler);
    }
    return ret;
}

bool nvsSetChannelCount(int channel)
{
    bool ret = true;
    if (nvsOpenNs(true))
    {
        esp_err_t err = nvs_set_i32(nvs_handler, "channel", channel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore scrittura (%s)", esp_err_to_name(err));
            ret = false;
        }
        err = nvs_commit(nvs_handler);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore commit (%s)", esp_err_to_name(err));
            ret = false;
        }
        nvs_close(nvs_handler);
    }
    return ret;
}

int nvsGetNodeAddress()
{
    int32_t ret = 0;
    if (nvsOpenNs(false))
    {
        esp_err_t err = nvs_get_i32(nvs_handler, "nodeAddress", &ret);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Valore non trovato");
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore lettura (%s)", esp_err_to_name(err));
        }
        nvs_close(nvs_handler);
    }
    return ret;
}

int nvsGetChannelCount()
{
    int32_t ret = 0;
    if (nvsOpenNs(false))
    {
        esp_err_t err = nvs_get_i32(nvs_handler, "channel", &ret);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Valore non trovato");
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore lettura (%s)", esp_err_to_name(err));
        }
        nvs_close(nvs_handler);
    }
    return ret;
}
