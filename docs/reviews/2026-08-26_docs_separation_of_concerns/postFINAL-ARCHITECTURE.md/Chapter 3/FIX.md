All four Pass-C findings are resolved with no architecture-semantic change.

## C-M1 — Shutdown states

1. Original lifecycle wording:

   > `DRAINING` — admitted work and producer services have quiesced → `CLOSING`

   It did not identify the corresponding shutdown step.

2. Original shutdown wording:

   > until the later BufferPool quiesce in step 4. Exact shutdown ordering is:

   It did not state which lifecycle state governed each step.

3. Confirmed boundary: successful completion of shutdown step 3 publishes `DRAINING -> CLOSING` before step 4.

4. Corrected §3.3.1 transition:

   > `DRAINING` | §3.3.6 steps 1–3 complete: admitted work and ordinary producer services have quiesced | `CLOSING` | publish this transition before step 4; final durability/resource teardown continues with the lock held

5. Corrected §3.3.6 wording:

   > Steps 1–3 execute in `DRAINING`. Successful completion of step 3 publishes `DRAINING -> CLOSING` before step 4; steps 4–8 execute in `CLOSING` unless a failure enters `NONCONTINUABLE`.

6. Shutdown mapping:

   - Steps 1–3: `DRAINING`
   - Boundary after step 3: `DRAINING -> CLOSING`
   - Steps 4–8: `CLOSING`
   - Failure during either phase: `NONCONTINUABLE` where §3.3.6 requires it
   - Successful step 8: `CLOSING -> CLOSED`

7. Original CLOSING failure edge:

   > a live guard/worker/ownership invariant still prevents safe destruction

8. Corrected edge:

   > final durability/resource-teardown failure under §3.3.6, including a live guard/worker/ownership invariant that prevents safe destruction

   The owner and lock remain retained until safe non-clean teardown or process termination.

9. This represents existing §3.3.6 behavior: that section already required BufferPool, WAL, checkpoint, control, directory-sync, invariant, and drain failures to enter `NONCONTINUABLE`.

10. Durable-COMMIT consistency: unchanged. A late CLOSING failure cannot convert COMMITTED to ABORTED.

11. Owner-lock consistency: unchanged. The lock remains held through DRAINING, CLOSING, and required NONCONTINUABLE teardown and is released last.

12. C-M1: **RESOLVED**.

## C-m1 — Platform terminology

13. Original phrases:

   - “The initial supported environment”
   - “later experimentation”
   - “requirements of the initial architecture”

14. Corrected phrases:

   - “The v1 supported platform”
   - “The Linux-first v1 baseline permits experimentation”
   - “the required v1 platform contract”

15. §3.1 now consistently describes a durable v1 platform baseline rather than a project roadmap.

16. Supported-platform semantics are unchanged: Linux, x86-64 or ARM64, POSIX file APIs, one exclusive database process, and multiple worker threads.

17. C-m1: **RESOLVED**.

## C-m2 — Revision authority

18. Original wording:

   > unless a later explicit decision introduces one

19. Corrected wording:

   > unless an explicit architecture revision introduces one

20. An explicit architecture revision is now the sole named authority for admitting compiler extensions into the architecture baseline.

21. The C++20 requirement and rationale are unchanged.

22. C-m2: **RESOLVED**.

## C-m3 — Failed-open cleanup

23. Original wording:

   > Open uses scoped ownership so any failure stops and joins all started recovery tasks...

24. Corrected wording:

   > A failed open whose outcome and process-local ownership are fully known and quiesceable uses scoped ownership to stop and join all started recovery tasks...

25. Cleanly unwindable failures still quiesce all resources, release the OS lock in the established order, remove the process claim, and end in `CLOSED`.

26. Uncertain or unquiesceable failures still enter `NONCONTINUABLE` and follow non-clean teardown.

27. The owner lock is released only for fully known, safely quiesced failures. It remains held when workers, guards, publication, or ownership cannot be reconciled.

28. C-m3: **RESOLVED**.

## Consistency checks

29. Resulting shutdown edges:

   - `READY -> DRAINING`
   - `DRAINING -> CLOSING` after step 3
   - `DRAINING -> NONCONTINUABLE` for steps 1–3 quiescence/invariant failure
   - `CLOSING -> NONCONTINUABLE` for final durability/teardown failure
   - `NONCONTINUABLE -> CLOSING` for non-clean teardown
   - `CLOSING -> CLOSED` only after safe final ownership release

30. Remaining temporal words in Chapter 3 describe runtime ordering or retry/reopen semantics: later path rebinding, later BufferPool quiescence, later open attempts, future opens, next owner/opener, and eventual non-clean teardown. No project-roadmap wording remains from the findings.

31. Normative consistency: the transition additions expose existing behavior; the failed-open qualifier now agrees with the following NONCONTINUABLE rule. No unrelated MUST/SHOULD/MAY language changed.

32. Semantic-diff answers:

   - Previously valid implementation made invalid: **No**
   - Previously forbidden implementation made valid: **No**
   - Shutdown outcome changed: **No**
   - Open-failure outcome changed: **No**
   - Platform/language contract changed: **No**

33. Frozen architecture semantic question: **None**.

34. Architecture mechanism changed: **No**.

35. Lifecycle state set changed: **No**.

36. Authoritative ownership changed: **No**.

37. Architecture status/version: unchanged—authoritative v1 contract, version v1.

38. Pass-A regression: passed; canonical v1, parallel-ready, and non-roadmap wording remain intact.

39. Pass-B regression: passed; Chapter 2 flow, metadata, storage-layering, and BufferPool wording remain intact.

40. §3.3.2: unchanged.
41. §3.3.3: unchanged.
42. §3.3.5: substantively unchanged.
43. §3.3.7: unchanged.
44. Known §7.5 observation: **unchanged**—“once the buffer layer exists” remains reserved for Chapter 7 review.

## Repository checks

45. Files changed: `docs/ARCHITECTURE.md` only.

46. Hunk classifications:

   - §3.1: C-m1 platform terminology
   - §3.2: C-m2 revision authority
   - §3.3.1: C-M1 state boundary and CLOSING failure edge
   - §3.3.4: C-m3 clean/uncertain failure distinction
   - §3.3.6: C-M1 shutdown-state mapping
   - Associated wrapping only

47. Initial Git state:

   - Untracked external file: `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 3/FIX.md`
   - Index: empty
   - HEAD: `74bbf17012616a868e8f3fbb9e23e61ebef2a295`

48. Final Git state:

   - `docs/ARCHITECTURE.md` modified
   - Same external file remains untracked
   - Index empty
   - HEAD unchanged

49. `git diff --check`: **passed**.

50. Pre-existing untracked review file: still present and untracked.

51. It was not modified or staged.

52. `AGENTS.md`: unchanged.
53. `README.md`: unchanged.
54. `docs/PROJECT_STATE.md`: unchanged.
55. `docs/DEVELOPMENT.md`: unchanged.
56. `docs/VERIFICATION.md`: unchanged.
57. Chapter 1: unchanged.
58. Chapter 2: unchanged.
59. Chapter 4+ including §7.5: unchanged.
60. Source, tests, benchmarks, and build configuration: unchanged.