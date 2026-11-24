#!/usr/bin/env bash
# Checks that each executable runs outside the toolchain that built it, as the runtime distribution
# contract has it (docs/stage-5-recovery-plan.md): loaded through the system's loader, with no
# RUNPATH, needing only glibc's own libraries, and no symbol of a glibc newer than the floor.
# check-distribution.sh <executable>...
set -euo pipefail

loader=/lib64/ld-linux-x86-64.so.2
floor=2.39
# glibc's own, the loader among them, which the static libstdc++ asks for by name.
allowed=' libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 ld-linux-x86-64.so.2 '

(($#)) || { echo 'usage: check-distribution.sh <executable>...' >&2; exit 2; }
failed=0
fail() {
  printf '%s: %s\n' "$1" "$2" >&2
  failed=1
}

for binary in "$@"; do
  name="$(basename "$binary")"
  interpreter="$(readelf -p .interp "$binary" 2>/dev/null | sed -nE 's/^ *\[ *0\] *//p')"
  [[ "$interpreter" == "$loader" ]] || fail "$name" "its interpreter is '$interpreter', not $loader"
  dynamic="$(readelf -d "$binary")"
  if grep -qE '\((RPATH|RUNPATH)\)' <<< "$dynamic"; then
    fail "$name" "it has a search path: $(grep -E '\((RPATH|RUNPATH)\)' <<< "$dynamic")"
  fi
  while read -r library; do
    [[ "$allowed" == *" $library "* ]] || fail "$name" "it needs $library"
  done < <(sed -nE 's/.*\(NEEDED\).*\[(.*)\]/\1/p' <<< "$dynamic")
  # Each versioned glibc symbol it imports, the newest first.
  newest="$(objdump -T "$binary" | sed -nE 's/.*[ (]GLIBC_([0-9.]+)\)? +(\S+)$/\1 \2/p' | sort -rV)"
  while read -r version symbol; do
    [[ "$(printf '%s\n' "$version" "$floor" | sort -V | tail -n 1)" == "$floor" ]] && break
    fail "$name" "$symbol needs glibc $version, newer than $floor (source/apps/distribution)"
  done <<< "$newest"
  printf '%s: glibc %s at most\n' "$name" "$(head -n 1 <<< "$newest" | cut -d' ' -f1)"
done
exit "$failed"
