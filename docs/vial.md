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

## Vial RGB (per-key lighting)

Advertised by byte `[12] == 1` in `vial_get_keyboard_id`. It does **not** use the `0xFE` prefix: it rides on VIA's own lighting commands, with the
sub-command in `data[1]` and the arguments from `data[2]`.

| VIA command | Value | What arrives |
| --- | --- | --- |
| `id_lighting_get_value` | 0x08 | a vialrgb sub-command in `data[1]` |
| `id_lighting_set_value` | 0x07 | the same, in the other direction |
| `id_lighting_save` | 0x09 | "write the lighting settings down" |

| Sub-command | Value | Request | Reply |
| --- | --- | --- | --- |
| `vialrgb_get_info` | 0x40 | — | protocol version 1 in `[0..1]`, maximum brightness in `[2]` |
| `vialrgb_get_mode` | 0x41 | — | the effect id in `[0..1]`, speed `[2]`, hue `[3]`, sat `[4]`, val `[5]` |
| `vialrgb_get_supported` | 0x42 | the last id in `[0..1]` | the ids above it, 16-bit little-endian, `0xFF`-filled |
| `vialrgb_get_number_leds` | 0x43 | — | the number of LEDs in `[0..1]` |
| `vialrgb_get_led_info` | 0x44 | the LED index in `[0..1]` | x, y, the flags, then the matrix row and column (0xFF,0xFF for a position that is not a key) |
| `vialrgb_set_mode` | 0x41 | effect id `[0..1]`, speed `[2]`, hue `[3]`, sat `[4]`, val `[5]` | — |
| `vialrgb_direct_fastset` | 0x42 | the first LED index `[0..1]`, the count `[2]`, then three bytes (h, s, v) per LED — up to 9 LEDs in a packet | — |

The effect ids are a fixed table (the app knows their names): 0 off, 1 direct (the per-key colours the app paints), 2 solid colour, 4 gradient up-down,
5 gradient left-right, 6 breathing, 7-12 band, 13-20 cycle, 21-29 beacon/pinwheel/raindrop, and so on. A firmware advertises only the ones it has.

**What it would take here**: the keyboard's own lighting is an effect engine with a colour wheel, not QMK's `rgb_matrix`, so the ids have to be
mapped onto the effects in `core/led_effect.c` (off, solid, up-down, left-right, breathing, radial) and an HSV rendering path added for hue/sat/val
to mean anything. Per-key colours (id 1) need 3 bytes an LED: 201 bytes of XDATA for the 67 keys, and as much again in the keymap slot to survive
power-off — and this firmware has 136 bytes of XDATA and 14 bytes of slot left. That is the decision to make before writing it: 6 layers instead
of 8 frees 320 bytes of each.
