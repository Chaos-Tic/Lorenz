#!/usr/bin/env bash
# Removes Lorenz from the current user's account, and the KWin extension if
# it is installed. Converted files and originals are never touched.
set -euo pipefail
cd "$(dirname "$0")"
data=${XDG_DATA_HOME:-$HOME/.local/share}
config=${XDG_CONFIG_HOME:-$HOME/.config}

pkill -f -x '.*/lorenz-desktop( --background)?' 2>/dev/null || true
if [[ -x $HOME/.local/bin/lorenz ]]; then "$HOME/.local/bin/lorenz" autostart disable >/dev/null || true; fi
rm -f "$config/autostart/lorenz.desktop"

qtpaths_tool=$(command -v qtpaths6 || echo /usr/lib/qt6/bin/qtpaths6)
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS 2>/dev/null || true)
if [[ -n $plugin_dir ]] && compgen -G "$plugin_dir/kwin/plugins/lorenz*.so" >/dev/null; then
    ./scripts/uninstall-kwin.sh
fi

rm -f "$HOME/.local/bin/lorenz" "$HOME/.local/bin/lorenz-desktop" \
    "$data/applications/lorenz.desktop" \
    "$data/kio/servicemenus/lorenz.desktop" \
    "$data/icons/hicolor/scalable/apps/lorenz.svg" \
    "$data"/nemo/actions/lorenz*.nemo_action \
    "$data/nautilus/scripts/Lorenz" \
    "$data/nautilus/scripts/Convert with Lorenz" \
    "$data/nautilus/scripts/Lorenz tools" \
    "$data/nautilus/scripts/Lorenz actions"

if [[ -d $config/lorenz ]]; then
    reply=n
    [[ -t 0 ]] && read -r -p "Also remove your Lorenz settings ($config/lorenz)? [y/N] " reply
    [[ ${reply,,} == y* ]] && rm -rf "$config/lorenz"
fi
printf 'Lorenz has been removed. Your files were not modified.\n'
