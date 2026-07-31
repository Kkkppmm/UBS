#!/usr/bin/env bash
# Build Debian/Ubuntu .deb package
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(grep '#define USBFORGE_VERSION' "$ROOT/common/usbforge.h" | awk '{print $3}' | tr -d '"')"
ARCH="amd64"
PKG_NAME="usbforge_${VERSION}_${ARCH}"
STAGE="$ROOT/build/deb/$PKG_NAME"
OUT_DIR="$ROOT/build/release"

echo "[deb] Building $PKG_NAME.deb"

make -C "$ROOT" all

rm -rf "$ROOT/build/deb"
mkdir -p "$STAGE/DEBIAN"
mkdir -p "$STAGE/usr/bin"
mkdir -p "$STAGE/usr/share/usbforge/docs"
mkdir -p "$STAGE/usr/share/usbforge/scripts"
mkdir -p "$STAGE/usr/share/applications"
mkdir -p "$STAGE/usr/share/doc/usbforge"
mkdir -p "$STAGE/usr/share/icons/hicolor"

# Control file with dynamic version
sed "s/^Version:.*/Version: ${VERSION}/" \
    "$ROOT/packaging/linux/debian/control" > "$STAGE/DEBIAN/control"
cp "$ROOT/packaging/linux/debian/postinst" "$STAGE/DEBIAN/postinst"
cp "$ROOT/packaging/linux/debian/postrm" "$STAGE/DEBIAN/postrm"
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"

install -m755 "$ROOT/build/usbforge-builder" "$STAGE/usr/bin/"
install -m755 "$ROOT/build/usbforge-live" "$STAGE/usr/bin/"
install -m755 "$ROOT/scripts/usbforge-update.sh" "$STAGE/usr/bin/usbforge-update"
install -m644 "$ROOT/docs/"* "$STAGE/usr/share/usbforge/docs/"
install -m755 "$ROOT/scripts/build-iso.sh" "$STAGE/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/write-media.sh" "$STAGE/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/usbforge-write.sh" "$STAGE/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/live-autostart.sh" "$STAGE/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/usbforge-update.sh" "$STAGE/usr/share/usbforge/scripts/"
mkdir -p "$STAGE/usr/share/polkit-1/actions"
install -m644 "$ROOT/packaging/linux/polkit/com.usbforge.write-media.policy" \
    "$STAGE/usr/share/polkit-1/actions/"
mkdir -p "$STAGE/usr/share/usbforge/ui"
cp -a "$ROOT/ui/." "$STAGE/usr/share/usbforge/ui/"
install -m644 "$ROOT/packaging/linux/usbforge-builder.desktop" "$STAGE/usr/share/applications/"
install -m644 "$ROOT/packaging/linux/usbforge-live.desktop" "$STAGE/usr/share/applications/"
cp -a "$ROOT/packaging/linux/icons/hicolor/." "$STAGE/usr/share/icons/hicolor/"
install -m644 "$ROOT/README.md" "$STAGE/usr/share/doc/usbforge/"
install -m644 "$ROOT/LICENSE" "$STAGE/usr/share/doc/usbforge/copyright"

# Installed-Size in KiB
SIZE="$(du -sk "$STAGE" | awk '{print $1}')"
echo "Installed-Size: $SIZE" >> "$STAGE/DEBIAN/control"

mkdir -p "$OUT_DIR"
fakeroot dpkg-deb --build "$STAGE" "$OUT_DIR/${PKG_NAME}.deb"
dpkg-deb -I "$OUT_DIR/${PKG_NAME}.deb" | head -25
# Validate desktop files inside the package
TMPD="$(mktemp -d)"
dpkg-deb -x "$OUT_DIR/${PKG_NAME}.deb" "$TMPD"
grep -l '\[Desktop Entry\]' "$TMPD"/usr/share/applications/*.desktop
test -f "$TMPD/usr/share/icons/hicolor/48x48/apps/usbforge.png"
test -x "$TMPD/usr/bin/usbforge-update"
rm -rf "$TMPD"
ls -lh "$OUT_DIR/${PKG_NAME}.deb"
echo "[deb] Done."
