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

uint16_t keymap_default_cfg(uint8_t field) { return cfg_defaults[field]; }

static bool caps;

bool led_hook_caps_lock(void) { return caps; }

// core/led.c asks the keyboard whether Caps Word is on (it lights the Caps key too): there is no keyboard in this test
static bool caps_word;

bool keyboard_caps_word(void) { return caps_word; }

// ---- helpers ----
// A full repaint: ask for it and let led_task() paint the 8 LEDs of a pass until it is done
static void render_all(void)
{
    led_refresh();

    for (unsigned i = 0; i < 2 * LED_ROWS * LED_COLS / 8; i++) {
        led_task();
    }
}

static void reset(uint16_t effect)
{
    cfg_defaults[CFG_LED_EFFECT] = effect;
    keymap_init();
    caps      = false;
    caps_word = false;
    led_init();
    render_all();
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
    render_all();
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

    // the wheel of an HSV picker: red, yellow, green, cyan, blue, magenta, red (the order the Vial app sends hues in)
    led_color_wheel(0, rgb);
    CHECK(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0); // red
    led_color_wheel(43, rgb);
    CHECK(rgb[0] == 255 && rgb[1] > 240 && rgb[2] == 0); // yellow
    led_color_wheel(85, rgb);
    CHECK(rgb[0] < 16 && rgb[1] == 255 && rgb[2] == 0); // green
    led_color_wheel(128, rgb);
    CHECK(rgb[0] == 0 && rgb[1] > 240 && rgb[2] > 240); // cyan
    led_color_wheel(170, rgb);
    CHECK(rgb[0] == 0 && rgb[1] < 16 && rgb[2] == 255); // blue
    led_color_wheel(213, rgb);
    CHECK(rgb[0] > 240 && rgb[1] == 0 && rgb[2] == 255); // magenta
    led_color_wheel(255, rgb);
    CHECK(rgb[0] == 255 && rgb[2] < 24); // and back towards red

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

// The table of the radial effect is the distance formula, scaled
static void test_the_radial_table_is_the_formula(void)
{
    unsigned d[LED_ROWS][LED_COLS];
    unsigned farthest = 1;

    for (int row = 0; row < LED_ROWS; row++) {
        for (int col = 0; col < LED_COLS; col++) {
            const int dx = 2 * col - (LED_COLS - 1);
            const int dy = 2 * row - (LED_ROWS - 1);
            unsigned  r  = 0;

            while ((r + 1) * (r + 1) <= (unsigned)(dx * dx + dy * dy)) {
                r++;
            }

            d[row][col] = r;
            if (r > farthest) {
                farthest = r;
            }
        }
    }

    for (uint8_t row = 0; row < LED_ROWS; row++) {
        for (uint8_t col = 0; col < LED_COLS; col++) {
            CHECK(led_effect_index(FX_RADIAL, row, col, 0) == d[row][col] * 255 / farthest);
        }
        CHECK(led_effect_index(FX_VERTICAL, row, 0, 0) == (uint8_t)((row << 8) / LED_ROWS));
    }

    for (uint8_t col = 0; col < LED_COLS; col++) {
        CHECK(led_effect_index(FX_HORIZONTAL, 0, col, 0) == (uint8_t)((col << 8) / LED_COLS));
    }
}

static void test_solid_white_fills_the_board(void)
{
    reset(FX_SOLID);

    for (uint8_t row = 0; row < LED_ROWS; row++) {
        for (uint8_t col = 0; col < LED_COLS; col++) {
            CHECK(is(row, col, 160, 160, 160));
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
            CHECK(led_level(col, ch) == 0);
        }
    }

    CHECK(!led_column_lit(0) && !led_column_lit(1) && !led_column_lit(5));
}

static void test_plain_effect_in_a_colour(void)
{
    reset(FX_SOLID);
    cfg_defaults[CFG_LED_COLOR] = 0;
    keymap_cfg_set(CFG_LED_COLOR, (uint16_t)(((uint16_t)LED_SAT_FULL << 8) | 80)); // hue 80, full saturation
    settle();

    uint8_t mixed[3];
    led_color_hsv(80, LED_SAT_FULL, 160, mixed);
    CHECK(is(0, 5, mixed[0], mixed[1], mixed[2]));
}

// The shortcuts in led_effect.c and led.h (no multiplication for 0 and 255, shifts for the width) give exactly what the plain formulas gave
static void test_the_fast_arithmetic_equals_the_plain_formulas(void)
{
    for (unsigned level = 0; level < 256; level++) {
        CHECK(LED_WIDTH(level) == (uint16_t)(level * 3u + (level >> 1)));
    }

    for (unsigned brightness = 0; brightness < 256; brightness++) {
        for (unsigned phase = 0; phase < 256; phase++) {
            uint8_t wheel[3];
            uint8_t got[3];

            led_color_wheel((uint8_t)phase, wheel);
            CHECK(led_effect_rgb(FX_BREATHE, 0, 0, (uint8_t)phase, (uint8_t)brightness, got)); // the whole board in one colour of the wheel

            for (unsigned c = 0; c < 3; c++) {
                CHECK(got[c] == (uint8_t)((wheel[c] * brightness) >> 8));
            }
        }
    }
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

static void test_the_radio_key_takes_the_inverse_colour(void)
{
    reset(FX_SOLID);
    frames(LED_ANIM_FRAMES + 1);
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
    frames(LED_ANIM_FRAMES + 1);
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

static bool is_green(uint8_t col) { return is(0, col, 0, 255, 0); }
static bool is_red(uint8_t col) { return is(0, col, 255, 0, 0); }

static unsigned battery_keys_lit(bool green)
{
    unsigned n = 0;

    for (uint8_t col = 1; col <= 10; col++) {
        n += green ? is_green(col) : is_red(col);
    }

    return n;
}

static void test_the_battery_level_on_the_number_keys(void)
{
    // percent -> keys lit (ceil(p / 10), at least one), green above 20%, red at 20% or below
    static const struct { uint8_t percent; unsigned keys; bool green; } cases[] = {
        {100, 10, true}, {95, 10, true}, {91, 10, true}, {90, 9, true}, {50, 5, true}, {21, 3, true},
        {20, 2, false}, {11, 2, false}, {10, 1, false}, {1, 1, false}, {0, 1, false},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        reset(FX_SOLID);
        frames(LED_ANIM_FRAMES + 1);
        led_show_battery(cases[i].percent);
        settle();

        CHECK(battery_keys_lit(cases[i].green) == cases[i].keys);
        CHECK(battery_keys_lit(!cases[i].green) == 0);

        // they are the first keys 1, 2, ... in order, and the rest of the number row is as it was
        for (uint8_t col = 1; col <= 10; col++) {
            if (col > cases[i].keys) {
                CHECK(is(0, col, 160, 160, 160));
            }
        }
    }
}

static void test_the_battery_display_ends_after_three_seconds_and_is_renewed(void)
{
    reset(FX_SOLID);
    frames(LED_ANIM_FRAMES + 1);
    led_show_battery(50);
    settle();
    CHECK(is_green(5));

    frames(LED_BATTERY_FRAMES - 5);
    settle();
    CHECK(is_green(5));

    led_show_battery(30); // a second tap: the time starts over, with the new level
    frames(LED_BATTERY_FRAMES - 5);
    settle();
    CHECK(is_green(3) && !is_green(4));

    frames(10);
    settle();
    CHECK(is(0, 3, 160, 160, 160)); // over
}

static void test_no_reading_blinks_all_ten_in_white(void)
{
    reset(FX_OFF);
    frames(LED_ANIM_FRAMES + 1);
    led_show_battery(LED_BATTERY_UNKNOWN);

    bool seen_on = false, seen_off = false;

    for (unsigned i = 0; i < 4; i++) {
        settle();

        bool all = true, none = true;

        for (uint8_t col = 1; col <= 10; col++) {
            all  = all && is(0, col, 255, 255, 255);
            none = none && is(0, col, 0, 0, 0);
        }

        CHECK(all || none); // always all ten together
        seen_on  = seen_on || all;
        seen_off = seen_off || none;
        frames(LED_RADIO_BLINK_HALF);
    }

    CHECK(seen_on && seen_off);
    CHECK(is(0, 0, 0, 0, 0) && is(0, 11, 0, 0, 0)); // and no other key
}

static void test_a_value_over_100_counts_as_no_reading(void)
{
    reset(FX_OFF);
    frames(LED_ANIM_FRAMES + 1);
    led_show_battery(101);
    CHECK(battery_keys_lit(true) == 0); // nothing painted yet
    frames(LED_RADIO_BLINK_HALF + 1);
    settle();
    CHECK(battery_keys_lit(true) == 0 && battery_keys_lit(false) == 0); // not a level
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

    // the colour keys walk the hue at full saturation, and white is the step after the last hue
    keymap_cfg_set(CFG_LED_COLOR, 0); // white
    led_color_up();
    CHECK(keymap_cfg_get(CFG_LED_COLOR) == ((uint16_t)LED_SAT_FULL << 8)); // hue 0
    led_color_down();
    CHECK(keymap_cfg_get(CFG_LED_COLOR) == 0); // and back to white

    keymap_cfg_set(CFG_LED_COLOR, (uint16_t)(((uint16_t)LED_SAT_FULL << 8) | 240)); // the last hue of the walk
    led_color_up();
    CHECK(keymap_cfg_get(CFG_LED_COLOR) == 0); // past it: white
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

static void test_caps_word_lights_the_caps_key_and_repaints_when_it_ends(void)
{
    reset(FX_OFF);
    caps_word = true;
    led_task(); // the change is seen by led_task: nothing else asked for a repaint
    settle();
    CHECK(is(2, 0, 255, 255, 255));
    caps_word = false;
    led_task();
    settle();
    CHECK(is(2, 0, 0, 0, 0));
}

// The effects that give every key one colour (solid, breathe, pulse) work the colour out once per repaint: every key must still get it,
// and the duty value the interrupt writes must follow the level (width = level * 3.5, on the channel's base)
static void test_the_uniform_effects_paint_every_key_alike_and_the_duty_follows(void)
{
    const led_effect_t uniform[] = {FX_SOLID, FX_BREATHE, FX_PULSE};

    for (unsigned u = 0; u < 3; u++) {
        reset(uniform[u]);
        frames(3 * LED_ANIM_FRAMES);
        settle();

        uint8_t first[3];

        key_rgb(0, 0, first);

        for (uint8_t row = 0; row < LED_ROWS; row++) {
            for (uint8_t col = 0; col < LED_COLS; col++) {
                if (row == 2 && col == 0) {
                    continue; // the Caps key
                }

                uint8_t rgb[3];

                key_rgb(row, col, rgb);
                CHECK(memcmp(first, rgb, 3) == 0);

                for (uint8_t c = 0; c < 3; c++) {
                    const uint8_t n = (uint8_t)(row * 3 + c);

                    CHECK(led_duty2(col)[n] == (uint16_t)(LED_DUTY1_BASE + n + LED_WIDTH(rgb[c])));
                }
            }
        }
    }
}

// The waves reuse the colours they have worked out (a column on every row, a row on every key): each key must still have the colour the effect
// gives it by itself, at the phase of the repaint, also after the phase has stepped
static void test_the_charge_display_on_the_cable(void)
{
    reset(FX_OFF);
    led_show_battery(LED_BATTERY_CHARGING); // charging: the ten number keys cyan
    settle();

    for (uint8_t col = 1; col <= 10; col++) {
        CHECK(is(0, col, 0, 255, 255));
    }

    CHECK(is(0, 0, 0, 0, 0) && is(0, 11, 0, 0, 0)); // and no others

    led_show_battery(LED_BATTERY_CHARGED); // done: all ten green
    settle();

    for (uint8_t col = 1; col <= 10; col++) {
        CHECK(is(0, col, 0, 255, 0));
    }

    led_show_battery(50); // the level display after it is the level again, not a leftover of the charge
    settle();
    CHECK(is(0, 5, 0, 255, 0) && is(0, 6, 0, 0, 0));

    frames(LED_BATTERY_FRAMES + 2); // and it ends by itself
    settle();
    CHECK(is(0, 3, 0, 0, 0));
}

static void test_the_waves_that_reuse_colours_match_the_plain_effect(void)
{
    const led_effect_t waves[] = {FX_HORIZONTAL, FX_VERTICAL, FX_DIAGONAL, FX_RADIAL};

    for (unsigned w = 0; w < 4; w++) {
        reset(waves[w]);
        frames(5 * LED_ANIM_FRAMES);
        settle();

        const uint8_t phase = (uint8_t)(5 * keymap_cfg_get(CFG_LED_SPEED)); // five steps of the phase, from zero

        for (uint8_t row = 0; row < LED_ROWS; row++) {
            for (uint8_t col = 0; col < LED_COLS; col++) {
                if (row == 2 && col == 0) {
                    continue; // the Caps key
                }

                uint8_t want[3];

                CHECK(led_effect_rgb(waves[w], row, col, phase, (uint8_t)keymap_cfg_get(CFG_LED_BRIGHT), want));
                CHECK(is(row, col, want[0], want[1], want[2]));
            }
        }
    }
}

int main(void)
{
    test_the_wheel_and_the_effects();
    test_the_radial_table_is_the_formula();
    test_solid_white_fills_the_board();
    test_off_is_dark();
    test_plain_effect_in_a_colour();
    test_the_fast_arithmetic_equals_the_plain_formulas();
    test_widths_stay_inside_the_pwm_period();
    test_the_column_sequence_and_frames();
    test_the_animation_steps_every_nine_frames();
    test_caps_lock_lights_its_key();
    test_the_radio_key_takes_the_inverse_colour();
    test_pairing_blinks();
    test_the_battery_level_on_the_number_keys();
    test_the_battery_display_ends_after_three_seconds_and_is_renewed();
    test_no_reading_blinks_all_ten_in_white();
    test_a_value_over_100_counts_as_no_reading();
    test_the_keys_of_the_lighting();
    test_settings_out_of_range_are_tamed();
    test_a_repaint_is_done_in_pieces();
    test_caps_word_lights_the_caps_key_and_repaints_when_it_ends();
    test_the_uniform_effects_paint_every_key_alike_and_the_duty_follows();
    test_the_waves_that_reuse_colours_match_the_plain_effect();
    test_the_charge_display_on_the_cable();
    return tests_result();
}
