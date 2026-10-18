#!/usr/bin/env bash
set -euo pipefail
: "${METROID_TEST_ROOT:?Run scripts/test-kwin-isolated.sh}"
: "${METROID_TEST_RUN:?Run scripts/test-kwin-isolated.sh}"
export METROID_ISOLATED_TEST=1
export XDG_CONFIG_HOME="$METROID_TEST_RUN/config"
export XDG_RUNTIME_DIR="$METROID_TEST_RUN/runtime"
export QT_PLUGIN_PATH="$METROID_TEST_RUN/plugins"
unset WAYLAND_DISPLAY
kwin_wayland --virtual --width 1024 --height 768 --socket metroid-test \
  --no-lockscreen --no-global-shortcuts --no-kactivities \
  "$METROID_TEST_ROOT/build/metroid-drag-source $METROID_TEST_RUN/sample.png" \
  > "$METROID_TEST_RUN/session.log" 2>&1 &
compositor_pid=$!
trap 'kill "$compositor_pid" 2>/dev/null || true; wait "$compositor_pid" 2>/dev/null || true' EXIT
call() { qdbus6 org.kde.KWin /MetroidTestInput "org.metroid.TestInput.$@"; }
active() { qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Active; }
for attempt in $(seq 1 100); do
    if qdbus6 org.kde.KWin /MetroidTestInput org.metroid.TestInput.Dragging >/dev/null 2>&1; then break; fi
    sleep 0.1
 done
sleep 2
[[ "$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.BuildId)" == "$METROID_PLUGIN_ID" ]]
[[ "$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.WheelSize)" == 384 ]]
original=$(sha256sum "$METROID_TEST_RUN/sample.png")
# Shift without a drag must not activate anything.
call Key 42 true
[[ "$(active)" == false ]]
call Key 42 false
# Start a genuine Wayland file drag, then tap Shift.
call Move 210 210
call Button true
call Move 300 300
sleep 0.3
[[ "$(call Dragging)" == true ]]
[[ "$(active)" == false ]]
# Continue moving after the drag starts: the seat pointer may now be stale.
call Move 650 420
call Key 42 true
[[ "$(call WheelCenteredAt 650 420)" == true ]] || { echo "Wheel is not centered at the Shift press position" >&2; exit 1; }
sleep 0.4
call Key 42 false
[[ "$(active)" == true ]]
formats=$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats)
[[ "$formats" == $'avif\nbmp\njpg\npdf\ntiff\nwebp' ]]
# WebP is the upper-left sector for this registry and sample.
call Move 590 317
[[ "$(call WheelCenteredAt 650 420)" == true ]] || { echo "Wheel is not centered at the Shift press position" >&2; exit 1; }
sleep 0.3
call Snapshot "$METROID_TEST_RUN/wheel.png"
call Button false
[[ "$(active)" == false ]]
[[ "$(call Dragging)" == false ]]
for attempt in $(seq 1 100); do
    [[ -f "$METROID_TEST_RUN/sample.webp" ]] && break
    sleep 0.1
 done
[[ -s "$METROID_TEST_RUN/sample.webp" ]]
[[ "$(sha256sum "$METROID_TEST_RUN/sample.png")" == "$original" ]]
# Escape must cancel without producing a second output.
call Move 210 210
call Button true
call Move 300 300
sleep 0.2
call Key 42 true
sleep 0.2
[[ "$(active)" == true ]]
call Key 42 false
call Key 1 true
call Key 1 false
call Button false
[[ "$(active)" == false ]]
[[ "$(call Dragging)" == false ]]
[[ ! -e "$METROID_TEST_RUN/sample-2.webp" ]]
# The center is another explicit cancellation target.
call Move 210 210
call Button true
call Move 300 300
sleep 0.2
call Key 42 true
sleep 0.2
call Key 42 false
call Move 300 300
call Button false
[[ "$(active)" == false ]]
[[ ! -e "$METROID_TEST_RUN/sample-2.webp" ]]
# A normal drag still works without activating the conversion wheel.
call Move 210 210
call Button true
call Move 300 300
sleep 0.2
[[ "$(call Dragging)" == true ]]
[[ "$(active)" == false ]]
call Button false
[[ "$(call Dragging)" == false ]]
# Unloading and reloading must leave no dangling input filter.
qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$METROID_PLUGIN_ID"
[[ "$(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$METROID_PLUGIN_ID")" == true ]]
[[ "$(active)" == false ]]
