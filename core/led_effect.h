#pragma once

// Colours of the backlight effects, per key. Pure maths: the framebuffer and the timing are in led.h.

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FX_RADIAL = 0, // rings spreading from the centre of the board
    FX_HORIZONTAL, // a wave across the columns
    FX_VERTICAL,   // a wave across the rows
    FX_SOLID,      // static, no animation (white, or one colour of the wheel)
    FX_DIAGONAL,   // a wave across columns and rows together
    FX_BREATHE,    // the whole board in one colour that cycles
    FX_PULSE,      // the whole board white, fading in and out
    FX_COUNT
} led_effect_t;

#define FX_OFF FX_COUNT // the effect number of "no backlight"

// Fills the distance tables (the radial effect is a function of the distance to the centre of the board).
void led_effect_init(void);

// Position of key (row, col) on the colour wheel for an effect, 0-255.
uint8_t led_effect_index(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase);

// The colour wheel: red -> blue -> green -> red, `index` 0-255
void led_color_wheel(uint8_t index, uint8_t out[3]);

// The colour of a key, scaled by `brightness`. False when the effect is off (out is then left alone).
bool led_effect_rgb(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase, uint8_t brightness, uint8_t out[3]);
