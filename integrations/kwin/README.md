# Shift-drag integration for KDE Wayland

[Back to Metroid](../../README.md)

This compositor extension is the primary Metroid interaction: **drag a local file,
tap Shift, and release over a format in the wheel at the cursor**. Releasing Shift
does not close the wheel. The batch window is optional.

## Supported scope

- Implemented and locally tested with KWin **6.8.0**, Wayland pointer drags, and local
  `text/uri-list` files. The build script currently accepts the 6.8 release series.
- XWayland source apps, touch/tablet drags, GNOME, other compositors, and native
  Windows gesture interception are not validated or implemented by this extension.
- Folders, remote URLs, and non-file drags are not converted. Mixed file selections
  use common formats only. Batches are limited to 128 paths and 256 KiB of URI data.
- Shift is the current trigger. Tools/actions wheels and configurable keys are future work.

## Install and remove

First install the core and desktop with `scripts/install-linux.sh`. Install KWin
headers and build dependencies through your distribution. On Arch these include
`base-devel`, `cmake`, `kwin`, `extra-cmake-modules`, and `vulkan-headers`.

```sh
./scripts/install-kwin.sh
```

The production module is installed in Qt's `kwin/plugins` directory using `sudo`.
The installer loads it through KWin's plugin interface and enables
`metroidEnabled` in the user's `kwinrc`. No compositor restart is required.

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
It activates only when Shift accompanies an active pointer drag offering file URIs.
It reads the URI payload asynchronously, asks the Rust registry for common formats,
and paints a small internal Qt wheel near the pointer. It never records typed text.

Releasing over a sector cancels the original drag, then launches a separate conversion
process. Cancellation is deliberate: Shift's usual move action must not remove the
original. Escape, the center, and the outside of the wheel cancel without conversion.
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
that remains latched, hover selection, actual PNG-to-WebP conversion, original-file
preservation, Escape, center cancellation, and plugin unload/reload.

The test driver is **not installed** by any installation script. Its factory refuses
to load without the isolated-test environment flag. The test requires a working
virtual KWin rendering backend and installed Metroid executables; it is separate from
the CI build check. The Qt wheel's geometry and segment hit testing also run through CTest.
