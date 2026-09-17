## 1. Initial HEAD/status

- HEAD: `017602be41954863c53f897e53918e0b2ed768f7`
- Commit: `017602b applied FIX-A 31 in ARCHITECTURE`
- Working tree: clean
- Index: clean

## 2. Final HEAD/status

- HEAD unchanged: `017602be41954863c53f897e53918e0b2ed768f7`
- Working tree: `docs/ARCHITECTURE.md` modified
- Index: clean

## 3. Fix-A preservation status

Preserved. The committed Fix-A rules remain intact:

- DML mutation/write publication is single-worker without forcing scans or evaluation to be single-worker.
- VACUUM has no relational result-row bag or speculative future progress rows.
- One coordinator owns PhysicalAnalyze publication without inventing parallel ANALYZE.

## 4. Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

## 5. Exact Architecture sections modified

- §§11.9, 11.10.3, 11.10.5, 11.10.6, 11.10.10, 11.15
- §§15.1.2, 15.1.3, 15.2–15.4, 15.7.1, 15.9
- §§21.13, 21.15, 21.16.1, 21.20
- §25.1.2
- §§31.1, 31.4.1, 31.5–31.9, 31.13
- §§39.1.3, 39.1.4, 39.1.8
- §§41.3–41.5

## 6. Approved N31-1 policy restatement

Every semantically demanded ordinary DML candidate in the final attempt must be established and canonically reduced before the first persistent user-DML statement write. A winning ordinary candidate fails before that boundary.

## 7. Final ordinary-candidate closure definition

The final lifecycle is:

1. Establish complete demanded input or finalized target domain.
2. Establish authoritative row images.
3. Evaluate and retain ordinary-candidate-producing values.
4. Perform authoritative immediate-uniqueness preparation.
5. Close and canonically reduce the candidate domain.
6. If no candidate wins, begin persistent mutation.
7. Complete writes, affected count, and statement-owned RETURNING spool.

Chapter 20 demand and Chapter 21 precedence remain authoritative.

## 8. Final ordinary-candidate class table

| Class | Pre-write closure |
|---|---|
| Scalar arithmetic/division | Required |
| Cast/conversion | Required |
| INSERT input conversion | Required |
| Demanded INSERT SELECT source expression/subquery | Required |
| UPDATE assignment/coercion | Required |
| Demanded target-qualification ordinary error | Required |
| Runtime NOT NULL | Required |
| Immediate UNIQUE/PRIMARY KEY | Required |
| Demanded RETURNING expression | Required |
| Other supported deterministic row constraint | Required |

No unsupported CHECK feature was introduced.

## 9. Final dynamic-failure exclusion table

| Dynamic class | Treatment |
|---|---|
| Deadlock | Occurrence-owned; not source-ranked |
| Serialization/revalidation conflict | Existing conflict/retry owner |
| OOM | Actual pre/post-write boundary |
| SpillIOError | Actual pre/post-write boundary |
| Cancellation | Actual pre/post-write boundary |
| Storage/WAL/I/O failure | Existing lower-layer owner |
| Persistent corruption | Existing noncontinuable classification |
| Internal invariant failure | Existing noncontinuable classification |

## 10. First-persistent-write definition regression

Unchanged. Section 39.1.2 remains canonical.

Temporary staging, spill writes, tuple encoding, logical locks, candidate construction, and WAL reservation do not cross the boundary. Only a transaction-owned WAL-backed database mutation published through the existing Chapter-12 boundary does.

## 11. Final conceptual DML phase model

The Architecture now separates:

- input/target-domain completion;
- authoritative ordinary-candidate closure;
- canonical error reduction;
- persistent mutation;
- statement completion.

Closure success is not itself a persistent write and does not guarantee later dynamic success.

## 12. Memory-bounded staging owner

Chapter 24 owns the exact, query-temporary, attempt-local, memory-accounted, spill-capable retained state. Existing `RowCollection`, target/RETURNING spools, exact hash/sort structures, or equivalent representations may be used.

## 13. INSERT VALUES staging semantics

All demanded occurrences are converted, completed, NOT-NULL checked, keyed, RETURNING-evaluated, and retained before publication. No first row may publish while a later demanded occurrence can establish an ordinary candidate.

## 14. INSERT SELECT staging semantics

The complete semantically demanded source is consumed into temporary candidate staging before target publication. Source-to-target persistent streaming before closure is forbidden.

## 15. INSERT SELECT spill semantics

Unbounded candidate volume may spill through Chapter 24. Spill must preserve exact occurrence multiplicity, row values, keys, provenance, and RETURNING values.

## 16. INSERT SELECT demand/self-read regression

- Vectorized/pipelined computation into staging remains legal.
- Chapter 20 demand is unchanged.
- Source encounter order is not semantic.
- Newly inserted rows cannot self-feed the prepublication source scan.

## 17. UPDATE target-domain semantics

The exact RID-deduplicated target spool remains finalized before mutation. Target-domain finalization and ordinary-candidate closure are explicitly separate required barriers.

## 18. UPDATE authoritative-old-image semantics

Every candidate derives from the exact post-lock, revalidated old version authorized for mutation. SET, new-row construction, unique keys, and RETURNING cannot rely on an unconfirmed spool image.

## 19. UPDATE lock-wait/revalidation semantics

After every wait, target authority is revalidated. Changed authority invalidates staged assignment, row, key, RETURNING, and pending-owner state and invokes the existing retry/conflict outcome. An authorized fresh attempt rebuilds the complete lock set and closure state.

## 20. UPDATE candidate new-row semantics

All SET expressions read one immutable complete old-row image. Assignment coercions, complete new-row construction, NOT NULL, keys, and RETURNING are completed before publication. `SET a=b, b=a` remains a swap.

## 21. DELETE closure semantics

DELETE establishes authoritative old rows, evaluates demanded RETURNING, completes affected lock preparation, and closes ordinary candidates before `xmax/cmax` publication. It does not construct unused new-row state.

## 22. RETURNING pre-write semantics

Demanded RETURNING expressions are evaluated and exact result rows retained before the first persistent DML write. A winning RETURNING expression error therefore prevents mutation. Rows remain unpublished until existing statement-success rules permit exposure.

No post-success result-ownership rule was added.

## 23. Same-statement pending UNIQUE-owner semantics

An exact attempt-local relation maps:

```text
(IndexId, full canonical key)
    -> prospective candidate occurrences and diagnostic provenance
```

It detects staged same-statement duplicates before either candidate publishes. Hash collisions, visit order, worker order, and spill order cannot define identity or diagnostics.

## 24. Existing-owner UNIQUE semantics

Committed/frozen owners, earlier-command owners in the same transaction, and authoritative current old owners remain visible to current-state uniqueness checks independently of the statement snapshot.

## 25. UNIQUE lock acquisition/order semantics

The complete finite `UNIQUE_KEY` set is acquired before publication in canonical:

```text
(IndexId, lexicographic encoded user-key bytes)
```

order. Checks are repeated authoritatively after grants. Deadlock remains a dynamic failure.

## 26. Exact old-RID exclusion regression

UPDATE may exclude only its exact authoritative old RID where the existing self-exclusion rule permits it. No TxnId-wide, CommandId-wide, other-target, or staged-row exclusion was introduced.

## 27. Key-swap regression

`A:1→2, B:2→1` still fails immediate uniqueness because each candidate encounters the other target’s authoritative old owner. Final-state deferred uniqueness was not introduced.

## 28. Current-command owner regression

Earlier commands’ published owners remain authoritative. Current-statement unpublished candidates are handled by exact pending-owner semantics rather than requiring one staged row to publish first.

## 29. TUPLE_WRITE claim/revalidation semantics

UPDATE/DELETE may acquire the complete required exact target claims before publication. Claims retain their existing transaction lifetime. No page latch, index latch, or read epoch may be held across a blocking logical-lock wait.

## 30. READ COMMITTED retry semantics

Same-TxnId retry remains permitted only before the first persistent statement write. An authorized retry:

- discards all closure state;
- unregisters the old snapshot;
- captures a fresh statement snapshot;
- rebuilds the complete input/target and candidate domain.

Retry after publication remains forbidden.

## 31. Failed-attempt cleanup/freshness

Cleanup includes candidate staging, pending owners, RETURNING rows, ordinary candidates, provisional count, closure cursors, and attempt-local spills. Transaction-owned locks and writer-gate ownership follow their existing owners.

## 32. Affected-row count regression

Staged candidate count is never public affected count. Only the final successful mutation attempt contributes. Ordinary pre-write failure or dynamic post-write failure publishes no successful count.

## 33. Resource-failure-before-write semantics

OOM, SpillIOError, representability failure, or cancellation during staging uses existing pre-write dynamic-failure rules. Implementations may spill exactly or fail controllably; they may not drop candidates or publish rows to relieve memory pressure.

## 34. Dynamic-failure-after-write semantics

Deadlock where reachable, cancellation, OOM, storage/WAL failure, corruption, and other dynamic failures may still occur after publication and retain Chapter-39 consequences.

## 35. Cancellation regression

Cancellation was not converted into an ordinary candidate. It follows the actual first-write state and cannot expose staged RETURNING or partially publish candidates as successful DML.

## 36. Chapter-11 edits and rationale

Chapter 11 now defines:

- complete statement-wide unique-lock preparation;
- exact same-statement pending owners;
- prepublication duplicate detection;
- immediate key-swap rejection;
- unchanged exact-RID self-exclusion;
- explicit lock-footprint and contention consequences.

## 37. Chapter-15 edits and rationale

Chapter 15 now owns the monotonic physical DML lifecycle, exact spill-capable staging, per-operation pre-write closure, authoritative target images, retry cleanup, affected-count behavior, and post-closure publication handoff.

## 38. Chapter-21 edits and rationale

Chapter 21 remains the canonical owner of candidate eligibility and precedence. It now expressly requires the complete demanded candidate domain to close before publication, making its physical-order-independence rule operational rather than aspirational.

## 39. Chapter-31 §31.6 INSERT edits

The old row/batch-at-a-time-looking execution description was replaced by:

- complete staging and ordinary closure;
- statement-wide uniqueness preparation;
- then mutation-ready publication.

Batching and locality optimization remain legal after closure.

## 40. Chapter-31 §31.7 UPDATE edits

UPDATE now has explicit target, authoritative closure, and write phases. Mutation consumes only successfully closed candidates, and no ordinary row-semantic candidate may first arise during write publication.

Fix A’s single-worker publication wording remains unchanged.

## 41. Chapter-31 §31.8 DELETE edits

DELETE now closes authoritative old-row and demanded RETURNING candidates before mutation while retaining its lightweight no-new-row model and VACUUM-owned index cleanup.

## 42. Chapter-31 §31.9 RETURNING edits

Only N31-1 was applied:

- demanded RETURNING evaluation occurs before persistent mutation;
- exact result rows remain statement-owned;
- pre-success construction/finalization failure remains execution failure;
- no result prefix escapes.

Post-success spill-read ownership remains intentionally unresolved pending Fix C.

## 43. Chapter-39 stale-example table

| Old scenario/rule | Classification | Final action |
|---|---|---|
| Rows 1–4 publish; row 5 conversion/arithmetic/constraint error | Ordinary contradiction | Replaced with pre-write ordinary failure and no rows published |
| Constraint checking may merely be ordered early; outcome depends on boundary | Ordinary contradiction | Replaced by mandatory closure before publication |
| Post-write failure example | Needed MA illustration | Reframed using cancellation/OOM/storage/WAL dynamic failure |
| Forbidden “commit rows 1–4 after row 5 failed” | Ambiguous ordinary framing | Narrowed to partial effects after post-write dynamic failure |
| UNIQUE before/after-write verification | Post-write ordinary case unreachable | Changed to pre-write outcome plus rejection of post-write discovery |

## 44. Final Chapter-39 ordinary/dynamic distinction

- Winning ordinary DML candidates are pre-write `FA` where recoverable.
- Post-write discovery of a demanded ordinary candidate is an execution invariant defect.
- Dynamic failures may occur on either boundary side and retain existing `FA`, `MA`, or noncontinuable outcomes.

## 45. Chapter-41 obligation edits

Future Verification must prove:

- complete ordinary closure before publication;
- spill-staged INSERT SELECT;
- authoritative post-wait old images;
- simultaneous UPDATE assignments;
- pre-write RETURNING;
- complete canonical unique-lock acquisition;
- exact pending owners;
- exact old-RID exclusion;
- immediate key-swap rejection;
- physical-order-independent winner and `FA`;
- no new ordinary candidate after publication;
- dynamic failures remain boundary-owned.

## 46. Complete global stale-N31-1 search table

| Search/result | Classification |
|---|---|
| Old row-5 conversion/arithmetic/constraint example | Removed |
| “Constraint checking may be ordered early” | Removed |
| Sequential check-and-publish wording | Removed |
| Current-command earlier-row truth-table entry | Valid defensive current-state predicate; normal staged duplicates use pending owners |
| Rows 1–4 then row-5 failure | Retained only for dynamic failure |
| Streaming target publication | Appears only as an explicit prohibition |
| Later-target RETURNING error | Appears only as a pre-write negative example |
| Post-write ordinary UNIQUE verification | Removed/replaced |
| Generic ownership-transfer wording elsewhere | Unrelated Chapter-23/24/26 ownership concepts |
| N31-2 result-ownership boundary language | No new hit |

## 47. INSERT VALUES error example result

Valid occurrence 1 plus occurrence 2 divide-by-zero:

- divide-by-zero enters closure;
- no occurrence publishes;
- recoverable explicit transaction remains ACTIVE under existing pre-write rules.

## 48. INSERT SELECT later-error example result

Valid source rows followed by a demanded conversion error:

- source ordinary domain closes before publication;
- no target row publishes;
- source staging may spill.

## 49. UPDATE later-SET/NOT-NULL example result

Target A valid and target B failing SET/NOT NULL:

- both candidates are considered before mutation;
- Chapter 21 selects the canonical winner;
- target A does not publish.

## 50. UPDATE later-RETURNING example result

A later target’s demanded RETURNING error closes before mutation. No earlier target mutation or RETURNING prefix occurs.

## 51. Same-statement duplicate UNIQUE example result

`INSERT ... VALUES (1), (1)` is rejected through exact pending-owner semantics before either row publishes. Physical visitation does not designate a semantic winner.

## 52. UPDATE key-swap example result

`A:1→2, B:2→1` remains an immediate conflict against current authoritative owners. It is not accepted as a deferred final-state permutation.

## 53. Implementation-freedom assessment

Preserved. Architecture does not mandate one staging class, vector width, spill partitioning scheme, hash/sort structure, tuple batch, or publication order.

## 54. Memory/accounting assessment

All statement-wide retained state is charged through existing query-memory and spill owners, including rows, old/new images, keys, pending owners, RETURNING values, provenance, cursors, and spill buffers.

## 55. Performance consequence documented correctly

Yes. The Architecture explicitly records increased:

- blocking before first write;
- memory and spill use;
- unique-lock and target-claim footprint;
- lock duration/contention;
- latency to first persistent mutation.

Vectorized evaluation, batching, and locality-aware publication remain allowed.

## 56. No physical user-DML undo introduced

Confirmed.

## 57. No deferred uniqueness introduced

Confirmed. Uniqueness remains immediate, current-state, and lock-protected.

## 58. No semantic DML row order introduced

Confirmed. RID, source encounter, batch, spool, hash, worker, lock, and mutation orders remain nonsemantic.

## 59. Fix-A wording preserved

Confirmed for DML, VACUUM, and ANALYZE.

## 60. N31-1 status

`CLOSED`

## 61. N31-2 status

```text
APPROVED ALTERNATIVE A
NOT IMPLEMENTED
```

No result-publication boundary `R`, post-success spool-read consequence, post-R cancellation rule, or result-ownership transfer was introduced.

## 62. New semantic question

`NONE`

## 63. `git diff --check` result

Passed with no diagnostics.

## 64. Cumulative Architecture diff summary

Against HEAD:

```text
docs/ARCHITECTURE.md | 672 lines changed
543 insertions
129 deletions
```

## 65. Final git status

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

## 66. Confirmations

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
```

CHAPTER 31 FIX B COMPLETE —
READY FOR N31-2 RESULT-OWNERSHIP REPAIR