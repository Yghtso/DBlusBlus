# 29. Aggregation and DISTINCT

## 29.1 Hash aggregation role

`PhysicalHashAggregate` is the main grouped-aggregation implementation.

It consumes:

```text
group-key expressions
aggregate descriptors
aggregate argument expressions
```

No grouping keys means global aggregation and uses the specialized single-state path in §29.5. Hash aggregation is a pipeline breaker until its group state is finalized.

## 29.2 Aggregate state API

Every aggregate implementation exposes conceptually:

```text
StateSize
StateAlignment
Initialize(state)
Update(state, vector inputs, selection)
Combine(target, source)
Finalize(state, output vector)
Destroy(state) if required
```

This contract supports:

```text
vectorized batch update
worker-local partial state
parallel combine
spill/repartition processing
```

`Update`, `Combine`, and `Finalize` must implement the semantic states and
reduction laws in §29.3. They are not permission to choose host-width integer
accumulators or an unbounded, unspecified FLOAT64 variation. A descriptor unable
to preserve the applicable exact-state or admitted binary64-tree contract cannot
be capability-enabled for serial, parallel, or spill execution.

The executor does not invoke one virtual aggregate callback per input row. Function/operator dispatch is resolved per aggregate/vector batch.

## 29.3 Aggregate semantics registry

This section is the canonical closed aggregate-value contract. It fixes
state, reduction, finalization, and numerical errors independently of physical
container layout. COUNT, integer SUM/AVG, and MIN/MAX produce the same value or
aggregate numerical error for the same logical group under every legal
execution shape. FLOAT64 SUM/AVG instead admit only the precisely bounded
binary64 reduction-tree result variability in §29.3.4; “usual floating-point
variation” without such a tree is not a conforming result.

Aggregate argument expressions are evaluated under the closed Chapter-17
scalar registry. An argument expression error occurs before that row supplies
an aggregate value and retains the behavior defined by Chapter 17 and §39.1.
This section begins only after an argument value has been produced successfully; it does not alter
scalar expression trees or their rounding points.

Aggregate syntax is exactly `COUNT(*)`, `COUNT(expr)`, `SUM(expr)`,
`AVG(expr)`, `MIN(expr)`, and `MAX(expr)` with the overloads below. Aggregate
`DISTINCT` forms and aggregate `FILTER` clauses are unsupported and are
rejected during parsing/binding; this registry does not add them.

### 29.3.1 Semantic state domains

The following are conceptual query-local semantic domains, not TypeIds or
persisted database formats:

```text
ExactSignedInteger:
    an exact mathematical signed integer with enough logical range for every
    subtotal admitted by this query

ExactNonnegativeCount:
    an exact mathematical integer >= 0

Binary64Partial:
    EMPTY or one binary64 subtotal produced by an admitted finite-input
    reduction tree

FloatAggregateState:
    saw_nonnull
    finite_partial as Binary64Partial
    saw_nan
    saw_positive_infinity
    saw_negative_infinity

OrderedCandidate<T>:
    EMPTY or one canonically represented non-NULL scalar T
```

The exact integer states are logically variable-range and never wrap. An
implementation may use multiword integers, another exact representation, or an
owned variable-size backing; host overflow and result-width checked partial
accumulation are not semantic substitutes.

`Binary64Partial` has binary64 semantics at every reduction edge. Host
`long double`, hidden x87 excess precision, an exact dyadic/superaccumulator
result, or another representation is conforming only when its observable result
is proven equal to one admitted §29.3.4 binary64 reduction tree; none defines a
separate aggregate semantic result.

State storage is charged to query memory and may use the existing spill
contract. Failure to preserve required state is an ordinary query
memory/resource error, never permission to omit contributions or return an
unadmitted approximation.

The descriptor's fixed `StateSize` may contain an inline state or an owned
handle to variable backing; any such backing is aggregate-state memory covered
by `Destroy`, query accounting, spill, and cancellation cleanup. Exact integer
range does not require one heap allocation per input value.

### 29.3.2 Closed overload table

`IGNORE NULL` means NULL arguments do not enter the state. `Canonical scalar`
means §29.3.6 normalization before comparison or retention.

| Aggregate | Input TypeId | Result | NULL / empty behavior | Semantic state | Merge | Finalization and numerical error |
|---|---|---|---|---|---|---|
| `COUNT(*)` | no argument | `INT64 NOT NULL` | every child row counts; empty = `0` | exact nonnegative row count, or the equivalent absorbing `COUNT_OVERFLOWED` state after the count is known greater than `INT64_MAX` | exact nonnegative addition; overflowed is absorbing | exact count `0..INT64_MAX` -> INT64; greater -> `NUMERIC_OVERFLOW` |
| `COUNT(expr)` | any concrete scalar TypeId | `INT64 NOT NULL` | count only non-NULL values; empty/all-NULL = `0` | exact nonnegative non-NULL count, with the same permitted overflow marker | same as `COUNT(*)` | same as `COUNT(*)` |
| `SUM` | `INT32` | nullable `INT64` | IGNORE NULL; no non-NULL = NULL | `saw_nonnull` + `ExactSignedInteger` | exact integer addition | exact final sum in `[-2^63,2^63-1]` -> INT64; otherwise `NUMERIC_OVERFLOW` |
| `SUM` | `INT64` | nullable `INT64` | IGNORE NULL; no non-NULL = NULL | `saw_nonnull` + `ExactSignedInteger` | exact integer addition | exact final sum in `[-2^63,2^63-1]` -> INT64; otherwise `NUMERIC_OVERFLOW` |
| `SUM` | `FLOAT64` | nullable `FLOAT64` | IGNORE NULL; no non-NULL = NULL | `FloatAggregateState` | one admitted binary64 addition edge for two nonempty finite partials; commutative OR of explicit-input special flags | §29.3.5 special-input rule; otherwise canonicalized root of the selected legal tree |
| `AVG` | `INT32` | nullable `FLOAT64` | IGNORE NULL; no non-NULL = NULL | `ExactSignedInteger` sum + `ExactNonnegativeCount` | exact componentwise addition | correctly round exact rational `sum/count` once to binary64, nearest/ties-even |
| `AVG` | `INT64` | nullable `FLOAT64` | IGNORE NULL; no non-NULL = NULL | `ExactSignedInteger` sum + `ExactNonnegativeCount` | exact componentwise addition | correctly round exact rational `sum/count` once to binary64, nearest/ties-even |
| `AVG` | `FLOAT64` | nullable `FLOAT64` | IGNORE NULL; no non-NULL = NULL | `FloatAggregateState` + `ExactNonnegativeCount` | admitted binary64 partial addition + exact count addition + special-flag OR | §29.3.5 special-input rule; otherwise one binary64 division of selected subtotal by converted exact count |
| `MIN` | `INT32`, `INT64`, `FLOAT64`, `VARCHAR`, `DATE`, or `TIMESTAMP` | nullable input TypeId | IGNORE NULL; no non-NULL = NULL | `OrderedCandidate<T>` | retain canonical lesser value under Chapter-17 order | canonical candidate; no arithmetic overflow |
| `MAX` | `INT32`, `INT64`, `FLOAT64`, `VARCHAR`, `DATE`, or `TIMESTAMP` | nullable input TypeId | IGNORE NULL; no non-NULL = NULL | `OrderedCandidate<T>` | retain canonical greater value under Chapter-17 order | canonical candidate; no arithmetic overflow |

BOOLEAN remains absent from MIN/MAX because it is not SQL-orderable in the defined architecture.
No aggregate overload performs an unlisted implicit string, Boolean, temporal,
or numeric conversion. The binder and every executor use this same table.

### 29.3.3 COUNT and integer SUM

COUNT's conceptual count never wraps. Because it never decreases, a partial
state proven greater than `INT64_MAX` may be represented by an absorbing
`COUNT_OVERFLOWED` marker. That marker is not surfaced as an error during
Update/Merge: the aggregate continues consuming all semantically required
child rows and evaluating required `COUNT(expr)` arguments. Finalize then
returns `NUMERIC_OVERFLOW`. This prevents batch size or worker scheduling from
using early COUNT termination to suppress a later demanded input-expression or
child-operator error.

Integer SUM is the exact mathematical sum of every non-NULL input. A subtotal
outside the INT64 result range is not itself an SQL error because later values
may cancel it. Only Finalize applies the declared INT64 range check. In
particular, no physical partition is allowed to fail merely because its local
subtotal is outside INT64 while the complete exact sum is representable.

The exact boundaries are:

```text
INT64 result minimum = -9223372036854775808
INT64 result maximum =  9223372036854775807
```

`SUM(INT32)` deliberately retains its existing INT64 result, so values outside
INT32 but inside this INT64 domain are valid. Combine adds exact subtotals and
is mathematically associative and commutative. It never narrows, wraps, or
depends on compiler integer extensions.

Integer AVG uses the same exact sum plus an exact non-NULL count, but does not
materialize or range-check a SUM result first. It converts the exact rational
`sum/count` directly to binary64 with one round-to-nearest, ties-to-even
operation. Since a nonempty average lies within the finite integer input
domain, valid integer AVG inputs cannot create FLOAT64 infinity; resource
failure while preserving the exact state remains separately possible. AVG's
internal count has no INT64 result-width limit and remains exact even when a
separate COUNT over the same input would overflow.

### 29.3.4 FLOAT64 binary reduction and finalization

For one FLOAT64 SUM/AVG group, every demanded non-NULL finite logical
occurrence is one leaf and participates exactly once in a finite binary
reduction tree selected by physical execution. The tree contains no inactive
vector lane, synthetic duplicate, or value-level deduplication. A CONSTANT
vector of cardinality N contributes N leaves, and repeated DICTIONARY
occurrences remain repeated leaves.

Every internal tree edge is one IEEE-754 binary64 addition in round-to-nearest,
ties-to-even mode, and its subtotal is binary64. An empty partial state is the
identity without a synthetic addition; combining two nonempty partials adds
their binary64 subtotals through one such edge. Update may form any admitted
tree over its prior nonempty partial and newly admitted leaves. Input encounter
order where no relational order is required, vector/chunk boundaries, worker
partitioning and scheduling, local-state partitioning, Combine-tree shape,
hash-versus-ordered aggregation, and spill partition/replay shape may select
different legal trees.

Different legal trees may produce different binary64 roots. When competing
roots are finite, this includes finite low-bit differences; edge overflow may
also make one tree finite and another infinite or NaN. Every result must
nevertheless be the root of one admitted tree over exactly the demanded finite
occurrences. Hidden excess precision, a changed rounding mode, an
arbitrary-precision or exact-dyadic reduction, or another mechanically
different representation is permitted only when proven observationally
equivalent to one admitted binary64 tree.

When §29.3.5's explicit-input classification does not determine FLOAT64 SUM,
Finalize uses the selected tree root. Binary64 overflow at a tree edge produces
ordinary IEEE infinity, not `NUMERIC_OVERFLOW`; subsequent tree edges retain
ordinary binary64 behavior. Final result representation maps any zero root to
`+0.0` and any NaN root to the canonical Chapter-17 quiet NaN without
reinterpreting the selected reduction history.

FLOAT64 AVG retains the same selected binary64 subtotal plus an exact
non-NULL count. The count increments for every non-NULL input, including NaN,
infinity, and signed zero, and has no INT64 result-width limit. In the ordinary
finite case, Finalize converts that exact nonnegative integer to binary64 using
round-to-nearest, ties-to-even, then performs one binary64
`subtotal / converted_count` division in the same rounding mode. The exact
count does not become approximate merely because this division operand is
rounded; an internal count beyond finite binary64 conversion range converts to
`+Infinity` rather than creating a COUNT-width error or practical row limit.
AVG does not average worker-local averages. Its exposed zero and NaN
representations are canonicalized exactly as for SUM.

These n-ary aggregate semantics remain distinct from one fixed scalar
expression tree: Chapter 17 fixes each scalar operator boundary, while physical
aggregate execution may select any reduction tree admitted above.

### 29.3.5 FLOAT64 special values

Explicit-input special-value finalization is closed and order-independent for
both SUM and AVG after at least one non-NULL input:

```text
if saw_nan:
    canonical quiet NaN
else if saw_positive_infinity and saw_negative_infinity:
    canonical quiet NaN
else if saw_positive_infinity:
    +Infinity
else if saw_negative_infinity:
    -Infinity
else:
    selected finite-input binary64 reduction from §29.3.4
```

The canonical NaN is the Chapter-17 semantic NaN and persisted bit pattern
`0x7ff8000000000000` when materialized by a canonical scalar codec. NaN payload
or encounter order is never retained. Finite values cannot cancel an explicit
input infinity. An infinity or NaN created by finite-input tree arithmetic does
not retroactively set an explicit-input flag and follows §29.3.4's tree-root
rule. Every zero result, including a zero tree root from only `-0.0` inputs,
mixed signed zeros, or cancellation in the selected tree, finalizes as `+0.0`
for SUM and AVG.

### 29.3.6 MIN/MAX canonical representation

MIN/MAX use the Chapter-17 SQL total order and canonical equality, not raw host
comparisons. Before a FLOAT64 candidate enters an ordered aggregate state:

```text
any NaN  -> canonical quiet NaN
-0.0 or +0.0 -> +0.0
```

Other scalar candidates retain their canonical owned representation; VARCHAR
uses exact bytes. Merge selects the lower/higher semantic class, and equal
classes retain the same canonical representative. Consequently:

```text
MIN(+0.0,-0.0) = +0.0
MAX(+0.0,-0.0) = +0.0
MIN(NaN,1.0)   = 1.0
MAX(NaN,1.0)   = canonical NaN
```

Multiple NaN payloads cannot make output bytes depend on encounter order.

### 29.3.7 Merge, execution shape, spill, and errors

COUNT, integer SUM/AVG, and MIN/MAX Merge retain the aggregate's initialized
empty state as identity and their existing exact associative semantic laws:
exact integer/count addition or canonical MIN/MAX selection. Every legal Merge
tree over those states has the same Finalize value/error.

FLOAT64 SUM/AVG use the different §29.3.4 rule. An empty finite partial is the
identity; Combine of two nonempty finite partials contributes one binary64
addition edge, exact counts combine exactly, and explicit-input special flags
combine by commutative OR. A different legal Combine tree may change its
binary64 root, including finite low bits or overflow-generated infinity/NaN,
but cannot change occurrence membership, NULL admission, exact count,
explicit-input special classification, result type/nullability, or canonical
exposed zero/NaN representation. In-memory state bytes need not be identical
when that difference is not otherwise observable.

These rules apply independently to the global state and every grouped state.
Vector Update may batch work, parallel workers may own local states, and
hash/sorted aggregation may visit groups in different orders. No path may use
result-width integer subtotals, average partial averages, or admit a FLOAT64
result outside the §29.3.4 tree model. Hash output group order remains
unspecified. Capability-enabled `PhysicalSortAggregate` and
`PhysicalHashAggregate` have identical semantics except for FLOAT64 SUM/AVG's
tree-selected binary64 root expressly admitted by different legal trees.

Spill/repartition preserves every semantic component. It may spill/replay
qualifying argument values or serialize query-temporary partial state. A
serialized FLOAT64 partial stores the exact binary64 subtotal bits, exact count
where applicable, explicit-input special flags, and other descriptor-owned F2
state; it is not converted through decimal text. Restored state continues as a
node in an admitted tree. Spill partition/replay shape may therefore change the
FLOAT64 tree root, but cannot omit or duplicate contributions, narrow an exact
count, lose flags, or change another aggregate result. Temporary encoding
remains query-local under Chapter 24 and changes no database persisted format.

Aggregate constant evaluation over an exactly known relational input uses this
same registry and selects one admitted FLOAT64 tree; it need not reproduce every
other legal runtime tree. Exact integer/count state and all other aggregate
state are ordinary query-accounted memory. If required memory or spill fails,
execution reports the existing `OutOfMemory` or `SpillIOError`; it never
uses resource failure to omit work or select an unadmitted approximation.

The exact-aggregate invariance and bounded FLOAT64 variability assume successful
demanded argument/child evaluation and availability of resources admitted by
the query-memory/spill contracts. External OOM, disk-full, cancellation, or
spill I/O failure remains an operational query failure and may prevent any
aggregate value from being produced; it cannot legalize a result outside the
applicable aggregate contract.

Input-expression/child errors occur before aggregate finalization when their
relational path is semantically demanded. COUNT overflow is not raised early,
and integer SUM cannot raise from an intermediate subtotal. After complete
input consumption, every group's aggregate states are numerically validated
before the first aggregate result row is externally exposed. All aggregate
range failures—COUNT result width and integer SUM result width—are
`NUMERIC_OVERFLOW`; if more than one aggregate descriptor would fail, the
descriptor with the lowest semantic aggregate ordinal supplies
the diagnostic. The binder assigns that ordinal by first aggregate occurrence
in ascending source-byte position within the query block and carries it through
rewrites; repeated shared occurrences retain the first ordinal. Group identity
and hash/finalization order are not part of the error. Resource failures remain
their existing operational categories. §39.1 alone determines the transaction
effect, including aggregate errors inside DML subqueries after published writes.
If any group has a failing numeric finalization, the whole query fails and no
successful aggregate-row prefix is exposed.

### 29.3.8 Normative boundary vectors

The following use already-decoded input values. `TREE-INVARIANT` means every
admitted execution shape has the displayed result. `TREE-DEPENDENT` gives
results for explicit legal parenthesizations and does not establish one
universal result for that input bag.

| Inputs / tree | Classification | Required result |
|---|---|---|
| `SUM(FLOAT64: ((1e16 + -1e16) + 1))` | TREE-DEPENDENT | `1.0` |
| `SUM(FLOAT64: (1e16 + (-1e16 + 1)))` | TREE-DEPENDENT | `+0.0` after result canonicalization |
| `SUM(+0.0)` | TREE-INVARIANT | `+0.0` |
| `SUM(-0.0)` | TREE-INVARIANT | `+0.0` |
| `SUM(+0.0,-0.0)` | TREE-INVARIANT | `+0.0` |
| `SUM(+Infinity, finite...)` | TREE-INVARIANT | `+Infinity` unless the explicit-input rule produces NaN |
| `SUM(-Infinity, finite...)` | TREE-INVARIANT | `-Infinity` unless the explicit-input rule produces NaN |
| `SUM(+Infinity,-Infinity)` | TREE-INVARIANT | canonical NaN |
| `SUM(any input NaN, finite...)` | TREE-INVARIANT | canonical NaN |
| `SUM(max_finite,max_finite)` | TREE-INVARIANT | `+Infinity`; no `NUMERIC_OVERFLOW` |
| `SUM(((max_finite + max_finite) + -max_finite))` | TREE-DEPENDENT | `+Infinity` |
| `SUM(max_finite + (max_finite + -max_finite))` | TREE-DEPENDENT | `max_finite` |
| `SUM(smallest_positive_subnormal)` | TREE-INVARIANT | that exact subnormal (`2^-1074`) |
| `SUM(1.0,2^-53)` | TREE-INVARIANT | `1.0` by edge rounding, ties-to-even |
| `SUM(INT64_MAX)` | TREE-INVARIANT | `INT64_MAX` |
| `SUM(INT64_MAX,1)` | TREE-INVARIANT | `NUMERIC_OVERFLOW` |
| `SUM(INT64_MAX,1,-1)` | TREE-INVARIANT | `INT64_MAX` |
| `SUM(INT64_MIN)` | TREE-INVARIANT | `INT64_MIN` |
| `SUM(INT64_MIN,-1)` | TREE-INVARIANT | `NUMERIC_OVERFLOW` |
| `SUM(INT64_MIN,-1,1)` | TREE-INVARIANT | `INT64_MIN` |
| `SUM(INT32_MAX,1)` | TREE-INVARIANT | INT64 `2147483648` |
| `COUNT = INT64_MAX` | TREE-INVARIANT | `INT64_MAX` |
| `COUNT = INT64_MAX+1` | TREE-INVARIANT | `NUMERIC_OVERFLOW` at Finalize |
| `AVG(empty)` or `AVG(all NULL)` | TREE-INVARIANT | typed NULL |
| `AVG(INT: 1,2)` | TREE-INVARIANT | exactly `1.5` |
| `AVG(INT64_MAX,INT64_MIN)` | TREE-INVARIANT | exactly `-0.5` |
| `AVG(FLOAT64: ((1e16 + -1e16) + 1))` | TREE-DEPENDENT | binary64 division `1.0 / 3.0`, bits `0x3fd5555555555555` |
| `AVG(FLOAT64: (1e16 + (-1e16 + 1)))` | TREE-DEPENDENT | `+0.0` |
| `AVG(+Infinity)` | TREE-INVARIANT | `+Infinity` |
| `AVG(+Infinity,-Infinity)` | TREE-INVARIANT | canonical NaN |
| `AVG(any input NaN,1.0)` | TREE-INVARIANT | canonical NaN |
| `AVG(-0.0)` / `AVG(+0.0,-0.0)` | TREE-INVARIANT | `+0.0` |
| `AVG(smallest_positive_subnormal,+0.0)` | TREE-INVARIANT | `+0.0`; binary64 division is halfway and ties to even zero |

The empty-input COUNT/SUM/MIN/MAX cases remain exactly those in the registry
table. Boundary verification may inject synthetic partial states rather than
materialize `INT64_MAX` rows.

### 29.3.9 Forbidden aggregate implementations

The architecture explicitly forbids:

1. omitting, duplicating, or deduplicating a demanded logical aggregate occurrence;
2. treating one CONSTANT payload as one contribution regardless of cardinality, collapsing repeated DICTIONARY occurrences, or reading inactive lanes;
3. using a FLOAT64 rounding mode other than binary64 nearest/ties-even at a reduction edge or final AVG division;
4. using host `long double`, hidden excess precision, an exact accumulator, or another mechanism to produce a FLOAT64 result that is not equivalent to an admitted §29.3.4 tree;
5. using `__int128` only when available and silently using INT64 accumulation elsewhere for exact integer state;
6. reporting integer SUM overflow from an order-specific partial subtotal when the exact final sum fits;
7. wrapping/saturating COUNT to an INT64 value instead of final `NUMERIC_OVERFLOW`;
8. averaging worker-local averages, weighted or unweighted, instead of combining FLOAT64 subtotal and exact-count state;
9. narrowing, rounding, wrapping, or otherwise corrupting AVG's exact count;
10. losing or deriving explicit-input NaN/infinity flags from physical encounter order;
11. exposing a noncanonical FLOAT64 NaN payload or negative-zero SUM/AVG result;
12. spilling FLOAT64 partial state through decimal text, changing its binary64 subtotal bits, losing flags, or narrowing exact integer/count state;
13. applying FLOAT64 tree variability to COUNT, integer SUM/AVG, MIN/MAX, grouping classes, or canonical GROUP BY/DISTINCT representatives;
14. returning one group's rows before another group's aggregate overflow is validated;
15. using physical group order to choose a different aggregate error category/ordinal;
16. folding a constant FLOAT64 aggregate to a result that corresponds to no admitted reduction tree;
17. falling back to omitted work, an unadmitted approximation, or incompatible precision after memory/spill failure.

## 29.4 Group hash table

Grouped aggregation uses a hash structure conceptually similar to hash join:

```text
hash directory
    ↓
one group row/state
```

Each group row owns:

```text
deep-copied group key
properly aligned aggregate state block
optional cached hash
```

Group equality uses the explicit GROUPING hash/equality mode from §28.2.1.

Therefore:

```text
NULL == NULL for grouping identity
-0.0 == +0.0 for FLOAT64 grouping
canonical-equivalent NaNs group together
VARCHAR grouping is binary-byte equality
```

This does not alter ordinary SQL `=` semantics with NULL.

The group table's internal key representation is implementation-defined subject
to the equality, ownership, and hash-compatibility rules above. When a grouping
key becomes relational output, every component is materialized using the
canonical representative required by §20.9 and defined in §17.10.3. First input
occurrence, hash seed, bucket layout or growth, worker/Combine order, spill
partition/replay order, and hash-versus-ordered aggregation therefore cannot
select different output bits for one group.

All group-table/key/state memory is accounted through the aggregate's memory reservation.

## 29.5 Global aggregate fast path

With no GROUP BY: `one aggregate state block` is sufficient.

No group hash table is created. Input vectors update the aggregate states directly. On empty input, this path still emits exactly one aggregate result row, with the per-function empty-input semantics from §29.3.

## 29.6 Hash-aggregate spill

When grouped state exceeds its reservation, execution partitions by group hash and uses SpillManager.

The robust spill path may spill:

```text
group keys
aggregate argument values / raw qualifying rows
```

then finish one partition at a time and combine equal groups. It does **not** require opaque in-memory aggregate state bytes to be a stable spill format.

An aggregate descriptor is eligible for serialized partial-state spilling only
when it defines safe temporary serialization plus Combine semantics.

For every aggregate in §29.3, either path preserves its exact
integer/count state, canonical-candidate state, or admitted FLOAT64 F2 state as
applicable. FLOAT64 spill may replay raw qualifying values or serialize the
exact bits of a binary64 subtotal together with the exact count where
applicable, explicit-input special flags, and required descriptor state.
Serialization through decimal text, result-width integer partials, lost flags,
and narrowed counts are not safe spill states. Spill/replay may select a
different legal FLOAT64 reduction tree and therefore a different binary64 root.

Recursive repartition is bounded in the same spirit as Grace hash join.

Aggregate spill MUST produce exactly the same groups and every invariant result
component as an unlimited-memory execution. A FLOAT64 SUM/AVG root may differ
only when each result corresponds to its path's admitted §29.3.4 tree.

## 29.7 DISTINCT

The baseline hash `PhysicalDistinct` is a blocking
Sink -> successful Finalize -> Source operator and may reuse the group-hash
infrastructure with:

```text
all output columns = grouping key
zero aggregate states
```

Its Sink consumes every semantically demanded input occurrence, evaluates the
required DISTINCT values, and inserts or locates their duplicate classes under
GROUPING-equivalence hash semantics, retaining exactly one query-owned state
entry per duplicate class. Merely observing one previously unseen class does
not authorize skipping later demanded input, and the hash path emits no row
during Sink.

Finalize completes the hash DISTINCT build and publishes successful readiness.
No dependent Source may observe incomplete or failed build state. After
successful Finalize, Source emits exactly one result row for each duplicate
class, materialized componentwise using the canonical representative required
by §20.10 and defined in §17.10.3. Nonexecution and early termination remain
governed by the semantic-demand and pipeline contracts in Chapters 20 and 26,
not by physical first encounter of a class.

Its equality is the same SQL grouping/distinct equivalence from Chapter 20:

```text
all NULLs in one duplicate class
FLOAT64 grouping equivalence
binary VARCHAR equality
```

Hash DISTINCT advertises no ordering property.

Ordered/streaming DISTINCT is a separate capability-conditional physical
alternative and does not change the baseline hash path's blocking contract.

## 29.8 Aggregation output properties

Hash aggregation/group hash DISTINCT do not preserve input ordering. Global aggregate produces at most one result row. Grouped output order is unspecified unless a different physical operator explicitly advertises order. The physical optimizer must insert/use Sort when SQL requires an ordering not otherwise produced.

## 29.9 Aggregation invariants

1. Aggregate updates are vectorized/batch-oriented.
2. Aggregate state size/alignment are descriptor-owned and respected by group storage.
3. COUNT results are INT64 NOT NULL, return zero on empty input, and raise NUMERIC_OVERFLOW rather than wrap above INT64_MAX.
4. SUM/MIN/MAX/AVG return NULL when no non-NULL input exists.
5. Integer SUM/AVG use exact mathematical integer states; only SUM Finalize applies the INT64 result range.
6. GROUP BY/DISTINCT use grouping equality, not ordinary NULL comparison.
7. Hashing and grouping equality agree.
8. Group keys/varlen data retained by a group table are deep-copied into group-owned storage.
9. Global aggregation avoids a hash table.
10. Spill does not require raw in-memory aggregate-state bytes to become a compatibility format.
11. Spilled, serial, parallel, hash, and capability-enabled ordered aggregation preserve every §29.3 invariant; only FLOAT64 SUM/AVG's tree-selected binary64 root may differ through admitted reduction trees.
12. FLOAT64 SUM/AVG use binary64 partial reduction, exact AVG count, order-independent explicit-input special flags, and canonical exposed zero/NaN representations.
13. MIN/MAX retain canonical representatives for FLOAT64 zero/NaN equality classes.
14. Hash aggregate/DISTINCT advertise no ordering.
15. GROUP BY and DISTINCT output materialize the canonical representatives from §§17.10.3 and 20.9–20.10 independently of physical execution order or algorithm.
16. PhysicalSortAggregate retains finalized rows in accounted, spill-capable, order-preserving query-temporary storage and exposes them only after complete successful readiness.

## 29.10 Ordered aggregation and streaming DISTINCT

`PhysicalSortAggregate` and ordered/streaming DISTINCT are
capability-conditional physical alternatives. Sections 22.4.1 and 38.24 admit
them only when the selected runtime capability is enabled and final physical-
plan validation succeeds.

`PhysicalSortAggregate` requires input ordered by its grouping keys.

The required compatible input `OrderingProperty` must be proven under Chapter
37. When planning enforces it with `PhysicalSort`, Chapter 30 owns physical
sorting and the complete comparator. The admitted ordering must make every
grouping-equivalence class contiguous.

When that requirement is satisfied, mutable aggregate execution state may be
approximately bounded by one active key group. This is an aggregate-state
bound, not a bound on total query memory or retained result size.

Once a group's complete partial state has been combined and the group closes,
`PhysicalSortAggregate` Finalizes and validates its aggregate states and
materializes the complete result row. A successful row is copied into an
order-preserving `RowCollection` or implementation-equivalent query-owned
retained-row collection. That storage owns stable fixed and varlen values,
uses exact extent arithmetic, is continuously `QueryMemoryManager`-accounted,
and is eligible for `SpillManager` temporary storage under Chapter 24. It is
query/attempt-local state with no WAL, persistence, recovery, or retry reuse.
Its iteration preserves only the output `OrderingProperty` proven for the
selected physical plan.

Retaining a finalized row is not a dependent-pipeline handoff, client-visible
output, or successful query publication. The retained rows become this
operator's Source only after all semantically demanded input is consumed, all
required local/global Combine work completes, every demanded group Finalizes
and validates successfully, all retained-output preparation succeeds, and the
blocking dependency publishes successful readiness under Chapter 26. A later
group or pre-readiness retained-output failure discards prior retained rows and
exposes no aggregate prefix.

Materialization, reservation, retained-row representation, varlen ownership,
spill, cancellation, and cleanup failures retain the categories and transaction
effects in Chapters 24 and 39; no aggregate-spool error or numerical
approximation fallback exists. A spill read failure while the ready Source is
being consumed remains a failed query, and any already returned external
prefix retains Chapter 31's general cursor semantics rather than becoming a
successful complete result. The operator does not rewind or re-execute its
original input to produce output.

Otherwise physical planning may enforce the required grouping order with `PhysicalSort` and compare: `Sort + PhysicalSortAggregate` against `PhysicalHashAggregate`.

An ordered/streaming DISTINCT similarly requires an input `OrderingProperty`
proven under Chapter 37 and ordered compatibly by every DISTINCT key. The
Chapter-30 comparator used to establish that order must make every DISTINCT
equivalence class contiguous, after which the operator can emit one row per
adjacent duplicate class. That row uses the same componentwise canonical
representative required by §20.10; the first physical member of the adjacent
class does not define its output representation.

These implementations use the same grouping equality as §20.9/§29.7.

They also use the same §29.3 aggregate state and Finalize rules. Input ordering
may reduce memory or change unspecified group-row order; it cannot define a
different MIN/MAX representative, GROUP BY key representative, or DISTINCT
class representative. Different admitted FLOAT64 SUM/AVG reduction trees may
produce different binary64 roots under §29.3.4. `PhysicalSortAggregate`
materializes each grouping-key component under §20.9 exactly as
`PhysicalHashAggregate` does.

These operators advertise only the provided properties proven for the selected
physical plan under Chapter 37. Chapter 38 owns capability eligibility,
costed enumeration, and final physical-plan validation.
