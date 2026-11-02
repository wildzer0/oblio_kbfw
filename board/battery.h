#pragma once

#include <stdbool.h>
#include <stdint.h>

// The battery level in percent, or LED_BATTERY_UNKNOWN (led.h) when there is no reading.
uint8_t battery_percent(void);

// The battery key was pressed: true if it was the second press within a moment, which switches the live readout (the keyboard types the raw
// reading once a second) on or off - and then the gauge is not shown.
bool battery_key(uint16_t now_ms);
void battery_log_task(uint16_t now_ms); // from the main loop
