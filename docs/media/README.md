# Documentation visuals

[Back to the project README](../../README.md)

| Asset | Purpose |
| --- | --- |
| `shift-drag-wheel.png`, `ctrl-drag-tools.png`, `alt-drag-actions.png` | The Convert, Tools and Actions wheels rendered by the app, one choice magnified |
| `three-wheels.png` | The three wheels side by side (light; `three-wheels-dark.png` is kept for other uses) |
| `demo.gif` | README hero: a photo is dragged, Shift is tapped, the wheel (native frames from the CI demo run, `LORENZ_WHEEL_DEMO`) opens over `tests/fixtures/demo-scene-light.jpg`, the photo is released on WebP and the new file appears with a notification. Composed with ImageMagick at 30 fps |
| `shift-drag.gif` | Native wheel frames every 40 ms: opening, magnifying, switching to Tools and Actions, dismissal |
| `settings.png` | Actual settings dialog (wheel keys, start at login, update check, language), light theme |
| `banner.svg` | Vector header: a glass wheel over a dawn backdrop (`banner-dark.svg` is a dusk variant, not used by the README) |
| `desktop-ready.png` | Actual native Qt window before adding files |
| `desktop-converted.png` | Actual successful PNG-to-WebP conversion using generated sample data |
| `workflow.gif` | Two-state slideshow of those screenshots, not a real-time recording (README, batch manager) |

Screenshots contain generated demonstration data, no private documents or personal
paths. The batch screenshots are 1060 × 820 and their slideshow holds each state for
three seconds. The primary wheel captures are 384 × 384; the animation uses native widget frames at 20 ms intervals. A still image is available beside the optional animation in the main
README. Keep future captures in English and avoid decorative motion outside the demo.

To refresh the visuals on Linux, build and install the current app, generate a PNG
in a disposable directory, then set `LORENZ_SCREENSHOT` to an absolute destination:

```sh
QT_QPA_PLATFORM=offscreen LORENZ_THEME=light LORENZ_SCREENSHOT=/absolute/path/desktop-ready.png \
  lorenz-desktop --smoke-test
QT_QPA_PLATFORM=offscreen LORENZ_THEME=light LORENZ_SCREENSHOT=/absolute/path/desktop-converted.png \
  lorenz-desktop --test-convert /absolute/path/sample.png
```

The second command performs a real conversion and leaves a converted file beside the
sample. Inspect captures for layout, filenames, and accidental personal information
before publishing. The SVG is editable source; no external font or image fetch is needed.

The wheel images come from CI (the `wheel-frames` artifact of the desktop-linux job): the wheel test renders its frames with `LORENZ_THEME=light|dark` over `tests/fixtures/wallpaper-*.jpg` (`LORENZ_WHEEL_BACKDROP`), then each frame is laid over the same wallpaper and given rounded corners with ImageMagick. The batch window and settings captures show the Liquid Glass interface in the light theme (`LORENZ_THEME=light`); README visuals are always light.

Earlier wheel captures came from the isolated KWin interaction test (`wheel.png`, `tools.png`,
`actions.png` in its evidence folder). The input driver captures only the wheel widget,
avoiding unrelated desktop content. The settings capture uses
`LORENZ_SCREENSHOT=/absolute/path/settings.png lorenz-desktop --settings`.

To capture the wheel animation frames: `QT_QPA_PLATFORM=offscreen
LORENZ_WHEEL_FRAMES=/absolute/path/frames build/lorenz-wheel-test`.
