CC       := gcc
CFLAGS   ?= -O2 -Wall -Wextra -Icommon
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0)
LDFLAGS  += -pthread

BUILD    := build
COMMON_OBJ := $(BUILD)/usbforge.o

.PHONY: all clean iso test smoke dirs install-deps

all: dirs $(BUILD)/usbforge-builder $(BUILD)/usbforge-live

dirs:
	@mkdir -p $(BUILD) $(BUILD)/iso

$(BUILD)/usbforge.o: common/usbforge.c common/usbforge.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/usbforge-builder: host/builder.c $(COMMON_OBJ) common/usbforge.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) host/builder.c $(COMMON_OBJ) -o $@ $(GTK_LIBS) $(LDFLAGS)

$(BUILD)/usbforge-live: live/live.c $(COMMON_OBJ) common/usbforge.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) live/live.c $(COMMON_OBJ) -o $@ $(GTK_LIBS) $(LDFLAGS)

smoke: all
	$(BUILD)/usbforge-live --smoke

iso: all
	bash scripts/build-iso.sh $(BUILD)/usbforge.iso

test: smoke
	@test -x $(BUILD)/usbforge-builder
	@test -x $(BUILD)/usbforge-live
	@echo "OK: binaries built"

clean:
	rm -rf $(BUILD)

install-deps:
	sudo apt-get install -y build-essential pkg-config libgtk-3-dev xorriso grub-common grub-pc-bin mtools
