## Chapter-15 cleanup verdict

**CHAPTER-15 ARCHITECTURE — CLEAN.**

F15-3, F15-4, F15-5, and F15-6 are resolved as document-only corrections. No DML semantics changed.

## Git baseline

Initial state:

- HEAD: `da456da80e5bd756359dcde12d17ad75f7d2fa9f`
- Working tree: clean
- Index: clean
- Architecture diff: none
- No review artifact appeared in initial status.

D15-M1 and D15-M2 were already committed baseline material and remain preserved.

## F15-3 — INSERT physical protection

Original wording:

```text
7. install the heap tuple version and corresponding page_lsn
...
9. release short-lived heap/B+ page latches/pins
```

This could imply that the heap latch/pin remained held through unrelated B+ MTRs and conflated latch and pin release.

Corrected protocol: [§15.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12165)

- Heap tuple and `page_lsn` publish under the heap write guard.
- The heap guard releases latch-before-pin before index MTR work.
- Each B+ MTR obtains and releases its own B+ page guards under the index’s latch/MTR rules.
- Physical guard release does not release transaction-duration logical ownership.

The explanatory boundary is at [§15.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12181).

Canonical owners consumed:

- §5.14 — heap INSERT boundary
- §§7.7 and 7.10.1 — guard, pin/latch, and mutation publication
- §§8.19 and 8.25 — B+ latching and MTR ownership
- §11.3 — logical-lock versus physical-protection separation
- §12.12 — WAL-backed mutation publication

Regression result:

- Heap bytes remain write-latch protected.
- Guard lifetime remains latch-before-pin release.
- B+ physical protection is unchanged and independently owned.
- WAL, `page_lsn`, dirty state, and checksum semantics are unchanged.
- TableWriterGate and UNIQUE_KEY remain transaction-duration resources.
- TUPLE_WRITE/read-epoch rules on paths using them are unchanged.
- INSERT’s canonical semantic order remains unchanged.
- No cross-page or heap-plus-index MTR was invented.

**F15-3: RESOLVED.**

## F15-4 — §15.7.2 retry-policy wording

Original:

> “A future explicit opt-in retry policy may do the same…”

Corrected timeless wording: [§15.7.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12419)

> “The v1 architecture does not define an explicit opt-in whole-request retry policy…”

The existing meaning remains:

- V1 does not transparently rerun an aborted autocommit transaction.
- Client resubmission is a separate request with a new TxnId.
- No separate request may reuse the aborted transaction identity.
- Clean pre-publication RC retry remains unchanged.
- Post-publication same-TxnId retry remains forbidden.
- No retry API, count, timeout, backoff, or option was designed.

**F15-4: RESOLVED.**

## F15-5 — §15.7.3 savepoint wording

Original:

> “Future statement savepoints/subtransactions could relax this v1 restriction…”

Corrected timeless wording: [§15.7.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12434)

> “Statement savepoints, subtransactions, and statement-local physical undo are outside the v1 architecture baseline…”

No savepoint, subtransaction, nested TxnId, CLR, or statement-local rollback protocol was introduced. Mandatory-abort and no-user-DML-undo behavior remain unchanged.

**F15-5: RESOLVED.**

## F15-6 — publication terminology

Original:

> “installed its first persistent write”

Corrected invariant: [§15.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12461)

```text
current_statement_has_published_write == false
```

must still hold before §39.1.2’s first transaction-owned mutation publication at the §12.12 publication point.

This is explicitly distinct from:

- private WAL encoding or reservation;
- WAL write or `fdatasync`;
- page-file durability;
- dirty-page writeback;
- transaction COMMIT.

FA/MA/NC, clean-retry, mandatory-abort, and D15-M2 provisional-count behavior are unchanged.

**F15-6: RESOLVED.**

## Semantic regression assessment

Unchanged:

- D15-M1 canonical `xmax/cmax` representation
- D15-M2 affected-row semantics
- fresh INSERT and UPDATE replacement headers
- statement-attempt and CommandId rules
- RC retry and RR serialization behavior
- TableWriterGate, TUPLE_WRITE, UNIQUE_KEY, and read epochs
- post-wait revalidation and current-owner handling
- current-state uniqueness and SELF_EXCLUDED
- INSERT, UPDATE, and DELETE logical behavior
- FA/MA/NC failure outcomes
- WAL authorization/publication and durability
- no physical user-DML undo
- explicit/autocommit result publication
- recovery’s no-SQL-replay rule
- all persistent formats

No frozen semantic question arose.

## Re-read results

Questions 1–13, F15-3: **YES**.

Questions 14–20, F15-4: **YES**.

Questions 21–27, F15-5: **YES**.

Questions 28–37, F15-6: **YES**.

Questions 38–55, semantic regression: **YES**.

- 56: **NO** — no persistent format changed.
- 57: **NO** — no new semantic question arose.

Documentation-model assessment:

- Project chronology, current-state narration, DEVELOPMENT sequencing, verification procedures, PROJECT_STATE facts, or history introduced: **NO**
- Physical terminology precise: **YES**
- §§15.7.2–15.7.3 timeless: **YES**
- Publication terminology canonical: **YES**
- Analytical rationale retained: **YES**
- Implementation freedom preserved: **YES**
- Readable without implementation-status knowledge: **YES**
- Chapter 15 semantic- and document-model clean: **YES**

## Temporality audit

Remaining Chapter-15 uses of “later” and “initially” describe valid runtime or transaction history, including:

- later transaction-gate waits;
- later statements or commands;
- later COMMIT/ROLLBACK;
- later runtime failure;
- initially discovered stale targets.

No roadmap, phase, current-implementation, or project-time wording remains in the corrected regions.

## Status

- F15-Q1: **CLOSED**
- D15-M1: **RESOLVED AND INTEGRATED**
- F15-Q2: **CLOSED**
- D15-M2: **RESOLVED AND INTEGRATED**
- F15-3: **RESOLVED**
- F15-4: **RESOLVED**
- F15-5: **RESOLVED**
- F15-6: **RESOLVED**
- Frozen Chapter-15 semantic questions: **NONE**
- Chapter-15 architecture: **CLEAN**
- Chapter-15 verification: **SYNCHRONIZATION PENDING**
- Chapter 15 fully closed: **NO**, pending verification synchronization
- Chapter-16 direct review: **NOT STARTED**

Likely later verification families, without editing `VERIFICATION.md`:

1. Statement attempts, CommandId, and RC retry
2. Current-owner and post-wait revalidation
3. Current-state uniqueness, SELF_EXCLUDED, and exact-key scans
4. INSERT heap/index publication and failures
5. UPDATE old/new-version publication and failures
6. DELETE publication and index policy
7. FA/MA/NC and no-physical-undo matrices
8. D15-M1 byte-exact header cases
9. D15-M2 affected-row/retry/no-op/autocommit cases
10. DML and D14-M3 RID-retention integration

## Diff and final Git state

Task-created hunk classifications:

- A — F15-3 INSERT physical-protection wording
- B — F15-3 canonical owner references
- C — F15-4 timeless retry-policy scope
- D — F15-5 timeless savepoint/subtransaction scope
- E — F15-6 canonical publication terminology
- F — precise cross-references
- G — local wrapping

Final state:

- Modified: `docs/ARCHITECTURE.md`
- Index: clean
- HEAD unchanged: `da456da80e5bd756359dcde12d17ad75f7d2fa9f`
- `git diff --check`: passed
- No review artifact present in final status
- No external repository change detected during this cleanup
- Chapter 16, §31.7, and Appendix C unchanged
- Only `docs/ARCHITECTURE.md` was task-modified

No implementation, build, tests, benchmarks, staging, commit, devlog, review artifact, or verification synchronization occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
