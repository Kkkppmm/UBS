#!/usr/bin/env bash
# Autostart helper for Live session environments.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIVE=""

for candidate in \
    "$ROOT/bin/usbforge-live" \
    /usbforge/bin/usbforge-live \
    ./build/usbforge-live \
    ./usbforge-live
do
    if [[ -x "$candidate" ]]; then
        LIVE="$candidate"
        break
    fi
done

if [[ -z "$LIVE" ]]; then
    echo "usbforge-live not found" >&2
    exit 1
fi

if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
    exec "$LIVE" "$@"
fi

echo "No graphical display detected."
echo "Start a desktop session, then run: $LIVE"
echo "Or use: $LIVE --smoke   (non-GUI check)"
exit 0
