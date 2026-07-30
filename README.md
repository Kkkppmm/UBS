# USBForge

**USBForge** is a C (GTK3) toolkit to create a bootable ISO, write it to USB, and boot into a friendly **USB Lab** - for testing, docs, diagnostics, and install - not install-only.

## What you get

| Tool | Role |
|------|------|
| `usbforge-builder` | Host GUI: build ISO, write to USB, verify, built-in help |
| `usbforge-live` | Live/boot GUI: USB probe, read-speed test, diagnostics, docs, optional install |
| `scripts/build-iso.sh` | Packages binaries + docs into a GRUB-bootable ISO |

## Quick start

```bash
# Dependencies (Debian/Ubuntu)
sudo apt-get install -y build-essential pkg-config libgtk-3-dev \
    xorriso grub-common grub-pc-bin mtools

# Build
make

# Smoke test (no display needed)
make smoke

# Create bootable ISO
make iso
# -> build/usbforge.iso

# Run GUIs (needs a display)
./build/usbforge-builder
./build/usbforge-live
```

## Typical workflow

1. Run **Builder** on your PC -> **Build Bootable ISO**.
2. Plug in a USB stick -> **Refresh** -> **Write ISO -> USB** (erases the stick).
3. Boot the PC from that USB (firmware boot menu).
4. In the live session, open **USBForge Live** for USB testing, help, and more - install is optional.

## Project layout

```
common/     Shared C helpers (USB scan, files, sizes)
host/       Builder GUI (GTK3)
live/       Live USB Lab GUI (GTK3)
docs/       In-app help topics
scripts/    ISO build + autostart helpers
iso/        ISO staging templates
```

## Safety

Writing an ISO to a disk **erases** it. Always confirm the device path (`/dev/sdX`) before writing. The Live read-speed test is **read-only**.

## License

MIT - see `LICENSE`.
