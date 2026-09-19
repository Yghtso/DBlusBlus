# Chapter 37 Architecture Fix A Report

## 1–7. Repository and diff state

1. Initial HEAD:

```text
515d0d6ea9a71747868078bb73196a45fcd9dd5b
515d0d6 chapter 37 ARCHITECTURE analysis
```

2. Initial tracked worktree and index: clean.

3. The Chapter-37 review artifact is now committed historical content under HEAD. It was not modified or used as semantic authority.

4. Final HEAD remains:

```text
515d0d6ea9a71747868078bb73196a45fcd9dd5b
```

Final status:

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

5. Task-modified files:

```text
docs/ARCHITECTURE.md
```

6. Changed subsections:

- §37.2
- §37.6
- §37.7
- §37.8
- §37.10
- §37.11
- §37.12
- §37.13
- §37.18

All hunks are inside Chapter 37. Chapter 38 now begins at line 27826 and remains unchanged.

7. Diff summary:

```text
164 insertions
18 deletions
```

## 8–18. N37-1 — Search configuration and RelationSet capacity

### Final configuration contract

`exhaustive_join_limit` is now a mandatory nonnegative integer search-configuration value:

```text
default = 10 BindingId occurrences
valid domain = integer >= 0
```

Zero is valid and sends every nonempty reorderable region to bounded heuristic mode.

For region size `N`:

```text
N <= exhaustive_join_limit:
    exhaustive bushy DP
    unless planning resources trigger fallback

N > exhaustive_join_limit:
    bounded heuristic
```

The count is per maximal reorderable region and counts distinct `BindingId` occurrences—not tables, workers, morsels, rows, or the whole statement.

Missing, negative, or unrepresentable values produce existing `OptimizerError` ownership before search-state construction. Values cannot be wrapped or clamped.

`large_join_max_local_passes` is likewise mandatory and nonnegative:

```text
default = 4
valid domain = integer >= 0
```

Zero means:

- construct the deterministic greedy initial tree;
- run no local-improvement pass.

A complete pass examines the enabled legal move neighborhood once in stable structural order, applies Chapter-38 objective/tie comparison, and adopts only a preferred candidate. Search stops after:

- the first non-improving complete pass; or
- exactly the configured maximum completed passes.

The greedy construction does not consume the pass budget. Missing, negative, unrepresentable, or overflowing values are invalid configuration.

### Large-join capacity

The undefined “configured large-join planning limit” was removed. Chapter 37 now defines no separate relation-count or native-word cap.

`RelationSet` must represent every `BindingId` in the complete region exactly. An implementation may use fixed-width storage only when it fits; otherwise it must use a wider/growable representation.

A 65-relation region cannot be truncated merely because an implementation has a 64-bit native word.

Outcomes are separated:

- malformed search configuration → `OptimizerError`;
- internal truncation/capacity/arithmetic defect → `OptimizerError` or internal invariant failure;
- actual planning-arena exhaustion → bounded fallback, then `OptimizerResourceLimit` if the bounded search also cannot fit;
- threshold exceedance alone → heuristic mode, not an error;
- no SQL relation-count limit was introduced.

Both search values are retained as part of the stable Chapter-33 invocation configuration. External changes can affect later invocations only.

### N37-1 adversarial results

| Case | Required outcome |
|---|---|
| A Default threshold | 10 |
| B Region equals threshold | Exhaustive mode |
| C One above threshold | Heuristic mode |
| D Negative threshold | `OptimizerError` before search |
| E Zero threshold | Heuristic for every nonempty region |
| F Unrepresentable threshold | `OptimizerError`; no clamp |
| G Default pass budget | 4 |
| H Negative pass count | `OptimizerError` |
| I Zero passes | Greedy tree only |
| J Counter reaches maximum | Stop exactly at configured bound |
| K Invalid setup value | Rejected before search-state construction |
| L External mid-plan change | Active invocation keeps retained values |
| M 64 BindingIds | Exact representation |
| N 65 BindingIds | Wider/growable exact representation |
| O Beyond native word | No truncation or alias collapse |
| P Growable representation under pressure | Use bounded fallback if possible |
| Q Exhaustive DP reaches guard | Switch to heuristic |
| R Bounded heuristic cannot fit | `OptimizerResourceLimit` |

**N37-1: CLOSED**

## 19–29. N37-2 — Cartesian admission

### Final subset and partition rule

A partition without a newly activated §37.8 crossing predicate is Cartesian. It is admitted only for:

1. **Multi-relation-predicate prerequisite**

   The target subset is a proper subset of an unavailable predicate’s exact referenced set, the predicate references at least three relations, and assembling the subset can make that predicate available later.

2. **Complete-component assembly**

   Both children are unions of complete connected components of the predicate hypergraph, and no predicate connects those groups.

Hypergraph connectivity treats a genuine multi-relation predicate as one hyperedge. It is not silently decomposed into pairwise predicates.

For any target subset:

- legal predicate-crossing partitions are considered first;
- if at least one exists, Cartesian partitions for that target are excluded;
- otherwise only the two necessary classes above are admitted.

“Connected first” no longer means “enumerate every Cartesian alternative afterward.”

### Required scenarios

- Pairwise chain `A--B--C`: `{A,C}` is excluded.
- Disconnected `{A,B}` and `{C,D}` components: optimize each component, then permit their Cartesian assembly.
- Sole predicate over `{A,B,C}`: permit a predicate-free two-relation prerequisite, then activate the predicate only at the complete three-relation node.
- Explicit CROSS JOIN: remains representable as complete-component assembly.
- `(A join B) join (C join D)`: remains a legal bushy tree; if no cross-component predicate exists, the final connection is Cartesian.
- Cartesian cost affects selection, not SQL legality or semantic proof.

The same admission policy now governs exhaustive DP, greedy extension, and legal local moves. No separate heuristic Cartesian policy remains.

**N37-2: CLOSED**

## 30–37. N37-3 — Multi-relation predicate placement

### Activation and crossing rules

For predicate occurrence `P` with referenced set `R`, node set `S` may evaluate or discharge it only when:

```text
R ⊆ S
```

For partition:

```text
S = L ∪ U
L ∩ U = ∅
```

`P` is crossing only when:

```text
R ⊆ S
R intersects L
R intersects U
```

and placement remains legal under Chapter-20 demand, error, and rewrite rules.

Partial intersection never activates a predicate.

### Exactly-once ownership

Each movable predicate occurrence belongs to the lowest legal join node in that candidate tree where:

- all referenced bindings are available; and
- the predicate crosses the two children.

Consequences:

- no premature evaluation;
- no omitted predicate;
- no duplicate ancestor attachment;
- one-relation predicates remain vertex/local work;
- unsafe predicates remain at their canonical semantic boundary;
- distinct bound occurrences with identical text remain distinct;
- derived equalities remain separate metadata and do not discharge the original expression.

“Exactly once” refers to one plan-placement owner, not one runtime evaluation for the statement. Runtime still evaluates the predicate over every required candidate occurrence.

The owning stage is also the single logical selectivity and work-cost stage for that tree. Chapter 35 continues to own selectivity; Chapters 36 and 38 own physical work and cost.

### N37-3 adversarial results

| Case | Outcome |
|---|---|
| A One-relation predicate | Vertex/local owner |
| B Two-relation equality | Lowest legal binary join |
| C `{A,B,C}` predicate | Deferred until all three available |
| D Same predicate at `{A,B}` | Unavailable |
| E At `{A,B,C}` | Eligible |
| F `{A,B}\|{C}` | Crossing and owned there |
| G `{A,C}\|{B}` | Crossing if that prerequisite tree is lawfully admitted |
| H One bound occurrence represented twice | One placement owner |
| I Two same-text bound occurrences | Each placed independently |
| J Derived equality plus original | Derived metadata does not replace original |
| K Erroring predicate | Remains at demand-safe legal boundary |
| L UNKNOWN-producing predicate | SQL 3VL preserved |
| M LEFT JOIN boundary | Cannot escape constrained boundary |
| N Hyperedge-only graph | Necessary prerequisite, then activation |
| O Missing referenced binding | Never activates; candidate cannot discharge it |

No Chapter-38 contradiction was introduced: §38.7’s “identify crossing join predicates” now has an unambiguous Chapter-37 input.

**N37-3: CLOSED**

## 38–43. N37-4 — Ordering normalization

### Final policy

Before property identity, memo lookup, satisfaction, interesting-order retention, or enforcement, order vectors are normalized left-to-right by removing later exact duplicate `OrderKey` entries.

Exact duplicate means all four fields match:

```text
LogicalSlotId
direction
NULL order
collation
```

The first key is retained.

No key is removed because of:

- matching display text;
- common lineage;
- a derived equality;
- statistics;
- apparent expression similarity.

Computed expressions with different hidden `LogicalSlotId` values remain distinct.

Normalization affects only the property vector. It does not:

- remove a retained slot;
- suppress demanded expression evaluation;
- merge distinct expression occurrences;
- fabricate ordering after projection removes a key.

### Required ordering cases

| Case | Canonical result |
|---|---|
| Empty ordering | Remains empty |
| `(x)` versus `(x)` | Identical |
| Required `(x,x)`, available `(x)` | Required normalizes to `(x)`; satisfied |
| Required `(x)`, available `(x,x)` | Available normalizes to `(x)`; satisfied |
| Same display name, different slots | Both retained; not identical |
| Different direction | Both retained |
| Different NULL order | Both retained |
| Different collation | Both retained |
| Hidden computed slot | Identity uses hidden slot |
| Potentially erroring duplicate expression | Property deduplication cannot suppress evaluation |
| Key projected away | Provided ordering no longer advertises it |
| Ordered index path | Canonical normalized property retained |
| Cheaper unordered alternative | Cannot dominate useful ordered alternative merely by local cost |
| Equivalent property vectors | Normalize to one memo property identity |

Exact prefix matching remains unchanged after normalization.

**N37-4: CLOSED**

## 44–45. N37-5 — Timeless wording

Original:

```text
A small deterministic beam may be added later...
```

Final:

```text
Deterministic beam search is an optional extension and is not required by the
v1 baseline.
```

Beam search remains optional.

**N37-5: CLOSED**

## 46–48. Invariants

The original 14 invariant meanings were preserved.

Affected invariants were refined:

- invariant 4: exact-duplicate ordering normalization;
- invariant 7: validated nonnegative threshold, default 10, per-region stability;
- invariant 8: deterministic greedy tree and nonnegative pass budget, default 4;
- invariant 9: hyperedge prerequisites and complete disconnected-component assembly.

One supporting invariant was appended:

```text
15. A predicate occurrence activates only when all referenced BindingIds are
available and has exactly one lowest legal owning placement in each candidate
join tree, without weakening demand, error, selectivity, or cost ownership.
```

Final invariant count: **15**.

## 49–57. Regression and downstream assessment

- LEFT JOIN nesting and supported hash orientation: unchanged.
- Expression-subquery fallback, occurrence identity, scalar checks, EXISTS demand, IN complete build, rewrites, and correlation rejection: unchanged.
- Chapter 31 DML slots, candidate closure, errors, and W/C/R: unchanged.
- Chapter 32 source occurrences, claims, replay, and early stop: unchanged.
- Chapter 33 stable inputs, active objectives, ties, bounded fallback, and validation: preserved.
- Chapter 34 retained statistics identities and advisory status: unchanged.
- Chapter 35 cardinality, 3VL, and proof separation: preserved.
- Chapter 36 finite cost, work ownership, and base-path legality: preserved.
- Chapter 38 memo identity, dominance, comparator, objectives, resource fallback, and final validation: unchanged and consistent.

## 58–62. Error ownership, consistency, and remaining issues

Error/resource classification:

| Condition | Outcome |
|---|---|
| Missing/negative/unrepresentable search configuration | `OptimizerError` |
| Internal RelationSet truncation/capacity defect | `OptimizerError` / invariant failure |
| Region above exhaustive threshold | Heuristic mode |
| Exhaustive planning guard reached | Bounded fallback |
| Bounded planning cannot fit | `OptimizerResourceLimit` |
| Unsupported optional algorithm | Capability exclusion |
| Invalid final property/predicate plan | Final validation failure |
| Runtime allocation/spill failure | Existing runtime owner |

Global consistency searches found:

- one definition of each search configuration;
- no remaining undefined “large-join planning limit”;
- one Cartesian policy for exhaustive and heuristic search;
- one crossing-predicate definition;
- one exact ordering-normalization policy;
- beam search remains optional;
- no competing comparator, proof source, or resource protocol.

Document role remains timeless, bounded, implementation-independent, and proportional to the v1 optimizer.

Remaining findings: **NONE within Fix-A scope**.

New semantic questions: **NONE**.

This task does not independently close Chapter-37 Architecture.

## 63–64. Validation and final state

`git diff --check`:

```text
PASS
```

Final repository state:

```text
HEAD: 515d0d6ea9a71747868078bb73196a45fcd9dd5b
 M docs/ARCHITECTURE.md
Index: clean
```

Confirmations:

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
CHAPTERS 31–36 NOT MODIFIED
CHAPTER 38 NOT MODIFIED
HISTORICAL ARTIFACTS NOT MODIFIED
PRE-EXISTING REVIEW DIRECTORY PRESERVED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
CHAPTER 38 REVIEW NOT STARTED
```

Finding disposition:

```text
N37-1: CLOSED
N37-2: CLOSED
N37-3: CLOSED
N37-4: CLOSED
N37-5: CLOSED
```

CHAPTER 37 ARCHITECTURE FIX A COMPLETE —
READY FOR INDEPENDENT READ-ONLY CLOSURE AUDIT