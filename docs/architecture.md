# Oblio — how the firmware is put together

One board (Yunzii B68: SH68F90A, 8051 core at 24 MHz, 64 KB flash, 4 KB of XDATA, BK3632 radio), one hex. Code is static: no `malloc`, no `#ifdef`s
for other boards. Everything that does not need a register is plain C that also runs on the host under `gcc`, so it is unit-tested.

## Layers

| Directory | What it holds | Tested by |
| --- | --- | --- |
| `core/` | hardware-independent logic: matrix debounce, layers, tapping (LT/MT/tap dance), reports, macros, profile store, settings, VIA/Vial/HUD/diagnostics protocols, USB descriptors and control-endpoint state machine, radio protocol and link logic, backlight effects, sleep timing, encoder decoder, timer-slot sequence | `tests/host/` (`make test`, plain gcc, `-Wall -Wextra -Werror`) |
| `hal/` | the SH68F90A's registers and drivers: GPIO, clock, delays, Timer 2 slot interrupt, USB controller, flash self-programming, PWM, power-down | the simulator (`make sim`) and the hardware |
| `board/` | what is specific to the B68: pinout, matrix scan, LED PWM channels, radio serial port and pins, mode selector, sleep sequence, the default keymap, the custom keycodes, the Vial definition | `check_vial.py`, the simulator, the hardware |
| `tools/` | build helpers (Vial definition generator, budget, interrupt and Vial checks) and `diag.py` | `make check` |

`core/` never touches hardware. When it needs something from the outside it declares a **hook**, a function the board (or a test) defines:
`report_hook_send()`, `store_hook_read/erase/program()`, `radio_hook_*()`, `usb_hook_*()`, `layer_hook_mods_add()`… A test defines the hook with a
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
 raw HID (EP2) ──► raw_hid ──► via_receive ──► keymap / macros / tap dances / settings / HUD (0xF0) / diagnostics (0xF1, unlocked only)
```

## Timing: the timer interrupt owns the columns

The 16 column lines are the key-matrix strobes **and** the sinks of the backlight LEDs, so a key scan and a backlight subframe can never overlap. Timer 2 plays
the sequence of `core/sched.c`: four LED slots of 400 µs, then a scan slot of 450 µs, over and over (a backlight frame of 16 columns is 8.2 ms; a key is
read every 2.05 ms). The timer runs in auto-reload mode; the interrupt writes the length of the *next* slot into `RCAP2`, so slot lengths are exact whatever the
interrupt latency, and the millisecond clock of the firmware is the sum of the slot lengths. The interrupt only copies pre-computed values and writes
registers; effects, debounce and everything else are done from the main loop.

## Concurrency rules

- Interrupts: Timer 2 (slots), USB, the serial port of the radio (highest priority: a byte lasts 38 µs), INT4 (wake-up from power-down). The main loop is a flat
  polling loop; nothing blocks except the flash operations (the timer and the PWM are stopped around them) and the radio's sleep frames.
- Data shared with an interrupt has **one writer**, is a single byte or is read twice until it agrees, and is handed over through a flag. No interrupt calls a function that
  the main loop also calls (`delay_us_nokick` for the scan, for instance).
- The build uses `--stack-auto`, which keeps locals on the stack; `tools/check_interrupts.py` makes sure SDCC's shared static overlay stays empty, because an overlay
  would let an interrupt overwrite main-loop variables. It also checks that exactly the four intended vectors exist.
- The watchdog is kicked from the main loop and from the delays; the scan's own wait does not kick it, so a stuck main loop still lets it reset the chip.

## Memory

Application flash ends at 0xAC00. After it: profiles 0–3 (two copies of four 512-byte sectors each) up to 0xEBFF, the settings record at 0xEC00, 0xEE00 untouched,
the stock bootloader from 0xF000 (never written: `hal/flash.c` refuses any address outside 0xAC00–0xF000). `make size` reports flash and XDATA use against 44032 and 4096
bytes. Pointers into XDATA are typed (`XDATA uint8_t *`, `core/platform.h`) in the packet paths: a generic pointer costs three bytes and a slow access on this
core and was worth 1.5 KB of flash. Constant tables are `ROM`.

## Where the unknowns are

Everything in the hook layer below `core/` has been written from the stock firmware's dump and the SMK port, and is **not yet tried on the hardware**: the exact
duration of the interrupt work, the stack depth with every interrupt busy (91–104 bytes of 223 in the simulator), the PWM output, the radio serial port, the sleep sequence.
`docs/first-boot.md` is the way to find out, one stage at a time, and `tools/diag.py` reads the counters that answer the questions.

## Testing

| What | How | Cost |
| --- | --- | --- |
| Logic | `make test` (about 23 programs, plus the Vial definition check) | seconds |
| Linked firmware | `make check-interrupts`, `make size`, `make stages` | seconds |
| The real binary on a simulated chip | `make sim` (needs `nix develop`): boot, timer slots, stack, the recovery chord, every USB descriptor, VIA/Vial/HUD over the raw HID pipe, key reports, the ISP command | 30 s |
| Hardware | `docs/first-boot.md` | the owner |

The simulator's Timer 2 runs at a virtual pace, so it shows the *order* of events and the state of the chip, not durations.
