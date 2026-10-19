#!/usr/bin/env bash
# Builds signed Ubuntu source packages of a release and uploads them to the
# Launchpad PPA, which compiles them for each series. Runs in Ubuntu (the CI
# release job uses ubuntu:24.04), from the repository root:
#   packaging/debian/ppa.sh VERSION [SERIES...]
# The signing key is $PPA_SIGNING_KEY (ASCII-armored secret key) or, when it
# is empty, the first secret key in gpg. Set PPA_UPLOAD=0 to build only, and
# PPA_REVISION=2 to rebuild a version already published. Uploads go to
# ppa:$PPA (default chaos-tic/lorenz). Ubuntu 22.04 is left out: its Qt is
# older than 6.4.
set -euo pipefail
version=${1:?usage: packaging/debian/ppa.sh VERSION [SERIES...]}
shift
series=("$@")
[ ${#series[@]} -gt 0 ] || series=(noble resolute stonking)
ppa=${PPA:-chaos-tic/lorenz}
revision=${PPA_REVISION:-1}
url=https://github.com/Chaos-Tic/Lorenz
maintainer='Chaos-Tic <58691435+Chaos-Tic@users.noreply.github.com>'

export DEBIAN_FRONTEND=noninteractive
command -v dpkg-buildpackage >/dev/null && command -v dput >/dev/null || {
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends dpkg-dev debhelper devscripts dput gnupg curl ca-certificates >/dev/null
}
if [ -n "${PPA_SIGNING_KEY:-}" ]; then
    export GNUPGHOME=$(mktemp -d)
    gpg --batch --quiet --import <<<"$PPA_SIGNING_KEY"
fi
key=$(gpg --list-secret-keys --with-colons | awk -F: '$1 == "fpr" { print $10; exit }')
[ -n "$key" ] || { echo 'No signing key: set PPA_SIGNING_KEY.' >&2; exit 1; }

# Older release tarballs lack packaging/debian: the checkout's is used then.
here_debian=$PWD/packaging/debian
out=$PWD/dist/ppa
mkdir -p "$out"
cd "$out"
# The release tarball is the upstream source; the vendor archive is its
# second component, unpacked in vendor/.
[ -f "lorenz_$version.orig.tar.gz" ] ||
    curl -fsSL -o "lorenz_$version.orig.tar.gz" "$url/archive/refs/tags/v$version.tar.gz"
[ -f "lorenz_$version.orig-vendor.tar.xz" ] ||
    curl -fsSL -o "lorenz_$version.orig-vendor.tar.xz" "$url/releases/download/v$version/lorenz-$version-vendor.tar.xz"

first=1
for s in "${series[@]}"; do
    case $s in
    noble) number=24.04 rust='cargo-1.85, rustc-1.85' ;;
    resolute) number=26.04 rust='cargo (>= 1.85), rustc (>= 1.85)' ;;
    stonking) number=26.10 rust='cargo (>= 1.85), rustc (>= 1.85)' ;;
    # The number orders versions across upgrades: add each new series here.
    *) echo "Unknown series $s: add it with its version number." >&2; exit 2 ;;
    esac
    full=$version-1~ppa$revision~ubuntu$number
    work=$(mktemp -d)
    mkdir "$work/lorenz-$version" "$work/lorenz-$version/vendor"
    tar -xzf "lorenz_$version.orig.tar.gz" -C "$work/lorenz-$version" --strip-components=1
    tar -xJf "lorenz_$version.orig-vendor.tar.xz" -C "$work/lorenz-$version/vendor" --strip-components=1
    debian=$work/lorenz-$version/packaging/debian
    [ -d "$debian" ] || debian=$here_debian
    rm -rf "$work/lorenz-$version/debian"
    cp -r "$debian" "$work/lorenz-$version/debian"
    rm "$work/lorenz-$version/debian/ppa.sh"
    sed -i "s/@RUST@/$rust/" "$work/lorenz-$version/debian/control"
    cat >"$work/lorenz-$version/debian/changelog" <<EOF
lorenz ($full) $s; urgency=medium

  * Lorenz $version, built from source for Ubuntu $number.

 -- $maintainer  $(date -R)
EOF
    # Launchpad keeps the first upload of the orig tarballs; later series refer to them.
    if [ $first = 1 ]; then sources=-sa; else sources=-sd; fi
    first=0
    ln -sf "$out"/lorenz_$version.orig*.tar.* "$work/"
    (cd "$work/lorenz-$version" && dpkg-buildpackage -S -d $sources -k"$key" >/dev/null)
    cp "$work"/lorenz_${full}* "$out/"
    rm -rf "$work"
    echo "built lorenz_${full}_source.changes"
    if [ "${PPA_UPLOAD:-1}" = 1 ]; then
        dput "ppa:$ppa" "lorenz_${full}_source.changes"
    fi
done
