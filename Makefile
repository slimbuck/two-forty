CC ?= cc
NODE ?= node

CPPFLAGS += -D_GNU_SOURCE -Iinclude
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += -Wl,--no-as-needed -l:libdrm.so.2 -l:libgbm.so.1 -l:libEGL.so.1 -l:libGLESv2.so.2

TARGET := build/two-forty-host
SOURCES := src/host.c src/input_bindings.c
GAME_SOURCES := $(wildcard games/*/game.c)
GAME_TARGETS := $(patsubst games/%/game.c,build/games/%.so,$(GAME_SOURCES))

.PHONY: all clean test

all: $(TARGET) $(GAME_TARGETS)

$(TARGET): $(SOURCES) src/input_bindings.h include/two_forty.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@.next $(LDLIBS)
	mv $@.next $@

.SECONDEXPANSION:
build/games/%.so: games/%/game.c $$(wildcard games/$$*/*.c games/$$*/*.h) include/two_forty.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -fPIC -shared $(filter %.c,$^) -o $@.next
	mv $@.next $@

clean:
	rm -rf build

test:
	$(NODE) --test dashboard/editors.test.js
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -ffunction-sections -fdata-sections tests/host_runtime.c src/input_bindings.c -Wl,--gc-sections -ldl -o /tmp/host-runtime-test
	/tmp/host-runtime-test $(CURDIR)/build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Igames/phosphor-run tests/phosphor_runtime.c games/phosphor-run/*.c -o /tmp/phosphor-runtime-test
	/tmp/phosphor-runtime-test
