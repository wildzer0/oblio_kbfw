#!/usr/bin/env python3
"""Checks that the USB code is where it was when the keyboard was last tested, and fails if it moved.

usage: check_usb_layout.py [MAP]        compare build/oblio.map with tools/usb_layout.txt
       check_usb_layout.py --update     write the file from the current map (do this ONLY after the keyboard has been tested, see below)

Why this exists. On the keyboard the USB stopped being recognised for some positions of the USB code in the flash: the 0.9.0 firmware worked, the same
code shifted by 50 bytes or more (nothing else changed) did not - measured with a series of files shifted one byte at a time, then cured by linking the USB
modules first (docs/design.md 14.2). The host tests and the simulator cannot see this (the model has no flash timing), and the cause is not understood, so
the position of the USB code is treated like a calibrated value: it is recorded here, and a change fails the build.

What is compared: the address of every global symbol of the USB modules (usb_desc, usb_device, raw_hid, usb_reports, usb = hal/usb.c, usb_app) in the
default build (LOOP_PROFILE=0; the profile build has extra code in the USB interrupt and is only a measuring tool). Because the Makefile links those modules
first, they move only when their own code changes (or the start-up code before them).

What to do when it fails:
  1. If you did not mean to touch the USB code, something changed the link order or the start-up code: look at the Makefile (USB_SRC first).
  2. If you did (a change in core/usb_*.c, hal/usb.c, board/usb_app.c, core/raw_hid.c): build, flash THIS build on the keyboard with the web updater (drop
     build/oblio.hex), check that the keyboard is recognised on USB (the page reads its version; Windows lists it), and only then run
     `python3 tools/check_usb_layout.py --update` and commit the new tools/usb_layout.txt together with the change.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "tools" / "usb_layout.txt"
USB_MODULES = {"usb_desc", "usb_device", "raw_hid", "usb_reports", "usb", "usb_app"}


def symbols(map_path):
    """{symbol: (address, module)} of the code symbols of the USB modules in SDCC's linker map."""
    found = {}
    for line in Path(map_path).read_text().splitlines():
        m = re.match(r"^C:\s+([0-9A-Fa-f]{8})\s+(_\w+)\s+(\w+)\s*$", line)
        if m and m.group(3) in USB_MODULES:
            found[m.group(2)] = (int(m.group(1), 16), m.group(3))
    return found


def main():
    args = sys.argv[1:]
    update = "--update" in args
    args = [a for a in args if a != "--update"]
    map_path = Path(args[0]) if args else ROOT / "build" / "oblio.map"

    # Only the default build is checked: the profile build has more code in the USB interrupt, and is not meant to be installed.
    options = map_path.parent / ".options"
    if options.exists() and "LOOP_PROFILE=1" in options.read_text():
        print("check_usb_layout: profile build, not checked")
        return 0

    current = symbols(map_path)
    if not current:
        sys.exit("check_usb_layout: no USB symbols in {} (is it a full build?)".format(map_path))

    if update:
        lines = ["# The addresses of the USB code when the keyboard was last tested (tools/check_usb_layout.py explains why they are pinned).",
                 "# symbol address module"]
        for name, (address, module) in sorted(current.items(), key=lambda kv: kv[1][0]):
            lines.append("{} {:#06x} {}".format(name, address, module))
        MANIFEST.write_text("\n".join(lines) + "\n")
        print("check_usb_layout: wrote {} ({} symbols)".format(MANIFEST.relative_to(ROOT), len(current)))
        return 0

    pinned = {}
    for line in MANIFEST.read_text().splitlines():
        if line and not line.startswith("#"):
            name, address, module = line.split()
            pinned[name] = (int(address, 16), module)

    problems = []
    for name, (address, module) in sorted(pinned.items(), key=lambda kv: kv[1][0]):
        now = current.get(name)
        if now is None:
            problems.append("{} ({}): no longer there".format(name, module))
        elif now[0] != address:
            problems.append("{} ({}): {:#06x} -> {:#06x}".format(name, module, address, now[0]))
    for name in sorted(set(current) - set(pinned)):
        problems.append("{} ({}): new, at {:#06x}".format(name, current[name][1], current[name][0]))

    if problems:
        print("check_usb_layout: THE USB CODE MOVED ({} symbols differ):".format(len(problems)), file=sys.stderr)
        for p in problems[:12]:
            print("  " + p, file=sys.stderr)
        if len(problems) > 12:
            print("  ... and {} more".format(len(problems) - 12), file=sys.stderr)
        print(__doc__.split("What to do when it fails:")[1], file=sys.stderr)
        return 1

    print("check_usb_layout: the {} USB symbols are where they were tested".format(len(pinned)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
