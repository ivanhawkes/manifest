#!/usr/bin/env bash
# The dedicated server unattended, killed and stopped: an administrator's token issued, the server
# started on a new world and an ephemeral port with no client, flushing its journal every second and
# writing a snapshot every two, left to step and killed outright; started again on the world, it takes
# up at a step its journal had flushed, steps on and is stopped with SIGINT, which writes a snapshot
# and its backup; the backup restores into a new directory at the step it stopped at, and not over the
# world; started a third time, it is at the step it stopped at.
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

# Starts the server and gives the step it serves from.
serve() {
  "$1" --content "$3" --scenario trading --serve --world "$work/world" \
    --tokens "$work/tokens.yaml" --port 0 --flush-seconds 1 --snapshot-seconds 2 --backups "$work/backups" > "$work/log" 2>&1 &
  server=$!
  for _ in $(seq 100); do
    grep -q 'serving on 127.0.0.1:' "$work/log" && break
    sleep 0.1
  done
  at="$(sed -nE 's/.*serving on [^ ]+ at step ([0-9]+), world [0-9a-f]{32} at batch [0-9]+$/\1/p' "$work/log")"
  [[ -n "$at" ]] || fail 'the server did not start'
}

"$2" --tokens "$work/tokens.yaml" issue administrator Operator > /dev/null
serve "$@"
[[ "$at" -eq 0 ]] || fail 'the world was not new'
# Past three seconds the world has stepped with nobody connected, flushed and written a snapshot.
sleep 3.4
kill -KILL "$server"
wait "$server" 2>/dev/null || true
[[ -n "$(ls "$work/world/snapshots")" ]] || fail 'no snapshot was written'

serve "$@"
# A flush at the least, and no more steps than the time it ran for.
[[ "$at" -ge 60 && "$at" -le 240 ]] || fail 'the killed server did not take up at a flushed step'
sleep 1.2
kill -INT "$server"
wait "$server" || fail "the server exited $?"
stopped="$(sed -nE 's/.*stopping at step ([0-9]+), batch.*/\1/p' "$work/log")"
[[ -n "$stopped" && "$stopped" -gt "$at" ]] || fail 'the world did not step on from its journal'

backup="$(find "$work/backups" -mindepth 1 -maxdepth 1 | sort | tail -n 1)"
[[ -n "$backup" ]] || fail 'no backup was written'
"$1" --content "$3" --scenario trading --restore "$backup" --world "$work/restored" > "$work/log" 2>&1 \
  || fail 'the backup did not restore'
grep -qE "restored world [0-9a-f]{32} at batch [0-9]+, step $stopped, into" "$work/log" \
  || fail 'the newest backup is not of the stop'
if "$1" --content "$3" --scenario trading --restore "$backup" --world "$work/world" > "$work/log" 2>&1; then
  fail 'a backup was restored over a world'
fi
grep -q 'is not empty' "$work/log" || fail 'a restore over a world was not refused for it'

serve "$@"
[[ "$at" -eq "$stopped" ]] || fail 'a clean stop lost steps'
kill -INT "$server"
wait "$server" || fail "the server exited $?"
server=
