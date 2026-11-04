#pragma once

#include <stdbool.h>
#include <stdint.h>

// The battery level in percent, or LED_BATTERY_UNKNOWN (led.h) when there is no reading.
uint8_t battery_percent(void);

// The raw reading of the cell when it means something (settled, from the radio), else 0. The battery log writes this down.
uint16_t battery_reading(void);

// Is the keyboard powered from outside (the cable is in)? The radio's reading says it when it saturates; a reading of a cell says it is not; with no
// reading (USB mode, the radio not answering yet) a host that has configured the keyboard or the charger's cable line do. See core/charger.h.
bool battery_cable(void);

// The gauge (Fn+Backspace): the level on battery; with the cable, whether the cell is charging or the charge is done (the level cannot be read then).
void battery_show(void);

// From the main loop: warns when the battery is low and sleeps when it is critical (on battery only).
void battery_task(uint16_t now_ms);
