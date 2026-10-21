# Chapter 31 — DML, DDL, VACUUM, and Result Interface Verification

This section is the dedicated verification family for §§31.1–31.13. It composes the
existing transaction, locking, WAL, reclamation, memory, pipeline, expression, statistics,
and COMMIT procedures; it does not redefine their semantic owners. Every fixture records
the statement attempt, CommandId, snapshot, exact `W` event, `C0`–`C6` events where
applicable, `R`, result delivery, transaction state, and cleanup ownership.

The procedures use `W`, `C`, and `R` only as verification event labels:

```text
W = first transaction-owned WAL-backed persistent statement mutation
C = completion of the existing autocommit C4–C5 requirement
R = atomic successful statement-result publication and result ownership transfer
```

`W` is not statement success. `C` is not `R`. Pipeline `Finalize`, readiness, cursor
construction, first `Next()`, first row, and cursor `FINISHED` are not `R`. Explicit DML
uses ordinary closure -> optional `W` -> execution/result preparation -> `R` -> delivery.
Autocommit DML uses ordinary closure -> optional `W` -> complete fallible preparation ->
C0–C5 -> `R` -> C6 delivery. Zero-row DML may omit `W`, but still prepares its command
result and crosses `R`.

## V31-A — W/C/R event model and independent result oracle

Create an explicit-event harness over the existing DML and COMMIT harness. Pause at target
finalization, ordinary-candidate reduction, `W`, result preparation, C0, C1, C2, C3, C4,
C5, `R`, C6, first delivery, later delivery, and `FINISHED`.

The independent oracle must distinguish:

```text
candidate established       != candidate published
W                           != statement success
C                           != R
R                           != first Next()/first row
R                           != successful FINISHED
count established           != count authoritative
count authoritative         != count delivered
```

Expected results are stable under legal batching, worker scheduling, target order, spill
order, and cursor chunking. A failed cursor operation is terminal error, never successful
`FINISHED`.

## V31-B — Ordinary DML candidate closure and provenance

For INSERT, UPDATE, and DELETE, construct competing semantically demanded ordinary
candidates with different source spans, phases, and logical occurrences. Vary vector width,
batch boundaries, target/source order, permitted worker schedule, hash order, and
in-memory versus spilled staging. The independent Chapter-21 oracle must select the same
winner and the same pre-W recoverable consequence in every execution shape.

Cover arithmetic, conversion, assignment/coercion, runtime NOT NULL, immediate UNIQUE,
demanded RETURNING expressions, demanded INSERT SELECT source expressions, and applicable
qualification/subquery candidates. Do not add unsupported constraint features.

Assert that no persistent user-DML mutation occurs while a demanded ordinary candidate can
still be established. A first discovery of an ordinary candidate after `W` is an invariant
failure, not an expected post-W MA case. Dynamic failures remain outside this ranking.

## V31-C — INSERT VALUES closure and publication

Use valid and invalid logical input occurrences, including:

```text
valid row followed by divide-by-zero
valid row followed by conversion/cast failure
valid rows followed by runtime NOT NULL failure
valid multirow INSERT with RETURNING
INSERT (1), (1) under a unique key
```

For ordinary failures, assert complete pre-W closure, no earlier heap/index publication,
the canonical diagnostic, existing FA behavior, no successful count, and no RETURNING
prefix. For success, assert exact logical multiplicity, final count, unordered RETURNING
bag, `R`, and post-`R` delivery behavior.

## V31-D — INSERT SELECT staging, demand, and spill

Use a source that yields valid rows followed by a demanded ordinary source expression or
conversion error. Assert that source computation may vectorize or pipeline into accounted
temporary staging, but target publication cannot begin while a later demanded ordinary
candidate remains possible.

Force tiny query budgets and verify exact spill/reload preservation of occurrence identity,
NULLs, VARCHAR bytes, provenance, and candidate values. Inject OOM and SpillIO during
staging and classify them as dynamic failures at their actual pre-W boundary. Source
encounter order is not SQL semantics. A same-table INSERT SELECT uses the frozen snapshot
and current-command rules and cannot self-feed newly inserted rows.

## V31-E — UPDATE/DELETE target domain, RID lifetime, and revalidation

Verify for UPDATE and DELETE:

```text
complete target spool before mutation
exact RID deduplication and one-target-once behavior
Halloween protection through index-key, heap-version, and predicate changes
read-epoch to TUPLE_WRITE handoff
VACUUM/RID-retention protection through queued and granted claims
post-wait re-fetch, identity validation, and predicate revalidation
authoritative old-row image before candidate acceptance
```

The concurrent writer/VACUUM interleaving must prove the actual snapshot, read-epoch, and
live-claim chain. The target spool alone is not a RID-retention claim. A stale pre-wait
image cannot supply SET, unique-key, or RETURNING state.

UPDATE tests additionally verify simultaneous SET evaluation, complete new-row staging,
NOT NULL and UNIQUE closure, exact old-RID exclusion, and demanded RETURNING closure.
DELETE tests verify authoritative old-row RETURNING, no unnecessary new-row construction,
one logical target once, and retention of physical secondary-index entries for VACUUM.

## V31-F — Immediate UNIQUE and pending-owner preparation

Verify the complete finite statement-wide UNIQUE_KEY set, canonical lock order, current-state
checks after waits, and terminal lock lifetime. Cover committed owners, earlier-command
owners, current-command owners, exact same-statement pending owners, full-key collisions,
and spilled/reloaded key state.

Required assertions:

```text
INSERT (1), (1) fails before W through exact pending-owner state
UPDATE self-exclusion applies only to the exact authoritative old RID
another target's old key is not excluded
A: 1 -> 2 and B: 2 -> 1 remains an immediate conflict
hash collisions cannot merge distinct canonical keys
```

The procedure must not verify deferred final-state uniqueness or rely on prior physical
publication of one staged candidate to discover another.

## V31-G — READ COMMITTED retry and failure-class separation

Force an authorized pre-W revalidation conflict. Assert same logical statement and
CommandId, fresh READ COMMITTED snapshot, discarded target/candidate/RETURNING state,
discarded pending owners, ordinary candidates, provisional count, cursors, and attempt-local
spill, followed by complete rebuild. Only the final attempt may supply the error, count, or
result.

Repeat after `W` and after `R`; no same-TxnId retry is permitted. Transaction-owned locks
and gates retain their Chapter-11/15 lifetime.

The failure matrix must classify ordinary expression/conversion/assignment/NOT NULL/UNIQUE/
RETURNING errors as pre-W closure candidates. Deadlock, serialization conflict, READ
COMMITTED conflict, OOM, SpillIOError, cancellation, storage/WAL failure, corruption, and
invariant failure retain their owner-specific runtime consequences and are never source-span
ranked against ordinary candidates. A dynamic cancellation/OOM/storage failure after some
mutations may produce the existing MA outcome; ordinary pre-write error closure
does not make mutation failure-free.

## V31-H — RETURNING pre-R construction and publication barrier

Verify INSERT RETURNING final new rows, UPDATE RETURNING authoritative final new rows, and
DELETE RETURNING authoritative old rows. Verify exact bag multiplicity, unordered semantics,
stable value ownership, and no borrowed producer backing escape.

Inject failure during expression evaluation, spool append, spill write, finalization,
structural validation, physical allocation, and ownership preparation. Before `R`, no count
or RETURNING prefix is client-visible; pre-W ordinary failures use closure, post-W dynamic
failures use the existing execution consequence, and autocommit preparation failures before
C0 prevent COMMIT admission.

## V31-I — Autocommit pre-C0 result preparation

For autocommit INSERT, UPDATE, and DELETE, with and without RETURNING and with zero rows,
pause before C0 and independently fault:

```text
execution/semantic finalization
affected-count finalization
RETURNING construction, append, spill, and finalization
schema/descriptor/framing/extent validation
envelope representation
result owner/cursor preparation
spill-handle preparation
publication-destination preparation
required physical allocation
resource registration and cleanup-owner preparation
```

Every row below is an independent preparation boundary. The test harness must report entry
into the named boundary before arming its fault; if the boundary is never entered, setup
fails rather than passing vacuously. Record each boundary's actual position relative to
ordinary closure and `W`. RETURNING expression evaluation and retention of its demanded
values belong to pre-W ordinary closure under Architecture §31.9; they are not postponed
until after that closure. Other required result-readiness work may follow closure but must
finish before C0. Inject ordinary RETURNING expression errors in the closure phase and
check its candidate oracle; later resource/I/O failures retain their dynamic category.

| Preparation category | Semantic boundary and injected fault | Reusable owner or required hook | Expected event trace and outcome | Cleanup/accounting oracle |
|---|---|---|---|---|
| DML semantic finalization | Final execution/Finalize has produced the complete provisional count and result inputs; inject the finalization failure | V26-H semantic finalization and V26-I error-owner transport | Finalization entered -> injected failure -> `W` is whatever the execution ledger records -> C0 not entered -> no `R`; use the Chapter-39 owner for that W state | V26-M reset/teardown; provisional count and result state are discarded |
| Affected-count representation | Final count is known and its required command metadata representation is requested; inject representation failure | V31 event hook at count-finalization boundary; V24-D ownership lifecycle for charged storage | Count established but not published -> targeted failure -> no C0, COMMIT append, `R`, or public count | Count backing and charge released exactly once |
| RETURNING row construction/append | Operation-specific final/old image is converted into one retained RETURNING occurrence; inject construction or spool append failure | V31-H plus V21-14 RETURNING bag oracle; V24-F/M retained-row and teardown owners | Construction/append entered -> failure -> no public prefix and no `R` | Retained values and partial spool are discarded; no borrowed backing survives |
| Required spill write | A required unpublished result block is written to temporary spill; inject short write/I/O failure | V24-J framing/access validation and V24-L resource-error taxonomy | Spill write entered -> `SpillIOError` -> C0 not entered -> no COMMIT append or `R` | Spill handle, partial block, and accounting are reclaimed by one owner |
| Spool finalization | Required result-spool directory/end marker/index is finalized; inject finalization failure | V24-J/K spill framing and reclamation; V31 hook identifies the finalization boundary | Finalization entered -> failure -> no C0, `R`, count, or RETURNING delivery | Temporary spill namespace and charges are reclaimed; no failed-attempt reuse |
| Schema/descriptor validation | Prepared result schema/descriptor is checked against the bound statement; inject validation failure | V21-1 bound-result handoff and V22-B output-schema mapping | Validation entered -> failure -> no C0 or `R`; classify as the existing validation/statement owner | Prepared descriptors are released without invalidating transaction-owned catalog state |
| Framing/extent/representability | Required value/result extent is checked before publication; inject malformed/unsupported extent or representability failure | V24-G/I exact retained-form and arithmetic procedures | Extent check entered -> failure -> no C0 or `R`; preserve the V24 error class | No truncation or narrowed value; provisional storage and charges are released |
| Command/result-envelope construction | The complete unpublished no-RETURNING or RETURNING envelope is assembled; inject construction failure | V31 event hook at envelope-construction boundary; V26-J external-publication handoff as the result-envelope oracle | Construction entered -> failure -> no successful envelope, C0, `R`, or delivery | Envelope-owned resources are destroyed exactly once |
| Result-owner storage acquisition | Required request/result owner storage is acquired before C0; inject acquisition failure | V24-D grant/allocation distinction plus required V31 owner-acquisition hook | Allocation attempt entered -> failure -> no C0; a reservation alone is not success | Allocation and reservation state are released without a charge gap |
| Cursor construction, where required | Required cursor state is constructed before C0; inject construction failure | V31 result-interface hook; V26-J handoff and V26-M backing-release procedures | Cursor construction entered -> failure -> no C0 or `R`; this is not a post-R delivery error | Cursor/backing ownership is discarded exactly once |
| Spill-handle acquisition/duplication | A handle needed to make the prepared result publishable is acquired or duplicated; inject failure | V24-K spill namespace/reclamation plus required V31 handle hook | Handle operation entered -> failure -> no C0 or `R` | No leaked file/handle; existing spill owner reclaims the temporary object |
| Resource registration | Required result resource is registered with its request/result owner; inject registration failure | V24-D ownership transfer and V24-M teardown plus required V31 registration hook | Registration entered -> failure -> no C0 or `R` | Pre-registration resources remain private and are released once |
| Publication destination/slot preparation | Destination used by `R` is made available; inject preparation failure | Required V31 publication-destination hook; V26-J publication handoff is the ownership oracle | Destination preparation entered -> failure -> no C0 or `R` | Destination and backing remain unexposed and fully cleaned |
| Required physical allocation after budget approval | Budget grant succeeds, then the required physical allocation fails | V24-D grant-versus-allocation procedure and V31-J negative control | Grant -> allocation attempt -> failure -> C0 refused; C1 must not perform the first required allocation | Grant is released; no live allocation remains uncharged |
| Retained-value/backing lifetime preparation | Required fixed/VARCHAR/dictionary backing is made independent of transaction teardown; inject lifetime-preparation failure | V23-G/H borrowing/backing owners and V24-F/M retained-row cleanup | Lifetime preparation entered -> failure -> no C0 or `R` | No dangling producer/transaction-arena pointer; backing is released once |
| Memory-accounting preparation | Required charge/coverage transfer is prepared; inject accounting/coverage failure | V24-B/C/D complete ledger and transfer-interval procedures | Accounting preparation entered -> failure -> no C0 or `R` | No unaccounted interval or double charge; unused reservation is released |
| Cleanup-owner preparation | Exactly one cleanup owner is established for prepared result state; inject owner-registration failure | V24-D/M ownership lifecycle and required V31 cleanup-owner hook | Cleanup-owner preparation entered -> failure -> no C0 or `R` | Every partial resource has a known owner; no leak, dangling pointer, or duplicate release |

For every injected required-preparation failure, the common event oracle is:

```text
actual ordinary-closure/W position recorded for this preparation boundary
targeted preparation boundary entered
injected failure observed
C0 NOT entered
no COMMIT-authorizing append
R NOT occurred
no successful result/count publication
no RETURNING prefix delivered
safe cleanup and accounting closure
```

The actual `W` event remains authoritative: `W = false` uses the canonical pre-W
consequence, while `W = true` uses the canonical post-W execution consequence unless a
stronger owner applies. A successful control for every row must show the operation before
C0, successful C0 admission, and subsequent C0–C5/`R` ordering. Preparation is not
delivery: a required owner allocation, spool write/finalization, or envelope construction
is tested before C0; a delivery-chunk allocation, future spill read, or transport operation
is tested separately after `R` by V31-M.

## V31-J — Reservation/allocation, C0–C6, and C5 lifetime

Run the negative reservation test: budget grant succeeds, required physical allocation
fails, and C0 admission is refused. Assert C1 validates already-prepared state and performs
no first required result allocation.

Reuse COMMIT fault injection for known C2 no-append, uncertain C2 append, incomplete
post-append publication, retryable C3 durability, irrecoverable uncertainty, durable COMMIT
with C4/C5 failure, and C6 transport failure. Preserve exact `CommitOutcomeUncertain`,
noncontinuable, and recovery-owned outcomes.

Place barriers around C5 and verify result envelope, count, schema, fixed/variable backing,
dictionary/constant vectors, cursor, spill files/handles, reservations, accounting, and
cleanup ownership survive C5. Verify transaction locks, gates, snapshots, and status
dependencies release normally and are not retained merely for result lifetime.

## V31-K — Non-failing C-to-R transition

Pause after successful C4–C5 and before `R`, then resume through `R` while recording a
structured operation trace. Each trace record contains:

```text
phase boundary: C4-complete, C5-complete, C-to-R, R-entry, R-published, or post-R-delivery
operation category
required-to-establish-R: yes/no
attempted: yes/no
ordering sequence
success, injected failure, or non-failing completion
owner and cleanup consequence
```

The required hook is semantic rather than a prescribed tracing implementation. It must
expose or reconstruct suboperations hidden behind ownership transfer; a generic
"ownership transfer" event cannot conceal an allocation, validation, I/O, registration,
or cursor-construction attempt.

The zero-operation oracle requires zero attempted operations marked
`required-to-establish-R` in the interval from successful C4–C5 through successful `R`:

```text
physical allocation
memory-budget grant
result validation or construction
spill read/write required for R
handle acquisition/duplication
resource registration
fallible cursor construction
external callback required for R
publication-destination preparation
```

An attempted operation is a conformance failure even if it succeeds. Existing COMMIT
internals through C5, non-failing semantic activation at `R`, instrumentation itself, and
post-`R` delivery operations are not forbidden. The classifier marks an operation as
required publication work when successful `R` depends on its completion; relabeling that
operation as delivery does not change its classification.

The positive control must observe at least one allowed fallible preparation operation
before C0 and at least one permitted delivery allocation or spill read after `R`. If the
required trace events are unavailable, the result is `NOT VERIFIED / TEST INFRASTRUCTURE
INCOMPLETE`, never a pass.

The conforming outcome is atomic publication of statement success, affected-count authority,
complete logical RETURNING ownership, and result/request ownership from already-prepared
state. An implementation attempting forbidden required work is nonconforming, not an
ordinary postcommit failure path.

If an invariant defect prevents `R`, assert the existing invariant/noncontinuable owner:
COMMITTED remains COMMITTED; no fabricated `R` or count; no ABORT; no automatic replay;
cleanup only through a known-safe owner.

## V31-L — Cancellation, crash, and session loss

Inject cancellation before W, after W during preparation, during pre-C0 preparation, before
the authorizing append, after the append, after C but before `R` with a usable session, and
after `R` during delivery. Assert uncancellable COMMIT after the valid authorizing append,
completion of prepared `R` after C, and delivery-only cancellation after `R`.

Crash and disconnect at pre-COMMIT, post-append/pre-durability, post-durable/pre-C4/C5,
post-C/pre-R, post-R, and delivery boundaries. Recovery follows WAL evidence. Durable
COMMIT remains COMMITTED; `R`, count reconstruction, result replay, and blind request replay
are not invented. A usable-session spill-read error is not session loss.

## V31-M — Post-R delivery, cursor, count, and cleanup

After `R`, separately fail the first spilled-result read, a later read after a delivered
prefix, delivery-chunk allocation, and transport. Also abandon cursors before and after a
prefix and complete successful exhaustion.

Assert statement success, authoritative count, explicit `ACTIVE` or autocommit `COMMITTED`
state, retained prefix, terminal cursor error distinct from `FINISHED`, no DML retry, and
exact result-owned cleanup. Verify chunk lifetime through the next `Next()` or destruction.

Run all DML forms with and without RETURNING. Count is independently checked as established,
published at `R`, and delivered. No-RETURNING uses metadata, not a fake row; zero-row DML
uses count zero and still crosses `R`.

## V31-N — DDL, VACUUM, ANALYZE, and cross-owner role checks

Reuse existing control-operator, catalog, VACUUM, and statistics procedures to verify only
Chapter-31 dispatch and role boundaries:

```text
DDL: single coordinator, private CREATE INDEX, correct metadata/completion ownership
VACUUM: nontransactional maintenance, no relational/progress SQL rows, same-table
         serialization, different-table concurrency, permitted DML/ANALYZE coexistence
ANALYZE: one publication coordinator, stable snapshot/manifest, transactional statistics
         rows, no incomplete global StatsVersion, no invented parallel ANALYZE
```

Do not duplicate the underlying Chapter-14, Chapter-21, or Chapter-34 procedures.

## V31 atomic architecture-obligation ledger

The following atomic checks are owned by the procedures above; existing component
procedures remain reusable owners for their lower-level details.

| ID | Obligation | Procedure | Owner/cross-reference |
|---|---|---|---|
| V31-001 | W/C/R events are distinct | V31-A | §§15.5, 31.9, 39.1.2 |
| V31-002 | `R` is not first `Next()` or `FINISHED` | V31-A | §§26.3, 31.10 |
| V31-003 | Ordinary winner is physical-order independent | V31-B | §§21.16.1, 25.1.2 |
| V31-004 | Ordinary candidate closure precedes W | V31-B | §§15.1.3, 31.13 |
| V31-005 | INSERT later ordinary error has no earlier write | V31-C | §§15.2, 39.1.3 |
| V31-006 | INSERT success count/multiplicity is exact | V31-C | §§15.1.2, 31.9 |
| V31-007 | INSERT SELECT cannot stream before closure | V31-D | §§20.13, 31.6 |
| V31-008 | INSERT SELECT staging preserves demand/occurrences | V31-D | §§20.14, 24.7 |
| V31-009 | Target spool finalizes before mutation | V31-E | §§31.1, 31.3 |
| V31-010 | Exact RID dedup is collision-safe | V31-E | §31.2 |
| V31-011 | RID retention protects revalidation | V31-E | §§14.6, 14.18 |
| V31-012 | UPDATE old image is post-wait authoritative | V31-E | §§11.3, 15.3 |
| V31-013 | DELETE uses authoritative old image | V31-E | §15.4 |
| V31-014 | UNIQUE lock set is complete and ordered | V31-F | §§11.8–11.9 |
| V31-015 | Pending duplicate owners are exact | V31-F | §11.10 |
| V31-016 | Key swaps remain immediate conflicts | V31-F | §11.10.6 |
| V31-017 | RC retry discards all attempt-local state | V31-G | §§15.7.1, 31.5 |
| V31-018 | Retry after W/R is forbidden | V31-G | §§15.7.2, 39.1.4 |
| V31-019 | Dynamic failures are not ordinary candidates | V31-G | §§26.3.2, 39.1.3 |
| V31-020 | RETURNING uses correct final/old image | V31-H | §21.15, §31.9 |
| V31-021 | Pre-R result rows are unpublished | V31-H | §§15.7.3, 31.9 |
| V31-022 | All required preparation precedes C0 | V31-I | §§15.5, 31.9 |
| V31-023 | No-RETURNING is still an envelope result | V31-I | §§15.1.2, 31.9 |
| V31-024 | Zero-row DML still prepares and crosses R | V31-I | §31.9 |
| V31-025 | Reservation does not prove allocation | V31-J | §§24.5, 31.9 |
| V31-026 | C1 validates; it does not construct | V31-J | §15.5 |
| V31-027 | C5 preserves independent result ownership | V31-J | §§15.5, 24.10 |
| V31-028 | C2/C3 uncertainty retains existing owner | V31-J | §§12.10.5, 15.5, 39.1.5 |
| V31-029 | C-to-R has no required fallible work | V31-K | §§31.9, 39.1.5 |
| V31-030 | Protocol defect cannot ABORT COMMITTED | V31-K | §§9.14, 39.1.5 |
| V31-031 | Cancellation after append cannot abort COMMIT | V31-L | §§15.5, 39.1.5 |
| V31-032 | Crash before R preserves committed effects | V31-L | §§12, 39.1.5–39.1.7 |
| V31-033 | Session loss is distinct from spill-read error | V31-L | §39.1.7 |
| V31-034 | Post-R spill error is delivery-only | V31-M | §§31.9–31.10, 39.3 |
| V31-035 | Prefix is not retracted | V31-M | §31.10 |
| V31-036 | Cursor error is not FINISHED | V31-M | §31.10 |
| V31-037 | Count authority begins at R | V31-M | §§15.1.2, 31.9 |
| V31-038 | Result cleanup is exactly once | V31-M | §§23.10, 24.10, 31.10 |
| V31-039 | DDL result/control role is correct | V31-N | §§21, 31.11 |
| V31-040 | VACUUM role/concurrency/result surface is correct | V31-N | §§14, 31.12 |
| V31-041 | ANALYZE publication role is correct | V31-N | §§34, 31.12.1 |

This ledger adds 14 V31 procedure families and 41 atomic V31 obligations. It does not
replace the existing Chapter-15 atomic inventory; those procedures remain reusable component
coverage and their stale expectations are repaired above.

## Chapter 31 subsection coverage map

| Architecture subsection | V31 owner | Reused component owner |
|---|---|---|
| §31.1 target materialization | V31-E | DML target-spool and RID-retention procedures |
| §31.2 exact target identity | V31-E | existing duplicate-target/RID oracle |
| §31.3 Halloween protection | V31-E | indexed UPDATE and target-finalize procedures |
| §31.4 memory/spill | V31-D, V31-E, V31-M | V24 memory/spill procedures |
| §31.4.1 candidate staging | V31-B, V31-D, V31-I | `V24-M — Retry, cancellation, teardown, and retained ownership`; `Pipeline Finalization and Resource Tests` |
| §31.5 revalidation/retry | V31-E, V31-G | `Transaction identity, snapshot, and status verification`; `Isolation Tests`; `Locking and Gate Tests`; `V21-2 — Statement attempts, CommandId, and retry` |
| §31.6 INSERT | V31-C, V31-D | existing INSERT protocol matrix |
| §31.7 UPDATE | V31-E, V31-F, V31-G | existing UPDATE protocol matrix |
| §31.8 DELETE | V31-E, V31-F | existing DELETE protocol matrix |
| §31.9 RETURNING/W/C/R | V31-A, V31-H–V31-M | COMMIT and result-envelope procedures |
| §31.10 cursor interface | V31-A, V31-M | `V26-J — Internal output and external publication`; `V26-M — Backing release, borrowing, and reset`; `V30-G — Temporary format, error categories, and Source failure` |
| §31.11 DDL | V31-N | `Control-Operator Tests`; `Catalog Tests` |
| §31.12 VACUUM | V31-N | `Vacuum and Reclamation Tests` |
| §31.12.1 ANALYZE | V31-N | `Statistics Publication and Versioning Tests` |
| §31.13 invariants | V31 atomic ledger | §§41.3 and 41.5 obligation maps |

## Chapter 41 obligation coverage map

| Chapter-41 obligation | V31 coverage |
|---|---|
| Ordinary closure before W and order independence | V31-B–V31-G |
| INSERT SELECT staging and spill | V31-D |
| Authoritative UPDATE image and immediate UNIQUE | V31-E–V31-F |
| Retry freshness and no retry after W | V31-G |
| Pre-C0 preparation and physical allocation | V31-I–V31-J |
| C1 readiness and C5 lifetime | V31-J |
| Non-failing C-to-R | V31-K |
| COMMIT uncertainty and cancellation | V31-J, V31-L |
| Crash/session loss before R | V31-L |
| Post-R spill failures and cursor state | V31-M |
| No-RETURNING, zero-row, count authority | V31-I, V31-M |
| Memory accounting and cleanup | V31-D, V31-J, V31-M |
| DDL/VACUUM/ANALYZE roles | V31-N |

## V31 negative-case consistency checks

The live procedures must contain no expected ordinary DML error after `W`, no physical
visitation-dependent FA/MA outcome, no direct streaming INSERT SELECT publication while a
demanded ordinary candidate remains possible, no uniqueness check that depends on prior
physical insertion, no result allocation first attempted after COMMIT, no C5 destruction of
prepared result backing, no required fallible C-to-R operation, no post-R abort/replay, no
cursor-error-as-`FINISHED`, and no missing-response-as-failure assertion. Any such case is
classified by V31-B, V31-G, V31-K, or V31-M and fails the corresponding verification procedure.
