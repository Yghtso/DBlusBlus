1. **Verdict:** Chapter-5 targeted fixes completed successfully. M1–M4 and E1 are resolved without semantic changes.

2. **Initial Git state:**

```text
status: clean
index:  clean
HEAD:   3d92bed0845fd1b75fc97a328b054efee925660c
```

3. No pre-existing user/external working-tree or index state required preservation.

4. **M1 original:** “maximum accepted raw tuple payload” and “row-too-large/unsupported outcome.”

5. **Arithmetic verified:**

```text
8192 - 48 - 8 = 8136
accepted v1 maximum = 8135
8136 remains deliberately rejected
```

6. **Corrected terminology:** “maximum accepted complete encoded tuple length” and “complete encoded tuple length limit for heap-page storage.”

7. **Corrected error:** Oversized insertion MUST produce `ROW_TOO_LARGE`.

8. Tuple-size semantics are unchanged: complete encoded tuple bytes are limited to 8135; 8136 remains rejected.

9. **M1 RESOLVED.**

10. **M2 original:** “For the initial page geometry” and deferral “until … layers are mature.”

11. **Corrected:** “For the v1 page geometry” and “Overflow/large-object storage is deferred from the v1 architecture baseline.”

12. Inline-only v1 storage remains unchanged; no overflow pages, TOAST, off-page values, or chained tuples were authorized.

13. **M2 RESOLVED.**

14. **M3 original:** “invalid/corrupt or incompatible relative to that schema.”

15. **Canonical comparison:** §4.13.8 classifies supported-format local heap codec failures as `CORRUPT_HEAP`.

16. **Corrected:** “Such a supported-v1 tuple is malformed heap data and MUST be classified as `CORRUPT_HEAP`.”

17. Schema ownership is unchanged: validation consumes a resolved immutable historical descriptor; catalog/schema semantics remain owned by Chapter 16.

18. **M3 RESOLVED.**

19. **M4 original:** “The first implementation MAY…” and “until such behavior is explicitly implemented.”

20. **Corrected timeless contract:**

- v1 tuple writers emit `SchemaVer{1}`;
- `SchemaVer{0}` is invalid;
- persisted historical versions remain resolvable;
- schema-changing ALTER and translation/rewriting are deferred from v1;
- additional SchemaVer production requires explicit architecture revision.

21. SchemaVer width, invalid-zero rule, history, nonreuse, and exhaustion semantics are unchanged.

22. No schema-changing or schema-evolution behavior was newly authorized.

23. **M4 RESOLVED.**

24. **§5.7.3 old reference:** “MVCC/transaction chapters.”

25. **New targets:** §§9.6–9.7 and 10.2–10.4.

26. **§5.14 old reference:** “later WAL/BufferPool transaction protocol.”

27. **New targets:** §§7.10.1, 12.11–12.12, and 15.2.

28. **§5.15 old reference:** “later transaction/durability chapters.”

29. **New targets:** §§9.6, 10.2–10.4, 12.12, 15.3, 15.6, and 39.1.

30. **E1 RESOLVED.** Every target exists and owns the referenced behavior.

31. **Temporality:** The authorized scopes contain none of the identified project-maturity wording.

32. **Terminology:** §5.6 now consistently describes the 8135 limit as complete encoded tuple length and names `ROW_TOO_LARGE`.

33. **Normative strength:** Preserved. Existing MUST-level rejection and historical-schema obligations remain intact.

34. **Cross-references:** All new section references resolve to current headings and canonical owners.

35. **Document role:** Architecture retains invariants and ownership boundaries; no verification recipe or implementation sequencing was added.

36. **Local re-review answers:**

| Question | Answer |
|---|---|
| Is 8135 unequivocally whole-tuple length? | YES |
| Is `ROW_TOO_LARGE` the sole §5.6 oversized-v1 classification? | YES |
| Is §5.6 free from project-maturity language? | YES |
| Does the NOT NULL violation classify as `CORRUPT_HEAP`? | YES |
| Is §5.13 timeless? | YES |
| Are all three references precise? | YES |
| Did semantic behavior change? | NO |

37. **Chapter-5 regression:** Heap header remains 48 bytes; slot entry 8 bytes; capacity 1018 descriptors; maximum SlotId 1017. RID, tuple format, owner validation, WAL/MTR, PAGE_INIT, recovery, compaction, and reclamation semantics are unchanged.

38. **Chapter-4 compatibility:** Preserved for error categories, common headers, PageNo, SlotId, RID, validation, owner identity, compatibility, checksums, and page_lsn.

39. `docs/VERIFICATION.md` is unchanged.

40. `docs/PROJECT_STATE.md` is unchanged.

41. §7.5 still contains “once the buffer layer exists” unchanged.

42. **FROZEN ARCHITECTURE SEMANTIC QUESTION:** None.

43. **FOLLOW-UP VERIFICATION GAP:** None.

44. **File changed:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:3068).

45. **Hunk classifications:**

| Hunk | Classification |
|---|---|
| §5.6 whole-tuple/error wording | A |
| §5.6 timeless v1 scope | B |
| §5.10.3 corruption classification | C |
| §5.13 timeless SchemaVer wording | D |
| §5.7.3 reference | E |
| §5.14 reference | F |
| §5.15 reference | G |
| Wrapping-only changes | H — none |

46. Only `docs/ARCHITECTURE.md` was task-modified.

47. **Final Git state:**

```text
status:  M docs/ARCHITECTURE.md
index:   clean
HEAD:    3d92bed0845fd1b75fc97a328b054efee925660c
```

48. **`git diff --check`:** Passed with no output.

49. No external repository changes appeared during the task.

50. No pre-existing material was modified, reverted, normalized, or staged.

51. No implementation, build, test, benchmark, formatting sweep, staging, or commit occurred.

52. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.