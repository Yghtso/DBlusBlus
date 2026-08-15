# DBlusBlus Verification and Benchmark Guide

## Purpose and authority

This document preserves detailed testing, crash-injection, fuzzing, regression, and benchmark procedures.

`ARCHITECTURE.md` defines the correctness/performance obligations. This guide describes practical ways to verify those obligations. A test recipe does not weaken or replace an architectural invariant, and benchmark numbers are measurements rather than persistent architecture constants.

Historical milestone labels in the source material are preserved only as useful grouping/context; current implementation status belongs in `PROJECT_STATE.md`.

---

## Testing Philosophy

_Source: archived architecture §45._

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

## Benchmarking Philosophy

_Source: archived architecture §46._

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

## Storage Milestone 1 Required Tests

_Source: archived architecture §104._

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

## Storage Milestone 1 Benchmarks

_Source: archived architecture §105._

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

## Required Deterministic Tests

_Source: archived architecture §170._

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

## Duplicate Stress Test

_Source: archived architecture §171._

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

## Randomized Tests

_Source: archived architecture §172._

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

## Concurrent Tests

_Source: archived architecture §173._

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

## B+ Tree Benchmarks

_Source: archived architecture §174._

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

## Crash Injection Framework

_Source: archived architecture §284._

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

## Recovery Property Tests

_Source: archived architecture §285._

For random transaction workloads:

1. execute operations,
2. crash at random instrumented points,
3. reopen/recover,
4. compare logical committed contents against a model that includes only transactions whose commit records became durable.

Repeat across many seeds.

Physical garbage is allowed.

Logical committed results must match.

---

## MVCC Visibility Tests

_Source: archived architecture §286._

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

## Isolation Tests

_Source: archived architecture §287._

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

## Locking Tests

_Source: archived architecture §288._

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

## Group Commit Benchmarks

_Source: archived architecture §289._

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

## Checkpoint/Recovery Benchmarks

_Source: archived architecture §290._

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

## Vacuum Benchmarks

_Source: archived architecture §291._

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

## SQL Grammar Testing

_Source: archived architecture §415._

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

## Binder Tests

_Source: archived architecture §416._

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

## Type-System Property Tests

_Source: archived architecture §417._

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

## Catalog Tests

_Source: archived architecture §418._

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

## Logical Planner Tests

_Source: archived architecture §419._

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

## Logical Rewrite Tests

_Source: archived architecture §420._

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

## Catalog / Front-End Benchmarks

_Source: archived architecture §421._

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

## Parser/AST Memory Benchmark

_Source: archived architecture §422._

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

## Front-End Fuzzing

_Source: archived architecture §423._

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

## Execution Testing Strategy

_Source: archived architecture §546._

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

## Vector Correctness Tests

_Source: archived architecture §547._

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

## String Lifetime Tests

_Source: archived architecture §548._

Create tests that deliberately:

1. scan VARCHAR data,
2. release/unpin source pages,
3. recycle source chunks,
4. continue downstream blocking processing,
5. verify strings remain valid.

Use allocator poisoning/debug memory where practical to catch accidental borrowed-pointer retention.

---

## Hash Join Tests

_Source: archived architecture §549._

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

## Aggregate Tests

_Source: archived architecture §550._

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

## Sort Tests

_Source: archived architecture §551._

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

## DML Execution Tests

_Source: archived architecture §552._

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

## Execution Microbenchmarks

_Source: archived architecture §553._

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

## Vector Size Benchmark

_Source: archived architecture §554._

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

## End-to-End Execution Benchmarks

_Source: archived architecture §555._

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

## Statistics Tests

_Source: archived architecture §701._

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

## Selectivity Estimation Tests

_Source: archived architecture §702._

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

## Join Estimation Tests

_Source: archived architecture §703._

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

## Access Path Tests

_Source: archived architecture §704._

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

## Join-Order Tests

_Source: archived architecture §705._

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

## Interesting-Order Tests

_Source: archived architecture §706._

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

## Memory/Spill Plan Tests

_Source: archived architecture §707._

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

## Optimizer Differential Correctness Tests

_Source: archived architecture §708._

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

## Optimizer Fuzzing

_Source: archived architecture §709._

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

## Cost Model Benchmarks

_Source: archived architecture §710._

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

## Plan Regression Suite

_Source: archived architecture §711._

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

## Optimizer Performance Benchmarks

_Source: archived architecture §712._

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

## Star Schema Benchmark

_Source: archived architecture §713._

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

## No Benchmark Gaming

_Source: archived architecture §714._

Do not hardcode:

```text
query text fingerprints
known benchmark table names
special-case TPC query shapes
```

Optimizer improvements must arise from general statistics/rules/costing.
