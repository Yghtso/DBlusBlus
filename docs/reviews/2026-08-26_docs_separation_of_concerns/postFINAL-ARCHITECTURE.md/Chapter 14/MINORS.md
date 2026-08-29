## Chapter 14 targeted cleanup

1. **Verdict:** **CHAPTER 14 ARCHITECTURE — CLEAN.** The four document-only findings were resolved without changing semantics in [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:10720).

2. Initial Git state:

   - Working tree: clean
   - Index: clean
   - HEAD: `0811eeb8da29bf77fb89441706e19450cbd61a5c`

3. D14-M1, D14-M2, and D14-M3 were committed baseline content and remained semantically unchanged.

4. No pre-existing review artifact appeared in Git status; none was read, modified, or staged.

## Finding corrections

5. M14-1 original wording:

   > transaction-status retention/truncation eligibility

6. Corrected wording:

   > transaction-status retention/reclamation eligibility

7. Sparse reclamation, absolute status PageNos, logical file length, exclusive cutoff, and no-renumbering semantics are unchanged.

8. M14-1: **RESOLVED**.

9. M14-2 original platform-specific wording:

   > On Linux the baseline physical optimization may use `fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)`.

10. Corrected platform-independent wording:

   > The storage layer MAY use any platform-supported sparse-deallocation mechanism for whole status-page byte ranges, provided it preserves logical file length and absolute PageNo mapping.

11. Fully retired status-page ranges may still be sparsely deallocated only after durable cutoff publication and BufferPool/DPT retirement. If deallocation is unavailable or fails, retaining allocated blocks remains safe.

12. Logical file length remains unchanged, PageNos remain absolute, and later pages never shift or become renumbered.

13. No syscall, kernel flag, filesystem API, or implementation container is prescribed.

14. M14-2: **RESOLVED**.

15. M14-3 original wording:

   > Optimizer statistics remain the responsibility of ANALYZE, though vacuum may later trigger or assist that subsystem.

16. Corrected timeless wording:

   > Optimizer statistics remain the responsibility of ANALYZE. Vacuum MAY trigger or assist that subsystem without assuming ownership of its published state.

17. FSM remains advisory and rebuildable; heap free-space state remains authoritative; ANALYZE retains ownership of optimizer-statistics publication.

18. M14-3: **RESOLVED**.

19. M14-4 original wording:

   > Background scheduling may later react to measured dead-version ratio, aborted-version pressure, freezing/status pressure, or table growth.

20. Corrected timeless wording:

   > Background scheduling MAY react to measured dead-version ratio, aborted-version pressure, freezing/status pressure, or table growth.

21. The same four policy inputs remain. No interval, threshold, worker count, priority, cadence, timing guarantee, or new scheduling input was introduced.

22. READY admission, DRAINING quiescence, cancellation, shutdown, failure, and noncontinuable-state requirements are unchanged.

23. M14-4: **RESOLVED**.

## Semantic regression assessment

24. D14-M1: unchanged—write-status dependency registration, classification, cutoff clamp, C5/A3 release, and persistent/future dependency distinction remain intact.

25. D14-M2: unchanged—crash-recoverable status independence, durable-page/durable-WAL alternatives, no-force, no mandatory checkpoint, WAL retention, and control publication ordering remain intact.

26. D14-M3: unchanged—queued/granted `TUPLE_WRITE` claims, epoch handoff, zero-claim reuse barrier, and no-lock-replay semantics remain intact.

27. Chapter-14 technical regression:

| Area | Result |
|---|---|
| Status-history dependencies | Unchanged |
| Exclusive/page-aligned cutoff | Unchanged |
| Creator freezing | Unchanged |
| Aborted-`xmax` normalization | Unchanged |
| Sparse TXN_STATUS reclamation | Unchanged |
| Read epochs | Unchanged |
| `TUPLE_WRITE` claims | Unchanged |
| DEAD/UNUSED | Unchanged |
| Index cleanup | Unchanged |
| `prev_RID` proof | Unchanged |
| Checkpoint interaction | Unchanged |
| WAL retention | Unchanged |
| FSM authority | Unchanged |
| Recovery/READY behavior | Unchanged |

## Local re-read answers

28. Answers to all 42 questions:

| # | Answer | Result |
|---:|:---:|---|
| 1 | YES | Misleading truncation wording removed |
| 2 | YES | Replacement describes reclamation |
| 3 | YES | Status PageNos remain absolute |
| 4 | YES | Logical-length semantics unchanged |
| 5 | YES | Sparse reclamation remains allowed |
| 6 | YES | Page renumbering remains forbidden |
| 7 | YES | `fallocate` removed from Architecture |
| 8 | YES | `FALLOC_FL_*` constants removed |
| 9 | YES | Required semantic result retained |
| 10 | YES | Platform mechanism is implementation-defined |
| 11 | YES | Existing safe-extra-storage fallback retained |
| 12 | YES | Durable cutoff still precedes physical reclaim |
| 13 | YES | BufferPool/DPT/guard constraints unchanged |
| 14 | YES | Absolute TXN_STATUS mapping unchanged |
| 15 | YES | §14.16 project-time wording removed |
| 16 | YES | §14.16 ownership unchanged |
| 17 | YES | FSM authority unchanged |
| 18 | YES | ANALYZE ownership unchanged |
| 19 | YES | §14.17 roadmap wording removed |
| 20 | YES | Existing scheduling inputs preserved |
| 21 | YES | No new input invented |
| 22 | YES | No interval or threshold introduced |
| 23 | YES | Correctness remains scheduling-independent |
| 24 | YES | Lifecycle semantics unchanged |
| 25 | YES | D14-M1 unchanged |
| 26 | YES | D14-M2 unchanged |
| 27 | YES | D14-M3 unchanged |
| 28 | YES | `TUPLE_WRITE` barrier unchanged |
| 29 | YES | Crash-recoverable cutoff safety unchanged |
| 30 | YES | Write-status dependency unchanged |
| 31 | NO | No persistent-format change |
| 32 | NO | No recovery-rule change |
| 33 | NO | No WAL-rule change |
| 34 | NO | No status-lookup-rule change |
| 35 | NO | No DEAD/UNUSED-rule change |
| 36 | NO | Chapter 15 unchanged |
| 37 | NO | §31.7 unchanged |
| 38 | NO | Appendix C unchanged |
| 39 | NO | No source API introduced |
| 40 | NO | No project chronology introduced |
| 41 | YES | All four M14 findings resolved |
| 42 | YES | Chapter 14 is semantic- and document-model clean |

Questions 31–35 were answered literally; their negative wording requires `NO` to confirm no semantic change.

29. Documentation-model assessment:

| Question | Result |
|---|---|
| A. Project chronology introduced? | NO |
| B. Current implementation narration introduced? | NO |
| C. DEVELOPMENT sequencing remains? | NO |
| D. VERIFICATION procedure introduced? | NO |
| E. PROJECT_STATE facts introduced? | NO |
| F. Devlog/history introduced? | NO |
| G. Terminology matches reclamation semantics? | YES |
| H. Platform coupling removed? | YES |
| I. Scheduling wording timeless? | YES |
| J. Architecture remains analytical? | YES |
| K. Freedom improved without weakened semantics? | YES |
| L. Readable without timeline knowledge? | YES |

30. Temporality assessment: no project chronology remains in the corrected wording. Remaining “later” occurrences in §14.14.2 describe runtime WAL ordering, subsequent page writes, or numerically later PageNos—not project time.

31. Terminology now consistently describes sparse physical reclamation rather than truncation, suffix shrinking, compaction, or PageNo renumbering.

32. Linux syscall and flag coupling was removed. Only the required sparse-deallocation outcome remains.

33. Analytical depth remains sufficient: the text still explains why logical length and absolute PageNo mapping must survive physical deallocation.

34. Implementation freedom improved: any compliant platform mechanism may be used, and unavailable sparse deallocation safely retains extra storage.

## Status

35. Frozen architecture semantic questions: **NONE**.

36. B14-1: **CLOSED**.

37. B14-2: **CLOSED**.

38. B14-3: **CLOSED**.

39. D14-M1: **RESOLVED AND INTEGRATED**.

40. D14-M2: **RESOLVED AND INTEGRATED**.

41. D14-M3: **RESOLVED AND INTEGRATED**.

42. M14-1: **RESOLVED**.

43. M14-2: **RESOLVED**.

44. M14-3: **RESOLVED**.

45. M14-4: **RESOLVED**.

46. Chapter-14 architecture: **CLEAN**.

47. Chapter-14 verification: **SYNCHRONIZATION PENDING**. Chapter 14 is not yet fully closed.

48. Future verification families, not edited here:

   1. write-status registration/cutoff races;
   2. cutoff prerequisite WAL durability and second-crash retention;
   3. `TUPLE_WRITE` waiter/holder versus RID reuse;
   4. read-epoch/grace/reuse matrices;
   5. freezing, aborted-`xmax`, status independence, and RETIRED-negative cases;
   6. status-page reclamation/checkpoint/control-cutoff crashes and sparse-deallocation behavior.

49. Chapter-15 direct review: **NOT STARTED**.

## Diff and repository state

50. Files changed:

   - [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:10720)

51. Task-created hunk classifications:

| Class | Hunk |
|---|---|
| A | §14.1 reclamation terminology |
| B | §14.14.2 platform-neutral sparse deallocation |
| C | §14.16 timeless ANALYZE boundary |
| D | §14.17 timeless scheduling permission |
| E | Local wrapping within those corrected sentences |

52. Only `docs/ARCHITECTURE.md` was task-modified.

53. Final Git state:

   - Working tree: `M docs/ARCHITECTURE.md`
   - Index: clean
   - HEAD unchanged: `0811eeb8da29bf77fb89441706e19450cbd61a5c`

54. `git diff --check`: **PASS**, no output.

55. No external repository change was detected during the task.

56. Pre-existing D14-M1/M2/M3 material remained preserved.

57. No review artifact was modified or staged.

58. No implementation, test, build, benchmark, formaMtting sweep, staging, commit, devlog, or verification synchronization occurred.

59. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
