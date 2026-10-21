# 10. MVCC Visibility and Tuple-Version Semantics

## 10.1 Heap-version MVCC and rollback model

The architecture uses heap-version MVCC.

An ordinary UPDATE creates a new physical tuple version:

```text
old version:
    xmax = updating transaction

new version:
    xmin = updating transaction
    prev = old RID
```

The new physical version receives a new RID.

Ordinary user-transaction abort is a transaction-status/visibility decision rather than a byte-for-byte physical rollback of heap and B+ tree user-DML changes.

### Aborted INSERT

A version whose:

```text
xmin = aborted transaction
```

is invisible to other transactions.

Its physical heap bytes and index entries may remain until vacuum.

### Aborted DELETE

A version whose:

```text
xmax = aborted transaction
```

is treated as not deleted.

### Aborted UPDATE

The old version's aborted `xmax` is ineffective.

The new version's aborted `xmin` makes the new version invisible.

New physical index entries may remain as vacuumable garbage.

The rollback model therefore preserves:

```text
STEAL + NO-FORCE
```

without requiring physical ordinary-user-DML undo or compensation-log-record-style restoration of heap/index bytes.

Section 12.10.4 defines abort's treatment of B+ structural shape, while
§§13.13–13.16 define redo, loser resolution, and the no-user-DML-CLR recovery
model without changing these visibility semantics.

## 10.2 Creator visibility

Given tuple version `T` and snapshot `S`, determine creator visibility first.

The exact rule is:

```text
if T.xmin == FROZEN_TXN_ID:
    creator_visible = true

else if T.xmin == S.owner_txn_id:
    if T.cmin < S.command_id:
        creator_visible = true
    else if T.cmin == S.command_id:
        creator_visible = false
    else:
        visibility evaluation fails: impossible future-command metadata

else:
    status_result = Status(T.xmin)

    if status lookup failed:
        propagate the exact lookup failure

    else if status_result == COMMITTED:
        if T.xmin >= S.xmax:
            creator_visible = false

        else if T.xmin is in S.active:
            creator_visible = false

        else:
            creator_visible = true

    else if status_result == IN_PROGRESS:
        creator_visible = false

    else if status_result == ABORTED:
        creator_visible = false

    else if status_result is RETIRED, INVALID, or RESERVED:
        visibility evaluation fails: status-dependent invariant violation

    else:
        visibility evaluation fails: impossible status result
```

If:

```text
creator_visible == false
```

the tuple is invisible regardless of `xmax`.

The strict self-created rule:

```text
cmin < snapshot.command_id
```

means a tuple created by the current statement is not rediscovered through ordinary snapshot visibility during that same statement.

Equality is the valid current-command case. A self creator with:

```text
cmin > snapshot.command_id
```

claims creation by a command in the evaluator's future and is not an ordinary
invisible case. Persisted tuple metadata with that relation is `CORRUPT_HEAP`;
an impossible runtime-only form detected before persistence is an internal
invariant failure under §39.1.

Operations such as `RETURNING` SHOULD use the operation's produced values rather than depending on an ordinary snapshot rescan of a just-created tuple.

## 10.3 Deleter visibility

After creator visibility succeeds, inspect:

```text
T.xmax
```

### 10.3.1 No deleter

```text
T.xmax == INVALID_TXN_ID
```

means the tuple remains visible.

### 10.3.2 Deleted/superseded by the current transaction

If:

```text
T.xmax == S.owner_txn_id
```

then:

```text
if T.cmax < S.command_id:
    tuple is no longer visible
else if T.cmax == S.command_id:
    tuple remains visible to the current statement
else:
    visibility evaluation fails: impossible future-command metadata
```

Thus a delete/update performed by the current statement does not retroactively remove the old version from that same statement snapshot.

A self deleter with `cmax > S.command_id` claims deletion or supersession by a
command in the evaluator's future. Persisted tuple metadata with that relation
is `CORRUPT_HEAP`; an impossible runtime-only form detected before persistence
is an internal invariant failure under §39.1.

### 10.3.3 Deleted/superseded by another transaction

Let:

```text
status_result = Status(T.xmax)
```

If status lookup fails, visibility propagates the exact lookup failure.

If:

```text
status_result == ABORTED
```

the delete/update never became logically effective, so the tuple remains visible.

If:

```text
status_result == IN_PROGRESS
```

the tuple remains visible to this snapshot.

Read visibility does not wait merely because another transaction currently has an in-progress `xmax`.

Write-conflict handling is owned by Chapter 11.

If:

```text
status_result == COMMITTED
```

the tuple is deleted for snapshot `S` only when:

```text
T.xmax < S.xmax
and
T.xmax not in S.active
```

Otherwise the deleting/updating transaction is too new for the snapshot, so the old tuple remains visible.

If `status_result` is `RETIRED`, `INVALID`, or `RESERVED`, visibility fails with
the canonical invariant/corruption result. `INVALID_TXN_ID` as `T.xmax` remains
the valid no-deleter sentinel handled by §10.3.1; a status lookup result of
`INVALID` for a normal referenced TxnId is not that sentinel. `RESERVED` remains
a recognized v1 encoding, but neither it nor `INVALID` is a terminal outcome.
Any other status result in this non-self branch is likewise an invariant error.

## 10.4 Visibility evaluation order

Ordinary MVCC visibility is an error-capable semantic decision, not a total
Boolean function over arbitrary tuple/status inputs. It is evaluated
conceptually as:

```text
1. validate tuple structure and ownership
       failure   -> propagate canonical validation error
2. validate same-transaction command causality
       failure   -> canonical corruption/internal-invariant error
3. evaluate creator
       invisible -> invisible
       error     -> propagate error
       visible   -> evaluate deleter
4. evaluate deleter
       visible   -> visible
       invisible -> invisible
       error     -> propagate error
```

Step 2 occurs before creator invisibility may short-circuit the decision and
rejects each of these impossible SELF relations:

```text
T.xmin == S.owner_txn_id and T.cmin > S.command_id
T.xmax == S.owner_txn_id and T.cmax > S.command_id
T.xmin == S.owner_txn_id and T.xmax == S.owner_txn_id and T.cmax < T.cmin
```

When both transaction fields identify the owner, the tuple therefore MUST
satisfy:

```text
T.cmin <= T.cmax
```

`T.cmax < T.cmin` would claim deletion or supersession before creation and is
therefore causally impossible. Impossible persisted tuple metadata is
`CORRUPT_HEAP`; an impossible unpublished runtime-only form is an internal
invariant failure under §39.1.3. A recognizable unsupported future format
retains its §4.14 unsupported-format result rather than being relabeled as
corruption.

`RETIRED` does not substitute for a terminal outcome. Under §§14.14.1–14.14.3,
it is returned only after durable reclamation proof establishes that no valid
persistent correctness object still requires that transaction's outcome. A
status-dependent creator or deleter that still resolves to `RETIRED` therefore
violates that ownership contract. Likewise, `INVALID` and `RESERVED` are valid
decoded status codes but are not terminal visibility outcomes. Ordinary
visibility MUST fail rather than guess visible or invisible in any of these
status-dependent cases.

For ordinary visibility over a validated persisted tuple, such an impossible
status-dependent reference is `CORRUPT_HEAP`. If the incoherence is detected as
a runtime-only invariant failure before persisted tuple metadata is implicated,
§39.1.3's internal-invariant rule applies. These classifications refine the
`CORRUPTION_OR_INTERNAL_ERROR` family used by §11.10.4 without introducing a
new error category.

Transaction-status I/O, checksum, ownership, corruption, recovery/storage, and
unsupported-format failures are propagated with their canonical lower-layer
classification. Converting such a failure into a Boolean could expose
uncommitted or aborted data, suppress committed data, or change snapshot
semantics. Section 39.1 owns the resulting continuation policy.

Readers do not acquire logical tuple locks merely to evaluate MVCC visibility.

The physical heap page supplies tuple metadata.

The visibility subsystem combines that metadata with:

```text
snapshot
+
transaction status
```

and owns the SQL visibility decision.

## 10.5 MVCC hint cleanup

Maintenance may normalize tuple metadata once transaction outcome is permanently known.

### Aborted xmax

When:

```text
Status(xmax) == ABORTED
```

maintenance may rewrite:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

### Frozen creator

When §14.13.2 establishes that the original committed creator need no longer be distinguished, maintenance may rewrite:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

These are physical cleanup optimizations.

They do not change the logical history that made the cleanup safe.

Because they modify persistent page bytes, they follow the WAL/page-LSN mutation
rules in §§12.10–12.12.

Aborted-`xmax` normalization is defined by §14.13.1. The freeze and
transaction-status retirement rules are §§14.13.2–14.14.3.

## 10.6 MVCC invariants

1. Heap-version MVCC identifies creator/deleter transactions through tuple `xmin/xmax`.
2. Ordinary user abort does not require physical heap/index undo.
3. An aborted creator makes its physical version invisible.
4. An aborted `xmax` does not delete the old version.
5. `FROZEN_TXN_ID` is creator-visible as committed.
6. A self-created version is visible when `cmin < snapshot.command_id`, is invisible to ordinary rescan when equal, and is an error when `cmin > snapshot.command_id`.
7. A self-deleted/superseded version becomes invisible when `cmax < snapshot.command_id`, remains visible to the current-command ordinary rescan when equal, and is an error when `cmax > snapshot.command_id`.
8. A committed creator with `xmin >= snapshot.xmax` is too new.
9. A committed creator present in `snapshot.active` remains invisible to that snapshot.
10. An in-progress or aborted other-transaction deleter does not make the old tuple invisible.
11. A committed deleter removes the tuple only if it committed within the snapshot's visible past: `xmax < snapshot.xmax` and not in `snapshot.active`.
12. Read visibility does not acquire tuple-write locks.
13. HeapPage does not decide visibility.
14. Hint cleanup is a physical page mutation and does not invent transaction outcome.
15. Ordinary MVCC visibility returns visible or invisible only for architecture-valid tuple/status inputs; it MUST NOT guess a transaction outcome or swallow a status-lookup failure.
16. A status-dependent `RETIRED`, `INVALID`, or `RESERVED` result is an invariant/error condition, not an ordinary visible/invisible state; Chapter 14 owns the proof that permits status retirement.
17. SELF command metadata MUST NOT point into the evaluator's future, and same-transaction creation/deletion command order MUST be causally valid; impossible values are corruption/internal errors rather than visibility states.
