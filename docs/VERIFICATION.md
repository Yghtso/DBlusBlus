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

Use:

#### Positive parser tests

Valid SQL -> expected AST shape.

#### Negative parser tests

Invalid SQL -> expected syntax error and source span.

#### Precedence tests

Verify:

```text
1 + 2 * 3
NOT a AND b
a OR b AND c
```

parse correctly.

#### Round-trip debug tests

A debug AST formatter may produce canonical SQL-like output for inspection.

It need not reproduce original whitespace/comments.

---

### Binder Tests

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
type promotion
invalid casts
NULL typing
aggregate placement
GROUP BY validation
ORDER BY alias
ORDER BY ordinal
LEFT JOIN nullability
DML target binding
unique/primary-key metadata
subquery scopes
```

Binder tests should not require physical execution.

Use an in-memory/mock catalog implementation where useful.

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

---

### Catalog Tests

Test:

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
- bounded failure behavior,
- useful parser error instead of undefined behavior.

SQL parser fuzzing is high-value because arbitrary text reaches it directly.

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

Test analyzer on controlled distributions:

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
BETWEEN
IN
IS NULL
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

---

### Interesting-Order Tests

Verify optimizer can retain a slightly more expensive ordered path and use it to avoid:

```text
Sort
```

or enable:

```text
MergeJoin / streaming aggregate
```

when total plan cost is lower.

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

---

### Optimizer Fuzzing

Fuzz:

```text
logical expression trees
predicate combinations
statistics values within legal ranges
join graphs
ordering requirements
```

Requirements:

```text
no crashes
no NaN/negative costs escaping
no invalid physical plan
bounded planning time above threshold
```

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

### Plan Regression Suite

Maintain a set of named optimizer scenarios with:

```text
schema
statistics
query
expected important plan properties
plan fingerprint where stable
```

Run on every optimizer change.

Examples:

```text
selective point lookup
large range scan
star join
skewed hot key
ORDER BY index match
small LIMIT
low-memory hash spill
```

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
