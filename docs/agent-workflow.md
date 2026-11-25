# Models and agent workflow

Who may do what on Manifest, how work is planned, and how a session ends. The rules apply to every
agent, primary or sub-agent, whatever its model, effort or configured role.

## Planning-only policy

The user's policy, recorded on 2026-11-15: **OpenAI models are for planning only. They must not
implement project work.**

OpenAI models may read the repository and existing logs to inform architecture, task breakdowns,
acceptance criteria, validation plans and handoffs to an implementer. When explicitly asked, they
may write or update planning and workflow documentation, this policy included. Such a request does
not authorise implementation.

OpenAI models must not write or modify implementation code, tests, fixtures, assets, build scripts
or configuration. Fixes, refactors, mechanical migrations, implementation delegation, integration
and running builds or gates belong to the implementer. A small or routine change is still
implementation; a lower effort or a cheaper OpenAI model does not permit it.

Implementation is done by the user or by a non-OpenAI implementer the user selects. This policy
selects no replacement. An implementation request to an OpenAI planning agent produces a plan and
handoff, not a patch.

## Implementer discretion to use stubs

The implementer may write stubs when they judge them useful ([D-056](decision-ledger.md#d-056)):
to scaffold an interface, isolate work or integrate in steps. Unfinished behaviour is marked
clearly and follows the engine's [error rules](../../jettison/docs/coding-standards.md#errors): an
unsupported production operation fails explicitly rather than report a made-up success. Test
doubles stay explicit test dependencies.

A stub does not meet the dependency or acceptance criterion of the behaviour it stands in for.
Dependent work may be built against the provisional interface, but is not accepted as integrated
until the real prerequisite and its checks pass. The handoff lists the stubs left and the tasks
that replace them. This permission does not relax the planning-only policy or widen scope.

## Planning integration work

Before planning work that spans the engine and the game, read the [task queue](delivery-order.md),
the feature document the work touches (listed in the [index](README.md)), the
[architecture](architecture.md) and Jettison's
[game integration](../../jettison/docs/game-integration.md). Record a small capability map: the
mechanic Manifest needs, the public engine API that serves it, the gap if one is shown, its owner
and the acceptance evidence. Jettison provisions reusable missing capabilities in engine code;
Manifest supplies the game's rules, schemas and content. Keep proposed APIs apart from built ones.

A handoff names the agreed design, the files or modules affected, the constraints, the acceptance
criteria, the checks required and the open questions. The implementer applies the change, reviews
its integration and runs the checks and the final [gate](build-and-gates.md#the-gate).

<a id="planning-discipline-after-the-2026-11-16-analysis"></a>

### Planning discipline

These rules came out of the [2026-11-16 analysis](history/project-analysis-2026-11-16.md):

- **Decisions are recorded once.** Selecting, changing or superseding a decision updates the
  [decision ledger](decision-ledger.md) in the same change: its ID, canonical section, status and
  owner. Other documents link the ID rather than restate the rule. Tuning values are authored data
  under `data/`, not prose.
- **Scope enters through a funnel.** A new mechanic is queued only with its
  [task ID](glossary.md#task-id), the public SDK API it consumes and the gap it shows, if any. A
  product pillar beyond the queue is recorded as deferred with a condition for taking it up, not
  as next work.
- **Gate claims name the stages that ran.** State which presets and stages ran. A Debug-only pass
  is not a gate, and a CI run is never gate evidence: CI stages and smoke-tests the bundle only
  ([verification evidence](build-and-gates.md#verification-evidence),
  [D-116](decision-ledger.md#d-116)).
- **Externally reported identifiers need a second source.** Tool output here has produced CI run
  IDs and commit SHAs that do not exist
  ([CI failure analysis](history/project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)).
  Confirm any run ID, commit SHA, push result or gate claim against `gh api`, `git ls-remote`,
  `FETCH_HEAD` or the remote's commit list before reporting it, and take CI counts from a live
  run list. Never write an unverified identifier into the documents, and never tell the user work
  may be lost unless the remote's commit list agrees.

Planning never marks work done without a named verifying suite and a recorded gate.

## Existing roles and delegation

`.codex/agents/` defines five OpenAI-backed roles, and `.codex/config.toml` sets their defaults.
They are configuration, not permission to implement:

| Role | Its configured scope | Allowed use |
| --- | --- | --- |
| `manifest_scout` | Inspect code, engine APIs or logs and return evidence | Read-only, in support of planning |
| `manifest_reviewer` | Review a bounded diff | Read-only, in support of planning |
| `manifest_maintainer` | Scoped documentation, fixture or mechanical changes | Not for its implementation scopes |
| `manifest_small_fix` | Fix a small reproduced issue | Do not invoke |
| `manifest_worker` | Implement one bounded module | Do not invoke |

The policy is an instruction, not an enforced filesystem sandbox.

Delegate only a bounded, independent, read-only investigation, when a small brief and concise
findings save the primary's context or an expensive model's work. Mapping loaders or reading a
failure log can qualify; a single search usually does not. Give the question and the relevant
paths, ask for concise findings with file references, avoid duplicate investigations, and reuse a
worker for a related follow-up.

Use at most two sub-agents at once. They do not delegate further unless the primary assigns it.
The primary keeps the architectural decisions and writes the handoff. Do not promise lower total
tokens or guaranteed savings.

## Session context at task completion

End every completed task with a short recommendation, and its reason:

- **Continue** when the next change needs the current reasoning and the session is still focused.
- **Compact** when related work continues but old searches, build output and settled issues fill
  the history (`/compact`).
- **Fresh session** when the task is done and the next can start from the documents and a short
  handoff (`/clear` or `/new`).

Advise; do not compact or clear automatically. Before advising a fresh session, give the branch and
commit, uncommitted changes, decisions taken, checks run, limits and the next task, so the user can
paste the handoff into the new session. Do not commit merely to prepare it.

Compacting the conversation is not flushing the server's prompt cache. Recommend from continuity
and the next task's needs; do not claim to measure or flush the cache, invent context-usage
figures, or guarantee savings. Use `/status` for context usage where it is available.
