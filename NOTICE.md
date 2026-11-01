# Notice

Oblio is a firmware written from scratch for the Yunzii B68 keyboard. It is not a fork of any other project.

It would not exist without these, which were read as references (concepts, protocols, hardware facts), not copied:

- **[SMK](https://github.com/karstx/smk)** by karstx (GPLv2): the SH68F90 toolchain setup (SDCC), the ISP flashing flow, and the proof that this
  chip family can run a full keyboard firmware. The first working B68 port was made on SMK (branch `port/yunzii-b68` of wildzer0/smk).
- **[sinowisp](https://github.com/karstx/sinowisp)**: flashing over the USB ISP.
- **[QMK](https://qmk.fm)** and **[Vial](https://get.vial.today)**: keycode layout and the VIA/Vial raw HID protocol, which Oblio implements so the Vial GUI works.
- The B68's stock firmware, analysed for the pinout, the LED driving and the BK3632 radio protocol (see `docs/keyboards/`).

Rule of the project: SMK and the others are read for ideas; modules are written anew. If a file is ever adapted from another project, it keeps that project's licence header and is listed here.
