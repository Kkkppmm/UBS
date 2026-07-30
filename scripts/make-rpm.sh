#!/usr/bin/env bash
# Build Fedora/RHEL-style binary .rpm
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(grep '#define USBFORGE_VERSION' "$ROOT/common/usbforge.h" | awk '{print $3}' | tr -d '"')"
OUT_DIR="$ROOT/build/release"
TOP="$ROOT/build/rpmbuild"
PAYLOAD="$TOP/PAYLOAD"

echo "[rpm] Building usbforge-$VERSION rpm"

make -C "$ROOT" all

rm -rf "$TOP"
mkdir -p "$TOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
mkdir -p "$PAYLOAD/usr/bin"
mkdir -p "$PAYLOAD/usr/share/usbforge/docs"
mkdir -p "$PAYLOAD/usr/share/usbforge/scripts"
mkdir -p "$PAYLOAD/usr/share/usbforge/ui"
mkdir -p "$PAYLOAD/usr/share/applications"
mkdir -p "$PAYLOAD/usr/share/doc/usbforge"
mkdir -p "$PAYLOAD/usr/share/licenses/usbforge"
mkdir -p "$PAYLOAD/usr/share/icons/hicolor"

install -m755 "$ROOT/build/usbforge-builder" "$PAYLOAD/usr/bin/"
install -m755 "$ROOT/build/usbforge-live" "$PAYLOAD/usr/bin/"
install -m755 "$ROOT/scripts/usbforge-update.sh" "$PAYLOAD/usr/bin/usbforge-update"
install -m644 "$ROOT/docs/"* "$PAYLOAD/usr/share/usbforge/docs/"
install -m755 "$ROOT/scripts/build-iso.sh" "$PAYLOAD/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/write-media.sh" "$PAYLOAD/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/live-autostart.sh" "$PAYLOAD/usr/share/usbforge/scripts/"
install -m755 "$ROOT/scripts/usbforge-update.sh" "$PAYLOAD/usr/share/usbforge/scripts/"
cp -a "$ROOT/ui/." "$PAYLOAD/usr/share/usbforge/ui/"
install -m644 "$ROOT/packaging/linux/usbforge-builder.desktop" "$PAYLOAD/usr/share/applications/"
install -m644 "$ROOT/packaging/linux/usbforge-live.desktop" "$PAYLOAD/usr/share/applications/"
cp -a "$ROOT/packaging/linux/icons/hicolor/." "$PAYLOAD/usr/share/icons/hicolor/"
install -m644 "$ROOT/README.md" "$PAYLOAD/usr/share/doc/usbforge/"
install -m644 "$ROOT/LICENSE" "$PAYLOAD/usr/share/licenses/usbforge/"

SPEC="$TOP/SPECS/usbforge-bin.spec"
cat > "$SPEC" <<EOF
Name:           usbforge
Version:        ${VERSION}
Release:        1
Summary:        Bootable ISO builder and Live USB Lab
License:        MIT
URL:            https://github.com/Kkkppmm/UBS
AutoReqProv:    no
BuildArch:      x86_64

%description
USBForge creates GRUB-bootable ISOs, writes them to USB, and provides a
Live USB Lab GUI for device testing, diagnostics, and documentation.

%prep
# binary payload package - nothing to unpack

%build
# prebuilt

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a ${PAYLOAD}/. %{buildroot}/

%post
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || :
fi

%postun
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || :
fi

%files
/usr/bin/usbforge-builder
/usr/bin/usbforge-live
/usr/bin/usbforge-update
/usr/share/usbforge
/usr/share/applications/usbforge-builder.desktop
/usr/share/applications/usbforge-live.desktop
/usr/share/icons/hicolor
/usr/share/doc/usbforge
/usr/share/licenses/usbforge
EOF

mkdir -p "$OUT_DIR"
rpmbuild -bb "$SPEC" \
    --define "_topdir $TOP" \
    --define "_rpmdir $OUT_DIR" \
    --define "_build_id_links none" \
    2>&1 | tail -40

# Flatten arch subdirectory if present
find "$OUT_DIR" -type f -name 'usbforge-*.rpm' | while read -r f; do
    base="$(basename "$f")"
    dir="$(dirname "$f")"
    if [[ "$dir" != "$OUT_DIR" ]]; then
        mv -f "$f" "$OUT_DIR/$base"
        rmdir "$dir" 2>/dev/null || true
    fi
    ls -lh "$OUT_DIR/$base"
done

test -n "$(find "$OUT_DIR" -maxdepth 1 -name 'usbforge-*.rpm' -print -quit)" \
    || { echo "[rpm] FAILED: no rpm produced"; find "$TOP" -name '*.rpm' -ls; exit 1; }

echo "[rpm] Done."
