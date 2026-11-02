# Oblio firmware — report for another agent

Written 2026-11-01 from the state of `main`. Read this first; then `CLAUDE.md` (working rules, in Italian), `docs/architecture.md` (how it is put
together), `docs/first-boot.md` (procedure on the keyboard), `CHANGELOG.md` (history, in Italian). Where this file says **verified** it means measured
on the keyboard or by a test that runs; **hypothesis** means reasoned but not proven.

## 1. What this is

Oblio is new firmware, written from scratch in C (SDCC), for **one board only**: the **Yunzii B68** keyboard (66 keys + a rotary encoder with a push
button). It does what the stock firmware does plus Vial/VIA live configuration, and is meant to replace the SMK port
(`~/dev/smk`, branch `port/yunzii-b68`), which is the source of truth for the hardware and for behaviour. It is a clean rewrite, not a copy: SMK is
credited in `NOTICE.md` and `README.md`. License GPL-2.0-or-later. Repo: github.com/wildzer0/oblio_kbfw (private), branch `main`.

## 2. Hardware (what constrains the code)

| Item | Value |
| --- | --- |
| MCU | Sinowealth **SH68F90A**: 8051 core, 1 clock per cycle, 24 MHz from the PLL, 64 KB flash, **4096 B XDATA**, USB device, UART, Timer 2, PWM |
| Radio | **BK3632** on a serial port (UART0, 260870 baud, `P5.5/P5.6`, select `P0.2`, attention `P4.7`) for 2.4G and Bluetooth (3 BT slots) |
| Matrix | 16 columns (strobes, active low) × 5 rows (inputs with pull-up). The column lines are **also the sinks of the RGB backlight**, so a key scan and a backlight subframe can never overlap |
| Backlight | RGB under the keys, 15 PWM channels (3 colours × 5 key rows), one column lit at a time |
| Other | encoder on `P0.6/P0.7`; mode selector near the USB-C port on `P0.4` (2.4G) / `P0.3` (Bluetooth), both high = USB; **Win/Mac switch on `P0.5`** (input with pull-up, **high = Mac**, low = Windows; measured on the keyboard 2026-11-01 and confirmed against the stock firmware, see §3), which selects the default layer; `P7.0` is read by the stock with the matrix rows but is not a row and does not change with the switch |
| Flash map | application up to **0xDC00** (56320 B budget); the keymap store (2 copies × 4 sectors) 0xDC00–0xEBFF; settings at 0xEC00 (a log of 56 records: a save appends, it does not erase); 0xEE00 is the
bootloader's marker sector and is **not touched**; **stock bootloader at 0xF000, never written** (`hal/flash.c` refuses addresses outside 0xDC00–0xF000) |
| Recovery | hold **Esc+1+2 while plugging the USB cable**: the firmware jumps to the stock ISP bootloader before USB or radio start. There is **no ICP programmer** — this is the only way back |
| Flashing | `sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex` |

`usb_device_configured()` means what it says: the cable is in **and** a host is driving the bus. Nothing on this chip reports a cable being pulled
out (`configured` is only cleared by another `SET_CONFIGURATION` or a bus reset), so the firmware watches the start-of-frame interrupts: if none has
come in 250 ms, the host is gone. Without that the keyboard believed for ever that it was plugged in (and the battery gauge, which only reads on battery, never did).

The USB device enumerates as `258a:dead`, product name "Oblio B68", with a keyboard interface (EP1) and a raw HID interface (usage page 0xFF60, EP2,
32-byte packets) used by Vial, VIA, the HUD app and the diagnostics. On interface 0 the report IDs are: keyboard 3, system 1, consumer 2,
**mouse 7** (five buttons, X and Y in 16 bits, wheel — as the stock firmware had it) and the ISP feature report 5.

## 3. What the firmware does

- **Win/Mac switch:** `board/os_switch.c` reads `P0.5` (debounced 50 ms) and makes layer 7 the default on Mac, layer 0 on Windows. That is not just
  a modifier swap: the two sides are **two complete keyboards**, each with its own Fn layer (Windows: layers 0 and 1, Mac: layers 5 and 4, the Fn
  key of the Mac side going to 6), both fully editable in Vial. The Mac base is a copy of the Windows base with Alt and GUI swapped; its Fn layer
  starts as a copy of the Windows one, and changing it changes nothing on the Windows side. This is what the stock firmware does with its second
  set of keymap tables. Switching does not write to flash: the switch is the source of truth.
- **Mouse keys** (`core/mouse.c`): the QMK `KC_MS_*` keycodes (0xCD–0xDF, so Vial knows them) move a pointer: a direction held steps every 8 ms and
  accelerates, `MS_ACCEL1/2` make the step longer, the wheel scrolls and buttons 1-5 hold their bit. That is what makes the encoder a scroll wheel:
  assign `MS_WH_UP` / `MS_WH_DOWN` to its two directions in Vial. **USB only**: the radio protocol has no mouse frame, so in 2.4G and Bluetooth
  those keycodes do nothing (the report is dropped in `board/transport.c`).
- **Keys:** matrix scan + debounce (**a time: 5 ms**, as QMK does with its `DEBOUNCE`, with a floor of three agreeing samples), 6 layers in RAM (`KEYMAP_LAYERS`), layer keys, tap-hold (LT/MT/OSM/OSL), tap dance, macros, encoder.
- **NKRO on USB:** the keyboard report (ID 3) is the bitmap — **112 usages**, not QMK's 120, because the chip's EP1 buffer is 16 bytes and the
  report ID takes one. Boot protocol still gets the 8-byte six-key report, so a BIOS works.
- **Caps Word** (`QK_CAPS_WORD_TOGGLE`, offered by the Vial app because the firmware advertises the feature bit): the letters are shifted until the
  word ends — at the first key that is not a letter, a digit, minus, backspace or delete.
- **Leader** (`core/leader.c`, the key `B68_LEADER` assignable in Vial): press it, then a sequence of keys, and the sequence acts instead of being
  typed; 400 ms of silence abandons it. The sequences are written in `board/leader.c` — compile-time, exactly as QMK does it.
- **Key overrides** (`core/override.c`, Vial's Key Overrides tab): up to 4. A trigger keycode under the modifiers it requires becomes a
  replacement (Shift+Backspace = Delete, say), optionally on a set of layers only, with modifiers that must not be held and modifiers to take
  away while it acts; the trigger itself never reaches the host and its release releases the replacement. The "when does it activate" options
  are read and ignored (none of them changes the everyday use).
- **Combos** (`core/combo.c`, Vial's Combos tab): up to 8 chords of up to 4 keycodes each giving one other keycode. A key that is an input of a
  configured combo is held back for at most 50 ms waiting for the rest of the chord; if it closes, the keys are dropped and the output is pressed
  (released when they are let go), if not they go out as usual. Exact matches only, one chord at a time. A key outside every chord pays nothing.
  Default keymap in `board/keymap_default.c`: layer 0 = ANSI-ish B68 layout with the encoder push as tap dance 0 (tap mute, hold play/pause, double tap next
  track) and the encoder turning volume; layer 1 (Fn) as below.
- **Fn layer (hold Fn):** F1–F12 on the number row; lighting on Q…Y (effect −/+, brightness −/+, speed −/+), plain colour −/+ on U I; sleep time −/+ on O P;
  sleep now on Tab (`B68_SLEEP`); Bluetooth slots 1–3 on B N M (tap selects, hold 3 s pairs); battery level on Backspace;
  media on the arrows. Custom keycodes live in `board/b68_keys.h` (0x7E00…0x7E13, same numbers as the SMK port so saved layouts keep their meaning).
- **Vial / VIA over raw HID:** live keymap, macros, tap dances, combos, key overrides, **lighting** (the Lighting tab: six of this keyboard's
  effects, brightness, speed, colour — the app has no per-key painter, see `docs/vial.md`), encoder; Vial unlock = hold Esc+Enter. Definition from `board/vial.json` (generated into the
  firmware by `tools/vial_gen.py`, checked by `tools/check_vial.py`).
- **HUD protocol v2** (`docs/hud-protocol-v2.md`, command `0xF0`): another app of the user's reads layers, pressed keys and the keymap. `B68_HUD_TOG`
  (0x7E00) bumps a counter the app watches.
- **The keymap in flash:** one keyboard (keys, encoders, macros, tap dances, lighting settings) in two copies with a CRC; it survives power-off and
  a power cut during a save. There were four switchable profiles until 2026-11-01: they cost 16 KB of the chip's flash for layouts nobody used, and
  with one keyboard the store moved to the end of the flash and the code got those 12 KB back (43935 → 43528 B used, 56320 B available).
- **Backlight:** effects, brightness, speed, colour — the colour of the plain effect is what a picker means by a colour, **a hue and a
  saturation in one setting** (hue in the low byte, saturation in the high one, 0 = white), so a colour picked in the Vial app is the colour that
  comes out, pastels included; the wheel is the HSV one, red-yellow-green-cyan-blue-magenta (it used to run red-blue-green, which turned an orange
  into a purple), **or the per-key picture painted in Vial** (`FX_VIAL`), all driven from the main loop into a framebuffer; the timer interrupt only copies pre-computed PWM widths
  (`core/led.c` keeps a ready-made `duty2` table, `board/led_hw.c` writes it with constant-address code). **Battery gauge** (Fn+Backspace, number keys
  green/red for 3 s): the battery is a **10-bit ADC reading** in bytes 4-5 of the radio's status answer (`core/radio.h`), turned into a percentage
  by a Li-Po discharge curve (`board/battery.c`: reading 800 = 3.30 V = 0 % ... 1020 = 4.20 V = 100 %; the cell is 5000 mAh, 4.2 V charge). The scale
  (~243 counts to the volt) rests on one measurement, 4.0378 V at rest = reading 981 - **provisional**. With the cable the reading saturates
  (1023), so the gauge ignores it and shows the last reading it took on battery; just after power-on the ADC reads 1-3, which is ignored too (the gauge
  says "unknown", ten white keys blinking). **The host Caps Lock is shown on its key in the wireless modes too**, from byte 1 of the same answer.
- **Wireless:** 2.4G and Bluetooth through the BK3632 (`core/radio.c`, `core/link.c`, `board/radio_hw.c`): frames, flow control, mode, slots, pairing.
  The mode selector is read with a 50 ms stability filter. Keys go to the radio queues when `link_wireless()`, otherwise to the USB queue.
- **Sleep:** in wireless modes only, after 1/5/10/30 min or never (default 10), or on demand; power-down with INT4 wake-up. Never sleeps on USB.
- **Diagnostics** (`core/diag.c`, VIA command `0xF1`, `tools/diag.py`): counters readable **without** the Vial unlock (they are for when the keys
  do not work).
- **Safety:** watchdog (kicked from the main loop and from `delay_us`, not from the scan wait), recovery chord at the start of `main`, flash writes confined to
  0xDC00–0xF000.

## 4. Structure of the code

```
core/   pure C, no hardware, unit-tested on the host with gcc (hooks `*_hook_*` for everything external; time comes in as `now` in ms)
hal/    SH68F90A registers and drivers: tick (Timer 2), usb, flash, power, clock, delay, gpio, pwm, watchdog
board/  B68 specific: pinout, matrix scan, LED PWM, radio hardware, selector, sleep, default keymap, custom keys, main.c, vial.json
tools/  vial_gen.py, check_vial.py, check_interrupts.py, budget.py, diag.py
tests/host/  22 test programs (plain gcc, -Wall -Wextra -Werror)       tests/sim/  simulation of the real hex in a patched ucsim
docs/   architecture.md, first-boot.md, hud-protocol-v2.md, vial.md (the Vial protocol as the app speaks it), keyboards/ (pin maps, stock firmware
       analysis, radio protocol), recovery-icp.md
```

`board/main.c`: `init()` (clock → GPIO → recovery check → matrix/keyboard/settings/store/encoder/LED/radio/sleep → `tick_init` → USB → `EA=1`), then a flat
polling loop: `watchdog_kick`, `tick_alive`, collect milliseconds, collect a scan → `matrix_feed`, `encoder_poll`, key events → `keyboard_event`,
`keyboard_task`, `store_task`, `settings_task`, `usb_task`, `led_task`, `led_app_task`, `link_task`, `sleep_hw_task`.

Interrupts (exactly four vectors, checked by `tools/check_interrupts.py`): **Timer 2** (slot scheduler: LED subframes and key scans), **USB**, **UART0** (radio,
highest priority), **INT4** (wake-up). Shared data has one writer or is read twice until it agrees; `--stack-auto` locals, no static overlay allowed.

**Where the key latency comes from** (measured and compared with QMK): a scan costs ~510 µs on the keyboard (16 columns, 5 µs of settle before
each of the two readings) and comes every ~2.4 ms; the debounce is 5 ms, as QMK's default; the USB interval is 1 ms. So a press shows up after
about 5–9 ms, against QMK's ~6–7 ms — and the debounce is now a *time*, so the scan rate can change without the latency paying for it (it was a
count of scans until 2026-11-02).

**Timing model (important, it caused the two hardware failures below):** Timer 2 plays 4 LED slots (400 µs each, timed from the start of their own interrupt)
and then 1 scan slot; the interrupt restarts the timer for the slot that begins and forces the next one as soon as a scan ends, so a long scan never cuts an
LED slot short. Milliseconds are the sum of the time that really passed (2000 timer counts per ms). Details: `docs/architecture.md`, `core/sched.[ch]`,
`hal/tick.c`.

## 5. Build, test, budget

All inside `nix develop` (SDCC 4.5, gcc, python, `ucsim-sh68f90` from the flake):

```
nix develop --command make test              # 27 host tests + Vial definition check
nix develop --command make fw size           # build/oblio.hex ; flash and XDATA against 56320 / 4096
nix develop --command make check-interrupts  # the four vectors, no static overlay
nix develop --command make sim               # ~50 s: the real hex in the chip simulator (24 tests)
nix develop --command make stages            # staged builds, see below
```

Current budget: **flash 51827 / 56320 (4.4 KB of margin), XDATA 3755 / 4096 (341 B free), keymap slot 1709 of 2048 bytes.** Six layers since
2026-11-02 (8 → 6 freed 320 B of XDATA and 320 B of slot); the Mac pair moved from 7/6 to 5/4. The per-key colour buffer went away again with the
painter (the app has none), which is why the slot has room for what comes next.; the stack is the other tight one (internal RAM, 223 B
from 0x21 up; the USB interrupt used to keep 48 B of buffers on it, now static in XDATA). Check the `flash` line before every commit; a failed link
leaves no stale `.ihx` any more (`.DELETE_ON_ERROR`). SDCC pitfalls seen here: `%`, `/` and loops with a variable count are very costly; use typed `XDATA` pointers
(`core/platform.h`), constant addresses in interrupts, `--std-c23 --Werror` (an empty `if` body is an error 110).

**Staged builds** (`OBLIO_STAGE=1..4`, `board/stage.h`) for bring-up on the keyboard: 1 matrix + USB + Vial; 2 + backlight; 3 + radio and selector; 4 = release
(+ sleep). `make stage N=2` → `build/stage2/oblio.hex`. CI (GitHub Actions) runs host tests, build, budget, vector check, stages and the simulation, uploads
`oblio.hex` and the stages as artifacts, and publishes a Release on a `v*` tag.

**Simulator caveat:** `make sim` runs the real firmware but its Timer 2 is a virtual flag — it proves *order* of events and **exact instruction counts**, not
durations. It has a guard on instructions per interrupt (backlight subframe < 1200, scan < 8500; today 433 and 8305).

## 6. State of verification

| Area | State |
| --- | --- |
| Host logic (27 test programs), Vial definition, vectors, simulator (24 tests), CI | **verified**, green |
| Keyboard on USB: typing | **verified by the user**: responsive, no visible latency |
| Keyboard on USB: backlight | **verified by the user**: stable after the timing fix (it flickered before) |
| 2.4G and Bluetooth | user reported "everything works normally" without the cable, on a build before the final timing change; **re-check on the current hex** |
| Vial app, HUD app, the keymap surviving power-off, encoder, tap-hold/macros on the keyboard | **not yet tested** on hardware |
| Win/Mac switch | **verified by the user**: the switch changes side, and the polarity is the one measured on the keyboard (Mac = `P0.5` high) |
| Sleep | **tested by the user**: it sleeps and wakes, but the LEDs stayed dark until a power cycle (cause found and fixed on 2026-11-01: `led_hw_init()` dropped the backlight permission and `led_app_task()` compared it against its own copy). Re-test pending |
| Battery gauge | reading and scale **measured** (981 at 4.0378 V); the curve is provisional until a second point (the reading right after taking the charger off, expected ~1015-1020) is taken |

## 7. What went wrong on the hardware, and why (read before touching interrupts)

1. **Stage 2 (backlight) — LEDs blinked, no key worked.** Cause: the timer interrupt work did not fit its slots. In the simulator a backlight subframe cost 4697
   instructions and a scan 8963; slots were 400/450 µs, so the main loop was starved (and then the watchdog fired). Fix: precomputed `duty2` table,
   constant-address unrolled interrupt code (now 433 instructions per subframe), a back-off that makes the backlight step aside if the main loop is starved
   (`hal/tick.c`), counters readable without the unlock.
2. **USB mode — heavy flicker (wireless fine).** Diagnostics showed that *every* scan ran past its fixed slot (a scan really takes about 610 µs, not the ~440 I
   had estimated), so the LED slot after each scan started late and lit its column for ~100 µs instead of 400. The SMK port avoids this by timing each slot
   from its own interrupt; Oblio now does the same (`core/sched`, `hal/tick.c`). Hardware result: slots late 0, back-off 0, LEDs stable.
3. **"Scans lost" is not a fault.** About a third of the scans are overwritten before the main loop collects them. The matrix is a state (SMK identical): no key
   is lost, debounce just takes a few ms more. The counters are labelled "(harmless)".

## 8. Diagnostics (`tools/diag.py`)

Works without unlocking Vial. Linux: `/dev/hidraw`, no dependencies. **Windows/macOS: `py -m pip install hidapi`**, then `py tools\diag.py [--watch 1]`
(keep the script in sync with the repo: an old copy prints wrong labels, because the order of the counters changes). Counters, in order: reset cause
(RSTSTAT, 0 = power-on), stack used (bytes of 223), LED slots late (0), slowest main-loop pass (ms), USB report queue overflows, backlight back-offs (rare),
scan slots late (0), longest sector erase (µs), frames received from the radio. `--radio` (no unlock) dumps the radio's six status bytes and the battery reading and
percentage they carry. The 32-byte packet holds at most **14** counters (nine today).

## 9. Open items, in priority order

1. **Functional test on the keyboard** per `docs/first-boot.md`: Vial app connects and saves a change, HUD app connects, the keymap surviving power-off, the encoder, 2.4G/BT with the
   current hex, then sleep. Report any failure with `diag.py --watch 1` output.
2. **Win/Mac switch — done (2026-11-01).** It is **`P0.5`**, an input with a pull-up, low on Mac: measured with a port readout (since removed; one bit
   different between the two positions, no dependence on the columns) and confirmed in the stock dump (the debounce at `0xA9D9`, the state in
   `XDATA[0x0307]`, used to pick between two sets of keymap tables). It now selects the default layer: 0 on Windows, `KEYMAP_MAC_LAYER` (7) on
   Mac, which is a copy of the base with Alt and GUI swapped. **To be tried on the keyboard**: on Mac the key labelled Win must send Command
   and the one labelled Alt must send Option, and Vial must show layer 7 editable. `P7.0` stays unused (the stock reads it with the rows but it
   is not a row and it does not follow the switch).
3. **Battery percentage — decoded, scale provisional.** Bytes 4-5 of the status answer are a 10-bit ADC reading (found in the stock firmware: the status
   handler at `0x05EA` builds `XDATA[0x02E6..7]` from them and the gauge routine at `0x7CF0` uses `(reading - 715) * 10 / 22`). Measured: 4.0378 V at rest
   reads 981, about 243 counts to the volt; the cable saturates it. Oblio uses a Li-Po curve rather than the stock straight line (which calls
   everything above ~3.85 V full). To confirm: the reading just after the charger comes off should be ~1015-1020.
4. **The scan takes ~610 µs instead of the ~440 estimated** (32 busy-waits of 10 µs are 320 µs of it). Unexplained, harmless now; the clock check says the
   timer is right. If it were found, the backlight frame would go from ~9.6 to ~8 ms.
5. **The main loop is slow**: 0–5 passes per scan period; a pass is ~1170 instructions in the simulator, 37 % of it `matrix_any_pressed()` called twice per pass
   (`store_task`, `settings_task`). Typing latency is not visible, so no action required; calling it once would save ~215 instructions. A counter of passes per
   second would give the real pass time.
6. **Flash margin is 12.8 KB** (2026-11-01: the four profiles went and the store moved to the end of the flash). The next features fit; the limit to
   watch is now XDATA (324 B free) and the stack.
7. **What is still missing, in the order a user would notice it:** the host Caps Lock is done (status byte 1); combos, key overrides and per-key RGB (vialrgb) from Vial — the
   key overrides (the entry layout is in `docs/vial.md`, the tapping term and the combos show the way); more than six keys at once on USB (no NKRO
   report, the radio has one).

## 10. Working agreements with the user

Answer in **Italian**. The assistant writes all code and docs; the user only tests on the keyboard. Non-trivial work goes through **plan mode** and approval
first. **No blind attempts on the hardware**: reason from the stock dump and SMK, instrument, and say what is hypothesis and what is verified. **Commit and push
at the end of every step**, update `CHANGELOG.md` every time, end commits with the `Co-Authored-By` line given in the session. `.vscode/` is untracked;
`tests/sim/__pycache__` should not be committed. `README.md` is for the public (English, no milestones); project state lives in `CHANGELOG.md` and `CLAUDE.md`.
