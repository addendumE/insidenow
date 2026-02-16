#include "driver/gpio.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ws2812_bitbang.h"


#include "esp_log.h"
#define WS2812_DELAY_US 2
static const char *TAG = "WS2812_BB";

static gpio_num_t ws_pin = -1;
static int ws_num_leds = 1;


static void ws2812_delay_short() {
    for (volatile int i = 0; i < 60; i++) {
        __asm__ __volatile__("nop");
    }
}
static void ws2812_delay_long() {
    for (volatile int i = 0; i < 180; i++) {
        __asm__ __volatile__("nop");
    }
}

void ws2812BitbangInit(gpio_num_t pin, int num_leds) {
    ws_pin = pin;
    ws_num_leds = num_leds > 0 ? num_leds : 1;
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << ws_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(ws_pin, 0);
}

static void ws2812_send_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            // '1' bit: T0H ~0.8us, T0L ~0.45us
            gpio_set_level(ws_pin, 1);
            ws2812_delay_long();
            gpio_set_level(ws_pin, 0);
            ws2812_delay_short();
        } else {
            // '0' bit: T1H ~0.4us, T1L ~0.85us
            gpio_set_level(ws_pin, 1);
            ws2812_delay_short();
            gpio_set_level(ws_pin, 0);
            ws2812_delay_long();
        }
    }
}

void ws2812BitbangSetAllRGB(uint8_t r, uint8_t g, uint8_t b) {
    ESP_LOGI(TAG, "ws2812BitbangSetAllRGB: R=%d G=%d B=%d", r, g, b);
    for (int i = 0; i < ws_num_leds; i++) {
        ws2812_send_byte(g);
        ws2812_send_byte(r);
        ws2812_send_byte(b);
    }
    // reset: low for >50us
    for (volatile int i = 0; i < 6000; i++) {
        gpio_set_level(ws_pin, 0);
        __asm__ __volatile__("nop");
    }
    ESP_LOGI(TAG, "ws2812BitbangSetAllRGB: done");
}
