#!/usr/bin/env bash
# Build a bootable USBForge ISO (GRUB + Live payload + docs).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/build/usbforge.iso}"
STAGE="$ROOT/build/iso-root"
LABEL="USBFORGE"

echo "[usbforge] Building bootable ISO -> $OUT"

mkdir -p "$ROOT/build"
make -C "$ROOT" all

rm -rf "$STAGE"
mkdir -p "$STAGE/boot/grub"
mkdir -p "$STAGE/usbforge/bin"
mkdir -p "$STAGE/usbforge/docs"
mkdir -p "$STAGE/usbforge/scripts"

cp -a "$ROOT/build/usbforge-builder" "$STAGE/usbforge/bin/"
cp -a "$ROOT/build/usbforge-live" "$STAGE/usbforge/bin/"
cp -a "$ROOT/docs/." "$STAGE/usbforge/docs/"
cp -a "$ROOT/scripts/"*.sh "$STAGE/usbforge/scripts/" 2>/dev/null || true
mkdir -p "$STAGE/usbforge/ui"
cp -a "$ROOT/ui/." "$STAGE/usbforge/ui/" 2>/dev/null || true

# README on ISO root
cat > "$STAGE/README.TXT" <<'EOF'
USBForge Live ISO
=================
Boot this media from firmware (USB/DVD) to open the GRUB menu.

When running inside a Linux environment with graphics:
  /usbforge/bin/usbforge-live

Host builder (create/write more ISOs):
  /usbforge/bin/usbforge-builder

Docs:
  /usbforge/docs/
EOF

# GRUB boot menu - bootable interface before/alongside the Live GUI
cat > "$STAGE/boot/grub/grub.cfg" <<'EOF'
set timeout=12
set default=0

insmod all_video
insmod gfxterm
insmod png
terminal_output gfxterm
set gfxmode=auto

set menu_color_normal=light-cyan/black
set menu_color_highlight=black/light-cyan
set color_normal=light-gray/black

menuentry "USBForge Live - USB Lab (recommended)" {
    echo "USBForge Live payload is on this ISO under /usbforge/"
    echo ""
    echo "If a Linux live kernel is present, it would start usbforge-live here."
    echo "This ISO ships the Live GUI, docs, and builder tools."
    echo ""
    echo "From a running Linux session mounted from this media:"
    echo "  /usbforge/bin/usbforge-live"
    echo ""
    echo "Press Escape to return to the menu..."
    sleep 8
}

menuentry "USB Device Testing Guide" {
    echo "========================================"
    echo " USBForge - USB Testing"
    echo "========================================"
    echo ""
    echo "1. Boot into a Linux desktop (or use host Builder)."
    echo "2. Run: /usbforge/bin/usbforge-live"
    echo "3. Choose 'USB Device Probe' or 'USB Read Speed Test'."
    echo "4. Review results - no writes are performed by the speed test."
    echo ""
    echo "Press Escape to return..."
    sleep 10
}

menuentry "Help & Documentation" {
    echo "========================================"
    echo " USBForge Help"
    echo "========================================"
    echo ""
    echo "Docs on this media: /usbforge/docs/"
    echo "  index.md           Overview"
    echo "  getting-started.md First steps"
    echo "  create-iso.md      Building ISOs"
    echo "  write-usb.md       Writing to USB"
    echo "  live-lab.md        Live session tools"
    echo "  usb-testing.md     Testing safely"
    echo "  safety.md          Don't wipe the wrong disk"
    echo ""
    echo "Press Escape to return..."
    sleep 12
}

menuentry "System / About" {
    echo "USBForge ${grub_version}"
    echo "Bootable toolkit: create ISO, test USB, install tools."
    echo "Live session is more than an installer - USB Lab first."
    echo ""
    echo "Version 1.0.0"
    sleep 6
}

menuentry "Reboot" {
    reboot
}

menuentry "Shutdown" {
    halt
}
EOF

# Prefer grub-mkrescue for a hybrid BIOS/UEFI-friendly ISO
if command -v grub-mkrescue >/dev/null 2>&1; then
    echo "[usbforge] Using grub-mkrescue..."
    grub-mkrescue -o "$OUT" "$STAGE" -- -volid "$LABEL" 2>&1 | tail -n 20
else
    echo "[usbforge] Using xorriso fallback..."
    xorriso -as mkisofs \
        -R -J -V "$LABEL" \
        -o "$OUT" \
        "$STAGE"
fi

ls -lh "$OUT"
echo "[usbforge] Done. Write with: sudo dd if=$OUT of=/dev/sdX bs=4M status=progress conv=fsync"
