#pragma once

#include <stdint.h>

// The battery level in percent, or LED_BATTERY_UNKNOWN (led.h) when there is no reading.
uint8_t battery_percent(void);

// The raw reading of the cell when it means something (settled, from the radio), else 0. The battery log writes this down.
uint16_t battery_reading(void);

// From the main loop: warns when the battery is low and sleeps when it is critical (on battery only).
void battery_task(uint16_t now_ms);
