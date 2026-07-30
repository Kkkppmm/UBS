# Creating an ISO

USBForge packages:

- `usbforge-live` - Live USB Lab GUI
- `usbforge-builder` - host builder (also on the ISO)
- Help docs under `/usbforge/docs`
- GRUB boot menu with guides

## From the Builder GUI

1. Choose an output `.iso` path
2. Click **Build Bootable ISO**
3. Progress and command output appear in the **Feedback / Log** panel
4. Optionally click **Verify ISO**

## From the command line

```bash
bash scripts/build-iso.sh build/usbforge.iso
# or
make iso
```

Requirements: `xorriso` and preferably `grub-mkrescue` (package `grub-common` / `grub-pc-bin`).

## What “bootable” means here

The ISO is a **GRUB-bootable hybrid image** with the USBForge payload. The Live GUI runs when you launch `usbforge-live` from a Linux graphical session (or a full live OS that includes that binary). The GRUB menu itself provides a bootable help/interface on firmware boot.
