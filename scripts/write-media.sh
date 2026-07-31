#!/usr/bin/env bash
# write-media.sh — write a bootable ISO to a USB drive.
# Handles Microsoft Windows ISOs (extract + FAT32, split WIM if needed)
# and hybrid/Linux ISOs (raw dd).
#
# Usage:
#   write-media.sh <iso-path> <block-device>     e.g. /dev/sdb
#   write-media.sh --check-deps [iso-path]      exit 0 if ready; print missing pkgs
# Must run as root for the write path (pkexec/sudo).
set -euo pipefail

ISO="${1:-}"
DEV="${2:-}"

log() { echo "[usbforge] $*"; }
die() { echo "[usbforge] ERROR: $*" >&2; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

need_root() {
    if [[ "$(id -u)" -ne 0 ]]; then
        die "Must run as root (use pkexec or sudo). Example: pkexec $0 <iso> <device>"
    fi
}

# --- dependency preflight (no root required) ---
check_deps() {
    local iso="${1:-}"
    local missing=()
    local warn=()
    local is_win=0

    if [[ "$(id -u)" -ne 0 ]]; then
        if ! have pkexec && ! have sudo; then
            missing+=("pkexec|policykit-1 (or sudo)")
        fi
    fi

    if [[ -n "$iso" && -f "$iso" ]]; then
        if have xorriso; then
            if xorriso -indev "$iso" -find /bootmgr -exec report_lba -- 2>/dev/null | grep -q bootmgr \
                || xorriso -indev "$iso" -find /sources -exec report_lba -- 2>/dev/null | grep -q sources; then
                is_win=1
            fi
        fi
        local base
        base="$(basename "$iso")"
        if [[ "$base" == Win* || "$base" == *Windows* || "$base" == *WIN1* || "$base" == *Win1* ]]; then
            is_win=1
        fi
    else
        # Assume Windows path tools may be needed when ISO unknown
        is_win=1
    fi

    if [[ "$is_win" -eq 1 ]]; then
        have parted || missing+=("parted")
        have mkfs.vfat || missing+=("dosfstools")
        have rsync || missing+=("rsync")
        have wipefs || missing+=("util-linux")
        if ! have wimlib-imagex && ! have wimsplit; then
            warn+=("wimtools (required only when install.wim is larger than 4GB)")
        fi
    fi
    have dd || missing+=("coreutils")
    have mount || missing+=("util-linux")

    if ((${#missing[@]})); then
        echo "[usbforge] MISSING: ${missing[*]}"
        echo "[usbforge] Install (Debian/Ubuntu): sudo apt-get install -y pkexec parted dosfstools rsync wimtools"
        echo "[usbforge] Install (Fedora): sudo dnf install -y polkit parted dosfstools rsync wimlib-utils"
        return 1
    fi
    if ((${#warn[@]})); then
        echo "[usbforge] NOTE: ${warn[*]}"
    fi
    echo "[usbforge] Dependencies OK"
    return 0
}

if [[ "${1:-}" == "--check-deps" ]]; then
    check_deps "${2:-}"
    exit $?
fi

[[ -n "$ISO" && -n "$DEV" ]] || die "usage: write-media.sh <iso> <device>"
[[ -f "$ISO" ]] || die "ISO not found: $ISO"
[[ -b "$DEV" ]] || die "Not a block device: $DEV"
need_root

# Resolve /dev/disk/by-id etc to real device node
if have readlink; then
    REAL="$(readlink -f "$DEV" 2>/dev/null || true)"
    [[ -n "$REAL" && -b "$REAL" ]] && DEV="$REAL"
fi

name="$(basename "$DEV")"
# If the user picked a partition (e.g. /dev/sdb1), use the whole disk.
if [[ -e "/sys/class/block/$name/partition" ]]; then
    parent="$(basename "$(dirname "$(readlink -f "/sys/class/block/$name")")")"
    if [[ -b "/dev/$parent" ]]; then
        log "Selected partition $DEV — using whole disk /dev/$parent"
        DEV="/dev/$parent"
        name="$parent"
    fi
fi

if [[ -f "/sys/block/$name/removable" ]] && [[ "$(cat "/sys/block/$name/removable")" == "0" ]]; then
    bus=""
    if have udevadm; then
        bus="$(udevadm info --query=property --name="$DEV" 2>/dev/null | sed -n 's/^ID_BUS=//p' | head -1 || true)"
    fi
    if [[ "$bus" != "usb" && "${USBFORGE_ALLOW_SYSTEM_DISK:-}" != "1" ]]; then
        die "Refusing non-removable disk $DEV (bus=${bus:-unknown}). Set USBFORGE_ALLOW_SYSTEM_DISK=1 to override."
    fi
fi

ISO_MNT="$(mktemp -d /tmp/usbforge-iso.XXXXXX)"
USB_MNT="$(mktemp -d /tmp/usbforge-usb.XXXXXX)"
cleanup() {
    sync || true
    [[ -n "${USB_MNT:-}" ]] && umount "$USB_MNT" 2>/dev/null || true
    [[ -n "${ISO_MNT:-}" ]] && umount "$ISO_MNT" 2>/dev/null || true
    [[ -n "${USB_MNT:-}" ]] && rmdir "$USB_MNT" 2>/dev/null || true
    [[ -n "${ISO_MNT:-}" ]] && rmdir "$ISO_MNT" 2>/dev/null || true
}
trap cleanup EXIT

unmount_all() {
    local p
    for p in "$DEV"*; do
        [[ -b "$p" ]] || continue
        umount "$p" 2>/dev/null || true
    done
    # Also try findmnt-based cleanup
    if have findmnt; then
        findmnt -nr -o TARGET -S "$DEV" 2>/dev/null | while read -r t; do
            umount "$t" 2>/dev/null || true
        done || true
    fi
}

raw_dd() {
    local iso="$1" dev="$2"
    log "dd if=$iso of=$dev bs=4M status=progress conv=fsync"
    if dd if="$iso" of="$dev" bs=4M status=progress conv=fsync oflag=direct 2>/tmp/usbforge-dd.err; then
        return 0
    fi
    log "dd with oflag=direct failed — retrying without oflag=direct..."
    cat /tmp/usbforge-dd.err >&2 || true
    dd if="$iso" of="$dev" bs=4M status=progress conv=fsync
}

rsync_copy() {
    if rsync --info=help 2>&1 | grep -q 'progress2'; then
        rsync -a --info=progress2 "$@"
    else
        rsync -a --progress "$@"
    fi
}

wait_for_part() {
    local dev="$1"
    local i cand
    for i in 1 2 3 4 5 6 7 8 9 10; do
        for cand in "${dev}1" "${dev}p1"; do
            if [[ -b "$cand" ]]; then
                echo "$cand"
                return 0
            fi
        done
        partprobe "$dev" 2>/dev/null || true
        if have blockdev; then
            blockdev --rereadpt "$dev" 2>/dev/null || true
        fi
        if have udevadm; then
            udevadm settle --timeout=5 2>/dev/null || true
        fi
        sleep 1
    done
    return 1
}

log "Inspecting ISO..."
if ! mount -o loop,ro "$ISO" "$ISO_MNT" 2>/tmp/usbforge-mount.err; then
    cat /tmp/usbforge-mount.err >&2 || true
    die "Could not mount ISO (is the file a valid ISO? loop device available?)"
fi

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

    unmount_all
    sleep 1
    raw_dd "$ISO" "$DEV"
    sync
    log "SUCCESS: raw ISO written to $DEV"
    exit 0
fi

log "Detected Microsoft Windows ISO — extracting to USB (FAT32)..."
have parted || die "parted is required for Windows USB creation — install package: parted"
have mkfs.vfat || die "mkfs.vfat is required — install package: dosfstools"
have rsync || die "rsync is required for Windows USB creation — install package: rsync"

unmount_all
sleep 1

log "Partitioning $DEV (MBR + single FAT32 partition)..."
wipefs -a "$DEV" 2>/dev/null || true
dd if=/dev/zero of="$DEV" bs=1M count=4 status=none conv=fsync 2>/dev/null || true
parted -s "$DEV" mklabel msdos
parted -s "$DEV" mkpart primary fat32 1MiB 100%
parted -s "$DEV" set 1 boot on
partprobe "$DEV" 2>/dev/null || true
if have udevadm; then
    udevadm settle --timeout=8 2>/dev/null || true
fi
sleep 1

PART="$(wait_for_part "$DEV" || true)"
[[ -n "$PART" ]] || die "Could not find partition on $DEV after partitioning (tried ${DEV}1 and ${DEV}p1)"

log "Formatting $PART as FAT32..."
wipefs -a "$PART" 2>/dev/null || true
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

# Rough capacity check (ISO used size vs partition)
ISO_USED=$(du -sb "$ISO_MNT" 2>/dev/null | awk '{print $1}')
PART_SIZE=$(blockdev --getsize64 "$PART" 2>/dev/null || echo 0)
if [[ -n "$ISO_USED" && "$PART_SIZE" -gt 0 && "$ISO_USED" -gt "$PART_SIZE" ]]; then
    die "USB partition is too small ($PART_SIZE bytes) for this ISO contents ($ISO_USED bytes)"
fi

if [[ "$NEED_SPLIT" -eq 1 ]]; then
    if ! have wimlib-imagex && ! have wimsplit; then
        die "install.wim is larger than 4GB. Install wimtools (Debian/Ubuntu: wimtools) and retry."
    fi
    log "install.wim is >4GB — copying files then splitting WIM for FAT32..."
    rsync_copy --exclude 'sources/install.wim' "$ISO_MNT"/ "$USB_MNT"/
    mkdir -p "$USB_MNT/sources"
    if have wimlib-imagex; then
        # 3800 MiB chunks stay under FAT32 4GiB file limit
        wimlib-imagex split "$WIM" "$USB_MNT/sources/install.swm" 3800
    else
        wimsplit "$WIM" "$USB_MNT/sources/install.swm" 3800
    fi
else
    log "Copying Windows files to USB (this can take several minutes)..."
    rsync_copy "$ISO_MNT"/ "$USB_MNT"/
fi

sync
# Ensure boot files landed
if [[ ! -e "$USB_MNT/bootmgr" && ! -e "$USB_MNT/bootmgr.efi" && ! -d "$USB_MNT/efi" ]]; then
    die "Copy finished but Windows boot files are missing on $PART — ISO may be incomplete"
fi

log "SUCCESS: Windows bootable USB created on $DEV ($PART)"
log "You can reboot and boot from this USB flash drive."
exit 0
