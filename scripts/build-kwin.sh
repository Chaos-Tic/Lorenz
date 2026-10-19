#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# KWin's binary plugin ABI is version-specific. Build against the installed release.
header=/usr/include/kwin/config-kwin.h
[[ -f "$header" ]] || { echo 'Install the KWin development package first.' >&2; exit 1; }
version=$(sed -n 's/^#define KWIN_PLUGIN_VERSION_STRING "\([0-9.]*\)"/\1/p' "$header")
[[ "$version" == 6.8.* ]] || { echo "This integration currently targets KWin 6.8; found $version." >&2; exit 1; }
source_dir="$PWD/build/kwin-source-$version"
mkdir -p "$source_dir"
if [[ ! -f /usr/include/kwin/wayland/abstract_data_source.h ]]; then
    curl --fail --silent --show-error --location \
      "https://invent.kde.org/plasma/kwin/-/raw/v$version/src/wayland/abstract_data_source.h" \
      -o "$source_dir/abstract_data_source.h"
else
    source_dir=/usr/include/kwin/wayland
fi
cmake -S integrations/kwin -B build/kwin -DCMAKE_BUILD_TYPE=Release \
  -DKWIN_SOURCE_INCLUDE="$source_dir" -DLORENZ_BUILD_ISOLATED_TEST_DRIVER=OFF
cmake --build build/kwin --parallel 2
