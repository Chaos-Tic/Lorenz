#!/usr/bin/env bash
# Installs a built package in a clean container, checks it, then removes it:
#   packaging/linux/test-package.sh dist/metroid_0.1.0_amd64.deb
set -euo pipefail
package=$(realpath "${1:?usage: packaging/linux/test-package.sh PACKAGE}")
case $package in
*.deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y --no-install-recommends "$package"
    remove() { apt-get remove -y metroid; } ;;
*.rpm)
    dnf install -y --setopt=install_weak_deps=False "$package"
    remove() { dnf remove -y metroid; } ;;
*.pkg.tar.zst)
    pacman -Sy --noconfirm
    pacman -U --noconfirm "$package"
    remove() { pacman -R --noconfirm metroid; } ;;
*) echo "Unknown package type: $package" >&2; exit 2 ;;
esac

metroid --version
metroid doctor
for file in /usr/share/applications/metroid.desktop /usr/share/icons/hicolor/scalable/apps/metroid.svg \
    /usr/share/kio/servicemenus/metroid.desktop /usr/share/nemo/actions/metroid-convert.nemo_action; do
    test -f "$file" || { echo "missing $file" >&2; exit 1; }
done
grep -q '^Exec="/usr/bin/metroid-desktop"' /usr/share/applications/metroid.desktop
# The Qt libraries and platform plugin the package depends on are enough to start.
QT_QPA_PLATFORM=offscreen metroid-desktop --smoke-test
remove
for file in /usr/bin/metroid /usr/share/applications/metroid.desktop; do
    test ! -e "$file" || { echo "removal left $file" >&2; exit 1; }
done
echo "OK: $(basename "$package")"
