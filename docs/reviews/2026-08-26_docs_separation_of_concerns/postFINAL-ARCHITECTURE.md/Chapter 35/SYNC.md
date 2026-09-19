## Chapter 35 Verification synchronization report

1. **Initial HEAD/status:** `04a1c1412186c198523e652f3d63a62b886d3eff` — `04a1c14 applied FIX-A 35 in ARCHITECTURE`; clean worktree and index.

2. **Final HEAD/status:** HEAD unchanged; worktree contains only `M docs/VERIFICATION.md`; index clean.

3. **Files modified:** [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:23603) only.

4. **Sections changed:** One new Chapter-35 Verification section, lines 23603–23894, inserted after V34 and before `Control-Operator Tests`. Existing procedures were not edited.

5. **Architecture status:** CLEAN — CLOSED / UNMODIFIED.

6. **V35 title/location:** `Chapter 35 — Cardinality Estimation and Semantic-Proof Verification`, beginning at line 23603.

7. **V35 family inventory:**

   - V35-A — Estimation event, identity, and ownership oracle
   - V35-B — Equijoin zero domains, 3VL, MCV, and uniqueness
   - V35-C — Fallback configuration, validation, and precedence
   - V35-D — Generic predicate and selectivity integration
   - V35-E — Known and unknown LIMIT/OFFSET and exact K
   - V35-F — Structural transfers and saturating arithmetic
   - V35-G — Operator cardinality, grouping, LEFT preservation, and width
   - V35-H — Stable statistics inputs and fallback classification
   - V35-I — Estimate/proof boundary, errors, and resources
   - V35-J — Frozen-owner regressions, diagnostics, and end-to-end handoff

8. **Atomic obligations:** 69 unique contiguous definitions, `V35-001` through `V35-069`.

9. **Existing procedures repaired:** None. The diff has 292 insertions and zero deletions.

10. **Reusable procedure inventory:** V20-4/5/6/10/11/15/16/19; V22-I/J/K; V29-B; V30-J; V31-A/B/G/N; V32-B/C/H/I; V33-E–H, especially V33-G; V34-B/C/F/G/J/K. Reused headings include `Selectivity Estimation Tests`, `Semantic Emptiness Tests`, `Join Estimation Tests`, `Cost Model Tests`, `Final Optimizer Validation Tests`, `Optimizer Differential Correctness Tests`, and `Optimizer Diagnostics Tests`.

11. **Estimation event/ownership oracle:** V35-A correlates invocation, logical-node and slot identities, catalog descriptors, per-object StatsVersions, statistical fields, configuration, selected model, 3VL result, cardinality, width representation, saturation, confidence, provenance, exact proof, cost handoff, and final validation.

12. **Positive instrumentation control:** V35-001 follows a real typed query through estimation, costing, legal plan selection, and validation.

13. **Missing instrumentation:** V35-002 requires `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`; output coincidence cannot PASS.

14. **Fault-path nonvacuity:** V35-003 requires proof that every named arithmetic, model, fault, race, or validation boundary was reached. All subsequent V35 fault procedures inherit this rule.

15. **Zero-domain equijoin:** V35-006–V35-009 cover both-all-NULL, one-sided zero domains, partial NULLs, NDV-one, and stale-zero statistics with runtime matches.

16. **Equijoin 3VL oracle:** Independently checks:

```text
UNKNOWN = 1 - (1 - nA) * (1 - nB)
```

TRUE is bounded by joint non-NULL mass, FALSE is the remainder, and INNER cardinality uses TRUE only.

17. **MCV/residual zero domains:** V35-010–V35-011 cover MCV-exhausted mass, zero residual mass/NDV, differing heavy hitters, duplicates, NULLs, and one-time MCV contribution.

18. **Unique-key refinement:** V35-012 distinguishes trusted non-NULL uniqueness from nullable, inapplicable, and estimated-NDV cases. Foreign-key refinement remains optional and outside baseline.

19. **Fallback inventory:** V35-013 covers all eleven live §35.25 categories: base rows, three width categories, equality/range/NULL/NDV estimates, generic 3VL, and unknown OFFSET/LIMIT effects.

20. **Invalid fallback configuration:** V35-014–V35-016 cover negative/nonfinite rows and widths, wrong width representation, invalid probabilities/triples, and out-of-bound count effects. Tests preserve the exact owner’s reject, clamp, replacement, and normalization distinctions.

21. **Base cardinality fallback:** V35-017–V35-018 verify valid-old selection first, finite missing fallback, stale-zero provenance, no synchronous ANALYZE, and no emptiness proof.

22. **Width categories and units:** V35-019 and V35-047–V35-049 independently cover logical/value, stored-tuple, and operator-temporary bytes, including known-layout precedence, pruning, and the reservation/allocation distinction.

23. **Generic predicate fallback:** V35-022–V35-024 require exact Chapter-17 semantics first, complete finite 3VL, LOW confidence, TRUE-only filtering, and preservation of demanded errors.

24. **Fallback precedence:** V35-020 and V35-039 test structural over heuristic, specialized over generic, valid statistics over missing fallback, valid-old over invalid-new, known layout over width fallback, known count over unknown-count fallback, and trusted constraints over statistical assumptions.

25. **Confidence/provenance:** V35-021 tests all collected, missing, stale, generic, mixed, independence, and damping paths; it requires the full material chain and least-confident material assumption.

26. **Known LIMIT/OFFSET:** V35-028 verifies OFFSET-before-LIMIT, exact max/min transfer, known LIMIT zero, numerical-zero children, and independent proof propagation.

27. **Unknown LIMIT/OFFSET:** V35-029–V35-032 cover every known/unknown combination, bounded configured effects, no eager evaluation, runtime zero/error/invalid values, and numerical-zero children without proof.

28. **Exact Top-N K:** V35-033 composes V22-J/K and V30-J. Approximate or unknown counts never create exact K; unrepresentable or unknown K retains the conforming ordering-plus-Limit alternative.

29. **Five transfer rows:** V35-034–V35-039 individually cover `LogicalValues`, no-FROM, `LogicalSort`, CROSS JOIN, and generic INNER JOIN, including specialized-estimator precedence.

30. **Saturating arithmetic:** V35-040–V35-041 use symbolic/arbitrary-precision reference arithmetic below, at, and above the bound, including zero-times-huge and huge-product/tiny-selectivity cases. NaN, infinity, undefined division, and unguarded overflow cannot escape.

31. **Estimate versus proof:** V35-056–V35-058 contrast statistical zeroes with zero-row Values, typed contradictions, actual LIMIT zero, and approved propagation. Forged proof provenance must fail validation.

32. **Selectivity/3VL integration:** V35-023–V35-027 cover NULL equality, MCV/residual equality, min/max, histograms, NULL predicates, IN/NOT IN, NOT, AND, OR, same-column constraints, and correlation uncertainty.

33. **Join integration:** V35-006–V35-012, V35-037–V35-039, and V35-045–V35-046 cover duplicate keys, NULLs, skew, uniqueness, stale/missing inputs, INNER, CROSS, generic INNER, and LEFT preservation.

34. **Aggregate/DISTINCT/NDV:** V35-043–V35-044 cover NULL grouping, duplicates, correlated multi-column damping, capping, DISTINCT, global-empty one-row output, and grouped-empty zero-row output.

35. **Rows/width/cost handoff:** V35-047–V35-049 and V35-067 correlate stage cardinalities, widths, slots, provenance, cost consumers, selected metadata, and validation. Wrong-unit and wrong-slot metadata are negative fixtures.

36. **Stable statistics generations:** V35-050–V35-051 test unequal T1/S1 and T2/S2, T1/S3 publication during planning, and self-join aliases without generation switching or global version equality.

37. **Missing/rejected/stale classification:** V35-052–V35-054 distinguish no generation, valid-old, no compatible generation, malformed outer framing, rejected advisory payload, stale valid input, and unsupported specialization.

38. **Configuration/determinism:** V35-055 fixes all planning inputs for deterministic-path checks, then varies sample, StatsVersion, fallback, budget, and model availability only where Architecture permits different valid estimates.

39. **Error/resource ownership:** V35-059–V35-061 preserve missing/rejected fallback, catalog corruption, configuration/invariant ownership, bounded planning fallback, `OptimizerResourceLimit`, runtime allocation/spill errors, and demanded SQL/count errors.

40. **Chapter-31 regression:** V35-062 preserves pre-W closure, canonical error selection, one mutation publisher, W/C/R, retry, and result ownership.

41. **Chapter-32 regression:** V35-063 preserves required occurrences, exclusive claims, no replay, legal early stop, demanded errors, and worker-independent SQL semantics.

42. **Chapter-33 regression:** V35-064 preserves capability eligibility, bounded search, objectives, dominance, ties, required properties/slots, fallback, and final validation.

43. **Chapter-34 regression:** V35-065 preserves complete per-object generations, cross-table version independence, valid-old/missing selection, retention, GC, and advisory-only statistics.

44. **Diagnostics/plan metadata:** V35-066 compares rows, representation-specific widths, StatsVersions, confidence, provenance, fallback path, proof, selected plan, and runtime actuals against the central ledger.

45. **All-23-invariant matrix:**

| Invariant | V35 coverage |
|---:|---|
| 1 | V35-014–016, 040–041 |
| 2 | V35-005, 056–058 |
| 3 | V35-022–027, 042 |
| 4 | V35-006–008, 015, 022–027 |
| 5 | V35-007–008, 023, 025 |
| 6 | V35-026 |
| 7 | V35-026 |
| 8 | V35-027 |
| 9 | V35-006–012, 043 |
| 10 | V35-010–011, 025, 041 |
| 11 | V35-027 |
| 12 | V35-027, 043 |
| 13 | V35-043 |
| 14 | V35-045–046 |
| 15 | V35-019, 042, 047–049 |
| 16 | V35-056–058 |
| 17 | V35-009, 018, 056 |
| 18 | V35-025, 052, 056 |
| 19 | V35-027, 038, 056 |
| 20 | V35-028, 034–038, 044, 057 |
| 21 | V35-006–011, 041 |
| 22 | V35-013–021, 029–032 |
| 23 | V35-034–039 |

All are marked COMPLETE with concrete setup and observable outcomes.

46. **Chapter-35 subsection matrix:** The added matrix covers every live subsection from §35.1 through §35.27, including §§35.6.1–35.6.5 separately and new §35.17.1. Every row identifies V35 procedures, exact reusable owners, and COMPLETE status.

47. **Chapter-41 matrix:** Eighteen integration rows map §41.6/§41.7 obligations for base rows, widths, compatibility, 3VL, MCV/residuals, ranges, joins, grouping, counts, exact K, saturation, fallback precedence, cost handoff, proof, diagnostics, and frozen Chapters 31–34.

48. **Global stale-rule search:** No stale normative rule requiring zero-NDV division, positive NDV for all valid inputs, two-valued NULL handling, estimate-derived proof, synchronous ANALYZE, hard-coded fallbacks, unknown-count zero, approximate Top-N K, no-FROM zero rows, unsaturated joins, global StatsVersion equality, weakened execution invariants, or globally cheapest unbounded search was found. Apparent matches were valid different-owner/stage/approximation/optional-capability text or explicit negative tests.

49. **Duplicate-ID check:** PASS. Ten unique families, V35-A through V35-J; 69 unique contiguous atomic definitions, V35-001 through V35-069.

50. **Broken-reference check:** PASS. All cited IDs and named headings resolve in the live Verification document.

51. **Document-role audit:** The addition is timeless, procedural, independently falsifiable, and implementation-independent. It introduces no SQL semantics, format, statistics field, estimator algorithm, hard-coded default, provenance enum, cost model, error enum, proof source, Top-N capability, C++ API, or execution claim.

52. **New semantic questions:** NONE.

53. **`git diff --check`:** PASS.

54. **Verification-only diff:** 292 insertions, 0 deletions, one file.

55. **Final Git status:**

```text
 M docs/VERIFICATION.md
```

Index remains clean.

56. **Explicit confirmations:**

```text
ARCHITECTURE NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
CHAPTERS 31–34 NOT MODIFIED
HISTORICAL REVIEW ARTIFACTS NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
CHAPTER 36 REVIEW NOT STARTED
```

CHAPTER 35 VERIFICATION SYNCHRONIZATION COMPLETE —
READY FOR READ-ONLY VERIFICATION CLOSURE AUDIT