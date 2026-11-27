#!/usr/bin/env bash
# The engine's render test sequence played by the graphical client (docs/client.md), offscreen on
# lavapipe, which the engine's headless helper finds: no display, no window on any desktop. Its game
# UI stage provisions the player and buys a tote through the screens.
# Usage: client-smoke.sh <manifest_client> <jettison>/scripts/lib/headless.sh <user directory>
set -euo pipefail
client="$1"
helper="$2"
user="$3"
# shellcheck source=/dev/null
source "$helper"
headless_use_lavapipe
rm -rf "$user"
exec env -u DISPLAY -u WAYLAND_DISPLAY "$client" --smoke-test --offscreen --gpu llvmpipe --user-dir "$user"
