# Writing to USB

## Write an existing ISO (recommended)

Use this for a Windows 10/11 ISO or any Linux hybrid ISO you already downloaded.

1. Open **USBForge Media Creation**
2. Accept → choose **Write an existing ISO to USB flash drive**
3. Browse to your `.iso` (example: `Win10_22H2_....iso`)
4. Select the USB flash drive → **Create**

### What happens

| ISO type | Method |
|----------|--------|
| **Windows** | Partition + FAT32, copy files; splits `install.wim` if >4GB (needs `wimtools`) |
| **Linux / hybrid** | Raw `dd` image write |

### Extra packages for Windows ISOs (Debian/Ubuntu)

```bash
sudo apt-get install -y parted dosfstools rsync wimtools
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
