# DBlusBlus Architecture

**Status:** Rewrite in progress  
**Architecture version:** v1 contract under structural rewrite  
**Active authority during rewrite:** the existing `ARCHITECTURE.md` remains authoritative until this document has completed semantic reconciliation and is explicitly adopted.

## Purpose

This document defines the technical architecture of DBlusBlus: a from-scratch, single-node relational database management system implemented in C++20.

It specifies architectural responsibilities, subsystem boundaries, persistent-format requirements, concurrency and lifetime rules, correctness invariants, and performance-relevant design constraints.

Project progress belongs in `PROJECT_STATE.md`. Historical implementation records belong in `devlog/`. Development-agent instructions do not form part of this architecture contract.

## Contract language

The original architecture uses `LOCKED` to mark hard requirements. During this rewrite those requirements are expressed using ordinary normative language:

- **MUST / MUST NOT** — required by the current architecture contract.
- **SHOULD / SHOULD NOT** — the architectural default; deviation requires a concrete technical reason and must remain compatible with all MUST-level requirements.
- **MAY** — explicitly permitted implementation freedom.
- **Deferred** — intentionally outside the current baseline; it is not part of the required v1 implementation unless later promoted by an explicit architecture revision.

A rewrite pass may reorganize, consolidate, or clarify existing requirements, but it does not change their meaning. Deliberate architecture changes are handled separately from structural rewriting.

---

# Part I — Foundations

# 1. Scope and Design Goals

## 1.1 System goal

DBlusBlus is a serious single-node relational database intended to expose and implement the core mechanisms of a relational engine rather than delegate them to external database frameworks.

The architecture is intended to support, end to end:

- persistent page-oriented storage,
- row-oriented heap tables,
- B+ tree indexes,
- transactions and multi-version concurrency control,
- write-ahead logging and crash recovery,
- vacuum and physical reclamation,
- a relational system catalog,
- a limited SQL front end,
- logical and physical planning,
- vectorized execution,
- statistics and cardinality estimation,
- cost-based optimization,
- concurrent transactions and, later, parallel query execution.

The system SHOULD remain small enough that one contributor can understand the complete path from SQL text to persistent bytes while still implementing realistic database-system mechanisms.

## 1.2 Design objectives

The architecture prioritizes:

1. **Correctness.** Persistent state, concurrency, transaction semantics, and recovery MUST be designed around explicit invariants rather than incidental behavior.
2. **Architectural clarity.** Subsystems MUST have clear ownership and dependency boundaries.
3. **Performance visibility.** CPU cache behavior, allocation, synchronization, I/O patterns, cardinality, intermediate-result size, and durability costs MUST remain measurable architectural concerns.
4. **Mechanism transparency.** Core database mechanisms such as page management, replacement, tuple storage, B+ trees, MVCC, WAL/recovery, execution, and optimization are intended to be implemented directly rather than hidden behind a database framework.
5. **Evolution without throwaway foundations.** Initial implementations MAY be simple, but the architecture SHOULD avoid choices that require replacing an entire subsystem merely to make measured performance improvements later.

The project deliberately accepts additional systems complexity where that complexity is inherent to the database mechanism being studied, provided the resulting engine remains realistically implementable.

## 1.3 Non-goals for the initial major version

The initial major version does not target:

- distributed consensus,
- replication,
- sharding,
- multi-node execution,
- cloud-native storage,
- full PostgreSQL or MySQL SQL compatibility,
- stored procedures,
- triggers,
- user-defined extensions,
- sophisticated authentication or authorization,
- columnar storage as the primary table representation,
- JIT query compilation,
- GPU execution.

These areas are outside the initial architecture scope and MAY be explored after the single-node engine is mature.

## 1.4 Selected foundational decisions

Two foundational choices are part of the v1 architecture baseline:

- the implementation language is **C++20**;
- transactional storage uses **heap-version MVCC**.

Changing either decision is a major architecture revision because each affects multiple subsystems, persistent behavior, and implementation strategy.

Any future proposal to replace one of these decisions MUST document at least:

- the current and proposed design,
- the correctness or performance motivation,
- affected subsystems,
- compatibility implications,
- migration cost,
- relevant benchmark evidence when the motivation is performance-related.

## 1.5 Design intent

The architecture is not optimized for the shortest path to a database that merely accepts SQL.

Where two approaches are otherwise viable, the design favors the one that exposes an important database-system mechanism rather than hiding it, so long as correctness, performance potential, and project feasibility remain acceptable.

This intent is rationale for the selected architecture; it does not override concrete subsystem contracts.

---

# 2. System Architecture and Dependency Model

## 2.1 Layered system model

The system follows a downward dependency model:

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
WAL + Page / File Management
    ↓
Operating System / Storage Device
```

The diagram represents dependency direction, not necessarily one runtime call stack.

Lower layers MUST NOT depend on higher-layer syntax or semantic objects.

Examples:

- B+ tree code MUST NOT require SQL table syntax or parser objects.
- Buffer management MUST NOT require tuple-schema interpretation.
- WAL MUST NOT depend on SQL statement kinds such as `SELECT`.
- Parser code MUST NOT depend on physical page layout.
- Physical page parsers MUST NOT own transaction-visibility policy.
- SQL name resolution MUST NOT occur inside execution hot loops.

## 2.2 Separation of logical and physical concerns

The architecture maintains explicit boundaries between:

- SQL syntax and bound semantic objects,
- logical relational operators and physical algorithms,
- transaction-level logical locks and short-lived in-memory latches,
- persistent page identity and in-memory buffer-frame identity,
- tuple-format interpretation and page-local byte management,
- optimizer estimates and actual execution metrics.

These distinctions are cross-cutting requirements. Later chapters define their concrete representations and protocols.

## 2.3 Correctness and performance model

Correctness precedes micro-optimization, but performance-sensitive structure MUST remain visible from the start.

Architectural and performance analysis SHOULD explicitly consider:

- CPU cache behavior,
- branch prediction,
- allocation frequency,
- memory layout,
- pointer chasing,
- synchronization and contention,
- sequential versus random I/O,
- buffer-pool hit rate,
- WAL and durable-flush frequency,
- query cardinality,
- intermediate-result size,
- query memory pressure and spill behavior.

Measured hot paths MAY justify more complex algorithms or representations. Complexity SHOULD NOT be introduced solely from intuition when a simpler architecture satisfies the same correctness requirements and preserves a viable optimization path.

## 2.4 Shared-state and concurrency direction

The engine is single-process and multi-threaded.

Centralized global locks SHOULD NOT be used in hot paths merely to simplify synchronization.

The architecture must permit progressively partitioned shared state, including:

- sharded buffer-pool lookup structures,
- per-page latches,
- partitioned logical lock management,
- thread-local or query-local arenas,
- coordinated WAL append state.

The first correct implementation of a subsystem MAY use simpler synchronization where the later removal of contention does not require replacing the subsystem's ownership model.

---

# 3. Platform and Runtime Baseline

## 3.1 Supported platform

The initial supported environment is:

- Linux,
- x86-64 or ARM64,
- POSIX file APIs,
- a single database process,
- multiple worker threads.

Portability is desirable but secondary to correctness, observability, and direct access to the required operating-system mechanisms.

The Linux-first baseline permits later experimentation with facilities such as `io_uring`, direct I/O, huge pages, and NUMA-aware designs without making those facilities requirements of the initial architecture.

## 3.2 Implementation language

The implementation MUST use C++20.

Compiler-specific extensions are not part of the architectural language baseline unless a later explicit decision introduces one.

C++20 was selected because it provides direct control over:

- object lifetime and ownership,
- memory layout,
- allocation behavior,
- cache-local data structures,
- atomics and synchronization,
- explicit byte manipulation,
- POSIX/Linux I/O,
- low-level profiling and optimization.

The design accepts the associated risks—undefined behavior, memory-safety failures, accidental allocation/copying, and difficult concurrency bugs—as engineering risks that must be controlled by explicit ownership, serialization, validation, testing, and sanitizer/tooling discipline.

---

# Part II — Storage and Persistence

# 4. Persistent Storage Foundations

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§6, 7, and 9. The exact storage contract is migrated in Pass 2.

## 4.1 Page-oriented persistence

Persistent random-access database structures MUST be page-oriented.

The initial page size is:

```text
8192 bytes (8 KiB)
```

Page size MUST be represented by a canonical constant or configuration value rather than duplicated assumptions.

Persistent references MUST use logical identifiers such as page IDs; persistent process pointers are prohibited.

The exact identifier widths, sentinels, page-addressing rule, page headers, page types, file superblocks, checksums, and page-allocation contract are defined by the concrete storage-foundation chapter migrated in Pass 2.

## 4.2 File organization

Major persistent objects use separate files rather than one monolithic database file.

The architecture includes distinct storage for objects such as:

```text
catalog metadata / catalog pages
table heap files
index B+ tree files
WAL
```

Heap free-space metadata is also architecturally separate from the heap data file; its concrete contract is defined in the free-space chapter.

This separation provides explicit object ownership, independent file lifecycle, natural `(file, page)` addressing, and an independently append-only WAL.

A future storage manager MAY introduce file segmentation. Segmentation is not required by the initial architecture.

## 4.3 Explicit serialization

Persistent structures MUST use explicit binary encoding.

Persistent formats MUST define, where applicable:

- byte order,
- fixed field widths,
- byte offsets,
- format versions,
- checksums and checksum coverage,
- reserved-field rules,
- validation requirements.

The initial persisted integer byte order is:

```text
little-endian
```

Raw C++ object layout, compiler padding, native alignment, and native endianness MUST NOT define persistent representation.

Every persisted page type MUST carry or otherwise participate in an explicit format-version contract.

---

# 5. Heap Storage and Tuple Format

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§10–12. Exact heap and tuple formats are migrated in Pass 3.

## 5.1 Table storage model

Primary table storage is row-oriented heap storage.

This choice supports general-purpose/OLTP behavior, physical tuple-version MVCC, page-local tuple management, and direct interaction with B+ tree indexes.

Columnar storage is outside the initial table-storage architecture.

## 5.2 Slotted heap pages

Heap data pages use a slotted-page design with:

```text
page metadata
slot directory
contiguous free-space region
tuple bytes
```

A physical tuple version is addressed by:

```text
RID = (PageId, SlotId)
```

Compacting tuple bytes within one page MUST preserve `SlotId`; ordinary page compaction MUST therefore preserve the physical RID.

The exact common page header, heap-specific header, slot-entry format, slot states, free-space geometry, and compaction rules are defined by the detailed heap contract migrated in Pass 3.

## 5.3 Compact binary tuples

Stored tuples use a compact schema-directed binary representation.

The physical representation supports the initial scalar family:

- BOOLEAN,
- INT32,
- INT64,
- FLOAT64,
- DATE,
- TIMESTAMP,
- VARCHAR,
- NULL values through nullability metadata.

Tuple representation MUST:

- use a null bitmap,
- avoid one heap allocation per stored column/value,
- permit schema-directed interpretation,
- represent variable-length fields using tuple-local offsets/descriptors,
- remain compatible with page-local storage and MVCC tuple headers.

Very large values requiring overflow/TOAST-style storage are deferred from the initial tuple format.

---

# 6. Free-Space Management and Physical Reclamation

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §13. The persisted/runtime FSM contract and page-local reclamation rules are migrated in Pass 4.

Free-space discovery is a distinct storage subsystem.

Heap insertion MUST NOT require a linear scan of every heap page merely to find a page with sufficient insertion space.

The initial design uses explicit approximate free-space metadata, such as a free-space map or bucketed directory, to identify candidate pages.

Free-space metadata MAY be approximate or stale when the concrete contract permits it; actual page suitability remains subject to the page-level insertion rules.

The exact persisted FSM representation, runtime candidate policy, repair semantics, and interaction with compaction/vacuum are defined in later sections of this chapter.

---

# 7. I/O and Buffer Management

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§8, 14, and 15. The concrete I/O, BufferPool, frame, guard, flushing, and ownership contracts are migrated in Pass 5.

## 7.1 Explicit I/O

The initial database storage path uses explicit file I/O such as:

```text
pread
pwrite
fdatasync / fsync
```

Memory-mapped database pages are not the primary storage architecture.

The database must retain explicit control over:

- caching,
- eviction,
- dirty-page flushing,
- WAL-before-data ordering,
- page lifetime,
- future prefetch policy.

Later experiments MAY include:

- `io_uring`,
- asynchronous prefetch,
- direct I/O,
- background writeback,
- deliberate Linux page-cache bypass.

These are not baseline requirements.

## 7.2 Database-managed buffer pool

Normal execution/storage page access MUST pass through an explicit database-managed buffer pool once the buffer layer exists.

A buffer frame conceptually tracks at least:

```text
page identity
page bytes
pin count
dirty state
page LSN
page latch
replacement metadata
```

The architecture requires operations equivalent in responsibility to:

```text
fetch an existing page
create/access a newly allocated page
release a pin
flush one page
flush a file/object
remove a page when allowed
```

The concrete API is implementation-specific and is defined in the detailed BufferPool contract.

Page lifetime SHOULD be expressed through RAII-style guards so pin release is coupled to object lifetime and accidental pin leaks are difficult.

## 7.3 Replacement policy

The initial replacement family is CLOCK.

CLOCK is selected as the baseline because it provides realistic replacement behavior with substantially less bookkeeping than exact LRU.

The replacement component SHOULD have a separable policy boundary so alternative policies can be measured later.

Deferred replacement experiments include:

- CLOCK-Pro,
- LRU-K,
- 2Q,
- scan-resistant policies.

---

# Part III — Indexing

# 8. B+ Tree Indexing

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§16–17. The complete B+ tree contract is migrated in Pass 6.

The primary general-purpose index is a page-backed B+ tree implemented by the database.

The architecture requires eventual support for:

- point lookup,
- insertion,
- deletion,
- node split,
- merge/rebalance,
- ordered range scan,
- sibling-linked leaves,
- unique and non-unique SQL indexes,
- concurrent traversal.

Index leaf entries reference physical heap tuple-version RIDs.

For non-unique keys, the physical ordering MUST include RID as a deterministic tie-breaker so duplicate SQL keys remain individually addressable.

B+ tree structural synchronization uses short-lived page/tree latches and MUST remain distinct from transaction-level logical locks.

The baseline concurrent-write design uses latch coupling/crabbing. More optimistic algorithms are deferred.

The exact persistent node formats, key encoding, separator semantics, split/rebalance rules, sibling-link protocol, latching order, free-page reuse, verifier, and WAL interaction are defined by the B+ tree contract migrated in Pass 6.

---

# Part IV — Transactions and Durability

# 9. Transaction Lifecycle and Snapshots

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §19. The concrete transaction/snapshot contract is migrated in Pass 7.

Transactions receive transaction identifiers and snapshots.

The core transaction states are:

```text
ACTIVE
COMMITTED
ABORTED
```

Transaction management owns the lifecycle of:

- transaction identifiers,
- the active-transaction registry,
- snapshots,
- commit/abort state,
- write-set/conflict state required by the concrete protocol.

The initial isolation targets are:

1. Read Committed,
2. Snapshot-Isolation / Repeatable-Read-style semantics as concretely defined by the transaction contract.

True serializable isolation is deferred until the base MVCC engine is correct.

Candidate later mechanisms include Serializable Snapshot Isolation, predicate locking, and key-range locking.

---

# 10. MVCC Visibility and Tuple-Version Semantics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §18. Exact tuple-header visibility rules are migrated in Pass 7.

The v1 architecture uses heap-version MVCC.

An UPDATE creates a new physical heap tuple version rather than modifying the row in place as one permanent physical object.

Conceptually:

```text
old version:
    xmin = creator transaction
    xmax = updating transaction

new version:
    xmin = updating transaction
    xmax = no invalidating transaction
```

This model intentionally exposes:

- transaction snapshots,
- creator/deleter visibility,
- old and new physical versions,
- dead tuple versions,
- version-chain traversal,
- storage bloat,
- vacuum,
- index/version interaction.

The architecture accepts additional heap growth and vacuum pressure in exchange for explicit and inspectable MVCC semantics.

Future experiments with alternative MVCC models SHOULD remain possible, but v1 behavior is defined by the heap-version model.

---

# 11. Logical Locking and Write Conflicts

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §20. The detailed lock/conflict protocol is migrated in Pass 7.

Transaction-level logical locking MUST NOT be implemented as one database-wide mutex.

The architecture must support sufficiently fine-grained write-conflict control, including tuple/row write protection and, where required by later features, table-intention or key/range locking.

Logical locks MAY be held for transaction lifetime.

Logical locks and in-memory latches are separate mechanisms and MUST NOT be treated as interchangeable.

The exact lock identities, compatibility rules, deadlock handling, unique-key locking, and write/write conflict semantics are defined by the detailed transaction/locking contract.

---

# 12. Write-Ahead Logging and Commit Durability

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§21–23. The complete WAL and commit contract is migrated in Pass 8.

## 12.1 WAL requirement

Write-ahead logging is mandatory.

The central durability ordering rule is:

> A persistent data page MUST NOT become durable with a WAL-protected modification before the WAL records required to recover that modification are durable.

Every WAL record has an LSN, and every WAL-protected dirty page tracks a page LSN according to the concrete page/WAL contract.

The log must represent the transaction and page/structural information required by the concrete recovery design, including transaction completion and persistent page changes.

The detailed v1 recovery design later refines which operations require physical undo; the generic early architecture overview is not an independent requirement to physically undo ordinary user DML.

## 12.2 Group commit

Commit durability uses group commit.

Transactions SHOULD share durable WAL flushes rather than each requiring an independent filesystem synchronization.

Conceptually:

```text
transaction A ┐
transaction B ├─> WAL buffer -> durable WAL flush -> commits become durable/visible
transaction C ┘
```

The exact relationship between durable LSN, transaction status, commit acknowledgement, and visibility is defined by the detailed transaction/WAL contract.

## 12.3 Buffer policy

The database uses:

```text
STEAL + NO-FORCE
```

**NO-FORCE:** transaction commit does not force every modified data page to durable storage.

**STEAL:** buffer management may write/evict a dirty page containing changes from an uncommitted transaction, provided WAL ordering and recovery semantics make the write safe.

These policies are architectural requirements and are the reason crash recovery cannot be reduced to simply flushing all data pages at commit.

---

# 13. Checkpointing and Crash Recovery

> **Rewrite status:** Pass 1 establishes the non-conflicting architectural baseline from legacy §§24–25. The concrete v1 recovery algorithm is migrated in Pass 8.

Crash recovery is ARIES-inspired and uses:

- LSNs,
- per-page LSNs,
- durable transaction state,
- checkpoints,
- redo of changes required to reconstruct durable state.

The legacy high-level description used the generic phrase `analysis / redo / undo`. A later concrete v1 contract explicitly refines that model for heap-version MVCC, so Pass 1 does not preserve generic physical user-DML undo as an independent normative requirement.

The concrete recovery phases, loser-transaction handling, structural-action treatment, and any operation-specific undo requirements are defined in the detailed recovery contract migrated in Pass 8.

Checkpoint architecture MUST be compatible with fuzzy/background checkpointing and MUST NOT fundamentally require stopping the entire database to create a checkpoint.

A simpler initial checkpoint implementation MAY be used if the persistent metadata and WAL format remain compatible with the later fuzzy checkpoint model.

---

# 14. Vacuum and Storage Reclamation

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §26. The complete vacuum/RID-reclamation protocol is migrated in Pass 9.

Heap-version MVCC requires garbage collection of dead physical tuple versions.

Vacuum uses a visibility/safety horizon derived from active transaction state to determine when old tuple versions are no longer reachable by legal snapshots.

Vacuum responsibilities include:

- identifying reclaimable tuple versions,
- reclaiming heap space,
- cleaning index entries when required,
- maintaining free-space metadata,
- participating in statistics maintenance when that functionality exists.

The first implementation MAY expose vacuum explicitly/manually.

Background vacuum is deferred until the correctness of MVCC, index cleanup, reclamation, and recovery is well tested.

---

# 15. Transactional Write Protocols

The detailed INSERT, UPDATE, DELETE, COMMIT, ABORT, retry, and cross-subsystem write protocols are defined by the concrete transaction/storage architecture migrated in later passes.

Pass 1 introduces no additional write-protocol requirements beyond the MVCC, locking, WAL, STEAL/NO-FORCE, and recovery baselines above.

---

# Part V — Catalog and SQL Semantics

# 16. Catalog and Schema Metadata

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §27. The complete catalog/schema contract is migrated in Pass 10.

Persistent database metadata uses a relational system catalog.

The catalog is expected to contain system relations representing at least:

```text
tables
columns
indexes
constraints
statistics
```

A small bootstrap mechanism MAY exist to locate and interpret the catalog before catalog access becomes self-hosting.

As the engine matures, ordinary metadata lookup SHOULD increasingly use the database's own storage and execution machinery rather than an indefinitely separate metadata system.

---

# 17. SQL Type and Value System

The concrete SQL type system, coercion rules, generic planning-time values, NULL semantics, comparison semantics, and type registries are migrated from the later upper-layer contract in Pass 10.

Pass 1 adds no independent type-system rules beyond the stored physical scalar baseline in Chapter 5.

---

# 18. Lexer, Parser, and AST

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §28. The complete front-end contract is migrated in Pass 10.

The SQL front end uses an in-project limited parser rather than delegating the language front end wholesale to an external SQL parser.

The front end includes:

- tokenization/lexing,
- a hand-written parser,
- an AST,
- a separate semantic binder,
- type checking.

The initial grammar is intentionally a controlled SQL subset rather than full SQL-standard compatibility.

Parser output represents syntax. Database-object resolution belongs to binding rather than parsing.

---

# 19. Binding and Expression Semantics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §29. The complete binder/expression contract is migrated in Pass 10.

Parser and binder are separate subsystems.

Binding is responsible for semantic resolution including:

- table names,
- column names,
- aliases,
- ambiguity detection,
- type checking,
- function/operator resolution,
- wildcard expansion,
- catalog lookup.

After binding, database-object references SHOULD use stable catalog identifiers rather than unresolved strings.

The concrete scope model, expression representation, logical types, nullability, coercion rules, function/operator registry, and diagnostic requirements are defined by the detailed upper-layer contract.

---

# 20. Logical Plans, Properties, and Rewrites

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§30–31. The complete logical-plan/rewrite contract is migrated in Pass 11.

## 20.1 Logical relational representation

Queries are represented using relational-algebra-style logical operators.

The initial logical operator family includes the semantics of:

```text
scan/get
filter
project
join
aggregate
sort
limit
insert
update
delete
```

Logical plans describe relational meaning and MUST NOT commit to a physical execution algorithm.

## 20.2 Logical rewrites

Rule-based semantic rewrites occur before cost-based physical selection.

Rewrite families include:

- constant folding,
- predicate simplification,
- predicate pushdown,
- projection pruning,
- redundant-operator removal,
- join-predicate extraction/canonicalization.

Every rewrite MUST preserve SQL semantics, including NULL behavior and any later-defined volatility constraints.

Rewrite correctness must be verifiable independently of cost-based physical planning.

---

# 21. DDL/DML Semantic Planning and SQL v1 Scope

The detailed DDL/DML logical semantics, supported SQL-v1 statement set, subquery/CTE rules, catalog-visibility behavior, and upper-layer invariants are migrated in Pass 11.

Pass 1 introduces no additional requirements in this chapter.

---

# Part VI — Physical Execution

# 22. Physical Plan and Runtime Operator Model

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§35–36. The complete physical-plan and execution contract is migrated in Pass 12.

Logical and physical plans are distinct.

A logical operation may have multiple physical implementations. For example:

```text
logical scan
    -> sequential scan
    -> index scan

logical join
    -> nested-loop join
    -> index nested-loop join
    -> hash join
    -> merge join
```

The optimizer selects among valid physical algorithms.

Production execution is vectorized/chunk-at-a-time rather than fundamentally one tuple per iterator call.

Operators exchange batches conceptually represented as:

```text
DataChunk
    typed columns/vectors
    cardinality
    null/validity information
```

The initial target chunk cardinality is approximately:

```text
1024 rows
```

with a practical operating range on the order of hundreds to a few thousand rows where later measurements justify tuning.

The vectorized model is intended to amortize function dispatch, branches, synchronization, allocation, and tuple decoding across rows.

A row-at-a-time reference executor MAY exist for correctness testing if useful, but it is not the production execution architecture.

---

# 23. Vectorized Data and String Representation

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §37. The complete vector/string representation is migrated in Pass 12.

Execution chunks use typed vectors rather than one heap-allocated polymorphic value object per cell in hot loops.

Vector storage SHOULD be contiguous where practical and includes:

- type-specific data representation,
- validity/null masks,
- selection vectors where useful.

Generic scalar `Value` objects MAY be used for parser literals, catalog metadata, planning, debugging, or other non-hot scalar contexts.

Generic per-cell objects MUST NOT dominate scan, join, aggregation, or other performance-critical vectorized loops.

---

# 24. Query Memory, Row Storage, and Spill

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §41. The complete memory/spill contract is migrated in Pass 12.

Query and operator temporary memory uses query-scoped or operator-scoped arenas/pools where appropriate.

The design SHOULD avoid general-purpose allocation in inner loops.

Arena-style lifetime management is expected for data such as:

- hash-table storage,
- temporary keys,
- intermediate buffers,
- reusable vector storage.

The architecture targets bulk allocation, predictable lifetime, inexpensive reset/free, and fewer allocator calls.

The detailed memory budgeting, reservation, row collections, spill manager, temporary-file semantics, and failure behavior are defined in the later execution contract.

---

# 25. Vectorized Expression Execution

The concrete expression kernels, unified vector access, selection handling, SQL three-valued boolean evaluation, hashing, casts, and arithmetic rules are migrated in Pass 12.

Pass 1 adds no independent expression-kernel requirements.

---

# 26. Pipeline Execution Model

The concrete source/operator/sink model, pipeline breakers, dependency DAG, runtime state, cancellation, and scheduling interface are migrated in Pass 12.

Pass 1 establishes only that the production executor is vectorized and must remain compatible with later parallel execution.

---

# 27. Scans and Unary Physical Operators

The concrete sequential scan, index scan, filter, projection, limit, values, and result-sink contracts are migrated in Pass 12.

---

# 28. Join Execution

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §38. Detailed join execution is migrated in Pass 13.

The physical architecture includes:

- nested-loop join as the simple/general baseline,
- hash join as the primary equality-join implementation,
- index nested-loop join when the inner side has a useful index,
- merge join as a later algorithm.

The old source expressed these partly as implementation order. The final architecture treats them as algorithm availability/role rather than as a development-agent instruction.

Hash join architecture must support:

- explicit build and probe phases,
- query-lifetime memory ownership,
- SQL NULL semantics,
- composite keys,
- later spill behavior when memory budgeting requires it.

The complete in-memory/spilled hash table, LEFT join behavior, residual predicates, skew handling, and parallel build/probe contracts are migrated in Pass 13.

---

# 29. Aggregation and DISTINCT

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §39. Detailed aggregation is migrated in Pass 13.

Hash aggregation is the primary initial grouped-aggregation architecture.

The architecture later permits:

- sort-based aggregation,
- streaming aggregation when input ordering is sufficient,
- spill-capable aggregation under memory pressure.

Aggregate state representation, DISTINCT integration, NULL grouping semantics, spill/finalization behavior, and parallel combine are defined by the detailed execution contract.

---

# 30. Sorting and Top-N

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §40. Detailed sorting is migrated in Pass 13.

The baseline sort implementation is in-memory comparison sorting.

Once query-memory budgeting exists, the architecture requires a path to external merge sorting for inputs that exceed memory.

Top-N, normalized sort keys, external run representation, merge behavior, and exact SQL comparator semantics are defined by the detailed execution contract.

---

# 31. DML, DDL, VACUUM, and Result Interface

Physical DML/DDL execution, Halloween protection, target materialization, `RETURNING`, VACUUM execution, result delivery, and retry-safe output behavior are migrated in Pass 13.

---

# 32. Parallel Execution and Scheduling

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §42 and relevant shared-state direction from §43. Detailed parallel runtime architecture is migrated in Pass 13.

The execution architecture must be parallel-ready even though the initial executor MAY execute a query on one worker.

The design MUST NOT preclude later:

- parallel heap scans,
- parallel hash build/probe,
- parallel aggregation,
- parallel sorting,
- task scheduling.

A complex parallel scheduler is not required before single-worker execution is correct.

The detailed worker-pool, morsel, local/global state, dependency scheduling, fairness, and parallel operator protocols are defined by the later execution contract.

---

# Part VII — Cost-Based Optimization

# 33. Optimizer Architecture

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §33. The complete optimizer architecture is migrated in Pass 14 and Pass 15.

Physical planning is cost-based.

The optimizer compares valid physical alternatives using estimated resource costs rather than hard-coded assumptions such as “an index scan is always better than a sequential scan.”

The initial cost model considers at least:

- estimated cardinality,
- CPU work,
- sequential page reads,
- random page reads,
- memory consumption.

Exact calibration MAY evolve, but physical algorithm selection MUST remain cost-driven.

---

# 34. Statistics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §32. The complete statistics contract is migrated in Pass 14.

The optimizer maintains explicit table/column statistics.

The architecture requires statistics sufficient to represent at least:

- row count,
- distinct-count estimate,
- null fraction,
- minimum/maximum values,
- histograms or another distribution summary.

Cardinality estimation is a first-class subsystem rather than an implicit heuristic embedded in operator selection.

---

# 35. Cardinality Estimation

The detailed selectivity, predicate truth, histogram/MCV, distinct-count, grouping, and join-cardinality models are migrated in Pass 14.

---

# 36. Cost Model and Base Access Paths

The detailed cost units, calibration parameters, sequential/index access costing, sargability, composite bounds, and base-path enumeration rules are migrated in Pass 14.

---

# 37. Physical Properties and Join Enumeration

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §34. Detailed physical properties and join search are migrated in Pass 15.

For small join sets, the optimizer uses dynamic-programming enumeration of useful join alternatives.

For larger join graphs, planning time is bounded using heuristics or another controlled search strategy.

Join-order optimization is implemented as part of the database rather than outsourced.

The exact exhaustive threshold, bushy-plan representation, legality constraints, interesting-order retention, join-algorithm enumeration, and deterministic tie-breaking are defined by the detailed optimizer contract.

---

# 38. Memo/Search and Memory-Aware Optimization

Memo representation, dominance, physical-property enforcement, memory/spill costing, large-join heuristics, plan fingerprints, and search diagnostics are migrated in Pass 15.

---

# Part VIII — Cross-Cutting Requirements

# 39. Error and Corruption Model

> **Rewrite status:** Pass 1 migrates the architectural baseline from legacy §44. Later subsystem passes add subsystem-specific error/corruption contracts.

The system distinguishes at least:

- SQL/user errors,
- transaction conflicts,
- I/O errors,
- persistent corruption,
- internal invariant failures.

Persistent corruption and impossible storage states MUST NOT be silently accepted.

Errors that can arise from valid external input, I/O behavior, concurrent transactions, or corrupted persistent bytes require explicit runtime handling; debug assertions alone are insufficient for such cases.

Internal invariant failures SHOULD fail loudly in debug/test configurations.

---

# 40. Observability and EXPLAIN

> **Rewrite status:** Pass 1 migrates the baseline from legacy §§47–48. Detailed subsystem metrics and optimizer/executor diagnostics are migrated in later passes.

## 40.1 Internal metrics

The engine must expose internal counters sufficient to reason about correctness and performance.

The initial metric families include:

- logical page reads,
- physical page reads,
- page writes,
- buffer hits/misses,
- WAL bytes,
- WAL flushes,
- B+ tree splits,
- rows scanned,
- rows filtered,
- hash build/probe activity,
- optimizer-estimated rows,
- actual rows.

Exact metric ownership and naming MAY evolve by subsystem.

## 40.2 EXPLAIN

The SQL interface eventually exposes `EXPLAIN` information including:

- logical plan,
- selected physical plan,
- estimated cardinalities,
- estimated costs.

`EXPLAIN ANALYZE` additionally exposes actual execution information such as row counts and timing so estimates can be compared with observed behavior.

These facilities are architectural observability requirements, not merely debugging conveniences.

---

# 41. Verification Requirements

> **Rewrite status:** Pass 1 preserves the architecture-level requirements from legacy §45. Detailed test recipes remain preserved in the legacy source until a dedicated verification document is adopted.

Every major subsystem requires focused verification appropriate to its invariants.

The verification strategy includes:

- unit tests for local contracts and boundary conditions,
- deterministic property/randomized tests for stateful data structures and storage algorithms,
- crash tests around WAL/data-write boundaries,
- differential SQL tests against a reference database where the supported semantics overlap,
- concurrency stress tests for lock/latch ordering, structural modifications, buffer eviction, and transaction conflicts.

Property/randomized testing is particularly important for:

- B+ tree operations,
- page compaction,
- tuple serialization,
- MVCC visibility,
- recovery.

Recovery correctness MUST be tested with simulated failures rather than inferred solely from normal shutdown/reopen behavior.

Tests MUST be able to use intentionally small resource limits, including tiny buffer pools, when required to force eviction and boundary behavior.

Detailed per-subsystem test recipes are verification documentation rather than canonical architecture and are not duplicated here.

---

# 42. Performance Requirements

> **Rewrite status:** Pass 1 preserves the architecture-level requirements from legacy §46. Detailed benchmark procedures remain preserved in the legacy source until a dedicated verification document is adopted.

Performance claims require measurement.

The benchmark program must eventually cover at least:

- sequential scan throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- buffer-pool hit/miss behavior,
- concurrent transaction throughput.

Relevant measurements include:

```text
rows / second
queries / second
transactions / second
p50 latency
p95 latency
p99 latency
CPU time
page reads / writes
WAL bytes
cache hit rate
```

Both microbenchmarks and end-to-end benchmarks are required because they answer different performance questions.

Performance-sensitive architecture changes SHOULD be supported by benchmark evidence rather than a single noisy measurement or intuition alone.

---

# Appendix A. Persistent Format Registry

The exact persistent-format registry is populated as the storage, index, transaction-status, WAL, catalog, and related format chapters are migrated.

This appendix will index canonical definitions rather than duplicate them.

---

# Appendix B. Global Invariants

The following global invariants are migrated from the legacy architecture baseline. Later passes may add subsystem-specific invariants and cross-references, but MUST NOT weaken these requirements without an explicit architecture revision.

1. Persistent structures use logical persistent identifiers such as page IDs, never process memory pointers.
2. On-disk formats use explicit serialization.
3. WAL required to recover a persistent data-page modification becomes durable before that page modification becomes durable.
4. Buffer pins are eventually released according to the page-lifetime contract.
5. Transaction-level logical locks and in-memory latches are distinct mechanisms.
6. Logical plans do not encode a physical implementation choice.
7. The optimizer may choose among multiple physical algorithms for one logical operation.
8. Performance-critical execution paths avoid unnecessary per-cell or per-row object allocation.
9. Lower layers do not depend on SQL syntax objects.
10. Verification can force constrained resource configurations, including tiny buffer pools, to exercise eviction.
11. Recovery is verified with simulated crashes.
12. Performance-sensitive changes require measurement.

---

# Appendix C. Deferred Features and Future Experiments

This appendix records broad deferred scope introduced by the legacy architecture overview. Later subsystem passes consolidate the complete deferred-feature lists.

Initial deferred or non-v1 areas include:

- distributed consensus, replication, sharding, and multi-node execution,
- cloud-native storage,
- full SQL-standard/vendor compatibility,
- stored procedures and triggers,
- sophisticated authentication/authorization,
- primary columnar table storage,
- JIT compilation,
- GPU execution,
- overflow/TOAST-style tuple storage,
- advanced/alternative buffer replacement such as CLOCK-Pro, LRU-K, and 2Q,
- `io_uring`, direct I/O, asynchronous prefetch, and deliberate page-cache bypass,
- optimistic/lock-free B+ tree techniques,
- true serializable isolation and its candidate mechanisms,
- background vacuum before the manual/correct baseline,
- complex parallel scheduling before single-worker execution is correct,
- merge join until the earlier join implementations exist,
- external sorting until query-memory budgeting exists.

Items listed here are future possibilities or staged functionality, not requirements to implement immediately.

---

# Appendix D. Open Architecture Decisions

Pass 1 introduces no new open architectural decision.

Rewrite/implementation consistency issues discovered while reorganizing the legacy contract are tracked separately in `ARCHITECTURE_REWRITE_ISSUES.md` so they are not silently converted into architecture decisions.

---

## Rewrite progress

Architecture content migrated so far:

```text
Pass 0    inventory and target structure
Pass 1    legacy §§0–52
```

The existing `ARCHITECTURE.md` remains the active architecture authority until the full rewrite, reconciliation audit, and explicit cutover are complete.
