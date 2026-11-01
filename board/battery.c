#include "battery.h"
#include "led.h"
#include "radio.h"

// The battery the way the radio reports it: seven levels, in the low three bits of one byte of its status answer (core/radio.h has the
// layout and why those are the bytes). Seven levels of a lithium cell are coarse, and the radio does not say what they mean in volts: the
// steps below are even, which is the honest thing to do with what there is - and the "critically low" flag the radio also sends turns the
// gauge red whatever the level says.
//
// While the cable charges the battery the radio answers 0xFF in that byte, which is a sentinel and not a level: the gauge then shows the last
// level it saw on battery, so it does not claim a full battery that is not there. With no reading at all (start-up, USB with no radio) it says
// "unknown" instead of inventing one.
static uint8_t last_level = RADIO_BATTERY_NONE;

uint8_t battery_percent(void)
{
    const uint8_t level = radio_battery_level();

    if (level != RADIO_BATTERY_NONE) {
        last_level = level;
    }

    if (last_level == RADIO_BATTERY_NONE) {
        return LED_BATTERY_UNKNOWN;
    }

    const uint8_t percent = (uint8_t)((uint16_t)last_level * 100u / 7u);

    return radio_low_power() && percent > LED_BATTERY_LOW ? LED_BATTERY_LOW : percent;
}
