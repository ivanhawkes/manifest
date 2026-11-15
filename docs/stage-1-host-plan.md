# Stage 1: public host and standalone bootstrap

Planning handoff for Claude, prepared 2026-11-15. This refines stage 1 of
[development stages](development-stages.md); it does not mark implementation complete.
OpenAI work in this session was repository inspection and this planning document only.

Apply the later [engine/game boundary assessment](engine-game-boundary-assessment.md).
The host must be genre-neutral; engine-local gameplay stays in explicit test/sample
targets. A neutral CPU consumer and presentation-free configuration are required alongside
the space sample, not deferred solely to a future server milestone.

## Baseline and prerequisites

- Manifest: `feature-base`, `2716380` (merge labelled as WIP with unknown build status).
  The working tree was clean before this document was added.
- Jettison: `feature-base`, `826c4bec125b6631a6ba8ee822c90463316c3511`, clean.
  Its tracked difference from Manifest's pinned SDK
  `f85092027eda49c29001f7589608df1f0b8827fb` is confined to `.gitignore`.
- Manifest's `devenv.nix` adds `lazygit` relative to that engine checkout;
  `devenv.yaml` and `devenv.lock` match. The user confirmed that `lazygit` is required
  on development machines for inspecting changes in the terminal. Retain it as a
  development-shell tool, with no runtime or distribution dependency. Reconcile the
  shared environment by adding it to the selected engine SDK's development shell,
  then synchronising Manifest's environment files with that committed SDK revision.
- No builds or gates were run for this plan. Historical gate results in
  [session handoff](session-handoff.md) do not validate the current merge.

Before editing, read both repositories' instructions, recheck their state, reconcile
environment parity and establish the current gate baseline. Record any pre-existing
failure separately from stage 1 changes. Retain the immutable SDK pin and explicit
`JETTISON_SOURCE_DIR` override throughout development.

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
Typed economy definitions, restore flows and script startup remain later work. Do not
invent placeholder save/script APIs to fill future slots in the interface.

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

Mount only the explicitly requested package for this bootstrap. Do not discover user
mods, create writable overlays or unpack archives into temporary directories. Both
forms must work from an unrelated working directory when given an absolute content
path. Installed asset discovery and full distribution relocation belong to stage 5.

## CMake contract and delivery slices

1. **Baseline and consumer contract.** Align the shared development environment while
   retaining `lazygit` as described above, and validate the
   merged baseline. Enumerate required SDK targets and helpers at the import boundary;
   reject a partial parent-provided SDK with an actionable configure error. Exercise
   consumer-local paths for `jettison_add_pack`. Replace reliance on the consumer's
   moving `main` fallback with an explicit revision/local-source test setup.
2. **Engine lifecycle and mandatory package primitives.** Add the minimal public host,
   extract reusable metadata validation and add lifecycle/failure tests. Link only its
   actual CPU-side dependencies. Do not import sample trading by default.
3. **Sample integration.** Route existing sample gameplay registration through explicit
   composition, preserve ordering and behaviour, and run affected sample/render checks.
4. **Manifest adapter and executable.** Implement the finite bootstrap, directory/pack
   integration tests and TUI-independent host test target against the local SDK override.
5. **Pin and document.** Commit and make the validated engine revision fetchable before
   updating Manifest's immutable pin. Keep all three environment files identical to
   that revision, verify the fetched SDK path and update architecture/build/roadmap docs
   with actual evidence. Leave stages 2–5 marked pending.

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
| Lifecycle | Ordered phases; readiness only after success; injected failure in each phase releases acquired resources; repeated shutdown; successive independent host instances. |
| Ownership | World callbacks/services cannot outlive captured state or definitions; definitions cannot outlive their VFS; sanitizer coverage of teardown and failed startup. |
| Game selection | A minimal test composition creates an empty world with no sample trading systems or entities; sample composition preserves existing regression behaviour. |
| Content | Directory and pack both start and shut down; missing source, corrupt pack, malformed metadata, unsupported metadata version, incompatible engine requirement, wrong identity and missing required path fail contextually. |
| CLI | Bad arguments fail; startup-check exits finitely; both content forms run from an unrelated working directory without a display or terminal. |
| Consumer build | Pinned fetch without the sibling checkout; explicit local override; parent-provided complete SDK; clear rejection of incomplete SDK; packaging with tests/TUI disabled. |
| Regression | Existing Manifest suites, affected engine host/app/mount suites, SDK consumer executable and sample/render checks; preserve sample save and registration behaviour. |

Use each repository's presets and required `./scripts/ci.sh --werror --asan` gate in
the locked environment. Set compile and CTest parallelism to two; avoid local clang-tidy.
Follow the engine review workflow within the user's planning-only model policy:
review may be read-only, while fixes and gate execution belong to Claude. Do not rerun
unrelated checks repeatedly once passing evidence covers the final changes.

## Completion and next step

Stage 1 is complete when the public lifecycle, sample integration and Manifest bootstrap
pass the matrix above against the committed SDK pin. A mounted package and an empty
world are bootstrap evidence, not playable gameplay.

The next planning task is stage 2: loader inventory, typed definition mapping, atomic
validation, schema/layering policy, content fingerprint and the small scenario. Preserve
price-only Decimal, floating quantities and whole-credit ledgers throughout.

For implementation, begin with delivery slice 1. The remaining design freedom is exact
public type/function naming and the smallest sample extraction that preserves its
registration order; ownership, phase ordering and failure semantics above are constraints.
