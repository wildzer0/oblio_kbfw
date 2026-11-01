#include "check.h"
#include "keycodes.h"
#include "led.h"
#include "led_effect.h"
#include "keymap.h"
#include <string.h>

// ---- the board's side ----
static uint16_t cfg_defaults[KEYMAP_CFG_COUNT] = {FX_SOLID, 160, 4, 200, 0};

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

uint16_t keymap_default_cfg(uint8_t field) { return cfg_defaults[field]; }

static bool    caps;
static uint8_t profile;

bool led_hook_caps_lock(void) { return caps; }
uint8_t led_hook_profile(void) { return profile; }

// ---- helpers ----
static void reset(uint16_t effect)
{
    cfg_defaults[CFG_LED_EFFECT] = effect;
    keymap_init();
    caps    = false;
    profile = 0;
    led_init();
    led_render_all();
}

// `n` whole frames, as the interrupt would play them (16 subframes each); the main loop looks after each one, as it does in the
// firmware (the interrupt counts frames in a byte)
static void frames(unsigned n)
{
    for (unsigned f = 0; f < n; f++) {
        for (unsigned c = 0; c < LED_COLS; c++) {
            led_next_column();
        }

        led_task();
    }
}

static void settle(void)
{
    led_render_all();
}

static void key_rgb(uint8_t row, uint8_t col, uint8_t out[3])
{
    for (uint8_t c = 0; c < 3; c++) {
        out[c] = led_level(col, (uint8_t)(row * 3 + c));
    }
}

static bool is(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t rgb[3];

    key_rgb(row, col, rgb);
    return rgb[0] == r && rgb[1] == g && rgb[2] == b;
}

static void test_the_wheel_and_the_effects(void)
{
    uint8_t rgb[3];

    led_color_wheel(0, rgb);
    CHECK(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0);
    led_color_wheel(85, rgb);
    CHECK(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 255);
    led_color_wheel(170, rgb);
    CHECK(rgb[0] == 0 && rgb[1] == 255 && rgb[2] == 0);
    led_color_wheel(84, rgb);
    CHECK(rgb[0] == 3 && rgb[2] == 252);

    led_effect_init();

    CHECK(led_effect_rgb(FX_SOLID, 0, 0, 7, 100, rgb) && rgb[0] == 100 && rgb[1] == 100 && rgb[2] == 100);
    CHECK(!led_effect_rgb((led_effect_t)FX_OFF, 0, 0, 0, 255, rgb));

    // pulse: dark at phase 0, brightest at phase 128, back to dark
    CHECK(led_effect_rgb(FX_PULSE, 0, 0, 0, 255, rgb) && rgb[0] == 0);
    CHECK(led_effect_rgb(FX_PULSE, 0, 0, 128, 255, rgb) && rgb[0] == 251);
    CHECK(led_effect_rgb(FX_PULSE, 0, 0, 64, 255, rgb) && rgb[0] < 251 && rgb[0] > 0);

    // breathe: every key the same colour, moving with the phase
    uint8_t a[3], b[3];
    led_effect_rgb(FX_BREATHE, 0, 0, 40, 255, a);
    led_effect_rgb(FX_BREATHE, 4, 15, 40, 255, b);
    CHECK(memcmp(a, b, 3) == 0);
    led_effect_rgb(FX_BREATHE, 0, 0, 41, 255, b);
    CHECK(memcmp(a, b, 3) != 0);

    // horizontal depends on the column only, vertical on the row only
    CHECK(led_effect_index(FX_HORIZONTAL, 0, 3, 0) == led_effect_index(FX_HORIZONTAL, 4, 3, 0));
    CHECK(led_effect_index(FX_HORIZONTAL, 0, 3, 0) != led_effect_index(FX_HORIZONTAL, 0, 4, 0));
    CHECK(led_effect_index(FX_VERTICAL, 2, 0, 0) == led_effect_index(FX_VERTICAL, 2, 9, 0));
    CHECK(led_effect_index(FX_VERTICAL, 1, 0, 0) != led_effect_index(FX_VERTICAL, 2, 0, 0));
    CHECK(led_effect_index(FX_DIAGONAL, 1, 1, 0) == (uint8_t)(led_effect_index(FX_HORIZONTAL, 0, 1, 0) + led_effect_index(FX_VERTICAL, 1, 0, 0)));

    // radial: the centre of the board is the nearest, the corners the farthest
    const uint8_t centre = led_effect_index(FX_RADIAL, 2, 7, 0);
    CHECK(centre < led_effect_index(FX_RADIAL, 0, 0, 0) && centre < led_effect_index(FX_RADIAL, 4, 15, 0));
    CHECK(led_effect_index(FX_RADIAL, 0, 0, 0) == 255 && led_effect_index(FX_RADIAL, 4, 15, 0) == 255); // the corners are the farthest
    CHECK(led_effect_index(FX_RADIAL, 2, 7, 0) == led_effect_index(FX_RADIAL, 2, 8, 0)); // the centre falls between columns 7 and 8
}

static void test_solid_white_fills_the_board(void)
{
    reset(FX_SOLID);

    for (uint8_t row = 0; row < LED_ROWS; row++) {
        for (uint8_t col = 0; col < LED_COLS; col++) {
            if (!(row == 3 && col == 1)) { // the profile's key is flashing
                CHECK(is(row, col, 160, 160, 160));
            }
        }
    }

    for (uint8_t col = 0; col < LED_COLS; col++) {
        CHECK(led_column_lit(col));
    }
}

static void test_off_is_dark(void)
{
    reset(FX_OFF);

    for (uint8_t col = 0; col < LED_COLS; col++) {
        for (uint8_t ch = 0; ch < LED_CHANNELS; ch++) {
            if (!(col == 1 && ch >= 9 && ch < 12)) { // except the profile's flash
                CHECK(led_level(col, ch) == 0);
            }
        }
    }

    CHECK(!led_column_lit(0) && !led_column_lit(5));
    CHECK(led_column_lit(1)); // the profile's flash lights its column
    frames(LED_PROFILE_FLASH + 2);
    settle();
    CHECK(!led_column_lit(1));
}

static void test_plain_effect_in_a_colour(void)
{
    reset(FX_SOLID);
    cfg_defaults[CFG_LED_COLOR] = 0;
    keymap_cfg_set(CFG_LED_COLOR, 6); // the wheel at (6 - 1) * 16 = 80
    settle();

    uint8_t wheel[3];
    led_color_wheel(80, wheel);
    CHECK(is(0, 5, (uint8_t)((wheel[0] * 160u) >> 8), (uint8_t)((wheel[1] * 160u) >> 8), (uint8_t)((wheel[2] * 160u) >> 8)));
}

static void test_widths_stay_inside_the_pwm_period(void)
{
    CHECK(LED_WIDTH(0) == 0);
    CHECK(LED_WIDTH(255) == 892);
    CHECK(LED_WIDTH(255) + 195 + LED_CHANNELS < 1200); // DUTY1 (195 + channel) + width below the 1200-count period
}

static void test_the_column_sequence_and_frames(void)
{
    reset(FX_SOLID);

    for (unsigned round = 0; round < 3; round++) {
        for (unsigned c = 0; c < LED_COLS; c++) {
            CHECK(led_next_column() == c);
        }
    }
}

static void test_the_animation_steps_every_nine_frames(void)
{
    reset(FX_HORIZONTAL);
    cfg_defaults[CFG_LED_SPEED] = 4;
    keymap_cfg_set(CFG_LED_SPEED, 4);

    uint8_t before[3], after[3];

    key_rgb(0, 5, before);
    frames(LED_ANIM_FRAMES - 1);
    settle();
    key_rgb(0, 5, after);
    CHECK(memcmp(before, after, 3) == 0); // not yet

    frames(1);
    settle();
    key_rgb(0, 5, after);
    CHECK(memcmp(before, after, 3) != 0); // the phase moved by the speed
}

static void test_caps_lock_lights_its_key(void)
{
    reset(FX_OFF);
    CHECK(is(2, 0, 0, 0, 0));
    caps = true;
    led_task();
    settle();
    CHECK(is(2, 0, 255, 255, 255));
    caps = false;
    settle();
    CHECK(is(2, 0, 0, 0, 0));
}

static void test_the_profile_key_flashes_then_goes(void)
{
    reset(FX_OFF);
    CHECK(is(3, 1, 255, 255, 255)); // profile 0 = the key in column 1 of the shift row (Z)

    frames(LED_PROFILE_FLASH - 1);
    settle();
    CHECK(is(3, 1, 255, 255, 255));
    frames(1);
    settle();
    CHECK(is(3, 1, 0, 0, 0));

    profile = 2; // another profile: its key (column 3) shows
    settle();
    CHECK(is(3, 3, 255, 255, 255) && is(3, 1, 0, 0, 0));
}

static void test_the_radio_key_takes_the_inverse_colour(void)
{
    reset(FX_SOLID);
    frames(LED_PROFILE_FLASH + 1);
    settle();

    led_flash_key(1, 2); // Bluetooth slot 2: the key 2, row 0 column 2
    settle();
    CHECK(is(0, 2, 95, 95, 95));  // 255 - 160
    CHECK(is(0, 3, 160, 160, 160));

    frames(LED_RADIO_FLASH);
    settle();
    CHECK(is(0, 2, 160, 160, 160));

    led_flash_key(0, 0); // 2.4G: key 4
    settle();
    CHECK(is(0, 4, 95, 95, 95));

    led_flash_key(3, 5); // a sleep step: keys 1-5
    settle();
    CHECK(is(0, 5, 95, 95, 95));
    led_flash_key(1, 9); // a slot that does not exist: the first key
    settle();
    CHECK(is(0, 1, 95, 95, 95));
}

static void test_pairing_blinks(void)
{
    reset(FX_SOLID);
    frames(LED_PROFILE_FLASH + 1);
    settle();

    led_flash_key(2, 1);
    settle();
    const bool first = is(0, 1, 95, 95, 95);

    frames(LED_RADIO_BLINK_HALF);
    settle();
    CHECK(is(0, 1, 95, 95, 95) != first); // the other half of the blink

    frames(LED_RADIO_PAIRING);
    settle();
    CHECK(is(0, 1, 160, 160, 160)); // after about ten seconds it stops
}

static void test_the_keys_of_the_lighting(void)
{
    reset(FX_RADIAL);

    led_effect_next();
    CHECK(keymap_cfg_get(CFG_LED_EFFECT) == FX_HORIZONTAL);
    led_effect_prev();
    led_effect_prev();
    CHECK(keymap_cfg_get(CFG_LED_EFFECT) == FX_OFF); // wraps to "off"
    led_effect_next();
    CHECK(keymap_cfg_get(CFG_LED_EFFECT) == FX_RADIAL);

    keymap_cfg_set(CFG_LED_BRIGHT, 250);
    led_brightness_up();
    CHECK(keymap_cfg_get(CFG_LED_BRIGHT) == 255);
    keymap_cfg_set(CFG_LED_BRIGHT, 20);
    led_brightness_down();
    CHECK(keymap_cfg_get(CFG_LED_BRIGHT) == 0);
    keymap_cfg_set(CFG_LED_BRIGHT, 100);
    led_brightness_up();
    CHECK(keymap_cfg_get(CFG_LED_BRIGHT) == 132);

    keymap_cfg_set(CFG_LED_SPEED, LED_SPEED_MAX);
    led_speed_up();
    CHECK(keymap_cfg_get(CFG_LED_SPEED) == LED_SPEED_MAX);
    keymap_cfg_set(CFG_LED_SPEED, LED_SPEED_MIN);
    led_speed_down();
    CHECK(keymap_cfg_get(CFG_LED_SPEED) == LED_SPEED_MIN);
    led_speed_up();
    CHECK(keymap_cfg_get(CFG_LED_SPEED) == LED_SPEED_MIN + 1);

    keymap_cfg_set(CFG_LED_COLOR, LED_COLORS - 1);
    led_color_up();
    CHECK(keymap_cfg_get(CFG_LED_COLOR) == 0);
    led_color_down();
    CHECK(keymap_cfg_get(CFG_LED_COLOR) == LED_COLORS - 1);
}

static void test_settings_out_of_range_are_tamed(void)
{
    reset(FX_SOLID);
    keymap_cfg_set(CFG_LED_EFFECT, 900); // a damaged profile: "off"
    keymap_cfg_set(CFG_LED_BRIGHT, 4000);
    settle();
    CHECK(!led_column_lit(5)); // 900 counts as off

    keymap_cfg_set(CFG_LED_EFFECT, FX_SOLID);
    settle();
    CHECK(is(0, 5, 255, 255, 255)); // 4000 counts as full brightness
}

static void test_a_repaint_is_done_in_pieces(void)
{
    reset(FX_SOLID);
    keymap_cfg_set(CFG_LED_BRIGHT, 96);
    led_brightness_down(); // 64, and a repaint is due
    led_task(); // eight LEDs, not the whole board
    unsigned changed = 0;

    for (uint8_t row = 0; row < LED_ROWS; row++) {
        for (uint8_t col = 0; col < LED_COLS; col++) {
            changed += (led_level(col, (uint8_t)(row * 3)) == 64);
        }
    }

    CHECK(changed > 0 && changed <= 8 + 2);
    settle();
    CHECK(is(0, 5, 64, 64, 64));
}

int main(void)
{
    test_the_wheel_and_the_effects();
    test_solid_white_fills_the_board();
    test_off_is_dark();
    test_plain_effect_in_a_colour();
    test_widths_stay_inside_the_pwm_period();
    test_the_column_sequence_and_frames();
    test_the_animation_steps_every_nine_frames();
    test_caps_lock_lights_its_key();
    test_the_profile_key_flashes_then_goes();
    test_the_radio_key_takes_the_inverse_colour();
    test_pairing_blinks();
    test_the_keys_of_the_lighting();
    test_settings_out_of_range_are_tamed();
    test_a_repaint_is_done_in_pieces();
    return tests_result();
}
