#!/usr/bin/env bash
# Writes THIRD-PARTY-NOTICES.txt for the packages that bundle third-party code:
# the Windows installer and the AppImage ship Qt (LGPL-3.0), and lorenz links
# the crates of Cargo.lock statically, whose licenses ask that their notices
# travel with the binary. From the repository root:
#   packaging/licenses/notices.sh OUTPUT [TARGET-TRIPLE]
set -euo pipefail
out=${1:?usage: packaging/licenses/notices.sh OUTPUT [TARGET-TRIPLE]}
target=${2:-$(rustc -vV | sed -n 's/^host: //p')}
vendor=$(mktemp -d)
trap 'rm -rf "$vendor"' EXIT
cargo vendor --quiet --locked --versioned-dirs "$vendor/v" >/dev/null

{
    cat <<'EOF'
Third-party software in Lorenz
==============================

Lorenz is free software under the GNU General Public License v3.0 or later
(LICENSE.txt). Packages that bundle the components below ship them under
their own licenses.

Qt 6
----
The Qt libraries and plugins next to lorenz-desktop are (C) The Qt Company
Ltd. and other contributors, used under the GNU Lesser General Public License
v3.0 (LGPL-3.0.txt, which refers to the GPL v3.0 in LICENSE.txt). They are
linked dynamically: you may replace them with your own build of the same Qt
version. Qt's source code: https://download.qt.io/official_releases/qt/

Rust crates linked into lorenz
------------------------------
EOF
    # Only the crates compiled for this target, without build-only ones.
    cargo tree --locked --target "$target" -e normal --prefix none --format '{p}' |
        sed 's/ (.*//; / v/!d' | sort -u | while read -r name version; do
            [ "$name" = lorenz ] && continue
            dir=$vendor/v/$name-${version#v}
            license=$(sed -n 's/^license = "\(.*\)"/\1/p' "$dir/Cargo.toml" | head -n1)
            printf '\n=== %s %s (%s) ===\n' "$name" "${version#v}" "$license"
            found=0
            for file in "$dir"/*; do
                [ -f "$file" ] || continue
                # LICENSE-MIT, license-mit, COPYING, UNLICENSE...
                case $(basename "$file" | tr '[:upper:]' '[:lower:]') in
                licen[cs]e* | copying* | unlicense*) ;;
                *) continue ;;
                esac
                printf '\n--- %s ---\n' "$(basename "$file")"
                cat "$file"
                found=1
            done
            [ $found = 1 ] || printf '\n(The crate ships no license file; its license is %s.)\n' "$license"
        done
} >"$out"
echo "$out"
