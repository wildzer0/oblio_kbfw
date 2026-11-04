#include "led.h"
#include "keymap.h"
#include "led_effect.h"
#include "keyboard.h"
#include "options.h"
#if LOOP_PROFILE
#    include "profile.h"
#endif
#include <string.h>

#define RENDER_CHUNK 8u // LEDs painted per pass of the main loop

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
static uint8_t  battery_charge; // 0: the level, 1: the cell is charging (ten cyan keys), 2: the charge is done (ten green keys)
static bool     last_caps;

void led_init(void)
{
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

// The level of a channel, worked back from its duty value (the frame buffer keeps only the duty values: a second table of the levels was 240 bytes of
// the 4 KB of XDATA and the interrupt never reads it). LED_WIDTH() grows with every level, so there is one answer. Only the host tests ask for it, so
// the firmware does not carry it (__SDCC is defined by the chip's compiler only).
#ifndef __SDCC
uint8_t led_level(uint8_t column, uint8_t channel)
{
    const uint16_t width = (uint16_t)(duty2[column][channel] - (uint16_t)(LED_DUTY1_BASE + channel));
    uint8_t        level = 0;

    while (level < 255u && LED_WIDTH(level) < width) {
        level++;
    }

    return level;
}
#endif

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

static uint8_t hue(void)
{
    return (uint8_t)(keymap_cfg_get(CFG_LED_COLOR) & 0xFFu);
}

static uint8_t saturation(void)
{
    return (uint8_t)(keymap_cfg_get(CFG_LED_COLOR) >> 8);
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
static uint8_t cur_color; // the hue
static uint8_t cur_sat;   // its saturation

static XDATA uint8_t rgb[3]; // static, in external RAM: the effect functions take XDATA pointers, which are faster than the generic ones a local array needs

// Some effects give every key the same colour (the solid colour, the breathing, the pulse): that colour is worked out once for the whole repaint
// and copied to each LED, instead of 80 times.
static bool            uniform;
static XDATA uint8_t   base[3];

// The waves repeat their colours: a repaint goes row by row, so the colours of the first row are the colours of all the others (horizontal wave),
// the colour of the first key of a row is that of the whole row (vertical wave), and the keys of one ring of the radial effect have one colour.
// Kept while the repaint goes on; `color_known` says which of the 16 entries of `color` have been worked out (rings: one bit each).
static XDATA uint8_t   color[LED_COLS][3];
static uint16_t        color_known;

// The phase the repaint in progress is painted with: it is taken at its start (the phase may step while the repaint goes on; that asks for another repaint)
static uint8_t         cur_phase;

static void read_settings(void)
{
    cur_effect     = effect();
    cur_brightness = brightness();
    cur_color      = hue();
    cur_sat        = saturation();

    cur_phase   = phase;
    color_known = 0;

    uniform = (cur_effect == FX_SOLID || cur_effect == FX_BREATHE || cur_effect == FX_PULSE);

    if (cur_effect == FX_SOLID) { // the colour of the profile: a hue and a saturation, at the brightness of the profile
        led_color_hsv(cur_color, cur_sat, cur_brightness, base);
    } else if (uniform) { // the position of the key does not matter to these two
        led_effect_rgb((led_effect_t)cur_effect, 0, 0, cur_phase, cur_brightness, base);
    }
}

// The Caps key is lit by the host's Caps Lock and by Caps Word
static bool caps_on(void)
{
    return led_hook_caps_lock() || keyboard_caps_word();
}

// The colour already worked out for this key, into the one being painted
static void take(XDATA const uint8_t *from)
{
    rgb[0] = from[0];
    rgb[1] = from[1];
    rgb[2] = from[2];
}

static void paint_one(void)
{
    if (uniform) {
        take(base);
    } else if (cur_effect == FX_HORIZONTAL || cur_effect == FX_VERTICAL) {
        XDATA uint8_t *known;

        if (cur_effect == FX_HORIZONTAL) { // by column: worked out on the first row, the same on the others
            known = color[paint_col];

            if (paint_row == 0) {
                led_effect_rgb(FX_HORIZONTAL, 0, paint_col, cur_phase, cur_brightness, known);
            }
        } else { // by row: worked out on the first key of the row
            known = base; // (vertical wave and the uniform effects are never both on)

            if (paint_col == 0) {
                led_effect_rgb(FX_VERTICAL, paint_row, 0, cur_phase, cur_brightness, known);
            }
        }

        take(known);
    } else if (cur_effect == FX_RADIAL) { // by ring: worked out by the first key met of the ring
        const uint8_t ring = led_effect_ring(paint_row, paint_col);

        if (!((color_known >> ring) & 1u)) {
            color_known |= (uint16_t)(1u << ring);
            led_effect_rgb(FX_RADIAL, paint_row, paint_col, cur_phase, cur_brightness, color[ring]);
        }

        take(color[ring]);
    } else if (!led_effect_rgb((led_effect_t)cur_effect, paint_row, paint_col, cur_phase, cur_brightness, rgb)) {
        rgb[0] = rgb[1] = rgb[2] = 0; // the effect is off
    }

    // Caps lock: the Caps key (row 2, column 0) is solid white while it is on
    if (paint_row == 2 && paint_col == 0 && caps_on()) {
        rgb[0] = rgb[1] = rgb[2] = 255;
    }

    if (radio_flash && paint_row == 0 && paint_col == radio_col && (!radio_blink || ((radio_flash / LED_RADIO_BLINK_HALF) & 1u))) {
        rgb[0] = (uint8_t)(255u - rgb[0]);
        rgb[1] = (uint8_t)(255u - rgb[1]);
        rgb[2] = (uint8_t)(255u - rgb[2]);
    }

    // The battery display is on top of everything else on the number keys
    if (battery_frames && paint_row == 0 && paint_col >= 1 && paint_col <= 10) {
        if (battery_charge) { // the cable: no level to show
            rgb[0] = 0;
            rgb[1] = 255;
            rgb[2] = (battery_charge == 1) ? 255 : 0;
        } else if (battery_keys == 0) { // no reading: all ten blink in white
            if ((battery_frames / LED_RADIO_BLINK_HALF) & 1u) {
                rgb[0] = rgb[1] = rgb[2] = 255;
            }
        } else if (paint_col <= battery_keys) {
            rgb[0] = (battery_low) ? 255 : 0;
            rgb[1] = (battery_low) ? 0 : 255;
            rgb[2] = 0;
        }
    }

    // The three channels of the LED: one pointer to each table, walked, instead of the two-dimensional index worked out for every access
    XDATA uint16_t *du = &duty2[paint_col][paint_row * 3];
    uint8_t         duty_base = (uint8_t)(paint_row * 3 + LED_DUTY1_BASE);
    uint8_t         changed_lit = lit[paint_col];

    for (uint8_t c = 0; c < 3; c++, du++, duty_base++) {
        const uint8_t  v    = rgb[c];
        const uint16_t want = (uint16_t)(duty_base + LED_WIDTH(v));

        if (*du == want) {
            continue; // the same width as before
        }

        if ((*du == duty_base) != (v == 0)) { // it was dark (width 0) and is not, or the other way round
            changed_lit = (uint8_t)(changed_lit + (v ? 1 : -1));
        }

        *du = want;
    }

    lit[paint_col] = changed_lit;

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

// The part of led_task that paints, timed in the profile build (core/profile.h, step 16); nothing in the others. (It had probes on the other parts
// too, steps 16-18: they were 30-40 us each and the flash of the profile build is full.)
#if LOOP_PROFILE
#    define PROBE(id, code)                                    \
        do {                                                   \
            const uint16_t probe_ = profile_hook_now();        \
            code;                                              \
            profile_end(id, probe_);                       \
        } while (0)
#else
#    define PROBE(id, code) code
#endif

void led_task(void)
{
    uint8_t seen;

    seen = frames; // one byte: read atomically
    while (last_frames != seen) {
        last_frames++;
        frame_passed();
    }

    // Caps lock changes outside the animation: repaint when it does
    bool caps = false;

    caps = caps_on(); // Caps Word lights the same key: its start and end repaint too

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

    PROBE(16, for (uint8_t i = 0; i < RENDER_CHUNK && paint_left; i++, paint_left--) {
        paint_one();
    });
}

void led_refresh(void)
{
    dirty = true;
}

void led_show_battery(uint8_t percent)
{
    battery_charge = 0;

    if (percent == LED_BATTERY_CHARGING || percent == LED_BATTERY_CHARGED) { // the cable: no level to show, all ten keys in one colour
        battery_charge = (percent == LED_BATTERY_CHARGING) ? 1 : 2;
        battery_keys   = 10; // (no reading would make them blink); the colour does not look at battery_low
    } else if (percent == LED_BATTERY_UNKNOWN || percent > 100) {
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

// The two keys of the colour walk the hue at full saturation, and white is the step after the last hue (as when the colour was one of seventeen)
static void set_color(uint8_t h, uint8_t s)
{
    set(CFG_LED_COLOR, (uint16_t)(((uint16_t)s << 8) | h));
}

void led_color_up(void)
{
    const uint8_t h = hue();

    if (saturation() == 0) {
        set_color(0, LED_SAT_FULL); // white -> the first colour
    } else if ((uint16_t)(h + LED_HUE_STEP) > 255u) {
        set_color(0, 0); // past the last one -> white
    } else {
        set_color((uint8_t)(h + LED_HUE_STEP), LED_SAT_FULL);
    }
}

void led_color_down(void)
{
    const uint8_t h = hue();

    if (saturation() == 0) {
        set_color((uint8_t)(255u - LED_HUE_STEP), LED_SAT_FULL); // white -> the last colour
    } else if (h < LED_HUE_STEP) {
        set_color(0, 0); // before the first one -> white
    } else {
        set_color((uint8_t)(h - LED_HUE_STEP), LED_SAT_FULL);
    }
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
