# 38. Memo, Costed Physical Search, and Memory-Aware Optimization

## 38.1 Search requirements and memo identity

A physical search request is conceptually:

```text
SearchRequirement {
    RequiredSlotSet
    OrderingProperty required_order
    RequiredRowsObjective required_rows
}
```

`RequiredRowsObjective` is:

```text
ALL_ROWS
or
FIRST_K_ROWS(K)
```

and is not itself a physical property.

When a finite required-rows objective is semantically propagated into a subproblem, it participates in memo/search identity because a low-startup plan may be preferable to the full-result cheapest plan.

For join enumeration, the common memo identity is conceptually:

```text
RelationSet
+
required output-slot class
+
required/interesting OrderingProperty class
+
propagated RequiredRowsObjective class
```

When required slots are uniquely derived for the RelationSet and no finite row objective is propagated, this reduces to the traditional:

```text
RelationSet + OrderingProperty
```

System-R-style key.

## 38.2 PlanAlternative

Every retained physical alternative stores at least:

```text
logical subproblem identity
physical plan prototype
estimated rows
estimated width
Cost
provided OrderingProperty
required output slots
estimated peak memory
estimated spill bytes
semantic-empty proof/provenance where applicable
capability/feasibility state
canonical structural tie key
```

Plan prototypes are optimizer/query-planning-arena objects.

They contain no execution-time mutable operator state.

## 38.3 Dominance

For the same logical/search requirement, plan `A` dominates plan `B` only if:

```text
A is semantically valid
A satisfies every requirement that B satisfies
A's provided ordering is at least as useful for the active property class
A is no worse under the active cost objective
A has no relevant feasibility disadvantage
```

For full-result optimization, the main objective is total cost.

For a propagated `FIRST_K_ROWS` objective, startup/partial-run cost participates in the comparison.

A cheaper unordered plan therefore does **not** dominate a slightly more expensive interesting-order plan that can avoid a downstream sort.

Likewise, a lower-total-cost high-startup plan does not automatically dominate a low-startup alternative when the memo key carries a finite required-rows objective.

Dominated alternatives are discarded promptly.

## 38.4 Cost ties and deterministic choice

All optimizer costs must remain:

```text
finite
nonnegative
```

`cost_tie_relative_epsilon` is a mandatory finite binary64 optimizer-search
configuration value in the half-open interval `[0, 1)`. Zero is valid and
means that only exactly equal objective costs share the minimum-cost tie
window. Its default is:

```text
cost_tie_relative_epsilon = 1e-9
```

The value is validated with the other Chapter-33 optimizer configuration before
physical alternative comparison and is retained unchanged for one optimizer
invocation. A missing, negative, NaN, infinite, `>= 1`, or unrepresentable
value is invalid optimizer configuration and is rejected under Chapter 39's
`OptimizerError` ownership. It is never clamped or replaced silently and is not
one of Chapter 36's cost-conversion coefficients.

Selection from one completed set of otherwise comparable alternatives uses one
set-level anchored ordering under the active objective:

1. Find the exact raw minimum finite objective cost `m` using the ordinary
   numeric order. This minimum is independent of candidate iteration order.
2. Compute:
   ```text
   scale = max(1, m)
   tolerance = checked_multiply(cost_tie_relative_epsilon, scale)
   tie_limit = saturating_add(m, tolerance)
   ```
   in the Chapter-36 finite cost-comparison domain without first evaluating an
   overflowing or nonfinite intermediate.
3. The minimum-cost tie window contains exactly the candidates whose objective
   cost is `<= tie_limit`. Order this window by the complete §38.5 structural
   key.
4. Order candidates outside the window by exact objective cost and then by the
   complete structural key.

This defines a total order for the completed candidate set: the first candidate
in the tie window is selected. Exact equality and two saturated costs are
resolved structurally; an ordinary cost always precedes a saturated cost unless
the ordinary cost lies in the anchored saturated tie window. A valid saturated
cost remains ordinary approximate planning metadata. Invalid raw costs are
rejected under §36.2.1 before this procedure.

A pairwise relative-nearness expression may be reported as diagnostic
near-equality information, but it is not a sorting comparator, associative
tournament, memo key, or incremental dominance order. In particular, a chain
in which `a` is near `b` and `b` is near `c` does not make the selected result
depend on insertion order: only the fixed global anchor `m` defines the tie
window.

Section 38.3 dominance first applies its semantic, requirement, property, and
feasibility conditions. Cost-based dominance and pruning must then preserve the
winner that this anchored ordering would choose from the completed comparison
set. An implementation that inserts alternatives incrementally therefore
retains provisional candidates, or an equivalent sufficient summary, until the
set's exact raw minimum and tie-window structural minimum are fixed; it must not
irreversibly discard a possible anchored-window winner through pairwise
approximate comparisons. This requirement does not retain alternatives already
excluded by an independent semantic, property, capability, or feasibility
proof.

Hash-map iteration order, allocator addresses, worker timing, and pointer values MUST NOT choose a plan.

## 38.5 Canonical structural key and plan fingerprint

Every physical plan produces a deterministic full structural key for
tie-breaking and diagnostics. The key is a recursively composed, tagged tuple.
It contains every applicable plan-distinguishing field, including:

```text
physical operator kind and capability-specific physical variant
logical-subproblem identity where the operator embodies that distinction
query-local BindingId / relation-occurrence identity
stable TableId, IndexId, descriptor, and access-path identities
join kind, physical algorithm, child orientation, and ordered child keys
bound predicate-occurrence identities and each exact referenced RelationSet
logical input/output LogicalSlotIds and plan-distinguishing output-slot mapping
access bounds, bound semantics, and index scan direction
normalized required/provided OrderingProperties and physical Sort keys
exact semantic Limit/Offset parameters and exact PhysicalTopN K
propagated row objective only when it changes a plan-local physical requirement
other capability-specific parameters that change executed physical behavior
materialization/spill mode only when it changes the physical algorithmic form
recursive child structural keys
```

Ordinary estimates, costs, memory targets, spill estimates, retained
`StatsVersion` values, and diagnostic counters are not structural fields unless
they select a genuinely different physical operator form or parameter. Query-
local `BindingId` and `LogicalSlotId` values remain query-local identities; this
key does not make them persisted or globally stable.

Canonical composition uses a fixed field order; an explicit type/variant tag
for every field; an explicit absent/present tag for optional fields; and
length-delimited or equivalently unambiguous framing for sequences and nested
keys. Ordered children retain their physical child order. Unordered logical
sets, including predicate sets, are sorted by their full canonical member keys
before composition. Exact integer parameters use their exact mathematical
value. These rules distinguish field sequences, optional absence, variant
types, child orientation, predicate multiplicity and occurrence, output-slot
mappings, and exact K without relying on concatenated display text.

The authoritative comparison is lexicographic comparison of the complete
tagged tuple. Full-key equality means equality of every applicable structural
field above. The representation need not use one binary encoding, endianness,
serialization library, or persisted format.

It excludes:

```text
memory addresses
unordered-container iteration order
runtime execution counters
```

The lexicographic full structural key is the final collision-free tie-break
inside one optimizer build/configuration. Distinct physical plans must differ
in at least one authoritative field; changing construction, allocator, or
unordered-container iteration order does not change the resulting key.

A debug:

```text
PlanFingerprint
```

may additionally hash a canonical serialization of this full key with FNV-1a-64 for compact display/regression tracking.

The hash alone never defines semantic or memo identity, merges alternatives,
decides correctness or tie order, or bypasses slots/properties/validation,
because collisions are possible. Fingerprint stability is required only for
the same canonical serialization version; the full structural key remains
authoritative.

## 38.6 Join-DP initialization

For every base relation occurrence:

1. compute the logical base-filter cardinality once,
2. enumerate `PhysicalSeqScan`,
3. enumerate every semantically usable implemented `PhysicalIndexScan`,
4. cost each access path,
5. attach its provided ordering,
6. retain non-dominated alternatives for relevant interesting-order/search requirements.

If the logical base subtree is not semantically proven empty, these runtime alternatives remain eligible even when its shared estimated cardinality is zero. Dominance and zero cost cannot reinterpret that estimate as an empty-result rewrite.

The same logical base cardinality is shared by all physical access alternatives.

A scan method never receives a different SQL row estimate merely to make its cost more attractive.

## 38.7 Join-DP transition

For subsets `S` in increasing cardinality:

```text
for each legal deterministic partition S = A ∪ B:
    identify crossing join predicates
    obtain logical join cardinality
    enumerate applicable implemented join algorithms/orientations
    obtain child alternatives satisfying algorithm requirements
    add property enforcement where required
    estimate width / memory / spill
    cost the physical alternative
    insert if non-dominated
```

Cardinality is cached by logical relation/predicate identity when possible.

Physical costs/properties remain alternative-specific.

The transition does not mutate the logical plan or statistics snapshot.

A zero logical join-cardinality estimate affects cost only. It does not skip construction of a semantically required join or create semantic proof in the memo.

## 38.8 Hash-join cost

Estimate:

```text
build child objective cost
probe child objective cost
+
build_rows * hash/build CPU
+
probe_rows * hash/probe CPU
+
expected matches * output/materialization CPU
+
residual predicate CPU
+
estimated memory/spill cost
```

Build payload width is the pruned width from §38.18 rather than the full base-row width.

Hash-table setup has nonzero startup cost.

For INNER joins, both supported build orientations are costed.

For LEFT joins, only the supported preserved/probe orientation is enumerated.

## 38.9 Hash-join memory and spill

Hash costing consumes the same retained runtime-capability configuration that
governs Chapter-28 hash construction and spill behavior. Its required inputs
are conceptually:

```text
hash_directory_target_load_factor
hash_spill_max_partition_fanout
hash_spill_max_recursive_repartition_passes
```

`hash_directory_target_load_factor` is a finite binary64 value in `(0, 1]`.
Its value is the exact retained ordinary runtime directory target; Chapter
28's approximately `0.70` value is the default tuning target, not an
independent Chapter-38 constant. `hash_spill_max_partition_fanout` is a
representable unsigned power of two `>= 2`. For each partitioning pass, the
common Chapter-28 runtime/planning selection rule chooses the smallest power of
two at least `max(2, ceil(P / T))`, capped at this maximum, where `P` is that
pass's largest build partition and `T` is the available target; when `T` is
zero, it chooses the configured maximum. Runtime applies the rule to actual
pressure/reservation values and planning applies it to their retained
estimates, so their selected fanouts may differ without using different policy.
The ceiling division, power-of-two selection, and cap use checked integer
operations and apply the cap before any unrepresentable growth.
The maximum recursive repartition-pass count is a representable unsigned
integer `>= 0`; zero is valid and permits initial partitioning but no additional
recursive repartitioning.

These are runtime/hash-capability inputs, not Chapter-36 conversion
coefficients. Every enabled spill-capable hash implementation supplies all
three. Missing, zero or negative where excluded, NaN, infinite, out-of-domain,
or unrepresentable values are invalid optimizer/runtime capability
configuration and are rejected under Chapter 39's `OptimizerError` ownership
before an affected hash alternative is costed. Values are retained unchanged
for the optimizer invocation. The selected physical implementation and the
cost model consume the same retained values; runtime pressure may still cause
more or less spill than estimated without changing SQL semantics.

A conservative baseline build-memory estimate uses:

```text
build_rows
*
(
    pruned build RowLayout width
    +
    hash-directory overhead
    +
    duplicate-chain metadata
)
/
target load factor
```

including average varlen widths.

`build_rows` and width/metadata inputs are finite nonnegative Chapter-35 or
runtime-descriptor quantities in rows and bytes. Multiplication, addition, and
division use Chapter-36 checked finite arithmetic and saturate a valid oversized
estimate at the applicable finite counter/cost bound. The validated positive
load factor prevents division by zero. A zero estimated build cardinality may
produce zero estimated row-dependent memory, but it is not semantic-empty
proof and does not remove the executable build.

When usable join-key NDV exists, implementations may refine directory-entry count separately from duplicate-row storage, but must not undercount the actual retained build payload.

Compare estimated required memory with the planner-assigned operator memory target.

If required memory exceeds the target:

```text
spill expected
```

and add:

```text
partition write(build + probe bytes)
partition read(build + probe bytes)
repartition hash CPU
per-partition rebuild/probe CPU
```

Section 38.9 owns this operator-local spill work; §38.20 and Chapter 36 convert
its temporary bytes and operations once. Child costs or generic materialization
must not charge the same pass again.

For deterministic recursive-spill prediction, initial partitioning is charged
once when spill is expected. Let `P` be the largest currently predicted build
partition. For each permitted additional pass, predict the next largest
partition as at least `ceil(P / selected_fanout)` and, when a
validated skew estimate exists, no smaller than that estimate's bounded largest-
partition result. Charge that pass's complete temporary writes, reads, hashing,
and rebuild/probe work. Stop when the predicted partition fits its assigned
target, when the retained recursive-pass bound is reached, or when the next
prediction would not be smaller than `P`.

Unknown skew uses the uniform `ceil(P / fanout)` estimate with low-confidence
provenance; it never proves fit. If the pass bound is reached or progress cannot
be predicted, record the unresolved/skew fallback and charge the existing
§28.11 controlled fallback work for the unresolved partition using §38.10's
materialized nested-processing dimensions and §38.20's retained-data work,
with checked finite saturation rather than zero work or unbounded recursion. A
zero assigned target therefore predicts no positive partition as fitting. This
estimate is cost metadata only: Chapter 24 and §§28.10–28.11 remain
authoritative for actual reservations, spill, repartition, controlled skew
fallback, and runtime resource errors.

## 38.10 Nested-loop cost

For a materialized inner side:

```text
outer child cost
+
inner child/materialization cost
+
outer_rows * inner_rows * predicate CPU
+
output/materialization CPU
```

This can beat hash join for very small inner inputs because hash construction has real startup/memory cost.

Materializing/copying the inner side is never free.

## 38.11 Index nested-loop cost and repeated-key locality

Estimate:

```text
outer child cost
+
outer_rows
*
(
    inner index lookup/range cost
    +
    expected heap fetch/MVCC cost
    +
    residual predicate CPU
)
```

When outer join-key NDV is substantially below `outer_rows`, repeated lookups may reuse:

```text
B+ upper paths
leaf pages
heap pages
```

The cost model may apply a calibrated, bounded locality reduction derived from:

```text
outer_rows
outer_key_NDV
index/heap correlation
```

but never assumes every lookup is fully cold or fully free.

INLJ is attractive when the outer input is small and the indexed inner lookup is selective.

## 38.12 Merge-join cost and properties

MergeJoin is enumerated only when the runtime capability is enabled and the predicate/operator supports it.

If both children already provide the required compatible join-key order:

```text
left child cost
+
right child cost
+
linear merge CPU
+
duplicate-group handling
```

If an input lacks the required ordering, add the applicable `PhysicalSort` enforcement cost.

Merge join may win even with similar raw join cost when its provided order is interesting downstream.

Its advertised output ordering is only the ordering guaranteed by the enabled runtime implementation.

## 38.13 Sort and Top-N cost

For full sort, approximate in-memory CPU as:

```text
N * log2(max(N, 2)) * comparison_cost
```

adjusted by key type/width.

Memory includes:

```text
N * pruned sort-record width
+
owned payload / varlen storage
```

If the assigned memory target is insufficient, add:

```text
run-generation writes
run reads
merge-pass temporary I/O
merge comparison CPU
```

For:

```sql
ORDER BY ... LIMIT N OFFSET O
```

the Top-N retention target is the checked:

```text
K = N + O
```

and approximate heap maintenance is:

```text
input_rows * log2(max(K, 2))
```

with memory roughly proportional to:

```text
K * retained row width
```

Top-N is selected by total objective cost when `K` is materially smaller than the full input; SQL syntax alone does not force it.

## 38.14 Aggregate and DISTINCT cost

### HashAggregate

Estimate:

```text
child cost
+
input_rows * hash/update CPU
+
estimated_groups * allocation/finalize CPU
+
spill cost when group state exceeds assigned memory
```

Memory derives from:

```text
estimated_groups
*
(
    grouping key RowLayout width
    +
    exact aggregate state size/alignment
    +
    hash overhead
)
```

Aggregate descriptors provide their real state size/alignment.

### SortAggregate

Enumerate only when `PhysicalSortAggregate` capability is enabled.

If child order already satisfies grouping keys:

```text
child cost
+
approximately linear ordered-aggregate CPU
+
retained-output materialization/spill cost
```

Mutable aggregate-state memory is low, but the finalized-output collection is
separately costed and assigned a spill-capable memory target under §§29.10,
38.19, and 38.20.

Otherwise compare:

```text
Sort enforcement
+
ordered aggregate plus retained-output storage
```

against hash aggregation.

### DISTINCT

When implementations exist, compare:

```text
hash PhysicalDistinct
```

with:

```text
ordered input / Sort
+
streaming ordered DISTINCT
```

and retain any useful resulting ordering.

## 38.15 Ordering enforcement and final ORDER BY

If an alternative does not naturally satisfy a required ordering:

```text
insert PhysicalSort
```

and include its startup, CPU, memory, and spill cost.

When the root requirement is:

```text
ORDER BY + finite LIMIT/OFFSET
```

the planner also considers `PhysicalTopN` only where it is semantically
equivalent, its runtime capability applies, and the selected implementation can
represent and honor the exact mathematical K from §30.7. Otherwise Top-N is
not a legal alternative, and search retains an exact ordering provider followed
by `PhysicalLimit`, inserting `PhysicalSort` when no existing provider
satisfies the required order.

For final ORDER BY compare:

```text
cheapest unordered plan + enforcement
```

against:

```text
possibly more expensive naturally ordered plan
```

such as an ordered IndexScan or capability-enabled MergeJoin/ordered aggregate.

The lowest active objective cost wins among legal alternatives. Cost, search
order, and optional capability availability cannot turn valid LIMIT/OFFSET SQL
into an error or otherwise change its semantics.

The optimizer never inserts Sort merely because an ORDER BY exists if an already-provided property satisfies it.

## 38.16 RequiredRowsObjective and startup cost

Every Cost retains:

```text
startup_cost
run_cost
total_cost
```

where:

```text
total_cost = startup_cost + run_cost
```

For full-result planning:

```text
objective = total_cost
```

The following quantities are distinct:

```text
LogicalLimit count:
    semantic under §§19.14 and 20.12

mathematical first-K:
    exact derived requirement for a physical algorithm that needs it

RequiredRowsObjective:
    cost/search metadata

PhysicalTopN K:
    algorithm-specific exact requirement under §30.7

estimated cardinality:
    estimate, never the actual or proven relation cardinality
```

For a semantically safe finite root LIMIT/OFFSET requirement, the optimizer
MAY carry `FIRST_K_ROWS(K)` when the exact mathematical K is representable in
its exact objective domain. It MAY instead use a wider or otherwise
domain-specific exact representation; no concrete numeric representation is
required.

When exact K is not representable in the active objective domain, the canonical
fallback is `ALL_ROWS`, no finite row goal, or an equivalent unbounded
objective, unless another exact representation is available. Inability to
represent the finite objective MUST NOT fail the query, remove all legal plans,
or become the retained-K bound of `PhysicalTopN`.

A saturated finite value MAY be used only as explicitly approximate costing
metadata under the ordinary estimate rules. It MUST NOT affect plan legality,
act as semantic proof, become an executor row cap or exact Top-N K, authorize
semantic early termination, or change query validity. The exact objective and
algorithmic Top-N bound therefore remain distinct even when both originate
from the same LIMIT/OFFSET pair.

A streaming/early-terminating plan may use a partial objective:

```text
startup_cost
+
fraction_of_run_cost_needed_for_required_rows
```

with the fraction clamped to `[0,1]`.

Blocking operators such as full Sort, HashAggregate, DISTINCT build, and the build side of HashJoin require their necessary blocking input before producing rows and therefore cannot pretend their blocking work scales linearly with the final LIMIT.

Propagation is conservative:

```text
Project:
    may preserve FIRST_K_ROWS

Filter/Scan:
    may use selectivity-based partial-consumption costing

Sort/Aggregate/Distinct:
    stop propagation across the blocking semantic boundary

Join:
    no general required_rows propagation in the defined architecture
    unless a specific physical algorithm proves a safe rule
```

Finite `required_rows` affects cost/search only.

It never changes result semantics or becomes an executor row cap in a location where SQL does not permit one.

It also never proves relation emptiness or cardinality, uniqueness,
scalar-subquery cardinality, safe expression pruning, or safe early
termination. Those facts require their existing exact semantic owners.

This remains true when the objective is `FIRST_K_ROWS(0)`: only an actual `LogicalLimit` with SQL LIMIT 0 supplies the §35.2 semantic proof.

## 38.17 Predicate CPU ordering

Bound/physical expressions carry an approximate evaluation-cost score.

Examples include:

```text
integer comparison        cheap
VARCHAR comparison        width-sensitive
IMMUTABLE scalar function descriptor/default cost
subquery                   separately costed
```

The optimizer may rank non-executable predicate metadata by a metric such as:

```text
evaluation_cost / rejection_probability
```

Such ranking is a cost/search aid, not permission to change executable child
order. Executable conjunctions retain §20.17.5's source-derived child order,
including when their components are IMMUTABLE and proven total/error-free.
Short-circuit work estimates follow that order and Chapter 17's actual demand
rules. Predicate placement and every other transformation still require the
independent Chapter-20 legality and demand/error checks; statistics or an
evaluation-cost score cannot establish those proofs.

## 38.18 Output-width and payload pruning

Physical costs use the materialized values actually required by the chosen plan.

Join output width is approximately:

```text
required left output width
+
required right output width
```

after projection pruning.

A HashJoin build row stores only:

```text
join-key data required for equality validation
build payload slots required by residual/downstream consumers
duplicate/hash metadata
```

rather than the complete base row.

Sort retains only:

```text
sort keys
downstream-required payload
```

or a stable compact row handle where the execution lifetime contract permits it.

HashAggregate uses exact aggregate state sizes/alignment.

Projection pruning therefore directly changes physical memory/spill cost.

## 38.19 Memory target assignment and pipeline-aware peak

The retained Chapter-24 per-query soft execution-memory budget, exposed to the
optimizer through §33.3, is named conceptually:

```text
query_execution_memory_budget_bytes
```

It is a representable unsigned byte count. Zero is valid and predicts zero-byte
targets for spill-capable blockers; it does not waive minimum runtime state or
guarantee execution success. A missing, negative, or unrepresentable value is
invalid optimizer configuration and is rejected as `OptimizerError` before
memory-aware costing. The value is retained for one optimizer invocation. It
is the existing execution-memory pressure input, not a new budget and not the
§38.21 planning-arena budget.

Planner-assigned operator memory targets are estimates derived from this one
query execution-memory budget, not independent grants and not one independent
full budget per blocking operator. Runtime `QueryMemoryManager` reservations
and the Chapter-24 per-query/global hard gates remain separate and authoritative.

Using the physical pipeline-dependency graph, identify blocking states that may coexist.

Chapter 26 pipeline dependencies define phase liveness: a blocker is active in
a phase while its retained blocking state can coexist with the other listed
states. Shared state is counted once per phase. Sequential states whose
lifetimes do not overlap are in different phases; no wall-clock schedule is
required. Distinct active blocker occurrences are ordered by a canonical
occurrence key: the root-to-node sequence of child ordinals followed by the
node's complete §38.5 structural key. The occurrence path breaks ties when two
repeated nodes have equal node structural keys and is derived from the plan
tree rather than an address or enumeration order.

Operator memory formulas produce a finite nonnegative raw byte estimate
`raw_need[i]`, which may be fractional because cardinalities and average widths
are fractional. Each raw estimate is converted to whole bytes exactly once, at
this allocation boundary, by rounding upward:

```text
need[i] = saturating_ceil_to_byte_count(raw_need[i])
```

The conversion uses the same checked finite arithmetic as §36.2.1: a valid raw
estimate above the byte-count representation's finite maximum saturates to that
maximum, and an invalid raw estimate (negative, NaN, or infinite) retains its
existing `OptimizerError`/internal-invariant ownership rather than being
converted. A positive sub-byte estimate therefore becomes one byte, an exact
integer estimate is unchanged, and zero remains zero. The integer `need[i]` is
the only need used by the allocator caps below and by the §38.20 fit/spill
comparison; no later step re-rounds or re-reads `raw_need[i]`. Rounding is a
conservative numeric conversion of approximate metadata, not semantic proof and
not a runtime reservation.

For each simultaneously-live phase, let `B` be the retained execution-memory
budget and let each blocker's integer byte need be `need[i]`. Every operand of
the loop below is therefore an integer. Assign integer-byte targets by
demand-capped equal sharing:

```text
target[i] = 0
remaining = B
U = blockers with positive residual need, in structural-key order

while remaining > 0 and U is nonempty:
    q = floor(remaining / |U|)
    r = remaining mod |U|
    proposal[i] = q + 1 for the first r members of U, else q
    grant[i] = min(residual_need[i], proposal[i])
    add every grant to its target and subtract their sum from remaining
    remove blockers whose need is satisfied
```

The loop redistributes every unused capped share until no residual demand or no
budget remains. Integer remainders always go to the earliest still-unsatisfied
structural keys. Zero-need blockers receive zero. Saturated needs remain finite
ordered byte quantities and consume only their allocated share. If total need
is below `B`, all needs are satisfied and the unused remainder stays
unassigned. The loop terminates because each nonempty round either exhausts the
remaining bytes or satisfies at least one blocker.

The sum of targets in every modeled simultaneous phase is therefore at most
`B`, and no blocker receives more than its estimated need. A blocker live in
multiple phases receives a phase-specific modeled target; operator costing that
requires one conservative target uses the minimum of those phase targets. Peak
planned query memory is the maximum sum of assigned targets in any one phase,
not the sum across sequential phases or every blocker in the plan. Spill
expectation uses the applicable target under §38.20.

A conservative overestimate is permitted and is exposed in diagnostics.

Runtime `QueryMemoryManager` remains authoritative for actual reservations;
planner targets neither reserve memory nor prevent runtime spill or controlled
runtime memory failure.

## 38.20 Spill and materialization cost

Spill expectation is deterministic:

```text
estimated_required_memory
>
assigned_memory_target
    -> spill expected
```

`estimated_required_memory` is the blocker's integer `need[i]` from §38.19,
after its single upward whole-byte conversion, and `assigned_memory_target` is
its applicable integer target; both sides of the comparison are whole-byte
quantities.

Estimate bytes/passes according to the relevant execution algorithm.

Materialization costs include:

```text
RowCollection append/copy
VARCHAR deep copy
temporary serialization
spill block formatting/checksum work
```

Comparison among NestedLoop materialization, HashJoin build, Sort, aggregate, and DML spool therefore never treats retained data as free.

The cost model may be inaccurate; it may not bypass a runtime memory hard limit.

## 38.21 Planning time and memory budget

Optimization itself is resource-bounded.

Track:

```text
optimization wall time
logical subproblems explored
partitions considered
physical alternatives costed
memo entries retained/pruned
peak planning-arena bytes
```

The optimizer uses a dedicated planning arena separate from execution memory.
Its existing configurable upper bound is named conceptually:

```text
optimizer_planning_arena_budget_bytes
```

It is a mandatory representable unsigned byte count supplied by the retained
Chapter-33 planning/search configuration. Zero is valid; a nontrivial plan may
then be unable to initialize and reach `OptimizerResourceLimit`, but the
configuration itself is not malformed. Deployment supplies the configured
default because a machine-independent byte default is not an architecture
constant. A missing, negative, or unrepresentable value is rejected as
`OptimizerError` before search-state construction. The exact value is retained
for one optimizer invocation and is not the §38.19 execution-memory budget.

The canonical planning-resource guard is the planning-arena byte guard. Before
any reservation, allocation, or growth that would raise charged live arena
bytes above `optimizer_planning_arena_budget_bytes`, checked integer arithmetic
triggers the guard without attempting that growth. Equality with the budget is
permitted. An unrepresentable requested extent or internally overflowing byte
calculation is an `OptimizerError`/internal invariant failure, not a valid
resource exhaustion. Wall time and the subproblem, partition, alternative, and
memo counters above are diagnostics only; they do not independently trigger
fallback. High estimated execution cost, execution-memory pressure, and the
relation-count threshold are not planning-arena guard conditions.

A catchable backing-allocation denial for a supported, representable arena
growth that the configured byte guard permits remains Chapter 39's
`OutOfMemory` operational cause. The optimizer may attempt the same clean
bounded fallback when that is an already permitted safe mitigation. If the
fallback successfully constructs and validates a legal plan, planning succeeds.
If a below-budget backing denial remains the cause preventing completion, the
terminal outcome is `OutOfMemory`, not `OptimizerResourceLimit`; attempting
fallback does not reclassify that cause. A distinct later configured-byte-guard
event retains the configured-bound outcome below. This distinction does not
authorize a different plan comparator or an uncontrolled allocation failure.

For each maximal legal reorderable region, §37.11 selects exhaustive DP when
`N <= exhaustive_join_limit` and the bounded §37.13 heuristic from the outset
when `N > exhaustive_join_limit`. Once exhaustive DP is selected, proximity to
or equality with the relation-count threshold does not interrupt it. The
remaining join-region search switches to §37.13's bounded heuristic only when
the canonical planning-resource guard triggers.

Each region search establishes an arena checkpoint before creating region
search state. If an exhaustive search's guard triggers, no partially built subset,
partition, alternative, or incomplete memo entry is a completed result. The
optimizer discards the region's exhaustive-search state, rolls the dedicated
arena back to that checkpoint (or uses an equivalent separately releasable
child arena), and restarts §37.13 bounded search from the retained immutable
logical, catalog/statistics, configuration, objective, property, and capability
inputs. Completed exhaustive alternatives are not reused by the baseline
fallback, avoiding dependence on the point at which the guard fired. This is
one optimizer invocation and starts no execution or side effect.

Bounded search is charged to the same byte budget from the clean region
checkpoint. A guard hit may therefore occur before useful exhaustive state,
during any subset/partition/alternative construction, while initializing the
heuristic, or after it has begun. If bounded search completes, its selected plan
still obeys all ordinary predicate, Cartesian, property, capability, objective,
tie, and validation rules.

If even bounded planning cannot fit within the configured planning resource limit, planning fails with a controlled `OptimizerResourceLimit` error rather than arbitrary process OOM.

Optimizer resource fallback never changes SQL semantics.

## 38.22 Missing/stale statistics in search

Search consumes one immutable statistics snapshot for the entire optimization.

Missing estimates use §35.25's centralized fallback configuration.

Confidence/provenance from §35.26 is retained on estimates and exposed to diagnostics.

Statistics staleness may reduce diagnostic confidence and influence explicitly configured cost heuristics, but the architecture does not automatically reject a plan or rewrite statistics because one query later observes a different actual row count.

Old cache entries, concurrent committed DML after ANALYZE, transaction-local statistics, and newly committed statistics versions all obey the same rule: statistics can change estimates/search order but cannot produce semantic emptiness or remove required execution.

One query's runtime cardinalities never directly rewrite persistent statistics in the defined architecture.

## 38.23 Optimizer trace and diagnostics

The optimizer can emit a debug trace containing at least:

```text
normalized predicates
estimate confidence/provenance
semantic-empty flag and exact proof provenance, shown separately from estimates
base cardinalities
enumerated access paths
cost components
join subsets/partitions explored
algorithms/orientations considered
alternatives pruned by dominance
interesting orders retained
memory targets / expected spill
property enforcement
final selected plan
canonical plan fingerprint
planning time/memory counters
```

A SQL/debug surface may expose this as:

```text
EXPLAIN (OPTIMIZER TRACE)
```

or an equivalent internal hook.

The exact textual presentation may evolve.

The trace is diagnostic output, not a stable persistent format.

## 38.24 Final physical-plan validation

Before execution, optimizer validation proves at least:

```text
logical output semantics preserved
all required LogicalSlotIds present
required order satisfied or explicitly enforced
join types/outer constraints preserved
predicates assigned only where semantically legal
DML hidden target slots preserved
every selected physical algorithm is capability-enabled
every selected PhysicalTopN can represent and honor the node's exact
mathematical K within the selected implementation's advertised exact domain
memory/spill annotations are finite/nonnegative
every empty-result/no-op replacement and semantic-empty annotation has an
approved exact proof under §§20.17.10 and 35.2 rather than numerical or
statistics provenance
every non-proven base/join subtree retains a semantically valid executable
path even when estimated rows are zero
```

The result then passes to the execution-layer `PhysicalPlanValidator`.

A selected `PhysicalTopN` that fails its exact-K capability condition is an
invalid physical plan. Validation MUST reject it before execution, and no
execution, DML, storage, catalog, WAL, or other side effect may begin. This is
an internal optimizer/plan-validation failure, not a user-visible LIMIT/OFFSET
overflow or a new SQL count-domain restriction. Correct physical search avoids
constructing or selecting that node and retains a conforming alternative under
§38.15.

A cost mistake may choose a slow valid plan.

A rewrite/legality mistake that changes results is a correctness failure.

## 38.25 Memo/search invariants

1. Memo alternatives are keyed by logical subproblem plus the requirements that can change the preferred physical plan.
2. A finite required-rows objective participates in search identity wherever it is propagated.
3. Dominance never discards a useful ordering or low-startup alternative required by the active search objective.
4. Costs and the validated retained tie tolerance remain finite/nonnegative; one fixed minimum-cost anchor and the full structural key define a total deterministic candidate-set order without using pairwise approximate ties as a comparator.
5. Hash-map iteration, candidate insertion, pointer, allocator, and worker-scheduling order never determine the chosen plan or resource assignment.
6. The complete tagged structural key distinguishes every plan-relevant physical identity and is collision-free for comparison; the possibly colliding compact PlanFingerprint is diagnostic only.
7. Base alternatives share one logical cardinality estimate regardless of scan algorithm.
8. Join output cardinality is independent of join algorithm.
9. Physical algorithm availability is explicitly gated by runtime capability.
10. Hash/spill costs use pruned payload widths, validated retained runtime-owned hash/spill inputs, deterministic execution-memory targets, and finite bounded recursive-spill prediction.
11. Sort/Top-N/aggregate/DISTINCT alternatives include required property-enforcement cost.
12. Full-result and first-K optimization objectives are distinct when semantically applicable.
13. The validated retained planning-arena byte guard deterministically bounds planning; a guard hit discards partial exhaustive region state and restarts bounded search, whose inability to fit produces `OptimizerResourceLimit`.
14. One optimization uses one stable statistics snapshot.
15. Missing/stale statistics remain explicit low-confidence assumptions, not hidden precision.
16. Optimizer diagnostics expose why estimates/alternatives were selected or pruned.
17. The optimizer may choose a slow plan but may never change SQL meaning.
18. Final plans pass optimizer semantic/property validation before execution.
19. Memo insertion, dominance, and pruning never promote estimated zero to semantic emptiness or discard required execution on that basis.
