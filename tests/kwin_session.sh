#!/usr/bin/env bash
set -euo pipefail
[[ -n "${METROID_TEST_TRACE:-}" ]] && set -x
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
# Starts a real drag from the sample. The Qt test source needs a moment to close
# a cancelled drag before it accepts a new one; retries are logged, never hidden.
start_drag() {
    sleep 0.5
    for try in 1 2 3; do
        call Move 210 210
        call Button true
        call Move 300 300
        for _ in $(seq 1 20); do [[ "$(call Dragging)" == true ]] && return 0; sleep 0.1; done
        echo "Drag did not start (attempt $try); retrying" >&2
        call Button false
        sleep 0.5
    done
    return 1
}
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
start_drag
[[ "$(active)" == false ]]
# Continue moving after the drag starts: the seat pointer may now be stale.
call Move 650 420
call Key 42 true
[[ "$(call WheelCenteredAt 650 420)" == true ]] || { echo "Wheel is not centered at the Shift press position" >&2; exit 1; }
sleep 0.4
call Key 42 false
[[ "$(active)" == true ]]
formats=$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats)
[[ "$formats" == $'jpg\nwebp\ntiff\nbmp\navif\npdf' ]]
[[ "$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Mode)" == convert ]]
# WebP is the right-hand sector for this registry and sample.
call Move 772 420
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
# Ctrl opens the tools wheel; Alt and Shift switch wheels while it stays open.
mode() { qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Mode; }
start_drag
call Key 29 true
call Key 29 false
sleep 0.4
[[ "$(active)" == true && "$(mode)" == tools ]]
qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats | grep -qx compress
call Snapshot "$METROID_TEST_RUN/tools.png"
call Key 56 true
call Key 56 false
sleep 0.4
[[ "$(active)" == true && "$(mode)" == actions ]]
qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats | grep -qx copy-to
call Snapshot "$METROID_TEST_RUN/actions.png"
call Key 42 true
call Key 42 false
sleep 0.4
[[ "$(mode)" == convert ]]
qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats | grep -qx webp
call Key 29 true
call Key 29 false
sleep 0.4
# Keyboard selection: Right selects the first tool (Compress), Enter applies it.
call Key 106 true
call Key 106 false
call Key 28 true
call Key 28 false
[[ "$(active)" == false ]]
[[ "$(call Dragging)" == false ]]
call Button false
for attempt in $(seq 1 100); do
    [[ -f "$METROID_TEST_RUN/sample-compressed.png" ]] && break
    sleep 0.1
 done
[[ -s "$METROID_TEST_RUN/sample-compressed.png" ]]
[[ "$(sha256sum "$METROID_TEST_RUN/sample.png")" == "$original" ]]
# Alt opens the actions wheel; Copy path (fourth item) runs from the keyboard and
# reaches the session clipboard through the environment KWin gives its children.
start_drag
call Key 56 true
call Key 56 false
sleep 0.4
[[ "$(mode)" == actions ]]
[[ "$(qdbus6 org.kde.KWin /Metroid org.metroid.KWin.Formats | sed -n 4p)" == path ]]
for _ in 1 2 3 4; do call Key 106 true; call Key 106 false; done
call Key 28 true
call Key 28 false
call Button false
[[ "$(active)" == false ]]
clipboard=''
for attempt in $(seq 1 50); do
    clipboard=$(WAYLAND_DISPLAY=metroid-test timeout 2 wl-paste --no-newline 2>/dev/null || true)
    [[ -n "$clipboard" ]] && break
    sleep 0.1
done
[[ "$clipboard" == "$(realpath "$METROID_TEST_RUN/sample.png")" ]] || { echo "Clipboard: '$clipboard'" >&2; exit 1; }
# Holding Ctrl before the drag keeps the normal copy drag: no wheel opens.
call Key 29 true
start_drag
[[ "$(active)" == false ]]
call Button false
call Key 29 false
[[ "$(call Dragging)" == false ]]
# Escape must cancel without producing a second output.
start_drag
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
start_drag
call Key 42 true
sleep 0.2
call Key 42 false
call Move 300 300
call Button false
[[ "$(active)" == false ]]
[[ ! -e "$METROID_TEST_RUN/sample-2.webp" ]]
# A normal drag still works without activating the conversion wheel.
start_drag
[[ "$(active)" == false ]]
call Button false
[[ "$(call Dragging)" == false ]]
# Unloading and reloading must leave no dangling input filter.
qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.UnloadPlugin "$METROID_PLUGIN_ID"
[[ "$(qdbus6 org.kde.KWin /Plugins org.kde.KWin.Plugins.LoadPlugin "$METROID_PLUGIN_ID")" == true ]]
[[ "$(active)" == false ]]
