# 3. Platform and Runtime Baseline

## 3.1 Supported platform

The supported platform is:

- Linux,
- x86-64 or ARM64,
- POSIX file APIs,
- one actively exclusive database process per opened database root,
- multiple worker threads.

Portability is desirable but secondary to correctness, observability, and direct access to the required operating-system mechanisms.

The Linux-first baseline permits experimentation with facilities such as `io_uring`, direct I/O, huge pages, and NUMA-aware designs, but such experimentation does not make those facilities part of the required platform contract.

## 3.2 Implementation language

The implementation MUST use C++20. Compiler-specific extensions are not part of the architectural language baseline unless an explicit architecture revision introduces one.

C++20 was selected because it provides direct control over:

- object lifetime and ownership,
- memory layout,
- allocation behavior,
- cache-local data structures,
- atomics and synchronization,
- explicit byte manipulation,
- POSIX/Linux I/O,
- low-level profiling and optimization.

The design accepts the associated risks—undefined behavior, memory-safety failures, accidental allocation/copying, and difficult concurrency bugs—as engineering risks that must be controlled by explicit ownership, serialization, validation, testing, and sanitizer/tooling discipline.

## 3.3 Database process lifecycle

This section is the canonical database-owner lifecycle. It owns process
exclusivity, open/recovery admission, READY publication, controlled shutdown,
failed-open cleanup, and the database-level response to the noncontinuable gate
from §§12.12.4 and 39.1. It does not change the local recovery, BufferPool,
transaction-terminal, catalog, or filesystem-publication protocols that it
orders.

### 3.3.1 Runtime states and legal transitions

Each process-local database owner is in exactly one conceptual state:

| State | Meaning and admitted work |
|---|---|
| `CLOSED` | No process-local database ownership, manager, worker, frame, guard, file registration, or exclusive lock remains. |
| `OPENING` | The final root has been opened and exclusive ownership is being acquired/held while bootstrap prerequisites and recovery inputs are validated. Only lifecycle-owned open/cleanup work is admitted. |
| `RECOVERING` | The exclusive owner and recovery-scoped managers exist; recovery has sole authority to inspect/repair persistent state. No ordinary transaction, SQL, DDL, ANALYZE, vacuum, normal checkpoint/writeback worker, or ordinary catalog publication is admitted; recovery-authorized page I/O/checkpoint work remains legal. |
| `READY` | Every §3.3.4 prerequisite has been published atomically and ordinary transaction/statement admission is enabled. |
| `DRAINING` | The shutdown admission gate has closed. No new ordinary transaction, statement, maintenance operation, or optional background task is admitted. New ordinary file/page work is rejected, while already-admitted terminal/cancellation and lifecycle shutdown protocols may still perform their required internal file/page/WAL operations under §3.3.6. |
| `CLOSING` | No ordinary work remains; the owner is executing final durability and/or resource teardown. It never readmits work. |
| `NONCONTINUABLE` | A database/storage invariant or outcome cannot support safe ordinary continuation. Admission is closed and only failure propagation, quiescence, and non-clean owner teardown/recovery preparation are legal. |

`OPEN_FAILED` and `SHUTDOWN_FAILED` are operation results, not additional
long-lived states. A failed open whose resources can be fully unwound ends in
`CLOSED`; an owner whose safe teardown is not yet complete remains
`NONCONTINUABLE`. A failed close may eventually reach `CLOSED` through non-clean
teardown, but that does not convert its result into a successful controlled
close or imply any clean-shutdown marker.

The normative transition table is:

| From | Trigger / required gate | To | Exclusive-lock and failure outcome |
|---|---|---|---|
| `CLOSED` | `OpenDatabase` safely opens the final root, claims the process-local root identity, and begins lock acquisition | `OPENING` | no database content is trusted; lock is acquired before validation/recovery |
| `OPENING` | lock held and bootstrap/recovery dependencies are sufficiently opened and validated | `RECOVERING` | lock remains held; recovery owns all mutation |
| `OPENING` | known validation/open failure and complete cleanup | `CLOSED` | all database users/descriptors except the lock descriptor stop first; then the OS lock is released, the process claim is removed, and future open may retry |
| `OPENING` | ownership, I/O outcome, worker stop, or cleanup cannot be established | `NONCONTINUABLE` | lock remains held by the cleanup owner |
| `RECOVERING` | §§13.11–13.19 and every §3.3.4 READY prerequisite complete | `READY` | READY/admission publication is one process-local linearization while the lock remains held |
| `RECOVERING` | known recovery/corruption failure and complete non-live cleanup | `CLOSED` | no handle is published; future open may retry/re-report the persistent failure |
| `RECOVERING` | uncertain mutation/publication state or incomplete quiescence/cleanup | `NONCONTINUABLE` | ordinary work remains forbidden and the lock stays held |
| `READY` | controlled close wins the admission gate | `DRAINING` | lock remains held; only already-admitted completion/cancellation and shutdown work continue |
| `READY` | database-fatal transition under §§12.12.4 and 39.1 | `NONCONTINUABLE` | admission closes immediately; durable transaction outcomes are preserved |
| `DRAINING` | §3.3.6 steps 1–3 complete: admitted work and ordinary producer services have quiesced | `CLOSING` | publish this transition before step 4; final durability/resource teardown continues with the lock held |
| `DRAINING` | transaction/producer quiescence or shutdown-invariant failure during §3.3.6 steps 1–3 | `NONCONTINUABLE` | successful close is forbidden |
| `NONCONTINUABLE` | owner starts non-clean quiescence/resource destruction | `CLOSING` | no final-checkpoint/clean-state claim; lock remains held until teardown's last step |
| `CLOSING` | every normal §3.3.6 close prerequisite completes | `CLOSED` | managers/descriptors close first; OS lock release is the external ownership transition, then the local claim/state publish completes; report success |
| `CLOSING` | non-clean teardown safely releases all process-local users | `CLOSED` | use the same final ownership-release order and report the original open/shutdown/noncontinuable failure, never success |
| `CLOSING` | final durability/resource-teardown failure under §3.3.6, including a live guard/worker/ownership invariant that prevents safe destruction | `NONCONTINUABLE` | successful clean close is forbidden; retain the owner and lock until non-clean teardown can safely proceed or process termination releases OS resources |

Only `READY` admits normal work. APIs and diagnostics MUST expose enough state to
distinguish not-open/closed, opening, recovering, ready, draining, and
noncontinuable; they may also expose closing. An operation cannot silently run
against an owner that has not reached `READY`.

### 3.3.2 Active process exclusivity and lock object

The architecture actively enforces one process-local database owner and one operating-system
process owner for one actual database root. The same ownership authority governs
creation publication, ordinary open, and offline whole-root removal. It never
permits independent WAL managers, BufferPools, file registries, recovery
coordinators, catalog caches, publication owners, or removal owners for that
root.

The lock object is the existing regular file `database_root/database.control`.

An ordinary opener or remover opens the final root with §4.7 no-follow,
directory-relative rules. A creation-publication owner uses the already
validated staging root before its final rename. Each opens `database.control`
read/write as a regular file with no-follow and close-on-exec and obtains a
nonblocking exclusive whole-file POSIX
`fcntl(F_SETLK)` write lock with `l_whence=SEEK_SET`, `l_start=0`, and `l_len=0`.
The lock is process-associated. It is OS lock state, not a byte, slot, flag, or generation
inside `database.control`; the control-file format is unchanged. There is no
lock file whose mere directory-entry existence denotes ownership.

For an ordinary open or offline removal, only opening/fstat/type-checking the
final root and `database.control`, process-local ownership reservation, and the
remover's preceding §4.7.10 lifecycle namespace-gate acquisition may occur before
the control-lock attempt. The engine
MUST NOT read/choose control slots, inventory managed objects/WAL, initialize
storage managers, run recovery, or mutate database state before the lock
succeeds. Failure because another process owns the lock returns a distinct
nonwaiting `DATABASE_BUSY` result and performs none of those operations. The
creator's private staged bootstrap and validation precede its ownership attempt
under §4.7.8 and are not ordinary inspection of a final database root.

Because process-associated record locks do not conflict with another open by
the same process, a synchronized process-local registry keyed by stable identity
of the actual opened root/control inode is also mandatory. Creation publication
claims the validated staging-root/control inode pair before final rename;
ordinary open and offline removal claim the retained final-root/control pair. A
second operation for the same root, including a path alias, fails
`DATABASE_BUSY`; the architecture does not coalesce handles and does not permit two independent
owner objects. Path strings alone are not owner identity. All managed access
remains relative to retained opened directory descriptors, so later path
rebinding does not move the ownership domain.

The same-filesystem root renames in §§4.7.8–4.7.9 preserve the root-directory
inode, `database.control` inode, retained descriptors, process-local stable
identity claim, and existing control lock. Only namespace classification changes
from staging to final or from final to retired. A final-path opener after a
creation rename therefore reaches the same locked control inode. The separate
§4.7.10 lifecycle namespace gate coordinates creation and cleanup before control
ownership is available; it does not replace or split database ownership. These
protocols introduce no persistent publication/removal marker or additional
DatabaseInstance state.

After an independent opener or remover acquires the final-root claim and lock,
it MUST retain the external parent, verify that the final entry still identifies
the retained root inode, successfully `fsync` that parent, and revalidate the
entry/identity against path rebinding before interpreting control contents beyond
the minimum identity needed for this adoption, inventorying WAL, recovering,
mutating/removing the database, or publishing READY. This adoption prerequisite
conservatively establishes final-root namespace durability after a prior owner
may have died between root rename and parent synchronization. The owner lock is
only the coordination/admission gate; external-parent `fsync` is the durability
mechanism.

The control descriptor that owns the lock is the sole independently opened
descriptor for that control inode in the owner process. All control reads and
alternating-slot writes borrow that descriptor. This avoids the POSIX rule that
closing another descriptor for the same inode can release the process's record
locks. The owner holds the lock continuously through `OPENING`, `RECOVERING`,
`READY`, `DRAINING`, `CLOSING`, and `NONCONTINUABLE`. It releases the lock only
after all workers, guards, BufferPool frames, file registrations, managers, and
other descriptors that can use database state are gone. The process-local claim
remains held while the OS lock is released/its descriptor is closed, preventing
a same-process opener from exploiting process-associated lock semantics; only
then is the claim removed and `CLOSING -> CLOSED` published.

Creation-publication ownership is held from §4.7.8's pre-rename gate through
durable final-root publication and either continues into the returned
DatabaseInstance or is released in the same lock-before-claim order before a
no-handle create returns. Offline removal ownership is held through §4.7.9's
durable retirement publication; private reclamation may retain it longer.

Process termination releases the POSIX lock automatically. Record locks are not
inherited as owned locks by a forked child, and close-on-exec prevents descriptor
inheritance through successful `exec`. Using a live database owner from a
post-`fork` child is unsupported: the child MUST NOT execute database APIs using
copied handles and must close/discard inherited descriptors or `exec`. A child
that independently opens the same database still conflicts with the parent's
lock. No fork-support or ownership-transfer protocol exists in the defined architecture.

### 3.3.3 Open preconditions, ordered protocol, and recovery entry

`OpenDatabase(path)` accepts only the exact final database root. A missing path,
non-directory, recognized `.dblusblus-creating` staging root, recognized
`.dblusblus-removing-<token>` retired root, missing
`database.control`, or incomplete bootstrap is not opened as a database. Managed
directories/files obey §4.7's object-type, no-follow, same-filesystem, permission,
and stable-directory-descriptor rules. The engine does not follow a symlink or
substitute a similarly named staging/private object.

The architecture has no persisted clean-shutdown bit and does not infer cleanliness from a
successful prior close. Every open runs the bounded WAL-tail validation,
analysis, redo, loser resolution, transaction-status repair, and recovery
checkpoint protocol in §§13.11–13.19. A successful final shutdown checkpoint
reduces that work but never authorizes skipping it. No persisted-format field is
introduced for this lifecycle decision.

The selected filesystem must also provide reliable interprocess POSIX record
locking for the control inode. Unsupported/broken lock service is an open
I/O/platform failure; the engine never degrades to convention-only ownership.

The ordered open protocol is:

| Step | Required action and dependency |
|---:|---|
| 1 | Open and identify the caller's final root under §4.7; reject staging roots and unsafe object types. |
| 2 | Atomically claim the process-local root identity, open `database.control`, and acquire §3.3.2's nonblocking exclusive lock. On contention, release the claim/descriptor and return `DATABASE_BUSY` without recovery. |
| 3 | Perform §3.3.2's independent-owner root-adoption barrier: retain the external parent, verify its final entry against the retained root inode, successfully synchronize the parent, and revalidate/reconcile identity before control-slot interpretation, WAL inventory, recovery, or database mutation. |
| 4 | Retain root, `pending/`, and `wal/` directory descriptors and inventory only exact managed names. Unknown names are recorded/ignored, never deleted by guesswork. |
| 5 | Validate both control slots independently under §13.2, the immutable `catalog.dat` bootstrap locator/header under §16.9, the `txn_status.dat` superblock, and bootstrap-referenced system heap/FSM identities needed to address recovery. Data/status/catalog pages that WAL may repair are opened in recovery-private form and are not published to ordinary readers. |
| 6 | Inventory exact WAL segment names, consider usable control generations in descending order, validate the selected checkpoint and required contiguous retained segment range, and establish that enough recovery input exists to enter recovery. |
| 7 | Construct only recovery-scoped DiskManager/file registry, WAL, BufferPool/page-reconstruction, checkpoint, transaction-status, and catalog-bootstrap services; do not start normal background services. Transition `OPENING -> RECOVERING`. |
| 8 | Establish the valid contiguous WAL prefix and reconcile its tail, then run analysis and redo for bootstrap-addressable status/catalog/system state under Chapter 13. Keep redo for not-yet-classified non-bootstrap FileIds deferred as §13.13.1 requires. Raw transaction-status data is not an ordinary lookup authority during this step. |
| 9 | Resolve losers and repair terminal status, decode the recovered self-hosted catalog rows through the exact §16.5 schema-v1 descriptors, perform §16.5.9–§16.5.10 fixed-point/reference validation, require every bootstrap/committed catalog-owned file at its exact final basename with valid identity/superblock, and then apply/finish any deferred redo and append-tail reconciliation for required files. |
| 10 | Durably complete/install the required recovery checkpoint, including control-slot publication, after all required redo/status/file reconciliation and before enabling ordinary status/catalog use. |
| 11 | Classify every exact managed `pending/` entry and unowned exact managed final file under §4.7.6. Classification is mandatory before READY; durable physical unlink may be completed now or retained as an explicit cleanup task because catalog/bootstrap lookup cannot expose the orphan. Unknown names remain untouched. |
| 12 | Enable ordinary transaction-status lookup, construct immutable catalog/schema and required runtime terminal/ownership caches, finish normal BufferPool/file registrations, and construct background services behind a closed start gate. |
| 13 | Atomically publish `RECOVERING -> READY`, open transaction/statement admission, and release the background-service start gate. The exclusive process lock remains held. |

The immutable catalog locator and bootstrap-required identities are pre-recovery
addressing roots, not ordinary catalog state. The self-hosted catalog rows become
semantic authority only after their physical pages have been recovered and the
post-recovery cross-check succeeds. Likewise, `txn_status.dat` may contain a
torn/corrupt recoverable page before the recovery protocols in §§12.10.5,
13.13.2, and 13.17; ordinary snapshot/visibility
lookup is enabled only after recovery status repair and loser resolution.

Control-file lifecycle outcomes are exact:

- each slot is accepted/rejected independently under §13.2.3;
- with two valid unequal generations, the highest usable generation wins; one
  valid slot is sufficient; equal-generation differing slots are corruption;
- a checksum-bad slot is simply not a valid candidate, but absence of any valid
  supported slot prevents open;
- a control slot or immutable bootstrap root with the correct format-family
  magic but an unsupported version returns the family-specific
  `UNSUPPORTED_DATABASE_FORMAT`/`UNSUPPORTED_CATALOG_SCHEMA` result and forbids
  fallback to an older supported slot; an unknown version is not treated as an
  ignorable torn update;
- invalid magic/incomplete bootstrap returns
  `NOT_A_DATABASE` when no database format can be established, otherwise
  `CORRUPT_DATABASE`;
- a selected checkpoint whose required sequence/WAL is unavailable may fall
  back only to another independently valid control generation whose complete
  recovery range remains valid; otherwise open fails `RECOVERY_FAILED` or
  `CORRUPT_DATABASE` and never reaches READY.

Required-file classification is:

| Class | Open rule |
|---|---|
| final root/control/directories and immutable bootstrap/status/catalog addressing roots required before recovery | absence, wrong type, unsupported format, or unrecoverable identity failure prevents recovery/READY |
| bootstrap system files named by the immutable locator | must be openable for recovery and valid after recovery; absence is corruption, not an orphan case |
| files required by recovered committed catalog state | may be classified only after recovery/catalog reconstruction, but each must then exist under its exact durable final name and validate before READY |
| exact managed private/final files proven unowned | orphan cleanup input; never semantic evidence and never opened as committed state |
| catalog-retired files pending lifetime cleanup | not required by current committed catalog; remain inaccessible and follow retirement/orphan cleanup |
| unrelated/unknown names | never treated as database objects or automatically deleted |

WAL startup accepts only §12.2's exact segment grammar and §13.11's contiguous
valid record prefix. An incomplete/torn final record is excluded with the tail;
malformed required earlier WAL or a missing interior required segment is
corruption. One exact next-contiguous all-zero precreated segment is only the
permitted empty artifact and may be adopted or durably removed through existing
rules. Older segments may be ignored/recycled only after the installed retention
floor proves them unnecessary. No control/checkpoint state is accepted unless
its complete required WAL range exists and validates.

### 3.3.4 READY publication and failed-open cleanup

The READY linearization may occur only when all of these are true:

```text
exclusive OS lock and process-local owner claim held
retained final-root entry verified and external-parent namespace durability established after ownership acquisition
stable managed directory/file ownership established
valid control/checkpoint and contiguous WAL prefix established
analysis, all required/deferred redo, append-tail reconciliation, and loser resolution complete
recovery checkpoint durable and installed
transaction-status state safe for ordinary lookup
immutable bootstrap and recovered self-hosted catalog cross-checked
every bootstrap/committed physical file present and validated
all managed pending/final survivors classified as required or orphan/retired
normal BufferPool/file registry/terminal/catalog services coherent
normal background services constructed but unable to race before the gate
```

Before that one publication, no transaction/statement API or background service
may observe a partly recovered database. After it, every admitted component sees
the same recovered ownership/status/catalog state.

A failed open whose outcome and process-local ownership are fully known and quiesceable
uses scoped ownership to stop and join all started recovery tasks, quiesce and destroy a
partial BufferPool before closing its files, close every registration/descriptor except
the lock descriptor, release and close the OS lock, and only then remove the process-local
identity claim. No owner is exposed as READY and no worker survives failed-open return.

A known validation/corruption/recovery error whose process-local state can be
fully quiesced ends in `CLOSED`; a later independent open may retry and either
recover or report the same persistent defect. Recovery writes that were validly
WAL-backed/durable remain ordinary recovery input for that later attempt. If an
append/publication outcome is uncertain, exact provisional restoration failed,
or a worker/guard/manager cannot be proven stopped, the temporary owner enters
`NONCONTINUABLE` and retains the lock until non-clean teardown or process exit.
An orphan unlink/sync failure alone does not prevent READY when the entry has
already been authoritatively classified unowned and remains catalog-inaccessible;
after §4.7.2 reconciliation, every possible namespace outcome remains that same
unowned cleanup input and its §4.7 cleanup task is retained/retried. Failure of a synchronization required
for WAL, status repair, checkpoint/control installation, or a required namespace
does prevent READY.

### 3.3.5 Database-noncontinuable behavior

Any `DATABASE_NONCONTINUABLE` transition under §§12.12.4 and 39.1 atomically closes the
database admission gate. No new transaction, statement, maintenance operation,
ordinary page/file load, WAL append, page writeback, checkpoint, or ordinary
mutation may start. In-flight work is canceled/failed at safe ownership
boundaries; protected provisional pages/barriers are not released as ordinary
state. No new successful COMMIT acknowledgement may be sent beyond an
acknowledgement whose complete §15.5 C6 boundary already occurred.

Transactions whose `TXN_COMMIT` is already durable remain COMMITTED forever,
including when terminal/cache/shutdown completion later fails. Transactions
without a surviving complete commit record remain recovery losers. The owner
preserves those facts, reports the causal failure plus database-noncontinuable
state, stops ordinary connections, and enters the non-clean `CLOSING` path.

Non-clean close does not flush uncertain protected frames, install a final
checkpoint, recycle WAL, or claim clean durability. It stops and joins components in
dependency order where possible, discards only volatile state after no observer
can use it, closes descriptors, and releases exclusivity last. If a live guard or
worker prevents safe destruction, the owner and lock remain in
`NONCONTINUABLE`; a second process is not allowed to recover concurrently. The
next owner after release always runs the full §3.3.3 recovery protocol.

### 3.3.6 Controlled shutdown protocol

Controlled close begins at one `READY -> DRAINING` admission linearization.
After it wins, new transactions, statements, COMMIT requests not already in
`COMMITTING`, maintenance work, ordinary file opens/page loads, and scheduled
checkpoint/flush producers are rejected. Already-admitted COMMIT/ABORT and
lifecycle cleanup may still use their required internal BufferPool/WAL paths
until the later BufferPool quiesce in step 4. Steps 1–3 execute in `DRAINING`.
Successful completion of step 3 publishes `DRAINING -> CLOSING` before step 4;
steps 4–8 execute in `CLOSING` unless a failure enters `NONCONTINUABLE`. Exact
shutdown ordering is:

| Step | Required action and dependency |
|---:|---|
| 1 | Publish `DRAINING`; stop admission and prevent new maintenance/checkpoint work from starting. |
| 2 | Request cancellation of admitted ordinary statements/maintenance at safe points and wait for their operation-local ownership to unwind. Drive every `ACTIVE` or `MUST_ABORT` transaction through §15.6 ABORT. Allow already-`COMMITTING` and already-`ABORTING` terminal protocols to complete under §39.1; a commit past its publication-authorizing append remains uncancellable. Wait for terminal publication, snapshots, logical locks, writer gates, and statement resources to release. |
| 3 | Stop/join vacuum, ANALYZE/statistics, status-reclamation/cleanup, scheduler/query workers, and ordinary checkpoint production under §14.17.1 after their admitted transaction work reaches step 2's terminal boundary. Keep BufferPool writeback and WAL writer/group-commit durability services alive. |
| 4 | Quiesce BufferPool external Fetch/new-page admission, drain loads/evictions/writebacks and every guard/pin/latch owner, and stably flush every dirty non-retired page required for normal close under WAL-before-data. A frame belonging to a proven retired/orphan object may use §7.12.5's discard rule; no other dirty frame may be discarded. |
| 5 | With no active writer and an empty DPT, append, durably flush, and install one final checkpoint through §13.5, including the alternating control-slot `fdatasync`. This checkpoint is mandatory for a successful controlled close but is an optimization, not a clean marker or a prerequisite for later recovery correctness. |
| 6 | Complete every already-classified orphan/retirement cleanup task owned by this lifecycle and every namespace mutation initiated by shutdown, including each required parent-directory `fsync` under §4.7. WAL recycling remains optional, but any recycling begun must likewise become directory-durable. Unknown names remain untouched. No unacknowledged namespace mutation or queued owned cleanup is called complete. |
| 7 | Stop and join BufferPool helper activity after its drain, then stop and join the WAL writer/group-commit service only after no page flush, terminal protocol, checkpoint, control update, or WAL recycle can need it. Destroy BufferPool frames/page table and close file registrations/managers and non-lock directory descriptors while retaining the control lock descriptor. |
| 8 | Release/close the exclusive OS lock descriptor only after step 7, then remove the process-local root claim, publish `CLOSED`, and only then return successful `CloseDatabase`. |

Shutdown does not wait for ordinary `ACTIVE` transactions to choose commit: it
cancels their current statement where safely possible and aborts them. A
`MUST_ABORT` transaction continues mandatory abort. An already `COMMITTING` or
`ABORTING` transaction is allowed to finish its terminal protocol because
discarding it would violate §§9.14, 15.5, and 15.6. Shutdown cannot release
transaction locks or live transaction state before terminal publication.

WAL durability service therefore outlives transaction terminal completion,
every required page-file flush, and final checkpoint/control publication.
`close()` is never used as a durability primitive. The final checkpoint is
required only to call the close successful; every later open still runs recovery
and no control bit records “clean.”

If WAL/page/checkpoint/control/directory synchronization fails, an internal
invariant fails, or a guard/worker will not drain, `CloseDatabase` MUST NOT report
success. The owner enters `NONCONTINUABLE`. Known safe resource teardown may then
release descriptors and the lock last and return `SHUTDOWN_FAILED`; this is a
non-clean close and the next open recovers from the last valid durable
control/WAL/data state. If teardown cannot prove all process-local users stopped,
the owner and lock remain live until teardown succeeds or the process exits.
Dirty frames are never dropped and described as durably flushed, and a shutdown
failure never changes a durable COMMIT to ABORTED.

### 3.3.7 Create, removal, crash, and lifecycle errors

Section 4.7.8's creation-publication owner MUST establish the ordinary
process-local root/control claim and exclusive control lock after validating the
staging root and before final rename, then retain both through external-parent
synchronization. A creator returning an open database handle continues that
same ownership without release/reacquire through the §3.3.2–§3.3.4 validation,
recovery, cache, and READY gates; no handle reaches READY or admits/acknowledges
ordinary work before durable root publication. A no-handle create releases the
lock, then the process claim, only after durable root publication and before
returning success. There is no separate less-validated “new database ready”
path.

Whole-database DROP/removal is not an online operation. Under §4.7.9 it starts
only with no ordinary DatabaseInstance, transaction, BufferPool/database
manager, worker, or background producer, then acquires the same process-local
root/control claim and exclusive control lock and performs §3.3.2's independent
root-adoption barrier. It proves supported-v1 destructive root identity without
running ordinary recovery or reaching READY, durably retires the complete final
root before deleting any contents, and treats later private reclamation as
retryable cleanup. Contention returns `DATABASE_BUSY` before destructive
inspection or mutation.

Process crash releases the process-associated lock and all runtime state. Machine
crash likewise removes OS lock state. Control-file existence alone is not
ownership. The §4.7.10 lifecycle gate likewise releases on owner death; existence
of any gate backing object is not evidence of a live owner.
The next opener acquires exclusivity and always runs recovery; §4.7 determines
which namespace mutations survived, while control/WAL/page protocols determine
database state.

Lifecycle APIs preserve distinctions equivalent to:

```text
DATABASE_BUSY
NOT_A_DATABASE
UNSUPPORTED_FORMAT
CORRUPT_DATABASE
RECOVERY_FAILED
IO_OR_DURABILITY_FAILURE
DATABASE_NONCONTINUABLE
SHUTDOWN_FAILED
```

`DATABASE_BUSY` is retryable only after the other owner releases exclusivity.
Known open/validation/I/O failures permit a future open after cleanup, although
the underlying persistent fault may recur. `DATABASE_NONCONTINUABLE` permits no
same-owner ordinary retry; teardown and a fresh recovery open are required.

Canonical crash-point outcomes are:

| Crash point | Durable/runtime consequence and next open |
|---|---|
| after lock acquisition or some required files opened | OS releases lock; no READY was published; next open repeats full validation/recovery |
| during recovery | only valid WAL/data/control/namespace publications survive; next open restarts idempotent recovery and loser resolution |
| after recovery completion but before READY | no user work was admitted; next open still reruns recovery and rebuilds runtime caches |
| immediately after READY | admitted work survives only through normal WAL/page rules; next open performs crash recovery |
| immediately after DRAINING or during statement/transaction abort drain | no clean claim exists; next open resolves every non-durable transaction as a loser and preserves durable commits |
| during dirty-page flush | WAL-before-data and checksums/FPI recovery govern any partial data write; next open restores from the valid durable prefix |
| during final checkpoint/control publication | dual control slots select the newest independently valid complete checkpoint or legal fallback; next open never trusts a torn slot/incomplete checkpoint |
| after all shutdown durability work but before lock release | runtime crash releases the lock; the durable final checkpoint may shorten, but never eliminates, next-open recovery |
| after lock release | all successful-close prerequisites and manager teardown already completed; the next owner still performs normal recovery validation |

The following implementations are forbidden:

1. relying on deployment convention instead of acquiring the exclusive database lock;
2. reading/choosing control state, inventorying WAL, mutating, or recovering before exclusivity;
3. treating a stale filename or control-file existence as proof of a live owner;
4. creating two independent owner/BufferPool/WAL-manager instances for one actual root in one process;
5. admitting normal work before recovery, status repair, catalog reconstruction, file validation, and READY publication complete;
6. trusting a torn transaction-status page before recovery under §§12.10.5, 13.13.2, and 13.17;
7. ignoring or recreating a missing bootstrap/committed catalog-owned file;
8. deleting unknown root entries as presumed orphans;
9. shutting down WAL durability before transaction completion, dirty-page drain, and final checkpoint;
10. dropping durability-required dirty pages after flush failure and reporting close success;
11. releasing the process lock while any manager, worker, guard, registration, or descriptor can still use database state;
12. changing a durable COMMIT to ABORTED because recovery/open/shutdown later failed;
13. recording or inferring clean shutdown without durably satisfying every required close prerequisite;
14. performing ordinary work after `DATABASE_NONCONTINUABLE`;
15. treating `close()` as file or namespace durability;
16. running ordinary transaction, maintenance, catalog-publication, or background writeback work concurrently with recovery.
