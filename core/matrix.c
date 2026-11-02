#include "matrix.h"

#define QUEUE_SIZE 16u // power of two

static uint8_t  stable[MATRIX_COLS];    // what the events have reported
static uint8_t  candidate[MATRIX_COLS]; // last raw sample
static uint16_t changed_at[MATRIX_COLS]; // when the candidate last moved, in milliseconds
static uint8_t  samples[MATRIX_COLS];    // scans that have agreed on it since, saturating

static matrix_event_t queue[QUEUE_SIZE];
static uint8_t        head;
static uint8_t        tail;

void matrix_init(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        stable[col]     = 0;
        candidate[col]  = 0;
        changed_at[col] = 0u;
        samples[col]    = MATRIX_DEBOUNCE_SAMPLES;
    }

    head = 0;
    tail = 0;
}

static bool queue_push(uint8_t col, uint8_t row, bool pressed)
{
    const uint8_t next = (uint8_t)((head + 1u) & (QUEUE_SIZE - 1u));

    if (next == tail) {
        return false;
    }

    queue[head].col     = col;
    queue[head].row     = row;
    queue[head].pressed = pressed;
    head                = next;
    return true;
}

void matrix_feed(const uint8_t raw[MATRIX_COLS], uint16_t now)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        const uint8_t sample = raw[col] & MATRIX_ROW_MASK;

        if (sample != candidate[col]) { // a new value: the window starts over
            candidate[col]  = sample;
            changed_at[col] = now;
            samples[col]    = 1;
        } else if (samples[col] < MATRIX_DEBOUNCE_SAMPLES) {
            samples[col]++;
        }

        if (samples[col] < MATRIX_DEBOUNCE_SAMPLES && (uint16_t)(now - changed_at[col]) < MATRIX_DEBOUNCE_MS) {
            continue; // neither long enough nor often enough yet
        }

        const uint8_t changed = (uint8_t)(candidate[col] ^ stable[col]);

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            const uint8_t bit = (uint8_t)(1u << row);

            if (!(changed & bit)) {
                continue;
            }

            // The stable bit flips only once the event is queued, so a full queue delays a key, never drops it.
            if (queue_push(col, row, (sample & bit) != 0)) {
                stable[col] ^= bit;
            }
        }
    }
}

bool matrix_event_pop(matrix_event_t *event)
{
    if (head == tail) {
        return false;
    }

    *event = queue[tail];
    tail   = (uint8_t)((tail + 1u) & (QUEUE_SIZE - 1u));
    return true;
}

bool matrix_is_pressed(uint8_t col, uint8_t row)
{
    return (stable[col] & (1u << row)) != 0;
}

uint16_t matrix_row_bits(uint8_t row)
{
    uint16_t bits = 0;

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (stable[col] & (1u << row)) {
            bits |= (uint16_t)(1u << col);
        }
    }

    return bits;
}

bool matrix_any_pressed(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        if (stable[col]) {
            return true;
        }
    }

    return false;
}
