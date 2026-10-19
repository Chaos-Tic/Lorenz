# Documentation visuals

[Back to the project README](../../README.md)

| Asset | Purpose |
| --- | --- |
| `shift-drag-wheel.png` | Actual KWin Convert wheel while hovering WebP during a file drag |
| `ctrl-drag-tools.png`, `alt-drag-actions.png` | Actual KWin Tools and Actions wheels from the same isolated session |
| `three-wheels.png` | The three captures above, side by side |
| `shift-drag.gif` | Native wheel frames every 20 ms: opening, hover, switching to Tools and Actions, dismissal |
| `settings.png` | Actual settings dialog (wheel keys and start at login) |
| `banner.svg` | Vector header using the desktop's graphite, mint, and soft-white palette |
| `desktop-ready.png` | Actual native Qt window before adding files |
| `desktop-converted.png` | Actual successful PNG-to-WebP conversion using generated sample data |
| `workflow.gif` | Two-state slideshow of those screenshots, not a real-time recording |

Screenshots contain generated demonstration data, no private documents or personal
paths. The batch screenshots are 1060 × 820 and their slideshow holds each state for
three seconds. The primary wheel captures are 384 × 384; the animation uses native widget frames at 20 ms intervals. A still image is available beside the optional animation in the main
README. Keep future captures in English and avoid decorative motion outside the demo.

To refresh the visuals on Linux, build and install the current app, generate a PNG
in a disposable directory, then set `LORENZ_SCREENSHOT` to an absolute destination:

```sh
QT_QPA_PLATFORM=offscreen LORENZ_SCREENSHOT=/absolute/path/desktop-ready.png \
  lorenz-desktop --smoke-test
QT_QPA_PLATFORM=offscreen LORENZ_SCREENSHOT=/absolute/path/desktop-converted.png \
  lorenz-desktop --test-convert /absolute/path/sample.png
```

The second command performs a real conversion and leaves a converted file beside the
sample. Inspect captures for layout, filenames, and accidental personal information
before publishing. The SVG is editable source; no external font or image fetch is needed.

Wheel captures come from the isolated KWin interaction test (`wheel.png`, `tools.png`,
`actions.png` in its evidence folder). The input driver captures only the wheel widget,
avoiding unrelated desktop content. The settings capture uses
`LORENZ_SCREENSHOT=/absolute/path/settings.png lorenz-desktop --settings`.

To capture the wheel animation frames: `QT_QPA_PLATFORM=offscreen
LORENZ_WHEEL_FRAMES=/absolute/path/frames build/lorenz-wheel-test`.
