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

`docs/DEVELOPMENT.md` preserves implementation sequencing, milestone targets, and recommended module-layout guidance.

It is planning guidance, not architecture authority and not current project state.

It does not authorize crossing an explicit phase/subsystem gate.

### `docs/VERIFICATION.md`

`docs/VERIFICATION.md` preserves detailed test, crash-injection, fuzzing, regression, and benchmark procedures.

Architecture-level correctness/performance obligations remain authoritative in `docs/ARCHITECTURE.md`.

Detailed procedures may evolve as tooling improves, provided they continue to verify the architecture contract.

### `docs/PROJECT_STATE.md`

`docs/PROJECT_STATE.md` describes the current implementation state of the project.

It records what has been implemented, what remains deferred, current phase/subsystem status, important current boundaries, open architecture follow-ups, and the latest validated project checkpoint.

It is not an architecture authority.

If `docs/PROJECT_STATE.md` and `docs/ARCHITECTURE.md` disagree about intended system behavior, `docs/ARCHITECTURE.md` wins.

Use `docs/PROJECT_STATE.md` to understand where development currently stands before starting substantial work.

Update `docs/PROJECT_STATE.md` when the completed work materially changes the current project state, for example when:

- a milestone completes or materially changes a subsystem,
- a phase or major implementation boundary changes,
- previously deferred functionality becomes implemented,
- an important limitation is removed or introduced,
- an open architecture question is resolved,
- the validated test/tooling checkpoint changes materially.

Do not update it for trivial edits that do not change the meaningful current state.

Keep `docs/PROJECT_STATE.md` as a current-state document rather than an accumulating historical log. Historical detail belongs in `devlog/`.

When updating it:

- describe current facts,
- remove or replace stale current-state claims,
- keep unresolved architectural proposals clearly identified as unresolved,
- do not turn it into a second architecture specification,
- do not add agent instructions or workflow rules to it.

### `devlog/`

`devlog/` contains append-only historical engineering records.

Completed non-trivial implementation milestones should create a new numbered Markdown entry under `devlog/`.

Do not rewrite, delete, or retroactively clean up older entries unless explicitly requested.

A devlog entry records facts established by that completed task, such as:

- milestone/task name,
- scope,
- files changed,
- architecture sections used,
- important implementation decisions,
- public API or persisted-format details when relevant,
- verification performed,
- assumptions,
- known limitations,
- deferred work,
- architecture questions discovered.

Do not use a devlog entry as architecture authority.

Do not use devlogs as the primary source for current project status when `docs/PROJECT_STATE.md` already summarizes it. Consult older devlogs when historical context, rationale, or milestone-specific detail is relevant.

### Reading workflow

Before substantial implementation work:

1. read this `AGENTS.md`,
2. read `docs/PROJECT_STATE.md` to understand current implementation status and phase boundaries,
3. read the relevant sections of `docs/ARCHITECTURE.md`,
4. consult `docs/DEVELOPMENT.md` when implementation sequencing/module guidance is relevant,
5. consult `docs/VERIFICATION.md` when test/benchmark procedures are relevant,
6. inspect the current task-relevant source code and tests,
7. read task-relevant devlogs when historical context is useful.

Do not assume documentation is sufficient evidence for implementation behavior. Inspect current source and tests before editing existing subsystems.

When a task reveals a discrepancy:

- `docs/ARCHITECTURE.md` determines intended architecture,
- current source/tests determine implementation reality,
- `docs/PROJECT_STATE.md` should be corrected if its current-state description is stale,
- devlogs remain unchanged historical records.

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

## Milestone documentation workflow

For a completed non-trivial implementation milestone:

1. create a new numbered Markdown entry under `devlog/`,
2. keep all older devlogs unchanged,
3. report any architecture questions rather than silently editing architectural contracts,
4. update `docs/PROJECT_STATE.md` when the milestone materially changes the current project state,
5. ensure documentation reflects the final implementation and verification results.

A milestone devlog should be created as part of the completed task unless the task is explicitly too small/non-milestone in nature or the user explicitly requests otherwise.

`docs/PROJECT_STATE.md` may be updated directly when the need is clear from the completed work. Its update does not require a separate architectural decision because it records current implementation facts rather than defining architecture.

Changes to `docs/ARCHITECTURE.md` follow the architecture-authority rules above and must not be inferred merely from implementation choices.
