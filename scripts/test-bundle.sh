#!/usr/bin/env bash
# Tests a staged bundle (stage-bundle.sh) where it is to run: unpacked into a read-only directory
# outside both checkouts and run as its operator runs it (bundle-smoke.sh), with nothing but HOME and
# PATH, outside the development shell; on this machine, and in containers of the oldest systems the
# runtime distribution contract supports (docs/host.md, "The distribution"), which have nothing of the
# build's.
# test-bundle.sh <bundle.tar.gz> [host|<docker image>]..., host, ubuntu:24.04 and debian:13 by default.
set -euo pipefail

tarball="$(realpath "${1:?usage: test-bundle.sh <bundle.tar.gz> [host|<image>]...}")"
shift
targets=("$@")
((${#targets[@]})) || targets=(host ubuntu:24.04 debian:13)
work="$(mktemp -d)"
trap 'chmod -R u+w "$work"; rm -rf "$work"' EXIT
tar -xzf "$tarball" -C "$work"
bundle="$(find "$work" -mindepth 1 -maxdepth 1 -type d)"
install -m 0444 "$(dirname "$0")/bundle-smoke.sh" "$work/bundle-smoke.sh"
chmod -R a-w "$bundle"

for target in "${targets[@]}"; do
  echo "test-bundle: $target"
  if [[ "$target" == host ]]; then
    # The system's PATH: NixOS's profile, or a conventional one.
    env -i "HOME=$work/home" PATH=/run/current-system/sw/bin:/usr/bin:/bin \
      bash "$work/bundle-smoke.sh" "$bundle"
  else
    docker run --rm --network none -v "$bundle:/opt/manifest:ro" \
      -v "$work/bundle-smoke.sh:/opt/bundle-smoke.sh:ro" "$target" \
      env -i HOME=/tmp/home PATH=/usr/bin:/bin bash /opt/bundle-smoke.sh /opt/manifest
  fi
done
