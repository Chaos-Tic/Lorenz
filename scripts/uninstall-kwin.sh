#!/usr/bin/env bash
set -euo pipefail
qtpaths_tool=$(command -v qtpaths6 || true)
if [[ -z "$qtpaths_tool" ]]; then qtpaths_tool=/usr/lib/qt6/bin/qtpaths6; fi
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS)
for file in "$plugin_dir"/kwin/plugins/metroid*.so; do
    id=$(basename "$file" .so)
    if [[ "$id" == metroid || "$id" =~ ^metroid-[0-9a-f]{12}$ ]]; then
        qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$id"
        kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" false
        sudo rm -f -- "$file"
    fi
done
printf 'Metroid Shift-drag integration removed. The standalone converter remains installed.\n'
