#include "check.h"
#include "diag_stubs.h"
#include "radio.h"
#include <string.h>

static bool on_battery; // the radio's status counts as "on battery" when this is set

bool radio_hook_on_battery(void) { return on_battery; }

// ---- a simulated radio and serial port ----
static bool     att;
static uint16_t now;
static bool     select_low;
static bool     select_ok;      // SEL was low whenever a frame started
static unsigned select_released; // times SEL was released
static uint16_t tx_ms;          // how long a frame takes to go out
static uint16_t tx_ends;
static bool     tx_going;
static uint8_t  tx[64][RADIO_FRAME_MAX];
static uint8_t  tx_length[64];
static unsigned tx_count;
static uint8_t  rx[128];
static unsigned rx_head, rx_tail;
static unsigned delay_total;
static unsigned att_low_delays; // radio_sleep: ATT stays low for this many delays

bool radio_hook_ready(void)
{
    return att;
}

void radio_hook_select(bool low)
{
    if (select_low && !low) {
        select_released++;
    }
    select_low = low;
}

void radio_hook_tx_start(const uint8_t *frame, uint8_t length)
{
    if (!select_low) {
        select_ok = false;
    }

    memcpy(tx[tx_count], frame, length);
    tx_length[tx_count++] = length;
    tx_going              = true;
    tx_ends               = (uint16_t)(now + tx_ms);
}

bool radio_hook_tx_done(void)
{
    if (tx_going && (int16_t)(now - tx_ends) >= 0) {
        tx_going = false;
    }

    return !tx_going;
}

bool radio_hook_rx_pop(uint8_t *byte)
{
    if (rx_head == rx_tail) {
        return false;
    }

    *byte = rx[rx_head++ % sizeof rx];
    return true;
}

uint16_t radio_hook_now_ms(void)
{
    return now;
}

void radio_hook_delay_ms(uint8_t ms)
{
    delay_total += ms;

    if (att_low_delays && --att_low_delays == 0) {
        att = true;
    }

    now = (uint16_t)(now + ms);
}

// ---- the harness ----
static void reset(void)
{
    att             = true;
    now             = 0;
    select_low      = false;
    select_ok       = true;
    select_released = 0;
    tx_ms           = 0;
    tx_going        = false;
    tx_count        = 0;
    rx_head = rx_tail = 0;
    delay_total     = 0;
    att_low_delays  = 0;
    radio_init();
}

static void run(unsigned ms)
{
    for (unsigned i = 0; i < ms; i++) {
        now++;
        radio_task();
    }
}

static void inject(const uint8_t *bytes, unsigned length)
{
    for (unsigned i = 0; i < length; i++) {
        rx[rx_tail++ % sizeof rx] = bytes[i];
    }
}

// a frame of the radio with its checksum
static void inject_frame(uint8_t header, uint8_t command, uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t f[6] = {header, command, a, b, c, 0};

    f[5] = radio_checksum(f, 5);
    inject(f, 6);
}

static void answer(uint8_t command)
{
    inject_frame(0x02, command, 0, 0, 0);
}

static void refuse(void)
{
    inject_frame(0x02, 0x00, 0x01, 0x00, 0x00);
}

static bool is_command(unsigned index, uint8_t command)
{
    return tx_length[index] == RADIO_COMMAND_LEN && tx[index][0] == 0x01 && tx[index][1] == command;
}

static unsigned count_of(uint8_t command, unsigned length)
{
    unsigned n = 0;

    for (unsigned i = 0; i < tx_count; i++) {
        n += (tx_length[i] == length && tx[i][1] == command);
    }

    return n;
}

// brings the radio to "in a mode, mode answered, names sent, nothing pending"
static void settle_in_mode(uint8_t slot)
{
    radio_set_link(slot, false);
    run(3);
    answer(0x01);
    run(10);
    answer(0x02);
    run(10);
    tx_count = 0;
}

// ---- tests ----
static void test_frames(void)
{
    uint8_t f[RADIO_FRAME_MAX];

    CHECK(radio_build_command(f, 0x01, 0, 0, 0) == 6);
    CHECK(f[0] == 0x01 && f[1] == 0x01 && f[5] == 0x53); // 0x55 - 2
    radio_build_command(f, 0x06, 0, 0, 0);
    CHECK(f[5] == 0x4E);
    radio_build_command(f, 0x01, 1, 2, 0); // pairing, slot 2
    CHECK(f[2] == 1 && f[3] == 2 && f[5] == (uint8_t)(0x55 - (1 + 1 + 1 + 2)));

    // a report with up to five keys: the list, an empty bitmap
    const uint8_t keys[6] = {4, 5, 0, 0, 0, 0};

    CHECK(radio_build_report(f, 0x02, keys) == 30);
    CHECK(f[0] == 0x01 && f[1] == 0x02 && f[2] == 0x02 && f[3] == 4 && f[4] == 5 && f[5] == 0);
    for (unsigned i = 8; i < 29; i++) {
        CHECK(f[i] == 0);
    }
    CHECK(f[29] == radio_checksum(f, 29));

    // six keys: an empty list, the bitmap (bit n = usage n); a usage of 128 and up has no bit
    const uint8_t six[6] = {4, 5, 0x29, 0xE0, 0x39, 0x52};

    radio_build_report(f, 0, six);
    CHECK(f[3] == 0 && f[7] == 0);
    CHECK(f[9 + 0] == ((1 << 4) | (1 << 5)));
    CHECK(f[9 + 5] == (1 << 1)); // 0x29 = 41: byte 5, bit 1
    CHECK(f[9 + 7] == (1 << 1)); // 0x39 = 57: byte 7, bit 1
    CHECK(f[9 + 10] == (1 << 2)); // 0x52 = 82: byte 10, bit 2
    CHECK(f[9 + 14] == 0 && f[9 + 15] == 0); // 0xE0 = 224: nowhere
    CHECK(f[29] == radio_checksum(f, 29));

    // consumer
    CHECK(radio_build_consumer(f, 0x00E2) == 13);
    CHECK(f[0] == 1 && f[1] == 3 && f[2] == 0xE2 && f[3] == 0 && f[4] == 0 && f[12] == radio_checksum(f, 12));
    radio_build_consumer(f, 0x0223);
    CHECK(f[2] == 0x23 && f[3] == 0x02);

    // names
    CHECK(radio_build_name(f, 1, "OBLIO") == 32);
    CHECK(f[1] == 9 && f[2] == 1 && f[3] == 5 && memcmp(&f[4], "OBLIO", 5) == 0 && f[9] == 0 && f[31] == radio_checksum(f, 31));
    radio_build_name(f, 0, "0123456789012345678901234567890");
    CHECK(f[3] == RADIO_NAME_MAX); // cut to the room there is
}

static void test_a_link_starts_with_the_mode_then_lets_go_of_everything(void)
{
    reset();
    radio_set_link(2, false);
    CHECK(radio_active());
    run(8);

    CHECK(tx_count == 3);
    CHECK(is_command(0, 0x01) && tx[0][2] == 0 && tx[0][3] == 2); // mode, no pairing, slot 2
    CHECK(tx_length[1] == 30 && tx[1][1] == 0x02 && tx[1][2] == 0 && tx[1][3] == 0); // an empty keyboard report
    CHECK(tx_length[2] == 13 && tx[2][1] == 0x03 && tx[2][2] == 0); // and an empty consumer report
    CHECK(select_ok);
    CHECK(!select_low && select_released == 3);

    reset();
    radio_set_link(1, true); // pairing
    run(3);
    CHECK(tx[0][2] == 1 && tx[0][3] == 1);
    reset();
    radio_set_link(9, false); // there is no slot 9
    run(3);
    CHECK(tx[0][3] == RADIO_MODE_BT3);
}

static void test_nothing_goes_out_while_att_is_low(void)
{
    reset();
    att = false;
    radio_set_link(0, false);
    run(50);
    CHECK(tx_count == 0);
    att = true;
    run(3);
    CHECK(tx_count > 0);
}

static void test_select_is_low_for_the_whole_frame(void)
{
    reset();
    tx_ms = 5; // a frame takes 5 ms
    radio_set_link(0, false);

    unsigned low_ms = 0;

    for (unsigned i = 0; i < 40; i++) {
        now++;
        radio_task();
        low_ms += select_low;
    }

    CHECK(select_ok);
    CHECK(low_ms >= 3 * 5 - 3 && low_ms <= 3 * 6 + 3); // three frames of about 5 ms
    CHECK(!select_low);
}

static void test_answers_are_not_waited_for(void)
{
    reset();
    settle_in_mode(0);

    static const uint8_t a[6] = {4, 0, 0, 0, 0, 0};
    static const uint8_t b[6] = {5, 0, 0, 0, 0, 0};

    radio_send_keys(0, a);
    radio_send_keys(0, b);
    run(4); // no answer to the first one has come
    CHECK(tx_count == 2 && tx[0][3] == 4 && tx[1][3] == 5);
}

static void test_the_radios_own_frames_are_answered_first(void)
{
    reset();
    settle_in_mode(0);

    static const uint8_t a[6] = {4, 0, 0, 0, 0, 0};

    radio_send_keys(0, a);
    inject_frame(0x03, 0x11, 0x22, 0x33, 0x44); // a notification
    run(2);
    CHECK(tx_count >= 1 && is_command(0, 0xF0)); // before the report

    tx_count = 0;
    uint8_t info[22] = {0x08, 1, 2, 3};

    info[21] = radio_checksum(info, 21);
    inject(info, sizeof info);
    radio_send_keys(0, a);
    run(4);
    CHECK(is_command(0, 0xF1));
    CHECK(tx_length[1] == 30);

    // a frame with a wrong checksum is not answered
    tx_count = 0;
    uint8_t bad[6] = {0x03, 1, 2, 3, 4, 0x99};

    inject(bad, sizeof bad);
    run(5);
    CHECK(count_of(0xF0, 6) == 0);
}

static void test_a_refused_report_is_offered_again_unless_it_changed_nothing(void)
{
    reset();
    settle_in_mode(0);

    static const uint8_t a[6] = {4, 0, 0, 0, 0, 0};

    radio_send_keys(0, a);
    run(2);
    CHECK(tx_count == 1);
    refuse();
    run(5);
    CHECK(tx_count == 1); // not yet: it waits ~10 ms
    run(10);
    CHECK(tx_count == 2 && tx[1][3] == 4); // the same state again

    answer(0x02); // accepted
    run(3);
    tx_count = 0;

    radio_send_keys(0, a); // the same state once more: the radio has it already
    run(2);
    CHECK(tx_count == 1);
    refuse();
    run(40);
    CHECK(tx_count == 1); // refused for changing nothing: forgotten
}

static void test_a_refused_consumer_report_is_offered_again(void)
{
    reset();
    settle_in_mode(0);

    radio_send_consumer(0x00E9);
    run(2);
    CHECK(tx_count == 1 && tx_length[0] == 13 && tx[0][2] == 0xE9);
    refuse();
    run(15);
    CHECK(tx_count == 2 && tx[1][2] == 0xE9);
}

static void test_the_mode_is_repeated_until_answered(void)
{
    reset();
    radio_set_link(1, false);
    run(3000);
    CHECK(count_of(0x01, 6) == 8); // eight tries, then it gives up

    reset();
    radio_set_link(1, false);
    run(400);
    CHECK(count_of(0x01, 6) == 2);
    answer(0x01);
    run(3000);
    CHECK(count_of(0x01, 6) == 2); // answered: no more
}

static void test_a_refusal_of_the_mode_counts_as_an_answer(void)
{
    reset();
    radio_set_link(0, false);
    run(2); // the mode frame went out
    refuse();
    run(3000);
    CHECK(count_of(0x01, 6) == 1);
}

static void test_bluetooth_names_are_set_once_after_the_first_answer(void)
{
    reset();
    radio_set_link(1, false);
    run(3);
    CHECK(count_of(0x09, 32) == 0);
    answer(0x01);
    run(10);

    CHECK(count_of(0x09, 32) == 2);
    unsigned first = 0;

    for (unsigned i = 0; i < tx_count; i++) {
        if (tx_length[i] == 32) {
            CHECK(tx[i][2] == first); // the LE name (0), then the classic one (1)
            first++;
        }
    }

    radio_set_link(2, false); // another link in the same power-up: not again
    run(5);
    answer(0x01);
    run(20);
    CHECK(count_of(0x09, 32) == 2);
}

static void test_status_is_polled_every_200_ms_and_kept(void)
{
    reset();
    settle_in_mode(0);

    run(1000);
    CHECK(count_of(0x06, 6) >= 4 && count_of(0x06, 6) <= 6);

    const uint8_t status[10] = {0x02, 0x06, 0x00, 0x01, 0x00, 0x03, 0xFF, 0x03, 0x01, 0};
    uint8_t       f[10];

    memcpy(f, status, 10);
    f[9] = radio_checksum(f, 9);
    inject(f, 10);
    run(2);
    CHECK(radio_status()[0] == 0x00 && radio_status()[1] == 0x01 && radio_status()[3] == 0x03 && radio_status()[4] == 0xFF);
}

// The battery and the host's LEDs come out of the status answer: bytes 4-5 are the 10-bit battery reading (little endian), byte 1 carries
// the LEDs in bits 0-2 (see core/radio.h)
static void test_the_status_answer_carries_the_battery_and_the_host_leds(void)
{
    uint8_t f[10];

    reset();
    settle_in_mode(0);

    CHECK(radio_battery_raw() == 0); // nothing has come yet
    CHECK(radio_host_leds() == 0);

    // a reading of 0x0303 (771: ~25%) in bytes 4 (low) and 5 (high), and caps lock down the flag byte
    const uint8_t status[10] = {0x02, 0x06, 0x00, 0x02, 0x00, 0x03, 0x03, 0x03, 0x01, 0};

    memcpy(f, status, 10);
    f[9] = radio_checksum(f, 9);
    inject(f, 10);
    run(2);

    CHECK(radio_battery_raw() == 0x0303);
    CHECK(radio_host_leds() == 2); // bit 1: caps lock

    // with the cable the reading saturates (measured: 00 01 00 03 ff 03 = 0x03FF)
    const uint8_t charging[10] = {0x02, 0x06, 0x00, 0x01, 0x00, 0x03, 0xFF, 0x03, 0x01, 0};

    memcpy(f, charging, 10);
    f[9] = radio_checksum(f, 9);
    inject(f, 10);
    run(2);
    CHECK(radio_battery_raw() == 0x03FF);

    // the same bytes on battery are a reading, and they are kept for the diagnostics to show (the cable is what reads them)
    on_battery = true;

    const uint8_t on_batt[10] = {0x02, 0x06, 0x00, 0x01, 0x00, 0x03, 0x03, 0x03, 0x01, 0};

    memcpy(f, on_batt, 10);
    f[9] = radio_checksum(f, 9);
    inject(f, 10);
    run(2);
    CHECK(radio_battery_raw() == 0x0303);

    on_battery = false;
}

static void test_the_queue_keeps_eight_and_replaces_the_newest(void)
{
    reset();
    settle_in_mode(0);

    att = false;

    for (uint8_t i = 1; i <= 10; i++) {
        const uint8_t keys[6] = {i, 0, 0, 0, 0, 0};

        radio_send_keys(0, keys);
    }

    att = true;
    run(60);
    CHECK(tx_count == 8);
    CHECK(tx[0][3] == 1 && tx[1][3] == 2 && tx[2][3] == 3 && tx[6][3] == 7 && tx[7][3] == 10); // the 8th place holds the newest
}

static void test_garbage_before_a_frame_is_skipped(void)
{
    reset();
    settle_in_mode(0);

    static const uint8_t a[6] = {4, 0, 0, 0, 0, 0};

    radio_send_keys(0, a);
    run(2);
    const uint8_t noise[3] = {0xAA, 0x55, 0x00};

    inject(noise, 3);
    refuse(); // a real refusal after the noise
    run(15);
    CHECK(tx_count == 2); // it was understood: the report is offered again
}

static void test_a_frame_that_never_ends_is_given_up_after_50_ms(void)
{
    reset();
    tx_ms = 5000;
    radio_set_link(0, false);
    run(10);
    CHECK(tx_count == 1 && select_low);
    run(60);
    CHECK(tx_count >= 2); // given up, the next one went out
    CHECK(select_released >= 1);
}

static void test_stop_and_sleep(void)
{
    reset();
    settle_in_mode(0);
    radio_stop();
    CHECK(!radio_active());

    static const uint8_t a[6] = {4, 0, 0, 0, 0, 0};

    radio_send_keys(0, a);
    run(50);
    CHECK(tx_count == 0); // nothing goes out when inactive
    CHECK(!select_low);

    reset();
    settle_in_mode(0);
    att = false;
    att_low_delays = 5; // busy for five milliseconds of waiting
    radio_sleep();
    CHECK(tx_count == 2);
    CHECK(is_command(0, 0x0C) && tx[0][2] == 0x08 && tx[0][3] == 0x07);
    CHECK(is_command(1, 0x0B) && tx[1][2] == 0x00);
    CHECK(delay_total >= 40);
    CHECK(!radio_active() && !select_low && select_ok);
}

static void test_idle(void)
{
    reset();
    CHECK(radio_idle());
    radio_set_link(0, false);
    CHECK(!radio_idle());
    run(10);
    answer(0x01);
    run(20);
    CHECK(radio_idle());
}

int main(void)
{
    test_frames();
    test_a_link_starts_with_the_mode_then_lets_go_of_everything();
    test_nothing_goes_out_while_att_is_low();
    test_select_is_low_for_the_whole_frame();
    test_answers_are_not_waited_for();
    test_the_radios_own_frames_are_answered_first();
    test_a_refused_report_is_offered_again_unless_it_changed_nothing();
    test_a_refused_consumer_report_is_offered_again();
    test_the_mode_is_repeated_until_answered();
    test_a_refusal_of_the_mode_counts_as_an_answer();
    test_bluetooth_names_are_set_once_after_the_first_answer();
    test_status_is_polled_every_200_ms_and_kept();
    test_the_status_answer_carries_the_battery_and_the_host_leds();
    test_the_queue_keeps_eight_and_replaces_the_newest();
    test_garbage_before_a_frame_is_skipped();
    test_a_frame_that_never_ends_is_given_up_after_50_ms();
    test_stop_and_sleep();
    test_idle();
    return tests_result();
}
