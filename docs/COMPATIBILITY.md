# Compatibility

Only what has actually been checked is listed, with its date. "Container"
means a clean container without a desktop session: it proves installation,
startup (`lorenz-desktop --smoke-test`, offscreen) and conversions, not the
interaction. Each row is redone before a release (see
[docs/TESTING.md](TESTING.md) for the manual checklist).

## Installation channels

| Channel | Environment | Result | Date |
| --- | --- | --- | --- |
| Fedora COPR `chaostic/lorenz` | Fedora 44 container | installs 1.0.0, starts | 2026-10-19 |
| Fedora COPR builds | Fedora 43, 44, 45 (COPR builders) | build and tests pass | 2026-10-19 |
| Ubuntu PPA | Ubuntu 24.04 container | installs 1.0.0, starts | 2026-10-19 |
| Ubuntu PPA builds | Ubuntu 24.04, 26.04, 26.10 (Launchpad) | build and tests pass | 2026-10-19 |
| apt repository | Debian 12 container (published repository) | installs 1.0.0 | 2026-10-19 |
| apt repository | Debian 12 and 13 containers (local copy) | installs 1.0.0, starts | 2026-10-19 |
| Release `.deb` | Debian 12, Debian stable, Ubuntu 24.04 (CI) | installs, starts, removes | each CI run |
| Release `.rpm`, Arch package | Fedora latest, Arch (CI) | installs, starts, removes | each CI run |
| Windows installer | Windows Server (GitHub runner, CI) | silent install, `doctor`, uninstall | each CI run |
| AUR `lorenz` | Arch | published, version 1.0.0-1; install reported by the maintainer | 2026-10-19 |

## Desktop sessions

| Session | What was checked | Date |
| --- | --- | --- |
| KDE Plasma 6.8, Wayland (Arch, real machine) | KWin extension builds and loads | 2026-10-19 |
| KDE Plasma (nested KWin, `scripts/test-kwin-isolated.sh`, run by hand) | drag gesture opens the wheel | not yet dated |
| KWin extension (CI, Arch) | builds against the current KWin | each CI run |
| Windows 10, Windows 11 (real machines) | not yet dated | — |
| GNOME, Cinnamon, XFCE, Hyprland, Sway (real sessions) | not yet dated | — |

## Not verified

- The drag gesture end to end on a real Plasma session after 1.0.0.
- Windows SmartScreen behaviour on a fresh Windows 10/11 machine.
- Performance and memory use on large files (no reproducible measurement yet).
