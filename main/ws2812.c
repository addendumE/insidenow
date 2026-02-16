#include "ws2812.h"
#include "led.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "WS2812";

static rmt_channel_handle_t rmt_chan = NULL;
static rmt_encoder_handle_t copy_encoder = NULL;
static rmt_transmit_config_t tx_config = {
    .loop_count = 0,
};
static int ws_num_leds = WS2812_NUM_LEDS;

void ws2812Init(gpio_num_t pin, int num_leds)
{
    if (rmt_chan) return;
    ws_num_leds = num_leds > 0 ? num_leds : WS2812_NUM_LEDS;

    // Try to reuse RMT channel from led.c if already initialized
    rmt_chan = ledGetRmtChannel();
    if (rmt_chan) {
        ESP_LOGI(TAG, "Reusing RMT channel from led");
        return;
    }

    // If not available, allocate a new one
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = pin,
        .mem_block_symbols = 1024, // large enough for typical strips
        .resolution_hz = 10 * 1000 * 1000, // 0.1 us tick
        .trans_queue_depth = 1,
    };
    esp_err_t r = rmt_new_tx_channel(&tx_chan_config, &rmt_chan);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel failed: %d", r);
        rmt_chan = NULL;
        return;
    }
    rmt_copy_encoder_config_t enc_cfg = {};
    r = rmt_new_copy_encoder(&enc_cfg, &copy_encoder);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_copy_encoder failed: %d", r);
        rmt_del_channel(rmt_chan);
        rmt_chan = NULL;
        return;
    }
    rmt_enable(rmt_chan);
}

// build symbol array and transmit
void ws2812ShowRGB(const uint8_t *rgb, size_t len_bytes)
{
    if (!rmt_chan || !copy_encoder || !rgb) return;
    size_t pixels = len_bytes / 3;
    if (pixels == 0) return;
    // each pixel 24 bits -> 24 symbols, plus one reset symbol
    size_t sym_count = pixels * 24 + 1;
    rmt_symbol_word_t *symbols = malloc(sizeof(rmt_symbol_word_t) * sym_count);
    if (!symbols) return;
    // timings in ticks (0.1us)
    const uint16_t T0H = 4; // 0.4us
    const uint16_t T0L = 9; // 0.9us
    const uint16_t T1H = 8; // 0.8us
    const uint16_t T1L = 5; // 0.5us

    size_t si = 0;
    for (size_t p = 0; p < pixels; p++) {
        // propagate in RGB order as received
        uint8_t r = rgb[p*3 + 0];
        uint8_t g = rgb[p*3 + 1];
        uint8_t b = rgb[p*3 + 2];
        uint8_t px[3] = { r, g, b };
        for (int c = 0; c < 3; c++) {
            uint8_t val = px[c];
            for (int bit = 7; bit >= 0; bit--) {
                bool one = (val >> bit) & 1;
                if (one) {
                    symbols[si].level0 = 1;
                    symbols[si].duration0 = T1H;
                    symbols[si].level1 = 0;
                    symbols[si].duration1 = T1L;
                } else {
                    symbols[si].level0 = 1;
                    symbols[si].duration0 = T0H;
                    symbols[si].level1 = 0;
                    symbols[si].duration1 = T0L;
                }
                si++;
            }
        }
    }
    // reset symbol: low for >50us (500 ticks)
    symbols[si].level0 = 0;
    symbols[si].duration0 = 600; // 60us
    symbols[si].level1 = 0;
    symbols[si].duration1 = 0;
    si++;

    esp_err_t r = rmt_transmit(rmt_chan, copy_encoder, symbols, si * sizeof(rmt_symbol_word_t), &tx_config);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %d", r);
        free(symbols);
        return;
    }
    // wait for completion then free
    rmt_tx_wait_all_done(rmt_chan, 1000);
    free(symbols);
}

void ws2812SetAllRGB(uint8_t r, uint8_t g, uint8_t b)
{
    size_t len = ws_num_leds * 3;
    uint8_t *buf = malloc(len);
    if (!buf) return;
    for (int i = 0; i < ws_num_leds; i++) {
        buf[i*3 + 0] = r;
        buf[i*3 + 1] = g;
        buf[i*3 + 2] = b;
    }
    ws2812ShowRGB(buf, len);
    free(buf);
}
