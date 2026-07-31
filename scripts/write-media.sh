#!/usr/bin/env bash
# write-media.sh — write a bootable ISO to a USB drive.
# Handles Microsoft Windows ISOs (extract + FAT32, split WIM if needed)
# and hybrid/Linux ISOs (raw dd).
#
# Usage:
#   write-media.sh <iso-path> <block-device>     e.g. /dev/sdb
#   write-media.sh --check-deps [iso-path]      exit 0 if ready; print missing pkgs
#   write-media.sh --install-deps [iso-path]    install missing pkgs (root)
# Must run as root for the write path (pkexec/sudo).
# Missing tools are auto-installed when running as root (unless USBFORGE_NO_AUTO_DEPS=1).
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

can_elevate() {
    [[ "$(id -u)" -eq 0 ]] || have pkexec || have sudo
}

detect_windows_iso() {
    local iso="${1:-}"
    [[ -n "$iso" && -f "$iso" ]] || { echo 1; return; }
    if have xorriso; then
        if xorriso -indev "$iso" -find /bootmgr -exec report_lba -- 2>/dev/null | grep -q bootmgr \
            || xorriso -indev "$iso" -find /sources -exec report_lba -- 2>/dev/null | grep -q sources; then
            echo 1
            return
        fi
    fi
    local base
    base="$(basename "$iso")"
    if [[ "$base" == Win* || "$base" == *Windows* || "$base" == *WIN1* || "$base" == *Win1* ]]; then
        echo 1
        return
    fi
    echo 0
}

# Populate global-ish arrays via namerefs-style echo — return missing cmds as lines
collect_missing_cmds() {
    local iso="${1:-}"
    local is_win
    is_win="$(detect_windows_iso "$iso")"

    if [[ "$(id -u)" -ne 0 ]]; then
        if ! have pkexec && ! have sudo; then
            echo "elevation"
        fi
    fi

    if [[ "$is_win" == "1" ]]; then
        have parted || echo "parted"
        have mkfs.vfat || echo "dosfstools"
        have rsync || echo "rsync"
        have wipefs || echo "util-linux"
        # Always pull wimtools for Windows ISOs so large WIMs work without a second pass
        if ! have wimlib-imagex && ! have wimsplit; then
            echo "wimtools"
        fi
    fi
    have dd || echo "coreutils"
    have mount || echo "util-linux"
}

# Map logical need → distro package names (space-separated on stdout)
packages_for_distro() {
    local kind="$1" # apt|dnf|pacman|zypper
    local need="$2"
    case "$kind:$need" in
        apt:elevation) echo "pkexec policykit-1" ;;
        apt:parted) echo "parted" ;;
        apt:dosfstools) echo "dosfstools" ;;
        apt:rsync) echo "rsync" ;;
        apt:util-linux) echo "util-linux" ;;
        apt:wimtools) echo "wimtools" ;;
        apt:coreutils) echo "coreutils" ;;
        dnf:elevation) echo "polkit" ;;
        dnf:parted) echo "parted" ;;
        dnf:dosfstools) echo "dosfstools" ;;
        dnf:rsync) echo "rsync" ;;
        dnf:util-linux) echo "util-linux" ;;
        dnf:wimtools) echo "wimlib-utils" ;;
        dnf:coreutils) echo "coreutils" ;;
        pacman:elevation) echo "polkit" ;;
        pacman:parted) echo "parted" ;;
        pacman:dosfstools) echo "dosfstools" ;;
        pacman:rsync) echo "rsync" ;;
        pacman:util-linux) echo "util-linux" ;;
        pacman:wimtools) echo "wimlib" ;;
        pacman:coreutils) echo "coreutils" ;;
        zypper:elevation) echo "polkit" ;;
        zypper:parted) echo "parted" ;;
        zypper:dosfstools) echo "dosfstools" ;;
        zypper:rsync) echo "rsync" ;;
        zypper:util-linux) echo "util-linux" ;;
        zypper:wimtools) echo "wimlib-utils" ;;
        zypper:coreutils) echo "coreutils" ;;
        *) echo "" ;;
    esac
}

detect_pkg_kind() {
    if have apt-get; then echo apt
    elif have dnf; then echo dnf
    elif have yum && ! have dnf; then echo yum
    elif have pacman; then echo pacman
    elif have zypper; then echo zypper
    else echo unknown
    fi
}

resolve_packages() {
    local iso="${1:-}"
    local kind need pkgs=()
    kind="$(detect_pkg_kind)"
    [[ "$kind" != "unknown" ]] || return 1
    # yum uses same package names as dnf mapping
    local map_kind="$kind"
    [[ "$kind" == "yum" ]] && map_kind="dnf"

    while IFS= read -r need; do
        [[ -n "$need" ]] || continue
        # shellcheck disable=SC2207
        pkgs+=( $(packages_for_distro "$map_kind" "$need") )
    done < <(collect_missing_cmds "$iso" | sort -u)

    if ((${#pkgs[@]} == 0)); then
        return 1
    fi
    # unique
    printf '%s\n' "${pkgs[@]}" | awk 'NF && !seen[$0]++' | tr '\n' ' '
    return 0
}

check_deps() {
    local iso="${1:-}"
    local missing=()
    local need

    while IFS= read -r need; do
        [[ -n "$need" ]] || continue
        missing+=("$need")
    done < <(collect_missing_cmds "$iso")

    if ((${#missing[@]} == 0)); then
        echo "[usbforge] Dependencies OK"
        return 0
    fi

    echo "[usbforge] MISSING: ${missing[*]}"
    local pkgs
    if pkgs="$(resolve_packages "$iso")"; then
        echo "[usbforge] PACKAGES: $pkgs"
        if can_elevate; then
            echo "[usbforge] AUTO_INSTALL: available (will install with pkexec/sudo)"
            return 2
        fi
    fi
    echo "[usbforge] Install (Debian/Ubuntu): sudo apt-get install -y pkexec parted dosfstools rsync wimtools"
    echo "[usbforge] Install (Fedora): sudo dnf install -y polkit parted dosfstools rsync wimlib-utils"
    return 1
}

install_deps() {
    local iso="${1:-}"
    need_root

    local kind pkgs still
    kind="$(detect_pkg_kind)"
    if [[ "$kind" == "unknown" ]]; then
        die "No supported package manager found (apt-get/dnf/pacman/zypper)"
    fi

    still="$(collect_missing_cmds "$iso" | grep -v '^elevation$' || true)"
    if [[ -z "$still" ]]; then
        log "Dependencies already installed"
        return 0
    fi

    pkgs="$(resolve_packages "$iso" || true)"
    if [[ -z "${pkgs// /}" ]]; then
        die "Missing tools ($still) but could not map them to packages for $kind"
    fi

    log "Installing packages: $pkgs"
    case "$kind" in
        apt)
            export DEBIAN_FRONTEND=noninteractive
            apt-get update -y || log "apt-get update failed — continuing with existing indexes"
            # shellcheck disable=SC2086
            apt-get install -y $pkgs
            ;;
        dnf)
            # shellcheck disable=SC2086
            dnf install -y $pkgs
            ;;
        yum)
            # shellcheck disable=SC2086
            yum install -y $pkgs
            ;;
        pacman)
            # shellcheck disable=SC2086
            pacman -Sy --noconfirm --needed $pkgs
            ;;
        zypper)
            # shellcheck disable=SC2086
            zypper --non-interactive install -y $pkgs
            ;;
    esac

    still="$(collect_missing_cmds "$iso" | grep -v '^elevation$' || true)"
    if [[ -n "$still" ]]; then
        die "Still missing after install: $still"
    fi
    log "Package install complete"
    return 0
}

ensure_deps() {
    local iso="${1:-}"
    local rc=0
    set +e
    check_deps "$iso"
    rc=$?
    set -e
    if [[ $rc -eq 0 ]]; then
        return 0
    fi
    if [[ "${USBFORGE_NO_AUTO_DEPS:-}" == "1" ]]; then
        die "Missing dependencies and USBFORGE_NO_AUTO_DEPS=1"
    fi
    if [[ $rc -eq 2 ]] || can_elevate; then
        if [[ "$(id -u)" -eq 0 ]]; then
            install_deps "$iso"
            return 0
        fi
        # Elevate only for install
        local self
        self="$(readlink -f "$0" 2>/dev/null || echo "$0")"
        log "Requesting administrator permission to install USB write tools..."
        if have pkexec; then
            pkexec /bin/bash "$self" --install-deps "${iso}"
        elif have sudo; then
            sudo /bin/bash "$self" --install-deps "${iso}"
        else
            die "Need pkexec or sudo to auto-install packages"
        fi
        # Verify
        set +e
        check_deps "$iso"
        rc=$?
        set -e
        [[ $rc -eq 0 ]] || die "Dependencies still missing after auto-install"
        return 0
    fi
    die "Missing dependencies and cannot auto-install (no pkexec/sudo)"
}

if [[ "${1:-}" == "--check-deps" ]]; then
    check_deps "${2:-}"
    exit $?
fi
if [[ "${1:-}" == "--install-deps" ]]; then
    install_deps "${2:-}"
    exit $?
fi
if [[ "${1:-}" == "--ensure-deps" ]]; then
    ensure_deps "${2:-}"
    exit $?
fi

[[ -n "$ISO" && -n "$DEV" ]] || die "usage: write-media.sh <iso> <device>"
[[ -f "$ISO" ]] || die "ISO not found: $ISO"
[[ -b "$DEV" ]] || die "Not a block device: $DEV"
need_root

# Auto-install any missing write tools while we already have root
if [[ "${USBFORGE_NO_AUTO_DEPS:-}" != "1" ]]; then
    install_deps "$ISO"
fi

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
