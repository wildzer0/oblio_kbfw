# The Vial protocol, as Vial speaks it

Read from the reference implementation (Vial-QMK, `quantum/vial.h` and `quantum/vial.c`, GPL-2.0-or-later) on 2026-11-02, because the parts Oblio
still has to implement — combos, key overrides, per-key RGB — are not guessable and a wrong opcode would corrupt what Vial already edits.

Every request and reply is a **32-byte** packet on the raw HID interface (EP2, usage page 0xFF60). The first byte is `0xFE` and the second is the
command; the reply replaces the request in the same buffer. `VIAL_PROTOCOL_VERSION` is **6**.

## Commands (`data[1]`)

| Value | Name | Request | Reply |
| --- | --- | --- | --- |
| 0x00 | `vial_get_keyboard_id` | — | protocol version in `[0..3]` (little-endian 32-bit), the 8-byte keyboard UID in `[4..11]`, `[12] = 1` when vialrgb is supported |
| 0x01 | `vial_get_size` | — | the definition size in `[0..3]` |
| 0x02 | `vial_get_def` | page in `[2..3]` (le) | 32 bytes of the definition from `page * 32` |
| 0x03 | `vial_get_encoder` | layer `[2]`, encoder `[3]` | the two keycodes (big-endian): counter-clockwise `[0..1]`, clockwise `[2..3]` |
| 0x04 | `vial_set_encoder` | layer `[2]`, encoder `[3]`, direction `[4]`, keycode `[5..6]` big-endian | — |
| 0x05 | `vial_get_unlock_status` | — | `[0] = unlocked`, `[1] = unlock in progress`, then the (row, col) pairs of the unlock keys |
| 0x06 | `vial_unlock_start` | — | — |
| 0x07 | `vial_unlock_poll` | — | `[0] = unlocked`, `[1] = in progress`, `[2] = counter left` (50 × 100 ms of the keys held) |
| 0x08 | `vial_lock` | — | — |
| 0x09 | `vial_qmk_settings_query` | the last QSID in `[2..3]` | the QSIDs above it, `0xFFFF` terminated; all `0xFF` = none |
| 0x0A | `vial_qmk_settings_get` | QSID `[2..3]` | `[0] = status`, the value from `[1]` |
| 0x0B | `vial_qmk_settings_set` | QSID `[2..3]`, value from `[4]` | `[0] = status` |
| 0x0C | `vial_qmk_settings_reset` | — | — |
| 0x0D | `vial_dynamic_entry_op` | see below | see below |

## `vial_dynamic_entry_op` (`data[2]` is the operation)

| Value | Name | Request | Reply |
| --- | --- | --- | --- |
| 0x00 | `dynamic_vial_get_number_of_entries` | — | `[0]` tap dances, `[1]` combos, `[2]` key overrides, `[3]` alt repeat keys, `[31]` feature bits: bit 0 = Caps Word, bit 1 = Layer Lock |
| 0x01 | `dynamic_vial_tap_dance_get` | index `[3]` | `[0]` status, then the entry from `[1]` |
| 0x02 | `dynamic_vial_tap_dance_set` | index `[3]`, entry from `[4]` | `[0]` status |
| 0x03 | `dynamic_vial_combo_get` | index `[3]` | `[0]` status, then the entry from `[1]` |
| 0x04 | `dynamic_vial_combo_set` | index `[3]`, entry from `[4]` | `[0]` status |
| 0x05 | `dynamic_vial_key_override_get` | index `[3]` | `[0]` status, then the entry from `[1]` |
| 0x06 | `dynamic_vial_key_override_set` | index `[3]`, entry from `[4]` | `[0]` status |
| 0x07 | `dynamic_vial_alt_repeat_key_get` | index `[3]` | `[0]` status, then the entry from `[1]` |
| 0x08 | `dynamic_vial_alt_repeat_key_set` | index `[3]`, entry from `[4]` | `[0]` status |

A status that is not 0 means "no such entry".

## The entries (memcpy'd raw, so little-endian, packed)

```c
typedef struct {          // 10 bytes
    uint16_t on_tap;
    uint16_t on_hold;
    uint16_t on_double_tap;
    uint16_t on_tap_hold;
    uint16_t custom_tapping_term;
} vial_tap_dance_entry_t;

typedef struct {          // 10 bytes
    uint16_t input[4];    // up to four keycodes, 0 = unused
    uint16_t output;
} vial_combo_entry_t;

typedef struct {          // 10 bytes
    uint16_t trigger;
    uint16_t replacement;
    uint16_t layers;      // which layers it is active on (bitmask), 0xFFFF = all
    uint8_t  trigger_mods;
    uint8_t  negative_mod_mask;
    uint8_t  suppressed_mods;
    uint8_t  options;     // bit 0..5 the activation options, bit 7 = enabled
} vial_key_override_entry_t;

typedef struct {          // 6 bytes
    uint16_t keycode;
    uint16_t alt_keycode;
    uint8_t  allowed_mods;
    uint8_t  options;     // bit 0 default, 1 bidirectional, 2 ignore handedness, 3 enabled
} vial_alt_repeat_key_entry_t;
```

## What Oblio answers today

`core/vial.c` implements commands 0x00–0x08 and 0x0D with **tap dances, combos and key overrides**: operation 0 returns the tap dance count in
`[0]`, the combo count in `[1]`, the key override count in `[2]` and zero in `[3]` (no alt repeat keys), operations 1/2 read and write a tap
dance, 3/4 a combo, 5/6 a key override. The QMK settings commands answer the tapping term (QSID 7). The engines are `core/combo.c` (the keys of
a chord are held back for at most 50 ms, so the letters of the chord do not reach the host) and `core/override.c` (a trigger keycode under the
required modifiers becomes the replacement, with the modifiers it lists taken away while it acts).

Per-key RGB (vialrgb) is a separate interface, not this one: it is advertised by byte `[12] == 1` in `vial_get_keyboard_id` and uses its own raw
HID channel (this keyboard has one free report ID range on interface 0 or could use the raw HID interface).
