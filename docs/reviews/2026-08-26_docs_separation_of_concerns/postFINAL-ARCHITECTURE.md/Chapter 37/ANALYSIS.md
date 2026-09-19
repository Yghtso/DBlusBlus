# Chapter 37 Initial Architecture Review

## 1–4. Verdict and repository state

**Initial Architecture review verdict: NEEDS ARCHITECTURE FIX**

Chapter 37 is broadly coherent and correctly integrated with Chapters 19–20, 22, and 33–38. Its property, join-algorithm, outer-join, cardinality, and subquery contracts are generally sound.

Five findings remain:

- 3 MAJOR
- 1 MINOR
- 1 EDITORIAL
- 0 BLOCKING
- 0 design-scope questions
- 0 frozen-semantic questions

Initial repository state:

```text
HEAD:   377372d8bdb68f7c50efd7a324b3e0f41fc707ef
Commit: 377372d applied SYNC FIX-1 36 in ARCHITECTURE
Index:  clean
Tracked worktree: clean
```

Pre-existing untracked path:

```text
docs/reviews/2026-08-26_docs_separation_of_concerns/
  postFINAL-ARCHITECTURE.md/Chapter 37/
```

It was preserved and was not treated as canonical evidence.

Final state is identical. Audit-created changes: **NONE**.

## 5–7. Live Chapter 37 structure

Title:

```text
37. Physical Properties and Join Enumeration
```

Boundaries:

- Chapter 37 begins: [docs/ARCHITECTURE.md:27228](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27228)
- Last substantive Chapter-37 line: 27676
- Separator: line 27678
- Chapter 38 begins: line 27680

Subsection inventory:

| Section | Title |
|---|---|
| §37.1 | Scope |
| §37.2 | OrderingProperty |
| §37.3 | Ordering satisfaction |
| §37.4 | RequiredSlotSet |
| §37.5 | Provided-ordering rules |
| §37.6 | Interesting orders |
| §37.7 | RelationSet |
| §37.8 | Join graph |
| §37.9 | Reorderable regions and outer-join constraints |
| §37.10 | Bushy dynamic programming |
| §37.11 | Exhaustive threshold |
| §37.12 | Cartesian products |
| §37.13 | Large-join heuristic |
| §37.14 | Join algorithm alternatives |
| §37.15 | Hash-join orientation |
| §37.16 | Logical join cardinality is algorithm-independent |
| §37.17 | Subquery physical planning |
| §37.18 | Join/property invariants |

Actual §37.18 invariant inventory:

1. Small baseline property system.
2. Exact four-part ordering-prefix satisfaction.
3. Only runtime-guaranteed ordering advertised.
4. Useful interesting-order retention without arbitrary classes.
5. Relation identity by `BindingId`.
6. Bushy exhaustive search for small regions.
7. Configurable exhaustive threshold, default 10.
8. Bounded deterministic large-join search.
9. Avoid unnecessary Cartesian joins.
10. Joint INNER order/algorithm/orientation optimization.
11. LEFT JOIN boundaries and supported hash orientation.
12. Algorithm-independent logical join cardinality.
13. Capability-enabled algorithms only.
14. Correlation rejection and mandatory expression-subquery fallback.

## 8. Canonical owner matrix

| Subject | Canonical owner | Chapter-37 role |
|---|---|---|
| Binding/occurrence identity | §§19.2–19.3 | Uses `BindingId` |
| Slots and logical schema | §§20.1–20.2 | Property and required-output identity |
| Join bag/NULL semantics | §20.8 | Searches only equivalent legal trees |
| Subquery semantics | §20.14 | Physical planning and costing |
| Rewrite/demand safety | §20.17 | Restricts graph derivation/reordering |
| Physical capability | §22.4.1 | Gates algorithms |
| Physical properties | §22.7 and Chapter 37 | Chapter 37 is canonical taxonomy owner |
| Runtime ordering | Chapters 27–30 | Determines advertised properties |
| Optimizer inputs/fallback | Chapter 33 | Stable search configuration and bounded planning |
| Statistics | Chapter 34 | Advisory immutable inputs |
| Cardinality | Chapter 35 | Shared logical estimates |
| Base paths/costs | Chapter 36 | Inputs to join search |
| Memo/cost/dominance | Chapter 38 | Consumes Chapter-37 search states |
| Errors/resources | Chapter 39 | Owns optimizer and runtime failures |
| Diagnostics | Chapter 40 | Makes search decisions inspectable |
| Verification obligations | §41.7 | Requires integration procedures |

## 9–17. Property and identity assessment

### Property-system scope

The baseline correctly tracks only:

```text
OrderingProperty
RequiredSlotSet
```

Partitioning, rewindability, and materialization are reserved extension points, not mandatory properties. `required_rows` is correctly separated as a Chapter-38 search objective.

Assessment: **COMPLETE**.

### OrderingProperty structure and identity

`OrderKey` includes:

```text
LogicalSlotId
ASC | DESC
NULLS_FIRST | NULLS_LAST
collation
```

The contract correctly preserves:

- query-local slot identity;
- hidden slots for computed ordering expressions;
- binary VARCHAR collation;
- direction and NULL-order distinctions;
- empty-vector “no ordering” semantics;
- self-join and alias separation.

Assessment: **COMPLETE**, subject to N37-4’s normalization ambiguity.

### Exact ordering satisfaction

The exact-prefix rule is unambiguous for already canonical vectors:

- `(a,b,c)` satisfies `(a,b)`;
- `(a)` does not satisfy `(a,b)`;
- `(a,b)` does not satisfy `(b)`;
- changed slot, direction, NULL order, or collation fails;
- empty required ordering is the empty prefix and imposes no ordering demand;
- forward-only B+ scans do not imply DESC.

Assessment: **COMPLETE**.

### Provided ordering by operator

| Operator | Chapter-37 contract | Assessment |
|---|---|---|
| SeqScan | none | Correct |
| Forward IndexScan | compatible ASC/NULLS FIRST key order | Correct; Chapter 27 preserves cursor order when advertised |
| Filter | preserves input | Correct |
| Simple/reference Project | preserves surviving unchanged keys | Correct |
| Limit | preserves child order | Correct |
| Sort | exact requested order | Correct |
| Top-N | exact final order when eligible | Correct |
| HashJoin | none | Correct |
| HashAggregate | none | Correct |
| Hash DISTINCT | none | Correct |
| NLJ/INLJ/merge/ordered algorithms | only when runtime contract guarantees it | Correct capability boundary |

### Interesting-order retention and dominance

The chapter correctly prevents an unordered locally cheaper plan from automatically dominating a useful ordered alternative. Chapter 38 supplies active-objective dominance and property-state identity.

The list is capability-conditional where appropriate; it does not mandate merge join, ordered aggregation, or ordered DISTINCT.

Assessment: **COMPLETE except N37-4**.

### RequiredSlotSet derivation

The declared closure includes:

- final output;
- predicates;
- join keys and residuals;
- grouping/aggregate arguments;
- ordering;
- DML assignments and `RETURNING`;
- hidden target RID/system state.

Computed ORDER BY expressions are supported through hidden slots in §37.2. Frozen DML state remains protected.

Assessment: **COMPLETE**.

### Memo/search identity and `required_rows`

Chapter 37’s reduced key:

```text
RelationSet + OrderingProperty
```

is explicitly conditional on deterministic slot derivation and absence of a propagated finite row objective.

Chapter 38 correctly expands identity to include:

```text
logical subproblem
required output-slot class
ordering class
propagated RequiredRowsObjective class
```

No collision between full-result and propagated first-K states is authorized.

Assessment: **COMPLETE**.

### RelationSet and BindingId

Relation identity correctly uses `BindingId`, not `TableId`. Self-join aliases consume distinct bits. Required operations and deterministic iteration are named.

Assessment: **COMPLETE except capacity/configuration details in N37-1**.

### Large relation-set capacity

Silent truncation is expressly forbidden, but the “configured large-join planning limit” is not itself defined. Its representation, valid range, relationship to resource limits, and over-limit outcome are absent.

Assessment: **GAP — N37-1**.

## 18–28. Join graph and search

### Join-graph construction

Vertices and local predicates are well-defined. Outer joins are kept outside unrestricted INNER/CROSS graphs. Safe equality derivation remains under §20.17.7.

Assessment: **PARTIAL** because multi-relation predicate activation is undefined.

### Multi-relation predicates and derived equalities

Safe derived equalities are correctly constrained by NULL/outer-join semantics.

However, an edge retains its exact referenced `RelationSet` without defining when it becomes executable. For a predicate referencing `A,B,C`, the chapter does not explicitly require:

```text
referenced_relations ⊆ current subset S
```

before treating it as a crossing predicate, nor does it define its exactly-once attachment point.

Assessment: **GAP — N37-3**.

### Reorderable regions and LEFT JOIN

Maximal legal INNER/CROSS regions are searched. LEFT JOIN nesting remains constrained, with legal optimization inside each side and cross-boundary movement only after an independently valid logical rewrite.

Assessment: **COMPLETE**.

### Bushy DP completeness

The chapter requires:

- singleton-to-larger subset construction;
- nonempty disjoint partitions;
- union equal to the target subset;
- useful-property combinations;
- bushy trees;
- deterministic symmetry elimination.

The `(A⋈B)⋈(C⋈D)` shape is explicitly included.

Assessment: **COMPLETE for predicate-connected subsets**, but Cartesian-subset admission is ambiguous under N37-2.

### Symmetric partition handling

Requiring the least `BindingId` bit to reside in one side is a valid deterministic symmetry-breaking example. Physical build/probe orientations remain separately enumerated.

Assessment: **COMPLETE**.

### Exhaustive threshold and defaults

Live default:

```text
exhaustive_join_limit = 10 relation bindings
```

It is correctly:

- per reorderable region;
- counted in relation occurrences;
- inclusive at the threshold;
- a planning parameter rather than SQL validity;
- subject to planning-memory fallback.

Its numeric domain and validation are missing.

Assessment: **PARTIAL — N37-1**.

### Planning-memory fallback

Chapter 38 requires transition to bounded heuristic search and ultimately controlled `OptimizerResourceLimit` if bounded planning also cannot fit.

Assessment: **COMPLETE**.

### Cartesian legality and priority

Necessary Cartesian products remain legal, and a blanket correctness-changing ban is prohibited.

The exact admission rule is inconsistent:

- §37.10 says connected partitions are enumerated **before** Cartesian alternatives;
- §37.12 says unnecessary Cartesian products are **not introduced** while a connected alternative exists;
- §37.13 says disconnected regions introduce Cartesian edges only when logically necessary.

For chain `A-B-C`, it is unclear whether subset `{A,C}` may be constructed through `A×C` before joining `B`. Its subset has no crossing edge, but the complete region is connected and has a non-Cartesian tree.

Assessment: **GAP — N37-2**.

### Large-join heuristic

The required core is reasonable:

- connected greedy construction;
- lowest incremental active-objective cost;
- stable structural tie-break;
- bounded local passes;
- early termination after a non-improving complete pass;
- default pass count 4;
- no beam requirement.

The listed local moves are deliberately optional. Their optionality is not itself a defect, and global optimality is not required.

The configuration domain for the pass count remains missing under N37-1.

Assessment: **PARTIAL**.

### Deterministic greedy and local improvement

Stable ties and fixed inputs prohibit unordered-container dependence. Legal moves must preserve outer-join, slot, property, and capability constraints.

Different optional deterministic move neighborhoods are permitted search approximations. A missed unvisited cheaper plan is not a defect.

Assessment: **COMPLETE except configuration validation**.

## 29–34. Join algorithms, cardinality, and proof

### Joint order/algorithm enumeration

Every implemented applicable algorithm is enumerated for each admitted binary pair, and order cannot be frozen before algorithm choice where that would discard required combinations.

Assessment: **COMPLETE**.

### Supported versus optional algorithms

- HashJoin: baseline where applicable.
- NestedLoopJoin: baseline.
- IndexNestedLoopJoin: baseline where an eligible indexed inner exists.
- MergeJoin: capability-conditional.
- Ordered aggregate/DISTINCT: capability-conditional.

Cost does not establish capability.

Assessment: **COMPLETE**.

### INNER hash orientation

Both semantically equivalent runtime-supported build/probe orientations are required. Payload width, memory, spill, probe work, and downstream properties are cost inputs.

Assessment: **COMPLETE**.

### LEFT hash orientation

The baseline requires:

```text
logical right -> build
logical left  -> preserved probe
```

No cheaper swapped orientation becomes legal without a prior semantic rewrite.

Assessment: **COMPLETE**.

### Algorithm-independent logical cardinality

One logical join/predicate identity consumes one Chapter-35 cardinality estimate across physical algorithms. Cost, memory, spill, ordering, and startup may differ; logical multiplicity may not.

Assessment: **COMPLETE**.

### Estimate versus exact proof

Estimated zero remains cost metadata. It cannot remove a join, establish absence, or create early termination. Exact proof remains owned by §§20.17.10 and 35.2.

Assessment: **COMPLETE**.

## 35–42. Subquery planning

### Expression-subquery fallback

Every accepted expression-subquery occurrence retains the mandatory lazy fallback unless an exact rewrite replaces it. The child is optimized as an ordinary physical subplan.

Assessment: **COMPLETE**.

### Occurrence and runtime-state identity

Runtime state is keyed by bound occurrence and initialized at most once per statement attempt. Same-text distinct occurrences remain distinct; retries discard old attempt state.

Assessment: **COMPLETE**.

### Scalar subqueries

The contract preserves:

- zero rows → typed NULL;
- one row → selected value;
- successfully constructed second row → cardinality violation;
- at-most-two final-row consumption;
- demanded first/second-row errors;
- no removal of the check from estimates or `required_rows=1`.

Assessment: **COMPLETE**.

### EXISTS

First-row demand and legal early stop are preserved. Projection-only errors are not fabricated as demanded, while relational work required to establish existence remains demanded.

Assessment: **COMPLETE**.

### IN / NOT IN

The complete child build, NULL marker, empty marker, duplicates, probe NULL, 3VL, and spill/error boundaries remain owned by §20.14.6. Cost cannot replace complete build with EXISTS-like behavior.

Assessment: **COMPLETE**.

### Rewrite proof and capability gating

Semi/anti/marker rewrites require both exact semantic proof and runtime capability. Fallback support never depends on rewrite success.

Assessment: **COMPLETE**.

### Correlated-subquery rejection

Correlated scalar, EXISTS, IN, and derived references are rejected by the binder as `UnsupportedCorrelation`; no physical node or speculative decorrelation is produced.

Assessment: **COMPLETE**.

### Literal-sensitive planning

Prepared parameters remain unsupported. Literal constants may use retained Chapter-34 statistics through Chapter-35 estimation. Different valid literals may produce different costs and plans without changing semantics.

Assessment: **COMPLETE**.

## 43–49. Regression and downstream integration

### Chapter 31

Required slots preserve target RID, system state, assignment inputs, `RETURNING`, and candidate closure. No cost/property choice alters W/C/R or mutation publication.

**No regression.**

### Chapter 32

`BindingId` occurrences are distinct from worker/morsel occurrences. Join search neither changes source multiplicity nor authorizes unsafe replay or early stop.

**No regression.**

### Chapter 33

Stable inputs, bounded search, active objectives, capabilities, tie-breaking, fallback, and final validation remain authoritative.

N37-1 leaves two Chapter-37 search inputs insufficiently validated but does not contradict Chapter 33.

**No frozen contradiction.**

### Chapter 34

Per-object retained generations and alias identity remain distinct from `RelationSet`. Statistics are advisory.

**No regression.**

### Chapter 35

Logical cardinality, width, fallback, unknown-count, 3VL, saturation, and exact-proof boundaries survive.

**No regression.**

### Chapter 36

Join search consumes legal finite base alternatives and cost metadata without changing eligibility, work ownership, or semantic proof.

**No regression.**

### Chapter 38 integration

Compatible interfaces:

- expanded memo identity;
- `PlanAlternative`;
- dominance and interesting-order retention;
- canonical tie-breaking;
- DP initialization and transition;
- algorithm-specific costs;
- memory/spill;
- full-result versus first-K objectives;
- property enforcement;
- bounded fallback;
- diagnostics;
- final validation.

Integration gaps inherited from Chapter 37:

- Chapter 38’s “crossing predicates” transition has no explicit multi-relation activation rule.
- Chapter 38 cannot resolve Chapter 37’s Cartesian-admission ambiguity.
- Chapter 38 references `exhaustive_join_limit` but supplies no missing domain/validation contract.

No competing comparator, objective, or cardinality rule was found.

## 50. Error and resource ownership

| Condition | Owner/outcome |
|---|---|
| Malformed property or internal search configuration | `OptimizerError` / internal invariant |
| Missing required slot or false provided ordering | optimizer/final-plan validation failure |
| Incompatible index | physical eligibility owner excludes it |
| Unsupported optional algorithm | capability registry excludes it |
| Illegal outer-join reorder | rewrite/search legality failure; final validation rejects |
| Correlated subquery | binder `UnsupportedCorrelation` under `UnsupportedFeature` |
| No legal physical alternative from an optimizer defect | `OptimizerError` |
| Planning budget exhausted after fallback | `OptimizerResourceLimit` |
| Runtime allocation failure | `OutOfMemory` |
| Runtime spill failure | `SpillIOError` |
| Scalar second row | `CardinalityViolation` → SQL `CardinalityError` |
| Demanded expression error | existing Chapter-17/39 SQL/runtime owner |

Exceeding the exhaustive threshold is not an error.

## 51. Determinism and complexity

- Deterministic BindingId iteration: required.
- Deterministic partition symmetry breaking: required.
- Deterministic greedy ties: required.
- Unordered-container dependence: forbidden.
- Local passes: bounded, but their count lacks a domain.
- Planning-memory fallback: defined.
- Property-state growth: bounded by useful requirements/dominance.
- Large relation identities: may use any representation, but may not truncate.

Complexity classification:

- Property model: **CORE**
- Bushy DP through 10 bindings: **JUSTIFIED ADVANCED**
- Bounded greedy/local search: **CORE**
- Interesting-order memo alternatives: **JUSTIFIED ADVANCED**
- Optional beam/Cascades/property lattice: correctly outside baseline

## 52. Adversarial-case matrix

| Case | Result |
|---|---|
| A Different slots with same name | COMPLETE — slot identity |
| B Self-join BindingIds | COMPLETE |
| C Computed ORDER BY hidden slot | COMPLETE |
| D Required prefix of longer order | COMPLETE |
| E Available shorter than required | COMPLETE |
| F Reversed direction | COMPLETE — fails satisfaction |
| G Different NULL order | COMPLETE — fails |
| H Different collation | COMPLETE — fails |
| I Forward index advertises DESC | Rejected |
| J Project drops ordered key | Property removed |
| K HashJoin advertises order | Rejected/validator failure |
| L Costlier useful order | Retained |
| M Useless arbitrary order | Not generated |
| N Required column pruned | Prevented by RequiredSlotSet |
| O DML RID pruned | Prevented/validator rejects |
| P `required_rows` made ordering key | Forbidden |
| Q Distinct memo subproblems collide | Prevented by Chapter-38 identity |
| R Self-join aliases share bit | Forbidden |
| S Native-width truncation | **GAP — N37-1** |
| T Three-relation predicate | **GAP — N37-3** |
| U Unsafe derived equality | Forbidden by §20.17.7 |
| V LEFT flattened | Forbidden |
| W Inner reorder inside LEFT side | Permitted |
| X Three-relation bushy DP | Required |
| Y Four-relation bushy plan omitted | Defect below threshold |
| Z Symmetric partition twice | Deterministically suppressed |
| AA Disconnected graph Cartesian | Legal when necessary; **admission scope GAP N37-2** |
| AB Connected graph unnecessary Cartesian | Intended forbidden, exact rule **GAP N37-2** |
| AC Small region hits memory guard | Bounded fallback |
| AD Region equals threshold | Exhaustive for valid configuration |
| AE Region above threshold | Bounded heuristic |
| AF Separate small regions | Threshold applied per region |
| AG Greedy tie from hash iteration | Forbidden |
| AH Illegal outer-join local move | Excluded |
| AI Passes exceed maximum | Forbidden, but domain/validation **GAP N37-1** |
| AJ Complete non-improving pass | Stop |
| AK No beam search | OUTSIDE BASELINE |
| AL Unsupported MergeJoin cheapest | CAPABILITY-CONDITIONAL; excluded |
| AM Tree frozen before algorithms | Forbidden where combinations lost |
| AN INNER orientation omitted | Defect when supported/applicable |
| AO LEFT hash sides swapped | Forbidden without rewrite |
| AP Algorithm changes cardinality | Forbidden |
| AQ Estimated zero removes join | Forbidden |
| AR Same-text occurrences share state | Forbidden |
| AS Same occurrence initializes twice | Forbidden |
| AT Retry reuses state | Forbidden |
| AU Scalar skips second-row check | Forbidden |
| AV EXISTS loses early stop | Forbidden |
| AW Nullable IN rewritten as INNER | Forbidden absent proof |
| AX IN build skipped for cost | Forbidden |
| AY Semi/anti/marker lacks proof/capability | Excluded |
| AZ Correlated subquery costed | Forbidden; binder rejects |
| BA Different literals change estimates | Permitted |
| BB Attractive plan lacks slot | Excluded/rejected |
| BC Cheap plan lacks capability | Excluded |
| BD Identical inputs differ by iteration | Forbidden |
| BE Heuristic misses cheaper unvisited plan | Permitted if returned plan is legal |
| BF False final ordering bypasses validation | Forbidden |

## 53. §37.18 invariant assessment

| # | Owner | Fixture | Result |
|---:|---|---|---|
| 1 | §§37.1, 22.7 | attempt to add partitioning/rewindability | COMPLETE |
| 2 | §§37.2–37.3 | change each key dimension | COMPLETE |
| 3 | §37.5; Chs. 27–30 | false SeqScan/hash ordering | COMPLETE |
| 4 | §37.6; §38.3 | ordered alternative versus local cheapest | **GAP — N37-4 normalization** |
| 5 | §37.7; §19.2 | self-join | COMPLETE |
| 6 | §37.10 | selective `(A⋈B)⋈(C⋈D)` | COMPLETE, subject to N37-2 |
| 7 | §37.11 | 10/11 relation boundary | **GAP — N37-1 domains** |
| 8 | §37.13 | tied greedy search and bounded passes | **GAP — N37-1 validation** |
| 9 | §37.12 | connected/disconnected graphs | **GAP — N37-2** |
| 10 | §§37.14–37.15 | algorithm/orientation tradeoff | COMPLETE |
| 11 | §§37.9, 37.15 | LEFT boundary/orientation | COMPLETE |
| 12 | §37.16; Ch. 35 | same join across algorithms | COMPLETE |
| 13 | §37.14; §22.4.1 | unavailable merge join | COMPLETE |
| 14 | §37.17; §20.14 | correlation and lazy occurrences | COMPLETE |

## 54. Global contradiction search

Results:

- Property taxonomy: valid shared ownership between §§22.7, 37.1–37.5, and Chapter 38.
- Ordering rules: consistent with Chapters 27–30.
- Required slots: consistent with logical slots, DML hidden state, and final validation.
- `required_rows`: valid different object from ordering and exact K.
- `BindingId`: consistent across Chapters 19, 20, 37, and 38.
- Join cardinality: consistent with Chapter 35 and §38.7.
- Outer joins: consistent with §§20.8, 20.17.7, and Chapter 28.
- Capabilities: consistent with §22.4.1.
- Subqueries: consistent with §§19.18 and 20.14.
- Errors/resources: consistent with Chapter 39.

One Chapter-37-internal inconsistency was found: Cartesian alternatives are described as ordered after connected partitions in §37.10 but as absent when unnecessary in §§37.12–37.13.

No frozen-owner contradiction was found.

## 55. Existing Verification reuse inventory

Principal exact reusable procedures:

- V19-18
- V20-2, V20-3, V20-6, V20-7, V20-10, V20-12–V20-16, V20-19, V20-20, V20-22
- V22-B, V22-C, V22-D, V22-I, V22-J, V22-K, V22-L
- V27-E, V27-G, V27-I–V27-K, V27-N, V27-R
- V28-B, V28-E–V28-G, V28-M–V28-P, V28-T, V28-U
- V29-O, V29-P
- V30-D, V30-J–V30-M
- V31-A, V31-B, V31-E, V31-G, V31-H, V31-N
- V32-A–V32-C, V32-H–V32-J, V32-M, V32-N
- V33-A–V33-E, V33-G, V33-H, V33-J–V33-N
- V34-B–V34-D, V34-J–V34-L
- V35-A, V35-B, V35-E, V35-G–V35-J
- V36-A, V36-C, V36-H–V36-J

Reusable named sections:

- Subquery Tests
- Scan and Unary Operator Tests
- Access Path Tests
- Join-Order Tests
- Physical Property and Enforcement Tests
- Memory/Spill Plan Tests
- Memo and Pruning Tests
- Cost Model Tests
- Optimizer Determinism and Resource-Limit Tests
- Final Optimizer Validation Tests
- Optimizer Differential Correctness Tests
- Optimizer Diagnostics Tests

These are component oracles, not a substitute for future V37 integration.

## 56. Missing future V37 Verification inventory

A future synchronization needs deterministic procedures for:

- four-part ordering prefix;
- empty ordering;
- duplicate/normalized ordering vectors after N37-4;
- every supported operator’s provided ordering;
- interesting-order retention and dominance;
- required-slot closure, including hidden ORDER BY and DML slots;
- `required_rows` identity separation;
- self-join `BindingId` bits;
- relation sets above native word width;
- search-limit validation;
- multi-relation predicate activation and exactly-once attachment;
- outer-join region boundaries;
- bushy subset/partition completeness;
- symmetry elimination;
- Cartesian admission after N37-2;
- 9/10/11 threshold cases;
- planning-memory fallback;
- deterministic greedy and bounded passes;
- joint order/algorithm enumeration;
- INNER and LEFT hash orientations;
- algorithm-independent cardinality;
- subquery occurrence/attempt state;
- scalar two-row check;
- EXISTS first-row demand;
- IN complete build and 3VL;
- proof/capability-gated rewrites;
- correlation rejection before costing;
- final property/slot validation;
- all 14 invariants.

These are Verification gaps only; they do not add further Architecture findings.

## 57. Document-role assessment

Chapter 37 is mostly:

- timeless;
- implementation-independent;
- appropriately scoped;
- consistent with closed Chapters 31–36;
- proportionate to a from-scratch optimizer;
- integrated with, but not duplicative of, Chapter 38.

It introduces no new SQL semantics, comparator, proof source, memory-grant protocol, or mandatory optional algorithm.

One roadmap-style phrase remains at §37.13: “may be added later.” That is editorial, not semantic.

## 58–63. Findings

### N37-1 — MAJOR: incomplete search-configuration and capacity contract

Locations:

- §37.7, line 27421
- §37.11, lines 27492–27504
- §37.13, lines 27532 and 27541–27547

Missing contract:

- representation/domain for `exhaustive_join_limit`;
- whether zero is valid;
- rejection of negative/invalid representations;
- representation/domain for `large_join_max_local_passes`;
- whether zero passes is valid;
- definition of the referenced “configured large-join planning limit”;
- controlled outcome when a region exceeds representable relation-set capacity;
- validation before search.

Reproducer:

```text
exhaustive_join_limit = -1
large_join_max_local_passes = -1
65 BindingIds with an implementation using one uint64_t RelationSet
```

Observable consequence:

- wraparound or inconsistent exhaustive/heuristic selection;
- silent alias-bit truncation;
- implementation-specific query rejection;
- malformed configuration reaching search.

Smallest repair:

- §37.7, §37.11, §37.13, and invariants 7–8;
- define domains and validation;
- route invalid configuration to existing `OptimizerError`;
- preserve `OptimizerResourceLimit` for genuine bounded-planning exhaustion.

### N37-2 — MAJOR: Cartesian-admission scope is inconsistent

Locations:

- §37.10 line 27480
- §37.12 lines 27508–27512
- §37.13 line 27551
- invariant 9 line 27671

Missing/conflicting contract:

Whether exhaustive DP constructs disconnected subsets inside an otherwise connected join region.

Reproducer:

```text
A -- B -- C
```

Subset `{A,C}` has no crossing predicate, while the full region is connected. §37.10 can be read to enumerate its Cartesian partition after connected work; §§37.12–37.13 can be read to forbid it as unnecessary.

Observable consequence:

Two conforming implementations enumerate different subset states and bushy trees under the same inputs.

Smallest repair:

Define one component/subset-level rule in §§37.10–37.13:

- when disconnected subsets are omitted;
- when Cartesian states are necessary;
- how separate graph components are connected;
- whether “before” means priority or eventual inclusion.

### N37-3 — MAJOR: multi-relation predicate activation is undefined

Locations:

- §37.8 lines 27431–27440
- §37.10 lines 27469–27476
- downstream §38.7 line 27860

Missing contract:

For a predicate referencing relation set `R`, define when it is available and at which join node it is attached.

Reproducer:

```text
predicate references A, B, and C
current subset S = {A,B}
partition A | B
```

The predicate crosses the two present sides but still references unavailable `C`.

Observable consequence:

- premature evaluation with missing slots;
- predicate omission;
- duplicate evaluation;
- changed demand/error behavior;
- invalid or incomplete join alternatives.

Smallest repair:

In §§37.8 and 37.10 require:

```text
R ⊆ S
```

before activation, require both partition sides to be referenced for crossing placement, and define exactly-once assignment at a legal node while retaining residual demand semantics.

### N37-4 — MINOR: “normalized ordering” is undefined

Location:

- §37.6 lines 27390–27394

Missing contract:

No canonical treatment is given for duplicate or provably redundant order keys.

Reproducer:

```sql
ORDER BY x, x
```

versus available ordering `(x)`.

Observable consequence:

Different memo property classes and redundant Sort/enforcement decisions. SQL correctness survives final validation, so severity is MINOR.

Smallest repair:

State either:

- exact vectors are retained without duplicate normalization; or
- define canonical duplicate/redundant-key normalization and its demand-safety boundary.

### N37-5 — EDITORIAL: roadmap wording

Location:

- §37.13 line 27549

Current wording:

```text
A small deterministic beam may be added later...
```

Smallest repair:

Use timeless capability wording, for example:

```text
Deterministic beam search is an optional extension and is not required by the v1 baseline.
```

Counts:

- BLOCKING: **0**
- MAJOR: **3**
- MINOR: **1**
- EDITORIAL: **1**
- DESIGN-SCOPE QUESTION: **0**
- FROZEN SEMANTIC QUESTION: **0**

## 64–65. Required next action

Exact next Architecture action:

**Chapter-37 Architecture Fix A**, confined to:

- relation-set/search-configuration domains and validation;
- Cartesian-subset/component admission;
- multi-relation predicate activation and attachment;
- ordering normalization;
- timeless beam-search wording;
- corresponding §37.18 invariants.

The repair should not alter frozen Chapters 31–36 or introduce new SQL semantics or algorithms.

Recommended following task:

**CHAPTER 37 — INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT, POST-FIX-A**

Verification synchronization must wait until Architecture closes.

## 66–67. Repository checks

`git diff --check`:

```text
PASS
```

Final repository state:

```text
HEAD:   377372d8bdb68f7c50efd7a324b3e0f41fc707ef
Index:  clean
Tracked worktree: clean
Untracked pre-existing Chapter-37 review directory: preserved
Audit-created changes: NONE
```

No build, tests, sanitizer, benchmark, calibration, staging, or commit was performed.

```text
CHAPTER 31 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 31 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 32 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 32 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 33 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 33 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 34 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 34 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 35 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 35 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 36 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 36 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 37 ARCHITECTURE:
    NEEDS ARCHITECTURE FIX

CHAPTER 37 VERIFICATION:
    NOT SYNCHRONIZED

CHAPTER 38 REVIEW:
    NOT STARTED

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE
```

END CHAPTER-37 INITIAL READ-ONLY
ARCHITECTURE REVIEW.