# Oblio — how the firmware is put together

*The short version. The detailed one, module by module, is [design.md](design.md) (Italian); every function is in [api.md](api.md).*

One board (Yunzii B68: SH68F90A, 8051 core at 24 MHz, 64 KB flash, 4 KB of XDATA, BK3632 radio), one hex. Code is static: no `malloc`, no `#ifdef`s
for other boards. Everything that does not need a register is plain C that also runs on the host under `gcc`, so it is unit-tested.

## Layers

| Directory | What it holds | Tested by |
| --- | --- | --- |
| `core/` | hardware-independent logic: matrix debounce, layers, tapping (LT/MT/tap dance), reports, macros, accented letters, keymap store, settings, VIA/Vial/diagnostics protocols, USB descriptors and control-endpoint state machine, radio protocol and link logic, backlight effects, sleep timing, encoder decoder, timer-slot sequence | `tests/host/` (`make test`, plain gcc, `-Wall -Wextra -Werror`) |
| `hal/` | the SH68F90A's registers and drivers: GPIO, clock, delays, Timer 2 slot interrupt, USB controller, flash self-programming, PWM, power-down | the simulator (`make sim`) and the hardware |
| `board/` | what is specific to the B68: pinout, matrix scan, LED PWM channels, radio serial port and pins, mode selector, Win/Mac switch (`os_switch`, which picks between two pairs of layers: 0/1 on Windows and 5/4 on Mac), sleep sequence, the default keymap, the custom keycodes, the Vial definition | `check_vial.py`, the simulator, the hardware |
| `tools/` | build helpers (Vial definition generator, budget, interrupt and Vial checks) and `diag.py` | `make check` |

`core/` never touches hardware. When it needs something from the outside it declares a **hook**, a function the board (or a test) defines:
`report_hook_send()`, `store_hook_read/erase/program()` (and `store_hook_flush_begin/end()`, which let the board stop the timer and the backlight once
around a whole save), `radio_hook_*()`, `usb_hook_*()`, `layer_hook_mods_add()`… A test defines the hook with a
recorder or a simulated device; the firmware defines it in `hal/` or `board/`. Time always comes in as an argument (`now`, in ms), never from a
timer the core reads itself.

## Data flow

```
 timer interrupt ── scan slot ──► matrix_hw_scan ──► raw[16] ──► tick_scan_take ──► matrix_feed (debounce) ──► events
                  └─ LED slot  ──► led_hw_subframe (one column of the backlight)

 events ──► keyboard_event ──► tapping_intercept ──► layer_key_down ──► tapping_event ──► run_keycode
              (core/keyboard.c)                                            │
                                         layer keys / macros / custom keys ┤
                                                                           ▼
                                      send_keycode ──► report state (core/report.c) ──► report_hook_send
                                                                                            │
                                         link_wireless() ?  radio queues (core/radio.c)  ◄─┴─►  USB queue (core/usb_reports.c) ──► EP1
 raw HID (EP2) ──► raw_hid ──► via_receive ──► keymap / macros / tap dances / settings / diagnostics (0xF1, read-only)
```

## Timing: the timer interrupt owns the columns

The 16 column lines are the key-matrix strobes **and** the sinks of the backlight LEDs, so a key scan and a backlight subframe can never overlap. Timer 2
plays the sequence of `core/sched.c`: four LED slots, then a scan slot, over and over. **Every slot is timed from the start of its own interrupt**, as in
the SMK port: the interrupt restarts the timer for the slot that begins (400 µs for an LED slot; for a scan, a safe maximum of 1000 µs), and when a scan has
finished it forces the overflow so that the next LED slot starts at once. An LED slot is therefore always a full 400 µs, however long the scan before it took
(a fixed grid of slots cut the LED slot after a long scan short, and one backlight column in five lit for a fraction of its time: a visible flicker on USB).
The scan (`matrix_hw_scan`, 4638 instructions in the simulator; each column touches only its own port) takes about 560–610 µs on the keyboard, so a backlight frame is about 9.6 ms and a key is sampled about every 2.2 ms.

The millisecond clock is the sum of the time that really passed between interrupts, in timer counts (2000 per ms): the nominal slot length (LED) or the
measured scan, plus the interrupt latency read from the counter. The interrupt only copies pre-computed values and writes registers; effects, debounce and
everything else are done from the main loop. A scan that the main loop does not collect before the next one is simply overwritten: the matrix is a state, so
no key is lost (the SMK port works the same way); it only takes a few more scans to debounce.

Instruction counts are exact in the simulator, time is not: on this core the interrupt code runs at about 3.8 clock cycles per instruction, so budget the
interrupt work by instructions (`make sim` has a guard) and by the durations the diagnostics report, never by guessing.

## Concurrency rules

- Interrupts: Timer 2 (slots), USB, the serial port of the radio (highest priority: a byte lasts 38 µs), INT4 (wake-up from power-down). The main loop is a flat
  polling loop; nothing blocks except the flash operations (the timer and the PWM are stopped around them) and the radio's sleep frames.
- Data shared with an interrupt has **one writer**, is a single byte or is read twice until it agrees, and is handed over through a flag. No interrupt calls a function that
  the main loop also calls (`delay_us_nokick` for the scan, for instance).
- The build uses `--stack-auto`, which keeps locals on the stack; `tools/check_interrupts.py` makes sure SDCC's shared static overlay stays empty, because an overlay
  would let an interrupt overwrite main-loop variables. It also checks that exactly the four intended vectors exist.
- The watchdog is kicked from the main loop and from the delays; the scan's own wait does not kick it, so a stuck main loop still lets it reset the chip.

## Memory

**Cable and charger.** `board/battery.c: battery_cable()` says whether the keyboard is powered from outside (USB mode; a saturated battery reading; a host that has configured it; the charger lines read by `board/charger_hw.c` and debounced in `core/charger.c`). Sleep, the battery warning and the gauge all ask it, not the USB host state, which is "gone" whenever the PC suspends the port. The raw HID pipe (Vial, diagnostics) is armed from the device's own configuration state for the same reason.

Application flash ends at 0xDC00 (0xDA00 when built with `BATTERY_LOG=1`, which is off by default since 0.10.0); the battery log (optional, one sector, `core/blog.c`) sits at 0xDA00-0xDBFF. After it: the keymap store (two copies of four 512-byte sectors, each with a 69-record log of word edits in its free tail, so most saves erase nothing) up to 0xEBFF, the settings log at
0xEC00 as a log of 36 small records (sleep, Bluetooth slot and the lighting; 14 bytes each) (a save appends to the first erased one, so a power cut during a save keeps the previous settings and the sector
is erased once every 36 saves), the bootloader's marker sector at 0xEE00 **untouched**,
the stock bootloader from 0xF000 (never written: `hal/flash.c` refuses any address outside 0xDC00–0xF000). `make size` reports flash and XDATA use against 56320 and 4096
bytes. Pointers into XDATA are typed (`XDATA uint8_t *`, `core/platform.h`) in the packet paths: a generic pointer costs three bytes and a slow access on this
core and was worth 1.5 KB of flash. Constant tables are `ROM`.

## Where the unknowns are

Everything in the hook layer below `core/` has been written from the stock firmware's dump and the SMK port, and is **not yet tried on the hardware**: the exact
duration of the interrupt work, the stack depth with every interrupt busy (91–104 bytes of 223 in the simulator), the PWM output, the radio serial port, the sleep sequence.
`docs/first-boot.md` says how to flash a build and what to look at, and `tools/diag.py` reads the counters that answer the questions.

## Testing

| What | How | Cost |
| --- | --- | --- |
| Logic | `make test` (about 23 programs, plus the Vial definition check) | seconds |
| Linked firmware | `make check-interrupts`, `make size` | seconds |
| The real binary on a simulated chip | `make sim` (needs `nix develop`): boot, timer slots, stack, the recovery chord, every USB descriptor, VIA/Vial over the raw HID pipe, key reports, the ISP command | 30 s |
| Hardware | `docs/first-boot.md` | the owner |

The simulator's Timer 2 runs at a virtual pace, so it shows the *order* of events and the state of the chip, not durations.
