# 9. Transaction Lifecycle and Snapshots

## 9.1 Scope and subsystem coordination

The transaction subsystem is a coordinated architecture rather than a set of independent local policies.

The following responsibilities must agree on transaction identity and visibility:

```text
TransactionManager
SnapshotManager
VisibilityManager
LockManager
TransactionStatusStore
```

They also interact with the durability and reclamation responsibilities owned by Chapters 12–14:

```text
WalManager
CommitCoordinator
CheckpointManager
RecoveryManager
VacuumManager
ReadEpochManager
```

This chapter owns:

- transaction identifiers and reservation,
- transaction registration and write-status-dependency lifetime,
- transaction lifecycle state,
- isolation-level identity,
- command IDs,
- snapshots and snapshot lifetime,
- transaction-status lookup/publication semantics,
- read-only transaction behavior.

Chapter 10 owns tuple visibility.

Chapter 11 owns logical write/uniqueness conflicts.

WAL record formats and commit flushing are owned by §§12.2–12.15; checkpoints and crash reconstruction are owned by §§13.4–13.19.

A shortcut in one subsystem MUST NOT violate the identity, visibility, durability, conflict, or reclamation rules of another.

## 9.2 Transaction identifiers

Transaction identifiers are 64-bit values.

The reserved values are:

```text
INVALID_TXN_ID      = 0
FROZEN_TXN_ID       = 1
FIRST_NORMAL_TXN_ID = 2
```

Normal transaction IDs are monotonically allocated beginning at:

```text
2
```

A normal transaction ID MUST NOT be reused after it has entered a durably reserved allocation range, even if a crash occurs before that ID is actually handed to a transaction.

`FROZEN_TXN_ID` means:

> the creator transaction committed sufficiently long ago that ordinary visibility no longer requires lookup of its original transaction-status entry.

For visibility purposes:

```text
FROZEN_TXN_ID -> COMMITTED
```

The 64-bit space makes numerical wraparound practically irrelevant for the intended system lifetime.

The allocator MUST NOT wrap and reuse old normal transaction IDs.

The last normal TxnId the architecture can reserve/allocate is the exact fixed-block value in
§4.3.2.1; larger uint64 values are not a partial final reservation block.

Freezing exists to permit eventual transaction-status reclamation, not primarily to solve 32-bit-style wraparound.

## 9.3 Durable transaction-ID reservation

The transaction allocator reserves normal TxnIds in durable blocks rather than synchronizing durable control metadata for every `BEGIN`.

The reservation block size is exactly:

```text
1,048,576 transaction IDs
```

Durable global control metadata stores:

```text
reserved_txn_id_end
```

The architecture defines this value as an **exclusive upper bound**.

At any instant, the durable reserved normal-TxnId space is:

```text
[FIRST_NORMAL_TXN_ID, reserved_txn_id_end)
```

and the allocator's current usable in-memory suffix is:

```text
[next_txn_id, reserved_txn_id_end)
```

A newly initialized database begins with:

```text
next_txn_id         = FIRST_NORMAL_TXN_ID
reserved_txn_id_end = FIRST_NORMAL_TXN_ID
```

which means no normal TxnId has yet been durably reserved.

Before the allocator can hand out the first normal TxnId—or any later ID when:

```text
next_txn_id == reserved_txn_id_end
```

it performs the block-reservation protocol:

```text
old_end = reserved_txn_id_end

new_end =
    old_end + 1,048,576
```

using checked 64-bit arithmetic.

Then:

1. durably publish `new_end` to the database control file,
2. only after that durable update succeeds set the runtime reservation end to `new_end`,
3. hand out TxnIds from the half-open interval:
   ```text
   [old_end, new_end)
   ```

If the durable reservation update fails, IDs from the not-yet-durable interval MUST NOT be handed out.

If adding the next exact reservation block would exceed
`MAX_RESERVED_TXN_ID_END` in §4.3.2.1, beginning a new transaction returns
`TXN_ID_EXHAUSTED` rather than shrinking the block, wrapping, or reusing an old ID.

After a crash, unused IDs from an already durable reservation MAY be skipped permanently.

An ID that may have appeared in persistent database state MUST never be reused.

The database-control-file physical layout and torn-update protocol are owned by §§13.2.1–13.2.4.

## 9.4 Transaction object and lifecycle state

A transaction contains at least the conceptual state:

```text
TxnId txn_id
TxnState state
IsolationLevel isolation
CommandId current_command_id
optional<Snapshot> transaction_snapshot
optional<Snapshot> statement_snapshot
Lsn last_wal_lsn
bool has_persistent_writes
bool current_statement_has_published_write
write-capable or explicitly read-only classification
registered write-status dependency when write-capable
held logical locks
held SchemaLock/TableWriterGate/object-publication modes
cancellation/deadlock flag
```

Every normal transaction is classified at registration as either:

```text
write-capable
explicitly read-only
```

Ordinary transactions are write-capable unless the transaction contract explicitly
establishes read-only mode. The classification is immutable for that transaction. Absence
of a prior write does not make a transaction read-only.

A **write-status dependency** is a runtime status-history-retention claim for one
write-capable transaction's own normal TxnId. Every write-capable transaction MUST register
that dependency as part of transaction registration, before admission of any operation
that may publish the TxnId in persistent `xmin`, `xmax`, or other correctness metadata.
The dependency is not a SQL snapshot, `StatusHistoryGuard`, or read epoch.

The dependency remains registered while the transaction is `ACTIVE`, `MUST_ABORT`,
`COMMITTING`, or `ABORTING`, and through terminal publication. COMMIT releases it during
C5; ABORT releases it during A3. A recoverable statement failure that leaves the
transaction ACTIVE does not release it, and entry into MUST_ABORT retains it through the
ordinary abort cleanup. Releasing this runtime future-publication blocker does not retire
existing persistent metadata: Chapter 14 must still prove every surviving reference to
the TxnId status-independent before the cutoff may pass it. Failure to complete safe C5/A3
cleanup follows the existing failure/noncontinuable policy and never authorizes an early
dependency release.

The conceptual runtime transaction states are:

```text
ACTIVE
MUST_ABORT
COMMITTING
COMMITTED
ABORTING
ABORTED
```

`ACTIVE` may begin ordinary statements and may request COMMIT or ABORT. `MUST_ABORT` is transaction-fatal but nonterminal: no ordinary statement or COMMIT is legal, and the command layer must drive ABORT/cleanup. `COMMITTING` and `ABORTING` admit no ordinary statement and retain the transaction's locks/registry ownership until terminal publication. `COMMITTED` and `ABORTED` are terminal.

The legal state-transition shapes are:

```text
ACTIVE -> COMMITTING -> COMMITTED
ACTIVE -> COMMITTING(before publication-authorizing append)
       -> MUST_ABORT -> ABORTING -> ABORTED
ACTIVE -> ABORTING -> ABORTED
ACTIVE -> MUST_ABORT -> ABORTING -> ABORTED
```

`COMMITTING -> MUST_ABORT` is legal only when commit fails or is canceled before the publication-authorizing `TXN_COMMIT` append. After that append, `COMMITTING` cannot transition to `MUST_ABORT`, `ABORTING`, or `ABORTED`; §39.1 owns completion/failure semantics from that point. State transitions are monotonic.

`COMMITTING` and `ABORTING` are transient lifecycle states.

`COMMITTED` and `ABORTED` are terminal states.

A terminal transaction MUST NOT transition back to an active/transient state.

Transaction admission is additionally gated by the database-owner state in
§3.3: only `READY` may create/admit an ordinary transaction or statement.
Controlled shutdown handles each nonterminal transaction state exactly as
§3.3.6 specifies without weakening the terminal-publication rules below.

The terminal status/WAL mutation and durability steps that complete `COMMITTING` are owned by §12.10.5 and §§12.13–12.15; §§13.12–13.19 own terminal-state reconstruction after a crash, and §39.1 owns failure/continuation outcomes.

## 9.5 Isolation levels

The architecture supports:

```text
READ COMMITTED
REPEATABLE READ
```

The default is:

```text
READ COMMITTED
```

`REPEATABLE READ` implements:

```text
snapshot isolation
```

It is **not** full serializability.

The SQL surface, documentation, diagnostics, and tests MUST NOT describe snapshot isolation as SERIALIZABLE.

Deferred isolation/concurrency mechanisms include:

```text
SERIALIZABLE
Serializable Snapshot Isolation (SSI)
predicate locking
key-range locking
```

## 9.6 Command IDs

Each SQL statement inside one transaction executes under one:

```text
CommandId
```

The initial command ID is:

```text
0
```

and `CommandId{0}` is a valid command ID.

The command layer assigns the current CommandId once when it admits one nonterminal SQL statement in an `ACTIVE` transaction. Every internal attempt/retry of that same logical statement uses that same CommandId.

The assigned CommandId is consumed exactly once when the statement ends, whether it succeeds, fails while leaving the transaction `ACTIVE`, or makes the transaction `MUST_ABORT`. CommandIds are monotonically increasing and never reused. If the transaction remains `ACTIVE`, its next admitted statement receives the checked next CommandId. Transaction-control commands that terminate the transaction do not need a new MVCC command boundary.

A parse/bind/planning failure inside an explicit transaction is still a failed admitted statement for this purpose. No tuple may exist for that CommandId, but reusing it is forbidden. `CommandId{UINT32_MAX}` is usable by one admitted statement; afterward §4.3.2.2 rejects later ordinary statement admission with `COMMAND_ID_EXHAUSTED` while preserving COMMIT/ROLLBACK eligibility for existing work.

Tuple versions store:

```text
cmin
cmax
```

so same-transaction visibility can distinguish:

- versions created by an earlier statement,
- versions created by the current statement,
- versions deleted/superseded by an earlier statement,
- versions deleted/superseded by the current statement.

Command IDs therefore preserve statement ordering within one transaction rather than collapsing all self-created/self-deleted versions into one state.

## 9.7 Snapshot representation

A snapshot contains:

```text
TxnId xmin
TxnId xmax
sorted vector<TxnId> active
TxnId owner_txn_id
CommandId command_id
```

### 9.7.1 xmax

At capture:

```text
xmax =
    next normal transaction ID that has not yet been assigned
```

A normal transaction satisfying:

```text
txn_id >= snapshot.xmax
```

started too late to be visible to that snapshot.

### 9.7.2 active

`active` contains **other** normal transactions that were nonterminal for snapshot purposes at capture and satisfy:

```text
txn_id < snapshot.xmax
txn_id != snapshot.owner_txn_id
```

The owner transaction is deliberately excluded from `active`.

Self-visibility is handled only by the explicit:

```text
xmin/xmax owner comparison
+
cmin/cmax command rules
```

rather than by active-set membership.

A transaction remains snapshot-active until terminal outcome publication removes it from the active registry. Therefore snapshot-active runtime states include the nonterminal lifecycle states:

```text
ACTIVE
MUST_ABORT
COMMITTING
ABORTING
```

as applicable at the capture instant.

A transaction represented in `active` remains invisible to this snapshot even if it commits later.

The `active` representation is a sorted vector. Membership testing MAY use binary search or another semantically equivalent lookup over that vector; it MUST preserve the captured membership, owner exclusion, snapshot stability, and sorted order used by §9.7.3's `xmin` derivation.

High-concurrency alternative snapshot active-set representations are deferred from the architecture baseline.

### 9.7.3 xmin

`xmin` is derived exactly from the snapshot's **other-transaction** active set:

```text
if active is non-empty:
    xmin = active[0]   // sorted minimum
else:
    xmin = xmax
```

The owner TxnId does not reduce `xmin`.

This separation is intentional.

For example, a READ COMMITTED transaction that is old but currently executing a statement with no older competing active transaction does not pin the vacuum horizon merely because its own TxnId is small.

The authoritative vacuum horizon remains the registry of active SQL snapshots, not the existence or age of transactions themselves.

### 9.7.4 owner and command

```text
owner_txn_id
```

identifies the transaction evaluating visibility.

```text
command_id
```

captures the statement-order boundary used by same-transaction `cmin/cmax` visibility.

## 9.8 Snapshot capture synchronization

The transaction manager maintains conceptually:

```text
next_txn_id
active transaction registry
```

Snapshot capture and transaction registration use one short synchronization protocol that atomically observes:

```text
transaction-ID high-water mark
+
active transaction IDs below that high-water mark
```

The protocol MUST prevent a transaction from becoming ambiguously active/inactive relative to a snapshot that has already chosen `xmax`.

For a write-capable normal transaction, registration is one conceptual publication that:

```text
assigns its stable TxnId
registers it in the active transaction registry
registers write-status dependency(TxnId)
```

The transaction is not admitted between these effects. This registration publication and
§14.17.1 cutoff candidate calculation/publication share a gap-free ordering relationship:

```text
transaction registration linearizes first:
    its write-status dependency constrains the cutoff

cutoff C linearizes first:
    the subsequently registered transaction is assigned a normal TxnId >= C
    and registers that dependency before admission
```

Thus no assigned-but-unregistered write-capable TxnId may fall below an authoritative
cutoff. This is an architectural linearization requirement, not a required mutex or
container. It does not change snapshot `xmin`, whose owner-exclusion and active-set meaning
remain §9.7.3. The TxnId is stable once this registration publication assigns it; a
below-cutoff candidate is not issued, no assigned TxnId is replaced, and retired status is
not resurrected.

The snapshot/registry synchronization is held only for capture/registration bookkeeping.

It MUST NOT be held while executing the query or statement.

## 9.9 READ COMMITTED snapshot lifetime

At the beginning of each SQL statement in a READ COMMITTED transaction:

```text
capture a new statement snapshot
register it as active
```

The statement uses that one stable snapshot for its full execution attempt.

At statement completion while the transaction remains usable:

```text
unregister the statement snapshot
consume current_command_id
```

This applies to both `SUCCESS` and `FAILED_TRANSACTION_REMAINS_ACTIVE`. The next READ COMMITTED statement captures a fresh statement snapshot and receives the next CommandId; it never reuses the failed statement's snapshot or CommandId.

Two statements in the same READ COMMITTED transaction may therefore observe different committed database states.

If an internal retry is permitted by the exact §15.7 boundary, the abandoned attempt unregisters its old statement snapshot and the retry captures a fresh READ COMMITTED statement snapshot.

If the conflict occurs after the attempt has installed a persistent statement write, §15.7 requires transaction abort rather than same-transaction statement restart.

## 9.10 REPEATABLE READ snapshot lifetime

For REPEATABLE READ, capture the transaction snapshot at the beginning of the first ordinary statement.

Reuse that snapshot for later statements.

The fixed transaction snapshot retains its transaction-ID visibility horizon, while:

```text
snapshot.command_id
```

is updated for each statement so same-transaction command visibility remains correct.

The transaction snapshot remains registered until transaction end.

A long-running REPEATABLE READ transaction may therefore pin the global vacuum horizon.

## 9.11 Transaction-status store

Persistent transaction outcome metadata lives in:

```text
txn_status.dat
```

The file is page based:

```text
page 0       FileSuperblock with FileKind::TXN_STATUS
page 1..N    TXN_STATUS pages
```

The persisted file-kind code is:

```text
FileKind::TXN_STATUS = 5
```

`FileSuperblock.object_id` is:

```text
0
```

for this singleton database-wide file.

The persistent two-bit status codes are:

| Two-bit code | State | meaning |
|---:|---|---|
| `00` | `INVALID` | no terminal outcome recorded in this slot |
| `01` | `COMMITTED` | terminal committed outcome |
| `10` | `ABORTED` | terminal aborted outcome |
| `11` | `RESERVED` | recognized nonterminal/reserved-format marker |

`IN_PROGRESS` remains runtime state represented by the active transaction registry; the architecture does not require a durable status-page write at every `BEGIN`.

### 9.11.1 RESERVED semantics

`RESERVED` is a legal decoded bit pattern but is **not emitted by the normal transaction-begin path**.

All four two-bit patterns are KNOWN under §4.14.4. `RESERVED` is therefore interpreted with this subsection's nonterminal semantics, not rejected as an unknown future enum and not confused with REDIRECT_RESERVED.

The normal execution path uses:

```text
active registry
```

for in-progress transactions and writes a terminal status only when COMMITTED/ABORTED is published.

Therefore correctness does not require materializing one `RESERVED` status for every TxnId covered by the durable block reservation.

If `RESERVED` is encountered:

- it MUST NOT be treated as COMMITTED,
- it MUST NOT be treated as ABORTED,
- while the referenced transaction is active, the active registry still wins,
- after completed recovery, a status-dependent persistent correctness object that references a normal TxnId whose only non-runtime status is `INVALID` or `RESERVED` indicates an invariant failure/corruption unless recovery has first classified and published the transaction's terminal outcome.

Assigning an operational writer for the `RESERVED` code requires an architecture revision and must preserve its nonterminal meaning.

## 9.12 Transaction-status page capacity

A transaction-status page uses exactly the normal 32-byte common page header and no additional page-specific header:

```text
page_type         = TXN_STATUS
format_version    = 1
header_size       = 32
common flags      = 0
common reserved16 = 0
page_no           = actual status-file PageNo
```

The persisted page-type code is:

```text
PageType::TXN_STATUS = 7
```

Therefore:

```text
status payload bytes =
    PAGE_SIZE - 32
    = 8160
```

At:

```text
4 two-bit status entries per byte
```

one status page represents exactly:

```text
8160 * 4
= 32,640 normal transaction IDs
```

Reserved TxnIds:

```text
0 = INVALID_TXN_ID
1 = FROZEN_TXN_ID
```

do **not** consume ordinary status entries.

For a normal TxnId:

```text
txn_id >= FIRST_NORMAL_TXN_ID
```

define:

```text
ordinal =
    txn_id - FIRST_NORMAL_TXN_ID

status_page_no =
    1 + ordinal / 32640

entry_in_page =
    ordinal % 32640

payload_byte_index =
    entry_in_page / 4

two_bit_index =
    entry_in_page % 4

page_byte_offset =
    32 + payload_byte_index

bit_shift =
    2 * two_bit_index
```

The two-bit field is packed LSB-first within the payload byte:

```text
mask = 0x3 << bit_shift
```

Extraction is:

```text
code =
    (page[page_byte_offset] >> bit_shift) & 0x3
```

Updating one entry replaces only those two bits.

All mapping arithmetic MUST be checked before physical page access.

A freshly initialized TXN_STATUS page is zero-filled, so every unmodified status slot begins canonically as:

```text
INVALID = 00
```

A TXN_STATUS decoder MUST reject:

- wrong `page_type`,
- unsupported `format_version`,
- `header_size != 32`,
- nonzero common flags,
- nonzero common `reserved16`,
- mismatched embedded `page_no`,
- checksum failure when ordinary-page checksum verification is active.

The complete status-page L1/high-water and recovery-publication requirements are §4.13.6. In particular, all assigned two-bit codes are structurally decodable, while the protocols in §§12.10.5 and 13.13.2 together with transaction-status semantics—not raw page decoding—decide whether a nonterminal code is valid for an authoritative transaction reference.

The page's common `page_lsn` records the newest WAL record physically reflected in the page. After a successful terminal update this is that update's `TXN_COMMIT`/`TXN_ABORT` LSN. During recovery it may temporarily be a system `PAGE_INIT`/`PAGE_IMAGE` LSN before the later semantic terminal record is applied, as defined by §12.10.5.

An in-memory hash table MUST NOT define the persistent status format.

## 9.13 Transaction-status lookup

Status lookup follows this semantic order:

```text
if txn_id == FROZEN_TXN_ID:
    COMMITTED

else if txn_id == current transaction:
    SELF

else if runtime terminal-outcome cache contains txn_id:
    COMMITTED or ABORTED

else if active transaction registry contains txn_id as nonterminal:
    IN_PROGRESS

else if txn_id < txn_status_reclaim_before:
    RETIRED

else:
    consult cached/persistent transaction status
```

The runtime terminal-outcome cache dominates a stale/nonremoved active-registry entry.

`RETIRED` is a runtime lookup result, not a persisted two-bit state. It means the architecture has durably proven that no valid persistent correctness object still requires the transaction outcome for that old status range.

A write-status dependency does not add a lookup result or override this precedence. It
prevents §14.17.1 from legally advancing the cutoff past a write-capable transaction while
that transaction can still create a persistent reference to its own TxnId.

Persisted:

```text
INVALID
RESERVED
```

are nonterminal results and MUST NOT be promoted to committed/aborted by guesswork.

`SELF`, `IN_PROGRESS`, and `RETIRED` are lookup results, not additional persisted two-bit codes.

A persistent tuple field or other correctness object whose interpretation requires
the status of a normal TxnId which is:

```text
not active
not terminal
and not legally replaceable by FROZEN_TXN_ID under the retained-status cutoff
```

after completed recovery indicates corruption or an invariant failure.

The engine MUST NOT silently treat unknown or retired history as committed.

This rule applies to a field whose interpretation still requires `Status(txn_id)`.
It does not turn every persisted numeric TxnId into a status-retention reference.
In particular, a globally published §34.3.1 `StatsVersion.stats_txn_id` is opaque
generation identity and is never passed to this lookup merely because it contains
an old TxnId.

## 9.14 Terminal status publication boundary

TransactionManager owns one short synchronization domain used by:

```text
snapshot capture
active-registry membership
runtime terminal-outcome publication
```

This creates one linearization point between nonterminal and terminal transaction state.

### 9.14.1 Runtime terminal publication

After the required terminal WAL/status-page prerequisites have been satisfied, terminal publication performs atomically under the transaction-registry synchronization:

```text
1. publish COMMITTED or ABORTED in the runtime terminal-outcome cache
2. change the transaction runtime state to the terminal state
3. remove/mark it non-active for future snapshot capture
```

After this linearization point:

- status lookup returns the terminal outcome even if cleanup of a stale registry object is still pending,
- a newly captured snapshot does not place the transaction in `active`,
- the terminal outcome cannot revert.

Transaction-lifetime logical locks and the transaction-owned SchemaLock,
TableWriterGate, and object-publication modes MUST NOT become acquirable by
another transaction until this terminal publication point has completed.

A write-capable transaction's write-status dependency is likewise retained through this
terminal publication point. Terminal publication alone does not release it: the COMMIT
path releases it during C5 and the ABORT path during A3 together with the transaction's
other retained correctness resources.

Thus another writer observes either:

```text
nonterminal transaction + its logical locks still protect the conflict
```

or:

```text
terminal transaction outcome
```

never `IN_PROGRESS` after the conflicting transaction's locks have been released.

### 9.14.2 Commit

The integrated COMMIT sequence is canonical in §15.5.

This chapter owns the transaction-lifecycle requirement that terminal COMMITTED state is published through §9.14.1's linearization point only after the required durable commit WAL exists. The transaction-status page remains NO-FORCE; durable commit WAL is authoritative after crash.

The physical transaction-status page mutation follows the full-image/terminal-record protocol in §12.10.5. Installing the status bits in a resident page is not the runtime terminal-publication linearization point.

Durable `TXN_COMMIT` is nevertheless already the irreversible semantic outcome. Failure before §9.14.1 completes cannot publish ABORTED; it follows §39.1.5's committed-but-runtime-noncontinuable/client-uncertain rule.

### 9.14.3 Abort

The integrated ABORT sequence is canonical in §15.6.

This chapter owns the transaction-lifecycle requirement that ABORTED publication uses §9.14.1's terminal linearization point before transaction-lifetime locks are released.

An ordinary abort does not require immediate abort-WAL `fdatasync` merely to acknowledge abort. If abort WAL is lost in a crash, recovery treats the transaction as a loser and establishes ABORTED again.

The physical transaction-status page mutation follows §12.10.5 even when the terminal record is a recovery-generated `TXN_ABORT`.

Ordinary user abort does not physically restore heap/index bytes.

Known/uncertain abort-publication failures and mandatory cleanup from `MUST_ABORT` follow §39.1.6; no abort failure restores commit eligibility or permits early lock release.

## 9.15 Read-only transactions

An explicitly read-only transaction is classified as such at transaction registration and
is forbidden from publishing its TxnId in persistent `xmin`, `xmax`, or other correctness
metadata that requires its terminal outcome. It cannot be silently upgraded to
write-capable. Because it cannot create such a future status dependency, it does not
register a write-status dependency for its own TxnId.

Such a transaction:

```text
does not require a transaction-commit WAL record
does not require a terminal transaction-status entry
```

It still:

- owns a TxnId,
- participates in the active transaction registry,
- owns/registers snapshots according to its isolation level,
- may pin the vacuum horizon.

Its registered SQL snapshots may constrain logical history independently of the omitted
write-status dependency. Conversely, a write-capable READ COMMITTED transaction remains
protected by its write-status dependency between statements even when it has no registered
SQL snapshot.

Durable TxnId block reservation prevents harmful TxnId reuse even when a read-only transaction leaves no WAL/status record.

## 9.16 Transaction/snapshot invariants

1. `INVALID_TXN_ID=0`, `FROZEN_TXN_ID=1`, and normal TxnIds begin at `2`.
2. Normal TxnIds are monotonically allocated and never reused after durable reservation.
3. A new TxnId reservation is not consumed before its global reservation metadata is durable.
4. A crash may create gaps in the TxnId sequence.
5. `FROZEN_TXN_ID` is always treated as committed for visibility.
6. Isolation levels are READ COMMITTED and REPEATABLE READ; REPEATABLE READ is snapshot isolation, not serializable isolation.
7. `CommandId{0}` is valid; one admitted statement and all of its internal retries use one CommandId, and that ID is consumed on every success or failure rather than reused.
8. Snapshot capture atomically observes the TxnId high-water mark and relevant nonterminal active registry.
9. `snapshot.active` excludes `owner_txn_id`; `snapshot.xmin` is the smallest other active TxnId, or `xmax` when none exists.
10. One READ COMMITTED statement attempt uses one stable snapshot.
11. REPEATABLE READ reuses one transaction snapshot and changes only its command boundary for later statements.
12. Runtime terminal outcome dominates nonterminal/active lookup.
13. Terminal publication and snapshot-active removal have one synchronization linearization point.
14. Logical transaction locks and transaction-owned SchemaLock/TableWriterGate/object-publication modes are not released before terminal publication has linearized.
15. A transaction is not published COMMITTED before its commit WAL is durable.
16. Read-only transactions may end without terminal persistent status because they create no persistent TxnId references.
17. A referenced normal TxnId with no legal active/terminal/frozen/retired interpretation is an invariant failure, not implicitly committed.
18. `MUST_ABORT` is runtime-only, remains nonterminal for visibility, forbids ordinary statements/COMMIT, and can transition only toward ABORTED.
19. A publication-authorizing commit-record append makes COMMIT uncancellable; durable commit makes the semantic outcome irreversibly COMMITTED.
20. Every write-capable normal transaction registers its own write-status dependency as part of gap-free transaction registration and retains it through C5 or A3.
21. An explicitly read-only transaction has no write-status dependency and cannot publish persistent correctness metadata containing its TxnId.
22. Releasing a write-status dependency removes only the possibility of future self-TxnId publication; existing persistent references retain status history until Chapter 14 proves them status-independent.
