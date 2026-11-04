# Flashing and checking the keyboard

Oblio has run on the real keyboard and every feature has been tried by the owner (USB, 2.4G and Bluetooth, Vial, the backlight, the battery gauge, sleep,
remote wakeup). This is how to put a new build on it and how to see that it is healthy. (Until 2026-11-02 the first runs were done in stages that started less of the
firmware; the staged builds were removed once they were no longer needed - see `CHANGELOG.md`.)

## The way back

Oblio leaves the stock bootloader (0xF000) alone. The recovery is: hold **Esc + 1 + 2** while plugging in the USB cable; the firmware then jumps to the bootloader
(`jmp 0xFF00` with A = 0x5A, B = 0xA5), where `sinowisp` can write a firmware again. A firmware that crashes before `recovery_check()` (the first thing after the clock and the pins)
cannot be recovered this way, and without the ICP programmer (`docs/recovery-icp.md`) there is no other way back: change the early start-up code with care.

## Flashing

```
nix develop
make fw size                # build/oblio.hex, and the flash / XDATA use against the budget
sinowisp write --platform sh68f90 --vendor_id 0x258a --product_id 0xdead --format ihex --isp_iface_num 0 -f build/oblio.hex
```

The same command works with Oblio running, because it keeps the id 258a:dead and the ISP command on interface 0. Close the Vial app first. The CI uploads the hex of every build as an artifact.

## Checking it

`tools/diag.py` (Windows: `py -m pip install hidapi`, then `py tools\diag.py`) prints the counters without any unlock; `--radio` adds the radio's status bytes and the battery reading;
`--watch 1` repeats and shows only what changed. What to look at:

| Counter | Healthy |
| --- | --- |
| reset cause | a power-on value; a repeating other one means resets |
| stack used | well under 223 bytes (about 150 at most today) |
| LED slots / scan slots that ran late | 0 |
| backlight back-offs | rare; many means the main loop is starved |
| slowest main-loop pass | a few ms (a flash save is the exception) |
| USB: times the host was declared gone | 0 with the cable in and the PC on |
| longest sector erase, keymap saves and log | see `docs/design.md` section 9 |

A short functional round: typing over the cable; Fn gives the F-row; the knob changes the volume; the Vial app loads the layout and a change survives unplugging;
the mode switch to 2.4G and to Bluetooth (Fn+B/N/M pick a slot, hold 3 s to pair); Fn+Backspace shows the battery on the number keys; Fn+Tab sleeps on battery and any key wakes.

## What to send back if something is wrong

The output of `tools/diag.py --radio`, what you did, and what looked wrong. `docs/session-state.md` lists the traps already met.
