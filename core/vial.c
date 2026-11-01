#include "vial.h"
#include "keymap.h"
#include "matrix.h"
#include "via.h"
#include <string.h>

#define QSID_TAPPING_TERM  7u // Vial's number for the tapping term setting (16 bits)
#define UNLOCK_COUNTER_MAX 50u
#define UNLOCK_STEP_MS     100u

static bool     unlocked;
static bool     unlock_in_progress;
static uint8_t  unlock_counter;
static uint16_t unlock_timer;

void vial_init(void)
{
    unlocked           = false;
    unlock_in_progress = false;
    unlock_counter     = 0;
    unlock_timer       = 0;
}

bool vial_is_unlocked(void)
{
    return unlocked;
}

bool vial_unlock_in_progress(void)
{
    return unlock_in_progress;
}

static uint16_t now16(void)
{
    return (uint16_t)via_hook_now();
}

static bool unlock_keys_held(void)
{
    for (uint8_t i = 0; i < vial_unlock_key_count; i++) {
        if (!matrix_is_pressed(vial_unlock_keys[i][1], vial_unlock_keys[i][0])) { // the table is (row, col)
            return false;
        }
    }

    return true;
}

static void put_u32_le(XDATA uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8) & 0xFFu);
    out[2] = (uint8_t)((value >> 16) & 0xFFu);
    out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t u16_le(XDATA const uint8_t *in)
{
    return (uint16_t)(in[0] | ((uint16_t)in[1] << 8));
}

static void unlock_status(XDATA uint8_t *data, uint8_t length)
{
    memset(data, 0xFF, length);
    data[0] = unlocked;
    data[1] = unlock_in_progress;

    for (uint8_t i = 0; i < vial_unlock_key_count; i++) {
        data[2 + i * 2]     = vial_unlock_keys[i][0];
        data[2 + i * 2 + 1] = vial_unlock_keys[i][1];
    }
}

// The GUI polls while the user holds the keys; they must stay down for UNLOCK_COUNTER_MAX steps of 100 ms
static void unlock_poll(XDATA uint8_t *data)
{
    if (unlock_in_progress) {
        if ((uint16_t)(now16() - unlock_timer) > UNLOCK_STEP_MS && unlock_keys_held()) {
            unlock_timer = now16();

            if (--unlock_counter == 0) {
                unlock_in_progress = false;
                unlocked           = true;
            }
        } else {
            unlock_counter = UNLOCK_COUNTER_MAX;
        }
    }

    data[0] = unlocked;
    data[1] = unlock_in_progress;
    data[2] = unlock_counter;
}

// 32-byte pages of the compressed definition; a page past the end gets no reply content (the request is echoed)
static void get_definition(XDATA uint8_t *data, uint8_t length)
{
    _Static_assert(VIA_RAW_SIZE == 32, "the pages are 32 bytes: the page number is shifted by 5");

    const uint16_t start = (uint16_t)(u16_le(&data[2]) << 5);
    uint16_t       end   = (uint16_t)(start + length);

    if (start >= vial_definition_size) {
        return; // the request is echoed back: Vial reads the size first
    }

    if (end > vial_definition_size || end < start) {
        end = vial_definition_size;
    }

    for (uint16_t i = start; i < end; i++) {
        data[i - start] = vial_definition[i];
    }
}

// The GUI asks for both directions at once: counter-clockwise first, then clockwise (big-endian keycodes)
static void get_encoder(XDATA uint8_t *data)
{
    const bool     valid = data[2] < KEYMAP_LAYERS && data[3] < ENCODER_COUNT;
    const uint16_t ccw   = valid ? keymap_get_encoder(data[2], data[3], false) : 0;
    const uint16_t cw    = valid ? keymap_get_encoder(data[2], data[3], true) : 0;

    data[0] = (uint8_t)(ccw >> 8);
    data[1] = (uint8_t)(ccw & 0xFFu);
    data[2] = (uint8_t)(cw >> 8);
    data[3] = (uint8_t)(cw & 0xFFu);
}

// 0x00 counts the entries (tap dances, combos and key overrides; no alt repeat keys), 0x01/0x02 read and write a tap dance (status, then its
// five words), 0x03/0x04 the same for a combo (status, then four input keycodes and the output), 0x05/0x06 for a key override (status, then
// trigger, replacement, layers and the four option bytes). Index in data[3], the entry from data[4]; see docs/vial.md.
static void dynamic_entry(XDATA uint8_t *data, uint8_t length)
{
    switch (data[2]) {
        case 0x00:
            memset(data, 0, length);
            data[0] = KEYMAP_TAP_DANCE_COUNT;
            data[1] = KEYMAP_COMBO_COUNT;
            data[2] = KEYMAP_KEY_OVERRIDE_COUNT;
            break;

        case 0x01: {
            const uint8_t index = data[3];

            memset(data, 0, length);

            if (index >= KEYMAP_TAP_DANCE_COUNT) {
                data[0] = 1;
                break;
            }

            for (uint8_t field = 0; field < TD_FIELDS; field++) {
                const uint16_t word = keymap_tap_dance_get(index, field);

                data[1 + field * 2] = (uint8_t)(word & 0xFFu);
                data[2 + field * 2] = (uint8_t)(word >> 8);
            }
            break;
        }

        case 0x02: {
            const uint8_t index  = data[3];
            const uint8_t status = (index >= KEYMAP_TAP_DANCE_COUNT) ? 1 : 0;

            if (!status) {
                for (uint8_t field = 0; field < TD_FIELDS; field++) {
                    keymap_tap_dance_set(index, field, u16_le(&data[4 + field * 2]));
                }
            }

            data[0] = status;
            break;
        }

        case 0x03: { // combo: four input keycodes, then the output (10 bytes, the layout of docs/vial.md)
            const uint8_t index = data[3];

            memset(data, 0, length);

            if (index >= KEYMAP_COMBO_COUNT) {
                data[0] = 1;
                break;
            }

            for (uint8_t field = 0; field < COMBO_FIELDS; field++) {
                const uint16_t word = keymap_combo_get(index, field);

                data[1 + field * 2] = (uint8_t)(word & 0xFFu);
                data[2 + field * 2] = (uint8_t)(word >> 8);
            }
            break;
        }

        case 0x04: {
            const uint8_t index  = data[3];
            const uint8_t status = (index >= KEYMAP_COMBO_COUNT) ? 1 : 0;

            if (!status) {
                for (uint8_t field = 0; field < COMBO_FIELDS; field++) {
                    keymap_combo_set(index, field, u16_le(&data[4 + field * 2]));
                }
            }

            data[0] = status;
            break;
        }

        case 0x05: { // a key override: trigger, replacement, layers, then four bytes (10 bytes, see docs/vial.md)
            const uint8_t index = data[3];

            memset(data, 0, length);

            if (index >= KEYMAP_KEY_OVERRIDE_COUNT) {
                data[0] = 1;
                break;
            }

            for (uint8_t field = 0; field < KO_FIELDS; field++) {
                const uint16_t word = keymap_override_get(index, field);

                data[1 + field * 2] = (uint8_t)(word & 0xFFu);
                data[2 + field * 2] = (uint8_t)(word >> 8);
            }
            break;
        }

        case 0x06: {
            const uint8_t index  = data[3];
            const uint8_t status = (index >= KEYMAP_KEY_OVERRIDE_COUNT) ? 1 : 0;

            if (!status) {
                for (uint8_t field = 0; field < KO_FIELDS; field++) {
                    keymap_override_set(index, field, u16_le(&data[4 + field * 2]));
                }
            }

            data[0] = status;
            break;
        }

        default:
            break;
    }
}

void vial_receive(XDATA uint8_t *data, uint8_t length)
{
    switch (data[1]) {
        case vial_get_keyboard_id:
            memset(data, 0, length);
            put_u32_le(&data[0], VIAL_PROTOCOL_VERSION);
            memcpy(&data[4], vial_keyboard_uid, 8);
            break;

        case vial_get_size:
            put_u32_le(&data[0], (uint32_t)vial_definition_size);
            break;

        case vial_get_def:
            get_definition(data, length);
            break;

        case vial_get_encoder:
            get_encoder(data);
            break;

        case vial_set_encoder: // layer, encoder, direction (0 = counter-clockwise, 1 = clockwise), keycode (big-endian)
            keymap_set_encoder(data[2], data[3], data[4] != 0, (uint16_t)(((uint16_t)data[5] << 8) | data[6]));
            break;

        case vial_get_unlock_status:
            unlock_status(data, length);
            break;

        case vial_unlock_start:
            unlock_in_progress = true;
            unlock_counter     = UNLOCK_COUNTER_MAX;
            unlock_timer       = now16();
            break;

        case vial_unlock_poll:
            unlock_poll(data);
            break;

        case vial_lock:
            unlocked = false;
            break;

        case vial_dynamic_entry_op:
            dynamic_entry(data, length);
            break;

        // QMK settings: only the tapping term of the profile in use. query: the numbers greater than the one in data[2..3];
        // 0xFFFF ends the list
        case vial_qmk_settings_query: {
            const bool more = u16_le(&data[2]) < QSID_TAPPING_TERM; // read before the buffer is overwritten

            memset(data, 0xFF, length);

            if (more) {
                data[0] = QSID_TAPPING_TERM;
                data[1] = 0;
            }
            break;
        }

        case vial_qmk_settings_get: // status in data[0] (0 = ok), the value from data[1]
            if (u16_le(&data[2]) == QSID_TAPPING_TERM) {
                const uint16_t term = keymap_tapping_term();

                data[0] = 0;
                data[1] = (uint8_t)(term & 0xFFu);
                data[2] = (uint8_t)(term >> 8);
            } else {
                data[0] = 1;
            }
            break;

        case vial_qmk_settings_set: // the value from data[4]
            if (u16_le(&data[2]) == QSID_TAPPING_TERM) {
                keymap_set_tapping_term(u16_le(&data[4]));
                data[0] = 0;
            } else {
                data[0] = 1;
            }
            break;

        case vial_qmk_settings_reset:
            keymap_cfg_set(CFG_TAP_TERM, keymap_default_cfg(CFG_TAP_TERM));
            data[0] = 0;
            break;

        default: // the request is echoed back
            break;
    }
}
