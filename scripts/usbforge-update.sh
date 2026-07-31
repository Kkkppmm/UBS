#!/usr/bin/env bash
# usbforge-update — check GitHub Releases and install the latest package
set -euo pipefail

API="https://api.github.com/repos/Kkkppmm/UBS/releases/latest"
UA="USBForge-Updater/${USBFORGE_VERSION:-1.1.1}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fetch() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -A "$UA" "$@"
    else
        wget -qO- --user-agent="$UA" "$@"
    fi
}

download() {
    local url="$1" out="$2"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -A "$UA" -o "$out" "$url"
    else
        wget -q -O "$out" --user-agent="$UA" "$url"
    fi
}

echo "USBForge updater"
echo "Checking $API ..."

JSON="$(fetch "$API")"
TAG="$(printf '%s' "$JSON" | sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n1)"
if [[ -z "$TAG" ]]; then
    echo "Could not determine latest release tag."
    echo "Open: https://github.com/Kkkppmm/UBS/releases"
    exit 1
fi

VER="${TAG#v}"
echo "Latest release: $TAG"

CURRENT=""
if command -v usbforge-builder >/dev/null 2>&1; then
    CURRENT="$(usbforge-live --smoke 2>/dev/null | sed -n 's/USBForge Live \([0-9.]*\).*/\1/p' | head -n1 || true)"
fi
[[ -z "$CURRENT" ]] && CURRENT="unknown"
echo "Installed:      $CURRENT"

pick_asset() {
    local needle="$1"
    printf '%s' "$JSON" | tr '{' '\n' | grep -F '"browser_download_url"' | grep -F "$needle" | \
        sed -n 's/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n1
}

URL=""
KIND=""
if command -v dpkg >/dev/null 2>&1 && [[ -f /var/lib/dpkg/status ]]; then
    URL="$(pick_asset '_amd64.deb')"
    KIND="deb"
elif command -v rpm >/dev/null 2>&1 && { [[ -d /var/lib/rpm ]] || command -v rpmbuild >/dev/null 2>&1; }; then
    URL="$(pick_asset '.x86_64.rpm')"
    KIND="rpm"
else
    URL="$(pick_asset 'linux-x86_64.tar.gz')"
    KIND="tar"
fi

if [[ -z "$URL" ]]; then
    echo "No matching package asset found for this system."
    echo "Browse releases: https://github.com/Kkkppmm/UBS/releases/tag/$TAG"
    exit 1
fi

FILE="$TMP/$(basename "$URL")"
echo "Downloading: $URL"
download "$URL" "$FILE"

echo "Installing ($KIND) — may ask for your password..."
case "$KIND" in
    deb)
        if command -v pkexec >/dev/null 2>&1; then
            pkexec dpkg -i "$FILE" || pkexec bash -c "dpkg -i '$FILE' || apt-get install -f -y"
        else
            sudo dpkg -i "$FILE" || sudo apt-get install -f -y
        fi
        ;;
    rpm)
        if command -v pkexec >/dev/null 2>&1; then
            pkexec rpm -Uvh "$FILE"
        else
            sudo rpm -Uvh "$FILE"
        fi
        ;;
    tar)
        DIR="$TMP/extract"
        mkdir -p "$DIR"
        tar -xzf "$FILE" -C "$DIR"
        INNER="$(find "$DIR" -maxdepth 1 -type d -name 'usbforge-*' | head -n1)"
        if [[ -x "$INNER/install.sh" ]]; then
            if command -v pkexec >/dev/null 2>&1; then
                pkexec env PREFIX=/usr/local bash "$INNER/install.sh"
            else
                sudo env PREFIX=/usr/local bash "$INNER/install.sh"
            fi
        else
            echo "Tarball missing install.sh"
            exit 1
        fi
        ;;
esac

# Refresh menus after upgrade
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
fi

echo "Done. Look for \"USBForge Builder\" and \"USBForge Live\" in your app menu."
echo "Or run: usbforge-builder"
