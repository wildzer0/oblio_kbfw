#include "vialrgb.h"
#include "keymap.h"
#include "led.h"
#include "led_effect.h"
#include "platform.h"
#include <string.h>

// VIA's lighting commands, and the sub-commands of docs/vial.md
#define LIGHTING_GET_INFO      0x40u
#define LIGHTING_GET_MODE      0x41u
#define LIGHTING_SET_MODE      0x41u
#define LIGHTING_GET_SUPPORTED 0x42u

#define VIALRGB_PROTOCOL_VERSION 1u
#define VIALRGB_MAX_BRIGHTNESS   255u

// The effects this keyboard has, against the ids the Vial app knows: the Lighting tab shows only the ids the firmware lists, under the names
// of the app's own table. These are this keyboard's effects, not QMK's rgb_matrix; the two that have no counterpart are reported as the
// nearest one so that the tab always has something selected.
typedef struct {
    uint8_t id;
    uint8_t effect;
} vialrgb_effect_t;

static const ROM vialrgb_effect_t effects[] = {
    {0, FX_OFF},        // "Disable"
    {2, FX_SOLID},      // "Solid Color"
    {4, FX_VERTICAL},   // "Gradient Up Down"
    {5, FX_HORIZONTAL}, // "Gradient Left Right"
    {6, FX_BREATHE},    // "Breathing"
    {18, FX_RADIAL},    // "Cycle Out In": rings spreading from the centre
};

#define EFFECT_COUNT ((uint8_t)(sizeof effects / sizeof effects[0]))

// Only the host tests call this: the firmware does not carry it (__SDCC is defined by the chip's compiler only)
#ifndef __SDCC
void vialrgb_init(void)
{
    // Nothing to set up: which effect is showing is a setting of the profile, like the brightness and the speed.
}
#endif

uint8_t vialrgb_id_of(uint8_t effect)
{
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (effects[i].effect == effect) {
            return effects[i].id;
        }
    }

    switch (effect) {
        case FX_PULSE:    return 6; // a breathing of its own
        case FX_DIAGONAL: return 5; // a wave across the board
        default:          return 0; // off
    }
}

uint8_t vialrgb_effect_of(uint8_t id)
{
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (effects[i].id == id) {
            return effects[i].effect;
        }
    }

    return FX_OFF; // an id this keyboard does not have
}

// The colour of the profile is a hue and a saturation in one word (core/keymap.h), so what the picker sends is what is kept, to the step.
static uint16_t color_from_hsv(uint8_t hue, uint8_t sat)
{
    return (uint16_t)(((uint16_t)sat << 8) | hue);
}

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
            const uint16_t color = keymap_cfg_get(CFG_LED_COLOR);

            args[0] = vialrgb_id_of((uint8_t)keymap_cfg_get(CFG_LED_EFFECT));
            args[1] = 0;
            args[2] = (uint8_t)keymap_cfg_get(CFG_LED_SPEED);
            args[3] = (uint8_t)(color & 0xFFu);  // the hue
            args[4] = (uint8_t)(color >> 8);     // the saturation
            args[5] = (uint8_t)keymap_cfg_get(CFG_LED_BRIGHT);
            break;
        }

        case LIGHTING_GET_SUPPORTED: {
            const uint8_t greater_than = args[0];

            memset(&args[0], 0xFF, 30);

            uint8_t at = 0;

            for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
                if (effects[i].id > greater_than) {
                    args[at++] = effects[i].id;
                    args[at++] = 0;
                }
            }
            break;
        }

        default:
            data[0] = 0xFF; // a sub-command this firmware does not have, as VIA expects
            break;
    }
}

void vialrgb_set_value(XDATA uint8_t *data)
{
    if (data[1] != LIGHTING_SET_MODE) {
        data[0] = 0xFF;
        return;
    }

    XDATA const uint8_t *args = &data[2];

    keymap_cfg_set(CFG_LED_EFFECT, vialrgb_effect_of(args[0]));

    if (args[2] >= LED_SPEED_MIN && args[2] <= LED_SPEED_MAX) {
        keymap_cfg_set(CFG_LED_SPEED, args[2]);
    }

    keymap_cfg_set(CFG_LED_COLOR, color_from_hsv(args[3], args[4]));
    keymap_cfg_set(CFG_LED_BRIGHT, args[5]);
}

void vialrgb_save(void)
{
    vialrgb_hook_save(); // the lighting settings are part of the keymap: the board writes them down now
}
