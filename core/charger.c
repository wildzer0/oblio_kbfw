#include "charger.h"

// Plain variables and one macro, no structure passed by pointer: on this chip a pointer to a variable is the slow, generic kind and the module was
// several times the size it needed. The time of the last change is kept as its low byte: the longest wait is 200 ms.
static bool    cable_raw;
static bool    cable_stable;
static bool    charging_raw;
static bool    charging_stable;
static bool    primed;
static uint8_t cable_since;
static uint8_t charging_since;

// The firmware starts from zeroed memory (the startup clears XDATA): this is for the tests, which start over
#ifndef __SDCC
void charger_init(void)
{
    cable_raw = cable_stable = charging_raw = charging_stable = primed = false;
}
#endif

#define DEBOUNCE(raw, stable, since, level, now, on_ms, off_ms)                                       \
    do {                                                                                              \
        if ((level) != raw) {                                                                         \
            raw   = (level);                                                                          \
            since = (uint8_t)(now);                                                                   \
        } else if (raw != stable && (uint8_t)((uint8_t)(now) - since) >= (raw ? (on_ms) : (off_ms))) { \
            stable = raw;                                                                             \
        }                                                                                             \
    } while (0)

void charger_feed(bool cable_asserted, bool charging_asserted, uint16_t now_ms)
{
    if (!primed) { // the first look: the levels as they are, there is no earlier state to wait out
        primed = true;
        cable_raw = cable_stable = cable_asserted;
        charging_raw = charging_stable = charging_asserted;
        cable_since = charging_since = (uint8_t)now_ms;
        return;
    }

    DEBOUNCE(cable_raw, cable_stable, cable_since, cable_asserted, now_ms, CHARGER_CABLE_MS, CHARGER_CABLE_MS);
    DEBOUNCE(charging_raw, charging_stable, charging_since, charging_asserted, now_ms, CHARGER_CHARGING_ON_MS, CHARGER_CHARGING_OFF_MS);
}

bool charger_cable(void)
{
    return cable_stable;
}

bool charger_charging(void)
{
    return charging_stable;
}
