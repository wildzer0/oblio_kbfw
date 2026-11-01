#include "leader.h"
#include "keycodes.h"

static uint16_t keys[LEADER_KEYS];
static uint8_t  count;
static bool     active;
static uint16_t since;

void leader_init(void)
{
    count  = 0;
    active = false;
    since  = 0;
}

bool leader_active(void)
{
    return active;
}

bool leader_process(uint16_t keycode, bool pressed, uint16_t now)
{
    if (leader_hook_is_key(keycode)) { // the key that arms it, and the one that disarms it
        if (pressed) {
            active = !active;
            count  = 0;
            since  = now;
        }

        return true;
    }

    if (!active) {
        return false;
    }

    if (!pressed) {
        return true; // the releases of the keys it swallowed follow them
    }

    if (count < LEADER_KEYS) {
        keys[count++] = keycode;
    }

    since = now;

    uint16_t action = 0;

    switch (leader_hook_match(keys, count, &action)) {
        case LEADER_MATCH:
            active = false;
            count  = 0;
            leader_hook_send(action);
            return true;

        case LEADER_PREFIX:
            return true; // still could be something: wait

        default:
            active = false; // nothing starts like this: the leader gives up, and nothing is typed
            count  = 0;
            return true;
    }
}

void leader_task(uint16_t now)
{
    if (active && (uint16_t)(now - since) >= LEADER_TIMEOUT_MS) {
        active = false;
        count  = 0;
    }
}
