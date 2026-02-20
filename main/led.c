#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_timer.h"

#define RMT_CHANNEL   RMT_CHANNEL_0

static rmt_channel_handle_t rmt_chan = NULL;
static rmt_encoder_handle_t encoder = NULL;

 static rmt_symbol_word_t symbol = {
        .level0 = 1,
        .duration0 = 10000,   // 10.000 µs
        .level1 = 0,
        .duration1 = 1
 };

 static rmt_transmit_config_t tx_config = {
    .loop_count = 0       // one-shot
 };


void ledInit(gpio_num_t  led)
{
    static bool init = false;
    if (init) return;
    init = true;

    // 1️⃣ Canale TX
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,   // 80 MHz
        .gpio_num = led,
        .mem_block_symbols = 64,
        .resolution_hz = 1 * 1000 * 1000, // 1 MHz → 1 tick = 1 µs
        .trans_queue_depth = 1,
    };

    rmt_new_tx_channel(&tx_chan_config, &rmt_chan);

    // 2️⃣ Encoder "raw" (copiamo direttamente i simboli)
    rmt_copy_encoder_config_t encoder_config = {};
    rmt_new_copy_encoder(&encoder_config, &encoder);

    // 3️⃣ Abilita il canale
    rmt_enable(rmt_chan);
}


void ledPulse(size_t us)
{
    symbol.duration0 = us;
    rmt_transmit(rmt_chan, encoder, &symbol, sizeof(symbol), &tx_config);
}

rmt_channel_handle_t ledGetRmtChannel(void)
{
    return rmt_chan;
}