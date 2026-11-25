# The network and Stage 4: the trading loop (2026-11-20 to 11-23)

Task records as they were written at the time, newest first. Moved from `docs/history.md` on
2026-11-25; the [history index](../history.md) lists every record.

## Stage 4 cross-review — 2026-11-23

Codex reviewed `78615f9..175f0ad` in two rounds (`build/reviews/stage-4`). Its first attempt was cut
off by the service's filter before it wrote a review, an unchanged retry running through; the leads it
had left in its scratch, and a sweep of the stage's modules the user asked for, were fixed first
(`cfaef5a`): a fill dearer than its escrow aborted settlement, a delivery's payment and procurement's
other credits were unchecked, `Owed` and a bond's forfeit overflowed for large tenders, a region
could load with two projects under way, and a policy's floor below 0, near-limit sums and casts, and
content's turn, tax, funds and totes were unbounded. Round 1's five findings (`830f217`): a policy
estimated refuelling as tanks filled to the brim, so a laden ship chose a burn whole totes could not
fly; a save loaded with fees of no pad class, a tax carry past a withheld credit or an opened tote of
a million kilograms (`Identities::SetCharges`, `SetTotes`); and the fixtures' goods ledger let any
item fall in a consuming step, now exact against `Trade::Consumed`, which every sink raises. The
marketplace's order life and fee bps and arrears starting after the clock were bounded as the user
asked. Round 2 found each resolved and two more, fixed after it and not reviewed again: a market with
no `Fees` at all loaded, and the ledger skipped an item held nowhere before a batch delivered it.
A third round the user asked for, aimed at what the first two had not examined, found four: the
wire left out ships' policies and jobs, companies' bids and the tenders, so a remote client saw none
of them; refuelling opened a job's lot of pellets for the magazine without the job knowing, so it
blocked trying to unload what was gone (`World::LeaveLot` now shrinks the lot, as a seizure does);
and a job bid on contracts it could not deliver within their window, defaulting on its bond. Those
three are fixed. The fourth, a snapshot longer than a network packet dropped (the trading scenario's
is 63,567 bytes of 65,536), the user booked for Jettison's next change, a message split across
packets ([Jettison's roadmap](../../../jettison/docs/roadmap.md)),
and delta snapshots in Manifest only if that is not enough.

## W14: Stage 4 closed — 2026-11-23

**Stage 4 is closed** ([delivered](stage-4-trading-plan.md#delivered-w14-2026-11-23)), each of its
checks held by a gated test, with no engine edit on pin `14fc23a`. The four strands are proved
together in `fixture_development`: the late joiner trades by hand in her own view of the markets
beside Ada's automated homeworld, and the world is saved with Ada's journey and the joiner's bid open
and continued from the pack to the same hash and views. The first save point fell with Ada's ship
docked, so the fixture now passes over a minute at a time until her job's journey is under way. The
step's table lacked `manifest.policy` (W10), which the `travel` suite's pin had all along; the
table is now the order the stage closed on, and Stage 5's handoff names where each contract is.

## W13: regional governance — 2026-11-23

In six slices with no engine edit, on [D-110](../decision-ledger.md#d-110) and the
[plan](stage-4-trading-plan.md#regional-governance-w13-2026-11-23): a company founds its principal's one
homeworld in a region no one governs, paying the founding price into its treasury; a region's projects
run in queue order, cancelled with what was delivered kept; a governor's standing policy tenders the
head's inputs within a limit; the snapshot shows every region, its treasury, queue and policy its
governor's alone, on the wire too; and `fixture_development`, a homeworld developed with no client
beside the AI's, a late joiner founding the last free region and a third refused. The fixture's first
run passed over every bid close: its days were counted in one-second steps on a host stepping at 1/60
s. A new saved component moves every pin, its storage listed in the save even empty: four moved with
the policy, two with the queue's field and two with Forge North's workshop, bytes alone each time.

## W12: competitive procurement — 2026-11-23

In six slices with no engine edit, on the [tender contract](stage-4-trading-plan.md#tender-contract-w11-2026-11-21):
a region's project, its tenders and their accounts, sealed bids and the award, delivery by the tote
with the bond and default, saved jobs that bid and deliver, and the governance fixture's Bronze
project supplied from Forge and Cassiter, worked and unlocked. The issuer rebuilt the market's rules
each turn, which put the console's Ore to Brine at 4.29 s against its 3.56 s budget, so it keeps
them; the budget, nearly spent by the economy since L27, was measured again at 3.50 s. The first
fast-forward test of delivery missed a broken quiet, the AI's closes falling on day boundaries the
system runs at anyway, so it uses a directed tender. Slice 4 wrongly said the pinned worlds hold
tenders; none does, and slice 5 corrected the plan. A probe saw an NPC default on tin when another
trader bought out its source's asks, the default and retender working as designed. A starter's
credits buy no project's goods, so the fixture funds its company, and its goods ledger allows a
project's start.

## W11: the clean tender lifecycle's choices — 2026-11-21

A planning task, no code: the user settled the Bronze project's totes and work time, the tender
windows, the AI issuer's budget and retendering, delivery at the region's venue, the money's rounding
and the policy's contract jobs ([D-101](../decision-ledger.md#d-101)), and [the tender
contract](stage-4-trading-plan.md#tender-contract-w11-2026-11-21) defines the saved state, rights,
reservations and W12's slices. The project's authored 90 kg and 10 kg were each below one tote, which
D-087 had made the least that moves. Reservations are accounts of their own, so a reserved budget or
bond is no one else's money and the ledgers sum as before.

## W21: fuel burn — 2026-11-22

On two Jettison provisions, a cruise's coast phase (pin `a9b8305`) and a station's reach measured
where physics placed the ship (`2b3cfce`). Ships fly a clean fusion torch burning reaction mass and
fusion pellets from tanks apart from the hold, refuel as they leave a venue while their owner allows
it, charge the fuel to the lot, and a standing policy picks the thrust share that makes the most an
hour ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w21-2026-11-22)). Choosing per hour sent the
policies hopping round Ore's one orbit, which showed ships leaving Vale held at `ClearOf`'s point,
measured a step before physics placed them, and departures, arrivals and cruises sharing one line,
booked as Jettison's station traffic lanes (the user's). A cruise is 24 million steps, so
`fixture_burn` compares stepping with the fast-forward over two minutes about each switch of
thrust; Brine's copper bids take two or three totes, so the full hold is the test's own copper.
The policy charges, sells and chooses in the step it docks, so the fixture sees no charge, and a
refuel turns totes into kilograms, so it keeps the credit ledger alone.

## W19: supply and demand pricing — 2026-11-22

With no engine edit; the pin moved to `f14c288` for the shell's profilers alone. Each broker posts
four rungs a side on a curve of its stock against its target, households bid up to the ask at the
target times one and three times a need's pressure, makers sell down to their wages, and tenders are
priced by need ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w19-2026-11-22)).
Eight rungs reposted every turn blew the step budget, so a requote keeps a ladder that has not moved;
ten stations did again, until perf, now in the shell, showed batches stepped through every turn and
books read whole. The households' reference was first `r` and their pressure a day's, and a fed
region bought nothing (the user's correction). Six days of the fixture made no shortage or glut of
their own, so `fixture_prices` makes them, a hoarder and a windfall, and shows eight days settling.

## W18: the transport and holding charges at their real values — 2026-11-21

On pin `0f973b2`, with no engine edit, as the plan expected. A ship pays by its hull's pad class and
its occupants, a venue's fees are set by the principal directing its operator or by an AI operator's
daily occupancy rule, the life support a lot's ship draws in transit is charged to it at the company's
last prices, and a ship held 7 days for its arrears has its cargo sold at the live bids
([what it chose and its evidence](stage-4-trading-plan.md#delivered-w18-2026-11-21)). The gate moved
to a second checkout during it, with tidy in it, after a local gate without tidy let a finding
through. Seizure pays the arrears from the company's account, the proceeds when it was empty; a
station's balance cannot show the payment in a test, since it also takes the NPC traders' fill fees.

## W10: standing trade policies, autopilot and NPC jobs — 2026-11-21

On pin `0f973b2`, with one engine provision. Each ship's saved job chooses, flies, buys and sells
through the players' own commands, a scenario's NPC traders run the same executor on their own
reports, and docking and berth fees are charged at nominal values, each venue's own
([what it chose and its evidence](stage-4-trading-plan.md#delivered-w10-2026-11-21)). The plan
expected no Jettison gap; the zero-client run found one, a ship that circled the point above its pad
at Ore until its reservation lapsed. Jettison's docks now bring a ship to rest out along its pad's
normal first, a stopgap, the quality solution on
[Jettison's roadmap](../../../jettison/docs/roadmap.md).
No suite had fast-forwarded a blocked job; the `policy` suite does since L28.

## L28: docked fleets and the fast-forward — 2026-11-21

On pin `0f973b2`, with no engine edit. Observation's skip captures each docked fleet's venue at the
last step passed over, and its quiet asks for no step, so a docked wait is passed over
([what it chose and its evidence](stage-4-trading-plan.md#delivered-l28-2026-11-21)): an hour docked
at Brine is 6 calls, from 3,600. The evidence the row named, W10's zero-client run, cannot show it:
each docked wait there falls while the other ship is steered in a station's frame, which runs every
step, so the fixture's cruise test holds the evidence instead.

## L27: a fast-forward over quiet steps — 2026-11-20

On pin `892b7b3`, with its engine provision. Each of Manifest's systems says when it next has to
run, and the console's `step` and the server pass over the steps between, to the same bits as
stepping them ([what it chose and its evidence](stage-4-trading-plan.md#delivered-l27-2026-11-20)):
Ore to Brine on the manned profile is 2.37 s, from 4 min 50 s. The plan had named life support and
observation as what writes every step; the economy's production does too, every batch's work added
each step, and like life support's air it is passed over by making the same addition once a step,
which keeps every rule and every bit. Observation is not yet: no step is passed over while a fleet is
docked.

## W09: the complete trading fixture — 2026-11-20

On pin `9e0274f`, with no engine edit. The `fixture` suite plays one command stream on the trading
scenario from purchase to upgrade, checking every step that credits and goods move only by the
upgrade, provisioning, delivered batches and the households' use, and runs it saved in flight,
replayed and from the pack ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w09-2026-11-20)).
Two things the plan had not foreseen: a batch is about a day, so the suite shortens it with an overlay
of its own to see production within the loop; and the trading fixture sold no oxygen or cartridges,
so Ore now makes them and Brine wants them. No suite had a refused cruise, which `travel` now has.

## L26: the console's long step — 2026-11-20

On pin `9e0274f`, with one engine provision. `step` runs a slice at a time beside the frontend,
with its progress in the TUI and `stop` to end it
([what it chose and its evidence](stage-4-trading-plan.md#delivered-l26-2026-11-20)). Profiled, a
cruising step was 26 µs, nearly half of it Jettison evaluating a station's orbit once for each
entity on its rails; placed once a body there, it is 13 µs, and `step-budget` fails a release
build's step above half as much again. Asked whether Manifest's systems could run less often for the
same results, the profile said they are under 5% of a step: what costs is the engine's movement.

## W08: company-funded cargo upgrades — 2026-11-20

On pin `54fb871`, with no engine edit. `Upgrade` buys a tier of an offer, a new versioned
`Manifest.Offer` record, for a company's docked ship with the company's own credits, and records it
in the ship's saved `Systems`; the ship's hold is its hull's with each system's effect, computed as
the effects engine computes a hull's stats, which `Recompute` now shares
([what it chose and its evidence](stage-4-trading-plan.md#delivered-w08-2026-11-20)). The starter had
no slot for any system and the only cargo effect is sized for the long-haul trader; the user gave the
starter the slot with the effect unchanged, a price only a completed trade reaches, docked purchase
with no payee, and the hull's mass authored ([D-096](../decision-ledger.md#d-096)). The console's
`upgrade` was taken by the development sandbox's skill tree, so the command is `fit`.

## W07: live and stale market observations — 2026-11-20

On pin `54fb871`, with no engine edit. `manifest.observe` runs after travel: each player's report of
a venue their fleet is docked at is refreshed as the step ends, from the completed book and the
market's sales over the hour, and stays as it was once the last ship leaves; provisioning gives
starting reports ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w07-2026-11-20)).
The plans' two readings of the starting grant disagreed, the market plan's neighbouring systems against
Stage 4's other station stale at bootstrap, and no scenario named a connection; the user chose both,
an authored connection list, and no reports for the host ([D-095](../decision-ledger.md#d-095)). The
`trade` module now sits under `company`, whose provisioning seeds the reports.

## W06: station life support and resupply — 2026-11-20

On pin `54fb871`, with no engine edit. `manifest.lifeSupport` runs after travel: a ship is
connected to its venue's life support as it docks while a connection is free, lets go as it leaves,
and off it draws its own tanks and cartridges in order; `Refill` and `Replace` resupply it from its
hold ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w06-2026-11-20)). The plans
left the connection, the resupply's source, an opened tote and a replaced cartridge open; the user
chose the connection on docking, the hold, the opened tote kept aboard and the old cartridge thrown
away ([D-094](../decision-ledger.md#d-094)). The goods are named by the tuning, so no code names an item.

## W04: the regional economy — 2026-11-20

On pin `54fb871`, with no engine edit. `manifest.economy` runs between the expiry sweep and the
brokers: each region's employer works its batches every step, and on a turn's last step pays the
wages earned, sells to its broker, and its households buy and consume, their shortages moving health
and happiness ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w04-2026-11-20)).
The user chose a regional employer per region, day-long batches with a wage share, households buying
whole totes through the book, and every region trading the essentials, which the trading fixture
lacked ([D-092](../decision-ledger.md#d-092)). A content check first refused a need's good with demand
on a region's row; the demand prices it instead, the need doing the wanting.

## W15: the vertical slice through the console — 2026-11-20

On pin `54fb871`, with no engine edit. The console sends the trade, cancel and travel commands, shows
a venue's book as the host, and steps by time, stopping as a journey ends; a player's snapshot holds
their ships' journeys and their orders, at protocol version 2
([what it chose and its evidence](stage-4-trading-plan.md#delivered-w15-2026-11-20)). The user chose
stepping by time over a local trip or the compressed cruise, and the host's book over an early live
view. A remote check at first read a stale snapshot: Jettison's `net` sends one at most once an
interval, so the test publishes until the step's arrives.

## W16: onboarding at a neutral spaceport — 2026-11-20

On pin `54fb871`, with no engine edit: Jettison's `Fleet::Park` already places a docked ship. A
scenario names its starter venue, and provisioning parks the starter there with no home region or
polity ([what it changed and its evidence](stage-4-trading-plan.md#delivered-w16-2026-11-20)).

## W17: the cruise by acceleration — 2026-11-20

On pin `54fb871`, with one engine provision, `Space::CruiseProfile`: a drive's boost, its time and a
sustained acceleration in place of the power law. The Manifest mode gives every hull a manned or an
unmanned profile by its occupants and Astralis keeps its drives
([what it chose and its evidence](stage-4-trading-plan.md#delivered-w17-2026-11-20)). In the engine,
a slow ship chasing a station round its planet need not settle in the cruise's eight solve rounds, so
a profiled cruise searches for the fewest whole steps that cover its course. The user chose that a
hull with its occupants left out, the long-haul trader, cruises manned.

## W05: ships travel between venues through the engine — 2026-11-20

On pin `377bd8d`, with two engine provisions: `Space::ClearOf`, and a cruise or lane letting a steered
ship's controls go with its destination. A new `travel` module puts the hosted world in space, parks
a new ship on a real pad and drives a saved journey through Jettison's pad release, flight out,
cruise and pad request ([what it chose and its evidence](stage-4-trading-plan.md#delivered-w05-2026-11-20)).

The user chose the star systems as engine records and the sample's shuttle timing
([D-088](../decision-ledger.md#d-088)), and said long hauls will be tuned later, early play perhaps
between stations round one planet or its moons. A trace of a ship waiting in a full station's queue
found it flying at the station at 86 m/s: the cruise kept the demand steering left from the flight
out, which the engine now lets go. The user also set new onboarding mid-task
([D-089](../decision-ledger.md#d-089)), queued as W16 rather than folded in.

## W03: the hosted book on owner and venue accounts — 2026-11-20

On pin `65846bb`, with no engine edit; the dock-before-lanes edge W05 needs is in Jettison's
`feature-base` (`bc5dc1a`), not yet pinned. A new `trade` module holds orders on their own entities,
reserving a seller's totes in their holding and a buyer's credits in escrow, matched at once on
placement and settled in one planned write, with a day's expiry and a broker at each venue quoting
around the development model's native price each turn ([what it chose and its
evidence](stage-4-trading-plan.md#delivered-w03-2026-11-20)).

Mid-task the user asked to plan for eighth-size IBCs, 500 mm a side, so traders buy expensive goods
in smaller amounts, and then to name them: they are totes ([D-087](../decision-ledger.md#d-087)), for
every good, and every count of goods is now in totes, Stage 3's holdings, holds and limits included,
since no save is durable before Stage 5; at the user's word content authors a hull's `CargoVolume`
and a venue's `stock` in totes as well, the cargo-hold upgrade adding 2,400 a tier. Every quantity the code, the data and
the current plans gave in IBCs is now in totes; the IBC remains only as the cubic metre container
the item fields are defined against, and in the records of D-078 and D-083, which D-087 supersedes. The governance fixture's
stocks are restated from item units to totes. The `company`
suite's pinned hash moved, the world now
holding brokers, venue accounts and the order and broker components. Left: the trade views and
console commands (W07, W15).

## W02: the step, settlement and travel choices — 2026-11-20

A planning task, documents only, with no engine edit. Checking the C01 table and `balance.yaml`
against the plans, as N03's handoff asked, found the trading fixture at odds with whole IBCs
([D-078](../decision-ledger.md#d-078)): its stocks are fractional item units, tin and gold under one
IBC, and one IBC of copper (about 118,500 credits), tin or gold beyond a 25,000-credit start, which
D-077's "two tonnes of copper" no longer described. The user selected, each the recommended
option: orders in whole IBCs priced per authored unit with the fixture restated in IBCs
([D-083](../decision-ledger.md#d-083)); a broker at each venue owned by its operator, requoting each
economic turn ([D-084](../decision-ledger.md#d-084)); a day's order life and a 64-fill pass, held by a
64-order side ([D-085](../decision-ledger.md#d-085)); a 1/60 s step and one travel command, free of
charges in the first slice ([D-086](../decision-ledger.md#d-086)).

The [step](stage-4-trading-plan.md#the-step-in-order) is now a table of declared edges from the
engine's registrations: `spatial`, `steering`, `flight`, `physics`, then `dock` and `lanes`, then
`cruise`, with Manifest's expiry, economy and brokers ahead and travel and observation after.
`dock` and `lanes` both write `Transform` and `InFrame` with no edge between them, and Manifest
cannot add one to systems Jettison registers: the one engine gap, for W05. Left: the brokers'
treasuries and spreads (W03's fixture values); the handling, life-support service and payroll
choices stay with W04, W06 and the economy plan's handling services.

## N03: several clients, reconnects and bounds — 2026-11-20

On Jettison's `net` as N02 left it (pin `65846bb`), with no engine edit. An actor may hold three
sessions at once ([D-082](../decision-ledger.md#d-082)): the server counts an actor's sessions as it
verifies a token and closes a fourth as `ServerFull` once admitted, since Jettison's verifier can
refuse only as `BadCredential`. `Client::Retry` sends a request again under the sequence it was first
sent with, so a client that reconnects without hearing a request complete asks again and is answered
`Completed`, the request not run twice.

Evidence, in the `remote` suite: three sessions of Ada's and one of Bea's served at once, each
sent only its own company; Ada's fourth closed as `ServerFull`, and a place freed when one leaves;
two of Ada's sessions sending one sequence with different content, the second refused as
`RequestConflict` and its next request queued under the sequence the refusal named; the world
stepping five times with no session, and a reconnecting administrator's retried provisioning
answered `Completed` with one company provisioned; `kPendingPerActor` requests queued and the next
`QueueFull`, while a client sending a thousand messages before the server takes in any is closed as
`InboundOverflow`. The console still advertises only `observe`, `provision` and `cargo` remotely,
and a snapshot holds no market rows (W07). Left: a session closing while its actor reconnects in the
same poll counts against the cap until the server hears it go, so a client may meet `ServerFull` and
try again; a request pending when its session closes is still reported to a later session only when
retried (R08); one session per actor for a real-time variant (D-082).

## N02: the dedicated server and remote TUI — 2026-11-20

Jettison provisioned the contract's gaps in its `net` module (engine `65846bb`, with its own
consumer's loopback run), and Manifest builds on them with no engine edit. A new `remote` module
holds the protocol (`manifest`, version 1: a welcome naming the token's actor and its next sequence,
a request, and a reply carrying the ledger's receipt and the actor's next sequence, sent at
admission and again once the request has run, so a client sends a refused sequence again), the token file (YAML of BLAKE2b digests, written whole through a file beside it), the
server and the client. The server resolves a session's actor only from its token, admits its
requests through `Command::Requests` between steps, and sends each session `Query::Observe`'s view
of its own actor after every step. It reads the token file again when it changes and closes, as
`Revoked`, every session whose token is gone. A message that is not a request, or does not decode,
closes the session as `BadMessage`.

`manifest <options> --serve --tokens <file> [--address] [--port]` steps the world at its fixed step
against the wall clock (`Time::FixedStepAccumulator`), with or without clients, and stops cleanly on
SIGINT or SIGTERM, telling its clients `Shutdown`. `manifest-token` issues and revokes tokens,
printing a token once and keeping only its digest. The console's `connect <host:port> <token>` and
`disconnect` play on a server: while connected, `observe`, `provision` and `cargo` go to it as the
token's actor, so `cargo` takes no principal; the TUI prints what arrives on its refresh tick and
shows the connection in its side pane.

Evidence: the `remote` suite (a bad token refused; the administrator provisioning two players; a
player refused provisioning as `NotAuthorised` and its next request sent under the same sequence; each player's snapshot only their own company;
revocation, a bad message and shutdown each closing the session with its reason), the `console`
suite's connect case against an in-process server, and `cli_serve`, which runs the executable with
no client for two seconds and checks that it stepped and stopped on SIGINT. Left: N03's several
clients, reconnects and bounded queues; a request still pending when its session closes is
reported to a later session only when it is retried; the TUI's frame itself is not under a test;
the port defaults to an ephemeral one; plaintext until encryption (D-081).

## N01: the network contract — 2026-11-20

The user selected the transport, the credential and the encryption plan
([D-079](../decision-ledger.md#d-079) to [D-081](../decision-ledger.md#d-081)): ENet over UDP rather than
TCP, because Manifest will also have variants played as real-time space combat; bearer tokens the
server CLI issues; plaintext until libsodium encryption, which blocks hosting outside a trusted
network. The [network contract](gameplay-and-server-plan.md#network-contract-n01-2026-11-20)
records the handshake, encoding, sessions and snapshots, and maps each mechanic to Jettison's
provision. Its gaps are the `net` module on ENet, the handshake, bearer tokens, framing and limits,
and snapshot replication, all for N02. Request admission and unattended stepping need none. No
code changed.
