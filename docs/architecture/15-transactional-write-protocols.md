# 15. Transactional Write Protocols

## 15.1 Scope and ownership

This chapter integrates the component contracts from heap/tuple storage, FSM, BufferPool, B+ tree, transactions/snapshots, LockManager, WAL/CommitCoordinator, and vacuum.

It defines cross-subsystem ordering for user DML and transaction completion.

The owning subsystem chapters remain authoritative for their local byte formats, visibility algorithms, lock semantics, WAL codecs, and recovery rules.

These responsibilities MUST NOT collapse into one monolithic “transaction engine” abstraction that hides ownership/lifetime boundaries.

### 15.1.1 DDL writer-gate integration

Before the first persistent write to target table `T`, a DML transaction acquires the shared `TableWriterGate(T)` defined by §21.2.1 and holds it until terminal transaction publication.

The gate is acquired before TUPLE_WRITE/UNIQUE_KEY locks and before any heap/B+ latch that could be retained across a wait.

Ordinary DML transactions may hold shared gates for multiple target tables; shared gates do not conflict with one another.

Schema-changing DDL follows the global ordering:

```text
SchemaLock
    -> target TableWriterGate exclusive, when required
    -> private construction/build, with all short latches released before a
       later transaction-gate wait
    -> MANIFEST_CHANGE on affected table/index identities, when required
    -> logical tuple/unique locks, when required
    -> live-object/catalog publication latches
```

The architecture does not perform an exclusive TableWriterGate upgrade while the same transaction already holds that table's shared gate. A schema-changing DDL statement that would require such an upgrade after prior persistent DML in the same explicit transaction is rejected rather than risking an upgrade deadlock.

Section 11.13 is authoritative for cross-statement retained ownership. In
particular, a shared writer gate does not globally prohibit later unrelated DDL:
its SchemaLock wait is legal and enters the same graph as the writer-gate wait.
The same-table upgrade remains the explicit proactive rejection.

Publishing the current normal TxnId in persistent MVCC correctness metadata is
authorized only while the transaction retains its §9.4 write-status dependency. This
transaction-lifetime precondition covers INSERT's new `xmin`, UPDATE's new `xmin` and old
version `xmax`, DELETE's `xmax`, and transaction-owned catalog/system MVCC writes. It is
established by write-capable transaction registration rather than by a lazy first-write
check, and no such publication is legal after C5/A3 releases it.

### 15.1.2 Affected-row result ownership

The SQL affected-row count is a non-negative, statement-scoped runtime result. It is the
logical cardinality produced by the final successful execution attempt of one SQL
statement, under the operation-specific rules in §§15.2–15.4. Each logical row acted upon
by that final attempt contributes at most once. The count is not candidate-discovery
progress and is not derived from tuple versions, index entries, WAL records, page
mutations, or internal retry count. This separation ensures that physical layout,
index count, WAL shape, and otherwise legal execution optimizations cannot change
SQL-visible behavior.

An implementation MAY maintain an attempt-local provisional count, but it has no external
authority. Every abandoned or internally retried attempt discards its entire provisional
count, and replacement attempt accounting starts at zero. Under READ COMMITTED, the final
attempt uses the fresh snapshot required by §§9.9 and 15.7.1 while retaining the same
CommandId. REPEATABLE READ snapshot and conflict behavior remain governed by §§9.10, 11.6,
and 39.1; no count reconciliation authorizes snapshot refresh or retry.

For UPDATE and DELETE, the finalized target spool in §§31.1–31.2 supplies one
occurrence of each logical target RID. A target contributes only if it survives the
operation's required post-wait revalidation and the final successful attempt logically
acts on it. An initially discovered target that is skipped as stale/nonqualifying
contributes zero; if the existing
READ COMMITTED rules instead restart the attempt, only the replacement attempt's result
counts. Duplicate candidate production cannot double count one target. Separate SQL
statements may each count the same logical row once; there is no transaction-wide
deduplication.

A statement that fails publishes its canonical error and no successful affected-row
result. This applies to `FAILED_TRANSACTION_REMAINS_ACTIVE`,
`FAILED_TRANSACTION_MUST_ABORT`, deadlock, serialization, constraint/evaluation/type
failure, and database-noncontinuable outcomes without changing §39.1's classification.
Published physical effects from a failed multirow statement after a dynamic
post-write failure remain governed by semantic ABORT, MVCC, and the
no-user-DML-undo rules; they are not partial SQL success and their provisional
count is not exposed. Internal diagnostic counts such as candidates visited,
physical mutations, retries, or index entries written are not the SQL affected-row count.

Affected-row count and any `RETURNING` rows belong to the same successful statement-result
envelope in §§15.7.3 and 31.9:

- the count becomes authoritative at §31.9's atomic successful statement-result
  publication boundary `R`, whether or not later result delivery successfully conveys it
  to the client;
- in an explicit transaction, `R` may publish the count while the transaction remains
  `ACTIVE`; a later COMMIT or ROLLBACK is separate and does not retroactively rewrite that
  statement result, which never implied transaction durability;
- in autocommit, the count remains request-owned and externally unexposed through the
  implicit §15.5 COMMIT's C4–C5 completion. Its exact value and successful-envelope
  representation are fully prepared but remain unpublished before admission to that
  COMMIT. A commit failure before successful response publication exposes the canonical
  commit/error outcome, not a successful affected-row count; after C4–C5 and `R`, delivery
  failure cannot revoke the authoritative count or the COMMITTED transaction even when the
  client did not receive that metadata.

The count is not persistent database state. It is not WAL-logged, checkpointed, stored in
`database.control`, transaction status, or catalog metadata, and crash recovery does not
reconstruct or replay it. A client disconnect does not create result-recovery state.

### 15.1.3 Ordinary DML candidate closure before publication

Every INSERT, UPDATE, and DELETE final attempt completes **ordinary DML
candidate closure** before its first persistent user-DML statement write. The
closed domain contains every semantically demanded ordinary row-semantic error
candidate defined by §21.16.1. Execution preserves each candidate's canonical
diagnostic provenance and reduces the complete domain through Chapter 21's
ordering. If a candidate wins, the statement fails before any of its heap,
index, `xmax/cmax`, or other transaction-owned user-DML effect reaches §12.12
publication.

The conceptual lifecycle is:

```text
complete demanded input or finalized target domain
    -> acquire/revalidate authority needed for exact candidate row images
    -> evaluate and retain all ordinary-candidate-producing values
    -> prepare authoritative immediate uniqueness checks
    -> close and reduce the ordinary candidate domain
    -> if no candidate wins, publish persistent mutations
    -> complete writes, affected count, and statement-owned RETURNING spool
```

This requirement changes when ordinary candidates must be established, not
their §21.16.1 ranking or Chapter 20 demand. It creates no RID, source-row,
batch, spool, hash, lock-acquisition, or mutation order. An uncorrelated side
plan keeps its existing evaluation cardinality; if its result is demanded, any
ordinary failure it produces participates in closure.

Closure excludes deadlock, serialization or revalidation conflict,
`OutOfMemory`, `SpillIOError`, runtime representability/resource failure,
cancellation, persistent storage/I/O failure, corruption, and internal
invariant failure. Those dynamic failures retain their actual runtime owner
and §39.1 consequence. Closure success is not a persistent write and does not
make later execution failure-free.

Candidate state is exact, query-temporary, attempt-local, memory-accounted,
and spill-capable through Chapter 24. It may extend the target or RETURNING
spools or use an implementation-equivalent retained representation. It
preserves required occurrence identity, target RID and authoritative old-row
image where applicable, complete candidate new rows, NULLs, VARCHAR bytes,
hidden slots/provenance, unique-key material or an exact reconstructable
equivalent, demanded RETURNING values, and error-reduction metadata. It is not
WAL-logged, persisted, or reused by a retry. Container, partitioning, vector
width, and batching remain implementation choices.

A failed or abandoned attempt releases its candidate staging, pending unique-
owner state, staged RETURNING rows, ordinary candidates, provisional count,
closure cursors, and attempt-local spill objects through the Chapter 24
cleanup owners. Query cleanup does not release transaction-owned logical
locks or writer-gate ownership contrary to their Chapters 11 and 15 owners.
Completion of closure is not itself a persistent write: a dynamic failure
after closure but before the first persistent user-DML write retains the
existing pre-write consequence.

Closure is intentionally blocking with respect to persistent user-DML writes.
It may increase retained memory, temporary spill I/O, lock footprint, and
latency to the first write; these costs are the consequence of deterministic
ordinary-error and transaction-state semantics without physical statement
undo. Evaluation, encoding, spill, and later publication may still be
vectorized or batched.

## 15.2 INSERT

Before persistent publication, an INSERT final attempt:

```text
1. determine the statement CommandId
2. consume the complete semantically demanded logical input and retain one
   mutation-ready candidate per logical occurrence
3. evaluate conversions and complete target rows, enforce runtime NOT NULL,
   derive every affected unique key, and evaluate demanded RETURNING values
4. acquire the complete finite UNIQUE_KEY lock set in §11.9 order, perform
   authoritative §11.10 current-state checks, and establish exact
   same-statement pending-owner conflicts
5. close and reduce every ordinary candidate through §21.16.1
```

No target heap/index row may publish while a later demanded input occurrence
can still establish an ordinary candidate. In particular, INSERT SELECT may
pipeline/vectorize source computation into spill-capable temporary staging,
but it cannot stream persistent target publication before closure. Chapter 20
still owns demand, occurrence multiplicity, source semantics, and the
statement snapshot; source encounter order is not semantic and newly inserted
rows cannot feed the prepublication source scan.

Only after closure succeeds, persistent publication processes the staged
candidates, possibly in batches and a nonsemantic physical order:

```text
1. encode the new tuple version:
       xmin = current TxnId
       xmax = INVALID_TXN_ID
       cmin = current CommandId
       cmax = 0
       prev = no previous version under §5.7.4

2. choose/validate a heap page using the advisory FSM
3. construct/append the required heap PAGE_INIT/PAGE_IMAGE/PAGE_DELTA redo
4. publish the heap tuple version and corresponding page_lsn under the heap
   write guard, then release that guard latch-before-pin under §§5.14, 7.7,
   7.10.1, and 12.12

5. for every index:
       perform the B+ MTR inserting
       (encoded_user_key, RID)
       using that MTR's own B+ page guards and §8.19 latch ordering
       release those guards under §§7.7 and 8.25 after successful MTR
       publication

6. retain UNIQUE_KEY locks through terminal COMMITTED/ABORTED publication
```

Heap redo describing the referenced RID is established before any B+ MTR that references that RID.

The heap write guard is not retained into an index MTR. Releasing either heap or B+
physical protection does not release transaction-duration TableWriterGate or UNIQUE_KEY
ownership; the same physical/logical distinction applies to TUPLE_WRITE on DML paths that
use it. Section 11.3 remains authoritative for the prohibition on transaction-lock waits
under short-lived physical protection.

For the final successful statement attempt, each logical input row successfully inserted
contributes exactly one affected row. A successful single-row INSERT therefore reports one,
and a successful multirow INSERT of `N` logical input rows reports `N`; heap versions, index
entries, WAL records, and page mutations do not multiply that logical count. Section 15.1.2
owns retry, failure, and external-publication consequences.

If the transaction aborts:

```text
new xmin -> ABORTED
tuple version -> logically invisible
physical index entries may remain
vacuum removes the garbage later
```

## 15.3 UPDATE

Before the first persistent UPDATE mutation, the final attempt performs this
statement-wide authoritative closure over the finalized, deduplicated target
domain:

```text
1. for every target, retain its discovery read epoch while releasing short
   physical latches, register TUPLE_WRITE(TableId, old RID), release the epoch
   before a blocking wait, and after grant re-fetch/revalidate the old version
2. apply the isolation-specific conflict and qualification rules; only the
   resulting authoritative target/old-row image enters candidate staging
3. evaluate all SET right-hand sides simultaneously against that complete
   immutable old-row image, apply assignment coercions, construct the complete
   candidate new row, enforce descriptor-wide NOT NULL, derive old/new unique
   keys, and evaluate demanded RETURNING values
4. acquire the complete affected fully non-NULL UNIQUE_KEY set in §11.9 order
5. after every unique-lock wait, re-fetch/revalidate each affected target;
   if any authoritative image changed, discard the stale assignment, row, key,
   RETURNING, and pending-owner state and take the existing retry/conflict
   outcome; an authorized fresh attempt derives its complete lock set again
6. perform §11.10 current-state and exact pending-owner checks, passing only
   the exact authoritative old RID as excluded_old_rid where permitted
7. close and reduce every ordinary candidate through §21.16.1
```

No target mutation publishes until this closure succeeds. TUPLE_WRITE and
UNIQUE_KEY claims are transaction-owned for their existing Chapter-11
lifetime; retry/cleanup does not release them merely because candidate staging
is attempt-local. No physical latch or read epoch is retained across a blocking
logical-lock wait. Establishing authoritative images for the complete target
domain before publication may therefore increase the number and duration of
retained TUPLE_WRITE claims; the deadlock/conflict owners remain Chapter 11's.

Persistent publication then uses only the successfully closed,
mutation-ready candidates:

```text
1. create new tuple version:
       xmin = current TxnId
       xmax = INVALID_TXN_ID
       cmin = current CommandId
       cmax = 0
       prev = old RID

2. WAL-log/install the new tuple version

3. WAL-log/install old-version header:
       xmax = current TxnId
       cmax = current CommandId

4. install new physical B+ entries through MTRs
5. retain old physical B+ entries
6. hold logical tuple/unique locks through terminal transaction publication
```

If the transaction aborts:

```text
old xmax -> aborted and ineffective
new xmin -> aborted and invisible
new index entries -> vacuumable garbage
```

No physical rollback is required.

For affected-row reporting, UPDATE counts each distinct §31.2 target that survives the
required lock waits, revalidation, current-owner handling, and predicate qualification and
is logically updated by the final successful statement attempt. The old-version mutation,
replacement version, index work, WAL records, and page mutations together contribute one,
not one each. A qualifying target contributes one even when every assigned value is equal
to its existing value. Any otherwise legal physical no-op optimization MUST preserve that
count; this rule does not independently authorize such an optimization.

UNIQUE remains immediate for every staged target. A same-key UPDATE excludes
only its exact authoritative old RID; every other target's old owner remains
current, and another staged prospective owner is represented by §11.10.3's
pending-owner relation. A changed-key UPDATE checks the new key against all
other current and staged prospective owners. Multirow key collisions and swaps
therefore follow §11.10.6 rather than a deferred final-state permutation rule.

## 15.4 DELETE

Before the first persistent DELETE mutation, the final attempt performs this
statement-wide closure over the finalized, deduplicated target domain:

```text
1. acquire/register every required exact TUPLE_WRITE claim through the
   §11.3 read-epoch handoff, then re-fetch/revalidate authoritative old rows
   after grants and apply existing conflict/qualification rules
2. evaluate and retain every demanded DELETE RETURNING row and ordinary
   candidate; no replacement/new-row state is constructed
3. derive the complete affected fully non-NULL unique-key set, acquire it in
   §11.9 order, and re-fetch/revalidate after waits; any changed authoritative
   old image invalidates retained old-row/RETURNING state and takes the
   existing retry/conflict outcome
4. close and reduce every ordinary candidate through §21.16.1
```

Only after successful closure does persistent publication process each
authoritative target:

```text
1. WAL-log/install:
       xmax = current TxnId
       cmax = current CommandId
2. retain existing physical index entries
3. hold logical locks through terminal transaction publication
```

Commit makes the version dead to sufficiently new snapshots.

Abort makes the `xmax` ineffective.

The retained old-key lock ensures another transaction cannot treat this delete as freeing the key before COMMITTED publication. If DELETE aborts, a waiter rechecks after ABORTED publication and observes the old owner through §11.10.

Vacuum removes tuple/index garbage only after Chapter 14's global reclamation rules are satisfied.

For affected-row reporting, DELETE counts each distinct §31.2 target that survives the
required lock waits, revalidation, current-owner handling, and predicate qualification and
receives the final successful statement attempt's logical delete. Candidate discovery,
`xmax/cmax` writes, retained or removed index entries, WAL records, and page mutations do
not define or multiply the count. A successful UPDATE or DELETE with no such target reports
zero.

## 15.5 COMMIT

For a transaction with persistent writes:

A transaction that created DDL physical resources MUST first satisfy the §21.5 durable final-name prerequisite for every file referenced by its prospective committed catalog state.

```text
C0. accept COMMIT only from ACTIVE; no ordinary statement may still be running;
    for implicit autocommit DML, require the complete but unpublished §31.9
    statement-result envelope and every fallible resource needed to publish it at R
    to have been prepared successfully
C1. validate every commit prerequisite, including DDL final-name durability,
    and, for implicit autocommit DML, validate that its already-prepared result,
    accounting, cleanup owner, and backing remain valid through C5 and R;
    reserve every fallible resource needed for COMMIT's own runtime publication;
    a persistent transaction already owns its §4.3.2.4 terminal-WAL credit
C2. state ACTIVE -> COMMITTING; while still nonterminal in the registry,
    execute the §12.10.5 status-page protocol under its write latch:
        append a system PAGE_IMAGE first if the dirty/FPI-epoch rule requires it
        validly append TXN_COMMIT(txn_id, prev_txn_lsn) at commit_lsn
        install COMMITTED and status-page page_lsn = commit_lsn
        publish the status frame's ordinary dirty/DPT state
C3. submit commit_lsn to CommitCoordinator and do not leave COMMITTING until
    durable_lsn >= commit_lsn
C4. execute the §9.14 runtime terminal-publication linearization, including
    runtime outcome cache COMMITTED, state COMMITTED, and active removal
C5. put catalog/statistics caches in a coherent installed-or-invalidated state,
    release TUPLE_WRITE/UNIQUE_KEY locks and
    TableWriterGate/SchemaLock/STATS_PUBLISH/MANIFEST_CHANGE holdings,
    unregister snapshots, release any registered write-status dependency, and finish
    required transaction cleanup without releasing or invalidating independently
    retained prepared-result backing, accounting, spill handles, or cleanup
    responsibility needed through R and delivery
C6. only now return/send successful COMMIT acknowledgement
```

Commit does **not** force dirty heap/index pages.

The C2 status-page installation intentionally precedes C3 durability under §§12.10.5 and 13.13.2; the status page remains NO-FORCE and cannot itself reach disk before WAL-before-data. C3 is the irreversible durable-commit point: once `TXN_COMMIT` is durable, recovery necessarily establishes COMMITTED and no later runtime, cache, cleanup, or client failure may change that outcome to ABORTED.

After the publication-authorizing `TXN_COMMIT` append within C2, commit is uncancellable. A connection loss or cancellation request cannot redirect it to ABORT. Exact failure outcomes for every C0–C6 boundary are canonical in §39.1.5.

For implicit autocommit DML, C0's result prerequisite is stronger than an
accounting reservation. Every required result allocation, cursor or owner
construction, spill-handle preparation, validation, registration, and other
fallible operation needed to make §31.9 `R` possible has already succeeded.
Section 24.5's accounting grant does not prove physical allocation success, so
C1 cannot validate readiness from a grant alone and COMMIT does not finish
statement-result construction. Delivery-only reads, chunk materialization, and
transport remain outside this prerequisite.

For implicit autocommit DML, successful C4–C5 is followed by §31.9 `R` before
C6 sends the successful statement/COMMIT response. C6 performs delivery, not
result preparation or result-authority construction; its transport failure
retains §§31.9 and 39.1.5 ownership and cannot undo COMMITTED.

A read-only transaction elides C2–C3 because it has no persistent references or terminal WAL requirement. Its runtime terminal publication at C4 ends the transaction; C5 still precedes acknowledgement.

## 15.6 ABORT

For an abortable transaction in `ACTIVE` or `MUST_ABORT`:

```text
A0. state ACTIVE/MUST_ABORT -> ABORTING; reject ordinary statements/COMMIT
A1. if persistent WAL-visible state exists:
       consume only the owning §4.3.2.4 terminal-WAL credit for numeric capacity
       execute the §12.10.5 status-page protocol under the page write latch:
           append a system PAGE_IMAGE first if the dirty/FPI-epoch rule requires it
           append TXN_ABORT at abort_lsn
           install ABORTED and status-page page_lsn = abort_lsn
A2. execute the §9.14 runtime terminal-publication linearization, including
    runtime outcome cache ABORTED, state ABORTED, and active removal
A3. release logical locks and
    TableWriterGate/SchemaLock/STATS_PUBLISH/MANIFEST_CHANGE holdings, unregister
    snapshots, release any registered write-status dependency, transfer private/orphan
    cleanup ownership, and finish cleanup
A4. only then acknowledge explicit ROLLBACK or return the transaction-fatal
    statement's original error with transaction outcome ABORTED
```

Ordinary abort performs no write-set scan to restore old heap/index bytes.

Aborted physical versions and index entries remain vacuum input.

A transaction whose COMMIT is already durable is no longer eligible to transition to ABORTED.

Ordinary ABORT does not wait for `abort_lsn` durability before runtime ABORTED publication, but §12.17 prevents its dirty status page from reaching disk before WAL is durable through that page's current `page_lsn`. If the abort record is lost in a crash, loser resolution repeats the canonical ABORT protocol.

The architecture automatically drives A0–A4 after a statement enters `MUST_ABORT`, including inside an explicit transaction block; the client is not required to issue a later ROLLBACK merely to prevent partial effects from committing. `MUST_ABORT` remains observable while abort is pending, and an explicit ROLLBACK arriving then may only join/wait for that cleanup. COMMIT and ordinary statements remain forbidden.

An abort-path failure never restores commit eligibility. The exact retry, noncontinuable escalation, diagnostic precedence, and recovery behavior are §39.1.6.

## 15.7 READ COMMITTED retry boundary

The architecture does **not** provide statement-level physical undo, savepoints, or subtransaction outcome IDs.

Therefore one statement attempt tracks process-local:

```text
current_statement_has_published_write
```

Its exact transition is §39.1.2: it becomes true at the first transaction-owned WAL-backed database mutation publication, not at LSN reservation or provisional mutation.

### 15.7.1 Retry before the first persistent write

If a READ COMMITTED conflict requires fresh-snapshot restart while:

```text
current_statement_has_published_write == false
```

then the engine may:

```text
release attempt-local resources
unregister the old statement snapshot
discard the attempt-local provisional affected-row count
discard target/candidate/RETURNING spools, pending-owner state, ordinary error
    candidates, closure cursors, and attempt-local spill state
capture a fresh statement snapshot
rebuild the complete target/input and ordinary-candidate closure domain
```

No database state from the abandoned attempt needs rollback.

The restarted attempt begins provisional affected-row accounting at zero. It retains the
logical statement's CommandId under §9.6 and may retain transaction-duration locks under
Chapter 11, but neither retained resource carries an affected-row contribution forward.

### 15.7.2 Conflict after a persistent statement write

If a conflict requiring statement restart is discovered after:

```text
current_statement_has_published_write == true
```

the architecture MUST NOT restart that statement inside the same transaction.

Instead:

```text
state ACTIVE -> MUST_ABORT
automatically execute §15.6 ABORT
return a retryable serialization/write-conflict outcome to the transaction-owning layer
```

The architecture does not transparently rerun an aborted autocommit transaction. It returns the retryable
conflict after ABORT; the client/application may submit a new request, which receives a
**new TxnId**. The architecture does not define an explicit opt-in whole-request retry
policy, and no separate request may reuse this transaction identity.

An explicit user transaction is terminally aborted by this failure and the client/application decides whether to begin a new transaction and retry.

This is necessary because writes from the abandoned statement attempt would otherwise become visible if the same transaction later advanced `CommandId`.

### 15.7.3 External output

Even a pre-write-retryable statement MUST NOT expose irreversible external rows/side effects before its retry-safe point.

`RETURNING` output is buffered until the attempt can no longer internally restart.

Physical DML applies the stronger §31.9 publication rule: no partial RETURNING prefix is exposed from a statement that later fails, so external emission begins only after successful statement completion.

For DML, successful completion becomes externally authoritative only through
§31.9's atomic result-publication boundary `R`. Before `R`, result state remains
unpublished. An explicit transaction prepares it before `R`. Autocommit prepares
every fallible publication prerequisite before admission to implicit COMMIT,
retains that prepared state through C0–C5, and performs the non-failing semantic
`R` transition after successful C4–C5. A preparation failure before COMMIT
admission uses §39.1's actual first-write state; a failure within COMMIT retains
§39.1.5's COMMIT owner.
After `R`, cursor delivery does not re-enter DML execution and cannot authorize
same-`TxnId` statement retry or retroactively change the published statement
outcome. Connection/session loss retains its independent §39.1.7 owner.

Statement savepoints, subtransactions, and statement-local physical undo are outside the
architecture baseline; they do not relax this publication or retry boundary.

## 15.8 Cross-layer contract

Higher catalog/SQL/planning/execution layers consume this transactional storage contract.

They MUST NOT redefine:

- MVCC creator/deleter visibility,
- write-conflict outcome,
- unique-key serialization,
- WAL-before-data,
- commit durability,
- affected-row counting or publication,
- vacuum/RID reuse safety.

This chapter is the boundary between the persistent transactional storage core and upper semantic/execution layers.

## 15.9 End-to-end invariants

1. User data is never published committed before durable commit WAL.
2. COMMIT remains NO-FORCE for heap/index pages.
3. Runtime terminal publication linearizes before transaction logical locks are released.
4. Logical lock waits occur outside physical page/B+ latch waits.
5. Index entries never decide MVCC visibility by themselves.
6. UPDATE/DELETE revalidate the target after logical-lock acquisition.
7. READ COMMITTED may internally restart only while
   `current_statement_has_published_write == false`, before §39.1.2's first
   transaction-owned mutation publication at the §12.12 publication point.
8. A retry-requiring READ COMMITTED conflict after a persistent statement write aborts the transaction.
9. Any higher-level automatic rerun after such an abort uses a new transaction identity.
10. REPEATABLE READ aborts on a conflicting committed post-snapshot write.
11. User abort changes logical outcome without requiring physical heap/index rollback.
12. B+ structural MTRs may survive user abort.
13. Transaction-lifetime tuple/unique locks are released only after terminal outcome publication.
14. Vacuum performs delayed exact index garbage removal and RID reuse.
15. External output from a potentially retryable statement does not escape before its retry-safe boundary.
16. A statement-execution non-success established before implicit COMMIT
    admission that has published a transaction-owned persistent write cannot
    leave the transaction commit-eligible; once C0 is admitted, the COMMIT
    protocol and its uncertainty rules are authoritative.
17. `MUST_ABORT` is followed automatically by semantic ABORT; no partial user-DML statement effects can later commit.
18. CommandIds are consumed by failed as well as successful statements and are never reused.
19. COMMIT is uncancellable after its publication-authorizing record append and irreversible after durable commit.
20. Successful COMMIT acknowledgement follows runtime terminal publication and required coherent cleanup.
21. DML publishes a fully non-NULL UNIQUE/PRIMARY KEY owner only after §11.10 returns `NO_CONFLICT` under the canonical transaction-lifetime key lock.
22. The affected-row count belongs only to the final successful SQL statement attempt; abandoned retries contribute nothing.
23. INSERT counts successful logical input rows once; UPDATE and DELETE count distinct finalized, revalidated logical targets once.
24. A qualifying UPDATE target counts even when every assigned value is unchanged; physical work and optimization do not define the count.
25. A failed statement publishes no successful or partial affected-row count, regardless of already-published physical effects.
26. An explicit transaction may publish the count at statement success, while autocommit retains it through implicit COMMIT C4–C5 under §31.9's result envelope.
27. Affected-row count is runtime result metadata and is never WAL-logged, checkpointed, or reconstructed by recovery.
28. Every semantically demanded ordinary user-DML candidate is closed and
    canonically reduced before that statement's first persistent user-DML
    write; no physical visitation or batching choice can move its winner across
    the §39.1 failure boundary.
29. Closure and its spill files are exact attempt-local state, not persistent
    writes; dynamic failures still follow their actual §39.1 boundary side.
30. DML result publication is the atomic §31.9 `R` transition: before it,
    result state is execution-owned; after it, affected count and statement
    success are authoritative and result delivery cannot reopen execution.
31. Before implicit autocommit DML enters C0, its complete successful result
    envelope and every fallible resource required for `R` are prepared but
    unpublished; an accounting reservation alone is insufficient when physical
    allocation is required.
32. C5 transaction cleanup preserves the independently owned prepared result,
    accounting, value backing, spill handles, and cleanup responsibility needed
    through `R` and delivery without retaining transaction locks for that purpose.
33. After successful autocommit C4–C5, `R` uses only already-prepared state and
    introduces no ordinary required allocation, validation, spill I/O, handle
    duplication, registration, callback, or other fallible semantic work.
