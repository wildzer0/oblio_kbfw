#include "led_effect.h"
#include "matrix.h"

// The centre of the board, in half-cells (so that it can fall between two keys): the middle of the 16 columns and of the 5 rows
#define CENTRE_X2 (MATRIX_COLS - 1)
#define CENTRE_Y2 (MATRIX_ROWS - 1)

static uint8_t radial[MATRIX_ROWS][MATRIX_COLS];
static uint8_t axis_x[MATRIX_COLS];
static uint8_t axis_y[MATRIX_ROWS];

static uint8_t isqrt(uint16_t n)
{
    uint8_t r = 0;

    while ((uint16_t)((r + 1u) * (r + 1u)) <= n) {
        r++;
    }

    return r;
}

void led_effect_init(void)
{
    uint8_t farthest = 1;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            const int16_t dx = (int16_t)(2 * col) - CENTRE_X2;
            const int16_t dy = (int16_t)(2 * row) - CENTRE_Y2;

            radial[row][col] = isqrt((uint16_t)(dx * dx + dy * dy));

            if (radial[row][col] > farthest) {
                farthest = radial[row][col];
            }
        }
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) { // distance -> 0-255
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            radial[row][col] = (uint8_t)((uint16_t)radial[row][col] * 255u / farthest);
        }
    }

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        axis_x[col] = (uint8_t)(((uint16_t)col << 8) / MATRIX_COLS);
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        axis_y[row] = (uint8_t)(((uint16_t)row << 8) / MATRIX_ROWS);
    }
}

uint8_t led_effect_index(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase)
{
    switch (fx) {
        case FX_HORIZONTAL:
            return (uint8_t)(axis_x[col] + phase);
        case FX_VERTICAL:
            return (uint8_t)(axis_y[row] + phase);
        case FX_DIAGONAL:
            return (uint8_t)(axis_x[col] + axis_y[row] + phase);
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
