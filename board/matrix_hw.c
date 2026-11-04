#include "matrix_hw.h"
#include "gpio.h"
#include "delay.h"

// Yunzii B68 matrix, read from the stock firmware and confirmed on the board with the probe layout
// (docs/keyboards/yunzii-b68.md).
//
// Rows R0..R4 are inputs with pull-up, pressed = 0: P7.1, P7.2, P7.3, P5.3, P5.4.
// Columns C0..C15 are the strobes, active low:
//   C0..C7  = P6.0..P6.7
//   C8..C10 = P5.0..P5.2, C11 = P5.7
//   C12 = P4.0, C13 = P4.2, C14 = P4.3, C15 = P4.5
// P7.0 is read by the stock firmware together with the rows but is not part of the matrix.

// Which bit of each port a column drives (0 = the column is on another port).
static const __code uint8_t col_p4[MATRIX_COLS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x04, 0x08, 0x20};
static const __code uint8_t col_p5[MATRIX_COLS] = {0, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x02, 0x04, 0x80, 0, 0, 0, 0};
static const __code uint8_t col_p6[MATRIX_COLS] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0, 0, 0, 0, 0, 0, 0, 0};

// Let the row lines settle after a strobe, and between the two readings. It was 10 us twice per column - 320 us of the scan's 753, measured on
// the keyboard - and 5 reads the same: the pull-up brings a line up in a couple of microseconds.
#define SETTLE_US 5u


// Rows into bits 0..4, pressed = 0; the bits above are forced to 1 so that they read as "not pressed".
static uint8_t read_rows(void)
{
    return (uint8_t)(((P7 >> 1) & 0x07u) | (P5 & 0x18u) | 0xE0u);
}

// The idle state of the stock firmware (function at 0xA627 in the dump, docs/keyboards/yunzii-b68-firmware-stock.md).
// The pins whose function is not known yet (LEDs, radio, mode switch) are left exactly as the stock firmware leaves them.
void board_gpio_init(void)
{
    // 1 = output
    GPIO_DIR_WRITE(0, 0x04);
    GPIO_DIR_WRITE(1, 0x3F);
    GPIO_DIR_WRITE(2, 0x3F);
    GPIO_DIR_WRITE(3, 0x3F);
    GPIO_DIR_WRITE(4, 0x2F);
    GPIO_DIR_WRITE(5, 0x87);
    GPIO_DIR_WRITE(6, 0xFF);
    GPIO_DIR_WRITE(7, 0x70);

    // 1 = pull-up on
    GPIO_PULLUP_WRITE(0, 0xF8);
    GPIO_PULLUP_WRITE(1, 0x3F);
    GPIO_PULLUP_WRITE(2, 0x3F);
    GPIO_PULLUP_WRITE(3, 0x3F);
    GPIO_PULLUP_WRITE(4, 0x6F);
    GPIO_PULLUP_WRITE(5, 0x9F);
    GPIO_PULLUP_WRITE(6, 0xFF);
    GPIO_PULLUP_WRITE(7, 0xFF);

    // Output latches: the column strobes (P4, P5, P6) idle high, P1-P3 low
    GPIO_WRITE(0, 0x04);
    GPIO_WRITE(1, 0x00);
    GPIO_WRITE(2, 0x00);
    GPIO_WRITE(3, 0x00);
    GPIO_WRITE(4, 0x2D);
    GPIO_WRITE(5, 0x87);
    GPIO_WRITE(6, 0xFF);
    GPIO_WRITE(7, 0x00);

    // Drive strength of P1, P2, P3, P5: the stock firmware writes 0, which is 25 mA
    DRVCON = DRVCON_UNLOCK_P1;
    P1DRV  = DRIVE_25MA;
    DRVCON = DRVCON_UNLOCK_P2;
    P2DRV  = DRIVE_25MA;
    DRVCON = DRVCON_UNLOCK_P3;
    P3DRV  = DRIVE_25MA;
    DRVCON = DRVCON_UNLOCK_P5;
    P5DRV  = DRIVE_25MA;
    DRVCON = DRVCON_LOCK;
}

// All the columns high: nothing is a sink
void matrix_hw_cols_off(void)
{
    GPIO_HIGH(4, 0x2D); // C12 C13 C14 C15
    GPIO_HIGH(5, 0x87); // C8 C9 C10 C11
    GPIO_HIGH(6, 0xFF); // C0-C7
}

// One column low: its LEDs can light (and its keys are the ones a scan would read)
void matrix_hw_col_on(uint8_t col)
{
    GPIO_LOW(4, col_p4[col]);
    GPIO_LOW(5, col_p5[col]);
    GPIO_LOW(6, col_p6[col]);
}

// One column: low, let the rows settle, read them twice, high again. The port of the column and its bit are known from the column number, so the other two
// ports are not touched (reading a RMW of a port with a zero mask changed nothing and cost a dozen instructions).
#define SCAN_COLUMN(port, mask)               \
    do {                                      \
        GPIO_LOW(port, mask);                 \
        delay_us_nokick(SETTLE_US);           \
        first = read_rows();                  \
        delay_us_nokick(SETTLE_US);           \
        second = read_rows();                 \
        GPIO_HIGH(port, mask);                \
        raw[col] = (first == second) ? (uint8_t)(~first & MATRIX_ROW_MASK) : 0u; \
    } while (0)

void matrix_hw_scan(XDATA uint8_t *raw)
{
    uint8_t first;
    uint8_t second;
    uint8_t col;

    for (col = 0; col < 8; col++) { // C0-C7 are P6.0-7
        const uint8_t bit = col_p6[col];

        SCAN_COLUMN(6, bit);
    }

    for (; col < 12; col++) { // C8-C11 are P5.0-2 and P5.7
        const uint8_t bit = col_p5[col];

        SCAN_COLUMN(5, bit);
    }

    for (; col < MATRIX_COLS; col++) { // C12-C15 are P4.0, P4.2, P4.3, P4.5
        const uint8_t bit = col_p4[col];

        SCAN_COLUMN(4, bit);
    }
}
