#include "led_effect.h"
#include "matrix.h"
#include "platform.h"

// The radial effect is a function of the distance of each key to the centre of the board, which falls between columns 7 and 8 and
// on the middle row. The table is that distance scaled to 0-255 (the farthest key, a corner, is 255). It is a table and not
// computed at start-up because that costs more flash than the table; test_led.c computes it again from the formula and compares:
//     d = isqrt((2 * col - (COLS - 1))^2 + (2 * row - (ROWS - 1))^2),  value = d * 255 / the largest d
static const ROM uint8_t radial[MATRIX_ROWS][MATRIX_COLS] = {
    {255, 221, 187, 153, 136, 102, 85, 68, 68, 85, 102, 136, 153, 187, 221, 255},
    {255, 221, 187, 153, 119, 85, 51, 34, 34, 51, 85, 119, 153, 187, 221, 255},
    {255, 221, 187, 153, 119, 85, 51, 17, 17, 51, 85, 119, 153, 187, 221, 255},
    {255, 221, 187, 153, 119, 85, 51, 34, 34, 51, 85, 119, 153, 187, 221, 255},
    {255, 221, 187, 153, 136, 102, 85, 68, 68, 85, 102, 136, 153, 187, 221, 255},
};

// The waves run along the columns (16 of them: 16 steps of the wheel each) and along the rows (5: (row * 256) / 5)
static const ROM uint8_t axis_y[MATRIX_ROWS] = {0, 51, 102, 153, 204};

static uint8_t axis_x(uint8_t col)
{
    return (uint8_t)(col << 4);
}

uint8_t led_effect_index(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase)
{
    switch (fx) {
        case FX_HORIZONTAL:
            return (uint8_t)(axis_x(col) + phase);
        case FX_VERTICAL:
            return (uint8_t)(axis_y[row] + phase);
        case FX_DIAGONAL:
            return (uint8_t)(axis_x(col) + axis_y[row] + phase);
        case FX_BREATHE:
            return phase;
        case FX_RADIAL:
        default:
            return (uint8_t)(radial[row][col] + phase);
    }
}

void led_color_wheel(uint8_t index, uint8_t out[3])
{
    if (index < 85) {
        out[0] = (uint8_t)(255 - index * 3);
        out[1] = 0;
        out[2] = (uint8_t)(index * 3);
    } else if (index < 170) {
        index  = (uint8_t)(index - 85);
        out[0] = 0;
        out[1] = (uint8_t)(index * 3);
        out[2] = (uint8_t)(255 - index * 3);
    } else {
        index  = (uint8_t)(index - 170);
        out[0] = (uint8_t)(index * 3);
        out[1] = (uint8_t)(255 - index * 3);
        out[2] = 0;
    }
}

static uint8_t scale(uint8_t value, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)value * brightness) >> 8);
}

bool led_effect_rgb(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase, uint8_t brightness, uint8_t out[3])
{
    if (fx == FX_SOLID) {
        out[0] = out[1] = out[2] = brightness; // white
        return true;
    }

    if (fx >= FX_OFF) {
        return false;
    }

    if (fx == FX_PULSE) {
        // a triangle wave of the phase, squared so that the dark end lasts
        const uint8_t triangle = (phase < 128) ? (uint8_t)(phase << 1) : (uint8_t)((uint8_t)(255 - phase) << 1);
        const uint8_t level    = (uint8_t)(((uint16_t)triangle * triangle) >> 8);

        out[0] = out[1] = out[2] = scale(level, brightness);
        return true;
    }

    led_color_wheel(led_effect_index(fx, row, col, phase), out);
    out[0] = scale(out[0], brightness);
    out[1] = scale(out[1], brightness);
    out[2] = scale(out[2], brightness);
    return true;
}
