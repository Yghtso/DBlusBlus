# Performance-First Relational Database

A single-node relational database management system built from first principles. The primary goal of this project is architectural: bridging the gap between relational database theory and a practical, highly performant engine. 

This repository serves as a rigorous implementation of modern database internals, prioritizing high-learning-value designs over convenient shortcuts.

Fresh Linux toolchain, build, test, analysis, and benchmark instructions are in the [development guide](docs/DEVELOPMENT.md#development-baseline).

## Architectural Overview

The database is designed with a strict, performance-first layered architecture. Lower layers (like storage and indexing) are completely decoupled from higher-level SQL concepts.

* **Platform:** Linux-first (x86-64 / ARM64), developed entirely in **C++20**.
* **Storage Layer:** Page-oriented storage (8 KiB slotted pages) with row-oriented heap tables.
* **Buffer Pool:** Explicit database-managed buffer pool using a CLOCK replacement policy, strictly enforcing Write-Ahead Logging (WAL) rules.
* **Indexing:** Custom page-backed B+ Trees serving as the primary general-purpose index.
* **Concurrency Control:** Heap-version MVCC (Multi-Version Concurrency Control) supporting Read Committed and Snapshot Isolation (Repeatable Read).
* **Durability:** Mandatory append-only WAL with group commit and ARIES-inspired crash recovery (utilizing STEAL + NO-FORCE policies).
* **Execution Engine:** Vectorized, chunk-at-a-time execution model (defaulting to 1024 rows/chunk) using typed vectors to maximize CPU cache efficiency and avoid per-cell allocations.
* **Query Optimizer:** Rule-based logical optimization paired with a System-R-style cost-based physical optimizer (bottom-up dynamic programming).

## System Layering

The system strictly adheres to the following dependency direction. Lower layers never depend on or have knowledge of the layers above them.

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

## Engineering Philosophy

* **Correctness before micro-optimization:** Implement the simplest version compatible with the architecture, test it heavily, benchmark it, and optimize only based on exact measurements.
* **Explicit Control:** Avoid OS-level shortcuts. The database explicitly controls caching, eviction, dirty-page flushing, and WAL-before-data ordering (using explicit `pread`/`pwrite`/`fdatasync`, avoiding `mmap` for primary data structures).
* **Performance Model:** Execution and storage paths are designed to explicitly respect CPU cache behavior, branch prediction, allocation frequency, and sequential vs. random I/O.
* **Explicit Serialization:** Persistent on-disk formats are explicitly serialized. The codebase never depends on compiler struct memory layouts for persistence.

## Implementation Roadmap

The implementation is broken down into distinct, strictly testable phases:

1. **Raw Storage:** File management, slotted heap pages, and tuple encoding.
2. **Buffer Management:** Frame tracking, page guards, and CLOCK replacement.
3. **Indexes:** B+ tree pages, latch coupling, splits/merges, and range scans.
4. **Transactions & Durability:** Transaction manager, MVCC visibility, WAL, group commit, crash recovery, and vacuuming.
5. **Catalog & SQL:** Handwritten parser, AST, semantic binder, and system catalog.
6. **Query Execution:** Logical plans, vectorized data chunks, hash joins, and aggregation.
7. **Optimization:** Statistics, rule rewrites, access-path selection, and cost models.
8. **Performance:** Profiling, allocation reduction, and parallel execution.

---
*Note: The architecture outlined in the underlying specifications acts as a strict contract. Any structural deviations from the locked architecture require explicit proposals and benchmarked justifications.*
