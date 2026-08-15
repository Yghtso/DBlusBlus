# DBlusBlus Architecture

**Status:** Rewrite in progress  
**Architecture version:** v1 contract under structural rewrite  
**Active authority during rewrite:** the existing `ARCHITECTURE.md` remains authoritative until this document has completed semantic reconciliation and is explicitly adopted.

## Purpose

This document defines the technical architecture of DBlusBlus: a from-scratch, single-node relational database management system implemented in C++20.

It specifies architectural responsibilities, subsystem boundaries, persistent-format requirements, concurrency and lifetime rules, correctness invariants, and performance-relevant design constraints.

Project progress belongs in `PROJECT_STATE.md`. Historical implementation records belong in `devlog/`. Development-agent instructions do not form part of this architecture contract.

## Contract language

The original architecture uses `LOCKED` to mark hard requirements. During this rewrite those requirements are expressed using ordinary normative language:

- **MUST / MUST NOT** — required by the current architecture contract.
- **SHOULD / SHOULD NOT** — the architectural default; deviation requires a concrete technical reason and must remain compatible with all MUST-level requirements.
- **MAY** — explicitly permitted implementation freedom.
- **Deferred** — intentionally outside the current baseline; it is not part of the required v1 implementation unless later promoted by an explicit architecture revision.

A rewrite pass may reorganize, consolidate, or clarify existing requirements, but it does not change their meaning. Deliberate architecture changes are handled separately from structural rewriting.

---

# Part I — Foundations

# 1. Scope and Design Goals

## 1.1 System goal

DBlusBlus is a serious single-node relational database intended to expose and implement the core mechanisms of a relational engine rather than delegate them to external database frameworks.

The architecture is intended to support, end to end:

- persistent page-oriented storage,
- row-oriented heap tables,
- B+ tree indexes,
- transactions and multi-version concurrency control,
- write-ahead logging and crash recovery,
- vacuum and physical reclamation,
- a relational system catalog,
- a limited SQL front end,
- logical and physical planning,
- vectorized execution,
- statistics and cardinality estimation,
- cost-based optimization,
- concurrent transactions and, later, parallel query execution.

The system SHOULD remain small enough that one contributor can understand the complete path from SQL text to persistent bytes while still implementing realistic database-system mechanisms.

## 1.2 Design objectives

The architecture prioritizes:

1. **Correctness.** Persistent state, concurrency, transaction semantics, and recovery MUST be designed around explicit invariants rather than incidental behavior.
2. **Architectural clarity.** Subsystems MUST have clear ownership and dependency boundaries.
3. **Performance visibility.** CPU cache behavior, allocation, synchronization, I/O patterns, cardinality, intermediate-result size, and durability costs MUST remain measurable architectural concerns.
4. **Mechanism transparency.** Core database mechanisms such as page management, replacement, tuple storage, B+ trees, MVCC, WAL/recovery, execution, and optimization are intended to be implemented directly rather than hidden behind a database framework.
5. **Evolution without throwaway foundations.** Initial implementations MAY be simple, but the architecture SHOULD avoid choices that require replacing an entire subsystem merely to make measured performance improvements later.

The project deliberately accepts additional systems complexity where that complexity is inherent to the database mechanism being studied, provided the resulting engine remains realistically implementable.

## 1.3 Non-goals for the initial major version

The initial major version does not target:

- distributed consensus,
- replication,
- sharding,
- multi-node execution,
- cloud-native storage,
- full PostgreSQL or MySQL SQL compatibility,
- stored procedures,
- triggers,
- user-defined extensions,
- sophisticated authentication or authorization,
- columnar storage as the primary table representation,
- JIT query compilation,
- GPU execution.

These areas are outside the initial architecture scope and MAY be explored after the single-node engine is mature.

## 1.4 Selected foundational decisions

Two foundational choices are part of the v1 architecture baseline:

- the implementation language is **C++20**;
- transactional storage uses **heap-version MVCC**.

Changing either decision is a major architecture revision because each affects multiple subsystems, persistent behavior, and implementation strategy.

Any future proposal to replace one of these decisions MUST document at least:

- the current and proposed design,
- the correctness or performance motivation,
- affected subsystems,
- compatibility implications,
- migration cost,
- relevant benchmark evidence when the motivation is performance-related.

## 1.5 Design intent

The architecture is not optimized for the shortest path to a database that merely accepts SQL.

Where two approaches are otherwise viable, the design favors the one that exposes an important database-system mechanism rather than hiding it, so long as correctness, performance potential, and project feasibility remain acceptable.

This intent is rationale for the selected architecture; it does not override concrete subsystem contracts.

---

# 2. System Architecture and Dependency Model

## 2.1 Layered system model

The system follows a downward dependency model:

```text
SQL / Parser
    ↓
Binder + Catalog
    ↓
Logical Plan
    ↓
Optimizer
    ↓
Physical Plan
    ↓
Execution Engine
    ↓
Tables / Indexes / Transactions
    ↓
Buffer Pool
    ↓
WAL + Page / File Management
    ↓
Operating System / Storage Device
```

The diagram represents dependency direction, not necessarily one runtime call stack.

Lower layers MUST NOT depend on higher-layer syntax or semantic objects.

Examples:

- B+ tree code MUST NOT require SQL table syntax or parser objects.
- Buffer management MUST NOT require tuple-schema interpretation.
- WAL MUST NOT depend on SQL statement kinds such as `SELECT`.
- Parser code MUST NOT depend on physical page layout.
- Physical page parsers MUST NOT own transaction-visibility policy.
- SQL name resolution MUST NOT occur inside execution hot loops.

## 2.2 Separation of logical and physical concerns

The architecture maintains explicit boundaries between:

- SQL syntax and bound semantic objects,
- logical relational operators and physical algorithms,
- transaction-level logical locks and short-lived in-memory latches,
- persistent page identity and in-memory buffer-frame identity,
- tuple-format interpretation and page-local byte management,
- optimizer estimates and actual execution metrics.

These distinctions are cross-cutting requirements. Later chapters define their concrete representations and protocols.

## 2.3 Correctness and performance model

Correctness precedes micro-optimization, but performance-sensitive structure MUST remain visible from the start.

Architectural and performance analysis SHOULD explicitly consider:

- CPU cache behavior,
- branch prediction,
- allocation frequency,
- memory layout,
- pointer chasing,
- synchronization and contention,
- sequential versus random I/O,
- buffer-pool hit rate,
- WAL and durable-flush frequency,
- query cardinality,
- intermediate-result size,
- query memory pressure and spill behavior.

Measured hot paths MAY justify more complex algorithms or representations. Complexity SHOULD NOT be introduced solely from intuition when a simpler architecture satisfies the same correctness requirements and preserves a viable optimization path.

## 2.4 Shared-state and concurrency direction

The engine is single-process and multi-threaded.

Centralized global locks SHOULD NOT be used in hot paths merely to simplify synchronization.

The architecture must permit progressively partitioned shared state, including:

- sharded buffer-pool lookup structures,
- per-page latches,
- partitioned logical lock management,
- thread-local or query-local arenas,
- coordinated WAL append state.

The first correct implementation of a subsystem MAY use simpler synchronization where the later removal of contention does not require replacing the subsystem's ownership model.

## 2.5 Storage subsystem ownership and dependency direction

The storage stack follows the dependency direction:

```text
common definitions
    ↓
raw disk / file I/O
    ↓
buffer management
    ↓
heap pages / B+ tree physical pages
    ↓
relation / index abstractions
```

Higher-level coordination builds on these layers rather than bypassing them.

The principal storage responsibilities are:

```text
DiskManager
    raw fixed-position file I/O and raw file lifecycle

BufferPool
    resident-page caching, pins, latches, replacement, dirty state, flush

HeapPage
    slotted-page mechanics over one caller-owned resident page buffer

HeapFile
    relation-wide heap operations across pages

FreeSpaceMap
    advisory candidate-page discovery for heap insertion

TupleCodec
    schema-directed tuple serialization and typed access

Visibility subsystem
    snapshot visibility of physical tuple versions

Table
    logical relation coordination across heap, indexes, and schema
```

These names describe architectural roles; exact C++ type names are not required by the contract.

Critical dependency rules:

- `HeapPage` MUST NOT perform raw file I/O or call `DiskManager` directly.
- Normal resident-page access flows through the BufferPool once buffer management exists.
- `HeapPage` interprets one already-resident page and does not own a second page-sized allocation.
- `TupleCodec` is independent of page management.
- `TupleCodec` MAY depend on schema/type definitions but MUST remain independent of SQL parser AST representation.
- Transaction visibility MAY inspect tuple headers but MUST NOT be embedded in the physical heap-page parser.
- Buffer management MUST remain independent of heap tuple and B+ tree key semantics.

Concrete source-directory and filename organization is not an architectural requirement.

---

# 3. Platform and Runtime Baseline

## 3.1 Supported platform

The initial supported environment is:

- Linux,
- x86-64 or ARM64,
- POSIX file APIs,
- a single database process,
- multiple worker threads.

Portability is desirable but secondary to correctness, observability, and direct access to the required operating-system mechanisms.

The Linux-first baseline permits later experimentation with facilities such as `io_uring`, direct I/O, huge pages, and NUMA-aware designs without making those facilities requirements of the initial architecture.

## 3.2 Implementation language

The implementation MUST use C++20.

Compiler-specific extensions are not part of the architectural language baseline unless a later explicit decision introduces one.

C++20 was selected because it provides direct control over:

- object lifetime and ownership,
- memory layout,
- allocation behavior,
- cache-local data structures,
- atomics and synchronization,
- explicit byte manipulation,
- POSIX/Linux I/O,
- low-level profiling and optimization.

The design accepts the associated risks—undefined behavior, memory-safety failures, accidental allocation/copying, and difficult concurrency bugs—as engineering risks that must be controlled by explicit ownership, serialization, validation, testing, and sanitizer/tooling discipline.

---

# Part II — Storage and Persistence

# 4. Persistent Storage Foundations

## 4.1 Scope

This chapter defines the common physical contract shared by persistent random-access database files and pages.

It fixes:

- logical identifier widths and invalid sentinels,
- persistent page identity,
- physical tuple-version identity,
- index-to-heap addressing semantics,
- random-access file kinds,
- page-zero superblocks,
- the common page header,
- persisted page-type codes,
- append-first page allocation,
- checksum staging and corruption-detection rules.

Heap-page internals, tuple bytes, the free-space map, BufferPool behavior, B+ tree node formats, WAL records, and recovery protocols are defined by their owning chapters.

The storage foundation is intentionally shared: independently implemented storage, buffer, index, and transaction components MUST agree on these identities and persistent formats.

## 4.2 Page-oriented persistence and serialization

Persistent random-access database structures are page-oriented.

The v1 page size is:

```text
PAGE_SIZE = 8192 bytes
```

Page size MUST be represented by one canonical constant or configuration value rather than repeated independent assumptions.

Persistent formats use explicit binary encoding. They MUST NOT depend on:

- C++ object representation,
- compiler padding,
- native alignment,
- host endianness,
- process memory addresses.

Unless a more specific format explicitly states otherwise, multi-byte integer fields defined by this architecture use explicit little-endian encoding.

Persistent formats define their field widths, byte offsets, format versions, checksum behavior, and reserved-field rules explicitly.

## 4.3 Fundamental identifier types

The v1 logical identifier widths are:

| Identifier | Width | Logical representation |
|---|---:|---|
| `FileId` | 32 bits | `uint32_t` |
| `PageNo` | 64 bits | `uint64_t` |
| `SlotId` | 16 bits | `uint16_t` |
| `TxnId` | 64 bits | `uint64_t` |
| `CommandId` | 32 bits | `uint32_t` |
| `Lsn` | 64 bits | `uint64_t` |
| `TableId` | 64 bits | `uint64_t` |
| `IndexId` | 64 bits | `uint64_t` |
| `SchemaVer` | 32 bits | `uint32_t` |

The v1 invalid sentinels defined at this layer are:

| Sentinel | Value |
|---|---:|
| `INVALID_FILE_ID` | `0` |
| `INVALID_PAGE_NO` | `UINT64_MAX` |
| `INVALID_SLOT_ID` | `UINT16_MAX` |
| `INVALID_TXN_ID` | `0` |
| `INVALID_LSN` | `0` |

These values are part of the architecture wherever the corresponding sentinel is used.

This chapter does not define an invalid sentinel for `CommandId`, `TableId`, `IndexId`, or `SchemaVer`.

### 4.3.1 File identity versus operating-system handles

`FileId` is a database-level logical identifier.

An operating-system file descriptor is a process-local resource and MUST NOT be exposed or persisted as `FileId`.

Persistent identity MUST remain valid independently of the descriptor number chosen by a particular process execution.

## 4.4 Page identity

In memory, persistent page identity is represented conceptually as:

```cpp
struct PageId {
    FileId file_id;
    PageNo page_no;
};
```

`PageId` denotes the persistent logical identity of one page.

Within the file identified by `file_id`, page `page_no` is physically addressed at:

```text
byte_offset = page_no * PAGE_SIZE
```

`PageId` MUST be comparable and hashable. It SHOULD be trivially copyable when practical.

Persistent page identity is independent of BufferPool state.

The following MUST NOT be used as persistent page identity:

```text
buffer-frame index
memory address
operating-system file descriptor
```

The checked-I/O rules governing conversion of `page_no` to a physical file offset are defined by the I/O chapter.

## 4.5 Physical tuple-version identity

A heap tuple version is identified conceptually by:

```cpp
struct Rid {
    PageId page;
    SlotId slot;
};
```

An RID identifies a **physical heap tuple version**, not a permanent logical SQL row.

Consequently, an ordinary UPDATE that creates a new heap version also creates a new RID.

This identity model is intentional: physical-version addressing keeps heap-version MVCC, version chains, index maintenance, and vacuum behavior explicit.

The standalone persisted encoding of RID values used by indexes is defined by the B+ tree chapter. The logical meaning defined here applies regardless of the encoding used by a particular persistent structure.

## 4.6 Index-to-heap version addressing

B+ tree leaf entries reference physical tuple-version RIDs.

Conceptually, a leaf payload is:

```text
(index key, RID)
```

For every B+ tree index, including a SQL UNIQUE index, physical ordering includes RID:

```text
(index key, RID)
```

so duplicate physical entries remain individually addressable.

SQL uniqueness is enforced transactionally above the physical tree; it does not change the B+ tree's physical `(index key, RID)` ordering.

### 4.6.1 UPDATE consequence

For ordinary v1 heap-version UPDATE behavior:

```text
old tuple version
    xmax = updating transaction

new tuple version
    xmin = updating transaction
    new RID

new index entries
    reference new RID
```

Old index entries are not required to disappear immediately when their referenced tuple version becomes invisible. They remain physical entries until vacuum can remove them safely.

### 4.6.2 Index-scan consequence

An index hit never establishes MVCC visibility by itself.

An ordinary index scan follows the semantic path:

```text
index lookup
    ↓
RID
    ↓
heap tuple-version fetch
    ↓
MVCC visibility check
    ↓
return or reject tuple version
```

The physical index therefore does not own heap-version visibility decisions.

### 4.6.3 Deferred update optimization

A HOT-like future optimization MAY allow updates that do not modify indexed columns to avoid creating some new index entries.

HOT-like update optimization is not part of the baseline storage contract.

## 4.7 Random-access file model and file kinds

Major persistent objects use separate files rather than one monolithic database file.

The storage manager recognizes explicit random-access file kinds.

The persisted v1 `file_kind` field is an unsigned 16-bit little-endian integer with these codes:

| Code | File kind | Meaning |
|---:|---|---|
| `0` | `INVALID` / unassigned | not a valid persistent random-access database file kind |
| `1` | `HEAP` | heap relation storage |
| `2` | `BTREE` | B+ tree index storage |
| `3` | `FSM` | heap free-space metadata |
| `4` | `CATALOG` | catalog storage |

Existing persisted codes MUST NOT be renumbered because of source-language enum declaration order.

Future file kinds MUST receive new explicit numeric codes.

Code `0` MUST be rejected when decoding a v1 random-access file superblock.

WAL is not a random-access page-file kind. It uses its own append-only persistent log format.

Every random-access database file reserves:

```text
page 0 = file superblock
```

Ordinary object/data pages begin at:

```text
page 1
```

## 4.8 Common page header

Every persistent random-access page begins with a 32-byte common logical page header. WAL records use their separate log-record format.

The v1 common page-header layout is:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 2 | `page_type` |
| `2` | 2 | `format_version` |
| `4` | 4 | `flags` |
| `8` | 8 | `page_lsn` |
| `16` | 4 | `checksum_crc32c` |
| `20` | 2 | `header_size` |
| `22` | 2 | `reserved16` |
| `24` | 8 | `page_no` |
|  | **32** | **total** |

All multi-byte fields are little-endian.

The meaning of `format_version`, `header_size`, `flags`, and any reserved-field restrictions is completed by the specific page format. A page-format decoder MUST apply both the common-header rules and the rules of the requested page type.

For an ordinary v1 page type whose owning format assigns no common-header flag bits:

```text
flags = 0
```

The encoder/initializer MUST write zero and the v1 decoder MUST reject nonzero common-header flags.

Future ordinary-page flag meanings require an explicit compatible format rule; unassigned v1 bits are not silently preserved as unknown semantics.

### 4.8.1 Embedded page number

Although the physical file offset already implies the page number, `page_no` is stored inside the page so the database can detect conditions such as:

- misdirected writes,
- incorrect buffer mappings,
- file/page mixups,
- corrupted persistent metadata.

### 4.8.2 Page LSN

`page_lsn` is the LSN of the newest WAL-protected modification reflected in the page.

Before WAL integration, a page that has no meaningful WAL position MAY use:

```text
INVALID_LSN
```

The later WAL/recovery chapters define when and how `page_lsn` advances.

## 4.9 Persisted page-type registry

The common-header `page_type` field is an unsigned 16-bit little-endian integer.

The initial persisted codes are:

| Code | Page type |
|---:|---|
| `0` | `SUPERBLOCK` |
| `1` | `HEAP_DATA` |
| `2` | `FSM_DATA` |
| `3` | `BTREE_INTERNAL` |
| `4` | `BTREE_LEAF` |
| `5` | `BTREE_FREE` |
| `6` | `CATALOG_DATA` |

These numeric codes are part of the persistent page-format contract.

Existing values MUST NOT be renumbered because the source-language enum declaration changes.

Future page types MUST receive new explicit numeric codes.

Page-type-specific parsing MUST validate that the persisted `page_type` is the expected type before interpreting type-specific bytes.

Arbitrary page bytes MUST NOT be reinterpreted as a requested page format without this validation.

## 4.10 File superblock v1

Every page-based random-access file begins with one 8192-byte superblock at page `0`.

The superblock exists to detect:

- wrong file type,
- incompatible format,
- wrong page size,
- accidental file mixups,
- corruption of basic file metadata.

The superblock uses the normal 32-byte common page header at offsets `0..31`; common fields are not duplicated in the superblock-specific region.

The v1 superblock constants shared by every random-access file are:

```text
magic          = ASCII "DBLUSBLS"
format_version = 1
page_size      = 8192
page_type      = SUPERBLOCK
page_no        = 0
base prefix    = 72 bytes
```

`header_size` is the complete file-kind-specific superblock header size:

```text
HEAP     -> 72
FSM      -> 72
CATALOG  -> 72
BTREE    -> 128
```

The first 72 bytes are the common FileSuperblock prefix.

### 4.10.1 Common FileSuperblock prefix

| Offset | Size | Field / required v1 meaning |
|---:|---:|---|
| `0` | 2 | `page_type = SUPERBLOCK` |
| `2` | 2 | `format_version = 1` |
| `4` | 4 | `flags` |
| `8` | 8 | `page_lsn` |
| `16` | 4 | `checksum_crc32c` |
| `20` | 2 | file-kind-specific `header_size` |
| `22` | 2 | common `reserved16 = 0` |
| `24` | 8 | `page_no = 0` |
| `32` | 8 | `magic = ASCII "DBLUSBLS"` |
| `40` | 2 | `file_kind` |
| `42` | 2 | superblock `reserved16 = 0` |
| `44` | 4 | `page_size = 8192` |
| `48` | 4 | `file_id` |
| `52` | 4 | `reserved32 = 0` |
| `56` | 8 | `object_id` |
| `64` | 8 | `creation_epoch` |
|  | **72** | **common prefix** |

All multi-byte integer fields are little-endian.

For v1 `HEAP`, `FSM`, and `CATALOG` superblocks:

```text
header_size = 72
bytes 72..8191 = 0
```

For a v1 `BTREE` superblock:

```text
header_size = 128
bytes 72..127 = the B+ tree extension defined in §8.2.1
bytes 128..8191 = 0
```

A decoder MUST choose the valid `header_size` and extension rules from `file_kind`; it MUST NOT treat a BTREE extension as generic nonzero reserved bytes.

### 4.10.2 Object identity

`object_id` is interpreted according to `file_kind`:

```text
HEAP / FSM     -> TableId
BTREE          -> IndexId
CATALOG        -> catalog object ID where applicable
```

`creation_epoch` is an opaque persisted 64-bit value in v1.

Its generation procedure, time unit, or other semantic interpretation MAY be specified later without changing its field width or byte offset.

### 4.10.3 Superblock checksum

The common-header checksum field at bytes `16..19` is the only checksum field for the base superblock.

The v1 checksum is computed exactly as follows:

```text
1. logically treat bytes 16..19 as zero;
2. compute CRC32C over exactly bytes 0..8191;
3. store the resulting uint32_t little-endian in bytes 16..19.
```

Changing only the stored checksum bytes does not change the logical checksum input because those bytes are zeroed for the computation.

### 4.10.4 Superblock validation

A v1 superblock decoder MUST reject:

- input shorter than 8192 bytes,
- CRC32C mismatch,
- `page_type != SUPERBLOCK`,
- unsupported `format_version`,
- a `header_size` inconsistent with the decoded `file_kind`,
- `page_no != 0`,
- magic different from `DBLUSBLS`,
- invalid or unknown `file_kind`,
- `page_size != 8192`,
- any nonzero reserved field required to be zero by the base or file-kind-specific format,
- any nonzero byte in the file-kind-specific trailing reserved region.

Unknown `flags` bits are preserved as raw bits unless a later format revision assigns semantics to them.

A codec MAY accept an input buffer larger than one page, but only the leading 8192 bytes participate in the v1 superblock representation.

Because v1 requires every reserved superblock field and every byte after the file-kind-specific header to be zero, assigning future meaning to those bytes requires an explicit compatible file-kind format revision.

The superblock MUST be encoded and decoded field-by-field. Raw serialization of a C++ superblock object is prohibited.

## 4.11 Append-first page allocation

Initial physical page allocation is append-first.

For an aligned random-access page file:

```text
new page_no = current_file_page_count
extend file by exactly one PAGE_SIZE page
```

A newly created raw random-access file begins at:

```text
0 bytes
0 pages
```

The raw file-creation layer does not implicitly create a superblock.

Higher-level page-file creation explicitly:

1. creates the empty raw file,
2. allocates page `0`,
3. initializes page `0` as the file superblock.

A write to an unallocated `page_no` MUST fail. Ordinary page writes MUST NOT implicitly extend the file or create sparse pages.

The compound append operation:

```text
discover current aligned page count
+
extend by exactly one page
```

MUST be serialized against concurrent extensions of the same managed storage so two allocators cannot receive the same `PageNo`.

Whole-file shrinking is not required by the baseline architecture.

### 4.11.1 Later page reuse

Object-specific subsystems MAY later recycle pages that are completely unused.

Examples include:

- a B+ tree free-page list,
- completely empty heap pages,
- unused FSM metadata pages.

Recycling or reinitializing a previously used heap `PageNo` MUST NOT bypass physical RID-reuse safety. If page recycling can make a previously used `(PageNo, SlotId)` identity reusable, it is subject to the same later global reclamation/read-epoch gate as slot-level RID reuse.

The baseline does not require a general-purpose extent allocator.

Extent allocation remains a later storage optimization rather than a prerequisite for heap, index, or recovery correctness.

## 4.12 Page checksums

Persistent random-access page checksums use CRC32C.

For every checksummed 8192-byte random-access page:

```text
1. logically treat bytes 16..19 as zero;
2. compute CRC32C over exactly bytes 0..8191;
3. store the resulting uint32_t little-endian in bytes 16..19.
```

The common page header reserves the checksum field from the beginning of the format so checksum support does not require moving later header fields.

The superblock uses this rule mandatorily from v1 creation. Ordinary page types may stage enablement as defined below, but whenever their checksum is generated or verified, the byte range and zeroing rule above are exact.

### 4.12.1 Staged use

Before WAL/recovery integration, checksum generation and verification for ordinary persistent pages MAY be optional behind configuration while the storage engine is brought up.

Once recovery is part of the engine, checksums SHOULD be enabled by default for persistent random-access pages.

The superblock is governed by the stricter mandatory v1 checksum and validation rules in §4.10.

### 4.12.2 Purpose and limitation

Page checksums provide:

- corruption detection,
- misdirected/torn-write diagnostics,
- crash-test observability.

A checksum detects corruption; it does not by itself repair a torn page.

Torn-page recovery is defined by the WAL/recovery architecture rather than by CRC32C alone.

## 4.13 Storage-foundation invariants

The following invariants apply across all later storage subsystems:

1. `FileId` is a database identity, never an operating-system file descriptor.
2. `PageId` is a persistent logical identity, never a frame index or memory address.
3. RID denotes a physical heap tuple version, not a permanent logical SQL row.
4. Persistent numeric file-kind and page-type codes are explicit and never implicitly renumbered.
5. Random-access database file page `0` is the superblock; ordinary object pages begin at page `1`.
6. Persistent multi-byte fields defined by this chapter use explicit little-endian serialization.
7. The common page header is 32 bytes.
8. Page-type-specific parsing validates the expected persisted page type before type-specific interpretation.
9. A v1 superblock is exactly 8192 bytes; its `header_size` is `72` for HEAP/FSM/CATALOG and `128` for BTREE.
10. Every v1 superblock reserved field and every byte after its file-kind-specific header is zero.
11. The superblock CRC32C covers exactly bytes `0..8191` with bytes `16..19` logically zero.
12. A write to an unallocated page does not allocate or sparsely extend the file.
13. Concurrent append allocation of one managed file cannot return the same new `PageNo` to two callers.
14. `page_lsn` represents the newest WAL-protected modification reflected in the page once WAL is active.
15. Page checksums are corruption-detection mechanisms, not torn-page repair mechanisms.
---

# 5. Heap Storage and Tuple Format

## 5.1 Scope and storage model

Primary table storage is a row-oriented heap.

A heap relation uses at least:

```text
table_<table_id>.heap
table_<table_id>.fsm
```

The heap file is organized as:

```text
page 0       heap-file superblock
page 1..N    heap data pages
```

Heap data pages SHOULD remain physically easy to scan in page-number order. Large amounts of unrelated metadata SHOULD NOT be interleaved among heap data pages; this is one reason free-space metadata is stored separately.

The `.fsm` file and its exact persisted/runtime format are defined in Chapter 6.

## 5.2 Heap scan order

A full sequential heap scan visits:

1. heap data pages in ascending `page_no`,
2. slots within each page in ascending `SlotId`.

The executor applies MVCC visibility after obtaining physical tuple versions.

Physical heap scan order is **not** SQL result ordering.

Without an explicit `ORDER BY`, SQL result ordering remains semantically unspecified even if a particular execution happens to follow physical page/slot order.

## 5.3 HEAP_DATA page format v1

A heap data page uses the 32-byte common page header defined in Chapter 4 followed immediately by a 16-byte heap-specific header.

For v1:

```text
page_type          = HEAP_DATA
format_version     = 1
header_size        = 48
common flags       = 0
common reserved16  = 0
```

The persisted common-header `page_no` MUST equal the logical `PageId.page_no` of the page being interpreted.

The total heap-page header is exactly:

```text
48 bytes
```

### 5.3.1 Heap-specific header layout

| Page offset | Size | Field |
|---:|---:|---|
| `32` | 2 | `slot_count` |
| `34` | 2 | `free_slot_head` |
| `36` | 2 | `lower` |
| `38` | 2 | `upper` |
| `40` | 4 | `prune_hint` |
| `44` | 4 | heap `reserved` |
|  | **16** | **heap-specific header** |

All multi-byte fields are little-endian.

`lower` is the first byte after the heap header and slot directory.

`upper` is the first byte of the tuple-data region.

The current contiguous free-space interval is:

```text
[lower, upper)
```

with size:

```text
free_bytes = upper - lower
```

Because every slot entry is 8 bytes:

```text
lower = 48 + slot_count * 8
```

Valid v1 page geometry requires:

```text
48 <= lower <= upper <= PAGE_SIZE
```

Slot entries grow upward from the fixed heap header.

Tuple bytes grow downward from the end of the page.

### 5.3.2 Free-slot-list field

The persisted empty-list sentinel is:

```text
free_slot_head = INVALID_SLOT_ID
               = UINT16_MAX
               = 0xFFFF
```

A blank heap page therefore stores:

```text
slot_count     = 0
free_slot_head = INVALID_SLOT_ID
```

Nonempty free-list linkage is not defined by this heap-page baseline. The field is reserved so a later safe slot-reuse protocol can avoid scanning the entire slot directory.

The existence of `free_slot_head` MUST NOT be interpreted as permission to reuse a `DEAD` slot immediately.

### 5.3.3 Reserved and hint fields

For HEAP_DATA format v1:

```text
common flags      = 0
common reserved16 = 0
heap reserved     = 0
```

Structural decoding/validation MUST reject a nonzero value in any of these unassigned/reserved fields.

Assigning semantics to either reserved field requires compatible version handling.

`prune_hint` is initialized to zero.

Its operational vacuum/pruning semantics are deferred until explicitly defined; this chapter does not assign a correctness meaning to a nonzero hint.

## 5.4 Slot directory format

Every heap slot entry is exactly 8 bytes.

| Slot-relative offset | Size | Field |
|---:|---:|---|
| `0` | 2 | `tuple_offset` |
| `2` | 2 | `tuple_length` |
| `4` | 2 | `slot_flags` / persisted slot-state code |
| `6` | 2 | `aux` |
|  | **8** | **total** |

All multi-byte fields are little-endian.

In heap-page v1, the 16-bit field at slot offset `4` is interpreted using these persisted slot-state codes:

| Code | State |
|---:|---|
| `0` | `UNUSED` |
| `1` | `NORMAL` |
| `2` | `DEAD` |
| `3` | `REDIRECT_RESERVED` |

These numeric codes are persistent-format values.

Existing codes MUST NOT be renumbered because of source-language enum order. Future states require new explicit numeric codes or an explicit compatible format revision.

A persisted slot-state value outside this set is structurally invalid in heap-page v1.

### 5.4.1 NORMAL slots

A newly created `NORMAL` slot MUST persist:

```text
aux = 0
```

The baseline insertion path assigns no other meaning to `aux` for `NORMAL` slots.

Giving `aux` operational meaning for `NORMAL` slots requires an explicitly compatible rule or coordinated page-format revision.

A `NORMAL` slot MUST reference tuple bytes wholly inside the tuple-data region and inside the page.

### 5.4.2 UNUSED and REDIRECT_RESERVED slots

Heap-page v1 assigns no additional tuple-range or `aux` semantics to:

```text
UNUSED
REDIRECT_RESERVED
```

`REDIRECT_RESERVED` and `aux` are reserved for possible HOT-like behavior, but that behavior is not defined by this baseline.

### 5.4.3 DEAD slots and physical reclamation

A `NORMAL -> DEAD` transition does not itself reclaim tuple bytes.

Before compaction, a `DEAD` slot MAY retain its previous:

```text
tuple_offset
tuple_length
aux
```

while those tuple bytes remain physically present.

After compaction has physically discarded the payload of a `DEAD` slot, the canonical persisted coordinates are:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved
```

Clearing the coordinates prevents the reclaimed `DEAD` slot from pointing into newly free space or into bytes moved for another tuple.

This canonicalization does **not** change the slot to `UNUSED`.

It does **not** make the slot reusable.

It does **not** place the slot on the free-slot list.

The `SlotId` and `DEAD` state remain stable until a later reclamation/reuse protocol explicitly permits a state transition.

### 5.4.4 Stable SlotId

Heap-page compaction MAY change:

```text
slot.tuple_offset
```

but MUST NOT change:

```text
SlotId
```

Therefore a physical RID remains stable across ordinary page compaction.

## 5.5 Free-space geometry and page compaction

Conceptually, a heap page is arranged as:

```text
0
┌──────────────────────────────┐
│ common page header           │
├──────────────────────────────┤
│ heap-specific header         │
├──────────────────────────────┤
│ slot 0                       │
│ slot 1                       │
│ ...                          │
│                              │ ← lower
├──────────────────────────────┤
│       contiguous free space  │
├──────────────────────────────┤
│                              │ ← upper
│ tuple bytes                  │
│ tuple bytes                  │
└──────────────────────────────┘
8192
```

Insertion requires sufficient space for:

```text
tuple bytes
+
a new slot entry, when no reusable slot is available under a separately defined safe-reuse protocol
```

The baseline page format itself does not define when slot reuse becomes safe.

Page compaction MAY be used when total reclaimable space is sufficient but the currently contiguous free-space interval is not.

Compaction MUST preserve stable `SlotId` values and the canonical `DEAD`-slot rules in §5.4.3.

## 5.6 Maximum inline tuple size

Tuple storage v1 is inline-only: one tuple must fit entirely inside one heap page.

Overflow/TOAST-style tuple storage is not part of the v1 heap format.

For the initial page geometry:

```text
PAGE_SIZE         = 8192
heap total header = 48
slot entry        = 8
```

the maximum accepted raw tuple payload is:

```text
8135 bytes
```

The bound is intentionally strict.

A raw payload of:

```text
8136 bytes
```

MUST be rejected even though that size would make the slot directory and tuple region meet exactly with zero bytes remaining.

The 8135-byte bound is a raw heap-page storage limit. It is not a promise that every byte string up to that size is a semantically valid encoded SQL tuple.

Schema-directed tuple encoding/validation may reject values for independent tuple-format reasons.

An oversized tuple insertion MUST produce an explicit row-too-large/unsupported outcome rather than truncating or corrupting the page.

Overflow/large-object storage remains deferred until the base heap, WAL/recovery, MVCC, and index layers are mature.

## 5.7 Tuple header v1

Every physical heap tuple version begins with a fixed 48-byte tuple header.

| Tuple offset | Size | Field |
|---:|---:|---|
| `0` | 8 | `xmin` |
| `8` | 8 | `xmax` |
| `16` | 4 | `cmin` |
| `20` | 4 | `cmax` |
| `24` | 8 | `prev_page_no` |
| `32` | 2 | `prev_slot` |
| `34` | 2 | `tuple_flags` |
| `36` | 2 | `header_bytes` |
| `38` | 2 | `null_bitmap_bytes` |
| `40` | 4 | `schema_version` |
| `44` | 4 | `reserved` |
|  | **48** | **total** |

All multi-byte fields are little-endian.

For tuple-header v1:

```text
header_bytes = 48
reserved     = 0
```

`header_bytes` covers only the fixed 48-byte tuple-header prefix. It does not include the null bitmap.

Tuple-header bytes `44..47` MUST be written as zero and MUST be zero when decoding v1.

Assigning them semantics requires a coordinated format revision.

### 5.7.1 xmin

`xmin` identifies the transaction that created the physical tuple version.

### 5.7.2 xmax

`xmax` identifies the transaction that invalidated, deleted, or superseded the physical tuple version.

The v1 representation for no invalidating transaction is:

```text
xmax = INVALID_TXN_ID = 0
```

### 5.7.3 cmin and cmax

`cmin` and `cmax` are command identifiers within a transaction.

They exist because transaction identity and statement/command visibility are distinct concepts.

`CommandId` value:

```text
0
```

is valid and MUST NOT be interpreted as an invalid sentinel.

The exact visibility interpretation of `xmin`, `xmax`, `cmin`, and `cmax` belongs to the MVCC/transaction chapters.

### 5.7.4 Previous-version pointer

The pair:

```text
(prev_page_no, prev_slot)
```

points to the immediately preceding physical tuple version in the **same heap file**.

No `FileId` is persisted in the tuple-version link.

No previous version is represented by:

```text
prev_page_no = INVALID_PAGE_NO
prev_slot    = INVALID_SLOT_ID
```

Tuple-header v1 requires the pair to be internally consistent:

```text
no previous version:
    prev_page_no = INVALID_PAGE_NO
    prev_slot    = INVALID_SLOT_ID

previous version exists:
    prev_page_no != INVALID_PAGE_NO
    prev_slot    != INVALID_SLOT_ID
```

A mixed sentinel/non-sentinel pair is structurally invalid.

The pointer creates a backward physical version chain.

That chain supports later behavior such as vacuum, debugging, and possible HOT-style/version-history operations without placing relation `FileId` into every tuple header.

## 5.8 Tuple flags v1

`tuple_flags` stores physical tuple-version/layout facts.

Tuple-header v1 assigns these bits:

| Bit mask | Meaning |
|---:|---|
| `0x0001` | `HAS_NULLS` |
| `0x0002` | `HAS_VARLEN` |

The v1 known mask is:

```text
0x0003
```

Every other bit is invalid in tuple-header v1.

Encoding and decoding MUST reject unknown bits rather than silently preserving them.

Existing bit assignments MUST NOT be renumbered. Future flags require explicit bit assignments plus compatibility consideration.

### 5.8.1 HAS_VARLEN

For tuple format v1:

```text
HAS_VARLEN is set
    iff
the interpreting physical schema contains at least one VARCHAR column
```

This is a property of the tuple's physical schema/layout.

It does not depend on the number of variable payload bytes in a particular tuple.

Therefore:

- an all-NULL VARCHAR layout still has `HAS_VARLEN`,
- an all-empty-but-present VARCHAR layout still has `HAS_VARLEN`,
- a fixed-only physical schema MUST NOT set `HAS_VARLEN`.

Schema-directed validation MUST reject either mismatch direction.

### 5.8.2 Deferred tuple flags

The following conceptual flags remain unassigned/deferred:

```text
IS_DELETED_HINT
CHAIN_ROOT
CHAIN_MEMBER
```

They MUST NOT be persisted as ad-hoc substitutes for transaction status.

A future physical tuple flag requires well-defined physical, recovery, and visibility semantics before it becomes part of the persisted format.

## 5.9 Canonical tuple body layout

After the fixed tuple header, tuple format v1 is:

```text
┌────────────────────────────┐
│ 48-byte tuple header       │
├────────────────────────────┤
│ null bitmap                │
├────────────────────────────┤
│ fixed layout area          │
│   fixed-width values       │
│   VARCHAR descriptors      │
├────────────────────────────┤
│ variable-length payload    │
└────────────────────────────┘
```

The interpreting schema owns a precomputed physical layout description.

For tuple format v1, that physical layout is derived deterministically from the physical schema.

Let:

```text
fixed_width(BOOLEAN)   = 1
fixed_width(INT32)     = 4
fixed_width(INT64)     = 8
fixed_width(FLOAT64)   = 8
fixed_width(DATE)      = 4
fixed_width(TIMESTAMP) = 8
fixed_width(VARCHAR)   = 8   // descriptor width
```

Then:

```text
fixed_area_offset =
    48 + null_bitmap_bytes

cursor =
    fixed_area_offset

for each physical column in schema order:
    fixed_offset[column] = cursor
    cursor += fixed_width(column.physical_type)

VarlenPayloadOffset() =
    cursor

MinimumTupleSize() =
    VarlenPayloadOffset()
```

There is no alignment padding and no gap between fixed fields/descriptors.

Every fixed offset is tuple-relative.

The same physical schema therefore derives the same fixed offsets independently of compiler ABI or host alignment.

Execution SHOULD NOT rediscover one column's physical offset by repeatedly walking all preceding columns.

The persisted layout is compact and MAY contain fields that are not naturally aligned for the host machine.

Serialization/deserialization MUST use unaligned-safe byte operations such as explicit endian helpers and `memcpy`-style bit transfer rather than undefined-behavior pointer casts.

The execution engine MAY decode persisted values into naturally aligned vectors. Persisted storage layout and execution layout are intentionally distinct representations.

### 5.9.1 Exact tuple length

For tuple format v1, physical length is canonical and exact:

```text
tuple_size =
    MinimumTupleSize()
    + sum(payload_length of each non-NULL VARCHAR)
```

No unreferenced trailing bytes are permitted.

Schema-directed validation MUST finish with the canonical varlen payload cursor exactly equal to the supplied tuple byte length.

This rule also applies to fixed-only schemas: a fixed-only tuple has exactly:

```text
MinimumTupleSize()
```

bytes.

## 5.10 Null bitmap

Tuple format v1 allocates exactly one null bit for **every physical schema column**, including a column declared `NOT NULL`.

For physical column index `i`:

```text
null_bit_index(i) = i
```

The byte count is:

```text
null_bitmap_bytes =
    column_count / 8
    + (column_count % 8 != 0 ? 1 : 0)
```

equivalently:

```text
ceil(column_count / 8)
```

The resulting byte count MUST fit the persisted 16-bit `null_bitmap_bytes` tuple-header field.

A zero-column physical schema has:

```text
null_bitmap_bytes = 0
```

### 5.10.1 Persisted bit meaning

```text
bit = 1  -> value is NULL
bit = 0  -> value is present
```

### 5.10.2 Persisted bit ordering

Bits are assigned LSB-first by physical schema-column index:

```text
column 0  -> byte 0, bit 0
column 1  -> byte 0, bit 1
...
column 7  -> byte 0, bit 7
column 8  -> byte 1, bit 0
...
```

In general:

```text
byte_index = column_index / 8
bit_index  = column_index % 8
bit_mask   = 1 << bit_index
```

This ordering is persistent-format behavior and MUST NOT depend on source-language bitfield or ABI layout.

### 5.10.3 Nullability metadata

Bitmap allocation is independent of whether a column is declared nullable.

A `NOT NULL` column still owns a physical null bit.

Low-level bitmap primitives do not enforce SQL `NOT NULL` constraints.

Schema-directed tuple validation MUST reject a persisted NULL bit for a column declared `NOT NULL` in the schema version interpreting that tuple.

Such a tuple is invalid/corrupt or incompatible relative to that schema.

### 5.10.4 Unused bitmap bits

Unused high bits in the final bitmap byte MUST be zero.

Schema-directed validation MUST reject nonzero unused high bits.

### 5.10.5 HAS_NULLS

`HAS_NULLS` is canonical per tuple:

```text
HAS_NULLS is set
    iff
at least one used null-bitmap bit is set
```

Both mismatch directions are invalid:

```text
HAS_NULLS set but no used NULL bit exists   -> invalid
NULL bit exists but HAS_NULLS is clear      -> invalid
```

A nullable declaration alone does not cause `HAS_NULLS` to be set.

### 5.10.6 Fixed-area bytes beneath NULL

A v1 writer MUST deterministically write zero into the fixed-area bytes reserved for a NULL fixed-width column.

While the null bit is set, those bytes are semantically unread.

A v1 reader is **not** required to reject nonzero bytes beneath a NULL bit.

This distinguishes canonical writer output from the minimum reader-validity contract.

## 5.11 Fixed-width physical values

The initial scalar widths are:

| Physical type | Width | Persisted representation |
|---|---:|---|
| `BOOLEAN` | 1 byte | exact byte `0x00` or `0x01` |
| `INT32` | 4 bytes | signed two's-complement bit pattern |
| `INT64` | 8 bytes | signed two's-complement bit pattern |
| `FLOAT64` | 8 bytes | exact IEEE-754 binary64 payload bits |
| `DATE` | 4 bytes | signed physical scalar |
| `TIMESTAMP` | 8 bytes | signed physical scalar |

Multi-byte values are encoded little-endian.

Persistent scalar encoding MUST NOT depend on native C++ object layout or alignment.

### 5.11.1 BOOLEAN

The only valid persisted BOOLEAN bytes are:

```text
false = 0x00
true  = 0x01
```

Any other byte is invalid BOOLEAN data in tuple format v1.

### 5.11.2 Signed scalar representations

`INT32`, `INT64`, `DATE`, and `TIMESTAMP` persist their fixed-width signed two's-complement bit patterns and then encode those bits little-endian.

`DATE` is physically a signed 32-bit scalar.

`TIMESTAMP` is physically a signed 64-bit scalar.

Their SQL-level epoch, unit, precision, calendar, and time-zone semantics are not defined by the storage codec and remain owned by the SQL type layer.

### 5.11.3 FLOAT64

`FLOAT64` persists the exact IEEE-754 binary64 payload bits as a 64-bit value encoded little-endian.

The storage codec MUST preserve all 64 payload bits, including:

- `+0.0` versus `-0.0`,
- positive and negative infinity,
- NaN sign,
- quiet/signaling state,
- NaN payload bits.

Tuple format v1 does not canonicalize NaNs.

Bit-preserving conversion plus explicit integer-endian encoding is appropriate; raw ABI-dependent floating-point object serialization is not.

## 5.12 VARCHAR representation

Every physical VARCHAR column owns an 8-byte descriptor in the tuple fixed area:

| Descriptor-relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `payload_offset` |
| `4` | 4 | `payload_length` |

Both fields are unsigned 32-bit little-endian integers.

`payload_offset` is absolute relative to tuple byte `0`.

VARCHAR payload bytes are stored inline in the same tuple.

No terminator byte is stored; length is explicit.

For every non-NULL VARCHAR, the descriptor MUST point at or after:

```text
VarlenPayloadOffset()
```

and checked evaluation of:

```text
payload_offset + payload_length
```

MUST remain within the exact physical tuple length.

### 5.12.1 NULL VARCHAR

A NULL VARCHAR is identified by its null bit and MUST persist the canonical descriptor:

```text
payload_offset = 0
payload_length = 0
```

Schema-directed validation MUST reject any other descriptor under a set NULL bit.

A NULL VARCHAR contributes zero payload bytes.

### 5.12.2 Present VARCHAR packing

Present VARCHAR payloads are packed consecutively in physical schema-column order beginning at:

```text
VarlenPayloadOffset()
```

Validation maintains an expected payload cursor.

For each present VARCHAR:

```text
descriptor.payload_offset = expected_payload_offset
expected_payload_offset += descriptor.payload_length
```

Therefore v1 permits no:

- payload gaps,
- payload overlaps,
- backward payload offsets,
- payload reordering,
- references into the tuple header,
- references into the null bitmap,
- references into the fixed area.

### 5.12.3 Present empty VARCHAR

A present empty VARCHAR is distinct from NULL:

```text
null bit       = 0
payload_length = 0
payload_offset = current expected payload cursor
```

Because its length is zero, multiple present empty VARCHAR values MAY share the same current payload cursor.

### 5.12.4 Payload semantics

VARCHAR payload bytes are opaque at the storage layer.

The storage format does not define:

- UTF-8 validity,
- collation,
- locale,
- character count,
- `VARCHAR(n)` semantics,
- a terminator convention.

Those semantics belong to higher layers.

Large/overflow values remain deferred; the complete tuple must satisfy the inline tuple-size limit.

## 5.13 Schema versioning

Every physical tuple version stores:

```text
schema_version
```

The first implementation MAY support only schema version:

```text
1
```

The field exists from the initial format so future schema-evolution experiments do not require silently changing the tuple representation.

SQL DDL MAY reject schema changes that require version translation until such translation is explicitly implemented.

## 5.14 INSERT protocol boundary

The conceptual heap INSERT path is:

```text
1. encode tuple and compute required bytes
2. ask FreeSpaceMap for a candidate heap page
3. fetch candidate through BufferPool
4. acquire exclusive page latch
5. verify actual free space
6. compact page if worthwhile
7. allocate a slot
   - reuse only if a separately defined safe-reuse protocol makes one eligible
8. write tuple bytes
9. update slot directory
10. update page LSN when WAL exists
11. mark buffer frame dirty
12. release latch/page guard
13. update FSM estimate
14. create index entries
```

The FSM is advisory.

A stale FSM candidate MUST NOT make insertion incorrect; actual heap-page free space is checked after fetching/latching the page.

The exact ordering of WAL creation, page mutation, dirty-state publication, and page-LSN updates is owned by the later WAL/BufferPool transaction protocol. This chapter does not preempt that ordering.

## 5.15 UPDATE protocol boundary

The v1 UPDATE storage model appends a new physical tuple version.

Conceptually:

```text
1. locate visible old version
2. acquire required transaction write/conflict protection
3. create a complete new tuple version
4. new.xmin = current transaction
5. new.prev = old RID
6. old.xmax = current transaction
7. insert new version into heap, possibly on another page
8. create required index entries for new RID
9. retain old tuple/index entries until vacuum
```

User-visible tuple bytes are not updated in place in v1.

The old version MUST remain physically available while a legal snapshot may still require it.

The heap layer MAY prefer placing the new version on the same page when sufficient space exists.

Same-page placement is a locality optimization, not an invariant.

The detailed transaction visibility, command-ID handling, locking, WAL, and abort behavior is defined by later transaction/durability chapters.

## 5.16 DELETE protocol boundary

The baseline DELETE operation is logical before physical reclamation.

Conceptually:

```text
set the visible tuple version's xmax
```

The tuple bytes and corresponding physical index entries remain present.

Physical reclamation occurs during vacuum only after the global visibility/reclamation rules determine that no legal snapshot can require the version.

## 5.17 MVCC visibility boundary

`HeapPage` does **not** decide SQL visibility.

`HeapPage` exposes physical tuple versions and their physical metadata.

A transaction/MVCC visibility component interprets:

```text
xmin
xmax
cmin
cmax
```

against transaction state and a snapshot.

Conceptually:

```text
HeapPage
    provides tuple header + bytes

Visibility / transaction subsystem
    interprets tuple MVCC metadata against snapshot

Table scan / index scan
    combines physical access with visibility
```

Visibility depends on global transaction state, not on heap-page bytes alone.

## 5.18 HeapPage representation boundary

`HeapPage` is a lightweight page-format view over caller-owned page bytes.

It MUST NOT independently own another 8192-byte page allocation merely to interpret a page already resident in the BufferPool.

Architecturally, `HeapPage` provides responsibilities equivalent to:

- formatting/initializing a heap page over supplied page bytes,
- opening/validating an existing heap page,
- inserting raw tuple bytes into a slot,
- obtaining a physical tuple view for a slot,
- marking a slot `DEAD` when a higher layer has authorized that transition,
- compacting already-reclaimable physical payload,
- reporting contiguous/reclaimable page-local space,
- iterating physical slots.

The exact C++ method names and signatures are not part of the persistent or subsystem contract.

Page-format algorithms operate on caller-owned resident bytes.

## 5.19 TupleCodec boundary

Tuple serialization is separate from page management.

The encode direction is conceptually:

```text
logical values + physical Schema/Layout
        ↓
TupleCodec
        ↓
encoded tuple bytes
        ↓
HeapPage insertion
```

The decode/access direction is conceptually:

```text
TupleView + physical Schema/Layout
        ↓
TupleCodec / typed accessors
        ↓
execution vectors or planning/runtime values
```

`HeapPage` MUST NOT need to know that a particular physical column is, for example, a VARCHAR.

Schema-directed tuple interpretation belongs to the tuple codec/layout layer.

## 5.20 Tuple views and execution decoding

Storage code SHOULD avoid unnecessary copies by exposing immutable tuple views into resident page bytes when lifetime safety can be maintained.

A tuple view into BufferPool-backed storage MUST NOT outlive the page guard/pin that keeps its backing frame resident.

The execution layer MAY decode or copy selected columns into naturally aligned typed vectors.

This intentionally separates:

```text
storage:
    compact persisted bytes
    zero-copy views where safe

execution:
    aligned typed vectors optimized for processing
```

No long-lived naked pointer/span into a buffer frame may survive after the protecting page lifetime object has been released.

## 5.21 Heap and tuple invariants

1. Heap data pages are HEAP_DATA format version `1` with total header size `48`.
2. Heap common `reserved16` and heap-specific `reserved` are zero in v1.
3. `lower = 48 + slot_count * 8`.
4. Valid geometry satisfies `48 <= lower <= upper <= PAGE_SIZE`.
5. A slot entry is exactly 8 bytes.
6. Persisted slot-state codes are fixed as `UNUSED=0`, `NORMAL=1`, `DEAD=2`, `REDIRECT_RESERVED=3`.
7. Unknown persisted slot-state codes are invalid in v1.
8. A new `NORMAL` slot has `aux=0`.
9. Page compaction does not change `SlotId`.
10. Physically reclaimed `DEAD` slots canonicalize tuple coordinates to `(0,0)` but remain `DEAD`.
11. A `DEAD` slot is not reusable merely because its payload was compacted away.
12. The maximum accepted raw tuple payload is `8135` bytes; `8136` is rejected.
13. A tuple header is exactly 48 bytes; `header_bytes=48` and tuple reserved bytes `44..47` are zero.
14. `CommandId{0}` is valid.
15. The previous-version pointer is either two invalid sentinels or two non-sentinels, and always refers within the same heap file.
16. The v1 tuple-flags known mask is `0x0003`; unknown bits are invalid.
17. `HAS_VARLEN` exactly reflects whether the interpreting physical schema contains VARCHAR.
18. Tuple physical length is exact; trailing unreferenced bytes are invalid.
19. Every physical schema column owns one LSB-first null bit.
20. Unused high null-bitmap bits are zero.
21. `HAS_NULLS` exactly reflects whether any used null bit is set.
22. Schema-directed validation rejects a NULL bit for a `NOT NULL` column.
23. BOOLEAN accepts only `0x00` and `0x01`.
24. FLOAT64 preserves exact IEEE-754 binary64 payload bits and does not canonicalize NaNs.
25. A NULL VARCHAR descriptor is exactly `(0,0)`.
26. Present VARCHAR payloads are packed consecutively in physical schema order with no gaps or overlaps.
27. Present empty VARCHAR is distinct from NULL.
28. `HeapPage` owns physical page mechanics, not SQL visibility.
29. Physical heap scan order does not imply SQL result ordering.
---

# 6. Free-Space Management and Physical Reclamation

## 6.1 Scope

Free-space discovery is a distinct storage subsystem.

Each heap relation has a separate page-based free-space-map file:

```text
table_<table_id>.fsm
```

The FSM is advisory performance metadata. It is not the source of truth for whether a heap-page insertion can succeed.

The canonical insertion decision is always made from the actual validated heap-page geometry after the candidate page has been fetched and appropriately latched.

This chapter defines:

- the persisted v1 free-space category,
- the exact category mapping and inverse lower-bound interpretation,
- the `FSM_DATA` page format,
- deterministic heap-page-to-FSM-entry addressing,
- initialized-prefix semantics,
- FSM page initialization,
- runtime acceleration boundaries,
- staleness, repair, and rebuild behavior,
- heap-page compaction constraints,
- vacuum's physical-reclamation boundary.

I/O ownership, BufferPool mechanics, and WAL/recovery ordering are defined by later chapters.

## 6.2 FSM file organization

The FSM is a separate random-access page file:

```text
page 0       FileSuperblock with FileKind::FSM
page 1..N    FSM_DATA pages
```

Each ordinary heap data page is represented by one approximate one-byte category.

The persisted category domain is:

```text
uint8_t category

0   = least represented insertion capacity
255 = greatest represented insertion capacity
```

Larger category values represent greater tuple-payload insertion capacity under the v1 mapping.

All byte values `0..255` are valid categories for initialized entries.

## 6.3 V1 free-space category semantics

The category conversion input is the current contiguous free-space gap of a structurally valid heap page:

```text
free_bytes = upper - lower
```

For the v1 heap-page layout:

```text
maximum contiguous gap
    = PAGE_SIZE - HEAP_PAGE_TOTAL_HEADER_SIZE
    = 8192 - 48
    = 8144 bytes
```

The persisted category represents **conservative tuple-payload insertion capacity assuming that a new 8-byte slot entry is required**.

The v1 mapping first computes:

```text
bounded_free =
    min(free_bytes, 8144)

usable_insertion_bytes =
    min(
        max(bounded_free - 8, 0),
        8135
    )
```

where:

```text
8    = HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE
8135 = HEAP_PAGE_MAX_RAW_TUPLE_SIZE
```

The category is then:

```text
category =
    floor(usable_insertion_bytes * 255 / 8135)
```

The conversion uses integer arithmetic only.

Inputs above the physical 8144-byte contiguous-gap maximum are clamped to 8144 before the slot-cost deduction.

The mapping is monotonic.

The mapping remains conservative even if a later insertion path can sometimes reuse a slot without paying the new 8-byte slot-entry cost. Persisted FSM metadata MUST NOT overstate guaranteed insertion capacity merely because slot reuse may become possible.

### 6.3.1 Locked representative boundaries

The following values are part of the v1 mapping contract:

```text
free_bytes 0, 1, 8, 9, 39 -> category 0
free_bytes 40             -> category 1
free_bytes 4075           -> category 127
free_bytes 8142           -> category 254
free_bytes 8143, 8144     -> category 255
```

## 6.4 Category lower-bound interpretation

For a persisted category `c`, the v1 inverse helper is an **inclusive lower bound** on represented usable tuple-payload bytes.

It is not an upper bound and not a bucket midpoint.

The exact definition is:

```text
minimum_usable(c) =
    ceil(c * 8135 / 255)
```

Using integer arithmetic:

```text
minimum_usable(c) =
    (c * 8135 + 254) / 255
```

Representative values are:

```text
category 0   -> 0
category 1   -> 32
category 127 -> 4052
category 254 -> 8104
category 255 -> 8135
```

A runtime candidate search MAY use this represented lower bound when locating a category capable of satisfying an insertion request.

The source contract in this pass does not prescribe a specific runtime tie-breaker, exact bucket-search algorithm, or in-memory candidate-index representation.

Regardless of candidate-search policy, the selected heap page MUST still be fetched and checked because persisted/runtime FSM information may be stale.

## 6.5 FSM_DATA page format v1

The persisted `FSM_DATA` page uses:

```text
page_type      = FSM_DATA
format_version = 1
header_size    = 48
```

The complete page size is 8192 bytes.

### 6.5.1 Byte layout

| Page offset | Size | Field |
|---:|---:|---|
| `0` | 32 | common page header |
| `32` | 8 | `first_heap_page_no`, `uint64`, little-endian |
| `40` | 2 | `entry_count`, `uint16`, little-endian |
| `42` | 2 | FSM `reserved16 = 0` |
| `44` | 4 | FSM `reserved32 = 0` |
| `48` | 8144 | one-byte category entries |
|  | **8192** | **total** |

The FSM-specific header is exactly 16 bytes after the common 32-byte page header.

All multi-byte fields are explicitly little-endian.

For FSM_DATA v1:

```text
common flags      = 0
common reserved16 = 0
FSM reserved16    = 0
FSM reserved32    = 0
```

Structural decoding/validation MUST reject a nonzero value in any of these unassigned/reserved fields.

The category region contains exactly:

```text
8144
```

one-byte entry slots.

V1 has no hierarchical FSM level, tree node, persisted pointer, or per-entry metadata.

## 6.6 Deterministic heap-page-to-FSM-entry mapping

Heap page `0` is the heap-file superblock and has no ordinary FSM category entry.

`INVALID_PAGE_NO` is not a valid FSM target.

For ordinary heap data page number `H`:

```text
heap_data_index = H - 1

fsm_page_index =
    heap_data_index / 8144

fsm_page_no =
    1 + fsm_page_index

entry_index =
    heap_data_index % 8144
```

FSM file page `0` remains its own FileSuperblock.

Representative mappings are:

```text
heap page 1
    -> FSM page 1, entry 0

heap page 8144
    -> FSM page 1, entry 8143

heap page 8145
    -> FSM page 2, entry 0
```

Mapping arithmetic MUST be checked for overflow rather than permitted to wrap.

For FSM data-page number `P`, the first represented heap page is:

```text
first_heap_page_no =
    1 + (P - 1) * 8144
```

The persisted `first_heap_page_no` MUST equal this deterministic value for the physical FSM page number.

The following are invalid FSM_DATA page numbers:

```text
0
INVALID_PAGE_NO
```

A represented heap-page range MUST NOT overflow and MUST NOT include `INVALID_PAGE_NO`.

## 6.7 entry_count and initialized-prefix semantics

`entry_count` is the number of initialized category entries in a contiguous prefix of one FSM_DATA page.

Valid initialized indices are:

```text
[0, entry_count)
```

with:

```text
0 <= entry_count <= 8144
```

The initialized prefix represents currently existing heap pages beginning at the page's persisted `first_heap_page_no`.

Entries satisfying:

```text
entry_index >= entry_count
```

are uninitialized future-entry storage.

In FSM_DATA v1, every byte in that uninitialized suffix MUST persist as zero.

Structural validation MUST reject a nonzero byte in the uninitialized suffix.

Entry access/update operations MUST reject an index outside the initialized prefix.

A later relation-wide FSM owner MAY grow the initialized prefix as additional heap pages are allocated. That ownership/lifecycle policy is separate from the persisted page format.

## 6.8 Blank FSM_DATA page initialization

Initializing a new FSM_DATA page first deterministically zeroes the entire 8192-byte page and then writes the explicit common and FSM-specific headers.

Before WAL integration, common-header defaults are:

```text
page_type       = FSM_DATA
format_version  = 1
header_size     = 48
page_no         = actual FSM PageId.page_no
flags           = 0
page_lsn        = INVALID_LSN unless explicitly supplied
checksum_crc32c = 0
reserved16      = 0
```

FSM-specific initialization writes:

```text
first_heap_page_no =
    deterministic value derived from FSM page number

entry_count =
    caller-selected initialized prefix length

reserved16 = 0
reserved32 = 0
```

`entry_count` is subject to the `0..8144` invariant in §6.7.

All category bytes initially remain zero.

Before WAL/recovery integration, ordinary FSM_DATA page mutation does not:

- advance `page_lsn`,
- generate or update a whole-page CRC32C checksum.

Once WAL/recovery is active, the global page-checksum and WAL policies apply.

## 6.9 Runtime FSM acceleration

The relation-wide FSM subsystem MAY maintain an in-memory accelerator derived from persisted FSM information.

The source architecture permits a bucketed representation conceptually shaped as:

```text
bucket[0]
bucket[1]
...
bucket[255]
```

Insertion asks the runtime FSM for a candidate category/page capable of satisfying the requested size.

This in-memory accelerator is rebuildable runtime metadata and is **not** part of the persisted `FSM_DATA` format.

The exact runtime data structure, tie-breaking policy, and search procedure are not locked by legacy §§82–85 and are therefore not invented in this pass.

## 6.10 Advisory, stale, repairable, and rebuildable semantics

FSM state is a performance optimization.

It is not the sole source of insertion correctness.

A candidate path MUST have the form:

```text
candidate page
    ↓
fetch + latch
    ↓
verify actual heap-page free space
```

If the candidate is wrong because the FSM information is stale, the engine repairs the FSM entry as appropriate and retries.

A stale category may cause:

- an unnecessary candidate-page fetch,
- a usable page to be temporarily overlooked,
- later FSM repair/rebuild work.

It MUST NOT cause insertion to bypass actual `HeapPage` free-space verification.

Recovery MAY tolerate stale FSM category information.

At startup or during maintenance, the engine MUST be able to repair or rebuild FSM state by scanning heap-page headers.

This allows free-space hint maintenance to remain outside the critical durability path when the recovery contract permits it.

The flat persisted v1 FSM is authoritative only for:

- deterministic heap-page-to-entry addressing,
- initialized-prefix metadata,
- category-byte interpretation,
- FSM page-format validation.

It is **not** authoritative for whether a heap insertion can currently succeed.

## 6.11 Heap-page compaction boundary

Heap-page compaction MAY move physical tuple bytes within one page.

Compaction MUST:

- hold the page's exclusive latch,
- update affected slot offsets as one latch-protected page mutation,
- preserve every `SlotId`,
- therefore preserve every RID,
- retain all non-reclaimed `NORMAL` tuple contents,
- obey the canonical `DEAD`-slot state rules.

Compaction MAY physically discard tuple bytes only for a slot that is already persistently:

```text
DEAD
```

The page layer does not decide whether a tuple version is globally safe to transition into that state.

After discarding a `DEAD` payload, compaction canonicalizes the slot to:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved
```

The following remain unchanged:

```text
slot_count
slot-directory positions
SlotId values
```

Compaction increases contiguous free space but does not make a `DEAD` slot reusable.

Retained `NORMAL` tuple ranges MUST be non-overlapping for compaction to proceed.

V1 MAY enforce this non-overlap condition specifically as a compaction precondition rather than as a universal `HeapPage` structural-validation rule until broader validation semantics are specified.

Compaction MUST NOT reclaim an MVCC-dead-looking tuple merely from local page evidence.

Only the vacuum/global visibility protocol may decide that a physical version is safe to remove.

## 6.12 Vacuum physical-reclamation boundary

Vacuum ultimately coordinates physical reclamation across visibility, indexes, heap pages, compaction, and free-space metadata.

The conceptual sequence is:

```text
1. determine global safe visibility horizon
2. identify tuple versions dead to all relevant snapshots
3. remove corresponding index entries when required
4. mark heap slots reclaimable/dead
5. compact pages where useful
6. update FSM
```

This sequence defines subsystem responsibilities, not the final crash-safe WAL ordering.

The exact crash-safe ordering is owned by the later WAL/recovery and vacuum protocols.

A heap page that becomes completely empty remains reusable database space.

The engine is not required to shrink the underlying operating-system file merely because a page becomes empty.

Physical slot reuse remains subject to the later safe RID-reuse protocol; vacuum eligibility and page compaction alone do not implicitly authorize immediate RID reuse.

The same rule applies to whole-page recycling: an empty heap page may remain reusable database space, but reinitializing its `PageNo` in a way that reuses former `(PageNo, SlotId)` identities requires the later physical RID-reuse safety protocol.

## 6.13 FSM and reclamation invariants

1. Each heap relation has a separate page-based FSM file.
2. FSM page `0` is an FSM FileSuperblock; `FSM_DATA` pages begin at page `1`.
3. Each ordinary heap data page maps deterministically to one one-byte FSM category.
4. Category values are exactly `0..255`, with larger values representing greater conservative insertion capacity.
5. The v1 category assumes an 8-byte new-slot cost and caps usable tuple payload at 8135 bytes.
6. `category = floor(usable_insertion_bytes * 255 / 8135)`.
7. `minimum_usable(c) = ceil(c * 8135 / 255)`.
8. FSM_DATA v1 is exactly 8192 bytes with a 48-byte total header and 8144 one-byte entries.
9. FSM common `reserved16`, FSM `reserved16`, and FSM `reserved32` are zero in v1.
10. FSM page/entry mapping arithmetic is checked for overflow.
11. `first_heap_page_no` is derived deterministically from the physical FSM page number.
12. `entry_count` is a contiguous initialized-prefix length in `0..8144`.
13. Every uninitialized suffix byte is zero.
14. Actual heap-page geometry, not FSM metadata, determines insertion correctness.
15. FSM state is repairable/rebuildable from heap-page headers.
16. Page compaction preserves SlotIds/RIDs.
17. Compaction discards payload only from already-persisted `DEAD` slots.
18. A compacted `DEAD` slot remains `DEAD` and non-reusable.
19. Global vacuum/visibility logic, not HeapPage compaction, decides reclamation eligibility.
20. Empty heap pages remain reusable database space; file shrinking is not required.
---

# 7. I/O and Buffer Management

## 7.1 Scope

This chapter defines the boundary between raw page-file I/O and database-managed resident pages.

It covers:

- `DiskManager` responsibilities,
- positional page I/O and file-size/error semantics,
- BufferPool ownership,
- resident-frame metadata,
- RAII page guards,
- resident-page lookup,
- pinning and eviction eligibility,
- dirty-page state,
- WAL-before-data enforcement,
- CLOCK replacement,
- lifetime safety for references into resident page bytes.

The BufferPool is intentionally storage-format agnostic: it manages pages, not tuples, indexes, schemas, or SQL semantics.

Later WAL/recovery chapters refine the buffer metadata and flush protocol where recovery requires additional state.

## 7.2 Explicit I/O model

The baseline page-file path uses explicit positional I/O:

```text
pread
pwrite
fdatasync
```

Database pages are not primarily managed through `mmap`.

Explicit I/O keeps control over:

- page residency,
- replacement,
- dirty writeback,
- WAL-before-data ordering,
- page lifetime,
- later prefetch/writeback policy.

Direct I/O, `io_uring`, asynchronous prefetch, and similar alternatives remain deferred experiments rather than baseline requirements.

## 7.3 DiskManager responsibility boundary

`DiskManager` is deliberately ignorant of database logical/physical page contents.

It owns raw operating-system/file concerns including:

- database file paths,
- mapping database `FileId` values to open process-local handles,
- fixed-size page reads and writes,
- raw file creation/open/close,
- file-size discovery,
- raw durable file synchronization,
- explicit file extension.

It does **not** interpret:

- tuples,
- schemas,
- MVCC,
- B+ tree algorithms,
- SQL,
- query plans.

The exact C++ API is not part of the architecture contract.

The required responsibility shape is equivalent to:

```text
read one already-allocated page
write one already-allocated page
extend one managed page file by one page
synchronize a page file
open/create/register raw files
close raw files
inspect file size
```

The legacy conceptual interface included a `SyncWal()` operation. The later WAL architecture assigns WAL writing, durability scheduling, and `durable_lsn` advancement to the WAL writer/flusher. `DiskManager` therefore does not own transaction-level WAL durability coordination merely because the early conceptual API showed such a method.

### 7.3.1 File lifecycle boundary

Creating an operating-system file and initializing a database file superblock are separate operations.

The raw disk layer MAY create/register an empty file, but it does not:

- choose `FileKind`,
- encode page `0`,
- interpret file-kind-specific metadata,
- establish higher-level persistent object identity.

A higher page-file/storage layer allocates and writes the superblock and performs persistent-format/identity validation.

`FileId` remains a database-level logical identifier.

POSIX file descriptors are private process-local resources of the raw I/O layer and MUST NOT escape as persistent identity.

## 7.4 Page-file I/O semantics

### 7.4.1 Positional access

Page reads and writes use positional I/O.

`pread` and `pwrite` MUST NOT depend on or mutate a shared file offset.

### 7.4.2 Complete page transfers

A normal page read requests exactly:

```text
PAGE_SIZE
```

bytes.

The I/O layer MUST handle short reads and writes explicitly; one syscall is not assumed to transfer all requested bytes.

For reads:

- EOF before the requested page begins is a missing-page error.
- EOF after only part of the page was transferred is a short-read/corruption-style I/O error.
- a failed read MUST NOT expose a partially initialized destination page as a valid page result.

For writes, the implementation MUST complete the full page transfer or return an error.

### 7.4.3 EINTR behavior

The following operations are retried when interrupted by `EINTR`, subject to their normal Linux semantics:

```text
pread
pwrite
fstat
ftruncate
fdatasync
```

`close` MUST NOT be blindly retried after `EINTR`, because the descriptor may already have been released and reused.

### 7.4.4 Allocation boundary

`WritePage` writes only already-allocated pages.

It MUST NOT:

- implicitly extend the file,
- create sparse page holes,
- combine ordinary page replacement with allocation.

Allocation uses the explicit append-first extension contract in §4.11.

### 7.4.5 File-size validity

A page-file size MUST be an exact multiple of:

```text
PAGE_SIZE
```

A misaligned size is an error.

The raw disk layer MUST NOT round, truncate, or silently repair a misaligned page-file size.

### 7.4.6 Checked physical offsets

Before page I/O, physical offset arithmetic MUST be checked so the complete `PAGE_SIZE` page extent is representable in the platform's positional-I/O offset type.

Arithmetic MUST NOT wrap.

### 7.4.7 Error context

An I/O failure must preserve enough context to identify at least:

```text
file
page
operation
errno
```

when those concepts apply to the failed operation.

### 7.4.8 Page-file durability primitive

The v1 page-file synchronization primitive is:

```text
fdatasync
```

with `EINTR` handling as above.

A later architecture revision MAY require stronger metadata durability from `fsync`, but the raw page-file baseline does not require it.

## 7.5 BufferPool ownership

The BufferPool owns the resident mapping:

```text
PageId -> resident buffer frame
```

and is responsible for:

- caching,
- pin acquisition/release,
- dirty-state tracking,
- page latches,
- replacement eligibility,
- eviction,
- page flushing,
- WAL-before-data coordination.

The BufferPool MUST NOT parse heap tuples, physical schemas, or B+ tree keys.

Normal page-format objects operate over bytes supplied through BufferPool-managed lifetime once the buffer layer exists.

## 7.6 Buffer frame

A resident buffer frame contains at least the conceptual state:

```text
aligned 8192-byte page bytes
PageId
pin count
dirty flag
reference/replacement metadata
page latch
I/O state
```

Frame metadata is process-local and MUST NOT be stored inside the persistent database page merely because it describes the resident copy.

The page byte storage SHOULD be suitably aligned for efficient I/O and memory copying.

Frequently modified frame metadata SHOULD avoid pathological false sharing where measurement shows contention.

This list is a baseline rather than an exhaustive recovery-era frame layout. Later durability/checkpointing chapters may require additional process-local metadata such as dirty-page recovery state.

## 7.7 RAII page guards

Resident page lifetime and page-latch ownership use RAII-style guards.

The architecture distinguishes conceptual read/write guard capabilities equivalent to:

```text
ReadPageGuard
WritePageGuard
```

A guard:

- owns one pin while alive,
- acquires the appropriate page latch for its access mode,
- releases its latch and pin during destruction/release,
- provides access to the resident page bytes,
- for mutable access, participates in marking the frame dirty after mutation.

Callers SHOULD NOT be required to manually pair raw `FetchPage`/`UnpinPage` operations across every success and error path.

### 7.7.1 Reference lifetime safety

A pointer, reference, span, tuple view, iterator state, or other non-owning reference into a frame's page bytes MUST NOT outlive the guard/pin that keeps the frame resident.

The architecture SHOULD make the following invalid pattern difficult to express:

```text
fetch/pin page
obtain pointer into page
release pin
frame becomes evictable/reused
dereference old pointer
```

Storage may expose zero-copy views only while this lifetime relation is preserved.

## 7.8 Resident-page table

Resident pages are found through a hash-based mapping:

```text
PageId -> FrameId
```

The initial synchronization MAY use a conventional hash map protected by a mutex.

The abstraction MUST allow later replacement/partitioning such as:

- sharded hash tables,
- concurrent maps,
- partitioned locks.

A lock-free page table is not required by the baseline architecture.

## 7.9 Pinning and eviction eligibility

A frame with:

```text
pin_count > 0
```

is not eligible for eviction or frame reuse.

One page guard owns one pin.

Background flushing MAY write a pinned page only under safe BufferPool synchronization rules.

A flush does not make a still-pinned frame eligible for replacement.

The implementation must make pin leaks observable/testable because a leaked pin permanently removes a frame from the eviction candidate set.

## 7.10 Dirty pages

Mutating a page through mutable/write access marks the resident frame dirty.

A dirty page MAY be written:

- during eviction,
- by an explicit flush,
- by a later background writer.

Transaction commit does not require dirty heap/index data pages to be written because the architecture is NO-FORCE.

A successful physical write may clear dirty state only under synchronization that establishes the written image was not superseded by a newer in-memory mutation. The exact recovery-era dirty-state/`rec_lsn` transition is defined by the later checkpoint/recovery contract.

## 7.11 WAL-before-data enforcement

The BufferPool flush path is the centralized WAL-before-data enforcement point for pages routed through it.

Before writing a WAL-protected dirty page whose page header contains:

```text
page_lsn = X
```

the flush path MUST establish:

```text
WalManager.durable_lsn >= X
```

Conceptually:

```text
if WalManager.durable_lsn < page.page_lsn:
    WalManager.flush_through(page.page_lsn)

write page through raw page-file I/O
```

This ordering requirement applies to all BufferPool data-page write paths, including:

- explicit flush,
- eviction,
- background writeback.

Storage objects MUST NOT each implement independent variants of the WAL-before-data rule.

The WAL subsystem owns WAL persistence and `durable_lsn`; the BufferPool owns checking/enforcing the dependency before data-page writeback.

Later WAL/recovery chapters may impose stronger temporary no-flush conditions for multi-page structural operations.

## 7.12 CLOCK replacement

The baseline replacement policy is CLOCK.

A frame participates as an eviction candidate only when:

```text
pin_count == 0
```

Accessing a frame sets its reference/use bit.

The clock hand applies:

```text
pinned?       -> skip
referenced?   -> clear reference bit and skip
otherwise     -> victim
```

A dirty victim MUST be flushed according to the WAL-before-data and dirty-state synchronization rules before its frame can be reused.

CLOCK operates only on process-local frame/replacement metadata. It does not inspect database tuple or index contents.

The replacement abstraction SHOULD remain separable so alternatives can be measured later.

Deferred replacement experiments include:

- CLOCK-Pro,
- LRU-K,
- 2Q,
- scan-resistant policies.

## 7.13 I/O and buffer invariants

1. Page I/O is positional and does not rely on a shared file offset.
2. A normal page read/write transfers exactly `PAGE_SIZE` bytes or reports an error.
3. A failed read never exposes a partial destination as a valid page.
4. `pread`, `pwrite`, `fstat`, `ftruncate`, and `fdatasync` handle `EINTR`; `close` is not blindly retried.
5. Ordinary page writes never allocate or sparsely extend files.
6. Page-file sizes are exact multiples of `PAGE_SIZE`.
7. Physical page-offset arithmetic is checked before I/O.
8. V1 page-file durable synchronization uses `fdatasync`.
9. `FileId` is not an operating-system descriptor.
10. BufferPool owns the `PageId -> resident frame` mapping and page lifetime.
11. Frame-management metadata is not persisted inside page bytes.
12. `pin_count > 0` forbids eviction/frame reuse.
13. One page guard owns one pin.
14. References into resident page bytes do not outlive their protecting guard/pin.
15. Mutable page access marks the frame dirty.
16. NO-FORCE means commit does not require writing dirty heap/index data pages.
17. Before writing a WAL-protected page with `page_lsn=X`, BufferPool ensures `durable_lsn >= X`.
18. CLOCK considers only unpinned frames for eviction.
19. A referenced CLOCK candidate receives a second chance by clearing its reference bit.
20. Dirty CLOCK victims are safely flushed before frame reuse.
---

# Part III — Indexing

# 8. B+ Tree Indexing

## 8.1 Scope and subsystem role

The primary general-purpose persistent index is a page-backed B+ tree.

The production tree operates through the real BufferPool and MUST NOT be implemented as a separate in-memory pointer tree whose persistence is added later.

The v1 architecture supports:

- equality lookup,
- ordered forward range scans,
- composite keys,
- variable-length VARCHAR keys,
- duplicate SQL user keys,
- SQL UNIQUE indexes enforced transactionally above the physical tree,
- concurrent readers and writers,
- leaf and internal splits,
- redistribution and merge,
- root replacement and contraction,
- tree-local page reuse,
- physical heap tuple-version RIDs,
- WAL/recovery integration.

The B+ tree owns physical ordered index structure.

It does not own SQL parsing, snapshot visibility, or user-transaction uniqueness semantics.

## 8.2 Index file and superblock metadata

Each index owns one page-based file:

```text
index_<index_id>.btree
```

with:

```text
page 0       B+ tree superblock
page 1..N    internal / leaf / free pages
```

The file uses the `BTREE` file kind defined by the persistent-storage foundation.

The B+ tree superblock is a file-kind-specific extension of the common 72-byte FileSuperblock prefix.

The common prefix stores:

```text
file_kind   = BTREE
object_id   = IndexId
header_size = 128
```

The B+ extension occupies bytes `72..127`.

### 8.2.1 BTREE superblock extension v1

| Page offset | Size | Field / required v1 meaning |
|---:|---:|---|
| `72` | 8 | `table_id` |
| `80` | 8 | `root_page_no` |
| `88` | 8 | `first_leaf_page_no` |
| `96` | 8 | `last_leaf_page_no` |
| `104` | 8 | `free_page_head` |
| `112` | 8 | `key_schema_fingerprint` |
| `120` | 4 | `key_schema_version = 1` |
| `124` | 2 | `tree_height` |
| `126` | 2 | `index_flags = 0` |
| `128` | 8064 | reserved bytes, all zero |
|  | **8192** | **total page** |

All multi-byte fields are little-endian.

`index_flags` has no assigned v1 bits. V1 encoders MUST write zero and v1 decoders MUST reject nonzero `index_flags`.

The B+ tree does not duplicate `IndexId` in the extension; the common FileSuperblock `object_id` is the index identifier.

`root_page_no`, `first_leaf_page_no`, and `last_leaf_page_no` MUST be valid non-sentinel page numbers in an initialized tree.

`free_page_head` uses:

```text
INVALID_PAGE_NO
```

for an empty B+ free list.

An initialized empty tree is represented by one valid empty leaf:

```text
tree_height = 1
root_page_no = one empty leaf page
first_leaf_page_no = root_page_no
last_leaf_page_no  = root_page_no
root.slot_count    = 0
```

A normal initialized empty tree MUST NOT use an invalid root.

The v1 B+ superblock extension above is now byte-exact.

Its `key_schema_fingerprint` is defined in §8.3.1 so the persisted metadata can be validated independently of compiler enums or ABI layout.

## 8.3 Index key schema

Each B+ tree has one fixed logical key schema supplied by catalog metadata and fingerprinted in the B+ tree superblock.

Version 1 supports:

```text
ascending key components
NULLS FIRST
binary VARCHAR collation
```

Native descending physical index components and locale-aware collations are deferred.

A descending SQL result may initially be produced by executor sorting rather than native reverse index traversal.

The B+ tree's key-order semantics MUST agree with the database's SQL comparison semantics for the supported indexable types.

### 8.3.1 Key-schema fingerprint v1

The B+ superblock persists:

```text
key_schema_version     = 1
key_schema_fingerprint = FNV-1a-64(canonical_descriptor)
```

The 64-bit FNV-1a parameters are:

```text
offset_basis = 14695981039346656037
prime        = 1099511628211
```

For each descriptor byte:

```text
hash = hash XOR byte
hash = hash * prime mod 2^64
```

The canonical descriptor bytes are:

```text
ASCII "DBLUS-INDEX-KEY-SCHEMA-V1"
0x00
component_count as uint32 little-endian
then, for each key component in schema order:
    1 byte physical type code
    1 byte order code
    1 byte null-order code
    1 byte collation code
```

Fingerprint-only v1 physical type codes are:

```text
1 BOOLEAN
2 INT32
3 INT64
4 FLOAT64
5 DATE
6 TIMESTAMP
7 VARCHAR
```

V1 order/null/collation codes are:

```text
order:
    0 = ASC

null order:
    0 = NULLS_FIRST

collation:
    0 = NONE / not applicable
    1 = BINARY
```

For VARCHAR, v1 requires `collation = BINARY`.

For non-VARCHAR components, v1 requires `collation = NONE`.

These codes define only the canonical fingerprint descriptor; they do not replace the catalog's logical type representation.

The fingerprint is a deterministic mismatch detector, not a cryptographic identity. Opening an index MUST reject a mismatch between the expected key-schema descriptor and the persisted fingerprint.

## 8.4 User keys, physical keys, and RID encoding

The architecture distinguishes:

```text
user key
    SQL-visible indexed-column tuple

physical key
    (user key, RID)
```

RID is a deterministic tiebreaker.

Consequently, every physical B+ tree entry has a unique total position even when multiple SQL rows have equal user keys.

This physical-key choice simplifies:

- duplicate-key representation,
- duplicate ranges spanning leaves,
- split separators,
- lower/upper-bound search,
- exact physical deletion.

### 8.4.1 Persisted RID encoding

An RID stored in a B+ tree entry occupies exactly 16 bytes:

| RID-relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `heap_file_id` |
| `4` | 8 | `heap_page_no` |
| `12` | 2 | `heap_slot_id` |
| `14` | 2 | reserved |
|  | **16** | **total** |

Multi-byte fields use the persistent integer byte order defined by the storage foundation.

Physical RID ordering is numeric lexicographic order:

```text
(heap_file_id, heap_page_no, heap_slot_id)
```

For v1, bytes `14..15` are strict reserved-zero bytes:

```text
encoder:
    MUST write zero

decoder:
    MUST reject nonzero values
```

A persisted leaf RID MUST also satisfy:

```text
heap_file_id != INVALID_FILE_ID
heap_page_no != INVALID_PAGE_NO
heap_slot_id != INVALID_SLOT_ID
```

A context-free RID decoder can validate these sentinel rules.

When the index handle knows the expected heap `FileId`, tree/index-level validation MUST additionally reject a leaf RID whose `heap_file_id` does not belong to the indexed relation.

This strict decoder rule is part of the architecture contract even though the Phase 1 implementation checkpoint recorded a decoder that still accepted nonzero reserved bytes.

## 8.5 Memcomparable user-key encoding

`IndexKeyCodec` converts logical indexed values into an order-preserving byte sequence.

For supported user keys `A` and `B`:

```text
lexicographic_compare(Encode(A), Encode(B))
```

MUST produce the same ordering as the database's index comparator.

The B+ tree therefore compares encoded bytes in the hot path rather than invoking a polymorphic SQL comparator for each binary-search comparison.

The index encoding is allowed to differ from ordinary tuple serialization because its purpose is order preservation, not compact row storage.

### 8.5.1 Field presence prefix

Every encoded field begins with:

```text
0x00 = NULL
0x01 = non-NULL
```

which establishes ascending `NULLS FIRST`.

A NULL field consists of the NULL marker for that component; non-NULL fields continue with the type-specific encoding below.

### 8.5.2 BOOLEAN

For a non-NULL BOOLEAN, the value byte is:

```text
false -> 0x00
true  -> 0x01
```

### 8.5.3 INT32 and DATE

Interpret the signed value as a 32-bit bit pattern and flip the sign bit:

```text
sortable =
    bits(value) XOR 0x80000000
```

Then encode `sortable` big-endian.

### 8.5.4 INT64 and TIMESTAMP

Interpret the signed value as a 64-bit bit pattern and flip the sign bit:

```text
sortable =
    bits(value) XOR 0x8000000000000000
```

Then encode `sortable` big-endian.

### 8.5.5 FLOAT64

Before encoding FLOAT64, normalize the IEEE-754 binary64 payload bits:

```text
-0.0 and +0.0
    -> 0x0000000000000000

every NaN
    -> 0x7ff8000000000000
```

Let `u` be the normalized 64-bit payload.

Compute:

```text
if (u & 0x8000000000000000) != 0:
    sortable = bitwise_not(u)
else:
    sortable = u XOR 0x8000000000000000
```

where `bitwise_not` and the XOR operate on exactly 64 bits.

Encode `sortable` as an unsigned 64-bit integer in big-endian byte order.

This yields the required total order:

```text
-infinity
...
negative finite values
...
zero
...
positive finite values
...
+infinity
NaN
```

All NaNs compare equal for v1 index-key semantics because they normalize to the same canonical payload.

The SQL FLOAT64 comparison layer MUST use the same equality/order semantics.

### 8.5.6 VARCHAR

Version 1 uses binary bytewise collation.

Encode payload bytes as:

```text
ordinary non-zero byte -> itself
0x00                   -> 0x00 0xFF
```

Terminate the VARCHAR field with:

```text
0x00 0x00
```

This makes the field self-delimiting while preserving binary lexicographic byte-string ordering.

### 8.5.7 Composite keys

Composite user keys concatenate encoded component fields in physical key-schema order:

```text
field0 || field1 || ... || fieldN
```

The combined user-key byte string remains lexicographically order preserving.

No per-comparison type dispatch is required inside the B+ tree.

## 8.6 Encoded-key size and physical comparison

The maximum encoded **user-key** size in v1 is:

```text
1024 bytes
```

An operation whose encoded user key exceeds this bound MUST fail explicitly.

The key MUST NOT be silently truncated.

The bound exists to preserve useful fanout on 8192-byte pages and avoid overflow-key pages in v1.

Physical-key comparison is:

```text
1. lexicographically compare encoded user-key bytes
2. if equal, numerically compare RID
```

Conceptual search-only sentinels:

```text
MIN_RID
MAX_RID
```

bound the complete duplicate range for user key `K`:

```text
(K, MIN_RID) ... (K, MAX_RID)
```

These search sentinels need not be persistable real RIDs.

Deferred key-space extensions include overflow keys, key compression, separator-prefix truncation, and alternative page sizes.

## 8.7 Node-page organization

Leaf and internal nodes are 8192-byte slotted pages using:

```text
32-byte common page header
64-byte total node header
8-byte slot descriptors
variable-length packed entries
```

The slot directory begins at byte `64` and grows upward.

Packed entry bytes grow downward from the end of the page.

Conceptually:

```text
0
┌──────────────────────────────┐
│ common page header           │
│ B+ tree node header          │
├──────────────────────────────┤
│ slot 0                       │
│ slot 1                       │
│ ...                          │
│                        lower │
├──────────────────────────────┤
│ contiguous free space        │
├──────────────────────────────┤
│ upper                        │
│ packed entry bytes           │
│ ...                          │
└──────────────────────────────┘
8192
```

For v1 B+ leaf/internal pages:

```text
format_version     = 1
header_size        = 64
common flags       = 0
common reserved16  = 0
node flags         = 0
```

V1 encoders MUST write zero and v1 decoders MUST reject nonzero values in these unassigned/reserved fields.

A blank node initializes:

```text
slot_count = 0
lower      = 64
upper      = PAGE_SIZE
```

For every valid node:

```text
lower = 64 + slot_count * 8
64 <= lower <= upper <= PAGE_SIZE
```

The slot array is kept in sorted physical-key order.

Packed entry payload bytes do not need to be physically ordered.

Insertion into the middle of a node SHOULD normally move slot descriptors rather than rewriting all existing variable-length entry payloads.

Page-local compaction MAY repack entry bytes and rewrite slot offsets while holding the page write latch.

## 8.8 Node slot entry

Every B+ tree slot descriptor occupies exactly 8 bytes:

| Slot-relative offset | Size | Field |
|---:|---:|---|
| `0` | 2 | `entry_offset` |
| `2` | 2 | `entry_length` |
| `4` | 2 | `user_key_length` |
| `6` | 2 | `flags` |
|  | **8** | **total** |

The slot `flags` field has no assigned v1 bits.

```text
slot.flags = 0
```

V1 encoders MUST write zero and v1 decoders MUST reject nonzero slot flags.

## 8.9 Leaf-node format

A leaf node has a 64-byte total header:

| Page offset | Size | Field |
|---:|---:|---|
| `0..31` | 32 | common page header |
| `32` | 2 | `level = 0` |
| `34` | 2 | `slot_count` |
| `36` | 2 | `lower` |
| `38` | 2 | `upper` |
| `40` | 4 | `flags` |
| `44` | 8 | `prev_leaf_page_no` |
| `52` | 8 | `next_leaf_page_no` |
| `60` | 4 | reserved |
|  | **64** | **total** |

Leaf level is always:

```text
0
```

A leaf packed entry is:

```text
encoded user key     user_key_length bytes
RID                  16 bytes
```

Therefore:

```text
entry_length = user_key_length + 16
```

The B+ tree stores no:

```text
xmin
xmax
snapshot metadata
```

inside leaf entries.

For v1 leaf pages:

```text
flags                    = 0
reserved bytes 60..63    = 0
no previous leaf         = INVALID_PAGE_NO
no next leaf             = INVALID_PAGE_NO
```

A v1 decoder MUST reject nonzero node flags or reserved bytes.

## 8.10 Internal-node format and routing semantics

An internal node also has a 64-byte total header:

| Page offset | Size | Field |
|---:|---:|---|
| `0..31` | 32 | common page header |
| `32` | 2 | `level > 0` |
| `34` | 2 | `slot_count` |
| `36` | 2 | `lower` |
| `38` | 2 | `upper` |
| `40` | 4 | `flags` |
| `44` | 8 | `leftmost_child_page_no` |
| `52` | 12 | reserved |
|  | **64** | **total** |

An internal node with `N` separator entries has:

```text
N + 1 children
```

Each packed internal entry contains:

```text
encoded user key       variable bytes
separator RID          16 bytes
right_child_page_no     8 bytes
```

Every separator is therefore a complete physical key:

```text
(user key, RID)
```

The full physical separator is required so routing remains correct when equal SQL keys span multiple subtrees.

For v1 internal pages:

```text
flags                 = 0
reserved bytes 52..63 = 0
```

A v1 decoder MUST reject nonzero node flags or reserved bytes.

### 8.10.1 Routing lower bounds

An internal node is represented conceptually as:

```text
C0, (K1 -> C1), (K2 -> C2), ... (Kn -> Cn)
```

with:

```text
all keys in C0 < K1

for each i > 0:
    all keys in Ci >= Ki

and, when K(i+1) exists:
    all keys in Ci < K(i+1)
```

`Ki` is a routing lower bound for child `Ci`.

Immediately after a split it normally equals the minimum physical key in `Ci`.

After ordinary deletion it MAY remain stale-low.

For example, if a right subtree minimum moves from `50` to `60`, retaining separator `50` remains correct so long as every key in that subtree is still `>= 50`.

A stale-low separator may cause an unnecessary descent that finds no key; it MUST NOT cause a real key to be missed.

A separator need not be tightened only because deletion increased the right-child minimum.

A separator MUST change when a structural operation moves entries across its routing boundary, including:

- redistribution left-to-right,
- redistribution right-to-left,
- right-child replacement,
- split installation of a new child.

After redistribution, the simplest correct separator is the first physical key of the new right child.

## 8.11 Node search

### 8.11.1 Internal search

For target physical key `T`, internal routing uses binary search equivalent to:

```text
upper_bound(separators, T)
```

so:

```text
T < K1            -> C0
K1 <= T < K2      -> C1
K2 <= T < K3      -> C2
...
Kn <= T           -> Cn
```

Production internal-node lookup MUST NOT linearly scan separators.

### 8.11.2 Leaf search

Leaf lookup uses binary search equivalent to:

```text
lower_bound(physical_keys, target)
```

Exact physical lookup uses:

```text
target =
    (encoded_user_key, RID)
```

SQL-user-key equality scanning begins at:

```text
target =
    (encoded_user_key, MIN_RID)
```

and proceeds forward until the user key changes.

For a range scan, binary search finds the initial slot once; iteration then proceeds sequentially rather than repeating a binary search for every next entry.

The hot comparison path SHOULD primarily be encoded-byte lexicographic comparison plus RID comparison when encoded user keys are equal.

## 8.12 Split trigger and node compaction

Before splitting because an insertion does not fit:

```text
if contiguous free bytes are insufficient
and existing fragmented/reclaimable bytes would be sufficient:
    compact the node
```

A split occurs only if the pending entry still does not fit after any useful compaction.

Variable-length node capacity, split balance, and occupancy are measured in **bytes**, not entry count.

## 8.13 Leaf split and sibling links

A leaf split:

1. includes the pending new entry in the conceptual sorted entry set,
2. chooses a boundary producing approximately balanced byte usage,
3. leaves at least one entry on each resulting leaf,
4. keeps lower physical keys in the original left page,
5. moves higher physical keys to a new right page,
6. copies the first physical key of the new right leaf into the parent as separator.

The separator remains present in the right leaf.

Splitting solely by entry count is not permitted because encoded keys are variable length.

### 8.13.1 Sibling-link publication

If the pre-split chain is:

```text
P <-> L <-> N
```

then after splitting `L`:

```text
P <-> L <-> R <-> N
```

with:

```text
R.prev = L
R.next = old L.next
L.next = R

if N exists:
    N.prev = R
else:
    superblock.last_leaf = R
```

Both forward and backward links are persisted.

V1 exposes native forward scans; maintaining the backward links does not by itself require native reverse scans.

## 8.14 Internal split

Given:

```text
C0, K1->C1, ... Km->Cm, ... Kn->Cn
```

an internal split chooses promoted physical key `Km` by byte balance.

The left page retains:

```text
C0
K1->C1
...
K(m-1)->C(m-1)
```

The parent receives:

```text
Km
```

The right page receives:

```text
leftmost_child = Cm
K(m+1)->C(m+1)
...
Kn->Cn
```

Unlike leaf splitting, the promoted internal separator is removed from the child level.

## 8.15 Root split and contraction

### 8.15.1 Root split

When the current root splits:

```text
allocate new internal root
old root -> leftmost child
new split page -> right child
install one separator
new_root.level = old_root.level + 1
update root_page_no
update tree_height
```

### 8.15.2 Root contraction

If an internal root reaches zero separator entries:

```text
its single child becomes the root
old root is retired/recycled
tree_height decreases by one
```

If the root is a leaf and becomes empty, that leaf remains the valid empty root and tree height remains `1`.

## 8.16 Occupancy and underflow

For B+ node occupancy:

```text
used_bytes =
    slot_directory_bytes
    +
    packed_entry_bytes
```

measured relative to:

```text
PAGE_SIZE - 64
```

A non-root node becomes a practical rebalance candidate below approximately:

```text
25% byte occupancy
```

This is a soft threshold, not a strict half-full B+ tree invariant.

The softer threshold is intended to reduce split/merge oscillation and write amplification with variable-length keys.

A temporarily sparse page is a performance issue rather than structural corruption.

## 8.17 Redistribution and merge

For an underfull page:

1. inspect an adjacent sibling with the same parent,
2. prefer redistribution when it can restore healthy occupancy,
3. otherwise merge when the combined entries fit in one page,
4. propagate parent underflow when a separator is removed.

Whenever redistribution moves keys across a routing boundary, the relevant parent separator MUST be updated.

Ordinary deletion that does not move keys between children does not require separator tightening.

### 8.17.1 Leaf merge

When possible, leaf merge is deterministic:

```text
merge right into left
```

Update:

```text
left.next = right.next

if right.next exists:
    right.next.prev = left
else:
    superblock.last_leaf = left
```

Remove the separator and child pointer for `right` from the parent, then retire `right`.

### 8.17.2 Internal rebalancing

Internal redistribution/merge is defined in terms of:

```text
children + routing separators
```

rather than blindly moving serialized entry bytes.

The parent separator participates in the child boundary and MUST be transformed consistently.

A clear reconstruction of the affected logical node entries is preferable to clever byte movement until equivalent correctness has been established; later implementation may optimize the physical copying without changing these semantics.

## 8.18 Tree-local free pages and safe reuse

The B+ tree superblock stores:

```text
free_page_head
```

A retired B+ page becomes `BTREE_FREE`.

The v1 free-page format is:

```text
page_type          = BTREE_FREE
format_version     = 1
header_size        = 40
common flags       = 0
common reserved16  = 0
```

| Page offset | Size | Field / required v1 meaning |
|---:|---:|---|
| `0` | 32 | common page header |
| `32` | 8 | `next_free_page_no` |
| `40` | 8152 | reserved bytes, all zero |
|  | **8192** | **total** |

`next_free_page_no` is a uint64 little-endian PageNo.

The terminal free-list link is:

```text
INVALID_PAGE_NO
```

A v1 decoder MUST reject nonzero common flags/common reserved16 or any nonzero byte in the free-page reserved region.

Allocation policy is:

```text
if tree-local free list is non-empty:
    reuse a retired B+ page
else:
    append a page to the index file
```

A global extent allocator is not required solely for B+ tree v1.

The tree-local free-list head and every terminal free-page link use `INVALID_PAGE_NO`.

### 8.18.1 Reuse safety

A retired page may be recycled only after:

- it is detached from every installed parent/root/sibling route that can legally reach it,
- the write latches required for that detachment are held,
- no traversal can retain an unprotected stale page reference.

This relies on latch-coupled traversal and guarded leaf handoff.

Long-lived raw `PageNo` cursors MUST NOT be exposed outside the B+ tree implementation.

## 8.19 Concurrency and latch ordering

Transaction-level logical locks and B+ tree page/metadata latches remain separate mechanisms.

### 8.19.1 Read traversal

Point lookup uses read-latch coupling:

```text
read-latch parent
    ↓
determine child
    ↓
pin + read-latch child
    ↓
release parent
```

The child is pinned and latched before the parent latch is released.

### 8.19.2 Write traversal

Initial insert/delete uses top-down write latch crabbing:

```text
write-latch parent
    ↓
write-latch child
    ↓
if child is safe for the operation:
    release older ancestors
else:
    retain required ancestors
```

For insertion, a node is safe only if the current operation cannot force a split to propagate above it.

For an internal node, this safety test includes possible separator insertion caused by a child split.

For deletion, a node is safe only if the current deletion cannot require redistribution/merge that propagates upward.

The architecture does not require a tree-wide write lock for ordinary writes.

### 8.19.3 Root metadata latch and optimistic root validation

Each tree has a small root-metadata latch protecting process/runtime access to structurally sensitive metadata such as:

- current `root_page_no`,
- current tree height,
- structurally necessary first/last leaf metadata,
- free-list-head metadata.

Runtime root metadata also maintains a process-local monotonically increasing:

```text
root_generation
```

`root_generation` is not persisted and does not participate in crash recovery.

It increments whenever a runtime structural publication changes:

```text
root_page_no
or
tree_height
```

Normal traversal obtains the root optimistically:

```text
1. acquire root-metadata latch
2. read candidate root_page_no and root_generation
3. pin candidate root while metadata still identifies it
4. release root-metadata latch

5. acquire the required latch on the pinned candidate root

6. while holding the candidate root page latch:
       acquire root-metadata latch
       validate:
           root_page_no == candidate
           root_generation == captured generation
       release root-metadata latch

7. if validation failed:
       release page latch/pin
       restart from the root
```

A thread MUST NOT wait to acquire a B+ page latch while holding the root-metadata latch.

This is the critical anti-deadlock rule for root acquisition.

A structural operation that already holds the relevant B+ page latch(s) MAY acquire the root-metadata latch to publish a new root/height or other protected metadata.

Thus the waiting order for operations that hold page latches is:

```text
B+ page latch(es)
    ->
root-metadata latch
```

while root acquisition uses the metadata latch only to snapshot/pin identity and releases it **before** waiting on the page latch.

Root replacement publication increments `root_generation` before releasing the metadata latch.

The old root page cannot become reusable merely because the superblock/runtime root pointer changed; normal B+ safe-detachment/page-reuse rules still apply.

### 8.19.4 Free-list and endpoint metadata updates

Operations that need `free_page_head`, `first_leaf_page_no`, or `last_leaf_page_no` and also need a B+ page latch use optimistic validate/update rather than holding the metadata latch while waiting on that page.

For example, popping a free page conceptually uses:

```text
1. snapshot free_page_head under metadata latch
2. release metadata latch
3. pin + write-latch the candidate BTREE_FREE page
4. read candidate.next_free_page_no
5. acquire metadata latch while still holding the page latch
6. validate free_page_head still equals candidate
7. if valid:
       publish free_page_head = candidate.next_free_page_no
   else:
       release and restart
```

Equivalent validation/restart applies when first/last leaf metadata must be changed concurrently.

### 8.19.5 Page-latch acquisition order

Vertical acquisition order is:

```text
parent before child
```

When adjacent leaf pages must be latched horizontally, acquisition order is:

```text
left to right in key order
```

If an operation already holding a later/right page discovers it would need to wait for an earlier/left page:

```text
release and restart
```

rather than waiting in the reverse order.

This rule avoids a major class of structural deadlocks.

## 8.20 Forward range scans and cursor lifetime

V1 natively supports ascending B+ tree range scans.

A forward cursor conceptually retains:

```text
current ReadPageGuard
current slot index
encoded upper bound
upper-bound inclusivity
```

Any tuple/key view derived from the current leaf MUST NOT outlive the current leaf page guard.

At the end of leaf `L`, handoff is latch coupled:

1. while holding `L` read latch, read `L.next`,
2. if no next page exists, finish,
3. pin and read-latch the next leaf,
4. validate the next page type and leaf level,
5. release `L`,
6. continue.

Holding the current leaf until the next leaf is safely pinned/latched prevents a concurrent merge from detaching and reusing the target page during handoff.

### 8.20.1 Reverse scans

`prev_leaf_page_no` is maintained from the initial format, but native descending/reverse scans are deferred.

Bidirectional latch coupling introduces additional deadlock-order concerns.

A later reverse-cursor design may use restart or nonblocking-latch techniques.

## 8.21 B+ tree and IndexKeyCodec API boundaries

Physical B+ tree operations provide responsibilities equivalent to:

```text
Insert(encoded_user_key, RID)
Erase(encoded_user_key, RID)

FindPhysical(encoded_user_key, RID)
LowerBound(encoded_user_key, RID-bound)
Scan(lower_bound, upper_bound)
```

A convenience user-key lookup may be implemented as a physical range:

```text
(K, MIN_RID) ... (K, MAX_RID)
```

Exact C++ method names are not part of the architecture contract.

The physical B+ tree does not own:

- SQL AST nodes,
- tuple visibility,
- user-transaction uniqueness policy.

`IndexKeyCodec` owns:

- logical indexed values -> memcomparable user-key bytes,
- type-specific order-preserving encoding,
- NULL ordering,
- composite-key encoding,
- key-size validation,
- FLOAT64 canonicalization,
- v1 binary VARCHAR collation.

The tree receives opaque encoded user-key bytes plus RIDs.

## 8.22 Duplicate keys, uniqueness, and MVCC

The physical tree always permits duplicate SQL user keys because physical keys differ by RID.

Entries with one user key appear consecutively in RID order.

This remains true for a SQL UNIQUE index because obsolete, aborted, and in-progress tuple versions can coexist physically.

### 8.22.1 Transactional uniqueness boundary

Uniqueness MUST NOT be enforced as:

```text
if BTree.Contains(user_key):
    reject
```

Matching physical entries may reference:

- globally dead versions,
- aborted versions,
- versions created by the current transaction,
- in-progress conflicting transactions.

The architectural shape is:

```text
acquire logical unique-key protection
    ↓
scan all physical entries with the user key
    ↓
fetch referenced heap versions
    ↓
consult transaction/MVCC state
    ↓
reject or permit insertion
```

The exact unique-key lock identity, acquisition order, duration, and conflict rules belong to the transaction/locking chapter.

For v1 SQL UNIQUE semantics:

```text
if any indexed key component is NULL:
    duplicate user keys are allowed
```

Only fully non-NULL user keys participate in duplicate rejection.

NULL-containing keys are still physically encoded, ordered, and stored normally.

### 8.22.2 Visibility

An index entry contains:

```text
encoded user key -> RID
```

while MVCC metadata remains in the heap tuple version.

Therefore an ordinary index scan is:

```text
B+ candidate
    ↓
RID
    ↓
heap fetch
    ↓
MVCC visibility check
    ↓
return or reject
```

A physical B+ tree hit never proves that a SQL row is visible.

Because visibility remains heap-owned, arbitrary index-only scans are deferred until additional machinery such as all-visible metadata and covering payloads exists.

## 8.23 UPDATE, DELETE, vacuum, and aborted user DML

With heap-version MVCC:

### UPDATE

A baseline UPDATE creates:

```text
new heap tuple version
    ↓
new RID
    ↓
new physical (key, RID) index entry
```

Even if indexed column values are unchanged, v1 creates the new physical `(key, new_RID)` entry.

Old physical entries remain until vacuum.

### DELETE

Logical SQL deletion updates heap MVCC metadata.

It does not synchronously remove every corresponding B+ tree entry.

### Vacuum

Before physically reclaiming a globally dead heap tuple version, vacuum can derive each indexed user key and erase the exact physical entry:

```text
Erase(encoded_user_key, dead_RID)
```

RID participation makes this exact even among duplicate user keys.

### Aborted user DML

The later locked v1 transaction/recovery architecture refines the earlier B+ tree discussion: ordinary user-DML heap/index modifications are not physically undone on user abort.

Consequently, an aborted INSERT/UPDATE may leave an index entry that references an invisible aborted heap version.

Vacuum removes such physical garbage later.

This refinement does not change the physical ordering or B+ structural invariants.

HOT-like update behavior that avoids some new index entries remains deferred.

## 8.24 BufferPool integration and index-scan cost

Root and internal B+ pages remain ordinary BufferPool pages.

The B+ tree MUST NOT create a second private cache for upper tree levels.

Frequently accessed root/internal pages should become resident naturally through the database replacement policy.

If future profiling justifies special treatment, it must remain coordinated through BufferPool policy rather than bypassing page-lifetime/flush ownership.

Secondary-index order does not imply heap-page locality.

A range scan may therefore generate many random heap-page accesses.

The optimizer MUST model this cost rather than assuming that the presence of an index automatically makes an index scan cheaper.

Potential later execution/storage improvements include RID batching, heap-page-aware fetching where semantics permit, covering indexes, clustered storage, and prefetch.

## 8.25 Structural modification and WAL boundary

A logical user index action is distinct from a B+ tree structural modification.

For example:

```text
user INSERT
    logical effect:
        create physical (key, RID) entry

    possible structural effects:
        split leaf
        split internal node
        replace root
```

A structurally valid split/merge/root shape is not conceptually rolled back merely because the owning user transaction later aborts.

The later WAL/recovery contract refines the exact mechanism by treating physical B+ mutations as recovery-safe B+ mini-transactions/system structural actions.

Chapter 8 does not duplicate that WAL record format or no-flush protocol; those are owned by the durability/recovery chapters.

### 8.25.1 Page LSN and WAL-before-data participation

Every persistent B+ tree modification participates in page-LSN/WAL ordering, including:

- leaf insertion/deletion,
- internal insertion/deletion,
- split,
- merge,
- redistribution,
- sibling-link change,
- root change,
- free-list change,
- B+ tree superblock change.

BufferPool remains the data-page enforcement point:

```text
durable WAL >= page.page_lsn
before
dirty page reaches the data file
```

The later B+ mini-transaction contract may impose a stronger temporary no-flush condition while one structural/mutation unit is being constructed.

## 8.26 Runtime structural publication

Concurrent runtime operations MUST NOT observe:

- a child pointer to an uninitialized page,
- a sibling pointer to an uninitialized page,
- a root pointer to an uninitialized page,
- a detached child before replacement routing is installed.

New pages are initialized before publication.

Routing pointers are published only while the required structural latches are held.

Runtime atomicity is provided by latch and publication ordering.

Crash atomicity is provided by the later WAL/recovery protocol.

## 8.27 Page validation and corruption handling

Opening a B+ node validates at least:

```text
page type
format version
self page_no
header size
level/type consistency
64 <= lower <= upper <= PAGE_SIZE
lower = 64 + slot_count * 8
slot-directory bounds
entry offsets/lengths
leaf entry_length = user_key_length + 16
internal entry_length = user_key_length + 24
child PageNo validity where applicable
```

All size/offset arithmetic used by these checks MUST be overflow-checked before dereferencing entry bytes.

Debug/verifier configurations SHOULD additionally validate sorted order.

Invalid persistent bounds or malformed node metadata MUST produce a controlled corruption error rather than undefined behavior.

V1 node parsing additionally enforces the exact format/header/zero-field rules in §§8.7–8.10 and the free-page rules in §8.18.

## 8.28 Full-tree verifier

The architecture requires an explicit full-tree verifier capable of checking at least:

1. every leaf is level `0`,
2. every internal child is exactly one level below its parent,
3. all leaves occur at the same depth,
4. leaf physical keys are sorted,
5. internal separators are sorted,
6. routing-lower-bound invariants hold,
7. global leaf-chain order matches tree order,
8. superblock `first_leaf` and `last_leaf` are correct,
9. every reachable child has the expected page type,
10. no reachable tree page is simultaneously on the free list,
11. internal child counts are consistent with separator counts,
12. obvious orphaned allocated pages are reported where detectable.

The verifier is a structural correctness facility, not merely a debugging convenience.

## 8.29 B+ tree invariants

1. An initialized B+ tree always has a valid root, including when empty.
2. Every leaf is level `0`.
3. Every internal child is exactly one level below its parent.
4. All physical entries are totally ordered by `(encoded_user_key, RID)`.
5. Leaf slot directories are sorted by physical key.
6. Internal separator slots are sorted.
7. Internal separators are valid lower routing bounds for their right children.
8. `N` internal separators imply `N+1` children.
9. Duplicate SQL user keys are legal at the physical-tree layer.
10. A B+ tree hit never decides MVCC visibility.
11. Forward scans use safe latch-coupled leaf handoff.
12. Published child, sibling, and root pointers reference initialized pages.
13. A detached page is not reused while a legal traversal can still reference it.
14. Structural shape changes are separate from user-transaction visibility/abort semantics.
15. Every persistent B+ tree mutation participates in page-LSN/WAL ordering.
16. Variable-length split/rebalance decisions are byte based.
17. Encoded user keys larger than 1024 bytes are rejected rather than truncated.
18. B+ tree pages are accessed through BufferPool, never directly through DiskManager.
19. Persistent B+ page parsing validates bounds before dereferencing offsets/lengths.
20. The complete tree can be checked by the explicit verifier.
21. V1 native range traversal is forward/ascending.
22. Root acquisition never waits for a B+ page latch while holding the root-metadata latch; root identity is validated using the process-local `root_generation`.
23. Structural metadata publication may acquire the root-metadata latch while holding the required page latch(es).
24. Vertical page-latch acquisition is parent-before-child.
25. Horizontal adjacent-leaf latch acquisition is left-to-right or the operation restarts.
26. Tree-local page reuse occurs only after safe structural detachment.
27. Physical UNIQUE-index duplicates remain representable because uniqueness is transactionally enforced above the tree.
---

# Part IV — Transactions and Durability

# 9. Transaction Lifecycle and Snapshots

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §19. The concrete transaction/snapshot contract is migrated in Pass 7.

Transactions receive transaction identifiers and snapshots.

The core transaction states are:

```text
ACTIVE
COMMITTED
ABORTED
```

Transaction management owns the lifecycle of:

- transaction identifiers,
- the active-transaction registry,
- snapshots,
- commit/abort state,
- write-set/conflict state required by the concrete protocol.

The initial isolation targets are:

1. Read Committed,
2. Snapshot-Isolation / Repeatable-Read-style semantics as concretely defined by the transaction contract.

True serializable isolation is deferred until the base MVCC engine is correct.

Candidate later mechanisms include Serializable Snapshot Isolation, predicate locking, and key-range locking.

---

# 10. MVCC Visibility and Tuple-Version Semantics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §18. Exact tuple-header visibility rules are migrated in Pass 7.

The v1 architecture uses heap-version MVCC.

An UPDATE creates a new physical heap tuple version rather than modifying the row in place as one permanent physical object.

Conceptually:

```text
old version:
    xmin = creator transaction
    xmax = updating transaction

new version:
    xmin = updating transaction
    xmax = no invalidating transaction
```

This model intentionally exposes:

- transaction snapshots,
- creator/deleter visibility,
- old and new physical versions,
- dead tuple versions,
- version-chain traversal,
- storage bloat,
- vacuum,
- index/version interaction.

The architecture accepts additional heap growth and vacuum pressure in exchange for explicit and inspectable MVCC semantics.

Future experiments with alternative MVCC models SHOULD remain possible, but v1 behavior is defined by the heap-version model.

---

# 11. Logical Locking and Write Conflicts

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §20. The detailed lock/conflict protocol is migrated in Pass 7.

Transaction-level logical locking MUST NOT be implemented as one database-wide mutex.

The architecture must support sufficiently fine-grained write-conflict control, including tuple/row write protection and, where required by later features, table-intention or key/range locking.

Logical locks MAY be held for transaction lifetime.

Logical locks and in-memory latches are separate mechanisms and MUST NOT be treated as interchangeable.

The exact lock identities, compatibility rules, deadlock handling, unique-key locking, and write/write conflict semantics are defined by the detailed transaction/locking contract.

---

# 12. Write-Ahead Logging and Commit Durability

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§21–23. The complete WAL and commit contract is migrated in Pass 8.

## 12.1 WAL requirement

Write-ahead logging is mandatory.

The central durability ordering rule is:

> A persistent data page MUST NOT become durable with a WAL-protected modification before the WAL records required to recover that modification are durable.

Every WAL record has an LSN, and every WAL-protected dirty page tracks a page LSN according to the concrete page/WAL contract.

The log must represent the transaction and page/structural information required by the concrete recovery design, including transaction completion and persistent page changes.

The detailed v1 recovery design later refines which operations require physical undo; the generic early architecture overview is not an independent requirement to physically undo ordinary user DML.

## 12.2 Group commit

Commit durability uses group commit.

Transactions SHOULD share durable WAL flushes rather than each requiring an independent filesystem synchronization.

Conceptually:

```text
transaction A ┐
transaction B ├─> WAL buffer -> durable WAL flush -> commits become durable/visible
transaction C ┘
```

The exact relationship between durable LSN, transaction status, commit acknowledgement, and visibility is defined by the detailed transaction/WAL contract.

## 12.3 Buffer policy

The database uses:

```text
STEAL + NO-FORCE
```

**NO-FORCE:** transaction commit does not force every modified data page to durable storage.

**STEAL:** buffer management may write/evict a dirty page containing changes from an uncommitted transaction, provided WAL ordering and recovery semantics make the write safe.

These policies are architectural requirements and are the reason crash recovery cannot be reduced to simply flushing all data pages at commit.

---

# 13. Checkpointing and Crash Recovery

> **Rewrite status:** Pass 1 establishes the non-conflicting architectural baseline from legacy §§24–25. The concrete v1 recovery algorithm is migrated in Pass 8.

Crash recovery is ARIES-inspired and uses:

- LSNs,
- per-page LSNs,
- durable transaction state,
- checkpoints,
- redo of changes required to reconstruct durable state.

The legacy high-level description used the generic phrase `analysis / redo / undo`. A later concrete v1 contract explicitly refines that model for heap-version MVCC, so Pass 1 does not preserve generic physical user-DML undo as an independent normative requirement.

The concrete recovery phases, loser-transaction handling, structural-action treatment, and any operation-specific undo requirements are defined in the detailed recovery contract migrated in Pass 8.

Checkpoint architecture MUST be compatible with fuzzy/background checkpointing and MUST NOT fundamentally require stopping the entire database to create a checkpoint.

A simpler initial checkpoint implementation MAY be used if the persistent metadata and WAL format remain compatible with the later fuzzy checkpoint model.

---

# 14. Vacuum and Storage Reclamation

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §26. The complete vacuum/RID-reclamation protocol is migrated in Pass 9.

Heap-version MVCC requires garbage collection of dead physical tuple versions.

Vacuum uses a visibility/safety horizon derived from active transaction state to determine when old tuple versions are no longer reachable by legal snapshots.

Vacuum responsibilities include:

- identifying reclaimable tuple versions,
- reclaiming heap space,
- cleaning index entries when required,
- maintaining free-space metadata,
- participating in statistics maintenance when that functionality exists.

The first implementation MAY expose vacuum explicitly/manually.

Background vacuum is deferred until the correctness of MVCC, index cleanup, reclamation, and recovery is well tested.

---

# 15. Transactional Write Protocols

The detailed INSERT, UPDATE, DELETE, COMMIT, ABORT, retry, and cross-subsystem write protocols are defined by the concrete transaction/storage architecture migrated in later passes.

Pass 1 introduces no additional write-protocol requirements beyond the MVCC, locking, WAL, STEAL/NO-FORCE, and recovery baselines above.

---

# Part V — Catalog and SQL Semantics

# 16. Catalog and Schema Metadata

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §27. The complete catalog/schema contract is migrated in Pass 10.

Persistent database metadata uses a relational system catalog.

The catalog is expected to contain system relations representing at least:

```text
tables
columns
indexes
constraints
statistics
```

A small bootstrap mechanism MAY exist to locate and interpret the catalog before catalog access becomes self-hosting.

As the engine matures, ordinary metadata lookup SHOULD increasingly use the database's own storage and execution machinery rather than an indefinitely separate metadata system.

---

# 17. SQL Type and Value System

The concrete SQL type system, coercion rules, generic planning-time values, NULL semantics, comparison semantics, and type registries are migrated from the later upper-layer contract in Pass 10.

Pass 1 adds no independent type-system rules beyond the stored physical scalar baseline in Chapter 5.

---

# 18. Lexer, Parser, and AST

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §28. The complete front-end contract is migrated in Pass 10.

The SQL front end uses an in-project limited parser rather than delegating the language front end wholesale to an external SQL parser.

The front end includes:

- tokenization/lexing,
- a hand-written parser,
- an AST,
- a separate semantic binder,
- type checking.

The initial grammar is intentionally a controlled SQL subset rather than full SQL-standard compatibility.

Parser output represents syntax. Database-object resolution belongs to binding rather than parsing.

---

# 19. Binding and Expression Semantics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §29. The complete binder/expression contract is migrated in Pass 10.

Parser and binder are separate subsystems.

Binding is responsible for semantic resolution including:

- table names,
- column names,
- aliases,
- ambiguity detection,
- type checking,
- function/operator resolution,
- wildcard expansion,
- catalog lookup.

After binding, database-object references SHOULD use stable catalog identifiers rather than unresolved strings.

The concrete scope model, expression representation, logical types, nullability, coercion rules, function/operator registry, and diagnostic requirements are defined by the detailed upper-layer contract.

---

# 20. Logical Plans, Properties, and Rewrites

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§30–31. The complete logical-plan/rewrite contract is migrated in Pass 11.

## 20.1 Logical relational representation

Queries are represented using relational-algebra-style logical operators.

The initial logical operator family includes the semantics of:

```text
scan/get
filter
project
join
aggregate
sort
limit
insert
update
delete
```

Logical plans describe relational meaning and MUST NOT commit to a physical execution algorithm.

## 20.2 Logical rewrites

Rule-based semantic rewrites occur before cost-based physical selection.

Rewrite families include:

- constant folding,
- predicate simplification,
- predicate pushdown,
- projection pruning,
- redundant-operator removal,
- join-predicate extraction/canonicalization.

Every rewrite MUST preserve SQL semantics, including NULL behavior and any later-defined volatility constraints.

Rewrite correctness must be verifiable independently of cost-based physical planning.

---

# 21. DDL/DML Semantic Planning and SQL v1 Scope

The detailed DDL/DML logical semantics, supported SQL-v1 statement set, subquery/CTE rules, catalog-visibility behavior, and upper-layer invariants are migrated in Pass 11.

Pass 1 introduces no additional requirements in this chapter.

---

# Part VI — Physical Execution

# 22. Physical Plan and Runtime Operator Model

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§35–36. The complete physical-plan and execution contract is migrated in Pass 12.

Logical and physical plans are distinct.

A logical operation may have multiple physical implementations. For example:

```text
logical scan
    -> sequential scan
    -> index scan

logical join
    -> nested-loop join
    -> index nested-loop join
    -> hash join
    -> merge join
```

The optimizer selects among valid physical algorithms.

Production execution is vectorized/chunk-at-a-time rather than fundamentally one tuple per iterator call.

Operators exchange batches conceptually represented as:

```text
DataChunk
    typed columns/vectors
    cardinality
    null/validity information
```

The initial target chunk cardinality is approximately:

```text
1024 rows
```

with a practical operating range on the order of hundreds to a few thousand rows where later measurements justify tuning.

The vectorized model is intended to amortize function dispatch, branches, synchronization, allocation, and tuple decoding across rows.

A row-at-a-time reference executor MAY exist for correctness testing if useful, but it is not the production execution architecture.

---

# 23. Vectorized Data and String Representation

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §37. The complete vector/string representation is migrated in Pass 12.

Execution chunks use typed vectors rather than one heap-allocated polymorphic value object per cell in hot loops.

Vector storage SHOULD be contiguous where practical and includes:

- type-specific data representation,
- validity/null masks,
- selection vectors where useful.

Generic scalar `Value` objects MAY be used for parser literals, catalog metadata, planning, debugging, or other non-hot scalar contexts.

Generic per-cell objects MUST NOT dominate scan, join, aggregation, or other performance-critical vectorized loops.

---

# 24. Query Memory, Row Storage, and Spill

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §41. The complete memory/spill contract is migrated in Pass 12.

Query and operator temporary memory uses query-scoped or operator-scoped arenas/pools where appropriate.

The design SHOULD avoid general-purpose allocation in inner loops.

Arena-style lifetime management is expected for data such as:

- hash-table storage,
- temporary keys,
- intermediate buffers,
- reusable vector storage.

The architecture targets bulk allocation, predictable lifetime, inexpensive reset/free, and fewer allocator calls.

The detailed memory budgeting, reservation, row collections, spill manager, temporary-file semantics, and failure behavior are defined in the later execution contract.

---

# 25. Vectorized Expression Execution

The concrete expression kernels, unified vector access, selection handling, SQL three-valued boolean evaluation, hashing, casts, and arithmetic rules are migrated in Pass 12.

Pass 1 adds no independent expression-kernel requirements.

---

# 26. Pipeline Execution Model

The concrete source/operator/sink model, pipeline breakers, dependency DAG, runtime state, cancellation, and scheduling interface are migrated in Pass 12.

Pass 1 establishes only that the production executor is vectorized and must remain compatible with later parallel execution.

---

# 27. Scans and Unary Physical Operators

The concrete sequential scan, index scan, filter, projection, limit, values, and result-sink contracts are migrated in Pass 12.

---

# 28. Join Execution

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §38. Detailed join execution is migrated in Pass 13.

The physical architecture includes:

- nested-loop join as the simple/general baseline,
- hash join as the primary equality-join implementation,
- index nested-loop join when the inner side has a useful index,
- merge join as a later algorithm.

The old source expressed these partly as implementation order. The final architecture treats them as algorithm availability/role rather than as a development-agent instruction.

Hash join architecture must support:

- explicit build and probe phases,
- query-lifetime memory ownership,
- SQL NULL semantics,
- composite keys,
- later spill behavior when memory budgeting requires it.

The complete in-memory/spilled hash table, LEFT join behavior, residual predicates, skew handling, and parallel build/probe contracts are migrated in Pass 13.

---

# 29. Aggregation and DISTINCT

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §39. Detailed aggregation is migrated in Pass 13.

Hash aggregation is the primary initial grouped-aggregation architecture.

The architecture later permits:

- sort-based aggregation,
- streaming aggregation when input ordering is sufficient,
- spill-capable aggregation under memory pressure.

Aggregate state representation, DISTINCT integration, NULL grouping semantics, spill/finalization behavior, and parallel combine are defined by the detailed execution contract.

---

# 30. Sorting and Top-N

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §40. Detailed sorting is migrated in Pass 13.

The baseline sort implementation is in-memory comparison sorting.

Once query-memory budgeting exists, the architecture requires a path to external merge sorting for inputs that exceed memory.

Top-N, normalized sort keys, external run representation, merge behavior, and exact SQL comparator semantics are defined by the detailed execution contract.

---

# 31. DML, DDL, VACUUM, and Result Interface

Physical DML/DDL execution, Halloween protection, target materialization, `RETURNING`, VACUUM execution, result delivery, and retry-safe output behavior are migrated in Pass 13.

---

# 32. Parallel Execution and Scheduling

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §42 and relevant shared-state direction from §43. Detailed parallel runtime architecture is migrated in Pass 13.

The execution architecture must be parallel-ready even though the initial executor MAY execute a query on one worker.

The design MUST NOT preclude later:

- parallel heap scans,
- parallel hash build/probe,
- parallel aggregation,
- parallel sorting,
- task scheduling.

A complex parallel scheduler is not required before single-worker execution is correct.

The detailed worker-pool, morsel, local/global state, dependency scheduling, fairness, and parallel operator protocols are defined by the later execution contract.

---

# Part VII — Cost-Based Optimization

# 33. Optimizer Architecture

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §33. The complete optimizer architecture is migrated in Pass 14 and Pass 15.

Physical planning is cost-based.

The optimizer compares valid physical alternatives using estimated resource costs rather than hard-coded assumptions such as “an index scan is always better than a sequential scan.”

The initial cost model considers at least:

- estimated cardinality,
- CPU work,
- sequential page reads,
- random page reads,
- memory consumption.

Exact calibration MAY evolve, but physical algorithm selection MUST remain cost-driven.

---

# 34. Statistics

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §32. The complete statistics contract is migrated in Pass 14.

The optimizer maintains explicit table/column statistics.

The architecture requires statistics sufficient to represent at least:

- row count,
- distinct-count estimate,
- null fraction,
- minimum/maximum values,
- histograms or another distribution summary.

Cardinality estimation is a first-class subsystem rather than an implicit heuristic embedded in operator selection.

---

# 35. Cardinality Estimation

The detailed selectivity, predicate truth, histogram/MCV, distinct-count, grouping, and join-cardinality models are migrated in Pass 14.

---

# 36. Cost Model and Base Access Paths

The detailed cost units, calibration parameters, sequential/index access costing, sargability, composite bounds, and base-path enumeration rules are migrated in Pass 14.

---

# 37. Physical Properties and Join Enumeration

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §34. Detailed physical properties and join search are migrated in Pass 15.

For small join sets, the optimizer uses dynamic-programming enumeration of useful join alternatives.

For larger join graphs, planning time is bounded using heuristics or another controlled search strategy.

Join-order optimization is implemented as part of the database rather than outsourced.

The exact exhaustive threshold, bushy-plan representation, legality constraints, interesting-order retention, join-algorithm enumeration, and deterministic tie-breaking are defined by the detailed optimizer contract.

---

# 38. Memo/Search and Memory-Aware Optimization

Memo representation, dominance, physical-property enforcement, memory/spill costing, large-join heuristics, plan fingerprints, and search diagnostics are migrated in Pass 15.

---

# Part VIII — Cross-Cutting Requirements

# 39. Error and Corruption Model

> **Rewrite status:** Pass 1 migrates the architectural baseline from legacy §44. Later subsystem passes add subsystem-specific error/corruption contracts.

The system distinguishes at least:

- SQL/user errors,
- transaction conflicts,
- I/O errors,
- persistent corruption,
- internal invariant failures.

Persistent corruption and impossible storage states MUST NOT be silently accepted.

Errors that can arise from valid external input, I/O behavior, concurrent transactions, or corrupted persistent bytes require explicit runtime handling; debug assertions alone are insufficient for such cases.

Internal invariant failures SHOULD fail loudly in debug/test configurations.

---

# 40. Observability and EXPLAIN

> **Rewrite status:** Pass 1 migrates the baseline from legacy §§47–48. Detailed subsystem metrics and optimizer/executor diagnostics are migrated in later passes.

## 40.1 Internal metrics

The engine must expose internal counters sufficient to reason about correctness and performance.

The initial metric families include:

- logical page reads,
- physical page reads,
- page writes,
- buffer hits/misses,
- WAL bytes,
- WAL flushes,
- B+ tree splits,
- rows scanned,
- rows filtered,
- hash build/probe activity,
- optimizer-estimated rows,
- actual rows.

Exact metric ownership and naming MAY evolve by subsystem.

## 40.2 EXPLAIN

The SQL interface eventually exposes `EXPLAIN` information including:

- logical plan,
- selected physical plan,
- estimated cardinalities,
- estimated costs.

`EXPLAIN ANALYZE` additionally exposes actual execution information such as row counts and timing so estimates can be compared with observed behavior.

These facilities are architectural observability requirements, not merely debugging conveniences.

---

# 41. Verification Requirements

> **Rewrite status:** Pass 1 preserves the architecture-level requirements from legacy §45. Detailed test recipes remain preserved in the legacy source until a dedicated verification document is adopted.

Every major subsystem requires focused verification appropriate to its invariants.

The verification strategy includes:

- unit tests for local contracts and boundary conditions,
- deterministic property/randomized tests for stateful data structures and storage algorithms,
- crash tests around WAL/data-write boundaries,
- differential SQL tests against a reference database where the supported semantics overlap,
- concurrency stress tests for lock/latch ordering, structural modifications, buffer eviction, and transaction conflicts.

Property/randomized testing is particularly important for:

- B+ tree operations,
- page compaction,
- tuple serialization,
- MVCC visibility,
- recovery.

Recovery correctness MUST be tested with simulated failures rather than inferred solely from normal shutdown/reopen behavior.

Tests MUST be able to use intentionally small resource limits, including tiny buffer pools, when required to force eviction and boundary behavior.

## 41.1 Storage verification obligations

Storage verification MUST exercise the architectural properties that cannot be established by simple encode/decode happy paths, including:

- heap-page fill/boundary behavior,
- compaction with stable SlotIds and unchanged retained tuple bytes,
- invalid slot/page access,
- persisted tuple round-trips across scalar, NULL, fixed/varlen, empty-VARCHAR, and unaligned layouts,
- raw page-file extension/read/write/reopen behavior,
- injectable short/error I/O paths,
- BufferPool eviction under capacity far smaller than the working set,
- dirty writeback,
- pin protection and pin-leak detection,
- CLOCK victim/second-chance behavior,
- concurrent read-latch and exclusive write-latch behavior,
- heap scans after reopen across many pages,
- stale-FSM candidate repair.

Physical slot reuse is tested only when the later safe RID-reuse protocol makes such reuse part of the implemented architecture; the old milestone recipe's generic “reusable slots” item is not an authorization for immediate DEAD-slot reuse.

Detailed test fixtures, exact test counts, and milestone checklists are verification documentation rather than canonical architecture and are not duplicated here.


## 41.2 B+ tree verification obligations

B+ tree verification MUST cover the architectural failure surfaces created by variable-length keys, structural changes, duplicate physical keys, persistence, and concurrency.

Required deterministic coverage includes:

- insertion without split,
- leaf/internal/cascading/root split,
- redistribution in both directions,
- leaf/internal merge,
- root contraction,
- tree-local free-page reuse,
- negative/positive signed key ordering,
- integer extremes,
- DATE/TIMESTAMP ordering,
- FLOAT64 infinities,
- `-0.0`/`+0.0` canonical equivalence,
- NaN canonicalization,
- empty VARCHAR,
- embedded zero bytes in VARCHAR,
- composite keys,
- NULL-containing keys,
- 1024-byte encoded-user-key boundary,
- oversized-key rejection.

Duplicate-heavy verification MUST place one SQL user key across multiple leaves and establish that:

```text
equality scan returns every RID exactly once
lower bound begins at the first duplicate
upper bound stops after the final duplicate
Erase(K,RID) removes only that physical entry
tree invariants survive redistribution/merge
```

Randomized persistent testing MUST compare the tree against a sorted oracle of physical `(encoded_user_key,RID)` keys across operations such as insert, exact erase, point/range lookup, and close/reopen.

Random seeds must be reproducible when failures occur.

Concurrent verification MUST stress:

- readers with writers,
- disjoint-range writers,
- hot-range writers,
- duplicate-heavy insertion,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes,
- deliberately small BufferPools,
- deadlock/restart behavior.

The full-tree verifier in §8.28 SHOULD be run frequently during deterministic, randomized, reopen, and concurrency testing.

---

# 42. Performance Requirements

> **Rewrite status:** Pass 1 preserves the architecture-level requirements from legacy §46. Detailed benchmark procedures remain preserved in the legacy source until a dedicated verification document is adopted.

Performance claims require measurement.

The benchmark program must eventually cover at least:

- sequential page-read throughput,
- sequential page-write throughput,
- buffer-pool resident-hit lookup,
- buffer-pool miss plus physical read,
- heap insertion throughput,
- heap sequential-scan throughput,
- tuple encode throughput,
- tuple decode throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- concurrent transaction throughput.

Relevant measurements include:

```text
rows / second
queries / second
transactions / second
p50 latency
p95 latency
p99 latency
CPU time
page reads / writes
WAL bytes
cache hit rate
```

Both microbenchmarks and end-to-end benchmarks are required because they answer different performance questions.

Storage benchmarks SHOULD distinguish cache/residency conditions and resource pressure when those distinctions affect the mechanism being measured, including small versus large BufferPool capacity and warm versus deliberately less-warm file-cache conditions where practical.

Performance-sensitive architecture changes SHOULD be supported by benchmark evidence rather than a single noisy measurement or intuition alone.


## 42.1 B+ tree performance measurements

B+ tree measurement should cover at least:

```text
random point lookups / second
sorted insertions / second
random insertions / second
exact deletions / second
short range-scan rows / second
long range-scan rows / second
duplicate-heavy equality lookup
split frequency
tree height
average leaf byte occupancy
average internal byte occupancy
BufferPool hit rate
page-latch wait time when available
```

Representative key shapes include:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

Measurements SHOULD distinguish:

- a mostly resident/hot tree, emphasizing CPU, comparison, latch, and cache behavior,
- a tree whose working set exceeds BufferPool capacity, emphasizing fanout, page access, replacement, and random-I/O sensitivity.

These are measurement dimensions, not requirements to prefer one optimization before evidence exists.

---

# Appendix A. Persistent Format Registry

This appendix indexes canonical persistent-format definitions. It does not replace the owning chapters.

| Persistent item | v1 width / size | Canonical definition |
|---|---:|---|
| `file_kind` code | 16 bits, little-endian | §4.7 |
| Common page header | 32 bytes | §4.8 |
| `page_type` code | 16 bits, little-endian | §4.9 |
| File superblock | 8192 bytes | §4.10 |
| File superblock common prefix | 72 bytes | §4.10 |
| File superblock CRC32C | 32 bits, little-endian | §4.10.3 |
| HEAP_DATA total page header | 48 bytes | §5.3 |
| HEAP_DATA heap-specific header | 16 bytes | §5.3.1 |
| Heap slot entry | 8 bytes | §5.4 |
| Heap slot-state code | 16 bits, little-endian | §5.4 |
| Maximum raw inline tuple payload | 8135 bytes | §5.6 |
| Tuple header | 48 bytes | §5.7 |
| Tuple flags | 16 bits, little-endian | §5.8 |
| Null bitmap | `ceil(column_count / 8)` bytes | §5.10 |
| VARCHAR descriptor | 8 bytes | §5.12 |
| FSM category | 1 byte (`0..255`) | §6.3 |
| FSM_DATA total page header | 48 bytes | §6.5 |
| FSM_DATA specific header | 16 bytes | §6.5 |
| FSM_DATA category region | 8144 bytes / entries | §6.5 |
| FSM_DATA `first_heap_page_no` | 8 bytes, little-endian | §6.5–§6.6 |
| FSM_DATA `entry_count` | 2 bytes, little-endian | §6.5–§6.7 |
| Persisted index RID | 16 bytes | §8.4.1 |
| Maximum encoded B+ user key | 1024 bytes | §8.6 |
| B+ leaf/internal total node header | 64 bytes | §8.7–§8.10 |
| B+ node slot descriptor | 8 bytes | §8.8 |
| B+ leaf entry | `user_key_length + 16` bytes | §8.9 |
| B+ internal entry | encoded user key + 16-byte RID + 8-byte child page number | §8.10 |
| BTREE superblock total header | 128 bytes | §8.2.1 |
| BTREE superblock extension | 56 bytes at offsets 72..127 | §8.2.1 |
| BTREE_FREE header | 40 bytes | §8.18 |
| FLOAT64 index key value payload | 8 sortable big-endian bytes after presence marker | §8.5.5 |
| Index key-schema fingerprint | FNV-1a-64 over canonical descriptor | §8.3.1 |

Transaction-status, WAL, catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

Transaction-status, WAL, catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

---

# Appendix B. Global Invariants

The following global invariants are migrated from the legacy architecture baseline. Later passes may add subsystem-specific invariants and cross-references, but MUST NOT weaken these requirements without an explicit architecture revision.

1. Persistent structures use logical persistent identifiers such as page IDs, never process memory pointers.
2. On-disk formats use explicit serialization.
3. WAL required to recover a persistent data-page modification becomes durable before that page modification becomes durable.
4. Buffer pins are eventually released according to the page-lifetime contract.
5. Transaction-level logical locks and in-memory latches are distinct mechanisms.
6. Logical plans do not encode a physical implementation choice.
7. The optimizer may choose among multiple physical algorithms for one logical operation.
8. Performance-critical execution paths avoid unnecessary per-cell or per-row object allocation.
9. Lower layers do not depend on SQL syntax objects.
10. Verification can force constrained resource configurations, including tiny buffer pools, to exercise eviction.
11. Recovery is verified with simulated crashes.
12. Performance-sensitive changes require measurement.

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.21; FSM/reclamation invariants are listed in §6.13; I/O/buffer invariants are listed in §7.13; B+ tree invariants are listed in §8.29.

---

# Appendix C. Deferred Features and Future Experiments

This appendix records broad deferred scope introduced by the legacy architecture overview. Later subsystem passes consolidate the complete deferred-feature lists.

Initial deferred or non-v1 areas include:

- distributed consensus, replication, sharding, and multi-node execution,
- cloud-native storage,
- full SQL-standard/vendor compatibility,
- stored procedures and triggers,
- sophisticated authentication/authorization,
- primary columnar table storage,
- JIT compilation,
- GPU execution,
- overflow/TOAST-style tuple storage,
- advanced/alternative buffer replacement such as CLOCK-Pro, LRU-K, and 2Q,
- `io_uring`, direct I/O, asynchronous prefetch, and deliberate page-cache bypass,
- optimistic/lock-free B+ tree techniques,
- true serializable isolation and its candidate mechanisms,
- background vacuum before the manual/correct baseline,
- complex parallel scheduling before single-worker execution is correct,
- merge join until the earlier join implementations exist,
- external sorting until query-memory budgeting exists,
- file segmentation,
- general-purpose extent allocation,
- whole-file shrinking as a required baseline capability,
- generalized physical-page recycling before object-specific reuse policies exist,
- HOT-like update optimization,
- nonempty heap free-slot linkage and physical slot reuse until the later safe-reclamation/reuse protocol is defined,
- SQL-level DATE/TIMESTAMP epoch/unit/time-zone semantics at the storage layer,
- UTF-8/collation/locale/`VARCHAR(n)` semantics at the storage layer,
- schema-version translation for schema-changing DDL until explicitly implemented,
- hierarchical/tree-structured FSM metadata beyond the flat v1 format,
- any runtime FSM candidate-index representation or tie-breaking policy beyond the architectural advisory/search requirements,
- physical RID/slot reuse until the later safe-reuse protocol permits it,
- page compression,
- tuple compression,
- huge-page buffer-pool experiments,
- NUMA-aware buffer-pool placement,
- a lock-free BufferPool page table,
- a visibility map,
- background vacuum beyond the explicit/manual-correctness baseline,
- parallel heap scans beyond the initial execution baseline,
- B-link internal right-links,
- optimistic versioned or latch-free B+ read traversal,
- lock-free B+ tree algorithms,
- B+ separator-prefix truncation,
- B+ leaf prefix/suffix compression,
- overflow index keys,
- native reverse B+ range scans,
- B+ bulk loading,
- online index build,
- covering/include columns,
- index-only scans,
- partial indexes,
- expression indexes,
- descending physical index components,
- locale-aware index collations,
- B+ range-scan asynchronous prefetch,
- parallel index scans,
- B+ root/upper-level latch-contention reduction beyond the baseline latch-coupled design,
- clustered-storage/index-locality experiments.

Items listed here are future possibilities or staged functionality, not requirements to implement immediately.

---

# Appendix D. Open Architecture Decisions

The structural rewrite does not silently decide gaps that the legacy architecture leaves underspecified.

The pre-Pass-7 coherence resolution closed the previously open decisions for:

- ordinary-page whole-page checksum coverage,
- BTREE FileSuperblock specialization and B+ persistent metadata,
- exact FLOAT64 memcomparable bytes,
- zero-only unassigned ordinary-page flags,
- byte-exact tuple fixed-area derivation,
- B+ root-metadata/page-latch concurrency.

The remaining rewrite issue register contains implementation mismatches, historical refinement notes, later-pass synchronization items, or issues outside the already migrated contract rather than unresolved choices among the decisions above.

The RID reserved-byte rule itself is not open: the architecture requires zero-on-write and reject-nonzero-on-v1-decode. R-001 records an implementation/state mismatch, not an unresolved architecture choice.

Other rewrite consistency/refinement notes remain in `ARCHITECTURE_REWRITE_ISSUES.md` until the final reconciliation pass.

---

## Rewrite progress

Architecture content migrated so far:

```text
Pass 0    inventory and target structure
Pass 1    legacy §§0–52
Pass 2    legacy §§53–63
Pass 3    legacy §§64–81
Pass 4    legacy §§82–85
Pass 5    legacy §§86–108
Pass 6    legacy §§109–179
```

The existing `ARCHITECTURE.md` remains the active architecture authority until the full rewrite, reconciliation audit, and explicit cutover are complete.
