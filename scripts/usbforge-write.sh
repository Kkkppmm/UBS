#!/usr/bin/env bash
# usbforge-write — elevate and run write-media.sh (pkexec preferred, sudo fallback).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="$ROOT/write-media.sh"
[[ -x "$SCRIPT" || -f "$SCRIPT" ]] || SCRIPT="/usr/share/usbforge/scripts/write-media.sh"
[[ -f "$SCRIPT" ]] || { echo "[usbforge] ERROR: write-media.sh not found" >&2; exit 1; }

if [[ "${1:-}" == "--check-deps" ]]; then
    exec bash "$SCRIPT" --check-deps "${2:-}"
fi

ISO="${1:-}"
DEV="${2:-}"
[[ -n "$ISO" && -n "$DEV" ]] || {
    echo "usage: usbforge-write <iso> <device>" >&2
    exit 1
}

# Preflight without root so the UI can show a clear message
if ! bash "$SCRIPT" --check-deps "$ISO"; then
    exit 2
fi

run_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        exec bash "$SCRIPT" "$ISO" "$DEV"
    fi
    if command -v pkexec >/dev/null 2>&1; then
        # Preserve a sane PATH; pkexec clears env by default on some systems
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
