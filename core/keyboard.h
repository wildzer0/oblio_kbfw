#pragma once

// Glue between the matrix events and the logic behind them: layers now, tapping, report and macros as they are added.

#include "matrix.h"

void keyboard_init(void);

// One debounced key event. `now` is the time in ms (wraps at 65536).
void keyboard_event(const matrix_event_t *event, uint16_t now);

// Regularly, with the current time: lets a key held past the tapping term become a hold.
void keyboard_task(uint16_t now);

// Provided by the board: a keycode of the keyboard-specific range (QK_KB), going down or up.
void keyboard_hook_custom(uint16_t keycode, bool pressed);

// Provided by the board: a key changed or the knob turned (the keyboard is in use).
void keyboard_hook_activity(void);
