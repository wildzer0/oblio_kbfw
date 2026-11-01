#!/usr/bin/env python3
"""Reads the diagnostic counters of an Oblio keyboard.

Linux: /dev/hidraw, no dependencies. Windows and macOS: `pip install hidapi` (the module is called `hid`).

usage: diag.py [--radio] [--monitor] [--watch SECONDS]

  (no option)  print the counters once
  --radio      also print the radio's last status bytes and its last 08 frame
  --monitor    with the cable in (USB mode): listen to the radio's status too (it is switched off in USB mode); implies --radio
  --watch N    repeat every N seconds, printing only what changed

The counters need no unlock (they are what you need when the keys do not work); the radio data and --monitor do: hold the unlock keys (Esc and Enter) when asked.
Run it as a user that may open the hidraw device (a udev rule, or sudo).
"""

import glob
import os
import struct
import sys
import time

try:
    import select
except ImportError:  # pragma: no cover
    select = None

try:
    import hid  # pip install hidapi: used where there is no /dev/hidraw
except ImportError:
    hid = None

VID_PID = "0000258A:0000DEAD"
RAW_SIZE = 32

NAMES = [
    ("reset cause (RSTSTAT)", "{:#04x}"),
    ("stack used (bytes of 223)", "{}"),
    ("LED slots that ran late", "{}"),
    ("scans overwritten (harmless)", "{}"),
    ("slowest main-loop pass (ms)", "{}"),
    ("USB report queue overflows", "{}"),
    ("own ms per 50 USB SOF (50 = ok)", "{}"),
    ("longest LED slot (us, of 400)", "{}"),
    ("longest scan slot (us, of 500)", "{}"),
    ("main passes between the last 2 scans", "{}"),
    ("scans with no main pass (harmless)", "{}"),
    ("backlight back-offs (starved)", "{}"),
    ("USB interrupts (not SOF)", "{}"),
    ("scan slots that ran late", "{}"),
]


def find_device():
    """The hidraw node of the raw HID interface (usage page 0xFF60) of the keyboard, or its hidapi path where there is no hidraw."""
    if not os.path.isdir("/sys/class/hidraw"):
        if hid is None:
            sys.exit("diag: no /dev/hidraw here; install the hidapi module:  py -m pip install hidapi")
        for info in hid.enumerate(0x258A, 0xDEAD):
            if info["usage_page"] == 0xFF60:
                return info["path"]
        return None

    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            uevent = open(os.path.join(node, "device/uevent")).read()
            descriptor = open(os.path.join(node, "device/report_descriptor"), "rb").read()
        except OSError:
            continue
        if VID_PID in uevent.upper() and b"\x06\x60\xff" in descriptor:
            return "/dev/" + os.path.basename(node)
    return None


class Keyboard:
    def __init__(self, path):
        self.device = None
        if isinstance(path, bytes):  # a hidapi path
            self.device = hid.device()
            self.device.open_path(path)
        else:
            self.fd = os.open(path, os.O_RDWR)

    def ask(self, *request):
        packet = bytes(request) + bytes(RAW_SIZE - len(request))

        if self.device:
            self.device.write(b"\x00" + packet)  # report id 0: the interface has none
            reply = self.device.read(RAW_SIZE, 1000)
            if not reply:
                sys.exit("diag: the keyboard did not answer")
            return bytes(reply)

        os.write(self.fd, packet)
        ready, _, _ = select.select([self.fd], [], [], 1.0)
        if not ready:
            sys.exit("diag: the keyboard did not answer")
        return os.read(self.fd, RAW_SIZE)

    def unlock(self):
        status = self.ask(0xFE, 0x05)
        if status[0]:
            return
        keys = [(status[2 + 2 * i], status[3 + 2 * i]) for i in range(14) if status[2 + 2 * i] != 0xFF]
        print("Hold the keys at (row, col) {} until it says unlocked...".format(keys))
        self.ask(0xFE, 0x06)
        while True:
            answer = self.ask(0xFE, 0x07)
            if answer[0]:
                print("unlocked")
                return
            time.sleep(0.12)


def counters(keyboard):
    reply = keyboard.ask(0xF1, 0)
    if reply[0] != 0xF1:
        sys.exit("diag: the keyboard does not know the diagnostics command (is this an Oblio build, and is it unlocked?)")
    count = reply[2]
    return list(struct.unpack_from("<" + "H" * count, reply, 3))


def show(values, previous=None):
    for i, value in enumerate(values):
        if previous is not None and previous[i] == value:
            continue
        name, fmt = NAMES[i] if i < len(NAMES) else ("counter {}".format(i), "{}")
        print("  {:32} {}".format(name, fmt.format(value)))


def radio(keyboard):
    reply = keyboard.ask(0xF1, 1)
    print("radio status bytes : " + " ".join("{:02x}".format(b) for b in reply[2:8]))
    print("last 08 frame      : " + " ".join("{:02x}".format(b) for b in reply[8:30]))


def main():
    args = sys.argv[1:]
    want_monitor = "--monitor" in args
    want_radio = "--radio" in args or want_monitor
    watch = None
    if "--watch" in args:
        watch = float(args[args.index("--watch") + 1])

    path = find_device()
    if not path:
        sys.exit("diag: no Oblio keyboard found (258a:dead, raw HID interface)")

    keyboard = Keyboard(path)

    if want_radio:
        keyboard.unlock()

    if want_monitor:
        if keyboard.ask(0xF1, 2, 1)[2] != 1:
            sys.exit("diag: the keyboard did not start listening to the radio")
        print("listening to the radio (only in USB mode; in the wireless modes it is in use anyway)")

    values = counters(keyboard)
    print("counters:")
    show(values)
    if want_radio:
        radio(keyboard)

    while watch:
        time.sleep(watch)
        new = counters(keyboard)
        if new != values:
            print("changed:")
            show(new, values)
            values = new
        if want_radio:
            radio(keyboard)


if __name__ == "__main__":
    main()
