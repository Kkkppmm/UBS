# Writing to USB

## Write an existing ISO (recommended)

Use this for a Windows 10/11 ISO or any Linux hybrid ISO you already downloaded.

1. Open **USBForge Media Creation**
2. Accept → choose **Write an existing ISO to USB flash drive**
3. Browse to your `.iso` (example: `Win10_22H2_....iso`)
4. Select the USB flash drive → **Create**
5. Approve the administrator prompt (`pkexec` / PolicyKit, or `sudo`)

### What happens

| ISO type | Method |
|----------|--------|
| **Windows** | Partition + FAT32, copy files; splits `install.wim` if >4GB (needs `wimtools`) |
| **Linux / hybrid** | Raw `dd` image write (retries without `oflag=direct` if needed) |

USBForge runs a **preflight check** before writing. Missing packages are **installed automatically** (apt/dnf/pacman/zypper) when you approve the administrator prompt.

### Auto-install

On Create, USBForge elevates once and installs whatever is missing for the write:

| Distro | Packages |
|--------|----------|
| Debian/Ubuntu | `parted` `dosfstools` `rsync` `wimtools` `pkexec` |
| Fedora | `parted` `dosfstools` `rsync` `wimlib-utils` `polkit` |
| Arch | `parted` `dosfstools` `rsync` `wimlib` `polkit` |

Disable with `USBFORGE_NO_AUTO_DEPS=1` if you prefer manual installs.

### Manual check / install

```bash
bash scripts/write-media.sh --check-deps /path/to/windows.iso
bash scripts/write-media.sh --ensure-deps /path/to/windows.iso   # prompts for admin
bash scripts/write-media.sh --install-deps /path/to/windows.iso  # already root
```

### Required privilege helper

You still need **either** `pkexec` (PolicyKit) **or** `sudo` so USBForge can elevate. If both are missing, install one manually first:

```bash
# Debian/Ubuntu
sudo apt-get install -y pkexec
```

### Manual write helper

```bash
# Check tools only
bash scripts/write-media.sh --check-deps /path/to/windows.iso

# Write (prompts for admin; auto-installs deps)
bash scripts/usbforge-write.sh /path/to/file.iso /dev/sdX
```

## Build a new USBForge ISO

Only if you want a USBForge Live ISO (not for writing a Windows ISO):

1. Choose **Build a new USBForge ISO file**
2. Pick a **new** output path such as `~/usbforge.iso` (do not select your Windows ISO)
3. Create

## Manual write (Linux hybrid only)

```bash
sudo dd if=build/usbforge.iso of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

## Windows app

The Windows installer uses `write-iso.ps1` to write an ISO to a removable drive (UAC).

## Safety

Writing bootable media **erases** the USB drive. Double-check the device.
If you selected a partition (`/dev/sdb1`), USBForge automatically uses the whole disk (`/dev/sdb`).
