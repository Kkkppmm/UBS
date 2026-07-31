#!/usr/bin/env bash
# write-media.sh — write a bootable ISO to a USB drive.
# Handles Microsoft Windows ISOs (extract + FAT32, split WIM if needed)
# and hybrid/Linux ISOs (raw dd).
#
# Usage: write-media.sh <iso-path> <block-device>   e.g. /dev/sdb
# Must run as root (pkexec/sudo).
set -euo pipefail

ISO="${1:-}"
DEV="${2:-}"

log() { echo "[usbforge] $*"; }
die() { echo "[usbforge] ERROR: $*" >&2; exit 1; }

[[ -n "$ISO" && -n "$DEV" ]] || die "usage: write-media.sh <iso> <device>"
[[ -f "$ISO" ]] || die "ISO not found: $ISO"
[[ -b "$DEV" ]] || die "Not a block device: $DEV"

name="$(basename "$DEV")"
if [[ -f "/sys/block/$name/removable" ]] && [[ "$(cat "/sys/block/$name/removable")" == "0" ]]; then
    if [[ "${USBFORGE_ALLOW_SYSTEM_DISK:-}" != "1" ]]; then
        die "Refusing non-removable disk $DEV (set USBFORGE_ALLOW_SYSTEM_DISK=1 to override)"
    fi
fi

ISO_MNT="$(mktemp -d /tmp/usbforge-iso.XXXXXX)"
USB_MNT="$(mktemp -d /tmp/usbforge-usb.XXXXXX)"
cleanup() {
    sync || true
    umount "$USB_MNT" 2>/dev/null || true
    umount "$ISO_MNT" 2>/dev/null || true
    rmdir "$USB_MNT" 2>/dev/null || true
    rmdir "$ISO_MNT" 2>/dev/null || true
}
trap cleanup EXIT

log "Inspecting ISO..."
mount -o loop,ro "$ISO" "$ISO_MNT"

is_windows=0
if [[ -e "$ISO_MNT/bootmgr" || -e "$ISO_MNT/bootmgr.efi" || -d "$ISO_MNT/sources" ]]; then
    is_windows=1
fi
if [[ -f "$ISO_MNT/sources/install.wim" || -f "$ISO_MNT/sources/install.esd" || -f "$ISO_MNT/sources/boot.wim" ]]; then
    is_windows=1
fi

if [[ "$is_windows" -eq 0 ]]; then
    log "Detected hybrid/Linux-style ISO — writing with dd (raw image)..."
    umount "$ISO_MNT"
    rmdir "$ISO_MNT"
    ISO_MNT=""
    umount "$USB_MNT" 2>/dev/null || true
    rmdir "$USB_MNT" 2>/dev/null || true
    USB_MNT=""
    trap - EXIT

    # Unmount any partitions before raw write
    for p in "$DEV"*; do
        [[ -b "$p" ]] || continue
        umount "$p" 2>/dev/null || true
    done

    log "dd if=$ISO of=$DEV bs=4M status=progress conv=fsync oflag=direct"
    dd if="$ISO" of="$DEV" bs=4M status=progress conv=fsync oflag=direct
    sync
    log "SUCCESS: raw ISO written to $DEV"
    exit 0
fi

log "Detected Microsoft Windows ISO — extracting to USB (FAT32)..."
command -v parted >/dev/null || die "parted is required for Windows USB creation"
command -v mkfs.vfat >/dev/null || die "dosfstools (mkfs.vfat) is required"
command -v rsync >/dev/null || die "rsync is required for Windows USB creation"

for p in "$DEV"*; do
    [[ -b "$p" ]] || continue
    umount "$p" 2>/dev/null || true
done
sleep 1

log "Partitioning $DEV (MBR + single FAT32 partition)..."
wipefs -a "$DEV" 2>/dev/null || true
parted -s "$DEV" mklabel msdos
parted -s "$DEV" mkpart primary fat32 1MiB 100%
parted -s "$DEV" set 1 boot on
partprobe "$DEV" 2>/dev/null || true
sleep 2

PART=""
for cand in "${DEV}1" "${DEV}p1"; do
    if [[ -b "$cand" ]]; then PART="$cand"; break; fi
done
[[ -n "$PART" ]] || die "Could not find partition on $DEV after partitioning"

log "Formatting $PART as FAT32..."
mkfs.vfat -F 32 -n "WINSETUP" "$PART"

log "Mounting USB partition..."
mount "$PART" "$USB_MNT"

WIM="$ISO_MNT/sources/install.wim"
NEED_SPLIT=0
if [[ -f "$WIM" ]]; then
    WIM_SIZE=$(stat -c%s "$WIM")
    if (( WIM_SIZE > 4294967294 )); then
        NEED_SPLIT=1
    fi
fi

if [[ "$NEED_SPLIT" -eq 1 ]]; then
    if ! command -v wimlib-imagex >/dev/null 2>&1 && ! command -v wimsplit >/dev/null 2>&1; then
        die "install.wim is larger than 4GB. Install wimtools (package: wimtools) and retry."
    fi
    log "install.wim is >4GB — copying files then splitting WIM for FAT32..."
    rsync -a --info=progress2 --exclude 'sources/install.wim' "$ISO_MNT"/ "$USB_MNT"/
    mkdir -p "$USB_MNT/sources"
    if command -v wimlib-imagex >/dev/null 2>&1; then
        wimlib-imagex split "$WIM" "$USB_MNT/sources/install.swm" 3800
    else
        wimsplit "$WIM" "$USB_MNT/sources/install.swm" 3800
    fi
else
    log "Copying Windows files to USB (this can take several minutes)..."
    rsync -a --info=progress2 "$ISO_MNT"/ "$USB_MNT"/
fi

sync
log "SUCCESS: Windows bootable USB created on $DEV ($PART)"
log "You can reboot and boot from this USB flash drive."
exit 0
