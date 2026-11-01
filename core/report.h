#pragma once

// What the host is told: the keyboard state as a report, and the current consumer and system usages. This module only holds the state and
// builds the bytes; the transport (USB or radio) asks for them when keyboard.c says something changed (report_hook_send).
//
// Two keyboard reports, and the host gets one of them at a time: the 120-bit NKRO one in report protocol, and QMK's 8-byte six-key one in boot
// protocol (a BIOS has no idea what a bitmap is).

#include <stdbool.h>
#include <stdint.h>

#define REPORT_BOOT_SIZE 8 // mods, reserved, 6 keys
#define REPORT_BOOT_KEYS 6

// The NKRO report: the modifier byte, then one bit per usage from 0 to REPORT_NKRO_BITS - 1. QMK uses 120 bits (15 bytes), but this chip's
// EP1 buffer is 16 bytes and the report ID takes one of them: 112 bits (up to usage 0x6F, which covers the letters, the digits, the function
// keys and the navigation keys) makes the packet exactly 16 bytes.
#define REPORT_NKRO_BITS  112
#define REPORT_NKRO_BYTES (REPORT_NKRO_BITS / 8)
#define REPORT_NKRO_SIZE  (1 + REPORT_NKRO_BYTES)

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

// Keys by HID usage (KC_A..). Every key goes into the NKRO bitmap; the boot report holds the first six and leaves out the rest.
void report_add_key(uint8_t key);
void report_del_key(uint8_t key);
void report_clear_keys(void);

// Modifier byte: left hand in bits 0-3, right hand in bits 4-7.
void    report_add_mods(uint8_t mods);
void    report_del_mods(uint8_t mods);
void    report_clear_mods(void);
uint8_t report_mods(void);

void report_build_boot(uint8_t out[REPORT_BOOT_SIZE]);
void report_build_nkro(uint8_t out[REPORT_NKRO_SIZE]);

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
