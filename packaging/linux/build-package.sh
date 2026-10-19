#!/usr/bin/env bash
# Builds a Linux package inside a container of the target distribution, from
# the repository root:
#   packaging/linux/build-package.sh deb    # in debian:12 (runs on Debian 12+, Ubuntu 24.04+)
#   packaging/linux/build-package.sh rpm    # in fedora:latest
#   packaging/linux/build-package.sh arch   # in archlinux:latest
# The package lands in dist/. Engines are recommended, never bundled.
set -euo pipefail
format=${1:?usage: packaging/linux/build-package.sh deb|rpm|arch}
version=$(sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -n1)
homepage=https://github.com/Chaos-Tic/Metroid
maintainer='Chaos-Tic <58691435+Chaos-Tic@users.noreply.github.com>'
summary='Local file conversion with radial wheels'
mkdir -p dist

build() {
    cargo build --release --locked
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
    cmake --build build --parallel "$(nproc)"
}

case $format in
deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y --no-install-recommends build-essential cmake qt6-base-dev dpkg-dev curl ca-certificates
    # Debian 12's Rust is older than the 1.85 that edition 2024 needs.
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
    . "$HOME/.cargo/env"
    build
    stage=$(mktemp -d)/metroid
    sh packaging/linux/stage.sh "$stage"
    install -d "$stage/usr/share/doc/metroid"
    cat >"$stage/usr/share/doc/metroid/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Metroid
Source: $homepage

Files: *
Copyright: Chaos-Tic
License: GPL-3.0+
 On Debian systems, the full text is in /usr/share/common-licenses/GPL-3.
EOF
    arch=$(dpkg --print-architecture)
    # dpkg-shlibdeps maps the linked libraries to package names and versions;
    # it only needs a control file to exist.
    work=$(mktemp -d); mkdir "$work/debian"
    printf 'Source: metroid\n\nPackage: metroid\nArchitecture: any\n' >"$work/debian/control"
    depends=$(cd "$work" && dpkg-shlibdeps -O "$stage"/usr/bin/* | sed 's/^shlibs:Depends=//')
    size=$(du -sk "$stage" | cut -f1)
    mkdir "$stage/DEBIAN"
    cat >"$stage/DEBIAN/control" <<EOF
Package: metroid
Version: $version
Architecture: $arch
Maintainer: $maintainer
Installed-Size: $size
Depends: $depends, qt6-qpa-plugins
Recommends: ffmpeg, imagemagick, pandoc, poppler-utils, qpdf, ghostscript, libarchive-tools, libnotify-bin, qt6-wayland
Suggests: libreoffice-writer-nogui, libreoffice-calc-nogui, libreoffice-impress-nogui, wl-clipboard | xclip
Section: utils
Priority: optional
Homepage: $homepage
Description: $summary
 Converts images, video, audio, documents and PDFs on this computer with
 FFmpeg, ImageMagick, Pandoc, Poppler, qpdf and LibreOffice; originals are
 never modified. Right-click files in Dolphin or Nemo, or open Metroid from
 the applications menu. Missing engines only hide the formats that need them.
EOF
    dpkg-deb --root-owner-group --build "$stage" "dist/metroid_${version}_${arch}.deb"
    ;;
rpm)
    dnf install -y gcc-c++ cmake qt6-qtbase-devel cargo rust rpm-build
    build
    stage=$(mktemp -d)/metroid
    sh packaging/linux/stage.sh "$stage"
    top=$(mktemp -d)
    # Library requirements are generated from the executables by rpmbuild.
    cat >"$top/metroid.spec" <<EOF
%global debug_package %{nil}
Name: metroid
Version: $version
Release: 1%{?dist}
Summary: $summary
License: GPL-3.0-or-later
URL: $homepage
Requires: qt6-qtbase-gui
Recommends: ffmpeg-free ImageMagick pandoc poppler-utils qpdf ghostscript bsdtar libnotify
Suggests: libreoffice-writer libreoffice-calc libreoffice-impress wl-clipboard xclip

%description
Converts images, video, audio, documents and PDFs on this computer with
FFmpeg, ImageMagick, Pandoc, Poppler, qpdf and LibreOffice; originals are
never modified. Right-click files in Dolphin or Nemo, or open Metroid from
the applications menu. Missing engines only hide the formats that need them.

%install
cp -a $stage/. %{buildroot}/

%files
/usr/bin/metroid
/usr/bin/metroid-desktop
/usr/share/applications/metroid.desktop
/usr/share/icons/hicolor/scalable/apps/metroid.svg
/usr/share/kio/servicemenus/metroid.desktop
/usr/share/nemo/actions/metroid-*.nemo_action
%license /usr/share/licenses/metroid/LICENSE
EOF
    rpmbuild -bb "$top/metroid.spec" --define "_topdir $top" --define "_rpmdir $PWD/dist" \
        --define '_build_name_fmt %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm'
    ;;
arch)
    pacman -Syu --noconfirm --needed base-devel cmake rust qt6-base
    # makepkg refuses to run as root, and builds from the source tarball the
    # PKGBUILD names; a local one with that name is used instead of downloading.
    id builder >/dev/null 2>&1 || useradd -m builder
    work=$(mktemp -d)
    sed "s/^pkgver=.*/pkgver=$version/" packaging/arch/PKGBUILD >"$work/PKGBUILD"
    tar --exclude=./.git --exclude=./target --exclude=./build --exclude=./dist \
        --transform "s,^\.,Metroid-$version," -czf "$work/metroid-$version.tar.gz" .
    chown -R builder "$work"
    (cd "$work" && runuser -u builder -- makepkg --force --noconfirm)
    cp "$work"/metroid-"$version"-*.pkg.tar.zst dist/
    cp "$work/PKGBUILD" dist/PKGBUILD
    ;;
*)
    echo "Unknown format: $format (deb, rpm or arch)" >&2
    exit 2
    ;;
esac
ls -l dist
