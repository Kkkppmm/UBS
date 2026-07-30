# Release packages

USBForge ships installers and archives for Linux distros and Windows.

## Quick build

```bash
# Everything (deb, rpm, tarball, ISO, Windows setup + portable)
make release

# Individual targets
make deb        # Debian/Ubuntu .deb
make rpm        # Fedora/RHEL-style .rpm
make tarball    # Distro-agnostic .tar.gz + install.sh
make windows    # NSIS setup.exe + portable zip
make iso        # Bootable USBForge ISO
```

Artifacts land in `build/release/` with `SHA256SUMS.txt`.

## Linux

| Package | Distros | Install |
|---------|---------|---------|
| `.deb` | Debian, Ubuntu, Mint, Pop!_OS | `sudo dpkg -i usbforge_*_amd64.deb` |
| `.rpm` | Fedora, RHEL, Rocky, openSUSE* | `sudo rpm -i usbforge-*.rpm` |
| `.tar.gz` | Any Linux (x86_64) | `tar xzf ... && sudo ./install.sh` |
| Arch `PKGBUILD` | Arch / Manjaro | `makepkg -si` from `packaging/linux/arch/` |
| `.iso` | Bootable media | Write with Builder or `dd` |

\* openSUSE may need `--nodeps` or rebuilt RPMs depending on GTK package names.

Desktop entries install for **USBForge Builder** and **USBForge Live**.

### Dependencies (runtime)

- `libgtk-3` / `gtk3`
- `xorriso` (ISO verify/build)
- Recommended: `grub-common` / `grub2-tools`, `mtools`, `pkexec`

## Windows

| Artifact | Use |
|----------|-----|
| `USBForge-*-windows-x64-setup.exe` | NSIS installer (Start Menu + Desktop shortcuts) |
| `USBForge-*-windows-x64-portable.zip` | Unzip and run `USBForge.exe` |

The Windows app:

- Lists removable USB drives
- Writes a pre-built ISO to USB (UAC + PowerShell helper)
- Shows built-in help/docs
- Explains how to build ISOs on Linux or via WSL (`wsl make iso`)

Full GRUB ISO **creation** remains a Linux/WSL step; Windows focuses on writing and help.

## Cross-compile notes

Windows packages are built on Linux with:

- `gcc-mingw-w64-x86-64`
- `nsis` (`makensis`)
