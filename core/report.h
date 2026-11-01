#pragma once

// What the host is told: the keyboard state as the 6-key boot report, and the current consumer and system usages. This
// module only holds the state and builds the bytes; the transport (USB or radio) asks for them when keyboard.c says
// something changed (report_hook_send).
//
// There is no NKRO report: the 8-byte boot report is the one on the wire, so more than six keys at once lose the ones
// that do not fit. (The radio has an NKRO form of its own and uses it in radio.c.)

#include <stdbool.h>
#include <stdint.h>

#define REPORT_BOOT_SIZE 8 // mods, reserved, 6 keys
#define REPORT_BOOT_KEYS 6

typedef enum {
    REPORT_KEYBOARD,
    REPORT_CONSUMER,
    REPORT_SYSTEM,
    REPORT_MOUSE,
} report_kind_t;

// Provided by the caller: the state of `kind` has changed and should reach the host now. Called once per change, so that
// a key pressed and released in the same pass still produces both reports.
void report_hook_send(report_kind_t kind);

void report_init(void);

// Keys by HID usage (KC_A..). A seventh key does not fit the boot report and is left out of it.
void report_add_key(uint8_t key);
void report_del_key(uint8_t key);
void report_clear_keys(void);

// Modifier byte: left hand in bits 0-3, right hand in bits 4-7.
void    report_add_mods(uint8_t mods);
void    report_del_mods(uint8_t mods);
void    report_clear_mods(void);
uint8_t report_mods(void);

void report_build_boot(uint8_t out[REPORT_BOOT_SIZE]);

// ---- the mouse (core/mouse.c fills it, the transport sends it) ----
// The buttons are a mask (bit 0 = button 1), the movement and the wheel are relative: they are added up until a report carries them and
// the build clears them (one report must not send the same movement twice).
#define REPORT_MOUSE_SIZE 6 // buttons, x (2 bytes), y (2 bytes), wheel
void    report_mouse_button(uint8_t mask, bool pressed);
void    report_mouse_move(int8_t dx, int8_t dy);
void    report_mouse_wheel(int8_t delta);
uint8_t report_mouse_buttons(void);
void    report_build_mouse(uint8_t out[REPORT_MOUSE_SIZE]);

// Consumer and system usages: one at a time. 0 = nothing held.
void     report_set_consumer(uint16_t usage);
uint16_t report_consumer(void);
void     report_set_system(uint16_t usage);
uint16_t report_system(void);

// Keycode (KC_MUTE.., KC_SYSTEM_POWER..) to usage. 0 for a key that has none.
uint16_t report_keycode_to_consumer(uint8_t keycode);
uint16_t report_keycode_to_system(uint8_t keycode);
