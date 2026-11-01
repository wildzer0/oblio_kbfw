#include "store.h"
#include "keycodes.h"
#include "keymap.h"

#define KEY_WORDS     ((uint16_t)(KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS))
#define ENCODER_WORDS ((uint16_t)(KEYMAP_LAYERS * ENCODER_COUNT * 2))
#define MACRO_WORDS   ((uint16_t)(KEYMAP_MACRO_BYTES / 2))
#define TD_WORDS      ((uint16_t)(KEYMAP_TAP_DANCE_COUNT * TD_FIELDS))
#define CFG_WORDS     ((uint16_t)KEYMAP_CFG_COUNT)

#define WORDS      ((uint16_t)(KEY_WORDS + ENCODER_WORDS + MACRO_WORDS + TD_WORDS + CFG_WORDS))
#define PAYLOAD    ((uint16_t)(WORDS * 2u))
#define HEADER     15u
#define SLOT_BYTES ((uint16_t)(STORE_SLOT_SECTORS * STORE_SECTOR_BYTES))
#define CHUNK      ((uint16_t)64u)

// Not the magic of the SMK port ('K', 'M'): slots written by that firmware are not read as ours.
#define MAGIC0  'O'
#define MAGIC1  'B'
#define VERSION 1u

#define NO_SLOT 0xFFu

_Static_assert(KEYMAP_MACRO_BYTES % 2 == 0 && KEYMAP_MACRO_BYTES / 8 < 256, "the macro buffer is a whole number of words, its size in 8-byte units fits a header byte");
_Static_assert((uint32_t)HEADER + PAYLOAD <= SLOT_BYTES, "the keymap does not fit in a slot");
_Static_assert(STORE_END == 0xEC00u, "the store must end where the settings sector starts");

static uint8_t chunk[CHUNK];

static uint8_t  active_slot = NO_SLOT;
static uint16_t active_seq;
static bool     dirty;
static uint8_t  seen_version; // keymap_version() at the last look
static uint16_t last_edit;

static uint16_t header_seq; // the header just read by slot_header()
static uint16_t header_crc;

// Fingerprint of the compiled-in keymap, kept in the header: a copy saved by a firmware whose defaults were different (a new key on
// the Fn layer, say) is not used, because it would hide what the new firmware adds.
static uint16_t defaults_print;

static uint16_t slot_addr(uint8_t slot)
{
    return (uint16_t)(STORE_BASE + (uint16_t)slot * SLOT_BYTES);
}

// CRC-16/CCITT-FALSE, bitwise: slow but tiny, and it only runs at load and on a save.
static uint16_t crc16_byte(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);

    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }

    return crc;
}

// ---- the payload as a run of words ----
// In order: the keys (layer, row, column), the encoders (layer, encoder, direction), the macros (two bytes a word, little-endian), the
// tap dances (index, field), the settings. A cursor walks it by counting, so no word needs a division to find its place.
typedef enum { SEC_KEYS, SEC_ENCODERS, SEC_MACROS, SEC_TAP_DANCES, SEC_CFG, SEC_END } section_t;

static section_t sec;
static uint8_t   ca, cb, cc;  // the position in the section
static uint16_t  cm;          // the position in the macro buffer, in words

static void cursor_start(void)
{
    sec = SEC_KEYS;
    ca = cb = cc = 0;
    cm = 0;
}

static void cursor_advance(void)
{
    switch (sec) {
        case SEC_KEYS:
            if (++cc >= MATRIX_COLS) {
                cc = 0;
                if (++cb >= MATRIX_ROWS) {
                    cb = 0;
                    if (++ca >= KEYMAP_LAYERS) {
                        sec = SEC_ENCODERS;
                        ca = 0;
                    }
                }
            }
            break;
        case SEC_ENCODERS:
            if (++cc >= 2) {
                cc = 0;
                if (++cb >= ENCODER_COUNT) {
                    cb = 0;
                    if (++ca >= KEYMAP_LAYERS) {
                        sec = SEC_MACROS;
                        ca = 0;
                    }
                }
            }
            break;
        case SEC_MACROS:
            if (++cm >= MACRO_WORDS) {
                sec = SEC_TAP_DANCES;
                ca = cb = 0;
            }
            break;
        case SEC_TAP_DANCES:
            if (++cb >= TD_FIELDS) {
                cb = 0;
                if (++ca >= KEYMAP_TAP_DANCE_COUNT) {
                    sec = SEC_CFG;
                    ca = 0;
                }
            }
            break;
        case SEC_CFG:
            if (++ca >= KEYMAP_CFG_COUNT) {
                sec = SEC_END;
            }
            break;
        default:
            break;
    }
}

// The word under the cursor, as the RAM copy has it
static uint16_t cursor_get(void)
{
    switch (sec) {
        case SEC_KEYS:       return keymap_get(ca, cb, cc);
        case SEC_ENCODERS:   return keymap_get_encoder(ca, cb, cc != 0);
        case SEC_MACROS:     return (uint16_t)(keymap_macro_byte((uint16_t)(cm * 2u)) | ((uint16_t)keymap_macro_byte((uint16_t)(cm * 2u + 1u)) << 8));
        case SEC_TAP_DANCES: return keymap_tap_dance_get(ca, cb);
        case SEC_CFG:        return keymap_cfg_get(ca);
        default:             return 0;
    }
}

// The word under the cursor, as the compiled-in keymap has it (the macros are empty by default)
static uint16_t cursor_default(void)
{
    switch (sec) {
        case SEC_KEYS:       return keymap_default(ca, cb, cc);
        case SEC_ENCODERS:   return keymap_default_encoder(ca, cb, cc != 0);
        case SEC_TAP_DANCES: return keymap_default_tap_dance(ca, cb);
        case SEC_CFG:        return keymap_default_cfg(ca);
        default:             return 0;
    }
}

static void cursor_set(uint16_t value)
{
    switch (sec) {
        case SEC_KEYS:
            keymap_set(ca, cb, cc, value);
            break;
        case SEC_ENCODERS:
            keymap_set_encoder(ca, cb, cc != 0, value);
            break;
        case SEC_MACROS: {
            const uint8_t bytes[2] = {(uint8_t)(value & 0xFFu), (uint8_t)(value >> 8)};

            keymap_macro_write((uint16_t)(cm * 2u), 2, bytes);
            break;
        }
        case SEC_TAP_DANCES:
            keymap_tap_dance_set(ca, cb, value);
            break;
        case SEC_CFG:
            keymap_cfg_set(ca, value);
            break;
        default:
            break;
    }
}

static void compute_defaults_print(void)
{
    uint16_t crc = 0xFFFFu;

    cursor_start();

    for (uint16_t i = 0; i < WORDS; i++) {
        const uint16_t word = cursor_default();

        crc = crc16_byte(crc, (uint8_t)(word & 0xFFu));
        crc = crc16_byte(crc, (uint8_t)(word >> 8));
        cursor_advance();
    }

    defaults_print = crc;
}

// Reads the header of `slot`; true when it is a header of this build's shape. Leaves seq and CRC in header_*.
static bool slot_header(uint8_t slot)
{
    store_hook_read(slot_addr(slot), chunk, HEADER);

    if (chunk[0] != MAGIC0 || chunk[1] != MAGIC1 || chunk[2] != VERSION || chunk[3] != KEYMAP_LAYERS || chunk[4] != MATRIX_ROWS
        || chunk[5] != MATRIX_COLS || chunk[10] != ENCODER_COUNT || chunk[11] != (KEYMAP_MACRO_BYTES / 8)
        || chunk[12] != (uint8_t)(defaults_print & 0xFFu) || chunk[13] != (uint8_t)(defaults_print >> 8) || chunk[14] != 0u) {
        return false;
    }

    header_seq = (uint16_t)(chunk[6] | ((uint16_t)chunk[7] << 8));
    header_crc = (uint16_t)(chunk[8] | ((uint16_t)chunk[9] << 8));
    return true;
}

// CRC of the payload of `slot`. With `apply`, the words also go into the RAM copy.
static uint16_t slot_payload(uint8_t slot, bool apply)
{
    uint16_t crc = 0xFFFFu;
    uint16_t at  = 0; // byte offset into the payload

    cursor_start();

    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        store_hook_read((uint16_t)(slot_addr(slot) + HEADER + at), chunk, n);

        for (uint8_t i = 0; i < n; i++) {
            crc = crc16_byte(crc, chunk[i]);
        }

        if (apply) {
            for (uint8_t i = 0; i < n; i += 2) {
                cursor_set((uint16_t)(chunk[i] | ((uint16_t)chunk[i + 1] << 8)));
                cursor_advance();
            }
        }

        at += n;
    }

    return crc;
}

// True when `slot` holds a complete keymap; its sequence is then in header_seq.
static bool slot_valid(uint8_t slot)
{
    return slot_header(slot) && slot_payload(slot, false) == header_crc;
}

static void mark_loaded(void)
{
    dirty        = false;
    seen_version = keymap_version();
    store_hook_loaded();
}

// Loads the keymap from its newest valid slot, or the defaults.
static void load_keymap(void)
{
    uint16_t seq0 = 0;
    uint16_t seq1 = 0;

    const bool valid0 = slot_valid(0);
    if (valid0) {
        seq0 = header_seq;
    }

    const bool valid1 = slot_valid(1);
    if (valid1) {
        seq1 = header_seq;
    }

    active_slot = NO_SLOT;

    if (!valid0 && !valid1) {
        keymap_init();
        mark_loaded();
        return;
    }

    // wrap-safe "newer": the sequence is only ever compared between two live slots
    const uint8_t newest = (!valid0 || (valid1 && (int16_t)(seq1 - seq0) > 0)) ? 1 : 0;

    slot_payload(newest, true);
    active_slot = newest;
    active_seq  = newest ? seq1 : seq0;
    mark_loaded();
}

void store_init(void)
{
    compute_defaults_print();
    load_keymap();
}

bool store_dirty(void)
{
    return dirty || keymap_version() != seen_version;
}

void store_flush(void)
{
    if (!store_dirty()) {
        return;
    }

    dirty        = false; // cleared first: an edit during the write is not lost
    seen_version = keymap_version();

    const uint8_t  target = (active_slot == 0) ? 1 : 0;
    const uint16_t seq    = (uint16_t)(active_seq + 1);
    const uint16_t base   = slot_addr(target);

    store_hook_flush_begin(); // the timer and the backlight stand still for the whole write, not once per chunk

    for (uint8_t sector = 0; sector < STORE_SLOT_SECTORS; sector++) {
        store_hook_erase((uint16_t)(base + sector * STORE_SECTOR_BYTES));
    }

    uint16_t crc = 0xFFFFu;
    uint16_t at  = 0;

    cursor_start();

    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        for (uint8_t i = 0; i < n; i += 2) {
            const uint16_t word = cursor_get();

            cursor_advance();
            chunk[i]     = (uint8_t)(word & 0xFFu);
            chunk[i + 1] = (uint8_t)(word >> 8);
        }

        for (uint8_t i = 0; i < n; i++) {
            crc = crc16_byte(crc, chunk[i]);
        }

        store_hook_program((uint16_t)(base + HEADER + at), chunk, n);
        at += n;
    }

    // last: until this lands the slot is not a valid keymap
    chunk[0]  = MAGIC0;
    chunk[1]  = MAGIC1;
    chunk[2]  = VERSION;
    chunk[3]  = KEYMAP_LAYERS;
    chunk[4]  = MATRIX_ROWS;
    chunk[5]  = MATRIX_COLS;
    chunk[6]  = (uint8_t)(seq & 0xFFu);
    chunk[7]  = (uint8_t)(seq >> 8);
    chunk[8]  = (uint8_t)(crc & 0xFFu);
    chunk[9]  = (uint8_t)(crc >> 8);
    chunk[10] = ENCODER_COUNT;
    chunk[11] = KEYMAP_MACRO_BYTES / 8; // the macro buffer's size, in 8-byte units, so a size change invalidates old data
    chunk[12] = (uint8_t)(defaults_print & 0xFFu);
    chunk[13] = (uint8_t)(defaults_print >> 8);
    chunk[14] = 0;
    store_hook_program(base, chunk, HEADER);

    store_hook_flush_end();

    active_slot = target;
    active_seq  = seq;
}

void store_task(uint16_t now, bool any_key_down)
{
    if (keymap_version() != seen_version) { // an edit: the quiet time starts over
        seen_version = keymap_version();
        dirty        = true;
        last_edit    = now;
    }

    if (any_key_down) {
        return;
    }

    if (dirty && (uint16_t)(now - last_edit) >= STORE_QUIET_MS) {
        store_flush();
    }
}
