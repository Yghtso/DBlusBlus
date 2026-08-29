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

## Numeric Exhaustion and Terminal-Boundary Verification

This section is the detailed verification owner for the checked-advancement,
exhaustion, and terminal-state contracts in [`ARCHITECTURE.md`](ARCHITECTURE.md)
§4.3.2 and §§4.3.2.1–4.3.2.6. The architecture remains the authority for legal
values and outcomes; this section defines how tests construct and observe those
boundaries. Existing WAL/MTR, PAGE_INIT, catalog, CommandId, statistics, and
vacuum procedures remain the specialized oracles referenced below.

### Exhaustion-domain inventory

The classifications used here are:

```text
A = durable monotonic identity
B = durable position or offset
C = per-transaction counter
D = file-local allocation bound
E = structural format bound
F = runtime generation or epoch
G = other architecture-defined exhaustion domain
```

The advancing identities, counters, positions, and allocation bounds are:

| Domain | Class | Width and valid maximum | Invalid/reserved terminal values | Advancement and persistence scope | Reuse policy | Exhaustion result | Canonical owner |
|---|---:|---|---|---|---|---|---|
| `FileId` | A | uint32; last returned `UINT32_MAX-1` | `0` invalid; `UINT32_MAX` is terminal next-value state, not a returned ID | Persist `next_file_id=candidate+1` through the control slot before return | Never | `FILE_ID_EXHAUSTED` before object publication | §§4.3.2.1, 13.2.5 |
| `TableId` | A | uint64; last allocated `UINT64_MAX-1` | `0` invalid; `1..6` fixed built-ins | Shared `next_catalog_object_id`, durable control update before return | Never | `ID_EXHAUSTED` before DDL publication | §§4.3.2.1, 13.2.6, 16.5.1 |
| `IndexId` | A | uint64; last allocated `UINT64_MAX-1` | `0` invalid; ordinary allocations are at least `7` | Same shared allocator as TableId and ConstraintId | Never | `ID_EXHAUSTED` before DDL publication | §§4.3.2.1, 13.2.6 |
| `ConstraintId` | A | uint64; last allocated `UINT64_MAX-1` | `0` invalid; ordinary allocations are at least `7` | Same shared allocator as TableId and IndexId | Never | `ID_EXHAUSTED` before DDL publication | §§4.3.2.1, 13.2.6 |
| `TxnId` | A | uint64; last `18,446,744,073,708,503,041` | `0` invalid, `1` frozen; larger terminal suffix is not a partial block | Reserve exact `2^20` blocks by durably advancing `reserved_txn_id_end` before issue | Never, including after freezing/status reclamation | `TXN_ID_EXHAUSTED` at transaction admission | §§4.3.2.1, 9.2–9.3 |
| `CommandId` | C | uint32; `UINT32_MAX` is usable once | No sentinel; `0` is legal | Checked transaction-local statement advancement; not persisted as a global allocator | Never within one transaction | `COMMAND_ID_EXHAUSTED` for the next ordinary statement | §§4.3.2.2, 9.6 |
| `ColumnId` | A | uint32; at most `UINT32_MAX`, subject to complete-schema/tuple bounds | `0` invalid | Table-local schema construction before catalog publication | Never within schema history | Reject schema construction/DDL before publication | §§4.3.2.1, 16.3–16.5 |
| `SchemaVer` | A | uint32; `UINT32_MAX`; v1 emits only `1` | `0` invalid | Checked schema-history advancement before catalog publication | Never for one table | Reject schema-changing DDL before publication | §§4.3.2.1, 16.5 |
| Control-slot generation | G | uint64; `UINT64_MAX` | `0` invalid | Alternating control-slot write and sync before runtime generation publication | Never | Control update fails; its requiring operation cannot proceed | §§4.3.2, 13.2.3–13.2.4 |
| `PageNo` | D | uint64 carrier; last allocatable ordinary page `1,125,899,906,842,622` | `UINT64_MAX` invalid; page `0` is superblock | Checked PageNo/count/offset/length before extension and WAL/page publication | Unpublished tail only after exact rollback; published reuse only by owner protocol | `PAGE_NUMBER_EXHAUSTED` for that file | §§4.3.2.3, 4.11, 12.12 |
| `published_page_count` | D | uint64 exclusive bound; max `1,125,899,906,842,623` | `0` cannot describe initialized file | Reconstructed from aligned length plus WAL/recovery, then advanced with page publication | May retreat only for proven unpublished serialized tail | Same PageNo/I/O boundary | §§4.3.2.3, 4.11, 12.12 |
| `SlotId` | E | uint16 carrier; v1 geometry permits `0..1017` | `UINT16_MAX` invalid; no global allocator terminal | Page-local slot-directory growth or grace-authorized free-slot pop | Only canonical `DEAD -> UNUSED` after §14.6 grace | Page-local `NO_SPACE`, never global ID exhaustion | §§4.3.2, 5.3–5.5, 14.6 |
| WAL record-start `Lsn` / exclusive end | B | uint64 start; last minimum-record start `2^64-48`; mathematical end may equal `2^64` | `0` invalid; end `2^64` is terminal and not an encodable Lsn | Simulate header, payload, alignment, segment tail/PAD, and exclusive end before reservation | Never; segment deletion does not reuse positions | `WAL_POSITION_EXHAUSTED` before reservation/publication | §§4.3.2.4, 12.2, 12.12–12.13 |
| WAL segment index | B | mathematical index; max `2^38-1` | Greater index forbidden even if filename grammar can spell it | Derive from admitted LSN; create exact segment namespace entry | Never assigned to a later position | `WAL_POSITION_EXHAUSTED` | §§4.3.2.4, 12.2.1 |
| Statistics `chunk_count` / `chunk_index` | E | count max `1,048,576`; index max `1,048,575` | count `0` invalid; index zero-based | Exact generation construction and catalog publication | Not reused within one generation | Fail/invalidate ANALYZE generation; statistics fallback | §§4.3.2, 16.5.7, 34.14 |
| B+ `tree_height` / node `level` | E | uint16; height max `UINT16_MAX`, level max `UINT16_MAX-1` | height `0` invalid; leaf level `0` legal | Root-growth MTR computes level/height before provisional mutation/publication | Height may contract; no numeric wrap | Fail root-growth MTR before publication | §§4.3.2, 8.15 |

The encoded-length, structural-count, and process-local domains are:

| Domain | Class | Width and valid maximum | Invalid/reserved terminal values | Advancement and persistence scope | Reuse policy | Exhaustion result | Canonical owner |
|---|---:|---|---|---|---|---|---|
| Heap `slot_count` | E | uint16 field; `1018` | Zero legal; values above geometry invalid | Page-local checked directory geometry | Count persists; slots reuse only through free list/grace | `NO_SPACE`, not numeric ID exhaustion | §§4.3.2.5, 5.3–5.5 |
| Heap `tuple_length` | E | uint16 field; complete encoded tuple through `8135` bytes | Zero only for canonical reclaimed states | Exact tuple construction before page publication | N/A | `ROW_TOO_LARGE` | §§4.3.2.5, 5.6–5.13 |
| FSM `entry_count` | E | uint16 field; `8144` | Zero legal | Checked page-local initialized-prefix construction | N/A | Next FSM page or owning PageNo/file failure | §§4.3.2.5, 6.5–6.7 |
| B+ node `slot_count` | E | uint16 field; `1016` | Zero legal | Checked node construction/split/rebalance before MTR publication | Slots are structural positions | Split/rebalance or pre-publication failure | §§4.3.2.5, 8.7–8.17 |
| B+ user-key / entry lengths | E | uint16 fields; key `1024`, leaf `1040`, internal `1048` | Zero-length key invalid | Exact key/entry construction before page/MTR publication | N/A | Key-too-large/construction failure | §§4.3.2.5, 8.6–8.10 |
| WAL `total_length` / payload length | E | uint32 fields; total `67,108,864`, payload `67,108,816` | total zero invalid; payload zero legal | Exact size plus Align8 before narrowing/reservation | N/A | `WAL_RECORD_TOO_LARGE` / `ENCODED_LENGTH_EXCEEDED` | §§4.3.2.4–4.3.2.5, 12.6–12.7 |
| Default-blob `total_length` | E | uint32 field; complete blob `4096` bytes | Zero invalid | Exact blob construction before catalog publication | N/A | Reject oversized default | §§4.3.2.5, 21.12.1 |
| `PersistedScalarV1.payload_length` | E | uint32; no independent maximum beyond exact `Align8(16+payload)` fitting its owner envelope | Zero is type/NULL dependent | Exact scalar plus enclosing default/statistics construction | N/A | Enclosing builder fails; never truncate | §§4.3.2.5, 17.13 |
| Statistics scope/manifest counts and lengths | E | uint32; complete scope/count arithmetic at most `UINT32_MAX` plus narrower owner limits | Zero is field-specific | Exact arrays, products, sums, and chunking before generation publication | N/A | Fail/invalidate complete generation | §§4.3.2.5, 34.14 |
| Checkpoint DATA indexes/counts/totals | E | uint32; indexes `0..UINT32_MAX-1`, counts/totals through `UINT32_MAX` within WAL bounds | Zero legal where owner permits | Exact checkpoint sequence construction before installation | N/A | Checkpoint remains uninstalled | §§4.3.2.5, 13.5–13.7 |
| Modification/writeback/FPI/root generations | F | Concrete width implementation-defined; no value may repeat while stale comparison is possible | Any next value that could compare equal to stale state is terminal until quiescence | Process-local frame/tree/checkpoint publication identity | Reseed only after complete owner-domain quiescence | Reject mutation/publication/checkpoint | §4.3.2.5 and owning runtime sections |
| Pin/reference counters | F | Concrete width implementation-defined; maximum is the largest checked nonoverflowing count | No wrapping value is legal | Process-local acquisition count | Decrement normally; failed increment changes nothing | Fail acquisition | §4.3.2.5 and §7.5–7.12 |
| `ReadEpochManager.current_epoch` | F | uint64; `UINT64_MAX` may be current | `0` invalid; no retirement/increment at max | Process-local retirement epoch | Reinitialize only after approved restart/quiescence | Disable further RID retirement/reuse requiring a new epoch | §§4.3.2.5, 14.6 |

`RID`, fixed built-in `TypeId` codes, and `creation_epoch` are intentionally absent:
§4.3.2 defines no independent advancing allocator or exhaustion state for them.

### Universal checked-boundary procedure

Every domain above uses a table-driven boundary fixture. For a current value `x`,
requested increment/allocation `k`, and architecture-valid last result `M`, run:

| Case | Fixture | Required observation |
|---|---|---|
| B-1 | A valid state whose operation yields a value immediately below `M` | Operation succeeds, publishes exactly that value, and advances only the owning state. |
| B0 | A valid state whose operation yields exactly `M` | Last legal operation succeeds without truncation, wrap, or premature exhaustion. |
| B+1 | The resulting terminal state followed by one more operation | Canonical exhaustion/capacity result occurs before persistence, namespace/page/WAL publication, or caller-visible identity return. |
| B+N | A representable malformed state above a semantic bound | Owning decoder/validator rejects it with its corruption/invalid-state result; it is not clamped, wrapped, or treated as valid. Use N/A when no above-bound value is encodable. |

The fixture records these semantic observation points without prescribing helper names:

```text
decoded current value
complete checked candidate/add/multiply/align result
field-domain and sentinel validation
durable high-water or reservation write
required durability barrier
runtime publication / identifier return / semantic publication
```

Pause independently at every applicable point. No test may infer ordering merely from
the final error. A rejection test asserts that later points were not reached and that
all externally observable bytes, metadata, namespace state, valid WAL end, frame state,
and caller-visible identities remain at the owning pre-operation oracle.

For wrap-prone domains, construct the maximum valid predecessor and prove checked
arithmetic rejects the successor before native increment. Assert that zero, an all-ones
sentinel, a prior value, or a low wrapped value is never returned, persisted, formatted,
or published. For narrowed fields, perform candidate construction in the semantic
mathematical domain, test the field range before encoding, and include values that would
truncate to an apparently valid low identifier or length. This is a semantic oracle and
does not prescribe an implementation intermediate type.

For every reserved value, test its immediate legal predecessor and attempted successor.
The reserved value is never returned as ordinary data and is never recorded as an
allocated value when the architecture permits it only as invalid/terminal state.
CommandId zero and leaf level zero receive positive controls because they are legal.

Maximum-construction cases independently exercise every addition, multiplication,
alignment, segment-tail, array-count, and headroom subtraction in the complete builder:

```text
largest legal complete object
one-unit larger object
largest operands whose complete result fits
first addition or multiplication that does not fit
alignment input immediately below, at, and above its legal rounded result
headroom exactly sufficient and one byte insufficient
```

The largest legal construction succeeds; the first illegal construction fails before
narrowing/reservation/publication; no intermediate arithmetic overflows; and no partial,
truncated, or split representation is substituted.

### Synthetic fixture and instrumentation rules

Huge domains are reached with deterministic semantic fixtures, never brute-force loops.
A test may initialize an allocator/counter through a test-only state injector, canonical
encoder, or controlled persistent fixture immediately below the target boundary. A valid
persistent fixture must preserve all unrelated invariants:

- supported format and schema versions;
- exact widths, little-endian encoding, and reserved-zero fields;
- checksums and complete control-slot/page/WAL framing;
- cross-field bounds, identities, alignment, file length, and catalog references;
- the owner-specific high-water, checkpoint, publication, and recovery relationships.

Arbitrary byte patching that could not represent a valid history is reserved for the B+N
corruption case and changes exactly one selected invariant. Boundary-success and
exhaustion fixtures remain canonical valid state. When the owner derives state from WAL,
file length, or control generations, construct all contributing authorities coherently
rather than patching only the cached value.

Test hooks expose barriers at the semantic points listed above and at allocator
serialization/ownership acquisition. They may report observations but must not change the
production ordering being tested. Separate-process crash cases terminate without running
destructors or clean shutdown, then reopen through the ordinary lifecycle and recovery
path as required by the Crash Injection Framework.

### Persistent high-water and crash procedure

FileId, the shared catalog-object allocator, and TxnId block reservation use this common
matrix. Control generation uses the same durability observations without an identity
return. PageNo and WAL specialize the matrix below.

| Boundary | Durable allocator state | Caller observed ID? | Crash/failure injection | Expected recovered next state | Reuse? | Oracle |
|---|---|---:|---|---|---|---|
| Before candidate/range validation | Old valid high-water | No | Stop before candidate construction | Old authority; retry starts from its candidate | Only if the owner has not consumed it | Selected durable high-water and absence of external reference |
| After validation, before durable write | Old valid high-water | No | Fail/stop before persistence begins | Old authority; no external durable reference names candidate | Owner-specific retry may use candidate | Durable high-water, namespace/catalog state, and caller result |
| During control write, before successful required sync | Old slot is the last acknowledged durable authority; a complete newer slot may or may not survive a crash | No | Inject short/torn write, sync failure, process stop, and machine crash separately | Recovery selects the highest complete valid surviving authority: old permits owner-specific retry; new consumes the candidate/range | Only when recovered authority proves no durable advance | Independent slot validation/selection plus recovered high-water; never guess from the failed call |
| After durable high-water, before return/publication | New high-water | No | Stop after successful durability barrier | New authority; next issue/allocation is beyond the lost candidate or reserved suffix | No for no-reuse IDs | Durable generation/high-water and consumed-gap assertion |
| After return or semantic publication | New high-water | Yes | Crash after caller/publication observation | New authority beyond candidate; every durable reference remains valid | No for no-reuse IDs | High-water, published identity/reference, and reopen result |
| Terminal high-water, failed next request | Terminal high-water | No new ID | Repeat failure, restart, and reopen | Same terminal authority; next request fails in the same semantic category | No reset or backward rebuild | Terminal high-water plus stable error and no persistence/publication attempt |

At each row, compare the recovered control generation/high-water, next issued value,
catalog/file presence, WAL evidence, and public result. Object absence never authorizes
moving an authoritative durable high-water backward. Drop, abort, status reclamation,
freezing, orphan cleanup, and restart do not reclaim FileId, TableId, IndexId,
ConstraintId, or TxnId space.

For an allocator that permits concurrent requests, initialize exactly `N` remaining legal
values and release more than `N` requesters through one barrier. Exactly `N` distinct legal
values may publish; every loser receives the canonical exhaustion result; no sentinel,
duplicate, wrapped value, or lost high-water update appears. Repeat with a pause after
durable range/high-water reservation and crash before all reserved values are returned.
After restart, every possibly reserved no-reuse value remains consumed. Per-transaction
CommandId and nonconcurrent structural builders do not acquire artificial concurrency
requirements.

### Durable identifier specializations

#### TxnId terminal block

Recompute and assert the exact §4.3.2.1 constants in the test oracle:

```text
BLOCK_SIZE                  = 1,048,576
MAX_RESERVED_TXN_ID_END     = 18,446,744,073,708,503,042
MAX_ALLOCATABLE_TXN_ID      = 18,446,744,073,708,503,041
```

Construct the exact full block below the terminal block and the exact terminal legal
block. Prove every issued TxnId lies in its reserved half-open range, the durable exclusive
end precedes issue, and the terminal block ends exactly at
`MAX_RESERVED_TXN_ID_END`. The next exact block request returns
`TXN_ID_EXHAUSTED`; it does not shrink the block to consume the remaining uint64 suffix,
wrap to zero/frozen values, or reuse status-reclaimed IDs. Crash after durable block
reservation but before issue may lose any suffix permanently; reopen/recovery starts from
the durable/recovered authority and preserves the terminal exhausted condition.

#### FileId and shared catalog-object IDs

For FileId and the one shared TableId/IndexId/ConstraintId allocator, run B-1/B0/B+1 at
`MAX-1`, verify durable `candidate+1` before return, and crash after that durability but
before object/catalog publication. The candidate remains consumed even if only a private
or orphan file used it. For the shared catalog allocator, alternate TableId, IndexId, and
ConstraintId requests at the boundary and prove they consume one sequence rather than
three independent maxima. Drop/abort/retire old objects, reopen, and prove neither allocator
reuses an old value or repairs a gap. Repeated terminal requests retain
`FILE_ID_EXHAUSTED` or `ID_EXHAUSTED` rather than becoming corruption.

#### CommandId, ColumnId, SchemaVer, and control generation

Admit a statement at `CommandId{UINT32_MAX}` and allow it to complete. The next ordinary
statement in that transaction is rejected with `COMMAND_ID_EXHAUSTED` before parsing,
binding, or execution publication; the transaction remains eligible to COMMIT existing
work or ROLLBACK, and unrelated transactions begin with legal CommandId zero. Connect this
case to Statement Failure and Transaction-State Tests and the existing CommandId procedure.

Construct complete schemas at the largest owner-encodable ColumnId/SchemaVer boundaries.
The first unencodable add-column/schema evolution is rejected before any catalog row or
descriptor publication, old schema versions remain readable, and no historical ColumnId
or SchemaVer wraps or is reused. Where v1 exposes no such ALTER, exercise the schema
builder/validator boundary without claiming SQL support or inventing a named error.

For control generation, install a valid selected generation `UINT64_MAX`, request each
operation that requires a control update, and prove failure occurs before constructing a
wrapped generation zero or publishing any dependent checkpoint/high-water/reclamation
state. A malformed persisted generation zero follows control-slot validation/fallback,
not ordinary numeric exhaustion.

### Page and slot structural specialization

For PageNo, assert the §4.3.2.3 arithmetic independently:

```text
MAX_FILE_PAGE_COUNT  = 1,125,899,906,842,623
MAX_FILE_PAGE_NO     = 1,125,899,906,842,622
MAX_ALIGNED_FILE_LEN = 9,223,372,036,854,767,616
```

Construct a canonical file/allocator authority immediately below the final page. The last
PageNo and exact aligned file length succeed. The next extension computes PageNo,
`new_page_count`, byte offset, and `new_page_count * 8192` with checked arithmetic and
returns `PAGE_NUMBER_EXHAUSTED` before `ftruncate`, page write, frame publication, or WAL
reservation. Reject `UINT64_MAX` and representable PageNos above the signed positional-I/O
cap through their owning validator; never narrow a product or publish a partial extension.

Separately inject filesystem/quota failure for a numerically valid extension. It returns
`RESOURCE_FULL`/I/O failure, not `PAGE_NUMBER_EXHAUSTED`. The PAGE_INIT procedure remains
the detailed owner for known extension failure: exact serialized tail restoration permits
the same PageNo to be retried only because it never published. Crash/reopen reconciles the
physical tail and WAL authority before ordinary access. Once a PageNo publishes, reuse is
legal only through the owning heap/B+ free/reclamation protocol and its safety predicates.

For SlotId, use a canonical §4.13.3/§5.3 page fixture with `slot_count=1018` to prove that
zero-based SlotId `1017` is representable and `1018` is not encodable as a valid in-range
slot. `UINT16_MAX` remains invalid. A page unable to fit a new directory entry or tuple
returns `NO_SPACE`; it does not report global ID exhaustion. Verify `DEAD` is nonreusable
immediately before the §14.6 grace predicate, then verify canonical `DEAD -> UNUSED`
free-list publication and SlotId reuse only after that predicate. The Vacuum and
Reclamation Tests remain the detailed grace/reuse owner.

Heap/FSM/B+ structural counts use the same B-1/B0/B+1 construction. Full page/node
capacity triggers `NO_SPACE`, another FSM page, split, or rebalance as specified; it never
truncates a count or manufactures a numeric identity exhaustion result.

### WAL position, maximum record, and terminal headroom specialization

WAL tests use the universal arithmetic oracle but not the generic durable-ID crash matrix.
For every reservation, independently compute the 48-byte header, exact payload,
`Align8`, current-segment tail/PAD, segment index, record start, exclusive end, and all
outstanding terminal credits. Cover placement immediately before, exactly at, and across a
segment boundary, including final segment `2^38-1`; index `2^38` is rejected and never
formatted through truncation.

Construct exact maximum records:

```text
MAX_WAL_TOTAL_LENGTH = 67,108,864
MAX_WAL_PAYLOAD      = 67,108,816
TERMINAL_CREDIT      = 33,128
                     = 2 * 16,520 + 88
```

The maximum record succeeds only where its complete aligned span fits one segment and
preserves credits. One larger total/payload, an overflowing Align8, or a segment-crossing
record returns `WAL_RECORD_TOO_LARGE`/`ENCODED_LENGTH_EXCEEDED` before LSN reservation.
An atomic BTREE_MTR is not split to evade the bound.

Near the terminal WAL end, run these cases independently:

- headroom exactly admits a transaction's first WAL-backed mutation and retains its
  credit through terminal closure;
- one byte less rejects that first mutation before publication;
- unrelated ordinary/checkpoint/maintenance work cannot consume outstanding credit;
- a credited writer can append the bounded status image/terminal sequence and COMMIT or
  ABORT even after ordinary work is refused;
- a read-only transaction uses the no-terminal-WAL path;
- speculative credit is released only after known no-append rollback with no prior
  persistent transaction WAL;
- append uncertainty does not guess that credit is free and follows the existing
  `NONCONTINUABLE` rule;
- the exclusive end may reach mathematical `2^64`, but no record start, page LSN,
  `durable_lsn`, or filename wraps to zero/segment zero.

Crash at reservation, valid append, WAL write, sync, and runtime-publication points and use
the existing Non-Crash WAL/MTR Failure Injection, COMMIT/ABORT, and Recovery Property Tests
as physical oracles. Recovery reconstructs the valid prefix and loser terminal obligations
without losing protected headroom. Reopen succeeds when required recovery/checkpoint WAL
fits; otherwise it returns the architecture-defined exhaustion/open failure and never
publishes READY. Controlled close cannot report success when its required final checkpoint
cannot be encoded, while already durable transactions remain durable. Repeated terminal
ordinary requests that lack admissible numeric position produce
`WAL_POSITION_EXHAUSTED`, not disk-full or corruption; an already-credited terminal
closure retains the stricter accounting/invariant outcome from §4.3.2.4.

### Encoded, structural, generation, and epoch specializations

For heap tuples, B+ keys/entries, default blobs, persisted scalars, statistics scopes and
chunks, and checkpoint counts, construct the exact maximum complete owner object and a
one-unit larger request. Exercise every nested count-times-width, header-plus-payload,
alignment, chunk-count, and total-length computation before encoding a narrower field.
Failure leaves no partial page/MTR/catalog generation/checkpoint publication. Statistics
failure invalidates the complete generation and uses the ordinary fallback; checkpoint
failure leaves it uninstalled; an oversized tuple returns `ROW_TOO_LARGE`.

For B+ height, construct a structurally valid maximum-height tree descriptor and request
root growth. A level/height successor beyond the representable contract fails before new
root allocation, provisional page mutation, or MTR publication, and the existing tree
remains valid. Root contraction remains legal and is not identifier reuse.

For finite runtime generation/token implementations, retain a stale observer/completion at
the terminal boundary and prove the operation requiring a fresh token is rejected before
bytes or runtime ownership change. Then quiesce the complete frame/tree/checkpoint domain,
prove no stale token or in-progress completion survives, atomically reseed every compared
token, and prove the next operation cannot compare equal to a pre-quiescence handle. Pin
and reference counters reject overflowing acquisition without changing the count. No
persistence-across-restart assertion is applied to these process-local domains.

For `ReadEpochManager.current_epoch`, prove zero is invalid and `UINT64_MAX` may be current,
but retirement at that value assigns no retirement epoch and performs no increment. RID
retirement/reuse requiring a fresh epoch remains disabled while unrelated operations may
continue. Reinitialization occurs only after process restart or complete manager quiescence
proves no old reader survives; recovered persistent DEAD slots are re-enqueued under the
new process epoch before reuse. Barriers around reader registration/release prove the exact
§14.6 grace predicate immediately before and after it becomes true.

### Error, statement, and lifecycle oracles

| Observed category | Fixture condition | Required classification |
|---|---|---|
| Numeric exhaustion | No legal successor/position exists in an otherwise valid state | Domain-specific `FILE_ID_EXHAUSTED`, `ID_EXHAUSTED`, `TXN_ID_EXHAUSTED`, `COMMAND_ID_EXHAUSTED`, `PAGE_NUMBER_EXHAUSTED`, `WAL_POSITION_EXHAUSTED`, or owning encoded-length result |
| `RESOURCE_FULL` / `NO_SPACE` | Candidate is numerically representable, but local page capacity or disk/quota/memory/platform resource is unavailable | Resource/local-capacity result; never global ID exhaustion |
| I/O failure | Candidate is valid and a required read/write fails | Structured I/O result under the owning protocol; not numeric exhaustion |
| Durability failure | Candidate is valid, write may exist, but required sync/publication durability fails | Owning known/uncertain durability outcome; no false success or guessed high-water |
| Corruption | Persisted supported-format state violates a semantic/range/cross-field invariant | Owning `CORRUPT_*`/invalid-generation outcome; never clamp or reinterpret as ordinary terminal exhaustion |
| Unsupported format | Stable discriminator identifies a newer unsupported owning format | Owning `UNSUPPORTED_*_FORMAT`; do not decode through v1 or call it exhaustion |

Where both numeric invalidity and an injectable I/O failure are possible, test separately.
A numerically invalid candidate fails before forbidden persistence I/O. A valid candidate
whose persistence fails reports I/O/durability failure. Do not impose an error precedence
where the architecture delegates it, but always prove that no published candidate follows
either failure.

For statement-scoped exhaustion, execute the applicable §39.1.3 row both before and after
the first transaction-owned published write. Record the operation result, statement state,
transaction state, already durable facts, and later legal operations. Exhaustion does not
universally force `MUST_ABORT` and never changes a durable COMMIT.

| Exhausted domain | Forbidden new operation | Operations that remain legal | Open/shutdown/recovery oracle |
|---|---|---|---|
| FileId/shared catalog ID | New object/DDL requiring the ID | Existing-object reads and operations not allocating that namespace; transaction outcome follows §39.1 | Reopen preserves terminal high-water; deletion does not reopen space |
| TxnId | New transaction admission | Already admitted owners continue under their normal rules; instance operations not requiring a new TxnId | Reopen preserves reserved terminal authority; no anonymous transaction mode |
| CommandId | Later ordinary statement in that transaction | COMMIT or ROLLBACK existing work; unrelated transactions unaffected | Process-local transaction ends normally; no database-wide terminal state |
| PageNo in one file | Append requiring a new PageNo | Reads and owner-authorized reuse of already published free pages | Reconcile tail on recovery; other files/domains remain usable |
| WAL position/headroom | New WAL-backed work that cannot preserve credits | Credited terminal closure; no-WAL read-only transaction; already durable outcomes | Open fails only if mandatory recovery/checkpoint WAL cannot fit; controlled close reports failure if final checkpoint cannot fit |
| Structural/encoded bound | The oversized page/node/record/schema/statistics/checkpoint operation | Unrelated operations and owner-defined split/fallback paths | Malformed persisted above-bound state follows owner validation; ordinary capacity failure is not instance corruption |
| Runtime generation/token | Mutation/acquisition requiring a fresh nonrepeating token | Unrelated operations; quiesce/reseed path where authorized | Process-local; restart persistence is N/A |
| Read epoch | New retirement/reuse requiring epoch advance | Operations not requiring physical RID reuse | Restart/quiescent reinitialization and DEAD re-enqueue are required before reuse |

Numeric exhaustion alone does not enter `NONCONTINUABLE`. WAL append uncertainty,
inability to honor already-owned terminal credit, or independent I/O/publication failures
retain their explicit §12.12/§39.1 escalation semantics. Tests assert database health and
client outcome separately from durable transaction outcome.

### Domain coverage matrix

`U` means the universal B-1/B0/B+1 procedure owns the case; `S` means the named
specialization adds the domain oracle; `P` means the persistent high-water matrix applies;
`V` means owner validation supplies B+N; and N/A means the architecture defines no such
dimension. Every inventory row appears separately. Every `COMPLETE` status is a required
coverage mapping, not a run result.

| Domain | B-1 | B0 | B+1 | B+N | Persistence | Crash gap | Restart | Reuse | Concurrency | Error category | Lifecycle outcome | Verification owner | Status |
|---|---:|---:|---:|---:|---|---|---|---|---|---|---|---|---|
| FileId | U | U | U | V | control high-water | P | terminal persists | forbidden | serialized/concurrent requests | `FILE_ID_EXHAUSTED` | only new file/object allocation fails | Durable identifier specializations | COMPLETE |
| TableId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations | COMPLETE |
| IndexId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations | COMPLETE |
| ConstraintId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations | COMPLETE |
| TxnId | U | S | S | V | durable block end | P, lost suffix | terminal persists | forbidden | block reservation contention | `TXN_ID_EXHAUSTED` | new transaction admission fails | TxnId terminal block | COMPLETE |
| CommandId | U | S | S | N/A above uint32 | transaction-local | N/A | transaction-local | forbidden within transaction | N/A | `COMMAND_ID_EXHAUSTED` | later statement fails; COMMIT/ROLLBACK legal | CommandId specialization | COMPLETE |
| ColumnId | U | U | U | V | catalog publication | no row publication | schemas survive reopen | forbidden in history | catalog owner | owning DDL rejection | schema construction fails | Schema specialization | COMPLETE |
| SchemaVer | U | U | U | V | catalog publication | no row publication | old schemas readable | forbidden in history | catalog owner | owning DDL rejection | schema evolution fails | Schema specialization | COMPLETE |
| Control-slot generation | U | S | S | V zero/malformed | alternating control slot | surviving-slot oracle | selected max remains | forbidden | serialized control update | requiring operation fails | dependent control update cannot proceed | Control specialization | COMPLETE |
| PageNo | U | S | S | V | file length + WAL publication | unpublished-tail exception | reconcile before READY | predicate-gated | serialized append | `PAGE_NUMBER_EXHAUSTED` | append in one file fails | Page specialization | COMPLETE |
| `published_page_count` | U | S | S | V | reconstructed exclusive bound | exact tail rollback | reconstruct/reconcile | unpublished retreat only | serialized append | PageNo/I/O boundary | cannot outrun file/WAL authority | Page specialization | COMPLETE |
| SlotId | U | S | S | V | heap page/WAL | no global gap | page recovery | grace-gated | page-latched | `NO_SPACE`, not ID exhaustion | page-local insertion/reuse result | Slot specialization | COMPLETE |
| WAL Lsn/exclusive end | U | S | S | V | valid WAL prefix/durable LSN | specialized append oracle | terminal end reconstructed | forbidden | WAL reservation | `WAL_POSITION_EXHAUSTED` | ordinary WAL work gated; credited closure preserved | WAL specialization | COMPLETE |
| WAL segment index | U | S | S | V | WAL namespace | segment publication oracle | inventory/recovery | forbidden | WAL reservation | `WAL_POSITION_EXHAUSTED` | no later segment position | WAL specialization | COMPLETE |
| Statistics chunk count/index | U | U | U | V | generation-atomic catalog rows | failed generation unpublished | fallback after reopen | not within generation | ANALYZE owner | generation invalid/failure | ordinary statistics fallback | Encoded specialization | COMPLETE |
| B+ tree height/node level | U | S | S | V | root MTR publication | no provisional root | existing tree valid | contraction only | tree protocol | root-growth failure | insertion fails before new root publication | B+ specialization | COMPLETE |
| Heap `slot_count` | U | U | U | V | heap page/WAL | no identity gap | page recovery | slots grace-gated | page-latched | `NO_SPACE` | page-local capacity path | Structural specialization | COMPLETE |
| Heap `tuple_length` | U | U | U | V | page/MTR publication | no partial tuple | page recovery | N/A | owner serialization | `ROW_TOO_LARGE` | tuple operation fails | Encoded specialization | COMPLETE |
| FSM `entry_count` | U | U | U | V | FSM page publication | no partial count | rebuild/reopen | N/A | owner serialization | next-page/PageNo result | owning FSM growth path | Free-space map verification — initialized prefix and suffix | COMPLETE |
| B+ node `slot_count` | U | U | U | V | MTR publication | no partial node | tree recovery | structural | tree protocol | split/rebalance result | existing tree remains valid | Encoded specialization | COMPLETE |
| B+ key/entry lengths | U | U | U | V | MTR publication | no partial entry | tree recovery | N/A | tree protocol | key/construction failure | index operation fails before publication | Encoded specialization | COMPLETE |
| WAL total/payload length | U | S | S | V | no reservation on failure | no WAL gap | valid prefix unchanged | N/A | WAL reservation | record/encoded-length failure | WAL operation fails before reservation | WAL specialization | COMPLETE |
| Default-blob `total_length` | U | U | U | V | catalog publication | no catalog row publication | catalog remains readable | N/A | catalog owner | oversized-default rejection | DDL/default construction fails | Encoded specialization | COMPLETE |
| `PersistedScalarV1.payload_length` | U | U | U | V | enclosing owner | no enclosing publication | owner-specific reopen | N/A | owner serialization | enclosing-builder result | default/statistics owner decides | Encoded specialization | COMPLETE |
| Statistics scope/manifest counts/lengths | U | U | U | V | generation-atomic publication | failed generation unpublished | fallback after reopen | N/A | ANALYZE owner | generation invalid/failure | statistics fallback | Encoded specialization | COMPLETE |
| Checkpoint DATA indexes/counts/totals | U | U | U | V | checkpoint sequence | remains uninstalled | prior installed checkpoint | N/A | checkpoint owner | checkpoint construction failure | close/open follows checkpoint requirement | Encoded specialization | COMPLETE |
| Runtime generation tokens | U | S | S | N/A persisted | process-local | N/A | N/A persistence | quiescence-gated | owning domain | operation rejection | unrelated runtime work may continue | Generation specialization | COMPLETE |
| Pin/reference counters | U | U | U | N/A persisted | process-local | N/A | N/A persistence | decrement only | owning domain | acquisition failure | existing owners remain valid | Generation specialization | COMPLETE |
| Read epoch | U | S | S | V zero | process-local | N/A durable gap | restart/quiescence reinit | restart/quiescence-gated | epoch mutex | retirement/reuse disabled | unrelated operations continue | Epoch specialization | COMPLETE |

### Architecture-obligation coverage map

The following inventory maps every cross-domain obligation to one procedure owner. Domain
rows marked N/A in the matrix are excluded only where the architecture does not define that
dimension.

| # | Architecture obligation and source | Verification owner | Status |
|---:|---|---|---|
| 1 | Checked candidate construction — §4.3.2 steps 1–3 | Universal checked-boundary procedure — observation sequence | COMPLETE |
| 2 | No wrap/increment-then-test — §§4.3.2, 4.3.2.6 item 1 | Universal checked-boundary procedure — wrap-prone paragraph | COMPLETE |
| 3 | No truncation/narrowing — §§4.3.2, 4.3.2.4–4.3.2.6 | Universal checked-boundary procedure — narrowing and maximum-construction paragraphs | COMPLETE |
| 4 | No sentinel allocation — §4.3.2 domain inventory | Universal checked-boundary procedure — reserved-value paragraph; domain specializations | COMPLETE |
| 5 | Exact maximum legal result — §§4.3.2.1–4.3.2.5 | Universal checked-boundary procedure — B0; domain specializations | COMPLETE |
| 6 | First illegal result rejected — §4.3.2 checked-next contract | Universal checked-boundary procedure — B+1 | COMPLETE |
| 7 | Rejection before publication — §§4.3.2, 4.3.2.6 | Universal checked-boundary procedure — observation points and rejection oracle | COMPLETE |
| 8 | Durable high-water before return — §§4.3.2.1, 9.3, 13.2.5–13.2.6 | Persistent high-water and crash procedure | COMPLETE |
| 9 | Consumed-gap behavior — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — after-durability/before-return row | COMPLETE |
| 10 | Restart persistence — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — restart and terminal rows | COMPLETE |
| 11 | No no-reuse ID reclamation — §§4.3.2.1, 9.2–9.3 | Durable identifier specializations — TxnId and FileId/shared-object cases | COMPLETE |
| 12 | Allowed-reuse preconditions — §§4.3.2.3, 4.3.2.5, 8.18, 14.6 | Page and slot structural specialization; Encoded, structural, generation, and epoch specializations | COMPLETE |
| 13 | Maximum-size construction — §§4.3.2.4–4.3.2.6 | Universal checked-boundary procedure; WAL and encoded specializations | COMPLETE |
| 14 | Arithmetic-overflow rejection — §§4.3.2, 4.3.2.6 items 3–4 | Universal checked-boundary procedure — arithmetic observation points and B+1 | COMPLETE |
| 15 | WAL terminal headroom — §4.3.2.4 | WAL position, maximum record, and terminal headroom specialization | COMPLETE |
| 16 | Error-domain distinction — §4.3.2.6 | Error, statement, and lifecycle oracles — category table and paired fixtures | COMPLETE |
| 17 | Statement consequence — §§4.3.2.2, 4.3.2.6, 39.1 | Error, statement, and lifecycle oracles; CommandId specialization | COMPLETE |
| 18 | Transaction consequence — §§4.3.2.4, 4.3.2.6, 39.1 | Error, statement, and lifecycle oracles; WAL specialization | COMPLETE |
| 19 | Instance/lifecycle consequence — §4.3.2.6 and Chapter 3 | Error, statement, and lifecycle oracles — legal-operations table | COMPLETE |
| 20 | Legal operations after exhaustion — §4.3.2.6 | Error, statement, and lifecycle oracles — legal-operations positive cases | COMPLETE |
| 21 | Open/recovery after exhaustion — §§4.3.2.4, 4.3.2.6 | Persistent high-water and crash procedure; WAL open/shutdown cases | COMPLETE |
| 22 | Corrupted above-bound state — §4.3.2 steps 1–3 and §§4.13–4.14 | Synthetic fixture and instrumentation rules — B+N corruption fixture | COMPLETE |
| 23 | Concurrent terminal allocation where applicable — §§9.3, 13.2.4–13.2.6 | Persistent high-water and crash procedure — concurrent terminal paragraph | COMPLETE |
| 24 | Crash during durable allocation where applicable — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — crash matrix and concurrent crash case | COMPLETE |

Coverage inventory: `24 COMPLETE`, `0 PARTIAL`, `0 MISSING`, and
`0 CONTRADICTORY`.

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

### Buffer management verification

This section is the detailed procedural owner for the I/O and BufferPool contract in
[`ARCHITECTURE.md`](ARCHITECTURE.md) Chapter 7. It specializes the generic checked-arithmetic,
page-format, owner-validation, PAGE_INIT, WAL/MTR, recovery, reclamation, and lifecycle
procedures rather than redefining them. The principal architecture owners are §§4.3.2,
4.7–4.14, 7.2–7.13, 12.10, 12.12, 12.16–12.17, 13.13–13.14, 14.5–14.12, 14.17, 39.1,
and 41.1.

Use pools substantially smaller than the working set, including a three-frame pool, while
retaining direct construction of synthetic runtime states for checked counter and failure
boundaries. Every fixture is canonical except for the single dimension being tested: page
size, format version, FileId, PageNo, FileKind, PageType, object owner, reserved fields,
checksum, and published bound are valid unless that field is the injected fault.

#### Deterministic harness and observability

Concurrency verification uses hooks, barriers, injected outcomes, and coordinated threads or
processes. It MUST NOT use sleeps, repeated racing until a failure appears, or elapsed time as
the ordering oracle. The harness exposes abstract semantic events rather than requiring
production function names:

```text
load intent installed
frame bound LOADING
victim candidate observed
victim reservation attempted/completed
pin increment attempted/completed
latch acquisition attempted/completed/canceled
guard returned/released
stable image copied with PageId/generation/page_lsn
WAL durability requested/completed/failed
page pwrite started/completed/short/failed
page-file fdatasync started/completed/failed
dirty reconciliation started/completed
mapping marked non-pinnable/removed
frame reset/new PageId bound
PAGE_INIT publication-authorizing WAL appended
owning published bound advanced
frame published RESIDENT
file gate changed ACTIVE/RETIRING/CLOSED
retirement drain completed
```

At each point the harness can inspect the logical frame state, bound PageId, page-table load
intent and ordinary mapping, pin count, latch ownership/waiters, dirty flag and generation,
I/O/no-flush/victim/drain reservations, replacement reference state, `page_lsn`, `rec_lsn`,
DPT membership, WAL durable LSN, registered-file state, and persistent page-file completion.
Test-only poisoning or generation assertions may detect stale borrowed references; production
APIs need not expose one particular instrumentation mechanism.

#### Frame lifecycle and publication

Exercise every row below from a controlled pre-state and pause at its publication point.
Assert the stated observable condition, race one forbidden operation at that point, and inject
each applicable failure before and after publication. This tests the matrix rather than using
it as a second architecture definition.

| State/transition case | Bound/mapped and caller-visible state | Deterministic operation and oracle |
|---|---|---|
| `FREE + NONE` | no PageId, no mapping, no pin, clean, not visible | bind only through a sole existing/new-page intent; stale metadata is absent |
| `FREE -> LOADING + READ_IN_PROGRESS` | PageId and in-progress entry, no public pin/guard | pause before read; another same-page fetch joins and no ordinary caller sees bytes |
| `FREE -> LOADING + NONE` for new page | private unpublished PageId, creator claim only | no ordinary fetch may join merely from the intended PageNo |
| existing-page `LOADING -> RESIDENT` | same mapping becomes pinnable only after full validation | publication assigns one pin per uncancelled claim and wakes waiters |
| new-page `LOADING -> RESIDENT` | owning bound and frame usability publish together | PAGE_INIT/MTR, canonical bytes, owner validation, bound, and creator pin are complete |
| failed `LOADING -> FREE` | failed intent removed; no ordinary mapping/pin | frame reset, all joiners receive one captured error, clean retry may install a new intent |
| `RESIDENT + NONE` | validated PageId, pinnable; dirty allowed | hit pin and persistent mutation follow their separate atomic publication points |
| `RESIDENT -> RESIDENT + WRITEBACK_IN_PROGRESS` | mapping remains usable; writeback copy is private | one writeback reservation; pins/readers/writers follow copied-writeback rules |
| resident writeback completion | same identity; dirty clears only on stable matching generation | newer generation or any failure leaves the frame dirty and coherent |
| `RESIDENT -> EVICTING` | old mapping present but non-pinnable; pin count zero | final eligibility recheck/reservation is the linearization point |
| clean `EVICTING -> FREE/LOADING` | old mapping removed before reset/rebind | no write; complete reset precedes any new PageId |
| dirty `EVICTING -> WRITEBACK_IN_PROGRESS` | old identity retained; no ordinary access | WAL-gated stable writeback precedes removal/rebind |
| dirty eviction success | old mapping removed; reset then FREE/new LOADING | exact write and file synchronization completed first |
| dirty eviction failure `-> RESIDENT` | old mapping pinnable again, old identity and dirty metadata retained | requesting load/joiners receive the same failure; frame is not rebound |
| retirement drain | RETIRING gate blocks new claims; existing ownership drains | mapping/reset/close/unlink cannot pass the drain or semantic discard authority |
| shutdown drain | quiescing rejects new acquisitions | existing guards/I/O drain and required dirty flush completes before teardown |

The corresponding state-condition matrix is exercised directly. For every row, inspect each
column before allowing its listed outgoing transition; a mismatch is a failed invariant, not
an alternate transition.

| State/condition | PageId bound? | Page-table entry | Caller pinnable? | Dirty permitted? | I/O state | Eviction eligible? | Ordinary visibility | Legal outgoing transitions |
|---|---:|---|---:|---:|---|---:|---|---|
| `FREE` | no | none | no | no | `NONE` | allocation candidate, not eviction victim | none | existing/new-page `LOADING` |
| existing-page `LOADING` | yes | in progress | no | no | `READ_IN_PROGRESS` | no | join coordination only | validated `RESIDENT`; failed `FREE` |
| private new-page `LOADING` | yes | private in progress | no | only protected unpublished initialization state | `NONE` | no | creator coordination only | co-published `RESIDENT`; failed `FREE`/noncontinuable disposition |
| `RESIDENT + NONE` | yes | usable | yes if file active and count representable | clean or dirty | `NONE` | only if every §7.9 predicate holds | guarded callers | hit/mutation; resident writeback; `EVICTING`; retirement drain |
| `RESIDENT + WRITEBACK_IN_PROGRESS` | yes | usable | yes | dirty until matching stable reconciliation | `WRITEBACK_IN_PROGRESS` | no | guarded callers; private copy hidden | `RESIDENT + NONE` clean, newer-dirty, or failed-dirty |
| clean `EVICTING + NONE` | yes | non-pinnable old mapping | no | no | `NONE` | already reserved | none | `FREE`; reset then new `LOADING` |
| dirty `EVICTING + NONE` | yes | non-pinnable old mapping | no | yes | `NONE` | already reserved | none | eviction `WRITEBACK_IN_PROGRESS` |
| `EVICTING + WRITEBACK_IN_PROGRESS` | yes | non-pinnable old mapping | no | yes until success | `WRITEBACK_IN_PROGRESS` | already reserved | none | `FREE`/new `LOADING` after success; restored `RESIDENT` after failure |
| file-gated `RETIRING` frame | yes until drain/reset | existing non-new-pinnable mapping | no new pin | according to required persistence/discard authority | `NONE` or draining I/O | no | pre-gate guards only | drained/reset, then file `CLOSED`; failure remains RETIRING |
| file `CLOSED` | no | none | no | no | none | no | none | no ordinary frame transition for the old FileId |

Negative transition tests reject direct `FREE ->` caller-visible `RESIDENT`, ordinary guard
return from `LOADING`, pinned or otherwise ineligible `RESIDENT -> EVICTING`, dirty eviction
rebind before stable writeback, and old-to-new PageId binding without mapping removal and
complete reset. `FREE` has only `NONE`; `LOADING`, `RESIDENT`, and `EVICTING` accept only the
I/O combinations in §7.6.1. No test-only state setter may make an illegal combination look
like a successful public operation.

Distinguish persistent-page publication from frame publication. An existing PageNo may be
persistently published while its frame remains private `LOADING`; a private new-page frame
may exist while its PageNo is unpublished. A load intent is visible to joiners but is not an
ordinary pinnable mapping. Tests observe these dimensions separately.

#### Same-page fetch, victim races, and failure cleanup

Coordinate at least two fetches for one valid nonresident PageId. Pause after the sole
`LOAD_INTENT` installation and again after the frame becomes `LOADING`. Assert exactly one
logical loader/read, one eventual ordinary resident identity, all joiners attached to that
load, and exactly one pin for every successfully returned guard. Duplicate mutable ordinary
copies are forbidden regardless of page-table container.

Inject raw read failure, short transfer, bad checksum, wrong owner, and unsupported format
while joiners wait. The loader removes/closes the in-progress mapping, resets the frame,
publishes no guard/pin, and wakes every registered joiner with the corresponding captured
error. Correct the underlying fixture and assert that a later independent fetch installs a
new intent and succeeds; failed pages are not poison-cached.

If a permitted bounded/cancellable waiting interface exists, cancel one joiner before
publication and another immediately after claim-to-pin assignment. The first withdraws only
its pending claim; the second releases only its assigned pin. Neither cancels the loader,
removes a successful mapping, changes another caller's pin, nor leaks waiter state. This case
is conditional and does not require such an optional interface.

For fetch versus eviction, prepare an eligible zero-pin resident frame and pause before the
final pin/victim reservation. If fetch wins, its pin makes victim reservation fail or retry.
If eviction wins, the mapping becomes non-pinnable and fetch cannot acquire the old frame; it
waits/retries according to §7.6.3. A pin acquired after reassignment begins is forbidden.

#### Pin, latch, guard, and borrowed-reference lifecycle

Use synthetic checked-counter states immediately below and at the pin maximum. The legal
increment succeeds, an increment beyond the maximum fails without changing the count or
PageId, and no wrap to zero occurs. Pair this with one-release-per-owner cases: normal
destruction, explicit early release followed by destruction, transferred ownership,
canceled latch acquisition after pinning, and attempted double release. Assert no underflow,
no duplicate unpin, and no leaked pin.

For canceled latch acquisition, pause after the pin and fail/cancel before latch ownership.
Exactly that pin is released, no latch or guard survives, and the mapped frame remains valid.
For guard transfer, the destination owns the one pin/latch claim and the source is inert;
destination release performs latch release before checked unpin exactly once. During early
release, pause between those events and prove eviction cannot win while the guard still
depends on protected bytes.

Exercise shared read guards, exclusive write guards, and transaction-lock separation.
Concurrent readers may proceed; writers exclude byte readers/writers; a transaction-level
wait retains no page latch. Pinning alone does not permit unsynchronized byte access and a
latch alone does not preserve frame identity.

To verify borrowed-reference lifetime, obtain a raw/typed view of page A, release its guard,
evict/reassign the frame to B, and attempt stale use through a debug poison, frame token, or
equivalent test facility. The stale view must not be accepted as a valid reference to B.
This is a contract-violation detector, not a required production handle design.

#### Copied stable flush, WAL, and dirty reconciliation

For a dirty resident frame, reserve one writeback, copy a stable 8192-byte image under the
read latch, and record copied PageId, `modification_generation`, and `page_lsn`. Release the
latch, finalize the private checksum, satisfy WAL-before-data, perform an exact complete page
write and required `fdatasync`, then pause at reconciliation. If identity and generation are
unchanged, dirty/`rec_lsn`/DPT state may publish clean atomically; flush preserves the resident
PageId and mapping.

Race a newer mutation after copying generation G and before or after G's physical write.
Generation G may become durable, but completion cannot clear generation G+1, its dirty flag,
or its dirty-interval recovery metadata. A later explicit flush can persist G+1. Also inject
a stale completion token for page A against a later identity B; it must not alter B. Normal
reservations should prevent this reassignment, and the injected case proves the reconciliation
defense.

Keep a page pinned while flushing it. Assert no eviction, a coherent private image,
WAL-before-data, preserved resident identity, and dirty clearing only for a matching
generation. Pinning is not an immutability claim.

When explicit flush requests overlap, accept join-and-recheck, serialized repeat, or an
equivalent mechanism. The current-contents request returns success only when the then-current
generation is stably clean. If optional background writeback exists, it uses the same copied
generation/WAL rules and may skip or requeue busy/no-flush frames; no background worker is a
required feature and it cannot make a pinned frame evictable.

Specialize the generic Disk tests with zero-byte failure, short write, and exact 8192-byte
write outcomes. Only the complete transfer may proceed toward stable completion. After a
successful full `pwrite`, inject `fdatasync` failure: dirty and all recovery metadata remain,
success/eviction is not published, and the frame retains the operation-appropriate identity.
Where writes are batched, no covered frame becomes clean or evictable before the covering
`fdatasync` succeeds.

For WAL-backed pages, inject `flush_through(copied_page_lsn)` failure and prove data-page
`pwrite` never begins. Dirty remains and the caller receives `WAL_DURABILITY_FAILURE`.
Corrupt the resident page after checksum validation only through an explicit test fault; the
write path must checksum the stable private image, not changing resident bytes. The existing
§12.10/§12.12 tests own no-flush and DPT/checkpoint publication races; add a BufferPool
observer proving checkpoint sees either the complete dirty transition or a safely durable
clean page, never an intermediate state.

#### CLOCK, eviction, failure restoration, and reset

Table-test every victim-eligibility predicate: `RESIDENT`, zero pins, I/O `NONE`, no latch
owner/waiter, no no-flush barrier, no competing victim/drain reservation, and a successful
final reservation. For CLOCK reference behavior, a successful hit/load sets the use bit; an
eligible referenced frame receives the defined second chance; an eligible unreferenced frame
may be reserved. `FREE`, `LOADING`, `EVICTING`, pinned, and reserved frames do not become
victims through the reference-bit rule.

Construct separate pools in which every frame is excluded by pins, I/O, latch ownership or
waiters, no-flush state, and victim/drain reservation. Count logical CLOCK visits and
eligibility decisions rather than time. One complete unsuccessful ordinary pass returns
`NO_REPLACEABLE_FRAME`, changes no pins/states, steals no frame, and does not wait
indefinitely. A conditional bounded/cancellable convenience path cannot weaken these
predicates.

For clean eviction, assert non-pinnable reservation, old-mapping removal, complete reset,
then optional new binding, with no data write. For dirty eviction, assert WAL durability,
exact write, `fdatasync`, mapping removal/reset, then new binding. Independently inject WAL,
write, short-write, and sync failure: restore the old pinnable `RESIDENT` mapping, preserve
dirty/`rec_lsn`/generation/DPT state, release the reservation, fail the requesting load and
its joiners, and permit a later retry. No alternative victim hides that failure.

After successful eviction or failed loading cleanup, inspect all architecture-significant
state: PageId, pins, dirty flag, cached/trusted `page_lsn` state where separately retained,
`rec_lsn`/DPT/FPI metadata, I/O and no-flush state, latch/waiter state, replacement bit,
victim/drain claims, and generation/identity tokens. Nothing owned by the old PageId may
influence the new binding. Dirty/writeback generation follows §7.6.1; inject
an old asynchronous completion and prove it cannot match a later residency. Apply the
runtime-generation terminal/quiesce procedure under “Encoded, structural, generation, and
epoch specializations” rather than brute-force wrapping.

Pair `NO_REPLACEABLE_FRAME` against disk `RESOURCE_FULL`, `PAGE_NUMBER_EXHAUSTED`,
`WAL_POSITION_EXHAUSTED`, identifier exhaustion, heap `NO_SPACE`, corruption, retirement,
and `BUFFERPOOL_QUIESCING`. Each fixture reaches only its owning domain and no result is
reported as generic “buffer full.”

#### Validation before residency

Parameterize ordinary load validation over the managed families applicable to the v1
page registry:

| Family/context | Owning specialization |
|---|---|
| `HEAP_DATA`, including catalog-relation heaps | §§4.13.3, 5.3–5.13, and immutable relation descriptor |
| `FSM_DATA` | §§4.13.6 and Chapter 6 FSM verification |
| `BTREE_INTERNAL`, `BTREE_LEAF`, `BTREE_FREE` | §§4.13.4–4.13.5 and B+ verification |
| `TXN_STATUS` | §§9.12, 12.10.5, and recovery/status ownership |
| `CATALOG_DATA` bootstrap page | §16.9 specialized bootstrap/open path; ordinary publication only where that owner registers it |
| `SUPERBLOCK`/page zero | specialized open/identity path below, not assumed to be an ordinary data-page fetch |

For each applicable ordinary family, start with canonical bytes and owner context. Observe
registered-owner lookup, published-bound check, exact read, family/version dispatch,
checksum, common PageId identity, FileKind/PageType compatibility, nonfetching L1/L2 owner
validation, `LOADING -> RESIDENT`, then guard return. Fault each earlier step separately and
assert no later ordinary publication, no guard/pin, complete load cleanup, one error for all
joiners, and successful retry after correcting the fixture.

Cases include bad checksum, wrong PageNo, wrong registered FileId/owner, wrong FileKind,
wrong PageType, and family-local structural corruption. A plausible `page_lsn` behind a bad
checksum must not be trusted for WAL/recovery decisions. For every applicable family pair a
malformed recognized-v1 fixture with a recognizable future version: the first yields its
canonical corruption result; the second yields `UNSUPPORTED_PAGE_FORMAT` or
`UNSUPPORTED_FILE_FORMAT`. Dispatch must not parse future bytes as v1.

#### BufferPool new-page publication

Pause after append intent reservation and private `LOADING` frame binding. The selected
PageNo remains outside the owning published bound, has no ordinary mapping or guard, and an
ordinary concurrent fetch cannot attach merely by guessing it. The private frame is not
evidence of persistent publication.

Inject every known pre-publication-authorizing-WAL failure through the generic PAGE_INIT
procedure. Assert private mapping/frame cleanup, unchanged published bound, no escaping
guard/reference, serialized tail restoration, and deterministic PageNo reuse where §4.11.1.1
permits it. After the publication-authorizing record validly appends, inject failure before
complete bound/frame publication and require retained completion/retry or
`STORAGE_NONCONTINUABLE`; do not roll back and reuse an authorized PageNo.

On success, observe one publication boundary that has canonical initialized bytes,
PAGE_INIT/MTR and frame recovery metadata, owner validation, owning `published_page_count`,
ordinary `RESIDENT` mapping, and exactly one creator pin. Before it, neither scans nor fetches
can use the page; after it, the bound includes the PageNo and the mapping is usable. Repeat
with a concurrent ordinary fetch paused on each side.

Specialize PAGE_INIT crash tests by asserting all frame-table, pin, latch, waiter, and CLOCK
state disappears on process death. Recovery uses only WAL/file publication state and admits
the page ordinarily only after reconstruction and canonical validation.

#### File retirement and shutdown specialization

Expose `ACTIVE -> RETIRING -> CLOSED` at the registered-file/BufferPool boundary. Race a new
load/pin with the RETIRING gate: a claim linearized before the gate drains as existing
ownership; a gate winner rejects the new operation with `FILE_RETIRED_OR_CLOSING`. No claim
is admitted after the gate.

Hold read/write guards across the transition. They remain valid until release, while no new
guard is admitted and close/unlink waits. Race copied writeback or victim I/O with retirement;
the I/O completes or fails before handle close, and all reservations drain before mapping
removal. A descriptor or fd must never be closed while in-flight I/O still owns it.

Test dirty discard in two matched fixtures. A proven semantic drop/retirement owner may
authorize discard after drain; RETIRING alone and generic eviction do not. Without that
authority, required dirty state is preserved/failed rather than discarded. Inject drain or
writeback failure and assert the file remains RETIRING/nonordinary, no unlink or false CLOSED
publication occurs, and higher-level failure handling receives the error. Successful CLOSED
state has no mapping, frame, pin, guard, or I/O for the FileId; nonreuse prevents an old
PageId from binding a new file.

The existing “Shutdown, draining, and failure injection” procedure owns the complete
database lifecycle. Its Chapter-7 specialization asserts BufferPool quiescing rejects new
fetch/new-page work, existing guards/I/O drain, required dirty pages use WAL-before-data,
required flush failure prevents clean shutdown, and BufferPool helpers/ownership end before
the WAL service.

#### Bounded raw-I/O exceptions

Inventory every permitted BufferPool bypass and instrument capability use so an unlisted
ordinary page operation cannot issue raw managed-page I/O:

| Exception owner | Permitted object and reason | Validation/termination oracle |
|---|---|---|
| WAL manager | WAL segments use the WAL record/durability protocol | never interpreted as BufferPool pages; exception ends at WalManager boundary |
| database lifecycle/recovery owner | `database.control` dual-slot lifecycle state | control validation precedes use; no PageGuard/CLOCK semantics are applied |
| registered-file/open owner | initial FileSuperblock/page-zero identity establishment | bounded raw read is validated before registration; it is not a permanent page-zero escape |
| bootstrap/create owner | private initial object/page construction before ordinary publication | canonical validation/publication completes before ordinary BufferPool use |
| recovery owner | torn-page private reconstruction | untrusted bytes remain private; reconstructed page validates before READY/residency |
| namespace owner/DiskManager | create/rename/unlink and directory synchronization | no page interpretation; exception ends at durable §4.7 namespace boundary |

Record every direct DiskManager page read/write in ordinary HEAP/FSM/BTREE/catalog/status
operations and require it to originate from BufferPool or one named private owner. The
existence of WAL/control/bootstrap/recovery/namespace paths is not a generic escape hatch.
Verify page-zero access according to its actual phase: identity-establishing access may be
specialized before registration, while registered ordinary data-page access returns to the
canonical BufferPool boundary.

For recovery, start from a torn page requiring a complete image. Ordinary load rejects it;
the private recovery owner may reconstruct without ordinary publication, but the resulting
page passes normal checksum, identity, format, and owner validation before READY or guard
return. Bootstrap follows the same private-construction-to-canonical-publication shape and
must never be described or tested as an implementation-stage absence of BufferPool.

#### Chapter 7 failure-classification matrix

| Fixture/outcome | Required result and BufferPool oracle |
|---|---|
| invalid or unpublished PageId | `FILE_OR_PAGE_NOT_FOUND`; no data-page publication |
| RETIRING/CLOSED owner | `FILE_RETIRED_OR_CLOSING`; no new claim |
| failed/short transfer | `RAW_IO_FAILURE`; partial bytes never become resident/stably clean |
| malformed recognized v1 / wrong owner | applicable corruption result, including `CORRUPT_PAGE`; no guard |
| recognizable unsupported page/file version | `UNSUPPORTED_PAGE_FORMAT` / `UNSUPPORTED_FILE_FORMAT` |
| WAL durability failure | `WAL_DURABILITY_FAILURE`; no dependent data-page write |
| page write or `fdatasync` failure | `RAW_IO_FAILURE`; dirty and identity retained |
| complete CLOCK pass without victim | `NO_REPLACEABLE_FRAME`; no waiting or stolen frame |
| quiescing BufferPool | `BUFFERPOOL_QUIESCING`; existing claims only drain |
| uncertain append/restoration/publication | `STORAGE_NONCONTINUABLE`; no ordinary retry/publication |

The higher statement/transaction/lifecycle consequence remains owned by §39.1 and the
existing failure procedures. Tests preserve distinctions rather than inventing BufferPool
aliases.

#### Chapter 7 concurrency matrix

| Race | Deterministic barrier | Legal survivor(s) | Forbidden outcome | Architecture owner |
|---|---|---|---|---|
| same-page fetch/fetch | sole load intent | one loader and one resident identity; joiners share outcome | duplicate usable frames | §§7.5, 7.6.3, 7.8 |
| fetch/victim | final pin/reservation | pin wins or mapping becomes non-pinnable | pin after reassignment begins | §§7.6.3, 7.12.1 |
| flush/new mutation | stable copy G | G clean if unchanged; G+1 remains dirty | old completion clears G+1 | §§7.10.2–7.10.3 |
| guard release/eviction | latch release before unpin | eviction only after complete release | eviction while guard uses bytes | §§7.7–7.9 |
| dirty eviction/WAL | WAL durability request | WAL durable then page write, or preserved dirty failure | data write before WAL | §§7.10–7.12.1 |
| retirement/fetch | RETIRING gate | pre-gate claim drains or post-gate rejection | new claim after gate | §7.12.5 |
| retirement/guard | existing guard held | guard finishes; close waits | invalidated live view/use-after-close | §7.12.5 |
| retirement/writeback | I/O reservation | I/O completes/fails before close | close/unlink during I/O | §7.12.5 |
| shutdown/fetch | quiescing publication | old work drains; new work rejected | post-quiesce acquisition | §§7.12.6, 3.3.6 |
| reassignment/stale view | complete frame reset | new identity only after old ownership ends | stale view accepted as new page | §§7.7.2, 7.12.2 |
| checkpoint/clean-to-dirty | DPT transition gate | complete old or complete new DPT state | missing/intermediate rec_lsn/FPI state | §§7.10.1, 7.10.5 |

#### Buffer management domain/case matrix

| Family | Deterministic fixture | Barrier/fault | Expected oracle | Architecture reference | Status |
|---|---|---|---|---|---|
| Frame transitions | one frame per legal/illegal edge | transition publication | exact legal survivor; illegal edge absent | §§7.6.1–7.6.2 | COMPLETE |
| Same-page miss | 2+ fetchers, nonresident page | load intent/read/validation | one loader/copy; shared result | §§7.6.3, 7.8 | COMPLETE |
| Fetch-victim | eligible zero-pin resident | final recheck | pin or victim, never both | §§7.6.3, 7.12.1 | COMPLETE |
| Pin arithmetic | synthetic max/zero states | increment/release | checked failure; no wrap/underflow | §§7.7, 7.9 | COMPLETE |
| Guard lifecycle | read/write/transferred guards | acquire/cancel/release | one claim; latch before pin release | §§7.7–7.7.2 | COMPLETE |
| Stable flush | dirty resident G | copied image/reconciliation | stable matching G may clean | §§7.10.2–7.10.3 | COMPLETE |
| Flush-mutation | mutate to G+1 during G I/O | before/after write | G+1 remains dirty | §7.10.3 | COMPLETE |
| Stable completion | injected write/sync/WAL outcomes | pwrite/fdatasync/WAL | no premature clean/write | §§7.10.3, 7.11 | COMPLETE |
| Eviction/reset | clean/dirty victim | reservation/write/reset | no old-state leak; failure restores | §§7.12.1–7.12.2 | COMPLETE |
| CLOCK exhaustion | each ineligibility predicate | one logical pass | exact `NO_REPLACEABLE_FRAME` | §§7.9, 7.12–7.12.3 | COMPLETE |
| Family validation | each managed page family | each validation stage | no invalid RESIDENT/guard | §§7.6.4, 4.13–4.14 | COMPLETE |
| New-page publication | private appended page | WAL/bound/frame publication | no partial ordinary visibility | §7.12.4 | COMPLETE |
| Retirement | active file with loads/pins/I/O | RETIRING/drain/CLOSED | no post-gate claim or premature unlink | §7.12.5 | COMPLETE |
| Shutdown | active BufferPool | quiesce/drain/flush | no false clean close | §§7.12.6, 3.3.6 | COMPLETE |
| Raw-I/O exceptions | one fixture per named owner | bypass capability | only bounded owner uses bypass | §§7.3–7.5, 13.14 | COMPLETE |


#### Chapter 7 architecture-obligation coverage map

Domains are: A layer/ownership, B frame state, C identity/page table, D pin, E latch,
F guard, G dirty state, H flush, I WAL/stable completion, J fetch, K PAGE_INIT/new page,
L replacement/eviction, M validation/format, N retirement, O shutdown, P raw-I/O exception,
Q concurrency, R failure, and S other architecture-defined storage behavior. Each row has
one primary procedure owner; cross-referenced generic procedures remain part of that owner.

| # | Domain | Atomic obligation and architecture owner | Verification owner and methodology | Status |
|---:|:---:|---|---|---|
| 1 | A | DiskManager owns managed raw positional I/O/handles — §7.3 | Bounded raw-I/O exceptions; capability trace | COMPLETE |
| 2 | A | Raw layer does not parse formats or publish database objects — §§7.3–7.3.1 | Bounded exceptions; negative interface/property case | COMPLETE |
| 3 | A | FileId is logical identity; fd is private and registration-lifetime-bound — §7.3.1 | Retirement/CLOSED and exception fixtures | COMPLETE |
| 4 | A | Higher storage owner initializes/validates superblock and object identity — §7.3.1 | Page-zero/open exception plus generic format tests | COMPLETE |
| 5 | A | Ordinary managed page views use BufferPool lifetime and perform no I/O — §7.5 | Capability trace across managed families | COMPLETE |
| 6 | A | BufferPool remains format agnostic and invokes owner validation — §§7.5, 7.6.4 | Cross-family parameterized load | COMPLETE |
| 7 | A | WAL durability owner is distinct; BufferPool enforces dependency — §§7.3, 7.11 | WAL failure barrier and call trace | COMPLETE |
| 8 | S | Positional I/O does not use shared offsets — §7.4.1 | Existing Disk tests; concurrent offset fixture | COMPLETE |
| 9 | S | Reads require an exact page and expose no partial result — §7.4.2 | Disk short-read injection plus LOADING oracle | COMPLETE |
| 10 | S | Writes require complete-page transfer — §7.4.2 | Stable-completion short/exact-write matrix | COMPLETE |
| 11 | S | EINTR retry and non-retried-close semantics — §7.4.3 | Existing Disk injectable syscall procedure | COMPLETE |
| 12 | S | No implicit extension/sparse write; aligned checked file bounds — §§7.4.4–7.4.6 | Disk/PageNo exhaustion procedures | COMPLETE |
| 13 | S | I/O errors retain file/page/operation/errno context — §7.4.7 | Injected I/O result inspection | COMPLETE |
| 14 | I | `fdatasync` owns file-byte stability; namespace sync remains separate — §7.4.8 | Stable completion and namespace lifecycle cross-reference | COMPLETE |
| 15 | B | Every frame maps to one legal ownership/I/O combination — §7.6.1 | Frame lifecycle matrix enumeration | COMPLETE |
| 16 | B | `FREE -> LOADING+READ` binds sole existing-page intent — §7.6.2 | Existing-page transition barrier | COMPLETE |
| 17 | B | `FREE -> LOADING+NONE` binds sole private new-page intent — §7.6.2 | New-page private-frame barrier | COMPLETE |
| 18 | B | Existing-page `LOADING -> RESIDENT` only after validation — §7.6.2 | Transition/validation publication fixture | COMPLETE |
| 19 | B | New-page `LOADING -> RESIDENT` coordinates bound and PAGE_INIT — §7.6.2 | New-page co-publication fixture | COMPLETE |
| 20 | B | Failed `LOADING -> FREE` removes intent and resets frame — §7.6.2 | Loader-failure matrix | COMPLETE |
| 21 | B | Resident hit atomically adds one checked pin — §7.6.2 | Hit/pin linearization fixture | COMPLETE |
| 22 | B | Persistent mutation publishes generation/dirty/recovery metadata atomically — §§7.6.2, 7.10.1 | Existing WAL/MTR publication observer plus BufferPool state capture | COMPLETE |
| 23 | B | Resident copied writeback reserves one orthogonal I/O state — §7.6.2 | Stable-flush base fixture | COMPLETE |
| 24 | B | Matching-generation stable completion may publish clean — §7.6.2 | Dirty reconciliation barrier | COMPLETE |
| 25 | B | Newer generation survives old writeback completion dirty — §7.6.2 | G/G+1 race | COMPLETE |
| 26 | B | Resident writeback failure returns coherent dirty RESIDENT — §7.6.2 | WAL/write/sync fault matrix | COMPLETE |
| 27 | B | Final victim reservation publishes `RESIDENT -> EVICTING` — §7.6.2 | Fetch/victim barrier | COMPLETE |
| 28 | B | Clean eviction may end in FREE after mapping removal/reset — §7.6.2 | Clean-eviction fixture | COMPLETE |
| 29 | B | Clean eviction may transfer reservation to new LOADING only after reset — §7.6.2 | Clean-rebind fixture | COMPLETE |
| 30 | B | Dirty victim enters eviction writeback without ordinary access — §7.6.2 | Dirty-eviction fixture | COMPLETE |
| 31 | B | Successful dirty eviction may end FREE after stable completion — §7.6.2 | Dirty-success/no-request fixture | COMPLETE |
| 32 | B | Successful dirty eviction may bind new LOADING only after reset — §7.6.2 | Dirty-success/request fixture | COMPLETE |
| 33 | B | Failed dirty eviction restores old pinnable RESIDENT — §§7.6.2, 7.10.4 | Eviction failure-restoration matrix | COMPLETE |
| 34 | B | Retirement drain removes mapping only after claims/I/O/latches drain — §§7.6.2, 7.12.5 | Retirement state harness | COMPLETE |
| 35 | B | Illegal direct visibility, pinned eviction, and pre-reset rebind transitions are absent — §§7.6.1–7.6.2 | Negative transition table | COMPLETE |
| 36 | C | BufferPool identity is PageId, not frame/fd/path/pointer — §§7.5–7.6 | Cross-frame/reopen identity fixtures | COMPLETE |
| 37 | C | At most one active load and usable frame exists per PageId — §§7.5, 7.8 | Same-page multi-fetch barrier | COMPLETE |
| 38 | C | `LOAD_INTENT` is installed before victim selection/I/O — §7.8 | Intent observability barrier | COMPLETE |
| 39 | C | Joiners register claims without public pins/latches — §7.8 | Joiner-state inspection | COMPLETE |
| 40 | C | Successful load atomically publishes RESIDENT and claim pins — §7.8 | Publication/wakeup barrier | COMPLETE |
| 41 | C | Canceled joiner removes/releases only its own claim — §7.8 | Conditional cancellation cases | COMPLETE |
| 42 | C | Failed load closes mapping, shares one error, and wakes joiners — §7.8 | Loader failure with 2+ joiners | COMPLETE |
| 43 | C | Corrected later fetch may retry; no poison cache — §7.8 | Failure-then-repair sequence | COMPLETE |
| 44 | C | Mapping never names A while frame bytes/identity are B — §§7.8, 7.12.2 | Reset/rebind observer | COMPLETE |
| 45 | J | Normal fetch rejects invalid/unpublished PageId before ordinary load — §7.6.3 | Published-bound fixture | COMPLETE |
| 46 | J | Resident-hit pin is the fetch linearization point — §7.6.3 | Hit/victim two-survivor race | COMPLETE |
| 47 | J | First miss/waiter linearize at one validated publication — §7.6.3 | Same-page success fixture | COMPLETE |
| 48 | Q | Fetch versus victim reservation has only pin-wins or eviction-wins survivor — §7.6.3 | Final recheck barrier | COMPLETE |
| 49 | D | Pin preserves frame residency/identity, not byte exclusion — §§7.7, 7.7.1 | Pin-without-latch negative case | COMPLETE |
| 50 | E | Read/shared and write/exclusive latches protect page bytes — §7.7.1 | Concurrent guard matrix | COMPLETE |
| 51 | E | Transaction lock waits retain no page latch — §7.7.1 | Lock/latch barrier cross-reference | COMPLETE |
| 52 | F | One guard owns exactly one pin and appropriate latch — §7.7 | Guard lifecycle counter trace | COMPLETE |
| 53 | F | Guard releases latch before checked unpin — §7.7 | Paused early-release observer | COMPLETE |
| 54 | F | Canceled/failed latch acquisition releases one pin and no guard — §7.7 | Post-pin cancellation fault | COMPLETE |
| 55 | D | Pin increment is checked and cannot wrap — §7.7 | Synthetic max boundary | COMPLETE |
| 56 | D | Release cannot underflow or double-decrement — §7.7 | Double/destructor-after-release cases | COMPLETE |
| 57 | F | Guard transfer leaves exactly one owner and inert source — §7.7.2 | Transfer/destruction trace | COMPLETE |
| 58 | F | Early release equals destruction and is ownership-idempotent — §7.7.2 | Early release plus destructor | COMPLETE |
| 59 | F | Borrowed page/view lifetime ends at guard release — §7.7.2 | Poison/token stale-use test | COMPLETE |
| 60 | C | Stale view cannot observe a reassigned frame as valid B — §§7.7.2, 7.12.2 | A-release/evict/B-bind test | COMPLETE |
| 61 | D | Pinned frame may flush but cannot evict — §§7.9, 7.10.2 | Pinned-flush and victim-negative pair | COMPLETE |
| 62 | L | Zero pins are necessary but all §7.9 predicates are required — §7.9 | Eligibility table | COMPLETE |
| 63 | G | Dirty means current published generation not known stable — §7.10 | State/result inspection | COMPLETE |
| 64 | G | Dirty does not imply commit/WAL durability/visibility; COMMIT is NO-FORCE — §7.10 | Commit-with-dirty-resident integration case | COMPLETE |
| 65 | G | Persistent mutation publication obeys WAL/no-flush/metadata order — §7.10.1 | Existing §12.12 procedure plus frame observer | COMPLETE |
| 66 | G | Published mutation advances generation exactly once; provisional rollback does not — §§7.6.1, 7.10.1 | Clean/dirty rollback state comparison | COMPLETE |
| 67 | S | Generation cannot repeat while stale completion exists; terminal handling quiesces — §§7.6.1, 4.3.2.5 | Runtime generation specialization | COMPLETE |
| 68 | H | Copied writeback captures stable bytes/PageId/generation/page_lsn under read latch — §7.10.2 | Stable-copy barrier | COMPLETE |
| 69 | H | Durable checksum is finalized on private stable image — §§7.10.2–7.10.3 | Changing-resident-copy checksum case | COMPLETE |
| 70 | I | WAL durable LSN reaches copied page_lsn before page write — §7.11 | WAL/data ordering trace | COMPLETE |
| 71 | I | WAL durability failure prevents data-page pwrite — §7.11 | `flush_through` fault | COMPLETE |
| 72 | I | Stable transfer requires exact 8192-byte pwrite — §7.10.3 | Short/exact-write fault matrix | COMPLETE |
| 73 | I | Stable completion additionally requires owning-file `fdatasync` — §7.10.3 | Full-write/sync-failure case | COMPLETE |
| 74 | I | Batched sync cannot clean/evict a covered frame before sync — §7.10.3 | Multi-frame batch barrier | COMPLETE |
| 75 | G | Matching PageId/generation reconciliation atomically clears dirty/DPT/rec_lsn — §§7.10.3, 7.10.5 | Reconciliation publication observer | COMPLETE |
| 76 | G | Newer mutation keeps dirty and recovery metadata after old completion — §7.10.3 | G/G+1 race | COMPLETE |
| 77 | C | Old-page completion cannot mutate later frame identity — §§7.10.3, 7.12.2 | Injected stale completion token | COMPLETE |
| 78 | H | Explicit overlapping/current-content flush joins/repeats to current clean generation — §7.10.2 | Two explicit callers | COMPLETE |
| 79 | H | Optional background writeback skips/requeues safely and remains optional — §§7.9, 7.10.2 | Conditional background case | COMPLETE |
| 80 | H | Pinned flush preserves mapping/identity and coherent copy — §§7.9, 7.10.2 | Held-pin stable flush | COMPLETE |
| 81 | R | Flush failure preserves mapping, dirty/recovery metadata, and retryability — §7.10.4 | WAL/write/sync failure table | COMPLETE |
| 82 | Q | DPT/checkpoint capture observes complete dirty or safely clean state — §7.10.5 | Transition-gate observer cross-reference | COMPLETE |
| 83 | L | CLOCK sets/tests reference state only for defined resident accesses — §7.12 | Reference-bit table | COMPLETE |
| 84 | L | Referenced eligible frame receives second chance — §7.12 | Ordered small-pool traversal | COMPLETE |
| 85 | L | Final victim reservation atomically rechecks all eligibility — §7.12.1 | Candidate-observed/final-recheck race | COMPLETE |
| 86 | L | One complete unsuccessful ordinary pass returns `NO_REPLACEABLE_FRAME` — §7.12.3 | Logical visit-count fixture | COMPLETE |
| 87 | L | Ordinary no-victim operation neither waits indefinitely nor steals ineligible frame — §7.12.3 | Barrier/progress-state oracle | COMPLETE |
| 88 | L | Optional bounded/cancellable wait path cannot weaken eligibility — §7.12.3 | Conditional interface contract test | COMPLETE |
| 89 | L | Clean eviction removes mapping and resets before reuse — §§7.12.1–7.12.2 | Clean-eviction ordering trace | COMPLETE |
| 90 | L | Dirty eviction completes WAL/write/sync before removal/reuse — §§7.11, 7.12.1 | Dirty-success trace | COMPLETE |
| 91 | R | Dirty-eviction failure restores old mapping/dirty state — §§7.10.4, 7.12.1 | Four-fault restoration matrix | COMPLETE |
| 92 | R | Failed dirty eviction fails requesting load and its joiners — §7.12.1 | Captured-error waiter case | COMPLETE |
| 93 | C | Complete reset removes every old identity/metadata influence — §7.12.2 | Post-reset field inspection | COMPLETE |
| 94 | S | Buffer exhaustion remains distinct from disk/numeric/page-space outcomes — §§7.12.3, 7.12.7 | Paired classification fixtures | COMPLETE |
| 95 | M | Registered owner and published bound validate before load — §§7.6.3–7.6.4 | Validation-order barrier | COMPLETE |
| 96 | M | Exact transfer completes before byte trust — §§7.4.2, 7.6.4 | Short-read failure | COMPLETE |
| 97 | M | Family/version dispatch precedes family-specific v1 parsing — §§7.6.4, 4.14 | Current/future paired fixtures | COMPLETE |
| 98 | M | Checksum/common validation precedes page_lsn trust — §§7.6.4, 4.12–4.13 | Bad-checksum/plausible-LSN fixture | COMPLETE |
| 99 | M | PageId/FileKind/PageType identity validates before publication — §7.6.4 | One-fault-per-field matrix | COMPLETE |
| 100 | M | Complete nonfetching L1/applicable L2 owner validation precedes publication — §7.6.4 | Cross-family owner fixtures | COMPLETE |
| 101 | M | Owner validator cannot recursively fetch or weaken missing context — §7.6.4 | Validator dependency/capability observer | COMPLETE |
| 102 | M | Any validation failure cleans LOADING and publishes no guard — §7.6.4 | Failure/joiner/retry fixture | COMPLETE |
| 103 | M | Malformed v1 and unsupported future format remain distinct — §§7.12.7, 4.14 | Paired corruption/unsupported table | COMPLETE |
| 104 | M | HEAP/FSM/BTREE/catalog/status families use their registered validators — §§4.13, 7.6.4 | Parameterized family harness | COMPLETE |
| 105 | K | New page begins private LOADING and cannot be ordinarily fetched — §7.12.4 | Private-frame/concurrent-fetch barrier | COMPLETE |
| 106 | K | Pre-WAL new-page failure resets frame/bound and restores tail — §7.12.4 | PAGE_INIT failure specialization | COMPLETE |
| 107 | K | Post-authorizing-WAL failure finishes publication or becomes noncontinuable — §7.12.4 | Post-append fault boundary | COMPLETE |
| 108 | K | PAGE_INIT bytes, owner bound, RESIDENT mapping, and creator pin co-publish — §7.12.4 | One publication observer | COMPLETE |
| 109 | Q | Concurrent fetch cannot access private unpublished new page — §7.12.4 | Before/after publication race | COMPLETE |
| 110 | K | Crash discards runtime frame state; recovery uses persistent WAL/file state — §§7.6.5, 7.12.4 | PAGE_INIT crash specialization | COMPLETE |
| 111 | N | ACTIVE/RETIRING gate linearizes new-fetch admission — §7.12.5 | Retirement/fetch race | COMPLETE |
| 112 | N | Existing guards remain valid and drain before close/unlink — §7.12.5 | Held-guard retirement fixture | COMPLETE |
| 113 | N | Existing writeback/I/O drains before fd close/unlink — §7.12.5 | Retirement/writeback barrier | COMPLETE |
| 114 | N | Dirty discard requires proven higher semantic retirement authority — §7.12.5 | Authorized/unauthorized paired fixtures | COMPLETE |
| 115 | R | Retirement failure preserves RETIRING state and prevents unlink/CLOSED — §7.12.5 | Drain/writeback fault injection | COMPLETE |
| 116 | N | CLOSED has no frame/mapping/pin/I/O; old FileId cannot rebind — §7.12.5 | Post-close inventory/nonreuse check | COMPLETE |
| 117 | O | Quiescing rejects new work, drains claims/I/O, flushes required dirty pages, and tears down before WAL — §7.12.6 | Existing shutdown procedure plus BufferPool observer | COMPLETE |
| 118 | P | WAL segments remain under specialized WalManager I/O/durability — §§7.3, 7.5 | WAL exception capability trace | COMPLETE |
| 119 | P | `database.control` remains under specialized lifecycle/recovery I/O — §§7.3, 7.5 | Control exception fixture | COMPLETE |
| 120 | P | Initial FileSuperblock/page-zero access is bounded to identity establishment — §§7.3.1, 7.5 | Open/registration boundary fixture | COMPLETE |
| 121 | P | Bootstrap/create private access ends at canonical publication — §§7.3.1, 7.5 | Bootstrap private-publication fixture | COMPLETE |
| 122 | P | Recovery-private torn-page access ends at validated reconstruction — §§7.6.4, 13.14 | Torn-page recovery fixture | COMPLETE |
| 123 | P | Namespace create/rename/unlink bypasses page interpretation only — §§7.3–7.4.8 | Namespace capability/durability trace | COMPLETE |
| 124 | P | Ordinary managed pages have no generic raw-I/O escape hatch — §7.5 | Direct-I/O capability trace | COMPLETE |
| 125 | C | Persistent page publication and frame publication are independent dimensions — §§7.6.3, 7.12.4 | Existing-page/new-page paired observer | COMPLETE |
| 126 | R | BufferPool errors preserve exact storage/format/exhaustion/lifecycle distinctions — §7.12.7 | Failure-classification matrix | COMPLETE |
| 127 | S | Frame/page-table/pin/latch/CLOCK state is process-local and not recovered — §7.6.5 | Crash specialization and post-reopen inventory | COMPLETE |

Coverage inventory: `127 COMPLETE`, `0 PARTIAL`, `0 MISSING`, and
`0 CONTRADICTORY`.

### Heap tests

- thousands of tuples,
- many pages,
- scan after reopen,
- deletion-marker behavior under MVCC visibility,
- FSM stale-entry repair.

### Free-space map verification

This verification family owns the deterministic procedures for the FSM contract in
`ARCHITECTURE.md` Chapter 6. It specializes the generic storage-format, owner-validation,
PAGE_INIT, WAL/MTR, recovery, and reclamation procedures rather than duplicating them. The
relevant generic owners are Architecture §§4.10–4.13, 7.3–7.12, 12.9, 12.12, 12.17,
13.11–13.19, 14.5–14.12, 14.16, 15.2–15.4, 39.1, and 41.1.

#### Deterministic fixtures and observability

Build FSM fixtures with explicit little-endian codecs. Unless a case intentionally targets
structural or owner corruption, each fixture has the correct v1 FileSuperblock, FileKind,
FileId, TableId, common header, `FSM_DATA` PageType, PageNo, 48-byte header, reserved-zero
fields, initialized prefix, zero suffix, paired published bounds, and checksum. This keeps a
category or mapping case from accidentally becoming a format-corruption case.

The FSM-specific codec fixture checks exact offsets and widths independently:

| Byte range | Verification oracle |
|---|---|
| `0..31` | canonical common page header |
| `32..39` | little-endian `first_heap_page_no` |
| `40..41` | little-endian `entry_count` |
| `42..43` | zero FSM `reserved16` |
| `44..47` | zero FSM `reserved32` |
| `48..8191` | exactly 8144 one-byte category entries |

Assert a 16-byte FSM-specific extension, a 48-byte total header, an 8192-byte complete page,
and no gap or overlap. The generic FileSuperblock procedure supplies the separate 72-byte
`FileKind::FSM` superblock checks.

The test oracle computes category and mapping results independently of the production helper
under test. A static expected vector or a test-side checked-arithmetic implementation is
acceptable; calling the production helper to calculate its own expected result is not.

Deterministic hooks or barriers expose these abstract events without prescribing production
function or class names:

```text
heap mutation published
heap page published
guarded heap free_bytes captured
FSM mapping computed
candidate category read
heap owner validation accepted
heap insertion recheck begins
FSM mutation begins
FSM WAL publication completes
FSM page writeback begins
FSM PAGE_INIT publication completes
prefix advancement begins and completes
local repair begins and completes
rebuild page publication completes
candidate publication occurs
```

Concurrency and crash cases use barriers, injected outcomes, or coordinated child processes;
they MUST NOT depend on sleeps or probabilistic timing. Category bytes cannot be observed as
trusted candidate metadata until transfer, format/version dispatch, checksum, page identity,
registered FSM owner, paired heap owner, FSM header/ranges, prefix, and suffix validation have
completed. A plausible `page_lsn` on a bad-checksum page must not become trusted state.

#### Category arithmetic and heap geometry

Exhaustively enumerate every legal `free_bytes` value in `0..8144`. For each value, calculate
the expected category with independent checked arithmetic:

```text
b        = min(free_bytes, 8144)
u        = min(max(b - 8, 0), 8135)
expected = floor(u * 255 / 8135)
```

Compare the expected value with the category codec/update result, assert that it is in
`0..255`, and assert monotonic nondecrease from the preceding input. This is an exhaustive
8145-case domain check, not a sampled property test. Where the category converter accepts a
wider input domain, also pass 8145 and the converter's maximum representable input and assert
that checked clamping produces the same result as 8144; these are converter-saturation cases,
not valid heap-geometry fixtures.

Independently enumerate every category `c` in `0..255` and calculate:

```text
minimum_usable(c) = (c * 8135 + 254) / 255
```

Assert exact equality with the inverse helper, monotonic nondecrease, checked intermediate
arithmetic, `minimum_usable(0) = 0`, `minimum_usable(255) = 8135`, and no result above 8135.
Keep explicit readable vectors for categories 1, 127, and 254 with expected lower bounds 32,
4052, and 8104 respectively.
For every forward-domain input and every threshold category, assert that a forward result at
least that threshold represents at least the corresponding `minimum_usable` byte count. This
joint exhaustive check proves that the inverse never promises more complete encoded tuple
bytes than the forward bucket guarantees.

Named boundary vectors keep failures readable:

| `free_bytes` | Usable bytes after the slot reserve | Expected category |
|---:|---:|---:|
| 0 | 0 | 0 |
| 1 | 0 | 0 |
| 8 | 0 | 0 |
| 9 | 1 | 0 |
| 39 | 31 | 0 |
| 40 | 32 | 1 |
| 71 | 63 | 1 |
| 72 | 64 | 2 |
| 4075 | 4067 | 127 |
| 8142 | 8134 | 254 |
| 8143 | 8135 | 255 |
| 8144 | 8135 | 255 |

Create otherwise valid `FSM_DATA` fixtures containing every byte value `0..255` inside the
initialized prefix and assert that all are accepted. Verify category zero in two contexts:
inside `[0, entry_count)` it is an initialized least-capacity estimate; at or beyond
`entry_count` the same zero byte is uninitialized storage and cannot be accessed or returned
as a candidate.

Exercise the eight-byte new-slot reserve with paired heap/FSM fixtures. For a requested
complete encoded tuple length `n`, compare a gap of `n + 8` with `n + 7` using the exact
inverse/bucket threshold: the first may advertise the request under the new-slot convention,
while the second must not guarantee it. Separately construct an architecture-authorized
reusable `UNUSED` slot and a gap that fits `n` only without a new slot. Heap insertion may
succeed, while FSM selection may conservatively skip the page; the oracle classifies this as
safe packing inefficiency and retains the guarded heap recheck.

For the whole-tuple limit, assert that complete encoded length 8135 is accepted by the heap
limit, a fresh page with `free_bytes = 8144` maps to category 255, free gaps 8143 and 8144 map
to 255, and `minimum_usable(255) = 8135`. A complete encoded tuple length of 8136 must still
be rejected by the Chapter 5 heap oracle; no category interpretation makes it legal.

Construct a fragmented heap page whose current contiguous `[lower,upper)` gap is smaller than
its total reclaimable space. Assert that pre-compaction categorization uses the current gap,
legal compaction may increase the recomputed category, and the old lower category remains a
safe stale value. FSM metadata must not itself authorize compaction or claim post-compaction
capacity before heap logic establishes it.

#### Mapping and maximum coverage

For every mapping case, the independent checked-arithmetic oracle computes:

```text
ordinal        = heap_page_no - 1
fsm_page_no    = 1 + ordinal / 8144
entry_index    = ordinal % 8144
reverse_page   = 1 + (fsm_page_no - 1) * 8144 + entry_index
```

Reject heap page zero, `INVALID_PAGE_NO`, underflow, overflow, an FSM data PageNo of zero, and
any entry index at least 8144 before narrowing or indexing. Verify these fixed vectors:

| Heap PageNo | Heap ordinal | FSM PageNo | Entry | Coverage boundary |
|---:|---:|---:|---:|---|
| 1 | 0 | 1 | 0 | first entry of FSM page 1 |
| 8144 | 8143 | 1 | 8143 | last entry of FSM page 1 |
| 8145 | 8144 | 2 | 0 | first entry of FSM page 2 |
| 16288 | 16287 | 2 | 8143 | last entry of FSM page 2 |
| 16289 | 16288 | 3 | 0 | first entry of FSM page 3 |
| 1,125,899,906,842,622 | 1,125,899,906,842,621 | 138,249,006,243 | 7773 | maximum physical heap PageNo |

For representative FSM page `P`, verify that entries `0..8143` reverse to the contiguous
range `1 + (P-1)*8144` through `P*8144`, subject to the physical PageNo and initialized-prefix
bounds. Forward then reverse and reverse then forward must each produce one identity, with no
gap or overlap.

At the v1 maximum physical heap PageNo, verify checked subtraction, division, remainder,
addition, multiplication, and file-offset formation before narrowing. The paired file needs
`138,249,006,243` FSM data pages, `138,249,006,244` total pages including its superblock, and
`1,132,535,859,150,848` bytes. Its last page begins with heap PageNo
`1,125,899,906,834,849` and needs initialized prefix length 7774. Assert the final FSM PageNo
and signed positional-I/O byte range remain legal, so FSM capacity does not exhaust before
the heap PageNo domain.

#### Initialized prefix, format, and ownership

Create valid pages with `entry_count` 0, 1, 8143, and 8144 when all represented pages lie
within the paired heap's published bound. Create raw malformed input with `entry_count = 8145`
and reject it before category access or ordinary publication.

Publish a heap extent longer than its initialized FSM prefix and assert that the FSM page is
structurally valid, only initialized entries are searchable, and omitted suffix coverage is
not corruption. Then extend the prefix without skipping positions: heap publication must
precede entry availability, the new entry must map to the exact heap PageNo, and every byte
after the new prefix must remain zero.

For each selected prefix length, verify all suffix bytes are zero. Inject one nonzero byte at
the first suffix position, a middle suffix position, and the final category byte where those
positions exist; each fixture is malformed v1 FSM data and must be rejected. Direct access,
update, or candidate publication at `index == entry_count` must also be rejected rather than
interpreting the suffix zero as category zero.

Pair each FSM page with the authoritative heap published bound. A prefix ending exactly at
that bound is accepted; a prefix extending one entry beyond it is rejected before ordinary
candidate use. An initialized entry for a retired, unpublished, or out-of-bound heap PageNo
must never become a candidate.

Use two fully valid relations A and B with distinct TableIds, heap FileIds, and FSM FileIds.
Ordinary FSM use for A must agree across the expected FSM descriptor, `FileKind::FSM`, FSM
FileId, superblock object_id/TableId, paired heap TableId/identity, requested FSM PageNo and
`FSM_DATA` type, and both files' published bounds. Attempting FSM(A) with heap(B) is rejected
as wrong-owner state before candidate publication, not treated as stale metadata.

Run a table-driven owner/format matrix containing:

```text
correct FSM FileId, wrong TableId
wrong FSM FileId, correct TableId
correct superblock, wrong registered descriptor
valid page bytes copied from another FSM file
correct TableId, wrong paired HEAP identity
FSM PageNo 0 used as FSM_DATA
INVALID_PAGE_NO
stored PageNo mismatch
wrong PageType or FileKind
unpublished FSM PageNo
bad header_size or checksum
nonzero common or FSM reserved field
wrong deterministic first_heap_page_no
recognized future format version
```

Malformed v1 and wrong-owner cases are rejected before category access. A recognized future
format/version produces the canonical unsupported-format result before future bytes are
interpreted. The bad-checksum case embeds a plausible high `page_lsn` and proves checksum
acceptance precedes trust in that LSN. Generic FileSuperblock and common-page checks reuse the
Chapter 4 storage procedures; this family adds the FSM pairing, deterministic mapping,
prefix/suffix, and candidate-publication oracles.

#### Advisory candidates and physical-space effects

For stale high, persist a structurally valid suitable category while the paired heap page has
insufficient actual insertion space. Candidate selection may suggest the page, but it must
then fetch and owner-validate the heap page, acquire the required protection, recompute actual
heap insertion feasibility, and reject mutation. Assert no extent overlap or out-of-bounds
write, and permit downward repair plus continued search/fallback. A test hook immediately
before heap mutation must fail the test if no guarded geometry recheck occurred.

For stale low, persist a category below the request threshold while the heap page can accept
the tuple. Candidate search may skip the page and choose another page or extension. The oracle
records only efficiency/packing loss; no exact packing or eventual-discovery guarantee is
imposed. Repair or rebuild may improve the category.

Construct a structurally and owner-valid category that differs from current heap geometry.
Structural validation must accept it, and ordinary use treats it as stale advisory metadata,
not corruption. Parameterize the stale-high and stale-low cases so that the stale source can
also be the process-local candidate accelerator while persisted FSM bytes remain exact.
Invalidating or rebuilding that accelerator must not affect persistent correctness, and no
container, bucket layout, or tie-breaker is prescribed.

Exercise physical-space changes rather than logical row counts:

- logical DELETE retains its tuple bytes and must not advertise reclaimed space;
- UPDATE consumes destination capacity while the old version retains its space until
  reclamation;
- an aborted but physically retained inserted version still consumes space;
- `NORMAL -> DEAD` and retained DEAD payload do not by themselves create reusable bytes;
- payload discard, `DEAD -> UNUSED`, or compaction may permit a refreshed category computed
  from the final validated heap geometry.

An FSM value that resembles an empty or maximally available page cannot authorize SlotId,
RID, or whole-page reuse. The Vacuum and Reclamation Tests remain the oracle for the
Chapter 14 grace and identity predicates.

#### Mutation, PAGE_INIT, and crash boundaries

Create a deterministic boundary where a heap mutation publishes successfully and the FSM
hint update is omitted or fails before publication. Assert that the heap mutation remains
valid, its statement/transaction outcome is not rolled back solely because the hint is
absent, the old category may remain stale, and later candidate use remains safe through heap
recheck. This directly permits separate heap and FSM MTRs without requiring them.

For an attempted FSM update after heap truth changes, inject:

| Boundary | FSM survivor | Required oracle |
|---|---|---|
| before FSM mutation publication | old complete legal category | heap result remains valid; stale hint is safe |
| complete FSM WAL publication, page not flushed | redoable mutation | generic recovery may install the complete new category |
| WAL/publication outcome is unsafe or indeterminate | canonical failure state | apply §39.1; do not invent an FSM-specific result |
| after complete FSM page publication | new complete legal category | `page_lsn`, checksum, and candidate visibility agree |

Specialize the generic PAGE_INIT and MTR Rollback Tests for new `FSM_DATA` pages. Pause after
private image construction, after PAGE_INIT WAL assignment, before published-bound advance,
and after ordinary publication. Assert correct PageNo/type/owner/header/prefix/suffix,
WAL-before-data, final checksum, and page_lsn. No private or unpublished page may enter either
the persisted or runtime candidate search.

Crash before PAGE_INIT publication, after PAGE_INIT WAL publication but before data flush,
and after page publication. Reuse the generic append/recovery oracle, adding that candidate
search never sees a private/unpublished FSM page. Failure before publication leaves no
ordinary searchable page; a published page must recover as a complete valid page.

Interrupt prefix advancement near both an ordinary boundary and 8144. Recovery must expose
either the old complete prefix or the new complete prefix, never a torn count, nonzero bytes
outside the recovered prefix, a skipped initialized position, or a prefix past the heap
published bound.

Use barriers for both growth races:

- while one worker publishes a new heap page, FSM search/update may lag but cannot lead the
  authoritative heap bound;
- when mapping first requires a new FSM data page, concurrent search ignores it until
  PAGE_INIT, publication, owner validation, prefix validity, and candidate publication all
  complete.

#### Required derived state, repair, rebuild, and recovery

For a committed table, ordinary open establishes the catalog-owned FSM descriptor, managed
identity, FileId, FileKind, TableId, and final file name. Remove the required FSM file and
assert that READY cannot silently treat it as optional or substitute another file. Where the
canonical recovery/repair owner permits reconstruction, the required object must be restored
at its exact identity before normal use.

Separate structurally valid staleness from structural invalidity. READY may admit a valid but
inaccurate FSM according to the recovery contract. Bad checksum, bad header size, nonzero
reserved data, wrong deterministic range, excessive prefix, nonzero suffix, or owner mismatch
must not enter ordinary use. An explicitly owned rebuild reads validated paired heap geometry;
it never treats invalid old category values as source truth.

This family proves the complete required/rebuildable/derived/persistent classification:

- required: the expected catalog-owned FSM identity is not ignored;
- rebuildable: the owned repair/recovery path can reconstruct invalid category state;
- derived: validated heap geometry, not old FSM bytes, row counts, or planner statistics, is
  the rebuild oracle;
- persistent: rebuilt pages use ordinary PAGE_INIT, WAL, page_lsn, checksum, and publication
  procedures.

For local repair, begin with one stale-high and one stale-low entry, capture a guarded and
owner-validated heap snapshot, independently calculate its category, and publish only the
mapped entry/prefix change permitted by the architecture. Assert owner and mapping stability.
The result may become stale again after a later heap mutation. Crash during repair and accept
only the old complete category or new complete category according to generic WAL/page
recovery; torn page state is forbidden.

For whole-file or page-range rebuild, scan validated paired heap-page headers, derive each
current contiguous gap, apply the same independent forward-category oracle used by incremental
updates, map each heap PageNo exactly once, construct the legal prefix, and zero the suffix.
Compare incremental update and rebuild across the exhaustive `free_bytes` domain to prove
formula identity.

Crash after any published rebuild page/prefix. Every published survivor must be independently
valid; short or missing coverage remains non-searchable rather than fabricated category-zero
coverage; unpublished work is ignored or reconciled; and rebuild can restart or resume under
the recovery owner. If rebuild scans concurrently with heap changes, accept structurally legal
stale categories and require heap recheck. A quiescent rebuild strategy is also conforming;
the test suite must not require one scheduling choice.

The crash/recovery specialization is:

| Injected boundary | Heap authority | Permitted FSM survivor | Recovery/candidate oracle |
|---|---|---|---|
| heap mutation durable, hint absent | new heap state | old legal category | READY may retain staleness; heap recheck is final |
| FSM WAL mutation complete, page unflushed | corresponding heap state | old disk image plus redoable WAL | redo installs a complete legal FSM mutation |
| FSM PAGE_INIT private or unpublished | published heap bound only | no ordinary FSM page | page cannot be searched; generic append recovery applies |
| FSM PAGE_INIT published | published heap bound | complete initialized FSM page | validate owner/header/prefix/suffix before search |
| prefix advancement interrupted | unchanged published heap bound | old or new complete prefix | no torn count, skipped entry, nonzero suffix, or lead beyond heap |
| local repair interrupted | authoritative heap page | old or new complete category | either survivor is advisory-safe; no torn page |
| rebuild partially published | validated heap pages | independently valid published prefixes/pages | short coverage is safe; restart/resume rebuild |
| page reinit or retirement before FSM refresh | new authoritative bound/incarnation | stale old category | bound and Chapter 14 gates prevent candidate or RID misuse |

#### Reclamation, retirement, and identity safety

Specialize the Vacuum and Reclamation Tests with an FSM estimate that suggests maximal
capacity while a former RID or page identity is still grace-protected. Before the complete
Chapter 14 predicate, SlotId reuse and whole-page reinitialization remain forbidden. After
grace and canonical reinitialization, the heap geometry resets and FSM state may be refreshed
for the new page incarnation; an old RID must not resolve to a new tuple prematurely.

Persist stale FSM metadata for a retired or out-of-bound heap PageNo and for a physically
present unpublished heap tail. Assert that candidate publication rejects each case before
heap access based on FSM alone. Repair/rebuild may remove or zero such metadata, but the stale
bytes cannot expand the authoritative heap bound.

#### Error and classification matrix

| Condition | Procedure | Expected classification or result |
|---|---|---|
| category differs from guarded heap geometry | stale-high/low fixture | legal advisory staleness; heap recheck decides |
| malformed v1 FSM page | raw format/prefix/suffix fixture | corruption; reject ordinary use |
| valid bytes owned by another FSM/heap pair | two-relation owner fixture | wrong-owner corruption; reject before candidate publication |
| required FSM file absent | committed-table open fixture | missing-required-object/open-repair outcome; never optional substitution |
| recognizable future page/file format | version-dispatch fixture | unsupported-format result before layout interpretation |
| exact or short ordinary I/O | generic disk fault specialization | canonical I/O failure |
| unsafe WAL/publication failure | FSM mutation injection | canonical §39.1 durability/instance consequence |
| candidate fails guarded heap-space recheck | stale-high insertion fixture | candidate `NO_SPACE`/retry path, not FSM corruption |

#### FSM verification domain/case matrix

| Domain | Deterministic fixture | Fault/race boundary | Expected oracle | Architecture reference | Status |
|---|---|---|---|---|---|
| Forward categories | all `free_bytes` 0..8144 | N/A | exact independent formula and monotonicity | §§6.3, 6.3.1 | COMPLETE |
| Inverse categories | all categories 0..255 | N/A | exact lower bound and monotonicity | §6.4 | COMPLETE |
| Slot reserve / tuple limit | paired gaps, reusable slot, 8135/8136 | heap recheck | conservative eight-byte rule; heap decides | §§5.4–5.6, 6.3–6.4 | COMPLETE |
| Mapping boundaries | fixed vectors and reverse map | arithmetic limits | no gap, overlap, wrap, or early FSM exhaustion | §6.6; §4.3.2.3 | COMPLETE |
| Prefix/suffix | counts 0, 1, 8143, 8144, malformed 8145 | prefix publication | short prefix accepted; suffix canonical | §§6.7–6.8 | COMPLETE |
| Owner pairing | two valid relations and cross-cases | before candidate publication | exact descriptor/FileId/TableId/heap pairing | §§4.10, 4.13.6, 6.13 | COMPLETE |
| Stale high | overstated legal category | before heap mutation | guarded heap recheck prevents unsafe insert | §§6.1, 6.10 | COMPLETE |
| Stale low | understated legal category | candidate enumeration | efficiency loss only | §6.10 | COMPLETE |
| PAGE_INIT | canonical private FSM page | WAL and publication boundaries | unpublished page never searchable | §§6.8, 12.9, 12.12 | COMPLETE |
| Prefix crash | prefix near ordinary/full boundary | count/category publication | old or new complete prefix only | §§6.7–6.8, 12.12 | COMPLETE |
| Local repair | stale-high and stale-low entries | repair WAL publication | old or new legal category | §6.10; §12.12 | COMPLETE |
| Rebuild | validated heap scan | partial page/range publication | restartable valid short coverage; formula identity | §6.10; §§13.11–13.19 | COMPLETE |
| Required file | committed catalog owner | missing/corrupt file at open | owned repair/rebuild or no READY | §§4.13.6, 6.10, 13.18 | COMPLETE |
| Reclamation/reuse | grace-protected RID/page | before and after grace | FSM never authorizes identity reuse | §§6.12–6.13, 14.5–14.12 | COMPLETE |

#### Chapter 6 architecture-obligation coverage map

The atomic inventory below is the procedural owner map for Chapter 6. “Complete” means that
the obligation has a deterministic fixture, operation or injected boundary, and explicit
oracle in this section or in the named generic procedure it specializes.

| # | Architecture obligation | Architecture owner | Verification owner | Status |
|---:|---|---|---|---|
| 1 | Forward formula exactness | §6.3 | Category arithmetic — exhaustive forward domain | COMPLETE |
| 2 | Inverse formula exactness | §6.4 | Category arithmetic — exhaustive inverse domain | COMPLETE |
| 3 | Forward monotonicity | §6.3 | Category arithmetic — adjacent full-domain comparison | COMPLETE |
| 4 | Saturation at physical/tuple maxima | §6.3 | Category arithmetic — 8143/8144 and upper-limit cases | COMPLETE |
| 5 | Category-zero boundary and meaning | §§6.2–6.4 | Category arithmetic — named boundaries and context test | COMPLETE |
| 6 | Every uint8 category is valid in-prefix | §§6.2, 6.5 | Category arithmetic — 0..255 encoded fixtures | COMPLETE |
| 7 | Eight-byte new-slot reserve | §6.3 | Category arithmetic — `n+8`/`n+7` paired cases | COMPLETE |
| 8 | Reusable-slot conservative understatement | §6.3 | Category arithmetic — reusable UNUSED fixture | COMPLETE |
| 9 | 8135 maximum complete encoded tuple interaction | §§5.6, 6.3–6.4 | Category arithmetic — whole-tuple boundary | COMPLETE |
| 10 | 8136 remains an invalid complete encoded tuple | §5.6 | Category arithmetic — heap final-authority boundary | COMPLETE |
| 11 | First heap-page mapping | §6.6 | Mapping — heap 1 vector | COMPLETE |
| 12 | Last entry of FSM page 1 | §6.6 | Mapping — heap 8144 vector | COMPLETE |
| 13 | First entry of FSM page 2 | §6.6 | Mapping — heap 8145 vector | COMPLETE |
| 14 | Exact 8144-entry boundaries | §6.6 | Mapping — 8144/8145 and 16288/16289 vectors | COMPLETE |
| 15 | Reverse mapping and coverage | §6.6 | Mapping — bidirectional range enumeration | COMPLETE |
| 16 | Checked mapping arithmetic | §6.6 | Mapping — checked operation/failure cases | COMPLETE |
| 17 | Maximum heap PageNo mapping | §§4.3.2.3, 6.6 | Mapping — terminal vector | COMPLETE |
| 18 | Maximum paired FSM coverage | §§4.3.2.3, 6.6 | Mapping — page-count/file-length proof | COMPLETE |
| 19 | `entry_count` lower bound | §6.7 | Prefix — zero-count valid fixture | COMPLETE |
| 20 | `entry_count` upper bound | §6.7 | Prefix — 8144 valid and 8145 invalid fixtures | COMPLETE |
| 21 | Short initialized prefix is valid | §6.7 | Prefix — heap extent longer than prefix | COMPLETE |
| 22 | Initialized category zero is valid | §§6.7–6.8 | Prefix — in-prefix zero fixture | COMPLETE |
| 23 | Suffix bytes are canonical zero | §6.7 | Prefix — valid zero suffix | COMPLETE |
| 24 | Nonzero suffix is malformed | §6.7 | Prefix — first/middle/final corruption fixtures | COMPLETE |
| 25 | Entry outside prefix is inaccessible | §6.7 | Prefix — `index == entry_count` operations | COMPLETE |
| 26 | Prefix cannot exceed paired heap bound | §6.7 | Prefix — exact-bound/one-past pair | COMPLETE |
| 27 | Correct HEAP/FSM TableId pairing | §§4.10, 4.13.6 | Ownership — valid relation fixture | COMPLETE |
| 28 | HEAP and FSM use distinct FileIds | §§4.7, 6.1–6.2 | Ownership — valid relation identity checks | COMPLETE |
| 29 | Wrong FSM/heap attachment is rejected | §§4.13.6, 6.13 | Ownership — two-relation cross-pair fixture | COMPLETE |
| 30 | Wrong FileKind is rejected | §§4.10, 6.2 | Ownership — malformed-owner table | COMPLETE |
| 31 | Wrong PageType is rejected | §§4.9, 6.5 | Ownership — malformed-owner table | COMPLETE |
| 32 | Wrong stored PageNo is rejected | §§4.9, 6.6 | Ownership — malformed-owner table | COMPLETE |
| 33 | Unpublished FSM page is rejected | §§4.11, 6.8 | Ownership/PAGE_INIT — publication barrier | COMPLETE |
| 34 | Entry beyond heap bound is rejected | §§6.7, 6.13 | Prefix/ownership — paired-bound fixture | COMPLETE |
| 35 | Stale-high category is safe | §6.10 | Advisory — overstated candidate fixture | COMPLETE |
| 36 | Stale-low category is safe | §6.10 | Advisory — understated candidate fixture | COMPLETE |
| 37 | Heap recheck precedes mutation | §§6.1, 6.4, 6.10 | Advisory — mandatory ordering hook | COMPLETE |
| 38 | Runtime candidate index is not authoritative | §6.9 | Advisory — invalidation/stale runtime parameter | COMPLETE |
| 39 | FSM cannot prove insertion success | §§6.1, 6.10 | Advisory — stale-high rejection | COMPLETE |
| 40 | FSM cannot authorize RID reuse | §§6.12–6.13 | Reclamation — grace-protected capacity fixture | COMPLETE |
| 41 | Heap mutation may precede FSM hint update | §6.10; §14.16 | Mutation — omitted-hint boundary | COMPLETE |
| 42 | Missed FSM update does not roll back heap truth | §6.10; §39.1 | Mutation — omitted/failed hint oracle | COMPLETE |
| 43 | Separate FSM MTR is permitted | §§6.8, 6.10; §12.12 | Mutation — separate publication fixture | COMPLETE |
| 44 | WAL-before-data applies to FSM | §§6.8, 12.17 | PAGE_INIT/WAL generic specialization | COMPLETE |
| 45 | `page_lsn` and checksum follow final FSM mutation | §§4.12, 6.8 | PAGE_INIT/WAL specialization | COMPLETE |
| 46 | FSM PAGE_INIT publication is atomic | §§6.8, 12.9, 12.12 | PAGE_INIT boundary matrix | COMPLETE |
| 47 | Private/unpublished FSM pages are not searchable | §6.8 | PAGE_INIT candidate-publication hook | COMPLETE |
| 48 | Complete FSM mutation is redoable | §§12.12, 13.13 | Mutation/recovery — WAL-published case | COMPLETE |
| 49 | READY may use structurally valid stale FSM | §6.10; §13.18 | Recovery — stale valid open fixture | COMPLETE |
| 50 | Structurally invalid FSM is rejected | §§4.13, 6.5–6.8 | Format/ownership corruption matrix | COMPLETE |
| 51 | Local stale-entry repair | §6.10 | Repair — guarded heap snapshot fixture | COMPLETE |
| 52 | Interrupted repair is atomic/recoverable | §§6.10, 12.12 | Repair — old/new survivor crash case | COMPLETE |
| 53 | Rebuild derives from heap geometry | §6.10 | Rebuild — validated heap scan | COMPLETE |
| 54 | Partial rebuild crash remains safe | §6.10; §§13.11–13.19 | Rebuild — published-prefix crash matrix | COMPLETE |
| 55 | Interrupted prefix advancement remains complete | §§6.7–6.8, 12.12 | Prefix — old/new survivor crash case | COMPLETE |
| 56 | Rebuild can restart/resume safely | §6.10; §13.18 | Rebuild — partial restart case | COMPLETE |
| 57 | FSM file is required for its catalog owner | §§4.7, 4.13.6 | Required-state — missing-file open fixture | COMPLETE |
| 58 | Missing FSM follows owned repair/open behavior | §§4.13.6, 6.10, 13.18 | Required-state — no optional substitution | COMPLETE |
| 59 | FSM is rebuildable derived persistent state | §§6.8, 6.10 | Required-state four-part classification fixture | COMPLETE |
| 60 | Logical DELETE does not free physical bytes | §§5.16, 15.4 | Physical effects — retained tuple fixture | COMPLETE |
| 61 | UPDATE old page retains physical consumption | §§5.15, 15.3 | Physical effects — old/new version fixture | COMPLETE |
| 62 | Vacuum/compaction may refresh category | §§6.11–6.12, 14.16 | Physical effects — state/compaction matrix | COMPLETE |
| 63 | `DEAD -> UNUSED` remains grace-gated | §§6.12, 14.5–14.12 | Reclamation — before/after grace fixture | COMPLETE |
| 64 | Whole-page reinit remains RID/page-reuse gated | §§6.12, 14.5–14.12 | Reclamation — page-incarnation fixture | COMPLETE |
| 65 | Retired/unpublished heap pages cannot be candidates | §§4.11, 6.7–6.8 | Retirement — stale out-of-bound fixtures | COMPLETE |
| 66 | Exact FSM page layout and little-endian fields | §§6.5–6.5.1 | Deterministic canonical fixture and codec checks | COMPLETE |
| 67 | Common/FSM reserved fields are zero | §§6.5.1, 6.13 | Format — table-driven nonzero fixtures | COMPLETE |
| 68 | Future format and malformed v1 dispatch differ | §§4.13–4.14, 6.5 | Format — version/classification matrix | COMPLETE |
| 69 | Category input is current contiguous heap gap | §§5.4, 6.3, 6.11 | Category/compaction fixture | COMPLETE |
| 70 | Runtime accelerator is derived and nonpersistent | §6.9 | Advisory — invalidate/rebuild runtime index | COMPLETE |
| 71 | FSM validation ordering protects LSN/category use | §§4.13, 6.13 | Observability — staged acceptance hooks | COMPLETE |
| 72 | Stale, corrupt, missing, unsupported, I/O, WAL, and `NO_SPACE` outcomes remain distinct | §§4.13–4.14, 6.10, 39.1 | Error and classification matrix | COMPLETE |

Coverage inventory: `72 COMPLETE`, `0 PARTIAL`, `0 MISSING`, and
`0 CONTRADICTORY`.

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

This section is the detailed procedural owner for the B+ tree contract in
[`ARCHITECTURE.md`](ARCHITECTURE.md) Chapter 8. It specializes, without duplicating, the
generic page-format, BufferPool, PAGE_INIT, WAL/MTR, recovery, uniqueness, and reclamation
procedures in this guide. Principal architecture owners are §§4.3.2, 4.6, 4.10–4.14,
7.5–7.12, 8.1–8.29, 11.8–11.10, 12.10.2–12.10.3, 12.12, 12.17,
13.13.3–13.14, 14.5–14.15, 15.2–15.4, 16.5.4–16.6, 17.4 and 17.7,
39.1, and 41.2.

Every synthetic fixture MUST be canonical except for the single tested dimension. Valid
fixtures have an exact FileSuperblock, FileId, IndexId, TableId, PageNo, PageType, level,
published bound, schema fingerprint/version, key and RID encodings, required-zero bytes, and
checksum. A routing or race case must not accidentally become a checksum or owner-validation
case.

### Deterministic harness, observability, and reference models

Concurrency and failure cases use explicit hooks, barriers, controlled threads, injected
outcomes, and separate-process termination where crash semantics require it. Sleeps,
wall-clock ordering, and repeated racing until a condition appears are not valid primary
oracles. Random stress remains complementary state-space exploration.

The harness can pause and inspect abstract semantic events without requiring source-level
function names:

```text
root metadata snapshot and root_generation capture
root/parent/child/sibling latch attempt and acquisition
safe/unsafe child decision and ancestor release
split/merge decision, page reservation, and private initialization
private split/rebalance distribution and separator preparation
BTREE_MTR reservation, authorizing append, and runtime publication
root/endpoint/free-list metadata publication
page detachment, BTREE_FREE publication, guard drain, and reuse eligibility
cursor next-leaf handoff and guard release
fault/crash boundary and recovery redo begin/complete
L1/L2 validation, ordinary residency publication, and L3 verifier begin/complete
```

Observable state includes complete page bytes, PageId and owner, page type/level, slot and
entry geometry, page LSN, root PageNo/height/generation, endpoint and free-list metadata,
parent/child/sibling reachability, guard/pin/latch ownership, no-flush and dirty generations,
WAL valid/durable ends, file published bound, ordinary residency, and verifier visited sets.

Use two independent oracles:

1. A byte oracle writes and reads expected little-/big-endian fields directly from specified
   offsets; production encode/decode cannot serve as both operation and oracle.
2. A logical ordered model compares semantic values and canonical numeric RIDs independently
   of `IndexKeyCodec` and B+ search code. It owns expected insert, exact erase, point, equality,
   range, and total-order results.

### Persisted byte and geometry verification

Construct the complete specialized BTREE superblock independently and compare every byte.
Exercise the canonical page and one-defect-at-a-time variants below; the ordinary open/load
path must classify the defect before typed use.

| Superblock case | Deterministic fixture/oracle | Expected result |
|---|---|---|
| Common prefix and extension | Check common bytes `0..71`; little-endian `table_id` at 72, `root_page_no` at 80, `first_leaf_page_no` at 88, `last_leaf_page_no` at 96, `free_page_head` at 104, fingerprint at 112, schema version at 120, height at 124, and flags at 126 | Exact 8192-byte match; `header_size=128` |
| Identity and kind | Mutate FileKind, common `object_id`/IndexId, or registered TableId independently | Registered-owner/open rejection with the architecture-owned corruption result |
| Root/endpoints/free head | Use invalid, out-of-bound, root/free overlap, and inconsistent empty-tree values one at a time | Local or L3 rejection at the owning layer; no ordinary tree use |
| Schema | Mutate fingerprint, zero version, and recognizable greater version separately | V1 mismatch/zero is corruption; future version is unsupported format |
| Height | Test zero, one-leaf height 1, maximum legal height, and root-level mismatch | Only architecture-legal state accepted; no wrap |
| Flags and reserved | Set `index_flags`, each common required-zero field, then table-drive every byte in `128..8191` independently | Recognized malformed v1 is rejected |

For BTREE_LEAF and BTREE_INTERNAL, independently check the 32-byte common header, 32-byte
node header, 8-byte slot descriptors, and packed-entry regions. Include blank nodes
(`slot_count=0`, `lower=64`, `upper=8192`), exact boundary values, and the structural
slot-only bound 1016 as an arithmetic/rejection boundary rather than an impossible populated
valid node. Exercise `lower==upper` in both a legal full image and an otherwise impossible
image. Checked arithmetic establishes `lower = 64 + slot_count * 8` and
`64 <= lower <= upper <= 8192`.

| Structure | Exact byte/geometry cases | One-defect cases and oracle |
|---|---|---|
| Leaf header | level 0; count/lower/upper; flags 0; prev/next PageNos; bytes 60..63 zero | Nonzero level/flags/reserved, invalid endpoint, or inconsistent geometry is rejected |
| Internal header | level >0; count/lower/upper; flags 0; leftmost child; bytes 52..63 zero | Level 0, invalid child, nonzero flags/reserved, or inconsistent geometry is rejected |
| Slot | Four little-endian uint16 fields at offsets 0, 2, 4, 6 | Reject nonzero flags, zero or >1024 key length, wrap, crossing page/directory, pairwise overlap, or forbidden aliased payload |
| Leaf entry | `entry_length = user_key_length + 16`; exact canonical RID | Reject inconsistent length, malformed key, RID sentinel/reserved/owner defect, overlap, or unsorted/duplicate physical key |
| Internal entry | `entry_length = user_key_length + 24`; separator RID then right-child uint64 | Reject inconsistent length, malformed separator, invalid child, overlap, or unsorted/duplicate separator |
| BTREE_FREE | Common header, `next_free_page_no` at 32, bytes `40..8191` zero | Reject wrong type/owner/PageNo, out-of-range link, locally detectable self-link, or any nonzero reserved body |

The required-zero matrix is explicit rather than represented by one generic case:

| Structure | Required-zero region | Fault coverage |
|---|---|---|
| Common FileSuperblock/page header | Every §4.8/§4.10 unassigned flag/reserved field | Set each field independently |
| BTREE superblock | `index_flags` and bytes `128..8191` | One flag case plus a table-driven walk covering every reserved byte |
| Leaf | node flags, bytes `60..63`, every slot flag, RID bytes `14..15` | One-field/byte-at-a-time mutation |
| Internal | node flags, bytes `52..63`, every slot flag, separator-RID bytes `14..15` | One-field/byte-at-a-time mutation |
| BTREE_FREE | common flags/reserved16 and bytes `40..8191` | One-field case plus complete reserved-body walk |

These cases catch ABI serialization, unchecked offset arithmetic, and parsers that trust a
checksum without proving safe ownership of each byte range.

### IndexKeyCodec and physical-order properties

For each supported key type, generate semantic values independently, encode only through the
system under test, sort one copy with a test-side semantic comparator, sort encoded bytes
lexicographically, and require identical equivalence classes and order. Decode/round-trip is
an additional assertion, not the ordering oracle. Chapter 17 owns scalar semantics; §8.5
owns index encoding.

| Key family | Deterministic/property fixtures | Independent oracle |
|---|---|---|
| NULL/presence | NULL, minimum non-NULL, zero-like, positive and negative values; NULL in every composite position | NULLS FIRST and exact `00`/`01` presence partition |
| BOOLEAN | NULL, false, true, including composite positions | Semantic `NULL < false < true` |
| INT32/DATE | signed minima, adjacent minima/maxima, -1, 0, 1, and DATE domain endpoints | Signed semantic order equals sign-flipped big-endian order |
| INT64/TIMESTAMP | analogous 64-bit and valid timestamp endpoints | Signed semantic order equals sign-flipped big-endian order |
| FLOAT64 | infinities, finite signs, subnormal/boundary values, both zeros, and multiple NaN payload/sign forms | Chapter 17 canonical total order; zeros and all NaNs collapse to their required classes without host NaN comparison |
| VARCHAR | empty, ASCII, embedded/repeated zero, prefixes, high bytes, and longest fitting component | Binary collation; `00 FF` escaping and `00 00` terminator preserve prefix order unambiguously |
| Composite | differences in first/middle/final components, NULL positions, and VARCHAR prefixes | Component-wise semantic lexicographic order equals concatenated encoded order |

For equal encoded user keys, vary canonical RIDs across FileId, PageNo, and SlotId boundaries.
The model compares the numeric tuple `(FileId,PageNo,SlotId)` and requires the same physical
order; RID reserved bytes stay zero. Distinct RIDs remain legal even for one SQL key, while
an exact duplicate physical key is rejected except for the narrowly proven same-operation
replay path.

Exercise encoded user-key lengths 1, 1023, 1024, and 1025. Legal sizes proceed when node
geometry permits. Size 1025 fails with the exact operation-level oversized-key result before
any structural publication, never truncates, and is not classified as persisted corruption.
Separately, representable persisted length fields that violate v1 formulas or page bounds
produce `CORRUPT_INDEX`.

### Routing, search, and duplicate ranges

Build internal pages directly as `C0,(K1->C1),...,(Kn->Cn)` and use a test-side routing
oracle implementing Chapter 8's lower-bound relation. Probe below K1, exact equality at every
separator, every between-separator interval, and above Kn. Exact equality MUST choose the
right child; repeat with equal user bytes and lower/equal/between/higher separator RIDs to
catch user-key-only internal comparisons.

Construct a legal stale-low separator by deleting the former minimum from a right child
without a structural boundary movement. Existing keys still route to that child, keys in the
stale gap may descend and fail locally, and L3 accepts the tree. Then make the separator
greater than a reachable right-child key; page-local checks may pass, but L3 must reject the
misrouting-high tree. Redistribution and child replacement cases verify exact boundary
refresh where Chapter 8 requires it.

Leaf lower-bound fixtures probe before first, exact first, between entries, equal SQL keys at
different RIDs, exact last, and beyond last. Equality starts at `(K,MIN_RID)`, traverses
next-leaf links, returns every matching RID once in physical order, and stops at the first
different user key. A duplicate run large enough to span several leaves is mandatory; it
catches implementations that treat a leaf boundary as an equality boundary.

### Split, redistribution, merge, and root publication

Create fragmented and compact leaf images with mixed key lengths at the insert-fit boundary:

1. fits without compaction;
2. fits only after compaction;
3. still does not fit and must split;
4. one very large legal key;
5. mixed small/large keys for which count balance differs from byte balance.

After a leaf split, compare the union against the pre-state plus insertion: every physical
entry appears exactly once, both pages fit and are nonempty, global order holds, the parent
separator is the first complete physical key in the right leaf, and sibling/end metadata is
reciprocal and correct. Force a split mostly or entirely inside one duplicate SQL-key run and
require RID order and cross-leaf equality completeness.

For an internal split, construct variable-length separator entries that force propagation.
The oracle reconstructs the original logical child/separator sequence and verifies the
selected promoted key is absent from both child pages, the right leftmost child is the
promoted key's child, all other children appear exactly once, levels are unchanged, parent
routing is correct, and every resulting page fits. Cascading and repeated splits apply the
same oracle at every level.

Exact 50/50 partition, slot midpoint, byte midpoint, physical memmove strategy, and internal
reconstruction algorithm are not test requirements. Every architecture-legal partition is
accepted if it preserves fit, nonempty results, canonical order, complete ownership,
routing, sibling/endpoints, level, and root invariants.

Run duplicate-heavy redistribution in both directions and right-into-left merge. Verify the
complete physical sequence before and after, exact parent lower-bound update when keys cross
the boundary, unchanged equality results, parent child removal, link/endpoint repair, and
detachment before BTREE_FREE publication. Sparse legal pages are accepted; approximately
25% occupancy is a soft policy, not a corruption threshold. If internal rebalance is chosen,
validate only the canonical resulting child/separator sequence and publication invariants.

For root split, contraction, and the one-empty-leaf case, pause before and after structural
publication and inspect root PageNo, tree height, root level, endpoints, page reachability,
`root_generation`, and the authorizing BTREE_MTR. Readers see the complete old tree before
publication and the complete new tree afterward. A root pointer to a private/incomplete page
is forbidden.

Race a reader that captured `(root_page_no,root_generation)` with split and contraction. The
reader latches the candidate, detects any changed identity/generation, releases, and restarts;
it never treats a former or reused root as current. Contraction decrements height exactly
once, retires the old root through the normal gate, and preserves one empty leaf at height 1.

### Latching, write crabbing, and cursor lifetime

All ordering tests use acquisition-attempt and acquisition-complete hooks. The test records a
wait-for graph and fails immediately on an architecture-forbidden edge; a watchdog is only a
secondary hang detector.

| Scenario | Required order | Deterministic race and oracle |
|---|---|---|
| Root acquisition | metadata snapshot/pin; release metadata; wait for root page; reacquire metadata to validate | Worker A holds root page and later needs metadata; worker B must not retain metadata while waiting and must restart on generation change |
| Root publication | required structural page latches before metadata latch | Reverse-order attempt cannot become a blocking edge; publication is complete before release |
| Vertical traversal | parent before child, with coupling where required | Competing writer makes early parent release or child-before-parent visible; only canonical order survives |
| Adjacent pages | left before right | A right-held operation discovering a needed left page releases/restarts rather than waiting right-to-left |
| Free-list/endpoint | snapshot metadata, release, latch page, reacquire metadata, revalidate | A competing head/endpoint change forces restart; stale candidate is never published |

For insert, construct a child that cannot propagate a split and one that can. The safe case
releases no-longer-needed ancestors at the canonical point; the unsafe case retains the
required path and can propagate without reverse-order reacquisition. Repeat for delete using
whether the chosen operation can require redistribution/merge propagation; do not harden the
soft occupancy policy. If optimistic release allows intervening mutation, force it at a
barrier and require revalidation/restart before structural use.

No transaction-level lock wait may retain a B+ page latch. Logical uniqueness wait cases are
owned by the transaction tests below, with a B+ observer asserting all page guards/latches
are released before the wait begins.

A forward cursor fixture records the current read guard, slot, upper bound, and next-leaf
handoff. At the leaf boundary it reads next while guarding the current leaf, acquires and
validates the next guard, then releases the old guard. Borrowed key/entry views are poisoned
or token-checked after release and cannot survive it.

Race handoff separately with split, merge, detach, and reuse. The physical traversal remains
ordered, misses no qualifying physical key solely because of handoff, and either reaches the
validated surviving chain or performs an architecture-permitted restart. A guarded current
or target page cannot expose replacement state; an unguarded numeric PageNo has no long-lived
identity guarantee. Transaction snapshot filtering is not inferred here: Chapter 10 owns
visibility of returned candidate RIDs.

### BTREE_MTR failure, crash, append, and recovery

Layer tree-specific checks on the generic Non-Crash WAL/MTR Failure Injection, PAGE_INIT and
MTR Rollback, and Crash Injection Framework below. For each structural operation, capture a
complete valid old tree and complete expected new-tree invariant set, then pause at page
reservation, private initialization, private mutation, MTR reservation, authorizing append,
runtime publication, allowed flush, and crash. Reopen through ordinary recovery and run the
full-tree verifier.

The recovered survivor is exactly the old tree or the complete new tree. It is never a
mixture. Exercise:

- leaf split: redistribution, reciprocal links, parent separator, and endpoint;
- internal/cascading split: child distribution, promotion, parent propagation;
- root split and contraction: root PageNo, height, root contents, and persistent metadata;
- redistribution/leaf and internal merge: movement, separator removal/update, links,
  endpoint, detachment, and free transition;
- free-list pop/push/reuse: head selection/revalidation, private reinitialization, and live
  publication;
- appended BTREE page: PAGE_INIT, file bound, private frame, MTR reference, and reachability.

Before an authorizing append, inject reservation, allocation/BufferPool, encoding, validation,
and known WAL append failures. Exact restoration compares page bytes, dirty/no-flush and
generation metadata, root/height/generation, endpoints, free head, frame/PageId state, and
new/reused page disposition. An unpublished appended tail follows §4.11.1.1 exactly.

After an authorizing append, inject uncertain append outcome, WAL write/sync failure, and
runtime publication failure separately. No case rolls back and continues. The operation
retains protected/nonordinary ownership for completion or enters the §12.12
`DATABASE_NONCONTINUABLE` lifecycle; no subsequent statement observes ambiguous structure.

For appended pages, ordinary traversal cannot reach or fetch the private page before the
same publication boundary establishes canonical page bytes, PAGE_INIT/BTREE_MTR authority,
file published bound, BufferPool residency, and structural reachability. After publication,
all dimensions agree. Recovery/private I/O cannot expose an unvalidated intermediate page to
ordinary callers.

Reopen matrices include clean tree, leaf split, internal/root split, merge/contraction,
endpoint change, free-page reuse, and interrupted MTR. Assert recovered root/height/endpoints,
exact-once contents, separators, levels, sibling/free graphs, page LSN coherence, and full
verifier acceptance. Run recovery again on the recovered files: no entry, separator,
free-list transition, or root metadata change repeats, and process-local `root_generation`
is initialized from recovered runtime state rather than treated as persistent history.

### L1, registered-owner, and unsupported-format validation

Ordinary load specializes the Chapters 4 and 7 order:

```text
exact full read
family/version dispatch
checksum
common PageId identity
registered file/index owner and published bound
PageType
B+ L1 structure
ordinary RESIDENT and typed use
```

`page_lsn` is not trusted before checksum. L1 validates one page without fetching referenced
pages; registered-owner validation supplies FileId/IndexId/TableId/schema/heap context; L3
owns global topology. A failed stage publishes no ordinary mapping/guard and later stages do
not run.

| Page family | One-defect-at-a-time L1 matrix | Expected result |
|---|---|---|
| Leaf | Common framing/checksum/PageId/type; flags/reserved; level; count/lower/upper; directory/payload and pairwise overlap; key/length/RID; owner heap FileId; strict physical order/duplicate; sibling domain | Recognized malformed v1 yields `CORRUPT_INDEX` before ordinary use |
| Internal | Common fields; level 0; missing/invalid leftmost child; count/child relation; separator codec/order/duplicate; right-child domain/self-reference where locally forbidden; overlap/length/reserved/bound | `CORRUPT_INDEX`; global subtree range remains L3 |
| BTREE_FREE | Type, owner/PageNo, link domain/local self-link, flags/reserved body | `CORRUPT_INDEX`; cycles and live overlap remain L3 |
| Superblock/owner | FileKind/FileId/IndexId/TableId/root/endpoints/free head/fingerprint/schema version/height/flags/reserved | Owning corruption or unsupported result; no tree publication |

Cross valid index A with registered index/descriptor B for FileId, IndexId, TableId, schema
fingerprint/version, expected heap FileId, and valid pages from the wrong file. Wrong ownership
is never accepted merely because bytes parse. Construct recognizable future file, page, and
key-schema versions separately: dispatch returns the exact unsupported-format result and does
not decode them as malformed v1. Zero/invalid v1 values remain corruption.

### L3 full-tree topology verification

Start from one deterministic valid multi-level tree containing duplicate runs across leaves,
a legal stale-low separator, valid endpoints, and a nonempty disjoint free list. L3 MUST
accept it. Mutate one global dimension at a time while keeping every page locally valid:

| L3 defect | Deterministic fixture | Full-verifier oracle |
|---|---|---|
| Child cycle | Back-edge to an ancestor | Bounded visited-set rejection; no loop |
| Duplicate parentage | One live child named by two parents | Reject nonunique live ownership |
| Subtree range | Locally sorted child contains key outside parent lower/upper bounds | Reject misrouting topology; legal stale-low remains accepted |
| Leaf global order | Locally sorted adjacent leaves reversed across boundary, including RID tie-break | Reject global physical-order violation |
| Leaf chain | Prev/next disagreement, cycle, disconnected reachable leaf | Reject with bounded traversal |
| Level/depth | Wrong child level, unequal leaf depth, root level/height mismatch | Reject |
| Endpoint/root | First/last not actual endpoints, invalid empty-tree tuple, wrong root type/level | Reject |
| Orphan | Published owned live node unreachable from root and absent from free list | Report unauthorized orphan under §§4.13.5/8.28 |
| Free/live overlap | One page reachable from both root and free head | Reject |
| Free-list topology | Cycle, duplicate membership/predecessor inconsistency, out-of-range link | Bounded rejection |
| Classification partition | Every published ordinary page assigned exactly once to live or free graph | Reject unclassified/duplicate ownership |

This distinction proves why locally safe parsing is not sufficient evidence of a correct
tree while avoiding an O(tree-size) validation requirement on every ordinary fetch.

### Detach, free-page reuse, and stale-reference safety

For page R, pause canonical merge/retirement after each ordering point: remove parent/root
routing, remove sibling/endpoint reachability, prevent new traversal, drain existing guards,
publish BTREE_FREE, then make R eligible for free-list allocation. A reachable page cannot be
rewritten as free, and a free page cannot remain live.

Hold an old PageGuard across logical detachment. New traversal may cease reaching R, but
physical reuse cannot expose a replacement image through that guard. After all lifetime gates
drain, reuse privately rewrites the complete page into one canonical leaf/internal image,
removes old free metadata/reserved-body residue, preserves the architecture-owned PageId, and
publishes free-list removal plus live reachability in one structural MTR.

Capture only R's numeric PageNo without a guard, detach/reuse it, and verify public access
must reacquire through current FileId/PageId, owner, validation, and guard rules. The raw
number has no generation guarantee and cannot implement a long-lived cursor. Inject an old
BufferPool asynchronous completion against the reused frame and apply Chapter 7's
identity/generation oracle: replacement state is unchanged.

### Cross-owner semantics, exhaustion, and failure classification

The physical tree permits `(K,RID1)` and `(K,RID2)`, can enumerate the entire user-key range,
and always returns physical candidates for heap/MVCC recheck. Construct invisible, aborted,
and dead-but-not-reclaimed target versions: the index remains structurally valid, lookup
returns the candidate, and Chapters 10–11 decide visibility/uniqueness. The UNIQUE tests below
own logical key locks and current-owner truth tables. Chapter 14 owns stale-entry removal and
RID grace; removing an index entry alone never authorizes heap RID reuse.

All ordinary index pages use Chapter 7 BufferPool PageId, guard, validation, writeback, and
retirement methodology. Add B+ observers proving traversal/split code retains no page pointer
after guard release and no specialized residency path creates a second cache/lifetime owner.

Specialize terminal-boundary tests with a maximum legal tree height and a growth attempt.
The maximum tree remains valid; growth fails before provisional mutation/page publication,
does not wrap height/level, and leaves the exact old tree. Page-local no-space first selects
compaction/split and is not disk exhaustion. Inject `NO_REPLACEABLE_FRAME`, disk
`RESOURCE_FULL`, PageNo exhaustion, height exhaustion, oversized operation key, malformed
persisted length, and WAL failures independently so each reaches only its owner.

| Condition | Verification oracle |
|---|---|
| Malformed recognized-v1 index bytes/topology | `CORRUPT_INDEX`; no ordinary malformed use |
| Recognizable future file/page/key-schema version | Exact unsupported-format result |
| Attempted encoded key >1024 | Operation-level oversized-key failure before publication |
| Current node does not fit | Compact if useful, then structural split; not `RESOURCE_FULL` |
| No BufferPool victim | `NO_REPLACEABLE_FRAME`; known pre-authorization tree unchanged |
| Disk capacity | `RESOURCE_FULL`; not buffer or numeric exhaustion |
| PageNo/height terminal growth | Owning deterministic exhaustion result; no wrap/partial root |
| WAL reservation/known append failure | Exact old tree and local continuation only where §12.12 permits |
| Authorizing append uncertainty | `DATABASE_NONCONTINUABLE`; no rollback-and-continue |
| Transactional uniqueness conflict | Chapter 11 logical/transaction result, not physical duplicate error |
| Structurally valid stale index entry | Not corruption by itself; heap/MVCC recheck and Chapter 14 cleanup |

### Chapter 8 procedural matrices

The following compact matrices index the detailed procedures above. `COMPLETE` means a
deterministic fixture, controlled boundary, independent oracle, and expected outcome are all
defined; architecture prose or random stress alone is insufficient.

#### Structural crash-outcome matrix

| Structural operation | Pre-authorization fault | Authorizing state / publication | Post-authorization uncertainty | Recovered oracle | Status |
|---|---|---|---|---|---|
| Leaf split | Exact old leaf/tree restored | One BTREE_MTR covers pages, links, parent, endpoint | Complete/retry or noncontinuable | Exact old or complete new tree | COMPLETE |
| Internal/cascading split | Old child/parent sequence restored | One MTR covers distribution/promotion/propagation | No local rollback | No orphan/duplicate child | COMPLETE |
| Root split | Old root/height retained | New root and metadata co-publish | No ambiguous root admitted | Coherent root/height/tree | COMPLETE |
| Redistribution | Old siblings/separator restored | Movement and boundary publish together | No mixed boundary | Old or canonical new sequence | COMPLETE |
| Leaf/internal merge | Old reachability restored | Movement, parent removal, links, detach publish together | Free transition cannot escape alone | No required detached child/live free page | COMPLETE |
| Root contraction | Old root/height retained | Child promotion, metadata, retirement co-publish | No ambiguous root admitted | Coherent root/height | COMPLETE |
| Endpoint update | Old endpoint retained | Endpoint included with structural change | No mixed chain/endpoint | Endpoint equals actual chain end | COMPLETE |
| Free-list pop | Old head/page remains free | Head removal and private conversion authorize together | Page protected/nonordinary | Page wholly free or wholly live | COMPLETE |
| Free-list push/reuse | Old live/free state restored | Detach/free or free/live transition is one MTR | No both/neither state | Disjoint complete graph | COMPLETE |
| Appended index page | Tail/frame/bound restored | PAGE_INIT, bound, residency, route authorize together | Protected completion/noncontinuable | Unpublished old or complete new page | COMPLETE |

#### Latch-order matrix

| Scenario | First action | Second action | Forbidden reverse/wait | Restart oracle | Status |
|---|---|---|---|---|---|
| Vertical traversal | Parent latch | Child pin/latch | Child then parent | Reacquire from root/path | COMPLETE |
| Adjacent pages | Left latch | Right latch | Wait right-to-left | Release and restart | COMPLETE |
| Root acquisition | Snapshot/pin under metadata, then release | Root page latch, then metadata validation | Waiting on page while retaining metadata | Generation/identity mismatch restarts | COMPLETE |
| Root publication | Structural page latches | Metadata latch | Metadata held while acquiring pages | Publication retries under canonical order | COMPLETE |
| Free-list/endpoint | Metadata snapshot, then release | Page latch, then metadata revalidation | Metadata retained during page wait | Changed head/endpoint restarts | COMPLETE |

#### Cursor-concurrency matrix

| Case | Barrier | Protected resource | Legal survivor | Forbidden survivor | Status |
|---|---|---|---|---|---|
| Ordinary handoff | After reading next, before next guard | Current guard and target identity | Valid next guard before current release | Unguarded borrowed view | COMPLETE |
| Handoff vs split | Split before/after next acquisition | Current/next guarded chain | Ordered continuation or permitted restart | Miss due solely to stale link | COMPLETE |
| Handoff vs merge | Detach before/after target guard | Existing guards and reciprocal chain | Surviving chain or restart | Use-after-detach/reuse | COMPLETE |
| Handoff vs detach | After numeric PageNo capture | Guard/lifetime gate | Fresh validated acquisition or restart | Raw PageNo treated as durable handle | COMPLETE |
| Handoff vs reuse | Reuse eligibility attempt | Pin/guard and BufferPool generation | Reuse waits or old access ends | Replacement bytes through old guard | COMPLETE |
| Guard release | Before/after release | Borrowed entry/key view | View invalid after release | View survives reassignment | COMPLETE |

#### L1 corruption matrix

| Page family | Defect family | Local/owner oracle | Classification | Before ordinary use? | Status |
|---|---|---|---|---:|---|
| Superblock | Framing, kind/identity, roots/endpoints/free head, schema, height, flags/reserved | Exact byte and registered-owner check | Corruption or exact unsupported format | yes | COMPLETE |
| Leaf | Header/level/geometry/overlap/key/RID/order/duplicate/sibling | Nonfetching L1 plus owner context | `CORRUPT_INDEX` | yes | COMPLETE |
| Internal | Header/level/geometry/overlap/separator/child/order | Nonfetching L1 plus bound context | `CORRUPT_INDEX` | yes | COMPLETE |
| BTREE_FREE | Header/type/owner/link/reserved body | Nonfetching free-page check | `CORRUPT_INDEX` | yes | COMPLETE |
| Cross-owner | FileId/IndexId/TableId/schema/heap mismatch | Registered descriptor validation | Owning corruption | yes | COMPLETE |
| Future format | Recognizable file/page/key-schema version | Dispatch before v1 parse | Unsupported-format result | yes | COMPLETE |

#### L3 topology matrix

| Defect | Deterministic fixture | Full-verifier oracle | Bounded? | Status |
|---|---|---|---:|---|
| Child cycle | Ancestor back-edge | Reject repeated PageNo | yes | COMPLETE |
| Duplicate parentage | Child named twice | Reject nonunique live ownership | yes | COMPLETE |
| Subtree range | Locally valid out-of-range child | Reject routing violation | yes | COMPLETE |
| Unequal leaf depth | One path shorter/longer | Reject level/depth mismatch | yes | COMPLETE |
| Global leaf order | Locally sorted boundary inversion | Reject physical-key inversion | yes | COMPLETE |
| Sibling chain | Reciprocity, cycle, or disconnect defect | Reject chain mismatch/repeat | yes | COMPLETE |
| Endpoint mismatch | First/last disagrees with chain | Reject metadata mismatch | yes | COMPLETE |
| Orphan live page | Published live page in neither graph | Report unauthorized orphan | yes | COMPLETE |
| Free/live overlap | Same page in both graphs | Reject duplicate classification | yes | COMPLETE |
| Free-list cycle/duplicate | Repeated free PageNo | Reject repeat | yes | COMPLETE |
| Root/height mismatch | Root type/level or empty tuple wrong | Reject | yes | COMPLETE |

#### Recovery/reopen matrix

| Persistent starting state | WAL/recovery action | Root/topology oracle | Full verifier | Status |
|---|---|---|---|---|
| Clean tree | Ordinary reopen | Same root/height/content | accepts | COMPLETE |
| Durable leaf split | Redo/reopen | Exact-once keys, links, endpoints | accepts | COMPLETE |
| Durable internal/root split | Atomic BTREE_MTR redo | Coherent root/height/levels | accepts | COMPLETE |
| Durable merge/contraction | Atomic redo | Removed pages unreachable, survivor canonical | accepts | COMPLETE |
| Durable free-page reuse | Atomic redo | Page appears exactly once, live or free | accepts | COMPLETE |
| Durable endpoint change | Atomic redo | Metadata equals recovered chain | accepts | COMPLETE |
| Interrupted structural MTR | WAL-prefix validation and redo | Exact old or complete new state | accepts survivor | COMPLETE |
| Already recovered tree | Repeat recovery/reopen | No duplicate action or persistent-generation drift | accepts unchanged state | COMPLETE |

#### High-level domain/case matrix

| Family | Deterministic fixture | Barrier/fault | Independent oracle | Architecture owner | Status |
|---|---|---|---|---|---|
| Superblock bytes | Exact 8192-byte vectors | One field/byte mutation | Direct offset decoder | §§8.2–8.3 | COMPLETE |
| Leaf/internal/free bytes | Canonical page vectors | Geometry/reserved mutation | Checked range/byte oracle | §§8.7–8.10, 8.18 | COMPLETE |
| Key codec/order | Boundary/property values | Type/component boundary | Semantic comparator | §§8.3–8.6; Ch. 17 | COMPLETE |
| Physical order | Equal key, varied RIDs | RID boundaries | Numeric RID model | §§8.4, 8.6 | COMPLETE |
| Routing/stale-low | Multi-separator tree | Equality/deletion boundary | Lower-bound relation | §§8.10–8.11 | COMPLETE |
| Leaf/internal split | Variable-byte full nodes | Fit/compaction/promotion | Invariant-set split oracle | §§8.12–8.15 | COMPLETE |
| Duplicate mutation | Multi-leaf duplicate run | Split/redistribute/merge | Ordered multiset | §§8.4, 8.13, 8.17 | COMPLETE |
| Root publication | Root split/contraction | Generation/publication hooks | Old-or-new tree | §§8.15, 8.19.3, 8.26 | COMPLETE |
| Latch/crabbing | Controlled competing workers | Acquisition/safe hooks | Wait-edge/order oracle | §8.19 | COMPLETE |
| Cursor handoff | Multi-leaf scan | Split/merge/reuse barriers | Ordered physical range | §8.20 | COMPLETE |
| BTREE_MTR crash | Every structural family | Reservation/append/publish/crash | Old-or-new tree | §§8.25–8.26; Chs. 12–13 | COMPLETE |
| Append failure | New/reused page | Pre/post-authorizing append | Exact rollback/noncontinuable | §§4.11, 12.12 | COMPLETE |
| L1/owner validation | Canonical page, one defect | Pre-residency stages | Independent parser/context | §§4.13, 8.27 | COMPLETE |
| L3 topology | Valid complex tree, one defect | Verifier traversal | Graph/order model | §8.28 | COMPLETE |
| Detach/reuse | Guarded retired page | Reachability/drain/reuse hooks | Lifetime and graph oracle | §§8.18–8.20; Ch. 7 | COMPLETE |
| Recovery/reopen | Durable/interrupted MTR states | Process crash/restart | Logical model + L3 | §§13.13.3–13.14 | COMPLETE |
| Exhaustion/failure | Isolated terminal/resource states | Owning fault point | Exact error and survivor | §§4.3.2, 8.6, 8.15, 39.1 | COMPLETE |

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
stress testing. Randomized tests explore long operation sequences and broad state space;
they do not replace any deterministic byte, boundary, latch, cursor, fault, or crash case
above.

---

### Concurrent Tests

First run deterministic barrier cases for:

```text
search versus split
split versus split on one path
root metadata versus root-page latch acquisition
safe and unsafe insert/delete crabbing
left/right sibling acquisition and restart
cursor handoff versus split, merge, detach, and reuse
free-list/endpoint snapshot revalidation
guard drain versus page reuse
```

Each case asserts legal and forbidden outcomes directly; no sleep or repeated race is an
ordering mechanism. Then use reproducible stress as complementary coverage:

- many readers + one writer,
- writers on disjoint ranges,
- writers on one hot range,
- duplicate-heavy inserts,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes.

Use deliberately tiny buffer pools in some tests.

Add watchdogs/timeouts as secondary deadlock detectors, while acquisition traces and barriers
remain the primary latch-order oracle.

---

### Chapter 8 architecture-obligation coverage map

The atomic inventory assigns each obligation one primary domain and one primary procedural
owner. Cross-owner references are dependencies, not duplicate ownership.

| Domain | Primary domain | Atomic obligations |
|---|---|---:|
| A | FILESUPERBLOCK | 8 |
| B | PAGE LAYOUT | 6 |
| C | SLOT / ENTRY GEOMETRY | 7 |
| D | KEY CODEC | 11 |
| E | PHYSICAL ORDER | 4 |
| F | ROUTING | 7 |
| G | DUPLICATE HANDLING | 5 |
| H | SEARCH / RANGE SCAN | 5 |
| I | LEAF SPLIT | 7 |
| J | INTERNAL SPLIT | 6 |
| K | ROOT PUBLICATION | 7 |
| L | DELETE / REDISTRIBUTION / MERGE | 7 |
| M | LATCHING / CRABBING | 11 |
| N | CURSOR LIFETIME | 6 |
| O | WAL / BTREE_MTR | 8 |
| P | APPEND / PAGE PUBLICATION | 5 |
| Q | VALIDATION L1 | 9 |
| R | VALIDATION L3 | 11 |
| S | OWNER VALIDATION | 6 |
| T | RECLAMATION / FREE LIST | 7 |
| U | RECOVERY / REOPEN | 7 |
| V | EXHAUSTION / FAILURE | 7 |
| W | UNIQUENESS / MVCC CROSS-OWNER | 6 |
| X | OTHER | 4 |
|  | **Total** | **167** |

| # / domain | Atomic obligation | Architecture owner | Verification owner | Procedure type | Status |
|---:|---|---|---|---|---|
| 1 (A1) | Common 72-byte FileSuperblock prefix is verified byte-for-byte | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 2 (A2) | BTREE extension fields are verified at exact offsets and widths | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 3 (A3) | All BTREE superblock multibyte fields are verified little-endian | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 4 (A4) | BTREE superblock header_size is exactly 128 | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 5 (A5) | Initialized root/first/last PageNos form a legal metadata tuple | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 6 (A6) | free_page_head sentinel and in-range domains are verified | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 7 (A7) | key_schema_version and fingerprint outcomes are distinguished | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 8 (A8) | index_flags and the complete trailing reserved region are zero | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 9 (B1) | Leaf common and node headers occupy the exact 64-byte prefix | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 10 (B2) | Internal common and node headers occupy the exact 64-byte prefix | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 11 (B3) | BTREE_FREE header_size and body boundaries are exact | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 12 (B4) | Leaf level and sibling fields use their canonical byte positions | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 13 (B5) | Internal level and leftmost-child fields use their canonical byte positions | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 14 (B6) | Every page-family required-zero region is tested independently | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 15 (C1) | Slot descriptors are exactly 8 bytes with four uint16 fields | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 16 (C2) | lower equals 64 plus eight times slot_count under checked arithmetic | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 17 (C3) | The 64 <= lower <= upper <= 8192 bounds are enforced | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 18 (C4) | Leaf entry_length equals user_key_length plus 16 | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 19 (C5) | Internal entry_length equals user_key_length plus 24 | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 20 (C6) | Entry payloads cannot overlap the slot directory or page end | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 21 (C7) | Distinct live entry payloads cannot overlap or alias | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle | COMPLETE |
| 22 (D1) | NULL presence encoding establishes NULLS FIRST | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 23 (D2) | BOOLEAN encoded order matches NULL, false, true semantic order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 24 (D3) | INT32 sign transform and big-endian bytes preserve order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 25 (D4) | DATE encoding preserves its canonical semantic domain order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 26 (D5) | INT64 sign transform and big-endian bytes preserve order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 27 (D6) | TIMESTAMP encoding preserves its canonical semantic domain order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 28 (D7) | FLOAT64 infinities and finite values preserve canonical order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 29 (D8) | FLOAT64 negative and positive zero canonicalize identically | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 30 (D9) | All FLOAT64 NaN payloads canonicalize to one ordered class | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 31 (D10) | VARCHAR zero escaping and termination are unambiguous | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 32 (D11) | Composite concatenation preserves component-wise order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 33 (E1) | Physical comparison uses encoded user-key bytes before RID | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 34 (E2) | Equal user keys use numeric FileId/PageNo/SlotId RID order | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 35 (E3) | RID reserved bytes are canonical zero | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 36 (E4) | Exact duplicate physical keys are rejected outside proven replay | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle | COMPLETE |
| 37 (F1) | Internal targets below the first separator route to C0 | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 38 (F2) | Separator equality routes to the separator's right child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 39 (F3) | Between-separator targets route to the bounded child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 40 (F4) | Targets above the last separator route to the last child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 41 (F5) | Routing compares the complete physical separator including RID | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 42 (F6) | Legal stale-low separators preserve reachability and are accepted | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 43 (F7) | Misrouting-high separators are rejected by full-tree validation | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 44 (G1) | Distinct RIDs for one SQL key remain physically representable | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 45 (G2) | Duplicate SQL-key runs remain in RID order | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 46 (G3) | Duplicate runs may cross leaf boundaries without omission | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 47 (G4) | Duplicate-heavy split retains complete physical order | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 48 (G5) | Duplicate redistribution and merge preserve exact-once contents | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 49 (H1) | Leaf exact search is lower_bound-equivalent on physical keys | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 50 (H2) | Equality scan begins at the conceptual MIN_RID bound | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 51 (H3) | Equality scan terminates exactly when the user key changes | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 52 (H4) | Forward range results are ordered and bounded correctly | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 53 (H5) | Every physical candidate remains subject to heap visibility recheck | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle | COMPLETE |
| 54 (I1) | Insert fitting contiguous space avoids split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 55 (I2) | Fragmented sufficient space compacts before split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 56 (I3) | Post-compaction no-fit selects split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 57 (I4) | Leaf split is byte-based for variable-size entries | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 58 (I5) | Both leaf survivors fit, are nonempty, and preserve exact contents | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 59 (I6) | Right-leaf first physical key becomes the parent separator | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 60 (I7) | Leaf sibling and endpoint metadata are updated coherently | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 61 (J1) | Internal split selection is verified by legal byte geometry | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 62 (J2) | The promoted separator is removed from the child level | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 63 (J3) | The promoted separator's child becomes the right leftmost child | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 64 (J4) | All children remain uniquely represented after split | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 65 (J5) | Resulting levels and routing bounds remain valid | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 66 (J6) | Cascading split propagation preserves the same invariants | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 67 (K1) | Root split constructs a complete new internal root privately | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 68 (K2) | Root PageNo and tree height publish with the structural MTR | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 69 (K3) | Readers observe only complete old or complete new root state | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 70 (K4) | root_generation changes with runtime root identity/height | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 71 (K5) | A stale root snapshot detects mismatch and restarts | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 72 (K6) | Root contraction promotes the sole child and decrements height once | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 73 (K7) | The empty tree remains one empty leaf at height one | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 74 (L1) | Ordinary deletion may retain a legal stale-low separator | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 75 (L2) | Sparse nodes below the soft threshold are not corruption | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 76 (L3) | Redistribution in either direction updates the crossing boundary | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 77 (L4) | Leaf merge moves right into left and repairs links/endpoints | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 78 (L5) | Parent separator and right-child removal are coherent | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 79 (L6) | Internal rebalance preserves canonical child/separator sequence | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 80 (L7) | Merged pages detach before entering BTREE_FREE state | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle | COMPLETE |
| 81 (M1) | Read traversal acquires parent before child | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 82 (M2) | Latch coupling retains parent until the child is pinned and latched | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 83 (M3) | Root metadata is released before waiting on a root page latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 84 (M4) | Root identity/generation is revalidated under the page latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 85 (M5) | Structural page latches precede root metadata publication latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 86 (M6) | Adjacent-page acquisition is left to right | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 87 (M7) | A required reverse-order sibling acquisition releases and restarts | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 88 (M8) | Insert-safe children permit release of unnecessary ancestors | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 89 (M9) | Insert-unsafe children retain ancestors needed for propagation | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 90 (N1) | Forward handoff retains the current guard until next is validated | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 91 (N2) | Borrowed key/entry views expire with their leaf guard | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 92 (N3) | Cursor handoff versus split preserves physical scan progress | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 93 (N4) | Cursor handoff versus merge reaches a valid chain or restarts | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 94 (N5) | Cursor handoff versus detach/reuse cannot dereference replacement state | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 95 (N6) | Long-lived unguarded raw PageNo cursors are rejected | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 96 (O1) | Every persistent B+ mutation participates in one owning BTREE_MTR | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 97 (O2) | One structural mutation covers every affected page and metadata item | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 98 (O3) | Affected pages receive the canonical common MTR LSN semantics | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 99 (O4) | No-flush ownership prevents provisional bytes from escaping | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 100 (O5) | Known pre-append failure restores the exact old structural state | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 101 (O6) | Authorizing append uncertainty never rolls back and continues | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 102 (O7) | Runtime publication exposes the complete MTR as one boundary | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 103 (O8) | WAL-before-data remains mandatory for every B+ page write | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 104 (P1) | A newly appended BTREE page remains private before publication | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 105 (P2) | PAGE_INIT and structural MTR authority cover the new page | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 106 (P3) | The owning file bound and structural reachability agree | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 107 (P4) | Pre-authorizing append failure restores/truncates the unpublished tail | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 108 (P5) | Post-authorizing publication failure follows completion or noncontinuable rules | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 109 (Q1) | Leaf header, geometry, and required-zero fields receive L1 validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 110 (Q2) | Leaf entries receive checked length and nonoverlap validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 111 (Q3) | Leaf keys and RIDs receive canonical codec validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 112 (Q4) | Leaf strict physical order and exact-duplicate rejection are L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 113 (Q5) | Internal header, geometry, and required-zero fields receive L1 validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 114 (Q6) | Internal separators and children receive local codec/domain validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 115 (Q7) | Internal strict separator order is L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 116 (Q8) | BTREE_FREE exact header, link domain, and zero body receive L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 117 (Q9) | Recognizable unsupported file/page/key-schema versions bypass v1 parsing | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 118 (R1) | A valid multi-level duplicate/stale-low/free-list fixture is accepted | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 119 (R2) | Child-reference cycles are rejected with bounded progress | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 120 (R3) | Duplicate live parentage is rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 121 (R4) | Subtree key-range violations are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 122 (R5) | All leaves at unequal depths are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 123 (R6) | Global leaf physical-order violations are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 124 (R7) | Sibling reciprocity, cycles, and disconnection are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 125 (R8) | First/last endpoint mismatches are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 126 (R9) | Unauthorized orphan live pages are reported | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 127 (R10) | Free/live overlap is rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 128 (R11) | Free-list cycles and duplicate membership are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle | COMPLETE |
| 129 (S1) | Registered FileKind and FileId match the loaded file/page | §§4.10.2, 4.13.1, 8.2–8.4, 8.27; §§16.5.4–16.6 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 130 (S2) | Common object_id matches the expected IndexId | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 131 (S3) | Persisted TableId matches the index descriptor | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 132 (S4) | Schema fingerprint/version match the resolved key descriptor | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 133 (S5) | Leaf RID heap FileId matches the indexed relation | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 134 (S6) | Valid BTREE bytes from another index file are rejected | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle | COMPLETE |
| 135 (T1) | A page is removed from parent/root reachability before free | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 136 (T2) | Sibling and endpoint reachability are removed before free | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 137 (T3) | New traversals cannot reach a detached page | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 138 (T4) | Existing guards/pins drain before replacement state is exposed | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 139 (T5) | BTREE_FREE publication and free-list membership are coherent | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 140 (T6) | Free-page reuse completely removes old free metadata | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 141 (T7) | An old asynchronous completion cannot alter a reused page | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle | COMPLETE |
| 142 (U1) | Leaf split reopens with exact-once contents and coherent links | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 143 (U2) | Internal/root split reopens with coherent levels/root metadata | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 144 (U3) | Merge/root contraction reopens with detached pages unreachable | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 145 (U4) | Free-page reuse reopens with exclusive free-or-live ownership | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 146 (U5) | Endpoint changes reopen consistent with the leaf chain | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 147 (U6) | Interrupted structural MTR recovers as exact old or complete new tree | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 148 (U7) | Repeated recovery is idempotent and does not persist generation drift | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle | COMPLETE |
| 149 (V1) | Maximum legal height remains a valid tree | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 150 (V2) | Growth beyond legal height fails before partial root publication | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 151 (V3) | Attempted oversized key differs from malformed persisted key length | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 152 (V4) | Page-local no-space selects compaction/split rather than disk exhaustion | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 153 (V5) | NO_REPLACEABLE_FRAME differs from RESOURCE_FULL and numeric exhaustion | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 154 (V6) | Known WAL failure preserves exact pre-authorized tree state | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 155 (V7) | Uncertain authorizing append enters the noncontinuable lifecycle | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 156 (W1) | Physical distinct-RID duplicates do not decide SQL uniqueness | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 157 (W2) | Physical equality enumeration supplies all uniqueness candidates | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 158 (W3) | Logical unique-key locking and current-owner decisions remain Chapter 11 owned | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 159 (W4) | Index presence does not decide MVCC visibility | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 160 (W5) | Structurally valid stale entries are not corruption by themselves | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 161 (W6) | Index entry removal alone does not authorize RID reuse | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle | COMPLETE |
| 162 (X1) | Ordinary B+ pages use BufferPool-managed PageId/guard lifetime | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle | COMPLETE |
| 163 (X2) | No specialized index path creates a second cache owner | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle | COMPLETE |
| 164 (X3) | Production operations are compared to an independent ordered model | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle | COMPLETE |
| 165 (X4) | Randomized and stress coverage remains complementary to deterministic cases | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle | COMPLETE |
| 166 (M10) | Delete-safe and delete-unsafe children release or retain ancestors according to propagation risk | §8.19.2 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |
| 167 (M11) | Optimistic ancestor release is followed by required structural revalidation or restart | §8.19.2 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle | COMPLETE |

Coverage inventory: `167 COMPLETE`, `0 PARTIAL`, `0 MISSING`, and
`0 CONTRADICTORY`.

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

### Transaction identity, snapshot, and status verification

This section is the detailed procedural owner for the transaction identity, snapshot,
transaction-status, and terminal-publication contracts in
[`ARCHITECTURE.md`](ARCHITECTURE.md) §§9.1–9.16. Numeric Exhaustion and
Terminal-Boundary Verification remains the detailed owner for TxnId reservation/exhaustion
and CommandId boundaries; the COMMIT, ABORT, lifecycle, recovery, isolation, and vacuum
sections below remain the detailed owners for their existing cross-layer procedures. The
procedures here add the Chapter-9-specific byte, lookup, lifetime, and race oracles without
redefining tuple visibility (§10), write/uniqueness conflicts (§11), WAL semantics
(Chapter 12), recovery outcomes (Chapter 13), or read-epoch reclamation (Chapter 14).

#### Deterministic harness, observability, and independent oracles

Transaction concurrency verification uses controlled scheduling and barriers rather than
elapsed-time sleeps or repeated stress until a race occurs. The harness can pause and record
these semantic events without requiring production function names:

```text
TxnId reservation begins / control high-water becomes durable / TxnId is issued
transaction registration begins / active-registry publication completes
snapshot high-water is read / active set is captured / snapshot registration completes
terminal status-page mutation begins / terminal WAL append completes
terminal WAL becomes durable
runtime terminal-cache publication / runtime state transition / active removal
snapshot unregister / read-only terminal cleanup
status-page PAGE_INIT reservation / append / page-bound publication
crash or injected failure
recovery image reconstruction / terminal redo / loser resolution / READY publication
```

The event log records TxnIds, isolation and CommandId, runtime states, snapshot fields,
active-registry membership, terminal-cache result, transaction `last_wal_lsn` and
persistent/current-statement write flags, durable WAL end, status PageId and bytes, file
published bound, held logical locks/gates, cancellation/deadlock state, snapshot-horizon
registration, database health, and client result. Every race below forces both legal orders
at the owning linearization point. Random stress remains complementary; it is not a
substitute for these schedules.

Test-side oracles are independent:

- TxnId-to-status mapping is computed from mathematical integers and checked before
  conversion to persisted widths;
- snapshot membership is an expected ordered set built from the harness-controlled states,
  not copied from the production snapshot;
- terminal-race outcomes follow the barrier order and §9.14 linearization, not the state
  returned by the implementation under test;
- recovery outcomes derive from the independently constructed valid durable WAL prefix, not
  from resident status bytes or the precrash API result.

Every valid fixture is canonical except for the one defect under test. Valid status fixtures
have the correct file/page identity, published bound, checksum, version, flags/reserved
bytes, PAGE_INIT history, and registered owner. A status-lookup test cannot accidentally use
a corrupt page, and a corruption test changes only its selected dimension.

#### TXN_STATUS persisted bytes, capacity, and mapping

The generic FileSuperblock codec and registered-owner procedures in Storage Verification and
§§4.10, 4.13.6 verify the `txn_status.dat` specialization with this matrix:

| Region/property | Canonical fixture | Independent mutation/oracle |
|---|---|---|
| Page 0 framing | Exact 8192-byte FileSuperblock; `FileKind::TXN_STATUS=5`; valid FileId; `object_id=0`; page size/version/header fields from §4.10 | Mutate kind, FileId, object identity, version, header size, flags, checksum, or each required-zero field/range independently |
| Trailing superblock bytes | Generic 72-byte header followed by canonical zero bytes | Walk the complete required-zero suffix with one nonzero byte per fixture |
| Physical/published bound | Aligned file length and §12.12-reconciled process-local `published_page_count`; this is not invented as a superblock field | Truncated, overlong, misaligned, and publication/WAL-disagreeing fixtures never expose an unowned page |
| Data-page framing | 32-byte common header; `PageType::TXN_STATUS=7`; no specialized header; payload bytes `32..8191` | Mutate PageId, type, version, `header_size`, flags/reserved, checksum, page length, and published-bound relation one at a time |

Ordinary lookup validates registered file/owner and published PageNo, performs an exact read,
dispatches file/page version, verifies checksum and common PageId/header fields, verifies
TXN_STATUS type, and only then reads the two-bit payload. A recognized future file/page
version produces the owning unsupported-format result. A supported-v1 framing/checksum,
identity, range, or reserved-zero defect produces the owning corruption/open/recovery
failure. The valid semantic code `INVALID` is neither corruption nor an unsupported format.

The test oracle independently recomputes:

```text
payload bytes       = 8192 - 32 = 8160
entries per byte    = 4
entries per page    = 8160 * 4 = 32640

ordinal             = txn_id - FIRST_NORMAL_TXN_ID
status_page_no       = 1 + ordinal / 32640
entry_in_page        = ordinal % 32640
payload_byte_index   = entry_in_page / 4
two_bit_index        = entry_in_page % 4
page_byte_offset     = 32 + payload_byte_index
bit_shift            = 2 * two_bit_index
```

All subtraction, division, addition, and PageNo/file-offset conversion uses checked
mathematical arithmetic. Reserved TxnIds `0` and `1`, values above the maximum normal TxnId,
and any result outside the v1 physical PageNo domain are rejected before page access.

The two-bit test is the Cartesian product of every status code and position:

| Position | Shift | Mask | Codes encoded/decoded independently |
|---:|---:|---:|---|
| 0 | 0 | `0x03` | `00 INVALID`, `01 COMMITTED`, `10 ABORTED`, `11 RESERVED` |
| 1 | 2 | `0x0c` | all four codes |
| 2 | 4 | `0x30` | all four codes |
| 3 | 6 | `0xc0` | all four codes |

For each of the 16 cases, prefill the other six bits with both zero and nonzero patterns,
replace only the selected two bits, and assert that neighboring statuses remain unchanged.
Decode through an independent mask/shift oracle so production encode/decode are not each
other's oracle.

The required boundary/status mapping matrix is:

| TxnId/case | Ordinal | PageNo | Entry | Byte offset | Shift | Status fixture | Expected observation/result |
|---|---:|---:|---:|---:|---:|---|---|
| `2` | 0 | 1 | 0 | 32 | 0 | INVALID plus active registry | First normal TxnId; lookup is IN_PROGRESS |
| `3` | 1 | 1 | 1 | 32 | 2 | COMMITTED | Second bit position; lookup is COMMITTED |
| `5` | 3 | 1 | 3 | 32 | 6 | ABORTED | Fourth bit position; lookup is ABORTED |
| `6` | 4 | 1 | 4 | 33 | 0 | RESERVED | Next payload byte; recognized nonterminal result |
| `32640` | 32638 | 1 | 32638 | 8191 | 4 | INVALID plus active registry | Penultimate first-page entry; lookup is IN_PROGRESS |
| `32641` | 32639 | 1 | 32639 | 8191 | 6 | COMMITTED | Last first-page entry; lookup is COMMITTED |
| `32642` | 32640 | 2 | 0 | 32 | 0 | ABORTED | First next-page entry; extension then ABORTED lookup |
| `32643` | 32641 | 2 | 1 | 32 | 2 | RESERVED | Adjacent next-page entry; recognized nonterminal result |
| `18,446,744,073,708,503,041` | `18,446,744,073,708,503,039` | `565,157,600,297,442` | 28,799 | 7,231 | 6 | COMMITTED | Maximum legal mapping; lookup is COMMITTED |
| Normal TxnId at/above durable allocation end | formula result, if physically represented | formula result | formula result | formula result | formula result | INVALID | Unallocated/high-water-invalid; never guessed ABORTED or allocated |
| Allocated nonretired TxnId on malformed page | formula result | published mapped page | formula result | formula result | formula result | untrusted | Owning corruption/open/recovery failure before status decode |

The maximum calculation is recomputed from the §4.3.2.1 block sequence, not copied from
production constants. The oracle additionally proves status PageNo
`565,157,600,297,442` is below maximum physical PageNo
`1,125,899,906,842,622`. The universal B-1/B0/B+1 procedure remains the no-wrap and
`TXN_ID_EXHAUSTED` owner.

A freshly initialized status payload decodes as `INVALID`, but interpretation also consumes
the durable allocation high-water and runtime context:

- an entry at or above `reserved_txn_id_end` must remain `INVALID` and is not evidence of an
  allocated or aborted transaction;
- an allocated active TxnId with `INVALID` persistent bits is `IN_PROGRESS` through the
  active registry, not ABORTED;
- an allocated, nonactive status-dependent reference with only `INVALID` or `RESERVED`
  after completed recovery is the §9.11.1/§9.13 invariant failure, not a guessed outcome;
- `RESERVED` is recognized v1 nonterminal state, never COMMITTED or ABORTED and never an
  unknown-format value.

#### Snapshot registration, representation, and lifetime

The BEGIN/snapshot harness pauses a new transaction immediately before and after TxnId issue
and active-registry publication, while another transaction pauses at high-water capture and
active-set capture under §9.8's shared synchronization. It forces these orders:

1. **Registration wins.** The new nonterminal TxnId is below captured `xmax` and appears in
   `active` unless it is the owner.
2. **Capture wins.** The new TxnId is assigned at or above the captured `xmax`; it need not
   appear in `active` and is too new for that snapshot.

For an ordinary writable BEGIN, instrumentation also asserts that registration, rather than
a persistent RESERVED/IN_PROGRESS status mutation, represents the new nonterminal
transaction. The valid status page, WAL end, and file bound remain unchanged by BEGIN.

The explicit forbidden oracle is:

```text
txn_id < snapshot.xmax
and transaction was nonterminal at capture
and txn_id != snapshot.owner_txn_id
and txn_id not in snapshot.active
```

This state fails the test because it would let a live transaction fall through both the
active and too-new classifications. The synchronization may be held only for the capture or
registration bookkeeping; instrumentation also asserts it is released before statement
execution.

Snapshot representation uses these canonical deterministic fixtures:

| Case | Controlled state at capture | Expected `active` | Expected `xmin` | Stability oracle |
|---|---|---|---:|---|
| No other active | owner 50, `xmax=60` | `[]` | 60 | Later BEGIN/terminal events do not alter fields |
| One lower active | active 20, owner 50, `xmax=60` | `[20]` | 20 | Membership retained after 20 later commits |
| Multiple active | 30, 10, 40; owner 50; `xmax=60` | `[10,30,40]` | 10 | Sorted vector remains immutable |
| Owner would be minimum | owner 10, other active 30, `xmax=60` | `[30]` | 30 | SELF is represented only by owner/command fields |
| Sparse TxnIds | active 2 and 1,048,578; owner 500; larger `xmax` | `[2,1048578]` | 2 | Reservation gaps create no synthetic members |
| BEGIN before capture | new 40 registered, owner 50, `xmax>40` | includes 40 | ordered minimum | Registration-wins oracle |
| Capture before BEGIN | captured `xmax=60`, new TxnId `>=60` | excludes new | unchanged | High-water-wins oracle |
| Terminal before capture | terminal cache/state published first | excludes terminal TxnId | from remaining set | Lookup observes terminal result |
| Terminal after capture | TxnId nonterminal at capture | retains TxnId | captured value | No retroactive removal |

V1 requires the sorted-vector representation but not one lookup routine. Tests assert exact
sorted membership, owner exclusion, snapshot stability, and §9.7.3 `xmin`; they do not
require `std::binary_search` or any other production helper. Alternative high-concurrency
active-set representations are outside the v1 test obligation.

After capture, controlled BEGIN, COMMIT, and ABORT operations mutate the live registry while
the captured `xmax`, `active`, and `xmin` remain byte/value stable. For REPEATABLE READ, only
the architecture-authorized `command_id` boundary changes between statements.

Isolation-identity cases begin transactions with no isolation clause, explicit READ
COMMITTED, explicit REPEATABLE READ, and each deferred §9.5 mode. They assert that omission
selects READ COMMITTED, the two supported identities reach the matching procedures below,
and deferred modes are rejected rather than silently mapped to a supported level. Public
metadata and diagnostics identify REPEATABLE READ as snapshot isolation and never as
SERIALIZABLE. The existing Isolation Tests remain the owner of behavioral visibility and
write-skew outcomes.

READ COMMITTED procedures hold one registered statement snapshot for the complete attempt,
including scans and result production. SUCCESS and
`FAILED_TRANSACTION_REMAINS_ACTIVE` unregister it and consume the CommandId; the next
statement obtains a fresh snapshot. A permitted §15.7 pre-write retry unregisters the old
attempt snapshot, captures a fresh snapshot, and retains the same logical CommandId. A
post-write conflict follows MUST_ABORT/ABORT and cannot reuse the snapshot.

REPEATABLE READ procedures assert no transaction snapshot exists merely because BEGIN
returned. The first ordinary statement captures and registers it; later statements retain
the same `xmax`, `active`, `xmin`, and owner while advancing the command boundary. It remains
registered through terminal transaction cleanup. Snapshot registration is observed by the
§14.2 global horizon: early unregister is a failure, and terminal/statement cleanup must not
leak a registration. SQL snapshot lifetime remains distinct from §14.6's RID read-epoch
guard.

#### Terminal publication versus snapshot capture

For COMMIT and ABORT separately, pause immediately before and after §9.14.1's atomic runtime
terminal publication and race a snapshot capture through the same synchronization domain.
Durable WAL/status prerequisites are arranged by the existing COMMIT/ABORT procedures; this
test observes only Chapter 9's cache/state/registry linearization.

| Race order | Expected snapshot/lookup result | Forbidden result |
|---|---|---|
| COMMITTED publication completes before capture | TxnId absent from `active`; terminal cache returns COMMITTED | Absent from active while lookup remains IN_PROGRESS |
| Capture completes before COMMITTED publication | Nonterminal TxnId `<xmax` remains captured in `active`, even after later commit | Retroactive removal from captured snapshot |
| ABORTED publication completes before capture | TxnId absent from `active`; terminal cache returns ABORTED | New snapshot records terminal TxnId as active |
| Capture completes before ABORTED publication | TxnId remains in captured `active`; later status lookup may return ABORTED | Captured membership mutates after abort |

At publication, the harness can retain a test observation of the old active-registry entry
while the runtime terminal cache is already installed. Direct lookup must return the cache's
COMMITTED/ABORTED value. This catches stale registry state overriding a terminal result.
Logical locks/gates remain unavailable to competitors until terminal publication; their
detailed waits remain Chapter-11 verification.

#### Transaction-status lookup precedence

Use canonical valid pages and independently control each runtime source. The mandatory
precedence matrix is:

| Case | FROZEN | SELF | Terminal cache | Active registry | Below retired cutoff | Persisted bits | Expected | Forbidden |
|---|---:|---:|---|---|---:|---|---|---|
| Frozen sentinel | yes | no | any | any | any | inaccessible | COMMITTED/frozen result | Page access |
| Current transaction | no | yes | none | active | no | INVALID/RESERVED | SELF | Persistent status overriding self |
| Cached commit with stale active | no | no | COMMITTED | stale active | no | any older value | COMMITTED | IN_PROGRESS |
| Cached abort with stale active | no | no | ABORTED | stale active | no | any older value | ABORTED | IN_PROGRESS/COMMITTED |
| Active ordinary TxnId | no | no | none | active | no | INVALID | IN_PROGRESS | Guessed ABORTED |
| Retired history | no | no | none | absent | yes | page may be punched | RETIRED | Missing-page corruption or guessed COMMITTED |
| Persisted commit | no | no | none | absent | no | COMMITTED | COMMITTED | IN_PROGRESS |
| Persisted abort | no | no | none | absent | no | ABORTED | ABORTED | COMMITTED |
| Persisted INVALID | no | no | none | absent | no | INVALID | nonterminal/invariant result defined by context | Guessed terminal result |
| Persisted RESERVED | no | no | none | absent | no | RESERVED | recognized nonterminal/invariant result | Corruption or terminal result |

RETIRED is tested only after the Chapter-14 status-history protocol durably publishes the
cutoff and proves every surviving persistent correctness object status-independent. A
missing or punched page without that proof is never synthesized as RETIRED. FROZEN and SELF
tests instrument storage access and require no status-page fetch. Persisted terminal tests
have valid owner/checksum/PageNo bytes and no earlier runtime source.

#### Status-page extension, failure, and recovery

The first terminal status mapped to an unpublished TXN_STATUS PageNo uses the generic
PAGE_INIT and §12.10.5 protocol. The harness records private page construction, PAGE_INIT
append, file-bound publication, terminal-record append, status-bit installation, and
writeback eligibility. Before bound publication ordinary lookup cannot fetch the page; after
publication the page is canonical and its first terminal update follows PAGE_INIT in WAL.

Run the same procedure at the last entry of status page `N` and first entry of `N+1`.
The former performs no extension; the latter selects exactly the next absolute PageNo,
zero-initializes all 8,160 payload bytes to INVALID, and changes only the mapped two bits.

| Case | Bound/WAL/page fixture | Injected boundary | Required outcome | READY? |
|---|---|---|---|---:|
| First status page | Bound contains only page 0 | PAGE_INIT then first terminal record | Page 1 publishes once; terminal bit and page_lsn follow WAL order | after valid completion |
| Existing-page last entry | Page N published | Last mapped two bits | No extension; neighboring bits unchanged | yes |
| Next-page first entry | N published, N+1 absent | PAGE_INIT for N+1 | Exact next PageNo; zero payload then terminal bit | after valid completion |
| Known pre-PAGE_INIT failure | Old bound and no authorizing append | Reservation/construction/known append failure | Exact old file/bound/frame/WAL state; private page invisible | yes if generic rollback succeeds |
| Post-authorizing failure | Valid PAGE_INIT or terminal append may exist | Runtime publication/status install failure | Retained completion/recovery or DATABASE_NONCONTINUABLE; no rollback-and-continue | no ordinary continuation until resolved |
| Durable COMMIT, stale page | Commit in durable valid WAL prefix | Crash before status/data flush | Reconstruct/redo COMMITTED | yes after repair |
| Durable ABORT, stale page | Abort terminal record survives | Crash before status flush | Reconstruct/redo ABORTED | yes after repair |
| Torn status page | Bad checksum plus retained image/terminal WAL | Recovery page read | Ignore stored bits/LSN; restore image then terminal redo | yes after valid repair |
| Unrecoverable malformed page | Bad page and no required retained reconstruction base | Recovery | Exact corruption/RECOVERY_FAILED result; no guessed statuses | no |
| Active writer at crash | No surviving terminal COMMIT/ABORT | Analysis/loser resolution | ABORTED through canonical recovery terminal protocol | yes after loser completion |

Crash prefixes cover PAGE_INIT append, terminal append, status-bit installation, page flush,
and client acknowledgement. A durable COMMIT with stale/unflushed status, heap, and index
pages recovers COMMITTED. A crash after durable COMMIT but before client acknowledgement
also recovers COMMITTED and records client knowledge as uncertain. A stale status image never
overrides terminal WAL; a torn page contributes neither trusted bits nor `page_lsn`.

#### Read-only transaction specialization

Read-only verification reuses ordinary transaction admission and snapshot procedures while
asserting the no-terminal-WAL/status specialization:

| Phase | TxnId | Active registry | Snapshot/horizon | Terminal WAL | Terminal status entry | Required result |
|---|---:|---:|---|---:|---:|---|
| BEGIN | yes | yes | none until owning statement boundary | no | no | ACTIVE transaction participates in registration race |
| READ COMMITTED statement | yes | yes | registered statement snapshot | no | no | Same capture/lifetime rules as writable RC |
| REPEATABLE READ first statement | yes | yes | transaction snapshot registered | no | no | Snapshot begins here, not merely at BEGIN |
| REPEATABLE READ later statement | yes | yes | same membership/horizon, new command boundary | no | no | Long-lived snapshot may hold global horizon |
| Successful completion | yes | removed at runtime terminal cleanup | snapshot unregistered | no | no | Successful transaction result after cleanup |
| Abort/failure | yes | removed only through runtime abort cleanup | snapshot unregistered | no persistent terminal record required | no | No leaked active/snapshot ownership |
| Long-running reader | yes | yes | registered RR/statement snapshot | no | no | §14.2 reclamation remains blocked where required |
| Crash while active | durably reserved identity | process-local registry disappears | no surviving snapshot | no | no | No invented durable read-only commit/status fact |

The long-running case cross-checks the Vacuum and Reclamation Tests: version/status history
needed by the registered snapshot is retained. It does not equate the SQL snapshot with a
§14.6 ReadEpochGuard. Durable reservation still prevents TxnId reuse even though no terminal
record exists.

#### Chapter-9 procedural matrices

The error/result matrix keeps semantic status codes, runtime results, and failures distinct:

| Condition | Expected category | Detailed owner |
|---|---|---|
| Exact next TxnId block unavailable | `TXN_ID_EXHAUSTED` | Numeric Exhaustion — TxnId terminal block |
| Statement after final CommandId | `COMMAND_ID_EXHAUSTED` | Numeric Exhaustion — CommandId specialization |
| Persisted `INVALID` | Valid nonterminal status code interpreted with runtime/high-water/recovery context | §§9.11–9.13 procedures above |
| Persisted `RESERVED` | Recognized v1 nonterminal status | RESERVED and lookup procedures above |
| Active registry winner | `IN_PROGRESS` runtime result | Lookup precedence |
| Proven retired history | `RETIRED` runtime result | Lookup precedence plus Chapter 14 |
| Malformed supported-v1 page | Owning corruption/open/recovery failure | Status-byte validation |
| Recognizable future format | Owning unsupported-format result | §4.14 dispatch tests |
| Known terminal-WAL failure | Exact retained-state retry/failure outcome | COMMIT/ABORT Fault-Injection Tests |
| Uncertain append/publication | `DATABASE_NONCONTINUABLE` | Non-Crash WAL/MTR and §39.1 |
| Recoverable effect-free statement failure | `FAILED_TRANSACTION_REMAINS_ACTIVE` | Statement Failure tests |
| Transaction-fatal statement | `FAILED_TRANSACTION_MUST_ABORT` and automatic abort | Statement Failure/ABORT tests |

The deterministic concurrency matrix is:

| Scenario | Barrier/linearization | Legal survivors | Forbidden survivor | Oracle/owner |
|---|---|---|---|---|
| BEGIN vs capture | §9.8 registry/high-water synchronization | registered below `xmax` and active, or assigned at/above `xmax` | below `xmax`, nonterminal, absent | Independent expected set |
| COMMIT vs capture | §9.14 runtime terminal publication | terminal-before capture, or captured active-before terminal | retroactive active removal | Terminal race matrix |
| ABORT vs capture | §9.14 runtime terminal publication | terminal-before capture, or captured active-before terminal | retroactive active removal | Terminal race matrix |
| Cache vs stale registry | terminal cache publication observed first | COMMITTED/ABORTED cache result | IN_PROGRESS | Lookup precedence |
| RC attempt vs next statement | snapshot unregister/next capture | stable old attempt, fresh next snapshot | membership mutation or snapshot reuse | RC lifetime procedure |
| RR snapshot vs later commit | first-statement capture | original active/horizon retained | later commit added to visible past | RR lifetime plus Chapter 10 |
| Unregister vs reclamation | snapshot registry removal | no reclaim before legal unregister | early horizon advance | Vacuum/reclamation cross-check |
| Shutdown vs BEGIN | READY→DRAINING admission gate | admitted-before drain or rejected-after gate | new transaction after gate | Database Lifecycle Tests |

The high-level domain/case matrix is:

| Domain | Deterministic fixture | Barrier/fault | Independent oracle | Architecture | Status |
|---|---|---|---|---|---|
| TxnId reservation | Exact block/high-water fixture | control write/sync/issue | durable exclusive end | §§4.3.2.1, 9.2–9.3 | COMPLETE |
| TxnId exhaustion | Terminal exact block | next reservation | mathematical block sequence | §§4.3.2.1, 9.3 | COMPLETE |
| CommandId | 0/max/failed/retry fixture | statement admission/end | checked command sequence | §§4.3.2.2, 9.6 | COMPLETE |
| Status bytes | Canonical page vectors | one-field/bit mutation | independent byte/mask codec | §§4.13.6, 9.11–9.12 | COMPLETE |
| Status mapping | Boundary TxnIds | page/bit access | mathematical formula | §9.12 | COMPLETE |
| BEGIN/capture | Two controlled transactions | shared registry/high-water sync | expected ordered set | §9.8 | COMPLETE |
| Snapshot representation | Empty/owner/sparse/multiple sets | capture | set/sort/min oracle | §§9.7–9.8 | COMPLETE |
| Isolation identity | Default/RC/RR/deferred modes | transaction admission/diagnostic | exact admitted identity | §9.5 | COMPLETE |
| RC lifetime | Two statements and retry | register/unregister | snapshot identity/event log | §9.9 | COMPLETE |
| RR lifetime | First/later statements | first capture/terminal cleanup | fixed fields plus command boundary | §9.10 | COMPLETE |
| COMMIT/capture | Durable writer and capturer | §9.14 publication | barrier order | §§9.14.1–9.14.2 | COMPLETE |
| ABORT/capture | Aborting writer and capturer | §9.14 publication | barrier order | §§9.14.1, 9.14.3 | COMPLETE |
| Lookup precedence | Controlled runtime/persistent sources | each precedence stage | explicit matrix | §9.13 | COMPLETE |
| Status PAGE_INIT | First/next status page | append/publication faults | old/new bound and WAL order | §§9.12, 12.10.5 | COMPLETE |
| Stale status vs WAL | Durable terminal records, stale page | crash/reopen | durable WAL model | §§12.10.5, 13.13.2–13.17 | COMPLETE |
| Torn status | Bad checksum plus retained image | recovery | image plus terminal redo | §§13.14, 13.17 | COMPLETE |
| Active loser | No terminal record | crash/analysis | WAL terminal evidence | §13.15 | COMPLETE |
| Read-only | RC/RR/no-write transactions | completion/crash | event and WAL/status absence | §9.15 | COMPLETE |
| Reclamation horizon | Long-lived read-only snapshot | vacuum/horizon query | registered snapshot set | §§14.2, 14.14 | COMPLETE |

#### Chapter 9 architecture-obligation coverage map

The atomic inventory contains 101 obligations. `COMPLETE` means that the obligation has a
deterministic procedure in this section or a precise existing owner below; an architecture
statement by itself is not counted as verification.

| # | Domain | Atomic obligation | Architecture owner | Verification procedure/reference | Status |
|---:|---|---|---|---|---|
| 1 | TXNID RESERVATION | TxnId width and reserved sentinel values | §§4.3.2.1, 9.2 | Numeric Exhaustion — TxnId domain/boundaries | COMPLETE |
| 2 | TXNID RESERVATION | First normal value and monotonic allocation | §§4.3.2.1, 9.2 | Numeric Exhaustion — TxnId domain/boundaries | COMPLETE |
| 3 | TXNID RESERVATION | Fixed `2^20` reservation block | §§4.3.2.1, 9.3 | Numeric Exhaustion — exact-block procedure | COMPLETE |
| 4 | TXNID RESERVATION | Reservation stores an exclusive durable end | §§4.3.2.1, 9.3 | Numeric Exhaustion — exact-block procedure | COMPLETE |
| 5 | TXNID RESERVATION | No issue before durable high-water; known failure issues none | §9.3 | Numeric Exhaustion — reservation failure procedure | COMPLETE |
| 6 | TXNID RESERVATION | Crash gaps are legal and reserved TxnIds are never reused | §9.3 | Numeric Exhaustion — restart/nonreuse procedure | COMPLETE |
| 7 | TXNID EXHAUSTION | Maximum normal TxnId, no wrap, deterministic next-allocation failure | §§4.3.2.1, 9.3 | Numeric Exhaustion — terminal exact block | COMPLETE |
| 8 | RECOVERY / STATUS RECONCILIATION | Torn control slots recover a valid durable TxnId high-water | §§9.3, 13.2 | Numeric Exhaustion — control-slot crash matrix | COMPLETE |
| 9 | COMMANDID | CommandId is uint32 and zero is the first legal value | §§4.3.2.2, 9.6 | Numeric Exhaustion — CommandId specialization | COMPLETE |
| 10 | COMMANDID | One CommandId is assigned per admitted logical statement | §9.6 | Numeric Exhaustion — statement-boundary sequence | COMPLETE |
| 11 | COMMANDID | Permitted pre-write retry reuses the same logical CommandId | §§9.6, 15.7 | Numeric Exhaustion — retry specialization | COMPLETE |
| 12 | COMMANDID | Success and recoverable failure consume the CommandId | §§9.6, 39.1 | Numeric Exhaustion plus Statement Failure tests | COMPLETE |
| 13 | COMMANDID | `UINT32_MAX` is legal and the next statement is rejected without successor arithmetic | §§4.3.2.2, 9.6 | Numeric Exhaustion — terminal boundary | COMPLETE |
| 14 | COMMANDID | Transaction-control operations do not consume ordinary CommandIds | §9.6 | Numeric Exhaustion — control-operation specialization | COMPLETE |
| 15 | TRANSACTION STATE | Runtime state domain is exact | §9.4 | Statement Failure and Transaction-State Tests | COMPLETE |
| 16 | TRANSACTION STATE | ACTIVE admits statements, commit, and abort as specified | §9.4 | Transaction-State Tests | COMPLETE |
| 17 | TRANSACTION STATE | MUST_ABORT rejects statements/commit and admits abort | §9.4 | Statement Failure and Transaction-State Tests | COMPLETE |
| 18 | TRANSACTION STATE | COMMITTING/ABORTING reject new statements and retain terminal ownership | §§9.4, 9.14 | COMMIT/ABORT Fault-Injection Tests | COMPLETE |
| 19 | TRANSACTION STATE | Every legal lifecycle edge and illegal edge is enforced | §9.4 | Transaction-State Tests transition matrix | COMPLETE |
| 20 | COMMIT | Authorizing commit append forbids transition to ABORTING/ABORTED | §§9.4, 9.14.2 | COMMIT Fault-Injection Tests | COMPLETE |
| 21 | TRANSACTION STATE | COMMITTED and ABORTED are terminal and irreversible | §§3.3.6, 9.4 | Lifecycle plus COMMIT/ABORT tests | COMPLETE |
| 22 | FAILURE / NONCONTINUABLE | Ordinary transaction admission requires READY | §§3.3, 9.4 | Database Lifecycle Tests | COMPLETE |
| 23 | FAILURE / NONCONTINUABLE | DRAINING/NONCONTINUABLE transaction handling preserves terminal facts | §§3.3.5–3.3.6, 9.16, 39.1 | Database Lifecycle and Non-Crash WAL/MTR tests | COMPLETE |
| 24 | BEGIN / REGISTRATION | TxnId issue, active registration, and snapshot high-water share the required synchronization | §§9.3, 9.8 | Snapshot registration race harness | COMPLETE |
| 25 | SNAPSHOT REPRESENTATION | Snapshot contains exactly `xmax`, `active`, `xmin`, owner, and command fields | §9.7 | Snapshot representation matrix | COMPLETE |
| 26 | SNAPSHOT REPRESENTATION | `xmax` is the captured next-unassigned normal TxnId | §§9.7.1, 9.8 | BEGIN/capture two-order procedure | COMPLETE |
| 27 | SNAPSHOT REPRESENTATION | `active` contains every relevant normal nonterminal TxnId below `xmax` | §§9.7.2, 9.8 | Snapshot representation and forbidden-gap oracle | COMPLETE |
| 28 | SNAPSHOT REPRESENTATION | Snapshot owner is excluded from `active` | §§9.7.2, 9.8 | Owner-exclusion fixture | COMPLETE |
| 29 | SNAPSHOT REPRESENTATION | ACTIVE, MUST_ABORT, COMMITTING, and ABORTING are snapshot-active | §§9.4, 9.7.2 | Controlled-state membership fixture | COMPLETE |
| 30 | SNAPSHOT REPRESENTATION | A captured nonterminal remains in `active` after later terminal publication | §§9.7.2, 9.14 | Terminal/capture race matrix | COMPLETE |
| 31 | SNAPSHOT REPRESENTATION | V1 `active` is a sorted vector | §9.7.2 | Sorted empty/owner/sparse/multiple fixtures | COMPLETE |
| 32 | SNAPSHOT REPRESENTATION | Membership implementation may vary without changing sorted-vector semantics | §9.7.2 | Representation-freedom assertion | COMPLETE |
| 33 | SNAPSHOT REPRESENTATION | Nonempty `xmin` is the minimum captured active TxnId | §9.7.3 | Xmin multi-member/owner fixtures | COMPLETE |
| 34 | SNAPSHOT REPRESENTATION | Empty `active` gives `xmin=xmax` | §9.7.3 | Empty-set fixture | COMPLETE |
| 35 | SNAPSHOT REPRESENTATION | Owner and command fields drive self visibility without self membership | §9.7.4 | Owner fixture plus CommandId cross-reference | COMPLETE |
| 36 | SNAPSHOT CAPTURE | High-water and active-set capture are one atomic observation | §9.8 | BEGIN/capture deterministic barriers | COMPLETE |
| 37 | SNAPSHOT CAPTURE | No below-`xmax` nonterminal nonowner can be absent from `active` | §9.8 | Explicit forbidden-classification oracle | COMPLETE |
| 38 | SNAPSHOT CAPTURE | Capture synchronization is not held during query execution | §9.8 | Harness lock-ownership observation | COMPLETE |
| 39 | SNAPSHOT CAPTURE | Captured membership/horizons are immutable | §§9.7–9.10 | BEGIN/COMMIT/ABORT post-capture mutation fixture | COMPLETE |
| 40 | RECLAMATION / HORIZON | Registered SQL snapshots contribute to the global snapshot horizon | §§9.9–9.10, 14.2 | Snapshot registration plus Vacuum/Reclamation tests | COMPLETE |
| 41 | SNAPSHOT LIFETIME | READ COMMITTED captures a fresh snapshot for each statement attempt | §9.9 | Two-statement RC fixture | COMPLETE |
| 42 | SNAPSHOT LIFETIME | RC snapshot remains stable through the complete attempt | §9.9 | In-attempt registry mutation fixture | COMPLETE |
| 43 | SNAPSHOT LIFETIME | RC success/recoverable failure unregisters snapshot and consumes command | §§9.6, 9.9 | RC event-log cleanup fixture | COMPLETE |
| 44 | SNAPSHOT LIFETIME | Allowed pre-write retry refreshes snapshot but reuses logical CommandId | §§9.9, 15.7 | RC retry fixture | COMPLETE |
| 45 | SNAPSHOT LIFETIME | Post-write conflict does not reuse the attempt snapshot | §§9.9, 15.7 | RC conflict/abort cross-reference | COMPLETE |
| 46 | SNAPSHOT LIFETIME | REPEATABLE READ captures on first ordinary statement, not BEGIN | §9.10 | RR BEGIN/first-statement fixture | COMPLETE |
| 47 | SNAPSHOT LIFETIME | RR retains membership/horizon/owner fields across statements | §9.10 | RR multi-statement fixture | COMPLETE |
| 48 | SNAPSHOT LIFETIME | RR updates only the command boundary per statement | §§9.6, 9.10 | RR fixture plus CommandId procedure | COMPLETE |
| 49 | SNAPSHOT LIFETIME | RR registration remains until terminal cleanup | §§9.10, 14.2 | RR terminal/unregister horizon fixture | COMPLETE |
| 50 | STATUS FILE LAYOUT | `txn_status.dat` superblock identity is canonical singleton metadata | §§4.10, 4.13.6, 9.12 | TXN_STATUS superblock specialization matrix | COMPLETE |
| 51 | STATUS FILE LAYOUT | Status data page uses common header, PageType 7, and no specialized header | §§4.13.6, 9.12 | Data-page framing mutation matrix | COMPLETE |
| 52 | STATUS FILE LAYOUT | Payload is 8,160 bytes and capacity is 32,640 entries | §§4.13.6, 9.12 | Independent capacity arithmetic | COMPLETE |
| 53 | STATUS FILE LAYOUT | All four two-bit encodings and four bit positions round-trip independently | §§4.13.6, 9.11–9.12 | 16-case mask/shift matrix | COMPLETE |
| 54 | STATUS FILE LAYOUT | RESERVED is recognized nonterminal v1 state | §§9.11–9.13 | RESERVED fixture and lookup matrix | COMPLETE |
| 55 | STATUS MAPPING | Reserved TxnIds have no ordinary status ordinal | §9.12 | Checked mapping-domain rejection | COMPLETE |
| 56 | STATUS MAPPING | Normal ordinal is `txn_id-2` | §9.12 | Independent mapping oracle | COMPLETE |
| 57 | STATUS MAPPING | Absolute page and entry mapping use 32,640-entry pages | §9.12 | Boundary mapping matrix | COMPLETE |
| 58 | STATUS MAPPING | Payload byte and least-significant-first shift are exact | §9.12 | Four-position encoding matrix | COMPLETE |
| 59 | STATUS MAPPING | Status mutation preserves all neighboring two-bit entries | §§9.12, 12.10.5 | Nonzero-neighbor mutation fixtures | COMPLETE |
| 60 | STATUS MAPPING | Checked maximum mapping fits the physical PageNo domain | §§4.3.2.1, 9.12 | Maximum-TxnId independent arithmetic | COMPLETE |
| 61 | STATUS FILE LAYOUT | Newly initialized payload is canonical INVALID | §§9.11–9.12, 12.10.5 | PAGE_INIT zero-payload fixture | COMPLETE |
| 62 | STATUS FILE LAYOUT | Owner, bound, version, checksum, identity, and reserved bytes validate before payload | §§4.10, 4.13.6 | Validation-order/corruption matrix | COMPLETE |
| 63 | STATUS LOOKUP | INVALID above allocation high-water is not a terminal transaction result | §§9.11–9.13 | High-water INVALID fixtures | COMPLETE |
| 64 | RECOVERY / STATUS RECONCILIATION | Status `page_lsn`/images accelerate reconstruction but terminal WAL is authoritative | §§9.11.1, 12.10.5, 13.17 | Status extension/recovery matrix | COMPLETE |
| 65 | STATUS LOOKUP | FROZEN resolves without status-page access | §9.13 | FROZEN precedence fixture | COMPLETE |
| 66 | STATUS LOOKUP | Current transaction resolves SELF before persistent state | §9.13 | SELF precedence fixture | COMPLETE |
| 67 | STATUS LOOKUP | Runtime terminal cache precedes active and persistent sources | §§9.13–9.14 | Terminal-cache fixture | COMPLETE |
| 68 | STATUS LOOKUP | Terminal cache wins over an intentionally stale active observation | §§9.13–9.14 | Cache-versus-stale-active race | COMPLETE |
| 69 | STATUS LOOKUP | Active ordinary transaction resolves IN_PROGRESS | §9.13 | Active/INVALID fixture | COMPLETE |
| 70 | STATUS LOOKUP | RETIRED requires a durably published Chapter-14 retirement proof | §§9.13, 14.14 | RETIRED/punched-page fixture | COMPLETE |
| 71 | STATUS LOOKUP | Persisted COMMITTED and ABORTED decode after earlier sources miss | §9.13 | Valid persisted-terminal fixtures | COMPLETE |
| 72 | STATUS LOOKUP | INVALID and RESERVED never produce guessed terminal results | §§9.11–9.13 | Lookup precedence matrix | COMPLETE |
| 73 | STATUS LOOKUP | Missing referenced, nonretired status produces the owning invariant/corruption result | §§9.11.1, 9.13 | Missing-page/no-retirement fixture | COMPLETE |
| 74 | TERMINAL PUBLICATION | Snapshot capture and terminal publication use one synchronization domain | §9.14.1 | COMMIT/ABORT capture race barriers | COMPLETE |
| 75 | TERMINAL PUBLICATION | Cache install, terminal state, and active removal are one atomic publication | §9.14.1 | Event-log atomicity assertion | COMPLETE |
| 76 | TERMINAL PUBLICATION | A new snapshot excludes a published terminal TxnId while old snapshots stay immutable | §9.14.1 | Terminal-before/after-capture fixtures | COMPLETE |
| 77 | TERMINAL PUBLICATION | Logical locks/gates release only after terminal publication | §§9.14.1, 11.2 | Terminal-race assertion plus Locking Tests | COMPLETE |
| 78 | COMMIT | Durable COMMIT C3 precedes runtime publication C4 | §9.14.2 | COMMIT C0–C6 Fault-Injection Tests | COMPLETE |
| 79 | COMMIT | Resident status bits alone do not publish runtime commit | §§9.11.1, 9.14.2 | Status/capture event-order fixture | COMPLETE |
| 80 | COMMIT | Durable COMMIT is irreversible despite later failure or lost acknowledgement | §§3.3.6, 9.14.2 | COMMIT faults plus crash-before-ack fixture | COMPLETE |
| 81 | ABORT | Runtime ABORTED publication precedes logical lock release | §9.14.3 | ABORT A0–A4 plus terminal-race fixture | COMPLETE |
| 82 | ABORT | Ordinary abort does not require immediate WAL sync | §9.14.3 | ABORT Fault-Injection Tests | COMPLETE |
| 83 | ABORT | Crash before durable abort is resolved as a loser, not guessed from status bytes | §§9.14.3, 13.15 | Recovery loser fixture | COMPLETE |
| 84 | ABORT | Abort requires no synchronous physical heap/index undo | §§5.6.3, 9.14.3 | ABORT tests plus MVCC/Reclamation cross-check | COMPLETE |
| 85 | READ-ONLY TRANSACTION | Read-only transactions use normal identity/registry/snapshots but no ordinary terminal WAL/status | §9.15 | Read-only phase matrix | COMPLETE |
| 86 | READ-ONLY TRANSACTION | Read-only success, abort/failure, and crash release runtime ownership without invented persistence | §9.15 | Read-only completion/crash fixtures | COMPLETE |
| 87 | STATUS MAPPING | First/new status pages use PAGE_INIT and publish bounds before lookup | §§9.12, 12.10.5 | Status-page extension matrix | COMPLETE |
| 88 | RECOVERY / STATUS RECONCILIATION | Durable COMMIT overrides stale/unflushed status and data pages | §§9.14.2, 13.13.2–13.17 | Durable-COMMIT stale-page crash fixture | COMPLETE |
| 89 | RECOVERY / STATUS RECONCILIATION | Durable ABORT overrides stale status bytes | §§9.14.3, 13.15–13.17 | Durable-ABORT stale-page fixture | COMPLETE |
| 90 | RECOVERY / STATUS RECONCILIATION | Torn status pages are reconstructed from valid images/WAL or recovery fails | §§13.14, 13.17 | Torn/unrecoverable page fixtures | COMPLETE |
| 91 | RECOVERY / STATUS RECONCILIATION | Active transaction without terminal record becomes recovery loser | §13.15 | Active-at-crash fixture | COMPLETE |
| 92 | FAILURE / NONCONTINUABLE | READY follows transaction-status/WAL reconciliation and loser completion | §§3.3, 13.19 | Page extension/recovery matrix plus Lifecycle Tests | COMPLETE |
| 93 | CROSS-OWNER VISIBILITY | Chapter 10 receives stable snapshot, owner, command, and status inputs | §§9.7–9.14, 10.2–10.3 | This section plus MVCC Visibility Tests | COMPLETE |
| 94 | CROSS-OWNER VISIBILITY | Chapter 11 remains owner of write/write and transactional uniqueness conflicts | §§9.14.1, 11.2–11.4 | Terminal input assertion plus Locking/UNIQUE Tests | COMPLETE |
| 95 | RECLAMATION / HORIZON | SQL snapshot horizon, RID read epoch, and status retirement remain distinct and coordinated | §§9.9–9.10, 14.2, 14.6, 14.14 | Snapshot lifetime plus Vacuum/Reclamation Tests | COMPLETE |
| 96 | OTHER | READ COMMITTED and REPEATABLE READ are the supported identities and READ COMMITTED is the default | §9.5 | Isolation-identity cases plus Isolation Tests | COMPLETE |
| 97 | OTHER | REPEATABLE READ is identified as snapshot isolation, never SERIALIZABLE | §9.5 | Isolation-identity diagnostics plus write-skew case | COMPLETE |
| 98 | OTHER | Deferred isolation/concurrency modes are not silently admitted as v1 modes | §9.5 | Isolation-identity rejection cases | COMPLETE |
| 99 | CROSS-OWNER VISIBILITY | WAL/durability and recovery mechanics remain owned by Chapters 12–13 while Chapter 9 supplies lifecycle inputs | §9.1 | COMMIT/ABORT Fault-Injection and Recovery Property Tests | COMPLETE |
| 100 | BEGIN / REGISTRATION | Ordinary BEGIN uses runtime registration and does not require persisted RESERVED/IN_PROGRESS status | §§9.8, 9.11, 9.11.1 | BEGIN event-log/status-absence fixture | COMPLETE |
| 101 | TRANSACTION STATE | Required transaction-local semantic resources remain observable through their owning lifetime and cleanup | §9.4 | Harness resource log plus Statement, COMMIT, ABORT, and Lifecycle Tests | COMPLETE |

Coverage totals: `COMPLETE=101`, `PARTIAL=0`, `MISSING=0`, `CONTRADICTORY=0`.

---

### WAL Persistent Codec, Append, and Recovery Verification

This section is the detailed procedural owner for the byte, append, durability,
tail, and transaction-status WAL obligations in [`ARCHITECTURE.md`](ARCHITECTURE.md)
Chapter 12. Numeric Exhaustion and Terminal-Boundary Verification remains the
owner of mathematical end-of-domain and terminal-credit cases; Buffer management
verification owns copied page writeback; B+ Tree Verification owns structural
old-or-complete-new outcomes; the COMMIT, ABORT, lifecycle, checkpoint, and
recovery sections own their existing cross-layer consequences. The procedures
below verify Chapter 12 without redefining those contracts.

#### Deterministic WAL harness and independent oracles

The harness records architecture-level events without requiring a production
class or function name:

```text
segment create requested / final name created / exact size established
segment fdatasync completed / wal-directory fsync completed
candidate reservation proposed / private record encoded / CRC and padding finalized
valid append end published / transaction last_wal_lsn updated
page/frame metadata published / physical WAL write begins or partially completes
durability target requested / segment fdatasync completes / durable_lsn advances
waiter released with success or exact failure
recovery inventory begins / header and payload decode / CRC validation
record accepted or rejected / valid tail established / READY published
```

Every concurrency or crash-prefix fixture uses a barrier, explicit scheduler
gate, injected exact transfer result, or directly constructed persisted prefix.
Elapsed sleeps, timing luck, and repeated random crashes are not correctness
oracles. Random crash/property testing remains complementary.

Four independent test-side models are mandatory:

1. **Byte oracle.** Construct expected little-endian bytes from mathematical
   field values and the architecture tables. Independently calculate header,
   payload, `total_length`, `physical_span`, zero padding, and nested lengths.
   Production encode followed by production decode is not sufficient.
2. **CRC oracle.** Compute CRC32C with a test-side implementation or fixed
   reference vectors over the 48-byte header with bytes `44..47` treated as zero
   plus the exact payload. Alignment padding is excluded and checked separately.
3. **LSN oracle.** Use widened arithmetic for Align8, segment index/offset,
   exclusive end, record placement, and terminal `2^64` reasoning. Production
   reservation helpers are not the oracle.
4. **Recovery-prefix oracle.** Build the expected valid prefix from independently
   accepted complete records and filesystem namespace facts, not from the
   runtime append result or precrash API status.

Each negative fixture is canonical except for one selected defect. A payload
grammar test retains valid outer framing and CRC; a CRC test retains valid
framing and payload; a namespace test changes only the synchronization fact.
This isolation is essential because malformed recognized-v1 WAL, unsupported
format, torn final tail, resource failure, and uncertain runtime authority have
different outcomes.

An ownership-seam fixture instruments WAL segment positional I/O, ordinary
BufferPool page access, and the lower file-service calls. WAL append, flush, and
recovery scanning must use the bounded raw-WAL path without treating a segment
as a BufferPool page; the lower service may report exact I/O completion/failure
but cannot choose append order or advance `durable_lsn`. This verifies the
Chapter-7/12 ownership boundary without prescribing source layout.

#### WAL segment namespace and creation

Construct segment-name fixtures for indexes `0`, `1`, and `2^38-1`. The
independent formatter expects exactly 16 lowercase hexadecimal digits, leading
zeros, and `.wal`. Reject shorter, uppercase, signed, decimal, alternate-suffix,
overwide, and index-`2^38` names through the owning namespace/inventory result.

For segment zero, verify bytes `0..7` are zero, append end begins at LSN `8`, and
no parser attempts to decode that prefix. For every later segment, place a record
at offset zero to prove there is no invented segment header. Every segment fixture
has exact length `67,108,864`; test one byte short, one byte long, and a legal
exact final offset without allocating the complete maximum-index namespace.

Instrument segment creation in this exact observable sequence:

```text
exact next final name created with exclusive/no-follow ownership
ftruncate establishes 67,108,864 bytes
segment fdatasync succeeds
fsync(database_root/wal) succeeds
records in the segment become eligible for a durability proof
```

Fail create, truncate, segment `fdatasync`, and directory `fsync` independently.
No failure may advance `durable_lsn` into that segment or release a durability
waiter successfully. A next-contiguous exact-size all-zero survivor is adoptable
only after zero validation and renewed segment/directory synchronization. This
procedure cross-references §§4.7, 12.2–12.2.1, and 13.10–13.11.

#### WAL record framing, header, CRC, padding, and segment tails

Build one canonical record with distinctive non-palindromic values in every
multibyte field. Compare all 48 header bytes independently at offsets for
`total_length`, `header_length=48`, `record_type`, zero flags/reserved, `lsn`,
`txn_id`, `prev_txn_lsn`, `payload_length`, and `crc32c`. The same fixture
verifies every integer is little-endian and `header.lsn` equals the physical
logical start.

Mutate one dimension at a time: header length, flags, reserved, total/payload
relationship, physical-start LSN, alignment, segment containment, one covered
header bit, one payload bit, and CRC bytes. Use a non-eight-aligned total to
verify `physical_span=Align8(total_length)`, external bytes are all zero, and
those bytes affect neither `total_length` nor CRC. A nonzero external padding
byte in a complete required record is malformed recognized-v1 WAL even though
the covered CRC remains unchanged.

At a segment boundary construct:

- a record whose physical span exactly consumes the remaining bytes;
- a record that would exceed the remainder by one byte and therefore is never
  split across segments;
- at least 48 remaining bytes encoded as one valid CRC-protected `WAL_PAD` whose
  zero payload consumes the tail;
- fewer than 48 remaining bytes as a raw all-zero tail;
- a complete header-sized all-zero suffix, which is unused zero tail rather
  than a valid record because it lacks valid framing;
- nonzero garbage in the equivalent tail position.

The scanner never assembles an ordinary record across two segments. Tail
classification is completed by the recovery matrix below; this local procedure
proves the physical bytes and parser bounds in §§12.3–12.4.

Feed an independently valid WAL_PAD to analysis and redo and assert it only
advances framing to the segment boundary: it creates no page, transaction,
checkpoint, or `page_lsn` action.

#### WAL PageId and per-transaction ownership codecs

Build the exact 16-byte WAL PageId independently: little-endian `file_id` at
offset 0, zero `reserved32` at 4, and little-endian `page_no` at 8. Exercise
minimum and high legal identities plus `INVALID_FILE_ID`, `INVALID_PAGE_NO`,
nonzero reserved bytes, wrong target owner, and unpublished target PageNo.
Malformed target identity must fail before redo or page publication.

For one user transaction encode records `R1`, `R2`, and a terminal record and
assert `R1.prev_txn_lsn=0`, `R2.prev_txn_lsn=R1.lsn`, and
`terminal.prev_txn_lsn=R2.lsn`. Pure system records use zero header TxnId and
previous LSN; BTREE_MTR's payload owner remains diagnostic. Also construct a
recovery-generated TXN_ABORT using the analysis-derived loser TxnId and prior
transaction LSN.

#### PAGE_DELTA codec and payload rejection

The minimum positive fixture uses `patch_count=1` and one one-byte patch at a
legal page offset. Independently calculate its 24-byte prefix, 8-byte patch
header, after-image byte, payload length `33`, record total `81`, physical span
`88`, CRC, and padding. A second fixture uses multiple ordered nonoverlapping
patches, including legal page-boundary ranges, and proves exact count and length
accounting.

Construct these complete outer-framing- and CRC-valid negative fixtures
independently:

| Defect | Required observation |
|---|---|
| `patch_count=0` | malformed recognized-v1 WAL/corruption; no redo or `page_lsn` advance |
| `patch_count=1`, patch length zero | malformed recognized-v1 WAL |
| descending or repeated offsets | malformed canonical ordering |
| overlapping ranges | malformed payload |
| checked `offset+length > 8192` | malformed payload without overflow |
| overlap with common-header bytes `8..19` | malformed payload |
| count/entry mismatch | malformed payload |
| valid entries plus trailing payload byte | malformed payload |
| nonzero patch reserved field | malformed recognized-v1 WAL |

The zero-count fixture remains a complete known-type record: it is neither an
empty legal operation, unsupported type, nor torn tail. Positive `count=1`
guards against accidentally implementing `count>1`, while synthetic maximum
record arithmetic preserves the existing upper bound.

Exercise the writer boundary independently by requesting construction with
`patch_count=0`. It must reject the request before LSN reservation and emit no
record bytes; inspection of every successfully emitted PAGE_DELTA fixture must
find at least one nonempty patch. Decoder rejection is not the writer oracle.

#### PAGE_INIT and PAGE_IMAGE codecs

Construct independent PAGE_INIT and PAGE_IMAGE vectors with the exact 24-byte
payload prefix followed by 8192 image bytes: payload `8216`, record total
`8264`. Verify WAL PageId, expected PageType, zero reserved field,
`image_length=8192`, image PageNo/type, image `page_lsn=record.lsn`, image
checksum, outer CRC, and complete physical span.

Mutate one image dimension at a time: FileId/PageNo owner, PageType, common
header, reserved bytes, image length, embedded `page_lsn`, and page checksum.
The test-side page builder/checksum is independent from the production WAL/page
encoder. PAGE_INIT targets a private new page and participates in bound/runtime
publication; PAGE_IMAGE replaces an already-published page. Tests never collapse
those record identities merely because their payload grammar is shared.

The existing BufferPool new-page publication and PAGE_INIT/MTR Rollback Tests
remain the detailed procedure for private extension, known pre-authorization
failure, valid append, publication failure, crash with WAL but no data-page
image, and WAL-before-data. These mappings remain COMPLETE.

#### BTREE_MTR codec and nested-entry rejection

The minimum positive PATCH_SET fixture uses `page_count=1`, one affected-page
entry, and nested `patch_count=1`. Independently calculate the 16-byte MTR
prefix, 24-byte entry prefix, 8-byte PATCH_SET prefix, one patch header/data,
payload `57`, record total `105`, physical span `112`, outer CRC, and padding.
Separately encode one FULL_IMAGE entry with exact `data_length=8192` and a valid
image whose `page_lsn=mtr.lsn`.

Use valid outer framing and CRC for each nested negative fixture:

| Defect | Required observation |
|---|---|
| `page_count=0` | malformed recognized-v1 WAL; never an empty MTR/barrier/no-op |
| one PATCH_SET with `patch_count=0` | malformed recognized-v1 WAL |
| encoding `0` or unregistered encoding | WAL corruption, not unsupported top-level type |
| short/long `data_length` | malformed nested payload |
| nested count/length mismatch or trailing bytes | malformed nested payload |
| nonzero entry/PATCH_SET reserved field | malformed recognized-v1 WAL |
| invalid or duplicate PageId | malformed MTR membership |
| unsorted affected-page entries | malformed canonical membership |
| invalid nested patch order/overlap/bounds | malformed nested payload |
| FULL_IMAGE length other than 8192 | malformed nested payload |

Positive one-count fixtures prove the legal minimum. Synthetic count/length
builders exercise the largest complete MTR admitted by the one-segment record
bound and one unit beyond, without requiring massive resident allocations.

Exercise both writer boundaries independently: request a BTREE_MTR with zero
affected pages and request a one-page PATCH_SET with zero patches. Each request
must fail before LSN reservation and emit no record. Inspect every successfully
emitted MTR to prove it has at least one page and every PATCH_SET has at least one
nonempty patch; decoder rejection is tested separately.

For a legal multi-page fixture, entries are unique and sorted by
`(file_id,page_no)`. Every final image and redo result uses the one common MTR
LSN. The existing BTREE_MTR failure/crash/recovery and full-tree procedures own
the no-flush, exact pre-authorization rollback, post-authorization completion,
and old-or-complete-new recovery oracle; malformed/zero-membership records fail
before that semantic layer.

Associate one legal structural MTR diagnostically with a user transaction, then
abort that user. Recovery and reopen preserve the authorized structural page
shape while MVCC/status rules suppress loser-owned logical entries; no user
abort physically reverses the system MTR.

#### Registry, terminal, and checkpoint record codecs

Build one independent valid vector for every record type code `0..9` and one
complete CRC-valid vector using an unknown code. The unknown record is
`UNSUPPORTED_WAL_FORMAT`, never an ignorable extension, generic corruption, or
torn tail. A recognizable future owning format uses the §4.14 unsupported-format
path; a known type with invalid v1 payload grammar uses corruption.

TXN_COMMIT and TXN_ABORT fixtures have zero payload, total length 48, normal
TxnId, and the exact transaction-chain tail. The recovery-generated abort form
uses analysis-derived ownership. Byte validity is tested separately from C3 or
ordinary ABORT durability. Read-only transaction procedures continue to require
no ordinary terminal WAL/status record.

Checkpoint vectors use the Chapter-13 owner rather than inventing Chapter-12
payload fields: exact 32-byte BEGIN, DATA's 24-byte prefix plus exact DPT/writer
arrays, and exact 32-byte END. Exercise zero DATA records, contiguous chunk
indexes, count totals, cross-linked BEGIN identities, reserved-zero fields,
redo bound, complete-sequence validation, WAL durability before control-slot
installation, and retention of required image/checkpoint records.

#### Append reservation, authorization, and physical failure

Place deterministic barriers immediately before candidate reservation, after
private encoding, immediately before and after valid-end publication, and before
page/frame metadata publication. A canceled reservation or any injected
reservation, allocation, validation, or known no-append failure must leave the
valid append end, transaction `last_wal_lsn`, published page metadata, and DPT
state unchanged. The next successful append uses the canonical current end, so
the independently decoded stream has neither a hole nor an abandoned LSN.

Schedule at least two appenders at each barrier. Decode the resulting physical
prefix independently and require one total order, disjoint complete spans,
correct segment padding, and each transaction's own `prev_txn_lsn` order. The
procedure observes semantics only; it does not require a mutex, append-buffer
capacity, ring, queue, thread count, or other realization.

Use the two sides of valid-end publication as the authorization oracle:

| Controlled stop/fault | Valid end | Published page/transaction metadata | Required outcome |
|---|---:|---|---|
| before authorization | old | old | exact restoration and ordinary error are permitted by the owning protocol |
| complete bytes installed but valid end not published | old | old | private bytes are not WAL and cannot authorize publication |
| immediately after valid-end publication | advanced across the complete record | publication must complete under retained ownership | rollback-and-continue is forbidden |
| physical short write after authorization | advanced | legal published dirty state | retain exact append bytes, retry the same physical range, and keep `durable_lsn` unchanged |
| append-end/assigned-byte ownership uncertain | unknown | never released as ordinary state | canonical database-noncontinuable path |

For short-write fixtures, inject exact returned byte counts at the first byte,
inside the header, inside the payload, and inside external padding. Compare every
retry byte with the retained independent vector. A crash image contains only the
chosen persisted prefix; recovery accepts only complete records. For uncertainty,
make the harness unable to prove either valid-end state or the exact bytes assigned
to it and assert no ordinary retry, page rollback, later append, writeback, or
durability acknowledgement. These procedures specialize §12.12 and retain the
broader Non-Crash WAL/MTR Failure Injection and §39.1 consequence tests.

#### Durable prefix, group commit, COMMIT, and WAL-before-data

Recovery first establishes the append end and initial durable-prefix fact from
the validated persisted stream. An empty stream begins appending at LSN `8`; the
test never interprets invalid LSN zero as a durable record.

For a single-segment request through record LSN `X`, pause after bytes are written
and again after segment `fdatasync`. The waiter cannot return and `durable_lsn`
cannot cover `X` until the complete record and every required preceding byte are
proven durable. For a cross-segment target, leave unsynchronized bytes in the
older segment and prove synchronizing only the newer file is insufficient. Then
synchronize every affected segment and permit monotonic advancement.

For a record in a newly created segment, hold `fsync(database_root/wal)` after
successful segment data synchronization. No durability waiter, COMMIT C3, or
page writeback may treat that record as durable. Release the directory barrier
and verify that namespace proof plus required segment synchronization permits the
advance. Injecting either synchronization failure preserves every independently
proven lower prefix and releases each affected waiter with the exact failure;
fatal WAL-service or shutdown failure releases all unsatisfiable waiters rather
than leaving them asleep.

Use three legal aligned commit starts, `1000`, `1104`, and `1256`, with separate
waiters. One durability operation through `1256` may release all three, but each
assertion is evaluated independently against its own target. A lower-target waiter
may return earlier only if its complete prefix is independently proven durable;
no batching cadence is required. A flush failure cannot report success for an
unproven target or falsely advance `durable_lsn`. This proves group-commit
semantics while leaving scheduling, thread count, buffering, and coalescing
mechanisms free.

The COMMIT fixture appends TXN_COMMIT at `L`, holds `durable_lsn < L`, and proves
C3 cannot complete. Once `durable_lsn >= L`, C3 may complete without a heap,
index, or status-page force. C4 runtime terminal publication, C5 resource release,
and C6 acknowledgement remain separately gated by the COMMIT Fault-Injection
Tests. Put the same record in a new segment to exercise the namespace prerequisite,
and coalesce several commits to prove each C3 remains tied to its own LSN. The
ABORT fixture preserves ordinary A2 publication without inventing synchronous
force, while WAL-before-data still prevents its status page from passing an
undurable terminal `page_lsn`. A successful read-only transaction remains exempt
from an ordinary terminal WAL/status record.

For direct WAL-before-data coverage, prepare a copied stable dirty image with
`page_lsn=L` and hold `durable_lsn<L`; assert data-page `pwrite` cannot begin.
Advance the canonical durable prefix through the complete record at `L`, then
permit writeback. Merely writing an OS byte at offset `L` never satisfies the
oracle. Inject page `pwrite` and page `fdatasync` failures after WAL durability:
the frame remains dirty under Chapter 7 and a durably COMMITTED transaction does
not reverse. The Buffer management copied-image fixture remains the owner for a
later generation `G+1` not being cleared by completion of copied generation `G`.
Repeat the gate for heap, TXN_STATUS, WAL-protected FSM, catalog, B+ tree, and
every applicable BufferPool-managed superblock/control-like page family; no
record family receives an implicit unlogged writeback exception.

#### WAL tail, segment inventory, and recovery handoff

Construct exact persisted byte prefixes instead of depending on kill timing. The
tail scanner matrix below covers a final suffix shorter than 48 bytes, a complete
header with truncated payload/span, and a complete-sized CRC-invalid candidate at
the first unrequired final suffix. Under §13.11 each is excluded from the valid
tail; a complete malformed record inside the required retained prefix, or an
invalid record followed by required history, is corruption and cannot be silently
truncated. No decoder reads past the constructed bytes.

Create a valid segment followed by one exact-size all-zero next-contiguous segment
and verify it is an empty/adoptable tail only through §§12.2.1 and 13.11. Separately
omit a required interior segment and require recovery failure. Retention-floor
fixtures may omit or reconcile only segments Chapter 13/14 prove wholly older than
the installed floor; an unexplained noncontiguous or future segment is never used
to bridge a gap.

Place a complete CRC-valid unknown type in the authoritative prefix and require
`UNSUPPORTED_WAL_FORMAT`. Place a complete known PAGE_DELTA/BTREE_MTR with one
forbidden zero count in the same position and require the canonical corruption
path. Neither is a torn tail, and neither permits READY. This precedence is tested
with otherwise canonical framing so nested grammar is the sole defect.

Recovery-admission barriers prove ordinary append cannot begin until exclusive
ownership, segment inventory, valid-tail reconstruction, append position,
durable-prefix state, analysis/redo/loser resolution, and the Chapter-3 READY gate
are established. Negative target fixtures use the actual PageId/catalog/file
ownership chain: wrong FileId, wrong PageNo/type/owner, missing required target,
and unpublished/orphan targets are classified before ordinary publication; no
invented WAL database UUID is assumed.

For redo, first use a checksum-invalid data page carrying a plausible high
`page_lsn`; the stored LSN must not suppress reconstruction. After a page is
valid/trusted, cover `page_lsn < record_lsn`, equality, and greater-than cases
with the exact Chapter-13 apply/skip oracle. PAGE_INIT/PAGE_IMAGE/full-image MTR
fixtures supply reconstruction bases; repeated recovery must be idempotent and a
complete MTR may not leave a subset of page effects.

#### TXN_STATUS image/terminal crash prefixes

Use a status mutation that requires preparatory system image `F` and terminal
record `T`, with `F<T`. Independently decode that `F` has system ownership and no
terminal bit for the subject transaction, while `T` is on the user transaction's
WAL chain. After successful runtime installation assert `rec_lsn=F` and
`page_lsn=T`; the same rule uses PAGE_INIT LSN `I` for a newly allocated status
page.

Construct every crash case by selecting persisted record bytes, segment/directory
durability facts, and status-page disk bytes independently. Runtime append success
is not evidence after restart. In particular, an F-only prefix is merely a
physical base and cannot create COMMITTED or ABORTED; a validly appended but
unproven T produces the terminal outcome only if a complete T survives in the
validated prefix; durable T reconstructs its terminal status even when the data
page was never flushed. A durable COMMIT remains COMMITTED across crashes before
runtime publication or acknowledgement.

Before Chapter-9 runtime terminal publication, pause after resident status-bit
installation and prove the active registry still reports the transaction as
nonterminal to ordinary lookup. Persistent bits become ordinary terminal
authority only through the owning publication/recovery rules; early resident
installation cannot expose COMMITTED before C3/C4.

For repeated same-page terminal mutations within one dirty interval, preserve the
first required `rec_lsn`, advance `page_lsn` to each latest authorizing terminal
record, and preserve dirty/DPT coherence. If a checkpoint FPI epoch adds a newer
image while the page is already dirty, the original dirty-interval `rec_lsn`
remains. Retention tests hold the image at `rec_lsn` and all later required
terminal WAL until a durably clean page/checkpoint state makes them unnecessary;
only then may Chapter 13/14 advance the floor.

#### Length, position, checkpoint, and lifetime cross-owners

For every record family, the independent builder exercises the smallest legal
payload, the largest aligned record that fits one segment, and the next byte or
count that would exceed the family, uint32, or segment bound. All additions and
alignment use widened integers before narrowing; rejection occurs before record
allocation, LSN reservation, provisional mutation, or publication. Count maxima
remain consequences of the unchanged record-size grammar, so the positive
nonzero-count boundary does not alter any upper limit.

Numeric Exhaustion and Terminal-Boundary Verification directly supplies
synthetic fixtures for the last legal minimum-size record start
`2^64-48`, a legal exclusive mathematical end of `2^64`, rejection of the next
reservation, maximum segment index `2^38-1`, and rejection of the next index
before name formatting or creation. A separate injected ENOSPC at an ordinary
legal LSN proves filesystem exhaustion is not `WAL_POSITION_EXHAUSTED`.
Its terminal-headroom fixture admits an active persistent writer, consumes all
general WAL capacity up to the credited boundary, rejects another ordinary WAL
operation, and still permits the one required TXN_COMMIT or TXN_ABORT plus any
bounded preparatory status image. Candidate reservation must preserve that
credit without turning it into a hole.

Checkpoint codecs and sequence validation are byte-covered above; Crash
Injection Framework, Database Lifecycle Tests, and Recovery Property Tests
remain the procedural owners for fuzzy capture, installation only after
checkpoint WAL/control prerequisites, DPT redo bounds, full-image retention,
and recycling. The status-image retention fixture specifically holds
`F=rec_lsn` until those owners prove it unnecessary.

Database Lifecycle Tests own the shutdown order: stop new admission, drain or
fail authorized append/durability requests, wake unsatisfiable waiters, preserve
WAL service until BufferPool no longer needs it, and perform no ordinary append
before recovery publishes READY. Chapter 15/§39 fault tables remain the owners
for the first-persistent-statement-write boundary; this section supplies the
underlying WAL known-failure versus uncertainty result without changing the SQL
consequence. Recovery continues to use redo plus loser status rather than
physical user-DML undo or CLRs.

#### WAL format-classification matrix

| Fixture | Outer framing complete? | CRC valid? | Known v1 type? | Payload grammar valid? | First unrequired final tail? | Expected classification |
|---|---:|---:|---:|---:|---:|---|
| canonical record | yes | yes | yes | yes | either | accepted |
| incomplete final header | no | N/A | undecodable | N/A | yes | incomplete/torn final tail; exclude suffix |
| complete header, incomplete final payload/span | no | N/A | yes | undecidable | yes | incomplete/torn final tail; exclude suffix |
| complete-sized CRC-invalid final candidate | yes | no | yes | otherwise yes | yes | invalid/torn final tail under §13.11; exclude suffix |
| CRC-invalid required interior record | yes | no | yes | otherwise yes | no | `CORRUPT_WAL`/recovery failure |
| complete unknown type | yes | yes | no | N/A | no | `UNSUPPORTED_WAL_FORMAT` |
| recognizable future owning format | yes | as owned | not v1 | N/A | no | canonical unsupported-format result |
| PAGE_DELTA `patch_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| BTREE_MTR `page_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| PATCH_SET `patch_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| nonzero external alignment byte | yes | yes over covered bytes | yes | framing invalid | no | malformed recognized-v1 WAL / corruption |
| malformed nested length | yes | yes | yes | no | no | malformed recognized-v1 WAL / corruption |

The incomplete/CRC-invalid final-tail rows apply only at the first unrequired
suffix. Relocating those same bytes into the required retained prefix changes
the outcome to corruption. A complete CRC-valid known-v1 record with a forbidden
zero count remains malformed/corrupt even when it is physically final; its
recognized payload grammar prevents torn-tail treatment.

#### Durable-prefix and WAL-before-data matrix

| Scenario | Valid append end | Namespace durable? | Required segment data durable? | `durable_lsn` may cover target? | Waiter may return success? | Page at target LSN may write back? |
|---|---|---:|---:|---:|---:|---:|
| append only | includes target | yes | no | no | no | no |
| data write complete, `fdatasync` pending | includes target | yes | no | no | no | no |
| single-segment `fdatasync` complete | includes target | yes | yes | yes | yes | yes |
| new-segment directory sync pending | includes target | no | yes | no | no | no |
| cross-segment target, older file unsynced | includes target | yes | no | no | no | no |
| cross-segment target, all required files synced | includes target | yes | yes | yes | yes | yes |
| group waiter at lower proven target | beyond target | yes | through own target | yes for lower target | yes | yes |
| group waiter at maximum unproven target | includes target | yes | no | no | no | no |
| flush failure | includes target | as constructed | no for target | preserve prior proven value | no for affected target | no for affected page |

#### WAL segment and framing matrix

| Domain | Positive fixture | Negative/boundary fixture | Independent observation | Status |
|---|---|---|---|---|
| segment basename | indexes 0, 1, `2^38-1` | short, uppercase, decimal, wrong suffix, `2^38` | exact 16 lowercase hex digits plus `.wal` | COMPLETE |
| segment-zero prefix | eight zero bytes; first LSN 8 | record-like bytes before 8 | scanner begins at 8 | COMPLETE |
| segment header | later segment record at offset 0 | invented header displacement | no header bytes consumed | COMPLETE |
| size | exactly 67,108,864 | one short/long | file length and terminal offset | COMPLETE |
| creation durability | truncate, file sync, directory sync | fail each step | no durability credit before both sync classes | COMPLETE |
| record start | eight-byte aligned | misaligned encoded/physical start | widened LSN arithmetic | COMPLETE |
| header | exact 48-byte vector | each field/length mutation | byte oracle | COMPLETE |
| CRC | independent canonical vector | header and payload bit flips | independent CRC32C | COMPLETE |
| padding | zero and outside `total_length`/CRC | nonzero byte | byte and CRC oracles | COMPLETE |
| no crossing | exact fit | one-byte excess | no cross-segment assembly | COMPLETE |
| segment tail | valid WAL_PAD; short zero tail | header-sized zeros; garbage | framing and §13.11 location oracle | COMPLETE |

#### WAL record-family codec matrix

| Type/code | Minimum payload / positive boundary | Principal persistent fields | Page / txn association | Malformed boundary | Oracle | Status |
|---|---|---|---|---|---|---|
| WAL_PAD / 0 | zero-filled payload sized to segment remainder | normal header, zero payload bytes | neither | wrong size/nonzero payload | byte/CRC/segment arithmetic | COMPLETE |
| PAGE_INIT / 1 | 8216 | PageId, type, reserved, length, full image | page; user or system per owner | image length/identity/LSN/checksum | independent page and WAL bytes | COMPLETE |
| PAGE_DELTA / 2 | 33, count 1 and one-byte patch | PageId, type, count, patches | page; user or system per owner | count 0, empty/invalid patch | byte/nested-length oracle | COMPLETE |
| PAGE_IMAGE / 3 | 8216 | same codec as PAGE_INIT, existing-page meaning | page; user or system per owner | image length/identity/LSN/checksum | independent page and WAL bytes | COMPLETE |
| BTREE_MTR / 4 | 57 for one one-byte PATCH_SET | diagnostic owner, page count, entries | system record; multiple pages | page count 0, patch count 0, bad nested entry | nested byte/LSN oracle | COMPLETE |
| TXN_COMMIT / 5 | 0 | header TxnId and previous transaction LSN | transaction | payload nonzero/invalid owner | byte/chain oracle | COMPLETE |
| TXN_ABORT / 6 | 0 | header TxnId and previous transaction LSN | transaction | payload nonzero/invalid owner | byte/chain oracle | COMPLETE |
| CHECKPOINT_BEGIN / 7 | 32 | exact Chapter-13 BEGIN fields | system | wrong identity/reserved/length | independent checkpoint vector | COMPLETE |
| CHECKPOINT_DATA / 8 | 24 plus exact arrays | chunk/count prefix, DPT/writers | system | count/length/index mismatch | independent array arithmetic | COMPLETE |
| CHECKPOINT_END / 9 | 32 | exact Chapter-13 END fields | system | cross-link/count/reserved mismatch | independent sequence oracle | COMPLETE |

#### TXN_STATUS crash-prefix matrix

| Controlled prefix/boundary | Surviving WAL | Status-page disk state | Runtime state after crash | Recovered terminal status | Recovery base | Legal client interpretation |
|---|---|---|---|---|---|---|
| before F | no F/T | prior trusted page | lost | loser becomes ABORTED where required | prior page/older retained image | no durable COMMIT |
| F appended only | complete F, no T | prior page | lost | F supplies no terminal outcome; loser resolution applies | F | no durable COMMIT |
| F durable only | durable F, no T | prior or F image | lost | same: F is semantically inert for terminal outcome | F | no durable COMMIT |
| T appended, absent from surviving valid prefix | F only | unflushed | lost | loser outcome | F | no durable COMMIT |
| complete T survives without prior acknowledgement | F then T in valid prefix | unflushed | lost | terminal code from T | F then terminal redo | client may be uncertain; recovery truth controls |
| T durable, status page unflushed | durable F/T | prior page | lost | exact COMMITTED/ABORTED from T | F then T | COMMIT durable and irreversible |
| page write attempted before T durability | T not durable | no new legal page | lost if crashed | derived from valid WAL only | prior/F | write must have been blocked |
| stable status page after WBD | WAL through page_lsn durable | trusted page at T or later | lost | reflected outcome, idempotently checked | trusted page | outcome agrees with durable WAL |
| crash after durable T before C4 | durable F/T | either | lost | COMMITTED for commit T | F/page plus T | success may be unknown; not ABORTED |
| crash after C3 before C6 | durable F/T | either | lost | COMMITTED | F/page plus T | transport uncertainty cannot reverse outcome |

#### WAL tail and inventory matrix

| Fixture | Location | Complete framing / CRC | Segment continuity | Expected valid end | Classification | READY allowed? |
|---|---|---|---|---|---|---:|
| clean EOF/zero suffix | final tail | no next record | contiguous | after last complete record | clean tail | yes after recovery |
| short all-zero tail | segment end, `<48` bytes | raw padding | contiguous | next segment boundary only if next valid record exists | legal tail padding | yes |
| incomplete final header | final suffix | incomplete | contiguous | prior record end | torn/incomplete tail | yes after discard |
| complete header, incomplete payload/span | final suffix | incomplete | contiguous | prior record end | torn/incomplete tail | yes after discard |
| complete-sized CRC-invalid candidate | first unrequired final suffix | complete / bad | contiguous | prior record end | invalid/torn final tail | yes after discard |
| CRC-invalid required interior record | retained interior | complete / bad | contiguous | cannot establish required prefix | corruption | no |
| all-zero next segment | next contiguous segment | no record | contiguous | prior valid end | permitted empty tail; resync before adoption | yes |
| missing required middle segment | retained interior | N/A | gap | cannot establish | corruption/recovery failure | no |
| complete unknown record | authoritative prefix | complete / valid | contiguous | scan stops with semantic failure | `UNSUPPORTED_WAL_FORMAT` | no |
| complete known malformed record, including final zero-count record | authoritative prefix or physical final record | complete / valid | contiguous | scan stops with grammar failure | `CORRUPT_WAL`/recovery failure | no |

#### WAL error/result matrix

| Condition | Expected architecture-owned result/consequence | Forbidden collapse |
|---|---|---|
| malformed recognized-v1 required WAL | `CORRUPT_WAL`/canonical recovery corruption | unsupported type, no-op, free truncation |
| unknown complete record type / future owning format | `UNSUPPORTED_WAL_FORMAT` or owning unsupported-format result | generic v1 corruption or skip |
| incomplete/invalid first final suffix | torn/invalid final-tail exclusion under §13.11 | accepted record or interior-corruption suppression |
| logical record physical span exceeds segment | `WAL_RECORD_TOO_LARGE`/`ENCODED_LENGTH_EXCEEDED` before reservation | partial append |
| mathematical WAL end exhausted | `WAL_POSITION_EXHAUSTED` before reservation/publication | wrap or ENOSPC |
| filesystem ENOSPC/write/sync failure at legal position | exact WAL/filesystem I/O failure and §12.12 consequence | numeric exhaustion |
| segment namespace synchronization failure | WAL I/O failure; no durability credit | durable segment inference from file data alone |
| uncertain valid-end/assigned bytes | `DATABASE_NONCONTINUABLE` | ordinary append failure/retry guess |
| fatal WAL service/shutdown failure | exact failure to all unsatisfiable waiters | indefinite wait or false success |
| data-page write/sync failure after WAL durable | page remains dirty; exact page I/O failure | reverse WAL or COMMITTED outcome |

#### High-level WAL domain/case matrix

| Domain/case | Deterministic fixture or order | Independent oracle | Architecture / verification owner | Status |
|---|---|---|---|---|
| segment namespace | indexes 0, 1, maximum and malformed names | text grammar | §§12.2–12.2.1 / segment subsection | COMPLETE |
| segment creation durability | fail create/truncate/file sync/directory sync | event order and namespace fact | §12.2.1 / segment subsection | COMPLETE |
| record header | distinctive 48-byte vector | byte oracle | §12.4 / framing subsection | COMPLETE |
| CRC and padding | bit flips and nonaligned total | CRC/byte oracle | §§12.3–12.4 / framing subsection | COMPLETE |
| no crossing | exact fit and one-byte excess | widened placement arithmetic | §12.3 / framing subsection | COMPLETE |
| WAL PageId | legal and invalid identities | 16-byte oracle | §12.5 / PageId subsection | COMPLETE |
| PAGE_DELTA valid | count 1 and multiple patches | nested byte oracle | §12.8 / PAGE_DELTA subsection | COMPLETE |
| PAGE_DELTA zero count | otherwise canonical complete record | grammar oracle | §12.8 / PAGE_DELTA subsection | COMPLETE |
| PAGE_INIT / PAGE_IMAGE | exact images and owner mutations | page/WAL oracle | §§12.9–12.10 / image subsection | COMPLETE |
| BTREE_MTR valid | one-page and multi-page | nested/common-LSN oracle | §12.10.2 / MTR subsection | COMPLETE |
| BTREE_MTR/PATCH_SET zero count | otherwise canonical complete records | grammar oracle | §12.10.2 / MTR subsection | COMPLETE |
| terminal records | commit/abort zero-payload vectors | byte/chain oracle | §§12.7.2, 9.14 / registry subsection | COMPLETE |
| append reservation | cancel before authorization | valid-end oracle | §12.12.1 / append subsection | COMPLETE |
| append authorization | barriers around valid-end publication | state-transition oracle | §§12.12.2–12.12.4 / append subsection | COMPLETE |
| physical short write | exact prefix return counts | retained-byte/persisted-prefix oracle | §12.12.2 / append subsection | COMPLETE |
| append uncertainty | indeterminate end or byte ownership | continuation-state oracle | §12.12.4 / append subsection | COMPLETE |
| single/cross-segment flush | hold each segment sync | durable-prefix oracle | §12.13 / durability subsection | COMPLETE |
| namespace prerequisite | hold directory sync | namespace/durable-prefix oracle | §§12.2.1, 12.13 / durability subsection | COMPLETE |
| group commit | aligned distinct targets | per-waiter predicate | §§12.14–12.15 / durability subsection | COMPLETE |
| WAL-before-data | hold `durable_lsn<L` | complete-record prefix oracle | §§7.10, 12.17 / durability subsection | COMPLETE |
| tail reconstruction | exact final byte prefixes | recovery-prefix oracle | §13.11 / tail subsection | COMPLETE |
| unknown vs malformed | complete unknown and known-invalid records | registry/payload grammar | §§12.7, 13.11 / format matrix | COMPLETE |
| status two-record protocol | F/T persisted-prefix matrix | terminal/status model | §§12.10.5, 13.13.2 / status subsection | COMPLETE |
| LSN/record exhaustion | synthetic terminal arithmetic | widened LSN oracle | §4.3.2.4 / numeric exhaustion | COMPLETE |
| recovery handoff | hold READY and append admission | lifecycle event oracle | §§3.3, 13.19 / tail subsection | COMPLETE |

#### Chapter-12 architecture-obligation coverage map

The atomic inventory below assigns each obligation one primary domain. Splitting
combined architecture sentences by independently faultable or observable result
produces 117 obligations; no target count is imposed. `COMPLETE` means a direct
procedure above or the named complete procedure supplies a
deterministic oracle, not merely that Architecture states the requirement.

| ID/domain | Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Status |
|---|---|---|---|---|---|
| 1 A | WAL resides in the database `wal/` namespace | §12.2 | WAL segment namespace | exact directory/name inventory | COMPLETE |
| 2 A | segment basename is 16 lowercase hex digits plus `.wal` | §12.2 | WAL segment namespace | indexes 0, 1, max plus malformed names | COMPLETE |
| 3 A | segment size is exactly 67,108,864 bytes | §12.2 | WAL segment namespace | exact/short/long length fixtures | COMPLETE |
| 4 A | v1 segment has no header | §12.2.1 | WAL segment namespace | later-segment record at offset zero | COMPLETE |
| 5 A | segment-zero bytes 0..7 are zero and first append end is 8 | §§12.2, 13.11 | WAL segment namespace / tail | reserved-prefix decode fixture | COMPLETE |
| 6 A | fresh unused segment bytes are zero | §12.2.1 | WAL segment namespace | exact-size zero-content validation | COMPLETE |
| 7 A | required retained segment inventory is contiguous | §13.11 | WAL tail and inventory | missing-middle fixture | COMPLETE |
| 8 B | segment creation orders create, truncate, file sync, directory sync | §12.2.1 | WAL segment namespace | event barriers and per-step faults | COMPLETE |
| 9 B | creation/sync failure cannot advance durability | §§12.2.1, 12.13 | WAL segment namespace / durable prefix | hold/fail each prerequisite | COMPLETE |
| 10 B | all-zero next segment requires validation and resynchronization before adoption | §§12.2.1, 13.11 | WAL tail and inventory | all-zero survivor fixture | COMPLETE |
| 11 D | ordinary record never crosses a segment | §12.3 | WAL record framing | exact-fit and one-byte-excess fixtures | COMPLETE |
| 12 D | ordinary record starts are eight-byte aligned | §§12.3–12.4 | WAL record framing / LSN oracle | aligned and misaligned physical starts | COMPLETE |
| 13 E | `total_length`, payload, and physical-span arithmetic is exact | §§12.3–12.4 | WAL record framing | byte vector plus widened arithmetic | COMPLETE |
| 14 D | external padding is zero and outside length/CRC | §§12.3–12.4 | WAL record framing | nonaligned total and padding mutation | COMPLETE |
| 15 D | WAL_PAD and raw short zero tail are distinct | §§12.3, 12.7.1 | WAL record framing / tail matrix | legal PAD, short zero, header-sized zero fixtures | COMPLETE |
| 16 C | record header is exactly 48 bytes at fixed offsets | §12.4 | WAL record framing | distinctive all-field byte vector | COMPLETE |
| 17 C | all multibyte header fields are little-endian | §12.4 | Byte oracle | non-palindromic field values | COMPLETE |
| 18 C | header/payload length relationships are validated | §12.4 | WAL record framing | inconsistent-length mutations | COMPLETE |
| 19 C | encoded LSN equals physical logical record start | §12.4 | WAL record framing / LSN oracle | relocated and mismatched fixtures | COMPLETE |
| 20 C | flags and reserved fields are zero | §12.4 | WAL record framing | independent nonzero mutations | COMPLETE |
| 21 E | CRC32C coverage is exact | §12.4 | CRC oracle | independent header-zero-field plus payload vector | COMPLETE |
| 22 E | covered header/payload corruption is rejected | §12.4 | WAL record framing | one-bit mutations | COMPLETE |
| 23 I | WAL PageId is exact 16-byte ABI-independent codec | §12.5 | WAL PageId codec | byte-exact boundary vectors | COMPLETE |
| 24 I | invalid/reserved/wrong target identity fails before redo | §§12.5, 13.13 | WAL PageId / recovery handoff | one-defect owner fixtures | COMPLETE |
| 25 M | user records maintain per-transaction previous-LSN chain | §§12.6–12.7.3 | ownership codecs | R1/R2/terminal chain | COMPLETE |
| 26 M | system record ownership and recovery abort ownership are canonical | §§12.6–12.7.3 | ownership codecs | system zero-owner and loser-abort fixtures | COMPLETE |
| 27 H | record codes 0..9 are stable and each codec is exercised | §12.7 | registry codec matrix | one independent vector per code | COMPLETE |
| 28 H | complete unknown record type is unsupported | §§12.7, 13.11 | registry / format matrix | CRC-valid unknown-code fixture | COMPLETE |
| 29 H | future format, malformed v1, and torn tail remain distinct | §§4.14, 12.7, 13.11 | format-classification matrix | canonical three-way fixtures | COMPLETE |
| 30 J | PAGE_DELTA count-one minimum is accepted | §12.8 | PAGE_DELTA codec | exact one-byte patch vector | COMPLETE |
| 31 J | PAGE_DELTA multiple ordered patches are accepted | §12.8 | PAGE_DELTA codec | boundary multi-patch vector | COMPLETE |
| 32 J | PAGE_DELTA zero count is corruption and cannot advance page_lsn | §§12.8, 12.18 | PAGE_DELTA codec | complete CRC-valid count-zero fixture | COMPLETE |
| 33 J | each PAGE_DELTA patch is nonempty | §12.8 | PAGE_DELTA codec | count-one/length-zero fixture | COMPLETE |
| 34 J | PAGE_DELTA order and nonoverlap are validated | §12.8 | PAGE_DELTA codec | descending/repeated/overlap fixtures | COMPLETE |
| 35 J | PAGE_DELTA bounds and protected header range are validated | §12.8 | PAGE_DELTA codec | checked end/page_lsn-checksum range fixtures | COMPLETE |
| 36 J | PAGE_DELTA count/length consumes payload exactly | §12.8 | PAGE_DELTA codec | mismatch and trailing-byte fixtures | COMPLETE |
| 37 K | PAGE_INIT exact full-image codec is accepted | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | independent 8216-byte payload vector | COMPLETE |
| 38 K | PAGE_INIT image identity/type/LSN/checksum is validated | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | one-field image mutations | COMPLETE |
| 39 K | PAGE_INIT private/publication/crash protocol is old-or-published | §§5.4.2, 12.12 | PAGE_INIT and MTR Rollback Tests | pre/post-authorization and crash fixtures | COMPLETE |
| 40 K | PAGE_IMAGE codec is distinct existing-page semantics | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | paired same-layout semantic fixtures | COMPLETE |
| 41 K | clean-to-dirty/checkpoint epoch requires a complete image | §12.10 | Buffer management verification / Crash Injection Framework | clean/dirty and FPI-epoch schedules | COMPLETE |
| 42 K | dirty-interval `rec_lsn` names retained complete image | §§12.10.1, 12.16 | Buffer management verification / Crash Injection Framework | clean-to-dirty and retention assertions | COMPLETE |
| 43 L | one-page BTREE_MTR is a legal minimum | §12.10.2 | BTREE_MTR codec | page-count-one PATCH_SET vector | COMPLETE |
| 44 L | zero-page BTREE_MTR is corruption, not no-op | §§12.10.2, 12.18 | BTREE_MTR codec | complete CRC-valid zero-page fixture | COMPLETE |
| 45 L | FULL_IMAGE nested entry has exact 8192-byte canonical image | §12.10.2 | BTREE_MTR codec | full-image positive/length-negative vectors | COMPLETE |
| 46 L | PATCH_SET count one is accepted | §12.10.2 | BTREE_MTR codec | nested one-byte patch vector | COMPLETE |
| 47 L | PATCH_SET zero count is corruption | §§12.10.2, 12.18 | BTREE_MTR codec | complete CRC-valid nested-zero fixture | COMPLETE |
| 48 L | nested encoding/length/reserved/patch grammar is validated | §12.10.2 | BTREE_MTR codec | isolated nested-entry matrix | COMPLETE |
| 49 L | affected PageIds are sorted and unique | §12.10.2 | BTREE_MTR codec | duplicate/unsorted fixtures | COMPLETE |
| 50 L | every MTR participant uses one common MTR LSN | §12.10.2 | BTREE_MTR codec and nested-entry rejection / B+ Tree Verification | legal multi-page recovery vector | COMPLETE |
| 51 L | complete MTR recovers old or complete new state | §§12.10.2–12.10.3, 13.13.3 | B+ Tree Verification | BTREE_MTR failure/crash/recovery procedure | COMPLETE |
| 52 L | no-flush excludes partial MTR writeback/publication | §§12.10.3, 12.17 | B+ Tree Verification / Publication-atomicity observers | deterministic checkpoint/writeback barriers | COMPLETE |
| 53 N | status mutation orders system image F before terminal T | §12.10.5 | TXN_STATUS crash prefixes | independently decoded F/T fixture | COMPLETE |
| 54 N | F alone is terminally inert | §12.10.5.4 | TXN_STATUS crash prefixes | image-only prefix | COMPLETE |
| 55 N | appended-undurable T is decided only by surviving valid WAL | §12.10.5.4 | TXN_STATUS crash prefixes | two persisted-prefix variants | COMPLETE |
| 56 N | durable T repairs unflushed status page | §§12.10.5, 13.13.2 | TXN_STATUS crash prefixes | durable-T/unflushed-page restart | COMPLETE |
| 57 N | status page uses `rec_lsn=F`, `page_lsn=T` | §§12.10.5.2, 12.16 | TXN_STATUS crash prefixes | runtime metadata assertion | COMPLETE |
| 58 N | repeated status updates preserve rec_lsn and advance page_lsn | §12.10.5.2 | TXN_STATUS crash prefixes | same dirty-interval schedule | COMPLETE |
| 59 N | status image/terminal WAL remains retained while required | §§12.10.5.3, 13.10, 14.14 | TXN_STATUS image/terminal crash prefixes / Crash Injection Framework | hold F until clean/checkpoint proof | COMPLETE |
| 60 Z | heap redo precedes index MTR that references its RID | §12.11 | B+ Tree Verification / Statement Failure and Transaction-State Tests | ordered WAL decode and crash prefixes | COMPLETE |
| 61 O | candidate reservation is private and non-consuming | §12.12.1 | Append reservation, authorization, and physical failure | cancel/fail before authorization | COMPLETE |
| 62 O | concurrent appends establish one hole-free total order | §§12.12.1–12.12.2 | Append reservation, authorization, and physical failure | two-appender barrier schedules | COMPLETE |
| 63 P | valid append is atomic complete-byte plus valid-end publication | §12.12.2 | Append reservation, authorization, and physical failure | before/after valid-end barriers | COMPLETE |
| 64 P | reserved LSN cannot publish page/transaction metadata | §§12.12.1–12.12.3 | Append reservation, authorization, and physical failure | metadata observers before valid end | COMPLETE |
| 65 Q | known preauthorization failure restores exact old state | §§12.12.3–12.12.4 | Non-Crash WAL/MTR Failure Injection | clean/dirty rollback captures | COMPLETE |
| 66 Q | postauthorization rollback-and-continue is forbidden | §§12.12.3–12.12.4 | Append reservation, authorization, and physical failure / PAGE_INIT and MTR Rollback Tests | publication completion or noncontinuable | COMPLETE |
| 67 Q | physical short write retains exact bytes and no durability credit | §12.12.2 | Append reservation, authorization, and physical failure | exact partial-count/retry fixtures | COMPLETE |
| 68 Q | uncertain append end/bytes is noncontinuable | §12.12.4 | Append reservation, authorization, and physical failure | uncertainty fault and admission assertions | COMPLETE |
| 69 O | append buffer retains required bytes while mechanism remains free | §12.12.5 | Append reservation, authorization, and physical failure / Database Lifecycle Tests | retry, no-hole, shutdown observations | COMPLETE |
| 70 R | recovery establishes initial append/durable-prefix facts | §§12.13, 13.11 | Durable prefix, group commit, COMMIT, and WAL-before-data / WAL tail, segment inventory, and recovery handoff | empty and nonempty reopen fixtures | COMPLETE |
| 71 R | single-segment durability waits for complete prefix sync | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | write/fdatasync barriers | COMPLETE |
| 72 R | cross-segment durability syncs every affected segment | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | older-segment-unsynced fixture | COMPLETE |
| 73 R | new-segment namespace sync precedes durability credit | §§12.2.1, 12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | directory-fsync barrier | COMPLETE |
| 74 R | durable_lsn is monotonic and means complete record/prefix | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | independent accepted-prefix model | COMPLETE |
| 75 S | group commit handles distinct aligned targets | §12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | 1000/1104/1256 waiter schedule | COMPLETE |
| 76 S | no waiter returns before its own target | §§12.13–12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | per-waiter predicate assertions | COMPLETE |
| 77 S | flush failure preserves proven prefix and wakes affected waiters | §§12.13–12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | multi-target sync failure | COMPLETE |
| 78 X | fatal WAL service/shutdown wakes unsatisfiable waiters | §§3.3.6, 12.13 | Database Lifecycle Tests / Durable prefix, group commit, COMMIT, and WAL-before-data | fatal-service and drain barriers | COMPLETE |
| 79 M | TXN_COMMIT codec and C3 durability boundary are exact | §§12.7.2, 12.15; §9.14.1 | Registry, terminal, and checkpoint record codecs / COMMIT Fault-Injection Tests | byte vector and `durable_lsn` barrier | COMPLETE |
| 80 M | TXN_ABORT codec preserves ordinary no-force semantics | §§12.7.2, 12.10.5.2; §9.14.2 | Registry, terminal, and checkpoint record codecs / ABORT Fault-Injection Tests | byte vector and status WBD gate | COMPLETE |
| 81 M | read-only terminal-WAL exception remains | §9.14 | Transaction identity, snapshot, and status verification / COMMIT Fault-Injection Tests | successful read-only transaction fixture | COMPLETE |
| 82 M | durable COMMIT is irreversible; C4–C6 remain separate | §§9.14.1, 12.15 | COMMIT Fault-Injection Tests | C3 through acknowledgement boundaries | COMPLETE |
| 83 T | page writeback is blocked while `durable_lsn < page_lsn` | §12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data / Buffer management verification | direct pwrite gate | COMPLETE |
| 84 T | WBD uses complete-record/prefix durability meaning | §§12.13, 12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data | independent record acceptance oracle | COMPLETE |
| 85 T | data flush failure leaves page dirty and cannot reverse commit | §§7.10–7.11, 12.17 | Buffer management verification | post-WAL page I/O faults | COMPLETE |
| 86 T | older copied writeback cannot clear newer dirty generation | §§7.10–7.11, 12.16 | Buffer management verification | G/G+1 reconciliation fixture | COMPLETE |
| 87 U | incomplete final header is excluded as torn tail | §13.11 | WAL tail and inventory | exact short persisted prefix | COMPLETE |
| 88 U | incomplete final payload/span is excluded as torn tail | §13.11 | WAL tail and inventory | exact truncated record variants | COMPLETE |
| 89 U | CRC-invalid complete-sized first final suffix is excluded as invalid tail | §13.11 | WAL tail and inventory | final-location CRC fixture | COMPLETE |
| 90 U | CRC-invalid/malformed required interior WAL is corruption | §13.11 | WAL tail and inventory | bad interior followed by required history | COMPLETE |
| 91 U | all-zero next segment is distinguished from required history | §§12.2.1, 13.11 | WAL tail and inventory | next-contiguous zero segment | COMPLETE |
| 92 U | missing required interior segment prevents recovery | §13.11 | WAL tail and inventory | N/N+2 inventory | COMPLETE |
| 93 W | retention-floor omissions/extras follow checkpoint ownership | §§13.10–13.11, 14.14 | WAL tail, segment inventory, and recovery handoff / Crash Injection Framework | installed-floor inventory variants | COMPLETE |
| 94 V | recovery establishes valid end before READY/ordinary append | §§3.3.3, 13.19 | WAL tail, segment inventory, and recovery handoff / Database Lifecycle Tests | append blocked at each open phase | COMPLETE |
| 95 V | checksum is validated before trusting disk page_lsn | §§4.13, 13.14 | WAL tail, segment inventory, and recovery handoff | high-LSN checksum-bad page | COMPLETE |
| 96 V | trusted redo page_lsn less/equal/greater cases are exact | §§12.8–12.10, 13.13 | WAL tail, segment inventory, and recovery handoff | apply/skip/idempotence matrix | COMPLETE |
| 97 G | record-family min/max and one-unit-excess lengths are checked | §§4.3.2.4, 12.3 | Length, position, checkpoint, and lifetime cross-owners / Numeric Exhaustion and Terminal-Boundary Verification | synthetic widened builders | COMPLETE |
| 98 F | final LSN/exclusive-end arithmetic never wraps | §4.3.2.4 | Numeric Exhaustion and Terminal-Boundary Verification | `2^64-48`, end `2^64`, next rejection | COMPLETE |
| 99 G | segment index stops at `2^38-1` without name truncation | §§4.3.2.4, 12.2 | Numeric Exhaustion and Terminal-Boundary Verification / WAL segment namespace and creation | max and next-index fixtures | COMPLETE |
| 100 Y | WAL_POSITION_EXHAUSTED differs from legal-position ENOSPC | §§4.3.2.4, 12.12.4 | Numeric Exhaustion and Terminal-Boundary Verification / WAL error/result matrix | synthetic exhaustion vs injected filesystem fault | COMPLETE |
| 101 W | checkpoint record codecs and sequence links are exact | §§13.5–13.8 | registry / Crash Injection Framework | BEGIN/DATA/END byte vectors | COMPLETE |
| 102 W | checkpoint installation follows WAL/control durability prerequisites | §§13.5–13.10 | Crash Injection Framework / Database Lifecycle Tests | crash boundaries before/after install | COMPLETE |
| 103 W | checkpoint/DPT retains required full images and status bases | §§12.10, 13.9–13.10 | Crash Injection Framework / TXN_STATUS image/terminal crash prefixes | rec_lsn retention fixtures | COMPLETE |
| 104 V | WAL target ownership prevents cross-database/file/page replay | §§4.7, 12.5, 13.13 | WAL PageId and per-transaction ownership codecs / WAL tail, segment inventory, and recovery handoff | actual registry/catalog owner negatives | COMPLETE |
| 105 Y | WAL failure consequences respect first persistent statement write | §§15.7, 39.1 | Non-Crash WAL/MTR Failure Injection / Statement Failure and Transaction-State Tests | prewrite and postwrite fault rows | COMPLETE |
| 106 X | shutdown drains authorized WAL before service teardown | §§3.3.6, 12.12.5 | Database Lifecycle Tests | DRAINING append/flush/waiter order | COMPLETE |
| 107 V | recovery uses redo plus loser status, not user-DML physical undo | §§12.1, 13.15–13.16 | Recovery Property Tests | committed model plus loser visibility | COMPLETE |
| 108 V | only complete valid persisted WAL determines recovery and terminal authority | §§12.12, 13.11–13.20 | Recovery-prefix oracle / COMMIT Fault-Injection Tests / ABORT Fault-Injection Tests | exact persisted-prefix and terminal model | COMPLETE |
| 109 J | PAGE_DELTA writers never emit zero-patch records | §§12.8, 12.18 | PAGE_DELTA codec and payload rejection | zero-count construction fails before reservation; emitted-record inspection | COMPLETE |
| 110 L | BTREE_MTR writers never emit zero-page records | §§12.10.2, 12.18 | BTREE_MTR codec and nested-entry rejection | zero-page construction fails before reservation; emitted-record inspection | COMPLETE |
| 111 L | PATCH_SET writers never emit zero-patch entries | §§12.10.2, 12.18 | BTREE_MTR codec and nested-entry rejection | nested zero-count construction fails before reservation; emitted-record inspection | COMPLETE |
| 112 Z | WAL raw positional I/O is a bounded WAL-owned exception, not BufferPool ownership | §§7.3, 12.1, 12.13 | Deterministic WAL harness and independent oracles | ownership-seam instrumentation and injected lower I/O result | COMPLETE |
| 113 G | reservation preserves terminal WAL/status-image headroom without creating holes | §§4.3.2.4, 12.12.1 | Numeric Exhaustion and Terminal-Boundary Verification / Length, position, checkpoint, and lifetime cross-owners | credited-terminal record after general-capacity exhaustion | COMPLETE |
| 114 H | WAL_PAD affects framing only and never enters page/transaction redo | §§12.7.1, 13.12–13.13 | WAL record framing, header, CRC, padding, and segment tails | independently decoded PAD through analysis/redo | COMPLETE |
| 115 T | every BufferPool-managed WAL-protected page family obeys WAL-before-data | §12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data / Buffer management verification | parameterized page-family writeback gate | COMPLETE |
| 116 L | user abort does not physically reverse an authorized system BTREE_MTR | §§12.10.2–12.10.4, 13.13.3 | BTREE_MTR codec and nested-entry rejection / B+ Tree Verification | diagnostic owner abort plus recovery/reopen | COMPLETE |
| 117 N | active-registry state dominates resident status bits before runtime terminal publication | §§9.13–9.14, 12.10.5.2 | TXN_STATUS image/terminal crash prefixes / Transaction identity, snapshot, and status verification | pause after status-bit install before C3/C4 | COMPLETE |

Coverage totals: **COMPLETE 117; PARTIAL 0; MISSING 0; CONTRADICTORY 0.**

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

This subsection is the detailed persistent-codec and selection-verification owner for
[`ARCHITECTURE.md`](ARCHITECTURE.md) §§13.2–13.3. It specializes the independent-oracle
rules already used for WAL without replacing the byte layout, allocator, checkpoint, or
open semantics owned by Architecture. Numeric Exhaustion and Terminal-Boundary Verification
remains the owner of terminal allocator behavior; the Crash Injection Framework remains the
owner of checkpoint-installation and torn-publication prefixes.

##### Independent byte, endian, CRC, and decode oracles

Build each candidate slot in a test-owned 4096-byte array initialized to zero. Write every
field from the mathematical fixture value by explicit byte shifts, not by copying a host
structure or calling a production control encoder. Assemble the complete file by placing
slot 0 at file bytes `0..4095` and slot 1 at `4096..8191`, then assert a file length of
exactly 8192 bytes. Independently reject 4095-byte and 4097-byte slot representations and
whole files shorter or longer than 8192 bytes; a malformed whole-file frame cannot be made
canonical by finding a plausible slot prefix inside it.

The endian oracle compares every uint16, uint32, and uint64 byte individually against
least-significant-byte-first expectations. The decode oracle reads those bytes with a
separate test-side routine and compares the resulting values with the fixture constants.
A production encode/decode round trip is useful additional coverage but is never the byte
or semantic oracle because both directions may share the same offset or endian defect.

The CRC oracle uses an independent CRC32C implementation or independently generated fixed
vectors. First require the standard check value:

```text
CRC32C("123456789") = 0xE3069283
```

Then compute over all 4096 slot bytes while supplying logical zero for offsets `80..83`,
regardless of the stored checksum bytes, and store the result little-endian at offset 80.
Whole-slot CRC detects torn or random changes; exact reserved-zero and relational checks
separately enforce the supported-v1 grammar, so a checksum-valid malformed v1 slot remains
invalid.

Every primary negative fixture changes exactly one independent contract. Recompute and
store a correct CRC after changing a semantic field or reserved byte; retain every semantic
field when testing the CRC itself. This prevents checksum failure from masking a format or
relational validator. Expected classifications come from §§4.14 and 13.2, never from the
production decoder's returned category or validator execution order.

##### Distinctive valid-slot byte fixture

The canonical byte-oracle vector uses the following legal non-palindromic values. Its
checkpoint-bearing form is paired with independently framed retained WAL containing a
complete checkpoint whose BEGIN and END records occur at the listed aligned record starts,
whose END redo value is `0x0000000001020308`, and whose DPT justifies that redo bound. The
slot-level vector can also be tested before the checkpoint usability layer so those two
validation stages remain observable.

| Offset | Width | Field | Encoding | Canonical value/domain | Positive fixture / expected bytes | Single-defect negative fixture | Oracle |
|---:|---:|---|---|---|---|---|---|
| 0 | 8 | family magic | exact ASCII | `DBLUSCTL` | `44 42 4C 55 53 43 54 4C` | changed, shifted, lowercase, or embedded-NUL magic, one case at a time | byte comparison before version dispatch |
| 8 | 2 | `format_version` | LE uint16 | `1` | `01 00` | `0`; separately `2` for future dispatch | byte/endian and version-class oracle |
| 10 | 2 | `header_size` | LE uint16 | `88` | `58 00` | `0`, `87`, `89`, and another representable value separately | exact supported-v1 grammar |
| 12 | 4 | `flags` | LE uint32 | `0` | `00 00 00 00` | one assigned bit set | exact zero mask with recomputed CRC |
| 16 | 8 | `generation` | LE uint64 | `0x0102030405060708` | `08 07 06 05 04 03 02 01` | zero | byte/endian plus nonzero semantic check |
| 24 | 8 | `latest_checkpoint_lsn` | LE uint64 LSN | `0x0000000011223348` | `48 33 22 11 00 00 00 00` | zero while either other checkpoint field remains nonzero | byte/endian, triplet, then referenced-WAL oracle |
| 32 | 8 | `latest_checkpoint_end_lsn` | LE uint64 LSN | `0x0000000022334458` | `58 44 33 22 00 00 00 00` | value below BEGIN | byte/endian, relation, then referenced-WAL oracle |
| 40 | 8 | `checkpoint_redo_lsn` | LE uint64 LSN | `0x0000000001020308` | `08 03 02 01 00 00 00 00` | zero while BEGIN/END remain nonzero | byte/endian, triplet, then END/DPT oracle |
| 48 | 8 | `reserved_txn_id_end` | LE uint64 exclusive end | `1,048,578 = 2 + 2^20` | `02 00 10 00 00 00 00 00` | `1` | byte/endian plus §9.3 reservation oracle |
| 56 | 8 | `txn_status_reclaim_before` | LE uint64 exclusive cutoff | `97,922 = 2 + 3 * 32,640` | `82 7E 01 00 00 00 00 00` | below 2, above reservation end, or misaligned, separately | byte/endian plus §13.2.3 relation oracle |
| 64 | 4 | `next_file_id` | LE uint32 next value | `0x12345678` | `78 56 34 12` | zero | byte/endian plus FileId-domain oracle |
| 68 | 4 | `reserved32` | four zero bytes | `0` | `00 00 00 00` | one bit set | exact reserved-zero check with recomputed CRC |
| 72 | 8 | `next_catalog_object_id` | LE uint64 next value | `0x1122334455667788` | `88 77 66 55 44 33 22 11` | zero | byte/endian plus shared-ID-domain oracle |
| 80 | 4 | `crc32c` | LE uint32 | independent whole-slot checksum | `0x9673EF23` -> `23 EF 73 96` | one stored bit changed | independent CRC oracle; field logically zero during computation |
| 84 | 4 | `reserved32` | four zero bytes | `0` | `00 00 00 00` | one bit set | exact reserved-zero check with recomputed CRC |
| 88 | 4008 | reserved suffix | exact zero byte sequence | every byte zero | `00` repeated 4008 times through byte 4095 | set byte 88, a middle byte, or byte 4095, separately | full-range byte scan with recomputed CRC |

The positive oracle compares all 4096 bytes, not only the listed nonzero runs, and then
compares an independently decoded semantic object with every chosen value. The expected
checksum `0x9673EF23` is derived from exactly this vector, including 4008 suffix zeros and
logical zeros at bytes `80..83` during calculation.

##### Canonical initial-file fixture

Construct the fresh-control state independently from §13.2.2 rather than by invoking a
create path. Slot 0 uses generation 1, three zero checkpoint fields,
`reserved_txn_id_end=2`, `txn_status_reclaim_before=2`, `next_file_id=1`, and
`next_catalog_object_id=1`; all flags/reserved bytes are zero. Its independent CRC32C is
`0x530BD55D`, stored as `5D D5 0B 53`. Slot 1 is exactly 4096 zero bytes and is a
noncandidate, not a reason to reject the valid initial file.

| File region / fact | Exact initial fixture | Independent assertion |
|---|---|---|
| file framing | 8192 bytes | no short/long/trailing data accepted |
| slot 0 | canonical v1 bytes at `0..4095` | full byte comparison; CRC `5D D5 0B 53` |
| checkpoint state | BEGIN/END/redo all zero | no installed checkpoint relation accepted |
| allocator state | TxnId end 2, reclaim cutoff 2, FileId next 1, catalog-object next 1 | decoded values equal the four architecture constants |
| slot 1 | zero at every byte `4096..8191` | invalid/noncandidate slot without corrupting slot 0 |
| selected state | slot 0, generation 1 | open/control selection has one semantic authority |

##### Field grammar, checkpoint usability, and high-water procedures

Magic tests compare all eight bytes exactly. Mutate one byte, shift the sequence within the
slot, use lowercase, and inject an embedded NUL as separate fixtures; no fuzzy prefix or
substring match establishes ownership. Contrast a wrong-magic candidate paired with a valid
v1 slot against an exact-magic positive version greater than 1 paired with an older valid
v1 slot. The former permits ordinary independent-slot fallback; the latter returns
`UNSUPPORTED_DATABASE_FORMAT` before supported-slot fallback. This fail-closed future
dispatch prevents a v1 owner from selecting and later overwriting older state after a
recognized newer owner has published control metadata.

For supported version 1, vary `header_size`, `flags`, generation, both reserved32 fields,
and the three suffix locations independently. Version zero is an invalid/corrupt claimed
control slot. Generation zero is likewise invalid; generation `UINT64_MAX` is a valid
selected terminal value, but every operation requiring another control publication must
fail before wrap, slot write, or dependent state publication. The existing control-
generation procedure under Numeric Exhaustion and Terminal-Boundary Verification remains
the no-wrap owner.

Checkpoint testing has two explicit layers:

1. **Structural slot validity.** Accept exactly the all-zero no-checkpoint triplet, or three
   nonzero fields with `latest_checkpoint_lsn <= latest_checkpoint_end_lsn`. Reject each
   mixed-zero combination and END-before-BEGIN with a recomputed valid CRC.
2. **Recovery usability.** For a structurally valid checkpoint-bearing slot, independently
   decode the referenced retained BEGIN/DATA/END sequence and apply §§13.5–13.9: record
   starts and cross-links, complete contiguous DATA indexes, totals, CRC/framing, END redo
   value, and valid WAL range must agree. Test a complete sequence, an incomplete or
   malformed sequence, an unaligned/non-record-start pointer, and a pointer beyond valid WAL.

A newer structurally valid but checkpoint-unusable slot may fall back to an older candidate
only when all recovery objects referenced by the older candidate are retained and valid.
The checkpoint codecs in WAL Persistent Codec Verification and the checkpoint boundaries in
the Crash Injection Framework remain the complete sequence and installation procedures;
this subsection supplies their exact control-slot bytes and selection input.

Exercise high-water fields with the following matrix. “Persisted maximum” is a legal
terminal next-value state; the next allocation/update is the separately tested exhaustion
operation, not persisted wraparound. TxnId writers additionally prove exact `2^20` block
progression even though slot validation's local lower-bound check is stated separately.

| Field / case | Values | Byte-valid? | Relationally valid? | Slot usable? | Required observation / exhaustion owner |
|---|---|---:|---:|---:|---|
| TxnId initial minimum | `reserved_txn_id_end=2` | yes | yes | yes | no range reserved yet |
| TxnId normal block | `1,048,578 = 2 + 2^20` | yes | yes | yes | durable exact block precedes issue; lost suffix is never reused |
| TxnId maximum block end | `18,446,744,073,708,503,042` | yes | yes | yes | next exact block returns `TXN_ID_EXHAUSTED` without partial block or wrap |
| TxnId below minimum | `reserved_txn_id_end=1` | yes | no | no | invalid supported-v1 slot |
| reclaim initial | cutoff `2`, reservation end `2` | yes | yes | yes | first normal boundary |
| reclaim normal aligned | cutoff `32,642 = 2 + 32,640`, reservation end at least cutoff | yes | yes | yes | one complete status-page range retired at slot level |
| reclaim maximum aligned under terminal reservation | cutoff `18,446,744,073,708,474,242`; reservation end `18,446,744,073,708,503,042` | yes | yes | yes | largest representable page-aligned cutoff not above authority |
| reclaim below minimum | cutoff `1` | yes | no | no | invalid supported-v1 slot |
| reclaim misaligned | cutoff `3`, reservation end greater | yes | no | no | modulo-32,640 relation rejected |
| reclaim beyond reservation | cutoff `32,642`, reservation end `2` | yes | no | no | cutoff cannot outrun reserved TxnId authority |
| FileId initial/normal | `next_file_id=1` / another nonzero value | yes | yes | yes | durable next value precedes returned candidate |
| FileId exhausted next state | `next_file_id=UINT32_MAX` | yes | yes | yes | last returned ID was `UINT32_MAX-1`; next request returns `FILE_ID_EXHAUSTED` |
| FileId invalid | `next_file_id=0` | yes | no | no | invalid supported-v1 slot |
| catalog ID initial/normal | `next_catalog_object_id=1` / another nonzero value | yes | yes | yes | one shared TableId/IndexId/ConstraintId sequence |
| catalog ID exhausted next state | `next_catalog_object_id=UINT64_MAX` | yes | yes | yes | last returned ID was `UINT64_MAX-1`; next request returns `ID_EXHAUSTED` |
| catalog ID invalid | `next_catalog_object_id=0` | yes | no | no | invalid supported-v1 slot |

The byte fixture proves representation and slot-level relations. The persistent high-water
crash matrix proves durable consumption and nonreuse; full status-history reclamation proof
remains with the Chapter-14 procedures rather than being inferred from a control value.

##### CRC and format-classification matrices

| CRC fixture | Independent expected CRC | Stored CRC | CRC-valid? | Semantic-valid? | Required result |
|---|---|---|---:|---:|---|
| distinctive canonical bytes | `0x9673EF23` | `0x9673EF23` | yes | yes | valid candidate, subject to checkpoint usability |
| covered field bit flipped, checksum unchanged | independently changed `C'` | `0x9673EF23` | no | otherwise yes | invalid candidate / CRC corruption |
| stored CRC bit flipped | `0x9673EF23` | one-bit-different value | no | otherwise yes | invalid candidate / CRC corruption |
| semantic field invalid, CRC recomputed | independently changed `C'` | same `C'` | yes | no | invalid supported-v1 candidate; checksum does not legalize grammar |
| suffix byte invalid, CRC recomputed | independently changed `C'` | same `C'` | yes | no | invalid supported-v1 candidate; proves full suffix validation |
| checker includes stored bytes instead of logical zeros | `0x9673EF23` under canonical rule | implementation's self-referential result | no oracle agreement | otherwise yes | test fails; canonical slot is not redefined by the wrong calculation |

The format matrix uses “fallback” only for another independently valid/usable supported-v1
slot. A short/long whole file fails control-file framing even if one prefix looks plausible.

| Fixture | Owning format recognized? | CRC valid under applicable grammar? | v1 semantic grammar valid? | Candidate valid/usable? | Fallback allowed? | Required result |
|---|---:|---:|---:|---:|---:|---|
| canonical v1 slot | yes | yes | yes | yes, then checkpoint usability | N/A | accept candidate |
| wrong magic | no | N/A | N/A | no | yes | invalid/non-owning candidate; lifecycle identity rule decides if none remain |
| exact magic, version 0 | yes | valid v1-shaped checksum in isolated fixture | no | no | yes | invalid/corrupt candidate |
| exact magic, positive version greater than 1 | yes | not interpreted with v1 CRC | N/A | no | no | `UNSUPPORTED_DATABASE_FORMAT` |
| wrong `header_size` | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| flags nonzero | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| generation zero | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| checkpoint triplet structurally inconsistent | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| checkpoint sequence missing/malformed/beyond valid WAL | yes | yes | yes at slot layer | no for recovery | yes, only to retained valid older authority | control/checkpoint corruption if no usable candidate |
| `reserved_txn_id_end < 2` | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| reclaim cutoff below 2, misaligned, or above reservation | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| `next_file_id=0` | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| reserved field at 68 nonzero | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| `next_catalog_object_id=0` | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| stored CRC invalid | yes | no | otherwise yes | no | yes | invalid/corrupt candidate |
| reserved field at 84 nonzero | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| any suffix byte nonzero | yes | yes | no | no | yes | invalid/corrupt v1 candidate |
| 4095/4097-byte standalone slot or non-8192-byte file | insufficient canonical framing | N/A | no | no | no whole-file salvage | control framing/open failure |
| two valid slots, equal generation, identical bytes | yes | yes | yes | equivalent authority | N/A | deterministic semantic acceptance; no physical-slot choice is observable |
| two valid slots, equal generation, different legal contents | yes | yes | yes individually | no unique authority | no | `CORRUPT_DATABASE`/control corruption |

##### Slot selection, update, and crash procedures

Construct each slot independently, then assemble the pair in both physical orders where
applicable. Selection is by supported validity, checkpoint usability, and generation—not by
slot number.

| Slot 0 | Slot 1 | Selected authority | Open allowed? | Classification / reason |
|---|---|---|---:|---|
| valid generation G | all-zero initial noncandidate | slot 0 G | yes | canonical initial/one-valid case |
| valid G | valid G+1 | slot 1 G+1 | yes | highest valid usable generation |
| valid G+1 | valid G | slot 0 G+1 | yes | physical position is irrelevant |
| valid G | CRC-invalid current-v1 candidate | valid G | yes | ordinary independent-slot fallback |
| valid G | semantic-invalid current-v1 candidate | valid G | yes | ordinary independent-slot fallback |
| invalid current-v1 candidate | invalid current-v1 candidate | none | no | no usable control authority; no default recreation |
| valid G, bytes X | valid G, identical bytes X | semantic state X; either copy equivalent | yes | equal generation has one byte-identical authority |
| valid G, legal bytes X | valid G, different legal bytes Y | none | no | equal-generation split authority is corruption |
| valid older v1 G | exact-magic positive future version | none | no | `UNSUPPORTED_DATABASE_FORMAT`; downgrade fallback forbidden |
| valid v1 G | wrong-magic candidate | valid v1 G | yes | non-owning invalid candidate differs from future owner |
| older valid/usable G | newer structurally valid but checkpoint-unusable G+1 | older G only if all its objects remain retained and valid | conditional | descending usable-candidate rule; otherwise control/checkpoint corruption |

Map alternating publication to the persistent high-water crash matrix and add byte-exact
assertions at each boundary:

```text
select the nonselected physical slot
derive every unchanged field from the latest selected in-memory state
checked generation + 1
encode the complete 4096-byte vector and independent expected CRC
one exact-position 4096-byte write
successful fdatasync(database.control)
only then runtime publication of the new generation
```

Pause before the write, before the CRC field, inside the reserved suffix, after all bytes
but before known synchronization, after successful synchronization, and before/after runtime
publication. A partial slot is invalid and the old slot remains authority. If a crash occurs
after complete bytes but before the caller knows synchronization, select only from the bytes
that independently survive as a complete valid slot; either old or new authority is legal
according to that exact durable image, never according to the interrupted call's return path.
The old slot remains a fallback until a complete newer slot survives. Two independently
validated slots are necessary because a torn candidate update must not destroy the previous
durable authority.

The control-write event trace also proves §13.3: ordinary transaction begin/commit/abort
within an already reserved TxnId block causes no control write or sync, while TxnId block
reservation, FileId/catalog-object allocation, and successful checkpoint installation use
the serialized alternating path. The test observes architecture events and does not require
a particular mutex or production function name.

##### Cross-owner exhaustion and checkpoint mappings

The existing procedures remain primary owners for these dimensions:

| Control-field/protocol concern | Existing detailed procedure | Byte-exact specialization supplied here |
|---|---|---|
| generation terminal value | Numeric Exhaustion — control generation | exact generation bytes, valid `UINT64_MAX` selected slot, and no attempted wrapped slot |
| FileId terminal allocation | Numeric Exhaustion — FileId specialization | exact `next_file_id` bytes and zero-field corruption case |
| shared catalog-object terminal allocation | Numeric Exhaustion — shared catalog-object IDs | exact uint64 bytes, zero-field corruption, and one shared carrier |
| TxnId durable reservation | Numeric Exhaustion — TxnId terminal block and persistent crash matrix | exact `reserved_txn_id_end` bytes and relations to reclaim cutoff |
| alternating/torn update | Persistent high-water crash procedure and Crash Injection Framework | complete independent slot bytes, CRC, physical placement, and partial-prefix cases |
| checkpoint record validity | WAL persistent checkpoint codecs | exact control triplet bytes and structural-versus-usable distinction |
| checkpoint installation | Crash Injection Framework and shutdown/recovery lifecycle procedures | WAL-durable-before-control and control-sync-before-runtime-selection assertions |

##### Atomic control-file obligation coverage map

The inventory below splits §§13.2–13.3 when a separately corruptible byte range,
classification, durability boundary, or observable allocator result exists. No target count
is imposed. `COMPLETE` means this subsection or the named existing owner supplies an
independent deterministic procedure, not merely that Architecture states the rule.

| # / domain | Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Status |
|---:|---|---|---|---|---|
| 1 A | control basename is exactly `database.control` | §13.2 | this subsection — file fixture | exact owner-path fixture | COMPLETE |
| 2 A | complete control file is exactly 8192 bytes | §13.2 | independent byte oracle | exact/short/long whole-file fixtures | COMPLETE |
| 3 A | file contains exactly two slots | §13.2 | independent byte oracle | two-region assembly and no third/trailing region | COMPLETE |
| 4 A | slot 0 and slot 1 occupy `0..4095` and `4096..8191` | §13.2 | independent byte oracle | absolute-offset comparison | COMPLETE |
| 5 A | each slot representation is exactly 4096 bytes | §13.2.1 | independent byte oracle | 4095/4096/4097 cases | COMPLETE |
| 6 B | family magic occupies bytes `0..7` exactly | §13.2.1 | slot-format matrix | exact ASCII and four isolated mutations | COMPLETE |
| 7 B | format version occupies bytes `8..9` | §13.2.1 | slot-format matrix | distinctive LE uint16 vector | COMPLETE |
| 8 B | header size occupies bytes `10..11` and equals 88 | §13.2.1 | slot-format/classification matrices | 0/87/88/89 cases | COMPLETE |
| 9 B | flags occupy bytes `12..15` and equal zero | §13.2.1 | slot-format matrix | recomputed-CRC one-bit mutation | COMPLETE |
| 10 B | generation occupies bytes `16..23` | §13.2.1 | distinctive vector | independent LE uint64 bytes | COMPLETE |
| 11 B | checkpoint BEGIN LSN occupies bytes `24..31` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes | COMPLETE |
| 12 B | checkpoint END LSN occupies bytes `32..39` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes | COMPLETE |
| 13 B | checkpoint redo LSN occupies bytes `40..47` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes | COMPLETE |
| 14 B | reserved TxnId end occupies bytes `48..55` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes | COMPLETE |
| 15 B | status reclaim cutoff occupies bytes `56..63` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes | COMPLETE |
| 16 B | next FileId occupies bytes `64..67` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint32 bytes | COMPLETE |
| 17 B | bytes `68..71` are reserved zero | §13.2.1 | slot-format matrix | one-bit defect with valid recomputed CRC | COMPLETE |
| 18 B | next catalog-object ID occupies bytes `72..79` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes | COMPLETE |
| 19 B | CRC field occupies bytes `80..83` | §13.2.1 | CRC matrix | exact expected LE CRC bytes | COMPLETE |
| 20 B | bytes `84..87` are reserved zero | §13.2.1 | slot-format matrix | one-bit defect with valid recomputed CRC | COMPLETE |
| 21 B | bytes `88..4095` are all zero | §13.2.1 | slot-format matrix | first/middle/final isolated defects | COMPLETE |
| 22 C | every uint16 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint16 values | COMPLETE |
| 23 C | every uint32 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint32 values, including CRC | COMPLETE |
| 24 C | every uint64 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint64 values | COMPLETE |
| 25 C | codec does not depend on host struct layout/endianness | §§4.2, 13.2.1 | byte oracle | byte-shift builder and full-array comparison | COMPLETE |
| 26 C | decoded semantic values are independently checked | §13.2.1 | decode oracle | test-side read compared with chosen constants | COMPLETE |
| 27 D | CRC32C covers all 4096 slot bytes | §13.2.1 | CRC oracle | distinctive fixed vector | COMPLETE |
| 28 D | CRC bytes are logically zero during calculation | §13.2.1 | CRC oracle | canonical-versus-self-referential calculation | COMPLETE |
| 29 D | CRC implementation is independent | §13.2.1 | CRC oracle | standard check vector plus test-side implementation | COMPLETE |
| 30 D | stored CRC is little-endian | §13.2.1 | slot-format/CRC matrices | `23 EF 73 96` and initial `5D D5 0B 53` | COMPLETE |
| 31 D | covered-byte or stored-CRC corruption is detected | §§13.2.1, 13.2.3 | CRC matrix | separate payload/stored-bit flips | COMPLETE |
| 32 D | CRC-valid malformed v1 remains invalid | §§4.14, 13.2.3 | single-defect policy / CRC matrix | semantic mutation with recomputed CRC | COMPLETE |
| 33 E | exact magic plus version 1 selects v1 grammar | §§4.14.2, 13.2.3 | format-classification matrix | canonical positive slot | COMPLETE |
| 34 E | exact magic plus version 0 is invalid/corrupt | §§4.14.2, 13.2.3 | format-classification matrix | isolated zero-version slot | COMPLETE |
| 35 E | exact magic plus positive future version is unsupported | §§4.14.2, 4.14.6, 13.2.3 | format-classification matrix | version-2 discriminator fixture | COMPLETE |
| 36 E | future owning version blocks fallback to older v1 | §§4.14.6, 13.2.3 | slot-selection matrix | future-plus-valid-v1 pair | COMPLETE |
| 37 E | wrong magic/current-v1 invalidity differs from future ownership | §§4.14.6, 13.2.3 | magic procedure / selection matrix | wrong-magic-plus-valid-v1 contrast | COMPLETE |
| 38 F | supported-v1 flags and all reserved bytes are exact zero | §§4.14.3, 13.2.1–13.2.3 | slot-format matrix | isolated recomputed-CRC mutations | COMPLETE |
| 39 F | generation must be nonzero | §13.2.3 | format-classification matrix | generation-zero fixture | COMPLETE |
| 40 F | highest valid usable generation is selected | §13.2.3 | slot-selection matrix | G/G+1 in both positions | COMPLETE |
| 41 F | physical slot number does not determine authority | §13.2.3 | slot-selection matrix | reversed generation pair | COMPLETE |
| 42 F | equal-generation identical slots are one equivalent authority | §13.2.3 | slot-selection matrix | byte-identical valid pair | COMPLETE |
| 43 F | equal-generation different contents are corruption | §13.2.3 | slot-selection matrix | CRC-correct legal X/Y pair | COMPLETE |
| 44 F | generation never wraps | §§4.3.2, 13.2.3 | Numeric Exhaustion — control generation | selected `UINT64_MAX`, next update rejected | COMPLETE |
| 45 G | no-checkpoint triplet is exactly all zero | §13.2.3 | checkpoint structural procedure | all-zero positive and mixed-zero negatives | COMPLETE |
| 46 G | installed-checkpoint triplet is entirely nonzero | §13.2.3 | checkpoint structural procedure | complete positive and each mixed-zero case | COMPLETE |
| 47 G | checkpoint BEGIN is not after END | §13.2.3 | checkpoint structural procedure | END-before-BEGIN fixture | COMPLETE |
| 48 G | checkpoint-bearing slot is usable only with a valid retained sequence | §§13.2.3, 13.5–13.9 | checkpoint usability procedure / WAL codec | complete, malformed, absent, and out-of-tail cases | COMPLETE |
| 49 G | older fallback requires all referenced objects retained and valid | §13.2.3 | slot-selection matrix / Crash Injection Framework | usable and unusable older-candidate variants | COMPLETE |
| 50 H | initial slot 0 has the exact canonical field values | §13.2.2 | initial-file matrix | full 4096-byte vector and CRC | COMPLETE |
| 51 H | initial slot 1 is all zero and a noncandidate | §13.2.2 | initial-file matrix | full 4096-byte zero comparison | COMPLETE |
| 52 I | reserved TxnId end is at least 2 | §13.2.3 | high-water matrix | values 2 and 1 | COMPLETE |
| 53 I | TxnId reservation uses durable exact blocks and forbids reuse | §§9.3, 13.2.4 | Numeric Exhaustion — TxnId / high-water mapping | first, normal, terminal, and crash-gap cases | COMPLETE |
| 54 I | reclaim cutoff is at least 2 | §13.2.3 | high-water matrix | values 2 and 1 | COMPLETE |
| 55 I | reclaim cutoff does not exceed reserved TxnId end | §13.2.3 | high-water matrix | isolated beyond-end fixture | COMPLETE |
| 56 I | reclaim cutoff obeys `(value-2) mod 32640 = 0` | §13.2.3 | high-water matrix | aligned and misaligned fixtures | COMPLETE |
| 57 I | next FileId is nonzero | §§13.2.3, 13.2.5 | high-water matrix | values 1 and 0 | COMPLETE |
| 58 I | FileId next value is durable before return and never reused | §13.2.5 | Numeric Exhaustion — persistent high-water | crash before/after sync and terminal state | COMPLETE |
| 59 I | next catalog-object ID is nonzero | §§13.2.3, 13.2.6 | high-water matrix | values 1 and 0 | COMPLETE |
| 60 I | catalog IDs share one durable no-reuse sequence | §13.2.6 | Numeric Exhaustion — shared catalog IDs | alternating typed requests and crash gaps | COMPLETE |
| 61 J | one valid and one zero/invalid slot selects the valid slot | §§13.2.2–13.2.3 | slot-selection matrix | zero, CRC-invalid, semantic-invalid variants | COMPLETE |
| 62 J | two invalid slots provide no default/recreated authority | §13.2.3 | slot-selection matrix / lifecycle open faults | paired invalid fixtures | COMPLETE |
| 63 J | checkpoint-unusable newer candidate uses descending fallback rules | §13.2.3 | slot-selection/checkpoint usability procedures | newer bad reference plus older variants | COMPLETE |
| 64 K | updates target the nonselected slot | §13.2.4 | alternating update procedure | physical-slot event trace | COMPLETE |
| 65 K | update derives from latest state so unrelated fields are preserved | §13.2.4 | alternating update procedure | concurrent distinct-field transitions | COMPLETE |
| 66 K | update uses checked old generation plus one | §§13.2.3–13.2.4 | alternating update / Numeric Exhaustion | normal and terminal generation cases | COMPLETE |
| 67 K | update encodes a complete slot and canonical CRC | §13.2.4 | alternating update / byte oracle | prewrite vector comparison | COMPLETE |
| 68 K | control write is one exact-position 4096-byte transfer request | §13.2.4 | alternating update procedure | exact/short transfer instrumentation | COMPLETE |
| 69 K | control file is synchronized before runtime publication | §13.2.4 | alternating update / crash framework | barriers before/after `fdatasync` | COMPLETE |
| 70 K | torn candidate preserves prior valid authority | §13.2.4 | alternating update procedure | stops before CRC/in suffix and reopen | COMPLETE |
| 71 K | complete uncertain write is judged from surviving bytes, not call outcome | §§13.2.3–13.2.4 | persistent high-water crash procedure | exact surviving old/new images | COMPLETE |
| 72 K | concurrent control transitions are serialized without lost fields | §13.2.4 | alternating update procedure | TxnId/FileId/checkpoint/reclaim contenders | COMPLETE |
| 73 L | generation exhaustion prevents dependent publication | §§4.3.2, 13.2.3 | Numeric Exhaustion — control generation | selected maximum fixture | COMPLETE |
| 74 L | FileId exhaustion preserves terminal next value | §§4.3.2.1, 13.2.5 | Numeric Exhaustion — FileId | last candidate and next failure | COMPLETE |
| 75 L | catalog-object exhaustion preserves terminal next value | §§4.3.2.1, 13.2.6 | Numeric Exhaustion — shared IDs | last candidate and next failure | COMPLETE |
| 76 L | TxnId exact-block exhaustion preserves nonreuse | §§4.3.2.1, 9.3 | Numeric Exhaustion — TxnId | terminal block and next failure | COMPLETE |
| 77 M | ordinary transactions do not rewrite/sync control within a reserved block | §13.3 | update-frequency event trace | ordinary begin/commit/abort positive control | COMPLETE |
| 78 M | global high-water/checkpoint transitions use durable control publication | §13.3 | update-frequency event trace / existing crash owners | each listed transition routed through alternating path | COMPLETE |
| 79 N | checkpoint WAL is durable before control installation | §13.5 | Crash Injection Framework / checkpoint codec | before/after END durability boundary | COMPLETE |
| 80 N | control sync precedes in-memory checkpoint/FPI publication | §§13.2.4, 13.5 | Crash Injection Framework / alternating update | before/after control sync and runtime epoch boundary | COMPLETE |
| 81 O | short/long file or slot framing cannot be salvaged | §§13.2–13.2.1 | independent byte oracle | exact 4095/4097 and 8191/8193 fixtures | COMPLETE |
| 82 O | corrupt v1, unsupported future owner, and checkpoint-unusable state remain distinct | §§4.14, 13.2.3 | classification and selection matrices | three isolated pair/file fixtures | COMPLETE |

Control-file coverage totals: **COMPLETE 82; PARTIAL 0; MISSING 0;
CONTRADICTORY 0.**

##### Required recovery inputs

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

This section is the detailed procedural owner for `ARCHITECTURE.md` §§10.1–10.6.
It verifies the visibility decision and its error boundary using Chapter 5's validated
physical tuple metadata and Chapter 9's snapshot/status inputs. Transaction-status lookup
precedence remains owned by Transaction identity, snapshot, and status verification;
write-conflict/current-owner decisions remain §11.10.4 and the Locking/UNIQUE sections;
physical reclamation remains the Vacuum and Reclamation Tests.

#### Deterministic harness, fixture canonicality, and independent oracle

The table-driven visibility harness controls these architecture-level inputs without
requiring a production function name or return type:

```text
tuple xmin / xmax / cmin / cmax
physical tuple/RID owner and persisted-versus-runtime-only provenance
snapshot owner_txn_id / command_id / xmax / sorted active
canonical Status(xmin/xmax) result or one injected lower-layer failure
scan source: direct heap candidate, sequential scan, or B+ candidate RID
```

The observable result domain is exactly:

```text
VISIBLE
INVISIBLE
ERROR with the canonical error family
```

An implementation may represent that result with a status return, tagged/expected-like
value, or another compatible mechanism. Tests assert semantics and exact propagated error,
not a C++ type, exception choice, or helper name.

Every fixture is canonical except for the single dimension under test. A semantic-status
fixture has a valid HEAP owner, checksum, PageId, NORMAL slot, complete §§5.7–5.13 tuple,
normal referenced TxnId, valid snapshot, and canonical TXN_STATUS/runtime lookup inputs. A
future-command fixture introduces no unrelated status or structure failure. An index case
uses a valid B+ path, exact relation/RID identity, and a structurally valid heap tuple. A
malformed tuple-header case is instead routed to the existing Storage Verification / §4.13.3
validation oracle before Chapter-10 semantic evaluation.

The independent reference evaluator uses mathematical fixture values rather than calling
the production visibility routine. It performs, in order:

1. structural/owner precondition classification by the existing heap validator;
2. SELF command-causality checks, including future fields and same-owner `cmax < cmin`;
3. creator classification as visible, invisible, or exact error;
4. deleter classification only for a visible creator, as visible, invisible, or exact
   error;
5. exact error propagation to the scan/query owner.

The causality precheck occurs before creator invisibility can short-circuit. Status lookup
calls are instrumented so the oracle can also require that FROZEN/SELF cases bypass
persistent lookup and that a lower-layer error is neither converted to a Boolean nor
followed by a different status source. Concurrency, retry, and status-publication fixtures
use deterministic barriers; sleeps and repeated random reproduction are insufficient.

#### Creator and deleter valid-state regression

Use direct values around one valid snapshot, for example owner `50`, command `C=10`,
`xmax=100`, and controlled sorted `active`. Use separate TxnIds for every status source so
one lookup result cannot accidentally satisfy another row.

The creator matrix is:

| Creator fixture | Status/snapshot relation | Expected creator result | Deleter consulted? |
|---|---|---|---:|
| `xmin=FROZEN_TXN_ID` | no status-page access | VISIBLE | yes |
| `xmin=owner`, `cmin=C-1` | earlier SELF command | VISIBLE | yes |
| `xmin=owner`, `cmin=C` | current SELF command | INVISIBLE | no, after causality precheck |
| other creator | COMMITTED, `xmin < xmax`, absent from `active` | VISIBLE | yes |
| other creator | COMMITTED, `xmin < xmax`, present in `active` | INVISIBLE | no |
| other creator | COMMITTED, `xmin == xmax` | INVISIBLE | no |
| other creator | COMMITTED, `xmin > xmax` | INVISIBLE | no |
| other creator | IN_PROGRESS | INVISIBLE | no |
| other creator | ABORTED | INVISIBLE | no |

The active-at-capture COMMITTED case first captures the TxnId in `active`, then publishes
COMMITTED through the Chapter-9 barrier. The immutable snapshot must retain membership and
the creator remains invisible. This distinguishes terminal status from snapshot-visible
history.

For every creator-visible row, apply the complete deleter matrix:

| Deleter fixture | Status/snapshot relation | Expected tuple result |
|---|---|---|
| `xmax=INVALID_TXN_ID` | canonical no-delete sentinel; no lookup | VISIBLE |
| `xmax=owner`, `cmax=C-1` | earlier SELF command | INVISIBLE |
| `xmax=owner`, `cmax=C` | current SELF command | VISIBLE |
| other deleter | COMMITTED, `xmax < snapshot.xmax`, absent from `active` | INVISIBLE |
| other deleter | COMMITTED, `xmax < snapshot.xmax`, present in `active` | VISIBLE |
| other deleter | COMMITTED, `xmax >= snapshot.xmax` | VISIBLE |
| other deleter | IN_PROGRESS | VISIBLE; no visibility wait |
| other deleter | ABORTED | VISIBLE |

Compose every creator-visible normal case with each deleter row and separately assert that
creator-invisible normal cases remain invisible without status-dependent deleter lookup.
The causality precheck still runs first, so impossible SELF deleter metadata cannot be hidden
by this short-circuit. These tables preserve every normal Boolean result while the error
tables below exercise the non-Boolean branches.

#### Status-result and lookup-failure propagation

Construct separate creator and creator-visible/deleter fixtures for each unusable status.
The TXN_STATUS decoder must succeed for `INVALID` and `RESERVED`; semantic rejection occurs
only after Chapter-9 precedence returns that decoded result to visibility. `RETIRED` is
constructed only by a durably published Chapter-14 cutoff; the negative fixture then
retains a visibility-dependent tuple reference in deliberate violation of the retirement
proof.

| Side | Controlled result/failure | Provenance | Expected result/error | May be treated as invisible/ineffective? | Continuation owner |
|---|---|---|---|---:|---|
| creator | RETIRED | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | INVALID | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | RESERVED | recognized v1 bits plus persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | status-page I/O failure | lower layer | propagate exact I/O error | no | §39.1 and lower owner |
| creator | checksum/owner/corruption failure | lower layer | propagate exact corruption error | no | §39.1 and §§4.13–4.14 |
| creator | recognizable future format | lower layer | propagate exact unsupported-format error | no | §§4.14, 39.1 |
| creator | recovery/storage failure | lower layer | propagate exact lower-layer error | no | Chapters 13 and 39 |
| deleter | RETIRED | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | INVALID | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | RESERVED | recognized v1 bits plus persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | status-page I/O failure | lower layer | propagate exact I/O error | no | §39.1 and lower owner |
| deleter | checksum/owner/corruption failure | lower layer | propagate exact corruption error | no | §39.1 and §§4.13–4.14 |
| deleter | recognizable future format | lower layer | propagate exact unsupported-format error | no | §§4.14, 39.1 |
| deleter | recovery/storage failure | lower layer | propagate exact lower-layer error | no | Chapters 13 and 39 |

For each lookup failure, record one call and the exact returned diagnostic. The creator
case must not become INVISIBLE; the deleter case must not become an ineffective delete and
VISIBLE tuple. The §39.1 first-published-write matrix, rather than this visibility harness,
decides whether a propagated operation failure leaves the transaction active, mandates
ABORT, or makes storage noncontinuable.

The sentinel/status distinction has direct coverage:

| Condition | Decoder/lookup result | Valid? | Visibility consequence |
|---|---|---:|---|
| tuple `xmax == INVALID_TXN_ID` | no status lookup | yes | no deleter; creator-visible tuple is VISIBLE |
| normal `xmin` resolves INVALID | valid `00` decode, required outcome absent | no semantic state | `CORRUPT_HEAP` |
| normal `xmax` resolves INVALID | valid `00` decode, required outcome absent | no semantic state | `CORRUPT_HEAP` |

The RESERVED fixture first proves successful v1 `11` decode through the Chapter-9 byte
oracle, then passes the recognized nonterminal result to creator/deleter visibility and
expects `CORRUPT_HEAP`. It must not report malformed bits or unsupported format. Conversely,
a recognizable future status-file/page format fails during format dispatch and propagates
its exact unsupported-format result; it is not relabeled `CORRUPT_HEAP` merely because no
visibility decision can be made.

The compact error/visibility summary prevents normal Boolean cases and error cases from
sharing an oracle:

| Input class | Side | Expected semantic result | Exact error family | May be treated as ordinary invisible/ineffective? | Architecture owner |
|---|---|---|---|---:|---|
| valid COMMITTED before snapshot | creator | VISIBLE | — | no | §10.2 |
| valid IN_PROGRESS | creator | INVISIBLE | — | yes | §10.2 |
| valid ABORTED | creator | INVISIBLE | — | yes | §§10.1–10.2 |
| valid COMMITTED delete before snapshot | deleter | INVISIBLE tuple | — | yes, as an effective delete | §10.3.3 |
| valid IN_PROGRESS | deleter | VISIBLE tuple | — | yes, as an ineffective delete | §10.3.3 |
| valid ABORTED | deleter | VISIBLE tuple | — | yes, as an ineffective delete | §§10.1, 10.3.3 |
| dependent RETIRED | either | ERROR | persisted `CORRUPT_HEAP` | no | §10.4 / §14.14 |
| dependent INVALID | either | ERROR | persisted `CORRUPT_HEAP` | no | §10.4 |
| dependent RESERVED | either | ERROR after valid decode | persisted `CORRUPT_HEAP` | no | §§9.11.1, 10.4 |
| lower-layer lookup failure | either | ERROR | exact lower-layer family | no | §10.4 / §39.1 |
| SELF `cmin>C` | creator | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §§10.2, 10.4 |
| SELF `cmax>C` | deleter | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §§10.3.2, 10.4 |
| same-owner `cmax<cmin` | precheck | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §10.4 |

#### SELF command boundaries and causal precheck

Use explicit `C=10` fixtures rather than arithmetic loops. Separate checked boundary cases
cover CommandId zero/maximum through Numeric Exhaustion and Terminal-Boundary Verification;
no fixture computes `C-1` or `C+1` at an overflow boundary.

| Field | Relation to `C` | Valid? | Ordinary visibility meaning | Invalid error |
|---|---|---:|---|---|
| `cmin=9` | `< C` | yes | SELF creator visible | — |
| `cmin=10` | `== C` | yes | SELF creator invisible to same-command rescan | — |
| `cmin=11` | `> C` | no | no Boolean result | persisted `CORRUPT_HEAP`; runtime-only invariant failure |
| `cmax=9` | `< C` | yes | SELF delete effective; old tuple invisible | — |
| `cmax=10` | `== C` | yes | SELF delete not yet effective; old tuple visible | — |
| `cmax=11` | `> C` | no | no Boolean result | persisted `CORRUPT_HEAP`; runtime-only invariant failure |

Run each invalid relation once as a canonical persisted NORMAL tuple and once through
abstract test-only instrumentation immediately before invalid runtime metadata could be
published. The first must use `CORRUPT_HEAP`; the second must enter §39.1.3's internal
invariant path. No public unsafe-construction API is required.

The same-transaction causality matrix is:

| Case | `xmin` owner? | `xmax` owner? | `cmin` | `cmax` | `C` | Valid? | Expected result |
|---|---:|---:|---:|---:|---:|---:|---|
| earlier insert | yes | no | 9 | — | 10 | yes | creator visible |
| current insert | yes | no | 10 | — | 10 | yes | creator invisible |
| current-command delete of earlier version | no | yes | — | 10 | 10 | yes | old tuple visible |
| later-command delete of self-created version | yes | yes | 8 | 9 | 10 | yes | old tuple invisible |
| future creator | yes | no | 11 | — | 10 | no | error before Boolean result |
| future deleter | no | yes | — | 11 | 10 | no | error before Boolean result |
| delete precedes creation | yes | yes | 10 | 9 | 10 | no | error before creator-current invisibility short-circuit |

The last fixture is the ordering oracle: creator evaluation alone would return INVISIBLE
because `cmin == C`, but the required result is `CORRUPT_HEAP`. This proves command-causality
validation precedes semantic short-circuiting. Do not add causal prohibitions beyond those
listed by §10.4 and the compatible §11.10.4 current-owner rule.

#### Statement effects and UPDATE version pairs

Direct physical-pair tests use two NORMAL RIDs and evaluate each candidate independently;
ordinary visibility does not infer a logical row identity from `prev`. Current-command
INSERT (`cmin=C`) is invisible to ordinary rescan, while an earlier-command INSERT
(`cmin<C`) is visible. Current-command DELETE (`cmax=C`) leaves the old tuple visible,
while an earlier-command DELETE (`cmax<C`) hides it.

The mandatory UPDATE matrix is:

| Updater relation | Status/snapshot relation | Old `xmax` interpretation | New `xmin` interpretation | Old visible? | New visible? | Emitted ordinary version |
|---|---|---|---|---:|---:|---|
| SELF current command | owner, `cmax=C`, `cmin=C` | current delete ineffective | current creator hidden | yes | no | old |
| SELF later command | owner, `cmax<C`, `cmin<C` | earlier delete effective | earlier creator visible | no | yes | new |
| other IN_PROGRESS | status IN_PROGRESS | delete ineffective | creator invisible | yes | no | old |
| other COMMITTED before snapshot | TxnId below `xmax`, absent from `active` | delete effective | creator visible | no | yes | new |
| other COMMITTED at/after `xmax` | TxnId `>=xmax` | delete too new | creator too new | yes | no | old |
| other active at capture, later COMMITTED | TxnId below `xmax`, retained in `active` | delete too new to snapshot | creator invisible to snapshot | yes | no | old |
| other ABORTED | status ABORTED | delete ineffective | creator invisible | yes | no | old |

Each row asserts exactly one emitted ordinary version. The active-at-capture case pauses
commit after snapshot capture and proves later terminal publication does not rewrite the
captured set. Aborted cases retain both physical versions and index entries to prove logical
selection does not require physical undo.

#### Isolation, retry, recovery, and retirement cross-checks

The existing Snapshot registration, representation, and lifetime procedures plus Isolation
Tests remain the detailed isolation owners. Add Chapter-10 result assertions at their
existing barriers:

| Case | Existing procedure | Chapter-10 result oracle |
|---|---|---|
| READ COMMITTED stable attempt | Chapter-9 RC snapshot lifetime | one snapshot for all tuples/operators; a commit after capture does not appear mid-attempt |
| READ COMMITTED next statement | Isolation Tests | fresh snapshot may expose the later commit |
| READ COMMITTED own writes | SELF matrix | earlier-command writes visible; current-command ordinary rescan hidden as specified |
| allowed pre-write retry | Statement Failure / Chapter-9 retry fixture | fresh snapshot, same logical CommandId, no published SELF metadata to trigger a future-command error |
| conflict after published write | Statement Failure and Transaction-State Tests | no same-TxnId retry; canonical MUST_ABORT/ABORT path |
| REPEATABLE READ first statement | Chapter-9 RR fixture | first ordinary statement captures the transaction snapshot |
| REPEATABLE READ later statements | Isolation Tests | external snapshot fields stable; later external commits remain invisible |
| REPEATABLE READ own later writes | SELF matrix | updated command boundary exposes own earlier commands without changing external membership |
| isolation identity | Chapter-9 isolation identity / Isolation Tests | REPEATABLE READ is snapshot isolation, not SERIALIZABLE |

Recovery Property Tests use known WAL prefixes and add direct tuple outcomes: a crash loser
creator is ABORTED/invisible; a crash loser deleter is ABORTED/ineffective; a durable
committed updater is classified from reconstructed terminal authority without requiring
heap/status-page force; and no precrash active registry or terminal cache is consulted after
reopen. The old/new matrix is rerun after READY against a newly captured snapshot.

Vacuum and Reclamation Tests remain the positive proof owner for §§14.13.1–14.14.3. They
must show that aborted-`xmax` normalization and creator freezing preserve visibility, that
their page mutations follow the existing WAL/page-LSN procedure, and that status retirement
is published only after all surviving visibility-dependent metadata is independent. The
negative RETIRED fixture above deliberately violates that proof and must fail; a missing
status page alone must never synthesize RETIRED. SQL invisibility is not global
reclaimability, and a persisted HEAP `DEAD` slot is not merely a synonym for an invisible
NORMAL version.

#### Heap and index candidate error propagation

Sequential scans validate each candidate NORMAL tuple through Chapter 5/§4.13.3 and then
apply the same visibility oracle under the statement's one registered snapshot. A semantic
visibility error terminates through the canonical query/§39 path; it is not a skipped row.
All operators/subqueries in one statement share the same snapshot and command boundary as
the existing Scan, Pipeline, and Subquery tests require.

For B+ scans, §8.22.2 remains authoritative: an index hit is a physical RID candidate. The
index/recheck matrix is:

| B+ candidate | Heap visibility result | Scan/query action | May silently skip? | Expected error |
|---|---|---|---:|---|
| valid RID | VISIBLE | emit decoded row | no | — |
| valid RID | INVISIBLE | skip this physical candidate and continue | yes | — |
| valid RID | creator/deleter RETIRED | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | creator/deleter INVALID | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | creator/deleter RESERVED | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | status lookup I/O/corruption/unsupported failure | stop and propagate | no | exact lower-layer error |
| valid RID | future SELF `cmin/cmax` or invalid causal order | stop and propagate | no | persisted `CORRUPT_HEAP` |

Repeat the error rows through an ordinary sequential scan. Include valid later candidates
after the failing one and assert none are emitted as a successful continuation. An
INVISIBLE or ERROR result for one physical candidate must not cause ordinary visibility to
follow `prev`; ERROR especially must not become “try predecessor.” Chapter 5 owns the
persisted link and Chapter 14 owns maintenance/splicing/reuse. Randomized MVCC and scan
stress remains complementary to these deterministic fixtures.

#### Chapter 10 high-level domain/case matrix

| Domain/case | Deterministic fixture/oracle | Architecture reference | Verification owner | Status |
|---|---|---|---|---|
| creator normal truth table | direct creator matrix plus active-at-capture barrier | §§10.2, 10.6 | MVCC Visibility Tests | COMPLETE |
| creator unusable status | canonical RETIRED/INVALID/RESERVED inputs | §§10.2, 10.4 | Status propagation matrix | COMPLETE |
| deleter normal truth table | creator-visible tuple crossed with deleter matrix | §§10.3–10.4 | MVCC Visibility Tests | COMPLETE |
| deleter unusable status | canonical RETIRED/INVALID/RESERVED inputs | §§10.3.3–10.4 | Status propagation matrix | COMPLETE |
| lookup error propagation | one injected lower-layer error per side | §10.4 | Status propagation matrix | COMPLETE |
| future `cmin` | persisted/runtime-only `C+1` | §§10.2, 10.4 | SELF command matrix | COMPLETE |
| future `cmax` | persisted/runtime-only `C+1` | §§10.3.2, 10.4 | SELF command matrix | COMPLETE |
| same-owner causal order | `cmin=C`, `cmax=C-1` precheck oracle | §§10.4, 10.6 | Causality matrix | COMPLETE |
| current/later INSERT | SELF `cmin==C` and `<C` | §10.2 | Statement-effects fixtures | COMPLETE |
| current/later DELETE | SELF `cmax==C` and `<C` | §10.3.2 | Statement-effects fixtures | COMPLETE |
| UPDATE old/new pair | all seven updater rows | §§10.1–10.4 | UPDATE matrix | COMPLETE |
| READ COMMITTED | stable attempt, next statement, retry | §§9.9, 10.2–10.4 | Snapshot and Isolation Tests cross-reference | COMPLETE |
| REPEATABLE READ | retained external snapshot plus SELF writes | §§9.10, 10.2–10.4 | Snapshot and Isolation Tests cross-reference | COMPLETE |
| index candidate recheck | visible/invisible/error rows | §§8.22.2, 10.4 | Index recheck matrix / Scan Tests | COMPLETE |
| heap scan | valid NORMAL candidates with Boolean/error outcomes | §§5.17, 10.4 | Heap scan propagation fixture | COMPLETE |
| recovery | loser/committed updater after READY | §§10.1, 13.13–13.19 | Recovery Property Tests cross-reference | COMPLETE |
| retirement boundary | positive proof plus dependent-RETIRED negative | §§10.4–10.5, 14.13–14.14 | Vacuum/Reclamation cross-reference | COMPLETE |
| no predecessor fallback | invisible and error candidates with valid `prev` | §§5.7.4, 10.4, 14.10 | Heap/index propagation fixture | COMPLETE |

#### Chapter 10 architecture-obligation coverage map

The atomic inventory contains 96 obligations. `COMPLETE` means a deterministic procedure
or a precise existing procedural owner supplies an independent oracle; an architecture
statement alone is not coverage.

| # | Domain | Atomic obligation | Architecture owner | Verification owner / procedure/reference | Status |
|---:|---|---|---|---|---|
| 1 | STRUCTURAL PRECONDITION | A candidate is a validated NORMAL tuple before semantic visibility | §§4.13.3, 5.17, 10.4 | Fixture canonicality plus Storage Verification | COMPLETE |
| 2 | STRUCTURAL PRECONDITION | `xmin` structural domain is FROZEN or normal TxnId | §§4.13.3, 5.7.1 | Heap validation cross-reference | COMPLETE |
| 3 | STRUCTURAL PRECONDITION | `xmax` domain is INVALID sentinel or normal non-FROZEN TxnId | §§4.13.3, 5.7.2 | Heap validation cross-reference | COMPLETE |
| 4 | STRUCTURAL PRECONDITION | Snapshot owner, command, horizon, and active inputs are canonical | §§9.7–9.8, 10.4 | Chapter-9 snapshot fixture | COMPLETE |
| 5 | STRUCTURAL PRECONDITION | Structural/owner failure precedes status semantics | §10.4 | Malformed-header routing fixture | COMPLETE |
| 6 | STRUCTURAL PRECONDITION | Visibility observable domain is VISIBLE/INVISIBLE/ERROR | §10.4 | Harness result-domain assertion | COMPLETE |
| 7 | STRUCTURAL PRECONDITION | API representation remains implementation-free | §10.4 | Semantic-result assertion only | COMPLETE |
| 8 | SELF COMMAND CAUSALITY | SELF causality validation precedes creator short-circuit | §10.4 | `cmin=C,cmax=C-1` ordering oracle | COMPLETE |
| 9 | CREATOR / XMIN | FROZEN creator is visible without status lookup | §§10.2, 10.6 | Creator matrix plus lookup-call assertion | COMPLETE |
| 10 | CREATOR / XMIN | SELF creator with `cmin<C` is visible | §10.2 | SELF creator trichotomy | COMPLETE |
| 11 | CREATOR / XMIN | SELF creator with `cmin==C` is invisible | §10.2 | SELF creator trichotomy | COMPLETE |
| 12 | SELF COMMAND CAUSALITY | SELF creator with `cmin>C` is error | §§10.2, 10.4 | Future-cmin persisted/runtime fixtures | COMPLETE |
| 13 | CREATOR / XMIN | Other creator consumes canonical Chapter-9 status lookup | §§9.13, 10.2 | Controlled lookup source/assertion | COMPLETE |
| 14 | CREATOR / XMIN | COMMITTED creator below `xmax` and absent from active is visible | §10.2 | Creator normal matrix | COMPLETE |
| 15 | CREATOR / XMIN | COMMITTED creator captured active remains invisible after commit | §§9.7.2, 10.2 | Active-at-capture barrier | COMPLETE |
| 16 | CREATOR / XMIN | COMMITTED creator at `xmax` is invisible | §10.2 | Exact equality fixture | COMPLETE |
| 17 | CREATOR / XMIN | COMMITTED creator above `xmax` is invisible | §10.2 | Above-horizon fixture | COMPLETE |
| 18 | CREATOR / XMIN | Other IN_PROGRESS creator is invisible | §10.2 | Creator normal matrix | COMPLETE |
| 19 | CREATOR / XMIN | ABORTED creator is invisible while bytes may remain | §§10.1–10.2 | Creator matrix plus retained-bytes fixture | COMPLETE |
| 20 | CREATOR / XMIN | Normal creator invisibility suppresses deleter semantic lookup | §§10.2, 10.4 | Instrumented short-circuit fixture | COMPLETE |
| 21 | CREATOR STATUS ERROR | Dependent creator RETIRED is persisted corruption | §§10.2, 10.4 | Creator RETIRED fixture | COMPLETE |
| 22 | CREATOR STATUS ERROR | Dependent creator INVALID is persisted corruption | §§10.2, 10.4 | Creator INVALID fixture | COMPLETE |
| 23 | CREATOR STATUS ERROR | Creator RESERVED decodes but is semantic corruption | §§9.11.1, 10.2, 10.4 | Decoder-then-visibility fixture | COMPLETE |
| 24 | ERROR PROPAGATION | Creator status I/O failure propagates exactly | §10.4 | Creator lookup-failure matrix | COMPLETE |
| 25 | ERROR PROPAGATION | Creator status corruption/ownership failure propagates exactly | §10.4 | Creator lookup-failure matrix | COMPLETE |
| 26 | ERROR PROPAGATION | Creator unsupported format propagates distinctly | §§4.14, 10.4 | Future-format dispatch fixture | COMPLETE |
| 27 | ERROR PROPAGATION | Creator recovery/storage failure is not Boolean invisibility | §10.4 | Lower-layer injected-failure fixture | COMPLETE |
| 28 | DELETER / XMAX | `xmax=INVALID_TXN_ID` is valid no-delete sentinel | §10.3.1 | Deleter normal/sentinel matrix | COMPLETE |
| 29 | DELETER / XMAX | SELF deleter with `cmax<C` hides tuple | §10.3.2 | SELF deleter trichotomy | COMPLETE |
| 30 | DELETER / XMAX | SELF deleter with `cmax==C` leaves tuple visible | §10.3.2 | SELF deleter trichotomy | COMPLETE |
| 31 | SELF COMMAND CAUSALITY | SELF deleter with `cmax>C` is error | §§10.3.2, 10.4 | Future-cmax persisted/runtime fixtures | COMPLETE |
| 32 | DELETER / XMAX | COMMITTED deleter below `xmax` and absent from active is effective | §10.3.3 | Deleter normal matrix | COMPLETE |
| 33 | DELETER / XMAX | COMMITTED deleter captured active is ineffective | §10.3.3 | Active-at-capture deleter fixture | COMPLETE |
| 34 | DELETER / XMAX | COMMITTED deleter at/above `xmax` is ineffective | §10.3.3 | At/above-horizon fixtures | COMPLETE |
| 35 | DELETER / XMAX | Other IN_PROGRESS deleter is ineffective without visibility wait | §10.3.3 | Deleter normal matrix | COMPLETE |
| 36 | DELETER / XMAX | ABORTED deleter is ineffective while `xmax` may remain | §§10.1, 10.3.3 | Deleter matrix plus retained-header fixture | COMPLETE |
| 37 | DELETER STATUS ERROR | Dependent deleter RETIRED is persisted corruption | §§10.3.3–10.4 | Deleter RETIRED fixture | COMPLETE |
| 38 | DELETER STATUS ERROR | Dependent deleter INVALID is persisted corruption | §§10.3.3–10.4 | Deleter INVALID fixture | COMPLETE |
| 39 | DELETER STATUS ERROR | Deleter RESERVED decodes but is semantic corruption | §§9.11.1, 10.3.3–10.4 | Decoder-then-visibility fixture | COMPLETE |
| 40 | ERROR PROPAGATION | Deleter lookup I/O/corruption/unsupported/storage failure propagates | §10.4 | Deleter lookup-failure matrix | COMPLETE |
| 41 | DELETER STATUS ERROR | INVALID status is distinct from INVALID_TXN_ID sentinel | §§10.3.1, 10.3.3 | Explicit sentinel/status matrix | COMPLETE |
| 42 | SELF COMMAND CAUSALITY | Same-owner `cmax<cmin` is causally impossible | §§10.4, 10.6 | Same-owner causality matrix | COMPLETE |
| 43 | SELF COMMAND CAUSALITY | Persisted impossible command metadata yields `CORRUPT_HEAP` | §10.4 | Persisted future/causal fixtures | COMPLETE |
| 44 | SELF COMMAND CAUSALITY | Runtime-only impossible command metadata uses internal-invariant path | §§10.4, 39.1.3 | Abstract prepublication instrumentation | COMPLETE |
| 45 | SELF COMMAND CAUSALITY | Valid equal command identities remain permitted | §§10.2–10.4 | Current INSERT/DELETE/UPDATE fixtures | COMPLETE |
| 46 | CREATOR / XMIN | Creator result composes before deleter result | §10.4 | Cartesian normal composition | COMPLETE |
| 47 | UPDATE OLD/NEW PAIR | Current-command INSERT is invisible to ordinary rescan | §10.2 | Statement-effects fixture | COMPLETE |
| 48 | UPDATE OLD/NEW PAIR | Later-command INSERT is visible | §10.2 | Statement-effects fixture | COMPLETE |
| 49 | UPDATE OLD/NEW PAIR | Current-command DELETE leaves old tuple visible | §10.3.2 | Statement-effects fixture | COMPLETE |
| 50 | UPDATE OLD/NEW PAIR | Later-command DELETE hides old tuple | §10.3.2 | Statement-effects fixture | COMPLETE |
| 51 | UPDATE OLD/NEW PAIR | SELF current UPDATE selects old, not new | §§10.2–10.3 | UPDATE matrix | COMPLETE |
| 52 | UPDATE OLD/NEW PAIR | SELF later UPDATE selects new, not old | §§10.2–10.3 | UPDATE matrix | COMPLETE |
| 53 | UPDATE OLD/NEW PAIR | IN_PROGRESS updater selects old, not new | §§10.2–10.3 | UPDATE matrix | COMPLETE |
| 54 | UPDATE OLD/NEW PAIR | COMMITTED-before updater selects new, not old | §§10.2–10.3 | UPDATE matrix | COMPLETE |
| 55 | UPDATE OLD/NEW PAIR | COMMITTED at/after horizon selects old, not new | §§10.2–10.3 | UPDATE matrix | COMPLETE |
| 56 | UPDATE OLD/NEW PAIR | Active-at-capture updater remains old after later commit | §§9.7.2, 10.2–10.3 | Barrier plus UPDATE matrix | COMPLETE |
| 57 | UPDATE OLD/NEW PAIR | ABORTED updater selects old, not new | §§10.1–10.3 | UPDATE matrix | COMPLETE |
| 58 | UPDATE OLD/NEW PAIR | Every valid UPDATE row emits exactly one ordinary version | §§10.1–10.4 | Pair-output cardinality assertion | COMPLETE |
| 59 | READ COMMITTED | One statement attempt uses one stable snapshot | §§9.9, 10.4 | Chapter-9 RC fixture plus scan assertion | COMPLETE |
| 60 | READ COMMITTED | Commit after capture does not appear during the attempt | §§9.9, 10.2–10.3 | Mid-attempt terminal barrier | COMPLETE |
| 61 | READ COMMITTED | Next statement captures fresh state | §9.9 | Isolation Tests cross-reference | COMPLETE |
| 62 | READ COMMITTED | Own earlier/current command effects follow SELF rules | §§9.6, 10.2–10.3 | SELF and statement-effects matrices | COMPLETE |
| 63 | RETRY | Pre-write retry refreshes snapshot and retains CommandId | §§9.9, 15.7.1 | Existing retry fixture plus no-future-error assertion | COMPLETE |
| 64 | RETRY | Post-write retry cannot continue in same TxnId | §§15.7.2, 39.1 | Statement Failure tests | COMPLETE |
| 65 | REPEATABLE READ | First ordinary statement captures RR snapshot | §9.10 | Chapter-9 RR fixture | COMPLETE |
| 66 | REPEATABLE READ | Later statements retain external snapshot fields | §9.10 | RR multi-statement fixture | COMPLETE |
| 67 | REPEATABLE READ | External commits after capture remain invisible | §§9.10, 10.2–10.3 | Isolation Tests barrier | COMPLETE |
| 68 | REPEATABLE READ | Own later writes become visible through updated command boundary | §§9.10, 10.2–10.3 | RR plus SELF matrix | COMPLETE |
| 69 | REPEATABLE READ | V1 RR is snapshot isolation, not SERIALIZABLE | §§9.5, 10.2–10.4 | Isolation identity/write-skew tests | COMPLETE |
| 70 | SCAN / SNAPSHOT CONSISTENCY | SeqScan validates then evaluates every NORMAL candidate | §§5.17, 10.4 | Heap scan fixture | COMPLETE |
| 71 | ERROR PROPAGATION | Heap-scan visibility error terminates rather than skips | §10.4 | Heap error-propagation fixture | COMPLETE |
| 72 | INDEX HEAP RECHECK | B+ entry is only physical candidate RID | §8.22.2 | Scan Tests plus index matrix | COMPLETE |
| 73 | INDEX HEAP RECHECK | Visible candidate is emitted | §§8.22.2, 10.4 | Index matrix visible row | COMPLETE |
| 74 | INDEX HEAP RECHECK | Invisible candidate alone is silently skipped | §§8.22.2, 10.4 | Index matrix invisible row | COMPLETE |
| 75 | INDEX HEAP RECHECK | Candidate visibility error propagates and stops success | §§8.22.2, 10.4 | Index error rows | COMPLETE |
| 76 | CROSS-OWNER | Visibility does not follow `prev` after INVISIBLE or ERROR | §§5.7.4, 10.4, 14.10 | No-predecessor-fallback fixture | COMPLETE |
| 77 | SCAN / SNAPSHOT CONSISTENCY | All candidates/operators in one attempt share snapshot/command | §§9.9–9.10, 10.4 | Scan/Pipeline/Subquery cross-reference | COMPLETE |
| 78 | RECOVERY | Aborted INSERT bytes may remain but version is invisible | §§10.1, 13.15 | Retained-bytes recovery fixture | COMPLETE |
| 79 | RECOVERY | Aborted DELETE header may remain but delete is ineffective | §§10.1, 13.15 | Retained-header recovery fixture | COMPLETE |
| 80 | RECOVERY | Aborted UPDATE selects old and ignores new | §§10.1, 13.15 | Post-READY UPDATE matrix | COMPLETE |
| 81 | RECOVERY | Crash loser creator/deleter resolves through ABORTED authority | §§13.15–13.19 | Recovery Property Tests specialization | COMPLETE |
| 82 | RECOVERY | Durable COMMIT remains visible without heap/status-page force | §§7.11, 10.1, 13.13 | Durable-WAL/unflushed-pages fixture | COMPLETE |
| 83 | RECOVERY | Precrash runtime active/cache state is not trusted after reopen | §§9.13, 13.19 | Reopen lookup-source instrumentation | COMPLETE |
| 84 | STATUS / RECLAMATION BOUNDARY | Aborted-`xmax` normalization preserves visibility | §§10.5, 14.13.1 | Vacuum normalization cross-reference | COMPLETE |
| 85 | STATUS / RECLAMATION BOUNDARY | Creator freezing preserves visibility and removes lookup need | §§10.5, 14.13.2 | Freeze fixture | COMPLETE |
| 86 | STATUS / RECLAMATION BOUNDARY | Hint cleanup mutation follows WAL/page-LSN rules | §§10.5, 12.10–12.12 | Vacuum crash/fault cross-reference | COMPLETE |
| 87 | STATUS / RECLAMATION BOUNDARY | RETIRED requires durable Chapter-14 proof | §§10.4, 14.14.1–14.14.3 | Positive retirement procedure | COMPLETE |
| 88 | STATUS / RECLAMATION BOUNDARY | Dependent metadata surviving RETIRED is negative corruption case | §§10.4, 14.14.3 | RETIRED visibility fixture | COMPLETE |
| 89 | STATUS / RECLAMATION BOUNDARY | Missing status page alone never means RETIRED | §§9.13, 14.14.3 | Chapter-9 lookup plus retirement fixture | COMPLETE |
| 90 | STATUS / RECLAMATION BOUNDARY | Snapshot invisibility is not global reclaimability/DEAD | §§5.4.3, 10.5, 14.3–14.12 | Vacuum horizon and slot-state tests | COMPLETE |
| 91 | CROSS-OWNER | Ordinary visibility acquires no tuple-write lock | §§10.4, 10.6 | Lock instrumentation on read fixtures | COMPLETE |
| 92 | CROSS-OWNER | Chapter 11 remains write-conflict/UNIQUE owner | §§10.3.3, 11.10.4 | Locking/UNIQUE cross-reference | COMPLETE |
| 93 | CROSS-OWNER | HeapPage supplies metadata but does not decide visibility | §§5.17, 10.4, 10.6 | Direct tuple versus visibility component fixture | COMPLETE |
| 94 | CROSS-OWNER | Chapter 9 status/snapshot is the sole visibility input authority | §§9.7–9.13, 10.4 | Controlled input and lookup-call assertions | COMPLETE |
| 95 | CROSS-OWNER | Read-only transactions cannot create persistent status-dependent tuple metadata | §§9.15, 10.4 | Read-only Chapter-9 specialization cross-reference | COMPLETE |
| 96 | FAILURE TAXONOMY | §39.1 owns continuation and no new visibility error enum is invented | §§10.4, 39.1 | Error matrix plus statement-failure cross-reference | COMPLETE |

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
highest normal TxnId per cyclic strongly connected component
lock release on commit
lock release on abort
cancelled waiter cleanup
```

Use deterministic barriers, scheduler gates, or semantic event hooks rather than timing
sleeps. Random stress may supplement these fixtures but cannot establish their required
ordering.

#### Deterministic logical-lock harness and independent oracles

The harness records and can pause the following implementation-independent events:

```text
request created / exact LockKey resolved
queue and current owners observed
holder installed / waiter enqueued
blocking owners and queue predecessors captured
wait-for node/edges installed / synchronous deadlock scan completed
requester begins waiting / requester becomes ineligible / request canceled
holder terminal state published / resource released
waiter selected / requester eligibility rechecked / grant installed / wake delivered
target, key range, descriptor, and transaction status revalidated
queue entry and wait-for edges removed
```

The fixture controls every event with barriers or explicit scheduler gates and forces both
legal orders at cancellation, terminal publication, release, and grant boundaries. A test
must fail if its required event is observable only by an elapsed-time timeout. Timeout
events may be observed diagnostically, but they are never the deadlock or queue-order
oracle.

Test-side models independently compute:

- exact `LockKey` equality, including complete tuple and encoded unique-key identities;
- exclusive compatibility and the §11.13.3 retained-gate matrix;
- FIFO order for each §11.12 LockManager queue and any represented gate predecessor;
- same-owner reentrant, idempotent, subsumed, and rejected-transition cases;
- requester eligibility and the cancellation linearization;
- all blocking owner/predecessor edges and cyclic strongly connected components;
- the highest normal TxnId victim in each cyclic component;
- legal transaction state, terminal release point, and required post-grant revalidation;
- tuple current-owner outcomes and UNIQUE current-state candidate outcomes.

Production grant, conflict, SCC, and uniqueness decisions are observations, not their own
oracles. Every fixture is valid except for its named mutation: physical page/RID/key and
snapshot inputs remain canonical in a status test; queue and transaction states remain
canonical in an ordering test; and queue/graph state is coherent immediately before an
allocation fault.

#### TUPLE_WRITE current-owner and failure verification

Drive the §11.4 current-owner decision after exclusive
`TUPLE_WRITE(TableId,physical RID)` grant and a fresh physical re-fetch. This is a direct
write-admission test, not an inference from Chapter-10 visibility. UPDATE and DELETE run
each applicable row, and no row may mutate the pre-wait target until identity, header,
visibility, `xmax/cmax`, and required status have been revalidated.

| Owner relation | Status/command fixture | Isolation/boundary | Wait? | Retry? | MUST_ABORT? | Expected error/result | May mutate original target? | Required revalidation |
|---|---|---|---:|---:|---:|---|---:|---|
| No competing owner | `xmax == INVALID_TXN_ID` sentinel; no status lookup | RC and RR | no | no | no | proceed if other checks pass | yes | full post-grant target check |
| SELF, earlier command | `cmax < current CommandId` | RC and RR | no | target reacquisition as owned by DML | no | stale/not-current target | no | current row/command semantics |
| SELF, same command | `cmax == current CommandId` | RC and RR | no | no independent writer retry | no | same-command result | only if same-operation rules authorize | exact operation/target context |
| SELF, future command | `cmax > current CommandId` | persisted or runtime-only | no | no | no | persisted `CORRUPT_HEAP`; otherwise internal-invariant result | no | fail before conflict classification |
| SELF, impossible order | `xmin == xmax == SELF`, `cmax < cmin` | persisted or runtime-only | no | no | no | corruption/internal-invariant result | no | fail before conflict classification |
| Other owner | `IN_PROGRESS` | RC and RR | yes | after wake, by owning isolation rule | only if resulting rule requires | wait, then classify fresh state | no | release physical protection; full re-fetch |
| Other owner | COMMITTED | RC, before first statement write | no after wake | fresh RC snapshot, same CommandId | no if retry remains clean | canonical pre-write retry/failed-active path | no | rediscover target and predicates |
| Other owner | COMMITTED | RC, after first statement write | no after wake | no | yes | failed transaction / original write conflict | no | preserve causal result through abort |
| Other owner | COMMITTED after retained snapshot | RR | no after wake | no | yes | serialization/write-conflict result | no | fixed snapshot remains unchanged |
| Other owner | ABORTED | RC and RR | no | no | no | aborted `xmax` is ineffective | yes, after validation | overwrite `xmax/cmax` only through write path |
| Other owner | RETIRED while outcome is required | RC and RR | no | no | no | corruption/invariant result | no | prove retirement contract violation |
| Other normal TxnId | persisted `INVALID` status | RC and RR | no | no | no | canonical error | no | distinguish status from no-owner sentinel |
| Other normal TxnId | recognized `RESERVED` status | RC and RR | no | no | no | canonical error | no | decoder succeeds; semantic use fails |
| Other owner lookup | injected I/O failure | RC and RR | no | no | per exact §39 row | exact lower-layer I/O result | no | no status guess |
| Other owner lookup | checksum, owner, or supported-v1 framing corruption | RC and RR | no | no | per exact §39 row | exact corruption/lower-layer result | no | no conflict conversion |
| Other owner lookup | recognizable unsupported format | RC and RR | no | no | per exact §39 row | exact unsupported-format result | no | no corruption/no-conflict guess |

The persisted `INVALID` row deliberately uses a normal competing TxnId. It is distinct from
the valid `INVALID_TXN_ID` tuple sentinel, which performs no status lookup. RETIRED is
constructed only through a valid Chapter-14 retirement fixture whose proof is then made
inconsistent with the still-live tuple reference. The future-SELF and `cmax < cmin` rows
cross-check Chapter 10, but the assertion here is that write-conflict code cannot convert
those failures into an ordinary conflict or admission.

For lookup failures, inject one exact status-layer outcome at a time through the canonical
transaction-status fixture: raw I/O, checksum/framing/owner corruption, and recognizable
future format. Assert exact error propagation, zero target mutation, no wait, and no
fallback to ABORTED or no owner. This prevents unavailable or malformed ownership metadata
from becoming write permission.

#### Lock-table, waiter, and wait-for-graph resource exhaustion

Provide fault points immediately before allocation or reservation of a new lock entry,
waiter record, graph node when separately allocated, and graph edge. Capture queue and graph
snapshots before and after the fault. A blocking request may begin sleep only after every
required dependency is represented; partial request, queue, or graph state must be removed
atomically on failure.

| Resource | Fault point | First statement write? | Permitted partial queue/graph state | Statement / transaction result | Cleanup and ownership oracle |
|---|---|---:|---|---|---|
| Lock entry | before exact-key entry becomes reachable | no | none | `OutOfMemory`/owning allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | no entry or holder; prior locks unchanged |
| Lock entry | same | yes | none | same causal error; `FAILED_TRANSACTION_MUST_ABORT` | abort retains prior ownership through A2/A3 |
| Waiter | before queue publication | no | none | allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | no waiter, edge, or later grant; holder unchanged |
| Waiter | same | yes | none | allocation error; `FAILED_TRANSACTION_MUST_ABORT` | no phantom waiter; prior ownership terminally released |
| Graph node | transaction registration, or lazy materialization before request publication | no | no sleeping request | owning allocation error under §39.1 | transaction/request registration is absent or coherently canceled |
| Graph node | same reachable post-write path, if representation allocates lazily | yes | no sleeping request | owning allocation error; `FAILED_TRANSACTION_MUST_ABORT` | abort from coherent pre-fault graph state |
| Graph edge | before one required blocker edge becomes visible | no | entire failed edge set rolled back | allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | request canceled; no sleep on hidden dependency |
| Graph edge | same | yes | entire failed edge set rolled back | allocation error; `FAILED_TRANSACTION_MUST_ABORT` | request canceled; abort retains owned resources to A3 |

If transaction registration preallocates graph nodes, the graph-node rows inject at that
registration owner and prove that no transaction capable of requesting a gate is published
without its node capacity. They do not require a second lazy allocation path. If graph
coherence cannot be established or restored after any fault, assert the existing internal-
invariant/`DATABASE_NONCONTINUABLE` path instead of ordinary allocation failure. This is
separate from an incompatible lock wait and from deadlock detection.

#### FIFO, same-owner, cancellation, and terminal lifetime

For one §11.12 exclusive LockManager key, install holder H and enqueue W1, W2, and W3 in
barrier-controlled order. H's terminal release makes W1 the first eligible owner; W1's and
W2's terminal releases then admit W2 and W3. Repeat for `TUPLE_WRITE` and `UNIQUE_KEY`.
Compatible gate requests need not be delayed merely to mimic this exclusive queue; if an
implementation elects a queue predecessor that prevents a gate grant, verify that the
predecessor is represented in the graph.

| Scenario | Queue before event | Controlled event | Expected next owner/result | Graph effect | Late grant? | Revalidation? |
|---|---|---|---|---|---:|---:|
| Ordinary FIFO | H; W1, W2, W3 | H releases after terminal publication | W1, then W2, then W3 after their releases | current blocker/predecessor edges rebuilt | no bypass | yes |
| Canceled head | H; W1, W2 | W1 cancellation linearizes before H release | W2 after H release | all W1 edges removed | W1: no | W2: yes |
| Deadlock-victim head | H; W1, W2 | W1 receives `DEADLOCK_DETECTED` and enters `MUST_ABORT` | W2 only after actual holder state permits | W1 outgoing wait removed; owned-resource edges persist until release | W1: no | W2: yes |
| Same-owner tuple reacquire | T owns TUPLE_WRITE K | T requests identical K | immediate idempotent/reentrant success | no self-edge | not applicable | no new wait conclusion |
| Same-owner unique reacquire | T owns UNIQUE_KEY K | T requests identical K | immediate idempotent/reentrant success | no self-edge | not applicable | no new wait conclusion |
| MUST_ABORT waiter | H; W | W enters MUST_ABORT before H release | request canceled; ordinary statement cannot resume | waiter edges removed | no | not applicable |
| ABORTING/ABORTED waiter | H; W | state transition precedes grant | request canceled/ineligible | waiter edges removed | no | not applicable |
| COMMITTING/COMMITTED waiter | H; W | state transition precedes ordinary grant | request canceled/ineligible | waiter edges removed | no | not applicable |

Same-owner reacquisition must not create an independently releasable acquisition or shorten
the original terminal lifetime; tests assert ownership remains until C5 or A3 without
requiring a reference-count implementation. Exercise every same-owner `ALLOW`/subsumption
case in §11.13.3 and the two proactive rejections in §11.13.2, including same-table
`SHARED_WRITER -> EXCLUSIVE_DDL`; a rejected transition creates no wait or self-edge.

At the cancel-versus-release boundary, force both orders: cancellation completes before
release, and release selects the request immediately before cancellation tries to
linearize. Exactly one legal result may win. Once transaction ineligibility or cancellation
has linearized, no later grant or wake may resume an ordinary statement. Observe requester
eligibility immediately before installing a grant, not only when enqueuing it.

#### Deadlock SCC, terminal release, and wake-up revalidation

In addition to the cross-family schedules below, construct one SCC containing overlapping
simple cycles and a second graph containing two disjoint cyclic SCCs. The independent graph
oracle computes SCCs from the complete blocker/predecessor edge set. Exactly the highest
normal TxnId in each cyclic SCC is selected; overlapping simple cycles within one SCC do
not authorize separate cycle-local choices.

Pause a selected victim after `MUST_ABORT` and before A2. Survivors must remain blocked on
victim-owned resources through A2 ABORTED publication and may be selected/granted only by
A3 release. The victim's outgoing wait is canceled immediately, but its held-resource
ownership remains represented until release. An ordinary deadlock yields
`DEADLOCK_DETECTED`, not timeout, resource exhaustion, or database noncontinuability.

For a woken `TUPLE_WRITE` waiter, mutate the physical target state while it sleeps, then
assert fresh RID/header/visibility/current-owner validation before any write. For a woken
`UNIQUE_KEY` waiter, invalidate every cached range position and candidate result and require
a complete `(K,MIN_RID)` rescan under fresh RID protection. Gate waiters perform the exact
descriptor/manifest/object revalidation in §11.13.4. A grant is never proof that pre-wait
physical conclusions remain valid.

#### Runtime lock-state recovery and admission

Extend Recovery Property Tests with a deterministic pre-crash state containing a
`TUPLE_WRITE` holder, a `UNIQUE_KEY` holder, waiters, wait-for edges, and a cross-resource
dependency. Crash at the controlled point, reopen, and observe LockManager/registered-gate
state before ordinary admission.

| Pre-crash runtime state | Durable terminal state | Runtime lock state after reopen | SQL lock replay? | UNIQUE predicate replay? | READY/admission | Expected transaction outcome |
|---|---|---|---:|---:|---|---|
| Active holder, no terminal record | none | no holder, waiter, queue, or edge | no | no | only after loser resolution and READY | canonical recovered ABORTED loser |
| Durable committed holder | COMMIT record durable | empty | no | no | after committed state/data reconstruction and READY | COMMITTED |
| Durable aborted holder | ABORT record durable | empty | no | no | after aborted state reconstruction and READY | ABORTED |
| Holder plus waiter | holder terminal or nonterminal as fixture selects | empty; waiter does not survive | no | no | no pre-READY transaction request | outcomes from WAL/status, never queue |
| Cyclic graph | any valid WAL prefix | no graph nodes/edges reconstructed from old waits | no | no | after recovery reaches READY | per durable terminal/loser analysis |
| Unique-key holder plus waiter | holder terminal or nonterminal as fixture selects | no key holder/waiter | no | no | fresh post-READY requests only | current state checked by new SQL work |

Instrument redo dispatch and assert it never requests `TUPLE_WRITE`, `UNIQUE_KEY`,
`SchemaLock`, `TableWriterGate`, `STATS_PUBLISH`, `MANIFEST_CHANGE`, an SQL wait, or the
§11.10 SQL UNIQUE predicate merely because historical DML/DDL held or checked them.
Recovery may use its separately owned physical page/structure coordination. No ordinary
transaction or logical-lock request is admitted until READY; after READY, fresh
transactions build fresh process-local coordination state. This proves that lock queues
and graph edges are runtime coordination rather than durable transaction truth.

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

#### Current-state status, failure, and race fixtures

The normal-regression table includes no candidate, a committed/frozen live creator, a
committed creator invisible to the checking transaction's old RR snapshot, an active
creator, an aborted creator, an aborted deleter, a committed effective deleter, and an
active deleter. It also includes exact `SELF_EXCLUDED` old/replacement RIDs, another live
row from the same transaction, current-command and earlier-command self deletes,
NULL-containing ordinary UNIQUE keys, and PRIMARY KEY NULL rejection. The independent
oracle enumerates the complete exact-key range and applies §11.10.4 aggregate precedence;
ordinary MVCC visibility is recorded only to prove it is not the admission oracle.

Run each nonterminal creator/deleter case to both terminal outcomes. The checker first
observes `WAIT_THEN_RECHECK`, publishes nothing, and waits without physical latches. After
COMMITTED or ABORTED publication and release, it restarts the whole range check. In
particular, a committed live creator is `UNIQUE_CONFLICT` even when invisible to the
caller's retained RR snapshot, while an aborted creator is ignorable only after canonical
status recheck.

The following negative matrix uses a structurally valid index candidate, matching protected
heap RID/relation/key, valid snapshot/operation context, and exactly one altered status or
identity dimension:

| Candidate status/result | Decoder valid? | Logical ownership resolvable? | Predicate/result | May proceed? | Exact error family/oracle |
|---|---:|---:|---|---:|---|
| RETIRED while candidate still requires outcome | yes | no | error, never candidate success | no | corruption/internal-invariant result; retirement proof contradicted |
| Persisted `INVALID` for allocated normal owner | yes | no | error | no | canonical invalid-status/current-owner error |
| Persisted `RESERVED` | yes | no | error | no | recognized encoding, semantically rejected state |
| Status I/O failure | not reached/available | no | propagated failure | no | exact lower-layer I/O result |
| Supported-v1 checksum/framing/owner corruption | no trusted status | no | propagated failure | no | exact corruption/lower-layer result |
| Recognizable unsupported status format | format dispatch rejects | no | propagated failure | no | exact unsupported-format result |
| Dangling, reused, wrong-relation, non-NORMAL, or key-mismatch RID | status is irrelevant after identity failure | no | `CORRUPTION_OR_INTERNAL_ERROR` conceptual branch | no | exact owning heap/index corruption result |

For `RESERVED`, independently observe successful two-bit decoding before semantic rejection.
For unsupported format, observe version dispatch before any status interpretation. For I/O
and corruption, assert the exact lower-layer diagnostic survives §39 classification. None
of these rows may become `NO_CONFLICT`, `WAIT_THEN_RECHECK`, or `UniqueViolation`; a
constraint violation requires a valid current logical owner.

Use two transactions for the same fully non-NULL key to cover INSERT/INSERT and
UPDATE/INSERT in both lock-acquisition orders. If the first holder commits, the waiter
rescans and reports `UniqueViolation`; if it aborts, the waiter may proceed only after a
complete no-other-owner rescan. For DELETE/INSERT, the inserter waits under the key lock: a
committed delete may free the key after recheck, while an aborted delete leaves the original
owner conflicting. The holder's key lock remains present through C4 or A2 and is released
only at C5 or A3; the waiter cannot begin its deciding rescan earlier.

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

### Chapter 11 Procedural Matrices and Coverage

The matrices below consolidate procedure ownership; they do not replace the architecture's
normative decisions.

#### Error and result matrix

| Observed condition | Expected result/state | Retry? | Ownership/release | Procedure owner |
|---|---|---:|---|---|
| Incompatible owner or represented predecessor | WAIT; transaction remains eligible | after grant only | existing ownership retained | Locking deterministic harness |
| Clean RC stale-target conflict before first statement write | architecture-permitted same-CommandId fresh-snapshot retry or failed-active result | yes, only at §15.7 boundary | retained transaction locks remain terminal-duration | TUPLE current-owner matrix / Statement Failure tests |
| RC stale-target conflict after first statement write | `FAILED_TRANSACTION_MUST_ABORT` | no | release through A2/A3 | TUPLE current-owner matrix / §39 tests |
| RR post-snapshot committed writer | transaction-fatal serialization/write conflict | no | release through A2/A3 | TUPLE current-owner matrix / Isolation Tests |
| Deadlock victim | `DEADLOCK_DETECTED`, then `MUST_ABORT` | no ordinary same-TxnId statement retry | held resources remain until A2/A3 | Deadlock SCC procedure |
| Valid current logical UNIQUE owner | `UniqueViolation`; §39 determines failed-active versus must-abort | no internal post-write retry | key lock remains terminal-duration | UNIQUE current-state procedures |
| Impossible persisted owner/RID/command state | exact corruption result | no | §39/lower-layer terminal handling | TUPLE/UNIQUE negative matrices |
| Impossible runtime-only invariant | internal-invariant result | no | ordinarily `DATABASE_NONCONTINUABLE` | TUPLE/UNIQUE negative matrices |
| Status/index/heap I/O failure | exact lower-layer result | only if owning layer explicitly permits | never guessed free/nonconflicting | Status-failure fixtures |
| Recognizable future format | exact unsupported-format result | no semantic retry invented here | no write/constraint admission | Status-failure fixtures |
| Coherent runtime allocation failure | `OutOfMemory` or exact owning resource error; FA before / MA after first write | per §39 only | partial request/graph removed; prior ownership preserved | Resource-exhaustion matrix |
| Uncertain queue/graph/ownership coherence | internal-invariant / `DATABASE_NONCONTINUABLE` | no | controlled teardown/recovery; no guessed release | Resource-exhaustion matrix |
| Waiter becomes transaction-ineligible | lock cancellation with original stronger cause preserved | no ordinary continuation | outgoing wait state removed; held resources terminally released | Queue/lifetime matrix |

#### High-level domain and case matrix

| Domain/case | Deterministic fixture | Controlled event/fault | Independent oracle | Architecture | Verification owner | Status |
|---|---|---|---|---|---|---|
| TUPLE_WRITE normal owner | Canonical target/current-owner rows | grant and re-fetch | owner/status/isolation matrix | §§11.2–11.7 | TUPLE current-owner procedure | COMPLETE |
| TUPLE_WRITE invalid owner/status | RETIRED/INVALID/RESERVED/future SELF | one status/command mutation | no-guessing matrix | §§11.4, 11.15 | TUPLE negative rows | COMPLETE |
| TUPLE_WRITE status failure | I/O/corruption/unsupported format | status lookup | exact injected result | §§11.4, 39.1 | TUPLE lookup rows | COMPLETE |
| RC conflict | Competing committed updater | pre/post first-write boundary | fixed §15.7/§39 state model | §§11.5, 15.7 | TUPLE/Statement Failure tests | COMPLETE |
| RR conflict | Retained snapshot plus later commit | holder terminal publication | fixed snapshot oracle | §11.6 | TUPLE/Isolation tests | COMPLETE |
| UNIQUE normal owner | Complete exact-key candidate range | candidate terminal states | independent current-state classifier | §§11.8–11.10 | UNIQUE normal regression | COMPLETE |
| UNIQUE invalid owner/status | RETIRED/INVALID/RESERVED | one status mutation | negative matrix | §11.10.4 | UNIQUE status fixtures | COMPLETE |
| UNIQUE lookup failure | I/O/corruption/unsupported format | candidate status lookup | exact injected result | §§11.10.4, 39.1 | UNIQUE status fixtures | COMPLETE |
| Lock-entry exhaustion | New exact key | pre-publication allocation | pre/post state snapshots | §§11.12, 39.1 | Resource-exhaustion matrix | COMPLETE |
| Waiter exhaustion | Contended exact key | enqueue allocation | queue/graph model | §§11.12–11.13, 39.1 | Resource-exhaustion matrix | COMPLETE |
| Graph-edge exhaustion | Blocking request | dependency installation | complete-edge model | §11.13.4 | Resource-exhaustion matrix | COMPLETE |
| FIFO | H/W1/W2/W3 | terminal releases | independent ordered queue | §11.12 | Queue/lifetime matrix | COMPLETE |
| Same-owner reacquire | Tuple/key current owner | identical request | owner identity model | §§11.12–11.13 | Queue/lifetime matrix | COMPLETE |
| Cancellation/no-late-grant | Queued eligible waiter | state/cancel versus release | eligibility linearization | §§11.12–11.13.5 | Queue/lifetime matrix | COMPLETE |
| Deadlock SCC | Overlapping and disjoint cycles | final edge addition | independent SCC/victim model | §11.13.4 | Deadlock SCC procedure | COMPLETE |
| Terminal release | Waiting competitor | C4/C5 and A2/A3 gates | event-order model | §§11.11, 15.5–15.6 | COMMIT/ABORT and locking tests | COMPLETE |
| Wake-up revalidation | Target/key/descriptor changes while waiting | release/grant | fresh-state oracle | §§11.3, 11.9, 11.13.4 | Wake-up procedures | COMPLETE |
| Recovery reset | Holders/waiters/edges at crash | reopen/recovery/READY | durable WAL/status model | §§11.12–11.13, 13.11–13.19 | Runtime lock-state recovery | COMPLETE |
| No SQL lock replay | DML WAL requiring redo | redo dispatch | event log excludes SQL locks | §§11.10.8, 13.13 | Runtime lock-state recovery | COMPLETE |
| No UNIQUE replay | Authorized historical unique DML | redo dispatch | event log excludes SQL predicate | §§11.10.8, 13.13 | Runtime lock-state recovery | COMPLETE |

#### Atomic architecture-obligation coverage map

`COMPLETE` means a deterministic procedure above or a precise existing owner supplies the
fixture, controlled event/fault, independent oracle, and expected observation. Architecture
text alone is not counted as coverage.

| ID | Primary domain | Atomic obligation | Architecture owner | Verification procedure/reference | Status |
|---:|---|---|---|---|---|
| 1 | LOCK IDENTITY | `TUPLE_WRITE` equality uses `(TableId,physical RID)` | §11.2 | Harness key oracle plus tuple-blocking cases | COMPLETE |
| 2 | LOCK IDENTITY | Different table/RID tuples do not alias | §11.2 | Exact-key positive/negative vectors | COMPLETE |
| 3 | LOCK IDENTITY | `UNIQUE_KEY` equality uses `(IndexId,complete encoded key)` | §11.8 | Harness key oracle plus unique races | COMPLETE |
| 4 | LOCK IDENTITY | Hash collision cannot replace complete unique-key equality | §11.8 | Forced-collision key vectors | COMPLETE |
| 5 | LOCK IDENTITY | Composite/scalar semantic equality matches encoded lock equality | §11.8 | UNIQUE key-codec matrix | COMPLETE |
| 6 | LOCK IDENTITY | NULL-containing ordinary UNIQUE key has no duplicate lock identity | §§11.8, 11.10.2 | UNIQUE normal regression | COMPLETE |
| 7 | LOCK MODE | Ordinary readers take no shared tuple lock | §11.1 | Isolation plus lock event log | COMPLETE |
| 8 | COMPATIBILITY | TUPLE_WRITE and UNIQUE_KEY are exclusive | §§11.1, 11.12 | Same/different-key blocking cases | COMPLETE |
| 9 | COMPATIBILITY | Different logical tuple/unique keys proceed independently | §§11.2, 11.8, 11.12 | Parallel exact-key fixtures | COMPLETE |
| 10 | CROSS-OWNER | Registered schema/writer/publication domains use their exact scopes/modes | §11.13.1 | Complete §11.13.3 matrix execution | COMPLETE |
| 11 | DEADLOCK GRAPH | Separate resource containers still share one graph | §§11.1, 11.13.1 | Cross-family cycle fixtures | COMPLETE |
| 12 | RECOVERY RESET | Lock table/queues are process-local runtime state | §§11.12, 11.13 | Runtime lock-state recovery | COMPLETE |
| 13 | ACQUISITION | Unowned exact logical key grants immediately | §11.12 | Harness grant event | COMPLETE |
| 14 | ACQUISITION | Another owner of an exclusive key causes represented wait | §§11.12, 11.13.4 | Holder/waiter fixture | COMPLETE |
| 15 | REENTRANCY | Same-owner exact key does not become a competing owner | §11.12 | Same-owner tuple/unique rows | COMPLETE |
| 16 | QUEUE ORDER | Exclusive LockManager waiters follow FIFO | §11.12 | H/W1/W2/W3 fixture | COMPLETE |
| 17 | ACQUISITION | UPDATE/DELETE acquires tuple key before owner mutation | §§11.4, 11.9 | DML revalidation integration | COMPLETE |
| 18 | ACQUISITION | Unique locks are requested only after key derivation and latch release | §11.9 | Unique wait event sequence | COMPLETE |
| 19 | LOCK ORDER | UPDATE old/new unique keys use total `(IndexId,key)` order when known | §11.9 | Opposite-expression-order fixture | COMPLETE |
| 20 | LOCK ORDER | Multiple known unique constraints use the same total order | §11.9 | Multi-index key-set fixture | COMPLETE |
| 21 | DEADLOCK GRAPH | Incrementally discovered multirow keys remain protected and graph-visible | §11.9 | Streaming multirow cross-key cycle | COMPLETE |
| 22 | LOCK ORDER | DML/DDL/ANALYZE gate order follows operation table | §§11.13.2, 11.13.6 | Every operation-order row | COMPLETE |
| 23 | COMPATIBILITY | Every §11.13.3 ALLOW/WAIT/REJECT cell is observed | §11.13.3 | Existing complete gate matrix procedure | COMPLETE |
| 24 | CANCELLATION | Ineligible transaction states cannot acquire a new ordinary resource | §§11.13.3, 11.13.6 | Queue/lifetime state rows | COMPLETE |
| 25 | LOCK/LATCH ORDER | TUPLE_WRITE wait retains no heap/B+/frame latch | §§11.3, 11.14 | Latch ownership event assertions | COMPLETE |
| 26 | LOCK/LATCH ORDER | UNIQUE_KEY wait retains no physical latch/read epoch | §§11.3, 11.9 | Unique wait/recheck fixture | COMPLETE |
| 27 | LOCK/LATCH ORDER | Registered gate wait retains no prohibited physical ownership | §§11.13.2, 11.13.7 | Cross-family schedules | COMPLETE |
| 28 | LOCK/LATCH ORDER | Page latches never become transaction locks or graph nodes | §11.14 | Instrumented owner-domain assertion | COMPLETE |
| 29 | WAKE-UP REVALIDATION | TUPLE_WRITE grant causes fresh target/owner validation | §§11.2–11.4 | TUPLE wake-up fixture | COMPLETE |
| 30 | WAKE-UP REVALIDATION | UNIQUE_KEY grant causes complete fresh exact-key rescan | §§11.9–11.10 | UNIQUE wake-up fixture | COMPLETE |
| 31 | WAKE-UP REVALIDATION | Gate grant causes resource-specific descriptor/manifest revalidation | §11.13.4 | Existing adversarial timelines | COMPLETE |
| 32 | REVALIDATION | A pre-wait RID cannot authorize post-wait mutation | §§11.3, 11.10.8 | Mutated-target fixture plus read-epoch checks | COMPLETE |
| 33 | TUPLE CURRENT OWNER | No-xmax sentinel permits write after ordinary checks | §11.4.1 | TUPLE matrix row | COMPLETE |
| 34 | TUPLE CURRENT OWNER | SELF earlier-command target is stale/not blindly mutable | §11.4.2 with Ch. 10 | TUPLE matrix row | COMPLETE |
| 35 | TUPLE CURRENT OWNER | SELF current-command uses same-command semantics | §11.4.2 with Ch. 10 | TUPLE matrix row | COMPLETE |
| 36 | TUPLE CURRENT OWNER | Future SELF command is corruption/invariant failure | §§11.4.2, 11.10.4 | TUPLE negative fixture | COMPLETE |
| 37 | TUPLE CURRENT OWNER | Same-owner `cmax < cmin` is not an ordinary conflict | §11.10.4 with §10.4 | TUPLE negative fixture | COMPLETE |
| 38 | TUPLE CURRENT OWNER | Other IN_PROGRESS owner is waited upon | §11.4.4 | TUPLE matrix row | COMPLETE |
| 39 | LOCK/LATCH ORDER | IN_PROGRESS wait releases physical protection first | §§11.3, 11.4.4 | Event-order assertion | COMPLETE |
| 40 | RC WRITE CONFLICT | Committed owner before first write permits canonical RC retry | §11.5 | TUPLE matrix / Statement Failure tests | COMPLETE |
| 41 | RC WRITE CONFLICT | Committed owner after first write requires MUST_ABORT | §11.5 | TUPLE matrix / §39 boundary | COMPLETE |
| 42 | RR WRITE CONFLICT | Post-snapshot committed owner is transaction-fatal | §11.6 | TUPLE matrix / Isolation tests | COMPLETE |
| 43 | TUPLE CURRENT OWNER | ABORTED xmax is ineffective after revalidation | §11.4.3 | TUPLE matrix row | COMPLETE |
| 44 | FAILURE PROPAGATION | Required RETIRED owner outcome is corruption/invariant | §§11.10.4, 14.14.3 | TUPLE RETIRED fixture | COMPLETE |
| 45 | FAILURE PROPAGATION | Persisted INVALID owner status is an error | §11.10.4 with §9.13 | TUPLE INVALID fixture | COMPLETE |
| 46 | TERMINOLOGY | Persisted INVALID status differs from `INVALID_TXN_ID` sentinel | §§11.4.1, 11.10.4 | Explicit paired TUPLE rows | COMPLETE |
| 47 | FAILURE PROPAGATION | Recognized RESERVED owner status is semantically rejected | §11.10.4 with §9.12 | TUPLE RESERVED fixture | COMPLETE |
| 48 | FAILURE PROPAGATION | Current-owner I/O failure propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture | COMPLETE |
| 49 | FAILURE PROPAGATION | Current-owner corruption propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture | COMPLETE |
| 50 | FAILURE PROPAGATION | Unsupported status format propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture | COMPLETE |
| 51 | WRITE CONFLICT | UPDATE follows lock/re-fetch/current-owner protocol | §11.4 | DML plus TUPLE matrix | COMPLETE |
| 52 | WRITE CONFLICT | DELETE follows the same conflict protocol | §11.4 | DML plus TUPLE matrix | COMPLETE |
| 53 | CONCURRENCY | Two writers cannot silently overwrite one target version | §11.7 | Deterministic competing-writer fixture | COMPLETE |
| 54 | CROSS-OWNER | Write admission is distinct from snapshot visibility | §§11.4–11.7 | TUPLE matrix plus MVCC Visibility tests | COMPLETE |
| 55 | RETRY / MUST_ABORT | Pre-write internal RC retry retains one CommandId | §§11.5, 15.7 | CommandId/snapshot retry tests | COMPLETE |
| 56 | RETRY / MUST_ABORT | RC retry captures a fresh statement snapshot | §§11.5, 15.7 | Isolation/Statement Failure tests | COMPLETE |
| 57 | RETRY / MUST_ABORT | Published-write RC attempt cannot transparently retry | §§11.5, 39.1 | Pre/post boundary fixture | COMPLETE |
| 58 | RR WRITE CONFLICT | RR never refreshes its retained snapshot after conflict | §11.6 | Isolation fixture | COMPLETE |
| 59 | FAILURE PROPAGATION | Metadata/status errors are not ordinary write conflicts | §§11.4, 11.10.4 | TUPLE negative assertions | COMPLETE |
| 60 | RETRY / MUST_ABORT | Statement/transaction result follows exact §39 class | §§11.5–11.6, 39.1 | Error/result matrix | COMPLETE |
| 61 | UNIQUE CURRENT STATE | Empty exact-key range is NO_CONFLICT | §11.10.4 | UNIQUE normal regression | COMPLETE |
| 62 | UNIQUE CURRENT STATE | Committed/frozen live creator conflicts | §§11.10.4–11.10.6 | UNIQUE normal regression | COMPLETE |
| 63 | UNIQUE CURRENT STATE | Snapshot-invisible committed live owner still conflicts | §§11.10.1, 11.10.4 | Old-RR-snapshot fixture | COMPLETE |
| 64 | UNIQUE CURRENT STATE | Active creator commit becomes conflict after rescan | §11.10.7 | Active-owner terminal fixture | COMPLETE |
| 65 | UNIQUE CURRENT STATE | Active creator abort may free key after rescan | §11.10.7 | Active-owner terminal fixture | COMPLETE |
| 66 | UNIQUE CURRENT STATE | Aborted creator is ignorable only after status check | §11.10.4 | UNIQUE normal regression | COMPLETE |
| 67 | UNIQUE CURRENT STATE | Active deleter commit may free key after rescan | §§11.10.4, 11.10.7 | Delete/insert race | COMPLETE |
| 68 | UNIQUE CURRENT STATE | Active deleter abort preserves conflict after rescan | §§11.10.4, 11.10.7 | Delete/insert race | COMPLETE |
| 69 | UNIQUE CURRENT STATE | Aborted deleter is ineffective | §11.10.4 | UNIQUE normal regression | COMPLETE |
| 70 | UNIQUE CURRENT STATE | Committed effective deleter removes old ownership | §11.10.4 | UNIQUE normal regression | COMPLETE |
| 71 | UNIQUE CURRENT STATE | Exact current UPDATE old RID is SELF_EXCLUDED | §§11.10.3, 11.10.6 | UPDATE exact-RID fixture | COMPLETE |
| 72 | UNIQUE CURRENT STATE | Published replacement exclusion is exact/context-local | §§11.10.3–11.10.4 | Continuation-context fixture | COMPLETE |
| 73 | UNIQUE CURRENT STATE | Same transaction's other live row conflicts | §§11.10.5–11.10.6 | Multirow/same-Txn fixture | COMPLETE |
| 74 | UNIQUE CURRENT STATE | Earlier-command self delete permits later reuse | §§11.10.4–11.10.6 | CommandId fixture | COMPLETE |
| 75 | UNIQUE CURRENT STATE | Current-command other-row self delete does not free key | §§11.10.4–11.10.6 | Same-command fixture | COMPLETE |
| 76 | UNIQUE CURRENT STATE | NULL-containing ordinary UNIQUE key skips duplicate admission | §11.10.2 | NULL matrix | COMPLETE |
| 77 | UNIQUE CURRENT STATE | PRIMARY KEY rejects NULL before unique predicate | §11.10.2 | PRIMARY KEY NULL fixture | COMPLETE |
| 78 | LOCK IDENTITY | Unique predicate equality uses canonical complete encoded bytes | §§11.8, 11.10.2 | Codec/equality vectors | COMPLETE |
| 79 | UNIQUE CURRENT STATE | Complete physical exact-key range is enumerated | §11.10.4 | Candidate harness event/count oracle | COMPLETE |
| 80 | UNIQUE CURRENT STATE | Corruption precedence survives an ordinary conflicting candidate | §11.10.4 | Conflict-plus-corrupt range fixture | COMPLETE |
| 81 | FAILURE PROPAGATION | RETIRED required by a unique candidate is an error | §11.10.4 | UNIQUE RETIRED fixture | COMPLETE |
| 82 | FAILURE PROPAGATION | INVALID required by a unique candidate is an error | §11.10.4 | UNIQUE INVALID fixture | COMPLETE |
| 83 | FAILURE PROPAGATION | RESERVED required by a unique candidate is an error | §11.10.4 | UNIQUE RESERVED fixture | COMPLETE |
| 84 | FAILURE PROPAGATION | Unique candidate status I/O failure propagates | §§11.10.4, 39.1 | UNIQUE failure matrix | COMPLETE |
| 85 | FAILURE PROPAGATION | Unique candidate status corruption propagates | §§11.10.4, 39.1 | UNIQUE failure matrix | COMPLETE |
| 86 | FAILURE PROPAGATION | Unique candidate unsupported format propagates | §§11.10.4, 39.1 | UNIQUE failure matrix | COMPLETE |
| 87 | REVALIDATION | Dangling/reused/wrong-key candidate RID is corruption | §§11.10.4, 11.10.8 | UNIQUE identity-failure fixture | COMPLETE |
| 88 | UNIQUE RACES | Concurrent same-key INSERTs cannot both commit | §§11.9, 11.10.7 | Both-holder-outcomes race | COMPLETE |
| 89 | UNIQUE RACES | Unique UPDATE versus INSERT cannot both own one key | §§11.9, 11.10.6–11.10.7 | Both lock-order/outcome fixtures | COMPLETE |
| 90 | UNIQUE RACES | DELETE/INSERT result follows deleter terminal outcome | §§11.9–11.10 | Delete/insert race | COMPLETE |
| 91 | TERMINAL RELEASE | UNIQUE_KEY remains held through C4/A2 and releases C5/A3 | §11.11 | Unique release-boundary fixture | COMPLETE |
| 92 | UNIQUE CURRENT STATE | Nonunique index maintenance does not invoke logical uniqueness admission | §§11.8–11.10 | Nonunique event-log fixture | COMPLETE |
| 93 | QUEUE ORDER | Ordinary exclusive queue admits W1/W2/W3 in FIFO order | §11.12 | Queue/lifetime matrix | COMPLETE |
| 94 | CANCELLATION | Canceled head is atomically removed and W2 can advance | §§11.12, 11.13.4 | Queue/lifetime matrix | COMPLETE |
| 95 | CANCELLATION | Deadlock-victim head cannot later receive grant | §§11.12, 11.13.4 | Queue/lifetime matrix | COMPLETE |
| 96 | REENTRANCY | Same-owner TUPLE_WRITE reacquire succeeds without wait | §11.12 | Queue/lifetime matrix | COMPLETE |
| 97 | REENTRANCY | Same-owner UNIQUE_KEY reacquire succeeds without wait | §11.12 | Queue/lifetime matrix | COMPLETE |
| 98 | REENTRANCY | Same-owner reacquire creates no self-edge/self-holder conflict | §§11.12–11.13 | Graph event assertion | COMPLETE |
| 99 | TERMINAL RELEASE | Reacquire cannot shorten or split terminal ownership | §§11.11–11.12 | Same-owner retained-lifetime fixture | COMPLETE |
| 100 | REENTRANCY | Identical/stronger same-owner gate requests are subsumed | §§11.13.2–11.13.3 | Complete gate matrix | COMPLETE |
| 101 | ACQUISITION | Unsupported same-owner gate transitions reject before wait | §11.13.2 | Rejection/no-edge fixtures | COMPLETE |
| 102 | NO-LATE-GRANT | MUST_ABORT waiter is canceled and cannot resume | §§11.12, 11.13.4 | Queue/lifetime matrix | COMPLETE |
| 103 | NO-LATE-GRANT | ABORTING/ABORTED waiter cannot receive ordinary grant | §§11.12–11.13 | Queue/lifetime matrix | COMPLETE |
| 104 | NO-LATE-GRANT | COMMITTING/COMMITTED waiter cannot receive ordinary grant | §§11.12–11.13 | Queue/lifetime matrix | COMPLETE |
| 105 | CANCELLATION | Cancel-versus-release admits only its linearized legal winner | §§11.12–11.13 | Two-order barrier fixture | COMPLETE |
| 106 | CANCELLATION | Canceled/disconnected request removes all outgoing wait state | §11.13.4 | Queue/graph snapshot assertion | COMPLETE |
| 107 | DEADLOCK GRAPH | Every registered transaction resource family uses one graph | §11.13.1 | Cross-family cycles | COMPLETE |
| 108 | DEADLOCK GRAPH | Waiter has an edge to every incompatible owner | §11.13.4 | Independent blocker-set comparison | COMPLETE |
| 109 | DEADLOCK GRAPH | Elected queue predecessors that prevent grant have edges | §11.13.4 | FIFO/fairness predecessor fixture | COMPLETE |
| 110 | DEADLOCK GRAPH | Relevant edge addition/replacement triggers synchronous detection | §11.13.4 | Scan-complete-before-sleep event order | COMPLETE |
| 111 | DEADLOCK VICTIM | Overlapping simple cycles in one SCC yield one SCC decision | §§11.13.4, 11.15 | Overlapping-cycle graph fixture | COMPLETE |
| 112 | DEADLOCK VICTIM | Disjoint cyclic SCCs each yield a victim | §11.13.4 | Two-SCC graph fixture | COMPLETE |
| 113 | DEADLOCK VICTIM | Victim is highest normal TxnId in each cyclic SCC | §11.13.4 | Independent SCC/max oracle | COMPLETE |
| 114 | DEADLOCK VICTIM | Victim receives DEADLOCK_DETECTED and enters MUST_ABORT | §11.13.4 | Victim state/result assertion | COMPLETE |
| 115 | CANCELLATION | Victim's outgoing wait is canceled for abort progress | §11.13.4 | Victim queue/edge snapshot | COMPLETE |
| 116 | TERMINAL RELEASE | Victim-held resources remain until A2/A3 | §§11.11, 11.13.4 | Paused-abort survivor fixture | COMPLETE |
| 117 | WAKE-UP REVALIDATION | Survivor wakes only on actual release and revalidates | §11.13.4 | Release/grant/recheck event sequence | COMPLETE |
| 118 | DEADLOCK VICTIM | Timeout is diagnostic and cannot choose the victim | §11.13.4 | Timeout-versus-SCC oracle fixture | COMPLETE |
| 119 | RESOURCE EXHAUSTION | Lock-entry allocation failure before first write is coherent FA | §§11.12, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 120 | RESOURCE EXHAUSTION | Lock-entry allocation failure after first write is MA | §§11.12, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 121 | RESOURCE EXHAUSTION | Waiter allocation failure before first write leaves no request/edge | §§11.12–11.13, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 122 | RESOURCE EXHAUSTION | Waiter allocation failure after first write is coherent MA | §§11.12–11.13, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 123 | RESOURCE EXHAUSTION | Graph-node capacity is established before request-capable registration | §§11.13.1, 39.1 | Registration/lazy-node fault fixture | COMPLETE |
| 124 | RESOURCE EXHAUSTION | Lazy post-write graph-node failure, if reachable, cannot publish a waiter | §§11.13.1, 39.1 | Representation-appropriate node fixture | COMPLETE |
| 125 | RESOURCE EXHAUSTION | Graph-edge allocation failure before first write cancels coherently | §§11.13.4, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 126 | RESOURCE EXHAUSTION | Graph-edge allocation failure after first write is coherent MA | §§11.13.4, 39.1 | Resource-exhaustion matrix | COMPLETE |
| 127 | DEADLOCK GRAPH | No request sleeps without every represented dependency | §11.13.4 | Sleep-entry graph snapshot | COMPLETE |
| 128 | FAILURE PROPAGATION | Uncertain graph/ownership coherence is noncontinuable | §§11.13.4, 39.1 | Graph-coherence failure fixture | COMPLETE |
| 129 | TERMINAL RELEASE | Commit release occurs at C5 after C4 publication | §§11.11, 15.5 | COMMIT Fault-Injection plus wait observer | COMPLETE |
| 130 | TERMINAL RELEASE | Abort release occurs at A3 after A2 publication | §§11.11, 15.6 | ABORT Fault-Injection plus wait observer | COMPLETE |
| 131 | TERMINAL RELEASE | MUST_ABORT retains owned tuple/key/gates until ABORTED | §§11.11, 11.13.4 | Paused-abort fixture | COMPLETE |
| 132 | WAKE-UP REVALIDATION | Holder terminal release never substitutes for fresh state checks | §§11.3, 11.13.4 | Tuple/unique/gate wake fixtures | COMPLETE |
| 133 | CANCELLATION | Every transaction-ineligible waiter is canceled before ordinary continuation | §§11.12–11.13.5 | State-transition queue matrix | COMPLETE |
| 134 | SHUTDOWN / LIFETIME | DRAINING rejects new ordinary statements/gates | §11.13.5 with §3.3.6 | Database Lifecycle Tests plus lock event log | COMPLETE |
| 135 | SHUTDOWN / LIFETIME | DATABASE_NONCONTINUABLE admits no new ordinary acquisition | §11.13.5 | Lifecycle/noncontinuable fixture | COMPLETE |
| 136 | SHUTDOWN / LIFETIME | Lock/gate manager outlives holders/waiters through controlled drain | §§11.12–11.13.5 | Drain/destruction event ordering | COMPLETE |
| 137 | RECOVERY RESET | Reopen creates empty transaction lock/gate wait state | §§11.12–11.13 | Recovery reset matrix | COMPLETE |
| 138 | RECOVERY RESET | Pre-crash holders do not survive | §§11.12–11.13 | Recovery reset matrix | COMPLETE |
| 139 | RECOVERY RESET | Pre-crash waiters do not survive | §§11.12–11.13 | Recovery reset matrix | COMPLETE |
| 140 | RECOVERY RESET | Pre-crash wait-for edges do not survive | §11.13.4 | Recovery reset matrix | COMPLETE |
| 141 | RECOVERY RESET | Redo does not replay SQL locks or gate waits | §§11.10.8, 13.13 | Redo-dispatch event assertion | COMPLETE |
| 142 | RECOVERY RESET | Redo does not rerun SQL UNIQUE current-state admission | §§11.10.8, 13.13 | Redo-dispatch event assertion | COMPLETE |
| 143 | RECOVERY RESET | Ordinary admission begins only after READY | §§11.13.5, 13.19 | READY gate fixture | COMPLETE |
| 144 | RECOVERY RESET | Loser outcome reconstruction needs no runtime-lock replay | §§11.11–11.13, 13.15 | Loser recovery row | COMPLETE |
| 145 | CROSS-OWNER | Physical B+ candidate presence is not UNIQUE authority | §§11.10.1, 11.10.4 | Candidate/status harness | COMPLETE |
| 146 | FAILURE PROPAGATION | Status/identity failure becomes neither free key nor conflict | §§11.4, 11.10.4 | TUPLE/UNIQUE negative matrices | COMPLETE |
| 147 | RECOVERY RESET | Exact physical replay is distinct from SQL conflict admission | §§11.10.8, 11.15 | No-UNIQUE-replay fixture | COMPLETE |
| 148 | OTHER | Queue/container/mutex choice is not a verification oracle | §11.12 | Semantic event/oracle harness | COMPLETE |
| 149 | CROSS-OWNER | Published RETIRING claim rejects/cancels without a transaction edge | §§11.13.4–11.13.5 | Object-state revalidation fixture | COMPLETE |
| 150 | CROSS-OWNER | Nontransaction maintenance owners remain outside the transaction graph | §11.13.5 | Owner-domain event assertions | COMPLETE |
| 151 | SHUTDOWN / LIFETIME | Deadlock victim selection never authorizes physical unlink | §§11.13.5, 11.13.7 | DROP-victim drain fixture | COMPLETE |
| 152 | WAKE-UP REVALIDATION | Writer/publication wake uses current manifest/object state | §11.13.4 | Gate wake adversarial timelines | COMPLETE |
| 153 | CROSS-OWNER | Ordinary SELECT uses MVCC/descriptors without transaction gates | §§11.1, 11.13.6 | SELECT lock-event fixture | COMPLETE |
| 154 | LOCK MODE | Deferred table/intention/schema/key-range LockManager families are not silently added | §11.1 | Registry enumeration and unsupported-scope checks | COMPLETE |
| 155 | FAILURE PROPAGATION | UniqueViolation is emitted only for a valid logical owner | §§11.10.3–11.10.4, 39.1 | UNIQUE error-versus-violation assertions | COMPLETE |
| 156 | OTHER | Implementation remains parallel-ready without container-specific expectations | §§11.12, 11.15 | Concurrency semantics across alternate harness scheduling | COMPLETE |

Coverage totals for this 156-obligation inventory are:

```text
COMPLETE:      156
PARTIAL:         0
MISSING:         0
CONTRADICTORY:   0
```

---

### Vacuum and Reclamation Tests

This section is the detailed procedural owner for `ARCHITECTURE.md` §§14.1–14.18. It
specializes, without redefining, the independent owners for slots and tuple headers
(§§5.3–5.8), FSM (§6), pins/latches/writeback (§§7.6–7.12), index RID references
(§§8.20–8.23), transaction status/snapshots/lifecycle (§§9.4–9.15), visibility and
`RETIRED` (§§10.2–10.5), `TUPLE_WRITE` (§§11.2–11.13), WAL/page publication
(§§12.8–12.17), control/checkpoint/recovery (§§13.2–13.19), DML handoff (§§15.3–15.6),
and failures (§39.1). Correctness procedures remain separate from Vacuum Benchmarks.

#### Deterministic reclamation harness and independent oracles

Core races use barriers at semantic events, never elapsed-time sleeps. Instrumentation may
observe these conceptual events without requiring production function or container names:

```text
TxnId issued / transaction registration begins / write-status dependency becomes visible
transaction admitted / C4 or A2 publishes / dependency releases at C5 or A3
SQL snapshot or StatusHistoryGuard registers/releases / status lookup begins/reads result
tuple validated / creator or deleter status resolved / freeze or normalization proven
maintenance WAL privately encoded / authorized / durable / page mutation and page_lsn publish
cutoff candidate computed / dependency proof revalidated / required WAL target fixed
control-slot write begins / control fdatasync completes / runtime cutoff publishes
status frame retirement begins / sparse deallocation begins/completes
read epoch requested/visible/exits / RID retires at E / grace becomes true
TUPLE_WRITE request begins/becomes queued/granted/canceled/releases
index cleanup / prev_RID splice / NORMAL -> DEAD / zero-claim check / DEAD -> UNUSED
same-slot allocation / crash / recovery / READY
```

Crash fixtures construct exact persistent prefixes: choose the complete WAL prefix, data
page images, valid or torn control slot, and sparse page presence independently; omit later
operations; then reopen through the normal recovery gate. Runtime races use barriers and
state snapshots. Stress/random scheduling is complementary, never the correctness oracle.

Every negative fixture is canonical except for one missing proof or injected defect. In
particular, the epoch-only test clears lock/index/predecessor barriers, the lock-only test
clears epochs and persistent references, and the durability test completes semantic
normalization in memory while withholding only required WAL durability.

The independent oracles are:

- **visibility:** evaluate §§10.2–10.4 directly from fixture snapshot/status/header values;
- **status mapping:** compute `ordinal=T-2`, `PageNo=1+ordinal/32640`, and entry position
  with widened checked arithmetic, independently of production helpers;
- **cutoff alignment:** enumerate whole status pages and choose the greatest legal
  page-aligned exclusive cutoff satisfying every independently modeled bound;
- **epoch grace:** for retire epoch `R`, require no active `E <= R`;
- **RID reuse:** explicitly conjoin global deadness, index absence, persistent DEAD,
  predecessor absence, epoch grace, zero live `TUPLE_WRITE` claims, frame/page eligibility,
  and canonical UNUSED publication;
- **WAL prefix:** derive `durable_lsn` and retained reconstruction records from exact
  persisted complete records and DPT `rec_lsn`, not a production flush result;
- **control authority:** decode/CRC-check both §13.2 slots and independently select the
  greatest valid usable generation.

The harness must not require one vacuum thread, mutex, claim counter, epoch container,
checkpoint cadence, scheduling interval, or sparse-deallocation API. Observable ordering
and persistent results are the oracle.

#### Protection-domain matrix

| Protection | What the fixture protects | Lifetime | Kind | Logical-deadness effect | Cutoff effect | Same-RID reuse effect | Crash | Substitute? |
|---|---|---|---|---|---|---|---|---|
| SQL snapshot | logical visibility history | RC attempt or retained RR snapshot | runtime | may block | contributes persistent-proof horizon | only through deadness | not replayed | no |
| page latch | page bytes/structure | short critical section | runtime | none after release | none | not a substitute | absent | no |
| BufferPool pin | resident frame binding | guard lifetime | runtime | none | coordinates status-frame retirement | not a substitute | absent | no |
| read epoch | retained physical RID identity | RID-retaining operation | runtime | none | none | blocks grace/reuse | absent | no |
| live `TUPLE_WRITE` request | one exact writer-target RID | queued/granted request through cancellation or C5/A3 | runtime | none by itself | none by itself | blocks reuse | absent | no |
| `StatusHistoryGuard(G)` | status lookups for normal TxnIds `>=G` | maintenance lookup/recheck | runtime | none | blocks cutoff above `G` | none | absent | no |
| write-status dependency | transaction's own possible future TxnId publication | registration through C5/A3 | runtime | none | blocks retirement of own TxnId | none | absent | no |

No row substitutes for another outside its stated domain.

#### Write-status registration, lifetime, and snapshot contrast

Pause write-capable transaction registration before and after its own write-status
dependency becomes coordinator-visible and before transaction admission. Assert that no
persistent write can begin before both active registration and the dependency are visible;
there is no first-write admission path. Race the pause with cutoff calculation in both
orders:

1. dependency first: any candidate that would retire `T` is clamped to the greatest legal
   aligned cutoff not passing `T`;
2. cutoff `C` first: the subsequently issued/admitted TxnId is `>=C` and registers before
   write admission;
3. pause between assignment and registration: exactly one of those orders wins, never
   `T<C` followed by unpinned write-capable admission.

For an idle READ COMMITTED transaction, unregister the statement snapshot while retaining
write capability; cutoff still cannot pass its own TxnId, while
`global_oldest_snapshot_xmin` excludes it. An explicitly read-only transaction has no
write-status dependency, may separately hold an RC/RR snapshot, and rejects every
persistent-write attempt rather than changing mode after admission.

Pause commit at C3, C4, and before/after C5, and abort at MUST_ABORT, A2, and before/after
A3. The dependency remains through C4/A2 and failed ACTIVE statements and disappears only
during C5/A3. After release, prohibit any new self-TxnId publication, but independently
retain the cutoff blocker while an existing tuple/catalog `xmin` or effective `xmax` still
needs the outcome.

The mandatory race/lifetime matrix is:

| Case | SQL snapshot | Write-status dependency | Cutoff may pass own T? | Existing persistent dependency | Expected |
|---|---|---|---|---|---|
| write-capable T registers first | optional | yes | no | optional | aligned candidate clamps |
| cutoff C publishes first | none yet | registered before admission | only because new `T>=C` | none yet | no retired-self TxnId |
| assignment/registration race | optional | one legal linearization | never while `T<C` is admitted | optional | no gap |
| idle RC write-capable T | none | yes | no | optional | own status pinned only |
| RR write-capable T | retained | yes | no | optional | two independent bounds |
| explicit read-only T | optional | no | according to snapshot/persistent proof | none creatable | write rejected |
| C4 before C5 | none required | yes | no | maybe | terminal publication insufficient |
| after C5, persisted xmin | none required | no | no | yes | persistent proof still blocks |
| A2 before A3 | none required | yes | no | maybe | terminal publication insufficient |
| after A3, persisted metadata | none required | no | no | yes | normalization/removal still required |

The Chapter-9 snapshot procedures remain the owner of RC/RR fields. This matrix asserts
only that snapshot-horizon and status-history bounds are never conflated.

#### StatusHistoryGuard, lookup, and `RETIRED`

Acquire `StatusHistoryGuard(G)` before a status-dependent scan and pause before reading a
terminal value. A candidate cutoff greater than `G`, runtime publication, and physical page
retirement must remain blocked until guard release and revalidation. In the opposite order,
publish `C`, then begin lookup for `T<C`; lookup returns `RETIRED` from cutoff precedence
without page access and no new guard resurrects retired history. Boundary fixtures use
`T=G-1`, `T=G`, and larger values to prove the architecture's lower-bounded protected
range, not an invented global or half-open interval.

For otherwise valid persisted metadata that still requires `Status(T)`, force `RETIRED`
separately for creator `xmin` and effective deleter `xmax`. Both take the §9.13/§10.4
reclamation-invariant failure path; neither outcome is guessed. Repeat with persisted
`INVALID`, recognized `RESERVED`, status I/O failure, checksum corruption, and unsupported
status format. Assert no freeze, normalization, DEAD decision, or page mutation is
authorized. The Transaction-status lookup and MVCC Visibility sections remain the detailed
owners of precedence and exact failure classification.

#### Creator freezing, aborted-xmax normalization, and status independence

Creator-freeze fixtures use a valid committed creator at the exact §14.13.2 horizon:
just eligible, equality boundary, and one relation too new. Independently evaluate every
legal future snapshot before and after WAL-backed `xmin=FROZEN_TXN_ID,cmin=0`; visibility
must be identical and only the creator dependency disappears. A normal `xmax` remains a
separate dependency. ABORTED, IN_PROGRESS, required-RETIRED, INVALID, RESERVED, and lookup
failure creator results prohibit freeze and leave bytes unchanged.

Normalization fixtures use a valid still-live tuple with `Status(xmax)=ABORTED`; publish
the WAL-backed `xmax=INVALID_TXN_ID,cmax=0` rewrite and independently prove pre/post
visibility equivalence. Creator dependency is unchanged and deleter dependency disappears.
COMMITTED, IN_PROGRESS, unsafe SELF, required-RETIRED, INVALID, RESERVED, and lookup
failure cases prohibit normalization. Pre-authorization failure restores/leaves the old
canonical bytes; post-authorization failure follows the existing §12.12 publication and
§39.1 rules without fabricated rollback.

The existing PAGE_DELTA/PAGE_IMAGE procedures remain the owner of record-family bytes and
assert that each authorized maintenance mutation installs its authorizing record-start LSN
as `page_lsn`, updates dirty/DPT state, and obeys WAL-before-data. Reclamation assumes no new
WAL record family or maintenance-commit record.

The status-independence matrix is:

| Persisted state | Creator dependency | Deleter dependency | Freeze? | Normalize? | Removal needed for represented dependency? | Cutoff may pass referenced T? | `RETIRED` legal afterward? |
|---|---|---|---|---|---|---|---|
| normal committed xmin | yes | as xmax says | when §14.13.2 eligible | no | freeze or removal | no before action | yes after crash-recoverable action |
| FROZEN xmin | no | as xmax says | already frozen | no | no creator removal | yes for old creator if other bounds clear | yes |
| aborted xmin | yes until version removal | as xmax says | no | no | yes | no before removal | yes after removal |
| normal committed xmax | as xmin says | yes | creator only | no | removal/authorized independence | no | yes only afterward |
| normal aborted xmax | as xmin says | yes | creator only | yes | normalization or removal | no | yes after recoverable action |
| normalized aborted xmax | as xmin says | no | creator only | already normalized | no deleter removal | yes for deleter if other bounds clear | yes |
| physically removed version | no | no | no | no | complete | yes if no other reference | yes |
| catalog MVCC tuple | same tuple rules | same tuple rules | when eligible | when aborted xmax | same as user tuple | only after equivalent proof | only afterward |

Freezing is also tested against a live physical tuple: it does not mark DEAD, remove an
index, satisfy epoch grace, or clear a lock claim. Normalizing aborted `xmax` generally
makes a tuple semantically more live and never authorizes DEAD/reuse. A two-axis fixture
permits status retirement for status-independent metadata while a stale index, epoch, or
lock claim still forbids physical RID reuse.
A write-capable transaction's own write-status dependency, when it retains no RID, likewise
does not block reuse of an unrelated RID.

#### Cutoff domain, prerequisite durability, and second-crash retention

For candidate `C`, build the dependency set independently, round down to the greatest legal
exclusive status-page boundary using the 32,640-entry mapping, and test `T=C-1`, `T=C`, a
candidate inside a page, the greatest legal aligned cutoff, and attempted overflow. `T<C`
is retired only after proof; `T=C` remains outside the retired range. No arithmetic wraps
or partially retires a status page.

For every freeze, normalization, or removal prerequisite at authorizing record-start LSN
`L`, select exactly one proof:

```text
durable page: the status-independent page image is stably stored and no longer needs L
durable WAL:  the old disk image remains, but the complete reconstructive WAL is durable
              and retained through the page's DPT rec_lsn dependency
```

With multiple prerequisites at `L1<L2<L3`, independently exclude already-durable pages and
set `required_wal_target` to the maximum authorizing LSN among the remaining WAL-dependent
pages. Pause WAL flush below and at that complete-record target. A successful
`fdatasync(database.control)` is never accepted as evidence that unrelated WAL reached
durability.

The mandatory durability matrix is:

| State | Semantically independent in memory? | Crash-recoverable? | Cutoff may publish? | WAL retained afterward? | READY after crash? |
|---|---:|---:|---:|---:|---:|
| dirty freeze, WAL undurable | yes | no | no | required but absent | old cutoff only |
| dirty freeze, WAL durable/retained | yes | yes | yes if other proofs hold | yes | yes after redo |
| durable frozen page | yes | yes | yes | only ordinary remaining dependencies | yes |
| dirty normalized xmax, WAL undurable | yes | no | no | required but absent | old cutoff only |
| dirty normalized xmax, WAL durable/retained | yes | yes | yes if other proofs hold | yes | yes after redo |
| control write pending | according to page/WAL proof | yes | not authoritative | yes | old valid slot selected |
| control durable, old data page | yes | yes via WAL | already published | yes | redo before READY |
| recycling crosses required rec_lsn | yes | would become no | forbidden | yes | not constructible |
| recovered dirty page before second crash | yes | yes only while WAL retained | already published | yes until dependency clears | yes |

Direct fixtures include:

1. volatile-only freeze and volatile-only normalization: control publication remains old;
2. durable status-independent page: publication needs neither a page force nor a checkpoint;
3. dirty old disk page plus durable retained WAL: publish `C`, crash, redo before READY;
4. required WAL durable but control publication fails: old cutoff remains authoritative and
   authorized maintenance progress remains valid;
5. control `C` durable plus old disk page plus missing/recycled WAL: impossible because the
   recycling floor refuses to cross the page's `rec_lsn`;
6. second crash: recover the dirty page after the first crash, leave it dirty under
   NO-FORCE, attempt recycling, crash again, and require the same reconstruction source.

WAL flush failure before control publication leaves the old cutoff. WAL authority
uncertainty follows the existing noncontinuable/no-guessing procedure. A torn or failed
new control slot is independently decoded on reopen; a complete durable new slot may be
selected only because every prerequisite was already recoverable. Runtime cutoff
publication is observed after durable control publication and before any physical status
page retirement that relies on it.

The checkpoint/retention matrix composes the Checkpoint/Recovery and WAL procedures:

| Case | Required `rec_lsn`/WAL | Cutoff publication | Recycling | Crash oracle |
|---|---|---|---|---|
| dirty frozen heap page | freeze dirty-interval `rec_lsn` | allowed after WAL durable | no crossing dependency | redo freeze |
| dirty normalized heap/catalog page | normalization dirty-interval `rec_lsn` | allowed after WAL durable | no crossing dependency | redo normalization |
| dirty TXN_STATUS page, `F/T` | `rec_lsn=F`, `page_lsn=T` | cutoff alone changes nothing | retain F until durable or safely retired | reconstruct from F then T |
| checkpoint before cutoff | captured DPT floor | later cutoff uses it | only installed floor | old/new cutoff both recoverable |
| checkpoint after cutoff | every still-dirty prerequisite represented | already authoritative | no premature advance | selected pair recoverable |
| cutoff durable, page dirty | prerequisite `rec_lsn` retained | yes | forbidden past it | old disk page repaired |
| WAL segment candidate | minimum of all DPT/writer/checkpoint needs | irrelevant by itself | only if below complete floor | no lost source |
| page durable/dependency cleared | no page need | yes | ordinary recycling allowed | stable page sufficient |

Race freeze, normalization, and status-page retirement independently against checkpoint
DPT capture. Force both legal linearizations and assert that the installed checkpoint
either captures the old recovery dependency or the new dirty interval. Then combine durable
cutoff, dirty page, checkpoint, recycling attempt, and second crash. No fixture introduces
a cutoff WAL record, forces a checkpoint, or forces the affected data page.

#### Read epochs, `TUPLE_WRITE` handoff, and claim lifetime

Pause read-epoch registration before and after the reader becomes visible to the reclaimer,
then race RID retirement/final reuse. Registration first blocks grace; reuse first means the
reader cannot validate/retain the old identity. For retire epoch `R`, independently test
active epochs `R-1`, `R`, and `R+1`: either of the first two blocks, while `R+1` alone does
not. A long-lived epoch may cause wait or deferral but never forced reuse. After the last
blocking exit, the candidate is revalidated before final publication; no notification
mechanism is assumed.

At `current_epoch=UINT64_MAX-1`, retire once and observe the last legal increment state;
the next retirement attempt fails/requires maintenance restart before reuse and never wraps
to zero or aliases an old reader. Page latches and pins are tested separately: a latch
protects bytes, a pin protects frame binding, and neither replaces an epoch once a RID is
retained outside that operation.

For a writer, pause request registration while the discovery epoch remains active. Reuse is
blocked by the epoch until the request becomes a canonical live queued or granted claim;
only then may the epoch release, and it releases before blocking. Cover immediate grant,
one queued waiter, three queued waiters, same-owner effective ownership, and resource
failure before the request becomes live. Failure leaves the epoch held and permits only the
existing fail/retry path.

The mandatory `TUPLE_WRITE` matrix is:

| State | Epoch active? | Live claim? | Grant possible? | Reuse? | Claim release | Revalidation? |
|---|---:|---:|---:|---:|---|---:|
| no request, no retained RID | no | no | no | other barriers decide | n/a | at reuse |
| registration in progress | yes | not yet | no | no | n/a | yes |
| immediate grant before epoch exit | yes, then no | yes | granted | no | C5/A3 | yes |
| queued waiter | no after registration | yes | yes | no | cancellation or grant | yes after grant |
| multiple waiters | no | yes for each | yes | no until all gone | individually linearized | yes |
| granted holder | no | yes | granted | no | C5/A3 | yes |
| C4 before C5 | no | yes | granted | no | C5 | yes |
| A2 before A3 | no | yes | granted | no | A3 | yes |
| cancellation/grant race | no | exactly winner's state | exactly winner | no if grant wins | cancellation or C5/A3 | if granted |
| queued deadlock victim | no | until canonical removal | until removal | no while possible | removal with no late grant | no continuation |
| granted deadlock victim | no | yes | granted | no | A3 | no ordinary continuation |
| crash with waiter | process ends | no after reopen | no | persistent barriers decide | process death | fresh discovery only |

Cancellation and grant are forced in both orders. Grant first converts continuously to
owner claim; cancellation first simultaneously removes grant eligibility and the claim.
No ended request may receive a late grant. A granted deadlock victim and every MUST_ABORT
holder remain through A3. Commit remains pinned through C4 until C5.

Race request registration against the reclaimer's zero-claim/reuse publication while the
writer still holds its epoch. Registration first blocks reuse. If reuse has already won,
epoch-protected identity revalidation prevents old-identity request admission. Post-wait
revalidation remains mandatory: mutate `xmax`, visibility, current-owner, or key state while
the waiter sleeps and require the existing Chapter-11/15 outcome despite stable RID
identity.

The read-epoch matrix is:

| Case | DEAD allowed? | UNUSED allowed? | Same-RID reuse? | Reclaimer action | Proof |
|---|---:|---:|---:|---|---|
| reader registered before retirement | yes after index cleanup | no | no | wait/defer | active `E<=R` |
| registration races reuse | legal winner only | only reuse-first | only reuse-first | revalidate | shared linearization |
| old epoch active | yes | no | no | wait/defer | minimum active `<=R` |
| grace complete | yes | other barriers decide | other barriers decide | continue/revalidate | no active `E<=R` |
| no old epoch, lock claim remains | yes | no | no | wait/defer | independent claim |
| long reader | yes | no | no | wait/defer indefinitely | no forced reuse |
| crash | persistent DEAD remains | only after fresh retirement/grace and other proof | only after every proof | re-enqueue | runtime epochs absent |

#### Index cleanup, predecessor links, DEAD/UNUSED, and complete reuse

For one reclaimable NORMAL RID referenced by at least two indexes, derive every historical
key, remove exact `(key,RID)` entries through ordinary B+ MTRs, and publish DEAD only after
all are known absent. Crash after zero, one, or all removals: a recovered NORMAL repeats
idempotent cleanup; recovered DEAD proves cleanup preceded publication. Any index failure
leaves the candidate NORMAL or DEAD-but-unreused according to the already-authorized prefix;
it never permits first-index-only success. Include a UNIQUE index to prove vacuum removes
the physical stale entry without rerunning SQL uniqueness admission.

Construct removable middle/head-adjacent/consecutive chain versions and surviving direct
successors. WAL-backed splicing must replace each `S.prev=V` with `S.prev=V.prev`; state
change or cleanup failure defers reuse. A stale index or `prev_RID` is isolated as the sole
remaining barrier and must prevent new-identity aliasing.

The complete one-missing-barrier matrix is:

| Fixture | Global dead | Indexes absent | DEAD durable | `prev_RID` absent | Epoch grace | Zero lock claims | Frame/page valid | DEAD allowed? | UNUSED allowed? | Same-RID reuse? |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| not globally dead | no | yes | no | yes | yes | yes | yes | no | no | no |
| one stale index | yes | no | no | yes | yes | yes | yes | no | no | no |
| DEAD not durable | yes | yes | no | yes | yes | yes | yes | publication pending | no | no |
| stale predecessor | yes | yes | yes | no | yes | yes | yes | yes | no | no |
| old epoch | yes | yes | yes | yes | no | yes | yes | yes | no | no |
| queued/granted claim | yes | yes | yes | yes | yes | no | yes | yes | no | no |
| frame/page transition ineligible | yes | yes | yes | yes | yes | yes | no | yes | no | no |
| every barrier clear | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes |

For the positive row, compact while still DEAD if needed, assert zero tuple coordinates,
publish exactly one free-list link and `UNUSED`, then allocate a new tuple into that same
SlotId. The independent reuse oracle proves no old reader, waiter, index entry, predecessor,
or resident-frame transition can treat the numeric RID as the old tuple. `NORMAL->UNUSED`
bypass and direct allocation from DEAD are rejected.

Crash fixtures cover: waiter present with DEAD persisted; legal UNUSED before crash; and a
WAL-valid new same-RID allocation. Reopen has no pre-crash epoch, guard, write-status claim,
or lock request. DEAD remains DEAD and is freshly retired before reuse; UNUSED retains its
canonical free-list semantics; new allocation never resurrects an old runtime identity.

Hold an old RR snapshot to prevent global deadness, then separately hold only a long read
epoch after logical death. The first blocks tuple garbage eligibility; the second permits
status-independent maintenance but blocks physical reuse. Old committed normal `xmin` and
surviving normal committed `xmax` independently block status cutoff until freeze,
normalization/removal, or another authorized status-independent representation becomes
crash-recoverable. Positive cases then allow retirement after FROZEN creator, normalized
aborted `xmax`, or fully removed version, subject to all other cutoff bounds.

#### Status-page reclamation, control crashes, and sparse deallocation

Use the independent status mapping to select a page wholly below `C`, the boundary page
containing `C`, and an interior page followed by later populated pages. Only the wholly
retired page may proceed through guard exclusion, pin/I/O drain, frame retirement,
BufferPool/DPT removal, and optional sparse deallocation. Later PageNos and file length
remain address-stable. `T<C` returns `RETIRED` without page access; a missing page required
for `T>=C` follows the owning I/O/corruption result.

Run two abstract storage capabilities:

1. keep-size sparse deallocation succeeds for an interior whole-page range; allocated
   backing may disappear while logical size and offsets remain unchanged;
2. deallocation is unavailable or fails; bytes remain safe extra storage, durable cutoff
   stays authoritative, and ordinary retry/resource policy applies.

No test requires a platform syscall or block-allocation granularity. Crash during the
physical optimization may leave any platform-valid allocation state wholly below the
already-authoritative cutoff, but the logical file remains correctly sized/addressable.

The mandatory cutoff/control/status-page crash matrix is:

| Persistent boundary | Selected cutoff | Status page required? | Maintenance WAL required? | Page absence legal? | Recovery action | READY? |
|---|---|---:|---:|---:|---|---:|
| old cutoff, old dependent page | old | yes | according to old DPT | no | ordinary recovery | yes if valid |
| maintenance WAL durable, control old | old | yes | retained/replayable | no premature reclaim | replay extra progress | yes |
| torn new control | older valid slot | yes under old cutoff | retained | no if old cutoff needs it | select old slot | yes if inputs present |
| new cutoff durable, status page present | new `C` | no below C | as pages require | yes but unnecessary | ignore stale status bytes | yes |
| new cutoff durable, page sparsely reclaimed | new `C` | no below C | as pages require | yes | `RETIRED` precedence | yes |
| crash during sparse deallocation | new `C` | no below C | unchanged | yes wholly below C | preserve logical mapping | yes |
| new `C`, dirty frozen heap page | new `C` | no below C | yes | yes | redo heap before READY | yes |
| second crash before heap writeback | new `C` | no below C | retained dirty-page source | yes | redo again | yes |
| failed candidate, old-control fallback | old | yes | old recovery floor | no premature absence | select old slot | yes if inputs remain |

Directly prohibit physical status-page retirement whose legality depends on `C` before the
new control generation is durable. Publish `C` while retaining the old status page and
verify stale storage is harmless; then retire it after every guard/frame/DPT condition and
verify absence is accepted. Pin a status frame and race retirement: the frame remains bound
until canonical drain and cannot later rewrite sparsely deallocated storage. Hold a
`StatusHistoryGuard` and prove the same page cannot be invalidated until guard release.

For a dirty TXN_STATUS page with `rec_lsn=F,page_lsn=T`, cutoff publication alone never
releases `F`. Case A keeps the page semantically required and retains F. Case B makes the
whole page retired below durable `C`, drains pins/I/O, and removes its BufferPool/DPT
ownership through §14.14.2 before its own reconstruction dependency may clear. Heap/catalog
`rec_lsn` dependencies are never cleared by status-page retirement.

#### FSM, lifecycle, scheduling, and failure methodology

After authoritative heap reclamation, inject stale-low and stale-high FSM values. Stale-low
may miss space; stale-high must be rejected by heap-page geometry recheck. FSM update
failure leaves legal heap state authoritative and the FSM rebuildable. Page compaction may
move tuple bytes while preserving SlotIds and cannot substitute for DEAD/UNUSED proof.
If retained status blocks contribute to a later storage-full condition, the next legitimate
write receives the existing storage/resource error; the fixture never permits status-page
compaction, renumbering, or a weakened cutoff proof.

Every permitted manual or background trigger runs the same safety procedures. Tests do not
assert cadence, threshold, worker count, fairness, or prompt progress. A guard, epoch, lock
claim, stale reference, or insufficient durability may produce wait, skip, or defer where
§14.17 leaves policy free; the oracle is only that unsafe cutoff/reuse does not publish.

Lifecycle barriers map to Database Lifecycle Tests: maintenance is admitted only in READY;
DRAINING admits no new pass and drives active transactions through ordinary C5/A3 while
maintenance completes/restores its current unit and quiesces; CLOSING, RECOVERING, and
NONCONTINUABLE admit no new reclamation mutation. After crash, epochs, guards,
write-status dependencies, and lock requests are absent; only persistent page/control/WAL/
index/predecessor state governs recovery. No runtime claim is replayed.

The failure matrix is:

| Failure/blocker | Mutation authorized? | Cutoff changes? | RID reusable? | Continuation/result owner |
|---|---:|---:|---:|---|
| candidate not globally reclaimable | no | no | no | benign skip/defer, §14.3 |
| active snapshot | no reclaim action | no past dependency | no | wait/defer, §§14.2–14.3 |
| active epoch | DEAD may exist | unaffected | no | wait/defer, §14.6 |
| live `TUPLE_WRITE` claim | DEAD may exist | unaffected | no | wait/defer, §§11.12, 14.12 |
| status lookup I/O | no dependent rewrite | no | no new permission | owning status I/O result, §39.1 |
| tuple/page corruption | no | no | no | corruption owner, §§4.13, 39.1 |
| required `RETIRED` dependency | no | no | no | reclamation invariant/noncontinuable, §§9.13, 14.17.1 |
| required INVALID/RESERVED | no | no | no | existing invariant result, §§9.11–9.13 |
| WAL failure before authorization | no publication | no | no new permission | existing pre-authorization result, §12.12 |
| WAL failure after authorization | authoritative progress | only if later proofs finish | only after all barriers | §12.12/§39.1 outcome |
| prerequisite WAL flush failure | page progress may exist | no new cutoff | unaffected | maintenance fails/defers |
| WAL authority uncertainty | uncertain unit | no new cutoff | no | existing noncontinuable path |
| control publication failure | prerequisites may remain valid | old cutoff | unaffected | old slot authority / I/O result |
| sparse deallocation failure | cutoff already authoritative | unchanged | unrelated | safe extra storage / retry policy |
| index cleanup failure | partial MTR prefix may exist | unaffected | no | exact B+ error; retry idempotently |
| `prev_RID` splice failure | authorized prefix only | unaffected | no | defer/error owner |
| runtime workspace exhaustion | no unsafe partial publication | no | no | existing resource result; fail/defer |
| claim-registration exhaustion | no live request | unaffected | epoch still blocks | Chapter-11/§39 result |
| shutdown cancellation | only completed/restored unit | no unsafe advance | no unsafe reuse | lifecycle owner |

#### High-level reclamation domain/case matrix

| Domain/case | Deterministic fixture/fault | Independent oracle | Architecture | Verification owner | Status |
|---|---|---|---|---|---|
| write-status registration | pause before admission | coordinator event order | §§9.4, 14.17.1 | registration procedure | COMPLETE |
| idle RC writer | no snapshot, candidate past T | separate horizon/dependency sets | §§14.2, 14.17.1 | write-status matrix | COMPLETE |
| explicit read-only | retained snapshot/no dependency | mode/write-admission oracle | §§9.15, 14.2 | write-status matrix | COMPLETE |
| C5/A3 release | pause each terminal stage | lifecycle stage oracle | §§9.14, 14.14 | lifetime procedure | COMPLETE |
| persistent dependency after release | stored normal xmin/xmax | status-dependency table | §14.14 | status matrix | COMPLETE |
| creator freeze | eligible committed creator | visibility oracle | §14.13.2 | freeze procedure | COMPLETE |
| aborted xmax | ABORTED deleter | visibility oracle | §14.13.1 | normalization procedure | COMPLETE |
| required RETIRED | valid dependent field | status precedence oracle | §§9.13, 14.14.3 | negative lookup | COMPLETE |
| cutoff alignment | inside-page candidate | widened page arithmetic | §14.14.1 | cutoff procedure | COMPLETE |
| candidate clamp | active dependency/guard | independent bound set | §14.17.1 | coordinator races | COMPLETE |
| guard race | pause guarded lookup | event order | §14.17.1 | guard procedure | COMPLETE |
| volatile freeze | dirty memory, WAL omitted | persistent-prefix oracle | §14.14.2 | durability matrix | COMPLETE |
| dirty freeze, durable WAL | old disk page | WAL/DPT oracle | §14.14.2 | crash fixture | COMPLETE |
| second crash | recovered page still dirty | retention-floor oracle | §§13.10, 14.14.2 | second-crash fixture | COMPLETE |
| epoch registration | registration/reuse barriers | epoch-set oracle | §14.6 | epoch procedure | COMPLETE |
| grace boundary | E=R-1/R/R+1 | mathematical predicate | §14.6.3 | epoch matrix | COMPLETE |
| queued waiter | live request, epoch released | claim-state oracle | §§11.12, 14.12 | lock matrix | COMPLETE |
| granted owner | pause C4/A2 | lifecycle/claim oracle | §§11.11, 14.12 | lock matrix | COMPLETE |
| waiter cancellation | cancel/grant both orders | no-late-grant oracle | §11.12 | cancellation procedure | COMPLETE |
| DEAD with waiter | all other barriers clear | reuse conjunction | §§14.5, 14.12 | reuse matrix | COMPLETE |
| stale index | only index barrier missing | exact B+ entry set | §§8.23, 14.9 | reuse matrix | COMPLETE |
| stale predecessor | only link barrier missing | decoded link set | §14.10 | reuse matrix | COMPLETE |
| complete reuse | every barrier true | explicit conjunction | §14.12 | positive reuse | COMPLETE |
| sparse status reclaim | interior page below C | mapping/file-length oracle | §14.14 | sparse procedure | COMPLETE |
| torn control | invalid new slot | independent slot selection | §§13.2, 14.14.2 | crash matrix | COMPLETE |
| reclaimed lookup below C | absent page | cutoff precedence | §14.14.3 | lookup fixture | COMPLETE |
| missing required page | absent page at/above C | mapping/status oracle | §§9.13, 14.14.3 | negative fixture | COMPLETE |

All core negative cases isolate one defect, use barriers or exact persistent prefixes, and
remain valid under any conforming scheduling, lock-table, epoch-registry, BufferPool, WAL,
checkpoint, or sparse-storage implementation.

#### Chapter 14 atomic architecture-obligation coverage map

The inventory below is derived from the final live §§14.1–14.18 contract. A row is
`COMPLETE` only when the named direct procedure or precise existing owner supplies a
deterministic fixture and independent oracle.

| # | Domain | Atomic obligation | Architecture owner | Verification owner / deterministic procedure | Status |
|---:|---|---|---|---|---|
| 1 | A STATUS-DEPENDENCY MODEL | Normal committed xmin requires creator outcome | §§14.13–14.14 | Status-independence matrix | COMPLETE |
| 2 | A | Effective normal xmax requires deleter outcome | §§14.13–14.14 | Status-independence matrix | COMPLETE |
| 3 | A | Opaque numeric occurrences do not automatically pin status | §14.14 | Status table plus Statistics verification | COMPLETE |
| 4 | A | Runtime future-publication and persisted dependencies are distinct | §14.14 | Write-status lifetime contrast | COMPLETE |
| 5 | B WRITE-STATUS REGISTRATION | Every write-capable normal transaction registers own dependency | §§9.4, 14.17.1 | Registration barrier fixture | COMPLETE |
| 6 | B | Registration precedes persistent-write admission | §§9.4, 14.17.1 | Pre-admission event assertion | COMPLETE |
| 7 | B | Registration is not lazy first-write work | §§9.4, 14.17.1 | No-first-write-path oracle | COMPLETE |
| 8 | B | Explicit read-only transaction may omit dependency | §§9.15, 14.17.1 | Read-only contrast | COMPLETE |
| 9 | B | Assignment-to-registration admits no cutoff gap | §§9.8, 14.17.1 | Two-order barrier fixture | COMPLETE |
| 10 | C WRITE-STATUS LIFETIME | ACTIVE and failed-ACTIVE retain dependency | §§9.4, 14.17.1 | Statement-failure pause | COMPLETE |
| 11 | C | MUST_ABORT and ABORTING retain dependency | §§9.14, 14.17.1 | Abort-stage fixture | COMPLETE |
| 12 | C | COMMITTING retains through C4 | §§9.14, 14.17.1 | Commit-stage fixture | COMPLETE |
| 13 | C | Commit release occurs only during C5 | §§9.14, 14.17.1 | C4/C5 cutoff probes | COMPLETE |
| 14 | C | Abort release occurs only during A3 | §§9.14, 14.17.1 | A2/A3 cutoff probes | COMPLETE |
| 15 | D SQL SNAPSHOT HORIZON | Horizon is minimum registered snapshot xmin | §14.2 | Snapshot registry oracle | COMPLETE |
| 16 | D | Idle RC writer does not pin logical horizon | §14.2 | RC no-snapshot contrast | COMPLETE |
| 17 | D | Retained RR snapshot pins logical horizon | §14.2 | Long-RR fixture | COMPLETE |
| 18 | D | Snapshot and write-status bounds remain independent | §§14.2, 14.17.1 | Three-case contrast matrix | COMPLETE |
| 19 | E STATUSHISTORYGUARD | Guard registers before dependent lookup | §14.17.1 | Guard event order | COMPLETE |
| 20 | E | Guard clamps cutoff above G | §14.17.1 | Guard-first race | COMPLETE |
| 21 | E | Cutoff-first guard cannot resurrect retired history | §14.17.1 | Cutoff-first race | COMPLETE |
| 22 | E | Multiple guards use minimum bound | §14.17.1 | Multi-guard boundary fixture | COMPLETE |
| 23 | F STATUS LOOKUP / RETIRED | Below-cutoff lookup returns RETIRED without page read | §§9.13, 14.14.3 | Access-instrumented lookup | COMPLETE |
| 24 | F | RETIRED dependent xmin is invariant failure | §§10.2, 14.14.3 | Creator negative fixture | COMPLETE |
| 25 | F | RETIRED dependent xmax is invariant failure | §§10.3, 14.14.3 | Deleter negative fixture | COMPLETE |
| 26 | F | INVALID/RESERVED required outcome is rejected | §§9.13, 14.17.1 | Status negative matrix | COMPLETE |
| 27 | F | Status I/O/corruption/format errors propagate | §§9.13, 39.1 | Lookup failure injection | COMPLETE |
| 28 | G CREATOR FREEZING | Only committed eligible creator freezes | §14.13.2 | Positive freeze fixture | COMPLETE |
| 29 | G | Freeze writes xmin FROZEN and cmin zero | §14.13.2 | Independent tuple-byte oracle | COMPLETE |
| 30 | G | Freeze preserves visibility for legal future snapshots | §§10.2, 14.13.2 | Independent visibility comparison | COMPLETE |
| 31 | G | Freeze horizon boundary is exact | §§14.2, 14.13.2 | B-1/B/B+1 snapshot fixture | COMPLETE |
| 32 | G | Invalid creator statuses never freeze | §§10.2, 14.13.2 | Creator negative matrix | COMPLETE |
| 33 | G | Freeze leaves normal xmax dependency independent | §§14.13–14.14 | FROZEN+xmax fixture | COMPLETE |
| 34 | H XMAX NORMALIZATION | Only proven ABORTED xmax normalizes | §14.13.1 | Positive normalization fixture | COMPLETE |
| 35 | H | Normalization writes INVALID xmax and cmax zero | §14.13.1 | Independent tuple-byte oracle | COMPLETE |
| 36 | H | Normalization preserves visibility | §§10.3, 14.13.1 | Independent visibility comparison | COMPLETE |
| 37 | H | Normalization removes only deleter dependency | §§14.13–14.14 | Dependency table | COMPLETE |
| 38 | H | Committed/in-progress/unsafe SELF xmax does not normalize | §§10.3, 14.13.1 | Xmax negative matrix | COMPLETE |
| 39 | H | RETIRED/INVALID/RESERVED/failure does not normalize | §§10.3, 14.17.1 | Xmax negative matrix | COMPLETE |
| 40 | I DEADNESS / RECLAIMABILITY | Aborted creator is garbage candidate | §14.3.1 | Status/garbage fixture | COMPLETE |
| 41 | I | Committed delete is garbage only below snapshot horizon | §14.3.2 | Long-RR and boundary fixtures | COMPLETE |
| 42 | I | IN_PROGRESS creator/deleter is skipped without latch wait | §14.4 | Barrier/latch event assertion | COMPLETE |
| 43 | I | Garbage eligibility is global, not vacuum snapshot visibility | §§14.2–14.3 | Divergent-snapshot fixture | COMPLETE |
| 44 | I | DEAD is semantically cleaned but not reusable | §§14.5, 14.8 | State-transition fixture | COMPLETE |
| 45 | J STATUS-INDEPENDENCE | FROZEN creator removes creator dependency | §§14.13–14.14 | Positive retirement fixture | COMPLETE |
| 46 | J | Normalized aborted xmax removes deleter dependency | §§14.13–14.14 | Positive retirement fixture | COMPLETE |
| 47 | J | Physical removal removes tuple status dependencies | §14.14 | Removal/dependency fixture | COMPLETE |
| 48 | J | Catalog MVCC metadata follows same proof | §14.14 | Catalog tuple specialization | COMPLETE |
| 49 | J | Runtime dependency release alone is insufficient | §14.14 | C5/A3 persisted-reference fixture | COMPLETE |
| 50 | J | Status-independent in memory is not crash proof | §14.14 | Volatile-page fixture | COMPLETE |
| 51 | K STATUS CUTOFF DOMAIN | Cutoff is exclusive | §14.14.1 | T=C-1/C oracle | COMPLETE |
| 52 | K | Cutoff is whole-page aligned | §14.14.1 | Independent 32640 arithmetic | COMPLETE |
| 53 | K | Initial cutoff is first normal TxnId | §14.14.1 | Initial control fixture | COMPLETE |
| 54 | K | Maximum aligned cutoff does not wrap | §§4.3, 14.14.1 | Synthetic exhaustion fixture | COMPLETE |
| 55 | K | Cutoff is monotonic | §§14.14, 14.17.1 | C1/C2/control-generation fixture | COMPLETE |
| 56 | L CUTOFF COORDINATION | Candidate accounts for every persistent dependency | §§14.14, 14.17.1 | Independent dependency-set oracle | COMPLETE |
| 57 | L | Candidate accounts for write-status dependencies | §14.17.1 | Transaction-first race | COMPLETE |
| 58 | L | Candidate accounts for SQL snapshots | §§14.2, 14.17.1 | Snapshot clamp fixture | COMPLETE |
| 59 | L | Candidate accounts for guards | §14.17.1 | Guard-first race | COMPLETE |
| 60 | L | Exactly one cutoff publisher wins | §14.17.1 | Two-reclaimer serialization fixture | COMPLETE |
| 61 | L | Candidate is revalidated before publication | §§14.11, 14.17.1 | State-change barrier fixture | COMPLETE |
| 62 | M CUTOFF PREREQUISITE DURABILITY | Volatile freeze with undurable WAL cannot authorize cutoff | §14.14.2 | Durability matrix | COMPLETE |
| 63 | M | Volatile normalization with undurable WAL cannot authorize cutoff | §14.14.2 | Durability matrix | COMPLETE |
| 64 | M | Durable status-independent page is sufficient | §14.14 | Durable-page fixture | COMPLETE |
| 65 | M | Durable retained reconstructive WAL is sufficient | §14.14 | Dirty-page crash fixture | COMPLETE |
| 66 | M | Checkpoint is not inherently required | §14.14 | No-checkpoint positive fixture | COMPLETE |
| 67 | M | Data-page force is not inherently required | §14.14 | Dirty-page positive fixture | COMPLETE |
| 68 | M | Required WAL target is max needed authorizing LSN | §14.14 | L1/L2/L3 independent oracle | COMPLETE |
| 69 | N CUTOFF CONTROL PUBLICATION | Required WAL durability precedes control sync | §14.14.2 | Held-WAL/control-sync race | COMPLETE |
| 70 | N | Control fdatasync does not durabilize WAL | §14.14.2 | Separate persistence fault domains | COMPLETE |
| 71 | N | Durable control precedes runtime cutoff publication | §14.14.2 | Event-order assertion | COMPLETE |
| 72 | N | Failed control publication leaves old cutoff | §14.14.2 | Alternating-slot fault fixture | COMPLETE |
| 73 | N | Torn control selects a safe valid slot | §§13.2, 14.14.2 | Independent control oracle | COMPLETE |
| 74 | N | Physical status retirement cannot precede durable C | §14.14.2 | Pre-cutoff reclaim rejection | COMPLETE |
| 75 | O WAL RETENTION / SECOND CRASH | Dirty prerequisite retains DPT rec_lsn | §§13.7–13.10, 14.14 | DPT snapshot assertion | COMPLETE |
| 76 | O | Cutoff publication does not clear rec_lsn | §14.14.2 | Before/after DPT comparison | COMPLETE |
| 77 | O | Required WAL cannot recycle while page depends on it | §§13.10, 14.14.2 | Recycling refusal fixture | COMPLETE |
| 78 | O | Recovery before READY reconstructs status independence | §§13.13, 14.14 | Dirty-old-page crash | COMPLETE |
| 79 | O | Recovered dirty page retains WAL for second crash | §14.14.2 | Two-crash fixture | COMPLETE |
| 80 | O | Durable page may clear ordinary WAL dependency | §§7.10, 13.10, 14.14 | Stable-page fixture | COMPLETE |
| 81 | O | TXN_STATUS F remains retention base until cleared/safely retired | §§12.10.5, 13.10, 14.14.2 | F/T matrix | COMPLETE |
| 82 | P TXN_STATUS PAGE RECLAMATION | Only page wholly below cutoff is eligible | §§14.14.1–14.14.2 | Whole/boundary page fixture | COMPLETE |
| 83 | P | Guard exclusion precedes page retirement | §14.17.1 | Guard/page race | COMPLETE |
| 84 | P | Pins and I/O drain before frame retirement | §14.14.2 | BufferPool retirement fixture | COMPLETE |
| 85 | P | Retired frame cannot write obsolete contents | §14.14.2 | Delayed-writeback assertion | COMPLETE |
| 86 | P | Reclaimed page below C is semantically unnecessary | §14.14.3 | Missing-below-C lookup | COMPLETE |
| 87 | P | Missing required page at/above C is error | §§9.13, 14.14.3 | Missing-required-page fixture | COMPLETE |
| 88 | Q SPARSE DEALLOCATION | Sparse reclaim preserves file length | §§14.14.2–14.14.3 | File-length oracle | COMPLETE |
| 89 | Q | Sparse reclaim preserves absolute PageNos | §§9.12, 14.14 | Interior-page fixture | COMPLETE |
| 90 | Q | Later pages are not shifted/renumbered | §14.14.2 | Offset/mapping comparison | COMPLETE |
| 91 | Q | Unsupported sparse deallocation permits safe extra storage | §14.14.2 | Capability-absent fixture | COMPLETE |
| 92 | Q | Failed sparse deallocation does not roll back cutoff | §14.14.2 | Post-cutoff failure fixture | COMPLETE |
| 93 | R READ-EPOCH REGISTRATION | Reader registers before retaining index RID | §14.6.1 | Cursor event-order fixture | COMPLETE |
| 94 | R | Registration/reuse race has two legal orders | §§14.6.1, 14.12 | Two-order barrier fixture | COMPLETE |
| 95 | R | New reader after retirement cannot obtain removed entry | §14.6.2 | Post-retirement traversal fixture | COMPLETE |
| 96 | R | Pure reader needs no TUPLE_WRITE claim | §§14.6.1, 14.7 | Reader-only fixture | COMPLETE |
| 97 | S READ-EPOCH GRACE | Retirement records pre-increment epoch | §14.6.2 | Event/value assertion | COMPLETE |
| 98 | S | Active E less than retire R blocks | §14.6.3 | Mathematical matrix | COMPLETE |
| 99 | S | Active E equal R blocks | §14.6.3 | Mathematical matrix | COMPLETE |
| 100 | S | Only E greater than R permits epoch component | §14.6.3 | Mathematical matrix | COMPLETE |
| 101 | S | Long epoch may delay but never permit unsafe reuse | §§14.6.3, 14.17 | Long-reader fixture | COMPLETE |
| 102 | T TUPLE_WRITE CLAIMS | Every queued grant-eligible request is a RID claim | §§11.12, 14.5 | Queued-waiter fixture | COMPLETE |
| 103 | T | Every granted owner is a RID claim | §§11.11–11.12, 14.5 | Granted-holder fixture | COMPLETE |
| 104 | T | Queue position does not change claim status | §11.12 | Multi-waiter fixture | COMPLETE |
| 105 | T | Empty lock-table container is not a claim | §§11.12, 14.12 | Empty-container semantic fixture | COMPLETE |
| 106 | T | Claim protects identity, not visibility/status | §§14.5, 14.7 | Changed-semantics revalidation fixture | COMPLETE |
| 107 | U EPOCH-TO-LOCK HANDOFF | Discovery epoch remains through request registration | §§11.3, 14.6.1 | Paused-registration fixture | COMPLETE |
| 108 | U | Immediate grant exists before epoch release | §§11.3, 14.6.1 | Immediate-grant fixture | COMPLETE |
| 109 | U | Queued claim exists before epoch release | §§11.3, 14.6.1 | Queued-handoff fixture | COMPLETE |
| 110 | U | Blocking wait retains no epoch/page latch | §§11.3, 14.7 | Wait-entry ownership assertion | COMPLETE |
| 111 | V CLAIM CANCELLATION / RELEASE | Cancellation removes claim with grant eligibility | §§11.12, 14.12 | Cancellation-first fixture | COMPLETE |
| 112 | V | Grant-first race preserves owner claim | §11.12 | Grant-first fixture | COMPLETE |
| 113 | V | Queued deadlock victim claims until canonical removal | §§11.13.4, 14.12 | Victim-removal fixture | COMPLETE |
| 114 | V | Granted deadlock victim retains claim through A3 | §§11.11, 11.13.4 | Paused-abort fixture | COMPLETE |
| 115 | V | Commit holder retains through C4 and releases C5 | §§11.11, 14.12 | C4/C5 reuse probes | COMPLETE |
| 116 | V | Abort holder retains through A2 and releases A3 | §§11.11, 14.12 | A2/A3 reuse probes | COMPLETE |
| 117 | W INDEX RID CLEANUP | Every required exact index entry is absent before DEAD | §14.9 | Multi-index cleanup fixture | COMPLETE |
| 118 | W | Exact erase is idempotent across crash/retry | §14.9 | Prefix crash matrix | COMPLETE |
| 119 | W | One remaining index blocks DEAD/reuse | §§14.5, 14.9 | One-defect fixture | COMPLETE |
| 120 | W | Unique residual cannot alias new tuple | §§8.23, 14.9 | Unique-index alias fixture | COMPLETE |
| 121 | W | B+ cleanup occurs without heap latch | §14.9 | Latch event assertion | COMPLETE |
| 122 | X PREV_RID CLEANUP | Surviving successor cannot point to reusable RID | §14.10 | Decoded-link oracle | COMPLETE |
| 123 | X | Splice rewrites direct successor to predecessor | §14.10 | WAL/page-byte fixture | COMPLETE |
| 124 | X | Unproven or failed splice defers reuse | §§14.10–14.12 | Failure/state-change fixture | COMPLETE |
| 125 | X | Consecutive removable versions splice safely | §14.10 | Multi-version chain fixture | COMPLETE |
| 126 | Y DEAD-TO-UNUSED / REUSE | NORMAL cannot bypass DEAD to UNUSED | §§5.4, 14.5 | Invalid-transition fixture | COMPLETE |
| 127 | Y | DEAD persistence precedes retirement/reuse | §§14.5, 14.8 | Crash-prefix fixture | COMPLETE |
| 128 | Y | Payload coordinates are zero before UNUSED | §§5.4.3, 14.12 | Independent page-byte oracle | COMPLETE |
| 129 | Y | UNUSED publishes exactly one free-list link | §§5.3.2, 14.12 | Free-list graph oracle | COMPLETE |
| 130 | Y | Epoch grace is independently required | §§14.6.3, 14.12 | One-missing-barrier matrix | COMPLETE |
| 131 | Y | Zero lock claims is independently required | §14.12 | One-missing-barrier matrix | COMPLETE |
| 132 | Y | Index and predecessor absence remain required | §§14.9–14.12 | One-missing-barrier matrix | COMPLETE |
| 133 | Y | Positive same-slot allocation requires all barriers | §14.12 | Complete conjunction fixture | COMPLETE |
| 134 | Y | Whole-page recycling cannot bypass per-RID proof | §§14.5, 14.18 | Whole-page fixture | COMPLETE |
| 135 | Z CRASH / RESTART | Pre-crash epochs are not replayed | §14.6.4 | Recovery runtime-state snapshot | COMPLETE |
| 136 | Z | Pre-crash guards/write dependencies/locks are not replayed | §§14.6.4, 14.17.1 | Recovery reset fixture | COMPLETE |
| 137 | Z | Persistent DEAD remains DEAD after reopen | §14.8 | DEAD crash fixture | COMPLETE |
| 138 | Z | Recovered DEAD receives fresh retirement before reuse | §§14.6.4, 14.8 | Re-enqueue event assertion | COMPLETE |
| 139 | Z | Legal UNUSED remains canonical after reopen | §§5.4, 14.12 | UNUSED crash fixture | COMPLETE |
| 140 | Z | Post-reuse recovery never resurrects old identity | §§14.6.4, 14.12 | New-allocation crash fixture | COMPLETE |
| 141 | AA BUFFERPOOL / FRAME | Reclaimer does not invalidate pinned status frame | §§7.9, 14.14.2 | Pin/drain fixture | COMPLETE |
| 142 | AA | Retired frame is non-writeback before sparse reclaim | §14.14.2 | Delayed-writeback race | COMPLETE |
| 143 | AA | Heap final transition revalidates under page latch | §§14.11–14.12 | State-change barrier fixture | COMPLETE |
| 144 | AA | Pin/latch never substitute for RID epoch | §§7.7–7.9, 14.7 | Protection matrix fixtures | COMPLETE |
| 145 | AB FSM / FREE SPACE | Heap state remains free-space authority | §§6.10–6.12, 14.16 | Stale-high/low fixtures | COMPLETE |
| 146 | AB | FSM update may be batched/stale | §14.16 | Delayed-update fixture | COMPLETE |
| 147 | AB | FSM update failure does not roll back heap reclaim | §§6.10, 14.16 | Failure fixture | COMPLETE |
| 148 | AB | Compaction preserves SlotIds and is not reuse | §§5.5, 14.12 | Compaction/RID fixture | COMPLETE |
| 149 | AC SHUTDOWN / LIFETIME | Maintenance is admitted only in READY | §14.17.1 | Lifecycle admission matrix | COMPLETE |
| 150 | AC | DRAINING rejects new work and quiesces active units | §14.17.1 | Drain event sequence | COMPLETE |
| 151 | AC | Active transactions retain dependencies through C5/A3 during drain | §14.17.1 | Drain/terminal fixture | COMPLETE |
| 152 | AC | NONCONTINUABLE admits no new maintenance mutation | §14.17.1 | Lifecycle failure fixture | COMPLETE |
| 153 | AC | Scheduling policy never weakens safety predicates | §§14.16–14.17 | Trigger-equivalence fixture | COMPLETE |
| 154 | AD FAILURE / EXHAUSTION | Vacuum workspace exhaustion publishes no unsafe partial state | §§14.11, 39.1 | Resource fault fixture | COMPLETE |
| 155 | AD | Claim-registration exhaustion retains epoch | §§11.3, 14.6 | Handoff resource fault | COMPLETE |
| 156 | AD | Status-history pressure cannot force cutoff | §§14.14, 14.17 | Pressure/defer fixture | COMPLETE |
| 157 | AD | Sparse-storage pressure cannot authorize renumbering | §14.14.2 | Full-storage fixture | COMPLETE |
| 158 | AD | Corrupt tuple/page authorizes no maintenance mutation | §§14.11, 39.1 | Single-defect corruption fixture | COMPLETE |
| 159 | AD | Pre-authorization failure leaves old canonical state | §§12.12, 14.13 | WAL fault fixture | COMPLETE |
| 160 | AD | Post-authorization failure preserves authoritative progress | §§12.12, 14.17.1 | WAL fault fixture | COMPLETE |
| 161 | AE OTHER / SEPARATION | Status retirement and RID reuse are independent axes | §§14.7, 14.14 | Two-axis matrix | COMPLETE |
| 162 | AE | No new old TxnId can be introduced below cutoff | §§9.4, 14.14 | Post-cutoff write-admission fixture | COMPLETE |
| 163 | AE | Maintenance scheduling correctness is trigger-independent | §§14.16–14.17 | Trigger-equivalence fixture | COMPLETE |
| 164 | AE | Verification uses semantic events, not implementation containers | §§14.6, 14.17.1 | Harness implementation-freedom assertions | COMPLETE |

Coverage totals for this 164-obligation inventory are:

```text
COMPLETE:      164
PARTIAL:         0
MISSING:         0
CONTRADICTORY:   0
```

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
