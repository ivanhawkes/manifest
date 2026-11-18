#!/usr/bin/env bash
# Runs check-env-parity.sh on copies against a throwaway Jettison repository: the match passes, and
# each controlled difference, missing file, bad pin or override is rejected.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
files=(devenv.nix devenv.yaml devenv.lock)
git_() { git -c user.name=test -c user.email=test@invalid -c commit.gpgsign=false "$@"; }

# The reference: one commit with all three files, a later one without devenv.lock.
engine="$work/engine"
git init --quiet "$engine"
cp "${files[@]/#/$root/}" "$engine"
git_ -C "$engine" add . && git_ -C "$engine" commit --quiet -m all
full="$(git -C "$engine" rev-parse HEAD)"
git_ -C "$engine" rm --quiet devenv.lock && git_ -C "$engine" commit --quiet -m partial
partial="$(git -C "$engine" rev-parse HEAD)"

failures=0
# expect <pass|fail> <name> <pin-line> [setup command...]: a fresh copy, the setup, then the check.
expect() {
  local want="$1" name="$2" pin_line="$3"
  shift 3
  local copy="$work/case"
  rm -rf "$copy" && mkdir -p "$copy/scripts" "$copy/source/external"
  cp "$root/scripts/check-env-parity.sh" "$copy/scripts/"
  cp "${files[@]/#/$root/}" "$copy/"
  printf '%b\n' "$pin_line" '    GIT_REPOSITORY https://github.com/ivanhawkes/jettison.git' \
    >"$copy/source/external/CMakeLists.txt"
  (($#)) && (cd "$copy" && "$@")
  local got=pass
  "$copy/scripts/check-env-parity.sh" "$engine" >"$work/out" 2>&1 || got=fail
  if [[ "$got" == "$want" ]]; then
    echo "ok   $name: $(tail -n1 "$work/out")"
  else
    echo "FAIL $name: wanted $want, got $got" && sed 's/^/     /' "$work/out"
    failures=$((failures + 1))
  fi
}
pin() { echo "set(MANIFEST_JETTISON_REVISION $1)"; }

expect pass "all three match" "$(pin "$full")"
for file in "${files[@]}"; do
  expect fail "$file differs" "$(pin "$full")" sh -c "echo '# drift' >>$file"
  expect fail "$file missing" "$(pin "$full")" rm "$file"
done
expect fail "devenv.lock absent at the pin" "$(pin "$partial")"
expect fail "no pin" "# no revision"
expect fail "two pins" "$(pin "$full")\n$(pin "$partial")"
expect fail "abbreviated pin" "$(pin "${full:0:7}")"
expect fail "pin not in the repository" "$(pin 0000000000000000000000000000000000000000)"
# Overrides: the engine's uncommitted edits and JETTISON_SOURCE_DIR are not the baseline.
echo '# uncommitted' >>"$engine/devenv.nix"
expect pass "engine working tree edited" "$(pin "$full")"
JETTISON_SOURCE_DIR="$root" expect fail "devenv.nix differs, JETTISON_SOURCE_DIR at matching files" \
  "$(pin "$full")" sh -c "echo '# drift' >>devenv.nix"

((failures == 0)) && echo "env-parity tests: all passed" || { echo "env-parity tests: $failures failed"; exit 1; }
