#include "report.h"
#include "keycodes.h"
#include "platform.h"
#include <string.h>

static uint8_t  mods;
static uint8_t  boot_keys[REPORT_BOOT_KEYS]; // in the order the keys went down
static uint8_t  nkro_bits[REPORT_NKRO_BYTES];
static uint16_t consumer;
static uint16_t system_usage;

static uint8_t mouse_buttons;
static int16_t mouse_x;
static int16_t mouse_y;
static int8_t  mouse_wheel;

void report_init(void)
{
    mods = 0;
    memset(boot_keys, 0, sizeof boot_keys);
    memset(nkro_bits, 0, sizeof nkro_bits);
    consumer     = 0;
    system_usage = 0;
    mouse_buttons = 0;
    mouse_x       = 0;
    mouse_y       = 0;
    mouse_wheel   = 0;
}

void report_add_key(uint8_t key)
{
    if (key == KC_NO) {
        return;
    }

    if (key < REPORT_NKRO_BITS) {
        nkro_bits[key >> 3] |= (uint8_t)(1u << (key & 7u));
    }

    uint8_t free_slot = REPORT_BOOT_KEYS;

    for (uint8_t i = 0; i < REPORT_BOOT_KEYS; i++) {
        if (boot_keys[i] == key) {
            return;
        }

        if (free_slot == REPORT_BOOT_KEYS && boot_keys[i] == 0) {
            free_slot = i;
        }
    }

    if (free_slot < REPORT_BOOT_KEYS) {
        boot_keys[free_slot] = key;
    }
}

void report_del_key(uint8_t key)
{
    if (key == KC_NO) {
        return;
    }

    if (key < REPORT_NKRO_BITS) {
        nkro_bits[key >> 3] &= (uint8_t)~(1u << (key & 7u));
    }

    for (uint8_t i = 0; i < REPORT_BOOT_KEYS; i++) {
        if (boot_keys[i] == key) {
            boot_keys[i] = 0;
        }
    }
}

void report_clear_keys(void)
{
    memset(boot_keys, 0, sizeof boot_keys);
    memset(nkro_bits, 0, sizeof nkro_bits);
}

void report_add_mods(uint8_t m)
{
    mods |= m;
}

void report_del_mods(uint8_t m)
{
    mods &= (uint8_t)~m;
}

void report_clear_mods(void)
{
    mods = 0;
}

uint8_t report_mods(void)
{
    return mods;
}

void report_build_boot(uint8_t out[REPORT_BOOT_SIZE])
{
    out[0] = mods;
    out[1] = 0;
    memcpy(&out[2], boot_keys, REPORT_BOOT_KEYS);
}

void report_build_nkro(uint8_t out[REPORT_NKRO_SIZE])
{
    out[0] = mods;
    memcpy(&out[1], nkro_bits, REPORT_NKRO_BYTES);
}

void report_set_consumer(uint16_t usage)
{
    consumer = usage;
}

uint16_t report_consumer(void)
{
    return consumer;
}

void report_set_system(uint16_t usage)
{
    system_usage = usage;
}

uint16_t report_system(void)
{
    return system_usage;
}

// ---- the mouse ----
#define MOUSE_LIMIT 127 // the deltas of one report stay well inside the 16-bit field: no overflow, and the host sees a smooth move

static int16_t clamped(int16_t value, int16_t add)
{
    const int16_t sum = (int16_t)(value + add);

    if (sum > MOUSE_LIMIT) {
        return MOUSE_LIMIT;
    }

    if (sum < -MOUSE_LIMIT) {
        return -MOUSE_LIMIT;
    }

    return sum;
}

void report_mouse_button(uint8_t mask, bool pressed)
{
    if (pressed) {
        mouse_buttons |= mask;
    } else {
        mouse_buttons &= (uint8_t)~mask;
    }
}

void report_mouse_move(int8_t dx, int8_t dy)
{
    mouse_x = clamped(mouse_x, dx);
    mouse_y = clamped(mouse_y, dy);
}

void report_mouse_wheel(int8_t delta)
{
    mouse_wheel = (int8_t)(mouse_wheel + delta);
}

uint8_t report_mouse_buttons(void)
{
    return mouse_buttons;
}

void report_build_mouse(uint8_t out[REPORT_MOUSE_SIZE])
{
    out[0] = mouse_buttons;
    out[1] = (uint8_t)(mouse_x & 0xFF);
    out[2] = (uint8_t)(mouse_x >> 8);
    out[3] = (uint8_t)(mouse_y & 0xFF);
    out[4] = (uint8_t)(mouse_y >> 8);
    out[5] = (uint8_t)mouse_wheel;

    // Sent: the next report starts from nothing (a relative axis must not repeat a movement)
    mouse_x     = 0;
    mouse_y     = 0;
    mouse_wheel = 0;
}

// Usages of the Consumer page (0x0C) and the Generic Desktop page (0x01), HID Usage Tables 1.12.
typedef struct {
    uint8_t  keycode;
    uint16_t usage;
} usage_entry_t;

static const ROM usage_entry_t consumer_usages[] = {
    {KC_MUTE, 0x0E2}, {KC_VOLU, 0x0E9}, {KC_VOLD, 0x0EA},
    {KC_MNXT, 0x0B5}, {KC_MPRV, 0x0B6}, {KC_MSTP, 0x0B7}, {KC_MPLY, 0x0CD},
    {KC_MSEL, 0x183}, // consumer control configuration
    {KC_EJCT, 0x0CC}, // stop/eject
    {KC_MAIL, 0x18A}, {KC_CALC, 0x192}, {KC_MYCM, 0x194}, // local browser
    {KC_WSCH, 0x221}, {KC_WHOM, 0x223}, {KC_WBAK, 0x224}, {KC_WFWD, 0x225}, {KC_WSTP, 0x226}, {KC_WREF, 0x227},
    {KC_WFAV, 0x22A},
    {KC_MFFD, 0x0B3}, {KC_MRWD, 0x0B4},
    {KC_BRIU, 0x06F}, {KC_BRID, 0x070},
    {KC_CPNL, 0x19F}, {KC_ASST, 0x1CB},
    {KC_MCTL, 0x29F}, // show all windows
    {KC_LPAD, 0x2A0}, // soft key left
};

uint16_t report_keycode_to_consumer(uint8_t keycode)
{
    for (uint8_t i = 0; i < sizeof consumer_usages / sizeof consumer_usages[0]; i++) {
        if (consumer_usages[i].keycode == keycode) {
            return consumer_usages[i].usage;
        }
    }

    return 0;
}

// The system control report is a bit field: power down, sleep, wake up
uint16_t report_keycode_to_system(uint8_t keycode)
{
    switch (keycode) {
        case KC_SYSTEM_POWER: return 1u << 0;
        case KC_SYSTEM_SLEEP: return 1u << 1;
        case KC_SYSTEM_WAKE:  return 1u << 2;
        default:              return 0;
    }
}
