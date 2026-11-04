#pragma once

#include <stdbool.h>
#include <stdint.h>

// Puts the keyboard to sleep when core/sleep.h says so: only on battery (a wireless mode, no cable seen by the PC). From the main
// loop, with the time in ms.
// `any_key_down`: the main loop has asked the matrix already
void sleep_hw_task(uint16_t now, bool any_key_down);

// Applies a sleep level (settings.sleep_level) to the counting; with `show`, keys 1-5 flash the step.
void sleep_hw_apply_level(uint8_t level, bool show);
