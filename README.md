# Oblio

Lean firmware for the **Yunzii B68** (SH68F90A + BK3632 radio): Vial/VIA keymap editing, layers, macros, tap dance, tap-hold, four profiles,
RGB backlight, 2.4G and Bluetooth, sleep. One board, one hex, small enough for 4 KB of XDATA and 43 KB of flash.

Status: **phase 0**, bootstrap. The working firmware today lives on the SMK port (`wildzer0/smk`, branch `port/yunzii-b68`); Oblio replaces it module by module.
See [CHANGELOG.md](CHANGELOG.md) for the plan and [NOTICE.md](NOTICE.md) for credits.

```
nix develop --command make test     # host tests of the pure logic
```

Layout: `hal/` registers and drivers, `core/` pure logic (tested on the host), `board/` B68 specifics, `docs/` hardware notes.

Licence: GPL-2.0-or-later (see [LICENSE](LICENSE)).
