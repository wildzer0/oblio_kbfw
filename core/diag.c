#include "diag.h"
#include "version.h"
#if LOOP_PROFILE
#    include "profile.h"
#endif
#include <string.h>

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

#if LOOP_PROFILE
static uint32_t window_started; // uptime_ms at the last start of the profile

static void put32(XDATA uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}

static void put16(XDATA uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void diag_profile(XDATA uint8_t *data, uint8_t kind)
{
    const uint8_t step = data[2];

    memset(&data[2], 0, 30);

    if (kind == 5) {
        data[2] = step;
        put16(&data[3], profile_step_count(step));
        put16(&data[5], profile_step_min(step));
        put16(&data[7], profile_step_max(step));
        put32(&data[9], profile_step_sum(step));
    } else if (kind == 6) {
        for (uint8_t i = 0; i < PROFILE_BANDS; i++) {
            put16(&data[2 + 2 * i], profile_band(i));
        }

        put32(&data[18], profile_isr_sum(PROFILE_ISR_TIMER));
        put32(&data[22], profile_isr_sum(PROFILE_ISR_USB));
        put32(&data[26], profile_isr_sum(PROFILE_ISR_UART));
    } else if (kind == 7) {
        profile_reset();
        window_started = diag_hook_uptime_ms();
    } else {
        put32(&data[2], diag_hook_uptime_ms() - window_started);
    }
}
#endif

void diag_receive(XDATA uint8_t *data, uint8_t length)
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

        const uint8_t *status = diag_hook_radio_status();

        if (status != 0) { // none has come yet: the zeros stay
            memcpy(&data[2], status, 6);
        }

#if !LOOP_PROFILE
    } else if (kind == 2) { // the firmware's version (core/version.h): [major, minor, patch, flags, commit (8 characters, the last is 0), dirty]
        memset(&data[2], 0, (size_t)(length - 2));
        data[2] = version_info.major;
        data[3] = version_info.minor;
        data[4] = version_info.patch;
        data[5] = version_info.flags;

        for (uint8_t i = 0; i < 8; i++) {
            data[6 + i] = (uint8_t)version_info.rev[i];
        }

        data[14] = version_info.dirty;
#endif
#if LOOP_PROFILE
    } else if (kind >= 5 && kind <= 8) {
        diag_profile(data, kind);
#endif
#if BATTERY_LOG
    } else if (kind == 4) { // the battery log, from the record asked for, as many as fit
        const uint8_t index = data[2];
        const uint8_t total = diag_hook_blog_count();

        memset(&data[2], 0, (size_t)(length - 2));
        data[2] = total;

        uint8_t out = 3;

        for (uint8_t i = index; i < total && (uint16_t)(out + 4u) <= length; i++) {
            uint8_t record[4];

            if (diag_hook_blog_get(i, record)) {
                memcpy(&data[out], record, 4);
                out = (uint8_t)(out + 4u);
            }
        }
#endif
    } else {
        data[0] = 0xFF;
    }
}
