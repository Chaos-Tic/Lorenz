# Distribution

Version 0.1 uses per-user installations: from source on Linux, from a prebuilt
unsigned package or from source on Windows. No package is claimed to be
published, signed, or accepted by a distribution repository.

- Linux: `./install.sh` builds from source for the current user (`~/.local`). It
  uses the distribution's package manager (pacman, apt, dnf, zypper, xbps, apk) only
  for build tools and engines, and asks before each step, including start at login.
  `./uninstall.sh` removes everything it installed.
- Windows: CI builds `Metroid-windows-x64.zip` (binaries with Qt deployed by
  windeployqt, `Install.cmd`, `Uninstall.cmd`), installs and uninstalls it, and
  attaches it to the release when a `v*` tag is pushed. The installer is
  per-user, asks before installing engines through winget and before enabling
  start at sign-in. `scripts/install-windows.ps1` also builds from a checkout.
- Keep the core and desktop executables in the same directory.
- Qt is dynamically linked; distribute its libraries and plugins under their
  applicable licenses. Conversion engines are optional and are not bundled.
- Core CI artifacts do not include Qt or conversion engines.

Before public distribution: choose the project license, audit engine licenses,
build and verify deb/rpm/PKGBUILD packages, deploy Qt on Windows and Linux, test in
VMs, then arrange signing and update channels. Flatpak requires an explicit policy
for host engines and permissions; network access must not be added for convenience.
