# Rewrite Pass 15 — Join Optimization, Properties, Memo, Diagnostics, and Optimizer Verification

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `628..725`
- processed source lines: `18781..21290`
- legacy architecture modified: **no**
- production code modified: **no**
- Pass 16 reconciliation performed: **no**

Working architecture:

```text
input  SHA-256: 77edb72e4872c9a3a6665e8a969b0f7c1a420b439c06b2c8dbc45832459f326d
output SHA-256: 35decf86d6f0cc4244789bdd36e1844244bfcdf0b3c2a1f6d9d9112e05d03281
```

## Canonical optimizer completion

Pass 15 replaces the remaining optimizer placeholders with:

```text
Chapter 37  Physical Properties and Join Enumeration
Chapter 38  Memo, Costed Physical Search, and Memory-Aware Optimization
```

and synchronizes logical rewrite, capability gating, estimation metadata, diagnostics, verification, performance, deferred scope, invariants, and rewrite progress.

## Physical properties

V1 tracks only:

```text
OrderingProperty
RequiredSlotSet
```

with exact ordering-prefix satisfaction on:

```text
LogicalSlotId
direction
NULL order
collation
```

Computed order expressions are represented by retained/hidden logical slots before property matching.

Only runtime-guaranteed ordering is advertised.

Interesting orders arise only from known downstream opportunities such as ORDER BY, ordered aggregate/DISTINCT, MergeJoin, and IndexScan.

## Join search

Relation identity is `BindingId`.

Small reorderable INNER/CROSS regions use bushy DP with default configurable exhaustive threshold:

```text
10 relation bindings
```

Symmetric partitions are removed deterministically.

LEFT JOIN boundaries remain constrained.

Above the threshold, or when planning-memory pressure demands it, search uses a deterministic greedy connected seed plus bounded local improvement.

Initial default:

```text
4 local-improvement passes
```

Join order and physical join algorithm/orientation are costed together.

## Capability gating

A cost formula is not sufficient to make an operator executable.

The physical planner enumerates only capability-enabled implementations.

This makes the existing architecture precise for:

```text
MergeJoin
SortAggregate
ordered/streaming DISTINCT
```

which are architecture-supported but not mandatory baseline runtime implementations.

## Costing

Pass 15 canonicalizes cost models for:

```text
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
MergeJoin
Sort
TopN
HashAggregate
SortAggregate
DISTINCT
property enforcement
materialization
spill
```

Physical memory cost uses projection-pruned payload width and exact aggregate state sizes where available.

Planner memory targets are assigned against simultaneously-live blocking states in the pipeline dependency graph rather than giving every operator the full query budget.

## LIMIT / startup objective

`RequiredRowsObjective` is explicitly **not** a physical property.

It is:

```text
ALL_ROWS
or
FIRST_K_ROWS(K)
```

A finite requirement participates in search/memo identity wherever it is propagated, preventing the full-result cheapest alternative from incorrectly dominating a low-startup plan useful for LIMIT.

Propagation remains conservative across blocking operators and joins.

## Memo/dominance/determinism

Plan alternatives retain:

```text
rows
width
Cost
ordering
memory
spill
physical prototype
canonical tie key
```

Dominance preserves useful ordering and finite-required-row startup alternatives.

Cost ties use configurable relative epsilon:

```text
1e-9 default
```

and a collision-free canonical structural comparison.

A compact FNV-1a-64 plan fingerprint is diagnostic only.

No pointer/hash-map iteration order chooses a plan.

## Estimation metadata

Missing statistics use centralized fallback configuration.

Estimates carry:

```text
HIGH / MEDIUM / LOW confidence
provenance tags
```

such as:

```text
MCV_HIT
HISTOGRAM_RANGE
UNIQUE_KEY
INDEPENDENCE_ASSUMPTION
MULTICOLUMN_DAMPING
MISSING_STATISTICS
STALE_STATISTICS
```

V1 cost search does not add a separate confidence penalty; the metadata is diagnostic.

## Logical optimizer rewrites

Pass 15 synchronizes the logical rewrite owner with:

```text
inner-join equality equivalence classes
safe constant propagation
contradiction/provably-empty detection
trusted key-property reasoning
```

without enabling foreign-key join elimination or unsafe outer-join propagation.

## Diagnostics

Optimizer diagnostics now include:

```text
normal EXPLAIN estimates/cost/order/memory/spill
optimizer trace
plans retained/pruned
interesting orders
memory targets/spill expectations
plan fingerprint
planning time/memory counters
estimate confidence/provenance
```

`EXPLAIN ANALYZE` q-error is exact:

```text
positive/positive:
    max(E/A, A/E)

0 / 0:
    1

one side zero:
    infinity marker
```

Runtime actual cardinalities are not written automatically into persistent statistics.

## Verification/performance

Architecture-level verification now covers:

```text
join order
bushy plans
outer constraints
interesting orders
memory-sensitive plan changes
required_rows
memo dominance/determinism
optimizer fuzzing
differential optimizer correctness
plan regressions
```

Planning benchmarks cover join counts from small through ~30 relations and demonstrate the exhaustive-to-bounded transition.

Cost benchmarks compare predicted relative ranking with observed runtime ranking rather than demanding cost units equal milliseconds.

Benchmark query/table-name fingerprints may not be hardcoded into the optimizer.

## Classification of legacy §§718–725

The concrete optimizer source-tree layout, implementation order, five optimizer milestones, and the historical `BEGIN IMPLEMENTATION` status directive are not copied into canonical architecture.

Their lasting architectural obligations already live in Chapters 33–38 and the cross-cutting requirements.

The unique end-to-end dependency summary from legacy §725 is already represented by Chapter 2.

Implementation/project phase state remains outside architecture.

## Coverage

```text
legacy §§0..725   explicit disposition complete
pending sections  0
```

This is **not** architectural cutover.

Pass 16 still performs the full document reconciliation, supporting-document preservation decision, stale rewrite-meta cleanup, cross-reference audit, and explicit cutover review.

## Issue state

No new unresolved optimizer architecture gap was introduced.

`R-049` records/resolves the Pass-15 search/property precision completions.

Earlier pre-Pass-15 upper-core gaps remain resolved.

## Validation

Mechanical validation confirms:

- pinned legacy SHA unchanged,
- §628 starts at line 18781,
- §725 reaches the legacy EOF at line 21290,
- every legacy numbered section 0..725 is non-PENDING,
- Chapters 37 and 38 occur exactly once,
- optimizer property/memo/required-rows/capability contracts are present,
- internal section references resolve,
- production code and legacy architecture were not modified.

## Exit status

**PASS 15 COMPLETE.**

The next and final rewrite task is:

```text
Pass 16 — full-document reconciliation and cutover review
```
