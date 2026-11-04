#pragma once

#include <stdint.h>

// Reads the two charger lines (core/charger.h) and feeds the debounce. From the main loop.
void charger_hw_sample(uint16_t now_ms);

// The two lines as they are right now, for the diagnostics: bit 0 = P4.4 is high, bit 1 = P7.7 is high.
uint8_t charger_hw_pins(void);
