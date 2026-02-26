#include "driver/uart.h"
#include "driver/gpio.h"
#include "hal/uart_hal.h"


#define DMX_DATA_SIZE 512

void dmxInit(uart_port_t _uartId ,gpio_num_t _rxPin);
extern bool dmxGet( uint8_t * dest);