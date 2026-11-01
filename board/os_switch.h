#pragma once

#include <stdbool.h>
#include <stdint.h>

// The Win/Mac switch of the B68. It is not in the key matrix: it is P0.5, an input with a pull-up that the stock firmware samples all the
// time, debounces with two counters (ten equal readings in a row) and keeps in XDATA[0x0307] (function at 0xA9D9 of the dump). Every
// keymap lookup of the stock then goes through that state, so the switch picks which of its two sets of keymap tables is the base: the
// same idea as here, where it picks the default layer (KEYMAP_MAC_LAYER, a copy of the base with Alt and GUI swapped).
//
// Measured on the board (diag kind 3, 2026-11-01): P0.5 reads high with the switch on Windows and low with it on Mac, and the level does
// not depend on the column strobes (it is not wired into the matrix).

#define OS_SWITCH_STABLE_MS 50u // the switch must read the same this long before the layer changes, as the mode selector

void os_switch_init(void);        // reads the switch once and sets the default layer; after the profile is loaded
void os_switch_task(uint16_t now); // regularly from the main loop: debounce, and change the default layer when it settles
void os_switch_apply(void);       // set the default layer from the state known now (a profile load resets the layers)

bool os_switch_mac(void); // the state in use: true = Mac
