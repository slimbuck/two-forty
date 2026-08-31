CC ?= cc

CPPFLAGS += -D_GNU_SOURCE -Iinclude
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += -Wl,--no-as-needed -l:libdrm.so.2 -l:libgbm.so.1 -l:libEGL.so.1 -l:libGLESv2.so.2

TARGET := build/two-forty-host
SOURCES := src/host.c
GAME_SOURCES := $(wildcard games/*/game.c)
GAME_TARGETS := $(patsubst games/%/game.c,build/games/%.so,$(GAME_SOURCES))

.PHONY: all clean

all: $(TARGET) $(GAME_TARGETS)

$(TARGET): $(SOURCES) include/two_forty.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@.next $(LDLIBS)
	mv $@.next $@

build/games/%.so: games/%/game.c include/two_forty.h
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -fPIC -shared $< -o $@.next
	mv $@.next $@

clean:
	rm -rf build
