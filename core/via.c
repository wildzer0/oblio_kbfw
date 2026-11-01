#include "via.h"
#include "diag.h"
#include "hud.h"
#include "keymap.h"
#include "matrix.h"
#include "vial.h"
#include "vialrgb.h"
#include <string.h>

#define VIA_PROTOCOL_VERSION 0x0009u

// Commands (data[0])
enum {
    id_get_protocol_version                 = 0x01,
    id_get_keyboard_value                   = 0x02,
    id_set_keyboard_value                   = 0x03,
    id_dynamic_keymap_get_keycode           = 0x04,
    id_dynamic_keymap_set_keycode           = 0x05,
    id_dynamic_keymap_reset                 = 0x06,
    id_lighting_set_value                   = 0x07,
    id_lighting_get_value                   = 0x08,
    id_lighting_save                        = 0x09,
    id_bootloader_jump                      = 0x0B,
    id_dynamic_keymap_macro_get_count       = 0x0C,
    id_dynamic_keymap_macro_get_buffer_size = 0x0D,
    id_dynamic_keymap_macro_get_buffer      = 0x0E,
    id_dynamic_keymap_macro_set_buffer      = 0x0F,
    id_dynamic_keymap_macro_reset           = 0x10,
    id_dynamic_keymap_get_layer_count       = 0x11,
    id_dynamic_keymap_get_buffer            = 0x12,
    id_dynamic_keymap_set_buffer            = 0x13,
    id_hud                                  = 0xF0, // the keyboard HUD (hud.h)
    id_oblio_diag                           = 0xF1, // counters for the first runs on the hardware (diag.h)
    id_vial_prefix                          = 0xFE,
    id_unhandled                            = 0xFF,
};

// Keyboard values (args[0] of get / set keyboard value)
enum {
    value_uptime              = 0x01,
    value_layout_options      = 0x02,
    value_switch_matrix_state = 0x03,
};

#define MAX_CHUNK 28u // a request carries at most 28 bytes of buffer data

// The layout-option bits the GUI stores for keyboards with layout variants (kept in RAM only)
static uint32_t layout_options;

static bool bootloader_requested;

void via_init(void)
{
    layout_options       = 0;
    bootloader_requested = false;
    vial_init();
}

bool via_bootloader_requested(void)
{
    return bootloader_requested;
}

#define BUFFER_CELLS ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define BUFFER_BYTES ((uint16_t)(KEYMAP_LAYERS * BUFFER_CELLS * 2u))

_Static_assert(MATRIX_COLS == 16, "the keymap buffer is addressed with shifts: 16 columns a row");

// The keymap as the GUI sees it: layer by layer, row by row, each keycode big-endian. A position in the buffer is found once; the loops
// then step from key to key, so that no division by the 80 keys of a layer is needed.
typedef struct {
    uint8_t  layer;
    uint8_t  row;
    uint8_t  col;
    bool     low;   // the second byte of the keycode
    uint16_t keycode;
} spot_t;

// False if the offset is past the end of the buffer
static bool spot_at(spot_t *spot, uint16_t offset)
{
    uint16_t word = offset >> 1;

    if (offset >= BUFFER_BYTES) {
        return false;
    }

    spot->layer = 0;

    while (word >= BUFFER_CELLS) {
        word = (uint16_t)(word - BUFFER_CELLS);
        spot->layer++;
    }

    spot->row = (uint8_t)(word >> 4);
    spot->col = (uint8_t)(word & 0x0Fu);
    spot->low = (offset & 1u) != 0;
    return true;
}

static void spot_next(spot_t *spot)
{
    if (!spot->low) {
        spot->low = true;
        return;
    }

    spot->low = false;

    if (++spot->col >= MATRIX_COLS) {
        spot->col = 0;

        if (++spot->row >= MATRIX_ROWS) {
            spot->row = 0;
            spot->layer++;
        }
    }
}

static uint8_t buffer_get(spot_t *spot)
{
    if (spot->layer >= KEYMAP_LAYERS) {
        return 0; // past the end
    }

    const uint16_t keycode = keymap_get(spot->layer, spot->row, spot->col);

    return spot->low ? (uint8_t)(keycode & 0xFFu) : (uint8_t)(keycode >> 8);
}

static void buffer_set(spot_t *spot, uint8_t value)
{
    if (spot->layer >= KEYMAP_LAYERS) {
        return;
    }

    uint16_t keycode = keymap_get(spot->layer, spot->row, spot->col);

    if (spot->low) {
        keycode = (uint16_t)((keycode & 0xFF00u) | value);
    } else {
        keycode = (uint16_t)((keycode & 0x00FFu) | ((uint16_t)value << 8));
    }

    keymap_set(spot->layer, spot->row, spot->col, keycode);
}

static void put_u32_be(XDATA uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint16_t u16_be(XDATA const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

// The GUI's matrix tester: one big-endian bit field per row, a bit per column
static void switch_matrix_state(XDATA uint8_t *args)
{
    _Static_assert(MATRIX_COLS == 16 && MATRIX_ROWS * 2 <= 28, "two bytes a row, all the rows in one report");

    uint8_t at = 1;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        const uint16_t bits = matrix_row_bits(row);

        args[at++] = (uint8_t)(bits >> 8);
        args[at++] = (uint8_t)(bits & 0xFFu);
    }
}

static void get_keyboard_value(XDATA uint8_t *data)
{
    XDATA uint8_t *args = &data[1];

    switch (args[0]) {
        case value_uptime:
            put_u32_be(&args[1], via_hook_now());
            break;

        case value_layout_options:
            put_u32_be(&args[1], layout_options);
            break;

        case value_switch_matrix_state:
            if (vial_is_unlocked()) { // it shows every key press, so only an unlocked keyboard answers
                switch_matrix_state(args);
            }
            break;

        default:
            data[0] = id_unhandled;
            break;
    }
}

static void set_keyboard_value(XDATA uint8_t *data)
{
    XDATA const uint8_t *args = &data[1];

    if (args[0] == value_layout_options) {
        layout_options = ((uint32_t)args[1] << 24) | ((uint32_t)args[2] << 16) | ((uint32_t)args[3] << 8) | args[4];
    } else {
        data[0] = id_unhandled;
    }
}

// While an unlock runs the GUI may only ask the unlock questions: the rest is echoed back untouched
static bool allowed_during_unlock(XDATA const uint8_t *data)
{
    const uint8_t cmd = data[1];

    return data[0] == id_vial_prefix
        && (cmd == vial_get_keyboard_id || cmd == vial_get_size || cmd == vial_get_def || cmd == vial_get_unlock_status
            || cmd == vial_unlock_start || cmd == vial_unlock_poll);
}

void via_receive(XDATA uint8_t *data, uint8_t length)
{
    XDATA uint8_t *args = &data[1];

    if (length != VIA_RAW_SIZE) {
        return;
    }

    if (vial_unlock_in_progress() && !allowed_during_unlock(data)) {
        return;
    }

    switch (data[0]) {
        case id_get_protocol_version:
            args[0] = (uint8_t)(VIA_PROTOCOL_VERSION >> 8);
            args[1] = (uint8_t)(VIA_PROTOCOL_VERSION & 0xFFu);
            break;

        case id_get_keyboard_value:
            get_keyboard_value(data);
            break;

        case id_set_keyboard_value:
            set_keyboard_value(data);
            break;

        case id_dynamic_keymap_get_keycode: {
            const uint16_t keycode = (args[0] < KEYMAP_LAYERS && args[1] < MATRIX_ROWS && args[2] < MATRIX_COLS) ? keymap_get(args[0], args[1], args[2]) : 0u;

            args[3] = (uint8_t)(keycode >> 8);
            args[4] = (uint8_t)(keycode & 0xFFu);
            break;
        }

        case id_dynamic_keymap_set_keycode:
            keymap_set(args[0], args[1], args[2], u16_be(&args[3]));
            break;

        case id_dynamic_keymap_reset:
            keymap_reset();
            break;

        case id_dynamic_keymap_get_layer_count:
            args[0] = KEYMAP_LAYERS;
            break;

        case id_dynamic_keymap_get_buffer: {
            const uint16_t offset = u16_be(&args[0]);
            const uint8_t  size   = args[2];
            spot_t         spot;

            if (size <= MAX_CHUNK && spot_at(&spot, offset)) {
                for (uint8_t i = 0; i < size; i++) {
                    args[3 + i] = buffer_get(&spot);
                    spot_next(&spot);
                }
            } else if (size <= MAX_CHUNK) {
                memset(&args[3], 0, size); // all of it is past the end
            }
            break;
        }

        case id_dynamic_keymap_set_buffer: {
            const uint16_t offset = u16_be(&args[0]);
            const uint8_t  size   = args[2];
            spot_t         spot;

            if (size <= MAX_CHUNK && spot_at(&spot, offset)) {
                for (uint8_t i = 0; i < size; i++) {
                    buffer_set(&spot, args[3 + i]);
                    spot_next(&spot);
                }
            }
            break;
        }

        case id_dynamic_keymap_macro_get_count:
            args[0] = KEYMAP_MACRO_COUNT;
            break;

        case id_dynamic_keymap_macro_get_buffer_size:
            args[0] = (uint8_t)(KEYMAP_MACRO_BYTES >> 8);
            args[1] = (uint8_t)(KEYMAP_MACRO_BYTES & 0xFFu);
            break;

        case id_dynamic_keymap_macro_get_buffer: {
            const uint16_t offset = u16_be(&args[0]);
            const uint8_t  size   = args[2];

            if (size <= MAX_CHUNK) {
                keymap_macro_read(offset, size, &args[3]);
            }
            break;
        }

        case id_dynamic_keymap_macro_set_buffer: {
            // until the keyboard is unlocked the macros cannot be changed: the request is echoed back untouched
            const uint16_t offset = u16_be(&args[0]);
            const uint8_t  size   = args[2];

            if (vial_is_unlocked() && size <= MAX_CHUNK) {
                keymap_macro_write(offset, size, &args[3]);
            }
            break;
        }

        case id_dynamic_keymap_macro_reset:
            if (vial_is_unlocked()) {
                keymap_macro_clear();
            }
            break;

        case id_bootloader_jump:
            // like the other dangerous commands it needs the unlock; the reply goes out first, then the jump
            if (vial_is_unlocked()) {
                bootloader_requested = true;
            }
            break;

        case id_lighting_get_value: // the per-key lighting of the Vial app (core/vialrgb.c)
            vialrgb_get_value(data);
            break;

        case id_lighting_set_value:
            vialrgb_set_value(data);
            break;

        case id_lighting_save:
            vialrgb_save();
            break;

        case id_vial_prefix:
            vial_receive(data, length);
            break;

        case id_hud:
            if (!hud_receive(data, length)) {
                data[0] = id_unhandled;
            }
            break;

        case id_oblio_diag:
            // The counters (kind 0), the radio data (kind 1) and the port readout (kind 3) are read-only and harmless, so they need no
            // unlock: they are what is needed when the keys do not work or when something has to be measured. Only the monitoring (kind 2),
            // which changes what the radio is doing, does.
            if (vial_is_unlocked() || data[1] == 0 || data[1] == 1 || data[1] == 3) {
                diag_receive(data, length);
            } else {
                data[0] = id_unhandled;
            }
            break;

        default:
            data[0] = id_unhandled;
            break;
    }
}
