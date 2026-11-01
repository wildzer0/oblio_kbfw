#pragma once

#include <stdint.h>

// 16-bit keycodes, laid out as QMK/Vial does: the Vial desktop app edits the keymap with these numbers, so the
// ranges cannot change. Only what Oblio uses is defined here.

// ---- ranges ----
#define QK_BASIC              0x0000u
#define QK_BASIC_MAX          0x00FFu
#define QK_MODS               0x0100u // modifiers + a basic key, e.g. LSFT(KC_A)
#define QK_MODS_MAX           0x1FFFu
#define QK_MOD_TAP            0x2000u
#define QK_MOD_TAP_MAX        0x3FFFu
#define QK_LAYER_TAP          0x4000u
#define QK_LAYER_TAP_MAX      0x4FFFu
#define QK_LAYER_MOD          0x5000u
#define QK_LAYER_MOD_MAX      0x51FFu
#define QK_TO                 0x5200u
#define QK_TO_MAX             0x521Fu
#define QK_MOMENTARY          0x5220u
#define QK_MOMENTARY_MAX      0x523Fu
#define QK_DEF_LAYER          0x5240u
#define QK_DEF_LAYER_MAX      0x525Fu
#define QK_TOGGLE_LAYER       0x5260u
#define QK_TOGGLE_LAYER_MAX   0x527Fu
#define QK_ONE_SHOT_LAYER     0x5280u
#define QK_ONE_SHOT_LAYER_MAX 0x529Fu
#define QK_ONE_SHOT_MOD       0x52A0u
#define QK_ONE_SHOT_MOD_MAX   0x52BFu
#define QK_LAYER_TAP_TOGGLE   0x52C0u
#define QK_LAYER_TAP_TOGGLE_MAX 0x52DFu
#define QK_TAP_DANCE          0x5700u
#define QK_TAP_DANCE_MAX      0x57FFu
#define QK_MACRO              0x7700u
#define QK_MACRO_MAX          0x777Fu
#define QK_KB                 0x7E00u // keyboard-specific keycodes
#define QK_KB_MAX             0x7E3Fu

#define IN_RANGE(code, lo, hi) ((code) >= (lo) && (code) <= (hi))

#define IS_QK_BASIC(code)           IN_RANGE(code, QK_BASIC, QK_BASIC_MAX)
#define IS_QK_MODS(code)            IN_RANGE(code, QK_MODS, QK_MODS_MAX)
#define IS_QK_MOD_TAP(code)         IN_RANGE(code, QK_MOD_TAP, QK_MOD_TAP_MAX)
#define IS_QK_LAYER_TAP(code)       IN_RANGE(code, QK_LAYER_TAP, QK_LAYER_TAP_MAX)
#define IS_QK_LAYER_MOD(code)       IN_RANGE(code, QK_LAYER_MOD, QK_LAYER_MOD_MAX)
#define IS_QK_TO(code)              IN_RANGE(code, QK_TO, QK_TO_MAX)
#define IS_QK_MOMENTARY(code)       IN_RANGE(code, QK_MOMENTARY, QK_MOMENTARY_MAX)
#define IS_QK_DEF_LAYER(code)       IN_RANGE(code, QK_DEF_LAYER, QK_DEF_LAYER_MAX)
#define IS_QK_TOGGLE_LAYER(code)    IN_RANGE(code, QK_TOGGLE_LAYER, QK_TOGGLE_LAYER_MAX)
#define IS_QK_ONE_SHOT_LAYER(code)  IN_RANGE(code, QK_ONE_SHOT_LAYER, QK_ONE_SHOT_LAYER_MAX)
#define IS_QK_ONE_SHOT_MOD(code)    IN_RANGE(code, QK_ONE_SHOT_MOD, QK_ONE_SHOT_MOD_MAX)
#define IS_QK_LAYER_TAP_TOGGLE(code) IN_RANGE(code, QK_LAYER_TAP_TOGGLE, QK_LAYER_TAP_TOGGLE_MAX)
#define IS_QK_TAP_DANCE(code)       IN_RANGE(code, QK_TAP_DANCE, QK_TAP_DANCE_MAX)
#define IS_QK_MACRO(code)           IN_RANGE(code, QK_MACRO, QK_MACRO_MAX)
#define IS_QK_KB(code)              IN_RANGE(code, QK_KB, QK_KB_MAX)

// ---- keycodes that build on a layer, a modifier set or a number ----
#define MO(layer)   (QK_MOMENTARY | ((layer) & 0x1Fu))     // layer while the key is down
#define TO(layer)   (QK_TO | ((layer) & 0x1Fu))            // this layer only
#define TG(layer)   (QK_TOGGLE_LAYER | ((layer) & 0x1Fu))  // toggle a layer
#define DF(layer)   (QK_DEF_LAYER | ((layer) & 0x1Fu))     // set the default layer
#define OSL(layer)  (QK_ONE_SHOT_LAYER | ((layer) & 0x1Fu)) // layer for the next key
#define OSM(mods)   (QK_ONE_SHOT_MOD | ((mods) & 0x1Fu))   // modifiers for the next key
#define TT(layer)   (QK_LAYER_TAP_TOGGLE | ((layer) & 0x1Fu)) // momentary; tapped 5 times it toggles
#define TD(n)       (QK_TAP_DANCE | ((n) & 0xFFu))
#define LT(layer, kc) (QK_LAYER_TAP | (((layer) & 0xFu) << 8) | ((kc) & 0xFFu))
#define MT(mods, kc)  (QK_MOD_TAP | (((mods) & 0x1Fu) << 8) | ((kc) & 0xFFu))
#define LM(layer, mods) (QK_LAYER_MOD | (((layer) & 0xFu) << 5) | ((mods) & 0x1Fu)) // layer + modifiers while down

#define QK_LAYER_TAP_GET_LAYER(code) (((code) >> 8) & 0xFu)
#define QK_LAYER_TAP_GET_KEY(code)   ((code) & 0xFFu)
#define QK_MOD_TAP_GET_MODS(code)    (((code) >> 8) & 0x1Fu)
#define QK_MOD_TAP_GET_KEY(code)     ((code) & 0xFFu)
#define QK_LAYER_MOD_GET_LAYER(code) (((code) >> 5) & 0xFu)
#define QK_LAYER_MOD_GET_MODS(code)  ((code) & 0x1Fu)
#define QK_MODS_GET_MODS(code)       (((code) >> 8) & 0x1Fu)
#define QK_MODS_GET_KEY(code)        ((code) & 0xFFu)

// A basic key with modifiers held around it: LSFT(KC_A) is shift + A. Same 5-bit mask as MOD_* below, in bits 8-12.
#define LCTL(kc) ((MOD_LCTL << 8) | (kc))
#define LSFT(kc) ((MOD_LSFT << 8) | (kc))
#define LALT(kc) ((MOD_LALT << 8) | (kc))
#define LGUI(kc) ((MOD_LGUI << 8) | (kc))
#define RCTL(kc) ((MOD_RCTL << 8) | (kc))
#define RSFT(kc) ((MOD_RSFT << 8) | (kc))
#define RALT(kc) ((MOD_RALT << 8) | (kc))
#define RGUI(kc) ((MOD_RGUI << 8) | (kc))

// Modifier masks in the QMK encoding: bit 4 moves the modifier to the right hand
#define MOD_LCTL 0x01u
#define MOD_LSFT 0x02u
#define MOD_LALT 0x04u
#define MOD_LGUI 0x08u
#define MOD_RCTL 0x11u
#define MOD_RSFT 0x12u
#define MOD_RALT 0x14u
#define MOD_RGUI 0x18u

// ---- basic keys (the HID usage ids of the keyboard page, plus the consumer ones QMK folds in) ----
#define KC_NO          0x0000u
#define KC_TRANSPARENT 0x0001u
#define KC_TRNS        KC_TRANSPARENT

#define KC_A 0x04u
#define KC_B 0x05u
#define KC_C 0x06u
#define KC_D 0x07u
#define KC_E 0x08u
#define KC_F 0x09u
#define KC_G 0x0Au
#define KC_H 0x0Bu
#define KC_I 0x0Cu
#define KC_J 0x0Du
#define KC_K 0x0Eu
#define KC_L 0x0Fu
#define KC_M 0x10u
#define KC_N 0x11u
#define KC_O 0x12u
#define KC_P 0x13u
#define KC_Q 0x14u
#define KC_R 0x15u
#define KC_S 0x16u
#define KC_T 0x17u
#define KC_U 0x18u
#define KC_V 0x19u
#define KC_W 0x1Au
#define KC_X 0x1Bu
#define KC_Y 0x1Cu
#define KC_Z 0x1Du

#define KC_1 0x1Eu
#define KC_2 0x1Fu
#define KC_3 0x20u
#define KC_4 0x21u
#define KC_5 0x22u
#define KC_6 0x23u
#define KC_7 0x24u
#define KC_8 0x25u
#define KC_9 0x26u
#define KC_0 0x27u

#define KC_ENT  0x28u
#define KC_ESC  0x29u
#define KC_BSPC 0x2Au
#define KC_TAB  0x2Bu
#define KC_SPC  0x2Cu
#define KC_MINS 0x2Du
#define KC_EQL  0x2Eu
#define KC_LBRC 0x2Fu
#define KC_RBRC 0x30u
#define KC_BSLS 0x31u
#define KC_SCLN 0x33u
#define KC_QUOT 0x34u
#define KC_GRV  0x35u
#define KC_COMM 0x36u
#define KC_DOT  0x37u
#define KC_SLSH 0x38u
#define KC_CAPS 0x39u

#define KC_F1  0x3Au
#define KC_F2  0x3Bu
#define KC_F3  0x3Cu
#define KC_F4  0x3Du
#define KC_F5  0x3Eu
#define KC_F6  0x3Fu
#define KC_F7  0x40u
#define KC_F8  0x41u
#define KC_F9  0x42u
#define KC_F10 0x43u
#define KC_F11 0x44u
#define KC_F12 0x45u

#define KC_PGUP 0x4Bu
#define KC_DEL  0x4Cu
#define KC_PGDN 0x4Eu
#define KC_RGHT 0x4Fu
#define KC_LEFT 0x50u
#define KC_DOWN 0x51u
#define KC_UP   0x52u

#define KC_LCTL 0xE0u
#define KC_LSFT 0xE1u
#define KC_LALT 0xE2u
#define KC_LGUI 0xE3u
#define KC_RCTL 0xE4u
#define KC_RSFT 0xE5u
#define KC_RALT 0xE6u
#define KC_RGUI 0xE7u

// system control (sent on their own report)
#define KC_SYSTEM_POWER 0xA5u
#define KC_SYSTEM_SLEEP 0xA6u
#define KC_SYSTEM_WAKE  0xA7u

// media and application keys (sent as consumer usages)
#define KC_MUTE 0xA8u
#define KC_VOLU 0xA9u
#define KC_VOLD 0xAAu
#define KC_MNXT 0xABu
#define KC_MPRV 0xACu
#define KC_MSTP 0xADu
#define KC_MPLY 0xAEu
#define KC_MSEL 0xAFu
#define KC_EJCT 0xB0u
#define KC_MAIL 0xB1u
#define KC_CALC 0xB2u
#define KC_MYCM 0xB3u
#define KC_WSCH 0xB4u
#define KC_WHOM 0xB5u
#define KC_WBAK 0xB6u
#define KC_WFWD 0xB7u
#define KC_WSTP 0xB8u
#define KC_WREF 0xB9u
#define KC_WFAV 0xBAu
#define KC_MFFD 0xBBu
#define KC_MRWD 0xBCu
#define KC_BRIU 0xBDu
#define KC_BRID 0xBEu
#define KC_CPNL 0xBFu
#define KC_ASST 0xC0u
#define KC_MCTL 0xC1u
#define KC_LPAD 0xC2u

// mouse: sent on their own report (core/mouse.c, core/report.c). QMK's numbers, so Vial knows them and a saved layout keeps them
#define KC_MS_UP        0xCDu
#define KC_MS_DOWN      0xCEu
#define KC_MS_LEFT      0xCFu
#define KC_MS_RIGHT     0xD0u
#define KC_MS_BTN1      0xD1u
#define KC_MS_BTN2      0xD2u
#define KC_MS_BTN3      0xD3u
#define KC_MS_BTN4      0xD4u
#define KC_MS_BTN5      0xD5u
#define KC_MS_WH_UP     0xD9u
#define KC_MS_WH_DOWN   0xDAu
#define KC_MS_WH_LEFT   0xDBu
#define KC_MS_WH_RIGHT  0xDCu
#define KC_MS_ACCEL0    0xDDu
#define KC_MS_ACCEL1    0xDEu
#define KC_MS_ACCEL2    0xDFu

#define KC_FIRST_KEY    0x04u // KC_A
#define KC_LAST_KEY     0xA4u // KC_EXSEL
#define KC_FIRST_SYSTEM KC_SYSTEM_POWER
#define KC_LAST_SYSTEM  KC_SYSTEM_WAKE
#define KC_FIRST_MEDIA  KC_MUTE
#define KC_LAST_MEDIA   KC_LPAD

#define KC_FIRST_MOUSE   KC_MS_UP
#define KC_LAST_MOUSE    KC_MS_ACCEL2

#define IS_KEY_KEYCODE(code)      IN_RANGE(code, KC_FIRST_KEY, KC_LAST_KEY)
#define IS_MOUSE_KEYCODE(code)    IN_RANGE(code, KC_FIRST_MOUSE, KC_LAST_MOUSE)
#define IS_SYSTEM_KEYCODE(code)   IN_RANGE(code, KC_FIRST_SYSTEM, KC_LAST_SYSTEM)
#define IS_CONSUMER_KEYCODE(code) IN_RANGE(code, KC_FIRST_MEDIA, KC_LAST_MEDIA)
#define IS_MODIFIER_KEYCODE(code) IN_RANGE(code, KC_LCTL, KC_RGUI)

// The bit of a modifier key (KC_LCTL..KC_RGUI) in the HID modifier byte
#define MOD_BIT(code) (1u << ((code) & 0x07u))

// The 5-bit modifier mask of MOD_*, OSM, LM and the QK_MODS range, as the HID modifier byte: the low 4 bits pick the
// modifiers, bit 4 moves them to the right hand.
#define MODS_FROM_MASK(mask) ((uint8_t)(((mask) & 0x10u) ? (((mask) & 0x0Fu) << 4) : ((mask) & 0x0Fu)))
