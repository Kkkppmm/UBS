# USB Testing

## Device probe

Scans `/sys/block` for disks, detects USB via sysfs paths, and shows:

- Device path (`/dev/sdX`, etc.)
- Model / vendor
- Capacity
- Transport (USB / Removable / Disk)
- Kernel I/O stats when available

## Read speed test

- Opens the device **read-only**
- Reads a small sequential sample (several MiB)
- Reports approximate MiB/s
- **Does not write** and does not format the drive

Large or busy systems may need root to open raw devices.

## Tips

- Unmount filesystems on the stick before raw tests if the OS complains
- Compare results across ports (USB2 vs USB3)
- A failed open usually means permissions or the wrong path
