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

uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_override(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
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
static unsigned flushes; // begin/end pairs: one for each save
static bool     flushing;

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

void store_hook_flush_begin(void)
{
    if (flushing) {
        violations++; // nested
    }
    flushing = true;
    flushes++;
}

void store_hook_flush_end(void)
{
    if (!flushing) {
        violations++;
    }
    flushing = false;
}

void store_hook_erase(uint16_t sector_addr)
{
    if (!flushing) {
        violations++; // a flash operation outside store_hook_flush_begin/end
    }

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
    // the programming is not wrapped (only the erases stall the chip), so this is allowed outside begin/end
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
    flushes    = 0;
    salt       = 0;
}

// Power on: the RAM copy starts from nothing, then the store loads what the flash has
static void boot(void)
{
    keymap_init();
    store_init();
}

static void test_blank_flash_gives_the_defaults(void)
{
    blank_flash();
    boot();
    CHECK(keymap_get(0, 0, 0) == KC_A);
    CHECK(keymap_get(KEYMAP_LAYERS - 1, 4, 15) == KC_A + ((KEYMAP_LAYERS - 1 + 4 + 15) & 7));
    CHECK(keymap_tap_dance_get(2, 3) == 23);
    CHECK(keymap_cfg_get(CFG_LED_BRIGHT) == 101);
    CHECK(!store_dirty());
    CHECK(violations == 0);
}

// Makes the next save a whole one (a reset, a macro, a lot at once do the same in practice)
static void whole_save(void)
{
    keymap_changes_all();
    store_flush();
}

static void edit_everything(uint16_t value)
{
    const uint8_t macro[] = {'h', 'e', 'l', 'l', 'o', 0};

    keymap_set(0, 0, 0, value);
    keymap_set(KEYMAP_LAYERS - 1, 4, 15, (uint16_t)(value + 1));
    keymap_set_encoder(3, 0, true, (uint16_t)(value + 2));
    keymap_tap_dance_set(KEYMAP_TAP_DANCE_COUNT - 1, 4, (uint16_t)(value + 3));
    keymap_cfg_set(CFG_TAP_TERM, (uint16_t)(value + 4)); // the lighting is saved with the settings, not here
    keymap_macro_write(0, sizeof macro, macro);
    keymap_macro_write(KEYMAP_MACRO_BYTES - 2, 1, (const uint8_t *)&value);
}

static void check_everything(uint16_t value)
{
    CHECK(keymap_get(0, 0, 0) == value);
    CHECK(keymap_get(KEYMAP_LAYERS - 1, 4, 15) == (uint16_t)(value + 1));
    CHECK(keymap_get_encoder(3, 0, true) == (uint16_t)(value + 2));
    CHECK(keymap_tap_dance_get(KEYMAP_TAP_DANCE_COUNT - 1, 4) == (uint16_t)(value + 3));
    CHECK(keymap_cfg_get(CFG_TAP_TERM) == (uint16_t)(value + 4));
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

static void test_whole_saves_alternate_and_the_newest_wins(void)
{
    blank_flash();
    boot();

    edit_everything(0x100);
    whole_save(); // slot 0 (nothing was active)
    edit_everything(0x200);
    whole_save(); // slot 1
    edit_everything(0x300);
    whole_save(); // back to slot 0

    boot();
    check_everything(0x300);
    CHECK(violations == 0);
    CHECK(erases == 3 * STORE_SLOT_SECTORS);
    CHECK(store_saves() == 3 && store_log_used() == 0);
}

static void test_damaged_newest_slot_falls_back_to_the_previous(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    whole_save();
    edit_everything(0x200);
    whole_save();

    // flip a bit in the payload of the newest slot: its CRC no longer matches
    const unsigned slot_bytes = STORE_SLOT_SECTORS * STORE_SECTOR_BYTES;
    const unsigned newest     = STORE_BASE + 1 * slot_bytes; // slot 0 was written first, slot 1 second
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

// The most operations a save can take: one per erased sector plus one per programmed byte, and the payload is smaller than the slot
#define CUT_POINTS (STORE_SLOT_SECTORS + (STORE_SLOT_SECTORS * STORE_SECTOR_BYTES) + 40)

static void test_power_cut_in_a_whole_save_leaves_the_old_or_the_new_copy_never_a_mix(void)
{
    unsigned old_seen = 0;
    unsigned new_seen = 0;

    for (long cut = 0; cut <= CUT_POINTS; cut += 7) {
        blank_flash();
        boot();
        edit_everything(0x100);
        whole_save();

        edit_everything(0x200);
        budget = cut;
        keymap_changes_all();
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

    CHECK(old_seen > 0 && new_seen > 0); // the loop crossed the moment the new copy became valid
    CHECK(violations == 0);
}

static void test_power_cut_during_the_very_first_save(void)
{
    for (long cut = 0; cut <= CUT_POINTS; cut += 53) {
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

// Loading the stored keymap tells the rest of the firmware, once
static void test_loading_tells_the_firmware(void)
{
    blank_flash();
    loaded = 0;
    boot();
    CHECK(loaded == 1);
    edit_everything(0x700);
    store_flush();

    keymap_init();
    loaded = 0;
    store_init();
    CHECK(loaded == 1);
    check_everything(0x700);
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

// ---- the log: an edit is a record at the end of the slot, not a whole save ----

static void test_an_edit_is_a_log_record_not_a_whole_save(void)
{
    blank_flash();
    boot();

    keymap_set(0, 0, 0, KC_Z);
    store_flush(); // nothing saved yet: a whole one
    CHECK(erases == STORE_SLOT_SECTORS && flushes == 1 && store_saves() == 1 && store_log_used() == 0);

    flushes = 0;
    keymap_set(0, 0, 1, KC_Y);
    store_flush();
    CHECK(erases == STORE_SLOT_SECTORS);     // nothing erased
    CHECK(flushes == 0);                     // and the backlight was not parked: no begin/end
    CHECK(store_saves() == 1 && store_log_used() == 1);
    CHECK(!store_dirty() && violations == 0);

    boot();
    CHECK(keymap_get(0, 0, 0) == KC_Z && keymap_get(0, 0, 1) == KC_Y);
    CHECK(store_log_used() == 1);            // the next record goes after it
    CHECK(!store_dirty());                   // loading the log is not an edit
}

static void test_the_log_is_replayed_in_order(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();

    keymap_set(2, 2, 2, KC_A);
    store_flush();
    keymap_set(2, 2, 2, KC_B);
    store_flush();
    keymap_set(2, 2, 2, KC_C);
    store_flush();
    CHECK(store_log_used() == 3);

    boot();
    CHECK(keymap_get(2, 2, 2) == KC_C);
    check_everything(0x100);
}

// A full log: the next edit is a whole save into the other slot, and the log starts again
static void test_a_full_log_goes_to_the_other_slot(void)
{
    blank_flash();
    boot();
    edit_everything(0x100);
    store_flush();
    CHECK(erases == STORE_SLOT_SECTORS);

    unsigned edits = 0;

    while (erases == STORE_SLOT_SECTORS && edits < 200) {
        keymap_set(2, (uint8_t)(edits / MATRIX_COLS), (uint8_t)(edits % MATRIX_COLS), (uint16_t)(0x500 + edits)); // a cell of its own each time
        store_flush();
        edits++;
    }

    CHECK(edits == 70);                      // 69 records fit in the room after the payload; the 70th is a whole save
    CHECK(erases == 2 * STORE_SLOT_SECTORS);
    CHECK(store_saves() == 2 && store_log_used() == 0);

    boot();
    check_everything(0x100);                                  // what was saved before the log is still there
    CHECK(keymap_get(2, 0, 0) == 0x500 && keymap_get(2, 4, 5) == (uint16_t)(0x500 + 69)); // the first and the last edit
    CHECK(violations == 0);
}

static void test_many_words_at_once_are_a_whole_save(void)
{
    blank_flash();
    boot();
    keymap_set(0, 0, 0, KC_Z);
    store_flush();

    for (uint8_t i = 0; i < KEYMAP_CHANGE_MAX + 6; i++) {
        keymap_set(1, (uint8_t)(i / MATRIX_COLS), (uint8_t)(i % MATRIX_COLS), (uint16_t)(0x600 + i));
    }

    store_flush();
    CHECK(erases == 2 * STORE_SLOT_SECTORS); // too many to list: written whole into the other slot
    CHECK(store_log_used() == 0);

    boot();
    CHECK(keymap_get(1, 0, 5) == 0x605 && keymap_get(1, 1, 8) == (uint16_t)(0x600 + 16 + 8));
}

// A record cut short by a power failure is skipped, and the log goes on after it
static void test_a_torn_record_is_skipped_and_the_log_goes_on(void)
{
    blank_flash();
    boot();
    keymap_set(0, 0, 0, KC_Z);
    store_flush();

    keymap_set(0, 0, 1, KC_Y);
    budget = 3; // the power goes after three of the five bytes
    store_flush();
    budget = -1;

    boot();
    CHECK(keymap_get(0, 0, 0) == KC_Z);
    CHECK(keymap_get(0, 0, 1) == KC_A + ((0 + 0 + 1) & 7)); // the edit that was cut is not there, and nothing else changed
    CHECK(store_log_used() == 1);                             // the torn record is passed over: the next goes after it

    keymap_set(0, 0, 2, KC_X);
    store_flush();
    boot();
    CHECK(keymap_get(0, 0, 2) == KC_X && keymap_get(0, 0, 0) == KC_Z);
    CHECK(violations == 0);
}

// A power cut in the middle of the records of one save: each word is the old one or the new one, nothing else
static void test_power_cut_in_the_log_never_corrupts_a_word(void)
{
    for (long cut = 0; cut <= 70; cut++) {
        blank_flash();
        boot();
        edit_everything(0x100);
        store_flush();

        edit_everything(0x200);
        budget = cut;
        store_flush();
        budget = -1;

        boot();

        const uint16_t k = keymap_get(0, 0, 0);
        const uint16_t e = keymap_get_encoder(3, 0, true);
        const uint16_t t = keymap_tap_dance_get(KEYMAP_TAP_DANCE_COUNT - 1, 4);

        CHECK(k == 0x100 || k == 0x200);
        CHECK(e == 0x102 || e == 0x202);
        CHECK(t == 0x103 || t == 0x203);
        CHECK(keymap_get(1, 1, 1) == KC_A + ((1 + 1 + 1) & 7)); // untouched
    }

    CHECK(violations == 0);
}

// The lighting is saved with the settings (settings.h), so changing it is not an edit of the keymap
static void test_the_lighting_is_not_a_keymap_edit(void)
{
    blank_flash();
    boot();
    keymap_cfg_set(CFG_LED_BRIGHT, 7);
    keymap_cfg_set(CFG_LED_EFFECT, 2);
    keymap_cfg_set(CFG_LED_SPEED, 3);
    keymap_cfg_set(CFG_LED_COLOR, 0x80A0);
    CHECK(!store_dirty());

    keymap_cfg_set(CFG_TAP_TERM, 150); // the tapping term is
    CHECK(store_dirty());
}

int main(void)
{
    test_blank_flash_gives_the_defaults();
    test_edits_survive_a_reboot();
    test_whole_saves_alternate_and_the_newest_wins();
    test_damaged_newest_slot_falls_back_to_the_previous();
    test_a_slot_of_another_build_is_ignored();
    test_a_slot_of_the_smk_port_is_ignored();
    test_power_cut_in_a_whole_save_leaves_the_old_or_the_new_copy_never_a_mix();
    test_power_cut_during_the_very_first_save();
    test_loading_tells_the_firmware();
    test_save_after_the_quiet_time_only();
    test_the_clock_wrap_does_not_save_early();
    test_reset_to_defaults_is_saved_too();
    test_an_edit_is_a_log_record_not_a_whole_save();
    test_the_log_is_replayed_in_order();
    test_a_full_log_goes_to_the_other_slot();
    test_many_words_at_once_are_a_whole_save();
    test_a_torn_record_is_skipped_and_the_log_goes_on();
    test_power_cut_in_the_log_never_corrupts_a_word();
    test_the_lighting_is_not_a_keymap_edit();
    return tests_result();
}
