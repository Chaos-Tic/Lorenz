# Interaction guide

[Back to the README](../README.md)

## Drag, tap Shift, drop

On KDE Wayland with the [KWin integration](../integrations/kwin/README.md) enabled:

1. Start dragging a local file from your file manager.
2. Tap **Shift** without releasing the mouse button.
3. Release Shift. The format wheel stays open near the cursor.
4. Move onto a format; its sector turns red.
5. Release the mouse button. Conversion runs locally and reports completion by notification.

![The real Shift-drag wheel with WebP selected](media/shift-drag-wheel.png)

You do not need to open a window, add a file to a list, or press a Convert button.
Multiple selected files use their common available formats. The source files remain
unchanged; outputs use `name.ext` and numbered names on later runs.

## Cancel safely

Press **Escape**, or release in the center or outside the wheel. No conversion starts.
Releasing Shift alone does not cancel. Shift without a drag opens nothing; dragging
without Shift retains the file manager's normal behavior.

If format discovery times out, the drag is cancelled. Files are not moved. While an
engine is running, a temporary tray icon provides **Cancel conversion**.

## Context-menu fallback

A file-manager **Convert with Metroid** action opens the same compact wheel for the
selected paths. Click a format to start; click its center to close. This is available
without global drag interception. Windows **Send to → Metroid** uses this path too.

## Optional batch manager

Launching Metroid from the application menu opens a tray companion. Choose **Open
batch manager** from its menu for explicit queue management and diagnostics. It can
also be launched with `metroid-desktop --manager`.

<details>
<summary>View the secondary batch manager</summary>

![Secondary batch manager after a sample conversion](media/desktop-converted.png)

</details>

Add files with **Add files** or `Ctrl+O`, select a common output format and optional
destination, then click **Convert**. **Clear** removes rows, not disk files. Files run
sequentially; errors do not stop the remaining queue. Hover over a status for details.
The progress strip indicates activity, not percentage. **Open last output folder**
opens the directory of the most recent successful output.

Closing during processing requests cancellation and keeps the window open until it
stops. Close it again afterward. The queue is not persisted between launches.

## Platform boundaries

The global gesture currently requires KWin 6.8 on Wayland. Other environments can
use the context-menu wheel, CLI, or batch manager. The converter building on Windows
does not imply native Windows Shift-drag support. See [Troubleshooting](TROUBLESHOOTING.md).
