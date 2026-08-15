# Rewrite Pass 5 — I/O, Buffer Management, and Storage Ownership

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `86..108`
- processed source lines: `3397..4201`
- legacy architecture modified: **no**
- production code modified: **no**
- B+ tree §109+ migrated: **no**

Pass 5 replaces Chapter 7's high-level overview with the canonical I/O/buffer-management contract and migrates cross-cutting storage ownership material into its proper chapters.

## Canonicalized DiskManager contract

DiskManager remains a deliberately raw layer.

It owns file/handle management, fixed-page reads/writes, raw file lifecycle, size discovery, page-file durable synchronization, and explicit extension. It does not interpret tuples, schema, MVCC, indexes, SQL, or plans.

Raw OS-file creation and database-superblock initialization remain separate. POSIX descriptors remain private process-local resources and cannot become persistent FileIds.

## Exact page I/O semantics

The canonical v1 page-file contract now states:

```text
pread / pwrite
positional I/O
exact PAGE_SIZE page transfers
fdatasync page-file synchronization
```

Preserved behavior includes short-read/write handling, EOF distinctions, destination safety on failed reads, EINTR rules, no blind close retry, no implicit allocation/sparse writes, page-size alignment, checked offsets, contextual errors, and no shared-file-offset dependence.

## Canonicalized BufferPool contract

The BufferPool owns:

```text
PageId -> resident frame
```

plus caching, pins, dirty state, page latches, replacement, eviction, flushing, and WAL-before-data coordination. It remains independent of heap tuple and B+ key parsing.

## Frame, guard, pin, dirty, and CLOCK contracts

Pass 5 canonicalizes:

- the baseline frame state and process-local metadata boundary,
- aligned page bytes,
- RAII read/write guards,
- one-pin-per-guard lifetime,
- no reference surviving guard release,
- hash-based `PageId -> FrameId` lookup with later sharding freedom,
- `pin_count > 0` as non-evictable,
- dirty-on-mutation and NO-FORCE behavior,
- centralized WAL-before-data enforcement,
- CLOCK's pinned/reference/victim state machine,
- dirty-victim flush before frame reuse.

Later recovery-era `rec_lsn` behavior is explicitly left for the recovery pass.

## Cross-chapter storage boundaries

Legacy §§96–102 were migrated into their proper owners rather than forced into Chapter 7.

Chapter 2 now defines the storage dependency/ownership stack and preserves the critical rule that `HeapPage` must not call `DiskManager` directly.

Chapter 5 now defines HeapPage as a lightweight view over caller-owned resident bytes, the TupleCodec boundary, zero-copy tuple-view behavior, and the guard-bound lifetime rule.

## Non-architecture material

- §101 source tree: moved out; subsystem boundaries retained.
- §103 Storage Milestone 1: project roadmap/history, not architecture.
- §104 detailed test checklist: architecture-level obligations moved to Chapter 41; detailed recipe remains outside the contract.
- §105 detailed benchmark checklist: benchmark families moved to Chapter 42; recipe remains outside the contract.
- §106 deferred storage features: consolidated into Appendix C.
- §107 storage-decision summary: deduplicated against canonical chapters.
- §108 next architecture topic: project roadmap/history, not architecture.

## Issue register

R-012 records the refinement from the early conceptual `DiskManager::SyncWal()` sketch to later dedicated WAL-writer ownership.

R-013 records that §104's old “reusable slots” milestone test is stale relative to the later delayed/safe RID-reuse architecture.

Neither requires a new architecture decision.

## Coverage result

```text
legacy §§0..108     migrated / explicitly disposed
legacy §§109..725   pending
```

All 23 Pass-5 sections are complete. No B+ tree section was marked migrated.

## Validation

Mechanical validation confirmed:

- pinned legacy SHA-256 unchanged,
- §86 starts at line 3397,
- §109 starts at line 4202,
- every source section 86..108 exists,
- all coverage rows through §108 are non-PENDING,
- every row from §109 onward remains PENDING,
- Chapter 7 occurs exactly once,
- required I/O/EINTR/pin/dirty/WAL/CLOCK rules exist,
- source-layout/milestone/next-topic prose is absent from the technical Chapter 7,
- legacy `ARCHITECTURE.md` was not modified,
- production code was not modified.

## Pass 5 exit status

**COMPLETE.**

Pass 6 should process only legacy §§109–179 and replace Chapter 8 with the canonical B+ tree contract while separating implementation milestones, detailed verification recipes, benchmark procedures, and historical next-stage material from the technical architecture.
