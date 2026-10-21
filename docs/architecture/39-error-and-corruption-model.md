# 39. Error and Corruption Model

The system distinguishes at least:

- SQL/user errors,
- transaction conflicts,
- I/O errors,
- persistent corruption,
- internal invariant failures.

Persistent corruption and impossible storage states MUST NOT be silently accepted.

Errors that can arise from valid external input, I/O behavior, concurrent transactions, or corrupted persistent bytes require explicit runtime handling; debug assertions alone are insufficient for such cases.

Internal invariant failures SHOULD fail loudly in debug/test configurations.

## 39.1 Transaction, durability, and recovery errors

The transaction/storage subsystem distinguishes at least:

```text
SerializationFailure
DeadlockVictim
UniqueViolation
TransactionAborted
TransactionMustAbort
CommitOutcomeUncertain
ConnectionFailure
LockCancelled
WalIOError
StorageNoncontinuable
RecoveryError
CorruptionError
```

These are not collapsed into one generic internal-error category.

`WalIOError` includes locally recoverable reservation/append/durability failures whose exact lower-layer outcome is known. `StorageNoncontinuable` is the §12.12.4 database-owner gate when append, restoration, or required publication cannot be established; it forbids further ordinary storage activity and requires non-clean controlled stop/recovery. `CommitOutcomeUncertain` describes what a client may know, not a third durable transaction status.

The database-level lifecycle transition caused by `StorageNoncontinuable`, its
admission closure, exclusive-lock lifetime, and non-clean teardown are canonical
in §3.3.5. Lifecycle/open/close failures preserve the distinct categories in
§3.3.7 rather than collapsing busy, unsupported, corrupt, recovery, and shutdown
outcomes into a generic transaction error.

The SQL surface may map them to SQLSTATE-like codes without erasing the underlying distinction.

### 39.1.1 Runtime states and statement outcomes

The canonical runtime states are §9.4's:

```text
ACTIVE
MUST_ABORT
COMMITTING
COMMITTED
ABORTING
ABORTED
```

Only `ACTIVE` admits an ordinary statement or COMMIT. `MUST_ABORT`, `COMMITTING`, and `ABORTING` remain nonterminal and snapshot-active until §9.14 terminal publication. `MUST_ABORT` may perform only automatic/explicit ABORT cleanup; COMMIT is categorically illegal. No new two-bit `TXN_STATUS` code is introduced for any runtime-only state.

One statement/transaction-control request has exactly one of these command-layer outcomes:

| Outcome | Meaning |
|---|---|
| `SUCCESS` | the requested operation completed semantically on the server: an ordinary statement in an explicit transaction remains `ACTIVE`; an autocommit statement has completed its implicit COMMIT through C4–C5; an explicit COMMIT is `COMMITTED`; and an explicit ROLLBACK is `ABORTED` |
| `FAILED_TRANSACTION_REMAINS_ACTIVE` | the statement failed without a transaction-fatal/database-fatal condition and without a published write from this statement; cleanup completed and another statement or COMMIT is legal |
| `FAILED_TRANSACTION_MUST_ABORT` | this statement cannot be allowed to commit its TxnId; transition to `MUST_ABORT` and automatically execute §15.6 |
| `COMMIT_OUTCOME_UNCERTAIN` | no successful acknowledgement was delivered after commit became uncancellable; the connection cannot continue and canonical server/recovery evidence determines or already knows the semantic outcome |
| `DATABASE_NONCONTINUABLE` | storage/runtime invariants do not permit ordinary continuation; close affected connections and perform non-clean controlled stop/recovery |

The command outcome, transaction semantic state, database-continuation state, and response-delivery state are distinct dimensions. `SUCCESS` describes completed server-side command semantics; it does not claim that a success response reached the client. An explicit COMMIT whose C6 response is lost uses `COMMIT_OUTCOME_UNCERTAIN` even when the server already knows that the actual state is `COMMITTED`. An explicit ROLLBACK whose A4 response is lost has completed with server-side `SUCCESS` and actual state `ABORTED`, but the connection reports/chains `ConnectionFailure` and does not claim that the client received `SUCCESS`; `COMMIT_OUTCOME_UNCERTAIN` is not a generic rollback-transport outcome. Automatic ABORT retains the originating statement's `FAILED_TRANSACTION_MUST_ABORT` outcome rather than becoming a successful user-requested ROLLBACK. Database noncontinuability and structured primary/chained causes are reported independently where the tables below require them.

`MUST_ABORT` is transaction-fatal only: other transactions and the database remain usable while this transaction is automatically terminated. `DATABASE_NONCONTINUABLE` is a database/storage-owner gate: ordinary mutation/transaction continuation for that database stops because WAL, page, terminal-publication, or ownership coherence is not established. Ordinary user/constraint/resource errors are not promoted to database-fatal merely because they fail a statement.

### 39.1.2 First persistent statement write and the no-undo rule

Each admitted statement begins with:

```text
current_statement_has_published_write = false
```

It becomes true exactly when an effect logically owned by that transaction and statement reaches §12.12's publication point as a valid ordinary WAL-backed database mutation understood by crash recovery and transaction visibility. The same event sets the transaction-wide `has_persistent_writes = true`, which never resets before transaction end. `has_persistent_writes` is intentionally broader: a user-transaction-chained empty-page allocation or other structural record may require later terminal WAL and set the transaction-wide bit without setting the per-statement logical-effect bit. Examples that set both include:

- publishing an inserted heap version,
- publishing an old-version `xmax/cmax` delete or supersession marker,
- publishing a B+ entry/MTR whose logical entry belongs to the transaction's tuple version,
- publishing transaction-owned catalog rows or catalog-row deletion markers,
- publishing any `sys_statistics` row for the statement's StatsVersion.

The following do **not** cross the boundary:

- parsing, binding, planning, expression evaluation, locks, snapshots, and query memory,
- temporary RowCollections/spill files/result spools,
- candidate LSN reservation or WAL-record construction alone,
- a §12.12 provisional mutation restored before its publication-authorizing append,
- durable TxnId/FileId/catalog-object-ID reservation/allocation gaps,
- publication of an empty ordinary page/PageNo or pure B+ allocation/shape/FSM maintenance state that is architecture-valid independently of the user transaction and carries no transaction-owned tuple/index/catalog/statistics effect,
- private DDL file construction and `PRIVATE_DURABLE`/`FINAL_DURABLE_UNCOMMITTED` orphan-capable files before transaction-owned catalog mutation,
- read-only work and system maintenance effects not logically owned by the user statement.

A WAL LSN or valid preparatory/structural record alone is insufficient; the transaction-owned logical mutation publication boundary matters. A record/MTR that also publishes a tuple version, `xmax`, logical index entry, catalog row, or statistics row does cross the boundary even if it simultaneously performs structural allocation. Once this per-statement flag becomes true it never becomes false for that attempt. The architecture has no savepoint, subtransaction, or complete physical statement-undo mechanism that can erase a crossed boundary. §12.12 rollback prevents one failing primitive from crossing it; it does not undo an earlier published effect from the same statement.

The normative rule is:

```text
recoverable non-success while flag == false:
    FAILED_TRANSACTION_REMAINS_ACTIVE

recoverable non-success while flag == true:
    FAILED_TRANSACTION_MUST_ABORT

independently transaction-fatal error:
    FAILED_TRANSACTION_MUST_ABORT regardless of flag

database/storage-fatal error:
    DATABASE_NONCONTINUABLE regardless of flag
```

For DML this rule classifies statement execution non-success before explicit-
transaction §31.9 `R` and, for autocommit, result-preparation non-success before
implicit COMMIT admission. Once autocommit enters C0, §§39.1.5–39.1.7 rather
than the first-write matrix own COMMIT failure, cancellation, crash, and session
loss. Successful C4–C5 is followed by a non-failing prepared `R` transition;
after `R`, ordinary result-spool reading, cursor advancement, cancellation of
remaining delivery, or transport failure is not a new non-success of the
completed statement and does not reapply this first-write matrix. Independently
transaction-fatal, session-loss, corruption, and database-noncontinuable owners
remain effective.

Earlier **successful statements** in the same explicit transaction do not make an otherwise effect-free later statement error transaction-fatal. The boundary is per current statement. Conversely, physical invisibility or complete-generation filtering of partial current-statement rows does not permit that transaction to commit them.

A failed SELECT or other read-only statement therefore normally leaves an explicit transaction `ACTIVE`. Persistent corruption, a lower-layer noncontinuable state, or an internal invariant failure overrides this statement-only rule.

### 39.1.3 Normative statement-error matrix

In this table:

```text
FA = FAILED_TRANSACTION_REMAINS_ACTIVE; COMMIT remains legal; no ABORT required
MA = FAILED_TRANSACTION_MUST_ABORT; COMMIT forbidden; automatic ABORT required
NC = DATABASE_NONCONTINUABLE; no ordinary transaction/connection continuation
AB = semantic ABORT requested/completed through §15.6
```

“Before” and “after” refer to the current statement's first persistent-write boundary.
For user DML, §§15.1.3 and 21.16.1 make the after-boundary state unreachable
for ordinary row-semantic candidates: those candidates close and reduce before
the boundary. The ordinary rows below say `unreachable` for that reason, not
because the boundary or the dynamic-failure rule changed.

| Failure class | Before boundary | After boundary | Required qualification |
|---|---:|---:|---|
| parse/lex error | FA | MA | normally cannot arise after execution, but delayed discovery cannot evade the universal rule |
| bind/name/type/catalog error | FA | MA | unsupported feature is the same |
| planner/optimizer resource failure | FA | MA | final-plan invariant failure is NC, not a user/resource error |
| ordinary user-DML scalar/subquery/final-row expression, arithmetic, division, conversion, cast, or assignment candidate | FA | unreachable | complete demanded candidate domain closes before user-DML publication under §21.16.1 |
| non-DML expression evaluation, arithmetic, division, or cast error | FA | MA | applies only where the owning statement can reach the boundary before that evaluation; §25.1.1 remains authoritative |
| aggregate COUNT/integer-SUM final `NUMERIC_OVERFLOW` | FA | MA | §29.3 defers numerical range failure to deterministic Finalize after complete demanded input; physical reduction shape cannot change the outcome |
| ordinary user-DML NOT NULL, UNIQUE, or PRIMARY KEY candidate | FA | unreachable | §11.10 decides conflict membership; §§15.1.3/21.16.1 close and reduce it before publication |
| READ COMMITTED write conflict or stale-target revalidation | retry/FA | MA | transparent retry is permitted only while the flag is false |
| REPEATABLE READ serialization failure or deadlock victim | MA | MA | independently transaction-fatal; victim ownership remains protective through ABORTED publication and cannot remain held after canonical A3 cleanup |
| cancellation/lock cancellation | FA | MA | unless deadlock/other transaction-fatal cause applies |
| `OutOfMemory` | FA | MA | temporary allocation itself is not persistent state |
| `SpillIOError`, temp disk full, or spill corruption | FA | MA | spill files remain temporary |
| BufferPool fetch/load persistent-page corruption | NC | NC | ordinary online execution stops; controlled recovery/repair is required |
| known BufferPool flush or WAL-durability failure during ordinary execution | FA | MA | the lower layer preserves dirty state; COMMIT/ABORT use §§39.1.5–39.1.6 |
| known WAL append failure with exact §12.12 restoration | FA | MA | the failed primitive adds no published write; earlier statement writes still govern |
| MTR provisional failure with exact restoration | FA | MA | exact pre-MTR state is not transaction garbage |
| MTR/append/restoration state reported noncontinuable | NC | NC | lower-layer fatal classification cannot be downgraded |
| known raw page/file I/O failure with coherent retained state | FA | MA | uncertain ownership/bytes/length instead yields NC |
| DDL namespace synchronization/publication failure | FA | MA | FA requires no catalog mutation and conservative orphan ownership; incoherent namespace ownership yields NC |
| ANALYZE collection/encoding failure | FA | MA | writing the first transaction-owned statistics row crosses the boundary |
| explicit user ROLLBACK | AB | AB | enters ABORTING; it is not a statement error |
| internal invariant failure | NC | NC | assertions are not a continuation policy |

For every `MA` result the command layer first records the original structured error, atomically changes `ACTIVE -> MUST_ABORT`, and invokes ABORT. The error response is not successful statement completion and indicates that the explicit transaction ended ABORTED. If ABORT itself encounters a stronger failure, §39.1.7 precedence applies.

The matrix applies when the failure terminates current statement execution:
before explicit-transaction `R`, or before autocommit C0. Once autocommit C0 is
admitted, §§39.1.5–39.1.7 own COMMIT and its uncertainty; no ordinary fallible
required result work remains after successful C4–C5. Section 31.9 owns ordinary
post-`R` DML result-delivery failures; they terminate the cursor, not the
already completed statement. An unrelated background copied-writeback/WAL
flush error that merely leaves another page dirty does not retroactively fail
an already completed or concurrently running statement; it is recorded and
affects a transaction only when a required operation observes/returns that
error or the lower layer raises the database-noncontinuable gate.

### 39.1.4 Statement completion, retry, and subsystem consequences

CommandId and snapshots obey §§9.6 and 9.9:

- one logical statement and all of its internal pre-write retries share one CommandId,
- every failed or successful admitted statement consumes that CommandId,
- a `FAILED_TRANSACTION_REMAINS_ACTIVE` READ COMMITTED statement unregisters its failed snapshot and the next statement obtains a fresh snapshot,
- no ordinary CommandId-producing statement begins in `MUST_ABORT`/`COMMITTING`/`ABORTING`.

Automatic statement retry is allowed only while `current_statement_has_published_write == false` and all attempt-local output/state is discardable. A retry gets a fresh READ COMMITTED snapshot but retains the logical statement's CommandId. Once the flag is true, transparent same-TxnId retry is forbidden. The architecture does not automatically rerun an aborted autocommit transaction; any client-initiated whole-request retry, or an opt-in retry authorized by an architecture revision, uses a new transaction and TxnId.

For INSERT VALUES with a valid first occurrence and a later demanded
division/conversion/constraint candidate, ordinary DML candidate closure
establishes and reduces that candidate before publication:

```text
current_statement_has_published_write = false
statement result = FAILED_TRANSACTION_REMAINS_ACTIVE
explicit transaction = ACTIVE and COMMIT-eligible
inserted rows from this statement = none
```

The same pre-write rule covers a later demanded INSERT SELECT source error,
UPDATE SET/NOT NULL/RETURNING candidate, DELETE RETURNING candidate, or
immediate UNIQUE candidate. Discovering a new semantically demanded ordinary
user-DML candidate after publication is an execution invariant defect, not a
legal path to reclassify that candidate as `MA`.

Post-write `MA` remains reachable for dynamic failure. For example, after
ordinary closure succeeds, rows 1–4 of a five-row INSERT may publish and a
cancellation, `OutOfMemory`, known storage/WAL I/O failure, or other applicable
dynamic execution failure may terminate publication of row 5. The result is:

```text
statement result = FAILED_TRANSACTION_MUST_ABORT
transaction      = MUST_ABORT -> ABORTING -> ABORTED
COMMIT            = forbidden
published rows    = physically retained, logically aborted garbage
```

Vacuum/recovery handles those versions under the existing no-physical-undo
rules. The same dynamic post-write result applies to UPDATE after any new
version/old `xmax`/index effect publishes and to DELETE after any `xmax/cmax`
publishes. Dynamic target-spool, candidate-staging, assignment-vector, or spill
failure before the first mutation remains on the pre-write side. Exact
UNIQUE/PRIMARY KEY conflict membership is §11.10; ordinary diagnostic ranking
and closure are §21.16.1.

DML `RETURNING` remains unpublished until §31.9's `R`. If a dynamic write,
spool append, spill write, validation, finalization, physical allocation, or
ownership-transfer preparation step fails before explicit `R` or before
autocommit C0, no prefix or successful count is published; after a crossed
write boundary the execution failure requires the existing `MA` outcome. An
autocommit result is fully prepared before C0, survives C5, and needs no
ordinary fallible required operation between successful C and `R`; failures
during C0–C5 retain the COMMIT owner below. A successfully published `R` is
final: later ordinary spool-read, cursor, cancellation, or transport failure is
result delivery failure, cannot reopen DML execution, and never authorizes
same-`TxnId` retry.
For a usable explicit session the transaction remains `ACTIVE`; an actual
session loss may abort it under §39.1.7 for that independent reason. In
autocommit, `R` follows C4–C5 and the transaction remains `COMMITTED` despite
incomplete delivery. Returned rows are not durability acknowledgements and an
already returned prefix is not retracted.

OOM and spill failures follow the same boundary rather than a uniform “disk full aborts everything” rule. Query-temporary files are cleaned independently; cleanup does not erase a prior database write.

ANALYZE in-memory collection/finalization failure is FA. Once any `sys_statistics` row publishes, later failure is MA even though incomplete-generation rules prevent global use. A successful ANALYZE remains transaction-local until commit. Failure to install a committed descriptor in the global cache follows §39.1.8 and does not reopen the transaction.

For CREATE, consumed nonreusable IDs, private construction, and durable final-name publication are allocator/cleanup-owned artifacts and do not cross the transaction statement-write boundary before catalog rows publish. A recoverable pre-catalog failure may therefore be FA after transferring every survivor to deterministic orphan cleanup and releasing attempt-only DDL ownership; allocated IDs remain consumed. CREATE catalog publication and DROP catalog `xmax` publication cross the boundary; later failure is MA. Namespace uncertainty that §4.7 can conservatively classify as an orphan is not WAL append uncertainty; inability to establish that ownership is NC.

The architecture supports autocommit. A one-statement implicit transaction applies the same classification, but it has no later statement continuation: statement success enters COMMIT, while either FA or MA statement failure drives ABORT. Post-commit-record and post-durable acknowledgement uncertainty remains exactly §39.1.5.

### 39.1.5 COMMIT failure, durability, and client acknowledgement

The exact successful stages are §15.5 C0–C6. For persistent transactions, C3 (`durable_lsn >= commit_lsn`) is the irreversible durable semantic point. The resident status-page update in C2 is required runtime state under §§12.10.5 and 13.13.2 but remains NO-FORCE; the globally observable runtime terminal cache is published only at C4.

Implicit autocommit DML reaches C0 only after §31.9 has prepared every fallible
resource required for later result publication. C1 validates that already-
allocated, already-validated state and its lifetime; it does not construct the
result or treat an accounting reservation as allocation success. C5 preserves
the prepared result's independent backing and cleanup owner. Once C4–C5 finish,
`R` performs no ordinary fallible required work, so this table has no supported
post-C/pre-`R` allocation, validation, registration, or spill-I/O failure row.

| Failure point | Transaction/storage outcome | Client/API outcome |
|---|---|---|
| C0/C1 precondition or resource failure, before terminal protocol | no commit record; `ACTIVE -> MUST_ABORT`, then automatic ABORT | COMMIT failed; never success |
| C2 preparatory image/reservation/construction or known commit-record no-append failure | §§12.10.5, 12.12, and 13.13.2 restore exact pre-terminal status state; any inert preparatory image may remain; automatic ABORT | COMMIT failed and transaction aborts; no external COMMIT retry |
| C2 append outcome uncertain | storage noncontinuable; do not append ABORT, publish terminal state, or release locks | `CommitOutcomeUncertain`/connection-fatal; recovery inspects the valid persisted prefix |
| C2 commit record appended but status-page/frame publication cannot complete | §12.12 noncontinuable; COMMIT cannot be redirected to ABORT | `CommitOutcomeUncertain`/connection-fatal; recovery may commit if the record survived |
| C3 WAL write/`fdatasync` failure with exact append bytes retained | remain COMMITTING and retry durability under retained ownership; do not abort or acknowledge | request remains pending; no ordinary failure result authorizes retry/cancellation |
| C3 durability cannot be established while safe continuation/retry is impossible | storage noncontinuable; recovery decides from the surviving complete WAL prefix | `CommitOutcomeUncertain`/connection-fatal |
| after C3, before/during C4 runtime terminal publication | durable outcome is irrevocably COMMITTED; publication failure makes runtime/database noncontinuable | never ABORTED; without C6 acknowledgement report/observe commit outcome uncertainty |
| read-only COMMIT before C4, or a C4 attempt known to have left the exact coherent prepublication state | no C2/C3 or commit WAL exists; retain ownership and retry C4, or, if COMMIT cannot continue while nonpublication is exact, transition to mandatory ABORT; no terminal COMMIT has occurred | never acknowledge COMMIT success; the failed request is `FAILED_TRANSACTION_MUST_ABORT` if it is terminated through ABORT |
| read-only C4 attempt whose atomic publication/nonpublication result or registry/cache coherence cannot be established | database noncontinuable; retain locks/registry ownership and permit recovery/controlled stop to classify the transaction from canonical evidence | `COMMIT_OUTCOME_UNCERTAIN`/connection-fatal; do not invent C3 durability or publish ABORTED |
| read-only C4 has atomically published `COMMITTED`, including a failure observed immediately afterward | outcome is irrevocably COMMITTED; continue with the shared C5 cleanup rules | never ABORTED; C6 acknowledgement and transport rules remain shared with persistent COMMIT |
| C5 catalog/statistics cache side effect fails but safe invalidate/bypass/fallback succeeds | COMMITTED; continue cleanup using authoritative MVCC catalog/statistics fallback | success may still be acknowledged after C5 completes safely |
| C5 registry/lock/visibility cleanup cannot be made coherent | COMMITTED plus database noncontinuable; retain unsafe ownership rather than expose contradiction | commit outcome uncertain unless success was already delivered (delivery occurs only at C6) |
| C6 success acknowledgement delivered | COMMITTED, runtime publication and required cleanup complete | `SUCCESS`; successful COMMIT response delivered |
| C6 socket/write failure before delivery | COMMITTED; database may remain healthy and closes that connection | `COMMIT_OUTCOME_UNCERTAIN` plus `ConnectionFailure`; no success response was delivered and retrying the transaction is unsafe without application reconciliation |

Once the publication-authorizing commit record validly appends, COMMIT is uncancellable: client disconnect, timeout, or cancellation does not initiate ABORT. The server continues C2–C5 or enters the noncontinuable gate. A known commit append failure is not exposed as a retryable COMMIT on the same transaction; the server aborts that transaction, and an application retry begins a new transaction.

A read-only transaction elides C2–C3 exactly as §15.5 requires: it has no `commit_lsn`, no `durable_lsn` event, and no fabricated terminal WAL record. For that path, the §9.14 C4 atomic terminal-publication linearization is the publication-authorizing and irreversible COMMIT boundary. Before that linearization, an exact coherent nonpublication result remains eligible for retained-owner retry or mandatory ABORT; once C4 has begun, its owner may not abandon a possibly publishing operation merely because cancellation or connection loss arrives. If the owner cannot establish either authoritative `COMMITTED` publication or exact coherent nonpublication, the database is noncontinuable. After C4 linearizes, every later failure is a C5/C6 failure and cannot authorize ABORT.

`WAL_POSITION_EXHAUSTED` cannot be the expected cause of a persistent
transaction's C2/A1 terminal-record no-append because §4.3.2.4 retained its
closure credit before the first persistent WAL record. If numeric capacity is
nevertheless unavailable, terminal-headroom accounting is incoherent and the
database follows the noncontinuable invariant-failure path; it does not restore
COMMIT eligibility or strand an admitted writer deliberately.

A group-commit flush failure applies this same rule to every waiting transaction whose commit record already validly appended. They remain COMMITTING while the exact shared WAL prefix is retried; none may be selectively converted to ABORTED because its individual waiter has not yet been acknowledged.

Under §§12.10.5 and 13.13.2, resident status-page installation is a C2 prerequisite and may not be intentionally deferred until after durability. If C3 has nevertheless completed and a latent failure proves that required installation/runtime state was not completed, the transaction remains COMMITTED and the database is noncontinuable. Ordinary post-C3 runtime cache publication, logical-lock release bookkeeping, catalog/statistics cache publication, or client transport failure likewise cannot change the outcome. No error path may append/publish ABORTED for that TxnId. A nonrequired background status-page flush scheduling failure may leave the page dirty/retryable under §7.10 and does not by itself fail an otherwise complete COMMIT.

Successful COMMIT is sent only after C4 terminal publication and C5's required coherent installed-or-invalidated cache/ownership state. The architecture does not acknowledge at WAL durability and finish runtime publication asynchronously.

A process crash after durable COMMIT but before autocommit `R` leaves recovery
authority unchanged: the transaction is COMMITTED, while the successful result
envelope may never have been published or observed. The affected count is not
reconstructed and absence of a response does not authorize blind request
replay. A conforming in-process implementation cannot reach the same interval
through an ordinary required result-preparation failure; such work completed
before C0.

### 39.1.6 ABORT failure and automatic MUST_ABORT cleanup

The exact successful stages are §15.6 A0–A4. ABORT performs semantic outcome publication, not physical user-DML undo.

| Failure point | Required outcome |
|---|---|
| before A1 with no persistent state | publish runtime ABORTED, release after terminal publication, acknowledge |
| known preparatory/`TXN_ABORT` append failure | retain ABORTING/locks and internally retry while exact outcome is known; never restore ACTIVE/commit eligibility |
| append outcome uncertain or post-abort-record status-page publication failure | storage noncontinuable; do not release locks as ordinary state; controlled stop/recovery |
| inability to complete a known-failure retry/abort publication | storage noncontinuable; preserve original error and require recovery loser handling |
| A2 runtime ABORTED publication failure | storage noncontinuable; no false lock release or terminal-cache claim |
| A3 cleanup failure after terminal ABORTED publication | outcome remains ABORTED; lock/registry ownership uncertainty makes the database noncontinuable |
| A4 acknowledgement delivered | outcome is ABORTED and required cleanup is complete; explicit ROLLBACK has command outcome `SUCCESS`, while automatic ABORT returns the originating failed-command outcome with transaction state ABORTED |
| A4 acknowledgement transport failure | outcome remains ABORTED; required A3 cleanup remains complete; close the connection and report/chain `ConnectionFailure`; an explicit ROLLBACK completed server-side with `SUCCESS` but no success response was delivered, while automatic ABORT retains its originating failed-command outcome; never use `COMMIT_OUTCOME_UNCERTAIN` |

Ordinary ABORT need not synchronously make `TXN_ABORT` durable. If it is lost, crash recovery classifies the noncommitted transaction as a loser and establishes ABORTED. An uncertain/failed abort can therefore never become COMMITTED, but ordinary execution still cannot release locks or claim clean completion without the required runtime terminal publication.

### 39.1.7 Connection loss, recovery classification, and error precedence

An ordinary post-`R` cursor read or transport error that leaves the session
usable is not connection/session loss. It leaves an explicit transaction in
its actual `ACTIVE` state under §31.9. If the session is actually lost, the
table below applies independently; aborting an active explicit transaction for
session loss does not retroactively make its published DML statement a failed
statement. A committed autocommit transaction cannot be reversed.

Connection/session loss has these outcomes:

| Runtime point | Required handling |
|---|---|
| ACTIVE, no COMMIT in progress | automatically ABORT and clean up |
| MUST_ABORT | continue/join mandatory ABORT cleanup |
| persistent COMMITTING before publication-authorizing commit append | cancel commit and ABORT; no response is delivered on the lost connection, and the actual noncommit outcome is not `COMMIT_OUTCOME_UNCERTAIN` |
| persistent COMMITTING after publication-authorizing append, before durable commit | commit is uncancellable; continue it or enter noncontinuable/recovery; client outcome is uncertain |
| persistent COMMITTING after durable C3, before C4 publication | transaction is semantically COMMITTED; finish C4–C5 or enter the noncontinuable gate; never ABORT; client outcome is uncertain |
| read-only COMMITTING before C4 begins | cancel commit and ABORT; no WAL COMMIT or durable point is fabricated; no response is delivered on the lost connection, and the actual noncommit outcome is not `COMMIT_OUTCOME_UNCERTAIN` |
| read-only COMMITTING during C4 | do not abandon the atomic publication attempt: complete/retry it; if exact coherent nonpublication is established, ABORT remains legal, if `COMMITTED` publication linearizes, continue C5, and if neither result can be established, enter the noncontinuable gate; without a delivered response use `COMMIT_OUTCOME_UNCERTAIN` because the client cannot know which canonical result occurred |
| COMMITTED after C4, before C5 cleanup completes | continue required C5 cleanup despite disconnect; retain locks/registry ownership until their canonical cleanup point; never ABORT or reverse terminal publication; client outcome is uncertain |
| COMMITTED after C5, before C6 acknowledgement | required transaction cleanup is complete; close the connection without changing `COMMITTED`; the client outcome is uncertain |
| COMMITTED after delivered C6 response | close the connection; terminal state and completed cleanup are unchanged |
| ABORTING before A2 terminal publication | continue abort publication and cleanup; never revive transaction or permit COMMIT |
| ABORTED after A2, before A3 cleanup completes | continue required A3 cleanup despite disconnect; never reactivate the transaction; retain locks until the canonical A3 release point |
| ABORTED after A3, before A4 acknowledgement | required transaction cleanup is complete; close the connection without changing `ABORTED`; an explicit ROLLBACK has no delivered success and reports `ConnectionFailure`, not `COMMIT_OUTCOME_UNCERTAIN` |
| ABORTED after delivered A4 response | close the connection; terminal state and completed cleanup are unchanged |

For implicit autocommit DML, the applicable persistent or read-only
`COMMITTED`-before-C6 row includes loss before §31.9 `R`. The prepared temporary
result is cleaned when safe, but no `R`, successful count authority, durable
response record, result reconstruction, or automatic DML replay is fabricated.
If the session remains usable and only cancellation is pending after successful
C4–C5, that is not session loss: the prepared `R` transition completes and the
cancellation applies to remaining delivery.

Crash recovery maps runtime states using only persisted WAL/status evidence:

| Precrash runtime state | Recovery outcome |
|---|---|
| ACTIVE or MUST_ABORT | loser -> ABORTED |
| COMMITTING before a surviving complete `TXN_COMMIT` | loser -> ABORTED |
| COMMITTING with complete commit record in the valid persisted WAL prefix | COMMITTED |
| COMMITTING after durable commit | COMMITTED |
| ABORTING with or without a surviving abort record | ABORTED, directly or by loser resolution |
| COMMITTED | COMMITTED |
| ABORTED | ABORTED |

Runtime-only states require no additional persisted status code.

When multiple errors occur, semantic precedence is:

```text
durable COMMIT outcome
    dominates every later error and can never be rewritten

authoritative read-only C4 COMMITTED publication
    is equally terminal and can never be rewritten

required noncommit/MUST_ABORT outcome
    remains noncommit even if abort cleanup later fails

authoritative ABORTED publication
    remains ABORTED even if cleanup or acknowledgement later fails

database/storage noncontinuable condition
    is additionally reported and governs server continuation

original statement/commit/rollback error
    remains the primary causal diagnostic, with cleanup/fatal errors chained

response delivery status
    reports acknowledgement or transport separately and never changes semantic state
```

For example, a UNIQUE violation followed by fatal abort I/O remains “UNIQUE violation; transaction must not commit” plus “database noncontinuable”; the cleanup error does not erase the original cause or make COMMIT legal.

### 39.1.8 Locks, terminal cache, derived caches, and observability

Transaction-lifetime locks, both TableWriterGate modes, SchemaLock,
STATS_PUBLISH/MANIFEST_CHANGE modes, and transaction snapshots remain owned
through §9.14 terminal publication. `MUST_ABORT` alone does not release them.
COMMIT releases only after COMMITTED publication; ABORT releases only after
ABORTED publication. Another transaction therefore never observes released
conflict protection while status lookup still says IN_PROGRESS. Every blocking
transaction dependency among those gates and TUPLE_WRITE/UNIQUE_KEY is visible
to §11.13's one graph.

`UNSUPPORTED_TRANSACTION_GATE_TRANSITION` is a proactive, pre-publication
statement admission error and follows the ordinary FA rule for an explicit
transaction. `DEADLOCK_DETECTED` is independently transaction-fatal and follows
the existing deadlock-victim MA row regardless of the current statement's write
boundary. Object RETIRING/revalidation failure retains its owning operation's
ordinary pre/post-publication classification. Only synchronization bookkeeping
incoherence, not a normal cycle, may escalate to database noncontinuability.

The globally observable terminal-outcome cache updates in the same §9.14 linearization that changes runtime state and active-registry membership. A BufferPool-resident `TXN_STATUS` page is not that cache and cannot independently make a transaction publicly terminal.

Catalog and statistics caches do not define transaction outcome. After durable commit they must either install the committed descriptor, safely invalidate/bypass the affected entry so authoritative MVCC catalog lookup is used, or—statistics only—retain an older/missing approximate descriptor. If coherent fallback cannot be guaranteed, the transaction remains COMMITTED and the database becomes noncontinuable.

Diagnostics/API metadata must distinguish at least:

```text
statement failed; transaction remains ACTIVE
statement failed; transaction entered mandatory ABORT and is ABORTED/ABORTING
statement succeeded; result delivery failed and a usable explicit transaction remains ACTIVE
COMMIT not acknowledged; client outcome uncertain
transaction durably COMMITTED despite later completion/transport failure
database/storage noncontinuable
```

The following implementations are forbidden:

1. committing partial user-DML effects after a post-write dynamic failure made
   their statement fail,
2. transparently retrying a statement after any transaction-owned write published,
3. reusing a failed statement's CommandId or READ COMMITTED snapshot,
4. treating an exactly restored §12.12 provisional mutation as published garbage,
5. changing durable COMMITTED to ABORTED or reporting ABORTED after durable commit,
6. acknowledging COMMIT before C4/C5 required runtime completion,
7. releasing transaction locks before terminal publication,
8. executing ordinary statements or COMMIT in `MUST_ABORT`,
9. making an ordinary effect-free SELECT user error transaction-fatal without an independent fatal cause,
10. treating every OOM/spill/disk-full failure identically without checking the statement boundary,
11. exposing any RETURNING prefix before `R` from statement execution that
    later fails, or exposing autocommit RETURNING before implicit COMMIT C4–C5,
12. allowing cache publication failure to redefine durable transaction outcome,
13. excluding SchemaLock/TableWriterGate/object-publication dependencies from the tuple/unique wait-for graph,
14. releasing a deadlock victim's transaction-owned gates before ABORTED terminal publication,
15. treating an ordinary post-`R` DML cursor error as retroactive statement
    failure, affected-count revocation, or permission for automatic DML retry,
16. reporting `FINISHED` for a cursor whose remaining result delivery failed,
17. requiring a client to drain a published RETURNING cursor to preserve the
    statement's already successful outcome.
18. admitting autocommit DML to C0 while any required result allocation,
    validation, spill I/O, handle preparation, resource registration, or
    cleanup-owner preparation remains incomplete;
19. treating a Chapter-24 accounting grant as proof that a required result
    allocation succeeded;
20. performing ordinary fallible required result-publication work between
    successful C4–C5 and `R`, or redirecting COMMITTED to ABORT when such an
    internal protocol defect is detected.

## 39.2 SQL front-end and logical-planning errors

The upper semantic layer distinguishes at least:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature
CardinalityError
FrontEndResourceLimit
```

Where a failure originates from SQL source, it carries the canonical
`SourceSpan` selected under §18.8.

User input errors do not become internal invariant failures merely because they are discovered after binding.

`LexerError` and `ParserError` classify source-language failures.
`FrontEndResourceLimit` instead means that a deliberate front-end safety or
configured operational budget was reached. `OutOfMemory` is the existing
cross-layer operational cause and retains that classification when a required
allocation fails during lexing, parsing, payload decoding, raw-AST
construction, or binding. Neither resource outcome becomes a source-language,
bind, or type error merely because it occurs in that stage. Both are controlled
operational outcomes, not internal invariant failures merely because they
occur during front-end or binding work. The exact front-end distinction and
complete-result rule are §18.17.

Logical-plan validator failures, by contrast, indicate internal architecture/implementation defects.

## 39.3 Execution errors

The execution layer distinguishes controlled failures including:

```text
ExecutionError
OutOfMemory
SpillIOError
QueryCancelled
CardinalityViolation
CastError
ArithmeticError
ConstraintViolation
TransactionConflict
```

`OutOfMemory` is a query operational resource cause, not an
execution-exclusive category. This section owns its execution-layer reporting
and cleanup when execution has begun; a front-end or binding occurrence uses
the same cause under §§18.17 and 39.2.

If a valid VARCHAR value cannot be materialized because no supported runtime
representation can express its exact byte length, execution reports controlled
`ExecutionError` with a runtime value-representability/resource-limit cause as
defined in §23.9. This does not make the value invalid and does not authorize
truncation or wraparound. If an exact supported representation exists but its
required allocation cannot be obtained, the cause remains `OutOfMemory`.

The same distinction applies to Chapter-24 memory and row storage. A valid
runtime size/address demand or retained row/value that no supported exact
runtime representation can express reports controlled `ExecutionError` with a
representability/resource-limit cause. An ordinary catchable allocation denial
for a supported exact representable form, or a hard-gate denial for which no
exact progress-producing action remains, reports `OutOfMemory`. Non-catchable
operating-system or process termination is outside the guarantee that execution
returns a controlled error.

`SpillIOError` covers temporary spill read/write or creation failure, temporary
capacity exhaustion including `ENOSPC`, unsupported spill encoding/file-offset/
I/O range addressability, and malformed spill framing, checksum, count, offset,
range, extent, or owner-defined structure. These are temporary spill failures,
not persistent-database corruption. Chapter-24 self-generated in-memory
construction-invariant violations remain internal defects rather than being
reclassified as spill I/O failures.

For DML RETURNING, controlled temporary spill, allocation, and runtime
representability categories occur in owner-specific stages. Before explicit
`R`, or before autocommit C0, required result preparation failures are statement
execution failures and §39.1 uses the actual first-write state. During
autocommit C0–C5, the existing COMMIT owner applies; all ordinary fallible
result-publication preparation has already completed. Successful C4–C5 is
followed by a non-failing prepared `R` transition. After `R`, an ordinary
result-spool read/framing or delivery-chunk materialization failure is terminal
result-delivery error: it is not successful cursor exhaustion and does not
retroactively change statement or transaction outcome. This stage distinction
does not downgrade an independently established persistent-corruption,
invariant, session-loss, or noncontinuable condition.

Lower-layer structured causes are preserved rather than erased. Examples include:

```text
CardinalityViolation -> SQL CardinalityError surface
ConstraintViolation  -> may contain UniqueViolation
TransactionConflict  -> may contain SerializationFailure / DeadlockVictim
```

RAII/lifetime ownership unwinds query-owned chunks, row collections, reservations, spill files, pipeline tasks, and operator state.

Execution failure does not independently release transaction-owned logical locks; the command/transaction layer drives the required abort/terminal path.

That command-layer decision is exactly §39.1's statement-write matrix; an execution operator does not choose statement-only versus transaction-fatal behavior independently.

No partially active pipeline/task graph remains runnable after a terminal query execution failure.

Scalar-subquery `CardinalityViolation` and every subquery child/build error use
§20.14.12's observable precedence and this same §39.1 statement-write matrix.
There is no subquery-local transaction rollback or retry policy.

### 39.3.1 Checked integer arithmetic

The exact unary/binary overloads, result types, quotient, remainder, and errors
are canonical in §§17.6.1–17.6.2; this section states their runtime enforcement.

Integer kernels MUST NOT rely on C++ signed-overflow undefined behavior.

Operations that can overflow use checked arithmetic for:

```text
+
-
*
unary minus
MIN_INT / -1
aggregate integer SUM/COUNT final result conversion
```

Scalar overflow and §29.3 integer/COUNT final aggregate range failure raise
`ArithmeticError`/`NUMERIC_OVERFLOW`. Exact integer aggregate Update/Combine itself
does not raise merely because an integer subtotal leaves the result domain;
inability to allocate/preserve its exact state is instead the existing query
resource error. FLOAT64 SUM/AVG tree-edge overflow follows §29.3's IEEE
infinity result and is not `NUMERIC_OVERFLOW`.

Integer division or remainder by zero raises `ArithmeticError`.

### 39.3.2 FLOAT64 arithmetic/division

FLOAT64 arithmetic follows the exact §17.4.3 binary64 rounding/NaN/signed-zero
semantics and §17.6.2 overload registry.

FLOAT64 aggregate SUM/AVG use §17.4.3 binary64 addition at each edge of a
physical §29.3 reduction tree. Tree shape may change finite low bits within
that bounded contract. Explicit-input NaN/infinity classification,
canonical exposed NaN/`+0.0`, and AVG's exact-count conversion and final
binary64 division are authoritative in §29.3.

In particular, FLOAT64 division by zero follows IEEE results rather than the integer division rule:

```text
finite nonzero / ±0.0 -> signed infinity
±0.0 / ±0.0          -> NaN
infinity / infinity   -> NaN
```

NaN/zero comparison/grouping/index semantics remain those defined in Chapters 8, 17, 20, and 29.

Compiler modes that silently replace these semantics are not allowed.

## 39.4 Optimizer errors

Optimizer planning may fail with controlled internal/resource categories such as:

```text
OptimizerError
OptimizerResourceLimit
```

`OptimizerResourceLimit` means the configured planning-time/memory bound prevented completion even after the bounded heuristic fallback.

It is not an out-of-process arbitrary allocation crash.

A cost-model mistake that merely selects a slower semantically valid plan is **not** an optimizer runtime error.

A final-plan semantic/property-validation failure is an internal invariant failure and MUST NOT be executed.
