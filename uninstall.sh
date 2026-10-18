#!/usr/bin/env bash
# Removes Metroid from the current user's account, and the KWin extension if
# it is installed. Converted files and originals are never touched.
set -euo pipefail
cd "$(dirname "$0")"
data=${XDG_DATA_HOME:-$HOME/.local/share}
config=${XDG_CONFIG_HOME:-$HOME/.config}

pkill -f -x '.*/metroid-desktop( --background)?' 2>/dev/null || true
if [[ -x $HOME/.local/bin/metroid ]]; then "$HOME/.local/bin/metroid" autostart disable >/dev/null || true; fi
rm -f "$config/autostart/metroid.desktop"

qtpaths_tool=$(command -v qtpaths6 || echo /usr/lib/qt6/bin/qtpaths6)
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS 2>/dev/null || true)
if [[ -n $plugin_dir ]] && compgen -G "$plugin_dir/kwin/plugins/metroid*.so" >/dev/null; then
    ./scripts/uninstall-kwin.sh
fi

rm -f "$HOME/.local/bin/metroid" "$HOME/.local/bin/metroid-desktop" \
    "$data/applications/metroid.desktop" \
    "$data/kio/servicemenus/metroid.desktop" \
    "$data/icons/hicolor/scalable/apps/metroid.svg" \
    "$data"/nemo/actions/metroid*.nemo_action \
    "$data/nautilus/scripts/Metroid" \
    "$data/nautilus/scripts/Convert with Metroid" \
    "$data/nautilus/scripts/Metroid tools" \
    "$data/nautilus/scripts/Metroid actions"

if [[ -d $config/metroid ]]; then
    reply=n
    [[ -t 0 ]] && read -r -p "Also remove your Metroid settings ($config/metroid)? [y/N] " reply
    [[ ${reply,,} == y* ]] && rm -rf "$config/metroid"
fi
printf 'Metroid has been removed. Your files were not modified.\n'
