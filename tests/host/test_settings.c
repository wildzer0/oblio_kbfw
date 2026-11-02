#include "check.h"
#include "settings.h"
#include "store.h"
#include <string.h>

// the lighting values the board keeps in the keymap's cfg
static uint16_t led_values[4];
uint16_t settings_hook_led_get(uint8_t index) { return led_values[index]; }
void settings_hook_led_set(uint8_t index, uint16_t value) { led_values[index] = value; }

static uint8_t  flash[0x10000];
static unsigned erases;
static unsigned programs;
static unsigned violations;

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    memcpy(dst, &flash[addr], len);
}

static bool flushing;

void store_hook_flush_begin(void) { flushing = true; }
void store_hook_flush_end(void) { flushing = false; }

void store_hook_erase(uint16_t sector)
{
    if (sector != SETTINGS_ADDR || !flushing) {
        violations++; // the settings sector, and only it, inside a save
        return;
    }
    erases++;
    memset(&flash[sector], 0xFF, SETTINGS_SECTOR);
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    if (addr < SETTINGS_ADDR || addr + len > SETTINGS_ADDR + SETTINGS_SECTOR) {
        violations++;
        return;
    }
    programs++;
    for (uint8_t i = 0; i < len; i++) {
        if (src[i] & ~flash[addr + i]) {
            violations++;
        }
        flash[addr + i] &= src[i];
    }
}

void store_hook_loaded(void) {}

static void blank(void)
{
    memset(flash, 0xFF, sizeof flash);
    erases     = 0;
    programs   = 0;
    violations = 0;
    flushing   = false;
}

static void test_blank_flash_gives_the_defaults(void)
{
    blank();
    settings_load();
    CHECK(settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
    CHECK(!settings_dirty());
}

static void test_changes_survive(void)
{
    blank();
    settings_load();
    settings_set_sleep_level(4);
    settings_set_bt_slot(2);
    CHECK(settings_dirty());
    settings_flush();
    CHECK(!settings_dirty() && erases == 0 && violations == 0); // appended in the erased sector: no erase

    settings.sleep_level = 0; // forget
    settings.bt_slot = 1;
    settings_load();
    CHECK(settings.sleep_level == 4 && settings.bt_slot == 2);
}

static void test_nothing_is_written_when_nothing_changed(void)
{
    blank();
    settings_load();
    settings_set_sleep_level(SETTINGS_SLEEP_DEFAULT);
    settings_set_bt_slot(1);
    CHECK(!settings_dirty());
    settings_flush();
    CHECK(erases == 0 && programs == 0);
}

static void test_out_of_range_values_are_refused(void)
{
    blank();
    settings_load();
    settings_set_sleep_level(9);
    settings_set_bt_slot(0);
    settings_set_bt_slot(4);
    CHECK(!settings_dirty());
    CHECK(settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
}

// The record of a save, at slot `slot`
static uint16_t record_at(uint8_t slot)
{
    return (uint16_t)(SETTINGS_ADDR + slot * 14u);
}

// A damaged record is skipped and the one before it stays the newest: the settings are never lost by a bad save
static void test_a_damaged_record_falls_back_to_the_one_before_it(void)
{
    blank();
    settings_load();
    settings_set_bt_slot(2);
    settings_flush();

    settings_set_bt_slot(3);
    settings_flush();

    flash[record_at(1) + 5] &= 0x01; // the last record changes, its CRC does not
    settings_load();
    CHECK(settings.bt_slot == 2); // the previous one

    blank();
    flash[SETTINGS_ADDR] = 'X'; // not a record at all, and nothing else
    settings_load();
    CHECK(settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
}

// A save appends: nothing is erased until the sector is full, and the newest record is the one read back
static void test_records_are_appended_and_the_sector_is_erased_only_when_full(void)
{
    blank();
    settings_load();

    settings_set_bt_slot(2);
    settings_flush();
    CHECK(flash[record_at(0)] == 'O' && flash[record_at(1)] == 0xFF && erases == 0);

    settings_set_sleep_level(4);
    settings_flush();
    CHECK(flash[record_at(1)] == 'O' && flash[record_at(0) + 5] == 2); // the previous record is still there
    CHECK(erases == 0);

    settings.bt_slot = 1;
    settings_load();
    CHECK(settings.bt_slot == 2 && settings.sleep_level == 4);
    CHECK(violations == 0);

    // fill the sector: the save that finds it full erases it once
    for (uint8_t i = 0; i < SETTINGS_SLOTS - 2; i++) {
        settings_set_sleep_level((uint8_t)(i % 5)); // the five levels
        settings_flush();
    }
    CHECK(erases == 0);

    settings_set_sleep_level(1);
    settings_flush();
    CHECK(erases == 1 && flash[record_at(0)] == 'O');

    settings.sleep_level = 0;
    settings_load();
    CHECK(settings.sleep_level == 1);
}

static void test_values_from_a_record_are_put_in_range(void)
{
    blank();
    settings_load();
    settings_set_sleep_level(2);
    settings_flush();
    // a record whose CRC is right but whose values are not: written by hand through the same format (byte 3 is the old profile field)
    uint8_t record[14] = {'O', 'S', 2, 7, 9, 5, 1, 2, 3, 4, 0, 0, 0, 0};
    uint16_t crc = 0xFFFF;
    for (unsigned i = 0; i < 11; i++) {
        crc ^= (uint16_t)(record[i] << 8);
        for (unsigned b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    record[11] = (uint8_t)crc;
    record[12] = (uint8_t)(crc >> 8);
    memset(&flash[SETTINGS_ADDR], 0xFF, SETTINGS_SECTOR);
    memcpy(&flash[SETTINGS_ADDR], record, sizeof record);

    settings_load();
    CHECK(settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
}

static void test_written_after_the_quiet_time_with_no_key_down(void)
{
    blank();
    settings_load();
    settings_task(1000, false);
    settings_set_bt_slot(2);

    settings_task(1100, false); // the change is first seen here
    settings_task(1100 + SETTINGS_QUIET_MS - 1, false);
    CHECK(programs == 0);
    settings_task(1100 + SETTINGS_QUIET_MS, true); // a key is down: later
    CHECK(programs == 0);
    settings_task(1100 + SETTINGS_QUIET_MS + 5, false);
    CHECK(programs == 1 && !settings_dirty());
}

// The lighting is saved here, with no erase, and put back after the keymap is loaded
static void test_the_lighting_is_saved_with_the_settings(void)
{
    blank();
    settings_load();
    settings_apply_led(); // nothing saved: the values the board has stay
    settings_hook_led_set(0, 3);
    settings_hook_led_set(1, 120);
    settings_hook_led_set(2, 5);
    settings_hook_led_set(3, 0x80A0);

    settings_task(1000, false); // seen
    CHECK(settings_dirty());
    settings_task(1000 + SETTINGS_QUIET_MS, false);
    CHECK(!settings_dirty() && programs == 1 && erases == 0 && violations == 0);

    for (uint8_t i = 0; i < 4; i++) { // a restart: the board's own values come first, then the saved ones are put back over them
        settings_hook_led_set(i, 0);
    }

    settings_load();
    settings_apply_led();
    CHECK(settings_hook_led_get(0) == 3 && settings_hook_led_get(1) == 120 && settings_hook_led_get(2) == 5 && settings_hook_led_get(3) == 0x80A0);

    settings_task(5000, false); // and putting them back is not a change
    CHECK(!settings_dirty());
}

static void test_the_clock_wrap(void)
{
    blank();
    settings_load();
    settings_task(65300, false);
    settings_set_bt_slot(2);
    settings_task(65400, false);
    settings_task(100, false); // 236 ms later
    CHECK(programs == 0);
    settings_task(500, false); // 636 ms
    CHECK(programs == 1);
}

int main(void)
{
    test_blank_flash_gives_the_defaults();
    test_changes_survive();
    test_nothing_is_written_when_nothing_changed();
    test_out_of_range_values_are_refused();
    test_a_damaged_record_falls_back_to_the_one_before_it();
    test_records_are_appended_and_the_sector_is_erased_only_when_full();
    test_values_from_a_record_are_put_in_range();
    test_written_after_the_quiet_time_with_no_key_down();
    test_the_lighting_is_saved_with_the_settings();
    test_the_clock_wrap();
    return tests_result();
}
