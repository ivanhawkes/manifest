#!/usr/bin/env bash
# Server play end to end (docs/client.md, "Server play"): a key, a principal's token and a server list
# written in a new directory, the dedicated server started on a new trading world and an ephemeral
# port, and the graphical client connected to it offscreen on lavapipe, which the engine's headless
# helper finds: no display, no window on any desktop. Driven by --run lines, the client founds the
# player's company and names its ship, reads the ship's name, buys a tote on the market screen and
# reads it held there. The server is stopped with SIGINT and started again on its world, and a second
# run of the client reads the ship's name and the tote still held.
# It reads what the screens hold (the client's read), not pixels.
# Usage: client-serve.sh <manifest_client> <manifest> <manifest-token> <content directory>
#   <jettison>/scripts/lib/headless.sh
set -euo pipefail
client="$1"
manifest="$2"
token_cli="$3"
content="$4"
# shellcheck source=/dev/null
source "$5"
headless_use_lavapipe

work="$(mktemp -d)"
# A crash report goes under the run's own state directory, never the real home's.
export XDG_STATE_HOME="$work/state"
server=
trap 'if [[ -n "$server" ]]; then kill -KILL "$server" 2>/dev/null || true; fi; rm -rf "$work"' EXIT

fail() {
  printf 'client-serve: %s\n' "$1" >&2
  for log in "$work"/*.log; do
    printf '== %s\n' "$log" >&2
    tail -n 60 "$log" >&2
  done
  exit 1
}

# Starts the server on the world and pins its key, at the port it took, in the player's server list.
serve() {
  "$manifest" --content "$content" --scenario trading --serve --world "$work/world" \
    --tokens "$work/tokens.yaml" --key "$work/server-key" --port 0 --backups "$work/backups" \
    > "$work/server.log" 2>&1 &
  server=$!
  for _ in $(seq 100); do
    grep -q 'serving on 127.0.0.1:' "$work/server.log" && break
    sleep 0.1
  done
  port="$(sed -nE 's/.*serving on 127\.0\.0\.1:([0-9]+) .*/\1/p' "$work/server.log")"
  [[ -n "$port" ]] || fail 'the server did not start'
  mkdir -p "$work/config/manifest"
  printf 'version: 1\nservers:\n  - endpoint: 127.0.0.1:%s\n    key: %s\n' "$port" "$public" \
    > "$work/config/manifest/servers.yaml"
}

stop() {
  kill -INT "$server"
  wait "$server" || fail "the server exited $?"
  server=
}

# The client connected as the token's player, its --run lines given; its log is $work/<name>.log.
play() {
  local name="$1"
  shift
  env -u DISPLAY -u WAYLAND_DISPLAY XDG_CONFIG_HOME="$work/config" "$client" --offscreen \
    --gpu llvmpipe --no-sound --user-dir "$work/user" --connect "127.0.0.1:$port" --token-file "$work/token" \
    "$@" > "$work/$name.log" 2>&1 || fail "the client's $name run exited $?"
}

public="$("$token_cli" --key "$work/server-key" key)"
(umask 077 && "$token_cli" --tokens "$work/tokens.yaml" issue principal Ada > "$work/token")

serve
# A snapshot comes at most once a second, and the first after the founding has been seen to take
# four, so each read waits well past the answer.
play first --run '@2s found "Ada Freight" "Long Shot"' --run '@12s read company-name' \
  --run '@12s read ship-0' --run '@12s screen market' \
  --run '@12s press buy-0' --run '@18s read held-0' --run '@18s screen company' \
  --run "@18s screenshot $work/company.png" --run '@19s quit'
grep -q 'company company-name: Ada Freight' "$work/first.log" || fail 'the company was not founded'
grep -q 'company ship-0: Long Shot$' "$work/first.log" || fail 'the ship is not shown by its name'
grep -q 'market held-0: 1$' "$work/first.log" || fail 'the tote bought is not held'
[[ -s "$work/company.png" ]] || fail 'no screenshot was written'
stop

serve
play second --run '@8s read ship-0' --run '@8s screen market' --run '@8s read held-0' --run '@9s quit'
grep -q 'market held-0: 1$' "$work/second.log" || fail 'the tote is not held after the restart'
grep -q 'company ship-0: Long Shot$' "$work/second.log" || fail 'the ship lost its name in the restart'
stop
