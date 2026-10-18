#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./scripts/build-kwin.sh
qtpaths_tool=$(command -v qtpaths6 || true)
if [[ -z "$qtpaths_tool" ]]; then qtpaths_tool=/usr/lib/qt6/bin/qtpaths6; fi
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS)
[[ -x "$HOME/.local/bin/metroid" && -x "$HOME/.local/bin/metroid-desktop" ]] || {
    echo 'Install the core and desktop first with scripts/install-linux.sh.' >&2; exit 1;
}
# A unique path prevents Qt from reusing an older mapped library.
plugin_id=$(cat build/kwin/plugin-id)
[[ "$plugin_id" =~ ^metroid-[0-9a-f]{12}$ ]]
previous=()
while IFS= read -r id; do
    if [[ "$id" == metroid || "$id" =~ ^metroid-[0-9a-f]{12}$ ]]; then
        previous+=("$id")
        qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$id"
    fi
done < <(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadedPlugins)
if [[ ! -f "$plugin_dir/kwin/plugins/$plugin_id.so" ]]; then
    sudo install -m755 build/kwin/metroid.so "$plugin_dir/kwin/plugins/$plugin_id.so"
fi
if [[ "$(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$plugin_id")" != true ]]; then
    for id in "${previous[@]}"; do qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$id"; done
    echo 'KWin did not load the update; previous plugins were restored.' >&2
    exit 1
fi
[[ "$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.BuildId)" == "$plugin_id" ]]
# Disable all older installed revisions for future logins.
for file in "$plugin_dir"/kwin/plugins/metroid*.so; do
    id=$(basename "$file" .so)
    if [[ "$id" == metroid || "$id" =~ ^metroid-[0-9a-f]{12}$ ]]; then
        kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" false
    fi
done
kwriteconfig6 --file kwinrc --group Plugins --key "${plugin_id}Enabled" true
printf 'Metroid update verified and active: %s\n' "$plugin_id"
