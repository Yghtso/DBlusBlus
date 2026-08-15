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

Initial kinds:

```text
HEAP
BTREE
FSM
CATALOG
```

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

The exact serialization must be explicit.

Minimum logical fields:

```text
magic              8 bytes
format_version     2 bytes
file_kind          2 bytes
page_size          4 bytes
file_id            4 bytes
flags              4 bytes
object_id          8 bytes
creation_epoch     8 bytes
reserved...
checksum            4 bytes
```

`object_id` means:

```text
TableId for HEAP/FSM files
IndexId for BTREE files
catalog object ID where applicable
```

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

Initial page type enum conceptually contains:

```text
SUPERBLOCK
HEAP_DATA
FSM_DATA
BTREE_INTERNAL
BTREE_LEAF
BTREE_FREE
CATALOG_DATA
```

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

Definitions:

```text
lower
    first byte after header + slot directory

upper
    first byte of tuple-data region

free bytes
    upper - lower
```

Tuple bytes grow from the end of the page downward.

Slot entries grow from the header upward.

`free_slot_head` may initially remain unused, but its format is reserved for efficient slot reuse.

`prune_hint` may initially be zero; it is reserved for future vacuum/pruning hints.

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

Initial slot states:

```text
UNUSED
NORMAL
DEAD
REDIRECT_RESERVED
```

`REDIRECT_RESERVED` and `aux` are intentionally reserved for a future HOT-like optimization.

They need not be used by the first implementation.

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

Initial candidates:

```text
HAS_NULLS
HAS_VARLEN
IS_DELETED_HINT
CHAIN_ROOT
CHAIN_MEMBER
```

Only persist a flag when it has a well-defined recovery/visibility meaning.

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

Errors must include enough context to identify:

```text
file
page
operation
errno
```

A normal page read must never silently return a partially initialized page.

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

This section defines the first production B+ tree architecture.

The tree must support:

- equality lookup,
- ordered range scans,
- composite keys,
- variable-length VARCHAR keys,
- duplicate user keys,
- unique-index enforcement by the transaction/index layer,
- concurrent readers and writers,
- page splits,
- redistribution,
- merge/root contraction,
- page reuse,
- physical tuple-version RIDs,
- future WAL/recovery integration.

The design intentionally exposes the difficult parts of a real database index instead of hiding them behind an in-memory standard-library container.

---

# 110. B+ Tree File

## LOCKED

Each index owns a page-based file:

```text
index_<index_id>.btree
```

Layout:

```text
page 0       B+ tree superblock
page 1..N    internal / leaf / free pages
```

The common file-superblock rules from the storage contract still apply.

The B+ tree superblock additionally stores at least:

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

`root_page_no`, `first_leaf_page_no`, and `last_leaf_page_no` use:

```text
INVALID_PAGE_NO
```

only before the tree is initialized.

An initialized empty tree has one empty leaf root.

---

# 111. Empty Tree Representation

## LOCKED

After index creation:

```text
height = 1
root = one leaf page
first_leaf = root
last_leaf = root
root.slot_count = 0
```

Do not represent an ordinary empty tree with a null root pointer.

This simplifies:

- lookup,
- insert,
- range-scan initialization,
- root replacement,
- recovery invariants.

---

# 112. Index Key Schema

## LOCKED

Every B+ tree has a fixed index-key schema stored in catalog metadata and fingerprinted in the B+ tree superblock.

The key schema defines:

- indexed columns,
- SQL types,
- nullability,
- collation,
- sort direction.

Version 1 supports:

```text
ascending columns only
NULLS FIRST
binary VARCHAR collation
```

Descending SQL output can initially use:

- an executor sort, or
- later reverse index scans.

Per-column descending storage order and locale-aware collations are deferred.

---

# 113. User Key vs Physical Key

## LOCKED

Distinguish two concepts.

### User key

The SQL-visible index key.

Example:

```text
(last_name, birth_date)
```

### Physical key

The total-order key used internally by the B+ tree:

```text
(user_key, RID)
```

The RID is a deterministic tiebreaker.

Therefore every physical B+ tree entry is unique even when many rows share the same SQL key.

This single decision greatly simplifies:

- duplicates,
- splits where equal user keys span pages,
- lower/upper bounds,
- internal separators,
- exact entry deletion.

---

# 114. Persistent RID Encoding Inside Indexes

## LOCKED: 16 bytes

Persist an RID in an index as:

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

Normal storage serialization uses explicit field encoding.

For physical-key ordering, RID comparison is numeric lexicographic order:

```text
(heap_file_id, heap_page_no, heap_slot_id)
```

The reserved field is ignored for ordering.

---

# 115. Order-Preserving User-Key Encoding

## LOCKED: memcomparable encoding

The B+ tree does not repeatedly invoke a general SQL expression comparator inside every binary-search comparison.

Instead, `IndexKeyCodec` converts a logical index key into an order-preserving byte sequence.

For supported v1 types:

```text
encoded_key_A <lexicographically> encoded_key_B
```

must mean exactly the same ordering as:

```text
SQL/index comparator says A < B
```

under the index's fixed v1 semantics.

This is intentionally more work than storing arbitrary serialized values, because it teaches how index representation and comparison semantics interact and makes hot comparisons much cheaper.

---

# 116. Index Field Encoding

## LOCKED

Each composite-key field begins with a one-byte null marker:

```text
0x00 = NULL
0x01 = non-NULL
```

This implements:

```text
NULLS FIRST
```

for ascending indexes.

After the non-null marker, encode by type.

### BOOLEAN

```text
0x00 = false
0x01 = true
```

### Signed INT32 / DATE

Transform signed integer:

```text
u = bits(value) XOR 0x80000000
```

then encode `u` in big-endian byte order.

### Signed INT64 / TIMESTAMP

Transform:

```text
u = bits(value) XOR 0x8000000000000000
```

then encode in big-endian order.

### FLOAT64

Use an order-preserving IEEE-754 transform.

Before encoding:

- canonicalize `-0.0` and `+0.0` to one representation,
- canonicalize all NaNs to one database-defined NaN value.

Database total order for indexed FLOAT64:

```text
-infinity
...
finite values
...
+infinity
NaN
```

The SQL comparison/type layer must use the same semantics.

Then transform the IEEE bits into sortable unsigned bytes and encode big-endian.

### VARCHAR

Version 1 uses binary bytewise collation.

For each byte:

```text
ordinary non-zero byte -> itself
0x00                   -> 0x00 0xFF
```

Terminate the VARCHAR field with:

```text
0x00 0x00
```

This preserves lexicographic string ordering while keeping the field self-delimiting.

No locale-specific normalization is performed.

---

# 117. Composite-Key Encoding

## LOCKED

Composite keys are encoded by concatenating the field encodings in schema order:

```text
field_0 || field_1 || ... || field_n
```

Because every field encoding is order preserving and self-delimiting where necessary, ordinary bytewise lexicographic comparison produces the composite index order.

This permits the hottest key-comparison path to be based primarily on:

```text
memcmp-style comparison
```

rather than repeated type dispatch.

---

# 118. Maximum Index Key Size

## LOCKED for v1

Maximum encoded **user key** size:

```text
1024 bytes
```

This limit applies after index-key encoding.

If an indexed value would exceed the limit, reject the index insertion with a clear error.

Reason:

- guarantees healthy minimum fanout on 8 KiB pages,
- avoids overflow-key complexity in the first B+ tree,
- keeps split/merge logic tractable,
- makes benchmark behavior predictable.

Later experiments may add:

- prefix truncation,
- key overflow pages,
- compressed separators,
- larger page sizes.

---

# 119. Physical-Key Comparison

## LOCKED

Compare physical keys as:

```text
1. compare encoded user-key bytes lexicographically
2. if unequal, return result
3. otherwise compare RID numerically
```

Conceptually:

```cpp
ComparePhysical(
    user_key_a, rid_a,
    user_key_b, rid_b
)
```

The tree must never use pointer identity or insertion order as a duplicate-key tiebreaker.

---

# 120. User-Key Search Bounds

## LOCKED

Define conceptual RID sentinels:

```text
MIN_RID
MAX_RID
```

They need not be persistable real RIDs.

Equality search for user key `K` becomes the physical range:

```text
[K, MIN_RID] <= entry <= [K, MAX_RID]
```

Lower bound:

```text
(K, MIN_RID)
```

Upper bound:

```text
(K, MAX_RID)
```

This is how duplicates spanning multiple leaves remain correct.

---

# 121. Common B+ Tree Page Rules

## LOCKED

B+ tree node pages use:

```text
8 KiB page
32-byte common page header
64-byte total B+ tree node header
slotted variable-length entries
```

Entries are packed from the end of the page downward.

Slots grow upward from offset 64.

Like heap pages:

```text
lower = end of slot directory
upper = beginning of packed entry bytes
free_bytes = upper - lower
```

Node compaction may move entry bytes without changing logical slot order.

---

# 122. B+ Tree Slot Entry

## LOCKED: 8 bytes

Each B+ tree slot is:

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

The slot directory itself is maintained in physical-key sorted order.

The packed entry bytes do not need to be physically sorted in the page.

Therefore inserting in the middle usually requires:

- append/copy new entry bytes into free packed space,
- shift a small region of 8-byte slot descriptors,

rather than moving all existing key bytes.

---

# 123. Leaf Page Header

## LOCKED: 64 bytes total

After the 32-byte common header:

```text
offset  size  field
------  ----  ----------------
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

Leaf level is always:

```text
0
```

Invalid sibling pointers use:

```text
INVALID_PAGE_NO
```

---

# 124. Leaf Entry Layout

## LOCKED

Each leaf entry contains:

```text
encoded user key      variable bytes
RID                   16 bytes
```

Slot metadata gives:

```text
user_key_length
entry_length
```

Therefore:

```text
entry_length = user_key_length + 16
```

No transaction visibility information is stored in the B+ tree leaf.

MVCC visibility remains a heap concern.

---

# 125. Internal Page Header

## LOCKED: 64 bytes total

After the 32-byte common header:

```text
offset  size  field
------  ----  ----------------
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

The first child lives in:

```text
leftmost_child_page_no
```

Each separator entry stores the child to its right.

---

# 126. Internal Entry Layout

## LOCKED

Internal entry:

```text
encoded user key      variable bytes
separator RID         16 bytes
right_child_page_no    8 bytes
```

Therefore the separator is a complete physical key:

```text
(user key, RID)
```

and not merely the SQL user key.

This is essential when duplicate SQL keys span multiple leaves/subtrees.

---

# 127. Internal Separator Semantics

## LOCKED: routing lower bounds

For an internal node:

```text
C0, (K1,C1), (K2,C2), ... (Kn,Cn)
```

the invariant is:

```text
all physical keys in C0 are < K1

for each i > 0:
    all physical keys in Ci are >= Ki
    all physical keys in Ci are < K(i+1), when K(i+1) exists
```

A separator is a **routing lower bound** for the child to its right.

After a split it is normally equal to the first physical key in the right subtree.

However, after deletions it is allowed to remain a stale-lower bound.

It does NOT have to be continuously rewritten to equal the current minimum key of that subtree.

---

# 128. Why Separators May Be Stale-Low

## LOCKED design rationale

Suppose a separator was:

```text
K = 50
```

and the minimum key in the right subtree was 50.

If key 50 is deleted and the new minimum becomes 60, keeping separator 50 is still safe:

```text
all right-subtree keys >= 60 >= 50
```

Searches for values 50..59 may visit the right subtree and find nothing.

That is a small false-routing cost, not a correctness error.

This avoids parent-page rewrites on many ordinary deletes.

A separator must be updated when redistribution or another structural operation would otherwise move a key across its routing boundary.

---

# 129. Internal Search Rule

## LOCKED

Given target physical key `T`, find:

```text
upper_bound(separator_keys, T)
```

Conceptually:

```text
T < K1            -> C0
K1 <= T < K2      -> C1
K2 <= T < K3      -> C2
...
Kn <= T           -> Cn
```

Binary search must operate on the sorted slot directory.

---

# 130. Leaf Search Rule

## LOCKED

Within a leaf:

```text
lower_bound(physical_keys, target)
```

using binary search over slots.

For exact physical lookup:

```text
target = (encoded_user_key, RID)
```

For equality on only the SQL key:

```text
target = (encoded_user_key, MIN_RID)
```

and then scan forward until the SQL key changes.

---

# 131. Page Compaction

## LOCKED

B+ tree pages may accumulate fragmented packed-entry space after deletions.

Before deciding that a page cannot accept an entry:

```text
if contiguous free space is insufficient
but total reclaimable space is sufficient:
    compact the page
```

Compaction:

- preserves slot ordering,
- rewrites entry offsets,
- does not change tree ordering,
- holds the page write latch.

---

# 132. Leaf Split Policy

## LOCKED: split by bytes, not entry count

When a leaf cannot fit a new entry after compaction:

1. include the pending entry in the conceptual sorted entry set,
2. choose a split boundary that makes used bytes on both pages as close to 50/50 as practical,
3. require at least one entry on each side,
4. keep lower physical keys in the old/left page,
5. move higher physical keys to the new/right page,
6. parent separator is the first physical key of the new right page.

The separator is **copied** into the parent.

It remains present in the leaf.

Because entry sizes vary, splitting by entry count is explicitly incorrect for this design.

---

# 133. Leaf Sibling Update on Split

## LOCKED

If leaf `L` splits and creates `R`:

Before:

```text
P <-> L <-> N
```

After:

```text
P <-> L <-> R <-> N
```

Update:

```text
R.prev = L
R.next = old L.next
L.next = R

if N exists:
    N.prev = R
else:
    superblock.last_leaf = R
```

The first-leaf pointer changes only if the tree was previously empty in a special initialization path; ordinary split does not change it.

---

# 134. Internal Split Policy

## LOCKED

Internal representation:

```text
C0, K1->C1, K2->C2, ... Kn->Cn
```

When splitting around promoted separator `Km`:

### Left node

Keeps:

```text
C0
K1->C1
...
K(m-1)->C(m-1)
```

### Promoted to parent

```text
Km
```

### Right node

Its:

```text
leftmost_child = Cm
```

and it keeps:

```text
K(m+1)->C(m+1)
...
Kn->Cn
```

Unlike a leaf split, the promoted internal separator is removed from the child level.

Choose `m` by byte usage so the two resulting nodes are as balanced as practical.

---

# 135. Root Split

## LOCKED

If the root splits:

1. allocate a new root internal page,
2. old root becomes the left child,
3. new split page becomes the right child,
4. root receives one separator,
5. new root level = old root level + 1,
6. update B+ tree superblock root and height.

Ordinary readers/writers must not observe a half-installed root.

Root metadata replacement is serialized by the tree's root-metadata latch.

---

# 136. Root Contraction

## LOCKED

After deletion/merge:

If an internal root has:

```text
0 separator entries
```

then its single child becomes the new root.

The old root page can be retired/recycled after the structural change is safely installed.

If the root is a leaf and becomes empty:

```text
keep that empty leaf as the root
height = 1
```

Do not shrink an empty tree to "no root".

---

# 137. Occupancy Metric

## LOCKED

For variable-length nodes, occupancy is measured in **bytes**:

```text
used_bytes =
    slot_bytes
    +
    packed_entry_bytes
```

relative to usable node capacity:

```text
PAGE_SIZE - 64
```

Do not use only:

```text
slot_count / max_slot_count
```

because keys are variable length.

---

# 138. Underflow Policy

## LOCKED

Non-root pages become rebalance candidates when byte occupancy falls below approximately:

```text
25%
```

This is a practical threshold rather than a strict textbook 50% invariant.

Reason:

- variable-length keys make strict half-full guarantees awkward,
- aggressive merging causes write amplification and split/merge oscillation,
- sparse nodes are a performance concern, not an immediate correctness failure.

The tree remains correct while temporarily below the threshold.

---

# 139. Rebalance Preference

## LOCKED

For an underfull node:

1. inspect an adjacent sibling under the same parent,
2. prefer redistribution when it can produce healthy occupancy without pathological movement,
3. otherwise merge if the combined entries fit in one page,
4. propagate parent underflow upward if a parent separator is removed.

After redistribution, update the relevant parent routing separator because entries crossed a routing boundary.

After a simple deletion with no redistribution, separator tightening is not required.

---

# 140. Leaf Redistribution

## LOCKED

When moving entries between adjacent leaves:

```text
left | separator | right
```

the parent separator after redistribution must become a valid lower bound for `right`.

The simplest correct choice is:

```text
new separator = first physical key currently in right
```

This update is required whether movement was:

```text
left -> right
```

or:

```text
right -> left
```

because entries crossed the prior routing boundary.

---

# 141. Leaf Merge

## LOCKED

Merge adjacent leaves only when their combined used bytes fit in one page.

Prefer:

```text
merge right into left
```

to make page-retirement rules deterministic.

Then:

```text
left.next = right.next
if right.next exists:
    right.next.prev = left
else:
    superblock.last_leaf = left
```

Remove from the parent the routing separator that pointed to `right`.

The detached right page becomes eligible for the B+ tree free-page mechanism after the structural modification is complete.

---

# 142. Internal Redistribution and Merge

## LOCKED

Internal redistribution/merge must preserve the separator-lower-bound invariant.

Because parent separators participate in the child key ranges, internal redistribution is not equivalent to blindly moving slot bytes.

The implementation should express these operations in terms of conceptual child/key sequences first, then serialize the resulting pages.

This is intentionally preferred over clever in-place byte manipulation until correctness is proven.

---

# 143. B+ Tree Free Pages

## LOCKED

The B+ tree maintains its own free-page list.

Superblock field:

```text
free_page_head
```

A free B+ tree page has page type:

```text
BTREE_FREE
```

and stores at least:

```text
next_free_page_no
```

Allocation order:

```text
1. reuse free_page_head when available
2. otherwise append a new page to the file
```

This is intentionally local to the B+ tree file rather than a global database extent allocator.

---

# 144. Safe Page Reuse Rule

## LOCKED

A B+ tree page may be reused only after:

- it is no longer reachable from any installed parent/root pointer,
- all structural latches required to detach it have been acquired,
- no reader can legally retain an unlatched stale page reference to it.

This relies on another locked rule:

> Tree traversal and range-scan handoff use latch coupling; callers do not retain naked B+ tree page IDs as long-lived cursors.

This permits page reuse without introducing an epoch-based reclamation system in v1.

---

# 145. Point-Lookup Concurrency

## LOCKED: read latch coupling

A point lookup descends:

```text
read-latch parent
    ↓
identify child
    ↓
pin + read-latch child
    ↓
release parent
```

Repeat until leaf.

The child must be safely pinned and latched before the parent latch is released.

This prevents structural modification from invalidating the parent-to-child handoff.

---

# 146. Write Concurrency

## LOCKED: write latch crabbing

Initial insert/delete implementation uses top-down write latch crabbing.

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

A node is "safe" when the current operation cannot force a structural change to propagate above it.

Examples:

### Insert

Safe if sufficient room exists for the operation without requiring a split.

For an internal node, safety must account for a possible separator insertion from its child.

### Delete

Safe if the deletion cannot make the node require redistribution/merge.

This protocol is deliberately simpler than a B-link or fully optimistic write algorithm while still allowing high concurrency once the path reaches safe nodes.

---

# 147. Root Metadata Latch

## LOCKED

Each B+ tree has a small root-metadata synchronization object protecting:

- root replacement,
- height replacement,
- first/last leaf changes when structurally necessary,
- free-list head changes where required.

Normal point lookup must hold this latch only long enough to safely acquire/pin the current root.

Do not hold a tree-global latch throughout normal traversal.

---

# 148. Structural Latch Ordering

## LOCKED

To avoid deadlocks:

### Vertical order

Acquire:

```text
parent before child
```

during tree descent.

### Horizontal leaf order

When a structural operation needs multiple adjacent leaves, acquire them in:

```text
left-to-right key order
```

If the operation discovers that it would need to acquire an already-passed page in the opposite direction:

```text
release and restart
```

rather than waiting while violating the order.

This rule also influences range-scan behavior.

---

# 149. Range Scan Architecture

## LOCKED: forward range scans first

Version 1 supports native ascending index range scans.

A range-scan cursor stores conceptually:

```text
current ReadPageGuard
current slot position
encoded upper bound
bound inclusivity
last physical key, when needed
```

The cursor must not expose a tuple/key pointer that outlives its current page guard.

---

# 150. Range-Scan Leaf Handoff

## LOCKED: latch-coupled forward handoff

When reaching the end of leaf `L`:

1. while still holding `L` read latch, read `L.next`,
2. if no next page, finish,
3. pin + read-latch the next leaf,
4. validate it is a leaf,
5. release `L`,
6. continue on the next leaf.

This left-to-right coupling prevents a concurrent merge from detaching/reusing the next page between pointer read and latch acquisition.

---

# 151. Reverse Range Scans

## DEFERRED, architecture-compatible

Leaf pages maintain `prev_leaf_page_no`, but native reverse scans are not required for the first B+ tree milestone.

Reason:

bidirectional latch coupling introduces additional deadlock-ordering concerns.

Initially:

```text
ORDER BY ... DESC
```

may use an executor sort.

A later milestone can add reverse cursors with an explicit restart/nonblocking latch protocol.

---

# 152. B+ Tree API Boundary

## LOCKED conceptually

The physical B+ tree should expose operations approximately like:

```cpp
Insert(encoded_user_key, Rid)
Erase(encoded_user_key, Rid)

FindPhysical(encoded_user_key, Rid)

LowerBound(encoded_user_key, RidBound)
Scan(lower_bound, upper_bound)
```

It may additionally expose:

```text
FindAllUserKey(K)
```

as a convenience built on the physical range.

The B+ tree itself does not own:

- SQL expressions,
- tuple visibility,
- unique-constraint transaction semantics.

---

# 153. IndexKeyCodec Boundary

## LOCKED

`IndexKeyCodec` owns:

- logical values -> memcomparable user key,
- encoded-key validation,
- maximum-size enforcement,
- index comparison semantics.

The B+ tree receives already encoded user-key bytes.

Therefore B+ tree page algorithms do not know whether a key represents:

```text
INT64
VARCHAR
(INT32, DATE)
...
```

This keeps storage/index structure independent from SQL expression evaluation while retaining a fast bytewise comparison path.

---

# 154. Duplicate User Keys

## LOCKED

The physical tree always permits multiple entries with the same user key because their RIDs differ.

Example:

```text
("Smith", RID 10)
("Smith", RID 21)
("Smith", RID 44)
```

They appear consecutively in RID order.

This remains true even for a SQL `UNIQUE` index because obsolete/uncommitted tuple versions may temporarily coexist physically.

Uniqueness is a higher-level transactional property.

---

# 155. Unique Index Enforcement

## LOCKED architectural boundary

A unique constraint cannot be enforced by simply asking:

```text
does this user key exist in the B+ tree?
```

because matching index entries may reference:

- dead heap versions,
- aborted versions,
- the same transaction's versions,
- in-progress conflicting transactions.

Correct uniqueness check:

```text
acquire logical unique-key lock/reservation
    ↓
scan physical index entries for user key
    ↓
fetch referenced heap tuple versions
    ↓
consult transaction/MVCC state
    ↓
detect visible or in-progress conflict
    ↓
insert if allowed
```

The exact transaction-lock protocol will be specified in the transaction architecture section.

---

# 156. UNIQUE and NULL

## LOCKED for v1

For ordinary SQL `UNIQUE` indexes:

```text
if any indexed key component is NULL:
    multiple rows with that key are allowed
```

Only fully non-NULL user keys participate in duplicate rejection.

The physical B+ tree still orders and stores NULL-containing keys normally.

---

# 157. MVCC Interaction

## LOCKED

B+ tree entries themselves are not MVCC-versioned.

Leaf:

```text
encoded user key -> RID
```

Heap tuple version:

```text
RID -> xmin/xmax/... -> visibility
```

Therefore every ordinary index scan does:

```text
B+ tree candidate
    ↓
heap fetch
    ↓
MVCC visibility check
```

This means v1 does not support a true index-only scan.

That limitation is deliberate and educational.

---

# 158. UPDATE Interaction

## LOCKED

With physical-version RIDs:

```text
UPDATE
    ↓
new heap tuple version
    ↓
new RID
    ↓
new index entry for each relevant index
```

Old index entries remain until vacuum.

Even if the indexed user key did not change, v1 still inserts an entry for the new RID.

A later HOT-like optimization may avoid that work when safe.

---

# 159. DELETE Interaction

## LOCKED

Logical SQL DELETE changes heap MVCC metadata.

It does not synchronously remove all B+ tree entries for the tuple version.

Vacuum eventually removes index entries once the corresponding heap version is globally dead.

This keeps transactional DELETE from doing unnecessary structural index work on the critical path.

---

# 160. Vacuum Index Cleanup

## LOCKED

Before reclaiming a dead heap tuple version, vacuum can derive each indexed user key from the tuple and issue exact physical deletion:

```text
Erase(encoded_user_key, dead_RID)
```

Because the physical key includes RID, the removal is unambiguous even with duplicate SQL keys.

Alternative index-driven cleanup can be explored later.

---

# 161. Index Scan Heap Locality

## LOCKED design awareness

Secondary-index RID order does not imply heap-page locality.

A range scan may therefore cause many random heap accesses.

The executor/buffer layer should expose this cost to the optimizer rather than assuming every index range is cheap.

Future experiments may add:

- RID batching,
- heap-page sorting of candidate RIDs where semantics permit,
- covering indexes,
- clustered storage.

---

# 162. Separator Prefix Compression

## DEFERRED

Internal separators initially store full encoded user keys plus separator RID.

Do not implement prefix truncation/compression in the first tree.

Later, a high-value optimization is to replace full separators with the shortest byte prefix that still separates the left and right key ranges.

Benefits:

- higher internal-node fanout,
- shallower trees,
- fewer cache misses.

This is explicitly retained as a future performance project.

---

# 163. Leaf Prefix Compression

## DEFERRED

Leaf keys are initially stored in full.

Later candidates:

- prefix compression within a page,
- restart points,
- suffix compression for composite keys.

Do not add compression before split/merge/recovery correctness is established.

---

# 164. Internal Binary Search

## LOCKED

Node lookup uses binary search over the slot directory.

Do not linearly scan internal nodes in the production path.

Because key encoding is memcomparable, comparison can usually use:

```text
common-prefix check / memcmp
```

with RID comparison only when user-key bytes are equal.

A later optimization may store short key prefixes in slot metadata or side arrays.

---

# 165. Leaf Binary Search

## LOCKED

Use binary search for the initial position inside a leaf.

Sequential iteration is then used for:

- duplicates,
- range continuation.

Do not repeatedly binary-search for each key inside one range scan.

---

# 166. Root and Internal-Page Caching

## LOCKED: buffer pool only initially

Do not create a second hidden cache for root/internal B+ tree pages.

They are normal buffer-pool pages.

Because upper tree levels are frequently accessed, the normal replacement policy should naturally keep them resident.

If profiling later proves root/internal eviction harmful, introduce explicit hot-page hints rather than bypassing the buffer manager.

---

# 167. Prefetch

## DEFERRED, architecture-compatible

Range scans should later be able to prefetch:

```text
next_leaf_page_no
```

through the buffer/I/O layer.

The leaf-sibling architecture is intentionally compatible with asynchronous read-ahead.

Do not block the first implementation on async I/O.

---

# 168. Structural Modifications and User Transactions

## LOCKED conceptual model

Distinguish:

```text
logical index action
```

from:

```text
structural modification operation (SMO)
```

Example insert:

```text
logical action:
    add physical key (K,RID)

possible SMO:
    split leaf
    split parent
    replace root
```

If the user transaction later aborts, the logical entry may need to be undone.

The fact that pages were split does not need to be undone.

The tree is allowed to remain in the new valid shape.

---

# 169. WAL Model for B+ Tree Structural Changes

## LOCKED direction; exact record bytes deferred to WAL design

B+ tree structural modifications will be treated as recovery-safe **system structural actions** rather than ordinary user-visible state that must be physically rolled back.

Target behavior:

```text
user txn aborts
    -> remove/reverse its logical index entry

page split/root shape change
    -> may remain
```

This is analogous in spirit to nested top actions/system transactions used by mature recovery designs.

Exact WAL record types and mini-transaction protocol will be locked during the WAL/recovery architecture stage.

---

# 170. Page LSN Requirement

## LOCKED

Every modified B+ tree page participates in the same page-LSN/WAL ordering contract as heap pages.

This includes:

- ordinary entry insert/delete,
- split,
- merge,
- sibling-link changes,
- root page changes,
- free-page list changes.

The buffer pool remains the enforcement point for:

```text
WAL durable through page_lsn
before
dirty page write
```

---

# 171. B+ Tree Superblock Durability

## LOCKED

Changes to:

```text
root_page_no
height
first_leaf
last_leaf
free_page_head
```

must eventually be WAL/recovery protected.

Do not rely on "write the superblock immediately and fsync" for every structural change.

That would violate the performance goals and NO-FORCE architecture.

---

# 172. Structural Change Atomicity

## LOCKED invariant

At runtime, concurrent tree operations must never observe a state in which:

- a new child is reachable without its initialized page contents,
- a child is detached while its replacement routing is absent,
- a root points to an uninitialized page,
- a leaf sibling pointer exposes a page that is not yet initialized as a leaf.

Implementation should order latch-protected in-memory modifications so every published pointer leads to a valid node.

Crash atomicity will be provided by WAL/recovery.

---

# 173. Page Validation

## LOCKED

Opening a B+ tree page validates at least:

```text
page type
format version
page_no self-check
header_size
lower/upper bounds
slot directory bounds
entry bounds
user_key_length <= entry_length
level/type consistency
sorted physical-key order in debug/verifier mode
```

Corrupt pages must produce a corruption error, not undefined behavior.

---

# 174. Tree Verifier

## LOCKED

Implement a debug/offline verifier early.

It should be able to walk the entire tree and assert:

- every internal child is reachable exactly where expected,
- all leaf entries are globally sorted,
- internal routing lower-bound invariants hold,
- all leaves are at the same level,
- sibling chain order matches tree order,
- first/last leaf metadata is correct,
- no reachable page is on the free list,
- no obvious orphaned allocated page exists where detectable.

Run the verifier aggressively in randomized tests.

---

# 175. B+ Tree Milestone 1

## LOCKED target

Before concurrency and WAL:

```text
single-threaded
page-backed
buffer-pool-backed
persistent B+ tree
```

must support:

```text
create
reopen
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

It must operate through the real buffer pool.

Do not first implement the production tree as an in-memory pointer tree and later port it to pages.

---

# 176. B+ Tree Milestone 2

## LOCKED target

Add:

```text
read latch coupling
write latch crabbing
concurrent point lookups
concurrent inserts
concurrent deletes
forward range scans during writes
deadlock/restart tests
```

Do not add lock-free algorithms before this is correct and measured.

---

# 177. B+ Tree Milestone 3

## LOCKED target

Integrate:

```text
transactional uniqueness semantics
MVCC heap visibility
vacuum index cleanup
WAL
crash recovery
```

Only at this stage does the index become fully transactionally durable.

---

# 178. Required B+ Tree Tests

## LOCKED

### Deterministic structural tests

- insert without split,
- leaf split,
- cascading internal split,
- root split,
- redistribution from left,
- redistribution from right,
- leaf merge,
- internal merge,
- root contraction,
- free-page reuse.

### Key-format tests

- INT32 ordering,
- negative/positive INT64 ordering,
- DATE/TIMESTAMP ordering,
- FLOAT64 ordering,
- `-0.0` vs `+0.0`,
- NaN canonicalization/order,
- empty VARCHAR,
- embedded zero bytes,
- composite keys,
- NULL-containing keys,
- maximum-length key,
- oversized-key rejection.

### Duplicate tests

Create enough identical user keys with different RIDs to span multiple leaf pages.

Verify:

```text
equality scan returns every RID exactly once
lower bound begins at first duplicate
upper bound ends after last duplicate
exact physical delete removes only one RID
```

### Persistence tests

- build tree,
- force buffer eviction,
- close,
- reopen,
- verify all point/range results,
- continue inserting/deleting after reopen.

---

# 179. Randomized B+ Tree Tests

## LOCKED

Compare the B+ tree against an in-memory oracle such as a sorted set/map of physical keys.

Random operation sequence:

```text
insert
erase
lookup
range
reopen
```

After intervals:

```text
run full tree verifier
compare complete ordered contents
```

Use reproducible random seeds and print the seed on failure.

This is one of the highest-value test suites in the project.

---

# 180. Concurrent B+ Tree Tests

## LOCKED

Stress with:

- many readers + one writer,
- many writers on disjoint key ranges,
- many writers on the same key range,
- duplicate-heavy inserts,
- repeated split/merge boundaries,
- forward scans during inserts/deletes.

Use small buffer pools to force buffer-latch and I/O interactions.

Include watchdogs/timeouts in tests to catch deadlocks.

---

# 181. B+ Tree Benchmarks

## LOCKED

Measure at least:

```text
random point lookup ops/sec
sorted insertion ops/sec
random insertion ops/sec
exact deletion ops/sec
short range scan rows/sec
long range scan rows/sec
duplicate-heavy lookup
split frequency
tree height
average leaf occupancy
average internal occupancy
buffer-pool hit rate
latch wait time when instrumentation exists
```

Test multiple key shapes:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

---

# 182. Benchmark Workloads That Matter

## LOCKED

Benchmark both:

### Hot index

Tree largely resident in buffer pool.

Measures:

- CPU cost,
- latch cost,
- comparison cost,
- cache behavior.

### Larger-than-buffer index

Tree/leaf working set exceeds buffer pool.

Measures:

- page access pattern,
- fanout,
- random I/O sensitivity,
- replacement behavior.

A B+ tree optimization is not "faster" in general unless we know which regime improved.

---

# 183. Deliberately Deferred B+ Tree Features

## LOCKED

Do not implement before the core tree is correct and benchmarked:

- B-link tree right-links for internal nodes,
- optimistic versioned/latch-free reads,
- lock-free tree algorithms,
- separator prefix truncation,
- leaf prefix compression,
- key overflow pages,
- native reverse scans,
- bulk loading,
- online index build,
- covering/include columns,
- index-only scans,
- partial indexes,
- expression indexes,
- descending physical key components,
- locale-aware collations,
- asynchronous prefetch,
- parallel index scans.

These are future high-value experiments.

---

# 184. First Performance Upgrade After Correctness

## LOCKED recommendation

After the latched B+ tree is correct, benchmark before changing anything.

The first likely high-reward optimization should be one of:

```text
1. separator prefix truncation
2. optimistic read traversal / B-link-style navigation
3. leaf prefetch for range scans
4. reduced latch contention around the root
```

Choose based on profiling rather than architectural fashion.

---

# 185. B+ Tree Invariants Summary

## LOCKED

Codex must preserve these invariants:

1. Every leaf is level 0.
2. Every internal child is exactly one level lower than its parent.
3. All physical keys are globally ordered by `(encoded_user_key, RID)`.
4. Leaf slot directories are sorted by physical key.
5. Internal slot directories are sorted by separator physical key.
6. Internal separators are valid lower routing bounds for right children.
7. An internal node with `N` separators has `N+1` children.
8. A leaf entry contains one encoded user key and one physical-version RID.
9. Duplicate SQL keys are legal at the physical tree layer.
10. An index hit is not an MVCC visibility decision.
11. Range scans move through the leaf chain with safe latch-coupled handoff.
12. Published child/sibling pointers always reference initialized valid pages.
13. A page is never reused while a legal traversal can still reference it.
14. Structural tree shape changes are separate from logical user-transaction undo.
15. All persistent B+ tree page modifications participate in page-LSN/WAL ordering.
16. The root is always a valid page in an initialized tree, even when empty.
17. Variable-length page split/rebalance decisions use bytes, not only entry counts.
18. Oversized encoded user keys are rejected rather than silently truncated.
19. B+ tree page algorithms operate only through buffer-pool-managed pages.
20. The full tree can be validated by an explicit verifier.

---

# 186. Architecture Decisions Added by the B+ Tree Design

## LOCKED

The architecture now additionally commits to:

```text
page-backed B+ tree
variable-length slotted node pages
64-byte node headers
8-byte node slots
memcomparable user-key encoding
1024-byte maximum encoded user key
physical ordering = (user key, RID)
full physical separator keys in internal nodes
routing-lower-bound separator semantics
binary search within nodes
byte-balanced splits
25% soft underflow threshold
redistribution before merge where useful
forward doubly-linked leaf chain
forward range scans first
read latch coupling
write latch crabbing
left-to-right sibling latch ordering
tree-local free-page list
B+ tree entries contain no MVCC metadata
unique enforcement above the physical tree
structural modifications survive user abort
future WAL treatment as recovery-safe system structural actions
```

No unresolved B+ tree design choice currently requires project-owner input.

---

# 187. Next Architecture Topic

The next architecture stage should design the transaction/durability core together:

```text
TransactionManager
Snapshot
VisibilityManager
LockManager
WAL
Commit Coordinator
Checkpointing
Recovery
Vacuum Horizon
```

These should be designed together rather than independently because choices in one directly constrain the others.

The most important upcoming decisions will include:

- exact transaction-ID lifecycle,
- snapshot representation,
- visibility rules using `xmin/xmax/cmin/cmax`,
- aborted/committed transaction status storage,
- write/write conflict behavior,
- unique-key locking,
- isolation-level semantics,
- WAL record structure,
- physiological vs logical logging boundaries,
- group commit protocol,
- checkpoint records,
- redo/undo algorithm,
- compensation log records,
- B+ tree structural-action recovery,
- transaction-ID wraparound strategy,
- vacuum's global-safe horizon.
