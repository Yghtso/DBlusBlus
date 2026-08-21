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
| `5` | `TXN_STATUS` | persistent transaction terminal-status storage |

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
| `7` | `TXN_STATUS` |

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
HEAP        -> 72
FSM         -> 72
CATALOG     -> 72
TXN_STATUS  -> 72
BTREE       -> 128
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

For v1 `HEAP`, `FSM`, `CATALOG`, and `TXN_STATUS` superblocks:

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
TXN_STATUS     -> 0
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
rec_lsn
reference/replacement metadata
page latch
I/O state
checkpoint/FPI epoch metadata
temporary no-flush state when required
```

Frame metadata is process-local and MUST NOT be stored inside the persistent database page merely because it describes the resident copy.

The page byte storage SHOULD be suitably aligned for efficient I/O and memory copying.

Frequently modified frame metadata SHOULD avoid pathological false sharing where measurement shows contention.

This metadata remains process-local. `rec_lsn`, checkpoint/FPI epoch state, and temporary no-flush barriers follow the exact WAL/checkpoint contracts in Chapters 12–13.

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

A successful physical write may clear dirty state only under synchronization that establishes the written image was not superseded by a newer in-memory mutation.

For a WAL-protected frame, §12.16 defines the exact `rec_lsn` transition: the first modification of a clean interval sets `rec_lsn`, later modifications preserve it, and only a successfully synchronized flush of the current image may clear both dirty state and `rec_lsn`.

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

B+ mini-transactions use the stronger temporary no-flush condition defined in §12.10.2.

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

Chapter 12 defines the exact recovery mechanism: every physical B+ mutation is a recovery-safe B+ mini-transaction/system structural action encoded as one logical `BTREE_MTR` record with a temporary no-flush barrier.

Chapter 8 does not duplicate that WAL payload format or barrier protocol; Chapter 12 owns them.

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

The B+ mini-transaction contract in §12.10.2 imposes a temporary no-flush barrier while one structural/mutation unit is being constructed.

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

## 9.1 Scope and subsystem coordination

The transaction subsystem is a coordinated architecture rather than a set of independent local policies.

The following responsibilities must agree on transaction identity and visibility:

```text
TransactionManager
SnapshotManager
VisibilityManager
LockManager
TransactionStatusStore
```

They also interact with later-owned durability and reclamation responsibilities:

```text
WalManager
CommitCoordinator
CheckpointManager
RecoveryManager
VacuumManager
ReadEpochManager
```

This chapter owns:

- transaction identifiers and reservation,
- transaction lifecycle state,
- isolation-level identity,
- command IDs,
- snapshots and snapshot lifetime,
- transaction-status lookup/publication semantics,
- read-only transaction behavior.

Chapter 10 owns tuple visibility.

Chapter 11 owns logical write/uniqueness conflicts.

The later durability/recovery chapters own WAL record formats, commit flushing, checkpoints, and crash reconstruction.

A shortcut in one subsystem MUST NOT violate the identity, visibility, durability, conflict, or reclamation rules of another.

## 9.2 Transaction identifiers

Transaction identifiers are 64-bit values.

The reserved values are:

```text
INVALID_TXN_ID      = 0
FROZEN_TXN_ID       = 1
FIRST_NORMAL_TXN_ID = 2
```

Normal transaction IDs are monotonically allocated beginning at:

```text
2
```

A normal transaction ID MUST NOT be reused after it has entered a durably reserved allocation range, even if a crash occurs before that ID is actually handed to a transaction.

`FROZEN_TXN_ID` means:

> the creator transaction committed sufficiently long ago that ordinary visibility no longer requires lookup of its original transaction-status entry.

For visibility purposes:

```text
FROZEN_TXN_ID -> COMMITTED
```

The 64-bit space makes numerical wraparound practically irrelevant for the intended system lifetime.

The allocator MUST NOT wrap and reuse old normal transaction IDs.

Freezing exists to permit eventual transaction-status reclamation, not primarily to solve 32-bit-style wraparound.

## 9.3 Durable transaction-ID reservation

The transaction allocator reserves normal TxnIds in durable blocks rather than synchronizing durable control metadata for every `BEGIN`.

The initial reservation block size is exactly:

```text
1,048,576 transaction IDs
```

Durable global control metadata stores:

```text
reserved_txn_id_end
```

V1 defines this value as an **exclusive upper bound**.

At any instant, the durable reserved normal-TxnId space is:

```text
[FIRST_NORMAL_TXN_ID, reserved_txn_id_end)
```

and the allocator's current usable in-memory suffix is:

```text
[next_txn_id, reserved_txn_id_end)
```

A newly initialized database begins with:

```text
next_txn_id         = FIRST_NORMAL_TXN_ID
reserved_txn_id_end = FIRST_NORMAL_TXN_ID
```

which means no normal TxnId has yet been durably reserved.

Before the allocator can hand out the first normal TxnId—or any later ID when:

```text
next_txn_id == reserved_txn_id_end
```

it performs the block-reservation protocol:

```text
old_end = reserved_txn_id_end

new_end =
    old_end + 1,048,576
```

using checked 64-bit arithmetic.

Then:

1. durably publish `new_end` to the database control file,
2. only after that durable update succeeds set the runtime reservation end to `new_end`,
3. hand out TxnIds from the half-open interval:
   ```text
   [old_end, new_end)
   ```

If the durable reservation update fails, IDs from the not-yet-durable interval MUST NOT be handed out.

If adding another reservation block would overflow the normal 64-bit TxnId space, beginning a new transaction MUST fail rather than wrap or reuse an old ID.

After a crash, unused IDs from an already durable reservation MAY be skipped permanently.

An ID that may have appeared in persistent database state MUST never be reused.

The database-control-file physical layout and torn-update protocol are owned by the later durability/recovery chapter.

## 9.4 Transaction object and lifecycle state

A transaction contains at least the conceptual state:

```text
TxnId txn_id
TxnState state
IsolationLevel isolation
CommandId current_command_id
optional<Snapshot> transaction_snapshot
optional<Snapshot> statement_snapshot
Lsn last_wal_lsn
bool has_persistent_writes
held logical locks
cancellation/deadlock flag
```

Initial transaction states are:

```text
ACTIVE
COMMITTING
COMMITTED
ABORTING
ABORTED
```

State transitions are monotonic.

`COMMITTING` and `ABORTING` are transient lifecycle states.

`COMMITTED` and `ABORTED` are terminal states.

A terminal transaction MUST NOT transition back to an active/transient state.

The exact WAL/durability steps that complete `COMMITTING` or reconstruct terminal state after a crash belong to the later durability/recovery chapters.

## 9.5 Isolation levels

Version 1 supports:

```text
READ COMMITTED
REPEATABLE READ
```

The default is:

```text
READ COMMITTED
```

Version-1 `REPEATABLE READ` implements:

```text
snapshot isolation
```

It is **not** full serializability.

The SQL surface, documentation, diagnostics, and tests MUST NOT describe v1 snapshot isolation as SERIALIZABLE.

Deferred isolation/concurrency mechanisms include:

```text
SERIALIZABLE
Serializable Snapshot Isolation (SSI)
predicate locking
key-range locking
```

## 9.6 Command IDs

Each SQL statement inside one transaction executes under one:

```text
CommandId
```

The initial command ID is:

```text
0
```

and `CommandId{0}` is a valid command ID.

After one SQL statement completes, the transaction increments the command ID exactly once.

Tuple versions store:

```text
cmin
cmax
```

so same-transaction visibility can distinguish:

- versions created by an earlier statement,
- versions created by the current statement,
- versions deleted/superseded by an earlier statement,
- versions deleted/superseded by the current statement.

Command IDs therefore preserve statement ordering within one transaction rather than collapsing all self-created/self-deleted versions into one state.

## 9.7 Snapshot representation

A snapshot contains:

```text
TxnId xmin
TxnId xmax
sorted vector<TxnId> active
TxnId owner_txn_id
CommandId command_id
```

### 9.7.1 xmax

At capture:

```text
xmax =
    next normal transaction ID that has not yet been assigned
```

A normal transaction satisfying:

```text
txn_id >= snapshot.xmax
```

started too late to be visible to that snapshot.

### 9.7.2 active

`active` contains **other** normal transactions that were nonterminal for snapshot purposes at capture and satisfy:

```text
txn_id < snapshot.xmax
txn_id != snapshot.owner_txn_id
```

The owner transaction is deliberately excluded from `active`.

Self-visibility is handled only by the explicit:

```text
xmin/xmax owner comparison
+
cmin/cmax command rules
```

rather than by active-set membership.

A transaction remains snapshot-active until terminal outcome publication removes it from the active registry. Therefore snapshot-active runtime states include the nonterminal lifecycle states:

```text
ACTIVE
COMMITTING
ABORTING
```

as applicable at the capture instant.

A transaction represented in `active` remains invisible to this snapshot even if it commits later.

The vector is sorted.

Initial membership testing MAY therefore use binary search.

A later high-concurrency implementation MAY replace this with a hybrid or different runtime representation without changing snapshot semantics.

### 9.7.3 xmin

`xmin` is derived exactly from the snapshot's **other-transaction** active set:

```text
if active is non-empty:
    xmin = active[0]   // sorted minimum
else:
    xmin = xmax
```

The owner TxnId does not reduce `xmin`.

This separation is intentional.

For example, a READ COMMITTED transaction that is old but currently executing a statement with no older competing active transaction does not pin the vacuum horizon merely because its own TxnId is small.

The authoritative vacuum horizon remains the registry of active SQL snapshots, not the existence or age of transactions themselves.

### 9.7.4 owner and command

```text
owner_txn_id
```

identifies the transaction evaluating visibility.

```text
command_id
```

captures the statement-order boundary used by same-transaction `cmin/cmax` visibility.

## 9.8 Snapshot capture synchronization

The transaction manager maintains conceptually:

```text
next_txn_id
active transaction registry
```

Snapshot capture and transaction registration use one short synchronization protocol that atomically observes:

```text
transaction-ID high-water mark
+
active transaction IDs below that high-water mark
```

The protocol MUST prevent a transaction from becoming ambiguously active/inactive relative to a snapshot that has already chosen `xmax`.

The snapshot/registry synchronization is held only for capture/registration bookkeeping.

It MUST NOT be held while executing the query or statement.

## 9.9 READ COMMITTED snapshot lifetime

At the beginning of each SQL statement in a READ COMMITTED transaction:

```text
capture a new statement snapshot
register it as active
```

The statement uses that one stable snapshot for its full execution attempt.

At successful statement completion:

```text
unregister the statement snapshot
increment current_command_id
```

Two statements in the same READ COMMITTED transaction may therefore observe different committed database states.

If the statement must be internally retried after a write conflict, the retry uses a fresh READ COMMITTED statement snapshot as defined in Chapter 11.

The later transactional write-protocol chapter completes the external-output/retry-safe boundary; Pass 7 does not import that later execution rule.

## 9.10 REPEATABLE READ snapshot lifetime

For REPEATABLE READ, capture the transaction snapshot at the beginning of the first ordinary statement.

Reuse that snapshot for later statements.

The fixed transaction snapshot retains its transaction-ID visibility horizon, while:

```text
snapshot.command_id
```

is updated for each statement so same-transaction command visibility remains correct.

The transaction snapshot remains registered until transaction end.

A long-running REPEATABLE READ transaction may therefore pin the global vacuum horizon.

## 9.11 Transaction-status store

Persistent transaction outcome metadata lives in:

```text
txn_status.dat
```

The file is page based:

```text
page 0       FileSuperblock with FileKind::TXN_STATUS
page 1..N    TXN_STATUS pages
```

The persisted file-kind code is:

```text
FileKind::TXN_STATUS = 5
```

`FileSuperblock.object_id` is:

```text
0
```

for this singleton database-wide file.

The persistent two-bit status codes are:

| Two-bit code | State | V1 meaning |
|---:|---|---|
| `00` | `INVALID` | no terminal outcome recorded in this slot |
| `01` | `COMMITTED` | terminal committed outcome |
| `10` | `ABORTED` | terminal aborted outcome |
| `11` | `RESERVED` | recognized nonterminal/reserved-format marker |

`IN_PROGRESS` remains runtime state represented by the active transaction registry; v1 does not require a durable status-page write at every `BEGIN`.

### 9.11.1 RESERVED semantics

`RESERVED` is a legal decoded v1 bit pattern but is **not emitted by the normal v1 transaction-begin path**.

The normal execution path uses:

```text
active registry
```

for in-progress transactions and writes a terminal status only when COMMITTED/ABORTED is published.

Therefore v1 correctness does not require materializing one `RESERVED` status for every TxnId covered by the durable block reservation.

If `RESERVED` is encountered:

- it MUST NOT be treated as COMMITTED,
- it MUST NOT be treated as ABORTED,
- while the referenced transaction is active, the active registry still wins,
- after completed recovery, a persistent correctness object that references a normal TxnId whose only non-runtime status is `INVALID` or `RESERVED` indicates an invariant failure/corruption unless recovery has first classified and published the transaction's terminal outcome.

Future architecture may assign an operational writer for the `RESERVED` code, but doing so must not change its v1 nonterminal meaning.

## 9.12 Transaction-status page capacity

A v1 transaction-status page uses exactly the normal 32-byte common page header and no additional page-specific header:

```text
page_type         = TXN_STATUS
format_version    = 1
header_size       = 32
common flags      = 0
common reserved16 = 0
page_no           = actual status-file PageNo
```

The persisted page-type code is:

```text
PageType::TXN_STATUS = 7
```

Therefore:

```text
status payload bytes =
    PAGE_SIZE - 32
    = 8160
```

At:

```text
4 two-bit status entries per byte
```

one status page represents exactly:

```text
8160 * 4
= 32,640 normal transaction IDs
```

Reserved TxnIds:

```text
0 = INVALID_TXN_ID
1 = FROZEN_TXN_ID
```

do **not** consume ordinary status entries.

For a normal TxnId:

```text
txn_id >= FIRST_NORMAL_TXN_ID
```

define:

```text
ordinal =
    txn_id - FIRST_NORMAL_TXN_ID

status_page_no =
    1 + ordinal / 32640

entry_in_page =
    ordinal % 32640

payload_byte_index =
    entry_in_page / 4

two_bit_index =
    entry_in_page % 4

page_byte_offset =
    32 + payload_byte_index

bit_shift =
    2 * two_bit_index
```

The two-bit field is packed LSB-first within the payload byte:

```text
mask = 0x3 << bit_shift
```

Extraction is:

```text
code =
    (page[page_byte_offset] >> bit_shift) & 0x3
```

Updating one entry replaces only those two bits.

All mapping arithmetic MUST be checked before physical page access.

A freshly initialized TXN_STATUS page is zero-filled, so every unmodified status slot begins canonically as:

```text
INVALID = 00
```

A v1 TXN_STATUS decoder MUST reject:

- wrong `page_type`,
- unsupported `format_version`,
- `header_size != 32`,
- nonzero common flags,
- nonzero common `reserved16`,
- mismatched embedded `page_no`,
- checksum failure when ordinary-page checksum verification is active.

The page's common `page_lsn` records the newest terminal-status WAL modification reflected in the page.

An in-memory hash table MUST NOT define the persistent status format.

## 9.13 Transaction-status lookup

Status lookup follows this semantic order:

```text
if txn_id == FROZEN_TXN_ID:
    COMMITTED

else if txn_id == current transaction:
    SELF

else if active transaction registry contains txn_id:
    IN_PROGRESS

else:
    consult cached/persistent terminal transaction status
```

A persistent or runtime result of:

```text
COMMITTED
ABORTED
```

has the obvious terminal meaning.

Persisted:

```text
INVALID
RESERVED
```

are nonterminal results and MUST NOT be promoted to committed/aborted by guesswork.

`SELF` and `IN_PROGRESS` are lookup results, not additional persisted two-bit status codes.

A persistent tuple or other correctness object that references a normal TxnId which is:

```text
not active
and
has neither COMMITTED nor ABORTED terminal status
```

after completed crash recovery indicates corruption or an invariant failure.

The engine MUST NOT silently treat unknown status as committed.

A read-only transaction that has ended without a terminal status entry does not violate this rule because, by definition, it created no persistent state that may legally reference its TxnId.

## 9.14 Terminal status publication boundary

### 9.14.1 Commit

A transaction with persistent state MUST NOT become visible as committed before its commit WAL record is durable.

The architectural publication order is:

```text
append transaction-commit WAL
    ↓
make WAL durable through commit LSN
    ↓
publish COMMITTED in TransactionStatusStore/cache
    ↓
release transaction-lifetime logical locks
    ↓
unregister transaction snapshot(s) and active transaction state
    ↓
return commit success
```

The transaction-status page update itself does not require a synchronous data-page flush before commit returns.

Its page LSN is set according to the durable commit record so crash recovery can reconstruct the status if the status page itself was not persisted before the crash.

The exact WAL record format and group-commit mechanism are owned by the later WAL/commit chapter.

### 9.14.2 Abort

Abort publication is conceptually:

```text
mark transaction ABORTING
    ↓
append transaction-abort WAL if the transaction produced persistent WAL-visible state
    ↓
publish ABORTED
    ↓
release transaction-lifetime logical locks
    ↓
unregister snapshot(s) and active transaction state
    ↓
finish ABORTED
```

An ordinary abort does not require an immediate WAL durability flush merely to acknowledge the abort.

If an abort WAL record is lost in a crash, later recovery treats the transaction as a loser and establishes ABORTED state again.

Ordinary user abort does not physically restore heap/index bytes; Chapter 10 defines that MVCC rollback model.

## 9.15 Read-only transactions

A transaction that never creates persistent state:

```text
does not require a transaction-commit WAL record
does not require a terminal transaction-status entry
```

It still:

- owns a TxnId,
- participates in the active transaction registry,
- owns/registers snapshots according to its isolation level,
- may pin the vacuum horizon.

Durable TxnId block reservation prevents harmful TxnId reuse even when a read-only transaction leaves no WAL/status record.

## 9.16 Transaction/snapshot invariants

1. `INVALID_TXN_ID=0`, `FROZEN_TXN_ID=1`, and normal TxnIds begin at `2`.
2. Normal TxnIds are monotonically allocated and never reused after durable reservation.
3. A new TxnId reservation is not consumed before its global reservation metadata is durable.
4. A crash may create gaps in the TxnId sequence.
5. `FROZEN_TXN_ID` is always treated as committed for visibility.
6. V1 isolation levels are READ COMMITTED and REPEATABLE READ; REPEATABLE READ is snapshot isolation, not serializable isolation.
7. `CommandId{0}` is valid and command IDs advance once per completed SQL statement.
8. Snapshot capture atomically observes the TxnId high-water mark and the relevant active registry.
9. `snapshot.active` excludes `owner_txn_id`; `snapshot.xmin` is the smallest other active TxnId, or `xmax` when none exists.
10. One READ COMMITTED statement uses one stable snapshot per execution attempt.
11. REPEATABLE READ reuses one transaction snapshot and changes only its command boundary for later statements.
12. A transaction-status lookup never guesses that an unknown referenced normal TxnId committed.
13. A transaction is not published COMMITTED before its commit WAL is durable.
14. Read-only transactions may end without terminal persistent status because they create no persistent TxnId references.
15. Transaction-lifetime logical locks are released only after terminal outcome publication.

---

# 10. MVCC Visibility and Tuple-Version Semantics

## 10.1 Heap-version MVCC and rollback model

Version 1 uses heap-version MVCC.

An ordinary UPDATE creates a new physical tuple version:

```text
old version:
    xmax = updating transaction

new version:
    xmin = updating transaction
    prev = old RID
```

The new physical version receives a new RID.

Ordinary user-transaction abort is a transaction-status/visibility decision rather than a byte-for-byte physical rollback of heap and B+ tree user-DML changes.

### Aborted INSERT

A version whose:

```text
xmin = aborted transaction
```

is invisible to other transactions.

Its physical heap bytes and index entries may remain until vacuum.

### Aborted DELETE

A version whose:

```text
xmax = aborted transaction
```

is treated as not deleted.

### Aborted UPDATE

The old version's aborted `xmax` is ineffective.

The new version's aborted `xmin` makes the new version invisible.

New physical index entries may remain as vacuumable garbage.

The v1 rollback model therefore preserves:

```text
STEAL + NO-FORCE
```

without requiring physical ordinary-user-DML undo or compensation-log-record-style restoration of heap/index bytes.

Later WAL/recovery chapters define loser-transaction reconstruction and system/structural recovery actions without changing these visibility semantics.

## 10.2 Creator visibility

Given tuple version `T` and snapshot `S`, determine creator visibility first.

The exact v1 rule is:

```text
if T.xmin == FROZEN_TXN_ID:
    creator_visible = true

else if T.xmin == S.owner_txn_id:
    creator_visible =
        (T.cmin < S.command_id)

else:
    status = Status(T.xmin)

    if status != COMMITTED:
        creator_visible = false

    else if T.xmin >= S.xmax:
        creator_visible = false

    else if T.xmin is in S.active:
        creator_visible = false

    else:
        creator_visible = true
```

If:

```text
creator_visible == false
```

the tuple is invisible regardless of `xmax`.

The strict self-created rule:

```text
cmin < snapshot.command_id
```

means a tuple created by the current statement is not rediscovered through ordinary snapshot visibility during that same statement.

Operations such as `RETURNING` SHOULD use the operation's produced values rather than depending on an ordinary snapshot rescan of a just-created tuple.

## 10.3 Deleter visibility

After creator visibility succeeds, inspect:

```text
T.xmax
```

### 10.3.1 No deleter

```text
T.xmax == INVALID_TXN_ID
```

means the tuple remains visible.

### 10.3.2 Deleted/superseded by the current transaction

If:

```text
T.xmax == S.owner_txn_id
```

then:

```text
if T.cmax < S.command_id:
    tuple is no longer visible
else:
    tuple remains visible to the current statement
```

Thus a delete/update performed by the current statement does not retroactively remove the old version from that same statement snapshot.

### 10.3.3 Deleted/superseded by another transaction

Let:

```text
status = Status(T.xmax)
```

If:

```text
status == ABORTED
```

the delete/update never became logically effective, so the tuple remains visible.

If:

```text
status == IN_PROGRESS
```

the tuple remains visible to this snapshot.

Read visibility does not wait merely because another transaction currently has an in-progress `xmax`.

Write-conflict handling is owned by Chapter 11.

If:

```text
status == COMMITTED
```

the tuple is deleted for snapshot `S` only when:

```text
T.xmax < S.xmax
and
T.xmax not in S.active
```

Otherwise the deleting/updating transaction is too new for the snapshot, so the old tuple remains visible.

## 10.4 Visibility evaluation order

Ordinary MVCC visibility is evaluated conceptually as:

```text
creator committed/visible before snapshot?
        no  -> invisible
        yes
         ↓
deleter absent/aborted/in-progress/too-new?
        yes -> visible
        no  -> invisible
```

Readers do not acquire logical tuple locks merely to evaluate MVCC visibility.

The physical heap page supplies tuple metadata.

The visibility subsystem combines that metadata with:

```text
snapshot
+
transaction status
```

and owns the SQL visibility decision.

## 10.5 MVCC hint cleanup

Maintenance may normalize tuple metadata once transaction outcome is permanently known.

### Aborted xmax

When:

```text
Status(xmax) == ABORTED
```

maintenance may rewrite:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

### Frozen creator

When later freeze rules establish that the original committed creator need no longer be distinguished, maintenance may rewrite:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

These are physical cleanup optimizations.

They do not change the logical history that made the cleanup safe.

Because they modify persistent page bytes, they participate in WAL/page-LSN rules once WAL is active.

The exact freeze horizon and status-page truncation rules belong to the later vacuum/reclamation pass.

## 10.6 MVCC invariants

1. Heap-version MVCC identifies creator/deleter transactions through tuple `xmin/xmax`.
2. Ordinary user abort does not require physical heap/index undo.
3. An aborted creator makes its physical version invisible.
4. An aborted `xmax` does not delete the old version.
5. `FROZEN_TXN_ID` is creator-visible as committed.
6. A self-created version is visible only when `cmin < snapshot.command_id`.
7. A self-deleted/superseded version becomes invisible only when `cmax < snapshot.command_id`.
8. A committed creator with `xmin >= snapshot.xmax` is too new.
9. A committed creator present in `snapshot.active` remains invisible to that snapshot.
10. An in-progress or aborted other-transaction deleter does not make the old tuple invisible.
11. A committed deleter removes the tuple only if it committed within the snapshot's visible past: `xmax < snapshot.xmax` and not in `snapshot.active`.
12. Read visibility does not acquire tuple-write locks.
13. HeapPage does not decide visibility.
14. Hint cleanup is a physical page mutation and does not invent transaction outcome.

---

# 11. Logical Locking and Write Conflicts

## 11.1 LockManager scope

Version 1 uses exclusive logical transaction locks for exactly two conflict families:

```text
TUPLE_WRITE
UNIQUE_KEY
```

Readers use MVCC and do not acquire shared tuple locks merely to read visible versions.

Deferred lock families include:

```text
TABLE
INTENTION
SCHEMA
KEY_RANGE
```

The physical B+ tree/heap latch architecture is separate from these transaction-lifetime logical locks.

## 11.2 Tuple-write lock identity

A tuple write lock is keyed by:

```text
(TableId, physical target RID)
```

The target RID is the currently visible physical tuple version that the transaction intends to:

```text
UPDATE
or
DELETE
```

Two writers that begin from the same visible physical version therefore contend on the same logical lock.

Because UPDATE creates a new RID, a writer that waits for the old version's logical lock MUST re-fetch and revalidate the target after lock acquisition.

## 11.3 Lock/latch separation rule

A transaction MUST NOT wait for a logical LockManager lock while holding:

- a heap page latch,
- a B+ tree page latch,
- a buffer-frame/page latch,
- a B+ structural latch/guard whose lifetime blocks physical structure progress.

The required shape is:

```text
identify candidate RID/key
    ↓
release short-lived page/index latches
    ↓
acquire/wait for logical lock
    ↓
re-fetch candidate
    ↓
re-check identity, visibility, header, predicate, key state
    ↓
perform mutation using short-lived physical latches
```

This prevents transaction-duration waits from coupling to microsecond-scale page-structure latches.

The rule composes with the B+ tree's own page-latch ordering and optimistic root validation: LockManager waits occur outside those physical-latch critical sections.

## 11.4 UPDATE / DELETE write-conflict protocol

For a candidate visible physical tuple version `R`:

1. release short-lived heap/index latches,
2. acquire exclusive `TUPLE_WRITE(TableId,RID)` on `R`,
3. re-fetch `R`,
4. re-check tuple identity and visibility,
5. inspect the current `xmax`,
6. apply the cases below.

### 11.4.1 No competing xmax

If:

```text
xmax == INVALID_TXN_ID
```

the write may proceed subject to its other constraints.

### 11.4.2 xmax belongs to self

Handle the target through same-transaction command semantics.

The transaction MUST NOT treat this as a second independent competing writer.

### 11.4.3 xmax belongs to an aborted transaction

The prior delete/update is logically ineffective.

The new writer MAY overwrite the aborted:

```text
xmax
cmax
```

metadata while installing its own write semantics.

### 11.4.4 xmax belongs to an in-progress transaction

The tuple-write lock protocol should ordinarily already serialize competing writers to the same visible version.

If an in-progress competing `xmax` is discovered because of a race:

```text
release physical page/index latches
wait/retry through the logical-lock protocol
re-fetch/revalidate
```

Do not wait while retaining physical page latches.

### 11.4.5 xmax belongs to a committed competing updater

The response is isolation-level-specific.

READ COMMITTED follows §11.5.

REPEATABLE READ follows §11.6.

## 11.5 READ COMMITTED write conflict

If a READ COMMITTED writer waited and then discovers that another transaction committed an UPDATE/DELETE of its original target, it MUST NOT blindly modify the stale physical version.

Instead:

```text
restart the affected statement's candidate search
using a fresh READ COMMITTED statement snapshot
```

The retry re-evaluates all semantically relevant statement state, including:

- current row version,
- predicates,
- index conditions,
- generated values when required.

The restarted statement may find:

- a newer row version that still matches,
- a newer row version that no longer matches,
- no matching row.

This is an internal statement retry.

The later write/execution protocol defines when external result emission becomes retry-safe.

## 11.6 REPEATABLE READ write conflict

Under v1 REPEATABLE READ / snapshot isolation, if another transaction committed a change to the target after the current transaction's fixed snapshot:

```text
abort with a serialization/write-conflict error
```

This is first-updater-wins behavior.

The writer MUST NOT silently follow a newer version that its fixed transaction snapshot could not see.

## 11.7 Lost-update prevention

Lost-update prevention relies on the combination:

```text
exclusive tuple-write locks
+
revalidation after lock wait/acquisition
+
READ COMMITTED statement restart
+
REPEATABLE READ serialization/write-conflict abort
```

This combination MUST prevent two concurrent writers from silently overwriting each other's work on one logical target version.

Snapshot-isolation write skew across different rows remains possible.

That behavior is one reason full SERIALIZABLE isolation is deferred to SSI/predicate/key-range mechanisms.

## 11.8 Unique-key lock identity

For a fully non-NULL SQL unique key:

```text
UniqueLockKey =
    (IndexId, full encoded user-key bytes)
```

The encoded user-key bytes are the canonical B+ user-key representation from Chapter 8.

A hash MAY select the LockManager shard/bucket.

Hash collisions MUST be resolved by comparison of the complete lock key.

Uniqueness MUST NOT depend on a 64-bit hash alone.

## 11.9 Unique-key lock protocol

Any DML operation that may create or remove a **fully non-NULL** unique key acquires the corresponding exclusive:

```text
UNIQUE_KEY(IndexId, encoded user-key)
```

Examples include:

```text
INSERT unique K
UPDATE old K -> new K
DELETE unique K
```

Unique-key locks remain held until transaction end.

When an UPDATE needs both old and new unique-key locks and both keys are known before waiting, acquire them in deterministic encoded-key order to reduce deadlock probability.

The deadlock detector remains the correctness fallback.

For v1 SQL UNIQUE semantics, if **any** indexed key component is NULL:

```text
duplicate rejection is skipped
```

and no duplicate-prevention unique-key lock is required for that NULL-containing key merely to enforce SQL uniqueness.

The physical B+ entries still exist and remain ordered normally.

## 11.10 Unique-check semantics

After obtaining the relevant unique-key lock:

1. scan every physical B+ entry with that encoded user key,
2. fetch every referenced heap tuple version,
3. inspect current transaction outcome/state rather than relying solely on the caller's historical snapshot,
4. reject if another logically live row owns the key,
5. ignore globally aborted/dead physical versions.

Uniqueness is a constraint on the database's current transactional state, not merely on what one historical snapshot can see.

If an unexpected in-progress conflicting creator is encountered, the operation waits/retries through the logical lock protocol.

It MUST NOT wait while holding B+ or heap page latches.

The physical B+ tree itself continues to permit duplicate user-key bytes because physical keys include RID.

## 11.11 Lock duration

V1 uses strict transaction-duration write locking.

Both:

```text
TUPLE_WRITE
UNIQUE_KEY
```

locks are held until:

```text
COMMIT
or
ABORT
```

They are not released immediately after the corresponding page modification.

This prevents another writer from acting as though a still-unresolved transaction outcome were final.

## 11.12 Lock table

The initial LockManager MAY use:

```text
hash map<LockKey, LockQueue>
```

protected by a conventional mutex.

Because every v1 logical lock mode is exclusive, each queue conceptually contains:

```text
current owner
FIFO waiter list
```

The initial compatibility rule is therefore:

```text
unowned key      -> grant exclusive lock
owned by self    -> same-transaction handling / no competing owner
owned by another -> wait
```

The abstraction must allow later:

- sharding,
- shared lock modes,
- intention/table/schema/range lock families.

A lock-free lock table is not required by the baseline architecture.

## 11.13 Deadlock detection

When a transaction blocks on a logical lock owned by another transaction, the wait-for graph receives:

```text
waiter -> current owner
```

dependency edges.

Cycle detection may run:

- when blocking edges are added,
- through a lightweight detector thread,
- or through a combination of both.

The initial victim policy is deterministic:

```text
abort the youngest transaction in the detected cycle
=
highest TxnId in that cycle
```

The chosen victim receives a cancellation/deadlock flag.

Blocked waits affected by the victim decision are awakened.

The victim follows the ordinary transaction abort/status-publication path.

Timeouts MAY exist for diagnostics or fallback behavior.

Timeout expiration is not the primary deadlock-correctness mechanism.

## 11.14 Physical latches are not LockManager locks

The architecture maintains a hard distinction:

```text
heap / B+ / buffer page latches:
    short-lived physical-memory/data-structure protection

LockManager locks:
    transaction-lifetime logical conflict protection
```

Page latches MUST NOT be represented as LockManager locks.

Tuple logical locks MUST NOT be used to protect B+ page structure.

Unique-key logical locks MUST NOT replace B+ structural latching.

Logical-lock wait-for-graph edges are transaction-conflict edges, not page-latch dependency edges.

## 11.15 Logical-locking invariants

1. V1 logical lock modes are exclusive `TUPLE_WRITE` and `UNIQUE_KEY`.
2. Readers evaluate MVCC visibility without shared tuple locks.
3. A tuple-write key is `(TableId, target physical RID)`.
4. A writer releases physical page/index latches before waiting for a logical transaction lock.
5. After acquiring/waiting for a tuple-write lock, the target is re-fetched and revalidated.
6. An aborted competing `xmax` is logically ineffective.
7. READ COMMITTED follows a fresh-snapshot statement retry after a committed competing update/delete.
8. REPEATABLE READ aborts on a post-snapshot competing committed write rather than following an invisible newer version.
9. The write protocol prevents lost updates on one target version.
10. Snapshot isolation may still permit cross-row write skew.
11. A unique-key lock is `(IndexId, full encoded non-NULL user-key bytes)`.
12. Hashing a unique key may select a lock-table shard but cannot replace full-key equality.
13. NULL-containing unique keys skip v1 duplicate rejection.
14. Unique checks inspect current transactional state of all physical matches, not only caller-snapshot visibility.
15. Tuple-write and unique-key locks are held until transaction end.
16. Deadlock correctness uses a wait-for graph; the initial victim is the highest TxnId in the cycle.
17. Page/B+ latches never become LockManager locks.
18. Logical transaction locks never substitute for physical B+ page-structure protection.
---

# 12. Write-Ahead Logging and Commit Durability

## 12.1 Scope and durability model

Write-ahead logging is mandatory.

The database uses:

```text
STEAL + NO-FORCE
```

Therefore:

- dirty data pages may reach their data files before the creating user transaction commits,
- commit does not force every heap/index/status/catalog page,
- WAL must contain enough durable redo and transaction-outcome information to reconstruct logical state after a crash.

The central ordering rule is:

> A WAL-protected persistent page MUST NOT become durable with a modification whose required recovery WAL is not already durable.

Ordinary user-DML heap/index changes use redo plus transaction-status visibility; they do not require physical user-DML undo.

## 12.2 Logical WAL stream and physical segments

The database has one logical WAL byte stream.

It is physically stored in fixed-size files under:

```text
wal/
```

with the legacy naming shape:

```text
0000000000000000.wal
0000000000000001.wal
...
```

The initial segment size is exactly:

```text
64 MiB
= 67,108,864 bytes
```

An LSN is the byte position of a WAL record in the logical WAL stream.

Consequently, for a logical byte position `L`:

```text
segment_index  = L / 67,108,864
segment_offset = L % 67,108,864
```

The exact filename-number radix/presentation beyond the locked example is not relied upon as a recovery semantic; segment order is determined by the logical segment index.

WAL segmentation provides bounded file units, recovery scan boundaries, and a natural recycling unit.

## 12.3 Record alignment and segment boundary

WAL records are aligned to 8-byte boundaries.

A normal logical record MUST NOT cross a 64 MiB segment boundary.

If the remaining bytes of a segment cannot hold the next record:

```text
emit/recognize WAL padding
advance to the next segment
write the record there
```

Recovery recognizes padding and does not interpret padding bytes as record payload.

A logical WAL record, including a `BTREE_MTR`, must be representable wholly within one segment after alignment.

The exact persisted encoding of the padding region and whether aligned tail padding contributes to `total_length` remain part of the WAL-format issue in Appendix D / the issue register.

## 12.4 WAL record header v1

Every ordinary WAL record begins with a 48-byte logical header:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `total_length` |
| `4` | 2 | `header_length = 48` |
| `6` | 2 | `record_type` |
| `8` | 4 | `flags` |
| `12` | 4 | `reserved` |
| `16` | 8 | `lsn` |
| `24` | 8 | `txn_id` |
| `32` | 8 | `prev_txn_lsn` |
| `40` | 4 | `payload_length` |
| `44` | 4 | `crc32c` |
|  | **48** | **total header** |

All integer fields use explicit little-endian serialization.

The record CRC32C covers:

```text
48-byte header with bytes 44..47 logically zero
+
exact semantic payload bytes
```

Segment/alignment padding bytes are not semantically part of the payload and are not included merely as payload bytes.

A record decoder validates lengths, alignment, segment containment, the embedded LSN, and CRC before exposing the record as valid.

The exact v1 numeric `record_type` codes, common WAL `flags` semantics, required value of header `reserved`, exact `total_length` versus alignment-padding convention, and byte-exact payload layouts are not fully specified by legacy §§215–255 and are tracked as R-025.

## 12.5 Per-transaction WAL chain

Each user transaction tracks:

```text
last_wal_lsn
```

and each of its WAL records carries:

```text
txn_id
prev_txn_lsn
```

forming a backwards per-transaction WAL chain.

The chain is retained even though v1 performs no physical ordinary-user-DML undo because it supports:

- diagnostics,
- tracing,
- recovery introspection,
- future savepoints/logical rollback work.

System records use:

```text
txn_id      = INVALID_TXN_ID = 0
prev_txn_lsn = INVALID_LSN   = 0
```

unless a future architecture explicitly creates a separate system-record chain.

## 12.6 Initial WAL record families

The initial logical record families are:

```text
WAL_PAD

PAGE_INIT
PAGE_DELTA
PAGE_IMAGE

BTREE_MTR

TXN_COMMIT
TXN_ABORT

CHECKPOINT_BEGIN
CHECKPOINT_DATA
CHECKPOINT_END
```

These names define semantic record families.

Their persistent numeric `record_type` assignments are not invented by Pass 8; R-025 tracks that byte-format completion.

The initial architecture deliberately uses generic page redo records plus the B+ atomic MTR record rather than requiring many subsystem-specific WAL classes.

## 12.7 PAGE_DELTA

`PAGE_DELTA` is a physiological redo record for exactly one page.

Its payload semantically contains:

```text
PageId
expected page type
patch_count
patches...
```

Each patch contains:

```text
offset
length
after-image bytes
```

The mutation code MUST ensure the patch set completely represents every persistent byte changed by the logged operation.

Ordinary MVCC user changes need redo after-images, not undo before-images.

Redo applies a valid delta only when:

```text
page.page_lsn < record.lsn
```

and after applying all patches sets:

```text
page.page_lsn = record.lsn
```

Patch bounds and arithmetic MUST be validated before modifying page bytes.

The byte widths/ordering of payload metadata such as `PageId`, patch count, patch offset, and patch length remain part of R-025.

## 12.8 PAGE_INIT

`PAGE_INIT` contains enough redo information to reconstruct one complete newly initialized non-B+ database page:

```text
PageId
full 8192-byte page after-image
```

It is used for page initialization such as:

- heap pages,
- FSM pages,
- transaction-status pages,
- catalog pages,
- other non-B+ ordinary page types.

B+ multi-page initialization may instead be contained in one `BTREE_MTR`.

Because PAGE_INIT already supplies a complete after-image, it satisfies the full-page-image requirement for that initial page state.

## 12.9 Full-page images and torn-data-page protection

Whole-page checksums detect torn/corrupt writes but do not repair them.

Version 1 therefore uses first-modification full-page images.

After a **completed checkpoint epoch**, the first WAL-protected modification of an existing persistent page must log a complete after-image of the resulting page state.

For a non-B+ ordinary page, that first modification uses:

```text
PAGE_IMAGE
```

instead of an ordinary `PAGE_DELTA`.

For a B+ MTR, the MTR embeds a full after-image for each affected page whose first-modification image is required.

Later modifications to the same page in that checkpoint epoch may use compact redo patches.

### 12.9.1 PAGE_IMAGE semantics

`PAGE_IMAGE` semantically contains:

```text
PageId
full 8192-byte after-image
```

and represents the complete page state at the image record's LSN.

During recovery:

```text
if the current data page is corrupt/torn
or current page_lsn < image_lsn:
    replace the complete page with the WAL image
```

then replay later deltas normally.

A resident buffer frame tracks which completed checkpoint/FPI epoch its current page has already satisfied.

### 12.9.2 Why the image epoch begins after checkpoint completion

The required chain is:

```text
completed checkpoint
    ↓
first later modification logs complete after-image
    ↓
later changes may use deltas
    ↓
a later data-page write may tear
    ↓
recovery restores the retained post-checkpoint image
    ↓
recovery replays later deltas
```

This provides recoverability from torn 8 KiB page writes without a v1 doublewrite buffer.

## 12.10 B+ tree mini-transactions

Every physical B+ tree mutation executes inside a short B+ tree mini-transaction (MTR), including:

- ordinary leaf entry insertion,
- exact leaf entry erasure,
- leaf/internal split,
- redistribution,
- merge,
- root replacement,
- tree-local free-list modification.

A B+ MTR is a system structural action:

```text
redoable
not rolled back because an owning user transaction aborts
```

User transaction visibility remains determined by the referenced heap tuple version.

### 12.10.1 Atomic BTREE_MTR record

One complete MTR is encoded as one logical:

```text
BTREE_MTR
```

record.

Its semantic payload contains:

```text
optional owner user TxnId for diagnostics
affected page count

for each affected page:
    PageId
    page type
    either:
        full 8192-byte after-image
    or:
        redo byte patches

allocation/free metadata changes
```

Every page modified by that MTR receives:

```text
page_lsn = BTREE_MTR.lsn
```

The complete valid record is the crash-recovery atomicity boundary.

The exact payload discriminators, counts, and field widths remain part of R-025.

### 12.10.2 MTR no-flush barrier

While an MTR is being constructed, every affected buffer frame is temporarily non-flushable.

The required publication protocol is:

```text
1. acquire/latch the required affected pages
2. install one valid final runtime tree state
3. build the complete BTREE_MTR redo payload
4. append the complete WAL record
5. assign every affected page_lsn = MTR LSN
6. remove the no-flush barriers
7. release page latches/pins
```

If the process fails before the record append completes:

- the incomplete/torn WAL record is not a valid record,
- protected dirty frames were not eligible for data-file write,
- partial MTR state could not become durable through normal BufferPool writeback.

An MTR append does **not** require its own `fdatasync`.

A later eviction/writeback may force WAL durability through the MTR LSN.

### 12.10.3 User abort does not reverse B+ physical shape

If an uncommitted user INSERT creates:

```text
heap version xmin = T
B+ entry -> new RID
```

and causes a split, then user abort means:

```text
heap version invisible
physical index entry may remain
split/root/tree shape remains
```

Vacuum later removes the exact garbage `(key,RID)` entry.

User abort never tries to reverse the split/merge/root structural action.

## 12.11 Heap redo before index MTR

When a DML operation creates a new physical heap tuple version and B+ entries that reference its RID:

```text
first establish/log heap tuple-version redo
then append the B+ MTR(s) that reference that RID
```

Therefore:

```text
heap redo LSN < later index MTR LSN
```

for the relevant dependency.

If an index page is forced before user commit, flushing WAL through the later index MTR also makes the earlier heap redo record durable.

The user transaction may still abort; MVCC status handles visibility.

## 12.12 WAL append buffer

`WalManager` owns an in-memory append buffer.

The initial target capacity is:

```text
8 MiB
```

and is configurable.

Conceptual append:

```text
serialize record
assign its LSN
copy complete record bytes into WAL buffer/reservation
advance the WAL append position
```

The initial implementation MAY serialize append/reservation under one mutex.

The architecture leaves room for later measured alternatives such as:

- atomic range reservation,
- per-thread staging,
- a larger/ring-style buffer.

Lock-free WAL append is not required for correctness.

## 12.13 WAL writer and durable LSN

A dedicated WAL writer/flusher:

- writes contiguous WAL bytes to segment files,
- executes `fdatasync` when a durability request requires it,
- advances a monotonic atomic `durable_lsn`,
- wakes waiters whose target record LSN has become durable.

`durable_lsn >= X` means the complete valid WAL record whose LSN is `X`, and all preceding required WAL bytes, are durable.

Background WAL may be written periodically without an explicit commit request.

The initial background flush interval is approximately:

```text
10 ms
```

as a tuning default, not a persistent-format invariant.

The WAL subsystem, not DiskManager's logical API, owns WAL durability scheduling and `durable_lsn`.

## 12.14 Group commit and CommitCoordinator

A normal commit does not execute one independent filesystem synchronization per transaction.

Conceptually, if waiting commits target:

```text
T1 -> 1000
T2 -> 1100
T3 -> 1250
```

the flusher may:

```text
write WAL through the record at 1250
fdatasync
publish durable_lsn >= 1250
wake T1, T2, T3
```

One durability operation may acknowledge many commits.

`CommitCoordinator` conceptually exposes:

```text
WaitUntilDurable(commit_lsn)
```

and batches naturally by waiting on monotonic `durable_lsn`, rather than enqueuing one fsync task per transaction.

Commit latency observability should distinguish:

```text
queue delay
WAL write time
fdatasync time
group size
```

## 12.15 Synchronous commit

Version 1 has synchronous commit.

For a transaction with persistent writes, successful COMMIT cannot return until:

```text
its TXN_COMMIT WAL record is durable
```

The detailed transaction state/status/lock-release sequence is canonical in §9.14.1 and later end-to-end write-protocol material.

Commit does not force the transaction's heap or B+ pages.

Asynchronous commit remains deferred and, if introduced, must expose its weaker durability semantics explicitly.

## 12.16 BufferPool recLSN

Every WAL-protected dirty buffer frame tracks:

```text
rec_lsn
```

meaning:

> the first WAL LSN that made the frame dirty since its last successful data-file flush.

Transitions are:

```text
clean -> dirty at modification_lsn X:
    dirty   = true
    rec_lsn = X

already dirty -> later modification:
    preserve existing rec_lsn
```

After a successful page write, dirty state can be cleared only if synchronization proves no newer in-memory modification raced the image written.

Only then:

```text
dirty   = false
rec_lsn = INVALID_LSN
```

`rec_lsn` is process-local buffer metadata.

It drives the dirty-page table used by fuzzy checkpointing and recovery.

## 12.17 WAL-before-data and temporary no-flush state

Before BufferPool writes any WAL-protected dirty page with:

```text
page_lsn = X
```

it must establish:

```text
WalManager.durable_lsn >= X
```

This applies to BufferPool-managed:

- heap pages,
- transaction-status pages,
- WAL-protected FSM pages,
- catalog pages,
- B+ pages,
- superblock/control-like database pages when routed through BufferPool.

B+ MTR no-flush barriers add a stronger temporary requirement:

```text
the frame is not flushable at all
until the complete BTREE_MTR record exists and page_lsn is installed
```

Only after that barrier is removed does ordinary WAL-before-data writeback apply.

## 12.18 WAL and commit invariants

1. The database has one logical WAL stream segmented into 64 MiB files.
2. Every ordinary WAL record begins on an 8-byte boundary and does not cross a segment boundary.
3. The ordinary WAL record header is exactly 48 bytes.
4. WAL CRC32C covers the header with its CRC field zeroed plus semantic payload bytes.
5. User transaction records maintain `prev_txn_lsn`; system records use transaction/previous LSN zero unless explicitly extended.
6. PAGE_DELTA is redo-only and advances page LSN to its record LSN when applied.
7. PAGE_INIT and PAGE_IMAGE contain full 8192-byte page after-images.
8. The first modification after a completed checkpoint epoch provides a full after-image unless PAGE_INIT already provides one for the page's initial state.
9. Every physical B+ tree mutation is an atomic redoable MTR system action.
10. Every MTR-modified page receives one common MTR LSN.
11. A frame protected by an MTR no-flush barrier cannot be written before the complete MTR WAL record exists.
12. Heap tuple-version redo precedes index MTR redo that references the new RID.
13. `durable_lsn` advances monotonically only after the relevant WAL bytes have reached durable storage.
14. Group commit is the normal commit durability path.
15. Successful v1 COMMIT with persistent writes waits for its TXN_COMMIT record to be durable.
16. `rec_lsn` is the first modification LSN of the frame's current dirty interval.
17. WAL-protected data-page writeback requires durable WAL through the page LSN.

---

# 13. Checkpointing and Crash Recovery

## 13.1 Scope and recovery model

Crash recovery is ARIES-inspired in its use of:

- LSNs,
- page LSNs,
- dirty-page recLSNs,
- fuzzy checkpoints,
- analysis,
- redo,
- transaction outcome reconstruction.

Version 1 deliberately replaces generic physical user-DML undo with:

```text
analysis
redo
loser-transaction status resolution
```

Ordinary aborted/crash-loser heap/index bytes may remain physically present.

Correctness is restored through terminal transaction status plus MVCC visibility.

No ordinary user-DML CLR stream is required.

## 13.2 Database control file

Global recovery/bootstrap metadata lives in:

```text
database.control
```

The file is exactly:

```text
8192 bytes
```

and consists of two alternating:

```text
4096-byte control slots
```

Each slot stores at least the semantic fields:

```text
magic
format_version
generation
latest_checkpoint_lsn
checkpoint_redo_lsn
reserved_txn_id_end
next_file_id
control flags
CRC32C
```

Update protocol:

```text
select inactive/older slot
write next complete slot generation
synchronize the control file
on startup choose the valid slot with highest generation
```

CRC plus alternating generations allows startup to reject a torn/incomplete newest slot and use the previous valid generation without requiring atomic 8 KiB writes.

The complete byte offsets/widths, magic bytes, flag semantics, initial slot values, CRC coverage, and precise generation-wrap policy are not specified by legacy §§237–238 and are tracked as R-026.

## 13.3 Control-file update frequency

`database.control` is durably rewritten only for important global metadata transitions such as:

- durable transaction-ID block reservation,
- successful checkpoint installation,
- future database-format/global metadata changes.

Ordinary transactions do not rewrite/synchronize the control file.

This keeps commit scalability independent of control-file update latency.

## 13.4 Fuzzy checkpoint goal

A checkpoint does not stop transaction processing merely to force every dirty page.

It captures enough WAL/recovery state to bound later crash recovery while ordinary transactions and page modifications continue.

A completed fuzzy checkpoint does **not** imply that all pages dirtied before it are already in their data files.

## 13.5 Checkpoint protocol

The canonical conceptual sequence is:

```text
1. append CHECKPOINT_BEGIN
2. capture the dirty-page table
3. capture active writer transaction state
4. capture relevant global metadata
5. append one or more CHECKPOINT_DATA records
6. append CHECKPOINT_END
7. flush WAL through CHECKPOINT_END
8. install the checkpoint pointer/redo bound in database.control
9. durably synchronize that control update
10. advance the completed checkpoint/FPI epoch
```

The installed checkpoint becomes authoritative only after:

```text
CHECKPOINT_END is durable
and
database.control identifies the completed checkpoint
```

No full database-page flush is required in this path.

Checkpoint construction may overlap ongoing transactions and dirtying.

The exact persistent linkage/identifier proving that a sequence of BEGIN/DATA/END records forms one complete checkpoint is not specified by legacy §§240–243 and is tracked as R-027.

## 13.6 Dirty-page table

The BufferPool's process-local dirty-page table/checkpoint representation stores, for each captured dirty page:

```text
PageId
rec_lsn
```

The checkpoint DPT includes every page that:

```text
was already dirty before CHECKPOINT_BEGIN
and
remains dirty when its state is captured
```

Pages dirtied after CHECKPOINT_BEGIN can be discovered by forward WAL analysis after the checkpoint start.

Frame enumeration/capture synchronization MUST prevent a pre-existing dirty page from being silently omitted.

`rec_lsn` retains its §12.16 meaning:

```text
first WAL LSN in the frame's current dirty interval
```

## 13.7 Active writer transaction checkpoint state

Checkpoint transaction state includes every transaction that:

```text
has produced persistent WAL state
and
is not terminal
```

Store at least:

```text
TxnId
last_wal_lsn
```

A transaction with no persistent writes need not appear.

This allows recovery analysis to rediscover a writer that modified pages before checkpoint begin and then crashed without any later WAL record.

## 13.8 Checkpoint record chunking and completeness

Checkpoint state may exceed one WAL record.

The sequence may therefore be:

```text
CHECKPOINT_BEGIN
CHECKPOINT_DATA
CHECKPOINT_DATA
...
CHECKPOINT_END
```

`CHECKPOINT_END` identifies completion of the checkpoint sequence.

An incomplete checkpoint sequence is ignored.

`database.control` is updated only after its `CHECKPOINT_END` is durable.

The exact CHECKPOINT_DATA entry encoding and the byte-exact BEGIN/DATA/END association mechanism remain R-025/R-027 rather than being invented in Pass 8.

## 13.9 Checkpoint redo start

The checkpoint redo bound is:

```text
redo_start_lsn =
    minimum rec_lsn in the captured DPT
```

If the DPT is empty, use the checkpoint's own appropriate start LSN, with `CHECKPOINT_BEGIN` as the legacy v1 example.

Persist the resulting bound as:

```text
database.control.checkpoint_redo_lsn
```

Recovery can begin page redo no earlier than this bound once checkpoint metadata has been reconstructed.

## 13.10 WAL recycling

Version 1 has no replication or point-in-time archive retention requirement.

A WAL segment can be recycled/deleted only when the **entire segment** lies before the oldest LSN still required by installed recovery state.

The conservative v1 retention bound is:

```text
retain from min(
    latest_checkpoint_begin_lsn,
    latest_checkpoint_redo_lsn
)
```

and also retain any segment containing records of the active installed checkpoint sequence.

WAL retention is correctness-sensitive; the engine MUST prefer retaining too much WAL over deleting a segment that may still be needed for redo or torn-page reconstruction.

## 13.11 Recovery startup and WAL-tail validation

Crash recovery begins by:

1. reading both control slots and selecting the highest-generation valid one,
2. locating the installed checkpoint / WAL scan position,
3. scanning WAL forward,
4. validating record length, 8-byte alignment, segment containment, embedded metadata, and CRC32C,
5. stopping at the first invalid/incomplete/torn tail record,
6. ignoring or truncating bytes after the last complete valid record.

A torn WAL tail is normal crash behavior when all earlier required records validate.

Corruption before the valid tail boundary is not silently treated as ordinary tail truncation.

## 13.12 Recovery phase 1: analysis

Analysis reconstructs at least:

```text
dirty-page table
active/loser writer transaction table
terminal transaction outcomes
maximum observed TxnId
checkpoint state
latest valid WAL end
```

Start from the latest installed valid checkpoint metadata when available, then scan required WAL forward.

### 13.12.1 Page records

For each redoable ordinary page record, if the page is not already in the reconstructed DPT:

```text
DPT[PageId] = record.lsn
```

### 13.12.2 BTREE_MTR

For every page affected by a valid MTR, if absent:

```text
DPT[PageId] = mtr.lsn
```

### 13.12.3 Transaction terminal records

```text
TXN_COMMIT -> transaction terminal outcome COMMITTED
TXN_ABORT  -> transaction terminal outcome ABORTED
```

At analysis end, any writer transaction known nonterminal and lacking a valid terminal record is a crash loser.

Analysis also observes TxnIds required to prevent allocator reuse after restart; exact post-recovery allocator reconciliation is completed with the database-control/transaction protocols.

## 13.13 Recovery phase 2: redo

Redo begins at:

```text
minimum reconstructed DPT rec_lsn
```

or the appropriate installed checkpoint bound when the DPT is empty.

For each valid redoable record:

### PAGE_DELTA / PAGE_IMAGE / PAGE_INIT

Use DPT/page-LSN logic to skip a page that already reflects the required WAL state.

Otherwise apply the record according to Chapter 12.

### BTREE_MTR

A complete valid `BTREE_MTR` is one committed system mini-transaction for recovery purposes.

For each affected page:

```text
if page_lsn < mtr.lsn:
    apply the page's full image or redo patches
    set page_lsn = mtr.lsn
```

A torn/incomplete MTR record is not a valid record and is never partly replayed.

## 13.14 Torn/corrupt data pages during redo

If a data-page checksum is invalid during recovery:

```text
do not trust its stored page_lsn
```

Recovery must find an applicable retained complete page image from:

```text
PAGE_INIT
PAGE_IMAGE
full-image portion of BTREE_MTR
```

restore that page state, then apply later valid deltas/MTR changes.

If WAL-retention/checkpoint invariants say the required full image should exist but no valid recoverable image is available, recovery reports unrecoverable corruption.

It MUST NOT guess page contents or trust a torn page's LSN.

## 13.15 Recovery phase 3: loser resolution

For each crash-loser **user** transaction:

```text
terminal outcome = ABORTED
```

No heap/index byte-by-byte undo is performed.

Before normal SQL traffic begins, recovery:

```text
1. appends recovery TXN_ABORT records for unresolved losers
2. updates the relevant transaction-status entries to ABORTED
3. assigns transaction-status page_lsn from terminal WAL where appropriate
4. durably flushes the WAL required for those terminal outcomes
5. completes and installs a recovery checkpoint
```

This makes the repaired terminal outcomes durable and avoids repeatedly rediscovering the same loser set on every subsequent restart.

## 13.16 No ordinary user-DML CLRs

Compensation log records are not required for ordinary v1 user transactions because recovery does not physically undo their heap/index DML.

The architecture must not add CLRs solely to resemble textbook ARIES terminology.

If a future subsystem introduces physical undo that itself must be crash-restartable, that subsystem may introduce an appropriate CLR/undo-progress mechanism.

## 13.17 Recovery of transaction-status pages

`TXN_COMMIT` and `TXN_ABORT` WAL records are authoritative terminal-outcome evidence.

During recovery, terminal records may repair/update stale transaction-status pages even when their prior data-file image did not include the status change.

When a terminal status record at LSN `X` is reflected into a status page:

```text
status_page.page_lsn = X
```

where appropriate for that page mutation.

This is why normal commit may acknowledge after durable commit WAL without forcing the status page itself.

## 13.18 Approximate/rebuildable metadata

Approximate performance metadata does not become a mandatory crash-consistency dependency merely because it is persistent.

Examples allowed to be stale/rebuildable include:

```text
FSM free-space estimates
some optimizer statistics
non-critical pruning hints
```

After recovery such metadata may be repaired:

- lazily,
- by scanning authoritative storage,
- by vacuum/analyze or its owning maintenance subsystem.

Correctness-critical transaction/status/page metadata is not downgraded to this category.

## 13.19 Recovery completion gate

After a crash, the database does not accept normal SQL traffic until all required recovery gates hold:

```text
valid WAL tail established
analysis complete
redo complete
losers published ABORTED
transaction status consistent
B+ structural MTRs recovered
control metadata valid
recovery checkpoint installed
```

Only then may database state become:

```text
ONLINE
```

## 13.20 Crash-recovery correctness principle

For a transaction whose COMMIT returned success:

```text
after every later successful crash/restart,
its durable logical changes remain committed
```

If COMMIT had not become durable before the crash:

```text
recovery may classify the transaction as aborted
```

For an explicitly aborted or crash-loser transaction:

```text
physical heap/index garbage may remain
but
no SQL-visible state may depend on that transaction as committed
```

This separation between physical residue and logical visibility is central to the v1 recovery architecture.

## 13.21 Recovery invariants

1. A completed checkpoint does not require all dirty data pages to have been flushed.
2. The authoritative installed checkpoint has a durable CHECKPOINT_END and a valid control-file pointer/generation.
3. A checkpoint DPT entry stores `(PageId, rec_lsn)`.
4. Checkpoint active-writer state includes nonterminal transactions with persistent WAL state.
5. Incomplete checkpoint sequences are ignored.
6. WAL recycling never removes a segment still needed by the installed checkpoint/redo/torn-page-recovery state.
7. Recovery validates and truncates/ignores only the invalid WAL tail; earlier malformed required WAL is not silently accepted.
8. Analysis reconstructs dirty pages, writer outcomes/losers, TxnId high-water information, checkpoint state, and valid WAL end.
9. Redo is page-LSN idempotent for valid ordinary page records.
10. A complete BTREE_MTR is replayed as one valid system structural action; a torn MTR is not partially replayed.
11. A corrupt/torn page LSN is not trusted until the page is reconstructed from a retained full image.
12. Crash losers are resolved by publishing ABORTED, not by physical ordinary-user-DML undo.
13. Ordinary v1 user-DML recovery requires no CLRs.
14. Terminal WAL records can reconstruct stale transaction-status pages.
15. Rebuildable approximate metadata is not on the critical recovery path.
16. SQL traffic begins only after recovery completion and a recovery checkpoint.
17. A transaction acknowledged committed remains logically committed across later successful crash/restart.
---

# 14. Vacuum and Storage Reclamation

## 14.1 Scope

Vacuum converts transaction-history facts into safe physical reclamation.

It owns:

- the global snapshot reclamation horizon,
- tuple-version garbage eligibility,
- exact secondary-index cleanup,
- persistent `NORMAL -> DEAD` retirement,
- read-epoch-delayed `DEAD -> UNUSED` physical RID reuse,
- version-chain splicing before reused storage can be referenced,
- cleanup of aborted `xmax`,
- freezing of sufficiently old committed creators,
- transaction-status retention/truncation eligibility,
- FSM maintenance after reclamation,
- vacuum maintenance counters.

Vacuum does **not** decide tuple visibility using only its own snapshot.

Vacuum does **not** wait for ordinary transactions while holding heap or B+ page latches.

## 14.2 Global snapshot horizon

`SnapshotManager` maintains the registry of active SQL snapshots.

Define:

```text
global_oldest_snapshot_xmin =
    minimum snapshot.xmin among all currently registered SQL snapshots
```

If no SQL snapshot is registered:

```text
global_oldest_snapshot_xmin =
    current next_txn_id
```

The authoritative reclamation horizon is therefore active SQL snapshots, not merely transactions that currently exist.

This distinction matters because:

- a READ COMMITTED transaction between statements may have no registered statement snapshot,
- a REPEATABLE READ transaction retains one transaction snapshot,
- a long-running query retains its statement snapshot for the executor lifetime.

A long-running registered snapshot may intentionally delay reclamation of versions it could still observe.

## 14.3 Tuple-version garbage eligibility

A physical tuple version is a vacuum garbage candidate when either rule below is satisfied.

### 14.3.1 Aborted creator

```text
Status(xmin) == ABORTED
```

Such a version never became logically visible as a committed row version.

### 14.3.2 Globally dead committed version

The creator is committed or frozen, and:

```text
xmax != INVALID_TXN_ID
Status(xmax) == COMMITTED
xmax < global_oldest_snapshot_xmin
```

together with any active-snapshot consistency check required by the SnapshotManager representation.

At that point no currently registered legal snapshot can require the old version.

Garbage eligibility is a **global** property.

Vacuum MUST NOT reclaim a version merely because the version is invisible to the vacuum worker's own snapshot.

## 14.4 In-progress transaction metadata

Vacuum does not reclaim a version based on:

```text
xmin IN_PROGRESS
or
xmax IN_PROGRESS
```

Such a candidate is skipped/retried later.

Vacuum MUST NOT wait for the owning transaction while retaining heap/B+ physical latches.

## 14.5 Two-phase physical RID reclamation

Secondary indexes store a physical RID without a generation counter.

Therefore a physical `(PageNo, SlotId)` must not be reused while an already-running reader may still possess an old index-derived RID with that identity.

Reclamation is:

```text
NORMAL
    ↓
exact index cleanup + semantic retirement
    ↓
DEAD
    ↓
read-epoch grace period
    ↓
UNUSED / reusable
```

`DEAD` means:

```text
the old tuple version is no longer semantically required
all required secondary-index entries have been removed
the physical RID is still not reusable
```

This rule also governs whole heap-page recycling when reinitialization would make old `(PageNo, SlotId)` identities reusable.

## 14.6 ReadEpochManager

Physical RID identity safety uses a lightweight `ReadEpochManager`, separate from MVCC snapshot visibility.

Every executing SQL statement/executor that may consume stored RIDs registers a `read_epoch` for the period in which it may retain or later dereference such RIDs.

The manager tracks conceptually:

```text
global monotonically increasing epoch
active reader epochs
```

When vacuum transitions:

```text
NORMAL -> DEAD
```

it associates that retired RID with a retirement epoch.

The RID may become `UNUSED` only after every active reader epoch that could have observed the old index entry has exited.

The legacy contract does not specify the exact epoch-allocation/advance operation or exact arithmetic grace predicate. Those details remain R-028 rather than being inferred.

## 14.7 Why MVCC visibility is insufficient for RID reuse

An index cursor may obtain:

```text
(key, old RID)
```

and delay fetching the corresponding heap page.

Meanwhile vacuum may remove the index entry.

If the physical RID were immediately reused for an unrelated tuple, the old cursor could dereference a physical identity whose meaning changed after the cursor observed it.

MVCC visibility alone is not the physical-identity guarantee.

The read-epoch grace period establishes:

> a physical RID does not change identity while an already-running reader may still possess that old RID.

## 14.8 Persistent DEAD state and crash behavior

`DEAD` is a persistent heap-slot state.

After WAL-protected index cleanup and retirement, a crash may leave either safe state:

```text
NORMAL
    -> vacuum must reconsider cleanup

DEAD
    -> required index entries were already removed
```

Pre-crash read-epoch registrations do not survive a process crash.

After recovery there are no pre-crash active readers, so a recovered `DEAD` slot may later satisfy the grace requirement without persisting old process-local epoch registrations.

The persistent `DEAD` state is what makes this restart boundary safe.

## 14.9 Vacuum index-cleanup protocol

For one garbage-eligible tuple version with physical RID `R`:

```text
1. read/copy enough tuple data to derive every indexed user key
2. re-check garbage eligibility
3. for every index:
       Erase(encoded_user_key, R)
       through ordinary B+ system MTRs
4. ensure those index mutations are logically installed
5. re-fetch the heap page
6. under write latch verify tuple/slot identity and expected header state
7. WAL-log/install:
       slot NORMAL -> DEAD
8. record the RID retirement epoch
```

The heap slot MUST NOT become `DEAD` before every required secondary-index entry is removed.

Vacuum performs B+ operations without retaining the heap-page latch.

## 14.10 Version-chain splicing

Before a retired RID becomes reusable, no surviving tuple version may retain a `prev` pointer to that soon-to-be-reused physical storage.

During a table vacuum pass, vacuum derives enough reverse-link information to locate surviving direct successors.

For a removable version `V`:

```text
V.prev = P
```

each surviving direct successor `S` whose current link is:

```text
S.prev = V
```

is rewritten to:

```text
S.prev = P
```

The successor-header mutation is WAL logged.

Only after required surviving links have been safely updated may reclamation of `V` progress toward RID reuse.

If the expected relationship cannot be proven under revalidation, vacuum defers that version to a later pass.

Version chains are not required for ordinary snapshot visibility, so shortening a chain is permitted when it leaves no surviving pointer to reusable storage.

## 14.11 Revalidation under concurrent activity

Before changing physical slot/header state, vacuum revalidates under the heap-page latch at least:

```text
slot state
xmin
xmax
expected version-chain fields
physical tuple identity
```

If the expected candidate state changed, the operation is skipped/retried.

Heap-page latches are not held while B+ tree cleanup occurs.

## 14.12 DEAD to UNUSED

After the read-epoch grace condition is satisfied:

```text
1. fetch heap page
2. write-latch it
3. verify the slot is still DEAD
4. WAL-log/install DEAD -> UNUSED
5. optionally compact reclaimable tuple bytes
6. update free-space metadata
```

Only after this transition may a future tuple version reuse that `SlotId`.

## 14.13 Metadata normalization and freezing

### 14.13.1 Aborted xmax

For a still-live version whose deleter outcome is:

```text
Status(xmax) == ABORTED
```

vacuum may WAL-log and normalize:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

No index mutation is required merely to remove an aborted `xmax`.

### 14.13.2 Frozen committed creator

For a live version whose creator is committed and older than every active snapshot that could distinguish that original creator, vacuum may WAL-log:

```text
xmin = FROZEN_TXN_ID
cmin = 0
```

Future visibility then treats the creator as committed without requiring the original transaction-status entry.

## 14.14 Transaction-status retention and reclamation

After sufficiently complete vacuum/freezing proves a cutoff `X` such that:

```text
no persistent tuple/catalog correctness object
references a normal TxnId below X
```

whole transaction-status pages whose represented TxnIds are strictly below `X` become semantically eligible for retirement.

The motivation is bounded status-storage growth, not TxnId wraparound.

The current absolute TxnId-to-status-PageNo mapping in §9.12 does not itself define how eligible **prefix** pages can be physically removed while preserving deterministic lookup.

Physical prefix truncation/remapping therefore remains R-029.

Until R-029 is resolved, retirement eligibility is defined but physical prefix compaction/re-numbering MUST NOT be guessed.

## 14.15 B+ garbage cleanup

Vacuum removes exact physical B+ entries that reference:

```text
aborted-created tuple versions
globally dead committed tuple versions
```

Because the B+ physical key is `(user_key, RID)`, cleanup removes exactly the stale physical entry.

Resulting underflow, redistribution, merge, or root contraction remains an ordinary B+ MTR system action.

## 14.16 FSM and maintenance statistics

After heap reclamation or repacking:

```text
update the table FSM estimate
```

FSM remains advisory and rebuildable.

Vacuum may batch FSM estimate changes rather than place every estimate update on the critical transactional path.

Vacuum may collect maintenance counters such as:

```text
live tuple versions
dead tuple versions
aborted versions
pages scanned
pages compacted
index entries removed
bytes reclaimed
frozen tuples
```

Optimizer statistics remain the responsibility of ANALYZE, though vacuum may later trigger or assist that subsystem.

## 14.17 Vacuum execution baseline

The architecture requires an explicit/manual vacuum path, for example:

```text
VACUUM table
```

or an equivalent internal maintenance invocation.

A sophisticated autovacuum scheduler is not required for the baseline.

Background scheduling may later react to measured dead-version ratio, aborted-version pressure, freezing/status pressure, or table growth.

The scheduling policy is operational, not part of tuple-reclamation correctness.

## 14.18 Vacuum/reclamation invariants

1. The global reclamation horizon is derived from registered SQL snapshots, not merely transaction existence.
2. Vacuum never uses only its own snapshot visibility as the garbage criterion.
3. In-progress creator/deleter state is not reclaimed by guesswork.
4. Required secondary-index entries are removed before `NORMAL -> DEAD`.
5. `DEAD` is persistent and means cleaned but not yet physically reusable.
6. Physical RID reuse requires a read-epoch grace period beyond semantic death.
7. Whole heap-page recycling cannot bypass RID-reuse safety.
8. A surviving version never keeps `prev` pointing at storage that may be reused.
9. Candidate state is revalidated under page latch before physical transition.
10. Vacuum never waits for ordinary transactions or performs B+ cleanup while holding the heap-page latch.
11. `DEAD -> UNUSED` is WAL protected and occurs only after the grace condition.
12. Aborted `xmax` cleanup does not require an index change.
13. Freezing is WAL protected and makes original creator status unnecessary for future visibility.
14. Status-page retirement requires proof that persistent correctness state no longer references the retired TxnId range.
15. B+ cleanup remains a B+ MTR system action.
16. FSM/statistical maintenance cannot weaken reclamation correctness.

---

# 15. Transactional Write Protocols

## 15.1 Scope and ownership

This chapter integrates the component contracts from heap/tuple storage, FSM, BufferPool, B+ tree, transactions/snapshots, LockManager, WAL/CommitCoordinator, and vacuum.

It defines cross-subsystem ordering for user DML and transaction completion.

The owning subsystem chapters remain authoritative for their local byte formats, visibility algorithms, lock semantics, WAL codecs, and recovery rules.

These responsibilities MUST NOT collapse into one monolithic “transaction engine” abstraction that hides ownership/lifetime boundaries.

## 15.2 INSERT

For an INSERT:

```text
1. determine the statement CommandId

2. encode the new tuple version:
       xmin = current TxnId
       xmax = INVALID_TXN_ID
       cmin = current CommandId

3. acquire every required non-NULL UNIQUE_KEY lock
4. perform current-state uniqueness checks
5. choose/validate a heap page using the advisory FSM
6. construct/append the required heap PAGE_INIT/PAGE_IMAGE/PAGE_DELTA redo
7. install the heap tuple version and corresponding page_lsn

8. for every index:
       perform the B+ MTR inserting
       (encoded_user_key, RID)

9. release short-lived heap/B+ page latches/pins
10. retain transaction-lifetime UNIQUE_KEY locks until COMMIT/ABORT
```

Heap redo describing the referenced RID is established before any B+ MTR that references that RID.

If the transaction aborts:

```text
new xmin -> ABORTED
tuple version -> logically invisible
physical index entries may remain
vacuum removes the garbage later
```

## 15.3 UPDATE

For each target row:

```text
1. obtain candidate visible old RID from the scan
2. release short-lived page/index latches
3. acquire TUPLE_WRITE(TableId, old RID)
4. re-fetch and revalidate the old version
5. apply isolation-specific write-conflict rules
6. acquire affected UNIQUE_KEY locks in deterministic encoded-key order
7. validate current-state uniqueness for new unique key(s)

8. create new tuple version:
       xmin = current TxnId
       cmin = current CommandId
       prev = old RID

9. WAL-log/install the new tuple version

10. WAL-log/install old-version header:
       xmax = current TxnId
       cmax = current CommandId

11. install new physical B+ entries through MTRs
12. retain old physical B+ entries
13. hold logical tuple/unique locks until transaction end
```

If the transaction aborts:

```text
old xmax -> aborted and ineffective
new xmin -> aborted and invisible
new index entries -> vacuumable garbage
```

No physical rollback is required.

## 15.4 DELETE

For each target row:

```text
1. obtain visible RID
2. release page/index latches
3. acquire TUPLE_WRITE(TableId, RID)
4. re-fetch and revalidate
5. apply write-conflict rules
6. acquire affected non-NULL UNIQUE_KEY locks
7. WAL-log/install:
       xmax = current TxnId
       cmax = current CommandId
8. retain existing physical index entries
9. hold logical locks until transaction end
```

Commit makes the version dead to sufficiently new snapshots.

Abort makes the `xmax` ineffective.

Vacuum removes tuple/index garbage only after Chapter 14's global reclamation rules are satisfied.

## 15.5 COMMIT

For a transaction with persistent writes:

```text
1. state ACTIVE -> COMMITTING
2. append TXN_COMMIT(txn_id, prev_txn_lsn)
3. submit commit_lsn to CommitCoordinator
4. wait until durable_lsn >= commit_lsn
5. publish COMMITTED in runtime/status storage
6. set affected transaction-status page_lsn = commit_lsn
7. release TUPLE_WRITE and UNIQUE_KEY locks
8. unregister active snapshot(s)
9. remove transaction from active transaction registry
10. state -> COMMITTED
11. return success
```

Commit does **not** force dirty heap/index pages.

Once a valid `TXN_COMMIT` record is durable, that transaction outcome cannot subsequently become ABORTED.

## 15.6 ABORT

For an abortable transaction:

```text
1. state ACTIVE/eligible transient state -> ABORTING
2. if persistent WAL-visible state exists:
       append TXN_ABORT
3. publish ABORTED
4. if an abort record exists:
       set transaction-status page_lsn to that abort LSN
5. release logical locks
6. unregister snapshots
7. remove transaction from active registry
8. state -> ABORTED
9. return/raise abort
```

Ordinary abort performs no write-set scan to restore old heap/index bytes.

Aborted physical versions and index entries remain vacuum input.

A transaction whose COMMIT is already durable is no longer eligible to transition to ABORTED.

## 15.7 READ COMMITTED retry boundary

An internal READ COMMITTED write retry restarts at a defined statement execution boundary.

A retryable statement MUST NOT expose irreversible external result rows or side effects before it is known not to restart.

For the initial SQL surface:

```text
DML without external side effects:
    retryable

RETURNING:
    buffer output until the statement crosses its retry-safe point
```

A retry captures a fresh READ COMMITTED statement snapshot and re-evaluates the candidate search/predicate as defined in Chapter 11.

Future features with external effects must define their retry semantics before becoming retryable.

## 15.8 Cross-layer contract

Higher catalog/SQL/planning/execution layers consume this transactional storage contract.

They MUST NOT redefine:

- MVCC creator/deleter visibility,
- write-conflict outcome,
- unique-key serialization,
- WAL-before-data,
- commit durability,
- vacuum/RID reuse safety.

This chapter is the boundary between the persistent transactional storage core and upper semantic/execution layers.

## 15.9 End-to-end invariants

1. User data is never published committed before durable commit WAL.
2. COMMIT remains NO-FORCE for heap/index pages.
3. Logical lock waits occur outside physical page/B+ latch waits.
4. Index entries never decide MVCC visibility by themselves.
5. UPDATE/DELETE revalidate the target after logical-lock acquisition.
6. READ COMMITTED retries from a statement boundary with a fresh statement snapshot.
7. REPEATABLE READ aborts on a conflicting committed post-snapshot write.
8. User abort changes logical outcome without requiring physical heap/index rollback.
9. B+ structural MTRs may survive user abort.
10. Transaction-lifetime tuple/unique locks are released only after terminal outcome publication.
11. Vacuum performs delayed exact index garbage removal.
12. External output from a potentially retryable statement does not escape before its retry-safe boundary.
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

## 39.1 Transaction, durability, and recovery errors

The transaction/storage subsystem distinguishes at least:

```text
SerializationFailure
DeadlockVictim
UniqueViolation
TransactionAborted
LockCancelled
WalIOError
RecoveryError
CorruptionError
```

These are not collapsed into one generic internal-error category.

A later SQL layer may map them to SQLSTATE-like surface codes without erasing the underlying distinction.

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

## 40.3 Transaction, WAL, recovery, and vacuum metrics

The engine exposes enough counters to diagnose both correctness and performance, including:

```text
transactions begun / committed / aborted
deadlock victims
serialization failures
statement retries
active snapshot count
oldest snapshot xmin
snapshot active-set sizes
tuple-lock waits
unique-lock waits
deadlock cycles
WAL bytes appended / synced
WAL flush count
group-commit batches and group size
commit wait latency
checkpoint count / duration
dirty-page-table size
redo-start distance
recovery records scanned
pages redone
full-page images restored
loser transactions
vacuum tuples examined
versions reclaimed
index entries removed
RID retire queue size
frozen versions
```

Exact counter names may evolve, but these observability dimensions are architectural.

## 40.4 Transaction/recovery debug introspection

Internal/debug interfaces or test hooks SHOULD permit inspection of:

```text
active transactions
active snapshots
transaction status by TxnId
logical lock table
wait-for graph
durable_lsn
current WAL end
dirty-page table
latest checkpoint
vacuum horizon
RID retirement epochs
```

These interfaces need not be stable SQL-user APIs.

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

## 41.3 Transaction, recovery, and reclamation verification obligations

Verification MUST exercise failure boundaries rather than only clean shutdown/reopen behavior.

Deterministic crash injection SHOULD cover boundaries around:

```text
WAL append / incomplete append
fdatasync
data-page pwrite
heap insert before index MTR
B+ MTR construction and publication
TXN_COMMIT durability
checkpoint construction/install
vacuum index cleanup
NORMAL -> DEAD
DEAD -> UNUSED
```

Recovery property tests compare reopened **logical committed contents** against a model containing only transactions whose commit became durable. Physical aborted garbage is allowed.

Table-driven MVCC verification covers creator/deleter committed, active, too-new, aborted, self-command, and frozen cases using exact `xmin/xmax/cmin/cmax` combinations.

Isolation verification establishes READ COMMITTED stable-per-statement snapshots and retry re-evaluation, REPEATABLE READ fixed-snapshot behavior and serialization failure on conflicting post-snapshot writes, and the fact that snapshot-isolation write skew remains possible.

Logical-lock verification establishes same-target serialization, disjoint-target concurrency, unique-key serialization, no physical-latch retention during logical waits, deterministic deadlock victim behavior, and safe waiter cleanup.

Vacuum/reclamation verification includes exact index cleanup before retirement, persistent DEAD restart behavior, grace-delayed RID reuse, version-chain splicing, and long-running snapshot interaction.

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

## 42.2 Transaction, durability, recovery, and vacuum measurements

Group-commit measurement SHOULD include representative committing-thread counts:

```text
1, 2, 4, 8, 16, 32+
```

and record transactions/sec, commit latency percentiles, `fdatasync` calls/sec, commits per sync, and WAL bytes/sec.

Checkpoint/recovery measurement includes checkpoint duration, checkpoint WAL/FPI bytes, DPT size, retained WAL, analysis time, redo time, pages redone, and total recovery time under both mostly-clean and heavily-dirty buffer states.

Vacuum measurement includes dead-version scan rate, exact index-cleanup rate, heap bytes reclaimed, B+ structural side effects, RID grace delay, FSM improvement, freezing rate, and foreground latency impact across update-heavy, delete-heavy, aborted-transaction, duplicate-key, and long-running-snapshot workloads.

These are measurement dimensions; they do not mandate a premature optimization.

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
| `TXN_STATUS` FileKind code | 16-bit little-endian code `5` | §4.7 / §9.11 |
| `TXN_STATUS` PageType code | 16-bit little-endian code `7` | §4.9 / §9.12 |
| Transaction-status page | 8192 bytes, 32-byte header + 8160-byte payload | §9.12 |
| Transaction-status entry | 2 bits, LSB-first packed | §9.11–§9.12 |
| Transaction-status page capacity | 32,640 normal TxnIds | §9.12 |
| WAL segment | 64 MiB | §12.2 |
| WAL record alignment | 8 bytes | §12.3 |
| WAL ordinary record header | 48 bytes | §12.4 |
| PAGE_INIT/PAGE_IMAGE full page | 8192-byte after-image plus payload metadata | §12.8–§12.9 |
| Database control file | 8192 bytes = two 4096-byte slots | §13.2 |

WAL record-type numeric codes/payload byte layouts, database-control slot byte layout, and checkpoint sequence framing remain explicitly tracked format gaps rather than implied by this registry.

Catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

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

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.21; FSM/reclamation invariants are listed in §6.13; I/O/buffer invariants are listed in §7.13; B+ tree invariants are listed in §8.29; transaction/snapshot invariants are listed in §9.16; MVCC invariants are listed in §10.6; logical-locking invariants are listed in §11.15; WAL/commit invariants are listed in §12.18; recovery invariants are listed in §13.21; vacuum/reclamation invariants are listed in §14.18; end-to-end write invariants are listed in §15.9.

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
- clustered-storage/index-locality experiments,
- SERIALIZABLE isolation,
- Serializable Snapshot Isolation (SSI),
- predicate locking,
- logical key-range locking,
- TABLE / INTENTION / SCHEMA logical lock families,
- shared logical lock modes beyond the v1 exclusive-only LockManager,
- lock-free LockManager structures,
- high-concurrency alternative snapshot active-set representations beyond the initial sorted-vector model,
- asynchronous / `synchronous_commit = off` transaction acknowledgement,
- direct-I/O / `io_uring` WAL experiments,
- per-thread or lock-free WAL reservation after profiling justifies it,
- a doublewrite buffer,
- replication/PITR-driven WAL retention,
- ordinary user-DML physical undo and CLRs unless a future subsystem requires them,
- savepoints and subtransactions,
- two-phase commit / XA / distributed transactions,
- lock escalation,
- speculative unique-insertion optimizations,
- prepared transactions,
- row-level logical lock modes beyond the v1 exclusive writer model,
- commit sequence numbers or timestamp-oracle visibility schemes,
- logical/physical replication and PITR archive management,
- online backup and read-only follower snapshots,
- lock-free active transaction registry,
- parallel/background vacuum beyond the correct manual-vacuum baseline.

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

The remaining rewrite issue register contains implementation mismatches, historical refinement notes, later-pass synchronization items, and newly exposed transaction-format/semantic gaps.

The pre-Pass-8 transaction coherence resolution closed the Pass-7 gaps for:

- byte-exact `txn_status.dat` FileKind/PageType/status-bit/mapping semantics,
- exclusive `reserved_txn_id_end` half-open reservation ranges,
- owner-excluded snapshot active sets and exact `xmin` derivation.

The issue register retains these items as resolved architecture decisions and continues to track implementation mismatches, later-pass synchronization work, and unresolved issues discovered by future passes.

Current unresolved Pass-8 format issues are:

1. **WAL record/payload byte format.** The 48-byte record header and logical record semantics are locked, but record-type codes, flags/reserved rules, padding/`total_length` convention, and byte-exact payload encodings are incomplete; see R-025.
2. **Database control slot byte format.** Alternating 4096-byte slots and their semantic fields are locked, but offsets/widths/magic/CRC/flags/initialization are not byte-exact; see R-026.
3. **Checkpoint sequence identity/framing.** BEGIN/DATA/END semantics are locked, but the persistent mechanism proving chunks belong to one checkpoint is not specified; see R-027.
4. **Read-epoch grace arithmetic.** RID-reuse safety is locked, but the exact epoch capture/advance and safe-retirement predicate are not; see R-028.
5. **Transaction-status prefix reclamation.** Status pages can become semantically retireable after freezing, but the current absolute TxnId-to-PageNo mapping does not define physical prefix truncation/remapping; see R-029.

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
Pass 7    legacy §§180–214
Pass 8    legacy §§215–255
Pass 9    legacy §§256–300
```

The existing `ARCHITECTURE.md` remains the active architecture authority until the full rewrite, reconciliation audit, and explicit cutover are complete.
