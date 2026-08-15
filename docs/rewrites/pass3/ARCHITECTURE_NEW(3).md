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

For a non-unique index, physical ordering includes RID:

```text
(index key, RID)
```

so duplicate SQL keys remain individually addressable.

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

The v1 superblock constants are:

```text
magic          = ASCII "DBLUSBLS"
format_version = 1
page_size      = 8192
page_type      = SUPERBLOCK
page_no        = 0
header_size    = 72
```

### 4.10.1 Byte layout

| Offset | Size | Field / required v1 meaning |
|---:|---:|---|
| `0` | 2 | `page_type = SUPERBLOCK` |
| `2` | 2 | `format_version = 1` |
| `4` | 4 | `flags` |
| `8` | 8 | `page_lsn` |
| `16` | 4 | `checksum_crc32c` |
| `20` | 2 | `header_size = 72` |
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
| `72` | 8120 | reserved bytes, all zero |
|  | **8192** | **total** |

All multi-byte integer fields are little-endian.

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
- `header_size != 72`,
- `page_no != 0`,
- magic different from `DBLUSBLS`,
- invalid or unknown `file_kind`,
- `page_size != 8192`,
- any nonzero reserved field,
- any nonzero byte in the trailing reserved region.

Unknown `flags` bits are preserved as raw bits unless a later format revision assigns semantics to them.

A codec MAY accept an input buffer larger than one page, but only the leading 8192 bytes participate in the v1 superblock representation.

Because v1 requires every reserved superblock field and trailing reserved byte to be zero, assigning future meaning to those bytes requires an explicit persistent-format revision.

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

The baseline does not require a general-purpose extent allocator.

Extent allocation remains a later storage optimization rather than a prerequisite for heap, index, or recovery correctness.

## 4.12 Page checksums

Persistent random-access page checksums use CRC32C.

The common page header reserves the checksum field from the beginning of the format so checksum support does not require moving later header fields.

During checksum computation, the checksum field itself is logically treated as zero.

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
9. A v1 superblock is exactly 8192 bytes and has `header_size = 72`.
10. Every v1 superblock reserved field and trailing reserved byte is zero.
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
page_type      = HEAP_DATA
format_version = 1
header_size    = 48
common reserved16 = 0
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
common reserved16 = 0
heap reserved      = 0
```

Structural decoding/validation MUST reject a nonzero value in either field.

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

## 5.18 Heap and tuple invariants

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

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §13. The persisted/runtime FSM contract and page-local reclamation rules are migrated in Pass 4.

Free-space discovery is a distinct storage subsystem.

Heap insertion MUST NOT require a linear scan of every heap page merely to find a page with sufficient insertion space.

The initial design uses explicit approximate free-space metadata, such as a free-space map or bucketed directory, to identify candidate pages.

Free-space metadata MAY be approximate or stale when the concrete contract permits it; actual page suitability remains subject to the page-level insertion rules.

The exact persisted FSM representation, runtime candidate policy, repair semantics, and interaction with compaction/vacuum are defined in later sections of this chapter.

---

# 7. I/O and Buffer Management

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§8, 14, and 15. The concrete I/O, BufferPool, frame, guard, flushing, and ownership contracts are migrated in Pass 5.

## 7.1 Explicit I/O

The initial database storage path uses explicit file I/O such as:

```text
pread
pwrite
fdatasync / fsync
```

Memory-mapped database pages are not the primary storage architecture.

The database must retain explicit control over:

- caching,
- eviction,
- dirty-page flushing,
- WAL-before-data ordering,
- page lifetime,
- future prefetch policy.

Later experiments MAY include:

- `io_uring`,
- asynchronous prefetch,
- direct I/O,
- background writeback,
- deliberate Linux page-cache bypass.

These are not baseline requirements.

## 7.2 Database-managed buffer pool

Normal execution/storage page access MUST pass through an explicit database-managed buffer pool once the buffer layer exists.

A buffer frame conceptually tracks at least:

```text
page identity
page bytes
pin count
dirty state
page LSN
page latch
replacement metadata
```

The architecture requires operations equivalent in responsibility to:

```text
fetch an existing page
create/access a newly allocated page
release a pin
flush one page
flush a file/object
remove a page when allowed
```

The concrete API is implementation-specific and is defined in the detailed BufferPool contract.

Page lifetime SHOULD be expressed through RAII-style guards so pin release is coupled to object lifetime and accidental pin leaks are difficult.

## 7.3 Replacement policy

The initial replacement family is CLOCK.

CLOCK is selected as the baseline because it provides realistic replacement behavior with substantially less bookkeeping than exact LRU.

The replacement component SHOULD have a separable policy boundary so alternative policies can be measured later.

Deferred replacement experiments include:

- CLOCK-Pro,
- LRU-K,
- 2Q,
- scan-resistant policies.

---

# Part III — Indexing

# 8. B+ Tree Indexing

> **Rewrite status:** Pass 1 establishes the architectural baseline from legacy §§16–17. The complete B+ tree contract is migrated in Pass 6.

The primary general-purpose index is a page-backed B+ tree implemented by the database.

The architecture requires eventual support for:

- point lookup,
- insertion,
- deletion,
- node split,
- merge/rebalance,
- ordered range scan,
- sibling-linked leaves,
- unique and non-unique SQL indexes,
- concurrent traversal.

Index leaf entries reference physical heap tuple-version RIDs.

For non-unique keys, the physical ordering MUST include RID as a deterministic tie-breaker so duplicate SQL keys remain individually addressable.

B+ tree structural synchronization uses short-lived page/tree latches and MUST remain distinct from transaction-level logical locks.

The baseline concurrent-write design uses latch coupling/crabbing. More optimistic algorithms are deferred.

The exact persistent node formats, key encoding, separator semantics, split/rebalance rules, sibling-link protocol, latching order, free-page reuse, verifier, and WAL interaction are defined by the B+ tree contract migrated in Pass 6.

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

Detailed per-subsystem test recipes are verification documentation rather than canonical architecture and are not duplicated here.

---

# 42. Performance Requirements

> **Rewrite status:** Pass 1 preserves the architecture-level requirements from legacy §46. Detailed benchmark procedures remain preserved in the legacy source until a dedicated verification document is adopted.

Performance claims require measurement.

The benchmark program must eventually cover at least:

- sequential scan throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- buffer-pool hit/miss behavior,
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

Performance-sensitive architecture changes SHOULD be supported by benchmark evidence rather than a single noisy measurement or intuition alone.

---

# Appendix A. Persistent Format Registry

This appendix indexes canonical persistent-format definitions. It does not replace the owning chapters.

| Persistent item | v1 width / size | Canonical definition |
|---|---:|---|
| `file_kind` code | 16 bits, little-endian | §4.7 |
| Common page header | 32 bytes | §4.8 |
| `page_type` code | 16 bits, little-endian | §4.9 |
| File superblock | 8192 bytes | §4.10 |
| File superblock logical header | 72 bytes | §4.10 |
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

FSM, B+ tree, transaction-status, WAL, catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

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

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.18.

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
- schema-version translation for schema-changing DDL until explicitly implemented.

Items listed here are future possibilities or staged functionality, not requirements to implement immediately.

---

# Appendix D. Open Architecture Decisions

Pass 1 introduces no new open architectural decision.

Rewrite/implementation consistency issues discovered while reorganizing the legacy contract are tracked separately in `ARCHITECTURE_REWRITE_ISSUES.md` so they are not silently converted into architecture decisions.

---

## Rewrite progress

Architecture content migrated so far:

```text
Pass 0    inventory and target structure
Pass 1    legacy §§0–52
Pass 2    legacy §§53–63
Pass 3    legacy §§64–81
```

The existing `ARCHITECTURE.md` remains the active architecture authority until the full rewrite, reconciliation audit, and explicit cutover are complete.
