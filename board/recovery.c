#include "recovery.h"
#include "matrix_hw.h"
#include "delay.h"
#include "watchdog.h"

// The chord has to be held, not just seen: a single sample could be one scan of a key being released.
#define RECOVERY_SAMPLES   6u
#define RECOVERY_SAMPLE_MS 10u

#define RECOVERY_KEYS 3u

// {column, row} of Esc, 1, 2
static const __code uint8_t chord[RECOVERY_KEYS][2] = {{0, 0}, {1, 0}, {2, 0}};

static bool chord_down(const uint8_t raw[MATRIX_COLS])
{
    for (uint8_t key = 0; key < RECOVERY_KEYS; key++) {
        if (!(raw[chord[key][0]] & (uint8_t)(1u << chord[key][1]))) {
            return false;
        }
    }

    return true;
}

// Enters the stock bootloader at 0xFF00 with the keys it expects in A and B (0x5A, 0xA5), interrupts off.
static void isp_jump(void) __naked
{
    // clang-format off
    __asm
        clr     IE.7
        mov     B, #0xa5
        mov     A, #0x5a
        ljmp    0xff00
    __endasm;
    // clang-format on
}

void recovery_check(void)
{
    uint8_t raw[MATRIX_COLS];

    for (uint8_t i = 0; i < RECOVERY_SAMPLES; i++) {
        watchdog_kick();
        matrix_hw_scan(raw);

        if (!chord_down(raw)) {
            return;
        }

        delay_ms(RECOVERY_SAMPLE_MS);
    }

    isp_jump();
}
