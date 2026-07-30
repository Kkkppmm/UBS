#!/usr/bin/env bash
# Install icons + desktop entries helper used by packaging scripts
set -euo pipefail
ROOT="${1:-}"
DEST="${2:-}"
if [[ -z "$ROOT" || -z "$DEST" ]]; then
    echo "usage: install-icons.sh <repo-root> <destdir>" >&2
    exit 1
fi

install -d "$DEST/usr/share/icons/hicolor"
if [[ -d "$ROOT/packaging/linux/icons/hicolor" ]]; then
    cp -a "$ROOT/packaging/linux/icons/hicolor/." "$DEST/usr/share/icons/hicolor/"
fi
