#pragma once

// For tests that link core/keymap.c or core/keyboard.c but have no use for what the board provides on top: every
// setting at 0 (which means "the default" for the tapping term), and no custom keycodes.
#include "keymap.h"

uint16_t keymap_default_cfg(uint8_t field)
{
    (void)field;
    return 0;
}

void keyboard_hook_custom(uint16_t keycode, bool pressed)
{
    (void)keycode;
    (void)pressed;
}
