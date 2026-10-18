# Distribution

Version 0.1 uses per-user installations from source. No package is claimed to be
published, signed, or accepted by a distribution repository.

- Linux: `./install.sh` builds from source for the current user (`~/.local`). It
  uses the distribution's package manager (pacman, apt, dnf, zypper, xbps, apk) only
  for build tools and engines, and asks before each step, including start at login.
  `./uninstall.sh` removes everything it installed.
- Windows: `scripts/install-windows.ps1`, after installing Rust, CMake, MSVC, and Qt6;
  `-Autostart` / `-NoAutostart` answer the start-at-sign-in question.
- Keep the core and desktop executables in the same directory.
- Qt is dynamically linked; distribute its libraries and plugins under their
  applicable licenses. Conversion engines are optional and are not bundled.
- Core CI artifacts do not include Qt or conversion engines.

Before public distribution: choose the project license, audit engine licenses,
build and verify deb/rpm/PKGBUILD packages, deploy Qt on Windows and Linux, test in
VMs, then arrange signing and update channels. Flatpak requires an explicit policy
for host engines and permissions; network access must not be added for convenience.
