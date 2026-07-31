# Live USB Lab

When you boot USBForge media (or run `usbforge-live` on a Linux desktop), you get a **USB Lab** - more than an installer.

## Home menu

| Action | Purpose |
|--------|---------|
| USB Device Probe | List USB/removable disks with model, size, I/O stats |
| USB Read Speed Test | Safe **read-only** sequential sample |
| System Diagnostics | Kernel, memory, CPU, disk free space |
| Help & Docs | Built-in documentation |
| Install / Copy Tools | Optional: copy USBForge onto a USB |
| Device List | Live view of block devices |

## Design goal

Install is available, but secondary. First-class features are **testing**, **feedback**, and **help** so you can trust media before you commit.

## Feedback

Every tool writes clear results to the Results page and updates the status bar at the bottom.
