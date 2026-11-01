# Oblio

Open firmware for the **Yunzii B68** keyboard (Sinowealth SH68F90A microcontroller, BK3632 wireless radio).

Oblio is written for a single board and a very small target: 43 KB of application flash and 4 KB of XDATA. It aims to offer what a
QMK/Vial keyboard offers, without the weight of a generic framework.

## Features

- Live keymap editing with the [Vial](https://get.vial.today) desktop app (VIA/Vial raw HID protocol)
- Up to 8 layers, momentary/toggle/one-shot layer keys, layer-tap and mod-tap with a millisecond-accurate tapping term
- Macros, tap dance, rotary encoder with a per-layer map
- One complete keymap in flash (keys, encoder, macros, lighting, tapping term), live-edited from Vial
- Mouse keys: the encoder can scroll, a key can click (`KC_MS_*`, USB only)
- Per-key RGB backlight with several effects, flicker-free
- USB, 2.4 GHz and Bluetooth (three pairing slots) from the hardware mode switch
- Low-power sleep on battery with a configurable inactivity time
- A live-state API for desktop overlays (the keyboard HUD protocol) and a battery gauge on the number keys

## Building

The toolchain is pinned with [Nix](https://nixos.org); [SDCC](https://sdcc.sourceforge.net) compiles for the 8051 core.

```
nix develop                  # shell with the toolchain, the flashing tool and the chip simulator
make test                    # unit tests of the platform-independent logic, run on the host
make fw size                 # the firmware (build/oblio.hex) and its flash / XDATA budget
make check                   # the Vial definition and the interrupt vectors of the linked firmware
make sim                     # the real firmware in a simulator of the chip: boot, USB, raw HID, recovery
make stages                  # builds that start less of the firmware, for the first runs on the hardware
```

## Flashing

Flashing uses the chip's USB ISP through [sinowisp](https://github.com/karstx/sinowisp):

```
sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f oblio.hex
```

The stock bootloader stays untouched. If a firmware misbehaves, hold **Esc + 1 + 2** while plugging in the USB cable to enter it.
Details on recovery, including the hardware programming header, are in [docs/recovery-icp.md](docs/recovery-icp.md).

## Repository layout

| Path | Content |
| --- | --- |
| `hal/` | register definitions and drivers for the SH68F90A |
| `core/` | hardware-independent logic (matrix, layers, tapping, reports, mouse, macros, the keymap store), unit-tested on the host |
| `board/` | everything specific to the B68: pinout, lighting, radio, sleep |
| `docs/` | hardware notes, radio protocol, lighting analysis |
| `tests/` | host unit tests (`tests/host/`) and simulator tests (`tests/sim/`) |
| `tools/` | build helpers, checks, the chip simulator patch and `diag.py` |

## Documentation

- [How the firmware is put together](docs/architecture.md)
- [First run on the keyboard](docs/first-boot.md)
- [Keyboard HUD protocol](docs/hud-protocol-v2.md)
- [Board and pinout](docs/keyboards/yunzii-b68.md)
- [BK3632 radio protocol](docs/keyboards/yunzii-b68-radio.md)
- [Stock firmware analysis](docs/keyboards/yunzii-b68-firmware-stock.md)
- [Lighting driver](docs/led-probe.md)
- [Recovery and programming header](docs/recovery-icp.md)

## Credits

Oblio builds on the groundwork of [SMK](https://github.com/karstx/smk), and on the QMK and Vial protocols. See [NOTICE.md](NOTICE.md).

## Licence

GPL-2.0-or-later. See [LICENSE](LICENSE).
