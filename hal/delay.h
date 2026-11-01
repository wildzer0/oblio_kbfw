#pragma once

#include <stdint.h>

// Busy-wait delays, calibrated for the 24 MHz clock (see delay.c). Not exact: for settling times, not for timing signals.
void delay_us(uint8_t us);
void delay_ms(uint16_t ms);
