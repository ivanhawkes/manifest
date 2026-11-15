# Public supply contracts and station bookings

Planning handoff, 2026-11-15. Extends the [planetary/corporate plan](planetary-development-plan.md)
and [roadmap](roadmap.md). Implementation and validation by the selected implementer remain
pending. This document changes no code, authored balance, schemas or engine SDK revision.
The inspected baseline remains Manifest `feature-base` at `8f02d54` and Jettison `feature-base`
at `0564f19`, with Manifest SDK pin `f85092027eda49c29001f7589608df1f0b8827fb`.

## Decisions and proposal status

Confirmed: public procurement requires corporate bids, including the governor's own company;
public and company accounts are separate; suppliers are paid for accepted partial deliveries
as they arrive. The user subsequently selected a supplier-funded performance bond: initially
10% of the winning offer, locked at award until contract close, returned on complete delivery
and proportionally forfeited for undelivered goods on abandonment or missed deadline. The
starting rate remains subject to balance testing. Add simple corruption and abstract bribery
after clean tendering works.
Station owners can set fees/priorities and refuse rivals, but must honour accepted bookings;
policy changes apply to future requests. Standing policies continue without connected clients.

Subsequent [economy/development requirements](economy-and-development-plan.md) specify instant
credits, physical commodities, principal brokers and separate warehouse handling. Contract
terms name which handling/transport legs the supplier pays and where acceptance occurs.
Frontier exclusivity uses real elapsed server-running time; tender deadlines below remain
proposed simulation-step deadlines rather than automatically using that different clock.

Except for the confirmed rules above and the performance-security section below, the particular
tender format, lifecycle, numeric settlement, expiry/default and booking terms below are proposed
defaults for the first slice. They are not additional approved mechanics or evidence of
implementation. They preserve the [authoritative command contract](stage-3-authority-plan.md),
[physical trading schedule](stage-4-trading-plan.md) and [recovery requirements](stage-5-recovery-plan.md).

## First public project and tender

Use one public port-capacity project, supplied with one existing authored good, at one
receiving venue. `ItemCopper` is a possible test input from stage 2's proposed eight-good set;
it does not establish a construction recipe or alter any base price. Pick quantities, budget,
work duration and output effect as explicit scenario values during implementation.

The public authority owns the delivered materials and completed improvement. A separate
corporate concession/ownership agreement is required to give a company operating rights.
The first test destination is an accessible public port; private-port dependencies can be
tested after bookings work. Purchasing supply must not secretly mint receiving access that
the port has not promised.

The proposed tender fixes an item/unit, target quantity `Q`, maximum whole-credit budget `B`,
receiving inventory/project, opening step, bid-close step and delivery-close step. All values
are finite, positive where required, within validated numeric bounds and ordered in time.
Definitions, scoring version and terms freeze when published. Editing the specification
requires cancelling/reissuing before award rather than changing an existing obligation.

Reserve `B` from the public treasury when publishing. The same money is neither spendable
on another project nor available to the company that controls the government. Reject an
unfunded tender atomically. Bound open tenders and retained outcomes independently of logs.

## Bids and award

Propose sealed bids, one active revision per company, quoting a positive total whole-credit
price `T <= B` for all `Q` delivered by the common deadline. This is a total offer, not a new
floating per-unit price. Any separately exposed per-unit price must follow the existing
Decimal policy. Qualification is native: company authority, target/world compatibility,
required transport capability and the published service terms. No bribe makes an invalid bid
eligible. The governor cannot bypass evaluation to select their company.

Before close, a company may replace/withdraw its bid with the expected bid revision. Store
the final revision and its server acceptance sequence. For the clean pilot, rank qualified
bids by lowest total price, then the final revision's sequence, then stable company ID.
All bids share the same required quantity and deadline, so an unimplemented reputation score
is unnecessary. More elaborate quality scoring is a later explicit change to tender terms.

Do not require suppliers to own all promised goods before bidding: physical procurement may
follow award. Outstanding commitments are obligations, not imaginary stock reservations.
As a pilot limit, assign a ship to at most one active procurement delivery job and bound
company commitments; reserve real stock/capacity only through its actual native accounting.
These limits are proposed scenario constraints, not a permanent company-size cap. Recheck
qualification/commitments at award in stable tender order so simultaneous awards cannot
assign one exclusive delivery job twice.

At award, also require the candidate to fund the published performance bond from unreserved
company funds. Lock that bond atomically with the award and public contract reservation. If
the candidate cannot fund it, consider the next qualified bidder; do not create an unsecured
award or debit a negative company balance. Earlier awards consume available funds before later
tenders are evaluated. Bid submission alone does not lock a bond.

At the closing boundary, award once to the best currently qualified bid. Move `T` of public
reservation into the contract and release `B - T` to the same public treasury. With no
qualified bid, close unawarded and release `B`. Record rejection/award results for retries.
Changing a client connection cannot reopen bidding, reset priorities or award the job again.

Keep other companies' bids private while open, including from a player who governs the
issuing planet. The authoritative award system can read them; that does not grant a GUI/TUI
or the governor's company privileged query access. Propose publishing winner, total price,
deadline and a structured award reason after close. Disclosure of losing bids and later
bribery details remains a design choice.

## Delivery, payment and project work

The supplier obtains goods through normal trade/production, carries them physically and
requests acceptance at the designated venue. Validate current company ownership, unreserved
inventory/cargo, authoritative docking/access and the active contract at execution. No remote
inventory transfer from a different venue is a delivery. A docked unload-and-accept operation
may combine native transfers atomically; it must not let unloading and acceptance credit the
same goods twice. Intermediary supplier inventory remains company-owned until accepted.

Accept at most the remaining quantity; retain excess as the supplier's property and report
the exact accepted amount. Transfer accepted goods into public project inventory, update
fulfilment, pay the incremental entitlement from contract reservation, and store the receipt
as one transaction. Rejection leaves both parties' business state unchanged. Native economic
consumption and other projects cannot spend these project-reserved materials.

Payment happens after each accepted partial delivery, as selected by the user. The proposed
new contract rule is cumulative entitlement: at accepted total `q`, entitlement is the
nearest whole credit to `T * (q / Q)`; the incremental payment is entitlement minus credits
already paid. At complete fulfilment, entitlement is exactly `T`. This prevents independently
rounding every small delivery into a larger total payment. Example only: `Q = 10`, `T = 7`,
deliveries of 3, 3 and 4 units pay 2, 2 and 3 credits.

This cumulative rule is a proposal for public supply contracts. Existing marketplace
per-fill rounding stays as documented in [market prices](market-prices.md); this plan does
not migrate wallets, fractional quantities or market settlement. The implementer must
specify checked arithmetic/representable ranges, floating quantity normalisation and minimum
meaningful delivery size. Never grant missing goods merely through an epsilon. Use canonical
accepted quantities, reject no-progress/nonfinite operations and preflight goods, entitlement,
ledger additions and refunds before writes. Compare payment under fragmented deliveries of
the same canonical quantities, including rounding boundaries, huge offers and the final
remaining quantity. Do not claim arbitrary real-number splitting is exact with doubles.

Construction begins when its defined required inputs are secured. The first project may
wait for the complete input set, then consume it once and advance a saved fixed-step work
counter. Partial supply payments do not themselves finish the infrastructure. On completion,
apply the improvement once through the appropriate local effect; do not rebuild the planet's
population, era or production deck wholesale.

## Deadlines, cancellation and failure

The following simple failure rules are proposed for the pilot:

| Event | Proposed result |
| --- | --- |
| Issuer cancels before bid close and award | Close tender; release its public reservation; notify bidders. |
| Supplier withdraws before close | Retire its active bid; no award or clean-pilot expense. |
| Bid/acceptance request executes at or after its closing step | Reject as late, regardless of earlier network receipt. |
| Supplier finishes all required deliveries | Mark fulfilled; pay exactly `T` cumulatively; release the delivery job. |
| Deadline arrives with goods missing, or supplier abandons after award | Mark partially fulfilled/defaulted; retain accepted public goods and settled payments; release only unpaid contract reservation to the public treasury. |
| Issuer wants to cancel after award | Unsupported in the pilot; a future termination/compensation rule must precede that command. |

Do not silently retry a failed project by publishing unbounded tenders or minting new funds.
A public policy can tender the remaining requirement within its remaining budget. The selected
performance bond adds the separate settlement below. No advance, additional fine or negative
company balance is required. Report project pauses/defaults to absent-player activity summaries.

## Performance security (selected)

Publish the bond requirement with the tender; use 10% of the winning total offer as the initial
rate, subject to later balance testing. Freeze the requirement with the other tender terms.
The supplier funds it at award from its company account, separately from the public payment
reservation. It is not a withholding from payments for accepted goods, and it remains locked
until the contract closes rather than being released after each partial delivery.

On complete delivery, return the entire bond to the supplier. On supplier abandonment or a
missed delivery deadline, forfeit the proportion corresponding to undelivered goods to the
issuing public treasury and return the rest to the supplier. Retain accepted public goods
and settled partial-delivery payments. Use accepted quantity against the original required
quantity to determine the undelivered share, not the number of shipments or payments.

Example: a 10,000-credit offer requires a 1,000-credit bond. At 70% accepted delivery before
default, the supplier has earned 7,000 credits for goods, recovers 700 credits of its bond
and forfeits 300 to the public treasury. Zero delivery forfeits the entire bond; complete
delivery forfeits none. The bond does not guarantee that a replacement supplier is affordable.

NPCs and the governor's company follow the same funding, release and forfeiture rules.
Disconnecting does not release security, stop deadlines or cancel obligations. Public and
company accounts remain distinct even when one player controls both.

The percentage and proportional disposition are selected game rules. Whole-credit rounding,
minimum representable bond and checked quantity-to-credit arithmetic still need a precise
numeric contract; these are not permission to change marketplace settlement or quantity types.
Termination caused by issuer breach or unavailable contracted station service also remains
design work; the selected rule does not establish a fault-adjudication or compensation system.

Persist the original bond amount, supplier and treasury identities, locked balance and terminal
release/forfeiture receipt with the contract. Award, fulfilment and default must transfer or
release the money once, including across retries and recovery. Required implementer evidence
includes insufficient-funds fallback, simultaneous awards competing for funds, zero/partial/full
delivery, quantity fragmentation, rounding bounds, NPC/own-company parity and crash/retry around
bond lock and final disposition. Refund plus forfeiture must equal the original locked amount.

## Fixed-step and idle execution

Use the clock value before `SimWorld::Step` advances it as execution label `s`. A bid or
delivery is valid only when its operation has opened and `s < its close step`;
command-before-systems ordering does not extend that deadline. Apply commands in assigned
order, then close tenders/contracts in a
declared stable order, then advance economic/development/movement systems according to their
dependencies and publish the completed snapshot. A ship docking during systems can deliver
at the next command boundary, which may be too late; expose that timing in estimated routes.

Server-side bidding policies specify allowed work, minimum margin/maximum acquisition cost,
delivery commitments and operating budget. Delivery jobs obtain, load, travel, dock and
deliver through the existing physical loop. Distinguish a submitted bid, won contract and
completed job. No browser/TUI callback is needed to submit a standing bid or collect payment.
NPCs use the same rules, resource checks, information access and bounded decision cadence.

## Later corruption slice

After the clean lifecycle, add a government's bounded corruption trait and a company-funded
abstract influence expense tied to a tender/bid. Use a versioned deterministic score over
qualified bids, with published terms for normal bid quality and the influence contribution.
The pure clean mode and zero-corruption case must reproduce the original price ordering.
Company control of a puppet government does not skip the tender or award-eligibility rules.

Specify influence commitment, debit destination/sink, cap, disclosure and refund policy
before implementing bribery. Different bids can be favoured without changing physical
delivery/payment validation. Prices, influence expenses and public reservations remain
separate amounts. Standing policies require an explicit influence-spending allowance;
disconnecting is not permission to spend one. Detailed officials, investigations and media
scandals remain later work.

## Station quotes, accepted bookings and owner policy

An owner may change fees, priorities and rival-access policy for future requests. The user
selected protection of accepted bookings. Establish an explicit acceptance boundary:

| State | Rights |
| --- | --- |
| Information/quote | Displays revisioned terms and quote validity; no funds or access commitment yet. |
| Pending booking request | Intention awaiting native acceptance; policy can still change before execution. |
| Accepted booking | Saves named company/ship/station, service scope, fee basis/cap, promised priority class and validity windows; reserves agreed funds. These terms survive later price increases or rival exclusion. |
| Arrival/queue/service | Valid accepted booking authorises its stated visit; physical compatibility and engine pad scheduling still apply. |
| Completed/cancelled/expired | Store result; settle delivered services and release unused reservation according to frozen terms. No indefinite reusable access pass. |

If policy changes before acceptance, a stale quote/request returns updated terms rather
than accepting a higher price silently. At execution, validate the requesting actor, ship,
station and quote/booking revision; one caller cannot accept another company's offer.
Record acceptance/order once and persist it before durable acknowledgement. Local admission
success before stage 5 does not promise crash durability.

For the first booking proposal, reserve admission, agreed service price and a priority class,
not a guaranteed pad/start time. Clearly display this distinction. Specify an arrival window,
a maximum queue wait and service/dwell limits at acceptance. Freeze those bounds and the
priority rank used for that booking; a later owner policy cannot downgrade or invalidate it.
Tie-break by accepted sequence and stable ship identity. Later bookings may have different
agreed classes under new policy; owning a high-priority class does not guarantee every future
visit. Binding timed pad/capacity guarantees need a separate reservation-calendar design.

Arrival with a valid booking uses its saved permission even if the owner now excludes that
company. Technical refusal, missed arrival, queue expiry and service failure return distinct
operation outcomes. A policy change is not a technical failure or a reason to shorten an
existing window. Terms for actual station incapacity must define refunds and reported breach;
returning money is not evidence that a promised service was performed.

Reserve the maximum agreed charge on acceptance; capture only completed service fees and
release unused funds at close. Specify any time-based billing cadence and caps in the original
terms. No new price applies mid-booking. Default the first service to a simple fixed fee;
complex storage/repair/refuelling tariffs are later services with separate contracts.

## Docked ships, private cargo and economic pressure

Existing accepted services finish under their terms. Ships can depart through the normal
release path; a new ban does not grant the owner a command to seize the ship or cargo.
Private inventory, open orders and escrow retain ownership independently of station policy.
Cancellation/refund queries must remain usable even when new commercial visits are denied.

Where storage is offered, its original terms must include withdrawal/collection and expiry
rules. A new exclusion cannot silently remove those existing rights. A bounded collection-only
visit can honour them without granting new deposits, trade or an unlimited future pass.
Design this before enabling remote storage; the first simple docking service need not add
a storage system. Whether financial liens, forced sale or other sanctions exist is separate
later scope, not a consequence of changing an access list.

An owner can still exert the intended pressure by refusing new bookings, raising future
prices or favouring its own future traffic. Existing commitments reduce how instantly that
power applies. Automation must distinguish grandfathered service from a denied new visit and
reroute, negotiate or report a blocked job. Competing ports on frontier worlds become eligible
after the exclusive resource period, even under an incumbent puppet government; starting homes
retain separate protection. Rival ports/destinations provide independent options without
forcing the original owner to accept new rivals at its own facility.

## State, capability boundaries and delivery

Persist tender/contract identities, terms/revisions, public reservation, final bids/sequences,
award/scoring version, accepted quantities, cumulative paid amount, private/project inventory,
bond terms/balance/disposition, assigned jobs and retry outcomes. Persist station policy
revisions, quotes needed for accepted operations, booking terms, funds, priority/window state,
technical operation identity and
receipts. Bind accepted visits to existing engine docking state; replays must not enqueue a
second pad request. Bounded retention cannot make an old paid request executable again.

| Mechanic | Existing public foundation | Required provision/owner |
| --- | --- | --- |
| Tenders, awards, payment and corruption | `SimWorld::RegisterCommand`, fixed steps, world services, archives and hashing; Manifest prices/inventory/progression rules | Manifest native schemas, business validation, obligations, settlement and scoring. No generic auction/bribery economy in Jettison. |
| Docking operation | `Dock::RequestPad`, `Docking`, `Pad`, `ReleasePad`; physical queue/reservation state | Manifest commercial booking and policy; Jettison reusable admission/refusal and queue-ranking seams. |
| Priority and grandfathered entry | Current `Dock::Station` is a compatibility-aware insertion queue; `RequestPad` validates ship control, not station permission | Game supplies saved permission/rank for each operation; engine consistently consults that policy for ordinary player/NPC arrivals. `Park` stays trusted startup/restore composition. |
| Network clients and recovery | In-memory engine commands/snapshots and save/load; prior plans identify missing network/host provision | Share typed commands/results and filtered views across TUI/GUI; use stage 5's accepted-order journal and recovery contract. |

Do not duplicate engine pad queues, handcraft a separate content loader or expose raw trusted
engine movement commands to clients. If the engine lacks an operation identity, refusal
reason or policy seam needed here, demonstrate and provision the smallest reusable capability
with an independent consumer before pinning it in Manifest. Game policy stays here.

Delivery slices: clean supply tender and one public project; partial acceptance/payment and
idle physical fulfilment; replay/recovery/failure coverage; station booking terms plus engine
admission/queue seams; owner-policy changes and old-booking protection; corruption/scoring.
Keep these separate from frontier generation and a full political simulation.

Required evidence includes no-bid/tie/withdrawal/late/invalid awards; simultaneous commitments;
partial and excess delivery; payment fragmentation and overflow; supplier default/retender;
own-company bidding without private-bid access; restart/retry around award, delivery and fee
capture; queued/docked and in-flight bookings across price/ban changes; NPC parity; expiry,
blocked travel and recovery with no clients. Test against both directory/pack content and
cross-build replay hashes. Use repository presets/gates with two local compile jobs and no
local clang-tidy for cross-repository implementation. None ran for this planning document.

The [construction-site rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping)
now select funded reservation, non-refundable mobilisation, a fixed completion deadline and
reopening on abandonment/expiry. Exact funding, deadline and salvage terms remain unresolved.
The selected [exploitation grant](economy-and-development-plan.md#real-time-exclusive-exploitation)
starts at the first eligible operational frontier port plus concession activation, with a
disclosed provisional 72-hour server-running term. The selected first
[tech project](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
uses 90 kg local copper and 10 kg imported tin with an operational workshop; competitive
procurement and physical delivery apply to its public inputs. Next design long-running
demand/public-income circulation; construction details, colonial rights, inhabited AI
governments and later-join starter capacity still need design. Continue from these boundaries.
