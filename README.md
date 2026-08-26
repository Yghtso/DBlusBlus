# DBlusBlus

DBlusBlus is a from-scratch, single-node relational database project implemented in
C++20 for Linux/POSIX environments. Its purpose is to make practical database internals
understandable by implementing the important storage, transaction, execution, and
optimization mechanisms directly rather than delegating them to a database framework.

The project combines correctness-focused systems engineering with deliberate performance
measurement. Its architecture is designed to remain small enough to study end to end while
still representing realistic database responsibilities and invariants.

## Goals and design philosophy

- **Mechanism transparency:** page management, replacement, tuple storage, indexing,
  concurrency control, recovery, execution, and optimization are intended to remain
  visible as they are implemented.
- **Correctness and explicit invariants:** persistent formats, I/O failures, concurrency,
  transaction semantics, and recovery behavior are governed by explicit contracts.
- **Architectural clarity:** subsystem ownership and dependency direction remain clear,
  with lower layers independent of higher-level SQL concerns.
- **Measured performance:** cache behavior, allocation, synchronization, I/O patterns,
  cardinality, and intermediate-result size are treated as measurable engineering
  concerns.
- **Durable foundations:** simple implementations are preferred when they satisfy the
  architecture without turning foundational subsystems into throwaway work.

## System characteristics

The authoritative v1 architecture specifies:

- a Linux-first C++20 platform using POSIX file APIs;
- page-oriented storage with 8 KiB pages, row-oriented heap tables, and explicit
  little-endian persistent encoding;
- a database-managed buffer pool with CLOCK replacement;
- page-backed B+ tree indexes;
- heap-version MVCC with transaction locking;
- write-ahead logging, group commit, and crash recovery under STEAL/NO-FORCE;
- a relational catalog and limited SQL front end;
- typed, vectorized query execution;
- statistics, cardinality estimation, and cost-based physical optimization.

These are architectural design characteristics, not a claim that every subsystem is
already implemented. See [`PROJECT_STATE.md`](docs/PROJECT_STATE.md) for implemented
capabilities, limitations, and active development boundaries.

The architecture follows a strict downward dependency direction:

```text
SQL / Parser
    ↓
Binder + Catalog
    ↓
Logical Plan
    ↓
Optimizer
    ↓
Physical Plan
    ↓
Execution Engine
    ↓
Tables / Indexes / Transactions
    ↓
Buffer Pool
    ↓
WAL + Page Manager
    ↓
Operating System / Storage Device
```

## Build

After installing the required development tools, configure, build, and test the primary
Clang Debug preset from the repository root:

```sh
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

See the [development guide](docs/DEVELOPMENT.md#development-baseline) for tool
requirements, Linux package setup, all build presets, sanitizers, static analysis,
formatting, benchmarks, clangd integration, and troubleshooting.

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — authoritative technical architecture,
  persisted formats, subsystem contracts, and invariants.
- [`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md) — implementation capabilities,
  limitations, and active boundaries.
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — development environment, build and
  tooling workflow, implementation sequencing, and module organization.
- [`docs/VERIFICATION.md`](docs/VERIFICATION.md) — testing, crash-injection, fuzzing,
  regression, and benchmark methodology.
- [`devlog/`](devlog/) — chronological implementation milestones and task-specific
  engineering records.
