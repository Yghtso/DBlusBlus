# 32. Parallel Execution and Scheduling

## 32.1 Worker model

Execution uses a fixed worker pool. The database does not create an arbitrary OS thread per query or per pipeline. Worker count is configuration. The worker/task model supports one-worker and multi-worker execution without changing its ownership or completion semantics.

## 32.2 Morsels

Parallel-ready sources divide work into independent units called morsels.

Examples are:

```text
heap scan:
    contiguous page ranges

Values:
    row ranges

spill work:
    partition/run ranges
```

For each parallel-ready source/operator instance, execution attempt, and
required algorithmic pass, the owning source defines the source domain that
the current plan must consume. When that domain requires complete
consumption, its morsels form a complete, pairwise disjoint partition of the
domain: their union is the required work, no required unit belongs to two
morsels, and no required unit is left unassigned. The domain is the source's
established demanded work after the Chapter-20 demand and any canonical
early-stop/pruning rule; this contract does not force work that the plan is
legally allowed to omit.

Morsel ownership is about required source work, not output cardinality or SQL
row order. One physical source unit may contain multiple logical occurrences,
and the owning source accounts for each required occurrence under
§26.4.2. A filter may therefore produce no output, a join may produce zero or
many output occurrences, and an aggregate may combine many inputs without
weakening the source-ownership rule. Within the defined source instance,
attempt, and pass, each required source occurrence is accepted exactly once;
a separate valid algorithmic pass has its own source domain and ownership.

A morsel claim is a linearized ownership transition. Conceptually, a morsel
is one of:

```text
available and unclaimed
exclusively claimed by one current execution owner
successfully completed by that owner
canceled or failed under the owning execution protocol
```

Concurrent claim attempts cannot both succeed for one available morsel. A
successful claim cannot disappear without the owner's completion, cancellation,
or failure outcome, and one morsel cannot be reported successfully completed
twice. Splitting preserves the parent's complete domain as disjoint child
domains; coalescing covers every child domain exactly once. A claimed or
accepted domain is not silently reassigned in a way that duplicates work.
These are ownership semantics, not requirements for a particular queue,
counter, lock, compare-exchange, or work-stealing structure.

An empty source domain has an empty morsel partition and may complete
successfully with no output. A zero-length morsel may be represented and
claimed, but owns no source occurrence and cannot create a duplicate or hide
another required unit. A nonempty claimed morsel may legitimately produce no
output after visibility or filtering. Coverage is established from the
assigned source domains and their claim/completion state, not from output
chunk cardinality alone.

Cancellation or failure of a claimed morsel does not count as successful
completion. It follows §§26.7 and 39.3: required missing work cannot yield a
successful complete result, other workers quiesce under the existing runtime
owner, and cleanup releases query-owned state without inventing a general
mid-attempt replay protocol. An implementation may reassign failed work only
where an existing canonical owner provides an attempt-isolated, exact retry
protocol; no generic automatic requeue is implied here.

A heap-scan morsel target may be configured around: `64–256 heap pages per morsel` and is tunable performance guidance, not a semantic partition size.

B+ ordered range scans are not freely partitioned in the baseline and may remain a single source until explicit range partitioning exists. Morsel boundaries are also natural cancellation/fairness points.

## 32.3 Worker-local state

Each worker/task owns hot mutable state such as:

```text
expression scratch
reusable output chunks
source cursor/morsel position
local aggregate table
local sort run
join probe continuation
local profiling counters
```

Workers do not contend on one shared mutable state object for every chunk when the operator can instead Combine/Finalize local state at a boundary. Immutable plan state remains shared. Global execution state contains only the coordination/shared structures that genuinely require it.

## 32.4 Parallel sequential scan

Workers claim heap page-range morsels under §32.2's complete, disjoint, and
exclusive-claim contract. For a scan pass requiring complete consumption, the
assigned page ranges cover the required page domain without gaps or overlap,
and a range cannot be counted complete until its required source occurrences
have been accepted once under §26.4.2. A missing or failed required range
therefore cannot produce successful scan completion. The same physical page
may be visited by a different statement or a separate valid algorithmic pass;
the ownership rule applies to the current scan instance and pass.

Every worker:

```text
uses the same effective query transaction/snapshot semantics
uses the same query-lifetime read-epoch protection
fetches through the shared BufferPool
produces independent chunks
```

The transaction/snapshot/CommandId view consumed by parallel read tasks is immutable for the lifetime of those tasks. Transaction terminal-state transitions and DML mutation are not performed concurrently by arbitrary read workers.

Without a required ordering property, worker output may interleave arbitrarily. If ordered output is required, the selected physical plan must include/order-preserve through an operator capable of that property. Parallel SeqScan itself still advertises no SQL ordering.

## 32.5 Parallel hash join

### Build

A parallel hash-join build may avoid one contended resizable global table:

```text
workers append to local RowCollections
compute hashes
optionally partition by hash
    ↓
build input barrier
    ↓
construct/finalize directory partitions in parallel
```

Local build storage is combined/referenced through query-local handles without invalidating retained varlen ownership. The dependent probe phase starts only after every build partition it needs is finalized.

### Probe

After finalization, the hash directory/partitions are immutable. Probe workers execute concurrently without hash-directory mutation latches.

Each owns:

```text
probe continuation state
output chunk
local counters
```

LEFT JOIN preserved-side semantics remain unchanged by parallelism.

## 32.6 Parallel hash aggregate

A parallel hash aggregate may use:

```text
worker-local group hash tables
    ↓
partitioned/combinable aggregate states
    ↓
Combine / Finalize
    ↓
result groups
```

Do not serialize every aggregate update through one global lock. For global aggregation, each worker may maintain one local aggregate state block and combine those states at finalize. Aggregate descriptors' `Combine` contract is therefore mandatory for aggregates that participate in parallel execution.

Every local state and Combine implements §29.3's applicable semantic state.
COUNT, integer SUM/AVG, and MIN/MAX remain invariant to worker count, partition
ownership, final worker order, and Merge-tree shape. FLOAT64 SUM/AVG parallel
reduction instead corresponds to an admitted §29.3.4 binary64 tree: those
physical choices may change the tree-selected binary64 root but cannot change occurrence
membership, exact AVG count, explicit-input special classification, result
type/nullability, or canonical zero/NaN representation. Result-width integer
subtotals remain forbidden.

## 32.7 Parallel sort

Workers generate sorted local runs independently.

Finalize is:

```text
parallel run generation
    ↓
merge tree / k-way merge
    ↓
ordered output source
```

There is no shared comparison-sort critical section. In-memory and spilled local runs enter the same merge architecture. When an ordering property is required, final emission occurs through the merge/order-preserving source rather than arbitrary worker interleaving.

## 32.8 Task scheduler and dependencies

A conforming parallel scheduler may use:

```text
fixed worker pool
global concurrent ready-task queue
pipeline dependency counters
morsel tasks
```

A task becomes ready only after its dependency counter reaches zero through successful predecessor Finalize transitions.

For a source or blocking stage whose current demand requires complete
consumption, successful completion also requires that every required morsel be
claimed and resolved successfully; an unclaimed, active, failed, or canceled
required morsel cannot be hidden by partial completion. A canonical early stop
or pruning rule may make the remaining domain unnecessary under §26.8 and
Chapter 20. This is a morsel-coverage precondition, not a second Finalize
authority: Chapter 26 continues to own dependency transitions, readiness, and
quiescence.

Query cancellation prevents new unnecessary tasks from being scheduled and causes running tasks to stop at normal cancellation points. The baseline intentionally does not require lock-free work stealing.

Per-worker work-stealing deques are a compatible optional optimization when
ready-queue contention warrants them.

## 32.9 Fairness

A long query should not monopolize a worker indefinitely without yield/cancellation boundaries. Morsel/task granularity provides those boundaries.

A multi-query scheduler may cap:

```text
active tasks per query
memory per query
worker share
```

The baseline scheduler can remain simple as long as its task model preserves these control points.

## 32.10 NUMA policy

NUMA-specific buffer pools, worker affinity, hash placement, and memory
placement are optional policies for multi-core execution.

Large allocation interfaces remain centralized so a NUMA policy can be
inserted without changing operator semantics.

## 32.11 SIMD and hot-loop policy

Vector kernels are written to enable compiler auto-vectorization:

```text
contiguous typed buffers
simple counted loops
all-valid fast paths
selection vectors
batch-level type/representation dispatch
```

Portable C++ is the baseline.

Explicit AVX2/AVX-512/NEON kernels are optional architecture-specific
optimizations for operations such as comparison, arithmetic, validity, and
hashing.

Hot loops prefer:

```text
specialized no-NULL/nullable paths
selection vectors
compact state machines
```

over:

```text
per-row type switches
per-row virtual calls
per-cell variant visitation
```

## 32.12 Prefetch

The architecture leaves explicit optimization points for:

```text
next sequential heap pages
next B+ leaf
hash-table probe buckets
spill-merge blocks
```

BufferPool/OS/cache behavior is a valid default for these access patterns.

Explicit prefetch is optional and may be used when profiles show benefit,
provided it does not extend unsafe page/data lifetimes.

## 32.13 Parallel-runtime invariants

1. The worker pool is fixed/configured rather than unbounded thread creation.
2. For each source instance, execution attempt, and required algorithmic pass,
   morsel domains completely and disjointly cover the required source work;
   each morsel has one exclusive claim owner, each required source occurrence
   is accepted exactly once within that scope, and only complete successful
   coverage or a canonical early stop may establish successful completion.
   Cancellation or failure never counts as successful coverage.
3. Worker-local hot state avoids per-chunk global synchronization when possible.
4. Parallel workers use the same transaction/snapshot/read-epoch semantics as single-worker execution.
5. Parallel SeqScan advertises no ordering.
6. Hash build is fully finalized before probe workers consume it.
7. Finalized hash probe state is immutable/read-only.
8. Aggregate parallelism uses §29.3's applicable local states plus Combine rather than one lock per row. Exact aggregate results remain execution-shape invariant; FLOAT64 SUM/AVG retain §29.3.7's admitted reduction-tree variation.
9. Parallel sort produces required order through merge/finalized ordered output.
10. Dependency counters prevent consumers from observing unfinalized breaker state.
11. DML mutation, DDL, and VACUUM remain single-coordinator/single-write-phase in the baseline unless separately specified.
12. Auto-SIMD/prefetch optimizations do not change SQL or memory-lifetime semantics.
