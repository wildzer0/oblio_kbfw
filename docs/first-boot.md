# First run on the keyboard

Nothing of Oblio has run on the real keyboard yet. This is the order to find out what works, with the least risk and with a way to see what is going on.

## Before anything

- **The way back.** Oblio leaves the stock bootloader (0xF000) alone. The recovery is: hold **Esc + 1 + 2** while plugging in the USB cable; the firmware then jumps to the
  bootloader (`jmp 0xFF00` with A = 0x5A, B = 0xA5), where `sinowisp` can write a firmware again. That jump has been seen to work on the SMK port; the simulator shows Oblio
  makes it with the right registers. **What has not been shown** is that a broken Oblio gets that far: a firmware that crashes before `recovery_check()` (the first thing after
  the clock and the pins) cannot be recovered this way. Without the ICP programmer (`docs/recovery-icp.md`) there is no other way back. This is why the first runs are in stages.
- Keep the SMK build (`~/dev/smk`, branch `port/yunzii-b68`, `build/yunzii-b68_default_smk.hex`) at hand: it is the known-good firmware to go back to.
- A battery in the keyboard is fine. For the sleep test you need it, and a multimeter in series to measure the current.

## Flashing

```
nix develop
make stages                 # build/stage1/oblio.hex … stage3, and `make fw` for stage 4 (build/oblio.hex)
sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/stage1/oblio.hex
```

(with the SMK firmware running; once Oblio runs, the same command works because it keeps the id 258a:dead and the ISP command on interface 0.) The CI also uploads the
stage hexes as an artifact. Read the counters with `tools/diag.py` (hold Esc and Enter when it asks: it is Vial's unlock).

## The stages

Each stage is the same code with less of it started. Go on to the next only when the one before is clean.

### Stage 1 — keys, USB, Vial, HUD (no backlight, no radio, no sleep)

Look for:
1. The keyboard enumerates as `258a:dead`, product "Oblio B68", serial `vial:f64c2b3c` (`lsusb -v`), with two HID interfaces.
2. Typing works over the cable; Fn gives the F-row; the knob changes the volume, and its press types mute (hold: play/pause, double tap: next).
3. The Vial app lists the keyboard, loads its layout (the 66 keys + the knob), changes a key, a macro, a tap dance; the change survives unplugging.
4. Your HUD app connects and shows layers, pressed keys and the map.
5. `tools/diag.py`: **reset cause** (a power-on value; anything repeating means resets), **stack used** (the simulator says 91–104 of 223), **LED and scan slots that ran late**: 0 (a scan that the main loop does not collect in time is overwritten by the next one and is not a fault: the matrix is a state, so no key is lost, it only takes a few more scans to debounce; on USB about a third of them are overwritten),
   **slowest main-loop pass**: a few ms (a flash save is the exception).

If it does not enumerate or the keys are wrong: hold Esc+1+2 on plugging in. If that does not get you to the bootloader, the fault is before `recovery_check()` (clock, pins, REGCON).

### Stage 2 — the backlight

The LED slots and the PWM start once the host has configured the keyboard (or after half a second with no host). Look for: the backlight comes on without flicker, all keys still
register at the same time, Fn+Q…Y change effect/brightness/speed, Fn+U/I the plain colour, caps lock lights its key. In `diag.py`: **slots late must stay 0**; if the
backlight flickers or keys miss, the scan slot (450 µs) does not hold its work: report the counters.

### Stage 3 — the radio

Move the mode switch to 2.4G, then to Bluetooth; Fn+B/N/M pick a slot (hold 3 s to pair). Look for the key 4 (2.4G) or 1–3 (slot) flashing, typing on the host, media keys and the knob in wireless.
`tools/diag.py --radio --watch 2` shows the radio's counters (refusals, frames that never ended) and its last status bytes and 08 frame. **Battery:** Fn+Backspace shows the level on the number keys (ten white blinking keys = no reading yet); `diag.py --radio` prints the reading and the percentage (the reading is saturated with the cable).

### Stage 4 — sleep (the release build)

Battery, no cable, wireless mode: Fn+Tab sleeps now; any key wakes. Fn+O/P step the idle time (1, 5, 10, 30 min, never; keys 1–5 flash the step). Measure the current asleep. If it does not wake:
unplug and replug the cable, and use Esc+1+2 if needed.

## What to send back

The output of `tools/diag.py --radio` after each stage, and what looked wrong (or that nothing did). Anything that disagrees with `docs/architecture.md` is a finding, not a failure.
