# Documentation visuals

[Back to the project README](../../README.md)

| Asset | Purpose |
| --- | --- |
| `banner.svg` | Vector header using the desktop's charcoal, red, and muted-gray palette |
| `desktop-ready.png` | Actual native Qt window before adding files |
| `desktop-converted.png` | Actual successful PNG-to-WebP conversion using generated sample data |
| `workflow.gif` | Two-state slideshow of those screenshots, not a real-time recording |

Screenshots contain generated demonstration data, no private documents or personal
paths. The screenshots were captured at 1060 × 820; the GIF holds each state for
three seconds. A still image is available beside the optional animation in the main
README. Keep future captures in English and avoid decorative motion outside the demo.

To refresh the visuals on Linux, build and install the current app, generate a PNG
in a disposable directory, then set `METROID_SCREENSHOT` to an absolute destination:

```sh
QT_QPA_PLATFORM=offscreen METROID_SCREENSHOT=/absolute/path/desktop-ready.png \
  metroid-desktop --smoke-test
QT_QPA_PLATFORM=offscreen METROID_SCREENSHOT=/absolute/path/desktop-converted.png \
  metroid-desktop --test-convert /absolute/path/sample.png
```

The second command performs a real conversion and leaves a converted file beside the
sample. Inspect captures for layout, filenames, and accidental personal information
before publishing. The SVG is editable source; no external font or image fetch is needed.
