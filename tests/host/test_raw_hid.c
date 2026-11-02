#include "check.h"
#include "raw_hid.h"
#include <string.h>

static bool     ep2_free = true;
static unsigned arms;
static unsigned sends;
static uint8_t  sent[RAW_HID_SIZE];
static unsigned processed;

static uint8_t ep_out[RAW_HID_SIZE]; // the endpoint's buffer

bool raw_hid_hook_ep2_free(void) { return ep2_free; }
void raw_hid_hook_out_read(XDATA uint8_t *dst) { memcpy(dst, ep_out, RAW_HID_SIZE); }
void raw_hid_hook_out_arm(void) { arms++; }

void raw_hid_hook_ep2_send(const uint8_t *data, uint8_t length)
{
    CHECK(length == RAW_HID_SIZE);
    memcpy(sent, data, length);
    sends++;
}

// The application: the reply is the request with every byte plus one
void raw_hid_hook_process(XDATA uint8_t *packet)
{
    processed++;

    for (unsigned i = 0; i < RAW_HID_SIZE; i++) {
        packet[i]++;
    }
}

static void reset(void)
{
    ep2_free  = true;
    arms      = 0;
    sends     = 0;
    processed = 0;
    memset(sent, 0, sizeof sent);
    raw_hid_reset();
    arms = 0; // the arm of the reset itself is not what the tests count
}

static void request(uint8_t first)
{
    for (unsigned i = 0; i < RAW_HID_SIZE; i++) {
        ep_out[i] = (uint8_t)(first + i);
    }

    raw_hid_out(RAW_HID_SIZE);
}

static void test_request_and_reply(void)
{
    reset();
    request(10);
    CHECK(sends == 0 && arms == 0); // taken in, not answered yet, and the endpoint is not armed
    raw_hid_task();
    CHECK(processed == 1 && sends == 1 && arms == 1);
    CHECK(sent[0] == 11 && sent[31] == 42);
    raw_hid_task();
    CHECK(sends == 1); // nothing more to do
}

static void test_reply_waits_for_the_endpoint(void)
{
    reset();
    request(1);
    ep2_free = false;
    raw_hid_task();
    raw_hid_task();
    CHECK(sends == 0 && arms == 0);
    CHECK(processed == 1); // the request is handled once, not once per pass
    ep2_free = true;
    raw_hid_task();
    CHECK(sends == 1 && arms == 1 && processed == 1);
}

static void test_no_second_request_while_one_is_in_flight(void)
{
    reset();
    request(1);
    raw_hid_out(RAW_HID_SIZE); // a stray one (the endpoint is not armed: it cannot happen) is ignored
    raw_hid_task();
    CHECK(sends == 1 && sent[0] == 2);
}

static void test_a_packet_of_another_size_is_dropped(void)
{
    reset();
    raw_hid_out(16);
    CHECK(arms == 1); // taken the next one
    raw_hid_task();
    CHECK(processed == 0 && sends == 0);

    raw_hid_out(64);
    CHECK(arms == 2 && processed == 0);
}

static void test_reset_drops_a_request_in_flight(void)
{
    reset();
    request(1);
    raw_hid_reset();
    CHECK(arms == 1);
    raw_hid_task();
    CHECK(processed == 0 && sends == 0);

    request(5); // and a new one is accepted
    raw_hid_task();
    CHECK(sends == 1 && sent[0] == 6);
}

int main(void)
{
    test_request_and_reply();
    test_reply_waits_for_the_endpoint();
    test_no_second_request_while_one_is_in_flight();
    test_a_packet_of_another_size_is_dropped();
    test_reset_drops_a_request_in_flight();
    return tests_result();
}
