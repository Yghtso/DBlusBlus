# 35. Cardinality Estimation

## 35.1 CardinalityEstimate

Estimated row counts use a wide floating representation such as:

```text
double
```

Intermediate estimates remain fractional.

A cardinality estimate tracks conceptually:

```text
estimated_rows >= 0
is_provably_empty
```

These fields are orthogonal. `estimated_rows` is approximate cost metadata. `is_provably_empty` is an exact logical fact with the restricted provenance defined by §35.2.

Pathological arithmetic is clamped to a large finite implementation maximum rather than producing NaN/Infinity inside optimizer math.

The executor still processes integer row counts.

## 35.2 Cardinality estimate versus semantic-emptiness proof

The architecture separates two concepts:

```text
estimated_rows / PredicateTruthEstimate:
    approximate cost-planning metadata

is_provably_empty:
    exact logical fact required for an empty-output simplification;
    demand-preserving replacement remains governed by §20.17.10
```

Any estimator may return numerical zero. In particular, statistics, sampling, histogram boundaries, MCV absence, HLL/NDV estimates, independence assumptions, clamping, and rounding may produce:

```text
estimated_rows == 0
PredicateTruthEstimate.true_fraction == 0
```

Neither value implies `is_provably_empty == true`, and an estimated probability of zero does not mean TRUE is logically impossible. An implementation may apply a positive minimum working estimate to a non-proven subtree to avoid cascading zero-cost plans, but that numerical policy does not change semantic proof state.

Only the logical semantic-reasoning layer may originate `is_provably_empty`. Cardinality/predicate estimators may consume and preserve an existing proof, but MUST NOT create one from an estimate or from any statistics descriptor. The exact whitelist of proof sources is:

1. an exact zero-row logical input, specifically `LogicalValues` with zero rows or an equivalent already-proven empty relational node;
2. successful exact evaluation of a complete constant predicate that is eligible for §20.17.1 constant folding, using the same Chapter-17/operator semantics as execution, when it is FALSE or UNKNOWN in a row-rejecting context; evaluation MUST NOT hide or newly force a SQL error, and STABLE/VOLATILE expressions are not a source;
3. an exact no-TRUE fact or logical contradiction from Chapter-17 operator/type semantics and normalized typed literals, including ordinary comparison with literal NULL, complete IN/NOT IN cases whose result can never be TRUE, equality of distinct exact constants, contradictory equality/IN/bound sets, and type-domain impossibility established by the binder/TypeResolver's authoritative type domain;
4. a contradiction with a trusted, currently enforced catalog/schema constraint: `NOT NULL`, including the NOT NULL component of PRIMARY KEY; PRIMARY KEY/UNIQUE key metadata may otherwise prove keys or multiplicity but does not by itself prove value absence;
5. `LogicalLimit` whose actual SQL `LIMIT` value is exactly zero; and
6. operator-specific propagation of an existing approved proof exactly as defined by §20.17.10.

The whitelist is closed for the defined architecture. CHECK constraints and foreign keys are deferred and cannot be assumed. Disabled, deferred, unvalidated, stale, or unsupported metadata cannot be a proof source. No table/index contents are treated as exact planning metadata merely because a collection routine once observed them.

The following are explicitly forbidden proof sources, individually or in composition:

```text
table analyzed_live_row_count / row_count
column or index NDV
column or index min/max
MCV presence, absence, or frequency
histogram boundaries, buckets, absence, or mass
HLL or any approximate sketch/domain estimate
null_fraction
ANALYZE samples or exact-at-ANALYZE observations
stale/current statistics cache entries
estimated join-domain overlap/disjointness
physical/logical/invisible index-entry counts
index leaf-page/occupancy/correlation statistics
missing-statistics fallback values
any composed estimate whose numerical result is 0 or 1
```

Semantic proof provenance is planner/query-lifetime metadata, not persisted metadata. Conceptually it records one or more exact kinds such as:

```text
EXACT_EMPTY_INPUT
CONSTANT_3VL_REJECTION
EXACT_PREDICATE_CONTRADICTION
TRUSTED_ENFORCED_CONSTRAINT
SQL_LIMIT_ZERO
PROPAGATED_SEMANTIC_PROOF
```

Estimate confidence/provenance from §35.26 remains separate and can never be promoted into this proof provenance.

Estimates, including zero/full selectivity, may affect join order, access-path cost, startup/total cost, and physical algorithm selection. They MUST NOT establish logical equivalence, remove a filter or join, eliminate rows, replace a subtree with empty output, or remove every runtime access path.

Consequently, each of these is an architecture violation:

```text
min/max says a value is absent -> replace a scan with empty output
analyzed row_count == 0 -> eliminate a scan
null_fraction == 0 -> eliminate IS NULL
statistical join ranges are disjoint -> eliminate INNER JOIN
value absent from MCV -> declare equality impossible
HLL/estimated domain omits a key -> prove the key absent
estimated_rows == 0 -> set is_provably_empty
estimated TRUE probability == 0 -> prove FALSE/impossible
index live/physical-entry estimate == 0 -> skip runtime index/heap access
```

SQL three-valued context is material. WHERE, HAVING, and INNER JOIN matching retain rows/pairs only for TRUE, so a complete predicate proven always FALSE or UNKNOWN rejects every candidate. For LEFT JOIN ON, such a proof establishes no matches but does not empty the preserved left output. FALSE and UNKNOWN remain distinct values in CASE, projection, and other scalar contexts. IN/NOT IN follow their complete SQL 3VL semantics; partial knowledge that UNKNOWN is possible is not an emptiness proof.

An explicit `is_provably_empty` flag with approved semantic provenance is
necessary for an empty-result replacement; it is not sufficient to eliminate
execution. The replacement must also preserve demanded correctness-observable
evaluations and all statement/operator semantics under §§20.17.8 and 20.17.10.
Estimated row count or probability supplies neither this exact proof nor
permission to suppress evaluation.

## 35.3 Row-width estimate

Every logical/physical estimate includes average output width appropriate to the representation/operator being costed.

For fixed types use the relevant execution/temporary representation width.

For VARCHAR use statistics such as:

```text
average_width
```

plus applicable vector/row-layout overhead.

Estimated width influences:

```text
hash memory
aggregate memory
sort/run size
spill probability
temporary I/O
decode/materialization CPU
```

## 35.4 PredicateTruthEstimate

Predicate estimation is three-valued.

Represent conceptually:

```text
PredicateTruthEstimate {
    true_fraction
    false_fraction
    unknown_fraction
}
```

with each fraction finite, clamped to `[0,1]`, and the sum normalized to approximately 1.

Every primitive estimator computes all three components rather than leaving FALSE/UNKNOWN implicit.

The three fractions are cardinality-planning fields, even when exact operator semantics determines a particular numerical value. When a primitive or composed NOT/AND/OR formula yields `0` or `1`, semantic impossibility is still derived separately under §35.2; statistical inputs cannot acquire exact logical provenance through composition.

A common finalization step conceptually performs:

```text
clamp t, f, u to [0,1]
reject/replace non-finite intermediate values
normalize small floating drift so t + f + u = 1
```

Large logical inconsistencies are estimator bugs, not values to hide through normalization.

This estimator-intermediate finalization is not persisted-statistics validation
and does not reuse or broaden `STATS_PROBABILITY_SUM_EPSILON`. Persisted inputs
must already have passed §34.14.6; Chapter 35 clamping cannot legalize a rejected
payload.

Filter cardinality uses only:

```text
true_fraction
```

This prevents `NOT`, NULL comparisons, AND, and OR from silently using two-valued Boolean math.

## 35.5 Selectivity bounds

Every selectivity is clamped to:

```text
0 <= s <= 1
```

For filters:

```text
estimated_rows = input_rows * true_fraction
```

followed by the provably-empty/minimum-nonempty rule.

Here “provably empty” means an independent §35.2 semantic proof. Multiplication, clamping, or rounding to numerical zero does not set the proof flag.

## 35.6 Equality to a constant

For:

```sql
column = constant
```

estimate in the following order.

### 35.6.1 NULL constant

Ordinary equality to NULL is never TRUE:

```text
true_fraction = 0
unknown_fraction = 1
```

for this comparison expression.

This is not rewritten into `IS NULL`.

This no-TRUE fact comes from exact SQL comparison semantics, not `null_fraction`. In a WHERE/HAVING/INNER JOIN row-rejecting context, the semantic layer therefore records the approved §35.2 proof even though the expression's exact result is UNKNOWN rather than FALSE.

### 35.6.2 MCV hit

If the non-NULL constant appears in the MCV list:

```text
true_fraction = MCV frequency
```

subject to consistency clamps.

### 35.6.3 Outside statistical range

When a non-NULL constant lies outside available orderable min/max statistics:

```text
true_fraction = 0
```

is an allowed numerical estimate for the equality predicate. It MUST NOT set `is_provably_empty`, even for a simple base filter with no other disjunct, because min/max is potentially stale. Absence from the MCV list, histogram, or an HLL-derived domain estimate has the same cost-only status.

### 35.6.4 Residual equality

Otherwise:

```text
remaining_nonnull_nonmcv_mass
/
max(1, NDV - MCV_distinct_count)
```

estimates equality probability for a non-MCV value.

### 35.6.5 Complete truth triple for non-NULL equality

For any non-NULL constant equality estimate with column NULL fraction `n` and estimated TRUE fraction `t`:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

with the normal finite/clamp/normalization step.

`t` is always bounded by the non-NULL mass:

```text
0 <= t <= 1 - n
```

If min/max estimates no non-NULL match:

```text
t = 0
unknown = n
false = 1 - n
```

This numerical triple does not prove the filter empty. A separate exact §35.2 rule may do so—for example, exact constant evaluation or a contradiction with an enforced NOT NULL/type-domain fact—but statistical min/max and `null_fraction` cannot.

## 35.7 Column-to-column equijoin

For:

```text
A.x = B.y
```

the baseline non-MCV formula is:

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

subject to bounds and stronger metadata refinements.

The zero-domain branch is required for valid empty/all-NULL statistics and for
any one-sided zero non-NULL domain. It performs no division and estimates no
TRUE equality matches. It does not convert NULL-driven UNKNOWN mass to FALSE,
and its numerical zero does not establish semantic emptiness. For validated
statistics, a positive non-NULL observation implies `NDV >= 1` under
§34.14.6.7; Chapter-35 arithmetic does not use this guard to accept an invalid
statistics payload.

For the comparison expression's truth-state estimate, if the input NULL fractions are `nA` and `nB`:

```text
unknown_fraction
    = 1 - (1 - nA) * (1 - nB)
```

The estimated TRUE pair probability is bounded by the joint non-NULL mass, and FALSE is the remaining probability.

If compatible statistical min/max ranges are disjoint:

```text
join_rows = 0
```

is an allowed cardinality estimate. It does not prove an INNER or range join semantically empty and does not authorize join removal. The same prohibition applies to semi/anti join extensions, which are outside the baseline. Later committed changes may create overlap. A LEFT JOIN remains required to preserve its left side regardless of estimated match absence.

## 35.8 MCV-aware equijoin

When both sides have MCV lists:

1. find common MCV values using SQL equality semantics,
2. add for each common value `v`:
   ```text
   rows_A * freq_A(v) * rows_B * freq_B(v)
   ```
3. remove that represented probability/NDV mass,
4. estimate residual values from residual non-NULL mass and residual NDVs.

This protects hot join keys from severe underestimation.

The residual path applies §35.7's zero-domain rule to the residual population:
if either residual non-NULL mass or residual NDV is zero, that path contributes
zero TRUE matches without division. MCV contribution remains separate and is
added exactly once.

No common MCV value, MCV absence on either side, or zero residual estimated mass proves that the join has no matching key.

## 35.9 Unique-key refinement

If one equijoin side `U` is a proven unique non-NULL key:

```text
each opposite-side row matches at most one U row
```

The estimated pair count is therefore bounded by the non-NULL opposite-side rows, subject to domain overlap.

Foreign-key refinements are outside the baseline because foreign keys are not
part of the trusted-constraint surface. An extension that adds them must
preserve the same key, multiplicity, NULL, and proof boundaries.

## 35.10 Range predicates

For:

```text
<  <=  >  >=
```

combine:

```text
explicit MCV contribution
+
residual histogram mass
```

For a boundary inside a histogram bin, interpolate when the type supports meaningful order interpolation.

For binary VARCHAR, the baseline within-bin interpolation may be rank/uniform-mass based rather than numeric-distance based.

Out-of-range statistical min/max may estimate zero or full non-NULL coverage.

For a comparison against a non-NULL constant, let:

```text
n = column null_fraction
t = estimated TRUE range mass
```

Then the complete truth triple is:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

with `0 <= t <= 1 - n`.

For `<`, `<=`, `>`, `>=`, and conjunctions of those registered comparisons,
boundary inclusion/exclusion controls the numerical estimate using the exact
comparison semantics. `BETWEEN` itself is not a scalar-registry entry under
§17.9.2. A zero estimate caused by a min/max boundary, histogram gap, MCV
absence, or clamping remains cost-only and MUST NOT set `is_provably_empty`.
Exact contradictory literal bounds are handled separately by §§20.17.8 and 35.2.

## 35.11 NULL predicates

Let:

```text
n = null_fraction
```

For:

```sql
column IS NULL
```

the estimated truth triple is:

```text
true_fraction    = n
false_fraction   = 1 - n
unknown_fraction = 0
```

For:

```sql
column IS NOT NULL
```

the estimated truth triple is:

```text
true_fraction    = 1 - n
false_fraction   = n
unknown_fraction = 0
```

A trusted NOT NULL constraint makes:

```text
IS NULL:
    true = 0, false = 1, unknown = 0
    filter is provably empty

IS NOT NULL:
    true = 1, false = 0, unknown = 0
```

The trusted constraint, not `null_fraction`, supplies that semantic result. Without it, `null_fraction == 0` may estimate `IS NULL` at zero but cannot prove it impossible, and `null_fraction == 1` may estimate `IS NOT NULL` at zero but cannot prove it impossible.

## 35.12 IN-list predicates

For a constant IN list:

1. deduplicate non-NULL constants using SQL equality/grouping-compatible scalar semantics,
2. sum their mutually exclusive equality TRUE selectivities into `t`,
3. clamp `t` to the column's non-NULL mass,
4. record whether the IN list contains at least one NULL element.

Let:

```text
n = column null_fraction
```

If the list contains **no NULL**:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

If the list contains **at least one NULL**:

```text
true_fraction    = t
false_fraction   = 0
unknown_fraction = 1 - t
```

because a non-NULL value that matches no non-NULL list element is still UNKNOWN when compared with the NULL list member.

A NULL input row is also UNKNOWN.

Filter cardinality counts only TRUE rows.

`NOT IN` obtains its semantics through the normal NOT transformation of this complete truth triple.

Statistics-derived equality estimates within `t`, including numerical zero, do not prove an IN/NOT IN filter empty. Only exact evaluation/constraint reasoning over the complete predicate may do so under §35.2.

For example, `x IN (NULL)` and `x NOT IN (NULL)` are exactly never TRUE for every `x` under the 3VL rules and therefore prove rejection in WHERE/HAVING/INNER JOIN contexts. This exact operator-semantic fact does not authorize collapsing FALSE and UNKNOWN in CASE, projection, or other scalar contexts.

## 35.13 NOT

For predicate truth estimate:

```text
NOT:
    true'    = false
    false'   = true
    unknown' = unknown
```

The estimator MUST NOT blindly compute `1 - true_fraction` when UNKNOWN is possible. The resulting numerical triple never creates semantic proof unless the semantic layer independently establishes an exact §35.2 fact.

## 35.14 AND

When no stronger correlation information exists, the architecture uses independence as a fallback while preserving three-valued truth states.

For independent truth triples:

```text
A = (tA, fA, uA)
B = (tB, fB, uB)
```

SQL AND uses:

```text
t = tA * tB
f = fA + fB - fA * fB
u = 1 - t - f
```

equivalently, UNKNOWN covers the combinations where neither operand is FALSE and at least one is UNKNOWN.

Numerical zero/one produced by this independence formula remains an estimate and cannot promote statistical operands to semantic proof.

For simple same-column constraints such as:

```text
x > 10 AND x < 20
```

use intersected constraint/histogram logic rather than multiplying independent estimates.

## 35.15 OR

When independence is the only available model, for:

```text
A = (tA, fA, uA)
B = (tB, fB, uB)
```

SQL OR uses:

```text
t = tA + tB - tA * tB
f = fA * fB
u = 1 - t - f
```

equivalently, UNKNOWN covers the combinations where neither operand is TRUE and at least one is UNKNOWN.

Numerical zero/one produced by this independence formula remains an estimate and cannot promote statistical operands to semantic proof.

For mutually exclusive/same-column ranges or MCVs, use the stronger union model rather than generic independence.

## 35.16 Same-column constraint sets

Before base-filter estimation, normalize simple predicates on one column into a semantic constraint set containing as applicable:

```text
equality
lower bound
upper bound
IN set
IS NULL / IS NOT NULL
```

Intersect compatible constraints and detect contradictions.

Example:

```text
x >= 10
AND x < 5
```

is provably empty.

This contradiction proof uses exact typed predicate bounds and trusted enforced constraints only. Histograms/min/max/MCVs/NDV may estimate the intersected set's cardinality but cannot make the semantic constraint set contradictory.

The same normalized constraint set feeds B+ access-bound construction in Chapter 36.

## 35.17 Correlated-column limitation

Statistics are primarily single-column.

Therefore predicates such as:

```text
city = 'Rome'
AND country = 'Italy'
```

may be substantially misestimated under independence.

The optimizer exposes this as a known limitation rather than inventing false precision.

Optional extended-statistics capabilities outside the baseline include:

```text
multi-column NDV
functional dependencies
multi-column MCVs
```

### 35.17.1 Core structural and join cardinality transfers

The following transfers complete the baseline operator-estimation surface. All
multiplication uses §35.1's finite saturating arithmetic, and every numerical
result remains separate from §35.2 semantic-proof provenance.

| Logical form | Cardinality estimate | Estimate provenance and proof boundary |
|---|---|---|
| `LogicalValues` | Exact structural count of its declared row occurrences. Duplicate value rows remain distinct occurrences. | Logical-structure provenance. Zero rows carry the approved `EXACT_EMPTY_INPUT` proof; a positive count does not suppress evaluation of any demanded row expression or its error. |
| no-FROM source | Exactly one zero-column source occurrence. | Logical-structure provenance, not table statistics or missing-statistics fallback. Demanded expressions above the source still execute. |
| `LogicalSort` | Preserve the child cardinality estimate. | Preserve the child's estimate/proof provenance; Sort changes order, not bag cardinality. |
| `LogicalJoin CROSS` | Saturating product `rows_left * rows_right`. | Derived child-estimate provenance. Numerical zero does not prove either child empty; exact proof propagates only under §20.17.10. |
| generic `LogicalJoin INNER` | Saturating product `rows_left * rows_right * predicate.true_fraction`; FALSE and UNKNOWN pairs do not match. | Retain child and predicate provenance. Use an eligible specialized model such as §§35.7–35.9 instead of this generic transfer; if no specialized predicate model applies, use §35.25's complete 3VL fallback. Numerical zero is not proof unless the semantic layer independently proves a child empty or the complete ON predicate never TRUE. |

These are cardinality transfers, not permissions to change Chapter-20 bag,
demand, error, or occurrence semantics.

## 35.18 Projection cardinality

Projection normally preserves row count:

```text
rows_out = rows_in
```

while recomputing output row width from its output expressions/slots.

## 35.19 Filter cardinality

```text
rows_out = rows_in * predicate.true_fraction
```

followed by exact-empty/minimum-nonempty handling.

## 35.20 LIMIT/OFFSET cardinality

When the applicable LIMIT/OFFSET values are known at planning time, apply the
exact estimate transfer in semantic order:

```text
rows_after_offset = max(0, rows_in - offset)
rows_out =
    min(rows_after_offset, limit)  if LIMIT exists
    rows_after_offset              otherwise
```

For a Chapter-19-admitted execution-start LIMIT or OFFSET whose value is not
available during planning, estimate the unknown step with the corresponding
named §35.25 configured assumption. An unknown OFFSET produces a finite value
bounded by `[0, rows_in]`; a known OFFSET is applied exactly. An unknown LIMIT
then produces a finite value bounded by `[0, rows_after_offset]`; a known LIMIT
is applied exactly. When both are unknown, apply those two configured steps in
OFFSET-then-LIMIT order. These assumptions are LOW-confidence approximate
costing metadata and diagnostics identify the unknown count expression; they
are not evaluated SQL count values. For a positive non-proven input, the
unknown-count assumptions themselves retain a positive working output estimate:
they neither assume that an unknown LIMIT is zero nor that an unknown OFFSET
exhausts the input. Numerical zero may still propagate from the child estimate
without becoming proof.

An actual bound SQL `LIMIT 0` known during planning is provably empty. An
unknown count estimate that happens to be zero is not. An independently
approved child proof still propagates under §20.17.10.

This refers only to the query's actual bound SQL LIMIT. A `required_rows` objective of zero or any other optimizer costing target is not a logical LIMIT and cannot create semantic emptiness; §38.16 remains authoritative.

Planning does not evaluate a residual execution-start count merely to improve
this estimate. Chapter 19 retains one execution-start acquisition and its
NULL/negative/error behavior. Unless the exact mathematical `K` is available
and representable under Chapters 30 and 38, an approximate unknown-count
estimate cannot authorize `PhysicalTopN`, an executor row cap, or semantic
early termination; physical search retains the canonical exact alternative.

## 35.21 DISTINCT cardinality

Estimate DISTINCT output from NDV of projected/grouping expressions when lineage/statistics permit.

For one nullable base column:

```text
distinct_groups
≈ NDV + (1 if null_fraction > 0 else 0)
```

capped by input rows.

A zero result produced from NDV or `null_fraction` is an estimate only; semantic emptiness follows the child-proof rules in §20.17.10.

For multiple columns without extended statistics use the damping rule in §35.23 rather than unrestricted NDV multiplication.

## 35.22 GROUP BY cardinality

For one grouping column:

```text
groups
≈ NDV + (1 if null_fraction > 0 else 0)
groups <= input_rows
```

For multiple columns, first form each component grouping NDV including its possible NULL group, then apply §35.23.

Global aggregation with no grouping columns produces:

```text
1 row
```

even for empty input, matching Chapter 29 aggregate semantics.

Grouped aggregation over a provably empty child is provably empty. Global aggregation over that same child is not; it emits one row. Statistical `input_rows == 0` changes neither semantic conclusion.

## 35.23 Multi-column NDV damping

For multiple grouping/DISTINCT components, sort component NDVs descending:

```text
combined = NDV_1
combined *= NDV_2 ^ 0.75
combined *= NDV_3 ^ 0.50
for every remaining NDV_i:
    combined *= NDV_i ^ 0.25
```

then cap:

```text
combined <= input_rows
```

This is explicitly a heuristic fallback, not claimed correlation knowledge.

Optimizer trace/EXPLAIN diagnostics SHOULD identify when this fallback drove an estimate.

## 35.24 LEFT JOIN cardinality

LEFT JOIN output is bounded below by:

```text
rows_left
```

unless the left input itself is provably empty.

Estimate:

```text
matched_pair_output
+
estimated_unmatched_left_rows
```

The estimator MUST distinguish:

```text
number of matching pairs
```

from:

```text
number of left rows with at least one match
```

A simple bounded probabilistic approximation for unmatched-left rows is acceptable in the defined architecture.

Statistics that estimate no right-side matches do not permit replacing the LEFT JOIN with empty output or dropping left preservation. Semantic simplification, if any, requires an approved exact proof and must preserve null extension.

## 35.25 Missing-statistics fallback

Missing statistics are explicit optimizer inputs, not fabricated precision.

One centralized `EstimatorFallbackConfig` supplies named fallback assumptions for at least:

```text
unknown base-relation row count
unknown logical/value width in bytes
unknown stored-tuple width in bytes
unknown operator-appropriate temporary row width in bytes
unknown equality selectivity
unknown ordered-range selectivity
unknown NULL fraction
generic NDV
generic predicate TRUE/FALSE/UNKNOWN distribution
unknown execution-start OFFSET cardinality effect
unknown execution-start LIMIT cardinality effect
```

The values are optimizer configuration/tuning parameters rather than persistent-format constants.

The base-row fallback is a finite nonnegative row estimate used when no
qualifying `TableStatistics` descriptor supplies `analyzed_live_row_count`.
Width fallbacks are finite nonnegative byte estimates in the specific
Chapter-34 logical/stored or Chapters-24/35 operator representation being
costed; known fixed-type or derivable layout widths take precedence, and widths
from different representations are not interchangeable. A width estimate is
neither a memory reservation nor evidence of physical allocation.

For a predicate without an applicable specialized estimator, exact Chapter-17
operator/NULL semantics, resolved nullability, and trusted constraints are
applied first. The generic fallback supplies only the statistically unresolved
complete `PredicateTruthEstimate`; its components satisfy §35.4 and preserve
any known UNKNOWN mass. It is not a two-valued shortcut. Eligible specialized
equality, range, NULL, IN, same-column, or join models take precedence.

The unknown execution-start LIMIT/OFFSET assumptions are deterministic
configuration functions or parameters with the bounds in §35.20. They carry
LOW confidence and explicit configured-fallback diagnostic provenance, not a
claim that the count was evaluated and not `MISSING_STATISTICS` when statistics
are present.

Every fallback result is:

```text
finite
clamped to its legal domain
marked LOW confidence
tagged with the applicable fallback provenance
```

The `MISSING_STATISTICS` tag applies when absent, rejected, or incompatible
advisory statistics require that fallback. A stale but valid selected
generation remains statistical input and carries `STALE_STATISTICS` rather
than being relabeled missing. Malformed outer catalog page/tuple framing keeps
Chapter 16/34's stronger corruption owner. Non-statistical configured
fallbacks, including unknown execution-start counts, are identified by their
actual fallback assumption and are not falsely tagged as missing statistics.
A generic predicate fallback retains any material input-statistics provenance
and identifies the configured generic-model assumption diagnostically; this
does not require a new persisted or public provenance enum.

Fallback assumptions are shown in optimizer trace/verbose EXPLAIN diagnostics.

A fallback never changes SQL semantics or proves a relation empty.

## 35.26 Estimate confidence and provenance

Cardinality/predicate estimates may carry:

```text
EstimateConfidence {
    HIGH
    MEDIUM
    LOW
}
```

plus one or more provenance tags.

Canonical provenance categories include:

```text
PROVEN_CONSTRAINT
MCV_HIT
HISTOGRAM_RANGE
NDV_ESTIMATE
UNIQUE_KEY
INDEPENDENCE_ASSUMPTION
MULTICOLUMN_DAMPING
MISSING_STATISTICS
STALE_STATISTICS
```

These tags explain how a number was estimated. Even `PROVEN_CONSTRAINT` on an estimate is not the semantic-empty flag: the separate §35.2 proof record must identify the exact contradiction and operator context before execution may be eliminated. Statistical tags, alone or composed with exact estimate inputs, never become semantic proof provenance.

Examples:

```text
provable contradiction / enforced key bound:
    HIGH

fresh exact/MCV evidence:
    HIGH

fresh approximate HLL/histogram evidence:
    MEDIUM or HIGH according to estimator rule

independence or multi-column damping:
    LOW

missing-stat fallback:
    LOW
```

Plan selection is driven by cost, not by a separate confidence penalty.

Confidence/provenance are retained for diagnostics and regression analysis.

A composite estimate carries the least-confident material assumption that substantially determines the result and retains the relevant provenance chain.

## 35.27 Estimation invariants


1. Estimated rows are nonnegative finite values and may be numerical zero without semantic proof.
2. Provably-empty state is explicit, carries approved §35.2 semantic provenance, and is never inferred from estimated rows or truth fractions, whether tiny, zero, or one.
3. Filter selectivity uses SQL TRUE probability, not TRUE+UNKNOWN.
4. Primitive predicate estimators return complete finite TRUE/FALSE/UNKNOWN triples.
5. Equality/range against non-NULL constants assigns column NULL mass to UNKNOWN.
6. IS NULL / IS NOT NULL never produce UNKNOWN.
7. IN-list NULL elements use the exact SQL UNKNOWN rule and NOT IN is derived through NOT.
8. Independence fallback for AND/OR uses the explicit three-valued formulas in §§35.14–35.15.
9. NDV excludes NULL; grouping adds one NULL class when applicable.
10. MCV and residual/histogram mass are not double counted.
11. Same-column constraints are intersected before generic independence assumptions.
12. Correlated-column uncertainty is exposed rather than hidden behind false precision.
13. Multi-column NDV multiplication is damped and capped by input rows.
14. LEFT JOIN cardinality cannot fall below its preserved left input cardinality.
15. Average row width is estimated alongside rows because memory/I/O costs depend on both.
16. Only the semantic reasoning layer originates emptiness proof; predicate/cardinality estimators may preserve but cannot manufacture it.
17. Table/column/index statistics, including values exact for an old ANALYZE snapshot, never establish relation, predicate, access-path, or join emptiness.
18. Statistical min/max disjointness, MCV/histogram/HLL absence, `null_fraction` extremes, and zero row/index counts are cost evidence only.
19. Composed NOT/AND/OR and join estimates do not convert numerical zero/one into logical impossibility.
20. Operator proof propagation follows §20.17.10, including LEFT JOIN preservation and the one-row global aggregate over empty input.
21. Equijoin estimation handles a zero non-NULL domain without division; the resulting zero TRUE estimate preserves NULL-driven UNKNOWN mass and carries no statistical emptiness proof.
22. Missing base rows, unavailable applicable widths, unsupported predicate models, and planning-time-unknown LIMIT/OFFSET effects use the centralized finite configured fallbacks with their required bounds, confidence, and provenance.
23. VALUES, no-FROM, Sort, CROSS JOIN, and generic INNER JOIN cardinalities follow §35.17.1 without changing Chapter-20 multiplicity, demand, error, or proof semantics.
