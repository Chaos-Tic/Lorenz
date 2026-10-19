# Distribution

Version 0.1 ships per-user installers on Windows and system packages on Linux,
all built and installed in clean environments by CI, and attached to the
GitHub release when a `v*` tag is pushed. None is signed or accepted by a
distribution repository yet.

- Windows: `windows/metroid.iss` (Inno Setup 6) builds `Metroid-Setup-x64.exe`
  from the executables and the Qt files deployed by windeployqt. It installs for
  the current user without administrator rights, adds Start, Send to and an
  optional desktop shortcut, can install engines through winget and enable start
  at sign-in, and registers an uninstaller in Settings → Apps.
  `scripts/install-windows.ps1` builds and installs from a checkout.
- Linux: `linux/build-package.sh deb|rpm|arch` builds a package in a container of
  the target family (`debian:12`, `fedora:latest`, `archlinux:latest`);
  `linux/test-package.sh` installs, checks and removes it. `linux/stage.sh` lays
  out the files every format shares (`/usr/bin`, launcher, icon, Dolphin and
  Nemo menus written by `metroid integrate --data-dir`). `arch/PKGBUILD` is
  AUR-ready once its checksum is set. `linux/build-appimage.sh` bundles Qt on
  Ubuntu 22.04. `./install.sh` still builds from source for one user.
- Keep the core and desktop executables in the same directory.
- Qt is dynamically linked; distribute its libraries and plugins under their
  applicable licenses. Conversion engines are recommended, never bundled.

Metroid is GPL-3.0-or-later; packages install the license text. Before wider
distribution: audit engine licenses, sign the Windows installer,
publish the PKGBUILD to the AUR, and test on physical machines. Flatpak requires
an explicit policy for host engines and permissions; network access must not be
added for convenience.
