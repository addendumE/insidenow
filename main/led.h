#include "driver/gpio.h"
#include "driver/rmt_tx.h"

void ledInit(gpio_num_t  led);
void ledPulse(size_t us);
rmt_channel_handle_t ledGetRmtChannel(void);