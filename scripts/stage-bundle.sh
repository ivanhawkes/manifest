#!/usr/bin/env bash
# Stages the distribution (docs/host.md, "The distribution") from a clean checkout, so that its game
# tree names a commit: the release build, linked to run outside its toolchain (cmake/Distribution.cmake)
# and checked so (check-distribution.sh), laid out with its content, its BUILD and the licence of every
# source tree compiled into it, and packed as manifest-<version>-<game tree>-linux-x86_64.tar.gz in the
# output directory, which it names.
# stage-bundle.sh [<output directory>], build/bundle by default.
set -euo pipefail
cd "$(dirname "$0")/.."

out="$(realpath -m "${1:-build/bundle}")"
if [[ -n "$(git status --porcelain)" ]]; then
  echo 'stage-bundle: the checkout has changes, and a bundle is staged from a commit' >&2
  exit 1
fi
preset=clang-release
build="build/$preset"
binaries=(bin/manifest bin/manifest-token bin/manifest-validate bin/manifest_tui)
cmake --preset "$preset" > /dev/null
cmake --build --preset "$preset" --target manifest manifest-token manifest-validate manifest_tui \
  manifest_validate_pack
./scripts/check-distribution.sh "${binaries[@]/#/$build/}"

# The identity a save of the build holds (D-111), as the build wrote it.
revision="$build/source/game/host/Revision.cpp"
game="$(sed -nE 's/.*kGameRevision = "(.*)";/\1/p' "$revision")"
sdk="$(sed -nE 's/.*kSdkRevision = "(.*)";/\1/p' "$revision")"
version="${game%%+*}"
tree="${game#*+}"
[[ -n "$version" && "$tree" =~ ^[0-9a-f]{40}$ && "$sdk" =~ ^[0-9a-f]{40}$ ]] \
  || { echo "stage-bundle: no revision in $revision" >&2; exit 1; }

name="manifest-$version-${tree:0:12}-linux-x86_64"
stage="$out/$name"
rm -rf "$stage" "$out/$name.tar.gz"
mkdir -p "$stage/bin" "$stage/share/manifest" "$stage/share/doc/manifest"
install -m 0755 "${binaries[@]/#/$build/}" "$stage/bin/"
install -m 0644 "$build/share/manifest/manifest.pack" "$build/share/manifest/servers.yaml" \
  "$stage/share/manifest/"

{
  echo "version: $version"
  echo "game: $game"
  echo "sdk: $sdk"
  echo 'content:'
  for scenario in data/scenarios/*.yaml; do
    scenario="$(basename "$scenario" .yaml)"
    fingerprint="$("$stage/bin/manifest-validate" --scenario "$scenario" 2>&1 |
      sed -nE 's/.*valid: scenario .*, content ([0-9a-f]+)$/\1/p')"
    [[ -n "$fingerprint" ]] || { echo "stage-bundle: $scenario does not validate" >&2; exit 1; }
    echo "  $scenario: $fingerprint"
  done
} > "$stage/share/manifest/BUILD"

# Every fetched source tree an object of a binary was compiled from or included a header of, from
# ninja's own record of the build, and the engine's and Manifest's.
licences="$stage/share/doc/manifest"
install -m 0644 LICENSE "$licences/LICENSE"
mapfile -t objects < <(ninja -C "$build" -t inputs "${binaries[@]}" | grep -E '\.o$')
mapfile -t trees < <({ printf '%s\n' "${objects[@]}"; ninja -C "$build" -t deps "${objects[@]}"; } |
  grep -oE '_deps/[^/]+-(src|build)/' | sed -E 's#_deps/(.+)-(src|build)/#\1#' | sort -u)
# What runs in a process substitution fails without stopping the script: the engine is always one.
[[ " ${trees[*]} " == *' jettison '* ]] || { echo 'stage-bundle: no source trees were found' >&2; exit 1; }
# A tree whose licence is a source file's leading comment, and that has no licence file.
declare -A licensed_in=([mikktspace]=mikktspace.h)
for tree in "${trees[@]}"; do
  if [[ -n "${licensed_in[$tree]:-}" ]]; then
    files=("$build/_deps/$tree-src/${licensed_in[$tree]}")
  else
    mapfile -t files < <(find "$build/_deps/$tree-src" -maxdepth 1 -type f \
      \( -iname 'licen[cs]e*' -o -iname 'copying*' -o -iname 'notice*' \))
  fi
  ((${#files[@]})) || { echo "stage-bundle: $tree has no licence file" >&2; exit 1; }
  mkdir -p "$licences/$tree"
  install -m 0644 "${files[@]}" "$licences/$tree/"
done

# The font in the pack is no source tree compiled in: Noto Sans JP's licence (SIL OFL 1.1), fetched with
# it by source/external, travels with the pack.
font_licence="$build/_deps/notosansjp-licence-src/LICENSE"
if [[ -f "$font_licence" ]]; then
  mkdir -p "$licences/notosansjp"
  install -m 0644 "$font_licence" "$licences/notosansjp/"
fi

# The same bytes from the same commit: no owners, times or order of the machine's.
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$(git log -1 --format=%ct)" \
  -C "$out" -cf - "$name" | gzip -n > "$out/$name.tar.gz"
echo "$out/$name.tar.gz"
