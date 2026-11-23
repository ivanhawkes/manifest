#!/usr/bin/env bash
# The dedicated server unattended, and killed: an administrator's token issued, the server started on
# a new world and an ephemeral port with no client, left to step and killed outright; started again on
# the world, it takes up at the steps its journal had committed, steps on, is stopped with SIGINT and
# is gone cleanly.
# serve-smoke.sh <manifest> <manifest-token> <content directory>
set -euo pipefail

work="$(mktemp -d)"
server=
trap 'if [[ -n "$server" ]]; then kill -KILL "$server" 2>/dev/null || true; fi; rm -rf "$work"' EXIT

fail() {
  printf 'serve-smoke: %s\n' "$1" >&2
  cat "$work/log" >&2
  exit 1
}

serve() {
  "$1" --content "$3" --scenario trading --serve --world "$work/world" \
    --tokens "$work/tokens.yaml" --port 0 > "$work/log" 2>&1 &
  server=$!
  for _ in $(seq 100); do
    grep -q 'serving on 127.0.0.1:' "$work/log" && return
    sleep 0.1
  done
  fail 'the server did not start'
}

"$2" --tokens "$work/tokens.yaml" issue administrator Operator > /dev/null
serve "$@"
grep -q 'at step 0, world [0-9a-f]\{32\} at batch 0$' "$work/log" || fail 'the world was not new'
# Past two seconds the world has stepped with nobody connected, each step committed first.
sleep 2.2
kill -KILL "$server"
wait "$server" 2>/dev/null || true

serve "$@"
resumed="$(sed -nE 's/.*serving on [^ ]+ at step ([0-9]+), world.*/\1/p' "$work/log")"
[[ "$resumed" -ge 2 ]] || fail 'the killed server'"'"'s committed steps were not taken again'
sleep 1.2
kill -INT "$server"
wait "$server" || fail "the server exited $?"
server=
stopped="$(sed -nE 's/.*stopping at step ([0-9]+), batch.*/\1/p' "$work/log")"
[[ -n "$stopped" && "$stopped" -gt "$resumed" ]] || fail 'the world did not step on from its journal'
