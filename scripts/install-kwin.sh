#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./scripts/build-kwin.sh
qtpaths_tool=$(command -v qtpaths6 || command -v qtpaths || printf /usr/lib/qt6/bin/qtpaths6)
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS)
[[ -x "$HOME/.local/bin/metroid" && -x "$HOME/.local/bin/metroid-desktop" ]] || {
    echo 'Install the core and desktop first with scripts/install-linux.sh.' >&2; exit 1;
}
# Only the production module is installed. The isolated input driver is never copied.
qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin metroid
sudo install -m755 build/kwin/metroid.so "$plugin_dir/kwin/plugins/metroid.so"
if [[ "$(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin metroid)" != true ]]; then
    echo 'KWin did not load Metroid. The plugin was not enabled for future sessions.' >&2
    exit 1
fi
kwriteconfig6 --file kwinrc --group Plugins --key metroidEnabled true
printf 'Metroid is active: drag a local file, tap Shift, and release over a format.\n'
