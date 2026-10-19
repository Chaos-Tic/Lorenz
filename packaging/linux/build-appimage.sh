#!/usr/bin/env bash
# Builds Metroid-x86_64.AppImage with Qt bundled, from the repository root.
# Needs Qt 6 in QT_ROOT_DIR (install-qt-action sets it) and the oldest system
# the AppImage should run on: it runs on newer distributions, never on older.
# Engines are not bundled; the AppImage uses those installed on the system.
set -euo pipefail
: "${QT_ROOT_DIR:?set QT_ROOT_DIR to the Qt 6 installation}"
tools=$(mktemp -d)
fetch() { # name url sha256
    curl -sSfL -o "$tools/$1" "$2"
    echo "$3  $tools/$1" | sha256sum -c -
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH="$QT_ROOT_DIR"
cmake --build build --parallel "$(nproc)"

appdir=build/AppDir
rm -rf "$appdir"
install -Dm755 target/release/metroid "$appdir/usr/bin/metroid"
# The AppImage path changes at every mount, so it ships no file-manager menus.
cat >build/metroid.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Metroid
GenericName=File converter
Comment=Convert files locally
Exec=metroid-desktop %F
Icon=metroid
Terminal=false
Categories=Utility;
EOF
mkdir -p dist
export PATH="$QT_ROOT_DIR/bin:$tools:$PATH" QMAKE="$QT_ROOT_DIR/bin/qmake"
# xcb runs under X11 and XWayland; offscreen serves the CI smoke test.
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so OUTPUT=dist/Metroid-x86_64.AppImage
APPIMAGE_EXTRACT_AND_RUN=1 linuxdeploy-x86_64.AppImage --appdir "$appdir" \
    --executable build/metroid-desktop --desktop-file build/metroid.desktop \
    --icon-file assets/metroid.svg --plugin qt --output appimage
ls -l dist
