#include "check.h"
#include "matrix.h"
#include <string.h>

static uint8_t raw[MATRIX_COLS];

static void scans(unsigned count)
{
    while (count--) {
        matrix_feed(raw);
    }
}

static void reset(void)
{
    memset(raw, 0, sizeof raw);
    matrix_init();
}

static unsigned drain(matrix_event_t *out, unsigned max)
{
    unsigned n = 0;
    matrix_event_t ev;

    while (matrix_event_pop(&ev)) {
        if (n < max) {
            out[n] = ev;
        }
        n++;
    }

    return n;
}

static void test_press_and_release_after_debounce(void)
{
    matrix_event_t ev[4];

    reset();
    raw[3] = 1u << 2; // c3r2

    scans(MATRIX_DEBOUNCE_SCANS - 1);
    CHECK(drain(ev, 4) == 0);
    CHECK(!matrix_is_pressed(3, 2));

    scans(1);
    CHECK(drain(ev, 4) == 1);
    CHECK(ev[0].col == 3 && ev[0].row == 2 && ev[0].pressed);
    CHECK(matrix_is_pressed(3, 2));

    scans(20); // held: no repeats
    CHECK(drain(ev, 4) == 0);

    raw[3] = 0;
    scans(MATRIX_DEBOUNCE_SCANS);
    CHECK(drain(ev, 4) == 1);
    CHECK(ev[0].col == 3 && ev[0].row == 2 && !ev[0].pressed);
    CHECK(!matrix_is_pressed(3, 2));
}

static void test_bounce_is_ignored(void)
{
    matrix_event_t ev[4];

    reset();

    for (unsigned i = 0; i < 20; i++) { // never steady for the whole window
        raw[0] = (i & 1u) ? 1u : 0u;
        scans(MATRIX_DEBOUNCE_SCANS - 1);
    }

    CHECK(drain(ev, 4) == 0);
}

static void test_glitch_during_press_restarts_the_window(void)
{
    matrix_event_t ev[4];

    reset();
    raw[1] = 1;
    scans(MATRIX_DEBOUNCE_SCANS - 1);
    raw[1] = 0; // released one scan short of being accepted
    scans(1);
    raw[1] = 1;
    scans(MATRIX_DEBOUNCE_SCANS - 1);
    CHECK(drain(ev, 4) == 0);

    scans(1);
    CHECK(drain(ev, 4) == 1);
}

static void test_two_keys_in_one_column_and_order(void)
{
    matrix_event_t ev[8];

    reset();
    raw[5] = (1u << 0) | (1u << 4);
    scans(MATRIX_DEBOUNCE_SCANS);

    CHECK(drain(ev, 8) == 2);
    CHECK(ev[0].row == 0 && ev[1].row == 4); // in row order
    CHECK(ev[0].pressed && ev[1].pressed);
}

static void test_columns_debounce_independently(void)
{
    matrix_event_t ev[8];

    reset();
    raw[0] = 1;
    scans(2);
    raw[15] = 1; // pressed later
    scans(MATRIX_DEBOUNCE_SCANS - 2);

    CHECK(drain(ev, 8) == 1);
    CHECK(ev[0].col == 0);

    scans(2);
    CHECK(drain(ev, 8) == 1);
    CHECK(ev[0].col == 15);
}

static void test_bits_above_the_last_row_are_ignored(void)
{
    matrix_event_t ev[4];

    reset();
    raw[7] = 0xE0;
    scans(MATRIX_DEBOUNCE_SCANS * 2);
    CHECK(drain(ev, 4) == 0);
}

static void test_full_queue_delays_but_never_drops(void)
{
    matrix_event_t ev[64];
    unsigned       total = 0;

    reset();

    for (uint8_t col = 0; col < MATRIX_COLS; col++) { // 16 columns x 2 rows = 32 presses, more than the queue holds
        raw[col] = 0x03;
    }

    scans(MATRIX_DEBOUNCE_SCANS);

    for (unsigned guard = 0; guard < 10; guard++) {
        total += drain(ev + total, 64 - total);
        scans(1);
    }

    CHECK(total == 32);

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        CHECK(matrix_is_pressed(col, 0) && matrix_is_pressed(col, 1));
    }
}

int main(void)
{
    test_press_and_release_after_debounce();
    test_bounce_is_ignored();
    test_glitch_during_press_restarts_the_window();
    test_two_keys_in_one_column_and_order();
    test_columns_debounce_independently();
    test_bits_above_the_last_row_are_ignored();
    test_full_queue_delays_but_never_drops();
    return tests_result();
}
