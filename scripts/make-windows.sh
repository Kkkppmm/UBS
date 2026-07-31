#!/usr/bin/env bash
# Cross-compile Windows builder + create NSIS installer
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(grep '#define USBFORGE_VERSION' "$ROOT/common/usbforge.h" | awk '{print $3}' | tr -d '"')"
WIN="$ROOT/build/windows"
OUT="$ROOT/build/release"
MINGW="${MINGW_CC:-x86_64-w64-mingw32-gcc}"

echo "[win] Cross-compiling USBForge.exe with $MINGW"

command -v "$MINGW" >/dev/null || { echo "Missing $MINGW (install gcc-mingw-w64-x86-64)"; exit 1; }
command -v makensis >/dev/null || { echo "Missing makensis (install nsis)"; exit 1; }

mkdir -p "$WIN" "$OUT"

"$MINGW" -O2 -Wall -Wextra -D_WIN32 -I"$ROOT/common" \
    "$ROOT/host/builder_win.c" "$ROOT/common/usbforge.c" \
    -o "$WIN/USBForge.exe" \
    -mwindows -lcomctl32 -lcomdlg32 -lshell32 -luser32 -lgdi32 -lkernel32

ls -lh "$WIN/USBForge.exe"

# Portable zip (no installer)
PORTABLE="$OUT/USBForge-${VERSION}-windows-x64-portable"
rm -rf "$PORTABLE"
mkdir -p "$PORTABLE/docs" "$PORTABLE/scripts"
cp "$WIN/USBForge.exe" "$PORTABLE/"
cp "$ROOT/scripts/write-iso.ps1" "$PORTABLE/"
cp "$ROOT/scripts/write-iso.ps1" "$PORTABLE/scripts/"
cp "$ROOT/LICENSE" "$ROOT/README.md" "$PORTABLE/"
cp "$ROOT/docs/"* "$PORTABLE/docs/"
(
  cd "$OUT"
  rm -f "USBForge-${VERSION}-windows-x64-portable.zip"
  if command -v zip >/dev/null; then
    zip -r "USBForge-${VERSION}-windows-x64-portable.zip" "USBForge-${VERSION}-windows-x64-portable"
  else
    tar -czf "USBForge-${VERSION}-windows-x64-portable.tar.gz" "USBForge-${VERSION}-windows-x64-portable"
  fi
)

# NSIS installer (keep APP_VERSION in sync with usbforge.h)
echo "[win] Building NSIS installer..."
sed -i "s/^!define APP_VERSION .*/!define APP_VERSION \"$VERSION\"/" \
  "$ROOT/packaging/windows/usbforge.nsi"
makensis -V2 "$ROOT/packaging/windows/usbforge.nsi"
ls -lh "$OUT/USBForge-${VERSION}-windows-x64-setup.exe"
echo "[win] Done."
