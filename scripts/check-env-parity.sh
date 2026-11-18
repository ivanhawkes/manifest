#!/usr/bin/env bash
# Compares devenv.nix, devenv.yaml and devenv.lock with Jettison's committed files at the SDK pin
# (D-006, D-054). The pin is MANIFEST_JETTISON_REVISION; JETTISON_SOURCE_DIR is never read.
# Usage: scripts/check-env-parity.sh [jettison-repository]   (default: the pin's GIT_REPOSITORY)
# A local clone serves only its committed objects at the pin, so uncommitted edits cannot pass.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake_file=source/external/CMakeLists.txt
fail() { echo "env-parity: $*" >&2; exit 1; }

[[ -f "$cmake_file" ]] || fail "$cmake_file: missing"
pin="$(sed -nE 's/^set\(MANIFEST_JETTISON_REVISION ([^ )]*)\)$/\1/p' "$cmake_file")"
[[ "$pin" =~ ^[0-9a-f]{40}$ ]] ||
  fail "$cmake_file: needs exactly one 40-hex MANIFEST_JETTISON_REVISION; found: $(echo ${pin:-none})"
repo="${1:-$(sed -nE 's/^\s*GIT_REPOSITORY (\S*jettison\S*)$/\1/p' "$cmake_file")}"
[[ -n "$repo" && "$repo" != *$'\n'* ]] ||
  fail "$cmake_file: needs exactly one Jettison GIT_REPOSITORY"

ref="$(mktemp -d)"
trap 'rm -rf "$ref"' EXIT
git init --quiet --bare "$ref"
git -C "$ref" fetch --quiet --depth 1 "$repo" "$pin" 2>"$ref/fetch.err" ||
  fail "cannot fetch $pin from $repo: $(tr '\n' ' ' <"$ref/fetch.err")"

status=0
for file in devenv.nix devenv.yaml devenv.lock; do
  if [[ ! -f "$file" ]]; then
    echo "env-parity: $file: missing from Manifest" >&2
    status=1
  elif ! git -C "$ref" cat-file -e "$pin:$file" 2>/dev/null; then
    echo "env-parity: $file: not in Jettison at $pin" >&2
    status=1
  elif ! diff -u --label "jettison@$pin/$file" --label "manifest/$file" \
    <(git -C "$ref" show "$pin:$file") "$file" >&2; then
    echo "env-parity: $file: differs from Jettison at $pin" >&2
    status=1
  fi
done
((status == 0)) && echo "env-parity: devenv.nix, devenv.yaml and devenv.lock match Jettison $pin"
exit "$status"
