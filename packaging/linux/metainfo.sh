#!/usr/bin/env bash
# Writes the AppStream metainfo of Lorenz to $1, with one <release> per
# "## VERSION — DATE" heading of CHANGELOG.md. Run from the repository root.
set -euo pipefail
out=${1:?usage: metainfo.sh OUTPUT}
releases=$(sed -n 's/^## \([0-9][0-9.]*\) — \([0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}\)$/    <release version="\1" date="\2"\/>/p' CHANGELOG.md)
[[ -n $releases ]] || { echo "metainfo: no release found in CHANGELOG.md" >&2; exit 1; }
version=$(sed -n 's/^version = "\(.*\)"$/\1/p' Cargo.toml | head -n1)
[[ $releases == *"version=\"$version\""* ]] || {
    echo "metainfo: CHANGELOG.md has no entry for version $version" >&2; exit 1; }
mkdir -p "$(dirname "$out")"
while IFS= read -r line; do
    if [[ $line == @RELEASES@ ]]; then printf '%s\n' "$releases"; else printf '%s\n' "$line"; fi
done <packaging/linux/lorenz.metainfo.xml.in >"$out"
