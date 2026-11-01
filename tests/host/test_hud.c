#include "check.h"
#include "hud.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"
#include "matrix.h"
#include "report.h"
#include <string.h>

// ---- the board's side ----
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    return (uint16_t)(0x1000 + layer * 0x100 + row * 16 + col);
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

uint16_t keymap_default_override(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_cfg(uint8_t field)
{
    (void)field;
    return 0;
}

static uint8_t host_leds;

uint8_t hud_hook_host_leds(void) { return host_leds; }
void layer_hook_mods_add(uint8_t mods) { report_add_mods(mods); }
void layer_hook_mods_del(uint8_t mods) { report_del_mods(mods); }

// ---- the app's side ----
static uint8_t packet[32];

static void ask(uint8_t command, uint8_t sequence, const uint8_t *args, unsigned argc)
{
    memset(packet, 0, sizeof packet);
    packet[0] = 0xF0;
    packet[1] = command;
    packet[3] = sequence;
    memcpy(&packet[4], args, argc);
    CHECK(hud_receive(packet, 32));
}

static void no_args(uint8_t command, uint8_t sequence)
{
    ask(command, sequence, 0, 0);
}

static uint8_t held[MATRIX_COLS];

static void press(uint8_t col, uint8_t row, bool down)
{
    matrix_event_t e;

    if (down) {
        held[col] |= (uint8_t)(1u << row);
    } else {
        held[col] &= (uint8_t) ~(1u << row);
    }

    for (unsigned i = 0; i < MATRIX_DEBOUNCE_SCANS; i++) {
        matrix_feed(held);
    }

    while (matrix_event_pop(&e)) {
    }
}

static void reset(void)
{
    keymap_init();
    matrix_init();
    layer_init();
    report_init();
    memset(held, 0, sizeof held);
    host_leds = 0;
}

static void test_info_as_in_the_protocol_document(void)
{
    reset();
    no_args(0x01, 0x05);

    // f0 01 00 05 | 02 05 10 08 | 08 00 | 00 7e | 0c | VV | 02
    CHECK(packet[0] == 0xF0 && packet[1] == 0x01 && packet[2] == 0x00 && packet[3] == 0x05);
    CHECK(packet[4] == 2);     // protocol 2
    CHECK(packet[5] == 5);     // rows
    CHECK(packet[6] == 16);    // columns
    CHECK(packet[7] == 8);     // layers
    CHECK(packet[8] == 0x08 && packet[9] == 0x00);   // capabilities: bit 3 = the HUD_TOG key exists
    CHECK(packet[10] == 0x00 && packet[11] == 0x7E); // its keycode, 0x7E00
    CHECK(packet[12] == 12);   // keycodes per GET_KEYMAP
    CHECK(packet[13] == keymap_version());
    CHECK(packet[14] == 2);    // bytes per row
    for (unsigned i = 15; i < 32; i++) {
        CHECK(packet[i] == 0);
    }
}

static void test_the_matrix(void)
{
    reset();
    press(1, 1, true);  // row 1, column 1 (Q)
    press(11, 2, true); // row 2, column 11

    no_args(0x04, 0x06);
    CHECK(packet[1] == 0x04 && packet[2] == 0 && packet[3] == 0x06);
    // row by row, two bytes a row, the first holds columns 0-7: 00 00 | 02 00 | 00 08 | 00 00 | 00 00
    static const uint8_t expected[10] = {0x00, 0x00, 0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00};

    CHECK(memcmp(&packet[4], expected, 10) == 0);
    for (unsigned i = 14; i < 32; i++) {
        CHECK(packet[i] == 0);
    }

    press(15, 4, true);
    no_args(0x04, 0x07);
    CHECK(packet[4 + 2 * 4 + 1] == 0x80); // row 4, column 15
}

static void test_state(void)
{
    reset();
    press(3, 0, true);   // row 0, column 3
    press(12, 0, true);  // beyond the first 8 columns: not in the state bytes
    press(0, 4, true);   // row 4, column 0

    layer_hold(2, true);
    layer_hold(5, true);
    layer_set_default(1);
    layer_hook_mods_add(0x42);
    host_leds = 0x03; // num lock and caps lock

    no_args(0x02, 0x09);

    CHECK(packet[3] == 0x09 && packet[2] == 0);
    CHECK(packet[4] == ((1 << 2) | (1 << 5)) && packet[5] == 0 && packet[6] == 0 && packet[7] == 0); // the layers that are on, 32 bits
    CHECK(packet[8] == (1 << 1));            // the default layer
    CHECK(packet[9] == 0);
    CHECK(packet[10] == 0x08);               // row 0: column 3
    CHECK(packet[11] == 0 && packet[12] == 0 && packet[13] == 0);
    CHECK(packet[14] == 0x01);               // row 4: column 0
    CHECK(packet[15] == 0 && packet[16] == 0 && packet[17] == 0);
    CHECK(packet[18] == 0x42);               // mods
    CHECK(packet[19] == 0 && packet[21] == 0);
    CHECK(packet[22] == 0xFF);               // no one-shot layer
    CHECK(packet[23] == (0x01 | 0x10));      // caps lock (bit 0) and num lock (bit 4)
    CHECK(packet[28] == keymap_version());
}

static void test_the_one_shots(void)
{
    reset();
    layer_process(OSM(MOD_LSFT), true, 0);
    layer_process(OSM(MOD_LSFT), false, 0);
    layer_process(OSL(3), true, 0);
    layer_process(OSL(3), false, 0);

    no_args(0x02, 1);
    CHECK(packet[20] == 0x02); // the one-shot shift
    CHECK(packet[22] == 3);    // the one-shot layer
}

static void test_the_toggle_counter(void)
{
    reset();
    no_args(0x02, 1);
    const uint8_t before = packet[27];

    hud_toggle_pressed();
    hud_toggle_pressed();
    no_args(0x02, 2);
    CHECK(packet[27] == (uint8_t)(before + 2));
}

static void test_keymap(void)
{
    reset();
    keymap_set(2, 1, 4, 0xABCD);

    const uint8_t a[4] = {2, (uint8_t)(1 * 16 + 3), 0, 3}; // layer 2, from row 1 column 3, three keys
    ask(0x03, 0x0A, a, 4);

    CHECK(packet[2] == 0 && packet[3] == 0x0A);
    CHECK(memcmp(&packet[4], a, 4) == 0); // the request is echoed
    CHECK(packet[8] == 0x13 && packet[9] == 0x12);   // 0x1000 + 2 * 0x100 + 16 + 3 = 0x1213, little-endian
    CHECK(packet[10] == 0xCD && packet[11] == 0xAB); // the edited key: the keycode as it is now
    CHECK(packet[12] == 0x15 && packet[13] == 0x12);

    const uint8_t last[4] = {0, (uint8_t)((MATRIX_ROWS * MATRIX_COLS - 12) & 0xFF), 0, 12}; // the last 12 keys fit
    ask(0x03, 1, last, 4);
    CHECK(packet[2] == 0);
}

static void test_keymap_arguments_are_checked(void)
{
    reset();

    const uint8_t layer[4]  = {8, 0, 0, 1};
    const uint8_t zero[4]   = {0, 0, 0, 0};
    const uint8_t many[4]   = {0, 0, 0, 13};
    const uint8_t beyond[4] = {0, (uint8_t)(MATRIX_ROWS * MATRIX_COLS - 11), 0, 12};
    const uint8_t wrap[4]   = {0, 0xFF, 0xFF, 12}; // an offset that wraps in 16 bits

    ask(0x03, 7, layer, 4);
    CHECK(packet[2] == 2 && packet[3] == 7);
    ask(0x03, 7, zero, 4);
    CHECK(packet[2] == 2);
    ask(0x03, 7, many, 4);
    CHECK(packet[2] == 2);
    ask(0x03, 7, beyond, 4);
    CHECK(packet[2] == 2);
    ask(0x03, 7, wrap, 4);
    CHECK(packet[2] == 2);
}

static void test_unknown_commands_and_other_packets(void)
{
    reset();
    no_args(0x7F, 0x33);
    CHECK(packet[0] == 0xF0 && packet[1] == 0x7F && packet[2] == 1 && packet[3] == 0x33);

    uint8_t other[32] = {0x01, 0x02};

    CHECK(!hud_receive(other, 32));
    CHECK(other[0] == 0x01 && other[1] == 0x02); // left as it was

    uint8_t shorter[32] = {0xF0, 0x01};

    CHECK(!hud_receive(shorter, 31));
}

int main(void)
{
    test_info_as_in_the_protocol_document();
    test_the_matrix();
    test_state();
    test_the_one_shots();
    test_the_toggle_counter();
    test_keymap();
    test_keymap_arguments_are_checked();
    test_unknown_commands_and_other_packets();
    return tests_result();
}
