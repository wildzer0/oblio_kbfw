#pragma once

// Key overrides: a key does something else while the right modifiers are held (Vial's Key Overrides tab edits them, core/keymap.c stores
// them). The entry is Vial's: a trigger keycode, a replacement, the layers it is active on (0xFFFF = all), the modifiers it needs, the ones
// that must not be down, the ones to take away while it acts, and the option bits (bit 7 = enabled).
//
// Example: trigger Backspace, replacement Delete, required Shift -> Shift+Backspace deletes forward, and the Shift is still reported (unless
// it is also in "suppressed"). The trigger itself never reaches the host while the override acts; its release releases the replacement.
//
// What is not implemented (none of them changes the everyday use): the QMK options about *when* the trigger activates (on the way down, or
// when the required modifiers arrive), "one modifier only", and the "do not re-register" variants. They are read and ignored.

#include <stdbool.h>
#include <stdint.h>

void override_init(void);

// A key of the matrix went down or up, with the keycode it resolved to, and the modifiers held now. True when an override took it.
bool override_process(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint8_t mods);

// Provided by the caller: press or release a keycode on the host, and add or remove modifiers.
void override_hook_send(uint16_t keycode, bool pressed);
void override_hook_mods(uint8_t mods, bool on);
