# DBlusBlus — Project State

This document summarizes implementation reality: implemented capabilities, material
limitations, active authorization boundaries, and known differences from the architecture
contract. [`ARCHITECTURE.md`](ARCHITECTURE.md) is authoritative for intended behavior;
[`DEVELOPMENT.md`](DEVELOPMENT.md) owns implementation sequencing, and
[`VERIFICATION.md`](VERIFICATION.md) owns detailed verification procedures.

## Implemented storage foundation

### Common primitives and encoding

The common layer implements fixed-width storage identifiers, `PageId`, `Rid`, sentinels,
and checked byte-oriented codecs. Identifier domains and persisted encodings are owned by
`ARCHITECTURE.md` §§4.3–4.5; the codecs use explicit little-endian bytes and validate RID
reserved bytes rather than serializing native C++ object layout.

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

The generic `FileSuperblock` codec implements the common format for `HEAP`, `FSM`, and
`CATALOG` files, decoding logical identity fields and validating checksum, kind, and
reserved/trailing-byte structure. Expected FileId, FileKind, and optional object-id
matching is owned by `PageFile::Open`.
`ARCHITECTURE.md` §4.10 owns its format. Common flag validation has the mismatch described
below.

`FileKind::BTREE` and its numeric code are recognized, but the generic codec rejects it.
The specialized B+ tree superblock codec defined by §8.2.1 is not implemented.

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

`Page` owns one `PageId` and one contiguous, deterministically initialized page buffer.
It encodes, decodes, and validates the §4.8 common page header against expected
page type and page number. Page-format controllers operate over this caller-owned buffer
instead of allocating a second page-sized representation.

### Heap pages

The heap implementation separates persisted format ownership from page-local mutation:

- `heap_page_format.*` owns heap-header geometry, slot-state codes, and explicit
  header/slot codecs;
- `heap_page.*` provides `HeapPage` initialization, validation, insertion, tuple-byte
  access, `NORMAL -> DEAD` transition, and compaction.

The heap page codec implements the `HEAP_DATA` representation owned by
`ARCHITECTURE.md` §§5.3–5.6. The page controller stores opaque tuple bytes and does not
provide schema-directed owner validation.

Initialization writes common flags and reserved fields as zero and rejects page 0 and
`INVALID_PAGE_NO`. Page-local validation checks common identity/version/geometry,
required-zero fields, explicit slot-state codes, NORMAL tuple bounds, and canonical
`UNUSED` slots. The free-slot chain must be valid, acyclic, and contain every `UNUSED`
slot exactly once.

Insertion stores opaque tuple bytes in one page, allocates a stable new slot, and returns a
physical `Rid`. Insertion enforces the architecture-defined raw inline tuple bound (§5.6). Slot reuse is not implemented,
so `UNUSED` validation exists without an active writer transition that creates reusable
slots.

`MarkDead` performs only the physical `NORMAL -> DEAD` transition. It does not decide MVCC
visibility, transaction outcome, vacuum eligibility, or global death. `Compact` reclaims
payload bytes from `DEAD` slots while preserving every `SlotId` and every NORMAL payload;
reclaimed DEAD slots use zero tuple coordinates and are non-reusable.

### Tuple physical representation

`TuplePhysicalLayout` and `TupleCodec` implement schema-directed layout planning,
construction, validation, and typed decoding for BOOLEAN, INT32, INT64, FLOAT64, DATE,
TIMESTAMP, and VARCHAR. The layout builder accepts caller-supplied `SchemaVer` values,
including zero; `TupleCodec` validates tuple bytes against the selected layout. Neither
provides catalog-backed schema-version domain enforcement. The codecs implement the tuple
layout, flags, null bitmap, scalar, reserved-byte, packed-payload, and inline-size checks
exercised by the current storage tests. Full tuple-header conformance to
`ARCHITECTURE.md` §5.7.3 is not implemented:
the codec accepts the noncanonical pair `xmax = INVALID_TXN_ID` with nonzero `cmax`.

Signed values use explicit bytes; FLOAT64 retains its binary64 payload with compile-time
representation checks. DATE and TIMESTAMP decoding provides physical scalars, without
implementing SQL calendar/time semantics. `PhysicalType` is in-memory layout metadata;
its enum values are not persisted type codes.

VARCHAR decoding distinguishes NULL from empty present values and returns views borrowed
from the tuple bytes. No BufferPool guard currently owns these views' lifetime.

Tuple headers carry MVCC/version-chain fields, but transaction visibility, version-chain
coordination, and transaction outcome semantics are not implemented.

### Free-space metadata

`FsmPage` implements flat `FSM_DATA` format version 1. It provides deterministic
heap-page-to-FSM-entry mapping, category conversion, page initialization, structural
validation, and one-byte category access/update.

Validation checks the deterministic represented heap range, initialized-prefix bounds,
identity, flags/reserved fields, and uninitialized suffix against the §6.5–§6.7 format.
Category conversion accounts for a new slot. FSM metadata remains advisory; heap-page
geometry determines insertion capacity under the Chapter-6 contract.

`FsmCandidateIndex` is a relation-local, rebuildable in-memory accelerator with 256 ordered
category buckets and reverse PageNo membership. It chooses the smallest PageNo in the
smallest sufficient category, performs no I/O, and is not thread-safe.

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

The architecture contract is clear for the implemented storage area, but five implementation
mismatches are present:

1. The generic `FileSuperblock` encoder and decoder preserve nonzero common `flags`, and
   tests exercise that behavior. `ARCHITECTURE.md` §4.10.4 assigns no superblock flag
   bits and requires writers and readers to enforce zero.
2. `HeapPage::Validate` does not provide the complete HEAP owner validation required by
   `ARCHITECTURE.md` §§4.13.3 and 5.21. It accepts `REDIRECT_RESERVED`, does not validate
   retained DEAD ranges and `aux` canonically, and does not enforce pairwise
   NORMAL/DEAD range non-overlap or schema-directed tuple validity. `Compact` performs
   additional DEAD-range and overlap checks before mutation, but that is not equivalent to
   ordinary owner validation.
3. `TupleHeader` validation does not enforce `xmax == INVALID_TXN_ID` ⇒ `cmax == 0` as
   required by `ARCHITECTURE.md` §5.7.3. The current test suite intentionally exercises
   the accepted noncanonical pair, so the mismatch is an implementation fact rather than
   an accepted format variant.
4. `DecodeFileSuperblock` checks the version-1 CRC before family/version dispatch and
   classifies version zero as an unsupported format. `ARCHITECTURE.md` §§4.10.4 and
   4.14.2 require dispatch before version-owned checksum/layout validation and distinguish
   corruption from a positive unsupported version. `PageFile::Open` delegates to this
   decoder; it does not repair the ordering or classification.
5. The tuple layout builder accepts caller-supplied `SchemaVer` values, including zero,
   without enforcing the defined writer/domain rule in `ARCHITECTURE.md` §5.13. The
   current tests exercise arbitrary values such as `SchemaVer{41}`; this is generic
   physical-layout support rather than catalog-backed production schema-version
   enforcement.

These are implementation defects against settled architecture rules, not unresolved
architecture questions. They require implementation repair rather than an architecture
revision.

The generic BTREE superblock guard is not a noncanonical substitute: B+ tree support and
its specialized codec are absent.

## Unimplemented capabilities and material limitations

### Database-root lifecycle

Database-root bootstrap/publication, process/control ownership, lifecycle namespace
coordination, whole-root retirement, and restarted reclamation are not implemented.
The existing file-level storage primitives do not provide these database-level
protocols. Their intended contracts are defined in `ARCHITECTURE.md` §§3.3 and
4.7.8–4.7.10; raw file creation and synchronization do not imply database-root
publication or cleanup capability.

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
