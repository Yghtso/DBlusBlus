# 33. Optimizer Architecture

## 33.1 Role

The optimizer converts a typed logical relational plan into one immutable physical plan.

Cost-based relational optimization applies to ordinary query/DML relational subplans.

Resolved control/maintenance statements such as:

```text
LogicalAnalyze
LogicalVacuum
DDL control nodes
```

may be lowered through their dedicated physical control operators without entering join-order/access-path search when they have no relational alternative to optimize.

The planning flow is:

```text
typed logical plan
    ↓
logical normalization
    ↓
predicate + required-column analysis
    ↓
stable statistics snapshot
    ↓
cardinality / row-width estimation
    ↓
base access-path enumeration
    ↓
join graph / join-order search
    ↓
physical operator alternatives
    ↓
physical-property enforcement
    ↓
canonical costed selection under the applicable search limits
    ↓
final PhysicalPlan validation
    ↓
immutable PhysicalPlan
```

Selection considers only legal, capability-enabled alternatives admitted,
explored, and retained by the applicable Chapters 37–38 exhaustive or bounded
search path. Among those alternatives, it follows Chapter 38's active cost
objective, dominance, and deterministic tie rules. It does not require
enumerating every legal physical plan or finding the global minimum outside the
explored search space. Chapter 38's planning-resource fallback and
`OptimizerResourceLimit` outcome remain authoritative, and every selected plan
must pass §38.24 final validation before handoff.

The architecture uses:

```text
System-R-style bottom-up dynamic programming
+
small memo keyed by logical subproblem / useful physical properties
+
rule-based logical normalization
+
cost-based physical selection
```

The baseline does not require a full Cascades/Volcano framework. It exposes the
fundamental relational optimization mechanisms directly without mandating a
more general optimizer framework.

## 33.2 Layering

The optimizer stages are conceptually:

```text
Bound / Logical Plan
        ↓
Logical Normalization
        ↓
Predicate Analysis
        ↓
Required-Column Analysis
        ↓
Statistics Lookup
        ↓
Cardinality Estimation
        ↓
Base Access-Path Enumeration
        ↓
Join Graph Construction
        ↓
Join-Order Enumeration
        ↓
Physical Operator Selection
        ↓
Physical Property Enforcement
        ↓
Canonical Costed Selection Under Applicable Search Limits
        ↓
Final Physical-Plan Validation
        ↓
Immutable PhysicalPlan
        ↓
Pipeline Builder
```

The optimizer does not execute queries.

The executor does not make ordinary runtime cost-based plan choices in the defined architecture.
Adaptive/runtime reoptimization is outside the baseline optimizer/executor
contract.

## 33.3 Planning inputs

One optimizer invocation consumes immutable or stable views of:

```text
logical plan
catalog/schema/index descriptors
statistics descriptor snapshot
optimizer planning/search configuration and resource limits
query execution-memory budget
optimizer/execution cost configuration
required final physical properties
```

Optimizer planning/search limits bound optimization work and the dedicated
planning arena under §38.21. They are distinct from the Chapter-24 query
execution-memory budget, which constrains runtime execution. Estimated peak
runtime memory and spill behavior are planning predictions, not evidence that a
budget grant or physical allocation has succeeded; actual runtime allocation,
reservation, spill, and cleanup remain owned by Chapter 24. If canonical
bounded planning cannot fit within the configured planning resource limit,
§39.4's `OptimizerResourceLimit` outcome applies.

Required final properties include at least:

```text
required output LogicalSlotIds
ORDER BY ordering when present
```

The tracked property set is limited to `OrderingProperty` and
`RequiredSlotSet`. Rewindability, partitioning, and other extension categories
are not tracked baseline physical properties and are not required by this
contract.

Ordinary planning MUST NOT inspect mutable heap/B+ page contents or exact momentary BufferPool residency.

## 33.4 Stable catalog/statistics view

The optimizer holds immutable catalog/statistics descriptors for the duration of one planning invocation.

A concurrently committed ANALYZE may publish a newer statistics descriptor for later planners, but one plan is never built from a mixture of old and new versions of the same statistics object.

Likewise, schema/index descriptors obey the caller's catalog snapshot from Chapters 16 and 21.

Statistics are performance metadata and do not change semantic visibility or query correctness.

Semantic-emptiness metadata is derived independently under §§20.17.10 and 35.2. The optimizer MUST NOT infer it from `estimated rows == 0`, a zero TRUE fraction, or any statistics descriptor.

## 33.5 PhysicalPlan output

The optimizer returns one immutable:

```text
PhysicalPlan
```

containing or making inspectable at every relevant node:

```text
chosen physical operator
child relationships
lazy subquery side-plan relationships and semantic mode
chosen access path
chosen join order
chosen join algorithm
estimated rows
provably-empty state where applicable
estimated average row width
cost components / scalar total cost
estimated peak memory
estimated spill behavior
provided physical properties
```

The executor receives these decisions already made.

The executor may collect actual runtime metrics, but it does not reinterpret the logical query or silently choose a different ordinary algorithm in the defined architecture.

## 33.6 Cost-based choice rule

Physical choices are driven by estimated cost rather than hard-coded selectivity folklore.

In particular, there is no primary rule such as:

```text
if selectivity < 10%:
    use index
```

The scan/index break-even point emerges from:

```text
relation pages
candidate rows
heap-page reuse
index/heap correlation
cache assumptions
required row width
CPU work
```

The same principle applies to later join/aggregate/sort alternatives.

## 33.7 Optimizer invariants

1. The optimizer consumes resolved logical semantics and never reparses SQL.
2. One planning invocation uses stable immutable catalog/statistics descriptors.
3. Statistics influence performance decisions, never query correctness.
4. Ordinary optimization does not inspect exact mutable page contents or live buffer residency.
5. The executor receives one finalized immutable PhysicalPlan.
6. Physical choice follows the active objective and canonical tie rules among legal, capability-enabled alternatives admitted and retained by the applicable canonical search path and resource bounds; fixed selectivity thresholds are not the primary selector, and a global exhaustive optimum is not required.
7. Logical normalization and physical algorithm selection remain distinct stages.
8. Full Cascades/Volcano optimization and adaptive runtime reoptimization are outside the baseline contract rather than hidden baseline complexity.
9. Only approved exact semantic proof may eliminate execution; numerical estimates, including zero, affect cost and physical choice only.
