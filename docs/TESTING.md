# Validation

## Automated coverage

Rust tests cover the registries, extensions, JSON errors, the JSON Lines worker,
source preservation, concurrent output collisions, unusual file names, cancellation,
timeouts, cleanup after failure, tool selection rules, and copy/move without
replacement (files, folders, folder into itself). Real engine tests cover:

- conversions: PNG → WebP, SRT → VTT, Markdown → HTML → text, WAV → FLAC, AVI → GIF,
  PDF → text/PNG, CSV → XLSX → CSV/PDF, RTF → PDF, PPTX → PNG/PDF/ODP, ZIP → TGZ;
- tools: image compress (JPEG + PNG in one selection), rotate, resize, name collisions,
  PDF merge/split/pages to PNG/compress, video first frame/mute/speed/half size,
  audio mono/normalize, ZIP of a folder and odd names (`-x`, `@x`), extraction,
  and an archive entry using `..` that must not escape the extraction folder.

Tests skip engines that are absent; read the log to know the real coverage.
CTest checks the wheel: sector hit testing for 1–8 items, screen-edge placement,
dismissal and reopening, keyboard stepping, tab targets and mode switching.
The headless Qt smoke test verifies window construction and shutdown.

A desktop integration test selects WebP and clicks the real conversion button:

```sh
QT_QPA_PLATFORM=offscreen lorenz-desktop --test-convert /path/to/test.png
```

Use a disposable input directory. The test creates a real output, returns a nonzero
exit code on failure, and leaves the input unchanged. `LORENZ_SCREENSHOT` optionally
sets an absolute screenshot destination for either desktop test mode.

## Manual checklist per platform

1. Drop two PNG files, inspect common formats, and select WebP using the wheel.
2. Convert twice: originals remain unchanged and output names are distinct.
3. Drop an unknown file type: no format is available and a clear message appears.
4. Drop incompatible file types together: conversion is disabled.
5. Test Unicode, spaces, quotes, and shell metacharacters in file names.
6. Select a different output directory, convert, and open the output folder.
7. Convert a long video, cancel, and verify the engine stops without partial output.
8. Close during conversion: cancellation is requested and the window stays open until it stops.
9. Check diagnostics, missing engines, and codec errors.
10. Test Nautilus/Dolphin/Nemo menus, Thunar, and Windows Send to separately.

Run this checklist on GNOME Wayland, KDE Wayland, Hyprland, Sway, XFCE X11,
Windows 10, and Windows 11. Successful compilation does not validate these interactions.

## Global drag gesture

Run `scripts/test-kwin-isolated.sh` to exercise real Wayland drags in a private KWin
session with freshly built binaries: Shift conversion by mouse, Ctrl tools by keyboard,
Alt/Shift switching, a held Ctrl ignored, Escape and center cancellation, and plugin
reload. See the [integration guide](../integrations/kwin/README.md). Set
`LORENZ_TEST_TRACE=1` to trace each step.

## Installer

`scripts/test-install-container.sh IMAGE` copies the source tree into a clean
container, runs `./install.sh --yes --no-office --no-kwin --no-autostart`, then checks
`doctor`, a conversion, two tools, the Dolphin menu, the Qt smoke test, autostart and
`./uninstall.sh`. CI runs it on Debian stable, Ubuntu 24.04, Fedora and Arch. It does
not exercise the interactive questions or the KWin extension; check those by hand.
