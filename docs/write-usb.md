# Writing to USB

## In Builder

1. Build or open an ISO path
2. **Refresh** the USB list
3. Select the correct device (check size and model)
4. **Write ISO -> USB**
5. Confirm the warning - this **erases** the drive

The tool uses `dd` via `pkexec` so you can grant admin rights safely.

## Manual write

```bash
sudo dd if=build/usbforge.iso of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

Replace `/dev/sdX` with your USB device (**not** a partition like `/dev/sdX1`).

## Windows

1. Install `USBForge-*-windows-x64-setup.exe` (or unzip the portable package)
2. Run **USBForge**
3. Browse to a USBForge `.iso` (from GitHub Releases or built on Linux/WSL)
4. Refresh and select the USB drive letter
5. **Write ISO -> USB** and approve the UAC prompt

The bundled `write-iso.ps1` writes to the physical disk and **erases** it.
