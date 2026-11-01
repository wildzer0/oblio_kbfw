#include "hud.h"
#include "keymap.h"
#include "layer.h"
#include "matrix.h"
#include "report.h"
#include <string.h>

#define PROTOCOL       2
#define KEYMAP_MAX     12 // keycodes per GET_KEYMAP reply
#define BYTES_PER_ROW  ((MATRIX_COLS + 7) / 8)
#define MATRIX_BYTES   (MATRIX_ROWS * BYTES_PER_ROW)
#define STATE_ROWS     (MATRIX_ROWS > 8 ? 8 : MATRIX_ROWS) // GET_STATE has room for eight rows

_Static_assert(MATRIX_BYTES <= 28, "GET_MATRIX must fit one packet");

enum { GET_INFO = 0x01, GET_STATE = 0x02, GET_KEYMAP = 0x03, GET_MATRIX = 0x04 };
enum { STATUS_OK = 0, STATUS_UNKNOWN_COMMAND = 1, STATUS_BAD_ARGUMENT = 2 };

#define CAP_HUD_TOG (1u << 3) // capability bit of GET_INFO

#define FLAG_CAPS_LOCK (1u << 0) // flag bits of GET_STATE
#define FLAG_NUM_LOCK  (1u << 4)

#define LED_NUM_LOCK  (1u << 0) // bits of the HID LED report
#define LED_CAPS_LOCK (1u << 1)

static uint8_t toggle_count; // +1 on every HUD_TOG press

void hud_toggle_pressed(void)
{
    toggle_count++;
}

static void put16(XDATA uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void get_info(XDATA uint8_t *data)
{
    data[4] = PROTOCOL;
    data[5] = MATRIX_ROWS;
    data[6] = MATRIX_COLS;
    data[7] = KEYMAP_LAYERS;
    put16(&data[8], CAP_HUD_TOG);
    put16(&data[10], HUD_TOGGLE_KEYCODE);
    data[12] = KEYMAP_MAX;
    data[13] = keymap_version();
    data[14] = BYTES_PER_ROW;
}

static void get_state(XDATA uint8_t *data)
{
    const uint32_t layers = layer_get_state();

    put16(&data[4], (uint16_t)(layers & 0xFFFFu)); // the mask is 32 bits wide, only 8 layers exist
    put16(&data[6], 0);
    data[8] = (uint8_t)(1u << layer_get_default());
    data[9] = 0; // no layer lock

    // the first eight columns of up to eight rows, as protocol 1 has it (data[10..17])
    for (uint8_t row = 0; row < STATE_ROWS; row++) {
        data[10 + row] = (uint8_t)(matrix_row_bits(row) & 0xFFu);
    }

    data[18] = report_mods();
    data[19] = 0; // no weak modifiers
    data[20] = layer_oneshot_mods();
    data[21] = 0; // no locked one-shot modifiers
    data[22] = layer_oneshot_layer();

    const uint8_t leds  = hud_hook_host_leds();
    uint8_t       flags = 0;

    if (leds & LED_CAPS_LOCK) {
        flags |= FLAG_CAPS_LOCK;
    }

    if (leds & LED_NUM_LOCK) {
        flags |= FLAG_NUM_LOCK;
    }

    data[23] = flags;

    // 24..26: OS detection, OS mode and active OS: none of those exist here
    data[27] = toggle_count;
    data[28] = keymap_version();
}

static void get_matrix(XDATA uint8_t *data)
{
    uint8_t at = 4;

    _Static_assert(BYTES_PER_ROW == 2, "GET_MATRIX writes two bytes a row: the columns 0-7, then 8-15");

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        const uint16_t bits = matrix_row_bits(row);

        data[at++] = (uint8_t)(bits & 0xFFu);
        data[at++] = (uint8_t)(bits >> 8);
    }
}

// The keycodes of `count` keys from position `offset` (row * columns + column) on a layer
static void get_keymap(XDATA uint8_t *data, XDATA const uint8_t *args)
{
    const uint8_t  layer  = args[0];
    const uint16_t offset = (uint16_t)(args[1] | ((uint16_t)args[2] << 8));
    const uint8_t  count  = args[3];

    memcpy(&data[4], args, 4); // the request is echoed

    if (layer >= KEYMAP_LAYERS || count == 0 || count > KEYMAP_MAX || (uint32_t)offset + count > (uint32_t)(MATRIX_ROWS * MATRIX_COLS)) {
        data[2] = STATUS_BAD_ARGUMENT;
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        const uint16_t position = (uint16_t)(offset + i);

        put16(&data[8 + 2 * i], keymap_get(layer, (uint8_t)(position / MATRIX_COLS), (uint8_t)(position % MATRIX_COLS)));
    }
}

bool hud_receive(XDATA uint8_t *data, uint8_t length)
{
    static uint8_t args[4]; // in external RAM like the packet, so that the pointers stay short

    if (data[0] != HUD_CMD || length < 32) {
        return false;
    }

    // data[1] (the command) and data[3] (the app's sequence number) stay as they are
    memcpy(args, &data[4], sizeof args);
    memset(&data[4], 0, (size_t)(length - 4));
    data[2] = STATUS_OK;

    switch (data[1]) {
        case GET_INFO:
            get_info(data);
            break;
        case GET_STATE:
            get_state(data);
            break;
        case GET_KEYMAP:
            get_keymap(data, args);
            break;
        case GET_MATRIX:
            get_matrix(data);
            break;
        default:
            data[2] = STATUS_UNKNOWN_COMMAND;
            break;
    }

    return true;
}
