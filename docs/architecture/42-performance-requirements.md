# 42. Performance Requirements

Performance claims require measurement.

Performance evaluation MUST cover at least:

- sequential page-read throughput,
- sequential page-write throughput,
- buffer-pool resident-hit lookup,
- buffer-pool miss plus physical read,
- heap insertion throughput,
- heap sequential-scan throughput,
- tuple encode throughput,
- tuple decode throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
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

Storage benchmarks SHOULD distinguish cache/residency conditions and resource pressure when those distinctions affect the mechanism being measured, including small versus large BufferPool capacity and warm versus deliberately less-warm file-cache conditions where practical.

Performance-sensitive architecture changes SHOULD be supported by benchmark evidence rather than a single noisy measurement or intuition alone.


## 42.1 B+ tree performance measurements

B+ tree measurement should cover at least:

```text
random point lookups / second
sorted insertions / second
random insertions / second
exact deletions / second
short range-scan rows / second
long range-scan rows / second
duplicate-heavy equality lookup
split frequency
tree height
average leaf byte occupancy
average internal byte occupancy
BufferPool hit rate
page-latch wait time when available
```

Representative key shapes include:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

Measurements SHOULD distinguish:

- a mostly resident/hot tree, emphasizing CPU, comparison, latch, and cache behavior,
- a tree whose working set exceeds BufferPool capacity, emphasizing fanout, page access, replacement, and random-I/O sensitivity.

These are measurement dimensions, not requirements to prefer one optimization before evidence exists.

## 42.2 Transaction, durability, recovery, and vacuum measurements

Group-commit measurement SHOULD include a single-committer baseline and representative multi-committer levels spanning batching and contention. It records transactions/sec, commit latency percentiles, `fdatasync` calls/sec, commits per sync, and WAL bytes/sec.

Checkpoint/recovery measurement includes checkpoint duration, checkpoint WAL/FPI bytes, DPT size, retained WAL, analysis time, redo time, pages redone, and total recovery time under both mostly-clean and heavily-dirty buffer states.

Vacuum measurement includes dead-version scan rate, exact index-cleanup rate, heap bytes reclaimed, B+ structural side effects, RID grace delay, FSM improvement, freezing rate, and foreground latency impact across update-heavy, delete-heavy, aborted-transaction, duplicate-key, and long-running-snapshot workloads.

These are measurement dimensions; they do not mandate a premature optimization.

## 42.3 Catalog/front-end/logical-planning measurements

Measure enough to detect pathological architecture without optimizing the front end prematurely.

Useful dimensions include:

```text
lexer MB/s
parser statements/s
binder latency
catalog name/ID lookup latency
historical schema lookup latency
catalog cache hit rate
large SELECT-list binding
large VALUES binding
large expression-tree binding
multi-join binding
logical-plan construction time
rewrite-phase time
front-end allocation calls and bytes
optimizer planning-arena allocations and bytes
```

Representative parser/AST memory cases include a wide SELECT, large VALUES insert, deep Boolean expression, and multi-join query.

Front-end allocation should limit avoidable per-node general-purpose heap-allocation churn without prescribing the AST representation; benchmark evidence—not guesswork—drives further optimization.

## 42.4 Execution measurements and hot-path constraints

Execution microbenchmarks measure at least:

```text
scan rows/sec and bytes/sec
filter/projection arithmetic rows/sec
VARCHAR comparison rows/sec
hash build/probe/output rows/sec
aggregate input rows/sec and groups/sec
in-memory sort rows/sec
external sort MB/sec
allocator calls/chunk allocations/temporary bytes per query
```

Benchmarks include NULL-free and NULL-heavy data.

The vector-size study includes representative chunk sizes below, at, and above the default on representative workloads.

The architecture default remains 1024 until measurements justify changing it.

End-to-end workloads include point/index-range lookup, full scan, filter/project, small and large joins, group aggregation, ORDER BY, ORDER BY LIMIT, index-driven UPDATE, bulk INSERT, and concurrent read/write.

Track at least:

```text
latency
throughput
CPU
peak query memory
buffer hits/misses
WAL bytes
spill bytes
```

For a physical path that supports multi-worker execution, parallel scaling is measured against a single-worker baseline at representative supported multi-worker levels to identify scheduler, synchronization, cache, and memory-bandwidth limits.

Strong implementation constraints are:

1. steady-state hot execution MUST NOT inherently require one general-purpose heap allocation per processed row or cell,
2. when operation, type, or representation is resolved at batch/kernel selection, the hot row loop MUST NOT re-dispatch that same choice through a virtual call or type switch per row,
3. no generic `Value` construction per hot cell,
4. do not pin heap pages merely to retain VARCHAR execution references,
5. blocking operators own retained varlen data,
6. large operator memory is QueryMemoryManager-accounted,
7. spill I/O is large/sequential where practical,
8. DataChunks/vector buffers are reused,
9. scans decode only required columns,
10. hash/directory storage is compact/contiguous where practical,
11. correctness is preserved under tiny budgets/forced spill,
12. aggressive optimization remains measurable with applicable Chapter-40 instrumentation/profiling,
13. within the supported Architecture capability scope, profiles—not intuition alone—justify optional explicit SIMD, permitted radix strategies, or explicit prefetch complexity; deferred capabilities such as JIT remain outside the supported scope absent an explicit Architecture revision.

## 42.5 Statistics and optimizer calibration measurements

ANALYZE measurements include:

```text
rows/second
columns analyzed
peak maintenance memory
HLL/MCV/sample memory
statistics payload bytes
full-scan I/O
```

Cost calibration measures the primitive dimensions in §36.4 on the target deployment and records the resulting relative weight configuration.

Base-access calibration workloads vary:

```text
selectivity
heap pages / rows per page
index/heap correlation
cache budget
required output width
VARCHAR width
dead-version fraction
```

The required demonstration is that the same query shape may select SeqScan or IndexScan as these measured conditions change, without an explicit hard-coded selectivity cutoff.

Cardinality-estimation benchmark suites report error distributions, not only average error, and include skew/MCV/NULL/correlation-limitation workloads.

## 42.6 Optimizer search and cost-model measurements

Operator cost calibration compares predicted **relative ranking** against measured runtime/resource ranking for representative:

```text
SeqScan
IndexScan
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
HashAggregate
Sort
TopN
spill paths
```

The architecture does not require predicted abstract cost to equal milliseconds.

Join-planning benchmarks use a configured `exhaustive_join_limit` that permits both search regimes, with relation counts near either side of that boundary and larger cases that expose bounded-heuristic scaling. They record:

```text
planning wall time
subsets explored
partitions considered
physical alternatives costed
memo entries retained/pruned
peak planning memory
heuristic/local-improvement work
```

They must demonstrate the transition from exhaustive bushy search to bounded heuristic behavior.

Interesting-order tests demonstrate cases where a locally more expensive ordered plan wins globally by avoiding Sort or enabling another capability-enabled ordered operator.

Memory-aware tests hold logical statistics constant while changing the query planning/execution memory budget and verify that estimated spill and plan choice can change where appropriate.

A synthetic star-schema workload is a recommended stress case for many joins, selective dimensions, a large fact relation, and aggregation.

Benchmark-specific query text/table names MUST NOT be hard-coded into optimizer decisions.

Optimizer improvements arise from general:

```text
statistics
semantic rewrites
properties
costing
search
```

rather than benchmark fingerprints.
