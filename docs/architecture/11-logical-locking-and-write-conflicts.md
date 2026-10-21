# 11. Logical Locking and Write Conflicts

## 11.1 LockManager scope

The architecture uses exclusive logical transaction locks for exactly two conflict families:

```text
TUPLE_WRITE
UNIQUE_KEY
```

Readers use MVCC and do not acquire shared tuple locks merely to read visible versions.

Deferred lock families include:

```text
TABLE
INTENTION
SCHEMA
KEY_RANGE
```

The physical B+ tree/heap latch architecture is separate from these transaction-lifetime logical locks.

`SchemaLock`, `TableWriterGate`, and the object-publication gate are separate
coordination domains rather than additional LockManager key families. They
nevertheless participate in §11.13's same transaction wait-for graph whenever
one transaction blocks on another; separate storage does not permit separate
deadlock visibility.

## 11.2 Tuple-write lock identity

A tuple write lock is keyed by: `(TableId, physical target RID)`.

The target RID is the currently visible physical tuple version that the transaction intends to:

```text
UPDATE
or
DELETE
```

Two writers that begin from the same visible physical version therefore contend on the same logical lock. Because UPDATE creates a new RID, a writer that waits for the old version's logical lock MUST re-fetch and revalidate the target after lock acquisition.

Every live request for `TUPLE_WRITE(TableId,RID)` is also a runtime physical
RID-retention claim for that exact key. A request is live while it is queued and still
grant-eligible or while it is granted. While any such request for the old identity exists,
Chapter 14 MUST NOT publish `DEAD -> UNUSED`, reuse the slot, or otherwise let the same RID
identify a different logical tuple/version. This consequence does not change the exact
`(TableId,RID)` key, add a RID/lock-key generation, or make the lock a visibility claim.

## 11.3 Lock/latch separation rule

A transaction MUST NOT wait for a logical LockManager lock while holding:

- a heap page latch,
- a B+ tree page latch,
- a buffer-frame/page latch,
- a B+ structural latch/guard whose lifetime blocks physical structure progress.

The required shape is:

```text
identify candidate RID/key
    ↓
release short-lived page/index latches
    ↓
acquire/wait for logical lock
    ↓
re-fetch candidate
    ↓
re-check identity, visibility, header, predicate, key state
    ↓
perform mutation using short-lived physical latches
```

This prevents transaction-duration waits from coupling to microsecond-scale page-structure latches.

For a `TUPLE_WRITE` target, the caller retains the discovery `ReadEpochGuard` after
releasing the short physical latches and through request registration. Registration either
immediately grants the request or places it in the canonical grant-eligible queue before
the epoch is released. No blocking LockManager wait retains the epoch. If registration
fails before the claim becomes live, the caller still owns the read epoch and MUST fail or
retry through the existing statement rules; it cannot release the epoch and continue or
wait without a claim. Thus physical identity protection passes gap-free from the read epoch
to the live request. This specialization does not change `UNIQUE_KEY` acquisition or its
full post-wait candidate re-probe.

The rule composes with the B+ tree's own page-latch ordering and optimistic root validation: LockManager waits occur outside those physical-latch critical sections.

## 11.4 UPDATE / DELETE write-conflict protocol

For a candidate visible physical tuple version `R`:

1. retain `R` under the discovery read epoch while releasing short-lived heap/index latches,
2. establish the exclusive `TUPLE_WRITE(TableId,RID)` request on `R` as a live
   RID-retention claim,
3. release the read epoch before any blocking wait,
4. after grant, re-fetch `R`,
5. re-check tuple identity and visibility,
6. inspect the current `xmax`,
7. apply the cases below.

### 11.4.1 No competing xmax

If: `xmax == INVALID_TXN_ID` the write may proceed subject to its other constraints.

### 11.4.2 xmax belongs to self

Handle the target through same-transaction command semantics. The transaction MUST NOT treat this as a second independent competing writer.

### 11.4.3 xmax belongs to an aborted transaction

The prior delete/update is logically ineffective.

The new writer MAY overwrite the aborted:

```text
xmax
cmax
```

metadata while installing its own write semantics.

### 11.4.4 xmax belongs to an in-progress transaction

The tuple-write lock protocol should ordinarily already serialize competing writers to the same visible version.

If an in-progress competing `xmax` is discovered because of a race:

```text
release physical page/index latches
wait/retry through the logical-lock protocol
re-fetch/revalidate
```

Do not wait while retaining physical page latches.

If the caller is still queued for `TUPLE_WRITE`, that queued request retains RID identity
during the wait. If it already owns `TUPLE_WRITE` and must wait/recheck transaction state,
the granted ownership remains the retention claim. Neither case retains a read epoch across
the blocking interval, and both still perform the required post-wake target revalidation.

### 11.4.5 xmax belongs to a committed competing updater

The response is isolation-level-specific. READ COMMITTED follows §11.5. REPEATABLE READ follows §11.6.

## 11.5 READ COMMITTED write conflict

If a READ COMMITTED writer waited and then discovers that another transaction committed an UPDATE/DELETE of its original target, it MUST NOT blindly modify the stale physical version. Apply the §15.7 retry boundary.

Before the statement attempt has installed any persistent write, the engine may:

```text
restart the affected candidate search
using a fresh READ COMMITTED statement snapshot
```

and re-evaluate the current row version, predicates, index conditions, and generated values. After the attempt has installed any persistent write, the architecture MUST abort the transaction instead of restarting that statement inside the same TxnId.

This distinction is required because the architecture deliberately has no statement-level physical undo/subtransaction state.

## 11.6 REPEATABLE READ write conflict

Under REPEATABLE READ / snapshot isolation, if another transaction committed a change to the target after the current transaction's fixed snapshot: `abort with a serialization/write-conflict error`.

This is first-updater-wins behavior. The writer MUST NOT silently follow a newer version that its fixed transaction snapshot could not see.

## 11.7 Lost-update prevention

Lost-update prevention relies on the combination:

```text
exclusive tuple-write locks
+
revalidation after lock wait/acquisition
+
READ COMMITTED statement restart
+
REPEATABLE READ serialization/write-conflict abort
```

This combination MUST prevent two concurrent writers from silently overwriting each other's work on one logical target version. Snapshot-isolation write skew across different rows remains possible. That behavior is one reason full SERIALIZABLE isolation is deferred to SSI/predicate/key-range mechanisms.

## 11.8 Unique-key lock identity

For a fully non-NULL SQL unique key:

```text
UniqueLockKey =
    (IndexId, full encoded user-key bytes)
```

The encoded user-key bytes are the canonical B+ user-key representation from Chapter 8. A hash MAY select the LockManager shard/bucket. Hash collisions MUST be resolved by comparison of the complete lock key. Uniqueness MUST NOT depend on a 64-bit hash alone.

The user-key bytes exclude RID. They encode every key component in catalog order through the exact Chapter-8 `IndexKeyCodec`, so lock equality and index user-key equality are identical for:

```text
BOOLEAN
INT32
INT64
FLOAT64, including canonical NaN and signed-zero handling
VARCHAR under binary collation
DATE
TIMESTAMP
composite keys
```

Two semantically equal fully non-NULL keys for one `IndexId` MUST therefore name the same logical lock even when their source expressions or physical tuple bytes differ. Composite lock identity is component-wise semantic equality after canonical encoding. A NULL-containing UNIQUE key does not enter the duplicate-conflict domain and has no duplicate-prevention lock identity; PRIMARY KEY rejects NULL before this point.

## 11.9 Unique-key lock protocol

Any DML operation that may create or remove a **fully non-NULL** unique key acquires the corresponding exclusive: `UNIQUE_KEY(IndexId, encoded user-key)`.

Examples include:

```text
INSERT unique K
UPDATE old K -> new K
DELETE unique K
```

Unique-key locks remain held through transaction terminal publication. When an UPDATE needs both old and new unique-key locks and both keys are known before waiting, acquire them in the deterministic total order below to reduce deadlock probability.

The deadlock detector remains the correctness fallback. For SQL UNIQUE semantics, if **any** indexed key component is NULL: `duplicate rejection is skipped` and no duplicate-prevention unique-key lock is required for that NULL-containing key merely to enforce SQL uniqueness.

The physical B+ entries still exist and remain ordered normally.

The required check/publish order is:

```text
INSERT:
    evaluate/type-check key values and enforce NOT NULL

UPDATE/DELETE:
    acquire TUPLE_WRITE for the target under §11.4
    re-fetch/revalidate the target, derive old key values, and evaluate/type-check
    any new key values needed by the row operation
    release every physical latch retained for that derivation

then across the complete staged statement domain:
    encode every complete user key canonically
    acquire/wait for the complete required UNIQUE_KEY set without physical
        latches and in the statement-wide order below
    after all grants, re-fetch/revalidate every affected UPDATE/DELETE target
    scan/recheck each complete physical candidate range under §11.10
    establish exact same-statement pending-owner conflicts
    only after every ordinary UNIQUE candidate enters §21.16.1 closure may any
        heap/index mutation publish
```

Probing before lock acquisition is only an advisory optimization. It never authorizes publication, and the complete candidate scan MUST be repeated after the lock is granted. This closes the check-then-insert race.

For one user-DML statement attempt, §§15.1.3 and 21.16.1 require the complete
finite set of UNIQUE_KEY locks needed by the staged candidate domain to be
known and acquired before the statement's first persistent user-DML write.
The locks are acquired in the total order: `(IndexId, lexicographic encoded user-key bytes)`.

An UPDATE includes both old and new fully non-NULL keys when they differ;
DELETE includes the old key. Candidate evaluation may discover those keys in
any physical order, but publication waits until the complete lock set has been
acquired and every required post-wait revalidation/current-state check has
succeeded. The deadlock detector remains the correctness fallback; a deadlock
is a dynamic failure and does not enter Chapter 21's ordinary-candidate order.
This complete prepublication acquisition intentionally may increase lock
footprint, lock duration, contention, and latency to the first write. Those
costs do not permit incremental publication, key omission, early lock release,
or approximate conflict detection.

## 11.10 Unique-check semantics

This section is the canonical UNIQUE/PRIMARY KEY current-state conflict contract. Constraints are immediate and non-deferrable.

### 11.10.1 Three distinct decisions

The implementation keeps three questions separate:

```text
ordinary MVCC visibility
    whether a tuple contributes to a query under one Snapshot

physical index candidacy
    whether an encoded (user_key,RID) entry must be heap-rechecked

UNIQUE conflict ownership
    whether current transaction/version state constitutes another owner of K
```

`Visible(snapshot, tuple)` is not the UNIQUE predicate. It deliberately hides current-command inserts, may retain a row deleted by the current command, and may not see a transaction that committed after a READ COMMITTED statement snapshot. Using it would permit same-statement duplicates or two committed owners.

The B+ tree is only a candidate source. Its physical key is `(user_key,RID)`, and one user-key range may legally contain current, aborted, superseded, and stale versions. Presence in that range is neither semantic proof of a conflict nor permission to ignore heap/current-status recheck.

In an architecture-valid published index, every transaction operation that may become a logical owner installs its required physical entry before statement/transaction success. The heap-before-index interval in §15.2/§15.3 remains protected by the same UNIQUE_KEY lock and cannot be observed as permission for a competing owner. A missing required entry is index corruption/consistency failure, not a second authoritative source of “no conflict.”

### 11.10.2 Key domain and NULL rule

For ordinary UNIQUE, duplicate rejection applies only when **every** key component is non-NULL. If any component is NULL:

```text
UniqueConflicts(K, ...) = NO_CONFLICT
no duplicate-prevention UNIQUE_KEY lock is acquired for K
```

The NULL-containing physical entry is still inserted normally. For fully non-NULL keys, equality is exact equality of the canonical Chapter-8 encoded user-key bytes and therefore follows §11.8 for every supported scalar/composite key.

PRIMARY KEY first enforces NOT NULL on every component and then applies this identical fully non-NULL conflict predicate, current-command handling, self-exclusion, and lock protocol. It has no NULL shortcut.

### 11.10.3 Predicate inputs and operation context

Conceptually, after the checking transaction owns `UNIQUE_KEY(IndexId,K)`, every physical candidate is classified by:

```text
UniqueCandidateOutcome(
    candidate_rid,
    candidate_tuple { xmin, cmin, xmax, cmax },
    current Status(xmin/xmax),
    checking_txn_id,
    checking_command_id,
    operation_context)
```

`operation_context` is runtime-only and contains at most:

```text
optional exact excluded_old_rid
optional exact operation_published_new_rid
IndexId and encoded key being checked
```

`excluded_old_rid` is present only for the currently processed UPDATE after exact TUPLE_WRITE revalidation, and only when that old version's unique key is semantically equal to `K`. `operation_published_new_rid` is present only while the same in-process row-mutation owner has continuously retained its operation context and logical locks and recorded that exact `(IndexId,K,RID)` as published; it is not reconstructed from TxnId/CommandId after an error return. Neither field is persisted, inferred from TxnId alone, or shared across rows. No persistent logical-row identifier is introduced.

Before the first persistent user-DML statement write, the final statement
attempt also owns an exact, attempt-local pending-owner relation for every
staged fully non-NULL unique key. Conceptually it records:

```text
(IndexId, full canonical encoded user-key bytes)
    -> prospective candidate occurrence(s) and their diagnostic provenance
```

It is not a persisted index, deferred-constraint state, or physical owner
visible to another transaction. Complete-key equality, not a hash collision,
defines membership. Under the already-acquired UNIQUE_KEY locks, more than one
distinct staged prospective owner of the same key establishes ordinary UNIQUE
candidate(s) for §21.16.1 even though none has yet been published. Candidate
comparison is symmetric: insertion order, worker order, spill order, and an
implementation-chosen representative do not determine the public diagnostic.

The conceptual outcomes are:

| Outcome | Meaning |
|---|---|
| `UNIQUE_CONFLICT` | another current logical owner exists; report UNIQUE/PRIMARY KEY violation |
| `NO_CONFLICT` | the complete candidate range contains no owner after all classifications |
| `WAIT_THEN_RECHECK` | another transaction's terminal outcome is required; publish nothing and restart the whole current-state range check afterward |
| `IGNORE_ABORTED_OR_STALE` | this candidate's creator aborted or its delete/supersession is terminally effective |
| `SELF_EXCLUDED` | this exact RID is the current UPDATE target/replacement authorized by operation context |
| `CORRUPTION_OR_INTERNAL_ERROR` | persistent identity/index/heap/status facts are malformed or an impossible runtime invariant was observed |

`SELF_EXCLUDED` applies to one exact physical RID, never to “any tuple from my TxnId,” any `prev`-chain relative, or any tuple with the same key.

### 11.10.4 Canonical current-owner algorithm

For each distinct physical candidate `(K,RID)`, in this order:

1. Under a §14.6 `ReadEpochGuard`, fetch the referenced NORMAL heap version and validate relation/FileId/RID identity. Re-encoding its indexed values under the historical schema must produce `K`; a dangling entry, reused RID, non-NORMAL target, wrong relation, or key mismatch is corruption.
2. If RID is the exact `excluded_old_rid`, return `SELF_EXCLUDED`. If RID is the exact proven `operation_published_new_rid`, return `SELF_EXCLUDED` for that internal continuation only.
3. Classify the creator:
   - `xmin == FROZEN_TXN_ID` or another transaction currently `COMMITTED`: creator is effective regardless of the checking statement snapshot;
   - `xmin == checking_txn_id`: require `cmin <= checking_command_id`; both an earlier-command and current-command creator are effective;
   - another transaction `ABORTED`: return `IGNORE_ABORTED_OR_STALE` without consulting `xmax`;
   - another transaction nonterminal (`ACTIVE`, `MUST_ABORT`, `COMMITTING`, or `ABORTING` through status lookup): return `WAIT_THEN_RECHECK`;
   - invalid, retired-but-still-referenced, unknown, or otherwise impossible creator state: return `CORRUPTION_OR_INTERNAL_ERROR`.
4. For an effective creator, classify `xmax`:
   - `INVALID_TXN_ID`: return `UNIQUE_CONFLICT`;
   - if `xmin == checking_txn_id` and non-invalid `xmax != checking_txn_id`: return `CORRUPTION_OR_INTERNAL_ERROR`, because another transaction cannot legally delete an uncommitted self-created version;
   - `xmax == checking_txn_id`: require `cmax <= checking_command_id`, and when `xmin == checking_txn_id` also require `cmin <= cmax`; `cmax < checking_command_id` returns `IGNORE_ABORTED_OR_STALE`, while `cmax == checking_command_id` returns `UNIQUE_CONFLICT` unless step 2 already returned `SELF_EXCLUDED` for this exact row operation;
   - another transaction `ABORTED`: its delete is ineffective, so return `UNIQUE_CONFLICT`;
   - another transaction `COMMITTED`: its delete/supersession is effective now, so return `IGNORE_ABORTED_OR_STALE` regardless of snapshot age;
   - another transaction nonterminal: return `WAIT_THEN_RECHECK`; an uncommitted delete never frees the key;
   - `FROZEN_TXN_ID` as `xmax`, a future self `cmin/cmax`, or invalid/unknown status evidence: return `CORRUPTION_OR_INTERNAL_ERROR`.

Provisional §12.12 bytes are not ordinary candidates. They are either unpublished/inaccessible or exactly restored; only §12.12-published tuple/header/index state participates.

The complete physical range is enumerated even after finding a semantic owner so an exact duplicate physical key or dangling/mismatched RID cannot be hidden by early exit. Aggregate outcome precedence is:

```text
any CORRUPTION_OR_INTERNAL_ERROR -> that error
else any UNIQUE_CONFLICT         -> UNIQUE_CONFLICT
else any WAIT_THEN_RECHECK       -> WAIT_THEN_RECHECK
else                              -> NO_CONFLICT
```

`SELF_EXCLUDED` and `IGNORE_ABORTED_OR_STALE` contribute no owner. A wait invalidates the entire scan result and no prior partial classification is reused after wakeup.

### 11.10.5 INSERT/current-state truth table

`C` is the checking CommandId. “Nonterminal other” includes every runtime state that status lookup exposes as in progress.

| Candidate creator | Candidate deleter | Relation to checker | Candidate outcome |
|---|---|---|---|
| committed or frozen | none | other row | `UNIQUE_CONFLICT` |
| committed or frozen | committed other | other row | `IGNORE_ABORTED_OR_STALE` |
| committed or frozen | aborted other | other row | `UNIQUE_CONFLICT` |
| committed or frozen | nonterminal other | other row | `WAIT_THEN_RECHECK`; commit -> ignore, abort -> conflict |
| aborted other | any | other row | `IGNORE_ABORTED_OR_STALE` |
| nonterminal other | any | other row | `WAIT_THEN_RECHECK`; commit -> reclassify, abort -> ignore |
| checking TxnId, `cmin < C` | none | earlier command | `UNIQUE_CONFLICT` |
| checking TxnId, `cmin == C` | none | current command, earlier row | `UNIQUE_CONFLICT` |
| effective creator | checking TxnId, `cmax < C` | self-deleted earlier command | `IGNORE_ABORTED_OR_STALE` |
| effective creator | checking TxnId, `cmax == C` | self-delete/supersession in current command, another row operation | `UNIQUE_CONFLICT` |
| any impossible command/status/RID state | any | any | `CORRUPTION_OR_INTERNAL_ERROR` |

Thus:

```sql
INSERT INTO t(k) VALUES (1), (1);
```

is rejected during prepublication preparation by the exact pending-owner
relation, even though neither current-statement occurrence has a published
`cmin == C` version. Chapter 21 chooses the public diagnostic from the
resulting ordinary candidates; this section does not declare that the first or
second physically visited occurrence wins. An INSERT in a later command of the
same transaction still conflicts with the published current owner unless the
prior owner was deleted/superseded by an earlier command. After `INSERT K;
DELETE row;` in earlier commands, a later INSERT of K is permitted because
`cmax < C`. A current-command self-delete does not free K for a different row
operation in that command.

Every vectorized, batched, spilled, or scalar implementation maintains
equivalent exact pending-owner semantics for the complete staged domain. It
MUST NOT probe each candidate only against the pre-statement index state or
publish one candidate merely to make it visible to another.

### 11.10.6 UPDATE truth table and immediate behavior

| Candidate relation | Exact condition | Candidate outcome |
|---|---|---|
| current UPDATE old target | RID equals revalidated `excluded_old_rid` and old key equals K | `SELF_EXCLUDED` |
| current UPDATE replacement | RID equals exact proven `operation_published_new_rid` | `SELF_EXCLUDED` only for internal continuation |
| same transaction, another row | effective creator, no effective self-delete | `UNIQUE_CONFLICT`, including `cmin == C` |
| same transaction, another row | self-delete/supersession with `cmax < C` | `IGNORE_ABORTED_OR_STALE` |
| same transaction, another row | self-delete/supersession with `cmax == C` | `UNIQUE_CONFLICT` |
| another logical row | committed/frozen live owner | `UNIQUE_CONFLICT` |
| old version of another committed UPDATE | committed effective `xmax` | `IGNORE_ABORTED_OR_STALE`; its replacement candidate decides ownership |
| another transaction changing this key | creator or deleter nonterminal | `WAIT_THEN_RECHECK` |
| candidate creator aborted | any | `IGNORE_ABORTED_OR_STALE` |
| duplicate exact physical `(K,RID)` or invalid heap identity | any | `CORRUPTION_OR_INTERNAL_ERROR` |

For an UPDATE that retains K, the checker acquires K, revalidates the old target, and excludes exactly that old RID during the pre-publication check. The old and replacement RIDs need no persistent logical-row identity.

For `old K -> new J`, the operation acquires the fully non-NULL old/new locks in §11.9 order, checks J, then follows §15.3's existing new-version/old-`xmax`/index publication sequence. The old K entry cannot conflict with J unless K and J are semantically equal, in which case this is the retaining-key case. NULL transitions acquire/check only the fully non-NULL participating side, while the old non-NULL lock is still retained when a key is removed.

Multirow UNIQUE enforcement remains immediate, not an end-of-statement
deferred constraint. Two staged prospective owners of the same key conflict
through the pending-owner relation before either publishes. Every other
target's authoritative old owner remains a current owner during preparation;
only the exact currently checked row may self-exclude its old RID. Therefore a
key swap such as `R1: 1 -> 2, R2: 2 -> 1` fails because each replacement key
encounters the other row's current old owner. The architecture does not temporarily delete
all old owners, accept a final-state permutation, or let target processing
order change the result.

### 11.10.7 Competing transactions, waiting, and recheck

The logical key lock is the serialization point. For T1 inserting K and T2 attempting K:

```text
T1 owns UNIQUE_KEY(K) through terminal publication
T2 waits without physical latches
T1 COMMITTED -> T2 acquires, rescans current state, and conflicts
T1 ABORTED   -> T2 acquires, rescans, ignores T1's aborted version, and may proceed
```

The same rule applies when T1 is deleting/updating the existing owner. T2 cannot treat T1's nonterminal `xmax` as freeing K. After T1 commits, the old owner is stale and K may be reused; after T1 aborts, the old owner still conflicts.

After every wait, the checker discards prior candidate/status conclusions and restarts from `(K,MIN_RID)` under a fresh ReadEpochGuard. It must not reuse a historical statement-snapshot result, status lookup, RID dereference, or partial range position. The statement snapshot may select DML targets; it never decides current UNIQUE ownership.

Conforming DML obtains the key lock before publication and retains it through terminal status publication, so another nonterminal owner should normally be encountered as the LockManager owner before candidate scanning. If a defensive scan nevertheless observes one, it returns `WAIT_THEN_RECHECK`, releases all physical latches/borrowed heap state, and waits through logical transaction coordination. If the checker and other transaction are both reported as exclusive owner of the same full lock key, that is an internal invariant failure rather than permission to continue.

### 11.10.8 Physical candidate integrity, retry, recovery, and vacuum

An ordered user-key scan must return each physical `(K,RID)` exactly once. Duplicate stored copies of the exact same physical key are B+ structural corruption, not two semantic owners and not a candidate to silently deduplicate. A dangling entry or one whose protected RID now denotes different heap bytes is likewise corruption; §14.6 read epochs prevent legal RID reuse during the check.

SQL ownership and physical replay are separate:

- a normal executor call that discovers its intended exact `(K,RID)` already present without proven same-operation continuation reports an internal invariant/corruption error, not `UniqueViolation`;
- an explicitly proven retry by the same in-process mutation owner, before returning control and while continuously retaining its operation context/logical ownership, may treat that exact already-published physical insertion as idempotent success/no-op;
- recovery never runs a fresh SQL UNIQUE predicate. It replays/skips the already-authorized heap redo and `BTREE_MTR` by page LSN, so replay of the same `(K,RID)` cannot become a new SQL violation.

Vacuum may remove aborted or terminally stale candidates only under Chapter 14. Their physical presence before cleanup does not change this predicate, and their removal cannot change which logical versions own K. UNIQUE correctness never depends on prompt vacuum.

### 11.10.9 CREATE UNIQUE INDEX

The offline §21.8 build uses the same canonical encoded equality, any-NULL non-conflict rule, creator/deleter current-state classification, and corruption checks. Because SchemaLock plus exclusive TableWriterGate drains and excludes target-table writers, the private bulk build may detect duplicate fully non-NULL live owners by sorted/grouped encoded keys rather than acquiring one transaction UNIQUE_KEY lock per row. It must still reject more than one current logical owner and must not use the DDL statement's historical snapshot to omit a current committed owner.

### 11.10.10 §39.1 and autocommit boundary

`UNIQUE_CONFLICT` determines only that the constraint contributes an ordinary
`UniqueViolation` candidate. Sections 15.1.3 and 21.16.1 close and reduce all
such ordinary candidates before the statement's first persistent user-DML
write. A winning recoverable violation therefore has §39.1's pre-write
consequence; autocommit aborts its implicit transaction normally. A successful
check is not a write or commit, and every acquired UNIQUE_KEY lock remains held
through the implicit or explicit transaction's terminal publication.

### 11.10.11 Forbidden UNIQUE implementations

The architecture specifically forbids:

1. using ordinary snapshot visibility as the UNIQUE current-owner predicate,
2. allowing same-statement duplicates because `cmin == checking_command_id`,
3. rejecting a retaining-key UPDATE solely because its exact revalidated old RID appears,
4. treating another transaction's uncommitted delete as freeing a key,
5. treating an aborted creator as a permanent owner,
6. treating an aborted deleter as freeing the old owner,
7. treating physical user-key presence as conflict without heap/status recheck,
8. releasing a UNIQUE_KEY lock at statement end or while `MUST_ABORT` is nonterminal,
9. authorizing from a pre-lock probe without a complete post-grant recheck,
10. running SQL UNIQUE enforcement during recovery redo or treating exact authorized replay as a new logical conflict,
11. mapping semantically equal FLOAT64/composite/VARCHAR keys to different lock identities,
12. dereferencing a candidate RID without read-epoch protection or reusing a pre-wait RID after wakeup,
13. allowing two committed owners merely because their statement snapshots could not see one another.

## 11.11 Lock duration

The architecture uses strict transaction-duration write locking.

Both:

```text
TUPLE_WRITE
UNIQUE_KEY
```

locks remain held through:

```text
the §9.14 COMMITTED
or ABORTED terminal-publication linearization point
```

They are not released immediately after the corresponding page modification or at statement
end. They remain retained through terminal publication and are released only during §15.5
C5 or §15.6 A3. `MUST_ABORT` and `ABORTING` retain granted ownership through A3;
`COMMITTING` retains it through C5.

This prevents another writer from acting as though a still-unresolved transaction outcome were final.

## 11.12 Lock table

LockManager maintains a process-local runtime mapping from each exact `LockKey`
to one conceptual `LockQueue`.

Because every logical lock mode is exclusive, each queue conceptually contains:

```text
current owner
FIFO waiter list
```

The compatibility rule is:

```text
unowned key      -> grant exclusive lock
owned by self    -> same-transaction handling / no competing owner
owned by another -> wait
```

The runtime realization MAY use any data structures and synchronization
mechanisms that preserve complete lock-key equality, the FIFO queue and
same-owner semantics above, atomic grant/cancellation with no late grant after
a transaction becomes ineligible, §11.13 wait-for-graph integration, and
§11.11 terminal-duration ownership and coherent wake-up behavior. It MUST
preserve these semantics under concurrent access and remain compatible with the
parallel-ready architecture.

For `TUPLE_WRITE`, every queued request that can still become owner and every granted owner
counts as a live RID-retention claim. Queue position does not change that meaning. A
same-owner idempotent/subsumed acquisition preserves one effective ownership claim without
changing lock compatibility; an empty lock-table container with no live request is not a
claim. Transition from queued request to granted ownership preserves the claim continuously.

Cancellation/removal and loss of grant eligibility are one semantic event: after the claim
ends, that request can never receive a late grant or resume ownership of the old RID, while
any request that may still be granted remains a claim. This applies to ordinary
cancellation, shutdown cancellation, and deadlock-victim waiter removal. A granted victim's
claim instead remains through ordinary A3 release. The representation of this lifetime and
the synchronization used to expose it to Chapter 14 are implementation-defined.

## 11.13 Unified transaction-level gates and deadlock detection

This section is the canonical coordination/deadlock contract for every
transaction-owned blocking resource that can be retained across statements. It
covers the existing `TUPLE_WRITE` and `UNIQUE_KEY` locks plus the DDL,
writer-exclusion, and statistics-publication gates defined elsewhere. Those
gates need not share one lock-table container, but every transaction-to-
transaction blocking dependency among them shares one wait-for graph and one
victim rule.

### 11.13.1 Transaction-level synchronization domains

The closed domain registry is:

| Domain | Scope and modes | Transaction ownership and release | Compatibility |
|---|---|---|---|
| `SchemaLock` | One database-global exclusive schema/catalog-DDL gate. Same-transaction acquisition is reentrant. | Attempt-owned while DDL remains pre-publication; after successful schema-changing DDL or the first transaction-owned catalog mutation, retained through §9.14 terminal publication and released in §15.5 C5 or §15.6 A3. | Conflicts with another transaction's `SchemaLock`. |
| `TableWriterGate(TableId)` | Per-table `SHARED_WRITER` and `EXCLUSIVE_DDL` modes. | Persistent DML acquires shared before its first write to that table and, once granted for persistent DML, retains it through terminal publication. CREATE INDEX, DROP TABLE, and DROP INDEX acquire exclusive for their target table; a clean pre-publication DDL failure may release attempt-only exclusive ownership under §39.1.4. | Shared/shared is compatible. Exclusive conflicts with every mode held by another transaction. Same-transaction exclusive subsumes a later shared request. |
| object-publication gate | `STATS_PUBLISH(TableId, schema_version, immutable manifest)` and `MANIFEST_CHANGE(TableId, affected IndexIds)` modes. `STATS_PUBLISH` claims are acquired for the table and then manifest IndexIds in ascending order. A DDL manifest-change scope overlaps every statistics claim whose table/manifest it can change. | A statistics claim that authorizes the first `sys_statistics` row and a manifest-change claim that authorizes RETIRING/catalog publication are retained through terminal publication. A failed operation may release them before that boundary only under the exact §14.17.1/§39.1.4 pre-publication rules. | Compatible `STATS_PUBLISH` claims are shared. An overlapping `MANIFEST_CHANGE` conflicts with every statistics or manifest-change claim owned by another transaction. |
| `TUPLE_WRITE` / `UNIQUE_KEY` | Exact §11.2/§11.8 exclusive logical keys. | Strict transaction duration under §11.11. | Existing exclusive compatibility. |

`MANIFEST_CHANGE` is a mode of the already-established object-publication gate,
not a persisted object state or a new catalog field. CREATE INDEX uses the
target table scope; DROP INDEX uses the owning table plus the dropped IndexId;
DROP TABLE uses the table and its complete current index manifest. CREATE TABLE
has no pre-existing TableId/manifest with which an ANALYZE can conflict and
therefore needs only `SchemaLock` until its new catalog identities exist.

`SchemaLock` remains intentionally database-global in the defined architecture. This section does not
replace it with per-table schema locking. `TableWriterGate` remains distinct
from `TUPLE_WRITE`/`UNIQUE_KEY`, and the object-publication gate remains distinct
from object `LIVE/RETIRING/RETIRED` lifetime state.

### 11.13.2 Per-statement order and cross-statement admission

Within one statement, acquisition uses these canonical shapes:

```text
persistent DML on T:
    TableWriterGate(T) SHARED_WRITER
        -> TUPLE_WRITE / UNIQUE_KEY in their existing orders
        -> short physical latches

schema-changing DDL:
    SchemaLock
        -> required TableWriterGate EXCLUSIVE_DDL targets by ascending TableId
        -> optional private-file construction/build, with no short latch retained
           across the next transaction-gate wait
        -> MANIFEST_CHANGE scopes by TableId, then IndexId ascending
        -> any required tuple/key locks
        -> short live-object/catalog publication latches

ANALYZE publication:
    no page/B+ latch or read-epoch guard
        -> STATS_PUBLISH table claim
        -> STATS_PUBLISH IndexId claims ascending
        -> current-object/manifest revalidation
        -> statistics catalog publication
```

No current DDL statement has more than one target table. `DROP TABLE`'s
dependent indexes remain one table scope. The ascending-TableId rule is locked
for any already-supported operation that must acquire several table gates; it
does not add a new multi-table DDL feature.

Per-statement order alone is not a deadlock proof because explicit transactions
retain ownership across later statements. The architecture therefore allows a later statement
to request a resource earlier in the per-statement order and uses the unified
wait-for graph below. It does **not** globally reject unrelated DDL after DML or
ANALYZE merely to avoid a possible cycle.

Two same-transaction transitions are proactively rejected before waiting,
durable ID/file allocation, or the current statement's first persistent
publication:

1. `TableWriterGate(T) SHARED_WRITER -> EXCLUSIVE_DDL` for the same `T`. The architecture has
   no writer-gate upgrade protocol. A transaction that already owns exclusive
   on `T` may perform later DML on `T`; the exclusive mode subsumes shared.
2. A `MANIFEST_CHANGE` affecting a table/index manifest for which the same
   transaction has already published rows under a retained `STATS_PUBLISH`.
   Such DDL would invalidate that transaction's immutable generation before
   commit. DDL on an unrelated manifest remains legal.

The conceptual proactive error is
`UNSUPPORTED_TRANSACTION_GATE_TRANSITION` (a more specific
`DDL_NOT_ALLOWED_AFTER_WRITE` is permitted for case 1). It is not a deadlock
victim or database corruption. Because admission occurs before this statement's
publication boundary, an explicit transaction remains `ACTIVE` under §39.1;
autocommit aborts its one-statement transaction normally. A same-transaction
request already covered by a stronger or identical mode is reentrant/subsumed,
not a wait.

DDL followed by ANALYZE of the transaction's own final manifest remains legal.
The statistics revalidation must see the exact transaction-local post-DDL
manifest; the same owner may hold its `MANIFEST_CHANGE` and compatible
`STATS_PUBLISH` modes together. Once those statistics rows publish, a later
same-scope manifest change is case 2 above.

### 11.13.3 Retained-ownership compatibility matrix

The only cell classifications are `ALLOW`, `WAIT + DEADLOCK GRAPH`, and
`REJECT BEFORE WAIT`; no listed combination is left implementation-defined.
`ALLOW` includes an identical/reentrant or stronger same-owner mode that needs
no sleep. `WAIT + DEADLOCK GRAPH` is a legal request that is granted immediately
if no external owner or elected queue predecessor blocks it, otherwise §11.13.4
applies. Mixed retained state uses the most restrictive applicable cell. Scope
qualifiers are normative.

| Transaction already retains | Later `SchemaLock` | Later shared `TableWriterGate(T)` | Later exclusive `TableWriterGate(T)` | Later `STATS_PUBLISH` | Later `MANIFEST_CHANGE` |
|---|---|---|---|---|---|
| `SchemaLock` | `ALLOW` (reentrant) | `WAIT + DEADLOCK GRAPH`; an already-owned same-T exclusive mode is `ALLOW` | `WAIT + DEADLOCK GRAPH`; owning shared on the same T instead makes it `REJECT BEFORE WAIT` | `WAIT + DEADLOCK GRAPH`; an overlapping same-owner manifest mode is compatible only after exact final-manifest revalidation | `WAIT + DEADLOCK GRAPH`; an identical already-owned scope is `ALLOW` |
| shared `TableWriterGate(A)` | `WAIT + DEADLOCK GRAPH`, including DDL for unrelated B | same A: `ALLOW`; different B: `WAIT + DEADLOCK GRAPH` | same A: `REJECT BEFORE WAIT`; different B: `WAIT + DEADLOCK GRAPH` after SchemaLock | `WAIT + DEADLOCK GRAPH` | same A: `REJECT BEFORE WAIT`; unrelated B: `WAIT + DEADLOCK GRAPH` after SchemaLock |
| exclusive `TableWriterGate(A)` | `ALLOW` because canonical acquisition already owns SchemaLock | same A: `ALLOW`; different B: `WAIT + DEADLOCK GRAPH` | same A: `ALLOW`; different B: `WAIT + DEADLOCK GRAPH` | `WAIT + DEADLOCK GRAPH`; overlapping same-owner manifest mode is compatible only after exact final-manifest revalidation | same already-owned scope: `ALLOW`; different scope: `WAIT + DEADLOCK GRAPH` |
| `STATS_PUBLISH(A,manifest)` after row publication | unrelated DDL: `WAIT + DEADLOCK GRAPH`; same-scope manifest-changing DDL is rejected before requesting SchemaLock | `WAIT + DEADLOCK GRAPH` | same affected manifest: `REJECT BEFORE WAIT`; unrelated B: `WAIT + DEADLOCK GRAPH` after SchemaLock | same/overlapping claim: `ALLOW`; unrelated scope: `WAIT + DEADLOCK GRAPH` | same affected manifest: `REJECT BEFORE WAIT`; unrelated scope: `WAIT + DEADLOCK GRAPH` after SchemaLock |
| `MANIFEST_CHANGE(A,scope)` | `ALLOW` because canonical acquisition already owns SchemaLock | same A: `ALLOW` because canonical acquisition already owns exclusive; different B: `WAIT + DEADLOCK GRAPH` | same A: `ALLOW`; different B: `WAIT + DEADLOCK GRAPH` | overlapping same-owner final manifest: `ALLOW` after exact revalidation; unrelated scope: `WAIT + DEADLOCK GRAPH` | same scope: `ALLOW` (reentrant); different scope: `WAIT + DEADLOCK GRAPH` |

Requests for `TUPLE_WRITE` and `UNIQUE_KEY` after any gate in the table remain
legal in their existing order and use the same graph if blocked. No transaction
in `MUST_ABORT`, `COMMITTING`, or `ABORTING` may begin a later ordinary
statement or acquire a new ordinary gate; it may only finish its required
terminal protocol.

The resulting explicit-transaction behavior is exact:

| Sequence | outcome |
|---|---|
| `UPDATE A; CREATE TABLE B` | Legal. The SchemaLock request may wait and participates in the graph. |
| `UPDATE A; CREATE INDEX ON A` | Rejected before waiting by the same-table shared-to-exclusive rule; the explicit transaction remains ACTIVE because the DDL statement published nothing. |
| `ANALYZE A; CREATE TABLE B` | Legal. The SchemaLock request may wait and participates in the graph. |
| `ANALYZE A; CREATE/DROP INDEX A` or `DROP TABLE A` after statistics rows published | Rejected before waiting because it would invalidate the retained immutable manifest. |
| `CREATE TABLE/INDEX;` then DML | Legal. A same-target exclusive writer gate subsumes the transaction's shared DML request; other target gates use the graph. |
| several supported DDL statements | Legal under the reentrant global SchemaLock and per-target gates, except the two proactive rejection cases above. |

Autocommit uses the same gates, graph, victim policy, and revalidation. Its
single statement simply cannot create a later-statement inversion in the same
transaction.

### 11.13.4 Unified wait-for graph and deterministic victim

Every transaction-level sleep caused by another transaction creates edges in
one database-local graph. Before the waiter can sleep, under synchronization
that is atomic with observing the incompatible owners/queue dependencies, it
installs:

```text
waiter TxnId -> every transaction whose ownership or earlier queued request
                currently prevents this grant
```

The mandatory edge registry is:

| Wait request | Edge target(s) | Recheck after wake |
|---|---|---|
| `SchemaLock` exclusive | Current SchemaLock owner | Catalog/name state and every bound DDL identity/precondition |
| shared `TableWriterGate(T)` | Exclusive owner, if any | Target descriptor, transaction-local schema/index set, and DML target/candidate state |
| exclusive `TableWriterGate(T)` | Every other shared or exclusive owner | Table identity/liveness, descriptor, writer drain, and DDL preconditions |
| `STATS_PUBLISH` | Every overlapping `MANIFEST_CHANGE` owner or queue dependency | Complete §14.17.1 table/schema/index manifest and non-RETIRING state |
| `MANIFEST_CHANGE` | Every overlapping statistics/manifest owner or queue dependency | Current manifest, object liveness, DDL name/identity state, and RETIRING eligibility |
| `TUPLE_WRITE` / `UNIQUE_KEY` | Existing conflicting owner(s) | Existing §11.3/§11.10 full target/candidate/status recheck |
| optional fairness/queue delay | Every earlier transaction request whose ordering alone prevents grant | The resource-specific recheck above |
| new claim observes already-published RETIRING | No edge and no sleep; reject/cancel through object revalidation | Not applicable |
| post-commit physical retirement drain | No transaction edge; terminal DROP no longer waits on a gate held by an ACTIVE transaction | Exact descriptor/guard/frame/file-owner drain before unlink |

An implementation may grant compatible requests without FIFO fairness. If it
chooses to make a compatible request sleep behind an earlier incompatible
waiter, that predecessor is a real graph dependency; queue policy must not
create an unrepresented wait.

For every shared writer-gate acquisition, including an uncontended grant or
a wake after waiting, physical write authority is revalidated against
the **current object-publication state**, not only the statement's older catalog
snapshot. If the table is RETIRING/dropped, the DML attempt fails before
publication. If CREATE/DROP INDEX changed only the current index manifest, the
pre-write attempt replaces its physical index-maintenance/UNIQUE-enforcement set
with the current committed plus transaction-local applicable manifest before
any heap/index mutation. This does not change SQL row visibility or add ALTER
schema translation; it prevents a writer that bound before an offline index
build from omitting the newly authoritative index. A DDL commit between binding
and an uncontended grant requires the same check as a commit that wakes a waiter.

Adding or replacing wait edges performs synchronous cycle detection before the
new waiter sleeps. Conceptually, for every cyclic strongly connected component
in the resulting finite transaction graph, the victim is: `the highest normal TxnId in that cyclic component`.

This deterministic victim policy applies uniformly across every registered
resource family. The graph algorithm/data structure is not fixed,
but an implementation may not defer detection such that a known cycle can wait
indefinitely. A detector thread may perform additional diagnostics or catch
stale bookkeeping, but is not the only correctness mechanism. Timeouts may
diagnose starvation or I/O delay; they do not choose an arbitrary deadlock
victim.

The selected victim receives `DEADLOCK_DETECTED`, atomically enters
`MUST_ABORT`, and is driven through the ordinary §15.6 abort path regardless of
whether its current statement crossed the publication boundary. This is the
transaction-fatal §39.1 matrix row, not `DATABASE_NONCONTINUABLE`. Its outgoing
wait edges are canceled so it can abort, but all resources it owns remain
protected until ABORTED terminal publication and are released only at A3.
For a queued TUPLE_WRITE victim, edge cancellation alone does not end RID retention: the
claim ends only when canonical request removal simultaneously makes any later grant
impossible. A victim that already owns TUPLE_WRITE retains the granted claim through A3.
Waiters wake on actual release, remove/rebuild their edges, and perform the full
resource-specific revalidation before continuing. An internal inability to
maintain graph/ownership coherence is an invariant failure and may be
noncontinuable; an ordinary detected cycle is not.

### 11.13.5 Nontransaction owners, retirement, and lifecycle

VACUUM's `TableVacuumOwner`, `OBJECT_USE`, `StatusHistoryGuard`, page/read-epoch
guards, physical latches, file-retirement drain, checkpoint, and shutdown gates
are not transaction-lifetime lock owners in this graph. Their canonical
protocols prevent cross-graph cycles:

- VACUUM/ANALYZE scan ownership never waits for SchemaLock, TableWriterGate,
  object-publication, tuple, or unique ownership while holding a page latch or
  read-epoch guard;
- CREATE INDEX drains mutating VACUUM at the §14.17.1 publication boundary;
  that pass releases TableVacuumOwner before any transaction-gate/logical-lock
  wait, so the drain cannot depend back on the DDL transaction;
- DROP may mark RETIRING and commit before `OBJECT_USE`/descriptor/physical
  owners drain, so the DDL transaction does not wait for those owners while
  retaining transaction gates;
- RETIRING rejects a new use/statistics claim or causes its current
  revalidation to fail; it is not an indefinite wait on a maintenance worker;
- physical unlink runs only after terminal DROP and drains/cancels maintenance
  ownership outside the transaction graph;
- an internal maintenance action that actually uses a transaction-owned gate
  or catalog transaction is represented by its real TxnId and follows this
  graph exactly; it may not be an anonymous blocker.

Selecting a deadlock victim does not itself release object, descriptor,
BufferPool, guard, or frame ownership. If a DROP victim had marked RETIRING,
its ABORT restores LIVE only through the existing object-gate protocol. If DROP
commits, physical unlink still waits for all owners to drain. No victim decision
is physical-retirement authorization.

On `READY -> DRAINING`, no new ordinary statement or gate request is admitted;
existing victims and other nonterminal transactions complete the canonical
COMMIT/ABORT handling in §3.3.6. At `DATABASE_NONCONTINUABLE`, no new ordinary
acquisition begins, retained ownership is not guessed away, and only the
existing controlled teardown/recovery rules apply. Connection loss likewise
drives ACTIVE/MUST_ABORT transactions through §39.1.7 and cannot remove their
gate ownership before terminal cleanup.

### 11.13.6 Operation gate-order table

| Operation | Transaction/maintenance gates in order | Lifetime | Can block / graph | Mandatory wake/revalidation |
|---|---|---|---|---|
| SELECT | No SchemaLock/TableWriterGate/publication claim; ordinary snapshot/descriptors | Statement/transaction snapshot rules | No transaction gate wait from this registry | Ordinary descriptor/page validation |
| INSERT/UPDATE/DELETE on T | shared `TableWriterGate(T)` -> existing tuple/unique locks | Through terminal publication after acquisition for persistent DML | Yes; every transaction blocker is in the unified graph | Descriptor/target/key/visibility recheck appropriate to the wait |
| ANALYZE scan | `OBJECT_USE` table then IndexIds; page/read-epoch guards only while scanning | Scan/safe-point lifetime | Nontransaction cancellation protocol, not graph | Object still LIVE/current before each protected continuation |
| ANALYZE publication | `STATS_PUBLISH` table then IndexIds ascending | Through terminal after first statistics-row publication | Yes; unified graph | Complete current manifest and object-state revalidation |
| CREATE TABLE | global `SchemaLock` | Through terminal after catalog publication; attempt-only before it | Yes; unified graph | Name/catalog allocator and DDL prerequisites |
| CREATE INDEX on T | `SchemaLock` -> exclusive `TableWriterGate(T)` -> `MANIFEST_CHANGE(T)` -> §14.17.1 VACUUM publication exclusion/drain | Through terminal after catalog publication; attempt-only before it | Transaction gates: unified graph; maintenance drain: §11.13.5 no-cycle rule | Table/name/descriptor/writer drain/complete manifest, maintenance admission and file-publication prerequisites |
| DROP TABLE T | `SchemaLock` -> exclusive `TableWriterGate(T)` -> `MANIFEST_CHANGE(T,all current indexes)` -> RETIRING | Through terminal; physical drain may outlive transaction | Yes for transaction gates; RETIRING/use drain follows maintenance protocol | Current table/manifest, no-new-publication winner, and retirement state |
| DROP INDEX I on T | `SchemaLock` -> exclusive `TableWriterGate(T)` -> `MANIFEST_CHANGE(T,I)` -> RETIRING(I) | Through terminal; physical drain may outlive transaction | Yes for transaction gates | Current index ownership/manifest and retirement state |
| VACUUM publication unit | `OBJECT_USE` -> TableVacuumOwner -> status/read-epoch/page ownership | Per pass/unit as §14.17.1 | No transaction gate wait while short ownership is retained | Existing vacuum/object/status rechecks |
| COMMIT | No new ordinary gate; terminal protocol C0-C6 | Release all retained transaction gates at C5 after C4 COMMITTED publication | Terminal WAL/durability waits are separately owned | Commit prerequisites and cache/ownership coherence |
| ABORT / deadlock victim | No new ordinary gate; terminal protocol A0-A4 | Release all retained transaction gates at A3 after A2 ABORTED publication | Victim wait is canceled; owned gates remain until A3 | Abort/status publication and cleanup coherence |

### 11.13.7 Adversarial outcomes and completeness argument

The required outcomes are:

1. T1 `UPDATE A`; T2 `CREATE INDEX A`; T1 `CREATE TABLE B`: T2 waits on
   T1's shared writer gate; T1 then waits on T2's SchemaLock. Both edges are in
   one cyclic component. The higher TxnId is the victim; after its ABORTED
   publication/releases, the survivor wakes, revalidates, and continues.
2. T1 publishes `ANALYZE A`; T2 obtains SchemaLock/writer ownership and waits
   for the A publication claim; T1 requests SchemaLock for unrelated B: the
   publication and SchemaLock edges form one cycle and the higher TxnId aborts.
   A same-scope DDL by T1 itself would instead be rejected before waiting.
3. T1 `UPDATE A`; T2 `UPDATE B`; T1 requests DDL on B; T2 requests DDL on A:
   the first SchemaLock owner can wait on the other's shared writer gate while
   the second waits on SchemaLock. The unified graph selects the higher TxnId.
4. T1 publishes `ANALYZE A`; T2 publishes `ANALYZE B`; both later request
   cross-object DDL: any SchemaLock/publication cycle uses the same victim rule.
   A transaction's own DDL affecting its already-published statistics scope is
   rejected before waiting.
5. Same-table shared-to-exclusive writer upgrade is rejected before waiting and
   creates no self-edge.
6. Autocommit UPDATE competing with autocommit CREATE INDEX has one writer-gate
   dependency but no same-transaction later-statement inversion; UPDATE reaches
   terminal outcome, then DDL wakes and revalidates. Any additional transaction
   cycle is still detected normally.

Completeness follows from the wait rule: every live transaction-to-transaction
sleep on SchemaLock, either TableWriterGate mode, either object-publication
mode, TUPLE_WRITE, UNIQUE_KEY, or queue ordering has an edge to every
transaction preventing grant. Therefore any cycle composed of those resources
is a cycle in the one finite graph and synchronously selects the deterministic
highest-TxnId victim. The only excluded waits are either proactively rejected
self-transitions or nontransaction physical/maintenance drains whose protocols
do not wait back on transaction gates. Starvation remains a scheduling concern;
it cannot be justified as an undetected cycle.

The architecture forbids:

1. tracking object-publication waits but omitting SchemaLock/TableWriterGate waits;
2. detecting deadlocks only among tuple/key locks or in separate graphs that cannot see cross-resource cycles;
3. treating per-statement acquisition order as protection from retained cross-statement cycles;
4. allowing a transaction-level sleep or fairness queue dependency without its complete graph edges;
5. treating only same-table upgrade as dangerous while leaving other cycles unobserved;
6. choosing victims by timeout, thread scheduling, or a policy other than highest TxnId in the cyclic component;
7. releasing writer, schema, logical-lock, or publication ownership before terminal publication merely to break a cycle;
8. granting a woken waiter without resource-specific current-state revalidation;
9. treating an ordinary deadlock as database corruption/noncontinuability;
10. waiting for any transaction gate while retaining a page/B+ latch, BufferPool frame transition, or read-epoch guard;
11. using deadlock-victim selection as permission to unlink a RETIRING object's files before every owner drains;
12. removing a disconnected transaction from the registry or leaking its gates without completing the existing terminal cleanup protocol.

## 11.14 Physical latches are not LockManager locks

The architecture maintains a hard distinction:

```text
heap / B+ / buffer page latches:
    short-lived physical-memory/data-structure protection

LockManager locks:
    transaction-lifetime logical conflict protection
```

Page latches MUST NOT be represented as LockManager locks. Tuple logical locks MUST NOT be used to protect B+ page structure. Unique-key logical locks MUST NOT replace B+ structural latching.

Unified transaction wait-for-graph edges are transaction-conflict edges, not
page-latch dependency edges.

## 11.15 Logical-locking invariants

1. Logical lock modes are exclusive `TUPLE_WRITE` and `UNIQUE_KEY`.
2. Readers evaluate MVCC visibility without shared tuple locks.
3. A tuple-write key is `(TableId, target physical RID)`.
4. A writer releases physical page/index latches before waiting for a logical transaction lock.
5. After acquiring/waiting for a tuple-write lock, the target is re-fetched and revalidated.
6. An aborted competing `xmax` is logically ineffective.
7. READ COMMITTED applies §15.7 after a committed competing update/delete: a fresh-snapshot statement retry is permitted only before the first persistent statement write; a retry-requiring conflict after that boundary aborts the transaction.
8. REPEATABLE READ aborts on a post-snapshot competing committed write rather than following an invisible newer version.
9. The write protocol prevents lost updates on one target version.
10. Snapshot isolation may still permit cross-row write skew.
11. A unique-key lock is `(IndexId, full canonical encoded non-NULL user-key bytes)`; semantic equality and lock equality cannot diverge.
12. Hashing a unique key may select a lock-table shard but cannot replace full-key equality.
13. NULL-containing unique keys skip duplicate rejection.
14. Unique checks use §11.10's current-owner predicate over every physical candidate and do not substitute caller-snapshot visibility.
15. Current-command and earlier-command same-transaction owners conflict unless exactly self-excluded or deleted/superseded by an earlier CommandId.
16. UPDATE self-exclusion is limited to the exact revalidated old/replacement RID in the current operation context.
17. Tuple-write and unique-key locks are held through terminal outcome publication, including while `MUST_ABORT` cleanup is pending.
18. A wait invalidates all prior UNIQUE candidate conclusions and requires a fresh range/status/heap recheck under RID protection.
19. Exact physical replay is distinct from SQL uniqueness; unproven duplicate `(key,RID)` storage is an invariant/corruption error.
20. Deadlock correctness uses a wait-for graph; each cyclic strongly connected component selects its highest normal TxnId as the victim.
21. Page/B+ latches never become LockManager locks.
22. Logical transaction locks never substitute for physical B+ page-structure protection.
23. SchemaLock, both TableWriterGate modes, both object-publication modes, TUPLE_WRITE, and UNIQUE_KEY share §11.13's one cross-resource wait-for graph.
24. A transaction-level waiter installs every owner/queue dependency before sleeping and cannot leave a detected cycle waiting indefinitely.
25. Deadlock victims retain all owned transaction gates through ABORTED publication and release them only during canonical abort cleanup.
26. Every queued grant-eligible or granted `TUPLE_WRITE(TableId,RID)` request is a runtime physical RID-retention claim.
27. A writer retains its discovery read epoch through TUPLE_WRITE request registration and releases the epoch before any blocking lock wait.
28. Request cancellation/removal ends RID retention atomically with loss of grant eligibility; no ended claim can receive a late grant.
29. Granted TUPLE_WRITE retention remains through C4/A2 and ends only with ordinary C5/A3 lock release.
30. Chapter 14 cannot rebind a RID while any live TUPLE_WRITE request for its old identity exists.
31. User DML acquires the complete finite staged UNIQUE_KEY lock set and
    establishes exact same-statement pending-owner conflicts before its first
    persistent user-DML write; neither hashing nor candidate visitation selects
    the ordinary diagnostic.
