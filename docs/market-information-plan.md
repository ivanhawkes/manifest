# Market information, fleet observation and stale reports

Selected product rules, 2026-11-15. Extends the [economy plan](economy-and-development-plan.md),
[gameplay/server contract](gameplay-and-server-plan.md) and
[physical trading plan](stage-4-trading-plan.md). These are requirements, not implemented
capabilities. Inspection used Manifest `feature-base` at `4e78dbb`, with planning edits,
and public Jettison interfaces at SDK pin `f850920`. No code, assets, SDK pin or environment
changed, and no builds or gameplay checks ran. W07 implemented the observation, starting-report and
sales rules and the query filter ([delivered](stage-4-trading-plan.md#delivered-w07-2026-11-20)); the
auto-trader rules below wait for W10.

The [regional correction](regional-economy-and-governance-plan.md) separates multiple regional
economies/governments from physical planets and station markets. Regional control, corporate
activity or bloc membership grants no additional live station reports and does not merge
players' intelligence. The onboarding neighbour reports still refer to system connections
and station markets, not to adjacent districts on a planet.

## Selected information rules

Exact current market information is available to a player only for a space station where
at least one member of their fleet is docked. This includes bids, asks and sales information.
The observation is station-specific: docking at one station does not reveal current prices
at every station in its system. Being in flight, nearby or waiting for a berth is not docking.

Fleet members share their observations with their player. Several docked ships may cover
several markets simultaneously; one remaining ship at a station maintains its coverage.
When the last qualifying fleet member departs, preserve that market's last observed data
and stop updating it. Returning a fleet member refreshes it. Fleet coverage supplies the
intended competitive information advantage for established players over new entrants.
The player's client connection is not the observation source: server-side fleet presence
continues to determine coverage while the player is disconnected.

Preserve a last-known snapshot for each market the player has observed, including its
observed bid/ask and sales values. Retain the snapshot through save/load and reconnect.
An unobserved market is unknown; an observed market with no quote or zero sales has different
information. Never represent an unknown value as a current zero or refresh an unseen value
just because a query or screen is opened. Market sales-volume definitions must be explicit:
today's `MarketQuote::volume` is cumulative traded quantity, not a rate over a recent window.
Observing a station's market summary does not override other companies' private inventory,
treasury and order permissions; define the permitted quote/sales fields explicitly.

Record each observation's market identity and authoritative observation step/revision, plus
whether it came from fleet docking or the starting report. A last trade's timestamp is not
the observation timestamp. The TUI and GUI must distinguish live, stale and unknown data,
show how old stale data is, and retain the observation time when displaying it repeatedly.
Measure report age in elapsed simulation time since observation. Display explicit units,
for example, `Stale · observed 15 simulation minutes ago`. Age advances while the world
runs, including while the player is disconnected, and pauses during server shutdown.
A starting report is stale even at zero seconds old because it grants no ongoing feed.
Save/load preserves the report and its original observation time; opening a screen does
not reset its age. Do not read a wall clock inside deterministic systems.

### Observation timing (selected)

At the end of each completed simulation step, refresh reports for stations with qualifying
fleet members docked at that observation boundary, using that step's completed market
state. A newly docked ship receives that step's completed report. When the last ship leaves,
retain the previously captured report; do not reveal later market changes from the departure
step. If another qualifying ship remains docked, updates continue. Report age is measured
from the last actual observation, not from the departure command or its UI notification.

Declare the system dependencies that produce this boundary, after market updates and docking
resolution. Queries and trader decisions use the latest completed permitted observation;
they must not read an unfinished step's market state. These are selected observable rules,
not evidence of an implemented scheduler or recovery path.

### Sales reporting (selected)

Report sales volume separately for each traded good at each station over the previous
60 simulation minutes. This duration is an initial balance setting, not a demonstrated
optimal window. Sum quantities from completed trades once; do not count the buyer and
seller sides twice. Also show the most recent sale price and when that sale occurred.
The last sale may predate the volume window, so its timestamp must remain explicit.

Zero volume means no sales in the observed period; an unknown market remains unknown.
If the market has existed for less than 60 simulation minutes, show the shorter history
covered. Live reports move their window with the completed observation step. Stale reports
retain their original window, volume, last-sale price and sale time until refreshed by
another permitted observation. Window bounds and history coverage belong to the report;
do not recompute a current rolling volume from hidden market state or age remembered
trades out of a frozen report. Preserve this meaning through save/load and starting reports.

For example, a report observed 15 simulation minutes ago can show 240 kg of copper sold
during the hour ending at observation, with the last sale at 8 credits/kg three simulation
minutes before observation. Its volume remains 240 kg while stale. These figures illustrate
presentation only; they are not selected prices, demand or balance evidence. Observed
activity does not guarantee future demand, available supply or arrival prices.

## Starting information

A new player starts docked at a station and has live access to that station's market.
At that player's spawn, also capture spot market data from every station market in each
system directly connected to the starting system. The user explicitly selected every
station market in those neighbouring systems, rather than one designated station per system.

Those neighbour reports contain values current at spawn but grant no ongoing feed. Mark
them as starting reports with their observation time; they age as stale information unless
the player later docks a fleet member at the relevant station. They suggest opportunities
for travel without guaranteeing the prices, volume or profitability upon arrival.

Use the authoritative connection graph and market set at spawn. Seed this information once
for that new-player start, preserving its identity and result for retries/recovery. Reconnect,
ordinary travel and later gate openings do not repeat the initial report. There is no
recursive report from neighbours' neighbours. Other stations in the starting system do not
gain live coverage merely because the starting ship is docked elsewhere in that system.
Topology discovery/reveal rules and what counts as a direct connection under changing gate
availability remain separate design work.

## Authoritative state and decision boundary

The server retains the real market state for matching, pricing and settlement. Player
knowledge is separate authoritative game state, with its own durable owner and observation
records. Resolve it through stage 3's player/company identities, not a transient client
session or a shared global market cache. The selected ship-transfer rules follow below;
sharing across future multiple companies remains to be designed. No information-sale
service or additional visibility exception is selected.

### Intelligence ownership and ship transfers (selected)

Remembered market information belongs to the player, not the ship. Selling or transferring
a ship does not erase the former owner's reports, and the recipient does not inherit the
ship's historical observations. Current coverage follows fleet ownership at the completed
observation boundary. A former owner losing their last docked ship retains the previous
report as stale; another qualifying ship preserves their live coverage. A recipient whose
new ship remains docked gains that station's live report at the next completed observation
step, without receiving the former owner's reports from other stations.

Acquiring a docked ship can therefore provide a foothold for observation. Initially, allied
players do not automatically share market intelligence. A future sharing or information-sale
mechanic requires explicit rules. The legal/economic ship-transfer operation, its atomic
ownership/controller changes and future company-level sharing remain separate design work.

Apply the same information boundary to player queries, native auto-traders, standing policies,
trade scripts and NPC trading companies using their own fleets. Internal market systems
still need their own actual stock, funds and orders; that does not grant trader decision
code global real-time market knowledge. Company-owned assets and transaction receipts keep
their existing private-view rules without granting an unseen market-wide refresh.

Build route candidates, expected profits, available-volume estimates, rankings and script
facts from the actor's live or last-known observations. Carry freshness through estimates.
Do not first choose globally profitable/feasible options from hidden current state and then
hide their prices: the candidates and rankings would already disclose unavailable information.
Exploration and decisions using stale information remain possible under the selected
auto-trader rules below. Detailed scouting selection and confidence discounts remain open.

Validate actual orders, access, stock and funds against current authoritative state when an
action executes. Stale observations do not lock prices, reserve goods, guarantee a buyer or
replace accepted contract terms. Specify responses to stale estimates and failed actions
without exposing unrelated hidden market values. Audit route previews, global price tables,
notifications and script inputs as well as the market screen. Filtering belongs at the
server query/decision boundary, before sending data to a frontend.

The existing development sandbox may retain privileged diagnostics. Its global `prices`
and `plan` workflows are not valid ordinary hosted player queries. Administrative access
must remain distinct from gameplay permissions.

## Auto-traders using stale reports (selected)

Auto-traders may plan routes from remembered prices and volumes. Show estimated profit
alongside the age of the reports used. Stale information has no automatic expiry; it remains
a lead to investigate, without becoming a guarantee of executable prices or quantities.
Unknown markets can be scouting destinations, but supply no invented prices, volumes or
profit estimates. Detailed scouting destination selection is not selected here.

Before buying or selling at a station, use the live report gained by docking and check the
player's configured trading limits. Follow the completed-step observation boundary above;
arrival does not permit reading an unfinished market update. Actual execution still validates
current authoritative state and enforces the authorised limits, since a live report cannot
guarantee unchanged terms by execution time. If changed prices exceed those limits, skip the
transaction and reconsider routes from the fleet's available reports without consulting
hidden current markets.

When cargo is already aboard, retain it while seeking an acceptable sale. Selling below
the configured minimum requires an authorised fallback policy; replanning or a script score
does not itself grant that authority. Standing policies govern these choices while the player
is disconnected. Preserve cargo, trading limits and the applicable policy through recovery.
This trading fallback is distinct from the host/script contract's native fallback for an
absent optional script; neither allows bypassing the other's limits or information boundary.

For example, a trader expecting to sell copper for 10 credits/kg finds a live bid of 6 on
arrival. With a configured minimum of 8, it retains the cargo and seeks another known
opportunity. The example's prices are illustrative, not selected balance or default limits.
The selected default limits and blocked-job behaviour follow below. Detailed route scoring
and scouting selection remain design work.

### Default trading limits and loss fallback (selected)

Use a preserve-capital default for unattended traders. Start a trade only when permitted
reports indicate positive estimated profit after purchase, transport and handling costs.
Set a maximum purchase price before buying. Require sale proceeds to recover the cargo's
purchase cost and attributable trading costs; players may configure a higher profit target.
Neither an inaccurate report nor elapsed waiting time authorises automatically relaxing
the trading limits.

If an acceptable sale disappears, retain the cargo and reconsider available markets every
60 simulation seconds. New permitted fleet observations can also trigger reconsideration.
Notify the player when the job first becomes blocked, and show the waiting reason and
holding costs. Standing policies continue while disconnected. This preserves explicit
limits, not guaranteed profitability: holding cargo may incur further charges that reduce
profit. Reconsideration still uses only the fleet's permitted reports and does not itself
authorise a move, another purchase or a below-limit sale.

Players may explicitly enable a loss-making fallback with a credit allowance for the whole
cargo. The default allowance is zero. An example instruction, `allow a loss of up to 100
credits on this cargo`, illustrates configuration only; 100 is not a selected default.
Partial sales share the same allowance rather than each receiving a fresh one. Preserve
the cargo identity, applicable limits, authorised allowance and its accounting through
partial fills, retries, replanning and recovery, so splitting transactions cannot multiply
the allowance. Script scoring and absence of a connected player grant no extra allowance.

Define cost attribution across cargo/partial sales, fee and whole-credit rounding, allowance
accounting/reservations, policy edits, and the interaction of observation-triggered retries
with the regular schedule before implementation. Those mechanics must preserve the selected
limits and shared allowance; no accounting or performance result has been demonstrated.

## Capability map and demonstrated gaps

| Requirement | Existing public evidence | Required work/owner |
| --- | --- | --- |
| Detect station observation | Pinned `Dock::IsDocked(registry, ship, station)` and reflected docking state | Manifest resolves qualifying fleet ownership and maintains station coverage at declared authoritative boundaries. Arrival/queue state alone is insufficient. |
| Store remembered reports | Pinned `SimWorld::RegisterComponent`, `WorldService`, `Save`, `Load`, `StateHash` | Manifest registers per-owner observation/provenance state and one-time starting reports using existing archive/definition mechanisms. No new generic persistence gap is demonstrated. |
| Actor market view | `MarketplaceService::Quotes`/`QuoteFor` accept world/market without an observing actor; `MarketQuote` carries prices, depth, last-trade time and cumulative volume | Manifest adds the hosted actor/observation adapter and query contract; these existing raw reads are internal evidence, not permission checks. |
| Timed sales summaries | Existing `MarketQuote::volume` is cumulative traded quantity | Manifest maintains completed-trade quantity summaries for the selected 60-simulation-minute window, with report bounds/history coverage and last-sale price/time. The cumulative field alone does not implement this requirement. |
| Filter before frontend delivery | Pinned `SimWorld::ShowInSnapshots<T>` copies every entity with the shown component and has no actor predicate | Manifest builds permitted views before replication. Do not transmit a full market snapshot and rely on UI hiding. Demonstrate a reusable SDK gap before provisioning one. |
| Informed trader decisions | `Simulation::PlanTrade` scans all civilisations; `ManifestConsole::CommandPlan` and its price display read global estimates | Manifest replaces hosted actor estimates with observation-based candidates/facts. Keep the development model distinct from the production decision surface. |

Source anchors: `source/game/ecs/include/manifest/ecs/Components.h`,
`source/game/market/include/manifest/market/Marketplace.h`,
`source/game/sim/src/Simulation.cpp`, `source/tools/tui/ManifestConsole.cpp`;
pinned Jettison `source/engine/{dock,sim}/include/jettison/{dock/Docks.h,sim/SimWorld.h}`.
Jettison supplies neutral docking, state, scheduling and persistence capabilities. Manifest
owns fleet intelligence rules, onboarding, permissions, observation-based decisions and UI
meaning. The separate transport/session gap remains as documented in the host plans.

## Required implementer evidence and open decisions

- Start docked at A: A is live; every market in directly connected systems has the same
  spawn observation time and stale-report provenance. Markets beyond those neighbours
  remain unknown. Reconnect/retry cannot refresh or duplicate the starting reports.
- Change an unobserved market's bid, ask and sales data. Remembered values and observation
  time stay unchanged while displayed age advances with simulation time. Disconnection
  does not pause age; server shutdown does. Starting reports remain stale at zero age.
  Docking refreshes them; undocking the last ship freezes them. Test a second ship keeping
  coverage and two stations in the same system with different coverage.
- Two players with different docked fleets receive different views. With permitted
  observations held fixed, changes only to hidden market data cannot change a trader's
  offered options, route ranking or script facts. Check TUI/GUI/API responses, previews,
  notifications and stale-action errors for the same information boundary.
- NPC traders and zero-client standing policies use their own observations. Hidden market
  knowledge must not enter through native fallback scoring or raw snapshot replication.
- Plan a route from a stale report and show estimated profit with report age. Age alone
  does not expire the report. On arrival, use the completed live observation and enforce
  configured limits at execution. A price outside those limits skips the transaction and
  replans only from permitted reports. If cargo is aboard, retain it; a below-minimum sale
  needs an authorised fallback policy. Check disconnected operation and recovery without
  losing cargo, resetting limits or granting fallback authority. Unknown markets may be
  scouting targets but cannot contribute fabricated commercial estimates.
- Under default limits, reject a new trade with nonpositive estimated net profit after
  purchase/transport/handling costs. Enforce the maximum purchase price and cost-recovering
  sale threshold, including a configured higher target. Waiting or stale-price errors cannot
  lower them. A blocked job retains cargo, retries on the 60-simulation-second schedule,
  can reconsider on new permitted observations, and reports its first blockage and costs.
  Test the default zero loss allowance and an explicitly authorised cargo allowance across
  partial/concurrent fills, retry and recovery; neither splitting sales nor replanning may
  multiply it. Show holding costs without claiming a guaranteed profit.
- Change prices in a docking or departure step. A newly docked ship receives the completed
  step's report; the last departing ship retains the previous report without that step's
  later market changes. A remaining docked ship keeps coverage. Verify age against the
  saved observation step, including immediately after departure.
- Complete several trades in one good and trades in another good/station. Verify separate
  quantities without double-counting either side; cancellations and unfilled orders add
  no sales. Declare and check deterministic window endpoints and expiry. Distinguish zero
  observed volume, unknown data, shorter new-market history and an older last sale.
  Freeze a report, advance beyond an hour and trade unseen: its volume, window bounds
  and last-sale facts remain unchanged until a permitted refresh. Recovery must preserve
  the live aggregation history and frozen reports rather than resetting coverage on restart.
- Save/load, journal replay and crash/retry preserve observation ownership, timestamps,
  coverage and onboarding identity. Replay must preserve the selected end-of-step boundary
  and last-known data without repeated one-time reports or resetting report age. Resolve
  atomic ship-transfer mechanics before implementing transfers. A transferred docked ship
  gives the recipient live coverage at the next completed boundary, not the former owner's
  history. The former owner retains remembered reports and loses updates only when no
  qualifying docked ship remains. Alliance membership alone shares no reports.
- Measure work by observed markets, covered stations, fleet changes, goods and decisions;
  do not refresh every player's memory from every market each step. Retain last-known
  reports while measuring memory growth as the explored world expands.

Remaining choices include concrete scheduler dependencies that
honour the selected observation boundary, atomic ship-transfer mechanics and future
company-level intelligence sharing,
topology discovery, detailed scouting/scoring, cost attribution and loss-allowance accounting,
policy-edit semantics and retry scheduling dependencies, sales-window aggregation/storage and exact endpoint
conventions, and concrete performance limits. No implementation, privacy,
determinism, scale or economic result has been established by this planning work.
