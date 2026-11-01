#include "tapping.h"
#include "keycodes.h"
#include "keymap.h"

#define QUEUE_SIZE 8u

typedef struct {
    uint8_t row;
    uint8_t col;
    bool    pressed;
} queued_event_t;

static bool     pending; // a tap-hold key is down and not decided yet
static uint8_t  pending_row;
static uint8_t  pending_col;
static uint16_t pending_keycode;
static uint16_t pending_since;
static bool     second_wait; // tap dance: the first tap is over, waiting for a second press

static queued_event_t queue[QUEUE_SIZE];
static uint8_t        queued;

static uint16_t td_held[KEYMAP_TAP_DANCE_COUNT]; // the keycode a tap dance key holds down, 0 = none

void tapping_init(void)
{
    pending     = false;
    second_wait = false;
    queued      = 0;

    for (uint8_t i = 0; i < KEYMAP_TAP_DANCE_COUNT; i++) {
        td_held[i] = 0;
    }
}

bool tapping_is_tap_hold(uint16_t keycode)
{
    if (IS_QK_TAP_DANCE(keycode)) {
        return (keycode - QK_TAP_DANCE) < KEYMAP_TAP_DANCE_COUNT;
    }

    return IS_QK_LAYER_TAP(keycode) || IS_QK_MOD_TAP(keycode);
}

static uint8_t td_index(uint16_t keycode)
{
    return (uint8_t)(keycode - QK_TAP_DANCE);
}

// An action that is itself a tap dance would recurse: it does nothing.
static uint16_t td_action(uint16_t keycode, uint8_t field)
{
    const uint16_t action = keymap_tap_dance_get(td_index(keycode), field);

    return IS_QK_TAP_DANCE(action) ? 0 : action;
}

static uint16_t term_for(uint16_t keycode)
{
    if (IS_QK_TAP_DANCE(keycode)) {
        const uint16_t own = keymap_tap_dance_get(td_index(keycode), TD_TERM);

        if (own) {
            return own;
        }
    }

    return keymap_tapping_term();
}

static void hold_action(uint16_t keycode, bool on)
{
    if (IS_QK_TAP_DANCE(keycode)) {
        const uint8_t index = td_index(keycode);

        if (on) {
            uint16_t action = td_action(keycode, TD_ON_HOLD);

            if (!action) {
                action = td_action(keycode, TD_ON_TAP);
            }

            td_held[index] = action;

            if (action) {
                tapping_hook_keycode(action, true);
            }
        } else if (td_held[index]) {
            tapping_hook_keycode(td_held[index], false);
            td_held[index] = 0;
        }
        return;
    }

    if (IS_QK_LAYER_TAP(keycode)) {
        tapping_hook_layer((uint8_t)QK_LAYER_TAP_GET_LAYER(keycode), on);
    } else {
        tapping_hook_mods(MODS_FROM_MASK(QK_MOD_TAP_GET_MODS(keycode)), on);
    }
}

// Replays what piled up behind the decision. A replayed event may itself start a new wait, and the rest then stays
// queued behind it.
static void drain(void)
{
    uint8_t at = 0;

    while (at < queued && !pending) {
        const queued_event_t event = queue[at++];

        tapping_hook_replay(event.row, event.col, event.pressed);
    }

    uint8_t left = 0;

    while (at < queued) {
        queue[left++] = queue[at++];
    }

    queued = left;
}

static void decide_hold(void)
{
    pending = false;
    hold_action(pending_keycode, true);
    drain();
}

static void decide_tap(void)
{
    pending = false;

    if (IS_QK_TAP_DANCE(pending_keycode)) {
        const uint16_t action = td_action(pending_keycode, TD_ON_TAP);

        second_wait = false;

        if (action) {
            tapping_hook_keycode(action, true);
            tapping_hook_keycode(action, false);
        }
    } else if (IS_QK_LAYER_TAP(pending_keycode)) {
        tapping_hook_tap((uint8_t)QK_LAYER_TAP_GET_KEY(pending_keycode));
    } else {
        tapping_hook_tap((uint8_t)QK_MOD_TAP_GET_KEY(pending_keycode));
    }

    drain();
}

bool tapping_intercept(uint8_t row, uint8_t col, bool pressed, uint16_t now)
{
    if (!pending) {
        return false;
    }

    const bool same_key = (row == pending_row && col == pending_col);

    if (second_wait && pressed && same_key) {
        // the second tap: "on double tap" goes down now and up when this key does
        const uint8_t index = td_index(pending_keycode);

        second_wait    = false;
        pending        = false;
        td_held[index] = td_action(pending_keycode, TD_ON_DOUBLE_TAP);

        if (td_held[index]) {
            tapping_hook_keycode(td_held[index], true);
        }

        drain();
        return true;
    }

    if (!pressed && same_key) {
        if (IS_QK_TAP_DANCE(pending_keycode) && !second_wait && td_action(pending_keycode, TD_ON_DOUBLE_TAP)) {
            second_wait   = true; // maybe a double tap: wait one more term
            pending_since = now;
            return true;
        }

        decide_tap(); // let go before the term: a tap
        return true;
    }

    if (queued < QUEUE_SIZE) {
        queue[queued].row     = row;
        queue[queued].col     = col;
        queue[queued].pressed = pressed;
        queued++;
        return true;
    }

    // No room to keep waiting: the wait becomes a hold (a tap, if the first tap is already over) and this event is
    // handled after it.
    if (second_wait) {
        decide_tap();
    } else {
        decide_hold();
    }

    return false;
}

bool tapping_event(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint16_t now)
{
    if (!tapping_is_tap_hold(keycode)) {
        return false;
    }

    if (pressed) {
        pending         = true;
        pending_row     = row;
        pending_col     = col;
        pending_keycode = keycode;
        pending_since   = now;
        second_wait     = false;
    } else {
        hold_action(keycode, false); // the release of a key that became a hold
    }

    return true;
}

void tapping_task(uint16_t now)
{
    if (!pending || (uint16_t)(now - pending_since) < term_for(pending_keycode)) {
        return;
    }

    if (second_wait) {
        decide_tap(); // no second tap came
    } else {
        decide_hold();
    }
}
