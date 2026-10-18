# Troubleshooting

[Back to the README](../README.md)

## No output format appears

Open **Diagnostics**. Install the missing engine, then restart Metroid so it sees an
updated `PATH`. If engines are present, try one file: mixed input types may have no
common output. Unknown extensions have no registered routes.

## The engine is installed but conversion fails

Hover over **Failed** to read the engine error. Executable availability does not
certify codecs, delegate permissions, or a valid source file. HEIC/AVIF support and
FFmpeg codecs vary across packages. Metroid currently expects ImageMagick 7's
`magick` executable; an ImageMagick 6 `convert` executable alone is insufficient.

## Pandoc cannot find a data file

Some distribution builds cannot export DOCX/ODT with `--sandbox` because they need
external data resources. Use a Pandoc build compiled with `embed_data_files`.
Metroid does not silently disable the sandbox after failure. Text and HTML routes
can still work. See [Pandoc's sandbox documentation](https://pandoc.org/MANUAL.html#general-options).

## Saving to a removable drive fails

Atomic output publication needs hard-link support. FAT/exFAT and some network
filesystems do not provide it. Choose a supported local filesystem for the output,
then copy the finished file to the removable drive.

## PDF export contains only one page

PDF-to-PNG currently exports the first page at 150 dpi. Multipage PDF image export
is planned, not implemented. `merge-pdf` merges PDF inputs through qpdf and is a CLI
operation rather than a desktop control in this version.

## The window does not stay on top

Wayland compositors control window placement and stacking. The checkbox requests
this behavior; it cannot override compositor rules. This checkbox belongs to the secondary batch manager. The KWin Shift-drag wheel
is a separate compositor integration.

## Menus are missing after installation

Restart the affected file manager. Linux integration writes per-user launcher and
Nautilus/Dolphin/Nemo files; each desktop still needs manual validation. Thunar uses
a manually configured custom action. Windows installation creates **Start** and
**Send to** shortcuts, not a native Windows 11 top-level context-menu extension.

## A conversion was interrupted by a crash or power failure

Originals are not modified. A temporary `.metroid-*` directory may remain beside the
outputs. Ensure no conversion is running before removing it. Restart Metroid and
add the files again; the previous queue is not persisted.

## Reporting a problem

Include the operating system, desktop/session type, Metroid version, input/output
formats, and the engine error. Review diagnostics and screenshots before sharing:
engine paths, file names, and document content may be personal. Prefer a generated
sample that reproduces the issue over uploading an original private document.

## Shift does not open a wheel during dragging

The global gesture needs the KWin 6.8 Wayland integration, not merely the standalone
app. Start dragging the file first, then tap Shift while still holding the mouse
button. No wheel is expected for text-only drags, folders, or Shift alone.

After a KWin update, rebuild and reinstall the matching extension. Other compositors
and Windows do not yet have global Shift-drag integration; use a context action there.
Opening the application menu entry shows a tray companion, not the old batch window.
The batch manager remains available from its tray menu.
