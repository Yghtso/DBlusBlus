# DBlusBlus Verification and Benchmark Guide

## Purpose and authority

This document defines detailed testing, crash-injection, fuzzing, regression, and benchmark procedures.

For fresh-machine toolchain setup and normal build/check invocation, see
[`DEVELOPMENT.md` — Development baseline](DEVELOPMENT.md#development-baseline).

[`ARCHITECTURE.md`](ARCHITECTURE.md) defines the correctness/performance obligations. This
guide describes practical ways to verify those obligations. A test recipe does not weaken
or replace an architectural invariant, and benchmark numbers are measurements rather than
persistent architecture constants.

Procedures are organized by subsystem capability and apply whenever that capability is
under verification. Implementation status belongs in `PROJECT_STATE.md`.

---

## Testing Philosophy

Every subsystem needs:

### Unit tests

For local invariants.

### Property/randomized tests

Especially for:

- B+ tree operations,
- page compaction,
- tuple serialization,
- transaction visibility,
- recovery.

### Crash tests

Simulate process failure at many WAL/page-write boundaries.

### Differential tests

Where practical, execute supported SQL against a reference database and compare results.

### Concurrency stress tests

Exercise:

- lock ordering,
- B+ tree splits,
- buffer eviction,
- transaction conflicts.

---

## Benchmarking Philosophy

Performance claims require measurements.

At minimum benchmark:

- sequential scan throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- buffer-pool hit/miss behavior,
- concurrent transaction throughput.

Track:

```text
rows/sec
queries/sec
transactions/sec
p50 latency
p95 latency
p99 latency
CPU time
page reads/writes
WAL bytes
cache hit rate
```

Microbenchmarks and end-to-end benchmarks should both exist.

---

## Storage Verification

At minimum:

### Slotted-page tests

- insert until full,
- basic heap-page mutation treats DEAD slots as non-reusable,
- NORMAL -> DEAD transitions and compaction preserve SlotIds,
- DEAD -> UNUSED transition and reusable-slot tests exercise the Chapter 14
  vacuum/reclamation protocol,
- ordinary owner validation enforces page identity, version, geometry, required-zero
  fields, and schema-directed tuple validity for every retained NORMAL and DEAD tuple,
- ordinary owner validation rejects overlapping NORMAL/NORMAL, NORMAL/DEAD, and DEAD/DEAD
  retained tuple ranges,
- retained DEAD slots receive complete tuple validation and use canonical retained or
  reclaimed coordinates with `aux=0`,
- `REDIRECT_RESERVED` is rejected by ordinary owner validation,
- compaction,
- invalid slot access,
- tuple bytes survive compaction.

### Tuple codec tests

- every scalar type,
- nulls,
- empty VARCHAR,
- long VARCHAR within inline limit,
- mixed fixed/varlen schemas,
- encode/decode round trip,
- unaligned field positions.

### Disk tests

- page zero/superblock,
- generic v1 superblock encoding writes zero flags and decoding rejects every nonzero flag
  or required-zero reserved field,
- extend file,
- random page read/write,
- reopen persistence,
- short/error I/O handling where injectable.

### Buffer tests

Use very small pools such as:

```text
3 frames
```

while touching many more than 3 pages.

Verify:

- eviction,
- dirty writeback,
- pin protection,
- no pin leaks,
- CLOCK behavior,
- concurrent read guards,
- exclusive write guards.

### Heap tests

- thousands of tuples,
- many pages,
- scan after reopen,
- deletion-marker behavior under MVCC visibility,
- FSM stale-entry repair.

---

## Storage Benchmarks

Establish measurements before optimizing storage mechanisms.

Measure at least:

```text
sequential page read throughput
sequential page write throughput
buffer-pool hit lookup
buffer-pool miss + read
heap insert throughput
heap sequential scan throughput
tuple encode throughput
tuple decode throughput
```

Run benchmarks using:

```text
warm cache
cold-ish cache where practical
tiny buffer pool
large buffer pool
```

Do not optimize based only on intuition.

---

## B+ Tree Verification

### Required Deterministic Tests

Structural cases:

- insert without split,
- leaf split,
- repeated leaf splits,
- internal split,
- cascading split,
- root split,
- redistribution left-to-right,
- redistribution right-to-left,
- leaf merge,
- internal merge,
- root contraction,
- free-page reuse.

Key-format cases:

- negative/positive INT32,
- INT64 extremes,
- DATE/TIMESTAMP,
- FLOAT64 infinities,
- `-0.0` and `+0.0`,
- NaN canonicalization,
- empty VARCHAR,
- embedded zero bytes,
- composite keys,
- NULLs,
- maximum-size key,
- oversized-key rejection.

---

### Duplicate Stress Test

Insert enough identical user keys with distinct RIDs to span many leaf pages.

Verify:

```text
equality scan returns all RIDs exactly once
lower bound starts at the first duplicate
upper bound stops after the last duplicate
exact Erase(K,RID) removes only one physical entry
tree remains valid after merges
```

This specifically validates the decision to route using full physical separators `(user_key,RID)`.

---

### Randomized Tests

Compare the B+ tree to an in-memory sorted oracle of physical keys.

Random operation stream:

```text
insert
erase
point lookup
range lookup
close/reopen
```

Periodically:

```text
run full verifier
compare complete sorted contents against oracle
```

Random seeds must be reproducible and printed on failure.

Single-threaded randomized structural verification is a prerequisite for concurrency
stress testing.

---

### Concurrent Tests

Stress:

- many readers + one writer,
- writers on disjoint ranges,
- writers on one hot range,
- duplicate-heavy inserts,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes.

Use deliberately tiny buffer pools in some tests.

Add watchdogs/timeouts to detect deadlocks.

---

### B+ Tree Benchmarks

Measure:

```text
random point lookup ops/sec
sorted insertion ops/sec
random insertion ops/sec
exact deletion ops/sec
short range scan rows/sec
long range scan rows/sec
duplicate-heavy equality lookup
split frequency
tree height
average leaf byte occupancy
average internal byte occupancy
buffer-pool hit rate
latch wait time when available
```

Benchmark key shapes:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

Benchmark both:

#### Hot tree

Mostly resident in buffer pool.

Focus:

- CPU,
- comparisons,
- latches,
- cache behavior.

#### Larger-than-buffer tree

Working set exceeds buffer pool.

Focus:

- fanout,
- page access patterns,
- replacement,
- random I/O sensitivity.

---

## Transaction, Durability, and Reclamation Verification

### Crash Injection Framework

Add deterministic process-termination points around:

```text
after WAL append
before WAL append completion
before/after fdatasync
before/after data-page pwrite
after heap insert before index MTR
during B+ MTR construction
after B+ MTR append before page release
before/after TXN_COMMIT flush
during checkpoint construction
after checkpoint durability but before checkpoint installation
after checkpoint installation
during vacuum index cleanup
before DEAD transition
before DEAD -> UNUSED transition
```

For each point:

1. construct a database with known logical contents and enough physical state to make the
   selected boundary observable;
2. terminate the process at exactly that boundary;
3. reopen through the ordinary database lifecycle and run recovery;
4. compare logical contents with the expected durable-transaction model;
5. inspect any physical invariant that the boundary is specifically intended to protect,
   such as checkpoint selection, page LSNs, index reachability, slot state, or WAL-prefix
   validity.

Crash injection uses a separate process and must not run destructors or clean-shutdown
logic after the selected boundary. A crash test passes only when reopen/recovery succeeds
or returns the exact architecture-defined persistent error; process survival alone is not
success.

---

### Non-Crash WAL/MTR Failure Injection

Exercise every non-crash outcome in [`ARCHITECTURE.md`](ARCHITECTURE.md) §12.12 separately
from process-crash testing. The harness must be able to fail reservation, record
construction, logical append, physical WAL writing, WAL synchronization, runtime
publication, and append-tail cleanup independently.

For each locally recoverable case:

1. capture the complete pre-operation state while the tested operation owns the required
   pins, latches, barriers, and frame-transition reservations;
2. inject one precise failure;
3. let the operation return its structured error without crashing the process;
4. compare the resulting state with the capture;
5. prove that ordinary operations may continue only when §12.12 classifies the outcome as
   locally recoverable.

The failure matrix must include:

```text
reservation/resource failure before page mutation
record construction/encoding/overflow/validation failure after no-flush acquisition
known append failure after one provisional page mutation
known append failure after all provisional page mutations
append-outcome or valid-end ownership uncertainty
valid append followed by runtime page/frame publication failure
valid append followed by WAL pwrite failure
valid append followed by WAL fdatasync failure
failure while restoring an unpublished appended tail
failure while truncating/verifying an unpublished appended tail
copied-writeback wins before no-flush reservation
no-flush reservation wins before copied-writeback
```

Known no-append failures must leave the valid WAL end and transaction WAL-chain tail
unchanged and restore every provisional mutation exactly. Append uncertainty must enter
the architecture-defined `NONCONTINUABLE` database state: no test may treat uncertainty as
an ordinary retryable append error.

After a valid append, WAL `pwrite`/`fdatasync` failure must retain the exact append bytes,
leave `durable_lsn` at the prior proven prefix, keep the mutation as legal dirty published
state, prevent dependent data-page writeback, and allow only the retry/escalation behavior
defined by §12.12. Publication failure after the publication-authorizing append must be
retried under retained ownership or become `NONCONTINUABLE`; it must not restore the old
state and continue.

The copied-writeback race is tested in both directions with deterministic barriers:

- when writeback reserves first, mutation waits for reconciliation and captures rollback
  state afterward; a reported writeback failure leaves the mutation unstarted;
- when no-flush reserves first, copied writeback and eviction cannot begin until complete
  publication or exact rollback releases the reservation.

#### Exact rollback-state assertions

Run rollback cases with every affected page initially clean and initially dirty. Compare
the complete captured state, including:

```text
exact page bytes
PageId/frame identity token
page_lsn
dirty/clean status
modification generation
rec_lsn
dirty-page-table membership
FPI/checkpoint epoch state
owning root/height/allocation/free-list runtime metadata where applicable
```

An initially dirty page must return to the same dirty bytes and metadata; rollback must not
clean it, assign another generation, change its original `rec_lsn`, or alter DPT/FPI state.
An initially clean page must remain clean and absent from the DPT. Also assert that no
ordinary guard, checkpoint capture, writeback, eviction, or unrelated reader can observe
the provisional state.

---

### PAGE_INIT and MTR Rollback Tests

#### PAGE_INIT failure

For ordinary page initialization, capture the file length/page count, runtime allocation
metadata, publication counts, and every structure that could refer to the new PageNo. Then
inject each known pre-append and cleanup failure required by §12.12 and §4.11.1.1.

For a known locally recoverable failure, assert:

- the unpublished appended tail is serialized against competing extension and immediately
  restored/truncated to the captured length;
- no published page count, parent/root/free-list reference, catalog reference, or other
  durable structure names the failed page;
- the valid WAL end and ordinary frame/DPT state do not advertise the failed initialization;
- the PageNo is reused deterministically by the next serialized successful extension where
  the architecture requires reuse.

Inject failure of tail restoration, truncation, and post-truncation verification
separately. If exact length/ownership cannot be established, assert transition to
`NONCONTINUABLE`, retained protection of uncertain state, rejection of ordinary work, and
recovery on the next open. Such failures must never be hidden as successful allocation or
ordinary rollback.

#### B+ MTR rollback

Construct operations that provisionally mutate several existing pages and combinations of
newly allocated and reused pages, including split/root-change paths. Capture the complete
pre-MTR state, inject a known failure before the publication-authorizing append, and assert
exact restoration of:

```text
every affected page and frame metadata item
new-page and reused-page disposition
root PageNo and tree height
left/right sibling links
parent links and separator state
allocation/free-list state
```

Run the same cases with clean and dirty input pages. No page may retain a consumed
modification generation or provisional LSN. If the publication-authorizing record validly
appended, test completion/retry or `NONCONTINUABLE` escalation instead of rollback.

#### Publication-atomicity observers

Pause a multi-page MTR after provisional mutation and again during publication. Coordinate
checkpoint DPT capture, flush/writeback, eviction, and permitted readers at those barriers.
Every observer must see either the complete pre-operation bytes/metadata or the complete
published WAL-backed state. No observer may see a mixed page set, a reserved-only LSN,
provisional root/free-list metadata, or a DPT snapshot that includes only part of the MTR.

---

### Database Lifecycle Tests

Exercise the exclusivity rules in `ARCHITECTURE.md` §3.3.2, ordered open protocol in
§3.3.3, READY and cleanup rules in §3.3.4, noncontinuable behavior in §3.3.5, controlled
shutdown protocol in §3.3.6, and lifecycle outcomes in §3.3.7. Use separate processes
wherever the contract depends on POSIX process ownership.

#### Exclusive ownership, fork, and exec

Use a parent/child coordination harness to verify:

1. two independent processes race to open the same final database root and exactly one
   reaches ownership/READY;
2. the loser receives `DATABASE_BUSY` before control-slot selection, WAL inspection,
   recovery construction, or database mutation;
3. path-equivalent and alias opens in one process are rejected by the process-local stable
   root/control identity registry rather than path-string comparison;
4. normal close releases ownership only after database users and descriptors drain;
5. abrupt process exit releases the POSIX lock and permits a new process to acquire it and
   recover.

In a fork test, the child must not use copied live handles, must not be treated as the
database owner, and must close/discard them or `exec`. A child independently opening the
same database must conflict with the parent's ownership. Verify the control descriptor's
close-on-exec behavior by executing a helper and proving it cannot accidentally retain the
lock after the parent releases its own descriptors.

#### Deterministic database-root lifecycle harness

The root-lifecycle procedures below verify `ARCHITECTURE.md` §§3.3.2–3.3.4 and 3.3.7,
§§4.7.1, 4.7.6, and 4.7.8–4.7.9, the root rules in §4.15 and §41.3, and Appendix B
invariants 13, 18, and 21–24. They use deterministic barriers rather than elapsed-time
sleeps. Separate processes are required for POSIX lock contention, lock release on process
death, and adoption after prior-owner death; in-process competitors separately verify the
process-local stable-identity claim.

The harness records an ordered event stream sufficient to assert, without depending on
implementation class names:

```text
external-parent/root/control descriptor acquisition and (device,inode) identity
process-local root/control claim acquisition and release
ordinary database.control lock attempt, acquisition, and release
root rename request/result and exact source/destination identities
external-parent fsync begin/result
parent-entry/root identity verification and post-fsync revalidation
control-slot inspection, WAL inventory, recovery, catalog inspection, and mutation start
READY publication and transaction admission
private cleanup unlink/rmdir operations
operation semantic result
```

Required conceptual barriers are:

```text
CREATE: staged bootstrap validated; ownership acquired; final rename completed;
        external-parent fsync before/after; READY before/after; ownership release
OPEN:   ownership acquired; adoption before/after; control inspection begins;
        WAL inventory begins; recovery begins
REMOVE: ownership acquired; adoption before/after; destructive identity validated;
        retirement rename before/after; retirement parent fsync before/after;
        private cleanup begins; retired-root rmdir completed; final parent fsync
```

Fault injection may intercept the corresponding filesystem operation or pause at the
conceptual boundary. Child-process IPC releases barriers and reports observations. A test
must not infer ordering from scheduler timing. Filesystem crash tests use the Crash
Injection Framework's durable-prefix model or an isolated equivalent that distinguishes
runtime rename visibility from parent-directory durability; real power removal is not
required.

Run the shared-ownership fixture once with creation publication, ordinary open, and offline
removal as the attempted owner. For the same retained root/control identity, assert that all
three use the same process claim and ordinary control-lock observations and that failure of
either common gate prevents ownership. No operation-specific creator/remover ownership
channel may admit a second owner. Repeat through a pathname alias to prove that path spelling
is not identity.

#### Database-root creation publication

For a staged create, pause after the no-replace final-root rename is runtime-visible and
before the external-parent `fsync` succeeds while the creator retains its claim, control
descriptor, and lock. In a second process call ordinary open on final `D`; in another run
attempt offline removal. Each competitor must receive `DATABASE_BUSY` through the ordinary
ownership gate. Its trace must contain no control-slot interpretation, WAL inventory,
recovery/catalog construction, namespace mutation, root retirement, READY publication, or
transaction admission. This demonstrates that ownership blocks use while the parent sync,
not the lock, remains the durability barrier.

At the same barrier, attempt another open in the creator process using both the original
spelling and an alias resolving to the same retained root/control inodes. The process claim
must reject both attempts, create no second owner/DatabaseInstance, and produce the same
pre-inspection `DATABASE_BUSY` semantics even though process-associated POSIX locks do not
conflict within one process.

Run create-result variants as follows:

| Variant | Required observations |
|---|---|
| returns an open handle | Claim and control lock are acquired before final rename and remain continuously held across rename, parent sync, ordinary recovery/READY prerequisites, and handle publication. No release/reacquire event occurs; a competing process stays busy throughout; READY, transaction admission, and handle publication occur only after the successful parent sync. |
| returns only durable-create success | Claim and lock remain held through successful parent sync; lock release occurs while the process claim still exists; claim removal follows lock release; success follows both. A competitor remains busy before release and may acquire/open after release. |

Inject failure of the external-parent `fsync` after a successful final rename. While the
creator is paused in reconciliation, assert no success, READY handle, transaction admission,
or ownership release and prove that competing open/removal remains busy. The oracle records
the rename as durability-uncertain—it must not label it definitely persisted or definitely
lost. If DatabaseInstance construction began, verify the ordinary failed-open versus
noncontinuable cleanup outcome without admitting work.

Terminate the creator process after final rename and before parent sync. The OS lock must be
released by process death, but that death is not a publication event. Start an independent
opener against a surviving final root and assert this order:

```text
claim and ordinary control lock
verify retained external-parent entry identifies retained root inode
fsync external parent
revalidate/reconcile parent entry and retained identity
control-slot selection / WAL inventory / recovery
```

No control/WAL/recovery observation may precede adoption. If the final root did not survive
an independently modeled machine crash, ordinary open instead follows the architecture's
missing/staging classification; process death by itself is never treated as publication
proof.

Exercise machine-crash durable prefixes around root publication:

| Crash boundary | Allowed survivor/classification | Assertions after restart |
|---|---|---|
| before final rename | staging root or no staged entry; no final live root | staging is never an ordinary open target; no acknowledged ordinary database work exists |
| after final rename, before external-parent sync | final or staging survivor according to the modeled durable prefix | no pre-crash ordinary work was acknowledged; a surviving final root completes independent adoption before use |
| after successful external-parent sync | durably published final root | ordinary ownership, adoption/open validation, and recovery may proceed |

For every create-publication case, capture root/control `(device,inode)` before rename and
after access through final `D`. Assert the retained descriptors and control lock still name
the same objects. This is platform-contract validation for the supported Linux/POSIX model,
not a replacement for the architecture's durability assertions.

#### Independent final-root adoption

Exercise ordinary open and offline removal with a barrier immediately after successful
process claim/control-lock acquisition. Permit each operation to continue one event at a
time and require parent-entry/root verification, external-parent `fsync`, and identity
revalidation to complete before any control interpretation beyond minimum adoption identity,
WAL inventory, recovery, catalog inspection, database/namespace mutation, removal identity
validation, or READY publication.

Inject open/read, identity-check, and parent-sync failures independently during adoption.
Assert the existing `IO_OR_DURABILITY_FAILURE` or architecture-defined identity error, no
READY/recovery/mutation/transaction admission, and dependency-ordered ownership cleanup or
retention when safe unwind is uncertain. Inspect the control bytes and filesystem to prove
that no persisted adoption marker, control bit, or alternate root state was written.

Test pathname rebinding in an isolated parent directory. After retaining parent/root/control
descriptors but before post-sync revalidation, have a coordinated helper move the final entry
and install a different root at the same pathname. The owner must detect that the parent
entry no longer identifies the retained root and stop before inspection or mutation of
either object. Repeat for open and removal. Where direct rebinding is unavailable, inject a
different `(device,inode)` identity result at the revalidation boundary and require the same
failure. Path-string equality alone is never an acceptable oracle.

#### Offline whole-database removal and destructive identity

For each active-owner condition below, attempt whole-database removal and assert
`DATABASE_BUSY` or the architecture-defined lifecycle rejection before destructive
validation, retirement rename, or content mutation:

```text
ordinary DatabaseInstance active before READY
READY DatabaseInstance active
DRAINING or CLOSING owner still retaining exclusivity
same-process owner through the stable root/control claim
cross-process owner through the ordinary database.control lock
creator retaining root-publication ownership
another offline remover
pathname alias of any of the same actual roots
```

The positive fixture begins from `CLOSED` with no ordinary DatabaseInstance, transaction,
BufferPool/database manager, worker, or background producer. Assert that only this offline
lifecycle owner can acquire removal authority; the test must not close or convert a live
instance as an implicit part of removal.

After legitimate offline ownership and adoption, record all service-construction events.
Removal must reach minimum destructive identity validation and retirement without READY,
normal recovery, BufferPool construction, catalog/transaction-status reconstruction, WAL
inventory/redo/undo, transaction admission, or background-service startup.

Use table-driven fixtures for destructive identity. Every allowed case first satisfies
no-follow retained-parent/root lookup, parent-entry/root identity match, regular no-follow
`database.control`, stable process claim, ordinary control lock, and completed adoption.

| Control/root fixture | Expected removal authority |
|---|---|
| complete slot with bytes `0..7 = DBLUSCTL`, little-endian `format_version=1` at `8..9`, and `header_size=88` at `10..11` | may select a retired name; no recovery-usable generation is required |
| one such complete slot plus a corrupt/invalid other slot | may select a retired name |
| exact v1 framing with bad CRC, invalid generation/range, invalid flags/reserved bytes, or failed cross-field validation | may select a retired name; ordinary open remains subject to its stricter validation |
| recognizable v1 control plus corrupt catalog, missing required managed file, or torn/malformed WAL | may select a retired name without catalog/WAL interpretation or recovery |
| missing root or `database.control` | `NOT_A_DATABASE`/the existing missing-target result; no destructive mutation |
| control symlink or wrong object type | `NOT_A_DATABASE`; no destructive mutation and no followed target |
| unreadable control with no established identity | `IO_OR_DURABILITY_FAILURE`; no destructive mutation |
| no exact supported control-family framing or unrelated ordinary directory | `NOT_A_DATABASE`; no destructive mutation |
| exact `DBLUSCTL` family with `format_version > 1` in either slot | `UNSUPPORTED_DATABASE_FORMAT`; no destructive mutation or fallback to another v1 slot |

Pair a damaged exact-v1 fixture with an otherwise similar future-version fixture. The first
must become retirement-eligible after exclusive authority; the second must be refused before
retired-name selection or mutation. Also place plausible database filenames in an unrelated
directory without exact control framing and compare a recursive tree manifest before/after
the refused operation. U1 authority must never arise from superficial filenames.

#### Retired-root classification and retirement publication

Table-test external-parent basename classification for exact
`D.dblusblus-removing-<token>`, where `<token>` is exactly 32 lowercase hexadecimal digits.
Accept exact length/lowercase-hex only. Reject shorter/longer, uppercase, nonhex, empty-token,
missing-token, and wrong-suffix variants. Classify `D.dblusblus-creating`, ordinary valid
final names, and retired names as three distinct cases; both private forms are non-openable,
and a caller-selected final basename matching either reserved grammar is rejected.

Precreate the first selected retired name to force a deterministic collision. Assert the
existing artifact is not overwritten, the no-replace retirement fails for that destination,
another token is selected, and old `D` remains complete/live until a later no-replace rename
actually succeeds. No statistical randomness test is required.

Pause removal before retirement rename while the remover owns `D`. Same-process, alias, and
cross-process open/removal competitors must encounter the common ownership gate and perform
no inspection/recovery/mutation. After runtime retirement rename but before parent sync,
ordinary `OpenDatabase(D)` must not attach to or search for the retired root; it observes no
live target unless a distinct `D` exists. This runtime observation is not evidence of durable
absence.

The publication test releases the retirement sequence one boundary at a time:

```text
no-replace rename D -> exact retired sibling succeeds
assert no semantic-success result and no internal unlink/rmdir
external-parent fsync succeeds
assert semantic removal of captured old D is now publishable
```

Inject rename failure and parent-sync failure separately. Rename failure leaves complete
live `D`. Parent-sync failure after rename produces `IO_OR_DURABILITY_FAILURE`, no semantic
success, no internal cleanup, and a durability-uncertain namespace; the live remover retains
authority while reconciling. The test oracle must not claim that old `D` is definitely
durably absent merely because runtime lookup fails.

During the same sequence, verify that the retained root/control descriptors have unchanged
inodes across retirement rename and that the ordinary control lock and process claim remain
continuous without release/reacquire through successful retirement parent sync. Capture all
unlink/replace operations: `database.control` must not be removed or made independently
recreatable while the captured old root remains live at `D`, and any control deletion must
follow durable retirement.

Attempt creation of a new `D` after runtime retirement rename but before its parent sync.
The new creator may not acknowledge publication from runtime absence alone: its trace must
establish/reconcile prior-name absence through the external-parent durability barrier before
its own final rename/publication. Repeat after successful retirement parent sync with the old
retired tree intact; creation may then publish a distinct root/control inode pair.

#### Whole-database removal crash matrices

Use separate process-termination and machine-crash/durable-prefix matrices. At every
boundary, record final/retired names, retained identities, lock release/ownership, semantic
result, and whether cleanup was permitted.

| Boundary | Process-death procedure and next action | Machine-crash durable outcome |
|---|---|---|
| before ownership | terminate before claim/lock; retry as a fresh operation | complete live `D` |
| after ownership, before retirement rename | terminate owner; next actor reacquires/adopts | complete live `D` |
| after rename, before retirement parent sync | terminate without cleanup; next actor reconciles exact external-parent namespace | live `D` or recognized complete retired root; never partially deleted content |
| after retirement parent sync | terminate before cleanup | old `D` durably absent; retired root durable |
| during private cleanup | terminate after selected contained unlinks | retired content may survive/reappear and cleanup retries; old `D` remains absent |
| after retired-root `rmdir`, before final parent sync | terminate before residue-durability acknowledgement | retired name may reappear after machine crash; semantic removal remains complete |
| after final parent sync | terminate after physical-completion barrier | retired residue durably absent |

For each prefix, retain a pre-removal structural manifest of the captured root. If restart
finds the captured old root inode at live `D`, assert its files/directories and contents were
not partially deleted by whole-root removal and that the event log contains no cleanup
operation. If restart finds a retired root, it must match the exact retired grammar and never
become an ordinary open target. Absence may be acknowledged as semantic removal only where
the retirement parent-sync barrier succeeded.

#### Contained retired-root reclamation and recreation

First prove the authority distinction. Add an unknown regular file and unknown directory to
a live supported-v1 root; ordinary §4.7.6 orphan classification must preserve both. In a
separate fixture, acquire exclusive removal authority, durably retire the root, and run
private cleanup; the same unknown contained forms may be deleted without catalog ownership
classification.

Exercise containment with sentinels:

- place a symlink in the retired root pointing to an external file and directory; cleanup
  unlinks only the symlink entry and both external targets remain byte-for-byte intact;
- where safely supported, add a FIFO and local socket and verify only their contained
  entries are removed; privileged device-node creation is not required;
- place unrelated files/directories and another database beside `D` and the retired sibling;
  compare their identities and contents before/after cleanup;
- in an isolated integration harness, place a mounted/cross-device directory beneath the
  retired root and require traversal refusal with cleanup left pending. Where mount setup is
  unavailable, inject a child filesystem/device-identity mismatch at the descent decision
  and assert no child open/unlink occurs.

Inject cleanup failure after durable retirement. The API-independent semantic result must
remain successful removal of old `D`; no rename-back occurs, the exact retired root remains
recognized cleanup residue, and no background-cleanup promise is assumed. A fresh
`RemoveDatabase(D)` with no live `D` follows the existing missing/non-database result rather
than re-reporting capture of the old database; explicit cleanup may still target the exact
retired artifact.

Build partially reclaimed trees and retry explicit cleanup after ordinary failure and after
process crash. Already-absent entries are accepted idempotently, remaining entries are
removed bottom-up under retained no-follow descriptors, no catalog/WAL/recovery service is
constructed, and no live `D` or external sibling is touched. Crash-restored internal entries
remain private residue and do not change semantic removal.

After retired-root `rmdir`, crash once before and once after the external-parent `fsync`.
Before that sync the retired name may reappear; afterward physical residue absence is
durable. Semantic removal remains successful in both cases because retirement publication
preceded cleanup.

For recreation isolation, keep the old retired root, create and durably publish a new `D`,
and record distinct root/control inodes. Continue old cleanup using only descriptors bound to
the old retired root. Place sentinel data in the new root and prove cleanup cannot resolve
through or mutate it. If convenient, allocate equal numeric database-local FileIds in the
old and new roots and confirm no cross-root lookup; inode/root isolation remains the required
oracle even when equal FileIds are not induced. Direct open or generic discovery of the old
retired basename must always reject it as a live database.

The root-removal result matrix is:

| Condition | Semantic result | Retirement mutation | Private cleanup |
|---|---|---:|---:|
| owner contention | `DATABASE_BUSY` | forbidden | forbidden |
| missing/unrecognizable root or wrong control type | `NOT_A_DATABASE`/existing missing-target result | forbidden | forbidden |
| unreadable identity input | `IO_OR_DURABILITY_FAILURE` | forbidden | forbidden |
| unsupported future control format | `UNSUPPORTED_DATABASE_FORMAT` | forbidden | forbidden |
| recognizable damaged v1 after ownership/adoption | eligible to proceed | allowed | only after durable retirement |
| adoption sync/identity failure | existing identity or `IO_OR_DURABILITY_FAILURE` | forbidden | forbidden |
| retirement rename failure | `IO_OR_DURABILITY_FAILURE` | not published | forbidden |
| retirement parent-sync failure | `IO_OR_DURABILITY_FAILURE`; durability uncertain | rename may be runtime-visible but removal is unacknowledged | forbidden |
| cleanup failure after durable retirement | semantic removal remains successful; cleanup residue is diagnostic/pending | already published | retry allowed |

#### Control slots and required recovery inputs

Build fixtures for one valid control slot, two valid slots, and no valid slots. Exercise
the selection, unsupported-version, corruption, and legal older-generation fallback cases
defined by §3.3.3 and §13.2. Assert the selected checkpoint/control generation and exact
structured open result; do not duplicate the control-slot truth table here.

Separately remove, replace, truncate, or corrupt each architecture-required recovery input:

```text
required WAL segment/range
immutable bootstrap locator and bootstrap system files
catalog.dat
txn_status.dat
files required by recovered committed catalog state
```

Assert that missing or invalid required state never reaches READY or becomes an orphan
shortcut. For recoverable torn transaction-status state, instrument status lookup and prove
that recovery repair and loser resolution complete before ordinary status lookup is
enabled.

#### Open fault matrix and READY admission

Exercise a failure or process crash on both sides of every stable transition in §3.3.3's
ordered open protocol, using §3.3.7 for the required structured result. For each point:

1. inject the selected validation, allocation, I/O, synchronization, worker-start, or
   publication failure;
2. assert no transaction, statement, maintenance task, ordinary page/file operation, or
   normal background task is admitted before the single READY publication;
3. assert no partly recovered status/catalog/ownership state is visible;
4. inspect and cleanly account for every resource acquired before the failure;
5. retry open/recovery when the architecture permits a later owner to do so.

Failed-open cleanup assertions include stopped/joined recovery workers, quiesced and
destroyed partial BufferPool state, released guards/pins, closed file registrations and
descriptors, released logical gates/locks, and removal of the process-local identity claim
only after the OS lock descriptor is released. If any worker, guard, append/publication
outcome, or cleanup result cannot be established, assert `NONCONTINUABLE` and retained
exclusivity rather than a false `CLOSED` result.

#### Orphan classification

Create exact managed pending/final names in every architecture-defined required,
retired, and unowned state, plus unrelated names and unsafe object types. Run open and
recovery classification, then assert:

- proven database-owned orphans follow the namespace reconciliation/cleanup protocol;
- required committed/bootstrap files are never reclassified as disposable orphans;
- unknown names are recorded/ignored and never deleted by pattern guessing;
- an orphan cleanup failure remains a retryable cleanup task when §3.3 permits READY, while
  failure of synchronization needed for required WAL/status/control/namespace state blocks
  READY.

#### Shutdown, draining, and failure injection

Drive `READY -> DRAINING` while transactions occupy each runtime state:

```text
ACTIVE
MUST_ABORT
COMMITTING
ABORTING
```

Require the exact outcomes in §§3.3.6, 15.5, and 15.6. Assert that DRAINING immediately
closes transaction, statement, maintenance, page/file, and scheduled-background admission;
already-admitted terminal and cancellation work may use only the internal facilities that
the shutdown protocol keeps alive.

Instrument each ordered shutdown step and verify this dependency sequence:

```text
close admission
cancel/drain ordinary statements and maintenance
complete required transaction terminal publication and release
stop producer/background services while retaining durability services
quiesce and drain BufferPool guards, pins, writebacks, and dirty pages
append, flush, and install the final checkpoint/control slot
complete required namespace cleanup and directory synchronization
stop BufferPool helpers, then the WAL/group-commit service
destroy managers/descriptors, release the OS lock last, publish CLOSED
```

Inject page-flush, WAL synchronization, checkpoint construction/installation, control-slot
update, namespace mutation, and directory-sync failures independently. For every failure,
assert the returned lifecycle result, retained durable interpretation, database health,
lock lifetime, and next-open recovery behavior. A required failure must never report
successful close, discard an unflushed dirty frame as durable, publish a clean marker, or
reinterpret a durable COMMIT as ABORTED.

#### NONCONTINUABLE and durable-commit preservation

Induce `NONCONTINUABLE` through append uncertainty, failed post-append publication, failed
exact restoration, and lifecycle teardown uncertainty. Assert atomically that:

- no new transaction, statement, maintenance operation, ordinary page/file load, WAL
  append, writeback, checkpoint, or ordinary mutation begins;
- in-flight operations fail/cancel at safe ownership boundaries without exposing protected
  provisional state;
- exclusivity remains held until dependency-ordered non-clean teardown is safe or the
  process exits;
- non-clean close does not flush uncertain frames, install a final checkpoint, recycle WAL,
  or claim successful shutdown;
- a subsequent owner performs the full recovery protocol.

For every lifecycle failure after a transaction's commit record becomes durable, reopen
and verify that the transaction is COMMITTED. Repeat with runtime terminal-cache
publication failure, cleanup failure, failed shutdown, and process restart. No lifecycle
or cache error may append, publish, report, or recover that TxnId as ABORTED.

---

### Statement Failure and Transaction-State Tests

Directly exercise every reachable row of the normative statement-error matrix in
`ARCHITECTURE.md` §39.1.3 on both sides of §39.1.2's first transaction-owned published-write
boundary. The architecture owns the exact `FAILED_TRANSACTION_REMAINS_ACTIVE`,
`FAILED_TRANSACTION_MUST_ABORT`, and `DATABASE_NONCONTINUABLE` classifications; tests own
the setup and observations.

For each matrix cell:

1. begin an explicit transaction and one identified CommandId;
2. arrange for the selected error before the boundary, then repeat after a controlled
   transaction-owned mutation has published where that failure remains reachable;
3. record the statement/client error, runtime transaction state, COMMIT eligibility,
   automatic ABORT behavior, database health, and retained logical ownership;
4. inspect logical visibility after terminal handling and after reopen/recovery;
5. verify that the original causal diagnostic is preserved if cleanup adds a stronger
   transaction/database failure.

The matrix includes effect-free user/read errors, parse/bind/type/cast/expression and
constraint errors, cancellation, lock cancellation, deadlock/serialization failures,
`OutOfMemory`, spill failures, persistent-page corruption, known and uncertain
WAL/BufferPool/page I/O outcomes, DDL namespace failure, ANALYZE failure, and invariant
failure. Do not assign one generic transaction outcome to all memory, disk, or I/O errors;
use the exact §39.1 row and the lower-layer §12.12 result.

#### Partial DML and exact local rollback

Use explicit tests for:

- a five-row INSERT whose fifth row fails after four rows publish;
- UPDATE failure after at least one replacement version, old-version marker, or index
  effect publishes;
- DELETE failure after at least one `xmax/cmax` publishes;
- equivalent target-spool, assignment, constraint, OOM, and spill failures before any
  transaction-owned mutation publishes.

Assert the architecture-defined statement result and transaction transition, prohibit
COMMIT when required, and verify that no successful row prefix becomes logically committed.
Physically retained aborted versions/index entries are allowed only where the architecture
classifies them as transaction garbage.

Test §12.12 exact local rollback in two transactions: one with no earlier successful
statement write and one with an earlier statement-owned published write. Capture and compare
the full provisional physical state as described above. Exact rollback of the failing
primitive must not itself set the current statement's published-write flag, but it also must
not erase an earlier published effect or reset transaction-wide persistent-write state.

Publish an empty page or pure allocation/shape/FSM record with no transaction-owned logical
tuple/index/catalog/statistics effect, then inject a later statement error. Assert that
§39.1.2 classifies the first-published-write boundary by logical transaction ownership, not
by the existence of any WAL record or physical page change. Repeat with a record that does
publish a logical row/index effect to prove the distinction.

#### CommandId, snapshot, retry, and RETURNING

Verify all retry rules in §§9.6, 9.9, and 39.1.4:

- every admitted failed statement consumes its CommandId;
- all internal pre-write retries of one logical statement retain that CommandId;
- `FAILED_TRANSACTION_REMAINS_ACTIVE` unregisters the failed READ COMMITTED snapshot and
  the next statement obtains a fresh snapshot;
- transparent same-TxnId retry occurs only while no transaction-owned write has published;
- once publication occurs, retry is forbidden and the required abort path runs;
- no ordinary statement or new gate begins in `MUST_ABORT`, `COMMITTING`, or `ABORTING`.

For DML `RETURNING`, force a later-row write/result-spool failure after earlier rows have
produced return values. An explicit-transaction failure must expose no partial successful
prefix. In autocommit, retain the complete result through implicit COMMIT C5 and release it
only with the successful post-commit response. Inject pre-durable commit failure and
post-durable connection/transport failure and assert client visibility separately from the
durable transaction outcome. Operator-level spool and Halloween behavior remains covered by
the DML execution section.

---

### COMMIT Fault-Injection Tests

Exercise every C0-C6 boundary defined by `ARCHITECTURE.md` §§15.5 and 39.1.5. Instrument
entry to and completion of each boundary rather than treating “before/after commit flush” as
the complete matrix.

For each injection, record independently:

```text
valid WAL append end and surviving commit record
durable_lsn relative to commit_lsn
resident transaction-status page state
runtime transaction state and terminal cache
database health / NONCONTINUABLE state
logical locks, gates, snapshots, and derived-cache ownership
client acknowledgement or uncertainty
recovery outcome after process restart
```

Required categories are:

- C0/C1 precondition or resource failure before the terminal protocol;
- C2 terminal-record construction and known no-append failure with exact restoration;
- C2 uncertain append outcome;
- valid commit append followed by status-page/frame publication failure;
- first and repeated C3 WAL write/`fdatasync` failure while COMMITTING ownership is retained;
- connection loss before append, after append but before durability, and after durability;
- C4 runtime terminal-state/cache publication failure after durable commit;
- C5 catalog/statistics cache installation or safe-invalidation failure, lock/registry
  cleanup failure, and incoherent cleanup escalation;
- C6 successful acknowledgement and acknowledgement transport failure.

Known append failure and uncertain append outcome must be separate test paths. A validly
appended but not durable commit is uncancellable: cancellation or connection loss must not
redirect it to ABORT. Repeated group-commit flush failure must leave every transaction with
a valid commit record in the architecture-defined COMMITTING/noncontinuable handling, not
selectively abort unacknowledged waiters.

At and after C3, assert the invariant directly:

```text
durable TXN_COMMIT => semantic outcome COMMITTED forever
```

Post-durable runtime publication, cache, cleanup, lock-release, shutdown, and transport
failures may change database health or client knowledge, but must never create or report
ABORTED for that TxnId. Recovery must reach the same COMMITTED outcome.

---

### ABORT Fault-Injection Tests

Exercise every A0-A4 boundary defined by `ARCHITECTURE.md` §§15.6 and 39.1.6. Cover explicit
ROLLBACK, automatic abort from `MUST_ABORT`, deadlock-victim abort, and connection-loss abort.

Inject independently:

```text
A0 state transition and abort-record preparation
A1 PAGE_IMAGE/TXN_ABORT construction and known append failure
A1 append-outcome uncertainty and post-append status-page publication failure
WAL write/failure paths for an appended abort record
A2 runtime ABORTED publication failure
A3 logical-lock/gate/snapshot/orphan/cache cleanup failure
A4 acknowledgement or connection failure
```

For each case assert the original causal error, transaction state, persisted WAL/status
evidence, terminal-cache state, retained/released ownership, database health, client result,
and recovery outcome. Known pre-publication failures retain `ABORTING` ownership and retry
as prescribed; uncertainty or inability to publish safely becomes `NONCONTINUABLE`.
Transaction locks and gates release only after runtime ABORTED publication. Cleanup or
acknowledgement failure must not restore ACTIVE/COMMIT eligibility or turn ABORTED into
COMMITTED.

---

### Recovery Property Tests

For random transaction workloads:

1. execute operations,
2. crash at random instrumented points,
3. reopen/recover,
4. compare logical committed contents against a model that includes only transactions whose commit records became durable.

Repeat across many seeds.

Physical garbage is allowed.

Logical committed results must match.

---

### MVCC Visibility Tests

Build table-driven tests for:

- committed creator before snapshot,
- creator active at snapshot then commits,
- creator starts after snapshot,
- creator aborts,
- committed deleter before snapshot,
- deleter active at snapshot then commits,
- deleter starts after snapshot,
- deleter aborts,
- own insert previous command,
- own insert current command,
- own delete previous command,
- own delete current command,
- frozen creator.

Test exact `xmin/xmax/cmin/cmax` combinations, not only end-to-end SQL.

---

### Isolation Tests

READ COMMITTED:

- second statement sees newly committed rows,
- one statement does not change snapshot mid-scan,
- writer waiting on updated row restarts/re-evaluates correctly.

Statement-error retry, CommandId consumption, and first-published-write restrictions are
verified by the Statement Failure and Transaction-State Tests above.

REPEATABLE READ / snapshot isolation:

- repeated reads use same snapshot,
- committed concurrent insert remains invisible,
- concurrent update of target causes serialization failure,
- write skew is demonstrably possible and documented as non-serializable.

---

### Locking and Gate Tests

Preserve the basic logical-lock cases:

```text
tuple writer blocks tuple writer
different tuple writers proceed
same unique key serializes
different unique keys proceed
logical lock waits hold no page latches
deadlock cycle detection
youngest victim policy
lock release on commit
lock release on abort
cancelled waiter cleanup
```

Use deterministic barriers rather than timing sleeps where possible.

In addition, exercise every `ALLOW`, `WAIT + DEADLOCK GRAPH`, and `REJECT BEFORE WAIT`
combination in `ARCHITECTURE.md` §11.13.3 and every wait-edge/recheck row in §11.13.4 for
the complete transaction-level resource registry in §11.13.1:

```text
SchemaLock
TableWriterGate SHARED_WRITER
TableWriterGate EXCLUSIVE_DDL
STATS_PUBLISH object-publication mode
MANIFEST_CHANGE object-publication mode
TUPLE_WRITE
UNIQUE_KEY
```

For each blocking combination, install an edge to every owner or queue predecessor that
prevents grant, then assert that all resource families participate in one graph. Include
cycles spanning at least three different resource families. Victim selection must be the
deterministic highest TxnId in the cyclic component rather than a timeout or scheduling
artifact.

Directly execute every adversarial timeline in §11.13.7 without copying that list here.
For each timeline:

1. use barriers to establish the exact ownership/wait sequence;
2. inspect the unified graph before victim selection;
3. assert the required victim and automatic ABORT path;
4. retain all protective ownership until ABORTED terminal publication and canonical A3
   cleanup;
5. wake the survivor and require resource-specific descriptor, target, key, visibility, or
   manifest revalidation before grant/continuation.

Cancelled/disconnected waiters must remove all graph edges and queue state without leaking
gates. No transaction-level wait may retain a heap/B+ page latch, BufferPool frame
transition, or read-epoch guard. Proactive same-owner gate-transition rejection must happen
before waiting and must not create a self-edge or be reported as a deadlock.

---

### UNIQUE and PRIMARY KEY Enforcement Tests

Exercise every row and aggregate-outcome rule in `ARCHITECTURE.md` §11.10 through a
table-driven candidate harness. Architecture owns the exact current-owner truth tables;
tests construct the candidate heap/index/status/CommandId state, run the ordinary
UNIQUE/PRIMARY KEY operation, and assert the specified outcome and transaction consequence.

Run the matrix over:

```text
ordinary UNIQUE keys containing NULL
PRIMARY KEY NULL rejection
composite keys
FLOAT64 canonical zero/NaN and ordering/equality cases
committed and frozen creators
aborted and nonterminal creators
absent, committed, aborted, and nonterminal deleters
same-transaction earlier-command creators
same-transaction current-command creators
earlier-command self-delete reuse
current-command self-delete/supersession of another row
```

The harness must probe the complete physical `(user_key,RID)` range and validate every
candidate's protected heap identity and re-encoded key. Include stale terminal entries,
aborted versions, exact duplicate physical keys, dangling/mismatched RIDs, and protected RID
reuse attempts. Corruption candidates must not be hidden by early exit after finding an
ordinary conflict.

#### Same-statement and UPDATE cases

Test duplicate values within one multirow INSERT for both sequential and batch/pending-set
checking. For UPDATE, cover:

- retaining the same logical key while excluding only the exact revalidated old RID;
- another row in the same transaction/current command already owning the target key;
- target-order permutations producing the same result;
- immediate conflicting key swaps;
- non-NULL-to-NULL, NULL-to-non-NULL, and composite partial-NULL transitions.

The test oracle must use the §11.10 logical current-owner decision, not mere physical index
presence or ordinary statement-snapshot visibility.

#### Wait, recheck, and transaction outcome

Hold `UNIQUE_KEY(IndexId,K)` in one transaction and block another without physical latches.
Complete the owner once as COMMITTED and once as ABORTED. After wakeup, assert that the
waiter discards every prior range position, status result, RID dereference, and read-epoch
guard; restarts from `(K,MIN_RID)` under a fresh guard; and performs a full current-state
recheck before deciding.

Exercise stale physical entries and exact protected RID identity during this recheck. A
wakeup must not assume either that the conflict persists or that it disappeared.

For every violation, apply `ARCHITECTURE.md` §39.1.3 separately before and after the
statement's first published write. Assert the statement/client result and transaction state
without defining a second UNIQUE-specific error policy. All `UNIQUE_KEY`, tuple, writer,
schema, and publication ownership remains held until COMMITTED or ABORTED runtime terminal
publication and releases only in C5 or A3.

---

### Vacuum and Reclamation Tests

Correctness testing is separate from Vacuum Benchmarks. Exercise the reclamation protocols
in `ARCHITECTURE.md` §§14.2–14.12 and their invariants in §14.18. Build indexed version
chains with known creator/deleter outcomes, retain controllable snapshots and read epochs,
and expose deterministic barriers around every reclamation publication unit.

#### Index cleanup and persistent DEAD

For one reclaimable NORMAL RID with several secondary indexes:

1. derive keys through the historical schema;
2. remove each exact `(key,RID)` entry with independently injectable crashes/retries;
3. re-fetch and revalidate the heap identity/header under the page latch;
4. publish `NORMAL -> DEAD` only after every required index entry is known absent;
5. retire the RID in the ReadEpochManager after the persistent DEAD transition.

Assert idempotence when some or all index entries were removed before a crash. A recovered
NORMAL slot repeats exact cleanup; a recovered DEAD slot proves cleanup preceded its
transition and is not returned to ordinary visibility. Close/reopen/recover with persistent
DEAD pages and verify each recovered DEAD RID is conservatively re-enqueued with a fresh
retirement epoch before reuse.

#### Grace period and RID reuse

Register readers before, during, and after RID retirement. Control registration and release
with barriers rather than elapsed-time sleeps. Assert that `DEAD -> UNUSED`, free-list
publication, slot reuse, and whole-page recycling remain forbidden while any active reader
epoch is less than or equal to the RID's retirement epoch. Readers registered after
retirement must not recover the removed index entry through normal traversal.

After all blocking readers release, verify the complete §14.12 eligibility test before
reuse, including version-chain proof and candidate revalidation. Assert canonical UNUSED
coordinates/free-list membership and prove that an old index-derived RID cannot observe a
new tuple identity.

#### Version-chain splicing and long-running snapshots

Construct chains with removable middle, head-adjacent, and multiple consecutive obsolete
versions plus surviving direct successors. Vacuum must rewrite each surviving successor's
`prev` link to the removed version's predecessor through WAL-backed mutation. Inject a
state change before revalidation and require deferral rather than an unproven splice. After
restart, assert that no surviving version points to storage eligible for reuse.

Hold a long-running snapshot and associated read-epoch protection while vacuum executes.
Versions and physical RID identities required by that snapshot must remain available;
versions become reclaimable only after the architecture horizon and epoch predicates permit
it. Repeat with transaction-status normalization/freezing where relevant so reclamation
never guesses a missing status outcome.

Connect the existing crash points for index cleanup, `NORMAL -> DEAD`, and
`DEAD -> UNUSED` to these end-to-end assertions. Page-local state-transition success alone
is not complete vacuum verification.

---

### Group Commit Benchmarks

Benchmark:

```text
1 committing thread
2
4
8
16
32+
```

Measure:

```text
transactions/sec
p50/p95/p99 commit latency
fdatasync calls/sec
average commits per fsync
WAL bytes/sec
```

Compare against a diagnostic mode that forces one fsync per commit to quantify group-commit benefit.

---

### Checkpoint/Recovery Benchmarks

Measure:

```text
checkpoint duration
checkpoint WAL bytes
full-page-image bytes
dirty-page-table size
WAL retained bytes
restart analysis time
redo time
pages redone
recovery total time
```

Test both:

- mostly clean buffer pool,
- heavily dirty buffer pool.

---

### Vacuum Benchmarks

Measure:

```text
dead-version scan rate
index cleanup rate
heap bytes reclaimed
B+ tree merge/split side effects
RID grace-period delay
FSM improvement
status-freezing rate
foreground latency impact
```

Use workloads with:

- update-heavy hot rows,
- delete-heavy tables,
- aborted transactions,
- duplicate secondary keys,
- long-running snapshots.

---

## Catalog, SQL, and Logical-Plan Verification

### SQL Grammar Testing

#### Positive parser tests

For every supported statement and clause family in
[`ARCHITECTURE.md`](ARCHITECTURE.md) §§18.10–18.12, parse representative minimal,
composed, and parenthesized forms and compare the complete syntax-relevant AST shape:

```text
node and token kinds
clause nesting and list order
textual identifier spelling/quoting state
decoded literal payload
operator tree
half-open source spans
```

Include SELECT with and without FROM, joins, aggregation, DML, supported DDL/control
statements, and each supported subquery spelling. Parser tests inspect syntax only: AST
names remain textual and contain no catalog IDs or resolved types.

#### Negative parser tests

For each unsupported or malformed syntax family, assert the architecture-defined
`LexerError`, `ParserError`, or later `UnsupportedFeature` boundary rather than a generic
failure bit. Check the smallest useful half-open source-byte span, no accepted malformed
AST, no arbitrary consumption beyond the synchronization boundary, and bounded work on
truncated/repeated delimiters.

#### Precedence and associativity tests

Verify:

```text
1 + 2 * 3
NOT a AND b
a OR b AND c
a - b - c
a / b / c
-a * b + c
NOT a = b OR c AND d
```

against the exact §18.15 AST. Binary arithmetic and Boolean infix operators are
left-associative, unary operators bind at their declared levels, parentheses override the
table, and comparisons such as `a < b < c` are rejected rather than chained.

#### Identifier tests

Tokenize and parse mixed-case unquoted identifiers, exact-case quoted identifiers,
case-insensitive keywords, keywords used as quoted identifiers, qualified names, and
delimiter/error boundaries. Assert unquoted names normalize to lowercase while quoted
names preserve exact bytes/case through the AST. Exercise quoted-delimiter escaping only
if it is registered by the grammar; §18.4 does not authorize an implementation-local
escape convention.

Pass the resulting AST names to binder/catalog fixtures separately. Unquoted aliases and
qualified names use normalized lookup; quoted names use exact binary lookup. Lexical
normalization must not pre-resolve ambiguity or catalog identity.

#### String and comment termination tests

For strings, cover empty and ordinary literals, doubled single quotes, embedded zero bytes
under §17.5.3, quote/comment marker text inside a literal, backslash as an ordinary byte
with no escape meaning, and an unterminated literal at EOF or before a later semicolon.
Assert decoded logical
bytes, original span, and a bounded source-positioned `LexerError` for invalid input.

For comments, cover line comments ending at newline and EOF, terminated non-nested block
comments, comment markers inside strings, quote markers inside comments, attempted nested
block comments under the non-nested rule, and an unterminated block comment. Verify the
next token and span after each valid comment and the exact lexical error boundary after an
invalid one.

#### Multi-statement synchronization tests

Parse batches shaped as:

```text
valid statement ; malformed statement ; valid statement ;
malformed statement ; independently malformed statement ; valid statement ;
unterminated lexical construct at end-of-input
```

For batch recovery supported by §21.17, assert the malformed statement is diagnosed,
synchronization advances to semicolon or end-of-input without merging statements, and a
later independent statement can be parsed with accurate spans. Single-statement mode may
stop at its first error. Do not require IDE-grade token insertion or recovery inside an
unterminated lexical token.

#### Round-trip debug tests

A debug AST formatter may produce canonical SQL-like output for inspection.

It need not reproduce original whitespace/comments.

Reparse the debug form and compare syntax structure, decoded literals, identifiers, and
operator grouping. Do not treat this formatter as canonical SQL serialization.

---

### Binder Tests

Use immutable mock descriptors and snapshot-aware catalog fixtures. For successful binding,
assert resolved `BindingId`, stable TableId/ColumnId, logical type, nullability, source
span, and inserted casts/operator identity—not merely later query success.

#### Name resolution and wildcards

Cover:

```text
unknown table
unknown column
ambiguous column
aliases
self-joins
qualified references
SELECT *
table.*
```

For self-joins, prove distinct BindingIds despite one TableId. Distinguish unknown qualifier
from unknown qualified column, and never accept a leftmost ambiguous match.

Expand `SELECT *` in visible FROM-relation order and each descriptor's logical presentation
order; expand `u.*` only from that binding. Cover aliases, self-joins, a missing qualifier,
and no-FROM queries. Assert hidden/system columns are absent and output identities/types/
nullability are explicit. ColumnId numerical order is not the wildcard ordering oracle.

#### Types, predicates, and aggregates

Table-driven binding covers implicit numeric promotion, explicit and assignment casts,
invalid casts, standalone/contextual NULL, all predicate BOOLEAN requirements, 3VL
nullability, and exact operator/aggregate overload selection from Chapters 17 and 29.

Aggregate legality cases include WHERE/JOIN aggregate rejection, unsupported nested or
DISTINCT/FILTER aggregate forms, grouped and nongrouped SELECT/HAVING expressions, grouping
constants, HAVING type/visibility, aggregate result type/nullability, and empty-input
metadata. Assert SELECT aliases are not visible in HAVING.

#### ORDER BY, LEFT JOIN, and DML

For ORDER BY, bind an ordinary expression, output alias, and 1-based ordinal; cover alias
versus input-name precedence, ambiguity, invalid ordinals, unsupported BOOLEAN ordering,
and resolved ASC/DESC/NULL order. Inspect the resulting bound expression or slot identity.

For LEFT JOIN, assert preserved-side nullability remains unchanged and every right-side
output becomes nullable, including expressions and descriptors derived from otherwise NOT
NULL columns. Repeat through nested LEFT/INNER structures.

For INSERT/UPDATE/DELETE, cover target-table and target-column lookup, canonical INSERT
column order, omitted/default/typed-NULL handling, duplicate assignments/column-list names,
closed assignment coercions, BOOLEAN WHERE, hidden target RID/old-value requirements, and
RETURNING row-image binding. Inspect resolved UNIQUE/PRIMARY KEY/NOT NULL metadata without
rederiving it from physical indexes. Binding allocates no persistent object/File IDs and
performs no execution.

Binder tests should not require physical execution.

Use an in-memory/mock catalog implementation where useful.

---

### Front-End Error and Source-Span Tests

Drive one representative failure through each architecture-owned category in §21.16:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature / UnsupportedCorrelation
CardinalityError
```

For SQL-originating failures, assert the smallest useful retained source-byte span: bad
token, unexpected grammar production, unknown/ambiguous identifier, invalid cast/operator,
invalid constraint definition, unsupported subquery form, and scalar-subquery occurrence
for a runtime cardinality error. Preserve lower-layer categories where §21.16 requires it.
Internal logical-validator defects use internal invariant/validation errors and need no
invented SQL span.

---

### Type-System Property Tests

Test consistency between:

```text
binder type resolution
constant evaluation
vectorized executor semantics
index-key comparator where relevant
```

Especially:

- signed numeric ordering,
- FLOAT64 NaN/zero semantics,
- NULL comparisons,
- three-valued logic,
- implicit numeric promotion.

A query must not have one semantic meaning in the binder and another in the index comparator/executor.

Generate shared values/types at signed boundaries, FLOAT64 NaN/signed-zero/infinity cases,
NULL/UNKNOWN combinations, VARCHAR byte-order cases, and legal/illegal promotions. For
each capability that exists, compare binder overload/type/nullability with bound constant
evaluation, the §41.5 vector-expression oracle, and index-key canonical comparison/hash
semantics. An absent downstream capability postpones execution of that comparison; it does
not remove the durable property requirement.

---

### Subquery Tests

Execute every row of `ARCHITECTURE.md` §20.14.1 as a table-driven bind/plan/result or
rejection case. The table supplies expected support, arity, bound result, zero/multirow
behavior, canonical logical mode, and physical fallback; this guide does not duplicate the
matrix. Every accepted occurrence is independently uncorrelated and every rejected form
produces its structured error/span without a partially valid bound or logical plan.

#### Scopes and derived tables

Create nested blocks with local names, local aliases, ambiguous local references, names
that exist only in an ancestor, and independently nested no-FROM SELECTs. Assert lookup is
local-only; an attempted ancestor capture is `UnsupportedCorrelation`, an unrelated missing
name remains the ordinary name error, and no executable OuterRef/Apply state appears.

For derived tables, test the mandatory relation alias with and without optional `AS`,
missing/duplicate/invalid aliases, explicit and direct-source output names, duplicate or
generated unnamed outputs, `d.*` projection order, and lookup through the derived
descriptor. Child aliases, hidden columns, and unnamed generated outputs do not leak.
Derived-column alias lists are wholly unsupported in v1, so every `d(a,b)` form is rejected
rather than tested as a supported column-count-mismatch feature.

#### Scalar cardinality and error precedence

For each scalar result type, construct final subquery output with zero, exactly one, two,
and more than two rows after WHERE/aggregation/HAVING/DISTINCT/ORDER/OFFSET/LIMIT. Assert:

```text
zero rows -> one typed NULL scalar
one row -> that value, including SQL NULL
second successfully constructed row -> CardinalityError
```

Inject an error while constructing the first row, the second row, and a later row. First/
second-row construction errors precede cardinality failure; after two rows construct
successfully, CardinalityError precedes later work. `LIMIT 1` may prove at-most-one, while
an estimate or `required_rows=1` may not remove the check.

#### EXISTS demand

Use children whose projection, DISTINCT value, or ORDER BY key would raise a controlled
error if value-evaluated. Assert projection-irrelevant work is bound but not demanded for
EXISTS, while FROM/WHERE/grouping/HAVING/OFFSET/LIMIT work remains demanded. A global
aggregate performs its required child work and then exists even over empty input.

Place deterministic counters/failures after the first final relational row. EXISTS stops
at that row; NOT EXISTS applies ordinary Boolean NOT. `LIMIT 0`, OFFSET, grouped
aggregation, and no-row cases exercise the opposite outcomes without relying on timing.

#### IN and NOT IN

Drive the complete §20.14.6 truth table with empty and nonempty RHS, NULL/non-NULL probes,
matching and nonmatching values, RHS NULL markers, all-NULL RHS, and duplicate values/
NULLs. Assert exact TRUE/FALSE/UNKNOWN in both filtering and projection contexts. Empty RHS
returns FALSE/TRUE for IN/NOT IN even with a NULL probe; duplicate multiplicity does not
change truth or require one physical deduplication strategy.

Repeat with promoted numerics, VARCHAR bytes, infinities, `+0.0/-0.0`, and canonical-
equivalent NaNs. Membership equality/hash state must agree with the shared §17.10.3 type
property tests. Inject output, memory, and spill errors throughout the complete RHS build;
on first demand a left-expression error precedes a dormant build, while any demanded build
error precedes probing.

#### Lazy occurrence, snapshot, and CommandId

Place scalar/EXISTS/IN occurrences in selected and skipped CASE arms, short-circuited AND/
OR right operands, and predicates over empty outer selections. A skipped occurrence never
runs; a demanded occurrence initializes at most once per bound occurrence per statement
attempt and serves all outer rows. On statement retry, old side state is discarded.

Use concurrent commits and current-command visibility fixtures to assert outer and subquery
plans share one effective snapshot, ReadEpochGuard, TxnId, and CommandId. A subquery never
takes a fresh READ COMMITTED snapshot, consumes a new CommandId, creates a nested
transaction, or independently owns logical locks. Cross-reference the §41.3 isolation and
statement-retry tests for the underlying visibility mechanics.

#### Relational clauses and DML integration

Cover global and grouped aggregates, DISTINCT, ORDER BY, LIMIT, and OFFSET inside scalar,
EXISTS, IN, and derived-table contexts. Include global aggregate over empty input, zero/
one/multiple grouped results, DISTINCT reducing scalar duplicates, scalar `LIMIT 1`,
EXISTS after OFFSET, and IN over the post-LIMIT/OFFSET result. Without ORDER BY, do not
assert which arbitrary row `LIMIT 1` selects.

Evaluate subquery errors in INSERT/UPDATE/DELETE WHERE, assignments, input expressions,
and RETURNING before and after the first published transaction-owned write. Assert the
subquery error enters the existing §39.1 matrix at that boundary; this section defines no
new rollback/retry policy.

#### Rewrites and unsupported forms

For each exactly proven empty subquery child, compare fallback and rewritten scalar,
EXISTS/NOT EXISTS, IN/NOT IN, and derived-table results. Preserve one-time left evaluation,
lazy demand, 3VL, schema, and errors. Repeat with `estimated_rows == 0` but no approved proof
and assert no exact-empty rewrite, cross-referencing the §41.6 Semantic Emptiness Tests.

Directly exercise every unsupported §20.14.1 row, including correlated forms, row-valued
or multi-column scalar/IN forms, ANY/SOME/ALL, LATERAL, CTE/set-operation forms, and
data-modifying subqueries. Assert the architecture-defined `UnsupportedFeature`,
`UnsupportedCorrelation`, type, or syntax category, useful source span, and absence of an
executable logical/physical node. Parser/AST capacity and hypothetical decorrelation never
authorize support.

---

### Catalog Tests

Preserve bootstrap/open and ordinary metadata coverage:

```text
bootstrap open
create table metadata
create index metadata
reopen persistence
name lookup by normalized identifier
quoted identifiers
stable object IDs
schema version increment
descriptor-cache invalidation
transactional catalog visibility
DROP retirement
```

Bootstrap tests create and reopen the minimal locator plus six self-hosted catalog
relations, then cross-check built-in identities and reconstruct immutable descriptors. Use
corrupt bootstrap/self-description mismatch fixtures only through the owning lifecycle/
catalog validation procedures; do not reproduce persisted catalog layouts here.

#### Identity and historical schema

Create table/index/constraint objects, record their durable IDs, drop/abort/retire them,
create replacements, and reopen. Assert zero is never allocated, crash/abort gaps remain
consumed, and TableId/IndexId/ConstraintId/FileId plus historical table-local ColumnIds are
never reused. Do not assert a particular gap size or allocator implementation.

Build several SchemaVers for one TableId and retain tuples/descriptors requiring the older
versions. Resolve current and historical valid versions and compare immutable ColumnIds,
physical/logical positions, types, defaults, and index-key reconstruction inputs. An
unavailable or corrupt required historical version produces its architecture-owned error;
the cache never substitutes the current schema.

#### Snapshot-aware cache and DDL visibility

Use two transactions with deterministic barriers around CREATE/DROP commit and abort.
Assert the creator sees completed earlier DDL after its CommandId boundary, another
transaction sees no uncommitted metadata, READ COMMITTED sees committed metadata at the
appropriate statement snapshot, and an older REPEATABLE READ snapshot retains its prior
view. Cache hits must produce the same result as snapshot-aware catalog relations.

Borrow an immutable descriptor in the old snapshot, publish a newer committed descriptor,
and invalidate/replace current cache ownership. Assert the borrowed object and its schema
remain byte/field unchanged, a new eligible snapshot may receive a distinct new descriptor,
and delayed invalidation/install cannot leak metadata across snapshots or regress current
identity. Addresses may help prove non-mutation but are not semantic IDs.

#### CREATE abort and DROP retirement

Abort CREATE after private allocation, after durable final-name publication, and after
transaction-owned catalog rows. Assert catalog invisibility, consumed IDs, deterministic
orphan-retirement ownership, and no treatment of filesystem existence as committed object
publication. Reuse the §41.3 lifecycle/orphan tests for namespace cleanup details.

For DROP, test pre-commit visibility, abort restoration, committed disappearance from new
snapshots, and continued safety for old snapshots/descriptors. Physical unlink waits for
the architecture's snapshot/descriptor/page/file-owner retirement conditions; identity is
not reused and cleanup failure does not mutate the committed catalog outcome. This section
does not duplicate vacuum or file-retirement protocols.

---

### Logical Planner Tests

Given bound statements, assert canonical logical-plan shapes.

Examples:

```sql
SELECT a FROM t WHERE b > 5;
```

becomes:

```text
Project(a)
  Filter(b > 5)
    Get(t)
```

Aggregate example:

```sql
SELECT dept, COUNT(*)
FROM emp
WHERE active
GROUP BY dept
HAVING COUNT(*) > 3;
```

becomes canonical:

```text
Project
  Filter(HAVING)
    Aggregate
      Filter(WHERE)
        Get(emp)
```

Add representative canonical shapes for no-FROM Values, INNER/LEFT/CROSS joins, DISTINCT,
Sort/Limit, INSERT VALUES/SELECT, UPDATE/DELETE with hidden RID and required old values,
supported DDL/control statements, derived-table `LogicalSubqueryScan`, and scalar/EXISTS/IN
independent child modes. Inspect LogicalSlotId identity, output order/type/nullability,
stable descriptor references, and hidden-system visibility. Do not assert physical access
or join algorithms.

---

### Logical-Plan Validator Tests

Test the logical validator independently by constructing immutable malformed plans that
bypass parser/binder construction. Supply otherwise valid descriptors so each test isolates
one invariant. Validator failure is an internal architecture error, not a user SQL syntax
error, and occurs before optimization or execution.

| Area | Malformed logical state | Required assertion |
|---|---|---|
| Slots/schema | Missing child slot, sibling/unavailable slot reference, duplicate output LogicalSlotId, output/expression order mismatch, or schema inconsistent with child/output expressions | Reject without assigning a physical interpretation |
| Types | Unresolved expression, non-BOOLEAN Filter/HAVING/Join predicate, incompatible comparison/cast, aggregate result mismatch, or output type/nullability contradiction | Reject using the already-resolved type contract |
| Operator shape | Wrong child count, condition on CROSS, missing required join condition, misplaced aggregate reference, impossible DML/DDL child, or hidden target RID removed | Reject before rewrite/search |
| Catalog/nullability | Inconsistent TableId/SchemaVer descriptor, mutated/stale identity pairing, or LEFT JOIN right output not null-extended | Reject rather than repair from names/current cache |
| Semantic proof | Empty annotation/replacement with missing, statistics-derived, or operator-invalid provenance | Reject under §§20.17.10 and 35.2 |
| Subquery state | Unsupported mode, wrong arity/type/nullability, missing/multiple independent child, OuterRef/correlated slot, invalid derived slot/name map, or NOT wrapper inconsistent with the mode | Reject using §20.14 without executing the support matrix again |

Maintain positive validator fixtures for every canonical logical family above so a
validator that rejects all plans cannot pass.

#### Validation around rewrites

At rewrite entry, pass a malformed logical plan and assert validation fails before any
rule-match/application counter advances. After each major rewrite phase, use a test-only
malformed transform or injected bad output to assert immediate validation before the plan
can enter another phase or physical search. Valid user SQL may produce a structured
front-end error, but it must never directly manufacture an accepted malformed logical
plan.

---

### Logical Rewrite Tests

For every rewrite rule:

```text
input logical plan
expected transformed plan
```

plus, when an execution implementation is available:

```text
differential semantic test:
    original plan result == rewritten plan result
```

Use randomized data with NULLs.

This is particularly important for:

- boolean simplification,
- predicate pushdown,
- outer joins,
- DISTINCT,
- aggregates.

For every rule, run structural input/output tests both when its preconditions hold and at
the nearest non-matching boundary. Validate the input before matching and the output after
the phase as described above. Differential tests use NULL-rich and FLOAT64 edge data,
controlled errors, and normalized unordered results.

#### Volatility, folding, and Boolean 3VL

Use test registry descriptors for IMMUTABLE, STABLE, and VOLATILE expressions without
changing the v1 user-visible empty scalar-function registry. Count evaluations and inject
errors to assert rewrites never duplicate/drop/reorder VOLATILE work, fold STABLE/VOLATILE
expressions as immutable constants, or hide/newly force an error.

Constant folding covers deterministic scalar results, NULL, casts, arithmetic errors,
searched CASE, and short-circuited branches using §17.10.2 precedence. Boolean
simplification runs complete TRUE/FALSE/UNKNOWN fixtures; accept only identities valid
under SQL 3VL and the architecture's evaluation-demand rules.

#### Predicate pushdown and projection pruning

For predicate pushdown, construct INNER/CROSS and LEFT JOIN plans with left-local,
right-local, cross-side, NULL-sensitive, and volatile/erroring predicates. Assert legal
inner-side pushdown, preserved ON-versus-WHERE meaning, and no movement across a LEFT JOIN
null-extension boundary without a separately proven transformation. A lower estimate is
not rewrite authority.

Projection-pruning fixtures place required values in parent projection, filters, join
conditions, grouping/aggregate arguments, sorting, DML assignments/RETURNING, hidden target
RID/system slots, and subquery/derived-slot maps. Assert only truly unused outputs are
removed and all retained LogicalSlotIds keep their semantic identity.

#### Exact-empty and subquery-sensitive rewrites

Exercise every §20.17.10 propagation case with approved proof provenance and with an
otherwise identical numerical-zero estimate lacking proof. Only the former may introduce a
schema-equivalent empty/no-op plan. Preserve LEFT JOIN rows, global-aggregate one-row
semantics, DML completion/affected-row behavior, and subquery lazy demand/left evaluation.
Cross-reference the §41.6 Semantic Emptiness Tests rather than repeating estimator setup.

For supported subquery semi/anti/marker or boundary-removal rewrites, compare against the
canonical independent fallback. Preserve 3VL, duplicate behavior, cardinality checks,
snapshot/CommandId, exact proof provenance, and §20.14.12 error precedence. Decorrelating
an unsupported form is not a way to make it supported.

---

### Catalog / Front-End Benchmarks

Performance is less critical than execution/storage, but benchmark enough to avoid pathological architecture.

Measure:

```text
lexer MB/s
parser statements/sec
binder latency
catalog name lookup latency
large SELECT-list binding
large expression-tree binding
logical plan construction time
```

Optimizer planning benchmarks cover complex-query planning work separately.

---

### Parser/AST Memory Benchmark

Track allocations/bytes for:

```text
100-column SELECT
large VALUES insert
deep boolean expression
multi-join query
```

Arena allocation should keep allocation count low.

Do not optimize syntax parsing before profiling, but prevent obvious per-token/per-node heap churn.

---

### Front-End Fuzzing

Fuzz at least:

```text
lexer
parser
tuple/type literal conversion
bound expression constant evaluator
```

Requirements:

- no crashes,
- no out-of-bounds access,
- no undefined behavior,
- bounded failure behavior,
- useful structured lexer/parser/binder/type error instead of generic failure,
- no unbounded parser-recovery loop.

SQL parser fuzzing is high-value because arbitrary text reaches it directly.

Use reproducible corpus entries and report the generated seed on failure. Include arbitrary
bytes, deeply nested delimiters, truncated literals/comments, long identifiers/lists,
numeric boundaries, and malformed multi-statement batches. Literal/type-conversion and
bound-constant-evaluator fuzzing must preserve the closed Chapter-17 error categories and
bounded allocation behavior.

Parser fuzzing may naturally reach subquery syntax, but deterministic table-driven
§20.14 tests remain authoritative for supported and rejected forms. Fuzzing does not stand
in for the subquery support matrix or logical-validator malformed-plan corpus.

---

## Execution Verification

### Execution Testing Strategy

#### Operator unit tests

Feed synthetic DataChunks directly into:

```text
Filter
Project
HashJoin
Aggregate
Sort
Limit
```

without SQL parser/storage.

#### Pipeline tests

Construct physical plans manually and validate chunk flow/dependencies.

#### End-to-end tests

SQL -> storage results.

#### Differential tests

Compare supported SQL results against a reference DB where semantics align.

#### Spill tests

Use tiny memory budgets to force:

```text
hash join spill
aggregate spill
external sort
DML spool spill
```

#### Cancellation tests

Cancel during:

```text
scan
join build
join probe
aggregate build/finalization
sort spill
merge
DML spool
RETURNING spool
subquery side-plan execution
```

and verify the resource and task-graph cleanup defined below.

Use deterministic barriers for execution dependencies and fault injection. Randomized
execution tests use fixed/reported seeds so failures can be reproduced. Differential tests
supplement but do not replace explicit invariant and malformed-plan tests.

---

### Physical-Plan Validator Tests

Test the execution-layer `PhysicalPlanValidator` independently from the optimizer. Build
physical plans directly so invalid state cannot be filtered out by ordinary planning. The
validator contract is owned by [`ARCHITECTURE.md`](ARCHITECTURE.md) §41.5, with plan,
property, and final-validation rules in §§22.3–22.8 and 38.24.

For each invalid plan:

1. construct the minimum immutable physical tree that contains one targeted defect;
2. supply otherwise valid resolved descriptors and execution context so the intended defect
   is the first failure;
3. invoke validation before pipeline construction or operator state creation;
4. assert a structured internal validation error identifying the violated invariant;
5. assert that no source, sink, task, memory reservation, spill file, page operation, or
   transaction-owned mutation was started.

The malformed-plan matrix includes:

| Area | Invalid plans to construct | Required assertion |
|---|---|---|
| Child/output slots | Missing child, reference to a slot absent from the required child, duplicate output `LogicalSlotId`, unavailable consumed slot, or output schema inconsistent with declared slots | Reject before execution; never substitute vector position, ColumnId, or SQL name for the missing identity |
| Physical expression types | Unresolved or mismatched arithmetic/comparison/cast/predicate type, non-BOOLEAN predicate, aggregate argument/result mismatch, or expression result inconsistent with its output slot | Reject using the closed type/operator registry; execution performs no new coercion or type resolution |
| Hidden target RID | UPDATE/DELETE path drops, aliases, substitutes, or makes the required RID unavailable before locking, target spooling, index maintenance, or mutation | Reject before target discovery or mutation |
| Join keys | Unequal key counts, pairwise incompatible resolved key types, unresolved coercion, invalid residual predicate, or output mapping inconsistent with preserved side | Reject rather than resolving types in the join implementation |
| Join type/capability | Unsupported logical join mode, unavailable physical algorithm, or LEFT plan with invalid preserved/build/probe orientation | Reject rather than silently map to a nearby join algorithm |
| Ordering/properties | Missing ordering slot, invalid type/collation/null-order metadata, false provided ordering, or incoherent child required/provided properties | Reject the property claim; incidental runtime row order is not accepted as proof |
| Pipeline graph | Invalid source/streaming/sink role, backward/cyclic breaker dependency, consumer runnable before successful Finalize, or impossible child execution arrangement | Reject before creating a runnable task graph |
| Memory/spill | Missing query-memory ownership, negative/nonfinite declaration, absent required spill capability, or spill-capable blocker without its required manager/policy declaration | Reject rather than infer an untracked runtime policy |
| Query context | Missing/incompatible transaction, snapshot, CommandId, read epoch, cancellation state, query memory, SpillManager, or immutable descriptor ownership | Reject validation or execution entry before operator work |
| Semantic proof | Empty/no-op physical replacement with absent, malformed, or statistics/numerical-estimate provenance | Reject unless the proof is one of the exact sources authorized by §§20.17.10 and 35.2 |
| Subquery structure | Unsupported semantic mode, mode/child arity mismatch, `OuterRef`, missing or multiply shared occurrence state, invalid lazy side-plan dependency, or undeclared scalar-two-row/EXISTS-one-row/IN-complete-build behavior | Reject using §§20.14.7–20.14.8 without executing the SQL-level support matrix here |
| Derived mapping | Duplicate/missing derived slot, inconsistent child-to-derived map, leaked child name, or an execution node attempting unresolved SQL name lookup | Reject; execution consumes resolved slot identities only |

Hidden-RID positive tests carry the same RID slot through valid filter/project/target-spool
paths that are allowed to preserve it, then consume it exactly once at UPDATE/DELETE. Valid
ordering tests similarly prove that a compatible IndexScan or Sort declaration is accepted
while a SeqScan's incidental physical traversal is not promoted to SQL ordering.

Maintain a positive corpus of representative valid plans:

```text
SeqScan and IndexScan
Filter -> Project -> ResultSink
NestedLoopJoin and HashJoin
HashAggregate
Sort -> Limit
Insert / Update / Delete with required hidden target state
resolved DDL, Vacuum, Analyze, and Explain control operators
supported scalar/EXISTS/IN physical side-plan roles
```

Each plan must validate and then execute against a small deterministic fixture. This
prevents an implementation that obtains rejection coverage by rejecting every plan.

#### Validation before data-changing effects

For each data-changing operator family, place a validator defect both above and below that
operator in the immutable tree. Snapshot heap/index/catalog bytes, WAL end,
`current_statement_has_published_write`, transaction state, logical lock ownership, and
the RETURNING/result sink before execution entry. Assert:

```text
validation fails
no pipeline or operator state begins
no heap/index/catalog/WAL mutation occurs
no transaction-owned write publishes
no RETURNING row is exposed
transaction outcome follows the §39.1 validation-error path
```

This procedure cross-references the Statement Failure and Transaction-State Tests; it does
not define another transaction-state policy.

---

### Vector Correctness Tests

Test every kernel across:

```text
FLAT
CONSTANT
DICTIONARY
```

inputs and combinations.

Include:

```text
all-valid
all-NULL
mixed validity
non-identity selection
nested dictionary normalization
empty chunk
full 1024-row chunk
```

Results must be representation independent.

Also verify common chunk invariants from `ARCHITECTURE.md` §§23.1–23.8:

- every column has the same logical cardinality;
- selected indices remain within physical bounds;
- validity outside active positions cannot affect results;
- CONSTANT validity repeats correctly;
- nested dictionaries flatten to one effective selection while preserving order and
  validity;
- cardinality zero is an ordinary empty chunk, not end-of-stream.

---

### Expression Execution Tests

Run every supported physical expression kernel over all applicable FLAT, CONSTANT, and
DICTIONARY input combinations, with identity/non-identity selections and all-valid,
all-NULL, and mixed validity. Compare against a scalar/reference oracle that implements the
closed Chapter-17 registry. The same logical input must produce the same value, NULL mask,
or controlled error regardless of representation or chunk boundary.

#### Checked integer arithmetic and division

Exercise every integer overload in `ARCHITECTURE.md` §§17.6.1–17.6.2 and 39.3.1. Use
systematic neighborhoods around each type's minimum, maximum, `-1`, `0`, and `1`, plus
generated near-boundary operand pairs. Cover:

```text
addition overflow
subtraction overflow
multiplication overflow
unary-minus overflow
division and remainder by zero
MIN_INT / -1
MIN_INT % -1 where defined by the registry
positive/negative quotient and remainder combinations
```

Assert the architecture-defined exact result or `ArithmeticError`; do not evaluate the
oracle through C++ signed-overflow behavior. Put a failing row at the beginning, middle, and
end of a vector and behind active/inactive selections to prove that only semantically
demanded rows raise an error.

#### BOOLEAN, casts, and FLOAT64

Use the complete three-by-three input matrix over TRUE, FALSE, and NULL for `NOT`, `AND`,
and `OR`. For AND/OR, place volatile/error-producing expressions on the right and prove
that §17.7.3's per-row short-circuit selection evaluates only demanded rows across every
vector representation.

For every supported cast in the closed registry, test successful boundary values, NULL,
invalid conversion, and range overflow. Run each through direct expression evaluation and
through selected/dictionary vectors. Assert the declared result type and controlled
`CastError`; execution must not select a new coercion absent from the bound expression.

Exercise FLOAT64 scalar arithmetic, comparison, and division under §§17.4.3, 17.6.2, and
39.3.2 with:

```text
finite boundary and subnormal values
+Infinity and -Infinity
multiple NaN payload inputs and canonical NaN results where required
-0.0 and +0.0
finite nonzero / ±0.0
±0.0 / ±0.0
Infinity / Infinity
```

Check exact result bits where the architecture fixes canonical representation or signed
zero, and semantic comparison classes otherwise. Compiler fast-math behavior is not an
alternative oracle.

#### Hash/comparison consistency

For ordinary join equality and GROUP BY/DISTINCT equality modes, generate values classified
as equal and assert equal hash values using `ARCHITECTURE.md` §28.2.1. Include NULL handling
for each mode, FLOAT64 signed zero and canonical-equivalent NaNs, exact VARCHAR bytes,
numeric/temporal types, and composite keys. Cross-check comparison, grouping, hash join,
aggregate grouping, and index-key semantics where each mode applies; a collision remains
legal only when full semantic comparison distinguishes unequal values.

---

### String Lifetime Tests

Create tests that deliberately:

1. scan VARCHAR data,
2. release/unpin source pages,
3. recycle source chunks,
4. continue downstream blocking processing,
5. verify strings remain valid.

Use allocator poisoning/debug memory where practical to catch accidental borrowed-pointer retention.

---

### Pipeline Finalization and Resource Tests

#### Blocking-state publication

Construct manual pipelines for hash-build/probe, aggregate build/output, sort input/output,
DML target-spool/write, and each supported materialized subquery side plan. Place barriers:

```text
before the final input is consumed
during local-state Combine
during global Finalize
immediately before dependent-task publication
```

At every barrier, attempt to schedule or inspect the consumer. Assert that dependency
counters keep it non-runnable and that no partial hash directory, aggregate group source,
sort run set, target spool, or subquery build is observable. After successful Finalize,
publish the complete immutable/blocking state once and allow dependents to run. Finalize
failure must publish no successful dependency transition.

#### Cancellation resource matrix

Cancel at deterministic points during scan, join build/probe, aggregate build/finalize,
sort spill/merge, DML target/RETURNING spooling, and subquery side-plan execution. For each
case capture and then assert release/destruction of:

```text
runnable and blocked tasks/dependency entries
worker-local and global operator state
borrowed chunks and owned RowCollections
query arena backing and MemoryReservations
spill buffers, temporary blocks, runs, and files
page guards and read-epoch ownership
profiling registrations
```

No partially failed task graph may remain runnable. Query-owned resources return to their
pre-query accounting/namespace state. Transaction-owned logical locks and gates must remain
owned until the command/transaction layer performs the terminal path described by §39.1;
executor cleanup must not release them independently.

#### Execution-failure cleanup

Repeat the cleanup matrix with controlled arithmetic, cast, expression, OOM, SpillIO,
transaction-conflict, constraint, and operator-internal execution errors rather than
cancellation. Inject before input consumption, after partial blocking state, during spill,
and after a DML target/result spool has accumulated rows. Assert:

- the structured lower-layer cause reaches the command/transaction layer unchanged;
- no use-after-free, borrowed-data lifetime violation, leaked reservation/file, or runnable
  pipeline remains;
- no successful partial result or profile completion is fabricated;
- executor code performs no physical transaction undo, same-TxnId retry, COMMIT/ABORT
  decision, or independent transaction-lock release;
- statement/transaction outcome is established only by the existing §39.1 verification
  procedures.

---

### Parallel Execution Tests

For every capability-enabled parallel operator, execute one immutable physical plan and
deterministic input with one worker, two workers, and several workers. Use the same
transaction, effective snapshot, CommandId, read-epoch protection, memory policy, and
cancellation state. Compare logical values and controlled errors; normalize output only
when the physical plan declares no ordering.

Include parallel SeqScan, hash-build/probe, hash aggregate, and sort where supported. Assert
that required ordering is produced only by an ordering-preserving/finalized source and that
parallel SeqScan itself advertises none.

Use interleaving barriers to inspect state ownership:

- immutable plan/configuration is shared and unchanged;
- cursors, chunks, expression scratch, continuation, local aggregate/sort state, and hot
  profiling counters are distinct worker-local objects;
- global mutable state changes only through declared synchronization, Combine, and Finalize;
- no worker embeds transaction/snapshot state in the immutable plan or obtains a different
  statement view;
- dependency counters publish consumers only after all required predecessors finalize;
- cancellation stops new unnecessary tasks and drains running tasks at defined boundaries.

Aggregate numerical equivalence uses the stronger Aggregate Tests below rather than an
unordered approximate comparison.

---

### Scan and Unary Operator Tests

Feed SeqScan, IndexScan, Values, Filter, Project, Limit/Offset, and ResultSink fixtures with
empty input, one row, partial chunks, full chunks, multiple chunk boundaries, NULL-heavy
data, and non-identity selections.

Verify:

```text
SeqScan applies heap MVCC and historical schema decoding
SeqScan returns no VARCHAR pointer into an unpinned page
IndexScan treats entries as candidates and rechecks heap visibility
index-derived RIDs remain read-epoch protected while retained
Filter preserves row order and treats empty output as a batch, not end-of-stream
Project emits declared LogicalSlotId order and owns computed VARCHAR results
LIMIT applies OFFSET first and emits exactly the required row count
LIMIT early-stop is not query cancellation
Values performs no parsing/type resolution
ResultSink never exposes an expired borrowed chunk
```

For IndexScan, compare advertised order against semantic key ordering and request
ASC/NULLS-FIRST cases the forward path can satisfy. Verify unsupported direction/null-order
requirements are not claimed. For every unary operator, compare direct synthetic-chunk
execution with an equivalent manually constructed pipeline.

---

### Hash Join Tests

Cover:

```text
no matches
one-to-one
one-to-many
many-to-many duplicates
NULL keys
composite keys
hash collisions
residual predicates
LEFT JOIN unmatched rows
output larger than one chunk per probe row
tiny memory forced Grace spill
skew partition
```

Compare to nested-loop join results on randomized small inputs.

---

### Aggregate Tests

Cover:

```text
empty input
global aggregate
one group
many groups
NULL group keys
all-NULL aggregate input
composite group keys
VARCHAR keys
forced spill
partial combine
COUNT/SUM/MIN/MAX/AVG
```

Randomized results should compare against a simple reference implementation.

Directly execute every normative boundary vector in `ARCHITECTURE.md` §29.3.8. Synthetic
partial states may represent COUNT/integer boundaries that cannot be materialized with a
practical number of rows.

#### Exact aggregate state and finalization

For each aggregate overload in §29.3.2, separately test Initialize, vector Update, Combine,
Finalize, and Destroy. Assert:

- NULL/empty behavior and result type;
- exact worker-local state remains valid when an integer subtotal leaves the final INT64
  domain but later values cancel it;
- COUNT and integer SUM range errors occur at Finalize rather than order-dependent Update
  or Combine points;
- AVG merges exact sum/count state and does not average partial averages or round SUM first;
- MIN/MAX retain the architecture's canonical representative for equal FLOAT64 classes and
  own VARCHAR bytes;
- every group's numerical state is validated before any successful aggregate-row prefix is
  exposed, with the lowest semantic aggregate ordinal supplying competing numeric errors.

Inject OOM and spill failures while exact state is growing. Assert controlled resource
failure and complete cleanup; an approximate or narrowed accumulator is never substituted.

#### FLOAT64 and execution-shape invariance

Exercise §29.3.8's finite cancellation, halfway rounding, subnormal, signed-zero,
infinity, mixed-infinity, and NaN cases for SUM and AVG. Compare exact result bits,
canonical NaN, and `+0.0` exact-zero results. These tests use the n-ary §29.3 contract, not
repeated scalar FLOAT64 addition.

For each one logical input/grouping fixture, compare:

```text
one, two, and many workers
one-row, standard, and irregular vector boundaries
serial, balanced, left-deep, right-deep, and varied Merge trees
different input partitions and hash iteration orders
no spill and forced spill
different spill partition/replay counts
hash aggregation and capability-enabled ordered aggregation
```

Values and numerical errors must be identical. Spill/reload must preserve every exact
integer/dyadic/count/special-value/canonical-candidate state component rather than only a
rounded scalar subtotal.

---

### Sort Tests

Cover:

```text
ascending
descending
NULLS FIRST
NULLS LAST
multiple sort keys
equal keys
VARCHAR
FLOAT64 edge cases
empty input
one row
forced external runs
multi-pass merge
```

Compare output ordering against the semantic comparator, not raw bytes.

---

### DML Execution Tests

Specifically test Halloween protection.

Example:

```sql
CREATE INDEX idx_x ON t(x);
UPDATE t SET x = x + 1 WHERE x < 100;
```

Run through an index access path and verify each qualifying logical target is updated once.

#### Target spool and one-target-once behavior

Run UPDATE and DELETE with an in-memory target spool and with a tiny memory budget that
forces spill. Create join-expanded inputs that produce one target RID repeatedly and assert
that the finalized spool contains each physical target at most once. Change indexed keys and
predicate values so a streaming mutation would rediscover rows; assert target discovery
finishes before any write and every qualifying target changes once.

At target-spool append, spill-write/read, deduplication, and Finalize boundaries, inject
cancellation, OOM, SpillIO, and expression errors. Assert no target mutation starts before
successful Finalize and every temporary row, reservation, block, and file is cleaned.

#### Revalidation, UNIQUE integration, and failure state

Use deterministic concurrent writers to change a materialized target before its write
phase. After logical lock acquisition, require re-fetch and exact identity/visibility
revalidation. Cross-reference the Isolation Tests and Statement Failure and
Transaction-State Tests for READ COMMITTED pre-write retry and REPEATABLE READ conflict
outcomes; the executor must surface the structured conflict and must not invent another
retry/abort rule.

For key-changing UPDATE, execute the integration path against the complete UNIQUE/PRIMARY
KEY procedures above, including logical key-lock wait/recheck and exact self-exclusion.
Do not duplicate §11.10's truth table in this section.

Inject operator failures before target publication, after one or more transaction-owned
writes, and while target/RETURNING state is retained. Assert:

- no executor-local partial-success result escapes;
- target and result spools unwind according to query ownership;
- the structured error reaches the command/transaction layer;
- §39.1's first-published-write rule alone determines transaction state;
- executor code performs neither statement undo nor transparent retry after publication.

#### RETURNING exposure

Produce several RETURNING rows, then fail a later row/write/spool operation. No successful
prefix may escape that failed statement attempt. For an explicit transaction, expose the
complete result only after the statement's safe execution boundary. For autocommit, retain
the request-owned spool through implicit COMMIT C4–C5 before exposing rows or successful
completion.

Inject known pre-durable COMMIT failure and transport failure during/after post-C5 delivery.
Compare with the §41.3 RETURNING and COMMIT Fault-Injection Tests: the first exposes no
rows, while the latter cannot redefine a durable COMMITTED transaction even if the client
observes only a prefix.

The DML integration matrix therefore includes:

```text
target-spool spill
concurrent update revalidation
READ COMMITTED statement retry
REPEATABLE READ conflict abort
RETURNING buffering
unique-key update
```

---

### Control-Operator Tests

Construct direct valid physical plans for architecture-supported resolved control roles:

```text
PhysicalCreateTable / PhysicalCreateIndex / PhysicalDrop
PhysicalVacuum
PhysicalAnalyze
PhysicalExplain
```

Assert that DDL dispatches to Chapter 21's catalog/storage protocol under its existing
transaction gates; VACUUM dispatches to Chapter 14 reclamation; and ANALYZE dispatches to
Chapter 34's resolved collection/publication protocol. These operators consume stable IDs
and immutable descriptors, perform no execution-time SQL name resolution, and do not enter
ordinary join-order/access-path enumeration to rediscover their dedicated work.

Run context validation, cancellation, structured failure propagation, single-coordinator
ownership, cleanup, and profiling for each role. Detailed catalog, vacuum, statistics, and
transaction semantics remain owned by their existing verification sections; this subsection
tests only physical dispatch and execution-path correctness.

---

### EXPLAIN ANALYZE and Profiling Tests

Build a small physical tree with controlled source rows, predicate rejections, join
multiplicity, aggregate groups, and spill behavior. Run ordinary EXPLAIN and EXPLAIN
ANALYZE, then compare the output with direct operator/pipeline instrumentation under
`ARCHITECTURE.md` §§40.2 and 40.6–40.8.

Assert:

- physical operator identity, child relationships, relevant required/provided properties,
  and estimates are inspectable without reparsing SQL names;
- actual input/output/filtered rows and invocation/chunk counts come from operators that
  executed rather than copied optimizer estimates;
- every physical node reports estimated rows, actual rows, and q-error with the exact
  zero/infinity handling defined by §40.8;
- parent input counts agree with the rows/chunks emitted by the observed child boundary,
  subject to explicit pipeline buffering/early-stop semantics;
- operator spill bytes/partitions/runs and peak attributed memory agree with SpillManager
  and QueryMemoryManager observations;
- pipeline IDs, dependency wait/execution durations, worker counts, morsels/tasks, and
  chunks correspond to the task graph;
- parallel hot counters combine from worker-local state at synchronization/finalization
  boundaries without changing execution semantics.

Do not assert exact wall-clock or CPU durations. Check only nonnegative/finite values,
presence where supported, and internally consistent relationships.

Cancel or inject failure before execution, during streaming work, during blocking
finalization, and after partial output. The profile must mark incomplete/failed/cancelled
execution distinctly, retain only counters for work actually performed, expose no
fabricated successful final row count, and leave no live profiling registration after query
cleanup. If diagnostics remain inspectable after failure, they must not imply successful
pipeline Finalize or transaction completion.

Semantic-empty proof kind and numerical estimate provenance must remain distinguishable at
the physical EXPLAIN/validator boundary. Detailed statistics provenance, optimizer search,
cost, memo, and q-error verification remains in the §41.6–§41.7 coverage areas.

---

### Execution Microbenchmarks

Measure:

```text
scan rows/sec
scan bytes/sec
filter rows/sec
projection arithmetic rows/sec
VARCHAR comparison rows/sec

hash build rows/sec
hash probe rows/sec
hash join output rows/sec

aggregate input rows/sec
groups/sec

in-memory sort rows/sec
external sort MB/sec

chunk allocations/query
general allocator calls/query
temporary bytes/query
```

Benchmark with NULL-free and NULL-heavy data.

---

### Vector Size Benchmark

Benchmark at least:

```text
256
512
1024
2048
4096
```

rows/chunk on representative workloads.

The architecture default remains 1024 until measurements justify changing it.

Do not assume a larger vector is always faster; cache footprint and branch behavior matter.

---

### End-to-End Execution Benchmarks

Create repeatable workloads for:

```text
point lookup
selective index range
full table scan
filter + projection
small join
large hash join
group aggregate
ORDER BY
ORDER BY LIMIT
UPDATE by index
bulk INSERT
concurrent read/write
```

Track:

```text
latency
throughput
CPU
memory peak
buffer hits/misses
WAL bytes
spill bytes
```

---

## Statistics and Optimizer Verification

### Statistics Tests

Test ANALYZE on controlled distributions:

```text
uniform integers
highly skewed MCV
many NULLs
all same value
all distinct
two-mode distribution
monotonic values
VARCHAR prefixes
FLOAT64 edge values
```

Verify:

```text
row counts
NDV accuracy bounds
MCV detection
histogram boundaries
null fraction
width estimates
```

Use exact reference counts for small inputs and fixed-seed generators for larger inputs.
Run each applicable distribution through in-memory candidate construction, canonical
serialization/decoding, and normalized descriptor materialization. Approximation quality
may vary within the architecture's contract, but descriptor validity and normalization
must not depend on input order, hash iteration, host rounding mode, or locale.

When §34.8's bounded small-table exact mode applies, compare every non-NULL frequency and
NDV with an exact map. Repeat immediately below/above the configured threshold and with an
insufficient maintenance-memory budget to verify that changing collection mode changes
approximation strategy, not descriptor validity or SQL semantics.

### Statistics Algorithm Tests

#### HLL NDV collection

Exercise the process-local HLL state defined by [`ARCHITECTURE.md`](ARCHITECTURE.md)
§§34.9 and 34.14.6.5 with empty input, one-value and very small domains, medium and large
known cardinalities, heavy duplicate repetition, and, when the collector exposes a merge
operation, disjoint partitions merged in varied orders. Assert:

```text
NULL never enters the sketch
canonical-equal values hash as one value
precision/register count and register domains are exact
the bounded writer result lies between observed lower bounds and the exact non-NULL count
nonfinite, negative, or malformed candidate state fails before persistence
```

Measure relative-error distributions over reproducible seeds and several cardinality
scales rather than requiring one sketch instance to equal one fragile estimate. Compare the
observed distribution with the HLL behavior implied by the architecture's fixed precision;
do not introduce a separate universal tolerance. When merge is supported, partitioned
collection must obey the same statistical and deterministic output bounds as direct
collection.

#### Heavy hitters and MCVs

Feed the bounded heavy-hitter collector fixtures with one dominant value, several dominant
values, values immediately around the top-entry cutoff, uniform data, many NULLs, and a
long duplicate-heavy tail. Compare with exact reference frequencies and assert:

- representable true heavy hitters are retained according to the collector's bounded-error
  contract;
- candidate count stays within the §34.10 bound and no unbounded exact map is required;
- NULL is excluded from MCV identity while each persisted frequency remains relative to
  all visible rows;
- canonical-equal FLOAT64 values and exact VARCHAR bytes use the statistics equality mode;
- canonical ordering, uniqueness, frequency bounds, and tie ordering satisfy §34.14.6.3;
- MCV mass is removed exactly once from the residual population.

Vary input and merge order to exercise bounded-error behavior. Approximate candidate sets
need not be identical unless the collector contract requires it; every resulting accepted
set must nevertheless receive the same deterministic canonical validation, ordering, and
normalization.

#### Reservoir and histogram construction

Use fixed reservoir choices or an injectable deterministic sampler so the same sampled
state can be replayed. Cover empty/tiny input, all-equal values, many duplicates, monotonic
input, bimodal/skewed data, exact bucket-count boundaries, and samples larger than the
configured reservoir. Test the structure directly rather than only through downstream
selectivity.

After MCV removal, assert that histogram boundaries are nondecreasing under the statistics
scalar order, equal adjacent boundaries retain their defined grouped-boundary meaning,
bin count remains bounded, each mass is valid, and normalized residual-bin mass covers the
residual population within §34.14.6's exact tolerance. Decreasing boundaries, MCV/boundary
identity overlap, impossible mass, and illegal degenerate forms invalidate the complete
candidate.

#### Value order, mass, and width

Use controlled rows with calculable NULL, MCV, residual, and width components. Verify NDV
excludes NULL, NULL fraction is separate, MCV and histogram mass do not overlap, and
probability components satisfy the exact dyadic construction and normalization rules in
§34.14.6. Width tests include empty/all-NULL columns, fixed-width values, and VARCHAR
length distributions, including maximum-observed-width boundaries.

Statistics scalar-order fixtures include:

```text
VARCHAR: empty bytes, embedded zero bytes, strict prefixes, high unsigned bytes,
         and values around any persisted-scalar length boundary
FLOAT64: negative finite values, -0.0/+0.0, infinities, canonical-equivalent NaNs,
         and adjacent total-order values
```

Assert binary VARCHAR ordering and the canonical FLOAT64 total order. Raw object bytes,
locale collation, and host unordered-NaN comparison are not valid oracles.

---

### Statistics Publication and Versioning Tests

#### ANALYZE snapshot and candidate ownership

Run ANALYZE with deterministic barriers while concurrent transactions insert, update,
delete, commit, and abort. Capture the statement's effective snapshot and independently
compute its visible rows. Assert that SQL-visible row/column statistics include exactly
that snapshot, later visibility changes do not mutate the in-progress candidate, and
physical heap/index-pressure observations remain separate approximate metadata. Reuse the
MVCC fixtures above for visibility semantics rather than defining another snapshot model.

For each statement attempt, assert one exact `StatsVersion = (TxnId, CommandId)`, one owner
transaction, and one version on every TABLE/COLUMN/INDEX row and payload. Repeated ANALYZE
commands in one transaction receive distinct monotonic CommandIds. Cross-version chunks,
invalid identifier domains, and row/payload version disagreement reject the generation.

#### Manifest completeness and terminal publication

Build a complete version, then independently remove, duplicate, reorder, corrupt, or
substitute each required chunk and each TABLE-manifest member. Also add an unlisted or
wrong-object COLUMN/INDEX member. The loader must accept only one complete, internally
consistent generation; it never combines members from different StatsVersions or salvages
valid scopes from a failed generation.

Place barriers before statement completion, before terminal COMMITTED publication, and at
the C5 cache side effect. Verify:

```text
the owner may use its complete transaction-local descriptor after the CommandId boundary
other transactions keep using the prior committed descriptor before terminal COMMITTED
ABORT/cancellation and incomplete candidates never publish globally
a complete committed generation becomes eligible atomically
existing planners retain their immutable descriptor
```

After durable COMMIT, inject cache allocation/installation and delayed-callback failure.
The transaction remains COMMITTED, the persistent complete generation remains usable, and
the cache establishes the architecture-approved older/missing safe fallback without
publishing a partial descriptor. Cross-reference the §41.3 post-durable COMMIT procedures;
this section does not restate C0–C6.

#### Generation selection and status reclamation

Provide an older complete generation and newer variants that are complete, incomplete,
aborted, unsupported, or corrupt. Load through ordinary catalog MVCC and assert selection
of the greatest visible committed complete valid StatsVersion, otherwise the older valid
generation or explicit missing-statistics fallback. Concurrent publication must not let a
delayed older install replace a newer applicable descriptor.

Freeze/reclaim the outer catalog tuple's creator status, retire the StatsVersion creator's
status history, close/reopen, and reload. Assert that the opaque numeric StatsVersion is
unchanged, directly comparable, and usable without transaction-status lookup, retained
status pages, or terminal-cache reconstruction. Outer catalog MVCC visibility and payload
identity remain distinct.

#### One stable statistics snapshot per optimization

Use optimizer hooks to publish generation B after one optimization has selected generation
A but before it finishes costing. Every table/column/index lookup in that invocation must
retain A's immutable applicable descriptors; no mixed A/B generation may enter one plan.
A later optimization may select B. Repeat with a failed B publication and with a
transaction-local descriptor visible only to its owner.

---

### Statistics Persistence and Validation Tests

Construct catalog rows and chunked payloads directly so each validation layer can be
isolated. Corrupt one aspect at a time:

| Area | Cases | Required result |
|---|---|---|
| Outer/chunk framing | Missing, duplicate, noncontiguous, reordered, trailing, or size-inconsistent chunks | Reject the generation after safe row/chunk reconstruction; malformed core catalog framing retains its owning corruption outcome |
| Header/identity | Magic, flags/reserved fields, scope, payload version, length arithmetic, CRC32C, TableId/ColumnId/IndexId, TypeId, StatsVersion, or schema/object fingerprint mismatch | Classify invalid versus well-framed unsupported statistics exactly as §§34.14.5–34.14.6 require |
| Scalar payload | Truncated/malformed codec, NULL where forbidden, wrong TypeId, noncanonical persisted scalar, or invalid VARCHAR/FLOAT64 carrier | Reject the whole StatsVersion; never parse best-effort |
| Probabilities/widths | NaN, infinity, negative or above-one probability, negative/nonfinite width, illegal correlation, or impossible cross-field relationship | Reject rather than clamp an individually invalid field |
| MCV/histogram | Duplicate canonical identity, invalid tie order, decreasing boundary, MCV/boundary overlap, excessive counts, bad aggregate mass, or illegal degenerate form | Reject the entire generation; never merge/drop individual entries |
| NDV/index consistency | NDV outside §34.14.6.5 bounds, invalid min/max relationship, wrong logical-live index count, or cross-generation disagreement | Reject generation-atomically |

Directly execute every normative boundary vector in §34.14.6.10, including `+0`, `-0`,
one, adjacent out-of-domain values, exact sums at and around `2^-40`, duplicate signed-zero
and canonical-NaN MCV identities, equal/decreasing histogram boundaries, empty/all-NULL/
one-value columns, exhausted residual mass, NDV limits, and correlation endpoints. Use
exact dyadic/rational fixture construction so acceptance does not depend on host floating
evaluation.

For each accepted payload, assert one deterministic process-local descriptor: numerical
zeros normalize to `+0`, scalar identities and MCV order are canonical, residual and bin
masses use the architecture's one-rounding normalization, and serialize/decode yields the
same semantic descriptor. Semantically identical permitted encodings normalize equally;
rejected corruption is never treated as another normalization path.

Use two tables with identical visible rows/selectivity but different dead versions,
physical B+ entries, invisible entries, leaf pressure, and heap correlation. Logical row,
NDV, and selectivity estimates remain equivalent while physical scan/index-pressure fields
and costs may differ. Zero physical/live-entry statistics never suppress runtime access.

---

### Selectivity Estimation Tests

For synthetic data with known distributions, compare:

```text
estimated selectivity
actual selectivity
q-error
```

for:

```text
=
<
<=
>
>=
equivalent lower/upper bound conjunctions
IN
IS NULL
IS NOT NULL
AND
OR
NOT
same-column ranges
```

Include values:

```text
inside MCV
outside MCV
outside min/max
near histogram boundaries
```

For every primitive predicate, inspect the complete finite
`PredicateTruthEstimate {TRUE, FALSE, UNKNOWN}` and assert normalization and legal bounds,
not only filter row count. Directly execute the architecture-owned cases in §§35.6 and
35.10–35.16:

- equality MCV hit/residual/outside-range behavior;
- range boundary inclusion and residual histogram interpolation;
- `IS NULL` and `IS NOT NULL` with and without enforced NOT NULL;
- IN lists with no NULL, one/multiple NULLs, duplicates, and no matching value;
- NOT, AND, and OR over controlled truth triples, including independence fallback;
- same-column equality/range/IN/NULL intersections and exact contradictions.

Direct `BETWEEN` estimator dispatch is not a v1 scalar-registry capability under §35.10;
assert it cannot bypass front-end support checks. The estimator exercises the equivalent
registered lower/upper-bound conjunction only when supplied as a valid normalized logical
predicate.

Same-column fixtures cover overlapping and nested ranges, touching inclusive/exclusive
boundaries, contradictory bounds, and equality inside/outside a range. Assert the
normalized constraint-set path is used instead of multiplying independent estimates.
Statistical min/max can influence its numerical estimate but cannot create the exact
contradiction.

Use exact or deliberately boundary-straddling scalar inputs for probability normalization,
histogram interpolation, and clamping. Keep estimator finalization distinct from persisted
statistics validation: downstream clamping must not legalize malformed statistics.

Measure q-error against actual/reference counts for quality regressions, recording
confidence and provenance. A high q-error is a performance-quality failure under the
chosen regression threshold, not permission to change SQL semantics; conversely, a small
or zero estimate used as semantic proof is a correctness failure regardless of q-error.

---

### Semantic Emptiness Tests

Carry each fixture through estimator output, logical/physical alternative construction,
memo insertion/dominance, final optimizer validation, and differential execution. At every
layer inspect numerical estimates and semantic-proof metadata separately. Unless the exact
§35.2 whitelist and §20.17.10 propagation rule apply, assert:

```text
estimated_rows may be 0
is_provably_empty remains false
normal access/join alternatives remain enumerable
no empty/no-op replacement is accepted
execution still runs and matches the reference result
```

The semantic-emptiness matrix includes:

| Fixture | Required proof/result |
|---|---|
| Rounding, sampling, histogram endpoint, absent MCV, HLL/NDV approximation, independence, clamping, or missing fallback yields numerical zero | No semantic proof; executable alternatives remain |
| Analyze an empty table, then commit a visible matching insert while retaining the stale zero descriptor | Estimate may remain zero; scan/index alternatives remain and execution returns the inserted row |
| Stale min/max, histogram, or MCV suggests an equality/range value is outside the analyzed domain | No proof; a later visible matching row is returned |
| Statistically disjoint join domains later gain a visible overlapping key | No proof; join remains executable and returns the match |
| `null_fraction` is 0 or 1 without an enforced constraint | No proof for `IS NULL`/`IS NOT NULL`; schema constraint and statistics remain distinct |
| Enforced NOT NULL contradicts `IS NULL` | Approved trusted-constraint proof in a row-rejecting context |
| Complete constant FALSE or UNKNOWN predicate, exact contradictory typed literals, or zero-row `LogicalValues` | Approved exact proof only in the contexts permitted by §§20.17.10 and 35.2 |
| LEFT JOIN has a proven-empty right side or an ON predicate proven never TRUE | Join is not empty; preserve every left row and null-extend it |
| LEFT JOIN preserved side is proven empty | Propagated empty proof is accepted |
| Grouped aggregate over a proven-empty child | Empty proof propagates |
| Global aggregate over the same child | Not empty; execution produces the architecture-defined one row |
| Actual SQL `LIMIT 0` | Approved `SQL_LIMIT_ZERO` proof |
| `FIRST_K_ROWS(0)` or another costing objective is zero | No semantic proof and no executor row cap is fabricated |
| Table/index live or physical-entry estimate is zero | Runtime SeqScan/usable IndexScan and heap visibility work are not suppressed |

For every positive proof case, mutate the provenance to statistics, confidence, cost, or
an unapproved metadata source and assert logical/physical/final validation rejection. For
every non-proof zero estimate, perform optimized-versus-reference execution with visible
rows. This is the canonical estimated-zero regression and must also pass through the memo
tests below.

---

### Join Estimation Tests

Synthetic joins:

```text
unique-to-many
many-to-many uniform
hot-key skew
disjoint domains
partially overlapping domains
NULL-heavy keys
duplicate-heavy MCVs
```

Compare baseline NDV estimator against MCV-aware estimator.

Store regression expectations for controlled high-q-error cases.

For each distribution, compute reference pair counts and complete NULL truth mass. Compare
baseline NDV, common-MCV/residual, and trusted unique-key refinements. No common MCV or
statistically disjoint domain may become semantic proof.

LEFT JOIN fixtures independently estimate matched pairs and left rows with at least one
match, and assert output cardinality never falls below the preserved-left estimate unless
that input is itself proven empty. Add controlled DISTINCT and GROUP BY fixtures for one
column, NULL grouping, and multiple columns; compare the §35.21–§35.23 NDV/damping rules
with reference group counts while retaining heuristic provenance.

---

### Access Path Tests

Construct catalog/stats scenarios where optimizer should choose:

```text
SeqScan for low-selectivity predicate
IndexScan for highly selective predicate
IndexScan for ORDER BY avoidance
SeqScan when index correlation is poor and result is large
IndexScan when correlation is high
```

Vary one architecture-owned input at a time:

```text
relation rows and physical pages
predicate selectivity and row width
required/projected payload width
dead-version and physical index-entry pressure
index/heap correlation
effective-cache and calibrated page/CPU cost configuration
required ordering
```

Inspect every enumerated SeqScan/usable IndexScan alternative, its bounds/residuals,
estimated rows/width, physical candidate pressure, provided ordering, and component costs.
Logical selectivity remains common to all physical algorithms; only physical work differs.

Use two fixtures with the same selectivity but different table size, correlation, required
width, ordering, or configured costs and require different legitimate path choices. This
proves no universal selectivity threshold controls access selection. Planning must use the
configured cache model rather than momentary BufferPool residency.

For an index `(a,b,c)`, cover equality prefixes, one range after an equality prefix,
inclusive/exclusive endpoints, duplicates/RID bounds, `IS NULL` on a nullable key, ordinary
`= NULL`, an unconstrained suffix, a later-key predicate without its leading prefix, and
residual predicates. Compare cursor results with the normalized SQL predicate and assert
exact leftmost-prefix bounds, transient sentinel use, and residual classification under
§§36.11–36.14; `IS NULL` may form its exact key bound while `= NULL` may not.

Do not assert arbitrary exact cost numbers unless testing the cost formula itself.

Prefer plan-shape expectations under controlled parameters.

---

### Join-Order Tests

Use known cardinalities where one join order is dramatically better.

Example:

```text
A = 1M rows
B = 1M rows
C = 100 rows

A ⋈ B produces huge intermediate
B ⋈ C highly selective
```

Verify DP chooses the selective early join when statistics indicate it.

Also test a case where a bushy plan wins.

Generate small join graphs and compare enumerated legal trees with an independent exhaustive
reference enumerator. Use distinct BindingIds for self-joins. Cover connected and
disconnected graphs, a query that semantically requires a Cartesian product, and a
connected graph where unnecessary Cartesian partitions must not displace connected
alternatives. Cartesian cost may be high but legality is never changed by a blanket ban.

Build LEFT JOIN regions with inner joins on each side. Assert the outer-join boundary stays
atomic/constrained, legal inner reordering occurs within each side, and a lower cost cannot
authorize crossing the boundary. Differentially execute representative legal reorderings;
inject illegal reorderings only to assert final rejection before execution.

For INNER equijoins, force cases where build-left and build-right are respectively cheaper
and assert both supported orientations are enumerated. For LEFT hash join assert only the
architecture-supported preserved/probe orientation appears. Also compare NestedLoop,
HashJoin, capability-enabled MergeJoin, and INLJ without changing the shared logical join
cardinality. INLJ fixtures include a tiny selective outer with a useful inner index and a
large/unselective outer where repeated lookups lose.

---

### Physical Property and Enforcement Tests

For each OrderingProperty, vary slot identity, direction, NULL order, collation, and prefix
length. Assert exact-prefix satisfaction, rejection of near-matches, and advertised order
only from runtime-capable providers. RequiredSlotSet fixtures retain predicates, join keys,
grouping/aggregate inputs, ordering slots, output values, and hidden DML state while pruning
unused payload.

#### Interesting orders and enforcement

Verify optimizer can retain a slightly more expensive ordered path and use it to avoid:

```text
Sort
```

or enable:

```text
MergeJoin / streaming aggregate
```

when total plan cost is lower.

Compare an unordered cheapest child plus Sort with a more expensive naturally ordered
IndexScan/join/aggregate path. Assert the ordered alternative survives dominance when it
reduces total downstream cost. When no child supplies the exact property, require explicit
Sort with startup, CPU, memory, and spill cost; when the property is already satisfied,
reject redundant Sort enforcement.

For ORDER BY with LIMIT/OFFSET, compare full Sort + Limit against Top-N using small and
large checked `K`, no LIMIT, incompatible order, and different memory targets. Top-N must
be semantically equivalent and selected only by the active objective cost.

For GROUP BY and DISTINCT, compare hash implementations with capability-enabled ordered/
streaming implementations under unordered input, already compatible ordering, required
downstream order, and constrained memory. Include any necessary Sort enforcement in the
total cost; an implementation's local operator cost is not compared as if properties were
free.

---

### Memory/Spill Plan Tests

With identical statistics but different query memory budgets:

```text
large budget
small budget
```

verify plan selection may change between:

```text
HashAggregate vs SortAggregate
HashJoin vs alternative
in-memory Sort vs spill-aware costs
```

where supported.

Also compare `ALL_ROWS` with a safely propagated `FIRST_K_ROWS(K)` objective for the same
logical query. Verify low-startup and low-total-cost alternatives can differ, blocking
operators do not pretend all work scales with K, and objective identity participates in
memo lookup where propagated. `FIRST_K_ROWS(0)` remains costing metadata, not semantic
emptiness.

With absent, rejected, stale, and low-confidence statistics, assert centralized finite
fallback assumptions, explicit confidence/provenance, deterministic planning, retained
runtime alternatives, and semantic correctness. A slower fallback plan is acceptable; a
fabricated proof is not.

---

### Memo and Pruning Tests

Construct memo/search requests that differ only in logical subproblem, required slots,
ordering class, or propagated required-rows objective, and construct alternatives that
differ in feasibility or semantic proof. Exercise insertion, dominance, replacement, and
pruning directly.
Assert:

- the only semantically valid or capability-enabled alternative is never discarded;
- useful ordering and low-startup alternatives survive when required by the active key;
- required slots/properties are never erased by merging distinct states;
- estimated rows and semantic-empty provenance remain orthogonal fields;
- hash-map order, allocator address, worker timing, and pointer values cannot affect
  retention or final selection.

The mandatory estimated-zero regression inserts an executable alternative with
`estimated_rows == 0` and `is_provably_empty == false`, then applies cost ties, dominance,
property pruning, and join-DP transitions. It must remain available whenever required. Run
the selected plan against data containing a visible row/match. A second malformed candidate
that promotes the estimate into proof must be rejected, not allowed to dominate the
executable plan.

---

### Cost Model Tests

For scans, joins, Sort, Top-N, aggregates, DISTINCT, materialization, and spill paths,
evaluate cost components over zero/small/large and boundary-straddling inputs. Assert every
ordinary cost and component is finite and nonnegative, `total = startup + run` for the
full-result objective, and invalid arithmetic cannot leak NaN or an uncontrolled infinity
into comparison. Test exact formulas only with their architecture-owned configured weights;
otherwise assert monotonic relationships and component attribution.

Use otherwise identical plans with large unused columns present/absent from RequiredSlotSet.
Required payload width, decode/materialization CPU, hash state, sort records, and spill
bytes must reflect the pruned plan rather than full table width.

Assign several deterministic query-memory budgets across simultaneously live blockers.
Assert assigned targets do not exceed the phase budget, stable redistribution order is
used, estimated peak is the maximum simultaneous phase rather than a blind sum, and hash,
sort, aggregate, DISTINCT, and materialization spill components respond to the targets.
Required Sort/Top-N/ordered aggregate/DISTINCT enforcement contributes its full startup,
memory, and spill cost before alternatives are compared.

---

### Optimizer Determinism and Resource-Limit Tests

Create exact and near-tied alternatives, then perturb insertion order, allocator layout,
hash seeds/iteration order, and repeat optimization. The canonical structural key must
choose the same plan under §38.4's tie rule. Verify the structural comparison key is
collision-free for the compared alternatives; force or simulate a compact
`PlanFingerprint` collision and prove the hash alone never decides identity, dominance, or
tie order. Fingerprints remain deterministic diagnostics rather than semantic identity or
a cryptographic guarantee.

At, below, and above the exhaustive join threshold, compare explored subsets/partitions
with expected exhaustive or bounded behavior. Independently lower the planning-arena/work
budget through deterministic hooks. Assert transition to the deterministic connected
heuristic, bounded local-improvement passes, bounded memo/arena growth, legal complete plan
return when fallback fits, and controlled `OptimizerResourceLimit` when even fallback
cannot fit. Wall-clock duration may be measured diagnostically but is not the correctness
trigger when a deterministic work/memory limit is available.

---

### Final Optimizer Validation Tests

After selection and before handing a plan to execution, invoke optimizer final validation
and then the execution-layer Physical-Plan Validator tests above. Construct one malformed
winner for each §38.24 class:

```text
changed logical output/join semantics or illegal predicate placement
missing required output/hidden RID slot
unsatisfied or falsely advertised ordering
crossed LEFT JOIN boundary or unsupported physical capability
negative/nonfinite memory or spill annotation
statistics-derived or otherwise unapproved semantic-empty/no-op proof
estimated-zero base/join subtree with every executable path removed
```

Give each malformed plan the lowest numerical cost. Assert final validation fails,
execution is never invoked, no DML/storage/catalog/WAL effect occurs, and a structured
internal optimizer/validation error is surfaced. Cost cannot excuse an invalid plan.

For the proof boundary, test both forms explicitly:

```text
estimated_rows == 0 + statistics-derived is_provably_empty -> reject
estimated_rows == 0 + is_provably_empty == false + executable path -> accept
```

Positive final plans must preserve output slots, hidden RIDs, join semantics, runtime
capabilities, memory/spill declarations, and every required/provided ordering. Reuse the
§41.5 validator and pre-DML-effect procedures rather than duplicating their malformed-plan
matrix.

---

### Optimizer Differential Correctness Tests

For small random schemas/data:

1. generate supported logical queries,
2. execute optimizer-chosen physical plan,
3. execute a simple trusted reference physical plan, such as:
   ```text
   SeqScan + NestedLoopJoin + straightforward operators
   ```
4. compare results.

This tests optimizer transformation correctness independently of cost quality.

Include NULL-rich and skewed inputs, stale/missing/rejected statistics, numerical-zero
estimates without proof, different legal join orders/algorithms, interesting-order choices,
and memory-driven plan changes. Normalize only results whose SQL ordering is unspecified.
Compare both result values and controlled errors; optimization may change performance but
never expression demand or SQL semantics.

---

### Optimizer Fuzzing

Fuzz:

```text
logical expression trees
predicate combinations
statistics values within legal ranges
join graphs
ordering requirements
required slot sets
semantic-proof metadata
memory budgets
```

Requirements:

```text
no crashes
no NaN/negative costs escaping
no invalid physical plan
bounded planning work/resources above threshold
no semantic proof fabricated from estimates/statistics
```

Generate malformed statistics/proof/property inputs only through paths intended to reject
them, and require bounded structured failure. Record reproducible seeds. Fuzzing supplements
the explicit publication, estimated-zero, memo, and final-validation regressions; it does
not replace them.

---

### Plan Regression Suite

Maintain named optimizer scenarios containing:

```text
schema and stable object identities
statistics descriptor/version and confidence
query/logical input
cost and memory configuration
important semantic/property expectations
canonical plan fingerprint where diagnostically stable
```

Include selective point lookup, large range scan, star/bushy join, skewed hot key, required
Cartesian product, LEFT JOIN boundary, ORDER BY index match, small LIMIT, low-memory spill,
missing/stale statistics, and estimated-zero-without-proof cases. Fix configuration and
assert exact shape only when deliberate; otherwise assert legality, properties, capability,
and semantic result so tests do not overfit incidental cost changes.

### Optimizer Diagnostics Tests

Inspect structured EXPLAIN/trace fields rather than unstable prose. Verify visibility of:

```text
selected StatsVersion and staleness/confidence/provenance
table/column/index pressure inputs and fallback assumptions
predicate TRUE/FALSE/UNKNOWN estimates
estimated rows and row width
is_provably_empty and approved proof provenance as a separate field
enumerated/chosen access bounds, residuals, join order, algorithms, and orientations
major cost components, memory targets, spill expectation, and required/provided properties
dominance pruning, interesting-order retention, and bounded-fallback reason
final plan structural key/fingerprint and planning resource counters
```

For an estimated zero without proof, diagnostics must show both facts distinctly. Compare
estimated rows with actual execution counters through the §41.5 EXPLAIN ANALYZE tests and
verify q-error/reporting does not mutate estimates, proofs, persistent statistics, or future
planning semantics.

---

### Cost Model Benchmarks

For each operator collect actual resource behavior across scales:

```text
SeqScan
IndexScan
HashJoin
NestedLoop
IndexNestedLoop
HashAggregate
Sort
TopN
spill paths
```

Compare:

```text
predicted relative ranking
actual runtime ranking
```

The goal is not perfect milliseconds.

The goal is that cheaper predicted plans usually correspond to faster actual plans.

---

### Optimizer Performance Benchmarks

Measure planning time for join counts:

```text
2
4
6
8
10
12
16
20
30
```

Track:

```text
subsets explored
partitions considered
physical plans costed
memo entries
peak planning memory
```

Verify exhaustive search transitions to bounded heuristic behavior.

---

### Star Schema Benchmark

Even though the engine is general purpose, use a synthetic star schema to stress:

```text
many joins
selective dimensions
large fact table
aggregation
```

This is an excellent optimizer-learning workload.

TPC-H-inspired queries may be added without claiming benchmark compliance.

---

### No Benchmark Gaming

Do not hardcode:

```text
query text fingerprints
known benchmark table names
special-case TPC query shapes
```

Optimizer improvements must arise from general statistics/rules/costing.

---

## Document Maintenance

Update this guide when verification obligations, stable procedures, invariant coverage, or
benchmark methodology changes. Do not update it merely because another run succeeds, a
test count changes, another compiler or machine is exercised, or an implementation
milestone completes. Run-specific evidence and historical results belong in `devlog/`, CI
artifacts, or task completion reports.
