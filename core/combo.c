#include "combo.h"
#include "keymap.h"

// A key being held back: where it is (so its release is recognised) and what it resolves to
typedef struct {
    uint8_t  row;
    uint8_t  col;
    uint16_t keycode;
} held_t;

static held_t   held[COMBO_INPUTS]; // the keys of a chord that has not decided yet
static uint8_t  held_count;
static uint16_t held_since;         // when the first of them went down

// The chord that fired, if any: its output is down, and it goes up when its keys are let go
static uint16_t active_output;
static held_t   absorbed[COMBO_INPUTS];
static uint8_t  absorbed_count;
static uint8_t  absorbed_down; // how many of them are still pressed

// True when `keycode` is an input of a combo that is actually configured
static bool is_input(uint16_t keycode)
{
    for (uint8_t index = 0; index < KEYMAP_COMBO_COUNT; index++) {
        if (keymap_combo_get(index, COMBO_INPUTS) == 0) {
            continue; // no output: the entry is not used
        }

        for (uint8_t i = 0; i < COMBO_INPUTS; i++) {
            if (keymap_combo_get(index, i) == keycode) {
                return true;
            }
        }
    }

    return false;
}

// The output of the combo whose inputs are exactly the held keys, or 0. Every input of the entry has to be one of the held keys, and the
// held keys have to be all of them.
static uint16_t match(void)
{
    for (uint8_t index = 0; index < KEYMAP_COMBO_COUNT; index++) {
        const uint16_t output = keymap_combo_get(index, COMBO_INPUTS);

        if (output == 0) {
            continue;
        }

        uint8_t inputs = 0;
        bool    ok     = true;

        for (uint8_t i = 0; i < COMBO_INPUTS && ok; i++) {
            const uint16_t input = keymap_combo_get(index, i);

            if (input == 0) {
                continue;
            }

            inputs++;

            bool found = false;

            for (uint8_t j = 0; j < held_count && !found; j++) {
                found = (held[j].keycode == input);
            }

            ok = found;
        }

        if (ok && inputs == held_count) {
            return output;
        }
    }

    return 0;
}

// The keys held back go out now, in the order they were pressed
static void flush(void)
{
    for (uint8_t i = 0; i < held_count; i++) {
        combo_hook_send(held[i].keycode, true);
    }

    held_count = 0;
}

void combo_init(void)
{
    held_count    = 0;
    held_since    = 0;
    active_output = 0;
    absorbed_count = 0;
    absorbed_down  = 0;
}

static void fire(uint16_t output)
{
    for (uint8_t i = 0; i < held_count; i++) {
        absorbed[i] = held[i];
    }

    absorbed_count = held_count;
    absorbed_down  = held_count;
    held_count     = 0;
    active_output  = output;

    combo_hook_send(output, true);
}

static bool absorbed_release(uint8_t row, uint8_t col)
{
    for (uint8_t i = 0; i < absorbed_count; i++) {
        if (absorbed[i].row == row && absorbed[i].col == col) {
            return true;
        }
    }

    return false;
}

static bool held_release(uint8_t row, uint8_t col)
{
    for (uint8_t i = 0; i < held_count; i++) {
        if (held[i].row == row && held[i].col == col) {
            held[i] = held[--held_count]; // order no longer matters: it was never sent
            return true;
        }
    }

    return false;
}

bool combo_process(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint16_t now)
{
    if (!pressed) {
        // The keys of the chord that fired: the output goes up with the last of them
        if (active_output && absorbed_release(row, col)) {
            if (absorbed_down) {
                absorbed_down--;
            }

            if (!absorbed_down) {
                combo_hook_send(active_output, false);
                active_output  = 0;
                absorbed_count = 0;
            }

            return true;
        }

        // A held key let go before the chord closed: nothing was sent, nothing to undo
        return held_release(row, col);
    }

    const bool candidate = is_input(keycode);

    if (!candidate && !held_count) {
        return false; // nothing to wait for: ordinary typing
    }

    if (held_count && !candidate) {
        flush(); // this key is not part of any chord: the held ones go first
        return false;
    }

    if (held_count == COMBO_INPUTS) {
        flush(); // no room to wait for more
        return false;
    }

    if (!held_count) {
        held_since = now;
    }

    held[held_count].row     = row;
    held[held_count].col     = col;
    held[held_count].keycode = keycode;
    held_count++;

    const uint16_t output = match();

    if (output) {
        fire(output);
    }

    return true; // held back, or turned into the combo
}

void combo_task(uint16_t now)
{
    if (held_count && (uint16_t)(now - held_since) >= COMBO_TERM_MS) {
        flush();
    }
}
