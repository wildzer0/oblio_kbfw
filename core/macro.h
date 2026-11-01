#pragma once

// Vial macros: a macro key (M0, M1, ...) types a stored sequence. The buffer is the one Vial reads and writes
// (keymap_macro_*): macros back to back, each ended by a 0 byte. Plain characters are typed as on a US keyboard, and 0x01
// introduces an action: tap / down / up of a key, a delay, or the same with a 16-bit keycode.
//
// A macro does not stop the keyboard: macro_task() runs it one step per millisecond, so every report reaches the host
// before the next one, and a delay is a deadline, not a busy wait. Macros started while one runs wait their turn (up to
// MACRO_QUEUE of them).

#include <stdbool.h>
#include <stdint.h>

#define MACRO_QUEUE 4

void macro_init(void);

// Queue macro `id` to play. Ignored for an id that does not exist or when the queue is full.
void macro_start(uint8_t id);

// True while a macro is playing or waiting its turn.
bool macro_busy(void);

// Regularly, with the current time in ms: does the next step if the millisecond has changed since the last one.
void macro_task(uint16_t now);

// Provided by the caller: press / release a keycode (basic key, modifier, media key, key with modifiers).
void macro_hook_press(uint16_t keycode);
void macro_hook_release(uint16_t keycode);
