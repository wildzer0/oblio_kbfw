# Oblio: one board, one hex. `make test` builds and runs the host tests (pure logic, plain gcc).
# `make fw` builds the firmware with SDCC, `make size` checks the flash/XDATA budget.

HOST_TESTS := $(patsubst %.c,%,$(wildcard tests/host/test_*.c))

# Memory map: application code up to 0xAC00 (profiles start there), 4 KB of XDATA.
CODE_LIMIT := 0xAC00
XRAM_SIZE  := 4096

SDCC      ?= sdcc
SDCCFLAGS := --std-c23 --Werror -mmcs51 --model-large --stack-auto \
             --xram-loc 0x0000 --xram-size $(XRAM_SIZE) --code-size $(CODE_LIMIT) \
             --opt-code-speed --out-fmt-ihx
FW_SRC    := $(wildcard core/*.c hal/*.c board/*.c)
FW_REL    := $(patsubst %.c,build/%.rel,$(FW_SRC))

.PHONY: test fw size clean
test: $(HOST_TESTS)
	@for t in $(HOST_TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "host tests: $(words $(HOST_TESTS)) ok"

# test_X.c tests core/X.c; the modules it needs besides are listed in LINK_X. The tests are rebuilt every time (they are quick).
LINK_layer := core/keymap.c
LINK_keyboard := core/layer.c core/keymap.c core/report.c core/tapping.c core/macro.c
LINK_tapping := core/keyboard.c core/layer.c core/keymap.c core/report.c core/macro.c
LINK_macro := core/keymap.c
LINK_store := core/keymap.c

.PHONY: FORCE
FORCE:

tests/host/test_%: tests/host/test_%.c core/%.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $< core/$*.c $(LINK_$*)

# the default keymap is board data, but plain C: checked against the matrix measured on the board
tests/host/test_default_keymap: tests/host/test_default_keymap.c board/keymap_default.c core/keymap.c FORCE
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Iboard -Itests/host -o $@ $(filter %.c,$^)

fw: build/oblio.hex

build/%.rel: %.c
	@mkdir -p $(dir $@)
	$(SDCC) $(SDCCFLAGS) -Icore -Ihal -Iboard -c $< -o $@

build/oblio.ihx: $(FW_REL)
	$(SDCC) $(SDCCFLAGS) $(FW_REL) -o $@

build/oblio.hex: build/oblio.ihx
	packihx $< > $@ 2>/dev/null

size: build/oblio.ihx
	python3 tools/budget.py build/oblio.mem $(CODE_LIMIT) $(XRAM_SIZE)

clean:
	rm -f $(HOST_TESTS)
	rm -rf build
