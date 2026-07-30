# Getting Started

## Install build dependencies

On Debian/Ubuntu:

```bash
sudo apt-get install -y build-essential pkg-config libgtk-3-dev \
    xorriso grub-common grub-pc-bin mtools
```

## Build

```bash
make
```

You should get:

- `build/usbforge-builder`
- `build/usbforge-live`

## Create your first ISO

1. Start Builder: `./build/usbforge-builder`
2. Open the **Builder** page
3. Set an output path (default `build/usbforge.iso`)
4. Click **Build Bootable ISO**
5. Watch the log for feedback

Or from the terminal:

```bash
make iso
```

## Write to USB

1. Plug in the USB stick
2. Click **Refresh** and select the correct drive
3. Click **Write ISO -> USB** and confirm

## Boot the Live USB Lab

1. Reboot and choose USB in the firmware boot menu
2. Use the GRUB menu for guides, or start `usbforge-live` in a graphical Linux session
3. Try **USB Device Probe** and **Help & Docs** before any install
