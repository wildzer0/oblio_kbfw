#pragma once

// Glue between the matrix events and the logic behind them: layers now, tapping, report and macros as they are added.

#include "matrix.h"

void keyboard_init(void);

// One debounced key event. `now` is the time in ms (wraps at 65536).
void keyboard_event(const matrix_event_t *event, uint16_t now);
