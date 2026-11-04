# Notice

Oblio is a firmware written from scratch for the Yunzii B68 keyboard. It is not a fork of any other project.

It would not exist without these, which were read as references (concepts, protocols, hardware facts), not copied:

- **[SMK](https://github.com/karstx/smk)** by karstx (GPLv2): the SH68F90 toolchain setup (SDCC), the ISP flashing flow, and the proof that this
  chip family can run a full keyboard firmware. The first working B68 port was made on SMK (branch `port/yunzii-b68` of wildzer0/smk).
- **[sinowisp](https://github.com/karstx/sinowisp)**: flashing over the USB ISP.
- **[QMK](https://qmk.fm)** and **[Vial](https://get.vial.today)**: keycode layout and the VIA/Vial raw HID protocol, which Oblio implements so the Vial GUI works.
- The B68's stock firmware, analysed for the pinout, the LED driving and the BK3632 radio protocol (see `docs/keyboards/`).

One file is adapted rather than rewritten: `board/vial.json`, the Vial definition of the B68 (matrix size, drawn layout, custom keycode names), comes from
the B68 port on SMK (GPLv2, by the same author as this repository). Only the entry of the first custom keycode (the HUD toggle of the SMK port, now reserved and unused) was changed.

`tools/ucsim/` (`sh68f90.cc`, `sh68f90cl.h`, `sh68f90-register.patch`) is the SinoWealth SH68F90 CPU variant for SDCC's uCsim simulator, taken from
the B68 port on SMK (GPLv2, same author as this repository; uCsim itself is GPLv2 by Daniel Drotos and the SDCC project). Changes made for Oblio, marked
in the source: an interrupt source for the raw HID endpoint (EP2 OUT), the whole 32-byte reply in the EP2 IN log, and no cap of 16 packets on the
control-transfer log.

Rule of the project: SMK and the others are read for ideas; modules are written anew. If a file is ever adapted from another project, it keeps that project's licence header and is listed here.
