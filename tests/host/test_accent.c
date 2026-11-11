#include "check.h"
#include "accent.h"
#include "keycodes.h"
#include <string.h>

// What the sequence does, as the keyboard module would do it: every key event and every change of the modifiers, in order
typedef struct {
    uint8_t kind; // 'k' a key (down or up), 'm' the modifier byte set to `value`
    uint8_t key;
    bool    down;
    uint8_t value;
} log_t;

static log_t   events[64];
static uint8_t n_events;
static uint8_t mods;

void accent_hook_key(uint16_t keycode, bool down)
{
    events[n_events++] = (log_t){'k', (uint8_t)keycode, down, 0};
}

uint8_t accent_hook_mods_get(void)
{
    return mods;
}

void accent_hook_mods_set(uint8_t value)
{
    mods                = value;
    events[n_events++] = (log_t){'m', 0, false, value};
}

static uint16_t now;

static void reset(uint8_t held_mods)
{
    n_events = 0;
    mods     = held_mods;
    now      = 1000;
}

// Plays the whole sequence, one task call every millisecond, and returns how many milliseconds it took
static unsigned play(void)
{
    unsigned elapsed = 0;

    while (accent_busy() && elapsed < 500) {
        now++;
        elapsed++;
        accent_task(now);
    }

    return elapsed;
}

// The keys of the events, as a string of codes: "k" + key + "+/-"; compared with an expected list of (key, down) pairs
static bool is(const uint16_t *expected, unsigned length)
{
    unsigned keys = 0;

    for (unsigned i = 0; i < n_events; i++) {
        if (events[i].kind != 'k') {
            continue;
        }

        if (keys >= length || events[i].key != (expected[keys] & 0xFFu) || events[i].down != ((expected[keys] & 0x100u) != 0)) {
            return false;
        }

        keys++;
    }

    return keys == length;
}

#define D 0x100u // down

static void test_windows_alt_code(void)
{
    reset(0);
    accent_start(now, 1, false, ACCENT_OS_WINDOWS, true); // è, Num Lock already on: Alt 0 2 3 2
    const unsigned ms = play();

    const uint16_t want[] = {KC_LALT | D, KC_KP_0 | D, KC_KP_0, KC_KP_2 | D, KC_KP_2, KC_KP_3 | D, KC_KP_3, KC_KP_2 | D, KC_KP_2, KC_LALT};
    CHECK(is(want, sizeof want / sizeof want[0]));
    CHECK(n_events == 10); // no modifier change: none were down
    CHECK(!accent_busy());
    CHECK(ms >= 10 * ACCENT_STEP_MS && ms <= 12 * ACCENT_STEP_MS); // one step every 4 ms
}

static void test_windows_capital_and_every_letter(void)
{
    static const uint8_t lower[6] = {224, 232, 233, 236, 242, 249};
    static const uint8_t upper[6] = {192, 200, 201, 204, 210, 217};

    for (uint8_t i = 0; i < 6; i++) {
        for (uint8_t cap = 0; cap < 2; cap++) {
            reset(0);
            accent_start(now, i, cap != 0, ACCENT_OS_WINDOWS, true);
            play();

            const uint8_t code = cap ? upper[i] : lower[i];
            const uint8_t digit[3] = {(uint8_t)(code / 100), (uint8_t)((code / 10) % 10), (uint8_t)(code % 10)};
            // events: Alt down, 0, d1, d2, d3, Alt up -> the downs of the keypad keys are at 2, 4, 6, 8
            unsigned seen = 0;

            for (unsigned e = 0; e < n_events; e++) {
                if (events[e].kind == 'k' && events[e].down && events[e].key != KC_LALT && seen++ >= 1) {
                    const uint8_t d = digit[seen - 2];
                    CHECK(events[e].key == (d == 0 ? KC_KP_0 : (uint8_t)(KC_KP_1 + d - 1)));
                }
            }

            CHECK(seen == 4); // the zero and the three digits
        }
    }
}

static void test_windows_turns_num_lock_on_and_back_off(void)
{
    reset(0);
    accent_start(now, 0, false, ACCENT_OS_WINDOWS, false); // à, Num Lock OFF on the host
    play();

    const uint16_t want[] = {KC_NUM_LOCK | D, KC_NUM_LOCK, KC_LALT | D, KC_KP_0 | D, KC_KP_0, KC_KP_2 | D, KC_KP_2, KC_KP_2 | D, KC_KP_2, KC_KP_4 | D, KC_KP_4, KC_LALT, KC_NUM_LOCK | D, KC_NUM_LOCK};
    CHECK(is(want, sizeof want / sizeof want[0]));
}

static void test_mac(void)
{
    reset(0);
    accent_start(now, 1, false, ACCENT_OS_MAC, true); // è: Option+` then e
    play();
    const uint16_t grave[] = {KC_LALT | D, KC_GRV | D, KC_GRV, KC_LALT, KC_E | D, KC_E};
    CHECK(is(grave, sizeof grave / sizeof grave[0]));

    reset(0);
    accent_start(now, 2, false, ACCENT_OS_MAC, true); // é: Option+e then e
    play();
    const uint16_t acute[] = {KC_LALT | D, KC_E | D, KC_E, KC_LALT, KC_E | D, KC_E};
    CHECK(is(acute, sizeof acute / sizeof acute[0]));

    reset(0);
    accent_start(now, 5, true, ACCENT_OS_MAC, true); // Ù: Shift on the vowel
    play();
    const uint16_t capital[] = {KC_LALT | D, KC_GRV | D, KC_GRV, KC_LALT, KC_LSFT | D, KC_U | D, KC_U, KC_LSFT};
    CHECK(is(capital, sizeof capital / sizeof capital[0]));
}

static void test_linux(void)
{
    reset(0);
    accent_start(now, 1, false, ACCENT_OS_LINUX, true); // è = U+00E8: Ctrl+Shift+U, e, 8, space
    play();
    const uint16_t want[] = {KC_LCTL | D, KC_LSFT | D, KC_U | D, KC_U, KC_LSFT, KC_LCTL, KC_E | D, KC_E, KC_8 | D, KC_8, KC_SPC | D, KC_SPC};
    CHECK(is(want, sizeof want / sizeof want[0]));

    reset(0);
    accent_start(now, 0, true, ACCENT_OS_LINUX, true); // À = U+00C0: the digit 0 is the key 0
    play();
    const uint16_t capital[] = {KC_LCTL | D, KC_LSFT | D, KC_U | D, KC_U, KC_LSFT, KC_LCTL, KC_C | D, KC_C, KC_0 | D, KC_0, KC_SPC | D, KC_SPC};
    CHECK(is(capital, sizeof capital / sizeof capital[0]));
}

static void test_the_modifiers_are_put_aside_and_back(void)
{
    reset(0x22); // both Shifts down (as for a capital typed with Shift held)
    accent_start(now, 3, true, ACCENT_OS_WINDOWS, true);

    accent_task(now + ACCENT_STEP_MS); // the first step
    CHECK(n_events == 1 && events[0].kind == 'm' && events[0].value == 0); // they come off before anything is typed

    play();
    CHECK(events[n_events - 1].kind == 'm' && events[n_events - 1].value == 0x22); // and go back at the end
    CHECK(mods == 0x22);
    CHECK(!accent_busy());
}

static void test_it_is_busy_until_the_end_and_ignores_a_second_start(void)
{
    reset(0);
    accent_start(now, 0, false, ACCENT_OS_MAC, true);
    CHECK(accent_busy());
    accent_start(now, 5, true, ACCENT_OS_LINUX, true); // ignored: one at a time
    play();
    CHECK(n_events == 6); // the Mac à only (Option+`, a: six events)

    accent_start(now, 9, false, ACCENT_OS_MAC, true); // an index out of range does nothing
    CHECK(!accent_busy());
}

static void test_one_step_per_four_milliseconds(void)
{
    reset(0);
    accent_start(now, 0, false, ACCENT_OS_MAC, true);

    accent_task(now);                      // too soon: the first step is due after the step time
    CHECK(n_events == 0);
    accent_task(now + 1);
    accent_task(now + 3);
    CHECK(n_events == 0);
    accent_task((uint16_t)(now + ACCENT_STEP_MS));
    CHECK(n_events == 1);
    accent_task((uint16_t)(now + ACCENT_STEP_MS + 1));
    CHECK(n_events == 1);
    accent_task((uint16_t)(now + 2 * ACCENT_STEP_MS));
    CHECK(n_events == 2);

    now = (uint16_t)(now + 2 * ACCENT_STEP_MS);
    play(); // let it finish: the next test starts a new one
}

static void test_the_clock_wraps(void)
{
    reset(0);
    now = 0xFFF0u;
    accent_start(now, 2, false, ACCENT_OS_MAC, true);
    play();
    CHECK(!accent_busy() && n_events == 6);
}

int main(void)
{
    test_windows_alt_code();
    test_windows_capital_and_every_letter();
    test_windows_turns_num_lock_on_and_back_off();
    test_mac();
    test_linux();
    test_the_modifiers_are_put_aside_and_back();
    test_it_is_busy_until_the_end_and_ignores_a_second_start();
    test_one_step_per_four_milliseconds();
    test_the_clock_wraps();
    return tests_result();
}
