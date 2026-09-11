# Chapter 29 Fix C — complete

1. Initial HEAD/status

- HEAD: `68f12c017f39752a5747353d053c1d306394eeca`
- Index: clean
- Existing tracked change: `docs/ARCHITECTURE.md`
- Existing untracked historical artifacts: `FIX-A.md`, `FIX-B.md`, and `FIX-C.md` under the Chapter-29 review directory. They were not read or modified.

2. Fix-B preservation

Confirmed. The unstaged canonical GROUP BY and DISTINCT representative changes remain present and unchanged.

3. Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

4. Architecture sections modified by Fix C

- §17.4.3 — aggregate FLOAT64 summary
- §22.4.1 — bounded physical-result variability
- §24.6 — memory-pressure preservation contract
- §25.8 — vector occurrence/reduction-tree handoff
- §29.2 — aggregate state API
- §29.3 and §§29.3.1–29.3.9 — F2 semantics and examples
- §29.6 — FLOAT64 spill state
- §29.9 — aggregate invariants
- §29.10 — ordered-aggregate retained output
- §32.6 — parallel aggregation
- §38.14 — SortAggregate cost/resource model
- §39.1.3 — aggregate overflow classification
- §§39.3.1–39.3.2 — integer/FLOAT64 execution errors
- §41.5 — Architecture-level verification obligations

5. M29-2 lifecycle

`PhysicalSortAggregate` now:

1. processes one active ordered group;
2. combines its complete partial state;
3. finalizes and validates the closed group;
4. materializes its complete output row;
5. retains the row in order-preserving query-temporary storage;
6. continues through all demanded groups;
7. publishes successful readiness only after all input, Combine, group validation, and output preparation succeeds;
8. exposes the retained rows as its Source only after readiness.

A later group or pre-readiness retained-output failure discards prior retained rows and exposes no aggregate prefix.

6. Mutable current-group rule

Approximately one active group bounds mutable aggregate-state memory only. It does not bound total query memory or retained output size.

7. Retained-output owner

A `RowCollection` or implementation-equivalent query-owned retained-row collection owns finalized rows and stable fixed/varlen payloads.

8. Memory accounting

All retained output uses exact extent arithmetic and continuous `QueryMemoryManager` accounting.

9. Spill behavior

The retained collection is `SpillManager`-eligible, order-preserving, query/attempt-local, non-WAL, nonpersistent, non-recoverable, and not reused across retry.

10. External-publication gate

No retained group row is a dependent-pipeline handoff or client-visible result before complete successful readiness. Post-readiness cursor failures retain Chapter 31’s existing prefix semantics without becoming successful query completion.

11. Ordering

The retained Source preserves only the `OrderingProperty` proven for the selected physical plan.

12. Old F1 model

`ExactFiniteDyadic`, exact finite accumulation, one-final-rounding FLOAT64 SUM, and exact-rational FLOAT64 AVG were removed as normative aggregate semantics.

13. F2 SUM

Every demanded non-NULL finite occurrence is one leaf in an admitted finite binary reduction tree and contributes exactly once. Every internal edge is binary64 addition under round-to-nearest, ties-to-even. SUM returns the selected tree root after external zero/NaN canonicalization.

14. F2 AVG

FLOAT64 AVG retains:

```text
selected F2 binary64 subtotal
+
exact non-NULL count
+
explicit-input special flags
```

Finite AVG converts the exact count to binary64 and performs one binary64 division:

```text
subtotal / converted_count
```

It never averages partial averages.

15. AVG count

The count remains exact and has no INT64 result-width limit. Conversion rounds nearest/ties-even; a value beyond finite binary64 conversion range becomes `+Infinity`, not COUNT overflow or a practical row limit.

16. Explicit-input specials

Order-independent precedence remains:

```text
input NaN                  -> canonical NaN
input +Infinity and -Infinity -> canonical NaN
input +Infinity only       -> +Infinity
input -Infinity only       -> -Infinity
otherwise                  -> selected finite-input tree
```

Finite-tree-generated infinity/NaN does not retroactively become an explicit-input flag.

17. Zero

Every exposed SUM/AVG zero is canonical `+0.0`.

18. NaN

Every exposed SUM/AVG NaN is the Chapter-17 canonical quiet NaN.

19. Legal tree

A legal tree contains exactly the demanded finite logical occurrences. Every internal node performs one correctly rounded binary64 addition. Empty partial state is an identity without a synthetic addition; combining nonempty partials creates one legal edge.

20. Shape-dependent results

The selected root may vary with:

- unconstrained input encounter order
- vector and chunk boundaries
- worker partitioning and scheduling
- local-state partitioning
- Combine-tree shape
- hash versus ordered aggregation
- spill partitioning and replay
- constant evaluation’s selected legal tree

When roots are finite, low bits may differ. Intermediate overflow can also make legal trees differ between finite, infinity, or NaN roots.

21. Invariant observables

Execution shape cannot change:

- occurrence membership or multiplicity
- NULL admission and empty behavior
- exact AVG count
- explicit-input NaN/infinity classification
- result TypeId or nullability
- canonical external zero/NaN representation
- grouping/DISTINCT classes and representatives
- schema or `LogicalSlotId`
- resource, cancellation, and transaction ownership

22. Update

FLOAT64 Update forms an admitted binary64 reduction path over every admitted occurrence exactly once. CONSTANT and repeated DICTIONARY occurrences retain logical multiplicity.

23. Combine

Combining two nonempty FLOAT64 partials adds their binary64 subtotals through one legal edge, combines AVG counts exactly, and ORs explicit-input flags.

24. Parallel execution

Worker count, scheduling, partitioning, and Merge shape may select a different legal FLOAT64 tree. COUNT, integer SUM/AVG, and MIN/MAX remain execution-shape invariant.

25. Aggregate spill

Spill may replay raw values or preserve exact binary64 subtotal bits, exact count, special flags, and descriptor state. Decimal serialization, lost contributions, narrowed counts, lost flags, and incompatible precision remain forbidden.

26. Hash versus ordered aggregation

They preserve the same admitted inputs, NULL/empty semantics, explicit-special policy, result type, schema, slots, canonical representatives, and failure ownership. Their F2 tree-selected FLOAT64 roots may differ.

27. Boundary vectors

The corpus now distinguishes `TREE-INVARIANT` from `TREE-DEPENDENT`. It includes:

- cancellation examples yielding `1.0` versus `+0.0`;
- AVG counterparts yielding rounded `1/3` versus `+0.0`;
- finite-overflow trees yielding `+Infinity` versus `max_finite`;
- retained invariant NULL, zero, NaN, infinity, integer, COUNT, and MIN/MAX boundaries.

28. Forbidden implementations

Removed F1-only prohibitions against ordinary binary64 partials and shape variation. The list now forbids:

- occurrence omission/duplication/deduplication;
- invalid vector-lane/cardinality handling;
- wrong rounding or hidden precision;
- results corresponding to no admitted tree;
- partial-average averaging;
- count corruption;
- lost special flags;
- noncanonical zero/NaN;
- unsafe spill encoding;
- extending F2 variability to exact aggregates or grouping semantics;
- early group publication;
- resource-failure approximation.

29. Error contract

- COUNT and integer SUM retain `NUMERIC_OVERFLOW`.
- Intermediate integer SUM overflow remains prohibited because integer state is exact.
- FLOAT64 tree-edge overflow produces IEEE infinity, not `NUMERIC_OVERFLOW`.
- Existing resource, representability, spill, cancellation, and transaction owners remain unchanged.

30. Global stale-F1 audit

| Location | Old concept | Disposition | Final status |
|---|---|---|---|
| §17.4.3 | Exact aggregate accumulation summary | Replaced with legal binary64-tree summary | CLEAN |
| §22.4.1 | Physical choice cannot vary any value | Qualified by explicitly bounded semantic result families | CLEAN |
| §24.6 | Spill must preserve one exact result value | Preserves owner contract; admits only §29.3.4 variability | CLEAN |
| §25.8 | Vector shape cannot affect aggregate reduction | May select legal tree; occurrence admission remains exact | CLEAN |
| §29.2 | Exact state and no physical-order reduction | Split exact-state/F2 contracts | CLEAN |
| §§29.3–29.3.2 | `ExactFiniteDyadic` state | Replaced with `Binary64Partial` | CLEAN |
| §29.3.4 | Exact dyadic SUM/exact-rational AVG | Replaced by F2 tree and binary64 division | CLEAN |
| §29.3.5 | Exact finite finalization | Replaced with selected finite tree; explicit flags retained | CLEAN |
| §29.3.7 | Merge-tree/result invariance | Exact aggregates remain invariant; F2 root may vary | CLEAN |
| §29.3.8 | Universal permutation results | Classified tree-invariant/tree-dependent | CLEAN |
| §29.3.9 | F1 prohibitions | Rewritten for bounded F2 correctness | CLEAN |
| §29.6 | Exact dyadic spill | Replaced with raw replay or exact F2 state bits | CLEAN |
| §29.9 | Exact FLOAT invariant | Replaced with F2 invariant | CLEAN |
| §29.10 | Hash/ordered numeric identity | Qualified by admitted F2 roots | CLEAN |
| §32.6 | Rounded worker partials forbidden | Legal parallel tree required | CLEAN |
| §38.14 | Ordered aggregation has only low state memory | Added retained-output materialization/spill cost | CLEAN |
| §39.1.3 | COUNT/SUM overflow could imply FLOAT SUM | Restricted to COUNT/integer SUM | CLEAN |
| §§39.3.1–39.3.2 | Exact n-ary FLOAT summary | Replaced with tree-edge IEEE behavior | CLEAN |
| §41.5 | Identical aggregate values across shapes | Independent legal-tree oracle for FLOAT SUM/AVG | CLEAN |
| Chapter 34 | Exact-dyadic statistics normalization | Independent statistics contract | UNRELATED—PRESERVED |

31. Regression matrix

| Contract | Result |
|---|---|
| COUNT | Unchanged; exact count and final overflow |
| Integer SUM | Unchanged; exact mathematical subtotal until Finalize |
| Integer AVG | Unchanged; exact rational sum/count |
| MIN/MAX | Unchanged; canonical candidates |
| GROUP BY representative | Unchanged; zero → `+0.0`, NaN → canonical NaN |
| DISTINCT representative | Unchanged; componentwise canonical |
| Baseline hash DISTINCT | Still blocking Sink → Finalize → Source |
| Ordering/capability ownership | Chapters 30/37/38 remain authoritative |

32. Finding closure

| Finding | Status |
|---|---|
| M29-1 | CLOSED |
| N29-5 | CLOSED |
| N29-1 | CLOSED |
| N29-2 | CLOSED |
| M29-2 | CLOSED |
| D29-1 | CLOSED |

33. New frozen semantic questions: **NONE**

34. `git diff --check`: **PASSED**

35. Cumulative Architecture diff relative to current HEAD:

```text
docs/ARCHITECTURE.md | 559 ++++++++++++++++++++++++++++++++++-----------------
1 file changed, 380 insertions(+), 179 deletions(-)
```

This includes the preserved Fix-A, Fix-B, and Fix-C changes.

36. Final status

```text
 M docs/ARCHITECTURE.md
?? docs/reviews/.../Chapter 29/FIX-A.md
?? docs/reviews/.../Chapter 29/FIX-B.md
?? docs/reviews/.../Chapter 29/FIX-C.md
```

HEAD remains `68f12c017f39752a5747353d053c1d306394eeca`; index remains clean. The untracked historical artifacts were untouched.

37. Confirmed:

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT

CHAPTER 29 ARCHITECTURE READY FOR READ-ONLY CLOSURE AUDIT