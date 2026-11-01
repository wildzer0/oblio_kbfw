#include "check.h"
#include "keyboard.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"
#include "report.h"
#include <string.h>

// Test keymap, row 0:  A  MO(1)  LSFT(B)  LCTL  OSM(LSFT)  MUTE  SLEEP  OSL(1)
//                layer 1:  X  TRNS ...
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    static const uint16_t l0[8] = {KC_A, MO(1), LSFT(KC_B), KC_LCTL, OSM(MOD_LSFT), KC_MUTE, KC_SYSTEM_SLEEP, OSL(1)};

    if (layer == 0) {
        return (row == 0 && col < 8) ? l0[col] : KC_NO;
    }

    return (layer == 1 && row == 0 && col == 0) ? KC_X : KC_TRANSPARENT;
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)layer;
    (void)encoder;
    (void)clockwise;
    return KC_NO;
}

// every report sent: kind, then the boot report, consumer and system usages at that moment
typedef struct {
    report_kind_t kind;
    uint8_t       boot[REPORT_BOOT_SIZE];
    uint16_t      consumer;
    uint16_t      system;
} sent_t;

static sent_t sent[64];
static unsigned sent_count;

void report_hook_send(report_kind_t kind)
{
    sent_t *s = &sent[sent_count++];

    s->kind = kind;
    report_build_boot(s->boot);
    s->consumer = report_consumer();
    s->system   = report_system();
}

static uint16_t clock_ms;

static void reset(void)
{
    sent_count = 0;
    clock_ms   = 0;
    keyboard_init();
}

static void key(uint8_t col, bool pressed)
{
    const matrix_event_t ev = {.col = col, .row = 0, .pressed = pressed};

    keyboard_event(&ev, clock_ms++);
}

static void test_plain_key(void)
{
    reset();
    key(0, true);
    CHECK(sent_count == 1 && sent[0].kind == REPORT_KEYBOARD);
    CHECK(sent[0].boot[2] == KC_A && sent[0].boot[0] == 0);
    key(0, false);
    CHECK(sent_count == 2 && sent[1].boot[2] == 0);
}

static void test_modifier_key(void)
{
    reset();
    key(3, true);
    CHECK(sent_count == 1 && sent[0].boot[0] == 0x01 && sent[0].boot[2] == 0);
    key(3, false);
    CHECK(sent[sent_count - 1].boot[0] == 0);
}

static void test_mods_plus_key_order(void)
{
    reset();
    key(2, true); // LSFT(KC_B)
    CHECK(sent_count == 2);
    CHECK(sent[0].boot[0] == 0x02 && sent[0].boot[2] == 0); // shift first
    CHECK(sent[1].boot[0] == 0x02 && sent[1].boot[2] == KC_B);
    key(2, false);
    CHECK(sent_count == 4);
    CHECK(sent[2].boot[0] == 0x02 && sent[2].boot[2] == 0); // key up first
    CHECK(sent[3].boot[0] == 0 && sent[3].boot[2] == 0);
}

static void test_layer_key_then_layer_key(void)
{
    reset();
    key(1, true); // MO(1): a layer key sends nothing
    CHECK(sent_count == 0);
    key(0, true);
    CHECK(sent[0].boot[2] == KC_X);
    key(1, false);
    key(0, false); // still releases X, though the layer is gone
    CHECK(sent[sent_count - 1].boot[2] == 0);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_A);
}

static void test_consumer_and_system(void)
{
    reset();
    key(5, true);
    CHECK(sent_count == 1 && sent[0].kind == REPORT_CONSUMER && sent[0].consumer == 0x0E2);
    key(5, false);
    CHECK(sent[1].kind == REPORT_CONSUMER && sent[1].consumer == 0);

    key(6, true);
    CHECK(sent[2].kind == REPORT_SYSTEM && sent[2].system == 2);
    key(6, false);
    CHECK(sent[3].system == 0);
}

static void test_oneshot_shift_applies_to_one_key(void)
{
    reset();
    key(4, true); // OSM(LSFT) tapped
    key(4, false);
    const unsigned before = sent_count;
    CHECK(sent[before - 1].boot[0] == 0x02); // shift is on

    key(0, true); // the next key
    CHECK(sent[sent_count - 1].boot[0] == 0x02 && sent[sent_count - 1].boot[2] == KC_A);
    key(0, false);
    CHECK(sent[sent_count - 1].boot[0] == 0 && sent[sent_count - 1].boot[2] == 0);

    key(0, true); // and the one after is plain
    CHECK(sent[sent_count - 1].boot[0] == 0);
}

static void test_oneshot_layer_applies_to_one_key(void)
{
    reset();
    key(7, true); // OSL(1) tapped
    key(7, false);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_X);
    key(0, false);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_A);
}

int main(void)
{
    test_plain_key();
    test_modifier_key();
    test_mods_plus_key_order();
    test_layer_key_then_layer_key();
    test_consumer_and_system();
    test_oneshot_shift_applies_to_one_key();
    test_oneshot_layer_applies_to_one_key();
    return tests_result();
}
