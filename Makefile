# Oblio: one board, one hex. `make test` builds and runs the host tests (pure logic, plain gcc).

HOST_TESTS := $(patsubst %.c,%,$(wildcard tests/host/test_*.c))

.PHONY: test clean
test: $(HOST_TESTS)
	@for t in $(HOST_TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "host tests: $(words $(HOST_TESTS)) ok"

tests/host/test_%: tests/host/test_%.c
	$(CC) -std=c11 -Wall -Wextra -Werror -Icore -Itests/host -o $@ $<

clean:
	rm -f $(HOST_TESTS)
