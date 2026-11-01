# Oblio: one board, one hex. `make test` builds and runs the host tests (pure logic, plain gcc).
# `make fw` builds the firmware with SDCC, `make size` checks the flash/XDATA budget.
#
# The first runs on the hardware go in stages, so that a fault is found in the module that brought it: `make stage N=1` builds the
# keyboard over USB with Vial and the HUD and nothing else running, 2 adds the backlight, 3 the radio and the mode selector, 4 sleep
# (stage 4 is the release build, `make fw`). Same code in every stage, only less of it started: see board/main.c.
STAGE ?= 4
B     ?= build

HOST_TESTS := $(patsubst %.c,%,$(wildcard tests/host/test_*.c))

# Memory map: application code up to 0xDC00 (the keymap store starts there), 4 KB of XDATA.
CODE_LIMIT := 0xDC00
XRAM_SIZE  := 4096

SDCC      ?= sdcc
SDCCFLAGS := --std-c23 --Werror -mmcs51 --model-large --stack-auto \
             --xram-loc 0x0000 --xram-size $(XRAM_SIZE) --code-size $(CODE_LIMIT) \
             --opt-code-speed --out-fmt-ihx -DWATCHDOG_ENABLE=1 -DOBLIO_STAGE=$(STAGE)
# every object depends on every header: coarse, but a changed header can never leave a stale object behind
HEADERS := $(wildcard core/*.h hal/*.h board/*.h)

# Sources compiled with SDCC but not linked, while a module waits for the hooks of the next step
NOT_LINKED :=
FW_SRC    := $(filter-out $(NOT_LINKED),$(wildcard core/*.c hal/*.c board/*.c))
FW_REL    := $(patsubst %.c,$(B)/%.rel,$(FW_SRC)) $(B)/vial_data.rel
CHECK_REL := $(patsubst %.c,$(B)/%.rel,$(NOT_LINKED))

.PHONY: test fw size clean check check-vial check-interrupts stage stages sim
.DELETE_ON_ERROR: # a failed build must not leave a half-made file that make takes for up to date

test: $(HOST_TESTS) check-vial
	@for t in $(HOST_TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "host tests: $(words $(HOST_TESTS)) ok"

# test_X.c tests core/X.c; the modules it needs besides are listed in LINK_X. The tests are rebuilt every time (they are quick).
LINK_layer := core/keymap.c
LINK_keyboard := core/layer.c core/keymap.c core/report.c core/tapping.c core/macro.c core/encoder.c
LINK_tapping := core/keyboard.c core/layer.c core/keymap.c core/report.c core/macro.c core/encoder.c
LINK_macro := core/keymap.c
LINK_hud := core/keymap.c core/matrix.c core/layer.c core/report.c
LINK_radio := core/diag.c
LINK_link := core/radio.c core/settings.c core/diag.c
LINK_led := core/led_effect.c core/keymap.c
LINK_via := core/vial.c core/keymap.c core/matrix.c core/diag.c core/hud.c core/layer.c core/report.c
LINK_usb_reports := core/report.c
LINK_store := core/keymap.c

.PHONY: FORCE
FORCE:

tests/host/test_%: tests/host/test_%.c core/%.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $< core/$*.c $(LINK_$*)

# includes core/usb_device.c itself, to reach its static functions
tests/host/test_usb_device: tests/host/test_usb_device.c core/usb_desc.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $(filter %.c,$^)

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

check: check-vial check-interrupts

check-vial:
	python3 tools/check_vial.py board/vial.json board/keymap_default.c board/b68_keys.h core/matrix.h "$(VIAL_UNLOCK)"

fw: $(B)/oblio.hex

# `make stage N=2`: the firmware of that stage in build/stage2/, checked like the release one; `make stages`: 1 to 3 (4 is `make fw`)
stage:
	@test -n "$(N)" || { echo "usage: make stage N=1..4"; exit 1; }
	$(MAKE) STAGE=$(N) B=build/stage$(N) fw size check-interrupts

stages:
	@for n in 1 2 3; do $(MAKE) stage N=$$n || exit 1; done

$(B)/%.rel: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) -Icore -Ihal -Iboard -c $< -o $@

$(B)/oblio.ihx: $(FW_REL) $(CHECK_REL)
	$(SDCC) $(SDCCFLAGS) $(FW_REL) -o $@

$(B)/oblio.hex: $(B)/oblio.ihx
	packihx $< > $@ 2>/dev/null

size: $(B)/oblio.ihx
	python3 tools/budget.py $(B)/oblio.mem $(CODE_LIMIT) $(XRAM_SIZE)

clean:
	rm -f $(HOST_TESTS)
	rm -rf build
