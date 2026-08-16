# DBlusBlus Architecture

**Status:** Authoritative v1 architecture contract  
**Architecture version:** v1

## Purpose

This document defines the technical architecture of DBlusBlus: a from-scratch, single-node relational database management system implemented in C++20.

It specifies architectural responsibilities, subsystem boundaries, persistent-format requirements, concurrency and lifetime rules, correctness invariants, and performance-relevant design constraints.

`ARCHITECTURE.md` is the authority for intended system behavior. Project progress belongs in `PROJECT_STATE.md`; implementation sequencing and module-layout guidance belong in `DEVELOPMENT.md`; detailed test and benchmark procedures belong in `VERIFICATION.md`; historical implementation records belong in `devlog/`.

## Contract language

Requirements use ordinary normative language:

- **MUST / MUST NOT** — required by the current architecture contract.
- **SHOULD / SHOULD NOT** — the architectural default; deviation requires a concrete technical reason and must remain compatible with all MUST-level requirements.
- **MAY** — explicitly permitted implementation freedom.
- **Deferred** — intentionally outside the current baseline; it is not part of the required v1 implementation unless later promoted by an explicit architecture revision.

Where a cross-layer summary restates a rule, the detailed format or protocol in the owning subsystem chapter is canonical. Summaries and registries are indexes and integration constraints; they do not create independent conflicting values.

Changes to an accepted architectural requirement require an explicit architecture revision. Implementation behavior does not silently redefine this document.

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

Concrete source-code directory and filename organization is not an architectural requirement. Persistent database namespace layout is the separate compatibility/recovery contract in §4.7.1–§4.7.8.

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
- the database-root namespace and durable create/rename/unlink lifecycle,
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

### 4.7.1 Database namespace root

One opened database owns one caller-selected final directory path:

```text
database_root
```

That directory is the database's persistent namespace root. V1 uses this exact managed layout:

```text
database_root/
    database.control
    catalog.dat
    txn_status.dat
    pending/
    wal/
    table_<table_id>.heap
    table_<table_id>.fsm
    index_<index_id>.btree
```

`<table_id>` and `<index_id>` are the canonical unsigned decimal identifier spelling with no leading zeroes. The singleton and object-file meanings remain owned by their format/catalog chapters.

Uncommitted DDL files use only:

```text
pending/txn_<txn_id>_file_<file_id>.heap
pending/txn_<txn_id>_file_<file_id>.fsm
pending/txn_<txn_id>_file_<file_id>.btree
```

using the same canonical unsigned decimal spelling. The exact private basename identifies its creating normal TxnId, allocated FileId, and file kind; the validated superblock supplies the object identity.

WAL segment basenames are exact in §12.2. Temporary query/spill files are not persistent database objects and remain under Chapter 24's separately managed temporary namespace.

While a database is open, the storage namespace owner keeps open directory descriptors for:

```text
database_root
database_root/pending
database_root/wal
```

and resolves managed entries relative to those descriptors. The external parent directory descriptor is required only while creating and durably publishing the database root itself.

Opening an existing database uses no-follow directory-relative lookup for managed names. `database_root`, `pending/`, and `wal/` must be directories; control/object/segment names must be regular files. A managed-name symlink or wrong filesystem object type is an open/corruption error, not an alternate path to follow.

All three managed directories and the database root's external parent/destination are on one filesystem for operations that use rename. V1 does not support cross-filesystem publication fallback.

### 4.7.2 Required Linux/POSIX durability semantics

V1 requires a Linux filesystem/configuration with all of these semantics:

- `openat`/equivalent create with exclusive, no-follow behavior creates one direct managed directory entry without following a symlink,
- successful complete `pwrite` changes regular-file bytes but does not itself make them durable,
- successful `fdatasync(regular_file)` makes prior file data and retrieval-critical metadata such as file length durable,
- successful `fsync(directory_fd)` makes prior completed create, rename, and unlink namespace mutations in that directory durable,
- same-filesystem rename is atomic for runtime name lookup but is not crash-durable until every affected directory has been synchronized,
- unlink removes a runtime name but is not crash-durable until its parent directory has been synchronized,
- `close` releases a descriptor and is not a durability primitive,
- synchronizing a regular file does not substitute for synchronizing its parent directory.

If the selected filesystem does not support reliable directory `fsync` with these semantics, it is not a supported v1 durable database filesystem. The engine MUST propagate directory-open/sync and regular-file sync failures rather than assuming persistence.

`fsync`/`fdatasync` retry `EINTR` according to Linux semantics. A namespace-mutating syscall that returns an error with an outcome not safely known MUST be reconciled by inspecting the exact source/destination entries under the same namespace synchronization; it is never assumed successful for durability acknowledgement.

Engine namespace mutations are serialized per managed directory, or use equivalent ordering, so each publication can identify a successful directory sync ordered after its own mutation. One directory sync MAY durably cover several preceding mutations, but none is considered published before that successful sync.

### 4.7.3 Persistent object-file state machine

An ordinary DDL-created heap/FSM/B+ file moves only through:

```text
ABSENT
    -> PRIVATE_DURABLE
        -> FINAL_DURABLE_UNCOMMITTED
            -> CATALOG_COMMITTED
                -> RETIRED_LINKED
                    -> UNLINK_PENDING
                        -> ABSENT_DURABLE
            -> ORPHAN_LINKED
                -> UNLINK_PENDING
                    -> ABSENT_DURABLE
        -> ORPHAN_LINKED
            -> UNLINK_PENDING
                -> ABSENT_DURABLE
```

Meanings are:

- `PRIVATE_DURABLE`: initialized file contents and the `pending/` entry are durable, but no committed catalog object may reference it,
- `FINAL_DURABLE_UNCOMMITTED`: the no-replace rename and both affected directory mutations are durable, but transaction commit has not established catalog ownership,
- `CATALOG_COMMITTED`: durable terminal commit and catalog visibility reference the already-durable final name,
- `ORPHAN_LINKED`: a private or final-uncommitted managed entry survived but no committed catalog ownership can require it,
- `RETIRED_LINKED`: committed catalog state no longer publishes the object, but lifetime/reclamation gates still require or may leave the final physical name,
- `UNLINK_PENDING`: unlink has completed in the running process but the owning directory has not yet successfully synchronized, so post-crash presence remains uncertain,
- `ABSENT_DURABLE`: unlink and parent-directory synchronization have durably removed the name.

File existence alone never establishes catalog commitment. Catalog commitment MUST NOT refer to a private basename.

### 4.7.4 Private creation and durable final-name publication

Creating a new private object file uses:

```text
1. allocate its nonreusable object/File identities
2. create the exact pending basename with exclusive no-follow semantics
3. establish the required initial file length and complete canonical
   superblock/initial pages
4. pwrite every required initialized byte
5. fdatasync the private regular file
6. fsync database_root/pending
7. only then classify the file PRIVATE_DURABLE and hand it to
   ordinary page/build ownership
```

Expected failure before step 7 never creates a published object. Any surviving managed private entry is cleanup input.

Before DDL may append catalog state that can become committed for the new object, every file in that object's physical file set follows one publication barrier:

```text
1. complete the private heap/FSM/B+ initialization or offline build
2. flush every required private-file page through ordinary WAL-before-data
3. fdatasync each private regular file after its final prepublication length/data
4. rename each private basename to its deterministic final basename using
   same-filesystem no-replace semantics
5. fsync database_root
6. fsync database_root/pending
7. only after all required file renames and both directory syncs succeed,
   classify the physical file set FINAL_DURABLE_UNCOMMITTED
```

V1 uses Linux `renameat2(..., RENAME_NOREPLACE)` or an equivalent same-filesystem no-clobber primitive. It MUST NOT overwrite an existing managed final name. An unexpected destination collision is an I/O/invariant/corruption error, not permission to replace another file.

The runtime rename is the atomic name switch for one file. The **durable physical namespace-publication point** is successful completion of both directory syncs ordered after that rename. A multi-file object has no filesystem-atomic group rename; its publication barrier completes only after every required file is at its final name and the root/pending directory mutations for the entire set have been synchronized.

A crash or error during a multi-file barrier may leave a mixture of pending and final names. Because terminal commit is forbidden before the whole barrier succeeds, none of those names can be required by committed catalog state; they remain orphan candidates.

The open file descriptor continues to identify the same inode across rename. The namespace/file-registration owner updates its process-local basename state before exposing the file as final; no other transaction discovers the object through the filesystem rather than catalog visibility.

### 4.7.5 DDL commit prerequisite and namespace failures

For CREATE TABLE/INDEX and their physical dependent files:

```text
all required files FINAL_DURABLE_UNCOMMITTED
    before
transaction-owned catalog rows become commit-eligible
    before
TXN_COMMIT durability
    before
runtime committed catalog publication
```

The DDL coordinator MUST NOT append/acknowledge a successful terminal commit while any referenced physical file lacks successful regular-file and required directory synchronization.

If file-content synchronization succeeds but a required directory sync fails, the namespace outcome is uncertain and durable publication has **not** succeeded. The transaction cannot be acknowledged committed; any private or final name that survives is an orphan candidate.

If the final directory entry is durable but catalog WAL construction, transaction commit, or precommit processing later fails, that file remains `FINAL_DURABLE_UNCOMMITTED` and is an orphan candidate. This is intentional physical residue, not a visible half-created object.

A namespace-durability failure is an I/O/durability failure and MUST NOT be reported as successful publication. Its diagnostic retains the affected directory/path, namespace operation, and underlying error. Its statement/transaction surface behavior follows §39's separate general error-state rules; this section requires that every CREATE namespace prerequisite completes before durable terminal commit. The separate general handling of an unrelated error discovered after commit is outside this protocol and is not changed by it.

Ordinary object-file creation has these canonical crash interpretations:

1. Before private create: no physical object exists.
2. After private create but before initialization: any surviving exact pending entry is an incomplete orphan; no catalog commit may reference it.
3. After initialization but before regular-file sync: the pending contents may be incomplete after restart and the entry is an orphan.
4. After regular-file sync but before pending-directory sync: the entry may survive or disappear, but no committed state depends on it; a survivor is an orphan.
5. After rename but before both final/source directory syncs: private, final, or conservatively both names may be observed after restart; terminal commit was forbidden, and every survivor is an orphan candidate.
6. After durable final-name publication but before catalog WAL: the final file is a durable orphan candidate.
7. After catalog WAL but before durable `TXN_COMMIT`: recovery classifies the DDL transaction as noncommitted/aborted and the final file as an orphan.
8. After durable `TXN_COMMIT` but before runtime descriptor publication: recovery reconstructs committed catalog ownership and finds the required already-durable final file; runtime publication resumes only after recovery.
9. After successful CREATE/COMMIT acknowledgement: both committed metadata and every required final namespace entry satisfy their durability prerequisites.

### 4.7.6 Orphan classification and recovery

After process restart there are no live owners of a `pending/` entry. Because §4.7.5 forbids committed catalog references to private basenames, every exact managed private entry is an orphan and its FileId is an unpublished target for recovery purposes.

Managed final object files are classified only after WAL recovery has established terminal outcomes and the committed catalog has been reconstructed:

```text
required:
    named by the immutable bootstrap set or visible committed catalog ownership

orphan:
    exact managed final basename/superblock identity with no required owner
```

Recovery may defer or skip page redo for a missing/private FileId only after proving that no bootstrap or committed catalog object owns it. A file required by bootstrap or committed catalog state that is absent, has the wrong final basename, or fails its required identity/superblock checks is corruption/open failure; recovery does not invent or silently recreate such a committed object file.

Orphan cleanup removes only entries matching the exact engine-managed private/final grammar and proven unowned by committed state. Unknown unrelated names are not guessed to be garbage. Each cleanup unlink follows §4.7.7 and is complete only after the owning directory is synchronized.

### 4.7.7 Durable unlink and retirement

An object final name is not unlinked merely because DROP committed. Chapter 21's snapshot/descriptor gates and the canonical BufferPool drain in §7.12.5 first establish that no live owner can use the file.

After that safe point, durable retirement is:

```text
1. prevent new opens/pins and drain/close all managed file ownership through §7.12.5
2. unlinkat the exact managed basename from its owning directory
3. fsync that owning directory
4. only then classify the namespace state ABSENT_DURABLE
```

If unlink fails, or succeeds but directory sync fails, physical retirement remains incomplete and is retried. The semantic DROP is not reversed: a surviving/reappearing final name has no committed catalog owner and is an orphan.

Crash outcomes are canonical:

- before unlink: the retired file remains linked and cleanup retries,
- after unlink but before directory sync: the name may be present or absent after restart; committed catalog state does not own it, so recovery accepts either and unlinks/synchronizes it if present,
- after directory sync: absence is durable.

The same mutation-plus-parent-`fsync` rule governs deletion of orphan private files and recyclable WAL segments, using `pending/` and `wal/` respectively.

### 4.7.8 Database-root bootstrap publication

Engine-managed database creation requires an existing, durably provisioned external parent directory and an absent requested final `database_root` path. Recursive creation/durability of ancestors above that parent belongs to the caller/provisioning environment. Given final basename `D`, creation uses a same-parent, same-filesystem staging directory named exactly:

```text
D.dblusblus-creating
```

The bootstrap protocol is:

```text
1. open the external parent directory and create the staging directory
   with exclusive no-follow semantics
2. open the staging root and create its pending/ and wal/ directories
3. create/initialize database.control, catalog.dat, txn_status.dat,
   all six system-relation heap/FSM files, any other required built-in
   object files, and initial WAL segment 0
4. fdatasync every startup-critical regular file after its complete
   initialized contents/length are written
5. fsync staging/pending, staging/wal, and the staging root directory
6. validate the complete bootstrap using the normal open-time identity,
   checksum, and cross-reference rules
7. rename the staging directory to final basename D using no-replace semantics
8. fsync the external parent directory
9. only then report database creation success
```

The staging root as a whole is private; startup-critical files inside it use their final basenames and are not ordinary transaction-owned DDL files. The initial WAL segment follows §12.2.1 before step 5.

A crash before step 7 leaves no final database root and may leave only a recognized staging-root orphan. A crash after rename but before parent sync is an unacknowledged creation: after restart the final or staging name may survive; a surviving final root is usable only if complete bootstrap validation succeeds, while a staging root is never opened as a database. A crash after step 8 leaves a durably named, fully initialized database. Stale staging roots are removed only by explicit engine create/maintenance logic after proving that no live creator owns them, and their removal is synchronized in the external parent directory.

Opening an existing database never treats `D.dblusblus-creating` as an alias for `D`.

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
CATALOG        -> 0 for the v1 singleton `catalog.dat`
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
8. install the canonical initialized image in its reserved, non-usable
   BufferPool frame through §7.12.4
9. as one publication event with respect to scans/fetches:
       advance published_page_count = page_no + 1
       publish that frame RESIDENT with the creator's pin
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
9. fdatasync the owning page file after that write, possibly as part of a
   batch covering multiple completed page writes to the same file
10. after successful file synchronization, under frame/DPT synchronization:
       if modification_generation is still G:
           clear dirty and rec_lsn
       else:
           leave the frame dirty
```

The checksum written to disk therefore describes exactly the same stable image whose `page_lsn` participates in WAL-before-data ordering.

The resident frame does not need to overwrite its own checksum bytes with the flush-buffer checksum. `pwrite` success without the required later file synchronization is not a stable flush and cannot clear dirty/DPT state; §7.10.3 owns that durability rule.

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
20. Regular-file synchronization does not durably publish create/rename/unlink directory entries; the required owning directories are synchronized explicitly.
21. Committed catalog ownership never names a private DDL basename and never precedes durable final-name publication of every required physical file.
22. A required committed object file is never reconstructed from filename guesswork when its durable final entry is missing.

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

When those bytes come from BufferPool, every `HeapPage`/tuple/VARCHAR view remains borrowed under the guard-lifetime contract in §7.7.2.

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

### 6.3.1 Representative boundaries

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

The runtime FSM MAY use a bucketed representation conceptually shaped as:

```text
bucket[0]
bucket[1]
...
bucket[255]
```

Insertion asks the runtime FSM for a candidate category/page capable of satisfying the requested size.

This in-memory accelerator is rebuildable runtime metadata and is **not** part of the persisted `FSM_DATA` format.

The exact runtime data structure, tie-breaking policy, and candidate-search procedure are implementation choices, provided they preserve the persisted FSM semantics and the heap-page revalidation requirement.

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
- the open database-root/pending directory handles and managed namespace operations from §4.7,
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

WAL writing, durability scheduling, and `durable_lsn` advancement belong to the WAL writer/flusher. `DiskManager` provides lower-level file synchronization primitives but does not own transaction-level WAL durability coordination.

### 7.3.1 File lifecycle boundary

Creating an operating-system file and initializing a database file superblock are separate operations.

The raw disk layer MAY create/register an empty file, but it does not:

- choose `FileKind`,
- encode page `0`,
- interpret file-kind-specific metadata,
- establish higher-level persistent object identity.

A higher page-file/storage layer allocates and writes the superblock and performs persistent-format/identity validation.

The raw disk layer's create/rename/unlink calls do not themselves publish higher-level objects. The storage namespace owner composes regular-file synchronization and parent-directory synchronization through the canonical §4.7 lifecycle before reporting durable namespace publication.

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
fsync
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

The v1 regular page-file content synchronization primitive is:

```text
fdatasync
```

with `EINTR` handling as above. It makes file bytes and retrieval-critical metadata such as length durable under §4.7.2, but it does not make create/rename/unlink namespace entries durable. `fsync` on each affected parent directory is separately mandatory at §4.7 publication/retirement boundaries.

## 7.5 BufferPool ownership

The BufferPool owns the process-local page table and every resident frame:

```text
PageId -> load/residency entry -> optional buffer frame
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

For one BufferPool instance, one `PageId` has at most one active load operation and at most one frame that can become or is usable as that page. Independently usable duplicate resident copies are forbidden.

The BufferPool MUST NOT parse heap tuples, physical schemas, or B+ tree keys.

Normal page-format objects operate over bytes supplied through BufferPool-managed lifetime once the buffer layer exists. `HeapPage`, `FsmPage`, B+ page controllers, and tuple decoders are non-owning page-local views; they do not pin, unpin, evict, flush, or perform I/O.

## 7.6 Canonical resident-frame state machine

### 7.6.1 Frame metadata and state dimensions

A buffer frame contains at least:

```text
aligned 8192-byte page bytes
PageId or no identity
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

The canonical **ownership state** of every frame is exactly one of:

```text
FREE
    owns no PageId, has no page-table binding, and is reusable

LOADING
    is exclusively bound to one PageId and one load operation, but its
    bytes are not published for ordinary access

RESIDENT
    contains one fully loaded and validated PageId and may receive pins

EVICTING
    still owns its old PageId but has been reserved against new pins while
    clean eviction or dirty writeback/removal completes
```

I/O progress is an orthogonal substate:

```text
NONE
READ_IN_PROGRESS
WRITEBACK_IN_PROGRESS
```

The legal combinations are:

| Ownership state | Legal I/O substate |
|---|---|
| `FREE` | `NONE` |
| `LOADING` | `READ_IN_PROGRESS` for an existing-page read; `NONE` while constructing, validating, or publishing a newly allocated page |
| `RESIDENT` | `NONE` or one `WRITEBACK_IN_PROGRESS` copied-image flush |
| `EVICTING` | `NONE` or one `WRITEBACK_IN_PROGRESS` eviction flush |

`FLUSHING` is therefore not a separate ownership state in v1. It means `RESIDENT + WRITEBACK_IN_PROGRESS`; the frame remains the one resident copy and may continue to serve accesses under §7.10.2. An eviction writeback instead means `EVICTING + WRITEBACK_IN_PROGRESS` and permits no ordinary page access.

Loading, writeback, victim, no-flush, and file-retirement reservations are internal ownership claims, not public pins. No source-code enum or particular arrangement of flags is mandated, but every runtime frame state MUST map unambiguously to this model.

`modification_generation` is a process-local monotonically increasing per-frame counter incremented for every persistent-byte mutation installed in that frame.

It is not persisted.

Its comparison token MUST NOT wrap/repeat while a writeback completion could compare against it. A finite implementation that cannot advance safely fails the next mutation before changing page bytes and may reseed only after quiescing the frame with no active writeback; silent wrap is forbidden.

Together with page latching it lets BufferPool determine whether a page changed after a stable flush image was copied but before the I/O completed.

Frame metadata remains process-local. `rec_lsn`, full-image epoch state, and temporary no-flush barriers follow Chapters 12–13.

The page-byte region SHOULD be suitably aligned for efficient copying/checksum work.

Process-local metadata SHOULD avoid pathological false sharing where measurement shows contention, but cache-line padding is not a persisted or correctness contract.

### 7.6.2 State transition table

The following transitions are canonical. “Mapping” includes the in-progress page-table entries defined by §7.8.

| Current state | Event and preconditions | Next state | Page-table effect | Pin effect | Dirty effect | I/O failure effect |
|---|---|---|---|---|---|---|
| `FREE` | bind the frame to the sole existing-page load intent for `X` | `LOADING + READ_IN_PROGRESS` | bind `X`'s in-progress entry to this frame before reading | no public pin yet; fetch claims remain pending | reset clean, `rec_lsn=INVALID_LSN` | load failure follows the `LOADING` failure row |
| `FREE` | bind the frame to the sole new-page publication intent for `X` | `LOADING + NONE` | bind `X`'s in-progress entry before constructing its image | no public pin yet; creator claim remains pending | reset clean until the PAGE_INIT/MTR publication mutation is installed | publication failure follows §7.12.4 |
| `LOADING` | existing-page read and all required validation succeed | `RESIDENT + NONE` | atomically publish the same entry as usable | convert every uncancelled fetch claim, including the loader's, into exactly one pin | loaded image is clean | not applicable |
| `LOADING` | new-page PAGE_INIT/MTR, validation, and owning publication bound succeed under §7.12.4 | `RESIDENT + NONE` | atomically publish the same entry as usable with the owning page-count/reachability publication | convert the creator's private claim into exactly one pin | publish dirty with PAGE_INIT/MTR recovery metadata and generation | not applicable |
| `LOADING` | read or validation fails | `FREE + NONE` | close/remove the failed in-progress entry; retain its completion result only for registered waiters | no claim becomes a pin | reset clean | all current waiters receive the same load error; a later independent fetch may retry |
| `RESIDENT` | fetch hit while not victim-reserved/retiring and pin count can increase | `RESIDENT` | unchanged | atomically add one pin | unchanged | not applicable |
| `RESIDENT` | publish one WAL-protected persistent mutation under §7.10.1 | `RESIDENT` | unchanged | caller already pinned/latched | increment generation; publish dirty/`rec_lsn`/DPT state | failure before publication remains protected and follows the owning WAL protocol; no inconsistent frame may become usable/flushable |
| `RESIDENT + NONE` | begin explicit/background copied writeback; dirty, flushable, no no-flush barrier | `RESIDENT + WRITEBACK_IN_PROGRESS` | unchanged | unchanged; new pins remain allowed | remains dirty | failure returns to `RESIDENT + NONE`, still dirty |
| `RESIDENT + WRITEBACK_IN_PROGRESS` | stable writeback succeeds and copied generation is still current | `RESIDENT + NONE` | unchanged | unchanged | atomically clean, clear `rec_lsn`, remove DPT entry | not applicable |
| `RESIDENT + WRITEBACK_IN_PROGRESS` | stable writeback succeeds but a newer generation exists | `RESIDENT + NONE` | unchanged | unchanged | remains dirty with existing dirty-interval metadata; reflush is required for current durability | not applicable |
| `RESIDENT + WRITEBACK_IN_PROGRESS` | WAL, page-write, or file-sync failure | `RESIDENT + NONE` | unchanged | unchanged | remains dirty; `rec_lsn` is preserved | report the failure to flush waiters/caller; frame remains coherent and retryable |
| `RESIDENT + NONE` | CLOCK reserves an eligible victim after final recheck | `EVICTING + NONE` | old mapping remains but is marked non-pinnable/evicting | must be zero and remains zero | unchanged | not applicable |
| `EVICTING + NONE` | old page is clean; no requesting load remains | `FREE + NONE` | remove old mapping before identity reset | remains zero | reset clean | not applicable |
| `EVICTING + NONE` | old page is clean; requesting load intent still owns the reservation | `LOADING` for the new PageId | remove old mapping, fully reset, then bind the requester's in-progress entry before new read/construction | no public pin; new claims remain pending | reset clean | later load/publication failure follows its normal row |
| `EVICTING + NONE` | old page is dirty and flushable | `EVICTING + WRITEBACK_IN_PROGRESS` | old non-pinnable mapping remains | remains zero | remains dirty | failure follows the next row |
| `EVICTING + WRITEBACK_IN_PROGRESS` | stable eviction writeback succeeds; no requesting load remains | `FREE + NONE` | remove old mapping, then reset identity/metadata | remains zero | clear dirty/`rec_lsn`, then reset | not applicable |
| `EVICTING + WRITEBACK_IN_PROGRESS` | stable eviction writeback succeeds; requesting load intent still owns the reservation | `LOADING` for the new PageId | remove old mapping, fully reset, then bind the requester's in-progress entry before new read/construction | no public pin; new claims remain pending | clear old dirty/`rec_lsn`, then start new clean loading metadata | later load/publication failure follows its normal row |
| `EVICTING + WRITEBACK_IN_PROGRESS` | WAL, page-write, or file-sync failure | `RESIDENT + NONE` | restore the old mapping to pinnable resident state | remains zero until a fetch pins it | remains dirty with all recovery metadata | release victim reservation, wake old-page fetchers, and fail the requesting load intent with the same error |
| `RESIDENT` | file-retirement drain has blocked new pins and all pins/I/O/latches have drained | `FREE + NONE` | remove mapping under §7.12.5 | zero | obsolete object bytes may be discarded only after the retirement owner authorizes it | no unlink follows until the drain succeeds |

Every transition that changes the mapping, ownership state, PageId, public pin count, or victim reservation is atomic with respect to competing fetch/victim decisions. The architecture does not require one global mutex; it requires the table's observable outcomes.

### 7.6.3 Fetch linearization

A normal fetch first verifies through the active registered-file owner that the PageId is an allocated/published page address. It cannot join a private new-page publication intent merely by guessing its not-yet-published PageNo. The internal new-page path in §7.12.4 is the sole exception and owns that unpublished PageId until publication.

A successful fetch returns only after all of the following hold:

```text
the frame owns the requested PageId
the bytes are fully loaded or were already resident
the required validation in §7.6.4 has succeeded
the fetch owns one pin
the frame cannot be reassigned while that pin exists
```

The logical linearization points are:

| Fetch case | Linearization point |
|---|---|
| resident hit | the atomic successful pin increment after rechecking `RESIDENT`, file-active state, and absence of victim reservation |
| first successful miss/load | the atomic `LOADING -> RESIDENT` publication that assigns the loader's fetch claim one pin |
| waiter on that load | the same successful publication, which assigns that waiter's registered fetch claim one pin before wakeup |

A fetch that sees `EVICTING` cannot pin the old frame. It waits for that page-table entry to change and retries: eviction success leads to a miss/reload; eviction failure restores the same resident page and permits a hit. If a hit's pin increment wins before victim reservation, eviction must abandon that candidate. If victim reservation wins first, the fetch cannot acquire the old frame. This is the canonical fetch-versus-eviction race rule.

### 7.6.4 Validation before resident publication

Ordinary disk load has four validation layers:

1. exact 8192-byte positional I/O completes,
2. the complete-page CRC32C and universal common-header encoding are valid before stored `page_lsn` is trusted,
3. the page's embedded `page_no` equals the requested PageId's PageNo and the requested FileId still identifies the validated registered file,
4. the storage owner's file-kind/page-type validation policy accepts the complete page-specific structure.

The BufferPool owns layers 1–3. It remains format agnostic by invoking, rather than implementing, the registered storage owner's nonmutating layer-4 validator before publication. The validator dispatches from the already validated file kind/page type and MUST perform bounded validation without trusting corrupt counts or offsets. It validates physical page structure from the loaded bytes plus already registered immutable file-format context; it MUST NOT recursively fetch another page, perform query/MVCC interpretation, or require catalog I/O while the frame is `LOADING`. All fetches for one registered file use the same owning validation policy.

Failure at any layer is a load failure. Malformed bytes are never published as an ordinary `RESIDENT` page or exposed through a normal guard. Recovery may use a separate recovery-only read/reconstruction path for a torn page, but it MUST install and validate a reconstructed canonical page before publishing it for ordinary access.

Newly allocated pages do not read uninitialized disk bytes; §7.12.4 requires construction and validation of their canonical initialized image before resident publication.

### 7.6.5 Crash versus runtime state

Frame/page-table/pin/latch state is process-local and vanishes on crash. Durable correctness depends only on the WAL/data ordering of §§7.10–7.11 and Chapters 12–13. A process crash never requires reconstructing the former CLOCK position, load waiter set, pin count, or frame ownership state.

## 7.7 RAII page guards

Resident page lifetime and page-latch ownership use RAII-style guards.

The architecture distinguishes conceptual read/write guard capabilities equivalent to:

```text
ReadPageGuard
WritePageGuard
```

A guard:

- owns exactly one already-acquired pin while alive,
- acquires the appropriate page latch for its access mode,
- releases its latch before its pin during destruction/explicit release,
- provides access to the resident page bytes,
- for mutable access, participates in the §7.10.1 mutation-publication protocol.

Callers SHOULD NOT be required to manually pair raw `FetchPage`/`UnpinPage` operations across every success and error path.

Conceptually, a guard first receives a pin/residency claim and then acquires its page latch. It releases in reverse order. Code MUST NOT wait for a page latch without retaining the pin that prevents reassignment. BufferPool-internal load/victim/writeback reservations use their explicit state-machine claims rather than pretending to be public guards.

If latch acquisition or guard construction is canceled/fails after pin acquisition, that path releases the pin exactly once and exposes no guard or borrowed view.

One pin represents one outstanding caller lifetime claim on the current `(frame,PageId)` binding. Fetch acquires it at the §7.6.3 linearization point; the owning guard releases it exactly once. Pin counts use checked arithmetic and MUST NOT wrap. A pin-count overflow fails that fetch without changing the count.

`pin_count == 0` is necessary but not sufficient for replacement. State, I/O, latch/no-flush, retirement, and final victim-reservation checks also apply.

### 7.7.1 Pin and latch are different contracts

A pin protects frame residency and identity. It does not serialize access to page bytes.

A page latch protects in-memory page-byte interpretation and mutation:

```text
read guard:
    one pin + shared/read latch

write guard:
    one pin + exclusive/write latch
```

A read guard prevents concurrent mutation while it interprets bytes. A write guard excludes all other page-byte readers/writers while installing a mutation. Transaction locks remain distinct and MUST NOT be waited for while holding a short-lived page latch.

### 7.7.2 Guard and reference lifetime safety

Guards have single-owner scoped lifetime. Moving a guard transfers its pin/latch/release responsibility and leaves the source inert. Explicit early release is permitted and has the same effect as destruction.

A pointer, reference, span, tuple/VARCHAR view, page-controller view, iterator state, or other non-owning reference into a frame's page bytes MUST NOT outlive the guard that supplies the required latch and pin. It becomes invalid for further access immediately when that guard releases, even if unrelated callers still pin the frame.

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

The logical page table contains entries in one of these states:

```text
LOAD_INTENT(PageId, pending fetch claims, no frame yet)
LOADING(PageId, FrameId, pending fetch claims)
RESIDENT(PageId, FrameId)
EVICTING(PageId, FrameId)
```

On an absent-page miss, one fetch atomically installs the sole `LOAD_INTENT` and becomes loader. This intent is visible before victim selection or I/O, so another miss for the same PageId joins it instead of reserving or reading a duplicate frame.

Each fetch joining `LOAD_INTENT` or `LOADING` registers one pending fetch claim and waits without a public pin or page latch. Registration fails without joining if the number of pending claims could not later be represented by the pin count. When a frame becomes available, the loader binds it and publishes `LOADING(PageId,FrameId)` before starting the disk read. Waiters can distinguish that in-progress state from usable `RESIDENT` state.

On successful validation, one atomic publication:

```text
LOADING -> RESIDENT
pending uncancelled fetch claims -> one pin each
wake all waiters
```

This initial pin assignment prevents eviction between load publication and waiter wakeup. A waiter canceled before publication withdraws its claim; if cancellation races after pin assignment, it releases that assigned pin before returning cancellation.

On load failure, the loader closes the current in-progress operation with one captured error, removes it from the page table so no new fetch can join it, wakes all registered waiters with that same error, and returns the frame to `FREE`. The completion result may remain process-locally reachable by those already registered waiters after mapping removal. A later independent fetch may install a new intent and retry; failed pages are not permanently poison-cached.

The initial implementation MAY realize this logical table with conventional hash maps/condition variables protected by mutexes. It is not required to use one concrete container or lock. The abstraction MUST allow later partitioning while preserving the same one-loader, publication, pin, failure, and wakeup semantics.

There is never a public state in which a page-table entry names Page A while the frame identity/bytes belong to Page B. `EVICTING` retains A's non-pinnable mapping until A is durably handled and removed. Only after removal and complete frame reset may the frame bind to B as `LOADING`.

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

A replacement candidate must satisfy all of:

```text
ownership state == RESIDENT
pin_count == 0
I/O substate == NONE
no page latch holder or waiter (legitimate latch users retain pins)
no MTR/no-flush barrier
file is not already governed by another drain/reservation
victim reservation succeeds on final atomic recheck
```

The last reservation, not an earlier CLOCK observation, is the eviction linearization point.

## 7.10 Dirty pages and stable writeback

A frame's published dirty state is true exactly when its current published persistent-byte generation is not known to be durably installed in its page file. Bytes temporarily installed behind an owning no-flush/write-latch protocol are not a separately publishable frame state. A persistent mutation increments:

```text
modification_generation
```

For WAL-protected pages, §12.16 additionally defines the clean-to-dirty `rec_lsn`/full-image transition.

Dirty pages may be written because of:

- eviction,
- explicit flush,
- checkpoint/background writeback.

Commit does not require heap/index data-page writeback because v1 is NO-FORCE.

### 7.10.1 Persistent mutation publication

Acquiring a write guard alone does not make a page dirty. A successful persistent mutation is published while its write guard is held and only after the owning WAL protocol has produced the complete record/image needed for that mutation. The owning protocol decides whether it stages bytes until after WAL append or temporarily installs final bytes behind a no-flush barrier before append; §12.10.3 owns the B+ MTR ordering.

The canonical publication order is:

```text
1. acquire the write guard and the frame/DPT-transition reservation before
   deciding clean/FPI state; establish any owning no-flush barrier before
   bytes can differ from the last published generation
2. determine §12.10 clean-to-dirty/checkpoint-epoch image requirements,
   then construct/validate the final after-image and append the required
   PAGE_INIT/PAGE_IMAGE/delta/MTR WAL in the owning protocol's order;
   no changed bytes are flushable or externally published before the
   complete owning WAL record exists
3. after that complete WAL record exists, while still holding the page
   write latch and frame/DPT-transition reservation:
       install, or confirm the protected installation of, all after-image bytes
       install the owning page_lsn
       increment modification_generation exactly once for this mutation
       if clean -> dirty:
           publish dirty=true and rec_lsn=the required full-image LSN
           insert the DPT entry
       else:
           preserve the existing dirty-interval rec_lsn
       publish checkpoint/FPI epoch metadata
4. only then remove any owning no-flush barrier and release the frame/DPT
   reservation and write guard in the owning protocol's order
```

The clean-to-dirty and checkpoint-epoch full-image rules are §12.10; `TXN_STATUS` uses the specialized semantic-record ordering in §12.10.5 without exemption from this frame publication rule.

An owning no-flush barrier and `WRITEBACK_IN_PROGRESS` are mutually exclusive. A protocol such as B+ MTR that must install protected bytes before its WAL record exists first waits for any older copied writeback to finish, then acquires the no-flush reservation before changing bytes; it does not wait while holding a page latch needed by that writeback's copy/reconciliation. Once installed, the barrier rejects/skips new writeback reservations until the owning protocol publishes or enters its defined failure disposition.

If a protocol stages bytes until step 3, expected failure beforehand leaves page bytes/frame metadata unchanged. If an owning protocol permits protected pre-WAL byte installation, BufferPool MUST preserve its write latch/no-flush reservation and MUST NOT independently publish, flush, or clean those bytes when the owning operation fails. The owning protocol defines its own rollback/failure disposition; this BufferPool rule does not resolve or replace that protocol. No path may release an ordinarily usable frame with partially installed bytes lacking matching `page_lsn`, generation, dirty, and recovery metadata. Such an untracked publication is an internal invariant failure, not a clean operation error.

Mutable access cannot be silently released after changing persistent bytes without this publication. A concrete write-guard API may require an explicit commit/mark operation or provide a mutation closure; it MUST make an unreported persistent mutation an invariant violation.

The owning WAL/page mutation protocol chooses and installs `page_lsn`. BufferPool owns the frame dirty/generation/`rec_lsn`/DPT bookkeeping and does not invent a semantic page LSN independently.

### 7.10.2 Copied stable writeback

V1 uses copied writeback, not latch-held I/O. BufferPool never computes the durable checksum from bytes that may be changing concurrently.

For ordinary explicit/background flush, BufferPool reserves the frame's single writeback substate, then copies a stable 8192-byte image while holding the page's read latch. It records that image's `page_lsn`, `modification_generation`, and PageId, releases the latch, finalizes the checksum in the private copy, establishes WAL-before-data, writes and durably synchronizes that copy, then reconciles frame state.

A flush request for an already clean frame succeeds without I/O. If a writeback is already active, a background attempt skips/requeues the frame; an explicit current-contents flush joins that completion and then rechecks/repeats until the current generation is durably clean or an error/cancellation occurs. An explicit request encountering an MTR/no-flush barrier waits for that owning operation to publish or fail; background writeback skips the frame. Neither path bypasses the barrier.

While a `RESIDENT` copied writeback performs WAL/file I/O:

- the writeback reservation prevents another flush or eviction of that frame,
- existing/new fetches may pin it,
- readers may read it under read latches,
- writers may mutate it after the brief copy latch is released,
- no caller observes the private writeback buffer as resident bytes.

An eviction writeback differs: the frame is already `EVICTING`, has no pins/latch users, and permits no new ordinary access. Its generation therefore cannot change during I/O.

### 7.10.3 Stable completion and dirty-generation reconciliation

A writeback is **stable** for dirty-clearing/DPT purposes only after:

```text
exact 8192-byte pwrite succeeded
AND
fdatasync of the owning page file succeeded after that write
```

Multiple completed writes to one file MAY be covered by one later `fdatasync`, but no covered frame may be published clean or evicted before that synchronization succeeds. A mere successful `pwrite` into the operating-system cache is not sufficient to clear dirty state, remove a DPT entry, or permit WAL recycling based on that page.

After stable completion, dirty state can be cleared only if the frame still owns the copied PageId and its current `modification_generation` equals the copied generation.

If a newer mutation raced the I/O, the older stable image may still be a valid durable database page, but the resident frame remains dirty and keeps the appropriate dirty-interval recovery state.

An explicit “flush current contents” request repeats copied writeback until the then-current generation becomes stably clean or an error/cancellation occurs. A background one-generation writeback may finish successfully yet report/requeue that a newer generation still requires flush.

Exact checksum finalization is §4.12.2.

### 7.10.4 Flush failure

If WAL flush, page write, complete-transfer handling, or required `fdatasync` fails:

- the writeback reservation is released,
- the resident mapping and PageId remain unchanged,
- dirty remains true,
- `rec_lsn`, generation, FPI epoch, and DPT membership are preserved,
- a normal resident frame remains pinnable/retryable,
- a victim-reserved frame returns to `RESIDENT` and becomes pinnable again,
- callers waiting for that flush receive the structured storage/WAL error.

The page is never reassigned on failed eviction writeback. Even if some bytes reached the operating system or disk before the reported error, runtime state conservatively treats the frame as dirty.

This section does not choose the later SQL transaction effect of a storage error. It fixes the BufferPool state after failure; §39 owns upper error propagation.

### 7.10.5 DPT and checkpoint synchronization

Dirty publication, current-generation stable-clean publication, and checkpoint DPT capture use one conceptual synchronization order that makes these outcomes exhaustive:

- checkpoint captures a dirty page and its existing `rec_lsn`, even if a later flush makes it clean, or
- stable flush publishes the page clean first, so omission is safe because the corresponding data-file image is durable, or
- clean-to-dirty publication occurs after capture ordering, and §12.10/§13.5 ordering guarantees its full image remains in the checkpoint's retained WAL scan/range.

In particular, frame/DPT-transition synchronization begins before the clean-to-dirty full-image decision/WAL append and ends only after dirty/`rec_lsn`/FPI metadata are visible, as specialized for status pages in §12.10.5. Checkpoint cannot observe “clean” between that image append and frame/DPT publication.

Stable-clean reconciliation removes the DPT entry and sets `rec_lsn=INVALID_LSN` atomically with respect to checkpoint capture. A generation-mismatched writeback changes neither.

## 7.11 WAL-before-data enforcement

The canonical WAL-before-data ordering rule is §12.17.

The BufferPool flush path is its centralized enforcement point for BufferPool-managed pages. Before explicit flush, eviction, or background writeback, BufferPool checks the page's `page_lsn` against `WalManager.durable_lsn` and requests `flush_through(page_lsn)` when §12.17 requires it.

Storage objects MUST NOT each implement independent variants of this rule.

The WAL subsystem owns WAL persistence and `durable_lsn`; BufferPool owns enforcing the dependency before data-page writeback.

B+ mini-transactions additionally obey the stronger temporary no-flush condition defined in §12.10.2.

The canonical stable flush sequence is:

```text
1. reserve one writeback and copy PageId/generation/page_lsn/page bytes
   under the page read latch
2. finalize the private image checksum
3. establish WalManager.durable_lsn >= copied page_lsn when required
4. pwrite the complete private image
5. fdatasync the owning page file, individually or as part of a batch
6. reconcile dirty/DPT state under §7.10.3–§7.10.5
```

If step 3 fails, no data write is attempted. Failure at steps 4–5 leaves the frame dirty. A crash before step 3 durability leaves no data write from this attempt; a crash after step 3 but before stable page write leaves redoable WAL; a torn step-4 write is reconstructed from the retained complete image; a crash after step 5 but before runtime clean publication leaves a durable page and only loses conservative process-local dirty metadata.

## 7.12 CLOCK replacement

The baseline replacement policy is CLOCK.

A frame participates as an eviction candidate only when all §7.9 conditions hold. CLOCK's initial page-use test is:

```text
pin_count == 0
```

Accessing a frame sets its reference/use bit.

Successful resident-hit pin acquisition and successful load publication set the reference bit. `FREE`, `LOADING`, `EVICTING`, and any frame with I/O/no-flush state do not participate in victim selection.

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

### 7.12.1 Victim reservation and eviction

CLOCK observation alone does not own a victim. After choosing a candidate, BufferPool atomically rechecks §7.9 and changes `RESIDENT -> EVICTING` while the old page-table entry remains present and marked non-pinnable.

That transition is the victim-reservation linearization point. It prevents the race in which eviction observes zero pins while a concurrent fetch pins the old PageId.

For a clean victim:

```text
reserve EVICTING
remove old mapping
reset frame to FREE
```

For a dirty victim:

```text
reserve EVICTING
perform exclusive stable writeback through §7.10–§7.11
on success remove old mapping and reset to FREE
on failure restore RESIDENT, preserve dirty/recovery state, wake waiters
```

A fetch of the old PageId that observes `EVICTING` waits for this outcome and retries. It never reads or pins the victim while reserved.

An eviction writeback failure is not hidden by silently trying a different victim for the same load intent. The old page is restored as above; the requesting load operation and all of its current waiters receive that storage/WAL error, its in-progress mapping is removed, and a later independent fetch may retry. Losing a victim-reservation race before I/O is not an I/O failure and merely continues CLOCK selection.

### 7.12.2 Mapping removal and frame reassignment

Mapping removal and frame identity reset occur only after a clean victim needs no write or a dirty victim has completed stable writeback. The old mapping is removed before changing the frame's PageId or bytes.

The victim reservation belongs to exactly one requesting load operation. After successful eviction, BufferPool removes the old mapping, fully resets old identity/metadata, and binds the frame to that requester's `LOADING` entry as one reserved transition; another load cannot steal the frame in between. If the requester is canceled before binding, the reset frame instead becomes ordinary `FREE`.

Before reassignment, BufferPool resets all old-page semantic metadata, including:

```text
PageId
pin/fetch claims and reservations
dirty and rec_lsn
page_lsn tracking derived from resident bytes
old-identity modification-generation comparison tokens
checkpoint/FPI epoch state
checksum/writeback generation state
no-flush/I/O state
reference/replacement metadata
```

No page latch holder/waiter may survive this reset. The reset boundary is complete before the new PageId is installed. It need not be externally observable as `FREE` when the existing reservation transfers directly to `LOADING`; implementations may combine bookkeeping steps under one synchronization region but MUST preserve old-mapping removal, reset, then new binding in that order.

The numeric `modification_generation` itself remains monotonically increasing for that frame or is replaced by an equivalently nonrepeating identity-plus-generation token. It is not reset in a way that could make any stale completion compare equal to a later residency.

### 7.12.3 No eligible frame

One complete CLOCK attempt that finds no reservable `FREE`/victim frame returns the ordinary `NO_REPLACEABLE_FRAME` resource result to the load operation and all its current waiters. It does not steal a pinned/reserved frame or wait indefinitely while the caller may itself hold the only releasable pins. A caller may release guards and retry. A separately named cancellable/waiting convenience API may be added later but cannot weaken victim eligibility.

### 7.12.4 Newly allocated pages

`PageFile`/`DiskManager` append allocation produces a new PageId; it does not itself publish resident contents. The allocating owner installs the sole page-table load intent, obtains a frame through the same reservation path, and binds that frame as `LOADING` without reading uninitialized disk bytes.

The owner then follows §4.11.1: constructs the complete initialized image, appends its `PAGE_INIT` (or B+ MTR), installs `page_lsn`/checksum and frame recovery metadata, and validates the canonical image while the frame remains non-usable `LOADING`. For an append-count-governed page, advancing the owning `published_page_count` and changing the frame to `RESIDENT` with a pin for the creator are one publication event with respect to ordinary scans/fetches. A B+ page remains additionally unreachable from tree traversal until its owning MTR publication rule permits it. The PageId cannot be fetched generally before the owning publication contract permits it.

Failure before publication removes the in-progress mapping and returns the frame to `FREE`. Physical append-tail reconciliation remains §4.11.3; this state machine does not silently invent rollback/reuse of an unpublished appended PageNo.

### 7.12.5 File retirement and drain

For fetch/drain purposes, each BufferPool-visible registered FileId is conceptually:

```text
ACTIVE     permits load intents and resident-hit pin acquisition
RETIRING   forbids new intents/pins while existing ownership drains
CLOSED     has no page-table entry, frame, pin, or I/O ownership
```

Before §4.7.7 may close/unlink a FileId, its storage owner atomically changes that FileId from `ACTIVE` to `RETIRING` at the BufferPool boundary. That transition is the retirement-gate linearization point. After it:

- new load intents and new pins for that FileId fail with `FILE_RETIRED_OR_CLOSING`,
- in-progress loads are not published and complete as retired/closing failures,
- existing guards are allowed to finish and their pins/latches drain,
- existing writeback/victim operations finish or fail and release their reservations,
- no frame for the FileId may remain usable before file-handle close/unlink.

After the semantic retirement owner has established that the physical object is obsolete and no committed state requires its contents, drained dirty frames for that FileId are discarded without writeback; writing them would only recreate obsolete data before unlink. This discard permission applies only to that proven retirement/orphan path, not ordinary file close or shutdown.

The retirement gate then removes mappings and any corresponding DPT entries under the same frame/DPT synchronization, resets frames, changes the FileId gate to `CLOSED`, closes managed file ownership, and permits the durable unlink protocol in §4.7.7. If drain cannot complete, `CLOSED`/unlink does not begin.

### 7.12.6 Controlled BufferPool shutdown

On controlled shutdown, BufferPool enters a quiescing state that rejects new external fetch/new-page operations, lets already issued guards and internal I/O drain, and performs stable flushes for dirty non-retired pages when the database owner requests a normal durable shutdown.

Flush/drain failure is reported to the database lifecycle owner and the BufferPool MUST NOT claim a successful clean shutdown or silently discard dirty pages whose persistence remains required. Process-crash shutdown instead relies on WAL recovery. The complete database boot/open/shutdown state machine is owned separately; this subsection fixes only BufferPool-local behavior.

### 7.12.7 BufferPool error categories

BufferPool operations preserve distinctions equivalent to:

```text
FILE_OR_PAGE_NOT_FOUND
RAW_IO_FAILURE
CORRUPT_PAGE
NO_REPLACEABLE_FRAME
WAL_DURABILITY_FAILURE
FILE_RETIRED_OR_CLOSING
PIN_COUNT_OVERFLOW
BUFFERPOOL_QUIESCING
INTERNAL_INVARIANT_FAILURE
```

The first eight are explicit operation/storage errors in their applicable contexts; none is converted into successful fetch/flush/publication. An internal invariant failure means frame/page-table ownership has become contradictory and is not an ordinary retry result. Whether a storage error aborts a SQL statement/transaction or places the whole database in a failed state is the separate §39 upper-layer policy.

## 7.13 I/O and buffer invariants

1. Page I/O is positional and does not rely on a shared file offset.
2. A normal page read/write transfers exactly `PAGE_SIZE` bytes or reports an error.
3. A failed read never exposes a partial destination as a valid page.
4. `pread`, `pwrite`, `fstat`, `ftruncate`, `fdatasync`, and `fsync` handle `EINTR`; `close` is not blindly retried.
5. Ordinary page writes never allocate or sparsely extend files.
6. Page-file sizes are exact multiples of `PAGE_SIZE`.
7. Physical page-offset arithmetic is checked before I/O.
8. V1 page-file durable synchronization uses `fdatasync`.
9. `FileId` is not an operating-system descriptor.
10. BufferPool owns page load intents, the `PageId -> frame` binding, and page lifetime.
11. Frame-management metadata is not persisted inside page bytes.
12. One PageId has at most one active load and at most one usable resident frame per BufferPool.
13. A failed load publishes no resident bytes, wakes its current waiters with one failure, removes the failed intent, and permits a later retry.
14. `pin_count > 0` forbids eviction/frame reuse; zero pins alone do not authorize it.
15. One page guard owns one pin; pin protects residency, while latch protects page-byte access.
16. References into resident page bytes do not outlive their protecting guard.
17. Page-specific structural validation succeeds before ordinary resident publication.
18. Every persistent resident-page mutation increments `modification_generation` and publishes matching dirty/recovery metadata before releasing its write latch.
19. A stable flush image is copied under the page latch and checksum-finalized outside mutable frame bytes.
20. Stable writeback requires complete page write plus owning-file `fdatasync`; `pwrite` alone cannot publish clean state.
21. Successful stable writeback clears dirty/`rec_lsn` only when PageId and copied generation are still current.
22. Every flush failure preserves dirty/`rec_lsn`/DPT state and forbids reassignment based on the failed write.
23. NO-FORCE means commit does not require writing dirty heap/index data pages.
24. Before writing a WAL-protected page image with `page_lsn=X`, BufferPool ensures `durable_lsn >= X`.
25. MTR no-flush state overrides ordinary flush eligibility.
26. CLOCK considers only fully eligible unpinned resident frames and gives referenced frames a second chance.
27. Victim reservation wins atomically before old-page pins are blocked; dirty victims are stably flushed before frame reuse.
28. Old mapping removal precedes PageId/byte reassignment, and all old metadata is reset.
29. DPT capture, dirty publication, and stable-clean publication cannot lose a clean-to-dirty transition.
30. File retirement blocks new pins/loads and drains every frame before close/unlink.
31. Regular-file `fdatasync` and parent-directory `fsync` have distinct content and namespace durability responsibilities.
32. A file create/rename/unlink is not durably published until the owning §4.7 directory synchronization succeeds.

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

This strict decoder rule is part of the persisted v1 architecture contract.

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

Leaf-node flag/reserved validation follows the canonical B+ node rules in §8.7.

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

Internal-node flag/reserved validation follows the canonical B+ node rules in §8.7.

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

The canonical v1 transaction/recovery architecture establishes: ordinary user-DML heap/index modifications are not physically undone on user abort.

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

The page's common `page_lsn` records the newest WAL record physically reflected in the page. After a successful terminal update this is that update's `TXN_COMMIT`/`TXN_ABORT` LSN. During recovery it may temporarily be a system `PAGE_INIT`/`PAGE_IMAGE` LSN before the later semantic terminal record is applied, as defined by §12.10.5.

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

The integrated COMMIT sequence is canonical in §15.5.

This chapter owns the transaction-lifecycle requirement that terminal COMMITTED state is published through §9.14.1's linearization point only after the required durable commit WAL exists. The transaction-status page remains NO-FORCE; durable commit WAL is authoritative after crash.

The physical transaction-status page mutation follows the full-image/terminal-record protocol in §12.10.5. Installing the status bits in a resident page is not the runtime terminal-publication linearization point.

### 9.14.3 Abort

The integrated ABORT sequence is canonical in §15.6.

This chapter owns the transaction-lifecycle requirement that ABORTED publication uses §9.14.1's terminal linearization point before transaction-lifetime locks are released.

An ordinary abort does not require immediate abort-WAL `fdatasync` merely to acknowledge abort. If abort WAL is lost in a crash, recovery treats the transaction as a loser and establishes ABORTED again.

The physical transaction-status page mutation follows §12.10.5 even when the terminal record is a recovery-generated `TXN_ABORT`.

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

Durability of that WAL and of newly created database objects also includes the filesystem namespace prerequisites in §4.7 and §12.2.1. Regular-file synchronization alone cannot satisfy an acknowledgement whose required filename may disappear after crash.

Ordinary user-DML heap/index changes use redo plus transaction-status visibility; they do not require physical user-DML undo.

## 12.2 Logical WAL stream and physical segments

The database has one logical WAL byte stream.

It is physically stored in fixed-size files under:

```text
wal/
```

with the segment naming shape:

```text
0000000000000000.wal
0000000000000001.wal
...
```

The basename is exactly the unsigned segment index encoded as 16 lowercase hexadecimal digits followed by `.wal`. Shorter, uppercase, signed, decimal, or alternate spellings are not v1 segment names.

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

WAL segmentation provides bounded file units, recovery scan boundaries, and a natural recycling unit.

### 12.2.1 WAL segment namespace and creation

WAL segments use direct final names under the already-durable `database_root/wal/` directory. They do not use DDL `pending/` names because their publication authority is the WAL writer's logical stream and `durable_lsn`, not catalog visibility.

V1 segments have no separate segment header. A fresh segment's logical contents begin as all zero bytes, which are valid unwritten/tail bytes under §12.3.

Creation of segment index `S` is serialized by the WAL segment/append owner and uses:

```text
1. require S to be the next contiguous segment index
2. create its exact final basename with exclusive no-follow semantics
3. ftruncate the fresh file to exactly 67,108,864 bytes
4. fdatasync the segment so its zero initial contents and length are durable
5. fsync database_root/wal
6. only after step 5 mark the segment namespace-durable and permit WAL
   bytes in S to participate in any durability request
```

Directory synchronization is therefore immediate for each newly created WAL segment, not deferred until an arbitrary later commit. It occurs once per creation; ordinary later WAL flushes do not resynchronize the directory unless another namespace mutation occurred.

Writing/acknowledging records in that segment then uses:

```text
7. pwrite complete contiguous WAL bytes
8. fdatasync the segment through the requested record/span
9. only after step 8 advance durable_lsn into S and wake durability waiters
```

No record in `S`, including `TXN_COMMIT`, is durable by architecture definition before both the segment's step-5 namespace publication and the step-8 content synchronization covering that record have succeeded.

Segment-creation failure or `fsync(wal_directory)` failure prevents `durable_lsn` from entering the segment and is propagated as WAL I/O failure. A possibly surviving empty/partial segment is not evidence of durable WAL beyond the prior segment.

After recovery, an exact-size next-contiguous all-zero segment may be adopted for future append only after validating its zero state, `fdatasync`ing it, and `fsync`ing `wal/` again. This reestablishes both content and namespace durability without assuming whether its precrash creation sync completed.

Crash outcomes are:

- before create: the prior segment prefix is the complete available WAL namespace,
- after create but before directory sync: the unacknowledged segment may be absent or present after restart; no durable outcome depended on it,
- after directory sync but before WAL write: the durable all-zero segment is an empty trailing segment,
- after WAL write but before segment `fdatasync`: recovery accepts only the complete valid prefix that actually survived; no waiter was acknowledged for the unsynchronized suffix,
- after segment `fdatasync` but before in-memory `durable_lsn` publication: the segment name and synchronized valid WAL prefix are durable and recovery uses them, although no waiter was yet acknowledged,
- after `durable_lsn` publication/acknowledgement: both the segment name and acknowledged WAL bytes survive under the required filesystem contract.

WAL-segment unlink/recycling follows §4.7.7 and §13.10.

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

These zero-payload records are the semantic evidence for terminal transaction outcome. They are not physical full-page images. When reflecting either outcome into a `TXN_STATUS` page requires a full image, the separate system `PAGE_IMAGE`/`PAGE_INIT` protocol in §12.10.5 supplies the physical reconstruction base.

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

The `PAGE_INIT` or `PAGE_IMAGE` that supplies a `TXN_STATUS` recovery base under §12.10.5 is such a system record. It does not become part of the terminating user's per-transaction WAL chain.

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

### 12.10.5 TXN_STATUS full-image and terminal mutation protocol

`TXN_STATUS` pages are ordinary WAL-protected pages for full-image, checksum, dirty-page, checkpoint, and WAL-retention purposes. They are not exempt from §12.10.

The two WAL roles are deliberately separate:

```text
PAGE_INIT/PAGE_IMAGE   = physical page reconstruction base
TXN_COMMIT/TXN_ABORT   = semantic terminal-outcome evidence
```

A status-page full image may preserve terminal bits whose semantic terminal records were established earlier and have since become recyclable. It MUST NOT establish the **new** terminal outcome for the update that caused the image: that pending outcome appears only in the later `TXN_COMMIT`/`TXN_ABORT` record and its subsequent page mutation.

#### 12.10.5.1 Canonical mutation order

Terminal updates to one resident status page are serialized by that page's write latch. The latch covers WAL record ordering and installation of the corresponding status bits so that status-page `page_lsn` advances in the same order as mutations reflected in the page.

For one terminal update at terminal-record LSN `T`:

```text
1. locate/pin the target status page; acquire its write latch and the
   frame/DPT-transition synchronization used by checkpoint capture
2. determine whether §12.10 condition A or B requires a full image
3. if a full image is required:
       reserve image LSN F
       construct a complete image of the current pre-terminal page contents
       set the image's embedded page_lsn = F
       finalize the image checksum
       append a complete system PAGE_IMAGE record at F
4. append the semantic TXN_COMMIT or TXN_ABORT record at T
5. only after the complete terminal record exists in WAL:
       install the target two-bit terminal state
       set the resident status-page page_lsn = T
       update dirty/frame metadata as §12.10.5.2 requires
6. release the frame/DPT-transition synchronization and page write latch/pin
```

When step 3 occurs:

```text
F < T
PAGE_IMAGE.txn_id       = 0
PAGE_IMAGE.prev_txn_lsn = 0
```

The terminal record remains on the user transaction's WAL chain and its `prev_txn_lsn` names the prior **user** WAL record, not `F`.

The full image contains every status already reflected in the page before this terminal update, but it contains neither this update's terminal bits nor any other semantic evidence for this terminal outcome. Its changed embedded `page_lsn=F` makes it the canonical physical after-image of the reconstruction-base installation.

If appending the terminal record fails after the system image was appended, the status bits, resident `page_lsn`, dirty state, `rec_lsn`, and FPI-epoch state remain unmodified by this attempt. The standalone pre-terminal image is redo-safe and semantically inert; a later attempt reevaluates the full-image conditions normally.

The frame/DPT-transition synchronization is held from before step 2 through the dirty/`rec_lsn`/FPI-epoch publication in step 5. Checkpoint DPT capture uses the same synchronization. Therefore a checkpoint either:

- captures before this transition begins, in which case `F`/`T` are ordered after that checkpoint's BEGIN and remain inside its retained WAL range, or
- captures after the completed transition and includes the dirty page with its existing `rec_lsn`.

It cannot omit a status page whose dirty-interval image precedes the checkpoint retention floor merely because the image was appended while the frame still appeared clean.

A newly allocated status page uses a system `PAGE_INIT` at LSN `I` as its complete reconstruction base before any terminal record for that page. The first terminal record has `I < T`; `rec_lsn=I`, and installing that terminal update advances `page_lsn` to `T` while preserving `rec_lsn`. While that initialized frame remains dirty, `PAGE_INIT` satisfies the checkpoint FPI epoch in which it was emitted; an immediately following terminal update does not require a redundant `PAGE_IMAGE` unless §12.10 condition B has independently become true.

#### 12.10.5.2 Frame metadata, repeat updates, and durability

If the page was clean and step 3 emitted image LSN `F`, successful installation at step 5 performs:

```text
dirty    = true
rec_lsn  = F
page_lsn = T
```

and records that the current checkpoint FPI epoch has been satisfied for the page.

If the page was already dirty:

- a later terminal update normally appends only its terminal record and preserves the existing `rec_lsn`,
- if the first post-checkpoint-epoch modification requires a new image, that image captures all status updates currently reflected in the page, the existing dirty-interval `rec_lsn` is still preserved, and the new epoch is marked satisfied only after step 5 succeeds,
- every successful terminal update sets `page_lsn` to its terminal-record LSN and increments the frame's persistent modification generation.

After a stable-image page write successfully makes the frame clean under §4.12.2/§7.10, `rec_lsn` becomes `INVALID_LSN`. The next terminal update begins a new dirty interval with a new system full image and a new `rec_lsn`.

COMMIT MUST establish durable WAL through `T` before §9.14.1 runtime COMMITTED publication and success return. Ordinary ABORT need not synchronously flush `T`, but the status page cannot be written before WAL is durable through its current `page_lsn`. Thus §12.17 WAL-before-data applies without exception.

Until §9.14.1 terminal publication, the transaction remains nonterminal in the active registry and that runtime state dominates persistent-page lookup under §9.13. Resident status-bit installation before COMMIT's durability wait therefore does not publish COMMITTED to concurrent transactions.

#### 12.10.5.3 Checkpoint, redo, and retention consequences

Checkpoint FPI epochs apply to status pages exactly as to other ordinary pages. The first protected status mutation after the latest completed epoch emits a system full image even if the page is already dirty; the frame keeps its older dirty-interval `rec_lsn`.

Recovery processes the records in WAL LSN order:

- a trusted intact status page skips a page image or terminal update only when its `page_lsn` proves that record and all earlier serialized status-page mutations are already reflected,
- an older trusted page applies the retained `PAGE_INIT`/`PAGE_IMAGE`, then later terminal records,
- a torn/corrupt status page does not contribute a trusted `page_lsn`; recovery restores the retained complete image and then applies later terminal records,
- applying an image without a following terminal record leaves the target transaction nonterminal in that image; analysis/loser resolution establishes ABORTED when required,
- after an image has been applied but a later terminal record has not, terminal redo applies the two-bit outcome and sets `page_lsn` to the terminal-record LSN.

Redo of a terminal record maps its TxnId to the canonical §9.12 status page/bit field, writes exactly the semantic terminal code, and sets that page's `page_lsn` to the terminal-record LSN. It does not reinterpret the preceding full image as terminal evidence.

The DPT `rec_lsn` for a dirty status page therefore always identifies a retained complete image (`PAGE_INIT` or `PAGE_IMAGE`). WAL recycling MUST retain that image and every later required terminal record until a checkpoint/data-page state makes them unnecessary under §13.10. Once a status page is durably clean with a valid checksum and an installed checkpoint permits older WAL recycling, its disk image is the complete recovery base; any later dirty interval emits a new image containing all retained status bits. Ancient terminal records are therefore not required solely to reconstruct a later dirty interval.

Status pages wholly below the durable transaction-status reclamation cutoff retain the skip/retirement semantics of §13.12–§14.14; this protocol does not recreate retired sparse status history.

#### 12.10.5.4 Canonical crash outcomes

For one terminal update that requires image `F` followed by terminal record `T`, recovery outcomes are:

1. Crash before `F` is appended: no status-page mutation exists; the transaction remains nonterminal and loser resolution establishes ABORTED if needed.
2. Crash after `F` is appended but before `T`: replaying `F` restores only the pre-terminal page; it cannot publish a terminal outcome, and loser resolution establishes ABORTED.
3. Crash after `T` is appended but before an explicit WAL flush: the resident page cannot have reached its data file unless WAL-before-data first made `T` durable. If `T` is absent from the valid durable WAL tail, the transaction is a loser; if `T` survived as a complete valid record, recovery applies its semantic outcome.
4. Crash after `T` is durable but before the status bits are installed: redo applies `T` to the image/base page and establishes the terminal outcome.
5. Crash after status-bit installation but before page flush: redo applies `F`/the earlier retained base and then `T` as necessary.
6. Crash during a torn status-page write: checksum failure makes the stored `page_lsn` untrusted; redo restores the retained image and applies later terminal records.
7. Crash after a successful status-page flush: WAL-before-data guarantees durability through the flushed `page_lsn`; a trusted page skips already-reflected image/terminal records.
8. Crash after checkpoint installation and permitted WAL recycling: a still-dirty page retains its image through its DPT `rec_lsn`; a clean page's valid data-file image is the base, and its next dirty interval cannot begin without a new image.

These outcomes apply equally to ordinary `TXN_ABORT` and recovery-generated `TXN_ABORT`; only COMMIT additionally requires durable `T` before runtime terminal publication and success return.

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

If `X` lies in a segment created during this process lifetime, this statement also proves that segment's final directory entry was synchronized under §12.2.1 before `durable_lsn` advanced into it.

A durability request spanning a segment boundary synchronizes every segment containing previously unsynchronized bytes in the requested logical prefix. Synchronizing only the newest segment cannot advance `durable_lsn` past unsynchronized bytes in an older segment.

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

If that terminal record resides in a newly created segment, “durable” includes successful prior synchronization of the segment's `wal/` directory entry under §12.2.1.

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

For `TXN_STATUS`, the full-image LSN and later semantic terminal-record LSN are intentionally distinct as specified by §12.10.5: `rec_lsn` names the image while `page_lsn` advances to the latest terminal record reflected in the page.

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

The complete copied-image, file-synchronization, generation-reconciliation, and failure state machine is canonical in §§7.10–7.11. WAL durability is established before `pwrite`; dirty/DPT clean publication occurs only after the covering page-file `fdatasync` succeeds.

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
21. A `TXN_STATUS` dirty interval has a retained system full-image reconstruction base; `TXN_COMMIT`/`TXN_ABORT` remain separate semantic terminal evidence.
22. Status-page WAL append and page mutation are serialized so a later status-page `page_lsn` implies that all earlier same-page terminal updates are reflected.
23. A WAL segment is namespace-durable before any record inside it may advance `durable_lsn` or satisfy commit/WAL-before-data.
24. Segment creation uses an exact final basename and immediate `fsync(database_root/wal)`; segment content durability still requires `fdatasync` through the requested record.

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

BufferPool dirty publication, stable-clean removal, and this capture use the canonical §7.10.5 frame/DPT-transition synchronization. Capture may conservatively retain an entry for a page made durably clean immediately afterward; it MUST NOT miss a clean-to-dirty transition whose recovery image would otherwise precede the installed retention range.

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

Once a segment is wholly older than every retention requirement, physical recycling is:

```text
unlinkat(database_root/wal, exact_segment_basename)
fsync(database_root/wal)
```

Only the successful directory sync makes segment removal durable. A crash after unlink but before that sync may make the old segment reappear; startup ignores/re-unlinks such a segment only after proving it is wholly below the installed retention floor. Reappearance consumes space but cannot extend the valid logical WAL range or override a newer segment index.

## 13.11 Recovery startup and WAL-tail validation

Startup:

1. opens/validates the final database root plus required `pending/` and `wal/` directories under §4.7,
2. inventories exact managed pending/final object basenames without yet treating final-file existence as catalog ownership,
3. validates required bootstrap singleton/system-relation names and both control slots, then chooses the highest usable control generation,
4. validates the referenced installed checkpoint sequence if nonzero,
5. validates the exact retained WAL segment-name sequence and scans WAL forward to identify the last complete valid record,
6. validates record length, alignment, segment containment, zero padding, embedded LSN, flags/reserved rules, and CRC,
7. stops at the first invalid/incomplete/torn **tail** record,
8. logically discards/zeroes bytes after the last valid WAL record while preserving the fixed 64 MiB segment-file length,
9. reconciles unpublished page-file append tails per §4.11.3 during recovery,
10. completes final ownership/orphan reconciliation at the §13.19 recovery gate.

Every segment needed from the installed recovery start through the valid WAL tail has its exact §12.2 basename and no missing interior segment index. An extra next-contiguous all-zero segment durably created under §12.2.1 is an empty tail, not corruption. Segments proven wholly older than the installed retention floor may be absent or may be re-unlinked if a precrash unlink was not directory-durable.

A next-contiguous empty/short segment left by a failed creation attempt, containing no valid WAL record beyond the prior durable logical end, is an unacknowledged namespace artifact: recovery may unlink/synchronize and recreate it through §12.2.1. A malformed, short, or missing segment inside the required retained WAL range is corruption.

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

- a redoable page record, including a system status-page full image from §12.10.5, inserts/updates DPT information without replacing an earlier required `rec_lsn`,
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

Recovery MUST NOT fail merely because WAL references a missing FileId that is later proven to belong only to uncommitted/private DDL. Redo for an unresolved non-bootstrap FileId may be deferred until catalog ownership is known. It is skipped only when §4.7.6 proves the target unpublished/orphaned; if committed catalog or bootstrap state requires the file, absence is corruption.

### 13.13.2 Terminal status redo

`TXN_COMMIT` and `TXN_ABORT` are also redoable terminal-status evidence.

When encountered anywhere in the redo scan, including before CHECKPOINT_BEGIN, recovery repairs the corresponding transaction-status entry if its page does not yet reflect that terminal record **and** the TxnId is not below `database.control.txn_status_reclaim_before`.

The canonical physical base and ordering are §12.10.5. A retained system `PAGE_INIT`/`PAGE_IMAGE` reconstructs an older, missing, or untrusted status page before a later terminal record is applied. Terminal redo maps the record's TxnId through §9.12, installs exactly `COMMITTED` or `ABORTED`, and sets the status-page `page_lsn` to the terminal-record LSN.

For an intact trusted status page, the same-page serialization rule permits `page_lsn >= terminal_record.lsn` to skip that terminal update. A torn/corrupt page's stored LSN is never used for this decision.

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

For a `TXN_STATUS` page, the applicable retained complete image is the system `PAGE_INIT`/`PAGE_IMAGE` required by §12.10.5; later `TXN_COMMIT`/`TXN_ABORT` records reestablish semantic outcomes. Recovery does not depend on retaining every ancient terminal record that preceded that complete image.

If WAL-retention/checkpoint invariants say the required full image should exist but no valid recoverable image is available, recovery reports unrecoverable corruption.

It MUST NOT guess page contents or trust a torn page's LSN.

This reconstruction is the explicit recovery-only exception to ordinary BufferPool load publication in §7.6.4. Recovery may hold untrusted bytes privately while finding a full image, but the resulting page must pass universal and owning-format validation before it becomes an ordinary `RESIDENT` page.

## 13.15 Recovery phase 3: loser resolution

For each crash-loser **user** transaction:

```text
terminal outcome = ABORTED
```

No heap/index byte-by-byte undo is performed.

Before normal SQL traffic begins, recovery:

```text
1. applies §12.10.5 for unresolved losers, appending any required
   system status-page image before each page's later recovery TXN_ABORT records
2. updates the relevant transaction-status entries to ABORTED
3. assigns each transaction-status page_lsn from the latest terminal WAL
   record reflected in that page
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

During recovery, terminal records may repair/update stale transaction-status pages even when their prior data-file image did not include the status change. The physical reconstruction base, record ordering, and FPI/checkpoint behavior are canonical in §12.10.5.

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
every bootstrap/committed-catalog physical file present at its exact durable final name
pending and unowned managed-final files classified for durable orphan cleanup
```

Pending entries are never required for ONLINE state and may be unlinked before open completes. An unowned final orphan may be removed immediately or queued for the same durable unlink protocol, but it cannot be opened as a committed object. Unknown non-managed directory entries are not removed automatically.

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

For committed CREATE DDL this guarantee includes continued presence of every required final object-file directory entry. For a terminal record in a rotated WAL segment it includes continued presence of that segment entry. Sections §4.7 and §12.2.1 establish those namespace prerequisites before acknowledgement.

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
21. A retained dirty `TXN_STATUS` page is reconstructible from its system full-image `rec_lsn` plus later semantic terminal records; terminal records are not substitutes for the image.
22. A transaction acknowledged committed remains logically committed across every later successful crash/restart.
23. Every physical file required by bootstrap or committed catalog state exists at its exact durably synchronized final name before ONLINE.
24. Missing recovery targets are skipped only after proving that no bootstrap/committed catalog owner requires them.
25. WAL-segment creation and recycling include the required `wal/` directory synchronization; reappearing recycled segments below the retention floor do not extend the logical WAL stream.

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

A transaction that created DDL physical resources MUST first satisfy the §21.5 durable final-name prerequisite for every file referenced by its prospective committed catalog state.

```text
1. state ACTIVE -> COMMITTING
2. while the transaction remains nonterminal in the active registry,
   execute the §12.10.5 status-page protocol under the page write latch:
       append a system PAGE_IMAGE first if the dirty/FPI-epoch rule requires it
       append TXN_COMMIT(txn_id, prev_txn_lsn) at commit_lsn
       install COMMITTED and status-page page_lsn = commit_lsn
3. submit commit_lsn to CommitCoordinator
4. wait until durable_lsn >= commit_lsn
5. execute the §9.14 runtime terminal-publication linearization
6. release TUPLE_WRITE and UNIQUE_KEY locks
7. release shared TableWriterGate holdings
8. unregister the transaction's own active snapshot(s)
9. finish transaction object cleanup
10. return success
```

Commit does **not** force dirty heap/index pages.

Once a valid TXN_COMMIT record is durable, that transaction outcome cannot subsequently become ABORTED.

## 15.6 ABORT

For an abortable transaction:

```text
1. state ACTIVE/eligible transient state -> ABORTING
2. if persistent WAL-visible state exists:
       execute the §12.10.5 status-page protocol under the page write latch:
           append a system PAGE_IMAGE first if the dirty/FPI-epoch rule requires it
           append TXN_ABORT at abort_lsn
           install ABORTED and status-page page_lsn = abort_lsn
3. execute the §9.14 runtime terminal-publication linearization
4. release logical locks
5. release shared TableWriterGate holdings
6. unregister the transaction's own snapshots
7. finish transaction object cleanup
8. return/raise abort
```

Ordinary abort performs no write-set scan to restore old heap/index bytes.

Aborted physical versions and index entries remain vacuum input.

A transaction whose COMMIT is already durable is no longer eligible to transition to ABORTED.

Ordinary ABORT does not wait for `abort_lsn` durability before runtime ABORTED publication, but §12.17 prevents its dirty status page from reaching disk before WAL is durable through that page's current `page_lsn`. If the abort record is lost in a crash, loser resolution repeats the canonical ABORT protocol.

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

Physical DML applies the stronger §31.9 publication rule: no partial RETURNING prefix is exposed from a statement that later fails, so external emission begins only after successful statement completion.

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

Their canonical semantic fields are:

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

For v1, `default_expr_blob/reference` contains or references the §21.12.1 `DefaultValueBlob` produced after complete immutable-default folding; it is not a serialized runtime expression object.

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

`sys_statistics` stores versioned statistics records using one logical row shape:

```text
table_id
scope_kind
scope_id
stats_txn_id
stats_command_id
chunk_index
chunk_count
payload_fragment
```

The v1 scope-kind codes are:

```text
1 = TABLE
2 = COLUMN
3 = INDEX
```

`scope_id` is one semantic uint64 field:

```text
0                    for TABLE scope
zero-extended ColumnId for COLUMN scope
IndexId                for INDEX scope
```

`(stats_txn_id, stats_command_id)` is the exact `StatsVersion` defined by Chapter 34.

One scope payload may span multiple catalog rows. `chunk_index` is zero-based, `chunk_count` is the total number of chunks for that scope payload, and `payload_fragment` is an arbitrary byte string stored through the v1 binary-VARCHAR semantics.

V1 limits each statistics payload fragment to:

```text
STATISTICS_CHUNK_BYTES = 4096
```

except that the final fragment may be shorter.

All chunks of one scope/version are transaction-owned catalog rows and become visible atomically through the owning transaction's normal MVCC commit semantics.

Chapter 34 owns the exact payload grammar, completeness rules, and descriptor-publication semantics.

The exact physical heap-tuple layout of the system relation itself may evolve through catalog schema versions.

Its semantic fields and stable identity relationships are architectural.

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

V1 uses one database-wide bootstrap file at the well-known database-root path:

```text
catalog.dat
```

Its only purpose is to locate and minimally interpret the six self-hosted catalog relations.

It is **not** the ordinary catalog row store.

The file is exactly:

```text
page 0    FileSuperblock with FileKind::CATALOG
page 1    one immutable CATALOG_DATA bootstrap page
```

V1 does not append ordinary catalog rows to `catalog.dat`.

### 16.9.1 CATALOG superblock identity

The `catalog.dat` FileSuperblock uses:

```text
file_kind   = CATALOG              // persisted code 4
object_id   = 0
header_size = 72
```

`file_id` is one ordinary nonzero FileId allocated through the durable FileId allocator.

Because `catalog.dat` is found by its well-known singleton path, startup can read that FileId from the validated superblock before opening the system-relation files named by the bootstrap page.

### 16.9.2 CATALOG_DATA bootstrap-page format

The bootstrap page is exactly PageNo `1`.

Its common header is:

```text
page_type      = CATALOG_DATA      // persisted code 6
format_version = 1
flags          = 0
page_lsn       = INVALID_LSN
header_size    = 64
reserved16     = 0
page_no        = 1
```

The page-specific bytes are:

| Offset | Size | Field / v1 meaning |
|---:|---:|---|
| `32` | 8 | magic = ASCII `DBLUSCAT` |
| `40` | 2 | bootstrap_version = `1` |
| `42` | 2 | entry_count = `6` |
| `44` | 4 | catalog_schema_version = `1` |
| `48` | 8 | bootstrap_generation = `1` |
| `56` | 8 | reserved64 = `0` |
| `64` | 192 | six 32-byte system-relation entries |
| `256` | 7936 | reserved zero bytes |

Each 32-byte system-relation entry is:

| Entry offset | Size | Field / v1 meaning |
|---:|---:|---|
| `0` | 2 | system_relation_code |
| `2` | 2 | flags = `0` |
| `4` | 4 | relation_schema_version |
| `8` | 8 | TableId |
| `16` | 4 | heap FileId |
| `20` | 4 | FSM FileId |
| `24` | 8 | reserved64 = `0` |

All multi-byte integers are little-endian.

Entries occur exactly once and in ascending `system_relation_code` order:

```text
1 = sys_tables
2 = sys_columns
3 = sys_indexes
4 = sys_index_columns
5 = sys_constraints
6 = sys_statistics
```

For the v1 bootstrap page:

```text
relation_schema_version = 1
```

for every entry.

The six TableIds are distinct, nonzero catalog-object IDs.

Every heap/FSM FileId is nonzero, has the expected file kind/object identity when opened, and no heap/FSM FileId may be reused by another bootstrap entry.

### 16.9.3 Bootstrap checksum and validation

The bootstrap page carries the ordinary whole-page CRC32C from Chapter 4.

For this startup-critical page, checksum creation and verification are mandatory from initial database creation rather than optional pre-WAL staging.

A v1 decoder rejects at least:

```text
wrong PageType/PageNo
wrong format_version/header_size
nonzero common flags/reserved fields
wrong magic
wrong bootstrap_version
entry_count != 6
catalog_schema_version != 1
bootstrap_generation != 1
unknown/out-of-order/duplicate system_relation_code
zero/duplicate object or file identities where prohibited
nonzero entry flags/reserved fields
nonzero bytes 256..8191
checksum mismatch
```

### 16.9.4 Minimal interpretation rule

The engine contains one versioned built-in bootstrap descriptor set for:

```text
catalog_schema_version = 1
```

It knows only enough physical/schema information to decode the six system relations named by the bootstrap entries.

That bootstrap descriptor set is not an independently mutable metadata authority.

Startup proceeds conceptually as:

```text
open + validate catalog.dat
    ↓
decode the six bootstrap entries
    ↓
open each referenced system heap/FSM using normal FileId/object checks
    ↓
decode sys_tables/sys_columns/... with bootstrap schema version 1
    ↓
construct ordinary immutable catalog descriptors
    ↓
cross-check the six bootstrap identities against visible self-describing catalog rows
    ↓
ordinary metadata lookup uses the catalog relations
```

A bootstrap identity/schema mismatch against the self-hosted catalog is corruption and prevents normal open.

### 16.9.5 Creation and lifetime

Database creation follows the canonical staging-root and parent-directory publication protocol in §4.7.8. Within that private staging root it durably allocates the bootstrap TableIds/FileIds, initializes the six system relation files, and seeds the required self-describing catalog rows as bootstrap-frozen committed metadata:

```text
xmin = FROZEN_TXN_ID
cmin = 0
xmax = INVALID_TXN_ID
cmax = 0
```

It then writes the complete checksummed `catalog.dat`, creates/synchronizes every startup-critical regular file and managed subdirectory, validates the bootstrap, renames the staging root without replacement, and synchronizes the external parent directory. Database creation MUST NOT report success before §4.7.8's final parent-directory `fsync` succeeds.

A failed incomplete creation is not a valid database open target.

V1 `catalog.dat` is immutable after successful database creation.

Changing the bootstrap relation set or bootstrap schema requires a new explicitly versioned bootstrap format/migration; ordinary DDL never edits page 1.

This keeps bootstrap metadata minimal and prevents a second indefinitely divergent catalog.

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
15. `catalog.dat` contains only the immutable v1 bootstrap locator; ordinary catalog rows live in the self-hosted catalog relations.
16. The six bootstrap identities are cross-validated against the self-hosted catalog during open.
17. Bootstrap metadata remains minimal and cannot become a permanently divergent second catalog.
18. Database creation is not durable until the complete staging root has been renamed to its final name and the external parent directory has been synchronized.

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

Arithmetic uses IEEE-754 binary64 behavior; the execution-level checked/integer-versus-FLOAT64 division boundary is canonical in §39.3.2.

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

## 17.13 Persisted scalar value encoding

Catalog metadata that must persist a typed scalar value uses one shared byte-exact v1 codec.

This codec is used by:

```text
v1 persisted column defaults
statistics min/max values
statistics MCV values
statistics histogram boundaries
```

It is independent of the executor's in-memory `Value`, Vector, or StringRef representation.

### 17.13.1 Scalar header

Each encoded scalar begins with a 16-byte little-endian header:

| Offset | Size | Field / v1 meaning |
|---:|---:|---|
| `0` | 4 | persisted TypeId |
| `4` | 4 | flags |
| `8` | 4 | payload_length |
| `12` | 4 | reserved32 = `0` |

V1 defines:

```text
flags bit 0 = IS_NULL
all other bits = 0
```

The complete scalar encoding occupies:

```text
Align8(16 + payload_length)
```

bytes.

Any bytes after the payload up to that 8-byte boundary are zero padding.

### 17.13.2 NULL

For a typed NULL:

```text
IS_NULL       = 1
payload_length = 0
```

No payload bytes follow.

The TypeId still records the concrete SQL type of the NULL value.

### 17.13.3 Non-NULL payloads

For non-NULL values:

| Logical type | payload_length | payload |
|---|---:|---|
| BOOLEAN | `1` | byte `0` or `1` |
| INT32 | `4` | signed two's-complement int32 little-endian |
| INT64 | `8` | signed two's-complement int64 little-endian |
| FLOAT64 | `8` | IEEE-754 binary64 bits, uint64 little-endian |
| DATE | `4` | signed day-count int32 little-endian |
| TIMESTAMP | `8` | signed microsecond-count int64 little-endian |
| VARCHAR | byte length | exact string bytes, no terminator |

FLOAT64 persistence preserves the sign of zero because it can affect later arithmetic.

Every persisted NaN uses the canonical quiet-NaN bit pattern:

```text
0x7ff8000000000000
```

Other NaN payloads are not emitted by the v1 encoder.

### 17.13.4 Validation

The decoder rejects:

```text
unknown/non-storable TypeId
unknown flag bits
nonzero reserved32
NULL with nonzero payload_length
wrong fixed-width payload_length
BOOLEAN payload other than 0 or 1
nonzero alignment padding
noncanonical persisted NaN
length arithmetic overflow / truncated input
```

The decoder returns one owned typed semantic value; it never exposes a pointer into unvalidated persistent bytes as a catalog Value.

### 17.13.5 Scalar-codec invariants

1. One canonical scalar codec is shared by defaults and statistics.
2. The codec uses stable catalog TypeIds, never process enum ordinals.
3. VARCHAR payload length is byte length.
4. Persisted FLOAT64 NaNs are canonical while signed zero is preserved.
5. Padding/reserved bytes are zero and validated.
6. Scalar decoding is bounds checked before type-specific interpretation.

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
ANALYZE
EXPLAIN
EXPLAIN ANALYZE
```

The v1 ANALYZE statement grammar is:

```sql
ANALYZE table_name;
```

`ANALYZE;` without a table target is reserved for the future all-table form and is not required by the initial parser.

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
AnalyzeStatement
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

`AnalyzeStatement` contains the unresolved target table name and source span; it does not contain a TableId before binding.

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
10. Pratt/precedence parsing uses the defined precedence hierarchy.
11. `ANALYZE table_name` is a first-class statement AST, not parsed as VACUUM/EXPLAIN syntax.
12. Unsupported syntax fails explicitly rather than being half-interpreted.

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

`implementation ID` is a process/runtime dispatch identity unless another architecture contract explicitly assigns a stable persistent code.

V1 catalog defaults do **not** persist this implementation ID: permitted closed immutable default expressions are evaluated to one typed scalar before persistence as defined in §21.12.

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

The Binder resolves aggregate argument coercions, return type, and nullability from the same aggregate registry semantics executed by Chapter 29.

The initial COUNT/SUM/MIN/MAX/AVG signatures and empty-input/nullability rules in §29.3 are therefore semantic binding rules as well as execution rules; execution does not choose a different return type later.

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
18. Runtime function/operator implementation IDs are not persisted as v1 default metadata.
19. Future parameter typing can reuse UNKNOWN/contextual inference.
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
LogicalAnalyze
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

### 20.13.5 `LogicalAnalyze`

`LogicalAnalyze` is a maintenance statement node containing at least:

```text
target TableId
immutable target TableDescriptor
analyzed SchemaVer
visible analyzed ColumnIds
visible analyzed IndexIds
```

It has no ordinary relational output requirement in v1.

Its semantic meaning is:

```text
collect one new statistics version for this resolved table
```

It does not choose the physical statistics algorithms, mutate catalog descriptors in place, or bypass transaction/catalog visibility.

The resolved descriptor/schema version is fixed at binding/planning time for that statement attempt.

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

### 20.17.7 Inner-join equality equivalence classes

Within semantics where ordinary equality and join type make transitivity safe, derive equality equivalence classes.

For example:

```text
A.x = B.x
B.x = C.x
```

creates one class containing:

```text
A.x
B.x
C.x
```

Uses include:

```text
transitive join-predicate derivation
constant propagation
join-graph connectivity
interesting-order recognition
```

Equivalence derivation does not cross a LEFT JOIN null-extension boundary unless a separate proven rewrite first removes that outer-join constraint.

### 20.17.8 Constant propagation and contradiction detection

For an inner-join/filter-safe equivalence:

```text
A.x = B.x
AND A.x = 5
```

the optimizer may derive:

```text
B.x = 5
```

which may create a new base-access alternative.

Constraint analysis also detects provable contradictions such as:

```text
x = 1 AND x = 2
x < 5 AND x >= 5
NOT NULL column IS NULL
```

and marks the corresponding logical relation as provably empty before physical search.

Derived predicates retain ordinary SQL NULL semantics.

No predicate is propagated across nullable outer-join semantics without proof.

### 20.17.9 Trusted key metadata

Only constraints actually enforced by the engine may establish logical key properties.

Trusted PRIMARY KEY / UNIQUE + NOT NULL metadata may support:

```text
candidate-key inference
maximum join multiplicity
DISTINCT redundancy reasoning
GROUP BY key properties
```

Low estimated NDV is never treated as proof of uniqueness.

Foreign-key-based rewrites and join elimination remain deferred until those constraints and semantic proofs exist.

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
14. `LogicalAnalyze` carries a resolved table/schema/index set and does not perform name lookup during execution.
15. EXPLAIN consumes the bound/logical representation rather than AST syntax alone.

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

A physical heap/FSM/B+ file created by uncommitted DDL follows the exact private/final namespace lifecycle in §4.7.1–§4.7.7.

Before durable final-name publication it is **private/unpublished** under its exact `pending/` basename. After durable no-replace rename but before transaction commit it is `FINAL_DURABLE_UNCOMMITTED`; filesystem visibility still does not make it a committed catalog object.

Other transactions cannot discover it through committed catalog metadata.

If CREATE DDL aborts:

```text
catalog rows become ABORTED/invisible
allocated object IDs/FileIds remain consumed
private or final-uncommitted physical files become orphan-retirement candidates
```

V1 need not physically undo their bytes. Orphan classification and durable unlink use §4.7.6–§4.7.7 only after no live in-process owner can reference the file.

An object file is never considered committed merely because it exists on disk.

Before a DDL transaction may enter its terminal COMMIT sequence, every physical file referenced by that transaction's new catalog state MUST have completed the §4.7.4 final-name publication barrier. A namespace-sync failure therefore occurs before durable terminal commit and cannot be reported as successful CREATE publication.

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
6. build any required primary/unique index objects as private files through
   their DDL protocol
7. complete §4.7.4 durable final-name publication for the entire required
   heap/FSM/index physical file set
8. only then install transaction-owned MVCC catalog rows/descriptors that
   name those final physical files
9. finish the DDL statement while retaining DDL exclusivity until the owning transaction is terminal
10. before COMMIT, reverify that the transaction's required file set remains
    durably final-name-published
11. if/when the owning transaction reaches terminal COMMITTED publication:
       publish new committed descriptor-cache/name entries
12. on ABORT:
       keep catalog rows invisible and retire private/final-uncommitted files later
13. release DDL exclusivity only at that terminal boundary
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
11. flush/synchronize and durably rename the private B+ file to its exact
    final name through §4.7.4
12. only then install transaction-owned catalog index rows/constraint links
13. finish the DDL statement while retaining target-writer/DDL exclusivity
14. before COMMIT, reverify that the required B+ file remains durably
    final-name-published
15. if/when the owning transaction reaches terminal COMMITTED publication:
        publish the index descriptor/name entry
16. on ABORT:
        keep catalog rows invisible and retire the private/final-uncommitted index file later
17. release target writer gate and SchemaLock only at the terminal boundary
```

Step 7 is a DDL maintenance/current-state scan after target writers have drained; it is not allowed to omit rows merely because the DDL transaction has an older REPEATABLE READ snapshot.

Readers may continue while the index is built because the private index is not visible to their catalog snapshots.

New target-table writers are blocked until the index commit/abort boundary, preventing missing entries.

A half-built index is never visible to planning.

If build/transaction aborts, the private or final-uncommitted index file becomes an orphan-retirement candidate.

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

The no-new-fetch/pin and frame-drain boundary is §7.12.5. Only after that drain may the managed file be unlinked through §4.7.7. Physical retirement is durable only after `fsync(database_root)` succeeds. If unlink or directory synchronization fails, semantic DROP remains committed and cleanup remains pending/retryable.

This retirement rule applies to table heap/FSM files and index B+ files.

`DROP TABLE` also retires dependent indexes/constraints as one DDL semantic operation.

## 21.10 Catalog cache publication

The canonical catalog-cache visibility and descriptor-lifetime rules are §16.10.

DDL execution integrates with those rules as follows:

```text
uncommitted CREATE/ALTER-like metadata:
    transaction-local/catalog-MVCC-visible only

terminal COMMITTED:
    publish/replace current committed cache/name entries

terminal committed DROP:
    remove the object from current-name lookup
    while older immutable descriptors may remain alive
```

DDL MUST NOT mutate descriptors already retained by active plans.

## 21.11 INSERT binding

For:

```sql
INSERT INTO t(a,c) VALUES (...);
```

Binder resolves the target table, verifies target columns are unique, maps omitted columns, binds input expressions, inserts allowed implicit casts, fills catalog defaults or typed NULL where legal, rejects statically impossible NOT NULL cases, and produces one canonical full target-column order.

Execution never resolves target column names again.

`INSERT ... SELECT` uses the same canonical target-column contract after binding the source relation.

## 21.12 Default expressions

V1 column-default syntax may contain:

```text
literal constants
casts
immutable scalar operators/functions
```

but it must be a **closed expression**.

A v1 default MUST NOT contain:

```text
table-column references
subqueries
aggregates
STABLE functions
VOLATILE functions
parameters
```

Because every permitted v1 default is closed and IMMUTABLE, DDL binding evaluates/constant-folds the complete expression exactly once under the normal SQL type/arithmetic rules.

The result is then coerced to the target column type.

If folding, casting, or constraint validation fails, the DDL statement fails rather than storing a partially resolved expression.

Execution of INSERT therefore consumes one persisted typed constant default and does not reopen/re-resolve an operator/function tree.

This deliberately keeps the first persistent default format small and stable while preserving the semantics of immutable default expressions.

### 21.12.1 DefaultValueBlob v1

The persisted default value is a byte sequence with this 24-byte header:

| Offset | Size | Field / v1 meaning |
|---:|---:|---|
| `0` | 8 | magic = ASCII `DBLUSDEF` |
| `8` | 2 | format_version = `1` |
| `10` | 2 | flags = `0` |
| `12` | 4 | total_length |
| `16` | 4 | checksum_crc32c |
| `20` | 4 | reserved32 = `0` |
| `24` | variable | one §17.13 PersistedScalarV1 |

All multi-byte integers are little-endian.

`total_length` is exactly:

```text
24 + encoded PersistedScalarV1 length
```

with no bytes after the encoded scalar inside the blob.

CRC32C is computed over exactly `total_length` bytes with bytes `16..19` logically zero.

V1 imposes:

```text
MAX_DEFAULT_VALUE_BLOB = 4096 bytes
```

so default metadata remains bounded and can fit the ordinary system-catalog storage path without introducing a separate large-object subsystem.

A larger default value is rejected as unsupported in v1.

On reopen, the decoder validates:

```text
magic/version/flags/reserved
length/checksum
PersistedScalarV1 structure
scalar TypeId == target column TypeId
```

before exposing the default to binding/execution.

Original SQL text MAY additionally be retained for display/debugging, but it is not execution authority.

A future architecture may define a new blob version containing a persistent expression tree with stable function/operator identities.

V1 does not need such identities because only the fully folded typed result is persisted.

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

The physical execution layer additionally buffers through successful statement completion as specified in §31.9, preventing a later row-level execution error from exposing a partial RETURNING prefix.

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

### 21.17.1 ANALYZE binding and transaction boundary

For:

```sql
ANALYZE table_name;
```

binding resolves exactly one currently visible base table through the normal catalog snapshot.

The bound statement captures:

```text
TableId
immutable TableDescriptor
current SchemaVer
analyzed ColumnIds
currently visible IndexIds
```

ANALYZE is not schema-changing DDL and does not acquire `SchemaLock` or an exclusive `TableWriterGate` merely to obtain a stable row set.

Its SQL-visible values come from the transaction's normal effective snapshot:

```text
READ COMMITTED:
    statement snapshot

REPEATABLE READ:
    transaction snapshot + current command boundary
```

Concurrent DML may make the completed statistics immediately approximate/stale; it does not invalidate query correctness.

ANALYZE writes its `sys_statistics` rows through the ordinary transaction/catalog MVCC path.

The newly built `StatsDescriptor` remains transaction-local until the owning transaction commits.

For an explicit transaction:

```text
successful ANALYZE statement
    -> own later statements may use the transaction-local descriptor
    -> global committed statistics cache remains unchanged
    -> COMMIT publishes the descriptor globally
    -> ABORT discards it
```

For autocommit, global publication occurs only after the statement's owning transaction reaches terminal COMMITTED.

A failed/cancelled ANALYZE publishes neither a partial descriptor nor a global cache entry.

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
ANALYZE
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
18. DROP file unlink waits until old catalog snapshots/descriptors can no longer reference the object and is not durably complete until its parent directory is synchronized.
19. Catalog-object IDs/FileIds that may have entered persistent state are never reused.
20. V1 defaults persist the fully folded typed scalar result; runtime function/operator identities are not persisted as default authority.
21. ANALYZE uses normal catalog/MVCC visibility and publishes a committed statistics descriptor only at transaction terminal COMMITTED.
22. Rewrites preserve NULL, volatility, outer-join, grouping, and hidden-slot semantics.
23. Logical-plan validation detects broken slot/schema references before execution.
24. Unsupported SQL fails explicitly rather than being partially reinterpreted.
25. CREATE catalog commitment is forbidden until every required physical file has completed durable final-name publication under §4.7.
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
PhysicalAnalyze

PhysicalExplain
PhysicalResultSink
```

An operator is added when it represents a distinct physical execution algorithm or statement execution role.

Detailed join/aggregate/sort/DML execution algorithms are specified in Chapters 28–31.

### 22.4.1 Physical implementation availability

The physical operator family is the architecture's algorithm vocabulary; runtime capability determines which algorithms are currently eligible for physical planning.

The physical planner has an explicit capability/implementation registry.

It enumerates an alternative only when the corresponding runtime implementation is available and validated.

In particular:

```text
PhysicalHashJoin
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalHashAggregate
PhysicalSort
PhysicalTopN
```

are baseline physical implementations.

Algorithms described as later/conditional in their execution chapters, including:

```text
PhysicalMergeJoin
PhysicalSortAggregate
ordered/streaming DISTINCT
```

are costed/enumerated only after their runtime capability is enabled.

An unavailable algorithm can never appear in the final PhysicalPlan merely because the cost model has a formula for it.

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

## 22.7 Physical properties

Physical nodes expose explicit properties rather than requiring later components to infer semantics from operator names.

Relevant properties include:

```text
output ordering
candidate/unique keys
partitioning
rewindability
blocking/streaming nature
estimated cardinality
estimated memory
spill capability
```

Ordering is an ordered list of descriptors:

```text
LogicalSlotId / resolved expression
ASC | DESC
NULLS FIRST | NULLS LAST
collation / type-order semantics
```

Examples:

```text
SeqScan:
    no guaranteed SQL ordering

forward compatible IndexScan:
    index-prefix ASC / NULLS FIRST ordering

HashJoin / HashAggregate:
    no guaranteed ordering

Sort:
    exact requested ordering
```

An operator that cannot preserve a required property MUST NOT advertise it merely because its current single-threaded implementation happens to emit rows in a convenient order.

## 22.8 Foundation invariants

1. Production execution is vectorized/chunk-at-a-time.
2. Physical plans are immutable after publication.
3. Per-execution mutable state is separate from the plan.
4. Global and local worker state are distinct from day one.
5. Execution consumes resolved IDs/types/slots and never performs SQL name resolution.
6. Transaction/snapshot/read-epoch state is execution context, not plan state.
7. A physical operator represents an execution algorithm or resolved statement-execution role, not unresolved SQL syntax.
8. Maintenance operators such as PhysicalVacuum/PhysicalAnalyze consume resolved descriptors and preserve their owning maintenance/transaction protocols.
9. Physical properties are explicit plan metadata, not string/name inference.
10. Runtime state is query-lifetime/process-local and never persistent format.

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

Detailed arithmetic-error and overflow behavior is defined by §39.3.1 and MUST remain consistent with these kernels.

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

Canonical dependencies include:

```text
hash probe waits for hash-build Finalize
sort output waits for input/run Finalize
aggregate output waits for aggregate Finalize
DML write waits for target-spool Finalize
```

A dependent pipeline becomes runnable only after every required predecessor has reached its successful finalization state.

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

Every breaker declares:

```text
memory-resident state
spillability
Finalize transition
post-finalize output/source state
dependent pipelines that must wait
```

Examples include:

```text
HashJoin build:
    build rows/directory -> finalized immutable probe state

Aggregate:
    group state -> finalized group source

Sort:
    input rows -> sorted in-memory/run state -> merge/output source

DML target materialization:
    target spool -> finalized write source
```

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

Worker-pool scheduling, morsels, local-state combining, and concrete parallel operator algorithms are defined in Chapter 32.

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

The detailed cursor/client result interface and DML RETURNING spool are defined in Chapter 31.

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

## 28.1 Join algorithm roles

The physical join family is:

```text
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalHashJoin
PhysicalMergeJoin
```

Their initial roles are:

```text
NestedLoop:
    correctness/general baseline for small materialized inner input

IndexNestedLoop:
    small outer side + selective indexed inner lookup

HashJoin:
    main equality-join implementation

MergeJoin:
    ordered-input algorithm added after the simpler join paths are stable
```

The physical optimizer chooses the algorithm later.

No join implementation may reinterpret SQL join semantics merely because a different physical algorithm is selected.

## 28.2 Central query-local hash semantics

Execution uses a centralized type-aware 64-bit hash utility.

The fundamental invariant is:

```text
if the relevant SQL equality mode says A and B are equal
then Hash(A) == Hash(B)
```

Hash values are query/process-local execution values.

They are not persistent database format and need not remain stable across database versions.

The architecture does not use `std::hash` as a semantic contract.

### 28.2.1 Equality modes

Hashing/equality receives an explicit semantic mode.

For ordinary equi-join equality:

```text
NULL in any key component -> non-matchable key
```

For GROUP BY / DISTINCT equality:

```text
NULL == NULL for grouping identity
```

The modes share the same concrete type semantics for non-NULL values.

In particular:

```text
FLOAT64 -0.0 and +0.0 hash equally
canonical-equivalent NaNs hash equally where the equality mode treats them equal
VARCHAR hashes exact binary bytes
composite keys mix component hashes and component/null markers
```

A strong 64-bit avalanche/mix function is used for composite keys, but the exact query-local mix algorithm is not a persistent compatibility promise.

## 28.3 Nested-loop join

`PhysicalNestedLoopJoin` is the correctness/small-input baseline.

For the baseline shape:

```text
materialize the right input
then probe it with left-side chunks
```

Supported logical join types are:

```text
INNER
LEFT
CROSS
```

The materialized right side uses query-owned row storage and deep-copies retained variable-length values.

A probe row may emit across multiple output chunks; the runtime keeps continuation state rather than requiring one full cross product to fit in memory.

Nested-loop join is not the universal large equi-join fallback once hash join is available.

## 28.4 Index nested-loop join

For each outer chunk:

```text
1. vector-evaluate inner lookup key expressions
2. batch B+ point/range lookups where practical
3. fetch candidate inner heap versions
4. apply heap MVCC visibility
5. evaluate the residual join predicate
6. emit joined rows
```

The index never proves inner-row visibility by itself.

V1 uses this path when:

```text
outer side is small
+
inner lookup is selective through an index
```

For a LEFT index nested-loop join, the logical left side is the preserved outer side and one NULL-extended row is produced when no inner candidate survives MVCC/residual evaluation.

## 28.5 Hash-join shape and preserved-side orientation

`PhysicalHashJoin` is the primary equality-join implementation.

Its inputs are:

```text
build input
probe input
equality-key expression pairs
optional residual predicate
join type
```

Initial join types are:

```text
INNER
LEFT
```

For INNER JOIN, the optimizer may select either logical input as build side when it preserves output-slot semantics.

For LEFT JOIN, v1 fixes:

```text
logical right side -> build
logical left side  -> probe / preserved side
```

The optimizer MUST NOT arbitrarily swap that orientation merely to make the build smaller unless a separate semantic transformation proves an equivalent physical plan.

This keeps unmatched-row production local and unambiguous.

## 28.6 Hash-join build storage and directory

Build rows are retained in `RowCollection` using a build `RowLayout` containing at least:

```text
join key values required for full equality validation
payload columns required downstream
optional cached 64-bit hash
duplicate-chain metadata
```

Variable-length build values are deep-copied into build-owned storage.

The hash directory is power-of-two open addressing.

Each occupied directory entry contains conceptually:

```text
64-bit hash
head build-row handle for one equal-key duplicate chain
```

Insertion/probe behavior is:

```text
hash mismatch:
    continue probe sequence

hash match:
    compare full SQL key against the entry's representative/head row

full key equal:
    use/append that duplicate chain

same hash but different key:
    continue open-addressing probe sequence
```

Thus different SQL keys with the same 64-bit hash remain distinct entries, while many build rows with the same key share one compact duplicate chain.

The initial target maximum directory load factor is approximately:

```text
0.70
```

Resize before probe lengths become pathological.

Directory/row memory is charged to the join's memory reservation.

## 28.7 Hash-build pipeline and finalization

The build pipeline is:

```text
build source
    ↓
build filters/projections
    ↓
HashJoinBuild sink
```

For each batch the sink:

```text
1. vector-evaluates build keys
2. excludes rows with NULL in an ordinary equi-key component from the match directory
3. deep-copies required build rows
4. computes/stores hashes
5. creates duplicate chains/directory entries
```

For the initial INNER/LEFT shape, non-matchable NULL-key build rows can be discarded from the match structure because the build side is not preserved.

`Finalize` completes/resizes/compacts the directory and publishes immutable probe state.

The probe pipeline cannot begin until successful build Finalize.

After Finalize, ordinary probe workers do not mutate the hash directory.

## 28.8 Probe continuation and residual predicates

A probe chunk can produce more than one output chunk.

Local probe state stores enough continuation information for at least:

```text
current probe row
current build-row handle in duplicate chain
whether any candidate survived the residual predicate
LEFT JOIN matched state
```

After equi-key match, candidate joined rows are passed through the residual predicate.

Example:

```sql
A.id = B.id AND A.ts < B.ts
```

uses:

```text
hash/equality key:
    A.id = B.id

residual:
    A.ts < B.ts
```

Arbitrary non-equality predicates are never smuggled into hash-key equality.

## 28.9 LEFT hash join

For every logical-left/probe row:

```text
NULL in an equi-key component:
    emit one NULL-extended right/build row

otherwise probe duplicate chain:
    emit every candidate that survives residual evaluation

if no candidate survives:
    emit one NULL-extended right/build row
```

A hash collision or an equi-key match whose residual later fails does not count as a successful LEFT JOIN match.

The output right/build slots have the nullable metadata required by the logical LEFT JOIN contract.

## 28.10 Grace hash-join spill

When build state cannot stay within its memory reservation, the join switches to partitioned Grace-style execution.

The core protocol is:

```text
hash build input
    ↓
partition using high hash bits
    ↓
spill build partitions

partition probe input with the identical partition function
    ↓
for each partition pair:
    load/build one build partition
    probe corresponding probe partition
```

Partition count is a power of two chosen from measured/estimated pressure and available reservation.

Spill blocks use the query-temporary SpillManager contract from Chapter 24.

For LEFT JOIN, probe rows with non-matchable NULL keys may be emitted as unmatched without entering a hash partition; every other probe row belongs to exactly one probe partition and can produce at most one unmatched LEFT output after that partition is fully evaluated.

Hash join never promises output ordering, so partition processing order may change physical output order.

## 28.11 Recursive repartition and skew fallback

If a build partition is still too large:

```text
repartition with additional hash bits
```

up to a bounded recursion depth.

The runtime records skew/repartition statistics.

If a pathological partition still cannot fit, execution uses a controlled correctness fallback such as chunked nested processing for that partition rather than recursing indefinitely or exceeding the hard memory limit.

The fallback remains memory-accounted.

## 28.12 Merge join

Merge join is an architecture-supported later execution algorithm once nested-loop/hash paths are stable.

It is useful when compatible ordering already exists or when ordered output has downstream value.

The initial implementation may focus on equality merge joins.

Ordinary equality retains ordinary NULL semantics:

```text
NULL does not match NULL
```

Duplicate key groups are processed incrementally.

The operator MUST NOT materialize the complete cross product of two large duplicate groups merely to produce one output chunk.

For LEFT semantics, unmatched logical-left rows are emitted exactly once.

Range-style merge opportunities may be added only with explicit predicate semantics rather than treating every inequality as the same algorithm.

## 28.13 Join invariants

1. Index candidates always return through heap MVCC visibility.
2. Query-local hash equality and hash values obey `equal => same hash`.
3. Ordinary equi-join NULL keys never match NULL keys.
4. Hash collisions always perform full key equality validation.
5. Equal build keys use duplicate chains without losing duplicate multiplicity.
6. LEFT hash join preserves the logical left/probe side in the baseline implementation.
7. Residual predicates run after equi-key candidate formation and determine LEFT matchedness.
8. One probe row may emit across multiple output chunks without losing state.
9. Hash-build state is finalized/immutable before dependent probe pipelines execute.
10. Retained build VARCHAR values are owned by build storage.
11. Grace spilling uses identical build/probe partition semantics.
12. Hash spill/repartition may change output order because hash join advertises no order.
13. Repartition recursion is bounded and has a controlled skew fallback.
14. Merge join duplicate groups are streamed incrementally rather than materialized as one cross product.

---

# 29. Aggregation and DISTINCT

## 29.1 Hash aggregation role

`PhysicalHashAggregate` is the main grouped-aggregation implementation.

It consumes:

```text
group-key expressions
aggregate descriptors
aggregate argument expressions
```

No grouping keys means global aggregation and uses the specialized single-state path in §29.5.

Hash aggregation is a pipeline breaker until its group state is finalized.

## 29.2 Aggregate state API

Every aggregate implementation exposes conceptually:

```text
StateSize
StateAlignment
Initialize(state)
Update(state, vector inputs, selection)
Combine(target, source)
Finalize(state, output vector)
Destroy(state) if required
```

This contract supports:

```text
vectorized batch update
worker-local partial state
parallel combine
spill/repartition processing
```

The executor does not invoke one virtual aggregate callback per input row.

Function/operator dispatch is resolved per aggregate/vector batch.

## 29.3 Initial aggregate signatures and NULL behavior

The v1 aggregate registry has these baseline semantics.

### COUNT(*)

```text
result type: INT64 NOT NULL
empty input: 0
```

Counts every input row.

### COUNT(expr)

```text
result type: INT64 NOT NULL
empty/all-NULL input: 0
```

Counts non-NULL argument values.

### SUM

NULL inputs are ignored.

No non-NULL input yields NULL.

Initial signatures are:

```text
SUM(INT32)   -> INT64 nullable
SUM(INT64)   -> INT64 nullable
SUM(FLOAT64) -> FLOAT64 nullable
```

Integer SUM uses a wider checked internal accumulator where available (for example a host/compiler 128-bit integer) and raises `ArithmeticError` if the declared INT64 result cannot be represented.

FLOAT64 SUM follows the database's binary64 arithmetic semantics.

### MIN / MAX

NULL inputs are ignored.

No non-NULL input yields NULL.

For every initially registered orderable scalar type, result type equals input type and comparison uses the Chapter-17 semantic ordering, including binary VARCHAR and FLOAT64 edge semantics.

### AVG

NULL inputs are ignored.

No non-NULL input yields NULL.

Initial numeric signatures are:

```text
AVG(INT32)   -> FLOAT64 nullable
AVG(INT64)   -> FLOAT64 nullable
AVG(FLOAT64) -> FLOAT64 nullable
```

State contains at least:

```text
sum
count
```

Integer-input AVG uses a checked wider integer sum where available before conversion/division into the FLOAT64 result.

More precise DECIMAL-style accumulation belongs to future DECIMAL/NUMERIC support rather than an implicit hidden type.

## 29.4 Group hash table

Grouped aggregation uses a hash structure conceptually similar to hash join:

```text
hash directory
    ↓
one group row/state
```

Each group row owns:

```text
deep-copied group key
properly aligned aggregate state block
optional cached hash
```

Group equality uses the explicit GROUPING hash/equality mode from §28.2.1.

Therefore:

```text
NULL == NULL for grouping identity
-0.0 == +0.0 for v1 FLOAT64 grouping
canonical-equivalent NaNs group together
VARCHAR grouping is binary-byte equality
```

This does not alter ordinary SQL `=` semantics with NULL.

All group-table/key/state memory is accounted through the aggregate's memory reservation.

## 29.5 Global aggregate fast path

With no GROUP BY:

```text
one aggregate state block
```

is sufficient.

No group hash table is created.

Input vectors update the aggregate states directly.

On empty input, this path still emits exactly one aggregate result row, with the per-function empty-input semantics from §29.3.

## 29.6 Hash-aggregate spill

When grouped state exceeds its reservation, execution partitions by group hash and uses SpillManager.

The robust v1 spill path may spill:

```text
group keys
aggregate argument values / raw qualifying rows
```

then finish one partition at a time and combine equal groups.

It does **not** require opaque in-memory aggregate state bytes to be a stable spill format.

A later aggregate descriptor may opt into serialized partial-state spilling only after that aggregate defines safe temporary serialization plus Combine semantics.

Recursive repartition is bounded in the same spirit as Grace hash join.

Aggregate spill MUST produce exactly the same groups and finalized values as an unlimited-memory execution.

## 29.7 DISTINCT

`PhysicalDistinct` may reuse the group-hash infrastructure with:

```text
all output columns = grouping key
zero aggregate states
```

Its equality is the same SQL grouping/distinct equivalence from Chapter 20:

```text
all NULLs in one duplicate class
v1 FLOAT64 grouping equivalence
binary VARCHAR equality
```

Hash DISTINCT advertises no ordering property.

A later physical planner may choose an ordered/streaming distinct when an input ordering makes that implementation beneficial.

## 29.8 Aggregation output properties

Hash aggregation/group hash DISTINCT do not preserve input ordering.

Global aggregate produces at most one result row.

Grouped output order is unspecified unless a different physical operator explicitly advertises order.

The physical optimizer must insert/use Sort when SQL requires an ordering not otherwise produced.

## 29.9 Aggregation invariants

1. Aggregate updates are vectorized/batch-oriented.
2. Aggregate state size/alignment are descriptor-owned and respected by group storage.
3. COUNT results are INT64 NOT NULL and return zero on empty input.
4. SUM/MIN/MAX/AVG return NULL when no non-NULL input exists.
5. Integer SUM never relies on signed-overflow undefined behavior.
6. GROUP BY/DISTINCT use grouping equality, not ordinary NULL comparison.
7. Hashing and grouping equality agree.
8. Group keys/varlen data retained by a group table are deep-copied into group-owned storage.
9. Global aggregation avoids a hash table.
10. Spill does not require raw in-memory aggregate-state bytes to become a compatibility format.
11. Spilled and in-memory aggregation produce semantically identical results.
12. Hash aggregate/DISTINCT advertise no ordering.

## 29.10 Ordered aggregation and streaming DISTINCT

The architecture supports later ordered implementations when the physical capability registry enables them.

`PhysicalSortAggregate` requires input ordered by its grouping keys.

When that requirement is already satisfied, it may aggregate one key group at a time with memory approximately bounded by the current group plus aggregate state.

Otherwise physical planning may enforce the required grouping order with `PhysicalSort` and compare:

```text
Sort + PhysicalSortAggregate
```

against `PhysicalHashAggregate`.

An ordered/streaming DISTINCT similarly requires input ordered compatibly by every DISTINCT key and can emit one row per adjacent duplicate class.

These implementations use the same grouping equality as §20.9/§29.7.

Until the runtime capability exists, the planner does not enumerate them.

---

# 30. Sorting and Top-N

## 30.1 PhysicalSort

`PhysicalSort` is a blocking operator.

It:

```text
1. evaluates resolved sort-key expressions
2. materializes required output payload
3. forms compact sortable records
4. sorts in memory while within its memory reservation
5. spills sorted runs when required
6. performs one- or multi-pass merge
7. emits sorted DataChunks
```

SQL sort is not required to be stable for rows equal on every ORDER BY key.

The operator's produced ordering property is exactly the resolved logical ordering:

```text
per-key ASC/DESC
NULLS FIRST/LAST
collation/type order
```

## 30.2 Sort storage and records

Sort-owned input rows live in query-temporary row/run storage and deep-copy retained VARCHAR bytes.

A compact sort record contains conceptually:

```text
normalized key prefix
row/payload handle
```

Full key values remain reachable for tie-breaking.

The initial normalized prefix target is:

```text
8–16 bytes
```

The normalized prefix is an **order-preserving prefix of the resolved SQL comparator**, not merely a hash.

It incorporates leading-key semantics only when those semantics can be encoded safely, including:

```text
ASC/DESC transformation
NULL position
binary VARCHAR/type ordering
```

If a key cannot contribute a sound prefix, the prefix is shortened rather than guessing an order.

Prefix comparison may decide order when prefixes differ.

On prefix equality, execution performs the complete SQL comparator over the full sort keys.

## 30.3 Sort comparator

The complete comparator obeys exactly:

```text
key order
ASC / DESC
NULLS FIRST / NULLS LAST
SQL type ordering
binary VARCHAR collation
FLOAT64 total-order contract
```

The same semantic ordering is used for:

```text
in-memory sort
external-run generation
merge
Top-N
physical ordering-property matching
```

No raw-byte comparator substitutes for semantic comparison unless the type's normalized representation is proven equivalent.

## 30.4 In-memory sort

The initial implementation uses a high-quality comparison sort such as an introsort-style standard/library algorithm over the compact sort records/handles.

The expensive payload is not repeatedly moved during comparison sorting.

Custom radix sorting is a future optimization, not a correctness dependency.

## 30.5 External merge sort

When the current in-memory run reaches its reservation:

```text
sort run
    ↓
write sequential spill run
    ↓
release/reset run memory
```

After input finalization:

```text
k-way merge sorted runs
```

Readers/writers are buffered.

Merge fan-in is chosen from available query memory and run count.

If all runs cannot participate in one merge, execution performs multiple merge passes.

Every merge comparison uses the same comparator as in-memory sorting.

## 30.6 Sort-run temporary format

A sort run is a query-temporary SpillManager object containing enough information to validate the run, including conceptually:

```text
run identity / temporary format version
RowLayout or schema fingerprint
sort-key descriptor fingerprint
record count
checksummed sequential blocks
sorted records/payload
```

The fingerprint detects accidental mismatch inside one query/runtime execution; it is not a long-lived database format registry.

Runs are never WAL logged or crash recovered.

A malformed/checksum-failing run raises `SpillIOError` and aborts the query.

## 30.7 PhysicalTopN

For:

```sql
ORDER BY ... LIMIT N [OFFSET M]
```

a physical planner may choose `PhysicalTopN`.

Define with checked arithmetic:

```text
K = N + OFFSET
```

Overflow is an execution/planning error rather than wraparound.

If `K = 0`, the result is empty without consuming unnecessary upstream work when pipeline dependencies permit.

Otherwise Top-N maintains at most `K` best records in a bounded heap using the exact sort comparator.

The heap's root represents the worst retained candidate so a better incoming row can replace it efficiently.

After input completion:

```text
sort the retained K records by the exact comparator
skip OFFSET
emit up to N rows
```

Heap/internal container order is never exposed as SQL sort order.

For very large K, a full/external sort may be the better physical plan.

## 30.8 Sorting invariants

1. Sort is a pipeline breaker and does not emit final rows before input/run finalization.
2. Retained sort keys/payload own their varlen bytes.
3. Normalized prefixes are order-preserving aids, not hashes or substitute semantics.
4. Prefix ties always fall back to the full comparator.
5. In-memory, spill-run, merge, and Top-N comparison semantics are identical.
6. External sort may use multiple merge passes under bounded memory.
7. Sort runs are temporary and never WAL logged/crash recovered.
8. Top-N retains at most checked `N + OFFSET` records.
9. Top-N sorts retained output before emission.
10. Equal-key SQL sort need not be stable unless another explicit semantic requirement says otherwise.

---

# 31. DML, DDL, VACUUM, and Result Interface

## 31.1 UPDATE/DELETE target materialization

UPDATE and DELETE use a statement-level target spool before any target mutation begins.

Read phase:

```text
scan/bind the logical target relation
    ↓
apply WHERE and supported target-producing joins
    ↓
materialize target RID
    +
required old values
```

Write phase begins only after successful spool Finalize:

```text
iterate finalized target rows
    ↓
acquire logical write locks
    ↓
re-fetch/revalidate
    ↓
perform Chapter-15 MVCC mutation
```

This is a deliberate pipeline breaker and the primary Halloween-protection boundary.

## 31.2 One logical target once

A logical target RID is represented at most once in one UPDATE/DELETE target spool.

The physical plan may satisfy this in either of two ways:

```text
prove the target-producing child has unique target RID
or
explicitly de-duplicate target RID during materialization
```

A join-expanded target plan MUST NOT cause the same physical target version to be mutated twice merely because multiple input rows produced the same RID.

The spool's RID-uniqueness mechanism is query-temporary and may itself use memory/spill-aware distinct/sort support where required.

## 31.3 Halloween protection

The target spool prevents UPDATE from rediscovering its own mutations through:

```text
an index whose key is modified
a newly created heap version
an altered predicate value
```

Current-command MVCC visibility remains important but is not the sole Halloween defense.

No target-table mutation starts before the target identity set is finalized.

## 31.4 Target-spool memory and spill

Small target sets use a query-owned `RowCollection`.

Large sets spill through SpillManager.

The spool row layout contains at least:

```text
RID
old values needed by assignment expressions
old values needed for complete new tuple construction
values needed for unique-key handling
values needed for RETURNING
```

Columns not required by these semantics are not materialized merely because they exist in the table.

Spill/reload preserves RID identity and old value bytes exactly for this statement attempt.

## 31.5 Revalidation and READ COMMITTED retry boundary

Materialization does not eliminate write races.

Before UPDATE/DELETE of each spooled target:

```text
acquire TUPLE_WRITE
re-fetch RID
revalidate tuple/version state
apply Chapter-11 isolation conflict rules
```

The Chapter-15 retry boundary remains authoritative.

If READ COMMITTED discovers a retry-requiring conflict **before this statement attempt has produced any persistent WAL-visible write**:

```text
discard target spool
discard buffered RETURNING
capture a fresh statement snapshot
rebuild/retry the statement attempt
```

If any persistent statement write has already occurred:

```text
same-TxnId whole-statement restart is forbidden
transaction -> abort/conflict outcome
```

This follows the Chapter-15 retry boundary and the no-physical-user-DML-undo architecture.

## 31.6 INSERT execution

`PhysicalInsert` is a sink consuming typed input chunks.

For each input batch it:

```text
1. evaluates/converts target-column vectors
2. enforces runtime NOT NULL constraints
3. acquires required unique-key locks in deterministic order
4. performs current-state uniqueness checks
5. encodes/installs heap tuple versions through Chapter 15
6. installs required B+ entries through their MTR path
7. appends requested RETURNING values to statement-owned result storage
```

Bulk insertion should amortize:

```text
tuple encoding
FSM candidate lookup
heap-page latching
index-key encoding
```

across chunks when doing so does not weaken lock/WAL ordering.

## 31.7 UPDATE execution

After target-spool finalization:

```text
1. consume targets in batches where practical
2. acquire/revalidate one target's logical write semantics
3. vector-evaluate assignment expressions over safely grouped targets
4. construct one complete new tuple version per target
5. install the new version and old xmax/cmax through Chapter 15
6. install new physical index entries
7. buffer RETURNING new-row values
```

Lock waits/conflicts may force the implementation to break otherwise-vectorized work around individual targets.

Correctness of the write protocol wins over artificial vectorization of lock acquisition.

The v1 mutation/write phase is single-worker unless a later architecture explicitly defines parallel transaction-write coordination.

## 31.8 DELETE execution

After target-spool finalization:

```text
for each target:
    acquire/revalidate TUPLE_WRITE semantics
    install xmax/cmax transactionally
    buffer RETURNING old-row values when requested
```

Secondary-index entries are not physically erased here.

Vacuum remains the exact index-garbage cleanup owner.

The v1 mutation/write phase is single-worker.

## 31.9 RETURNING spool

DML `RETURNING` is statement-owned output.

For small output it uses an in-memory RowCollection.

For large output it uses a spill-capable result spool.

No RETURNING row becomes externally visible while the statement attempt may still restart or fail after producing only a prefix of its result.

The safe publication boundary is:

```text
DML target/input processing completed successfully
all required writes for the statement completed
no internal retry remains possible
statement execution outcome is successful
```

At that point the result spool may be consumed by the client/result cursor even if the surrounding explicit transaction has not yet committed; a later explicit transaction rollback does not retroactively invalidate the fact that the client observed a successful statement's RETURNING result.

If the statement aborts/fails, the unpublished RETURNING spool is discarded.

## 31.10 Query result interface

Execution exposes results through a result sink/cursor abstraction.

An internal borrowed producer DataChunk is never exposed beyond its owner lifetime.

The baseline cursor contract is:

```text
Next()
    -> client-visible result chunk or FINISHED
```

The cursor/result layer owns or safely retains every value in the returned chunk.

For a simple synchronous client, a returned chunk remains valid until the next cursor `Next()` call or cursor destruction, whichever comes first; callers that need a longer lifetime copy/materialize it.

This lifetime is an API/runtime contract, not a persistent format.

A CLI may format rows immediately while consuming chunks.

## 31.11 Physical DDL execution

DDL physical operators are control/management operators rather than vector hot paths.

Examples:

```text
PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
```

They invoke the Chapter-21 catalog/storage protocols under SchemaLock/TableWriterGate and ordinary transaction rules.

CREATE INDEX runs the offline private build pipeline and does not publish committed catalog metadata before the owning transaction's terminal publication boundary.

DDL execution remains single-coordinator in v1.

## 31.12 Physical VACUUM

PhysicalVacuum invokes the Chapter-14 VacuumManager/reclamation protocol.

It is a maintenance operator, not a requirement to express vacuum's internal page/index work as ordinary relational Filter/Project/Hash operators.

It may later expose progress/debug result chunks without changing vacuum correctness ownership.

V1 vacuum execution remains conservatively serialized according to its maintenance/storage coordination rules rather than being parallelized implicitly by the general query scheduler.

### 31.12.1 Physical ANALYZE

`PhysicalAnalyze` executes the Chapter-34 statistics collection/publication protocol for one already-resolved target table.

It uses the ordinary `QueryExecutionContext` and therefore participates in:

```text
transaction/snapshot visibility
ReadEpochGuard where index-derived RIDs are retained
QueryMemoryManager accounting
SpillManager only if a bounded statistics helper explicitly needs it
query cancellation
profiling
```

The baseline implementation is single-coordinator.

`LogicalAnalyze` lowers directly to `PhysicalAnalyze`; it does not enter join-order/access-path enumeration merely to rediscover that v1 ANALYZE performs its required full visible heap scan.

Collection is conceptually:

```text
resolved TableDescriptor / SchemaVer
    ↓
stable visible heap scan
    ↓
bounded per-column HLL / MCV / reservoir state
    ↓
index physical-statistics scans where required
    ↓
finalize complete StatsDescriptor
    ↓
encode + write one StatsVersion of sys_statistics rows
    ↓
statement success
    ↓
transaction-local descriptor available to own later statements
    ↓
terminal COMMITTED -> publish global immutable descriptor/cache entry
```

ANALYZE never mutates an existing published `StatsDescriptor` in place.

A cancellation/error before statement success discards the in-memory candidate and leaves the previous committed descriptor authoritative.

An abort after successful ANALYZE but before transaction commit leaves the new catalog rows MVCC-invisible to other transactions and suppresses global cache publication.

ANALYZE does not acquire schema-changing DDL exclusivity merely to block ordinary DML; its SQL-visible row set is stabilized by MVCC.

## 31.13 DML/result invariants

1. UPDATE/DELETE finalize a target spool before mutating any target.
2. Each physical target RID is represented at most once per statement attempt.
3. Target materialization is the primary Halloween-protection boundary.
4. Large target/RETURNING spools are query-memory accounted and spill-capable.
5. Every target is revalidated after logical write-lock acquisition.
6. READ COMMITTED whole-statement retry is permitted only before the first persistent statement write.
7. INSERT/UPDATE/DELETE consume resolved typed inputs and do not redo binding.
8. DML write phases remain single-worker in the v1 execution baseline.
9. DELETE/UPDATE do not physically clean old secondary-index entries.
10. RETURNING does not expose partial output from a statement attempt that later restarts/fails.
11. Client-visible result chunks never depend on an expired internal borrowed chunk.
12. DDL/VACUUM/ANALYZE control operators preserve their owning catalog/storage/statistics transaction protocols.
13. ANALYZE never globally publishes an uncommitted or partial StatsDescriptor.

---

# 32. Parallel Execution and Scheduling

## 32.1 Worker model

Execution uses a fixed worker pool.

The database does not create an arbitrary OS thread per query or per pipeline.

Worker count is configuration.

The first production executor may run one query using one worker, while the same state/task model remains capable of multiple workers.

## 32.2 Morsels

Parallel-ready sources divide work into independent units called morsels.

Examples are:

```text
heap scan:
    contiguous page ranges

Values:
    row ranges

spill work:
    partition/run ranges
```

The initial heap-scan target is approximately:

```text
64–256 heap pages per morsel
```

and is tunable.

B+ ordered range scans are not freely partitioned in the baseline and may remain a single source until explicit range partitioning exists.

Morsel boundaries are also natural cancellation/fairness points.

## 32.3 Worker-local state

Each worker/task owns hot mutable state such as:

```text
expression scratch
reusable output chunks
source cursor/morsel position
local aggregate table
local sort run
join probe continuation
local profiling counters
```

Workers do not contend on one shared mutable state object for every chunk when the operator can instead Combine/Finalize local state at a boundary.

Immutable plan state remains shared.

Global execution state contains only the coordination/shared structures that genuinely require it.

## 32.4 Parallel sequential scan

Workers claim heap page-range morsels.

Every worker:

```text
uses the same effective query transaction/snapshot semantics
uses the same query-lifetime read-epoch protection
fetches through the shared BufferPool
produces independent chunks
```

The transaction/snapshot/CommandId view consumed by parallel read tasks is immutable for the lifetime of those tasks. Transaction terminal-state transitions and DML mutation are not performed concurrently by arbitrary read workers.

Without a required ordering property, worker output may interleave arbitrarily.

If ordered output is required, the selected physical plan must include/order-preserve through an operator capable of that property.

Parallel SeqScan itself still advertises no SQL ordering.

## 32.5 Parallel hash join

### Build

The preferred build architecture avoids one contended resizable global table:

```text
workers append to local RowCollections
compute hashes
optionally partition by hash
    ↓
build input barrier
    ↓
construct/finalize directory partitions in parallel
```

Local build storage is combined/referenced through query-local handles without invalidating retained varlen ownership.

The dependent probe phase starts only after every build partition it needs is finalized.

### Probe

After finalization, the hash directory/partitions are immutable.

Probe workers execute concurrently without hash-directory mutation latches.

Each owns:

```text
probe continuation state
output chunk
local counters
```

LEFT JOIN preserved-side semantics remain unchanged by parallelism.

## 32.6 Parallel hash aggregate

The preferred first parallel design is:

```text
worker-local group hash tables
    ↓
partitioned/combinable aggregate states
    ↓
Combine / Finalize
    ↓
result groups
```

Do not serialize every aggregate update through one global lock.

For global aggregation, each worker may maintain one local aggregate state block and combine those states at finalize.

Aggregate descriptors' `Combine` contract is therefore mandatory for aggregates that participate in parallel execution.

## 32.7 Parallel sort

Workers generate sorted local runs independently.

Finalize is:

```text
parallel run generation
    ↓
merge tree / k-way merge
    ↓
ordered output source
```

There is no shared comparison-sort critical section.

In-memory and spilled local runs enter the same merge architecture.

When an ordering property is required, final emission occurs through the merge/order-preserving source rather than arbitrary worker interleaving.

## 32.8 Task scheduler and dependencies

The first parallel scheduler may use:

```text
fixed worker pool
global concurrent ready-task queue
pipeline dependency counters
morsel tasks
```

A task becomes ready only after its dependency counter reaches zero through successful predecessor Finalize transitions.

Query cancellation prevents new unnecessary tasks from being scheduled and causes running tasks to stop at normal cancellation points.

The baseline intentionally does not require lock-free work stealing.

If profiling later shows ready-queue contention, per-worker work-stealing deques are a compatible future optimization.

## 32.9 Fairness

A long query should not monopolize a worker indefinitely without yield/cancellation boundaries.

Morsel/task granularity provides those boundaries.

A future multi-query scheduler may cap:

```text
active tasks per query
memory per query
worker share
```

The baseline scheduler can remain simple as long as its task model preserves these control points.

## 32.10 NUMA policy

NUMA-specific buffer pools, worker affinity, hash placement, and memory placement are deferred until ordinary multi-core parallel execution is measured.

Large allocation interfaces remain centralized so a future NUMA policy can be inserted without changing operator semantics.

## 32.11 SIMD and hot-loop policy

Vector kernels are written to enable compiler auto-vectorization:

```text
contiguous typed buffers
simple counted loops
all-valid fast paths
selection vectors
batch-level type/representation dispatch
```

Portable C++ is the baseline.

Explicit AVX2/AVX-512/NEON kernels are profiling-driven later work for operations such as comparison, arithmetic, validity, and hashing.

Hot loops prefer:

```text
specialized no-NULL/nullable paths
selection vectors
compact state machines
```

over:

```text
per-row type switches
per-row virtual calls
per-cell variant visitation
```

## 32.12 Prefetch

The architecture leaves explicit optimization points for:

```text
next sequential heap pages
next B+ leaf
hash-table probe buckets
spill-merge blocks
```

Initial execution relies on BufferPool/OS/cache behavior.

Explicit prefetch is added only when profiles show benefit and it does not extend unsafe page/data lifetimes.

## 32.13 Parallel-runtime invariants

1. The worker pool is fixed/configured rather than unbounded thread creation.
2. Morsels are independent work units with cancellation/fairness boundaries.
3. Worker-local hot state avoids per-chunk global synchronization when possible.
4. Parallel workers use the same transaction/snapshot/read-epoch semantics as single-worker execution.
5. Parallel SeqScan advertises no ordering.
6. Hash build is fully finalized before probe workers consume it.
7. Finalized hash probe state is immutable/read-only.
8. Aggregate parallelism uses local states plus Combine rather than one lock per row.
9. Parallel sort produces required order through merge/finalized ordered output.
10. Dependency counters prevent consumers from observing unfinalized breaker state.
11. DML mutation, DDL, and VACUUM remain single-coordinator/single-write-phase in the v1 baseline unless separately specified.
12. Auto-SIMD/prefetch optimizations do not change SQL or memory-lifetime semantics.
---

# Part VII — Cost-Based Optimization

# 33. Optimizer Architecture

## 33.1 Role

The optimizer converts a typed logical relational plan into one immutable physical plan.

Cost-based relational optimization applies to ordinary query/DML relational subplans.

Resolved control/maintenance statements such as:

```text
LogicalAnalyze
LogicalVacuum
DDL control nodes
```

may be lowered through their dedicated physical control operators without entering join-order/access-path search when they have no relational alternative to optimize.

The planning flow is:

```text
typed logical plan
    ↓
logical normalization
    ↓
predicate + required-column analysis
    ↓
stable statistics snapshot
    ↓
cardinality / row-width estimation
    ↓
base access-path enumeration
    ↓
join graph / join-order search
    ↓
physical operator alternatives
    ↓
physical-property enforcement
    ↓
lowest-cost valid PhysicalPlan
```

Version 1 uses:

```text
System-R-style bottom-up dynamic programming
+
small memo keyed by logical subproblem / useful physical properties
+
rule-based logical normalization
+
cost-based physical selection
```

A full Cascades/Volcano framework is deliberately deferred.

The architecture exposes the fundamental relational optimization mechanisms directly before introducing a more general optimizer framework.

## 33.2 Layering

The optimizer stages are conceptually:

```text
Bound / Logical Plan
        ↓
Logical Normalization
        ↓
Predicate Analysis
        ↓
Required-Column Analysis
        ↓
Statistics Lookup
        ↓
Cardinality Estimation
        ↓
Base Access-Path Enumeration
        ↓
Join Graph Construction
        ↓
Join-Order Enumeration
        ↓
Physical Operator Selection
        ↓
Physical Property Enforcement
        ↓
Final Physical Plan
        ↓
Pipeline Builder
```

The optimizer does not execute queries.

The executor does not make ordinary runtime cost-based plan choices in v1.

Adaptive/runtime reoptimization remains deferred.

## 33.3 Planning inputs

One optimizer invocation consumes immutable or stable views of:

```text
logical plan
catalog/schema/index descriptors
statistics descriptor snapshot
query execution-memory budget
optimizer/execution cost configuration
required final physical properties
```

Required final properties include at least:

```text
required output LogicalSlotIds
ORDER BY ordering when present
```

Future properties such as rewindability/partitioning may be added later.

Ordinary planning MUST NOT inspect mutable heap/B+ page contents or exact momentary BufferPool residency.

## 33.4 Stable catalog/statistics view

The optimizer holds immutable catalog/statistics descriptors for the duration of one planning invocation.

A concurrently committed ANALYZE may publish a newer statistics descriptor for later planners, but one plan is never built from a mixture of old and new versions of the same statistics object.

Likewise, schema/index descriptors obey the caller's catalog snapshot from Chapters 16 and 21.

Statistics are performance metadata and do not change semantic visibility or query correctness.

## 33.5 PhysicalPlan output

The optimizer returns one immutable:

```text
PhysicalPlan
```

containing or making inspectable at every relevant node:

```text
chosen physical operator
child relationships
chosen access path
chosen join order
chosen join algorithm
estimated rows
provably-empty state where applicable
estimated average row width
cost components / scalar total cost
estimated peak memory
estimated spill behavior
provided physical properties
```

The executor receives these decisions already made.

The executor may collect actual runtime metrics, but it does not reinterpret the logical query or silently choose a different ordinary algorithm in v1.

## 33.6 Cost-based choice rule

Physical choices are driven by estimated cost rather than hard-coded selectivity folklore.

In particular, there is no primary rule such as:

```text
if selectivity < 10%:
    use index
```

The scan/index break-even point emerges from:

```text
relation pages
candidate rows
heap-page reuse
index/heap correlation
cache assumptions
required row width
CPU work
```

The same principle applies to later join/aggregate/sort alternatives.

## 33.7 Optimizer invariants

1. The optimizer consumes resolved logical semantics and never reparses SQL.
2. One planning invocation uses stable immutable catalog/statistics descriptors.
3. Statistics influence performance decisions, never query correctness.
4. Ordinary optimization does not inspect exact mutable page contents or live buffer residency.
5. The executor receives one finalized immutable PhysicalPlan.
6. Physical choice is cost-driven; fixed selectivity thresholds are not the primary selector.
7. Logical normalization and physical algorithm selection remain distinct stages.
8. Full Cascades/adaptive optimization remains future work rather than hidden v1 complexity.

---

# 34. Statistics

## 34.1 Statistics role

Statistics are approximate, rebuildable performance metadata.

They may be:

```text
sampled
stale
approximate
missing
```

without making a query semantically incorrect.

A bad estimate may produce a bad plan; it MUST NOT produce a wrong result.

Statistics therefore remain outside WAL-critical data-correctness paths except for the ordinary transactional persistence of the catalog rows that store them.

## 34.2 ANALYZE

The SQL maintenance interface includes:

```sql
ANALYZE table_name;
```

and may later include:

```sql
ANALYZE;
```

for all tables.

Version 1 uses explicit/manual ANALYZE.

Automatic analyze is deferred until optimizer/executor behavior is well measured.

VACUUM and ANALYZE remain distinct operations even if a later maintenance command schedules both.

## 34.3 ANALYZE visibility and publication

One ANALYZE invocation uses one stable effective SQL snapshot under the transaction's normal isolation rules.

Rows visible to that snapshot contribute to SQL-visible live-row/column statistics.

Physical heap-page, dead-version, and B+ entry/leaf information may additionally be inspected as approximate maintenance metadata without changing SQL visibility semantics.

Every ANALYZE statement receives one exact:

```text
StatsVersion {
    TxnId     txn_id;
    CommandId command_id;
}
```

equal to the owning transaction and current command that perform the ANALYZE.

The pair is unique for one statement attempt under one TxnId and requires no separate persistent version allocator.

Version comparison is lexicographic by:

```text
(txn_id, command_id)
```

for choosing among multiple visible committed statistics versions for the same table.

This ordering is a deterministic freshness preference, not SQL semantic truth; overlapping ANALYZE transactions may still produce approximate/stale statistics.

ANALYZE constructs one complete immutable candidate `StatsDescriptor` before writing/publishing its statistics version.

All `sys_statistics` rows for one StatsVersion are written by the same transaction.

The TABLE-scope payload is the version manifest: it names the exact analyzed ColumnIds and IndexIds that must have complete matching payloads for the version to be accepted.

Publication has two levels:

```text
inside owning transaction after statement success:
    transaction-local StatsDescriptor may be used by own later statements

outside owning transaction:
    previous committed StatsDescriptor remains authoritative
    until owning transaction reaches terminal COMMITTED
```

At COMMITTED publication:

```text
install one complete immutable descriptor/cache entry
```

without mutating descriptors held by existing planners.

ABORT, cancellation, an incomplete chunk set, a missing manifest member, or payload validation failure does not globally publish that version.

After restart, the catalog/statistics loader selects the highest visible committed TABLE-manifest StatsVersion whose complete listed COLUMN/INDEX payload set validates.

It may fall back to an older complete visible version or to missing-statistics behavior rather than combining fragments from different versions.

## 34.4 TableStatistics

For each table, collect at least:

```text
TableId
StatsVersion
analyzed_schema_version
analyzed_live_row_count
physical_heap_pages
average_logical_row_width
average_stored_tuple_width
dead_version_estimate
```

Useful derived values include:

```text
live_rows_per_heap_page
physical_tuple_version_estimate
```

The optimizer costs SQL-visible rows primarily from `analyzed_live_row_count`, while scan I/O uses physical heap pages and scan CPU may include dead-version pressure.

## 34.5 ColumnStatistics

For each analyzed base column, collect:

```text
TableId
ColumnId
StatsVersion
LogicalType
null_fraction
NDV                       // non-NULL distinct values
min_value
max_value
MCV values
MCV frequency fractions
histogram
average_width             // especially VARCHAR
maximum_observed_width
```

`NDV` excludes NULL.

All scalar values in statistics use the same logical type semantics as binder/executor comparisons.

## 34.6 IndexStatistics

The cost model also owns approximate statistics required by index access costing.

For an analyzed index, retain when available:

```text
IndexId
StatsVersion

physical_entry_count
logical_live_entry_count
invisible_entry_count_estimate

leaf_page_count
average_entries_per_leaf
leading_key_heap_correlation
```

The counts have distinct meanings:

```text
physical_entry_count:
    approximate number of physical B+ leaf entries observed

logical_live_entry_count:
    number of entries expected for SQL-visible live rows in the ANALYZE snapshot

invisible_entry_count_estimate:
    approximate physical entries not represented by that live-row baseline
    (aborted/dead/obsolete tuple-version index garbage and concurrent skew)
```

For the v1 non-partial index model, one visible live tuple contributes one logical entry to each ordinary index, including rows whose key contains NULL.

A practical initial estimate is:

```text
invisible_entry_count_estimate
    = max(0, physical_entry_count - logical_live_entry_count)
```

with the result treated as approximate because physical index collection may overlap concurrent DML.

The derived candidate-inflation factor is:

```text
max(
    1,
    physical_entry_count
    /
    max(1, logical_live_entry_count)
)
```

and may be used as a fallback when the optimizer lacks a more key-specific garbage distribution model.

`leading_key_heap_correlation` is in:

```text
[-1.0, +1.0]
```

and summarizes correlation between leading-key order and heap PageNo order.

These statistics are approximate planning metadata, not B+ structural correctness metadata.

B+ metadata such as the actual current tree height remains owned by Chapter 8 and may be read from an immutable descriptor/metadata snapshot for costing.

## 34.7 V1 collection strategy

ANALYZE performs a vectorized full heap scan in v1.

During the visible heap scan it derives:

```text
exact live-row count for the ANALYZE snapshot
exact NULL count
exact min/max over visible non-NULL values
approximate NDV
approximate MCVs
bounded distribution samples
width statistics
physical/dead-version maintenance estimates
```

For each visible index, ANALYZE additionally obtains the physical index-maintenance statistics required by §34.6.

The baseline may use a full leaf-chain walk or bounded sampling for:

```text
physical_entry_count
leaf_page_count / occupancy
sampled key-rank <-> heap-PageNo correlation
```

These index-physical observations are performance metadata and need not be from the identical instant as the MVCC heap snapshot.

The descriptor records their approximate nature rather than treating them as visibility facts.

A full heap scan is intentionally preferred to a complex initial page sampler.

Large-table heap sampling is a future performance optimization.

## 34.8 Small-table exact mode

For tables with at most approximately:

```text
50,000 live rows
```

ANALYZE may collect exact:

```text
NDV
value frequencies
```

when its bounded maintenance-memory budget permits.

The threshold is configurable and is not a persistent-format value.

## 34.9 HyperLogLog NDV estimation

For larger inputs use a HyperLogLog-style sketch.

Initial precision:

```text
p = 14
register_count = 16,384
```

Hash the same canonical non-NULL SQL value representation used by query hash semantics for that logical type.

NULL is excluded.

Version 1 persists the resulting NDV estimate, not necessarily the complete HLL sketch.

Persisting sketches for incremental statistics is deferred.

## 34.10 Most-common values

For large inputs use a bounded heavy-hitter method such as:

```text
SpaceSaving
```

Initial target:

```text
64 MCV entries per analyzed column
```

Persist conceptually:

```text
value
estimated frequency fraction
```

in descending frequency order.

The MCV frequency fraction is relative to all visible rows, so the total MCV mass plus NULL mass and residual non-NULL mass cannot exceed 1 except for bounded estimator rounding.

An unbounded exact value->count map is forbidden for large relations.

## 34.11 Histogram collection

Maintain a bounded reservoir sample of non-NULL values.

Initial target:

```text
100,000 sampled values per analyzed column
```

subject to maintenance memory pressure.

After scanning:

1. sort sample values using SQL type ordering,
2. remove or discount MCV-represented mass so MCV and histogram estimation do not double count it,
3. construct approximately:
   ```text
   100 equi-depth bins
   ```
4. retain enough bin boundary/mass information for range estimation.

Histogram mass conceptually represents the residual non-NULL, non-MCV distribution.

## 34.12 Equi-depth rationale

Equi-depth bins target approximately equal row mass per bin.

This handles skew more usefully than equal-width numeric buckets for distributions such as:

```text
salary
dates
hot ID ranges
binary VARCHAR prefixes
```

without requiring an enormous bucket count.

## 34.13 Histogram value semantics

Histogram sorting/comparison uses the same logical ordering as execution and index-key semantics for the supported type/collation mode.

In particular:

```text
VARCHAR -> binary byte ordering
FLOAT64 -> canonical v1 total order/equality conventions
DATE/TIMESTAMP -> Chapter 17 scalar ordering
```

Statistics MUST NOT introduce a second locale/FLOAT ordering model.

## 34.14 Statistics persistence

Statistics live semantically in `sys_statistics` using the chunked row contract from §16.5.6.

Each TABLE/COLUMN/INDEX scope is first encoded as one complete `StatisticsPayloadV1` byte sequence and then split into catalog-row fragments of at most:

```text
STATISTICS_CHUNK_BYTES = 4096
```

Reassembly concatenates fragments in strictly increasing `chunk_index`.

A scope is invalid unless:

```text
chunk_count >= 1
every index 0..chunk_count-1 appears exactly once
all rows have identical table/scope/StatsVersion identity
every nonfinal fragment has exactly 4096 bytes
final fragment has 1..4096 bytes
reassembled length matches payload total_length
payload checksum/structure validates
```

### 34.14.1 Common statistics-payload header

Every scope payload begins with this 40-byte prefix:

| Offset | Size | Field / v1 meaning |
|---:|---:|---|
| `0` | 8 | magic = ASCII `DBLUSSTA` |
| `8` | 2 | payload_version = `1` |
| `10` | 2 | scope_kind (`1` TABLE, `2` COLUMN, `3` INDEX) |
| `12` | 4 | total_length |
| `16` | 4 | checksum_crc32c |
| `20` | 4 | flags = `0` |
| `24` | 8 | stats_txn_id |
| `32` | 4 | stats_command_id |
| `36` | 4 | reserved32 = `0` |

All multi-byte integers are little-endian.

`total_length` is the exact number of bytes in the reassembled scope payload.

CRC32C is computed over exactly `total_length` bytes with bytes `16..19` logically zero.

The `(stats_txn_id, stats_command_id)` pair is the payload's `StatsVersion` and must equal the catalog-row identity.

### 34.14.2 TABLE payload / version manifest

For `scope_kind = TABLE`, bytes `40..103` are:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 4 | analyzed SchemaVer |
| `52` | 4 | column_count |
| `56` | 4 | index_count |
| `60` | 4 | reserved32 = `0` |
| `64` | 8 | analyzed_live_row_count |
| `72` | 8 | physical_heap_pages |
| `80` | 8 | dead_version_estimate |
| `88` | 8 | average_logical_row_width as binary64 |
| `96` | 8 | average_stored_tuple_width as binary64 |

Both width fields MUST decode to finite binary64 values numerically greater than or equal to zero. Zero is valid, including for an empty relation. A NaN, positive or negative infinity, or value numerically less than zero in either field is corruption and invalidates the persisted statistics payload.

Starting at byte `104`, the manifest contains:

```text
column_count entries:
    ColumnId  uint32 little-endian
    reserved uint32 = 0

then index_count entries:
    IndexId uint64 little-endian
```

ColumnIds and IndexIds are each strictly increasing within their respective arrays and contain no duplicates.

The exact TABLE payload length is:

```text
104
+ column_count * 8
+ index_count * 8
```

with checked uint32 `total_length` arithmetic.

The TABLE payload is the completeness manifest for the StatsVersion.

Its `scope_id` in `sys_statistics` is exactly `0`.

### 34.14.3 COLUMN payload

For `scope_kind = COLUMN`, bytes `40..103` are:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 4 | ColumnId |
| `52` | 4 | persisted TypeId |
| `56` | 8 | null_fraction as binary64 |
| `64` | 8 | NDV estimate as binary64 |
| `72` | 8 | average_width as binary64 |
| `80` | 8 | maximum_observed_width |
| `88` | 4 | mcv_count |
| `92` | 4 | histogram_bin_count |
| `96` | 4 | value_flags |
| `100` | 4 | reserved32 = `0` |

V1 `value_flags` are:

```text
bit 0 = HAS_MIN
bit 1 = HAS_MAX
all other bits = 0
```

Variable data begins at byte `104` in this exact order:

```text
if HAS_MIN:
    one §17.13 PersistedScalarV1

if HAS_MAX:
    one §17.13 PersistedScalarV1

mcv_count times:
    one non-NULL PersistedScalarV1 value
    one binary64 frequency_fraction

histogram_bin_count times:
    one non-NULL PersistedScalarV1 upper_boundary
    one binary64 residual_mass
```

Every scalar TypeId must equal the column TypeId.

MCV entries are in descending frequency order with deterministic scalar-order tie breaking.

Histogram boundaries are nondecreasing in the Chapter-17 semantic order.

All stored fractions/estimates are finite.

Required bounds include:

```text
0 <= null_fraction <= 1
NDV >= 0
0 <= every MCV frequency <= 1
0 <= every histogram residual_mass <= 1
```

MCV mass, NULL mass, and histogram residual mass must not exceed `1` except for explicitly tolerated small floating rounding.

### 34.14.4 INDEX payload

For `scope_kind = INDEX`, the v1 payload length is exactly `112` bytes:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 8 | IndexId |
| `56` | 8 | physical_entry_count |
| `64` | 8 | logical_live_entry_count |
| `72` | 8 | invisible_entry_count_estimate |
| `80` | 8 | leaf_page_count |
| `88` | 8 | average_entries_per_leaf as binary64 |
| `96` | 8 | leading_key_heap_correlation as binary64 |
| `104` | 8 | reserved64 = `0` |

`average_entries_per_leaf` is finite and nonnegative.

Correlation is finite and lies in:

```text
[-1, +1]
```

The three entry-count fields keep physical B+ work separate from SQL-visible live-row cardinality.

### 34.14.5 Payload validation and rebuildability

A statistics loader validates:

```text
catalog row identity
chunk completeness/order
magic/version/scope/flags/reserved fields
length arithmetic
CRC32C
StatsVersion equality
TableId/ColumnId/IndexId identity
scalar codecs
counts/fractions/order invariants
TABLE manifest completeness
```

A payload version unknown to the current engine, malformed statistics payload, or incomplete statistics version invalidates that statistics version.

Because statistics are rebuildable performance metadata, such a failure may produce:

```text
missing statistics + diagnostic
```

rather than making otherwise valid user data unreadable.

The loader MUST NOT mix rows from different StatsVersions to repair an incomplete descriptor.

Arbitrary C++ object graphs, enum ordinals, process pointers, or native object dumps are never serialized.

## 34.15 Statistics snapshots and cache

Planning obtains one immutable `StatsDescriptor` for the required table/index statistics.

For ordinary cross-transaction planning, the descriptor must come from a complete visible committed StatsVersion.

A transaction that has successfully completed ANALYZE may use its own complete transaction-local descriptor for later commands after the normal CommandId boundary even before COMMIT.

A concurrent committed ANALYZE publishes a new immutable descriptor atomically.

Existing planners may finish with the old descriptor.

The catalog/statistics cache is an acceleration mechanism and preserves the caller's catalog visibility rules.

Global cache publication is a commit-side effect; an aborted transaction never leaves its descriptor globally published.

## 34.16 Freshness and modification counters

Track approximate table modifications since the last ANALYZE:

```text
rows_inserted
rows_updated
rows_deleted
```

A useful diagnostic staleness measure is:

```text
changed_rows
/
max(1, analyzed_live_row_count)
```

The optimizer may surface staleness in traces/EXPLAIN diagnostics.

Version 1 does not reject or automatically refresh a plan solely because statistics are stale.

Exact crash-persistent modification-counter accounting is not a correctness requirement.

When a committed ANALYZE descriptor becomes the global current version for a table, runtime staleness counters may reset relative to that version.

A transaction-local or later-aborted ANALYZE does not reset globally visible modification counters.

## 34.17 Statistics invariants

1. Statistics are planning metadata, not semantic truth.
2. One ANALYZE uses one stable SQL visibility snapshot for live-row/value statistics.
3. A StatsVersion is exactly `(TxnId, CommandId)` and all of its catalog rows are owned by one transaction.
4. The TABLE payload is the completeness manifest for one statistics version.
5. Global statistics publication occurs only after the owning transaction reaches terminal COMMITTED.
6. One optimizer invocation does not mix statistics descriptor versions.
7. NDV excludes NULL.
8. MCV and histogram mass are not double counted.
9. Histogram/hash semantics agree with SQL value semantics.
10. HLL/MCV/reservoir structures are bounded in memory.
11. Large relations do not require unbounded exact frequency maps.
12. Index statistics distinguish logical live entries from physical entry/garbage pressure.
13. Unsupported/stale/incomplete statistics may degrade plans but never query correctness.
14. Statistics payloads use the byte-exact chunked v1 format and shared persisted-scalar codec.

---

# 35. Cardinality Estimation

## 35.1 CardinalityEstimate

Estimated row counts use a wide floating representation such as:

```text
double
```

Intermediate estimates remain fractional.

A cardinality estimate tracks conceptually:

```text
estimated_rows >= 0
is_provably_empty
```

Pathological arithmetic is clamped to a large finite implementation maximum rather than producing NaN/Infinity inside optimizer math.

The executor still processes integer row counts.

## 35.2 Provably empty versus estimated small

The estimator distinguishes:

```text
provably empty
```

from:

```text
nonempty but estimated very small
```

Provably-empty examples include:

```text
WHERE FALSE
contradictory normalized bounds
LIMIT 0
```

For a relation not proven empty, the estimator may use a minimum working estimate near:

```text
1 row
```

to avoid cascading zero-cost plans.

The explicit `is_provably_empty` flag—not row count alone—carries logical impossibility.

## 35.3 Row-width estimate

Every logical/physical estimate includes average output width appropriate to the representation/operator being costed.

For fixed types use the relevant execution/temporary representation width.

For VARCHAR use statistics such as:

```text
average_width
```

plus applicable vector/row-layout overhead.

Estimated width influences:

```text
hash memory
aggregate memory
sort/run size
spill probability
temporary I/O
decode/materialization CPU
```

## 35.4 PredicateTruthEstimate

Predicate estimation is three-valued.

Represent conceptually:

```text
PredicateTruthEstimate {
    true_fraction
    false_fraction
    unknown_fraction
}
```

with each fraction finite, clamped to `[0,1]`, and the sum normalized to approximately 1.

Every primitive estimator computes all three components rather than leaving FALSE/UNKNOWN implicit.

A common finalization step conceptually performs:

```text
clamp t, f, u to [0,1]
reject/replace non-finite intermediate values
normalize small floating drift so t + f + u = 1
```

Large logical inconsistencies are estimator bugs, not values to hide through normalization.

Filter cardinality uses only:

```text
true_fraction
```

This prevents `NOT`, NULL comparisons, AND, and OR from silently using two-valued Boolean math.

## 35.5 Selectivity bounds

Every selectivity is clamped to:

```text
0 <= s <= 1
```

For filters:

```text
estimated_rows = input_rows * true_fraction
```

followed by the provably-empty/minimum-nonempty rule.

## 35.6 Equality to a constant

For:

```sql
column = constant
```

estimate in the following order.

### 35.6.1 NULL constant

Ordinary equality to NULL is never TRUE:

```text
true_fraction = 0
unknown_fraction = 1
```

for this comparison expression.

This is not rewritten into `IS NULL`.

### 35.6.2 MCV hit

If the non-NULL constant appears in the MCV list:

```text
true_fraction = MCV frequency
```

subject to consistency clamps.

### 35.6.3 Outside exact known range

When orderable min/max statistics prove the non-NULL constant impossible:

```text
true_fraction = 0
is_provably_empty = true
```

for a simple base filter whose truth cannot arise by another disjunct.

### 35.6.4 Residual equality

Otherwise:

```text
remaining_nonnull_nonmcv_mass
/
max(1, NDV - MCV_distinct_count)
```

estimates equality probability for a non-MCV value.

### 35.6.5 Complete truth triple for non-NULL equality

For any non-NULL constant equality estimate with column NULL fraction `n` and estimated TRUE fraction `t`:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

with the normal finite/clamp/normalization step.

`t` is always bounded by the non-NULL mass:

```text
0 <= t <= 1 - n
```

If min/max or another exact rule proves no non-NULL match:

```text
t = 0
unknown = n
false = 1 - n
```

The filter may then be provably empty even though the comparison expression still evaluates UNKNOWN on NULL input rows.

## 35.7 Column-to-column equijoin

For:

```text
A.x = B.y
```

the baseline non-MCV formula is:

```text
rows_A
* rows_B
* nonnull_fraction_A
* nonnull_fraction_B
/
max(NDV_A, NDV_B)
```

subject to bounds and stronger metadata refinements.

For the comparison expression's truth-state estimate, if the input NULL fractions are `nA` and `nB`:

```text
unknown_fraction
    = 1 - (1 - nA) * (1 - nB)
```

The estimated TRUE pair probability is bounded by the joint non-NULL mass, and FALSE is the remaining probability.

If compatible min/max ranges are provably disjoint:

```text
join_rows = 0
```

## 35.8 MCV-aware equijoin

When both sides have MCV lists:

1. find common MCV values using SQL equality semantics,
2. add for each common value `v`:
   ```text
   rows_A * freq_A(v) * rows_B * freq_B(v)
   ```
3. remove that represented probability/NDV mass,
4. estimate residual values from residual non-NULL mass and residual NDVs.

This protects hot join keys from severe underestimation.

## 35.9 Unique-key refinement

If one equijoin side `U` is a proven unique non-NULL key:

```text
each opposite-side row matches at most one U row
```

The estimated pair count is therefore bounded by the non-NULL opposite-side rows, subject to domain overlap.

Trusted foreign-key refinements remain future-compatible because foreign keys are deferred in SQL v1.

## 35.10 Range predicates

For:

```text
<  <=  >  >=  BETWEEN
```

combine:

```text
explicit MCV contribution
+
residual histogram mass
```

For a boundary inside a histogram bin, interpolate when the type supports meaningful order interpolation.

For binary VARCHAR, initial within-bin interpolation may be rank/uniform-mass based rather than numeric-distance based.

Out-of-range min/max may prove zero or full non-NULL coverage.

For a comparison against a non-NULL constant, let:

```text
n = column null_fraction
t = estimated TRUE range mass
```

Then the complete truth triple is:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

with `0 <= t <= 1 - n`.

## 35.11 NULL predicates

Let:

```text
n = null_fraction
```

For:

```sql
column IS NULL
```

the exact truth triple is:

```text
true_fraction    = n
false_fraction   = 1 - n
unknown_fraction = 0
```

For:

```sql
column IS NOT NULL
```

the exact truth triple is:

```text
true_fraction    = 1 - n
false_fraction   = n
unknown_fraction = 0
```

A trusted NOT NULL constraint makes:

```text
IS NULL:
    true = 0, false = 1, unknown = 0
    filter is provably empty

IS NOT NULL:
    true = 1, false = 0, unknown = 0
```

## 35.12 IN-list predicates

For a constant IN list:

1. deduplicate non-NULL constants using SQL equality/grouping-compatible scalar semantics,
2. sum their mutually exclusive equality TRUE selectivities into `t`,
3. clamp `t` to the column's non-NULL mass,
4. record whether the IN list contains at least one NULL element.

Let:

```text
n = column null_fraction
```

If the list contains **no NULL**:

```text
true_fraction    = t
unknown_fraction = n
false_fraction   = 1 - t - n
```

If the list contains **at least one NULL**:

```text
true_fraction    = t
false_fraction   = 0
unknown_fraction = 1 - t
```

because a non-NULL value that matches no non-NULL list element is still UNKNOWN when compared with the NULL list member.

A NULL input row is also UNKNOWN.

Filter cardinality counts only TRUE rows.

`NOT IN` obtains its semantics through the normal NOT transformation of this complete truth triple.

## 35.13 NOT

For predicate truth estimate:

```text
NOT:
    true'    = false
    false'   = true
    unknown' = unknown
```

The estimator MUST NOT blindly compute `1 - true_fraction` when UNKNOWN is possible.

## 35.14 AND

When no stronger correlation information exists, v1 uses independence as a fallback while preserving three-valued truth states.

For independent truth triples:

```text
A = (tA, fA, uA)
B = (tB, fB, uB)
```

SQL AND uses:

```text
t = tA * tB
f = fA + fB - fA * fB
u = 1 - t - f
```

equivalently, UNKNOWN covers the combinations where neither operand is FALSE and at least one is UNKNOWN.

For simple same-column constraints such as:

```text
x > 10 AND x < 20
```

use intersected constraint/histogram logic rather than multiplying independent estimates.

## 35.15 OR

When independence is the only available model, for:

```text
A = (tA, fA, uA)
B = (tB, fB, uB)
```

SQL OR uses:

```text
t = tA + tB - tA * tB
f = fA * fB
u = 1 - t - f
```

equivalently, UNKNOWN covers the combinations where neither operand is TRUE and at least one is UNKNOWN.

For mutually exclusive/same-column ranges or MCVs, use the stronger union model rather than generic independence.

## 35.16 Same-column constraint sets

Before base-filter estimation, normalize simple predicates on one column into a semantic constraint set containing as applicable:

```text
equality
lower bound
upper bound
IN set
IS NULL / IS NOT NULL
```

Intersect compatible constraints and detect contradictions.

Example:

```text
x >= 10
AND x < 5
```

is provably empty.

The same normalized constraint set feeds B+ access-bound construction in Chapter 36.

## 35.17 Correlated-column limitation

Version 1 statistics are primarily single-column.

Therefore predicates such as:

```text
city = 'Rome'
AND country = 'Italy'
```

may be substantially misestimated under independence.

The optimizer exposes this as a known limitation rather than inventing false precision.

Future extended statistics may include:

```text
multi-column NDV
functional dependencies
multi-column MCVs
```

## 35.18 Projection cardinality

Projection normally preserves row count:

```text
rows_out = rows_in
```

while recomputing output row width from its output expressions/slots.

## 35.19 Filter cardinality

```text
rows_out = rows_in * predicate.true_fraction
```

followed by exact-empty/minimum-nonempty handling.

## 35.20 LIMIT/OFFSET cardinality

When limit/offset are known at planning time:

```text
rows_after_offset = max(0, rows_in - offset)
```

and if LIMIT exists:

```text
rows_out = min(rows_after_offset, limit)
```

Otherwise:

```text
rows_out = rows_after_offset
```

`LIMIT 0` is provably empty.

## 35.21 DISTINCT cardinality

Estimate DISTINCT output from NDV of projected/grouping expressions when lineage/statistics permit.

For one nullable base column:

```text
distinct_groups
≈ NDV + (1 if null_fraction > 0 else 0)
```

capped by input rows.

For multiple columns without extended statistics use the damping rule in §35.23 rather than unrestricted NDV multiplication.

## 35.22 GROUP BY cardinality

For one grouping column:

```text
groups
≈ NDV + (1 if null_fraction > 0 else 0)
groups <= input_rows
```

For multiple columns, first form each component grouping NDV including its possible NULL group, then apply §35.23.

Global aggregation with no grouping columns produces:

```text
1 row
```

even for empty input, matching Chapter 29 aggregate semantics.

## 35.23 Multi-column NDV damping

For multiple grouping/DISTINCT components, sort component NDVs descending:

```text
combined = NDV_1
combined *= NDV_2 ^ 0.75
combined *= NDV_3 ^ 0.50
for every remaining NDV_i:
    combined *= NDV_i ^ 0.25
```

then cap:

```text
combined <= input_rows
```

This is explicitly a heuristic fallback, not claimed correlation knowledge.

Optimizer trace/EXPLAIN diagnostics SHOULD identify when this fallback drove an estimate.

## 35.24 LEFT JOIN cardinality

LEFT JOIN output is bounded below by:

```text
rows_left
```

unless the left input itself is provably empty.

Estimate:

```text
matched_pair_output
+
estimated_unmatched_left_rows
```

The estimator MUST distinguish:

```text
number of matching pairs
```

from:

```text
number of left rows with at least one match
```

A simple bounded probabilistic approximation for unmatched-left rows is acceptable in v1.

## 35.25 Missing-statistics fallback

Missing statistics are explicit optimizer inputs, not fabricated precision.

One centralized `EstimatorFallbackConfig` supplies named fallback assumptions for at least:

```text
unknown equality selectivity
unknown ordered-range selectivity
unknown NULL fraction
generic NDV
```

The values are optimizer configuration/tuning parameters rather than persistent-format constants.

Every fallback result is:

```text
finite
clamped to its legal domain
marked LOW confidence
tagged with MISSING_STATISTICS provenance
```

Fallback assumptions are shown in optimizer trace/verbose EXPLAIN diagnostics.

A fallback never changes SQL semantics or proves a relation empty.

## 35.26 Estimate confidence and provenance

Cardinality/predicate estimates may carry:

```text
EstimateConfidence {
    HIGH
    MEDIUM
    LOW
}
```

plus one or more provenance tags.

Canonical provenance categories include:

```text
PROVEN_CONSTRAINT
MCV_HIT
HISTOGRAM_RANGE
NDV_ESTIMATE
UNIQUE_KEY
INDEPENDENCE_ASSUMPTION
MULTICOLUMN_DAMPING
MISSING_STATISTICS
STALE_STATISTICS
```

Examples:

```text
provable contradiction / enforced key bound:
    HIGH

fresh exact/MCV evidence:
    HIGH

fresh approximate HLL/histogram evidence:
    MEDIUM or HIGH according to estimator rule

independence or multi-column damping:
    LOW

missing-stat fallback:
    LOW
```

V1 plan selection is driven by cost, not by a separate confidence penalty.

Confidence/provenance are retained for diagnostics and regression analysis.

A composite estimate carries the least-confident material assumption that substantially determines the result and retains the relevant provenance chain.

## 35.27 Estimation invariants


1. Estimated rows are nonnegative finite values.
2. Provably-empty state is explicit and not inferred solely from a tiny estimate.
3. Filter selectivity uses SQL TRUE probability, not TRUE+UNKNOWN.
4. Primitive predicate estimators return complete finite TRUE/FALSE/UNKNOWN triples.
5. Equality/range against non-NULL constants assigns column NULL mass to UNKNOWN.
6. IS NULL / IS NOT NULL never produce UNKNOWN.
7. IN-list NULL elements use the exact SQL UNKNOWN rule and NOT IN is derived through NOT.
8. Independence fallback for AND/OR uses the explicit three-valued formulas in §§35.14–35.15.
9. NDV excludes NULL; grouping adds one NULL class when applicable.
10. MCV and residual/histogram mass are not double counted.
11. Same-column constraints are intersected before generic independence assumptions.
12. Correlated-column uncertainty is exposed rather than hidden behind false precision.
13. Multi-column NDV multiplication is damped and capped by input rows.
14. LEFT JOIN cardinality cannot fall below its preserved left input cardinality.
15. Average row width is estimated alongside rows because memory/I/O costs depend on both.

---

# 36. Cost Model and Base Access Paths

## 36.1 Cost philosophy

Version 1 uses calibrated abstract cost units rather than pretending to predict exact milliseconds.

The model separates at least:

```text
sequential persistent I/O
random persistent I/O
CPU tuple/vector work
expression/operator work
hash work
comparison work
memory pressure
temporary spill I/O
```

A scalar comparison cost is a weighted sum using centrally configured/calibrated weights.

The component vector remains available for EXPLAIN/diagnostics.

## 36.2 Cost structure

Represent cost conceptually as:

```text
Cost {
    startup_cost
    run_cost
    total_cost

    seq_page_reads
    random_page_reads
    temp_page_reads
    temp_page_writes

    cpu_rows
    cpu_expressions
    hash_ops
    compare_ops

    estimated_peak_memory
    estimated_spill_bytes
}
```

`startup_cost` is scalar work required before the first output can become available.

`run_cost` is scalar remaining work to exhaust the operator.

```text
total_cost = startup_cost + run_cost
```

for a complete-consumption objective unless a later LIMIT/property rule deliberately evaluates partial-consumption cost.

Component counters are additive where semantically appropriate; peak memory is combined according to operator lifetime/dependency rather than blindly summed.

## 36.3 Cost units

Central configuration contains relative weights such as:

```text
seq_page_cost
random_page_cost
cpu_tuple_cost
cpu_operator_cost
hash_cost
comparison_cost
temp_page_cost
```

The architecture does not permanently lock textbook numeric constants.

Defaults are shipped as configuration and may be calibrated against the engine/hardware.

## 36.4 Calibration

A calibration tool measures at least:

```text
cached sequential scan throughput
cold-ish sequential page reads
random page reads
integer predicate throughput
VARCHAR comparison throughput
hash throughput
sort comparison throughput
temporary spill read/write throughput
```

It proposes relative cost weights.

Calibration results are deployment/configuration data, not persistent database format.

## 36.5 Cache model

Version 1 uses a configured:

```text
effective_cache_pages
```

together with relation/index size to estimate likely caching.

The optimizer MUST NOT read exact momentary BufferPool contents during ordinary planning.

Upper B+ levels may be modeled as more likely cached than heap leaf/data pages.

The cache model should be stable enough that plans do not oscillate merely because a particular page happened to be resident during optimization.

## 36.6 Sequential scan cost

For a table:

```text
physical_tuple_versions
≈ analyzed_live_row_count + dead_version_estimate
```

as a planning approximation.

SeqScan components include:

```text
I/O:
    physical_heap_pages sequential reads

CPU:
    physical_tuple_versions * visibility/header inspection
    + live_rows * pushed predicate work
    + output_rows * required-column decode/materialization work
```

Dead-version pressure therefore raises scan CPU even when logical cardinality is unchanged.

Required-column width/decode cost is applied separately from physical page I/O.

## 36.7 B+ point-lookup cost

Approximate one point lookup as:

```text
tree descent
+
leaf binary/local search
+
candidate RID heap fetches
+
MVCC visibility
+
required tuple decode
```

Tree descent considers:

```text
tree_height
upper-level cache likelihood
key comparison cost
```

Not every upper level is automatically charged as a cold random read.

Even a unique index lookup still visits the heap because v1 index entries do not prove MVCC visibility and there is no index-only visibility path.

A SQL UNIQUE index may still contain multiple physical entries for one user key because aborted/obsolete tuple versions are cleaned asynchronously by vacuum.

Point-lookup costing therefore may inflate expected candidate-RID/MVCC work using the §34.6 physical-versus-live entry pressure when no key-specific garbage statistic exists.

## 36.8 Index range-scan cost

A range-scan estimate includes:

```text
root-to-first-leaf descent
physical leaf pages traversed
physical index entries examined
candidate RID count
estimated invisible/garbage candidates
estimated distinct heap pages fetched
MVCC rejects
required heap decode
residual predicate work
```

Logical result selectivity is estimated from live-row/column statistics.

Physical B+ work is then inflated separately when `IndexStatistics` show accumulated non-live entries.

A baseline derived factor is:

```text
candidate_inflation
    = max(
          1,
          physical_entry_count
          /
          max(1, logical_live_entry_count)
      )
```

and a first physical-candidate estimate may use:

```text
physical_candidates
    ≈ min(
          physical_entry_count,
          logical_candidate_rows * candidate_inflation
      )
```

subject to range/point-specific clamps and stronger measurements.

This is intentionally approximate: global index garbage may not be distributed uniformly across key space.

The optimizer trace should expose when this fallback inflation model materially affects cost.

Estimated leaf pages use:

```text
physical candidate/index-entry estimate
average_entries_per_leaf / leaf occupancy stats
```

where available.

Heap fetch locality uses §36.9 correlation or §36.10 fallback.

## 36.9 Index/heap correlation

When available, `IndexStatistics.leading_key_heap_correlation` is an approximate coefficient:

```text
-1.0 .. +1.0
```

collected from sampled:

```text
(key rank, heap PageNo rank)
```

pairs for an indexed leading key.

High absolute correlation implies range RID fetches tend toward sequential heap-page order.

Low absolute correlation implies near-random access.

The sign describes direction; locality costing primarily uses absolute correlation unless the chosen scan direction makes direction itself relevant.

The initial cost model interpolates conservatively between sequential and random heap-page cost as correlation strengthens rather than applying a binary clustered/unclustered label.

## 36.10 Fallback distinct heap-page estimate

Without useful correlation, assume secondary-index heap candidates are approximately scattered across table pages.

Expected distinct heap pages MUST be capped by:

```text
physical_heap_pages
```

and MUST NOT charge one cold random read per matching row when many rows necessarily share pages.

Use a standard occupancy/distinct-page approximation based on:

```text
candidate rows
physical heap pages
rows-per-page density
```

with stable numerical clamping for empty/small relations.

The exact numerical occupancy approximation is a cost-model implementation detail/calibration choice, not a correctness contract.

## 36.11 Access-predicate classification

Before base access enumeration, classify each applicable predicate as:

```text
index-search condition
scan-pushable condition
residual condition
```

Classification is based on the normalized semantic predicate and the particular index key schema.

A predicate is not index-searchable merely because it mentions an indexed column.

## 36.12 B+ sargability

For a B+ index on:

```text
(a, b, c)
```

a tight search prefix consists of:

```text
zero or more leading equality-like constraints
followed by at most one range-constrained component
```

Equality-like v1 constraints include exact non-NULL equality and `IS NULL` when the key component is nullable, because the index has an exact NULL key representation.

Ordinary:

```sql
a = NULL
```

is never converted to an index NULL equality because its SQL result is UNKNOWN.

Example:

```text
a = 5
AND b = 7
AND c >= 10
AND c < 20
```

produces one tight composite range.

But:

```text
a > 5
AND b = 7
```

uses the range on `a`; `b = 7` is residual in v1 because the leftmost-prefix search cannot skip across the range component.

Expressions/functions on a base column are not searchable through a plain column index unless a future expression-index architecture explicitly supports them.

## 36.13 Composite bounds

Composite index bounds are encoded with the exact Chapter-8 `IndexKeyCodec`.

Conceptually, unspecified trailing user-key components use transient:

```text
LOW_KEY_COMPONENT
HIGH_KEY_COMPONENT
```

search sentinels ordered before/after any valid encoded value for that bound position.

Because physical B+ entries are ordered by:

```text
(user_key, RID)
```

search construction may append transient:

```text
MIN_RID_BOUND
MAX_RID_BOUND
```

to express inclusive/exclusive duplicate-key endpoints.

These bound sentinels are optimizer/B+ cursor search objects only:

```text
they are never persisted as RID/index entries
and are never accepted by persisted RID decoding
```

Bound construction preserves:

```text
inclusive/exclusive endpoint semantics
NULL ordering
ASC key encoding
type/collation semantics
```

## 36.14 Residual predicates

An index-search predicate may coexist with residual predicates.

For:

```text
index(a,b)
WHERE a = 5
  AND b > 10
  AND expensive_function(c)
```

search bounds represent:

```text
a = 5, b > 10
```

while the function predicate remains residual.

Because v1 B+ user-key encoding is exact, a predicate fully proven by exact index bounds need not be re-evaluated merely as a safety check.

Any predicate not fully represented by the selected bounds remains residual.

## 36.15 Base access alternatives

For every `LogicalGet`, enumerate at least:

```text
one PhysicalSeqScan
every semantically usable PhysicalIndexScan
```

Each alternative records:

```text
estimated output rows
estimated average row width
Cost
provided ordering property
required heap ColumnIds/LogicalSlotIds
index-search bounds when applicable
residual predicate
```

No access path is selected solely because an index exists.

## 36.16 One-index baseline

Version 1 chooses at most one B+ index access path for one base relation occurrence.

Deferred:

```text
bitmap index intersection
bitmap index union
index merge
```

Predicates unsupported by the chosen index remain scan/residual filters.

## 36.17 Index/SeqScan break-even

The cost model must naturally permit either of:

```text
few candidate rows + selective/local index access
    -> IndexScan cheaper

many scattered heap candidates
    -> SeqScan cheaper
```

The break-even point depends on relation size, index/heap correlation, candidate density, cache assumptions, required columns, and calibrated CPU/I/O weights.

It is not a hard-coded selectivity percentage.

## 36.18 Required-column cost

Projection pruning changes access cost.

A SeqScan decoding two narrow fixed-width columns has similar page I/O to a scan of twenty wide columns, but substantially less:

```text
tuple decode CPU
vector materialization work
memory traffic
output width
```

IndexScan still fetches heap tuples for visibility in v1, but required output/predicate columns determine how much tuple decoding/materialization work remains after the fetch.

## 36.19 Base-access invariants

1. Cost is a calibrated abstract resource model, not promised wall-clock milliseconds.
2. Cost components remain inspectable rather than collapsing into one unexplained magic constant.
3. Planning does not inspect exact current BufferPool residency.
4. SeqScan costs physical pages/versions as well as logical live rows.
5. B+ point/range access still performs heap MVCC visibility checks.
6. Logical visible-row selectivity and physical B+ entry/candidate pressure are costed as distinct quantities.
7. Range heap locality is costed using correlation when available and a bounded distinct-page fallback otherwise.
8. Access predicates are classified per index schema before path enumeration.
9. Composite B+ search obeys the leftmost equality-prefix + one-range rule.
10. `IS NULL` may form an exact nullable-key search; `= NULL` never does.
11. Search-bound RID/key sentinels are transient and never persisted as actual database identities.
12. Exact index-proven predicates need not be redundantly rechecked; partial predicates remain residual.
13. Every LogicalGet has a SeqScan alternative and every usable single-index alternative.
14. V1 uses at most one index per base relation occurrence.
15. Index-versus-sequential break-even emerges from costs rather than a fixed selectivity threshold.
16. Required-column width/decode work affects cost even when underlying heap-page I/O is similar.

---

# 37. Physical Properties and Join Enumeration

## 37.1 Scope

This chapter defines the physical properties and join-search space used by the v1 cost optimizer.

The v1 property system intentionally remains small.

It tracks:

```text
OrderingProperty
RequiredSlotSet
```

and reserves future extension points for:

```text
partitioning
rewindability
materialization
```

without constructing a full property lattice before those properties are needed.

`required_rows` for LIMIT-sensitive optimization is an optimization objective/requirement, not a physical data property; §38.16 defines how it participates in search identity when propagated.

## 37.2 OrderingProperty

An ordering is an ordered vector of:

```text
OrderKey {
    LogicalSlotId slot
    ASC | DESC
    NULLS_FIRST | NULLS_LAST
    collation
}
```

Physical property reasoning is slot-based.

If SQL ordering refers to a computed expression, logical/physical planning retains or introduces a hidden `LogicalSlotId` for that resolved expression before ordering-property comparison.

V1 collation is the binary VARCHAR collation from Chapter 17.

An empty key vector means:

```text
no required/provided ordering
```

Ordering properties are query-local and never persistent metadata.

## 37.3 Ordering satisfaction

An available ordering satisfies a required ordering exactly when the required keys are a prefix of the available keys and every corresponding key has identical:

```text
LogicalSlotId
direction
NULL order
collation
```

Therefore:

```text
available: (a ASC NULLS FIRST, b ASC NULLS FIRST, c ASC NULLS FIRST)
required:  (a ASC NULLS FIRST, b ASC NULLS FIRST)
```

is satisfied.

Changing direction, NULL order, collation, or slot identity breaks satisfaction.

A longer ordering is useful as a provider of its exact prefixes.

The planner never infers descending B+ order from a forward-only index scan.

## 37.4 RequiredSlotSet

Each physical-planning subproblem has a required set of output `LogicalSlotId` values.

The set contains every value still needed by an ancestor for:

```text
final output
predicate evaluation
join keys/residual predicates
grouping/aggregate arguments
sorting
DML assignments / RETURNING
hidden DML target RID/system state
```

Projection pruning may remove every other value.

For a fixed optimizer invocation, the required slots of a join `RelationSet` are derived deterministically from final requirements and predicates that cross its boundary.

This lets the common join-DP key use:

```text
RelationSet + OrderingProperty
```

while preserving the required-output contract.

## 37.5 Provided-ordering rules

A physical operator advertises only ordering that its runtime implementation actually guarantees.

Baseline rules are:

```text
PhysicalSeqScan:
    no ordering

forward PhysicalIndexScan:
    compatible ascending index-key ordering
    (ASC / NULLS FIRST in the v1 key schema)

PhysicalFilter:
    preserves input ordering

simple/reference PhysicalProject:
    preserves ordering for retained slots
    only while the corresponding ordered keys survive unchanged

PhysicalLimit:
    preserves input ordering

PhysicalSort:
    provides its exact sort ordering

PhysicalTopN:
    provides its exact final ordering

PhysicalHashJoin:
PhysicalHashAggregate:
hash PhysicalDistinct:
    no ordering
```

Nested-loop/index-nested-loop and later merge/ordered-aggregate implementations may advertise additional order only when their execution contract explicitly guarantees it.

The capability registry from §22.4.1 gates such alternatives.

## 37.6 Interesting orders

The optimizer retains some non-cheapest alternatives because their ordering may reduce later work.

Interesting orders originate from:

```text
final ORDER BY
GROUP BY / ordered-aggregate opportunity
merge-join key requirements
usable index access order
DISTINCT / ordered-distinct opportunity
```

Only normalized orderings that can satisfy a known current/downstream requirement are retained as interesting.

Arbitrary orderings are not generated merely to enlarge the memo.

If one available ordering satisfies another, dominance may treat the longer ordering as at least as useful when all other relevant requirements are equal.

## 37.7 RelationSet

A reorderable join region assigns one bit position to each participating relation occurrence.

Identity is:

```text
BindingId
```

not `TableId`.

A self-join therefore consumes two different relation bits.

`RelationSet` is a value-like bitset abstraction supporting:

```text
union
intersection
difference
subset
cardinality
deterministic iteration
```

The concrete bitset representation is implementation-specific and MUST support the configured large-join planning limit rather than silently truncating relation identities.

## 37.8 Join graph

For a maximal INNER/CROSS reorderable region, construct:

```text
vertices:
    BindingId relation occurrences

edges:
    join predicates spanning relation sets

vertex predicates:
    local filters
```

Safe equality-equivalence predicates derived by §20.17.7 may add connectivity or access opportunities.

Each edge/predicate retains the exact referenced `RelationSet` so a DP partition can determine whether that predicate crosses its two sides.

LEFT JOIN and other non-reorderable semantic boundaries are represented as constrained/atomic inputs rather than silently flattened into this unrestricted graph.

## 37.9 Reorderable regions and outer-join constraints

The optimizer searches maximal regions in which INNER/CROSS associativity/commutativity is semantically legal.

A LEFT JOIN boundary retains its logical nesting relative to dependent relations in v1.

Within each side of that boundary the optimizer may still choose:

```text
base access paths
inner-join order
physical join algorithm
```

The optimizer does not reorder across the outer-join boundary unless an earlier, independently proven logical rewrite converts it to semantics where that reorder is valid.

Advanced outer-join reordering is deferred.

## 37.10 Bushy dynamic programming

For a small reorderable `RelationSet S`, use bottom-up bushy subset DP.

For every subset size from one upward:

```text
for each subset S:
    for each legal non-empty partition:
        S = A ∪ B
        A ∩ B = ∅

        combine retained useful-property plans
        from A and B
```

Symmetric partitions are skipped by one deterministic rule, for example requiring the least BindingId bit in `S` to belong to `A`.

Partitions connected by join predicates are enumerated before Cartesian alternatives.

The search considers bushy trees such as:

```text
(A ⋈ B) ⋈ (C ⋈ D)
```

rather than restricting the optimizer to left-deep plans.

## 37.11 Exhaustive threshold

The initial configurable default is:

```text
exhaustive_join_limit = 10 relation bindings
```

within one reorderable region.

Below/equal to the threshold, exhaustive bushy DP is the baseline unless the optimizer planning-memory budget forces an earlier bounded fallback.

The threshold is a planning-tuning parameter, not a SQL semantic limit.

Raising it requires planning-time/memory evidence.

## 37.12 Cartesian products

A partition with no crossing join predicate is a Cartesian alternative.

While a connected predicate-join alternative is available for the same unresolved components, the optimizer prefers connected partitions and does not introduce an unnecessary Cartesian product.

Cartesian products remain legal where required by the logical query graph.

Their logical cardinality begins from:

```text
rows_A * rows_B
```

before later applicable filtering.

They receive the corresponding materialization/CPU/memory cost rather than an arbitrary correctness-changing ban.

## 37.13 Large-join heuristic

Above the exhaustive threshold—or when exhaustive search reaches the planning-memory guard—the optimizer switches to a bounded deterministic heuristic.

Baseline:

1. construct a greedy connected initial tree using lowest estimated incremental objective cost,
2. use a stable structural tie-break,
3. apply a configurable bounded number of local-improvement passes,
4. local moves may include:
   ```text
   adjacent swap
   join rotation
   limited subtree exchange
   ```
5. stop early when a complete pass finds no improving legal tree.

Initial default:

```text
large_join_max_local_passes = 4
```

This is optimizer configuration, not persistent format.

A small deterministic beam may be added later, but v1 does not require one for correctness.

Disconnected query regions introduce Cartesian edges only when logically necessary.

## 37.14 Join algorithm alternatives

For every legal binary join pair, enumerate every **implemented** applicable algorithm:

```text
PhysicalHashJoin
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalMergeJoin when capability-enabled and predicates/order permit
```

Join order and physical join algorithm are costed together.

The optimizer does not first freeze a complete join tree and only afterward choose algorithms if doing so would discard a cheaper order/algorithm combination.

## 37.15 Hash-join orientation

For an INNER equijoin, consider both semantically equivalent orientations when the runtime supports them:

```text
build left  / probe right
build right / probe left
```

Cost includes:

```text
build rows
build payload width
memory/spill
probe rows
downstream properties
```

For the baseline LEFT hash join, Chapter 28 fixes:

```text
logical right -> build
logical left  -> preserved probe
```

and the optimizer enumerates only that supported orientation unless a separate semantic transformation changes the logical join.

## 37.16 Logical join cardinality is algorithm-independent

For the same logical:

```text
A ⋈ B ON predicate P
```

the estimated output cardinality is identical regardless of whether the physical algorithm is:

```text
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
MergeJoin
```

Algorithm choice changes:

```text
cost
memory
spill
provided ordering
startup behavior
```

not logical row semantics.

Cardinality is cached by logical relation/predicate identity where practical so alternative physical algorithms do not repeatedly invoke different estimators for the same subproblem.

## 37.17 Subquery physical planning

Supported uncorrelated scalar/EXISTS/IN subqueries are optimized once as independent physical subplans.

Their cost includes applicable:

```text
startup
materialization
hash-set / comparison work
```

and uses their ordinary output/cardinality contracts.

Correlated execution remains deferred.

A correlated form MUST NOT be costed as if it executes once when its semantics would require repeated execution.

Without prepared-statement parameters, v1 optimization sees literal constants directly and may use MCV/histogram information for those literals.

## 37.18 Join/property invariants

1. V1 physical properties track ordering and required output slots without a premature full property lattice.
2. Ordering satisfaction is exact prefix matching on slot, direction, NULL order, and collation.
3. Only runtime-guaranteed ordering is advertised.
4. Interesting orders retain useful non-cheapest alternatives but do not create unbounded arbitrary order classes.
5. Join relation identity uses BindingId, not TableId.
6. Exhaustive search enumerates bushy trees for small reorderable regions.
7. The initial exhaustive threshold is configurable and defaults to 10 relation bindings.
8. Large-join search is bounded and deterministic.
9. Unnecessary Cartesian joins are avoided while connected alternatives exist.
10. INNER join order and physical algorithm/orientation are optimized together.
11. LEFT JOIN semantic boundaries and supported hash orientation are preserved.
12. Logical join cardinality is independent of the physical join algorithm.
13. Only capability-enabled physical algorithms enter the plan search.
14. Correlated subqueries are never mis-costed as one-time uncorrelated execution.

---

# 38. Memo, Costed Physical Search, and Memory-Aware Optimization

## 38.1 Search requirements and memo identity

A physical search request is conceptually:

```text
SearchRequirement {
    RequiredSlotSet
    OrderingProperty required_order
    RequiredRowsObjective required_rows
}
```

`RequiredRowsObjective` is:

```text
ALL_ROWS
or
FIRST_K_ROWS(K)
```

and is not itself a physical property.

When a finite required-rows objective is semantically propagated into a subproblem, it participates in memo/search identity because a low-startup plan may be preferable to the full-result cheapest plan.

For join enumeration, the common memo identity is conceptually:

```text
RelationSet
+
required output-slot class
+
required/interesting OrderingProperty class
+
propagated RequiredRowsObjective class
```

When required slots are uniquely derived for the RelationSet and no finite row objective is propagated, this reduces to the traditional:

```text
RelationSet + OrderingProperty
```

System-R-style key.

## 38.2 PlanAlternative

Every retained physical alternative stores at least:

```text
logical subproblem identity
physical plan prototype
estimated rows
estimated width
Cost
provided OrderingProperty
required output slots
estimated peak memory
estimated spill bytes
capability/feasibility state
canonical structural tie key
```

Plan prototypes are optimizer/query-planning-arena objects.

They contain no execution-time mutable operator state.

## 38.3 Dominance

For the same logical/search requirement, plan `A` dominates plan `B` only if:

```text
A is semantically valid
A satisfies every requirement that B satisfies
A's provided ordering is at least as useful for the active property class
A is no worse under the active cost objective
A has no relevant feasibility disadvantage
```

For full-result optimization, the main objective is total cost.

For a propagated `FIRST_K_ROWS` objective, startup/partial-run cost participates in the comparison.

A cheaper unordered plan therefore does **not** dominate a slightly more expensive interesting-order plan that can avoid a downstream sort.

Likewise, a lower-total-cost high-startup plan does not automatically dominate a low-startup alternative when the memo key carries a finite required-rows objective.

Dominated alternatives are discarded promptly.

## 38.4 Cost ties and deterministic choice

All optimizer costs must remain:

```text
finite
nonnegative
```

A configurable relative tolerance defines an effective cost tie.

Initial default:

```text
cost_tie_relative_epsilon = 1e-9
```

Two scalar objective costs `a` and `b` are tied when:

```text
abs(a - b)
<=
epsilon * max(1, abs(a), abs(b))
```

Tied alternatives use a deterministic canonical structural key.

Hash-map iteration order, allocator addresses, worker timing, and pointer values MUST NOT choose a plan.

## 38.5 Canonical structural key and plan fingerprint

Every physical plan can produce a deterministic structural serialization for diagnostics/tie-breaking.

It includes, as applicable:

```text
physical operator names
stable TableId/IndexId identities
join tree/orientation
access path identity
search-bound semantics
ordering keys
important operator parameters
child structural keys
```

It excludes:

```text
memory addresses
unordered-container iteration order
runtime execution counters
```

The lexicographic structural key is the final collision-free tie-break inside one optimizer build/configuration.

A debug:

```text
PlanFingerprint
```

may additionally hash this canonical serialization with FNV-1a-64 for compact display/regression tracking.

The hash alone never decides correctness or tie order because collisions are possible.

## 38.6 Join-DP initialization

For every base relation occurrence:

1. compute the logical base-filter cardinality once,
2. enumerate `PhysicalSeqScan`,
3. enumerate every semantically usable implemented `PhysicalIndexScan`,
4. cost each access path,
5. attach its provided ordering,
6. retain non-dominated alternatives for relevant interesting-order/search requirements.

The same logical base cardinality is shared by all physical access alternatives.

A scan method never receives a different SQL row estimate merely to make its cost more attractive.

## 38.7 Join-DP transition

For subsets `S` in increasing cardinality:

```text
for each legal deterministic partition S = A ∪ B:
    identify crossing join predicates
    obtain logical join cardinality
    enumerate applicable implemented join algorithms/orientations
    obtain child alternatives satisfying algorithm requirements
    add property enforcement where required
    estimate width / memory / spill
    cost the physical alternative
    insert if non-dominated
```

Cardinality is cached by logical relation/predicate identity when possible.

Physical costs/properties remain alternative-specific.

The transition does not mutate the logical plan or statistics snapshot.

## 38.8 Hash-join cost

Estimate:

```text
build child objective cost
probe child objective cost
+
build_rows * hash/build CPU
+
probe_rows * hash/probe CPU
+
expected matches * output/materialization CPU
+
residual predicate CPU
+
estimated memory/spill cost
```

Build payload width is the pruned width from §38.18 rather than the full base-row width.

Hash-table setup has nonzero startup cost.

For INNER joins, both supported build orientations are costed.

For LEFT joins, only the supported preserved/probe orientation is enumerated.

## 38.9 Hash-join memory and spill

A conservative baseline build-memory estimate uses:

```text
build_rows
*
(
    pruned build RowLayout width
    +
    hash-directory overhead
    +
    duplicate-chain metadata
)
/
target load factor
```

including average varlen widths.

When usable join-key NDV exists, implementations may refine directory-entry count separately from duplicate-row storage, but must not undercount the actual retained build payload.

Compare estimated required memory with the planner-assigned operator memory target.

If required memory exceeds the target:

```text
spill expected
```

and add:

```text
partition write(build + probe bytes)
partition read(build + probe bytes)
repartition hash CPU
per-partition rebuild/probe CPU
```

If one partitioning pass cannot make the largest expected partition fit, estimate additional bounded recursive repartition passes using the execution partitioning/fanout configuration.

## 38.10 Nested-loop cost

For a materialized inner side:

```text
outer child cost
+
inner child/materialization cost
+
outer_rows * inner_rows * predicate CPU
+
output/materialization CPU
```

This can beat hash join for very small inner inputs because hash construction has real startup/memory cost.

Materializing/copying the inner side is never free.

## 38.11 Index nested-loop cost and repeated-key locality

Estimate:

```text
outer child cost
+
outer_rows
*
(
    inner index lookup/range cost
    +
    expected heap fetch/MVCC cost
    +
    residual predicate CPU
)
```

When outer join-key NDV is substantially below `outer_rows`, repeated lookups may reuse:

```text
B+ upper paths
leaf pages
heap pages
```

The cost model may apply a calibrated, bounded locality reduction derived from:

```text
outer_rows
outer_key_NDV
index/heap correlation
```

but never assumes every lookup is fully cold or fully free.

INLJ is attractive when the outer input is small and the indexed inner lookup is selective.

## 38.12 Merge-join cost and properties

MergeJoin is enumerated only when the runtime capability is enabled and the predicate/operator supports it.

If both children already provide the required compatible join-key order:

```text
left child cost
+
right child cost
+
linear merge CPU
+
duplicate-group handling
```

If an input lacks the required ordering, add the applicable `PhysicalSort` enforcement cost.

Merge join may win even with similar raw join cost when its provided order is interesting downstream.

Its advertised output ordering is only the ordering guaranteed by the enabled runtime implementation.

## 38.13 Sort and Top-N cost

For full sort, approximate in-memory CPU as:

```text
N * log2(max(N, 2)) * comparison_cost
```

adjusted by key type/width.

Memory includes:

```text
N * pruned sort-record width
+
owned payload / varlen storage
```

If the assigned memory target is insufficient, add:

```text
run-generation writes
run reads
merge-pass temporary I/O
merge comparison CPU
```

For:

```sql
ORDER BY ... LIMIT N OFFSET O
```

the Top-N retention target is the checked:

```text
K = N + O
```

and approximate heap maintenance is:

```text
input_rows * log2(max(K, 2))
```

with memory roughly proportional to:

```text
K * retained row width
```

Top-N is selected by total objective cost when `K` is materially smaller than the full input; SQL syntax alone does not force it.

## 38.14 Aggregate and DISTINCT cost

### HashAggregate

Estimate:

```text
child cost
+
input_rows * hash/update CPU
+
estimated_groups * allocation/finalize CPU
+
spill cost when group state exceeds assigned memory
```

Memory derives from:

```text
estimated_groups
*
(
    grouping key RowLayout width
    +
    exact aggregate state size/alignment
    +
    hash overhead
)
```

Aggregate descriptors provide their real state size/alignment.

### SortAggregate

Enumerate only when `PhysicalSortAggregate` capability is enabled.

If child order already satisfies grouping keys:

```text
child cost + approximately linear streaming aggregate CPU
```

with low state memory.

Otherwise compare:

```text
Sort enforcement
+
streaming aggregate
```

against hash aggregation.

### DISTINCT

When implementations exist, compare:

```text
hash PhysicalDistinct
```

with:

```text
ordered input / Sort
+
streaming ordered DISTINCT
```

and retain any useful resulting ordering.

## 38.15 Ordering enforcement and final ORDER BY

If an alternative does not naturally satisfy a required ordering:

```text
insert PhysicalSort
```

and include its startup, CPU, memory, and spill cost.

When the root requirement is:

```text
ORDER BY + finite LIMIT/OFFSET
```

the planner also considers `PhysicalTopN` where semantically equivalent.

For final ORDER BY compare:

```text
cheapest unordered plan + enforcement
```

against:

```text
possibly more expensive naturally ordered plan
```

such as an ordered IndexScan or capability-enabled MergeJoin/ordered aggregate.

The lowest active objective cost wins.

The optimizer never inserts Sort merely because an ORDER BY exists if an already-provided property satisfies it.

## 38.16 RequiredRowsObjective and startup cost

Every Cost retains:

```text
startup_cost
run_cost
total_cost
```

where:

```text
total_cost = startup_cost + run_cost
```

For full-result planning:

```text
objective = total_cost
```

For a semantically safe finite root requirement:

```text
required_rows = LIMIT + OFFSET
```

with checked arithmetic.

A streaming/early-terminating plan may use a partial objective:

```text
startup_cost
+
fraction_of_run_cost_needed_for_required_rows
```

with the fraction clamped to `[0,1]`.

Blocking operators such as full Sort, HashAggregate, DISTINCT build, and the build side of HashJoin require their necessary blocking input before producing rows and therefore cannot pretend their blocking work scales linearly with the final LIMIT.

Propagation is conservative:

```text
Project:
    may preserve FIRST_K_ROWS

Filter/Scan:
    may use selectivity-based partial-consumption costing

Sort/Aggregate/Distinct:
    stop propagation across the blocking semantic boundary

Join:
    no general required_rows propagation in the v1 baseline
    unless a specific physical algorithm proves a safe rule
```

Finite `required_rows` affects cost/search only.

It never changes result semantics or becomes an executor row cap in a location where SQL does not permit one.

## 38.17 Predicate CPU ordering

Bound/physical expressions carry an approximate evaluation-cost score.

Examples include:

```text
integer comparison        cheap
VARCHAR comparison        width-sensitive
IMMUTABLE scalar function descriptor/default cost
subquery                   separately costed
```

For a conjunction whose components are:

```text
IMMUTABLE
safe to reorder
not evaluation-order/error sensitive
```

the optimizer may rank predicates by a metric such as:

```text
evaluation_cost / rejection_probability
```

so cheap/selective predicates execute earlier.

VOLATILE or semantically error-sensitive expressions retain their required evaluation behavior.

## 38.18 Output-width and payload pruning

Physical costs use the materialized values actually required by the chosen plan.

Join output width is approximately:

```text
required left output width
+
required right output width
```

after projection pruning.

A HashJoin build row stores only:

```text
join-key data required for equality validation
build payload slots required by residual/downstream consumers
duplicate/hash metadata
```

rather than the complete base row.

Sort retains only:

```text
sort keys
downstream-required payload
```

or a stable compact row handle where the execution lifetime contract permits it.

HashAggregate uses exact aggregate state sizes/alignment.

Projection pruning therefore directly changes physical memory/spill cost.

## 38.19 Memory target assignment and pipeline-aware peak

Planner memory is a query-level budget, not one independent full budget per blocking operator.

Using the physical pipeline-dependency graph, identify blocking states that may coexist.

For each simultaneously-live blocking phase, v1 assigns deterministic rough operator targets by:

1. giving each active blocker at most its estimated required memory,
2. distributing the query planning memory budget across active blockers,
3. redistributing unused share to still-unsatisfied blockers in stable structural order.

The sum of assigned targets for one modeled simultaneous phase does not exceed the configured planning query-memory budget.

Peak query memory is estimated as the maximum modeled simultaneous phase, not the sum of all blocking operators in the plan.

A conservative overestimate is permitted and is exposed in diagnostics.

Runtime `QueryMemoryManager` remains authoritative for actual reservations; planner targets are estimates used for selection.

## 38.20 Spill and materialization cost

V1 spill expectation is deterministic:

```text
estimated_required_memory
>
assigned_memory_target
    -> spill expected
```

Estimate bytes/passes according to the relevant execution algorithm.

Materialization costs include:

```text
RowCollection append/copy
VARCHAR deep copy
temporary serialization
spill block formatting/checksum work
```

Comparison among NestedLoop materialization, HashJoin build, Sort, aggregate, and DML spool therefore never treats retained data as free.

The cost model may be inaccurate; it may not bypass a runtime memory hard limit.

## 38.21 Planning time and memory budget

Optimization itself is resource-bounded.

Track:

```text
optimization wall time
logical subproblems explored
partitions considered
physical alternatives costed
memo entries retained/pruned
peak planning-arena bytes
```

The optimizer uses a dedicated planning arena with a configurable upper budget separate from execution memory.

If exhaustive join DP approaches either:

```text
exhaustive_join_limit
planning arena budget
```

the remaining join-region search switches to §37.13's bounded heuristic.

If even bounded planning cannot fit within the configured planning resource limit, planning fails with a controlled `OptimizerResourceLimit` error rather than arbitrary process OOM.

Optimizer resource fallback never changes SQL semantics.

## 38.22 Missing/stale statistics in search

Search consumes one immutable statistics snapshot for the entire optimization.

Missing estimates use §35.25's centralized fallback configuration.

Confidence/provenance from §35.26 is retained on estimates and exposed to diagnostics.

Statistics staleness may reduce diagnostic confidence and influence explicitly configured cost heuristics, but v1 does not automatically reject a plan or rewrite statistics because one query later observes a different actual row count.

One query's runtime cardinalities never directly rewrite persistent statistics in v1.

## 38.23 Optimizer trace and diagnostics

The optimizer can emit a debug trace containing at least:

```text
normalized predicates
estimate confidence/provenance
base cardinalities
enumerated access paths
cost components
join subsets/partitions explored
algorithms/orientations considered
alternatives pruned by dominance
interesting orders retained
memory targets / expected spill
property enforcement
final selected plan
canonical plan fingerprint
planning time/memory counters
```

A SQL/debug surface may expose this as:

```text
EXPLAIN (OPTIMIZER TRACE)
```

or an equivalent internal hook.

The exact textual presentation may evolve.

The trace is diagnostic output, not a stable persistent format.

## 38.24 Final physical-plan validation

Before execution, optimizer validation proves at least:

```text
logical output semantics preserved
all required LogicalSlotIds present
required order satisfied or explicitly enforced
join types/outer constraints preserved
predicates assigned only where semantically legal
DML hidden target slots preserved
every selected physical algorithm is capability-enabled
memory/spill annotations are finite/nonnegative
```

The result then passes to the execution-layer `PhysicalPlanValidator`.

A cost mistake may choose a slow valid plan.

A rewrite/legality mistake that changes results is a correctness failure.

## 38.25 Memo/search invariants

1. Memo alternatives are keyed by logical subproblem plus the requirements that can change the preferred physical plan.
2. A finite required-rows objective participates in search identity wherever it is propagated.
3. Dominance never discards a useful ordering or low-startup alternative required by the active search objective.
4. Costs remain finite/nonnegative and near-ties use a deterministic stable rule.
5. Hash-map iteration order and pointer addresses never determine the chosen plan.
6. The structural tie key is collision-free for comparison; the compact PlanFingerprint is diagnostic only.
7. Base alternatives share one logical cardinality estimate regardless of scan algorithm.
8. Join output cardinality is independent of join algorithm.
9. Physical algorithm availability is explicitly gated by runtime capability.
10. Hash/spill costs use pruned payload widths and assigned memory targets.
11. Sort/Top-N/aggregate/DISTINCT alternatives include required property-enforcement cost.
12. Full-result and first-K optimization objectives are distinct when semantically applicable.
13. Planning memory is bounded and can trigger heuristic search rather than unbounded growth.
14. One optimization uses one stable statistics snapshot.
15. Missing/stale statistics remain explicit low-confidence assumptions, not hidden precision.
16. Optimizer diagnostics expose why estimates/alternatives were selected or pruned.
17. The optimizer may choose a slow plan but may never change SQL meaning.
18. Final plans pass optimizer semantic/property validation before execution.
---

# Part VIII — Cross-Cutting Requirements

# 39. Error and Corruption Model

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

## 39.3 Execution errors

The execution layer distinguishes controlled failures including:

```text
ExecutionError
OutOfMemory
SpillIOError
QueryCancelled
CardinalityViolation
CastError
ArithmeticError
ConstraintViolation
TransactionConflict
```

Lower-layer structured causes are preserved rather than erased. Examples include:

```text
CardinalityViolation -> SQL CardinalityError surface
ConstraintViolation  -> may contain UniqueViolation
TransactionConflict  -> may contain SerializationFailure / DeadlockVictim
```

RAII/lifetime ownership unwinds query-owned chunks, row collections, reservations, spill files, pipeline tasks, and operator state.

Execution failure does not independently release transaction-owned logical locks; the command/transaction layer drives the required abort/terminal path.

No partially active pipeline/task graph remains runnable after a terminal query execution failure.

### 39.3.1 Checked integer arithmetic

Integer kernels MUST NOT rely on C++ signed-overflow undefined behavior.

Operations that can overflow use checked arithmetic for:

```text
+
-
*
unary minus
MIN_INT / -1
aggregate integer accumulation/finalization
```

Overflow raises `ArithmeticError`.

Integer division or remainder by zero raises `ArithmeticError`.

### 39.3.2 FLOAT64 arithmetic/division

FLOAT64 arithmetic follows the database's IEEE-754 binary64 semantics.

In particular, v1 FLOAT64 division by zero follows IEEE results rather than the integer division rule:

```text
finite nonzero / ±0.0 -> signed infinity
±0.0 / ±0.0          -> NaN
infinity / infinity   -> NaN
```

NaN/zero comparison/grouping/index semantics remain those defined in Chapters 8, 17, 20, and 29.

Compiler modes that silently replace these semantics are not allowed.

## 39.4 Optimizer errors

Optimizer planning may fail with controlled internal/resource categories such as:

```text
OptimizerError
OptimizerResourceLimit
```

`OptimizerResourceLimit` means the configured planning-time/memory bound prevented completion even after the bounded heuristic fallback.

It is not an out-of-process arbitrary allocation crash.

A cost-model mistake that merely selects a slower semantically valid plan is **not** an optimizer runtime error.

A final-plan semantic/property-validation failure is an internal invariant failure and MUST NOT be executed.

---

# 40. Observability and EXPLAIN

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

## 40.6 Execution profiling

Every executed physical operator records at least:

```text
input rows
output rows
chunks
wall time
CPU time where available
peak operator/query memory attribution
spill bytes
```

Operator-specific counters include useful dimensions such as:

```text
SeqScan:
    heap pages visited
    visible tuples
    MVCC rejects
    predicate rejects

IndexScan:
    index entries visited
    heap RIDs fetched
    invisible candidate RIDs

HashJoin:
    build/probe rows
    output matches
    hash collisions
    directory load factor
    spill partitions/bytes
    skew/repartition count

Aggregate:
    input rows
    groups
    collisions
    spill bytes

Sort:
    rows
    comparisons
    runs
    spill bytes
    merge passes
```

Parallel operators accumulate hot counters in worker-local state and combine them at synchronization/finalization boundaries rather than requiring one atomic increment per row.

### 40.6.1 EXPLAIN ANALYZE

`EXPLAIN ANALYZE` executes the selected physical plan and reports both estimates and actual execution information, including:

```text
estimated rows
actual rows
operator time
memory
spill behavior
important operator-specific counters
```

Actual counters come from the physical operators that really executed, not from logical estimates copied into an output template.

### 40.6.2 Pipeline profiling

Pipeline-level profiling records at least:

```text
pipeline ID
dependency wait time
execution time
chunks processed
worker count
morsels/tasks
```

This distinguishes CPU work from blocker/dependency waiting and scheduler imbalance.

## 40.7 Statistics, estimation, and base-access diagnostics

Optimizer diagnostics expose enough information to explain base-plan choices, including:

```text
StatsVersion / analyzed schema version
stats staleness ratio
table live rows / physical pages / dead-version estimate
column null fraction / NDV / MCV / histogram availability
index physical/live/invisible entry pressure
index leaf occupancy / heap correlation when available
predicate truth/selectivity estimate
provably-empty flag
estimated rows and row width
SeqScan and candidate IndexScan cost components
chosen search bounds / residual predicates
provided index ordering property
```

EXPLAIN should show the chosen estimated rows/cost and may show rejected base alternatives in verbose/debug optimizer trace mode.

Diagnostics identify when important fallback assumptions are used, including:

```text
stale/missing statistics
independence assumption
multi-column NDV damping
missing index/heap correlation
```

These diagnostics explain optimizer behavior; they do not change semantics.

## 40.8 Optimizer trace, plan fingerprint, and estimate error

Normal physical EXPLAIN shows at least:

```text
physical operator
estimated rows
estimated width
startup cost
total cost
estimated peak/operator memory where relevant
estimated spill where relevant
provided/required ordering where useful
```

Verbose/debug optimizer trace additionally exposes the search/estimate information from §38.23.

`EXPLAIN ANALYZE` reports for every physical node:

```text
estimated rows
actual rows
q-error
```

For positive estimate `E` and actual `A`:

```text
q_error = max(E / A, A / E)
```

Zero handling is exact:

```text
E = 0 and A = 0:
    q_error = 1

exactly one of E/A is 0:
    q_error = infinity / explicit infinite marker
```

Estimate-attribution output should make the earliest material divergence visible together with confidence/provenance, because an upper join-order failure may originate in one lower predicate estimate.

Runtime actual rows are diagnostic only.

V1 does not automatically write persistent statistics from one query's actual cardinalities.

The canonical plan fingerprint is the compact debug hash defined by §38.5 and is suitable for regression comparison, not as a collision-free semantic identity.

---

# 41. Verification Requirements

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

Physical slot reuse tests MUST follow the Chapter-14 grace protocol and persisted free-slot list; immediate DEAD-slot reuse is not a valid test expectation.

Detailed test fixtures, exact test counts, and benchmark procedures are maintained in `VERIFICATION.md`; this chapter states the architecture-level verification obligations.


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

Fuzzing targets at least:

```text
lexer
parser
literal/type conversion
bound constant evaluator
```

with no crashes/out-of-bounds access and bounded failure behavior on arbitrary input.

## 41.5 Physical-execution verification obligations

Before execution, a physical-plan validator checks at least:

```text
child/output LogicalSlotId compatibility
physical expression types
required hidden target RID presence
join-key type compatibility
requested join-type support
ordering-property consistency
source/streaming/sink pipeline legality
breaker dependency legality
memory/spill capability declarations
required transaction/query context
```

Validation occurs before data-changing side effects.

Execution verification includes synthetic operator/chunk tests, manual physical-pipeline tests, SQL end-to-end tests, differential tests where semantics align, forced-spill tests under tiny query budgets, cancellation tests, and single-worker/parallel equivalence tests.

Vector kernels are exercised over:

```text
FLAT / CONSTANT / DICTIONARY
all-valid / all-NULL / mixed validity
identity and non-identity selection
nested dictionary normalization
empty and full-size chunks
```

String-lifetime tests deliberately unpin source pages and recycle source chunks before downstream blocking use.

Join tests cover:

```text
no/one/many matches
many-to-many duplicate keys
NULL/composite keys
forced hash collisions
residual predicates
LEFT unmatched rows
one probe row producing >1 chunk
Grace spill
skew/repartition fallback
```

Randomized small join results are compared with the nested-loop reference algorithm.

Aggregate tests cover empty input, global/grouped aggregation, NULL grouping keys, all-NULL inputs, composite/VARCHAR/FLOAT64 keys, partial Combine, and forced spill.

Sort tests cover ASC/DESC, NULL order, multi-key ties, VARCHAR/FLOAT64 edges, in-memory/external runs, and multi-pass merge using the semantic comparator as oracle.

DML execution tests specifically prove Halloween protection and one-target-once behavior, including index-key-changing UPDATE, target-spool spill, concurrent revalidation, the Chapter-15 READ COMMITTED retry boundary, REPEATABLE READ conflict abort, unique-key updates, and RETURNING suppression on failed/restarted attempts.

Forced-spill/cancellation/error tests verify temporary resources are cleaned without corrupting the transaction/storage layer.

## 41.6 Statistics, estimator, and base-access verification obligations

Statistics verification covers at least:

```text
exact small-table NDV/frequencies
HLL error distribution
SpaceSaving heavy-hitter capture
reservoir/histogram boundary monotonicity
NULL exclusion from NDV
MCV + residual mass consistency
VARCHAR/FLOAT ordering consistency
stable immutable stats publication during concurrent ANALYZE/planning
aborted ANALYZE does not publish globally
statistics chunk missing/duplicate/reordered corruption
statistics payload CRC/version/scalar-codec validation
physical index entry pressure versus live-row baseline
```

Cardinality-estimator tests use synthetic distributions with analytically known answers or reference counts for:

```text
equality / MCV hits
ranges / histogram interpolation
IS NULL / IS NOT NULL
IN lists with and without NULL list members
exact primitive TRUE/FALSE/UNKNOWN triples
NOT / AND / OR under SQL 3VL, including independence formulas
same-column contradictions
unique-key joins
MCV-skewed joins
DISTINCT / GROUP BY NDV
LEFT JOIN lower bound
```

Estimator quality is measured with q-error or an equivalent multiplicative error metric, while exact-zero/provably-empty cases are tested separately.

Base-access tests compare enumerated SeqScan/IndexScan alternatives across:

```text
selectivity
relation size
required column width
index/heap correlation
dead-version pressure
physical B+ garbage/invisible-entry pressure
cache configuration
```

and assert that no fixed selectivity threshold controls the decision.

Composite-bound tests compare B+ cursor results against the normalized SQL predicate for inclusive/exclusive, duplicate RID, NULL, and multi-column leftmost-prefix cases.

## 41.7 Join-search, properties, memo, and optimizer verification obligations

Optimizer verification includes controlled scenarios for:

```text
bushy join-order choice
large-join bounded heuristic transition
Cartesian-product legality
LEFT JOIN search constraints
both INNER hash-build orientations
INLJ selective/small-outer cases
interesting-order retention
index-order ORDER BY avoidance
Sort enforcement
Top-N alternatives
HashAggregate versus capability-enabled ordered aggregate
hash versus ordered DISTINCT
memory-budget-driven spill/plan changes
required_rows / LIMIT startup objective
missing-statistics fallback/confidence/provenance
deterministic ties and plan fingerprints
planning-memory fallback
```

Synthetic join-estimation suites include:

```text
unique-to-many
many-to-many
hot-key skew
disjoint domains
partial domain overlap
NULL-heavy keys
duplicate-heavy MCVs
```

and compare estimator q-error with known/reference results.

For small generated schemas/data, differential optimizer correctness tests:

1. generate a supported logical query,
2. execute the optimizer-selected physical plan,
3. execute a trusted simple reference plan using semantically equivalent scans/nested loops/straightforward operators,
4. compare results.

This separates rewrite/search correctness from cost quality.

Optimizer fuzzing covers:

```text
logical expression trees
predicate combinations
legal statistics values
join graphs
ordering requirements
memory budgets
```

and requires:

```text
no crash
no NaN/negative cost escaping
no invalid final physical plan
bounded planning behavior above the exhaustive threshold
```

Plan regression scenarios record:

```text
schema
statistics
query
important expected plan properties
plan fingerprint where stable
```

without asserting arbitrary exact floating cost numbers unless the cost formula itself is under test.

---

# 42. Performance Requirements

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

## 42.4 Execution measurements and hot-path constraints

Execution microbenchmarks measure at least:

```text
scan rows/sec and bytes/sec
filter/projection arithmetic rows/sec
VARCHAR comparison rows/sec
hash build/probe/output rows/sec
aggregate input rows/sec and groups/sec
in-memory sort rows/sec
external sort MB/sec
allocator calls/chunk allocations/temporary bytes per query
```

Benchmarks include NULL-free and NULL-heavy data.

The vector-size study includes at least:

```text
256
512
1024
2048
4096
```

rows/chunk on representative workloads.

The architecture default remains 1024 until measurements justify changing it.

End-to-end workloads include point/index-range lookup, full scan, filter/project, small and large joins, group aggregation, ORDER BY, ORDER BY LIMIT, index-driven UPDATE, bulk INSERT, and concurrent read/write.

Track at least:

```text
latency
throughput
CPU
peak query memory
buffer hits/misses
WAL bytes
spill bytes
```

Parallel scaling is measured at representative worker counts such as:

```text
1, 2, 4, 8, ...
```

and used to identify scheduler, synchronization, cache, and memory-bandwidth limits.

Strong implementation constraints are:

1. no one heap allocation per execution row or cell,
2. no virtual dispatch/type switch per hot-loop row when batch specialization is available,
3. no generic `Value` construction per hot cell,
4. do not pin heap pages merely to retain VARCHAR execution references,
5. blocking operators own retained varlen data,
6. large operator memory is QueryMemoryManager-accounted,
7. spill I/O is large/sequential where practical,
8. DataChunks/vector buffers are reused,
9. scans decode only required columns,
10. hash/directory storage is compact/contiguous where practical,
11. correctness is preserved under tiny budgets/forced spill,
12. instrumentation exists before aggressive optimization,
13. profiles—not intuition alone—justify SIMD/radix/JIT/prefetch complexity.

## 42.5 Statistics and optimizer calibration measurements

ANALYZE measurements include:

```text
rows/second
columns analyzed
peak maintenance memory
HLL/MCV/sample memory
statistics payload bytes
full-scan I/O
```

Cost calibration measures the primitive dimensions in §36.4 on the target deployment and records the resulting relative weight configuration.

Base-access calibration workloads vary:

```text
selectivity
heap pages / rows per page
index/heap correlation
cache budget
required output width
VARCHAR width
dead-version fraction
```

The required demonstration is that the same query shape may select SeqScan or IndexScan as these measured conditions change, without an explicit hard-coded selectivity cutoff.

Cardinality-estimation benchmark suites report error distributions, not only average error, and include skew/MCV/NULL/correlation-limitation workloads.

## 42.6 Optimizer search and cost-model measurements

Operator cost calibration compares predicted **relative ranking** against measured runtime/resource ranking for representative:

```text
SeqScan
IndexScan
HashJoin
NestedLoopJoin
IndexNestedLoopJoin
HashAggregate
Sort
TopN
spill paths
```

The architecture does not require predicted abstract cost to equal milliseconds.

Join-planning benchmarks include relation counts such as:

```text
2, 4, 6, 8, 10, 12, 16, 20, 30
```

and record:

```text
planning wall time
subsets explored
partitions considered
physical alternatives costed
memo entries retained/pruned
peak planning memory
heuristic/local-improvement work
```

They must demonstrate the transition from exhaustive bushy search to bounded heuristic behavior.

Interesting-order tests demonstrate cases where a locally more expensive ordered plan wins globally by avoiding Sort or enabling another capability-enabled ordered operator.

Memory-aware tests hold logical statistics constant while changing the query planning/execution memory budget and verify that estimated spill and plan choice can change where appropriate.

A synthetic star-schema workload is a recommended stress case for many joins, selective dimensions, a large fact relation, and aggregation.

Benchmark-specific query text/table names MUST NOT be hard-coded into optimizer decisions.

Optimizer improvements arise from general:

```text
statistics
semantic rewrites
properties
costing
search
```

rather than benchmark fingerprints.

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
| `CATALOG` FileKind code | 16-bit little-endian code `4` | §4.7 / §16.9 |
| `CATALOG_DATA` PageType code | 16-bit little-endian code `6` | §4.9 / §16.9 |
| `catalog.dat` bootstrap file | exactly 2 pages in v1 | §16.9 |
| CATALOG_DATA bootstrap page | 8192 bytes; 64-byte header + six 32-byte entries | §16.9.2 |
| CATALOG_DATA system-relation entry | 32 bytes | §16.9.2 |
| PersistedScalarV1 header | 16 bytes | §17.13 |
| PersistedScalarV1 total size | `Align8(16 + payload_length)` | §17.13 |
| DefaultValueBlob v1 header | 24 bytes | §21.12.1 |
| DefaultValueBlob v1 maximum | 4096 bytes | §21.12.1 |
| Statistics catalog fragment maximum | 4096 bytes | §16.5.6 / §34.14 |
| StatisticsPayloadV1 common header | 40 bytes | §34.14.1 |
| Statistics TABLE payload fixed prefix | 104 bytes before manifest arrays | §34.14.2 |
| Statistics COLUMN payload fixed prefix | 104 bytes before scalar/MCV/histogram data | §34.14.3 |
| Statistics INDEX payload | exactly 112 bytes | §34.14.4 |
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
| CHECKPOINT_BEGIN payload | 32 bytes | §13.6 |
| CHECKPOINT_DATA prefix | 24 bytes | §13.7 |
| Checkpoint DPT entry | 24 bytes | §13.7 |
| Checkpoint writer entry | 16 bytes | §13.7 |
| CHECKPOINT_END payload | 32 bytes | §13.8 |

Catalog, spill, and other persistent formats are added as their canonical chapters are migrated.

---

# Appendix B. Global Invariants

The following global invariants apply across subsystem boundaries and MUST NOT be weakened without an explicit architecture revision.

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
13. Acknowledged durable state never depends on a file or WAL-segment namespace entry whose required parent-directory synchronization has not succeeded.

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.21; FSM/reclamation invariants are listed in §6.13; I/O/buffer invariants are listed in §7.13; B+ tree invariants are listed in §8.29; transaction/snapshot invariants are listed in §9.16; MVCC invariants are listed in §10.6; logical-locking invariants are listed in §11.15; WAL/commit invariants are listed in §12.18; recovery invariants are listed in §13.21; vacuum/reclamation invariants are listed in §14.18; end-to-end write invariants are listed in §15.9. Catalog invariants are listed in §16.11; type/value invariants in §17.12 and persisted-scalar invariants in §17.13.5; lexer/parser/AST invariants in §18.16; binder/expression invariants in §19.20; logical-plan/rewrite invariants in §20.20; upper semantic-layer invariants in §21.20; physical-plan/runtime invariants in §22.8; vector/string invariants in §23.14; memory/spill invariants in §24.11; expression-execution invariants in §25.8; pipeline invariants in §26.10; scan/unary invariants in §27.12; join invariants in §28.13; aggregation invariants in §29.9; sorting invariants in §30.8; DML/result invariants in §31.13; parallel-runtime invariants in §32.13; optimizer invariants in §33.7; statistics invariants in §34.17; estimation invariants in §35.27; base-access/cost invariants in §36.19; join/property invariants in §37.18; memo/search invariants in §38.25.

---

# Appendix C. Deferred Features and Future Experiments

This appendix records functionality and experiments intentionally outside the required v1 baseline.

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
- prepared-statement parameters in the v1 parser/planner baseline,
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
- spill-file crash recovery or WAL logging,
- JIT / LLVM query compilation,
- GPU execution,
- adaptive query execution and runtime join reordering,
- vector compression beyond the baseline representations,
- radix hash join / pervasive radix partitioning,
- pervasive radix sorting,
- explicit SIMD intrinsics for every kernel,
- NUMA-aware execution scheduling/memory placement,
- lock-free/work-stealing scheduler before profiles justify it,
- parallel ordered B+ scan partitioning,
- distributed exchange operators and remote spilling,
- engine-wide late-materialization redesign,
- Bloom-filter pushdown from hash build until explicitly costed/proven,
- automatic ANALYZE / maintenance-trigger scheduling,
- page/block sampling as a replacement for the v1 full-scan ANALYZE path,
- persisted incremental HLL/other statistics sketches,
- multi-column/extended statistics such as dependencies, joint NDV, and multi-column MCVs,
- bitmap index intersection/union and general index-merge access paths,
- index-only scans until visibility/covering-index semantics are explicitly designed,
- adaptive/runtime reoptimization based on observed cardinalities,
- a full Cascades/Volcano transformation optimizer,
- arbitrary memo transformation rules over expression groups,
- runtime join switching,
- learned cardinality estimation,
- functional-dependency and other multi-column extended statistics,
- DPccp / connected-subgraph join enumeration as a replacement experiment,
- correlated-subquery decorrelation,
- Bloom-filter/semi-join reduction planning,
- materialized-view matching,
- join elimination beyond simple explicitly proven rules,
- partition pruning,
- generic/custom parameter-sensitive prepared plans,
- persistent cardinality feedback from query execution,
- detailed probabilistic parallel-memory/cost modeling,
- distributed/GPU optimizer cost models.

Items listed here are future possibilities or staged functionality, not requirements to implement immediately.

---

# Appendix D. Open Architecture Questions

## D.1 V1 architecture status

No unresolved v1 core-architecture question remains in the current v1 contract.

Every identified architecture gap affecting v1 semantics or persistent formats is either:

- resolved explicitly and integrated into its owning chapter, or
- classified as intentionally deferred functionality in Appendix C.

Deferred features are not unresolved v1 questions.

## D.2 Architecture revision rule

A future implementation discovery may justify changing an accepted architectural decision, but the change requires an explicit architecture revision that identifies:

```text
current contract
proposed replacement
reason
benefits and drawbacks
persistent/migration consequences
affected subsystems
verification obligations
```

Until such a revision is accepted, this document remains authoritative.
