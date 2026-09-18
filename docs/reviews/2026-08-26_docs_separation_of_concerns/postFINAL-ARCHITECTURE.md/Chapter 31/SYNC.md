## 1. Initial HEAD/status

```text
HEAD:
03f802d9d5794d61e48e4bc502593beb25d4fb89

Commit:
03f802d applied FIX-D 31 in ARCHITECTURE

Working tree:
clean

Index:
clean
```

## 2. Final HEAD/status

```text
HEAD:
03f802d9d5794d61e48e4bc502593beb25d4fb89

Working tree:
M docs/VERIFICATION.md

Index:
clean
```

## 3. Files modified

Only:

[docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md)

## 4. Exact Verification sections modified

- Existing DML harness and result-envelope procedures.
- Existing D15-M2 affected-row matrix.
- Existing DML error/result matrix.
- Existing Chapter-15 obligation map entry 168.
- New Chapter-31 verification family beginning at line 22400.

## 5. Architecture status

```text
CLEAN — CLOSED / UNMODIFIED
```

`docs/ARCHITECTURE.md` was not modified.

## 6. Final V31 section

```text
## Chapter 31 — DML, DDL, VACUUM, and Result Interface Verification
```

Location: [docs/VERIFICATION.md:22400](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:22400)

## 7. V31 subsection inventory

14 new procedure families:

```text
V31-A  W/C/R event model
V31-B  ordinary DML candidate closure
V31-C  INSERT VALUES
V31-D  INSERT SELECT staging/spill
V31-E  UPDATE/DELETE target and RID lifecycle
V31-F  immediate UNIQUE/pending owners
V31-G  retry and failure-class separation
V31-H  RETURNING pre-R
V31-I  autocommit pre-C0 preparation
V31-J  reservation/allocation and C0–C6
V31-K  non-failing C-to-R
V31-L  cancellation/crash/session loss
V31-M  post-R delivery/cursor/count/cleanup
V31-N  DDL/VACUUM/ANALYZE roles
```

## 8. New atomic V31 procedures

```text
Procedure families: 14
Atomic V31 obligations: 41
```

## 9. Repaired existing procedures

Repaired:

- Deterministic DML harness event model.
- Affected-row/result-envelope verification.
- RETURNING exposure procedure.
- D15-M2 affected-row matrix.
- DML error/result matrix.
- Existing atomic obligation 168 wording.

## 10. Stale-generic-case repair table

| Stale case | Repair |
|---|---|
| Seven published rows then unspecified row-eight MA | Rewritten as later dynamic failure after W |
| Second row-eight MA example | Rewritten as dynamic post-W failure |
| Post-write UNIQUE violation | Marked unreachable for ordinary candidates |
| Post-write NOT NULL/row constraint | Marked unreachable for ordinary candidates |
| Post-write ordinary expression/type/conversion | Marked unreachable for ordinary candidates |
| Statement success/result/response conflation | Replaced with explicit W, C, R, and C6 events |

## 11. N31-1 ordinary-candidate coverage

V31-B through V31-G verify:

- complete ordinary candidate closure before W;
- canonical Chapter-21 reduction;
- physical-order independence;
- arithmetic, conversion, assignment, NOT NULL, UNIQUE, RETURNING, and source-expression candidates;
- no ordinary candidate first discovered after W;
- dynamic failures excluded from ordinary ranking.

## 12. INSERT VALUES coverage

V31-C covers:

- later divide-by-zero;
- later conversion failure;
- later NOT NULL failure;
- duplicate same-statement keys;
- exact count and RETURNING multiplicity;
- no pre-W publication;
- no partial successful result.

## 13. INSERT SELECT coverage

V31-D covers:

- complete demanded source consumption;
- spill-capable staging;
- source errors after valid rows;
- NULL/VARCHAR/provenance preservation;
- OOM and SpillIO;
- nonsemantic source order;
- self-read snapshot/current-command behavior.

## 14. UPDATE target/RID/revalidation coverage

V31-E covers:

- finalized target spool;
- exact RID deduplication;
- Halloween protection;
- read epoch and TUPLE_WRITE handoff;
- VACUUM/RID retention;
- post-wait revalidation;
- authoritative old-row images;
- simultaneous assignment;
- complete new-row and RETURNING staging.

## 15. DELETE coverage

V31-E covers:

- finalized target domain;
- authoritative old image;
- one target once;
- DELETE RETURNING before W;
- exact count;
- no unnecessary replacement row;
- secondary-index garbage remaining VACUUM-owned.

## 16. Immediate UNIQUE/pending-owner coverage

V31-F covers:

- complete finite key set;
- canonical lock order;
- current-state checks after waits;
- committed and same-transaction owners;
- exact same-statement pending owners;
- collision-safe full-key comparison;
- exact old-RID exclusion;
- immediate key-swap rejection.

## 17. READ COMMITTED retry coverage

V31-G verifies:

- same CommandId;
- fresh snapshot;
- discarded old target/candidate/RETURNING state;
- discarded pending owners, errors, count, cursors, and spill;
- complete rebuild;
- no retry after W, R, or COMMIT.

## 18. Ordinary versus dynamic errors

Ordinary errors are verified as pre-W closure candidates.

Dynamic failures retain runtime ownership:

```text
deadlock
serialization conflict
READ COMMITTED conflict
OOM
SpillIOError
cancellation
storage/WAL failure
corruption
invariant failure
```

Permitted dynamic post-W MA examples remain covered.

## 19. RETURNING pre-R coverage

V31-H verifies:

- INSERT final-new-row images;
- UPDATE authoritative final-new-row images;
- DELETE authoritative old-row images;
- unordered bag semantics;
- exact multiplicity;
- no borrowed backing escape;
- no pre-R count or row publication;
- pre-C0 autocommit preparation failures.

## 20. W/C/R event model

The verification model now distinguishes:

```text
W = first persistent statement mutation
C = completed autocommit C4–C5
R = successful result publication and ownership transfer
```

Neither `W`, C, pipeline readiness, cursor creation, first `Next()`, first row, nor `FINISHED` is equivalent to `R`.

## 21. Pre-C0 preparation coverage

V31-I verifies all required preparation before C0:

- execution finalization;
- count;
- RETURNING construction;
- spool append/spill/finalization;
- schema and framing validation;
- envelope;
- cursor/owner;
- spill handles;
- publication destination;
- physical allocation;
- registration;
- cleanup ownership.

## 22. Reservation-versus-allocation coverage

V31-J adds the required negative case:

```text
reservation succeeds
physical allocation fails
C0 admission is refused
```

C1 is verified as validation only.

## 23. C0/C1 coverage

V31-I and V31-J verify:

- unpublished prepared result at C0;
- C0 admission prerequisites;
- C1 validation of backing, accounting, ownership, and lifetime;
- no first required allocation at C1.

## 24. C2/C3 uncertainty regression

V31-J reuses existing COMMIT fault injection for:

- known no-append;
- uncertain append;
- incomplete post-append publication;
- retryable durability failure;
- irrecoverable uncertainty;
- recovery-owned outcomes.

## 25. C4/C5 lifetime coverage

V31-J verifies that result backing, count, cursor, spill handles, reservations, accounting, and cleanup ownership survive C5 while transaction locks and gates release normally.

## 26. C-to-R no-fallible-work coverage

V31-K verifies no required allocation, grant, validation, construction, spill I/O, registration, callback, or cursor construction occurs between C and R.

A violation is classified as nonconforming/invariant failure, not ordinary postcommit execution failure.

## 27. C6 delivery coverage

V31-J and V31-M verify:

- R precedes successful C6 delivery;
- C6 performs delivery only;
- transport failure cannot undo COMMITTED;
- client acknowledgement uncertainty remains distinct from transaction outcome.

## 28. No-RETURNING and zero-row coverage

V31-I and V31-M cover:

- DML without RETURNING;
- exact metadata/count envelope;
- zero-row DML;
- no fake row cursor;
- zero count;
- R despite absent W.

## 29. Explicit post-R failure coverage

V31-M verifies that usable explicit transactions remain `ACTIVE` after post-R delivery failure, with later statements, COMMIT, and ROLLBACK still available.

## 30. Autocommit post-R failure coverage

V31-M verifies that autocommit remains `COMMITTED` after post-R spill-read, chunk-allocation, transport, or abandonment failure.

## 31. Count authority versus delivery coverage

The harness distinguishes:

```text
count established
count published at R
count delivered to client
```

Post-R delivery failure cannot revoke count authority.

## 32. Cursor error versus FINISHED coverage

V31-A and V31-M require:

```text
chunk
FINISHED = successful exhaustion only
terminal error
```

A later delivery failure cannot be reported as successful `FINISHED`.

## 33. Cancellation coverage

V31-L covers cancellation:

- before W;
- after W during preparation;
- during pre-C0 preparation;
- before authorizing append;
- after authorizing append;
- after C before R;
- after R during delivery.

COMMIT remains uncancellable after the valid authorizing append.

## 34. Process-crash and session-loss coverage

V31-L covers crash/disconnect before COMMIT, during COMMIT, after durable COMMIT before R, after R, and during delivery.

It verifies:

- WAL-owned transaction outcome;
- committed effects remain committed;
- no result reconstruction;
- no automatic replay;
- no conflation of spill-read failure with session loss.

## 35. Result-spool accounting/cleanup coverage

V31-J and V31-M verify continuous accounting and exactly-once cleanup across preparation, C0–C5, R, delivery, failure, exhaustion, abandonment, and session loss.

## 36. SELECT/Sort regression coverage

Existing SELECT and Chapter-30 behavior is preserved:

- SELECT may return a prefix before a later error;
- prefix is not retracted;
- later Source spill failure may fail complete SELECT;
- Sort readiness is not DML R;
- SELECT is not forced into complete-result buffering.

## 37. DDL coverage

V31-N verifies:

- single-coordinator DDL;
- private CREATE INDEX publication;
- correct command metadata;
- no fabricated row bag;
- transaction-gate ownership.

## 38. VACUUM coverage

V31-N verifies:

- nontransactional maintenance;
- completed units survive cancellation/failure;
- same-table serialization;
- different-table concurrency;
- permitted DML/ANALYZE coexistence;
- no relational or progress/debug SQL rows.

## 39. ANALYZE coverage

V31-N verifies:

- one publication coordinator;
- stable snapshot and manifest;
- transactional statistics rows;
- no incomplete global StatsVersion;
- correct failure after statistics publication;
- no invented parallel ANALYZE.

## 40. Complete Chapter-31 subsection coverage matrix

Added coverage for:

```text
§31.1       V31-E
§31.2       V31-E
§31.3       V31-E
§31.4       V31-D, V31-E, V31-M
§31.4.1     V31-B, V31-D, V31-I
§31.5       V31-E, V31-G
§31.6       V31-C, V31-D
§31.7       V31-E, V31-F, V31-G
§31.8       V31-E, V31-F
§31.9       V31-A, V31-H–V31-M
§31.10      V31-A, V31-M
§31.11      V31-N
§31.12      V31-N
§31.12.1    V31-N
§31.13      V31 atomic ledger
```

## 41. Chapter-41 obligation coverage matrix

Added mapping for:

- ordinary closure;
- INSERT SELECT staging;
- authoritative UPDATE images;
- UNIQUE preparation;
- retry freshness;
- pre-C0 allocation;
- C1/C5;
- C-to-R;
- COMMIT uncertainty;
- cancellation;
- crash/session loss;
- post-R delivery;
- count authority;
- memory/cleanup;
- DDL/VACUUM/ANALYZE.

## 42. Reused existing procedure inventory

Reused:

- Statement Failure and Transaction-State Tests;
- COMMIT Fault-Injection Tests;
- Isolation Tests;
- Locking and Gate Tests;
- Vacuum and Reclamation Tests;
- Query Memory/Spill procedures;
- Pipeline Finalization and Resource Tests;
- existing DML execution harness;
- uniqueness and current-owner matrices;
- affected-row and result-envelope procedures;
- Control-Operator Tests;
- Statistics Publication and Versioning Tests;
- Chapter-30 Source failure procedures.

## 43. Duplicate-ID check

```text
PASS
No duplicate V31 atomic IDs.
No duplicate V31 family IDs.
```

## 44. Broken-reference check

```text
PASS
Referenced Architecture sections exist.
Referenced existing Verification families exist.
```

## 45. Global stale-wording search results

No stale live cases remain for:

- row-eight ordinary MA;
- post-W ordinary UNIQUE/NOT NULL/expression failure;
- direct streaming INSERT SELECT publication;
- uniqueness requiring prior physical insertion;
- result allocation after COMMIT;
- C5 destroying result backing;
- required fallible C-to-R work;
- post-R abort or replay;
- cursor error equal to FINISHED;
- missing response proving nonexecution.

Remaining matches are valid unrelated terminology or explicit V31 prohibitions.

## 46. Document-role audit

Pass.

The changes contain:

- verification procedures;
- fault points;
- expected outcomes;
- ownership assertions;
- cross-references.

They do not contain implementation status, test results, build claims, chronology, or new Architecture semantics.

## 47. New semantic questions

```text
NONE
```

## 48. `git diff --check`

```text
PASS
```

## 49. Diff summary

```text
docs/VERIFICATION.md | 407 lines changed
385 insertions
22 deletions
```

Only `docs/VERIFICATION.md` was modified.

## 50. Final Git status

```text
 M docs/VERIFICATION.md
```

The index remains clean.

## 51. Explicit confirmations

```text
ARCHITECTURE NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
CHAPTER 32 REVIEW NOT STARTED
```

CHAPTER 31 VERIFICATION SYNCHRONIZATION COMPLETE —
READY FOR READ-ONLY VERIFICATION CLOSURE AUDIT