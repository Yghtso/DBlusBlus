# DBlusBlus Project Instructions

## Project purpose

DBlusBlus is a from-scratch relational database implemented in C++20. Its goals are to:

1. learn practical database internals deeply,
2. implement the important mechanisms ourselves,
3. preserve correctness and architectural clarity,
4. pursue performance deliberately and measurably.

Minimizing implementation effort is not the goal. Prefer an implementation that exposes an
important database or systems mechanism over a shortcut that hides it, as long as the design
stays realistically implementable.

## Documents and their authority

Each document has one job. Keep their purposes separate.

| Document | Role |
|---|---|
| `docs/ARCHITECTURE.md` and `docs/architecture/` | The authoritative architecture contract. `ARCHITECTURE.md` is the index; each chapter is a file under `docs/architecture/`. Section numbers (`§4.13.3`) are stable and are how code and documents cite it. |
| `docs/PROJECT_STATE.md` | What is implemented now: capabilities, limits, active boundaries, open architecture questions, and architecture/implementation mismatches. Not an authority. |
| `docs/DEVELOPMENT.md` | Toolchain requirements, build and tooling workflows, implementation order, module layout and code conventions. Planning guidance, not an authority; it does not open a gated phase. |
| `docs/VERIFICATION.md` and `docs/verification/` | Test obligations and procedures, crash injection, fuzzing, property and regression procedures, and benchmark methodology. The architecture stays the authority for what must hold. |
| `devlog/` | Append-only, numbered history of completed work. Never an authority. |
| `docs/reviews/` | Index of archived review material, which lives at a git tag. |

The documentation also builds as an mdBook (`mdbook build docs`); see `docs/README.md`.

When documents disagree, `docs/ARCHITECTURE.md` decides intended behavior and the source and
tests decide implementation reality. Fix a stale `docs/PROJECT_STATE.md` by rewriting the
affected description, not by appending a correction. Devlogs stay unchanged.

### Architecture rules

- Read the relevant architecture chapters before implementing or materially changing a
  subsystem.
- `MUST` / `MUST NOT` requirements, exact persistent-format values and stated invariants are
  hard constraints. Do not silently replace an architectural mechanism with an easier one.
- If implementation evidence suggests an accepted decision should change, stop and propose an
  architecture revision: the current decision, the replacement, why the current design is a
  problem, benefits and drawbacks, migration cost, and affected subsystems. Do not implement it
  until it is explicitly accepted.
- Do not edit the architecture to match a local implementation choice. When a revision is
  accepted, replace the canonical rule; the history of the change goes in `devlog/`, while
  technical rationale for the resulting rule may stay.
- The architecture contains no implementation progress, dates, milestone numbers, review
  history, test results or toolchain snapshots.

### PROJECT_STATE rules

Update `docs/PROJECT_STATE.md` when its description becomes false or materially incomplete:
a capability appears or disappears, a limitation or boundary changes, a mismatch appears or
is resolved, or an architecture question opens or closes. Do not update it only because work
finished, tests ran or a review happened. Describe the resulting state; do not add dates,
"last updated" fields, milestone histories, test counts, or agent workflow rules, and do not
turn it into a second architecture specification. Mark unresolved proposals as unresolved.
State an active gate and its authorization requirement directly instead of narrating what
comes next. It records facts, so it can be corrected without an architecture decision.

### Devlog rules

A completed non-trivial milestone gets a new numbered entry under `devlog/`, unless the task
is too small or the user says otherwise. Never rewrite older entries unless asked. An entry may
hold anything temporal about the work: date, scope, before and after, files changed,
architecture sections used, decisions, API or format details, tool versions, exact test,
sanitizer and benchmark results, bugs found, assumptions, limitations, deferred work and open
architecture questions. Live documents plus source and tests must be enough to understand the
project without replaying devlogs.

## Writing live documentation

Live documents (`README.md`, `AGENTS.md`, and everything under `docs/` except `reviews/`)
describe the project as it is, not how it got there. Write in the present tense and replace
stale text instead of adding layers; git and `devlog/` keep the history. When a task
authorizes it, merge redundant sections and reorganize for clarity.

Keep out of live documents (put them in `devlog/` if worth keeping):

- dates, "last updated" or "as of" notes, and references to recent events;
- talk about "this task", "this review" or what just changed;
- milestone timelines, phase-transition history, or what happens next;
- test counts, pass/fail results, one-machine environment details, tool versions and
  sanitizer outcomes;
- lists of changed files and audit or fix issue identifiers.

Use judgment, not word bans: "current transaction", "previous WAL record" and "future format
version" are technical terms. Dates that belong to the domain, such as SQL `DATE` behavior,
are fine.

Stable requirements go in the document that owns them, for example C++20, the CMake minimum,
Ninja, and Clang/GCC support in `docs/DEVELOPMENT.md`. Do not grow "verified environment"
sections unless the user makes a compatibility matrix a project contract.
`docs/VERIFICATION.md` says what to test and how, not latest results; measurements go in
`devlog/` or dedicated benchmark records. A regression case may stay there when it defines a
durable required test, but not the story of when the bug was found. Phase numbering may stay
in `docs/DEVELOPMENT.md` where it shows dependencies, as long as it does not become a progress
diary or weaken a gate. The architecture may fix performance constants only
when they are real architectural decisions.

Keep technical rationale ("X is required because crash scenario Y would break invariant Z")
and drop historical provenance ("we changed X in review Y"). Documents should get more useful
over time, not just longer.

| Avoid | Prefer |
|---|---|
| `Last updated: <date>` or `Current milestone: <n>` | `Implemented storage capabilities: ...` |
| `Clang <version> passed <n>/<n> tests on <distro>.` | `Clang is the primary development compiler; GCC is used for portability verification.` |
| `Milestone <n> added BufferPool.` | `BufferPool owns resident-page caching, pins, replacement, and dirty-page management.` |
| `Phase 1 finished, so Phase 2 is next.` | `BufferPool implementation is authorization-gated; HeapFile depends on BufferPool-managed page lifetime.` |

`README.md` is stable orientation: purpose, main characteristics, prerequisites, how to build,
and documentation links. `AGENTS.md` holds durable contributor and agent behavior only; add
status to it only to enforce a gate the user authorized. Archived reviews, changelogs and
similar records may contain chronology; do not clean them up unless asked.

## Workflow

Before substantial work:

1. read this file,
2. read `docs/PROJECT_STATE.md`,
3. read the relevant architecture chapters,
4. consult `docs/DEVELOPMENT.md` for sequencing, layout and code conventions, and
   `docs/VERIFICATION.md` for test and benchmark procedures,
5. inspect the relevant source and tests (documentation is not evidence of behavior),
6. read relevant devlogs when history helps.

After implementation work:

1. update live documents only where their content changed, rewriting to the resulting state;
2. report architecture questions instead of editing the architecture;
3. add a devlog entry for a non-trivial milestone.

## Scope and gates

- Build bottom-up in small, testable milestones. Implement only the requested milestone and do
  not start later subsystems.
- Keep preparatory work minimal and explain why it is needed.
- Respect phase and subsystem gates recorded in the project state and architecture. Do not
  bypass a gate by implementing the same responsibility in a lower layer or under another
  name. If the work depends on a gated subsystem, report the dependency.
- No speculative abstractions, empty classes, placeholder directories or interfaces for
  components that do not exist yet.
- Prefer explicit systems code over hidden framework behavior. Do not delegate the mechanisms
  this project exists to learn (page management, buffer replacement, tuple storage, B+ trees,
  MVCC, locking, WAL and recovery, query execution, optimization) to external libraries. Small
  general-purpose development and testing libraries need explicit approval.

## C++

- C++20, no compiler extensions without justification. Production code compiles with Clang
  (primary) and GCC.
- Explicit ownership and lifetimes; RAII; value semantics where appropriate; no raw owning
  pointers; smart pointers only when ownership needs them. Non-owning pointers and references
  are fine when the lifetime is clear. No global mutable state.
- No exceptions as ordinary control flow in database internals, no iostreams on
  performance-sensitive paths, no per-row or per-cell heap allocation in execution, and no
  generic `std::variant` or type-erased values in tuple or vector loops when typed
  representations exist.
- Fixed-width integers for persisted formats and identifiers. Persistent formats use explicit
  encoding and decoding; never write raw struct memory or rely on padding, alignment or host
  endianness.
- Follow the error-handling, namespace and identifier conventions in `docs/DEVELOPMENT.md`.

## Errors

- Runtime failures use explicit error results. Never silently ignore I/O failures, short reads
  or writes, corrupt persistent data, invalid page formats, WAL corruption or transaction
  conflicts.
- Internal invariants fail loudly. An assertion is never the only protection against
  something valid external input or persistent corruption can cause.

## Concurrency

- Transaction locks and internal latches are different concepts and stay different in code.
- Never hold a page latch, B+ tree structural latch or similar short-lived latch while waiting
  for a transaction lock.
- No broad global locks on hot paths just to simplify concurrency. When correctness is
  uncertain, prefer a simpler correctly latched design before optimistic or lock-free ones.

## Performance

Correctness first, but keep performance-sensitive structure visible. Do not add complexity on
intuition alone: for a performance-sensitive change, identify or build a benchmark, make the
change, and compare. Prefer contiguous memory, batching and predictable access; avoid needless
allocation, copying, pointer chasing and synchronization on hot paths. Do not micro-optimize
code that has not been shown to matter.

## Testing and benchmarks

- Each new subsystem gets focused unit tests before integration tests depend on it. Prefer
  deterministic tests; add randomized or property tests for data structures and storage
  algorithms where useful. Storage, index and transaction components should grow boundary,
  randomized, restart, corruption and concurrency tests as they apply.
- Run the smallest relevant tests first and a broader preset before calling work complete.
  Do not claim sanitizer safety without running the sanitizer build.
- Benchmarks live under `benchmarks/`, separate from correctness tests, run on optimized
  builds, and are never judged from one noisy run.

## Build and tooling

- CMake and Ninja, with target-oriented build logic (`target_compile_options` and friends over
  global flags). Do not change the system compiler configuration.
- Do not fetch or install dependencies, or add `FetchContent` dependencies, without explicit
  approval.
- The primary editor is Neovim: keep clangd working and `compile_commands.json` generated, and
  add no editor files that the build needs.
- `clang-format` defines formatting and `clang-tidy` defines static analysis. Do not reformat
  the repository during an unrelated change.
- Never hand-edit or commit generated files: build directories, compiler output, sanitizer
  output, test databases, spill files or compilation databases.

## Git

- Check `git status` before substantial work and review the final diff before finishing.
- Do not commit unless explicitly asked. Do not rewrite history or discard unrelated changes.

## Completion report

For non-trivial work, report the files changed, important design decisions, tests and checks
run, sanitizer and benchmark results when applicable, architecture questions found, whether
`docs/PROJECT_STATE.md` changed and why, and the new devlog entry. The report is conversation,
not documentation; do not copy it into live documents.
