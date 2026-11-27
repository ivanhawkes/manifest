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
[the graphical client](client.md)), on a local world. What it still lacks is in the
[roadmap](roadmap.md#the-graphical-client): server play, the galaxy panel, the shipyard and fleet
screens, names, Japanese glyphs ([languages](client.md#languages)), and the looks. The
owners' screens are [L15b](delivery-order.md#l15b), which waits on [L04](delivery-order.md#l04),
[L05](delivery-order.md#l05) and [L29](delivery-order.md#l29); L29's open choices are settled
([D-120](decision-ledger.md#d-120)). The pin is Jettison's , with its server pack, client-side mods and world content (J63), and the content mounted as .165fada`. The queue's first row, the
drives past the torch ([W22](delivery-order.md#w22)), waits for the first larger hull. Saves still
run on the step's thread ([L40](delivery-order.md#l40), [D-115](decision-ledger.md#d-115)).
