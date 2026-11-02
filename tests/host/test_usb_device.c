#include "check.h"
#include "usb_desc.h"
#include "../../core/usb_device.c" // the .c itself: the test reaches its static send()
#include <string.h>

// ---- the chip, as the device sees it ----
static bool    in_pending;
static uint8_t in_packet[USB_EP0_SIZE];
static uint8_t in_length;
static bool    stalled;
static bool    out_armed;
static int     address = -1;
static int     configured_calls;
static bool    configured_last;
static int     led = -1;
static int     isp_requests;
static int     halt_calls;
static uint8_t halt_ep;
static bool    halt_state;

void usb_hook_ep0_in(const uint8_t *data, uint8_t len)
{
    in_pending = true;
    in_length  = len;
    if (len) {
        memcpy(in_packet, data, len);
    }
}

void usb_hook_ep0_out_arm(void) { out_armed = true; }
void usb_hook_ep0_stall(void) { stalled = true; }
void usb_hook_set_address(uint8_t a) { address = a; }

// The bus: a counter of start-of-frame interrupts, which this test decides whether to move
static uint8_t sof_ticks;

uint8_t usb_hook_sof_ticks(void) { return sof_ticks; }

void usb_hook_configured(bool c)
{
    configured_calls++;
    configured_last = c;
}

void usb_hook_ep_halt(uint8_t ep, bool halted)
{
    halt_calls++;
    halt_ep    = ep;
    halt_state = halted;
}

void usb_hook_led(uint8_t l) { led = l; }
void usb_hook_isp_request(void) { isp_requests++; }

// ---- the host ----
static void setup_packet(uint8_t out[8], uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length)
{
    out[0] = type;
    out[1] = request;
    out[2] = (uint8_t)value;
    out[3] = (uint8_t)(value >> 8);
    out[4] = (uint8_t)index;
    out[5] = (uint8_t)(index >> 8);
    out[6] = (uint8_t)length;
    out[7] = (uint8_t)(length >> 8);
}

static uint8_t data_in[2048];
static unsigned data_in_length;
static unsigned packets;

// A control transfer with data to the host. True if the device answered; false if it stalled.
static bool control_in(uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length)
{
    uint8_t setup[8];

    stalled        = false;
    in_pending     = false;
    data_in_length = 0;
    packets        = 0;
    setup_packet(setup, type, request, value, index, length);
    usb_device_setup(setup, false);

    while (in_pending && !stalled) {
        in_pending = false;
        packets++;
        memcpy(&data_in[data_in_length], in_packet, in_length);
        data_in_length += in_length;
        usb_device_ep0_in_done(); // the host took it: the next packet, or the end of the data
    }

    if (stalled) {
        return false;
    }

    usb_device_ep0_out(0, 0); // the host's status stage
    return true;
}

// A control transfer with no data. True if the status stage came; false if stalled.
static bool control_nodata(uint8_t type, uint8_t request, uint16_t value, uint16_t index)
{
    uint8_t setup[8];

    stalled    = false;
    in_pending = false;
    setup_packet(setup, type, request, value, index, 0);
    usb_device_setup(setup, false);

    if (stalled || !in_pending || in_length != 0) {
        return false;
    }

    in_pending = false;
    usb_device_ep0_in_done();
    return true;
}

// A control transfer with data from the host (one packet)
static bool control_out(uint8_t type, uint8_t request, uint16_t value, uint16_t index, const uint8_t *data, uint8_t length, uint16_t w_length)
{
    uint8_t setup[8];

    stalled    = false;
    in_pending = false;
    setup_packet(setup, type, request, value, index, w_length);
    usb_device_setup(setup, false);

    if (stalled) {
        return false;
    }

    usb_device_ep0_out(data, length);

    if (stalled || !in_pending || in_length != 0) {
        return false;
    }

    in_pending = false;
    usb_device_ep0_in_done();
    return true;
}

static void bus_reset(void)
{
    usb_device_reset();
    address          = -1;
    configured_calls = 0;
    led              = -1;
    isp_requests     = 0;
    halt_calls       = 0;
}

static void enumerate(void)
{
    bus_reset();
    CHECK(control_nodata(0x00, 5, 7, 0)); // SET_ADDRESS 7
    CHECK(control_nodata(0x00, 9, 1, 0)); // SET_CONFIGURATION 1
}

static void test_enumeration_as_a_host_does_it(void)
{
    bus_reset();

    // first look at the device descriptor: 8 bytes only
    CHECK(control_in(0x80, 6, 0x0100, 0, 8));
    CHECK(data_in_length == 8 && data_in[0] == 18 && data_in[1] == 1 && packets == 1);

    CHECK(control_nodata(0x00, 5, 7, 0));
    CHECK(address == 7);

    CHECK(control_in(0x80, 6, 0x0100, 0, 18));
    CHECK(data_in_length == 18 && packets == 3); // 8 + 8 + 2

    CHECK(control_in(0x80, 6, 0x0200, 0, 9)); // the configuration header, then the whole
    CHECK(data_in_length == 9);
    const unsigned total = (unsigned)(data_in[2] | (data_in[3] << 8));
    CHECK(control_in(0x80, 6, 0x0200, 0, (uint16_t)total));
    CHECK(data_in_length == total);

    CHECK(control_in(0x80, 6, 0x0300, 0, 255)); // language ids
    CHECK(data_in_length == 4);
    CHECK(control_in(0x80, 6, 0x0302, 0x0409, 255));
    CHECK(data_in[1] == 3 && data_in[2] == 'O');

    CHECK(!usb_device_configured());
    CHECK(control_nodata(0x00, 9, 1, 0));
    CHECK(usb_device_configured() && configured_calls == 1 && configured_last);

    CHECK(control_in(0x80, 8, 0, 0, 1)); // GET_CONFIGURATION
    CHECK(data_in_length == 1 && data_in[0] == 1);
}

// The cable is pulled out: nothing tells the device, so the start-of-frame interrupts stopping is what says the host is gone. Without this the
// keyboard believed it was still plugged in - and the battery log, which writes only while on battery, wrote nothing all night.
static void test_a_host_that_goes_away_leaves_the_keyboard_unconfigured(void)
{
    bus_reset();
    sof_ticks = 0;

    usb_device_bus_check(1000); // no start-of-frame: nothing is driving the bus
    sof_ticks++;
    usb_device_bus_check(1300); // and now one has come

    CHECK(control_nodata(0x00, 5, 7, 0)); // SET_ADDRESS 7: needed before a configuration
    CHECK(control_nodata(0x00, 9, 1, 0)); // SET_CONFIGURATION 1
    CHECK(usb_device_configured());

    usb_device_bus_check(1600); // no start-of-frame since the last look: the cable is out
    CHECK(!usb_device_configured());

    sof_ticks++;
    usb_device_bus_check(1900); // plugged back in: the bus beats again
    CHECK(usb_device_configured());
}

static void test_address_waits_for_the_status_stage(void)
{
    uint8_t setup[8];

    bus_reset();
    setup_packet(setup, 0x00, 5, 9, 0, 0);
    usb_device_setup(setup, false);
    CHECK(in_pending && in_length == 0);
    CHECK(address == -1); // not yet: the status packet must go out with the old address
    usb_device_ep0_in_done();
    CHECK(address == 9);

    // a SETUP that overtakes the completion of the status stage
    bus_reset();
    setup_packet(setup, 0x00, 5, 9, 0, 0);
    usb_device_setup(setup, false);
    setup_packet(setup, 0x80, 6, 0x0100, 0, 8);
    usb_device_setup(setup, true);
    CHECK(address == 9);

    CHECK(!control_nodata(0x00, 5, 200, 0)); // not an address
}

static void test_every_descriptor_in_every_length(void)
{
    static const struct {
        uint16_t value;
        uint16_t index;
    } wanted[] = {
        {0x0100, 0}, {0x0200, 0}, {0x0300, 0}, {0x0301, 0x0409}, {0x0302, 0x0409}, {0x0303, 0x0409},
        {0x2100, 0}, {0x2100, 1}, {0x2200, 0}, {0x2200, 1},
    };

    enumerate();

    for (unsigned i = 0; i < sizeof wanted / sizeof wanted[0]; i++) {
        const uint8_t *d;
        uint16_t       length;

        CHECK(usb_desc_get((uint8_t)(wanted[i].value >> 8), (uint8_t)wanted[i].value, wanted[i].index, &d, &length));

        // asked for more than there is, for exactly that, and for less: the host gets min(asked, length) and the transfer ends
        const uint16_t asks[] = {0xFFFF, length, (uint16_t)(length - 1), 1, 8};

        for (unsigned a = 0; a < sizeof asks / sizeof asks[0]; a++) {
            const unsigned expected = asks[a] < length ? asks[a] : length;

            CHECK(control_in(0x80, 6, wanted[i].value, wanted[i].index, asks[a]));
            CHECK(data_in_length == expected);
            CHECK(memcmp(data_in, d, expected) == 0);

            // The transfer ends with a short packet. When the data is a multiple of 8 and shorter than the host asked for, that
            // packet is an empty one; when it is exactly what was asked, the host stops by itself.
            unsigned expected_packets = expected / USB_EP0_SIZE;

            if ((expected % USB_EP0_SIZE) != 0 || expected < asks[a]) {
                expected_packets++;
            }

            CHECK(packets == expected_packets);
        }
    }
}

// No descriptor is a multiple of 8 bytes long, so the empty packet that ends such a transfer is tested on send() itself
static void test_empty_packet_ends_a_transfer_of_whole_packets(void)
{
    static const uint8_t sixteen[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned             lengths[4];
    unsigned             count;

    // asked for more than there is: 8, 8, then an empty packet
    bus_reset();
    stalled    = false;
    in_pending = false;
    send(sixteen, 16, 64);
    count = 0;
    while (in_pending) {
        in_pending = false;
        lengths[count++] = in_length;
        usb_device_ep0_in_done();
    }
    CHECK(count == 3 && lengths[0] == 8 && lengths[1] == 8 && lengths[2] == 0);

    // asked for exactly that: the host stops after two packets, no empty one
    bus_reset();
    send(sixteen, 16, 16);
    count = 0;
    while (in_pending) {
        in_pending = false;
        lengths[count++] = in_length;
        usb_device_ep0_in_done();
    }
    CHECK(count == 2 && lengths[1] == 8);

    // asked for less: cut at the request, no empty packet
    bus_reset();
    send(sixteen, 16, 8);
    count = 0;
    while (in_pending) {
        in_pending = false;
        lengths[count++] = in_length;
        usb_device_ep0_in_done();
    }
    CHECK(count == 1 && lengths[0] == 8);
}

static void test_unsupported_requests_stall(void)
{
    enumerate();
    CHECK(!control_in(0x80, 6, 0x0600, 0, 10));  // device qualifier
    CHECK(!control_in(0x80, 6, 0x0304, 0x0409, 255)); // no such string
    CHECK(!control_in(0x80, 6, 0x2200, 2, 255)); // no such interface
    CHECK(!control_in(0x80, 99, 0, 0, 8));       // unknown request
    CHECK(!control_nodata(0x40, 1, 0, 0));       // vendor request
    CHECK(!control_nodata(0x21, 0x0A, 0, 5));    // class request to an interface that does not exist
    CHECK(!control_nodata(0x00, 9, 2, 0));       // no such configuration
    CHECK(!control_in(0x00, 6, 0x0100, 0, 8));   // wrong direction

    bus_reset();
    CHECK(!control_nodata(0x00, 9, 1, 0)); // configuring before an address
}

static void test_hid_protocol_and_idle(void)
{
    enumerate();

    CHECK(control_in(0xA1, 3, 0, 0, 1)); // GET_PROTOCOL: report protocol to begin with
    CHECK(data_in[0] == 1 && usb_device_protocol(0) == 1);
    CHECK(control_nodata(0x21, 0x0B, 0, 0)); // SET_PROTOCOL boot
    CHECK(usb_device_protocol(0) == 0 && usb_device_protocol(1) == 0);
    CHECK(control_in(0xA1, 3, 0, 0, 1) && data_in[0] == 0);
    CHECK(!control_nodata(0x21, 0x0B, 2, 0)); // not a protocol

    CHECK(control_nodata(0x21, 0x0A, 0x2500, 0)); // SET_IDLE 0x25
    CHECK(control_in(0xA1, 2, 0, 0, 1) && data_in[0] == 0x25);

    bus_reset(); // a bus reset puts it all back
    CHECK(usb_device_protocol(0) == 1);
}

static void test_led_report(void)
{
    const uint8_t one[]       = {0x02};
    const uint8_t with_id[]   = {REPORT_ID_KEYBOARD, 0x05};
    const uint8_t too_many[]  = {1, 2};

    enumerate();

    CHECK(control_out(0x21, 9, 0x0200, 0, one, 1, 1)); // SET_REPORT, output, no ID
    CHECK(led == 0x02);
    CHECK(control_out(0x21, 9, 0x0203, 0, with_id, 2, 2)); // in report protocol the ID comes first
    CHECK(led == 0x05);

    led = -1;
    CHECK(!control_out(0x21, 9, 0x0200, 0, one, 1, 3)); // a length that is not the LED report
    CHECK(!control_out(0x21, 9, 0x0200, 1, one, 1, 1)); // raw HID has no LEDs
    CHECK(!control_out(0x21, 9, 0x0200, 0, too_many, 2, 1)); // the host sends more than it announced
    CHECK(led == -1);
}

static void test_isp_request(void)
{
    const uint8_t enter[] = {REPORT_ID_ISP, 0x75, 0, 0, 0, 0};
    const uint8_t other[] = {REPORT_ID_ISP, 0x76, 0, 0, 0, 0};
    uint8_t       setup[8];

    enumerate();

    CHECK(control_out(0x21, 9, 0x0305, 0, other, 6, 6));
    CHECK(isp_requests == 0);

    // the request is acted on after the status stage, not before
    setup_packet(setup, 0x21, 9, 0x0305, 0, 6);
    usb_device_setup(setup, false);
    usb_device_ep0_out(enter, 6);
    CHECK(isp_requests == 0 && in_pending);
    usb_device_ep0_in_done();
    CHECK(isp_requests == 1);
    usb_device_ep0_in_done(); // only once
    CHECK(isp_requests == 1);

    // a SETUP that follows the OUT data drops a request whose status stage never happened
    setup_packet(setup, 0x21, 9, 0x0305, 0, 6);
    usb_device_setup(setup, false);
    usb_device_ep0_out(enter, 6);
    setup_packet(setup, 0x80, 6, 0x0100, 0, 8);
    usb_device_setup(setup, false);
    usb_device_ep0_in_done();
    CHECK(isp_requests == 1);

    CHECK(!control_out(0x21, 9, 0x0305, 0, enter, 6, 0)); // no data announced
    CHECK(!control_out(0x21, 9, 0x0305, 1, enter, 6, 6));  // not on the raw HID interface
    CHECK(!control_out(0x21, 9, 0x0306, 0, enter, 6, 6));  // not the ISP report id
}

static void test_status_remote_wakeup_and_halt(void)
{
    enumerate();

    CHECK(control_in(0x80, 0, 0, 0, 2) && data_in[0] == 0); // GET_STATUS device
    CHECK(control_nodata(0x00, 3, 1, 0));                   // SET_FEATURE remote wakeup
    CHECK(usb_device_remote_wakeup());
    CHECK(control_in(0x80, 0, 0, 0, 2) && data_in[0] == 2);
    CHECK(control_nodata(0x00, 1, 1, 0));
    CHECK(!usb_device_remote_wakeup());
    CHECK(!control_nodata(0x00, 3, 5, 0)); // another feature

    CHECK(control_in(0x82, 0, 0, 0x81, 2) && data_in[0] == 0); // endpoint status
    CHECK(control_nodata(0x02, 3, 0, 0x81));                   // halt EP1 IN
    CHECK(halt_calls == 1 && halt_ep == 0x81 && halt_state);
    CHECK(usb_device_ep_halted(0x81) && !usb_device_ep_halted(0x82));
    CHECK(control_in(0x82, 0, 0, 0x81, 2) && data_in[0] == 1);
    CHECK(control_nodata(0x02, 1, 0, 0x81)); // clear it
    CHECK(!usb_device_ep_halted(0x81) && !halt_state);
    CHECK(control_nodata(0x02, 3, 0, 0x02)); // EP2 OUT too
    CHECK(usb_device_ep_halted(0x02));

    CHECK(control_nodata(0x00, 9, 1, 0)); // configuring again starts the endpoints clean
    CHECK(!usb_device_ep_halted(0x02));

    CHECK(!control_nodata(0x02, 3, 0, 0x83)); // no such endpoint
    CHECK(control_in(0x82, 0, 0, 0x80, 2) && data_in[0] == 0); // EP0 is always fine
    CHECK(control_in(0x81, 0, 0, 1, 2) && data_in_length == 2); // interface status

    bus_reset();
    CHECK(!control_in(0x82, 0, 0, 0x81, 2)); // not configured: no endpoint status
}

static void test_interface_requests(void)
{
    enumerate();

    CHECK(control_in(0x81, 10, 0, 1, 1) && data_in[0] == 0); // GET_INTERFACE
    CHECK(control_nodata(0x01, 11, 0, 1));                   // SET_INTERFACE 0
    CHECK(!control_nodata(0x01, 11, 1, 1));                  // no alternate settings
    CHECK(!control_in(0x81, 10, 0, 2, 1));

    bus_reset();
    CHECK(!control_in(0x81, 10, 0, 1, 1)); // not configured
}

int main(void)
{
    test_enumeration_as_a_host_does_it();
    test_a_host_that_goes_away_leaves_the_keyboard_unconfigured();
    test_address_waits_for_the_status_stage();
    test_every_descriptor_in_every_length();
    test_empty_packet_ends_a_transfer_of_whole_packets();
    test_unsupported_requests_stall();
    test_hid_protocol_and_idle();
    test_led_report();
    test_isp_request();
    test_status_remote_wakeup_and_halt();
    test_interface_requests();
    return tests_result();
}
