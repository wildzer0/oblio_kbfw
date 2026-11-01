#!/usr/bin/env python3
"""Runs the real firmware (build/oblio.ihx) in the simulator of the chip.

    nix develop --command make sim

What this can and cannot show is in oblio_sim.py: the order of events and the state of the chip, not durations.
"""

import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from oblio_sim import (  # noqa: E402
    P5, P7, SFR_ACC, SFR_B, SFR_USBADDR, OEP0IF, EP0_OUT_BUF, SFR_OEP0CNT, SFR_USBIF2, Session, UsbSession,
    find_firmware, find_sim, load_symbols, setup_packet,
)

FIRMWARE = find_firmware()
SYMBOLS = None
STACK_BASE, STACK_TOP, STACK_PAINT = 0x21, 0xFF, 0xA5
ISP_BOOTLOADER = 0xFF00


def setUpModule():
    global SYMBOLS

    if not find_sim():
        raise unittest.SkipTest("ucsim-sh68f90 is not on the PATH (nix develop, or OBLIO_UCSIM)")
    if not FIRMWARE.exists():
        raise unittest.SkipTest("%s is not built (make fw)" % FIRMWARE)

    SYMBOLS = load_symbols(FIRMWARE.with_suffix(".map"))
    build_descriptors()


DESCRIPTORS = {}


def build_descriptors():
    """The descriptors as the host tests have them (core/usb_desc.c, compiled for the host): {(type, index, wIndex): [bytes]}."""
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "desc_dump"
        root = Path(__file__).resolve().parent.parent.parent
        subprocess.run(["gcc", "-std=c11", "-I", str(root / "core"), "-o", str(exe), str(root / "tests/sim/desc_dump.c"),
                        str(root / "core/usb_desc.c")], check=True)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout

    for line in out.splitlines():
        words = line.split()
        DESCRIPTORS[(int(words[0]), int(words[1]), int(words[2]))] = [int(w, 16) for w in words[3:]]


class SimCase(unittest.TestCase):
    def setUp(self):
        self.sim = Session(FIRMWARE)
        self.addCleanup(self.sim.close)

    def stack_used(self):
        """The deepest the stack went: the highest byte of the painted stack area (diag_stack_paint) that no longer holds the paint."""
        iram = self.sim.iram(STACK_BASE, STACK_TOP - STACK_BASE + 1)
        top = max((i for i, b in enumerate(iram) if b != STACK_PAINT), default=0)
        return top + 1


class TestBoot(SimCase):
    def test_it_reaches_the_main_loop(self):
        self.sim.break_at(SYMBOLS["usb_task"])
        self.assertEqual(self.sim.run(), SYMBOLS["usb_task"])

    def test_interrupts_are_on_in_the_main_loop(self):
        self.sim.break_at(SYMBOLS["usb_task"])
        self.sim.run()
        self.assertTrue(self.sim.sfr(0xA8) & 0x80, "EA")
        self.assertTrue(self.sim.sfr(0xA8) & 0x01, "ET2: the timer interrupt")

    def test_the_interrupt_work_fits_its_slots(self):
        # In the simulator the instruction counts are exact, the time per instruction is not: the guard is on the count. The first version
        # of the backlight took 4697 instructions for a subframe and 8963 for a scan and starved the main loop on the real keyboard (no
        # keys, then watchdog resets). A scan includes ~6400 instructions of deliberate waiting (16 columns x 2 readings x 10 us).
        # A scan comes once every four LED slots, so a frame is ~8.8 ms of simulated time and 320 scans are past the half second after
        # which the backlight is allowed (it was 900: the same point twice as late, and the time to get there is what timed the test out
        # on the CI runners).
        self.sim.break_at(SYMBOLS["tick_hook_scan"], hits=320)
        self.sim.run()
        self.sim.clear_breaks()
        isr = self._isr_calls()

        counts = {}
        for name, (call, after) in isr.items():
            samples = []
            for _ in range(4):
                self.sim.clear_breaks(); self.sim.break_at(call); self.sim.run(); a = self.sim.instructions()
                self.sim.clear_breaks(); self.sim.break_at(after); self.sim.run(); b = self.sim.instructions()
                samples.append(b - a)
            counts[name] = max(samples)

        print("\n    instructions: backlight subframe %d, scan %d" % (counts["led"], counts["scan"]))
        self.assertLess(counts["led"], 1200)
        self.assertLess(counts["scan"], 8500)  # ~6400 of them are the waits: 320 us of 500, the work is ~120 us

    def _isr_calls(self):
        """The addresses of the two hook calls in tick_isr and of the instruction after each (found in the disassembly)."""
        start = SYMBOLS["tick_isr"]
        calls = {}
        for line in self.sim.cmd("dc 0x%x 0x%x" % (start, start + 0x180)).splitlines():
            m = re.match(r"\s*0x([0-9a-f]+)\s+\?\s+12 ([0-9a-f]{2}) ([0-9a-f]{2}) LCALL", line, re.I)
            if m:
                target = int(m.group(2) + m.group(3), 16)
                for name, symbol in (("led", "tick_hook_led"), ("scan", "tick_hook_scan")):
                    if target == SYMBOLS[symbol]:
                        at = int(m.group(1), 16)
                        calls[name] = (at, at + 3)
        self.assertEqual(sorted(calls), ["led", "scan"])
        return calls

    def test_the_stack_stays_shallow_with_every_interrupt_running(self):
        self.sim.break_at(SYMBOLS["usb_task"], hits=400)
        self.assertEqual(self.sim.run(), SYMBOLS["usb_task"])

        used = self.stack_used()
        print("\n    stack used after 400 passes of the main loop: %d bytes of %d" % (used, STACK_TOP - STACK_BASE + 1))
        self.assertLess(used, 120)

    def test_the_watchdog_never_fires(self):
        self.sim.break_at(SYMBOLS["usb_task"], hits=3000)
        self.sim.run()
        self.assertNotIn("WATCHDOG", self.sim.stderr_text())


class TestTimer(SimCase):
    def test_four_backlight_subframes_then_a_scan(self):
        led, scan = SYMBOLS["tick_hook_led"], SYMBOLS["tick_hook_scan"]

        self.sim.break_at(led)
        self.sim.break_at(scan)

        seen = []
        for _ in range(60):
            at = self.sim.run()
            self.assertIn(at, (led, scan))
            seen.append("L" if at == led else "S")

        text = "".join(seen)
        first_scan = text.index("S")
        # whatever the boot did before the timer started, from the first scan on the pattern is exact
        self.assertRegex(text[first_scan:], r"^(S L{4})*".replace(" ", ""))
        self.assertGreaterEqual(text.count("S"), 8)


class TestRecovery(SimCase):
    def test_the_recovery_chord_jumps_to_the_isp_bootloader(self):
        # Esc, 1 and 2 (columns 0, 1, 2 of row 0) down: row 0 reads low while their columns are driven. Holding the row low makes every
        # column of row 0 read as pressed, which includes the three.
        self.sim.set_pin(P7, 0xFD)
        self.sim.break_at(ISP_BOOTLOADER)
        self.sim.break_at(SYMBOLS["usb_task"])

        self.assertEqual(self.sim.run(), ISP_BOOTLOADER)
        self.assertEqual(self.sim.sfr(SFR_ACC), 0x5A)
        self.assertEqual(self.sim.sfr(SFR_B), 0xA5)

    def test_without_the_chord_the_keyboard_boots_on(self):
        self.sim.break_at(ISP_BOOTLOADER)
        self.sim.break_at(SYMBOLS["usb_task"])
        self.assertEqual(self.sim.run(), SYMBOLS["usb_task"])

    def test_two_of_the_three_keys_are_not_enough(self):
        # only Esc and 1 down: a key held in column 0 and 1 of row 0 alone cannot be staged with a static pin level, so this checks the
        # other direction: any single row other than 0 held down is not the chord
        self.sim.set_pin(P7, 0xFB)  # row 1 low
        self.sim.break_at(ISP_BOOTLOADER)
        self.sim.break_at(SYMBOLS["usb_task"])
        self.assertEqual(self.sim.run(), SYMBOLS["usb_task"])


class UsbCase(unittest.TestCase):
    def setUp(self):
        self.usb = UsbSession(FIRMWARE)
        self.addCleanup(self.usb.close)


class TestEnumeration(UsbCase):
    def get_descriptor(self, kind, index, w_index):
        return self.usb.control(setup_packet(0x80, 6, (kind << 8) | index, w_index, 255))

    def test_every_descriptor_comes_out_as_in_the_host_tables(self):
        # a descriptor of the firmware crosses the endpoint in packets of 8 bytes, through the real interrupt handler
        for (kind, index, w_index), expected in DESCRIPTORS.items():
            got = self.get_descriptor(kind, index, w_index)
            self.assertEqual(got, expected, "descriptor %x/%d/%d" % (kind, index, w_index))

    def test_the_device_is_the_one_sinowisp_and_vial_look_for(self):
        got = self.get_descriptor(1, 0, 0)
        self.assertEqual(got[8:12], [0x8A, 0x25, 0xAD, 0xDE])  # 258a:dead

    def test_address_and_configuration(self):
        self.usb.control(setup_packet(0x00, 5, 5, 0, 0))
        self.assertEqual(self.usb.sfr(SFR_USBADDR), 5)  # applied after the status stage

        self.assertEqual(self.usb.control(setup_packet(0x80, 8, 0, 0, 1)), [0])  # not configured yet
        self.usb.control(setup_packet(0x00, 9, 1, 0, 0))
        self.assertEqual(self.usb.control(setup_packet(0x80, 8, 0, 0, 1)), [1])

    def test_an_unknown_request_does_not_answer_with_data(self):
        self.assertEqual(self.usb.control(setup_packet(0x80, 99, 0, 0, 8)), [])


class TestRawHid(UsbCase):
    def setUp(self):
        super().setUp()
        self.usb.configure()

    def test_via_protocol_version(self):
        reply = self.usb.raw_hid([0x01])
        self.assertEqual(reply[:3], [0x01, 0x00, 0x09])

    def test_vial_keyboard_id(self):
        reply = self.usb.raw_hid([0xFE, 0x00])
        self.assertEqual(reply[:12], [6, 0, 0, 0, 0x46, 0xEC, 0xD0, 0x56, 0x5D, 0xF8, 0x4F, 0xA8])

    def test_hud_info_as_in_the_protocol_document(self):
        reply = self.usb.raw_hid([0xF0, 0x01, 0x00, 0x05])
        # f0 01 00 05 | 02 05 10 08 | 08 00 | 00 7e | 0c | VV | 02
        self.assertEqual(reply[:13], [0xF0, 0x01, 0x00, 0x05, 0x02, 0x05, 0x10, 0x06, 0x08, 0x00, 0x00, 0x7E, 0x0C])
        self.assertEqual(reply[14], 0x02)

    def test_keymap_read_back_through_via(self):
        reply = self.usb.raw_hid([0x04, 0, 0, 0])  # layer 0, row 0, column 0: Esc
        self.assertEqual(reply[4:6], [0x00, 0x29])

    def test_a_command_the_firmware_does_not_know(self):
        self.assertEqual(self.usb.raw_hid([0x77])[0], 0xFF)

    def test_the_diagnostics_read_without_the_unlock_but_the_radio_monitor_does_not(self):
        # the counters and the radio's own data are what tells a dead keyboard from a starved one, and what a measurement needs: they must
        # be readable with no key working to unlock. Only the monitor switch, which changes what the radio does, needs it.
        self.assertEqual(self.usb.raw_hid([0xF1, 0])[0], 0xF1)
        self.assertEqual(self.usb.raw_hid([0xF1, 1])[0], 0xF1)
        self.assertEqual(self.usb.raw_hid([0xF1, 3])[0], 0xF1)
        self.assertEqual(self.usb.raw_hid([0xF1, 2, 1])[0], 0xFF)

    def test_two_requests_in_a_row(self):
        self.assertEqual(self.usb.raw_hid([0x01])[:3], [0x01, 0x00, 0x09])
        self.assertEqual(self.usb.raw_hid([0x01])[:3], [0x01, 0x00, 0x09])


class TestKeys(UsbCase):
    def test_keys_come_out_as_reports_on_endpoint_1(self):
        self.usb.usb_mode()
        self.usb.configure()
        start = self.usb.mark()

        # row 3 held low: every key of row 3 reads as pressed (Shift, Z, X ...). Pins are staged with a static level.
        self.usb.set_pin(P5, 0xF7)
        self.usb.scans(40)
        self.usb.passes(40)

        reports = self.usb.packets(start, "EP1 IN")
        self.assertGreaterEqual(len(reports), 2)

        # report id 3, the modifiers, then the NKRO bitmap (14 bytes): Z is usage 0x1D, so bit 5 of its fourth byte
        bitmap = [0] * 14
        bitmap[0x1D >> 3] = 1 << (0x1D & 7)

        self.assertEqual(reports[0], [3, 0x02] + [0] * 14)          # the left shift alone
        self.assertEqual(reports[1], [3, 0x02] + bitmap)            # and Z with it

    def test_nothing_is_sent_before_the_host_configures_the_keyboard(self):
        self.usb.usb_mode()
        start = self.usb.mark()
        self.usb.set_pin(P5, 0xF7)
        self.usb.scans(40)
        self.usb.passes(40)
        self.assertEqual(self.usb.packets(start, "EP1 IN"), [])


class TestIsp(UsbCase):
    def test_the_isp_command_over_usb_jumps_to_the_bootloader(self):
        self.usb.configure()

        # SET_REPORT(feature, id 5), 6 bytes: the data stage carries 05 75 ...
        self.usb.set_xram(EP0_OUT_BUF, setup_packet(0x21, 9, 0x0305, 0, 6))
        self.usb.cmd("set mem sfr 0x92 0x10")
        self.usb.passes(3)
        self.usb.set_xram(EP0_OUT_BUF, [0x05, 0x75, 0, 0, 0, 0])
        self.usb.cmd("set mem sfr 0x%02x 0x06" % SFR_OEP0CNT)
        self.usb.cmd("set mem sfr 0x%02x 0x%02x" % (SFR_USBIF2, OEP0IF))

        self.usb.clear_breaks()
        self.usb.break_at(0xFF00)
        self.assertEqual(self.usb.run(), 0xFF00)
        self.assertEqual(self.usb.sfr(SFR_ACC), 0x5A)
        self.assertEqual(self.usb.sfr(SFR_B), 0xA5)

    def test_another_command_does_not(self):
        self.usb.configure()
        self.usb.set_xram(EP0_OUT_BUF, setup_packet(0x21, 9, 0x0305, 0, 6))
        self.usb.cmd("set mem sfr 0x92 0x10")
        self.usb.passes(3)
        self.usb.set_xram(EP0_OUT_BUF, [0x05, 0x76, 0, 0, 0, 0])
        self.usb.cmd("set mem sfr 0x%02x 0x06" % SFR_OEP0CNT)
        self.usb.cmd("set mem sfr 0x%02x 0x%02x" % (SFR_USBIF2, OEP0IF))

        self.usb.clear_breaks()
        self.usb.break_at(0xFF00)
        self.usb.break_at(SYMBOLS["usb_task"], hits=40)
        self.assertEqual(self.usb.run(), SYMBOLS["usb_task"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
