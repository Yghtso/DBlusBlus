# DBlusBlus — Project State

This document summarizes implementation reality: implemented capabilities, material
limitations, active authorization boundaries, and known differences from the architecture
contract. [`ARCHITECTURE.md`](ARCHITECTURE.md) is authoritative for intended behavior;
[`DEVELOPMENT.md`](DEVELOPMENT.md) owns implementation sequencing, and
[`VERIFICATION.md`](VERIFICATION.md) owns detailed verification procedures.

## Implemented storage foundation

### Common primitives and encoding

The implementation defines fixed-width storage identifiers and sentinels:

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

`PageId` and `Rid` represent persistent page identity and physical tuple-version identity.
`CommandId{0}` is valid; `TxnId{0}` is the invalid transaction sentinel.

Byte-oriented codecs provide fixed-width little-endian encoding and decoding without
persisting native C++ object layout. The persisted `PageId` and `Rid` encodings are 12 and
16 bytes respectively. RID encoding writes two reserved bytes as zero, and decoding
rejects either reserved byte when nonzero.

A portable CRC-32C/Castagnoli primitive is available. It is used by the file-superblock
codec; ordinary mutable data-page checksum integration depends on the durability layer and
is not implemented.

### Disk I/O

`DiskManager` provides Linux/POSIX fixed-position page I/O and process-local file
registration:

- exclusive file creation, existing-file open, and close;
- aligned file-size and page-count inspection;
- complete `pread` and `pwrite` page transfers with `EINTR` handling;
- rejection of missing pages, short reads, misaligned files, and unrepresentable offsets;
- explicit append extension by one page;
- `fdatasync` file synchronization.

Same-file extension is serialized, so concurrent extension callers receive unique,
contiguous page numbers. Ordinary writes do not allocate or extend files.
`DiskManager` does not interpret page formats or tuple contents.

The file registry is not a general concurrent lifecycle manager: registration and closure
must not race active I/O. Only the append-extension path has explicit internal
serialization.

### File superblocks and managed page files

The generic `FileSuperblock` codec implements the 72-byte common format for `HEAP`, `FSM`,
and `CATALOG` files, including the 8192-byte page image, magic, file kind, page size,
logical identities, creation epoch, CRC32C, and required-zero reserved/trailing bytes.

`FileKind::BTREE` and its numeric code are recognized, but the generic encoder and decoder
reject it. The architecture-defined specialized 128-byte B+ tree superblock codec is not
implemented.

`PageFile` provides:

- create/open around a validated page-zero superblock;
- expected file-kind, `FileId`, and optional object-id checks;
- append-first ordinary page allocation beginning at page 1;
- move-only ownership of one `DiskManager` file registration.

`PageFile` stores a non-owning `DiskManager` pointer. The manager must outlive the
`PageFile`, external code must leave the registration under `PageFile` control, destruction
closes the registration, and move operations transfer cleanup responsibility. Page
allocation is page-type agnostic.

### Page representation

`Page` owns one `PageId` and one contiguous, deterministically initialized 8192-byte byte
buffer. It encodes, decodes, and validates the 32-byte common page header against expected
page type and page number. Page-format controllers operate over this caller-owned buffer
instead of allocating a second page-sized representation.

### Heap pages

The heap implementation separates persisted format ownership from page-local mutation:

- `heap_page_format.*` owns heap-header geometry, slot-state codes, and explicit
  header/slot codecs;
- `heap_page.*` provides `HeapPage` initialization, validation, insertion, tuple-byte
  access, `NORMAL -> DEAD` transition, and compaction.

`HEAP_DATA` format version 1 uses:

```text
common page header       32 bytes
heap-specific header     16 bytes
total heap header        48 bytes
slot entry                8 bytes
page size              8192 bytes
```

Initialization writes common flags and reserved fields as zero and rejects page 0 and
`INVALID_PAGE_NO`. Page-local validation checks common identity/version/geometry,
required-zero fields, explicit slot-state codes, NORMAL tuple bounds, and canonical
`UNUSED` slots. The free-slot chain must be valid, acyclic, and contain every `UNUSED`
slot exactly once.

Insertion stores opaque tuple bytes in one page, allocates a stable new slot, and returns a
physical `Rid`. The maximum raw inline tuple is 8135 bytes. Slot reuse is not implemented,
so `UNUSED` validation exists without an active writer transition that creates reusable
slots.

`MarkDead` performs only the physical `NORMAL -> DEAD` transition. It does not decide MVCC
visibility, transaction outcome, vacuum eligibility, or global death. `Compact` reclaims
payload bytes from `DEAD` slots while preserving every `SlotId` and every NORMAL payload;
reclaimed DEAD slots use zero tuple coordinates and are non-reusable.

### Tuple physical representation

The tuple layer implements:

- an exact 48-byte tuple header with version metadata, flags, schema version, and
  reserved-zero validation;
- schema-directed physical layouts for `BOOLEAN`, `INT32`, `INT64`, `FLOAT64`, `DATE`,
  `TIMESTAMP`, and `VARCHAR`;
- one LSB-first null bit per physical column;
- tightly packed fixed fields without native alignment padding;
- 8-byte VARCHAR descriptors and packed inline payloads;
- checked tuple-size planning, construction, validation, and typed decoding.

The persisted tuple flags are `HAS_NULLS = 0x0001` and `HAS_VARLEN = 0x0002`. Validation
enforces their relationship to the schema and null bitmap, rejects nonzero unused bitmap
bits, rejects NULL in non-nullable columns, and requires present BOOLEAN values to be
exactly `0x00` or `0x01`.

Fixed-width signed values use explicit two's-complement bytes. FLOAT64 preserves the exact
IEEE-754 binary64 payload and is guarded by compile-time representation checks. DATE and
TIMESTAMP are raw signed 32-bit and 64-bit physical scalars. `PhysicalType` is in-memory
layout metadata; its enum values are not persisted type codes.

VARCHAR payloads are inline and packed in schema order without gaps. NULL VARCHAR uses
descriptor `(0,0)`, an empty present value is distinct from NULL, trailing
unreferenced bytes are rejected, and decoded VARCHAR values borrow from the tuple bytes.
All encoded tuples are subject to the 8135-byte inline limit.

Tuple headers carry MVCC/version-chain fields, but transaction visibility, version-chain
coordination, and transaction outcome semantics are not implemented.

### Free-space metadata

`FsmPage` implements flat `FSM_DATA` format version 1. It provides deterministic
heap-page-to-FSM-entry mapping, category conversion, page initialization, structural
validation, and one-byte category access/update.

The page has a 48-byte total header and 8144 category entries. Validation enforces page
identity, deterministic represented heap range, initialized-prefix bounds, zero common and
FSM flags/reserved fields, and a zero uninitialized suffix. Every byte value `0..255` is a
valid category.

FSM categories conservatively approximate tuple insertion capacity after accounting for a
new 8-byte slot. FSM metadata is advisory; validated heap-page geometry is authoritative
for insertion.

`FsmCandidateIndex` is a relation-local, rebuildable in-memory accelerator with 256 ordered
category buckets and reverse PageNo membership. It chooses the smallest PageNo in the
smallest sufficient category, performs no I/O, and is not thread-safe.

## Persisted-format implementation summary

This section is a compact implementation inventory; `ARCHITECTURE.md` owns the complete
format contracts.

```text
PAGE_SIZE                              8192

CommonPageHeader                         32 bytes
Generic FileSuperblock header            72 bytes
BTREE FileSuperblock header             128 bytes; specialized codec absent

Heap page total header                   48 bytes
Heap slot entry                           8 bytes
Maximum raw inline tuple                8135 bytes

TupleHeader                               48 bytes
VARCHAR descriptor                         8 bytes

FSM_DATA total header                     48 bytes
FSM entries/page                        8144
FSM entry width                             1 byte

PageId persisted size                     12 bytes
Rid persisted size                        16 bytes
```

## Source organization and responsibility boundaries

Production storage code is organized by implemented responsibility:

```text
src/
  common/
  storage/
    disk/
    file/
    page/
    heap/
    tuple/
```

The principal boundaries are:

- `DiskManager` owns raw positional file I/O and process-local file registration.
- `PageFile` owns page-file lifecycle, superblock validation, and append-first allocation.
- `Page` owns one in-memory page buffer and its persistent identity.
- `HeapPage` owns physical mechanics for one heap page, not SQL or visibility semantics.
- `TuplePhysicalLayout` and `TupleCodec` own schema-directed tuple bytes and perform no
  storage I/O.
- `FsmPage` owns one persisted FSM page and performs no file I/O.
- `FsmCandidateIndex` owns rebuildable runtime candidate metadata and does not inspect live
  heap pages.

## Verification-relevant implementation properties

The storage foundation has focused tests for exact encodings, boundary and corruption
rejection, persistence/reopen behavior, page allocation, heap insertion and reclamation,
tuple/FSM model properties, borrowed VARCHAR lifetime, and concurrent same-file extension.
Detailed verification obligations and procedures belong in
[`VERIFICATION.md`](VERIFICATION.md); run-specific results do not belong in this state
document.

## Architecture and implementation consistency

The architecture contract is clear for the implemented storage area, but two implementation
mismatches are present:

1. The generic `FileSuperblock` encoder and decoder preserve nonzero common `flags`, and
   tests exercise that behavior. `ARCHITECTURE.md` §4.10.4 assigns no v1 superblock flag
   bits and requires writers and readers to enforce zero.
2. `HeapPage::Validate` does not provide the complete HEAP owner validation required by
   `ARCHITECTURE.md` §§4.13.3 and 5.21. It accepts `REDIRECT_RESERVED`, does not validate
   retained DEAD ranges and `aux` canonically, and does not enforce pairwise
   NORMAL/DEAD range non-overlap or schema-directed tuple validity. `Compact` performs
   additional DEAD-range and overlap checks before mutation, but that is not equivalent to
   ordinary owner validation.

These are implementation defects against settled architecture rules, not unresolved
architecture questions. They require implementation repair rather than an architecture
revision.

The generic BTREE superblock guard is not a noncanonical substitute: B+ tree support and
its specialized codec are absent.

## Unimplemented capabilities and material limitations

### Buffer management

Buffer management is not implemented. The repository contains no `BufferPool`,
`BufferFrame`, frame table, pin/unpin ownership, page guards, CLOCK replacement, buffered
dirty-page state, page-latch integration, eviction, or dirty flushing.

Buffer management is an explicitly authorization-gated implementation boundary. It must
not be implemented without explicit user authorization.

### Relation-wide storage

`HeapFile` is not implemented because relation-wide page access and borrowed-view lifetime
depend on BufferPool-managed page ownership. There is no relation-wide `FreeSpaceMap` owner,
automatic FSM loading/updating/repair around heap operations, or guard-backed lifetime for
tuple/VARCHAR views into buffered pages.

The page-local `FsmPage` validator cannot cross-check its initialized range against a paired
heap file's published page count because the relation-wide owner does not exist.

Physical slot reuse and relation-wide vacuum/reclamation coordination are absent. DEAD
slots retain physical identities and are not linked into the reusable free-slot chain.

### Indexing

No B+ tree implementation exists. The repository has architecture-defined B+ page-type
and file-kind codes, but no B+ page controllers, key codec, tree search/mutation,
latch-coupling logic, free-page management, or specialized 128-byte superblock codec.

### Transactions and durability

There is no transaction manager, snapshot or MVCC visibility implementation,
transaction-level lock manager, write-conflict handling, WAL, group commit, recovery,
checkpointing, transaction-status storage, or vacuum eligibility logic.

The tuple header's transaction fields and the common page header's `page_lsn` field are
physical format foundations only. They do not provide transaction or durability behavior.

### Catalog and SQL

There is no catalog subsystem, catalog cache, SQL lexer/parser, binder, logical type
semantics, or logical planner. Generic `CATALOG` superblock support is only a file-format
primitive.

### Execution and optimization

There is no physical execution engine, vector/data-chunk runtime, expression executor,
pipeline scheduler, query-memory/spill layer, statistics subsystem, cardinality estimator,
cost model, physical optimizer, or parallel execution runtime.

## Document maintenance

Update this document when its canonical description of implementation reality becomes
false or incomplete. Replace stale descriptions rather than appending history; historical
task information belongs in `devlog/`.
