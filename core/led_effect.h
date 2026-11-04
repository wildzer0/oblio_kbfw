#pragma once

// Colours of the backlight effects, per key. Pure maths: the framebuffer and the timing are in led.h.

#include "platform.h"
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


// Position of key (row, col) on the colour wheel for an effect, 0-255.
uint8_t led_effect_index(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase);

// Which ring of the radial effect the key is on, 1-15: keys of one ring have one colour at any phase (the rings are the distinct distances of the table)
uint8_t led_effect_ring(uint8_t row, uint8_t col);

// The colour wheel at full saturation and value: red -> yellow -> green -> cyan -> blue -> magenta, `index` 0-255
void led_color_wheel(uint8_t index, XDATA uint8_t *out);

// A colour as the picker of the Vial app gives it: hue 0-255, saturation 0-255 (0 = white) and value 0-255 (the brightness)
void led_color_hsv(uint8_t hue, uint8_t saturation, uint8_t value, XDATA uint8_t *out);

// The colour of a key, scaled by `brightness`. False when the effect is off (out is then left alone).
bool led_effect_rgb(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase, uint8_t brightness, XDATA uint8_t *out);
