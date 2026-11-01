#include "layer.h"
#include "keycodes.h"
#include "keymap.h"

static uint16_t down_keycode[MATRIX_ROWS][MATRIX_COLS]; // what each key resolved to when it went down

static uint8_t layer_state;   // bit n = layer n is on
static uint8_t default_layer; // always part of the active set, tried last

// TT (layer tap toggle)
static uint8_t  tt_layer = LAYER_NONE;
static uint8_t  tt_count;   // taps in a row
static uint16_t tt_down;    // when the key went down
static uint16_t tt_last;    // when it was last let go
static bool     tt_toggled; // the layer is latched on

// OSL (one-shot layer)
static uint8_t os_layer = LAYER_NONE;
static bool    os_held;
static bool    os_used; // a key went down while the OSL key was held

// OSM (one-shot modifiers), HID modifier-byte bits
static uint8_t osm_held;    // OSM keys currently down
static uint8_t osm_used;    // of those, the ones a key was pressed under
static uint8_t osm_armed;   // tapped, waiting for the next key
static uint8_t osm_pending; // armed mods applied to a key that is still down

void layer_init(void)
{
    layer_state   = 0;
    default_layer = 0;
    tt_layer      = LAYER_NONE;
    tt_count      = 0;
    tt_toggled    = false;
    os_layer      = LAYER_NONE;
    os_held       = false;
    os_used       = false;
    osm_held      = 0;
    osm_used      = 0;
    osm_armed     = 0;
    osm_pending   = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            down_keycode[row][col] = KC_NO;
        }
    }
}

static bool layer_valid(uint8_t layer)
{
    return layer < KEYMAP_LAYERS;
}

static void layer_on(uint8_t layer)
{
    layer_state |= (uint8_t)(1u << layer);
}

static void layer_off(uint8_t layer)
{
    layer_state &= (uint8_t)~(1u << layer);
}

void layer_set_default(uint8_t layer)
{
    if (layer_valid(layer)) {
        default_layer = layer;
    }
}

uint8_t layer_get_default(void)
{
    return default_layer;
}

void layer_hold(uint8_t layer, bool on)
{
    if (!layer_valid(layer)) {
        return;
    }

    if (on) {
        layer_on(layer);
    } else {
        layer_off(layer);
    }
}

uint8_t layer_get_state(void)
{
    return layer_state;
}

uint8_t layer_oneshot_layer(void)
{
    return os_layer;
}

uint8_t layer_oneshot_mods(void)
{
    return osm_armed;
}

uint16_t layer_lookup(uint8_t row, uint8_t col)
{
    for (uint8_t layer = KEYMAP_LAYERS; layer-- > 0;) {
        if (!(layer_state & (uint8_t)(1u << layer))) {
            continue;
        }

        const uint16_t keycode = keymap_get(layer, row, col);

        if (keycode != KC_TRANSPARENT) {
            return keycode;
        }
    }

    const uint16_t keycode = keymap_get(default_layer, row, col);
    return (keycode == KC_TRANSPARENT) ? KC_NO : keycode;
}

uint16_t layer_lookup_encoder(uint8_t encoder, bool clockwise)
{
    for (uint8_t layer = KEYMAP_LAYERS; layer-- > 0;) {
        if (!(layer_state & (uint8_t)(1u << layer))) {
            continue;
        }

        const uint16_t keycode = keymap_get_encoder(layer, encoder, clockwise);

        if (keycode != KC_TRANSPARENT) {
            return keycode;
        }
    }

    const uint16_t keycode = keymap_get_encoder(default_layer, encoder, clockwise);
    return (keycode == KC_TRANSPARENT) ? KC_NO : keycode;
}

uint16_t layer_key_down(uint8_t row, uint8_t col)
{
    const uint16_t keycode = layer_lookup(row, col);

    down_keycode[row][col] = keycode;
    return keycode;
}

uint16_t layer_key_up(uint8_t row, uint8_t col)
{
    const uint16_t keycode = down_keycode[row][col];

    down_keycode[row][col] = KC_NO;
    return keycode;
}

static void osl_finish(void)
{
    if (os_layer != LAYER_NONE) {
        layer_off(os_layer);
        os_layer = LAYER_NONE;
    }
}

static void process_osl(uint8_t layer, bool pressed)
{
    if (pressed) {
        osl_finish(); // an earlier one-shot still waiting is dropped
        layer_on(layer);
        os_layer = layer;
        os_held  = true;
        os_used  = false;
    } else if (os_layer == layer) {
        os_held = false;

        if (os_used) {
            osl_finish();
        }
    }
}

static void process_tt(uint8_t layer, bool pressed, uint16_t now)
{
    if (pressed) {
        if (tt_layer != layer || (uint16_t)(now - tt_last) > LAYER_TT_TERM) {
            tt_count = 0; // a pause or another layer starts the count over
        }

        if (tt_layer != layer) {
            tt_toggled = false;
        }

        tt_layer = layer;
        tt_down  = now;
        layer_on(layer);
        return;
    }

    tt_count = ((uint16_t)(now - tt_down) < LAYER_TT_TERM) ? (uint8_t)(tt_count + 1) : 0;
    tt_last  = now;

    if (tt_count >= LAYER_TT_TAPS) {
        tt_count   = 0;
        tt_toggled = !tt_toggled;
    }

    if (!tt_toggled) {
        layer_off(layer);
    }
}

static void process_osm(uint8_t mods, bool pressed)
{
    if (pressed) {
        layer_hook_mods_add(mods);
        osm_held |= mods;
        return;
    }

    osm_held &= (uint8_t)~mods;

    if (osm_used & mods) {
        osm_used &= (uint8_t)~mods;
        layer_hook_mods_del(mods);
    } else {
        osm_armed |= mods; // tapped: the modifiers stay on for the next key
    }
}

bool layer_process(uint16_t keycode, bool pressed, uint16_t now)
{
    if (IS_QK_MOMENTARY(keycode)) {
        layer_hold((uint8_t)(keycode & 0x1F), pressed);
        return true;
    }

    if (IS_QK_TO(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);

        if (pressed && layer_valid(layer)) {
            layer_state = (uint8_t)(1u << layer);
            os_layer    = LAYER_NONE;
        }
        return true;
    }

    if (IS_QK_TOGGLE_LAYER(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);

        if (pressed && layer_valid(layer)) {
            layer_state ^= (uint8_t)(1u << layer);
        }
        return true;
    }

    if (IS_QK_DEF_LAYER(keycode)) {
        if (pressed) {
            layer_set_default((uint8_t)(keycode & 0x1F));
        }
        return true;
    }

    if (IS_QK_ONE_SHOT_LAYER(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);

        if (layer_valid(layer)) {
            process_osl(layer, pressed);
        }
        return true;
    }

    if (IS_QK_LAYER_MOD(keycode)) {
        const uint8_t layer = (uint8_t)QK_LAYER_MOD_GET_LAYER(keycode);
        const uint8_t mods  = MODS_FROM_MASK(QK_LAYER_MOD_GET_MODS(keycode));

        if (layer_valid(layer)) {
            layer_hold(layer, pressed);

            if (pressed) {
                layer_hook_mods_add(mods);
            } else {
                layer_hook_mods_del(mods);
            }
        }
        return true;
    }

    if (IS_QK_LAYER_TAP_TOGGLE(keycode)) {
        const uint8_t layer = (uint8_t)(keycode & 0x1F);

        if (layer_valid(layer)) {
            process_tt(layer, pressed, now);
        }
        return true;
    }

    if (IS_QK_ONE_SHOT_MOD(keycode)) {
        process_osm(MODS_FROM_MASK(keycode & 0x1Fu), pressed);
        return true;
    }

    return false;
}

void layer_key_pressed(void)
{
    if (os_layer != LAYER_NONE) {
        if (os_held) {
            os_used = true; // used under the held OSL key: it ends when that key is let go
        } else {
            osl_finish(); // tapped OSL: this key was the one it was waiting for
        }
    }

    osm_used |= osm_held;

    if (osm_armed) {
        osm_pending |= osm_armed; // stays on until this key is released
        osm_armed = 0;
    }
}

void layer_key_released(void)
{
    if (osm_pending) {
        layer_hook_mods_del(osm_pending);
        osm_pending = 0;
    }
}
