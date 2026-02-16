// Simple WS2812 driver using RMT copy encoder
#pragma once
#include "driver/gpio.h"
#include <stdint.h>
#include <stddef.h>

#ifndef WS2812_NUM_LEDS
#define WS2812_NUM_LEDS 1
#endif

void ws2812Init(gpio_num_t pin, int num_leds);
void ws2812SetAllRGB(uint8_t r, uint8_t g, uint8_t b);
void ws2812ShowRGB(const uint8_t *rgb, size_t len_bytes);
