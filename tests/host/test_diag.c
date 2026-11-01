#include "check.h"
#include "blog.h"
#include "diag.h"
#include <string.h>

static unsigned refreshes;
static uint8_t  status[22] = {1, 2, 3, 4, 5, 6};
static uint8_t  info[22];

void diag_hook_refresh(void)
{
    refreshes++;
    diag_set(DIAG_STACK_USED, 77); // the board brings the counters it reads elsewhere up to date
}

const uint8_t *diag_hook_radio_status(void) { return status; }
const uint8_t *diag_hook_radio_info(void) { return info; }

static void test_counters(void)
{
    diag_init();
    CHECK(diag_get(DIAG_SLOT_LATE) == 0);
    diag_inc(DIAG_SLOT_LATE);
    diag_inc(DIAG_SLOT_LATE);
    CHECK(diag_get(DIAG_SLOT_LATE) == 2);

    diag_set(DIAG_RESET_CAUSE, 0x0A);
    CHECK(diag_get(DIAG_RESET_CAUSE) == 0x0A);

    diag_max(DIAG_LOOP_MAX_MS, 5);
    diag_max(DIAG_LOOP_MAX_MS, 3);
    diag_max(DIAG_LOOP_MAX_MS, 9);
    CHECK(diag_get(DIAG_LOOP_MAX_MS) == 9);

    diag_set(DIAG_SCAN_LOST, 0xFFFE);
    diag_inc(DIAG_SCAN_LOST);
    diag_inc(DIAG_SCAN_LOST); // saturates, does not wrap to 0
    CHECK(diag_get(DIAG_SCAN_LOST) == 0xFFFF);

    diag_init();
    CHECK(diag_get(DIAG_SLOT_LATE) == 0 && diag_get(DIAG_SCAN_LOST) == 0);
}

static void test_counters_reply(void)
{
    uint8_t data[32];

    diag_init();
    diag_set(DIAG_RESET_CAUSE, 0x0102);
    diag_inc(DIAG_SLOT_LATE);
    memset(data, 0xAA, sizeof data);
    data[0] = 0xF1;
    data[1] = 0;

    refreshes = 0;
    diag_receive(data, 32);

    CHECK(refreshes == 1);
    CHECK(data[0] == 0xF1 && data[1] == 0 && data[2] == DIAG_COUNT);
    CHECK(data[3 + 2 * DIAG_RESET_CAUSE] == 0x02 && data[4 + 2 * DIAG_RESET_CAUSE] == 0x01); // little-endian
    CHECK(data[3 + 2 * DIAG_STACK_USED] == 77); // refreshed before it was read
    CHECK(data[3 + 2 * DIAG_SLOT_LATE] == 1);
    CHECK(data[3 + 2 * DIAG_COUNT] == 0 && data[31] == 0); // the rest of the packet is cleared
    CHECK(3 + 2 * DIAG_COUNT <= 32); // all the counters fit in one packet
}

static void test_radio_reply(void)
{
    uint8_t data[32];

    memset(info, 0, sizeof info);
    info[0] = 0x08;
    info[21] = 0x99;
    memset(data, 0, sizeof data);
    data[0] = 0xF1;
    data[1] = 1;
    diag_receive(data, 32);

    CHECK(data[0] == 0xF1 && data[1] == 1);
    CHECK(data[2] == 1 && data[7] == 6); // the six status bytes
    CHECK(data[8] == 0x08 && data[29] == 0x99); // the 22 bytes of the last 08 frame
    CHECK(data[30] == 0x03 && data[31] == 0x01); // and the status as last seen on battery

    // kind 4: the battery log, six records of four bytes from the index asked for
    memset(data, 0, sizeof data);
    data[0] = 0xF1;
    data[1] = 4;
    diag_receive(data, 32);
    CHECK(data[2] == blog_count());
    CHECK(data[2] == 0); // nothing has been logged in this test
}

// core/blog.c asks the radio for its last answer on battery: there is no radio in this test, and no log either
const uint8_t *radio_status_on_battery(void) { return 0; }
bool radio_hook_on_battery(void) { return false; }

void diag_hook_radio_on_battery(uint8_t *battery, uint8_t *flags)
{
    *battery = 0x03;
    *flags   = 0x01;
}

static bool monitoring;

void diag_hook_radio_monitor(bool on) { monitoring = on; }

void diag_hook_ports(uint8_t *out)
{
    for (uint8_t i = 0; i < DIAG_PORT_BYTES; i++) {
        out[i] = (uint8_t)(0x10 + i);
    }

}

static void test_ports_reply(void)
{
    uint8_t data[32];

    memset(data, 0xAA, sizeof data);
    data[0] = 0xF1;
    data[1] = 3;
    diag_receive(data, 32);

    CHECK(data[0] == 0xF1 && data[1] == 3);
    CHECK(data[2] == 0x10 && data[9] == 0x17);                   // P0 .. P7
    CHECK(data[10] == 0);                                        // the rest of the packet is cleared
}

static void test_radio_monitor_switch(void)
{
    uint8_t data[32] = {0xF1, 2, 1};

    diag_receive(data, 32);
    CHECK(monitoring && data[0] == 0xF1 && data[1] == 2 && data[2] == 1);

    uint8_t off[32] = {0xF1, 2, 0};

    diag_receive(off, 32);
    CHECK(!monitoring && off[2] == 0);

    uint8_t odd[32] = {0xF1, 2, 7};

    diag_receive(odd, 32);
    CHECK(monitoring && odd[2] == 1);
}

static void test_unknown_kind_is_unhandled(void)
{
    uint8_t data[32] = {0xF1, 9};

    diag_receive(data, 32);
    CHECK(data[0] == 0xFF);
}

int main(void)
{
    test_counters();
    test_counters_reply();
    test_radio_reply();
    test_radio_monitor_switch();
    test_ports_reply();
    test_unknown_kind_is_unhandled();
    return tests_result();
}
