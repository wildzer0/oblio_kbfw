#include "check.h"
#include "combo.h"
#include "keycodes.h"
#include "keymap.h"
#include <string.h>

// The combos as the keymap would hold them: two real ones and one that is not configured
static uint16_t table[KEYMAP_COMBO_COUNT][COMBO_FIELDS];

uint16_t keymap_combo_get(uint8_t index, uint8_t field)
{
    return (index < KEYMAP_COMBO_COUNT && field < COMBO_FIELDS) ? table[index][field] : 0;
}

// What reached the host
typedef struct {
    uint16_t keycode;
    bool     pressed;
} sent_t;

static sent_t   sent[32];
static unsigned sent_count;

void combo_hook_send(uint16_t keycode, bool pressed)
{
    if (sent_count < 32) {
        sent[sent_count].keycode = keycode;
        sent[sent_count].pressed = pressed;
        sent_count++;
    }
}

static void reset(void)
{
    memset(table, 0, sizeof table);
    table[0][0] = KC_J;
    table[0][1] = KC_K;
    table[0][COMBO_INPUTS] = KC_ESC; // j + k = Escape

    table[1][0] = KC_A;
    table[1][1] = KC_B;
    table[1][2] = KC_C;
    table[1][COMBO_INPUTS] = KC_ENT; // a + b + c = Enter

    sent_count = 0;
    combo_init();
}

// A key of the matrix: (row, col) only matters to tell two keys apart
static bool key(uint8_t n, uint16_t keycode, bool pressed, uint16_t now)
{
    return combo_process(0, n, keycode, pressed, now);
}

static void test_a_key_that_is_not_in_a_combo_goes_straight_through(void)
{
    reset();

    CHECK(!key(1, KC_X, true, 0));
    CHECK(sent_count == 0); // the caller sends it, not the combo engine
}

static void test_a_chord_fires_and_the_keys_of_it_never_reach_the_host(void)
{
    reset();

    CHECK(key(1, KC_J, true, 0));    // held back
    CHECK(sent_count == 0);
    CHECK(key(2, KC_K, true, 10));   // the chord closes
    CHECK(sent_count == 1 && sent[0].keycode == KC_ESC && sent[0].pressed);

    CHECK(key(1, KC_J, false, 20));  // one key of the chord let go: the output stays down
    CHECK(sent_count == 1);
    CHECK(key(2, KC_K, false, 30));  // the last one: the output goes up
    CHECK(sent_count == 2 && sent[1].keycode == KC_ESC && !sent[1].pressed);
}

static void test_the_term_expiring_sends_the_held_key(void)
{
    reset();

    CHECK(key(1, KC_J, true, 0));
    CHECK(sent_count == 0);

    combo_task(COMBO_TERM_MS - 1);
    CHECK(sent_count == 0);

    combo_task(COMBO_TERM_MS); // too slow for a chord: the key goes out on its own
    CHECK(sent_count == 1 && sent[0].keycode == KC_J && sent[0].pressed);

    // K is an input of the same combo, so it is held back in its turn and goes out when its own term expires: the keys of a chord pay
    // the latency one by one, the keys outside any chord pay nothing
    CHECK(key(2, KC_K, true, COMBO_TERM_MS + 1));
    CHECK(sent_count == 1);
    combo_task(2 * COMBO_TERM_MS + 1);
    CHECK(sent_count == 2 && sent[1].keycode == KC_K);
}

static void test_a_key_let_go_before_the_chord_is_dropped(void)
{
    reset();

    CHECK(key(1, KC_J, true, 0));
    CHECK(key(1, KC_J, false, 5)); // let go: it was never sent
    CHECK(sent_count == 0);

    combo_task(1000);
    CHECK(sent_count == 0);
}

static void test_a_key_outside_the_chord_flushes_the_held_ones_first(void)
{
    reset();

    CHECK(key(1, KC_J, true, 0));
    CHECK(!key(3, KC_X, true, 5)); // not part of anything: the held J goes first
    CHECK(sent_count == 1 && sent[0].keycode == KC_J);

    combo_task(1000);
    CHECK(sent_count == 1); // and the X is the caller's, not ours
}

static void test_a_three_key_chord(void)
{
    reset();

    key(1, KC_A, true, 0);
    key(2, KC_B, true, 10);
    CHECK(sent_count == 0); // not a chord yet
    key(3, KC_C, true, 20);
    CHECK(sent_count == 1 && sent[0].keycode == KC_ENT);

    key(3, KC_C, false, 30);
    key(2, KC_B, false, 31);
    key(1, KC_A, false, 32);
    CHECK(sent_count == 2 && !sent[1].pressed);
}

static void test_a_combo_with_no_output_is_ignored(void)
{
    reset();

    table[1][COMBO_INPUTS] = 0; // a and b, but no output: not a combo

    CHECK(!key(1, KC_A, true, 0));
    CHECK(!key(2, KC_B, true, 10));
    CHECK(sent_count == 0);
}

static void test_the_same_key_twice_is_not_a_chord(void)
{
    reset();

    key(1, KC_J, true, 0);
    CHECK(key(4, KC_J, true, 5)); // a second J is not the K the combo wants
    combo_task(COMBO_TERM_MS);
    CHECK(sent_count == 2); // both go out, in the order they were pressed
    CHECK(sent[0].keycode == KC_J && sent[1].keycode == KC_J);
}

int main(void)
{
    test_a_key_that_is_not_in_a_combo_goes_straight_through();
    test_a_chord_fires_and_the_keys_of_it_never_reach_the_host();
    test_the_term_expiring_sends_the_held_key();
    test_a_key_let_go_before_the_chord_is_dropped();
    test_a_key_outside_the_chord_flushes_the_held_ones_first();
    test_a_three_key_chord();
    test_a_combo_with_no_output_is_ignored();
    test_the_same_key_twice_is_not_a_chord();
    return tests_result();
}
