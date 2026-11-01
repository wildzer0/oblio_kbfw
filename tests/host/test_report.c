#include "check.h"
#include "keycodes.h"
#include "report.h"
#include <string.h>

static void test_boot_report_keeps_the_order_and_six_keys(void)
{
    uint8_t out[REPORT_BOOT_SIZE];

    report_init();
    for (uint8_t key = KC_A; key < KC_A + 6; key++) {
        report_add_key(key);
    }
    report_add_key(KC_G); // the seventh does not fit

    report_build_boot(out);
    CHECK(out[0] == 0 && out[1] == 0);
    CHECK(out[2] == KC_A && out[7] == KC_F);
    CHECK(report_key_pressed(KC_G)); // but the NKRO bitmap has it

    report_del_key(KC_B);
    report_add_key(KC_H); // takes the free slot
    report_build_boot(out);
    CHECK(out[3] == KC_H);

    report_add_key(KC_A); // already down: no duplicate
    report_build_boot(out);
    unsigned a = 0;
    for (unsigned i = 2; i < REPORT_BOOT_SIZE; i++) {
        a += out[i] == KC_A;
    }
    CHECK(a == 1);

    report_clear_keys();
    report_build_boot(out);
    CHECK(out[2] == 0 && !report_key_pressed(KC_A));
}

static void test_nkro_bitmap(void)
{
    uint8_t out[REPORT_NKRO_SIZE];

    report_init();
    report_add_key(KC_A);                 // usage 4: byte 0, bit 4
    report_add_key(KC_F12);               // usage 0x45: byte 8, bit 5
    report_add_key(REPORT_NKRO_BITS - 1); // the last one in the bitmap
    report_add_key(REPORT_NKRO_BITS);     // outside it: ignored by the bitmap, fine for the boot report
    report_add_mods(0x42);

    report_build_nkro(out);
    CHECK(out[0] == 0x42);
    CHECK(out[1 + 0] == (1u << 4));
    CHECK(out[1 + 8] == (1u << 5));
    CHECK(out[1 + REPORT_NKRO_BYTES - 1] == 0x80);

    report_del_key(KC_A);
    report_build_nkro(out);
    CHECK(out[1] == 0);
}

static void test_modifiers(void)
{
    report_init();
    report_add_mods(0x01);
    report_add_mods(0x20);
    CHECK(report_mods() == 0x21);
    report_del_mods(0x01);
    CHECK(report_mods() == 0x20);
    report_clear_mods();
    CHECK(report_mods() == 0);
}

static void test_keycode_encoding(void)
{
    // the numbers Vial uses
    CHECK(LSFT(KC_B) == 0x0205);
    CHECK(RALT(KC_A) == 0x1404);
    CHECK(MT(MOD_LCTL, KC_A) == 0x2104);
    CHECK(LT(1, KC_SPC) == 0x412C);
    CHECK(MO(1) == 0x5221 && TG(2) == 0x5262 && OSL(1) == 0x5281 && TD(0) == 0x5700);
    CHECK(MODS_FROM_MASK(MOD_RSFT) == 0x20 && MODS_FROM_MASK(MOD_LGUI) == 0x08);
}

static void test_usages(void)
{
    CHECK(report_keycode_to_consumer(KC_MUTE) == 0x0E2);
    CHECK(report_keycode_to_consumer(KC_VOLU) == 0x0E9);
    CHECK(report_keycode_to_consumer(KC_VOLD) == 0x0EA);
    CHECK(report_keycode_to_consumer(KC_MPLY) == 0x0CD);
    CHECK(report_keycode_to_consumer(KC_MNXT) == 0x0B5);
    CHECK(report_keycode_to_consumer(KC_A) == 0);

    CHECK(report_keycode_to_system(KC_SYSTEM_POWER) == 1);
    CHECK(report_keycode_to_system(KC_SYSTEM_SLEEP) == 2);
    CHECK(report_keycode_to_system(KC_SYSTEM_WAKE) == 4);
    CHECK(report_keycode_to_system(KC_A) == 0);

    // every media keycode of the range has a usage
    for (uint16_t kc = KC_FIRST_MEDIA; kc <= KC_LAST_MEDIA; kc++) {
        CHECK(report_keycode_to_consumer((uint8_t)kc) != 0);
    }

    report_init();
    report_set_consumer(0x0E2);
    report_set_system(2);
    CHECK(report_consumer() == 0x0E2 && report_system() == 2);
    report_init();
    CHECK(report_consumer() == 0 && report_system() == 0);
}

int main(void)
{
    test_boot_report_keeps_the_order_and_six_keys();
    test_nkro_bitmap();
    test_modifiers();
    test_keycode_encoding();
    test_usages();
    return tests_result();
}
