#pragma once

// Combos: two or more keys pressed together give one other key (Vial's Combos tab edits them, core/keymap.c stores them). The entries are
// keycodes, not positions: a combo fires when every one of its inputs is down within COMBO_TERM_MS of the first one.
//
// The keys of a chord must not reach the host on their own, or "j" + "k" would type "jk" and then Escape. So a key that is an input of a
// configured combo is held back for at most COMBO_TERM_MS: if the chord completes, the held keys are dropped and the output is pressed (and
// released when they are let go); if the term passes, the held keys go out as if nothing had happened. A key that is not an input of any
// combo flushes what is held and goes through at once, so ordinary typing pays nothing.
//
// This is the simple half of QMK's combo engine: exact matches only (the inputs of a combo, all of them and no more), one chord at a time,
// no overlapping chords.

#include <stdbool.h>
#include <stdint.h>

#define COMBO_TERM_MS 50u // how long the keys are held back waiting for the rest of the chord (QMK's default)

void combo_init(void);

// A key of the matrix went down or up, with the keycode it resolved to. True when the combo engine took the event (it is holding the key
// back, swallowed it, or turned it into a combo): in that case the caller must not act on the keycode.
bool combo_process(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint16_t now);

// Regularly from the main loop: the keys held back go out once the term has passed.
void combo_task(uint16_t now);

// Provided by the caller: press or release a keycode on the host.
void combo_hook_send(uint16_t keycode, bool pressed);
