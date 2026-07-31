#!/usr/bin/env bash
# usbforge-write — elevate and run write-media.sh (pkexec preferred, sudo fallback).
# Auto-installs missing packages (parted, dosfstools, rsync, wimtools, …) during the
# elevated write session.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="$ROOT/write-media.sh"
[[ -x "$SCRIPT" || -f "$SCRIPT" ]] || SCRIPT="/usr/share/usbforge/scripts/write-media.sh"
[[ -f "$SCRIPT" ]] || { echo "[usbforge] ERROR: write-media.sh not found" >&2; exit 1; }

if [[ "${1:-}" == "--check-deps" ]]; then
    exec bash "$SCRIPT" --check-deps "${2:-}"
fi
if [[ "${1:-}" == "--ensure-deps" || "${1:-}" == "--install-deps" ]]; then
    exec bash "$SCRIPT" "$1" "${2:-}"
fi

ISO="${1:-}"
DEV="${2:-}"
[[ -n "$ISO" && -n "$DEV" ]] || {
    echo "usage: usbforge-write <iso> <device>" >&2
    exit 1
}

# Soft preflight: if deps missing but elevation works, continue (auto-install as root)
set +e
bash "$SCRIPT" --check-deps "$ISO"
rc=$?
set -e
if [[ $rc -eq 1 ]]; then
    echo "[usbforge] ERROR: missing packages and cannot auto-install (need pkexec or sudo)." >&2
    exit 2
fi
if [[ $rc -eq 2 ]]; then
    echo "[usbforge] Missing tools will be installed automatically (admin password may be required)."
fi

run_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        exec bash "$SCRIPT" "$ISO" "$DEV"
    fi
    if command -v pkexec >/dev/null 2>&1; then
        exec pkexec /bin/bash "$SCRIPT" "$ISO" "$DEV"
    fi
    if command -v sudo >/dev/null 2>&1; then
        exec sudo -A /bin/bash "$SCRIPT" "$ISO" "$DEV" 2>/dev/null \
            || exec sudo /bin/bash "$SCRIPT" "$ISO" "$DEV"
    fi
    echo "[usbforge] ERROR: Need pkexec (policykit-1) or sudo to write USB media." >&2
    exit 1
}

run_root
