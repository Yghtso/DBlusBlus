# Execution Integration and Transactional Write Verification

## Pipeline Finalization and Resource Tests

Use V26-A–S for generic protocol, lifecycle, error-owner, and completion expectations.
The procedures below specialize V26-G/H/K's independent dependency and ownership models;
production readiness counters and cleanup callbacks are observations, not their oracle.

### Blocking-state publication

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

### Cancellation resource matrix

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

### Execution-failure cleanup

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

## Parallel Execution Tests

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

Aggregate numerical conformance uses the Aggregate Tests smoke index and the complete V29
procedures in the dedicated Chapter-29 family. COUNT, integer SUM/AVG, and MIN/MAX use exact
owner-defined results;
FLOAT64 SUM/AVG instead require exact logical-occurrence coverage, an admitted §29.3.4
binary64 reduction tree, exact AVG count and special flags, and canonical exposed zero/NaN.
No tolerance comparison or cross-worker bit-identity assumption is an oracle.

Parallel sort conformance uses the Sort Tests smoke index and V30-F/H/K. Require the exact
occurrence bag, every strict comparator relation, the declared OrderingProperty, demanded
errors, and cleanup. Worker partitioning or scheduling may alter only the permitted relative
order and LIMIT membership inside complete-key tie classes; compare those results through
the V30 `TO` family oracle rather than exact cross-worker row-sequence equality.

---

## Scan and Unary Operator Tests

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
Project emits columns in its declared ordered physical output schema and owns computed VARCHAR results
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

## Hash Join Tests

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

Use V28-B/C/J's independent tagged-bag, typed-equality, and LEFT-extension models as the
expected result for deterministic and seeded-random small inputs. Compare HashJoin and
NestedLoopJoin to each other only as a secondary substitutability check; neither production
algorithm is the other's sole oracle.

---

## Aggregate Tests

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

### Aggregate state and finalization smoke checks

For each aggregate overload in §29.3.2, separately test Initialize, vector Update, Combine,
Finalize, and Destroy. Assert:

- NULL/empty behavior and result type;
- exact worker-local state remains valid when an integer subtotal leaves the final INT64
  domain but later values cancel it;
- COUNT and integer SUM range errors occur at Finalize rather than order-dependent Update
  or Combine points;
- integer AVG merges exact sum/count state and does not average partial averages;
- FLOAT64 AVG merges one admitted binary64 subtotal, exact count, and explicit-input flags
  and does not average partial averages;
- MIN/MAX retain the architecture's canonical representative for equal FLOAT64 classes and
  own VARCHAR bytes;
- every group's numerical state is validated before any successful aggregate-row prefix is
  exposed, with the lowest semantic aggregate ordinal supplying competing numeric errors.

Inject OOM and spill failures while exact state is growing. Assert controlled resource
failure and complete cleanup; an approximate or narrowed accumulator is never substituted.

### FLOAT64 legal-tree smoke checks

Exercise §29.3.8's finite cancellation, halfway rounding, subnormal, signed-zero,
infinity, mixed-infinity, and NaN cases for SUM and AVG. Compare exact bits for each
explicit tree and every tree-invariant result. For an unconstrained execution shape, use
V29-H/I's independent legal-tree set or observed reduction-trace oracle; do not substitute
an exact-dyadic sum or tolerance comparison.

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

COUNT, integer SUM/AVG, MIN/MAX, classes, canonical representatives, schemas, exact counts,
and explicit-input special classification must remain identical. FLOAT64 SUM/AVG roots
may differ only when every observed result belongs to an admitted §29.3.4 tree. Partial-
state spill preserves exact binary64 subtotal bits, exact count, flags, and descriptor
state; raw-value replay preserves every occurrence. V29-A–V is the normative Chapter-29
methodology; this section remains a compact smoke/index entry point.

---

## Sort Tests

This is the compact smoke/index entry point. V30-A through V30-O own the complete
deterministic Chapter-30 procedures and independent oracles. Cover:

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

Compare output ordering against the semantic comparator, not raw bytes. Equal-key order is
not required to be stable. Sort + Limit, Top-N, and an exact ordered provider + Limit are
compared by membership in the same Chapter-20 permitted result family, not by requiring
identical boundary-tied occurrences. Prefix, external-run, exact-`K`, blocking, failure,
memory, cancellation, and property details delegate to V30-A through V30-O.

---

## DML Execution Tests

Specifically test Halloween protection.

Example:

```sql
CREATE INDEX idx_x ON t(x);
UPDATE t SET x = x + 1 WHERE x < 100;
```

Run through an index access path and verify each qualifying logical target is updated once.

This section is the procedural owner for the Chapter-15 composition contract. It uses the
component procedures above for tuple bytes, page guards, B+ MTRs, transaction state, MVCC,
logical locks, WAL publication, recovery, and reclamation; it does not restate those
subsystem rules. Every DML fixture identifies one statement, one or more execution attempts,
the attempt's snapshot and CommandId, and the exact first transaction-owned publication
event, then compares the observed outcome with independent test-side models.

### Deterministic DML harness and independent oracles

The harness records and can pause these implementation-independent semantic events:

```text
statement admitted / CommandId assigned / attempt begins / snapshot registered
candidate produced / target-spool append / target finalized / predicate rechecked
TableWriterGate acquired
read epoch entered / TUPLE_WRITE request made live / epoch released
tuple-lock waiter blocks / grant occurs / post-grant revalidation completes
UNIQUE_KEY acquired / exact-key scan begins / candidate heap-rechecked / scan completes
RID selected / tuple encoded / heap write guard acquired
WAL record or MTR privately encoded / publication-authorizing append completes
heap bytes and page_lsn publish / heap guard latch releases / heap pin releases
index MTR begins / index entry and page_lsn publish / index guards release
clean retry requested / attempt abandoned / replacement snapshot registered
W reached / persistent statement mutation published
ordinary execution and result preparation complete / provisional count finalized
R publishes the successful result envelope and transfers result ownership
autocommit C0..C5 complete before R / C6 response delivery
MUST_ABORT publishes / automatic ABORT reaches A2 and A3
implicit COMMIT reaches C0, C1, C2, C3, C4, C5, R, and C6 response delivery
```

Fault points exist immediately before and after every publication-authorizing append and
page/MTR publication, after each successful row, before and after every wait, during result
spool finalization, and at each relevant C0–C6/A0–A4 boundary. Crash fixtures persist an
explicit selected WAL prefix and selected old or new page images, omit all later operations,
then reopen. Barriers and persisted-prefix construction, never sleeps, establish order.

The independent oracles are:

- a **statement ledger** that assigns one CommandId per admitted SQL statement, nests all
  internal attempts beneath it, records the snapshot registration for each attempt, and
  independently computes reuse, consumption, retry eligibility, and terminal state from
  §§9.6, 9.9–9.10, 15.7, and 39.1;
- the Chapter-10 **visibility oracle**, evaluated from fixture bytes, snapshot fields, and an
  independent transaction-status map rather than through the production visibility path;
- a **current-owner oracle** that maps the exact `xmax` sentinel/SELF/terminal/nonterminal/
  invalid result and isolation level to wait, proceed, clean retry, serialization failure,
  or corruption without consulting the DML implementation;
- a **uniqueness oracle** that sorts canonical `(IndexId, encoded_key)` claims, enumerates
  the complete physical equal-key run, heap-rechecks every RID, and applies §11.10's
  current-state creator/deleter and exact `excluded_old_rid` rules;
- a **header-byte oracle** that constructs the 48-byte little-endian tuple header from
  field values and validates relational invariants independently of `TupleCodec`;
- a **publication-prefix oracle** that starts from exact pre-operation page/frame/WAL state,
  applies only validly appended records in order, and predicts published bytes, `page_lsn`,
  `rec_lsn`, dirty/FPI state, and recovery outcome;
- a **RID-retention oracle** that independently evaluates read-epoch registration/grace,
  live queued/granted TUPLE_WRITE claims, index and predecessor cleanup, and slot state as
  the Chapter-14 same-RID reuse conjunction;
- a **transaction-state oracle** that combines the first-published-write flag, the
  independently classified causal error, and terminal-publication stage to derive FA, MA,
  NC, COMMIT eligibility, and required ownership lifetime from §39.1;
- an **affected-row oracle** that uses only the final successful attempt's distinct finalized
  logical inputs/targets and operation kind, never physical mutation or diagnostic counts;
- a **result-envelope oracle** that separates statement success, transaction outcome, and
  client observation for explicit transactions, autocommit, and transport failure.

Negative fixtures alter one condition at a time while preserving every unrelated invariant.
Instrumentation reports semantic events and ownership sets; container shape, worker count,
batching, tuple-builder API, and lock implementation are not oracles.

### Statement attempts, CommandId, snapshots, and retry

Create a statement object with CommandId `C` and place a barrier at each attempt start. For
READ COMMITTED, force a current-owner conflict before the first transaction-owned
publication. Assert that the first attempt unregisters its snapshot, discards its target and
result spools and provisional count, and that the replacement attempt registers a fresh
snapshot while retaining `C`. The fixture changes committed data between registrations so
snapshot reuse and snapshot refresh produce distinguishable results. Retained transaction
locks are allowed, but the replacement attempt repeats target discovery, revalidation, and
every current-state uniqueness scan; no retained lock carries a prior semantic decision or
row-count contribution.

Repeat with the conflict after the first publication event. Assert no replacement attempt
begins, `ACTIVE -> MUST_ABORT`, automatic A0–A4 cleanup occurs, and the client receives the
canonical retryable conflict only after ABORT. A later request, if submitted by the fixture,
has a new TxnId. For REPEATABLE READ, force the same committed post-snapshot conflict and
assert the retained transaction snapshot is never replaced and the serialization path is
transaction-fatal.

The CommandId fixture starts at `0`, performs successful, clean-retried, recoverably failed,
and MUST_ABORT-producing statements, and records allocation/consumption at statement and
attempt boundaries. It proves that internal retry does not consume another ID, whereas every
admitted success or failure consumes the statement's ID. A multirow statement uses one ID.
Reuse the Numeric Exhaustion and Terminal-Boundary Verification procedure for
`UINT32_MAX`: it is the final legal ID and no statement is admitted after exhaustion or
wraparound.

### DML lock order, RID handoff, and post-wait revalidation

Compose the logical-lock harness with the Chapter-14 reclamation harness. For UPDATE and
DELETE, pause after candidate discovery while the read epoch is active, then exercise both
legal registration/reuse orders:

```text
TUPLE_WRITE request becomes live first -> old RID reuse remains blocked
same-RID rebinding becomes authoritative first -> old-identity request cannot be admitted
```

In the first order, release the epoch only after the request is live and before any blocking
wait. Queue the request, mutate or retire the candidate while it waits, grant it, and require
a fresh pin/latch, exact `(TableId,RID)` identity check, tuple-header validation, visibility/
current-owner decision, and required predicate recheck. The stale pre-wait bytes cannot
authorize a mutation. Immediate-grant and no-wait paths perform the same revalidation.

Event-log assertions establish:

```text
shared TableWriterGate before TUPLE_WRITE/UNIQUE_KEY and before persistent write
TUPLE_WRITE before sorted affected UNIQUE_KEY claims for UPDATE
old-key UNIQUE_KEY before DELETE xmax publication
no transaction wait while a page/B+ latch or read epoch is held
fresh page pin/latch after lock grant; logical lock is not physical protection
TUPLE_WRITE and UNIQUE_KEY survive statement completion and C4/A2
release only at C5/A3 after terminal publication
```

The RID-retention request remains a live RID-retention claim while queued, granted, canceled in a
deadlock, or owned by a transaction being committed/aborted, until its architecture-defined
removal. The complete reuse conjunction remains owned by Vacuum and Reclamation Tests.

### Current-owner and current-state uniqueness composition

Drive each current-owner status through the lock/re-fetch path rather than calling the
status helper alone. The fixture distinguishes the no-deleter sentinel from a persisted
status value, constructs SELF command boundaries, and forces IN_PROGRESS terminal outcomes
on both sides of the wait. RC COMMITTED is run before and after the first publication;
RR COMMITTED uses the retained snapshot. `RETIRED`, recognized `INVALID`/`RESERVED`, status
I/O/checksum/format failure, future SELF command metadata, `DEAD`, `UNUSED`, and rebound RID
all prohibit a guessed write.

For each fully non-NULL unique key, assert lock acquisition in canonical sorted
`(IndexId, encoded_key)` order before any conflicting publication. The index fixture places
one equal-key run across at least three leaves with stale but structurally valid candidates
before, between, and after live owners. Require complete enumeration and a heap recheck of
every candidate before `NO_CONFLICT` or `UniqueViolation`; first-hit and caller-snapshot
answers deliberately differ from the independent oracle.

Cover committed/frozen, active-then-commit, active-then-abort, and aborted creators; absent,
committed, active-then-commit, active-then-abort, and aborted deleters; exact UPDATE
`SELF_EXCLUDED`; same-transaction other row; same-command other-row delete; and
earlier-command self delete. A NULL-containing ordinary UNIQUE key creates no ordinary
UNIQUE_KEY request. PRIMARY KEY NULL rejects before uniqueness admission. A malformed,
dangling, wrong-key, wrong-table, or reused candidate is corruption rather than conflict or
free space. A second intentional physical `(key,RID)` insertion is rejected except through
the idempotent recovery fixture.

### Byte-exact DML tuple headers

Encode the complete 48-byte header with the independent header oracle, compare every byte
with the published page and WAL/page-image payload, then decode it through the ordinary
reader. Required cases are:

```text
INSERT command 0: xmin=T, xmax=0, cmin=0, cmax=0, prev=invalid
INSERT command 7: xmin=T, xmax=0, cmin=7, cmax=0, prev=invalid
UPDATE replacement command 9: xmin=T, xmax=0, cmin=9, cmax=0, prev=old RID
actual DELETE/self-deleter command 9: xmax=T, cmax=9
```

Starting from otherwise valid recognized-v1 bytes, set invalid `xmax` with `cmax=1`,
`UINT32_MAX`, and a stale prior deleting command. Each must be `CORRUPT_HEAP`, not an
alternate encoding or unsupported format, and ordinary read must not rewrite it. For an
eligible aborted deleter, run the Chapter-14 normalization procedure and assert the exact
post-WAL pair `(INVALID_TXN_ID,0)`. These fixtures leave tuple width, offsets, creator
`cmin`, actual-deleter `cmax`, flags, schema version, body, WAL grammar, and checksum
coverage unchanged.

### INSERT publication and failure prefixes

Use one single-row and one multirow input. Reject NOT NULL, PRIMARY KEY NULL, invalid type,
and encoded size `8136` before publication; accept size `8135` when all other constraints
hold. Run unique admission before selecting/publishing a conflicting tuple. Force stale-low
and stale-high FSM hints: both lead to authoritative heap geometry checks, never overwrite;
the search may select another page or create one through the existing PAGE_INIT procedure.

At heap publication, record exact tuple/page bytes, `page_lsn`, statement flag, and RID.
Place a barrier after the heap guard latch and pin are released but before the first B+ MTR;
assert no heap physical guard is held and transaction gate/key ownership remains. For every
index, publish exactly one `(encoded_user_key,RID)` through its own MTR/guards. Heap redo for
the RID must precede every referring B+ MTR.

Inject before heap publication, immediately after heap publication, and after each index
MTR. The first reachable recoverable failure is FA and leaves no transaction-owned row
effect. Every later statement failure is MA and automatic ABORT; no same-TxnId retry or
successful partial count/RETURNING prefix is legal. Reopen each valid WAL prefix and use
MVCC plus heap-rechecked index scans to prove that an aborted creator is invisible even
when physical heap/index bytes remain. A successful statement in an explicit transaction
remains distinct from COMMIT.

### UPDATE version publication and failure prefixes

Finalize the target spool before any target mutation, then execute one target through the
read-epoch/TUPLE_WRITE/current-owner/UNIQUE protocol above. Compare the replacement header
and predecessor pointer byte-for-byte; its RID must be fresh and `prev` must be the exact
old RID. Follow the live §15.3 publication order: publish the new version first, then the old
version's `xmax=current TxnId,cmax=current CommandId`, then one new physical entry for every
index while retaining every old entry. An unchanged indexed key therefore has old and new
RIDs under the same key; a changed key has retained old-key and new-key candidates. Both are
resolved through heap recheck.

Inject before the new-version publication, after it, after the old-header publication, and
after each index MTR. The direct “old xmax only, no replacement publication” runtime prefix
is N/A: §15.3 publishes the replacement first. The after-old-header fixture necessarily
contains both tuple effects. Every post-publication non-success is MA and uses semantic
ABORT, not physical repair or same-TxnId retry. Reopen each prefix and prove independently:

```text
ABORTED updater: old xmax ineffective; new creator invisible; old row remains visible
COMMITTED updater: old deleter effective; new creator visible to qualifying snapshots
old RR snapshot: Chapter-10 snapshot rules select the legal historical version
same transaction later command: SELF cmin/cmax causality selects the legal current version
```

The uniqueness fixture separately covers old key equal to new key with only the exact old
RID excluded, changed keys with all affected claims sorted, same-command collisions, and
immediate key-swap rejection. Failure in one of multiple index MTRs leaves no successful
statement result; recovery repeats physical WAL only and heap recheck keeps retained or
aborted candidates from becoming semantic aliases.

### DELETE publication and failure prefixes

Finalize and revalidate each target through the same epoch/TUPLE_WRITE protocol. Acquire
every required old-key UNIQUE_KEY claim, repeat current-state revalidation, then publish
only `xmax=current TxnId,cmax=current CommandId`. Assert that ordinary DELETE performs no
physical index erase; every old index entry remains until Chapter-14 cleanup.

Inject immediately before and after the header publication and after several targets. A
pre-publication recoverable failure is FA; a post-publication statement failure is MA and
automatic ABORT. On abort, the deleter is ineffective and old index candidates still lead
to the live row. On commit, new qualifying snapshots treat the version as deleted while
the retained candidate is heap-rechecked as non-owning. A deterministic DELETE/INSERT race
holds the old-key claim through terminal publication, then proves commit frees the key and
abort preserves its owner.

### First-publication failures, semantic abort, and recovery

For each INSERT, UPDATE, and DELETE fault prefix, snapshot exact pages, frame metadata, WAL
end, logical ownership, transaction state, provisional count, and output spool. The
Statement Failure and Transaction-State Tests remain the owner of the complete §39.1.3
table. This section specializes it by proving that the flag changes only at §12.12
transaction-owned publication, not at LSN reservation, private encoding, provisional page
mutation, WAL durability, page durability, or COMMIT.

Run representative user/constraint, resource, cancellation, deadlock, RR serialization,
known WAL/BufferPool failure, append uncertainty, and corruption cases. Assert FA, MA, or
NC exactly as §39.1 requires, preserving the original structured diagnostic. FA leaves the
explicit transaction ACTIVE and consumes the statement CommandId; clean RC retry is an
internal attempt outcome, not FA. MA retains locks/dependencies through automatic ABORT and
publishes no success result. NC closes ordinary database admission and is never downgraded.

No fixture expects user-DML physical undo. After crash/reopen, the Recovery Property Tests
replay only physical WAL, classify losers ABORTED, and compare logical committed contents.
They do not reconstruct SQL attempts, locks, snapshots, affected-row counts, or RETURNING
results. Aborted INSERT, UPDATE, and DELETE bytes remain legal garbage until their existing
vacuum owners remove or normalize them.

### Affected-row and result-envelope verification

The affected-row oracle consumes the final successful attempt only:

```text
INSERT       number of successfully inserted logical input rows
UPDATE       number of distinct finalized/revalidated targets logically updated
DELETE       number of distinct finalized/revalidated targets logically deleted
failed DML   no successful affected-row result
```

Run exact cardinality fixtures for single INSERT `1`, five-row INSERT `5`, UPDATE eight
distinct targets `8`, `SET x=x` over six finalized targets `6`, zero-target UPDATE `0`,
DELETE four targets `4`, and zero-target DELETE `0`. Vary replacement-version count, index
count, MTR count, page splits, batching, and optional legal no-op physical work while holding
logical inputs fixed; the result must not change. Duplicate candidate production finalized
once contributes one. A stale/nonqualifying target contributes zero unless a replacement
attempt later acts on it.

For RC retry, force provisional `5 -> 3` and `4 -> 0` attempts and assert final results `3`
and `0`, never `5`, `8`, or `4`. Force provisional `2` followed by a permitted dynamic
failure after `W`, such as cancellation, OOM, or an owner-valid storage failure; publish an
error and no successful partial count. For an ordinary row-semantic error in a later logical
occurrence, place the failure during pre-W closure and assert that no earlier row publishes.
Locks may remain transaction-owned across a clean retry, but the counter, candidate set, and
output spool reset.

In an explicit transaction, pause after `R` and assert the complete count and RETURNING
spool may publish while state remains ACTIVE; later ROLLBACK does not rewrite the already
returned statement result and the result never claimed durability. In autocommit, pause at
C0, C1, C2, C3, C4, C5, `R`, and C6. No count, RETURNING row, or successful result escapes
before `R`; no required preparation remains after C5. A pre-C0 or COMMIT failure withholds
the result; after successful C4–C5, `R` uses prepared state and C6 performs delivery. A
transport failure during delivery cannot change the COMMITTED outcome. Assert no row-count
WAL, checkpoint/control/status/catalog state, or recovery reconstruction exists.

### Cross-owner concurrency, resources, and lifecycle

Compose DML with vacuum by pausing an UPDATE/DELETE at candidate discovery, request
registration, wait, grant, and post-grant revalidation. The Chapter-14 epoch/claim oracle
must prevent reuse until handoff and claim lifetime permit it; DML never treats an index
candidate, pin, latch, snapshot, or status dependency as a substitute. Keep a write-capable
transaction idle between statements and verify its write-status dependency prevents cutoff
from retiring a TxnId it may still publish; DML publication after C5/A3 is impossible.

Run checkpoint while each heap/index mutation is provisional, fully published, dirty, and
flushed. Reuse the WAL/DPT oracle to prove old-or-new publication only, WAL-before-data,
correct `rec_lsn`, and no statement-level checkpoint/page-force requirement. Recovery runs
with ordinary DML admission disabled and performs no SQL, lock, uniqueness, or result replay.

Hold the shared TableWriterGate while changing the catalog/schema/index set from a competing
DDL owner. DDL cannot publish an incompatible set mid-operation. Invalid schema version,
stale descriptor, and catalog lookup failure stop before unsafe encoding/publication or
follow the current statement's §39 boundary; no cached name substitutes for stable identity.

Specialize resource/error procedures for `NO_REPLACEABLE_FRAME`, WAL-position exhaustion,
known WAL ENOSPC/I/O, heap corruption, index corruption, status lookup failure, invalid RID,
and `DEAD`/`UNUSED`/rebound targets. Each fixture records whether the failure is before or
after publication and applies the exact §39 result; corruption and uncertain ownership are
never converted to row-not-found, free key, or retry. READY admits the statement; DRAINING,
RECOVERING, CLOSING, and NONCONTINUABLE reject new DML. Shutdown drains in-flight owners
through their canonical terminal path without inventing DML-specific lifecycle state.

### Target spool and one-target-once behavior

Run UPDATE and DELETE with an in-memory target spool and with a tiny memory budget that
forces spill. Create join-expanded inputs that produce one target RID repeatedly and assert
that the finalized spool contains each physical target at most once. Change indexed keys and
predicate values so a streaming mutation would rediscover rows; assert target discovery
finishes before any write and every qualifying target changes once.

At target-spool append, spill-write/read, deduplication, and Finalize boundaries, inject
cancellation, OOM, SpillIO, and expression errors. Assert no target mutation starts before
successful Finalize and every temporary row, reservation, block, and file is cleaned.

### Revalidation, UNIQUE integration, and failure state

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

### RETURNING exposure

Produce several RETURNING rows, then fail a later row/write/spool operation. No successful
prefix may escape that failed statement attempt. For an explicit transaction, expose the
complete result only after the statement's safe execution boundary. For autocommit, complete
all required result preparation before C0, retain the prepared request-owned state through
implicit COMMIT C4–C5, perform `R`, and expose rows only through the resulting delivery path.

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

### Chapter-15 procedural matrices

The tables below are compact indexes into the deterministic procedures above and the
component procedures with independent oracles. “Published” always means §12.12 publication of a
transaction-owned logical mutation, not reservation, durability, page flush, or COMMIT.

#### Statement-attempt matrix

| Case | Snapshot | CommandId | Published? / retry | Transaction and locks | Provisional count / external result |
|---|---|---|---|---|---|
| First attempt | RC statement snapshot or retained RR snapshot | Assigned once | No initially; retry depends on later boundary | ACTIVE; acquired terminal locks remain owned | Starts zero; unexposed |
| RC clean pre-publication retry | Fresh registration | Same ID | No; legal internal retry | ACTIVE; transaction locks may remain | Old count discarded; new zero; no old output |
| Successful attempt | Its effective snapshot | Same statement ID | Any legal completed writes; no retry remains | Explicit transaction remains ACTIVE | Final count/result envelope becomes authoritative |
| Recoverable failed statement | Registered attempt snapshot released | Consumed | No published write; no internal success | FA, ACTIVE; transaction locks retain normal lifetime | Discarded; error only |
| MA/MUST_ABORT attempt | No replacement snapshot | Consumed | Published or independently fatal; no retry | MUST_ABORT -> A0–A4; locks through A2/A3 | Discarded; canonical error only |
| NC/uncertain attempt | No continuation snapshot | Consumed if admitted | No retry | Database noncontinuable; ownership retained for stop/recovery | No success result; causal/fatal error chain |

#### CommandId matrix

| Case | Current C | Reused? | Consumed? | Next C / state / result |
|---|---:|---:|---:|---|
| First statement | `0` | No | On completion/failure | `1` if ACTIVE; ordinary result |
| Success | `C` | Across its attempts only | Yes | `C+1` if legal; success |
| Clean internal retry | `C` | Yes | Not by abandoned attempt | Still `C`, ACTIVE, no external result |
| Recoverable failed statement | `C` | No later reuse | Yes | `C+1`, ACTIVE, error |
| MUST_ABORT-producing statement | `C` | No | Yes | No ordinary next statement; automatic ABORT |
| Final legal ID | `UINT32_MAX` | Only internal attempts | Yes | No representable successor |
| After maximum | None | No | No | Statement admission fails; no wrap or execution |

#### Lock and protection matrix

| Protection | Protects | Lifetime / blocking | Held across wait? | DML stage | Crash | RID reuse / status cutoff |
|---|---|---|---|---|---|---|
| TableWriterGate shared | Stable target schema/index set | Terminal transaction; may block | Yes as logical ownership, but acquired without forbidden physical guards | Before tuple/key locks and first write | No replay | No / no |
| TUPLE_WRITE | Exact `(TableId,RID)` writer and retention claim | C5/A3; may block | Request survives its own wait | UPDATE/DELETE handoff | No replay | Yes while live / no |
| UNIQUE_KEY | Current-state admission for one canonical key | C5/A3; may block | Yes | Before conflicting DML publication | No replay | No / no |
| Page latch | Stable in-frame bytes | Short; never a transaction wait | No | Re-fetch, encode, publish | Recreated | No / no |
| BufferPool pin | Frame lifetime | Short or operation-scoped; not a logical wait authority | May outlive latch, but not substitute for lock/epoch | Physical access/writeback | Recreated | No / no |
| Read epoch | Prevents reuse while a RID is retained before claim handoff | Read-side critical section; registration does not block | Released before transaction wait | Discovery through live request registration | Recreated | Yes through grace / no |
| Write-status dependency | Own future publication of transaction TxnId | Registration through C5/A3; nonblocking dependency | Yes across transaction waits | All write-capable DML | Recreated by new transactions only | No / yes |

#### Current-owner matrix

| Current owner | RC action | RR action | Wait / retry | Write? | State consequence / oracle |
|---|---|---|---|---:|---|
| No `xmax` sentinel | Revalidate and proceed | Revalidate and proceed | No / no | Yes | Current-owner oracle |
| SELF | Apply exact command causality | Same | No / no | Only if exact SELF rule permits | Chapter-10 visibility/current-owner oracle |
| IN_PROGRESS other | Wait, grant, re-fetch | Same | Yes / recheck | Not before recheck | Terminal-status barrier |
| COMMITTED other | Clean fresh-snapshot retry before publication; MA after | Serialization failure | No further owner wait / isolation result | No stale write | Statement ledger + visibility oracle |
| ABORTED other | Re-fetch and proceed if still valid | Same | No / no | Yes after revalidation | Status map |
| RETIRED required outcome | Corruption/invariant result | Same | No / no | No | Status precedence oracle |
| Persisted INVALID status | Error | Same | No / no | No | Status precedence oracle |
| RESERVED | Error | Same | No / no | No | Status precedence oracle |
| Lookup I/O/checksum/format failure | Propagate exact failure | Same | No guessed retry | No | Independent injected lower-layer result |

#### Uniqueness matrix

| Candidate/key state | UNIQUE_KEY? | Heap recheck / wait | Conflict? | Exact result and oracle |
|---|---:|---|---:|---|
| NULL-containing ordinary UNIQUE | No | No duplicate admission | No | NULL-domain rule |
| PRIMARY KEY NULL | No ordinary admission | Reject before scan | N/A | Constraint result |
| Committed creator, live | Yes | Every candidate | Yes | `UniqueViolation` |
| Frozen creator, live | Yes | Every candidate | Yes | `UniqueViolation` |
| In-progress creator | Yes | Recheck after terminal wait | Outcome-dependent | Current-state oracle |
| Aborted creator | Yes | Status-proven | No | Continue complete scan |
| Committed effective deleter | Yes | Status-proven | No owner | Continue complete scan |
| Aborted deleter | Yes | Status-proven ineffective | Yes if creator owns | Current-state oracle |
| In-progress deleter | Yes | Wait and complete rescan | Outcome-dependent | Current-state oracle |
| SELF exact old UPDATE target | Yes | Exact RID only | Excluded | `SELF_EXCLUDED` |
| SELF other row | Yes | Full recheck | Yes if owner | `UniqueViolation` |
| Same-command other-row delete | Yes | Full recheck | Yes | Immediate uniqueness rule |
| Earlier-command self delete | Yes | Full recheck | No if deletion effective | Command-aware oracle |
| Dangling/malformed/rebound candidate | Yes | Identity proof fails | Not a duplicate answer | Corruption |
| Exact physical duplicate | N/A SQL admission | Physical-key model | Forbidden | Reject except authorized idempotent redo |

#### INSERT protocol matrix

| Stage | May wait / physical guard | Published yet? | Injected failure and state | Affected result / procedure |
|---|---|---:|---|---|
| Input/type/NOT NULL/size/PK-NULL validation | No / none | No | FA if statement error | No success count; input fixtures |
| Shared writer gate | May wait / none | No | §39 pre-boundary result | Zero provisional authority; lock harness |
| Sorted UNIQUE_KEY claims | May wait / no page latch | No | Deadlock MA or ordinary pre-boundary error | No success; lock matrix |
| Complete current-state scan | Status wait only outside physical latch | No | UniqueViolation FA before first row publication | No success; uniqueness oracle |
| RID/page selection | No transaction wait / heap guard only for check | No logical row yet | FSM/resource result | No success; FSM/BufferPool procedures |
| Heap WAL/publication | No logical wait / heap write guard | Becomes yes at publication | Before=FA; after=MA | Attempt-local only; prefix oracle |
| Heap guard release | No / latch then pin released | Yes | Ownership invariant failure if retained into B+ wait | Logical locks remain; event order |
| Each index MTR | No transaction wait / MTR-owned B+ guards | Yes | Failure=MA or NC by lower layer | No partial count; MTR prefix fixture |
| Statement success | No / no leaked physical guard | Complete | N/A | One per logical input; result-envelope oracle |

#### UPDATE protocol matrix

| Stage | Wait / physical protection | Published? | Finalized / RID retained | Failure outcome | Count / procedure |
|---|---|---:|---|---|---|
| Candidate discovery | No transaction wait / epoch plus short guards | No | Not finalized / epoch | Discard/FA as applicable | Zero; target-spool procedure |
| Epoch-to-request handoff | No wait until request live | No | Finalized spool RID / epoch then claim | Registration failure per §39 | Zero; RID-retention composition |
| TUPLE_WRITE wait | Yes / no epoch or page latch | No | Claim retains RID | Deadlock MA; cancellation FA/MA | Zero; lock harness |
| Post-grant revalidation | No / fresh pin+latch | No | Valid only after checks | RC retry, RR MA, or error | Stale target zero |
| Sorted UNIQUE_KEY claims | Yes / no physical guard | No | Revalidated target / claim | Deadlock or §39 result | Still provisional |
| Complete exact-key scan | Status waits outside guards | No | Target revalidated again | Conflict/error by oracle | Still provisional |
| Replacement version publication | No logical wait / heap guard | First write for target | Yes / TUPLE_WRITE | Later failure MA | One logical target provisionally |
| Old `xmax/cmax` publication | No logical wait / heap guard | Yes; replacement already published | Yes / claim | Later failure MA | Still one, not two |
| Every new index MTR | No logical wait / MTR guards | Yes | Yes / claim | Partial-index failure MA/NC | Still one |
| Result success | No leaked guards | Complete | Distinct finalized target | N/A | Final attempt count; envelope oracle |

#### DELETE protocol matrix

| Stage | Wait / physical protection | Published? | Failure state | Index policy / count |
|---|---|---:|---|---|
| Candidate discovery | No transaction wait / epoch+short guard | No | Attempt-local/FA | No count |
| TUPLE_WRITE handoff | Request live before epoch release | No | Registration result by §39 | No count |
| Wait and grant | May block / no epoch/latch | No | Deadlock MA; cancellation FA/MA | No count |
| Post-grant revalidation | No / fresh pin+latch | No | Skip, RC retry, RR MA, or error | Stale target zero |
| Old-key UNIQUE_KEY | May block / no physical guard | No | §39/deadlock outcome | Retained terminally |
| Current-state recheck | Status wait outside guard | No | Exact owner/error result | No count yet |
| `xmax/cmax` publication | No logical wait / heap guard | Yes | Later failure MA | One finalized delete provisionally |
| Index-policy step | No erase operation | Yes | N/A | Entries retained for vacuum; no count multiplication |
| Statement success | No leaked guard | Complete | N/A | Distinct deletes once; envelope oracle |

#### FA / MA / NC matrix

| Case | Published flag | Retry | ACTIVE / MA / NC | Physical undo | Count / client result |
|---|---:|---|---|---:|---|
| User/constraint failure before publication | False | No internal success; later statement allowed | FA / ACTIVE | No need | No success count; error |
| Resource failure before publication | False | Only existing policy | Usually FA, exact §39 row | No | No success count; error |
| Clean RC conflict before publication | False | Yes, fresh snapshot/same ID | ACTIVE | No | Old count discarded; no output |
| Failure immediately after first publication | True | No | MA -> automatic ABORT | No | Error; no partial count |
| Failure after heap plus one index | True | No | MA or NC if lower layer uncertain | No | Error; no partial count |
| WAL append/publication uncertainty | Either | No | NC | No guessed repair | Fatal/uncertain result |
| Persistent heap/index corruption | Either | No | NC | No | Corruption result |
| Deadlock victim | Either | No | MA | No | Deadlock error only |
| RR serialization | Either | No | MA | No | Serialization error only |
| Cancellation before publication | False | No | FA unless stronger cause | No | Cancellation, no count |
| Cancellation after publication | True | No | MA | No | Cancellation, no count |

#### Tuple mutation header matrix

| Case | `xmin` | `xmax` | `cmin` | `cmax` | `prev` | Valid/classification | Byte fixture |
|---|---:|---:|---:|---:|---|---|---|
| INSERT command 0 | T | 0 | 0 | 0 | invalid pair | Valid | Exact 48-byte oracle |
| INSERT command 7 | T | 0 | 7 | 0 | invalid pair | Valid | Exact 48-byte oracle |
| UPDATE replacement command 9 | T | 0 | 9 | 0 | old RID | Valid | Exact 48-byte oracle |
| DELETE/self-deleter command 9 | creator | T | creator C | 9 | unchanged | Valid | Actual-deleter control |
| Invalid xmax + cmax 1 | otherwise valid | 0 | C | 1 | valid form | `CORRUPT_HEAP` | Single-field mutation |
| Invalid xmax + cmax max | otherwise valid | 0 | C | `UINT32_MAX` | valid form | `CORRUPT_HEAP` | Single-field mutation |
| Invalid xmax + stale cmax | otherwise valid | 0 | C | prior delete C | valid form | `CORRUPT_HEAP` | Single-field mutation |
| Aborted-xmax normalization | creator | D -> 0 | creator C | delete C -> 0 | unchanged | Valid after WAL publication | Pre/post exact bytes |

#### Affected-row result matrix

| Case | Final successful attempt? | Logical rows / earlier provisional | Published count or error | Commit gate / oracle |
|---|---:|---|---|---|
| Single INSERT | Yes | 1 / 0 | 1 | Explicit statement envelope |
| N-row INSERT | Yes | N / 0 | N | Logical-input oracle |
| UPDATE N distinct finalized targets | Yes | N / 0 | N | Distinct-target oracle |
| No-op UPDATE | Yes | N qualifying / 0 | N | Independent of physical elision |
| Zero-target UPDATE | Yes | 0 / 0 | 0 | Successful statement |
| DELETE N | Yes | N / 0 | N | Distinct-target oracle |
| Zero-target DELETE | Yes | 0 / 0 | 0 | Successful statement |
| Duplicate candidates, finalized once | Yes | 1 / duplicates | 1 | Finalized spool identity |
| Stale target not acted upon | Yes | 0 / candidate seen | 0 contribution | Revalidation oracle |
| RC retry 5 -> 3 | Second only | 3 / 5 | 3 | First count discarded |
| RC retry 4 -> 0 | Second only | 0 / 4 | 0 | First count discarded |
| Provisional 2 -> fatal | No | none / 2 | Error, no success count | Failure envelope |
| Seven rows -> later dynamic failure after W | No | none / 7 | Error, no partial 7 | Automatic ABORT |
| Explicit success, later rollback | Yes | N / 0 | N remains prior response | No COMMIT claim |
| Autocommit count 7, COMMIT fails | Statement only, request fails | 7 / 0 | Commit error, no count | Withheld through C4–C5 |
| Autocommit success | Yes + COMMIT success | N / 0 | N | Shared RETURNING envelope |
| Different index/MTR counts | Yes | 1 / 0 | 1 in every shape | Physical-work independence |

#### UPDATE outcome matrix

| State/prefix | Old visible? | New visible? | Old xmax / new creator | Index-candidate action | Cleanup owner |
|---|---|---|---|---|---|
| Before UPDATE | Per creator/snapshot | Absent | No updater effects | Old entries heap-rechecked | None |
| Update IN_PROGRESS | Other readers see legal old/new MVCC outcome | Creator not dirty-visible | Nonterminal | Writers/uniqueness may wait | Transaction |
| “Old xmax only” runtime prefix | N/A | N/A | Unreachable: §15.3 publishes replacement first | N/A | N/A |
| Abort after replacement only | Old remains as before | New creator ABORTED/invisible | No effective old delete | New entries if any ignored by heap | Vacuum |
| Abort after replacement + old header | Old visible; xmax ineffective | New invisible | Both updater statuses ABORTED | Old remains owner; new ignored | Vacuum/normalization |
| Commit | Old hidden for qualifying new snapshots | New visible | Both updater effects effective | Both candidate generations heap-rechecked | Vacuum later |
| Old RR snapshot | Chapter-10 historical result | Chapter-10 result | Snapshot/status oracle | Candidate only | Vacuum after horizons |
| Fresh post-commit snapshot | Old not current | New current | COMMITTED | New candidate owns | Vacuum later |
| Same transaction later command | SELF command rules | SELF command rules | Exact cmin/cmax causality | Heap recheck | Vacuum later |

#### Retry and lock-retention matrix

| Retry point | Legal? | Retained ownership | Snapshot / CommandId | Repeated work | Count / outcome |
|---|---:|---|---|---|---|
| Before any locks | Yes if clean RC conflict | None beyond transaction baseline | Fresh / same | Full attempt | Reset zero |
| With TableWriterGate | Yes before publication | Shared gate | Fresh / same | Descriptor revalidation | Reset zero |
| With UNIQUE_KEY | Yes before publication | Key claim terminally | Fresh / same | Complete exact-key rescan | Reset zero |
| With old-RID TUPLE_WRITE | Yes before publication | Old claim terminally | Fresh / same | New discovery and target revalidation | Reset zero |
| Replacement targets different RID | Yes before publication | Prior claims plus canonical new claims | Fresh / same | Lock order/revalidation for new target | Reset zero |
| After first publication | No | Retained through automatic ABORT | No fresh attempt / consumed | None | MA; no success result |

#### Error and result matrix

| Error | Boundary relevance | State / retry | Count / undo | Exact owner |
|---|---|---|---|---|
| Ordinary UNIQUE violation | FA before first write; unreachable after W | No transparent post-write retry | No success / no physical undo | §§11.10, 15.1.3, 39.1.3 |
| Ordinary NOT NULL / supported row constraint | FA before first write; unreachable after W | No | No success / no undo | §§15.2, 15.1.3, 39.1.3 |
| Ordinary expression/type/conversion | FA before first write; unreachable after W | No | No success / no undo | §§5.6, 15.1.3, 39.1.3 |
| Deadlock | Independently fatal | MA | No success / no undo | §§11.13.4, 39.1.3 |
| RR serialization | Independently fatal | MA | No success / no undo | §§11.6, 39.1.3 |
| No replaceable frame | Before/after | FA/MA by boundary | No success / no undo | §§7.12.3, 39.1.3 |
| WAL position exhaustion | Before authorization or invariant failure | Exact Chapter-12/§39 result | No success / no guessed mutation | Numeric/WAL procedures |
| WAL I/O/ENOSPC | Known versus uncertain is decisive | FA/MA or NC | No success / exact local rollback only | §§12.12, 39.1 |
| Heap corruption | Independent of boundary | NC | No success / no repair guess | §§5.21, 39.1.3 |
| Index corruption | Independent of boundary | NC | No success / no heap-scan substitution | §§8.27–8.29, 39.1 |
| Status lookup failure | Before/after plus lower-layer class | FA/MA or NC as owned | No success / no guessed outcome | §§9.13, 39.1 |
| Schema/catalog failure | Before/after | FA/MA or NC if incoherent | No success / no stale descriptor guess | §§15.1.1, 39.1 |
| Cancellation | Before/after | FA/MA; deadlock remains MA | No success / no undo | §39.1.3 |
| Shutdown admission | Lifecycle state decides | Reject new; drain admitted work | No fabricated success | §§3.3.6, 39.1 |
| Database noncontinuable | Boundary does not downgrade | NC, no retry | No success / recovery owner | §§3.3.5, 12.12, 39.1 |

#### Cross-chapter composition matrix

| Owner | Chapter-15 obligation | Existing procedure | New composition |
|---|---|---|---|
| Chapter 5 | Header/RID/slot/update-version bytes | Slotted-page and tuple-codec tests | Tuple mutation header headers and old/new link fixture |
| Chapter 7 | Pin/latch/guard and publication | Buffer management verification | Heap-guard release before B+ MTR |
| Chapter 8 | Candidate scans and B+ MTR | B+ Tree Verification | DML heap-before-index and retained-entry prefixes |
| Chapter 9 | CommandId/snapshot/state | Transaction identity/snapshot tests | Statement-attempt ledger |
| Chapter 10 | Creator/deleter/SELF visibility | MVCC Visibility Tests | UPDATE/DELETE abort/commit proof |
| Chapter 11 | Locks/current owner/uniqueness | Locking and UNIQUE tests | End-to-end DML waits and scans |
| Chapter 12 | Authorization/publication/failure | WAL and non-crash fault procedures | Per-operation publication prefixes |
| Chapter 13 | Physical replay/no SQL replay | Recovery Property Tests | Aborted DML and no-result replay |
| Chapter 14 | Epoch/status/RID reuse | Vacuum and Reclamation Tests | DML request handoff and cutoff blocker |
| Chapter 15 | Attempt and DML composition | This section | Direct procedural owner |
| §31 | Finalized distinct targets/result spool | Target-spool and RETURNING procedures | Affected-row result cardinality/envelope |
| §39 | FA/MA/NC and no undo | Statement Failure tests | INSERT/UPDATE/DELETE specializations |
| §41 | Required deterministic coverage | Verification requirement map | Matrices and atomic map below |

#### High-level DML domain/case matrix

| Case | Deterministic ordering/fault | Independent oracle | Architecture | Verification owner |
|---|---|---|---|---|
| Clean RC retry | Conflict before first publication | Statement ledger | §§15.7.1, 39.1 | Attempt procedure |
| Post-publication failure | Fault after first publication | Prefix/state oracle | §§15.7.2, 39.1 | Failure-prefix procedure |
| COMMITTED owner, RC/RR | Same owner fixture under both isolation modes | Current-owner oracle | §§11.5–11.6, 15.7 | Current-owner composition |
| Multi-leaf exact-key scan | Equal run spans three leaves | Uniqueness oracle | §§11.10, 15.2–15.4 | Uniqueness composition |
| Exact SELF exclusion | Old RID plus same-Txn other RID | Uniqueness oracle | §15.3 | Uniqueness composition |
| INSERT/UPDATE headers | Commands 0/7/9 | Header-byte oracle | §§5.7, 15.2–15.3 | Header fixtures |
| Malformed no-deleter cmax | Single-field byte mutations | Header validator model | §5.7.3 | Header fixtures |
| INSERT heap -> index failure | Barriers after heap/each MTR | Prefix oracle | §15.2 | INSERT prefixes |
| UPDATE replacement/old-header failures | Barriers after each live publication stage | Visibility/prefix oracles | §15.3 | UPDATE prefixes |
| DELETE failure | Before/after xmax publication | Visibility/prefix oracles | §15.4 | DELETE prefixes |
| Aborted UPDATE | Persist each legal prefix, recover | Visibility oracle | §§10, 15.3, 15.6 | UPDATE outcome matrix |
| FA / MA / NC | One isolated failure each side of boundary | Statement ledger | §39.1 | Failure matrix |
| No-op UPDATE count | Same rows, varied physical work | Affected-row oracle | §§15.1.2, 15.3 | Count fixtures |
| Retry count reset | Provisional 5 -> final 3 | Affected-row oracle | §§15.1.2, 15.7.1 | Count fixtures |
| Partial multirow abort | Rows 1–7 publish, row 8 fails | State/count oracle | §§15.1.2, 39.1.4 | Failure/count fixtures |
| Explicit/autocommit output | Pause at statement/C3/C4/C5 | Result-envelope oracle | §§15.1.2, 15.7.3 | Output fixtures |
| DML/vacuum handoff | Barriers at epoch/request/reuse | Reuse conjunction oracle | §§14.6, 14.18, 15.3–15.4 | Cross-owner procedure |
| Checkpoint concurrency | Capture during each publication stage | WAL/DPT oracle | §§7.10.5, 12, 13 | Cross-owner procedure |
| Recovery | Crash at valid prefixes | Logical committed model | §§13, 15.6 | Recovery Property Tests |

### Chapter 15 atomic architecture-obligation coverage map

Each row is one independently falsifiable Chapter-15 obligation. Component-only rules map
to their procedure with an independent oracle; cross-subsystem ordering and result rules map to the
DML procedures and matrices above.

| ID | Domain | Atomic obligation | Architecture owner | Deterministic procedure/reference |
|---:|---|---|---|---|
| 1 | A | One SQL statement may own multiple internal attempts | §§15.7, 39.1.4 | Statement ledger with two attempts |
| 2 | A | Every attempt has an observable begin/end | §15.7 | Attempt barriers/event log |
| 3 | A | Abandoned attempts expose no logical result | §§15.1.2, 15.7.3 | Retry/output fixture |
| 4 | A | Final successful attempt alone owns statement success | §15.1.2 | Statement ledger/result oracle |
| 5 | A | Multirow work remains one admitted statement | §§9.6, 15.1.2 | Multirow ledger fixture |
| 6 | B | First admitted statement uses CommandId 0 | §9.6 | CommandId matrix |
| 7 | B | Clean internal retry reuses the same CommandId | §§9.6, 15.7.1 | RC retry fixture |
| 8 | B | Successful statement consumes its CommandId | §9.6 | Sequential statement fixture |
| 9 | B | Recoverable failed statement consumes its CommandId | §§9.6, 39.1.4 | FA then next-statement fixture |
| 10 | B | MUST_ABORT-producing statement consumes its CommandId | §§9.6, 39.1.4 | MA ledger fixture |
| 11 | B | One multirow statement uses one CommandId | §9.6 | Multirow header/ledger fixture |
| 12 | B | `UINT32_MAX` is final legal and exhaustion never wraps | §§9.6, 39.1 | Numeric exhaustion specialization |
| 13 | C | Each RC attempt registers a statement snapshot | §9.9 | Snapshot event log |
| 14 | C | Clean retry unregisters the abandoned snapshot | §§9.9, 15.7.1 | Two-snapshot barrier fixture |
| 15 | C | RC replacement attempt captures a fresh snapshot | §§9.9, 15.7.1 | Distinguishable committed-change fixture |
| 16 | C | Retry is legal only before first statement publication | §15.7.1 | Pre-publication conflict fixture |
| 17 | C | Retry discards target/result attempt state | §§15.7.1, 31.5 | Spool snapshot and cleanup assertions |
| 18 | C | Retry repeats discovery, revalidation, and uniqueness | §§15.7.1, 31.5 | Semantic event-count assertions |
| 19 | C | Post-publication RC restart is forbidden | §15.7.2 | Post-publication conflict fixture |
| 20 | C | Whole-request resubmission, if any, uses a new TxnId | §15.7.2 | Two-request identity fixture |
| 21 | D | RR retains its transaction snapshot | §§9.10, 15.7 | RR snapshot-registration assertion |
| 22 | D | RR conflict does not refresh/retry | §§11.6, 15.7 | Committed-owner RR fixture |
| 23 | D | RR serialization is transaction-fatal | §§11.6, 39.1.3 | RR MA/automatic-ABORT fixture |
| 24 | E | DML acquires shared TableWriterGate before first write | §15.1.1 | Gate/publication event order |
| 25 | E | Writer gate precedes tuple/key locks | §15.1.1 | Operation-order event log |
| 26 | E | Gate acquisition holds no forbidden physical protection | §15.1.1 | Ownership snapshot at wait |
| 27 | E | Gate stabilizes target schema/index set | §§15.1.1, 21.2.1 | DDL/DML barrier fixture |
| 28 | E | Gate remains through terminal publication/release | §§11.11, 15.5–15.6 | C4/C5/A2/A3 observers |
| 29 | F | UPDATE registers exact old-RID TUPLE_WRITE | §15.3 | Request-key event assertion |
| 30 | F | DELETE registers exact target-RID TUPLE_WRITE | §15.4 | Request-key event assertion |
| 31 | F | Immediate grant still requires revalidation | §§15.3–15.4 | No-wait revalidation fixture |
| 32 | F | Queued request remains a RID-retention claim | §§14.18, 15.3–15.4 | Reuse-versus-wait schedule |
| 33 | F | Granted ownership remains through C5/A3 | §§11.11, 15.5–15.6 | Terminal lifetime fixture |
| 34 | F | Deadlock/cancellation cannot leave or later grant a stale request | §§11.13.4, 14.18 | Cancellation/grant/reuse matrix |
| 35 | G | Candidate discovery occurs under a read epoch | §§14.6, 15.3–15.4 | Epoch event assertion |
| 36 | G | Epoch overlaps live TUPLE_WRITE registration | §§14.18, 15.3–15.4 | Registration barrier |
| 37 | G | Epoch releases only after request is live | §§14.18, 15.3–15.4 | Event-order assertion |
| 38 | G | Epoch releases before any blocking wait | §§14.18, 15.3–15.4 | Wait-entry ownership assertion |
| 39 | G | Registration/reuse race has only two legal linearizations | §14.18 | Paired handoff schedules |
| 40 | H | Grant causes fresh page pin/latch and tuple fetch | §§11.3, 15.3–15.4 | Post-grant event sequence |
| 41 | H | TableId/RID identity is revalidated | §§11.2, 15.3–15.4 | Rebound-RID fixture |
| 42 | H | Slot/header validity is revalidated | §§5.4, 15.3–15.4 | DEAD/UNUSED/corrupt fixtures |
| 43 | H | MVCC/current owner is recomputed after wait | §§11.4, 15.3–15.4 | Terminal-owner schedules |
| 44 | H | Required predicate qualification is rechecked | §§15.1.2, 31.5 | Changed-predicate fixture |
| 45 | H | UNIQUE wait is followed by target revalidation | §§15.3–15.4 | Unique-grant target-change fixture |
| 46 | H | Pre-wait bytes never authorize post-wait mutation | §§11.3, 15.3–15.4 | Poisoned old-copy assertion |
| 47 | I | No-xmax sentinel permits write after validation | §11.4.1 | Current-owner matrix |
| 48 | I | SELF uses exact command-aware causality | §§10.3.2, 11.4.2 | SELF boundary fixtures |
| 49 | I | IN_PROGRESS waits and rechecks | §11.4.4 | Commit/abort terminal schedules |
| 50 | I | COMMITTED owner gives clean RC retry only pre-publication | §§11.5, 15.7 | RC paired boundary fixture |
| 51 | I | COMMITTED owner gives RR serialization | §11.6 | RR owner fixture |
| 52 | I | ABORTED owner is ineffective only after revalidation | §11.4.3 | Aborted-owner fixture |
| 53 | I | Required RETIRED/INVALID/RESERVED outcome is rejected | §§9.13, 11.4 | Negative status rows |
| 54 | I | Status I/O/checksum/format failure is propagated | §§9.13, 39.1 | Injected lookup failures |
| 55 | I | Current-owner error cannot become row-not-found or guessed status | §§10.4, 39.1 | Error-precedence assertions |
| 56 | J | Fully non-NULL unique claims use stable IndexId/key identity | §§11.8–11.9 | Lock-key oracle |
| 57 | J | Multiple unique claims use canonical sorted order | §11.9 | Opposite-input-order fixture |
| 58 | J | NULL ordinary UNIQUE creates no ordinary key claim | §11.10.2 | Lock-event NULL fixture |
| 59 | J | PK NULL rejects before unique admission | §11.10.2 | Error/event-order fixture |
| 60 | J | UNIQUE_KEY remains terminal-duration | §11.11 | C4/C5/A2/A3 fixture |
| 61 | J | Nonunique indexes do not invoke uniqueness admission | §§11.8–11.10 | Nonunique lock-event fixture |
| 62 | K | Uniqueness uses current state, not caller snapshot | §11.10.1 | Old-snapshot/live-owner fixture |
| 63 | K | Exact-key scan begins at complete lower bound | §§8.11, 11.10.4 | Multi-leaf candidate model |
| 64 | K | Every equal-key candidate across leaves is enumerated | §§8.22, 11.10.4 | Candidate-count oracle |
| 65 | K | Every candidate RID is heap-rechecked | §11.10.4 | Recheck event-count oracle |
| 66 | K | Committed/frozen live creator conflicts | §§11.10.4–11.10.6 | Creator matrix |
| 67 | K | Active creator/deleter waits then rescans completely | §11.10.7 | Both terminal outcomes |
| 68 | K | Aborted creator and committed deleter do not own key | §11.10.4 | Status matrix |
| 69 | K | Aborted deleter preserves creator ownership | §11.10.4 | Status matrix |
| 70 | K | Dangling/malformed/reused candidate is corruption | §§11.10.4, 11.10.8 | Identity-negative fixture |
| 71 | L | Same-key UPDATE excludes only exact old RID | §§11.10.3, 15.3 | Two-RID SELF fixture |
| 72 | L | Same transaction's other live row conflicts | §§11.10.5–11.10.6 | Other-row fixture |
| 73 | L | Same-command other-row delete does not free key | §11.10.6 | Command-aware key fixture |
| 74 | L | Earlier-command self delete may free key | §11.10.6 | Earlier-command fixture |
| 75 | L | Current-command replacement is not transaction-wide excluded | §§11.10.3–11.10.6 | Multirow UPDATE collision |
| 76 | L | Immediate key swap follows current-state result | §11.10.6 | Two-row swap fixture |
| 77 | M | INSERT type/conversion validation precedes unsafe publication | §§15.2, 31.6 | Pre-publication error fixture |
| 78 | M | NOT NULL failure has no published row | §§15.2, 39.1.3 | Constraint event assertion |
| 79 | M | Tuple size 8135 succeeds when otherwise valid | §5.6 | Boundary encode/insert fixture |
| 80 | M | Tuple size 8136 rejects before publication | §5.6 | Boundary negative fixture |
| 81 | M | Same-statement input rows participate in immediate uniqueness | §§15.2, 31.6 | Multirow duplicate fixture |
| 82 | N | Fresh INSERT RID comes from validated heap geometry | §§5.14, 15.2 | FSM/heap oracle |
| 83 | N | FSM stale-low cannot make heap unsafe | §§6, 15.2 | Stale-low fixture |
| 84 | N | FSM stale-high cannot overrun page | §§6, 15.2 | Stale-high fixture |
| 85 | N | New page uses existing PAGE_INIT path | §§7.12.4, 12.9, 15.2 | PAGE_INIT composition |
| 86 | N | INSERT publishes byte-exact fresh header/body | §§5.7–5.12, 15.2 | Header/page oracle |
| 87 | N | Write-status dependency is held at xmin publication | §§9.4, 15.1.1 | Dependency/publication event assertion |
| 88 | N | Heap publication sets page_lsn/dirty/DPT correctly | §§7.10, 12.12, 15.2 | Prefix oracle |
| 89 | N | Heap guard releases latch-before-pin before B+ MTR | §15.2 | Direct physical-ownership barrier |
| 90 | O | Heap redo precedes every referring index MTR | §§12.11, 15.2 | WAL-order assertion |
| 91 | O | Every required index receives `(key,RID)` | §§8.23, 15.2 | Manifest/index-set comparison |
| 92 | O | Each index MTR uses its own B+ guards | §§8.19, 8.25, 15.2 | Guard-owner event log |
| 93 | O | Physical guard release does not release logical locks | §15.2 | Simultaneous ownership snapshot |
| 94 | O | Exact physical duplicate is not intentionally published | §§8.22, 15.2 | Duplicate-key model |
| 95 | O | Failure after heap/each index is MA or lower-layer NC | §§15.2, 39.1 | Per-stage fault prefix |
| 96 | O | Aborted INSERT index candidate is ignored only after heap/status proof | §§8.23, 10.2, 15.2 | Reopen/index-recheck fixture |
| 97 | P | UPDATE target set finalizes before mutation | §§31.1, 31.3 | Spool-finalize barrier |
| 98 | P | Finalized target spool contains each RID once | §31.2 | Duplicate-producing child fixture |
| 99 | P | Spool spill preserves RID and needed old bytes | §31.4 | In-memory/spill equivalence |
| 100 | P | Failed spool finalization publishes no target mutation | §§31.1, 39.1 | Finalize fault fixture |
| 101 | P | Final target survives lock/revalidation/predicate rules | §§15.1.2, 31.5 | Changed-target fixture |
| 102 | P | Halloween key/predicate changes do not rediscover target | §31.3 | Indexed key-changing UPDATE |
| 103 | Q | Old UPDATE version receives updater xmax | §15.3 | Exact header/page fixture |
| 104 | Q | Old UPDATE version receives same statement cmax | §15.3 | Exact header/page fixture |
| 105 | Q | Old header publishes only after replacement publication | §15.3 | Event-order assertion |
| 106 | Q | Direct old-xmax-only runtime prefix is unreachable | §15.3 | N/A with live-order trace |
| 107 | R | Replacement UPDATE version receives fresh physical RID | §§5.15, 15.3 | RID inequality assertion |
| 108 | R | Replacement xmin/cmin use current transaction/command | §15.3 | Header-byte oracle |
| 109 | R | Replacement no-deleter pair is `(0,0)` | §§5.7.3, 15.3 | Tuple mutation header fixture |
| 110 | R | Replacement prev points to exact old RID | §§5.7.4, 15.3 | Link-byte oracle |
| 111 | R | Replacement publishes before old header | §15.3 | Publication event order |
| 112 | R | Aborted replacement is invisible without physical undo | §§10.2, 15.3, 15.6 | Abort/reopen visibility fixture |
| 113 | S | UPDATE installs a new entry for every index | §§8.23, 15.3 | Index-manifest comparison |
| 114 | S | UPDATE retains every old physical entry | §§8.23, 15.3 | Before/after tree oracle |
| 115 | S | Unchanged indexed key has old/new RID candidates | §15.3 | Same-key two-RID fixture |
| 116 | S | Changed indexed key retains old and adds new candidate | §15.3 | Changed-key fixture |
| 117 | S | Partial multi-index failure yields no successful statement | §§15.3, 39.1 | Per-MTR fault fixture |
| 118 | S | Heap recheck prevents aborted/stale candidate from semantic aliasing | §§8.23, 15.3 | Abort/reuse candidate fixture |
| 119 | T | DELETE target set finalizes before mutation | §§31.1, 31.3 | Spool-finalize barrier |
| 120 | T | DELETE deduplicates one target RID | §31.2 | Join-expanded target fixture |
| 121 | T | DELETE performs epoch/TUPLE_WRITE handoff | §§14.18, 15.4 | Handoff schedule |
| 122 | T | DELETE revalidates owner/predicate after every wait | §§15.4, 31.5 | Changed-target fixture |
| 123 | T | Stale/nonqualifying target is not deleted or counted | §§15.1.2, 15.4 | Stale-target oracle |
| 124 | U | DELETE publishes current TxnId as xmax | §15.4 | Header-byte fixture |
| 125 | U | DELETE publishes current CommandId as cmax | §15.4 | Header-byte fixture |
| 126 | U | DELETE holds required old-key claims before xmax | §15.4 | Lock/publication event order |
| 127 | U | DELETE retains physical index entries | §§8.23, 15.4 | Exact tree before/after |
| 128 | U | Abort makes deleter ineffective; commit makes it effective | §§10.3, 15.4 | Paired terminal visibility fixture |
| 129 | U | DELETE/INSERT key race follows deleter terminal outcome | §§11.10, 15.4 | Two-terminal race fixture |
| 130 | V | Private WAL construction does not set published-write flag | §§12.12, 39.1.2 | Pre-append barrier |
| 131 | V | Reservation alone does not set the flag | §§12.12.1, 39.1.2 | Reservation barrier |
| 132 | V | Publication-authorizing append/page publication is exact boundary | §§12.12, 39.1.2 | Event-order/prefix oracle |
| 133 | V | Publication differs from WAL durability | §§12.12–12.13 | Pause before flush |
| 134 | V | Publication differs from page durability | §§7.10–7.11 | Pause before writeback |
| 135 | V | Publication differs from transaction COMMIT | §§9.14, 15.5 | Explicit-transaction visibility fixture |
| 136 | V | DML introduces no new WAL record family | §§12.7–12.11, 15.2–15.4 | WAL registry/payload inspection |
| 137 | W | Pre-publication recoverable statement failure is FA | §§39.1.2–39.1.4 | Boundary matrix |
| 138 | W | Clean RC retry is distinct from FA result | §§15.7.1, 39.1.4 | No-client-result assertion |
| 139 | W | Post-publication non-success is MA | §§15.7.2, 39.1.3 | Per-stage faults |
| 140 | W | Independently fatal deadlock/RR serialization is MA | §39.1.3 | Fault/status fixtures |
| 141 | W | Uncertain/incoherent storage state is NC | §§12.12.4, 39.1 | Uncertain append/publication fixture |
| 142 | W | Original diagnostic survives automatic ABORT errors | §39.1.7 | Chained-failure fixture |
| 143 | W | MA keeps resources through A2/A3 | §§15.6, 39.1.8 | Paused automatic-ABORT fixture |
| 144 | W | COMMIT is forbidden after MA | §§9.4, 39.1.1 | Admission assertion |
| 145 | W | NC closes ordinary database admission | §§3.3.5, 39.1.1 | Lifecycle gate fixture |
| 146 | X | Ordinary user ABORT performs no heap/index write-set undo | §§13.16, 15.6 | Exact page before/after ABORT |
| 147 | X | Failed INSERT physical bytes become aborted garbage | §§15.2, 15.6 | Crash/reopen visibility fixture |
| 148 | X | Failed UPDATE preserves old and aborted new semantics | §§10, 15.3, 15.6 | Update outcome matrix |
| 149 | X | Failed DELETE leaves ineffective xmax | §§10.3, 15.4, 15.6 | Abort visibility fixture |
| 150 | X | Vacuum, not statement rollback, removes retained garbage | §§14, 15.2–15.6 | Reclamation cross-reference |
| 151 | Y | INSERT command-0 header has cmax zero | §§5.7.3, 15.2 | Exact header matrix |
| 152 | Y | INSERT nonzero-command header still has cmax zero | §§5.7.3, 15.2 | Exact header matrix |
| 153 | Y | UPDATE replacement header has cmax zero | §§5.7.3, 15.3 | Exact header matrix |
| 154 | Y | Actual deleter stores actual deleting CommandId | §§5.7.3, 15.3–15.4 | Deleter control fixture |
| 155 | Y | Invalid-xmax/nonzero-cmax cases are `CORRUPT_HEAP` | §5.7.3 | Three byte-negative fixtures |
| 156 | Y | Ordinary read does not normalize malformed cmax | §5.7.3 | Before/after byte assertion |
| 157 | Y | Aborted-xmax normalization ends in exact `(0,0)` | §14.13.1 | Pre/post WAL byte fixture |
| 158 | Y | Header oracle is independent of production codec | §§5.7, 15.2–15.3 | Test-side little-endian builder |
| 159 | Z | Affected-row count is final-successful-attempt logical metadata | §15.1.2 | Affected-row oracle |
| 160 | Z | Single/N-row INSERT counts logical input rows once | §§15.1.2, 15.2 | 1 and N fixtures |
| 161 | Z | UPDATE counts distinct finalized targets once | §§15.1.2, 15.3 | N-target fixture |
| 162 | Z | No-op UPDATE counts qualifying finalized targets | §15.3 | `SET x=x` fixture |
| 163 | Z | DELETE counts distinct finalized deletes once | §§15.1.2, 15.4 | N-target fixture |
| 164 | Z | Zero-target UPDATE/DELETE succeeds with zero | §15.4 | Empty finalized spool fixture |
| 165 | Z | Stale target contributes zero unless final attempt acts | §15.1.2 | Changed-predicate fixture |
| 166 | Z | Duplicate candidate cannot double count | §§15.1.2, 31.2 | Duplicate-target fixture |
| 167 | Z | RC retry discards old provisional count | §§15.1.2, 15.7.1 | 5->3 and 4->0 fixtures |
| 168 | Z | Failed/MA multirow DML publishes no partial count | §§15.1.2, 39.1.4 | Seven-then-dynamic-failure fault |
| 169 | Z | Count is independent of versions/indexes/WAL/pages | §15.1.2 | Physical-shape metamorphic fixture |
| 170 | AA | Failed statement publishes error, not count/RETURNING prefix | §§15.1.2, 15.7.3 | Late-row/result-spool faults |
| 171 | AA | Explicit transaction may publish at statement success | §§15.1.2, 31.9 | Pause before later COMMIT |
| 172 | AA | Later rollback does not rewrite prior statement response | §15.1.2 | Success-then-rollback fixture |
| 173 | AA | Statement success does not imply COMMIT/durability | §§15.1.2, 15.5 | Explicit transaction fixture |
| 174 | AA | Autocommit withholds count/RETURNING through C4–C5 | §§15.1.2, 31.9 | Commit-stage barriers |
| 175 | AA | Autocommit COMMIT failure exposes no success result | §§15.1.2, 39.1.5 | Pre-success commit fault |
| 176 | AA | Count/result metadata is not persisted or recovered | §15.1.2 | WAL/control/checkpoint/reopen absence assertions |
| 177 | AB | Successful multirow INSERT counts each logical row once | §§15.1.2, 15.2 | N-input fixture |
| 178 | AB | Multirow UPDATE/DELETE uses one finalized distinct target set | §§31.1–31.2 | In-memory/spill target equivalence |
| 179 | AB | No target mutation begins before spool Finalize | §§31.1, 31.3 | Finalize barrier |
| 180 | AB | Duplicate target production cannot mutate twice | §31.2 | Join-expanded fixture |
| 181 | AB | Partial physical multirow progress is not partial SQL success | §§15.1.2, 39.1.4 | Row-N fault fixture |
| 182 | AB | Separate statements may each count same logical row once | §15.1.2 | Successive-command UPDATE fixture |
| 183 | AC | FSM is advisory, heap geometry authoritative | §§6, 15.2 | Stale-high/low fixtures |
| 184 | AC | No-space search cannot reuse DEAD directly | §§5.4.3, 14.12 | DEAD candidate fixture |
| 185 | AC | Page/slot exhaustion never wraps or overwrites | §§4.3, 5.4 | Numeric/page allocation procedures |
| 186 | AC | FSM update failure cannot redefine DML correctness | §§6.12, 15.2 | Post-publication FSM fault |
| 187 | AD | No-replaceable-frame maps by first-publication boundary | §§7.12.3, 39.1 | Pre/post resource fixture |
| 188 | AD | WAL position exhaustion prevents unauthorized mutation | §§12.12, 39.1 | Terminal-boundary procedure |
| 189 | AD | WAL ENOSPC distinguishes known from uncertain outcome | §§12.12.4, 39.1 | Append fault matrix |
| 190 | AD | OOM maps FA/MA without hidden retry | §39.1.3 | Pre/post allocation fault |
| 191 | AD | Spill failure cleans temporary state and maps FA/MA | §§31.4, 39.1.3 | Spool spill fault fixture |
| 192 | AD | Lock/request registration exhaustion leaves coherent ownership | §§11.12–11.13, 39.1 | Lock resource matrix |
| 193 | AD | Resource failures never fabricate partial success | §§15.1.2, 39.1 | Result-envelope assertions |
| 194 | AE | Heap corruption is never skipped as row-not-found | §§5.21, 39.1 | Corrupt target fixture |
| 195 | AE | Index corruption is not replaced by an ad hoc heap scan | §§8.27–8.29, 39.1 | Corrupt candidate fixture |
| 196 | AE | Status corruption/failure cannot invent owner outcome | §§9.13, 10.4 | Lookup fault fixtures |
| 197 | AE | Invalid RID sentinel is never a physical target | §§4.3, 5.4 | Revalidation negative fixture |
| 198 | AE | DEAD/UNUSED target is not writable old identity | §§5.4, 14.12 | Slot-state revalidation rows |
| 199 | AE | Rebound same RID cannot satisfy old request | §§14.18, 15.3–15.4 | Reuse-first handoff schedule |
| 200 | AF | Shared writer gate stabilizes schema/index descriptors | §15.1.1 | DDL/DML concurrency fixture |
| 201 | AF | Tuple encoder uses valid schema version | §§5.13, 15.2–15.3 | Descriptor/header comparison |
| 202 | AF | Every stable index in operation set is maintained | §§15.1.1, 15.2–15.3 | Manifest/index event comparison |
| 203 | AF | Catalog lookup failure cannot use unvalidated stale descriptor | §§15.1.1, 39.1 | Lookup fault fixture |
| 204 | AF | Same-table DDL upgrade prohibition is preserved | §15.1.1 | Gate transition rejection fixture |
| 205 | AG | Read epoch/claim prevents vacuum RID rebind during DML | §§14.18, 15.3–15.4 | Cross-owner barrier schedule |
| 206 | AG | Index/predecessor cleanup remains vacuum-owned | §§14.9–14.12, 15.4 | Ownership/event absence assertion |
| 207 | AG | DML write-status dependency blocks unsafe cutoff | §§14.14, 15.1.1 | Idle writer/cutoff fixture |
| 208 | AG | DML cannot publish current TxnId after C5/A3 | §§15.1.1, 15.5–15.6 | Post-release admission assertion |
| 209 | AH | Checkpoint sees old or fully published DML page state | §§7.10.5, 12.12, 13.5 | Publication observer barriers |
| 210 | AH | DML dirty pages preserve WAL-before-data/DPT rec_lsn | §§7.11, 12.16, 15.2–15.4 | WAL/DPT oracle |
| 211 | AH | DML requires neither statement checkpoint nor data-page force | §§15.5, 15.9 | Checkpoint/writeback absence assertions |
| 212 | AI | Recovery admits no ordinary DML before READY | §§13.19, 15.8 | Lifecycle admission fixture |
| 213 | AI | Recovery replays physical WAL, not SQL statements | §§13.13, 15.8 | Redo-dispatch event assertion |
| 214 | AI | Recovery does not replay locks or uniqueness admission | §§11.10.8, 13.13 | Runtime-state reset assertion |
| 215 | AI | Recovery does not reconstruct count/RETURNING output | §§15.1.2, 31.9 | Reopen result-state absence |
| 216 | AI | Logical reopen model contains only durable commits | §§13.20, 15.5–15.6 | Recovery Property Tests |
| 217 | AJ | DML begins only in READY | §§3.3, 15.8 | Lifecycle state matrix |
| 218 | AJ | DRAINING rejects new DML | §§3.3.6, 15.8 | Admission barrier |
| 219 | AJ | In-flight DML quiesces through canonical transaction path | §§3.3.6, 15.5–15.6 | Shutdown/transaction schedule |
| 220 | AJ | NONCONTINUABLE admits no ordinary continuation | §§3.3.5, 39.1 | Lifecycle gate fixture |
| 221 | AK | Retained locks across retry do not retain semantic decisions | §§11.11, 15.7.1 | Retry/lock matrix |
| 222 | AK | Retained UNIQUE_KEY still requires fresh exact-key rescan | §§11.10, 15.7.1 | Retry with key claim fixture |
| 223 | AK | Retained old-RID TUPLE_WRITE does not bind replacement attempt target | §§11.2, 15.7.1 | Retry targeting different RID |
| 224 | AK | Retry count/boundedness remains policy-free within legal boundary | §15.7.1 | Finite barrier fixtures; no timing/count assumption |
| 225 | AK | verification assumes no transparent whole-request automatic rerun | §15.7.2 | Assert no new request/TxnId appears without fixture submission |
| 226 | AK | verification assumes no savepoint/subtransaction outcome | §15.7.3 | Runtime/WAL registry absence assertion |
| 227 | AK | verification assumes no statement-local physical undo | §§15.6–15.7.3 | Failed-prefix bytes plus semantic ABORT oracle |
