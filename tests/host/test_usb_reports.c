#include "check.h"
#include "keycodes.h"
#include "usb_reports.h"
#include <string.h>

static bool    configured = true;
static uint8_t protocol   = 1;
static bool    ep_free    = true;
static uint8_t sent[64][16];
static uint8_t sent_length[64];
static unsigned sent_count;

bool usb_reports_hook_ep1_free(void) { return ep_free; }
bool usb_reports_hook_configured(void) { return configured; }
uint8_t usb_reports_hook_protocol(void) { return protocol; }

void usb_reports_hook_ep1_send(const uint8_t *data, uint8_t length)
{
    memcpy(sent[sent_count], data, length);
    sent_length[sent_count++] = length;
}

static void reset(void)
{
    configured = true;
    protocol   = 1;
    ep_free    = true;
    sent_count = 0;
    report_init();
    usb_reports_init();
}

static void drain(void)
{
    for (unsigned i = 0; i < 40; i++) {
        usb_reports_pump();
    }
}

static void test_keyboard_report_framing(void)
{
    reset();
    report_add_key(KC_A);
    report_add_mods(0x02);
    usb_reports_send(REPORT_KEYBOARD);
    drain();
    CHECK(sent_count == 1 && sent_length[0] == 9);
    CHECK(sent[0][0] == 3 && sent[0][1] == 0x02 && sent[0][2] == 0 && sent[0][3] == KC_A); // ID, mods, reserved, key

    reset();
    protocol = 0; // a BIOS: boot protocol has no report ID
    report_add_key(KC_A);
    usb_reports_send(REPORT_KEYBOARD);
    drain();
    CHECK(sent_count == 1 && sent_length[0] == 8 && sent[0][2] == KC_A);
}

static void test_consumer_and_system_framing(void)
{
    reset();
    report_set_consumer(0x0E2);
    usb_reports_send(REPORT_CONSUMER);
    report_set_system(2);
    usb_reports_send(REPORT_SYSTEM);
    drain();
    CHECK(sent_count == 2);
    CHECK(sent_length[0] == 3 && sent[0][0] == 2 && sent[0][1] == 0xE2 && sent[0][2] == 0x00);
    CHECK(sent_length[1] == 2 && sent[1][0] == 1 && sent[1][1] == 2);

    reset();
    protocol = 0; // no boot form for these
    report_set_consumer(0x0E2);
    usb_reports_send(REPORT_CONSUMER);
    usb_reports_send(REPORT_SYSTEM);
    drain();
    CHECK(sent_count == 0);
}

static void test_each_change_is_its_own_packet_in_order(void)
{
    reset();
    report_add_key(KC_A);
    usb_reports_send(REPORT_KEYBOARD); // pressed...
    report_del_key(KC_A);
    usb_reports_send(REPORT_KEYBOARD); // ...and released in the same pass

    usb_reports_pump(); // one per call, as the endpoint frees up
    CHECK(sent_count == 1 && sent[0][3] == KC_A);
    ep_free = false;
    usb_reports_pump();
    CHECK(sent_count == 1); // the endpoint is busy
    ep_free = true;
    usb_reports_pump();
    CHECK(sent_count == 2 && sent[1][3] == 0);
    usb_reports_pump();
    CHECK(sent_count == 2);
}

static void test_nothing_is_sent_before_the_host_configures(void)
{
    reset();
    configured = false;
    report_add_key(KC_A);
    usb_reports_send(REPORT_KEYBOARD);
    configured = true;
    drain();
    CHECK(sent_count == 0);
}

static void test_flush(void)
{
    reset();
    report_add_key(KC_A);
    usb_reports_send(REPORT_KEYBOARD);
    usb_reports_flush();
    drain();
    CHECK(sent_count == 0);
}

static void test_full_queue_keeps_the_newest_state_of_each_kind(void)
{
    reset();
    ep_free = false; // the host is not taking anything

    for (uint8_t key = KC_A; key < KC_A + 3 * USB_REPORT_QUEUE; key++) {
        report_clear_keys();
        report_add_key(key);
        usb_reports_send(REPORT_KEYBOARD);
    }

    CHECK(usb_reports_overflows() > 0);

    report_set_consumer(0x0E9);
    usb_reports_send(REPORT_CONSUMER); // a different kind when full: the oldest keyboard packet goes

    ep_free = true;
    drain();

    CHECK(sent_count == USB_REPORT_QUEUE - 1);
    CHECK(sent[sent_count - 1][0] == 2 && sent[sent_count - 1][1] == 0xE9); // the consumer packet got in
    CHECK(sent[sent_count - 2][3] == KC_A + 3 * USB_REPORT_QUEUE - 1);       // and the keyboard state is the last one
}

int main(void)
{
    test_keyboard_report_framing();
    test_consumer_and_system_framing();
    test_each_change_is_its_own_packet_in_order();
    test_nothing_is_sent_before_the_host_configures();
    test_flush();
    test_full_queue_keeps_the_newest_state_of_each_kind();
    return tests_result();
}
