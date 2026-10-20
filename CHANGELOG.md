# Changelog

## Unreleased

- Lorenz checks GitHub once a day for a newer release and shows how to update
  for the way it was installed (package manager, winget, AppImage, source),
  from the tray and the batch manager. One anonymous request; turn it off in
  Settings.
- New icon matching the frosted-glass wheel of the README and the app: the
  window, the sidebar, the installed icon and the Windows installer use it.
- The Windows installer and the AppImage bundle Qt: they now also ship Qt's
  license (LGPL-3.0) and `THIRD-PARTY-NOTICES.txt`, the licenses of the Rust
  crates linked into Lorenz.

## 1.1.0 — 2026-10-19

### Install and updates

- Fedora 43–45: `sudo dnf copr enable chaostic/lorenz`, rebuilt by Fedora COPR
  from each release.
- Debian 12+ and Ubuntu 24.04+: the Chaos-Tic apt repository
  (chaos-tic.github.io/apt), refreshed every hour from the releases.
- Ubuntu 24.04, 26.04, 26.10: `ppa:chaos-tic/lorenz`, built by Launchpad.
- The AUR package and the PPA follow each release from CI.
- Each release carries `SHA256SUMS`; [docs/VERIFYING.md](docs/VERIFYING.md)
  explains how to check a download.

### Fixes

- Cancelling a conversion, or reaching its time limit, now also stops the
  processes the engine started (LibreOffice's `soffice.bin`, Ghostscript under
  ImageMagick…), not only the engine itself.

### Documentation

- [SECURITY.md](SECURITY.md): how to report a vulnerability privately, the
  threat model and its known limits.
- [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md): what has been verified,
  where and when.
- README: where the name comes from, acknowledgements, official download
  channels, code signing status, and the gaps not closed yet.

## 1.0.0 — 2026-10-19

First release: the Convert, Tools and Actions wheels under the cursor on
Windows 10/11 and KDE Plasma 6.8 (Wayland), file-manager menus elsewhere,
frosted-glass wheel, English and French, Windows installer, Linux packages
(deb, rpm, Arch, AppImage) and the AUR package.
