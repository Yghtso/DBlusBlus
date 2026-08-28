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
