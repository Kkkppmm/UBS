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

## After writing

- Eject safely
- Boot from USB in firmware settings / boot menu
- Use USBForge Live for testing and docs
