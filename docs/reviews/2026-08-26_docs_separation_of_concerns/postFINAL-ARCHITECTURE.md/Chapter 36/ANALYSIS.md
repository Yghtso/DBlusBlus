# Chapter 36 Initial Architecture Review

## 1. Initial review verdict

**NEEDS ARCHITECTURE FIX**

Chapter 36 is coherent in its semantic boundaries and base-access intent, but it is not yet complete enough for two independent implementations to cost every required alternative without inventing policy.

Findings:

- BLOCKING: 0
- MAJOR: 3
- MINOR: 0
- EDITORIAL: 1
- DESIGN-SCOPE QUESTIONS: 0
- FROZEN SEMANTIC QUESTIONS: 0

## 2–4. Repository state

Initial and final state:

- HEAD: `a7d3c50a7f1756bf7c3c0d6be1d019fcf975718d`
- Commit: `a7d3c50 applied SYNC FIX-1 35 in ARCHITECTURE`
- Index: clean
- Pre-existing worktree entry:

  ```text
  ?? docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 36/
  ```

- Audit-created changes: **NONE**
- The pre-existing untracked Chapter-36 review directory was preserved and was not treated as semantic authority.

## 5–7. Live Chapter 36 scope

Title: **36. Cost Model and Base Access Paths**

Boundaries:

- Starts: [docs/ARCHITECTURE.md:26462](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26462)
- Ends: line 26961
- Chapter 37 starts: line 26963

Subsections:

1. §36.1 Cost philosophy
2. §36.2 Cost structure
3. §36.3 Cost units
4. §36.4 Calibration
5. §36.5 Cache model
6. §36.6 Sequential scan cost
7. §36.7 B+ point-lookup cost
8. §36.8 Index range-scan cost
9. §36.9 Index/heap correlation
10. §36.10 Fallback distinct heap-page estimate
11. §36.11 Access-predicate classification
12. §36.12 B+ sargability
13. §36.13 Composite bounds
14. §36.14 Residual predicates
15. §36.15 Base access alternatives
16. §36.16 One-index baseline
17. §36.17 Index/SeqScan break-even
18. §36.18 Required-column cost
19. §36.19 Base-access invariants

The four normative equations or formula families are:

```text
total_cost = startup_cost + run_cost

physical_tuple_versions
≈ analyzed_live_row_count + dead_version_estimate

candidate_inflation
= max(1, physical_entry_count / max(1, logical_live_entry_count))

physical_candidates
≈ min(physical_entry_count,
      logical_candidate_rows * candidate_inflation)
```

The distinct-heap-page occupancy calculation is intentionally left as a calibrated implementation choice, subject to stable clamping and the physical-page cap.

### §36.19 invariants

All 17 live invariants remain internally coherent:

1. Abstract cost, not promised milliseconds.
2. Inspectable components.
3. No momentary BufferPool-residency input.
4. SeqScan includes physical pages and tuple versions.
5. Index access retains heap MVCC checks.
6. Logical selectivity and physical index pressure stay distinct.
7. Range locality uses correlation or bounded fallback.
8. Access predicates are classified per index schema.
9. Leftmost equality prefix plus one range.
10. `IS NULL` may be searchable; `= NULL` is not.
11. Bound sentinels are transient.
12. Exact bounds may discharge represented predicates; partial predicates remain residual.
13. SeqScan and every usable single-index alternative are enumerated.
14. At most one index per relation occurrence.
15. Break-even emerges from cost.
16. Required-column work affects cost.
17. Numerical zero cannot suppress valid runtime access or prove absence.

## 8. Canonical owner matrix

| Subject | Canonical owner |
|---|---|
| SQL comparison, NULL, errors | Chapter 17 |
| Bound expressions and execution-start counts | Chapter 19 |
| Bag semantics, demand, exact proof | Chapter 20 |
| Physical capabilities, slots, validation | Chapter 22 |
| Runtime row representation and memory | Chapters 23–24 |
| Pipelines, scheduling, cancellation | Chapters 25–26 |
| Physical operator execution | Chapters 27–30 |
| DML publication and error closure | Chapter 31 |
| Parallel occurrence ownership | Chapter 32 |
| Stable planning inputs and legal selection | Chapter 33 |
| Statistics identity and field meanings | Chapter 34 |
| Cardinality, width, fallback and provenance | Chapter 35 |
| Base-access cost model | Chapter 36 |
| Properties and join enumeration | Chapter 37 |
| Join/operator costs, memo, objectives and spill | Chapter 38 |
| Error/resource categories | Chapter 39 |
| Diagnostics | Chapter 40 |
| Verification obligations | Chapter 41 |

No competing owner was introduced by Chapter 36.

## 9–12. Cost inputs, units, configuration and arithmetic

### Cost inputs

The live model consumes:

- Chapter-35 input/output row estimates;
- representation-specific row widths;
- required slots and projection-pruned payload;
- table live rows, physical heap pages and dead-version estimate;
- index physical/live/invisible entry counts;
- index leaf pages and occupancy;
- leading-key heap correlation;
- current immutable B+ tree height;
- predicate and expression workload;
- required/provided ordering;
- execution-memory budget and operator memory targets;
- calibrated page, CPU, hash, comparison and temporary-I/O weights;
- `effective_cache_pages`;
- startup/full-result objective metadata.

Most producers and identities are defined by Chapters 33–35. Missing physical table/index statistics are not fully covered; see N36-2.

### Cost-unit system

The model correctly uses normalized abstract work units, not milliseconds. Component counts are intended to be converted through configured coefficients before scalar comparison.

It correctly distinguishes:

- sequential and random persistent pages;
- temporary pages;
- tuple/vector CPU;
- expression CPU;
- hash and comparison work;
- memory and spill quantities.

However, §36.3 says the configuration contains weights “such as” the listed fields and does not provide a closed mandatory inventory, dimensional mapping, or validation contract. This is N36-1.

### Cost configuration

Explicit fields include:

```text
seq_page_cost
random_page_cost
cpu_tuple_cost
cpu_operator_cost
hash_cost
comparison_cost
temp_page_cost
effective_cache_pages
```

Missing normative details:

- complete mandatory field set;
- finite-domain requirements;
- negative-value rejection;
- whether zero is permitted for each field;
- missing-field behavior;
- units such as “abstract cost units per page/tuple/expression”;
- configuration validation boundary;
- fixed identity for the duration of one invocation;
- treatment of invalid calibrated data.

### Numeric safety

Chapter 38 requires all optimizer costs to be finite and nonnegative. Chapter 36 does not define how that invariant is achieved when:

- uint64 counters are added or multiplied;
- cardinality is already saturated;
- a coefficient is NaN, infinite or negative;
- repeated composition overflows;
- weighted sums overflow;
- intermediate arithmetic becomes nonfinite.

The final invariant is present, but the construction/finalization contract is absent. This is part of N36-1.

## 13–14. Startup, total and first-K objectives

The semantic distinction is sound:

```text
startup_cost = work before first output can become available
run_cost     = remaining work to exhaust the operator
total_cost   = startup_cost + run_cost
```

Chapter 38 correctly owns:

- full-result versus `FIRST_K_ROWS`;
- exact-K representability;
- partial-run fraction clamped to `[0,1]`;
- blocking boundaries;
- conservative propagation;
- the prohibition on deriving exact Top-N K from an estimate.

The gap is operational: neither Chapter 36 nor Chapter 38 supplies a complete generic rule for attaching local work and child work to startup/run for mandatory streaming operators. That gap is N36-3.

## 15–17. Estimate handoff, widths and stable statistics

Chapter 35 correctly supplies:

- finite row estimates;
- logical, stored and temporary width categories;
- predicate truth distributions;
- confidence and provenance;
- proof separation;
- unknown-count estimates.

Chapter 36 correctly keeps:

- stored-page I/O separate from projected output width;
- heap visibility work separate from SQL-visible output;
- required-column decode/materialization separate from page I/O.

It does not authorize representation substitution or treat width as allocation.

Per-object stability is preserved by Chapters 33–34:

- T1/S1 and T2/S2 may differ;
- later T1/S3 publication cannot alter the active invocation;
- aliases of one underlying table retain one descriptor generation;
- no global StatsVersion equality is required;
- no mixed generation members are permitted.

Chapter 36 contains no conflicting refresh or cache protocol.

## 18–24. Access paths and composition

### Base paths

The Chapter-36 baseline consists of:

- `PhysicalSeqScan`;
- every semantically usable `PhysicalIndexScan`;
- at most one B+ index per base relation occurrence.

Bitmap intersection, union and general index merge are outside the baseline.

### Sequential scan

The intended components are sound:

- physical heap pages;
- live plus dead tuple-version inspection;
- pushed predicate work on examined visible rows;
- required-column decode/materialization on produced rows.

The formula properly distinguishes physical work from output cardinality.

The unresolved case is an unanalyzed table with no `physical_heap_pages` or `dead_version_estimate`. Chapter 35 provides logical row and width fallbacks, but not these physical inputs. This is N36-2.

### Index access

The model correctly distinguishes:

- tree descent;
- leaf work;
- physical candidates;
- heap RID access;
- visibility checks;
- required decoding;
- residual predicates;
- physical garbage pressure;
- locality.

Physical index-pressure statistics remain advisory and cannot prove key absence.

### Covering/index-only scope

Index-only scans are expressly outside the baseline. Even a unique point lookup performs a heap visit for visibility. Required slots affect decoding, but covering payload does not waive MVCC.

### Index versus SeqScan fallback

A legal index is not mandatory. SeqScan remains the baseline fallback. Missing index statistics must not make valid SQL unplannable, but the finite physical-work assumptions for that case are not defined—N36-2.

### Predicate and expression work

Pushed SeqScan predicate work has an explicit live-row basis. Index residual work is named but its evaluated-row basis is not specified. General `PhysicalFilter`, `PhysicalProject`, `PhysicalValues` and `PhysicalLimit` local-cost transfers are also absent.

This can cause one implementation to charge an expensive residual predicate per candidate while another charges only final output rows. That is N36-3.

### Parent-child composition

“Additive where semantically appropriate” is insufficient as a complete composition contract. It does not settle:

- exactly-once child inclusion;
- local versus child startup;
- streaming unary composition;
- residual-expression workload;
- partial-consumption interaction;
- when local work belongs to startup or run.

Chapter 38 resolves several blocking operators individually, but not the general gap. This is N36-3.

## 25–32. Physical operator costing

### Nested-loop joins

Owned by §38.10 and §38.11:

- materialized NLJ includes outer, inner/materialization, candidate-pair CPU and output CPU;
- INLJ charges lookup/range, heap/MVCC and residual work per outer row;
- repeated-key locality reductions must be bounded.

No Chapter-36 contradiction was found.

### Hash joins

Owned by §§38.8–38.9 and §38.18:

- pruned temporary build width;
- setup startup;
- hash/build/probe/output work;
- duplicate metadata;
- memory target;
- spill passes;
- LEFT orientation restrictions.

A predicted fit is not an allocation guarantee.

### Merge joins

Capability-conditional under Chapters 22, 37 and 38:

- compatible ordering required;
- missing order incurs Sort;
- existing order avoids Sort;
- only guaranteed output ordering may be advertised.

### Aggregation

Owned by §38.14 with semantic cardinality from Chapters 29 and 35. Global/grouped empty behavior remains unchanged. Optional sorted aggregation is capability-gated.

### Sort and Top-N

Owned by §§38.13, 38.15–38.16:

- full-sort comparison and memory work;
- spill runs and merge work;
- exact `K = LIMIT + OFFSET`;
- Top-N only when exact K is representable;
- Sort/Limit fallback otherwise;
- pruning affects sort payload width.

### Parallel costing

No general parallel cost model is part of the Chapter-36 baseline. Chapter 42 explicitly excludes detailed probabilistic parallel-memory/cost modeling. Parallel execution semantics remain Chapter 32-owned.

Classification: **OUTSIDE BASELINE / CAPABILITY-CONDITIONAL**, not a defect.

## 33–40. Resources, properties, statistics, proof and determinism

### Resource and spill estimates

Chapters 24 and 38 correctly separate:

- estimated required memory;
- assigned planner target;
- QueryMemoryManager grant;
- physical allocation;
- spill;
- runtime failure.

Peak memory follows simultaneous lifetime phases, not blind summation.

### Spill

Legal spill-capable plans remain eligible even when expensive. Predicted spill affects cost, not SQL validity. Runtime spill correctness and cleanup remain execution-owned.

### Properties and slots

Low cost cannot authorize:

- missing output slots;
- false ordering;
- illegal join orientation;
- unavailable algorithms;
- Top-N without exact K.

Final validation remains mandatory.

### Comparator, dominance and ties

Chapter 38 owns:

- full-result and first-K objectives;
- dominance;
- relative tie epsilon;
- canonical structural tie-break;
- bounded search;
- no global-optimum promise.

Chapter 36 does not conflict with these rules.

### Missing/rejected/stale statistics

Selection ordering is sound:

1. newest compatible complete valid generation;
2. older compatible complete valid generation;
3. missing-statistics fallback.

Stale valid metadata remains usable. Malformed outer catalog framing remains corruption. The remaining defect is the missing physical-work fallback in N36-2.

### Estimate versus proof

Chapter 36 correctly says numerical zero cannot:

- eliminate scans or joins;
- suppress MVCC;
- prove uniqueness or absence;
- remove demanded errors;
- create exact LIMIT K.

No proof-authority regression was found.

### Error/resource ownership

| Condition | Owner/outcome |
|---|---|
| Missing/rejected advisory statistics | Chapters 34–36 fallback |
| Malformed catalog framing | Chapters 16/34 corruption |
| Invalid cost configuration | Insufficiently specified: N36-1 |
| Nonfinite cost | Must not escape; construction outcome insufficiently specified: N36-1 |
| Unsupported capability | Excluded before costing |
| Invalid selected plan | Internal validation failure |
| Planning-resource exhaustion | `OptimizerResourceLimit` after bounded fallback fails |
| Runtime allocation/spill failure | Chapters 24/39 |
| Demanded SQL error | Chapters 17/20/25/39 |

No new `CostModelError` is warranted.

### Determinism and calibration

Different valid statistics samples or calibrated configurations may produce different costs and plans. Fixed planning inputs and configuration must follow deterministic search and tie rules.

No absolute latency-accuracy promise is required.

## 41–45. Frozen-chapter regressions

- **Chapter 31:** No regression. Cost choices cannot alter pre-W closure, error reduction, mutation ownership, W/C/R, retries or result ownership.
- **Chapter 32:** No regression. Cost cannot redefine source occurrences, claims, replay, cancellation or legal early stop.
- **Chapter 33:** No regression. Chapter 36 does not introduce another optimizer or global-optimum requirement.
- **Chapter 34:** No regression. Statistics remain per-object, complete, immutable and advisory.
- **Chapter 35:** No regression. Costing consumes, rather than privately recomputes, cardinality/width estimates and cannot manufacture proof.

## 46–47. Document role and complexity

Chapter 36 is largely timeless and implementation-independent.

Complexity classification:

- **CORE:** cost structure, units, base scan/index work, access predicates, bounds, alternatives and invariants.
- **JUSTIFIED ADVANCED:** cache approximation, index/heap correlation and occupancy-based distinct-page estimation.
- **POSSIBLE OVERENGINEERING:** none identified.

Editorial chronology remains in two passages:

- “The initial cost model…” at line 26715.
- “unless a future expression-index architecture…” at line 26800.

These should use timeless baseline/optional-extension wording.

## 48. Adversarial case matrix

| Case | Owner and required/permitted outcome | Chapter-36 result |
|---|---|---|
| A | Ch34–36: missing stats must retain legal SeqScan | **Insufficient: N36-2** |
| B | Ch34: use older compatible valid generation | Sufficient |
| C | Ch34: stale valid input remains advisory | Sufficient |
| D | Ch33–34: different tables may use different versions | Sufficient |
| E | Ch33–34: aliases share retained underlying descriptor | Sufficient |
| F | Ch35/36: zero estimate cannot suppress runtime match | Sufficient |
| G | Ch20/35: demanded error cannot be eliminated | Sufficient |
| H | §36.6: many pages and few outputs still costs all scan pages | Sufficient when physical input exists |
| I | §36.6: dead pressure raises scan CPU | Sufficient |
| J | Physical heap-page input missing | **Insufficient: N36-2** |
| K | Missing index physical statistics | **Insufficient: N36-2** |
| L | §36.17: scattered index fetches may lose to SeqScan | Sufficient |
| M | §§36.7–36.8: duplicate keys increase physical candidates | Sufficient with inputs |
| N | Ch22/37: missing slot makes path ineligible; no index-only baseline | Sufficient |
| O | Ch37/38: valid index order may avoid Sort | Sufficient |
| P | Ch22/34/38: incompatible retained index rejected | Sufficient |
| Q | Predicate charged only on survivors | **Insufficient for residual/general filters: N36-3** |
| R | Parent omits child work | **Insufficient: N36-3** |
| S | Parent charges child twice | **Insufficient: N36-3** |
| T | §38.10–38.11: repeated NLJ/INLJ work charged by actual algorithm | Sufficient |
| U | §38.9/38.18: hash memory uses pruned temporary width | Sufficient |
| V | Ch24: runtime allocation may fail despite estimated fit | Sufficient |
| W | §38.20: expensive legal spill path remains legal | Sufficient |
| X | Ch37/38: missing required merge order adds Sort | Sufficient |
| Y | Existing compatible order avoids Sort | Sufficient |
| Z | Ch22/37/38: false ordering rejected | Sufficient |
| AA | Ch29/35: global empty aggregate yields one row | Sufficient |
| AB | Ch29/35: grouped empty aggregate yields zero | Sufficient |
| AC | §38.14: few groups reduce aggregate state | Sufficient |
| AD | Ch35 finite group estimate feeds cost | Sufficient |
| AE | Estimated-zero Sort input is not proof | Sufficient |
| AF | §38.18: pruning narrows Sort payload | Sufficient |
| AG | §38.13/20: constrained memory adds spill | Sufficient |
| AH | Ch20/35: actual LIMIT 0 may carry approved proof | Sufficient |
| AI | Ch19/35/38: unknown LIMIT remains estimate, not exact K | Sufficient |
| AJ | Unknown OFFSET is not known zero | Sufficient |
| AK | Ch22/30/38: estimated K cannot enable Top-N | Sufficient |
| AL | Unrepresentable exact K uses Sort/Limit fallback | Sufficient |
| AM | Required order defeats cheaper unordered alternative | Sufficient |
| AN | Ch38: first-row and total objectives may prefer different legal plans | Semantics sufficient; local costing affected by N36-3 |
| AO | Ch38: blockers cannot claim immediate output | Sufficient |
| AP | Ch38: near/equal costs use deterministic tie rule | Sufficient |
| AQ | Ch22/38: cheaper plan missing slots is illegal | Sufficient |
| AR | Ch22: unavailable capability never enters final plan | Sufficient |
| AS | Ch32/42: logical occurrence unchanged; detailed parallel costing outside baseline | Capability-conditional |
| AT | Ch24/38: per-worker and global memory cannot be conflated | Sufficient |
| AU | Overflow before saturation/finalization | **Insufficient: N36-1** |
| AV | NaN or infinity in cost | **Insufficient construction contract: N36-1** |
| AW | Negative coefficient | **Domain undefined: N36-1** |
| AX | Zero coefficient | **Per-field policy undefined: N36-1** |
| AY | Bytes added directly to page cost | **Dimensional mapping incomplete: N36-1** |
| AZ | Ch24: estimate is not allocation success | Sufficient |
| BA | Ch38/39: bounded fallback, then `OptimizerResourceLimit` | Sufficient |
| BB | Ch20/38: cheap plan cannot violate demanded error | Sufficient |
| BC | Ch34/36: different valid samples may produce different costs | Permitted |
| BD | Ch38: container/evaluation nondeterminism cannot select plan | Sufficient at search level; arithmetic finalization needs N36-1 |
| BE | Ch33/37/38: bounded search may leave cheaper plan unexplored | Permitted |
| BF | Ch22/38: illegal property fails validation despite low cost | Sufficient |

## 49. Global contradiction search

No true cross-chapter contradiction was found.

Classifications:

- **VALID DIFFERENT OWNER:** join/aggregate/sort/Top-N costs in Chapter 38; allocation in Chapter 24; diagnostics in Chapter 40.
- **VALID DIFFERENT STAGE:** logical cardinality in Chapter 35 versus physical work in Chapters 36/38.
- **VALID DIFFERENT COST OBJECTIVE:** full-result versus `FIRST_K_ROWS`.
- **VALID DIFFERENT STATISTICS GENERATION:** different tables may retain different StatsVersions.
- **VALID APPROXIMATION:** correlation interpolation, occupancy estimate and calibrated weights.
- **VALID OPTIONAL CAPABILITY:** MergeJoin, SortAggregate, ordered DISTINCT and parallel costing.
- **NON-NORMATIVE:** Chapter-42 benchmark and calibration examples.
- **TRUE CONTRADICTION:** none.

The §38.22 reference to §35.25 does not solve missing physical heap/index inputs because §35.25 covers estimator rows, widths and predicate/count fallbacks, not Chapter-36 physical-work counters. This is the omission recorded as N36-2.

## 50. Existing Verification reuse inventory

Exact reusable families/procedures include:

- V20-15, V20-16, V20-19
- V22-D, V22-F, V22-I, V22-J, V22-K, V22-L
- V24-A, V24-C, V24-D, V24-H through V24-N as applicable
- V27-B through V27-K, V27-N, V27-P, V27-R
- V28-E through V28-T as algorithm-applicable
- V29-B, V29-N through V29-Q
- V30-D through V30-K
- V31-A, V31-B, V31-G, V31-N
- V32-B, V32-C, V32-H through V32-M
- V33-A, V33-C through V33-I, V33-K, V33-M, V33-N
- V34-B, V34-C, V34-F, V34-G, V34-J, V34-K
- V35-A through V35-J, especially V35-001–005, 013–021, 047–055, 056–061 and 066–068

Reusable named sections:

- Access Path Tests
- Physical Property and Enforcement Tests
- Memory/Spill Plan Tests
- Memo and Pruning Tests
- Cost Model Tests
- Optimizer Determinism and Resource-Limit Tests
- Final Optimizer Validation Tests
- Optimizer Differential Correctness Tests
- Optimizer Diagnostics Tests
- Cost Model Benchmarks

## 51. Missing Chapter-36 Verification inventory

A future V36 synchronization will need deterministic integration procedures for:

- complete CostConfig field inventory and identity;
- coefficient units and validation;
- zero/negative/NaN/infinite coefficient cases;
- finite weighted-sum arithmetic and saturation/failure;
- `effective_cache_pages` validation;
- missing physical table/index input fallback;
- valid-old versus stale versus missing physical metadata;
- SeqScan physical-page/dead-version accounting;
- point/range traversal, candidate and heap-fetch separation;
- correlation and occupancy fallback boundaries;
- predicate classification and composite bounds;
- residual predicate evaluation-row basis;
- general child/local cost composition;
- startup/run classification for streaming operators;
- no omitted or double-counted child work;
- first-K versus full-result base-access cost;
- projection-pruned decode/materialization;
- stable per-object statistics during costing;
- zero estimate versus exact proof;
- dimensional handoff to downstream operator costs;
- deterministic ties and final validation.

These are Verification gaps, not substitutes for repairing N36-1 through N36-3.

## 52–57. Findings

### N36-1 — MAJOR

- Location: §§36.1–36.5, especially lines 26481 and 26522–26573.
- Owner: Chapter 36, with the finite-comparison invariant consumed by §38.4.
- Defect: CostConfig is not a closed, dimensioned, validated numeric contract, and cost arithmetic has no total finite-finalization rule.
- Reproducer: negative or NaN `random_page_cost`; missing hash coefficient; zero coefficient of unspecified legality; huge weighted sums.
- Consequence: invalid or non-comparable costs can enter selection, or implementations invent different validation and conversion policy.
- Smallest repair: define the mandatory configuration categories, units, legal finite domains including zero policy, one-time invocation validation, and checked finite cost composition/finalization. Do not introduce a new public error enum.

### N36-2 — MAJOR

- Location: §§36.6–36.10 and §36.15, lines 26575–26739 and 26871–26894.
- Owner: Chapter 36, after Chapter-34 generation selection and Chapter-35 logical fallback.
- Defect: No finite fallback or stable derivation is specified for absent physical heap-page/dead-version and index-pressure/occupancy inputs.
- Reproducer: valid unanalyzed table with no compatible TableStatistics; legal index with no IndexStatistics.
- Consequence: mandatory SeqScan/IndexScan alternatives cannot be costed without invented numbers, despite valid SQL and mandatory path availability.
- Smallest repair: define precedence and finite configurable or stable-descriptor derivations for required physical cost inputs, with missing/stale provenance and no semantic-proof authority.

### N36-3 — MAJOR

- Location: §36.2, §§36.6–36.8, §36.14 and §36.18; lines 26510–26520, 26586–26600, 26636–26648, 26846–26869 and 26926–26939.
- Owner: Chapter 36 cost composition, consumed by Chapter 38 objectives.
- Defect: No complete local-plus-child transfer exists for mandatory streaming operators, and residual/general expression work lacks a canonical evaluation-row basis and startup/run allocation.
- Reproducer: expensive residual index predicate; Filter/Project above alternate children; parent omits or double-counts child; first-row objective over a streaming scan.
- Consequence: conforming implementations may materially disagree by omitting, duplicating or charging work to output rather than examined rows.
- Smallest repair: add compact operator-local composition rules: child objective included exactly once, predicate work charged per actual estimated evaluation domain, projection per produced row/expression, local startup/run placement based on blocking/streaming contract, and zero estimates remain non-proof.

### N36-4 — EDITORIAL

- Locations:
  - §36.9 line 26715: “The initial cost model…”
  - §36.12 line 26800: “unless a future expression-index architecture…”
- Owner: Chapter 36 document role.
- Defect: roadmap wording in timeless Architecture.
- Smallest repair: replace with “the v1 baseline model” and “unless an optional expression-index capability is defined/enabled,” preserving scope and formulas.

Counts:

- BLOCKING: **0**
- MAJOR: **3**
- MINOR: **0**
- EDITORIAL: **1**
- DESIGN-SCOPE QUESTIONS: **0**
- FROZEN SEMANTIC QUESTIONS: **0**

## 58–59. Required next action

Exact next Architecture action:

> One minimal Chapter-36-only Architecture Fix A addressing N36-1 through N36-4.

It should:

1. totalize and validate cost configuration/arithmetic;
2. complete physical-statistics fallbacks;
3. define local/child/startup/run and expression-work composition;
4. remove the two roadmap phrases.

Afterward, require an independent read-only Chapter-36 Architecture closure audit.

Do not synchronize Verification until that closure audit passes. Do not begin Chapter 37.

## 60. Documentation check

`git diff --check`: **PASS** — no output.

No build, tests, sanitizers or benchmarks were run.

## 61. Final repository confirmation

- Final HEAD unchanged: `a7d3c50a7f1756bf7c3c0d6be1d019fcf975718d`
- Index remains clean.
- Worktree remains exactly as initially found, including the preserved pre-existing untracked Chapter-36 review directory.
- Audit-created changes: **NONE**

CHAPTER 31 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 31 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 32 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 32 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 33 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 33 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 34 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 34 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 35 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 35 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;CLOSED / UNMODIFIED

CHAPTER 36 ARCHITECTURE:
&nbsp;&nbsp;&nbsp;&nbsp;NEEDS ARCHITECTURE FIX

CHAPTER 36 VERIFICATION:
&nbsp;&nbsp;&nbsp;&nbsp;NOT SYNCHRONIZED

CHAPTER 37 REVIEW:
&nbsp;&nbsp;&nbsp;&nbsp;NOT STARTED

IMPLEMENTATION:
&nbsp;&nbsp;&nbsp;&nbsp;NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
&nbsp;&nbsp;&nbsp;&nbsp;NOT RUN

AUDIT-CREATED CHANGES:
&nbsp;&nbsp;&nbsp;&nbsp;NONE

END CHAPTER-36 INITIAL READ-ONLY ARCHITECTURE REVIEW.