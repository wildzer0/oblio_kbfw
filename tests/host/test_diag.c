#include "check.h"
#include "diag.h"
#include <string.h>

static unsigned refreshes;
static uint8_t  status[22] = {1, 2, 3, 4, 5, 6};

void diag_hook_refresh(void)
{
    refreshes++;
    diag_set(DIAG_STACK_USED, 77); // the board brings the counters it reads elsewhere up to date
}

const uint8_t *diag_hook_radio_status(void) { return status; }

#if BATTERY_LOG
static uint8_t log_records[3][4] = {{1, 0, 0x10, 0x08}, {2, 0, 0x20, 0x01}, {7, 0, 0x30, 0x02}};

uint8_t diag_hook_blog_count(void) { return 3; }

bool diag_hook_blog_get(uint8_t index, uint8_t out[4])
{
    if (index >= 3) {
        return false;
    }
    memcpy(out, log_records[index], 4);
    return true;
}
#endif

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

    diag_set(DIAG_USB_OVERFLOW, 0xFFFE);
    diag_inc(DIAG_USB_OVERFLOW);
    diag_inc(DIAG_USB_OVERFLOW); // saturates, does not wrap to 0
    CHECK(diag_get(DIAG_USB_OVERFLOW) == 0xFFFF);

    diag_init();
    CHECK(diag_get(DIAG_SLOT_LATE) == 0 && diag_get(DIAG_USB_OVERFLOW) == 0);
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

    memset(data, 0, sizeof data);
    data[0] = 0xF1;
    data[1] = 1;
    diag_receive(data, 32);

    CHECK(data[0] == 0xF1 && data[1] == 1);
    CHECK(data[2] == 1 && data[7] == 6); // the six status bytes
    CHECK(data[8] == 0); // and nothing after them
}

#if BATTERY_LOG
// kind 4: the battery log, from the record asked for
static void test_battery_log_reply(void)
{
    uint8_t data[32] = {0xF1, 4, 0};

    diag_receive(data, 32);
    CHECK(data[0] == 0xF1 && data[1] == 4 && data[2] == 3); // the count
    CHECK(data[3] == 1 && data[5] == 0x10 && data[6] == 0x08); // the first record
    CHECK(data[11] == 7 && data[13] == 0x30);                   // the third
    CHECK(data[15] == 0);                                       // and nothing after

    uint8_t from_second[32] = {0xF1, 4, 1};

    diag_receive(from_second, 32);
    CHECK(from_second[2] == 3 && from_second[3] == 2 && from_second[7] == 7); // from the second record on
}
#endif

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
#if BATTERY_LOG
    test_battery_log_reply();
#endif
    test_unknown_kind_is_unhandled();
    return tests_result();
}
