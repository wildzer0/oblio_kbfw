#include "check.h"
#include "keymap.h"
#include "keycodes.h"
#include "led_effect.h"
#include "vialrgb.h"
#include <string.h>

// The keymap side the module talks to
static uint16_t leds[KEYMAP_LED_COUNT];
static uint16_t cfg[KEYMAP_CFG_COUNT];
static uint16_t keys[MATRIX_ROWS][MATRIX_COLS];
static unsigned saves;

uint16_t keymap_led_get(uint16_t index)
{
    return (index < KEYMAP_LED_COUNT) ? leds[index] : 0;
}

bool keymap_led_set(uint16_t index, uint16_t rgb565)
{
    if (index >= KEYMAP_LED_COUNT) {
        return false;
    }

    leds[index] = rgb565;
    return true;
}

uint16_t keymap_cfg_get(uint8_t field)
{
    return (field < KEYMAP_CFG_COUNT) ? cfg[field] : 0;
}

bool keymap_cfg_set(uint8_t field, uint16_t value)
{
    cfg[field] = value;
    return true;
}

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    (void)layer;
    return (row < MATRIX_ROWS && col < MATRIX_COLS) ? keys[row][col] : KC_NO;
}

void vialrgb_hook_save(void)
{
    saves++;
}

static uint8_t packet[32];

static void request(uint8_t command, const uint8_t *args, uint8_t count)
{
    memset(packet, 0, sizeof packet);
    packet[0] = 0x08; // id_lighting_get_value
    packet[1] = command;

    for (uint8_t i = 0; i < count; i++) {
        packet[2 + i] = args[i];
    }

    vialrgb_get_value(packet);
}

static void reset(void)
{
    memset(leds, 0, sizeof leds);
    memset(cfg, 0, sizeof cfg);
    memset(keys, 0, sizeof keys);
    keys[4][0] = KC_A; // a key, for the LED info
    saves      = 0;
    vialrgb_init();
}

static void test_the_info(void)
{
    reset();
    request(0x40, 0, 0);

    CHECK(packet[2] == 1 && packet[3] == 0);   // protocol 1
    CHECK(packet[4] == 255);                   // maximum brightness
}

static void test_the_effects_it_offers(void)
{
    const uint8_t args[2] = {0xFF, 0xFF}; // "the ids above this one": none
    reset();
    request(0x42, args, 2);
    CHECK(packet[2] == 0xFF);

    const uint8_t from_zero[2] = {0x00, 0x00}; // off and direct
    request(0x42, from_zero, 2);
    CHECK(packet[2] == VIALRGB_EFFECT_DIRECT && packet[3] == 0 && packet[4] == 0xFF);
}

static void test_the_mode_is_the_effect_of_the_profile(void)
{
    reset();
    cfg[CFG_LED_EFFECT] = FX_VIAL;
    cfg[CFG_LED_BRIGHT] = 128;
    cfg[CFG_LED_SPEED]  = 7;

    request(0x41, 0, 0);
    CHECK(packet[2] == VIALRGB_EFFECT_DIRECT && packet[3] == 0);
    CHECK(packet[4] == 7 && packet[7] == 128); // speed and brightness

    // choosing "direct" from the app turns the per-key effect on
    uint8_t set[6] = {VIALRGB_EFFECT_DIRECT, 0, 9, 0, 0, 200};
    memset(packet, 0, sizeof packet);
    packet[0] = 0x07; // id_lighting_set_value
    packet[1] = 0x41;
    memcpy(&packet[2], set, sizeof set);
    vialrgb_set_value(packet);
    CHECK(cfg[CFG_LED_EFFECT] == FX_VIAL && cfg[CFG_LED_BRIGHT] == 200 && cfg[CFG_LED_SPEED] == 9);

    // anything else is "off": this keyboard has no rgb_matrix effects
    set[0] = 6;
    memset(packet, 0, sizeof packet);
    packet[0] = 0x07;
    packet[1] = 0x41;
    memcpy(&packet[2], set, sizeof set);
    vialrgb_set_value(packet);
    CHECK(cfg[CFG_LED_EFFECT] == FX_OFF);
}

static void test_the_leds_and_their_places(void)
{
    const uint8_t count[2] = {0, 0};
    reset();
    request(0x43, count, 2);
    CHECK(packet[2] == (uint8_t)VIALRGB_LEDS && packet[3] == (uint8_t)(VIALRGB_LEDS >> 8));

    // the first cell of the last row is a key: the app gets its matrix position and a place on its grid
    const uint8_t last[2] = {(uint8_t)(4 * MATRIX_COLS), 0};
    request(0x44, last, 2);
    CHECK(packet[2] == 0 && packet[3] == (uint8_t)(4 * 224 / MATRIX_ROWS)); // x, y
    CHECK(packet[4] == 4);                                                   // a key light
    CHECK(packet[5] == 4 && packet[6] == 0);                                 // row, column

    // an empty cell is not a key
    const uint8_t empty[2] = {14, 0};
    request(0x44, empty, 2);
    CHECK(packet[5] == 0xFF && packet[6] == 0xFF);
}

static void test_painting_a_key(void)
{
    reset();

    // LED 0: pure red (hue 0, full saturation and value). Two LEDs in one packet.
    uint8_t set[3 + 6] = {0, 0, 2, 0, 255, 255, 85, 255, 255};

    memset(packet, 0, sizeof packet);
    packet[0] = 0x07;
    packet[1] = 0x42;
    memcpy(&packet[2], set, sizeof set);
    vialrgb_set_value(packet);

    uint8_t rgb[3];

    CHECK(vialrgb_colour(0, 0, rgb));
    CHECK(rgb[0] > 200 && rgb[1] < 40 && rgb[2] < 40); // red

    CHECK(vialrgb_colour(0, 1, rgb));
    CHECK(rgb[1] > 200 && rgb[0] < 40); // green (hue 85)

    CHECK(!vialrgb_colour(0, 2, rgb)); // nothing painted there
    CHECK(cfg[CFG_LED_EFFECT] == FX_VIAL); // painting shows the painting
}

static void test_saving_writes_it_down(void)
{
    reset();
    vialrgb_save();
    CHECK(saves == 1);
}

int main(void)
{
    test_the_info();
    test_the_effects_it_offers();
    test_the_mode_is_the_effect_of_the_profile();
    test_the_leds_and_their_places();
    test_painting_a_key();
    test_saving_writes_it_down();
    return tests_result();
}
