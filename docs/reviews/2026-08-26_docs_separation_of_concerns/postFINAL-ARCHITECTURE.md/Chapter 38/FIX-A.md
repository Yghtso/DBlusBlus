# Chapter 38 Architecture Fix A report

1. **Initial HEAD and commit:** `a268d9954a3d5a9e12f1e93cbff44eee9493fcc6` — `a268d99 chapter 38 ARCHITECTURE analysis`.

2. **Initial worktree/index:** clean. The newer commit added only the historical `Chapter 38/ANALYSIS.md` artifact; it made no normative Architecture change.

3. **Final HEAD/status:** HEAD remains `a268d9954a3d5a9e12f1e93cbff44eee9493fcc6`. Worktree has one unstaged modification: `docs/ARCHITECTURE.md`. Index remains clean.

4. **Task-modified files:** only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

5. **Changed sections and live lines:**

   - [§38.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27917), lines 27917–27987.
   - [§38.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27988), lines 27988–28058.
   - [§38.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28127), lines 28127–28235.
   - [§38.19](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28655), lines 28655–28728.
   - [§38.21](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28755), lines 28755–28831.
   - [§38.25](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28916), invariants 4, 5, 6, 10, and 13 only.

6. **Diff count:** 286 insertions, 39 deletions.

## N38-1 — Cost comparison

7. **Original defect:** §38.4 lacked a complete configuration contract and used a nontransitive pairwise approximate-tie relation that could make insertion order affect selection.

8. **Epsilon contract:** `cost_tie_relative_epsilon` is mandatory finite binary64 in `[0,1)`, with zero valid and default `1e-9`.

9. **Validation and retention:** missing, negative, NaN, infinite, `>=1`, or unrepresentable values produce `OptimizerError` before comparison. No clamping occurs. The validated value remains fixed for one optimizer invocation.

10. **Safe arithmetic:** tolerance uses checked multiplication and saturating addition in Chapter 36’s finite domain:

    `scale=max(1,m)`, `tolerance=checked_multiply(epsilon,scale)`, `tie_limit=saturating_add(m,tolerance)`.

11. **Total-selection procedure:** find the exact global minimum `m`; form one anchored tie window ending at `tie_limit`; sort that window by the complete structural key; order remaining candidates by exact objective cost and then structural key.

12. **Nontransitivity resolution:** for `a=100`, `b=109`, `c=119`, and epsilon `0.1`, the old pairwise rule ties `a/b` and `b/c` but not `a/c`. The repaired rule anchors at `100`, admits only `a` and `b` to the window ending at `110`, and gives the same result for every insertion order.

13. **Dominance compatibility:** cost-based pruning must preserve the candidate that the completed anchored comparison would select. Incremental implementations must retain candidates or an equivalent sufficient summary until the anchor and structural minimum are fixed. Independent semantic, property, capability, and feasibility exclusions remain valid.

14. **Saturation:** valid saturated costs remain ordinary estimate metadata. Two saturated costs use the structural key. An ordinary cost precedes a saturated cost unless the anchored window legitimately includes both. Invalid raw costs remain `OptimizerError`/internal failures.

15. **Comparator adversarial results:**

| Cases | Result |
|---|---|
| A equal costs | Structural key selects deterministically. |
| B outside tolerance | Lower exact objective cost wins. |
| C inside tolerance | Structural key selects inside the anchored window. |
| D–G nontransitive chain/order/container changes | Fixed global anchor produces one result. |
| H epsilon zero | Only exact equality ties. |
| I default epsilon | Valid and retained. |
| J epsilon near one | Valid only while finite and `<1`; anchored procedure remains total. |
| K–O missing/negative/NaN/infinite/unrepresentable | `OptimizerError` before comparison. |
| P invalid raw cost | Rejected under §36.2.1. |
| Q saturated versus ordinary | Exact order unless both enter the anchored window. |
| R both saturated | Structural key decides. |
| S near-overflow tolerance | Checked arithmetic; no nonfinite intermediate. |
| T ALL_ROWS versus FIRST_K_ROWS | Separate active objectives/search identities, not mixed candidate sets. |
| U low-startup alternative | Preserved under the finite-row objective and §38.3. |
| V allocator changes | No effect. |

16. **N38-1 disposition:** **CLOSED**.

## N38-2 — Structural identity

17. **Original defect:** the structural key could collide for distinct relation occurrences, predicate occurrences, slot mappings, join kinds, exact K values, and other physical parameters.

18. **Complete field inventory:** the full key now covers operator and capability variant, logical subproblem where relevant, `BindingId`, stable table/index/access identities, join kind/algorithm/orientation, predicate occurrence and referenced `RelationSet`, logical slots and mappings, bounds and scan direction, normalized properties and sort keys, exact LIMIT/OFFSET and Top-N K, plan-local row objectives, physical modes, and recursive child keys.

19. **Composition:** one recursively tagged tuple with fixed field order, type tags, explicit optional presence, unambiguous sequence framing, canonical set ordering, ordered children, and exact numeric values. Comparison is lexicographic over the complete tuple.

20. **Self-join result:** equal `TableId`/`IndexId` leaves with different `BindingId`s have unequal keys.

21. **Predicate/slot/join/K results:** distinct predicate occurrences, referenced sets, attachment nodes, slot identities/mappings, join kinds, orientations, bounds, directions, order fields, and exact K values remain distinct.

22. **Fingerprint separation:** FNV-1a-64 remains optional and diagnostic. A collision cannot merge alternatives, decide a tie, establish equivalence, or bypass validation.

23. **Structural adversarial results:**

| Case | Result |
|---|---|
| A–B same table/index, different occurrence | Unequal by `BindingId`. |
| C same display name, different slots | Unequal by `LogicalSlotId`/mapping. |
| D INNER versus LEFT | Unequal join-kind tag. |
| E different orientation | Unequal ordered child/orientation fields. |
| F–H predicate occurrence/multiplicity | Unequal occurrence-tagged predicate sequence. |
| I different attachment node | Unequal recursive operator placement. |
| J different output mapping | Unequal mapping field. |
| K–L slot/order identity differences | Unequal normalized property fields. |
| M–N bounds/direction | Unequal access fields. |
| O K=10 versus K=11 | Unequal exact-K field. |
| P different objective | Unequal where plan-local; otherwise separated by memo/search identity. |
| Q ambiguous concatenation | Prevented by tags and framing. |
| R absent versus present parameter | Unequal presence tag. |
| S construction field order | Canonicalized to fixed field order. |
| T fingerprint collision | Full keys still compare independently. |
| U–V pointer/container changes | No effect. |

24. **N38-2 disposition:** **CLOSED**.

## N38-3 — Execution-memory targets

25. **Original defect:** simultaneous blockers could receive arbitrary valid-looking allocations such as 50/50, 80/20, or 20/80.

26. **Budget owner/name:** `query_execution_memory_budget_bytes` names the retained Chapter-24 per-query soft execution-memory budget exposed through §33.3. It is distinct from the optimizer planning arena.

27. **Initial allocation:** integer-byte demand-capped equal sharing among simultaneously live blockers.

28. **Redistribution and rounding:** remainders go to the earliest canonical occurrence keys. Capped unused shares are repeatedly redistributed until no demand or budget remains.

29. **Phase construction:** Chapter 26 dependencies determine coexistence. Shared state is counted once per phase; sequential blockers occupy separate phases. Repeated equal-looking nodes use root-to-node child ordinals plus their structural key.

30. **Peak/spill:** every phase sum is at most the execution-memory budget. Peak is the maximum phase sum. A blocker live in several phases uses the minimum phase target for conservative operator costing.

31. **80/80 under budget 100:** exact allocation is `50/50`; unused budget is zero; both predict spill because `80>50`.

32. **Sequential versus simultaneous:** two sequential blockers needing 80 each receive 80 in their respective phases and produce peak 80, not 160. Concurrent blockers share the budget.

33. **Runtime distinction:** planner targets reserve nothing. `QueryMemoryManager`, hard gates, runtime spill, and runtime allocation errors remain authoritative.

34. **Memory adversarial results:**

| Case | Allocation/outcome |
|---|---|
| A 80, budget 100 | 80; 20 unused; no predicted spill. |
| B 80/80, budget 100 | 50/50; both spill. |
| C 10/80, budget 100 | 10/80; 10 unused. |
| D 10/80, budget 50 | 10/40; second spills. |
| E three unequal blockers | Deterministic repeated equal sharing; structural order resolves byte remainders. |
| F sequential 80/80 | Separate 80-byte targets; peak 80. |
| G zero plus positive need | Zero receives zero; positive receives up to budget/need. |
| H total need below budget | Every need satisfied; remainder unassigned. |
| I total equals budget | Exact needs; no remainder. |
| J total exceeds budget | Entire budget allocated; spill follows strict need/target comparison. |
| K zero budget | All targets zero; positive needs predict spill. |
| L saturated need | Finite saturated byte demand participates without wraparound. |
| M indivisible budget | Earliest canonical occurrence keys receive remainder bytes. |
| N reversed insertion order | No effect. |
| O equal node keys | Root-to-node occurrence path breaks the tie. |
| P blocker in multiple phases | Per-phase targets; conservative minimum used by operator costing. |
| Q runtime grant denied | Chapter-24 spill/resource outcome. |
| R actual need exceeds estimate | Runtime accounting/spill handles it; planning estimate is not a grant. |

35. **N38-3 disposition:** **CLOSED**.

## N38-4 — Planning-resource guard

36. **Original defect:** no canonical resource input or trigger distinguished arena capacity from work counters, wall time, or other resources.

37. **Planning configuration:** `optimizer_planning_arena_budget_bytes`, owned by retained Chapter-33 planning/search configuration.

38. **Domain/default/zero:** mandatory representable unsigned byte count; zero is valid. Deployment supplies its configured default because no machine-independent byte default is frozen. Missing, negative, or unrepresentable values are invalid.

39. **Validation/stability:** malformed values produce `OptimizerError` before search-state construction. The exact value is retained for one invocation.

40. **Guard trigger:** immediately before an arena reservation/allocation/growth would raise charged live bytes above the budget. Equality is allowed. Checked arithmetic is mandatory.

41. **Wall time/counters:** wall time, subproblems, partitions, alternatives, and memo counts are diagnostics only. They do not trigger fallback.

42. **Partial memo:** each region starts from an arena checkpoint. On an exhaustive guard hit, all region-specific partial exhaustive state—including completed alternatives—is discarded and the arena rolls back.

43. **Fallback initiation:** bounded search restarts from immutable retained logical, descriptor/statistics, configuration, objective, property, and capability inputs within the same optimizer invocation.

44. **Resource-limit boundary:** if bounded planning cannot initialize or finish within the valid bound, it returns `OptimizerResourceLimit`. Malformed configuration remains `OptimizerError`.

45. **Chapter-37 regression:**

   - `N=9`, limit 10, guard clear: exhaustive.
   - `N=10`, limit 10, guard clear: exhaustive.
   - `N=11`, limit 10: heuristic from outset.
   - `N=1`, limit 0: heuristic from outset.
   - At threshold with guard triggered: clean bounded fallback.
   - Threshold proximity/equality alone never triggers fallback.

46. **Planning-resource adversarial results:**

| Case | Outcome |
|---|---|
| A configured default present | Valid retained budget. |
| B–C missing/negative | `OptimizerError`. |
| D zero | Valid; likely `OptimizerResourceLimit` if required state cannot fit. |
| E unrepresentable | `OptimizerError`. |
| F huge representable | Valid; checked accounting still applies. |
| G–H below/equal threshold, guard clear | Exhaustive remains active. |
| I above threshold | Heuristic from outset. |
| J exhaustive guard hit | Rollback and bounded restart. |
| K before useful DP state | Clean bounded restart. |
| L after completed entries | Entries discarded; clean restart. |
| M midway through partition | Partial partition discarded. |
| N bounded fits | Legal bounded plan continues to validation. |
| O–P bounded cannot initialize/exhausts | `OptimizerResourceLimit`. |
| Q external change mid-plan | Retained value remains in force. |
| R diagnostic counters high | No fallback while byte guard is clear. |
| S wall time varies | No search-mode effect. |
| T high estimated plan cost | Not a resource trigger. |
| U arena arithmetic unrepresentable | `OptimizerError`/internal invariant failure. |
| V runtime memory failure | Chapter-24 runtime owner, not planning configuration. |

47. **N38-4 disposition:** **CLOSED**.

## N38-5 — Hash/spill configuration

48. **Original defect:** hash load factor, spill fanout, and recursion bounds lacked complete identity, domains, validation, and bounded prediction.

49. **Runtime owner:** Chapter 28’s retained hash runtime-capability configuration.

50. **Load-factor contract:** `hash_directory_target_load_factor`, finite binary64 in `(0,1]`. Chapter 28’s approximately `0.70` value is the initial tuning baseline; costing consumes the exact retained runtime setting.

51. **Spill configuration:**

   - `hash_spill_max_partition_fanout`: representable unsigned power of two, at least 2.
   - `hash_spill_max_recursive_repartition_passes`: representable unsigned integer, at least 0; zero permits initial partitioning but no recursive pass.
   - Per-pass fanout is the smallest power of two satisfying predicted/actual pressure, capped by the configured maximum.

52. **Validation/retention:** all three are mandatory for an enabled spill-capable hash implementation, validated before costing, retained for one invocation, and not Chapter-36 conversion coefficients.

53. **Recursive predictor:** charge initial partitioning, reduce predicted largest partition using selected fanout and optional validated skew, charge every pass, then stop on fit, configured pass bound, or no predicted progress. Bound/no-progress uses the existing §28.11 correctness fallback cost.

54. **Numeric safety:** checked byte/counter operations, checked ceiling division and power-of-two selection, positive denominator, finite saturation, no wraparound, and no semantic inference from estimated zero.

55. **Runtime distinction:** the planner and runtime use one selection policy but may choose different fanouts from estimated versus actual pressure. Runtime reservations, spill, skew handling, and errors remain authoritative.

56. **Hash/spill adversarial results:**

| Case | Outcome |
|---|---|
| A valid retained configuration | Costing proceeds. |
| B missing load factor | `OptimizerError`. |
| C–D zero/negative load factor | Invalid. |
| E load factor >1 | Invalid. |
| F–H NaN/infinite/unrepresentable | Invalid. |
| I valid near-zero load factor | Valid; estimate may saturate, never divide by zero. |
| J load factor 1 | Valid upper bound. |
| K runtime/costing target mismatch | Invalid configuration/implementation inconsistency; not silently accepted. |
| L need below target | No predicted spill. |
| M need equals target | No predicted spill. |
| N need exceeds target | Spill predicted and charged. |
| O missing fanout | Invalid. |
| P fanout zero | Invalid. |
| Q fanout one | Invalid because recursive progress cannot be guaranteed. |
| R valid power-of-two fanout | Accepted. |
| S unrepresentable fanout | Invalid. |
| T recursive limit zero | Valid; initial partitioning followed by fallback if still nonfitting. |
| U pass limit reached | Stop recursion and charge controlled fallback. |
| V pathological skew/no reduction | Stop and charge fallback; no infinite recursion. |
| W unknown skew | Uniform low-confidence estimate; never semantic proof. |
| X saturated memory estimate | Remains finite and predicts pressure/spill. |
| Y concurrent ANALYZE | Retained statistics/configuration identity remains unchanged. |
| Z unexpected runtime spill | Runtime manager and Chapter-28 spill path remain authoritative. |

57. **N38-5 disposition:** **CLOSED**.

## Invariants and regressions

58. **All 19 invariants:** count remains 19 and numbering remains contiguous.

59. **Invariant changes:**

| Invariant | Status |
|---|---|
| 1 | Unchanged: complete memo/search identity. |
| 2 | Unchanged: finite row objective in identity. |
| 3 | Unchanged: useful ordering/low-startup retention. |
| 4 | Refined: validated tolerance and anchored total ordering. |
| 5 | Refined: insertion, pointer, allocator, container, and scheduling independence. |
| 6 | Refined: complete tagged structural key; diagnostic fingerprint only. |
| 7 | Unchanged: shared base cardinality. |
| 8 | Unchanged: algorithm-independent join cardinality. |
| 9 | Unchanged: capability gating. |
| 10 | Refined: validated hash/spill inputs, deterministic targets, bounded spill prediction. |
| 11 | Unchanged: enforcement costs. |
| 12 | Unchanged: full versus first-K objectives. |
| 13 | Refined: validated byte guard, clean restart, controlled bounded exhaustion. |
| 14 | Unchanged: stable statistics snapshot. |
| 15 | Unchanged: advisory missing/stale statistics. |
| 16 | Unchanged: diagnostics. |
| 17 | Unchanged: SQL meaning preserved. |
| 18 | Unchanged: final validation. |
| 19 | Unchanged: estimated zero remains non-semantic. |

60. **Chapter 31:** no edits; DML target state, candidate closure, W/C/R, errors, and publication remain intact.

61. **Chapter 32:** no edits; source/worker identity, coverage, replay, and early-stop rules remain intact.

62. **Chapter 33:** one retained invocation, stable inputs, bounded planning, active objective, and final validation remain intact.

63. **Chapter 34:** retained compatible per-object statistics generations remain intact.

64. **Chapter 35:** logical cardinality, widths, finite estimates, and proof separation remain intact.

65. **Chapter 36:** the seven cost coefficients, `MAX_FINITE_COST`, saturation, and work ownership remain intact. No coefficient was added.

66. **Chapter 37:** no edits; exact threshold equality, Cartesian admission, predicate ownership, deterministic heuristic, and zero-pass behavior remain intact.

67. **Chapter 39:** malformed configuration uses `OptimizerError`; exhausted bounded planning uses `OptimizerResourceLimit`; runtime memory/spill failures retain runtime ownership. No error enum was added.

68. **Global consistency search:** all relevant matches were inspected. Chapter 28’s approximate `0.70` target is a valid runtime-owner baseline; Chapter 33’s budget split is consistent; Chapter 36’s delegation of tolerance/hash/spill inputs is consistent; Chapter 37’s exact threshold is preserved; Chapter 40 references are diagnostic-stage uses. No remaining contradiction was found.

69. **Document role/minimality:** the repair is timeless, implementation-independent, bounded, and deterministic. It adds no SQL semantics, physical operator, property dimension, cost coefficient, proof source, optimizer framework, persisted format, or public error.

70. **Remaining findings:** none within N38-1 through N38-5.

71. **New semantic questions:** none.

72. **Exact diff scope:** every hunk is inside the six authorized Chapter-38 locations. Chapter 38 remains bounded by line 27826 and the Chapter-39 boundary at line 28941.

73. **`git diff --check`:** passed with no output.

74. **Final repository state:** HEAD unchanged; one unstaged `docs/ARCHITECTURE.md` modification; index clean.

Explicit confirmations:

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- CHAPTERS 31–37 NOT MODIFIED
- CHAPTER 39 ONWARD NOT MODIFIED
- HISTORICAL ARTIFACTS NOT MODIFIED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT
- CHAPTER 39 REVIEW NOT STARTED

Finding dispositions:

- N38-1: **CLOSED**
- N38-2: **CLOSED**
- N38-3: **CLOSED**
- N38-4: **CLOSED**
- N38-5: **CLOSED**

Chapter 38 Architecture has not been independently declared closed.

**CHAPTER 38 ARCHITECTURE FIX A COMPLETE —
READY FOR INDEPENDENT READ-ONLY CLOSURE AUDIT**