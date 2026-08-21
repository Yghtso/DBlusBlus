# DBlusBlus Development Guide

## Purpose and authority

This document defines implementation sequencing, milestone targets, and recommended module-layout guidance that is useful for engineering work but is not part of the technical architecture contract.

`ARCHITECTURE.md` defines intended system behavior and persistent/concurrency contracts. `PROJECT_STATE.md` records what is currently implemented and which phase is active. This guide does **not** authorize crossing a project phase gate; current project status and explicit project decisions remain authoritative for sequencing.

Exact filenames and directory organization may evolve. The subsystem responsibility boundaries defined by `ARCHITECTURE.md` do not.

The sections below organize development guidance by dependency and subsystem. When this guide and `ARCHITECTURE.md` differ on intended behavior, `ARCHITECTURE.md` is authoritative.

---

## Suggested Implementation Order

This order intentionally follows dependency direction and maximizes learning.

## Phase 1 — Raw storage

1. byte serialization utilities,
2. DiskManager and PageFile primitives,
3. page abstraction,
4. append-first page allocation primitives,
5. slotted heap page,
6. tuple encoding,
7. persisted FSM page and in-memory candidate-index foundations.

## Phase 2 — Buffer management

8. buffer frames,
9. page guards,
10. CLOCK replacement,
11. dirty-page flushing,
12. HeapFile and relation-wide FreeSpaceMap integration through BufferPool-managed page lifetimes,
13. buffer-pool and buffered-storage benchmarks.

## Phase 3 — Indexes

14. B+ tree page formats,
15. lookup,
16. insert/split,
17. range scans,
18. delete/rebalance,
19. latch coupling,
20. randomized B+ tree tests.

## Phase 4 — Transactions and durability

21. transaction manager,
22. selected MVCC representation,
23. visibility rules,
24. write conflicts,
25. WAL format,
26. group commit,
27. STEAL/NO-FORCE integration,
28. crash recovery,
29. vacuum/GC.

## Phase 5 — Catalog and SQL

30. bootstrap catalog,
31. system tables,
32. lexer,
33. parser,
34. AST,
35. binder,
36. type system.

## Phase 6 — Query execution

37. logical plans,
38. physical plans,
39. vector/chunk representation,
40. sequential scan,
41. filter,
42. projection,
43. nested-loop join,
44. hash join,
45. aggregation,
46. sort,
47. index scan.

## Phase 7 — Optimization

48. statistics,
49. rule rewrites,
50. selectivity estimation,
51. cost model,
52. access-path selection,
53. join-order dynamic programming,
54. `EXPLAIN`,
55. `EXPLAIN ANALYZE`.

## Phase 8 — Performance work

56. profiling,
57. allocation reduction,
58. cache/layout improvements,
59. contention reduction,
60. prefetch/asynchronous I/O experiments,
61. parallel execution.

---

## Module Layout Guidance

### Current implemented layout

The implemented Phase 1 source tree is organized by current subsystem ownership:

```text
src/
  common/
    crc32c.*
    encoding.h
    types.h

  storage/
    disk/
      disk_manager.*

    file/
      file_superblock.*
      page_file.*

    page/
      page.*
      page_header.h

    heap/
      fsm_candidate_index.*
      fsm_page.*
      heap_page.*
      heap_page_format.*

    tuple/
      tuple_codec.*
      tuple_header.*
      tuple_layout.*
```

Tests mirror these implemented ownership groups under `tests/common/` and `tests/storage/`, while
cross-project smoke coverage remains at the test root.

### Future expansion guidance

As authorized milestones begin, new implemented subsystems should gain ownership-oriented modules
such as:

```text
storage/buffer/
storage/index/
txn/
wal/
catalog/
execution/
optimizer/
```

These are future organization guidance, not present source directories or authorization to create
placeholder classes. A directory or abstraction is added only when its dependency prerequisites
are met and its implementation milestone has been explicitly authorized. Exact filenames may
evolve; the subsystem responsibility boundaries defined by `ARCHITECTURE.md` remain authoritative.

---

## Storage Milestone 1

This end-to-end buffered-storage milestone follows implementation of BufferPool, page guards, and HeapFile. It is not part of the pre-BufferPool Phase 1 foundation and does not itself authorize crossing the current phase gate.

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

## Next Development Stage — B+ Tree

Before implementing the B+ tree, read and follow the canonical B+ tree contract in `ARCHITECTURE.md` Chapter 8.

Chapter 8 has already settled the persisted layouts, page formats, physical-key and RID identity, FLOAT64 key encoding, separator and duplicate ordering, free-page format, root publication protocol, cursor lifetime, and latch/publication semantics. Implementation must conform to those contracts rather than selecting alternatives.

Implement in dependency order:

1. exact BTREE superblock, node-page, slot, and free-page codecs and validators,
2. the canonical `IndexKeyCodec`, including RID suffixes and FLOAT64 canonicalization/ordering,
3. page-local search and mutation over BufferPool-managed pages,
4. lookup, insertion, split propagation, exact physical deletion, redistribution, merge, and free-page reuse,
5. optimistic root publication and generation validation,
6. forward range cursors with the specified guarded page lifetime,
7. the canonical latch-crabbing and structural-publication protocols,
8. MVCC, uniqueness, WAL, and recovery integration only at their later dependency milestones.

This checklist controls implementation order only. Any proposed change to a settled Chapter 8 contract requires an explicit architecture revision before implementation.

After the B+ tree implementation reaches its required milestone, the next major development dependency is:

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

## B+ Tree Milestone 1

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

## B+ Tree Milestone 2

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

## B+ Tree Milestone 3

Integrate:

```text
MVCC heap visibility
transactional unique-key enforcement
vacuum index cleanup
WAL
crash recovery
```

Only after this development stage does the B+ tree implementation satisfy the transaction/durability integration expected by the architecture.

---

## Next Development Stage — Transactions and Durability

The transaction and durability core should be implemented as one coordinated dependency group, following `ARCHITECTURE.md` Chapters 9–15:

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

Before implementation, verify the relevant canonical contracts for:

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

## Recommended Transaction/Durability Module Layout

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

## Implementation Order for the Transaction/Durability Core

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

## Transaction/Durability Milestone 1

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

## Transaction/Durability Milestone 2

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

## Transaction/Durability Milestone 3

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

## Recommended Module Layout for the Upper Layer

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

## Implementation Order for Catalog + SQL Front End

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

## Upper-Layer Milestone 1

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

## Upper-Layer Milestone 2

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

## Upper-Layer Milestone 3

Connect logical plans to the vectorized executor defined by `ARCHITECTURE.md` Chapters 22–32.

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

## Recommended Execution Module Layout

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

## Execution Implementation Order

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

## Execution Milestone 1

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

## Execution Milestone 2

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

## Execution Milestone 3

Run with intentionally tiny query memory limits and correctly spill:

```text
HashJoin
HashAggregate
Sort
DML target spool
```

Results must match unlimited-memory execution.

---

## Execution Milestone 4

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

## Execution Milestone 5

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

## Recommended Optimizer Module Layout

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

## Optimizer Implementation Order

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

## Optimizer Milestone 1

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

## Optimizer Milestone 2

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

## Optimizer Milestone 3

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

## Optimizer Milestone 4

Cost memory pressure and spill:

```text
HashJoin
HashAggregate
Sort
```

Plan decisions should respond sensibly to query-memory budget changes.

`EXPLAIN ANALYZE` must compare estimate vs actual.

---

## Optimizer Milestone 5

Handle large join graphs with bounded planning time:

```text
exhaustive DP below threshold
heuristic/beam/local improvement above threshold
```

Planning time and plan quality must be benchmarked.
