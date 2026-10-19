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
  `linux/test-package.sh` installs, checks and removes it. `linux/stage.sh` lays
  out the files every format shares (`/usr/bin`, launcher, icon, Dolphin and
  Nemo menus written by `lorenz integrate --data-dir`). `arch/PKGBUILD` is
  published on the AUR as `lorenz`. `linux/build-appimage.sh` bundles Qt on
  Ubuntu 22.04. `./install.sh` still builds from source for one user.
- Keep the core and desktop executables in the same directory.
- Qt is dynamically linked; distribute its libraries and plugins under their
  applicable licenses. Conversion engines are recommended, never bundled.

Lorenz is GPL-3.0-or-later; packages install the license text. Before wider
distribution: audit engine licenses, sign the Windows installer
and test on physical machines. Flatpak requires
an explicit policy for host engines and permissions; network access must not be
added for convenience.
