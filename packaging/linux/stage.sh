#!/bin/sh
# Installs the built executables and system desktop entries under DESTDIR/usr,
# for packages. Run from the repository root after building:
#   cargo build --release --locked && cmake --build build
#   sh packaging/linux/stage.sh DESTDIR
set -eu
dest=${1:?usage: packaging/linux/stage.sh DESTDIR}
install -Dm755 target/release/metroid "$dest/usr/bin/metroid"
install -Dm755 build/metroid-desktop "$dest/usr/bin/metroid-desktop"
strip "$dest/usr/bin/metroid" "$dest/usr/bin/metroid-desktop" 2>/dev/null || true
# Launcher, Dolphin and Nemo menus, icon; the same entries `metroid integrate`
# writes for one user, pointing at the system executable.
"$dest/usr/bin/metroid" integrate --data-dir "$dest/usr/share" --exe /usr/bin/metroid-desktop >/dev/null
