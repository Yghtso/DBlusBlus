## 1. Initial HEAD and status

```text
HEAD:
    183c0614814b989a586e3e14e139be5292d325a0

commit:
    183c061 applied FIX-C 31 in ARCHITECTURE

working tree:
    clean

index:
    clean
```

## 2. Final HEAD and status

```text
HEAD:
    183c0614814b989a586e3e14e139be5292d325a0

working tree:
     M docs/ARCHITECTURE.md

index:
    clean
```

## 3. Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) was modified.

## 4. Exact Architecture sections changed

- §15.1.2 — Affected-row result ownership
- §15.5 — COMMIT
- §15.7.3 — External output
- §15.9 — End-to-end invariants
- §26.3.1 — Execution lifecycle and completion
- §26.3.2 — Error ownership and result handoff
- §31.9 — RETURNING spool
- §31.13 — DML/result invariants
- §39.1.2 — First persistent statement write
- §39.1.3 — Statement-error matrix scope
- §39.1.4 — Statement completion and subsystem consequences
- §39.1.5 — COMMIT failure and acknowledgement
- §39.1.7 — Session loss and recovery classification
- §39.1.8 — Forbidden outcome implementations
- §39.3 — Execution errors
- §41.3 — Transaction/recovery verification obligations
- §41.5 — Physical-execution verification obligations

§31.10 did not require modification.

## 5. Approved N31-3 policy restatement

For autocommit DML, every fallible operation required to make successful result publication `R` possible must complete before COMMIT admission at C0.

The prepared result remains unpublished through C0–C5. C5 preserves its backing and cleanup ownership. After successful C4–C5, `R` uses only prepared state and performs no ordinary fallible required work.

## 6. Final pre-C0 preparation contract

Before autocommit DML enters C0:

- DML execution and semantic finalization are complete.
- The exact affected count is finalized.
- The successful result envelope is fully represented.
- RETURNING output, if any, is completely constructed and finalized.
- Every required publication resource has been allocated, validated, registered, and assigned a safe cleanup owner.
- The state is ready to survive C5 and subsequently cross `R`.
- Nothing has yet been published as a successful client result.

Explicit transactions retain their existing preparation-before-`R` contract and do not use this implicit-COMMIT prerequisite.

## 7. Complete required preparation inventory

The Architecture now lists, where applicable:

- final DML semantic execution;
- exact affected count;
- RETURNING row construction;
- result-spool append and required spill writes;
- result-spool finalization;
- schema, descriptor, framing, and extent validation;
- command/count metadata representation;
- successful-envelope representation;
- result-owner preparation;
- cursor preparation;
- spill-handle preparation;
- resource registration;
- publication destination/slot preparation;
- retained value backing and lifetime;
- query-memory accounting;
- actual required physical allocation;
- one valid cleanup owner surviving C5.

No specific C++ container or ownership class is prescribed.

## 8. Reservation-versus-allocation rule

An accounting or budget grant does not prove successful physical allocation.

A conforming implementation cannot:

```text
obtain grant at C1
-> commit successfully
-> attempt first required allocation before R
```

If `R` requires allocated storage, that allocation must already have succeeded before C0.

Allocation for a later delivery chunk or later spool read remains valid post-`R` delivery work.

## 9. C0 admission rule

C0 now admits implicit autocommit DML only when:

- the transaction is `ACTIVE`;
- no ordinary statement execution remains running;
- the complete successful-result candidate is prepared but unpublished;
- every fallible resource required for `R` is available.

Preparation failure occurs before COMMIT admission and follows the real `W` state.

## 10. C1 validation rule

C1:

- validates already-prepared result readiness;
- validates accounting, backing, lifetime, and cleanup ownership through C5 and `R`;
- reserves resources needed for COMMIT’s own runtime publication.

C1 does not:

- construct the statement result;
- perform the first required result allocation;
- become `R`;
- equate a reservation with allocation success.

## 11. C2/C3 regression

Preserved unchanged:

- C2 performs the publication-authorizing `TXN_COMMIT` append and status-page publication.
- C3 establishes durability.
- A known no-append failure can abort.
- Append uncertainty remains noncontinuable/outcome-uncertain.
- Retained exact bytes with durability failure remain `COMMITTING` and retry durability.
- Durable COMMIT is irreversible.

## 12. C4 regression

C4 remains the runtime terminal-publication linearization:

```text
outcome cache = COMMITTED
state = COMMITTED
active-registry removal
```

No result authority is published merely by C4.

## 13. C5 lifetime rule

C5 may release transaction-owned:

- tuple and unique locks;
- writer/schema/statistics/manifest gates;
- snapshots;
- write-status dependencies;
- transaction-owned cleanup state.

C5 must preserve independently retained prepared-result:

- envelope and count;
- schema;
- fixed-width and VARCHAR backing;
- cursor state;
- spill files and handles;
- memory accounting and reservations;
- cleanup responsibility.

Result lifetime must not depend exclusively on a transaction arena destroyed at C5. Transaction locks are not retained merely to keep result data alive.

## 14. C6 acknowledgement regression

For implicit autocommit DML:

```text
C4–C5
-> R
-> C6 response delivery
```

C6 remains acknowledgement/delivery, not result preparation or authority construction.

A C6 transport failure cannot undo COMMITTED or revoke an already completed `R`.

## 15. Final C-to-R transition contract

After successful C4–C5, `R` is a non-failing in-process semantic transition using only prepared state.

It:

- publishes successful statement-result authority;
- makes the count authoritative;
- establishes the logical RETURNING bag;
- transfers or activates result/request ownership.

No required allocation, validation, construction, spill I/O, handle duplication, registration, callback, or cursor construction may remain.

Attempting such work is an internal protocol violation, not an ordinary supported postcommit error.

## 16. Exact limits of “non-failing”

“Non-failing” applies only to ordinary required semantic work in the successful in-process C-to-`R` transition.

It does not assert that:

- processes cannot crash;
- sessions or transports cannot fail;
- future spill reads cannot fail;
- delivery chunks are preallocated;
- every spill block was reread before COMMIT;
- corruption or invariant defects are impossible;
- a client must observe the successful result.

No result replay or recovery mechanism was added.

## 17. No-RETURNING DML handling

INSERT, UPDATE, and DELETE without RETURNING must still prepare before C0:

- command completion metadata;
- exact affected count;
- successful result envelope;
- required publication owner/storage.

They cross `R` but create no fake row or row cursor.

## 18. Zero-row DML handling

Zero-row DML:

- may omit `W`;
- prepares exact count `0`;
- prepares an unpublished command-result envelope;
- satisfies canonical COMMIT prerequisites;
- crosses `R` after successful C4–C5.

Absence of `W` alone does not classify the transaction as read-only or authorize skipping commit prerequisites.

## 19. Final affected-count authority

The exact count is computed and represented before C0 but remains provisional and unpublished.

Authority begins only at `R`.

Therefore:

- COMMIT success alone does not publish the count.
- Crash/session loss before `R` may leave no authoritative published count.
- Missing count delivery is not evidence that no rows were affected.
- After `R`, delivery failure cannot revoke the count.
- No count WAL record or persistent reconstruction was introduced.

## 20. Execution versus COMMIT versus delivery ownership

| Stage | Owner |
|---|---|
| Explicit result preparation before `R` | Statement execution |
| Autocommit result preparation before C0 | Statement execution |
| C0–C5 | Existing COMMIT protocol |
| Successful C-to-`R` | Prepared non-failing result publication |
| After `R` | Result/cursor delivery |
| Process crash/session loss | Existing recovery/session owner |
| Invariant/noncontinuable defect | Existing stronger owner |

## 21. Precommit preparation-failure outcomes

Required result preparation failure before C0:

- is statement/request execution failure;
- uses the actual `W` state;
- exposes no count or RETURNING prefix;
- safely cleans partial/unpublished state;
- cannot be reclassified as delivery failure.

For autocommit, the request terminates through the existing failure/ABORT protocol.

## 22. Existing COMMIT-failure outcomes

Preserved:

- C0/C1 failure before terminal append: COMMIT fails and transaction aborts.
- Known C2 no-append: exact restoration and ABORT.
- C2 uncertainty: noncontinuable/`CommitOutcomeUncertain`.
- Appended record with incomplete publication: noncontinuable; no ABORT redirection.
- C3 retryable durability failure: remain `COMMITTING`.
- Irrecoverable durability uncertainty: recovery-owned uncertainty/noncontinuability.
- Post-durable C4/C5 failure: transaction remains COMMITTED; database may become noncontinuable.
- C6 transport failure: COMMITTED with client-observation uncertainty.

## 23. Postcommit invariant-defect classification

A conforming implementation has no ordinary required result operation between successful C and `R`.

If an implementation nevertheless attempts one, it violates the N31-3 protocol.

If an invariant or noncontinuable defect prevents `R`:

- use the existing stronger error owner;
- never change COMMITTED to ABORTED;
- never fabricate `R` or count authority;
- never authorize replay;
- retain or clean resources only through a known-safe owner;
- introduce no new public error enum.

## 24. Cancellation matrix

| Point | Required result |
|---|---|
| Result preparation before C0 | Statement cancellation; actual `W` determines consequence |
| C0/C1 before authorizing append | Existing cancellation-to-ABORT path |
| After valid authorizing append | COMMIT is uncancellable |
| After successful C, before `R`, usable session | Complete prepared `R`; cancellation applies to delivery |
| After `R` | Result-delivery abandonment; statement remains successful |
| Actual session loss | Independent §39.1.7 handling |

## 25. Process-crash matrix

| Point | Transaction | Result |
|---|---|---|
| Before durable COMMIT | Recovery decides from WAL under existing rules | No fabricated success |
| After durable COMMIT, before C4/C5 | COMMITTED | No result authority required |
| After successful C, before `R` | COMMITTED | `R` may never occur; no count/result reconstruction |
| After `R` | COMMITTED | Published authority existed; delivery may be incomplete |

Missing acknowledgement does not mean failure and does not authorize blind replay.

## 26. Session-loss matrix

| Point | Outcome |
|---|---|
| Explicit transaction ACTIVE | Existing automatic ABORT/session cleanup |
| Autocommit before authorizing append | Existing COMMIT cancellation/ABORT rules |
| After authorizing append, before durability | COMMIT continues or becomes uncertain/noncontinuable |
| After durable COMMIT, before `R` | COMMITTED; `R` may not occur; safe temporary cleanup |
| After `R` | Delivery abandonment; COMMITTED remains |
| Ordinary usable-session spool error | Not session loss |

## 27. Resource/lifetime matrix

| Resource | Before C0 | C0–C5 | At `R` | After `R` |
|---|---|---|---|---|
| Envelope/count | Prepared, unpublished | Retained | Authoritative | Result metadata |
| RETURNING spool | Finalized, unpublished | Retained | Ownership transfer/activation | Cursor-owned |
| VARCHAR/vector backing | Stable | Survives C5 | Transferred/activated | Retained through cursor lifetime |
| Spill files/handles | Valid | Preserved | Result-owned | Cleaned on exhaustion/error/abandonment |
| Memory charge | Live | Continuous | Continuous handoff | Released exactly once |
| Transaction locks | Transaction-owned | Released at C5 | Not required | Absent |
| Cleanup owner | Prepared | Survives C5 | Result owner activated | Performs final cleanup |

## 28. Result-spool accounting regression

The final contract preserves:

- one conceptual accounting owner;
- no unaccounted handoff gap;
- no double release;
- no dangling producer backing;
- no lost spill ownership;
- no failed-attempt reuse;
- no requirement for a duplicate copy when safe ownership transfer is possible.

## 29. Explicit-transaction regression

Preserved:

```text
execution/result preparation
-> R
-> transaction remains ACTIVE
-> result delivery
-> optional later COMMIT/ROLLBACK
```

Post-`R` ordinary delivery failure leaves a usable explicit transaction `ACTIVE`. The autocommit pre-C0 rule was not imposed on the explicit transaction’s later independent COMMIT.

## 30. N31-1 regression

**PRESERVED / CLOSED**

- Ordinary candidates still close before `W`.
- INSERT SELECT still stages complete demanded input.
- UPDATE still uses authoritative post-wait images.
- UNIQUE remains immediate and statement-wide.
- Pending owners remain exact.
- Key swaps remain conflicts.
- Demanded RETURNING expressions still evaluate before `W`.
- Dynamic failures retain actual boundary ownership.
- Same-TxnId retry remains forbidden after `W`.

## 31. N31-2 regression

**PRESERVED / CLOSED**

After `R`:

- ordinary delivery failure does not fail DML;
- explicit transaction remains `ACTIVE` when usable;
- autocommit remains `COMMITTED`;
- count remains authoritative;
- returned prefixes are not retracted;
- cursor error is not `FINISHED`;
- abandonment does not revoke success;
- no DML retry occurs.

## 32. SELECT and Chapter-30 regression

No SELECT or Chapter-30 semantics changed.

- SELECT may still return a prefix before a later query error.
- External Sort readiness is not `R`.
- External Sort may encounter a later spill-read failure.
- DML’s pre-C0 preparation requirement does not impose whole-query buffering on SELECT or Sort.

## 33. DDL/VACUUM/ANALYZE regression

No control-operator sections were modified.

- DDL remains single-coordinator and Chapter-21-owned.
- VACUUM remains Chapter-14 maintenance with no relational result bag.
- Different-table VACUUM concurrency remains legal.
- ANALYZE remains transactional system DML with one publication coordinator.
- No parallel capability or result surface was introduced.

## 34. Chapter-15 edits

Chapter 15 now:

- prepares count representation before autocommit COMMIT;
- makes prepared result readiness a C0 prerequisite;
- makes C1 validate rather than construct result state;
- preserves result backing across C5;
- orders implicit DML `R` before C6 response delivery;
- separates preparation, COMMIT, `R`, and delivery;
- adds corresponding invariants.

## 35. Chapter-26 edits

Chapter 26 now clarifies:

- successful autocommit execution includes full result-publication preparation before C0;
- pipeline readiness, reservation, or an unbacked cursor is insufficient;
- prepared state survives C5;
- `R` uses already-prepared state;
- cancellation after the authorizing append cannot reopen execution;
- process crash and session loss remain independent owners.

SELECT and Sort handoffs were explicitly left unchanged.

## 36. Chapter-31 §31.9 edits

§31.9 now canonically defines:

- explicit and autocommit lifecycle sequences;
- pre-C0 preparation inventory;
- reservation/allocation distinction;
- no-RETURNING and zero-row behavior;
- pre-C0 execution failure;
- C0–C5 COMMIT ownership;
- C5 lifetime survival;
- non-failing C-to-`R`;
- cancellation after commit append;
- crash/session-loss behavior;
- invariant/noncontinuable defect behavior;
- unchanged post-`R` delivery semantics.

## 37. Chapter-31 invariant edits

The invariants now state:

- pre-C0 physical allocation is required where publication needs it;
- C1 validates prepared state;
- C5 preserves independent backing and accounting;
- successful C-to-`R` performs no ordinary fallible required work;
- crash/session loss before `R` leaves COMMITTED without result reconstruction or replay;
- the former blanket “all pre-R failure is W-based execution failure” is replaced by explicit/auto stage ownership.

## 38. Chapter-39 reconciliation

Chapter 39 now distinguishes:

1. explicit pre-`R` or autocommit pre-C0 preparation failure;
2. C0–C5 COMMIT failure;
3. successful non-failing C-to-`R`;
4. post-`R` delivery failure;
5. independently fatal corruption, invariant, session, or noncontinuable failure.

It explicitly forbids:

- C0 admission with incomplete result resources;
- reservation-as-allocation;
- ordinary fallible work between C and `R`;
- attempts to ABORT COMMITTED.

## 39. Chapter-41 obligations

Architecture Verification obligations now require proof of:

- complete fallible preparation before C0;
- allocation versus reservation;
- C1 validation;
- C5 lifetime survival;
- no fallible required C-to-`R` work;
- no-RETURNING and zero-row envelopes;
- COMMIT uncertainty preservation;
- cancellation after append;
- crash/session loss before `R`;
- post-`R` first/later read failure;
- no replay or abort after COMMITTED;
- continuous accounting and exact cleanup.

No test procedure was added to `VERIFICATION.md`.

## 40. Full mandatory scenario matrix

| Case | Transaction outcome | Statement/result authority | Count authority | Client result/error | Retry | Cleanup owner |
|---|---|---|---|---|---|---|
| A. Result-owner allocation fails before C0 | Actual `W`: FA/MA; autocommit fails/aborts | No `R` | None | `OutOfMemory`/owned error | Only existing pre-W internal rule; never post-W | Statement/attempt |
| B. Grant succeeds, allocation fails before C0 | Same as A | No `R` | None | `OutOfMemory`; grant released | Same as A | Statement/attempt |
| C. Spool validation fails before C0 | Actual `W` decides; autocommit fails | No `R` | None | `SpillIOError` | No post-W retry | Statement/spill owner |
| D. No-RETURNING metadata construction fails | Actual `W` decides; autocommit fails | No `R` | None | Allocation/representation error | Existing boundary only | Statement/attempt |
| E. Zero-row preparation fails | No `W`; recoverable pre-write failure, autocommit request aborts | No `R` | None | Controlled preparation error | No blind whole-request retry | Statement/attempt |
| F. C0/C1 fails | `ACTIVE -> MUST_ABORT -> ABORTED` | No `R` | None | COMMIT failure | No same-transaction retry | COMMIT/ABORT |
| G. C2 append known absent | ABORTED after exact restoration | No `R` | None | COMMIT failure | No external COMMIT retry | COMMIT/ABORT |
| H. C2 append uncertain | Recovery-owned; database noncontinuable | No `R` | None | `CommitOutcomeUncertain` | Forbidden | WAL/recovery |
| I. C3 durability retry required | Remains `COMMITTING` | No `R` yet | None | Request pending | No cancellation/replay | COMMIT/WAL |
| J. Durable commit; C4/C5 incoherent | COMMITTED; database noncontinuable | No `R` | None | Outcome uncertain/fatal | No ABORT/replay | COMMIT/runtime owner |
| K. C5 succeeds | COMMITTED | Prepared but unpublished until immediate `R` | None until `R` | No response yet | None | Prepared-result owner |
| L. Required allocation attempted C→R | COMMITTED | Protocol violation; no fabricated `R` | None | Existing invariant/noncontinuable owner | Forbidden | Known-safe retained owner |
| M. Required spill I/O attempted C→R | COMMITTED | Protocol violation; no fabricated `R` | None | Existing invariant/noncontinuable owner | Forbidden | Known-safe retained owner |
| N. C succeeds; R succeeds | COMMITTED | Statement successful | Authoritative | Success eligible for C6 delivery | None | Result owner |
| O. Cancellation after authorizing append | COMMIT continues or becomes owner-defined uncertain; never ABORT by cancellation | `R` after successful C | At `R` | Cancellation applies to delivery | None | COMMIT then result owner |
| P. Pending cancellation after C, usable session | COMMITTED | Complete `R`, then abandon delivery | Authoritative | Delivery cancellation | None | Result owner |
| Q. Crash after durable COMMIT before R | COMMITTED on recovery | `R` may not occur | Not reconstructed | No/uncertain response | No blind replay | Recovery/OS cleanup |
| R. Disconnect after durable COMMIT before R | COMMITTED | `R` need not occur | None published | Client outcome uncertain | No automatic replay | Session/COMMIT cleanup |
| S. First post-R spill read fails | COMMITTED | Successful statement | Authoritative | Cursor `SpillIOError`, no rows | None | Result owner |
| T. Later post-R read fails after prefix | COMMITTED | Successful statement | Authoritative | Prefix retained; cursor error | None | Result owner |
| U. Post-R delivery-chunk allocation fails | COMMITTED | Successful statement | Authoritative | Terminal delivery OOM/error | None | Result owner |
| V. Post-R cursor abandoned | COMMITTED | Successful statement | Authoritative | Unread rows discarded | None | Result owner |
| W. Explicit R then delivery failure | Transaction remains `ACTIVE` if usable | Successful statement | Authoritative | Cursor error; later commands/COMMIT/ROLLBACK allowed | No DML retry | Result owner |

## 41. Global stale-rule search results

| Searched stale rule | Final result |
|---|---|
| Blanket W-based treatment for every pre-`R` failure | Removed; explicit/pre-C0/COMMIT/post-R stages distinguished |
| Result preparation after C5 | Forbidden |
| Fallible cursor construction after C before R | Forbidden |
| Fallible result-owner allocation after COMMIT | Forbidden |
| Reservation equals allocation success | Explicitly rejected |
| C5 destroys all result/request backing | Explicitly rejected |
| Cancellation after authorizing append can ABORT | Explicitly rejected |
| R performs required spill I/O | Explicitly rejected |
| R first establishes result storage | Rejected; storage prepared before C0 |
| Count becomes authoritative at C | Rejected; authority remains at `R` |
| Missing response proves failure | Rejected |
| Postcommit failure authorizes replay | Rejected |
| No-RETURNING skips envelope preparation | Rejected |
| Zero-row DML skips `R` | Rejected |
| Post-`R` error revokes success | Rejected |

No unrelated SELECT or Chapter-30 rule was altered.

## 42. Document-role audit

**PASS**

All added wording is timeless Architecture:

- no implementation-status claims;
- no review history;
- no roadmap;
- no commit references;
- no benchmark claims;
- no concrete C++ class mandate;
- no new persistent result format;
- no new WAL record;
- no response replay protocol.

The only “subsequent” language describes runtime ordering.

## 43. No physical undo introduced

Confirmed. N31-3 does not add statement-level physical undo or permit committed mutations to be reversed.

## 44. No new COMMIT stage introduced

Confirmed. C0–C6 numbering and semantics remain intact.

`C` remains completion of the existing C4–C5 withholding requirement. `R` remains a separate result-publication handoff.

## 45. No persistent result store introduced

Confirmed.

Prepared results remain temporary, memory-accounted, spill-managed, non-WAL, nonpersistent, and not crash-recovered.

## 46. New semantic question discovered

```text
NONE
```

## 47. `git diff --check` result

```text
clean
```

## 48. Fix-D-only diff summary

```text
docs/ARCHITECTURE.md | 325 lines changed
260 insertions
65 deletions
```

The diff is limited to the N31-3 result-preparation, COMMIT handoff, error ownership, and Architecture verification-obligation contracts.

## 49. Final Git status

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

## 50. Confirmations

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
```

```text
Fix A:
    PRESERVED

N31-1:
    PRESERVED / CLOSED

N31-2:
    PRESERVED / CLOSED

N31-3:
    CLOSED
```

CHAPTER 31 FIX D COMPLETE —
READY FOR FINAL READ-ONLY ARCHITECTURE CLOSURE AUDIT