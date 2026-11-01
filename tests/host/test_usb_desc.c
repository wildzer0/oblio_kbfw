#include "check.h"
#include "usb_desc.h"
#include "report.h"
#include <string.h>

static const uint8_t *get(uint8_t type, uint8_t index, uint16_t wIndex, uint16_t *length)
{
    const uint8_t *data = 0;

    if (!usb_desc_get(type, index, wIndex, &data, length)) {
        return 0;
    }

    return data;
}

// ---- a small HID report descriptor reader: the size of every report, by kind and ID ----
enum { KIND_INPUT, KIND_OUTPUT, KIND_FEATURE, KINDS };

typedef struct {
    unsigned bits[KINDS][8]; // by report ID (0..7)
    bool     valid;
    int      depth;
} parsed_t;

static parsed_t parse(const uint8_t *d, uint16_t length)
{
    parsed_t p;
    unsigned size = 0, count = 0, id = 0;

    memset(&p, 0, sizeof p);
    p.valid = true;

    for (uint16_t i = 0; i < length;) {
        const uint8_t prefix = d[i];
        const unsigned bytes = (prefix & 3) == 3 ? 4 : (prefix & 3);
        const unsigned type  = (prefix >> 2) & 3;
        const unsigned tag   = prefix >> 4;
        unsigned value       = 0;

        if (i + 1 + bytes > length) {
            p.valid = false;
            break;
        }

        for (unsigned b = 0; b < bytes; b++) {
            value |= (unsigned)d[i + 1 + b] << (8 * b);
        }

        if (type == 0) { // main
            if (tag == 0xA) p.depth++;
            if (tag == 0xC) p.depth--;
            if (tag == 0x8 || tag == 0x9 || tag == 0xB) {
                const unsigned kind = tag == 0x8 ? KIND_INPUT : tag == 0x9 ? KIND_OUTPUT : KIND_FEATURE;
                p.bits[kind][id] += size * count;
            }
        } else if (type == 1) { // global
            if (tag == 7) size = value;
            if (tag == 9) count = value;
            if (tag == 8) id = value;
        }

        i = (uint16_t)(i + 1 + bytes);
    }

    if (p.depth != 0) {
        p.valid = false;
    }

    return p;
}

static void test_device_descriptor(void)
{
    uint16_t length;
    const uint8_t *d = get(USB_DESC_DEVICE, 0, 0, &length);

    CHECK(d && length == 18 && d[0] == 18 && d[1] == 1);
    CHECK(d[7] == USB_EP0_SIZE);
    CHECK((d[8] | (d[9] << 8)) == 0x258A && (d[10] | (d[11] << 8)) == 0xDEAD);
    CHECK((d[12] | (d[13] << 8)) == USB_BCD_DEVICE);
    CHECK(d[17] == 1);
}

static void test_configuration_is_consistent(void)
{
    uint16_t length;
    const uint8_t *c = get(USB_DESC_CONFIGURATION, 0, 0, &length);

    CHECK(c && c[0] == 9 && c[1] == 2);
    CHECK((c[2] | (c[3] << 8)) == length); // wTotalLength
    CHECK(c[4] == USB_INTERFACES);

    unsigned interfaces = 0, endpoints = 0, hids = 0, at = c[0];
    unsigned ep_addr[4] = {0};

    while (at < length) {
        CHECK(c[at] >= 2);
        const uint8_t type = c[at + 1];

        if (type == 4) {
            CHECK(c[at + 2] == interfaces); // numbered in order
            interfaces++;
        } else if (type == 5) {
            ep_addr[endpoints++] = c[at + 2];
            CHECK(c[at + 3] == 3); // interrupt
        } else if (type == 0x21) {
            uint16_t rlen;
            const uint8_t *r = get(USB_DESC_HID_REPORT, 0, hids, &rlen);

            CHECK(r != 0);
            CHECK((c[at + 7] | (c[at + 8] << 8)) == rlen); // the report descriptor is as long as the HID descriptor says
            hids++;
        }

        at += c[at];
    }

    CHECK(at == length);
    CHECK(interfaces == USB_INTERFACES && hids == 2 && endpoints == 3);
    CHECK(ep_addr[0] == 0x81 && ep_addr[1] == 0x82 && ep_addr[2] == 0x02);
}

static void test_strings(void)
{
    uint16_t length;
    const uint8_t *s = get(USB_DESC_STRING, 0, 0, &length);

    CHECK(s && length == 4 && s[2] == 0x09 && s[3] == 0x04);

    for (uint8_t i = 1; i <= 3; i++) {
        s = get(USB_DESC_STRING, i, 0x0409, &length);
        CHECK(s && s[0] == length && s[1] == 3 && length % 2 == 0 && length > 2);
    }

    s = get(USB_DESC_STRING, 3, 0, &length); // Vial finds the keyboard by this
    CHECK(s && length == 2 + 2 * 13 && s[2] == 'v' && s[4] == 'i' && s[3] == 0);
    CHECK(!get(USB_DESC_STRING, 4, 0, &length));
}

static void test_unknown_descriptors(void)
{
    uint16_t length;

    CHECK(!get(0x06, 0, 0, &length));                // device qualifier
    CHECK(!get(USB_DESC_CONFIGURATION, 1, 0, &length));
    CHECK(!get(USB_DESC_HID, 0, 2, &length));
    CHECK(!get(USB_DESC_HID_REPORT, 0, 2, &length));
}

static void test_report_descriptors_match_the_reports(void)
{
    uint16_t length;
    const uint8_t *r0 = get(USB_DESC_HID_REPORT, 0, 0, &length);
    parsed_t p        = parse(r0, length);

    CHECK(p.valid);
    CHECK(p.bits[KIND_INPUT][REPORT_ID_KEYBOARD] == 8 * REPORT_BOOT_SIZE); // modifiers, reserved, 6 keys
    CHECK(p.bits[KIND_OUTPUT][REPORT_ID_KEYBOARD] == 8);                   // the LEDs
    CHECK(p.bits[KIND_INPUT][REPORT_ID_SYSTEM] == 8);
    CHECK(p.bits[KIND_INPUT][REPORT_ID_CONSUMER] == 16);
    CHECK(p.bits[KIND_FEATURE][REPORT_ID_ISP] == 40);
    CHECK(8 * (1 + REPORT_BOOT_SIZE) <= 8 * USB_EP1_SIZE); // the largest report with its ID fits EP1

    const uint8_t *r1 = get(USB_DESC_HID_REPORT, 0, 1, &length);
    p                 = parse(r1, length);

    CHECK(p.valid);
    CHECK(p.bits[KIND_INPUT][0] == 8 * RAW_HID_SIZE);
    CHECK(p.bits[KIND_OUTPUT][0] == 8 * RAW_HID_SIZE);
    CHECK(p.bits[KIND_FEATURE][0] == 0);
}

int main(void)
{
    test_device_descriptor();
    test_configuration_is_consistent();
    test_strings();
    test_unknown_descriptors();
    test_report_descriptors_match_the_reports();
    return tests_result();
}
