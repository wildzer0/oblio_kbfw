#include "b68_keys.h"
#include "keyboard.h"
#include "keycodes.h"
#include "leader.h"
#include "timebase.h"

// The sequences of the Leader key. They are written here, like QMK writes them in the keymap: press the Leader key (B68_LEADER, assignable from
// Vial) then these keys, in order. The keys of the sequence are not typed; the action is tapped once the whole sequence has been read.
//
// Two examples, to show the shape of the table and to try it: Leader, E, S, C = Escape; Leader, M, U, T, E = the mute key. Replace them with
// what is useful - the sequences of a real keyboard are the owner's.
typedef struct {
    const __code uint16_t *keys;
    uint8_t                count;
    uint16_t               action;
} leader_sequence_t;

static const __code uint16_t sequence_esc[]  = {KC_E, KC_S, KC_C};
static const __code uint16_t sequence_mute[] = {KC_M, KC_U, KC_T, KC_E};

static const __code leader_sequence_t sequences[] = {
    {sequence_esc, sizeof sequence_esc / sizeof sequence_esc[0], KC_ESC},
    {sequence_mute, sizeof sequence_mute / sizeof sequence_mute[0], KC_MUTE},
};

#define SEQUENCES ((uint8_t)(sizeof sequences / sizeof sequences[0]))

// The Leader key is one of the keyboard's own codes (B68_LEADER, assignable from Vial); QMK's QK_LEADER is accepted too, in case a layout
// carries one.
bool leader_hook_is_key(uint16_t keycode)
{
    return keycode == B68_LEADER || keycode == QK_LEADER;
}

leader_match_t leader_hook_match(const uint16_t *keys, uint8_t count, uint16_t *action)
{
    bool prefix = false;

    for (uint8_t i = 0; i < SEQUENCES; i++) {
        const leader_sequence_t *s = &sequences[i];

        if (count > s->count) {
            continue;
        }

        uint8_t j = 0;

        while (j < count && s->keys[j] == keys[j]) {
            j++;
        }

        if (j < count) {
            continue; // it parts company with this one before the end of what was typed
        }

        if (count == s->count) {
            *action = s->action;
            return LEADER_MATCH;
        }

        prefix = true; // this one starts like that: there is more to type
    }

    return prefix ? LEADER_PREFIX : LEADER_NONE;
}

void leader_hook_send(uint16_t keycode)
{
    keyboard_tap(keycode);
}
