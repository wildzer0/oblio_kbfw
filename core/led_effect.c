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

// Hue, saturation and value, the way a colour picker means them: the hue is the side of the wheel (red, yellow, green, cyan, blue, magenta), the
// saturation how far from white it goes (0 is white, 255 the pure colour) and the value how bright. Taking the pure colour from the wheel and
// mixing it with white costs far less code than six sides of arithmetic: what an LED shows does not tell the difference (the divisions are by
// 256, which is a shift).
void led_color_hsv(uint8_t hue, uint8_t saturation, uint8_t value, XDATA uint8_t *out)
{
    if (saturation == 0) { // no colour at all: white, at this brightness
        out[0] = out[1] = out[2] = value;
        return;
    }

    led_color_wheel(hue, out);

    const uint16_t white = (uint16_t)(255u - saturation); // how much white is mixed in

    for (uint8_t c = 0; c < 3; c++) {
        const uint16_t mixed = (uint16_t)(white + (uint16_t)(((uint16_t)out[c] * saturation) >> 8));

        out[c] = (uint8_t)((((uint16_t)value * mixed)) >> 8);
    }
}

// The wheel of an HSV colour picker: red, yellow, green, cyan, blue, magenta and back to red, in six sides of 43 steps each. It used to run
// red -> blue -> green, which is a wheel of its own: a colour chosen in the Vial app (whose picker is HSV) came out as another one - orange
// arrived as purple.
void led_color_wheel(uint8_t index, XDATA uint8_t *out)
{
    const uint8_t side = (uint8_t)(index / 43u);               // 0..5, which side of the wheel
    const uint8_t rest = (uint8_t)((index - side * 43u) * 6u); // 0..255, how far along it

    switch (side) {
        case 0:  out[0] = 255; out[1] = rest; out[2] = 0; break;                    // red -> yellow
        case 1:  out[0] = (uint8_t)(255 - rest); out[1] = 255; out[2] = 0; break;   // yellow -> green
        case 2:  out[0] = 0; out[1] = 255; out[2] = rest; break;                    // green -> cyan
        case 3:  out[0] = 0; out[1] = (uint8_t)(255 - rest); out[2] = 255; break;   // cyan -> blue
        case 4:  out[0] = rest; out[1] = 0; out[2] = 255; break;                    // blue -> magenta
        default: out[0] = 255; out[1] = 0; out[2] = (uint8_t)(255 - rest); break;   // magenta -> red
    }
}

// value * brightness / 256. The wheel gives 0 and 255 on most channels of a colour, and for those the answer needs no multiplication
// (255 * b / 256 is b - 1, for b above 0): a multiplication is a call to the compiler's 16-bit multiply, ~230 instructions, and every LED has three channels.
static uint8_t scale(uint8_t value, uint8_t brightness)
{
    if (value == 0 || brightness == 0) {
        return 0;
    }

    if (value == 255u) {
        return (uint8_t)(brightness - 1u);
    }

    // shifts and adds instead of the compiler's 16-bit multiply (~230 instructions; this is about 110)
    uint16_t sum    = 0;
    uint16_t addend = value;

    for (uint8_t bits = brightness; bits != 0; bits >>= 1) {
        if (bits & 1u) {
            sum = (uint16_t)(sum + addend);
        }

        addend = (uint16_t)(addend << 1);
    }

    return (uint8_t)(sum >> 8);
}

bool led_effect_rgb(led_effect_t fx, uint8_t row, uint8_t col, uint8_t phase, uint8_t brightness, XDATA uint8_t *out)
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
