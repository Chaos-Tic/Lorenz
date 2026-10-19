#!/usr/bin/env bash
# Builds signed Ubuntu source packages of a release and uploads them to the
# Launchpad PPA, which compiles them for each series. From the repository root:
#   packaging/debian/ppa.sh                      # build and upload, default series
#   packaging/debian/ppa.sh --no-upload noble    # build one series, keep the files
# Ubuntu 22.04 is left out: its Qt is older than 6.4. Needs docker, and in gpg
# the key registered on the Launchpad account; its agent signs from inside the
# container. Rebuild a published version with PPA_REVISION=2. Uploads go to
# ppa:$PPA (default chaos-tic/lorenz).
set -euo pipefail
upload=1
if [ "${1:-}" = --no-upload ]; then upload=0; shift; fi
series=("$@")
[ ${#series[@]} -gt 0 ] || series=(noble resolute stonking)
version=$(sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -n1)
ppa=${PPA:-chaos-tic/lorenz}
revision=${PPA_REVISION:-1}
url=https://github.com/Chaos-Tic/Lorenz
key=${DEBSIGN_KEYID:-$(gpg --list-secret-keys --with-colons | awk -F: '$1 == "fpr" { print $10; exit }')}
[ -n "$key" ] || { echo 'No gpg secret key: create one and add it to Launchpad.' >&2; exit 1; }

out=dist/ppa
mkdir -p "$out"
# The release tarball is the upstream source; the vendor archive is its
# second component, unpacked in vendor/.
[ -f "$out/lorenz_$version.orig.tar.gz" ] ||
    curl -fsSL -o "$out/lorenz_$version.orig.tar.gz" "$url/archive/refs/tags/v$version.tar.gz"
if [ ! -f "$out/lorenz_$version.orig-vendor.tar.xz" ]; then
    [ -f "dist/lorenz-$version-vendor.tar.xz" ] ||
        curl -fsSL -o "dist/lorenz-$version-vendor.tar.xz" "$url/releases/download/v$version/lorenz-$version-vendor.tar.xz"
    cp "dist/lorenz-$version-vendor.tar.xz" "$out/lorenz_$version.orig-vendor.tar.xz"
fi

gpg --export --armor "$key" >"$out/signing-key.asc"
agent=$(gpgconf --list-dirs agent-extra-socket)
first=1
for s in "${series[@]}"; do
    case $s in
    noble) number=24.04 rust='cargo-1.85, rustc-1.85' ;;
    resolute) number=26.04 rust='cargo (>= 1.85), rustc (>= 1.85)' ;;
    stonking) number=26.10 rust='cargo (>= 1.85), rustc (>= 1.85)' ;;
    # The number orders versions across upgrades: add each new series here.
    *) echo "Unknown series $s: add it with its version number." >&2; exit 2 ;;
    esac
    # Launchpad keeps the first upload of the orig tarballs; later series refer to them.
    if [ $first = 1 ]; then sources=-sa; else sources=-sd; fi
    first=0
    docker run --rm -i -v "$PWD:/src:ro" -v "$PWD/$out:/out" -v "$agent:/root/.gnupg/S.gpg-agent" \
        -e "S=$s" -e "N=$number" -e "RUST=$rust" -e "V=$version" -e "R=$revision" -e "KEY=$key" -e "SRC=$sources" -e "OWNER=$(id -u):$(id -g)" \
        ubuntu:24.04 bash -euo pipefail <<'EOF'
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends dpkg-dev debhelper devscripts gnupg >/dev/null
chmod 700 /root/.gnupg
gpg --batch --quiet --import /out/signing-key.asc
work=$(mktemp -d)
cp /out/lorenz_$V.orig*.tar.* "$work/"
cd "$work"
mkdir lorenz-$V
tar -xzf lorenz_$V.orig.tar.gz -C lorenz-$V --strip-components=1
mkdir lorenz-$V/vendor
tar -xJf lorenz_$V.orig-vendor.tar.xz -C lorenz-$V/vendor --strip-components=1
cp -r /src/packaging/debian lorenz-$V/debian
rm lorenz-$V/debian/ppa.sh
sed -i "s/@RUST@/$RUST/" lorenz-$V/debian/control
cat >lorenz-$V/debian/changelog <<CHANGES
lorenz ($V-1~ppa$R~ubuntu$N) $S; urgency=medium

  * Lorenz $V, built from source for Ubuntu $N.

 -- Chaos-Tic <58691435+Chaos-Tic@users.noreply.github.com>  $(date -R)
CHANGES
cd lorenz-$V
dpkg-buildpackage -S -d $SRC -k"$KEY" >/dev/null
cd ..
cp lorenz_$V-1~ppa$R~ubuntu$N* /out/
chown "$OWNER" /out/*
echo "built $(ls lorenz_*_source.changes)"
EOF
    if [ $upload = 1 ]; then
        docker run --rm -v "$PWD/$out:/out" ubuntu:24.04 bash -c \
            "apt-get update -qq && apt-get install -y -qq dput gnupg >/dev/null && gpg --batch --quiet --import /out/signing-key.asc && dput ppa:$ppa /out/lorenz_$version-1~ppa${revision}~ubuntu${number}_source.changes"
    fi
done
ls -l "$out"
