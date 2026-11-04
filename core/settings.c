#include "settings.h"
#include <string.h>
#include "store.h"

#define MAGIC0  'O'
#define MAGIC1  'S'
#define VERSION 2u
#define SIZE    14u // magic 2, version 1, one byte unused, sleep, slot, lighting effect, brightness, speed, colour (2), CRC 2, one byte unused
#define CRC_AT  11u // the CRC follows the 11 bytes it covers

#define WATCH_MS     8u // how often the lighting values are compared with the saved ones

#define SLEEP_LEVELS 5 // see sleep.h: 1, 5, 10, 30 minutes, never

settings_t settings;

static bool     dirty;
static uint16_t last_change;
static uint8_t  next_slot; // where the next save goes; SETTINGS_SLOTS = no room, the sector has to be erased first

// The lighting: effect, brightness, speed, colour - in the order of the record (the board maps them to the keymap's cfg fields)
static uint16_t led_seen[4]; // what was last saved (or loaded): a different cfg value is a change to save
static uint16_t led_saved[4];
static bool     led_have;    // the record read at start-up had a lighting

static uint16_t crc16(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFu;

    for (uint8_t i = 0; i < length; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);

        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

static void defaults(void)
{
    settings.sleep_level = SETTINGS_SLEEP_DEFAULT;
    settings.bt_slot     = 1;
}

static uint16_t slot_addr(uint8_t slot)
{
    return (uint16_t)(SETTINGS_ADDR + (uint16_t)slot * SIZE);
}

// True when nothing was ever written in this slot (an erased flash cell is 0xFF)
static bool slot_blank(const uint8_t *record)
{
    for (uint8_t i = 0; i < SIZE; i++) {
        if (record[i] != 0xFFu) {
            return false;
        }
    }

    return true;
}

// True when `record` is a complete record of this build
static bool record_valid(const uint8_t *record)
{
    return record[0] == MAGIC0 && record[1] == MAGIC1 && record[2] == VERSION
        && crc16(record, CRC_AT) == (uint16_t)(record[CRC_AT] | ((uint16_t)record[CRC_AT + 1] << 8));
}

void settings_load(void)
{
    uint8_t record[SIZE];

    dirty    = false;
    led_have = false;
    defaults();

    next_slot = SETTINGS_SLOTS; // the sector is full (or unreadable): the first save erases it

    for (uint8_t slot = 0; slot < SETTINGS_SLOTS; slot++) {
        store_hook_read(slot_addr(slot), record, SIZE);

        if (slot_blank(record)) {
            next_slot = slot;
            return;
        }

        // a record cut short by a power failure, or damaged: skipped, the one before it stays the newest
        if (!record_valid(record)) {
            continue;
        }

        // values out of range are put back in range, a record written by another build may hold anything. Byte 3 used to hold the
        // profile in use: it is ignored now, and written back as 0.
        settings.sleep_level = (record[4] < SLEEP_LEVELS) ? record[4] : SETTINGS_SLEEP_DEFAULT;
        settings.bt_slot     = (record[5] >= 1 && record[5] <= 3) ? record[5] : 1;
        led_saved[0]         = record[6];
        led_saved[1]         = record[7];
        led_saved[2]         = record[8];
        led_saved[3]         = (uint16_t)(record[9] | ((uint16_t)record[10] << 8));
        led_have             = true;
    }
}

void settings_apply_led(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        if (led_have) {
            settings_hook_led_set(i, led_saved[i]);
        }

        led_seen[i] = settings_hook_led_get(i);
    }
}

static void changed(void)
{
    dirty       = true;
    last_change = 0; // set by the next settings_task()
}

void settings_set_sleep_level(uint8_t level)
{
    if (level < SLEEP_LEVELS && settings.sleep_level != level) {
        settings.sleep_level = level;
        changed();
    }
}

void settings_set_bt_slot(uint8_t slot)
{
    if (slot >= 1 && slot <= 3 && settings.bt_slot != slot) {
        settings.bt_slot = slot;
        changed();
    }
}

bool settings_dirty(void)
{
    return dirty;
}

void settings_flush(void)
{
    uint8_t record[SIZE];

    if (!dirty) {
        return;
    }

    dirty = false;

    memset(record, 0, sizeof record);
    record[0] = MAGIC0;
    record[1] = MAGIC1;
    record[2] = VERSION;
    record[4] = settings.sleep_level;
    record[5] = settings.bt_slot;

    for (uint8_t i = 0; i < 4; i++) {
        led_seen[i] = settings_hook_led_get(i);
    }

    record[6]  = (uint8_t)led_seen[0];
    record[7]  = (uint8_t)led_seen[1];
    record[8]  = (uint8_t)led_seen[2];
    record[9]  = (uint8_t)(led_seen[3] & 0xFFu);
    record[10] = (uint8_t)(led_seen[3] >> 8);

    const uint16_t crc = crc16(record, CRC_AT);

    record[CRC_AT]     = (uint8_t)(crc & 0xFFu);
    record[CRC_AT + 1] = (uint8_t)(crc >> 8);

    if (next_slot >= SETTINGS_SLOTS) { // the log is full: start it over, and only now is the old content given up
        store_hook_flush_begin(); // only the erase stalls the chip for long (~32 ms): the backlight stands still for that and for nothing else
        store_hook_erase(SETTINGS_ADDR);
        store_hook_flush_end();
        next_slot = 0;
    }

    store_hook_program(slot_addr(next_slot), record, SIZE); // a byte at a time, microseconds each: the display runs on

    next_slot++;
}

void settings_task(uint16_t now, bool any_key_down)
{
    static bool     timing;
    static uint16_t watched_at;

    // The lighting keys and the Vial lighting tab change the cfg: that is a setting to save. Looked at every WATCH_MS, not every pass of the main loop
    // (eight calls of the hooks, a sixth of what a pass costs): the quiet time that follows is half a second, so a few milliseconds more do not matter.
    if ((uint16_t)(now - watched_at) >= WATCH_MS) {
        watched_at = now;

        for (uint8_t i = 0; i < 4; i++) {
            if (settings_hook_led_get(i) != led_seen[i]) {
                led_seen[i] = settings_hook_led_get(i);
                changed();
                break;
            }
        }
    }

    if (!dirty) {
        timing = false;
        return;
    }

    if (!timing) { // the quiet time starts when the change is first seen
        timing      = true;
        last_change = now;
        return;
    }

    if (!any_key_down && (uint16_t)(now - last_change) >= SETTINGS_QUIET_MS) {
        timing = false;
        settings_flush();
    }
}
