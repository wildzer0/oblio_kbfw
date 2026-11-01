#pragma once

// Leader (QMK's): a key starts a sequence, and the keys pressed after it are not typed - they are matched against a table of sequences, and the
// one that matches acts. QMK keeps that table in the keymap too: it is written at compile time, not edited from Vial. Here it lives in
// board/leader.c.
//
// The sequence abandons itself on a key that no sequence starts with, or after LEADER_TIMEOUT_MS of silence.

#include <stdbool.h>
#include <stdint.h>

#define LEADER_KEYS       6
#define LEADER_TIMEOUT_MS 400u

typedef enum {
    LEADER_NONE,   // no sequence starts like this
    LEADER_PREFIX, // a sequence starts like this: wait for more
    LEADER_MATCH,  // a whole sequence: act
} leader_match_t;

// Provided by the board: is this the key that arms and disarms the Leader? It is one of the board's own keycodes.
bool leader_hook_is_key(uint16_t keycode);

// Provided by the board: does this run of keycodes match a sequence? A match also gives the keycode to tap.
leader_match_t leader_hook_match(const uint16_t *keys, uint8_t count, uint16_t *action);
void           leader_hook_send(uint16_t keycode);

void leader_init(void);
bool leader_active(void);

// A key went down or up. True while the leader is collecting (or is on the key that turns it on): the key must not be typed.
bool leader_process(uint16_t keycode, bool pressed, uint16_t now);

// Regularly from the main loop: gives up after the timeout.
void leader_task(uint16_t now);
