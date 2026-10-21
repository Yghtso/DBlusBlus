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

A portable CRC-32C/Castagnoli primitive is available. The file-superblock codec uses it, and
`page_checksum.*` applies the §4.12 random-access page rule (bytes 16..19 treated as zero,
stored little-endian). BufferPool verifies that checksum before resident publication and
finalizes it in each private flush image. Full-page WAL images do not exist, so their
checksum handling is not implemented.

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
matching is owned by `PageFile::Open`. `ARCHITECTURE.md` §4.10 owns its format.
The common `flags` field is required zero: the encoder always writes zero, the decoder
rejects any nonzero value, and the in-memory `FileSuperblock` has no flags member to carry
or round-trip. Decode ordering has the mismatch described below.

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
`ARCHITECTURE.md` §§5.3–5.6.

Initialization writes common flags and reserved fields as zero and rejects page 0 and
`INVALID_PAGE_NO`.

A `HeapPage` is constructed over caller-owned page bytes together with a
`HeapTupleSchemaHistory`: a non-owning view of the relation's immutable historical
`TuplePhysicalLayout`s, resolved by each tuple's `schema_version` (`SchemaVer{0}` never
resolves). `HeapPage` hands retained tuple bytes to the tuple codec under the resolved layout
and does not interpret column types itself. No catalog exists, so callers supply the schema
history directly.

`Validate` implements the complete page-local HEAP owner validation of `ARCHITECTURE.md`
§4.13.3, in a deterministic order:

- common identity/version/geometry and required-zero header fields;
- canonical slot descriptors: `UNUSED` with zero coordinates, `NORMAL` and `DEAD` with
  `aux = 0`, `NORMAL` nonempty, `DEAD` either retained (nonzero coordinates) or reclaimed
  (`(0,0)`), and `REDIRECT_RESERVED` rejected as `UNSUPPORTED_RESERVED_STATE`;
- every retained `NORMAL` or `DEAD` range nonempty and inside `[upper, PAGE_SIZE)`, with
  pairwise non-overlap checked by sorting ranges on `(start, end, SlotId)`;
- every retained tuple, including retained `DEAD` payloads, passing schema-directed tuple
  validation under its historical layout;
- the bounded tuple-header domain checks that need no status lookup: `xmin` is
  `FROZEN_TXN_ID` or a normal TxnId, `xmax` is not `FROZEN_TXN_ID`, no deleter implies
  `cmax = 0`, and a present previous-version pointer names an ordinary page `>= 1`, is not
  the tuple's own RID, and stays within `slot_count` when it targets the same page;
- the complete free-slot chain: valid, acyclic, and containing every `UNUSED` slot exactly
  once.

Cross-page previous-version targets and version-chain acyclicity are L2/L3 checks and are
not performed by the page-local validator.

`HeapPage` operations (`Insert`, `MarkDead`, `Compact`, `TupleBytes`) work on a page that already
passed `Validate`, which the BufferPool runs before a page becomes resident (§4.13.1). They first
run `ValidateStructure`: every check above except decoding the stored tuples, at O(slots) cost,
which also keeps their byte writes inside the page. A rejected operation leaves the page
unchanged. Insertion additionally validates the new tuple bytes under the full tuple rules, as
if already stored in its new slot, before any mutation; `MarkDead` and `Compact` never change
tuple bytes. Each mutation therefore keeps the page valid under `Validate`. It stores the tuple
in one page, allocates a stable new slot, and returns a physical `Rid`. Insertion enforces the
architecture-defined raw inline tuple bound (§5.6). Slot reuse is not implemented, so `UNUSED` validation exists without an
active writer transition that creates reusable slots.

`MarkDead` performs only the physical `NORMAL -> DEAD` transition. It does not decide MVCC
visibility, transaction outcome, vacuum eligibility, or global death. `Compact` reclaims
payload bytes from `DEAD` slots while preserving every `SlotId` and every NORMAL payload;
reclaimed DEAD slots use the canonical `(0,0)` coordinates with `aux = 0` and are
non-reusable. `Compact` relies on owner validation for range bounds and non-overlap and
rejects pages containing `UNUSED` slots.

### Tuple physical representation

`TuplePhysicalLayout` and `TupleCodec` implement schema-directed layout planning,
construction, validation, and typed decoding for BOOLEAN, INT32, INT64, FLOAT64, DATE,
TIMESTAMP, and VARCHAR. The layout builder accepts caller-supplied `SchemaVer` values,
including zero; `TupleCodec` validates tuple bytes against the selected layout. Neither
provides catalog-backed schema-version domain enforcement. The codecs implement the tuple
layout, flags, null bitmap, scalar, reserved-byte, packed-payload, and inline-size checks
exercised by the storage tests. The standalone codec does not apply the
`ARCHITECTURE.md` §5.7.3 tuple-header domain rules; it accepts the noncanonical pair
`xmax = INVALID_TXN_ID` with nonzero `cmax`.

Signed values use explicit bytes; FLOAT64 retains its binary64 payload with compile-time
representation checks. DATE and TIMESTAMP decoding provides physical scalars, without
implementing SQL calendar/time semantics. `PhysicalType` is in-memory layout metadata;
its enum values are not persisted type codes.

VARCHAR decoding distinguishes NULL from empty present values and returns views borrowed
from the tuple bytes. No relation-wide owner yet ties these views to a BufferPool guard.

Tuple headers carry MVCC/version-chain fields, but transaction visibility, version-chain
coordination, and transaction outcome semantics are not implemented. The structural domain
checks on those fields are applied by heap-page owner validation; the standalone tuple codec
does not apply them.

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

### Buffer management

`BufferPool` (`storage/buffer/`) owns the process-local page table, a fixed array of frames,
pins, page latches, dirty state, CLOCK replacement, eviction, and copied stable writeback. It
is format agnostic; each registered FileId supplies an owner validator.

Frames use the §7.6 ownership states `FREE`, `LOADING`, `RESIDENT`, and `EVICTING` with the
orthogonal I/O substates `NONE`, `READ_IN_PROGRESS`, and `WRITEBACK_IN_PROGRESS`. Each frame
carries a checked pin count, a dirty flag, a monotonic per-frame `modification_generation`,
a monotonic binding epoch, a CLOCK reference bit, and a `std::shared_mutex` page latch.

Implemented behavior:

- one load intent per PageId; concurrent misses join it, and successful publication turns
  every pending claim into one pin; a failed load wakes every joiner with the same error and
  is not cached;
- validation before residency: page checksum, common-header PageNo identity, then the owner
  validator; ordinary fetch rejects page 0 and PageNos at or beyond the published bound;
- move-only `ReadPageGuard` (pin + shared latch) and `WritePageGuard` (pin + exclusive latch);
  guards release the latch before the pin; a write guard publishes a mutation only through
  `MarkDirty()`, and debug builds abort when a guard releases unreported byte changes;
- copied writeback: the image is copied under the read latch, checksummed privately, written
  with `pwrite`, and made stable with `fdatasync`; dirty clears only when the frame still holds
  the copied generation; explicit flush repeats until the current generation is clean;
- CLOCK with second chance; victim reservation (`RESIDENT -> EVICTING`) is the eviction
  linearization point; dirty victims are stably written before reuse; a failed eviction
  writeback restores the old page as pinnable `RESIDENT` and fails the requester; one
  unsuccessful pass returns `NO_REPLACEABLE_FRAME`;
- new pages are built in a reserved frame without reading disk bytes, validated while
  `LOADING`, and published `RESIDENT` and dirty together with the advanced page bound; the
  frame is reserved before the file is extended;
- file retirement (`ACTIVE -> RETIRING -> CLOSED`) and controlled shutdown, both draining
  guards and I/O before flushing or discarding.

Synchronization is one pool mutex for the page table, file registry, frame metadata, and
CLOCK hand, plus per-frame latches. I/O, validation, page initialization, and latch waits run
without the mutex. A page latch may be acquired before the pool mutex but never after it. The
page table is not partitioned.

A test observer interface exposes semantic events (load intent installed, victim reserved,
image copied, and so on) so tests can build deterministic interleavings, and a `PageIo`
interface lets tests inject read, write, short-write, and `fdatasync` faults.

BufferPool limitations:

- there is no WAL, so frames carry no `rec_lsn`, DPT membership, checkpoint/FPI epoch, or MTR
  no-flush state, and the flush path performs no WAL-before-data step; pages keep
  `page_lsn = INVALID_LSN`;
- the WAL-owned error categories and `UNSUPPORTED_FILE_FORMAT` are not produced;
- new-page failure after file extension cannot truncate the appended page. The page stays
  outside the published bound and the next new page for that file reuses its PageNo;
- a closed FileId reports `FILE_OR_PAGE_NOT_FOUND`, because closed registrations are removed;
- there is no background writer, batched `fdatasync`, or bounded/cancellable waiting API;
- the destructor performs no I/O; durable close requires `Shutdown()` or `RetireFile()`.

## Source organization and responsibility boundaries

Production storage code is organized by implemented responsibility:

```text
src/
  common/
  storage/
    buffer/
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
- `BufferPool` owns resident-page caching, pins, latches, replacement, eviction, and dirty-page
  writeback; `HeapPage` and `FsmPage` operate on pages it supplies through guards.
- `HeapPage` owns physical mechanics for one heap page, not SQL or visibility semantics.
- `TuplePhysicalLayout` and `TupleCodec` own schema-directed tuple bytes and perform no
  storage I/O.
- `FsmPage` owns one persisted FSM page and performs no file I/O.
- `FsmCandidateIndex` owns rebuildable runtime candidate metadata and does not inspect live
  heap pages.

## Verification-relevant implementation properties

The storage foundation has focused tests for exact encodings, boundary and corruption
rejection, persistence/reopen behavior, page allocation, heap insertion and reclamation,
tuple/FSM model properties, borrowed VARCHAR lifetime, concurrent same-file extension, and
BufferPool frame transitions, deterministic fetch/eviction/writeback races, fault injection,
retirement, shutdown, and reopen after eviction.
Detailed verification obligations and procedures belong in
[`VERIFICATION.md`](VERIFICATION.md); run-specific results do not belong in this state
document.

## Architecture and implementation consistency

The architecture contract is clear for the implemented storage area, but three
implementation mismatches are present:

1. The standalone `TupleHeader` codec does not enforce `xmax == INVALID_TXN_ID` ⇒
   `cmax == 0` as required by `ARCHITECTURE.md` §5.7.3, and its tests exercise the
   noncanonical pair. Heap-page owner validation rejects that pair for stored tuples, so
   the gap is limited to direct codec use; it is an implementation fact rather than an
   accepted format variant.
2. `DecodeFileSuperblock` checks the version-1 CRC before family/version dispatch and
   classifies version zero as an unsupported format. `ARCHITECTURE.md` §§4.10.4 and
   4.14.2 require dispatch before version-owned checksum/layout validation and distinguish
   corruption from a positive unsupported version. `PageFile::Open` delegates to this
   decoder; it does not repair the ordering or classification.
3. The tuple layout builder accepts caller-supplied `SchemaVer` values, including zero,
   without enforcing the defined writer/domain rule in `ARCHITECTURE.md` §5.13. Tests
   exercise arbitrary values such as `SchemaVer{41}`. `HeapTupleSchemaHistory` never
   resolves `SchemaVer{0}`, but no catalog-backed production schema-version enforcement
   exists.

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

### Relation-wide storage

`HeapFile` is not implemented: no relation-wide owner uses BufferPool guards for page access
or borrowed-view lifetime. There is no relation-wide `FreeSpaceMap` owner,
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
