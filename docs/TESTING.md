# Validation

## Automated coverage

Rust tests cover the registry, extensions, JSON errors, the JSON Lines worker,
source preservation, concurrent output collisions, unusual file names, cancellation,
timeouts, and cleanup after failure. Real engine tests cover PNG → WebP, SRT → VTT,
Markdown → HTML → text, WAV → FLAC, AVI → GIF, PDF → text/PNG, and PDF merging.
The headless Qt smoke test verifies window construction and shutdown.

A desktop integration test selects WebP and clicks the real conversion button:

```sh
QT_QPA_PLATFORM=offscreen metroid-desktop --test-convert /path/to/test.png
```

Use a disposable input directory. The test creates a real output, returns a nonzero
exit code on failure, and leaves the input unchanged. `METROID_SCREENSHOT` optionally
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

## Global Shift-drag

Run `scripts/test-kwin-isolated.sh` to exercise a real Wayland drag and conversion
in a private KWin session. See the [integration guide](../integrations/kwin/README.md).
CTest also validates radial segment selection and screen-edge placement. The manual
batch-window checklist above covers the secondary interface, not global gestures.
