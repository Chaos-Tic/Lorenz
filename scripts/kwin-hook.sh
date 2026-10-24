#!/usr/bin/env bash
# Pacman PostTransaction hook: rebuilds the KWin drag-gesture plugin against
# the installed KWin headers so KWin upgrades never break the gesture.
set -euo pipefail

qtpaths_tool=$(command -v qtpaths6 || true)
if [[ -z "$qtpaths_tool" ]]; then qtpaths_tool=/usr/lib/qt6/bin/qtpaths6; fi
[[ -x "$qtpaths_tool" ]] || exit 0
plugin_dir=$("$qtpaths_tool" --query QT_INSTALL_PLUGINS 2>/dev/null || true)
[[ -n "$plugin_dir" ]] || exit 0

# Only build when KWin headers and a C++ toolchain exist.
[[ -f /usr/include/kwin/config-kwin.h ]] || exit 0
command -v cmake >/dev/null 2>&1 && command -v c++ >/dev/null 2>&1 || exit 0

root=$(cd "$(dirname "$0")/.." && pwd)
[[ -f "$root/integrations/kwin/CMakeLists.txt" ]] || exit 0

work=$(mktemp -d "${TMPDIR:-/tmp}/lorenz-kwin-hook.XXXXXX")
trap 'rm -rf "$work"' EXIT

if ! LORENZ_KWIN_BUILD_DIR="$work" "$root/scripts/build-kwin.sh" >/dev/null 2>&1; then
    exit 0
fi

plugin_id=$(cat "$work/kwin/plugin-id" 2>/dev/null || true)
[[ "$plugin_id" =~ ^lorenz-[0-9a-f]{12}$ ]] || exit 0

old_ids=()
for file in "$plugin_dir"/kwin/plugins/lorenz*.so; do
    [[ -e "$file" ]] || continue
    id=$(basename "$file" .so)
    if [[ "$id" != "$plugin_id" && ( "$id" == lorenz || "$id" =~ ^lorenz-[0-9a-f]{12}$ ) ]]; then
        old_ids+=("$id")
    fi
done

install -Dm755 "$work/kwin/lorenz.so" "$plugin_dir/kwin/plugins/$plugin_id.so"
for id in "${old_ids[@]}"; do
    rm -f -- "$plugin_dir/kwin/plugins/$id.so"
done

qdbus=$(command -v qdbus6 || command -v qdbus-qt6 || command -v qdbus || echo /usr/lib/qt6/bin/qdbus)

# Update kwinrc and hot-reload in active KDE Wayland sessions.
for home in /home/*; do
    [[ -d "$home" ]] || continue
    kwinrc="$home/.config/kwinrc"
    [[ -f "$kwinrc" ]] || continue
    if grep -qE '^lorenz(-[0-9a-f]{12})?Enabled=' "$kwinrc" 2>/dev/null; then
        owner=$(stat -c '%u:%g' "$kwinrc")
        uid=$(stat -c '%u' "$kwinrc")
        user=$(stat -c '%U' "$kwinrc")
        sed -i -E '/^lorenz(-[0-9a-f]{12})?Enabled=/d' "$kwinrc"
        if grep -q '^\[Plugins\]' "$kwinrc"; then
            sed -i "/^\[Plugins\]/a ${plugin_id}Enabled=true" "$kwinrc"
        else
            printf '\n[Plugins]\n%sEnabled=true\n' "$plugin_id" >>"$kwinrc"
        fi
        chown "$owner" "$kwinrc"
        if [[ -S "/run/user/$uid/bus" && -x "$qdbus" ]] && command -v runuser >/dev/null 2>&1; then
            for id in "${old_ids[@]}"; do
                runuser -u "$user" -- env DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$uid/bus" \
                    "$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$id" >/dev/null 2>&1 || true
            done
            runuser -u "$user" -- env DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$uid/bus" \
                "$qdbus" org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$plugin_id" >/dev/null 2>&1 || true
        fi
    fi
done
