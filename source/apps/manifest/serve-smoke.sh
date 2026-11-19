#!/usr/bin/env bash
# The dedicated server unattended: an administrator's token issued, the server started on an
# ephemeral port with no client, left to step, stopped with SIGINT and gone cleanly, having stepped.
# serve-smoke.sh <manifest> <manifest-token> <content directory>
set -euo pipefail

work="$(mktemp -d)"
server=
trap 'if [[ -n "$server" ]]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$work"' EXIT

"$2" --tokens "$work/tokens.yaml" issue administrator Operator > /dev/null
"$1" --content "$3" --scenario trading --serve --tokens "$work/tokens.yaml" --port 0 \
  > "$work/log" 2>&1 &
server=$!

for _ in $(seq 50); do
  grep -q 'serving on 127.0.0.1:' "$work/log" && break
  sleep 0.1
done
grep -q 'serving on 127.0.0.1:' "$work/log" || { cat "$work/log" >&2; exit 1; }
# One-second steps: past two seconds the world has stepped with nobody connected.
sleep 2.2
kill -INT "$server"
wait "$server" || { printf 'serve-smoke: the server exited %s\n' "$?" >&2; cat "$work/log" >&2; exit 1; }
if ! grep -Eq 'stopping at step ([2-9]|[1-9][0-9]+)$' "$work/log"; then
  printf 'serve-smoke: the world did not step unattended:\n' >&2
  cat "$work/log" >&2
  exit 1
fi
