# Session handoff

## Next, on resuming — 2026-11-19 (after A06)

A07, as `delivery-order.md` has it: close Stage 3, proving authorisation, conservation, rejection
atomicity, sequencing and retries, and in-memory save and replay with the TUI off, then hand stage 4
the order, travel and upgrade contracts. A06 is done
([history](history.md#a06-the-console-apart-from-ftxui--2026-11-19)): the console builds and tests
with the TUI off and drives one hosted world through admission and queries, beside its sandbox. The
company tests still read the registry: A07 moves their player-visible checks onto `Query::Observe`
(credits, venue stock and reservations, cargo, dock, and that another's or a rejected command changed
nothing visible). They keep direct registry reads only for what the snapshot leaves out: identity
counters, which rejection atomicity needs, the provisioned `Polity` and region, and regional accounts.
Fixture writes such as `Stock(...).holdings` stay, as controlled bootstrap. Stage 3 closing is the point for the cross-review of its
commits (A02 to A07). Still for later: before W02, check the C01 table and
`data/tuning/balance.yaml` against the plans they derive from.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
