# Stage 1: public host and standalone bootstrap

Planning handoff for the selected implementer, prepared 2026-11-15 and refreshed
2026-11-16. This refines stage 1 of
[development stages](development-stages.md); it does not mark implementation complete.
OpenAI work is repository inspection and planning documentation only, under the
[agent workflow](agent-workflow.md#planning-only-policy).

**Planning status: complete and ready for implementation.** The capability map,
lifecycle, delivery slices and acceptance matrix below form the Stage 1 handoff.
No product decision remains that blocks this stage; exact API names and extraction
details are implementer choices within these constraints. Implementation and its
acceptance evidence remain pending, after the maintenance prerequisites below.
Use the [prioritised delivery order](delivery-order.md#priority-1-stage-1-host-and-bootstrap)
for task dependencies across this stage and the wider plan.

Apply the later [engine/game boundary assessment](engine-game-boundary-assessment.md).
The host must be genre-neutral; engine-local gameplay stays in explicit test/sample
targets. A neutral CPU consumer and presentation-free configuration are required alongside
the space sample, not deferred solely to a future server milestone.

## Delivery status, 2026-11-18

Slices 1–5 are done (slice 4, H06, and slice 5, H07, in Manifest); Stage 1 is closed. The
[delivery order's status](delivery-order.md#status) is the canonical status. Jettison's
[history](../../jettison/docs/history.md#neutral-sdk-and-manifest-integration) records what the
engine slices built (`Jettison::Host`: `Host::Session`, `Host::Game`, `host/Package.h`). The
evidence below describes the engine before those slices, and it is why they exist.

## Baseline and prerequisites

Historical: this is the read-only refresh of 2026-11-16, before the slices above.

- Manifest: `feature-base`, `728bb95729c41bec887569ae60334acdd7056ea0`, clean
  before this planning closeout. T10, T5, T7 and the preceding Stage 1 handoff are
  committed documentation; their implementation remains pending. The
  [T1/T9 verification record](history.md#t1t9-local-verification--2026-11-16)
  establishes the local model-library baseline, not Stage 1 readiness.
- The immutable SDK pin in `source/external/CMakeLists.txt` remains
  `f85092027eda49c29001f7589608df1f0b8827fb`. The clean sibling checkout is on
  `feature-base` at `0564f19ed41bc7e0a278952153567d4a137a2f5b`.
  Comparison found no changes under engine `source/`, `cmake/`, `assets/` or its
  top-level CMake file between these revisions. The newer boundary documentation
  has not provisioned the host or build seams described below.
- The selected revision has no `docs/game-integration.md`. The newer sibling's
  [integration contract](../../jettison/docs/game-integration.md) was consulted as
  target guidance; its presence does not change the selected SDK or prove extraction.
- Environment parity remains open. Follow the selected
  [T5 plan](build-and-gates.md#t5-selected-environment-parity-plan), preserving
  [D-006](decision-ledger.md) and [D-054](decision-ledger.md#d-054).
- No build, gate or consumer configure was run for this refresh. Source inspection
  demonstrates current coupling; runtime and configuration acceptance remain pending.

Execution proceeds through the selected [T10](build-and-gates.md#t10-selected-gate-and-promotion-policy),
[T5](build-and-gates.md#t5-selected-environment-parity-plan) and
[T7](build-and-gates.md#t7-selected-editor-example-plan) handoffs before Stage 1.
Before implementation, recheck both repositories' instructions, commits and uncommitted
work. Use their recorded final verification instead of rerunning an unchanged baseline;
establish fresh evidence when changes or unresolved failures require it. Retain the
immutable SDK pin and explicit `JETTISON_SOURCE_DIR` override throughout development.

## Evidence and design direction

Jettison already publishes `Vfs::FileSystem`, `Sim::DefinitionRegistry` and
`Sim::SimWorld`. The registry borrows its VFS; world services may retain definition
references. Their lifetimes therefore belong in an explicit host owner.

The application shell is unsuitable as Manifest's bootstrap today:

- `source/engine/app/src/Application.cpp` registers trading and tolls in its
  constructor, loads trade/trader definitions in `InitResources`, and creates sample
  traders, stations and a ship in `InitScene`.
- `source/engine/app/CMakeLists.txt` links rendering, UI, audio and GLFW alongside
  simulation services. The public `RunRenderApplication` has no game composition input.
- Private `app/src/Mounts.*` combines reusable package metadata with sample shell
  policy: mod discovery, writable user overlays, list rewriting and skipping bad mods.
  Mandatory Manifest content must fail startup when invalid; exposing this helper
  unchanged would give it the wrong failure behaviour.
- `source/test/consumer` currently exercises animation and command queue calls through
  `Jettison::Engine`; its fetch fallback follows `main`. It does not prove host lifecycle
  or consumer-scope packaging.
- Manifest's `source/external/CMakeLists.txt` accepts an existing SDK after checking
  only `Jettison::Math`, although packaging also requires `jettison_add_pack`.

Introduce a small engine host module, provisionally `Jettison::Host`, separate from the
render shell. Use existing VFS, definition and simulation implementations. Do not make
Manifest include private app headers or link the aggregate engine merely to obtain
lifecycle services. Names below describe responsibilities, not prescribed C++ signatures.

### Capability map and demonstrated gaps

This maps the existing [D-003](decision-ledger.md) and [D-005](decision-ledger.md)
boundary requirements to the inspected SDK. All additions below are pending. Paths
refer to the Jettison checkout unless prefixed with `Manifest`.

| Required capability | Existing API and evidence | Smallest provision and owner | Acceptance evidence |
| --- | --- | --- | --- |
| Explicit directory or pack mounting | Public `Vfs::FileSystem::AddMount`, `Read` and `Enumerate`; Manifest's `source/test/ModTests.cpp` compares both forms | Reuse VFS unchanged unless a concrete gap emerges; Manifest selects the mandatory source | Both forms start through the host; missing or corrupt sources fail before readiness |
| Stable host ownership and composition | Public `Sim::SimWorld` and `Sim::DefinitionRegistry` exist; the registry borrows its VFS. `App::RunRenderApplication` accepts no game composition | Jettison supplies the public CPU host owner and lifecycle hooks; Manifest supplies its adapter | Independent composition proves order, failure cleanup and repeated instances; Manifest mounts and tears down through the same API |
| Package metadata and engine requirement checking | `Serialise::YamlArchive` is public; `ModDescription`, `RegisterModTypes` and `EngineAtLeast` are private in `app/src/Mounts.*` | Jettison extracts reusable parsing/version checks; Manifest enforces identity, mandatory metadata and paths | Directory/pack metadata produce equivalent results; invalid metadata fails contextually; existing sample mod behavior remains covered |
| Neutral configuration and target closure | Top-level CMake and `source/external/CMakeLists.txt` require Vulkan; `source/CMakeLists.txt` returns when `PLUGIN_IMGUI` is off and otherwise adds assets, tools and samples | Jettison makes presentation, editor and samples optional while retaining CPU modules and the packer | A fresh CPU consumer configures with graphics discovery unavailable, builds and runs; target/header inspection shows no presentation or sample dependency |
| Explicit sample selection and resources | `app/CMakeLists.txt` owns Trading, Traders, Fleet and game screens; `Application.cpp` registers/loads/creates the sample; `assets/CMakeLists.txt` stages mixed resources | Jettison extracts an explicit sample composition and content target outside neutral engine code | Neutral startup has no sample registrations or content dependency; the explicit sample preserves affected behavior and save registrations |
| Complete consumer import contract | Manifest's external CMake guards only `Jettison::Math`; it also uses Core, VFS, warnings, shared dependencies and `jettison_add_pack` | Manifest checks its actual required targets/helpers, including the new host; Jettison keeps packaging usable independently of sample/editor targets | Complete parent SDK works; partial SDK fails at import with missing capabilities named; pinned fetch and local override both work |
| Reproducible independent consumer | `source/test/consumer/CMakeLists.txt` links `Jettison::Engine` and falls back to moving `main` | Jettison adds the tiny neutral lifecycle consumer and requires an explicit revision or local source for reproducible checks | Fetched committed SDK and explicit local source both build/run without a sample pack; the consumer actually starts and stops the host |

The pack helper is currently defined in `source/CMakeLists.txt`, with its executable
in `source/tools/pack/`; it is not a separate installed CMake package. Preserve a
consumer's staging paths and keep the packer available when editor and sample targets
are disabled. `EXCLUDE_FROM_ALL` limits what builds, but does not prevent the current
unconditional dependency discovery during configure.

The CPU consumer must not create a fake galaxy, transform or star system. Existing
`SimWorld.h` includes `Snapshot.h`, whose schema contains `InSystem`; `SaveGame.h`
also retains galaxy/system metadata. This refresh does not claim those schemas are
neutral. Inventory any such edge that blocks the minimal host, extracting only what
is needed for this stage; resolve the broader frame/save contract before Stage 3 and
the physics/AI/scene splits before Stage 4, as the boundary assessment requires.
Do not expose those space-specific fields as mandatory host options or rewrite all
spatial subsystems to deliver the startup check.

## Lifecycle and ownership contract

The host owns the VFS, definition registry and one simulation world. Game composition
is explicitly supplied and lives until teardown completes. Prefer per-session owned
composition state over callbacks capturing temporary objects.

Startup follows this order:

1. Validate startup options and mount the explicit mandatory content source read-only.
2. Register reflected types, including package metadata and game types, before decoding
   their data. Reflection registration must be safe across successive host instances;
   do not clear a process-global registry during teardown.
3. Validate mandatory package metadata and invoke the game's content-loading phase.
4. Construct the world with explicit fixed-step and seed settings; invoke game
   registration to attach components, services, commands and systems in stable order.
5. Invoke initial-state creation after registrations and definitions are available.
6. Expose readiness only when every preceding phase succeeds.

Stage 1 supplies lifecycle hooks for content and world registration, but Manifest's
content phase only verifies the package contract and its initial world is empty.
Typed economy definitions, restore flows and script startup remain later work. The
implementer may use useful scaffolding or stubs under
[D-056](decision-ledger.md#d-056); this does not require speculative save/script APIs
or establish those later capabilities as implemented.

On any startup failure, release all acquired resources and report the phase, source
path and underlying cause. Partially started composition must clean up through RAII;
it must not depend on receiving a successful-start shutdown callback. Normal shutdown
is idempotent and nonthrowing. Destroy world/services before definitions, then the VFS;
composition state captured by world callbacks must outlive the world. Specify this
ordering in the public API and verify it with lifetime probes.

Move sample registration behind an explicit sample composition used by the existing
render shell. Keep sample-only presentation bindings in that sample's adapter and sample
gameplay out of the generic host. Adapt constructor, resource and scene registration points
together so the sample cannot register systems twice. Preserve its current behaviour
and save registration order. This is a bounded extraction, not a rewrite of the frame loop.

Jetsam and sample regression harnesses explicitly select the sample composition and
pack. Flotsam's generic startup does not activate trading, traders, fleet purchasing
or sample scripts; opening the sample project is an explicit selection. Preserve the
sample's behavior in that selected mode, rather than retaining today's implicit game
in every render entry point. Generic app targets and the engine aggregate must not
acquire a dependency on sample code through the module-registration helper.

The current sample registers trading in the constructor, fleet operations after
definitions load, and traders after star-system definitions are available. Separate
reflection registration from world registration and prepare these dependencies before
their consumers run. Preserve component/service registration order and existing save
compatibility when mapping them to the host phases. In particular, `RegisterTraders`
takes its system definitions by value: moving it earlier with an empty vector would
silently disable routing. Add a regression that exercises a trader route, not just a
successful sample startup.

## Manifest bootstrap contract

Add a host adapter under `source/game/host/` and a thin executable under a separate
application source category, provisionally `source/apps/manifest/`. Publish
`Manifest::Host` separately from `Manifest::Game` to avoid an aggregate dependency cycle.
Host tests must build with `MANIFEST_BUILD_TUI=OFF` and require no terminal or display.

Proposed first CLI: `manifest --content <directory-or-pack> --check-startup`.
This finite command starts the host, reports readiness, shuts down and returns success.
Invalid arguments or failed startup return nonzero. Keep startup diagnostics on engine
logging. No arguments should produce usage, not silently locate a checkout's content.
An unattended simulation loop and signal handling can follow when there is hosted work
to advance; this milestone does not deliver a running network server.

Use the existing staged directory and `manifest.pack`. Require `mod.yaml`, the supported
metadata format, Manifest identity, a compatible engine requirement and the existing
namespaced content paths checked by `source/test/ModTests.cpp`. Reuse extracted engine
metadata parsing/version validation; keep Manifest identity and mandatory-file policy
in Manifest. The current engine version field is a minimum requirement, not a native
ABI or complete content compatibility guarantee. Do not introduce a new content
schema in this stage or claim that presence checks validate the economy configuration.
Preserve existing metadata YAML keys and reflected names when extracting the parser;
moving `Shell.ModDescription` is not a reason to invalidate current sample metadata.
Do not infer Manifest identity from the caller's directory or archive filename.

Archive decoding alone is insufficient: omitted fields keep their default values,
and the sample mount policy allows descriptions without an engine requirement.
Manifest must explicitly reject absent or empty required identity/engine fields
before readiness, using the existing authored metadata contract. Any declared package
dependency must be satisfied by the explicitly selected content; this bootstrap must
fail an unmet dependency rather than discover another package or ignore the declaration.
Read required content files through the VFS so unreadable directory entries or damaged
required pack payloads fail as well as missing paths. This verifies access to those
bytes; semantic YAML/economy validation still belongs to Stage 2.

Mount only the explicitly requested package for this bootstrap. Do not discover user
mods, create writable overlays or unpack archives into temporary directories. Both
forms must work from an unrelated working directory when given an absolute content
path. Installed asset discovery and full distribution relocation belong to stage 5.

## CMake contract and delivery slices

1. **Baseline and consumer contract.** Consume the completed maintenance handoffs and
   their verification evidence. Enumerate required SDK targets and helpers at the import boundary;
   reject a partial parent-provided SDK with an actionable configure error. Exercise
   consumer-local paths for `jettison_add_pack`. Replace reliance on the consumer's
   moving `main` fallback with an explicit revision/local-source test setup. Check the
   current capabilities first; require the new host target only when Manifest begins
   consuming the revision that provides it.
2. **Engine lifecycle and mandatory package primitives.** Add the minimal public host,
   extract reusable metadata validation and add lifecycle/failure tests. Provision the
   CPU build options and independent consumer in this slice, retaining the packer with
   presentation/editor/samples disabled. Link only actual CPU-side dependencies and
   prove the consumer configuration before proceeding to sample integration.
3. **Sample integration.** Route existing sample gameplay registration through explicit
   composition, preserve ordering and behaviour, and run affected sample/render checks.
4. **Manifest adapter and executable.** Implement the finite bootstrap, directory/pack
   integration tests and TUI-independent host test target against the local SDK override.
5. **Pin and document.** Commit and make the validated engine revision fetchable before
   updating Manifest's immutable pin. Keep all three environment files identical to
   that revision, verify the fetched SDK path and update architecture/build/roadmap docs
   with actual evidence. Commit Manifest's adapter/import requirements with the SDK
   update that satisfies them; do not land a bootstrap that requires a local override.
   Leave stages 2–5 marked pending.

Use reviewable commits with green required gates. Engine and Manifest changes must be
delivered together as a compatible pair; a local override alone is not the final result.

Runtime and configuration independence from graphics are required for the neutral CPU
consumer. Jettison currently requires Vulkan/glslc at configure time and adds presentation
modules unconditionally; implement a bounded build-options change alongside the host seam.
Keep the render sample's dependencies explicit. Do not describe an Xvfb check or a CPU-only
executable as proof of a graphics-free SDK configure. Further generic physics/space-adapter
extraction follows the sequencing in the boundary assessment.

## Acceptance and validation handoff

The implementer runs and records these checks; none are reported as passing by this plan.

| Boundary | Required evidence |
| --- | --- |
| Lifecycle | Ordered phases; readiness only after success; injected failure in each phase releases acquired resources; repeated shutdown; successive independent host instances, including a successful instance after a failed startup. No later hook runs after failure. |
| Ownership | World callbacks/services cannot outlive captured state or definitions; definitions cannot outlive their VFS; sanitizer coverage of teardown and failed startup. |
| Game selection | A minimal test composition creates an empty world with no sample trading systems or entities; Jetsam/sample harnesses select the sample explicitly; generic Flotsam startup has no sample gameplay. Sample save registrations and an actual trader route remain covered. |
| Content | Directory and pack both start and shut down; missing source, corrupt pack index or required payload, malformed metadata, unsupported metadata version, absent/empty required fields, incompatible engine requirement, wrong identity, unmet package dependency and missing/unreadable required path fail contextually. |
| CLI | Bad arguments fail; startup-check exits finitely; both content forms run from an unrelated working directory without a display or terminal. |
| Neutral configuration | Fresh build tree, presentation/editor/samples disabled, ImGui disabled and Vulkan/glslc unavailable to discovery; CPU consumer and packer build without sample assets. Inspect public headers and transitive target dependencies as well as the executable. |
| Independent consumer | A tiny non-space composition loads a fixture definition through the engine registry, registers its own component/system, steps deterministically, round-trips world state and tears down; no galaxy, transform or sample registrations are required. This is an API probe, not Manifest gameplay or a new save contract. |
| Consumer build | Pinned fetch without the sibling checkout; explicit local override; parent-provided complete SDK; clear rejection of incomplete SDK; packaging with tests/TUI disabled. |
| Regression | Existing Manifest suites, affected engine host/app/mount suites, SDK consumer executable and sample/render checks; preserve sample save and registration behaviour. |

Use each repository's presets and required `./scripts/ci.sh --werror --asan` gate in
the locked environment. Follow [D-051](decision-ledger.md#d-051) for machine-specific
concurrency and the [build/gate policy](build-and-gates.md) for optional local tidy
and the selected T10 ordering. Follow the engine review workflow within the user's
planning-only model policy: review may be read-only, while fixes and gate execution
belong to the selected implementer. Do not rerun unrelated checks repeatedly once
passing evidence covers the final changes.

Record engine and Manifest commits, SDK pin and environment parity, exact configure
options, target names, commands, exit results and log paths for the final pair. Distinguish
the graphics-free consumer check from render smoke tests under Xvfb. Verify the final
Manifest fetch without a sibling override after the engine commit is available remotely;
passing only against a dirty local engine does not complete delivery.

## Implementation completion and next step

Stage 1 is complete when the public lifecycle, sample integration and Manifest bootstrap
pass the matrix above against the committed SDK pin. A mounted package and an empty
world are bootstrap evidence, not playable gameplay.

Planning closes with this handoff; do not mark Stage 1 implemented until the final
committed pair has the required evidence. Record results in
[session handoff](session-handoff.md), then update the stage index and roadmap with
the verified capabilities and remaining limitations.

The next planning task is stage 2: loader inventory, typed definition mapping, atomic
validation, schema/layering policy, content fingerprint and the small scenario. Preserve
price-only Decimal, floating quantities and whole-credit ledgers throughout.

For implementation, begin with delivery slice 1. The remaining design freedom is exact
public type/function naming and the smallest sample extraction that preserves its
registration order; ownership, phase ordering and failure semantics above are constraints.
