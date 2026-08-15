# DBlusBlus — Project State

Last updated: 2026-08-15  
Current checkpoint: Milestone `0022` — HEAP_DATA/FSM_DATA Zero-Only Common Flags
Architecture checkpoint: `ARCHITECTURE.md` is the authoritative v1 architecture contract

---

## 1. Current status

Phase 1 raw storage is complete at the BufferPool boundary.

The Phase 1 audit found all currently required BufferPool-independent storage primitives present. One tuple-validation bug was found and fixed during the audit, and no remaining correctness issue was found that blocks the next architectural phase.

Phase 2 buffer management has not started.

`HeapFile` is intentionally not implemented yet because its relation-wide page access and page-lifetime model depend on the future BufferPool.

---

## 2. Phase status

| Phase | Status |
|---|---|
| Phase 1 — Raw storage | Complete |
| Phase 2 — Buffer management | Not started |
| Phase 3 — Indexes | Not started |
| Transactions / MVCC / WAL / recovery | Not started |
| Catalog / SQL front end | Not started |
| Execution engine | Not started |
| Optimizer / statistics | Not started |

The next architectural dependency is Phase 2 buffer management.

---

## 3. Implemented storage foundation

### Identifiers and sentinels — `0001`

Implemented fixed-width identifier aliases:

```text
FileId      uint32
PageNo      uint64
SlotId      uint16
TxnId       uint64
CommandId   uint32
Lsn         uint64
TableId     uint64
IndexId     uint64
SchemaVer   uint32
```

Implemented the architecture-defined sentinels together with `PageId` and `Rid`.

Important current semantics:

- `Rid` identifies a physical heap tuple version.
- `CommandId{0}` is valid.
- `TxnId{0}` is the invalid/no-transaction sentinel.
- `PageId` identifies persistent page identity independently from any future buffer frame.

### Little-endian encoding — `0002`

Implemented fixed-width little-endian encoding and decoding for signed and unsigned 8-, 16-, 32-, and 64-bit integers.

The encoding layer is byte-oriented, safe for unaligned fields, and does not persist native C++ object layout.

### PageId and RID codecs — `0003`

Persisted sizes:

```text
PageId   12 bytes
Rid      16 bytes
```

RID layout:

```text
0..3     page.file_id
4..11    page.page_no
12..13   slot
14..15   reserved
```

The encoder writes the RID reserved bytes as zero, and the decoder rejects any encoding where
either reserved byte is nonzero.

### Common page header — `0004`

Implemented the explicit 32-byte common page header and the persisted page-type codes used by the current storage formats.

### CRC32C — `0005`

Implemented a portable CRC-32C/Castagnoli primitive over byte spans.

Whole-page checksum policy for ordinary mutable data pages remains staged until WAL/recovery integration.

### File superblock — `0006`

Implemented the persistent 8192-byte file superblock with:

- common page header,
- file magic,
- file kind,
- page size,
- file identity,
- object identity,
- creation epoch,
- reserved-zero validation,
- CRC32C validation.

### DiskManager — `0007`

Implemented Linux/POSIX random-access page I/O with:

- file creation/open/close,
- process-local FileId registration,
- `pread` / `pwrite`,
- exact page reads and writes,
- page-aligned size validation,
- append extension,
- checked offsets,
- `fdatasync`.

`DiskManager` remains independent of page-type and tuple semantics.

### Raw Page abstraction — `0008`

Implemented `Page` as one `PageId` plus one owned 8192-byte byte buffer.

Page-type-specific controllers operate over this byte storage rather than owning additional page-sized buffers.

### PageFile — `0009`

Implemented the page-file lifecycle above `DiskManager`:

- create/open,
- superblock initialization and validation,
- file identity checks,
- append-first ordinary page allocation,
- move-only lifecycle ownership.

Page allocation remains page-type agnostic.

---

## 4. Heap-page implementation

### Heap page structure — `0010`

Implemented `HEAP_DATA` format version 1.

`HeapPage` initialization always writes common-header flags as zero, and validation rejects any
persisted HEAP_DATA page with nonzero common flags.

Current physical geometry:

```text
common page header       32 bytes
heap-specific header     16 bytes
total heap header        48 bytes
slot entry                8 bytes
page size              8192 bytes
```

Implemented explicit slot states, slot-directory encoding, deterministic blank-page initialization, and structural validation.

### Raw insertion — `0011`

Implemented insertion of opaque tuple bytes into a single heap page.

Current raw tuple maximum:

```text
8135 bytes
```

Insertion creates a new stable slot and returns a physical `Rid`.

No slot reuse is currently implemented.

### NORMAL -> DEAD transition — `0012`

Implemented the physical page-local transition:

```text
NORMAL -> DEAD
```

The page layer does not decide MVCC visibility, global death, vacuum eligibility, or transaction outcome.

Marking a slot DEAD does not immediately reclaim its tuple bytes.

### Heap compaction — `0013`

Implemented deterministic page-local compaction of already-DEAD tuple payloads.

Compaction:

- preserves every SlotId,
- preserves NORMAL tuple contents,
- repacks NORMAL payloads,
- reclaims DEAD payload bytes,
- leaves DEAD slots non-reusable,
- canonicalizes reclaimed DEAD tuple coordinates to zero,
- validates overlap and page geometry before mutation.

---

## 5. Tuple storage implementation

### Tuple header — `0014`

Implemented the exact 48-byte physical tuple header.

Current tuple flags:

```text
HAS_NULLS  = 0x0001
HAS_VARLEN = 0x0002
```

The tuple header contains MVCC/version-chain metadata fields but visibility and transaction semantics are not implemented yet.

### Physical tuple layout — `0015`

Implemented schema-directed physical layout metadata for:

```text
BOOLEAN
INT32
INT64
FLOAT64
DATE
TIMESTAMP
VARCHAR
```

Implemented:

- one null bit per physical column,
- LSB-first bitmap order,
- tightly packed fixed-area offsets,
- fixed 8-byte VARCHAR descriptor slots,
- checked tuple-size planning,
- schema-version metadata.

`PhysicalType` currently exists only as in-memory layout metadata; no persisted numeric type codes were introduced.

### Fixed-width tuple codec — `0016`

Implemented fixed scalar storage for:

- BOOLEAN,
- INT32,
- INT64,
- FLOAT64,
- DATE,
- TIMESTAMP,
- NULL.

Current physical rules include:

- BOOLEAN is exactly `0x00` or `0x01`,
- signed integers use fixed-width two's-complement representation,
- FLOAT64 preserves the exact IEEE-754 binary64 payload bits,
- DATE is currently a raw signed 32-bit physical scalar,
- TIMESTAMP is currently a raw signed 64-bit physical scalar,
- fixed bytes reserved for NULL values are written as zero,
- `HAS_NULLS` exactly reflects the used null-bitmap bits.

### General VARCHAR tuple codec — `0017`

Implemented mixed fixed-width/VARCHAR tuple construction, validation, and decoding.

VARCHAR descriptor:

```text
uint32 payload_offset
uint32 payload_length
```

Current rules:

- offsets are relative to tuple byte zero,
- NULL VARCHAR descriptor is `(0,0)`,
- present VARCHAR payloads are packed in physical schema order without gaps,
- empty non-NULL VARCHAR remains distinct from NULL,
- `HAS_VARLEN` is set when the physical layout contains at least one VARCHAR column,
- physical tuple length is exact,
- trailing unreferenced bytes are rejected,
- VARCHAR decode returns a non-owning view into the tuple bytes.

All current tuples remain inline and subject to the 8135-byte raw tuple limit.

---

## 6. Free-space management

### Persisted FSM page format — `0018`

Implemented flat `FSM_DATA` format version 1.

`FsmPage` initialization always writes common-header flags as zero, and validation rejects any
persisted FSM_DATA page with nonzero common flags.

Physical layout:

```text
common page header       32 bytes
FSM-specific header      16 bytes
total header             48 bytes
category entries       8144 bytes
```

Each initialized entry stores one byte in the range `0..255`.

The category represents approximate tuple insertion capacity after conservatively accounting for the current new-slot cost.

The FSM is advisory. Actual heap-page free space remains the source of truth for insertion.

### In-memory FSM candidate index — `0019`

Implemented a relation-local, rebuildable candidate index using:

```text
256 ordered category buckets
reverse PageNo -> category membership
```

Current candidate policy:

1. determine the minimum sufficient category,
2. search upward through the categories,
3. select the first nonempty category,
4. choose the smallest PageNo within that category.

The candidate index performs no disk or page I/O.

It is currently unsynchronized and not thread-safe.

---

## 7. Phase 1 audit — `0020`

The complete BufferPool-independent Phase 1 storage implementation was audited against the current architecture.

Areas reviewed included:

- persisted byte layouts,
- identifier and sentinel use,
- checked arithmetic,
- corruption validation,
- failure atomicity,
- ABI-independent serialization,
- allocation behavior,
- borrowed-view lifetimes,
- storage-layer dependencies,
- the Phase 1 / Phase 2 boundary.

### Production correctness fix

The audit found one bug:

`ValidateTuple` accepted malformed present BOOLEAN values such as `0x02` unless that column was later decoded explicitly.

The validator was corrected so malformed present BOOLEAN bytes are rejected during whole-tuple validation.

### FLOAT64 portability hardening

A compile-time IEEE-754 capability check was added so a target with an incompatible `double` representation cannot silently violate the persisted FLOAT64 contract.

### Property testing

A deterministic 2,000-iteration tuple property test was added covering mixed schemas and values, canonical re-encoding, NULLs, empty VARCHAR values, opaque VARCHAR bytes, and exact FLOAT64 payload-bit comparisons.

### Audit classification

```text
A. COMPLETE AT BUFFERPOOL BOUNDARY
```

No additional BufferPool-independent primitive required before the future `HeapFile` was found.

---

## 8. Current persisted-format summary

This is a convenience summary. `ARCHITECTURE.md` remains the detailed format specification.

```text
PAGE_SIZE                         8192

CommonPageHeader                    32 bytes
FileSuperblock common prefix        72 bytes
BTREE FileSuperblock header        128 bytes required by architecture; current codec mismatch (§11)

Heap page total header              48 bytes
Heap slot entry                       8 bytes
Maximum raw inline tuple           8135 bytes

TupleHeader                          48 bytes
VARCHAR descriptor                    8 bytes

FSM_DATA total header                48 bytes
FSM entries/page                   8144
FSM entry width                        1 byte

PageId persisted size                12 bytes
Rid persisted size                   16 bytes
```

---

## 9. Current storage-layer boundaries

### DiskManager

Owns low-level positional file I/O and process-local file descriptor registration.

It does not interpret page formats or tuple contents.

### PageFile

Owns page-file lifecycle, file-superblock validation, and append-first page allocation.

It does not own buffered page lifetime or replacement policy.

### Page

Owns one in-memory database-page byte buffer and its persistent `PageId`.

### HeapPage

Operates on one heap page.

It owns page-local physical validation and mutation, but not SQL semantics, MVCC visibility, or global reclamation policy.

### TuplePhysicalLayout / TupleCodec

Own schema-directed physical tuple layout and serialization.

They perform no storage I/O.

### FsmPage

Operates on one persisted FSM page.

It performs no file I/O.

### FsmCandidateIndex

Owns rebuildable runtime candidate metadata.

It performs no file I/O and does not inspect live heap pages.

### HeapFile

Not implemented yet.

Its relation-wide page access and lifetime model depends on BufferPool integration.

---

## 10. Current test checkpoint

Current verified results:

```text
Clang Debug            189 / 189 passed
Clang ASan + UBSan     189 / 189 passed
GCC Debug              189 / 189 passed
clang-tidy             clean
clang-format           clean
git diff --check       clean
project warnings       none
```

LeakSanitizer was disabled in the ptrace execution environment. AddressSanitizer and UndefinedBehaviorSanitizer remained enabled and reported no errors.

No Phase 1 storage benchmark suite exists yet beyond the generic benchmark smoke target.

---

## 11. Architecture / implementation consistency items

The currently verified mismatch set in implemented code contains two categories:

1. BTREE FileSuperblock codec,
2. heap UNUSED/free-list validation.

These are implementation mismatches against settled architecture contracts. Unimplemented later subsystems remain deferred work rather than mismatches.

### BTREE FileSuperblock codec

The accepted v1 architecture requires:

```text
BTREE FileSuperblock:
    header_size = 128
    bytes 72..127 = canonical fixed BTREE extension
```

The current generic `FileSuperblock` codec recognizes `FileKind::BTREE`, but emits and accepts only the generic 72-byte header and requires bytes after byte 71 to be zero.

This is an **implementation mismatch**, not an open architecture question. The persisted BTREE superblock format remains settled by `ARCHITECTURE.md` §§4.10 and 8.2.1.

No code change has yet reconciled this mismatch. A later implementation fix must either:

- dispatch `FileKind::BTREE` to a codec for the canonical specialized 128-byte superblock, or
- reject `FileKind::BTREE` in the generic codec until that specialized support exists.

The current BTREE implementation remains deferred; this consistency item does not start index or BufferPool implementation.

### Heap UNUSED/free-list validation

The accepted v1 architecture requires canonical zero tuple coordinates for `UNUSED` slots, `aux` next-free links, a valid-or-sentinel `free_slot_head`, an in-range acyclic list containing every reusable `UNUSED` slot exactly once, and no `NORMAL` or `DEAD` list members, as specified by `ARCHITECTURE.md` §§5.3.2, 5.4.2, and 5.21.

The current `HeapPage::Validate` checks `free_slot_head` only in limited cases and primarily applies tuple coordinate/range checks to `NORMAL` slots. It can accept an `UNUSED` slot with nonzero tuple coordinates that is not represented correctly in the free list.

This is an **implementation mismatch**, not an open architecture question and not evidence against the Chapter-14 reclamation contract. Immediate DEAD-slot reuse remains forbidden; delayed `DEAD -> UNUSED` reuse remains governed by Chapter 14.

No code strategy is selected here. A later implementation task must either fully validate the canonical `UNUSED`/free-list invariants or reject persisted `UNUSED`/free-list states that the implementation cannot yet validate safely.

---

## 12. Deferred work

### Buffer management

Not implemented:

- BufferFrame,
- frame table,
- pin/unpin ownership,
- page guards,
- CLOCK replacement,
- dirty-page management,
- buffer-aware page latching,
- dirty flushing.

### Relation-wide storage

Deferred until the required buffer-management layer exists:

- HeapFile,
- relation-wide FreeSpaceMap owner,
- automatic FSM loading/updating/repair around live heap-page operations,
- guarded lifetime for tuple/VARCHAR views backed by buffered pages.

### Later database subsystems

Not implemented:

- B+ tree,
- transactions,
- MVCC visibility,
- write-conflict handling,
- WAL,
- recovery,
- vacuum eligibility,
- delayed physical RID-slot reuse,
- catalog,
- SQL parser/binder/type semantics,
- execution engine,
- statistics,
- optimizer,
- parallel execution.

---

## 13. Next project transition

The next architectural phase is:

```text
Phase 2 — Buffer Management
```

Phase 2 has not started.

The first work in that phase will establish buffered page ownership and lifetime management before relation-wide `HeapFile` operations are introduced.

---

## 14. Milestone index

```text
0001  Identifiers and sentinels
0002  Fixed-width little-endian encoding
0003  PageId / RID persisted codecs
0004  Common page header
0005  CRC32C primitive
0006  File superblock
0007  DiskManager POSIX I/O
0008  Raw Page abstraction
0009  PageFile lifecycle and allocation
0010  Heap page structure
0011  Heap page raw insertion
0012  Heap page NORMAL -> DEAD transition
0013  Heap page compaction
0014  TupleHeader codec
0015  Tuple physical layout
0016  Fixed-width tuple codec
0017  General VARCHAR / varlen tuple codec
0018  Persisted FSM page format
0019  In-memory FSM candidate index
0020  Phase 1 raw-storage completion and hardening audit
0021  Persistent RID reserved-byte decoder enforcement
0022  HEAP_DATA/FSM_DATA zero-only common flags
```

Detailed milestone history remains in `devlog/`.

---

## 15. Document maintenance

`PROJECT_STATE.md` summarizes the current implementation state.

It should be updated when the accepted project state changes materially, especially when:

- a milestone completes a subsystem,
- a phase boundary is crossed,
- a major architectural dependency is resolved,
- an important deferred item becomes implemented,
- an architecture/implementation consistency item is resolved or introduced.

Historical detail should remain in the numbered devlogs rather than accumulating indefinitely in this file.
