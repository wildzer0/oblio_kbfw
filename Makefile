# Oblio: one board, one hex. `make test` builds and runs the host tests (pure logic, plain gcc).
# `make fw` builds the firmware with SDCC, `make size` checks the flash/XDATA budget.
B := build

# Build options (core/options.h has the defaults). `make BATTERY_LOG=0 fw` leaves the battery log out: ~1 KB less code and the flash sector at
# 0xDA00 back for the application (the code limit goes from 0xDA00 to 0xDC00). The objects are rebuilt when an option changes.
BATTERY_LOG ?= 1
# `make LOOP_PROFILE=1 fw` times each step of the main loop (see core/options.h); off by default.
LOOP_PROFILE ?= 0

HOST_TESTS := $(patsubst %.c,%,$(wildcard tests/host/test_*.c))

# Memory map: application code up to 0xDA00 with the battery log (its sector follows) or 0xDC00 without it (the keymap store starts there), 4 KB of XDATA.
CODE_LIMIT := $(if $(filter 0,$(BATTERY_LOG)),0xDC00,0xDA00)
XRAM_SIZE  := 4096

SDCC      ?= sdcc
SDCCFLAGS := --std-c23 --Werror -mmcs51 --model-large --stack-auto \
             --xram-loc 0x0000 --xram-size $(XRAM_SIZE) --code-size $(CODE_LIMIT) \
             --opt-code-speed --out-fmt-ihx -DWATCHDOG_ENABLE=1 -DBATTERY_LOG=$(BATTERY_LOG) -DLOOP_PROFILE=$(LOOP_PROFILE)
# every object depends on every header: coarse, but a changed header can never leave a stale object behind
HEADERS := $(wildcard core/*.h hal/*.h board/*.h) $(B)/.options

FW_SRC := $(wildcard core/*.c hal/*.c board/*.c)

# THE LINK ORDER: THE USB CODE FIRST. Do not "tidy" this into alphabetical order.
# The linker places the modules one after the other in the order it is given. With the USB modules in the middle (alphabetical order), every change to a
# module that comes before them moved the USB code, and on the keyboard the USB stopped being recognised for some positions: the 0.9.0 firmware worked, the
# same code shifted by 50 bytes or more did not (a measured series, docs/design.md 14.2; the host tests and the simulator cannot see it: the model has no
# flash timing). The cause is not understood; the cure is that the position of the USB code no longer depends on the rest of the firmware: these modules come
# first, right after the start-up code, so they only move when they change themselves. tools/check_usb_layout.py (make check-usb-layout, and the CI) fails
# when they move: test on the keyboard first (the web updater), then `python3 tools/check_usb_layout.py --update`.
USB_SRC   := core/usb_desc.c core/usb_device.c core/raw_hid.c core/usb_reports.c hal/usb.c board/usb_app.c
OTHER_SRC := $(filter-out $(USB_SRC),$(FW_SRC))
FW_REL    := $(patsubst %.c,$(B)/%.rel,$(USB_SRC) $(OTHER_SRC)) $(B)/vial_data.rel

# The version (core/version.h): the file VERSION ("major.minor.patch") and the git commit. Only core/version.c gets them, so a new commit recompiles that one file
# and not the whole firmware; the stamp .version makes it notice. A tree with uncommitted changes to tracked files is marked "dirty" in the image.
FW_VERSION := $(strip $(shell cat VERSION))
FW_REV     := $(shell git rev-parse --short=7 HEAD 2>/dev/null || echo 0000000)
FW_DIRTY   := $(shell git diff --quiet HEAD 2>/dev/null && echo 0 || echo 1)
VERSION_DEFS := -DFW_MAJOR=$(word 1,$(subst ., ,$(FW_VERSION))) -DFW_MINOR=$(word 2,$(subst ., ,$(FW_VERSION))) -DFW_PATCH=$(word 3,$(subst ., ,$(FW_VERSION))) \
                -DFW_REV='"$(FW_REV)"' -DFW_DIRTY=$(FW_DIRTY)

$(B)/.version: FORCE
	@mkdir -p $(B)
	@echo "$(VERSION_DEFS)" | cmp -s - $@ || echo "$(VERSION_DEFS)" > $@

# The options the objects were built with: when they change, everything is rebuilt
$(B)/.options: FORCE
	@mkdir -p $(B)
	@echo "BATTERY_LOG=$(BATTERY_LOG) LOOP_PROFILE=$(LOOP_PROFILE)" | cmp -s - $@ || echo "BATTERY_LOG=$(BATTERY_LOG) LOOP_PROFILE=$(LOOP_PROFILE)" > $@

api:
	python3 tools/gen_api.py

check-api:
	python3 tools/gen_api.py --check

.PHONY: api check-api test fw size clean check check-vial check-interrupts check-version check-usb-layout web-test sim FORCE
FORCE:
.DELETE_ON_ERROR: # a failed build must not leave a half-made file that make takes for up to date

test: $(HOST_TESTS) check-vial
	@for t in $(HOST_TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "host tests: $(words $(HOST_TESTS)) ok"

# test_X.c tests core/X.c; the modules it needs besides are listed in LINK_X. The tests are rebuilt every time (they are quick).
LINK_layer := core/keymap.c
LINK_keyboard := core/layer.c core/keymap.c core/report.c core/tapping.c core/macro.c core/encoder.c core/mouse.c core/combo.c core/override.c core/leader.c
LINK_tapping := core/keyboard.c core/layer.c core/keymap.c core/report.c core/macro.c core/encoder.c core/mouse.c core/combo.c core/override.c core/leader.c
LINK_macro := core/keymap.c
LINK_radio := core/diag.c core/version.c
LINK_link := core/radio.c core/settings.c core/diag.c core/version.c
LINK_led := core/led_effect.c core/keymap.c
LINK_via := core/vial.c core/vialrgb.c core/keymap.c core/matrix.c core/diag.c core/version.c core/layer.c core/report.c
LINK_combo :=
LINK_override :=
LINK_leader :=
LINK_diag := core/version.c
LINK_usb_reports := core/report.c
LINK_mouse := core/report.c
LINK_store := core/keymap.c


tests/host/test_%: tests/host/test_%.c core/%.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $< core/$*.c $(LINK_$*)

# includes core/usb_device.c itself, to reach its static functions
tests/host/test_usb_device: tests/host/test_usb_device.c core/usb_desc.c core/diag.c core/version.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $(filter %.c,$^)

# core/profile.c exists only in the profile build: its test is built that way
tests/host/test_profile: tests/host/test_profile.c core/profile.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -DLOOP_PROFILE=1 -Icore -Itests/host -o $@ $(filter %.c,$^)

# the default keymap is board data, but plain C: checked against the matrix measured on the board
tests/host/test_default_keymap: tests/host/test_default_keymap.c board/keymap_default.c core/keymap.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Iboard -Itests/host -o $@ $(filter %.c,$^)

# The Vial definition: board/vial.json compressed, with the keyboard id and the keys held to unlock (Esc + Enter, cells 0,0 and 2,13)
VIAL_UID    := 0x46ecd0565df84fa8
VIAL_UNLOCK := 0,0;2,13

$(B)/vial_data.c: board/vial.json tools/vial_gen.py
	@mkdir -p $(B)
	python3 tools/vial_gen.py board/vial.json $(VIAL_UID) "$(VIAL_UNLOCK)" > $@

$(B)/vial_data.rel: $(B)/vial_data.c $(HEADERS)
	$(SDCC) $(SDCCFLAGS) -Icore -c $< -o $@

# the linked firmware: the interrupt vectors are the intended ones, and the static overlay is empty
check-interrupts: $(B)/oblio.hex
	python3 tools/check_interrupts.py $(B)/oblio.hex $(B)/oblio.map

# The real firmware in the simulator of the chip (slow, and needs `ucsim-sh68f90`: `nix develop`); not part of `make test`
sim: $(B)/oblio.hex
	python3 tests/sim/test_sim.py

# The web updater (web/): its tests in Node (the hex parser, the ISP protocol against a fake bootloader) and a whole update through the page in headless Chrome
# (tests/web/e2e; skipped when Chrome is not there). The end-to-end test uses the real build/oblio.hex.
web-test: $(B)/oblio.hex
	node --test tests/web/*.test.mjs tests/web/e2e/run.mjs

# the USB code is where it was when the keyboard was last tested (the link order above); see tools/check_usb_layout.py
check-usb-layout: $(B)/oblio.hex
	python3 tools/check_usb_layout.py

# the version is in the image exactly once and says what the file VERSION says
check-version: $(B)/oblio.hex
	python3 tools/check_version.py $(B)/oblio.hex

check: check-vial check-interrupts check-version check-usb-layout

check-vial:
	python3 tools/check_vial.py board/vial.json board/keymap_default.c board/b68_keys.h core/matrix.h "$(VIAL_UNLOCK)"

fw: $(B)/oblio.hex

$(B)/%.rel: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) -Icore -Ihal -Iboard -c $< -o $@

$(B)/core/version.rel: core/version.c $(HEADERS) $(B)/.version
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) $(VERSION_DEFS) -Icore -Ihal -Iboard -c $< -o $@

$(B)/oblio.ihx: $(FW_REL)
	$(SDCC) $(SDCCFLAGS) $(FW_REL) -o $@

$(B)/oblio.hex: $(B)/oblio.ihx
	packihx $< > $@ 2>/dev/null

size: $(B)/oblio.ihx
	python3 tools/budget.py $(B)/oblio.mem $(CODE_LIMIT) $(XRAM_SIZE)

clean:
	rm -f $(HOST_TESTS)
	rm -rf build
