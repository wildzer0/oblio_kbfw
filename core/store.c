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

#define NO_SLOT    0xFFu
#define NO_PROFILE 0xFFu

_Static_assert(KEYMAP_MACRO_BYTES % 2 == 0 && KEYMAP_MACRO_BYTES / 8 < 256, "the macro buffer is a whole number of words, its size in 8-byte units fits a header byte");
_Static_assert((uint32_t)HEADER + PAYLOAD <= SLOT_BYTES, "the profile does not fit in a slot");
_Static_assert(STORE_END == 0xEC00u, "the profiles must end where the settings sector starts");

static uint8_t chunk[CHUNK];

static uint8_t  profile;
static uint8_t  pending_profile = NO_PROFILE;
static uint8_t  active_slot     = NO_SLOT;
static uint16_t active_seq;
static bool     dirty;
static uint8_t  seen_version; // keymap_version() at the last look
static uint16_t last_edit;

static uint16_t header_seq; // the header just read by slot_header()
static uint16_t header_crc;

// Fingerprint of the compiled-in profile, kept in the header: a copy saved by a firmware whose defaults were different (a new
// key on the Fn layer, say) is not used, because it would hide what the new firmware adds.
static uint16_t defaults_print;

static uint16_t slot_addr(uint8_t slot)
{
    return (uint16_t)(STORE_BASE + (uint16_t)(profile * 2u + slot) * SLOT_BYTES);
}

// CRC-16/CCITT-FALSE, bitwise: slow but tiny, and it only runs at load and on a save.
static uint16_t crc16_word(uint16_t crc, uint16_t word)
{
    for (uint8_t byte = 0; byte < 2; byte++) {
        crc ^= (uint16_t)((byte ? (word >> 8) : (word & 0xFFu)) << 8);

        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

static uint16_t crc16_byte(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);

    for (uint8_t bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }

    return crc;
}

// The compiled-in value of payload word `index` (the macros are empty by default)
static uint16_t default_word(uint16_t index)
{
    if (index < KEY_WORDS) {
        const uint16_t per_layer = (uint16_t)(MATRIX_ROWS * MATRIX_COLS);
        const uint16_t in_layer  = (uint16_t)(index % per_layer);

        return keymap_default((uint8_t)(index / per_layer), (uint8_t)(in_layer / MATRIX_COLS), (uint8_t)(in_layer % MATRIX_COLS));
    }

    index = (uint16_t)(index - KEY_WORDS);

    if (index < ENCODER_WORDS) {
        return keymap_default_encoder((uint8_t)(index / (ENCODER_COUNT * 2u)), (uint8_t)((index / 2u) % ENCODER_COUNT), (index & 1u) != 0);
    }

    index = (uint16_t)(index - ENCODER_WORDS);

    if (index < MACRO_WORDS) {
        return 0;
    }

    index = (uint16_t)(index - MACRO_WORDS);

    if (index < TD_WORDS) {
        return keymap_default_tap_dance((uint8_t)(index / TD_FIELDS), (uint8_t)(index % TD_FIELDS));
    }

    return keymap_default_cfg((uint8_t)(index - TD_WORDS));
}

// Payload word `index` of the RAM copy: the keys, the encoders, the macros (two bytes a word, little-endian), the tap
// dances, the settings.
static uint16_t word_get(uint16_t index)
{
    if (index < KEY_WORDS) {
        const uint16_t per_layer = (uint16_t)(MATRIX_ROWS * MATRIX_COLS);
        const uint16_t in_layer  = (uint16_t)(index % per_layer);

        return keymap_get((uint8_t)(index / per_layer), (uint8_t)(in_layer / MATRIX_COLS), (uint8_t)(in_layer % MATRIX_COLS));
    }

    index = (uint16_t)(index - KEY_WORDS);

    if (index < ENCODER_WORDS) {
        return keymap_get_encoder((uint8_t)(index / (ENCODER_COUNT * 2u)), (uint8_t)((index / 2u) % ENCODER_COUNT), (index & 1u) != 0);
    }

    index = (uint16_t)(index - ENCODER_WORDS);

    if (index < MACRO_WORDS) {
        return (uint16_t)(keymap_macro_byte((uint16_t)(index * 2u)) | ((uint16_t)keymap_macro_byte((uint16_t)(index * 2u + 1u)) << 8));
    }

    index = (uint16_t)(index - MACRO_WORDS);

    if (index < TD_WORDS) {
        return keymap_tap_dance_get((uint8_t)(index / TD_FIELDS), (uint8_t)(index % TD_FIELDS));
    }

    return keymap_cfg_get((uint8_t)(index - TD_WORDS));
}

static void word_set(uint16_t index, uint16_t value)
{
    if (index < KEY_WORDS) {
        const uint16_t per_layer = (uint16_t)(MATRIX_ROWS * MATRIX_COLS);
        const uint16_t in_layer  = (uint16_t)(index % per_layer);

        keymap_set((uint8_t)(index / per_layer), (uint8_t)(in_layer / MATRIX_COLS), (uint8_t)(in_layer % MATRIX_COLS), value);
        return;
    }

    index = (uint16_t)(index - KEY_WORDS);

    if (index < ENCODER_WORDS) {
        keymap_set_encoder((uint8_t)(index / (ENCODER_COUNT * 2u)), (uint8_t)((index / 2u) % ENCODER_COUNT), (index & 1u) != 0, value);
        return;
    }

    index = (uint16_t)(index - ENCODER_WORDS);

    if (index < MACRO_WORDS) {
        const uint8_t bytes[2] = {(uint8_t)(value & 0xFFu), (uint8_t)(value >> 8)};

        keymap_macro_write((uint16_t)(index * 2u), 2, bytes);
        return;
    }

    index = (uint16_t)(index - MACRO_WORDS);

    if (index < TD_WORDS) {
        keymap_tap_dance_set((uint8_t)(index / TD_FIELDS), (uint8_t)(index % TD_FIELDS), value);
        return;
    }

    keymap_cfg_set((uint8_t)(index - TD_WORDS), value);
}

static void compute_defaults_print(void)
{
    uint16_t crc = 0xFFFFu;

    for (uint16_t i = 0; i < WORDS; i++) {
        crc = crc16_word(crc, default_word(i));
    }

    defaults_print = crc;
}

// Reads the header of `slot`; true when it is a header of this build's shape and profile. Leaves seq and CRC in header_*.
static bool slot_header(uint8_t slot)
{
    store_hook_read(slot_addr(slot), chunk, HEADER);

    if (chunk[0] != MAGIC0 || chunk[1] != MAGIC1 || chunk[2] != VERSION || chunk[3] != KEYMAP_LAYERS || chunk[4] != MATRIX_ROWS
        || chunk[5] != MATRIX_COLS || chunk[10] != ENCODER_COUNT || chunk[11] != (KEYMAP_MACRO_BYTES / 8)
        || chunk[12] != (uint8_t)(defaults_print & 0xFFu) || chunk[13] != (uint8_t)(defaults_print >> 8) || chunk[14] != profile) {
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

    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        store_hook_read((uint16_t)(slot_addr(slot) + HEADER + at), chunk, n);

        for (uint8_t i = 0; i < n; i++) {
            crc = crc16_byte(crc, chunk[i]);
        }

        if (apply) {
            for (uint8_t i = 0; i < n; i += 2) {
                word_set((uint16_t)((at + i) / 2), (uint16_t)(chunk[i] | ((uint16_t)chunk[i + 1] << 8)));
            }
        }

        at += n;
    }

    return crc;
}

// True when `slot` holds a complete profile; its sequence is then in header_seq.
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

// Loads the current profile from its newest valid slot, or the defaults.
static void load_profile(void)
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

void store_init(uint8_t first_profile)
{
    compute_defaults_print();
    profile         = (first_profile < STORE_PROFILES) ? first_profile : 0;
    pending_profile = NO_PROFILE;
    load_profile();
}

uint8_t store_profile(void)
{
    return profile;
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

    for (uint8_t sector = 0; sector < STORE_SLOT_SECTORS; sector++) {
        store_hook_erase((uint16_t)(base + sector * STORE_SECTOR_BYTES));
    }

    uint16_t crc = 0xFFFFu;
    uint16_t at  = 0;

    while (at < PAYLOAD) {
        const uint8_t n = (PAYLOAD - at > CHUNK) ? (uint8_t)CHUNK : (uint8_t)(PAYLOAD - at);

        for (uint8_t i = 0; i < n; i += 2) {
            const uint16_t word = word_get((uint16_t)((at + i) / 2));

            chunk[i]     = (uint8_t)(word & 0xFFu);
            chunk[i + 1] = (uint8_t)(word >> 8);
        }

        for (uint8_t i = 0; i < n; i++) {
            crc = crc16_byte(crc, chunk[i]);
        }

        store_hook_program((uint16_t)(base + HEADER + at), chunk, n);
        at += n;
    }

    // last: until this lands the slot is not a valid profile
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
    chunk[14] = profile;
    store_hook_program(base, chunk, HEADER);

    active_slot = target;
    active_seq  = seq;
}

bool store_request(uint8_t index)
{
    if (index >= STORE_PROFILES) {
        return false;
    }

    pending_profile = index;
    return true;
}

static void switch_profile(uint8_t index)
{
    if (index == profile) {
        return;
    }

    store_flush(); // the edits of the profile being left
    profile = index;
    load_profile();
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

    if (pending_profile != NO_PROFILE) {
        const uint8_t index = pending_profile;

        pending_profile = NO_PROFILE;
        switch_profile(index);
        return;
    }

    if (dirty && (uint16_t)(now - last_edit) >= STORE_QUIET_MS) {
        store_flush();
    }
}
