#include "raw_hid.h"

static uint8_t       packet[RAW_HID_SIZE];
static volatile bool received; // a packet is in the endpoint's buffer, not read yet
static bool          pending;  // a request is in `packet`; no other is accepted until it is answered
static bool          processed;

void raw_hid_reset(void)
{
    received  = false;
    pending   = false;
    processed = false;
    raw_hid_hook_out_arm();
}

void raw_hid_out(uint8_t length)
{
    if (pending || received) {
        return; // cannot happen while the endpoint is not armed; a stray one is ignored
    }

    if (length != RAW_HID_SIZE) {
        raw_hid_hook_out_arm();
        return;
    }

    received = true;
}

void raw_hid_task(void)
{
    if (!pending) {
        if (!received) {
            return;
        }

        raw_hid_hook_out_read(packet);
        received = false;
        pending  = true;
    }

    if (!processed) {
        raw_hid_hook_process(packet);
        processed = true;
    }

    if (!raw_hid_hook_ep2_free()) {
        return; // the previous reply is still on its way: try again on the next pass
    }

    raw_hid_hook_ep2_send(packet, RAW_HID_SIZE);
    processed = false;
    pending   = false;
    raw_hid_hook_out_arm();
}
