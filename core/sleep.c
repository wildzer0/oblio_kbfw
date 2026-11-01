#include "sleep.h"
#include "platform.h"

static const ROM uint16_t seconds_by_level[SLEEP_LEVELS] = {60, 300, 600, 1800, 0};

static uint16_t timeout_seconds = 600;
static uint16_t idle_seconds;     // seconds without use so far
static uint16_t millis;           // milliseconds counted towards the next second
static uint16_t last;             // the clock at the last look
static bool     activity;
static bool     requested;

uint16_t sleep_level_seconds(uint8_t level)
{
    return seconds_by_level[level < SLEEP_LEVELS ? level : SLEEP_LEVEL_DEFAULT];
}

static void restart(void)
{
    idle_seconds = 0;
    millis       = 0;
    activity     = false;
}

void sleep_init(uint16_t now)
{
    timeout_seconds = sleep_level_seconds(SLEEP_LEVEL_DEFAULT);
    last            = now;
    requested       = false;
    restart();
}

void sleep_set_level(uint8_t level)
{
    timeout_seconds = sleep_level_seconds(level);
    activity        = true; // the count restarts: a shorter time never finds it already past
}

void sleep_note_activity(void)
{
    activity = true;
}

void sleep_now(void)
{
    requested = true;
}

bool sleep_due(uint16_t now, bool allowed)
{
    const uint16_t elapsed = (uint16_t)(now - last);

    last = now;

    if (!allowed) {
        requested = false;
        restart();
        return false;
    }

    if (activity) {
        restart();
        requested = false;
        return false;
    }

    if (requested) {
        requested = false;
        return true;
    }

    if (timeout_seconds == 0) {
        return false;
    }

    millis = (uint16_t)(millis + elapsed);

    while (millis >= 1000u) {
        millis = (uint16_t)(millis - 1000u);

        if (idle_seconds < 0xFFFFu) {
            idle_seconds++;
        }
    }

    return idle_seconds >= timeout_seconds;
}

void sleep_woke(uint16_t now)
{
    last      = now;
    requested = false;
    restart();
}
