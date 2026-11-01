#include "usb_reports.h"
#include "usb_desc.h"
#include <string.h>

#define PACKET_MAX (1 + REPORT_BOOT_SIZE)

typedef struct {
    report_kind_t kind;
    uint8_t       length;
    uint8_t       data[PACKET_MAX];
} packet_t;

static packet_t queue[USB_REPORT_QUEUE];
static uint8_t  head; // where the next packet goes
static uint8_t  tail; // the oldest packet
static uint8_t  overflows;

void usb_reports_init(void)
{
    head      = 0;
    tail      = 0;
    overflows = 0;
}

void usb_reports_flush(void)
{
    head = 0;
    tail = 0;
}

uint8_t usb_reports_overflows(void)
{
    return overflows;
}

static uint8_t next(uint8_t index)
{
    return (uint8_t)((index + 1u) % USB_REPORT_QUEUE);
}

static uint8_t build(packet_t *packet, report_kind_t kind)
{
    packet->kind = kind;

    switch (kind) {
        case REPORT_KEYBOARD:
            if (usb_reports_hook_protocol() == 0) { // boot protocol: no report ID
                report_build_boot(packet->data);
                return REPORT_BOOT_SIZE;
            }

            packet->data[0] = REPORT_ID_KEYBOARD;
            report_build_boot(&packet->data[1]);
            return 1 + REPORT_BOOT_SIZE;

        case REPORT_SYSTEM:
            if (usb_reports_hook_protocol() == 0) {
                return 0;
            }

            packet->data[0] = REPORT_ID_SYSTEM;
            packet->data[1] = (uint8_t)(report_system() & 0x07);
            return 2;

        case REPORT_CONSUMER:
            if (usb_reports_hook_protocol() == 0) {
                return 0;
            }

            packet->data[0] = REPORT_ID_CONSUMER;
            packet->data[1] = (uint8_t)(report_consumer() & 0xFF);
            packet->data[2] = (uint8_t)(report_consumer() >> 8);
            return 3;

        case REPORT_MOUSE:
            if (usb_reports_hook_protocol() == 0) {
                return 0; // a BIOS has no pointing device
            }

            packet->data[0] = REPORT_ID_MOUSE;
            report_build_mouse(&packet->data[1]); // and the movement is used up by this packet
            return 1 + REPORT_MOUSE_SIZE;
    }

    return 0;
}

void usb_reports_send(report_kind_t kind)
{
    packet_t packet;

    if (!usb_reports_hook_configured()) {
        return;
    }

    packet.length = build(&packet, kind);

    if (packet.length == 0) {
        return;
    }

    if (next(head) == tail) {
        // Full. Reports are whole states, so the newest of the same kind can stand in for the last one queued; with none of
        // that kind, the oldest packet goes.
        overflows++;

        for (uint8_t i = head; i != tail; i = (uint8_t)((i + USB_REPORT_QUEUE - 1u) % USB_REPORT_QUEUE)) {
            const uint8_t at = (uint8_t)((i + USB_REPORT_QUEUE - 1u) % USB_REPORT_QUEUE);

            if (queue[at].kind == kind) {
                queue[at] = packet;
                return;
            }
        }

        tail = next(tail);
    }

    queue[head] = packet;
    head        = next(head);
}

void usb_reports_pump(void)
{
    if (head == tail || !usb_reports_hook_ep1_free()) {
        return;
    }

    usb_reports_hook_ep1_send(queue[tail].data, queue[tail].length);
    tail = next(tail);
}
