#include "raw_hid.h"

static uint8_t       packet[RAW_HID_SIZE];
static volatile bool pending; // a request is in `packet`; no other is accepted until it is answered
static bool          processed;

void raw_hid_reset(void)
{
    pending   = false;
    processed = false;
    raw_hid_hook_out_arm();
}

void raw_hid_out(const uint8_t *data, uint8_t length)
{
    if (pending) {
        return; // cannot happen while the endpoint is not armed; a stray one is ignored
    }

    if (length != RAW_HID_SIZE) {
        raw_hid_hook_out_arm();
        return;
    }

    for (uint8_t i = 0; i < RAW_HID_SIZE; i++) {
        packet[i] = data[i];
    }

    pending = true;
}

void raw_hid_task(void)
{
    if (!pending) {
        return;
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
