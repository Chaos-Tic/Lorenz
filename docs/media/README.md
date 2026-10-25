# Documentation visuals

[Back to the project README](../../README.md)

Every file here is regenerated from canned demo data by
[`scripts/render-docs.sh`](../../scripts/render-docs.sh); no real file, window or
path is ever shown. Following the Chaos-Tic Liquid Glass art direction, the hero
visuals and the key captures ship in two matching variants — light and dark —
that share the same accents (Convert blue `#40baf0`, Tools amber `#ffa840`,
Actions violet `#ce6cf0`) and only flip the surface. The README and the
interaction guide pick the variant with `<picture>` and `prefers-color-scheme`.

| Asset (light · dark) | Purpose |
| --- | --- |
| `banner.svg` · `banner-dark.svg` | Vector header: the three wheels over a dawn / dusk backdrop. Text is set in Geist and flattened to paths, so GitHub needs no font. |
| `three-wheels.gif` · `three-wheels-dark.gif` | README hero: the whole gesture in one loop — open Convert (blue), magnify WebP, switch to Tools (amber), then Actions (violet), then dismiss. 600 px, 16 fps. |
| `three-wheels.png` · `three-wheels-dark.png` | The three wheels side by side (Convert · Tools · Actions), each with one choice magnified. |
| `workflow.gif` · `workflow-dark.gif` | Batch manager: a two-state slideshow (empty list, then a finished WebP conversion), not a real-time recording. |
| `settings.png` · `settings-dark.png` | The actual settings dialog (wheel keys, start at login, update check, language). |
| `shift-drag-wheel.png`, `ctrl-drag-tools.png`, `alt-drag-actions.png` | Each wheel on its own, light theme, for the interaction guide. |
| `desktop-converted.png` | The batch window after one sample conversion, light theme, for the interaction guide. |

## Regenerating

Build the desktop, then run the script:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./scripts/render-docs.sh
```

The wheels, the batch window and the settings dialog are rendered head-less
(`QT_QPA_PLATFORM=offscreen`) by `build/lorenz-wheel-test` and
`build/lorenz-desktop` — the same mechanism as CI's `wheel-frames` job — so no
graphical session is needed and the output is reproducible. `build/lorenz-wheel-test`
cycles the three wheels into `frame-000…frame-119` and drops clean stills, and the
script composes the GIFs (ffmpeg), the stills (ImageMagick) and the banners
(Inkscape flattens the Geist text to paths; librsvg is used for previews).

The banners need the Geist font (OFL-1.1) only at render time, to vectorise the
text. The script fetches the static TTFs once into a scratch directory; set
`GEIST_DIR=/path/to/geist` to render fully offline from a local copy. The font is
never installed on the host and never shipped in the repository.

Inspect new captures for layout, filenames and any accidental personal
information before publishing. Keep captures in English.
