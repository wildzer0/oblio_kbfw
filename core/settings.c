#include "settings.h"
#include "store.h"

#define MAGIC0  'O'
#define MAGIC1  'S'
#define VERSION 1u
#define SIZE    9u // magic 2, version 1, three fields, CRC 2... and one reserved byte

#define SLEEP_LEVELS 5 // see sleep.h: 1, 5, 10, 30 minutes, never

settings_t settings;

static bool     dirty;
static uint16_t last_change;

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
    settings.profile     = 0;
    settings.sleep_level = SETTINGS_SLEEP_DEFAULT;
    settings.bt_slot     = 1;
}

static void clamp(void)
{
    if (settings.profile >= STORE_PROFILES) {
        settings.profile = 0;
    }

    if (settings.sleep_level >= SLEEP_LEVELS) {
        settings.sleep_level = SETTINGS_SLEEP_DEFAULT;
    }

    if (settings.bt_slot < 1 || settings.bt_slot > 3) {
        settings.bt_slot = 1;
    }
}

void settings_load(void)
{
    uint8_t record[SIZE];

    dirty = false;
    defaults();

    store_hook_read(SETTINGS_ADDR, record, SIZE);

    if (record[0] != MAGIC0 || record[1] != MAGIC1 || record[2] != VERSION) {
        return;
    }

    if (crc16(record, 6) != (uint16_t)(record[6] | ((uint16_t)record[7] << 8))) {
        return;
    }

    settings.profile     = record[3];
    settings.sleep_level = record[4];
    settings.bt_slot     = record[5];
    clamp();
}

static void changed(void)
{
    dirty       = true;
    last_change = 0; // set by the next settings_task()
}

void settings_set_profile(uint8_t profile)
{
    if (profile < STORE_PROFILES && settings.profile != profile) {
        settings.profile = profile;
        changed();
    }
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
    record[3] = settings.profile;
    record[4] = settings.sleep_level;
    record[5] = settings.bt_slot;

    const uint16_t crc = crc16(record, 6);

    record[6] = (uint8_t)(crc & 0xFFu);
    record[7] = (uint8_t)(crc >> 8);
    record[8] = 0;

    store_hook_erase(SETTINGS_ADDR);
    store_hook_program(SETTINGS_ADDR, record, SIZE);
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
