# 14. Vacuum and Storage Reclamation

## 14.1 Scope

Vacuum converts transaction-history facts into safe physical reclamation.

It owns:

- the global snapshot reclamation horizon,
- tuple-version garbage eligibility,
- exact secondary-index cleanup,
- persistent `NORMAL -> DEAD` retirement,
- read-epoch-delayed `DEAD -> UNUSED` physical RID reuse,
- version-chain splicing before reused storage can be referenced,
- cleanup of aborted `xmax`,
- freezing of sufficiently old committed creators,
- transaction-status retention/reclamation eligibility,
- FSM maintenance after reclamation,
- vacuum maintenance counters.

Vacuum does **not** decide tuple visibility using only its own snapshot.

Vacuum does **not** wait for ordinary transactions while holding heap or B+ page latches.

## 14.2 Global snapshot horizon

`SnapshotManager` maintains the registry of active SQL snapshots.

Define:

```text
global_oldest_snapshot_xmin =
    minimum snapshot.xmin among all currently registered SQL snapshots
```

If no SQL snapshot is registered:

```text
global_oldest_snapshot_xmin =
    current next_txn_id
```

The authoritative reclamation horizon is therefore active SQL snapshots, not merely transactions that currently exist.

This distinction matters because:

- a READ COMMITTED transaction between statements may have no registered statement snapshot,
- a REPEATABLE READ transaction retains one transaction snapshot,
- a long-running query retains its statement snapshot for the executor lifetime.

A long-running registered snapshot may intentionally delay reclamation of versions it could still observe.

SQL-snapshot lifetime and transaction-status retention are distinct. An ACTIVE READ
COMMITTED transaction between statements may have no registered snapshot and therefore
does not constrain `global_oldest_snapshot_xmin`; if it is write-capable, its §9.4
write-status dependency continuously prevents retirement of its own TxnId. A REPEATABLE
READ transaction may independently constrain both mechanisms through different claims:
its retained SQL snapshot protects logical history, while its write-status dependency
protects possible future persistence of its own TxnId.

## 14.3 Tuple-version garbage eligibility

A physical tuple version is a vacuum garbage candidate when either rule below is satisfied.

### 14.3.1 Aborted creator

```text
Status(xmin) == ABORTED
```

Such a version never became logically visible as a committed row version.

### 14.3.2 Globally dead committed version

The creator is committed or frozen, and:

```text
xmax != INVALID_TXN_ID
Status(xmax) == COMMITTED
xmax < global_oldest_snapshot_xmin
```

together with any active-snapshot consistency check required by the SnapshotManager representation.

At that point no currently registered legal snapshot can require the old version.

Garbage eligibility is a **global** property.

Vacuum MUST NOT reclaim a version merely because the version is invisible to the vacuum worker's own snapshot.

## 14.4 In-progress transaction metadata

Vacuum does not reclaim a version based on:

```text
xmin IN_PROGRESS
or
xmax IN_PROGRESS
```

Such a candidate is skipped/retried later.

Vacuum MUST NOT wait for the owning transaction while retaining heap/B+ physical latches.

## 14.5 Two-phase physical RID reclamation

Secondary indexes store a physical RID without a generation counter.

Therefore a physical `(PageNo, SlotId)` must not be reused while an already-running reader may still possess an old index-derived RID with that identity.

Reclamation is:

```text
NORMAL
    ↓
exact index cleanup + semantic retirement
    ↓
DEAD
    ↓
read-epoch grace period
    ↓
TUPLE_WRITE holder/waiter claim quiescence + version-chain reuse proof
    ↓
UNUSED / reusable
```

`DEAD` means:

```text
the old tuple version is no longer semantically required
all required secondary-index entries have been removed
the physical RID is still not reusable
```

`DEAD` may remain persistent while a live `TUPLE_WRITE(TableId,RID)` request blocks
final reuse. The claim does not by itself make the tuple visible or forbid an otherwise
eligible index cleanup, normalization, freeze, or `NORMAL -> DEAD` transition; it prevents
only rebinding the RID to a different logical identity. Read-epoch grace, predecessor-link
safety, and lock-claim quiescence are independent barriers.

This rule also governs whole heap-page recycling when reinitialization would make old
`(PageNo, SlotId)` identities reusable; every old RID on that page must satisfy the same
epoch, TUPLE_WRITE-claim, index, predecessor, and slot-state barriers.

## 14.6 ReadEpochManager

Physical RID identity safety uses a lightweight `ReadEpochManager`, separate from MVCC visibility.

For a writer that retains a candidate RID after releasing its discovery page latch, the
read epoch protects identity only until the gap-free §11.3 handoff establishes a live
`TUPLE_WRITE` request. The request then protects the same RID through immediate ownership
or a blocking wait. Neither mechanism substitutes for the other's lifetime.

The correctness baseline intentionally uses one mutex-protected epoch registry rather than a lock-free epoch scheme.

Process-local state is:

```text
uint64 current_epoch = 1
map<uint64, uint64 active_reader_count>
```

Epoch value `0` is invalid.

### 14.6.1 Reader registration

Before an executor can read an index entry or otherwise retain a stored RID beyond the current protected page operation, it registers:

```text
lock epoch mutex
e = current_epoch
active_reader_count[e] += 1
unlock
```

The returned `e` is held by an RAII read-epoch guard.

Guard destruction decrements that exact epoch count under the same mutex and removes a zero count.

A reader never changes epoch while one guard is alive.

The §11.10 UNIQUE candidate scan is such a reader: it registers before consuming an index-derived RID and retains protection through that RID's heap/current-owner recheck. If it must wait for transaction outcome, it discards the candidate and guard first, then re-probes the entire key range under a fresh guard after wakeup.

An UPDATE/DELETE candidate that will request `TUPLE_WRITE` retains its read-epoch guard
through registration of that request, releases the guard only after the request is a live
RID-retention claim, and releases it before any blocking wait. A pure reader continues to
use only the read epoch and acquires no tuple lock merely for reclamation safety.

### 14.6.2 RID retirement

After every required exact index entry has been removed and the heap slot has been installed persistently as `DEAD`, vacuum retires the RID:

```text
lock epoch mutex
retire_epoch = current_epoch
if current_epoch == UINT64_MAX:
    fail/require maintenance restart before further RID reuse
current_epoch += 1
record (RID, retire_epoch) in the process-local retire queue
unlock
```

Retirement increments the epoch exactly once.

A reader that registered before or during index cleanup may conservatively hold an epoch `<= retire_epoch`.

A reader that registers after retirement obtains an epoch `> retire_epoch` and cannot have obtained the removed index entry through normal post-retirement traversal.

### 14.6.3 Exact grace predicate

A retired RID with epoch `R` is safe for `DEAD -> UNUSED` exactly when, under the epoch mutex:

```text
there exists no active reader epoch E such that E <= R
```

Equivalent implementation:

```text
active_reader_count empty
OR
minimum active epoch > R
```

No heuristic time delay substitutes for this predicate.

This predicate completes only the read-epoch component of RID reuse. Even after it is
satisfied, `DEAD -> UNUSED` remains forbidden while any live queued or granted
`TUPLE_WRITE(TableId,RID)` claim exists. Conversely, zero lock claims never bypasses the
epoch predicate.

### 14.6.4 Crash/restart

Epochs and the retire queue are process-local and not WAL logged.

After crash/restart there are no surviving pre-crash readers.

There are likewise no surviving pre-crash `TUPLE_WRITE` owners or waiters; LockManager
starts empty and no lock request is WAL-logged, checkpointed, or replayed. This is safe
because the callers that could resume using those old RIDs also ceased to exist.

A recovered persistent `DEAD` slot that is not represented in the new process retire queue is conservatively re-enqueued with a fresh retirement epoch before being converted to `UNUSED`.

This may delay reuse unnecessarily but cannot reuse too early.

## 14.7 Why MVCC visibility is insufficient for RID reuse

An index cursor may obtain:

```text
(key, old RID)
```

and delay fetching the corresponding heap page.

Meanwhile vacuum may remove the index entry.

If the physical RID were immediately reused for an unrelated tuple, the old cursor could dereference a physical identity whose meaning changed after the cursor observed it.

MVCC visibility alone is not the physical-identity guarantee.

The read-epoch grace period establishes:

> a physical RID does not change identity while an already-running reader may still possess that old RID.

A writer cannot retain that read epoch across a blocking transaction-lock wait. The live
`TUPLE_WRITE` request therefore supplies the complementary guarantee:

> a physical RID does not change identity while any queued or granted writer request still
> names that old RID.

Post-grant revalidation remains necessary because tuple visibility, `xmin`/`xmax`, command,
key, and current-owner state may change while the RID's physical identity remains stable.
The claim prevents identity aliasing; it does not prove that the write remains legal.

The protection domains remain distinct:

| Mechanism | Protects | RID-reuse effect | Status-cutoff effect | Crash persistence |
|---|---|---|---|---|
| SQL snapshot | logical MVCC visibility history | only through logical garbage eligibility | may constrain status proof through snapshot history | none; no pre-crash snapshot is replayed |
| page latch | short-lived page bytes/structure | none after release | none | none |
| BufferPool pin | frame residency and binding | not a logical RID-reuse substitute | none | none |
| read epoch | retained physical RID identity | blocks grace completion | none | none |
| live `TUPLE_WRITE` request | one writer's exact physical RID identity, queued or granted | blocks `DEAD -> UNUSED` and same-RID rebinding | none by itself | none |
| `StatusHistoryGuard` | bounded transaction-status lookup range | none | blocks cutoff advancement past its range | none |
| write-status dependency | write-capable transaction's own future TxnId publication | none | blocks retirement of that TxnId | none |

## 14.8 Persistent DEAD state and crash behavior

`DEAD` is persistent.

After WAL-protected index cleanup/retirement, crash may leave:

```text
NORMAL
    -> vacuum re-runs exact cleanup idempotently

DEAD
    -> required index cleanup completed before the persisted transition
```

Pre-crash epoch registrations do not survive.

A recovered DEAD slot is re-enqueued in the new process ReadEpochManager before reuse, as §14.6.4 specifies.

It may also have remained DEAD before the crash solely because a live TUPLE_WRITE claim
blocked final reuse. That runtime claim disappears with its owner/waiter, but recovery still
re-evaluates every persistent and new-process epoch prerequisite before publishing UNUSED;
it does not replay the old lock request or infer that all other barriers are complete.

## 14.9 Vacuum index-cleanup protocol

For one garbage-eligible tuple version with RID `R`:

```text
1. read the tuple header and resolve:
       ResolveSchema(TableId, tuple.schema_version)
   through the historical catalog contract
2. use that historical schema to read/copy enough tuple data
   to derive every indexed user key in the pass manifest protected by §14.17.1
3. re-check garbage eligibility
4. for every index:
       EraseIfPresent(encoded_user_key, R)
       through ordinary B+ system MTRs
5. re-fetch heap page
6. under write latch verify tuple/slot identity and expected header state
7. WAL-log/install slot NORMAL -> DEAD
8. release heap latch
9. register RID retirement with ReadEpochManager
```

`EraseIfPresent(key,RID)` is vacuum-idempotent:

```text
exact physical entry removed -> success
exact physical entry already absent -> success
B+ corruption / unrelated failure -> error
```

Because the physical B+ key is `(user_key,RID)`, there can be at most one exact target entry.

This idempotence is required after a crash that removed some/all exact index entries but did not yet persist `NORMAL -> DEAD`.

The heap slot MUST NOT become DEAD before every required secondary-index entry is known absent.

Vacuum performs B+ operations without retaining the heap-page latch.

## 14.10 Version-chain splicing

Before a retired RID becomes reusable, no surviving tuple version may retain a `prev` pointer to that soon-to-be-reused physical storage.

During a table vacuum pass, vacuum derives enough reverse-link information to locate surviving direct successors.

For a removable version `V`:

```text
V.prev = P
```

each surviving direct successor `S` whose current link is:

```text
S.prev = V
```

is rewritten to:

```text
S.prev = P
```

The successor-header mutation is WAL logged.

Only after required surviving links have been safely updated may reclamation of `V` progress toward RID reuse.

If the expected relationship cannot be proven under revalidation, vacuum defers that version to a later pass.

Version chains are not required for ordinary snapshot visibility, so shortening a chain is permitted when it leaves no surviving pointer to reusable storage.

## 14.11 Revalidation under concurrent activity

Before changing physical slot/header state, vacuum revalidates under the heap-page latch at least:

```text
slot state
xmin
xmax
expected version-chain fields
physical tuple identity
```

If the expected candidate state changed, the operation is skipped/retried.

Heap-page latches are not held while B+ tree cleanup occurs.

Before final `DEAD -> UNUSED` publication, vacuum also obtains a zero-live-claim result for
`TUPLE_WRITE(TableId,RID)` under a synchronization relationship that linearizes that result
with request registration and RID reuse. It may defer the candidate or wait without a page
latch/read epoch, then reacquire and revalidate before publication. Failure to establish
zero claims is benign inability to reclaim: the slot remains DEAD.

## 14.12 DEAD to UNUSED and free-slot publication

A slot may enter `UNUSED` only when all are true:

```text
read-epoch grace predicate satisfied
required version-chain splicing complete
no live queued or granted TUPLE_WRITE(TableId,RID) request exists
```

Before final reuse publication, vacuum must prove that no surviving tuple version still has `prev` equal to this RID. If that proof cannot be reconstructed/revalidated, the slot remains DEAD.

The zero-claim observation, prevention of a new old-identity request, and final reuse
publication have one gap-free ordering relationship:

```text
request registration linearizes first:
    the request is a retention claim and reuse cannot publish

reuse linearizes first:
    a writer remains under its discovery read epoch and must observe/revalidate the
    reclaimed or changed identity; it cannot establish a request for the old identity
```

The check includes every grant-eligible waiter and granted owner, not merely the current
owner or queue head. It concerns semantic request lifetime; an empty LockManager resource
container may remain allocated. No specific counter, observer API, mutex, or waiting policy
is required. If a claim exists, vacuum may skip/defer rather than block, and it MUST NOT
wait for claim release while holding a page latch or read epoch.

Then:

```text
1. establish the gap-free zero-claim/request-registration ordering described above
2. fetch and write-latch the heap page
3. verify the slot is still DEAD
4. revalidate that read-epoch, lock-claim, and version-chain reuse prerequisites remain satisfied
5. if its payload coordinates are nonzero:
       compact/repack while the slot is still logically DEAD
       produce canonical DEAD coordinates (0,0)
6. verify tuple_offset = 0 and tuple_length = 0
7. prepare the free-list transition:
       slot.state = UNUSED
       slot.aux   = old free_slot_head
       free_slot_head = this SlotId
8. WAL-log/install the page mutation as one page redo unit
9. update/rebuild advisory free-space metadata
```

The slot is not observable as reusable between payload reclamation and free-list publication.

The final canonical reusable slot is:

```text
tuple_offset = 0
tuple_length = 0
state        = UNUSED
aux          = next free SlotId or INVALID_SLOT_ID
```

Every reusable slot is discovered through the persisted `free_slot_head` chain defined in §5.3.2.

Insertion never reuses a DEAD slot directly.

Because only canonical UNUSED slots enter `free_slot_head`, a later allocation may bind the
same numeric RID to a new tuple only after no old read epoch, TUPLE_WRITE request, index
entry, or predecessor link can legally use it as the prior identity.

## 14.13 Metadata normalization and freezing

### 14.13.1 Aborted xmax

For a still-live version whose deleter outcome is:

```text
Status(xmax) == ABORTED
```

vacuum may WAL-log and normalize:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

No index mutation is required merely to remove an aborted `xmax`.

### 14.13.2 Frozen committed creator

For a live version whose creator is committed and older than every active snapshot that could distinguish that original creator, vacuum may WAL-log:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

Future visibility then treats the creator as committed without requiring the original transaction-status entry.

These normalization/freezing rewrites are ordinary WAL-backed page mutations. When one is
used to justify advancing the transaction-status cutoff, changing only a volatile dirty
frame is insufficient: the resulting status-independent state must satisfy the
crash-recoverability proof in §14.14.2. The proof is independent of whether the canonical
page mutation used `PAGE_DELTA`, `PAGE_IMAGE`, or another already-defined Chapter-12 page
record.

## 14.14 Transaction-status retention and physical reclamation

After sufficiently complete vacuum/freezing proves a cutoff `X` such that no persistent correctness object still requires transaction-outcome lookup for a normal TxnId below `X`, whole transaction-status pages below that cutoff may be retired.

Cutoff proof has two distinct status-dependency classes:

```text
runtime future-publication blocker:
    a registered write-capable transaction may still create persistent metadata
    containing its own TxnId

existing persistent dependency:
    already-published tuple/catalog correctness metadata still requires Status(TxnId)
```

The cutoff may pass a normal TxnId only when both classes are absent. C5/A3 release of a
write-status dependency removes only the first class; creator freezing, aborted-`xmax`
normalization, physical version removal, or another already-authorized status-independent
rewrite must eliminate the second class.

A persistent dependency is **crash-recoverably status-independent for cutoff `C`** only
when, after a crash at or after durable publication of `C`, recovery can reconstruct before
READY a state in which no correctness metadata requires the terminal outcome of any normal
TxnId `T < C`. For every page mutation used in that proof, one of these conditions MUST
hold:

```text
durable-page proof:
    the resulting status-independent page state is already durably stored under
    the ordinary stable-image page-writeback rules in §7.10 and §12.17

durable-WAL reconstruction proof:
    the WAL sufficient to reconstruct the status-independent state is durable
    and remains retained until the page no longer depends on it
```

Either proof is sufficient; both are not required. A volatile frozen/normalized/removed
state in a dirty frame is not a proof. If its data-file image still contains a below-cutoff
`xmin` or effective `xmax`, the authorizing WAL must be durable and the page's DPT/
`rec_lsn` plus installed-checkpoint retention state must continue to preserve a complete
reconstruction path under §12.16 and §§13.7–13.10. The same rule applies to heap, catalog,
system, or other persistent pages and to physical version removal when removal is the
status-independence proof. It does not change the eligibility rules for freezing,
aborted-`xmax` normalization, DEAD classification, or physical reclamation.

A DEAD/removal transition is not automatically a durability proof: the page state that
makes the old metadata irrelevant must itself be durable or WAL-reconstructable. Every
proof uses canonically validated page/tuple ownership, framing, checksum, and transaction
status; a validation or status-lookup failure prevents advancement and retains its existing
error classification.

For all prerequisite mutations relying on WAL reconstruction, define:

```text
required_wal_target =
    maximum authorizing record-start LSN among those mutations
```

If that set is nonempty, the existing §12.13 complete-record-prefix meaning requires
`durable_lsn >= required_wal_target` before cutoff control publication. No record-end LSN or
filesystem byte position is introduced. WAL durability establishes survival at the next
crash boundary; DPT/checkpoint retention separately prevents later recycling while an old
data-file image still requires that WAL. Durable cutoff publication never clears a dirty
page's `rec_lsn` or releases its reconstruction WAL.

This proof preserves NO-FORCE. A durably stored status-independent page may satisfy the
proof without retaining WAL that the ordinary checkpoint/recovery rules have already made
unnecessary. A dirty page may satisfy it through durable retained WAL without being forced.
An installed checkpoint may participate in an ordinary durable-page or WAL-retention proof,
but no checkpoint is inherently required for cutoff advancement.

Status retirement is monotonic. A runtime terminal cache, a recreated status page, or a
newly attempted write cannot make a below-cutoff TxnId usable again; the coordinator must
prevent a write-capable transaction's own TxnId from entering that range in the first
place.

Concurrent vacuum lookup and cutoff advancement are ordered by the canonical
`StatusHistoryGuard`/reclaimer protocol in §14.17.1; proof of a cutoff without
that runtime coordination is insufficient.

The proof concerns **status dependency**, not numerical occurrence. A heap/catalog
tuple `xmin` or effective `xmax` normally remains status-dependent until the
ordinary freezing/normalization rules make it independent. By contrast, a
globally published §34.3.1 `StatsVersion.stats_txn_id` is an immutable opaque
generation value and does not pin status history. Historical WAL `txn_id` fields
remain governed by WAL retention/recycling and likewise are not, merely by
occurring in retained WAL, status-page pins. This classification is field-specific
and does not create a general exception for ordinary MVCC metadata.

Write-status dependencies are runtime-only and are not replayed after a crash. This is safe
because no pre-crash transaction can publish new metadata after restart, surviving
already-persistent references remain subject to the proof above, and Chapter 13 resolves
terminal outcomes/losers before READY.

The absolute §9.12 TxnId-to-PageNo mapping is preserved permanently.

The architecture therefore reclaims old status history with **sparse hole punching while keeping file length and logical PageNos unchanged**.

### 14.14.1 Page-aligned reclaim cutoff

Status page `P >= 1` represents:

```text
first_txn(P) = FIRST_NORMAL_TXN_ID + (P - 1) * 32640
end_txn(P)   = first_txn(P) + 32640
```

where `end_txn` is exclusive.

A page is reclaimable only when:

```text
end_txn(P) <= proven frozen cutoff X
```

The persisted:

```text
database.control.txn_status_reclaim_before
```

is always page-aligned and means:

> every normal TxnId below this value belongs to logically retired status history.

Initial value is `FIRST_NORMAL_TXN_ID`.

### 14.14.2 Safe reclaim order

To advance the cutoff to page-aligned value `C`:

```text
1. under status-history coordination establish and revalidate the complete proof:
       a. every runtime future-publication blocker and StatusHistoryGuard permits C
       b. every existing persistent dependency below C has been eliminated by an
          eligible freeze, normalization, removal, or other canonical rewrite
       c. classify every prerequisite page state as already durable or WAL-dependent
       d. for the WAL-dependent set, establish
              durable_lsn >= required_wal_target
       e. prove DPT/rec_lsn/checkpoint retention keeps every required reconstruction
          record until the corresponding page no longer depends on it
2. using §13.2.4, durably update database.control.txn_status_reclaim_before = C
3. publish the same cutoff in the runtime TransactionStatusStore so no new lookup pins a retired page
4. for each affected TXN_STATUS page:
       wait for existing pins/I/O to drain without holding unrelated page latches
       mark the frame retired/non-writeback
       remove it from BufferPool/DPT ownership without flushing obsolete contents
5. only then optionally sparsely deallocate the corresponding full-page byte ranges from txn_status.dat while preserving logical file length and absolute PageNos
```

For status PageNo `P`, the physical sparse-deallocation range is exactly:

```text
[P * PAGE_SIZE, (P + 1) * PAGE_SIZE)
```

The order is deliberate:

- `fdatasync(database.control)` does not make unrelated WAL files durable, so step 1's WAL
  durability observation necessarily precedes step 2,
- crash after step 2 but before frame invalidation/sparse deallocation leaks physical space only,
- no retired dirty frame can later rewrite a sparsely deallocated page,
- the architecture never permits sparsely deallocating pages that the durable cutoff still says may be needed.

Cutoff publication does not itself release ordinary dirty-page recovery dependencies. In
particular, §12.10.5's preparatory TXN_STATUS image `F = rec_lsn` and every later required
terminal record remain retained until the dirty status-page dependency clears under
§13.10, or step 4 safely removes that wholly retired page from BufferPool/DPT ownership.
The cutoff cannot be used to recycle reconstruction WAL for still-dirty heap, catalog, or
system pages. This remains true after recovery: if redo reconstructs such a page and it
remains dirty, its recovered DPT/checkpoint state retains the required WAL until ordinary
stable writeback clears the dependency. The guarantee therefore covers a second crash, not
only the crash immediately after control publication.

The canonical crash outcomes are:

| Boundary | Authoritative result |
|---|---|
| prerequisite WAL undurable, dirty page, control old | `C` cannot publish; the old cutoff and required old status history remain authoritative |
| prerequisite WAL durable/retained, dirty page, control old | `C` may publish after all other proofs; a crash before publication selects the old cutoff and may replay harmless extra progress |
| status-independent page durably stored, old WAL no longer required | `C` may publish after all other proofs without forcing a checkpoint |
| prerequisite proof complete, control publication is known not to become authoritative | the older valid control slot remains authoritative; completed freeze/normalization progress remains valid |
| control write/sync outcome uncertain or new slot torn | runtime does not guess; reopen's §13.2.3 selection chooses the old slot or a complete valid new slot, both of which are safe |
| control `C` durable, old page image on disk, reconstruction WAL retained | recovery replays the retained WAL before READY and needs no status below `C` |
| control `C` durable, physical status-page reclamation fails | `C` remains authoritative and the extra physical status storage is harmless |

The state `control C durable + status-dependent old page image + missing required WAL` is
forbidden. Likewise, no physical status-page retirement that depends on `C` may precede
step 2, because a torn new slot can select the old cutoff. Failure or uncertainty while
establishing required WAL durability prevents step 2; ordinary §12.12 failure handling
retains already-authorized page progress, and uncertain storage authority follows its
noncontinuable/no-guessing path. Failure after durable `C` to sparsely deallocate a status
page does not roll the cutoff backward.

Checkpoint publication may occur before or after cutoff publication. In either order, the
selected checkpoint/control generations and the §13.10 retention floor MUST preserve every
reconstruction source needed by the authoritative cutoff. Neither ordering requires a
checkpoint-per-cutoff serialization or changes checkpoint format.

The storage layer MAY use any platform-supported sparse-deallocation mechanism for whole
status-page byte ranges, provided it preserves logical file length and absolute PageNo
mapping. If no such mechanism is available or the operation fails, correctness is
unaffected; the old blocks may remain physically allocated and reclamation may be retried.

The logical file size is not shrunk and later PageNos are never renumbered.

### 14.14.3 Lookup below the cutoff

Status lookup for:

```text
txn_id < txn_status_reclaim_before
```

returns runtime result `RETIRED` without reading/validating a punched status page.

Valid persistent tuple/catalog MVCC metadata must not retain a status-dependent
reference to such a TxnId; creators that still matter must have been rewritten to
`FROZEN_TXN_ID` before the cutoff advanced. Persisted fields whose specified
meaning is already status-independent may still numerically contain the value.
In particular, neither `MIN(sys_statistics.stats_txn_id)` nor any other
StatsVersion occurrence constrains the cutoff, and status reclamation requires no
statistics rewrite, replacement version, deletion, or invalidation.

Physical sparse reclamation therefore preserves the deterministic absolute mapping while bounding allocated disk blocks for old history.

## 14.15 B+ garbage cleanup

Vacuum removes exact physical B+ entries that reference:

```text
aborted-created tuple versions
globally dead committed tuple versions
```

Because the B+ physical key is `(user_key, RID)`, cleanup removes exactly the stale physical entry.

Resulting underflow, redistribution, merge, or root contraction remains an ordinary B+ MTR system action.

## 14.16 FSM and maintenance statistics

After heap reclamation or repacking:

```text
update the table FSM estimate
```

FSM remains advisory and rebuildable.

Vacuum may batch FSM estimate changes rather than place every estimate update on the critical transactional path.

Vacuum may collect maintenance counters such as:

```text
live tuple versions
dead tuple versions
aborted versions
pages scanned
pages compacted
index entries removed
bytes reclaimed
frozen tuples
```

Optimizer statistics remain the responsibility of ANALYZE. Vacuum MAY trigger or assist
that subsystem without assuming ownership of its published state.

## 14.17 Vacuum execution baseline

The architecture requires an explicit/manual vacuum path, for example:

```text
VACUUM table
```

or an equivalent internal maintenance invocation.

A sophisticated autovacuum scheduler is not required for the baseline.

Background scheduling MAY react to measured dead-version ratio, aborted-version pressure,
freezing/status pressure, or table growth.

The scheduling policy is operational, not part of tuple-reclamation correctness.

### 14.17.1 Maintenance coordination

This section is the canonical coordination contract among `VACUUM`,
`ANALYZE`, transaction-status reclamation, catalog DROP/object retirement,
statistics publication/garbage collection, and physical cleanup. The gates in
this section are conceptual runtime ownership domains; they add no persisted
state or format field.

#### Operation classes and ownership domains

| Operation | Scope and persistent effect | User-work concurrency | Same-operation concurrency |
|---|---|---|---|
| `VACUUM(TableId)` | One table plus the exact `IndexId` set in its retained descriptor. It performs independently WAL-backed heap/index/FSM/freezing maintenance units; the pass is not an ordinary user transaction and is not all-or-nothing. | Online with SELECT/INSERT/UPDATE/DELETE under §§14.2–14.12. | At most one mutating pass per TableId. A second request waits, then rechecks object liveness and starts a new pass or fails if the object retired. |
| `ANALYZE(TableId)` | One transaction-owned statement, one snapshot, one immutable table/schema/index manifest, and transactional `sys_statistics` rows. | Online with user work and VACUUM. | Concurrent ANALYZE statements are allowed, including on the same table; StatsVersion selection resolves publication order. |
| `TXN_STATUS_RECLAIM` | Database-global proof/cutoff publication and sparse physical retirement under §14.14. | Concurrent with ordinary work and table vacuum only through the status-history protocol below. | Exactly one global reclaimer owns cutoff publication at a time. A second request waits and recomputes after the owner finishes. |
| object/file retirement cleanup | One catalog-retired TableId or IndexId and its exact managed files. It drains runtime owners and performs §4.7 unlink/directory durability. | Unrelated live objects continue. The retired object admits no new work. | Serialized per object; a duplicate request waits and then observes completed or still-pending cleanup. |
| orphan cleanup | Database namespace, but only exact §4.7 entries already proven unowned. | May coexist with READY work because classified entries are semantically inaccessible. | One database-global namespace-cleanup owner at a time; a second request waits. |

Fuzzy checkpointing remains separately coordinated. VACUUM/status/retirement
mutations use ordinary WAL, DPT, WAL-before-data, and checkpoint rules; a
checkpoint neither grants maintenance ownership nor serializes these operations.
Maintenance admission is legal only in database state `READY`.

#### Object lifetime and publication gates

Every maintenance operation resolves stable nonreused `TableId`/`IndexId`
identities, never names, and uses the maintenance-facing object state:

```text
LIVE -> RETIRING -> RETIRED
```

These are runtime/catalog-lifecycle concepts, not persisted catalog flags.
`DROP` owns the `LIVE -> RETIRING` transition; aborting that DROP restores LIVE,
while terminal COMMITTED makes retirement semantic and permanent. The object
gate provides three distinct claim/mode kinds:

```text
OBJECT_USE(TableId/IndexId)
    admitted only while LIVE
    retains descriptors/files/pages against physical retirement
    does not prevent DROP from marking RETIRING

STATS_PUBLISH(TableId, schema_version, immutable manifest identities)
    transaction-owned and admitted only while every named object is LIVE/current
    shared with another compatible STATS_PUBLISH claim
    incompatible with DROP's transition to RETIRING and with catalog publication
    that changes the table/schema/index manifest
    once it authorizes a statistics-row publication, retained through COMMITTED
    or ABORTED terminal publication

MANIFEST_CHANGE(TableId, affected IndexIds)
    transaction-owned exclusive mode on the same object-publication gate
    acquired by CREATE INDEX, DROP INDEX, or DROP TABLE before it may change
    the applicable manifest or mark an affected identity RETIRING
    conflicts with overlapping STATS_PUBLISH/MANIFEST_CHANGE claims
    retained through terminal publication once it authorizes RETIRING or a
    catalog manifest mutation
```

The exact modes, same-transaction admission rules, release points, and unified
wait-for-graph behavior are canonical in §11.13. In particular, DDL waiting on
statistics and ANALYZE/later statements waiting on SchemaLock cannot form an
unobserved cross-statement cycle.

`VACUUM` and the expensive scan phase of `ANALYZE` hold `OBJECT_USE` claims on
the table and every required index. Index claims are acquired in increasing
`IndexId` order. An open file descriptor or cached descriptor without this
claim is not maintenance mutation authority. `DROP` may mark an object RETIRING
while ordinary use claims exist, immediately blocks new claims, and requests
maintenance cancellation; physical retirement and unlink still wait for every
use claim, descriptor/query handle, BufferPool/file owner, guard, and frame to
drain under §§7.12.5, 21.9, and 4.7.7.

An admitted VACUUM that observes RETIRING finishes or exactly rolls back its
current publication unit under §12.12, begins no next mutation unit, releases its
claims, and ends canceled. An admitted ANALYZE scan likewise releases its
current page/read-epoch ownership and cancels collection unless it already owns
the applicable `STATS_PUBLISH` claims. DROP may commit catalog deletion before
ordinary use claims drain, but it may not physically unlink their files.

#### Table VACUUM ownership, foreground work, and RID reuse

One exclusive runtime `TableVacuumOwner(TableId)` covers an entire mutating
vacuum pass. It serializes same-table passes but does not serialize different
tables. It is distinct from `TableWriterGate`: VACUUM does not take that gate
exclusive and remains online with foreground DML. A waiting second pass is
cancellable and must reacquire/revalidate the current descriptor and object
claims after it wins ownership.

**CREATE INDEX publication exclusion.** A mutating pass resolves and retains the
complete authoritative committed index manifest only after admission under
TableVacuumOwner. Pass admission and CREATE INDEX publication exclusion are
ordered atomically for the same TableId; merely retaining an older descriptor
before that boundary never authorizes a new pass.

After acquiring MANIFEST_CHANGE and before its first catalog index publication,
CREATE INDEX closes admission of new mutating passes on the target table and
waits for any admitted pass to finish or safely quiesce. Quiescing finishes or
exactly restores the current §12.12 publication unit, starts no next unit, and
releases TableVacuumOwner and the pass claims. Closing admission alone is not
permission to publish while an earlier pass still owns mutation authority. The
private build may overlap an admitted pass under the existing row-lifetime and
writer-exclusion rules; this exclusion is required before catalog publication.

Once all earlier mutating passes have drained, CREATE INDEX revalidates the
current manifest and retains the admission exclusion through terminal disposition
and C5/A3 ownership release. On COMMIT, the resulting committed manifest must be
available before a waiting/new pass may resolve its descriptor. On ABORT, that
pass resolves the surviving committed manifest, never the aborted index. A pass
that waited or quiesced starts anew from that authoritative manifest; it cannot
resume the earlier manifest as mutation authority. Same-transaction repeated
CREATE INDEX reuses the retained exclusion for that table. Different tables have
independent maintenance admission.

The exclusion is runtime coordination with TableVacuumOwner, not a new catalog
field, WAL record, transaction lock mode, or persisted owner. The CREATE INDEX
transaction acquires it after SchemaLock, exclusive TableWriterGate, and
MANIFEST_CHANGE; it holds no page latch, read epoch, or frame transition while
waiting for a pass to drain. A pass holding TableVacuumOwner MUST NOT wait for
transaction-owned gates or logical locks needed by the DDL owner. If it cannot
continue without such a wait, it quiesces and releases pass ownership first.
Thus the nontransaction drain cannot participate in a hidden cycle with the
transaction wait-for graph. Existing transaction-to-transaction dependencies
remain in §11.13's graph.

Cancellation or failure while waiting cannot grant late publication permission.
Before catalog publication, attempt-only admission exclusion may be released
only after the attempt can no longer publish, under §39.1.4; previously retained
same-transaction exclusion is not attempt-only. After catalog publication,
cancellation, connection loss, or victim selection uses ordinary abort handling
and keeps exclusion until A3. Storage uncertainty follows noncontinuable teardown
and recovery; it does not reopen ordinary admission. Crash discards runtime
claims, and recovered catalog authority determines the manifest before READY.
DROP retains its existing RETIRING/cancellation protocol; this publication drain
does not require a DROP transaction to wait for ordinary OBJECT_USE claims.

Foreground operations and ANALYZE are protected from physical reclamation by
ordinary MVCC snapshot registration, page/B+ latches and guards, and
`ReadEpochGuard` whenever an index-derived RID may be retained. A foreground writer
performs §11.3's gap-free handoff to a live TUPLE_WRITE RID-retention claim before
releasing that epoch for a lock wait. VACUUM performs the §14.11 recheck before each
mutation and never waits for transaction, maintenance, schema, or unique-key ownership
while holding a page/B+ latch or read epoch.
It does not need every `UNIQUE_KEY` lock to erase a terminally stale exact
`(key,RID)` entry: B+ mutation ownership plus the ReadEpoch protocol protect the
physical operation, while §11.10 current-owner recheck protects logical UNIQUE
correctness.

Concurrent workers MUST preserve this existing order for each RID:

```text
globally reclaimable logical version
    -> every required exact index entry known absent
    -> persistent NORMAL -> DEAD
    -> ReadEpoch retirement/grace
    -> version-chain reuse proof
    -> zero live queued/granted TUPLE_WRITE claims for the RID
    -> DEAD -> UNUSED / RID reuse
```

All indexes in the retained descriptor participate. Different-table vacuum
passes may run concurrently; their only global serialization is the narrow
ReadEpoch mutex and status-history coordination defined here. FSM updates use
ordinary FSM page synchronization and need no database-global maintenance
lock; an FSM estimate may be stale while heap free space remains authoritative.

TUPLE_WRITE request registration and the zero-claim/reuse publication in §14.12 have a
gap-free per-RID ordering. Registration first blocks reuse. Reuse first prevents a caller,
still protected by its discovery epoch, from registering a request for the old logical
identity after rebinding. Vacuum may defer a claimed RID; if it waits, it retains no page
latch or read epoch and revalidates the candidate before final publication.

#### ANALYZE snapshot, immutable identity, and publication revalidation

One ANALYZE statement has one owning TxnId/current CommandId and one stable
effective SQL snapshot under §34.3. The same snapshot governs all SQL-visible
heap rows, columns, and index-derived logical observations for that invocation.
Its bound identity and immutable candidate manifest contain at least:

```text
TableId
schema_version
exact ColumnIds
exact IndexIds
each required index's owning TableId and key-schema/fingerprint identity
```

The expensive scan runs with ordinary `OBJECT_USE` claims and without
schema-changing exclusivity, so VACUUM and DML may proceed. Physical index/page
statistics may change during collection and remain approximate under Chapter
34; page/RID access still uses guards and read epochs. A retirement request may
cancel the scan at a safe page-operation boundary.

After constructing one complete immutable descriptor/manifest, but **before
publishing the first `sys_statistics` row**, ANALYZE must:

```text
1. release scan page/B+ latches and transient RID ownership
2. acquire STATS_PUBLISH for the TableId and then all manifest IndexIds in
   increasing IndexId order
3. under that gate consult current object-publication state, not merely the
   possibly old ANALYZE statement/transaction snapshot
4. prove the TableId is still current/live (or is a transaction-owned CREATE
   that will publish atomically), schema_version is unchanged, every manifest
   ColumnId still belongs to that table, every manifest IndexId is live and
   belongs to that table, each required key schema/fingerprint still matches,
   and the current applicable ColumnId/IndexId sets equal the candidate manifest
5. prove that the immutable TABLE manifest still names exactly the candidate's
   required TABLE/COLUMN/INDEX scopes
6. publish all rows transactionally and retain every STATS_PUBLISH claim through
   terminal COMMITTED or ABORTED publication
```

This final check is independent of ordinary old-snapshot catalog visibility. A
check followed by releasing the gate before row publication/transaction terminal
outcome is invalid because DROP could linearize in between.

If claim acquisition/revalidation fails before any statistics row publishes,
the attempt releases those claims immediately. Once the validated operation
publishes its first row, its claims are transaction-lifetime locks. CREATE INDEX,
DROP, or any future schema/index-manifest change that arrives after that point
waits for terminal publication; one that published its manifest change first
causes the ANALYZE revalidation to fail.

Failure of any check discards the **entire** generation, including when only one
manifest IndexId was dropped. The architecture never edits/recomputes a generation manifest
during publication and never publishes a table generation lacking a formerly
required index member. Because revalidation precedes the first persistent stats
row, a recoverable failure may leave an explicit transaction ACTIVE under §39.1;
autocommit aborts its implicit transaction. If a later failure occurs after any
row publication, §39.1 requires abort and completeness filtering keeps the
generation globally unusable.

The ANALYZE/DROP linearization is the object publication gate:

```text
ANALYZE obtains every STATS_PUBLISH claim first:
    DROP waits; ANALYZE may reach terminal outcome for the still-live identities;
    after claim release, DROP may mark RETIRING and commit

DROP marks any required identity RETIRING first:
    ANALYZE cannot acquire/revalidate its complete claim set and publishes no
    generation
```

DROP obtains the overlapping `MANIFEST_CHANGE` mode before it may publish the
RETIRING transition. If ANALYZE owns `STATS_PUBLISH`, DROP waits through the
unified §11.13 graph; if DROP owns `MANIFEST_CHANGE` first, a new statistics
claim cannot pass current-object revalidation. CREATE INDEX and DROP INDEX use
the same winner rule for their table/index manifest scope.

The same rule covers `DROP TABLE` and `DROP INDEX`. It does not use the object
name, so DROP/recreate under the same name cannot capture stale statistics.
Schema-changing ALTER is not added by this rule; any future operation that
changes `schema_version` must participate in the same publication gate.

#### Concurrent statistics publication, cache, and garbage collection

Concurrent same-table ANALYZE operations may both commit complete generations.
Among generations applicable to the same current TableId/schema identity, the
greatest unsigned-lexicographic StatsVersion under §34.3.1 is selected. A cache
install is conditional on both current object/schema applicability and a newer
selected StatsVersion; reinstall of the same exact version is an idempotent
no-op, and an older version is rejected. Thread/callback order MUST NOT regress
the cache.

Terminal committed DROP first makes the TableId/IndexId inapplicable to new
planning and invalidates/bypasses its current statistics cache entry. Every
delayed ANALYZE cache callback rechecks current object/schema applicability
under the same cache/publication ordering before install. If DROP has won, the
callback is discarded and cannot reinstall the entry. Nonreused IDs ensure a
same-name replacement cannot inherit it. A commit-side cache failure still
follows §39.1: committed `sys_statistics` rows remain authoritative rebuildable
metadata, and safe older/missing-statistics fallback is allowed.

Older generations become statistics-GC candidates only after a newer committed
complete applicable generation is authoritative, or all of an object's
generations become candidates after that object is terminally dropped. Their
deletion is ordinary catalog MVCC maintenance and must preserve active catalog
snapshots; an in-flight newer ANALYZE is not sufficient. DROP makes table/index
statistics semantically inapplicable at terminal catalog publication even if
their rows are physically removed later. StatsVersion payload TxnIds create no
status-retention claim under §34.3.1.

#### Status-history guard and cutoff publication

Transaction-status reclamation and status-dependent vacuum lookup share one
runtime `StatusHistoryCoordinator`. Before a vacuum pass inspects any tuple
metadata that may require status lookup, it atomically registers a conceptual:

```text
StatusHistoryGuard(G)
G = currently published txn_status_reclaim_before
protected lookup range = every normal TxnId >= G
```

The guard is held through every status-dependent scan/recheck in that pass.
While it exists, a reclaimer may not publish a cutoff greater than `G`. With
several guards, the greatest new cutoff is bounded by the minimum guard value.
Vacuum never asks the guard to restore history below `G`: metadata below `G`
must already be frozen/normalized/status-independent under §14.14.
A pass that produced new freeze evidence releases its guard before requesting a
cutoff advance; the one global reclaimer then recomputes the proof and does not
special-case the former pass's own guard.

Exactly one reclaimer computes and publishes a cutoff. Under the same
coordinator serialization, before advancing to page-aligned `C` it must account
for the crash-recoverable persistent status-independence proof, registered SQL snapshots,
every active write-status dependency, other explicitly registered status-correctness
dependencies, every active StatusHistoryGuard, and any retained recovery/checkpoint
correctness dependency.
A write-capable transaction's registered dependency MUST prevent `C` from retiring its own
TxnId; the reclaimer chooses the greatest legal page-aligned cutoff satisfying that and all
other bounds. Transaction existence without this dependency does not itself constrain
status retirement. Only an explicitly read-only transaction, which is incapable of
creating a persistent status dependency, may omit the dependency. A StatsVersion
occurrence is not a dependency.

The coordinator's semantic phases are candidate calculation, dependency/guard exclusion,
status-independence mutation and revalidation, §14.14.2 recovery-safety proof, durable
control publication, matching runtime publication, and physical reclamation. Before the
control phase, the reclaimer distinguishes already-durable page states from WAL-dependent
states, establishes durability through the maximum required authorizing record-start LSN,
and confirms that DPT/checkpoint retention cannot recycle the reconstruction source while
any page still depends on it. Failure to establish WAL durability or retention abandons
the candidate before control publication; it does not weaken the old cutoff or undo
already-authorized maintenance progress.

The coordinator excludes new guard registration and write-capable transaction registration
from the final proof through §14.14.2 steps 2–3 (durable control publication and matching
runtime cutoff publication). Transaction registration and cutoff publication obey §9.8's
gap-free ordering: a registered dependency first clamps the cutoff, while a cutoff `C`
first causes a subsequently registered write-capable transaction to receive a TxnId at or
above `C`. BufferPool retirement and optional hole punching then proceed under §14.14.2
without holding the registration coordinator.

The race is exact:

```text
guard registers first:
    C is clamped to <= G until guard release

reclaimer publishes C first:
    a later guard observes G=C and may use only status at/above C; it must
    re-evaluate/fail rather than assume older history exists

write-capable transaction T registers first:
    C cannot retire T while write-status dependency(T) remains registered

reclaimer publishes C first:
    a later write-capable transaction receives T >= C and registers its
    write-status dependency before admission
```

An idle write-capable READ COMMITTED transaction therefore pins its own status even when it
has no SQL snapshot. An explicitly read-only transaction may omit this claim, while any SQL
snapshot it registers continues to constrain the independent logical reclamation horizon.
`StatusHistoryGuard` remains a bounded maintenance lookup-range claim; it is not replaced by
the one-TxnId transaction-lifetime write-status dependency. Neither mechanism is a
ReadEpoch, which protects physical RID identity rather than transaction-status history.

Status lookup absence/`RETIRED` is never interpreted as ABORTED or COMMITTED.
A below-cutoff tuple field that is frozen or otherwise canonically
status-independent is valid and needs no lookup. A below-cutoff `xmin`/effective
`xmax` that still requires status is the §9.13 reclamation invariant violation:
the pass fails without mutation or guesswork and the database becomes
noncontinuable because required transaction outcome can no longer be proven.

#### Compatibility matrix

The terms below are semantic: `SERIALIZED` means one owner waits for the other;
`WAIT/REVALIDATE` means the winner is determined at the named gate and the
loser waits, cancels, or rechecks before publication; `INCOMPATIBLE` forbids new
admission.

| Operation A | Operation B / scope | Result | Required reason |
|---|---|---|---|
| VACUUM | VACUUM, same table | `SERIALIZED` | one `TableVacuumOwner`; second waits and revalidates |
| VACUUM | VACUUM, different tables | `CONCURRENT` | independent table ownership; narrow shared epoch/status synchronization only |
| VACUUM | CREATE INDEX, same table | `WAIT/REVALIDATE` at catalog publication | §14.17.1 publication exclusion drains the admitted pass and blocks new passes through terminal DDL disposition; private build may overlap |
| VACUUM | ANALYZE, same or different table | `CONCURRENT` | MVCC snapshot + page/ReadEpoch protection; ANALYZE publication separately revalidates |
| ANALYZE | ANALYZE, same or different table | `CONCURRENT` | immutable distinct StatsVersions; shared publication claims and monotonic cache install |
| VACUUM | DROP TABLE or a required DROP INDEX | `WAIT/REVALIDATE` | DROP marks RETIRING and cancels at a vacuum unit boundary; unlink waits for claims/guards |
| ANALYZE | DROP TABLE or manifest DROP INDEX | `WAIT/REVALIDATE` | `STATS_PUBLISH` versus RETIRING linearization; entire stale manifest is discarded |
| ANALYZE | CREATE INDEX or another applicable schema/index-manifest publication | `WAIT/REVALIDATE` | the same publication gate makes either ANALYZE's immutable manifest or the DDL publish first |
| TXN_STATUS_RECLAIM | VACUUM, any table | `WAIT/REVALIDATE` | guard/cutoff coordinator orders history use and cutoff publication |
| TXN_STATUS_RECLAIM | TXN_STATUS_RECLAIM | `SERIALIZED` | one global monotonic cutoff owner |
| TXN_STATUS_RECLAIM | foreground SQL | `CONCURRENT` | SQL snapshots, write-status/correctness dependencies, and status-page pins are included before cutoff/page retirement |
| VACUUM | foreground SELECT/INSERT/UPDATE/DELETE | `CONCURRENT` | online reclamation predicate, revalidation, latches, and ReadEpoch protocol |
| ANALYZE | foreground DML/SELECT | `CONCURRENT` | one fixed SQL snapshot; concurrent changes make statistics stale, not incorrect |
| DROP TABLE/INDEX | target foreground DML | `SERIALIZED` | existing SchemaLock/TableWriterGate transaction protocol |
| VACUUM/ANALYZE/status reclaim | fuzzy checkpoint | `CONCURRENT` | ordinary WAL/DPT/checkpoint rules serialize physical details |
| any new maintenance | `RECOVERING`, `DRAINING`, `CLOSING`, or `NONCONTINUABLE` | `INCOMPATIBLE` | database lifecycle admission is not READY |

#### Lifecycle, failure, and lock ordering

On `READY -> DRAINING`, no new maintenance is admitted. VACUUM, status reclaim,
statistics GC, and cleanup finish or restore their current publication unit and
quiesce; an ACTIVE ANALYZE is canceled and follows the ordinary abort handling
in §39.1 and Chapter 3, while already-COMMITTING or ABORTING ANALYZE transactions complete
terminal publication. Namespace cleanup already initiated by shutdown completes
under §3.3.6. During `RECOVERING` no normal maintenance exists. At
`DATABASE_NONCONTINUABLE`, all new maintenance mutation and publication stops;
workers only release safe volatile ownership for non-clean teardown, and no
vacuum, ANALYZE row publication, status-cutoff advance, statistics GC, or new
cleanup write continues.

During drain, every existing write-capable transaction retains its write-status dependency
through its ordinary C5/A3 cleanup, and any concurrent status reclaimer remains constrained
by those registrations. Shutdown introduces no separate early-release path.

A VACUUM pass is a sequence of system-maintenance publication units. An exactly
restored provisional unit may fail the pass safely; earlier successful WAL-backed
units remain valid and idempotent. Storage uncertainty escalates under §12.12.
ANALYZE is transactional and follows §39.1. A status cutoff durable at
§14.14.2 step 2 never moves backward; failure before durable publication leaves
the old cutoff, while inability to establish matching runtime state after it is
durable is noncontinuable. Retirement unlink/sync failure leaves semantic DROP
committed and cleanup pending under §4.7.

Conceptual acquisition/order rules are:

```text
database READY admission
    -> resolve stable IDs/descriptors
    -> OBJECT_USE claims (table, then IndexIds ascending)
    -> TableVacuumOwner when VACUUM, ordered with CREATE INDEX admission exclusion
    -> resolve/revalidate the complete committed pass manifest after admission
    -> StatusHistoryGuard before status-dependent vacuum scanning
    -> BufferPool/read-epoch guards
    -> short heap/B+ latches

ANALYZE final publication:
    no page/B+ latch
    -> STATS_PUBLISH table claim
    -> STATS_PUBLISH IndexId claims ascending
    -> current-object revalidation
    -> catalog row publication

CREATE INDEX publication:
    SchemaLock -> target TableWriterGate exclusive -> MANIFEST_CHANGE
    -> close target mutating-VACUUM admission and drain admitted pass
    -> revalidate manifest -> catalog publication
    -> retain admission exclusion until C5/A3

DROP:
    existing SchemaLock
    -> existing TableWriterGate exclusive where required
    -> MANIFEST_CHANGE table/index scope
    -> object RETIRING transition/publication ordering
```

Every transaction-owned SchemaLock, TableWriterGate, STATS_PUBLISH, and
MANIFEST_CHANGE wait follows §11.13's one graph and deterministic victim rule.
No operation waits for a long schema/object/maintenance/status/logical lock
while holding a heap/B+ page latch, BufferPool frame transition, or read-epoch
guard. Maintenance is best-effort:
the architecture promises no starvation-free scheduler. Delay may retain disk/status history
or degrade plans, but correctness never depends on prompt VACUUM or ANALYZE.

#### Required race outcomes

1. ANALYZE scans, DROP marks/commits retirement, then ANALYZE seeks publication:
   the claim/revalidation fails and no generation publishes.
2. ANALYZE obtains all publication claims first: DROP waits; ANALYZE reaches
   terminal commit/abort, releases claims, then DROP may retire.
3. Two ANALYZE operations finish/cache-install in reverse wall-clock order: both
   complete committed generations remain valid catalog history, but selection
   and cache retain the greater applicable StatsVersion and never regress.
4. ANALYZE commits, its cache callback is delayed, DROP commits, then the callback
   runs: current-object recheck rejects the callback; statistics are not
   reinstalled.
5. Vacuum guard wins before a cutoff attempt: the cutoff cannot pass its `G`.
6. Reclaimer publishes first: the vacuum guard starts at the new cutoff and may
   not request retired history.
7. Vacuum sees a frozen/status-independent tuple below cutoff: it proceeds without
   lookup. Seeing a status-dependent one is a noncontinuable invariant failure.
8. VACUUM owns an object when DROP marks RETIRING: the current atomic unit
   completes/restores, VACUUM cancels and drains, and physical unlink waits.
9. DROP marks RETIRING before VACUUM admission: the pass is not admitted.
10. DROP commits while VACUUM still has pages/guards: logical DROP is effective,
    but those physical files remain linked/owned until all claims and guards drain.
11. A write-capable transaction registers before a cutoff attempt: the greatest legal
    page-aligned cutoff cannot retire that transaction's TxnId.
12. A cutoff `C` publishes before a new write-capable transaction registers: its newly
    assigned TxnId is at least `C`, and its dependency is registered before admission.
13. An idle READ COMMITTED write-capable transaction with no SQL snapshot still retains its
    own write-status dependency.
14. C4/A2 terminal publication alone does not remove the dependency; C5/A3 does, after
    which already-persistent references independently continue to pin status history.
15. A writer holds a read epoch and has not registered TUPLE_WRITE: RID reuse remains
    blocked by the epoch.
16. TUPLE_WRITE registration completes before epoch release: both claims overlap, then the
    queued or granted request alone blocks reuse.
17. A queued TUPLE_WRITE waiter sleeps: its live request blocks `DEAD -> UNUSED` and
    same-RID reuse.
18. A granted TUPLE_WRITE owner retains the claim through C5/A3; C4/A2 is insufficient.
19. TUPLE_WRITE cancellation/removal ends the claim only when grant becomes impossible;
    no removed waiter can wake as owner of the old RID.
20. Epoch grace with a remaining TUPLE_WRITE claim, or zero lock claims with an old epoch,
    is insufficient; every independent barrier must clear.
21. A process crashes with queued/granted TUPLE_WRITE requests: requests and callers vanish,
    no lock state is replayed, and persistent DEAD plus remaining restart barriers govern reuse.

#### Forbidden maintenance implementations

The architecture explicitly forbids:

1. two uncoordinated mutating vacuum passes on one table;
2. advancing status cutoff past an active vacuum history dependency;
3. interpreting missing/reclaimed status as ABORTED or COMMITTED;
4. publishing ANALYZE statistics after the stable TableId/schema/manifest object was dropped or replaced;
5. using a table/index name or StatsVersion as object-liveness proof;
6. allowing an older/delayed ANALYZE callback to replace a newer StatsVersion or reinstall after DROP;
7. unlinking table/index files while maintenance claims, descriptors, guards, frames, or file owners remain;
8. admitting new maintenance after an object is RETIRING or the database leaves READY;
9. changing a statistics generation manifest during publication;
10. relying on prompt VACUUM for UNIQUE correctness;
11. reusing a RID before all stale index references and read epochs satisfy Chapter 14;
12. running normal maintenance during recovery or continuing mutation after noncontinuable publication;
13. waiting for maintenance/schema/status/logical ownership while retaining short page/B+ latches;
14. physically deleting old statistics without ordinary catalog MVCC/snapshot safety.
15. placing STATS_PUBLISH waits in a graph that cannot see SchemaLock, TableWriterGate, manifest-change, tuple-lock, or unique-key dependencies;
16. treating a deadlock victim as permission to release publication/retirement ownership before §9.14 terminal publication.
17. advancing status cutoff past a registered write-capable transaction's own TxnId;
18. publishing persistent `xmin`/`xmax` for a transaction whose write-status dependency is absent or already released.
19. rebinding a RID while any queued grant-eligible or granted TUPLE_WRITE request still names its old identity.

## 14.18 Vacuum/reclamation invariants

1. The global reclamation horizon is derived from registered SQL snapshots, not transaction existence.
2. Vacuum never uses only its own snapshot visibility as the garbage criterion.
3. In-progress creator/deleter state is not reclaimed by guesswork.
4. Required secondary-index entries are known absent before `NORMAL -> DEAD`.
5. Vacuum exact-index removal is idempotent across crash/retry.
6. `DEAD` is persistent and means semantically cleaned but not physically reusable.
7. Reader epoch registration occurs before an executor may retain/index-consume a stored RID.
8. RID retirement increments the process epoch and records the pre-increment retirement epoch.
9. `DEAD -> UNUSED` is legal only when no active reader epoch is `<= retire_epoch`.
10. `DEAD -> UNUSED` also requires proof that all surviving direct-successor version-chain links have been spliced away from that RID.
11. Whole heap-page recycling cannot bypass RID-reuse safety.
12. A surviving version never keeps `prev` pointing at storage that may be reused.
13. Payload compaction occurs while the slot is DEAD; the final reusable UNUSED slot has zero coordinates.
14. Every reusable UNUSED slot appears exactly once in the persisted free-slot list.
15. Candidate state is revalidated under page latch before physical transition.
16. Vacuum never waits for ordinary transactions or performs B+ cleanup while holding the heap-page latch.
17. Aborted `xmax` cleanup does not require an index change.
18. Freezing is WAL protected and makes original creator status unnecessary for future visibility.
19. The transaction-status reclaim cutoff advances durably before any old status-page hole is punched.
20. Sparse status reclamation preserves absolute status PageNos and file length.
21. B+ cleanup remains a B+ MTR system action.
22. FSM/statistical maintenance cannot weaken reclamation correctness.
23. Status retention is pinned by fields that still require transaction-outcome
    lookup, not by opaque numeric occurrences such as a published StatsVersion.
24. Exactly one mutating vacuum pass owns a TableId at a time; different tables may vacuum concurrently.
25. Every maintenance mutation is authorized by a live stable-ID object claim, not merely an open file or cached descriptor.
26. Status-cutoff publication cannot pass an active StatusHistoryGuard.
27. ANALYZE publishes only after current table/schema/index-manifest revalidation under a terminal-lifetime publication claim.
28. Statistics cache install is monotonic by applicable object identity and StatsVersion and cannot reinstall after DROP.
29. DROP may become semantically committed before maintenance owners drain, but physical unlink waits for every claim/guard/owner.
30. Status-cutoff publication cannot retire the TxnId of a registered write-capable transaction that may still publish that TxnId into persistent correctness metadata.
31. A TxnId may enter the retired range only after both its runtime future-publication blocker and every existing persistent status dependency are absent.
32. A durably published status cutoff guarantees that every below-cutoff persistent dependency is already durable in status-independent form or reconstructable before READY from durable retained WAL.
33. WAL/DPT recovery dependencies used by the cutoff proof remain retained until the corresponding page state no longer depends on them; cutoff publication alone never releases them.
34. Every live queued or granted TUPLE_WRITE request is a runtime physical RID-retention claim for its exact `(TableId,RID)`.
35. Discovery read-epoch protection overlaps TUPLE_WRITE request registration; the epoch is released only after the claim exists and before any blocking wait.
36. `DEAD -> UNUSED` and same-RID rebinding require both read-epoch grace and zero live TUPLE_WRITE claims, in addition to every index, predecessor-link, and page/slot prerequisite.
37. Request registration and RID reuse have one gap-free ordering: registration first blocks reuse, while reuse first prevents admission of an old-identity request.
