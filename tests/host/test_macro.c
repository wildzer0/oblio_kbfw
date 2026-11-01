#include "check.h"
#include "stubs.h"
#include "keycodes.h"
#include "keymap.h"
#include "macro.h"
#include <string.h>

uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    (void)layer;
    (void)row;
    (void)col;
    return KC_NO;
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)layer;
    (void)encoder;
    (void)clockwise;
    return KC_NO;
}

uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

static char     log_text[4096];
static uint16_t log_time[1024];
static unsigned log_count;
static uint16_t now;

static void note(char kind, uint16_t keycode)
{
    char text[16];

    snprintf(text, sizeof text, "%s%c%X", log_text[0] ? " " : "", kind, keycode);
    strcat(log_text, text);
    log_time[log_count++] = now;
}

void macro_hook_press(uint16_t keycode) { note('P', keycode); }
void macro_hook_release(uint16_t keycode) { note('R', keycode); }

static void reset(void)
{
    keymap_init();
    keymap_macro_clear();
    macro_init();
    log_text[0] = 0;
    log_count   = 0;
    now         = 0;
}

// Sets the buffer: macros back to back, each with its 0 byte (the last one too)
static void store(const char *bytes, unsigned size)
{
    keymap_macro_write(0, 0, 0);

    for (unsigned at = 0; at < size; at += 32) {
        keymap_macro_write((uint16_t)at, (uint8_t)(size - at < 32 ? size - at : 32), (const uint8_t *)bytes + at);
    }
}

#define STORE(literal) store(literal, sizeof(literal)) // sizeof keeps the final 0

// Runs the macro engine one ms at a time until it is idle; the time it took
static uint16_t run(void)
{
    for (now = 1; now < 5000 && macro_busy(); now++) {
        macro_task(now);
    }

    return now;
}

#define CHECK_LOG(expected)                                                          \
    do {                                                                             \
        if (strcmp(log_text, expected) != 0) {                                       \
            printf("  FAIL %s:%d: log \"%s\", expected \"%s\"\n", __FILE__, __LINE__, log_text, expected); \
            checks_failed++;                                                         \
        }                                                                            \
    } while (0)

static void test_text(void)
{
    reset();
    STORE("ab\0");
    macro_start(0);
    run();
    CHECK_LOG("P4 R4 P5 R5");

    reset();
    STORE("A1!\0"); // shifted letter, digit, shifted digit
    macro_start(0);
    run();
    CHECK_LOG("PE1 P4 R4 RE1 P1E R1E PE1 P1E R1E RE1");

    reset();
    STORE("a b\n\0");
    macro_start(0);
    run();
    CHECK_LOG("P4 R4 P2C R2C P5 R5 P28 R28");
}

static void test_every_printable_character_has_a_key(void)
{
    char all[96];

    for (unsigned i = 0; i < 95; i++) {
        all[i] = (char)(0x20 + i);
    }
    all[95] = 0;

    reset();
    store(all, 96);
    macro_start(0);
    run();

    unsigned presses = 0;

    for (const char *p = log_text; *p; p++) {
        presses += (*p == 'P');
    }

    CHECK(presses >= 95); // one key press per character, plus the shifts
}

static void test_actions(void)
{
    reset();
    STORE("\x01\x01\x04" "\x01\x02\xE0" "\x01\x03\xE0" "\0"); // tap A, shift... : tap 0x04, down LCTL, up LCTL
    macro_start(0);
    run();
    CHECK_LOG("P4 R4 PE0 RE0");

    reset();
    STORE("\x01\x05\x04\x02\0"); // 16-bit tap of 0x0204 = LSFT(KC_A)
    macro_start(0);
    run();
    CHECK_LOG("P204 R204");

    reset();
    STORE("\x01\x06\x04\x02\x01\x07\x04\x02\0"); // 16-bit down, up
    macro_start(0);
    run();
    CHECK_LOG("P204 R204");
}

static void test_zero_byte_keycodes_are_stored_shifted(void)
{
    reset();
    STORE("\x01\x05\x02\xFF\0"); // 0xFF02 stands for 0x0200 (a low byte of 0 cannot be stored)
    macro_start(0);
    run();
    CHECK_LOG("P200 R200");
}

static void test_delay_is_a_deadline(void)
{
    reset();
    STORE("a\x01\x04\x65\x01" "b\0"); // delay: (0x65-1) + (1-1)*255 = 100 ms
    macro_start(0);
    run();
    CHECK_LOG("P4 R4 P5 R5");
    CHECK(log_time[2] - log_time[1] >= 100); // b comes 100 ms or more after a
    CHECK(log_time[2] - log_time[1] < 110);

    reset();
    STORE("\x01\x04\x01\x03" "a\0"); // (1-1) + (3-1)*255 = 510 ms
    macro_start(0);
    run();
    CHECK(log_time[0] >= 510 && log_time[0] < 520);
}

static void test_one_step_per_millisecond(void)
{
    reset();
    STORE("ab\0");
    macro_start(0);

    for (now = 1; now < 20; now++) {
        macro_task(now);
        macro_task(now); // the same millisecond again: nothing
    }

    CHECK_LOG("P4 R4 P5 R5");

    for (unsigned i = 1; i < log_count; i++) {
        CHECK(log_time[i] != log_time[i - 1]);
    }
}

static void test_macro_selection(void)
{
    reset();
    STORE("a\0b\0c\0");
    macro_start(1);
    run();
    CHECK_LOG("P5 R5");

    reset();
    STORE("a\0b\0c\0");
    macro_start(2);
    run();
    CHECK_LOG("P6 R6");

    reset();
    STORE("a\0");
    macro_start(3); // no such macro: the buffer is only zeros past the first
    run();
    CHECK_LOG("");

    macro_start(KEYMAP_MACRO_COUNT); // no such id
    CHECK(!macro_busy());
}

static void test_macros_wait_their_turn(void)
{
    reset();
    STORE("a\0b\0");
    macro_start(0);
    macro_start(1);
    macro_start(0);
    run();
    CHECK_LOG("P4 R4 P5 R5 P4 R4");

    reset();
    STORE("a\0");

    for (unsigned i = 0; i < MACRO_QUEUE + 3; i++) { // more than the queue holds: the extra ones are dropped
        macro_start(0);
    }
    run();

    unsigned plays = 0;

    for (const char *p = log_text; *p; p++) {
        plays += (*p == 'P');
    }

    CHECK(plays == MACRO_QUEUE);
}

static void test_damaged_buffer_plays_nothing(void)
{
    reset();
    STORE("a\0");
    uint8_t not_zero = 'x';
    keymap_macro_write(KEYMAP_MACRO_BYTES - 1, 1, &not_zero); // the last byte of the buffer must be 0
    macro_start(0);
    run();
    CHECK_LOG("");
}

static void test_truncated_actions_end_the_macro(void)
{
    reset();
    STORE("a\x01\0"); // a prefix with nothing after it
    macro_start(0);
    run();
    CHECK_LOG("P4 R4");
    CHECK(!macro_busy());

    reset();
    STORE("\x01\x04\x05\0"); // a delay with one byte only
    macro_start(0);
    run();
    CHECK_LOG("");
    CHECK(!macro_busy());
}

int main(void)
{
    test_text();
    test_every_printable_character_has_a_key();
    test_actions();
    test_zero_byte_keycodes_are_stored_shifted();
    test_delay_is_a_deadline();
    test_one_step_per_millisecond();
    test_macro_selection();
    test_macros_wait_their_turn();
    test_damaged_buffer_plays_nothing();
    test_truncated_actions_end_the_macro();
    return tests_result();
}
