#include "led.h"
#include "keymap.h"
#include "led_effect.h"
#include <string.h>

#define RENDER_CHUNK 8u // LEDs painted per pass of the main loop

static uint8_t level[LED_COLS][LED_CHANNELS]; // [column][row * 3 + colour]
static uint8_t lit[LED_COLS];                 // channels of the column that are not dark
static uint16_t duty2[LED_COLS][LED_CHANNELS]; // the same levels as the DUTY2 values the interrupt writes: no arithmetic there

// The interrupt's: the column being played and the frames counted
static uint8_t          next_column;
static volatile uint8_t frames;

// The main loop's
static uint8_t last_frames;
static uint8_t phase;     // animation phase
static uint8_t anim_ctr;  // frames since the last step of the phase
static uint8_t paint_row; // the LED the repaint in progress is at
static uint8_t paint_col;
static uint8_t paint_left; // LEDs still to paint in the repaint in progress
static bool    dirty;      // something changed: a repaint is due

static uint16_t radio_flash;   // frames left of the radio / sleep feedback
static uint8_t  radio_col;     // its key: row 0, this column
static bool     radio_blink;

static uint16_t battery_frames; // frames left of the battery display
static uint8_t  battery_keys;   // keys lit, 1-10; 0 = no reading
static bool     battery_low;
static bool     last_caps;

void led_init(void)
{
    memset(level, 0, sizeof level);
    memset(lit, 0, sizeof lit);

    for (uint8_t col = 0; col < LED_COLS; col++) {
        for (uint8_t n = 0; n < LED_CHANNELS; n++) {
            duty2[col][n] = (uint16_t)(LED_DUTY1_BASE + n); // no width: dark
        }
    }

    next_column   = 0;
    frames        = 0;
    last_frames   = 0;
    phase         = 0;
    anim_ctr      = 0;
    paint_row     = 0;
    paint_col     = 0;
    paint_left    = 0;
    radio_flash   = 0;
    battery_frames = 0;
    last_caps     = false;
    dirty         = true;
}

uint8_t led_next_column(void)
{
    const uint8_t column = next_column;

    if (++next_column >= LED_COLS) {
        next_column = 0;
        frames++;
    }

    return column;
}

bool led_column_lit(uint8_t column)
{
    return lit[column] != 0;
}

XDATA const uint16_t *led_duty2(uint8_t column)
{
    return duty2[column];
}

uint8_t led_level(uint8_t column, uint8_t channel)
{
    return level[column][channel];
}

// ---- the settings of the profile in use, checked as they are read: the profile can change at any time ----
static uint8_t effect(void)
{
    const uint16_t fx = keymap_cfg_get(CFG_LED_EFFECT);

    return (fx > FX_OFF) ? FX_OFF : (uint8_t)fx;
}

static uint8_t brightness(void)
{
    const uint16_t v = keymap_cfg_get(CFG_LED_BRIGHT);

    return (v > 255) ? 255 : (uint8_t)v;
}

static uint8_t speed(void)
{
    const uint16_t v = keymap_cfg_get(CFG_LED_SPEED);

    return (v < LED_SPEED_MIN) ? LED_SPEED_MIN : (v > LED_SPEED_MAX) ? LED_SPEED_MAX : (uint8_t)v;
}

static uint8_t color(void)
{
    const uint16_t c = keymap_cfg_get(CFG_LED_COLOR);

    return (c >= LED_COLORS) ? 0 : (uint8_t)c;
}

static void set(uint8_t field, uint16_t value)
{
    keymap_cfg_set(field, value);
    dirty = true;
}

// ---- painting ----
// The settings of the profile in use, read once per repaint: a repaint paints 8 LEDs per pass of the main loop, and reading them from the
// keymap (with the range check) for each of the three channels of each LED was most of what a pass cost. A change during the repaint asks
// for another one (set() sets dirty), so nothing is left painted with the old settings.
static uint8_t cur_effect;
static uint8_t cur_brightness;
static uint8_t cur_color;

static void read_settings(void)
{
    cur_effect     = effect();
    cur_brightness = brightness();
    cur_color      = color();
}

static void paint_one(void)
{
    uint8_t rgb[3] = {0, 0, 0};

    if (cur_effect == FX_SOLID && cur_color != 0) {
        led_color_wheel((uint8_t)((cur_color - 1u) * 16u), rgb);

        for (uint8_t c = 0; c < 3; c++) {
            rgb[c] = (uint8_t)(((uint16_t)rgb[c] * cur_brightness) >> 8);
        }
    } else if (!led_effect_rgb((led_effect_t)cur_effect, paint_row, paint_col, phase, cur_brightness, rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0; // the effect is off
    }

    // Caps lock: the Caps key (row 2, column 0) is solid white while it is on
    if (paint_row == 2 && paint_col == 0 && led_hook_caps_lock()) {
        rgb[0] = rgb[1] = rgb[2] = 255;
    }

    if (radio_flash && paint_row == 0 && paint_col == radio_col && (!radio_blink || ((radio_flash / LED_RADIO_BLINK_HALF) & 1u))) {
        rgb[0] = (uint8_t)(255u - rgb[0]);
        rgb[1] = (uint8_t)(255u - rgb[1]);
        rgb[2] = (uint8_t)(255u - rgb[2]);
    }

    // The battery display is on top of everything else on the number keys
    if (battery_frames && paint_row == 0 && paint_col >= 1 && paint_col <= 10) {
        if (battery_keys == 0) { // no reading: all ten blink in white
            if ((battery_frames / LED_RADIO_BLINK_HALF) & 1u) {
                rgb[0] = rgb[1] = rgb[2] = 255;
            }
        } else if (paint_col <= battery_keys) {
            rgb[0] = (battery_low) ? 255 : 0;
            rgb[1] = (battery_low) ? 0 : 255;
            rgb[2] = 0;
        }
    }

    for (uint8_t c = 0; c < 3; c++) {
        const uint8_t i   = (uint8_t)(paint_row * 3 + c);
        const uint8_t old = level[paint_col][i];

        if ((old == 0) != (rgb[c] == 0)) {
            lit[paint_col] = (uint8_t)(lit[paint_col] + (rgb[c] ? 1 : -1));
        }

        level[paint_col][i] = rgb[c];
        duty2[paint_col][i] = (uint16_t)(LED_DUTY1_BASE + i + LED_WIDTH(rgb[c]));
    }

    if (++paint_col >= LED_COLS) {
        paint_col = 0;

        if (++paint_row >= LED_ROWS) {
            paint_row = 0;
        }
    }
}

// One frame has gone by: the time of the animation and of the flashes
static void frame_passed(void)
{
    if (radio_flash) {
        radio_flash--;

        if (radio_flash == 0 || (radio_blink && radio_flash % LED_RADIO_BLINK_HALF == 0)) {
            dirty = true;
        }
    }

    if (battery_frames) {
        battery_frames--;

        if (battery_frames == 0 || (battery_keys == 0 && battery_frames % LED_RADIO_BLINK_HALF == 0)) {
            dirty = true;
        }
    }

    if (++anim_ctr >= LED_ANIM_FRAMES) {
        anim_ctr = 0;
        phase    = (uint8_t)(phase + speed());
        dirty    = true;
    }
}

void led_task(void)
{
    uint8_t seen;

    seen = frames; // one byte: read atomically
    while (last_frames != seen) {
        last_frames++;
        frame_passed();
    }

    // Caps lock changes outside the animation: repaint when it does
    const bool caps = led_hook_caps_lock();

    if (caps != last_caps) {
        last_caps = caps;
        dirty     = true;
    }

    if (paint_left == 0) {
        if (!dirty) {
            return;
        }

        dirty      = false; // cleared first: a step of the phase during the repaint asks for another
        paint_left = (uint8_t)(LED_ROWS * LED_COLS);

        read_settings(); // once for the whole repaint, not once per LED
    }

    for (uint8_t i = 0; i < RENDER_CHUNK && paint_left; i++, paint_left--) {
        paint_one();
    }
}

void led_refresh(void)
{
    dirty = true;
}

void led_show_battery(uint8_t percent)
{
    if (percent == LED_BATTERY_UNKNOWN || percent > 100) {
        battery_keys = 0;
        battery_low  = false;
    } else {
        battery_keys = (uint8_t)((percent + 9u) / 10u);
        battery_keys = (battery_keys == 0) ? 1 : battery_keys; // even an empty battery shows its first key
        battery_low  = percent <= LED_BATTERY_LOW;
    }

    battery_frames = LED_BATTERY_FRAMES;
    dirty          = true;
}

void led_flash_key(uint8_t kind, uint8_t index)
{
    radio_blink = (kind == 2);

    if (kind == 0) {
        radio_col = 4;
    } else if (index >= 1 && index <= (kind == 3 ? 5 : 3)) { // kind 3: a sleep-time step, keys 1-5
        radio_col = index;
    } else {
        radio_col = 1;
    }

    radio_flash = (kind == 2) ? LED_RADIO_PAIRING : LED_RADIO_FLASH;
    dirty       = true;
}

// ---- the keys of the lighting ----
void led_effect_next(void)
{
    const uint8_t fx = effect();

    set(CFG_LED_EFFECT, (fx >= FX_OFF) ? 0 : (uint8_t)(fx + 1));
}

void led_effect_prev(void)
{
    const uint8_t fx = effect();

    set(CFG_LED_EFFECT, (fx == 0) ? FX_OFF : (uint8_t)(fx - 1));
}

void led_brightness_up(void)
{
    const uint8_t v = brightness();

    set(CFG_LED_BRIGHT, (v > (uint8_t)(255 - LED_BRIGHTNESS_STEP)) ? 255 : (uint8_t)(v + LED_BRIGHTNESS_STEP));
}

void led_brightness_down(void)
{
    const uint8_t v = brightness();

    set(CFG_LED_BRIGHT, (v < LED_BRIGHTNESS_STEP) ? 0 : (uint8_t)(v - LED_BRIGHTNESS_STEP));
}

void led_color_up(void)
{
    const uint8_t c = color();

    set(CFG_LED_COLOR, (c + 1u >= LED_COLORS) ? 0 : (uint8_t)(c + 1u));
}

void led_color_down(void)
{
    const uint8_t c = color();

    set(CFG_LED_COLOR, (c == 0) ? (LED_COLORS - 1u) : (uint8_t)(c - 1u));
}

void led_speed_up(void)
{
    const uint8_t v = speed();

    if (v < LED_SPEED_MAX) {
        set(CFG_LED_SPEED, (uint8_t)(v + 1));
    }
}

void led_speed_down(void)
{
    const uint8_t v = speed();

    if (v > LED_SPEED_MIN) {
        set(CFG_LED_SPEED, (uint8_t)(v - 1));
    }
}
