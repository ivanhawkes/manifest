# The graphical client

The graphical client, `manifest_client`, is how most players will play
([L15a](delivery-order.md#l15a)). It is Jettison's rendered [shell](../../jettison/docs/glossary.md#shell)
with Manifest composed into it: the same `Host::Game` the server and the tests start, and the same
commands and queries as the [terminal UI](console.md).

## How it works

### Composition

`Manifest::Host::MakeGame(scenario)` makes Manifest's `Host::Game`, the one `Host::Start` composes
for the server. The client hands it to `Jettison::App::RunRenderApplication`, and the shell starts
it through a session of its own ([Jettison's J30](../../jettison/docs/history/neutral-sdk.md#j30-shell-session)).
The client keeps a reference to the game, so `Host::HostedServices(game)` reaches the world's
identities and requests wherever the session is held.

The shell mounts the engine's assets first, as `engine`, then Manifest's content as `base`, then
the user's mods and the user's directory. The Manifest package is therefore the first source that is
not the engine's, and the content phase checks it as it checks the server's first source.

`--scenario <name>` is the client's own and is taken off the command line before the shell reads
the rest; with none it plays `trading`. Every other argument is the shell's
([the app shell](../../jettison/docs/app-shell.md)): `--content` names other content, and
`--offscreen`, `--screenshot` and `--user-dir` work as they do for the engine's programs.

## Using it

```bash
build/clang-debug/source/apps/client/manifest_client               # the trading scenario
build/clang-debug/source/apps/client/manifest_client --scenario governance
```

It is built in every preset with the engine's presentation (`MANIFEST_BUILD_CLIENT`, off in
`clang-notui`).

## Limits and open work

- The content mounts as `base`, not `manifest`, so an overlay that `requires: manifest` does not
  mount in the client.
- It is not in the [bundle](glossary.md#bundle) yet, and plays only a local world, not a server's.
- What is still to come is listed in [the graphical client](roadmap.md#the-graphical-client).

## Source

- `source/apps/client/main.cpp`: the program.
- `source/game/host/src/Start.cpp`: `MakeGame` and `HostedServices`.
