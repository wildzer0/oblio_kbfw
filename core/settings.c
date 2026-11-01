#include "settings.h"
#include "store.h"

#define MAGIC0  'O'
#define MAGIC1  'S'
#define VERSION 1u
#define SIZE    9u // magic 2, version 1, three fields, CRC 2, one byte unused (0)

#define SLEEP_LEVELS 5 // see sleep.h: 1, 5, 10, 30 minutes, never

settings_t settings;

static bool     dirty;
static uint16_t last_change;
static uint8_t  next_slot; // where the next save goes; SETTINGS_SLOTS = no room, the sector has to be erased first

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
        && crc16(record, 6) == (uint16_t)(record[6] | ((uint16_t)record[7] << 8));
}

void settings_load(void)
{
    uint8_t record[SIZE];

    dirty = false;
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

    record[0] = MAGIC0;
    record[1] = MAGIC1;
    record[2] = VERSION;
    record[3] = 0;
    record[4] = settings.sleep_level;
    record[5] = settings.bt_slot;

    const uint16_t crc = crc16(record, 6);

    record[6] = (uint8_t)(crc & 0xFFu);
    record[7] = (uint8_t)(crc >> 8);
    record[8] = 0;

    store_hook_flush_begin();

    if (next_slot >= SETTINGS_SLOTS) { // the log is full: start it over, and only now is the old content given up
        store_hook_erase(SETTINGS_ADDR);
        next_slot = 0;
    }

    store_hook_program(slot_addr(next_slot), record, SIZE);
    store_hook_flush_end();

    next_slot++;
}

void settings_task(uint16_t now, bool any_key_down)
{
    static bool timing;

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
