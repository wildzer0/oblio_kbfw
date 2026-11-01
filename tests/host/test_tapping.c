#include "check.h"
#include "stubs.h"
#include "keyboard.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"
#include "report.h"
#include <string.h>

// Row 0:  col0 LT(1,SPC)   col1 MT(LSFT,A)   col2 B   col3 TD(0)   col4 C   col5 TD(1)
// Layer 1, row 0: col2 X, the rest transparent.
// TD(0): tap MUTE, hold PLAY, double tap NEXT.   TD(1): tap D only.
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    static const uint16_t l0[6] = {LT(1, KC_SPC), MT(MOD_LSFT, KC_A), KC_B, TD(0), KC_C, TD(1)};

    if (row != 0 || col >= 6) {
        return layer == 0 ? KC_NO : KC_TRANSPARENT;
    }

    if (layer == 0) {
        return l0[col];
    }

    return (layer == 1 && col == 2) ? KC_X : KC_TRANSPARENT;
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
    static const uint16_t td0[TD_FIELDS] = {KC_MUTE, KC_MPLY, KC_MNXT, 0, 0};
    static const uint16_t td1[TD_FIELDS] = {KC_D, 0, 0, 0, 0};

    return index == 0 ? td0[field] : index == 1 ? td1[field] : 0;
}

uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_override(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_led(uint16_t index)
{
    (void)index;
    return 0;
}

// What reached the host, as a readable log: "+A" key down, "-A" key up, "mods=NN", "c=XXX" consumer
static char     log_text[512];
static uint16_t last_consumer;
static uint8_t  last_mods;

static void note(const char *what)
{
    if (log_text[0]) {
        strcat(log_text, " ");
    }
    strcat(log_text, what);
}

static uint8_t held_keys[REPORT_BOOT_KEYS];

void report_hook_send(report_kind_t kind)
{
    char text[32];
    uint8_t boot[REPORT_BOOT_SIZE];

    report_build_boot(boot);

    if (kind == REPORT_CONSUMER) {
        snprintf(text, sizeof text, "c%03X", report_consumer());
        note(text);
        last_consumer = report_consumer();
        return;
    }

    if (boot[0] != last_mods) {
        snprintf(text, sizeof text, "m%02X", boot[0]);
        note(text);
        last_mods = boot[0];
    }

    for (unsigned i = 0; i < REPORT_BOOT_KEYS; i++) { // the log is made of key changes
        if (boot[2 + i] && !memchr(held_keys, boot[2 + i], REPORT_BOOT_KEYS)) {
            snprintf(text, sizeof text, "+%02X", boot[2 + i]);
            note(text);
        }
    }

    for (unsigned i = 0; i < REPORT_BOOT_KEYS; i++) {
        if (held_keys[i] && !memchr(&boot[2], held_keys[i], REPORT_BOOT_KEYS)) {
            snprintf(text, sizeof text, "-%02X", held_keys[i]);
            note(text);
        }
    }

    memcpy(held_keys, &boot[2], REPORT_BOOT_KEYS);
}

static void reset(void)
{
    log_text[0]   = 0;
    last_consumer = 0;
    last_mods     = 0;
    memset(held_keys, 0, sizeof held_keys);
    keyboard_init();
}

static void ev(uint8_t col, bool pressed, uint16_t t)
{
    const matrix_event_t e = {.col = col, .row = 0, .pressed = pressed};

    keyboard_event(&e, t);
    keyboard_task(t);
}

static void at(uint16_t t)
{
    keyboard_task(t);
}

#define CHECK_LOG(expected)                                                          \
    do {                                                                             \
        if (strcmp(log_text, expected) != 0) {                                       \
            printf("  FAIL %s:%d: log \"%s\", expected \"%s\"\n", __FILE__, __LINE__, log_text, expected); \
            checks_failed++;                                                         \
        }                                                                            \
    } while (0)

static void test_layer_tap_as_tap(void)
{
    reset();
    ev(0, true, 0);
    CHECK_LOG(""); // undecided: nothing yet
    ev(0, false, 50);
    CHECK_LOG("+2C -2C"); // space
    CHECK(layer_get_state() == 0);
}

static void test_layer_tap_as_hold(void)
{
    reset();
    ev(0, true, 0);
    at(199);
    CHECK(layer_get_state() == 0);
    at(200);
    CHECK(layer_get_state() == 0x02);

    ev(2, true, 250); // B's cell, but layer 1 says X
    ev(2, false, 260);
    CHECK_LOG("+1B -1B");
    ev(0, false, 300);
    CHECK(layer_get_state() == 0);
    CHECK_LOG("+1B -1B"); // releasing the layer key sends nothing
}

static void test_keys_typed_during_the_wait_follow_the_decision(void)
{
    reset();

    // released as a tap: the queued key is replayed on the base layer, after the space
    ev(0, true, 0);
    ev(2, true, 30);
    ev(2, false, 60);
    CHECK_LOG("");
    ev(0, false, 80);
    CHECK_LOG("+2C -2C +05 -05");

    // held: the queued key is replayed on layer 1
    reset();
    ev(0, true, 0);
    ev(2, true, 30);
    ev(2, false, 60);
    at(200);
    CHECK(layer_get_state() == 0x02);
    CHECK_LOG("+1B -1B");
    ev(0, false, 300);
    CHECK(layer_get_state() == 0);
}

static void test_mod_tap(void)
{
    reset();
    ev(1, true, 0);
    ev(1, false, 50);
    CHECK_LOG("+04 -04"); // a

    reset();
    ev(1, true, 0);
    at(200);
    CHECK_LOG("m02");
    ev(4, true, 250);
    ev(4, false, 260);
    CHECK_LOG("m02 +06 -06"); // shift is held while C is typed
    ev(1, false, 300);
    CHECK_LOG("m02 +06 -06 m00");
}

static void test_tap_dance_tap_waits_for_a_second_tap(void)
{
    reset();
    ev(3, true, 0);
    ev(3, false, 50);
    at(249);
    CHECK_LOG(""); // still waiting, a second tap could come
    at(250);
    CHECK_LOG("c0E2 c000"); // mute
}

static void test_tap_dance_double_tap(void)
{
    reset();
    ev(3, true, 0);
    ev(3, false, 50);
    ev(3, true, 100);
    CHECK_LOG("c0B5"); // next track goes down with the second press
    ev(3, false, 150);
    CHECK_LOG("c0B5 c000");
    at(1000);
    CHECK_LOG("c0B5 c000"); // and nothing more
}

static void test_tap_dance_hold(void)
{
    reset();
    ev(3, true, 0);
    at(200);
    CHECK_LOG("c0CD"); // play/pause held
    ev(3, false, 400);
    CHECK_LOG("c0CD c000");
}

static void test_tap_dance_without_double_tap_answers_at_once(void)
{
    reset();
    ev(5, true, 0);
    ev(5, false, 50);
    CHECK_LOG("+07 -07"); // d, no waiting for a second tap

    reset();
    ev(5, true, 0);
    at(200); // no hold action: the tap action is held instead
    CHECK_LOG("+07");
    ev(5, false, 300);
    CHECK_LOG("+07 -07");
}

static void test_full_queue_turns_the_wait_into_a_hold(void)
{
    reset();
    ev(0, true, 0);

    for (unsigned i = 0; i < 4; i++) { // 8 events fit
        ev(2, true, (uint16_t)(10 + i * 10));
        ev(2, false, (uint16_t)(15 + i * 10));
    }
    CHECK(layer_get_state() == 0);

    ev(4, true, 100); // the ninth does not: the key is a hold, well before the term
    CHECK(layer_get_state() == 0x02);
}

static void test_term_is_taken_across_the_clock_wrap(void)
{
    reset();
    ev(0, true, 65500);
    at(65535);
    CHECK(layer_get_state() == 0);
    at(164); // 200 ms after the press: the 16-bit clock has wrapped
    CHECK(layer_get_state() == 0x02);
}

static void test_tapping_term_setting(void)
{
    reset();
    keymap_set_tapping_term(100);
    ev(0, true, 0);
    at(99);
    CHECK(layer_get_state() == 0);
    at(100);
    CHECK(layer_get_state() == 0x02);
}

int main(void)
{
    test_layer_tap_as_tap();
    test_layer_tap_as_hold();
    test_keys_typed_during_the_wait_follow_the_decision();
    test_mod_tap();
    test_tap_dance_tap_waits_for_a_second_tap();
    test_tap_dance_double_tap();
    test_tap_dance_hold();
    test_tap_dance_without_double_tap_answers_at_once();
    test_full_queue_turns_the_wait_into_a_hold();
    test_term_is_taken_across_the_clock_wrap();
    test_tapping_term_setting();
    return tests_result();
}
