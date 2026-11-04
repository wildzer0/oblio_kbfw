#include "battery.h"
#include "charger.h"
#include "charger_hw.h"
#include "led.h"
#include "link.h"
#include "radio.h"
#include "sleep.h"
#include "usb_device.h"

// The battery the way the radio reports it: its status answer carries the cell voltage as a 10-bit ADC reading (core/radio.h). The scale was
// measured on the keyboard: 4.0378 V at rest read 981, which is about 243 counts to the volt, so 4.2 V (the charge voltage printed on the cell, a
// 5000 mAh Li-Po) is about 1020 and the ADC's full scale (1023) is just above a full cell - which is also why the reading saturates with the cable.
// PROVISIONAL: it rests on that one point; a second one (the reading just after taking the charger off) would confirm it.
//
// The percent is NOT the stock firmware's straight line (715 to 935, which calls everything above ~3.85 V full): it follows the discharge curve
// of a Li-Po cell, point by point, with the readings of 3.30 V (where the stock warns of a low battery, 800) up to 4.20 V.
//
// With the cable the reading saturates, so it says nothing about the charge: it is ignored, and the gauge shows the last reading it took on
// battery. With no reading at all (start-up, USB with no radio) it says "unknown" instead of inventing one.
#define BATTERY_RAW_FLOOR 600u  // below this the ADC has not settled yet (just after power-on the reading is 1 or 3) - not a cell
#define BATTERY_RAW_CABLE 1021u // at or above this the reading is the saturated one of the cable, not a cell voltage
#define CURVE_POINTS      9u

static const __code uint16_t curve_raw[CURVE_POINTS]     = {800, 850, 875, 899, 923, 948, 972, 996, 1020}; // 3.30 3.50 3.60 3.70 3.80 3.90 4.00 4.10 4.20 V
static const __code uint8_t  curve_percent[CURVE_POINTS] = {0, 10, 20, 35, 50, 65, 80, 90, 100};

static uint8_t last_percent = LED_BATTERY_UNKNOWN;

static uint8_t percent_of(uint16_t raw)
{
    if (raw <= curve_raw[0]) {
        return 0;
    }

    for (uint8_t i = 1; i < CURVE_POINTS; i++) {
        if (raw <= curve_raw[i]) {
            const uint16_t span = (uint16_t)(curve_raw[i] - curve_raw[i - 1]);
            const uint16_t into = (uint16_t)(raw - curve_raw[i - 1]);
            const uint8_t  rise = (uint8_t)(curve_percent[i] - curve_percent[i - 1]);

            return (uint8_t)(curve_percent[i - 1] + (uint8_t)((uint16_t)(into * rise) / span));
        }
    }

    return 100;
}

bool battery_cable(void)
{
    if (!link_wireless()) {
        return true; // the USB mode is the cable's
    }

    const uint16_t raw = radio_battery_raw();

    if (raw >= BATTERY_RAW_CABLE) {
        return true; // saturated: the supply is from outside, whether or not a PC is driving the bus (a charger alone, or a port the PC has suspended)
    }

    if (raw >= BATTERY_RAW_FLOOR) {
        return false; // the voltage of a cell: it is on its battery
    }

    return usb_device_configured() || charger_cable(); // no reading yet
}

void battery_show(void)
{
    if (battery_cable()) {
        led_show_battery(charger_charging() ? LED_BATTERY_CHARGING : LED_BATTERY_CHARGED);
    } else {
        led_show_battery(battery_percent());
    }
}

uint16_t battery_reading(void)
{
    const uint16_t raw = radio_battery_raw();

    return (raw >= BATTERY_RAW_FLOOR) ? raw : 0; // the cable's saturated reading is a reading too: the log marks it
}

uint8_t battery_percent(void)
{
    const uint16_t raw = radio_battery_raw();

    if (raw >= BATTERY_RAW_FLOOR && raw < BATTERY_RAW_CABLE && radio_hook_on_battery()) {
        last_percent = percent_of(raw);
    }

    return last_percent;
}

// ---- the warning ----
// The stock firmware warns below the reading 800 (3.30 V) and goes critical below 752 (3.09 V); so does this. Low: the gauge (red, one key) shows
// itself for 3 s, at once and then every 30 s. Critical for 10 s: the keyboard goes to sleep, so that it does not run the cell down to its protection
// circuit (a key wakes it; if the battery is still that low it sleeps again 10 s later - charge it).
#define BATTERY_RAW_WARN     800u
#define BATTERY_RAW_CRITICAL 752u
#define LOW_CHECK_MS         250u
#define LOW_WARN_CHECKS      120u // 30 s
#define LOW_CRITICAL_CHECKS  40u  // 10 s

void battery_task(uint16_t now_ms)
{
    static uint16_t checked_at;

    charger_hw_sample(now_ms); // the lines of the charger, debounced: every pass, they are two pin reads

    static uint8_t  warn_checks;
    static uint8_t  critical_checks;

    if ((uint16_t)(now_ms - checked_at) < LOW_CHECK_MS) {
        return;
    }

    checked_at = now_ms;

    const uint16_t raw = radio_battery_raw();

    if (raw < BATTERY_RAW_FLOOR || raw >= BATTERY_RAW_CABLE || !radio_hook_on_battery()) { // no reading, or the cable: nothing to say
        warn_checks     = 0;
        critical_checks = 0;
        return;
    }

    if (raw >= BATTERY_RAW_WARN) {
        warn_checks     = 0;
        critical_checks = 0;
        return;
    }

    if (warn_checks == 0) {
        led_show_battery(percent_of(raw));
    }

    warn_checks = (uint8_t)(warn_checks + 1u >= LOW_WARN_CHECKS ? 0u : warn_checks + 1u);

    if (raw >= BATTERY_RAW_CRITICAL) {
        critical_checks = 0;
    } else if (++critical_checks >= LOW_CRITICAL_CHECKS) {
        critical_checks = 0;
        sleep_now();
    }
}
