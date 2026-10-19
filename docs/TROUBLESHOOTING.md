# Troubleshooting

[Back to the README](../README.md)

## No output format appears

Open **Diagnostics**. Install the missing engine, then restart Lorenz so it sees an
updated `PATH`. If engines are present, try one file: mixed input types may have no
common output. Unknown extensions have no registered routes.

## The engine is installed but conversion fails

Hover over **Failed** to read the engine error. Executable availability does not
certify codecs, delegate permissions, or a valid source file. HEIC/AVIF support and
FFmpeg codecs vary across packages: Fedora's `ffmpeg-free` cannot encode H.264, so
**Compress** for video fails there until RPM Fusion's `ffmpeg` is installed. Lorenz
uses ImageMagick 7's `magick`, or ImageMagick 6's `convert` when `magick` is absent.

## Images missing from a converted document

Documents are read in Pandoc's sandbox, then written through a filter. Images
linked from other files or from the web (rather than stored in the document) are
replaced by their caption in DOCX, ODT and EPUB, and raw HTML is removed from EPUB.
This prevents a received document from carrying your other files into its output.
See [Pandoc's sandbox documentation](https://pandoc.org/MANUAL.html#general-options).

## Saving to a removable drive fails

Atomic output publication needs hard-link support. FAT/exFAT and some network
filesystems do not provide it. Choose a supported local filesystem for the output,
then copy the finished file to the removable drive.

## PDF export contains only one page

The **Convert** wheel's PNG/JPG route exports the first page at 300 dpi. Use the
**Tools** wheel (Ctrl) → **Pages to PNG** for every page, in a new folder. **Merge**
appears in the Tools wheel when two or more PDFs are selected.

## Office, slide or spreadsheet formats are missing

These routes need LibreOffice (`soffice`). Install it, or run `./install.sh --office`.
Lorenz starts LibreOffice with a private temporary profile; a LibreOffice window
that is already open is neither reused nor modified. The first conversion can take
several seconds while that profile is created.

The profile is created in the system temporary folder rather than beside the file,
because LibreOffice on Windows crashes (exit code `0xc0000409`) when its profile
path is longer than about 148 characters. If Office conversions still fail with that
code, check that `TEMP` points to a short path such as `%LOCALAPPDATA%\Temp`.

## The window does not stay on top

Wayland compositors control window placement and stacking. The checkbox requests
this behavior; it cannot override compositor rules. This checkbox belongs to the secondary batch manager. The KWin Shift-drag wheel
is a separate compositor integration.

## Lorenz does not start at login

Check **Settings → Start Lorenz when I log in**, or run `lorenz autostart status`.
The entry is `~/.config/autostart/lorenz.desktop`. Hyprland, Sway and similar
compositors may ignore XDG autostart: add `lorenz-desktop --background` to your
compositor's startup commands. The KWin gesture itself does not need the companion.

## An action is missing from the Actions wheel

Actions appear only when their helper exists: **Share** needs KDE Connect, **Float**
needs `dragon-drop`, `dragon` or `ripdrag`, and **Copy**/**Copy path** need `wl-copy`
(Wayland) or `xclip` (X11). **Share** is offered for files, not folders.

## Menus are missing after installation

Restart the affected file manager. Linux integration writes per-user launcher and
Nautilus/Dolphin/Nemo files; each desktop still needs manual validation. Thunar uses
a manually configured custom action. Windows installation creates **Start** and
**Send to** shortcuts, not a native Windows 11 top-level context-menu extension.

## A conversion was interrupted by a crash or power failure

Originals are not modified. A temporary `.lorenz-*` directory may remain beside the
outputs. Ensure no conversion is running before removing it. Restart Lorenz and
add the files again; the previous queue is not persisted.

## Reporting a problem

Include the operating system, desktop/session type, Lorenz version, input/output
formats, and the engine error. Review diagnostics and screenshots before sharing:
engine paths, file names, and document content may be personal. Prefer a generated
sample that reproduces the issue over uploading an original private document.

## No wheel opens during dragging

The global gesture needs the KWin 6.8 Wayland integration, not merely the standalone
app. Start dragging first, then **tap** the key while still holding the mouse button:
a key held before the drag starts is deliberately ignored. Check the keys in
**Settings** (a wheel can be set to *Off*). No wheel is expected for text-only drags
or a key without a drag.

After a KWin update, rebuild and reinstall the matching extension. Other compositors
and Windows do not yet have global Shift-drag integration; use a context action there.
Opening the application menu entry shows a tray companion, not the old batch window.
The batch manager remains available from its tray menu.
