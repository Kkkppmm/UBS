#!/usr/bin/env bash
# Build all release packages: Linux (deb/rpm/tarball/iso) + Windows installer
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(grep '#define USBFORGE_VERSION' "$ROOT/common/usbforge.h" | awk '{print $3}' | tr -d '"')"
OUT="$ROOT/build/release"

echo "========================================"
echo " USBForge $VERSION release packages"
echo "========================================"

mkdir -p "$OUT"
make -C "$ROOT" all iso

# Generic Linux tarball (distro-agnostic)
TAR_NAME="usbforge-${VERSION}-linux-x86_64"
STAGE="$OUT/$TAR_NAME"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/docs" "$STAGE/scripts" "$STAGE/share/applications" "$STAGE/share/icons/hicolor"
cp "$ROOT/build/usbforge-builder" "$ROOT/build/usbforge-live" "$STAGE/bin/"
cp "$ROOT/scripts/usbforge-update.sh" "$STAGE/bin/usbforge-update"
cp "$ROOT/docs/"* "$STAGE/docs/"
cp "$ROOT/scripts/"*.sh "$STAGE/scripts/" 2>/dev/null || true
cp "$ROOT/packaging/linux/"*.desktop "$STAGE/share/applications/"
cp -a "$ROOT/packaging/linux/icons/hicolor/." "$STAGE/share/icons/hicolor/"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$STAGE/"
cat > "$STAGE/install.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
PREFIX="${PREFIX:-/usr/local}"
DIR="$(cd "$(dirname "$0")" && pwd)"
install -d "$PREFIX/bin" "$PREFIX/share/usbforge/docs" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor"
install -m755 "$DIR/bin/"* "$PREFIX/bin/"
install -m644 "$DIR/docs/"* "$PREFIX/share/usbforge/docs/"
install -m644 "$DIR/share/applications/"* "$PREFIX/share/applications/" 2>/dev/null || true
cp -a "$DIR/share/icons/hicolor/." "$PREFIX/share/icons/hicolor/" 2>/dev/null || true
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
fi
echo "Installed USBForge to $PREFIX"
echo "Open your app menu and search for: USBForge Builder / USBForge Live"
echo "Or run: usbforge-builder"
EOF
chmod +x "$STAGE/install.sh" "$STAGE/scripts/"*.sh "$STAGE/bin/"*
(
  cd "$OUT"
  tar -czf "${TAR_NAME}.tar.gz" "$TAR_NAME"
)
echo "[ok] $OUT/${TAR_NAME}.tar.gz"

# Copy ISO into release folder
if [[ -f "$ROOT/build/usbforge.iso" ]]; then
  cp -f "$ROOT/build/usbforge.iso" "$OUT/usbforge-${VERSION}.iso"
  echo "[ok] $OUT/usbforge-${VERSION}.iso"
fi

# Distro packages (do not wipe $OUT)
bash "$ROOT/scripts/make-deb.sh"
bash "$ROOT/scripts/make-rpm.sh"
bash "$ROOT/scripts/make-windows.sh"

# Checksums
(
  cd "$OUT"
  : > SHA256SUMS.txt
  find . -maxdepth 1 -type f \( \
      -name '*.deb' -o -name '*.rpm' -o -name '*.tar.gz' -o -name '*.iso' \
      -o -name '*.exe' -o -name '*.zip' \) -printf '%P\n' | sort | while read -r f; do
    sha256sum "$f" >> SHA256SUMS.txt
  done
  echo "---- SHA256SUMS ----"
  cat SHA256SUMS.txt
)

echo ""
echo "Release artifacts in: $OUT"
ls -lh "$OUT"
echo "Done."
