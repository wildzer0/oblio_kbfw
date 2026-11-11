#include "accent.h"
#include "keycodes.h"
#include "platform.h"

#if !LOOP_PROFILE

// Per accent: the Latin-1 code of the lower and upper case (Windows and Linux type it), and for the Mac the key that is pressed with Option and the
// vowel that follows it. è is Option+` then E; é is Option+E then E (the same two keys).
typedef struct {
    uint8_t lower;
    uint8_t upper;
    uint8_t dead;
    uint8_t vowel;
} accent_t;

static const ROM accent_t accents[ACCENT_COUNT] = {
    {0xE0, 0xC0, KC_GRV, KC_A}, // à À
    {0xE8, 0xC8, KC_GRV, KC_E}, // è È
    {0xE9, 0xC9, KC_E, KC_E},   // é É
    {0xEC, 0xCC, KC_GRV, KC_I}, // ì Ì
    {0xF2, 0xD2, KC_GRV, KC_O}, // ò Ò
    {0xF9, 0xD9, KC_GRV, KC_U}, // ù Ù
};

#define DOWN       0x100u // in an event: the key goes down (up when absent); the key itself is the low byte
#define MAX_EVENTS 20u    // the longest is Windows with Num Lock off: 14 events (a margin of a few)

static uint16_t events[MAX_EVENTS];
static uint8_t  count;      // events in the list
static uint8_t  at;         // the step being played: 0 puts the modifiers aside, 1..count the events, count + 1 puts them back
static uint8_t  saved_mods; // the modifiers that were down when the sequence started
static uint16_t last;       // when the last step was played
static bool     busy;

static void push(uint8_t key, bool down)
{
    events[count++] = (uint16_t)(key | (down ? DOWN : 0u));
}

static void tap(uint8_t key)
{
    push(key, true);
    push(key, false);
}

// The key of a decimal digit on the numeric keypad, and of a hex digit on the main keys
static uint8_t keypad(uint8_t digit)
{
    return (digit == 0) ? KC_KP_0 : (uint8_t)(KC_KP_1 + digit - 1u);
}

static uint8_t hex_digit(uint8_t nibble)
{
    if (nibble < 10u) {
        return (nibble == 0) ? KC_0 : (uint8_t)(KC_1 + nibble - 1u);
    }

    return (uint8_t)(KC_A + nibble - 10u);
}

void accent_start(uint16_t now, uint8_t index, bool upper, accent_os_t os, bool num_lock)
{
    if (busy || index >= ACCENT_COUNT) {
        return;
    }

    const uint8_t code  = upper ? accents[index].upper : accents[index].lower;
    const uint8_t dead  = accents[index].dead;
    const uint8_t vowel = accents[index].vowel;

    count = 0;

    if (os == ACCENT_OS_MAC) {
        push(KC_LALT, true);
        tap(dead);
        push(KC_LALT, false);

        if (upper) {
            push(KC_LSFT, true);
        }

        tap(vowel);

        if (upper) {
            push(KC_LSFT, false);
        }
    } else if (os == ACCENT_OS_LINUX) {
        push(KC_LCTL, true);
        push(KC_LSFT, true);
        tap(KC_U);
        push(KC_LSFT, false);
        push(KC_LCTL, false);
        tap(hex_digit(code >> 4));
        tap(hex_digit(code & 0x0Fu));
        tap(KC_SPC);
    } else { // Windows: Alt, then 0 and the three decimal digits of the code, on the keypad
        if (!num_lock) {
            tap(KC_NUM_LOCK);
        }

        push(KC_LALT, true);
        tap(KC_KP_0);
        tap(keypad(code / 100u));
        tap(keypad((code / 10u) % 10u));
        tap(keypad(code % 10u));
        push(KC_LALT, false);

        if (!num_lock) {
            tap(KC_NUM_LOCK); // as it was
        }
    }

    saved_mods = accent_hook_mods_get();
    at         = 0;
    last       = (uint16_t)(now - ACCENT_STEP_MS); // the first step is due at the next call
    busy       = true;
}

void accent_task(uint16_t now)
{
    if (!busy || (uint16_t)(now - last) < ACCENT_STEP_MS) {
        return;
    }

    last = now;

    if (at == 0) {
        if (saved_mods) {
            accent_hook_mods_set(0);
        }
    } else if (at <= count) {
        accent_hook_key((uint8_t)events[at - 1u], (events[at - 1u] & DOWN) != 0);
    } else {
        if (saved_mods) {
            accent_hook_mods_set(saved_mods);
        }

        busy = false;
        return;
    }

    at++;
}

bool accent_busy(void)
{
    return busy;
}

#else

typedef int accent_left_out; // an empty file is not valid C

#endif
