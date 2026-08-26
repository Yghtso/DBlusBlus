# DBlusBlus Project Instructions

## Project purpose

DBlusBlus is a from-scratch relational database implemented in C++20.

The primary goals are:

1. learn practical database internals deeply,
2. implement the important mechanisms ourselves,
3. preserve correctness and architectural clarity,
4. pursue performance deliberately and measurably.

This is not a project where minimizing implementation effort is the primary goal.

Prefer an implementation that exposes an important database or systems mechanism over a shortcut that hides it, provided the resulting design remains realistically implementable.

## Repository documentation and authority

The project uses distinct documentation roles. Keep their purposes separate.

### `docs/ARCHITECTURE.md`

`docs/ARCHITECTURE.md` is the authoritative architecture contract for this project.

It defines what the system is intended to be, including locked architectural decisions, persisted formats, subsystem responsibilities, invariants, and cross-subsystem constraints.

Keep `docs/ARCHITECTURE.md` maximally time-independent. It may contain intended behavior,
persisted formats, invariants, responsibilities, concurrency protocols, supported or
unsupported semantics, and durable technical rationale. It must not contain implementation
progress, dates, milestone numbers, review or audit history, statements that a decision was
recently resolved, implementation completion status, latest test results, or
machine/toolchain validation snapshots.

When an architecture revision is accepted, replace the canonical architectural rule.
Historical rationale about the change belongs in `devlog/` or an explicitly archival review
artifact. Technical rationale explaining why the resulting rule exists may remain in the
architecture document.

Before implementing or materially changing a subsystem, read the relevant portions of `docs/ARCHITECTURE.md`.

`MUST` / `MUST NOT` requirements, exact persistent-format values, and explicitly stated invariants are hard architectural constraints.

Do not silently replace an architectural mechanism with an easier alternative.

If implementation evidence suggests an accepted architecture decision should change, stop and clearly propose an architecture revision containing:

- the current decision,
- the proposed replacement,
- why the current design is problematic,
- benefits and drawbacks,
- migration cost,
- affected subsystems.

Do not implement the architectural change until it has been explicitly accepted.

Do not modify `docs/ARCHITECTURE.md` merely to make the implementation match a local choice. Architecture synchronization must reflect an explicitly accepted architectural decision.

### `docs/DEVELOPMENT.md`

`docs/DEVELOPMENT.md` preserves durable development-environment requirements, setup and
build/tooling workflows, implementation sequencing, dependency-driven milestone structure,
development practices, and recommended module-layout guidance.

It is planning guidance, not architecture authority and not current project state.

It does not authorize crossing an explicit phase/subsystem gate.

Keep stable minimum requirements and intentionally supported tool ranges there. Do not turn
it into an environment-validation diary containing dated successful builds, whichever
compiler or distribution was most recently installed, latest test counts, one-off
sanitizer outcomes, task completion reports, or project-progress narration.

### `docs/VERIFICATION.md`

`docs/VERIFICATION.md` preserves test obligations and strategies, deterministic scenarios,
crash-injection, fuzzing, property and regression procedures, benchmark methodology,
expected invariant checks, and stable verification commands.

Architecture-level correctness/performance obligations remain authoritative in `docs/ARCHITECTURE.md`.

Detailed procedures may evolve as tooling improves, provided they continue to verify the architecture contract.

Do not use `docs/VERIFICATION.md` as a rolling results dashboard. Latest pass/fail results,
test-run dates, mutable test counts, benchmark measurements, machine/compiler snapshots,
and historical regression stories belong in `devlog/`, task reports, CI artifacts, or
dedicated benchmark records. A regression case itself may remain when it defines a durable
required test; the history of when or where the bug was found does not.

### `docs/PROJECT_STATE.md`

`docs/PROJECT_STATE.md` describes the current implementation state of the project.

It records implemented capabilities, unimplemented or deferred capabilities, important
active implementation boundaries, known limitations, unresolved architecture questions,
and meaningful current architecture/implementation mismatches.

It is not an architecture authority.

If `docs/PROJECT_STATE.md` and `docs/ARCHITECTURE.md` disagree about intended system behavior, `docs/ARCHITECTURE.md` wins.

Use `docs/PROJECT_STATE.md` to understand present implementation capabilities and boundaries
before starting substantial work.

Update `docs/PROJECT_STATE.md` when its current description becomes false or materially
incomplete, for example when:

- a capability described as absent becomes implemented,
- an important limitation is removed or introduced,
- a dependency or authorization boundary changes,
- an architecture/implementation mismatch appears or disappears,
- an unresolved architecture question is introduced or resolved.

Do not update it merely because a milestone number increased, a task completed without
changing its descriptive facts, another successful test run occurred, another
machine/compiler was validated, or a review was performed.

Keep `docs/PROJECT_STATE.md` state-aware rather than history-aware. It must not accumulate
dates, “Last updated” fields, milestone indexes or histories, completed-task narratives,
old phase statuses, previous validation results, chronological fix sequences, or lists of
historical audits and reviews. Exact test counts belong elsewhere unless a count is
intrinsically necessary to describe a present invariant rather than progress.

When updating it:

- describe current facts,
- remove or replace stale current-state claims,
- rewrite the relevant description to the resulting state instead of adding a dated
  correction or another historical layer,
- keep unresolved architectural proposals clearly identified as unresolved,
- do not turn it into a second architecture specification,
- do not add agent instructions or workflow rules to it.

When a development gate matters, state the active boundary and authorization requirement
directly. Avoid narrating phase transitions, milestone completion history, or what happens
“next.” Phase numbering may remain in `docs/DEVELOPMENT.md` where it provides useful
dependency structure, but it must not become a progress diary and explicit subsystem gates
must not be weakened.

### `devlog/`

`devlog/` is the primary explicitly chronological documentation area and contains
append-only historical engineering records.

Completed non-trivial implementation milestones should create a new numbered Markdown entry under `devlog/`.

Do not rewrite, delete, or retroactively clean up older entries unless explicitly requested.

A devlog entry may record the temporal and task-specific facts established by completed
work, such as:

- date and milestone/task number,
- milestone/task name,
- scope,
- before/after descriptions,
- files changed,
- architecture sections used,
- important implementation decisions,
- public API or persisted-format details when relevant,
- compiler/tool versions and portability-machine details,
- exact tests, sanitizer runs, and benchmark measurements,
- bugs discovered or fixed,
- historical architecture or review references,
- assumptions,
- limitations at that point,
- deferred work,
- architecture questions discovered.

Do not use a devlog entry as architecture authority.

Do not use devlogs as the primary source for current architecture, semantics, capabilities,
or project status. Consult them when historical context, rationale, provenance, or
milestone-specific evidence is relevant; live documentation and source/tests must be
sufficient to understand the resulting project without replaying devlogs.

### Reading workflow

Before substantial implementation work:

1. read this `AGENTS.md`,
2. read `docs/PROJECT_STATE.md` to understand present implementation capabilities and boundaries,
3. read the relevant sections of `docs/ARCHITECTURE.md`,
4. consult `docs/DEVELOPMENT.md` when implementation sequencing/module guidance is relevant,
5. consult `docs/VERIFICATION.md` when test/benchmark procedures are relevant,
6. inspect the current task-relevant source code and tests,
7. read task-relevant devlogs when historical context is useful.

Do not assume documentation is sufficient evidence for implementation behavior. Inspect current source and tests before editing existing subsystems.

When a task reveals a discrepancy:

- `docs/ARCHITECTURE.md` determines intended architecture,
- current source/tests determine implementation reality,
- `docs/PROJECT_STATE.md` should be rewritten if its current-state description is stale;
  do not append a dated correction note,
- devlogs remain unchanged historical records.

Do not reconstruct current behavior by replaying devlogs. Current descriptive
documentation together with source and tests should be sufficient for current work.

## Documentation style and temporality

Live documentation describes the project as it is defined or structured, not the history
of how it reached that state. Make maintained descriptive documents as time-independent,
analytical, descriptive, and canonical as practicable.

### Live documents are canonical snapshots

For live documents such as `README.md`, `docs/ARCHITECTURE.md`,
`docs/DEVELOPMENT.md`, `docs/VERIFICATION.md`, `docs/PROJECT_STATE.md`, and
`AGENTS.md`, prefer:

- present-tense descriptions,
- stable analytical explanations,
- canonical current rules,
- current implementation characteristics,
- durable procedures,
- explicit supported or deferred capabilities,
- stable dependency and responsibility descriptions.

Do not use live documents as chronological journals. When project facts change, update or
replace the existing description so it states the new canonical result. Do not append
another historical layer. Except for `devlog/` and explicitly archival artifacts, live
documentation is not append-only: when a documentation task authorizes the scope, replace
stale descriptions, merge redundant sections, remove obsolete situational wording, and
reorganize material for clarity. Git and `devlog/` preserve history.

### Situational chronology is excluded by default

Unless a document's semantic purpose genuinely requires it, do not add:

- document-history dates, “Last updated” timestamps, “as of” dates, or references to
  today or recent events;
- prose about “this task,” “this review,” “this pass,” what was just changed, or
  before/after task narration;
- milestone completion history, numbered milestone timelines, phase-transition history,
  or statements about which step happens next;
- old or latest test counts, one-off validation results, or rolling pass/fail summaries;
- one-machine environment snapshots, installed compiler/tool versions, or transient
  sanitizer outcomes;
- lists of files changed by completed work;
- audit/fix issue identifiers used as explanatory history.

These facts belong in `devlog/` when preservation is useful. This rule concerns document
history and project-progress chronology; it does not prohibit dates intrinsic to external
semantics or data, such as SQL `DATE` behavior.

Apply semantic judgment rather than mechanically banning words such as “current,”
“previous,” “next,” “future,” “version,” “generation,” or “epoch.” They may have precise
technical meanings, as in “current transaction,” “previous WAL record,” “next sibling
page,” or “future format version.” A concise present-state fact such as an unsupported
capability may be appropriate in `docs/PROJECT_STATE.md`; a narrative that a milestone
made the project ready for the next phase is not.

### Stable contracts versus temporal evidence

Stable requirements belong in the appropriate analytical document. For example, C++20,
the CMake minimum encoded by the project, the Ninja generator requirement, and the
Clang/GCC support expectation belong in `docs/DEVELOPMENT.md`.

One-machine observations belong in `devlog/`, task completion reports, CI artifacts, or
purpose-built result records. This includes distribution and kernel snapshots, exact local
compiler versions, exact test counts and pass results, ptrace-specific LeakSanitizer
outcomes, and other dated portability evidence. Do not accumulate “verified environment”
sections in `docs/DEVELOPMENT.md` unless the user explicitly establishes a maintained
compatibility matrix as a project contract.

Exact test counts and pass results are normally temporal evidence.
`docs/VERIFICATION.md` states what must be tested and how; it does not act as a rolling
test dashboard. `docs/PROJECT_STATE.md` may state that a subsystem has meaningful
verification coverage when that is a useful present characteristic, but should not list
rolling test totals.

Similarly, `docs/VERIFICATION.md` owns benchmark methodology and required measurements,
while dated measurements and machine configurations belong in `devlog/` or dedicated
benchmark records. `docs/ARCHITECTURE.md` may contain intentionally selected performance
constants only when they are genuine architectural decisions.

### Analytical rationale and update method

Distinguish historical rationale from technical rationale:

- “We changed X during review Y” is historical provenance for `devlog/` or review history.
- “X is required because otherwise crash scenario Y violates invariant Z” is technical
  rationale for the appropriate analytical document.

Do not remove useful technical rationale merely to make a document timeless. When choosing
between logging that a change happened and explaining the resulting mechanism, invariant,
dependency, tradeoff, or procedure, live documentation should prefer the analytical
explanation. Improve the canonical explanation when implementation knowledge increases;
do not append a dated discovery note. Analytical documents should become more useful over
time, not merely longer.

After implementation work:

1. update live documents only when their canonical descriptive content changed;
2. rewrite the affected description to reflect the resulting project;
3. omit task chronology from that rewrite;
4. place detailed completion history and evidence in the new devlog when one is required.

For example:

| Avoid in live documentation | Prefer |
|---|---|
| `Last updated: <date>` or `Current milestone: <number>` | `Implemented storage capabilities: ...` |
| `Clang <local-version> passed <count>/<count> tests on <distro>.` | `Clang is the primary development compiler; GCC is used for portability verification.` |
| `Milestone <number> added BufferPool.` | `BufferPool owns resident-page caching, pins, replacement, and dirty-page management.` |
| `Phase 1 finished, so Phase 2 is next.` | `BufferPool implementation is authorization-gated; HeapFile depends on BufferPool-managed page lifetime.` |

### README, AGENTS, archives, and exceptions

Keep `README.md` as stable orientation: project purpose, major characteristics,
prerequisites, a basic build entry point, and documentation links. Avoid current-milestone
banners, latest test totals, dated portability results, upcoming implementation steps, or
development-diary prose unless the user explicitly requests a maintained status section.

Keep `AGENTS.md` itself focused on durable contributor and Codex behavior. Do not add a
current milestone number, date, latest test count, local tool versions, or transient
implementation status unless needed to enforce an explicit user-authorized gate.

Historical review folders, archived architecture material, and other explicitly archival
artifacts may naturally contain dates, issue IDs, old states, and chronology. Release
notes, changelogs, migration records, external-standard dates, and similar artifacts may
also contain temporal information when chronology is intrinsic to their purpose. Do not
clean archival material merely to enforce this live-document rule unless the user
explicitly requests archival cleanup, and do not use these exceptions to justify ordinary
progress logging in live documents.

## Development philosophy

Implement the database incrementally from the bottom upward.

Prefer small, testable milestones over implementing many incomplete subsystems simultaneously.

Do not create speculative abstractions for features that do not yet exist.

Do not create large collections of empty classes, interfaces, directories, or placeholder implementations merely because the architecture mentions future components.

Create abstractions when the current implementation requires them.

Prefer explicit systems code over hidden framework behavior.

Avoid dependencies that implement database mechanisms this project is intended to learn.

Examples of mechanisms that must not be delegated to external libraries include:

- page management,
- buffer replacement,
- tuple storage,
- B+ trees,
- MVCC,
- transaction locking,
- WAL and recovery,
- query execution,
- relational optimization.

Small general-purpose development/testing libraries may be used when explicitly approved.

## C++ standard

Use C++20.

Do not depend on compiler extensions unless explicitly justified.

Production code should compile with both:

- Clang
- GCC

Clang is the primary development compiler.

## C++ style

Prefer explicit ownership and lifetimes.

Use RAII for resource ownership.

Prefer value semantics where appropriate.

Use smart pointers only when ownership semantics require them; do not replace simple scoped objects with heap allocation unnecessarily.

Avoid raw owning pointers.

Non-owning pointers/references are allowed when lifetime is clear.

Avoid global mutable state.

Avoid exceptions as an ordinary control-flow mechanism in hot database internals.

Do not use C++ iostreams in performance-sensitive database paths.

Avoid per-row and per-cell heap allocation in execution paths.

Do not use generic `std::variant`/type-erased value objects inside performance-critical tuple/vector loops when typed representations are available.

Prefer fixed-width integer types for persisted formats and identifiers.

Never serialize C++ structs by writing their raw memory representation to persistent storage.

Persistent data formats must use explicit encoding/decoding.

Do not rely on compiler struct padding, native alignment, or host endianness for persisted data.

## Performance

Correctness comes first, but performance-sensitive design must remain visible.

Do not introduce performance complexity based solely on intuition.

For meaningful performance-sensitive changes:

1. establish or identify a benchmark,
2. make the change,
3. compare measurements.

Prefer contiguous memory, batching, and predictable access patterns where appropriate.

Avoid unnecessary allocation, copying, pointer chasing, and synchronization in hot paths.

Do not micro-optimize code that has not been identified as important.

## Concurrency

Transaction locks and internal latches are distinct concepts and must remain distinct in the implementation.

Do not hold a page latch, B+ tree structural latch, or similar short-lived internal latch while waiting for a transaction-level logical lock.

Do not introduce broad global locks in hot paths merely to simplify concurrency.

When concurrency correctness is uncertain, favor a simpler correctly-latched implementation before attempting optimistic/lock-free behavior.

## Error handling

Internal invariants should fail loudly in debug/testing builds.

External/runtime failures must use explicit error propagation.

Do not silently ignore:

- I/O failures,
- short reads/writes,
- corrupted persistent data,
- invalid page formats,
- WAL corruption,
- transaction conflicts.

Assertions must not be the only protection for errors that can occur with valid external input or persistent corruption.

## Testing

Every new subsystem should have focused unit tests before large integration tests depend on it.

Prefer deterministic tests.

Add randomized/property-style tests for data structures and storage algorithms where valuable.

Important storage/index/transaction components should eventually include:

- boundary tests,
- randomized tests,
- restart/reopen tests,
- corruption/error tests where applicable,
- concurrency tests where applicable.

Run the smallest relevant tests first.

Before declaring work complete, run the appropriate broader preset when reasonable.

Do not claim sanitizer safety unless the sanitizer build was actually run.

## Benchmarks

Do not mix correctness tests and benchmarks.

Benchmarks belong under `benchmarks/`.

Release/optimized builds should be used for meaningful performance measurements.

Do not draw conclusions from a single noisy benchmark execution.

## Build system

Use CMake and Ninja.

Keep build logic target-oriented.

Prefer:

- `target_compile_options`
- `target_link_options`
- `target_include_directories`
- `target_compile_definitions`

over global compiler/linker flags where practical.

Do not modify global system compiler configuration.

Do not fetch or install dependencies automatically without explicit approval.

Do not add dependencies through CMake FetchContent without explicit approval.

## Tooling

The primary editor is Neovim.

Keep the project compatible with clangd.

Maintain `compile_commands.json` generation.

Do not introduce editor-specific project files that are required to build the project.

`clang-format` defines mechanical C++ formatting.

`clang-tidy` defines project static-analysis checks.

Do not perform repository-wide formatting while implementing an unrelated change.

## Generated files

Do not manually edit generated build files.

Do not commit:

- build directories,
- compiler-generated files,
- sanitizer output,
- test temporary databases,
- spill files,
- generated compilation databases.

## Git discipline

Inspect `git status` before substantial work.

Do not commit unless explicitly requested.

Do not rewrite history.

Do not discard unrelated working-tree changes.

Before completing a task, inspect the final diff and ensure only intentional files changed.

## Scope

Implement only the requested milestone.

Do not opportunistically begin later database subsystems.

If a clean implementation requires preparatory work, keep it minimal and explain why it is necessary.

Respect explicit phase or subsystem gates recorded in the current project state and architecture. Do not bypass them by implementing the same responsibility through a lower layer or under a different name.

If the requested work depends on a later gated subsystem, report the dependency instead of silently crossing the boundary.

## Completion report

When finishing non-trivial implementation work, report:

- files changed,
- important design decisions,
- tests/checks run,
- sanitizer results when applicable,
- benchmarks run when applicable,
- architecture questions discovered,
- whether `docs/PROJECT_STATE.md` was updated and why,
- the new devlog entry created.

The completion report is situational conversation, not canonical repository
documentation. It may report task results, exact checks, sanitizer or benchmark outcomes,
and files changed. Do not copy it verbatim into analytical live documents.

## Milestone documentation workflow

For a completed non-trivial implementation milestone:

1. create a new numbered Markdown entry under `devlog/`,
2. keep all older devlogs unchanged,
3. report any architecture questions rather than silently editing architectural contracts,
4. update analytical live documents only when their canonical descriptive content has
   changed,
5. rewrite affected live descriptions to the resulting state without milestone/date/test
   chronology,
6. update `docs/PROJECT_STATE.md` only when its present capability, limitation, boundary,
   mismatch, or open-question description has become false or incomplete.

A milestone devlog should be created as part of the completed task unless the task is explicitly too small/non-milestone in nature or the user explicitly requests otherwise.

Milestone existence alone does not require a `docs/PROJECT_STATE.md` update, and
`docs/PROJECT_STATE.md` must not maintain a milestone index. When its current description
does need correction, it may be updated directly without a separate architectural decision
because it records implementation facts rather than defining architecture.

Changes to `docs/ARCHITECTURE.md` follow the architecture-authority rules above and must not be inferred merely from implementation choices.
