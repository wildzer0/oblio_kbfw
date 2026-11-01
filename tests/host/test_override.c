#include "check.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"
#include "override.h"
#include <string.h>

// The override entries as the keymap would hold them, and the layers as the layer engine has them
static uint16_t table[KEYMAP_KEY_OVERRIDE_COUNT][KO_FIELDS];
static uint8_t layers_active;
static uint8_t default_layers;

uint16_t keymap_override_get(uint8_t index, uint8_t field)
{
    return (index < KEYMAP_KEY_OVERRIDE_COUNT && field < KO_FIELDS) ? table[index][field] : 0;
}

// core/override.c asks the layer engine which layers are on
uint8_t layer_get_state(void)
{
    return layers_active;
}

uint8_t layer_get_default(void)
{
    return default_layers;
}

// What reached the host
typedef struct {
    uint16_t keycode;
    bool     pressed;
} sent_t;

static sent_t   sent[16];
static unsigned sent_count;
static uint8_t  mods_added;
static uint8_t  mods_removed;

void override_hook_send(uint16_t keycode, bool pressed)
{
    if (sent_count < 16) {
        sent[sent_count].keycode = keycode;
        sent[sent_count].pressed = pressed;
        sent_count++;
    }
}

void override_hook_mods(uint8_t mods, bool on)
{
    if (on) {
        mods_added |= mods;
    } else {
        mods_removed |= mods;
    }
}

static void set(uint8_t index, uint16_t trigger, uint16_t replacement, uint16_t layers, uint8_t required, uint8_t negative, uint8_t suppressed, uint8_t options)
{
    table[index][KO_TRIGGER]     = trigger;
    table[index][KO_REPLACEMENT] = replacement;
    table[index][KO_LAYERS]      = layers;
    table[index][KO_MODS]        = (uint16_t)(required | ((uint16_t)negative << 8));
    table[index][KO_ACTION]      = (uint16_t)(suppressed | ((uint16_t)options << 8));
}

static void reset(void)
{
    memset(table, 0, sizeof table);
    // Shift + Backspace = Delete, on every layer, Shift kept
    set(0, KC_BSPC, KC_DEL, 0xFFFF, MOD_LSFT, 0, 0, KO_OPTIONS_ENABLED);

    layers_active  = 0;
    default_layers = 0;
    sent_count     = 0;
    mods_added     = 0;
    mods_removed   = 0;
    override_init();
}

static void test_without_the_modifiers_the_key_is_left_alone(void)
{
    reset();

    CHECK(!override_process(1, 1, KC_BSPC, true, 0));  // no Shift: an ordinary Backspace
    CHECK(!override_process(1, 1, KC_BSPC, false, 0));
    CHECK(sent_count == 0);

    CHECK(!override_process(1, 1, KC_A, true, MOD_LSFT)); // a key no override mentions
}

static void test_the_trigger_becomes_the_replacement(void)
{
    reset();

    CHECK(override_process(1, 1, KC_BSPC, true, MOD_LSFT)); // Shift+Backspace
    CHECK(sent_count == 1 && sent[0].keycode == KC_DEL && sent[0].pressed);

    CHECK(override_process(1, 1, KC_BSPC, false, MOD_LSFT));
    CHECK(sent_count == 2 && sent[1].keycode == KC_DEL && !sent[1].pressed);
}

static void test_a_suppressed_modifier_goes_away_while_it_acts(void)
{
    reset();
    // Shift + Backspace = Delete, and the Shift does not reach the host while it acts
    set(0, KC_BSPC, KC_DEL, 0xFFFF, MOD_LSFT, 0, MOD_LSFT, KO_OPTIONS_ENABLED);

    override_process(1, 1, KC_BSPC, true, MOD_LSFT);
    CHECK(mods_removed == MOD_LSFT && mods_added == 0);

    override_process(1, 1, KC_BSPC, false, MOD_LSFT);
    CHECK(mods_added == MOD_LSFT);
}

static void test_the_modifiers_it_must_not_see(void)
{
    reset();
    // Ctrl + Backspace = Delete, but only when Alt is NOT held
    set(0, KC_BSPC, KC_DEL, 0xFFFF, MOD_LCTL, MOD_LALT, 0, KO_OPTIONS_ENABLED);

    CHECK(override_process(1, 1, KC_BSPC, true, MOD_LCTL));
    CHECK(sent_count == 1 && sent[0].keycode == KC_DEL);

    reset();
    set(0, KC_BSPC, KC_DEL, 0xFFFF, MOD_LCTL, MOD_LALT, 0, KO_OPTIONS_ENABLED);
    CHECK(!override_process(1, 1, KC_BSPC, true, MOD_LCTL | MOD_LALT)); // Alt is held: nothing happens
    CHECK(sent_count == 0);
}

static void test_an_override_can_belong_to_a_layer(void)
{
    reset();
    set(0, KC_A, KC_1, 1u << 2, 0, 0, 0, KO_OPTIONS_ENABLED); // only on layer 2

    CHECK(!override_process(1, 1, KC_A, true, 0)); // layer 0: a plain A

    layers_active = 1u << 2;
    CHECK(override_process(1, 1, KC_A, true, 0));
    CHECK(sent_count == 1 && sent[0].keycode == KC_1);
}

static void test_a_disabled_entry_does_nothing(void)
{
    reset();
    set(0, KC_BSPC, KC_DEL, 0xFFFF, MOD_LSFT, 0, 0, 0); // bit 7 clear = disabled

    CHECK(!override_process(1, 1, KC_BSPC, true, MOD_LSFT));
    CHECK(sent_count == 0);
}

static void test_the_release_of_another_key_does_not_end_it(void)
{
    reset();

    CHECK(override_process(1, 1, KC_BSPC, true, MOD_LSFT));
    CHECK(!override_process(2, 2, KC_A, false, MOD_LSFT)); // another key let go
    CHECK(sent_count == 1);                                // the Delete is still down

    CHECK(override_process(1, 1, KC_BSPC, false, MOD_LSFT));
    CHECK(sent_count == 2 && !sent[1].pressed);
}

int main(void)
{
    test_without_the_modifiers_the_key_is_left_alone();
    test_the_trigger_becomes_the_replacement();
    test_a_suppressed_modifier_goes_away_while_it_acts();
    test_the_modifiers_it_must_not_see();
    test_an_override_can_belong_to_a_layer();
    test_a_disabled_entry_does_nothing();
    test_the_release_of_another_key_does_not_end_it();
    return tests_result();
}
