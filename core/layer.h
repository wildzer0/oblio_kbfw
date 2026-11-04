#pragma once

// Layer engine: a set of active layers on top of a default layer, the layer keycodes (MO, TO, TG, DF, OSL, TT, LM)
// and one-shot modifiers (OSM).
//
// A key is looked up in the highest active layer that does not hold KC_TRANSPARENT; the default layer is tried last.
// The keycode found when a key goes down is the one used when it goes up (layer_key_down / layer_key_up), so a
// layer change between the two can never leave a key stuck.

#include <stdbool.h>
#include <stdint.h>

// TT: a tap shorter than this, five in a row each started within this time of the last, toggles the layer
#define LAYER_TT_TERM 200u // ms
#define LAYER_TT_TAPS 5u

#define LAYER_NONE 0xFFu

// Provided by the caller: apply / remove modifier bits (HID modifier byte, left hand in bits 0-3, right in 4-7).
void layer_hook_mods_add(uint8_t mods);
void layer_hook_mods_del(uint8_t mods);

void layer_init(void);

void    layer_set_default(uint8_t layer);
uint8_t layer_get_default(void);

// A layer on / off for a key that is held (the hold of a layer tap).
void layer_hold(uint8_t layer, bool on);

// Bit n set = layer n is on (the default layer is not part of it).
uint8_t layer_get_state(void);

// The keycode under the active layers for a key about to go down. KC_NO when nothing answers.
uint16_t layer_lookup(uint8_t row, uint8_t col);

// The keycode for one step of an encoder, through the active layers.
uint16_t layer_lookup_encoder(uint8_t encoder, bool clockwise);

// A key went down / up: the keycode to act on. Down resolves through the layers and remembers the result; up
// returns that same keycode, whatever the layers say by then.
uint16_t layer_key_down(uint8_t row, uint8_t col);
uint16_t layer_key_up(uint8_t row, uint8_t col);

// Layer and one-shot keycodes. Returns true when `keycode` was one of them (and handled). `now` is the time in ms.
bool layer_process(uint16_t keycode, bool pressed, uint16_t now);

// Every other key calls these around its press and release, so that one-shots get used up.
void layer_key_pressed(void);
void layer_key_released(void);
