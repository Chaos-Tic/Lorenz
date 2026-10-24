#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
# KWin's binary plugin ABI is version-specific. Build against the installed release.
header=/usr/include/kwin/config-kwin.h
[[ -f "$header" ]] || { echo 'Install the KWin development package first.' >&2; exit 1; }
version=$(sed -n 's/^#define KWIN_PLUGIN_VERSION_STRING "\([0-9.]*\)"/\1/p' "$header")
[[ "$version" == 6.* ]] || { echo "This integration requires KWin 6; found $version." >&2; exit 1; }
if [[ -n "${LORENZ_KWIN_BUILD_DIR:-}" ]]; then
    build_root="$LORENZ_KWIN_BUILD_DIR"
elif [[ -w "$root" ]]; then
    build_root="$root/build"
else
    build_root="${TMPDIR:-/tmp}/lorenz-kwin-${EUID:-0}"
fi
source_dir="$build_root/kwin-source-$version"
mkdir -p "$source_dir"
if [[ -f /usr/include/kwin/wayland/abstract_data_source.h ]]; then
    source_dir=/usr/include/kwin/wayland
elif [[ ! -s "$source_dir/abstract_data_source.h" ]]; then
    series="${version%.*}.0"
    curl --fail --silent --show-error --location \
      "https://invent.kde.org/plasma/kwin/-/raw/v$version/src/wayland/abstract_data_source.h" \
      -o "$source_dir/abstract_data_source.h" || \
    curl --fail --silent --show-error --location \
      "https://invent.kde.org/plasma/kwin/-/raw/v$series/src/wayland/abstract_data_source.h" \
      -o "$source_dir/abstract_data_source.h"
fi
cmake -S "$root/integrations/kwin" -B "$build_root/kwin" -DCMAKE_BUILD_TYPE=Release \
  -DKWIN_SOURCE_INCLUDE="$source_dir" -DLORENZ_BUILD_ISOLATED_TEST_DRIVER=OFF
cmake --build "$build_root/kwin" --parallel 2
