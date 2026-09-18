## 1. Initial HEAD, commit, and status

- HEAD: `c5aecd3c78d70672358c8e498e7061c4d80ef267`
- Commit: `c5aecd3 applied FIX-B 31 in ARCHITECTURE`
- Working tree: clean
- Index: clean

## 2. Final HEAD and status

- HEAD unchanged: `c5aecd3c78d70672358c8e498e7061c4d80ef267`
- Working tree: `docs/ARCHITECTURE.md` modified
- Index: clean

## 3. Fix-A preservation

Preserved:

- DML mutation/write publication remains single-worker.
- Scans, target materialization, and expression evaluation were not forced single-worker.
- VACUUM still has no relational result bag.
- PhysicalAnalyze retains one publication coordinator without introducing parallel ANALYZE.

## 4. Fix-B/N31-1 preservation

N31-1 remains closed:

- ordinary DML candidates close before `W`;
- immediate uniqueness and exact pending owners remain intact;
- UPDATE uses authoritative old images;
- RETURNING expressions are evaluated before `W`;
- no same-`TxnId` retry occurs after `W`.

## 5. Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

## 6. Sections modified by Fix C

- §15.1.2 — affected-row ownership
- §15.7.3 — external output
- §15.9 — invariants
- §26.3.1 — execution completion
- §26.3.2 — result handoff
- §31.9 — RETURNING spool
- §31.10 — query result interface
- §31.13 — invariants
- §§39.1.2–39.1.4 — first-write/error consequences
- §39.1.7 — session loss
- §39.1.8 — observability/forbidden implementations
- §39.3 — execution and spill errors
- §§41.3 and 41.5 — verification obligations

## 7. Final definition of `R`

```text
R = successful statement-result publication
```

`R` atomically establishes both:

1. authoritative publication of the successful DML statement-result envelope; and
2. transfer of any finalized result spool/cursor from statement execution to the result/request owner.

Readiness, finalization alone, cursor allocation, first `Next()`, first row, and exhaustion are not `R`.

## 8. `W` regression

`W` remains §39.1.2’s first persistent transaction-owned statement write. Its definition and retry consequences are unchanged.

## 9. `C` regression

`C` denotes completion of the existing autocommit C4–C5 result-withholding requirement. It is not a new COMMIT stage and does not change the C0–C6 protocol.

## 10. W/C/R ordering

```text
Explicit:
ordinary closure -> [W if applicable] -> execution success -> R -> delivery

Autocommit:
ordinary closure -> [W if applicable] -> execution success -> C -> R -> delivery
```

Zero-row DML may omit `W` but still crosses `R`.

## 11. Pre-`R` execution ownership

Before `R`:

- RETURNING and count state remain unpublished and statement-owned;
- spool append, spill writing, finalization, validation, and handoff preparation remain execution work;
- failure follows Chapter 39 using the actual `W` state;
- no successful count or RETURNING prefix escapes.

## 12. Post-`R` result ownership

After `R`:

- statement success and count are final;
- the complete logical RETURNING bag is established;
- cursor reads and transport are result-delivery work;
- ordinary delivery failure cannot reopen DML execution, authorize retry, revoke count authority, or independently set `MUST_ABORT`.

## 13. Successful-envelope publication point

Publication occurs only after all DML execution, mutation, RETURNING construction, required validation, and result ownership-transfer preparation succeed. For autocommit, C4–C5 must also have completed.

## 14. Explicit-transaction rule

After `R`, an ordinary cursor/read/transport failure leaves a usable explicit transaction `ACTIVE`. Later statements, COMMIT, or ROLLBACK remain available under normal command sequencing.

## 15. Autocommit rule

Autocommit cannot cross `R` before C4–C5. After `C` and `R`, delivery failure leaves the transaction `COMMITTED`; rollback and same-statement retry are impossible.

## 16. Affected-count authority

The final successful attempt’s count becomes authoritative at `R`.

## 17. Count authority versus delivery

Count authority is distinct from whether the client received the metadata. Delivery failure cannot revoke the count. It remains result-envelope metadata, not a relational column or `LogicalSlotId`.

## 18. First-read spool failure

After `R`, failure on the first spool read produces a terminal cursor error. The DML statement remains successful; an explicit usable transaction remains `ACTIVE`, and autocommit remains `COMMITTED`.

## 19. Later-read spool failure

A later failed read has the same ownership outcome. The cursor fails without retroactively failing the statement.

## 20. Returned-prefix non-retraction

Already returned rows remain historical observations and retain their guaranteed lifetime. They are not retracted.

## 21. Terminal error versus `FINISHED`

The cursor contract now distinguishes:

```text
chunk
FINISHED
terminal result error
```

`FINISHED` means successful exhaustion only. A failed cursor returns no further successful rows.

## 22. Cursor abandonment

Voluntary abandonment:

- discards unread rows;
- cleans cursor-owned temporary resources;
- preserves statement success and count authority;
- does not independently cause `MUST_ABORT`.

Draining the complete cursor is not required to preserve DML success.

## 23. Pre-/post-`R` cancellation

- Before `R`: statement-execution cancellation governed by the actual `W` state.
- After `R`: result-delivery abandonment and cleanup, without retroactive DML failure.
- Independently fatal session loss retains its own owner.

## 24. Session-loss distinction

A usable-session cursor error is not session loss. Actual loss of an explicit transaction’s session invokes §39.1.7 and may abort the still-active transaction, but does not rewrite the completed DML statement as failed. Autocommit remains committed.

## 25. Storage/corruption distinction

Ordinary temporary-spill read/framing errors remain result-delivery errors after `R`. Persistent corruption, invariant failure, or database-noncontinuable conditions retain their stronger canonical classifications.

## 26. Result-spool lifetime

The spool is:

- query/request-temporary;
- spill-managed;
- nonpersistent and non-WAL;
- not crash-recovered;
- unavailable to another attempt;
- invalid as a SQL-row source after result destruction.

## 27. Accounting through ownership transfer

Accounting coverage remains continuous across statement-to-result ownership transfer. Required buffers, VARCHAR backing, dictionary state, reservations, and spill resources remain valid without prescribing a particular C++ ownership mechanism.

## 28. Failed-cursor cleanup

Terminal error or abandonment releases result-owned reservations, buffers, and spill objects. It does not release transaction-owned locks or reopen statement execution.

## 29. Construction/finalization failure

Spool append, spill write, framing/extent validation, finalization, or transfer preparation failure before `R` remains execution failure. If `W` was crossed, the existing `MA` consequence applies.

## 30. Precommit autocommit failure

Failure before required C4–C5 completion publishes no successful count or RETURNING prefix.

## 31. Postcommit delivery failure

After `C` and `R`, spool-read, delivery cancellation, transport, or session-delivery failure cannot undo COMMITTED.

## 32. Explicit later-statement/COMMIT eligibility

A usable explicit transaction remains eligible for later statements and COMMIT or ROLLBACK. A concrete API may require closing the failed cursor before accepting another command.

## 33. SELECT regression

SELECT prefix/later-error semantics remain unchanged: a prefix is not retracted, but does not make a failed complete query successful.

## 34. Chapter-30 Sort regression

Chapter 30 was not modified. Its ready-Source prefix followed by later spill failure remains a failed query-completion model. It was not imported automatically into post-`R` DML.

## 35. Chapter-15 edits

Chapter 15 now states:

- count authority begins at `R`;
- count delivery can fail independently;
- pre-`R` result state remains execution-owned;
- post-`R` delivery cannot authorize retry or change the statement outcome.

## 36. Chapter-26 edits

Chapter 26 now distinguishes:

- internal readiness/completion;
- DML `R`;
- result-owner delivery;
- internal `FINISHED`;
- client successful `FINISHED`;
- terminal cursor error.

SELECT and external-sort completion semantics were preserved.

## 37. Chapter-31 §31.9 edits

Section 31.9 now owns:

- the atomic `R` transition;
- W/C/R ordering;
- pre-`R` execution failure;
- post-`R` delivery failure;
- explicit/autocommit consequences;
- count authority;
- non-retraction;
- cancellation, abandonment, session loss, accounting, and cleanup.

## 38. Chapter-31 §31.10 edits

The public synchronous cursor now distinguishes chunk, successful exhaustion, and terminal error. Chunk lifetime and result-envelope metadata ownership are explicit.

## 39. Chapter-31 invariant edits

Added invariants cover atomic `R`, pre-/post-`R` failure ownership, explicit/autocommit transaction state, cursor error versus `FINISHED`, non-retraction, accounting, and abandonment.

## 40. Chapter-39 reconciliation

The first-write matrix is now explicitly scoped to statement-execution failure before successful result publication. Post-`R` ordinary delivery errors do not reapply it.

Pre-`W`, post-`W`/pre-`R`, fatal, corruption, and session-loss consequences remain unchanged.

## 41. Chapter-41 obligations

Future Verification must prove:

- pre-`R` versus post-`R` classification;
- explicit `ACTIVE` and autocommit `COMMITTED` outcomes;
- count and prefix survival;
- error versus `FINISHED`;
- cancellation/session-loss separation;
- no retry;
- accounting transfer and cleanup;
- abandonment without statement revocation.

## 42. Required scenario matrix

| Scenario | Final outcome |
|---|---|
| Ordinary candidate during pre-`W` closure | Ordinary error; no persistent write |
| Dynamic failure before `W` | Existing pre-write owner |
| Dynamic failure after `W`, before `R` | Existing post-write execution consequence |
| Spool append fails before `R` | Execution failure |
| Spool finalization fails before `R` | Execution failure |
| Explicit DML reaches `R` | Statement successful; transaction `ACTIVE` |
| Explicit first read fails after `R` | Cursor error; statement successful; `ACTIVE` |
| Explicit later read fails after prefix | Same; prefix retained |
| Count delivered before later failure | Count remains authoritative |
| Count not delivered before failure | Authoritative but possibly unobserved |
| Explicit cursor abandoned | DML success retained; resources cleaned |
| Explicit delivery cancellation | DML success retained; no independent abort |
| Explicit session lost | Session owner aborts active transaction |
| Autocommit COMMIT fails before `R` | No successful result publication |
| Autocommit reaches C then R | `COMMITTED`; envelope successful |
| Autocommit first read fails | `COMMITTED`; cursor error |
| Autocommit later read fails | `COMMITTED`; prefix retained |
| Autocommit transport/session loss | `COMMITTED` remains |
| Cursor reports `FINISHED` | Successful exhaustion only |
| Cursor read error | Terminal error, never `FINISHED` |

## 43. Failure-injection thought experiments

- Case A: `W`, then finalization failure before `R` → execution failure, `MA`, no prefix/count.
- Case B: explicit `R`, first read fails → statement successful, count authoritative, `ACTIVE`, cursor error.
- Case C: explicit `R`, chunk 1 returned, chunk 2 fails → chunk 1 retained, cursor failed, transaction `ACTIVE`.
- Case D: autocommit `C`, then `R`, later read fails → transaction remains `COMMITTED`; no undo or retry.

## 44. Global stale-rule search

| Rule searched | Result |
|---|---|
| Every post-write failure always means `MA` | Qualified to pre-`R` execution failures |
| Post-success spool reads remain execution work | Rejected |
| Statement success requires draining RETURNING | Rejected |
| Delivery failure revokes count | Rejected |
| Cursor failure reopens retry | Rejected |
| Spool-read failure aborts explicit transaction | Rejected for usable post-`R` session |
| Autocommit exposure before C4–C5 | Still forbidden |
| Returned rows retracted | Rejected |
| Cursor error equals `FINISHED` | Rejected |
| Result spool is persistent/WAL | Rejected |
| Abandonment fails DML | Rejected |
| Chapter-29/30 ready-Source failures | Valid different query-completion owners |
| SELECT prefix/later error | Valid SELECT behavior |

No stale N31-2 rule remains.

## 45. Document-role audit

Passed. The edits are timeless semantic ownership rules. They add no roadmap, implementation status, review history, wire protocol, C++ type, mandatory duplication, or new storage subsystem.

## 46. N31-1 regression matrix

| N31-1 contract | Result |
|---|---|
| Ordinary closure before `W` | Preserved |
| Canonical error reduction | Preserved |
| Spillable INSERT/INSERT SELECT staging | Preserved |
| Authoritative UPDATE images | Preserved |
| Complete UNIQUE preparation | Preserved |
| Exact pending owners | Preserved |
| Immediate key-swap rejection | Preserved |
| RETURNING evaluation before `W` | Preserved |
| No retry after `W` | Preserved |
| Dynamic pre-`R` failure uses actual `W` | Preserved |

## 47. Fix-A regression

Passed. DML single-worker publication, VACUUM result/concurrency semantics, and ANALYZE coordinator ownership remain unchanged.

## 48. No new C++/storage/API design imposed

Confirmed. No required class, atomic primitive, reference-counting scheme, allocator, coroutine, wire format, or multi-cursor design was introduced.

## 49. No physical undo introduced

Confirmed.

## 50. Verification edit

`docs/VERIFICATION.md` was not modified.

## 51. New semantic questions

`NONE`

## 52. `git diff --check`

Passed with no diagnostics.

## 53. Fix-C-only diff summary

```text
docs/ARCHITECTURE.md
239 insertions
47 deletions
```

## 54. Cumulative Architecture diff summary

Fix A and Fix B are committed in the live HEAD. Therefore the current unstaged cumulative diff equals Fix C alone.

Across the Chapter-31 Fix-A/Fix-B/Fix-C campaign relative to `823b3df`:

```text
778 insertions
160 deletions
```

## 55. Final Git status

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

## 56. Final confirmations

```text
ARCHITECTURE MODIFIED: docs/ARCHITECTURE.md ONLY
VERIFICATION MODIFIED: NO
DEVELOPMENT MODIFIED: NO
PROJECT_STATE MODIFIED: NO
IMPLEMENTATION: NOT STARTED
BUILD: NOT RUN
TESTS: NOT RUN
SANITIZERS: NOT RUN
BENCHMARK: NOT RUN
STAGING: NONE
COMMIT: NONE
CHAPTER 32 REVIEW: NOT STARTED
```

CHAPTER 31 FIX C COMPLETE —
ARCHITECTURE READY FOR READ-ONLY CLOSURE AUDIT