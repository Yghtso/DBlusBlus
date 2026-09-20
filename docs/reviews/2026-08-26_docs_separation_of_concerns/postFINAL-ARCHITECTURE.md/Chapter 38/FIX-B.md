## Chapter 38 Architecture Fix B report

1. **Initial HEAD and commit:** `4ea12a45e962dbb2a8055d79cd69b04bf790c7b9` — `applied FIX-A 38 in ARCHITECTURE`.

2. **Initial repository state:** tracked worktree clean; index clean.

3. **Final repository state:** HEAD unchanged. Worktree contains only `M docs/ARCHITECTURE.md`; index remains clean.

4. **Task-modified files:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28797) only.

5. **Changed section and lines:** Chapter 38, §38.21, lines 28797–28806 after the edit.

6. **Diff size:** 10 insertions, 5 deletions; one substantive hunk.

7. **N38-4 original contradiction:** a catchable, representable backing allocation denied while the configured arena guard was clear could ultimately be reported as `OptimizerResourceLimit`, despite the configured budget not preventing completion.

8. **Original §38.21 wording:** it normalized a catchable backing-allocation denial into inability to satisfy the planning-resource bound and reported `OptimizerResourceLimit` if bounded search also failed.

9. **§39.3 owner:** lines 29451–29468 define `OutOfMemory` as a cross-layer operational cause and classify an ordinary catchable allocation denial for a supported, exactly representable form as `OutOfMemory`.

10. **§39.4 owner:** lines 29563–29574 reserve `OptimizerResourceLimit` for cases where the configured planning-time or memory bound prevents completion even after bounded fallback.

11. **Final §38.21 rule:** a guard-permitted, supported, representable backing request that is catchably denied retains the Chapter-39 `OutOfMemory` cause. Safe existing bounded mitigation may be attempted; successful validated recovery completes normally. Persistent below-budget denial remains `OutOfMemory`. A distinct configured-byte-guard event retains the configured-bound outcome.

12. **Configured guard preserved:** checked projected charged bytes greater than `optimizer_planning_arena_budget_bytes` trigger the guard before allocation. Equality remains permitted.

13. **Below-budget denial:** classified as `OutOfMemory` when unresolved.

14. **Successful mitigation:** produces a legal plan subject to ordinary final validation and no terminal resource error.

15. **Persistent backing denial:** remains `OutOfMemory`, including when bounded mitigation encounters the same below-budget physical denial.

16. **Configured bounded exhaustion:** remains `OptimizerResourceLimit` when the retained configured byte bound prevents bounded planning from completing.

17. **Mixed causes:** the actual terminal preventing cause governs. A persistent physical denial remains `OutOfMemory`; a distinct later configured-guard event that prevents completion is governed by `OptimizerResourceLimit`. No new precedence rule was introduced.

18. **Error-ownership matrix:**

| Case | Limiting cause / guard | Mitigation and terminal outcome | Owner | Result |
|---|---|---|---|---|
| A | Guard clear; allocation succeeds | Planning proceeds | §38.21 | PASS |
| B | Exhaustive guard triggers | Clean bounded restart fits; planning succeeds | §§37.13, 38.21 | PASS |
| C | Configured bound blocks bounded search | `OptimizerResourceLimit` | §§38.21, 39.4 | PASS |
| D | Initial heuristic cannot initialize within bound | `OptimizerResourceLimit` | §§38.21, 39.4 | PASS |
| E | Initial heuristic later exceeds bound | `OptimizerResourceLimit` | §§38.21, 39.4 | PASS |
| F | Guard clear; allocator denies representable growth | Permitted mitigation or unresolved `OutOfMemory` | §§38.21, 39.3 | PASS |
| G | Same denial; safe mitigation succeeds | Validated plan; no terminal error | §38.21 | PASS |
| H | Mitigation encounters the same physical denial | `OutOfMemory` | §§38.21, 39.3 | PASS |
| I | Guard rejects growth before allocation | Configured-guard fallback path, not physical OOM | §38.21 | PASS |
| J | Charged usage equals budget; zero growth | Equality does not trigger guard | §38.21 | PASS |
| K | Projected usage exceeds budget by one byte | Guard triggers | §38.21 | PASS |
| L | Missing or negative mandatory effective budget | `OptimizerError` before search state | §38.21 | PASS |
| M | Valid zero budget cannot fit positive state | Bounded failure yields `OptimizerResourceLimit` | §§38.21, 39.4 | PASS |
| N | Arena accounting overflows | Existing internal/`OptimizerError` outcome | §§36.2.1, 38.21 | PASS |
| O | Runtime QueryMemoryManager denies grant | Runtime resource handling; ordinarily `OutOfMemory` if unresolved | §§24.5–24.6, 39.3 | PASS |
| P | High estimated plan cost only | Ordinary cost comparison; no resource error | §§36, 38.21 | PASS |
| Q | Wall time/counters vary; byte guard clear | No resource fallback | §38.21 | PASS |
| R | External budget changes mid-invocation | Retained validated budget remains authoritative | §§33.3, 38.21 | PASS |

19. **Chapter-37 threshold regression:** unchanged and coherent:

- `N=9`, limit `10`, guard clear: exhaustive.
- `N=10`, limit `10`, guard clear: exhaustive.
- `N=11`, limit `10`: heuristic initially.
- `N=1`, limit `0`: heuristic initially.
- At threshold with a real guard event: clean bounded restart.
- Threshold proximity or equality alone never triggers fallback.

20. **Checkpoint and partial memo:** unchanged. Guard-triggered exhaustive state is discarded at the region checkpoint; incomplete subsets, partitions, alternatives, and memo entries cannot become completed results.

21. **Same-invocation restart:** unchanged. Bounded search restarts from retained logical, catalog/statistics, configuration, objective, property, and capability inputs without beginning execution or side effects.

22. **N38-1:** PREVIOUSLY CLOSED / NOT REOPENED. Epsilon validation, anchored total selection, dominance compatibility, and finite saturation were untouched.

23. **N38-2:** PREVIOUSLY CLOSED / NOT REOPENED. Complete tagged structural identity and diagnostic-only fingerprints were untouched.

24. **N38-3:** PREVIOUSLY CLOSED / NOT REOPENED. Execution-memory allocation, phase accounting, rounding, redistribution, and runtime grant authority were untouched.

25. **N38-5:** PREVIOUSLY CLOSED / NOT REOPENED. Hash load-factor, fanout, bounded recursion, and runtime spill contracts were untouched.

26. **All 19 invariants:** unchanged, contiguous, and coherent. Invariant 13 continues to describe the configured planning-arena guard; it does not claim that below-budget backing denial is `OptimizerResourceLimit`.

27. **Chapters 31–37:** unchanged. DML, parallel execution, optimizer invocation, statistics, estimates, costs, join enumeration, threshold, and bounded-search contracts remain intact.

28. **Chapter-39 consistency:** restored. Physical allocation denial maps to `OutOfMemory`; configured planning-bound exhaustion maps to `OptimizerResourceLimit`; malformed configuration maps to `OptimizerError`. No new error category was introduced.

29. **Global consistency search:** relevant Architecture occurrences divide consistently into:

- catchable physical allocation denial: `OutOfMemory`;
- configured planning/front-end budget refusal: owner-specific resource-limit outcome;
- planning-arena guard and bounded exhaustion: `OptimizerResourceLimit`;
- query execution-memory pressure and runtime grants: Chapters 24 and 39;
- threshold selection: Chapter 37;
- cost saturation: ordinary planning metadata, not resource failure.

No remaining normative contradiction was found.

30. **Remaining Architecture findings:** none within Fix B’s authorized scope. N38-4 is **CLOSED by the repair**, pending independent closure audit.

31. **New semantic questions:** none.

32. **Exact diff scope:** one replacement paragraph in §38.21. No other Chapter-38 subsection or invariant changed.

33. **`git diff --check`:** passed with no output.

34. **Final repository confirmation:**

- VERIFICATION NOT MODIFIED
- DEVELOPMENT NOT MODIFIED
- PROJECT_STATE NOT MODIFIED
- CHAPTERS 31–37 NOT MODIFIED
- CHAPTER 38: ONLY §38.21 ERROR-HANDOFF TEXT MODIFIED
- CHAPTER 39 ONWARD NOT MODIFIED
- HISTORICAL ARTIFACTS NOT MODIFIED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING/COMMIT
- CHAPTER 39 REVIEW NOT STARTED
- Final HEAD remains `4ea12a45e962dbb2a8055d79cd69b04bf790c7b9`
- Audit/task worktree change: `M docs/ARCHITECTURE.md`
- Index: clean

Finding dispositions:

- N38-1: PREVIOUSLY CLOSED / NOT REOPENED
- N38-2: PREVIOUSLY CLOSED / NOT REOPENED
- N38-3: PREVIOUSLY CLOSED / NOT REOPENED
- N38-4: CLOSED
- N38-5: PREVIOUSLY CLOSED / NOT REOPENED

**CHAPTER 38 ARCHITECTURE FIX B COMPLETE —
READY FOR FOCUSED INDEPENDENT READ-ONLY
CLOSURE AUDIT**