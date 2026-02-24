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
#include "now.h"
#include "nvs.h"
#include "console.h"
#include "led_strip.h"

static const char *TAG = "MAIN";

#define NOW_CHANNEL 6
#define MAX_NODES 10
#define NUM_LEDS 12

// GATEWAY MODE CONFIGRATION
#define UART_RX_PIN GPIO_NUM_9
#define LED_STS_PIN GPIO_NUM_8
#define LED_CTRL_PIN GPIO_NUM_8
#define LED_POWER_PIN GPIO_NUM_3
// NODE MODE CONFIGURATION

int nodeAddress;//if nodeAddress < 0 then gateway mode
static led_strip_handle_t pStrip_a;

static int consoleChannel(int argc, char **argv);
static int consoleAddress(int argc, char **argv);
static int consoleReboot(int argc, char **argv);
static int consoleTx(int argc, char **argv);
static int consoleLed(int argc, char **argv);
static void gatewayTask(void *arg);
static void nodeTask(void *arg);
static void led_strip_set_all_rgb(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);



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

static const esp_console_cmd_t cmd_led = {
     .command = "led",
     .help = NULL,
     .hint = NULL,
     .func = &consoleLed,
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

static const esp_console_cmd_t cmd_channel = {
	 .command = "channel",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleChannel,
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

int consoleChannel(int argc, char **argv)
{
    if (argc == 2) {
        int channels;
        if (sscanf(argv[1],"%d",&channels) == 1) {
            if (channels <= 250 && channels >= 1) {
                nvsSetChannelCount(channels);
            }
            else {
                ESP_LOGE(TAG,"channel range is [1-250]");
                return -1;
            }
        }
        else {
            ESP_LOGE(TAG,"wrong channel format");
            return -1;
        }
    }
    else {
        printf ("channel count is: %d\n",nvsGetChannelCount());
    }
    return 0;
}

int consoleReboot(int argc, char **argv)
{
    esp_restart();
    return 0;
}

int consoleLed(int argc, char **argv)
{
    if (argc == 4) {
        int r,g,b;
        if (sscanf(argv[1],"%d",&r) == 1 && sscanf(argv[2],"%d",&g) == 1 && sscanf(argv[3],"%d",&b) == 1) {
            led_strip_set_all_rgb(r,g,b,0,0,0);
        }
        else {
            ESP_LOGE(TAG,"wrong color format");
            return -1;
        }
    }
    else {
        ESP_LOGE(TAG,"usage: led R G B");
        return -1;
    }
    return 0;
}
void led_toggle()
{
    static bool level = false;
    gpio_set_level(LED_STS_PIN, level);    
    level = !level;
}

// MAIN TASK FOR GATEWAY MODE
void  gatewayTask(void *arg)
{
    size_t channels = nvsGetChannelCount();
    uint32_t oldCrc, frmCounter = 0, failCounter = 0;
    uint8_t dmxData[channels];
    TickType_t xLastHBtime = 0 ;

    ESP_LOGI(TAG, "MAIN GATEWAY TASK READY");

    while(1)
    {
        
        if (xTaskGetTickCount() - xLastHBtime > 1000/portTICK_PERIOD_MS) {
            // toggle HB LED
            xLastHBtime =  xTaskGetTickCount();
        }
        if (dmxGet(dmxData)) {
            led_toggle();
            uint32_t newCrc = esp_rom_crc32_le(0, dmxData, channels);
            if ((frmCounter % 15) == 0 || oldCrc != newCrc)
            {
                nowSend(dmxData,channels);
                oldCrc = newCrc;
            }
            frmCounter ++;
            failCounter = 0;
        }
        else {
            failCounter++;
            frmCounter = 0;
            if (failCounter % 25 == 0) {
               led_toggle();
            }
        }
    }
}

void led_strip_set_all_rgb(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2)
{
    gpio_set_level(LED_POWER_PIN, 1);
    ESP_LOGI(TAG, "Setting all LEDs to R:%d G:%d B:%d + R:%d G:%d B:%d", r1, g1, b1, r2, g2, b2);
    for (int i = 0; i < NUM_LEDS/2; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(pStrip_a, i, r1, g1, b1));
    }
    for (int i = NUM_LEDS/2; i < NUM_LEDS; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(pStrip_a, i, r2, g2, b2));
    }
    ESP_ERROR_CHECK(led_strip_refresh(pStrip_a));
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
            // interpret payload: 3 bytes per node; take our node's 3 bytes and apply to whole strip
            if (nodeAddress >= 0) {
                size_t idx = nodeAddress * 3;
                if (pl.len >= idx + 6) {
                    uint8_t r1 = pl.data[idx + 0];
                    uint8_t g1 = pl.data[idx + 1];
                    uint8_t b1 = pl.data[idx + 2];
                    uint8_t r2 = pl.data[idx + 3];
                    uint8_t g2 = pl.data[idx + 4];
                    uint8_t b2 = pl.data[idx + 5];
                    ESP_LOGI(TAG, "Applying color R:%d G:%d B:%d + R:%d G:%d B:%d", r1, g1, b1, r2, g2, b2);
                    led_strip_set_all_rgb(r1, g1, b1, r2, g2, b2);
                }
                else {
                    ESP_LOGW(TAG, "payload too short for node %d (len=%d)", nodeAddress, pl.len);
                }
            }
        }
    }
}

void gatewayInit()
{
    dmxInit(UART_NUM_1, UART_RX_PIN,nvsGetChannelCount());
    gpio_reset_pin(LED_STS_PIN);
    gpio_set_direction(LED_STS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STS_PIN, true);    
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
    
    // Inizializza e accendi il pin di alimentazione LED
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_POWER_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(LED_POWER_PIN, 1);
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_CTRL_PIN,
        .max_leds = NUM_LEDS, 
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &pStrip_a));
    
    //led_strip_set_all_rgb(0, 0, 255);
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
        consoleRegister(&cmd_channel);
        gatewayInit();
        ESP_LOGI(TAG, "GATEWAY READY TO GO");
    }
    else {
        consoleRegister(&cmd_led);
        nodeInit();
        ESP_LOGI(TAG, "NODE READY TO GO ON ADDRESS: %d",nodeAddress);
    }
}
