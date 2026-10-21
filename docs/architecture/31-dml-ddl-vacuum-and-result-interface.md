# 31. DML, DDL, VACUUM, and Result Interface

## 31.1 UPDATE/DELETE target materialization

UPDATE and DELETE use a statement-level target spool before any target mutation begins.

Read phase:

```text
scan/bind the logical target relation
    ↓
apply WHERE and supported target-producing joins
    ↓
materialize target RID
    +
required old values
```

Every semantically demanded ordinary candidate produced while evaluating the
target scan, WHERE predicate, or another supported target-producing child is
retained for the statement's §21.16.1 closure. Target-domain finalization and
later authoritative candidate-row closure are distinct requirements, and no
target mutation may publish if either fails.

After successful spool Finalize, UPDATE/DELETE first complete authoritative
ordinary DML candidate closure:

```text
iterate finalized target rows
    ↓
acquire exact logical write claims
    ↓
re-fetch/revalidate
    ↓
retain authoritative old-row images
    ↓
evaluate all demanded ordinary-candidate-producing work
    ↓
prepare immediate uniqueness and reduce ordinary candidates
```

The persistent write phase begins only after that closure succeeds:

```text
consume mutation-ready staged targets
    ↓
perform Chapter-15 MVCC mutations
```

This is a deliberate pipeline breaker and the primary Halloween-protection boundary.

## 31.2 One logical target once

A logical target RID is represented at most once in one UPDATE/DELETE target spool.

The physical plan may satisfy this in either of two ways:

```text
prove the target-producing child has unique target RID
or
explicitly de-duplicate target RID during materialization
```

A join-expanded target plan MUST NOT cause the same physical target version to be mutated twice merely because multiple input rows produced the same RID.

The spool's RID-uniqueness mechanism is query-temporary and may itself use memory/spill-aware distinct/sort support where required.

## 31.3 Halloween protection

The target spool prevents UPDATE from rediscovering its own mutations through:

```text
an index whose key is modified
a newly created heap version
an altered predicate value
```

Current-command MVCC visibility remains important but is not the sole Halloween defense.

No target-table mutation starts before the target identity set is finalized.

## 31.4 Target-spool memory and spill

Small target sets use a query-owned `RowCollection`.

Large sets spill through SpillManager.

The spool row layout contains at least:

```text
RID
old values needed by assignment expressions
old values needed for complete new tuple construction
values needed for unique-key handling
values needed for RETURNING
```

Columns not required by these semantics are not materialized merely because they exist in the table.

Spill/reload preserves RID identity and old value bytes exactly for this statement attempt.

### 31.4.1 Ordinary-candidate staging storage

The §15.1.3 closure phase may extend the target spool, use the RETURNING
spool, or use another implementation-equivalent query-temporary retained
representation. Its memory and spill ownership remains Chapter 24's. Across
chunk reuse, lock waits, spill/reload, and Finalize it preserves exactly the
required logical occurrence, target RID, authoritative old-row values,
complete candidate new row, NULLs, VARCHAR bytes, hidden slots/provenance,
unique-key material or an exact reconstructable equivalent, demanded
RETURNING values, pending-owner state, and ordinary-error metadata.

The retained representation is attempt-local, non-WAL, and nonpersistent. A
hash may accelerate exact lookup but cannot establish target/key identity by
collision alone. No candidate value may be truncated, canonicalized, dropped,
or persistently published to reduce memory pressure; the operator instead
uses exact spill/progress or reports the existing controlled dynamic
`OutOfMemory`, `SpillIOError`, representability, or cancellation failure on
the actual pre-write side of §39.1.

## 31.5 Revalidation and READ COMMITTED retry boundary

Materialization does not eliminate write races.

Before UPDATE/DELETE of each spooled target:

```text
acquire TUPLE_WRITE
re-fetch RID
revalidate tuple/version state
apply Chapter-11 isolation conflict rules
```

The Chapter-15 retry boundary remains authoritative.

If READ COMMITTED discovers a retry-requiring conflict **before this statement attempt has produced any persistent WAL-visible write**:

```text
discard target spool
discard mutation-ready candidate staging
discard pending unique-owner state
discard buffered RETURNING
discard ordinary error candidates and provisional affected count
discard attempt-local spill files/cursors
capture a fresh statement snapshot
rebuild the complete target/input and closure domain
```

If any persistent statement write has already occurred:

```text
same-TxnId whole-statement restart is forbidden
transaction -> MUST_ABORT -> automatic ABORT/conflict outcome
```

This follows the Chapter-15 retry boundary and the no-physical-user-DML-undo architecture.

## 31.6 INSERT execution

`PhysicalInsert` is a blocking sink with respect to persistent user-DML
publication. Its staging/closure phase consumes the complete semantically
demanded typed logical input:

```text
1. evaluate/convert target-column vectors and construct complete candidate rows
2. enforce runtime NOT NULL and preserve every ordinary candidate/provenance
3. derive every affected canonical unique key
4. evaluate and retain every demanded RETURNING row
5. retain exact mutation-ready candidates in memory-accounted, spill-capable
   statement storage
6. acquire the complete finite UNIQUE_KEY set in §11.9 order
7. perform authoritative §11.10 current-state checks, including owners from
   earlier commands, and exact same-statement pending-owner checks
8. close and reduce the complete ordinary candidate domain through §21.16.1
```

No heap or index mutation for the statement publishes before successful
closure. For INSERT SELECT, source evaluation may remain vectorized and may
pipeline into the temporary candidate spool, but source-to-target persistent
publication cannot stream while a later demanded source occurrence may still
establish an ordinary candidate. Source occurrence multiplicity, semantic
demand, snapshot/current-command self-read behavior, and unordered encounter
order remain Chapter 20-owned.

After successful closure, the single-worker mutation/write-publication phase
installs only the retained mutation-ready candidates through Chapter 15 and
the required B+ MTR paths. It may amortize:

```text
tuple encoding
FSM candidate lookup
heap-page latching
index-key encoding
```

across batches when doing so does not weaken lock/WAL ordering. Physical
publication order remains nonsemantic and cannot create a new ordinary
candidate.

## 31.7 UPDATE execution

After target-spool finalization, `PhysicalUpdate` completes this authoritative
closure phase over every distinct final target before any mutation publishes:

```text
1. acquire/register each exact TUPLE_WRITE claim through §11.3 and re-fetch/
   revalidate after grant to establish the authoritative target and old image
2. vector-evaluate every SET RHS against that target's one complete immutable
   old-row image, apply bound coercions, and construct the complete new row
3. enforce descriptor-wide NOT NULL, derive all old/new unique keys, and
   evaluate/retain demanded RETURNING new-row values
4. acquire the complete finite affected UNIQUE_KEY set in §11.9 order
5. after every wait, re-fetch/revalidate; if an authoritative version changed,
   discard stale SET, row, key, RETURNING, and pending-owner state and take the
   existing retry/conflict outcome; an authorized fresh attempt derives its
   complete lock set again
6. perform §11.10 current-state and pending-owner checks, excluding only the
   exact authoritative old target RID where applicable
7. close and reduce the complete ordinary candidate domain through §21.16.1
```

Lock waits/conflicts may force the implementation to break otherwise-vectorized
work around individual targets. Dynamic conflict/retry outcomes remain
Chapter-11/15-owned and do not enter ordinary-candidate ranking.

Correctness of the write protocol wins over artificial vectorization of lock acquisition.

For each target, the UNIQUE check receives only that target's exact revalidated old RID as `excluded_old_rid` when retaining a key. It does not exclude another target, another version with the same TxnId, or another current-command replacement. Immediate collision/key-swap behavior is §11.10.6.

After successful closure, the mutation/write-publication phase consumes only
the retained mutation-ready candidates: it installs one complete new version,
the old version's `xmax/cmax`, and the required new physical index entries
through Chapter 15. No new ordinary row-semantic candidate may first be
discovered in this phase; dynamic failures may still occur and follow §39.1.

In the defined architecture, the mutation/write-publication phase of `PhysicalInsert`,
`PhysicalUpdate`, and `PhysicalDelete` is single-worker. This restriction does
not independently require their input scans, target materialization, or
expression evaluation to be single-worker.

## 31.8 DELETE execution

After target-spool finalization, `PhysicalDelete` completes the following
closure before persistent mutation:

```text
for every distinct final target:
    acquire/register exact TUPLE_WRITE semantics
    re-fetch/revalidate the authoritative target and old-row image after grant
    evaluate and retain demanded RETURNING old-row values/errors
derive/acquire the complete affected UNIQUE_KEY set
after every wait, re-fetch/revalidate target authority; if it changed, discard
    retained old-row/RETURNING state and take the existing retry/conflict
    outcome
close and reduce every ordinary candidate through §21.16.1
```

DELETE constructs no unused replacement/new-row state. After successful
closure its single-worker mutation/write-publication phase installs each
authoritative target's `xmax/cmax` through Chapter 15. Dynamic conflicts and
resource/storage failures retain their existing owner.

Secondary-index entries are not physically erased here.

Vacuum remains the exact index-garbage cleanup owner.

The mutation/write phase is single-worker.

## 31.9 RETURNING spool

DML `RETURNING` is retained first as statement-owned output. Small output may
use an in-memory `RowCollection`; large output uses an exact spill-capable
result spool. Every semantically demanded expression is evaluated during
§15.1.3 ordinary DML candidate closure before the statement's first persistent
user-DML write. The exact INSERT/UPDATE final-new-row or DELETE authoritative-
old-row result is retained. A winning ordinary RETURNING candidate therefore
fails before mutation; a later target's expression cannot follow an earlier
target mutation.

This chapter defines one atomic semantic transition:

```text
R = successful statement-result publication
```

`R` succeeds only when:

1. DML execution, every required mutation, and RETURNING construction and
   finalization have completed successfully, and the successful statement-
   result envelope is authoritatively published; and
2. the finalized result spool/cursor, when present, transfers from statement/
   attempt ownership to the result/request owner.

The envelope includes command completion and the authoritative affected-row
count plus the complete logical RETURNING bag when present. DML without
RETURNING still crosses `R`; it needs no fake row cursor. The transition is
atomic semantically, without requiring one C++ atomic primitive or object.
Input exhaustion, ordinary-candidate closure, the first persistent write,
internal operator `Finalize` or readiness, spool construction alone, cursor
allocation, first `Next()`, first delivered row, and cursor exhaustion are not
`R`.

The three relevant boundaries remain distinct:

```text
W = §39.1.2 first persistent transaction-owned statement write
C = completion of the existing autocommit C4-C5 result-withholding requirement
R = successful statement-result publication and result ownership transfer

explicit:  ordinary closure -> [W if rows are written] -> result preparation -> execution success -> R -> delivery
autocommit: ordinary closure -> [W if rows are written] -> result preparation -> C0..C5 -> R -> delivery
```

`C` is not a new COMMIT stage. Zero-row DML need not cross `W`, while successful
publication still crosses `R` and an autocommit statement still satisfies its
canonical COMMIT prerequisites; absence of `W` alone does not classify the
transaction as read-only or authorize skipping those prerequisites.
No-RETURNING and zero-row DML prepare command completion and the exact affected-
count metadata without fabricating a row or row cursor.

Before an autocommit DML command is admitted to §15.5 C0, every fallible
operation required to make its eventual successful `R` transition possible
has completed. As applicable, the complete but unpublished prepared state
contains:

```text
final DML semantic execution and affected count
complete RETURNING row construction, spool append/spill writes, and finalization
publication-readiness schema, descriptor, framing, and extent validation
command/count metadata and successful-envelope representation
required result owner, cursor, spill handle, resource registration, and
    publication destination/slot preparation
value backing and lifetime preparation
memory accounting plus every required physical allocation
one valid cleanup owner that survives C5
```

An accounting or budget grant is not physical allocation under §24.5. If `R`
requires allocated storage, the allocation has already succeeded before C0;
an implementation cannot reserve capacity at C1 and perform the first required
allocation after COMMIT. Allocation or I/O needed only for a subsequent delivery
chunk or subsequent result-spool read remains post-`R` delivery work and need not be
performed before COMMIT.

Before `R`, RETURNING storage and the provisional affected count remain
unpublished. A result/request-lifetime owner, cursor, handle, publication slot,
or destination may be prepared before `R`, but preparation grants no successful-
result authority. Failure during expression evaluation, mutation, result-row
materialization, spool append or required spill write, spool finalization,
schema/framing/extent validation, physical allocation, registration, or
ownership-transfer preparation is statement execution failure when it occurs
before explicit-transaction `R` or before autocommit C0. Section 39.1 applies
using the real `W` state. The unpublished spool and count are discarded, no
RETURNING prefix escapes, and cursor allocation alone changes nothing. Once
implicit COMMIT is admitted, failures within C0–C5 instead retain §39.1.5's
existing COMMIT owner.

The prepared autocommit state remains valid and continuously accounted through
C0–C5. C5 may release transaction-owned locks, gates, snapshots, status
dependencies, and transaction arenas under their existing owners, but it does
not release or invalidate the prepared envelope, result schema, affected count,
retained value backing, cursor, spill files/handles, reservations, accounting
context, or cleanup responsibility needed through `R` and delivery. Such state
cannot depend exclusively on an arena or owner destroyed by C5, and preserving
it does not retain transaction locks.

After successful C4–C5, autocommit `R` is a non-failing in-process semantic
publication and ownership transition using only that prepared state. No new
required allocation, budget grant, validation, result construction, spill I/O,
handle duplication, resource registration, external callback, or fallible
cursor construction remains between `C` and `R`. Attempting one is an internal
protocol violation, not a supported ordinary postcommit failure path.

At `R`, statement success and affected count become final and authoritative,
the complete logical RETURNING bag is established, and any spool becomes
result/request-cursor-owned. The spool remains query/request-temporary,
memory-accounted, spill-managed, nonpersistent, non-WAL, not crash-recovered,
and unusable by another attempt. After its owning result is destroyed it is no
longer a source of SQL rows. Ownership transfer preserves continuous accounting
and every Chapter-23 value lifetime without dangling producer buffers or
requiring a particular class, allocator, reference count, file handle, or
duplicate materialization.

After `R`, spool reads, delivery-chunk materialization, cursor advancement,
cursor cancellation, and transport are result-delivery work and do not re-enter
DML execution. An ordinary result-spool `SpillIOError` or malformed temporary-
spill failure, or controlled allocation/representability failure while
materializing a delivery chunk, terminates the cursor with a controlled
delivery error, not successful `FINISHED`; it does not retroactively fail the
DML statement, invalidate the affected count, retract an already returned
prefix, authorize DML retry, or independently set `MUST_ABORT`. The same public
error category may be retained; ownership stage determines the transaction
consequence.
Persistent-database corruption, invariant failure, session loss, or another
independently fatal condition keeps its existing owner and is not downgraded by
`R`.

For an **explicit** transaction, `R` may occur while the transaction remains
`ACTIVE`. A post-`R` ordinary cursor/read/transport failure leaves it `ACTIVE`
when the session remains usable; later statements and COMMIT or ROLLBACK remain
available under ordinary command sequencing. A concrete API may require the
failed cursor to be closed first, but that is not automatic transaction abort.
Actual connection/session loss invokes §39.1.7 and may abort the still-active
transaction independently; the completed DML statement does not become failed.
A later explicit ROLLBACK changes the transaction outcome but cannot erase the
historical fact that the successful statement result or returned prefix was
observed.

For **autocommit**, `R` cannot precede `C`. A failure before C4–C5 exposes no
successful count or RETURNING prefix: result-preparation failure occurs before
C0 and COMMIT failure retains §§15.5 and 39.1.5 ownership. After the
publication-authorizing COMMIT append, cancellation cannot redirect COMMIT to
ABORT. If cancellation is pending after successful `C` while the session remains
usable, the prepared `R` transition completes and cancellation applies only to
remaining delivery. After `C` and `R`, the transaction remains `COMMITTED`
despite spool-read, cancellation, transport, or session-delivery failure. No
rollback or same-statement retry is authorized.

A process crash or actual session loss after durable COMMIT but before `R` may
prevent result publication or observation. Recovery still establishes
`COMMITTED`; no successful result envelope or affected-count authority need
have been published, missing output does not prove nonexecution, and the request
must not be blindly replayed. No durable response log, result-spool recovery, or
reconnect/resume protocol exists. Unpublished temporary result state is cleaned
when a known-safe owner can do so. An invariant or noncontinuable defect that
unexpectedly prevents the prepared C-to-`R` transition retains its stronger
owner, never changes COMMITTED to ABORTED, never fabricates `R`, and never
authorizes automatic replay.

The affected count's authority and its delivery are different facts. At `R`
the count is authoritative even if a later cursor or transport error prevents
the client from receiving it. It is result-envelope metadata, not a relational
column or `LogicalSlotId`.

Voluntary abandonment of a post-`R` cursor may discard unread rows and cleans
its result-owned memory and spill resources. It does not revoke statement
success, count authority, or transaction state, and the client need not drain
every row to preserve those facts. Cancellation before explicit-transaction
`R`, or before autocommit admission at C0, remains statement-execution
cancellation under §39.1 and the real `W` state. Cancellation during implicit
COMMIT follows §§15.5 and 39.1.5; after its publication-authorizing append it
cannot cause ABORT, and after successful C it cannot prevent the prepared `R`
transition in a usable session. Cancellation after `R` abandons result delivery
unless an independently fatal session condition also applies.

## 31.10 Query result interface

Execution exposes results through a result sink/cursor abstraction.

An internal borrowed producer DataChunk is never exposed beyond its owner lifetime.

The baseline synchronous cursor contract distinguishes:

```text
Next()
    -> client-visible result chunk
    -> FINISHED                  // successful exhaustion only
    -> terminal result error
```

No particular C++ return type, exception/status representation, coroutine, or
wire protocol is required. A failed cursor never reports successful
`FINISHED`, and after terminal error it produces no further successful rows.
Internal operator/pipeline `FINISHED` is not client `FINISHED`.

The cursor/result layer owns or safely retains every value in the returned chunk.

For a simple synchronous client, a returned chunk remains valid until the next cursor `Next()` call or cursor destruction, whichever comes first; callers that need a longer lifetime copy/materialize it.

A subsequent read or delivery failure does not retrospectively invalidate the
previous chunk during that guaranteed lifetime. Cursor destruction, voluntary
abandonment, terminal delivery error, or successful exhaustion releases result-
owned reservations, buffers, and spill objects under Chapters 23–24 without
affecting transaction-owned locks or reopening the statement.

For DML, affected count and command completion are metadata in the successful
§31.9 result envelope. After `R`, failure to deliver some or all metadata or
RETURNING rows is delivery uncertainty, not revocation of the authoritative
statement result. The cursor does not claim that the complete RETURNING bag was
delivered unless it reaches successful `FINISHED`.

A result chunk already returned by a completed cursor operation is not
retroactively retracted when a later cursor operation establishes an ordinary
non-DML expression error under §25.1.1. The returned prefix does not make the
failed query a successful complete query. This boundary requires neither
whole-query buffering nor a different returned-value lifetime or cursor
protocol.

This lifetime is an API/runtime contract, not a persistent format.

A CLI may format rows immediately while consuming chunks.

## 31.11 Physical DDL execution

DDL physical operators are control/management operators rather than vector hot paths.

Examples:

```text
PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
```

They invoke the Chapter-21 catalog/storage protocols under SchemaLock/TableWriterGate and ordinary transaction rules.

CREATE INDEX runs the offline private build pipeline and does not publish committed catalog metadata before the owning transaction's terminal publication boundary.

DDL execution remains single-coordinator in the defined architecture.

## 31.12 Physical VACUUM

PhysicalVacuum invokes the Chapter-14 VacuumManager/reclamation protocol.

It is a maintenance operator, not a requirement to express vacuum's internal page/index work as ordinary relational Filter/Project/Hash operators.

PhysicalVacuum produces no relational result-row bag; successful execution is
reported through ordinary command completion. Internal tracing, metrics,
profiling, or debug instrumentation do not define SQL result rows.

One PhysicalVacuum pass is not implicitly parallelized internally by the
general query scheduler. Chapter 14 is authoritative for maintenance
concurrency: it serializes same-table passes without serializing
different-table passes and permits coexistence with ordinary DML and ANALYZE
under its ownership rules.

### 31.12.1 Physical ANALYZE

`PhysicalAnalyze` executes the Chapter-34 statistics collection/publication protocol for one already-resolved target table.

It uses the ordinary `QueryExecutionContext` and therefore participates in:

```text
transaction/snapshot visibility
ReadEpochGuard where index-derived RIDs are retained
QueryMemoryManager accounting
SpillManager only if a bounded statistics helper explicitly needs it
query cancellation
profiling
```

In the defined architecture, one coordinator owns each `PhysicalAnalyze` statement and its
transactional statistics publication. This control-operator ownership does not
prohibit vectorized or batched collection work permitted by Chapter 34 and
introduces no parallel ANALYZE capability.

`LogicalAnalyze` lowers directly to `PhysicalAnalyze`; it does not enter join-order/access-path enumeration merely to rediscover that ANALYZE performs its required full visible heap scan.

Collection is conceptually:

```text
resolved TableDescriptor / SchemaVer
    ↓
stable visible heap scan
    ↓
bounded per-column HLL / MCV / reservoir state
    ↓
index physical-statistics scans where required
    ↓
finalize complete StatsDescriptor
    ↓
encode + write one StatsVersion of sys_statistics rows
    ↓
statement success
    ↓
transaction-local descriptor available to own later statements
    ↓
terminal COMMITTED C4 -> generation globally committed
    ↓
C5 -> install or safely invalidate/bypass the global descriptor/cache entry
```

ANALYZE never mutates an existing published `StatsDescriptor` in place.

A cancellation/error before statement success discards the in-memory candidate and leaves the previous committed descriptor authoritative.

If no transaction-owned statistics row published, this is a statement-only failure for an explicit transaction unless independently fatal. If one or more rows for the new StatsVersion published, the transaction automatically aborts under §39.1 even though completeness filtering keeps that version globally unusable.

An abort after successful ANALYZE but before transaction commit leaves the new catalog rows MVCC-invisible to other transactions and suppresses global cache publication.

ANALYZE does not acquire schema-changing DDL exclusivity merely to block ordinary DML; its SQL-visible row set is stabilized by MVCC.

## 31.13 DML/result invariants

1. UPDATE/DELETE finalize a target spool before mutating any target.
2. Each physical target RID is represented at most once per statement attempt.
3. Target materialization is the primary Halloween-protection boundary.
4. Large target/RETURNING spools are query-memory accounted and spill-capable.
5. Every target is revalidated after logical write-lock acquisition.
6. READ COMMITTED whole-statement retry is permitted only before the first persistent statement write.
7. INSERT/UPDATE/DELETE consume resolved typed inputs and do not redo binding.
8. DML write phases remain single-worker in the execution baseline.
9. DELETE/UPDATE do not physically clean old secondary-index entries.
10. RETURNING does not expose partial output from a statement attempt that later restarts/fails.
11. Client-visible result chunks never depend on an expired internal borrowed chunk.
12. DDL/VACUUM/ANALYZE control operators preserve their owning catalog/storage/statistics transaction protocols.
13. ANALYZE never globally publishes an uncommitted or partial StatsDescriptor.
14. A DML statement-execution failure established before explicit `R` or
    autocommit C0 automatically aborts after a published transaction-owned
    write; a pre-write recoverable failure may leave an explicit transaction
    active, while admitted autocommit follows the COMMIT owner.
15. Autocommit DML does not expose RETURNING rows before implicit COMMIT C4–C5 complete; later transport failure cannot undo that commit.
16. UNIQUE enforcement uses current-state ownership rather than ordinary
    snapshot visibility, detects earlier-command published owners and exact
    current-statement pending owners, and self-excludes only the exact current
    UPDATE target/replacement RID.
17. Every semantically demanded ordinary DML candidate closes and is
    canonically reduced before the first persistent user-DML statement write;
    no persistent mutation may precede complete closure.
18. INSERT SELECT stages its complete demanded candidate domain before target
    publication; UPDATE/DELETE establish authoritative post-wait row images
    before accepting candidate state.
19. Exact attempt-local pending owners detect same-statement unique collisions
    before publication without introducing deferred uniqueness or DML row
    order.
20. Closure staging is query-memory accounted and spill-capable; retry rebuilds
    it fresh and never reuses abandoned-attempt rows, errors, or spill state.
21. `R` atomically publishes successful DML result authority and transfers any
    finalized RETURNING spool to the result/request owner; readiness, cursor
    allocation, first delivery, and exhaustion are not `R`.
22. Result-spool/preparation failure before explicit `R` or autocommit C0 is
    statement execution failure governed by the real first-write state;
    autocommit C0–C5 failure is COMMIT-owned, and after `R` ordinary delivery
    failure cannot retroactively fail DML, revoke its count, or authorize retry.
23. A usable explicit transaction remains `ACTIVE` after ordinary post-`R`
    result failure; autocommit is already `COMMITTED` before `R`.
24. Cursor terminal error is not successful `FINISHED`, and no already returned
    row is retracted or invalidated during its guaranteed lifetime.
25. Result ownership transfer preserves memory accounting, value lifetimes, and
    temporary-spill cleanup; cursor abandonment alone changes no statement or
    transaction outcome.
26. Autocommit DML completes every fallible result-publication prerequisite,
    including required physical allocation rather than only reservation, before
    C0 while keeping the prepared envelope unpublished.
27. C1 validates already-prepared result readiness, C5 preserves its independent
    backing/accounting/cleanup owner, and successful C-to-`R` publication needs
    no ordinary fallible required operation.
28. Process crash or actual session loss after durable COMMIT but before `R`
    leaves the transaction COMMITTED without inventing result recovery, count
    reconstruction, automatic replay, or a false successful publication.
