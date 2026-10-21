# Numeric Exhaustion and Terminal-Boundary Verification

This section is the detailed verification owner for the checked-advancement,
exhaustion, and terminal-state contracts in [`ARCHITECTURE.md`](../ARCHITECTURE.md)
§4.3.2 and §§4.3.2.1–4.3.2.6. The architecture remains the authority for legal
values and outcomes; this section defines how tests construct and observe those
boundaries. Existing WAL/MTR, PAGE_INIT, catalog, CommandId, statistics, and
vacuum procedures remain the specialized oracles referenced below.

## Exhaustion-domain inventory

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
| `SchemaVer` | A | uint32; `UINT32_MAX`; the architecture emits only `1` | `0` invalid | Checked schema-history advancement before catalog publication | Never for one table | Reject schema-changing DDL before publication | §§4.3.2.1, 16.5 |
| Control-slot generation | G | uint64; `UINT64_MAX` | `0` invalid | Alternating control-slot write and sync before runtime generation publication | Never | Control update fails; its requiring operation cannot proceed | §§4.3.2, 13.2.3–13.2.4 |
| `PageNo` | D | uint64 carrier; last allocatable ordinary page `1,125,899,906,842,622` | `UINT64_MAX` invalid; page `0` is superblock | Checked PageNo/count/offset/length before extension and WAL/page publication | Unpublished tail only after exact rollback; published reuse only by owner protocol | `PAGE_NUMBER_EXHAUSTED` for that file | §§4.3.2.3, 4.11, 12.12 |
| `published_page_count` | D | uint64 exclusive bound; max `1,125,899,906,842,623` | `0` cannot describe initialized file | Reconstructed from aligned length plus WAL/recovery, then advanced with page publication | May retreat only for proven unpublished serialized tail | Same PageNo/I/O boundary | §§4.3.2.3, 4.11, 12.12 |
| `SlotId` | E | uint16 carrier; geometry permits `0..1017` | `UINT16_MAX` invalid; no global allocator terminal | Page-local slot-directory growth or grace-authorized free-slot pop | Only canonical `DEAD -> UNUSED` after §14.6 grace | Page-local `NO_SPACE`, never global ID exhaustion | §§4.3.2, 5.3–5.5, 14.6 |
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

## Universal checked-boundary procedure

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

## Synthetic fixture and instrumentation rules

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

## Persistent high-water and crash procedure

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

## Durable identifier specializations

### TxnId terminal block

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

### FileId and shared catalog-object IDs

For FileId and the one shared TableId/IndexId/ConstraintId allocator, run B-1/B0/B+1 at
`MAX-1`, verify durable `candidate+1` before return, and crash after that durability but
before object/catalog publication. The candidate remains consumed even if only a private
or orphan file used it. For the shared catalog allocator, alternate TableId, IndexId, and
ConstraintId requests at the boundary and prove they consume one sequence rather than
three independent maxima. Drop/abort/retire old objects, reopen, and prove neither allocator
reuses an old value or repairs a gap. Repeated terminal requests retain
`FILE_ID_EXHAUSTED` or `ID_EXHAUSTED` rather than becoming corruption.

### CommandId, ColumnId, SchemaVer, and control generation

Admit a statement at `CommandId{UINT32_MAX}` and allow it to complete. The next ordinary
statement in that transaction is rejected with `COMMAND_ID_EXHAUSTED` before parsing,
binding, or execution publication; the transaction remains eligible to COMMIT existing
work or ROLLBACK, and unrelated transactions begin with legal CommandId zero. Connect this
case to Statement Failure and Transaction-State Tests and the existing CommandId procedure.

Construct complete schemas at the largest owner-encodable ColumnId/SchemaVer boundaries.
The first unencodable add-column/schema evolution is rejected before any catalog row or
descriptor publication, old schema versions remain readable, and no historical ColumnId
or SchemaVer wraps or is reused. Where the architecture exposes no such ALTER, exercise the schema
builder/validator boundary without claiming SQL support or inventing a named error.

For control generation, install a valid selected generation `UINT64_MAX`, request each
operation that requires a control update, and prove failure occurs before constructing a
wrapped generation zero or publishing any dependent checkpoint/high-water/reclamation
state. A malformed persisted generation zero follows control-slot validation/fallback,
not ordinary numeric exhaustion.

## Page and slot structural specialization

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

## WAL position, maximum record, and terminal headroom specialization

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

## Encoded, structural, generation, and epoch specializations

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

## Error, statement, and lifecycle oracles

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

## Domain coverage matrix

`U` means the universal B-1/B0/B+1 procedure owns the case; `S` means the named
specialization adds the domain oracle; `P` means the persistent high-water matrix applies;
`V` means owner validation supplies B+N; and N/A means the architecture defines no such
dimension. Every inventory row appears separately. Each mapping identifies a required verification procedure.

| Domain | B-1 | B0 | B+1 | B+N | Persistence | Crash gap | Restart | Reuse | Concurrency | Error category | Lifecycle outcome | Verification owner |
|---|---:|---:|---:|---:|---|---|---|---|---|---|---|---|
| FileId | U | U | U | V | control high-water | P | terminal persists | forbidden | serialized/concurrent requests | `FILE_ID_EXHAUSTED` | only new file/object allocation fails | Durable identifier specializations |
| TableId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations |
| IndexId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations |
| ConstraintId | U | U | U | V | shared control high-water | P | terminal persists | forbidden | shared allocator contention | `ID_EXHAUSTED` | DDL requiring ID fails | Durable identifier specializations |
| TxnId | U | S | S | V | durable block end | P, lost suffix | terminal persists | forbidden | block reservation contention | `TXN_ID_EXHAUSTED` | new transaction admission fails | TxnId terminal block |
| CommandId | U | S | S | N/A above uint32 | transaction-local | N/A | transaction-local | forbidden within transaction | N/A | `COMMAND_ID_EXHAUSTED` | later statement fails; COMMIT/ROLLBACK legal | CommandId specialization |
| ColumnId | U | U | U | V | catalog publication | no row publication | schemas survive reopen | forbidden in history | catalog owner | owning DDL rejection | schema construction fails | Schema specialization |
| SchemaVer | U | U | U | V | catalog publication | no row publication | old schemas readable | forbidden in history | catalog owner | owning DDL rejection | schema evolution fails | Schema specialization |
| Control-slot generation | U | S | S | V zero/malformed | alternating control slot | surviving-slot oracle | selected max remains | forbidden | serialized control update | requiring operation fails | dependent control update cannot proceed | Control specialization |
| PageNo | U | S | S | V | file length + WAL publication | unpublished-tail exception | reconcile before READY | predicate-gated | serialized append | `PAGE_NUMBER_EXHAUSTED` | append in one file fails | Page specialization |
| `published_page_count` | U | S | S | V | reconstructed exclusive bound | exact tail rollback | reconstruct/reconcile | unpublished retreat only | serialized append | PageNo/I/O boundary | cannot outrun file/WAL authority | Page specialization |
| SlotId | U | S | S | V | heap page/WAL | no global gap | page recovery | grace-gated | page-latched | `NO_SPACE`, not ID exhaustion | page-local insertion/reuse result | Slot specialization |
| WAL Lsn/exclusive end | U | S | S | V | valid WAL prefix/durable LSN | specialized append oracle | terminal end reconstructed | forbidden | WAL reservation | `WAL_POSITION_EXHAUSTED` | ordinary WAL work gated; credited closure preserved | WAL specialization |
| WAL segment index | U | S | S | V | WAL namespace | segment publication oracle | inventory/recovery | forbidden | WAL reservation | `WAL_POSITION_EXHAUSTED` | no later segment position | WAL specialization |
| Statistics chunk count/index | U | U | U | V | generation-atomic catalog rows | failed generation unpublished | fallback after reopen | not within generation | ANALYZE owner | generation invalid/failure | ordinary statistics fallback | Encoded specialization |
| B+ tree height/node level | U | S | S | V | root MTR publication | no provisional root | existing tree valid | contraction only | tree protocol | root-growth failure | insertion fails before new root publication | B+ specialization |
| Heap `slot_count` | U | U | U | V | heap page/WAL | no identity gap | page recovery | slots grace-gated | page-latched | `NO_SPACE` | page-local capacity path | Structural specialization |
| Heap `tuple_length` | U | U | U | V | page/MTR publication | no partial tuple | page recovery | N/A | owner serialization | `ROW_TOO_LARGE` | tuple operation fails | Encoded specialization |
| FSM `entry_count` | U | U | U | V | FSM page publication | no partial count | rebuild/reopen | N/A | owner serialization | next-page/PageNo result | owning FSM growth path | Free-space map verification — initialized prefix and suffix |
| B+ node `slot_count` | U | U | U | V | MTR publication | no partial node | tree recovery | structural | tree protocol | split/rebalance result | existing tree remains valid | Encoded specialization |
| B+ key/entry lengths | U | U | U | V | MTR publication | no partial entry | tree recovery | N/A | tree protocol | key/construction failure | index operation fails before publication | Encoded specialization |
| WAL total/payload length | U | S | S | V | no reservation on failure | no WAL gap | valid prefix unchanged | N/A | WAL reservation | record/encoded-length failure | WAL operation fails before reservation | WAL specialization |
| Default-blob `total_length` | U | U | U | V | catalog publication | no catalog row publication | catalog remains readable | N/A | catalog owner | oversized-default rejection | DDL/default construction fails | Encoded specialization |
| `PersistedScalarV1.payload_length` | U | U | U | V | enclosing owner | no enclosing publication | owner-specific reopen | N/A | owner serialization | enclosing-builder result | default/statistics owner decides | Encoded specialization |
| Statistics scope/manifest counts/lengths | U | U | U | V | generation-atomic publication | failed generation unpublished | fallback after reopen | N/A | ANALYZE owner | generation invalid/failure | statistics fallback | Encoded specialization |
| Checkpoint DATA indexes/counts/totals | U | U | U | V | checkpoint sequence | remains uninstalled | prior installed checkpoint | N/A | checkpoint owner | checkpoint construction failure | close/open follows checkpoint requirement | Encoded specialization |
| Runtime generation tokens | U | S | S | N/A persisted | process-local | N/A | N/A persistence | quiescence-gated | owning domain | operation rejection | unrelated runtime work may continue | Generation specialization |
| Pin/reference counters | U | U | U | N/A persisted | process-local | N/A | N/A persistence | decrement only | owning domain | acquisition failure | existing owners remain valid | Generation specialization |
| Read epoch | U | S | S | V zero | process-local | N/A durable gap | restart/quiescence reinit | restart/quiescence-gated | epoch mutex | retirement/reuse disabled | unrelated operations continue | Epoch specialization |

## Architecture-obligation coverage map

The following inventory maps every cross-domain obligation to one procedure owner. Domain
rows marked N/A in the matrix are excluded only where the architecture does not define that
dimension.

| # | Architecture obligation and source | Verification owner |
|---:|---|---|
| 1 | Checked candidate construction — §4.3.2 steps 1–3 | Universal checked-boundary procedure — observation sequence |
| 2 | No wrap/increment-then-test — §§4.3.2, 4.3.2.6 item 1 | Universal checked-boundary procedure — wrap-prone paragraph |
| 3 | No truncation/narrowing — §§4.3.2, 4.3.2.4–4.3.2.6 | Universal checked-boundary procedure — narrowing and maximum-construction paragraphs |
| 4 | No sentinel allocation — §4.3.2 domain inventory | Universal checked-boundary procedure — reserved-value paragraph; domain specializations |
| 5 | Exact maximum legal result — §§4.3.2.1–4.3.2.5 | Universal checked-boundary procedure — B0; domain specializations |
| 6 | First illegal result rejected — §4.3.2 checked-next contract | Universal checked-boundary procedure — B+1 |
| 7 | Rejection before publication — §§4.3.2, 4.3.2.6 | Universal checked-boundary procedure — observation points and rejection oracle |
| 8 | Durable high-water before return — §§4.3.2.1, 9.3, 13.2.5–13.2.6 | Persistent high-water and crash procedure |
| 9 | Consumed-gap behavior — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — after-durability/before-return row |
| 10 | Restart persistence — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — restart and terminal rows |
| 11 | No no-reuse ID reclamation — §§4.3.2.1, 9.2–9.3 | Durable identifier specializations — TxnId and FileId/shared-object cases |
| 12 | Allowed-reuse preconditions — §§4.3.2.3, 4.3.2.5, 8.18, 14.6 | Page and slot structural specialization; Encoded, structural, generation, and epoch specializations |
| 13 | Maximum-size construction — §§4.3.2.4–4.3.2.6 | Universal checked-boundary procedure; WAL and encoded specializations |
| 14 | Arithmetic-overflow rejection — §§4.3.2, 4.3.2.6 items 3–4 | Universal checked-boundary procedure — arithmetic observation points and B+1 |
| 15 | WAL terminal headroom — §4.3.2.4 | WAL position, maximum record, and terminal headroom specialization |
| 16 | Error-domain distinction — §4.3.2.6 | Error, statement, and lifecycle oracles — category table and paired fixtures |
| 17 | Statement consequence — §§4.3.2.2, 4.3.2.6, 39.1 | Error, statement, and lifecycle oracles; CommandId specialization |
| 18 | Transaction consequence — §§4.3.2.4, 4.3.2.6, 39.1 | Error, statement, and lifecycle oracles; WAL specialization |
| 19 | Instance/lifecycle consequence — §4.3.2.6 and Chapter 3 | Error, statement, and lifecycle oracles — legal-operations table |
| 20 | Legal operations after exhaustion — §4.3.2.6 | Error, statement, and lifecycle oracles — legal-operations positive cases |
| 21 | Open/recovery after exhaustion — §§4.3.2.4, 4.3.2.6 | Persistent high-water and crash procedure; WAL open/shutdown cases |
| 22 | Corrupted above-bound state — §4.3.2 steps 1–3 and §§4.13–4.14 | Synthetic fixture and instrumentation rules — B+N corruption fixture |
| 23 | Concurrent terminal allocation where applicable — §§9.3, 13.2.4–13.2.6 | Persistent high-water and crash procedure — concurrent terminal paragraph |
| 24 | Crash during durable allocation where applicable — §§4.3.2.1, 4.3.2.6 | Persistent high-water and crash procedure — crash matrix and concurrent crash case |
