#include "macro.h"
#include "keycodes.h"
#include "keymap.h"
#include "platform.h"

// The action codes after the 0x01 prefix
#define MACRO_PREFIX 0x01
#define ACTION_TAP   0x01
#define ACTION_DOWN  0x02
#define ACTION_UP    0x03
#define ACTION_DELAY 0x04
#define EXT_TAP      0x05 // the same three with a 16-bit keycode
#define EXT_DOWN     0x06
#define EXT_UP       0x07

#define SHIFT 0x80u // in the character table: the character needs shift

typedef struct {
    char    character;
    uint8_t usage; // KC_* of the key, SHIFT set when shift is needed (US layout)
} char_entry_t;

// Everything printable that is not a letter or a digit
static const ROM char_entry_t punctuation[] = {
    {' ', KC_SPC},
    {'!', SHIFT | KC_1}, {'@', SHIFT | KC_2}, {'#', SHIFT | KC_3}, {'$', SHIFT | KC_4}, {'%', SHIFT | KC_5},
    {'^', SHIFT | KC_6}, {'&', SHIFT | KC_7}, {'*', SHIFT | KC_8}, {'(', SHIFT | KC_9}, {')', SHIFT | KC_0},
    {'-', KC_MINS}, {'_', SHIFT | KC_MINS}, {'=', KC_EQL}, {'+', SHIFT | KC_EQL},
    {'[', KC_LBRC}, {'{', SHIFT | KC_LBRC}, {']', KC_RBRC}, {'}', SHIFT | KC_RBRC},
    {'\\', KC_BSLS}, {'|', SHIFT | KC_BSLS}, {';', KC_SCLN}, {':', SHIFT | KC_SCLN},
    {'\'', KC_QUOT}, {'"', SHIFT | KC_QUOT}, {'`', KC_GRV}, {'~', SHIFT | KC_GRV},
    {',', KC_COMM}, {'<', SHIFT | KC_COMM}, {'.', KC_DOT}, {'>', SHIFT | KC_DOT}, {'/', KC_SLSH}, {'?', SHIFT | KC_SLSH},
    {'\b', KC_BSPC}, {'\t', KC_TAB}, {'\n', KC_ENT}, {0x1B, KC_ESC}, {0x7F, KC_DEL},
};

typedef struct {
    uint16_t keycode;
    bool     pressed;
} step_t;

#define STEPS_MAX 4 // a shifted character: shift down, key down, key up, shift up

static uint8_t queue[MACRO_QUEUE]; // ids waiting, queue[0] is the next
static uint8_t queued;

static bool     playing;
static uint16_t position;     // where the next byte is read from
static step_t   steps[STEPS_MAX];
static uint8_t  step_count;   // steps decoded and not done yet
static uint8_t  step_next;
static bool     waiting;      // a delay is running
static uint16_t wait_since;
static uint16_t wait_ms;
static uint16_t last_now;
static bool     have_last;

void macro_init(void)
{
    queued     = 0;
    playing    = false;
    step_count = 0;
    step_next  = 0;
    waiting    = false;
    have_last  = false;
}

bool macro_busy(void)
{
    return playing || queued > 0;
}

void macro_start(uint8_t id)
{
    if (id >= KEYMAP_MACRO_COUNT || queued >= MACRO_QUEUE) {
        return;
    }

    queue[queued++] = id;
}

static uint8_t read_byte(void)
{
    return keymap_macro_byte(position++);
}

static void add_step(uint16_t keycode, bool pressed)
{
    steps[step_count].keycode = keycode;
    steps[step_count].pressed = pressed;
    step_count++;
}

static void add_tap(uint16_t keycode)
{
    add_step(keycode, true);
    add_step(keycode, false);
}

static void add_action(uint8_t action, uint16_t keycode)
{
    if (action == ACTION_TAP || action == EXT_TAP) {
        add_tap(keycode);
    } else {
        add_step(keycode, action == ACTION_DOWN || action == EXT_DOWN);
    }
}

static void add_character(uint8_t c)
{
    uint8_t usage = 0;

    if (c >= 'a' && c <= 'z') {
        usage = (uint8_t)(KC_A + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
        usage = (uint8_t)(SHIFT | (KC_A + (c - 'A')));
    } else if (c >= '1' && c <= '9') {
        usage = (uint8_t)(KC_1 + (c - '1'));
    } else if (c == '0') {
        usage = KC_0;
    } else {
        for (uint8_t i = 0; i < sizeof punctuation / sizeof punctuation[0]; i++) {
            if (punctuation[i].character == (char)c) {
                usage = punctuation[i].usage;
                break;
            }
        }
    }

    if (!usage) {
        return; // not a character this layout has
    }

    if (usage & SHIFT) {
        add_step(KC_LSFT, true);
    }

    add_tap(usage & 0x7F);

    if (usage & SHIFT) {
        add_step(KC_LSFT, false);
    }
}

// A 16-bit keycode cannot hold a 0 byte (it would end the macro), so Vial stores 0x0100 as 0xFF01, 0x0200 as 0xFF02, ...
static uint16_t decode_keycode(uint16_t keycode)
{
    return (keycode > 0xFF00u) ? (uint16_t)((keycode & 0xFFu) << 8) : keycode;
}

// Moves to the start of macro `id`. False when the buffer has no such macro or is not a valid buffer.
static bool locate(uint8_t id)
{
    // a buffer whose last byte is not 0 is one being rewritten, or damaged: play nothing
    if (keymap_macro_byte(KEYMAP_MACRO_BYTES - 1) != 0) {
        return false;
    }

    position = 0;

    while (id > 0) {
        if (position >= KEYMAP_MACRO_BYTES) {
            return false;
        }

        if (read_byte() == 0) {
            id--;
        }
    }

    return true;
}

static void finish(void)
{
    playing = false;
}

// Decodes what comes next into steps, or into a delay, or ends the macro.
static void decode_next(uint16_t now)
{
    const uint8_t c = read_byte();

    if (c == 0 || position > KEYMAP_MACRO_BYTES) {
        finish();
        return;
    }

    if (c != MACRO_PREFIX) {
        add_character(c);
        return;
    }

    const uint8_t action = read_byte();

    if (action == ACTION_TAP || action == ACTION_DOWN || action == ACTION_UP) {
        const uint8_t key = read_byte();

        if (key) {
            add_action(action, key);
        }
    } else if (action == EXT_TAP || action == EXT_DOWN || action == EXT_UP) {
        const uint8_t low  = read_byte();
        const uint8_t high = low ? read_byte() : 0;

        if (low && high) {
            add_action(action, decode_keycode((uint16_t)(low | ((uint16_t)high << 8))));
        }
    } else if (action == ACTION_DELAY) {
        const uint8_t d0 = read_byte();
        const uint8_t d1 = d0 ? read_byte() : 0;

        if (d0 == 0 || d1 == 0) {
            finish();
            return;
        }

        // 0 cannot be stored, so both bytes are kept one higher
        wait_ms    = (uint16_t)((d0 - 1) + (d1 - 1) * 255);
        wait_since = now;
        waiting    = true;
    } else {
        finish(); // 0: the macro ended mid-action; anything else: not an action we know
    }
}

void macro_task(uint16_t now)
{
    if (have_last && now == last_now) {
        return; // one step per millisecond
    }

    have_last = true;
    last_now  = now;

    if (step_next < step_count) {
        const step_t step = steps[step_next++];

        if (step.pressed) {
            macro_hook_press(step.keycode);
        } else {
            macro_hook_release(step.keycode);
        }
        return;
    }

    step_count = 0;
    step_next  = 0;

    if (waiting) {
        if ((uint16_t)(now - wait_since) < wait_ms) {
            return;
        }
        waiting = false;
    }

    if (!playing) {
        if (queued == 0) {
            return;
        }

        const uint8_t id = queue[0];

        for (uint8_t i = 1; i < queued; i++) {
            queue[i - 1] = queue[i];
        }
        queued--;

        playing = locate(id);
        return;
    }

    decode_next(now);
}
