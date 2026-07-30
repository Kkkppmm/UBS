CC       := gcc
CFLAGS   ?= -O2 -Wall -Wextra -Icommon
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0)
LDFLAGS  += -pthread
MINGW_CC ?= x86_64-w64-mingw32-gcc

BUILD    := build
COMMON_OBJ := $(BUILD)/usbforge.o
VERSION  := $(shell sed -n 's/.*USBFORGE_VERSION.*"\(.*\)".*/\1/p' common/usbforge.h)

.PHONY: all clean iso test smoke dirs install-deps \
        release deb rpm windows tarball

all: dirs $(BUILD)/usbforge-builder $(BUILD)/usbforge-live

dirs:
	@mkdir -p $(BUILD) $(BUILD)/iso $(BUILD)/release $(BUILD)/windows

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

deb:
	bash scripts/make-deb.sh

rpm:
	bash scripts/make-rpm.sh

windows:
	bash scripts/make-windows.sh

tarball: all
	bash -c 'OUT=build/release; mkdir -p $$OUT; \
	  NAME=usbforge-$(VERSION)-linux-x86_64; rm -rf $$OUT/$$NAME; \
	  mkdir -p $$OUT/$$NAME/{bin,docs,scripts,share/applications}; \
	  cp build/usbforge-builder build/usbforge-live $$OUT/$$NAME/bin/; \
	  cp docs/* $$OUT/$$NAME/docs/; cp scripts/*.sh $$OUT/$$NAME/scripts/; \
	  cp packaging/linux/*.desktop $$OUT/$$NAME/share/applications/; \
	  cp README.md LICENSE $$OUT/$$NAME/; \
	  tar -C $$OUT -czf $$OUT/$$NAME.tar.gz $$NAME; echo $$OUT/$$NAME.tar.gz'

release:
	bash scripts/make-release.sh

test: smoke
	@test -x $(BUILD)/usbforge-builder
	@test -x $(BUILD)/usbforge-live
	@echo "OK: binaries built (v$(VERSION))"

clean:
	rm -rf $(BUILD)

install-deps:
	sudo apt-get install -y build-essential pkg-config libgtk-3-dev \
	  xorriso grub-common grub-pc-bin mtools fakeroot rpm nsis gcc-mingw-w64-x86-64 zip
