#include "check.h"
#include "settings.h"
#include "store.h"
#include <string.h>

static uint8_t  flash[0x10000];
static unsigned erases;
static unsigned programs;
static unsigned violations;

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    memcpy(dst, &flash[addr], len);
}

void store_hook_erase(uint16_t sector)
{
    if (sector != SETTINGS_ADDR) {
        violations++; // the settings sector, and only it
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
}

static void test_blank_flash_gives_the_defaults(void)
{
    blank();
    settings_load();
    CHECK(settings.profile == 0 && settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
    CHECK(!settings_dirty());
}

static void test_changes_survive(void)
{
    blank();
    settings_load();
    settings_set_profile(3);
    settings_set_sleep_level(4);
    settings_set_bt_slot(2);
    CHECK(settings_dirty());
    settings_flush();
    CHECK(!settings_dirty() && erases == 1 && violations == 0);

    settings.profile = 0; // forget
    settings.sleep_level = 0;
    settings.bt_slot = 1;
    settings_load();
    CHECK(settings.profile == 3 && settings.sleep_level == 4 && settings.bt_slot == 2);
}

static void test_nothing_is_written_when_nothing_changed(void)
{
    blank();
    settings_load();
    settings_set_profile(0);
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
    settings_set_profile(STORE_PROFILES);
    settings_set_sleep_level(9);
    settings_set_bt_slot(0);
    settings_set_bt_slot(4);
    CHECK(!settings_dirty());
    CHECK(settings.profile == 0 && settings.bt_slot == 1);
}

static void test_a_damaged_record_is_not_used(void)
{
    blank();
    settings_load();
    settings_set_profile(2);
    settings_flush();

    flash[SETTINGS_ADDR + 3] &= 0x01; // the profile byte changes, the CRC does not
    settings_load();
    CHECK(settings.profile == 0 && settings.sleep_level == SETTINGS_SLEEP_DEFAULT);

    blank();
    flash[SETTINGS_ADDR] = 'X'; // not a record at all
    settings_load();
    CHECK(settings.profile == 0);
}

static void test_values_from_a_record_are_put_in_range(void)
{
    blank();
    settings_load();
    settings_set_profile(1);
    settings_flush();
    // a record whose CRC is right but whose values are not: written by hand through the same format
    uint8_t record[9] = {'O', 'S', 1, 7, 9, 5, 0, 0, 0};
    uint16_t crc = 0xFFFF;
    for (unsigned i = 0; i < 6; i++) {
        crc ^= (uint16_t)(record[i] << 8);
        for (unsigned b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    record[6] = (uint8_t)crc;
    record[7] = (uint8_t)(crc >> 8);
    memset(&flash[SETTINGS_ADDR], 0xFF, SETTINGS_SECTOR);
    memcpy(&flash[SETTINGS_ADDR], record, sizeof record);

    settings_load();
    CHECK(settings.profile == 0 && settings.sleep_level == SETTINGS_SLEEP_DEFAULT && settings.bt_slot == 1);
}

static void test_written_after_the_quiet_time_with_no_key_down(void)
{
    blank();
    settings_load();
    settings_task(1000, false);
    settings_set_profile(1);

    settings_task(1100, false); // the change is first seen here
    settings_task(1100 + SETTINGS_QUIET_MS - 1, false);
    CHECK(erases == 0);
    settings_task(1100 + SETTINGS_QUIET_MS, true); // a key is down: later
    CHECK(erases == 0);
    settings_task(1100 + SETTINGS_QUIET_MS + 5, false);
    CHECK(erases == 1 && !settings_dirty());
}

static void test_the_clock_wrap(void)
{
    blank();
    settings_load();
    settings_task(65300, false);
    settings_set_profile(1);
    settings_task(65400, false);
    settings_task(100, false); // 236 ms later
    CHECK(erases == 0);
    settings_task(500, false); // 636 ms
    CHECK(erases == 1);
}

int main(void)
{
    test_blank_flash_gives_the_defaults();
    test_changes_survive();
    test_nothing_is_written_when_nothing_changed();
    test_out_of_range_values_are_refused();
    test_a_damaged_record_is_not_used();
    test_values_from_a_record_are_put_in_range();
    test_written_after_the_quiet_time_with_no_key_down();
    test_the_clock_wrap();
    return tests_result();
}
