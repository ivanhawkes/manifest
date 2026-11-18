#!/usr/bin/env bash
# The faults data/build.sh refuses rather than package: a null in a required field, and two IDs
# with one record name, which yq's split would otherwise reduce to one record without a word.
# packaging-refusals.sh <data source directory>
set -euo pipefail

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# expect_refusal <message>: build.sh over the damaged copy fails, saying so.
expect_refusal() {
  local output
  if output=$(bash "$work/data/build.sh" "$work/out/definitions" 2>&1); then
    printf 'packaging-refusals: build.sh packaged a copy that should fail with "%s"\n' "$1" >&2
    exit 1
  fi
  if [[ "$output" != *"$1"* ]]; then
    printf 'packaging-refusals: expected "%s", got:\n%s\n' "$1" "$output" >&2
    exit 1
  fi
}

fresh_copy() {
  rm -rf "$work/data" "$work/out"
  cp -r "$1" "$work/data"
  rm -rf "$work/data/core-config" "$work/data/definitions"
}

fresh_copy "$1"
sed -i '0,/displayName: Copper$/s//displayName: null/' "$work/data/trade-items/metals.yaml"
expect_refusal "ItemCopper has a null displayName"

fresh_copy "$1"
sed -i 's/itemId: ItemSilver$/itemId: ItemCOPPER/' "$work/data/trade-items/metals.yaml" \
  "$work/data/game-mode/full.yaml" "$work/data/game-mode/simple.yaml"
expect_refusal "two items share a record name"
