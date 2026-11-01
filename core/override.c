#include "override.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"

// The override that is acting on one key of the matrix: its trigger is down and the host is seeing the replacement instead
typedef struct {
    uint8_t  row;
    uint8_t  col;
    uint16_t trigger;
    uint16_t replacement;
    uint8_t  suppressed; // the modifiers taken away while it acts, to be put back on release
    bool     busy;
} acting_t;

// One per override entry: a key override is used on one key at a time (a second key with the same trigger is left alone)
static acting_t acting[KEYMAP_KEY_OVERRIDE_COUNT];

void override_init(void)
{
    for (uint8_t i = 0; i < KEYMAP_KEY_OVERRIDE_COUNT; i++) {
        acting[i].busy = false;
    }
}

// The layers an entry applies on: 0xFFFF means all of them, otherwise any of its bits being active is enough (the default layer counts as
// active, as it does for the keymap itself)
static bool layer_matches(uint16_t layers)
{
    if (layers == 0xFFFFu) {
        return true;
    }

    const uint16_t active = (uint16_t)(layer_get_state() | (uint16_t)(1u << layer_get_default()));

    return (layers & active) != 0;
}

// The index of the override that acts on this keycode now, or 0xFF
static uint8_t find(uint16_t keycode, uint8_t mods)
{
    for (uint8_t i = 0; i < KEYMAP_KEY_OVERRIDE_COUNT; i++) {
        const uint16_t trigger = keymap_override_get(i, KO_TRIGGER);
        const uint8_t  action  = (uint8_t)(keymap_override_get(i, KO_ACTION) >> 8);

        if (!(action & KO_OPTIONS_ENABLED) || trigger == 0 || trigger != keycode) {
            continue;
        }

        if (!layer_matches(keymap_override_get(i, KO_LAYERS))) {
            continue;
        }

        const uint16_t mods_word = keymap_override_get(i, KO_MODS);
        const uint8_t  required  = (uint8_t)(mods_word & 0xFFu);
        const uint8_t  negative  = (uint8_t)(mods_word >> 8);

        if ((mods & required) != required) {
            continue; // a modifier it needs is missing
        }

        if (mods & negative) {
            continue; // one that must not be held is
        }

        return i;
    }

    return 0xFFu;
}

bool override_process(uint8_t row, uint8_t col, uint16_t keycode, bool pressed, uint8_t mods)
{
    if (pressed) {
        const uint8_t index = find(keycode, mods);

        if (index == 0xFFu || acting[index].busy) {
            return false;
        }

        acting_t *a = &acting[index];
        a->row         = row;
        a->col         = col;
        a->trigger     = keycode;
        a->replacement = keymap_override_get(index, KO_REPLACEMENT);
        a->suppressed  = (uint8_t)(keymap_override_get(index, KO_ACTION) & 0xFFu);
        a->busy        = true;

        if (a->suppressed) {
            override_hook_mods(a->suppressed, false);
        }

        if (a->replacement) {
            override_hook_send(a->replacement, true);
        }

        return true; // the trigger itself does not go out
    }

    for (uint8_t i = 0; i < KEYMAP_KEY_OVERRIDE_COUNT; i++) {
        acting_t *a = &acting[i];

        if (!a->busy || a->row != row || a->col != col) {
            continue;
        }

        if (a->replacement) {
            override_hook_send(a->replacement, false);
        }

        if (a->suppressed) {
            override_hook_mods(a->suppressed, true);
        }

        a->busy = false;
        return true;
    }

    return false;
}
