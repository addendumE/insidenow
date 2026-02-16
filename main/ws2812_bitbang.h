#pragma once
#include "driver/gpio.h"
#include <stdint.h>

void ws2812BitbangInit(gpio_num_t pin, int num_leds);
void ws2812BitbangSetAllRGB(uint8_t r, uint8_t g, uint8_t b);
