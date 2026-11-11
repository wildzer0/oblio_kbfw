#pragma once

// Accented letters (à è é ì ò ù, lower and upper case) for a keyboard that sends HID keys only, whatever the layout of the host: the key does not carry
// the letter, it types the sequence that makes the host write it. The sequences, which depend on the operating system:
//   Windows  Alt + the Latin-1 code on the numeric keypad (Alt 0 2 3 2 is è). The keypad needs Num Lock ON: if the host has it off, it is turned on
//            before and off again after.
//   macOS    Option + a dead key, then the vowel (Shift on the vowel for the capital): grave is Option+` (à è ì ò ù), acute is Option+E (é).
//   Linux    Ctrl+Shift+U, the two hex digits of the code, space (GTK / IBus / Qt).
// The sequence is a dozen key events. They are played ONE EVERY ACCENT_STEP_MS, from the main loop (accent_task): the queue of reports towards the USB
// host holds 7 states and the one towards the radio 8, and the radio sends one every 2-3 ms, so a burst would lose states and a digit would go missing.
// While the sequence runs the board does not take new key events from the matrix (accent_busy): they wait and are handled after it - a Shift let go
// in the middle would otherwise be restored by the end of the sequence and stay stuck.
// The modifiers that are down when the sequence starts are put aside (the sequence needs none of them down) and put back at its end.
//
// Not in the profile build (LOOP_PROFILE): it has a handful of bytes of flash to spare.

#include "options.h"
#include <stdbool.h>
#include <stdint.h>

#define ACCENT_COUNT   6u // à è é ì ò ù, in this order
#define ACCENT_STEP_MS 4u

typedef enum { ACCENT_OS_WINDOWS, ACCENT_OS_MAC, ACCENT_OS_LINUX } accent_os_t;

#if !LOOP_PROFILE

// Starts the sequence of accent `index` (0..5), upper or lower case, for `os`. `num_lock` is the host's Num Lock (Windows only). Ignored while another one runs.
void accent_start(uint16_t now, uint8_t index, bool upper, accent_os_t os, bool num_lock);

// From the main loop, with the clock in ms: plays the next step when ACCENT_STEP_MS have gone by.
void accent_task(uint16_t now);

// True while a sequence is being played (and for the step that puts the modifiers back).
bool accent_busy(void);

// Provided by the keyboard module: a key (a HID usage, or a modifier KC_LCTL..KC_RGUI) goes down or up, as if a key of the keymap did it.
void accent_hook_key(uint16_t keycode, bool down);

// Provided by the keyboard module: the modifier byte of the report (HID bits), and putting it back to a value.
uint8_t accent_hook_mods_get(void);
void    accent_hook_mods_set(uint8_t mods);

#else // the profile build has none of it: the calls vanish

#    define accent_start(now, index, upper, os, num_lock) ((void)0)
#    define accent_task(now)                              ((void)0)
#    define accent_busy()                                 false

#endif
