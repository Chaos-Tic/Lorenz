#!/bin/sh
# Installs the built executables and system desktop entries under DESTDIR/usr,
# for packages. Run from the repository root after building:
#   cargo build --release --locked && cmake --build build
#   sh packaging/linux/stage.sh DESTDIR
set -eu
dest=${1:?usage: packaging/linux/stage.sh DESTDIR}
install -Dm755 target/release/lorenz "$dest/usr/bin/lorenz"
install -Dm755 build/lorenz-desktop "$dest/usr/bin/lorenz-desktop"
strip "$dest/usr/bin/lorenz" "$dest/usr/bin/lorenz-desktop" 2>/dev/null || true
# Launcher, Dolphin and Nemo menus, icon; the same entries `lorenz integrate`
# writes for one user, pointing at the system executable.
"$dest/usr/bin/lorenz" integrate --data-dir "$dest/usr/share" --exe /usr/bin/lorenz-desktop >/dev/null
install -Dm644 LICENSE "$dest/usr/share/licenses/lorenz/LICENSE"
