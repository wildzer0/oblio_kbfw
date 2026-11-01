#include "check.h"
#include "diag_stubs.h"
#include "stubs.h"
#include "keycodes.h"
#include "keymap.h"
#include "matrix.h"
#include "via.h"
#include "vial.h"
#include <string.h>

// ---- the board's side ----
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    return (uint16_t)(0x0100 * (layer + 1) + row * 16 + col); // distinct in every cell, and not 0 or 1
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)encoder;
    return (uint16_t)(0x0300 + layer * 2 + (clockwise ? 1 : 0));
}

uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field)
{
    return (uint16_t)(index * 16 + field);
}

uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint8_t hud_hook_host_leds(void) { return 0; }
void layer_hook_mods_add(uint8_t m) { (void)m; }
void layer_hook_mods_del(uint8_t m) { (void)m; }

static uint32_t now_ms;

uint32_t via_hook_now(void) { return now_ms; }

// a 70-byte "definition": three pages of 32, the last one partial
const ROM uint8_t vial_definition[70] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69,
};
const uint16_t vial_definition_size = 70;
const ROM uint8_t vial_keyboard_uid[8] = {0x46, 0xEC, 0xD0, 0x56, 0x5D, 0xF8, 0x4F, 0xA8};
const ROM uint8_t vial_unlock_keys[][2] = {{0, 0}, {2, 13}}; // (row, col)
const uint8_t vial_unlock_key_count = 2;

// ---- the host ----
static uint8_t buf[VIA_RAW_SIZE];

static void request(const uint8_t *bytes, unsigned length)
{
    memset(buf, 0, sizeof buf);
    memcpy(buf, bytes, length);
    via_receive(buf, VIA_RAW_SIZE);
}

#define REQUEST(...)                                  \
    do {                                              \
        static const uint8_t bytes_[] = {__VA_ARGS__}; \
        request(bytes_, sizeof bytes_);               \
    } while (0)

static uint8_t raw[MATRIX_COLS];

static void press(uint8_t col, uint8_t row, bool down)
{
    if (down) {
        raw[col] |= (uint8_t)(1u << row);
    } else {
        raw[col] &= (uint8_t) ~(1u << row);
    }

    for (unsigned i = 0; i < MATRIX_DEBOUNCE_SCANS; i++) {
        matrix_feed(raw);
    }

    matrix_event_t event;

    while (matrix_event_pop(&event)) { // nobody else consumes them here; a full queue would hold changes back
    }
}

static void reset(void)
{
    keymap_init();
    matrix_init();
    via_init();
    now_ms = 0;
    memset(raw, 0, sizeof raw); // nothing held
}

// Runs the whole unlock with the two keys held
static void unlock(void)
{
    REQUEST(0xFE, vial_unlock_start);
    press(0, 0, true);
    press(13, 2, true);

    for (unsigned i = 0; i < UINT8_MAX && !vial_is_unlocked(); i++) {
        now_ms += 101;
        REQUEST(0xFE, vial_unlock_poll);
    }

    press(0, 0, false);
    press(13, 2, false);
}

static void test_protocol_version_and_values(void)
{
    reset();
    REQUEST(0x01);
    CHECK(buf[0] == 0x01 && buf[1] == 0x00 && buf[2] == 0x09);

    now_ms = 0x01020304;
    REQUEST(0x02, 0x01);
    CHECK(buf[2] == 0x01 && buf[3] == 0x02 && buf[4] == 0x03 && buf[5] == 0x04); // uptime, big-endian

    REQUEST(0x03, 0x02, 0xDE, 0xAD, 0xBE, 0xEF); // layout options: the GUI stores them
    REQUEST(0x02, 0x02);
    CHECK(buf[2] == 0xDE && buf[3] == 0xAD && buf[4] == 0xBE && buf[5] == 0xEF);

    REQUEST(0x02, 0x7F);
    CHECK(buf[0] == 0xFF);
    REQUEST(0x03, 0x7F, 1, 2, 3, 4);
    CHECK(buf[0] == 0xFF);
    REQUEST(0x77);
    CHECK(buf[0] == 0xFF);
}

static void test_wrong_length_is_ignored(void)
{
    reset();
    memset(buf, 0, sizeof buf);
    buf[0] = 0x01;
    via_receive(buf, 31);
    CHECK(buf[1] == 0 && buf[2] == 0);
}

static void test_keycodes(void)
{
    reset();
    REQUEST(0x04, 2, 3, 4);
    CHECK(buf[4] == 0x03 && buf[5] == 0x34); // layer 2: 0x0300 + 3*16 + 4, big-endian

    REQUEST(0x05, 2, 3, 4, 0x12, 0x34);
    CHECK(keymap_get(2, 3, 4) == 0x1234);
    REQUEST(0x04, 2, 3, 4);
    CHECK(buf[4] == 0x12 && buf[5] == 0x34);

    REQUEST(0x04, KEYMAP_LAYERS, 0, 0); // no such layer, row or column
    CHECK(buf[4] == 0 && buf[5] == 0);
    REQUEST(0x04, 0, MATRIX_ROWS, 0);
    CHECK(buf[4] == 0 && buf[5] == 0);
    REQUEST(0x05, KEYMAP_LAYERS, 0, 0, 0x99, 0x99); // and nothing is written
    CHECK(keymap_get(0, 0, 0) == 0x0100);

    REQUEST(0x11);
    CHECK(buf[1] == KEYMAP_LAYERS);

    REQUEST(0x06); // reset
    CHECK(keymap_get(2, 3, 4) == 0x0334);
}

static void test_keymap_buffer(void)
{
    reset();

    REQUEST(0x12, 0, 0, 28); // the first 14 keycodes of layer 0
    CHECK(buf[4] == 0x01 && buf[5] == 0x00 && buf[6] == 0x01 && buf[7] == 0x01 && buf[30] == 0x01 && buf[31] == 0x0D);

    const unsigned total = KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS * 2;

    REQUEST(0x12, (uint8_t)((total - 4) >> 8), (uint8_t)(total - 4), 8); // runs past the end: zeros
    CHECK(buf[4] == 0x08 && buf[5] == 0x4E && buf[6] == 0x08 && buf[7] == 0x4F); // the last two cells of layer 7 (0x0800 + 78, 79)
    CHECK(buf[8] == 0 && buf[9] == 0 && buf[10] == 0 && buf[11] == 0);

    REQUEST(0x12, 0, 0, 29); // more than a report holds: the request comes back as it is
    CHECK(buf[4] == 0 && buf[2] == 0 && buf[3] == 29);

    // write layer 2, row 0, cols 3-4 (offset = 2 * (2 * 80 + 3)), then one odd byte
    const unsigned offset = 2 * (2 * 80 + 3);

    REQUEST(0x13, (uint8_t)(offset >> 8), (uint8_t)offset, 4, 0xAA, 0xBB, 0xCC, 0xDD);
    CHECK(keymap_get(2, 0, 3) == 0xAABB && keymap_get(2, 0, 4) == 0xCCDD);
    REQUEST(0x13, (uint8_t)((offset + 1) >> 8), (uint8_t)(offset + 1), 1, 0x11); // the low byte of the first only
    CHECK(keymap_get(2, 0, 3) == 0xAA11);

    REQUEST(0x13, (uint8_t)(total >> 8), (uint8_t)total, 2, 1, 2); // past the end: nothing
    CHECK(keymap_get(7, 4, 15) == 0x084F);
}

static void test_encoders(void)
{
    reset();
    REQUEST(0xFE, vial_get_encoder, 1, 0);
    CHECK(buf[0] == 0x03 && buf[1] == 0x02 && buf[2] == 0x03 && buf[3] == 0x03); // layer 1: ccw 0x0302, cw 0x0303

    REQUEST(0xFE, vial_set_encoder, 1, 0, 1, 0x12, 0x34); // clockwise
    CHECK(keymap_get_encoder(1, 0, true) == 0x1234 && keymap_get_encoder(1, 0, false) == 0x0302);
    REQUEST(0xFE, vial_set_encoder, 1, 0, 0, 0x56, 0x78);
    CHECK(keymap_get_encoder(1, 0, false) == 0x5678);

    REQUEST(0xFE, vial_get_encoder, KEYMAP_LAYERS, 0);
    CHECK(buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0);
    REQUEST(0xFE, vial_get_encoder, 0, ENCODER_COUNT);
    CHECK(buf[0] == 0 && buf[2] == 0);
}

static void test_keyboard_id_and_definition(void)
{
    reset();
    REQUEST(0xFE, vial_get_keyboard_id);
    CHECK(buf[0] == 6 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0);
    CHECK(memcmp(&buf[4], vial_keyboard_uid, 8) == 0 && buf[12] == 0);

    REQUEST(0xFE, vial_get_size);
    CHECK(buf[0] == 70 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0);

    REQUEST(0xFE, vial_get_def, 0, 0);
    CHECK(memcmp(buf, vial_definition, 32) == 0);
    REQUEST(0xFE, vial_get_def, 1, 0);
    CHECK(memcmp(buf, vial_definition + 32, 32) == 0);
    REQUEST(0xFE, vial_get_def, 2, 0); // the last page: 6 bytes, the rest of the buffer is the request
    CHECK(memcmp(buf, vial_definition + 64, 6) == 0 && buf[6] == 0 && buf[7] == 0);
    REQUEST(0xFE, vial_get_def, 3, 0); // past the end: the request comes back
    CHECK(buf[0] == 0xFE && buf[1] == vial_get_def && buf[2] == 3);
}

static void test_unlock_flow(void)
{
    reset();

    REQUEST(0xFE, vial_get_unlock_status);
    CHECK(buf[0] == 0 && buf[1] == 0);
    CHECK(buf[2] == 0 && buf[3] == 0 && buf[4] == 2 && buf[5] == 13 && buf[6] == 0xFF && buf[31] == 0xFF);

    REQUEST(0xFE, vial_unlock_start);
    press(0, 0, true);
    press(13, 2, true);

    for (unsigned i = 1; i < 50; i++) { // 49 steps of 101 ms: not yet
        now_ms += 101;
        REQUEST(0xFE, vial_unlock_poll);
        CHECK(buf[0] == 0 && buf[1] == 1 && buf[2] == 50 - i);
    }

    now_ms += 101;
    REQUEST(0xFE, vial_unlock_poll);
    CHECK(buf[0] == 1 && buf[1] == 0);
    CHECK(vial_is_unlocked() && !vial_unlock_in_progress());

    REQUEST(0xFE, vial_lock);
    CHECK(!vial_is_unlocked());
}

static void test_unlock_needs_the_keys_held_and_a_steady_pace(void)
{
    reset();
    REQUEST(0xFE, vial_unlock_start);
    press(0, 0, true);
    press(13, 2, true);

    for (unsigned i = 0; i < 20; i++) {
        now_ms += 101;
        REQUEST(0xFE, vial_unlock_poll);
    }
    CHECK(buf[2] == 30);

    press(13, 2, false); // a key let go: the count starts again
    now_ms += 101;
    REQUEST(0xFE, vial_unlock_poll);
    CHECK(buf[2] == 50);
    press(13, 2, true);

    now_ms += 101;
    REQUEST(0xFE, vial_unlock_poll);
    CHECK(buf[2] == 49);
    now_ms += 50; // polled too soon
    REQUEST(0xFE, vial_unlock_poll);
    CHECK(buf[2] == 50);
}

static void test_only_the_unlock_questions_are_answered_during_an_unlock(void)
{
    reset();
    REQUEST(0xFE, vial_unlock_start);
    CHECK(vial_unlock_in_progress());

    REQUEST(0x01); // protocol version: left alone
    CHECK(buf[0] == 0x01 && buf[1] == 0x00 && buf[2] == 0x00);
    REQUEST(0xFE, vial_get_encoder, 0, 0);
    CHECK(buf[0] == 0xFE && buf[1] == vial_get_encoder);

    REQUEST(0xFE, vial_get_keyboard_id); // allowed
    CHECK(buf[0] == 6);
}

static void test_locked_keyboard_protects_macros_matrix_and_bootloader(void)
{
    reset();

    REQUEST(0x0F, 0, 0, 3, 'a', 'b', 0); // macro write
    CHECK(keymap_macro_byte(0) == 0);
    REQUEST(0x10); // macro reset
    REQUEST(0x02, 0x03); // the matrix tester
    CHECK(buf[2] == 0 && buf[3] == 0);
    REQUEST(0x0B);
    CHECK(!via_bootloader_requested());

    unlock();
    CHECK(vial_is_unlocked());

    REQUEST(0x0F, 0, 0, 3, 'a', 'b', 0);
    CHECK(keymap_macro_byte(0) == 'a' && keymap_macro_byte(1) == 'b');
    REQUEST(0x0E, 0, 0, 4); // read back
    CHECK(buf[4] == 'a' && buf[5] == 'b' && buf[6] == 0);
    REQUEST(0x0F, 0, 5, 29, 1); // too big for a report
    CHECK(keymap_macro_byte(5) == 0);
    REQUEST(0x10);
    CHECK(keymap_macro_byte(0) == 0);

    REQUEST(0x0B);
    CHECK(via_bootloader_requested());
}

static void test_macro_geometry(void)
{
    reset();
    REQUEST(0x0C);
    CHECK(buf[1] == KEYMAP_MACRO_COUNT);
    REQUEST(0x0D);
    CHECK(buf[1] == 0x02 && buf[2] == 0x00); // 512
    REQUEST(0x0E, 0x01, 0xFE, 4); // the last two bytes of the buffer, then past the end
    CHECK(buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 0);
}

static void test_matrix_state(void)
{
    reset();
    press(3, 1, true);
    press(15, 4, true);

    REQUEST(0x02, 0x03);
    CHECK(buf[2] == 0 && buf[3] == 0 && buf[4] == 0 && buf[5] == 0); // locked: nothing shown

    unlock();
    press(3, 1, true);
    press(15, 4, true);
    REQUEST(0x02, 0x03);
    // per row two bytes, big-endian: row 1 has column 3, row 4 has column 15
    CHECK(buf[2] == 0 && buf[3] == 0);
    CHECK(buf[4] == 0x00 && buf[5] == 0x08);
    CHECK(buf[6] == 0 && buf[7] == 0 && buf[8] == 0 && buf[9] == 0);
    CHECK(buf[10] == 0x80 && buf[11] == 0x00);
}

static void test_qmk_settings(void)
{
    reset();

    REQUEST(0xFE, vial_qmk_settings_query, 0, 0);
    CHECK(buf[0] == 7 && buf[1] == 0 && buf[2] == 0xFF && buf[31] == 0xFF);
    REQUEST(0xFE, vial_qmk_settings_query, 7, 0);
    CHECK(buf[0] == 0xFF && buf[1] == 0xFF);

    REQUEST(0xFE, vial_qmk_settings_get, 7, 0);
    CHECK(buf[0] == 0 && buf[1] == 200 && buf[2] == 0);
    REQUEST(0xFE, vial_qmk_settings_set, 7, 0, 0x2C, 0x01); // 300
    CHECK(buf[0] == 0 && keymap_tapping_term() == 300);
    REQUEST(0xFE, vial_qmk_settings_get, 7, 0);
    CHECK(buf[1] == 0x2C && buf[2] == 0x01);

    REQUEST(0xFE, vial_qmk_settings_get, 3, 0);
    CHECK(buf[0] == 1);
    REQUEST(0xFE, vial_qmk_settings_set, 3, 0, 1, 0);
    CHECK(buf[0] == 1 && keymap_tapping_term() == 300);

    REQUEST(0xFE, vial_qmk_settings_reset);
    CHECK(buf[0] == 0 && keymap_tapping_term() == KEYMAP_TAPPING_TERM_DEFAULT);
}

static void test_tap_dances(void)
{
    reset();

    REQUEST(0xFE, vial_dynamic_entry_op, 0x00);
    CHECK(buf[0] == KEYMAP_TAP_DANCE_COUNT && buf[1] == KEYMAP_COMBO_COUNT && buf[2] == 0); // combos, no key overrides

    REQUEST(0xFE, vial_dynamic_entry_op, 0x01, 2); // tap dance 2: words 32..36
    CHECK(buf[0] == 0 && buf[1] == 32 && buf[2] == 0 && buf[9] == 36 && buf[10] == 0);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x02, 3, 1, 0, 2, 0, 3, 0x01, 4, 0, 5, 0); // five words from byte 4
    CHECK(buf[0] == 0);
    CHECK(keymap_tap_dance_get(3, 0) == 1 && keymap_tap_dance_get(3, 1) == 2 && keymap_tap_dance_get(3, 2) == 0x0103);
    CHECK(keymap_tap_dance_get(3, 3) == 4 && keymap_tap_dance_get(3, 4) == 5);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x01, KEYMAP_TAP_DANCE_COUNT); // no such entry
    CHECK(buf[0] == 1);
    REQUEST(0xFE, vial_dynamic_entry_op, 0x02, KEYMAP_TAP_DANCE_COUNT, 9, 9);
    CHECK(buf[0] == 1);

    // combos: four input keycodes then the output, 10 bytes
    REQUEST(0xFE, vial_dynamic_entry_op, 0x03, 1);
    CHECK(buf[0] == 0 && buf[1] == 0 && buf[9] == 0);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x04, 1, KC_J, 0, KC_K, 0, 0, 0, 0, 0, KC_ESC, 0);
    CHECK(buf[0] == 0);
    CHECK(keymap_combo_get(1, 0) == KC_J && keymap_combo_get(1, 1) == KC_K && keymap_combo_get(1, 3) == 0);
    CHECK(keymap_combo_get(1, COMBO_INPUTS) == KC_ESC);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x03, 1);
    CHECK(buf[0] == 0 && buf[1] == KC_J && buf[2] == 0 && buf[3] == KC_K && buf[9] == KC_ESC && buf[10] == 0);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x03, KEYMAP_COMBO_COUNT); // no such entry
    CHECK(buf[0] == 1);
    REQUEST(0xFE, vial_dynamic_entry_op, 0x04, KEYMAP_COMBO_COUNT, KC_A, 0);
    CHECK(buf[0] == 1);

    REQUEST(0xFE, vial_dynamic_entry_op, 0x7F); // an operation we do not know: echoed
    CHECK(buf[0] == 0xFE);
    REQUEST(0xFE, 0x7F); // a Vial command we do not know: echoed
    CHECK(buf[0] == 0xFE && buf[1] == 0x7F);
}

static void test_the_hud_is_reached_through_via(void)
{
    reset();
    REQUEST(0xF0, 0x01, 0, 0x05); // GET_INFO, sequence 5
    CHECK(buf[0] == 0xF0 && buf[1] == 0x01 && buf[2] == 0 && buf[3] == 0x05 && buf[4] == 2); // protocol 2: it answered

    REQUEST(0xF0, 0x7F, 0, 0x09); // a command it does not know: status 1
    CHECK(buf[0] == 0xF0 && buf[2] == 1 && buf[3] == 0x09);
}

static void test_diagnostics_need_no_unlock_except_the_monitor(void)
{
    reset();
    REQUEST(0xF1, 0);
    CHECK(buf[0] == 0xF1 && buf[2] == DIAG_COUNT); // the counters answer locked too: they are what is needed when the keys do not work
    REQUEST(0xF1, 1);
    CHECK(buf[0] == 0xF1); // the radio's data is read-only: it answers locked too
    REQUEST(0xF1, 2, 1);
    CHECK(buf[0] == 0xFF); // the monitor switch changes what the radio does: unlocked only

    unlock();
    REQUEST(0xF1, 0);
    CHECK(buf[0] == 0xF1 && buf[1] == 0 && buf[2] == DIAG_COUNT);
    REQUEST(0xF1, 1);
    CHECK(buf[0] == 0xF1 && buf[1] == 1);
}

static void test_edits_are_seen_by_the_store(void)
{
    reset();
    const uint8_t before = keymap_version();

    REQUEST(0x05, 0, 0, 0, 0x00, 0x04);
    CHECK(keymap_version() != before); // the store saves on a version change: no separate "changed" call is needed
}

int main(void)
{
    test_protocol_version_and_values();
    test_wrong_length_is_ignored();
    test_keycodes();
    test_keymap_buffer();
    test_encoders();
    test_keyboard_id_and_definition();
    test_unlock_flow();
    test_unlock_needs_the_keys_held_and_a_steady_pace();
    test_only_the_unlock_questions_are_answered_during_an_unlock();
    test_locked_keyboard_protects_macros_matrix_and_bootloader();
    test_macro_geometry();
    test_matrix_state();
    test_qmk_settings();
    test_tap_dances();
    test_the_hud_is_reached_through_via();
    test_diagnostics_need_no_unlock_except_the_monitor();
    test_edits_are_seen_by_the_store();
    return tests_result();
}
