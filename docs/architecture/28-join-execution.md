# 28. Join Execution

## 28.1 Join algorithm roles

The physical join family is:

```text
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalHashJoin
PhysicalMergeJoin
```

Their roles are:

```text
NestedLoop:
    correctness/general baseline for small materialized inner input

IndexNestedLoop:
    small outer side + selective indexed inner lookup

HashJoin:
    main equality-join implementation

MergeJoin:
    capability-conditional ordered-input algorithm when predicates and physical properties permit
```

Physical planning selects one capability-valid join algorithm before execution.

No join implementation may reinterpret SQL join semantics merely because a different physical algorithm is selected.

## 28.2 Central query-local hash semantics

Execution uses a centralized type-aware 64-bit hash utility.

The fundamental invariant is:

```text
if the relevant SQL equality mode says A and B are equal
then Hash(A) == Hash(B)
```

Hash values are query/process-local execution values.

They are not persistent database format and need not remain stable across database versions.

The architecture does not use `std::hash` as a semantic contract.

### 28.2.1 Equality modes

Hashing/equality receives an explicit semantic mode.

Both modes use §17.10.3's closed non-NULL scalar equality and hash-normalization
contract; only their NULL treatment differs.

For ordinary equi-join equality:

```text
NULL in any key component -> non-matchable key
```

For GROUP BY / DISTINCT equality:

```text
NULL == NULL for grouping identity
```

The modes share the same concrete type semantics for non-NULL values.

In particular:

```text
FLOAT64 -0.0 and +0.0 hash equally
canonical-equivalent NaNs hash equally where the equality mode treats them equal
VARCHAR hashes exact binary bytes
composite keys mix component hashes and component/null markers
```

A strong 64-bit avalanche/mix function is used for composite keys, but the exact query-local mix algorithm is not a persistent compatibility promise.

## 28.3 Nested-loop join

`PhysicalNestedLoopJoin` is the correctness/small-input baseline.

For the baseline shape:

```text
materialize the right input
then probe it with left-side chunks
```

Supported logical join types are:

```text
INNER
LEFT
CROSS
```

The materialized right side uses query-owned row storage and deep-copies retained variable-length values.

A probe row may emit across multiple output chunks; the runtime keeps continuation state rather than requiring one full cross product to fit in memory.

Nested-loop join is a costed physical alternative, not a universal large equi-join fallback.

## 28.4 Index nested-loop join

For each outer chunk:

```text
1. vector-evaluate inner lookup key expressions
2. batch B+ point/range lookups where practical
3. fetch candidate inner heap versions
4. apply heap MVCC visibility
5. evaluate the residual join predicate
6. emit joined rows
```

The index never proves inner-row visibility by itself.

The architecture uses this path when:

```text
outer side is small
+
inner lookup is selective through an index
```

For a LEFT index nested-loop join, the logical left side is the preserved outer side and one NULL-extended row is produced when no inner candidate survives MVCC/residual evaluation.

## 28.5 Hash-join shape and preserved-side orientation

`PhysicalHashJoin` is the primary equality-join implementation.

Its inputs are:

```text
build input
probe input
equality-key expression pairs
optional residual predicate
join type
```

`PhysicalHashJoin` supports:

```text
INNER
LEFT
```

For INNER JOIN, the optimizer may select either logical input as build side when it preserves output-slot semantics.

For LEFT JOIN, the architecture fixes:

```text
logical right side -> build
logical left side  -> probe / preserved side
```

The optimizer MUST NOT arbitrarily swap that orientation merely to make the build smaller unless a separate semantic transformation proves an equivalent physical plan.

This keeps unmatched-row production local and unambiguous.

## 28.6 Hash-join build storage and directory

Build rows are retained in `RowCollection` using a build `RowLayout` containing at least:

```text
join key values required for full equality validation
payload columns required downstream
optional cached 64-bit hash
duplicate-chain metadata
```

Variable-length build values are deep-copied into build-owned storage.

The hash directory is power-of-two open addressing.

Each occupied directory entry contains conceptually:

```text
64-bit hash
head build-row handle for one equal-key duplicate chain
```

Insertion/probe behavior is:

```text
hash mismatch:
    continue probe sequence

hash match:
    compare full SQL key against the entry's representative/head row

full key equal:
    use/append that duplicate chain

same hash but different key:
    continue open-addressing probe sequence
```

Thus different SQL keys with the same 64-bit hash remain distinct entries, while many build rows with the same key share one compact duplicate chain.

The ordinary maximum directory load-factor tuning target is approximately:

```text
0.70
```

Resize before probe lengths become pathological.

Directory/row memory is charged to the join's memory reservation.

## 28.7 Hash-build pipeline and finalization

The build pipeline is:

```text
build source
    ↓
build filters/projections
    ↓
HashJoinBuild sink
```

For each batch the sink:

```text
1. vector-evaluates build keys
2. excludes rows with NULL in an ordinary equi-key component from the match directory
3. deep-copies required build rows
4. computes/stores hashes
5. creates duplicate chains/directory entries
```

For the supported INNER/LEFT hash-join shape, non-matchable NULL-key build rows can be discarded from the match structure because the build side is not preserved.

That pruning is permitted only when the selected hash-key/residual
decomposition is demand-safe under §§17.7.3 and 20.17. A logical ON predicate
whose source-ordered evaluation can raise an observable error before the
equijoin key is evaluated cannot be treated as harmless residual work for a
pair merely because the key contains NULL. In that case the physical plan must
preserve the required predicate demand and child order, for example by using a
conforming non-pruning algorithm or another exact implementation; it may not
discard the pair solely from the hash-key result. When the moved/decomposed
evaluation is proven total and error-free over the complete omitted pair
domain, NULL-key pruning remains valid and produces no match.

`Finalize` completes/resizes/compacts the directory and publishes immutable probe state.

The probe pipeline cannot begin until successful build Finalize.

After Finalize, ordinary probe workers do not mutate the hash directory.

## 28.8 Probe continuation and residual predicates

A probe chunk can produce more than one output chunk.

Local probe state stores enough continuation information for at least:

```text
current probe row
current build-row handle in duplicate chain
whether any candidate survived the residual predicate
LEFT JOIN matched state
```

After equi-key match, candidate joined rows are passed through the residual predicate.

Example:

```sql
A.id = B.id AND A.ts < B.ts
```

uses:

```text
hash/equality key:
    A.id = B.id

residual:
    A.ts < B.ts
```

Arbitrary non-equality predicates are never smuggled into hash-key equality.

## 28.9 LEFT hash join

For every logical-left/probe row:

```text
NULL in an equi-key component:
    emit one NULL-extended right/build row

otherwise probe duplicate chain:
    emit every candidate that survives residual evaluation

if no candidate survives:
    emit one NULL-extended right/build row
```

A hash collision or an equi-key match whose residual later fails does not count as a successful LEFT JOIN match.

The output right/build slots have the nullable metadata required by the logical LEFT JOIN contract.

## 28.10 Grace hash-join spill

When build state cannot stay within its memory reservation, the join switches to partitioned Grace-style execution.

The core protocol is:

```text
hash build input
    ↓
partition using high hash bits
    ↓
spill build partitions

partition probe input with the identical partition function
    ↓
for each partition pair:
    load/build one build partition
    probe corresponding probe partition
```

Partition count is a power of two chosen from measured/estimated pressure and available reservation.

Spill blocks use the query-temporary SpillManager contract from Chapter 24.

For LEFT JOIN, probe rows with non-matchable NULL keys may be emitted as
unmatched without entering a hash partition only when the same demand-safe
decomposition rule permits omitting their candidate-pair predicate work. If an
omitted pair could expose a required source-ordered error, the selected
implementation must preserve that demand before producing the unmatched row.
Every other probe row belongs to exactly one probe partition and can produce at
most one unmatched LEFT output after that partition is fully evaluated.

Hash join never promises output ordering, so partition processing order may change physical output order.

## 28.11 Recursive repartition and skew fallback

If a build partition is still too large:

```text
repartition with additional hash bits
```

up to a bounded recursion depth.

The runtime records skew/repartition statistics.

If a pathological partition still cannot fit, execution uses a controlled correctness fallback such as chunked nested processing for that partition rather than recursing indefinitely or exceeding the hard memory limit.

The fallback remains memory-accounted.

## 28.12 Merge join

`PhysicalMergeJoin` is a capability-conditional physical alternative when compatible input
ordering and admitted predicate semantics satisfy the physical-planning rules in Chapters
22, 37, and 38.

It is useful when compatible ordering already exists or when ordered output has downstream value.

The `PhysicalMergeJoin` contract admits equality merge joins.

Ordinary equality retains ordinary NULL semantics:

```text
NULL does not match NULL
```

Duplicate key groups are processed incrementally.

The operator MUST NOT materialize the complete cross product of two large duplicate groups merely to produce one output chunk.

For LEFT semantics, unmatched logical-left rows are emitted exactly once.

Range-style merge joins are outside the Chapter-28 execution contract; every admitted
merge-join form requires explicit predicate semantics rather than treating inequalities as
one interchangeable algorithm.

## 28.13 Join invariants

1. Index candidates always return through heap MVCC visibility.
2. Query-local hash equality and hash values obey `equal => same hash`; hash-key
   pruning never suppresses a demanded source-ordered predicate error.
3. Ordinary equi-join NULL keys never match NULL keys.
4. Hash collisions always perform full key equality validation.
5. Equal build keys use duplicate chains without losing duplicate multiplicity.
6. LEFT hash join preserves the logical left/probe side in the baseline implementation.
7. Residual predicates run after equi-key candidate formation and determine LEFT matchedness.
8. One probe row may emit across multiple output chunks without losing state.
9. Hash-build state is finalized/immutable before dependent probe pipelines execute.
10. Retained build VARCHAR values are owned by build storage.
11. Grace spilling uses identical build/probe partition semantics.
12. Hash spill/repartition may change output order because hash join advertises no order.
13. Repartition recursion is bounded and has a controlled skew fallback.
14. Merge join duplicate groups are streamed incrementally rather than materialized as one cross product.
