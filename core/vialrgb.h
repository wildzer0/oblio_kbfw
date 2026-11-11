#pragma once

// The Lighting tab of the Vial app (docs/vial.md): the app's effect list, brightness, speed and colour, driving the effects this keyboard has.
//
// The app decides whether to show the tab from the keyboard *definition* (the field "lighting": "vialrgb" in board/vial.json) and then asks
// the firmware which effects it has. This firmware has its own effects, not QMK's rgb_matrix, so the ids of the six that have a counterpart are
// offered and mapped onto them (core/vialrgb.c); the colour is one of the seventeen the effects use.

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

void vialrgb_init(void); // host tests only (not in the firmware)

// The effect in use, as one of the ids the Vial app knows, and back. The app's Lighting tab shows the ids the firmware lists, so these are
// the two functions that decide what it can offer.
uint8_t vialrgb_id_of(uint8_t effect);
uint8_t vialrgb_effect_of(uint8_t id);

// The protocol, on VIA's lighting commands (core/via.c routes them): data[1] is the sub-command from docs/vial.md.
void vialrgb_get_value(XDATA uint8_t *data);
void vialrgb_set_value(XDATA uint8_t *data);
void vialrgb_save(void);

// Provided by the board (or a test): write the keymap down now, because the app asked for it
void vialrgb_hook_save(void);
