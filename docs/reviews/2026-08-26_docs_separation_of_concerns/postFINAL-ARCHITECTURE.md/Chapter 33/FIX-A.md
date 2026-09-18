# Chapter 33 Architecture Fix A Report

1. **Initial HEAD/status**

- HEAD: `332f2881026507aaeb135cb62ed3c4643242f151`
- Commit: `332f288 chapter 33 ARCHITECTURE analysis`
- Working tree: clean
- Index: clean

2. **Final HEAD/status**

- HEAD unchanged: `332f2881026507aaeb135cb62ed3c4643242f151`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean

3. **Files modified**

- [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24121) only.

4. **Chapter-33 sections changed**

- §33.1 Role
- §33.2 Layering
- §33.3 Planning inputs
- §33.7 Optimizer invariants

All changed hunks are within Chapter 33. Chapter 34 now begins at line 24352.

5. **N33-1 defect and canonical owner**

The unqualified `lowest-cost valid PhysicalPlan` at former line 24160 appeared to require a global optimum.

Canonical owners §§37.11, 37.13, 38.3–38.4, 38.15, 38.21, and 38.24 instead permit exhaustive or bounded search according to configured limits, followed by canonical cost comparison and final validation.

6. **Final bounded-search choice contract**

§33.1 now requires:

- legal, capability-enabled alternatives;
- the applicable exhaustive or bounded Chapters 37–38 search path;
- canonical active-objective comparison, dominance, and tie rules;
- canonical planning-resource fallback;
- §38.24 final validation;
- immutable plan handoff.

It explicitly disclaims exhaustive enumeration and a global minimum outside the explored search space.

7. **Active-objective and cost-comparison preservation**

The selected plan still follows Chapter 38’s active objective among retained legal alternatives. The repair does not permit arbitrary valid-plan selection.

8. **Tie-rule preservation**

Deterministic Chapter-38 tie rules remain authoritative. No second tolerance, ordering, or tie-break rule was introduced.

9. **Exhaustive versus bounded search**

- Small eligible regions may use exhaustive bushy DP.
- Large regions or planning-memory pressure use the bounded deterministic heuristic.
- Neither bounded search nor fallback must compare against unexamined alternatives.

10. **Retained/explored alternative distinction**

Selection operates on legal alternatives admitted, explored, and retained by the applicable canonical search path. An unexamined plan is not treated as an available comparison candidate.

11. **Resource-bound and fallback preservation**

Chapter 38 remains the sole owner of:

- exhaustive-search limits;
- planning-arena limits;
- bounded heuristic fallback;
- controlled failure when bounded planning cannot fit.

12. **No-legal-plan/error behavior**

The repair creates no fallback or error type. Invalid or unavailable alternatives cannot be selected. Resource exhaustion after canonical bounded fallback remains `OptimizerResourceLimit`; all other no-plan conditions retain their existing canonical owner.

13. **Final plan validation preservation**

The planning flow now explicitly includes final `PhysicalPlan` validation before immutable handoff. Missing slots, unsatisfied properties, unavailable operators, and invalid semantic proofs remain rejection conditions under §38.24.

14. **N33-1 thought-experiment matrix**

| Case | Canonical outcome | Final wording |
|---|---|---|
| Small exhaustive join graph | Compare retained legal alternatives under active objective | Consistent |
| Large bounded join graph | Deterministic heuristic may return a valid non-global optimum | Consistent |
| Cheaper unexamined plan | No required comparison against it | Consistent |
| Two retained alternatives | Active objective selects lower-cost alternative | Consistent |
| Canonical tie | Chapter-38 deterministic structural tie rule | Consistent |
| Budget exhausted after legal plan found | Canonical fallback/search path decides retained result | Consistent |
| §38.21 fallback activated | Bounded heuristic applies | Consistent |
| Bounded planning cannot fit | `OptimizerResourceLimit` | Consistent |
| Cheap unavailable operator | Ineligible and excluded | Consistent |
| Missing required slot | Rejected by final validation | Consistent |
| Unsatisfied ordering | Enforce property or reject invalid final plan | Consistent |

**N33-1: CLOSED**

15. **N33-2 defect and canonical owner**

§33.3 listed execution-memory and cost configuration without explicitly listing the separate optimizer planning/search resource configuration required by §38.21 and classified by §39.4.

16. **Final planning-resource input**

The input list now includes:

`optimizer planning/search configuration and resource limits`

17. **Planning resources versus execution memory**

The text now distinguishes:

- planning/search limits and the dedicated planning arena;
- Chapter-24 query execution-memory budget;
- runtime memory/spill estimates;
- actual runtime reservation, allocation, spill, and cleanup.

18. **Planning estimate versus physical allocation**

Estimated peak memory and spill behavior are explicitly planning predictions. Neither an estimate nor a budget grant proves successful physical allocation.

19. **OptimizerResourceLimit preservation**

The document now directly preserves §39.4: if bounded planning cannot fit within the configured planning resource limit, `OptimizerResourceLimit` applies.

20. **N33-2 resource-scenario matrix**

| Scenario | Required owner/outcome |
|---|---|
| Fixed execution budget; reduced planning bound | Search may fall back or report `OptimizerResourceLimit`; execution budget is unchanged |
| Fixed planning bound; reduced execution budget | Runtime estimates and plan choice may change; planning arena is unchanged |
| Planning bound reached with fitting fallback | Canonical bounded fallback may return a valid plan |
| Bounded planning cannot fit | Controlled `OptimizerResourceLimit` |
| Runtime exceeds estimate | Chapter-24 runtime reservation/spill/error protocol |
| Allocation fails after budget grant | Actual runtime allocation failure; grant is not allocation proof |

**N33-2: CLOSED**

21. **N33-3 phrase-by-phrase cleanup**

| Previous wording | Replacement |
|---|---|
| “lowest-cost valid PhysicalPlan” | Canonical costed selection under applicable search limits, validation, immutable handoff |
| “full Cascades/Volcano framework is deliberately deferred” | Baseline does not require that framework |
| “before introducing a more general optimizer framework” | Fundamental mechanisms are exposed without mandating a general framework |
| “Adaptive/runtime reoptimization remains deferred” | Outside the baseline optimizer/executor contract |
| “Future properties … may be added later” | Exact tracked property set and non-baseline extension categories |
| “remains future work” | Full Cascades/Volcano and adaptive runtime optimization are outside the baseline contract |

**N33-3: CLOSED**

22. **Timeless baseline/optionality assessment**

Chapter 33 now describes supported baseline scope and excluded baseline capabilities without implementation chronology. It neither requires nor prohibits optional frameworks beyond the canonical owner contracts.

23. **Tracked-property preservation**

The tracked v1 property set remains exactly:

- `OrderingProperty`
- `RequiredSlotSet`

Rewindability, partitioning, and other extension categories remain outside the tracked baseline.

24. **SQL semantic-preservation regression**

Unchanged:

- typed and bound logical input;
- types and NULL behavior;
- bag and occurrence semantics;
- demand and error preservation;
- required slots and ordering;
- snapshot and transaction semantics.

Bounded search is not permission to return a semantically invalid plan.

25. **Statistics/semantic-emptiness regression**

Unchanged. Statistics and numerical estimates affect cost and plan selection only. Exact semantic emptiness remains owned by §§20.17.10 and 35.2.

26. **Operator eligibility regression**

Unchanged. Alternatives must be implemented, capability-enabled, and eligible for the particular logical instance. A cost formula does not establish availability.

27. **Immutable PhysicalPlan regression**

Unchanged. The output remains one validated immutable `PhysicalPlan`; transaction, snapshot, cursor, allocation, spill, cancellation, and worker-local state remain execution-owned.

28. **Lazy-subquery regression**

Unchanged. Scalar, EXISTS, and IN side-plan roles retain their canonical lazy demand, cardinality, and error contracts.

29. **DML/W/C/R regression**

No Chapter-31 text changed. Ordinary closure before `W`, the single mutation publisher, retry boundaries, and `W/C/R` remain intact.

30. **Chapter-32 parallel-runtime regression**

No Chapter-32 text changed. Morsel coverage, exclusive claims, exactly-once acceptance, legal early stop, repeated passes, and worker-count independence remain intact.

31. **DDL/VACUUM/ANALYZE regression**

Dedicated control lowering remains available when no relational alternatives require search. DDL, VACUUM, and ANALYZE coordinator ownership is unchanged.

32. **Global contradiction-search results**

- No remaining global-optimum requirement conflicts with bounded search.
- Existing Chapter-38 “lowest active objective cost” wording applies correctly to legal alternatives available to that selection path.
- Planning arena and execution memory remain distinct.
- Estimates do not guarantee allocation.
- Unavailable operators cannot enter the final plan.
- Resource fallback does not bypass final validation.
- Adaptive runtime replanning remains outside the baseline.
- Statistics still cannot establish semantic emptiness.
- No physical scheduling rule was promoted to SQL semantics.
- No cross-owner contradiction was found.

33. **Document-role audit**

Chapter 33 remains:

- timeless;
- implementation-independent;
- behaviorally precise;
- proportional to the baseline;
- free of review history, implementation status, benchmark claims, concrete APIs, duplicate formulas, new errors, or new fallback policy.

34. **New semantic questions**

NONE.

35. **git diff --check**

PASS — no output.

36. **Repair-only diff summary**

- `docs/ARCHITECTURE.md`
- 40 insertions
- 10 deletions
- Four Chapter-33 sections changed
- No other chapter changed

37. **Final Git status**

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

38. **Confirmations**

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- CHAPTERS 31–32 NOT MODIFIED
- CHAPTER 34 NOT MODIFIED
- HISTORICAL REVIEW ARTIFACTS UNTOUCHED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT
- CHAPTER 34 REVIEW NOT STARTED

CHAPTER 33 ARCHITECTURE FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT