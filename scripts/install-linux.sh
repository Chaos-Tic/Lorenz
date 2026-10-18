#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
install -Dm755 target/release/metroid "$HOME/.local/bin/metroid"
install -Dm755 build/metroid-desktop "$HOME/.local/bin/metroid-desktop"
"$HOME/.local/bin/metroid" integrate
printf 'Metroid est installé dans le menu des applications.\n'
