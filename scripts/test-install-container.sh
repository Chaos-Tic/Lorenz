#!/usr/bin/env bash
# Runs the real installer in a clean distribution container, then checks the
# installed commands. Usage: scripts/test-install-container.sh IMAGE
set -euo pipefail
image=${1:?Usage: $0 IMAGE}
root=$(cd "$(dirname "$0")/.." && pwd)
docker run --rm -v "$root:/src:ro" "$image" bash -c '
set -euo pipefail
# Arch images ship an empty package database; a user system would already have one.
if command -v pacman >/dev/null; then pacman -Syu --noconfirm >/dev/null; fi
mkdir /work && tar -C /src --exclude=./target --exclude=./build --exclude=./.local --exclude=./.git -cf - . | tar -C /work -xf -
cd /work
./install.sh --yes --no-office --no-kwin --no-autostart
export PATH="$HOME/.local/bin:$PATH"
metroid doctor
magick -size 40x30 xc:red /tmp/sample.png 2>/dev/null || convert -size 40x30 xc:red /tmp/sample.png
metroid menu tools /tmp/sample.png | grep -q "^compress"
metroid convert /tmp/sample.png --to webp
metroid tool rotate-right /tmp/sample.png
metroid tool zip /tmp/sample.png
test -s /tmp/sample.webp && test -s /tmp/sample-rotated.png && test -s /tmp/sample.zip
test -f "$HOME/.local/share/kio/servicemenus/metroid.desktop"
QT_QPA_PLATFORM=offscreen metroid-desktop --smoke-test
metroid autostart enable && test -f "$HOME/.config/autostart/metroid.desktop"
./uninstall.sh && test ! -e "$HOME/.local/bin/metroid"
echo "INSTALLER OK: '"$image"'"
'
