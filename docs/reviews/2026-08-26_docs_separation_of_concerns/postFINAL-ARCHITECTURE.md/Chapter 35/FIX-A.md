# Chapter 35 Architecture Fix A Report

## 1–3. Repository state and files

| Item | Initial | Final |
|---|---|---|
| HEAD | `40e028a82f3affcd59ce71e3b5d2020166c3d453` | unchanged |
| Commit | `40e028a chapter 35 ARCHITECTURE analysis` | unchanged |
| Worktree | clean | `M docs/ARCHITECTURE.md` |
| Index | clean | clean |

Files modified:

- [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

The live HEAD differed from the earlier review report because the Chapter-35 analysis was committed externally before this repair. No pre-existing worktree changes were present.

## 4. Exact Chapter-35 sections changed

- §35.7 — zero-domain equijoin arithmetic and timeless semi/anti wording
- §35.8 — zero residual-domain MCV guard
- §35.9 — timeless foreign-key extension wording
- §35.10 — timeless baseline interpolation wording
- §35.17 — timeless optional extended-statistics wording
- New §35.17.1 — structural and join cardinality-transfer table
- §35.20 — planning-time-unknown LIMIT/OFFSET estimation
- §35.25 — complete centralized fallback contract
- §35.27 — three supporting invariants, now 23 total

All changed hunks are inside Chapter 35. Chapter 36 content was not modified.

# N35-1 — Zero-domain equijoin

## 5–10. Defect and final rule

Original defect: §35.7 divided by `max(NDV_A, NDV_B)`, which is zero when both valid non-NULL domains have NDV zero.

The repaired rule at [§35.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25799) is:

```text
if nonnull_fraction_A == 0
   or nonnull_fraction_B == 0
   or NDV_A == 0
   or NDV_B == 0:
    join_rows = 0
else:
    join_rows =
        rows_A
        * rows_B
        * nonnull_fraction_A
        * nonnull_fraction_B
        /
        max(NDV_A, NDV_B)
```

Consequences:

- Two all-NULL inputs estimate zero TRUE matches without division.
- A one-sided zero non-NULL domain also estimates zero TRUE matches.
- UNKNOWN remains:

  ```text
  1 - (1 - nA) * (1 - nB)
  ```

- FALSE remains the residual probability.
- Estimated zero carries no semantic-emptiness proof.
- Invalid payloads still fail Chapter-34 validation.
- MCV contribution remains separate and counted once.
- A zero residual mass or residual NDV contributes zero without division.
- Unique-key eligibility, duplicate multiplicity and runtime visibility remain unchanged.

## 11. N35-1 scenario matrix

| Case | Numeric/truth result | Proof status | Arithmetic |
|---|---|---|---|
| A. Both children exactly proven empty | Cardinality zero | Existing child proof propagates | Finite |
| B. Nonempty, both all NULL | TRUE 0, UNKNOWN 1, FALSE 0 | No statistical proof | No division |
| C. Left zero non-NULL domain | TRUE 0; UNKNOWN by NULL formula | No statistical proof | No division |
| D. Right zero non-NULL domain | Same | No statistical proof | No division |
| E. Both NDV 1 | Existing formula with denominator 1 | Estimate only | Finite/saturated |
| F. Repeated equal keys | Full pair multiplicity retained; MCV may refine | Estimate only | Finite/saturated |
| G. Partial NULL fractions | TRUE bounded by joint non-NULL mass; UNKNOWN exact formula | Estimate only | Finite |
| H. MCV consumes all non-NULL mass | MCV contribution once; residual zero | Estimate only | No residual division |
| I. Zero non-MCV residual | Residual contributes zero | Estimate only | No division |
| J. Stale zero domain, runtime match | Estimate may remain zero; join executes | No proof | Finite |

**N35-1: CLOSED**

# N35-2 — Fallback completeness

## 12–18. Final fallback contract

Original defect: §35.25 did not explicitly cover missing base cardinality, applicable widths or predicates lacking a specialized estimator.

The centralized `EstimatorFallbackConfig` now includes:

- unknown base-relation row count;
- unknown logical/value width in bytes;
- unknown stored-tuple width in bytes;
- unknown operator-appropriate temporary row width in bytes;
- unknown equality selectivity;
- unknown range selectivity;
- unknown NULL fraction;
- generic NDV;
- generic complete TRUE/FALSE/UNKNOWN predicate distribution;
- unknown execution-start OFFSET effect;
- unknown execution-start LIMIT effect.

Validation domains:

| Input | Required domain |
|---|---|
| Base rows | Finite, nonnegative row estimate |
| Widths | Finite, nonnegative bytes in the specific representation |
| Selectivity | `[0,1]` |
| Predicate fallback | Complete finite normalized 3VL triple |
| Unknown OFFSET effect | `[0, rows_in]` |
| Unknown LIMIT effect | `[0, rows_after_offset]` |

Known fixed-width and derivable layout information takes precedence over configured width fallback. Logical, stored and temporary widths are not interchangeable, and no width estimate is a memory reservation.

Generic predicate fallback applies exact Chapter-17 NULL/operator behavior, resolved nullability and trusted constraints first. Configuration supplies only the statistically unresolved part and cannot collapse UNKNOWN into FALSE.

Provenance rules:

- Statistics-driven fallback is LOW confidence with `MISSING_STATISTICS`.
- A stale but valid selected generation retains `STALE_STATISTICS`.
- Generic-model assumptions retain material input provenance and identify the configured generic model diagnostically.
- Unknown execution-start counts are not falsely tagged as missing statistics.
- Malformed outer catalog framing retains its stronger corruption owner.
- Composite estimates retain the least-confident material assumption.

## 19. N35-2 scenario matrix

| Case | Required result |
|---|---|
| A. No TABLE statistics | Configured finite base-row and applicable width fallback; LOW/MISSING |
| B. Required member absent | Reject incomplete generation; select valid older generation or missing fallback |
| C. Malformed newest, valid older | Use older compatible generation; do not fabricate missing precision |
| D. No compatible generation | Centralized missing-statistics fallbacks |
| E. Row count exists, width unavailable | Representation-specific finite byte fallback |
| F. No specialized predicate model | Complete generic 3VL fallback |
| G. NULL behavior known, TRUE selectivity unknown | Preserve exact UNKNOWN behavior; estimate unresolved mass only |
| H. Stale zero-row statistic | May remain numerical zero with STALE provenance; no proof |
| I. Index statistics absent | SeqScan remains legal; missing physical inputs use their canonical cost fallback |
| J. Mixed collected/fallback inputs | Composite carries the least-confident material assumption and provenance chain |

No case requires synchronous ANALYZE or permits rejection of otherwise valid SQL merely because advisory statistics are absent.

**N35-2: CLOSED**

# N35-3 — Unknown LIMIT/OFFSET

## 20–24. Final unknown-count rule

Original defect: §35.20 covered only count values known during planning.

The repaired [§35.20](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26177) now:

- applies exact OFFSET-then-LIMIT arithmetic when values are known;
- uses named centralized assumptions for residual execution-start expressions;
- bounds unknown OFFSET output to `[0, rows_in]`;
- bounds unknown LIMIT output to `[0, rows_after_offset]`;
- applies unknown steps in semantic OFFSET-then-LIMIT order;
- marks them LOW-confidence configured fallback;
- does not evaluate the residual expression early;
- does not assume unknown LIMIT is zero or unknown OFFSET exhausts positive input;
- permits numerical zero to propagate from an independently zero child estimate without creating proof.

Runtime count acquisition, NULL/negative validation and expression errors remain owned by Chapter 19.

An approximate count is not exact mathematical `K`. `PhysicalTopN` remains ineligible unless exact `K` is available and representable. Full Sort plus the canonical exact alternative remains available.

## 25. N35-3 scenario matrix

| Case | Estimate/provenance | Runtime and Top-N |
|---|---|---|
| A. Known LIMIT 0 | Exact zero with `SQL_LIMIT_ZERO` proof | Top-N unnecessary; exact Limit semantics |
| B. Known positive LIMIT | Exact `min(after_offset, limit)` transfer | Exact K may be eligible if representable |
| C. Known positive OFFSET | Exact `max(0, rows-offset)` | Existing runtime semantics |
| D. Unknown LIMIT | Positive bounded configured estimate for positive input; LOW | No exact K |
| E. Unknown OFFSET | Positive bounded configured estimate for positive input; LOW | Unknown offset not treated as zero |
| F. Both unknown | Apply configured OFFSET then LIMIT effects | No exact K |
| G. Runtime value becomes zero | Planning fallback was not proof; runtime Limit emits zero | Runtime owner unchanged |
| H. Runtime invalid/error | Estimate does not suppress acquisition or error | Chapter-19 error |
| I. Exact K unrepresentable | Estimate may still exist | Top-N ineligible; exact alternative retained |
| J. Child estimate is zero without proof | Zero may propagate numerically | No `LIMIT 0` proof or unsafe early stop |

**N35-3: CLOSED**

# N35-4 — Operator transfer coverage

## 26–30. Final transfer rules

New [§35.17.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26142) defines:

| Logical form | Transfer |
|---|---|
| `LogicalValues` | Exact structural number of declared row occurrences; duplicates retained |
| No-FROM source | Exactly one zero-column source occurrence |
| `LogicalSort` | Preserve child cardinality estimate |
| `LogicalJoin CROSS` | Saturating `rows_left * rows_right` |
| Generic `LogicalJoin INNER` | Saturating `rows_left * rows_right * predicate.true_fraction` |

All multiplication uses §35.1’s finite saturation.

Structural cardinalities remain distinct from semantic execution outcomes:

- zero-row Values carries the approved structural proof;
- positive Values counts do not suppress demanded row-expression errors;
- no-FROM is not a table or missing-statistics fixture;
- Sort preserves the child’s proof state;
- estimated-zero CROSS/INNER output is not proof unless §20.17.10 independently provides it.

Specialized equijoin/MCV/unique-key models take precedence over the generic INNER transfer. Generic predicate fallback is used only where no applicable specialized model exists.

## 31. N35-4 scenario matrix

| Case | Required result |
|---|---|
| A. Zero-row Values | Structural zero plus approved exact-empty proof |
| B. Duplicate Values rows | Each declaration counts as an occurrence |
| C. Erroring Values expression | Positive structural estimate does not suppress demanded evaluation/error |
| D. No-FROM constant | One source occurrence; expression still demanded |
| E. Sort over nonempty input | Child cardinality preserved |
| F. Sort over estimated zero | Numerical zero preserved; proof state unchanged |
| G. Nonempty CROSS JOIN | Saturating child-product estimate |
| H. CROSS with estimated-zero child | Numerical zero; no proof unless child independently proven empty |
| I. CROSS product exceeds bound | Saturate to §35.1 finite maximum |
| J. Generic INNER TRUE fraction zero | Numerical zero, UNKNOWN/FALSE kept distinct, no automatic proof |
| K. Generic INNER with UNKNOWN | Only TRUE contributes matches |
| L. Missing predicate model | Complete configured 3VL fallback |
| M. Specialized equijoin available | Specialized §§35.7–35.9 model wins |

**N35-4: CLOSED**

# N35-5 — Timeless wording

## 32–33. Phrase-by-phrase cleanup

| Live area | Previous wording | Final wording |
|---|---|---|
| §35.7 | “future semi/anti join implementation” | Semi/anti extensions are outside the v1 baseline |
| §35.9 | “future-compatible” foreign-key refinement | Foreign-key refinements are outside the v1 trusted-constraint surface |
| §35.10 | “initial within-bin interpolation” | “baseline within-bin interpolation” |
| §35.17 | “Future extended statistics may include” | Optional extended-statistics capabilities outside the v1 baseline |

“Later committed changes may create overlap” was preserved because it describes real runtime/statistics staleness, not project chronology.

**N35-5: CLOSED**

# 34. Global numerical-hazard search

| Area | Result |
|---|---|
| Residual equality denominator | Already guarded by `max(1, …)` |
| Baseline equijoin denominator | Repaired with explicit zero-domain branch |
| MCV residual denominator | Repaired; zero mass/NDV contributes zero |
| Range and IN fractions | Bounded and finalized under §§35.4–35.5 |
| AND/OR arithmetic | Complete 3VL formulas with normalization |
| LIMIT/OFFSET subtraction | Protected by `max(0, …)` and bounded unknown effects |
| CROSS/INNER products | §35.1 finite saturation |
| Multi-column NDV powers | Inputs are validated nonnegative; result capped by input rows |
| LEFT JOIN estimate | Existing preserved-side lower bound unchanged |
| Width fallback | Finite nonnegative bytes in explicit representation |
| Nonfinite intermediates | Cannot escape estimator finalization or final plan validation |

No additional Chapter-35 numerical defect was found.

# 35–37. Semantic regression checks

### SQL 3VL

Unchanged:

- complete TRUE/FALSE/UNKNOWN triples;
- filter TRUE-only selection;
- strict NULL comparisons;
- IS NULL/IS NOT NULL behavior;
- IN/NOT IN NULL semantics;
- NOT/AND/OR formulas;
- same-column intersections;
- correlation uncertainty.

The generic fallback is explicitly not two-valued.

### Exact proof and demand

Unchanged:

- §35.2’s closed proof whitelist;
- §20.17.10 propagation;
- no proof from statistics or fallback;
- no demanded-expression suppression;
- exact-at-ANALYZE data remain advisory;
- composed zero/one estimates remain non-proof.

### Join, aggregate and width

Unchanged:

- duplicate join multiplicity;
- NULL equality nonmatches;
- LEFT JOIN lower bound;
- global aggregate one-row behavior;
- grouped-empty behavior;
- multi-column NDV damping;
- projection width recomputation;
- required-slot join width;
- width/memory-reservation distinction.

# 38–41. Frozen-chapter regressions

- Chapter 31: ordinary candidate closure, error reduction, W/C/R, mutation and result ownership unchanged.
- Chapter 32: source occurrences, claims, replay, early stop and worker-independent semantics unchanged.
- Chapter 33: legal bounded cost selection, fallback and final validation unchanged.
- Chapter 34: complete per-object generations, independent table versions, validation, selection and advisory-only statistics unchanged.

No frozen chapter was edited or reopened.

# 42. Global contradiction search

No true contradiction was found.

| Apparent interaction | Classification |
|---|---|
| Zero-NDV/all-NULL representation versus join estimate | VALID DIFFERENT STAGE; now total |
| Values structural count versus runtime expression error | VALID DIFFERENT OWNER |
| CROSS product estimate versus exact bag multiplicity | VALID DIFFERENT STAGE |
| Missing statistics versus stale valid statistics | VALID DIFFERENT INPUT GENERATION |
| Generic fallback versus specialized estimator | VALID APPROXIMATION with explicit precedence |
| Unknown count estimate versus exact runtime count | VALID DIFFERENT STAGE |
| Approximate output count versus Top-N K | VALID DIFFERENT OWNER |
| Finite estimate saturation versus exact SQL cardinality | VALID APPROXIMATION |
| Optional semi/anti/extended statistics | VALID OPTIONAL CAPABILITY |
| Width estimate versus runtime allocation | VALID DIFFERENT STAGE |

# 43–44. Document role and semantic questions

Chapter 35 remains:

- timeless;
- implementation-independent;
- numerically bounded;
- explicit about advisory estimates;
- separate from persisted statistics format and the cost model;
- free of implementation-status or test-result claims.

No hard-coded estimator defaults, new SQL operators, persisted fields, error enums, optimizer framework or accuracy guarantees were introduced.

New semantic questions: **NONE**

# 45–47. Validation and final Git state

`git diff --check`: **PASS**

Repair-only diff:

```text
120 insertions
24 deletions
1 file changed
```

Final Git status:

```text
 M docs/ARCHITECTURE.md
```

Index: clean.

All diff hunks are within the original live Chapter-35 range. HEAD remained:

```text
40e028a82f3affcd59ce71e3b5d2020166c3d453
```

# 48. Explicit confirmations

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- CHAPTERS 31–34 NOT MODIFIED
- CHAPTER 36 NOT MODIFIED
- HISTORICAL REVIEW ARTIFACTS UNTOUCHED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT
- CHAPTER 36 REVIEW NOT STARTED

Finding disposition:

```text
N35-1: CLOSED
N35-2: CLOSED
N35-3: CLOSED
N35-4: CLOSED
N35-5: CLOSED
```

CHAPTER 35 ARCHITECTURE FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT