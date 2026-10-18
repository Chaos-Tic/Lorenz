#!/usr/bin/env bash
# Builds a test-only input module, then exercises a real drag in a private
# Wayland compositor and D-Bus session. Never connects to the desktop's bus.
set -euo pipefail
cd "$(dirname "$0")/.."
export METROID_TEST_ROOT="$PWD"
./scripts/build-kwin.sh
cmake -S integrations/kwin -B build/kwin -DMETROID_BUILD_ISOLATED_TEST_DRIVER=ON
cmake --build build/kwin --parallel 2
# The compositor runs the binaries built from this tree, not an installed copy.
cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
c++ -std=c++17 tests/drag_source.cpp -o build/metroid-drag-source $(pkg-config --cflags --libs Qt6Widgets)
run_dir=$(mktemp -d "$PWD/build/interaction-XXXXXX")
export METROID_TEST_RUN="$run_dir"
export METROID_PLUGIN_ID=$(cat build/kwin/plugin-id)
mkdir -p "$run_dir/plugins/kwin/plugins" "$run_dir/config" "$run_dir/runtime" "$run_dir/bin"
cp target/release/metroid build/metroid-desktop "$run_dir/bin/"
export METROID_BIN_DIR="$run_dir/bin"
chmod 700 "$run_dir/runtime"
cp build/kwin/metroid.so "$run_dir/plugins/kwin/plugins/$METROID_PLUGIN_ID.so"
cp build/kwin/metroid-testinput.so "$run_dir/plugins/kwin/plugins/"
printf '[Plugins]\n%sEnabled=true\nmetroid-testinputEnabled=true\n' "$METROID_PLUGIN_ID" > "$run_dir/config/kwinrc"
magick -size 32x24 xc:red "$run_dir/sample.png"
dbus-run-session -- bash "$METROID_TEST_ROOT/tests/kwin_session.sh"
printf 'Isolated interaction checks passed. Evidence: %s\n' "$run_dir"
