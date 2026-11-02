#!/usr/bin/env python3
"""Reads the diagnostic counters of an Oblio keyboard.

Linux: /dev/hidraw, no dependencies. Windows and macOS: `pip install hidapi` (the module is called `hid`).

usage: diag.py [--radio] [--monitor] [--ports] [--watch SECONDS]

  (no option)  print the counters once
  --radio      also print the radio's last status bytes and its last 08 frame
  --monitor    with the cable in (USB mode): listen to the radio's status too (it is switched off in USB mode); implies --radio
  --ports      print the raw port bytes (P0.5 is the Win/Mac switch: low = Mac; P0.3/P0.4 the mode selector)
  --watch N    repeat every N seconds, printing only what changed

The counters, the radio data and --ports need no unlock (they are read-only); --monitor does, because it changes what the radio is doing.
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

# The order of the counters in core/diag.h: keep this list in step with it (an old copy prints the wrong labels)
NAMES = [
    ("reset cause (RSTSTAT)", "{:#04x}"),
    ("stack used (bytes of 223)", "{}"),
    ("LED slots that ran late", "{}"),
    ("scans overwritten (harmless)", "{}"),
    ("slowest main-loop pass (ms)", "{}"),
    ("USB report queue overflows", "{}"),
    ("longest LED slot (us, of 400)", "{}"),
    ("longest scan slot (us, of 500)", "{}"),
    ("main passes between the last 2 scans", "{}"),
    ("backlight back-offs (starved)", "{}"),
    ("scan slots that ran late", "{}"),
    ("longest sector erase (us)", "{}"),
    ("radio: frames received (checksum ok)", "{}"),
    ("radio: answers to the mode command", "{}"),
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


def ports(keyboard):
    reply = keyboard.ask(0xF1, 3)
    names = ["P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"]
    print("ports              : " + " ".join("{}={:02x}".format(n, b) for n, b in zip(names, reply[2:10])))


def radio(keyboard):
    reply = keyboard.ask(0xF1, 1)
    print("radio status bytes : " + " ".join("{:02x}".format(b) for b in reply[2:8]))
    print("last 08 frame      : " + " ".join("{:02x}".format(b) for b in reply[8:30]))

    # the status as last seen on battery: the one that carries the level (with the cable the radio answers 0xFF there)
    battery, flags = reply[30], reply[31]
    if battery == 0xFF and flags == 0xFF:
        print("on battery         : never seen (no status answer has come with the cable out)")
    else:
        print("on battery         : battery byte {:#04x} (level {}), flags byte {:#04x}".format(battery, battery & 0x07, flags))


def blog(keyboard):
    """The battery log the keyboard keeps while it runs on its battery: minutes awake, the battery byte and the flags byte of the status
    answer. It is the only way to see those readings, because the diagnostics need a cable and with the cable the battery charges."""
    first = keyboard.ask(0xF1, 4, 0)
    count = first[2]

    if count == 0:
        print("battery log        : empty (it fills up while the keyboard runs on its battery)")
        return

    print("battery log        : {} records (minutes awake, battery byte, flags byte)".format(count))

    index = 0
    while index < count:
        reply = keyboard.ask(0xF1, 4, index)
        for at in range(3, len(reply) - 3, 4):
            if index >= count:
                break
            minutes = reply[at] | (reply[at + 1] << 8)
            battery = reply[at + 2]
            flags = reply[at + 3]
            cable = "  (cable in: charging, the byte is the sentinel)" if flags & 0x40 else ""
            print("  {:5d} min   battery {:#04x} (level {})   flags {:#04x}{}".format(minutes, battery, battery & 0x07, flags, cable))
            index += 1


def daemon(keyboard, seconds, path):
    """The counters (and the radio, if asked) appended to a file every `seconds`, with the time: leave it running and read it afterwards."""
    import datetime

    with open(path, "a") as out:
        out.write("# counters, in the order this tool prints them\n")
        while True:
            values = counters(keyboard)
            line = ",".join(str(v) for v in values)
            out.write("{},{}\n".format(datetime.datetime.now().isoformat(timespec="seconds"), line))
            out.flush()
            time.sleep(seconds)


def main():
    args = sys.argv[1:]
    want_monitor = "--monitor" in args
    want_radio = "--radio" in args or want_monitor
    want_ports = "--ports" in args
    want_blog = "--blog" in args
    watch = None
    if "--watch" in args:
        watch = float(args[args.index("--watch") + 1])

    daemon_seconds = None
    if "--daemon" in args:
        at = args.index("--daemon")
        more = args[at + 1:]
        daemon_seconds = float(more[0]) if more and not more[0].startswith("--") else 60.0

    path = find_device()
    if not path:
        sys.exit("diag: no Oblio keyboard found (258a:dead, raw HID interface)")

    keyboard = Keyboard(path)

    if want_monitor:
        keyboard.unlock()
        if keyboard.ask(0xF1, 2, 1)[2] != 1:
            sys.exit("diag: the keyboard did not start listening to the radio")
        print("listening to the radio (only in USB mode; in the wireless modes it is in use anyway)")

    if daemon_seconds is not None:
        out = [a for a in args[args.index("--daemon") + 1:] if not a.startswith("--")]
        out = out[1] if len(out) > 1 else "oblio-log.csv"
        print("logging every {:g} s to {} (Ctrl+C to stop)".format(daemon_seconds, out))
        daemon(keyboard, daemon_seconds, out)
        return

    values = counters(keyboard)
    print("counters:")
    show(values)
    if want_radio:
        radio(keyboard)
    if want_ports:
        ports(keyboard)
    if want_blog:
        blog(keyboard)

    while watch:
        time.sleep(watch)
        new = counters(keyboard)
        if new != values:
            print("changed:")
            show(new, values)
            values = new
        if want_radio:
            radio(keyboard)
        if want_blog:
            blog(keyboard)


if __name__ == "__main__":
    main()
