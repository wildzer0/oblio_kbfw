#pragma once

// Tap-or-hold keys: LT(layer, kc) and MT(mods, kc). Tapped, the key sends `kc`; held longer than the tapping term it
// switches the layer on or holds the modifiers.
//
// TD(n) keys do the same with the five words of tap dance n (keymap_tap_dance_get): a tap sends "on tap", a hold longer than
// the term holds "on hold" (or "on tap" if that is empty), and when "on double tap" is set a second tap within the term sends it
// instead (a single tap then waits one term to be sure).
//
// While such a key is undecided, the key events that follow are kept in a small queue and replayed once it is decided, so a
// key typed during the wait lands on the right layer (QMK does the same). No "permissive hold" or "hold on other key press":
// the decision is the release (tap) or the timeout (hold).

#include <stdbool.h>
#include <stdint.h>

void tapping_init(void);

bool tapping_is_tap_hold(uint16_t keycode);

// Call first for every key event, before the keycode is looked up. While a tap-hold key is undecided the events are kept
// (true), and its own release decides it as a tap. False: no wait is going on, handle the event as usual.
bool tapping_intercept(uint8_t row, uint8_t col, bool pressed, uint16_t now);

// Then, with the keycode the key went down (or up) with. True when it is a tap-hold key: it starts the wait on a press, or
// ends a hold on its release, and there is nothing more to do with it. False: handle it as usual.
bool tapping_event(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint16_t now);

// Regularly: a key held past the tapping term becomes a hold.
void tapping_task(uint16_t now);

// Provided by the caller.
void tapping_hook_replay(uint8_t row, uint8_t col, bool pressed); // feed a queued event through the normal path
void tapping_hook_tap(uint8_t basic_keycode);                     // press and release a key
void tapping_hook_mods(uint8_t mods, bool on);                    // hold / release HID modifier bits
void tapping_hook_layer(uint8_t layer, bool on);                  // layer on / off while the key is held
void tapping_hook_keycode(uint16_t keycode, bool pressed);        // any keycode, as if a key of the keymap had it (tap dances)
