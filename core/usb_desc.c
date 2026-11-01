#include "usb_desc.h"
#include "platform.h"

// ---- HID report descriptor items (short items: prefix byte = tag | type | size) ----
#define PAGE(p)          0x05, (p)
#define PAGE16(p)        0x06, (uint8_t)((p) & 0xFF), (uint8_t)((p) >> 8)
#define USAGE(u)         0x09, (u)
#define USAGE_MIN(u)     0x19, (u)
#define USAGE_MIN16(u)   0x1A, (uint8_t)((u) & 0xFF), (uint8_t)((u) >> 8)
#define USAGE_MAX(u)     0x29, (u)
#define USAGE_MAX16(u)   0x2A, (uint8_t)((u) & 0xFF), (uint8_t)((u) >> 8)
#define LOGICAL_MIN(v)   0x15, (uint8_t)(v)
#define LOGICAL_MIN16(v) 0x16, (uint8_t)((uint16_t)(v) & 0xFFu), (uint8_t)((uint16_t)(v) >> 8)
#define LOGICAL_MAX(v)   0x25, (uint8_t)(v)
#define LOGICAL_MAX16(v) 0x26, (uint8_t)((uint16_t)(v) & 0xFFu), (uint8_t)((uint16_t)(v) >> 8)
#define REPORT_SIZE(n)   0x75, (n)
#define REPORT_COUNT(n)  0x95, (n)
#define REPORT_ID(n)     0x85, (n)
#define COLLECTION_APP      0xA1, 0x01
#define COLLECTION_PHYSICAL 0xA1, 0x00
#define END_COLLECTION   0xC0
#define INPUT(f)         0x81, (f)
#define OUTPUT(f)        0x91, (f)
#define FEATURE(f)       0xB1, (f)

#define DATA_VAR_ABS  0x02 // data, variable, absolute
#define DATA_VAR_REL  0x06 // data, variable, relative (the mouse's axes)
#define DATA_ARRAY    0x00
#define CONSTANT      0x01

// Interface 0: keyboard (ID 3), system control (ID 1), consumer control (ID 2), the ISP feature report (ID 5)
static const ROM uint8_t report_desc_keyboard[] = {
    PAGE(0x01), USAGE(0x06), COLLECTION_APP,        // Generic Desktop, Keyboard
        REPORT_ID(REPORT_ID_KEYBOARD),
        PAGE(0x07), USAGE_MIN(0xE0), USAGE_MAX(0xE7), // modifiers, one bit each
        LOGICAL_MIN(0), LOGICAL_MAX(1), REPORT_SIZE(1), REPORT_COUNT(8), INPUT(DATA_VAR_ABS),
        REPORT_SIZE(8), REPORT_COUNT(1), INPUT(CONSTANT), // reserved byte
        PAGE(0x07), USAGE_MIN(0x00), USAGE_MAX(0xFF),     // six keys
        LOGICAL_MIN(0), LOGICAL_MAX16(0xFF), REPORT_SIZE(8), REPORT_COUNT(6), INPUT(DATA_ARRAY),
        PAGE(0x08), USAGE_MIN(0x01), USAGE_MAX(0x05),     // LEDs: num, caps, scroll, compose, kana
        LOGICAL_MIN(0), LOGICAL_MAX(1), REPORT_SIZE(1), REPORT_COUNT(5), OUTPUT(DATA_VAR_ABS),
        REPORT_SIZE(3), REPORT_COUNT(1), OUTPUT(CONSTANT),
    END_COLLECTION,

    PAGE(0x01), USAGE(0x80), COLLECTION_APP,        // Generic Desktop, System Control
        REPORT_ID(REPORT_ID_SYSTEM),
        USAGE_MIN(0x81), USAGE_MAX(0x83),           // power down, sleep, wake up
        LOGICAL_MIN(0), LOGICAL_MAX(1), REPORT_SIZE(1), REPORT_COUNT(3), INPUT(DATA_VAR_ABS),
        REPORT_COUNT(5), INPUT(CONSTANT),
    END_COLLECTION,

    PAGE(0x0C), USAGE(0x01), COLLECTION_APP,        // Consumer, Consumer Control
        REPORT_ID(REPORT_ID_CONSUMER),
        USAGE_MIN(0x00), USAGE_MAX16(0x03FF), // up to KC_LPAD (0x2A0) in core/report.c: a host drops what is past this
        LOGICAL_MIN(0), LOGICAL_MAX16(0x03FF), REPORT_SIZE(16), REPORT_COUNT(1), INPUT(DATA_ARRAY),
    END_COLLECTION,

    PAGE(0x01), USAGE(0x02), COLLECTION_APP,        // Generic Desktop, Mouse (as the stock firmware's interface had it)
        REPORT_ID(REPORT_ID_MOUSE),
        USAGE(0x01), COLLECTION_PHYSICAL,           // Pointer
            PAGE(0x09), USAGE_MIN(1), USAGE_MAX(5), // five buttons
            LOGICAL_MIN(0), LOGICAL_MAX(1), REPORT_SIZE(1), REPORT_COUNT(5), INPUT(DATA_VAR_ABS),
            REPORT_SIZE(3), REPORT_COUNT(1), INPUT(CONSTANT), // padding to the byte
            PAGE(0x01), USAGE(0x30), USAGE(0x31),   // X and Y, relative
            LOGICAL_MIN16(-32767), LOGICAL_MAX16(32767),
            REPORT_SIZE(16), REPORT_COUNT(2), INPUT(DATA_VAR_REL),
            USAGE(0x38),                            // wheel
            LOGICAL_MIN(-127), LOGICAL_MAX(127),
            REPORT_SIZE(8), REPORT_COUNT(1), INPUT(DATA_VAR_REL),
        END_COLLECTION,
    END_COLLECTION,

    PAGE16(0xFF00), USAGE(0x01), COLLECTION_APP,    // vendor: the ISP command
        REPORT_ID(REPORT_ID_ISP),
        USAGE_MIN(0x01), USAGE_MAX(0x02),
        LOGICAL_MIN(0), LOGICAL_MAX16(0xFF), REPORT_SIZE(8), REPORT_COUNT(5), FEATURE(DATA_VAR_ABS),
    END_COLLECTION,
};

// Interface 1: raw HID, one 32-byte input report and one 32-byte output report, no IDs
static const ROM uint8_t report_desc_raw[] = {
    PAGE16(0xFF60), USAGE(0x61), COLLECTION_APP,
        USAGE(0x62), LOGICAL_MIN(0), LOGICAL_MAX16(0xFF), REPORT_SIZE(8), REPORT_COUNT(RAW_HID_SIZE), INPUT(DATA_VAR_ABS),
        USAGE(0x63), LOGICAL_MIN(0), LOGICAL_MAX16(0xFF), REPORT_SIZE(8), REPORT_COUNT(RAW_HID_SIZE), OUTPUT(DATA_VAR_ABS),
    END_COLLECTION,
};

static const ROM uint8_t device_desc[18] = {
    18, USB_DESC_DEVICE,
    0x00, 0x02,                                  // USB 2.0
    0, 0, 0,                                     // class, subclass, protocol: per interface
    USB_EP0_SIZE,
    USB_VID & 0xFF, USB_VID >> 8,
    USB_PID & 0xFF, USB_PID >> 8,
    USB_BCD_DEVICE & 0xFF, USB_BCD_DEVICE >> 8,
    1, 2, 3,                                     // manufacturer, product, serial number strings
    1,                                           // one configuration
};

#define CONFIG_SIZE (9 + (9 + 9 + 7) + (9 + 9 + 7 + 7))

#define HID_DESC(report) 9, USB_DESC_HID, 0x11, 0x01, 0, 1, USB_DESC_HID_REPORT, (uint8_t)(sizeof(report) & 0xFF), (uint8_t)(sizeof(report) >> 8)
#define EP_IN(n, size)   7, 5, (0x80 | (n)), 0x03, (size), 0, 1 // interrupt, 1 ms
#define EP_OUT(n, size)  7, 5, (n), 0x03, (size), 0, 1

static const ROM uint8_t config_desc[CONFIG_SIZE] = {
    9, USB_DESC_CONFIGURATION, CONFIG_SIZE & 0xFF, CONFIG_SIZE >> 8, USB_INTERFACES, 1, 0,
    0xA0, // bus powered, remote wakeup
    250,  // 500 mA

    9, 4, 0, 0, 1, 0x03, 0x01, 0x01, 0, // interface 0: HID, boot subclass, keyboard
    HID_DESC(report_desc_keyboard),
    EP_IN(1, USB_EP1_SIZE),

    9, 4, 1, 0, 2, 0x03, 0x00, 0x00, 0, // interface 1: HID, raw
    HID_DESC(report_desc_raw),
    EP_IN(2, RAW_HID_SIZE),
    EP_OUT(2, RAW_HID_SIZE),
};

static const ROM uint8_t hid_desc_0[9] = {HID_DESC(report_desc_keyboard)};
static const ROM uint8_t hid_desc_1[9] = {HID_DESC(report_desc_raw)};

static const ROM char str_manufacturer[] = "wildzer0";
static const ROM char str_product[]      = "Oblio B68";
static const ROM char str_serial[]       = "vial:f64c2b3c"; // Vial's desktop app lists a keyboard whose serial contains this

#define STRING_MAX 32 // characters of a string descriptor
static uint8_t scratch[2 + 2 * STRING_MAX];

static bool string_desc(const ROM char *text, const uint8_t **data, uint16_t *length)
{
    uint8_t n = 2;

    while (*text && n < sizeof scratch - 1) {
        scratch[n++] = (uint8_t)*text++;
        scratch[n++] = 0;
    }

    scratch[0] = n;
    scratch[1] = USB_DESC_STRING;
    *data      = scratch;
    *length    = n;
    return true;
}

bool usb_desc_get(uint8_t type, uint8_t index, uint16_t wIndex, const uint8_t **data, uint16_t *length)
{
    switch (type) {
        case USB_DESC_DEVICE:
            *data   = device_desc;
            *length = sizeof device_desc;
            return index == 0;

        case USB_DESC_CONFIGURATION:
            *data   = config_desc;
            *length = sizeof config_desc;
            return index == 0;

        case USB_DESC_STRING:
            switch (index) {
                case 0: { // the languages: English (United States)
                    static const ROM uint8_t langid[4] = {4, USB_DESC_STRING, 0x09, 0x04};

                    *data   = langid;
                    *length = sizeof langid;
                    return true;
                }
                case 1: return string_desc(str_manufacturer, data, length);
                case 2: return string_desc(str_product, data, length);
                case 3: return string_desc(str_serial, data, length);
                default: return false;
            }

        case USB_DESC_HID:
            if (wIndex == 0) {
                *data   = hid_desc_0;
                *length = sizeof hid_desc_0;
                return true;
            }
            if (wIndex == 1) {
                *data   = hid_desc_1;
                *length = sizeof hid_desc_1;
                return true;
            }
            return false;

        case USB_DESC_HID_REPORT:
            if (wIndex == 0) {
                *data   = report_desc_keyboard;
                *length = sizeof report_desc_keyboard;
                return true;
            }
            if (wIndex == 1) {
                *data   = report_desc_raw;
                *length = sizeof report_desc_raw;
                return true;
            }
            return false;

        default:
            return false;
    }
}
