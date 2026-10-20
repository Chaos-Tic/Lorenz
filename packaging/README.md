# Distribution

Version 1.0 ships per-user installers on Windows and system packages on Linux,
all built and installed in clean environments by CI, and attached to the
GitHub release when a `v*` tag is pushed. None is signed yet; Arch users can
install from the AUR (`yay -S lorenz`).

- Windows: `windows/lorenz.iss` (Inno Setup 6) builds `Lorenz-Setup-x64.exe`
  from the executables and the Qt files deployed by windeployqt. It installs for
  the current user without administrator rights, adds Start, Send to and an
  optional desktop shortcut, can install engines through winget and enable start
  at sign-in, and registers an uninstaller in Settings → Apps.
  `scripts/install-windows.ps1` builds and installs from a checkout.
- Linux: `linux/build-package.sh deb|rpm|arch` builds a package in a container of
  the target family (`debian:12`, `fedora:latest`, `archlinux:latest`);
  `linux/test-package.sh` installs, checks and removes it. The standard
  `cmake --install` rule lays out the files every format shares (`/usr/bin`,
  launcher, icon, Dolphin and Nemo menus). `linux/stage.sh` remains a wrapper
  for older packaging commands. `arch/PKGBUILD` is
  published on the AUR as `lorenz`. `linux/build-appimage.sh` bundles Qt on
  Ubuntu 22.04. `./install.sh` still builds from source for one user.
- Keep the core and desktop executables in the same directory.
- Qt is dynamically linked; distribute its libraries and plugins under their
  applicable licenses. Conversion engines are recommended, never bundled.

For a staged distro install after building the Rust and Qt executables:

```sh
DESTDIR="$pkgdir" cmake --install build --prefix /usr
```

The CMake install rule copies the Rust binary, the Qt desktop executable and
license, then runs `lorenz integrate` against the staged data directory. The
generated launcher points at `/usr/bin/lorenz-desktop` after installation.

## Package repositories

Packages built from source by the distributions' own build services, so that
`dnf upgrade` and `apt upgrade` deliver new versions. Both build offline: each
release carries `lorenz-VERSION-vendor.tar.xz`, the crates of `Cargo.lock`,
written by `linux/vendor.sh` in CI.

- Fedora COPR (`chaostic/lorenz`, Fedora 43 to 45): `fedora/lorenz.spec`
  builds from the release tarball and the vendor archive (`%bcond vendor`,
  on by default), declaring every bundled crate. CI job `fedora-spec` builds it and installs the result. The
  COPR package uses source type SCM, method `make_srpm`: `.copr/Makefile`
  downloads both archives of the version in `Cargo.toml`, so the spec's
  `Version` needs no edit. The release job calls the package's COPR custom
  webhook (secret `COPR_WEBHOOK`, from the project's Integrations page) once
  the release and its vendor archive are published. Add each new Fedora
  release with `copr-cli edit-chroot` or in the project settings; the API
  token in `~/.config/copr` lasts 180 days.
- Ubuntu PPA (`ppa:chaos-tic/lorenz`, Launchpad account `chaos-tic`):
  `debian/` is the Debian packaging, kept out of the source tree as Debian
  asks of upstreams. The release job `ppa` runs `debian/ppa.sh VERSION` in
  `ubuntu:24.04`: it builds a source package per Ubuntu series (24.04, 26.04,
  26.10), signs it with the secret `PPA_SIGNING_KEY` (a key dedicated to CI,
  registered on Launchpad) and uploads it with dput; Launchpad compiles it.
  The vendor archive is the `orig-vendor` component, unpacked in `vendor/`.
  Ubuntu 24.04 builds with its `rustc-1.85`; 22.04 is left out, its Qt is
  older than 6.4. A new Ubuntu series needs a line in its `case`.
- Arch AUR (`lorenz`): the release job `aur` runs `arch/aur.sh VERSION` in
  `archlinux:latest`, with the secret `AUR_SSH_KEY` (a key dedicated to CI,
  added to the AUR account): it sets `pkgver`, updates the checksums and
  `.SRCINFO` with makepkg, and pushes.
- Debian apt repository (`https://chaos-tic.github.io/apt`, repository
  `Chaos-Tic/apt`): publishes the `.deb` of each release, built on Debian 12
  by the `linux-package` job, signed with the repository's own key; it
  refreshes every hour.

A release (`v*` tag) therefore reaches Fedora, Ubuntu and Arch within the
hour of its build, and Debian within the next hour.

### Official Fedora and Debian archives

Both require a sponsor and a review, and both prefer each crate packaged on
its own rather than bundled:

- Fedora: build the spec `--without vendor`, which takes the crates from
  Fedora's `rust-*` packages, file a Package Review request in Red Hat
  Bugzilla, and join the `packager` group through a sponsor.
- Debian (Ubuntu inherits it): file an ITP bug against `wnpp`, upload to
  mentors.debian.net and find a sponsor. The Debian Rust team packages each
  crate separately; the crates Debian lacks (the newest `syn` and `toml`, for
  instance) would have to be packaged first, or Lorenz kept on versions
  Debian has.

Lorenz is GPL-3.0-or-later; packages install the license text. Before wider
distribution: audit engine licenses, sign the Windows installer
and test on physical machines. Flatpak requires
an explicit policy for host engines and permissions; network access must not be
added for convenience.
