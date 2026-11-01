#include "vialrgb.h"
#include "keymap.h"
#include "keycodes.h"
#include "led.h"
#include "led_effect.h"
#include "platform.h"
#include <string.h>

// VIA's lighting commands, and the sub-commands of docs/vial.md
#define LIGHTING_GET_INFO    0x40u
#define LIGHTING_GET_MODE    0x41u
#define LIGHTING_SET_MODE    0x41u
#define LIGHTING_GET_SUPPORTED 0x42u
#define LIGHTING_DIRECT_FASTSET 0x42u
#define LIGHTING_GET_NUMBER_LEDS 0x43u
#define LIGHTING_GET_LED_INFO 0x44u

#define VIALRGB_PROTOCOL_VERSION 1u
#define VIALRGB_MAX_BRIGHTNESS   255u

#define VIALRGB_EFFECT_OFF_ID    0u
#define VIALRGB_EFFECT_DIRECT_ID 1u

void vialrgb_init(void)
{
    // The colours are in the keymap already (they come from flash or from the defaults): nothing to do here, and the effect that shows them
    // is chosen by the profile's effect setting like every other one.
}

// ---- the colours, as RGB565 in the keymap ----
static uint8_t expand5(uint16_t v)
{
    return (uint8_t)((v << 3) | (v >> 2));
}

static uint8_t expand6(uint16_t v)
{
    return (uint8_t)((v << 2) | (v >> 4));
}

static uint16_t pack(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3));
}

bool vialrgb_colour(uint8_t row, uint8_t col, uint8_t out[3])
{
    const uint16_t word = keymap_led_get((uint8_t)(row * MATRIX_COLS + col));

    if (word == 0) {
        return false;
    }

    out[0] = expand5((uint16_t)(word >> 11));
    out[1] = expand6((uint16_t)((word >> 5) & 0x3Fu));
    out[2] = expand5((uint16_t)(word & 0x1Fu));
    return true;
}

// ---- hue/saturation/value, as the app sends them ----
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t out[3])
{
    if (s == 0) { // grey: only the value counts
        out[0] = out[1] = out[2] = v;
        return;
    }

    const uint8_t region = (uint8_t)(h / 43u);            // 0..5, the six sides of the colour hexagon
    const uint8_t rest   = (uint8_t)((h - region * 43u) * 6u); // 0..255 inside the side

    const uint16_t p = (uint16_t)((uint16_t)v * (255u - s) / 255u);
    const uint16_t q = (uint16_t)((uint16_t)v * (255u - (uint16_t)((uint16_t)s * rest / 255u)) / 255u);
    const uint16_t t = (uint16_t)((uint16_t)v * (255u - (uint16_t)((uint16_t)s * (255u - rest) / 255u)) / 255u);

    switch (region) {
        case 0:  out[0] = v; out[1] = (uint8_t)t; out[2] = (uint8_t)p; break;
        case 1:  out[0] = (uint8_t)q; out[1] = v; out[2] = (uint8_t)p; break;
        case 2:  out[0] = (uint8_t)p; out[1] = v; out[2] = (uint8_t)t; break;
        case 3:  out[0] = (uint8_t)p; out[1] = (uint8_t)q; out[2] = v; break;
        case 4:  out[0] = (uint8_t)t; out[1] = (uint8_t)p; out[2] = v; break;
        default: out[0] = v; out[1] = (uint8_t)p; out[2] = (uint8_t)q; break;
    }
}

// ---- the effect the profile is showing ----
static uint16_t mode_id(void)
{
    return (keymap_cfg_get(CFG_LED_EFFECT) == FX_VIAL) ? VIALRGB_EFFECT_DIRECT_ID : VIALRGB_EFFECT_OFF_ID;
}

static void set_mode(uint16_t id)
{
    if (id == VIALRGB_EFFECT_DIRECT_ID) {
        keymap_cfg_set(CFG_LED_EFFECT, FX_VIAL);
    } else {
        keymap_cfg_set(CFG_LED_EFFECT, FX_OFF); // anything else is "off" as far as this keyboard goes: it has no rgb_matrix effects
    }
}

// ---- the protocol ----
void vialrgb_get_value(XDATA uint8_t *data)
{
    const uint8_t command = data[1];

    XDATA uint8_t *args = &data[2];

    switch (command) {
        case LIGHTING_GET_INFO:
            args[0] = VIALRGB_PROTOCOL_VERSION;
            args[1] = 0;
            args[2] = VIALRGB_MAX_BRIGHTNESS;
            break;

        case LIGHTING_GET_MODE: {
            const uint16_t id = mode_id();

            args[0] = (uint8_t)(id & 0xFFu);
            args[1] = (uint8_t)(id >> 8);
            args[2] = (uint8_t)keymap_cfg_get(CFG_LED_SPEED);
            args[3] = 0; // hue and saturation belong to the effects this keyboard does not have
            args[4] = 0;
            args[5] = (uint8_t)keymap_cfg_get(CFG_LED_BRIGHT);
            break;
        }

        case LIGHTING_GET_SUPPORTED: {
            const uint16_t greater_than = (uint16_t)(args[0] | ((uint16_t)args[1] << 8));
            static const uint16_t supported[2] = {VIALRGB_EFFECT_OFF_ID, VIALRGB_EFFECT_DIRECT_ID};

            memset(&args[0], 0xFF, 30);

            uint8_t at = 0;

            for (uint8_t i = 0; i < 2; i++) {
                if (supported[i] > greater_than) {
                    args[at++] = (uint8_t)(supported[i] & 0xFFu);
                    args[at++] = (uint8_t)(supported[i] >> 8);
                }
            }
            break;
        }

        case LIGHTING_GET_NUMBER_LEDS:
            args[0] = (uint8_t)(VIALRGB_LEDS & 0xFFu);
            args[1] = (uint8_t)(VIALRGB_LEDS >> 8);
            break;

        case LIGHTING_GET_LED_INFO: {
            const uint16_t led = (uint16_t)(args[0] | ((uint16_t)args[1] << 8));

            if (led >= VIALRGB_LEDS) {
                break;
            }

            const uint8_t row = (uint8_t)(led / MATRIX_COLS);
            const uint8_t col = (uint8_t)(led % MATRIX_COLS);

            // A grid of 224 x 224, which is the space the app draws in: the app colours by matrix position, so the coordinates only have to
            // be distinct and in order
            args[0] = (uint8_t)((uint16_t)col * 224u / MATRIX_COLS);
            args[1] = (uint8_t)((uint16_t)row * 224u / MATRIX_ROWS);
            args[2] = 4; // a key light

            if (keymap_get(0, row, col) != KC_NO) {
                args[3] = row;
                args[4] = col;
            } else {
                args[3] = 0xFF; // not a key: the app leaves it alone
                args[4] = 0xFF;
            }
            break;
        }

        default:
            data[0] = 0xFF; // an unhandled sub-command, as VIA expects
            break;
    }
}

void vialrgb_set_value(XDATA uint8_t *data)
{
    const uint8_t command = data[1];

    XDATA const uint8_t *args = &data[2];

    if (command == LIGHTING_SET_MODE) {
        set_mode((uint16_t)(args[0] | ((uint16_t)args[1] << 8)));

        if (args[2] >= LED_SPEED_MIN && args[2] <= LED_SPEED_MAX) {
            keymap_cfg_set(CFG_LED_SPEED, args[2]);
        }

        keymap_cfg_set(CFG_LED_BRIGHT, args[5]);
        return;
    }

    if (command != LIGHTING_DIRECT_FASTSET) {
        data[0] = 0xFF;
        return;
    }

    // first LED (2 bytes), how many (1), then three bytes of hue, saturation and value each
    const uint16_t first = (uint16_t)(args[0] | ((uint16_t)args[1] << 8));
    const uint8_t  count = args[2];

    if (count == 0 || count > 9) {
        return; // a 32-byte packet holds nine LEDs
    }

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t led = (uint16_t)(first + i);

        if (led >= VIALRGB_LEDS) {
            break;
        }

        uint8_t rgb[3];

        hsv_to_rgb(args[3 + i * 3], args[4 + i * 3], args[5 + i * 3], rgb);
        keymap_led_set((uint8_t)led, pack(rgb[0], rgb[1], rgb[2]));
    }

    // Painting a key means showing the painting: the app does not always send set_mode first
    if (keymap_cfg_get(CFG_LED_EFFECT) != FX_VIAL) {
        keymap_cfg_set(CFG_LED_EFFECT, FX_VIAL);
    }
}

void vialrgb_save(void)
{
    vialrgb_hook_save(); // the colours are part of the keymap: the board writes them down now
}
