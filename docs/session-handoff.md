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

The user plays the first loop in the graphical client ([L15a](delivery-order.md#l15a)), not the
terminal UI, which is mainly for administration (the user's, 2026-11-27). L15a stands on Jettison's
J30, done 2026-11-27 (the shell starts a `Host::Game` through a session); the owners' screens were
split off as [L15b](delivery-order.md#l15b) on 2026-11-27, which waits on [L04](delivery-order.md#l04),
[L05](delivery-order.md#l05) and [L29](delivery-order.md#l29). L29's open choices are settled
([D-120](decision-ledger.md#d-120)). The queue's first row, the drives past
the torch ([W22](delivery-order.md#w22)), waits for the first larger hull. Saves still run on the
step's thread ([L40](delivery-order.md#l40), [D-115](decision-ledger.md#d-115)).
