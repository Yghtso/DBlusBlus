1. **Initial HEAD/status:** `ffec26d70440727b2418e6e9e92fe0f61195a370` — `applied FIX-A 36 in ARCHITECTURE`. Worktree and index were clean.

2. **Final HEAD/status:** HEAD unchanged. Worktree contains only unstaged `docs/VERIFICATION.md`; index remains clean.

3. **Files modified:** [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:23897) only.

4. **Exact Verification sections changed:** One new section, **“Chapter 36 — Cost Model and Base-Access Verification,”** inserted after V35 and before `Control-Operator Tests`. No existing text was rewritten.

5. **Architecture status:** **CLEAN — CLOSED / UNMODIFIED**.

6. **V36 location/title:** Lines 23897–24211, **“Chapter 36 — Cost Model and Base-Access Verification.”**

7. **Family inventory:**

   - V36-A — Cost event, identity, and evidence oracle
   - V36-B — CostConfig, domains, dimensions, and stability
   - V36-C — Finite cost arithmetic and comparison
   - V36-D — Physical input selection and heap fallbacks
   - V36-E — Index pressure, locality, and candidate estimates
   - V36-F — Access paths, predicates, and required columns
   - V36-G — Streaming ownership and composition
   - V36-H — Startup objectives, specialized costs, and plan legality
   - V36-I — Proof, failures, diagnostics, and end-to-end handoff
   - V36-J — Frozen-owner regressions and integrity

8. **Atomic obligations:** 76 contiguous unique definitions, `V36-001` through `V36-076`. No duplicate, missing, out-of-range, or orphan local ID.

9. **Existing procedures repaired:** None.

10. **Exact reusable inventory:** The V36 body and inventory independently expand to the same 114 external identifiers:

   - V20-5/11/15/16/19.
   - V22-C/D/F/G/I/J/K.
   - V24-A/I/L/N.
   - V27-B/D/E/F/H/I/J/K/L/N/Q.
   - V28-E/F/K/L/O.
   - V29-N/O.
   - V30-E/F/I/J/K.
   - V31-A/B/G/N.
   - V32-B/C/H/I.
   - V33-A/C/D/E/F/G/H/I/K/N.
   - V34-B/C/F/G/J/K.
   - V35-001–005, 013–021, 028–033, 040–041, 047–061, and 064–069.
   - V35-A/C/E/F/G/H/I/J family owners.

   Twelve named headings resolve: `Scan and Unary Operator Tests`, `Selectivity Estimation Tests`, `Access Path Tests`, `Physical Property and Enforcement Tests`, `Memory/Spill Plan Tests`, `Memo and Pruning Tests`, `Cost Model Tests`, `Cost Model Benchmarks`, `Optimizer Determinism and Resource-Limit Tests`, `Final Optimizer Validation Tests`, `Optimizer Differential Correctness Tests`, and `Optimizer Diagnostics Tests`.

11. **Event/identity/ownership oracle:** V36-A correlates invocation, logical node, slots/properties, descriptors, StatsVersions, Chapter-35 estimates, CostConfig, workloads, coefficients, local/child ownership, startup/run/total, saturation, objective, proof, selection, and validation.

12. **Positive instrumentation control:** V36-001 follows one real typed query through estimation, SeqScan/IndexScan enumeration, costing, comparison, selection, and final validation.

13. **Missing instrumentation:** V36-002 suppresses each essential evidence category independently. Missing evidence yields `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`.

14. **Fault-path nonvacuity:** V36-003 requires proof that the intended invocation and boundary were reached and the condition actually fired. All later injected-boundary procedures inherit it.

15. **Complete CostConfig inventory:** Individual procedures V36-006–V36-020 cover all 15 fields:

   - Seven conversion coefficients.
   - `effective_cache_pages`.
   - Seven physical-work fallbacks.

16. **Domain/zero-policy coverage:**

| Category | Required domain |
|---|---|
| Seven conversion weights | finite `> 0` |
| Cache pages | unsigned `>= 0`; zero valid |
| Unknown heap pages | unsigned `>= 1` |
| Unknown dead versions | finite `>= 0`; zero valid |
| Minimum physical index entries | unsigned `>= 1` |
| Entry-pressure multiplier | finite `>= 1` |
| Entries per leaf | finite `> 0` |
| Heap rows per page | finite `> 0` |
| Correlation | finite `[-1,1]`; zero valid |

V36-021 covers missing, negative, NaN, infinity, forbidden zero, invalid unsigned, and out-of-range inputs.

17. **Configuration stability:** V36-022 pauses an invocation, replaces external configuration, and proves the active invocation retains one complete validated identity while a later invocation may use the replacement.

18. **Dimensional conversion:** V36-023 independently calculates all seven natural-unit products with deliberately distinct coefficients, checks conversion exactly once, and rejects direct addition of bytes, widths, probabilities, or memory.

19. **MAX_FINITE_COST/numeric guards:** V36-024–V36-026 cover below/at/above-bound values, huge counters, multiplication and composition, component saturation, invalid raw values, and arbitrary-precision reference arithmetic.

20. **Startup/run/total validity:** V36-027 requires finite nonnegative components, saturating total construction, and `startup <= total`, including independently saturated and sum-only-saturated cases.

21. **Saturated comparison/ties:** V36-028 covers ordinary differences, exact and relative ties, one/both saturated alternatives, canonical structural tie-breaking, and a low-cost illegal candidate.

22. **Physical-input precedence:** V36-030 tests only sources permitted by each live §36.5.1 input-map row and distinguishes retained structural facts, compatible statistics, valid-old statistics, derivation, and configuration fallback by identity and provenance.

23. **Heap/dead fallbacks:** V36-031–V36-032 cover no TABLE statistics, missing pages, missing dead pressure, authoritative zero pages, logical estimated zero, large dead pressure, checked version aggregation, and finite SeqScan cost.

24. **Index-entry/leaf fallbacks:** V36-037–V36-039 cover the entry-pressure formula, invisible-entry subtraction, leaf-page ceiling, missing occupancy, valid collected zero occupancy, large values, and structurally empty indexes.

25. **Occupancy/correlation/density:** V36-039–V36-040 test zero/missing/positive occupancy, correlation endpoints and invalid values, known/missing density, scattered/localized access, bounded pages, and zero-safe arithmetic.

26. **B+ descriptor ownership:** V36-036 verifies retained Chapter-8 height and rejects missing, corrupt, or mismatched structural metadata regardless of attractive cost.

27. **Candidate cap as estimate:** V36-041 proves collected or fallback candidate caps affect cost only and cannot limit cursor traversal, heap visits, visibility checks, runtime results, or proof metadata.

28. **SeqScan costability:** V36-046 observes page work, tuple-version work, visible predicate evaluations, output materialization, widths, provenance, and finite startup/run/total for fresh, missing, stale, and zero-estimated inputs.

29. **Index-path costability:** V36-042–V36-043 separately cover point/range descent, leaf work, physical candidates, RID/heap/MVCC work, residual evaluation, decode, locality, pages, and ordering with fresh/stale/missing statistics.

30. **Sargability/bounds:** V36-044–V36-045 integrate equality prefixes, one range, NULL semantics, composite bounds, transient sentinels, non-sargable expressions, exact-bound discharge, residual retention, and evaluation domains.

31. **Enumeration/one-index limit:** V36-047–V36-048 require pre-comparison observation of SeqScan plus every usable single index, reject incompatible indexes, retain missing-statistics paths, and prohibit unsupported index combinations.

32. **Local-work ownership:** V36-050 gives every modeled unit one physical owner across pushed/separate Filter, fused/separate Project, index residuals, Limit counts, and blocking children.

33. **Streaming inventory:** V36-051–V36-057 individually cover `PhysicalValues`, `PhysicalSeqScan`, `PhysicalIndexScan`, `PhysicalFilter`, `PhysicalProject`, `PhysicalLimit`, and supported fused scan/filter/project.

34. **Expression domains:** V36-045 and V36-052–V36-056 distinguish physical versions, visible scan tuples, physical entries, post-MVCC residual candidates, Filter inputs, Project occurrences, survivors, and one count acquisition.

35. **Exactly-once child inclusion:** V36-058 injects omitted child, whole-child-plus-startup duplication, local duplication, and pushed-expression duplication, while preserving specialized repeated work.

36. **Fused accounting:** V36-050/V36-057 require ownership transfer to the fused source and reject both duplicate charging and missing demanded work.

37. **Startup/run/first attempt:** V36-059 covers source setup, first page/candidate/occurrence, early/late/no Filter match, Project evaluation, and estimated-zero inputs. Startup is explicitly not proof of output.

38. **First-row/full-result/first-K:** V36-060–V36-061 compare ALL_ROWS, partial/first-row, exact representable K, unrepresentable K, unknown counts, and approximate/saturated estimates.

39. **Chapter-38 precedence:** V36-062 covers nested-loop/materialization, INLJ repetition, hash build/probe, capability-enabled merge, Sort, aggregation, Top-N, spill, and enforcement without duplicating generic work.

40. **Slots/properties/eligibility:** V36-063 gives the lowest cost to candidates with missing slots/order, unavailable capability, incompatible index, illegal orientation, or ineligible Top-N. They must still be excluded or rejected.

41. **Stable per-object statistics:** V36-034 covers unequal cross-table StatsVersions, self-join aliases, concurrent ANALYZE publication, retained planner inputs, and a later planner selecting the new generation.

42. **Missing/rejected/stale/corrupt classification:** V36-033/V36-066 distinguish no generation, valid old, valid stale, rejected advisory input, malformed catalog framing, invalid index structure, invalid configuration, and unsupported capability.

43. **Estimate/proof boundary:** V36-065 contrasts estimated zeros, stale exclusion, saturation, structural zero, approved proof, and runtime matches/errors. Cost never originates proof.

44. **Error/resource ownership:** V36-066–V36-067 preserve configuration, invariant, advisory fallback, corruption, eligibility, `OptimizerResourceLimit`, runtime allocation/spill, and ordinary SQL error owners.

45. **Determinism/calibration:** V36-029/V36-069 require fixed-input determinism while permitting different valid configurations, samples, generations, cache assumptions, and fallbacks to change costs or plan shape.

46. **Diagnostics/end-to-end handoff:** V36-068–V36-070 correlate inputs, representations, workloads, components, startup/run/total, saturation, objective, plan, validation, and runtime metrics, then differentially check SQL results and errors.

47. **Chapter-31 regression:** V36-071 reuses V31-A/B/G/N; cost cannot alter pre-W closure, canonical error selection, mutation publication, W/C/R, retry, or results.

48. **Chapter-32 regression:** V36-072 reuses V32-B/C/H/I; cost cannot redefine occurrences, claims, replay, legal early stop, or worker-independent semantics.

49. **Chapter-33 regression:** V36-073 covers stable inputs, legal alternatives, bounded search, objectives, dominance, ties, fallback, slots/properties, and validation.

50. **Chapter-34 regression:** V36-074 covers complete per-object generations, valid-old/missing selection, independent StatsVersions, retention, publication, and advisory authority.

51. **Chapter-35 regression:** V36-075 covers representations, logical fallbacks, unknown counts, finite estimates, provenance, exact-K separation, and proof boundaries.

52. **Adversarial matrix:** All cases A–BC are explicitly mapped to controlled V36 procedures and reusable oracles with status `COMPLETE`. Capability-conditional algorithms remain conditional.

53. **All-22-invariant matrix:** Every final §36.19 invariant maps to concrete V36 procedures and an independent oracle. All 22 rows are `COMPLETE`.

54. **Chapter-36 subsection matrix:** Every normative subsection from §36.1 through §36.19, including §§36.2.1, 36.2.2, and 36.5.1, is mapped to exact V36 procedures and reusable coverage. All rows are `COMPLETE`.

55. **Chapter-41 matrix:** Fifteen integration rows cover configuration, arithmetic, stable inputs, physical access work, sargability, alternatives, slots/properties, streaming composition, exact K, specialized algorithms, proof, statistics classification, comparison/fallback, resources, diagnostics, and frozen regressions. All are `COMPLETE`.

56. **Global stale-rule search:** No stale current normative rule required repair. Apparent matches classified as valid different owners/stages, valid approximations, optional capabilities, or explicit negative tests. Existing V31–V35 procedures remain untouched.

57. **ID integrity:** 10 unique families; 76 contiguous atomic definitions; no duplicates, gaps, out-of-range references, or orphan coverage entries.

58. **Broken references:** None. Every cited external ID and all 12 named headings resolve in the live Verification document.

59. **Reuse exhaustiveness:** Independently extracted set A and inventory set B both contain 114 unique external identifiers. `A − B = ∅`, `B − A = ∅`, duplicate inventory entries = 0.

60. **Document role:** V36 is timeless, procedural, implementation-independent, and falsifiable. It introduces no SQL semantics, physical operators, persisted statistics, machine constants, comparator, error enum, grant protocol, proof source, Top-N rule, mandatory optional capability, concrete C++ API, progress claim, or executed-test claim.

61. **New semantic questions:** **NONE**.

62. **git diff --check:** **PASS**.

63. **Verification-only diff:** `docs/VERIFICATION.md`: 316 insertions, 0 deletions. One new V36 section only.

64. **Final repository status:** HEAD unchanged at `ffec26d70440727b2418e6e9e92fe0f61195a370`; ` M docs/VERIFICATION.md`; index clean.

65. **Confirmations:**

```text
ARCHITECTURE NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
CHAPTERS 31–35 NOT MODIFIED
HISTORICAL ARTIFACTS NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
CHAPTER 37 REVIEW NOT STARTED
```

The new procedures have not been executed, and this synchronization does not independently close Chapter-36 Verification.

**CHAPTER 36 VERIFICATION SYNCHRONIZATION COMPLETE —
READY FOR READ-ONLY VERIFICATION CLOSURE AUDIT**