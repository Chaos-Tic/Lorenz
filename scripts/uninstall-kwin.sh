#!/usr/bin/env bash
set -euo pipefail
qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin metroid
kwriteconfig6 --file kwinrc --group Plugins --key metroidEnabled false
qtpaths_tool=$(command -v qtpaths6 || command -v qtpaths || printf /usr/lib/qt6/bin/qtpaths6)
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS)
sudo rm -f -- "$plugin_dir/kwin/plugins/metroid.so"
printf 'Metroid Shift-drag integration removed. The standalone converter remains installed.\n'
