#pragma once

// The RGB backlight: a framebuffer of 8-bit levels, rendered from the effect of the profile in use, and the sequence of the 16
// column subframes the hardware plays.
//
// Every key has an RGB LED. A LED lights when its colour channel (a PWM output) is high AND the strobe line of its column is low, so
// the strobes of the key matrix are also the sinks of the LEDs: all the keys of one column (5 rows x R, G, B = 15 channels) are lit by
// one subframe, and a frame is 16 subframes interleaved with the key scans. The interrupt (board side) asks led_next_column() what to
// light and reads the levels; everything that takes time (the effects) is done in led_task() from the main loop, a few LEDs per
// pass, so that a repaint never holds up the keys.

#include "matrix.h"
#include <stdbool.h>
#include <stdint.h>

#define LED_ROWS     MATRIX_ROWS
#define LED_COLS     MATRIX_COLS
#define LED_CHANNELS (LED_ROWS * 3) // per column: row 0 R G B, row 1 R G B, ...

// 8-bit level -> pulse width in PWM counts: 3.5 per step, 892 at most, so that DUTY2 stays below the 1200-count period
#define LED_WIDTH(level) ((uint16_t)((uint16_t)(level) * 3u + ((uint16_t)(level) >> 1)))

// Frames (about 8 ms each) per step of the animation, and the lengths of the key flashes, in frames
#define LED_ANIM_FRAMES       9u
#define LED_PROFILE_FLASH     60u   // ~0.5 s: the profile's key (Z X C V) is white
#define LED_RADIO_FLASH       62u   // ~0.5 s
#define LED_RADIO_PAIRING     1250u // ~10 s of blinking
#define LED_RADIO_BLINK_HALF  30u   // half a period of that blink

#define LED_SPEED_MIN 1
#define LED_SPEED_MAX 16
#define LED_BRIGHTNESS_STEP 32
#define LED_COLORS 17u // plain effect: 0 = white, 1-16 = the wheel in steps of 16

// Provided by the board (or a test).
bool    led_hook_caps_lock(void); // the host's caps lock is on
uint8_t led_hook_profile(void);   // the profile in use

void led_init(void);

// ---- for the interrupt ----
// The column to light in this subframe; moves on to the next one and counts a frame when it wraps. Constant time.
uint8_t led_next_column(void);

// Whether any channel of the column is lit (a dark column is not strobed).
bool    led_column_lit(uint8_t column);
uint8_t led_level(uint8_t column, uint8_t channel);

// ---- for the main loop ----
void led_task(void);

// Radio / sleep feedback on the number row: kind 0 = 2.4G (key 4), 1 = Bluetooth slot `index` (1-3), 2 = pairing of slot `index`
// (the key blinks), 3 = a sleep-time step `index` (keys 1-5). The key takes the INVERSE of its colour, so that it shows against a
// backlight of any colour.
void led_flash_key(uint8_t kind, uint8_t index);

// The battery level on the number keys 1 2 ... 9 0 (row 0, columns 1-10) for about 3 s: n keys lit with n = ceil(percent / 10), so that
// 1 means about 10% and 0 (the tenth) means full, green above LED_BATTERY_LOW and red at or below it. LED_BATTERY_UNKNOWN (no reading)
// blinks all ten in white. A second call renews the display.
#define LED_BATTERY_UNKNOWN 0xFFu
#define LED_BATTERY_LOW     20u    // percent: at or below this the keys are red
#define LED_BATTERY_FRAMES  366u   // ~3 s
void led_show_battery(uint8_t percent);

// The keys of the lighting: they change the setting of the profile in use.
void led_effect_next(void);
void led_effect_prev(void);
void led_brightness_up(void);
void led_brightness_down(void);
void led_speed_up(void);
void led_speed_down(void);
void led_color_up(void);
void led_color_down(void);

// Everything at once, for the tests and for the moment the board turns the LEDs on: a repaint of the whole board is made at once.
void led_render_all(void);
