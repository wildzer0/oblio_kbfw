#pragma once

// Matrix debounce and key events. Pure logic: no registers, so it runs under the host tests.
//
// The board scans every column and hands the raw samples to matrix_feed(); a key change is accepted once that column has read the same value
// for MATRIX_DEBOUNCE_MS milliseconds, then queued as an event.

#include <stdbool.h>
#include <stdint.h>

#define MATRIX_ROWS 5
#define MATRIX_COLS 16

// How long a column must hold a new value before it counts. It is a time and not a number of scans, as in QMK: the two knobs then move
// independently - the scan rate is free to follow the backlight's needs (core/sched.h) without the debounce paying for it. QMK's own default
// is 5 ms, and its choice of algorithm (sym_defer_pk) waits the same way.
#define MATRIX_DEBOUNCE_MS 5u

// A floor on how many scans must agree before a change counts. The time above is the rule; this only matters where the clock cannot be
// trusted to move (the simulator drives Timer 2 from a virtual flag), and at the real scan period of ~4 ms the time always comes first.
#define MATRIX_DEBOUNCE_SAMPLES 3u

#define MATRIX_ROW_MASK ((uint8_t)((1u << MATRIX_ROWS) - 1u))

typedef struct {
    uint8_t col;
    uint8_t row;
    bool    pressed;
} matrix_event_t;

void matrix_init(void);

// raw[col] has bit `row` set for a pressed key; bits above the last row are ignored.
// A fresh scan of the whole matrix, with the time it was taken: `now` is what the debounce is measured against.
void matrix_feed(const uint8_t raw[MATRIX_COLS], uint16_t now);

// Takes the oldest event. False when there is none.
bool matrix_event_pop(matrix_event_t *event);

// Debounced state of one key, as the events have reported it so far.
bool matrix_is_pressed(uint8_t col, uint8_t row);

// The debounced state of a whole row: bit `col` is set for a pressed key (the matrix tester of Vial shows it).
uint16_t matrix_row_bits(uint8_t row);

// True while any key is down, as the events have reported it.
bool matrix_any_pressed(void);

// Times a change was held back because the event queue was full. The change is not lost: it is accepted on a
// later scan, once the queue has room.
