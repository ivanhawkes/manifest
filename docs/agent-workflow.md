# Models and agent workflow

## Planning-only policy

The user's policy, recorded on 2026-11-15, is: **OpenAI models are for planning only.
They must not implement project work.** This applies to the primary agent and every
sub-agent, regardless of model, effort or configured role. It supersedes the previous
OpenAI implementation recommendations in this document and `AGENTS.md`.

OpenAI models may inspect the repository and existing logs read-only to inform architecture,
task breakdowns, acceptance criteria, validation plans and implementation handoffs. When
explicitly requested, they may write or update planning and workflow documentation, including
this policy. Such a documentation request does not authorize project implementation.

OpenAI models must not write or modify implementation code, tests, fixtures, assets, build
scripts or configuration. Fixes, refactors, mechanical migrations, implementation delegation,
integration and build/gate execution belong to the implementer. A small or routine change is
still implementation; lowering effort or choosing a cheaper OpenAI model does not permit it.

Implementation is performed by the user or a non-OpenAI implementer selected by the user.
This policy does not select a replacement provider, model or tool. An implementation request
to an OpenAI planning agent produces a plan and handoff under this policy, not a patch.

## Roadmap planning

Before planning an integration, consult [development stages](development-stages.md),
[gameplay requirements](gameplay-and-server-plan.md) and the
[engine/game boundary assessment](engine-game-boundary-assessment.md). Record a small
capability map: required Manifest mechanic, existing public engine API, demonstrated gap,
owner, and acceptance evidence. Jettison implementers must inspect Manifest's current
requirements and provision reusable missing capabilities in engine code; Manifest
implementers supply the game's rules, schemas and content. Consult the selected SDK's
`docs/game-integration.md` when available. Keep proposed APIs distinct from implemented ones.
The planning-only policy above applies to this cross-repository work too.

| Work | Planning output |
| --- | --- |
| 1. Host boundary and script capabilities | State ownership, host/script contracts, capability limits and acceptance criteria |
| 2. Decimal market prices | Precision, overflow and rounding rules, affected-field inventory and migration plan |
| 3. VFS, layered definitions and schema migration | Loader inventory, layering and migration semantics, compatibility and validation plan |
| 4. Fixed-step commands, saves, replay and hashing | Ordering and state contracts, save format decisions, replay and determinism checks |
| 5. Scripts, staging and an engine-hosted scenario | Capability/ABI design, packaging requirements and scenario acceptance criteria |

A handoff should name the agreed design, affected files or modules, constraints, acceptance
criteria, required checks and unresolved questions. The implementer applies changes, reviews
integration and runs the relevant checks and final gate. Local builds remain limited to two
compile jobs; local clang-tidy remains opt-in as described in [build and gates](build-and-gates.md).

### Planning discipline after the 2026-11-16 analysis

Two rules bind planning edits, from the [project analysis](project-analysis-2026-11-16.md):

- **Decisions are recorded once.** Selecting, changing or superseding a decision updates the
  [decision ledger](decision-ledger.md) with its ID, single canonical section, status, owner and
  stage in the same change. The roadmap, `scope.md`, `architecture.md`, `development-stages.md` and
  `session-handoff.md` link the ID rather than restating the rule, and numeric balance values are
  not written into prose; they are authored data (handoff T4) referenced from the ledger.
- **Scope enters through a funnel.** A new mechanic is scheduled only with its stage, the public
  SDK API it consumes, and the demonstrated gap when one exists. Product pillars beyond the current
  sequence are recorded as deferred with a re-entry condition, not as next work. The first playable
  loop needs stages 1–4: a new pillar may not displace Stage 1.

- **Gate claims name the stages that ran.** Before planning, implementing or reporting validation,
  read the "Current verification and CI status" section of `AGENTS.md` and
  [CI failure analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16). State which
  presets and stages ran, and never treat a CI run as Release or sanitiser evidence while the gate
  aborts in tidy, nor a Debug-only pass as a full gate.

- **Externally-reported identifiers need a second source.** Tool output in this project has emitted
  CI run records and commit SHAs that resolve to nothing on GitHub (see the data-quality note in
  [CI failure analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)). Confirm any
  run ID, commit SHA, push result or gate claim against `gh api`, `git ls-remote`, `FETCH_HEAD` or the
  remote's own commit list before reporting it, and take CI counts from a live paginated run list
  rather than from recalled output. Never write an unverified identifier into project documentation,
  and never tell the user work may have been lost without the remote's commit list agreeing.

Planning output never marks work `implemented` without a named verifying suite and a recorded gate.

## Existing roles and delegation

The definitions under `.codex/agents/` and defaults in `.codex/config.toml` are existing
OpenAI-backed configuration, not authorization to implement. Do not invoke
`manifest_maintainer`, `manifest_small_fix` or `manifest_worker` for their implementation
scopes. Read-only scout or review work is allowed only when it supports planning; it must
not become delegated execution or patch production. The policy is an instruction, not an
enforced filesystem sandbox.

Delegate only a bounded, independent, read-only planning investigation when a small brief
and concise findings reduce primary context or expensive-model work. Mapping loaders or
examining an existing failure log can qualify; a single search usually does not. Prefer fresh
context, specify the question and relevant paths, and request concise findings with file
references. Avoid duplicate investigations and reuse a worker for related follow-up.

Use at most two sub-agents concurrently. They must not delegate further unless the primary
explicitly assigns further delegation. The primary retains architectural decisions and
prepares the handoff. Sub-agents add context and coordination overhead; do not promise lower
aggregate tokens or guaranteed savings.

## Session context at task completion

End every completed roadmap task with a short context recommendation:

- **Continue** when the next change needs the current reasoning and the session remains focused.
- **Compact** when related work continues but old searches, build output and resolved issues
  dominate the history. In Codex CLI, `/compact` summarises the conversation while preserving
  useful continuity.
- **Fresh session** when the task is complete and the next milestone can start from repository
  documentation plus a short handoff. In Codex CLI, `/clear` or `/new` starts a fresh chat.

Give the reason and advise rather than executing these actions automatically. Before advising
a fresh session, provide the branch/commit, uncommitted changes, important decisions, completed
checks, limitations and the next task so the user can paste the handoff into the new session.
Do not commit changes merely to prepare that handoff.

Conversation compaction is distinct from flushing the server-managed KV/prompt cache. Cache
reuse can reduce input cost, while compaction and a fresh chat may incur summary or context
reconstruction work. Recommend based on continuity and expected future context needs, without
claiming guaranteed savings or inventing unavailable usage metrics. Use context usage from
`/status` if it is available; the user can supply it when the agent cannot inspect it.
See the official [Codex commands](https://learn.chatgpt.com/docs/developer-commands?surface=cli)
and [prompt-caching guidance](https://developers.openai.com/api/docs/guides/prompt-caching).
