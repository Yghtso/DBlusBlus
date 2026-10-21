# 30. Sorting and Top-N

## 30.1 PhysicalSort

`PhysicalSort` is a blocking operator.

It:

```text
1. evaluates resolved sort-key expressions
2. materializes required output payload
3. forms compact sortable records
4. sorts in memory while within its memory reservation
5. spills sorted runs when required
6. performs one- or multi-pass merge
7. emits sorted DataChunks
```

SQL sort is not required to be stable for rows equal on every ORDER BY key.

The operator's produced ordering property is exactly the resolved logical ordering:

```text
per-key ASC/DESC
NULLS FIRST/LAST
collation/type order
```

## 30.2 Sort storage and records

Sort-owned input rows live in query-temporary row/run storage and deep-copy retained VARCHAR bytes.

A compact sort record contains conceptually:

```text
normalized key prefix
row/payload handle
```

Full key values remain reachable for tie-breaking.

Normalized-prefix width is an implementation/configuration tuning choice and
may vary by key shape or implementation. A zero-length or shorter prefix is
valid when a sound normalized prefix is not useful or available.

The normalized prefix is an **order-preserving prefix of the resolved SQL comparator**, not merely a hash.

It incorporates leading-key semantics only when those semantics can be encoded safely, including:

```text
ASC/DESC transformation
NULL position
binary VARCHAR/type ordering
```

If a key cannot contribute a sound prefix, the prefix is shortened rather than guessing an order.

Prefix comparison may decide order when prefixes differ.

On prefix equality, execution performs the complete SQL comparator over the full sort keys.

## 30.3 Sort comparator

The complete comparator obeys exactly:

```text
key order
ASC / DESC
NULLS FIRST / NULLS LAST
SQL type ordering
binary VARCHAR collation
FLOAT64 total-order contract
```

The same semantic ordering is used for:

```text
in-memory sort
external-run generation
merge
Top-N
physical ordering-property matching
```

No raw-byte comparator substitutes for semantic comparison unless the type's normalized representation is proven equivalent.

## 30.4 In-memory sort

In-memory sorting MAY use a high-quality comparison sort over the compact sort
records/handles, or another sorting algorithm proven equivalent to the complete
semantic comparator over its supported key domain.

The expensive payload is not repeatedly moved during comparison sorting.

Radix, normalized-key, or hybrid sorting strategies MAY be used only where
their ordering is proven equivalent to the complete semantic comparator;
otherwise execution uses the complete comparator/fallback path.

## 30.5 External merge sort

When the current in-memory run reaches its reservation:

```text
sort run
    ↓
write sequential spill run
    ↓
release/reset run memory
```

After input finalization:

```text
k-way merge sorted runs
```

Readers/writers are buffered.

Merge fan-in is chosen from available query memory and run count.

If all runs cannot participate in one merge, execution performs multiple merge passes.

Every merge comparison uses the same comparator as in-memory sorting.

## 30.6 Sort-run temporary format

A sort run is a query-temporary SpillManager object containing enough information to validate the run, including conceptually:

```text
run identity / temporary format version
RowLayout or schema fingerprint
sort-key descriptor fingerprint
record count
checksummed sequential blocks
sorted records/payload
```

The fingerprint detects accidental mismatch inside one query/runtime execution; it is not a long-lived database format registry.

Runs are never WAL logged or crash recovered.

A malformed/checksum-failing run raises `SpillIOError` and aborts the query.

## 30.7 PhysicalTopN

For:

```sql
ORDER BY ... LIMIT N [OFFSET M]
```

a physical planner may choose `PhysicalTopN`.

For validated nonnegative INT64 `N` and `OFFSET`, define the physical
algorithm's required prefix size mathematically:

```text
K = N + OFFSET
```

`K` is an exact physical-algorithm requirement, not a SQL INT64 value or a
user-visible arithmetic operation. `PhysicalTopN` is eligible only when the
selected implementation can represent and honor exact K. If it cannot,
`PhysicalTopN` is ineligible and physical search uses another conforming
realization under §§30.8 and 38.15; the LIMIT/OFFSET statement remains valid
and no public count-overflow error is raised.

An implementation MAY use a wider or otherwise domain-specific exact internal
representation to retain Top-N eligibility. No concrete numeric representation
is required. A saturated or clamped retained-K bound MUST NOT be used unless an
independent exact architecture proof establishes that the reduced bound cannot
under-read in the applicable domain. The architecture defines no global relation-cardinality
bound that supplies such a proof.

If `K = 0`, the result is empty without consuming unnecessary upstream work when pipeline dependencies permit.

Otherwise Top-N maintains at most `K` best records in a bounded heap using the exact sort comparator.

The heap's root represents the worst retained candidate so a better incoming row can replace it efficiently.

After input completion:

```text
sort the retained K records by the exact comparator
skip OFFSET
emit up to N rows
```

Heap/internal container order is never exposed as SQL sort order.

For very large K, a full/external sort may be the better physical plan.

Exact-K representability is resolved by physical eligibility; genuine
execution failures such as `OutOfMemory`, `SpillIOError`, or cancellation
remain governed by their existing resource and runtime owners.

## 30.8 Sorting invariants

1. Sort is a pipeline breaker and does not emit final rows before input/run finalization.
2. Retained sort keys/payload own their varlen bytes.
3. Normalized prefixes are order-preserving aids, not hashes or substitute semantics.
4. Prefix ties always fall back to the full comparator.
5. In-memory, spill-run, merge, and Top-N comparison semantics are identical.
6. External sort may use multiple merge passes under bounded memory.
7. Sort runs are temporary and never WAL logged/crash recovered.
8. An eligible Top-N uses the exact mathematical K from §30.7 as its retention
   bound and retains at most K records; an unproved saturated/clamped bound is
   forbidden.
9. Top-N sorts retained output before emission.
10. Equal-key SQL sort need not be stable unless another explicit semantic requirement says otherwise.
11. An eligible Top-N belongs to the same Chapter-20 permitted ordered-result
    family as any exact ordering provider followed by `PhysicalLimit`, including
    bag cardinality, semantic order, comparator, demanded-error, schema/
    `LogicalSlotId`, and transaction/result behavior. When LIMIT/OFFSET cuts
    through a class equal on every resolved ORDER BY key, valid providers or
    executions MAY choose and order different actual child occurrences within
    that class; each result still preserves every strict comparator relation,
    returns only actual child occurrences, and neither deduplicates nor
    fabricates occurrences. No hidden RID, payload, or stable-input tiebreaker
    becomes SQL semantics.
12. When Top-N is ineligible, search retains another legal exact ordering
    provider followed by `PhysicalLimit`; this includes `PhysicalSort` followed
    by `PhysicalLimit` when no existing provider satisfies the required order.
