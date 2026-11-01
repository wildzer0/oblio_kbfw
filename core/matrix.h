#pragma once

// Matrix debounce and key events. Pure logic: no registers, so it runs under the host tests.
//
// The board scans every column and hands the raw samples to matrix_feed(); a key change is accepted once the
// column has read the same for MATRIX_DEBOUNCE_SCANS scans in a row, then queued as an event.

#include <stdbool.h>
#include <stdint.h>

#define MATRIX_ROWS 5
#define MATRIX_COLS 16

// Scans a column must hold a new value before it counts. A scan comes every ~2 ms (core/sched.h), so this is ~6 ms.
#define MATRIX_DEBOUNCE_SCANS 3

#define MATRIX_ROW_MASK ((uint8_t)((1u << MATRIX_ROWS) - 1u))

typedef struct {
    uint8_t col;
    uint8_t row;
    bool    pressed;
} matrix_event_t;

void matrix_init(void);

// raw[col] has bit `row` set for a pressed key; bits above the last row are ignored.
void matrix_feed(const uint8_t raw[MATRIX_COLS]);

// Takes the oldest event. False when there is none.
bool matrix_event_pop(matrix_event_t *event);

// Debounced state of one key, as the events have reported it so far.
bool matrix_is_pressed(uint8_t col, uint8_t row);

// True while any key is down, as the events have reported it.
bool matrix_any_pressed(void);

// Times a change was held back because the event queue was full. The change is not lost: it is accepted on a
// later scan, once the queue has room.
uint8_t matrix_deferred(void);
