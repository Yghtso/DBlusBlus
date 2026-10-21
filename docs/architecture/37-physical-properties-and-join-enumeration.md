# 37. Physical Properties and Join Enumeration

## 37.1 Scope

This chapter defines the physical properties and join-search space used by the cost optimizer.

The property system intentionally remains small.

It tracks:

```text
OrderingProperty
RequiredSlotSet
```

The following remain optional property-extension categories:

```text
partitioning
rewindability
materialization
```

The baseline does not track them or construct a full property lattice.

`required_rows` for LIMIT-sensitive optimization is an optimization objective/requirement, not a physical data property; §38.16 defines how it participates in search identity when propagated.

## 37.2 OrderingProperty

An ordering is an ordered vector of:

```text
OrderKey {
    LogicalSlotId slot
    ASC | DESC
    NULLS_FIRST | NULLS_LAST
    collation
}
```

Physical property reasoning is slot-based.

If SQL ordering refers to a computed expression, logical/physical planning retains or introduces a hidden `LogicalSlotId` for that resolved expression before ordering-property comparison.

Collation is the binary VARCHAR collation from Chapter 17.

Before an ordering participates in property identity, memo lookup, satisfaction,
interesting-order retention, or enforcement, it is normalized by scanning its
keys from left to right and removing every later **exact duplicate** `OrderKey`.
Exact duplicate means identical `LogicalSlotId`, direction, NULL order, and
collation. The first occurrence is retained. Keys that differ in any of those
four fields remain distinct; matching display text, lineage, a derived equality,
or statistics is not sufficient for removal.

This normalization changes only the property vector. It does not remove the
retained slot, suppress evaluation of a demanded ordering expression, or merge
distinct computed-expression occurrences. Two separately bound computed
expressions with different hidden `LogicalSlotId` values therefore remain
different keys even when their source text is identical.

An empty key vector means:

```text
no required/provided ordering
```

Ordering properties are query-local and never persistent metadata.

## 37.3 Ordering satisfaction

An available ordering satisfies a required ordering exactly when the required keys are a prefix of the available keys and every corresponding key has identical:

```text
LogicalSlotId
direction
NULL order
collation
```

Therefore:

```text
available: (a ASC NULLS FIRST, b ASC NULLS FIRST, c ASC NULLS FIRST)
required:  (a ASC NULLS FIRST, b ASC NULLS FIRST)
```

is satisfied.

Changing direction, NULL order, collation, or slot identity breaks satisfaction.

A longer ordering is useful as a provider of its exact prefixes.

The planner never infers descending B+ order from a forward-only index scan.

## 37.4 RequiredSlotSet

Each physical-planning subproblem has a required set of output `LogicalSlotId` values.

The set contains every value still needed by an ancestor for:

```text
final output
predicate evaluation
join keys/residual predicates
grouping/aggregate arguments
sorting
DML assignments / RETURNING
hidden DML target RID/system state
```

Projection pruning may remove every other value.

For a fixed optimizer invocation, the required slots of a join `RelationSet` are derived deterministically from final requirements and predicates that cross its boundary.

This lets the common join-DP key use:

```text
RelationSet + OrderingProperty
```

while preserving the required-output contract.

## 37.5 Provided-ordering rules

A physical operator advertises only ordering that its runtime implementation actually guarantees.

Baseline rules are:

```text
PhysicalSeqScan:
    no ordering

forward PhysicalIndexScan:
    compatible ascending index-key ordering
    (ASC / NULLS FIRST in the v1 key schema)

PhysicalFilter:
    preserves input ordering

simple/reference PhysicalProject:
    preserves ordering for retained slots
    only while the corresponding ordered keys survive unchanged

PhysicalLimit:
    preserves input ordering

PhysicalSort:
    provides its exact sort ordering

PhysicalTopN:
    provides its exact final ordering

PhysicalHashJoin:
PhysicalHashAggregate:
hash PhysicalDistinct:
    no ordering
```

Nested-loop/index-nested-loop and capability-enabled merge/ordered-aggregate implementations may advertise additional order only when their execution contract explicitly guarantees it.

The capability registry from §22.4.1 gates such alternatives.

## 37.6 Interesting orders

The optimizer retains some non-cheapest alternatives because their ordering may reduce later work.

Interesting orders originate from:

```text
final ORDER BY
GROUP BY / ordered-aggregate opportunity
merge-join key requirements
usable index access order
DISTINCT / ordered-distinct opportunity
```

Only orderings normalized by §37.2 that can satisfy a known current/downstream requirement are retained as interesting.

Arbitrary orderings are not generated merely to enlarge the memo.

If one available ordering satisfies another, dominance may treat the longer ordering as at least as useful when all other relevant requirements are equal.

## 37.7 RelationSet

A reorderable join region assigns one bit position to each participating relation occurrence.

Identity is:

```text
BindingId
```

not `TableId`.

A self-join therefore consumes two different relation bits.

`RelationSet` is a value-like bitset abstraction supporting:

```text
union
intersection
difference
subset
cardinality
deterministic iteration
```

The concrete representation is implementation-specific, but it MUST represent
every `BindingId` occurrence in the complete reorderable region exactly. Chapter
37 defines no separate relation-count or native-word-sized planning limit. A
fixed-width representation may be used only when the region fits; otherwise the
planner uses a wider/growable representation or reports the existing
`OptimizerResourceLimit` outcome if exact search state cannot be represented
within the configured planning arena even for §37.13's bounded fallback. It
MUST NOT truncate, wrap, merge bits, or reject supported SQL merely because a
native integer word is too small.

Before search-state construction, the planner determines the exact region
occurrence count and verifies that its chosen representation can preserve every
bit. A representation-capacity defect, truncation, or arithmetic failure not
caused by the configured planning-resource bound is `OptimizerError`/internal
invariant failure; genuine exhaustion of the dedicated planning arena retains
the §38.21/§39.4 bounded-fallback and `OptimizerResourceLimit` contract.

## 37.8 Join graph

For a maximal INNER/CROSS reorderable region, construct:

```text
vertices:
    BindingId relation occurrences

edges:
    join predicates spanning relation sets

vertex predicates:
    local filters
```

Safe equality-equivalence predicates derived by §20.17.7 may add connectivity or access opportunities.

Each edge/predicate retains the exact referenced `RelationSet` so a DP partition can determine whether that predicate crosses its two sides.

For one predicate occurrence with referenced relation set `R`, a candidate node
with available relation set `S` may evaluate or discharge that occurrence only
when:

```text
R is a subset of S
```

For a binary partition `S = L union U`, where `L` and `U` are nonempty and
disjoint, that occurrence is a crossing predicate exactly when:

```text
R is a subset of S
R intersects L
R intersects U
```

and placement at that node is legal under Chapter 20's demand, error, and
join-rewrite rules. Partial intersection never makes a predicate available.

Each movable predicate occurrence is assigned to the lowest legal join node in
the candidate tree where all of its referenced bindings are available and it
crosses the two children. This gives that tree one owning placement: the
predicate is not evaluated prematurely, omitted, or attached again at an
ancestor. A one-relation predicate remains vertex/local work. A predicate that
cannot be moved safely remains at its canonical semantic boundary rather than
becoming an unrestricted graph edge. Separately bound occurrences with the same
text remain separate; a derived equality is separate metadata and does not
discharge or duplicate the original demanded expression.

The owning placement is also the one logical selectivity and expression-work
stage for that occurrence in that candidate tree. Chapter 35 remains the
selectivity owner, Chapters 36 and 38 remain the work/cost owners, and the rule
does not mean that a predicate is evaluated only once for the whole statement:
it is evaluated over every candidate occurrence required by its owning physical
operator.

LEFT JOIN and other non-reorderable semantic boundaries are represented as constrained/atomic inputs rather than silently flattened into this unrestricted graph.

## 37.9 Reorderable regions and outer-join constraints

The optimizer searches maximal regions in which INNER/CROSS associativity/commutativity is semantically legal.

A LEFT JOIN boundary retains its logical nesting relative to dependent relations in the defined architecture.

Within each side of that boundary the optimizer may still choose:

```text
base access paths
inner-join order
physical join algorithm
```

The optimizer does not reorder across the outer-join boundary unless an earlier, independently proven logical rewrite converts it to semantics where that reorder is valid.

Advanced outer-join reordering is outside the baseline. Only an independently proven Chapter-20 logical rewrite and an applicable supported physical capability may produce a plan that crosses the original boundary.

## 37.10 Bushy dynamic programming

For a small reorderable `RelationSet S`, use bottom-up bushy subset DP.

For every subset size from one upward:

```text
for each subset S:
    for each legal non-empty partition:
        S = A ∪ B
        A ∩ B = ∅

        combine retained useful-property plans
        from A and B
```

Symmetric partitions are skipped by one deterministic rule, for example requiring the least BindingId bit in `S` to belong to `A`.

For each target subset, §37.12's admission rule determines whether a partition
with no newly activated crossing predicate is a necessary Cartesian assembly.
Predicate-crossing partitions are considered first. A Cartesian partition is
enumerated only when §37.12 admits it; “connected first” is not permission to
enumerate every other Cartesian partition afterward.

The search considers bushy trees such as:

```text
(A ⋈ B) ⋈ (C ⋈ D)
```

rather than restricting the optimizer to left-deep plans.

## 37.11 Exhaustive threshold

`exhaustive_join_limit` is a mandatory nonnegative integer optimizer-search
configuration value. Its default is:

```text
exhaustive_join_limit = 10 relation bindings
```

within one reorderable region. Zero is valid and selects bounded heuristic mode
for every nonempty reorderable region. A configured value must preserve its
exact mathematical nonnegative value in the implementation's validated
configuration representation; a missing, negative, or unrepresentable value is
invalid optimizer configuration and is rejected as `OptimizerError` before any
join-search state is constructed. It is never wrapped or clamped.

The threshold counts distinct `BindingId` occurrences in one maximal legal
reorderable region, not distinct tables, workers, morsels, output rows, or the
whole statement across separate regions. For a region of size `N`:

```text
N <= exhaustive_join_limit:
    exhaustive bushy DP, unless the planning-resource guard triggers fallback

N > exhaustive_join_limit:
    bounded §37.13 heuristic
```

Equality is therefore on the exhaustive side of the boundary.

The threshold is a planning-tuning parameter, not a SQL semantic or relation-count limit.

Raising it requires planning-time/memory evidence.

Chapter 33's retained optimizer-configuration identity fixes this value for one
optimizer invocation. An external configuration change may affect a later
invocation but cannot change the active invocation's mode boundary.

## 37.12 Cartesian products

A partition with no newly activated crossing predicate under §37.8 is a
Cartesian alternative. Cartesian products remain legal, but the search admits
such a partition only in either of these cases:

1. **multi-relation-predicate prerequisite** — the target subset is a proper
   subset of the exact referenced set of an as-yet unavailable predicate that
   references at least three relations, and assembling the subset can make that
   predicate available at a later binary node; or
2. **component assembly** — both children are unions of complete connected
   components of the region's predicate hypergraph and no predicate can connect
   those component groups. Singleton vertices are components. Explicit CROSS
   JOINs therefore remain representable through component assembly.

Hypergraph connectivity treats each exact multi-relation predicate set as one
hyperedge; it is not silently decomposed into pairwise predicates. Safe derived
predicates may contribute edges only under §20.17.7.

For a target subset with one or more legal predicate-crossing partitions, every
such partition is enumerated and Cartesian partitions for that target are
excluded. If there is no predicate-crossing partition, only the two necessary
classes above are admitted. Thus in a pairwise chain `A--B--C`, subset `{A,C}`
is not constructed, while a sole predicate over `{A,B,C}` permits a
predicate-free two-relation prerequisite and activates the predicate only at
the complete three-relation node. For disconnected `{A,B}` and `{C,D}`
components, each component is optimized normally and their complete retained
alternatives may be combined by a Cartesian partition.

This rule is a search-space restriction, not semantic proof. No predicate is
dropped, no Cartesian cardinality is treated as exact emptiness, and a required
Cartesian assembly remains subject to applicable physical capability, cost,
properties, and final validation.

Their logical cardinality begins from:

```text
rows_A * rows_B
```

before later applicable filtering.

They receive the corresponding materialization/CPU/memory cost rather than an arbitrary correctness-changing ban.

## 37.13 Large-join heuristic

Above the exhaustive threshold—or when exhaustive search reaches the planning-memory guard—the optimizer switches to a bounded deterministic heuristic.

Baseline:

1. construct a greedy connected initial tree using lowest estimated incremental objective cost,
2. use a stable structural tie-break,
3. apply a configurable bounded number of local-improvement passes,
4. local moves may include:
   ```text
   adjacent swap
   join rotation
   limited subtree exchange
   ```
5. stop early when a complete pass finds no improving legal tree.

`large_join_max_local_passes` is a mandatory nonnegative integer optimizer-search
configuration value. Its default is:

```text
large_join_max_local_passes = 4
```

Zero is valid: the deterministic greedy tree is still constructed, but no local
improvement pass runs. A missing, negative, or unrepresentable value is invalid
optimizer configuration and is rejected as `OptimizerError` before search-state
construction; it is never converted to an unsigned value, clamped, or allowed
to overflow a pass counter.

One complete pass examines every candidate in the implementation's enabled
legal local-move neighborhood once in a stable structural order from the
current tree, compares the resulting legal candidates under Chapter 38's active
objective and canonical tie rules, and adopts the preferred candidate only when
it is better under those rules. The next pass starts from the adopted tree. The
search stops after the first complete pass with no improvement or after exactly
`large_join_max_local_passes` completed passes, whichever occurs first. The
initial greedy construction is independent of and does not consume this pass
budget. The enabled move kinds remain implementation choices from the bounded
set of semantics-preserving moves; no particular optional move or beam is a
correctness requirement.

This is optimizer configuration, not persistent format. Chapter 33's retained
configuration identity fixes it for one invocation; a later invocation may use
a newly validated value.

Deterministic beam search is an optional extension and is not required by the
baseline.

Greedy extension and every local move apply §37.12's same Cartesian-admission
rule. They choose predicate-connected extensions when available, introduce a
Cartesian extension only for an admitted multi-relation-predicate prerequisite
or complete-component assembly, and never cross a constrained outer-join
boundary. Cost and the active objective select among admitted legal extensions;
they do not change Cartesian legality.

## 37.14 Join algorithm alternatives

For every legal binary join pair, enumerate every **implemented** applicable algorithm:

```text
PhysicalHashJoin
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalMergeJoin when capability-enabled and predicates/order permit
```

Join order and physical join algorithm are costed together.

The optimizer does not first freeze a complete join tree and only afterward choose algorithms if doing so would discard a cheaper order/algorithm combination.

## 37.15 Hash-join orientation

For an INNER equijoin, consider both semantically equivalent orientations when the runtime supports them:

```text
build left  / probe right
build right / probe left
```

Cost includes:

```text
build rows
build payload width
memory/spill
probe rows
downstream properties
```

For the baseline LEFT hash join, Chapter 28 fixes:

```text
logical right -> build
logical left  -> preserved probe
```

and the optimizer enumerates only that supported orientation unless a separate semantic transformation changes the logical join.

## 37.16 Logical join cardinality is algorithm-independent

For the same logical:

```text
A ⋈ B ON predicate P
```

the estimated output cardinality is identical regardless of whether the physical algorithm is:

```text
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
MergeJoin
```

Algorithm choice changes:

```text
cost
memory
spill
provided ordering
startup behavior
```

not logical row semantics.

Likewise, `estimated_join_rows == 0` is algorithm-independent cost metadata, not semantic-empty state. Only an approved exact proof from §§20.17.10 and 35.2 may replace or eliminate the logical join.

Cardinality is cached by logical relation/predicate identity where practical so alternative physical algorithms do not repeatedly invoke different estimators for the same subproblem.

## 37.17 Subquery physical planning

Every supported expression subquery lowers to the mandatory independent lazy
fallback in §20.14.7 unless an exact §20.14.11 rewrite replaces it. Each child
is optimized as an ordinary physical subplan, but its runtime state is keyed by
the bound occurrence and initialized at most once per statement attempt.

Costing includes applicable:

```text
startup
materialization
hash-set / comparison work
spill I/O
```

and uses scalar's at-most-two final-row consumption, EXISTS first-row demand,
IN's complete build, and derived-table ordinary relational cardinality. Lazy
once cost may be modeled as startup/conditional-on-demand cost, but a probability
estimate cannot alter whether runtime demand occurs.

Semi/anti/marker alternatives enter search only with the exact context/NULL/error
proofs in §20.14.11 and with runtime capability. A costing objective such as
`required_rows=1` never removes scalar cardinality checking or becomes an
existence/emptiness fact.

Correlated forms have no bound/logical/physical alternative and are rejected
before costing. The optimizer therefore never costs them as one-time work and
never assumes decorrelation succeeds.

Without prepared-statement parameters, optimization sees literal constants directly and may use MCV/histogram information for those literals.

## 37.18 Join/property invariants

1. Physical properties track ordering and required output slots without a premature full property lattice.
2. Ordering satisfaction is exact prefix matching on slot, direction, NULL order, and collation.
3. Only runtime-guaranteed ordering is advertised.
4. Interesting orders use §37.2's exact-duplicate normalization, retain useful non-cheapest alternatives, and do not create unbounded arbitrary order classes.
5. Join relation identity uses BindingId, not TableId.
6. Exhaustive search enumerates bushy trees for small reorderable regions.
7. The validated nonnegative exhaustive threshold counts BindingId occurrences per reorderable region, defaults to 10, and is stable for one optimizer invocation.
8. Large-join search always constructs its deterministic greedy tree; its validated nonnegative local-pass budget defaults to 4, with zero meaning no local passes.
9. Predicate-connected partitions are preferred and unnecessary Cartesian subsets are excluded, while hyperedge prerequisites and disconnected complete components retain the necessary Cartesian alternatives defined by §37.12.
10. INNER join order and physical algorithm/orientation are optimized together.
11. LEFT JOIN semantic boundaries and supported hash orientation are preserved.
12. Logical join cardinality is independent of the physical join algorithm.
13. Only capability-enabled physical algorithms enter the plan search.
14. Correlated subqueries are rejected before physical search; every accepted expression subquery has the once-per-attempt fallback/costing contract in §37.17.
15. A predicate occurrence activates only when all referenced BindingIds are available and has exactly one lowest legal owning placement in each candidate join tree, without weakening demand, error, selectivity, or cost ownership.
