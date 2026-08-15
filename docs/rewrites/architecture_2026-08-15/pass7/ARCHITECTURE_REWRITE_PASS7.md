# Rewrite Pass 7 — Transactions, Snapshots, MVCC, and Logical Locking

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `180..214`
- processed source lines: `6168..7282`
- legacy architecture modified: **no**
- production code modified: **no**
- WAL record/layout, checkpoint, and recovery body from §215+ migrated: **no**

Pass 7 replaces the Pass-1 transaction/MVCC/locking summaries with canonical Chapters 9–11.

## Transaction identity and lifecycle

Canonical transaction IDs:

```text
INVALID_TXN_ID      = 0
FROZEN_TXN_ID       = 1
FIRST_NORMAL_TXN_ID = 2
```

Normal IDs are 64-bit, monotonically allocated, and never reused after durable reservation.

The initial reservation block is exactly:

```text
1,048,576 TxnIds
```

A newly reserved block cannot be consumed before its reservation metadata is durable.

Crash gaps are allowed.

The exact inclusive/exclusive meaning of `reserved_txn_id_end` is not specified by the source and is now R-023.

The transaction object/state model preserves:

```text
ACTIVE
COMMITTING
COMMITTED
ABORTING
ABORTED
```

with monotonic state movement and terminal COMMITTED/ABORTED states.

## Isolation and command IDs

V1:

```text
default = READ COMMITTED
REPEATABLE READ = snapshot isolation
not serializable
```

Command IDs begin at zero and advance exactly once after each completed SQL statement.

Tuple `cmin/cmax` preserves same-transaction statement ordering.

## Snapshots

Canonical fields:

```text
xmin
xmax
sorted active TxnIds
owner_txn_id
command_id
```

`xmax` is the next unassigned TxnId at capture.

Transactions `>= xmax` are too new.

Active-set members remain invisible even if they commit later.

Snapshot capture atomically observes high-water mark + relevant active registry under short synchronization.

READ COMMITTED captures/registers one stable statement snapshot per statement execution attempt.

REPEATABLE READ captures one transaction snapshot on the first ordinary statement, reuses its transaction visibility horizon, updates only command visibility, and keeps it registered through transaction end.

The source does not precisely specify whether the owner is a member of the stored active vector / exact xmin derivation, so Pass 7 records R-024 rather than guessing.

## Transaction status

Canonical semantic file:

```text
txn_status.dat

page 0     superblock
page 1..N  transaction-status pages
```

Status state names:

```text
INVALID
COMMITTED
ABORTED
RESERVED
```

IN_PROGRESS is primarily runtime active-registry state.

Page geometry:

```text
8192 total
32 common header
8160 payload
4 two-bit states / byte
32,640 TxnIds / page
```

Lookup order:

```text
FROZEN -> COMMITTED
self   -> SELF
active -> IN_PROGRESS
else   -> cached/persistent terminal status
```

A referenced normal TxnId that is neither active nor terminal after recovery is corruption/invariant failure, not implicitly committed.

The source does not define FileKind/PageType/two-bit codes/exact mapping/RESERVED semantics, recorded as R-022.

## Commit/abort publication boundary

Pass 7 preserves only the status-publication semantics from §§194–195:

```text
COMMIT:
    commit WAL durable
    -> publish COMMITTED
    -> release logical locks
    -> unregister snapshots/active state
    -> success
```

Status-page data itself is NO-FORCE and can be reconstructed from WAL later.

Abort publishes ABORTED, releases locks/unregisters, and does not require immediate abort-WAL fsync.

The exact WAL record format, group commit, checkpoint, and recovery algorithm remain outside this pass.

## Read-only transactions

Read-only transactions still receive TxnIds and snapshots and may pin vacuum.

They need neither commit WAL nor terminal status entries because they create no persistent TxnId references.

## No physical user-DML undo

Chapter 10 now canonically expresses the later refinement:

```text
aborted INSERT:
    xmin aborted -> invisible

aborted DELETE:
    xmax aborted -> old version live

aborted UPDATE:
    old xmax aborted -> old live
    new xmin aborted -> new invisible

physical heap/index garbage:
    may remain for vacuum
```

No ordinary user heap/index byte restoration is required.

## Exact MVCC visibility

Creator rule is now byte/algorithm exact from §197:

```text
FROZEN -> visible

self:
    cmin < command_id

other:
    COMMITTED
    and xmin < snapshot.xmax
    and xmin not in snapshot.active
```

Otherwise creator invisible.

Deleter rule from §198 is also exact:

```text
xmax=0
    visible

self:
    cmax < command_id -> invisible
    otherwise visible

ABORTED other:
    visible

IN_PROGRESS other:
    visible

COMMITTED other:
    invisible only when
        xmax < snapshot.xmax
        and xmax not in snapshot.active
```

Reads do not take tuple locks to evaluate visibility.

## Hint cleanup

Vacuum/maintenance may clean:

```text
aborted xmax:
    xmax=0
    cmax=0

ancient committed creator:
    xmin=FROZEN_TXN_ID
    cmin=0
```

when later freeze rules permit.

These are WAL-protected physical page changes once WAL is active.

## Logical locking

V1 LockManager has only exclusive:

```text
TUPLE_WRITE
UNIQUE_KEY
```

Tuple-write key:

```text
(TableId, physical target RID)
```

Unique-key key:

```text
(IndexId, full encoded user-key bytes)
```

Hashing may select a shard but full-key comparison remains mandatory.

No logical lock wait occurs while holding heap/B+/buffer/structural page latches.

## Write conflicts

UPDATE/DELETE:

```text
candidate
-> release physical latches
-> acquire tuple-write lock
-> refetch/revalidate
-> inspect xmax
```

Aborted prior xmax is ineffective.

In-progress races wait/retry outside physical latches.

Committed competing writers cause:

```text
READ COMMITTED:
    fresh-snapshot statement restart

REPEATABLE READ:
    serialization/write-conflict abort
```

This combination implements lost-update prevention.

Snapshot-isolation write skew across distinct rows remains possible.

## Unique-key conflicts

Fully non-NULL unique keys acquire transaction-lifetime unique locks.

INSERT, UPDATE, and DELETE may all need the lock because another transaction must not treat an unresolved key removal/creation as final.

Multiple known unique keys are acquired in deterministic encoded-byte order when practical.

After locking, uniqueness scans every physical B+ duplicate, fetches heap versions, and evaluates **current transactional state**, not only historical snapshot visibility.

NULL-containing unique keys skip duplicate rejection in v1.

## Lock table and deadlocks

Initial LockManager:

```text
hash map<LockKey,LockQueue>
mutex
current owner
FIFO waiters
exclusive-only compatibility
```

Wait-for edges are:

```text
waiter -> current owner
```

Initial deadlock victim:

```text
youngest transaction
=
highest TxnId in cycle
```

Victim cancellation wakes blocked waits and follows normal abort/status publication.

Timeouts remain diagnostic/fallback rather than the primary correctness mechanism.

## Later-source consistency check

Later legacy write-protocol sections 275–280 and the later transaction invariant list were inspected only for refinement conflicts.

They confirm, rather than contradict:

- no physical user-DML undo,
- tuple/unique locks held to transaction end,
- READ COMMITTED statement retry,
- REPEATABLE READ conflict abort,
- no logical-lock waits while holding physical latches.

Their detailed WAL/write/retry-output protocols were not migrated and remain scheduled for later passes.

## New architecture issues

### R-022 — transaction-status persistent format

Exact FileKind/PageType/two-bit codes, RESERVED semantics, and mapping arithmetic are not defined by the source.

### R-023 — reservation boundary convention

`reserved_txn_id_end` is required but not defined as inclusive versus exclusive.

### R-024 — snapshot owner/xmin convention

The source does not lock whether owner_txn_id is stored in snapshot.active or the exact xmin derivation that follows.

No values were guessed.

## Coverage result

```text
legacy §§0..214     migrated / explicitly disposed
legacy §§215..725   pending
```

All 35 Pass-7 sections are complete.

No §215+ row was marked migrated.

## Validation

Mechanical validation confirmed:

- pinned legacy SHA-256 unchanged,
- §180 begins at line 6168,
- §215 begins at line 7283,
- all source sections 180..214 exist,
- every coverage row through §214 is non-PENDING,
- every row from §215 onward remains PENDING,
- Chapters 9, 10, and 11 each occur exactly once,
- TxnId constants/block size/isolation/command/snapshot/status-page capacities are present,
- creator and deleter visibility inequalities are present,
- tuple/unique lock identities and transaction-duration rules are present,
- deadlock victim policy is present,
- no WAL record layout/checkpoint/recovery section from §215+ was imported,
- legacy ARCHITECTURE.md was not modified,
- production code was not modified.

## Pass 7 exit status

**COMPLETE WITH THREE EXPLICIT ARCHITECTURE GAPS (R-022, R-023, R-024).**

Before implementing the transaction-status store / TxnId allocator / SnapshotManager, these three should be resolved.

Rewrite Pass 8 may proceed structurally if the project intentionally carries those issues in the register, but resolving them before Pass 8 would produce a cleaner durability contract.
