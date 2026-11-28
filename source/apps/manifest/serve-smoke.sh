#!/usr/bin/env bash
# The dedicated server unattended, killed and stopped: the server's key made private, refused a
# replacement unasked and replaced when asked; an administrator's token and a key issued with only
# XDG_CONFIG_HOME set; no server started on a key file missing or holding no key; the server
# started on a new world and an ephemeral port with no client, flushing its journal every second and
# writing a snapshot every two, left to step and killed outright; started again on the world, it takes
# up at a step its journal had flushed, steps on and is stopped with SIGINT, which writes a snapshot
# and its backup; the backup restores into a new directory at the step it stopped at, and not over the
# world; started a third time, it is at the step it stopped at. Each start names the key's public half.
# serve-smoke.sh <manifest> <manifest-token> <content directory>
set -euo pipefail

work="$(mktemp -d)"
# A crash report goes under the run's own state directory, never the real home's.
export XDG_STATE_HOME="$work/state"
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
    --tokens "$work/tokens.yaml" --key "$work/server-key" --port 0 --flush-seconds 1 --snapshot-seconds 2 --backups "$work/backups" > "$work/log" 2>&1 &
  server=$!
  for _ in $(seq 100); do
    grep -q 'serving on 127.0.0.1:' "$work/log" && break
    sleep 0.1
  done
  at="$(sed -nE 's/.*serving on [^ ]+ at step ([0-9]+), world [0-9a-f]{32} at batch [0-9]+, key [0-9a-f]{64}$/\1/p' "$work/log")"
  [[ -n "$at" ]] || fail 'the server did not start'
  grep -q "key $public$" "$work/log" || fail 'the serving line does not name the public key'
}

"$2" --tokens "$work/tokens.yaml" issue administrator Operator > /dev/null
public="$("$2" --key "$work/server-key" key)"
[[ "$public" =~ ^[0-9a-f]{64}$ ]] || fail 'no public key was printed'
[[ "$(stat -c %a "$work/server-key")" == 600 ]] || fail 'the key file is not private'
if "$2" --key "$work/server-key" key > "$work/log" 2>&1; then
  fail 'a key was replaced unasked'
fi
grep -q 'already holds a key' "$work/log" || fail 'a key already there was not refused for it'
replaced="$("$2" --key "$work/server-key" key --replace)"
[[ "$replaced" =~ ^[0-9a-f]{64}$ && "$replaced" != "$public" ]] || fail 'the key was not replaced'
public="$replaced"
# With nothing in the environment but the one directory it writes to.
env -i XDG_CONFIG_HOME="$work/config" "$2" issue administrator Operator > /dev/null \
  || fail 'a token was not issued with XDG_CONFIG_HOME alone'
[[ -f "$work/config/manifest/tokens" ]] || fail 'the token file is not under XDG_CONFIG_HOME'
env -i XDG_CONFIG_HOME="$work/config" "$2" key > /dev/null || fail 'no key was made with XDG_CONFIG_HOME alone'
[[ -f "$work/config/manifest/server-key" ]] || fail 'the key file is not under XDG_CONFIG_HOME'

# Refuses to serve, naming the file, on a key file missing and on one holding no key.
refused() {
  if "$1" --content "$3" --scenario trading --serve --world "$work/refused" --tokens "$work/tokens.yaml" \
    --key "$4" --backups "$work/backups" > "$work/log" 2>&1; then
    fail "a server started on $4"
  fi
  grep -qF "the server key file $4 $5" "$work/log" || fail "a server on $4 was not refused for it"
}
refused "$@" "$work/none" 'cannot be read'
refused "$@" "$work/tokens.yaml" 'holds no secret key'
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
