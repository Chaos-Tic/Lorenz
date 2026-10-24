# Drag gesture for KDE Plasma (Wayland)

[Back to Lorenz](../../README.md)

This compositor extension is the primary Lorenz interaction: **drag local files,
tap Shift (convert), Ctrl (tools) or Alt (actions), and release over a sector of the
wheel at the cursor**. Releasing the key does not close the wheel; tapping another
wheel key switches wheels in place. The batch window is optional.

## Supported scope

- Implemented and locally tested with KWin **6.8.0** and **6.8.1**, Wayland pointer
  drags, and local `text/uri-list` files. The build script accepts the KWin 6 release
  series.
- XWayland source apps, touch/tablet drags, GNOME and other compositors are not
  validated or implemented by this extension. Windows has its own gesture in the
  tray companion (`desktop/windowsgesture.h`).
- Remote URLs and non-file drags are ignored. Folders get the actions and ZIP tool.
  Mixed selections show common items only. Batches are limited to 128 paths and
  256 KiB of URI data.
- Keys are configurable per wheel (Shift, Ctrl, Alt or off) in the companion's
  settings, stored in `~/.config/lorenz/lorenz.conf` and read at each drag.

## Install and remove

`./install.sh` offers this step automatically on KDE Plasma 6. Package installations
also place the sources and scripts in `/usr/share/lorenz`. On Arch Linux, the Pacman
hook `/usr/share/libalpm/hooks/90-lorenz-kwin.hook` rebuilds the extension
automatically whenever `kwin` or `lorenz` is upgraded. To run it by hand (or via
**Diagnostics → Enable KWin drag gesture**), install KWin headers and build files
through your distribution: `kwin extra-cmake-modules vulkan-headers` on Arch,
`kwin-dev extra-cmake-modules libvulkan-dev` on Debian/Ubuntu, `kwin-devel
extra-cmake-modules vulkan-headers` on Fedora.

```sh
./scripts/install-kwin.sh
```

The production module is installed in Qt's `kwin/plugins` directory using `sudo` (or
`pkexec` from the graphical interface) and obsolete revisions are removed. The
installer loads it through KWin's plugin interface and enables
`lorenz-<revision>Enabled` in the user's `kwinrc`. No compositor restart is required.

```sh
./scripts/uninstall-kwin.sh
```

Removal unloads the extension and disables future loading. It does not remove the
standalone converter. Rebuild after KWin updates: the plugin is tied to the compositor
ABI, and KWin rejects a mismatched plugin version. Do not reuse an old binary.

Some distribution development packages omit `abstract_data_source.h`. The build
script fetches that header from the official KWin source tag matching the installed
version. Its original copyright/license notices remain in the build cache; third-party
source is not copied into this repository.

## How the gesture works

The input filter runs after screen-lock handling and before normal drag-and-drop.
It activates only on a configured key *press* during an active left-button pointer
drag offering file URIs; a modifier held before the drag keeps its normal copy/move
meaning. It reads the URI payload asynchronously, asks `lorenz menu` for the wheel's
items, and anchors the internal Qt wheel to the compositor pointer position at the
key press. The anchor stays fixed while selecting; screen-edge placement keeps the wheel
visible. It does not use the Wayland seat position, which can be stale during a drag. It never records typed text.

Releasing over a sector (or pressing Enter after choosing with the arrow keys) cancels
the original drag, then launches a separate conversion, tool or action process.
Cancellation is deliberate: Shift's usual move action must not remove the original.
Escape, the center, and the outside of the wheel cancel without running anything.
Modifier keys still reach applications; other keys are swallowed while a wheel is open.
URI and format discovery have a five-second timeout; processing never waits for a
conversion engine inside the compositor. A short-lived tray job handles cancellation
and a desktop notification reports the result.

This is native compositor code. Test changes in isolation before loading them into
a working desktop. The standalone core remains useful if the integration is disabled.

## Verification

```sh
./scripts/test-kwin-isolated.sh
```

The test uses a private D-Bus session, a virtual KWin output, generated sample data,
and a real Wayland drag source. A separately built test module injects input only
inside that session. It checks Shift without a drag, an ordinary drag, a Shift tap
that remains latched at the pointer, hover selection, actual PNG-to-WebP conversion,
original-file preservation, the Ctrl tools wheel, switching to Alt and back to Shift,
keyboard selection running **Compress**, Ctrl held before a drag (no wheel), Escape,
center cancellation, and plugin unload/reload.

The test driver is **not installed** by any installation script. Its factory refuses
to load without the isolated-test environment flag. The test requires a working
virtual KWin rendering backend; it builds the core and desktop from the working tree
and points the extension at them with `LORENZ_BIN_DIR`. It is separate from the CI
build check. The Qt wheel's geometry and segment hit testing also run through CTest.

### Live updates

Qt can retain a plugin library after KWin reports it unloaded. Builds therefore
use a source-derived plugin identity and installed filename. The installer loads
the new identity, verifies its `BuildId` over D-Bus, and disables older revisions
for future logins. It never restarts the compositor or closes desktop applications.
Old libraries may remain mapped until logout; they no longer have active input filters.

Inspect the running version with `qdbus6 org.kde.KWin /Lorenz org.lorenz.KWin.BuildId`.
`org.lorenz.KWin.WheelSize` reports the actual loaded wheel size (384 pixels, including
transparent margins and the wheel tabs). `Mode` and `Formats` report the open wheel and
its item identifiers.
