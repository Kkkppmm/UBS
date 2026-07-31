# Safety

## Golden rules

1. **Double-check the device path** before writing (`/dev/sdb` vs `/dev/sda`).
2. Writing an ISO to a disk **destroys all data** on that disk.
3. Prefer removable/USB entries shown by USBForge; still verify size and model.
4. The Live **read speed test** is read-only - safe to explore.
5. “Install / Copy Tools” may need mounts and root; it is not a full OS installer.

## If you pick the wrong disk

Stop immediately, do not write again, and restore from backups if you have them. USBForge cannot undo a `dd` write.

## Permissions

- Building an ISO: normal user is fine
- Writing to `/dev/sdX`: needs administrator rights
- Raw USB read tests: often need root for `/dev/sdX`
