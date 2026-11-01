#pragma once

// What the host is told: the keyboard state as the 6-key boot report and as the NKRO bitmap, and the current
// consumer and system usages. This module only holds the state and builds the bytes; the transport (USB or radio)
// asks for them when keyboard.c says something changed (report_hook_send).

#include <stdbool.h>
#include <stdint.h>

#define REPORT_BOOT_SIZE 8 // mods, reserved, 6 keys
#define REPORT_BOOT_KEYS 6

// NKRO: the modifier byte then one bit per usage, 0 .. REPORT_NKRO_BITS-1 (the stock report has 120 bits, usage 0-0x77)
#define REPORT_NKRO_BITS  120
#define REPORT_NKRO_BYTES (REPORT_NKRO_BITS / 8)
#define REPORT_NKRO_SIZE  (1 + REPORT_NKRO_BYTES)

typedef enum {
    REPORT_KEYBOARD,
    REPORT_CONSUMER,
    REPORT_SYSTEM,
} report_kind_t;

// Provided by the caller: the state of `kind` has changed and should reach the host now. Called once per change, so that
// a key pressed and released in the same pass still produces both reports.
void report_hook_send(report_kind_t kind);

void report_init(void);

// Keys by HID usage (KC_A..). A seventh key does not fit the boot report and is left out of it; the NKRO bitmap has it.
void report_add_key(uint8_t key);
void report_del_key(uint8_t key);
void report_clear_keys(void);
bool report_key_pressed(uint8_t key);

// Modifier byte: left hand in bits 0-3, right hand in bits 4-7.
void    report_add_mods(uint8_t mods);
void    report_del_mods(uint8_t mods);
void    report_clear_mods(void);
uint8_t report_mods(void);

void report_build_boot(uint8_t out[REPORT_BOOT_SIZE]);
void report_build_nkro(uint8_t out[REPORT_NKRO_SIZE]);

// Consumer and system usages: one at a time. 0 = nothing held.
void     report_set_consumer(uint16_t usage);
uint16_t report_consumer(void);
void     report_set_system(uint16_t usage);
uint16_t report_system(void);

// Keycode (KC_MUTE.., KC_SYSTEM_POWER..) to usage. 0 for a key that has none.
uint16_t report_keycode_to_consumer(uint8_t keycode);
uint16_t report_keycode_to_system(uint8_t keycode);
