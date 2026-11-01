#include "check.h"
#include "keycodes.h"
#include "leader.h"
#include <string.h>

// A board with two sequences: "esc" (E, S, C) and "mute" (M, U, T, E)
static const uint16_t seq_esc[]  = {KC_E, KC_S, KC_C};
static const uint16_t seq_mute[] = {KC_M, KC_U, KC_T, KC_E};

static uint16_t sent;
static unsigned sends;

bool leader_hook_is_key(uint16_t keycode) // on this board the Leader key is one keycode of its own
{
    return keycode == QK_LEADER;
}

leader_match_t leader_hook_match(const uint16_t *keys, uint8_t count, uint16_t *action)
{
    static const struct {
        const uint16_t *keys;
        uint8_t         count;
        uint16_t        action;
    } table[] = {
        {seq_esc, 3, KC_ESC},
        {seq_mute, 4, KC_MUTE},
    };

    bool prefix = false;

    for (uint8_t i = 0; i < 2; i++) {
        if (count > table[i].count) {
            continue;
        }

        uint8_t j = 0;

        while (j < count && table[i].keys[j] == keys[j]) {
            j++;
        }

        if (j < count) {
            continue;
        }

        if (count == table[i].count) {
            *action = table[i].action;
            return LEADER_MATCH;
        }

        prefix = true;
    }

    return prefix ? LEADER_PREFIX : LEADER_NONE;
}

void leader_hook_send(uint16_t keycode)
{
    sent = keycode;
    sends++;
}

static void reset(void)
{
    sent = 0;
    sends     = 0;
    leader_init();
}

static bool key(uint16_t keycode, bool pressed, uint16_t now)
{
    return leader_process(keycode, pressed, now);
}

static void test_not_armed_means_ordinary_typing(void)
{
    reset();
    CHECK(!key(KC_E, true, 0));
    CHECK(!leader_active());
}

static void test_a_sequence_acts_and_types_nothing(void)
{
    reset();
    CHECK(key(QK_LEADER, true, 0)); // the key that arms it: swallowed
    CHECK(key(QK_LEADER, false, 0));
    CHECK(leader_active());

    CHECK(key(KC_E, true, 10));  // a sequence: nothing is typed, nothing acts yet
    CHECK(key(KC_E, false, 10));
    CHECK(sends == 0);
    CHECK(key(KC_S, true, 20));
    CHECK(sends == 0);
    CHECK(key(KC_C, true, 30)); // and the last key of it acts
    CHECK(sends == 1 && sent == KC_ESC);
    CHECK(!leader_active());
}

static void test_a_key_outside_every_sequence_gives_up(void)
{
    reset();
    key(QK_LEADER, true, 0);

    key(KC_E, true, 10); // the start of both
    CHECK(leader_active());
    CHECK(key(KC_Z, true, 20)); // nothing starts with E, Z
    CHECK(!leader_active());
    CHECK(sends == 0);

    CHECK(!key(KC_E, true, 30)); // and typing is ordinary again
}

static void test_the_timeout_gives_up(void)
{
    reset();
    key(QK_LEADER, true, 0);

    key(KC_M, true, 10);
    CHECK(leader_active());

    leader_task(10 + LEADER_TIMEOUT_MS - 1);
    CHECK(leader_active());

    leader_task(10 + LEADER_TIMEOUT_MS); // too slow: nothing happens, and nothing is typed
    CHECK(!leader_active());
    CHECK(sends == 0);
}

static void test_the_longer_sequence_still_works(void)
{
    reset();
    key(QK_LEADER, true, 0);

    key(KC_M, true, 10); // "mute" and nothing shorter starts with M
    key(KC_U, true, 20);
    key(KC_T, true, 30);
    key(KC_E, true, 40);
    CHECK(sends == 1 && sent == KC_MUTE);
}

static void test_the_arming_key_also_disarms(void)
{
    reset();
    key(QK_LEADER, true, 0);
    CHECK(leader_active());

    key(QK_LEADER, true, 10); // the same key again
    CHECK(!leader_active());
    CHECK(sends == 0);
}

int main(void)
{
    test_not_armed_means_ordinary_typing();
    test_a_sequence_acts_and_types_nothing();
    test_a_key_outside_every_sequence_gives_up();
    test_the_timeout_gives_up();
    test_the_longer_sequence_still_works();
    test_the_arming_key_also_disarms();
    return tests_result();
}
