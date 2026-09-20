# Initial review verdict

**NEEDS ARCHITECTURE FIX**

Chapter 38 is broadly coherent and preserves the closed semantic contracts, but five major specification gaps prevent two independent implementations from implementing identical mandatory comparison, memory-target, spill-configuration, and planning-resource behavior without inventing policy.

No frozen semantic contradiction or new SQL-semantics decision was found.

## Repository state

- Initial HEAD: `8c5f32ac0470f8ee1fe578443e47488580df1a08`
- Commit: `8c5f32a synced VERIFICATION after chapter 37 ARCHITECTURE fix`
- Initial worktree: clean
- Initial index: clean
- Final HEAD: unchanged
- Final worktree: clean
- Final index: clean
- Audit-created changes: **NONE**
- Build/test/sanitizer/benchmark: not run

## Live Chapter 38 inventory

Title: `38. Memo, Costed Physical Search, and Memory-Aware Optimization`

Boundaries:

- Start: [ARCHITECTURE.md:27826](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27826)
- Final substantive line: 28688
- Chapter 39 begins: [ARCHITECTURE.md:28694](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28694)

All expected subsections exist:

1. §38.1 Search requirements and memo identity
2. §38.2 PlanAlternative
3. §38.3 Dominance
4. §38.4 Cost ties and deterministic choice
5. §38.5 Canonical structural key and plan fingerprint
6. §38.6 Join-DP initialization
7. §38.7 Join-DP transition
8. §38.8 Hash-join cost
9. §38.9 Hash-join memory and spill
10. §38.10 Nested-loop cost
11. §38.11 Index nested-loop cost and repeated-key locality
12. §38.12 Merge-join cost and properties
13. §38.13 Sort and Top-N cost
14. §38.14 Aggregate and DISTINCT cost
15. §38.15 Ordering enforcement and final ORDER BY
16. §38.16 RequiredRowsObjective and startup cost
17. §38.17 Predicate CPU ordering
18. §38.18 Output-width and payload pruning
19. §38.19 Memory target assignment and pipeline-aware peak
20. §38.20 Spill and materialization cost
21. §38.21 Planning time and memory budget
22. §38.22 Missing/stale statistics in search
23. §38.23 Optimizer trace and diagnostics
24. §38.24 Final physical-plan validation
25. §38.25 Memo/search invariants

There are exactly 19 contiguous invariants.

## Findings

### N38-1 — MAJOR: Incomplete and non-total cost-tie contract

Location: [§38.4, lines 27917–27944](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27917)

Canonical owners: §§36.2.1, 36.3, 38.3–38.4, 39.4.

The chapter defines:

```text
cost_tie_relative_epsilon = 1e-9
abs(a-b) <= epsilon * max(1, abs(a), abs(b))
```

but does not define:

- The required representation/domain of epsilon.
- Whether zero is legal.
- Missing, negative, NaN, infinite, or unrepresentable handling.
- Validation timing and invocation stability.
- Checked evaluation of the tolerance expression.
- A total, transitive ordering when approximate ties overlap.

The pairwise relation is not transitive. For suitable finite `a < b < c`, `a` can tie `b`, and `b` can tie `c`, while `a` does not tie `c`. A tournament or insertion-based minimum can therefore select different plans after container-order changes, despite §38.4’s determinism requirement.

Observable consequence: same retained inputs and configuration may choose different plans solely from alternative insertion order.

Smallest repair surface: §§38.4 and 38.25 invariants 4–5. Define the complete configuration contract and one deterministic total comparison procedure that retains the intended tolerance without a nontransitive sorting relation.

### N38-2 — MAJOR: Structural tie key cannot substantiate collision freedom

Location: [§38.5, lines 27946–27981](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27946)

Canonical owners: Chapters 19, 22, 33, 37 and §§38.1, 38.4–38.5.

The required structural serialization lists TableId/IndexId, tree/orientation, access paths, ordering, “important operator parameters,” and child keys. It does not explicitly require:

- BindingId/relation-occurrence identity.
- LogicalSlotId identities and output mapping where plan-distinguishing.
- Join type.
- Bound predicate occurrence identity and referenced set.
- Exact Top-N K and propagated objective where plan-local.
- Capability-specific parameters affecting physical behavior.
- Unambiguous tagging/length framing of variable structural fields.

Reproducer: a self-join where both leaves share TableId and IndexId, but represent different BindingIds and slot mappings. Distinct physical alternatives can serialize identically if the implementation follows only the listed fields.

The FNV fingerprint is correctly diagnostic-only and is not defective. The defect is the supposedly collision-free full key.

Observable consequence: a true cost tie may remain unresolved or become dependent on insertion order.

Smallest repair surface: §§38.5 and 38.25 invariant 6. Define the minimum semantic/physical identity fields and an unambiguous canonical composition rule without prescribing a concrete byte encoding.

### N38-3 — MAJOR: Pipeline memory-target allocation is underdefined

Location: [§38.19, lines 28511–28529](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28511)

Canonical owners: Chapters 24 and 26, §§33.3, 38.18–38.20.

The chapter correctly requires:

- Simultaneously-live phases.
- Per-phase target sum not exceeding the query budget.
- No blocker receiving more than estimated need.
- Stable redistribution.
- Peak as maximum simultaneous phase.
- Runtime QueryMemoryManager authority.

It does not define the initial distribution rule when multiple blockers remain unsatisfied. “Distributing” the budget does not specify equal shares, weighted shares, structural-order filling, or another canonical function. The later redistribution rule cannot resolve the initial ambiguity.

Terminology also alternates between:

- `query planning memory budget`
- `planning query-memory budget`
- Chapter 33’s `query execution-memory budget`
- §38.21’s separate planning-arena budget.

Reproducer: two simultaneous blockers need 80 and 80 units under a 100-unit query execution budget. Assignments 50/50, 80/20, and 20/80 all meet the stated upper bounds but can predict different spills and select different plans.

Observable consequence: identical inputs and budgets can produce materially different required spill estimates without the difference being identified as implementation freedom or configuration.

Smallest repair surface: §38.19 and invariant 10. Name the consumed Chapter-24 execution-memory planning input and define one bounded deterministic allocation rule, or explicitly define the permitted policy as a retained configuration identity.

### N38-4 — MAJOR: Canonical planning-resource guard and configuration are incomplete

Location: [§38.21, lines 28557–28583](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28557)

Canonical owners: §§33.1–33.3, 37.7, 37.11, 37.13, 38.21, 39.4.

The repaired threshold handoff is correct. However, the “canonical planning-resource guard” remains undefined:

- No canonical configuration field or retained configuration identity is named.
- No representation/domain or zero policy is supplied.
- Missing, negative, or unrepresentable configuration outcomes are unspecified.
- Validation timing is unspecified.
- It is unclear whether the guard is arena-only, deterministic work counters, wall-clock time, or a combination.
- Wall time is tracked but not explicitly excluded from deterministic plan-selection triggers.
- Handling of partial exhaustive memo state before bounded fallback is not specified.
- The minimum resources required to begin the bounded fallback are not distinguished from invalid configuration.

Reproducer: two implementations receive the same region, arena byte budget, and search configuration. One switches when an allocation would exceed the arena; another switches after a partition-count threshold; a third uses elapsed wall time. All can claim the current “guard” triggered.

Observable consequence: fallback and `OptimizerResourceLimit` can differ for identical retained inputs, and wall-clock behavior can conflict with deterministic selection.

Smallest repair surface: §38.21 and invariant 13. Define the retained planning-resource configuration, deterministic trigger(s), validation boundary, relationship to the arena/work counters, and resource-error boundary. Preserve the corrected §37.11 threshold rule.

### N38-5 — MAJOR: Hash-memory and recursive-spill tuning inputs lack an owner contract

Location: [§38.9, lines 28051–28090](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28051)

Canonical owners: §§28.6, 28.10–28.11, 36.2.1, 36.3, 38.9.

Chapter 36 explicitly leaves hash load factor and spill partition/fanout settings to their owners. Chapter 38 divides by `target load factor` and consumes an `execution partitioning/fanout configuration`, but does not specify:

- Canonical configuration names or retained identities.
- Required finite domains.
- Whether load factor must be in `(0,1]`.
- Missing/zero/negative/NaN/infinite handling.
- Validation timing.
- Invocation stability.
- Relationship between runtime’s approximately 0.70 target and the costing value.
- Bounded recursion-depth/fanout requirements consumed by the estimator.

Reproducer: load factor zero causes invalid division; load factor greater than one can understate directory pressure; absent fanout/depth leaves the number of modeled spill passes implementation-invented.

Observable consequence: invalid configuration may reach cost arithmetic, or identical runtime configuration may receive incompatible memory/spill predictions.

Smallest repair surface: §38.9 and invariant 10, with a narrow cross-reference to the existing Chapter-28 runtime configuration. No new cost coefficient is needed.

## Canonical owner assessment

| Concern | Canonical owner | Assessment |
|---|---|---|
| SQL bags, NULLs, demand, errors | Chapters 17 and 20 | Preserved |
| Bound occurrence/slot identity | Chapter 19 | Preserved except structural-key omission |
| Physical eligibility/capabilities | Chapter 22 | Preserved |
| Runtime memory/grants/spill | Chapter 24 | Preserved |
| Pipeline dependencies | Chapter 26 | Preserved; does not define target allocation |
| Join execution/orientation/spill | Chapter 28 | Preserved |
| Aggregate/DISTINCT runtime | Chapter 29 | Preserved |
| Sort/Top-N exact K | Chapter 30 | Preserved |
| DML publication | Chapter 31 | Preserved |
| Parallel occurrence ownership | Chapter 32 | Preserved |
| Stable optimizer invocation | Chapter 33 | Preserved |
| Statistics generations | Chapter 34 | Preserved |
| Logical estimates/widths/proof | Chapter 35 | Preserved |
| Finite costs/work ownership | Chapter 36 | Preserved |
| Search space/properties/predicates | Chapter 37 | Preserved |
| Memo/comparison/resources | Chapter 38 | Five gaps above |
| Error/resource categories | Chapter 39 | Preserved |

## Memo and PlanAlternative

Memo identity is otherwise coherent:

- Logical subproblem identity is required.
- RelationSet is BindingId-based.
- Required slots and normalized ordering participate.
- `RequiredRowsObjective` is separate from physical properties.
- The definition `FIRST_K_ROWS(K)` together with invariant 1 means exact K is part of the requirement identity; `FIRST_K_ROWS(1)` and `(100)` cannot share a state.
- Equivalent raw ordering vectors normalize before memo identity under §37.2.
- Different predicates and outer-join boundaries remain distinct logical subproblems.

`PlanAlternative` is otherwise complete enough for implementation-independent planning:

- Logical identity, prototype, estimates, Cost, ordering, slots, memory/spill, proof provenance, capability/feasibility, and tie key are retained.
- Width detail is supplied by §35.3’s operator-appropriate representation.
- Stable descriptor/statistics lifetime belongs to the invocation.
- No execution-time mutable state is allowed in a prototype.

## Dominance

The dominance contract is safe when read conservatively:

- It applies only within the same logical/search requirement.
- Semantic validity and requirement satisfaction are prerequisites.
- Useful ordering and low-startup alternatives survive.
- Capability and exact representability are eligibility prerequisites.
- If the implementation cannot prove “no relevant feasibility disadvantage,” it must retain the candidate rather than claim dominance.

The phrase is broad but does not independently require a finding because it acts as a proof precondition, not permission to discard uncertain alternatives. N38-1 still affects the “no worse” numeric comparison.

## Cost arithmetic and objectives

Chapter 36 resolves the apparent unchecked shorthand in §38.16:

```text
total_cost = saturating_add(startup_cost, run_cost)
```

All components remain finite and nonnegative, and valid overflow saturates at `MAX_FINITE_COST`. Negative, NaN, or infinite raw values are OptimizerError/internal invariant failures. Saturation is neither proof nor resource exhaustion.

Full-result and first-K concepts remain correctly separated:

- LogicalLimit count.
- Exact mathematical K.
- RequiredRowsObjective.
- PhysicalTopN K.
- Estimated cardinality.

Unrepresentable objective K falls back to ALL_ROWS or another exact representation. Unrepresentable Top-N K makes that implementation ineligible, not the SQL invalid. `FIRST_K_ROWS(0)` is not semantic emptiness.

Partial-consumption costing is conservative:

- Fraction is clamped to `[0,1]`.
- Project may propagate.
- Filter/Scan may estimate partial consumption.
- Sort, Aggregate, and DISTINCT stop propagation.
- Hash build remains blocking.
- General join propagation is forbidden without a specific safe rule.
- Costing never creates runtime early-stop authority.

## Operator-specific costing

### Join DP

Initialization correctly requires SeqScan plus every usable single-index path, shared logical cardinality, finite fallbacks, exact ordering, and estimated-zero executable paths.

Transitions consume Chapter 37’s repaired partition and predicate rules. Physical algorithms share one Chapter-35 logical cardinality and receive alternative-specific costs/properties. No predicate can activate before all referenced bindings are present.

### Hash join

The cost model includes both child costs, hashing, probing, expected matches, residual work, output work, memory/spill, and nonzero startup. Build-side blocking and LEFT orientation are preserved.

The baseline memory formula is conservatively row-based and cannot use optional NDV refinement to undercount retained payload. Its configuration defect is N38-5.

### Nested loop and INLJ

Materialized-inner cost is charged once, including append/copy. Predicate work uses the outer×inner domain. An expensive materialized inner is neither free nor repeatedly re-executed.

INLJ includes outer cost, per-row index/range work, heap fetch/MVCC, and residuals. Locality reduction is optional, calibrated, bounded, and may not make all lookups free. Zero/unknown NDV simply cannot establish the optional refinement. Statistics do not prove key absence.

### MergeJoin

MergeJoin remains capability-conditional and equality-only. Input ordering must be exactly compatible or legally enforced by Sort. Child costs, merge work, duplicate groups, and runtime-guaranteed output order are preserved.

### Sort and Top-N

Sort covers comparison work, key/record width, owned varlen payload, run writes, reads, merge passes, and comparison CPU. `max(N,2)` keeps zero/one estimates numerically valid without creating semantic emptiness.

Top-N requires exact K and runtime representability. Full Sort/order-provider plus Limit remains available. Top-N is blocking and does not suppress demanded child work merely because K is small.

### Aggregate and DISTINCT

HashAggregate includes child, hash/update, group allocation/finalization, exact state size/alignment, memory targets, and spill. Chapter 35 prevents a global aggregate’s output from disappearing on estimated-empty input.

SortAggregate and ordered DISTINCT are capability-conditional. Required Sort and retained-output costs are included. Baseline hash forms remain available.

## Ordering, predicate ordering, pruning, and memory

Ordering enforcement correctly compares naturally ordered plans against unordered plans plus Sort. Exact normalized-prefix satisfaction remains Chapter 37-owned.

Predicate CPU ordering is optional and restricted to immutable, safely reorderable, non-error-sensitive conjuncts. The ratio shown is illustrative; an implementation using it must guard a zero/unknown rejection probability or choose another deterministic ranking. No exact formula is mandated.

Payload pruning preserves:

- Required join keys and full equality data.
- Residual/downstream slots.
- Sort keys and owned varlen payload.
- DML RID/system state.
- Aggregate state size/alignment.
- Valid lifetime for compact row handles.

Spill expectation uses strict `required > target`; equality therefore does not itself predict spill. Runtime QueryMemoryManager remains authoritative. Temporary reads/writes, deep copies, serialization, and formatting/checksum work are included.

## Planning resources and Chapter-37 handoff

The corrected threshold rule remains coherent:

- `N <= exhaustive_join_limit`: exhaustive DP initially.
- `N > exhaustive_join_limit`: bounded heuristic from outset.
- Threshold proximity/equality alone: no fallback.
- Actual resource guard: bounded fallback.
- Bounded fallback unable to fit: `OptimizerResourceLimit`.

N38-4 concerns definition of the guard itself, not this threshold handoff. Chapter 37 is not reopened.

## Statistics, diagnostics, and validation

Search uses a stable retained collection of compatible per-object statistics descriptors. Different objects may have different StatsVersions; self-join aliases share the underlying retained descriptor. Concurrent ANALYZE cannot mix generations. Statistics remain advisory and runtime actuals do not rewrite persistent statistics.

Diagnostics distinguish estimates/confidence from exact proof and include alternatives, partitions, algorithms, pruning, costs, memory, spill, enforcement, selected plan, fingerprint, and resource counters. Fingerprints remain nonpersistent and nonauthoritative.

Final validation covers:

- Logical semantics.
- Required slots/order.
- Join and predicate legality.
- DML hidden slots.
- Capabilities.
- Exact Top-N K.
- Finite memory/spill annotations.
- Exact empty/no-op proof.
- Executable paths for estimated-zero subtrees.

Invalid candidates are rejected before execution or side effects, then passed through the execution-layer validator. An invalid Top-N annotation is an internal optimizer/validation failure, not a SQL count error.

## Frozen-chapter regressions

- Chapter 31: no DML RID, assignment, RETURNING, candidate closure, W/C/R, retry, or publication regression.
- Chapter 32: BindingId remains distinct from worker/source occurrence; objectives cannot authorize unsafe early stop.
- Chapter 33: one invocation, objective, bounded search, capability filtering, fallback, and final validation remain.
- Chapter 34: per-object complete generations and valid-old retention remain.
- Chapter 35: one logical cardinality, representation-aware widths, 3VL, saturation, and proof separation remain.
- Chapter 36: seven cost coefficients, checked saturation, child/work ownership, and cost-only authority remain.
- Chapter 37: normalized order, required slots, exact RelationSet, Cartesian rules, predicate ownership, threshold, pass budget, orientation, and subqueries remain.

No frozen semantic question was discovered.

## Error/resource outcome matrix

| Condition | Canonical outcome |
|---|---|
| Malformed CostConfig | OptimizerError before costing |
| Malformed search configuration | OptimizerError before search state |
| Invalid tie tolerance | **Undefined—N38-1** |
| Nonfinite raw cost | OptimizerError/internal invariant |
| Valid saturated cost | Ordinary approximate cost metadata |
| Unsupported algorithm | Ineligible; not invalid SQL |
| Incompatible index | Ineligible |
| Invalid prototype/selected plan | Internal optimizer/validation failure |
| Exhaustive guard triggers | Bounded fallback |
| Bounded fallback exhausted | OptimizerResourceLimit |
| Estimated memory exceeds target | Spill expected; not an error |
| Runtime reservation denied | Runtime memory/resource owner |
| Spill I/O failure | SpillIOError |
| Invalid Top-N exact K | Ineligible during search; selected form rejected before execution |
| Scalar subquery second row | CardinalityError |
| Demanded SQL expression error | Existing SQL/runtime expression owner |
| High plan cost | Neither resource failure nor invalid SQL |

No new `CostModelError`, `MemoError`, `PlanFingerprintError`, or `JoinEnumeratorError` is needed.

## Complexity assessment

| Component | Classification |
|---|---|
| Memo identity and dominance | CORE |
| Full structural tie key | CORE |
| Optional compact fingerprint | JUSTIFIED ADVANCED |
| Bushy DP | CORE |
| Bounded heuristic | CORE |
| Operator-specific costs | CORE |
| Pipeline-aware memory assignment | JUSTIFIED ADVANCED, but underdefined |
| Recursive-spill prediction | JUSTIFIED ADVANCED, but configuration incomplete |
| Optional ordered algorithms | JUSTIFIED ADVANCED / capability-conditional |
| Optimizer trace | JUSTIFIED ADVANCED |

No mandatory Cascades framework, beam search, optional algorithm, global optimum, machine-specific formula, or unsupported SQL feature is introduced.

## Adversarial matrix

`GAP` entries correspond to the five findings; capability-dependent cases remain conditional.

| Case | Owner and required outcome | Assessment |
|---|---|---|
| A | §§38.1–2: predicates remain distinct logical states | COMPLETE |
| B | §§37.4,38.1: slot classes distinct | COMPLETE |
| C | §37.2: equivalent normalized order shares identity | COMPLETE |
| D | §§37.2–3,38.1: different order remains distinct | COMPLETE |
| E | §§38.1,38.16: ALL_ROWS/FIRST_K distinct | COMPLETE |
| F | §§38.1,38.16: exact K values distinguish requirements | COMPLETE |
| G | §§19.2,37.7: self-join BindingIds distinct | COMPLETE |
| H | §38.2: capability/feasibility retained | COMPLETE |
| I | §§22.2,38.2: no mutable execution state in plan | COMPLETE |
| J | §§38.3,38.15: useful order survives | COMPLETE |
| K | §§38.3,38.16: low-startup alternative survives | COMPLETE |
| L | §§22.4.1,38.3: infeasible alternative cannot dominate | COMPLETE |
| M | §§36.2.1,38.4: invalid raw cost rejected | COMPLETE |
| N | §36.2.1: saturation remains comparable metadata | COMPLETE |
| O | §38.4: invalid epsilon handling | **GAP N38-1** |
| P | §§36.2.1,38.4: tolerance arithmetic | **GAP N38-1** |
| Q | §38.4: approximate tie must be total/transitive | **GAP N38-1** |
| R | §38.5: physically different plans need distinct key | **GAP N38-2** |
| S | §38.5: fingerprint collision cannot decide | COMPLETE |
| T | §§38.4–5: pointer cannot affect tie | **GAP N38-1/N38-2** |
| U | §§38.4–5: hash iteration cannot affect selection | **GAP N38-1/N38-2** |
| V | §38.6: estimated-zero SeqScan retained | COMPLETE |
| W | §§36.5,38.6: absent ANALYZE retains legal index | COMPLETE |
| X | §§35,38.6: access algorithms share cardinality | COMPLETE |
| Y | §§37.8,38.7: three-way predicate unavailable early | COMPLETE |
| Z | §§37.8,38.7: one predicate owner/work stage | COMPLETE |
| AA | §§37.10,38.7: bushy transition retained | COMPLETE |
| AB | §§37.15,38.7: invalid LEFT orientation excluded | COMPLETE |
| AC | §§28.7,38.8,38.16: build remains blocking | COMPLETE |
| AD | §§28.6,38.9: duplicate payload retained | COMPLETE |
| AE | §38.9: zero load factor invalid | **GAP N38-5** |
| AF | §§28.11,38.9: bounded recursive passes | **GAP N38-5** |
| AG | §§36.2.2,38.10: inner materialization not free | COMPLETE |
| AH | §38.10: one materialization not repeatedly charged | COMPLETE |
| AI | §38.11: absent usable NDV disables refinement | COMPLETE |
| AJ | §38.11: locality cannot make lookups free | COMPLETE |
| AK | §§27–28,38.11: MVCC work retained | COMPLETE |
| AL | §§22.4.1,38.12: unsupported MergeJoin excluded | CAPABILITY-CONDITIONAL / COMPLETE |
| AM | §§37.3,38.12: mismatched order enforced/rejected | COMPLETE |
| AN | §§36.2.1,38.13: N=0/1 finite | COMPLETE |
| AO | §§30.2,38.13: varlen payload included | COMPLETE |
| AP | §§30.5,36.3,38.13: spill reads/writes included | COMPLETE |
| AQ | §§30.7,38.15: unrepresentable K makes Top-N ineligible | COMPLETE |
| AR | §§30.7,38.15: valid LIMIT retains fallback | COMPLETE |
| AS | §§19.14,38.16: unknown count does not produce exact K | COMPLETE |
| AT | §§35.2,38.16: K=0 objective is not proof | COMPLETE |
| AU | §§29.5,35.22,38.14: global aggregate output retained | COMPLETE |
| AV | §§29.2,38.14: exact state size/alignment | COMPLETE |
| AW | §38.14: ordered aggregate optional | CAPABILITY-CONDITIONAL / COMPLETE |
| AX | §38.14: ordered DISTINCT optional | CAPABILITY-CONDITIONAL / COMPLETE |
| AY | §§37.3,38.15: no redundant Sort | COMPLETE |
| AZ | §38.15: enforcement cost included | COMPLETE |
| BA | §§36.2.1,38.16: total saturates | COMPLETE |
| BB | §38.16: fraction clamped | COMPLETE |
| BC | §38.16: blocking work not scaled away | COMPLETE |
| BD | §38.16: no general join propagation | COMPLETE |
| BE | §§20,38.17: VOLATILE not reordered | COMPLETE |
| BF | §§20,38.17: error-sensitive predicate fixed | COMPLETE |
| BG | §38.17: ratio is optional/guarded | COMPLETE |
| BH | §§31,37.4,38.18: DML RID retained | COMPLETE |
| BI | §§23–24,38.18: row-handle lifetime required | COMPLETE |
| BJ | §38.19: simultaneous target sum bounded | COMPLETE |
| BK | §38.19: sequential phases not blindly summed | COMPLETE |
| BL | §38.19: canonical distribution/redistribution | **GAP N38-3** |
| BM | §38.20: equality does not predict spill | COMPLETE |
| BN | §§38.13,38.20: reads/deep-copy included | COMPLETE |
| BO | §§24,38.20: runtime spill remains possible | COMPLETE |
| BP | §§37.11,38.21: equality remains exhaustive | COMPLETE |
| BQ | §38.21: triggered guard requires fallback | COMPLETE |
| BR | §§38.21,39.4: bounded failure controlled | COMPLETE |
| BS | §§34,38.22: retained snapshot stable | COMPLETE |
| BT | §§34,38.22: actuals do not rewrite stats | COMPLETE |
| BU | §38.23: estimate/proof distinct | COMPLETE |
| BV | §38.5: fingerprint nonauthoritative | COMPLETE |
| BW | §§37.5,38.24: false ordering rejected | COMPLETE |
| BX | §§37.4,38.24: missing slot rejected | COMPLETE |
| BY | §§30.7,38.24: invalid Top-N rejected pre-execution | COMPLETE |
| BZ | §§35.2,38.24: estimated-zero empty rejected | COMPLETE |
| CA | §§31,38.24: no side effects before validation | COMPLETE |
| CB | §§36.4,38.4: valid calibration may change plan | COMPLETE |
| CC | §§38.4–5: fixed-input container independence | **GAP N38-1/N38-2** |
| CD | §§22.4.1,38.12–14: optional absence legal | COMPLETE |
| CE | §§36.2.1,39.4: high cost not resource error | COMPLETE |
| CF | §§33.3,38.19,38.21: planning/query-memory separation | **GAP N38-3/N38-4** |
| CG | §§24,33.3,39: runtime failure not config error | COMPLETE |
| CH | §§35,37.16,38.7: join cardinality algorithm-independent | COMPLETE |
| CI | §§20.14,37.17,38.16: scalar second row retained | COMPLETE |
| CJ | §§20.14,37.17,38.16: IN complete build retained | COMPLETE |
| CK | §§20.17.10,35.2,38.2–3,38.24: proof preserved | COMPLETE |
| CL | §§38.22–23: diagnostics retain provenance | COMPLETE |

## Invariant assessment

| Invariant | Operative owner | Adversarial case | Assessment |
|---|---|---|---|
| 1 | §§38.1–2 | A–F | COMPLETE |
| 2 | §§38.1,38.16 | E–F | COMPLETE |
| 3 | §§38.3,38.15–16 | J–K | COMPLETE |
| 4 | §§36.2.1,38.4 | M–Q | **GAP N38-1** |
| 5 | §§38.4–5 | T–U, CC | **GAP N38-1/N38-2** |
| 6 | §38.5 | R–S, BV | **GAP N38-2** |
| 7 | §§35,38.6 | V–X | COMPLETE |
| 8 | §§35,37.16,38.7 | X, CH | COMPLETE |
| 9 | §§22.4.1,38.7/12/14 | AL, AW, AX, CD | COMPLETE |
| 10 | §§38.9,38.18–20 | AD–AF, BL | **GAP N38-3/N38-5** |
| 11 | §§38.13–15 | AP, AZ | COMPLETE |
| 12 | §§38.1,38.16 | E–F, AT | COMPLETE |
| 13 | §§33,37.11,38.21 | BP–BR, CF | **GAP N38-4** |
| 14 | §§33.4,34,38.22 | BS | COMPLETE |
| 15 | §§34–35,38.22 | BS–BT | COMPLETE |
| 16 | §§38.23,40 | BU, CL | COMPLETE |
| 17 | §§20,38.24 | BZ, CA | COMPLETE |
| 18 | §§22,38.24 | BW–CA | COMPLETE |
| 19 | §§35.2,38.6–7,38.24 | V, AP, BZ | COMPLETE |

## Global contradiction search

No true cross-chapter semantic contradiction was found.

Important classifications:

- `total_cost = startup + run`: valid shorthand resolved by Chapter 36’s checked saturating addition.
- RequiredRowsObjective versus Top-N K: valid different roles.
- Planning arena versus query execution memory: valid different resources, but §38.19 terminology/allocation is underdefined.
- Estimated zero versus semantic proof: consistently separated.
- Crossing predicates: valid upstream Chapter-37 owner.
- Stable statistics “snapshot”: valid retained per-object descriptor set, not one global StatsVersion.
- PlanFingerprint collisions: explicitly diagnostic-only.
- Merge/ordered algorithms: valid optional capabilities.
- §38.21 threshold behavior: consistent with closed Chapter 37.
- Historical pre-Fix-B wording: nonnormative and not used.

## Verification readiness

Existing reusable procedures include:

- V20-12–16, V20-20–22: subquery demand, predicate safety, logical/physical ownership.
- V22-B–D, V22-I–L: slots, properties, capability, objectives, validation, structural determinism.
- V24-A–O: memory accounting, arenas, exact extents, spill, errors, lifecycle.
- V27-G/N: ordering and required slots.
- V28-A–T: join algorithms, orientation, hash memory/spill, MergeJoin, determinism.
- V29-A–O: aggregate state, spill, ordered alternatives, properties.
- V30-A–K: Sort, external merge, Top-N, exact K, demand, properties.
- V31-A/B/E/G/H/N: DML closure, RID, retry, publication.
- V32-A/B/C/H/I/J: occurrence, claims, replay, completion.
- V33-A–N: planning trace, memo/dominance, ties, resources, stable inputs, validation.
- V34-B/C/D/J/K: retained generations and advisory statistics.
- V35-A–J: cardinality, widths, exact K, fallback, proof.
- V36-A–J: CostConfig, saturation, work ownership, objectives, resources.
- V37-001–070: Chapter-37 properties, search, predicates, resources, algorithms, and validation.
- Named suites: Memo and Pruning Tests, Cost Model Tests, Optimizer Determinism and Resource-Limit Tests, Memory/Spill Plan Tests, Join-Order Tests, Final Optimizer Validation Tests, Optimizer Diagnostics Tests.

Future V38 synchronization must add integration procedures for all 25 subsections and 19 invariants, especially:

- Exact epsilon domain and total comparator after repair.
- Structural-key field completeness and collision fixtures.
- Dominance feasibility.
- Every specialized child/local/repeated cost owner.
- Hash load-factor and recursive-spill configuration.
- Pipeline liveness and target assignment.
- Exact first-K versus blocking behavior.
- Planning-resource guard/configuration/fallback.
- Final malformed-candidate validation.
- Diagnostics/provenance.
- All adversarial cases in this review.

These are future Verification gaps, not additional Architecture findings.

## Finding counts

- BLOCKING: 0
- MAJOR: 5
- MINOR: 0
- EDITORIAL: 0
- DESIGN-SCOPE QUESTIONS: 0
- FROZEN SEMANTIC QUESTIONS: 0

## Required next action

The next Architecture action should be one narrowly scoped **Chapter-38 Architecture Fix A** limited to:

- §38.4 — tie configuration and deterministic total comparison.
- §38.5 — collision-free structural-key identity.
- §38.9 — hash/spill tuning configuration.
- §38.19 — canonical execution-memory target assignment and terminology.
- §38.21 — planning-resource configuration and deterministic guard.
- Corresponding refinements to §38.25 invariants 4–6, 10, and 13.

No new optimizer architecture, SQL semantics, cost coefficients, algorithms, error enums, or public resource failures are required.

After that repair, perform a separate independent read-only Chapter-38 Architecture closure audit. Do not synchronize Chapter-38 Verification before Architecture closes.

`git diff --check`: passed with no output.

CHAPTERS 31–37 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTERS 31–37 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 38 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;NEEDS ARCHITECTURE FIX

CHAPTER 38 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;NOT SYNCHRONIZED

CHAPTER 39 REVIEW:
&nbsp;&nbsp;&nbsp;&nbsp;NOT STARTED

IMPLEMENTATION:
&nbsp;&nbsp;&nbsp;&nbsp;NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
&nbsp;&nbsp;&nbsp;&nbsp;NOT RUN

AUDIT-CREATED CHANGES:
&nbsp;&nbsp;&nbsp;&nbsp;NONE

END CHAPTER-38 INITIAL READ-ONLY
ARCHITECTURE REVIEW.