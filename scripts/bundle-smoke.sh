#!/usr/bin/env bash
# The distribution run as its operator runs it, every place left to its default
# (docs/host.md, "The distribution"): from an empty directory, the content found beside the executables,
# and the world, its backups, the token file, the server's key and the TUI's transcript under HOME's XDG
# directories, each made private. A start; the pack validated; an administrator's token issued and the
# server's key made; the server started on a new world, the TUI (when there is one) connected to it
# with the token, pinning the key from the player's server list, and the server stopped, which
# writes a snapshot and its backup; the backup restored into a new directory at the step the server
# stopped at.
# bundle-smoke.sh <prefix>, the directory holding bin/ and share/manifest/, a bundle's or a build
# tree's. HOME is made, and must be new or empty; the XDG variables are not read.
set -euo pipefail

prefix="$(cd "${1:?usage: bundle-smoke.sh <prefix>}" && pwd)"
bin="$prefix/bin"
pack="$prefix/share/manifest/manifest.pack"
unset XDG_DATA_HOME XDG_CONFIG_HOME XDG_STATE_HOME XDG_CACHE_HOME
logs="$(mktemp -d)"
empty="$(mktemp -d)"
server=
trap 'if [[ -n "$server" ]]; then kill -KILL "$server" 2>/dev/null || true; fi; rm -rf "$logs" "$empty"' EXIT

fail() {
  printf 'bundle-smoke: %s\n' "$1" >&2
  cat "$logs"/* >&2 2>/dev/null || true
  exit 1
}

[[ "${HOME:-}" == /* ]] || fail 'HOME must be an absolute path'
mkdir -p "$HOME"
[[ -z "$(ls -A "$HOME")" ]] || fail "HOME ($HOME) must be new or empty"
cd "$empty"

"$bin/manifest" --scenario trading --check-startup > "$logs/start" 2>&1 || fail 'it did not start'
grep -qF "ready, from $pack, scenario trading" "$logs/start" || fail 'it did not start on its own pack'

"$bin/manifest-validate" --scenario trading > "$logs/validate" 2>&1 || fail 'the pack is not valid'
grep -qF "mount 1: manifest, from $pack" "$logs/validate" || fail 'it did not validate its own pack'

token="$("$bin/manifest-token" issue administrator Operator)"
[[ -s "$HOME/.config/manifest/tokens" ]] || fail 'the token file is not where it belongs'
key="$("$bin/manifest-token" key)"
[[ "$(stat -c %a "$HOME/.config/manifest/server-key")" == 600 ]] || fail 'the key file is not private'
[[ -f "$prefix/share/manifest/servers.yaml" ]] || fail 'the server list is not beside the content'

"$bin/manifest" --scenario trading --serve > "$logs/server" 2>&1 &
server=$!
for _ in $(seq 100); do
  grep -q 'serving on 127.0.0.1:' "$logs/server" && break
  sleep 0.1
done
port="$(sed -nE 's/.*serving on 127\.0\.0\.1:([0-9]+) at step 0, world .*/\1/p' "$logs/server")"
[[ -n "$port" ]] || fail 'the server did not start on a new world'
grep -q "key $key$" "$logs/server" || fail 'the serving line does not name the public key'

if [[ -x "$bin/manifest_tui" ]]; then
  printf 'version: 1\nservers:\n  - endpoint: 127.0.0.1:%s\n    key: %s\n' "$port" "$key" \
    > "$HOME/.config/manifest/servers.yaml"
  # A terminal for it, typed into a line at a time, and quit with Ctrl-D.
  {
    sleep 3
    printf 'connect 127.0.0.1:%s %s\r' "$port" "$token"
    sleep 3
    printf 'status\r'
    sleep 1
    printf '\004'
    sleep 1
  } | script -qec "$bin/manifest_tui" /dev/null > "$logs/tui-screen" 2>&1 || fail 'the TUI did not quit cleanly'
  grep -qF "connected: to 127.0.0.1:$port as Operator" "$HOME/.local/state/manifest/tui-session.log" \
    || fail 'the TUI did not connect'
fi

kill -TERM "$server"
wait "$server" || fail "the server exited $?"
server=
stopped="$(sed -nE 's/.*stopping at step ([0-9]+), batch.*/\1/p' "$logs/server")"
[[ -n "$stopped" ]] || fail 'the server did not stop cleanly'
backup="$(find "$HOME/.local/share/manifest/backups" -mindepth 1 -maxdepth 1 | sort | tail -n 1)"
[[ -n "$backup" ]] || fail 'no backup was written'
"$bin/manifest" --scenario trading --restore "$backup" --world "$HOME/restored" > "$logs/restore" 2>&1 \
  || fail 'the backup did not restore'
grep -qE "restored world [0-9a-f]{32} at batch [0-9]+, step $stopped, into" "$logs/restore" \
  || fail 'the newest backup is not of the stop'

for directory in .local/share/manifest .config/manifest .local/state/manifest; do
  [[ ! -d "$HOME/$directory" || "$(stat -c %a "$HOME/$directory")" == 700 ]] \
    || fail "$HOME/$directory is not private"
done
[[ -z "$(ls -A "$empty")" ]] || fail 'something was written to the working directory'
echo "bundle-smoke: passed, from $prefix"
