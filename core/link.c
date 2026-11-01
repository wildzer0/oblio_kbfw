#include "link.h"
#include "radio.h"
#include "settings.h"

static link_mode_t mode = LINK_NONE;
static bool        leaving;   // the selector went to USB: the radio first lets go of the keys it holds
static link_mode_t candidate = LINK_NONE;
static uint16_t    since;

static uint8_t  key_slot; // the Bluetooth key that is down, 0 = none
static uint16_t key_down_at;
static bool     key_fired;

void link_init(void)
{
    radio_init();
    mode      = LINK_NONE;
    candidate = LINK_NONE;
    leaving   = false;
    key_slot  = 0;
    key_fired = false;
}

link_mode_t link_mode(void)
{
    return mode;
}

bool link_wireless(void)
{
    return mode == LINK_2G4 || mode == LINK_BT;
}

static uint8_t bt_slot(void)
{
    return (settings.bt_slot >= 1 && settings.bt_slot <= 3) ? settings.bt_slot : 1;
}

static void apply(link_mode_t selected)
{
    static const uint8_t no_keys[6] = {0, 0, 0, 0, 0, 0};

    mode    = selected;
    leaving = false;

    switch (selected) {
        case LINK_2G4:
            radio_set_link(RADIO_MODE_2G4, false);
            link_hook_flash(0, 0);
            break;

        case LINK_BT:
            radio_set_link(bt_slot(), false);
            link_hook_flash(1, bt_slot());
            break;

        default: // USB: whatever the radio holds down is released, then it is left alone
            if (radio_active()) {
                radio_send_keys(0, no_keys);
                radio_send_consumer(0);
                leaving = true;
            }
            break;
    }
}

void link_wake(void)
{
    radio_init();
    mode      = LINK_NONE;
    candidate = LINK_NONE;
    leaving   = false;
}

void link_key(uint8_t slot, bool pressed, uint16_t now)
{
    if (pressed) {
        key_slot    = slot;
        key_down_at = now;
        key_fired   = false;
        return;
    }

    if (key_slot != slot) {
        return;
    }

    if (!key_fired) { // a tap: this slot is the one to use
        settings_set_bt_slot(slot);

        if (mode == LINK_BT) {
            radio_set_link(slot, false);
            link_hook_flash(1, slot);
        }
    }

    key_slot = 0;
}

void link_task(uint16_t now)
{
    const link_mode_t raw = link_hook_selector();

    if (raw != candidate) {
        candidate = raw;
        since     = now;
    } else if (raw != mode && (uint16_t)(now - since) >= LINK_SELECTOR_STABLE_MS) {
        apply(raw);
    }

    if (key_slot && !key_fired && (uint16_t)(now - key_down_at) >= LINK_PAIRING_HOLD_MS) {
        key_fired = true;
        settings_set_bt_slot(key_slot);

        if (mode == LINK_BT) {
            radio_set_link(key_slot, true);
            link_hook_flash(2, key_slot);
        }
    }

    radio_task();

    if (leaving && radio_idle()) {
        leaving = false;
        radio_stop();
    }
}
