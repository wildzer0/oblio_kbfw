#include "diag.h"
#include <string.h>

#define DIAG_RADIO_INFO_LEN 22

static uint16_t counter[DIAG_COUNT];

void diag_init(void)
{
    memset(counter, 0, sizeof counter);
}

void diag_set(diag_id_t id, uint16_t value)
{
    counter[id] = value;
}

void diag_inc(diag_id_t id)
{
    if (counter[id] < 0xFFFFu) {
        counter[id]++;
    }
}

void diag_max(diag_id_t id, uint16_t value)
{
    if (value > counter[id]) {
        counter[id] = value;
    }
}

uint16_t diag_get(diag_id_t id)
{
    uint16_t a;
    uint16_t b;

    do { // a 16-bit value is not read atomically: two readings until they agree
        a = counter[id];
        b = counter[id];
    } while (a != b);

    return a;
}

void diag_receive(uint8_t *data, uint8_t length)
{
    const uint8_t kind = data[1];

    if (kind == 0) {
        diag_hook_refresh();
        memset(&data[2], 0, (size_t)(length - 2));
        data[2] = DIAG_COUNT;

        for (uint8_t i = 0; i < DIAG_COUNT; i++) {
            const uint16_t v = diag_get((diag_id_t)i);

            data[3 + 2 * i] = (uint8_t)(v & 0xFF);
            data[4 + 2 * i] = (uint8_t)(v >> 8);
        }
    } else if (kind == 1) {
        memset(&data[2], 0, (size_t)(length - 2));
        memcpy(&data[2], diag_hook_radio_status(), 6);
        memcpy(&data[8], diag_hook_radio_info(), DIAG_RADIO_INFO_LEN);
    } else {
        data[0] = 0xFF;
    }
}
