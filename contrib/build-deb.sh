#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="put-in-pipe"
PKG_VERSION="${1:-0.1.0}"
PKG_ARCH="$(dpkg --print-architecture)"
PKG_MAINTAINER="Roman Lyubimov"
PKG_DESCRIPTION="Privacy-preserving streaming file transfer server"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
STAGE_DIR="$(mktemp -d)"

trap 'rm -rf "$STAGE_DIR"' EXIT

# --- Build -----------------------------------------------------------

echo "==> Building $PKG_NAME $PKG_VERSION ($PKG_ARCH) ..."

cmake -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTS=OFF \
      "$PROJECT_DIR/src"

cmake --build "$BUILD_DIR" -j"$(nproc)"

# --- Stage -----------------------------------------------------------

echo "==> Staging package tree ..."

install -Dm755 "$BUILD_DIR/$PKG_NAME"              "$STAGE_DIR/usr/bin/$PKG_NAME"
install -Dm644 "$SCRIPT_DIR/config.ini.example"     "$STAGE_DIR/etc/$PKG_NAME/config.ini"
install -Dm644 "$SCRIPT_DIR/$PKG_NAME.service"      "$STAGE_DIR/usr/lib/systemd/system/$PKG_NAME.service"

# --- DEBIAN control --------------------------------------------------

mkdir -p "$STAGE_DIR/DEBIAN"

cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Architecture: $PKG_ARCH
Maintainer: $PKG_MAINTAINER
Description: $PKG_DESCRIPTION
 The server acts as an intermediate buffer between a sender and multiple
 receivers. Files are transferred in chunks without the server storing the
 complete file on disk. Clients never connect directly to each other,
 preserving IP-address privacy.
Priority: optional
Section: net
EOF

# conffiles — dpkg won't overwrite user-edited config on upgrade
cat > "$STAGE_DIR/DEBIAN/conffiles" <<EOF
/etc/$PKG_NAME/config.ini
EOF

# postinst — create system user
cat > "$STAGE_DIR/DEBIAN/postinst" <<'POSTINST'
#!/bin/sh
set -e

if ! getent passwd put-in-pipe >/dev/null 2>&1; then
    adduser --system --group --no-create-home --home /nonexistent put-in-pipe
fi

if [ -d /run/systemd/system ]; then
    systemctl daemon-reload
fi
POSTINST
chmod 755 "$STAGE_DIR/DEBIAN/postinst"

# postrm — clean up on purge
cat > "$STAGE_DIR/DEBIAN/postrm" <<'POSTRM'
#!/bin/sh
set -e

if [ "$1" = "purge" ]; then
    if getent passwd put-in-pipe >/dev/null 2>&1; then
        deluser --system put-in-pipe || true
    fi
    rm -rf /etc/put-in-pipe
fi

if [ -d /run/systemd/system ]; then
    systemctl daemon-reload
fi
POSTRM
chmod 755 "$STAGE_DIR/DEBIAN/postrm"

# --- Build .deb ------------------------------------------------------

DEB_FILE="$PROJECT_DIR/${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"

echo "==> Packaging $DEB_FILE ..."
dpkg-deb --root-owner-group --build "$STAGE_DIR" "$DEB_FILE"

echo "==> Done: $DEB_FILE"
echo "   Install:  sudo dpkg -i $DEB_FILE"
echo "   Enable:   sudo systemctl enable --now $PKG_NAME"
