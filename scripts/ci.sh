#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
werror=OFF
asan=false
release=true
log=''
tidy=false
force=false
# Keep local builds modest; CI can override these limits.
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
export CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-2}"
while (($#)); do
  case "$1" in
    --werror) werror=ON ;;
    --asan) asan=true ;;
    --tidy) tidy=true ;;
    --skip-release) release=false ;;
    --force) force=true ;;
    --log) shift; log="${1:?--log requires a path}" ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done
if [[ -n "$log" ]]; then
  mkdir -p "$(dirname "$log")"
  exec > >(tee "$log") 2>&1
fi

# A gate that passed is not run again on the same inputs (--force runs it): the working tree's files,
# tracked or untracked and not ignored, *.md aside (no stage reads them), the toolchain and the
# options, read through a scratch copy of the index. The engine is in it by its pin; a preset built
# against a local engine (JETTISON_SOURCE_DIR) is not, so such a gate always runs and is not kept.
gate_key() {
  local index tree
  if grep -qs '^JETTISON_SOURCE_DIR:PATH=.' build/clang-{debug,release,asan}/CMakeCache.txt; then
    echo local-engine
    return
  fi
  index="$(mktemp "${TMPDIR:-/tmp}/manifest-gate-index.XXXXXX")"
  cp "$(git rev-parse --git-path index)" "$index"
  GIT_INDEX_FILE="$index" git add -A
  tree="$(GIT_INDEX_FILE="$index" git write-tree)"
  rm -f "$index"
  {
    git ls-tree -r "$tree" | grep -v '\.md$'
    clang --version
    cmake --version
    echo "$werror $asan $release $tidy"
  } | sha256sum | cut -d' ' -f1
}
gate_passed=build/gate-passed
key="$(gate_key)"
if ! "$force" && [[ "$key" != local-engine && -e "$gate_passed/$key" ]]; then
  echo "gate passed: the inputs are those of the gate passed $(date -r "$gate_passed/$key" '+%F %T') (--force runs it)"
  exit 0
fi
./scripts/format-check.sh
build_and_test() {
  local preset="$1"
  cmake --preset "$preset" -DMANIFEST_WERROR="$werror"
  cmake --build --preset "$preset"
  ctest --preset "$preset"
}
build_and_test clang-debug
if "$release"; then
  build_and_test clang-release
fi
if "$asan"; then
  build_and_test clang-asan
fi
# Only Manifest translation units, using the SDK's compile commands and tidy rules. Last, so a
# finding fails the gate without hiding the results of the build and test stages before it.
if "$tidy"; then
  python3 scripts/tidy.py build/clang-debug
fi
if [[ "$key" == local-engine ]]; then
  echo "gate passed, against a local engine: not recorded as passed"
elif [[ "$(gate_key)" == "$key" ]]; then
  mkdir -p "$gate_passed" && touch "$gate_passed/$key"
  echo "gate passed"
else
  echo "gate passed, on inputs that changed while it ran: not recorded as passed"
fi
