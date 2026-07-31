# USBForge

**USBForge** creates bootable ISO/USB media with a **Windows Media Creation Tool–style wizard**, then boots into a USB Lab for testing and docs.

## What you get

| Tool | Role |
|------|------|
| `usbforge-builder` | Host GUI (Linux/WebKit): Media Creation wizard with review step + auto-deps |
| `usbforge-live` | Live/boot GUI: USB probe, read-speed test, diagnostics, docs |
| `USBForge.exe` | Windows writer + help (NSIS installer / portable zip) |
| `scripts/write-media.sh` | Writes Windows/Linux ISOs to USB (auto-installs tools) |
| `scripts/build-iso.sh` | Packages a GRUB-bootable ISO |

## Quick start (Linux)

```bash
sudo apt-get install -y build-essential pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev \
    xorriso grub-common grub-pc-bin mtools parted dosfstools rsync wimtools pkexec

make
make smoke
make iso                 # -> build/usbforge.iso
./build/usbforge-builder   # WebKit + ui/builder (HTML/CSS/JS)
./build/usbforge-live
```

USB writes **auto-install** missing tools (`parted`, `dosfstools`, `rsync`, `wimtools`) when you approve the admin prompt.

## Release packages

```bash
# Needs: fakeroot, rpm, nsis, gcc-mingw-w64-x86-64, zip
make install-deps
make release             # -> build/release/
```

| Artifact | Platform |
|----------|----------|
| `usbforge_*_amd64.deb` | Debian / Ubuntu |
| `usbforge-*.rpm` | Fedora / RHEL family |
| `usbforge-*-linux-x86_64.tar.gz` | Any Linux |
| `usbforge-*.iso` | Bootable media |
| `USBForge-*-windows-x64-setup.exe` | Windows installer |
| `USBForge-*-windows-x64-portable.zip` | Windows portable |

See **docs/packages.md** for install commands and distro notes.

## Typical workflow

1. Build or download a USBForge ISO.
2. Write it to USB (Linux Builder or Windows app).
3. Boot from USB -> GRUB / Live USB Lab (test & docs first; install optional).

## Project layout

```
common/      Shared C helpers (Linux + Windows)
host/        Builder GUI (GTK3) + Windows Win32 builder
live/        Live USB Lab GUI (GTK3)
docs/        In-app help topics
packaging/   Linux deb/rpm/arch + Windows NSIS
scripts/     ISO build, release, Windows write helper
```

## Safety

Writing an ISO to a disk **erases** it. Always confirm the target drive. The Live read-speed test is **read-only**.

## License

MIT - see `LICENSE`.
