/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "nvs_flash.h"
#include "dmx.h"
#include "led.h"
#include "now.h"
#include "nvs.h"
#include "console.h"

static const char *TAG = "MAIN";

#define NOW_CHANNEL 6
#define MAX_NODES 10

// GATEWAY MODE CONFIGRATION
#define UART_RX_PIN GPIO_NUM_4
#define LED_CTRL_PIN GPIO_NUM_5
// NODE MODE CONFIGURATION

int nodeAddress;//if nodeAddress < 0 then gateway mode

static int consoleAddress(int argc, char **argv);
static int consoleReboot(int argc, char **argv);
static int consoleTx(int argc, char **argv);
static void gatewayTask(void *arg);
static void nodeTask(void *arg);

static const esp_console_cmd_t cmd_tx = {
	 .command = "tx",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleTx,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

static const esp_console_cmd_t cmd_address = {
	 .command = "address",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleAddress,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

static const esp_console_cmd_t cmd_reboot = {
	 .command = "reboot",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleReboot,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

int consoleTx(int argc, char **argv)
{
    if (argc == 2)
    {
        size_t len = strlen(argv[1])/2;
        if (len > MAX_PAYLOAD_SIZE) len = MAX_PAYLOAD_SIZE;
        uint8_t *data = malloc(len);
        char *pos = argv[1];
        // from hex strin to byte array
        for (size_t count = 0; count < len; count++) {
            if (sscanf(pos, "%2hhx", data+count) != 1)
            {
                ESP_LOGE(TAG,"wrong payload data");
                free(data);
                return -1;
            }
            pos += 2;
        }
        nowSend(data, len);
        free(data);
    }
    return 0;
}

int consoleAddress(int argc, char **argv)
{
    if (argc == 2) {
        int addr;
        if (sscanf(argv[1],"%d",&addr) == 1) {
            if (addr < MAX_NODES) {
                nvsSetNodeAddress(addr);
            }
            else {
                ESP_LOGE(TAG,"max address is %d",MAX_NODES);
                return -1;
            }
        }
        else {
            ESP_LOGE(TAG,"wrong address format");
            return -1;
        }
    }
    else {
        printf ("node address is: %d\n",nvsGetNodeAddress());
    }
    return 0;
}

int consoleReboot(int argc, char **argv)
{
    esp_restart();
    return 0;
}

// MAIN TASK FOR GATEWAY MODE
void  gatewayTask(void *arg)
{
    uint32_t oldCrc, frmCounter = 0, failCounter = 0;
    uint8_t dmxData[DMX_CHANNELS];
    TickType_t xLastHBtime = 0 ;

    ESP_LOGI(TAG, "MAIN GATEWAY TASK READY");

    while(1)
    {
        
        if (xTaskGetTickCount() - xLastHBtime > 1000/portTICK_PERIOD_MS) {
            // toggle HB LED
            xLastHBtime =  xTaskGetTickCount();
        }
        if (dmxGet(dmxData)) {
            
            uint32_t newCrc = esp_rom_crc32_le(0, dmxData, DMX_CHANNELS);
            if ((frmCounter % 15) == 0 || oldCrc != newCrc)
            {
                nowSend(dmxData,DMX_CHANNELS);
                ledPulse(15000);
                ESP_LOG_BUFFER_HEX_LEVEL("SEND", dmxData,DMX_CHANNELS, ESP_LOG_INFO);
                oldCrc = newCrc;
            }
            else
            {
                ledPulse(5000);
            }
            frmCounter ++;
            failCounter = 0;
        }
        else {
            failCounter++;
            frmCounter = 0;
            if (failCounter % 100 == 0) {
               ESP_LOGI(TAG, "NO SYNC");
            }
        }
    }
}

// MAIN TASK FOR NODE MODE
void nodeTask(void *arg)
{
    ESP_LOGI(TAG, "MAIN NODE TASK READY WITH ADDRESS %d",nodeAddress);
    while(1)
    {
        t_payload pl;
        if (nowReceive(&pl)) {
            ESP_LOG_BUFFER_HEX_LEVEL("RECEIVE", pl.data,pl.len, ESP_LOG_INFO);
        }
    }
}

void gatewayInit()
{
    dmxInit(UART_NUM_1, UART_RX_PIN,MAX_NODES*3);
    ledInit(LED_CTRL_PIN);
    nowInit(NOW_CHANNEL,false,false); // rx disabled 
    xTaskCreate(
        gatewayTask,
        "gatewayTask",
        4096,
        NULL,
        10,
        NULL
    );
}

void nodeInit()
{
    nowInit(NOW_CHANNEL,false,true); // rx enabled
    xTaskCreate(
        nodeTask,
        "nodeTask",
        4096,
        NULL,
        10,
        NULL
    );

}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    consoleInit();
    consoleRegister(&cmd_tx);
    consoleRegister(&cmd_reboot);
    consoleRegister(&cmd_address);
    nodeAddress = nvsGetNodeAddress();
    if ( nodeAddress < 0) {
        gatewayInit();
        ESP_LOGI(TAG, "GATEWAY READY TO GO");
    }
    else {
        nodeInit();
        ESP_LOGI(TAG, "NODE READY TO GO ON ADDRESS: %d",nodeAddress);
    }
}
