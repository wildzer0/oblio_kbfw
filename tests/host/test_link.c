#include "check.h"
#include "link.h"
#include "radio.h"
#include "settings.h"
#include "store.h"
#include <string.h>

// ---- the board's side ----
static link_mode_t selector;
static uint16_t    now;
static unsigned    flash_kind[16], flash_index[16], flashes;

link_mode_t link_hook_selector(void) { return selector; }

void link_hook_flash(uint8_t kind, uint8_t index)
{
    flash_kind[flashes]    = kind;
    flash_index[flashes++] = index;
}

// the flash of the settings: erased, nothing else matters here
void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    (void)addr;
    memset(dst, 0xFF, len);
}
void store_hook_erase(uint16_t sector) { (void)sector; }
void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len) { (void)addr; (void)src; (void)len; }
void store_hook_loaded(void) {}

// ---- a radio that always listens ----
static uint8_t  tx[32][RADIO_FRAME_MAX];
static uint8_t  tx_length[32];
static unsigned tx_count;
static bool     select_low;
static uint8_t  answers[64];
static unsigned answers_head, answers_tail;

bool radio_hook_ready(void) { return true; }
void radio_hook_select(bool low) { select_low = low; }

void radio_hook_tx_start(const uint8_t *frame, uint8_t length)
{
    memcpy(tx[tx_count], frame, length);
    tx_length[tx_count++] = length;

    if (length == 6 && frame[1] == 0x01) { // the radio answers every mode command: 02 01 00 00 00 chk
        static const uint8_t answer[6] = {0x02, 0x01, 0x00, 0x00, 0x00, 0x52};

        for (unsigned i = 0; i < 6; i++) {
            answers[answers_tail++ % sizeof answers] = answer[i];
        }
    }
}

bool radio_hook_tx_done(void) { return true; }
bool radio_hook_rx_pop(uint8_t *byte)
{
    if (answers_head == answers_tail) {
        return false;
    }

    *byte = answers[answers_head++ % sizeof answers];
    return true;
}
uint16_t radio_hook_now_ms(void) { return now; }
void radio_hook_delay_ms(uint8_t ms) { now = (uint16_t)(now + ms); }

static void reset(void)
{
    selector = LINK_USB;
    now      = 0;
    flashes  = 0;
    tx_count = 0;
    answers_head = answers_tail = 0;
    settings_load();
    settings_set_bt_slot(1);
    link_init();
}

static void run(unsigned ms)
{
    for (unsigned i = 0; i < ms; i++) {
        now++;
        link_task(now);
    }
}

static unsigned modes_sent(void)
{
    unsigned n = 0;

    for (unsigned i = 0; i < tx_count; i++) {
        n += (tx_length[i] == 6 && tx[i][1] == 0x01);
    }

    return n;
}

// The first mode command sent (the radio also polls its status every 200 ms, so frame 0 is not always the one)
static const uint8_t *first_mode(void)
{
    for (unsigned i = 0; i < tx_count; i++) {
        if (tx_length[i] == 6 && tx[i][1] == 0x01) {
            return tx[i];
        }
    }

    return 0;
}

static void test_the_selector_must_be_steady_for_50_ms(void)
{
    reset();
    selector = LINK_BT;
    run(LINK_SELECTOR_STABLE_MS);
    CHECK(link_mode() == LINK_NONE && tx_count == 0); // 50 reads, the first only starts the count

    run(2);
    CHECK(link_mode() == LINK_BT && link_wireless());
    run(5);
    CHECK(tx_count > 0 && tx[0][1] == 0x01 && tx[0][2] == 0 && tx[0][3] == 1); // the mode command, slot 1, no pairing
    CHECK(flashes == 1 && flash_kind[0] == 1 && flash_index[0] == 1);
}

static void test_a_bouncing_selector_is_ignored(void)
{
    reset();

    for (unsigned i = 0; i < 20; i++) {
        selector = (i & 1) ? LINK_BT : LINK_2G4;
        run(10);
    }

    CHECK(link_mode() == LINK_NONE && tx_count == 0);
}

static void test_usb_is_the_cable(void)
{
    reset();
    selector = LINK_USB;
    run(100);
    CHECK(link_mode() == LINK_USB && !link_wireless() && tx_count == 0 && flashes == 0);
    CHECK(!radio_active());
}

static void test_2g4(void)
{
    reset();
    selector = LINK_2G4;
    run(60);
    CHECK(link_wireless() && link_mode() == LINK_2G4);
    CHECK(tx[0][1] == 0x01 && tx[0][3] == 0); // slot 0
    CHECK(flashes == 1 && flash_kind[0] == 0);
}

static void test_leaving_the_radio_lets_go_of_the_keys_first(void)
{
    reset();
    selector = LINK_BT;
    run(100);
    tx_count = 0;

    selector = LINK_USB;
    run(100);
    CHECK(link_mode() == LINK_USB);
    CHECK(tx_count >= 2); // an empty keyboard report and an empty consumer report
    CHECK(tx_length[0] == 30 && tx[0][3] == 0 && tx_length[1] == 13 && tx[1][2] == 0);
    CHECK(!radio_active() && !select_low); // then the radio is left alone
}

static void test_a_tap_on_a_bluetooth_key_chooses_the_slot(void)
{
    reset();
    selector = LINK_BT;
    run(100);
    tx_count = 0;
    flashes  = 0;

    link_key(3, true, now);
    run(100);
    link_key(3, false, now);
    run(5);

    CHECK(settings.bt_slot == 3 && settings_dirty());
    CHECK(first_mode() && first_mode()[2] == 0 && first_mode()[3] == 3); // the mode command for slot 3
    CHECK(flashes == 1 && flash_kind[0] == 1 && flash_index[0] == 3);
}

static void test_a_hold_pairs(void)
{
    reset();
    selector = LINK_BT;
    run(100);
    tx_count = 0;
    flashes  = 0;

    link_key(2, true, now);
    run(LINK_PAIRING_HOLD_MS - 1);
    CHECK(modes_sent() == 0 && flashes == 0);
    run(2);
    CHECK(first_mode() && first_mode()[2] == 1 && first_mode()[3] == 2); // pairing, slot 2
    CHECK(flashes == 1 && flash_kind[0] == 2 && flash_index[0] == 2);
    CHECK(settings.bt_slot == 2);

    tx_count = 0;
    link_key(2, false, now); // letting go after the hold does nothing more
    run(5);
    CHECK(modes_sent() == 0 && flashes == 1);
}

static void test_the_keys_outside_bluetooth_only_remember_the_slot(void)
{
    reset();
    selector = LINK_2G4;
    run(100);
    tx_count = 0;

    link_key(3, true, now);
    link_key(3, false, now);
    CHECK(settings.bt_slot == 3);
    run(5);
    CHECK(tx_count == 0 || tx[0][3] == 0); // nothing re-sent for another slot: 2.4G stays 2.4G

    link_key(2, true, now);
    run(LINK_PAIRING_HOLD_MS + 5); // a hold does not pair in 2.4G either
    CHECK(settings.bt_slot == 2);
    for (unsigned i = 0; i < tx_count; i++) {
        CHECK(!(tx_length[i] == 6 && tx[i][1] == 0x01 && tx[i][2] == 1));
    }
}

static void test_another_key_released_is_not_a_tap(void)
{
    reset();
    selector = LINK_BT;
    run(100);
    tx_count = 0;
    link_key(1, true, now);
    link_key(2, false, now); // a different key
    CHECK(settings.bt_slot == 1);
    link_key(1, false, now);
    run(5);
    CHECK(settings.bt_slot == 1);
}

static void test_wake_applies_the_selector_again(void)
{
    reset();
    selector = LINK_2G4;
    run(100);
    CHECK(link_mode() == LINK_2G4);

    link_wake();
    CHECK(link_mode() == LINK_NONE && !radio_active());
    tx_count = 0;
    run(100);
    CHECK(link_mode() == LINK_2G4 && tx_count > 0 && tx[0][1] == 0x01);
}

int main(void)
{
    test_the_selector_must_be_steady_for_50_ms();
    test_a_bouncing_selector_is_ignored();
    test_usb_is_the_cable();
    test_2g4();
    test_leaving_the_radio_lets_go_of_the_keys_first();
    test_a_tap_on_a_bluetooth_key_chooses_the_slot();
    test_a_hold_pairs();
    test_the_keys_outside_bluetooth_only_remember_the_slot();
    test_another_key_released_is_not_a_tap();
    test_wake_applies_the_selector_again();
    return tests_result();
}
