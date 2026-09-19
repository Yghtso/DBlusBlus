# Chapter 36 Architecture Fix A Report

## 1–5. Repository and scope

1. **Initial HEAD:** `1d7e9a809c21794a7e9b25f4c015eabebc52111c`
   Commit: `1d7e9a8 chapter 36 ARCHITECTURE analysis`

2. **Initial status:** worktree clean; index clean.

3. **Pre-existing review directory:** It is no longer untracked. The external HEAD commit added `Chapter 36/ANALYSIS.md`. That historical artifact was not modified.

4. **Final HEAD:** unchanged at `1d7e9a809c21794a7e9b25f4c015eabebc52111c`.
   Final worktree: `M docs/ARCHITECTURE.md`
   Final index: clean.

5. **File modified by this task:** [docs/ARCHITECTURE.md:26462](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26462), Chapter 36 only.

Changed sections:

- §36.1
- §36.2
- new §36.2.1
- new §36.2.2
- §36.3
- §36.5
- new §36.5.1
- §§36.6–36.10
- §36.12
- §§36.14–36.15
- §§36.18–36.19

Chapter 37 now begins at line 27228 and was not modified.

## 6–14. N36-1 — Cost configuration and finite arithmetic

6. **Original defect:** `CostConfig` lacked an exhaustive inventory, dimensional mapping, field domains, zero policy, validation boundary, stable invocation identity and total finite arithmetic.

7. **Final CostConfig inventory:** 15 mandatory baseline fields:

| Category | Fields |
|---|---|
| Scalar conversion weights | `seq_page_cost`, `random_page_cost`, `cpu_tuple_cost`, `cpu_operator_cost`, `hash_cost`, `comparison_cost`, `temp_page_cost` |
| Cache assumption | `effective_cache_pages` |
| Physical fallbacks | `unknown_physical_heap_pages`, `unknown_dead_version_count`, `unknown_index_minimum_physical_entries`, `unknown_index_entry_pressure_multiplier`, `unknown_index_entries_per_leaf`, `unknown_heap_rows_per_page`, `unknown_index_heap_correlation` |

Chapter-38 tie tolerance, memory budgets, hash load factor and spill fanout remain under their existing owners and are not duplicate CostConfig conversion fields.

8. **Dimensional mapping:**

```text
seq_page_reads              * seq_page_cost
random_page_reads           * random_page_cost
(temp_page_reads + writes)  * temp_page_cost
cpu_rows                    * cpu_tuple_cost
cpu_expressions             * cpu_operator_cost
hash_ops                    * hash_cost
compare_ops                 * comparison_cost
```

Bytes, widths, probabilities and memory estimates cannot be added directly to scalar cost. Chapter-38 normalized local costs are not multiplied again.

9. **Numeric domains and zero policy:**

- All seven scalar conversion coefficients: finite `> 0`; zero forbidden.
- `effective_cache_pages`: unsigned `>= 0`; zero means no assumed cache capacity.
- `unknown_physical_heap_pages`: unsigned `>= 1`.
- `unknown_dead_version_count`: finite `>= 0`; zero allowed.
- `unknown_index_minimum_physical_entries`: unsigned `>= 1`.
- `unknown_index_entry_pressure_multiplier`: finite `>= 1`.
- `unknown_index_entries_per_leaf`: finite `> 0`.
- `unknown_heap_rows_per_page`: finite `> 0`.
- `unknown_index_heap_correlation`: finite `[-1,1]`; zero allowed.

10. **Validation and identity:** The complete configuration is validated before physical-alternative costing and remains immutable for the optimization invocation.

11. **Missing/invalid configuration:** Missing fields, negatives, forbidden zeros, NaN and infinity produce the existing Chapter-39 `OptimizerError`/internal configuration failure before comparison. There is no silent default or new error enum.

12. **Finite construction:** The repair defines one positive cost-domain `MAX_FINITE_COST`. Checked arithmetic is required for:

- integer conversions;
- additions and multiplications;
- component counters;
- page/candidate calculations;
- weighted sums;
- repeated composition;
- startup plus run.

Valid overflow saturates. Invalid negative or nonfinite input is rejected rather than saturated. Saturation is diagnostic estimate metadata, not proof or resource exhaustion.

13. **Startup/run/total consistency:**

```text
total_cost = saturating_add(startup_cost, run_cost)
```

All three values remain finite and nonnegative, with `startup_cost <= total_cost`. Chapter 38 retains objective, dominance and tie ownership.

14. **N36-1 adversarial cases:**

| Case | Final outcome |
|---|---|
| Missing coefficient | `OptimizerError` before comparison |
| Negative coefficient | Rejected |
| NaN random-page coefficient | Rejected |
| Infinite CPU coefficient | Rejected |
| Deliberately permitted zero | Accepted only for cache/dead-count/correlation fields |
| Forbidden zero | Rejected |
| Invalid cache pages | Rejected at configuration validation |
| Huge page count | Checked conversion; finite result or saturation |
| Huge candidates × page cost | Checked multiplication; finite result or saturation |
| Repeated additions | Saturating checked composition |
| Saturated cardinality input | Remains approximate; checked cost conversion |
| Weighted sum exceeds domain | `MAX_FINITE_COST` |
| Startup plus run exceeds domain | Total saturates; no wrap |
| Different legal calibrations | Different valid costs permitted |
| Repeated fixed-input planning | Stable configuration and deterministic arithmetic path |

**N36-1: CLOSED**

## 15–27. N36-2 — Physical-input fallbacks

15. **Original defect:** Missing TABLE/INDEX statistics left mandatory physical work quantities undefined.

16. **Physical-input inventory:** The chapter now identifies sources and fallbacks for:

- logical/base rows;
- physical heap pages;
- dead versions;
- physical tuple versions;
- B+ height;
- logical live index entries;
- physical index entries;
- invisible entries;
- leaf pages and entries per leaf;
- index/heap correlation;
- heap rows per page.

17. **Precedence:**

1. retained authoritative physical metadata, when it owns the quantity;
2. selected complete compatible Chapter-34 generation, including valid-old selection;
3. stable derivation from already selected inputs;
4. validated CostConfig physical fallback.

No mixed generations, synchronous ANALYZE, mutable BufferPool inspection or mid-invocation refresh is allowed.

18. **Missing heap pages:** Uses positive `unknown_physical_heap_pages`. An authoritative physical page count of zero remains valid for a physically empty heap.

19. **Missing dead-version input:** Uses finite nonnegative `unknown_dead_version_count`; physical tuple versions use a checked finite sum with selected live rows.

20. **Missing index entry/leaf inputs:**

```text
physical_entry_count
≈ max(
    unknown_index_minimum_physical_entries,
    logical_live_entry_count
      * unknown_index_entry_pressure_multiplier
)

invisible_entry_count_estimate
= max(0, physical_entry_count - logical_live_entry_count)

leaf_page_count
≈ max(
    1,
    ceil(physical_entry_count
         / unknown_index_entries_per_leaf)
)
```

21. **Missing occupancy/correlation:**

- Missing or zero unusable occupancy uses `unknown_index_entries_per_leaf`.
- Missing correlation uses `unknown_index_heap_correlation`.
- A valid collected `average_entries_per_leaf == 0` no longer creates division by zero or invalidates the generation.
- Missing correlation is not treated as perfect locality.

22. **B+ height:** Remains an authoritative retained Chapter-8 descriptor/metadata fact, not advisory statistics. An index without valid required structural metadata is not usable.

23. **Provenance:**

- valid selected statistics retain StatsVersion and stale provenance;
- absent/rejected/incompatible advisory data uses LOW confidence and `MISSING_STATISTICS`;
- the diagnostic also identifies the configured physical assumption;
- descriptor-derived facts retain descriptor identity;
- no new public provenance enum was introduced.

24. **SeqScan costability:** Every legal SeqScan receives finite page, tuple-version, predicate and decode inputs even without ANALYZE.

25. **Index costability:** Every semantically usable single-index path receives finite pressure, occupancy, correlation and heap-locality inputs. Missing advisory statistics do not make it ineligible.

26. **Logical versus physical pressure:** Logical candidates, physical entries, invisible entries, RID candidates, visibility checks and expected distinct heap pages remain separate. The existing candidate-inflation formula is preserved.

27. **N36-2 adversarial cases:**

| Case | Final result |
|---|---|
| Unanalyzed table | Chapter-35 rows plus Chapter-36 physical fallbacks |
| Logical rows available, pages missing | Positive page fallback |
| Logical estimate zero, heap has pages | Physical pages still costed; no proof |
| Pages known, dead estimate missing | Known pages plus configured dead count |
| Valid older generation | Selected before fallback |
| Valid stale generation | Used with stale provenance |
| Malformed newest generation | Chapter-34 valid-old/missing selection |
| Legal index without IndexStatistics | Descriptor height plus configured/derived pressure |
| Height known, pressure absent | Height retained; other values derived |
| Correlation missing | Configured correlation assumption |
| Occupancy missing or zero | Positive configured entries-per-leaf |
| Candidate estimate zero, runtime match | Path remains executable |
| Physically known empty heap | Authoritative zero pages permitted |
| Incompatible index descriptor | Ineligible under Chapters 22/34 |
| SeqScan and index need different fallbacks | Each resolves independently under §36.5.1 |

**N36-2: CLOSED**

## 28–40. N36-3 — Streaming cost composition

28. **Original defect:** Child inclusion, local work, residual evaluation domains and startup/run placement were underdefined.

29. **Local-work ownership:** Every modeled unit has one physical owner. Pushed/fused work belongs to the scan; retained Filter/Project work belongs to that separate operator.

30. **Parent/child inclusion:** Ordinary streaming unary nodes include the child objective exactly once and add only their local work. Specialized repeated/blocking formulas override the generic transfer.

31. **Mandatory streaming transfer table:** Added explicit rules for:

- `PhysicalValues`
- `PhysicalSeqScan`
- `PhysicalIndexScan`
- `PhysicalFilter`
- `PhysicalProject`
- `PhysicalLimit`
- fused scan/filter/project forms

No generic binary formula was invented.

32. **Evaluation-row bases:**

- heap visibility/header: physical tuple versions;
- scan-pushed predicates: SQL-visible tuples examined;
- index residuals: visible candidates reaching the predicate;
- predicates evaluated on index entries: corresponding physical-entry domain;
- Filter predicates: input occurrences presented to the Filter;
- projection expressions: one-to-one processed occurrences;
- output-only decode/materialization: surviving output rows;
- count-expression acquisition: exactly once where execution-start evaluation applies.

33. **Projection/materialization ownership:** Scan-fused work and separate Project work are mutually exclusive owners. Required predicate/output/erroring expressions remain demanded.

34. **Startup:** Includes child startup, fixed local setup and bounded first-attempt work. Source startup includes cursor positioning and first page/tuple or index descent/candidate attempt.

35. **Run:** Includes the child run under the propagated objective and remaining local work.

36. **Total:** Uses checked saturating startup-plus-run composition.

37. **First-row/first-K regression:** Selectivity-based partial consumption remains §38.16-owned. Unknown counts do not create exact K; Top-N eligibility remains unchanged.

38. **No omission or duplication:** Child omission, duplicate child charging and duplicate pushed-expression charging are explicitly prohibited.

39. **Chapter-38 precedence:** Hash build/probe, nested-loop rescans/materialization, merge prerequisites, Sort, aggregate, Top-N, spill and property enforcement retain their specialized formulas.

40. **N36-3 adversarial cases:**

| Case | Final rule |
|---|---|
| One-row Values | First declared occurrence charged in startup |
| Erroring Values expression | Remains demanded and owned by Values |
| SeqScan pushed predicate | Charged on examined visible tuples |
| Separate Filter | Child once; Filter predicate once |
| Many index candidates, few visible rows | Physical access charged before residual/output work |
| Expensive residual | Charged on candidates reaching it |
| Residual before final survivor selection | Not undercounted as output-only work |
| Expensive Project expression | Charged per Project occurrence |
| Fused projection | Scan owns it; removed Project does not |
| Execution-start Limit count | Acquisition/validation charged once in startup |
| Same Filter/Project over alternate children | Each alternative includes its child once |
| Parent omits child | Violates mandatory composition |
| Parent doubles child | Violates mandatory composition |
| Blocking child, streaming parent | Child decomposition preserved; blocker rule remains authoritative |
| Numerical-zero child without proof | Fixed setup and executable path remain |
| First-row comparison | Uses startup plus Chapter-38 partial run |
| Full-result comparison | Uses total cost |
| Nested-loop rescan | §38.10/§38.11 override |
| Sort | Blocking §38.13–§38.16 rules override |
| Invalid arithmetic during composition | Rejected or valid overflow saturated under §36.2.1 |

**N36-3: CLOSED**

## 41–44. N36-4 and invariants

41. **Original phrases:**

- “The initial cost model…”
- “unless a future expression-index architecture…”

42. **Final wording:**

- “The v1 baseline model…”
- “unless an optional expression-index capability is defined and enabled.”

No formulas or capability requirements changed.

43. **Existing invariants:** All original 17 meanings remain intact.

44. **Added invariants:** Five supporting invariants were appended, producing **22 total**:

18. stable validated CostConfig and dimensional conversions;
19. checked saturation and proof/resource separation;
20. finite physical fallbacks retain legal path costability;
21. one work owner and exactly-once ordinary child inclusion;
22. correct evaluation domains without changing demand, exact K or properties.

They were added because the repaired contracts otherwise would not be reflected in the chapter’s invariant summary.

**N36-4: CLOSED**

## 45–56. Regression and document audit

45. **Numeric-hazard search:** Resolved guards now cover configuration NaN/infinity/negatives, overflow, startup-plus-run overflow, cache zero, candidate multiplication, zero logical-live denominator, zero occupancy divisor and repeated composition. No remaining Chapter-36 numerical defect was found.

46. **Missing-input search:** Heap pages, dead pressure, physical index entries, live entries, invisible entries, leaf pages, occupancy, correlation and rows-per-page now have selected sources or finite fallbacks. B+ height correctly remains structural descriptor metadata.

47. **Demand/proof regression:** Costs and fallbacks cannot prove emptiness, uniqueness, absence, visibility or exact K. Estimated zero cannot remove demanded expressions or runtime access.

48. **Chapter 31:** Candidate closure, ordinary-error reduction, W/C/R and mutation ownership unchanged.

49. **Chapter 32:** Occurrence identity, claims, replay, early stop and worker-independent semantics unchanged.

50. **Chapter 33:** Stable planning inputs, bounded legal selection, objective, dominance, ties and validation unchanged.

51. **Chapter 34:** Complete compatible generations, valid-old/missing selection, per-object StatsVersions and advisory authority unchanged.

52. **Chapter 35:** Cardinality, width units, 3VL, estimator fallbacks, unknown counts and proof separation unchanged.

53. **Chapter 38:** Specialized blocking/repeated-work formulas, comparator, exact-K rules, memory/spill ownership, bounded fallback and validation remain authoritative. No contradiction was found.

54. **Document role:** The additions are timeless, implementation-independent and configuration-driven. No machine constants, progress claims, benchmark results or new framework were introduced.

55. **Contradiction search:** Apparent overlaps were valid different owners or stages:

- Chapter 35: logical estimates;
- Chapter 36: scalar conversion, base physical work and generic streaming composition;
- Chapter 38: specialized algorithm costs, objectives and comparison;
- Chapter 39: optimizer/configuration failures;
- Chapter 40: diagnostics.

No true contradiction remains.

56. **New semantic questions:** NONE.

## 57–60. Validation and final state

57. `git diff --check`: **PASS**

58. **Repair-only diff:** `docs/ARCHITECTURE.md` only; 278 insertions, 13 deletions. Every hunk is within Chapter 36.

59. **Final status:**

```text
 M docs/ARCHITECTURE.md
```

Index remains clean. HEAD remains `1d7e9a809c21794a7e9b25f4c015eabebc52111c`.

60. **Confirmations:**

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- CHAPTERS 31–35 NOT MODIFIED
- CHAPTER 37 NOT MODIFIED
- CHAPTER 38 NOT MODIFIED
- HISTORICAL ARTIFACTS UNTOUCHED
- PRE-EXISTING REVIEW DIRECTORY PRESERVED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT
- CHAPTER 37 REVIEW NOT STARTED

## Finding disposition

- N36-1: **CLOSED**
- N36-2: **CLOSED**
- N36-3: **CLOSED**
- N36-4: **CLOSED**

CHAPTER 36 ARCHITECTURE FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT