#!/usr/bin/env bash
# Installs a built package in a clean container, checks it, then removes it:
#   packaging/linux/test-package.sh dist/lorenz_1.0.0_amd64.deb
set -euo pipefail
package=$(realpath "${1:?usage: packaging/linux/test-package.sh PACKAGE}")
case $package in
*.deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y --no-install-recommends "$package"
    remove() { apt-get remove -y lorenz; } ;;
*.rpm)
    dnf install -y --setopt=install_weak_deps=False "$package"
    remove() { dnf remove -y lorenz; } ;;
*.pkg.tar.zst)
    pacman -Sy --noconfirm
    pacman -U --noconfirm "$package"
    remove() { pacman -R --noconfirm lorenz; } ;;
*) echo "Unknown package type: $package" >&2; exit 2 ;;
esac

lorenz --version
lorenz doctor
for file in /usr/share/applications/lorenz.desktop /usr/share/icons/hicolor/scalable/apps/lorenz.svg \
    /usr/share/lorenz/scripts/install-kwin.sh /usr/share/lorenz/integrations/kwin/plugin.cpp \
    /usr/share/kio/servicemenus/lorenz.desktop /usr/share/nemo/actions/lorenz-convert.nemo_action; do
    test -f "$file" || { echo "missing $file" >&2; exit 1; }
done
grep -q '^Exec="/usr/bin/lorenz-desktop"' /usr/share/applications/lorenz.desktop
# The Qt libraries and platform plugin the package depends on are enough to start.
QT_QPA_PLATFORM=offscreen lorenz-desktop --smoke-test
remove
for file in /usr/bin/lorenz /usr/share/applications/lorenz.desktop; do
    test ! -e "$file" || { echo "removal left $file" >&2; exit 1; }
done
echo "OK: $(basename "$package")"
