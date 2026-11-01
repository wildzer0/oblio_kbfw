#include "check.h"
#include "keycodes.h"
#include "keymap.h"
#include "store.h"
#include <string.h>

// The compiled-in profile of the test: a pattern, so that a mixed-up word is noticed.
static uint16_t salt;

uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    return (uint16_t)(KC_A + ((layer + row + col + salt) & 7));
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)encoder;
    return (uint16_t)(KC_F1 + layer + (clockwise ? 1 : 0));
}

uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field)
{
    return (uint16_t)(index * 10 + field);
}

uint16_t keymap_default_cfg(uint8_t field)
{
    return (uint16_t)(100 + field);
}

// ---- a flash: erased cells are 0xFF, programming can only clear bits, a power cut stops it half way ----
static uint8_t  flash[0x10000];
static long     budget = -1; // operations left before the power goes; -1 = no cut
static unsigned violations;  // writes the real flash would not do right
static unsigned erases;

static bool in_store(uint16_t addr, unsigned len)
{
    return addr >= STORE_BASE && (unsigned)addr + len <= STORE_END;
}

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    if (!in_store(addr, len)) {
        violations++;
        return;
    }
    memcpy(dst, &flash[addr], len);
}

void store_hook_erase(uint16_t sector_addr)
{
    if (!in_store(sector_addr, STORE_SECTOR_BYTES) || (sector_addr & (STORE_SECTOR_BYTES - 1))) {
        violations++;
        return;
    }

    erases++;

    if (budget == 0) {
        return;
    }

    // the power goes in the middle of the erase: half of the sector is done
    const unsigned size = (budget == 1) ? STORE_SECTOR_BYTES / 2 : STORE_SECTOR_BYTES;

    memset(&flash[sector_addr], 0xFF, size);

    if (budget > 0) {
        budget--;
    }
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    if (!in_store(addr, len)) {
        violations++;
        return;
    }

    for (uint8_t i = 0; i < len; i++) {
        if (budget == 0) {
            return;
        }

        if (src[i] & ~flash[addr + i]) {
            violations++; // a 0 cell asked to become 1: it was not erased
        }

        flash[addr + i] &= src[i];

        if (budget > 0) {
            budget--;
        }
    }
}

static unsigned loaded;

void store_hook_loaded(void)
{
    loaded++;
}

static void blank_flash(void)
{
    memset(flash, 0xFF, sizeof flash);
    budget     = -1;
    violations = 0;
    erases     = 0;
    salt       = 0;
}

// Power on: the RAM copy starts from nothing, then the store loads what the flash has
static void boot(void)
{
    keymap_init();
    store_init(0);
}

static void test_blank_flash_gives_the_defaults(void)
{
    blank_flash();
    boot();
    CHECK(keymap_get(0, 0, 0) == KC_A);
    CHECK(keymap_get(7, 4, 15) == KC_A + ((7 + 4 + 15) & 7));
    CHECK(keymap_tap_dance_get(2, 3) == 23);
    CHECK(keymap_cfg_get(CFG_LED_BRIGHT) == 101);
    CHECK(!store_dirty());
    CHECK(store_profile() == 0);
    CHECK(violations == 0);
}

static void edit_everything(uint16_t value)
{
    const uint8_t macro[] = {'h', 'e', 'l', 'l', 'o', 0};

    keymap_set(0, 0, 0, value);
    keymap_set(7, 4, 15, (uint16_t)(value + 1));
    keymap_set_encoder(3, 0, true, (uint16_t)(value + 2));
    keymap_tap_dance_set(7, 4, (uint16_t)(value + 3));
    keymap_cfg_set(CFG_LED_SPEED, (uint16_t)(value + 4));
    keymap_macro_write(0, sizeof macro, macro);
    keymap_macro_write(KEYMAP_MACRO_BYTES - 2, 1, (const uint8_t *)&value);
}

static void check_everything(uint16_t value)
{
    CHECK(keymap_get(0, 0, 0) == value);
    CHECK(keymap_get(7, 4, 15) == (uint16_t)(value + 1));
    CHECK(keymap_get_encoder(3, 0, true) == (uint16_t)(value + 2));
    CHECK(keymap_tap_dance_get(7, 4) == (uint16_t)(value + 3));
    CHECK(keymap_cfg_get(CFG_LED_SPEED) == (uint16_t)(value + 4));
    CHECK(keymap_macro_byte(0) == 'h' && keymap_macro_byte(4) == 'o' && keymap_macro_byte(5) == 0);
    CHECK(keymap_macro_byte(KEYMAP_MACRO_BYTES - 2) == (uint8_t)(value & 0xFF));
}

static void test_edits_survive_a_reboot(void)
{
    blank_flash();
    boot();
    edit_everything(0x1234);
    CHECK(store_dirty());
    store_flush();
    CHECK(!store_dirty());

    boot();
    check_everything(0x1234);
    CHECK(keymap_get(1, 1, 1) == KC_A + ((1 + 1 + 1) & 7)); // the rest is as it was
    CHECK(violations == 0);
}

static void test_slots_alternate_and_the_newest_wins(void)
{
    blank_flash();
    boot();

    edit_everything(0x100);
    store_flush(); // slot 0 (nothing was active)
    edit_everything(0x200);
    store_flush(); // slot 1
    edit_everything(0x300);
    store_flush(); // back to slot 0

    boot();
    check_everything(0x300);
    CHECK(violations == 0);
    CHECK(erases == 3 * STORE_SLOT_SECTORS);
}

static void test_damaged_newest_slot_falls_back_to_the_previous(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();
    edit_everything(0x200);
    store_flush();

    // flip a bit in the payload of the newest slot: its CRC no longer matches
    const unsigned slot_bytes = STORE_SLOT_SECTORS * STORE_SECTOR_BYTES;
    const unsigned newest     = STORE_BASE + 1 * slot_bytes; // profile 0: slot 0 was written first, slot 1 second
    flash[newest + 40] &= 0xFE;
    flash[newest + 41] ^= 0xFF;

    boot();
    check_everything(0x100);
}

static void test_a_slot_of_another_build_is_ignored(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();

    salt = 1; // the new firmware ships another default keymap
    boot();
    CHECK(keymap_get(0, 0, 0) == KC_A + 1); // the new default, not the saved word
}

static void test_a_slot_of_the_smk_port_is_ignored(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();

    flash[STORE_BASE + 0] = 'K'; // the magic of the SMK port's slots
    flash[STORE_BASE + 1] = 'M';
    boot();
    CHECK(keymap_get(0, 0, 0) == KC_A); // the only slot written is not ours any more: the defaults
    CHECK(violations == 0);
}

static void test_power_cut_leaves_the_old_or_the_new_profile_never_a_mix(void)
{
    unsigned old_seen = 0;
    unsigned new_seen = 0;

    for (long cut = 0; cut <= STORE_SLOT_SECTORS + 1920 + 40; cut += 7) {
        blank_flash();
        boot();
        edit_everything(0x100);
        store_flush();

        edit_everything(0x200);
        budget = cut;
        store_flush();
        budget = -1;

        boot();

        if (keymap_get(0, 0, 0) == 0x200) {
            check_everything(0x200);
            new_seen++;
        } else {
            check_everything(0x100);
            old_seen++;
        }
    }

    CHECK(old_seen > 0 && new_seen > 0); // the loop crossed the moment the new profile became valid
    CHECK(violations == 0);
}

static void test_power_cut_during_the_very_first_save(void)
{
    for (long cut = 0; cut <= STORE_SLOT_SECTORS + 1920 + 40; cut += 53) {
        blank_flash();
        boot();
        edit_everything(0x100);
        budget = cut;
        store_flush();
        budget = -1;

        boot();

        if (keymap_get(0, 0, 0) == 0x100) {
            check_everything(0x100);
        } else {
            CHECK(keymap_get(0, 0, 0) == KC_A); // nothing valid: the defaults
            CHECK(keymap_macro_byte(0) == 0);
        }
    }
}

static void test_profiles_are_independent(void)
{
    blank_flash();
    boot();
    edit_everything(0x100); // profile 0

    CHECK(store_request(1));
    store_task(0, false); // switching saves the profile being left
    CHECK(store_profile() == 1);
    CHECK(keymap_get(0, 0, 0) == KC_A); // a profile never used starts from the defaults
    CHECK(keymap_macro_byte(0) == 0);

    edit_everything(0x400); // profile 1
    CHECK(store_request(3));
    store_task(0, false);
    CHECK(store_profile() == 3);
    CHECK(keymap_get(0, 0, 0) == KC_A);

    CHECK(store_request(0));
    store_task(0, false);
    check_everything(0x100);
    CHECK(store_request(1));
    store_task(0, false);
    check_everything(0x400);

    boot(); // power on lands on profile 0 whatever was in use
    check_everything(0x100);
    CHECK(violations == 0);
    CHECK(!store_request(STORE_PROFILES));
}

static void test_the_first_profile_can_be_chosen(void)
{
    blank_flash();
    boot();
    store_request(2);
    store_task(0, false);
    edit_everything(0x500); // profile 2
    store_flush();

    keymap_init();
    store_init(2);
    CHECK(store_profile() == 2);
    check_everything(0x500);

    keymap_init();
    store_init(9); // no such profile: the first
    CHECK(store_profile() == 0);
}

static void test_a_switch_waits_for_all_keys_up(void)
{
    blank_flash();
    boot();
    store_request(2);
    store_task(0, true); // a key is down
    CHECK(store_profile() == 0);
    store_task(10, false);
    CHECK(store_profile() == 2);
}

static void test_loading_tells_the_firmware(void)
{
    blank_flash();
    loaded = 0;
    boot();
    CHECK(loaded == 1);
    store_request(1);
    store_task(0, false);
    CHECK(loaded == 2);
}

static void test_save_after_the_quiet_time_only(void)
{
    blank_flash();
    boot();

    store_task(1000, false);
    keymap_set(0, 0, 0, KC_Z);
    store_task(1100, false); // the edit is seen here
    CHECK(store_dirty());

    store_task(1100 + STORE_QUIET_MS - 1, false);
    CHECK(erases == 0);

    keymap_set(0, 0, 1, KC_Y); // another edit: the quiet time starts over
    store_task(1100 + STORE_QUIET_MS, false);
    CHECK(erases == 0);

    store_task(1100 + 2 * STORE_QUIET_MS, true); // a key is down: not now
    CHECK(erases == 0);

    store_task(1100 + 2 * STORE_QUIET_MS + 5, false);
    CHECK(erases == STORE_SLOT_SECTORS);
    CHECK(!store_dirty());

    boot();
    CHECK(keymap_get(0, 0, 0) == KC_Z && keymap_get(0, 0, 1) == KC_Y);
}

static void test_the_clock_wrap_does_not_save_early(void)
{
    blank_flash();
    boot();
    store_task(65000, false);
    keymap_set(0, 0, 0, KC_Z);
    store_task(65100, false);
    store_task(1000, false); // 1436 ms later, the 16-bit clock has wrapped
    CHECK(erases == 0);
    store_task(1700, false); // 2136 ms
    CHECK(erases == STORE_SLOT_SECTORS);
}

static void test_reset_to_defaults_is_saved_too(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();

    keymap_reset();
    CHECK(store_dirty());
    store_flush();

    boot();
    CHECK(keymap_get(0, 0, 0) == KC_A);
    CHECK(keymap_macro_byte(0) == 'h'); // the macros are not part of a keymap reset
}

int main(void)
{
    test_blank_flash_gives_the_defaults();
    test_edits_survive_a_reboot();
    test_slots_alternate_and_the_newest_wins();
    test_damaged_newest_slot_falls_back_to_the_previous();
    test_a_slot_of_another_build_is_ignored();
    test_a_slot_of_the_smk_port_is_ignored();
    test_power_cut_leaves_the_old_or_the_new_profile_never_a_mix();
    test_power_cut_during_the_very_first_save();
    test_profiles_are_independent();
    test_the_first_profile_can_be_chosen();
    test_a_switch_waits_for_all_keys_up();
    test_loading_tells_the_firmware();
    test_save_after_the_quiet_time_only();
    test_the_clock_wrap_does_not_save_early();
    test_reset_to_defaults_is_saved_too();
    return tests_result();
}
