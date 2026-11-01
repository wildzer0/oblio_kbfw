"""Drives the SH68F90 build of uCsim (flake package `ucsim-sh68f90`) over its command socket.

The simulator models the chip: the core with its timers, the interrupt vectors of this part (Timer 2 on 0x0003, INT4 on 0x000B, USB on
0x003B, the serial port on 0x006B), the USB controller's endpoint registers, flash self-programming, the watchdog and power-down. It models
no board: what is wired to the pins is staged by the test (see PIN_STAGE).

Its Timer 2 is driven from a virtual flag at a fixed pace of CPU cycles, not from the timer's own count, so the *order* of what the firmware
does can be checked here, not how long anything takes: durations belong to the hardware.
"""

import os
import re
import shutil
import socket
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

# SH68F90 SFRs (hal/sh68f90.h)
SFR_ACC, SFR_B, SFR_SP = 0xE0, 0xF0, 0x81
P5, P7 = 0x88, 0xF8

# The pins that the board drives into P5 and P7, as the model reads them (an input bit reads this, idle = high)
PIN_STAGE = {P5: 0x1F15, P7: 0x1F17}


def find_sim():
    env = os.environ.get("OBLIO_UCSIM")
    if env:
        return env
    return shutil.which("ucsim-sh68f90")


def find_firmware(stage_dir="build"):
    return ROOT / stage_dir / "oblio.ihx"


def load_symbols(map_path):
    """{name: address} of the code symbols (functions) in SDCC's linker map; names without the leading underscore."""
    symbols = {}
    for line in Path(map_path).read_text().splitlines():
        m = re.match(r"^\s*C:\s+([0-9A-Fa-f]{8})\s+_(\w+)", line)
        if m:
            symbols[m.group(2)] = int(m.group(1), 16)
    return symbols


class Session:
    """uCsim interactive over a socket, with its replies framed on the NUL prompt (-P)."""

    def __init__(self, firmware, sim=None):
        self.sim = sim or find_sim()
        self.firmware = Path(firmware)
        last = None

        for _ in range(5):
            with socket.socket() as s:
                s.bind(("127.0.0.1", 0))
                self.port = s.getsockname()[1]

            self.proc = subprocess.Popen([self.sim, "-b", "-P", "-Z", str(self.port), "-t", "sh68f90"],
                                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            self.stderr_lines = []
            self._lock = threading.Lock()
            threading.Thread(target=self._drain, daemon=True).start()

            self.sock = None
            end = time.time() + 5
            while self.sock is None and time.time() < end and self.proc.poll() is None:
                try:
                    self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=0.3)
                except OSError as e:
                    last = e
                    time.sleep(0.02)
            if self.sock:
                break
            self.proc.kill()

        if self.sock is None:
            raise RuntimeError("the simulator's command socket never opened: %r" % last)

        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(0.3)
        try:
            while self.sock.recv(4096):
                pass
        except OSError:
            pass
        self.sock.settimeout(30)
        self.cmd('file "%s"' % self.firmware)

    def _drain(self):
        for line in iter(self.proc.stderr.readline, b""):
            with self._lock:
                self.stderr_lines.append(line.decode("latin1"))

    def stderr_text(self):
        with self._lock:
            return "".join(self.stderr_lines)

    def cmd(self, text):
        self.sock.sendall((text + "\n").encode())
        buf = bytearray()
        while b"\x00" not in buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        return buf.split(b"\x00", 1)[0].decode("latin1").replace("\r", "")

    def close(self):
        try:
            self.cmd("kill")
        except OSError:
            pass
        self.proc.kill()
        self.proc.wait()
        self.sock.close()
        self.proc.stderr.close()

    # --- breakpoints and running ---
    def break_at(self, address, hits=1):
        self.cmd("break 0x%x %d" % (address, hits) if hits > 1 else "break 0x%x" % address)

    def clear_breaks(self):
        self.cmd("delete")

    def run(self):
        """Runs to the next breakpoint; the address it stopped at, or None."""
        out = self.cmd("run")
        found = re.findall(r"Stop at 0x([0-9a-fA-F]+)", out)
        return int(found[-1], 16) if found else None

    # --- memory ---
    @staticmethod
    def _bytes(output):
        values = []
        for line in output.splitlines():
            m = re.match(r"\s*0x[0-9a-fA-F]+\s+((?:[0-9a-fA-F]{2} )+)", line)
            if m:
                values += [int(x, 16) for x in m.group(1).split()]
        return values

    def xram(self, address, count=1):
        return self._bytes(self.cmd("dump xram 0x%x 0x%x" % (address, address + count - 1)))[:count]

    def iram(self, address, count=1):
        return self._bytes(self.cmd("dump iram 0x%x 0x%x" % (address, address + count - 1)))[:count]

    def sfr(self, address):
        out = self.cmd("dump sfr 0x%02x 0x%02x" % (address, address))
        data = out.split("\n", 1)[1] if "\n" in out else out
        m = re.search(r"0b[01]+\s+0x([0-9a-fA-F]{2})", data) or re.search(r"^\s*0x[0-9a-fA-F]+\s+([0-9a-fA-F]{2})\b", data, re.M)
        return int(m.group(1), 16) if m else None

    def set_xram(self, address, data):
        self.cmd("set mem xram 0x%x " % address + " ".join("0x%02x" % b for b in data))

    def set_pin(self, port, level):
        self.set_xram(PIN_STAGE[port], [level])


# --- the USB side: the model logs the packets the firmware puts on the endpoints to the simulator's stderr ---
SFR_USBADDR, SFR_USBIF1, SFR_USBIF2, SFR_OEP0CNT, SFR_OEP2CNT = 0x96, 0x92, 0x93, 0x9E, 0xA4
EP0_OUT_BUF, EP2_OUT_BUF = 0x1100, 0x1140
SETUPIF, OEP0IF, OEP2IF = 0x10, 0x10, 0x40


def setup_packet(request_type, request, value, index, length):
    return [request_type, request, value & 0xFF, value >> 8, index & 0xFF, index >> 8, length & 0xFF, length >> 8]


class UsbSession(Session):
    """A session booted to the main loop, with helpers that play the host."""

    PASSES = 6  # passes of the main loop given to the firmware after each stimulus: the interrupts run between them

    def __init__(self, firmware):
        super().__init__(firmware)
        self.symbols = load_symbols(Path(firmware).with_suffix(".map"))
        self.break_at(self.symbols["usb_task"])
        self.run()
        self.clear_breaks()

    def passes(self, count=None):
        self.break_at(self.symbols["usb_task"], hits=count or self.PASSES)
        self.run()
        self.clear_breaks()

    def scans(self, count):
        """Runs until the timer interrupt has scanned the matrix `count` times (about 2 ms of the real keyboard each)."""
        self.break_at(self.symbols["tick_hook_scan"], hits=count)
        self.run()
        self.clear_breaks()

    def usb_mode(self):
        """The mode selector reads P0.4 and P0.3 low as 2.4G / Bluetooth. The model has no pull-ups on port 0 (it reads the latch), so the board's
        'both high' = the cable is staged by setting the latch bits, and the firmware is given the 50 ms it wants to believe it."""
        self.cmd("set mem sfr 0x80 0xff")
        self.scans(40)

    def mark(self):
        """Where the log of packets stands: what comes after is the answer to the next stimulus."""
        return len(self.stderr_lines)

    def packets(self, since, endpoint):
        """The packets logged on `endpoint` ("EP0 IN", "EP1 IN", "EP2 IN") after `since`, each as a list of bytes."""
        found = []
        with self._lock:
            lines = self.stderr_lines[since:]

        for line in lines:
            m = re.match(r"\[SIE\] %s(?:\[\d+\])? (\d+) bytes:((?: [0-9a-f]{2})*)" % re.escape(endpoint), line)
            if m:
                found.append([int(x, 16) for x in m.group(2).split()])

        return found

    def control(self, setup, passes=None):
        """One control transfer with no data from the host; the bytes of the answer, joined."""
        start = self.mark()

        self.set_xram(EP0_OUT_BUF, setup)
        self.cmd("set mem sfr 0x%02x 0x%02x" % (SFR_USBIF1, SETUPIF))
        self.passes(passes)

        return [b for packet in self.packets(start, "EP0 IN") for b in packet]

    def configure(self):
        self.control(setup_packet(0x00, 5, 5, 0, 0))  # SET_ADDRESS 5
        self.control(setup_packet(0x00, 9, 1, 0, 0))  # SET_CONFIGURATION 1

    def raw_hid(self, request):
        """A 32-byte request on the raw HID pipe; the 32-byte reply."""
        start = self.mark()

        self.set_xram(EP2_OUT_BUF, (request + [0] * 32)[:32])
        self.cmd("set mem sfr 0x%02x 0x20" % SFR_OEP2CNT)
        self.cmd("set mem sfr 0x%02x 0x%02x" % (SFR_USBIF2, OEP2IF))
        self.passes(10)

        replies = self.packets(start, "EP2 IN")
        return replies[-1] if replies else None
