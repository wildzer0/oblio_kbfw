#include "os_switch.h"
#include "keymap.h"
#include "layer.h"
#include "sh68f90.h"

// P0.5: an input with a pull-up (P0CR = 0x04 leaves it an input, P0PCR = 0xF8 leaves the pull-up on).
// The polarity is the one the keyboard showed: with the switch on Mac the pin reads HIGH, on Windows LOW (the first reading of the two
// captures had them the other way round, and the hardware settled it). One line to change if a different board turns out opposite.
#define OS_SWITCH_PIN   0x20u
#define OS_SWITCH_IS_MAC 1

static bool     candidate; // what the pin reads now
static bool     settled;   // what the default layer is following
static uint16_t since;     // when the pin started to disagree with the state in use

static bool switch_mac(void)
{
    const bool high = (P0 & OS_SWITCH_PIN) != 0;

    return OS_SWITCH_IS_MAC ? high : !high;
}

void os_switch_apply(void)
{
    layer_set_default(settled ? KEYMAP_MAC_LAYER : 0);
}

void os_switch_init(void)
{
    // No debounce at power-on: the switch is where it is, and the first key must already go to the right layer.
    candidate = switch_mac();
    settled   = candidate;
    since     = 0;
    os_switch_apply();
}

void os_switch_task(uint16_t now)
{
    const bool mac = switch_mac();

    if (mac != candidate) {
        candidate = mac;
        since     = now;
        return;
    }

    if (candidate != settled && (uint16_t)(now - since) >= OS_SWITCH_STABLE_MS) {
        settled = candidate;
        os_switch_apply();
    }
}

bool os_switch_mac(void)
{
    return settled;
}
