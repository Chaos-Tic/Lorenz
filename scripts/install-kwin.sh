#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
if [[ -n "${LORENZ_KWIN_BUILD_DIR:-}" ]]; then
    build_root="$LORENZ_KWIN_BUILD_DIR"
elif [[ -w "$root" ]]; then
    build_root="$root/build"
else
    build_root="${TMPDIR:-/tmp}/lorenz-kwin-${EUID:-0}"
fi
export LORENZ_KWIN_BUILD_DIR="$build_root"
"$root/scripts/build-kwin.sh"
qtpaths_tool=$(command -v qtpaths6 || true)
if [[ -z "$qtpaths_tool" ]]; then qtpaths_tool=/usr/lib/qt6/bin/qtpaths6; fi
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS)
# Distributions name the Qt 6 D-Bus tool differently.
qdbus=$(command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus || echo /usr/lib/qt6/bin/qdbus)
has_core() {
    { command -v lorenz >/dev/null 2>&1 && command -v lorenz-desktop >/dev/null 2>&1; } ||
    [[ -x "$HOME/.local/bin/lorenz" && -x "$HOME/.local/bin/lorenz-desktop" ]] ||
    [[ -x /usr/bin/lorenz && -x /usr/bin/lorenz-desktop ]]
}
has_core || {
    echo 'Install the core and desktop first with ./install.sh or your package manager.' >&2
    exit 1
}
as_root() {
    if [[ $EUID -eq 0 ]]; then
        "$@"
    elif [[ -t 0 ]] || sudo -n true 2>/dev/null; then
        sudo "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        pkexec "$@"
    else
        sudo "$@"
    fi
}
# A unique path prevents Qt from reusing an older mapped library.
plugin_id=$(cat "$build_root/kwin/plugin-id")
[[ "$plugin_id" =~ ^lorenz-[0-9a-f]{12}$ ]]
previous=()
while IFS= read -r id; do
    if [[ "$id" == lorenz || "$id" =~ ^lorenz-[0-9a-f]{12}$ ]]; then previous+=("$id"); fi
done < <("$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadedPlugins 2>/dev/null || true)
stale=()
for file in "$plugin_dir"/kwin/plugins/lorenz*.so; do
    [[ -e "$file" ]] || continue
    id=$(basename "$file" .so)
    if [[ "$id" != "$plugin_id" && ( "$id" == lorenz || "$id" =~ ^lorenz-[0-9a-f]{12}$ ) ]]; then
        keep=false
        for active_id in "${previous[@]}"; do
            [[ "$id" == "$active_id" ]] && keep=true
        done
        $keep || stale+=("$file")
    fi
done
# Install the file first (and clean stale unloaded revisions in the same call):
# if administrator access fails, nothing has changed.
if [[ ! -f "$plugin_dir/kwin/plugins/$plugin_id.so" ]] || ((${#stale[@]} > 0)); then
    as_root sh -e -c '
        src=$1; dst=$2; shift 2
        install -Dm755 "$src" "$dst"
        if [ "$#" -gt 0 ]; then rm -f -- "$@"; fi
    ' sh "$build_root/kwin/lorenz.so" "$plugin_dir/kwin/plugins/$plugin_id.so" "${stale[@]}"
fi
# From here on, any failure reloads the revisions that were active before.
restore() {
    for id in "${previous[@]}"; do "$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$id" >/dev/null 2>&1 || true; done
    echo 'KWin did not accept the update; the previous version was restored.' >&2
}
trap restore ERR
for id in "${previous[@]}"; do "$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$id"; done
[[ "$("$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$plugin_id")" == true ]]
[[ "$("$qdbus" org.kde.KWin /Lorenz org.lorenz.KWin.BuildId)" == "$plugin_id" ]]
trap - ERR
# Remove older kwinrc keys and enable only the verified revision for future logins.
for file in "$plugin_dir"/kwin/plugins/lorenz*.so; do
    [[ -e "$file" ]] || continue
    id=$(basename "$file" .so)
    if [[ "$id" == lorenz || "$id" =~ ^lorenz-[0-9a-f]{12}$ ]]; then
        kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" --delete 2>/dev/null || \
            kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" false
    fi
done
for id in "${previous[@]}"; do
    if [[ "$id" != "$plugin_id" ]]; then
        kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" --delete 2>/dev/null || \
            kwriteconfig6 --file kwinrc --group Plugins --key "${id}Enabled" false
        if sudo -n true 2>/dev/null; then
            sudo rm -f -- "$plugin_dir/kwin/plugins/$id.so"
        fi
    fi
done
kwriteconfig6 --file kwinrc --group Plugins --key "${plugin_id}Enabled" true
printf 'Lorenz update verified and active: %s\n' "$plugin_id"
