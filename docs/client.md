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

### The model

`Manifest::Client::ClientModel` (`source/apps/client`, the `client_model` module) is the player's
side of a local world with nothing of the UI in it, so its suite plays the first loop with no window.
It holds the player's principal and makes a `Screen` from `Query::Observe` as that player's actor:
the company and its credits, its ships, holdings and resting orders, the market where the ship is
docked, and the venues the player has a report of, as destinations. Its actions are the console's
payloads, each admitted through `Command::Requests::Admit` with the player's next sequence:
provisioning (as the trusted harness, as the console's host provisions), a tote bought at the best
ask or sold at the best bid, an order cancelled, a good's totes loaded or unloaded, a journey, the life
support's refill and replacement, and refuelling. It keeps the last request's answer, or that one is
still to come; an action with nothing to act on, such as a purchase with no ship docked, is refused
before anything is sent.

The screens act on the company's first ship, the only one the first loop has. A journey's thrust is
set in percent of its cruise and its acceleration in tenths of g ([D-107](decision-ledger.md#d-107)),
kept within 0.1 g to 3 g; the world refuses more than the hull sustains.

**Fast-forward.** While a journey is under way, `ClientModel::FastForward(budget)` steps the world on
with `SimWorld::FastForward`, its quiet steps passed over, for at most the budget and no further than
the journey's end. The client calls it once a frame, so a journey's days pass in seconds and the loop
never waits on them.

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
- `source/apps/client/include/manifest/client/ClientModel.h`: the model; its suite is
  `tests/ClientModelTests.cpp`.
- `source/game/host/src/Start.cpp`: `MakeGame` and `HostedServices`.
