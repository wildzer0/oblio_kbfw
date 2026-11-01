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

// The encoder lines, set by the test
uint8_t stub_encoder_state;

uint8_t encoder_hook_state(uint8_t index)
{
    (void)index;
    return stub_encoder_state;
}

void keyboard_hook_activity(void) {}
