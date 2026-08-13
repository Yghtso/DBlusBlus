# Performance-First Relational Database — Architecture Contract

**Status:** LOCKED v1  
**Purpose:** This file is the architectural source of truth for the database implementation.  
**Audience:** Project owner + Codex/AI coding agents + future contributors.

---

## 0. How Codex should use this file

Treat decisions marked **LOCKED** as hard constraints.

When implementing a feature:

1. Prefer the architecture in this file over locally convenient shortcuts.
2. Do not silently replace a locked design with an easier design.
3. If a locked decision becomes clearly harmful, stop and propose an explicit architecture change with:
   - the current decision,
   - the proposed replacement,
   - advantages,
   - disadvantages,
   - migration cost,
   - affected subsystems.
4. Keep lower layers independent of higher layers.
5. Prefer implementations that expose real database-system concepts over wrappers that hide them.
6. Optimize only after correctness exists, but design hot paths so they can become fast without being rewritten from scratch.
7. Add benchmarks for performance-sensitive components.
8. Keep persistent on-disk formats explicitly serialized; never depend on compiler struct layout.

This project intentionally prefers **high-learning-value implementations** over the shortest implementation.

---

# 1. Project Goal

Build a serious **single-node relational database management system** from first principles.

Primary goals:

- understand how relational database theory maps to a real engine,
- build the core storage engine ourselves,
- implement transactions, MVCC, WAL, recovery, indexes, and query execution,
- implement a real query optimizer,
- study memory layout, caching, concurrency, and I/O behavior,
- make performance measurable and architecturally important.

The system should eventually support a meaningful SQL subset and concurrent transactions while remaining small enough for one person to understand end-to-end.

---

# 2. Non-Goals

The first major version will NOT target:

- distributed consensus,
- replication,
- sharding,
- multi-node execution,
- cloud-native storage,
- full PostgreSQL/MySQL SQL compatibility,
- stored procedures,
- triggers,
- user-defined extensions,
- sophisticated authentication/authorization,
- columnar storage as the primary table format,
- JIT query compilation,
- GPU execution.

These can be explored only after the single-node engine is mature.

---

# 3. Architectural Principles

## 3.1 Layering

Dependency direction:

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

Lower layers must not depend on SQL-layer concepts.

Examples:

- the B+ tree must not know what a SQL table is,
- the buffer pool must not know what a tuple schema means,
- WAL must not know what a `SELECT` statement is,
- the parser must not know how pages are stored.

---

## 3.2 Correctness before micro-optimization

For each subsystem:

1. implement the simplest version compatible with the final architecture,
2. test it heavily,
3. benchmark it,
4. optimize based on measurements.

Avoid throwaway architectures that require a complete rewrite for performance later.

---

## 3.3 Performance model

Performance work should explicitly consider:

- CPU cache behavior,
- branch prediction,
- allocation frequency,
- data layout,
- synchronization,
- sequential vs random I/O,
- buffer-pool hit rate,
- WAL/fsync frequency,
- query cardinality,
- intermediate-result size.

---

# 4. Platform

## LOCKED: Linux-first

Initial supported environment:

- Linux
- x86-64 or ARM64
- POSIX file APIs
- single process
- multiple worker threads

Portability is desirable but secondary.

Rationale:

- exposes real OS I/O primitives,
- makes profiling easier,
- enables later experiments with `io_uring`, direct I/O, huge pages, NUMA, etc.

---

# 5. Implementation Language

## LOCKED: C++20

Selected decision:

```text
A1 = C++20
```

### Why C++20 was selected

**Pros**

- excellent fit for low-level database implementation,
- direct control over memory layout and allocation,
- easy access to raw bytes, atomics, cache-line-aware structures, and POSIX APIs,
- many database engines and educational projects use C/C++,
- forces understanding of ownership and lifetime at the systems level,
- minimal abstraction between code and machine behavior,
- strong profiling/tooling ecosystem.

**Cons**

- memory-safety bugs,
- undefined behavior,
- accidental copies/allocations can be subtle,
- concurrency bugs can be difficult,
- language complexity can distract from database concepts.

**Learning profile:** maximum exposure to classical systems/database engineering.

### Locked rationale

C++20 was selected because the primary goal is to learn practical database internals directly:

- memory layout,
- object lifetime,
- allocation behavior,
- cache locality,
- atomics and synchronization,
- explicit byte manipulation,
- POSIX/Linux I/O,
- low-level profiling and optimization.

The extra risk of memory-safety and undefined-behavior bugs is accepted as part of the systems-learning goal.

---

# 6. Persistent Storage Model

## LOCKED: page-oriented storage

All persistent database structures are built from fixed-size pages.

Initial page size:

```text
8192 bytes (8 KiB)
```

Page size must be represented as a configuration/constant rather than scattered assumptions.

Use explicit page IDs.

Suggested representation:

```text
PageId {
    FileId file;
    uint64_t page_no;
}
```

No persistent raw pointers.

---

# 7. File Organization

## LOCKED: multiple database files

Use separate files for major persistent objects rather than one monolithic database file.

Examples:

```text
catalog.meta
table_<id>.heap
index_<id>.btree
database.wal
```

Why:

- easier inspection while learning,
- clearer ownership,
- simpler debugging,
- page IDs naturally become `(file_id, page_number)`,
- relation/index lifecycle is explicit,
- WAL remains independently append-only.

A later storage manager may add file segmentation.

---

# 8. Disk / I/O Layer

## LOCKED: explicit I/O, not mmap

Initial storage access should use explicit file operations such as:

```text
pread
pwrite
fdatasync/fsync
```

Do not use memory-mapped database pages as the primary architecture.

Reason:

the database should explicitly control:

- caching,
- eviction,
- dirty-page flushing,
- WAL-before-data ordering,
- prefetching,
- page lifetime.

### Later experiments

After the engine works:

- `io_uring`,
- asynchronous prefetch,
- direct I/O,
- background writeback,
- Linux page-cache bypass.

---

# 9. On-Disk Serialization

## LOCKED: explicit binary encoding

Never write C++/Rust structs directly to disk.

Persistent structures must define:

- byte order,
- field widths,
- offsets,
- version numbers,
- checksums where appropriate.

Initial on-disk integer encoding:

```text
little-endian
```

Every page type should have a format version.

---

# 10. Table Storage

## LOCKED: row-oriented heap tables

Primary table storage is row-oriented.

Reason:

- appropriate for general-purpose/OLTP work,
- natural fit for tuple-level MVCC,
- good learning path for page organization,
- natural interaction with B+ tree indexes.

Columnar storage can be a later independent project.

---

# 11. Heap Page Layout

## LOCKED: slotted pages

Each heap page contains:

```text
┌────────────────────────────┐
│ Page Header                │
├────────────────────────────┤
│ Slot Directory             │
│ ...                        │
├────────────────────────────┤
│ Free Space                 │
├────────────────────────────┤
│ Tuple Bytes                │
│ ...                        │
└────────────────────────────┘
```

Tuple identifier:

```text
RID = (PageId, SlotId)
```

The RID should remain stable when tuples are compacted within a page.

Page header should eventually include at least:

- page type,
- format version,
- page LSN,
- slot count,
- free-space boundaries,
- flags,
- checksum field if enabled.

---

# 12. Tuple Representation

## LOCKED: compact binary tuples

Avoid heap allocation per column/value inside stored tuples.

Schema determines how tuple bytes are interpreted.

Initial supported scalar types:

- BOOLEAN
- INT32
- INT64
- FLOAT64
- DATE
- TIMESTAMP
- VARCHAR
- NULL

Use a null bitmap.

Variable-length data should use offsets into tuple-local variable data where practical.

Very large values/overflow pages are deferred until the basic engine is stable.

---

# 13. Free-Space Management

## LOCKED: explicit free-space metadata

Do not linearly scan every heap page to find insertion space.

Start with a simple free-space map or bucketed free-space directory.

It can be improved later, but the architecture must treat free-space discovery as its own subsystem.

---

# 14. Buffer Pool

## LOCKED: explicit database-managed buffer pool

Execution/storage code obtains pages through the buffer pool.

A frame tracks at least:

```text
page_id
page_bytes
pin_count
dirty
page_lsn
latch
replacement_metadata
```

Required operations conceptually:

```text
FetchPage(PageId)
NewPage(FileId)
UnpinPage(PageId, dirty)
FlushPage(PageId)
FlushFile(FileId)
DeletePage(PageId)
```

Prefer RAII/page guards so pin/unpin mistakes are difficult.

---

# 15. Buffer Replacement

## LOCKED: CLOCK initially

Use a CLOCK-family replacement policy for the first implementation.

Why:

- simpler than production-grade adaptive policies,
- much cheaper bookkeeping than exact LRU,
- good learning value,
- realistic enough for meaningful benchmarks.

Later experiments:

- CLOCK-Pro,
- LRU-K,
- 2Q,
- scan-resistant policies.

Replacement policy should be behind an interface so alternatives are benchmarkable.

---

# 16. Indexing

## LOCKED: B+ tree as the primary general-purpose index

Implement a page-based B+ tree ourselves.

Required eventual features:

- lookup,
- insert,
- delete,
- page split,
- page merge/rebalance,
- range scan,
- sibling-linked leaves,
- unique indexes,
- non-unique indexes,
- concurrent traversal.

Leaf payload:

```text
index_key -> RID
```

For non-unique keys:

```text
(index_key, RID)
```

should provide deterministic ordering.

---

# 17. B+ Tree Concurrency

## LOCKED: page latches separate from transaction locks

Short-lived in-memory synchronization:

```text
latches
```

Transactional logical protection:

```text
locks
```

These are different systems.

Initial concurrent B+ tree writes should use latch coupling/crabbing.

More optimistic algorithms can be explored later.

---

# 18. MVCC

## LOCKED: Heap-version MVCC

Selected decision:

```text
B1 = heap-version MVCC
```

### Architecture

Updates create new tuple versions in heap storage.

Conceptually:

```text
old version:
  xmin = T1
  xmax = T7

new version:
  xmin = T7
  xmax = INF
```

**Pros**

- visibility rules are concrete and easy to inspect,
- excellent for learning snapshot semantics,
- naturally demonstrates database bloat,
- forces implementation of vacuum/garbage collection,
- closely resembles PostgreSQL-style MVCC concepts,
- old versions are directly accessible.

**Cons**

- UPDATE may create significant heap bloat,
- indexes may need maintenance for new versions,
- vacuum becomes critical,
- cache locality can degrade,
- high-update workloads are harder to optimize.

**Learning reward:** extremely high.

### Locked rationale

Heap-version MVCC was selected because it makes important database mechanics explicit and observable:

- transaction snapshots,
- tuple visibility,
- old/new versions,
- dead tuples,
- vacuum,
- storage bloat,
- version-chain traversal,
- index/version interaction,
- update optimization opportunities.

This is intentionally preferred over an undo-chain design for the first engine because it exposes more of the practical machinery behind MVCC and creates a better learning path.

The architecture should still avoid choices that would make a future undo-based or alternative MVCC experiment impossible.

---

# 19. Transaction Model

## LOCKED: MVCC snapshots + explicit write conflict control

Transactions receive IDs and snapshots.

Core transaction states:

```text
ACTIVE
COMMITTED
ABORTED
```

The transaction manager owns:

- transaction IDs,
- active transaction registry,
- snapshots,
- commit/abort state,
- write sets,
- conflict checks.

Initial isolation targets:

1. Read Committed
2. Snapshot Isolation / Repeatable Read semantics

True Serializable isolation is deferred until the base MVCC engine is correct.

Later candidates:

- Serializable Snapshot Isolation,
- predicate locking,
- key-range locking.

---

# 20. Locks

## LOCKED: fine-grained logical locks for writes

Do not use a database-wide mutex for transaction semantics.

Initial locking may begin conservatively but architecture must permit:

- row/tuple write locks,
- table-level intention locks if needed,
- index-key/range locks later.

Locks may live for a transaction.

Latches must never be treated as transaction locks.

---

# 21. Write-Ahead Log

## LOCKED: WAL is mandatory

The database uses append-only write-ahead logging.

Core rule:

> A dirty data page must never reach durable storage before all WAL records needed to recover that page are durable.

Every WAL record receives an LSN.

Every dirty page tracks a page LSN.

WAL supports:

- redo,
- undo where necessary,
- transaction commit,
- transaction abort,
- page modifications,
- structural index changes.

---

# 22. Commit Path

## LOCKED: group commit

Transactions should not require one independent `fsync` each.

Commit pipeline conceptually:

```text
txn A ┐
txn B ├─> WAL buffer -> durable flush -> commits visible
txn C ┘
```

This is a core throughput optimization and should be designed into WAL from the beginning.

---

# 23. Buffer/WAL Policy

## LOCKED: STEAL + NO-FORCE

### NO-FORCE

Commit does not force every modified data page to disk.

### STEAL

The buffer manager may evict dirty pages containing uncommitted changes, provided WAL ordering makes recovery safe.

These policies make WAL/recovery meaningful and avoid toy-engine durability behavior.

---

# 24. Crash Recovery

## LOCKED: ARIES-inspired recovery, implemented incrementally

Recovery architecture should use:

```text
analysis
redo
undo
```

with:

- LSNs,
- page LSNs,
- transaction state,
- checkpoints.

Do not attempt every advanced ARIES feature immediately.

Implement the smallest correct subset compatible with STEAL + NO-FORCE, then expand.

---

# 25. Checkpointing

## LOCKED: fuzzy/background-compatible checkpoints

The architecture must not require stopping the whole database simply to checkpoint.

A simpler first implementation is acceptable, but persistent formats and WAL metadata should support eventual fuzzy checkpoints.

---

# 26. Vacuum / Garbage Collection

## LOCKED if heap-version MVCC is selected

Dead tuple versions are reclaimed using a visibility horizon based on the oldest active snapshot.

Vacuum responsibilities:

- identify unreachable tuple versions,
- reclaim heap space,
- clean index entries where needed,
- maintain free-space metadata,
- eventually update statistics.

Start with explicit/manual vacuum.

Add background vacuum only after correctness is well tested.

---

# 27. Catalog

## LOCKED: relational system catalog

Persistent metadata should eventually live in system tables such as:

```text
sys_tables
sys_columns
sys_indexes
sys_constraints
sys_statistics
```

A small bootstrap catalog mechanism is acceptable.

The database should increasingly use its own storage/execution machinery to read metadata.

---

# 28. SQL Front End

## LOCKED: implement our own limited parser

Implement:

- lexer/tokenizer,
- hand-written parser,
- AST,
- semantic binder,
- type checking.

Do not attempt full SQL grammar initially.

Why this choice:

- enough compiler experience to understand SQL front ends,
- avoids hiding parsing entirely behind a library,
- scope can remain controlled,
- implementation is educational without needing to become a standards-compliance project.

Initial SQL subset should grow incrementally.

---

# 29. Binder

## LOCKED: parser and binder are separate

Parser output should contain syntax, not resolved database objects.

Binder responsibilities:

- table-name resolution,
- column-name resolution,
- alias handling,
- ambiguity detection,
- type checking,
- function/operator resolution,
- wildcard expansion,
- catalog lookup.

Bound nodes should reference stable catalog IDs rather than only strings.

---

# 30. Logical Query Representation

## LOCKED: relational algebra-style logical plan

Core operators:

```text
LogicalScan
LogicalFilter
LogicalProject
LogicalJoin
LogicalAggregate
LogicalSort
LogicalLimit
LogicalInsert
LogicalUpdate
LogicalDelete
```

This is the main bridge between database theory and implementation.

---

# 31. Query Rewrite

## LOCKED: rule-based logical optimization before costing

Examples:

- constant folding,
- predicate simplification,
- predicate pushdown,
- projection pruning,
- redundant operator removal,
- join predicate extraction.

Rules must preserve semantics.

Optimizer tests should compare results before and after rewriting.

---

# 32. Statistics

## LOCKED: explicit table/column statistics

The optimizer should eventually maintain at least:

- row count,
- distinct-count estimate,
- null fraction,
- min/max,
- histograms or another distribution summary.

Cardinality estimation is a first-class subsystem.

Do not hard-code optimizer decisions such as "index always beats scan."

---

# 33. Cost-Based Optimizer

## LOCKED

Physical planning should compare alternative implementations using estimated costs.

Initial cost dimensions:

- estimated rows,
- CPU work,
- sequential page reads,
- random page reads,
- memory consumption.

Exact calibration can improve over time.

The important architectural principle is that physical algorithm choice is cost-driven.

---

# 34. Join Ordering

## LOCKED: dynamic programming for small join sets

For small numbers of joined relations, enumerate useful alternatives using dynamic programming.

For larger join graphs, introduce heuristics to bound planning time.

This subsystem is intentionally implemented rather than outsourced because it has very high learning value.

---

# 35. Physical Operators

## LOCKED: logical and physical plans are distinct

Examples:

```text
LogicalScan
    ->
SeqScan
IndexScan

LogicalJoin
    ->
NestedLoopJoin
IndexNestedLoopJoin
HashJoin
MergeJoin
```

The optimizer selects physical operators.

---

# 36. Execution Model

## LOCKED: vectorized/chunk-at-a-time execution

Primary execution operators exchange batches rather than single tuples.

Conceptual API:

```text
DataChunk
    columns/vectors
    cardinality
    null masks
```

Operators consume/produce chunks such as roughly:

```text
256–2048 rows
```

Initial default target:

```text
1024 rows/chunk
```

Rationale:

- lower virtual/function-call overhead,
- better cache behavior,
- natural SIMD opportunities,
- high learning value,
- closer to modern high-performance analytical/general-purpose execution.

A tiny row-at-a-time reference executor may be created only if it materially helps correctness testing; it is not the production execution architecture.

---

# 37. Execution Data Layout

## LOCKED: typed vectors inside chunks

Avoid one heap-allocated polymorphic `Value` object per cell in hot paths.

Prefer typed vectors:

```text
Int64Vector
DoubleVector
BooleanVector
StringVector
```

with:

- contiguous data where possible,
- null bitmap/mask,
- selection vectors where useful.

Generic scalar `Value` objects are acceptable in:

- parser literals,
- catalog metadata,
- planning,
- debugging.

They should not dominate scan/join/aggregation hot loops.

---

# 38. Primary Join Algorithms

## LOCKED implementation order

1. Nested-loop join — correctness baseline and small inputs.
2. Hash join — main equality-join implementation.
3. Index nested-loop join — when the inner side has a useful index.
4. Merge join — later.

Hash join should support:

- build/probe phases,
- query-lifetime memory arena,
- null semantics,
- composite keys,
- later spilling when memory limits are introduced.

---

# 39. Aggregation

## LOCKED: hash aggregation first

Implement grouped aggregation using a hash table.

Later add:

- sort aggregation,
- streaming aggregation for preordered inputs,
- spilling.

---

# 40. Sorting

## LOCKED: in-memory sort first, external sort later

Start with an in-memory sort operator.

Then implement external merge sort once memory budgeting exists.

External sorting is intentionally retained as a learning milestone.

---

# 41. Query Memory Management

## LOCKED: query-scoped arenas/pools

Avoid general-purpose allocation in inner loops.

Use query/operator-lifetime arenas for:

- hash tables,
- temporary keys,
- intermediate buffers,
- vector storage.

Goals:

- bulk allocation,
- cheap reset/free,
- fewer allocator calls,
- predictable lifetime.

---

# 42. Parallelism

## LOCKED: concurrency-ready architecture, parallel execution later

Initial executor may run a query on one worker.

The architecture must avoid preventing later:

- parallel table scans,
- parallel hash build/probe,
- parallel aggregation,
- task scheduling.

Do not build a complex parallel scheduler before the single-thread executor is correct.

---

# 43. Threading / Shared State

## LOCKED

Avoid centralized global locks in hot paths.

Likely future partitioning:

- sharded buffer-pool page table,
- per-page latches,
- partitioned lock manager,
- thread-local/query-local arenas,
- WAL append coordination.

Start simpler where needed, then remove measured contention.

---

# 44. Error Handling

## LOCKED

Distinguish:

- SQL/user errors,
- transaction conflicts,
- I/O errors,
- corruption,
- internal invariant failures.

Internal corruption/invariant failures should fail loudly in debug/test builds.

Do not silently continue after impossible storage states.

---

# 45. Testing Philosophy

## LOCKED

Every subsystem needs:

### Unit tests

For local invariants.

### Property/randomized tests

Especially for:

- B+ tree operations,
- page compaction,
- tuple serialization,
- transaction visibility,
- recovery.

### Crash tests

Simulate process failure at many WAL/page-write boundaries.

### Differential tests

Where practical, execute supported SQL against a reference database and compare results.

### Concurrency stress tests

Exercise:

- lock ordering,
- B+ tree splits,
- buffer eviction,
- transaction conflicts.

---

# 46. Benchmarking Philosophy

## LOCKED

Performance claims require measurements.

At minimum benchmark:

- sequential scan throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- buffer-pool hit/miss behavior,
- concurrent transaction throughput.

Track:

```text
rows/sec
queries/sec
transactions/sec
p50 latency
p95 latency
p99 latency
CPU time
page reads/writes
WAL bytes
cache hit rate
```

Microbenchmarks and end-to-end benchmarks should both exist.

---

# 47. Observability

## LOCKED

Build internal counters early.

Useful counters:

- logical page reads,
- physical page reads,
- page writes,
- buffer hits/misses,
- WAL bytes,
- WAL flushes,
- B+ tree splits,
- rows scanned,
- rows filtered,
- hash-table build/probe counts,
- optimizer estimated rows,
- actual rows.

This will make later performance work evidence-driven.

---

# 48. Explain

## LOCKED

Eventually support an `EXPLAIN`-style command showing:

- logical plan,
- selected physical plan,
- estimated cardinalities,
- estimated costs.

Later:

```text
EXPLAIN ANALYZE
```

should show estimated vs actual rows/timing.

This is important both for learning and optimizer debugging.

---

# 49. Suggested Implementation Order

This order intentionally follows dependency direction and maximizes learning.

## Phase 1 — Raw storage

1. byte serialization utilities,
2. file manager,
3. page abstraction,
4. page allocator,
5. slotted heap page,
6. tuple encoding,
7. heap file.

## Phase 2 — Buffer management

8. buffer frames,
9. page guards,
10. CLOCK replacement,
11. dirty-page flushing,
12. buffer-pool benchmarks.

## Phase 3 — Indexes

13. B+ tree page formats,
14. lookup,
15. insert/split,
16. range scans,
17. delete/rebalance,
18. latch coupling,
19. randomized B+ tree tests.

## Phase 4 — Transactions and durability

20. transaction manager,
21. selected MVCC representation,
22. visibility rules,
23. write conflicts,
24. WAL format,
25. group commit,
26. STEAL/NO-FORCE integration,
27. crash recovery,
28. vacuum/GC.

## Phase 5 — Catalog and SQL

29. bootstrap catalog,
30. system tables,
31. lexer,
32. parser,
33. AST,
34. binder,
35. type system.

## Phase 6 — Query execution

36. logical plans,
37. physical plans,
38. vector/chunk representation,
39. sequential scan,
40. filter,
41. projection,
42. nested-loop join,
43. hash join,
44. aggregation,
45. sort,
46. index scan.

## Phase 7 — Optimization

47. statistics,
48. rule rewrites,
49. selectivity estimation,
50. cost model,
51. access-path selection,
52. join-order dynamic programming,
53. `EXPLAIN`,
54. `EXPLAIN ANALYZE`.

## Phase 8 — Performance work

55. profiling,
56. allocation reduction,
57. cache/layout improvements,
58. contention reduction,
59. prefetch/asynchronous I/O experiments,
60. parallel execution.

---

# 50. Architecture Invariants

Codex should treat these as non-negotiable unless this file is explicitly changed.

1. Persistent structures use page IDs, never process pointers.
2. On-disk formats are explicitly serialized.
3. WAL records required for recovery become durable before corresponding dirty data pages.
4. Buffer pins are always released.
5. Transaction locks and in-memory latches are different mechanisms.
6. Logical plans never encode a physical implementation choice.
7. The optimizer may choose between multiple physical algorithms.
8. Hot execution paths do not allocate one object per cell/tuple unnecessarily.
9. Lower layers do not depend on SQL syntax objects.
10. Tests must be able to force tiny buffer pools to exercise eviction.
11. Recovery must be tested with simulated crashes.
12. Performance-sensitive changes need benchmark evidence.

---

# 51. Locked Architecture Decisions

The project owner selected:

```text
A1 = C++20
B1 = heap-version MVCC
```

These are now part of the **LOCKED v1** architecture baseline.

Changing either decision requires an explicit architecture revision with:

- rationale,
- affected subsystems,
- migration cost,
- compatibility impact,
- benchmark or correctness motivation where applicable.

---

# 52. Working Philosophy

This is not intended to be the shortest path to "a database that accepts SQL."

It is intended to be the shortest reasonable path to understanding **why real relational databases are built the way they are**.

When choosing between:

```text
easy abstraction that hides an important database mechanism
```

and:

```text
harder implementation that teaches the mechanism
```

prefer the second, provided the project remains finishable.

---

# 53. Concrete Storage Engine Contract

## LOCKED

This section turns the earlier storage decisions into an implementable physical contract.

The initial implementation should follow this section closely enough that two independently written components agree on:

- what a page ID means,
- how a page is addressed on disk,
- what common metadata exists on every page,
- how heap tuples are laid out,
- how tuple versions are addressed,
- how inserts find free space,
- how the buffer pool and disk layer interact,
- where WAL ordering is enforced.

The goal is to avoid a common failure mode in database projects: implementing `HeapPage`, `BufferPool`, indexes, and transactions independently and discovering later that their assumptions are incompatible.

---

# 54. Fundamental Identifier Types

## LOCKED

Use fixed-width logical identifier types.

Conceptually:

```cpp
using FileId      = uint32_t;
using PageNo      = uint64_t;
using SlotId      = uint16_t;
using TxnId       = uint64_t;
using CommandId   = uint32_t;
using Lsn         = uint64_t;
using TableId     = uint64_t;
using IndexId     = uint64_t;
using SchemaVer   = uint32_t;
```

Reserve zero where useful as an invalid/unassigned value.

Recommended sentinels:

```text
INVALID_FILE_ID = 0
INVALID_PAGE_NO = UINT64_MAX
INVALID_SLOT_ID = UINT16_MAX
INVALID_TXN_ID  = 0
INVALID_LSN     = 0
```

Do not expose OS file descriptors as `FileId`.

An OS file descriptor is a process-local resource.

A `FileId` is a database-level identifier.

---

# 55. PageId

## LOCKED

In memory:

```cpp
struct PageId {
    FileId file_id;
    PageNo page_no;
};
```

Semantics:

```text
PageId = persistent logical identity of a page
```

A page is physically addressed at:

```text
byte_offset = page_no * PAGE_SIZE
```

inside the file represented by `file_id`.

`PageId` must be:

- comparable,
- hashable,
- trivially copyable if practical,
- independent from buffer-frame identity.

Never use:

```text
buffer_frame_index
memory_address
file_descriptor
```

as persistent page identity.

---

# 56. RID

## LOCKED

A heap tuple version is identified by:

```cpp
struct Rid {
    PageId page;
    SlotId slot;
};
```

Important semantic decision:

> An RID identifies a **physical heap tuple version**, not a permanent logical SQL row.

Therefore an UPDATE normally produces a new RID.

This is intentional.

It makes MVCC mechanics visible and keeps the first implementation conceptually close to a traditional heap-version engine.

---

# 57. Index-to-Heap Addressing

## LOCKED: indexes reference physical tuple-version RIDs

Initial B+ tree leaf payload:

```text
(index key, RID)
```

For a non-unique index, ordering is conceptually:

```text
(index key, RID)
```

so duplicate keys remain individually addressable.

### Consequence for UPDATE

Version 1 behavior:

```text
old tuple version
    xmax = updating transaction

new tuple version
    xmin = updating transaction
    new RID

new index entries
    point to new RID
```

Old index entries are not immediately destroyed merely because their tuple version became invisible.

They are reclaimed by vacuum when safe.

### Consequence for index scans

An index match is never sufficient by itself.

The executor must:

```text
index lookup
    ↓
RID
    ↓
heap tuple fetch
    ↓
MVCC visibility check
    ↓
return or reject tuple
```

This is a valuable performance and correctness property to understand.

### Future optimization

A HOT-like optimization may later allow some updates that do not change indexed columns to avoid creating new index entries.

Do not implement HOT in the first storage milestone.

---

# 58. File Kinds

## LOCKED

The storage manager recognizes explicit file kinds.

Initial persisted file-kind codes are:

```text
0 = INVALID / unassigned
1 = HEAP
2 = BTREE
3 = FSM
4 = CATALOG
```

The persisted `file_kind` field is a 16-bit unsigned integer encoded little-endian.

These numeric codes are part of the persistent file-format contract. Existing values must not be renumbered merely by changing source-language enum order. Future file kinds must receive new explicit numeric codes.

Code `0` is not a valid persistent random-access file kind and must be rejected when decoding a superblock.

WAL uses its own append-only log format and is not treated as a normal random-access page file.

Each random-access database file reserves:

```text
page 0 = superblock
```

Data/object pages begin at:

```text
page 1
```

---

# 59. File Superblock

## LOCKED

Every page-based file begins with an 8 KiB superblock page.

The superblock exists to detect:

- wrong file type,
- incompatible format,
- wrong page size,
- accidental file mixups,
- corruption of basic metadata.

The exact serialization is part of the persistent v1 contract.

The superblock uses the normal 32-byte common page header at offsets `0..31`.
The common-header fields provide the superblock's:

- `format_version`,
- `flags`,
- `page_lsn`,
- `checksum_crc32c`,
- `header_size`,
- `page_no`.

These fields are not duplicated in the superblock-specific region.

Initial superblock constants:

```text
magic                  = ASCII "DBLUSBLS"
format_version         = 1
page_size              = 8192
page_type              = SUPERBLOCK
page_no                = 0
header_size            = 72
```

Persisted byte layout:

```text
offset  size  field
------  ----  -----------------------------------------------
0       2     page_type = SUPERBLOCK
2       2     format_version = 1
4       4     flags
8       8     page_lsn
16      4     checksum_crc32c
20      2     header_size = 72
22      2     common reserved16 = 0
24      8     page_no = 0

32      8     magic = ASCII "DBLUSBLS"
40      2     file_kind
42      2     reserved16 = 0
44      4     page_size = 8192
48      4     file_id
52      4     reserved32 = 0
56      8     object_id
64      8     creation_epoch
72      8120  reserved bytes = 0
------------------------------------------------------------
total         8192 bytes
```

All multi-byte integers are little-endian.

`object_id` means:

```text
TableId for HEAP/FSM files
IndexId for BTREE files
catalog object ID where applicable
```

`creation_epoch` is an opaque persisted 64-bit value in v1. Its generation and unit semantics may be specified later without changing its field width or offset.

### Superblock checksum

The common-header checksum field at offsets `16..19` is the only checksum field for the base superblock page.

Checksum computation:

```text
1. treat bytes 16..19 as zero,
2. compute CRC32C over exactly bytes 0..8191,
3. store the resulting uint32_t little-endian at bytes 16..19.
```

Changing the stored checksum bytes alone must not change the logical checksum input because those bytes are treated as zero during computation.

### Superblock validation

Decoding a v1 superblock must reject:

- input shorter than one page,
- CRC32C mismatch,
- `page_type` other than `SUPERBLOCK`,
- unsupported `format_version`,
- `header_size` other than `72`,
- `page_no` other than `0`,
- magic other than `DBLUSBLS`,
- invalid or unknown `file_kind`,
- persisted `page_size` other than `8192`,
- any nonzero reserved field or trailing reserved byte.

Unknown flag bits are preserved as raw bits unless a later format revision assigns semantics to them.

Inputs larger than one page may be accepted by codecs, but only the leading 8192-byte page participates in the v1 superblock format.

Because v1 decoding requires all reserved bytes to be zero, later use of those bytes requires an explicit persistent-format revision rather than silently assigning new meaning to them.

Do not serialize the C++ struct by writing its memory representation.

Use explicit encode/decode helpers.

---

# 60. Page Allocation Policy

## LOCKED: append-first allocation

Initial physical allocation uses:

```text
new page_no = current_file_page_count
extend file by one page
```

A newly created random-access page file begins at zero bytes / zero pages.
Higher-level file-creation logic explicitly allocates and initializes page `0`
as the superblock; `DiskManager::CreateFile` does not implicitly create it.

Page allocation is explicit. A page write to an unallocated `page_no` must fail
rather than implicitly extending the file or creating a sparse page. Initial
allocation occurs through the append-first extension path.

The compound append operation:

```text
discover current aligned page count
+
extend file by exactly one PAGE_SIZE page
```

must be serialized with concurrent extensions of the same managed storage so
two callers cannot receive the same `PageNo`.

This keeps page allocation understandable and deterministic.

Whole-file shrinking is not required.

### Page reuse

Object-specific subsystems may later recycle entirely unused pages.

Examples:

- B+ tree free-page list,
- heap pages that become completely empty,
- FSM metadata pages.

Do not introduce a complicated general-purpose extent allocator before the storage engine works.

### Why this choice

An extent allocator is useful in mature engines, but implementing it before basic heap/index/recovery logic gives relatively little learning reward for substantial complexity.

Extents remain a later optimization milestone.

---

# 61. Common Page Header

## LOCKED: 32-byte common header

Every page except WAL records begins with a common logical header.

On-disk layout:

```text
offset  size  field
------  ----  ----------------
0       2     page_type
2       2     format_version
4       4     flags
8       8     page_lsn
16      4     checksum_crc32c
20      2     header_size
22      2     reserved16
24      8     page_no
-------------------------------
total   32 bytes
```

All multi-byte integers are little-endian.

### Why store page_no inside the page?

Although the file offset already implies the page number, storing it inside the page helps detect:

- misdirected writes,
- incorrect buffer mappings,
- file corruption,
- debugging mistakes.

### `page_lsn`

`page_lsn` is the LSN of the newest WAL-protected modification reflected in the page.

It becomes meaningful once WAL is integrated.

Before WAL integration it may remain `INVALID_LSN`.

---

# 62. Page Types

## LOCKED

Initial page type enum and persisted numeric codes are:

```text
0 = SUPERBLOCK
1 = HEAP_DATA
2 = FSM_DATA
3 = BTREE_INTERNAL
4 = BTREE_LEAF
5 = BTREE_FREE
6 = CATALOG_DATA
```

The persisted `page_type` field is a 16-bit unsigned integer encoded little-endian.

These numeric codes are part of the persistent page-format contract and must not be changed or reordered merely by changing the source-language enum declaration.

Future page types must receive new explicit numeric codes rather than renumbering existing values.

Page-type-specific parsing must validate that the actual page type matches the expected type.

Do not reinterpret arbitrary page bytes as a requested page structure without validation.

---

# 63. Page Checksums

## LOCKED: checksum field reserved from day one

Use CRC32C for persistent page checksums.

Implementation staging:

### Before WAL/recovery milestone

Checksum generation/verification may be optional behind a configuration flag to simplify early bring-up.

### From recovery milestone onward

Checksums should be enabled by default for persistent random-access pages.

Checksum computation treats the checksum field itself as zero.

Purpose:

- corruption detection,
- misdirected/torn-write debugging,
- crash-test observability.

A checksum does **not** itself solve torn-page recovery.

That will be addressed when the WAL/recovery format is designed.

---

# 64. Heap File Structure

## LOCKED

A heap relation uses at least:

```text
table_<table_id>.heap
table_<table_id>.fsm
```

Heap file:

```text
page 0       heap superblock
page 1..N    heap data pages
```

The heap file should remain easy to sequentially scan.

Avoid mixing large amounts of unrelated metadata pages between heap data pages.

This is one reason the free-space map lives in a separate file.

---

# 65. Heap Scan Order

## LOCKED: physical page order

A full sequential heap scan visits data pages in ascending `page_no`.

Within each page, visit slots in ascending `slot_id`.

The executor then applies MVCC visibility.

Physical scan order is **not SQL result ordering**.

Without `ORDER BY`, SQL results remain unordered semantically.

---

# 66. Heap Data Page Header

## LOCKED

Immediately after the 32-byte common page header, a heap page contains a small heap-specific header.

The initial persisted `HEAP_DATA` page format version is:

```text
format_version = 1
```

For v1 heap pages, the common header must contain:

```text
page_type      = HEAP_DATA
format_version = 1
header_size    = 48
reserved16     = 0
```

The persisted `page_no` must match the page's logical `PageId.page_no`.

Logical layout:

```text
offset-from-page  size  field
----------------  ----  ----------------
32                2     slot_count
34                2     free_slot_head
36                2     lower
38                2     upper
40                4     prune_hint
44                4     reserved
-----------------------------------------
total page header = 48 bytes
```

All multi-byte fields are little-endian.

Definitions:

```text
lower
    first byte after header + slot directory

upper
    first byte of tuple-data region

free bytes
    upper - lower
```

For v1:

```text
lower = 48 + slot_count * 8
```

and valid page geometry requires:

```text
48 <= lower <= upper <= PAGE_SIZE
```

Tuple bytes grow from the end of the page downward.

Slot entries grow from the header upward.

### Empty free-slot list

The persisted empty-list sentinel is:

```text
free_slot_head = INVALID_SLOT_ID = UINT16_MAX = 0xFFFF
```

A blank heap page therefore stores `slot_count = 0` and
`free_slot_head = INVALID_SLOT_ID`.

Nonempty free-list linkage remains deferred until slot reuse is implemented.
The field is reserved so that later slot reuse can avoid scanning the complete
slot directory.

### Reserved and hint fields

For heap-page format version 1:

```text
common reserved16 = 0
heap reserved      = 0
```

Decoding/structural validation must reject nonzero values for either reserved
field. Assigning semantics to either reserved field requires coordinated
format-version handling.

`prune_hint` is initialized to zero and remains only a future vacuum/pruning
hint until its semantics are explicitly specified.

---

# 67. Slot Directory

## LOCKED: 8-byte slot entries

Each slot entry is exactly 8 bytes:

```text
offset  size  field
------  ----  ------------
0       2     tuple_offset
2       2     tuple_length
4       2     slot_flags
6       2     aux
---------------------------
total   8 bytes
```

All multi-byte fields are little-endian.

The initial persisted 16-bit slot-state codes are:

```text
0 = UNUSED
1 = NORMAL
2 = DEAD
3 = REDIRECT_RESERVED
```

These numeric codes are part of the persistent heap-page v1 format and must not
depend on source-language enum declaration order. Existing codes must not be
renumbered. Future states must receive new explicit numeric codes or be
introduced through an explicit format revision when compatibility requires it.

Persisted slot-state values outside the defined set are invalid for heap-page
format version 1 and must be rejected by structural decoding/validation.

`REDIRECT_RESERVED` and `aux` are intentionally reserved for a future HOT-like optimization.

They need not be used by the first implementation.

For heap-page format version 1, every newly created `NORMAL` slot must persist:

```text
aux = 0
```

The initial insertion path does not assign any other meaning to `aux`. A future
feature that gives `aux` operational meaning for `NORMAL` slots requires an
explicitly compatible rule or a coordinated heap-page format revision.

### DEAD slot coordinates before and after physical reclamation

A `NORMAL -> DEAD` transition does not immediately reclaim tuple bytes.

Before compaction, a `DEAD` slot may therefore retain its previous:

```text
tuple_offset
tuple_length
aux
```

while the old tuple bytes remain physically present.

Once compaction physically discards the payload of a `DEAD` slot, heap-page
format version 1 requires the canonical persisted coordinates:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved
```

Clearing the coordinates prevents a reclaimed `DEAD` slot from continuing to
point into free space or into tuple bytes that were moved for another slot.

This canonicalization does **not** make the slot `UNUSED`, reusable, or part of
the free-slot list. Its `SlotId` and persistent `DEAD` state remain unchanged
until the later delayed-reuse protocol explicitly permits a state transition.

At this stage, no additional tuple-range or `aux` semantics are assigned to
`UNUSED` or `REDIRECT_RESERVED`. `NORMAL` entries must reference tuple bytes
wholly within the tuple-data region and within the page.

### Stable slot semantics

Compacting tuple bytes inside a page changes:

```text
slot.tuple_offset
```

but not:

```text
SlotId
```

therefore the RID remains stable during ordinary page compaction.

---

# 68. Heap Page Free-Space Geometry

## LOCKED

Conceptually:

```text
0
┌──────────────────────────────┐
│ common page header           │
├──────────────────────────────┤
│ heap page header             │
├──────────────────────────────┤
│ slot 0                       │
│ slot 1                       │
│ ...                          │
│                              │ ← lower
├──────────────────────────────┤
│          free space          │
├──────────────────────────────┤
│                              │ ← upper
│ tuple bytes                  │
│ tuple bytes                  │
└──────────────────────────────┘
8192
```

Insertion requires space for:

```text
tuple bytes
+
new slot entry, if no reusable slot exists
```

Page compaction may be performed when total reclaimable space is sufficient but not contiguous.

---

# 69. Maximum Inline Tuple Size

## LOCKED for v1

A tuple must fit entirely inside one heap page.

No TOAST/overflow storage in the first storage format.

Therefore:

```text
maximum tuple size
<
PAGE_SIZE - page headers - at least one slot entry
```

For the initial 8192-byte heap-page format:

```text
PAGE_SIZE                 = 8192
heap total header         = 48
one slot entry            = 8

maximum accepted raw tuple payload = 8135 bytes
```

The strict inequality is intentional for v1. A payload of 8136 bytes is
rejected even though it would make the slot directory and tuple region meet
exactly with zero free bytes remaining.

This raw-page byte limit is a storage-format bound, not yet a statement about
the minimum or maximum size of a semantically valid encoded SQL tuple. The
tuple codec may impose a smaller maximum once its own mandatory header and
layout rules are applied.

Attempting to insert an oversized tuple should return a clear unsupported/row-too-large error.

### Later milestone

Implement overflow/large-object pages only after:

- heap correctness,
- WAL,
- recovery,
- MVCC,
- indexes

are working.

This postponement is intentional: overflow storage is useful, but it obscures the basic heap mechanics too early.

---

# 70. Tuple Header

## LOCKED: explicit MVCC-aware tuple header

Each physical heap tuple version begins with a fixed logical header.

Initial layout:

```text
offset  size  field
------  ----  -----------------------
0       8     xmin
8       8     xmax
16      4     cmin
20      4     cmax
24      8     prev_page_no
32      2     prev_slot
34      2     tuple_flags
36      2     header_bytes
38      2     null_bitmap_bytes
40      4     schema_version
44      4     reserved
-------------------------------------
total   48 bytes
```

All multi-byte fields are little-endian.

For tuple-header format v1:

```text
header_bytes = 48
reserved     = 0
```

`header_bytes` describes only the fixed 48-byte tuple-header prefix. It does
not include the null bitmap that follows it.

The four reserved bytes at offsets `44..47` must be written as zero and must be
zero when decoding tuple-header format v1. Assigning them semantics requires a
coordinated format revision.

### `xmin`

Transaction that created this tuple version.

### `xmax`

Transaction that invalidated/deleted/superseded this tuple version.

Use:

```text
INVALID_TXN_ID = 0
```

for "no xmax".

### `cmin` / `cmax`

Command identifiers inside a transaction.

They allow correct semantics when one transaction performs multiple statements that affect the same logical rows.

This is more machinery than a toy MVCC implementation needs, but it teaches an important real-engine distinction:

```text
transaction identity
!=
statement/command visibility
```

`CommandId` zero is valid and must not be treated as an invalid sentinel.

### Previous-version pointer

```text
(prev_page_no, prev_slot)
```

points to the immediately preceding physical tuple version in the same heap file.

If no previous version exists:

```text
prev_page_no = INVALID_PAGE_NO
prev_slot    = INVALID_SLOT_ID
```

Tuple-header v1 requires the pair to be internally consistent:

```text
no previous version:
    prev_page_no = INVALID_PAGE_NO
    prev_slot    = INVALID_SLOT_ID

previous version present:
    prev_page_no != INVALID_PAGE_NO
    prev_slot    != INVALID_SLOT_ID
```

A mixed sentinel/non-sentinel pair is structurally invalid.

This creates a backward version chain.

The chain is useful for:

- debugging,
- vacuum,
- future HOT-style behavior,
- understanding version history.

Do not store `FileId` in the tuple-version link because all versions of a row belong to the same heap relation.

---

# 71. Tuple Flags

## LOCKED

Reserve tuple flags for facts about the physical tuple version.

Tuple-header v1 initially assigns only the physical-layout flags:

```text
HAS_NULLS  = 0x0001
HAS_VARLEN = 0x0002
```

The known-mask for tuple-header v1 is therefore:

```text
0x0003
```

All other bits are invalid in tuple-header format v1 and must be rejected on
encode/decode rather than silently preserved.

The following candidates remain reserved/deferred until their operational,
visibility, and recovery semantics are explicitly defined:

```text
IS_DELETED_HINT
CHAIN_ROOT
CHAIN_MEMBER
```

Existing persisted flag masks must not be renumbered. Future flags require new
explicit bit assignments and compatibility consideration.

Only persist a flag when it has a well-defined physical/recovery/visibility
meaning.

Do not use ad-hoc flags as substitutes for transaction status.

Transaction commit/abort state belongs to transaction-management structures.

---

# 72. Tuple Data Layout

## LOCKED: schema-directed compact binary layout

Physical tuple:

```text
┌────────────────────────────┐
│ 48-byte tuple header       │
├────────────────────────────┤
│ null bitmap                │
├────────────────────────────┤
│ fixed layout area          │
│                            │
│ fixed values               │
│ varlen descriptors         │
├────────────────────────────┤
│ variable-length payload    │
└────────────────────────────┘
```

The schema owns a precomputed physical layout description.

Do not repeatedly rediscover column offsets by walking every preceding field during execution.

---

# 73. Null Bitmap

## LOCKED

Use one bit per nullable column.

Conceptually:

```text
bit = 1  => value is NULL
bit = 0  => value is present
```

Exact bit ordering must be documented and tested.

Recommended ordering:

```text
column 0 -> least-significant bit of byte 0
column 1 -> next bit
...
```

Columns declared `NOT NULL` may still have schema-level knowledge that avoids checking their bit in optimized execution.

For format simplicity, v1 may allocate bits for all columns.

---

# 74. Fixed-Width Values

## LOCKED

Initial physical widths:

```text
BOOLEAN      1 byte
INT32        4 bytes
INT64        8 bytes
FLOAT64      8 bytes
DATE         4 bytes
TIMESTAMP    8 bytes
```

Disk encoding is little-endian.

Do not use C++ ABI-specific representations for persisted values.

For example:

- BOOLEAN is explicitly `0` or `1`,
- FLOAT64 is encoded using its IEEE-754 64-bit representation,
- DATE/TIMESTAMP units are defined by the SQL type layer and then serialized as explicit integers.

---

# 75. VARCHAR Representation

## LOCKED: inline varlen payload with descriptor

A VARCHAR column's fixed-area slot contains:

```text
offset   uint32_t
length   uint32_t
```

The referenced bytes live in the variable-length payload area of the same tuple.

The offset is relative to the beginning of the tuple.

No null terminator is required.

Strings may contain arbitrary bytes as determined by the SQL string semantics.

The database must not use `strlen()` to discover persisted VARCHAR length.

---

# 76. Tuple Alignment

## LOCKED: compact persisted layout, aligned execution layout

Do not waste substantial on-disk space solely to align every persisted field to its natural machine alignment.

Persistent tuples are a compact binary format.

Serialization helpers must safely read/write potentially unaligned fields using:

```text
memcpy
explicit endian helpers
```

rather than undefined-behavior pointer casts.

The vectorized executor may decode into naturally aligned column vectors.

This intentionally distinguishes:

```text
storage representation
```

from:

```text
execution representation
```

---

# 77. Schema Versioning

## LOCKED: tuple carries schema version

Every tuple version stores:

```text
schema_version
```

The first implementation may support only schema version `1`.

The field exists from the beginning so future `ALTER TABLE` experiments do not require silently changing the tuple format.

Initial SQL DDL may reject schema-changing operations that would require version translation.

---

# 78. INSERT Path

## LOCKED

Conceptual insert sequence:

```text
1. encode tuple and compute required bytes
2. ask FreeSpaceMap for a candidate heap page
3. fetch candidate through BufferPool
4. acquire exclusive page latch
5. verify actual free space
6. compact page if worthwhile
7. allocate/reuse slot
8. write tuple bytes
9. update slot directory
10. update page LSN when WAL exists
11. mark buffer frame dirty
12. release latch/page guard
13. update FSM estimate
14. create index entries
```

Once WAL exists, the exact ordering of steps involving logging will be refined by the WAL contract.

The FSM is advisory.

If it gives a stale answer, insertion must remain correct.

---

# 79. UPDATE Path

## LOCKED: append a new physical version

Initial UPDATE behavior:

```text
1. locate visible old version
2. acquire required transaction write lock/conflict protection
3. create a complete new tuple version
4. new.xmin = current transaction
5. new.prev = old RID
6. old.xmax = current transaction
7. insert new version into heap, possibly on another page
8. create required index entries for new RID
9. keep old tuple/index entries until vacuum
```

Do not update the user-visible tuple bytes in place in v1.

The old version must remain available for snapshots that can still see it.

### Same-page preference

The heap layer may prefer placing the new version on the same page if sufficient space exists.

This is a locality optimization, not an invariant.

---

# 80. DELETE Path

## LOCKED: logical deletion first

DELETE initially means:

```text
set visible tuple version's xmax
```

The tuple bytes and index entries remain physically present.

Physical reclamation happens during vacuum once no active/future snapshot can require that version.

---

# 81. MVCC Visibility Boundary

## LOCKED

`HeapPage` does **not** decide SQL visibility by itself.

`HeapPage` exposes physical tuple versions and metadata.

A transaction/MVCC visibility component decides whether a tuple version is visible to a snapshot.

Reason:

visibility depends on global transaction state, not merely page bytes.

Conceptual layering:

```text
HeapPage
    provides tuple header + bytes

VisibilityManager / TransactionManager
    interprets xmin/xmax/cmin/cmax against snapshot

TableScan
    combines both
```

---

# 82. Free-Space Map

## LOCKED: separate approximate FSM

Each heap relation has:

```text
table_<id>.fsm
```

The FSM records an approximate free-space category for each heap page.

Use a compact category such as:

```text
0..255
```

where larger values mean more available insertion space.

The exact byte-to-space conversion must be deterministic.

Example conceptual mapping:

```text
category = floor(free_bytes * 255 / usable_page_bytes)
```

### In-memory accelerator

At runtime, maintain bucketed candidate sets derived from FSM information.

Conceptually:

```text
bucket[0]
bucket[1]
...
bucket[255]
```

Insertion asks for a bucket capable of satisfying the requested size.

### FSM is advisory

Because a crash or concurrent modification can make FSM information stale:

```text
candidate page
    ↓
fetch + latch
    ↓
verify actual free space
```

is mandatory.

If wrong, repair the FSM entry and retry.

This design teaches an important database principle:

> Performance metadata may be approximate; correctness metadata may not be.

---

# 83. FSM Persistence Semantics

## LOCKED

FSM state is a performance optimization, not the sole source of correctness.

Therefore recovery may tolerate stale FSM entries.

At startup or during maintenance, the system must be able to repair/rebuild FSM data by scanning heap-page headers.

This avoids making every free-space hint update part of the critical durability path.

---

# 84. Heap Page Compaction

## LOCKED

Heap-page compaction is allowed to move physical tuple bytes inside the page.

It must update slot offsets atomically while holding the exclusive page latch.

Because RIDs use slot numbers, compaction must not change the RID.

Compaction may physically discard tuple bytes only for slots that are already
persistently `DEAD`; the page layer does not itself decide that transition is
globally safe.

After discarding a `DEAD` payload, compaction canonicalizes that slot to:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved
```

`slot_count`, slot-directory positions, and `SlotId` values remain unchanged.
Compaction therefore increases contiguous free space without making the `DEAD`
slot reusable.

Retained `NORMAL` tuple ranges must be non-overlapping for compaction to proceed.
The v1 implementation may enforce this as a compaction precondition rather than
as a universal `HeapPage` validation rule until broader validation semantics are
specified.

Compaction must not reclaim MVCC-dead tuples merely because they appear dead locally.

Only vacuum, using the global visibility horizon, may decide that a version is safe to remove.

---

# 85. Vacuum Physical Reclamation

## LOCKED

Vacuum eventually performs:

```text
1. determine global safe visibility horizon
2. identify tuple versions dead to all relevant snapshots
3. remove corresponding index entries when required
4. mark heap slots reclaimable/dead
5. compact pages where useful
6. update FSM
```

The exact crash-safe ordering will be specified with WAL/recovery.

A page that becomes fully empty remains reusable database space; the engine is not required to shrink the operating-system file.

---

# 86. DiskManager Responsibilities

## LOCKED

`DiskManager` is deliberately dumb.

It knows:

- database file paths,
- mapping from `FileId` to open file handles,
- fixed-size page reads/writes,
- file creation/open/close,
- file-size discovery,
- durable flush operations,
- file extension.

It does **not** know:

- tuples,
- schemas,
- MVCC,
- B+ tree algorithms,
- SQL,
- query plans.

Conceptual interface:

```cpp
class DiskManager {
public:
    void ReadPage(PageId id, std::span<std::byte, PAGE_SIZE> out);
    void WritePage(PageId id, std::span<const std::byte, PAGE_SIZE> data);

    PageNo ExtendFile(FileId file_id);

    void SyncFile(FileId file_id);
    void SyncWal();

    FileHandle Open(...);
    void Close(...);
};
```

The actual API may differ, but the responsibility boundary must remain.

### File lifecycle boundary

Creating an OS file and initializing its database superblock are separate
operations.

`DiskManager` may create/register an empty file, but it does not choose the
file kind, encode page `0`, or validate file-kind-specific metadata. A higher
storage layer is responsible for allocating and writing the superblock during
database-file creation and for validating persistent identity when that
validation layer is introduced.

A `FileId` remains a database-level identifier. POSIX file descriptors are
private process-local resources owned by the disk layer and must not escape as
persistent identity.

---

# 87. File I/O Semantics

## LOCKED

Page reads/writes use positional I/O.

Initial implementation:

```text
pread
pwrite
```

A short read/write must be handled explicitly.

Do not assume one syscall always transfers the requested number of bytes.

Retry `pread`, `pwrite`, `fstat`, `ftruncate`, and `fdatasync` when interrupted
by `EINTR`, subject to the syscall's normal Linux semantics. Do not blindly
retry `close` after `EINTR`, because the descriptor may already have been
released and reused.

A normal page read requests exactly `PAGE_SIZE` bytes. EOF before the requested
page begins is a missing-page error. EOF after transferring only part of the
page is a short-read/corruption-style I/O error. Failed reads must not expose a
partially initialized destination page.

`WritePage` writes only pages that are already allocated. It must not
implicitly extend the file, create sparse pages, or combine allocation with
ordinary page replacement. Allocation uses the explicit append-first extension
path from §60.

Page-file sizes must be exact multiples of `PAGE_SIZE`. Misaligned sizes are an
error and must not be rounded or silently repaired by the disk layer.

Physical page-offset arithmetic must be checked before I/O so the complete
`PAGE_SIZE` page extent fits in the platform's positional-I/O offset type.

Errors must include enough context to identify:

```text
file
page
operation
errno
```

For page-file durability, the initial implementation uses:

```text
fdatasync
```

with retry on `EINTR`. This is the page-file sync primitive for v1 unless a
later architecture revision demonstrates a need for stronger metadata
semantics from `fsync`.

`pread`/`pwrite` are positional and must not depend on or mutate a shared file
offset.

---

# 88. BufferPool Responsibilities

## LOCKED

The buffer pool owns:

```text
PageId -> resident buffer frame
```

and is responsible for:

- caching,
- pinning,
- unpinning,
- dirty tracking,
- page latches,
- replacement eligibility,
- eviction,
- flushing,
- coordinating WAL-before-data.

The buffer pool must not parse heap tuples or B+ tree keys.

---

# 89. Buffer Frame

## LOCKED

A frame contains conceptually:

```text
aligned 8 KiB page bytes
PageId
pin count
dirty flag
reference/replacement metadata
page latch
I/O state
```

Frame metadata is not stored inside the database page.

Page bytes should be suitably aligned for efficient I/O and copying.

Avoid false sharing for frequently modified frame metadata where profiling shows contention.

---

# 90. Page Guards

## LOCKED: RAII page ownership

Use RAII guards for buffer pins and latches.

Conceptual types:

```cpp
ReadPageGuard
WritePageGuard
```

A guard:

- pins the frame while alive,
- acquires the appropriate page latch,
- releases latch and pin on destruction,
- may mark a page dirty after mutation.

Do not rely on every caller manually pairing:

```text
FetchPage
UnpinPage
```

across every error path.

This is one area where C++ RAII should actively reduce database bugs.

---

# 91. Buffer-Pool Page Table

## LOCKED: hash lookup abstraction

Resident pages are located through a hash-based page table:

```text
PageId -> FrameId
```

Initial implementation may use a conventional hash map under a mutex.

The interface must permit later replacement by:

- sharded hash tables,
- concurrent maps,
- partitioned locks.

Do not prematurely build a complex lock-free hash table.

---

# 92. Pinning Semantics

## LOCKED

A frame with:

```text
pin_count > 0
```

is not eligible for eviction.

A page guard owns one pin.

Background flushing may write a pinned page only under the buffer manager's safe synchronization rules, but eviction may not reuse its frame.

Tests must explicitly detect pin leaks.

---

# 93. Dirty-Page Semantics

## LOCKED

Mutating a page through a write guard marks the frame dirty.

A dirty page may be written:

- during eviction,
- by an explicit flush,
- by a background writer later.

Commit does not require writing dirty heap/index pages because the architecture is NO-FORCE.

---

# 94. WAL-before-Data Enforcement Point

## LOCKED: enforced by BufferPool flush path

Whenever the buffer pool is about to write a dirty page with:

```text
page_lsn = X
```

to its data file, it must first ensure:

```text
durable_wal_lsn >= X
```

Conceptually:

```text
if (wal_manager.durable_lsn() < page.page_lsn()) {
    wal_manager.flush_through(page.page_lsn());
}

disk_manager.write_page(page);
```

This rule must apply to:

- explicit page flush,
- eviction,
- background writeback.

Do not duplicate slightly different WAL-ordering logic in multiple storage objects.

---

# 95. CLOCK Replacement Contract

## LOCKED

Frames participate in CLOCK only when:

```text
pin_count == 0
```

Accessing a frame sets its reference/use bit.

Eviction walks the clock hand:

```text
pinned?       skip
referenced?   clear and skip
otherwise     victim
```

Dirty victims are flushed using the WAL-before-data rule before reuse.

The replacement policy operates on frame metadata, not on database page contents.

---

# 96. Storage Object Boundaries

## LOCKED

Use responsibilities approximately like:

```text
DiskManager
    raw fixed-position file I/O

BufferPool
    cached pages + replacement + latching + flush

HeapPage
    slotted-page mechanics for one already-pinned page

HeapFile
    relation-wide heap operations across pages

FreeSpaceMap
    candidate-page discovery for inserts

TupleCodec
    schema-directed tuple encode/decode

VisibilityManager
    snapshot visibility of physical tuple versions

Table
    logical relation interface coordinating heap + indexes + schema
```

Critical rule:

> `HeapPage` must never call `DiskManager` directly.

Pages reach storage through the buffer pool.

---

# 97. HeapPage API Shape

## LOCKED conceptually

`HeapPage` should be a lightweight view over a page buffer rather than independently owning another 8 KiB allocation.

Possible conceptual methods:

```cpp
class HeapPage {
public:
    static HeapPage Format(std::span<std::byte, PAGE_SIZE> page, PageNo page_no);
    static HeapPage Open(std::span<std::byte, PAGE_SIZE> page);

    std::optional<SlotId> Insert(std::span<const std::byte> tuple);
    TupleView Get(SlotId slot) const;

    void MarkSlotDead(SlotId slot);
    void Compact();

    size_t ContiguousFreeBytes() const;
    size_t ReclaimableBytes() const;

    SlotIterator begin() const;
    SlotIterator end() const;
};
```

The final C++ API may differ.

The architectural point is that page-format algorithms operate on caller-owned buffer-pool bytes.

---

# 98. TupleCodec Boundary

## LOCKED

Tuple serialization must be isolated from page management.

Conceptually:

```text
logical values + Schema
        ↓
TupleCodec::Encode
        ↓
byte sequence
        ↓
HeapPage::Insert
```

and:

```text
TupleView + Schema
        ↓
TupleCodec / typed accessors
        ↓
execution vectors / values
```

`HeapPage` should not know that column 3 is a VARCHAR.

---

# 99. Zero-Copy vs Decode Policy

## LOCKED: views in storage, vectors in execution

Storage code should avoid unnecessary copies by exposing immutable tuple views into pinned pages.

However, a tuple view must never outlive the page guard that pins its backing frame.

The vectorized execution engine may decode/copy selected columns into execution vectors.

This gives us a useful distinction:

```text
storage path:
    zero-copy views where safe

execution path:
    compact typed vectors optimized for processing
```

---

# 100. Lifetime Safety Rule

## LOCKED

Never return a naked long-lived pointer/span into a buffer page after the page guard has been released.

This should be enforced by API design where practical.

A common class of storage bugs is:

```text
fetch page
take tuple pointer
unpin/evict page
use dangling pointer
```

The project should make that pattern difficult to express.

---

# 101. Initial Module Layout

## LOCKED as a recommended project structure

Codex should initially organize the C++ code approximately as:

```text
src/
  common/
    types.h
    status.h
    endian.h
    crc32c.h

  storage/
    page_id.h
    page_layout.h

    disk/
      disk_manager.h
      disk_manager.cpp
      file_registry.h

    buffer/
      buffer_pool.h
      buffer_pool.cpp
      buffer_frame.h
      page_guard.h
      clock_replacer.h
      clock_replacer.cpp

    tuple/
      schema.h
      physical_layout.h
      tuple_codec.h
      tuple_codec.cpp
      tuple_view.h

    heap/
      heap_page.h
      heap_page.cpp
      heap_file.h
      heap_file.cpp
      free_space_map.h
      free_space_map.cpp

    index/
      btree_page.h
      btree.h

  txn/
    transaction.h
    snapshot.h
    visibility.h

  wal/
    wal_manager.h
    wal_record.h

  catalog/

  execution/

  optimizer/

tests/
benchmarks/
```

The exact filenames may evolve.

The subsystem boundaries should not.

---

# 102. Storage-Layer Dependency Direction

## LOCKED

Prefer:

```text
common
  ↓
disk
  ↓
buffer
  ↓
heap / btree physical pages
  ↓
table/index abstractions
```

`TupleCodec` may depend on schema/type definitions but should remain independent of SQL parser AST nodes.

Transaction visibility may inspect tuple headers but should not become embedded inside the physical page parser.

---

# 103. Storage Milestone 1

## LOCKED target

Before implementing B+ trees or SQL, the storage layer should be able to pass the following end-to-end scenario:

```text
create heap file
    ↓
initialize superblock
    ↓
create buffer pool with intentionally tiny capacity
    ↓
insert enough tuples to create many pages
    ↓
force repeated eviction
    ↓
close database
    ↓
reopen database
    ↓
sequentially scan heap
    ↓
decode every tuple
    ↓
verify exact values
```

No WAL/MVCC durability guarantees are required for this first milestone.

The purpose is to validate:

- persistent serialization,
- page addressing,
- buffer eviction,
- dirty-page flushing,
- slot stability,
- tuple encoding,
- FSM repair/use.

---

# 104. Storage Milestone 1 Required Tests

## LOCKED

At minimum:

### Slotted-page tests

- insert until full,
- reusable slots,
- compaction,
- invalid slot access,
- tuple bytes survive compaction,
- slot IDs remain stable.

### Tuple codec tests

- every scalar type,
- nulls,
- empty VARCHAR,
- long VARCHAR within inline limit,
- mixed fixed/varlen schemas,
- encode/decode round trip,
- unaligned field positions.

### Disk tests

- page zero/superblock,
- extend file,
- random page read/write,
- reopen persistence,
- short/error I/O handling where injectable.

### Buffer tests

Use very small pools such as:

```text
3 frames
```

while touching many more than 3 pages.

Verify:

- eviction,
- dirty writeback,
- pin protection,
- no pin leaks,
- CLOCK behavior,
- concurrent read guards,
- exclusive write guards.

### Heap tests

- thousands of tuples,
- many pages,
- scan after reopen,
- delete markers only after MVCC phase,
- FSM stale-entry repair.

---

# 105. Storage Milestone 1 Benchmarks

## LOCKED

Add benchmarks before optimizing.

Measure at least:

```text
sequential page read throughput
sequential page write throughput
buffer-pool hit lookup
buffer-pool miss + read
heap insert throughput
heap sequential scan throughput
tuple encode throughput
tuple decode throughput
```

Run benchmarks using:

```text
warm cache
cold-ish cache where practical
tiny buffer pool
large buffer pool
```

Do not optimize based only on intuition.

---

# 106. Deliberately Deferred Storage Features

## LOCKED

Do not implement these before Storage Milestone 1 is correct and benchmarked:

- overflow/TOAST values,
- page compression,
- tuple compression,
- extent allocation,
- file shrinking,
- direct I/O,
- `io_uring`,
- asynchronous prefetch,
- huge pages,
- NUMA-aware buffer pools,
- lock-free buffer page table,
- HOT updates,
- visibility map,
- background vacuum,
- parallel scans.

These are future experiments, not discarded ideas.

---

# 107. Storage Decisions Record

## LOCKED

The concrete storage design now additionally locks:

```text
8 KiB pages
page 0 superblock per page-based file
32-byte common page header
48-byte heap-page header total
8-byte slot entries
48-byte MVCC tuple header
physical-version RID addressing
indexes -> physical tuple-version RID
separate heap FSM file
approximate/rebuildable free-space metadata
append-first page allocation
compact persisted tuples
aligned vectorized execution representation
RAII read/write page guards
buffer pool owns WAL-before-data enforcement
physical-page-order sequential heap scans
```

No unresolved storage-layout choice currently requires project-owner input.

Any later proposal to change one of these choices should be treated as an explicit architecture revision rather than an incidental implementation detail.

---

# 108. Next Architecture Topic

The next design stage should lock the **B+ tree implementation** before coding it.

That discussion should decide:

- internal/leaf page layouts,
- key encoding,
- variable-length vs fixed-width index keys,
- fanout,
- separator-key semantics,
- split policy,
- merge/rebalance policy,
- sibling links,
- duplicate-key ordering,
- latch crabbing protocol,
- root replacement,
- free-page reuse,
- range-scan iterator lifetime,
- MVCC/index interaction,
- WAL logging of structural modifications.

After B+ tree design, the next major architecture stage should be:

```text
transaction manager
+
snapshot semantics
+
WAL record format
+
crash recovery
```

---

# 109. B+ Tree Architecture Contract

## LOCKED

This section defines the production B+ tree architecture.

The tree must support:

- equality lookup,
- ordered forward range scans,
- composite keys,
- variable-length VARCHAR keys,
- duplicate SQL keys,
- SQL UNIQUE indexes at the transactional layer,
- concurrent readers and writers,
- leaf/internal splits,
- redistribution and merge,
- root replacement/contraction,
- page reuse,
- physical tuple-version RIDs,
- future WAL/recovery integration.

The implementation must be page-backed from the beginning and operate through the real buffer pool. Do not build the production tree first as an in-memory pointer tree.

---

# 110. B+ Tree File and Superblock

## LOCKED

Each index owns:

```text
index_<index_id>.btree
```

Layout:

```text
page 0       B+ tree superblock
page 1..N    internal / leaf / free pages
```

The superblock additionally stores at least:

```text
IndexId
TableId
root_page_no
first_leaf_page_no
last_leaf_page_no
tree_height
index_flags
key_schema_version
key_schema_fingerprint
free_page_head
```

An initialized empty tree is represented as:

```text
height = 1
root = one empty leaf page
first_leaf = root
last_leaf = root
root.slot_count = 0
```

Do not represent a normal empty tree with an invalid root.

---

# 111. Index Key Schema

## LOCKED

Each B+ tree has a fixed key schema defined by catalog metadata and fingerprinted in its superblock.

Version 1 supports:

```text
ascending key components
NULLS FIRST
binary VARCHAR collation
```

Native descending index components and locale-aware collations are deferred.

SQL descending output can initially use an executor sort; native reverse index scans are deferred as described below.

---

# 112. User Key vs Physical Key

## LOCKED

Distinguish:

```text
user key
    SQL-visible indexed column tuple

physical key
    (user key, RID)
```

Example:

```text
user key:
    ("Smith", 2001-05-02)

physical key:
    (("Smith", 2001-05-02), RID(3,91,7))
```

The RID is a deterministic tiebreaker.

Therefore every physical tree entry is uniquely ordered even if many rows have the same SQL key.

This decision simplifies:

- duplicates,
- duplicate keys spanning leaves,
- split separators,
- lower/upper bound searches,
- exact physical deletion.

---

# 113. Persistent RID Encoding in Indexes

## LOCKED: 16 bytes

Persist an RID as:

```text
offset  size  field
------  ----  ----------------
0       4     heap_file_id
4       8     heap_page_no
12      2     heap_slot_id
14      2     reserved
-------------------------------
total   16 bytes
```

Physical RID ordering is numeric lexicographic order:

```text
(heap_file_id, heap_page_no, heap_slot_id)
```

---

# 114. Order-Preserving Index Key Encoding

## LOCKED: memcomparable encoding

`IndexKeyCodec` converts logical SQL index values into an order-preserving byte sequence.

For two supported user keys `A` and `B`:

```text
lexicographic_compare(Encode(A), Encode(B))
```

must produce exactly the same ordering as the database's index comparator.

This is deliberately preferred over invoking a polymorphic SQL comparator for every B+ tree binary-search comparison.

Benefits:

- inexpensive hot comparisons,
- simple binary search,
- simpler composite keys,
- higher learning value because representation and ordering semantics become explicit.

The index key format is allowed to use a different byte order from ordinary tuple serialization because its purpose is order preservation.

---

# 115. Index Field Encoding

## LOCKED

Each field begins with:

```text
0x00 = NULL
0x01 = non-NULL
```

giving ascending `NULLS FIRST`.

For non-NULL values:

### BOOLEAN

```text
false -> 0x00
true  -> 0x01
```

### INT32 / DATE

Interpret the signed value as 32 bits, flip the sign bit:

```text
sortable = bits(value) XOR 0x80000000
```

then encode big-endian.

### INT64 / TIMESTAMP

```text
sortable = bits(value) XOR 0x8000000000000000
```

then encode big-endian.

### FLOAT64

Before encoding:

- canonicalize `-0.0` and `+0.0` to one value,
- canonicalize all NaNs to one representation.

Database total order:

```text
-infinity
...
finite values
...
+infinity
NaN
```

Apply the standard sortable IEEE-754 bit transform, then encode big-endian.

The SQL FLOAT64 comparison layer must use the same equality/order semantics.

### VARCHAR

Version 1 uses binary bytewise collation.

Encode bytes as:

```text
ordinary non-zero byte -> itself
0x00                   -> 0x00 0xFF
```

Terminate the field with:

```text
0x00 0x00
```

This preserves lexicographic byte-string ordering and keeps the field self-delimiting.

---

# 116. Composite Keys

## LOCKED

Composite user keys concatenate component encodings in schema order:

```text
field0 || field1 || ... || fieldN
```

The resulting byte sequence remains lexicographically order preserving.

No per-comparison type dispatch is required in the B+ tree.

---

# 117. Maximum Encoded User Key

## LOCKED for v1

Maximum encoded user-key size:

```text
1024 bytes
```

If encoding exceeds this limit, reject the index operation clearly.

Do not silently truncate an index key.

Reason:

- preserves useful fanout on 8 KiB pages,
- avoids overflow-key pages in v1,
- keeps split/merge behavior tractable.

Possible later work:

- separator prefix truncation,
- overflow keys,
- compression,
- alternative page sizes.

---

# 118. Physical-Key Comparison and Search Sentinels

## LOCKED

Compare physical keys:

```text
1. compare encoded user-key bytes
2. if equal, compare RID
```

Define conceptual:

```text
MIN_RID
MAX_RID
```

which are search sentinels and need not be real persisted RIDs.

All physical entries matching SQL key `K` occupy:

```text
(K, MIN_RID) ... (K, MAX_RID)
```

Therefore equality and duplicate scans remain correct even when duplicate SQL keys span many leaf pages.

---

# 119. Node Page Organization

## LOCKED

Both internal and leaf nodes are:

```text
8 KiB pages
32-byte common page header
64-byte total node header
slotted variable-length entries
```

Slots grow upward from byte 64.

Packed entries grow downward from the end of the page.

Conceptually:

```text
┌──────────────────────────────┐
│ common header                │
│ B+ tree node header          │
├──────────────────────────────┤
│ slot 0                       │
│ slot 1                       │
│ ...                          │
│                         lower│
├──────────────────────────────┤
│ free space                   │
├──────────────────────────────┤
│upper                         │
│ packed entry bytes           │
│ ...                          │
└──────────────────────────────┘
```

The slot array is kept in physical-key sorted order.

Packed entry bytes need not be physically ordered.

---

# 120. B+ Tree Slot Entry

## LOCKED: 8 bytes

```text
offset  size  field
------  ----  ----------------
0       2     entry_offset
2       2     entry_length
4       2     user_key_length
6       2     flags
-------------------------------
total   8 bytes
```

Insertion in the middle should normally shift only slot descriptors, not all existing key payloads.

Page compaction can repack entry bytes and rewrite offsets while holding the page write latch.

---

# 121. Leaf Header

## LOCKED: 64 bytes total

```text
offset  size  field
------  ----  ----------------
0..31         common page header
32      2     level = 0
34      2     slot_count
36      2     lower
38      2     upper
40      4     flags
44      8     prev_leaf_page_no
52      8     next_leaf_page_no
60      4     reserved
-------------------------------
total   64 bytes
```

Leaf level is always `0`.

---

# 122. Leaf Entry

## LOCKED

Leaf packed entry:

```text
encoded user key     variable bytes
RID                  16 bytes
```

Therefore:

```text
entry_length = user_key_length + 16
```

The B+ tree stores no `xmin`, `xmax`, or snapshot metadata.

---

# 123. Internal Header

## LOCKED: 64 bytes total

```text
offset  size  field
------  ----  ----------------
0..31         common page header
32      2     level (> 0)
34      2     slot_count
36      2     lower
38      2     upper
40      4     flags
44      8     leftmost_child_page_no
52      12    reserved
-------------------------------
total   64 bytes
```

An internal node with `N` separator entries has:

```text
N + 1 children
```

---

# 124. Internal Entry

## LOCKED

Each internal entry contains:

```text
encoded user key       variable bytes
separator RID          16 bytes
right_child_page_no     8 bytes
```

Thus every separator is a complete physical key:

```text
(user key, RID)
```

This is required for correct routing when equal SQL keys span subtrees.

---

# 125. Internal Representation and Separator Semantics

## LOCKED: routing lower bounds

Represent an internal node conceptually as:

```text
C0, (K1 -> C1), (K2 -> C2), ... (Kn -> Cn)
```

Invariant:

```text
all keys in C0 < K1

for each i > 0:
    all keys in Ci >= Ki

and, when K(i+1) exists:
    all keys in Ci < K(i+1)
```

`Ki` is a routing lower bound for child `Ci`.

Immediately after a split it normally equals the minimum physical key of `Ci`.

After ordinary deletes, it may remain stale-low.

Example:

```text
separator = 50
right subtree minimum was 50
delete 50
new right minimum = 60
```

Keeping separator `50` remains correct because every right-subtree key is still `>= 50`.

Searches in `[50,60)` may take an unnecessary path and find nothing, but no real key is missed.

This avoids parent rewrites on many deletes.

---

# 126. When a Separator Must Change

## LOCKED

A separator need not be tightened merely because the right-child minimum increased after deletion.

It **must** change when a structural operation moves entries across the routing boundary.

Examples:

- redistribution from left to right,
- redistribution from right to left,
- replacement of the right child,
- split installing a new child.

After redistribution between adjacent children, the simplest correct separator is:

```text
first physical key of the new right child
```

---

# 127. Internal Search

## LOCKED

For target physical key `T`, use binary search equivalent to:

```text
upper_bound(separators, T)
```

Routing:

```text
T < K1            -> C0
K1 <= T < K2      -> C1
K2 <= T < K3      -> C2
...
Kn <= T           -> Cn
```

Production internal-node lookup must not be a linear scan.

---

# 128. Leaf Search

## LOCKED

Use binary search:

```text
lower_bound(physical_keys, target)
```

Exact physical lookup:

```text
target = (encoded_user_key, RID)
```

SQL-key equality lower bound:

```text
target = (encoded_user_key, MIN_RID)
```

Then scan forward until the user key changes.

---

# 129. Split Trigger and Compaction

## LOCKED

Before splitting because an insertion does not fit:

```text
if contiguous free bytes are insufficient
and fragmented/reclaimable bytes would be sufficient:
    compact page
```

Split only if the entry still cannot fit afterward.

Variable-length node capacity is measured in bytes, not entry count.

---

# 130. Leaf Split

## LOCKED: byte-balanced

When a leaf must split:

1. include the pending new entry in the conceptual sorted entry set,
2. choose a boundary that makes byte usage approximately 50/50,
3. require at least one entry on each page,
4. keep lower keys in the original left page,
5. move higher keys to a new right page,
6. copy the first physical key of the right page into the parent as separator.

The separator remains in the right leaf.

Because keys vary in size, splitting purely by number of entries is explicitly disallowed.

---

# 131. Leaf Sibling Links on Split

## LOCKED

Before:

```text
P <-> L <-> N
```

After splitting `L` into `L` and `R`:

```text
P <-> L <-> R <-> N
```

Set:

```text
R.prev = L
R.next = old L.next
L.next = R

if N exists:
    N.prev = R
else:
    superblock.last_leaf = R
```

Both forward and backward links are persisted, although v1 only exposes forward scans.

---

# 132. Internal Split

## LOCKED

Given:

```text
C0, K1->C1, ... Km->Cm, ... Kn->Cn
```

choose promoted physical key `Km` by byte balance.

Left node keeps:

```text
C0
K1->C1
...
K(m-1)->C(m-1)
```

Promote to parent:

```text
Km
```

Right node gets:

```text
leftmost_child = Cm
K(m+1)->C(m+1)
...
Kn->Cn
```

Unlike a leaf split, the promoted internal separator is removed from the child level.

---

# 133. Root Split and Contraction

## LOCKED

### Root split

When the root splits:

```text
allocate new internal root
old root -> leftmost child
new split page -> right child
install one separator
new_root.level = old_root.level + 1
update root_page_no
update tree_height
```

### Root contraction

If an internal root reaches zero separator entries:

```text
its single child becomes the root
old root is retired/recycled
height decreases by one
```

If a leaf root becomes empty, keep it as the empty root with height 1.

---

# 134. Occupancy and Underflow

## LOCKED

Occupancy is byte based:

```text
used_bytes =
    slot_directory_bytes
    +
    packed_entry_bytes
```

relative to:

```text
PAGE_SIZE - 64
```

Non-root nodes become rebalance candidates below approximately:

```text
25% occupancy
```

This is a soft practical threshold, not a strict textbook half-full invariant.

Reasons:

- variable-length keys,
- reduced split/merge oscillation,
- lower write amplification.

A temporarily sparse page is a performance issue, not a correctness failure.

---

# 135. Redistribution and Merge Policy

## LOCKED

For an underfull page:

1. inspect an adjacent sibling with the same parent,
2. prefer redistribution when it can restore healthy occupancy,
3. otherwise merge if the combined entries fit in one page,
4. propagate parent underflow when a separator is removed.

After redistribution, update the relevant parent separator because keys crossed the routing boundary.

After a plain delete with no key movement across children, separator tightening is unnecessary.

---

# 136. Leaf Merge

## LOCKED

Prefer deterministic:

```text
merge right into left
```

when combined bytes fit.

Update:

```text
left.next = right.next

if right.next exists:
    right.next.prev = left
else:
    superblock.last_leaf = left
```

Remove from the parent the separator and child pointer corresponding to `right`.

Then retire `right`.

---

# 137. Internal Rebalancing

## LOCKED

Internal redistribution/merge must be implemented in terms of conceptual:

```text
children + routing separators
```

rather than blindly moving serialized slot bytes.

The parent separator participates in the child boundary and must be transformed correctly.

Prefer clear reconstruction of the affected node entries until correctness is proven; optimize byte movement only after benchmarking.

---

# 138. Tree-Local Free Page List

## LOCKED

The B+ tree superblock stores:

```text
free_page_head
```

A retired B+ page becomes:

```text
page_type = BTREE_FREE
```

and stores:

```text
next_free_page_no
```

Allocation:

```text
if free list non-empty:
    reuse free page
else:
    append page to index file
```

Do not introduce a global extent allocator solely for B+ tree pages in v1.

---

# 139. Safe Page Reuse

## LOCKED

A page may be recycled only after:

- it is detached from every installed parent/root/sibling route that could legally reach it,
- the write latches needed for detachment are held,
- no traversal is allowed to retain a naked stale page reference.

This relies on latch-coupled traversal and leaf handoff.

Do not expose long-lived raw `PageNo` cursors outside the B+ tree implementation.

---

# 140. Point Lookup Concurrency

## LOCKED: read latch coupling

Traversal:

```text
read-latch parent
    ↓
determine child
    ↓
pin + read-latch child
    ↓
release parent
```

Repeat to leaf.

The child must be safely pinned and latched before the parent's latch is released.

---

# 141. Write Concurrency

## LOCKED: write latch crabbing

Initial insert/delete uses top-down write latch crabbing.

Conceptually:

```text
write-latch parent
    ↓
write-latch child
    ↓
if child is safe for this operation:
    release older ancestors
else:
    retain required ancestors
```

### Insert safety

A node is safe when the current insertion cannot force a split to propagate above it.

For internal nodes this must account for the possible separator insertion caused by a child split.

### Delete safety

A node is safe when the current deletion cannot require redistribution/merge that propagates upward.

This is preferred over a tree-wide write lock and is sufficiently realistic to benchmark contention.

---

# 142. Root Metadata Synchronization

## LOCKED

Each B+ tree has a small root-metadata latch protecting:

- root replacement,
- height changes,
- structurally necessary first/last leaf updates,
- free-list head updates where needed.

Normal traversal holds this metadata latch only long enough to safely obtain/pin the current root.

Do not hold a tree-global latch throughout ordinary point operations.

---

# 143. Latch Ordering

## LOCKED

### Vertical

Acquire:

```text
parent before child
```

### Horizontal leaf operations

Acquire adjacent leaves:

```text
left to right in key order
```

If an operation discovers it would need to acquire an earlier/left page while already holding a later/right page:

```text
release and restart
```

rather than wait in the opposite order.

This avoids a major class of structural deadlocks.

---

# 144. Forward Range Scan

## LOCKED

Version 1 natively supports ascending B+ tree scans.

Cursor state conceptually contains:

```text
current ReadPageGuard
current slot index
encoded upper bound
bound inclusivity
```

A tuple/key view from the current leaf must not outlive the leaf page guard.

---

# 145. Leaf Handoff During Range Scan

## LOCKED: latch coupled

At the end of leaf `L`:

1. while holding `L` read latch, read `L.next`,
2. if invalid, finish,
3. pin + read-latch the next leaf,
4. validate page type/level,
5. release `L`,
6. continue.

Holding the current leaf until the next leaf is safely latched prevents a concurrent merge from detaching/reusing the next page during handoff.

---

# 146. Reverse Scans

## DEFERRED

`prev_leaf_page_no` is maintained from day one, but native descending scans are not required for the first milestone.

Bidirectional latch coupling introduces additional deadlock-order concerns.

Initially:

```text
ORDER BY indexed_columns DESC
```

may use executor sorting.

A later optimization can add reverse cursors with a restart/nonblocking latch protocol.

---

# 147. B+ Tree API Boundary

## LOCKED conceptually

Physical B+ tree operations approximately:

```cpp
Insert(encoded_user_key, Rid)
Erase(encoded_user_key, Rid)

FindPhysical(encoded_user_key, Rid)

LowerBound(encoded_user_key, RidBound)
Scan(lower_bound, upper_bound)
```

Possible convenience operation:

```text
FindAllUserKey(K)
```

implemented as a physical range.

The B+ tree does not own:

- SQL AST nodes,
- tuple visibility rules,
- user-transaction uniqueness semantics.

---

# 148. IndexKeyCodec Boundary

## LOCKED

`IndexKeyCodec` owns:

```text
logical indexed values
    ↓
memcomparable encoded user key
```

It is responsible for:

- type-specific ordering encoding,
- NULL ordering,
- composite encoding,
- key-size validation,
- FLOAT64 canonicalization,
- VARCHAR binary collation.

The B+ tree itself receives opaque encoded key bytes and RID values.

---

# 149. Duplicate SQL Keys

## LOCKED

The physical tree always permits duplicate user keys because physical keys differ by RID:

```text
("Smith", RID 10)
("Smith", RID 21)
("Smith", RID 44)
```

They appear consecutively in RID order.

This remains true even when the index implements a SQL `UNIQUE` constraint, because obsolete or in-progress tuple versions may coexist physically.

---

# 150. Unique Index Enforcement

## LOCKED architectural boundary

Do not enforce uniqueness with:

```text
if BTree.Contains(user_key):
    reject
```

Matching entries may reference:

- globally dead versions,
- aborted versions,
- versions created by the same transaction,
- in-progress conflicting transactions.

Correct shape:

```text
acquire logical unique-key lock/reservation
    ↓
scan physical entries with matching user key
    ↓
fetch heap versions
    ↓
consult MVCC/transaction state
    ↓
detect visible/in-progress conflict
    ↓
insert if permitted
```

The exact key-lock protocol belongs to the upcoming transaction architecture.

---

# 151. UNIQUE and NULL

## LOCKED for v1

For ordinary SQL UNIQUE semantics:

```text
if any indexed key component is NULL:
    duplicate user keys are allowed
```

Only fully non-NULL keys participate in duplicate rejection.

The B+ tree still stores and orders NULL-containing keys normally.

---

# 152. MVCC Interaction

## LOCKED

Index entries contain:

```text
encoded user key -> RID
```

Heap tuple versions contain:

```text
xmin
xmax
cmin
cmax
...
```

Therefore an ordinary index scan is:

```text
B+ tree candidate
    ↓
RID
    ↓
heap fetch
    ↓
MVCC visibility check
    ↓
return/reject
```

A B+ tree hit alone is never proof that a SQL row is visible.

---

# 153. Index-Only Scans

## DEFERRED

Because visibility lives in the heap, v1 cannot safely return arbitrary rows purely from the index.

True index-only scans require additional machinery such as:

- visibility map / all-visible page metadata,
- covering payload columns.

This is deliberately deferred.

---

# 154. UPDATE and DELETE Interaction

## LOCKED

With heap-version MVCC and physical RIDs:

### UPDATE

```text
new heap tuple version
    ↓
new RID
    ↓
new physical index entry
```

Even when indexed column values do not change, v1 creates the new `(key,new_RID)` entry.

Old entries remain until vacuum.

### DELETE

Logical SQL deletion marks heap MVCC metadata.

It does not synchronously remove every B+ tree entry.

Vacuum performs physical cleanup later.

---

# 155. Vacuum Index Cleanup

## LOCKED

Before reclaiming a globally dead heap tuple version, vacuum can derive each indexed user key from the tuple and call:

```text
Erase(encoded_user_key, dead_RID)
```

Because RID participates in the physical key, deletion is exact even when many equal SQL keys exist.

---

# 156. HOT-Like Optimization

## DEFERRED, explicitly planned

A later high-learning optimization should allow some updates that do not modify indexed columns to avoid inserting new B+ tree entries.

That requires a heap-local redirection/version-chain design similar in spirit to HOT.

Do not implement it before the baseline MVCC/index interaction is working and measurable.

---

# 157. Index Scan Cost Awareness

## LOCKED

Secondary-index ordering does not imply heap-page locality.

A range scan may produce many random heap-page accesses.

The optimizer must eventually model this cost instead of assuming:

```text
index exists => index scan is faster
```

Potential later execution improvements:

- RID batching,
- heap-page-aware fetch ordering where semantics permit,
- covering indexes,
- clustered storage.

---

# 158. Prefix Compression

## DEFERRED

Initial leaves and internal pages store full encoded user keys.

High-value future optimizations:

### Internal separator prefix truncation

Store the shortest separator prefix that still distinguishes left and right routing ranges.

Benefit:

```text
higher internal fanout
shallower tree
fewer upper-level cache misses
```

### Leaf prefix/suffix compression

Possible techniques:

- common page prefix,
- restart points,
- suffix compression for composite keys.

Do not add these before structural correctness and recovery.

---

# 159. Internal and Leaf Search Performance

## LOCKED

Use binary search on the slot directory.

Hot comparison path should primarily be:

```text
lexicographic byte comparison
```

plus RID comparison when user keys are equal.

Do not use a linear scan in production nodes.

During a range scan:

```text
binary search once for starting position
then iterate sequentially
```

Do not binary-search for every next entry.

---

# 160. Root/Internal Page Caching

## LOCKED

Root and internal pages remain ordinary buffer-pool pages.

Do not build a second secret B+ tree cache.

Frequently accessed upper pages should naturally become hot under the buffer replacement policy.

If profiling later proves this insufficient, add explicit buffer hints rather than bypassing the buffer manager.

---

# 161. Structural Modification vs User Transaction

## LOCKED

Distinguish:

```text
logical index action
```

from:

```text
structural modification operation (SMO)
```

Example:

```text
user INSERT
    logical action:
        insert (K,RID)

    possible SMOs:
        split leaf
        split internal node
        replace root
```

If the user transaction later aborts:

```text
logical inserted entry may be undone
```

but:

```text
the valid split tree shape does not need to be physically undone
```

The tree may remain more fragmented/taller than before while remaining correct.

---

# 162. WAL Direction for Structural Operations

## LOCKED architectural direction

B+ tree structural changes will be treated as recovery-safe **system structural actions** rather than ordinary user state that must be rolled back on user abort.

Target model is analogous in spirit to:

```text
nested top actions / system transactions
```

in mature recovery systems.

The exact WAL record format and mini-transaction protocol are deferred to the WAL/recovery architecture stage.

This choice means:

```text
user transaction abort:
    undo logical index entry

tree split caused during that transaction:
    may remain
```

---

# 163. Page LSN and WAL Ordering

## LOCKED

Every modified B+ tree page participates in page-LSN/WAL-before-data rules.

This includes:

- leaf insert/delete,
- internal insert/delete,
- split,
- merge,
- redistribution,
- sibling-link updates,
- root changes,
- free-list changes,
- B+ tree superblock changes.

BufferPool remains the single enforcement point:

```text
durable WAL >= page.page_lsn
before
dirty page reaches data file
```

---

# 164. Structural Publication Invariant

## LOCKED

Concurrent runtime operations must never observe:

- a child pointer to an uninitialized page,
- a sibling pointer to an uninitialized page,
- a root pointer to an uninitialized page,
- a detached child before replacement routing is installed.

Initialize new pages first.

Publish routing pointers only while appropriate structural latches are held.

Crash atomicity is handled later through WAL/recovery; runtime atomicity is handled through latch ordering and publication order.

---

# 165. Page Validation

## LOCKED

Opening a node validates at least:

```text
page type
format version
self page_no
header size
level/type consistency
lower <= upper
slot directory bounds
entry offsets/lengths
user_key_length <= entry_length
child PageNo validity where applicable
```

Debug/verifier builds should additionally validate sorted order.

Corruption must produce a controlled corruption error, not undefined behavior.

---

# 166. Full Tree Verifier

## LOCKED

Implement a verifier early.

It should check:

1. every leaf is level 0,
2. every internal child is exactly one level lower,
3. all leaves occur at the same depth,
4. leaf physical keys are sorted,
5. internal separators are sorted,
6. routing-lower-bound invariants hold,
7. global leaf-chain order matches tree order,
8. `first_leaf` and `last_leaf` are correct,
9. every reachable child page is the correct page type,
10. no reachable page is on the free list,
11. parent child counts are consistent,
12. obvious orphaned allocated pages are reported where detectable.

Run this verifier often in randomized tests.

---

# 167. B+ Tree Milestone 1

## LOCKED

Single-threaded persistent tree through the real buffer pool must support:

```text
create/reopen
point lookup
insert
leaf split
internal split
root split
forward range scan
exact physical delete
redistribution
merge
root contraction
free-page reuse
```

Do not add concurrency before this milestone survives heavy randomized verification.

---

# 168. B+ Tree Milestone 2

## LOCKED

Add:

```text
read latch coupling
write latch crabbing
concurrent point lookup
concurrent insert
concurrent delete
forward range scans during writes
deadlock/restart tests
```

Benchmark contention before considering a more optimistic tree algorithm.

---

# 169. B+ Tree Milestone 3

## LOCKED

Integrate:

```text
MVCC heap visibility
transactional unique-key enforcement
vacuum index cleanup
WAL
crash recovery
```

Only after this stage is the B+ tree fully transactionally durable.

---

# 170. Required Deterministic Tests

## LOCKED

Structural cases:

- insert without split,
- leaf split,
- repeated leaf splits,
- internal split,
- cascading split,
- root split,
- redistribution left-to-right,
- redistribution right-to-left,
- leaf merge,
- internal merge,
- root contraction,
- free-page reuse.

Key-format cases:

- negative/positive INT32,
- INT64 extremes,
- DATE/TIMESTAMP,
- FLOAT64 infinities,
- `-0.0` and `+0.0`,
- NaN canonicalization,
- empty VARCHAR,
- embedded zero bytes,
- composite keys,
- NULLs,
- maximum-size key,
- oversized-key rejection.

---

# 171. Duplicate Stress Test

## LOCKED

Insert enough identical user keys with distinct RIDs to span many leaf pages.

Verify:

```text
equality scan returns all RIDs exactly once
lower bound starts at the first duplicate
upper bound stops after the last duplicate
exact Erase(K,RID) removes only one physical entry
tree remains valid after merges
```

This specifically validates the decision to route using full physical separators `(user_key,RID)`.

---

# 172. Randomized Tests

## LOCKED

Compare the B+ tree to an in-memory sorted oracle of physical keys.

Random operation stream:

```text
insert
erase
point lookup
range lookup
close/reopen
```

Periodically:

```text
run full verifier
compare complete sorted contents against oracle
```

Random seeds must be reproducible and printed on failure.

This test suite is mandatory before concurrency.

---

# 173. Concurrent Tests

## LOCKED

Stress:

- many readers + one writer,
- writers on disjoint ranges,
- writers on one hot range,
- duplicate-heavy inserts,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes.

Use deliberately tiny buffer pools in some tests.

Add watchdogs/timeouts to detect deadlocks.

---

# 174. B+ Tree Benchmarks

## LOCKED

Measure:

```text
random point lookup ops/sec
sorted insertion ops/sec
random insertion ops/sec
exact deletion ops/sec
short range scan rows/sec
long range scan rows/sec
duplicate-heavy equality lookup
split frequency
tree height
average leaf byte occupancy
average internal byte occupancy
buffer-pool hit rate
latch wait time when available
```

Benchmark key shapes:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

Benchmark both:

### Hot tree

Mostly resident in buffer pool.

Focus:

- CPU,
- comparisons,
- latches,
- cache behavior.

### Larger-than-buffer tree

Working set exceeds buffer pool.

Focus:

- fanout,
- page access patterns,
- replacement,
- random I/O sensitivity.

---

# 175. Deliberately Deferred B+ Tree Features

## LOCKED

Do not implement before the baseline tree is correct and benchmarked:

- B-link internal right-links,
- optimistic versioned/latch-free reads,
- lock-free B+ tree algorithms,
- separator prefix truncation,
- leaf compression,
- overflow index keys,
- reverse range scans,
- bulk loading,
- online index build,
- covering/include columns,
- index-only scans,
- partial indexes,
- expression indexes,
- descending physical components,
- locale-aware collations,
- async prefetch,
- parallel index scans.

These are future experiments, not rejected ideas.

---

# 176. First Post-Correctness Optimization Candidates

## LOCKED recommendation

After correctness, profile first.

Likely high-reward experiments:

```text
separator prefix truncation
optimistic/B-link-style read traversal
range-scan leaf prefetch
root/upper-level latch contention reduction
```

Choose based on measurements.

---

# 177. B+ Tree Invariants

## LOCKED

Codex must preserve:

1. Every leaf is level 0.
2. Every internal child is exactly one level below its parent.
3. All physical entries are ordered by `(encoded_user_key,RID)`.
4. Leaf slot directories are sorted by physical key.
5. Internal separator slots are sorted.
6. Internal separators are valid lower routing bounds for right children.
7. `N` internal separators imply `N+1` children.
8. Duplicate SQL keys are legal at the physical tree layer.
9. An index hit never decides MVCC visibility.
10. Forward scans use safe latch-coupled leaf handoff.
11. Published child/sibling/root pointers reference initialized pages.
12. Detached pages are not reused while legal traversals can still reference them.
13. Structural shape changes are separate from logical user-transaction undo.
14. All persistent B+ tree changes participate in page-LSN/WAL ordering.
15. An initialized tree always has a valid root, including when empty.
16. Variable-length split/rebalance decisions are byte based.
17. Oversized encoded user keys are rejected, never silently truncated.
18. B+ tree pages are accessed through BufferPool, never directly through DiskManager.
19. Page payload parsing validates persistent bounds.
20. The full tree can be checked by the explicit verifier.

---

# 178. Decisions Added by the B+ Tree Architecture

## LOCKED

The architecture now commits to:

```text
page-backed B+ tree from day one
one B+ tree file per index
64-byte internal/leaf node headers
8-byte slotted node entries
variable-length encoded keys
memcomparable SQL key encoding
1024-byte encoded user-key maximum
physical order = (user key, RID)
full physical separators including RID
routing-lower-bound separator semantics
binary search within nodes
byte-balanced splits
25% soft underflow threshold
redistribution before merge when beneficial
persistent prev/next leaf links
forward native range scans first
read latch coupling
write latch crabbing
parent-before-child vertical latch order
left-to-right horizontal leaf latch order
tree-local free-page reuse
no MVCC metadata in index entries
uniqueness enforced transactionally above B+ tree
ordinary deletes leave index cleanup to vacuum
structural modifications need not be undone with user abort
future WAL treatment as recovery-safe system structural actions
```

No B+ tree choice currently requires project-owner input.

---

# 179. Next Architecture Stage

The next architecture stage should lock the transaction and durability core **together**:

```text
TransactionManager
Snapshot
VisibilityManager
LockManager
WAL
CommitCoordinator
CheckpointManager
RecoveryManager
Vacuum horizon
```

These systems should not be designed independently because their invariants overlap.

The next decisions include:

- transaction-ID lifecycle,
- snapshot representation,
- exact `xmin/xmax/cmin/cmax` visibility rules,
- transaction-status storage,
- write/write conflict semantics,
- unique-key locking,
- Read Committed vs Repeatable Read behavior,
- WAL record header/layout,
- logical vs physiological logging boundaries,
- log-buffer organization,
- group commit,
- checkpoints,
- redo,
- undo,
- compensation log records,
- B+ tree system-SMO recovery,
- transaction-ID wraparound strategy,
- global vacuum-safe horizon.

---

# 180. Transaction, MVCC, Locking, WAL, Recovery, and Vacuum Contract

## LOCKED

This section defines the transaction/durability subsystem as one coherent architecture.

The following components are designed together:

```text
TransactionManager
SnapshotManager
VisibilityManager
LockManager
TransactionStatusStore
WalManager
CommitCoordinator
CheckpointManager
RecoveryManager
VacuumManager
ReadEpochManager
```

They must agree on:

- transaction identity,
- snapshot visibility,
- tuple `xmin/xmax/cmin/cmax`,
- write/write conflicts,
- uniqueness conflicts,
- commit visibility,
- WAL durability,
- crash recovery,
- B+ tree structural actions,
- garbage collection,
- safe RID reuse.

A local shortcut in one subsystem must not violate another subsystem's invariants.

---

# 181. Architecture Refinement: No Physical User-Transaction Undo

## LOCKED

The earlier generic ARIES-inspired `analysis / redo / undo` direction is refined for the chosen heap-version MVCC architecture.

Version 1 uses:

```text
analysis
    ↓
redo
    ↓
loser-transaction resolution
```

but **does not physically undo ordinary heap/index modifications made by aborted user transactions**.

Why this is correct:

### Aborted INSERT

Tuple:

```text
xmin = aborted transaction
```

is invisible to every other transaction.

Its index entries may remain physically present but resolve to an invisible heap version.

Vacuum removes them later.

### Aborted DELETE

Old tuple:

```text
xmax = aborted transaction
```

is treated as not deleted.

The old row remains visible according to normal snapshot rules.

### Aborted UPDATE

Old version:

```text
xmax = aborted transaction
```

remains logically live.

New version:

```text
xmin = aborted transaction
```

is invisible.

New physical index entries may remain as vacuumable garbage.

Therefore transaction rollback is primarily a **transaction-status decision**, not a byte-for-byte restoration of user pages.

This design:

- removes a large amount of physical undo complexity,
- matches the selected heap-version MVCC philosophy,
- keeps STEAL + NO-FORCE,
- makes aborted-version garbage and vacuum behavior visible,
- avoids CLRs for ordinary user DML in v1.

Physical/system undo is still allowed where a future non-MVCC subsystem genuinely requires it.

---

# 182. Reserved Transaction IDs

## LOCKED

Use 64-bit transaction IDs.

Reserve:

```text
INVALID_TXN_ID = 0
FROZEN_TXN_ID  = 1
FIRST_NORMAL_TXN_ID = 2
```

Normal transaction IDs are monotonically allocated from `2` upward.

`FROZEN_TXN_ID` means:

```text
creator transaction committed so long ago that its original transaction-status lookup is no longer required
```

64-bit transaction IDs make numerical wraparound practically irrelevant for this project.

Freezing still exists to reclaim transaction-status storage and to teach long-lived MVCC metadata management.

---

# 183. Durable Transaction-ID Reservation

## LOCKED: reserve ID blocks

Do not force the control file for every `BEGIN`.

Reserve transaction IDs in blocks.

Initial block size:

```text
1,048,576 transaction IDs
```

Control metadata stores:

```text
reserved_txn_id_end
```

When the current in-memory block is exhausted:

1. calculate a new reservation end,
2. durably update the database control file,
3. only after that durable update begin handing out IDs from the new block.

A crash may permanently skip unused reserved IDs.

It must never reuse an ID that may have appeared in persistent database state.

This gives durable monotonic identity without an fsync per transaction.

---

# 184. Transaction Object

## LOCKED conceptually

A transaction contains at least:

```text
TxnId txn_id
TxnState state
IsolationLevel isolation
CommandId current_command_id
optional<Snapshot> transaction_snapshot
optional<Snapshot> statement_snapshot
Lsn last_wal_lsn
bool has_persistent_writes
held logical locks
cancellation/deadlock flag
```

Initial transaction states:

```text
ACTIVE
COMMITTING
COMMITTED
ABORTING
ABORTED
```

State transitions must be monotonic.

---

# 185. Isolation Levels

## LOCKED

Version 1 supports:

```text
READ COMMITTED
REPEATABLE READ
```

Default:

```text
READ COMMITTED
```

`REPEATABLE READ` implements **snapshot isolation**, not full serializability.

Deferred:

```text
SERIALIZABLE
SSI
predicate locking
```

The documentation and SQL surface must not falsely call snapshot isolation serializable.

---

# 186. Command IDs

## LOCKED

Each SQL statement inside a transaction executes under a `CommandId`.

Initial value:

```text
0
```

Increment once after each completed SQL statement.

Tuple versions store:

```text
cmin
cmax
```

so one transaction can distinguish:

- versions created by an earlier statement,
- versions created by the current statement,
- versions deleted by an earlier statement,
- versions deleted by the current statement.

This prevents same-transaction statement ordering from being collapsed into one visibility state.

---

# 187. Snapshot Representation

## LOCKED

A snapshot contains:

```text
TxnId xmin
TxnId xmax
sorted vector<TxnId> active
TxnId owner_txn_id
CommandId command_id
```

Semantics:

### `xmax`

At snapshot capture:

```text
xmax = next transaction ID that has not yet been assigned
```

Any normal transaction:

```text
txn_id >= xmax
```

started too late to be visible to the snapshot.

### `active`

Contains transactions that were active when the snapshot was captured and have:

```text
txn_id < xmax
```

A transaction in this set remains invisible to this snapshot even if it commits later.

### `xmin`

Conceptually:

```text
minimum transaction ID relevant to the snapshot's active set
```

If no such transaction exists:

```text
xmin = xmax
```

`active` is kept sorted so membership can initially use binary search.

A later high-concurrency implementation may use a hybrid vector/hash representation.

---

# 188. Snapshot Capture Synchronization

## LOCKED

Snapshot capture and transaction registration must use synchronization that prevents this race:

```text
snapshot obtains xmax
another transaction becomes visible as active/inactive inconsistently
snapshot misses it
```

Conceptually the TransactionManager maintains:

```text
next_txn_id
active transaction registry
```

under a short snapshot/registry synchronization protocol.

Snapshot capture must atomically observe:

```text
high-water mark
+
active transaction IDs below that mark
```

Do not hold this synchronization while executing the query.

---

# 189. READ COMMITTED Snapshots

## LOCKED

At the beginning of every SQL statement:

```text
capture a new snapshot
register it as active
```

At statement completion:

```text
unregister statement snapshot
increment command ID
```

Therefore two statements in one transaction may observe different committed database states.

An individual statement sees a stable snapshot for its full execution.

---

# 190. REPEATABLE READ Snapshots

## LOCKED

At the beginning of the first ordinary statement in the transaction:

```text
capture transaction snapshot
```

Reuse it for all later statements.

Update only:

```text
snapshot.command_id
```

for same-transaction command visibility.

The transaction snapshot remains registered until transaction end.

This pins the vacuum horizon as expected for a long-running repeatable-read transaction.

---

# 191. Transaction Status Store

## LOCKED

Persistent terminal transaction status lives in:

```text
txn_status.dat
```

It is a normal page-based database file and uses:

```text
page 0 = superblock
page 1..N = TXN_STATUS pages
```

Status states:

```text
INVALID
COMMITTED
ABORTED
RESERVED
```

`IN_PROGRESS` is primarily represented by the in-memory active transaction registry rather than requiring a durable page write at `BEGIN`.

Two bits per transaction are sufficient for the persistent terminal state.

---

# 192. Transaction Status Page Mapping

## LOCKED

A transaction-status page uses the normal 32-byte common page header.

Payload bytes:

```text
8192 - 32 = 8160 bytes
```

At 4 transaction states per byte:

```text
8160 * 4 = 32,640 transaction IDs/page
```

Mapping must be a deterministic pure function:

```text
TxnId -> status PageNo + byte offset + two-bit position
```

Do not persist an in-memory hash table as the status format.

---

# 193. Transaction Status Lookup

## LOCKED

Status lookup order:

```text
if txn_id == FROZEN_TXN_ID:
    COMMITTED

if txn_id == current transaction:
    SELF

if active transaction registry contains txn_id:
    IN_PROGRESS

otherwise:
    consult cached/persistent terminal status page
```

A tuple that references a normal transaction ID which is:

```text
not active
and
has no terminal status
```

after completed crash recovery indicates corruption or an invariant failure.

Do not silently treat unknown status as committed.

---

# 194. Commit Status Publication

## LOCKED

A transaction must not become visible as committed until its commit WAL record is durable.

Commit order:

```text
append TXN_COMMIT
    ↓
group-flush WAL through commit LSN
    ↓
publish COMMITTED in TransactionStatusStore/cache
    ↓
release logical locks
    ↓
unregister transaction/snapshot
    ↓
return success
```

The status page update itself need not be synchronously flushed.

Its `page_lsn` is set to the durable commit LSN.

If the database crashes before the status page reaches disk, recovery reconstructs the committed state from WAL.

---

# 195. Abort Status Publication

## LOCKED

Abort order:

```text
mark transaction ABORTING
    ↓
append TXN_ABORT if transaction produced persistent WAL-visible state
    ↓
publish ABORTED
    ↓
release logical locks
    ↓
unregister snapshots/transaction
    ↓
return aborted/error
```

An ordinary abort does not require an immediate WAL fsync.

Reason:

- no user commit is being acknowledged,
- page flushing still obeys WAL-before-data,
- if the abort record is lost in a crash, recovery treats the transaction as a loser and marks it aborted again.

---

# 196. Read-Only Transactions

## LOCKED

A transaction that never creates persistent state:

```text
does not require a TXN_COMMIT WAL record
does not require a terminal status-page entry
```

It still:

- owns a TxnId,
- participates in active snapshot management,
- may pin the vacuum horizon.

Durable TxnId block reservation prevents harmful ID reuse even when a read-only transaction leaves no WAL record.

---

# 197. Tuple Visibility: Creator Rule

## LOCKED

Given tuple version `T` and snapshot `S`, first decide whether `T.xmin` makes the version exist for this statement.

Conceptually:

```text
if T.xmin == FROZEN_TXN_ID:
    creator_visible = true

else if T.xmin == S.owner_txn_id:
    creator_visible = (T.cmin < S.command_id)

else:
    status = Status(T.xmin)

    if status != COMMITTED:
        creator_visible = false

    else if T.xmin >= S.xmax:
        creator_visible = false

    else if T.xmin is in S.active:
        creator_visible = false

    else:
        creator_visible = true
```

If `creator_visible == false`, the tuple version is invisible regardless of `xmax`.

The strict:

```text
cmin < command_id
```

rule gives statement-level stability inside one transaction.

A `RETURNING` implementation should use the operation's produced values directly rather than relying on rescanning a just-created tuple through ordinary snapshot visibility.

---

# 198. Tuple Visibility: Deleter Rule

## LOCKED

After creator visibility succeeds, inspect `xmax`.

### No deleter

```text
xmax == INVALID_TXN_ID
```

means visible.

### Deleted/updated by current transaction

If:

```text
xmax == snapshot.owner_txn_id
```

then:

```text
if cmax < snapshot.command_id:
    tuple is no longer visible
else:
    tuple remains visible to the current statement snapshot
```

### Other transaction

Lookup `Status(xmax)`.

If:

```text
ABORTED
```

the deletion/update never logically happened.

Tuple remains visible.

If:

```text
IN_PROGRESS
```

tuple remains visible to this snapshot.

Write-conflict handling is a separate concern from read visibility.

If:

```text
COMMITTED
```

then the tuple is considered deleted for snapshot `S` only when:

```text
xmax < S.xmax
and
xmax not in S.active
```

Otherwise the deleting/updating transaction was too new for the snapshot, so the old tuple remains visible.

---

# 199. Visibility Algorithm Summary

## LOCKED

Ordinary visibility is:

```text
creator committed before snapshot?
        no  -> invisible
        yes
         ↓
deleter absent/aborted/in-progress/too-new?
        yes -> visible
        no  -> invisible
```

Reads do not acquire row locks merely to evaluate visibility.

---

# 200. Hint Cleanup

## LOCKED

Vacuum or maintenance may normalize tuple metadata when transaction status is permanently known.

Examples:

### Aborted `xmax`

If:

```text
Status(xmax) == ABORTED
```

vacuum may rewrite:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

### Ancient committed creator

Vacuum may rewrite:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

when freeze rules permit.

These are physical cleanup optimizations.

They must be WAL logged as page changes.

---

# 201. Logical Lock Types

## LOCKED

Version 1 LockManager needs only exclusive logical locks for:

```text
TUPLE_WRITE
UNIQUE_KEY
```

Readers use MVCC and do not acquire shared tuple locks.

Future DDL/table locking may add:

```text
TABLE
INTENTION
SCHEMA
KEY_RANGE
```

but these are deferred.

---

# 202. Tuple Write Lock Key

## LOCKED

A tuple write lock is keyed by:

```text
(TableId, physical target RID)
```

The target RID is the currently visible tuple version that the transaction intends to UPDATE or DELETE.

Concurrent writers that begin from the same visible version therefore conflict on the same logical lock.

Because new versions receive new RIDs, the updater must re-fetch and revalidate the old version after acquiring/waiting for the lock.

---

# 203. Critical Lock/Latch Rule

## LOCKED

Never wait for a logical transaction lock while holding:

- heap page latch,
- B+ tree page latch,
- buffer-frame latch,
- structural B+ tree latch.

Correct pattern:

```text
identify candidate RID
    ↓
release page/index latches
    ↓
acquire logical lock
    ↓
re-fetch candidate
    ↓
re-check visibility/header/predicate
    ↓
perform mutation
```

This prevents transaction lock waits from blocking low-level page structure progress and avoids lock/latch deadlock coupling.

---

# 204. UPDATE / DELETE Write Conflict Protocol

## LOCKED

For a candidate visible tuple version `R`:

1. release short-lived page/index latches,
2. acquire `TUPLE_WRITE(TableId,RID)` exclusively,
3. re-fetch `R`,
4. re-check tuple identity and visibility,
5. inspect current `xmax`.

Cases:

### `xmax == 0`

Proceed.

### `xmax` belongs to self

Handle as a same-transaction command case; do not create a second independent writer.

### `xmax` belongs to an aborted transaction

Treat it as ineffective.

The new writer may overwrite the aborted `xmax/cmax` metadata.

### `xmax` belongs to an in-progress transaction

The lock protocol should normally already have serialized this case.

If discovered because of a race, wait/retry without holding page latches.

### `xmax` belongs to a committed competing updater

Apply the isolation-specific rule below.

---

# 205. READ COMMITTED Write Conflict Behavior

## LOCKED

If a writer waited and discovers that another transaction committed an UPDATE/DELETE of its target:

```text
restart the affected statement's candidate search using a fresh READ COMMITTED statement snapshot
```

The executor must re-evaluate:

- the row version,
- predicates,
- index conditions,
- generated values as required.

This is an internal statement retry.

Do not blindly update the stale physical version.

A retry may ultimately find:

- a newer row that still matches,
- a newer row that no longer matches,
- no row.

---

# 206. REPEATABLE READ Write Conflict Behavior

## LOCKED

Under snapshot isolation, if the target row was changed by another transaction that committed after the transaction's fixed snapshot:

```text
abort with a serialization/write-conflict error
```

This is a first-updater-wins rule.

Do not silently move the write to a version that the transaction snapshot could not see.

---

# 207. Lost-Update Prevention

## LOCKED

The combination of:

```text
exclusive tuple-write locks
+
revalidation after waiting
+
READ COMMITTED statement restart
+
REPEATABLE READ conflict abort
```

must prevent lost updates.

Write skew across different rows remains possible under snapshot isolation and is one reason SERIALIZABLE is deferred to a later SSI/predicate-locking milestone.

---

# 208. Unique-Key Lock Key

## LOCKED

For a non-NULL SQL unique key:

```text
UniqueLockKey =
    (IndexId, full encoded user-key bytes)
```

A hash may be used to select a lock-table shard, but collision resolution must compare the full key.

Do not enforce uniqueness using only a 64-bit hash.

---

# 209. Unique-Key Lock Protocol

## LOCKED

Any DML operation that may create or remove a fully non-NULL unique key acquires the corresponding `UNIQUE_KEY` lock.

Examples:

```text
INSERT unique K
UPDATE old K -> new K
DELETE unique K
```

Locks are held until transaction end.

When updating from one unique key to another, acquire keys in deterministic encoded-key order where both are known in advance to reduce deadlocks.

The deadlock detector remains the correctness fallback.

NULL-containing unique keys skip duplicate rejection under the v1 SQL semantics already locked in the B+ tree section.

---

# 210. Unique Check Semantics

## LOCKED

After acquiring the unique-key lock:

1. scan all physical B+ tree entries with the encoded user key,
2. fetch every referenced heap tuple version,
3. inspect current transaction status rather than only the caller's snapshot,
4. reject if another logically live row owns the key,
5. ignore globally aborted/dead versions.

Uniqueness is a constraint on the database's current transactional state, not merely on what the caller's historical snapshot can see.

If an unexpected in-progress conflicting creator is encountered, wait/retry through the lock protocol.

---

# 211. Lock Duration

## LOCKED: strict write locking

Tuple-write and unique-key locks are held until:

```text
COMMIT
or
ABORT
```

Do not release them immediately after the page modification.

This gives straightforward write conflict semantics and prevents another writer from acting on a transaction whose outcome is still unknown.

---

# 212. Lock Table

## LOCKED

Initial LockManager may use:

```text
hash map<LockKey, LockQueue>
```

with a conventional mutex.

Each queue contains:

```text
current owner
FIFO waiter list
```

Since all v1 logical lock modes are exclusive, lock compatibility is initially simple.

The abstraction must allow future:

- sharding,
- shared modes,
- intention locks.

Do not start with a lock-free lock table.

---

# 213. Deadlock Detection

## LOCKED: wait-for graph

When a transaction blocks:

```text
waiter -> current owner
```

creates a wait-for dependency.

Run cycle detection when adding blocking edges and/or through a lightweight detector thread.

Initial victim policy:

```text
abort the youngest transaction
= highest TxnId in the detected cycle
```

The victim receives a cancellation flag and blocked waits are awakened.

Deadlock abort goes through the normal transaction abort/status path.

Timeouts are diagnostic/fallback behavior, not the primary deadlock correctness mechanism.

---

# 214. B+ Tree Latches Are Not LockManager Locks

## LOCKED

Reiterate the separation:

```text
B+ tree/heap latches:
    microsecond-scale physical structure protection

LockManager locks:
    transaction-lifetime logical conflict protection
```

Never put page latches into the LockManager.

Never use tuple logical locks to protect B+ tree page structure.

---

# 215. WAL Physical Organization

## LOCKED: one logical WAL stream, segmented files

Conceptually the database has one:

```text
database WAL stream
```

Physically store it as fixed-size segments:

```text
wal/
  0000000000000000.wal
  0000000000000001.wal
  ...
```

Initial segment size:

```text
64 MiB
```

An LSN is the byte position in the logical WAL stream.

Benefits:

- manageable files,
- simpler recycling,
- bounded file growth units,
- clean recovery scanning.

---

# 216. WAL Record Alignment

## LOCKED

WAL records are 8-byte aligned.

A normal record must not cross a 64 MiB segment boundary.

If insufficient bytes remain in a segment:

```text
emit/recognize padding
continue record at next segment
```

This simplifies:

- record validation,
- torn-tail detection,
- segment recycling.

---

# 217. WAL Record Header

## LOCKED: 48-byte logical header

```text
offset  size  field
------  ----  ----------------
0       4     total_length
4       2     header_length = 48
6       2     record_type
8       4     flags
12      4     reserved
16      8     lsn
24      8     txn_id
32      8     prev_txn_lsn
40      4     payload_length
44      4     crc32c
-------------------------------
total   48 bytes
```

All ordinary WAL integer metadata is explicitly serialized.

CRC32C is computed over:

```text
header with crc field zeroed
+
payload
```

Padding bytes are not semantically part of the payload.

---

# 218. Per-Transaction WAL Chain

## LOCKED

User transaction records contain:

```text
prev_txn_lsn
```

and the Transaction object tracks:

```text
last_wal_lsn
```

Even though v1 does not physically undo user DML, this chain is valuable for:

- diagnostics,
- tracing,
- future logical rollback features,
- future savepoints,
- recovery introspection.

System records use:

```text
txn_id = 0
prev_txn_lsn = 0
```

unless a future subsystem requires a separate system chain.

---

# 219. WAL Record Types

## LOCKED initial set

At minimum:

```text
WAL_PAD

PAGE_INIT
PAGE_DELTA
PAGE_IMAGE

BTREE_MTR

TXN_COMMIT
TXN_ABORT

CHECKPOINT_BEGIN
CHECKPOINT_DATA
CHECKPOINT_END
```

Future additions may specialize page deltas by subsystem for compactness.

The first architecture should avoid dozens of bespoke WAL record classes before the generic recovery machinery works.

---

# 220. PAGE_DELTA Record

## LOCKED

A `PAGE_DELTA` is a physiological redo record for one page.

Payload contains:

```text
PageId
expected page type
patch_count
patches...
```

Each patch:

```text
offset
length
after-image bytes
```

Only redo data is required for ordinary MVCC user changes.

The page modification code must ensure patches completely describe the persistent bytes changed by the operation.

Recovery applies the delta only when:

```text
page.page_lsn < record.lsn
```

then sets:

```text
page.page_lsn = record.lsn
```

---

# 221. PAGE_INIT Record

## LOCKED

Initializing a new database page logs enough information to reconstruct the entire valid page.

Payload:

```text
PageId
full 8192-byte page after-image
```

Use for:

- newly allocated heap pages,
- FSM/status/catalog pages,
- other non-B+ tree page initialization.

B+ tree multi-page initialization may occur inside a `BTREE_MTR`.

---

# 222. Torn-Page Protection

## LOCKED: first-modification full-page images

Page checksums alone detect torn writes but cannot repair them.

Version 1 adds a PostgreSQL-style learning concept with our own explicit semantics:

> On the first modification of a persistent page after a completed checkpoint epoch, WAL must contain a full **after-image** of that page.

For that first modification:

```text
PAGE_IMAGE
```

replaces the ordinary delta for the page.

For a B+ tree mini-transaction, the MTR embeds a full after-image for affected pages whose first-modification image is required.

After the first image in that checkpoint epoch, later modifications may use compact deltas.

---

# 223. Full-Page Image Semantics

## LOCKED

`PAGE_IMAGE` contains:

```text
PageId
full 8192-byte after-image
```

and represents the complete page state at that record's LSN.

During recovery:

```text
if page is corrupt/torn
or page_lsn < image_lsn:
    replace page with WAL image
```

then replay later deltas normally.

Buffer-frame metadata tracks the checkpoint/FPI epoch for each resident page.

A newly created `PAGE_INIT` already provides a full image and satisfies the full-page requirement for that initial state.

---

# 224. Why Full-Page Images Are Checkpoint-Epoch Based

## LOCKED rationale

Suppose:

1. checkpoint completes,
2. page is first modified,
3. WAL logs full after-image,
4. page is modified several more times with deltas,
5. a later data-page write tears during crash.

Recovery can:

```text
restore first post-checkpoint full image
    ↓
redo later page deltas
    ↓
reconstruct latest durable WAL state
```

This gives recoverability from torn 8 KiB writes without introducing a separate doublewrite buffer in v1.

---

# 225. B+ Tree Mini-Transactions

## LOCKED

All physical B+ tree mutations execute inside short **B+ tree mini-transactions (MTRs)**.

Examples:

- ordinary leaf entry insert,
- ordinary exact entry erase,
- leaf split,
- internal split,
- redistribution,
- merge,
- root replacement,
- free-list modification.

A B+ MTR is a **system structural action**:

```text
it is redoable
it is not rolled back when the owning user transaction aborts
```

User transaction visibility remains controlled by the referenced heap tuple version.

---

# 226. Atomic BTREE_MTR WAL Record

## LOCKED

A complete B+ mini-transaction is encoded as one logical `BTREE_MTR` WAL record.

Payload contains:

```text
optional owner user TxnId for diagnostics
affected page count
for each page:
    PageId
    page type
    either:
        full after-image
    or:
        list of redo byte patches
allocation/free metadata changes
```

All pages modified by the MTR receive:

```text
page_lsn = BTREE_MTR.lsn
```

This creates one recovery atomicity boundary for the entire structural action.

---

# 227. B+ Mini-Transaction No-Flush Barrier

## LOCKED

While a B+ MTR is being constructed:

```text
affected frames may not be written to disk
```

even though they are latched/pinned.

Implementation concept:

```text
MiniTxnPageGuard / no-flush barrier
```

Protocol:

1. acquire/latch affected pages,
2. mutate them into a valid final runtime state,
3. build `BTREE_MTR` redo payload,
4. append the complete WAL record,
5. set all affected `page_lsn` values to MTR LSN,
6. release no-flush barriers,
7. release page latches/pins.

If the process crashes before step 4 finishes:

- the WAL record is absent/torn and ignored,
- affected dirty pages were not eligible for data-file write,
- no partial structural state became durable.

No fsync is required for every MTR.

Normal buffer eviction may later force WAL durability through the MTR LSN.

---

# 228. B+ Tree Changes Survive User Abort

## LOCKED

Suppose an uncommitted INSERT creates:

```text
heap tuple version xmin = T
B+ tree entry -> new RID
```

and insertion causes a split.

If transaction `T` aborts:

```text
heap version is invisible
index entry may remain temporarily
split remains
```

Vacuum later removes the exact `(key,RID)` entry.

Therefore user abort never attempts to reverse a tree split/merge/root change.

---

# 229. Heap WAL Ordering Relative to Index WAL

## LOCKED

When creating a new tuple version and its index entries:

```text
log/establish heap tuple version first
then perform B+ tree MTRs referencing its RID
```

A later index MTR LSN is therefore after the WAL describing the referenced heap version.

If an index page is forced to disk before user commit, WAL flush-through of that later MTR also makes the earlier heap redo record durable.

The user transaction may still abort; visibility handles that safely.

---

# 230. WAL Buffer

## LOCKED

WalManager owns an in-memory append buffer.

Initial target size:

```text
8 MiB
```

configurable.

Append:

```text
serialize record
assign LSN
copy to WAL buffer
advance written/reserved position
```

Initial implementation may serialize append under one mutex.

Architecture must allow later:

- atomic space reservation,
- per-thread staging buffers,
- larger ring-buffer implementation.

Do not begin with lock-free WAL reservation before correctness.

---

# 231. WAL Writer / Flusher

## LOCKED

A dedicated WAL writer/flusher thread:

- writes contiguous WAL bytes to segment files,
- performs `fdatasync` when durability is requested,
- advances atomic `durable_lsn`,
- wakes commit waiters whose target LSN is durable.

Non-commit/background WAL may be written periodically even without an explicit commit request.

Initial configurable background flush interval:

```text
~10 ms
```

This is a tuning default, not a persistent format invariant.

---

# 232. Group Commit

## LOCKED

Commit does not call `fdatasync` independently for each transaction.

When multiple commits are waiting:

```text
T1 commit_lsn = 1000
T2 commit_lsn = 1100
T3 commit_lsn = 1250
```

the flusher may:

```text
write through 1250
fdatasync
durable_lsn = 1250
wake T1, T2, T3
```

One durability operation can acknowledge many transactions.

This is mandatory for the performance-first commit path.

---

# 233. Commit Coordinator

## LOCKED

`CommitCoordinator` provides conceptually:

```text
WaitUntilDurable(commit_lsn)
```

It must batch naturally by waiting on monotonic `durable_lsn`, not by creating one fsync task per transaction.

Commit latency instrumentation should record:

```text
queue delay
WAL write time
fdatasync time
group size
```

---

# 234. Synchronous Commit

## LOCKED for v1

A normal successful `COMMIT` returns only after:

```text
TXN_COMMIT WAL record is durable
```

No asynchronous-commit mode initially.

Future experimentation may add:

```text
synchronous_commit = off
```

but its weaker durability must be explicit.

---

# 235. Dirty Page recLSN

## LOCKED

Each buffer frame tracks:

```text
rec_lsn
```

meaning:

```text
the first WAL LSN that made this frame dirty since its last successful data-file flush
```

Transition:

```text
clean -> dirty:
    rec_lsn = modification_lsn

already dirty -> modified:
    keep existing rec_lsn
```

After successful page write and confirmation that no newer in-memory modification raced the flush:

```text
dirty = false
rec_lsn = INVALID_LSN
```

This drives the dirty-page table used by fuzzy checkpoints/recovery.

---

# 236. WAL-Before-Data Restatement

## LOCKED

Before writing any dirty page with:

```text
page_lsn = X
```

BufferPool must guarantee:

```text
WalManager.durable_lsn >= X
```

This applies to:

- heap pages,
- transaction-status pages,
- FSM pages when WAL-protected,
- catalog pages,
- B+ tree pages,
- superblock/control-like page files where routed through BufferPool.

B+ MTR no-flush barriers add the stronger temporary condition that pages are not flushable until their complete MTR WAL record exists.

---

# 237. Database Control File

## LOCKED

Introduce:

```text
database.control
```

Size:

```text
8192 bytes
```

organized as two alternating:

```text
4096-byte control slots
```

Each slot stores at least:

```text
magic
format_version
generation
latest_checkpoint_lsn
checkpoint_redo_lsn
reserved_txn_id_end
next_file_id
control flags
CRC32C
```

The inactive slot is rewritten, synced, then becomes the newest valid generation.

On startup choose the valid slot with the highest generation.

CRC + alternating slots allow recovery from a torn control-file update without requiring atomic 8 KiB writes.

---

# 238. Control File Update Frequency

## LOCKED

Durably update `database.control` only for important global metadata transitions such as:

- transaction-ID block reservation,
- successful checkpoint installation,
- future database-format metadata.

Do not rewrite/sync it for ordinary transactions.

---

# 239. Fuzzy Checkpoint Goal

## LOCKED

A checkpoint must not stop transaction processing merely to flush every dirty page.

It captures enough state so crash recovery can begin from a bounded WAL position.

Transactions and page modifications may continue while the checkpoint is being constructed.

---

# 240. Checkpoint Protocol

## LOCKED

Conceptual checkpoint:

```text
1. append CHECKPOINT_BEGIN
2. capture dirty-page table
3. capture active writer transaction table
4. capture relevant global metadata
5. append one or more CHECKPOINT_DATA records
6. append CHECKPOINT_END
7. flush WAL through CHECKPOINT_END
8. atomically install checkpoint pointer in database.control
9. advance checkpoint epoch for future full-page-image decisions
```

No full database page flush is required in this path.

---

# 241. Dirty-Page Table Checkpoint Entry

## LOCKED

Checkpoint DPT entry:

```text
PageId
rec_lsn
```

The DPT contains every page that was already dirty before checkpoint begin and remains dirty when captured.

Pages dirtied after `CHECKPOINT_BEGIN` can be discovered by scanning WAL after the checkpoint start.

Buffer-frame metadata enumeration must be synchronized enough that a pre-existing dirty page is not silently missed.

---

# 242. Active Writer Transaction Checkpoint Entry

## LOCKED

Checkpoint records transactions that have produced persistent WAL state and are not terminal.

Store at least:

```text
TxnId
last_wal_lsn
```

A transaction with no persistent writes need not appear.

This lets recovery identify a transaction that modified pages before the checkpoint and then crashed without producing any later WAL record.

---

# 243. Checkpoint Data Chunking

## LOCKED

A checkpoint may contain too much DPT/transaction data for one WAL record.

Use:

```text
CHECKPOINT_BEGIN
CHECKPOINT_DATA ...
CHECKPOINT_DATA ...
CHECKPOINT_END
```

`CHECKPOINT_END` identifies the complete checkpoint sequence.

An incomplete checkpoint sequence is ignored.

`database.control` is updated only after the END record is durable.

---

# 244. Checkpoint Redo Start

## LOCKED

Compute:

```text
redo_start_lsn =
    minimum rec_lsn in checkpoint DPT
```

If the DPT is empty, use an appropriate checkpoint LSN such as `CHECKPOINT_BEGIN`.

Store this value in:

```text
database.control.checkpoint_redo_lsn
```

Recovery may start page redo at this bound after reconstructing checkpoint metadata.

---

# 245. WAL Recycling

## LOCKED

Version 1 has no replication or point-in-time archive retention.

A WAL segment may be recycled/deleted only when it lies completely before the oldest LSN still needed by the installed checkpoint/recovery state.

Conservative initial rule:

```text
retain from min(
    latest_checkpoint_begin_lsn,
    latest_checkpoint_redo_lsn
)
```

plus any segment containing the active checkpoint records.

Do not optimize WAL retention until repeated crash/restart testing proves correctness.

---

# 246. Recovery Startup: WAL Tail Validation

## LOCKED

On startup:

1. read latest valid `database.control` slot,
2. locate checkpoint/WAL starting position,
3. scan WAL records forward,
4. verify lengths, alignment, segment boundaries, and CRC32C,
5. stop at the first invalid/torn tail record,
6. truncate/ignore bytes after the last complete valid record.

A torn WAL tail is expected crash behavior, not database corruption, provided all earlier records validate.

---

# 247. Recovery Phase 1: Analysis

## LOCKED

Analysis reconstructs:

```text
dirty-page table
active/loser writer transactions
terminal transaction outcomes
maximum observed TxnId
checkpoint state
latest valid WAL end
```

Start from the latest valid checkpoint metadata when available.

As WAL is scanned:

### PAGE records

Update DPT if page not already present:

```text
DPT[PageId] = record.lsn
```

### BTREE_MTR

Add every affected page to DPT if absent.

### TXN_COMMIT

Mark transaction committed in recovery transaction table.

### TXN_ABORT

Mark transaction aborted.

At analysis end, any writer transaction known active without terminal record is a crash loser.

---

# 248. Recovery Phase 2: Redo

## LOCKED

Start at:

```text
minimum DPT rec_lsn
```

For each redoable WAL record:

### PAGE_DELTA / PAGE_IMAGE / PAGE_INIT

If page does not need redo according to DPT/page LSN, skip.

Otherwise apply.

### BTREE_MTR

Treat the complete valid record as one committed system mini-transaction.

For each affected page:

```text
if page_lsn < mtr.lsn:
    apply its full image or redo patches
    set page_lsn = mtr.lsn
```

A torn/incomplete BTREE_MTR record is not considered a valid WAL record and is never partially replayed.

---

# 249. Redo and Corrupt/Torn Data Pages

## LOCKED

If a page checksum is invalid during recovery:

```text
do not trust its page_lsn
```

Recovery must locate an applicable retained:

```text
PAGE_INIT
PAGE_IMAGE
or full-image portion of BTREE_MTR
```

and reconstruct the page before later deltas are applied.

If no recoverable full image exists despite WAL-retention invariants, report unrecoverable corruption rather than guessing.

---

# 250. Recovery Phase 3: Loser Resolution

## LOCKED

For every crash-loser user transaction:

```text
publish ABORTED
```

No heap/index byte-by-byte undo is performed.

Before opening the database for normal traffic:

1. append recovery `TXN_ABORT` records for unresolved losers,
2. update transaction-status pages to ABORTED,
3. flush required WAL,
4. complete a recovery checkpoint.

This makes future restarts independent of repeatedly rediscovering the same loser set.

---

# 251. No CLRs in v1 User-DML Recovery

## LOCKED

Compensation log records are not required for ordinary v1 user transactions because there is no physical user-DML undo phase.

Do not implement CLRs merely to imitate textbook ARIES terminology.

If a later subsystem introduces physical undo that can itself crash mid-undo, CLRs may be added for that subsystem.

This is an explicit learning decision:

> use recovery machinery because the chosen storage semantics require it, not because a named algorithm contains a step.

---

# 252. Recovery of Transaction Status Pages

## LOCKED

`TXN_COMMIT` and `TXN_ABORT` records are authoritative terminal-outcome evidence.

During redo/recovery, terminal outcomes may update the corresponding transaction-status page even if the status page itself was stale on disk.

Set:

```text
status_page.page_lsn = terminal_record.lsn
```

where appropriate.

Thus transaction-status persistence may lag normal commits without sacrificing correctness.

---

# 253. Recovery of Approximate Metadata

## LOCKED

The following are allowed to be stale and rebuildable:

```text
FSM free-space estimates
some optimizer statistics
non-critical pruning hints
```

After recovery they may be:

- repaired lazily,
- rebuilt by scan,
- refreshed by vacuum/analyze.

Do not put approximate performance metadata on the critical crash-consistency path unless necessary.

---

# 254. Recovery Completion

## LOCKED

Before accepting SQL traffic after a crash:

```text
WAL valid tail established
redo complete
losers marked aborted
transaction status consistent
B+ tree structural MTRs recovered
control metadata valid
recovery checkpoint installed
```

Only then set database state:

```text
ONLINE
```

---

# 255. Crash-Recovery Correctness Principle

## LOCKED

For any transaction:

### COMMIT returned success

After any subsequent crash/restart:

```text
its durable changes must remain logically committed
```

### COMMIT had not become durable

After crash/restart:

```text
transaction may be treated as aborted
```

### ABORTED or crash loser

Its physical garbage may remain, but:

```text
no SQL-visible state may depend on it as committed
```

This distinction between physical residue and logical visibility is central to the architecture.

---

# 256. Vacuum Global Visibility Horizon

## LOCKED

SnapshotManager maintains a registry of active SQL snapshots.

Define:

```text
global_oldest_snapshot_xmin
```

as the minimum `xmin` of all currently registered snapshots.

If there are no active snapshots:

```text
global_oldest_snapshot_xmin = current next_txn_id
```

A long-running snapshot therefore explicitly delays removal of versions that it might still observe.

---

# 257. Why Active Transactions Alone Are Not the Vacuum Horizon

## LOCKED

Vacuum safety is about:

```text
active snapshots
```

not merely:

```text
transactions that exist
```

Examples:

- a READ COMMITTED transaction between statements has no active statement snapshot,
- a REPEATABLE READ transaction holds one snapshot for the whole transaction,
- a long-running query keeps its statement snapshot active.

Snapshot registration is therefore the authoritative visibility horizon.

---

# 258. Tuple Version Garbage Eligibility

## LOCKED

A tuple version is a vacuum garbage candidate when one of the following is true.

### Creator aborted

```text
Status(xmin) == ABORTED
```

The version was never visible.

### Version deleted/superseded long enough ago

Creator is committed/frozen and:

```text
xmax != 0
Status(xmax) == COMMITTED
xmax < global_oldest_snapshot_xmin
```

with any additional active-set/horizon checks required by the snapshot registry implementation.

At that point no currently active snapshot can require the old version.

Do not reclaim a version merely because it is invisible to the vacuum worker's own snapshot.

---

# 259. Vacuum Treatment of In-Progress Metadata

## LOCKED

Vacuum does not reclaim based on:

```text
xmin IN_PROGRESS
xmax IN_PROGRESS
```

It skips/retries those versions.

Vacuum should not wait for ordinary user transactions while holding heap/B+ tree latches.

---

# 260. Two-Phase Physical RID Reclamation

## LOCKED

Because secondary indexes store physical RIDs without a generation counter, a reclaimed `(page,slot)` must not be reused while an in-flight index scan could still hold that old RID.

Therefore physical tuple reclamation has two phases:

```text
NORMAL
    ↓
index cleanup + semantic death
    ↓
DEAD
    ↓
read-epoch grace period
    ↓
UNUSED / reusable
```

This makes the previously reserved heap slot states operational:

```text
NORMAL
DEAD
UNUSED
```

`DEAD` means:

```text
tuple is no longer semantically needed
all required index entries have been removed
slot is not yet safe for immediate RID reuse
```

---

# 261. Read Epoch Manager

## LOCKED

Introduce a lightweight `ReadEpochManager` for physical RID reuse safety.

Every executing SQL statement that may consume stored RIDs registers:

```text
read_epoch
```

for the duration of the statement/executor lifetime.

The manager tracks:

```text
global monotonically increasing epoch
active reader epochs
```

Vacuum records a retirement epoch when it converts a tuple slot from NORMAL to DEAD.

A DEAD RID can become UNUSED only after:

```text
all active reader epochs that could have observed the old index entry have exited
```

This is epoch-based reclamation for physical RID reuse, not MVCC visibility.

---

# 262. Why Snapshot Visibility Alone Is Not Enough for RID Reuse

## LOCKED rationale

An index cursor can read:

```text
(key -> old RID)
```

then, before it fetches the heap page, vacuum may remove the index entry.

If vacuum immediately reused that same RID for an unrelated tuple, the cursor could dereference a physical identity that now means something different.

MVCC often makes the replacement invisible, but correctness must not depend on subtle timing/type/predicate accidents.

The read-epoch grace period guarantees:

> a physical RID does not change identity while an already-running reader may still possess it.

---

# 263. DEAD Slot Persistence Across Crash

## LOCKED

`DEAD` is a persistent slot state.

Once the index cleanup and DEAD transition are WAL-protected, a crash leaves one of two safe outcomes:

- slot remains NORMAL and must be reconsidered by vacuum,
- slot is DEAD and all required index entries were already removed.

After restart there are no pre-crash active readers.

Therefore DEAD slots may be promoted to reusable state lazily without preserving pre-crash read-epoch metadata.

---

# 264. Vacuum Index-Cleanup Protocol

## LOCKED

For a garbage-eligible tuple version:

1. read/copy enough tuple data to derive every indexed user key,
2. re-check garbage eligibility,
3. for every index issue exact:
   ```text
   Erase(encoded_user_key, RID)
   ```
   through B+ tree system MTRs,
4. ensure index changes are logically installed,
5. re-fetch heap page,
6. verify tuple version header still matches expected identity/state,
7. WAL-log transition:
   ```text
   slot NORMAL -> DEAD
   ```
8. record retirement epoch.

Do not mark the heap slot DEAD before its required secondary-index entries are removed.

This ordering prevents a reused RID from being reachable through an old index entry.

---

# 265. Vacuum Version-Chain Splicing

## LOCKED

Before a dead tuple slot becomes reusable, surviving tuple versions must not retain a `prev` pointer to that soon-to-be-reused RID.

During a table vacuum pass:

1. scan version headers,
2. build enough reverse-link information to find surviving direct successors,
3. for each removable version `V` with:
   ```text
   V.prev = P
   ```
   rewrite each surviving direct successor `S` from:
   ```text
   S.prev = V
   ```
   to:
   ```text
   S.prev = P
   ```
4. WAL-log the successor header update,
5. only then complete DEAD/reuse processing for `V`.

If the safe relationship cannot be established under revalidation, defer reclaiming that version to a later vacuum cycle.

Version chains are not required for ordinary snapshot visibility, so vacuum may shorten them aggressively as long as no surviving pointer references reused storage.

---

# 266. Vacuum and Concurrent Updates

## LOCKED

A tuple version eligible because:

```text
committed xmax < global horizon
```

cannot be a valid target for a new transaction snapshot.

Nevertheless vacuum must re-check the tuple header under page latch before changing slot state.

If:

- `xmin`,
- `xmax`,
- slot state,
- expected version-chain fields

no longer match the candidate record, skip/retry.

Do not hold page latches while performing B+ tree operations.

---

# 267. DEAD to UNUSED Transition

## LOCKED

After the read-epoch grace period:

1. fetch heap page,
2. write-latch it,
3. verify slot is still DEAD,
4. mark slot UNUSED/reusable,
5. optionally compact tuple bytes,
6. update free-space metadata,
7. WAL-log the persistent page change.

A slot ID may then be reused by a future inserted tuple version.

The read-epoch rule guarantees no pre-existing cursor can legally interpret the old RID after this point.

---

# 268. Vacuum Handling of Aborted `xmax`

## LOCKED

For a live tuple whose:

```text
Status(xmax) == ABORTED
```

vacuum may normalize:

```text
xmax = 0
cmax = 0
```

This removes repeated transaction-status lookups and allows a later updater to operate on cleaner metadata.

No index change is required merely for clearing an aborted `xmax`.

---

# 269. Freezing Ancient Live Tuples

## LOCKED

For a live tuple version whose creator:

```text
is COMMITTED
and
xmin is older than every active snapshot that could distinguish the original creator
```

vacuum may rewrite:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

This means future visibility checks no longer need the original transaction-status entry.

Freezing is WAL logged.

---

# 270. Transaction Status Truncation

## LOCKED architecture, later operational milestone

Because transaction IDs are 64-bit, wraparound pressure is not the motivation.

The purpose is to prevent:

```text
txn_status.dat
```

from growing forever.

After sufficiently complete vacuum/freezing establishes:

```text
no persistent tuple/catalog object references transaction IDs below X
```

whole status pages strictly below `X` may be retired/truncated according to file-management capabilities.

Do not implement aggressive status truncation before basic vacuum/freezing correctness.

---

# 271. Vacuum and B+ Tree Garbage

## LOCKED

Vacuum cleans:

```text
index entries pointing to:
    aborted-created tuple versions
    globally dead committed tuple versions
```

Because leaf physical key is `(user_key,RID)`, cleanup is exact.

Removing garbage may trigger:

- leaf underflow,
- redistribution,
- merge,
- root contraction.

Those remain B+ tree MTRs and do not become user transactions.

---

# 272. Vacuum and FSM

## LOCKED

After reclaiming/repacking heap storage:

```text
update the table FSM estimate
```

FSM remains advisory/rebuildable.

Vacuum may batch FSM updates instead of logging every estimate change on the critical path.

---

# 273. Vacuum and Statistics

## LOCKED

Vacuum may collect useful maintenance counters:

```text
live tuple versions
dead tuple versions
aborted versions
pages scanned
pages compacted
index entries removed
bytes reclaimed
frozen tuples
```

Optimizer statistics remain a separate ANALYZE concern, although vacuum may trigger/assist later.

---

# 274. Manual Vacuum First

## LOCKED implementation order

Initial implementation:

```text
VACUUM table
```

or internal explicit maintenance invocation.

Do not build a sophisticated autovacuum scheduler before manual vacuum is correct and observable.

Later add background/autovacuum based on measurable thresholds:

- dead version ratio,
- aborted version count,
- status/freezing pressure,
- table growth.

---

# 275. Transaction + Index + Heap INSERT Protocol

## LOCKED high-level sequence

For an INSERT:

```text
1. determine CommandId
2. encode tuple with:
       xmin = current txn
       xmax = 0
       cmin = current command
3. acquire required UNIQUE_KEY locks
4. perform uniqueness checks
5. choose heap page via FSM
6. WAL-log heap page image/delta
7. install new heap tuple version
8. perform B+ tree MTRs for index entries
9. release short-lived page/index latches
10. keep logical unique locks until txn end
```

If transaction aborts:

```text
tuple xmin becomes logically aborted
index entries may remain
vacuum cleans later
```

---

# 276. Transaction + Index + Heap UPDATE Protocol

## LOCKED high-level sequence

For each target row:

```text
1. obtain candidate visible RID from scan
2. release page/index latches
3. acquire TUPLE_WRITE lock on old RID
4. re-fetch and revalidate old version
5. apply isolation-specific conflict rules
6. acquire affected UNIQUE_KEY locks in deterministic key order
7. validate uniqueness for new key(s)
8. create new tuple version:
       xmin = current txn
       cmin = current command
       prev = old RID
9. WAL-log/install new version
10. WAL-log old tuple header:
       xmax = current txn
       cmax = current command
11. install new B+ tree entries via MTRs
12. keep old index entries
13. hold logical locks until txn end
```

If transaction aborts:

```text
old xmax is treated as aborted => old version remains live
new xmin is aborted => new version invisible
new index entries are garbage
```

No physical rollback is required.

---

# 277. Transaction + Heap DELETE Protocol

## LOCKED high-level sequence

For each target row:

```text
1. obtain visible RID
2. release page/index latches
3. acquire TUPLE_WRITE lock
4. re-fetch/revalidate
5. apply write conflict rules
6. acquire unique-key locks for affected unique indexes
7. WAL-log/install:
       xmax = current txn
       cmax = current command
8. retain existing index entries
9. hold logical locks until txn end
```

Commit makes the version logically dead to future snapshots.

Abort makes the `xmax` ineffective.

Vacuum later removes the index entries and tuple version when globally safe.

---

# 278. Transaction COMMIT Protocol

## LOCKED detailed sequence

For a transaction with persistent writes:

```text
1. state ACTIVE -> COMMITTING
2. append TXN_COMMIT(txn_id, prev_txn_lsn)
3. submit commit_lsn to CommitCoordinator
4. wait until durable_lsn >= commit_lsn
5. update in-memory/status page to COMMITTED
6. set status-page page_lsn = commit_lsn
7. release TUPLE_WRITE and UNIQUE_KEY locks
8. unregister active snapshot(s)
9. remove from active transaction registry
10. state -> COMMITTED
11. return success
```

Do not flush heap or B+ tree pages in this protocol.

That would violate NO-FORCE.

---

# 279. Transaction ABORT Protocol

## LOCKED detailed sequence

```text
1. state ACTIVE/COMMITTING-as-allowed -> ABORTING
2. if persistent WAL-visible state exists:
       append TXN_ABORT
3. publish ABORTED status
4. set status-page page_lsn to abort record when one exists
5. release logical locks
6. unregister snapshots
7. remove from active registry
8. state -> ABORTED
9. return/raise abort
```

No scan through the write set is required to restore old tuple bytes.

Optional eager cleanup of aborted versions is deferred; vacuum is the baseline reclamation mechanism.

---

# 280. Statement Retry Boundary

## LOCKED

Internal READ COMMITTED write retries restart at a well-defined statement execution boundary.

Do not partially emit externally visible rows or side effects before a statement that may restart has crossed its retry-safe point.

For initial SQL support:

- DML without external side effects is retryable,
- `RETURNING` output should be buffered until the statement is known not to restart.

Later features with external effects must explicitly define retry semantics.

---

# 281. Error Categories

## LOCKED

Transaction subsystem should distinguish at least:

```text
SerializationFailure
DeadlockVictim
UniqueViolation
TransactionAborted
LockCancelled
WalIOError
RecoveryError
CorruptionError
```

Do not convert all transaction failures into a generic internal error.

The SQL layer may map them to SQLSTATE-like codes later.

---

# 282. Transaction Observability

## LOCKED

Expose internal counters:

```text
transactions begun
committed
aborted
deadlock victims
serialization failures
statement retries

active snapshot count
oldest snapshot xmin
snapshot active-set sizes

tuple-lock waits
unique-lock waits
deadlock cycles

WAL bytes appended
WAL bytes synced
WAL flush count
group-commit batches
average/max group size
commit wait latency

checkpoint count
checkpoint duration
DPT size
redo_start distance

recovery records scanned
pages redone
full-page images restored
loser transactions

vacuum tuples examined
versions reclaimed
index entries removed
RID retire queue size
frozen versions
```

These counters are essential for performance work.

---

# 283. Transaction Debug Introspection

## LOCKED

Provide debug-only/internal commands or test hooks to inspect:

```text
active transactions
active snapshots
transaction status by TxnId
lock table
wait-for graph
durable_lsn
current WAL end
dirty-page table
latest checkpoint
vacuum horizon
RID retirement epochs
```

This will dramatically reduce the difficulty of debugging concurrency/recovery tests.

---

# 284. Crash Injection Framework

## LOCKED

Add deterministic crash/fault injection points around:

```text
after WAL append
before WAL append completion
before/after fdatasync
before/after data-page pwrite
after heap insert before index MTR
during B+ MTR construction
after B+ MTR append before page release
before/after TXN_COMMIT flush
during checkpoint
during vacuum index cleanup
before DEAD transition
before DEAD -> UNUSED transition
```

Tests should be able to terminate the process at a selected injection point, reopen, recover, and verify logical database contents.

---

# 285. Recovery Property Tests

## LOCKED

For random transaction workloads:

1. execute operations,
2. crash at random instrumented points,
3. reopen/recover,
4. compare logical committed contents against a model that includes only transactions whose commit records became durable.

Repeat across many seeds.

Physical garbage is allowed.

Logical committed results must match.

---

# 286. MVCC Visibility Tests

## LOCKED

Build table-driven tests for:

- committed creator before snapshot,
- creator active at snapshot then commits,
- creator starts after snapshot,
- creator aborts,
- committed deleter before snapshot,
- deleter active at snapshot then commits,
- deleter starts after snapshot,
- deleter aborts,
- own insert previous command,
- own insert current command,
- own delete previous command,
- own delete current command,
- frozen creator.

Test exact `xmin/xmax/cmin/cmax` combinations, not only end-to-end SQL.

---

# 287. Isolation Tests

## LOCKED

READ COMMITTED:

- second statement sees newly committed rows,
- one statement does not change snapshot mid-scan,
- writer waiting on updated row restarts/re-evaluates correctly.

REPEATABLE READ / snapshot isolation:

- repeated reads use same snapshot,
- committed concurrent insert remains invisible,
- concurrent update of target causes serialization failure,
- write skew is demonstrably possible and documented as non-serializable.

---

# 288. Locking Tests

## LOCKED

Test:

```text
tuple writer blocks tuple writer
different tuple writers proceed
same unique key serializes
different unique keys proceed
logical lock waits hold no page latches
deadlock cycle detection
youngest victim policy
lock release on commit
lock release on abort
cancelled waiter cleanup
```

Use deterministic barriers rather than timing sleeps where possible.

---

# 289. Group Commit Benchmarks

## LOCKED

Benchmark:

```text
1 committing thread
2
4
8
16
32+
```

Measure:

```text
transactions/sec
p50/p95/p99 commit latency
fdatasync calls/sec
average commits per fsync
WAL bytes/sec
```

Compare against a diagnostic mode that forces one fsync per commit to quantify group-commit benefit.

---

# 290. Checkpoint/Recovery Benchmarks

## LOCKED

Measure:

```text
checkpoint duration
checkpoint WAL bytes
full-page-image bytes
dirty-page-table size
WAL retained bytes
restart analysis time
redo time
pages redone
recovery total time
```

Test both:

- mostly clean buffer pool,
- heavily dirty buffer pool.

---

# 291. Vacuum Benchmarks

## LOCKED

Measure:

```text
dead-version scan rate
index cleanup rate
heap bytes reclaimed
B+ tree merge/split side effects
RID grace-period delay
FSM improvement
status-freezing rate
foreground latency impact
```

Use workloads with:

- update-heavy hot rows,
- delete-heavy tables,
- aborted transactions,
- duplicate secondary keys,
- long-running snapshots.

---

# 292. Deliberately Deferred Transaction Features

## LOCKED

Do not implement before the baseline architecture is correct and crash-tested:

- savepoints,
- subtransactions,
- two-phase commit,
- XA/distributed transactions,
- SERIALIZABLE / SSI,
- predicate locks,
- lock escalation,
- asynchronous commit,
- logical replication,
- physical replication,
- PITR archive management,
- online backup,
- read-only follower snapshots,
- speculative insertion optimizations,
- prepared transactions,
- row-level lock modes beyond exclusive writer,
- commit sequence numbers,
- timestamp oracle,
- lock-free transaction registry.

These remain future high-value projects.

---

# 293. High-Reward Future Transaction Experiments

## LOCKED recommendation

After the baseline is correct and benchmarked, strong learning/performance experiments include:

```text
Serializable Snapshot Isolation
predicate/range locking
speculative unique insertion
commit sequence numbers
faster snapshot representation
sharded lock manager
lock-free/RCU active transaction registry
asynchronous commit
parallel/background vacuum
adaptive WAL buffering
direct I/O / io_uring WAL writer
incremental checkpoint tuning
```

Choose based on measured bottlenecks and desired learning focus.

---

# 294. Transaction/Durability Invariants

## LOCKED

Codex must preserve all of the following:

1. A transaction is not visible as committed before its commit WAL record is durable.
2. COMMIT does not force dirty heap/index pages.
3. Dirty page writeback never outruns its page LSN in durable WAL.
4. Ordinary user abort does not require physical heap/index undo.
5. Aborted `xmin` makes a version invisible.
6. Aborted `xmax` does not delete the old version.
7. An index entry never decides MVCC visibility by itself.
8. Logical locks are never waited on while holding physical page/B+ latches.
9. Tuple-write locks are held to transaction end.
10. Unique-key locks are held to transaction end.
11. READ COMMITTED uses one stable snapshot per statement.
12. REPEATABLE READ uses one stable transaction snapshot.
13. Repeatable-read write conflicts abort rather than silently following a newer invisible row.
14. WAL records are checksum validated before recovery use.
15. An incomplete WAL tail is ignored/truncated at the last valid record.
16. B+ MTR pages cannot flush before the complete MTR WAL record exists.
17. All pages changed by one B+ MTR receive the same MTR LSN.
18. B+ structural changes may survive owner user-transaction abort.
19. Crash losers become ABORTED before the database returns ONLINE.
20. Torn data pages are recoverable from retained full-page images plus later redo.
21. Checkpoints are fuzzy and do not require flushing every dirty page.
22. The installed checkpoint pointer is updated only after checkpoint WAL is durable.
23. Garbage eligibility depends on global active snapshots, not vacuum's local visibility.
24. Index entries are removed before a heap RID is retired.
25. A retired RID is not reused until a read-epoch grace period passes.
26. A surviving tuple version never retains a `prev` pointer to reused physical storage.
27. DEAD heap slots are persistent and distinguish cleaned-but-not-yet-reusable RIDs.
28. `FROZEN_TXN_ID` is always treated as committed.
29. Normal TxnIds are never reused after durable reservation.
30. Transaction status `UNKNOWN` for a referenced normal TxnId after recovery is an invariant failure.
31. Approximate metadata may be rebuilt; correctness metadata may not be guessed.
32. Crash recovery is validated against committed logical outcomes, not byte-for-byte absence of aborted garbage.

---

# 295. Modules Added by This Architecture

## LOCKED recommended project structure extension

```text
src/
  txn/
    transaction.h
    transaction_manager.h
    transaction_manager.cpp
    transaction_status_store.h
    transaction_status_store.cpp
    snapshot.h
    snapshot_manager.h
    snapshot_manager.cpp
    visibility.h
    visibility.cpp
    lock_manager.h
    lock_manager.cpp
    deadlock_detector.h
    deadlock_detector.cpp
    read_epoch_manager.h
    read_epoch_manager.cpp

  wal/
    wal_record.h
    wal_codec.h
    wal_codec.cpp
    wal_manager.h
    wal_manager.cpp
    wal_segment.h
    commit_coordinator.h
    commit_coordinator.cpp
    checkpoint_manager.h
    checkpoint_manager.cpp
    recovery_manager.h
    recovery_manager.cpp

  maintenance/
    vacuum.h
    vacuum.cpp

  storage/
    control_file.h
    control_file.cpp
```

Exact filenames may evolve.

Responsibility boundaries may not silently collapse into one global "transaction engine" class.

---

# 296. Implementation Order for the Transaction/Durability Core

## LOCKED

Implement in this order because each stage produces testable invariants.

### Phase T1 — Transaction identity and visibility

1. database control file,
2. durable TxnId block reservation,
3. Transaction object/state,
4. active transaction registry,
5. Snapshot capture,
6. transaction-status store,
7. `xmin/xmax/cmin/cmax` visibility tests.

### Phase T2 — Logical conflict control

8. tuple-write LockManager,
9. unique-key LockManager,
10. deadlock detector,
11. UPDATE/DELETE revalidation,
12. READ COMMITTED retry,
13. REPEATABLE READ conflict abort.

### Phase T3 — WAL foundations

14. segmented WAL files,
15. WAL codec/checksums,
16. WAL append buffer,
17. PAGE_INIT/PAGE_DELTA/PAGE_IMAGE,
18. page LSN integration,
19. buffer recLSN,
20. WAL-before-data tests.

### Phase T4 — Commit durability

21. TXN_COMMIT/TXN_ABORT,
22. WAL flusher,
23. CommitCoordinator,
24. group commit,
25. terminal status publication,
26. durability/crash tests.

### Phase T5 — B+ tree durability

27. no-flush MTR page guards,
28. BTREE_MTR encoding,
29. B+ page LSN integration,
30. structural crash tests,
31. aborted transaction + persistent index-garbage tests.

### Phase T6 — Checkpoint and recovery

32. checkpoint DPT,
33. active writer checkpoint table,
34. control-file checkpoint installation,
35. WAL tail validation,
36. analysis,
37. redo,
38. torn-page image recovery,
39. loser abort resolution,
40. recovery checkpoint.

### Phase T7 — Vacuum

41. snapshot global horizon,
42. garbage candidate selection,
43. exact index cleanup,
44. DEAD slot state,
45. ReadEpochManager,
46. DEAD -> UNUSED reclamation,
47. version-chain splicing,
48. aborted-xmax cleanup,
49. freezing,
50. vacuum benchmarks.

Do not begin SERIALIZABLE or replication work before all seven phases are reliable.

---

# 297. Transaction/Durability Milestone 1

## LOCKED target

Without B+ tree integration yet:

```text
two concurrent transactions
heap-version MVCC
READ COMMITTED
REPEATABLE READ
tuple write conflicts
WAL durable commits
crash/restart recovery
```

must pass deterministic tests.

Aborted tuple garbage may remain.

---

# 298. Transaction/Durability Milestone 2

## LOCKED target

Add:

```text
unique-key locks
B+ tree MTR WAL
index scans through heap visibility
aborted index-entry garbage
crash during splits/merges
group commit
```

The tree must remain structurally valid after every injected crash.

---

# 299. Transaction/Durability Milestone 3

## LOCKED target

Add:

```text
fuzzy checkpoints
WAL recycling
full-page-image torn-write recovery
manual vacuum
read-epoch RID reclamation
version-chain splicing
freezing
```

At this point the engine has a coherent persistent transactional storage core rather than a collection of isolated features.

---

# 300. Architecture Status After This Section

## LOCKED v1

The project now has locked specifications for:

```text
physical page storage
heap tuples
buffer pool
free-space management
B+ tree indexing
MVCC transactions
snapshot visibility
write conflict control
unique-key locking
deadlock detection
transaction status persistence
segmented WAL
group commit
B+ tree structural mini-transactions
fuzzy checkpoints
crash recovery
torn-page recovery
vacuum
safe physical RID reuse
transaction freezing
```

The next architecture stage should move upward into:

```text
catalog + schema/type system
SQL lexer/parser
binder/name resolution
logical relational algebra
expression representation
```

After that:

```text
vectorized executor
statistics
cost model
join ordering
optimizer
```

Those higher layers should consume the transactional storage contract defined here rather than redefining its semantics.

---

# 301. Catalog, SQL Front End, Binder, Expression, and Logical Plan Contract

## LOCKED

This section defines the upper semantic layer of the database.

The components are:

```text
Catalog
Schema / Type System
Lexer
Parser
AST
Binder
Bound Expressions
Logical Plan
DDL / DML Planning
```

The main design goal is to create a clean bridge:

```text
SQL text
    ↓
syntax
    ↓
resolved semantics
    ↓
typed expressions
    ↓
relational algebra
```

The parser must not know physical storage.

The binder must not choose physical algorithms.

The logical plan must not contain buffer-page or B+ tree implementation details.

---

# 302. Upper-Layer Dependency Direction

## LOCKED

Dependency direction:

```text
SQL text
  ↓
Lexer
  ↓
Parser
  ↓
AST
  ↓
Binder
  ├── Catalog
  └── Type System
  ↓
Bound Statements / Bound Expressions
  ↓
Logical Planner
  ↓
Logical Relational Algebra
  ↓
Logical Optimizer
  ↓
Physical Planner
```

Do not allow:

```text
Parser -> BufferPool
Binder -> BTreePage
LogicalFilter -> DiskManager
```

or any equivalent leakage.

---

# 303. Catalog Philosophy

## LOCKED

The catalog is the database's authoritative semantic metadata layer.

It owns knowledge about:

- tables,
- columns,
- types,
- indexes,
- uniqueness,
- primary keys,
- nullable constraints,
- schemas/namespaces,
- object IDs,
- schema versions,
- statistics metadata handles.

It does not own:

- heap page bytes,
- B+ tree page internals,
- transaction snapshots,
- SQL parsing.

The catalog maps:

```text
human-readable names
    ↓
stable internal identifiers
```

---

# 304. Database Namespace Model

## LOCKED for v1

Version 1 supports one logical database with one SQL namespace:

```text
main
```

Unqualified table names resolve inside `main`.

Examples:

```sql
SELECT * FROM users;
SELECT * FROM main.users;
```

Both are accepted.

Multiple schemas/namespaces are architecture-compatible but deferred.

Reason:

the interesting implementation work is name resolution and catalog identity, not namespace administration.

---

# 305. Catalog Object Identifiers

## LOCKED

Use stable numeric IDs:

```cpp
using TableId  = uint64_t;
using ColumnId = uint32_t;
using IndexId  = uint64_t;
using TypeId   = uint32_t;
```

A table's `ColumnId` is stable for the lifetime of that table definition.

Do not identify catalog objects internally by strings after binding.

Bound references should carry IDs.

---

# 306. Catalog System Tables

## LOCKED

Persistent metadata should eventually live in relational system tables.

Initial system tables:

```text
sys_tables
sys_columns
sys_indexes
sys_index_columns
sys_constraints
sys_statistics
```

Recommended logical schemas:

### `sys_tables`

```text
table_id
namespace
table_name
heap_file_id
fsm_file_id
schema_version
flags
```

### `sys_columns`

```text
table_id
column_id
column_name
logical_position
type_id
nullable
default_expr_blob/reference
schema_version_added
schema_version_removed
flags
```

### `sys_indexes`

```text
index_id
table_id
index_name
btree_file_id
unique
primary
key_schema_version
flags
```

### `sys_index_columns`

```text
index_id
ordinal
column_id
```

### `sys_constraints`

```text
constraint_id
table_id
constraint_type
constraint_name
definition payload
```

### `sys_statistics`

```text
table_id
column_id / object scope
stats_version
row_count
null_fraction
ndv estimate
min/max
histogram payload
```

Exact physical system-table layout may evolve.

The semantic fields are locked.

---

# 307. Catalog Bootstrap

## LOCKED

A small bootstrap catalog is allowed so the engine can open the system tables before the catalog is fully self-hosting.

Bootstrap metadata may contain only the minimum required to locate and interpret:

```text
sys_tables
sys_columns
sys_indexes
...
```

After bootstrap:

```text
ordinary metadata lookup uses the catalog system tables
```

Do not keep two indefinitely divergent metadata systems.

---

# 308. Catalog Cache

## LOCKED

Maintain an in-memory catalog cache for frequently used immutable metadata descriptors:

```text
TableDescriptor
IndexDescriptor
SchemaDescriptor
```

Cache keys use stable object IDs and names where needed.

Catalog cache entries should be immutable snapshots.

Schema-changing DDL invalidates or replaces affected descriptors rather than mutating them in place while queries may hold references.

---

# 309. Catalog Descriptor Lifetime

## LOCKED

Bound/planned queries may hold shared immutable catalog descriptors.

Do not return pointers into mutable hash-map storage whose addresses become invalid after rehashing.

Use stable ownership such as:

```text
shared immutable descriptor objects
generation/versioned handles
```

The exact C++ ownership type is implementation-specific.

The semantic lifetime guarantee is not.

---

# 310. Schema Versioning

## LOCKED

Each table has:

```text
schema_version
```

which monotonically increases when a schema-changing DDL operation commits.

Every tuple already stores:

```text
schema_version
```

from the lower-layer architecture.

Version 1 DDL support may initially allow only operations that do not require tuple rewrite, but the metadata model must preserve historical schema interpretation.

---

# 311. Column Identity vs Position

## LOCKED

Distinguish:

```text
ColumnId
```

from:

```text
logical display position
```

Never assume:

```text
ColumnId == zero-based SELECT * position
```

This allows future:

- dropped columns,
- added columns,
- schema evolution,
- reordered presentation.

Execution/binding should refer to columns by stable IDs and assigned logical-plan slots.

---

# 312. SQL Type System

## LOCKED initial scalar types

Initial logical SQL types:

```text
BOOLEAN
INT32
INT64
FLOAT64
DATE
TIMESTAMP
VARCHAR
NULL
```

Additionally define:

```text
UNKNOWN
```

as a binder-only type for unresolved NULL literals / parameters where applicable.

`UNKNOWN` must never appear in stored table schemas.

---

# 313. LogicalType Representation

## LOCKED

Use a compact value-like representation.

Conceptually:

```cpp
enum class TypeKind {
    Boolean,
    Int32,
    Int64,
    Float64,
    Date,
    Timestamp,
    Varchar,
    Null,
    Unknown
};

struct LogicalType {
    TypeKind kind;
};
```

Future extensibility may add type parameters.

Do not use an inheritance hierarchy with one heap allocation per type descriptor.

---

# 314. NULL Is Not an Ordinary Runtime Type

## LOCKED

SQL NULL is primarily a value-state.

Stored/execution columns have a logical type plus nullability.

Examples:

```text
INT64 nullable
VARCHAR not-null
```

A NULL literal may temporarily carry:

```text
LogicalType::Unknown
```

during binding.

It must be coerced to a concrete context type where possible.

---

# 315. Type Promotion

## LOCKED

Numeric promotion hierarchy:

```text
INT32
  ↓
INT64
  ↓
FLOAT64
```

Binary arithmetic/comparison on mixed numeric operands finds the smallest common promoted type.

Examples:

```text
INT32 + INT64  -> INT64
INT64 + FLOAT64 -> FLOAT64
```

Implicit narrowing is not allowed.

---

# 316. String Coercion Policy

## LOCKED

Do not silently coerce arbitrary numeric/date values to VARCHAR in general expression comparison.

Examples:

```sql
1 = '1'
```

should not become true through hidden string conversion.

Prefer explicit casts.

This keeps type semantics predictable and prevents binder behavior from becoming dialect-specific magic.

---

# 317. Boolean Context

## LOCKED

Predicates require BOOLEAN.

Do not implement C-like truthiness such as:

```sql
WHERE 5
```

Version 1 should reject non-boolean predicates unless an explicit cast/operator is defined.

---

# 318. SQL Three-Valued Logic

## LOCKED

Boolean expression semantics use:

```text
TRUE
FALSE
UNKNOWN
```

where UNKNOWN is represented by NULL BOOLEAN.

Important truth cases:

```text
TRUE AND NULL  -> NULL
FALSE AND NULL -> FALSE

TRUE OR NULL   -> TRUE
FALSE OR NULL  -> NULL

NOT NULL       -> NULL
```

The execution engine and constant folder must use the same truth tables.

---

# 319. Comparison and NULL

## LOCKED

Ordinary comparisons:

```text
=
<>
<
<=
>
>=
```

with a NULL operand return:

```text
NULL
```

not TRUE/FALSE.

NULL testing uses:

```sql
IS NULL
IS NOT NULL
```

Do not rewrite:

```sql
x = NULL
```

into `x IS NULL`.

It must evaluate according to SQL NULL semantics.

---

# 320. Initial Cast Model

## LOCKED

Support explicit `CAST(expr AS type)`.

Initial safe implicit casts:

```text
INT32 -> INT64
INT32 -> FLOAT64
INT64 -> FLOAT64
UNKNOWN NULL -> contextual target type
```

Other conversions require explicit cast.

Initial explicit casts may include:

```text
INT32 <-> INT64 where range valid
numeric -> FLOAT64
numeric -> VARCHAR
BOOLEAN -> VARCHAR
DATE/TIMESTAMP -> VARCHAR
VARCHAR -> numeric/date/timestamp where parser succeeds
```

Failed runtime casts produce SQL errors.

---

# 321. Type Resolution API

## LOCKED

Centralize type logic.

Conceptual component:

```text
TypeResolver
```

responsible for:

```text
common numeric type
implicit cast legality
explicit cast legality
comparison compatibility
arithmetic result type
function/operator signature matching
```

Do not duplicate coercion rules across Binder, ConstantFolder, and Executor.

---

# 322. SQL Value Representation

## LOCKED

A generic scalar `Value` type is acceptable in non-hot semantic layers:

```text
parser literals
constant folding
catalog defaults
tests
debug printing
```

It may conceptually contain:

```text
LogicalType
is_null
small scalar union
owned VARCHAR bytes/string
```

Do not use one generic `Value` object per execution cell in hot vectorized loops.

That lower-layer decision remains locked.

---

# 323. Lexer

## LOCKED: handwritten lexer

Implement our own lexer.

It produces tokens with:

```text
TokenKind
source byte span
line/column or source offset
optional literal payload
```

The parser should not repeatedly rescan raw source text to identify token boundaries.

---

# 324. Lexer Token Classes

## LOCKED

Initial token classes include:

```text
identifier
quoted identifier
integer literal
floating literal
string literal
keyword
operator
punctuation
end-of-input
```

Keywords may be recognized either:

- directly by lexer,
- or via identifier-to-keyword lookup.

Do not make SQL keywords case-sensitive.

---

# 325. Identifier Case Rules

## LOCKED

Unquoted identifiers are normalized to:

```text
lowercase
```

Quoted identifiers preserve exact spelling/case.

Examples:

```sql
Users
USERS
users
```

all resolve to:

```text
users
```

while:

```sql
"Users"
```

is distinct.

This gives predictable SQL-like semantics.

---

# 326. SQL String Literals

## LOCKED

Single quotes represent strings:

```sql
'Alice'
```

Escape a single quote using doubled quote:

```sql
'It''s fine'
```

Do not initially implement every vendor-specific backslash escape mode.

String token payload should contain decoded logical bytes, while source spans remain available for diagnostics.

---

# 327. Numeric Literal Lexing

## LOCKED

Recognize at least:

```text
123
-123        parsed as unary minus + positive literal where practical
1.25
1e10
1.2e-3
```

Prefer treating leading `-` as an operator in the parser rather than embedding sign into every numeric token.

This keeps:

```text
-2147483648
```

and unary-expression rules explicit.

---

# 328. Comments

## LOCKED

Support:

```sql
-- line comment
```

and:

```sql
/* block comment */
```

Nested block comments are deferred.

Unterminated comments must produce source-positioned errors.

---

# 329. Source Locations

## LOCKED

Every AST node stores a source span:

```text
start byte offset
end byte offset
```

Line/column may be computed or cached.

Binder/type errors should point back to the smallest useful source span.

Do not discard source-position information after parsing.

---

# 330. Parser Architecture

## LOCKED: handwritten recursive descent + Pratt expressions

Use:

```text
recursive descent
```

for statements/clauses and:

```text
Pratt parser / precedence climbing
```

for expressions.

This is preferred over a parser generator for the learning goals.

It teaches:

- grammar structure,
- precedence,
- associativity,
- syntax diagnostics,
- AST construction

without requiring implementation of the entire SQL standard grammar.

---

# 331. Initial SQL Statement Set

## LOCKED

Version 1 parser/binder target:

```sql
CREATE TABLE
CREATE INDEX
CREATE UNIQUE INDEX
DROP TABLE
DROP INDEX

INSERT
UPDATE
DELETE

SELECT

BEGIN
COMMIT
ROLLBACK

VACUUM
EXPLAIN
EXPLAIN ANALYZE
```

`ALTER TABLE` is deferred initially.

The catalog/schema-version architecture must remain compatible with adding it later.

---

# 332. Initial SELECT Grammar Surface

## LOCKED

Support meaningful relational queries:

```sql
SELECT [DISTINCT] select_list
FROM table_reference [joins...]
[WHERE predicate]
[GROUP BY expressions]
[HAVING predicate]
[ORDER BY expressions [ASC|DESC] ...]
[LIMIT integer]
[OFFSET integer]
```

Initial FROM supports:

```text
base tables
aliases
INNER JOIN
LEFT JOIN
CROSS JOIN
```

Deferred initially:

```text
RIGHT JOIN
FULL OUTER JOIN
LATERAL
recursive CTEs
window functions
set operations
```

---

# 333. Subqueries

## LOCKED scope

Support architecture for subqueries from the beginning.

Initial implementation target:

```text
scalar uncorrelated subquery
EXISTS uncorrelated subquery
IN uncorrelated subquery
derived table in FROM
```

Correlated subqueries are deferred until the logical optimizer can decorrelate or execute them deliberately.

Do not bake parser/binder structures that make correlation impossible later.

---

# 334. AST Philosophy

## LOCKED

AST represents syntax, not resolved semantics.

Examples of AST nodes:

```text
SelectStatement
CreateTableStatement
InsertStatement
UpdateStatement
DeleteStatement

AstIdentifier
AstQualifiedName
AstLiteral
AstBinaryExpression
AstUnaryExpression
AstFunctionCall
AstCast
AstStar
AstSubqueryExpression
```

AST identifiers remain textual names.

They are not replaced by TableId/ColumnId until binding.

---

# 335. AST Ownership

## LOCKED

Use arena ownership for AST nodes per parsed statement/query batch.

Benefits:

- cheap allocation,
- trivial bulk destruction,
- stable node addresses,
- simple parser code.

Do not allocate/deallocate AST nodes individually with general-purpose `new/delete` in hot parser loops.

---

# 336. Expression Precedence

## LOCKED

Initial precedence from low to high:

```text
OR
AND
NOT
comparison / IS NULL / IN
+ -
* / %
unary + -
primary
```

Parentheses override precedence.

Comparison chaining such as:

```sql
a < b < c
```

should be rejected unless explicitly implemented with SQL semantics.

---

# 337. Binder Responsibilities

## LOCKED

Binder converts AST into semantically resolved structures.

Responsibilities:

```text
table lookup
column lookup
alias scopes
ambiguity detection
wildcard expansion
type resolution
implicit cast insertion
aggregate validation
GROUP BY validation
ORDER BY resolution
function/operator resolution
subquery scope creation
DML target resolution
constraint/type validation
```

Binder does **not**:

```text
choose SeqScan vs IndexScan
choose HashJoin vs NestedLoop
estimate cardinality
access heap pages
```

---

# 338. Binding Context / Scope

## LOCKED

Use explicit nested binding scopes.

A scope contains:

```text
visible table bindings
table aliases
output aliases where SQL semantics allow
parent scope pointer for future correlation
```

Each visible relation binding has a unique:

```text
BindingId
```

within the bound query.

This is distinct from `TableId`.

Example self-join:

```sql
FROM users u1
JOIN users u2 ON ...
```

has:

```text
same TableId
different BindingId
```

---

# 339. Bound Column Reference

## LOCKED

A bound column expression contains at least:

```text
BindingId
TableId
ColumnId
LogicalType
nullable
source span
```

Later logical planning assigns:

```text
output slot / column index
```

Do not keep only the textual column name after binding.

---

# 340. Unqualified Column Resolution

## LOCKED

For:

```sql
SELECT id
FROM users u
JOIN orders o ...
```

Binder searches visible relations.

Cases:

```text
0 matches -> unknown column error
1 match   -> bind
>1 match  -> ambiguous column error
```

Never silently choose the leftmost matching relation.

---

# 341. Qualified Column Resolution

## LOCKED

For:

```sql
u.id
```

resolve:

```text
u -> relation binding
id -> column in that binding
```

Unknown qualifier and unknown column errors are distinct.

---

# 342. SELECT * Expansion

## LOCKED

Binder expands:

```sql
SELECT *
```

into bound column references in visible FROM-relation order.

Support:

```sql
SELECT u.*
```

for one relation binding.

Wildcard expansion happens during binding, not execution.

This means logical plans contain explicit output expressions.

---

# 343. Output Names

## LOCKED

Each SELECT output has:

```text
expression
display name
logical type
nullable
```

Naming priority:

1. explicit `AS alias`,
2. simple column name for direct column reference,
3. generated expression name for other expressions.

Generated names are presentation metadata, not stable catalog identities.

---

# 344. Expression IR

## LOCKED

After binding, use typed bound-expression nodes.

Initial kinds:

```text
BoundConstant
BoundColumnRef
BoundUnary
BoundBinary
BoundComparison
BoundBoolean
BoundCast
BoundFunction
BoundAggregate
BoundCase
BoundIsNull
BoundInList
BoundSubquery
```

Every bound expression exposes:

```text
LogicalType return_type
bool nullable
```

No type inference should remain for the executor.

---

# 345. Expression Immutability

## LOCKED

Bound expressions are immutable once produced.

Optimizer rewrites construct new expressions or structurally share immutable children.

Do not mutate expression meaning in place while multiple plan nodes may reference it.

This simplifies:

- rewrite correctness,
- memoization,
- common-subexpression reasoning,
- debug printing.

---

# 346. Expression Ownership

## LOCKED

Use query-plan arena ownership or shared immutable nodes.

Avoid one reference-counted heap allocation per tiny expression node if profiling shows excessive overhead.

Initial implementation may prefer an arena because plan lifetime is query scoped.

---

# 347. Operator Registry

## LOCKED

Arithmetic/comparison operator resolution should use a centralized registry/table.

Example signatures:

```text
+(INT32, INT32) -> INT32
+(INT64, INT64) -> INT64
+(FLOAT64, FLOAT64) -> FLOAT64

=(T,T) -> BOOLEAN nullable
<(T,T) -> BOOLEAN nullable
```

Binder inserts implicit casts before selecting the final operator implementation.

Do not scatter operator type rules across AST node classes.

---

# 348. Function Registry

## LOCKED

Functions are resolved through metadata descriptors:

```text
name
argument types / polymorphic rule
return type rule
volatility class
null handling
implementation ID
```

Initial scalar functions may be small.

The architecture must support later:

- deterministic functions,
- stable functions,
- volatile functions,
- aggregates.

---

# 349. Function Volatility

## LOCKED

Classify functions conceptually:

```text
IMMUTABLE
STABLE
VOLATILE
```

Initial optimizer constant folding may only pre-evaluate:

```text
IMMUTABLE
```

functions with constant arguments.

Do not constant-fold volatile expressions.

This decision matters later for optimizer correctness.

---

# 350. Aggregate Expressions

## LOCKED

Initial aggregates:

```text
COUNT(*)
COUNT(expr)
SUM(expr)
MIN(expr)
MAX(expr)
AVG(expr)
```

Bound aggregate nodes are semantically distinct from scalar function calls.

Binder validates legal placement.

Aggregates are not allowed arbitrarily inside:

```text
WHERE
JOIN ON
```

for the same SELECT level.

---

# 351. Aggregate Query Semantics

## LOCKED

A SELECT is an aggregate query if it contains:

```text
GROUP BY
or
aggregate expressions
```

For such queries, every SELECT/HAVING expression must be derivable from:

```text
grouping keys
aggregate results
constants
```

Version 1 uses strict SQL-style grouping validation.

Do not implement MySQL-like arbitrary non-grouped column selection.

---

# 352. HAVING

## LOCKED

Logical order:

```text
FROM / JOIN
WHERE
GROUP BY / aggregate
HAVING
SELECT projection
ORDER BY
LIMIT
```

HAVING may reference:

- grouping expressions,
- aggregate expressions,
- expressions derivable from them.

Binder resolves aliases according to the chosen SQL surface rather than executor order.

For v1, SELECT aliases are not visible inside HAVING unless explicitly added later.

---

# 353. ORDER BY Resolution

## LOCKED

ORDER BY may refer to:

```text
bound expression
SELECT-list alias
1-based SELECT-list ordinal
```

Resolution priority for a bare identifier should be deterministic and documented.

Version 1 recommendation:

1. match SELECT output alias,
2. otherwise bind as normal input expression.

Example:

```sql
SELECT salary AS s
FROM users
ORDER BY s;
```

is valid.

---

# 354. LIMIT / OFFSET

## LOCKED

Initial syntax accepts non-negative integer expressions that bind to integral types.

For v1, require them to be constant at execution start.

Negative values produce SQL errors.

Logical operators remain separate:

```text
LogicalLimit
```

with:

```text
limit
offset
```

---

# 355. DISTINCT

## LOCKED

`SELECT DISTINCT` becomes an explicit logical duplicate-elimination operator.

Conceptually:

```text
LogicalDistinct
```

or an aggregate-like logical node.

Do not hide DISTINCT as a flag inside projection that the physical planner cannot reason about.

---

# 356. CASE

## LOCKED

Support searched CASE:

```sql
CASE
    WHEN predicate THEN expr
    WHEN predicate THEN expr
    ELSE expr
END
```

Binder finds a common result type across branches using type coercion rules.

Missing ELSE behaves as:

```text
ELSE NULL
```

---

# 357. IN List

## LOCKED

Initial:

```sql
expr IN (expr, expr, ...)
```

binds as its own expression node.

Semantics must preserve NULL behavior.

Do not naively rewrite every IN list into an OR chain before optimization.

Later physical expression code may choose:

- linear comparison,
- hash set,
- sorted lookup

depending on list size.

---

# 358. Parameters

## DEFERRED, architecture-compatible

Prepared-statement parameters such as:

```sql
$1
?
```

are deferred from the first parser milestone.

The binder/type system should not assume every expression type is known solely from literals forever.

Future parameter typing can reuse `UNKNOWN` plus contextual type inference.

---

# 359. Logical Plan Node Contract

## LOCKED

Every logical plan node exposes at least:

```text
operator kind
children
output schema
estimated properties placeholder
debug representation
```

Output schema contains logical plan slots:

```text
SlotId / expression binding
display name
LogicalType
nullable
source lineage metadata
```

Logical nodes are immutable after construction.

---

# 360. Logical Plan Slot Identity

## LOCKED

Introduce query-local logical output slot IDs.

A slot is not:

```text
ColumnId
```

and not:

```text
physical vector position forever
```

It identifies one value flowing through a logical plan.

This matters for:

- self-joins,
- aliases,
- duplicate column names,
- computed expressions,
- optimizer rewrites.

---

# 361. Initial Logical Operators

## LOCKED

Relational operators:

```text
LogicalGet
LogicalValues
LogicalFilter
LogicalProject
LogicalJoin
LogicalAggregate
LogicalDistinct
LogicalSort
LogicalLimit
LogicalInsert
LogicalUpdate
LogicalDelete
LogicalCreateTable
LogicalCreateIndex
LogicalDrop
LogicalVacuum
LogicalExplain
```

Additional helper nodes may be introduced if they represent real relational semantics rather than physical algorithms.

---

# 362. LogicalGet

## LOCKED

Logical table access:

```text
LogicalGet
    TableId
    BindingId
    required output columns
    optional logical predicate set after rewrites
```

It means:

```text
read rows from this logical relation
```

It does **not** mean:

```text
sequential scan
```

or:

```text
index scan
```

That choice belongs to physical planning.

---

# 363. LogicalValues

## LOCKED

Represent literal row sets such as:

```sql
INSERT INTO t VALUES (1,'a'), (2,'b');
```

and potentially:

```sql
SELECT 1;
```

using:

```text
LogicalValues
```

with typed bound expressions.

This avoids special-casing constant-only queries throughout the executor.

---

# 364. LogicalFilter

## LOCKED

Represents:

```text
input relation
+
BOOLEAN predicate
```

Its output schema is identical to its child's schema.

LogicalFilter does not encode whether evaluation is:

- vectorized selection mask,
- index predicate,
- pushed into scan.

Those are later decisions.

---

# 365. LogicalProject

## LOCKED

Represents a list of output expressions:

```text
expr_0 -> output slot
expr_1 -> output slot
...
```

Projection can:

- reorder columns,
- duplicate values,
- calculate expressions,
- drop unused inputs.

It is the main semantic boundary between internal relation shape and SELECT output.

---

# 366. LogicalJoin

## LOCKED

Join node stores:

```text
join type
left child
right child
join condition
```

Initial join types:

```text
INNER
LEFT
CROSS
```

The condition is a bound BOOLEAN expression.

Do not encode:

```text
hash join
nested loop
merge join
```

inside the logical join.

---

# 367. Join Predicate Decomposition

## LOCKED

The logical optimizer may decompose conjunctions:

```text
a.x = b.x
AND a.y > 10
AND b.z < 20
```

into:

```text
equi-join predicates
left-local predicates
right-local predicates
residual predicates
```

This decomposition is optimizer metadata/rewrite output.

The original semantics must remain reproducible for correctness/debugging.

---

# 368. Outer Join Semantics

## LOCKED

LEFT JOIN introduces NULL-extended rows from the right side.

The output nullability metadata must reflect this.

Example:

```text
right-side NOT NULL column
```

becomes:

```text
nullable in LogicalJoin output
```

for LEFT JOIN.

This is important for later simplification and execution correctness.

---

# 369. LogicalAggregate

## LOCKED

Stores:

```text
grouping expressions
aggregate expressions
child
```

Output schema consists only of:

```text
group keys
aggregate outputs
```

Expressions after aggregation refer to these aggregate/group output slots, not arbitrary pre-aggregate columns.

---

# 370. LogicalSort

## LOCKED

Stores ordered sort keys:

```text
expression/slot
ASC | DESC
NULLS FIRST | NULLS LAST semantic flag
```

Version 1 parser surface may default:

```text
ASC -> NULLS FIRST
DESC -> NULLS LAST
```

or another explicitly documented choice.

The logical node must carry the resolved ordering semantics so the physical sorter/index-order matcher does not guess.

---

# 371. LogicalLimit

## LOCKED

Stores:

```text
optional limit
offset
```

Logical optimizer may later push LIMIT through safe operators or use Top-N physical implementations.

The logical meaning remains explicit.

---

# 372. LogicalInsert

## LOCKED

Stores:

```text
target TableId
target ColumnIds
input relation
column coercion expressions
RETURNING projection if supported
```

INSERT input may come from:

```text
VALUES
SELECT
```

Both use the same logical contract.

---

# 373. LogicalUpdate

## LOCKED

Stores:

```text
target TableId
target BindingId
child producing target physical RID + required old columns
assignment expressions by ColumnId
optional RETURNING
```

A critical hidden/internal output from the scan side is:

```text
target physical RID
```

needed by the transaction write protocol.

RID is an execution/system slot, not an SQL-visible column.

---

# 374. LogicalDelete

## LOCKED

Stores:

```text
target TableId
child producing target physical RID
optional RETURNING
```

DELETE logical planning must preserve the exact target row identity through filters/joins required by the supported SQL syntax.

Initial DELETE may support only one target base table.

---

# 375. Hidden System Slots

## LOCKED

Logical plans may carry hidden system values such as:

```text
RID
```

They are marked:

```text
system/internal
```

and do not appear in `SELECT *`.

Optimizer projection pruning must preserve hidden slots required by downstream DML operators.

---

# 376. CREATE TABLE Binding

## LOCKED

Binder validates:

```text
table name does not exist
column names are unique
types are supported
PRIMARY KEY shape
UNIQUE constraints
NOT NULL constraints
default expressions
```

Initial table constraints:

```text
PRIMARY KEY
UNIQUE
NOT NULL
```

Foreign keys and CHECK constraints are deferred initially.

---

# 377. PRIMARY KEY Semantics

## LOCKED

A PRIMARY KEY implies:

```text
UNIQUE
+
NOT NULL for every key column
```

Initial implementation creates a unique B+ tree index automatically.

Catalog records the primary-key constraint separately from the physical index object.

Do not treat "primary key" as merely an index-name flag with no semantic catalog representation.

---

# 378. CREATE INDEX Binding

## LOCKED

Initial index keys may reference base table columns only.

Expression indexes are deferred.

Binder resolves:

```text
table
column IDs
uniqueness
index name
```

and builds an `IndexDescriptor`.

For a unique index, transactional uniqueness rules from the lower layer apply.

---

# 379. CREATE INDEX Execution Semantics

## LOCKED

Initial implementation may require an exclusive schema/table-level DDL lock and build the index offline.

High-level:

```text
acquire DDL exclusivity
create B+ tree file
scan visible committed table state
encode keys
insert physical entries
validate uniqueness if requested
install catalog metadata
commit
```

Online concurrent index build is deferred.

Do not accidentally expose a half-built index to the optimizer.

---

# 380. DROP Semantics

## LOCKED

DROP TABLE / DROP INDEX should be transactional at the catalog visibility level.

Physical file deletion can be deferred until no active query/catalog descriptor can reference the object.

Introduce an object-retirement mechanism rather than unlinking files while other threads may still hold them open.

Initial implementation may serialize DDL aggressively.

---

# 381. DDL Concurrency

## LOCKED: conservative first

Use a coarse:

```text
SchemaLock
```

or equivalent catalog DDL mutex for schema-changing operations.

Ordinary queries should only hold short/shared metadata references, not this mutex for their whole runtime.

This is one area where conservative serialization is justified initially because:

- DDL frequency is low,
- catalog invalidation is subtle,
- learning reward from fine-grained concurrent DDL is lower than optimizer/executor work.

---

# 382. Catalog Transaction Visibility

## LOCKED

Catalog changes participate in normal transactional durability.

A schema object created by an uncommitted transaction must not become visible to other transactions.

Version 1 may achieve this by:

- storing catalog rows through normal MVCC,
- plus schema cache publication only after commit.

Do not publish immutable descriptor cache entries for uncommitted DDL.

---

# 383. Statement Binding Snapshot

## LOCKED

Binding uses the transaction's appropriate catalog visibility:

```text
READ COMMITTED:
    statement snapshot

REPEATABLE READ:
    transaction snapshot
```

Therefore a long-running repeatable-read transaction does not unexpectedly resolve objects created after its snapshot unless explicit DDL semantics later say otherwise.

Initial DDL concurrency may be stricter than this through schema locking.

---

# 384. INSERT Binding

## LOCKED

For:

```sql
INSERT INTO t(a,c) VALUES (...);
```

Binder:

1. resolves target table,
2. validates target columns unique,
3. maps omitted columns,
4. binds input expressions,
5. inserts allowed implicit casts,
6. fills defaults/NULLs,
7. validates NOT NULL possibility where statically known,
8. creates canonical full target-column order.

Execution should not need to resolve target column names.

---

# 385. Default Expressions

## LOCKED initial scope

Column defaults may initially be:

```text
constant
immutable scalar expression
```

evaluated per inserted row as needed.

Volatile defaults such as sequence functions can be added later.

Defaults are stored in bound/serializable catalog expression form, not raw SQL text only.

Raw original SQL may also be retained for display.

---

# 386. UPDATE Binding

## LOCKED

Binder validates:

```text
target table
assignment column names
no duplicate target assignments
assignment expression types
WHERE predicate BOOLEAN
```

Each assignment becomes:

```text
ColumnId -> typed bound expression
```

A column not mentioned retains its old value.

The logical planner requests old column slots needed to build the complete new physical tuple version.

---

# 387. DELETE Binding

## LOCKED

Binder resolves target table and BOOLEAN WHERE predicate.

Logical planner ensures the child produces:

```text
RID
```

plus values required for:

- unique-key lock cleanup semantics,
- RETURNING,
- executor predicates.

Index entries are still physically removed only by vacuum, per lower-layer design.

---

# 388. RETURNING

## LOCKED initial support recommendation

Support:

```sql
INSERT ... RETURNING ...
UPDATE ... RETURNING ...
DELETE ... RETURNING ...
```

because it strongly exercises:

- DML binding,
- expression projection,
- command visibility,
- statement retry buffering.

Per the transaction architecture, output must not be externally emitted before a READ COMMITTED statement retry is no longer possible.

If implementation scope becomes too large, RETURNING may be staged after basic DML but the plan structures should support it.

---

# 389. Constant Folding

## LOCKED logical rewrite

After binding, fold expressions that are:

```text
composed entirely of constants
and
use immutable operators/functions
```

Examples:

```text
1 + 2 -> 3
10 > 5 -> TRUE
FALSE AND expensive_expr -> FALSE only when SQL semantics/volatility permit
```

NULL/three-valued logic must be preserved.

Do not fold volatile functions.

---

# 390. Boolean Simplification

## LOCKED

Examples of legal simplifications under SQL three-valued logic must be encoded explicitly.

Safe examples:

```text
x AND TRUE  -> x
x OR FALSE  -> x
NOT NOT x   -> x
```

Be careful with rewrites involving NULL and volatile expressions.

Do not import ordinary two-valued boolean algebra blindly.

---

# 391. Predicate Pushdown

## LOCKED architecture

Logical optimizer may push predicates closer to base relations when semantics allow.

Examples:

```text
Filter(a.x > 10)
    ↓
InnerJoin(A,B)
```

can push the predicate to A if it references only A.

Outer joins require stricter null-preservation reasoning.

Do not apply inner-join pushdown rules blindly to LEFT JOIN.

---

# 392. Projection Pruning

## LOCKED

Compute columns/slots required by ancestors and remove unused outputs from children.

This should eventually reduce:

- tuple decoding,
- vector materialization,
- hash-table width,
- memory bandwidth.

Projection pruning must preserve:

- hidden RID slots needed by DML,
- join keys,
- filter inputs,
- sort keys,
- group keys.

---

# 393. Expression Canonicalization

## LOCKED

The logical optimizer may normalize expressions for easier rule matching.

Examples:

```text
flatten AND chains
canonicalize commutative equality ordering
normalize constant side of comparisons where legal
```

Keep source/display metadata separately if needed for EXPLAIN readability.

Do not canonicalize in ways that change floating-point, NULL, or volatile-function semantics.

---

# 394. Logical Join Graph

## LOCKED

For inner/cross joins, the optimizer should eventually extract a join graph:

```text
relations as vertices
join predicates as edges
local predicates attached to vertices
```

This becomes the input to cost-based join ordering.

Outer joins impose ordering constraints and must not be freely reordered as ordinary inner joins.

---

# 395. Logical Plan Construction for SELECT

## LOCKED

Canonical construction order:

```text
FROM
    ↓
JOINs
    ↓
WHERE Filter
    ↓
Aggregate, if needed
    ↓
HAVING Filter
    ↓
Projection
    ↓
Distinct
    ↓
Sort
    ↓
Limit/Offset
```

Optimizer may transform this where semantics permit.

This canonical shape is intentionally easy to reason about and compare against relational algebra.

---

# 396. SELECT Without FROM

## LOCKED

Support:

```sql
SELECT 1 + 2;
```

using:

```text
LogicalValues(single empty row)
    ↓
LogicalProject
```

This avoids special execution paths for constant SELECT statements.

---

# 397. INNER JOIN Binding

## LOCKED

For:

```sql
A JOIN B ON predicate
```

Binder:

1. creates left scope,
2. creates right binding,
3. creates combined ON-expression scope,
4. binds ON predicate as BOOLEAN,
5. returns joined relation binding.

ON can reference both sides.

---

# 398. LEFT JOIN Binding

## LOCKED

Same name-resolution model as INNER JOIN.

Output metadata marks right-side slots nullable regardless of base NOT NULL constraints.

Predicates in:

```text
ON
```

and:

```text
WHERE
```

must remain semantically distinct.

Do not collapse them during parsing/binding.

---

# 399. CROSS JOIN

## LOCKED

CROSS JOIN has no join condition.

Logical representation may use:

```text
LogicalJoin(CROSS)
```

Optimizer may later combine it with WHERE predicates into an inner join condition where equivalent.

---

# 400. NATURAL / USING JOIN

## DEFERRED

Do not initially implement:

```sql
NATURAL JOIN
JOIN ... USING(...)
```

They introduce substantial name-merging/output-column semantics that distract from the core join architecture.

Explicit `ON` is sufficient for the first serious engine.

---

# 401. Subquery Binding

## LOCKED architecture

Every subquery gets its own binding scope.

Uncorrelated subquery:

```text
does not reference parent scope
```

Future correlated subquery:

```text
may bind a reference through parent scope
```

Represent such references distinctly, e.g.:

```text
BoundCorrelatedColumnRef(depth, binding, column)
```

even if correlated execution is initially rejected after detection.

This prevents redesign later.

---

# 402. Scalar Subquery Semantics

## LOCKED

A scalar subquery must produce:

```text
exactly one column
```

Runtime cardinality:

```text
0 rows -> NULL
1 row  -> that value
>1 row -> SQL cardinality error
```

Initial physical execution may materialize an uncorrelated scalar subquery once.

---

# 403. EXISTS

## LOCKED

`EXISTS(subquery)` returns non-null BOOLEAN:

```text
TRUE if subquery returns at least one row
FALSE otherwise
```

Optimizer may later turn it into a semi-join.

The bound representation should preserve EXISTS semantics rather than immediately projecting arbitrary subquery values.

---

# 404. IN Subquery

## LOCKED architecture

For:

```sql
x IN (SELECT y ...)
```

bind the subquery and ensure one output column compatible with `x`.

NULL semantics must be correct.

Optimizer may later transform uncorrelated/correlated forms into:

```text
semi join
mark join
hash set
```

as appropriate.

---

# 405. Common Table Expressions

## DEFERRED

Non-recursive CTEs are deferred from the first parser milestone.

When added, they should be logical named subplans, not automatically forced materialization barriers.

Recursive CTEs are much later.

---

# 406. Expression Evaluation Contract

## LOCKED

Bound expressions must be executable in a vectorized manner.

Every expression should conceptually support:

```text
Evaluate(input DataChunk, selection)
    -> output Vector
```

but expression classes themselves should remain semantic and not own operator scheduling.

The vectorized executor implementation is specified in the next architecture stage.

---

# 407. Expression Nullability Metadata

## LOCKED

Every expression carries conservative nullability.

Examples:

```text
NOT NULL column ref -> non-nullable
nullable column ref -> nullable

x + y -> nullable if either input nullable
x = y -> nullable if either input nullable
IS NULL(x) -> non-nullable BOOLEAN

COUNT(*) -> non-nullable INT64
SUM(nullable input) -> nullable
```

Accurate nullability helps optimizer simplification and execution specialization.

---

# 408. Expression Lineage

## LOCKED recommendation

Track lightweight lineage for output slots:

```text
base TableId/ColumnId origins where meaningful
```

This helps:

- statistics lookup,
- predicate selectivity,
- EXPLAIN,
- projection pruning,
- index matching.

Computed expressions may have multiple/no direct base-column lineage.

---

# 409. Logical Properties

## LOCKED architecture

Logical optimizer nodes should eventually expose derived properties:

```text
output slots
candidate keys
nullability
known constants
ordering requirements/properties
estimated cardinality placeholder
```

Do not implement every property immediately.

Design node interfaces so physical planning does not require reparsing SQL syntax.

---

# 410. Constraint-Derived Logical Properties

## LOCKED

Catalog constraints may inform logical reasoning.

Examples:

```text
PRIMARY KEY -> candidate key
UNIQUE + NOT NULL -> candidate key
NOT NULL -> nullability
```

Potential optimizer uses later:

- join cardinality estimation,
- redundant DISTINCT removal,
- outer-to-inner join simplification,
- uniqueness-aware aggregation.

Do not perform such rewrites until proven semantics/tests exist.

---

# 411. Logical Plan Validation

## LOCKED

After binding/planning and after major optimizer rewrite phases, provide a debug validator that checks:

```text
all referenced slots exist in child outputs
expression types are resolved
filter predicates are BOOLEAN
join conditions are BOOLEAN
aggregate references are legal
hidden DML RID slots are preserved
output slot IDs are unique where required
node schemas agree with expressions
```

Do not rely solely on executor crashes to discover malformed plans.

---

# 412. EXPLAIN Logical Output

## LOCKED

Before physical planning is implemented, EXPLAIN should be able to print the logical tree.

Example:

```text
Limit 10
  Sort [orders.amount DESC]
    Project [users.name, orders.amount]
      Filter [orders.amount > 100]
        InnerJoin [users.id = orders.user_id]
          Get users
          Get orders
```

Include:

```text
slot IDs in debug mode
resolved table/index IDs where useful
types
nullability optionally
```

EXPLAIN output must not depend on AST pretty-printing after binding.

---

# 413. Error Model

## LOCKED

Distinguish:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature
```

Every user-facing front-end error should include:

```text
source span
human-readable message
```

where a source location exists.

---

# 414. Parser Error Recovery

## LOCKED scope

The first CLI/API parser may stop at the first syntax error for one statement.

When parsing multiple statements in one input batch, synchronize on:

```text
semicolon
end of input
```

to report later statement errors where practical.

Full IDE-grade error recovery is not required.

---

# 415. SQL Grammar Testing

## LOCKED

Use:

### Positive parser tests

Valid SQL -> expected AST shape.

### Negative parser tests

Invalid SQL -> expected syntax error and source span.

### Precedence tests

Verify:

```text
1 + 2 * 3
NOT a AND b
a OR b AND c
```

parse correctly.

### Round-trip debug tests

A debug AST formatter may produce canonical SQL-like output for inspection.

It need not reproduce original whitespace/comments.

---

# 416. Binder Tests

## LOCKED

Cover:

```text
unknown table
unknown column
ambiguous column
aliases
self-joins
qualified references
SELECT *
table.*
type promotion
invalid casts
NULL typing
aggregate placement
GROUP BY validation
ORDER BY alias
ORDER BY ordinal
LEFT JOIN nullability
DML target binding
unique/primary-key metadata
subquery scopes
```

Binder tests should not require physical execution.

Use an in-memory/mock catalog implementation where useful.

---

# 417. Type-System Property Tests

## LOCKED

Test consistency between:

```text
binder type resolution
constant evaluation
vectorized executor semantics
index-key comparator where relevant
```

Especially:

- signed numeric ordering,
- FLOAT64 NaN/zero semantics,
- NULL comparisons,
- three-valued logic,
- implicit numeric promotion.

A query must not have one semantic meaning in the binder and another in the index comparator/executor.

---

# 418. Catalog Tests

## LOCKED

Test:

```text
bootstrap open
create table metadata
create index metadata
reopen persistence
name lookup by normalized identifier
quoted identifiers
stable object IDs
schema version increment
descriptor-cache invalidation
transactional catalog visibility
DROP retirement
```

---

# 419. Logical Planner Tests

## LOCKED

Given bound statements, assert canonical logical-plan shapes.

Examples:

```sql
SELECT a FROM t WHERE b > 5;
```

becomes:

```text
Project(a)
  Filter(b > 5)
    Get(t)
```

Aggregate example:

```sql
SELECT dept, COUNT(*)
FROM emp
WHERE active
GROUP BY dept
HAVING COUNT(*) > 3;
```

becomes canonical:

```text
Project
  Filter(HAVING)
    Aggregate
      Filter(WHERE)
        Get(emp)
```

---

# 420. Logical Rewrite Tests

## LOCKED

For every rewrite rule:

```text
input logical plan
expected transformed plan
```

plus, once execution exists:

```text
differential semantic test:
    original plan result == rewritten plan result
```

Use randomized data with NULLs.

This is particularly important for:

- boolean simplification,
- predicate pushdown,
- outer joins,
- DISTINCT,
- aggregates.

---

# 421. Catalog / Front-End Benchmarks

## LOCKED

Performance is less critical than execution/storage, but benchmark enough to avoid pathological architecture.

Measure:

```text
lexer MB/s
parser statements/sec
binder latency
catalog name lookup latency
large SELECT-list binding
large expression-tree binding
logical plan construction time
```

Optimizer planning benchmarks will later dominate complex-query planning work.

---

# 422. Parser/AST Memory Benchmark

## LOCKED recommendation

Track allocations/bytes for:

```text
100-column SELECT
large VALUES insert
deep boolean expression
multi-join query
```

Arena allocation should keep allocation count low.

Do not optimize syntax parsing before profiling, but prevent obvious per-token/per-node heap churn.

---

# 423. Front-End Fuzzing

## LOCKED

Fuzz at least:

```text
lexer
parser
tuple/type literal conversion
bound expression constant evaluator
```

Requirements:

- no crashes,
- no out-of-bounds access,
- bounded failure behavior,
- useful parser error instead of undefined behavior.

SQL parser fuzzing is high-value because arbitrary text reaches it directly.

---

# 424. Supported SQL v1 Target

## LOCKED milestone target

A realistic first SQL surface should eventually handle queries like:

```sql
CREATE TABLE users (
    id INT64 PRIMARY KEY,
    name VARCHAR NOT NULL,
    age INT32,
    salary FLOAT64
);

CREATE INDEX users_age_idx ON users(age);

INSERT INTO users(id, name, age, salary)
VALUES
    (1, 'Alice', 24, 70000),
    (2, 'Bob', 31, 82000);

SELECT name, salary
FROM users
WHERE age >= 25
ORDER BY salary DESC
LIMIT 10;

UPDATE users
SET salary = salary * 1.05
WHERE age >= 30;

DELETE FROM users
WHERE id = 2;
```

and multi-table aggregate queries such as:

```sql
SELECT u.name, COUNT(*) AS order_count, SUM(o.amount) AS total
FROM users u
JOIN orders o ON u.id = o.user_id
WHERE o.amount > 10
GROUP BY u.id, u.name
HAVING COUNT(*) >= 2
ORDER BY total DESC
LIMIT 20;
```

This is enough SQL to exercise the real relational engine without becoming a SQL-standard compatibility project.

---

# 425. Deliberately Deferred SQL Features

## LOCKED

Do not implement before the core planner/executor is mature:

```text
ALTER TABLE
foreign keys
CHECK constraints
views
materialized views
triggers
stored procedures
recursive CTEs
window functions
RIGHT/FULL OUTER JOIN
NATURAL/USING JOIN
MERGE statement
UPSERT / ON CONFLICT
generated columns
sequences
identity columns
expression indexes
partial indexes
collation framework
time zones
DECIMAL/NUMERIC
INTERVAL
JSON
arrays
user-defined types
prepared statement parameters
SQL privileges
roles
```

These remain future expansion points.

---

# 426. High-Reward Future Front-End Features

## LOCKED recommendation

Strong later learning projects include:

```text
correlated subquery decorrelation
window functions
recursive CTE execution
DECIMAL/NUMERIC implementation
collation-aware string comparison
prepared statements and parameter typing
ALTER TABLE with schema-version translation
foreign-key enforcement
expression indexes
partial indexes
views and view expansion
```

Prioritize only after the physical optimizer/executor exists.

---

# 427. Front-End / Logical-Layer Invariants

## LOCKED

Codex must preserve:

1. Parser output contains syntax names, not resolved object IDs.
2. Binder output contains resolved IDs and complete types.
3. Executor never performs SQL name resolution.
4. Logical operators never encode physical algorithms.
5. `ColumnId`, `BindingId`, and logical output slot IDs are distinct concepts.
6. Self-joins use different BindingIds even for the same TableId.
7. All bound expressions have resolved return type and nullability.
8. SQL boolean semantics are three-valued.
9. Non-boolean WHERE/HAVING/ON predicates are rejected.
10. `x = NULL` is not rewritten into `x IS NULL`.
11. Numeric implicit casts widen but do not silently narrow.
12. Catalog object references use stable IDs after binding.
13. SELECT `*` is expanded during binding.
14. Aggregate legality is validated before execution.
15. LEFT JOIN right-side outputs become nullable.
16. DML plans preserve hidden physical RID slots where required.
17. LogicalFilter does not imply how the predicate is physically evaluated.
18. LogicalGet does not imply sequential or index access.
19. LogicalJoin does not imply hash/nested-loop/merge implementation.
20. SQL source spans survive long enough for useful diagnostics.
21. Catalog cache never publishes uncommitted DDL as committed metadata.
22. Immutable descriptors/plans are not mutated behind active queries.
23. Optimizer rewrites preserve NULL and volatility semantics.
24. Logical plan validators can detect broken slot/schema references.
25. The supported SQL subset is explicit; unsupported syntax fails clearly rather than being half-interpreted.

---

# 428. Recommended Module Layout for the Upper Layer

## LOCKED

```text
src/
  catalog/
    catalog.h
    catalog.cpp
    catalog_cache.h
    catalog_bootstrap.h
    table_descriptor.h
    index_descriptor.h
    schema_descriptor.h
    system_tables.h

  sql/
    lexer/
      token.h
      lexer.h
      lexer.cpp

    parser/
      ast.h
      parser.h
      parser.cpp
      expression_parser.cpp

    types/
      logical_type.h
      value.h
      type_resolver.h
      type_resolver.cpp

    binder/
      binding.h
      bind_context.h
      binder.h
      binder.cpp
      bound_statement.h
      bound_expression.h
      function_registry.h
      operator_registry.h

  planner/
    logical/
      logical_operator.h
      logical_get.h
      logical_filter.h
      logical_project.h
      logical_join.h
      logical_aggregate.h
      logical_sort.h
      logical_limit.h
      logical_dml.h
      logical_ddl.h
      logical_plan_validator.h

    logical_planner.h
    logical_planner.cpp

  optimizer/
    logical_rewrite/
      constant_fold.h
      boolean_simplify.h
      predicate_pushdown.h
      projection_prune.h
      expression_canonicalize.h
```

Exact filenames may evolve.

The layer boundaries may not.

---

# 429. Implementation Order for Catalog + SQL Front End

## LOCKED

### Phase U1 — Type system

1. `LogicalType`
2. generic `Value`
3. NULL semantics
4. numeric promotion
5. casts
6. operator registry
7. type-resolution tests

### Phase U2 — Catalog

8. bootstrap descriptors
9. system-table schemas
10. catalog lookup
11. immutable descriptors
12. catalog cache
13. transactional metadata visibility
14. create/drop metadata tests

### Phase U3 — Lexer/parser

15. token model
16. lexer
17. source spans
18. Pratt expression parser
19. SELECT
20. CREATE TABLE / INDEX
21. INSERT
22. UPDATE
23. DELETE
24. transaction statements
25. parser fuzzing

### Phase U4 — Binder

26. scopes and BindingId
27. column resolution
28. type resolution/casts
29. wildcard expansion
30. joins
31. aggregates/GROUP BY
32. ORDER BY
33. DDL/DML binding
34. subquery binding
35. binder test suite

### Phase U5 — Logical planning

36. logical output slots
37. LogicalGet/Values
38. Filter/Project
39. Join
40. Aggregate
41. Distinct/Sort/Limit
42. DML/DDL nodes
43. plan validator
44. logical EXPLAIN

### Phase U6 — Initial logical rewrites

45. constant folding
46. boolean simplification
47. projection pruning
48. basic predicate pushdown
49. canonicalization
50. rewrite differential tests

Do not start cost-based physical optimization before this logical layer can represent and validate real queries.

---

# 430. Upper-Layer Milestone 1

## LOCKED target

Without physical execution:

```text
SQL text
  ↓
parse
  ↓
bind against catalog
  ↓
produce typed logical plan
  ↓
EXPLAIN logical plan
```

for:

```text
CREATE TABLE
CREATE INDEX
INSERT VALUES
basic SELECT
WHERE
INNER/LEFT JOIN
GROUP BY
HAVING
ORDER BY
LIMIT
UPDATE
DELETE
```

must work deterministically.

---

# 431. Upper-Layer Milestone 2

## LOCKED target

Connect catalog DDL to the transactional storage core:

```text
CREATE TABLE
CREATE INDEX
DROP
```

and persist/reopen metadata.

At this stage:

```text
create schema
restart database
parse/bind query against reopened catalog
```

must work.

---

# 432. Upper-Layer Milestone 3

## LOCKED target

Connect logical plans to the vectorized executor specified in the next architecture stage.

Required first end-to-end path:

```text
SQL
  ↓
Parser
  ↓
Binder
  ↓
Logical Plan
  ↓
Physical Plan
  ↓
Vectorized Executor
  ↓
Transactional Heap/B+ Tree
```

Only after this path works should broad SQL syntax expansion become a priority.

---

# 433. Architecture Status After Upper Semantic Layer

## LOCKED v1

The architecture now specifies:

```text
persistent storage
heap layout
buffer pool
B+ tree
MVCC
locking
WAL
group commit
checkpoint/recovery
vacuum
catalog
SQL types
lexer/parser
AST
binder
typed expression IR
logical relational algebra
basic logical rewrites
DDL/DML semantic planning
```

The next architecture stage should lock:

```text
vectorized execution engine
physical operators
pipeline model
query memory manager
hash tables
hash join
aggregation
sorting
spilling
parallelism boundary
physical plan representation
```

After that, the final major architecture stage should lock:

```text
statistics
cardinality estimation
cost model
access-path selection
join enumeration
physical optimization
EXPLAIN ANALYZE
```

Those two stages will complete the core database-engine architecture.

---

# 434. Vectorized Physical Execution Engine Contract

## LOCKED

This section defines the concrete execution engine.

The engine is:

```text
vectorized
chunk-at-a-time
pipeline-oriented
memory-budgeted
spill-capable
parallel-ready
```

The execution layer consumes:

```text
typed logical / physical plans
```

and talks to:

```text
transactional heap
B+ tree
catalog descriptors
query memory manager
```

It must not perform:

```text
SQL parsing
name resolution
catalog-name lookup in hot loops
cost-based planning
```

The practical performance objective is:

> amortize branches, virtual dispatch, synchronization, allocation, and tuple decoding across batches instead of paying them once per row.

---

# 435. Physical Plan vs Runtime Operator State

## LOCKED

Distinguish:

```text
PhysicalOperator
    immutable query plan description

GlobalOperatorState
    state shared by all workers executing that operator

LocalOperatorState
    state owned by one worker/task
```

A physical plan may be cached/reused later.

Runtime state is created per execution.

Never store transaction-specific mutable state directly inside an immutable physical-plan node.

---

# 436. Physical Plan Node Contract

## LOCKED

Every physical operator exposes conceptually:

```text
operator kind
children
output schema
required input slots
physical properties
estimated rows/cost placeholders
EXPLAIN metadata
```

Runtime execution methods belong to operator implementation/state interfaces, not to SQL AST classes.

---

# 437. Initial Physical Operators

## LOCKED

Initial physical operators:

```text
PhysicalSeqScan
PhysicalIndexScan
PhysicalValues

PhysicalFilter
PhysicalProject

PhysicalNestedLoopJoin
PhysicalHashJoin
PhysicalIndexNestedLoopJoin
PhysicalMergeJoin          later in execution milestone

PhysicalHashAggregate
PhysicalSortAggregate      later
PhysicalDistinct

PhysicalSort
PhysicalTopN
PhysicalLimit

PhysicalInsert
PhysicalUpdate
PhysicalDelete

PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
PhysicalVacuum

PhysicalExplain
PhysicalResultSink
```

Additional physical operators are allowed when they represent a distinct execution algorithm.

---

# 438. DataChunk

## LOCKED

The basic execution unit is:

```cpp
DataChunk
```

Conceptually:

```text
capacity
cardinality
Vector columns[]
```

Initial standard capacity:

```text
1024 rows
```

Every normal vector in a chunk has the same logical cardinality.

The final chunk of a stream may contain fewer rows.

A chunk with:

```text
cardinality = 0
```

contains no result rows and is not used as an end-of-stream sentinel.

End-of-stream is represented explicitly by operator/source status.

---

# 439. Why 1024 Rows

## LOCKED rationale

1024 rows is large enough to amortize:

- function dispatch,
- selection setup,
- buffer access,
- expression dispatch,
- iterator overhead,

while remaining small enough that common vectors fit comfortably in CPU caches.

Examples:

```text
1024 INT64 values = 8 KiB
1024 INT32 values = 4 KiB
1024 uint16 selection indices = 2 KiB
1024 validity bits = 128 bytes
```

The capacity is a compile-time/default constant but should remain configurable in benchmarks.

Do not scatter literal `1024` assumptions through operator code.

---

# 440. Vector Kinds

## LOCKED initial set

Execution vectors support:

```text
FLAT
CONSTANT
DICTIONARY
```

### FLAT

Owns or references a contiguous typed value buffer.

### CONSTANT

Represents one value repeated for all active rows.

Useful for:

- literals,
- constant-folded expressions,
- NULL vectors.

### DICTIONARY

References another vector through a selection vector.

Useful for:

- filters,
- joins,
- projection/reference propagation

without copying every surviving value.

Later vector kinds may include:

```text
SEQUENCE
RLE
```

only after profiling demonstrates value.

---

# 441. Flat Numeric Vector Layout

## LOCKED

For fixed-width types:

```text
data buffer
+
validity mask
```

Data buffer is contiguous and naturally aligned.

Examples:

```text
INT32    int32_t[capacity]
INT64    int64_t[capacity]
FLOAT64  double[capacity]
BOOLEAN  uint8_t[capacity]
DATE     int32_t[capacity]
TIMESTAMP int64_t[capacity]
```

BOOLEAN is intentionally byte-per-value in execution memory.

Do not bit-pack BOOLEAN execution vectors initially.

Reason:

- simpler SIMD/vector loops,
- cheaper random access,
- lower bit-manipulation overhead.

Storage remains independent.

---

# 442. Validity Mask

## LOCKED

Use one validity bit per logical row.

Convention:

```text
1 = valid / non-NULL
0 = NULL
```

Store in 64-bit words.

For 1024 rows:

```text
16 uint64_t words
= 128 bytes
```

A vector may carry a fast flag:

```text
all_valid = true
```

so hot kernels skip validity-mask reads when no NULL exists.

---

# 443. SelectionVector

## LOCKED

Use:

```cpp
uint16_t indices[capacity]
```

for standard chunks.

Because:

```text
capacity <= 65535
```

a 16-bit index is sufficient.

A SelectionVector maps logical active positions to underlying vector positions.

Common cases:

```text
identity selection
filter selection
dictionary vector selection
join match selection
```

Avoid materializing an identity array when a boolean/enum can represent identity mapping.

---

# 444. Dictionary Composition

## LOCKED

Repeated filtering/projection can create:

```text
dictionary over dictionary over dictionary
```

Do not allow unbounded indirection chains.

Before hot evaluation, normalize through:

```text
UnifiedVectorFormat
```

or flatten selection composition into one effective selection vector.

This prevents one logical lookup from becoming many dependent pointer/index loads.

---

# 445. UnifiedVectorFormat

## LOCKED

Provide an adapter exposing any input vector as:

```text
typed base data pointer
effective selection
validity information
```

Conceptually:

```cpp
UnifiedVectorFormat {
    const void* data;
    SelectionView sel;
    ValidityView validity;
}
```

Vectorized kernels can therefore handle:

```text
FLAT
CONSTANT
DICTIONARY
```

without one type/representation branch per row.

Representation dispatch happens once per vector/chunk.

---

# 446. VARCHAR Execution Representation

## LOCKED

Use a compact in-memory string reference:

```cpp
struct StringRef {
    uint32_t length;
    uint32_t prefix;
    const char* data;
};
```

On 64-bit systems this is naturally 16 bytes.

`prefix` caches up to the first four bytes in canonical byte order for fast equality/order rejection before touching the full string.

Semantics:

```text
length = exact byte length
data   = pointer to bytes
```

No NUL terminator is required.

---

# 447. String Lifetime Rules

## LOCKED

A `StringRef` is only valid while its owning memory remains alive.

Possible owners:

```text
DataChunk StringHeap
query arena / RowCollection
hash-table build storage
sort run storage
constant-plan storage
```

A blocking operator that retains a string beyond the current input chunk must deep-copy it into operator/query-owned storage.

Never retain a `StringRef` pointing into:

- an unpinned heap page,
- a source chunk that will be reset,
- a temporary expression buffer that will be recycled.

---

# 448. DataChunk StringHeap

## LOCKED

Each reusable DataChunk owns a small chunk-local variable-length string heap.

When a scan decodes VARCHAR values from heap tuples:

```text
copy bytes into chunk StringHeap
store StringRef into VARCHAR vector
```

This deliberately avoids keeping heap page guards pinned merely because downstream execution still references a string.

Reset the StringHeap only when the chunk is no longer in use.

---

# 449. Borrowed Vectors Inside a Pipeline

## LOCKED

Non-blocking operators may produce dictionary/reference vectors borrowing an input chunk when the pipeline guarantees immediate downstream consumption before that input chunk is reused.

Examples:

```text
Filter
simple column Project
```

Blocking operators must materialize/deep-copy retained data.

The pipeline executor owns this lifetime guarantee.

Do not expose borrowed chunks asynchronously to client code.

---

# 450. Chunk Reuse

## LOCKED

Operators should reuse preallocated DataChunks and vector buffers.

Hot loops must not:

```text
allocate a new DataChunk
allocate every vector
free every vector
```

for every batch.

Use:

```text
operator-local reusable chunks
buffer pools
query-lifetime allocations
```

as appropriate.

---

# 451. Execution Row Layout

## LOCKED

Blocking operators need a compact row representation distinct from:

```text
heap tuple format
```

and:

```text
columnar DataChunk format
```

Define:

```text
RowLayout
```

from a list of logical types.

Row layout contains:

```text
null bitmap
fixed-width values
StringRef-like offset/length descriptors
variable payload in row/block storage
optional hash/next metadata owned by operator
```

Uses:

- hash-join build rows,
- grouped aggregate keys/state,
- sort materialization,
- DML target spool,
- spill serialization.

Do not reuse persistent heap-tuple headers (`xmin/xmax/...`) for query temporary rows.

---

# 452. RowCollection

## LOCKED

A `RowCollection` stores temporary rows in large contiguous blocks.

Initial block target:

```text
256 KiB
```

configurable.

Properties:

```text
append-oriented
stable row handles while collection lives
bulk deallocation
minimal per-row allocation
```

Variable-length payload may use associated varlen blocks.

A row handle is query-local and not a persistent RID.

---

# 453. Query Arena

## LOCKED

Each query owns an arena for small lifetime-bound objects such as:

```text
expression state
operator state
temporary key descriptors
small metadata arrays
```

Arena allocation:

```text
bump pointer
bulk release at query end
```

Do not use the arena for all potentially huge data.

Large/spillable buffers must be accounted through the QueryMemoryManager.

---

# 454. QueryMemoryManager

## LOCKED

Introduce:

```text
QueryMemoryManager
```

responsible for:

```text
per-query memory accounting
global memory accounting
operator reservations
spill pressure
hard-limit enforcement
```

Initial configuration should expose:

```text
global database execution-memory limit
per-query soft budget
per-query hard maximum
```

Exact defaults are deployment configuration, not persistent-format invariants.

---

# 455. Memory Reservations

## LOCKED

Large operators request tracked reservations:

```text
MemoryReservation
```

Examples:

```text
hash join build
hash aggregate
sort
DISTINCT
DML spool
```

A reservation records:

```text
bytes currently held
operator owner
spillability
```

Memory obtained through tracked large allocations must not bypass accounting.

---

# 456. Memory Pressure Protocol

## LOCKED

When an operator cannot extend its reservation:

```text
if operator is spillable:
    request/perform spill
    retry reservation
else:
    fail query with controlled out-of-memory error
```

Do not let the process discover query memory exhaustion through random `std::bad_alloc` in a hot operator.

---

# 457. SpillManager

## LOCKED

Introduce query-local:

```text
SpillManager
```

creating temporary files under a dedicated temp directory.

It owns:

```text
temp file lifecycle
block allocation
sequential read/write
checksums where useful
cleanup on query completion/error
```

Spill data is:

```text
temporary
not WAL logged
not recovered after process crash
```

A crashed query is aborted; its temp files are cleaned during startup/temp-directory maintenance.

---

# 458. Spill Block Format

## LOCKED

Use a simple self-describing temporary block format.

Each spill block contains at least:

```text
magic
format version
operator/run/partition ID
payload length
row count
CRC32C
payload
```

Payload serialization uses explicit query RowLayout/vector formats.

Do not dump compiler-native C++ object memory directly to spill files.

---

# 459. Spill I/O Pattern

## LOCKED

Prefer:

```text
large sequential writes
large sequential reads
```

over many tiny records.

Initial spill I/O block target:

```text
1 MiB
```

configurable.

Buffer at operator level before issuing disk writes.

---

# 460. Expression Executor

## LOCKED

Bound expressions are compiled/translated into reusable execution state.

Execution is vectorized:

```text
Evaluate(expr, input chunk, active selection)
    -> Vector result
```

Operator/type dispatch occurs per expression/vector batch, not per row.

---

# 461. Vectorized Arithmetic Kernels

## LOCKED

For fixed-width arithmetic:

```text
resolve physical type once
obtain UnifiedVectorFormat inputs
loop over active rows
write flat result
```

Provide specialized no-NULL fast paths.

Conceptually:

```cpp
if (lhs.all_valid && rhs.all_valid) {
    FastKernel(...);
} else {
    NullableKernel(...);
}
```

Do not put generic `Value` construction in inner arithmetic loops.

---

# 462. Vectorized Comparison Kernels

## LOCKED

Comparison kernels produce:

```text
BOOLEAN vector
```

with SQL NULL semantics.

Filter evaluation may instead write matching row positions directly into a SelectionVector to avoid materializing a boolean vector when possible.

VARCHAR comparison:

1. length/prefix fast rejection where applicable,
2. full byte comparison only when needed.

---

# 463. AND / OR Short-Circuiting

## LOCKED

Preserve SQL three-valued logic while evaluating only necessary row subsets.

Example for:

```text
A AND B
```

after evaluating `A`:

```text
A = FALSE
    result definitely FALSE; B need not run

A = TRUE or NULL
    evaluate B for these rows
```

Likewise for OR.

This produces selection-based vectorized short circuiting.

Do not eagerly evaluate volatile/error-producing branches whose SQL semantics permit them to be skipped.

---

# 464. Physical Pipeline Model

## LOCKED

Execution is organized into pipelines:

```text
Source
  ↓
zero or more streaming Operators
  ↓
Sink
```

Examples:

```text
SeqScan -> Filter -> Project -> ResultSink
```

and:

```text
SeqScan(build)
    -> HashJoinBuildSink
        [pipeline dependency]
SeqScan(probe)
    -> HashJoinProbe
    -> Project
    -> ResultSink
```

Pipelines form a dependency DAG.

---

# 465. Pipeline Roles

## LOCKED

### Source

Produces DataChunks.

Examples:

```text
SeqScan
IndexScan
Values
hash-table scan during some operators
sort run merge source
```

### Streaming operator

Transforms one input chunk without needing all future input.

Examples:

```text
Filter
Project
Limit
hash join probe
```

### Sink / pipeline breaker

Consumes input and must build/finalize state before dependent output continues.

Examples:

```text
HashJoin build
HashAggregate
Sort
Distinct build
DML target materialization
```

---

# 466. Physical Plan to Pipeline Graph

## LOCKED

Build pipelines after the physical plan is finalized.

Do not force every operator into a recursive `Next()` call chain.

The pipeline builder identifies:

```text
sources
streaming chains
sinks
dependencies
```

This is the execution-time representation.

The immutable physical tree remains useful for:

```text
EXPLAIN
optimizer output
plan validation
```

---

# 467. Pipeline Runtime Interfaces

## LOCKED conceptually

A source supports something like:

```text
GetData(local_source_state, output_chunk)
```

A streaming operator:

```text
Execute(input_chunk, output_chunk, local_state)
```

A sink:

```text
Sink(input_chunk, local_sink_state, global_sink_state)
Combine(local_sink_state, global_sink_state)
Finalize(global_sink_state)
```

Exact C++ virtual/template structure may change.

Semantics and state separation are locked.

---

# 468. Single-Thread First, Parallel-Ready

## LOCKED

First production execution milestone may run each query with one worker.

However, every pipeline/operator state design must distinguish:

```text
global state
local worker state
```

from day one.

Do not write operators around implicit thread-local globals or one mutable object that makes later parallelization require a complete rewrite.

---

# 469. Pipeline Cancellation

## LOCKED

Each query has:

```text
QueryExecutionContext
```

with:

```text
transaction
snapshot
memory manager
spill manager
cancellation flag
profiling state
```

Long loops periodically check cancellation at chunk or reasonable block boundaries.

Cancellation must unwind through RAII:

```text
page guards
memory reservations
temp files
locks via transaction abort path
```

---

# 470. Sequential Scan

## LOCKED

`PhysicalSeqScan` operates over heap pages in ascending physical page order.

The scan receives:

```text
TableDescriptor
required ColumnIds
snapshot
optional pushed predicates
```

It requests pages through BufferPool.

For each heap page:

1. read-latch page,
2. iterate slots,
3. inspect tuple header,
4. apply MVCC visibility,
5. evaluate eligible cheap pushed predicates if configured,
6. decode only required columns,
7. append visible rows to output DataChunk,
8. release page guard when safe.

Do not decode every table column for `SELECT one_column`.

---

# 471. Scan Page Guard Lifetime

## LOCKED

A scan may not return vectors that contain pointers into an unpinned heap page.

Fixed-width values are copied into execution vectors.

VARCHAR bytes are copied into the output chunk StringHeap.

Therefore the heap page may be unpinned after the scan advances.

This prevents long pipelines from pinning many storage pages.

---

# 472. Scan Predicate Pushdown Boundary

## LOCKED

Physical scan may evaluate predicates that have already been proven semantically safe and assigned by the physical planner.

The scan itself does not discover new SQL rewrite opportunities.

Useful scan-local predicates may reduce:

```text
tuple decoding
chunk output cardinality
memory traffic
```

General filter semantics remain representable by `PhysicalFilter`.

---

# 473. Index Scan

## LOCKED

`PhysicalIndexScan` stores:

```text
IndexId
encoded lower bound
encoded upper bound
bound inclusivity
required heap columns
snapshot
```

Execution:

```text
B+ tree range cursor
    ↓
batch candidate RIDs
    ↓
heap fetch
    ↓
MVCC visibility check
    ↓
decode required columns
    ↓
output chunk
```

An index entry is always a candidate, never proof of row visibility.

---

# 474. RID Batching for Index Scan

## LOCKED initial optimization

Do not fetch exactly one heap RID and immediately return one row at a time.

Collect a small candidate RID batch.

Initial target:

```text
up to one DataChunk worth
```

Then process candidates.

The first implementation may preserve exact index order and fetch in that order.

Later optimizer/executor work may reorder heap fetches by PageId only when query ordering semantics permit.

---

# 475. Index Order Property

## LOCKED

A forward B+ tree range scan produces physical user-key order.

Physical planning may use this to satisfy compatible:

```text
ORDER BY indexed columns ASC
```

provided:

- collation matches,
- NULL ordering matches,
- required prefix/direction matches,
- MVCC heap filtering does not alter order.

This property belongs in physical-plan properties, not ad-hoc executor logic.

---

# 476. PhysicalFilter

## LOCKED

Evaluate predicate into a SelectionVector.

Preferred output when possible:

```text
DICTIONARY/reference vectors over input
+
reduced cardinality
```

rather than copying every surviving cell.

If downstream lifetime requires ownership, materialize/flatten at the appropriate boundary.

---

# 477. PhysicalProject

## LOCKED

Projection expressions are evaluated vectorized.

Simple column references may forward/reference input vectors.

Computed expressions allocate/reuse output flat vectors.

Projection is not a pipeline breaker.

---

# 478. PhysicalLimit

## LOCKED

Maintain:

```text
rows_skipped
rows_emitted
```

per execution.

It may slice/select an input chunk rather than copy.

Once limit is satisfied:

```text
signal upstream cancellation/early-stop for this pipeline
```

where safe.

Do not continue scanning millions of rows after `LIMIT 10` when no blocking operator requires it.

---

# 479. Nested-Loop Join

## LOCKED baseline

Implement nested-loop join first as the correctness/small-input baseline.

For small right input:

```text
materialize right side
```

then probe chunks from left.

Support:

```text
INNER
LEFT
CROSS
```

The cost-based optimizer later decides when it is appropriate.

Do not use nested loop as the universal fallback for large equi-joins once hash join exists.

---

# 480. Index Nested-Loop Join

## LOCKED

For each outer chunk:

1. vector-evaluate inner lookup keys,
2. batch B+ tree point/range lookups where practical,
3. fetch candidate heap versions,
4. apply MVCC,
5. apply residual join condition,
6. emit joined rows.

Best for:

```text
small outer side
+
selective indexed inner lookup
```

Physical optimizer chooses it later.

---

# 481. Hash Join Role

## LOCKED

Main equality-join implementation:

```text
PhysicalHashJoin
```

Supports initial:

```text
INNER
LEFT
```

Hash join uses:

```text
build side
probe side
equality key expressions
optional residual predicate
```

The physical optimizer decides which input is build side.

---

# 482. Hash Join Build Storage

## LOCKED

Build rows are stored in:

```text
RowCollection
```

with a build RowLayout containing:

```text
join key values required for equality validation
payload columns required downstream
optional hash
duplicate-chain metadata
```

Variable-length build values are deep-copied into build-owned memory.

Do not retain pointers to input DataChunks.

---

# 483. Hash Join Hash Directory

## LOCKED

Use a power-of-two open-addressed directory.

Each occupied directory entry contains conceptually:

```text
64-bit hash
head build-row index/handle
```

Rows with equal join keys form a compact duplicate chain/list in RowCollection metadata.

Collision handling:

```text
hash match
    ↓
full SQL key equality check
```

Different keys with the same hash remain separate directory entries/probe sequences.

Initial target maximum load factor:

```text
~0.70
```

Resize before pathological probe lengths.

---

# 484. Why Directory + Duplicate Chains

## LOCKED rationale

A join needs to represent:

```text
many build rows with the same key
```

efficiently.

Storing one open-addressed directory entry per duplicate wastes probe-table space.

Instead:

```text
hash/key directory
    ↓
head of matching-row chain
    ↓
all build payload rows for that key
```

This keeps lookup locality good while preserving duplicate join semantics.

---

# 485. Hash Function Contract

## LOCKED

Centralize type-aware 64-bit hashing.

Required invariant:

```text
if SQL equality says A == B
then Hash(A) == Hash(B)
```

under the database's canonical semantics.

This matters especially for:

```text
FLOAT64 -0/+0
canonical NaN policy where equality permits
VARCHAR binary bytes
NULL handling
composite keys
```

Composite key hashing repeatedly mixes component hashes with a strong 64-bit avalanche function.

Do not use `std::hash` as a persistent semantic contract.

Hash values are query-local and need not be stable across database versions.

---

# 486. Hash Join NULL Semantics

## LOCKED

For ordinary equality:

```sql
A.key = B.key
```

NULL does not equal NULL.

Therefore build rows with a NULL in any equi-join key component:

```text
cannot match through the hash directory
```

For LEFT JOIN, unmatched probe-side rows still generate NULL-extended output as required.

Future `IS NOT DISTINCT FROM` semantics may use a different key mode.

---

# 487. Hash Join Build Pipeline

## LOCKED

Build pipeline:

```text
build source
    ↓
build-side filters/projections
    ↓
HashJoinBuildSink
```

Sink:

1. evaluate join keys vectorized,
2. discard/non-hash NULL keys as semantics allow,
3. deep-copy required build rows,
4. compute/store hashes,
5. append rows,
6. build/finalize hash directory.

Dependent probe pipeline cannot begin until build `Finalize`.

---

# 488. Hash Join Probe State

## LOCKED

A probe chunk may match many build rows.

Local probe state stores enough continuation information to emit results across multiple output chunks:

```text
current probe row
current matching build-row handle
match state
LEFT JOIN matched flag
```

Do not require the full cross-product output for one probe chunk to fit in one DataChunk.

---

# 489. Hash Join Residual Predicate

## LOCKED

After equi-key match:

```text
candidate joined rows
    ↓
evaluate residual predicate
    ↓
emit survivors
```

Example:

```sql
A.id = B.id
AND A.ts < B.ts
```

uses hash key:

```text
A.id = B.id
```

and residual:

```text
A.ts < B.ts
```

Do not incorrectly include arbitrary non-equality predicates in hash-key equality.

---

# 490. LEFT Hash Join

## LOCKED

For each probe row:

```text
if one or more build matches survive residual:
    emit matches
else:
    emit one row with build-side outputs NULL
```

Output nullability must match logical-plan metadata.

A NULL probe hash key is immediately unmatched for ordinary equality.

---

# 491. Grace Hash Join Spilling

## LOCKED

When hash-join build cannot remain within its memory reservation, switch to partitioned Grace-style execution.

High-level:

```text
hash input
    ↓
partition on high hash bits
    ↓
spill build partitions
    ↓
partition probe side identically
    ↓
for each partition pair:
    load/build in memory
    probe
```

Choose partition count as a power of two based on estimated memory pressure.

---

# 492. Recursive Hash Repartition

## LOCKED

If a spilled build partition is still too large:

```text
repartition using additional hash bits
```

up to a bounded recursion depth.

If a pathological skew partition remains too large:

```text
fall back to a controlled alternative
```

such as chunked nested processing, rather than infinite repartition recursion.

Record skew statistics for profiling.

---

# 493. Hash Join Spill Ordering

## LOCKED

Hash join does not promise output ordering.

Therefore spilled partition processing may change physical output order.

This is semantically fine unless an explicit upstream/downstream sort property is required.

Physical planner must not claim hash join preserves input order.

---

# 494. Hash Aggregate

## LOCKED

Main grouped aggregation implementation:

```text
PhysicalHashAggregate
```

Inputs:

```text
group key expressions
aggregate function descriptors
aggregate argument expressions
```

No grouping keys means global aggregation and may use a specialized simpler state.

---

# 495. Aggregate Function State API

## LOCKED

Aggregate implementations expose conceptually:

```text
StateSize
StateAlignment

Initialize(state)
Update(state, vector inputs, selection)
Combine(target, source)
Finalize(state, output vector)
Destroy(state) if required
```

This supports:

```text
vectorized updates
thread-local partial states
parallel combine
spilling
```

Do not implement aggregates as one virtual callback per row.

---

# 496. Initial Aggregate Semantics

## LOCKED

### COUNT(*)

Counts all input rows.

Return:

```text
INT64 NOT NULL
```

### COUNT(expr)

Counts non-NULL values.

Return:

```text
INT64 NOT NULL
```

### SUM

Ignores NULL input.

No non-NULL inputs:

```text
NULL
```

Initial integer SUM should use a wider internal accumulator where practical to detect overflow before producing the declared result/error.

### MIN / MAX

Ignore NULL.

No non-NULL inputs:

```text
NULL
```

### AVG

Maintain:

```text
sum
count
```

Finalize according to the type system's declared AVG result semantics.

---

# 497. Group Hash Table

## LOCKED

Use a hash table conceptually similar to hash join:

```text
hash directory
    ↓
group row/state
```

Each group row stores:

```text
deep-copied group key
aggregate state block
optional cached hash
```

Equality uses the same SQL grouping equality semantics as the binder/executor contract.

---

# 498. GROUP BY NULL Semantics

## LOCKED

For GROUP BY:

```text
all NULL values for one grouping component belong to the same group
```

This differs from ordinary `=` join semantics.

Therefore group-key equality/hashing must support:

```text
NULL == NULL for grouping identity
```

without changing ordinary SQL comparison semantics.

The hash utility must make equality mode explicit.

---

# 499. Global Aggregate Fast Path

## LOCKED

For no GROUP BY:

```text
one aggregate state block
```

No hash table is required.

Process input vectors directly into aggregate states.

This is a simple but important specialization.

---

# 500. Hash Aggregate Spill

## LOCKED

When group state exceeds memory:

```text
partition rows/states by group hash
spill partitions
```

Initial robust approach:

1. partition raw/partially aggregated input,
2. finish one partition at a time,
3. combine equal groups,
4. emit final groups.

Later optimization may spill serialized partial aggregate states where every aggregate implementation supports safe serialization/combine.

---

# 501. DISTINCT

## LOCKED

`PhysicalDistinct` may reuse the group-hash infrastructure with:

```text
all output columns as grouping key
zero aggregates
```

For ordered input, later physical planning may use sort/streaming distinct.

Do not create a completely separate duplicate hash-table implementation unless measurements justify it.

---

# 502. Sort Operator

## LOCKED

`PhysicalSort` is a blocking operator.

It:

1. evaluates sort-key expressions,
2. materializes required output payload,
3. forms sortable records,
4. sorts in memory while within budget,
5. spills sorted runs when necessary,
6. merges runs,
7. emits DataChunks.

SQL sort is not required to be stable for rows equal on every ORDER BY key.

---

# 503. Sort Record

## LOCKED

A sort record conceptually contains:

```text
normalized key prefix
row handle / payload handle
```

Full key values remain available for tie-breaking comparisons.

Initial normalized prefix target:

```text
8–16 bytes
```

derived from leading sort keys where possible.

Purpose:

```text
reject most comparisons without chasing full variable-length keys
```

Do not require the entire composite SQL key to fit in the prefix.

---

# 504. Sort Comparison

## LOCKED

Comparison obeys exactly:

```text
per-key ASC/DESC
NULLS FIRST/LAST
SQL type ordering
binary VARCHAR collation
FLOAT64 ordering contract
```

Use normalized prefix first.

On prefix tie:

```text
compare full bound key values
```

The same ordering semantics must agree with B+ tree property matching where applicable.

---

# 505. In-Memory Sort Algorithm

## LOCKED initial implementation

Use a high-quality comparison sort over compact sort records, such as an introsort-style implementation/library algorithm with our comparator.

The expensive payload is not repeatedly moved.

Sort:

```text
small records/handles
```

then read payload through row handles.

Later high-reward experiment:

```text
radix sort / radix-prefix sort
```

for fixed-width normalized keys.

Do not block first correctness on a custom sorting algorithm.

---

# 506. External Merge Sort

## LOCKED

When memory budget fills:

```text
sort current in-memory run
    ↓
write sequential spill run
    ↓
reset run memory
```

After input ends:

```text
k-way merge sorted runs
```

Use buffered readers/writers.

Merge fan-in depends on:

```text
available memory
number of runs
```

If too many runs exist, perform multi-pass merge.

---

# 507. Sort Spill Run Format

## LOCKED

A spill run contains:

```text
run header
RowLayout/schema fingerprint
record count
sequential sorted records/payload
checksummed blocks
```

Runs are query-temporary and never WAL logged.

On query failure/cancel:

```text
SpillManager removes them
```

---

# 508. PhysicalTopN

## LOCKED

For:

```sql
ORDER BY ... LIMIT N
```

physical planner may use:

```text
PhysicalTopN
```

instead of full sort when beneficial.

Use a bounded heap containing at most:

```text
N + OFFSET
```

best records.

For very large N, full/external sort may still be better.

The cost model will decide later.

---

# 509. Merge Join

## LOCKED architecture, later implementation milestone

Implement merge join after hash/nested-loop paths are stable.

Useful when:

```text
both sides already ordered compatibly
range-like join opportunities
large equality joins where sort order is useful downstream
```

It must operate chunk-wise without materializing the full cross product of duplicate groups at once.

Physical optimizer may choose it only once implemented.

---

# 510. DML Target Materialization

## LOCKED

UPDATE and DELETE use a statement-level target spool before applying mutations.

Read phase:

```text
scan logical target relation
    ↓
apply WHERE / joins supported by DML
    ↓
materialize target RID
    +
required old column values
```

Write phase:

```text
iterate materialized targets
    ↓
acquire logical write locks
    ↓
revalidate
    ↓
perform MVCC mutation
```

This is a deliberate pipeline breaker.

---

# 511. Halloween Problem Protection

## LOCKED

The DML target spool prevents an UPDATE from rediscovering its own effects through:

- an index whose key is being changed,
- heap versions created by the statement,
- altered predicate values.

Do not depend on current-command MVCC visibility alone as the only Halloween protection.

A row selected as a target is represented once in the target spool.

---

# 512. DML Target Spool Memory and Spill

## LOCKED

Small statements keep target rows in a RowCollection.

Large target sets spill through SpillManager.

Spool row layout contains at least:

```text
RID
columns needed for assignments
columns needed for unique keys
columns needed for RETURNING
```

Do not materialize every table column if not required.

---

# 513. DML Revalidation

## LOCKED

Materialization does not eliminate concurrency races.

Before UPDATE/DELETE of each spooled RID:

```text
acquire TUPLE_WRITE
re-fetch RID
revalidate tuple header/version
apply READ COMMITTED retry or REPEATABLE READ conflict rules
```

If READ COMMITTED requires complete statement restart:

```text
discard target spool
discard buffered RETURNING
rebuild from fresh statement snapshot
```

---

# 514. INSERT Execution

## LOCKED

`PhysicalInsert` is a sink consuming input rows.

For each input chunk:

1. evaluate/convert target column vectors,
2. validate runtime NOT NULL constraints,
3. acquire unique-key locks in deterministic order,
4. perform uniqueness checks,
5. encode heap tuple versions,
6. insert through transactional heap path,
7. install B+ tree entries,
8. append RETURNING rows to retry-safe result buffer when requested.

Bulk insert path should amortize:

```text
tuple encoding
FSM lookup
page latching
index key encoding
```

across chunks where safe.

---

# 515. UPDATE Execution

## LOCKED

After target-spool materialization:

1. process targets in batches,
2. acquire/revalidate one target's write semantics,
3. vector-evaluate assignment expressions over batches where possible,
4. create complete new tuple versions,
5. update old tuple xmax/cmax,
6. insert new index entries,
7. buffer RETURNING.

Because locks may wait/retry per row, the implementation may need to break vector work around conflict points.

Correctness wins over artificial vectorization of the lock protocol.

---

# 516. DELETE Execution

## LOCKED

After target spool:

```text
for target:
    acquire/revalidate write lock
    set xmax/cmax transactionally
    buffer RETURNING if requested
```

Do not physically remove secondary-index entries here.

Vacuum owns that work.

---

# 517. RETURNING Buffer

## LOCKED

DML `RETURNING` output is buffered until the statement crosses the point where an internal READ COMMITTED restart can no longer occur.

For small results:

```text
memory RowCollection
```

For large results:

```text
spill-capable result spool
```

Only then stream chunks to the client/result consumer.

This avoids emitting rows from a statement execution that later restarts.

---

# 518. Query Result Interface

## LOCKED

Execution exposes result chunks to the client/API through a result sink/cursor abstraction.

Do not expose internal borrowed DataChunks after their lifetime ends.

Client-visible result chunks:

```text
own or safely retain their result memory
```

The first CLI may simply format rows immediately as it consumes result chunks.

---

# 519. Physical DDL Execution

## LOCKED

DDL physical operators are not vector hot paths.

They call catalog/storage management under the transactional/SchemaLock rules already specified.

Examples:

```text
PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
```

`CREATE INDEX` executes an offline scan/build pipeline and publishes catalog metadata only when build success is transactionally established.

---

# 520. Physical VACUUM

## LOCKED

VACUUM is a maintenance operator invoking the VacuumManager.

It may expose progress chunks/debug output later but is not treated as ordinary relational data processing internally.

Do not force vacuum logic through hash/project operator APIs merely for uniformity.

---

# 521. Physical Operator Properties

## LOCKED

Physical nodes may expose properties such as:

```text
output ordering
unique keys
partitioning
rewindability
blocking/streaming nature
estimated cardinality
estimated memory
```

These properties are inputs to physical optimization and pipeline construction.

Do not infer them later from operator names by string matching.

---

# 522. Ordering Property

## LOCKED

Represent ordering as ordered key descriptors:

```text
slot/expression
ASC/DESC
NULLS FIRST/LAST
collation
```

Examples:

```text
SeqScan:
    no guaranteed SQL ordering

IndexScan:
    index-prefix ordering

HashJoin:
    no guaranteed ordering

Sort:
    explicit produced ordering
```

Physical planner can avoid redundant sorts when child properties satisfy requirements.

---

# 523. Pipeline Breaker Memory Semantics

## LOCKED

Every pipeline breaker must declare:

```text
memory-resident state
spillability
Finalize transition
output source state after finalize
```

Examples:

```text
HashJoin:
    build state -> finalized hash table

Sort:
    input rows -> sorted runs -> merge source

Aggregate:
    group table -> finalized group source
```

This makes blocking behavior explicit rather than hidden inside `GetNext()` calls.

---

# 524. Pipeline Dependency DAG

## LOCKED

Dependencies include:

```text
hash join probe waits for build finalize
sort output waits for sort input finalize
aggregate output waits for aggregation finalize
DML write waits for target spool finalize
```

A pipeline executes only when its dependencies are satisfied.

This DAG becomes the basis for later parallel scheduling.

---

# 525. Worker Model

## LOCKED architecture

Database execution uses a fixed worker pool rather than creating arbitrary OS threads per query.

Initial single-query implementation may schedule all work onto one worker.

Later:

```text
multiple ready pipeline tasks
    ↓
worker pool
```

Number of workers is configuration.

---

# 526. Pipeline Morsels

## LOCKED for parallel-ready sources

A parallel source should be divisible into independent work units:

```text
morsels
```

Examples:

### Heap scan

```text
contiguous page ranges
```

Initial target morsel:

```text
~64–256 heap pages
```

tunable.

### Values

row ranges.

### Spill partitions

partition/run ranges.

B+ tree ordered range scans are less freely partitionable initially and may remain single-source until explicit range partitioning exists.

---

# 527. Local Operator State

## LOCKED

Each worker gets its own local state for hot mutable data:

```text
expression scratch
output chunks
local aggregate table
local sort run
source cursor
profiling counters
```

Avoid unnecessary synchronization on one shared state object for every chunk.

Combine/finalize at pipeline boundaries.

---

# 528. Parallel Sequential Scan

## LOCKED later milestone

Parallel scan workers claim heap page-range morsels.

Each worker:

```text
reads pages through shared BufferPool
uses same transaction snapshot
produces independent chunks
```

Without ORDER BY, output interleaving is semantically irrelevant.

If ordered output is required, a later Sort/order-preserving operator handles it.

---

# 529. Parallel Hash Join Build

## LOCKED architecture

Do not make every build-row insertion contend on one resizable global hash table.

Preferred architecture:

1. workers append build rows into local RowCollections,
2. compute hashes,
3. optionally partition by hash,
4. after build input completes, construct/finalize directory partitions in parallel,
5. probe workers read finalized immutable hash partitions.

This exploits the fact that hash build is a blocking phase.

---

# 530. Parallel Hash Join Probe

## LOCKED architecture

After build finalization, hash table is read-only.

Probe morsels/chunks may run concurrently without hash-directory latches.

Each worker owns:

```text
probe state
output chunk
local counters
```

This is one reason immutable-after-finalize build state is preferred.

---

# 531. Parallel Hash Aggregate

## LOCKED architecture

Preferred first parallel design:

```text
worker-local group hash tables
    ↓
partitioned combine/finalize
    ↓
global result
```

Avoid a lock on every aggregate update.

For low-cardinality global aggregation, specialized combine may use one local state block per worker.

---

# 532. Parallel Sort

## LOCKED architecture

Workers generate sorted local runs independently.

Finalize:

```text
parallel run generation
    ↓
merge tree / k-way merge
```

No shared comparison-sort lock.

External spilled runs naturally integrate with the same merge architecture.

---

# 533. Task Scheduler

## LOCKED staged implementation

First parallel scheduler may use:

```text
fixed worker pool
global concurrent ready-task queue
dependency counters
```

This is sufficient to learn pipeline parallelism.

Later, if profiling shows scheduling contention:

```text
per-worker work-stealing deques
```

are a high-value optimization.

Do not start the project by implementing a lock-free work-stealing runtime.

---

# 534. Query Fairness

## LOCKED architecture

A long query should not monopolize one worker indefinitely without cancellation/fairness points.

Task/morsel sizing provides natural yield boundaries.

Future multi-query scheduling may limit:

```text
active tasks per query
memory per query
```

Initial scheduler can remain simple while preserving these control points.

---

# 535. NUMA

## DEFERRED

Do not build NUMA-aware:

```text
buffer pools
hash partitions
worker affinity
memory placement
```

before the engine is correct and parallel execution is measured on ordinary multi-core systems.

Keep large allocation APIs centralized so NUMA policy can later be inserted.

---

# 536. SIMD Policy

## LOCKED architecture-compatible

Write vector kernels with:

```text
contiguous typed buffers
simple loops
selection vectors
all-valid fast paths
```

so modern compilers can auto-vectorize.

Initial implementation may use portable C++ loops.

Later benchmark:

```text
explicit AVX2/AVX-512/NEON kernels
```

for:

- comparisons,
- arithmetic,
- validity processing,
- hashing.

Do not hand-write SIMD for every expression before profiling.

---

# 537. Branch Reduction

## LOCKED guideline

Hot loops should prefer:

```text
batch-level representation/type checks
specialized NULL/no-NULL kernels
selection vectors
compact state machines
```

over:

```text
switch(type) per row
virtual call per row
std::variant visit per cell
```

Use profiles to verify improvements.

---

# 538. Prefetch

## LOCKED future optimization points

Architecture should permit prefetching:

```text
next sequential heap pages
next B+ tree leaf
hash-table probe buckets
spill merge blocks
```

Initial implementation relies on normal BufferPool/OS behavior.

Add explicit prefetch only after evidence.

---

# 539. Query Profiling

## LOCKED

Every physical operator records:

```text
input rows
output rows
chunks
wall time
CPU time where available
memory peak
spill bytes
```

Operator-specific metrics:

### SeqScan

```text
heap pages visited
visible tuples
tuples rejected by MVCC
predicate rejects
```

### IndexScan

```text
index entries visited
heap RIDs fetched
invisible RIDs
```

### HashJoin

```text
build rows
probe rows
matches
hash collisions
directory load factor
spill partitions
spill bytes
```

### Aggregate

```text
input rows
groups
hash collisions
spill bytes
```

### Sort

```text
rows
comparisons
runs
spill bytes
merge passes
```

---

# 540. EXPLAIN ANALYZE

## LOCKED

`EXPLAIN ANALYZE` executes the query and reports the physical plan with:

```text
estimated rows
actual rows
operator time
memory
spilling
important operator counters
```

Example:

```text
HashJoin [u.id = o.user_id]
  estimated_rows=12000
  actual_rows=18743
  build_rows=1000
  probe_rows=95000
  memory=4.2MiB
  time=18.4ms
```

This is essential for debugging both execution and the future optimizer.

---

# 541. Pipeline Profiling

## LOCKED

Also record pipeline-level:

```text
pipeline ID
dependency wait time
execution time
chunks processed
worker count
morsels
```

This helps distinguish:

```text
operator CPU bottleneck
```

from:

```text
waiting for a blocking dependency
```

or:

```text
scheduler imbalance
```

---

# 542. Physical Plan Validator

## LOCKED

Before execution, validate:

```text
child/output slot compatibility
physical expression types
required hidden RID presence
join key type compatibility
ordering property consistency
operator support for requested join type
sink/source pipeline legality
memory-spill capability flags
transaction context requirements
```

A malformed physical plan should fail before mutating data.

---

# 543. Execution Error Model

## LOCKED

Distinguish:

```text
ExecutionError
OutOfMemory
SpillIOError
QueryCancelled
CardinalityViolation
CastError
ArithmeticError
ConstraintViolation
TransactionConflict
```

RAII must unwind temporary query resources.

DML errors abort or mark the surrounding transaction according to SQL transaction policy chosen by the command layer.

Do not leave partially active runtime pipelines after an exception/error.

---

# 544. Integer Arithmetic Errors

## LOCKED

Do not silently invoke C++ signed-overflow undefined behavior.

Numeric kernels must use checked arithmetic where SQL semantics require overflow errors.

Potential later physical fast path:

```text
compiler intrinsics
vectorized overflow detection
```

Correctness comes first.

FLOAT64 uses the database's established IEEE semantics.

---

# 545. Division

## LOCKED

Integer division by zero:

```text
SQL execution error
```

FLOAT64 division semantics should be explicitly consistent with the chosen SQL/type contract; do not accidentally depend on compiler flags that change behavior.

Vector kernels must report row-level failure as a query error, not continue with corrupted output.

---

# 546. Execution Testing Strategy

## LOCKED

### Operator unit tests

Feed synthetic DataChunks directly into:

```text
Filter
Project
HashJoin
Aggregate
Sort
Limit
```

without SQL parser/storage.

### Pipeline tests

Construct physical plans manually and validate chunk flow/dependencies.

### End-to-end tests

SQL -> storage results.

### Differential tests

Compare supported SQL results against a reference DB where semantics align.

### Spill tests

Use tiny memory budgets to force:

```text
hash join spill
aggregate spill
external sort
DML spool spill
```

### Cancellation tests

Cancel during:

```text
scan
join build
sort spill
merge
DML spool
```

and verify cleanup.

---

# 547. Vector Correctness Tests

## LOCKED

Test every kernel across:

```text
FLAT
CONSTANT
DICTIONARY
```

inputs and combinations.

Include:

```text
all-valid
all-NULL
mixed validity
non-identity selection
nested dictionary normalization
empty chunk
full 1024-row chunk
```

Results must be representation independent.

---

# 548. String Lifetime Tests

## LOCKED

Create tests that deliberately:

1. scan VARCHAR data,
2. release/unpin source pages,
3. recycle source chunks,
4. continue downstream blocking processing,
5. verify strings remain valid.

Use allocator poisoning/debug memory where practical to catch accidental borrowed-pointer retention.

---

# 549. Hash Join Tests

## LOCKED

Cover:

```text
no matches
one-to-one
one-to-many
many-to-many duplicates
NULL keys
composite keys
hash collisions
residual predicates
LEFT JOIN unmatched rows
output larger than one chunk per probe row
tiny memory forced Grace spill
skew partition
```

Compare to nested-loop join results on randomized small inputs.

---

# 550. Aggregate Tests

## LOCKED

Cover:

```text
empty input
global aggregate
one group
many groups
NULL group keys
all-NULL aggregate input
composite group keys
VARCHAR keys
forced spill
partial combine
COUNT/SUM/MIN/MAX/AVG
```

Randomized results should compare against a simple reference implementation.

---

# 551. Sort Tests

## LOCKED

Cover:

```text
ascending
descending
NULLS FIRST
NULLS LAST
multiple sort keys
equal keys
VARCHAR
FLOAT64 edge cases
empty input
one row
forced external runs
multi-pass merge
```

Compare output ordering against the semantic comparator, not raw bytes.

---

# 552. DML Execution Tests

## LOCKED

Specifically test Halloween protection.

Example:

```sql
CREATE INDEX idx_x ON t(x);
UPDATE t SET x = x + 1 WHERE x < 100;
```

Run through an index access path and verify each qualifying logical target is updated once.

Also test:

```text
target-spool spill
concurrent update revalidation
READ COMMITTED statement retry
REPEATABLE READ conflict abort
RETURNING buffering
unique-key update
```

---

# 553. Execution Microbenchmarks

## LOCKED

Measure:

```text
scan rows/sec
scan bytes/sec
filter rows/sec
projection arithmetic rows/sec
VARCHAR comparison rows/sec

hash build rows/sec
hash probe rows/sec
hash join output rows/sec

aggregate input rows/sec
groups/sec

in-memory sort rows/sec
external sort MB/sec

chunk allocations/query
general allocator calls/query
temporary bytes/query
```

Benchmark with NULL-free and NULL-heavy data.

---

# 554. Vector Size Benchmark

## LOCKED

Benchmark at least:

```text
256
512
1024
2048
4096
```

rows/chunk on representative workloads.

The architecture default remains 1024 until measurements justify changing it.

Do not assume a larger vector is always faster; cache footprint and branch behavior matter.

---

# 555. End-to-End Execution Benchmarks

## LOCKED

Create repeatable workloads for:

```text
point lookup
selective index range
full table scan
filter + projection
small join
large hash join
group aggregate
ORDER BY
ORDER BY LIMIT
UPDATE by index
bulk INSERT
concurrent read/write
```

Track:

```text
latency
throughput
CPU
memory peak
buffer hits/misses
WAL bytes
spill bytes
```

---

# 556. Deliberately Deferred Execution Features

## LOCKED

Do not implement before the baseline vectorized engine is correct and benchmarked:

```text
JIT query compilation
LLVM code generation
GPU execution
adaptive query execution
runtime join reordering
vector compression
RLE vectors
late-materialization engine-wide redesign
radix hash join
radix sort everywhere
SIMD intrinsics for every kernel
NUMA scheduling
lock-free task scheduler
parallel B+ tree ordered scan partitioning
distributed exchange operators
remote spilling
```

These remain future experiments.

---

# 557. High-Reward Future Execution Experiments

## LOCKED recommendation

After baseline profiling, especially valuable projects are:

```text
SIMD filter/comparison kernels
radix-partitioned hash join
radix sort / normalized-key sort
Bloom filters pushed from hash join build to probe scans
selection-vector specialization
string prefix/inlining optimization
parallel pipelines
work stealing
prefetch
adaptive spill partition sizing
late materialization for selected workloads
JIT expression compilation
```

Choose based on actual profiles.

---

# 558. Practical Performance Rules

## LOCKED

Codex must treat the following as strong implementation rules:

1. No one heap allocation per execution row.
2. No one heap allocation per execution cell.
3. No virtual dispatch per row.
4. No generic `Value` construction per hot-loop cell.
5. No SQL type switch per row when batch-level specialization is possible.
6. Do not keep heap pages pinned merely to preserve VARCHAR execution references.
7. Blocking operators deep-copy retained varlen data.
8. Large operator memory is tracked by QueryMemoryManager.
9. Spill writes/reads are large and sequential where possible.
10. Reuse DataChunks and vector buffers.
11. Decode only required scan columns.
12. Hash tables use compact contiguous storage where practical.
13. Query-wide correctness is preserved under tiny memory budgets and forced spilling.
14. Operators expose metrics before aggressive optimization.
15. Profile before replacing simple correct algorithms with complex ones.

---

# 559. Execution Invariants

## LOCKED

Codex must preserve:

1. Every DataChunk column has the same logical cardinality.
2. A chunk never exposes pointers into an unpinned heap page.
3. A borrowed Vector never outlives its owner.
4. Blocking operators own/deep-copy retained input data.
5. NULL validity semantics are representation independent.
6. Dictionary chains are normalized before pathological indirection develops.
7. Physical SeqScan does not imply SQL result ordering.
8. IndexScan still performs heap MVCC visibility checks.
9. HashJoin ordinary equality never matches NULL to NULL.
10. GROUP BY treats NULL grouping values as belonging to one group.
11. Hash/equality semantics agree.
12. HashJoin build state is immutable before parallel probe begins.
13. A probe row may emit across multiple output chunks without losing duplicate matches.
14. HashJoin spill preserves join semantics despite partition processing order.
15. Aggregate spill produces the same groups/results as in-memory aggregation.
16. External sort exactly matches in-memory comparator semantics.
17. DML target rows are materialized before mutation.
18. DML revalidates targets after logical lock acquisition.
19. DML RETURNING is not externally emitted before retry safety.
20. Query temp spill files are never WAL logged.
21. Query cancellation releases temp/memory/runtime resources.
22. Physical-plan validation occurs before data-changing execution.
23. Pipeline dependencies prevent consumers from observing unfinalized blocking state.
24. Local worker state is separate from immutable plan/global state.
25. Parallel execution of a query uses the same transaction/snapshot semantics as single-thread execution.
26. EXPLAIN ANALYZE counters reflect actual operator execution rather than estimates.
27. Integer kernels do not rely on undefined signed overflow.
28. Query memory exhaustion becomes controlled spill/error behavior, not process corruption.
29. The executor consumes resolved IDs/slots and never resolves SQL names.
30. Physical operators implement algorithms, not relational semantic reinterpretation.

---

# 560. Recommended Execution Module Layout

## LOCKED

```text
src/
  execution/
    chunk/
      data_chunk.h
      vector.h
      flat_vector.h
      constant_vector.h
      dictionary_vector.h
      validity_mask.h
      selection_vector.h
      unified_vector_format.h
      string_ref.h
      string_heap.h

    row/
      row_layout.h
      row_collection.h

    memory/
      query_memory_manager.h
      query_memory_manager.cpp
      query_arena.h
      spill_manager.h
      spill_manager.cpp

    expression/
      expression_executor.h
      arithmetic_kernels.h
      comparison_kernels.h
      boolean_kernels.h
      cast_kernels.h
      hash_kernels.h

    physical/
      physical_operator.h
      physical_scan.h
      physical_filter.h
      physical_project.h
      physical_join.h
      physical_aggregate.h
      physical_sort.h
      physical_limit.h
      physical_dml.h
      physical_ddl.h

    pipeline/
      pipeline.h
      pipeline_builder.h
      pipeline_executor.h
      source.h
      sink.h
      task_scheduler.h
      worker_pool.h

    join/
      hash_table.h
      hash_join.h
      grace_hash_join.h
      nested_loop_join.h
      index_nested_loop_join.h

    aggregate/
      aggregate_function.h
      group_hash_table.h
      hash_aggregate.h

    sort/
      sort_key.h
      sort_run.h
      external_merge_sort.h
      top_n.h

    dml/
      target_spool.h
      returning_spool.h

    profiling/
      operator_profiler.h
      query_profile.h
      explain_analyze.h
```

Exact filenames may evolve.

The architectural responsibilities may not collapse into one giant executor class.

---

# 561. Execution Implementation Order

## LOCKED

### Phase E1 — Vector foundation

1. ValidityMask
2. SelectionVector
3. FLAT vectors
4. CONSTANT vectors
5. DICTIONARY vectors
6. UnifiedVectorFormat
7. StringRef/StringHeap
8. DataChunk reuse
9. vector correctness tests

### Phase E2 — Expression execution

10. constants/column refs
11. arithmetic
12. comparisons
13. casts
14. Boolean/3VL
15. selection-based filter evaluation
16. vectorized hashing

### Phase E3 — Pipeline skeleton

17. source/operator/sink interfaces
18. pipeline builder
19. single-worker pipeline executor
20. cancellation
21. physical plan validator
22. profiling counters

### Phase E4 — Streaming operators

23. Values
24. SeqScan
25. IndexScan
26. Filter
27. Project
28. Limit
29. ResultSink

### Phase E5 — Blocking memory infrastructure

30. RowLayout
31. RowCollection
32. QueryMemoryManager
33. MemoryReservation
34. SpillManager
35. temporary block serialization

### Phase E6 — Joins

36. NestedLoopJoin
37. HashJoin in memory
38. LEFT HashJoin
39. residual predicates
40. IndexNestedLoopJoin
41. Grace spill
42. skew handling

### Phase E7 — Aggregation/distinct

43. aggregate state API
44. global aggregate
45. group hash table
46. grouped hash aggregate
47. DISTINCT reuse
48. aggregate spill

### Phase E8 — Sorting

49. sort comparator
50. normalized prefix
51. in-memory sort
52. external sorted runs
53. k-way merge
54. TopN

### Phase E9 — DML execution

55. target spool
56. Halloween-safe UPDATE
57. DELETE
58. INSERT sink
59. spillable target spool
60. buffered RETURNING
61. statement-retry integration

### Phase E10 — Parallel execution

62. fixed worker pool
63. morselized SeqScan
64. dependency scheduler
65. parallel hash build/probe
66. local aggregate + combine
67. parallel sort runs
68. parallel profiling

Do not begin JIT or exotic SIMD work before these phases are correct.

---

# 562. Execution Milestone 1

## LOCKED target

End-to-end:

```text
SQL
  ↓
logical plan
  ↓
manually/simple-selected physical plan
  ↓
pipeline
  ↓
SeqScan / Filter / Project / Limit
  ↓
result chunks
```

for transactional heap data.

No hash join/spill required yet.

---

# 563. Execution Milestone 2

## LOCKED target

Add:

```text
IndexScan
NestedLoopJoin
HashJoin
global/group aggregation
DISTINCT
Sort
TopN
```

All work single-threaded but vectorized.

`EXPLAIN ANALYZE` must show real row counts/timing.

---

# 564. Execution Milestone 3

## LOCKED target

Run with intentionally tiny query memory limits and correctly spill:

```text
HashJoin
HashAggregate
Sort
DML target spool
```

Results must match unlimited-memory execution.

---

# 565. Execution Milestone 4

## LOCKED target

DML is fully integrated:

```text
Halloween-safe UPDATE
DELETE
bulk INSERT
unique constraints
MVCC conflict/retry
RETURNING
```

with crash-safe lower-layer transactional behavior.

---

# 566. Execution Milestone 5

## LOCKED target

Parallelize:

```text
SeqScan
HashJoin
HashAggregate
Sort
```

through the same pipeline architecture.

Benchmark scaling:

```text
1
2
4
8
...
workers
```

and identify synchronization/memory-bandwidth limits.

---

# 567. Architecture Status After Practical Execution Layer

## LOCKED v1

The architecture now specifies the practical path from persistent bytes to executed relational queries:

```text
disk files
  ↓
buffer pages
  ↓
heap / B+ tree
  ↓
MVCC / WAL / recovery
  ↓
catalog
  ↓
SQL parser / binder
  ↓
logical relational algebra
  ↓
physical operators
  ↓
pipeline graph
  ↓
vectorized DataChunks
  ↓
hash join / aggregate / sort
  ↓
spill / query memory
  ↓
result chunks
```

The final major architectural block is now the **cost-based physical optimizer**:

```text
statistics collection
cardinality estimation
selectivity
cost units
access-path enumeration
physical-property propagation
join-order dynamic programming
join algorithm selection
sort avoidance
memory/spill costing
plan memoization/search
EXPLAIN estimate-vs-actual feedback
```

After that block is locked, the core engine architecture is complete enough to begin implementation phase-by-phase without major design gaps.

---

# 568. Cost-Based Optimizer Contract

## LOCKED

This section defines the cost-based physical optimizer.

The optimizer's job is:

```text
typed logical relational plan
        ↓
semantic rewrites
        ↓
statistics + cardinality estimation
        ↓
physical alternative enumeration
        ↓
costing
        ↓
join-order search
        ↓
physical-property reasoning
        ↓
lowest-cost valid physical plan
```

The selected architecture is:

```text
System-R-style bottom-up dynamic programming
+
small memo keyed by logical relation set / physical properties
+
rule-based logical normalization
+
cost-based physical selection
```

Do **not** begin with a full Cascades/Volcano optimizer framework.

Reason:

- System-R-style optimization exposes the core database ideas directly,
- the implementation remains understandable end-to-end,
- it is powerful enough for serious multi-join queries,
- a later Cascades rewrite becomes a meaningful advanced project rather than framework overhead before basic costing is understood.

---

# 569. Optimizer Layering

## LOCKED

Optimization stages:

```text
Bound / Logical Plan
        ↓
Logical Normalization
        ↓
Predicate Analysis
        ↓
Required-Column Analysis
        ↓
Statistics Lookup
        ↓
Cardinality Estimation
        ↓
Base Access-Path Enumeration
        ↓
Join Graph Construction
        ↓
Join-Order Enumeration
        ↓
Physical Operator Selection
        ↓
Physical Property Enforcement
        ↓
Final Physical Plan
        ↓
Pipeline Builder
```

The optimizer does not execute queries.

The executor does not make cost-based plan choices at runtime in v1.

---

# 570. Optimizer Inputs

## LOCKED

The optimizer consumes:

```text
immutable logical plan
catalog descriptors
table/index metadata
statistics snapshot
query memory budget
execution configuration
required final physical properties
```

Examples of required final properties:

```text
ORDER BY
required output columns
possibly rewindability later
```

The optimizer must not query mutable page contents to make ordinary planning decisions.

---

# 571. Optimizer Output

## LOCKED

Output is one immutable:

```text
PhysicalPlan
```

containing:

```text
chosen physical operators
child relationships
chosen access paths
chosen join order
chosen join algorithms
estimated rows at every node
estimated row width
estimated cost components
estimated memory
estimated spill behavior
physical properties
```

The executor receives this already-decided plan.

---

# 572. Statistics Philosophy

## LOCKED

Statistics are:

```text
approximate performance metadata
```

not correctness metadata.

A bad estimate may produce a slow plan.

It must never produce an incorrect query result.

Statistics may therefore be:

```text
stale
sampled
approximate
rebuildable
```

without entering WAL-critical correctness paths.

---

# 573. ANALYZE

## LOCKED

Introduce:

```sql
ANALYZE table_name;
```

and eventually:

```sql
ANALYZE;
```

for all tables.

Initial implementation is explicit/manual.

Automatic analyze is deferred until optimizer/executor correctness is established.

`VACUUM` and `ANALYZE` remain distinct concepts even if a later maintenance command combines them.

---

# 574. Table Statistics

## LOCKED

For each table collect at least:

```text
live_row_count
physical_heap_pages
average_logical_row_width
average_stored_tuple_width
dead_version_estimate
last_analyze_txn / generation
stats_version
```

Useful derived value:

```text
live_rows_per_heap_page
```

The optimizer primarily costs SQL-visible live rows but also uses physical page count for scan I/O.

---

# 575. Column Statistics

## LOCKED

For each base column collect:

```text
null_fraction
NDV estimate
min value
max value
MCV list
MCV frequencies
equi-depth histogram
average width for VARCHAR
maximum observed width
```

`NDV` means:

```text
number of distinct non-NULL values
```

MCV means:

```text
most common values
```

---

# 576. Statistics Collection Strategy

## LOCKED

`ANALYZE` performs a vectorized full heap scan in v1.

This is intentionally chosen over a complicated page sampler initially.

During one scan collect:

```text
exact live-row count
exact NULL count
exact min/max
approximate NDV
approximate most-common values
bounded distribution sample
width statistics
```

The full scan teaches how real statistics are derived while avoiding unbounded exact frequency maps.

Later large-table sampling is a performance optimization.

---

# 577. Small-Table Exact Statistics

## LOCKED

For tables with at most approximately:

```text
50,000 live rows
```

the analyzer may collect exact:

```text
NDV
value frequencies
```

when the bounded memory budget permits.

For larger tables use streaming approximate structures.

The threshold is configurable.

---

# 578. NDV Estimation

## LOCKED: HyperLogLog

For larger columns use a HyperLogLog-style sketch.

Initial precision:

```text
p = 14
```

giving:

```text
16,384 registers
```

per actively analyzed column when using the full sketch.

Hash the same canonical SQL value representation used by query hashing semantics.

NULL values are excluded from NDV.

Persist:

```text
estimated NDV
```

not necessarily the entire HLL sketch in v1.

A later incremental statistics system may persist sketches.

---

# 579. Most-Common-Value Collection

## LOCKED

Use a bounded heavy-hitter algorithm such as:

```text
SpaceSaving
```

for large inputs.

Initial target:

```text
64 MCV entries/column
```

After candidate collection, a second pass over the bounded reservoir/sample or exact small-table counts may refine frequencies where practical.

Persist:

```text
value
estimated frequency fraction
```

in descending frequency order.

Do not build an unbounded `unordered_map<Value,count>` for a billion-row column.

---

# 580. Histogram Collection

## LOCKED

Maintain a bounded reservoir sample of non-NULL column values.

Initial target sample size:

```text
100,000 values/column
```

for analyzed columns, bounded by query/maintenance memory.

After scan:

1. sort sample using SQL column ordering,
2. remove/discount values represented by MCVs where practical,
3. build approximately:
   ```text
   100 equi-depth histogram bins
   ```

Each bin stores enough boundary information to estimate range selectivity.

---

# 581. Why Equi-Depth Histograms

## LOCKED rationale

Equi-depth histograms attempt to place approximately equal row mass in each bin.

Compared with equal-width numeric bins, they handle skew much better.

This is especially important for data such as:

```text
salary
dates
IDs with hotspots
VARCHAR prefixes
```

without requiring an enormous number of buckets.

---

# 582. Histogram Value Semantics

## LOCKED

Histogram ordering uses the same logical type ordering as:

```text
executor comparisons
B+ tree user-key ordering
```

for the supported collation/type mode.

Do not compare VARCHAR histogram boundaries using a different locale/collation than SQL execution.

FLOAT64 canonicalization/order must remain consistent.

---

# 583. Statistics Serialization

## LOCKED

Statistics are stored in:

```text
sys_statistics
```

using an explicitly versioned payload.

Do not serialize arbitrary C++ object graphs.

The payload must support:

```text
type/version tag
row/page counts
column summary
MCV array
histogram boundaries
```

A statistics version mismatch may invalidate/rebuild stats rather than corrupt the database.

---

# 584. Statistics Snapshot During Planning

## LOCKED

One optimization invocation uses a stable immutable statistics snapshot.

Do not let half of a plan use old statistics while another half observes a concurrently installed ANALYZE result.

Catalog cache publishes a new immutable statistics descriptor atomically.

Queries already optimizing may finish with the previous descriptor.

---

# 585. Statistics Freshness

## LOCKED

Track table modification counters:

```text
rows inserted
rows updated
rows deleted
```

since last ANALYZE.

Expose a staleness indicator such as:

```text
changed_rows / max(1, analyzed_live_rows)
```

The optimizer may include staleness in diagnostics.

v1 does not automatically refuse stale statistics.

Later autovacuum/analyze can trigger on thresholds.

---

# 586. CardinalityEstimate

## LOCKED

Represent estimated row counts using:

```text
double
```

or another wide floating estimate type.

Do not round every intermediate estimate to integer rows.

Maintain:

```text
estimated_rows >= 0
```

and cap pathological arithmetic at a very large finite maximum.

The final executor still processes integer rows.

---

# 587. Exact-Zero vs Estimated-Small

## LOCKED

Distinguish:

```text
provably empty
```

from:

```text
estimated close to zero
```

Examples of provably empty:

```text
WHERE FALSE
x < 5 AND x > 10 after proven normalization
LIMIT 0
```

For a non-provably-empty relation:

```text
minimum working estimate ≈ 1 row
```

is allowed to avoid pathological downstream zero-cost plans.

Keep an explicit:

```text
is_provably_empty
```

property rather than overloading row count.

---

# 588. Row Width Estimate

## LOCKED

Every logical/physical node carries estimated average output width.

For fixed types use execution/stored width as appropriate.

For VARCHAR use column:

```text
average_width
```

plus vector/row-layout overhead relevant to the operator being costed.

Width affects:

```text
memory
hash-table size
sort size
spill probability
I/O of temporary runs
```

Do not cost all rows as if every tuple were the same width.

---

# 589. Selectivity Range

## LOCKED

Every selectivity estimate is clamped to:

```text
0.0 <= selectivity <= 1.0
```

Estimated rows:

```text
input_rows * selectivity
```

then apply exact-zero/minimum-nonempty rules.

---

# 590. Equality Selectivity: Constant

## LOCKED

For:

```sql
column = constant
```

estimate in order:

### NULL constant

Ordinary equality with NULL:

```text
selectivity = 0
```

because result is UNKNOWN, not TRUE.

### Constant found in MCV list

Use:

```text
MCV frequency
```

### Outside known min/max

For orderable exact stats:

```text
selectivity = 0
```

when impossible.

### Otherwise

Estimate remaining non-NULL, non-MCV mass as:

```text
remaining_mass
/
max(1, NDV - MCV_distinct_count)
```

This is far better than a universal `1 / NDV` rule on skewed columns.

---

# 591. Equality Selectivity: Column to Column

## LOCKED initial estimate

For an equijoin:

```text
A.x = B.y
```

baseline:

```text
join_rows =
    rows_A
    *
    rows_B
    *
    nonnull_A
    *
    nonnull_B
    /
    max(NDV_A, NDV_B)
```

Then improve using:

```text
uniqueness constraints
primary/foreign-key metadata when available later
overlapping MCVs
min/max disjointness
```

If min/max ranges are provably disjoint:

```text
join_rows = 0
```

for compatible ordered types.

---

# 592. MCV-Aware Join Estimation

## LOCKED

When both sides have MCV lists:

1. match equal MCV values,
2. estimate exact/approximate contribution:
   ```text
   rows_A * freq_A(v) * rows_B * freq_B(v)
   ```
3. remove this probability mass,
4. estimate the remaining values using residual NDVs.

This avoids severe underestimation for hot join keys.

---

# 593. Unique-Key Join Estimation

## LOCKED

If join equality targets a proven unique non-NULL key on side `U`:

```text
each row from other side matches at most one U row
```

Estimated cardinality is bounded by:

```text
rows_other_nonnull
```

subject to domain overlap.

If metadata later identifies a trusted foreign key:

```text
matching rows may approach rows_foreign_side
```

Foreign keys are not implemented in SQL v1, so this refinement is future-compatible.

---

# 594. Range Selectivity

## LOCKED

For:

```text
column < constant
column <= constant
column > constant
column >= constant
BETWEEN
```

use:

```text
MCV contributions
+
histogram interpolation
```

For a constant inside one histogram bin, interpolate within the bin when the type supports meaningful order interpolation.

For VARCHAR/binary strings, interpolation may initially treat the bin's mass uniformly by rank rather than numeric distance.

---

# 595. IS NULL Selectivity

## LOCKED

```sql
column IS NULL
```

uses:

```text
null_fraction
```

and:

```sql
column IS NOT NULL
```

uses:

```text
1 - null_fraction
```

If column is constrained NOT NULL:

```text
IS NULL -> provably empty
IS NOT NULL -> selectivity 1
```

---

# 596. IN-List Selectivity

## LOCKED

For:

```sql
column IN (c1,c2,...)
```

after constant deduplication:

```text
sum equality selectivities
```

clamped to 1.

NULL list elements do not themselves make rows TRUE, but must preserve SQL UNKNOWN semantics where relevant to expression evaluation.

For filter selectivity we estimate only rows evaluating TRUE.

---

# 597. NOT Selectivity

## LOCKED

For a predicate with known TRUE selectivity and NULL/UNKNOWN possibility, do not blindly use:

```text
1 - selectivity
```

unless the estimator tracks enough three-valued information.

Initial estimator may carry:

```text
true_fraction
false_fraction
unknown_fraction
```

for predicate estimation.

Then:

```text
NOT:
true'    = false
false'   = true
unknown' = unknown
```

This is intentionally more correct than two-valued optimizer math.

---

# 598. PredicateTruthEstimate

## LOCKED

Represent predicate estimates conceptually as:

```text
true_fraction
false_fraction
unknown_fraction
```

with:

```text
sum ≈ 1
```

Filter cardinality uses:

```text
true_fraction
```

This improves:

```text
NOT
NULL comparisons
AND
OR
```

reasoning.

---

# 599. AND Selectivity

## LOCKED baseline

When no stronger correlation knowledge exists, assume independence:

```text
P(A AND B) ≈ P(A) * P(B)
```

but compute three-valued truth probabilities consistently.

When predicates constrain the same column:

```text
x > 10 AND x < 20
```

use interval intersection / histogram logic rather than multiplying independent estimates.

---

# 600. OR Selectivity

## LOCKED baseline

When independence approximation is necessary:

```text
P(A OR B)
≈
P(A) + P(B) - P(A)*P(B)
```

with three-valued truth-state handling.

When predicates are mutually exclusive or operate on one column with known ranges, use stronger interval/MCV reasoning.

---

# 601. Same-Column Constraint Set

## LOCKED

Before estimating base filters, normalize simple predicates on one column into a constraint set:

```text
equality
lower bound
upper bound
IN set
IS NULL / NOT NULL
```

Detect contradictions.

Example:

```text
x >= 10
AND x < 5
```

becomes provably empty.

This also feeds index-range construction.

---

# 602. Correlated Columns

## LOCKED limitation

v1 statistics are primarily single-column.

Therefore predicates such as:

```text
city = 'Rome'
AND country = 'Italy'
```

may be badly estimated under independence.

The optimizer must expose this as a known limitation.

Do not invent false precision.

Future extended statistics:

```text
multi-column NDV
functional dependencies
multi-column MCVs
```

are a high-value later feature.

---

# 603. Cardinality Through Projection

## LOCKED

Projection normally preserves row count:

```text
rows_out = rows_in
```

except when expression analysis proves a separate semantic operator affects cardinality.

Row width changes based on projected expressions.

---

# 604. Cardinality Through Filter

## LOCKED

```text
rows_out =
    rows_in
    *
    predicate_true_fraction
```

with exact-empty handling.

---

# 605. Cardinality Through Limit

## LOCKED

```text
rows_out =
    min(max(0, rows_in - offset), limit)
```

where values are statically known.

If LIMIT absent:

```text
rows_out = max(0, rows_in - offset)
```

---

# 606. Cardinality Through DISTINCT

## LOCKED

Estimate output distinct rows using:

```text
NDV of projected/grouping expressions
```

where lineage/statistics permit.

For multiple columns without extended stats, use a bounded independence heuristic:

```text
product of NDVs
```

capped by input rows, with dampening to avoid absurd overestimation.

---

# 607. GROUP BY Cardinality

## LOCKED

For one grouping column:

```text
groups ≈ NDV + (1 if NULL_fraction > 0 else 0)
```

capped by input rows.

For multiple independent grouping columns:

```text
raw = product(component NDVs)
groups = dampened(raw)
groups <= input_rows
```

Use a damping model rather than blindly multiplying many huge NDVs.

A future extended-statistics system replaces this approximation.

---

# 608. Multi-Column NDV Damping

## LOCKED initial heuristic

For grouping/distinct over multiple columns sorted by descending individual NDV:

```text
combined = NDV_1
combined *= NDV_2 ^ 0.75
combined *= NDV_3 ^ 0.5
combined *= remaining_NDV ^ 0.25
```

then:

```text
combined <= input_rows
```

This is an explicitly heuristic fallback.

Mark it in optimizer trace so future estimate errors are understandable.

---

# 609. LEFT JOIN Cardinality

## LOCKED

Estimate matched rows using join predicate.

For LEFT JOIN:

```text
rows_out >= rows_left
```

because each unmatched left row survives once.

Approximate:

```text
matched_output
+
estimated_unmatched_left
```

The estimator must distinguish:

```text
join pair count
```

from:

```text
number of left rows with at least one match
```

A simple probabilistic approximation is acceptable initially.

---

# 610. Cost Model Philosophy

## LOCKED

Do not attempt to predict exact wall-clock milliseconds in v1.

Use a calibrated abstract cost model with explicit components:

```text
sequential I/O
random I/O
CPU tuple/vector work
comparison/hash work
memory pressure
temporary spill I/O
```

Final scalar cost is:

```text
weighted sum of components
```

Weights are configuration/calibration parameters.

---

# 611. Cost Structure

## LOCKED

Represent cost internally as:

```text
startup_cost
run_cost
total_cost

seq_page_reads
random_page_reads
temp_page_reads
temp_page_writes
cpu_rows
cpu_expressions
hash_ops
compare_ops
estimated_peak_memory
estimated_spill_bytes
```

The optimizer may compare:

```text
total_cost
```

while EXPLAIN shows useful components.

This prevents the cost model from becoming one opaque magic number.

---

# 612. Default Cost Units

## LOCKED initial calibration shape

Define configurable relative units such as:

```text
seq_page_cost
random_page_cost
cpu_tuple_cost
cpu_operator_cost
hash_cost
comparison_cost
temp_page_cost
```

Do not permanently lock arbitrary textbook constants.

Ship sensible defaults, then calibrate against our own engine benchmarks.

The architecture requires central configuration, not specific numeric values forever.

---

# 613. Cost Calibration Tool

## LOCKED

Build a small calibration benchmark that measures:

```text
cached sequential scan
cold-ish sequential page reads
random page reads
integer predicate throughput
VARCHAR comparison throughput
hash throughput
sort comparisons
temp spill read/write
```

Use these to propose relative cost weights.

Calibration results are deployment/configuration data.

They are not part of persistent database format.

---

# 614. Buffer Cache Assumption

## LOCKED initial model

v1 costing uses:

```text
effective_cache_pages
```

configuration plus relation size to estimate whether upper index/tree pages and some heap pages are likely cached.

Do not attempt to inspect the exact current BufferPool contents during optimization.

Reason:

plans should not oscillate wildly based on momentary individual page residency.

---

# 615. Sequential Scan Cost

## LOCKED

Approximate:

```text
I/O:
    physical_heap_pages * seq_page_cost

CPU:
    physical tuple versions inspected * visibility/header cost
    +
    live rows * pushed predicate/expression cost
    +
    output rows * decode/materialization cost
```

Dead-version fraction from table statistics may increase scan CPU even though logical row count is lower.

This links vacuum quality to scan performance.

---

# 616. B+ Tree Point Lookup Cost

## LOCKED

Approximate:

```text
tree descent:
    tree_height random/cached page accesses

leaf work:
    binary search + local scan

heap:
    candidate RID heap fetch

CPU:
    key comparisons
    MVCC visibility
    tuple decode
```

Upper tree levels are likely cached, so not every level must be charged as a full cold random I/O.

Use cache model conservatively.

---

# 617. Index Range Scan Cost

## LOCKED

Components:

```text
root-to-first-leaf descent
leaf pages traversed
index entries examined
heap candidate fetches
MVCC rejects
residual predicates
```

Estimated leaf entries derive from selectivity and index occupancy statistics.

Heap fetch cost depends heavily on clustering/correlation.

---

# 618. Index-Heap Correlation

## LOCKED initial statistic

Collect/derive an approximate correlation between:

```text
index key order
```

and:

```text
heap PageNo order
```

for indexed leading columns when practical during ANALYZE/index statistics.

Represent approximately:

```text
-1.0 .. +1.0
```

using sampled `(key rank, heap page rank)` pairs.

High absolute correlation means range scans are more sequential.

Low correlation means near-random heap access.

This substantially improves range-index costing.

---

# 619. Fallback Heap Fetch Cost

## LOCKED

If no useful correlation statistic exists, assume secondary-index heap fetches are mostly random but cap expected distinct heap pages by:

```text
table physical heap pages
```

Do not charge one cold random page per matching row if many matches necessarily share pages.

Use an occupancy/distinct-page estimate.

---

# 620. Access-Path Predicate Analysis

## LOCKED

Before access-path enumeration classify predicates into:

```text
index-search conditions
scan-pushable filters
residual filters
```

This classification is semantic and index-schema aware.

Do not choose an index simply because one predicate mentions an indexed column.

---

# 621. Sargable B+ Tree Conditions

## LOCKED

For an index on:

```text
(a,b,c)
```

usable search pattern is:

```text
zero or more leading equality constraints
followed by at most one range-constrained key component
```

Example:

```text
a = 5
AND b = 7
AND c >= 10
AND c < 20
```

gives a tight range.

But:

```text
a > 5
AND b = 7
```

uses `a` range for search; `b=7` is residual for v1.

This is the standard leftmost-prefix behavior of the B+ tree.

---

# 622. Composite Index Bounds

## LOCKED

Construct encoded physical user-key lower/upper bounds using the same `IndexKeyCodec`.

Use conceptual low/high sentinels for unspecified trailing key components.

For duplicate physical keys, RID search sentinels are appended:

```text
MIN_RID
MAX_RID
```

as required.

Bound construction must preserve:

```text
inclusive / exclusive
NULL ordering
type/collation semantics
```

---

# 623. Index Predicate Residuals

## LOCKED

A predicate may be partially satisfied by index search and still require residual evaluation.

Example:

```text
index(a,b)

WHERE a = 5
AND b > 10
AND expensive_function(c)
```

Search:

```text
a=5, b>10
```

Residual:

```text
expensive_function(c)
```

Do not re-evaluate index-proven predicates unnecessarily unless required for correctness due to lossy encoding; our v1 B+ tree encoding is exact.

---

# 624. Base Access Paths

## LOCKED

For every `LogicalGet`, enumerate at least:

```text
PhysicalSeqScan
every usable PhysicalIndexScan
```

Each path has:

```text
estimated rows
estimated cost
ordering property
required heap columns
residual predicate
```

Do not hardcode "use index if possible."

---

# 625. Multiple Indexes

## LOCKED v1

Choose one B+ tree access path per base relation.

Deferred:

```text
bitmap index intersection
bitmap index union
index merge
```

Predicates not supported by the chosen index remain residual filters.

This keeps base path enumeration understandable.

---

# 626. Index Scan Break-Even

## LOCKED

Costing should naturally find the break-even point where:

```text
many random RID heap fetches
```

become more expensive than:

```text
one sequential heap scan
```

No rule such as:

```text
if selectivity < 10% use index
```

is allowed as the primary decision mechanism.

Thresholds should emerge from costs, table size, correlation, cache assumptions, and row width.

---

# 627. Required Columns and Access Cost

## LOCKED

Projection pruning informs scan costing.

A SeqScan decoding:

```text
2 narrow columns
```

is cheaper in CPU/memory than decoding:

```text
20 columns + long VARCHAR
```

even though physical heap-page I/O is similar.

IndexScan still needs heap visibility, but required output width affects heap decode cost.

---

# 628. Physical Property Model

## LOCKED initial properties

Track:

```text
OrderingProperty
```

and:

```text
required output slots
```

in v1.

Architecture reserves future properties:

```text
partitioning
rewindability
materialization
```

Do not build a full property lattice before needed.

---

# 629. Ordering Satisfaction

## LOCKED

An available ordering satisfies a required ordering if it provides the required prefix with identical:

```text
expression/slot identity
direction
NULL ordering
collation
```

Example:

```text
available: (a ASC, b ASC, c ASC)
required:  (a ASC, b ASC)
```

is satisfied.

But:

```text
available: (a ASC, b DESC)
required:  (a ASC, b ASC)
```

is not.

---

# 630. Interesting Orders

## LOCKED

Retain certain non-cheapest plans when they produce useful ordering.

Interesting orders come from:

```text
final ORDER BY
GROUP BY / sort aggregate opportunities
merge-join keys
index access order
DISTINCT/order opportunities
```

A slightly more expensive child may produce a cheaper overall plan by avoiding a large sort.

This is a core System-R lesson and must be represented explicitly.

---

# 631. Plan Alternative Key

## LOCKED

The optimizer memo/table stores alternatives keyed conceptually by:

```text
logical relation/subproblem identity
+
physical property class
```

For join enumeration:

```text
RelationSet
+
OrderingProperty
```

For each key retain the cheapest known valid plan.

Do not retain thousands of dominated plans with identical useful properties.

---

# 632. RelationSet

## LOCKED

For join optimization with up to a practical threshold, represent participating base bindings as a bitset.

Example:

```text
A = bit 0
B = bit 1
C = bit 2

{A,C} = 0b101
```

This makes subset DP efficient.

`BindingId`, not `TableId`, identifies a relation occurrence.

Self-joins therefore occupy distinct bits.

---

# 633. Join Graph

## LOCKED

Inner/cross joins are transformed into a join graph:

```text
vertices:
    relation bindings

edges:
    join predicates

vertex predicates:
    local filters
```

Outer joins produce ordering/associativity constraints and are not freely inserted into the unrestricted inner-join graph.

---

# 634. Join Enumeration Scope

## LOCKED

For a maximal reorderable inner-join region:

```text
enumerate alternative join trees
```

while treating non-reorderable boundaries such as:

```text
LEFT JOIN constraints
blocking semantic subplans
```

as atomic inputs or constrained edges.

Do not reorder across an outer-join boundary unless a proven logical rewrite converted it safely to an inner join.

---

# 635. Bushy Dynamic Programming

## LOCKED

For small join regions, enumerate bushy join trees, not only left-deep trees.

For subset `S`:

```text
for each non-empty partition:
    S = A ∪ B
    A ∩ B = ∅
```

combine best useful-property plans from `A` and `B`.

Skip symmetric duplicates by a deterministic partition rule.

Prefer connected partitions when join predicates exist.

This can discover plans such as:

```text
(A ⋈ B) ⋈ (C ⋈ D)
```

which left-deep-only enumeration misses.

---

# 636. Exhaustive Join Threshold

## LOCKED initial default

Use exhaustive/bushy subset DP up to approximately:

```text
10 relation bindings
```

in one reorderable join region.

This default is configurable.

The threshold is chosen because exhaustive bushy search grows rapidly.

Benchmark planning time before raising it.

---

# 637. Large-Join Heuristic

## LOCKED

Above the exhaustive threshold:

1. build a good initial plan greedily using estimated incremental cost,
2. prefer connected joins over Cartesian products,
3. apply bounded local improvement:
   ```text
   adjacent swap
   join rotation
   limited subtree exchange
   ```
4. optionally keep a small beam of best candidates.

Do not allow planning time to explode exponentially for 30-table joins.

---

# 638. Cartesian Products

## LOCKED

Avoid Cartesian joins while a connected join predicate is available between remaining components.

Allow Cartesian product only when logically required by the query graph.

Charge appropriately high cardinality/cost:

```text
rows_A * rows_B
```

before later filters.

---

# 639. Join Algorithm Alternatives

## LOCKED

For each legal binary join pair consider applicable:

```text
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
MergeJoin when implemented/ordered
```

Do not select join order first and algorithm second in complete isolation.

Algorithm cost may make a different join order preferable.

---

# 640. Hash Join Cost

## LOCKED

Estimate:

```text
build child cost
probe child cost

+
build rows * hash/build CPU
+
probe rows * hash/probe CPU
+
expected matches * output CPU
+
residual predicate cost

+
memory cost / spill I/O when build exceeds memory
```

Build side should usually be smaller after filters, but cost determines this rather than a fixed left/right rule.

---

# 641. Hash Join Memory Estimate

## LOCKED

Estimate build memory from:

```text
build_rows
*
(
    build RowLayout width
    +
    hash-directory overhead
    +
    duplicate-chain metadata
)
/
target hash load factor
```

Include varlen average widths.

Compare against:

```text
available operator/query memory budget
```

to estimate spill.

---

# 642. Hash Join Spill Cost

## LOCKED

If estimated build memory exceeds reservation:

```text
partition write:
    build + probe bytes

partition read:
    build + probe bytes

+
hash repartition CPU
+
per-partition rebuild/probe CPU
```

If one pass is insufficient, estimate additional recursive repartition passes.

This makes memory budget a real physical-planning input.

---

# 643. Nested Loop Cost

## LOCKED

For materialized inner side:

```text
outer cost
+
inner build/materialization cost
+
outer_rows * inner_rows * predicate_cpu
```

For very small inner relation, this can beat hash-table setup.

Do not assign hash join zero startup cost.

---

# 644. Index Nested-Loop Cost

## LOCKED

Estimate:

```text
outer child cost
+
outer_rows
*
(
    inner B+ tree lookup/range cost
    +
    expected inner heap fetch cost
    +
    residual predicate CPU
)
```

Apply batching/cache reductions for repeated access where reasonable.

This plan is attractive when:

```text
outer_rows is small
and
inner index lookup is selective
```

---

# 645. Repeated INLJ Key Locality

## LOCKED initial refinement

If outer join keys have low NDV relative to outer rows, repeated index lookups may reuse:

```text
same B+ tree paths
same heap pages
```

Reduce estimated random I/O modestly based on outer key NDV.

Do not assume every outer row causes a fully cold lookup.

---

# 646. Merge Join Cost

## LOCKED when implemented

If both sides already satisfy required key ordering:

```text
left cost
+
right cost
+
linear merge CPU
+
duplicate group handling
```

If one/both sides require Sort:

```text
include sort enforcement cost
```

Merge join may win when ordering is useful downstream even when raw join cost is close to hash join.

---

# 647. Sort Cost

## LOCKED

In-memory approximate CPU:

```text
N * log2(max(N,2)) * comparison_cost
```

adjusted by key width/type.

Memory:

```text
N * sort_record_width
+
payload storage
```

If memory exceeds budget:

```text
run generation write
run read
merge passes
```

cost temporary sequential I/O.

---

# 648. TopN Cost

## LOCKED

For:

```text
ORDER BY ... LIMIT K
```

estimate heap cost:

```text
N * log2(max(K,2))
```

with memory roughly:

```text
K * row width
```

Choose TopN only when:

```text
K << N
```

by cost, not a fixed SQL syntax rule.

---

# 649. Hash Aggregate Cost

## LOCKED

Estimate:

```text
child cost
+
input_rows * hash/update CPU
+
estimated_groups * group allocation/finalize CPU
+
spill cost if group table exceeds memory
```

Memory derives from:

```text
groups
*
(group key width + aggregate state width + hash overhead)
```

---

# 650. Sort Aggregate Cost

## LOCKED when implemented

If child is already ordered by grouping keys:

```text
streaming aggregate cost
```

is low memory and approximately linear.

Otherwise:

```text
sort cost
+
streaming aggregate cost
```

Physical optimizer may choose this over hash aggregation when:

- ordering already exists,
- memory pressure makes hash spill expensive,
- ordering is useful downstream.

---

# 651. DISTINCT Cost

## LOCKED

Enumerate applicable:

```text
HashDistinct
Sort + StreamingDistinct
```

where implementations exist.

Retain ordering properties when sort-based distinct may satisfy a later ORDER BY.

---

# 652. Sort Enforcement

## LOCKED

If a required ordering is not produced naturally:

```text
insert PhysicalSort
```

and include its cost.

Potentially use:

```text
PhysicalTopN
```

when requirement comes from ORDER BY + LIMIT.

Ordering enforcement belongs to physical planning.

---

# 653. Final ORDER BY Optimization

## LOCKED

For final ORDER BY, compare:

```text
unordered cheapest plan + Sort
```

against:

```text
slightly more expensive naturally ordered plan
```

such as an IndexScan/merge path.

Choose based on total cost.

Do not always sort simply because SQL has ORDER BY.

---

# 654. Limit-Aware Planning

## LOCKED

A small LIMIT changes cost priorities.

Track:

```text
startup cost
run cost
```

so a plan capable of producing early rows cheaply may beat one with lower full-result total cost.

For required first `K` rows, compare approximately:

```text
startup_cost
+
fraction_of_run_cost_needed_for_K
```

when the operator supports streaming/early termination.

Blocking HashJoin build/Sort may have high startup.

IndexScan + Limit may therefore win.

---

# 655. Required Rows Objective

## LOCKED

Optimizer root may specify:

```text
required_rows
```

derived from LIMIT where semantically safe.

Do not propagate it blindly through:

```text
sort
aggregate
distinct
```

which may need complete input.

Streaming filters/projections/scans may use it for cost interpolation.

---

# 656. Predicate CPU Cost

## LOCKED

Assign expression cost metadata.

Examples:

```text
simple integer comparison:
    cheap

VARCHAR comparison:
    width-sensitive

immutable scalar function:
    function-specific/default cost

subquery:
    separately costed
```

Predicate ordering inside a filter may later evaluate cheap/selective predicates first where volatility/error semantics permit.

Initial optimizer can use a simple expression-cost score.

---

# 657. Filter Predicate Ordering

## LOCKED later optimization

For conjunctive predicates that are:

```text
immutable
safe to reorder
```

rank roughly by:

```text
expected cost per rejected row
```

or:

```text
evaluation_cost / rejection_probability
```

to reduce expensive downstream predicate evaluation.

Do not reorder volatile or error-sensitive expressions without proving semantic safety.

---

# 658. Physical Plan Memo

## LOCKED

Use a compact optimizer-owned memo/table rather than attaching alternatives to logical AST nodes.

For each subproblem/property retain:

```text
best cost
physical plan prototype
estimated rows
width
properties
```

Plan prototypes are query-arena allocated.

Discard dominated alternatives.

---

# 659. Dominance Rule

## LOCKED

Plan A dominates B for the same logical subproblem when:

```text
A.cost <= B.cost
```

and A provides physical properties at least as useful as B with no relevant disadvantage.

Example:

```text
same rows
same ordering
lower cost
```

=> discard B.

But an unordered cheaper plan does not dominate a slightly more expensive interesting-order plan.

---

# 660. Join DP Initialization

## LOCKED

For each base relation:

1. estimate base-filter selectivity,
2. enumerate SeqScan and usable indexes,
3. estimate cardinality once at the logical base-filter level,
4. cost each physical access path,
5. retain cheapest alternatives by interesting ordering/property.

The same logical base cardinality should not vary merely because one physical scan method was chosen.

---

# 661. Join DP Transition

## LOCKED

For each subset `S` of increasing size:

```text
for each legal partition S = A ∪ B:
    identify join predicates crossing A/B
    estimate join cardinality
    enumerate join algorithms
    combine interesting child properties
    cost alternatives
    retain non-dominated best plans
```

Cache cardinality for logical relation sets/predicate sets where possible.

Do not recompute expensive stats logic for identical subproblems unnecessarily.

---

# 662. Join Cardinality Must Be Algorithm-Independent

## LOCKED

For semantically identical:

```text
A ⋈ B on predicate P
```

estimated output rows must not depend on whether physical algorithm is:

```text
HashJoin
NestedLoopJoin
MergeJoin
```

Physical algorithm affects cost/properties, not logical cardinality.

This separation is critical.

---

# 663. Outer Join Search Constraints

## LOCKED

Initial optimizer preserves the logical nesting/order of LEFT JOIN relative to dependent relations.

It may still optimize:

```text
access paths inside each side
inner-join regions contained within a side
physical join algorithm
```

More advanced outer-join reorder transformations are deferred until formal rules/tests exist.

---

# 664. Subquery Physical Planning

## LOCKED initial scope

Uncorrelated scalar/EXISTS/IN subqueries may be planned once as independent physical subplans.

Cost includes:

```text
subquery execution startup
materialization/hash-set cost where applicable
```

Correlated subquery optimization is deferred until decorrelation.

Do not accidentally estimate a correlated subquery as one-time execution.

---

# 665. Parameter-Free Plans

## LOCKED v1

Without prepared statement parameters, plans are optimized using literal values directly.

This allows histogram/MCV selectivity for:

```sql
WHERE age = 37
```

Prepared statements later introduce:

```text
generic vs custom plan
parameter-sensitive planning
```

and are intentionally deferred.

---

# 666. Statistics Missing Fallbacks

## LOCKED

When statistics are absent, use explicit conservative fallback defaults.

Examples may include:

```text
equality unknown selectivity
range unknown selectivity
generic NDV
generic null fraction
```

Centralize these defaults.

EXPLAIN should mark estimates derived from fallback stats.

Do not silently pretend missing statistics are precise.

---

# 667. Statistics Confidence

## LOCKED recommendation

Each estimate may carry a coarse confidence:

```text
HIGH
MEDIUM
LOW
```

Examples:

```text
constraint/proven exact:
    HIGH

fresh MCV/histogram:
    HIGH/MEDIUM

independence across columns:
    LOW

missing-stat fallback:
    LOW
```

v1 plan selection may not directly optimize on confidence, but EXPLAIN/diagnostics should expose it.

This makes bad estimates easier to learn from.

---

# 668. Estimation Provenance

## LOCKED

Optimizer trace should record why a selectivity was chosen:

```text
MCV hit
histogram range
NDV fallback
unique constraint
independence assumption
multi-column damping
missing statistics default
```

This is invaluable for debugging a poor plan.

---

# 669. Optimizer Trace

## LOCKED

Provide a debug mode:

```text
EXPLAIN (OPTIMIZER TRACE)
```

or equivalent internal hook showing:

```text
logical normalized predicates
base cardinality estimates
available access paths
costs
join subsets explored
plans pruned
interesting orders retained
final chosen plan
```

Do not expose every internal byte to ordinary users, but make the reasoning inspectable for this project.

---

# 670. EXPLAIN Estimates

## LOCKED

Normal EXPLAIN should show at least:

```text
physical operator
estimated rows
estimated width
startup cost
total cost
estimated memory
estimated spill when relevant
output ordering where useful
```

Example:

```text
HashJoin
  rows=18,200
  width=48
  cost=310.4..928.1
  memory=3.8MiB
```

The exact text format may evolve.

---

# 671. EXPLAIN ANALYZE Estimate Error

## LOCKED

For each physical node report:

```text
estimated rows
actual rows
q-error
```

Define q-error for positive estimates/actuals as:

```text
max(
    estimated / actual,
    actual / estimated
)
```

Handle zero carefully and report:

```text
∞ / special marker
```

where one side is zero and the other nonzero.

This is one of the best tools for learning optimizer behavior.

---

# 672. Cardinality Error Attribution

## LOCKED recommendation

EXPLAIN ANALYZE should make it possible to see the first operator where estimates diverge badly.

Important because a join-order failure often originates much lower at:

```text
one predicate estimate
```

rather than in the join algorithm itself.

---

# 673. No Runtime Statistics Feedback in v1

## LOCKED

Do not automatically rewrite persistent statistics from one query's actual rows.

Reasons:

- feedback may be parameter/literal specific,
- concurrent workload bias,
- complicated validity semantics.

Record estimate-vs-actual diagnostics.

Later research project:

```text
cardinality feedback / adaptive reoptimization
```

---

# 674. Planner Time Budget

## LOCKED

Optimization itself has a cost.

Track:

```text
optimization wall time
subproblems explored
physical alternatives costed
plans retained/pruned
```

For large joins, obey the heuristic threshold rather than spending seconds seeking a marginally better plan.

---

# 675. Planning Memory Budget

## LOCKED

Optimizer allocations use a query planning arena with a configurable upper budget.

If exhaustive search approaches the budget:

```text
switch to heuristic search
```

rather than crashing.

Plan search must not compete unboundedly with execution memory.

---

# 676. Deterministic Optimization

## LOCKED

Given identical:

```text
logical plan
catalog
statistics
optimizer configuration
```

the optimizer should choose the same plan deterministically.

Tie-break equal/near-equal costs using a stable rule such as:

```text
lower structural plan fingerprint
or
preferred simpler operator order
```

Determinism makes testing/regressions vastly easier.

---

# 677. Cost Tie Tolerance

## LOCKED

Floating costs can differ by tiny numerical noise.

Treat plans within a small relative epsilon as effectively tied.

Use deterministic tie-breaking.

Do not let hash-map iteration order choose query plans.

---

# 678. Plan Fingerprint

## LOCKED

Generate a stable debug fingerprint from:

```text
physical operator kinds
object IDs
join order
access paths
important operator parameters
```

Do not include memory addresses.

Uses:

```text
optimizer regression tests
benchmark comparison
EXPLAIN debugging
future plan cache keys
```

---

# 679. Logical Rewrite Before Costing

## LOCKED

Always perform safe high-value logical rewrites before physical enumeration:

```text
constant folding
boolean simplification
predicate pushdown
projection pruning
inner-join predicate extraction
empty-plan detection
```

This reduces optimizer search space and improves cardinality inputs.

Do not mix trivial semantic normalization into every physical cost rule.

---

# 680. Join Predicate Equivalence Classes

## LOCKED

For inner joins, derive equality equivalence classes.

Example:

```text
A.x = B.x
B.x = C.x
```

implies:

```text
A.x, B.x, C.x
```

same equivalence class.

Uses:

```text
derive transitive join predicates
propagate constants
recognize interesting orders
improve join graph connectivity
```

Only derive under semantics where NULL/equality behavior and join type make it safe.

---

# 681. Constant Propagation Through Equality

## LOCKED

For inner-join/filter-safe equivalence:

```text
A.x = B.x
AND A.x = 5
```

derive:

```text
B.x = 5
```

This may enable an index access path on B.

Do not propagate across nullable outer-join semantics without proof.

---

# 682. Contradiction Detection

## LOCKED

Logical constraint analysis should detect cases such as:

```text
x = 1 AND x = 2
x < 5 AND x >= 5
NOT NULL column IS NULL
```

and produce:

```text
provably empty plan
```

before physical optimization.

This improves both planning time and execution.

---

# 683. Unique/Key Metadata in Optimization

## LOCKED

Use catalog constraints to infer:

```text
candidate uniqueness
maximum join multiplicity
DISTINCT redundancy opportunities
GROUP BY key properties
```

Only trust constraints enforced by the engine.

Do not infer uniqueness merely because an index happens to have low estimated NDV.

---

# 684. Foreign-Key Metadata

## DEFERRED

Once foreign keys exist, optimizer may use trusted FK relationships for join cardinality and potentially join elimination.

Do not design v1 optimizer around metadata the SQL engine does not yet enforce.

---

# 685. Join Elimination

## DEFERRED

Do not initially remove joins based on constraints unless the semantic proof is straightforward and thoroughly tested.

High-value later optimization:

```text
FK join elimination
unused unique-side join elimination
```

Correctness risk is higher than the early performance reward.

---

# 686. Index-Order Match

## LOCKED

Physical planner checks whether an IndexScan ordering can satisfy:

```text
ORDER BY
GROUP BY ordering for streaming aggregate
MergeJoin input requirement
```

Use the index's actual key schema:

```text
column order
ASC/DESC capability
NULL ordering
collation
```

v1 B+ tree forward scan only naturally provides its ascending physical order.

---

# 687. Reverse-Scan Limitation

## LOCKED

Because native reverse B+ tree scans are deferred:

```text
ORDER BY key DESC
```

cannot claim descending index order in v1.

Optimizer must insert Sort/TopN as needed.

When reverse scans are implemented later, add a physical property alternative.

---

# 688. Hash Join Build-Side Selection

## LOCKED

For an INNER JOIN, enumerate both:

```text
build left / probe right
build right / probe left
```

when semantically equivalent.

Cost based on:

```text
build rows
build width
memory/spill probability
probe rows
```

Do not assume "always build right".

For LEFT JOIN, semantics constrain preserved/probe behavior of the initial implementation; only enumerate supported orientations.

---

# 689. Join Output Width

## LOCKED

Estimate:

```text
width_join =
    required left output widths
    +
    required right output widths
```

after projection pruning.

Do not cost hash join memory based on columns that downstream never needs.

This makes projection pruning materially affect physical plans.

---

# 690. Hash Table Payload Pruning

## LOCKED

For HashJoin build side, store only:

```text
join key data required for validation
build payload slots required downstream/residual
```

Cost model uses this pruned RowLayout width.

Do not store the entire base table row by default.

---

# 691. Sort Payload Pruning

## LOCKED

Sort materializes only:

```text
sort keys
payload values required downstream
```

or compact row handles where lifetime permits.

Cost/memory estimate must use this projected width.

---

# 692. Aggregate State Width

## LOCKED

Each aggregate function reports:

```text
state size
alignment
```

to the optimizer.

HashAggregate memory estimate is therefore based on the real group state layout, not a universal guessed row size.

---

# 693. Spill Probability

## LOCKED

Treat spill as a deterministic estimate in v1:

```text
estimated_required_memory > assigned_memory_budget
    => spill expected
```

Then estimate passes/bytes.

Later optimizer may model concurrency and uncertain memory availability probabilistically.

---

# 694. Query Memory Allocation During Planning

## LOCKED

The physical planner assigns rough memory targets among simultaneous blocking operators based on pipeline dependencies.

Operators that cannot be active concurrently need not all reserve their peak at once.

For v1, use a conservative query-level budget and per-operator estimate.

Do not pretend each blocking operator independently has the full global database memory limit.

---

# 695. Pipeline-Aware Peak Memory

## LOCKED recommendation

Estimate peak memory using the physical pipeline dependency graph:

```text
which blocking states coexist?
```

Example:

```text
HashJoin build table
may coexist with
probe-side upstream operator state
```

but a child sort may be finalized/released before a later unrelated sort.

Initial implementation may use a conservative upper bound.

Expose overestimation in diagnostics rather than risking OOM.

---

# 696. Materialization Cost

## LOCKED

Blocking/materializing operators include CPU/memory copy cost for:

```text
RowCollection append
string deep copy
temporary serialization
```

This matters when comparing:

```text
NestedLoop materialize inner
HashJoin build
Sort
DML target spool
```

Do not treat materialization as free.

---

# 697. Startup vs Total Cost

## LOCKED

Every physical operator exposes both:

```text
startup_cost
total_cost
```

Examples:

### SeqScan

Low startup.

### IndexScan

Low-to-medium startup.

### HashJoin

Build side contributes substantial startup.

### Sort

Full sort has high startup.

### TopN

Still blocking in current implementation, but less memory/CPU.

This supports LIMIT-sensitive choices.

---

# 698. Plan Search and LIMIT

## LOCKED

At the root, if only a small number of rows are required and no blocking semantic operator intervenes, compare plans on:

```text
startup + partial run cost
```

not only full-result total cost.

This is especially important for:

```text
IndexScan + LIMIT
```

vs:

```text
SeqScan + Sort + LIMIT
```

---

# 699. Optimizer Correctness Boundary

## LOCKED

The optimizer is allowed to choose a bad plan.

It is never allowed to change query meaning.

Therefore:

```text
cost estimate errors -> performance bug

semantic rewrite errors -> correctness bug
```

Keep logical equivalence rules separated and more heavily tested than cost heuristics.

---

# 700. Optimizer Validation

## LOCKED

Validate final physical plan:

```text
logical output semantics preserved
required slots present
required ordering satisfied/enforced
join types preserved
predicates assigned exactly where semantically legal
transaction/DML hidden slots preserved
physical operator supports requested semantics
```

Then pass to the execution-layer PhysicalPlanValidator.

---

# 701. Statistics Tests

## LOCKED

Test analyzer on controlled distributions:

```text
uniform integers
highly skewed MCV
many NULLs
all same value
all distinct
two-mode distribution
monotonic values
VARCHAR prefixes
FLOAT64 edge values
```

Verify:

```text
row counts
NDV accuracy bounds
MCV detection
histogram boundaries
null fraction
width estimates
```

---

# 702. Selectivity Estimation Tests

## LOCKED

For synthetic data with known distributions, compare:

```text
estimated selectivity
actual selectivity
q-error
```

for:

```text
=
<
<=
>
BETWEEN
IN
IS NULL
AND
OR
NOT
same-column ranges
```

Include values:

```text
inside MCV
outside MCV
outside min/max
near histogram boundaries
```

---

# 703. Join Estimation Tests

## LOCKED

Synthetic joins:

```text
unique-to-many
many-to-many uniform
hot-key skew
disjoint domains
partially overlapping domains
NULL-heavy keys
duplicate-heavy MCVs
```

Compare baseline NDV estimator against MCV-aware estimator.

Store regression expectations for cases that previously failed badly.

---

# 704. Access Path Tests

## LOCKED

Construct catalog/stats scenarios where optimizer should choose:

```text
SeqScan for low-selectivity predicate
IndexScan for highly selective predicate
IndexScan for ORDER BY avoidance
SeqScan when index correlation is poor and result is large
IndexScan when correlation is high
```

Do not assert arbitrary exact cost numbers unless testing the cost formula itself.

Prefer plan-shape expectations under controlled parameters.

---

# 705. Join-Order Tests

## LOCKED

Use known cardinalities where one join order is dramatically better.

Example:

```text
A = 1M rows
B = 1M rows
C = 100 rows

A ⋈ B produces huge intermediate
B ⋈ C highly selective
```

Verify DP chooses the selective early join when statistics indicate it.

Also test a case where a bushy plan wins.

---

# 706. Interesting-Order Tests

## LOCKED

Verify optimizer can retain a slightly more expensive ordered path and use it to avoid:

```text
Sort
```

or enable:

```text
MergeJoin / streaming aggregate
```

when total plan cost is lower.

---

# 707. Memory/Spill Plan Tests

## LOCKED

With identical statistics but different query memory budgets:

```text
large budget
small budget
```

verify plan selection may change between:

```text
HashAggregate vs SortAggregate
HashJoin vs alternative
in-memory Sort vs spill-aware costs
```

where supported.

---

# 708. Optimizer Differential Correctness Tests

## LOCKED

For small random schemas/data:

1. generate supported logical queries,
2. execute optimizer-chosen physical plan,
3. execute a simple trusted reference physical plan, such as:
   ```text
   SeqScan + NestedLoopJoin + straightforward operators
   ```
4. compare results.

This tests optimizer transformation correctness independently of cost quality.

---

# 709. Optimizer Fuzzing

## LOCKED

Fuzz:

```text
logical expression trees
predicate combinations
statistics values within legal ranges
join graphs
ordering requirements
```

Requirements:

```text
no crashes
no NaN/negative costs escaping
no invalid physical plan
bounded planning time above threshold
```

---

# 710. Cost Model Benchmarks

## LOCKED

For each operator collect actual resource behavior across scales:

```text
SeqScan
IndexScan
HashJoin
NestedLoop
IndexNestedLoop
HashAggregate
Sort
TopN
spill paths
```

Compare:

```text
predicted relative ranking
actual runtime ranking
```

The goal is not perfect milliseconds.

The goal is that cheaper predicted plans usually correspond to faster actual plans.

---

# 711. Plan Regression Suite

## LOCKED

Maintain a set of named optimizer scenarios with:

```text
schema
statistics
query
expected important plan properties
plan fingerprint where stable
```

Run on every optimizer change.

Examples:

```text
selective point lookup
large range scan
star join
skewed hot key
ORDER BY index match
small LIMIT
low-memory hash spill
```

---

# 712. Optimizer Performance Benchmarks

## LOCKED

Measure planning time for join counts:

```text
2
4
6
8
10
12
16
20
30
```

Track:

```text
subsets explored
partitions considered
physical plans costed
memo entries
peak planning memory
```

Verify exhaustive search transitions to bounded heuristic behavior.

---

# 713. Star Schema Benchmark

## LOCKED recommendation

Even though the engine is general purpose, use a synthetic star schema to stress:

```text
many joins
selective dimensions
large fact table
aggregation
```

This is an excellent optimizer-learning workload.

Later TPC-H-inspired queries can be added without claiming benchmark compliance.

---

# 714. No Benchmark Gaming

## LOCKED

Do not hardcode:

```text
query text fingerprints
known benchmark table names
special-case TPC query shapes
```

Optimizer improvements must arise from general statistics/rules/costing.

---

# 715. Deliberately Deferred Optimizer Features

## LOCKED

Do not implement before the v1 cost optimizer is correct and measurable:

```text
full Cascades optimizer
memo transformation rules over arbitrary expressions
adaptive query reoptimization
runtime join switching
learned cardinality estimation
multi-column extended statistics
bitmap index intersection
materialized-view matching
join elimination
correlated-subquery decorrelation
parameter-sensitive plan cache
generic/custom prepared plans
partition pruning
parallel-cost modeling in great detail
distributed cost model
GPU cost model
```

These remain advanced projects.

---

# 716. High-Reward Future Optimizer Experiments

## LOCKED recommendation

After v1, especially valuable projects include:

```text
multi-column statistics
functional-dependency statistics
DPccp connected-subgraph join enumeration
Cascades-style memo optimizer
correlated subquery decorrelation
Bloom-filter/semi-join reduction planning
bitmap index intersection
adaptive cardinality feedback
parameter-sensitive plans
parallel degree selection
learned cardinality estimation experiments
```

These should be compared experimentally against the understandable v1 baseline.

---

# 717. Cost-Based Optimizer Invariants

## LOCKED

Codex must preserve:

1. Statistics affect performance choices, never result correctness.
2. Logical cardinality estimates are independent of physical algorithm choice.
3. Physical costs do not change SQL semantics.
4. One optimization uses a stable statistics snapshot.
5. Missing statistics are explicit low-confidence fallbacks.
6. Every selectivity remains in `[0,1]`.
7. Provably empty is distinguished from merely estimated-small.
8. NULL selectivity uses SQL three-valued semantics.
9. Hash/equality/grouping statistics use compatible canonical value semantics.
10. Base access-path enumeration includes SeqScan and every usable index path.
11. Index existence alone never forces IndexScan.
12. Composite B+ tree access obeys leftmost-prefix/range rules.
13. Residual predicates remain attached when index bounds do not fully express them.
14. Join order and join algorithm are optimized together.
15. Inner-join DP uses BindingId occurrences, not TableId identity.
16. Outer-join semantic constraints are preserved.
17. Interesting ordering properties may justify retaining a non-cheapest subplan.
18. Required ordering is explicitly enforced when not naturally satisfied.
19. Memory/spill cost is considered for blocking operators.
20. Projection pruning affects row width and physical memory cost.
21. LIMIT may change optimization objective through startup/partial cost.
22. HashJoin build-side orientation is costed rather than hardcoded where semantics permit.
23. Cost ties are resolved deterministically.
24. Hash-map iteration order may not determine the chosen plan.
25. Optimizer search is bounded for large join graphs.
26. Final plans pass semantic and physical validation.
27. EXPLAIN exposes estimates and chosen physical decisions.
28. EXPLAIN ANALYZE exposes actual rows and estimation error.
29. Cost calibration is centralized and benchmark-driven.
30. A slower plan is an optimizer-quality issue; an incorrect plan is an optimizer correctness failure and must be treated separately.

---

# 718. Recommended Optimizer Module Layout

## LOCKED

```text
src/
  statistics/
    table_statistics.h
    column_statistics.h
    histogram.h
    mcv_list.h
    hyperloglog.h
    space_saving.h
    reservoir_sampler.h
    analyze.h
    analyze.cpp

  optimizer/
    logical_rewrite/
      ...

    estimation/
      predicate_truth_estimate.h
      selectivity_estimator.h
      selectivity_estimator.cpp
      cardinality_estimator.h
      cardinality_estimator.cpp
      join_estimator.h
      join_estimator.cpp

    properties/
      ordering_property.h
      physical_properties.h
      interesting_orders.h

    cost/
      cost.h
      cost_model.h
      cost_model.cpp
      cost_parameters.h
      calibration.h

    access/
      predicate_analyzer.h
      index_bound_builder.h
      access_path.h
      access_path_enumerator.h

    join/
      relation_set.h
      join_graph.h
      join_enumerator.h
      join_dp.h
      large_join_heuristic.h

    memo/
      plan_alternative.h
      optimizer_memo.h

    physical/
      physical_planner.h
      physical_planner.cpp
      property_enforcer.h

    debug/
      optimizer_trace.h
      plan_fingerprint.h
```

Exact filenames may evolve.

Keep:

```text
statistics
estimation
costing
enumeration
physical planning
```

as separable concepts.

---

# 719. Optimizer Implementation Order

## LOCKED

### Phase O1 — Statistics foundation

1. table statistics descriptor
2. column statistics descriptor
3. exact small-table collection
4. HLL NDV
5. SpaceSaving MCV
6. reservoir sample
7. equi-depth histogram
8. ANALYZE persistence
9. statistics tests

### Phase O2 — Selectivity/cardinality

10. PredicateTruthEstimate
11. equality
12. NULL
13. ranges
14. IN
15. same-column constraint intersection
16. AND/OR/NOT
17. projection/filter/limit cardinality
18. GROUP BY/DISTINCT NDV
19. equijoin estimator
20. MCV-aware join estimator
21. estimation q-error tests

### Phase O3 — Cost model

22. Cost structure
23. cost parameters
24. SeqScan cost
25. IndexScan cost
26. HashJoin cost
27. NestedLoop cost
28. IndexNestedLoop cost
29. Aggregate cost
30. Sort/TopN cost
31. spill cost
32. calibration benchmarks

### Phase O4 — Base access planning

33. predicate classification
34. B+ tree sargability
35. composite bound construction
36. SeqScan alternatives
37. IndexScan alternatives
38. ordering properties
39. access-path tests

### Phase O5 — Join optimization

40. join graph
41. RelationSet
42. base memo entries
43. bushy subset DP
44. join algorithm enumeration
45. build-side enumeration
46. interesting-order retention
47. outer-join constraints
48. large-join heuristic
49. join-order regression tests

### Phase O6 — Property enforcement

50. final ordering requirements
51. Sort enforcement
52. TopN alternative
53. aggregate ordering alternatives
54. merge-join properties when implemented
55. deterministic tie-breaking
56. PhysicalPlan finalization

### Phase O7 — Diagnostics

57. EXPLAIN estimates
58. optimizer trace
59. plan fingerprint
60. EXPLAIN ANALYZE q-error
61. plan regression suite

Do not begin Cascades/adaptive optimization before these phases produce strong plans reliably.

---

# 720. Optimizer Milestone 1

## LOCKED target

For single-table queries:

```text
ANALYZE
    ↓
statistics
    ↓
predicate selectivity
    ↓
SeqScan vs IndexScan costing
    ↓
physical plan
```

must work.

Required demonstration:

```text
same query shape
different literal/selectivity
    ↓
different access path where appropriate
```

---

# 721. Optimizer Milestone 2

## LOCKED target

For inner joins up to the exhaustive threshold:

```text
join graph
bushy DP
HashJoin
NestedLoop
IndexNestedLoop
```

must choose join order/algorithm by cost.

`EXPLAIN` must display estimates at every node.

---

# 722. Optimizer Milestone 3

## LOCKED target

Add physical property reasoning:

```text
ORDER BY avoidance
TopN
interesting index order
SortAggregate/streaming opportunities
MergeJoin when implementation exists
```

A non-cheapest local plan must sometimes win globally because it provides useful ordering.

---

# 723. Optimizer Milestone 4

## LOCKED target

Cost memory pressure and spill:

```text
HashJoin
HashAggregate
Sort
```

Plan decisions should respond sensibly to query-memory budget changes.

`EXPLAIN ANALYZE` must compare estimate vs actual.

---

# 724. Optimizer Milestone 5

## LOCKED target

Handle large join graphs with bounded planning time:

```text
exhaustive DP below threshold
heuristic/beam/local improvement above threshold
```

Planning time and plan quality must be benchmarked.

---

# 725. Complete Core Architecture Status

## LOCKED v1

At this point the core relational database architecture is specified end-to-end:

```text
SQL text
    ↓
Lexer / Parser
    ↓
Binder / Type System / Catalog
    ↓
Logical Relational Algebra
    ↓
Logical Rewrites
    ↓
Statistics / Cardinality Estimation
    ↓
Cost-Based Physical Optimizer
    ↓
Physical Plan / Properties
    ↓
Pipeline Builder
    ↓
Vectorized Execution
    ↓
Heap / B+ Tree
    ↓
MVCC / Locks
    ↓
Buffer Pool
    ↓
WAL / Checkpoint / Recovery
    ↓
Disk
```

The project now has a locked architectural answer for the major core-engine questions.

Further architectural work should be incremental and driven by implementation evidence.

The recommended next step is no longer another large architecture section.

The recommended next step is:

```text
BEGIN IMPLEMENTATION
```

starting from the dependency bottom:

```text
Phase 1:
    common binary utilities
    file manager
    page format
    page allocator
    slotted heap page
    tuple encoding

Phase 2:
    buffer pool + CLOCK

Phase 3:
    B+ tree

...
```

and continuously validating the actual implementation against this document.

Any implementation discovery that invalidates a locked assumption must trigger an explicit architecture revision rather than a silent local workaround.
