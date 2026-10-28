#!/usr/bin/env bash
# Classify documentation-only changes conservatively; anything uncertain runs full CI.
set -euo pipefail
full=true
base=''
head=''
case "${GITHUB_EVENT_NAME:-}" in
  pull_request) base="${PR_BASE_SHA:-}"; head="${PR_HEAD_SHA:-}" ;;
  push) if [[ "${GITHUB_REF:-}" == refs/heads/* ]]; then base="${PUSH_BEFORE_SHA:-}"; head="${GITHUB_SHA:-}"; fi ;;
esac
if [[ "$base" =~ ^[0-9a-f]{40}$ && "$head" =~ ^[0-9a-f]{40}$ && ! "$base" =~ ^0+$ ]] && git cat-file -e "$base^{commit}" 2>/dev/null && git cat-file -e "$head^{commit}" 2>/dev/null; then
  paths_file=$(mktemp)
  trap 'rm -f "$paths_file"' EXIT
  git diff --no-renames --name-only -z "$base" "$head" -- > "$paths_file"
  if [[ -s "$paths_file" ]]; then
    full=false
    while IFS= read -r -d '' path; do
      case "$path" in
        *.md|*.rst|docs/*.txt|docs/*.png|docs/*.jpg|docs/*.jpeg|docs/*.svg|docs/*.webp) ;;
        *) full=true; break ;;
      esac
    done < "$paths_file"
  fi
fi
printf 'full=%s\n' "$full" >> "$GITHUB_OUTPUT"
printf 'Full checks: %s\n' "$full"
