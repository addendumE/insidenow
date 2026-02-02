#include "driver/uart.h"
#include "driver/gpio.h"
#include "hal/uart_hal.h"

#define DMX_CHANNELS 30


void dmxInit(uart_port_t _uartId ,gpio_num_t _rxPin, size_t _maxChannels);
extern bool dmxGet( uint8_t * dest);
extern bool dmxSet( uint8_t * dest);
