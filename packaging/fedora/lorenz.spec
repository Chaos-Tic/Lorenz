# Builds Lorenz from the release tarball. With vendor (the default, for COPR)
# the crates come from the vendor archive attached to the release; built
# --without vendor, they come from Fedora's rust-* packages, as Fedora requires.
%bcond vendor 1

Name:           lorenz
Version:        1.1.0
Release:        1%{?dist}
Summary:        Local file conversion with radial wheels

SourceLicense:  GPL-3.0-or-later
# Lorenz is GPL-3.0-or-later; the rest covers the crates linked into lorenz.
License:        GPL-3.0-or-later AND MIT AND (MIT OR Apache-2.0) AND Unicode-3.0 AND (Unlicense OR MIT) AND (Apache-2.0 WITH LLVM-exception OR Apache-2.0 OR MIT)
URL:            https://github.com/Chaos-Tic/Lorenz
Source0:        %{url}/archive/v%{version}/Lorenz-%{version}.tar.gz
%if %{with vendor}
# packaging/linux/vendor.sh
Source1:        %{url}/releases/download/v%{version}/lorenz-%{version}-vendor.tar.xz
%endif

BuildRequires:  cargo-rpm-macros >= 25
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  desktop-file-utils

Requires:       qt6-qtbase-gui
Recommends:     ffmpeg-free
Recommends:     ImageMagick
Recommends:     pandoc
Recommends:     poppler-utils
Recommends:     qpdf
Recommends:     ghostscript
Recommends:     bsdtar
Recommends:     libnotify
Suggests:       libreoffice-writer
Suggests:       libreoffice-calc
Suggests:       libreoffice-impress
Suggests:       wl-clipboard
Suggests:       xclip

%description
Converts images, video, audio, documents and PDFs on this computer with
FFmpeg, ImageMagick, Pandoc, Poppler, qpdf and LibreOffice; originals are
never modified. Right-click files in Dolphin or Nemo, or open Lorenz from
the applications menu. Missing engines only hide the formats that need them.

%prep
%if %{with vendor}
%autosetup -n Lorenz-%{version} -p1 -a1
%cargo_prep -v vendor
%else
%autosetup -n Lorenz-%{version} -p1
%cargo_prep
%endif

%if %{without vendor}
%generate_buildrequires
%cargo_generate_buildrequires
%endif

%build
%cargo_build
%{cargo_license_summary}
%{cargo_license} > LICENSE.dependencies
%if %{with vendor}
%cargo_vendor_manifest
%endif
%cmake -DBUILD_TESTING=OFF -DLORENZ_CLI_BINARY="$PWD/target/rpm/lorenz"
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/lorenz.desktop
%cargo_test

%files
%license %{_datadir}/licenses/lorenz/LICENSE LICENSE.dependencies
%if %{with vendor}
%license cargo-vendor.txt
%endif
%doc README.md
%{_bindir}/lorenz
%{_bindir}/lorenz-desktop
%{_datadir}/applications/lorenz.desktop
%{_datadir}/icons/hicolor/scalable/apps/lorenz.svg
# Dolphin and Nemo read these; neither is required.
%dir %{_datadir}/kio
%dir %{_datadir}/kio/servicemenus
%{_datadir}/kio/servicemenus/lorenz.desktop
%dir %{_datadir}/nemo
%dir %{_datadir}/nemo/actions
%{_datadir}/nemo/actions/lorenz-*.nemo_action

%changelog
* Mon Oct 19 2026 Chaos-Tic <58691435+Chaos-Tic@users.noreply.github.com> - 1.1.0-1
- Stop the engine's own processes on cancel and timeout

* Mon Oct 19 2026 Chaos-Tic <58691435+Chaos-Tic@users.noreply.github.com> - 1.0.0-1
- First package built from source
