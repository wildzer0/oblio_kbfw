#include "battery.h"
#include "keyboard.h"
#include "keycodes.h"
#include "led.h"
#include "radio.h"
#include "sleep.h"

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

uint8_t battery_percent(void)
{
    const uint16_t raw = radio_battery_raw();

    if (raw >= BATTERY_RAW_FLOOR && raw < BATTERY_RAW_CABLE && radio_hook_on_battery()) {
        last_percent = percent_of(raw);
    }

    return last_percent;
}

// ---- the live readout: the keyboard types its battery reading ----
// The only road the keyboard has to the PC without the cable is the keyboard report, so for measuring it types: once a second, the raw
// reading of the radio's ADC as decimal digits and Enter, into whatever has the focus (tools/battery-log.html listens for it and draws
// the curve). Switched on and off by pressing the battery key (Fn+Backspace) twice within BATLOG_DOUBLE_MS. The characters go out one every
// BATLOG_CHAR_MS: the radio's queue is four deep and a burst would lose keys. The keyboard is kept awake while it types.
#define BATLOG_DOUBLE_MS 700u
#define BATLOG_LINE_MS   1000u
#define BATLOG_CHAR_MS   30u

static bool     typing;
static bool     pressed_before;
static uint16_t pressed_at;
static uint16_t line_at;
static uint16_t char_at;
static uint8_t  line[6];
static uint8_t  line_len;
static uint8_t  line_pos;

bool battery_key(uint16_t now_ms)
{
    const bool toggled = pressed_before && (uint16_t)(now_ms - pressed_at) < BATLOG_DOUBLE_MS;

    pressed_before = !toggled;
    pressed_at     = now_ms;

    if (toggled) {
        typing   = !typing;
        line_len = 0;
        line_pos = 0;
        line_at  = (uint16_t)(now_ms - BATLOG_LINE_MS);
    }

    return toggled;
}

void battery_log_task(uint16_t now_ms)
{
    if (!typing) {
        return;
    }

    if (line_pos >= line_len) { // the last line is out: the next one is due a second after the start of this one
        if ((uint16_t)(now_ms - line_at) < BATLOG_LINE_MS) {
            return;
        }

        uint16_t raw = radio_battery_raw();
        uint8_t  n   = 0;
        uint8_t  digits[4];

        do {
            digits[n++] = (uint8_t)(raw % 10u);
            raw         = (uint16_t)(raw / 10u);
        } while (raw != 0 && n < 4);

        line_len = 0;
        while (n > 0) {
            const uint8_t d = digits[--n];

            line[line_len++] = (uint8_t)(d == 0 ? KC_0 : KC_1 + d - 1u);
        }
        line[line_len++] = KC_ENT;
        line_pos         = 0;
        line_at          = now_ms;
        sleep_note_activity();
    }

    if ((uint16_t)(now_ms - char_at) >= BATLOG_CHAR_MS) {
        char_at = now_ms;
        keyboard_tap(line[line_pos++]);
    }
}
