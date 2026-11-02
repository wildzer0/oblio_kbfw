#pragma once

#include <stdint.h>

// The battery level in percent, or LED_BATTERY_UNKNOWN (led.h) when there is no reading.
uint8_t battery_percent(void);
