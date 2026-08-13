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

## Architecture authority

`ARCHITECTURE_LOCKED_V1.md` is the authoritative architecture contract for this project.

Before implementing or materially changing a subsystem, read the relevant portions of `ARCHITECTURE_LOCKED_V1.md`.

Decisions marked `LOCKED` are hard architectural constraints.

Do not silently replace a locked mechanism with an easier alternative.

If implementation evidence suggests a locked decision should change, stop and clearly propose an architecture revision containing:

- the current decision,
- the proposed replacement,
- why the current design is problematic,
- benefits and drawbacks,
- migration cost,
- affected subsystems.

Do not implement the architectural change until it has been explicitly accepted.

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

## Completion report

When finishing non-trivial implementation work, report:

- files changed,
- important design decisions,
- tests/checks run,
- sanitizer results when applicable,
- benchmarks run when applicable,
- known limitations or deferred work.
