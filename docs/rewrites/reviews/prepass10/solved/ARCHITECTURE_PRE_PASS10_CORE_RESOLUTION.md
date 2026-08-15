# Pre-Pass-10 Core Resolution

## Scope and authority

This resolution closes only the already-migrated transactional-storage issues `R-025..R-035`.

```text
Rewrite Pass 10: NOT performed
legacy ARCHITECTURE.md: unchanged
production code: unchanged
coverage rows §301+: still PENDING
```

Architecture snapshots:

```text
pre-resolution: c2e94c7ebc70c64ffa3db684e4fdd30e679f7906e574c15ffa1b0963315d96c8
final:          093f38ba408bad061d32b4d03b29f363920c9ffcc3fa539a35f420380c8dd0a9
legacy source:  2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86
```

The legacy architecture supplied the required subsystem semantics but left these exact binary/state/concurrency choices open. The selections below are explicit owner-delegated architecture decisions chosen for correctness, learning value, and compatibility with the already accepted design.

## R-025 — complete WAL v1 grammar

Record types are now stable persisted codes:

```text
0 WAL_PAD
1 PAGE_INIT
2 PAGE_DELTA
3 PAGE_IMAGE
4 BTREE_MTR
5 TXN_COMMIT
6 TXN_ABORT
7 CHECKPOINT_BEGIN
8 CHECKPOINT_DATA
9 CHECKPOINT_END
```

Key exact rules:

```text
header              = 48 bytes
total_length        = 48 + payload_length
physical span       = align_up(total_length, 8)
external pad bytes  = zero, outside CRC/total_length
flags/reserved      = zero in v1
WAL PageId          = 16 bytes
TXN_COMMIT/ABORT payload = 0 bytes
```

PAGE_DELTA patch metadata, PAGE_INIT/PAGE_IMAGE, BTREE_MTR affected-page entries, checkpoint payloads, padding and oversized-record rejection are byte-exact. Variable codecs reject trailing bytes.

User-owned page records participate in that user transaction's WAL chain; pure system page changes use TxnId zero. BTREE_MTR remains a system record with optional owner TxnId only for diagnostics.

## R-026 — exact `database.control`

The 8192-byte control file contains two independently checksummed 4096-byte slots. Each slot has an exact 80-byte v1 header with:

```text
DBLUSCTL magic
format/header version
zero flags
generation
checkpoint BEGIN/END/redo LSNs
reserved_txn_id_end
txn_status_reclaim_before
next_file_id
CRC32C
zero suffix
```

All control updates share one update mutex and merge from the latest in-memory state. The inactive slot is written and `fdatasync`ed before publication. Generation/FileId allocation never wraps; crash gaps are allowed.

## R-027 — checkpoint identity and recovery bounds

Checkpoint identity is exactly its `CHECKPOINT_BEGIN` LSN. DATA chunks use zero-based contiguous indexes; END stores chunk and entry totals plus redo LSN.

Recovery now separates:

```text
analysis:
    load validated checkpoint data
    scan WAL forward from CHECKPOINT_BEGIN

redo:
    scan from checkpoint_redo_lsn
    which may precede CHECKPOINT_BEGIN
```

This lets terminal records older than BEGIN repair a still-dirty transaction-status page without polluting loser analysis.

## R-028 — exact ReadEpochManager

The correctness baseline is deliberately mutex-based:

```text
reader registration:
    e = current_epoch
    active[e]++

RID retirement:
    retire_epoch = current_epoch
    current_epoch++

safe reuse:
    no active reader epoch <= retire_epoch
```

Recovered DEAD slots are conservatively re-enqueued because process-local epochs do not survive crash.

## R-029 — transaction-status physical reclamation

Absolute TxnId -> PageNo mapping remains unchanged. `txn_status_reclaim_before` is a page-aligned durable cutoff.

After the cutoff is durable, TransactionStatusStore stops new access to the retired range, any resident frames are drained and invalidated without obsolete writeback, and Linux may punch exact whole-page ranges using keep-size sparse-file semantics. File length and PageNos never shift.

Recovery ignores checkpoint/redo state for status pages wholly below the durable cutoff.

## R-030 — page checksum lifecycle

All persisted ordinary random-access pages are checksummed in WAL/recovery-enabled v1. Dirty resident checksum bytes may be stale.

BufferPool copies a stable image under the page latch, captures `modification_generation`, finalizes the CRC in the private copy, establishes WAL-before-data for that image's page LSN, writes it, and clears dirty/rec_lsn only if no newer mutation raced the write.

Full PAGE_INIT/PAGE_IMAGE/MTR images carry a canonical page LSN and valid embedded page checksum.

## R-031 — crash-safe append publication

The accepted Phase-1 raw append operation remains the low-level primitive. WAL mode adds a per-file append-publication lock and `published_page_count`.

A new ordinary page is visible only after its PAGE_INIT LSN is reserved, the canonical full image is finalized, and the complete record is appended. A new B+ page becomes reachable only through its complete publishing MTR.

Recovery may re-extend a file when durable publication WAL survived but the pre-crash file-size update did not. It then redoes those publications and truncates only a contiguous unpublished append suffix. This prevents durable interior allocation holes.

Heap publication is authoritative; FSM coverage follows as advisory/rebuildable metadata.

## R-032 — complete reusable-slot/vacuum state machine

V1 now activates the persisted intrusive free-slot list:

```text
UNUSED slot:
    tuple_offset = 0
    tuple_length = 0
    state        = UNUSED
    aux          = next UNUSED SlotId / INVALID_SLOT_ID
```

`free_slot_head` owns the list; DEAD slots never appear on it.

Vacuum cleanup is crash-idempotent through `EraseIfPresent(key,RID)`. After semantic death/index cleanup, payload can be compacted while the slot remains DEAD. Final `DEAD -> UNUSED` additionally requires both epoch grace and proof that all surviving version-chain successors have been spliced away from the RID.

## R-033 — READ COMMITTED retry correctness

A statement attempt tracks `has_persistent_statement_writes`. Same-TxnId internal retry with a fresh snapshot is legal only while that flag is false.

If a retry-requiring conflict appears after any attempt write was installed, v1 aborts the transaction. Any automatic autocommit rerun must use a new TxnId.

This preserves no-physical-user-DML-undo and avoids silently committing writes from an abandoned statement attempt.

## R-034 — terminal publication linearization

Runtime terminal outcome, terminal state, and removal from future snapshot-active membership linearize together under the transaction-registry synchronization.

Status lookup checks terminal runtime outcome before active membership, and transaction logical locks are released only after terminal publication. A new writer therefore cannot acquire a released lock while still seeing the prior owner as `IN_PROGRESS`.

## R-035 — FPI retention invariant

Every WAL-protected clean -> dirty interval begins with a complete page image. Therefore:

```text
rec_lsn = full-image LSN for the current dirty interval
```

The post-checkpoint FPI rule remains as an additional full-image trigger. A checkpoint DPT's minimum recLSN therefore retains at least one complete reconstruction base for every dirty page, even if a later data-page write tears.

## Phase-1/devlog compatibility

No Phase-1 fact is rewritten by these decisions. The accepted audit already states that:

- no slot reuse is implemented,
- checksums remain in pre-recovery staging,
- BufferPool/dirty management/WAL/recovery/vacuum are deferred,
- raw PageFile allocation is append-first and ordinary allocation does not sync every extension,
- HeapFile/FSM relation-wide integration waits on the BufferPool boundary.

The new rules define those later layers rather than retroactively claiming Phase-1 implemented them. No devlog was edited.

## Final status

```text
R-025 RESOLVED
R-026 RESOLVED
R-027 RESOLVED
R-028 RESOLVED
R-029 RESOLVED
R-030 RESOLVED
R-031 RESOLVED
R-032 RESOLVED
R-033 RESOLVED
R-034 RESOLVED
R-035 RESOLVED
```
