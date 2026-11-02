#include "usb_device.h"
#include "usb_desc.h"
#include <string.h>

// bmRequestType
#define USB_BUS_LOOK_MS 250u // how often the main loop looks for the host's start-of-frame
#define DIR_IN       0x80
#define TYPE_MASK    0x60
#define TYPE_STD     0x00
#define TYPE_CLASS   0x20
#define RECIP_MASK   0x1F
#define RECIP_DEVICE 0x00
#define RECIP_IFACE  0x01
#define RECIP_ENDPT  0x02

// bRequest, standard
#define REQ_GET_STATUS        0
#define REQ_CLEAR_FEATURE     1
#define REQ_SET_FEATURE       3
#define REQ_SET_ADDRESS       5
#define REQ_GET_DESCRIPTOR    6
#define REQ_GET_CONFIGURATION 8
#define REQ_SET_CONFIGURATION 9
#define REQ_GET_INTERFACE     10
#define REQ_SET_INTERFACE     11

// bRequest, HID
#define HID_SET_REPORT   0x09
#define HID_GET_IDLE     0x02
#define HID_SET_IDLE     0x0A
#define HID_GET_PROTOCOL 0x03
#define HID_SET_PROTOCOL 0x0B

#define FEATURE_ENDPOINT_HALT  0
#define FEATURE_REMOTE_WAKEUP  1

#define REPORT_TYPE_OUTPUT  2
#define REPORT_TYPE_FEATURE 3

#define ISP_COMMAND_ENTER 0x75

typedef enum {
    STAGE_IDLE,
    STAGE_DATA_IN,    // sending data to the host
    STAGE_STATUS_OUT, // waiting for the host's empty OUT that ends an IN transfer
    STAGE_DATA_OUT,   // waiting for the host's data
    STAGE_STATUS_IN,  // our empty IN packet is out, waiting for it to be taken
} stage_t;

typedef enum {
    OUT_LED,
    OUT_ISP,
} out_target_t;

typedef struct {
    uint8_t  type;
    uint8_t  request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} setup_t;

static stage_t      stage;
static out_target_t out_target;
static uint8_t      out_length;

static bool    address_pending;
static uint8_t address_value;
static bool    isp_pending; // the ISP command arrived: act on it once the status stage is done
static bool    configured;
static bool    remote_wakeup;
static uint8_t protocol[USB_INTERFACES];
static uint8_t idle_time;
static uint8_t halted; // bit 0: EP1 IN, bit 1: EP2 IN, bit 2: EP2 OUT

// What is being sent to the host
static const uint8_t *in_data;
static uint16_t       in_left;
static bool           in_fills_request; // the data is as long as the host asked: no empty packet to end it
static bool           in_last_sent;

static uint8_t small[2]; // answers of one or two bytes

void usb_device_reset(void)
{
    stage           = STAGE_IDLE;
    address_pending = false;
    address_value   = 0;
    isp_pending     = false;
    configured      = false;
    remote_wakeup   = false;
    protocol[0]     = 1; // a device starts in report protocol: the reports of interface 0 carry IDs
    protocol[1]     = 0;
    idle_time       = 0;
    halted          = 0;
}

// The host is gone when nothing drives the bus: `configured` is set by SET_CONFIGURATION and cleared only by another one or by a bus reset,
// neither of which happens when the cable is simply pulled out. Without this the keyboard believed for ever that it was plugged in - and the
// battery log, which writes only while on battery, wrote nothing at all overnight.
static bool     bus_gone;
static uint8_t  last_sof;
static uint16_t last_sof_look;

void usb_device_bus_check(uint16_t now)
{
    if ((uint16_t)(now - last_sof_look) < USB_BUS_LOOK_MS) {
        return;
    }

    const uint8_t ticks = usb_hook_sof_ticks();

    bus_gone = (ticks == last_sof); // no start-of-frame since the last look: no host
    last_sof = ticks;
    last_sof_look = now;
}

bool usb_device_configured(void)
{
    return configured && !bus_gone;
}

bool usb_device_remote_wakeup(void)
{
    return remote_wakeup;
}

uint8_t usb_device_protocol(uint8_t interface)
{
    return interface < USB_INTERFACES ? protocol[interface] : 0;
}

static uint8_t halt_bit(uint8_t endpoint)
{
    switch (endpoint) {
        case 0x81: return 1u << 0;
        case 0x82: return 1u << 1;
        case 0x02: return 1u << 2;
        default:   return 0;
    }
}

bool usb_device_ep_halted(uint8_t endpoint)
{
    return (halted & halt_bit(endpoint)) != 0;
}

static void stall(void)
{
    stage = STAGE_IDLE;
    usb_hook_ep0_stall();
    usb_hook_ep0_out_arm();
}

// The status stage of a request with no data, or after OUT data
static void ack(void)
{
    usb_hook_ep0_out_arm();
    stage = STAGE_STATUS_IN;
    usb_hook_ep0_in(0, 0);
}

static void send_next(void)
{
    if (in_last_sent) {
        stage = STAGE_STATUS_OUT;
        return;
    }

    const uint8_t n = (in_left > USB_EP0_SIZE) ? USB_EP0_SIZE : (uint8_t)in_left;

    usb_hook_ep0_in(in_data, n);
    in_data += n;
    in_left -= n;

    // A short packet ends the transfer. So does a full one when the data is as long as the host asked for; if it is shorter
    // and a multiple of the packet size, an empty packet follows.
    in_last_sent = (n < USB_EP0_SIZE) || (in_left == 0 && in_fills_request);
}

// Sends `length` bytes, but no more than the host asked for
static void send(const uint8_t *data, uint16_t length, uint16_t asked)
{
    if (asked == 0) {
        ack();
        return;
    }

    if (length > asked) {
        length = asked;
    }

    in_data          = data;
    in_left          = length;
    in_fills_request = (length == asked);
    in_last_sent     = false;
    stage            = STAGE_DATA_IN;

    usb_hook_ep0_out_arm();
    send_next();
}

static void receive(out_target_t target, uint16_t length)
{
    if (length == 0 || length > USB_EP0_SIZE) {
        stall();
        return;
    }

    out_target = target;
    out_length = (uint8_t)length;
    stage      = STAGE_DATA_OUT;
    usb_hook_ep0_out_arm();
}

static void get_descriptor(const setup_t *req)
{
    const uint8_t *data;
    uint16_t       length;

    if (!usb_desc_get((uint8_t)(req->value >> 8), (uint8_t)(req->value & 0xFF), req->index, &data, &length)) {
        stall();
        return;
    }

    send(data, length, req->length);
}

static void get_status(const setup_t *req)
{
    small[0] = 0;
    small[1] = 0;

    switch (req->type & RECIP_MASK) {
        case RECIP_DEVICE:
            small[0] = remote_wakeup ? 0x02 : 0;
            break;

        case RECIP_IFACE:
            if (!configured || req->index >= USB_INTERFACES) {
                stall();
                return;
            }
            break;

        case RECIP_ENDPT:
            if (req->index == 0x00 || req->index == 0x80) {
                break;
            }

            if (!configured || !halt_bit((uint8_t)req->index)) {
                stall();
                return;
            }
            small[0] = usb_device_ep_halted((uint8_t)req->index) ? 1 : 0;
            break;

        default:
            stall();
            return;
    }

    send(small, 2, req->length);
}

static void set_feature(const setup_t *req, bool set)
{
    switch (req->type & RECIP_MASK) {
        case RECIP_DEVICE:
            if (req->value == FEATURE_REMOTE_WAKEUP) {
                remote_wakeup = set;
                ack();
                return;
            }
            break;

        case RECIP_ENDPT:
            if (req->value != FEATURE_ENDPOINT_HALT) {
                break;
            }

            if (req->index == 0x00 || req->index == 0x80) {
                ack();
                return;
            }

            if (configured && halt_bit((uint8_t)req->index)) {
                const uint8_t bit = halt_bit((uint8_t)req->index);

                halted = set ? (uint8_t)(halted | bit) : (uint8_t)(halted & ~bit);
                usb_hook_ep_halt((uint8_t)req->index, set);
                ack();
                return;
            }
            break;

        default:
            break;
    }

    stall();
}

static void set_configuration(const setup_t *req)
{
    // an address must have been assigned first
    if (address_value == 0 || req->value > 1) {
        stall();
        return;
    }

    configured = (req->value != 0);
    halted     = 0;
    usb_hook_configured(configured);
    ack();
}

static void standard_request(const setup_t *req)
{
    const bool in = (req->type & DIR_IN) != 0;

    switch (req->request) {
        case REQ_GET_STATUS:
            if (in) {
                get_status(req);
                return;
            }
            break;

        case REQ_CLEAR_FEATURE:
        case REQ_SET_FEATURE:
            if (!in) {
                set_feature(req, req->request == REQ_SET_FEATURE);
                return;
            }
            break;

        case REQ_SET_ADDRESS:
            if (!in && (req->type & RECIP_MASK) == RECIP_DEVICE && req->value <= 127) {
                address_value   = (uint8_t)req->value;
                address_pending = true;
                ack();
                return;
            }
            break;

        case REQ_GET_DESCRIPTOR:
            if (in) {
                get_descriptor(req);
                return;
            }
            break;

        case REQ_GET_CONFIGURATION:
            if (in && (req->type & RECIP_MASK) == RECIP_DEVICE) {
                small[0] = configured ? 1 : 0;
                send(small, 1, req->length);
                return;
            }
            break;

        case REQ_SET_CONFIGURATION:
            if (!in && (req->type & RECIP_MASK) == RECIP_DEVICE) {
                set_configuration(req);
                return;
            }
            break;

        case REQ_GET_INTERFACE:
            if (in && (req->type & RECIP_MASK) == RECIP_IFACE && configured && req->index < USB_INTERFACES) {
                small[0] = 0;
                send(small, 1, req->length);
                return;
            }
            break;

        case REQ_SET_INTERFACE:
            if (!in && (req->type & RECIP_MASK) == RECIP_IFACE && configured && req->index < USB_INTERFACES && req->value == 0) {
                ack();
                return;
            }
            break;

        default:
            break;
    }

    stall();
}

static void set_report(const setup_t *req)
{
    const uint8_t report_type = (uint8_t)(req->value >> 8);
    const uint8_t report_id   = (uint8_t)(req->value & 0xFF);

    if (req->index == 0) { // everything but the raw HID pipe is on interface 0
        // the LED report: one byte, or two with the report ID in front
        if (report_type == REPORT_TYPE_OUTPUT && (req->length == 1 || req->length == 2)) {
            receive(OUT_LED, req->length);
            return;
        }

        if (report_type == REPORT_TYPE_FEATURE && report_id == REPORT_ID_ISP) {
            receive(OUT_ISP, req->length);
            return;
        }
    }

    stall();
}

static void hid_request(const setup_t *req)
{
    const bool in = (req->type & DIR_IN) != 0;

    if ((req->type & RECIP_MASK) != RECIP_IFACE || req->index >= USB_INTERFACES) {
        stall();
        return;
    }

    switch (req->request) {
        case HID_SET_REPORT:
            if (!in) {
                set_report(req);
                return;
            }
            break;

        case HID_SET_IDLE:
            if (!in) {
                idle_time = (uint8_t)(req->value >> 8);
                ack();
                return;
            }
            break;

        case HID_GET_IDLE:
            if (in) {
                small[0] = idle_time;
                send(small, 1, req->length);
                return;
            }
            break;

        case HID_SET_PROTOCOL:
            if (!in && req->value <= 1) {
                protocol[req->index] = (uint8_t)req->value;
                ack();
                return;
            }
            break;

        case HID_GET_PROTOCOL:
            if (in) {
                small[0] = protocol[req->index];
                send(small, 1, req->length);
                return;
            }
            break;

        default:
            break;
    }

    stall();
}

void usb_device_ep0_in_done(void)
{
    if (stage == STAGE_DATA_IN) {
        send_next();
        return;
    }

    if (stage != STAGE_STATUS_IN) {
        return;
    }

    stage = STAGE_IDLE;

    if (address_pending) {
        address_pending = false;
        usb_hook_set_address(address_value);
    }

    if (isp_pending) {
        isp_pending = false;
        usb_hook_isp_request();
    }
}

void usb_device_setup(const uint8_t packet[USB_SETUP_SIZE], bool previous_status_done)
{
    setup_t req;

    if (previous_status_done && stage == STAGE_STATUS_IN) {
        usb_device_ep0_in_done();
    }

    stage           = STAGE_IDLE;
    address_pending = false;
    isp_pending     = false;

    req.type    = packet[0];
    req.request = packet[1];
    req.value   = (uint16_t)(packet[2] | ((uint16_t)packet[3] << 8));
    req.index   = (uint16_t)(packet[4] | ((uint16_t)packet[5] << 8));
    req.length  = (uint16_t)(packet[6] | ((uint16_t)packet[7] << 8));

    switch (req.type & TYPE_MASK) {
        case TYPE_STD:
            standard_request(&req);
            break;

        case TYPE_CLASS:
            hid_request(&req);
            break;

        default:
            stall();
            break;
    }
}

void usb_device_ep0_out(const uint8_t *data, uint8_t len)
{
    usb_hook_ep0_out_arm();

    if (stage == STAGE_DATA_OUT) {
        if (len != out_length) {
            stall();
            return;
        }

        if (out_target == OUT_LED) {
            usb_hook_led(data[out_length - 1]); // the LED byte is the last one
        } else if (data[0] == REPORT_ID_ISP && out_length >= 2 && data[1] == ISP_COMMAND_ENTER) {
            isp_pending = true;
        }

        ack();
    } else if (stage == STAGE_DATA_IN || stage == STAGE_STATUS_OUT) {
        stage = STAGE_IDLE;
    }
}
