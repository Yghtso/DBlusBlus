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

MUST be serialized against concurrent extensions of the same managed file so two allocators cannot receive the same `PageNo`.

The existing raw append primitive is a lower-layer storage mechanism. Once WAL/recovery is active, a newly extended ordinary page is not database-visible merely because the file grew.

### 4.11.1 WAL-mode page publication

Each managed append-only page file has a process-local **append-publication lock** and a process-local:

```text
published_page_count
```

which is the exclusive upper PageNo bound visible to relation/index scans and ordinary allocation users.

For a new non-B+ ordinary page, publication is:

```text
1. acquire the file's append-publication lock
2. reserve page_no = physical file page count
3. extend the file by exactly one 8192-byte page
4. create the initialized logical page bytes except final page_lsn/checksum
5. reserve the PAGE_INIT LSN
6. finalize the canonical image with page_lsn = PAGE_INIT.lsn and valid checksum
7. append the complete PAGE_INIT record
8. install the canonical initialized resident image
9. advance published_page_count = page_no + 1
10. release the append-publication lock
```

The next append to that same file MUST NOT reserve a later PageNo before step 7 for the earlier page has completed.

The PAGE_INIT record does not need to be durable merely to release the append-publication lock. WAL append order is enough: if a later page's initialization WAL becomes durable, every earlier WAL byte in that logical prefix is durable as well.

A new page MUST NOT become reachable from persistent relation/index metadata before its initializing WAL record has been completely appended.

For B+ pages allocated by file extension, the equivalent publication record is the complete `BTREE_MTR` that contains the new page as a full-image affected page. A newly appended B+ page is not tree-reachable before that MTR exists.

A page reused from an already-published object-specific free list does not extend the file and follows the owning reuse/MTR rules instead.

### 4.11.2 Runtime visibility and heap/FSM growth

Ordinary heap scans use the relation/file published page bound, not a transient larger raw file size that may contain an unpublished append tail.

When a new `HEAP_DATA` page is published:

- the heap's authoritative published page count grows immediately,
- the page may be used directly by the allocating insertion path,
- the corresponding FSM category is then created/repaired as advisory metadata.

If the required FSM_DATA page does not yet exist, it is itself allocated/published through the same PAGE_INIT protocol.

A crash or failure that leaves the FSM behind the heap does not invalidate the heap page; FSM is rebuildable from authoritative heap headers.

### 4.11.3 Recovery reconciliation of unpublished append tails

A crash may physically extend a file before the initializing WAL record becomes a valid durable WAL record.

Recovery distinguishes:

```text
published page:
    valid on-disk page
    OR retained durable PAGE_INIT / BTREE_MTR publication redo

unpublished append-tail page:
    trailing physical page
    with no valid on-disk page image
    and no valid durable initialization/publication WAL
```

Before normal database open completes, recovery:

1. uses durable PAGE_INIT/BTREE_MTR publication records to determine any published trailing PageNos,
2. if such a durable publication names a PageNo beyond the currently observed file length, re-extend the file to include that PageNo,
3. replay the durable initialization record(s) needed to reconstruct those published pages,
4. then remove only the **contiguous trailing suffix** of physical pages that have no durable publication,
5. restore `published_page_count` from the reconciled physical file length.

The append-publication ordering guarantees that recovery never needs to remove an unpublished interior hole while retaining a later published appended page.

If an invalid/torn page is known published and required recovery WAL exists, recovery reconstructs it rather than truncating it.

If a nontrailing invalid page is neither reconstructible nor legitimately unused under an owning page-reuse protocol, recovery reports corruption.

Whole-file shrinking remains unnecessary during normal execution; the narrow recovery truncation above exists only to discard unpublished append tails.

### 4.11.4 Later page reuse

Object-specific subsystems MAY recycle pages that are completely unused.

Examples include:

- a B+ tree free-page list,
- completely empty heap pages,
- unused FSM metadata pages.

Recycling or reinitializing a previously used heap `PageNo` MUST NOT bypass physical RID-reuse safety. If page recycling can make a previously used `(PageNo, SlotId)` identity reusable, it is subject to the global reclamation/read-epoch gate in Chapter 14.

The baseline does not require a general-purpose extent allocator.

Extent allocation remains a later storage optimization rather than a prerequisite for heap, index, or recovery correctness.

## 4.12 Page checksums

Persistent random-access page checksums use CRC32C.

For every persisted 8192-byte random-access page:

```text
1. logically treat bytes 16..19 as zero;
2. compute CRC32C over exactly bytes 0..8191;
3. store the resulting uint32_t little-endian in bytes 16..19.
```

The superblock uses this rule from initial file creation.

In the WAL/recovery-enabled v1 architecture, **every persisted ordinary random-access page MUST carry a valid checksum and MUST be checksum-verified before its stored `page_lsn` is trusted during recovery or normal page open.**

### 4.12.1 Resident-page checksum state

The checksum field in a dirty resident frame is not itself authoritative while the page is being modified.

A persistent mutation may leave the resident bytes `16..19` stale until a flush image or full-page WAL image is finalized.

Code MUST NOT use a dirty resident page's stored checksum bytes as proof that the mutable in-memory image is internally current.

### 4.12.2 Canonical flush-image finalization

BufferPool writes a private stable 8192-byte flush image rather than computing the disk checksum over concurrently mutable frame bytes.

Conceptually:

```text
1. verify the frame is flushable and no MTR no-flush barrier is active
2. under the page's read latch:
       capture modification_generation G
       copy all 8192 resident bytes to a private flush buffer
       capture image_page_lsn
3. release the page latch
4. set flush_buffer[16..19] = 0
5. CRC32C exactly flush_buffer[0..8191]
6. store the CRC little-endian at flush_buffer[16..19]
7. establish durable_lsn >= image_page_lsn
8. pwrite exactly that 8192-byte flush image
9. after successful write, under frame synchronization:
       if modification_generation is still G:
           clear dirty and rec_lsn
       else:
           leave the frame dirty
```

The checksum written to disk therefore describes exactly the same stable image whose `page_lsn` participates in WAL-before-data ordering.

The resident frame does not need to overwrite its own checksum bytes with the flush-buffer checksum.

### 4.12.3 Canonical full-page WAL images

A `PAGE_INIT`, `PAGE_IMAGE`, or full-image page entry inside `BTREE_MTR` contains a canonical complete page image:

```text
page_lsn = the owning WAL record LSN
checksum = valid CRC32C for that exact 8192-byte image
```

The WAL writer may reserve/know the record LSN before finalizing these bytes.

Recovery may validate a full image's embedded page checksum in addition to validating the WAL record CRC.

`PAGE_DELTA` does not log checksum-byte patches. Recovery installs the logical after-image bytes and page LSN; the normal page-write path finalizes the checksum before the page is persisted again.

### 4.12.4 Purpose and limitation

Page checksums provide:

- corruption detection,
- misdirected/torn-write diagnostics,
- crash-test observability.

A checksum detects corruption; it does not by itself repair a torn page.

Torn-page repair requires a retained complete WAL page image as defined by Chapters 12–13.

## 4.13 Storage-foundation invariants

1. `FileId` is a database identity, never an operating-system file descriptor.
2. `PageId` is a persistent logical identity, never a frame index or memory address.
3. RID denotes a physical heap tuple version, not a permanent logical SQL row.
4. Persistent numeric file-kind and page-type codes are explicit and never implicitly renumbered.
5. Random-access database file page `0` is the superblock; ordinary object pages begin at page `1`.
6. Persistent multi-byte fields defined by this chapter use explicit little-endian serialization.
7. The common page header is 32 bytes.
8. Page-type-specific parsing validates the expected persisted page type before type-specific interpretation.
9. A v1 superblock is exactly 8192 bytes; `header_size=72` for HEAP/FSM/CATALOG/TXN_STATUS and `128` for BTREE.
10. Every v1 superblock reserved field and every byte after its file-kind-specific header is zero.
11. The superblock CRC32C covers exactly bytes `0..8191` with bytes `16..19` logically zero.
12. In WAL/recovery-enabled v1 every persisted ordinary random-access page also carries a valid whole-page CRC32C.
13. A dirty resident checksum may be stale; only a canonical stable flush/full-image copy supplies the durable checksum.
14. A write to an unallocated page does not allocate or sparsely extend the file.
15. Concurrent raw append allocation of one managed file cannot return the same new `PageNo` twice.
16. WAL-mode append publication serializes initialization-record append before `published_page_count`/reachability advances.
17. Recovery may remove only the contiguous unpublished append suffix after reconstructing every durable page publication.
18. `page_lsn` is the newest WAL-protected modification reflected in the page once WAL is active.
19. Page checksums detect torn/corrupt writes; retained full-image WAL supplies repair.

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

Once Chapter 14 has made a slot safely reusable, v1 uses an intrusive singly linked free-slot list.

For an `UNUSED` slot:

```text
tuple_offset = 0
tuple_length = 0
state        = UNUSED
aux          = next UNUSED SlotId
               or INVALID_SLOT_ID at list end
```

`free_slot_head` identifies the first reusable `UNUSED` slot.

Every SlotId reachable from `free_slot_head` MUST:

- be `< slot_count`,
- be in state `UNUSED`,
- have zero tuple coordinates,
- appear at most once in the free list.

Every safely reusable v1 `UNUSED` slot MUST appear exactly once in this list.

`DEAD` slots are never linked into the free-slot list.

The existence of `free_slot_head` therefore does not authorize immediate `DEAD` reuse; Chapter 14 controls the only transition that can add a retired slot to this list.

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

For v1 `UNUSED` slots, `aux` is the free-slot next pointer defined by §5.3.2 and tuple coordinates are exactly zero.

For `REDIRECT_RESERVED`, heap-page v1 still assigns no tuple-range or `aux` semantics.

`REDIRECT_RESERVED` remains reserved for possible HOT-like behavior and MUST NOT be emitted by the baseline heap/vacuum implementation.

### 5.4.3 DEAD slots and physical reclamation

A `NORMAL -> DEAD` transition does not itself reclaim tuple bytes or make the SlotId reusable.

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
aux          = preserved until final reuse transition
```

Clearing the coordinates prevents the reclaimed `DEAD` slot from pointing into newly free space or bytes moved for another tuple.

Compaction alone does **not** change `DEAD` to `UNUSED` and does not link the slot into the free list.

Only the Chapter-14 grace-complete reclamation protocol may atomically convert the canonical zero-coordinate `DEAD` slot to `UNUSED` and install the free-list link.

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

Insertion requires sufficient space for the tuple bytes plus:

```text
0 bytes of new slot directory
    when a safely reusable UNUSED slot is popped

8 bytes of new slot directory
    when free_slot_head == INVALID_SLOT_ID
```

To reuse a slot:

```text
slot_id = free_slot_head
validate slot is canonical UNUSED
free_slot_head = slot.aux
install new tuple coordinates/state NORMAL
slot.aux = 0
```

Popping the free-slot list and installing the new NORMAL slot occur in the same page mutation/WAL unit.

Page compaction MAY be used when total reclaimable space is sufficient but the current contiguous free-space interval is not.

Compaction MUST preserve stable SlotId values and the canonical DEAD-slot rules in §5.4.3.

Compaction may discard payload bytes for a `DEAD` slot before its final transition to `UNUSED`; it does not independently decide that the RID reuse grace period has completed.

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

Tuple interpretation uses the catalog contract:

```text
ResolveSchema(TableId, tuple.schema_version)
    -> immutable SchemaDescriptor
```

defined in Chapter 16.

The first implementation MAY support only schema version `1`, but any persisted schema version that may still occur in tuple storage MUST remain historically resolvable.

The field exists from the initial format so schema evolution does not require silently changing the tuple representation.

SQL DDL MAY reject schema changes that require unsupported version translation/tuple rewriting until such behavior is explicitly implemented.

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
9. Every `UNUSED` slot has zero tuple coordinates and uses `aux` as the next free SlotId/sentinel.
10. `free_slot_head` is either `INVALID_SLOT_ID` or the head of an acyclic in-range chain containing every reusable UNUSED slot exactly once.
11. Page compaction does not change `SlotId`.
12. Physically reclaimed `DEAD` slots canonicalize tuple coordinates to `(0,0)` but remain `DEAD` until the grace-complete reuse transition.
13. A `DEAD` slot is never linked into the reusable free-slot list.
14. The maximum accepted raw tuple payload is `8135` bytes; `8136` is rejected.
15. A tuple header is exactly 48 bytes; `header_bytes=48` and tuple reserved bytes `44..47` are zero.
16. `CommandId{0}` is valid.
17. The previous-version pointer is either two invalid sentinels or two non-sentinels, and always refers within the same heap file.
18. The v1 tuple-flags known mask is `0x0003`; unknown bits are invalid.
19. `HAS_VARLEN` exactly reflects whether the interpreting physical schema contains VARCHAR.
20. Tuple physical length is exact; trailing unreferenced bytes are invalid.
21. Every physical schema column owns one LSB-first null bit.
22. Unused high null-bitmap bits are zero.
23. `HAS_NULLS` exactly reflects whether any used null bit is set.
24. Schema-directed validation rejects a NULL bit for a `NOT NULL` column.
25. BOOLEAN accepts only `0x00` and `0x01`.
26. FLOAT64 preserves exact IEEE-754 binary64 payload bits and does not canonicalize NaNs.
27. A NULL VARCHAR descriptor is exactly `(0,0)`.
28. Present VARCHAR payloads are packed consecutively in physical schema order with no gaps or overlaps.
29. Present empty VARCHAR is distinct from NULL.
30. `HeapPage` owns physical page mechanics, not SQL visibility.
31. Physical heap scan order does not imply SQL result ordering.
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

A resident buffer frame contains at least:

```text
aligned 8192-byte page bytes
PageId
pin count
dirty flag
rec_lsn
modification_generation
reference/replacement metadata
page latch
I/O state
checkpoint/FPI epoch metadata
temporary no-flush state when required
```

`modification_generation` is a process-local monotonically increasing per-frame counter incremented for every persistent-byte mutation installed in that frame.

It is not persisted.

Together with page latching it lets BufferPool determine whether a page changed after a stable flush image was copied but before the I/O completed.

Frame metadata remains process-local. `rec_lsn`, full-image epoch state, and temporary no-flush barriers follow Chapters 12–13.

The page-byte region SHOULD be suitably aligned for efficient copying/checksum work.

Process-local metadata SHOULD avoid pathological false sharing where measurement shows contention, but cache-line padding is not a persisted or correctness contract.

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

## 7.10 Dirty pages and stable writeback

A persistent mutable/write operation marks the frame dirty and increments:

```text
modification_generation
```

For WAL-protected pages, §12.16 additionally defines the clean-to-dirty `rec_lsn`/full-image transition.

Dirty pages may be written because of:

- eviction,
- explicit flush,
- checkpoint/background writeback.

Commit does not require heap/index data-page writeback because v1 is NO-FORCE.

### 7.10.1 Write image

BufferPool never computes the durable checksum from bytes that may be changing concurrently.

The write path copies a stable 8192-byte image while holding the page's read latch, records that image's `page_lsn` and `modification_generation`, releases the latch, finalizes the checksum in the private copy, establishes WAL-before-data, and writes that copy.

After successful I/O, dirty state can be cleared only if the frame's current `modification_generation` still equals the copied generation.

If a newer mutation raced the I/O, the older stable image may still be a valid durable database page, but the resident frame remains dirty and keeps the appropriate dirty-interval recovery state.

Exact checksum finalization is §4.12.2.

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
15. Every persistent resident-page mutation increments `modification_generation` and marks the frame dirty.
16. A stable flush image is copied under the page latch and checksum-finalized outside mutable frame bytes.
17. Successful I/O clears dirty/rec_lsn only when the copied `modification_generation` is still current.
18. NO-FORCE means commit does not require writing dirty heap/index data pages.
19. Before writing a WAL-protected page image with `page_lsn=X`, BufferPool ensures `durable_lsn >= X`.
20. MTR no-flush state overrides ordinary flush eligibility.
21. CLOCK considers only unpinned frames for eviction and gives referenced frames a second chance.
22. Dirty CLOCK victims are safely flushed before frame reuse.

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

If an internal retry is permitted by the exact §15.7 boundary, the abandoned attempt unregisters its old statement snapshot and the retry captures a fresh READ COMMITTED statement snapshot.

If the conflict occurs after the attempt has installed a persistent statement write, §15.7 requires transaction abort rather than same-transaction statement restart.

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

else if runtime terminal-outcome cache contains txn_id:
    COMMITTED or ABORTED

else if active transaction registry contains txn_id as nonterminal:
    IN_PROGRESS

else if txn_id < txn_status_reclaim_before:
    RETIRED

else:
    consult cached/persistent transaction status
```

The runtime terminal-outcome cache dominates a stale/nonremoved active-registry entry.

`RETIRED` is a runtime lookup result, not a persisted two-bit state. It means the architecture has durably proven that the old status range is no longer needed by any valid persistent correctness object.

Persisted:

```text
INVALID
RESERVED
```

are nonterminal results and MUST NOT be promoted to committed/aborted by guesswork.

`SELF`, `IN_PROGRESS`, and `RETIRED` are lookup results, not additional persisted two-bit codes.

A persistent tuple or other correctness object that references a normal TxnId which is:

```text
not active
not terminal
and not legally replaceable by FROZEN_TXN_ID under the retained-status cutoff
```

after completed recovery indicates corruption or an invariant failure.

The engine MUST NOT silently treat unknown or retired history as committed.

## 9.14 Terminal status publication boundary

TransactionManager owns one short synchronization domain used by:

```text
snapshot capture
active-registry membership
runtime terminal-outcome publication
```

This creates one linearization point between nonterminal and terminal transaction state.

### 9.14.1 Runtime terminal publication

After the required terminal WAL/status-page prerequisites have been satisfied, terminal publication performs atomically under the transaction-registry synchronization:

```text
1. publish COMMITTED or ABORTED in the runtime terminal-outcome cache
2. change the transaction runtime state to the terminal state
3. remove/mark it non-active for future snapshot capture
```

After this linearization point:

- status lookup returns the terminal outcome even if cleanup of a stale registry object is still pending,
- a newly captured snapshot does not place the transaction in `active`,
- the terminal outcome cannot revert.

Transaction-lifetime logical locks MUST NOT become acquirable by another transaction until this terminal publication point has completed.

Thus another writer observes either:

```text
nonterminal transaction + its logical locks still protect the conflict
```

or:

```text
terminal transaction outcome
```

never `IN_PROGRESS` after the conflicting transaction's locks have been released.

### 9.14.2 Commit

For a transaction with persistent writes:

```text
append TXN_COMMIT
    ↓
make WAL durable through commit LSN
    ↓
install/update COMMITTED in transaction-status page/cache
    ↓
runtime terminal publication linearization
    ↓
release transaction-lifetime logical locks
    ↓
unregister the transaction's own SQL snapshot(s)
    ↓
return commit success
```

The persistent status-page update itself remains NO-FORCE; durable commit WAL is authoritative after crash.

### 9.14.3 Abort

Abort publication is:

```text
state -> ABORTING
    ↓
append TXN_ABORT when persistent WAL-visible state exists
    ↓
install/update ABORTED transaction status
    ↓
runtime terminal publication linearization
    ↓
release transaction-lifetime logical locks
    ↓
unregister SQL snapshot(s)
    ↓
finish ABORTED
```

An ordinary abort does not require immediate abort-WAL `fdatasync` merely to acknowledge abort.

If abort WAL is lost in a crash, recovery treats the transaction as a loser and establishes ABORTED again.

Ordinary user abort does not physically restore heap/index bytes.

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
7. `CommandId{0}` is valid and command IDs advance once per successfully completed SQL statement.
8. Snapshot capture atomically observes the TxnId high-water mark and relevant nonterminal active registry.
9. `snapshot.active` excludes `owner_txn_id`; `snapshot.xmin` is the smallest other active TxnId, or `xmax` when none exists.
10. One READ COMMITTED statement attempt uses one stable snapshot.
11. REPEATABLE READ reuses one transaction snapshot and changes only its command boundary for later statements.
12. Runtime terminal outcome dominates nonterminal/active lookup.
13. Terminal publication and snapshot-active removal have one synchronization linearization point.
14. Logical transaction locks are not released before terminal publication has linearized.
15. A transaction is not published COMMITTED before its commit WAL is durable.
16. Read-only transactions may end without terminal persistent status because they create no persistent TxnId references.
17. A referenced normal TxnId with no legal active/terminal/frozen/retired interpretation is an invariant failure, not implicitly committed.

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

Apply the §15.7 retry boundary.

Before the statement attempt has installed any persistent write, the engine may:

```text
restart the affected candidate search
using a fresh READ COMMITTED statement snapshot
```

and re-evaluate the current row version, predicates, index conditions, and generated values.

After the attempt has installed any persistent write, v1 MUST abort the transaction instead of restarting that statement inside the same TxnId.

This distinction is required because v1 deliberately has no statement-level physical undo/subtransaction state.

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

## 12.3 Record alignment, physical span, and segment boundary

Every ordinary WAL record starts at an 8-byte-aligned logical LSN.

For every non-tail-padding record:

```text
total_length   = 48 + payload_length
physical_span  = align_up(total_length, 8)
```

`total_length` excludes external alignment bytes.

Bytes in:

```text
[record_lsn + total_length,
 record_lsn + physical_span)
```

MUST be zero.

They are not included in the record CRC.

No ordinary record may cross a 64 MiB segment boundary.

If the next record does not fit in the remaining segment bytes:

- when at least 48 bytes remain, emit one `WAL_PAD` record whose `total_length` consumes exactly the remaining bytes and whose payload is all zero;
- when fewer than 48 bytes remain, write an all-zero raw tail and begin the next record at the next segment boundary.

Recovery recognizes an all-zero short segment tail as padding, not as a record header.

A logical record whose aligned `physical_span` is greater than 64 MiB is rejected as `WAL_RECORD_TOO_LARGE` before LSN reservation. One logical `BTREE_MTR` therefore has the same hard limit.

## 12.4 WAL record header v1

Every ordinary WAL record begins with exactly 48 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `total_length` |
| `4` | 2 | `header_length = 48` |
| `6` | 2 | `record_type` |
| `8` | 4 | `flags = 0` |
| `12` | 4 | `reserved = 0` |
| `16` | 8 | `lsn` |
| `24` | 8 | `txn_id` |
| `32` | 8 | `prev_txn_lsn` |
| `40` | 4 | `payload_length` |
| `44` | 4 | `crc32c` |
|  | **48** | **total header** |

All integer fields are explicit little-endian.

V1 assigns no WAL-header flag bits. Encoders write zero and decoders reject nonzero `flags` or `reserved`.

A decoder requires:

```text
header_length = 48
total_length  = 48 + payload_length
total_length >= 48
align_up(total_length,8) remains inside the current segment
header.lsn = physical logical record start LSN
```

CRC32C covers:

```text
48-byte header with bytes 44..47 logically zero
+
exact payload_length payload bytes
```

External alignment bytes are excluded and must be zero.

## 12.5 WAL PageId codec

Whenever a WAL payload persists a `PageId`, it uses exactly 16 bytes:

| Relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `file_id` |
| `4` | 4 | `reserved32 = 0` |
| `8` | 8 | `page_no` |
|  | **16** | **total** |

The decoder rejects `INVALID_FILE_ID`, `INVALID_PAGE_NO`, or nonzero reserved bytes when the owning record requires a real page.

This WAL PageId encoding is independent of C++ object layout.

## 12.6 Per-transaction WAL chain

Each user transaction tracks `last_wal_lsn`.

A user transaction record stores:

```text
txn_id       = owning normal TxnId
prev_txn_lsn = previous WAL record for that user transaction
```

System records store:

```text
txn_id       = INVALID_TXN_ID = 0
prev_txn_lsn = INVALID_LSN    = 0
```

B+ MTRs remain system records even when they carry an optional diagnostic owner TxnId in their payload.

## 12.7 Persisted WAL record-type registry

The v1 16-bit little-endian `record_type` codes are:

| Code | Record type |
|---:|---|
| `0` | `WAL_PAD` |
| `1` | `PAGE_INIT` |
| `2` | `PAGE_DELTA` |
| `3` | `PAGE_IMAGE` |
| `4` | `BTREE_MTR` |
| `5` | `TXN_COMMIT` |
| `6` | `TXN_ABORT` |
| `7` | `CHECKPOINT_BEGIN` |
| `8` | `CHECKPOINT_DATA` |
| `9` | `CHECKPOINT_END` |

Existing codes MUST NOT be renumbered.

### 12.7.1 WAL_PAD

A `WAL_PAD` record is a system record.

Its payload consists only of zero bytes and its `total_length` fills the remainder of the current segment.

The record has a normal valid CRC.

`WAL_PAD` is never passed to page/transaction redo logic.

### 12.7.2 TXN_COMMIT and TXN_ABORT

Both terminal transaction records have:

```text
payload_length = 0
total_length   = 48
```

Their header stores:

```text
txn_id       = the normal transaction being terminated
prev_txn_lsn = that transaction's previous WAL record, or 0
```

`TXN_COMMIT` and ordinary user `TXN_ABORT` therefore become the tail of the per-transaction WAL chain.

A recovery-generated TXN_ABORT uses the loser TxnId and the last transaction WAL LSN reconstructed by analysis as `prev_txn_lsn`.

### 12.7.3 Page-record transaction ownership

`PAGE_INIT`, `PAGE_DELTA`, and `PAGE_IMAGE` use:

```text
txn_id = owning user TxnId
prev_txn_lsn = prior user WAL LSN
```

when the page change contains user-transaction-owned persistent state whose crash-loser outcome must be tracked.

Pure system/maintenance page changes use:

```text
txn_id = 0
prev_txn_lsn = 0
```

The structural existence of a newly initialized page may survive a user abort even when the PAGE_INIT record participates in that user's WAL chain; no physical user-DML undo follows from the header ownership.

`BTREE_MTR`, checkpoint records, and WAL_PAD are system records with header `txn_id=0` / `prev_txn_lsn=0`. A BTREE_MTR's optional owner TxnId remains diagnostic payload only.

## 12.8 PAGE_DELTA

`PAGE_DELTA` payload prefix is exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected `PageType` |
| `18` | 2 | `reserved16 = 0` |
| `20` | 4 | `patch_count` |

Each patch follows as:

| Size | Field |
|---:|---|
| 2 | page-relative `offset` |
| 2 | `length` |
| 4 | `reserved32 = 0` |
| `length` | after-image bytes |

Patches are serialized in strictly ascending offset order and MUST be nonempty and nonoverlapping.

For every patch:

```text
offset + length <= 8192
```

using checked arithmetic.

Delta patches MUST NOT overlap common-header bytes `8..19` (`page_lsn` and checksum). Recovery manages those fields centrally.

The mutation code must ensure the patch set describes every other persistent byte changed by that logical page mutation.

The payload is valid only when parsing exactly `patch_count` entries consumes exactly `payload_length` bytes; trailing payload bytes are forbidden.

Redo applies the delta only when the trusted current page has:

```text
page.page_lsn < record.lsn
```

then sets:

```text
page.page_lsn = record.lsn
```

The resident checksum becomes stale until normal checksum finalization.

## 12.9 PAGE_INIT and PAGE_IMAGE

`PAGE_INIT` and `PAGE_IMAGE` have the same exact payload layout:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected `PageType` |
| `18` | 2 | `reserved16 = 0` |
| `20` | 4 | `image_length = 8192` |
| `24` | 8192 | complete canonical page after-image |
|  | **8216** | **payload** |

The embedded full page image must itself satisfy:

```text
page_no   = WAL PageId.page_no
page_type = expected PageType
page_lsn  = owning WAL record LSN
checksum  = valid whole-page CRC32C
```

`PAGE_INIT` publishes/reconstructs one newly initialized non-B+ page.

`PAGE_IMAGE` replaces the complete state of an already-published page.

## 12.10 Full-page-image invariant and torn-page protection

V1 requires a complete page image whenever either condition is true:

```text
A. a WAL-protected page transitions clean -> dirty

or

B. this is the first WAL-protected modification of that page
   after the most recently completed checkpoint FPI epoch
```

For an ordinary non-B+ page, that modification uses `PAGE_IMAGE` (or `PAGE_INIT` for initial publication).

For a B+ MTR, the MTR uses a full-image affected-page entry for each page satisfying either condition.

The clean-to-dirty rule is the critical WAL-retention invariant:

> Every dirty interval begins at a WAL record that contains a complete recoverable image of that page.

Therefore the frame's `rec_lsn` always identifies a retained recovery image while the frame is dirty.

Later changes during the same dirty interval may use deltas unless condition B independently requires another checkpoint-epoch image.

### 12.10.1 BufferPool recLSN consequence

On clean -> dirty at full-image LSN `X`:

```text
dirty   = true
rec_lsn = X
```

Already-dirty later modifications preserve the existing `rec_lsn` even when a later checkpoint-epoch full image is emitted.

WAL retention through the minimum DPT `rec_lsn` therefore retains at least one complete recovery image for every dirty page.

### 12.10.2 B+ tree mini-transactions

Every physical B+ mutation executes as a system `BTREE_MTR`, including ordinary leaf insert/erase, split, redistribution, merge, root replacement, and tree-local free-list modification.

The exact payload prefix is 16 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | diagnostic `owner_txn_id` or `INVALID_TXN_ID` |
| `8` | 4 | `page_count` |
| `12` | 4 | `reserved32 = 0` |

Affected-page entries are serialized in ascending `(file_id,page_no)` order and every PageId appears at most once.

Each affected-page entry begins with exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected B+ `PageType` |
| `18` | 1 | encoding |
| `19` | 1 | `reserved8 = 0` |
| `20` | 4 | `data_length` |

Encoding codes are:

```text
1 = FULL_IMAGE
2 = PATCH_SET
```

For `FULL_IMAGE`:

```text
data_length = 8192
```

followed by one canonical page image whose `page_lsn = mtr.lsn` and checksum is valid.

For `PATCH_SET`, the data begins with:

```text
uint32 patch_count
uint32 reserved32 = 0
```

followed by the same patch entry grammar/rules as PAGE_DELTA. `data_length` must equal exactly `8 + sum(8 + patch.length)` for those entries.

After exactly `page_count` affected-page entries are parsed, no trailing MTR payload bytes are permitted.

Persistent allocation/free/root metadata changes are represented by including the affected metadata/free pages in the MTR; there is no second hidden persistent side channel.

A newly appended B+ page MUST use `FULL_IMAGE` in the publishing MTR.

Every MTR-modified page receives:

```text
page_lsn = mtr.lsn
```

### 12.10.3 MTR no-flush barrier

While an MTR is being constructed, every affected resident frame is non-flushable.

The publication sequence is:

```text
1. acquire/latch required pages
2. install one valid final runtime tree state
3. reserve the MTR LSN and construct the complete canonical payload
4. append the complete BTREE_MTR record
5. install each affected resident page_lsn = mtr.lsn
6. remove no-flush barriers
7. release page latches/pins
```

If the process fails before step 4 completes, protected frames were not eligible for data-file write.

MTR append itself does not require `fdatasync`; later writeback or transaction commit may make it durable.

### 12.10.4 User abort does not reverse B+ shape

User abort leaves valid B+ structural shape in place.

Heap/index versions belonging to the aborted transaction remain logically invisible/vacuumable; split/merge/root/free-list system actions are not physically undone.

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

Every WAL-protected dirty frame tracks:

```text
rec_lsn
```

meaning:

> the LSN of the full page image that began the frame's current clean-to-dirty interval.

Because §12.10 requires a full image on every clean-to-dirty transition, `rec_lsn` is both:

- the first WAL LSN of the current dirty interval,
- an LSN from which the complete page can be reconstructed without trusting the current data-file image.

Transitions are:

```text
clean -> dirty at full-image LSN X:
    dirty   = true
    rec_lsn = X

already dirty -> later modification:
    preserve rec_lsn
```

After a successful stable-image page write, dirty state can be cleared only if §4.12.2/§7.10 prove no newer frame modification raced the image written.

Only then:

```text
dirty   = false
rec_lsn = INVALID_LSN
```

`rec_lsn` is process-local frame/checkpoint metadata.

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
2. Every ordinary record starts at an 8-byte-aligned LSN and has `total_length = 48 + payload_length`.
3. External record-alignment bytes are zero and excluded from CRC/`total_length`.
4. An ordinary logical record never crosses a segment boundary.
5. The v1 WAL header is exactly 48 bytes with zero flags/reserved fields.
6. Persisted record-type codes are explicit and stable.
7. WAL PageId encoding is exactly 16 bytes and independent of ABI layout.
8. PAGE_DELTA patch entries are canonical, ordered, nonoverlapping, bounds checked, and never patch page_lsn/checksum bytes.
9. PAGE_INIT/PAGE_IMAGE carry canonical complete 8192-byte after-images.
10. Every clean-to-dirty page transition begins with a full page image and sets `rec_lsn` to that image LSN.
11. The first post-checkpoint-epoch modification also carries a full image when required by the FPI epoch rule.
12. Every physical B+ mutation is an atomic redoable MTR system action.
13. Every MTR-modified page receives one common MTR LSN.
14. A frame protected by an MTR no-flush barrier cannot be written before the complete MTR record exists.
15. Heap tuple-version redo precedes index MTR redo that references the new RID.
16. `durable_lsn` advances monotonically only after the relevant WAL bytes reach durable storage.
17. Group commit is the normal synchronous commit durability path.
18. Successful v1 COMMIT with persistent writes waits for durable TXN_COMMIT WAL.
19. WAL-protected data-page writeback requires durable WAL through the stable write image's page_lsn.
20. Every persisted ordinary random-access page has a valid whole-page checksum.

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

Global recovery/bootstrap metadata lives in exactly:

```text
database.control
```

with total size:

```text
8192 bytes
```

composed of two alternating 4096-byte slots.

### 13.2.1 Control-slot v1 byte layout

Each slot is independently encoded as:

| Slot offset | Size | Field / v1 meaning |
|---:|---:|---|
| `0` | 8 | ASCII `DBLUSCTL` |
| `8` | 2 | `format_version = 1` |
| `10` | 2 | `header_size = 88` |
| `12` | 4 | `flags = 0` |
| `16` | 8 | `generation` |
| `24` | 8 | `latest_checkpoint_lsn` = installed CHECKPOINT_BEGIN LSN, or `0` |
| `32` | 8 | `latest_checkpoint_end_lsn`, or `0` |
| `40` | 8 | `checkpoint_redo_lsn`, or `0` |
| `48` | 8 | `reserved_txn_id_end` exclusive upper bound |
| `56` | 8 | `txn_status_reclaim_before` |
| `64` | 4 | `next_file_id` |
| `68` | 4 | `reserved32 = 0` |
| `72` | 8 | `next_catalog_object_id` |
| `80` | 4 | `crc32c` |
| `84` | 4 | `reserved32 = 0` |
| `88` | 4008 | reserved bytes, all zero |
|  | **4096** | **total slot** |

All multi-byte integers are little-endian.

V1 assigns no control flags. Nonzero `flags` or reserved bytes are rejected.

CRC32C is computed over exactly all 4096 slot bytes with bytes `80..83` logically zero.

### 13.2.2 Initial valid state

A freshly initialized database writes:

```text
slot 0:
    generation                 = 1
    latest_checkpoint_lsn      = 0
    latest_checkpoint_end_lsn  = 0
    checkpoint_redo_lsn        = 0
    reserved_txn_id_end        = FIRST_NORMAL_TXN_ID = 2
    txn_status_reclaim_before  = FIRST_NORMAL_TXN_ID = 2
    next_file_id               = 1
    next_catalog_object_id     = 1
    valid CRC

slot 1:
    all zero bytes
    therefore invalid as a control slot
```

`FileId{0}` remains invalid.

### 13.2.3 Validation and slot selection

Startup validates each slot independently:

```text
magic
format_version
header_size
zero flags/reserved bytes
CRC32C
nonzero generation
reserved_txn_id_end >= FIRST_NORMAL_TXN_ID
txn_status_reclaim_before >= FIRST_NORMAL_TXN_ID
txn_status_reclaim_before <= reserved_txn_id_end
(txn_status_reclaim_before - FIRST_NORMAL_TXN_ID) % 32640 == 0
next_file_id != INVALID_FILE_ID
next_catalog_object_id != 0
checkpoint field consistency
```

For no installed checkpoint:

```text
latest_checkpoint_lsn     = 0
latest_checkpoint_end_lsn = 0
checkpoint_redo_lsn       = 0
```

Otherwise all three are nonzero and:

```text
latest_checkpoint_lsn <= latest_checkpoint_end_lsn
```

During ordinary open, choose the structurally valid slot with the highest `generation`. During crash recovery, a checkpoint-bearing slot is usable only if its referenced checkpoint sequence is still present and validates; candidates may be considered in descending generation order.

Falling back to an older structurally valid slot is legal only when every recovery object it references is still retained and valid.

Equal-generation valid slots with different contents are corruption.

Generation is uint64 monotonic and MUST NOT wrap. An update when the selected generation is `UINT64_MAX` fails rather than wrapping.

### 13.2.4 Alternating update protocol

All control-state updates are serialized by one control-file update mutex and are constructed from the latest selected in-memory control state so unrelated fields cannot be lost by concurrent TxnId/FileId/checkpoint/reclamation updates.

To persist a new control state:

```text
1. choose the slot other than the currently selected valid slot
2. construct a complete new slot with generation = old_generation + 1
3. compute its CRC
4. pwrite exactly that 4096-byte slot
5. fdatasync(database.control)
6. only after successful sync publish that generation as current in memory
```

The previous valid slot remains a fallback if the new write tears.

### 13.2.5 FileId allocation

`next_file_id` is the next unassigned database FileId.

FileId allocation is serialized through the control-file update path:

```text
candidate = next_file_id
new_next  = candidate + 1
persist/sync new_next in a new control slot
only then return candidate
```

FileId allocation therefore tolerates crash gaps but never reuses a FileId that may have been published.

If increment would overflow uint32 or produce `INVALID_FILE_ID`, allocation fails rather than wrapping.

### 13.2.6 Catalog-object ID allocation

`next_catalog_object_id` is the next unassigned value in one database-wide uint64 catalog-object namespace.

This allocator is used for:

```text
TableId
IndexId
ConstraintId
```

The type wrappers remain distinct even though their numeric values come from one common allocator.

Allocation is serialized through the same alternating control-slot update path:

```text
candidate = next_catalog_object_id
new_next  = candidate + 1

durably publish new_next
only then return candidate
```

`0` is never assigned by this allocator.

Crash gaps are acceptable.

A catalog-object ID that may have appeared in persistent state is never reused, including after transaction abort.

If increment would overflow uint64 or produce `0`, allocation fails rather than wrapping.

`ColumnId` and built-in `TypeId` do not use this allocator.

## 13.3 Control-file update frequency

`database.control` is durably rewritten only for important global metadata transitions such as:

- durable transaction-ID block reservation,
- durable FileId or catalog-object-ID allocation,
- successful checkpoint installation,
- future database-format/global metadata changes.

Ordinary transactions do not rewrite/synchronize the control file.

This keeps commit scalability independent of control-file update latency.

## 13.4 Fuzzy checkpoint goal

A checkpoint does not stop transaction processing merely to force every dirty page.

It captures enough WAL/recovery state to bound later crash recovery while ordinary transactions and page modifications continue.

A completed fuzzy checkpoint does **not** imply that all pages dirtied before it are already in their data files.

## 13.5 Checkpoint identity and capture protocol

A checkpoint is identified persistently by:

```text
checkpoint_id = CHECKPOINT_BEGIN.lsn
```

The installed `database.control.latest_checkpoint_lsn` is exactly that BEGIN LSN.

The conceptual protocol is:

```text
1. append CHECKPOINT_BEGIN at LSN B
2. capture DPT + active writer state under their required short synchronization
3. append CHECKPOINT_DATA chunks, each referencing B
4. append CHECKPOINT_END referencing B
5. flush WAL through CHECKPOINT_END
6. persist database.control:
       latest_checkpoint_lsn     = B
       latest_checkpoint_end_lsn = END.lsn
       checkpoint_redo_lsn       = computed redo bound
7. fdatasync database.control
8. only then mark the new checkpoint/FPI epoch complete in memory
```

Checkpointing remains fuzzy and does not force all dirty pages.

## 13.6 CHECKPOINT_BEGIN payload

`CHECKPOINT_BEGIN` is a system WAL record (`txn_id=0`, `prev_txn_lsn=0`) with exact 32-byte payload:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | previous installed checkpoint BEGIN LSN, or `0` |
| `8` | 8 | captured `next_txn_id` high-water value |
| `16` | 8 | captured `reserved_txn_id_end` |
| `24` | 4 | captured `next_file_id` |
| `28` | 4 | `reserved32 = 0` |

The record's own LSN is the checkpoint ID.

## 13.7 CHECKPOINT_DATA payload and dirty-page/writer entries

One checkpoint may use zero or more `CHECKPOINT_DATA` system records (`txn_id=0`, `prev_txn_lsn=0`).

Each DATA payload starts with exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | `checkpoint_begin_lsn` |
| `8` | 4 | zero-based `chunk_index` |
| `12` | 4 | `dpt_count` |
| `16` | 4 | `writer_count` |
| `20` | 4 | `reserved32 = 0` |

Then come exactly `dpt_count` DPT entries, each 24 bytes:

```text
16-byte WAL PageId
8-byte rec_lsn
```

followed by exactly `writer_count` active-writer entries, each 16 bytes:

```text
8-byte TxnId
8-byte last_wal_lsn
```

No internal padding exists between entries.

A DPT snapshot includes every page already dirty before CHECKPOINT_BEGIN that remains dirty when captured, plus any page whose synchronized capture rules require inclusion.

Each `rec_lsn` retains §12.16 semantics and therefore identifies a full recovery image.

The active-writer table includes every transaction that has produced persistent WAL state and is nonterminal at capture.

A transaction with no persistent writes need not appear.

## 13.8 CHECKPOINT_END payload and completeness validation

`CHECKPOINT_END` is a system record (`txn_id=0`, `prev_txn_lsn=0`) with exactly 32 payload bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | `checkpoint_begin_lsn` |
| `8` | 4 | `data_record_count` |
| `12` | 4 | total DPT entries across DATA chunks |
| `16` | 4 | total writer entries across DATA chunks |
| `20` | 4 | `reserved32 = 0` |
| `24` | 8 | `checkpoint_redo_lsn` |

A checkpoint sequence is complete only when recovery can validate all of:

```text
BEGIN exists and its LSN == checkpoint_begin_lsn
END references that same BEGIN LSN
DATA records referencing that BEGIN have chunk_index exactly 0..N-1
N == END.data_record_count
summed DPT/writer counts equal END totals
all record CRC/length rules are valid
END.checkpoint_redo_lsn is valid for the reconstructed DPT
```

An incomplete, duplicate-index, mismatched-count, or cross-linked sequence is not installable and is ignored unless database.control incorrectly points to it, in which case startup reports control/checkpoint corruption and may fall back only to another independently valid control generation.

## 13.9 Checkpoint redo bound

The checkpoint redo bound is:

```text
if captured DPT nonempty:
    checkpoint_redo_lsn = minimum captured rec_lsn
else:
    checkpoint_redo_lsn = checkpoint_begin_lsn
```

Because every dirty interval begins with a full page image, every nonempty DPT `rec_lsn` is itself a reconstructible page-image LSN.

`checkpoint_redo_lsn` may be earlier than `checkpoint_begin_lsn`.

## 13.10 WAL recycling

Version 1 has no replication/PITR retention requirement.

The oldest installed recovery LSN is:

```text
wal_retention_floor = min(
    database.control.latest_checkpoint_lsn,
    database.control.checkpoint_redo_lsn
)
```

when a checkpoint is installed.

No WAL segment containing any byte at or after this floor may be recycled.

Also retain every segment containing the installed BEGIN/DATA/END sequence through `latest_checkpoint_end_lsn`.

Because each dirty page's `rec_lsn` identifies a full image, retaining from `checkpoint_redo_lsn` retains a complete reconstructible base for every dirty page captured by the installed checkpoint.

WAL recycling is conservative: retaining too much is permitted; deleting required WAL is corruption.

## 13.11 Recovery startup and WAL-tail validation

Startup:

1. validates both control slots and chooses the highest valid generation,
2. validates the referenced installed checkpoint sequence if nonzero,
3. scans WAL forward to identify the last complete valid record,
4. validates record length, alignment, segment containment, zero padding, embedded LSN, flags/reserved rules, and CRC,
5. stops at the first invalid/incomplete/torn **tail** record,
6. ignores/truncates bytes after the last valid WAL record,
7. reconciles unpublished page-file append tails per §4.11.3 during recovery.

Malformed required WAL before the valid-tail boundary is corruption, not ordinary crash-tail truncation.

## 13.12 Recovery phase 1: analysis

When an installed checkpoint exists:

```text
1. validate/load its complete CHECKPOINT_DATA state
2. initialize reconstructed DPT and active-writer table from that state
3. discard any checkpoint DPT entry for a TXN_STATUS page wholly below the durable `txn_status_reclaim_before` cutoff
4. initialize allocator high-water metadata from BEGIN/control information
5. scan WAL forward starting at checkpoint_begin_lsn
```

Starting at BEGIN is required because transactions/pages can change while the fuzzy checkpoint DATA records themselves are being emitted.

During the forward analysis scan:

- a redoable page record inserts/updates DPT information without replacing an earlier required `rec_lsn`,
- each BTREE_MTR does the same for every affected page,
- TXN_COMMIT marks its transaction terminal COMMITTED,
- TXN_ABORT marks it terminal ABORTED,
- later activity updates active/loser writer state,
- observed TxnIds advance the recovered high-water requirement.

A writer that remains nonterminal at analysis end is a crash loser.

When no checkpoint exists, analysis starts at the oldest retained WAL record required for database creation/recovery.

## 13.13 Recovery phase 2: redo

Redo begins at:

```text
database.control.checkpoint_redo_lsn
```

for an installed checkpoint, or the earliest required WAL LSN otherwise.

This redo scan may therefore begin **before CHECKPOINT_BEGIN**.

That is intentional.

### 13.13.1 Page redo

For PAGE_INIT/PAGE_IMAGE/PAGE_DELTA and BTREE_MTR page entries, apply the page-LSN/full-image rules from Chapter 12.

A redo action targeting a TXN_STATUS page wholly below the durable reclaim cutoff is skipped as retired history.

A trusted valid page whose `page_lsn >= record.lsn` skips that page's redo action.

### 13.13.2 Terminal status redo

`TXN_COMMIT` and `TXN_ABORT` are also redoable terminal-status evidence.

When encountered anywhere in the redo scan, including before CHECKPOINT_BEGIN, recovery repairs the corresponding transaction-status entry if its page does not yet reflect that terminal record **and** the TxnId is not below `database.control.txn_status_reclaim_before`.

Terminal records below the durable reclaim cutoff are logically retired history and do not recreate punched status pages.

This is required for a retained dirty transaction-status page whose `rec_lsn` predates checkpoint begin and whose terminal status was never forced to the status file.

### 13.13.3 BTREE_MTR atomicity

A complete valid BTREE_MTR is one committed system mini-transaction for recovery.

For each affected page with older/untrusted state, apply its full image or patches and set `page_lsn = mtr.lsn`.

A torn/incomplete MTR record is not a record and is never partly replayed.

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

1. `database.control` has two independently checksummed 4096-byte generation slots.
2. Control generation never wraps; highest independently valid generation wins.
3. `latest_checkpoint_lsn` means the installed CHECKPOINT_BEGIN LSN exactly.
4. A completed checkpoint does not require all dirty data pages to have been flushed.
5. A checkpoint DATA sequence is identified by BEGIN LSN and validates contiguous chunk indexes/count totals.
6. The installed checkpoint has durable END and a durably synchronized control slot.
7. Every DPT `rec_lsn` identifies a complete recovery page image.
8. WAL recycling retains from `min(latest_checkpoint_lsn, checkpoint_redo_lsn)` and cannot discard a needed dirty-page base image.
9. Analysis initializes from checkpoint DATA and scans forward from CHECKPOINT_BEGIN.
10. Redo starts at checkpoint_redo_lsn, which may precede CHECKPOINT_BEGIN.
11. Pre-BEGIN TXN_COMMIT/TXN_ABORT records in the redo range can repair dirty transaction-status pages.
12. Recovery validates/truncates only an invalid WAL tail; malformed required earlier WAL is corruption.
13. Redo is page-LSN idempotent for trusted pages.
14. A corrupt/torn page LSN is not trusted until reconstructed from retained full-image WAL.
15. A complete BTREE_MTR is replayed as one system action; a torn MTR is never partly replayed.
16. Crash losers are resolved by publishing ABORTED, not by ordinary physical user-DML undo.
17. Ordinary v1 user-DML recovery requires no CLRs.
18. Unpublished append-tail pages are removed only after durable initialization publications are reconstructed.
19. Rebuildable approximate metadata is not on the critical recovery path.
20. SQL traffic begins only after recovery completion and a recovery checkpoint.
21. A transaction acknowledged committed remains logically committed across every later successful crash/restart.

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

Physical RID identity safety uses a lightweight `ReadEpochManager`, separate from MVCC visibility.

The v1 correctness baseline intentionally uses one mutex-protected epoch registry rather than a lock-free epoch scheme.

Process-local state is:

```text
uint64 current_epoch = 1
map<uint64, uint64 active_reader_count>
```

Epoch value `0` is invalid.

### 14.6.1 Reader registration

Before an executor can read an index entry or otherwise retain a stored RID beyond the current protected page operation, it registers:

```text
lock epoch mutex
e = current_epoch
active_reader_count[e] += 1
unlock
```

The returned `e` is held by an RAII read-epoch guard.

Guard destruction decrements that exact epoch count under the same mutex and removes a zero count.

A reader never changes epoch while one guard is alive.

### 14.6.2 RID retirement

After every required exact index entry has been removed and the heap slot has been installed persistently as `DEAD`, vacuum retires the RID:

```text
lock epoch mutex
retire_epoch = current_epoch
if current_epoch == UINT64_MAX:
    fail/require maintenance restart before further RID reuse
current_epoch += 1
record (RID, retire_epoch) in the process-local retire queue
unlock
```

Retirement increments the epoch exactly once.

A reader that registered before or during index cleanup may conservatively hold an epoch `<= retire_epoch`.

A reader that registers after retirement obtains an epoch `> retire_epoch` and cannot have obtained the removed index entry through normal post-retirement traversal.

### 14.6.3 Exact grace predicate

A retired RID with epoch `R` is safe for `DEAD -> UNUSED` exactly when, under the epoch mutex:

```text
there exists no active reader epoch E such that E <= R
```

Equivalent implementation:

```text
active_reader_count empty
OR
minimum active epoch > R
```

No heuristic time delay substitutes for this predicate.

### 14.6.4 Crash/restart

Epochs and the retire queue are process-local and not WAL logged.

After crash/restart there are no surviving pre-crash readers.

A recovered persistent `DEAD` slot that is not represented in the new process retire queue is conservatively re-enqueued with a fresh retirement epoch before being converted to `UNUSED`.

This may delay reuse unnecessarily but cannot reuse too early.

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

`DEAD` is persistent.

After WAL-protected index cleanup/retirement, crash may leave:

```text
NORMAL
    -> vacuum re-runs exact cleanup idempotently

DEAD
    -> required index cleanup completed before the persisted transition
```

Pre-crash epoch registrations do not survive.

A recovered DEAD slot is re-enqueued in the new process ReadEpochManager before reuse, as §14.6.4 specifies.

## 14.9 Vacuum index-cleanup protocol

For one garbage-eligible tuple version with RID `R`:

```text
1. read the tuple header and resolve:
       ResolveSchema(TableId, tuple.schema_version)
   through the historical catalog contract
2. use that historical schema to read/copy enough tuple data
   to derive every indexed user key
3. re-check garbage eligibility
4. for every index:
       EraseIfPresent(encoded_user_key, R)
       through ordinary B+ system MTRs
5. re-fetch heap page
6. under write latch verify tuple/slot identity and expected header state
7. WAL-log/install slot NORMAL -> DEAD
8. release heap latch
9. register RID retirement with ReadEpochManager
```

`EraseIfPresent(key,RID)` is vacuum-idempotent:

```text
exact physical entry removed -> success
exact physical entry already absent -> success
B+ corruption / unrelated failure -> error
```

Because the physical B+ key is `(user_key,RID)`, there can be at most one exact target entry.

This idempotence is required after a crash that removed some/all exact index entries but did not yet persist `NORMAL -> DEAD`.

The heap slot MUST NOT become DEAD before every required secondary-index entry is known absent.

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

## 14.12 DEAD to UNUSED and free-slot publication

A slot may enter `UNUSED` only when **both** are true:

```text
read-epoch grace predicate satisfied
required version-chain splicing complete
```

Before final reuse publication, vacuum must prove that no surviving tuple version still has `prev` equal to this RID. If that proof cannot be reconstructed/revalidated, the slot remains DEAD.

Then:

```text
1. fetch and write-latch the heap page
2. verify the slot is still DEAD
3. revalidate that version-chain reuse prerequisites remain satisfied
4. if its payload coordinates are nonzero:
       compact/repack while the slot is still logically DEAD
       produce canonical DEAD coordinates (0,0)
5. verify tuple_offset = 0 and tuple_length = 0
6. prepare the free-list transition:
       slot.state = UNUSED
       slot.aux   = old free_slot_head
       free_slot_head = this SlotId
7. WAL-log/install the page mutation as one page redo unit
8. update/rebuild advisory free-space metadata
```

The slot is not observable as reusable between payload reclamation and free-list publication.

The final canonical reusable slot is:

```text
tuple_offset = 0
tuple_length = 0
state        = UNUSED
aux          = next free SlotId or INVALID_SLOT_ID
```

Every v1 reusable slot is discovered through the persisted `free_slot_head` chain defined in §5.3.2.

Insertion never reuses a DEAD slot directly.

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

## 14.14 Transaction-status retention and physical reclamation

After sufficiently complete vacuum/freezing proves a cutoff `X` such that no persistent correctness object references a normal TxnId below `X`, whole transaction-status pages below that cutoff may be retired.

The absolute §9.12 TxnId-to-PageNo mapping is preserved permanently.

V1 therefore reclaims old status history with **sparse hole punching while keeping file length and logical PageNos unchanged**.

### 14.14.1 Page-aligned reclaim cutoff

Status page `P >= 1` represents:

```text
first_txn(P) = FIRST_NORMAL_TXN_ID + (P - 1) * 32640
end_txn(P)   = first_txn(P) + 32640
```

where `end_txn` is exclusive.

A page is reclaimable only when:

```text
end_txn(P) <= proven frozen cutoff X
```

The persisted:

```text
database.control.txn_status_reclaim_before
```

is always page-aligned and means:

> every normal TxnId below this value belongs to logically retired status history.

Initial value is `FIRST_NORMAL_TXN_ID`.

### 14.14.2 Safe reclaim order

To advance the cutoff to page-aligned value `C`:

```text
1. prove all pages below C are semantically retireable
2. durably update database.control.txn_status_reclaim_before = C
3. publish the same cutoff in the runtime TransactionStatusStore so no new lookup pins a retired page
4. for each affected TXN_STATUS page:
       wait for existing pins/I/O to drain without holding unrelated page latches
       mark the frame retired/non-writeback
       remove it from BufferPool/DPT ownership without flushing obsolete contents
5. only then optionally punch the corresponding full-page byte ranges from txn_status.dat using a keep-size sparse-file operation
```

For status PageNo `P`, the physical punch range is exactly:

```text
[P * PAGE_SIZE, (P + 1) * PAGE_SIZE)
```

The order is deliberate:

- crash after step 2 but before frame invalidation/punching leaks physical space only,
- no retired dirty frame can later rewrite a punched page,
- the architecture never permits punching pages that the durable cutoff still says may be needed.

On Linux the baseline physical optimization may use:

```text
fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE)
```

for whole status-page byte ranges.

If hole punching is unsupported or fails, correctness is unaffected; the old blocks may remain physically allocated and reclamation can be retried later.

The logical file size is not shrunk and later PageNos are never renumbered.

### 14.14.3 Lookup below the cutoff

Status lookup for:

```text
txn_id < txn_status_reclaim_before
```

returns runtime result `RETIRED` without reading/validating a punched status page.

Valid persistent tuple/catalog state must not reference such a TxnId; creators that still matter must have been rewritten to `FROZEN_TXN_ID` before the cutoff advanced.

Physical sparse reclamation therefore preserves the deterministic absolute mapping while bounding allocated disk blocks for old history.

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

1. The global reclamation horizon is derived from registered SQL snapshots, not transaction existence.
2. Vacuum never uses only its own snapshot visibility as the garbage criterion.
3. In-progress creator/deleter state is not reclaimed by guesswork.
4. Required secondary-index entries are known absent before `NORMAL -> DEAD`.
5. Vacuum exact-index removal is idempotent across crash/retry.
6. `DEAD` is persistent and means semantically cleaned but not physically reusable.
7. Reader epoch registration occurs before an executor may retain/index-consume a stored RID.
8. RID retirement increments the process epoch and records the pre-increment retirement epoch.
9. `DEAD -> UNUSED` is legal only when no active reader epoch is `<= retire_epoch`.
10. `DEAD -> UNUSED` also requires proof that all surviving direct-successor version-chain links have been spliced away from that RID.
11. Whole heap-page recycling cannot bypass RID-reuse safety.
12. A surviving version never keeps `prev` pointing at storage that may be reused.
13. Payload compaction occurs while the slot is DEAD; the final reusable UNUSED slot has zero coordinates.
14. Every reusable UNUSED slot appears exactly once in the persisted free-slot list.
15. Candidate state is revalidated under page latch before physical transition.
16. Vacuum never waits for ordinary transactions or performs B+ cleanup while holding the heap-page latch.
17. Aborted `xmax` cleanup does not require an index change.
18. Freezing is WAL protected and makes original creator status unnecessary for future visibility.
19. The transaction-status reclaim cutoff advances durably before any old status-page hole is punched.
20. Sparse status reclamation preserves absolute status PageNos and file length.
21. B+ cleanup remains a B+ MTR system action.
22. FSM/statistical maintenance cannot weaken reclamation correctness.

---

# 15. Transactional Write Protocols

## 15.1 Scope and ownership

This chapter integrates the component contracts from heap/tuple storage, FSM, BufferPool, B+ tree, transactions/snapshots, LockManager, WAL/CommitCoordinator, and vacuum.

It defines cross-subsystem ordering for user DML and transaction completion.

The owning subsystem chapters remain authoritative for their local byte formats, visibility algorithms, lock semantics, WAL codecs, and recovery rules.

These responsibilities MUST NOT collapse into one monolithic “transaction engine” abstraction that hides ownership/lifetime boundaries.

### 15.1.1 DDL writer-gate integration

Before the first persistent write to target table `T`, a DML transaction acquires the shared `TableWriterGate(T)` defined by §21.2.1 and holds it until terminal transaction publication.

The gate is acquired before TUPLE_WRITE/UNIQUE_KEY locks and before any heap/B+ latch that could be retained across a wait.

Ordinary DML transactions may hold shared gates for multiple target tables; shared gates do not conflict with one another.

Schema-changing DDL follows the global ordering:

```text
SchemaLock
    -> target TableWriterGate exclusive, when required
    -> logical tuple/unique locks, when required
    -> physical page/B+ latches
```

V1 does not perform an exclusive TableWriterGate upgrade while the same transaction already holds that table's shared gate. A schema-changing DDL statement that would require such an upgrade after prior persistent DML in the same explicit transaction is rejected rather than risking an upgrade deadlock.

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
5. install/update COMMITTED in the transaction-status page/cache
6. set status-page page_lsn = commit_lsn
7. execute the §9.14 runtime terminal-publication linearization
8. release TUPLE_WRITE and UNIQUE_KEY locks
9. release shared TableWriterGate holdings
10. unregister the transaction's own active snapshot(s)
11. finish transaction object cleanup
12. return success
```

Commit does **not** force dirty heap/index pages.

Once a valid TXN_COMMIT record is durable, that transaction outcome cannot subsequently become ABORTED.

## 15.6 ABORT

For an abortable transaction:

```text
1. state ACTIVE/eligible transient state -> ABORTING
2. if persistent WAL-visible state exists:
       append TXN_ABORT
3. install/update ABORTED transaction status
4. if an abort record exists:
       set status-page page_lsn = abort_lsn
5. execute the §9.14 runtime terminal-publication linearization
6. release logical locks
7. release shared TableWriterGate holdings
8. unregister the transaction's own snapshots
9. finish transaction object cleanup
10. return/raise abort
```

Ordinary abort performs no write-set scan to restore old heap/index bytes.

Aborted physical versions and index entries remain vacuum input.

A transaction whose COMMIT is already durable is no longer eligible to transition to ABORTED.

## 15.7 READ COMMITTED retry boundary

V1 does **not** provide statement-level physical undo, savepoints, or subtransaction outcome IDs.

Therefore one statement attempt tracks process-local:

```text
has_persistent_statement_writes
```

which becomes true immediately after the first persistent heap/index/catalog WAL-visible mutation belonging to that attempt is installed.

### 15.7.1 Retry before the first persistent write

If a READ COMMITTED conflict requires fresh-snapshot restart while:

```text
has_persistent_statement_writes == false
```

then the engine may:

```text
release attempt-local resources
unregister the old statement snapshot
capture a fresh statement snapshot
restart candidate search/evaluation
```

No database state from the abandoned attempt needs rollback.

### 15.7.2 Conflict after a persistent statement write

If a conflict requiring statement restart is discovered after:

```text
has_persistent_statement_writes == true
```

v1 MUST NOT restart that statement inside the same transaction.

Instead:

```text
abort the transaction
return a retryable serialization/write-conflict outcome to the transaction-owning layer
```

An autocommit SQL layer may later choose to create a **new TxnId** and rerun the entire statement according to its own bounded retry policy.

An explicit user transaction is aborted and the client/application decides whether to retry.

This is necessary because writes from the abandoned statement attempt would otherwise become visible if the same transaction later advanced `CommandId`.

### 15.7.3 External output

Even a pre-write-retryable statement MUST NOT expose irreversible external rows/side effects before its retry-safe point.

`RETURNING` output is buffered until the attempt can no longer internally restart.

Future statement savepoints/subtransactions could relax this v1 restriction, but they are not implicitly present.

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
3. Runtime terminal publication linearizes before transaction logical locks are released.
4. Logical lock waits occur outside physical page/B+ latch waits.
5. Index entries never decide MVCC visibility by themselves.
6. UPDATE/DELETE revalidate the target after logical-lock acquisition.
7. READ COMMITTED may internally restart only before the current statement attempt has installed its first persistent write.
8. A retry-requiring READ COMMITTED conflict after a persistent statement write aborts the transaction.
9. Any higher-level automatic rerun after such an abort uses a new transaction identity.
10. REPEATABLE READ aborts on a conflicting committed post-snapshot write.
11. User abort changes logical outcome without requiring physical heap/index rollback.
12. B+ structural MTRs may survive user abort.
13. Transaction-lifetime tuple/unique locks are released only after terminal outcome publication.
14. Vacuum performs delayed exact index garbage removal and RID reuse.
15. External output from a potentially retryable statement does not escape before its retry-safe boundary.

---

# Part V — Catalog and SQL Semantics

# 16. Catalog and Schema Metadata

## 16.1 Role and dependency boundary

The catalog is the authoritative semantic metadata layer between persisted transactional storage and the SQL semantic layer.

The upper-layer dependency direction is:

```text
SQL text
  ↓
Lexer
  ↓
Parser
  ↓
AST
  ↓
Binder
  ├── Catalog
  └── Type System
  ↓
Bound Statements / Bound Expressions
  ↓
Logical Planner
```

The parser does not know physical storage.

The binder does not choose physical algorithms.

Catalog descriptors describe semantic objects and their persistent identities; they do not expose heap-page, B+ node, or BufferPool implementation details.

The catalog owns knowledge about at least:

- tables,
- columns,
- logical types,
- indexes,
- uniqueness,
- primary keys,
- nullability,
- namespaces,
- stable object IDs,
- schema versions,
- statistics metadata handles.

It does not own:

- heap-page bytes,
- B+ node layout,
- transaction snapshot implementation,
- SQL tokenization/parsing.

## 16.2 V1 namespace model

Version 1 has one logical database and one SQL namespace:

```text
main
```

Unqualified table names resolve in `main`.

Both:

```sql
SELECT * FROM users;
SELECT * FROM main.users;
```

refer to the same namespace lookup.

Multiple SQL schemas/namespaces are deferred.

Within `main`:

```text
table names are unique among live tables
index names are unique among live indexes
column names are unique within one table schema version
```

Table and index names occupy separate semantic name classes.

## 16.3 Stable catalog identities

The stable identifier widths are:

```text
TableId  = uint64
ColumnId = uint32
IndexId  = uint64
TypeId   = uint32
```

A table's `ColumnId` is stable for the lifetime of that column identity.

Internal references after binding use stable IDs rather than object-name strings.

These identities are distinct from:

```text
BindingId
logical output slot
physical column position
display position
```

V1 allocates `TableId`, `IndexId`, and `ConstraintId` from the durable database-wide catalog-object allocator in §13.2.6.

The allocator never returns zero, tolerates crash gaps, and never reuses an ID that may have appeared in persistent state.

Column IDs are table-local:

```text
CREATE TABLE:
    assign ColumnId values 1..N in declaration order
```

`ColumnId{0}` is not assigned by v1.

A future schema evolution that adds a column allocates a value greater than every historical ColumnId for that table; dropped/removed ColumnIds are never reused.

Built-in TypeIds remain the fixed registry in §16.4 rather than using the catalog-object allocator.

## 16.4 Built-in TypeId registry

Because `sys_columns.type_id` is persistent semantic metadata, v1 assigns stable built-in TypeId values:

| TypeId | Logical type |
|---:|---|
| `0` | invalid / not a stored type |
| `1` | BOOLEAN |
| `2` | INT32 |
| `3` | INT64 |
| `4` | FLOAT64 |
| `5` | DATE |
| `6` | TIMESTAMP |
| `7` | VARCHAR |

`NULL` and `UNKNOWN` are not legal persisted column TypeIds.

These TypeId values are a catalog-format contract and MUST NOT be renumbered because of source-language enum order.

## 16.5 Catalog system relations

Persistent semantic metadata is represented through relational system tables.

The initial system relations are:

```text
sys_tables
sys_columns
sys_indexes
sys_index_columns
sys_constraints
sys_statistics
```

Their locked semantic fields are:

### 16.5.1 `sys_tables`

```text
table_id
namespace
table_name
heap_file_id
fsm_file_id
schema_version
flags
```

### 16.5.2 `sys_columns`

```text
table_id
column_id
column_name
logical_position
type_id
nullable
default_expr_blob/reference
schema_version_added
schema_version_removed
flags
```

The schema-version membership semantics are half-open:

```text
column belongs to schema version V
iff
    schema_version_added <= V
and
    (schema_version_removed is absent
     or V < schema_version_removed)
```

An absent removal version means the column remains present in the latest known schema.

### 16.5.3 `sys_indexes`

```text
index_id
table_id
index_name
btree_file_id
unique
primary
key_schema_version
flags
```

### 16.5.4 `sys_index_columns`

```text
index_id
ordinal
column_id
```

### 16.5.5 `sys_constraints`

```text
constraint_id   // uint64, allocated from the global catalog-object allocator
table_id
constraint_type
constraint_name
definition payload
```

Constraint names are table-local in v1.

### 16.5.6 `sys_statistics`

```text
table_id
column_id / object scope
stats_version
row_count
null_fraction
ndv estimate
min/max
histogram payload
```

The exact physical row layouts of these system relations may evolve.

Their semantic fields and stable identity relationships are architectural.

## 16.6 Immutable descriptors

Catalog lookup produces immutable semantic descriptors such as:

```text
TableDescriptor
SchemaDescriptor
IndexDescriptor
```

A `TableDescriptor` identifies at least:

```text
TableId
namespace/name
heap FileId
FSM FileId
current SchemaVer
current schema descriptor
index/constraint metadata references
```

A `SchemaDescriptor` identifies one exact:

```text
(TableId, SchemaVer)
```

and contains enough immutable information to interpret tuple format v1 for that schema version, including:

```text
stable ColumnIds
physical-column order
logical display positions
logical types / TypeIds
nullability
default metadata where applicable
```

An `IndexDescriptor` contains at least:

```text
IndexId
TableId
index name
B+ FileId
unique/primary semantics
ordered key ColumnIds
key-schema version
```

Descriptor representation may use compact derived/precomputed layouts, but descriptor meaning is independent of C++ container addresses.

## 16.7 Historical schema interpretation

The catalog MUST support the semantic lookup:

```text
ResolveSchema(TableId, SchemaVer)
    -> immutable SchemaDescriptor
```

for every schema version that may still appear in persisted tuple versions or remain referenced by a live query/catalog descriptor.

Tuple decoding uses the tuple's persisted:

```text
schema_version
```

to choose that historical physical schema.

Vacuum likewise resolves the historical schema before decoding an old tuple version to reconstruct indexed user keys.

Historical schema metadata MUST NOT be discarded while a persisted tuple version can still require it for:

- tuple decoding,
- visibility/maintenance inspection,
- exact index garbage cleanup,
- version-chain maintenance.

The table `schema_version` is the version of the logical/physical column schema used to interpret tuple bodies.

A DDL commit that changes that tuple-interpreting schema monotonically increases `schema_version`.

Index-only metadata changes such as CREATE INDEX / DROP INDEX do not force otherwise-identical tuples to acquire a new tuple schema version.

Version-1 DDL may reject changes that require unsupported tuple rewriting, but the metadata model itself remains history-capable.

## 16.8 Column identity versus position

`ColumnId` is not a display or storage ordinal.

The architecture distinguishes:

```text
ColumnId
physical schema position
logical display position
logical-plan output slot
```

Never assume:

```text
ColumnId == zero-based SELECT * position
```

This permits future add/drop/reorder behavior while old tuple versions remain interpretable.

`SELECT *` uses the logical presentation order of the bound schema, not ColumnId numeric order.

## 16.9 Catalog bootstrap

A small bootstrap catalog MAY exist solely to locate and interpret the self-hosted catalog relations.

Bootstrap metadata contains only the minimum needed to locate/interpret:

```text
sys_tables
sys_columns
sys_indexes
...
```

After bootstrap, ordinary metadata lookup uses the catalog system relations.

The architecture MUST NOT maintain two indefinitely divergent authoritative metadata systems.

The byte-exact bootstrap representation and the role/layout of the reserved `CATALOG_DATA` page type are not yet defined by the v1 contract; see R-036.

## 16.10 Catalog cache and descriptor lifetime

Frequently used immutable descriptors may be cached in memory.

The cache is an acceleration structure, **not** a catalog-visibility authority.

Cache keys use stable object IDs/schema versions and names where name lookup is required.

A cache hit may be used only when its descriptor is proven visible to the caller's catalog snapshot; otherwise lookup falls back to snapshot-aware catalog relations.

Published descriptors are immutable snapshots.

A schema-changing operation invalidates/replaces current-name/current-version cache entries only after its transaction reaches the terminal publication boundary.

Older immutable descriptors may remain alive while older snapshots/queries still reference them.

Bound/planned queries may retain shared immutable descriptors or generation/versioned handles for their required lifetime.

The exact C++ ownership mechanism is implementation-specific.

The semantic lifetime guarantee is not.

Uncommitted DDL may be visible to its own transaction through normal MVCC/self-visibility after the command boundary, but it is not installed into the globally published committed-descriptor cache.

## 16.11 Catalog invariants

1. The catalog maps human-readable names to stable semantic identities.
2. Parser output does not contain resolved catalog IDs.
3. Bound references use stable IDs rather than unresolved object-name strings.
4. TableId, ColumnId, IndexId, BindingId, schema position, and plan output slots are distinct identities.
5. Built-in persisted TypeIds are stable numeric codes.
6. Catalog system-relation semantic fields are stable even if their physical row layout evolves.
7. Catalog descriptors are immutable snapshots once published.
8. Active queries are never made to observe in-place descriptor mutation.
9. Every persisted tuple schema version that may still exist has a resolvable historical SchemaDescriptor.
10. Historical schema interpretation is retained long enough for vacuum to reconstruct exact old index keys.
11. Catalog cache lookup never bypasses the caller's MVCC/catalog snapshot.
12. Uncommitted DDL is never published as globally committed cache metadata.
13. TableId/IndexId/ConstraintId values are durably allocated before persistent use and never reused.
14. ColumnId values are table-local and never reused within a table's historical schema.
15. Bootstrap metadata remains minimal and cannot become a permanently divergent second catalog.

---

# 17. SQL Type and Value System

## 17.1 Scope

The SQL type subsystem defines logical scalar types, nullability/value-state semantics, type promotion, casts, comparison compatibility, BOOLEAN three-valued logic, planning-time scalar values, and centralized operator/function type resolution.

Storage byte layout remains owned by Chapter 5.

Index-key byte ordering remains owned by Chapter 8.

The SQL type layer MUST use semantics compatible with both.

## 17.2 Logical scalar types

The initial logical scalar kinds are:

```text
BOOLEAN
INT32
INT64
FLOAT64
DATE
TIMESTAMP
VARCHAR
NULL
UNKNOWN
```

A compact value-like representation is used conceptually:

```text
LogicalType {
    TypeKind kind;
}
```

Type descriptors do not require an inheritance hierarchy or one heap allocation per type.

Future type parameters may extend this representation.

## 17.3 Storable types versus semantic pseudo-types

Stored table columns use only:

```text
BOOLEAN
INT32
INT64
FLOAT64
DATE
TIMESTAMP
VARCHAR
```

`NULL` and `UNKNOWN` are not legal stored schema types.

SQL NULL is primarily a value state attached to a concrete logical type.

`UNKNOWN` is a binder-only unresolved type used for context-dependent expressions such as an untyped NULL literal and future parameters.

`NULL` may be used by generic non-hot semantic values to denote an explicitly null-only value before contextual coercion, but it does not become a persisted column type or executor vector element type.

Where context supplies a concrete target type, an unresolved NULL is coerced to:

```text
target LogicalType
+
is_null = true
```

## 17.4 Scalar semantic baseline

### 17.4.1 BOOLEAN

BOOLEAN has SQL TRUE/FALSE values plus nullable NULL state.

No integer truthiness is implied.

### 17.4.2 INT32 and INT64

Signed integer semantics correspond to the persisted two's-complement widths from Chapter 5.

### 17.4.3 FLOAT64

FLOAT64 uses IEEE-754 binary64.

SQL comparison/equality semantics MUST agree with the v1 total-order/index semantics from Chapter 8:

```text
-infinity
...
finite values
...
+infinity
NaN
```

For comparison/equality purposes in v1:

- `-0.0` and `+0.0` compare equal,
- all NaNs are canonical-equivalent,
- NaN sorts after `+infinity`.

Execution, constant folding, hashing/grouping support, and B+ ordering MUST NOT silently use incompatible equality/order rules.

### 17.4.4 DATE

DATE is a signed 32-bit count of civil days from:

```text
1970-01-01
```

using the proleptic Gregorian calendar.

The persisted scalar is the signed day count.

### 17.4.5 TIMESTAMP

TIMESTAMP is a signed 64-bit count of microseconds from:

```text
1970-01-01 00:00:00
```

Version 1 TIMESTAMP is timezone-naive.

Timezone-aware types/conversion rules are deferred.

Leap-second modeling is not part of the v1 scalar contract.

### 17.4.6 VARCHAR

VARCHAR values are byte strings.

Version 1 uses:

```text
binary bytewise comparison/collation
```

matching Chapter 8 index ordering.

The v1 type layer does not require UTF-8 validation, locale collation, character-count semantics, or `VARCHAR(n)` length semantics.

String length in the baseline representation is a byte length.

## 17.5 Numeric promotion

The implicit numeric widening hierarchy is:

```text
INT32
  ↓
INT64
  ↓
FLOAT64
```

Mixed numeric arithmetic/comparison chooses the smallest common promoted type.

Examples:

```text
INT32 + INT64    -> INT64
INT64 + FLOAT64  -> FLOAT64
```

Implicit narrowing is not allowed.

## 17.6 String coercion

The binder does not silently convert arbitrary numeric/date values to VARCHAR merely to make a comparison type-check.

For example:

```sql
1 = '1'
```

does not become true through hidden string conversion.

Such conversions require an explicit cast unless another explicitly registered operator signature says otherwise.

## 17.7 BOOLEAN context and three-valued logic

WHERE/HAVING/JOIN predicates require BOOLEAN.

C-like truthiness such as:

```sql
WHERE 5
```

is rejected.

SQL BOOLEAN semantics are:

```text
TRUE
FALSE
UNKNOWN
```

where UNKNOWN is represented by nullable BOOLEAN with NULL state.

The required truth cases include:

```text
TRUE  AND NULL -> NULL
FALSE AND NULL -> FALSE

TRUE  OR NULL  -> TRUE
FALSE OR NULL  -> NULL

NOT NULL       -> NULL
```

Binder-time folding and execution-time evaluation use the same truth tables.

## 17.8 Comparison and NULL

Ordinary comparisons:

```text
=
<>
<
<=
>
>=
```

with a NULL operand produce NULL.

NULL testing uses:

```sql
IS NULL
IS NOT NULL
```

The binder MUST NOT rewrite `x = NULL` into `x IS NULL`.

Non-NULL comparisons require compatible types after allowed implicit promotion/casts.

VARCHAR comparison is binary bytewise.

FLOAT64 comparison follows §17.4.3.

## 17.9 Cast model

Version 1 supports explicit:

```sql
CAST(expr AS type)
```

Safe implicit casts are:

```text
INT32 -> INT64
INT32 -> FLOAT64
INT64 -> FLOAT64
UNKNOWN NULL -> contextual target type
```

Other conversions require an explicit cast.

Initial explicit conversions may include:

```text
INT32 <-> INT64 where range-valid
numeric -> FLOAT64
numeric -> VARCHAR
BOOLEAN -> VARCHAR
DATE/TIMESTAMP -> VARCHAR
VARCHAR -> numeric/date/timestamp when parsing succeeds
```

A runtime conversion failure is a SQL error.

An explicit narrowing integer cast performs a range check rather than wrapping silently.

## 17.10 Central TypeResolver

Type compatibility is centralized in a semantic component conceptually named `TypeResolver`.

It owns:

```text
common numeric type
implicit-cast legality
explicit-cast legality
comparison compatibility
arithmetic result type
operator signature matching
function signature matching
CASE branch common-type resolution
IN-list common-type resolution
```

Binder, constant folding, and execution MUST NOT independently invent coercion rules.

## 17.11 Generic scalar Value

A generic owned `Value` representation is acceptable in non-hot semantic paths such as:

```text
parser literals
constant folding
catalog defaults
tests
diagnostic/debug printing
```

Conceptually it contains:

```text
LogicalType
is_null
small scalar payload
owned VARCHAR bytes
```

It is not the per-cell representation for hot vectorized execution.

The executor's vector representation is defined later.

## 17.12 Type/value invariants

1. NULL is primarily a value state, not a storable column type.
2. UNKNOWN is binder-only and never appears in persisted schemas.
3. Numeric implicit conversion widens and never silently narrows.
4. Non-BOOLEAN predicates are rejected.
5. SQL Boolean semantics are three-valued.
6. Ordinary comparison with NULL returns NULL.
7. `x = NULL` is not rewritten as `x IS NULL`.
8. General expression comparison does not silently stringify unlike values.
9. VARCHAR v1 equality/order is binary bytewise and agrees with B+ ordering.
10. FLOAT64 SQL comparison semantics agree with B+ key semantics.
11. DATE/TIMESTAMP logical units agree with their persisted signed scalar representations.
12. TypeResolver is the single semantic owner of coercion/type compatibility.
13. Generic Value is not the hot executor cell representation.

---

# 18. Lexer, Parser, and AST

## 18.1 Front-end boundary

The SQL front end is implemented in-project.

It consists of:

```text
Lexer
Parser
AST
```

followed by the separate Binder in Chapter 19.

Parser output represents syntax.

Object lookup, type resolution, and database identity belong to binding.

The front end targets an explicit SQL subset rather than pretending to implement the full SQL standard.

## 18.2 Token model

The handwritten lexer emits tokens containing at least:

```text
TokenKind
source byte span
optional literal payload
source offset / diagnostic location
```

Token spans use half-open source-byte ranges `[start, end)`.

Line/column information may be cached or derived from byte offsets.

The parser does not repeatedly rescan source text merely to rediscover token boundaries.

## 18.3 Token classes

Initial token classes include:

```text
identifier
quoted identifier
integer literal
floating literal
string literal
keyword
operator
punctuation
end-of-input
```

Keywords are case-insensitive.

They may be recognized directly or through identifier-to-keyword lookup.

## 18.4 Identifier rules

Unquoted identifiers are normalized to lowercase.

Thus `Users`, `USERS`, and `users` all resolve textually as `users`.

Quoted identifiers preserve exact spelling/case.

AST identifiers remain textual until binding.

## 18.5 String literals

Single quotes delimit SQL string literals.

A literal single quote is represented by doubled quote:

```sql
'It''s fine'
```

The token payload contains decoded logical bytes while the original source span remains available for diagnostics.

Vendor-specific backslash escape modes are not part of the baseline.

An unterminated string is a source-positioned lexical error.

## 18.6 Numeric literals

The lexer recognizes at least:

```text
123
1.25
1e10
1.2e-3
```

Leading `-` is preferably tokenized as the unary operator rather than embedded in every numeric token.

Thus `-2147483648` is parsed as unary minus applied to a positive numeric literal representation, keeping type/range resolution explicit.

Literal overflow/type selection is a semantic/type-resolution concern rather than unchecked host-language overflow in the lexer.

## 18.7 Comments

Version 1 recognizes line comments and non-nested block comments:

```sql
-- line comment
/* block comment */
```

Nested block comments are deferred.

An unterminated block comment is a source-positioned lexical error.

## 18.8 Source locations

Every AST node retains a source span:

```text
start byte offset
end byte offset
```

Binder/type errors refer to the smallest useful available source span.

Source positions are retained beyond parsing long enough to support semantic diagnostics.

## 18.9 Parser architecture

Statements/clauses use handwritten recursive descent.

Expressions use Pratt parsing / precedence climbing.

This keeps precedence, associativity, AST construction, and syntax diagnostics explicit.

No external parser generator defines the language semantics.

## 18.10 Initial statement set

The initial parser/binder surface includes:

```sql
CREATE TABLE
CREATE INDEX
CREATE UNIQUE INDEX
DROP TABLE
DROP INDEX
INSERT
UPDATE
DELETE
SELECT
BEGIN
COMMIT
ROLLBACK
VACUUM
EXPLAIN
EXPLAIN ANALYZE
```

`ALTER TABLE` is deferred initially.

The schema-version/catalog architecture remains compatible with adding it later.

## 18.11 SELECT grammar surface

The baseline SELECT surface is:

```sql
SELECT [DISTINCT] select_list
FROM table_reference [joins...]
[WHERE predicate]
[GROUP BY expressions]
[HAVING predicate]
[ORDER BY expressions [ASC|DESC] ...]
[LIMIT integer]
[OFFSET integer]
```

Initial FROM supports:

```text
base tables
aliases
INNER JOIN
LEFT JOIN
CROSS JOIN
```

Deferred from the initial surface:

```text
RIGHT JOIN
FULL OUTER JOIN
LATERAL
recursive CTEs
window functions
set operations
```

## 18.12 Subquery syntax

Parser/AST structures support subqueries from the beginning.

Initial semantic targets are:

```text
scalar uncorrelated subquery
EXISTS uncorrelated subquery
IN uncorrelated subquery
derived table in FROM
```

Correlated subqueries are deferred.

AST/binding structures MUST NOT make future correlation impossible.

## 18.13 AST contract

The AST records syntax, not resolved semantics.

Representative AST kinds include:

```text
SelectStatement
CreateTableStatement
InsertStatement
UpdateStatement
DeleteStatement
AstIdentifier
AstQualifiedName
AstLiteral
AstBinaryExpression
AstUnaryExpression
AstFunctionCall
AstCast
AstStar
AstSubqueryExpression
```

Textual names remain names in the AST.

They are not replaced with TableId/ColumnId until binding.

## 18.14 AST ownership

AST nodes use statement/query-batch arena ownership.

The architectural properties are stable node addresses for the AST lifetime, cheap bulk destruction, and no need for one general-purpose allocation/free pair per tiny node.

The concrete arena implementation is not part of the persistent or SQL-semantic contract.

## 18.15 Expression precedence

Initial precedence from low to high is:

```text
OR
AND
NOT
comparison / IS NULL / IN
+ -
* / %
unary + -
primary
```

Parentheses override precedence.

Binary arithmetic and Boolean infix operators use conventional left associativity.

Comparisons are non-chainable in v1.

Thus `a < b < c` is rejected rather than assigned accidental host-language semantics.

## 18.16 Front-end invariants

1. Lexer/parser are handwritten in-project components.
2. Token and AST source spans use source-byte positions suitable for diagnostics.
3. SQL keywords are case-insensitive.
4. Unquoted identifiers normalize to lowercase; quoted identifiers preserve spelling/case.
5. String quote escaping uses doubled single quotes in v1.
6. Unary minus remains syntactically distinct from the numeric literal token where practical.
7. Nested block comments are not part of v1.
8. Parser output contains textual syntax names, not resolved catalog IDs.
9. AST nodes retain source spans through semantic binding.
10. Pratt/precedence parsing uses the locked precedence hierarchy.
11. Unsupported syntax fails explicitly rather than being half-interpreted.

---

# 19. Binding and Expression Semantics

## 19.1 Binder role

The Binder converts AST syntax into resolved, typed semantic structures.

It owns:

```text
table lookup
column lookup
alias scopes
ambiguity detection
wildcard expansion
type resolution
implicit cast insertion
aggregate validation
GROUP BY validation
ORDER BY resolution
function/operator resolution
subquery scope creation
DML target resolution
constraint/type validation
```

It does not choose physical access/join algorithms, estimate cardinality, or access heap/B+ pages directly.

## 19.2 Binding scopes and BindingId

Binding uses explicit nested scopes.

A scope contains conceptually:

```text
visible relation bindings
table aliases
output aliases where SQL semantics allow
parent scope link
```

Every visible relation occurrence receives a query-local `BindingId` distinct from `TableId`.

A self-join therefore has the same TableId but different BindingIds.

The parent-scope structure is retained for future correlation, even though correlated subqueries are deferred from the initial target.

A v1 uncorrelated subquery MUST NOT silently capture a parent-column reference.

## 19.3 Bound column references

A bound column expression contains at least:

```text
BindingId
TableId
ColumnId
LogicalType
nullable
source span
```

Logical planning later assigns output-slot identity.

Bound column semantics are never represented solely by the original textual column name.

## 19.4 Column name resolution

### 19.4.1 Unqualified names

Exactly:

```text
0 matches -> unknown-column error
1 match   -> bind that column
>1 match  -> ambiguous-column error
```

Never silently choose the leftmost match.

### 19.4.2 Qualified names

For `u.id`, resolve `u` to one relation binding and `id` to one column in that binding.

Unknown qualifier and unknown column are distinct semantic errors.

## 19.5 Wildcards and output names

`SELECT *` expands during binding into explicit bound column references in visible FROM-relation order and each relation's logical presentation order.

`SELECT u.*` expands one relation binding.

Execution never performs wildcard expansion.

Each SELECT output has:

```text
expression
display name
logical type
nullable
```

Display-name priority is:

1. explicit `AS alias`,
2. simple source column name for a direct column reference,
3. generated expression display name.

Generated display names are presentation metadata, not stable catalog identity.

## 19.6 Bound-expression IR

After binding, expressions are typed semantic nodes.

Initial kinds include:

```text
BoundConstant
BoundColumnRef
BoundUnary
BoundBinary
BoundComparison
BoundBoolean
BoundCast
BoundFunction
BoundAggregate
BoundCase
BoundIsNull
BoundInList
BoundSubquery
```

Every bound expression exposes:

```text
LogicalType return_type
bool nullable
source span
```

The executor receives resolved types and does not redo SQL type inference.

Bound expressions remain semantic nodes but are required to be compatible with later vectorized evaluation conceptually of the form:

```text
Evaluate(input DataChunk, selection) -> output Vector
```

Expression nodes do not own operator scheduling or pipeline state.

Nullability is conservative and semantic. Required examples include:

```text
NOT NULL column ref -> non-nullable
nullable column ref -> nullable
x + y -> nullable if either input nullable
x = y -> nullable if either input nullable
IS NULL(x) -> non-nullable BOOLEAN
COUNT(*) -> non-nullable INT64
SUM(nullable input) -> nullable
```

## 19.7 Expression immutability and ownership

Bound expressions are immutable after construction.

A rewrite creates a new expression or structurally shares immutable children; it does not change the meaning of an expression object that other plans may already reference.

Expression lifetime is query/plan scoped.

Arena ownership or shared immutable nodes are both architecture-compatible.

The design should avoid one independently reference-counted heap allocation per tiny expression unless measurement justifies it.

## 19.8 Operator registry

Arithmetic/comparison resolution is centralized.

Representative signatures include:

```text
+(INT32, INT32)     -> INT32
+(INT64, INT64)     -> INT64
+(FLOAT64, FLOAT64) -> FLOAT64
=(T,T)              -> BOOLEAN
<(T,T)              -> BOOLEAN
```

The Binder first applies TypeResolver coercion/promotion rules, inserts required implicit casts, then selects the final operator implementation/signature.

Operator type rules are not scattered across AST classes.

## 19.9 Function registry and volatility

Functions resolve through descriptors containing at least:

```text
name
argument-type signature / polymorphic rule
return-type rule
volatility class
null-handling rule
implementation ID
```

Volatility classes are:

```text
IMMUTABLE
STABLE
VOLATILE
```

Only IMMUTABLE functions with constant arguments are eligible for ordinary compile/bind-time constant folding.

VOLATILE expressions are never constant-folded as though their result were stable.

Bound function nodes retain the resolved semantic implementation identity rather than requiring executor-time name lookup.

## 19.10 Aggregate expressions

Initial aggregate expressions are:

```text
COUNT(*)
COUNT(expr)
SUM(expr)
MIN(expr)
MAX(expr)
AVG(expr)
```

A bound aggregate is semantically distinct from a scalar function call.

Aggregate calls at one SELECT level are illegal inside that same level's WHERE or JOIN ON.

Binder validates aggregate placement before execution.

## 19.11 Aggregate query semantics

A SELECT is an aggregate query when it contains GROUP BY or aggregate expressions.

For an aggregate query, every SELECT/HAVING expression must be derivable from grouping keys, aggregate results, or constants.

Version 1 uses strict SQL-style grouping validation.

Arbitrary selection of non-grouped columns is rejected.

## 19.12 HAVING

The semantic clause order is:

```text
FROM / JOIN
WHERE
GROUP BY / aggregate
HAVING
SELECT projection
ORDER BY
LIMIT
```

HAVING may reference grouping expressions, aggregate expressions, and expressions derivable from them.

Version 1 does **not** make SELECT aliases visible inside HAVING.

HAVING predicates must type-check as BOOLEAN.

## 19.13 ORDER BY resolution

ORDER BY may refer to:

```text
a bound expression
a SELECT-list alias
a 1-based SELECT-list ordinal
```

For a bare identifier, v1 resolves deterministically:

1. match a SELECT output alias,
2. otherwise bind it as a normal input expression.

An invalid ordinal is a semantic error.

Physical sorting belongs to later planning/execution.

## 19.14 LIMIT and OFFSET

LIMIT/OFFSET expressions must bind to an integral type and be non-negative.

For v1 they must be constant at execution start.

Negative values or values not representable by the runtime limit-count domain are SQL errors.

The logical planner later represents the resolved limit/offset explicitly rather than hiding them in a scan.

## 19.15 DISTINCT

`SELECT DISTINCT` is a semantic duplicate-elimination requirement.

Logical planning MUST represent duplicate elimination explicitly, for example as `LogicalDistinct` or an equivalent aggregate-like logical operator.

It is not merely an opaque projection flag that physical planning cannot reason about.

## 19.16 Searched CASE

Version 1 supports searched CASE.

Every WHEN condition must bind to BOOLEAN.

Evaluation selects the first WHEN whose predicate is TRUE.

FALSE or NULL/UNKNOWN predicates do not select that branch.

Binder finds one common result type for THEN/ELSE branches using TypeResolver coercion rules.

A missing ELSE is semantically `ELSE NULL`.

## 19.17 IN-list semantics

An IN list binds as its own semantic expression node.

Binder resolves a common comparison type and inserts allowed casts.

The exact three-valued result is:

```text
if any comparison is TRUE:
    TRUE
else if the left operand is NULL
     or any list comparison is NULL:
    NULL
else:
    FALSE
```

The architecture does not require rewriting IN lists into OR chains.

Later execution may choose linear comparison, hashing, or sorted lookup according to list size/type semantics.

## 19.18 Subquery binding boundary

The binder supports the initial uncorrelated forms defined by Chapter 18.

A scalar/EXISTS/IN/derived-table subquery receives its own binding scope.

Structures preserve a parent-scope relationship for future correlation, but v1 rejects outer-column capture for forms whose supported semantics are explicitly uncorrelated.

Detailed logical/execution semantics are completed in later chapters.

## 19.19 Parameters

Prepared-statement parameter syntax such as `$1` or `?` is deferred from the initial parser target.

The type system/binder remains compatible with future parameter typing through `UNKNOWN` plus contextual type inference.

## 19.20 Binder/expression invariants

1. Binder output contains resolved IDs, types, nullability, and source spans.
2. Binder does not choose physical access/join algorithms.
3. BindingId is distinct from TableId.
4. Self-joins use distinct BindingIds.
5. Unqualified ambiguous columns are errors.
6. Qualified lookup distinguishes unknown qualifier from unknown column.
7. `SELECT *` is expanded during binding.
8. Bound expressions are immutable.
9. Executor-time SQL name resolution is forbidden.
10. Every bound expression has a resolved return type and conservative nullability.
11. Type coercion/operator/function resolution is centralized.
12. Non-BOOLEAN predicate contexts are rejected.
13. Aggregate/grouping legality is validated before execution.
14. SELECT aliases are not visible in v1 HAVING.
15. ORDER BY alias/ordinal resolution is deterministic.
16. DISTINCT remains an explicit relational semantic requirement.
17. CASE and IN preserve SQL NULL/three-valued behavior.
18. Future parameter typing can reuse UNKNOWN/contextual inference.
---

# 20. Logical Plans, Properties, and Rewrites

## 20.1 Logical-plan node contract

A logical plan is an immutable relational-semantic tree.

Every logical node exposes at least:

```text
operator kind
children
output schema
derived/estimated logical-property placeholder
debug representation
```

Logical operators describe **what relation/result is required**.

They do not encode the physical algorithm used to produce it.

For example:

```text
LogicalGet    != SeqScan
LogicalJoin   != HashJoin
LogicalSort   != one particular sorting algorithm
```

Physical choices belong to later planning.

## 20.2 Logical output schema and `LogicalSlotId`

Every logical output value has one query-local:

```text
LogicalSlotId
```

The canonical name deliberately distinguishes it from the persisted 16-bit heap `SlotId`.

A logical output slot contains metadata such as:

```text
LogicalSlotId
display name
LogicalType
nullable
lineage
system/internal flag
```

`LogicalSlotId` is not:

```text
ColumnId
BindingId
heap SlotId
a permanent physical vector position
```

It identifies one semantic value flowing through a logical plan.

A direct pass-through value SHOULD preserve its `LogicalSlotId` across operators that do not create a new semantic value.

A new computed/projected/aggregate output receives a fresh query-local `LogicalSlotId`.

This distinction is required for self-joins, aliases, duplicate column names, computed expressions, rewrites, and hidden system values.

## 20.3 Initial logical operator family

The initial logical family is:

```text
LogicalGet
LogicalValues
LogicalFilter
LogicalProject
LogicalJoin
LogicalAggregate
LogicalDistinct
LogicalSort
LogicalLimit
LogicalInsert
LogicalUpdate
LogicalDelete
LogicalCreateTable
LogicalCreateIndex
LogicalDrop
LogicalVacuum
LogicalExplain
```

Additional helper nodes are allowed when they represent real relational or statement semantics rather than a physical implementation algorithm.

## 20.4 `LogicalGet`

`LogicalGet` represents access to one logical base relation and contains at least:

```text
TableId
BindingId
required ColumnIds / output LogicalSlotIds
optional logical predicate set after rewrites
immutable table/schema descriptor reference
```

Its meaning is:

```text
produce rows of this logical relation
```

It does not decide sequential versus index access.

A later optimizer may attach/push logical predicates to the Get while preserving the original query semantics.

## 20.5 `LogicalValues`

`LogicalValues` represents a typed literal relation.

It supports cases such as:

```sql
INSERT INTO t VALUES (1, 'a'), (2, 'b');
SELECT 1;
```

Every row contains typed bound expressions.

A constant SELECT without FROM uses one logical empty input row followed by projection rather than a special executor path.

## 20.6 `LogicalFilter`

`LogicalFilter` contains:

```text
child
BOOLEAN predicate
```

Its output schema is identical to its child's schema.

It does not specify whether the predicate will later be vectorized into a selection mask, used to form an index bound, fused with a scan, or evaluated by another physical strategy.

## 20.7 `LogicalProject`

`LogicalProject` contains ordered output expressions:

```text
expr_0 -> output LogicalSlotId
expr_1 -> output LogicalSlotId
...
```

Projection may reorder values, duplicate a source value, calculate expressions, rename outputs, or drop unused input values.

It is the semantic boundary between an internal relation shape and a SELECT output shape.

## 20.8 Logical joins

### 20.8.1 Join node

`LogicalJoin` contains:

```text
join type
left child
right child
bound BOOLEAN condition when applicable
```

V1 join types are:

```text
INNER
LEFT
CROSS
```

CROSS has no join condition.

The node never selects HashJoin/NestedLoop/MergeJoin.

### 20.8.2 Binding scope

For INNER/LEFT JOIN:

```text
1. bind left relation
2. create/bind right relation
3. create a combined ON-expression scope
4. bind ON as BOOLEAN
5. produce joined output bindings
```

The ON expression may reference both sides.

ON and WHERE remain semantically distinct.

### 20.8.3 LEFT JOIN null extension

Every right-side output of a LEFT JOIN becomes nullable in the join output even when the base column is `NOT NULL`.

The logical schema records that nullability change.

### 20.8.4 Predicate decomposition

For INNER/CROSS joins, optimizer metadata may decompose conjunctions into:

```text
equi-join predicates
left-local predicates
right-local predicates
residual predicates
```

The original semantics remain reconstructible/debuggable.

Outer-join decomposition/pushdown remains constrained by null-extension semantics.

## 20.9 `LogicalAggregate` and grouping equality

`LogicalAggregate` contains:

```text
grouping expressions
aggregate expressions
child
```

Its outputs consist only of group-key outputs and aggregate outputs.

Expressions above the aggregate refer to those outputs, not arbitrary pre-aggregate columns.

Grouping and DISTINCT use SQL grouping-equivalence semantics:

```text
NULL values belong to the same group / duplicate class
-0.0 and +0.0 are equivalent
all canonical NaNs are equivalent under v1 FLOAT64 semantics
VARCHAR uses binary byte equality
```

This grouping equivalence is intentionally different from ordinary SQL `=` with NULL, which returns NULL.

Hashing and comparison implementations used later for aggregate/DISTINCT must agree with this contract.

## 20.10 `LogicalDistinct`

`SELECT DISTINCT` is represented as explicit duplicate-elimination semantics using `LogicalDistinct` or an explicitly equivalent aggregate-like node.

DISTINCT is not hidden as an opaque projection flag.

The optimizer and physical planner must be able to reason about its cost, keys, ordering opportunities, and memory requirements.

## 20.11 `LogicalSort`

`LogicalSort` contains ordered sort keys:

```text
expression / LogicalSlotId
ASC | DESC
resolved NULL order
```

V1 default ordering is:

```text
ASC  -> NULLS FIRST
DESC -> NULLS LAST
```

This matches the existing ascending NULL-first B+ key order and its natural reverse.

If an explicit `NULLS FIRST/LAST` SQL surface is added later, the resolved choice is still stored directly in the logical sort key.

Physical sorting/index-order matching never guesses NULL ordering.

## 20.12 `LogicalLimit`

`LogicalLimit` contains:

```text
optional limit
offset
```

Both have already passed the binder's nonnegative integral / execution-start-constant checks.

The optimizer may later use safe LIMIT pushdown or Top-N alternatives without changing the logical limit semantics.

## 20.13 DML logical nodes and hidden system slots

### 20.13.1 Hidden system values

Logical plans may carry internal values such as:

```text
target physical RID
```

Such slots are marked `system/internal`.

They never appear through `SELECT *` or ordinary user-visible column enumeration.

Projection pruning MUST preserve internal slots required by downstream DML.

### 20.13.2 `LogicalInsert`

Contains:

```text
target TableId
canonical target ColumnIds
input relation
column coercion/default expressions
optional RETURNING projection
```

Input may be VALUES or SELECT.

Both use the same logical insert contract.

### 20.13.3 `LogicalUpdate`

Contains:

```text
target TableId
target BindingId
child producing target RID + required old values
assignment expressions keyed by ColumnId
optional RETURNING
```

The child must preserve the exact target RID needed by Chapter 15's write protocol.

The planner requests old values required to construct the complete new physical tuple version and to evaluate assignments/RETURNING.

### 20.13.4 `LogicalDelete`

Contains:

```text
target TableId
child producing target RID
required old values
optional RETURNING
```

V1 DELETE targets one base table.

The child preserves exact row identity through its supported filters/joins.

Physical index cleanup still belongs to vacuum, not LogicalDelete.

## 20.14 Subquery logical semantics

Every subquery has its own bound scope and later its own logical subplan.

A future correlated reference is represented distinctly, conceptually:

```text
BoundCorrelatedColumnRef(
    depth,
    BindingId,
    ColumnId,
    type,
    nullable
)
```

so correlation is never confused with a local column reference.

V1 may detect such a correlated reference and reject execution because correlated subqueries remain deferred.

### 20.14.1 Scalar subquery

A scalar subquery must expose exactly one output column.

Runtime cardinality semantics are:

```text
0 rows -> NULL
1 row  -> that value
>1 row -> SQL cardinality error
```

### 20.14.2 EXISTS

`EXISTS(subquery)` returns non-null BOOLEAN:

```text
TRUE  if the subquery produces at least one row
FALSE otherwise
```

Its semantic node remains EXISTS; it is not rewritten prematurely into arbitrary projected subquery values.

### 20.14.3 IN subquery

`x IN (subquery)` requires exactly one compatible subquery output column and preserves SQL NULL semantics.

Later optimization may implement the semantics with a semi-join, mark join, hash set, or another proven equivalent.

### 20.14.4 CTEs

Non-recursive CTEs are deferred.

When added, they are logical named subplans rather than mandatory materialization barriers.

Recursive CTEs remain later work.

## 20.15 Canonical SELECT logical shape

Before optimization, SELECT planning uses the canonical shape:

```text
FROM
    ↓
JOINs
    ↓
WHERE Filter
    ↓
Aggregate, if required
    ↓
HAVING Filter
    ↓
Projection
    ↓
Distinct
    ↓
Sort
    ↓
Limit/Offset
```

The optimizer may transform this only when it proves semantic equivalence.

SELECT without FROM uses:

```text
LogicalValues(single empty row)
    ↓
LogicalProject
```

## 20.16 Logical properties

Logical nodes expose or permit derivation of properties such as:

```text
output LogicalSlotIds
nullability
candidate keys
known constants
lineage
semantic ordering requirements
estimated-cardinality placeholder
```

Constraint-derived examples include:

```text
PRIMARY KEY           -> candidate key
UNIQUE + NOT NULL     -> candidate key
NOT NULL              -> non-nullable base output
LEFT JOIN right side  -> nullable output
```

Such properties may support later optimization, but a rewrite based on them is not valid until its semantic proof/test exists.

Logical properties do not substitute for the later physical-property system.

## 20.17 Logical rewrites

Logical rewrites occur after binding and before cost-based physical selection.

### 20.17.1 Constant folding

Fold a subtree only when it is composed entirely of constants and uses IMMUTABLE operators/functions whose semantics are known.

NULL and three-valued logic are preserved.

VOLATILE functions are never folded.

STABLE functions are not treated as immutable compile-time constants.

### 20.17.2 Boolean simplification

Safe identities include:

```text
x AND TRUE -> x
x OR FALSE -> x
NOT NOT x  -> x
```

Rules are expressed in SQL three-valued logic rather than imported from two-valued Boolean algebra.

A rewrite MUST NOT duplicate, drop, or reorder a VOLATILE expression.

The baseline optimizer is conservative around expressions whose evaluation may raise a SQL error; it does not use Boolean algebra merely to hide or newly force such evaluation.

### 20.17.3 Predicate pushdown

Predicates may be pushed toward base relations when the referenced slots and join semantics permit.

For INNER/CROSS joins, a predicate referencing only one side may be pushed to that side.

LEFT JOIN uses stricter null-preservation rules.

V1 does not apply an inner-join pushdown rule across a LEFT JOIN unless an explicit outer-join transformation proves equivalence.

### 20.17.4 Projection pruning

Projection pruning computes values required by ancestors and removes unused outputs.

It MUST retain every value still required by:

```text
filters
join predicates
group keys
aggregate arguments
sort keys
RETURNING
DML assignments
hidden target RID/system slots
```

### 20.17.5 Expression canonicalization

Safe canonicalization may include:

```text
flattening AND chains
canonical operand ordering for truly commutative operators
placing constants on one side of comparisons where operator reversal is exact
```

Canonicalization does not change NULL semantics, FLOAT64 semantics, or volatility/evaluation-count semantics.

Source/display metadata may be retained separately for readable EXPLAIN output.

### 20.17.6 Join graph extraction

INNER/CROSS joins may be summarized as a join graph:

```text
relation occurrences (BindingIds) -> vertices
join predicates                    -> edges
local predicates                   -> vertex metadata
```

Outer joins impose semantic ordering constraints and are not freely inserted into an ordinary inner-join graph.

## 20.18 Logical-plan validation

Validation runs after initial logical planning and after major rewrite phases.

At minimum it checks:

```text
every referenced LogicalSlotId exists in the required child output
output slot IDs are unique where required
expression return types are resolved
filter/HAVING/join predicates are BOOLEAN
aggregate references are legal
node output schemas agree with expressions
LEFT JOIN right-side nullability is preserved
hidden DML RID/system slots survive when required
catalog descriptor/schema-version references are internally consistent
```

Malformed logical plans are architecture errors detected before execution, not conditions left for executor crashes.

## 20.19 Logical EXPLAIN

Logical plans have an AST-independent debug representation.

Before physical planning exists, EXPLAIN can print a tree such as:

```text
Limit 10
  Sort [orders.amount DESC NULLS LAST]
    Project [users.name, orders.amount]
      Filter [orders.amount > 100]
        InnerJoin [users.id = orders.user_id]
          Get users
          Get orders
```

Debug detail may include `LogicalSlotId`, `TableId`, `BindingId`, types, nullability, and lineage.

The logical EXPLAIN representation does not depend on reparsing or pretty-printing the original AST.

## 20.20 Logical-plan/rewrite invariants

1. Logical operators encode relational/statement semantics, not physical algorithms.
2. `LogicalSlotId` is distinct from ColumnId, BindingId, heap SlotId, and physical vector position.
3. Logical nodes and bound expressions are immutable once published.
4. LEFT JOIN null extension is represented in output nullability.
5. ON and WHERE remain semantically distinct.
6. Aggregate outputs contain only grouping/aggregate-derived values.
7. GROUP BY and DISTINCT use SQL grouping equivalence, including one NULL group.
8. Sort keys always carry resolved ASC/DESC and NULL order.
9. Hidden DML RID slots are not user-visible and are preserved while required.
10. SELECT planning begins from one deterministic canonical logical shape.
11. Rewrites preserve NULL, FLOAT64, volatility, and outer-join semantics.
12. Correlated references are represented distinctly even when their execution is deferred.
13. Logical validation occurs before execution/physical planning consumes a plan.
14. EXPLAIN consumes the bound/logical representation rather than AST syntax alone.

---

# 21. DDL/DML Semantic Planning and SQL v1 Scope

## 21.1 Scope

This chapter owns the upper semantic integration for schema-changing DDL, transactional catalog visibility, catalog-object identity publication, offline index construction, DROP/object retirement, DML binding/planning, default semantics, RETURNING, and SQL-v1 scope.

Storage mutation ordering remains Chapter 15's responsibility.

Logical operator semantics remain Chapter 20's responsibility.

## 21.2 Conservative DDL concurrency model

V1 serializes schema-changing DDL with one database-wide:

```text
SchemaLock
```

or equivalent exclusive catalog-DDL mutex.

A transaction that performs schema-changing DDL holds this exclusivity through its terminal publication/abort boundary.

Ordinary read-only queries do not hold this lock for their execution lifetime.

### 21.2.1 Target-table writer gate

DDL that must observe or retire a stable physical table state additionally uses a per-table writer gate.

DML that performs persistent writes to table `T` acquires:

```text
TableWriterGate(T) shared
```

before the first persistent write to `T` and holds it until transaction end.

CREATE INDEX / DROP TABLE on `T` acquire:

```text
TableWriterGate(T) exclusive
```

and wait for existing table writers to reach terminal outcome before proceeding.

Reads do not require this writer gate.

The gate is a DDL coordination mechanism, not a replacement for tuple-write/unique-key locking.

No SchemaLock/TableWriterGate wait occurs while holding heap/B+ page latches.

DDL lock order is:

```text
SchemaLock
    -> TableWriterGate exclusive when required
    -> lower logical/page locks
```

V1 does not support lock upgrade from a transaction's already-held shared TableWriterGate to exclusive DDL ownership. If an explicit transaction has already performed persistent DML on the target table, a later schema-changing DDL statement requiring that exclusive gate is rejected.

This deliberately coarse baseline prevents an offline index build from missing concurrent writes and avoids an implicit lock-upgrade deadlock protocol.

## 21.3 Catalog visibility during binding

Catalog rows participate in normal MVCC.

Binding uses the transaction's normal catalog snapshot:

```text
READ COMMITTED:
    statement snapshot

REPEATABLE READ:
    transaction snapshot
```

Therefore uncommitted DDL from another transaction is invisible, a long-running REPEATABLE READ transaction does not suddenly resolve a newly committed object, and the current transaction may observe its own completed earlier DDL statement through ordinary self/CommandId visibility.

The catalog cache cannot override this rule.

A cached descriptor is usable only if it is visible to the binding snapshot.

## 21.4 Durable catalog-object IDs

R-037 is resolved by the allocator in §13.2.6.

V1 uses one durable uint64 catalog-object ID namespace for:

```text
TableId
IndexId
ConstraintId
```

Allocation occurs during DDL execution, not during syntax parsing or side-effect-free binding.

Binding may create an immutable/provisional descriptor specification, but it does not perform durable identity allocation or file creation merely to resolve a statement.

The allocation sequence is:

```text
acquire relevant DDL exclusivity
durably reserve/advance next_catalog_object_id
receive object ID
only then place that ID in persistent catalog/file state
```

Crash gaps and IDs consumed by aborted DDL are acceptable.

Reuse is forbidden.

ColumnIds follow the table-local rule in §16.3.

## 21.5 DDL private physical resources

A physical heap/FSM/B+ file created by uncommitted DDL is **private/unpublished**.

Other transactions cannot discover it through committed catalog metadata.

If CREATE DDL aborts:

```text
catalog rows become ABORTED/invisible
allocated object IDs/FileIds remain consumed
private physical files become orphan-retirement candidates
```

V1 need not physically undo their bytes.

A private file may be unlinked after no live in-process owner can reference it.

After crash recovery, startup/catalog maintenance may reconcile managed object files against committed catalog ownership and remove files that are provably unpublished/orphaned.

An object file is never considered committed merely because it exists on disk.

## 21.6 CREATE TABLE

### 21.6.1 Binding

Binder validates:

```text
table name is not already visible in main
column names are unique
types are supported
PRIMARY KEY shape
UNIQUE constraints
NOT NULL constraints
default expressions
```

Initial table constraints are PRIMARY KEY, UNIQUE, and NOT NULL.

Foreign keys and CHECK constraints are deferred.

The bound statement contains a schema/constraint specification without allocating persistent object/File IDs.

### 21.6.2 Execution/publication

Under SchemaLock:

```text
1. revalidate table-name availability against current committed catalog state
2. allocate TableId
3. allocate heap FileId and FSM FileId
4. create/initialize private heap/FSM files
5. assign initial ColumnIds 1..N
6. install transaction-owned MVCC catalog rows/descriptors
7. build any required primary/unique index objects through their DDL protocol
8. finish the DDL statement while retaining DDL exclusivity until the owning transaction is terminal
9. if/when the owning transaction reaches terminal COMMITTED publication:
       publish new committed descriptor-cache/name entries
10. on ABORT:
       keep catalog rows invisible and retire private files later
11. release DDL exclusivity only at that terminal boundary
```

Initial tuple schema version is `1`.

If the transaction aborts, its catalog rows remain invisible and created files become orphan-retirement candidates.

In autocommit mode the statement-owning transaction normally reaches COMMIT immediately after successful statement completion. In an explicit transaction, the object remains transaction-local until the later user COMMIT and the conservative SchemaLock remains held for that interval.

## 21.7 PRIMARY KEY

A PRIMARY KEY means:

```text
UNIQUE
+
NOT NULL for every key column
```

The catalog records the semantic primary-key constraint as its own `ConstraintId`.

V1 also creates a unique B+ index implementing key lookup/enforcement.

The physical index and semantic primary-key constraint remain distinct catalog objects.

## 21.8 CREATE INDEX

### 21.8.1 Binding

Initial index keys reference base table columns only.

Expression indexes are deferred.

Binder resolves:

```text
target TableId
ordered key ColumnIds
uniqueness
index name
```

and creates an immutable index specification.

Persistent IndexId/FileId allocation occurs only during execution.

A unique index uses Chapter 11's transactional uniqueness semantics.

### 21.8.2 Offline build protocol

V1 builds indexes offline with conservative writer exclusion:

```text
1. acquire SchemaLock
2. acquire target TableWriterGate exclusive
3. wait until pre-existing target-table writers are terminal
4. revalidate table/index-name/catalog state
5. allocate IndexId and B+ FileId
6. create one private empty B+ tree
7. scan the target table's current logically live committed row versions
8. encode keys using the table/index descriptors
9. insert every physical (key,RID) through ordinary B+ MTRs
10. validate uniqueness when requested
11. install transaction-owned catalog index rows/constraint links
12. finish the DDL statement while retaining target-writer/DDL exclusivity
13. if/when the owning transaction reaches terminal COMMITTED publication:
        publish the index descriptor/name entry
14. on ABORT:
        keep catalog rows invisible and retire the private index file later
15. release target writer gate and SchemaLock only at the terminal boundary
```

Step 7 is a DDL maintenance/current-state scan after target writers have drained; it is not allowed to omit rows merely because the DDL transaction has an older REPEATABLE READ snapshot.

Readers may continue while the index is built because the private index is not visible to their catalog snapshots.

New target-table writers are blocked until the index commit/abort boundary, preventing missing entries.

A half-built index is never visible to planning.

If build/transaction aborts, the private index file becomes an orphan-retirement candidate.

## 21.9 DROP and physical object retirement

DROP TABLE / DROP INDEX is transactional at catalog visibility.

Under DDL exclusivity:

```text
mark the relevant catalog rows deleted by the DDL transaction
do not unlink physical files immediately
commit/abort normally
```

On abort, the deletion `xmax` is ineffective and the object remains.

On commit, new catalog snapshots no longer resolve the object, while older snapshots/descriptors may still legally reference it.

Physical file retirement therefore waits until:

```text
the dropping catalog row/version is globally dead to registered SQL snapshots
AND
no immutable descriptor/query handle can reference the physical object
AND
no BufferPool/page/file owner still requires it
```

Only then may the managed file be unlinked.

This retirement rule applies to table heap/FSM files and index B+ files.

`DROP TABLE` also retires dependent indexes/constraints as one DDL semantic operation.

## 21.10 Catalog cache publication

Catalog cache publication follows transaction terminal state.

CREATE/ALTER-like descriptor versions remain transaction-local/catalog-MVCC-visible until terminal COMMITTED publication, after which current committed cache/name lookup may publish them.

After committed DROP, current-name lookup stops returning the object while old immutable descriptor objects may remain alive for older users.

Cache invalidation/replacement does not mutate descriptors already held by active plans.

## 21.11 INSERT binding

For:

```sql
INSERT INTO t(a,c) VALUES (...);
```

Binder resolves the target table, verifies target columns are unique, maps omitted columns, binds input expressions, inserts allowed implicit casts, fills catalog defaults or typed NULL where legal, rejects statically impossible NOT NULL cases, and produces one canonical full target-column order.

Execution never resolves target column names again.

`INSERT ... SELECT` uses the same canonical target-column contract after binding the source relation.

## 21.12 Default expressions

V1 column defaults may be:

```text
constant
immutable scalar expression
```

and are evaluated per inserted row as required.

Volatile defaults such as sequences are deferred.

A default is stored as a typed, serializable semantic catalog expression, not raw SQL text alone.

Original SQL text may additionally be retained for display.

The exact versioned persistent encoding of nontrivial default-expression trees/function/operator identities is not byte-exact in the legacy source and remains R-040.

## 21.13 UPDATE binding/planning

Binder validates target table, assignment column names, no duplicate target assignments, assignment expression types, and a BOOLEAN WHERE predicate.

Each assignment becomes:

```text
ColumnId -> typed bound expression
```

Unmentioned columns retain their old values.

Logical planning requests target RID, old values needed by assignment expressions, old values needed to construct the complete new physical tuple, and values needed by RETURNING.

The resulting `LogicalUpdate` feeds Chapter 15's update protocol.

## 21.14 DELETE binding/planning

Binder resolves the target table and BOOLEAN WHERE predicate.

The logical child produces target RID plus old values needed by RETURNING or semantic evaluation.

DELETE does not physically erase index entries.

Vacuum remains the index-garbage owner.

V1 DELETE targets one base table.

## 21.15 RETURNING

The logical DML structures support:

```sql
INSERT ... RETURNING ...
UPDATE ... RETURNING ...
DELETE ... RETURNING ...
```

RETURNING expressions bind against the DML-defined row image:

```text
INSERT -> inserted/new row
UPDATE -> updated/new row
DELETE -> deleted/old row
```

The exact physical buffering mechanism is defined by the execution stage.

Chapter 15's retry rule still applies: before persistent statement writes READ COMMITTED may restart internally; after persistent writes the transaction aborts rather than replaying the same attempt under the same TxnId.

No externally visible RETURNING row is emitted from an attempt that may still restart.

## 21.16 Error contract for semantic planning

Front-end semantic failures are classified rather than collapsed.

Important categories include:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature
CardinalityError
```

Where source syntax exists, the error carries the smallest useful source span and a human-readable message.

Storage/transaction errors retain their lower-layer category and may later be mapped to SQLSTATE-like surface codes.

## 21.17 Parser error recovery

For one statement, the initial parser may stop at the first syntax error.

For a multi-statement input batch it may synchronize at semicolon or end-of-input and continue reporting later independent statement errors where practical.

IDE-grade error recovery is not required.

## 21.18 SQL v1 supported target

The intended first serious SQL surface includes:

```text
CREATE TABLE
CREATE INDEX / CREATE UNIQUE INDEX
DROP TABLE / DROP INDEX
INSERT
UPDATE
DELETE
SELECT
WHERE
INNER JOIN
LEFT JOIN
CROSS JOIN
GROUP BY
HAVING
ORDER BY
DISTINCT
LIMIT / OFFSET
BEGIN
COMMIT
ROLLBACK
VACUUM
EXPLAIN
EXPLAIN ANALYZE
```

The front end remains architecturally capable of the initial uncorrelated scalar/EXISTS/IN/derived-table subquery forms.

The purpose of this surface is to exercise the real relational engine, not to claim broad SQL-standard compatibility.

## 21.19 Explicitly deferred SQL/front-end features

V1 deliberately defers:

```text
ALTER TABLE
foreign keys
CHECK constraints
views / materialized views
triggers
stored procedures
recursive CTEs
window functions
RIGHT/FULL OUTER JOIN
NATURAL/USING JOIN
LATERAL
MERGE
UPSERT / ON CONFLICT
generated columns
sequences / identity columns
expression indexes
partial indexes
collation framework
time zones
DECIMAL / NUMERIC
INTERVAL
JSON
arrays
user-defined types
prepared-statement parameters
SQL privileges / roles
correlated-subquery execution/decorrelation
```

These are future architecture-compatible features, not hidden requirements of the initial engine.

## 21.20 Upper semantic-layer invariants

1. Parser output contains syntax names; binder output contains resolved IDs/types.
2. Executor does not perform SQL name resolution.
3. Logical operators never encode physical algorithms.
4. ColumnId, BindingId, LogicalSlotId, heap SlotId, and physical vector position are distinct.
5. Self-joins use distinct BindingIds.
6. All bound expressions have resolved type/nullability before logical planning.
7. Non-BOOLEAN WHERE/HAVING/ON predicates are rejected.
8. Numeric implicit casts widen and do not silently narrow.
9. `SELECT *` is expanded before logical planning.
10. Aggregate/grouping legality is validated before execution.
11. LEFT JOIN right-side outputs are nullable.
12. Hidden DML target RID slots survive until the DML operator consumes them.
13. Catalog visibility follows the caller's MVCC snapshot; cache hits cannot bypass it.
14. Uncommitted DDL is never globally published as committed metadata.
15. Schema-changing DDL is conservatively serialized in v1.
16. Offline CREATE INDEX blocks target-table writers so the published index is complete.
17. CREATE abort may leave physical garbage but never a visible half-created object.
18. DROP file unlink waits until old catalog snapshots/descriptors can no longer reference the object.
19. Catalog-object IDs/FileIds that may have entered persistent state are never reused.
20. Rewrites preserve NULL, volatility, outer-join, grouping, and hidden-slot semantics.
21. Logical-plan validation detects broken slot/schema references before execution.
22. Unsupported SQL fails explicitly rather than being partially reinterpreted.
---

# Part VI — Physical Execution

# 22. Physical Plan and Runtime Operator Model

## 22.1 Execution architecture

The production execution engine is:

```text
vectorized
chunk-at-a-time
pipeline-oriented
memory-budgeted
spill-capable
parallel-ready
```

It consumes resolved physical plans and interacts with:

```text
transactional heap access
B+ tree access
immutable catalog descriptors
QueryMemoryManager
SpillManager
```

The execution layer MUST NOT perform:

```text
SQL parsing
SQL name resolution
catalog-name lookup in hot loops
cost-based search
```

Its performance purpose is to amortize branches, dispatch, synchronization, allocation, and tuple decoding across batches instead of paying those costs once per row.

A row-at-a-time reference executor MAY exist for differential/correctness testing, but it is not the production execution architecture.

## 22.2 Immutable physical plan versus execution state

The architecture distinguishes:

```text
PhysicalOperator
    immutable plan description

GlobalOperatorState
    mutable state shared by workers for one execution

LocalOperatorState
    mutable state owned by one worker/task
```

A physical plan may later be cached or reused.

Runtime state is created per execution.

Transaction, snapshot, read-epoch registration, cancellation state, memory usage, cursors, and other execution-specific mutable state MUST NOT live directly inside an immutable physical-plan node.

## 22.3 Physical operator contract

Every physical operator describes at least:

```text
operator kind
children
output schema / LogicalSlotIds
required child inputs
physical properties
estimated row/cost placeholders
EXPLAIN metadata
```

Physical operators encode a selected execution algorithm.

For example:

```text
LogicalGet
    -> PhysicalSeqScan
    -> PhysicalIndexScan
```

but execution methods/state are not SQL AST methods and do not perform binding again.

The detailed optimizer rules that choose one physical operator are owned by the optimizer chapters.

## 22.4 Initial physical operator family

The execution architecture includes:

```text
PhysicalSeqScan
PhysicalIndexScan
PhysicalValues

PhysicalFilter
PhysicalProject

PhysicalNestedLoopJoin
PhysicalHashJoin
PhysicalIndexNestedLoopJoin
PhysicalMergeJoin

PhysicalHashAggregate
PhysicalSortAggregate
PhysicalDistinct

PhysicalSort
PhysicalTopN
PhysicalLimit

PhysicalInsert
PhysicalUpdate
PhysicalDelete

PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
PhysicalVacuum

PhysicalExplain
PhysicalResultSink
```

An operator is added when it represents a distinct physical execution algorithm or statement execution role.

The detailed join/aggregate/sort/DML implementations are owned by Pass 13.

## 22.5 Query execution context

Each execution owns one:

```text
QueryExecutionContext
```

containing or referencing at least:

```text
Transaction
effective statement/transaction Snapshot
ReadEpochGuard
QueryMemoryManager
SpillManager
query cancellation state
pipeline-local early-stop state
profiling state
query-scoped arena
```

The `ReadEpochGuard` is held for the period in which execution may retain or dereference index-derived RIDs, satisfying Chapter 14's physical-RID reuse contract.

For READ COMMITTED, the effective snapshot is the statement snapshot.

For REPEATABLE READ, it is the transaction snapshot with the current command boundary.

The execution context does not weaken transaction-owned snapshot/lock lifetimes defined by Chapters 9–15.

## 22.6 Runtime ownership

The immutable plan owns semantic/physical configuration such as:

```text
resolved IDs
resolved types
physical expressions
bounds
operator properties
```

Runtime state owns mutable work such as:

```text
source cursors
reusable DataChunks
expression scratch
row collections
memory reservations
spill runs
counters
continuation state
```

No runtime pointer may be persisted or placed into a catalog/storage format.

## 22.7 Foundation invariants

1. Production execution is vectorized/chunk-at-a-time.
2. Physical plans are immutable after publication.
3. Per-execution mutable state is separate from the plan.
4. Global and local worker state are distinct from day one.
5. Execution consumes resolved IDs/types/slots and never performs SQL name resolution.
6. Transaction/snapshot/read-epoch state is execution context, not plan state.
7. A physical operator represents an execution algorithm, not SQL syntax.
8. Runtime state is query-lifetime/process-local and never persistent format.

---

# 23. Vectorized Data and String Representation

## 23.1 Standard vector size and DataChunk

The basic execution batch is:

```text
DataChunk {
    capacity
    cardinality
    Vector columns[]
    chunk-local StringHeap
}
```

The standard v1 vector capacity is:

```text
STANDARD_VECTOR_SIZE = 1024
```

The constant is centralized and benchmark-configurable; operator code MUST NOT scatter literal `1024`.

A v1 DataChunk capacity MUST satisfy:

```text
capacity <= 65535
```

so a uint16 SelectionVector can address every physical position.

Every normal column in one chunk has the same logical cardinality, with:

```text
0 <= cardinality <= capacity
```

A final stream chunk may be partially filled.

`cardinality = 0` is an empty batch, **not** an end-of-stream marker.

Source completion is represented explicitly by runtime status.

## 23.2 Why 1024

The standard size is large enough to amortize dispatch/selection/buffer/iterator overhead while keeping common vectors cache-friendly.

Representative sizes are:

```text
1024 INT64 values           8 KiB
1024 INT32 values           4 KiB
1024 uint16 selection       2 KiB
1024 validity bits          128 B
```

The chosen size is an execution tuning constant, not a persistent-format value.

## 23.3 Vector kinds

The initial vector representations are:

```text
FLAT
CONSTANT
DICTIONARY
```

### FLAT

Stores/references contiguous typed values for physical positions.

### CONSTANT

Represents one scalar value repeated for all logical rows.

The scalar may itself be NULL.

### DICTIONARY

References another Vector plus one SelectionVector.

It does not own an independent value array.

The logical row `i` maps to the child physical position selected by the dictionary.

Later representations such as:

```text
SEQUENCE
RLE
```

are deferred until measurement justifies them.

## 23.4 Flat fixed-width vectors

For fixed-width types, a FLAT vector uses:

```text
naturally aligned contiguous data buffer
+
validity state
```

Physical execution elements are:

```text
BOOLEAN    uint8
INT32      int32
INT64      int64
FLOAT64    binary64/double
DATE       int32
TIMESTAMP  int64
```

BOOLEAN deliberately uses one byte per execution value.

It is not bit-packed in the initial executor.

This choice is independent of persistent storage representation.

## 23.5 Validity mask

Validity uses one bit per physical value position:

```text
1 = valid / non-NULL
0 = NULL
```

Words are `uint64`.

For a standard 1024-row vector:

```text
16 words
= 128 bytes
```

A vector carries `all_valid` as a fast path.

When `all_valid = true`, kernels do not need to read validity words.

Bits outside the vector's active physical capacity/cardinality are semantically irrelevant and MUST NOT influence results.

For a CONSTANT vector, one scalar validity state is logically repeated.

For a DICTIONARY vector, effective validity is the referenced child value's validity after selection composition.

## 23.6 SelectionVector

A standard SelectionVector stores:

```text
uint16 indices[capacity]
```

A selection maps logical active position `i` to an underlying physical position `indices[i]`.

Every selected index MUST be within the referenced vector's physical bounds.

Common uses include:

```text
filter selections
dictionary vectors
join match selections
chunk slicing
```

Identity selection is represented as metadata/view state rather than materializing `0,1,2,3,...` for every batch.

## 23.7 Dictionary composition

Repeated filtering/projection may otherwise produce:

```text
dictionary -> dictionary -> dictionary -> ...
```

V1 does not permit unbounded lookup chains in hot kernels.

When composing:

```text
outer_sel[i] -> inner logical row
inner_sel[...] -> base physical row
```

the effective selection is flattened to the resulting base physical index.

Normalization occurs before a representation reaches a hot vector kernel when chain depth would otherwise exceed one effective indirection.

Dictionary normalization preserves:

```text
logical row order
effective validity
underlying value identity
```

without copying value payloads solely to remove indirection.

## 23.8 UnifiedVectorFormat

Vector kernels consume a normalized view conceptually:

```text
UnifiedVectorFormat {
    typed/base data pointer
    effective SelectionView
    effective ValidityView
}
```

Representation dispatch happens once per input vector/batch.

A kernel therefore does not branch per row on:

```text
FLAT vs CONSTANT vs DICTIONARY
```

For CONSTANT input, the effective selection maps every logical row to scalar position zero.

For DICTIONARY input, nested selections are composed into one effective selection before the hot loop.

UnifiedVectorFormat is an adapter/view; it does not become a persistent materialization format.

## 23.9 VARCHAR `StringRef`

The in-memory VARCHAR scalar reference is:

```text
StringRef {
    uint32 length
    uint32 prefix
    const char* data
}
```

and is naturally 16 bytes on a 64-bit process.

`length` is the exact byte length.

No trailing NUL is required.

For `length = 0`, `data` need not be dereferenced.

### 23.9.1 Prefix definition

`prefix` caches the first up to four string bytes as one unsigned big-endian 32-bit value:

```text
b0 b1 b2 b3
->
(b0 << 24) | (b1 << 16) | (b2 << 8) | b3
```

Missing bytes are zero-filled.

Examples:

```text
""      -> 0x00000000
"a"     -> 0x61000000
"ab"    -> 0x61620000
"abcd"  -> 0x61626364
```

Bytes are interpreted unsigned.

A differing prefix may be used as a fast binary-order/equality rejection.

An equal prefix is never sufficient proof of full-string equality/order; length/full byte comparison resolves the remaining case.

This agrees with Chapter 17's binary VARCHAR collation.

## 23.10 String ownership

A StringRef is valid only while its byte owner remains alive.

Valid owners include:

```text
DataChunk StringHeap
query arena / stable query-owned constant storage
RowCollection/row blocks
blocking-operator build storage
sort/run storage
```

A StringRef MUST NOT outlive:

```text
an unpinned heap page
a reset/reused source chunk
temporary expression scratch
a spilled/in-memory block whose storage was released
```

Any operator retaining a VARCHAR beyond the current input-consumption lifetime deep-copies the bytes into its own query/operator-owned storage.

## 23.11 DataChunk StringHeap

Every reusable DataChunk owns chunk-local variable-length storage.

Heap scan decoding follows:

```text
copy VARCHAR bytes from tuple/page
    ↓
chunk StringHeap
    ↓
StringRef in output VARCHAR vector
```

This prevents downstream execution from extending heap-page pin lifetime merely to keep a string pointer alive.

The StringHeap may be reset only when no downstream borrowed vector/chunk still references it.

## 23.12 Borrowed vectors

Streaming operators may return reference/dictionary vectors borrowing input storage when the pipeline guarantees synchronous downstream consumption before the owner is reset.

Typical cases:

```text
PhysicalFilter
simple column PhysicalProject
```

A blocking operator deep-copies retained data.

A result/client boundary also materializes or otherwise safely retains its memory before asynchronous/client-visible lifetime can exceed the producer chunk.

The pipeline executor owns this lifetime guarantee.

## 23.13 Chunk/vector reuse

Operators reuse DataChunk/vector buffers instead of allocating/freeing them for each batch.

A reusable output chunk may be reset only after its previous contents are no longer borrowed.

Reset conceptually:

```text
cardinality = 0
clear/reinitialize vector logical state
reset chunk StringHeap
preserve reusable allocated capacity
```

Large reusable buffers remain memory-accounted through the query memory contract where applicable.

## 23.14 Vector/string invariants

1. Every DataChunk column has one common logical cardinality.
2. Empty chunk is not end-of-stream.
3. V1 standard selection indices are uint16 and chunk capacity never exceeds 65535.
4. Validity bit `1` means non-NULL and `0` means NULL.
5. Vector representation does not change SQL NULL semantics.
6. Dictionary chains are normalized to bounded effective indirection.
7. UnifiedVectorFormat performs representation dispatch per batch, not per row.
8. BOOLEAN execution values are byte-per-value in v1.
9. StringRef length is byte length and requires no terminator.
10. StringRef prefix has the exact big-endian first-four-byte meaning in §23.9.1.
11. A borrowed vector never outlives its owner.
12. A chunk never exposes VARCHAR pointers into an unpinned heap page.
13. Blocking/retaining boundaries own/deep-copy varlen data.
14. Chunk reuse never resets storage that remains borrowed.

---

# 24. Query Memory, Row Storage, and Spill

## 24.1 Execution RowLayout

Blocking operators use a query-temporary row layout distinct from:

```text
persistent heap tuple layout
columnar DataChunk layout
```

A `RowLayout` is derived from resolved logical types.

It contains conceptually:

```text
null bitmap
fixed-width values
varlen offset/length descriptors
variable payload in row/block-owned storage
operator-owned optional metadata
```

Examples of operator-owned metadata include hash, duplicate-chain link, and partition/run identifier.

A temporary row never carries persistent heap MVCC headers merely for query execution.

Variable-length descriptors use offsets/lengths into row/block-owned storage rather than assuming a stable raw pointer inside a movable/serializable row image.

## 24.2 RowCollection

`RowCollection` stores temporary rows in append-oriented blocks.

The initial target block size is:

```text
256 KiB
```

and is configuration/tuning state, not persistent format.

Properties are:

```text
append-oriented
stable in-memory row handles while the collection/block lives
bulk deallocation
minimal per-row allocation
associated varlen blocks where required
```

A row handle is query-local.

It is never a persistent RID.

If data is spilled and later reconstructed, the spill representation/handle is separate from an in-memory row handle.

## 24.3 QueryArena

Each query owns an arena for small query-lifetime objects such as:

```text
expression state
operator/local-state objects
temporary key descriptors
small metadata arrays
pipeline graph nodes
```

Allocation is bump-oriented with bulk release at query end.

The arena is **not** the storage mechanism for unbounded hash tables, large row collections, sort runs, large vector buffers, or DML/result spools.

Arena backing pages are obtained/accounted as non-spillable query memory so the arena cannot bypass the query memory budget merely because its individual allocations are small.

## 24.4 QueryMemoryManager

`QueryMemoryManager` owns execution-memory accounting across:

```text
one query
the database/global execution budget
operator reservations
spill pressure
hard-limit enforcement
```

Configuration exposes conceptually:

```text
global execution-memory limit
per-query soft budget
per-query hard maximum
```

Exact deployment defaults are not architecture constants.

The manager tracks at least:

```text
currently reserved/held bytes
peak query bytes
operator ownership
spillable versus non-spillable memory
```

The manager does not count persistent BufferPool capacity as query execution memory.

Query-owned temporary buffers backed by ordinary process memory are accounted when large or when owned by tracked arenas/reservations.

## 24.5 MemoryReservation

Large operators obtain one or more tracked `MemoryReservation` objects.

A reservation records at least:

```text
operator owner
bytes currently held
spillability
```

Examples include:

```text
hash build
aggregate state
sort
DISTINCT
DML target spool
result spool
large RowCollection
```

Reservation growth is explicit.

An operator MUST NOT acquire an equivalent large untracked allocation to bypass a failed reservation request.

Releasing/destroying the reservation returns its accounted bytes to query/global accounting.

## 24.6 Soft and hard pressure

The per-query soft budget is a pressure signal.

Crossing it may request spill/reduction even when the hard limit is not yet reached.

The hard query/global limit is an allocation gate.

When a requested reservation extension cannot be granted:

```text
spillable operator:
    spill/release eligible state
    retry reservation

non-spillable operator:
    controlled OutOfMemory error
```

Spilling/reclaim callbacks execute outside the QueryMemoryManager's internal accounting lock so an operator can safely release/re-request memory without recursive lock coupling.

The query MUST NOT rely on arbitrary `std::bad_alloc` as its normal memory-pressure protocol.

A true allocator failure during a permitted small/runtime allocation is still converted to controlled query failure where possible.

## 24.7 SpillManager

Every query owns one `SpillManager` for temporary execution files under a dedicated managed temp directory.

It owns:

```text
query temp-file naming/lifecycle
temporary block/run allocation
large sequential write/read helpers
cleanup on normal completion/error/cancel
```

Spill data is:

```text
temporary
not WAL logged
not part of crash recovery
not a persistent database format
```

A process crash aborts the query.

Startup temp-directory maintenance may remove files proven to belong to the engine's managed spill namespace.

It MUST NOT treat arbitrary unrelated files as spill garbage.

## 24.8 Spill block contract

A spill block is self-describing and contains at least:

```text
magic
temporary format version
operator/run/partition identity
payload length
row count
CRC32C
payload
```

Integer serialization is explicit rather than native C++ object dumping.

The CRC32C validates the encoded block metadata/payload according to the block format used by that operator.

A checksum or structural mismatch is a `SpillIOError` and aborts query execution.

Because spill data is query-temporary and never crash-recovered, the architecture does not assign it a long-lived database persistent-format compatibility promise.

Specialized join/aggregate/sort spill payloads are completed by their owning Pass-13 operator contracts.

## 24.9 Spill I/O

Spill I/O favors:

```text
large sequential writes
large sequential reads
```

The initial operator-level target is:

```text
~1 MiB I/O blocks
```

and is configurable.

Operators buffer enough serialized rows/blocks to avoid issuing one tiny write per row.

Spill buffers themselves remain query-memory accounted.

## 24.10 Error and cleanup boundary

The memory/spill subsystem reports controlled categories including:

```text
OutOfMemory
SpillIOError
```

Query cancellation or execution failure unwinds MemoryReservation, RowCollection, arena backing pages, spill files, and operator state through ordinary lifetime/RAII ownership.

Spill cleanup does not commit/abort the SQL transaction by itself; the command/transaction layer owns the surrounding transaction outcome.

## 24.11 Memory/spill invariants

1. Persistent heap tuple layout and temporary RowLayout are distinct.
2. Query row handles are never persistent RIDs.
3. Retained VARCHAR payload is owned by the retaining row/operator storage.
4. QueryArena is for small bounded metadata/state, not unbounded operator data.
5. QueryArena backing memory is accounted.
6. Large execution allocations do not bypass QueryMemoryManager accounting.
7. Spillable operators respond to memory pressure through controlled spill/retry.
8. Non-spillable memory exhaustion becomes controlled query failure.
9. Spill files are temporary, never WAL logged, and never crash recovered.
10. Spill serialization never dumps compiler-native pointer/object memory.
11. Spill I/O is block/sequential oriented rather than row-at-a-time.
12. Cancellation/error releases memory and temp resources through ownership semantics.

---

# 25. Vectorized Expression Execution

## 25.1 Expression execution state

Bound/physical expressions are translated into reusable per-execution expression state.

The execution contract is:

```text
Evaluate(
    expression_state,
    input DataChunk,
    active selection
)
    -> Vector result
```

Operator/type/representation dispatch occurs at expression/vector-batch granularity.

The executor does not construct one generic `Value` object per active cell in hot loops.

Physical expression state is query-execution state and does not mutate the immutable bound/logical expression meaning.

## 25.2 Input normalization

A fixed-width/vector kernel:

1. resolves its physical type/operator implementation once,
2. obtains UnifiedVectorFormat for each input,
3. loops over the active logical rows,
4. writes/references one result Vector.

The effective input index for each active logical row comes from the normalized selection view.

NULL validity is read through the normalized validity view.

Kernels therefore produce representation-independent results for FLAT, CONSTANT, and DICTIONARY inputs.

## 25.3 Arithmetic kernels

For fixed-width arithmetic, provide a specialized no-NULL path:

```text
if every required input is all-valid:
    FastKernel
else:
    NullableKernel
```

Result validity follows Chapter 17's expression semantics.

The kernel uses the resolved physical type directly; it does not switch on SQL type once per row when the batch-level operation is already known.

Detailed arithmetic-error/overflow behavior is completed with the later execution-error contract; Pass 12 does not invent a conflicting rule.

## 25.4 Comparison kernels

Comparison kernels produce SQL BOOLEAN results with nullable validity where ordinary comparison may yield NULL.

For filter contexts, a comparison kernel MAY write matching logical row positions directly to a SelectionVector instead of materializing an intermediate BOOLEAN vector.

VARCHAR comparison uses prefix/length fast rejection where valid and full unsigned byte comparison when required.

FLOAT64 comparison remains compatible with Chapter 17/Chapter 8 semantics.

## 25.5 Vectorized AND short-circuit

For `A AND B`, evaluate `A` first.

Per logical row:

```text
A = FALSE:
    result is definitely FALSE
    do not evaluate B for this row

A = TRUE or NULL:
    row enters B evaluation subset
```

After B is evaluated for that subset, combine according to SQL three-valued logic.

## 25.6 Vectorized OR short-circuit

For `A OR B`, evaluate `A` first.

Per logical row:

```text
A = TRUE:
    result is definitely TRUE
    do not evaluate B for this row

A = FALSE or NULL:
    row enters B evaluation subset
```

Then combine with SQL three-valued logic.

These selection-based rules preserve the architecture's volatility/error-evaluation boundary: an expression branch that SQL semantics permit to skip is not eagerly executed merely because evaluation is vectorized.

## 25.7 Result ownership

A computed fixed-width result uses reusable output vector storage owned by the expression/operator state or output chunk.

A computed VARCHAR result owns/copies its bytes into storage whose lifetime covers the returned result vector, normally the output chunk StringHeap for streaming expressions.

A simple column reference may produce a borrowed/reference vector when the pipeline lifetime rules permit.

## 25.8 Expression invariants

1. Expression execution is vectorized.
2. Generic Value construction is not a hot per-cell representation.
3. Vector kind normalization happens outside the per-row representation branch.
4. All-valid inputs have specialized validity-free paths where useful.
5. Comparison preserves SQL NULL semantics.
6. Filter contexts may produce selections directly.
7. AND/OR short-circuiting evaluates only the rows whose second operand is semantically required.
8. Vectorized short-circuiting does not eagerly execute skippable volatile/error-producing branches.
9. VARCHAR/FLOAT64 comparison semantics agree with the type/index contracts.
10. Computed varlen results own bytes for their required lifetime.

---

# 26. Pipeline Execution Model

## 26.1 Pipeline graph

Execution is organized into pipelines:

```text
Source
  ↓
zero or more Streaming Operators
  ↓
Sink
```

Example:

```text
SeqScan
  -> Filter
  -> Project
  -> ResultSink
```

Pipelines form a dependency DAG when blocking state must be finalized before another pipeline can run.

The immutable physical operator tree remains useful for optimizer output, EXPLAIN, and plan validation.

The pipeline graph is the execution-time work representation built after the physical plan is finalized.

## 26.2 Pipeline roles

### Source

Produces DataChunks.

Foundation examples:

```text
SeqScan
IndexScan
Values
```

Later blocking operators may expose a source after finalization.

### Streaming operator

Transforms one input batch without requiring all future input.

Foundation examples:

```text
Filter
Project
Limit
```

### Sink / pipeline breaker

Consumes input into state that must be finalized before dependent output continues.

Examples owned by later operator chapters include hash build, aggregate build, sort, DISTINCT build, and DML target materialization.

The source/streaming/sink role is explicit rather than hidden inside recursive iterator behavior.

## 26.3 Pipeline construction

The pipeline builder walks the finalized physical plan and identifies:

```text
sources
streaming chains
sinks
dependencies
```

It does not force every physical operator into a recursive row-at-a-time `Next()` chain.

The builder creates execution graph/state references without mutating the semantic meaning of the immutable physical plan.

## 26.4 Runtime interfaces

Conceptually, a source supports:

```text
GetData(
    QueryExecutionContext,
    LocalSourceState,
    output DataChunk
)
    -> source status
```

The minimum source statuses distinguish:

```text
HAVE_MORE
FINISHED
```

and end-of-stream is never encoded as an empty chunk.

A streaming operator supports conceptually:

```text
Execute(
    input DataChunk,
    output DataChunk,
    LocalOperatorState
)
```

A sink supports conceptually:

```text
Sink(input, local_sink_state, global_sink_state)
Combine(local_sink_state, global_sink_state)
Finalize(global_sink_state)
```

Exact C++ virtual/template mechanics are implementation-specific.

The semantic state/lifetime separation is not.

## 26.5 Global and local state

Even when one worker executes the first production version, every operator separates immutable plan configuration, global per-execution state, and local per-worker/task state.

Local state owns hot mutable items where practical:

```text
cursors
reusable chunks
expression scratch
local buffers
continuation counters
```

Global state owns shared/finalized state where the operator requires it.

No operator depends on an implicit process thread-local singleton for query correctness.

This lets later scheduling parallelize pipelines without redesigning every operator state object.

## 26.6 Borrowed-data lifetime in a pipeline

The executor may pass borrowed vectors/chunks synchronously across streaming operators.

Before the upstream owner is reset/reused:

```text
all immediate downstream users of that borrowed data
must have completed consumption
```

A sink that retains data across calls or beyond the current batch copies/materializes it.

Pipeline scheduling MUST NOT asynchronously queue a borrowed DataChunk for later execution after its owner has been recycled.

## 26.7 Query cancellation

`QueryExecutionContext` contains a query-wide cancellation flag/token.

Long loops check cancellation at chunk or reasonable block boundaries.

Cancellation unwinds query-owned resources through RAII/ownership:

```text
page guards
memory reservations
spill files
local/global operator state
read-epoch guard
```

Transaction-owned logical locks are released only through the transaction's normal terminal/abort path.

A query execution layer does not silently release transaction locks independently.

## 26.8 Pipeline early stop

A streaming operator such as LIMIT may signal `pipeline early stop` when no further upstream rows are semantically needed.

Pipeline early stop is distinct from `query cancellation`.

It stops only the safely unnecessary upstream portion of the execution graph.

It MUST NOT abort the transaction, report QueryCancelled, or prevent required blocking/side-effect work elsewhere in the query from completing.

## 26.9 Single-thread first, parallel-ready

The first production executor may run one query with one worker.

The architecture nevertheless requires:

```text
global/local state split
pipeline dependency representation
source state that can later be partitioned where valid
no mutable query state embedded in immutable plan nodes
```

Worker-pool scheduling, morsels, local-state combining, and concrete parallel operator algorithms are completed in Pass 13.

## 26.10 Pipeline invariants

1. A pipeline is Source -> streaming operators -> Sink.
2. Blocking/finalization dependencies form an explicit DAG.
3. Empty chunks are not used as end-of-stream.
4. Physical-plan construction and runtime pipeline state are distinct.
5. Borrowed batch data is consumed before the owner is recycled.
6. A retaining sink owns/deep-copies retained values.
7. Global and local runtime state remain distinct even in single-worker execution.
8. Query cancellation and safe pipeline early-stop are different mechanisms.
9. Cancellation releases query resources but transaction locks follow the transaction terminal path.
10. Later parallel execution must preserve the same transaction/snapshot/read-epoch semantics.

---

# 27. Scans and Unary Physical Operators

## 27.1 Physical sequential scan

`PhysicalSeqScan` contains resolved scan configuration such as:

```text
TableDescriptor / TableId
required ColumnIds / output LogicalSlotIds
eligible planner-assigned pushed predicates
```

Runtime scan state obtains its effective snapshot/read epoch through the QueryExecutionContext.

The scan walks heap pages in ascending physical PageNo order and slots in physical slot order.

This physical traversal does **not** establish a SQL ordering property.

For each heap page:

```text
1. BufferPool fetch/read-latch
2. iterate valid tuple slots
3. inspect tuple header
4. apply exact MVCC visibility
5. evaluate eligible cheap pushed predicates
6. decode only columns still required
7. append surviving values to output DataChunk
8. release guard when the page is no longer needed
```

The scan does not decode every table column merely because the table contains it.

Predicate dependency columns may be decoded before output-only columns so rejected rows avoid unnecessary decoding.

When a tuple's persisted `schema_version` requires historical interpretation, scan decoding uses Chapter 16's:

```text
ResolveSchema(TableId, tuple.schema_version)
```

contract.

## 27.2 Scan page and string lifetime

A scan MUST NOT return execution vectors containing pointers into a heap page after that page guard may be released.

Therefore:

```text
fixed-width tuple values
    -> copied to output vectors

VARCHAR tuple bytes
    -> copied to output DataChunk StringHeap
    -> StringRef points into chunk-owned memory
```

The heap page may then be unpinned as the scan advances.

This prevents long pipelines from pinning pages solely for downstream string lifetime.

## 27.3 Scan predicate pushdown boundary

The physical planner may assign predicates to a scan only after their semantic pushdown safety has been established.

The scan executes those assigned predicates.

It does **not** discover additional SQL rewrite opportunities at runtime.

A scan-local predicate may reduce tuple decoding, output cardinality, and memory traffic.

General predicate semantics remain representable by PhysicalFilter.

The scan requests/decode dependencies needed by pushed predicates even when those columns are not final outputs.

## 27.4 Physical index scan

`PhysicalIndexScan` contains at least:

```text
IndexId / immutable IndexDescriptor
encoded lower bound
encoded upper bound
lower/upper inclusivity
required heap columns
output LogicalSlotIds
ordering property
```

Execution is:

```text
B+ range cursor
    ↓
candidate RID batch
    ↓
heap fetch
    ↓
MVCC visibility
    ↓
decode required columns
    ↓
output DataChunk
```

An index entry is only a candidate.

It is never proof of SQL row visibility.

The QueryExecutionContext's read-epoch guard protects candidate physical RIDs from reuse while execution may retain them.

## 27.5 RID batching

Index scan collects a small batch of candidate RIDs rather than performing one candidate/one returned row iterator step.

Initial target:

```text
up to one DataChunk capacity
```

The initial implementation preserves B+ cursor order while fetching those candidates.

A future implementation may group heap fetches by PageId only if the physical plan no longer promises/needs the original index ordering.

The batch is query-local temporary state and is never a persistent RID list.

## 27.6 Index ordering property

A forward B+ range cursor produces ascending physical user-key order.

Therefore a compatible PhysicalIndexScan may advertise an ascending ordering property when:

```text
index key prefix matches
binary collation/type semantics match
NULL ordering matches
requested direction is supported
```

For v1 forward scan:

```text
ASC
NULLS FIRST
```

is the natural index property.

MVCC filtering may remove rows but does not reorder the surviving candidate sequence.

Native reverse scan remains deferred by Chapter 8, so DESC ordering is not claimed by the baseline PhysicalIndexScan merely by walking forward.

Physical planning—not executor guesswork—decides whether this property satisfies ORDER BY.

## 27.7 PhysicalFilter

PhysicalFilter evaluates its bound physical predicate over one input chunk/active selection.

Preferred output when ownership permits is:

```text
reduced cardinality
+
DICTIONARY/reference vectors over the input
```

rather than copying every surviving value.

Filter preserves logical row order.

If no row survives, `output cardinality = 0`, which is an empty batch, not end-of-stream.

If downstream lifetime exceeds the input owner's lifetime, materialization occurs at the appropriate retaining boundary.

## 27.8 PhysicalProject

PhysicalProject evaluates projection expressions vectorized.

For a direct input slot:

```text
reference/borrow input vector
```

when lifetime permits.

For a computed fixed-width expression:

```text
write/reuse flat output vector
```

For a computed VARCHAR:

```text
result bytes live in output-owned StringHeap/other valid result owner
```

Projection is streaming and is not a pipeline breaker.

Its output order is the projection's LogicalSlotId order.

## 27.9 PhysicalLimit

Runtime state tracks:

```text
rows_skipped
rows_emitted
```

OFFSET is applied before LIMIT.

The operator may use selection/slicing instead of cell copying.

Once the limit is satisfied, it requests `pipeline early stop` through §26.8 when no further upstream rows are required.

It does not turn ordinary LIMIT completion into whole-query cancellation.

With no finite LIMIT, only OFFSET accounting applies.

## 27.10 PhysicalValues

PhysicalValues is a source over already bound/typed literal rows.

It produces DataChunks up to standard capacity.

Constant scalar/string payload owned by the immutable plan may be referenced only when its lifetime safely exceeds every execution consumer; otherwise values are copied into the output chunk.

PhysicalValues performs no SQL parsing/type resolution.

## 27.11 PhysicalResultSink foundation

A result sink consumes final DataChunks synchronously or materializes/retains result memory according to the later client/result-interface contract.

It MUST NOT expose a borrowed producer chunk beyond the producer/owner lifetime.

The detailed cursor/client result interface and DML RETURNING spool are completed in Pass 13.

## 27.12 Scan/unary invariants

1. Sequential physical page order is not a SQL ordering guarantee.
2. Scan visibility is always heap MVCC visibility.
3. Scans decode only required/predicate-dependent columns.
4. Historical tuple schema interpretation uses the catalog schema-version contract.
5. Scan output never points into an unpinned heap page.
6. Index entries are candidates and never bypass heap MVCC.
7. Index candidate RID lifetime is covered by the execution read epoch.
8. Initial index RID batching preserves index order.
9. Forward baseline index ordering is ASC/NULLS FIRST when compatible.
10. Filter prefers selection/dictionary output when safe and preserves row order.
11. Project is streaming and computed varlen results own their bytes.
12. Limit uses pipeline early-stop rather than query cancellation.
13. Values/result execution does not redo parsing/binding/type resolution.
14. Client/result boundaries do not leak borrowed chunk lifetime.
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

## 39.2 SQL front-end and logical-planning errors

The upper semantic layer distinguishes at least:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature
CardinalityError
```

Where a failure originates from SQL source, it carries the smallest useful source span.

User input errors do not become internal invariant failures merely because they are discovered after binding.

Logical-plan validator failures, by contrast, indicate internal architecture/implementation defects.

## 39.3 Execution-foundation errors

The execution foundation distinguishes controlled runtime failures including:

```text
OutOfMemory
SpillIOError
QueryCancelled
```

These failures unwind query-owned temporary resources.

They do not silently release transaction-owned logical locks outside the transaction terminal/abort path.

Detailed arithmetic/operator-specific execution error categories are completed by the later execution pass.

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

## 40.5 Logical EXPLAIN and front-end diagnostics

Before physical planning exists, EXPLAIN can display the bound logical tree independently of AST pretty-printing.

Useful debug detail includes:

```text
LogicalSlotId
BindingId
TableId
resolved type
nullability
lineage
catalog schema version
logical predicate/join/aggregate structure
```

Front-end/catalog observability may additionally expose parser/binder latency, catalog cache hits/misses, descriptor-version lookups, and DDL/object-retirement queue size.

These diagnostics must not become a correctness dependency.

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

Physical slot reuse tests must follow the Chapter-14 grace protocol and persisted free-slot list; the old milestone recipe's generic “reusable slots” item is not an authorization for immediate DEAD-slot reuse.

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

## 41.4 Catalog, front-end, and logical-plan verification obligations

Parser verification includes positive syntax/AST-shape cases, negative syntax/error-span cases, precedence/associativity cases, quoted/unquoted identifier behavior, string/comment termination errors, and multi-statement synchronization.

Binder/type verification includes unknown/ambiguous names, aliases/self-joins, qualified references, wildcard expansion, type promotion/casts, NULL typing/3VL, aggregate/GROUP BY legality, ORDER BY alias/ordinal, LEFT JOIN nullability, DML target binding, and subquery scopes.

Type property tests MUST compare binder semantics with constant evaluation, future vectorized execution, and index-key comparison where the same type participates.

Catalog verification includes:

```text
bootstrap/open
reopen persistence
normalized/quoted name lookup
stable non-reused object IDs
historical schema lookup
snapshot-aware cache behavior
transactional DDL visibility
CREATE abort orphan handling
DROP delayed retirement
descriptor invalidation without in-place mutation
```

Logical-plan tests assert canonical plan shapes from bound statements.

Every rewrite rule has an input logical plan and expected transformed plan and, once execution exists, differential semantic tests with NULL-rich data and volatility-sensitive cases.

Logical validators are exercised both before and after rewrite phases.

Fuzzing targets at least lexer, parser, literal/type conversion, and the bound constant evaluator, with no crashes/out-of-bounds access and bounded failure behavior on arbitrary input.

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

## 42.3 Catalog/front-end/logical-planning measurements

Measure enough to detect pathological architecture without optimizing the front end prematurely.

Useful dimensions include:

```text
lexer MB/s
parser statements/s
binder latency
catalog name/ID lookup latency
historical schema lookup latency
catalog cache hit rate
large SELECT-list binding
large VALUES binding
large expression-tree binding
multi-join binding
logical-plan construction time
rewrite-phase time
AST/plan arena allocations and bytes
```

Representative parser/AST memory cases include a 100-column SELECT, large VALUES insert, deep Boolean expression, and multi-join query.

Arena-based ownership should avoid obvious per-node allocation churn, but benchmark evidence—not guesswork—drives further optimization.

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
| Built-in catalog TypeId codes | `0` invalid, `1..7` BOOLEAN/INT32/INT64/FLOAT64/DATE/TIMESTAMP/VARCHAR | §16.4 |
| WAL segment | 64 MiB | §12.2 |
| WAL record alignment | 8 bytes | §12.3 |
| WAL ordinary record header | 48 bytes | §12.4 |
| WAL PageId | 16 bytes | §12.5 |
| WAL record_type code | 16 bits, little-endian | §12.7 |
| PAGE_DELTA prefix | 24 bytes | §12.8 |
| PAGE_DELTA patch header | 8 bytes | §12.8 |
| PAGE_INIT/PAGE_IMAGE payload prefix | 24 bytes | §12.9 |
| PAGE_INIT/PAGE_IMAGE page image | 8192 bytes | §12.9 |
| BTREE_MTR payload prefix | 16 bytes | §12.10.2 |
| BTREE_MTR affected-page prefix | 24 bytes | §12.10.2 |
| Database control file | 8192 bytes = two 4096-byte slots | §13.2 |
| Database control-slot v1 header | 88 bytes | §13.2.1 |
| Catalog-object ID allocator | uint64 monotonic, durable, never reused | §13.2.6 / §21.4 |
| Database control slot header | 80 bytes | §13.2.1 |
| CHECKPOINT_BEGIN payload | 32 bytes | §13.6 |
| CHECKPOINT_DATA prefix | 24 bytes | §13.7 |
| Checkpoint DPT entry | 24 bytes | §13.7 |
| Checkpoint writer entry | 16 bytes | §13.7 |
| CHECKPOINT_END payload | 32 bytes | §13.8 |

Catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

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

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.21; FSM/reclamation invariants are listed in §6.13; I/O/buffer invariants are listed in §7.13; B+ tree invariants are listed in §8.29; transaction/snapshot invariants are listed in §9.16; MVCC invariants are listed in §10.6; logical-locking invariants are listed in §11.15; WAL/commit invariants are listed in §12.18; recovery invariants are listed in §13.21; vacuum/reclamation invariants are listed in §14.18; end-to-end write invariants are listed in §15.9. Catalog invariants are listed in §16.11; type/value invariants in §17.12; lexer/parser/AST invariants in §18.16; binder/expression invariants in §19.20; logical-plan/rewrite invariants in §20.20; upper semantic-layer invariants in §21.20; physical-plan/runtime invariants in §22.7; vector/string invariants in §23.14; memory/spill invariants in §24.11; expression-execution invariants in §25.8; pipeline invariants in §26.10; scan/unary invariants in §27.12.

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
- parallel/background vacuum beyond the correct manual-vacuum baseline,
- multiple SQL schemas/namespaces beyond `main`,
- `ALTER TABLE` until its DDL/version-translation semantics are implemented,
- RIGHT JOIN and FULL OUTER JOIN,
- LATERAL,
- correlated subqueries until deliberately supported,
- recursive CTEs,
- window functions,
- set operations,
- nested block comments,
- prepared-statement parameters in the initial parser milestone,
- locale-aware VARCHAR collations and a collation framework,
- UTF-8 validity/character-count semantics and `VARCHAR(n)`,
- timezone-aware SQL types/rules,
- foreign keys,
- CHECK constraints,
- views and materialized views,
- triggers and stored procedures,
- MERGE,
- UPSERT / ON CONFLICT,
- generated columns,
- sequences / identity columns,
- expression indexes,
- partial indexes,
- DECIMAL / NUMERIC,
- INTERVAL,
- JSON,
- arrays,
- user-defined types,
- SQL privileges / roles,
- online concurrent index build,
- fine-grained concurrent DDL beyond the conservative SchemaLock/writer-gate baseline,
- SEQUENCE and RLE vector representations until profiling justifies them,
- native reverse B+ index scan until explicitly implemented,
- asynchronous client exposure of borrowed internal chunks,
- per-query multi-worker scheduling details beyond the parallel-ready state split,
- spill-file crash recovery or WAL logging.

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

The pre-Pass-10 core resolution closed the already-owned transactional-storage decisions for:

- byte-exact WAL record grammar and record-type registry,
- byte-exact dual-slot `database.control`,
- checkpoint BEGIN/DATA/END identity and analysis/redo scan bounds,
- exact ReadEpochManager registration/retirement grace arithmetic,
- sparse transaction-status physical reclamation without PageNo renumbering,
- mandatory ordinary-page checksum finalization and stable flush-image semantics,
- crash-safe page append publication and recovery tail reconciliation,
- complete `DEAD -> UNUSED` / free-slot-list state machine and idempotent vacuum cleanup,
- READ COMMITTED retry restrictions after persistent statement writes,
- terminal outcome / active registry / lock-release linearization,
- full-page-image retention through every clean-to-dirty interval.

These items remain recorded as resolved decisions in `ARCHITECTURE_REWRITE_ISSUES.md`.

Pass 11 resolves the DDL-owned catalog-object identity gap:

- TableId/IndexId/ConstraintId use one durable non-reused uint64 catalog-object allocator in `database.control`,
- ColumnId allocation is table-local and non-reused,
- DDL performs identity/file allocation only during execution under DDL coordination.

Current upper-layer gaps intentionally left explicit are:

1. **Catalog bootstrap/CATALOG_DATA physical representation.** The semantic bootstrap role is locked, but no byte-exact bootstrap format or `CATALOG_DATA` page layout is specified; see R-036.
2. **Persistent default-expression encoding.** V1 defaults may be immutable semantic expressions, but the exact versioned catalog encoding and stable function/operator identity representation are not byte-exact; see R-040.

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
Pass 10   legacy §§301–358
Pass 11   legacy §§359–433
Pass 12   legacy §§434–478
```

The existing `ARCHITECTURE.md` remains the active architecture authority until the full rewrite, reconciliation audit, and explicit cutover are complete.
