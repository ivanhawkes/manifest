# Session handoff

Where Manifest stands and what comes next. How each task went is in [history](history.md); the
open work, in order, is the [task queue](delivery-order.md).

## Where things stand — 2026-11-25

- All five [stages](glossary.md#stage) are closed. The last, persistence, recovery and the
  distribution, closed on 2026-11-25, and its cross-review ended after round 6
  ([history](history/stage-5.md#stage-5-cross-review--2026-11-25)).
- The first loop runs: a [dedicated server](server.md) over a journalled world, the TUI as its
  client, and the bundle staged and tested on Ubuntu 24.04 and Debian 13 ([host](host.md)).
- CI stages and smoke-tests the bundle; it is not the gate ([D-116](decision-ledger.md#d-116)).
  The gate runs locally before every push ([build and gates](build-and-gates.md#the-gate)).
- The documents were restructured on the `docs-review` branch: one document per feature, a
  glossary, and history as a folder.

## Next

The first loop plays in the graphical client, `manifest_client` (L15a, done 2026-11-27,
[the graphical client](client.md)), on a local world or, since L52 (2026-11-28), on a dedicated
server. What it still lacks is in the [roadmap](roadmap.md#the-graphical-client): the galaxy panel, the shipyard and fleet
screens, names, and the looks. The
owners' screens are [L15b](delivery-order.md#l15b), which waits on [L04](delivery-order.md#l04) and
[L05](delivery-order.md#l05); the regions' tax rates it edits are in (L29, 2026-11-28). The pin is Jettison's `b8134e3`: numbers, dates and times in the language's form, which the client's screens use (J15, [languages](client.md#languages)); the mods' scripts in the host session, which the server runs and saves ([J62](server.md#the-servers-loop)); and a scene held behind its fade until its models load (J13), which the client has no scene change to use yet. The drives past the torch
are done ([W22b](history/tasks.md#w22b), 2026-11-27): the pulse carrier, its dry docks, licence and
override, and the freighter granted. Saves still
run on the step's thread ([L40](delivery-order.md#l40), [D-115](decision-ledger.md#d-115)).
