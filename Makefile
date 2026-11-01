# Oblio: one board, one hex. `make test` builds and runs the host tests (pure logic, plain gcc).
# `make fw` builds the firmware with SDCC, `make size` checks the flash/XDATA budget.

HOST_TESTS := $(patsubst %.c,%,$(wildcard tests/host/test_*.c))

# Memory map: application code up to 0xAC00 (profiles start there), 4 KB of XDATA.
CODE_LIMIT := 0xAC00
XRAM_SIZE  := 4096

SDCC      ?= sdcc
SDCCFLAGS := --std-c23 --Werror -mmcs51 --model-large --stack-auto \
             --xram-loc 0x0000 --xram-size $(XRAM_SIZE) --code-size $(CODE_LIMIT) \
             --opt-code-speed --out-fmt-ihx -DWATCHDOG_ENABLE=1
# Sources compiled with SDCC but not linked, while a module waits for the hooks of the next step
NOT_LINKED :=
FW_SRC    := $(filter-out $(NOT_LINKED),$(wildcard core/*.c hal/*.c board/*.c))
FW_REL    := $(patsubst %.c,build/%.rel,$(FW_SRC)) build/vial_data.rel
CHECK_REL := $(patsubst %.c,build/%.rel,$(NOT_LINKED))

.PHONY: test fw size clean check check-vial check-interrupts
.DELETE_ON_ERROR: # a failed build must not leave a half-made file that make takes for up to date

test: $(HOST_TESTS) check-vial
	@for t in $(HOST_TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "host tests: $(words $(HOST_TESTS)) ok"

# test_X.c tests core/X.c; the modules it needs besides are listed in LINK_X. The tests are rebuilt every time (they are quick).
LINK_layer := core/keymap.c
LINK_keyboard := core/layer.c core/keymap.c core/report.c core/tapping.c core/macro.c core/encoder.c
LINK_tapping := core/keyboard.c core/layer.c core/keymap.c core/report.c core/macro.c core/encoder.c
LINK_macro := core/keymap.c
LINK_radio := core/diag.c
LINK_link := core/radio.c core/settings.c core/diag.c
LINK_led := core/led_effect.c core/keymap.c
LINK_via := core/vial.c core/keymap.c core/matrix.c core/diag.c
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

build/vial_data.c: board/vial.json tools/vial_gen.py
	@mkdir -p build
	python3 tools/vial_gen.py board/vial.json $(VIAL_UID) "$(VIAL_UNLOCK)" > $@

build/vial_data.rel: build/vial_data.c $(HEADERS)
	$(SDCC) $(SDCCFLAGS) -Icore -c $< -o $@

# the linked firmware: the interrupt vectors are the intended ones, and the static overlay is empty
check-interrupts: build/oblio.hex
	python3 tools/check_interrupts.py build/oblio.hex build/oblio.map

check: check-vial check-interrupts

check-vial:
	python3 tools/check_vial.py board/vial.json board/keymap_default.c board/b68_keys.h core/matrix.h "$(VIAL_UNLOCK)"

fw: build/oblio.hex

# every object depends on every header: coarse, but a changed header can never leave a stale object behind
HEADERS := $(wildcard core/*.h hal/*.h board/*.h)

build/%.rel: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) -Icore -Ihal -Iboard -c $< -o $@

build/oblio.ihx: $(FW_REL) $(CHECK_REL)
	$(SDCC) $(SDCCFLAGS) $(FW_REL) -o $@

build/oblio.hex: build/oblio.ihx
	packihx $< > $@ 2>/dev/null

size: build/oblio.ihx
	python3 tools/budget.py build/oblio.mem $(CODE_LIMIT) $(XRAM_SIZE)

clean:
	rm -f $(HOST_TESTS)
	rm -rf build
