# ARCHITECTURE DOCUMENT REVIEW — PASS D

## 1–8. Verdict, scope, and counts

**Verdict: PASS D — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Primary scope read completely:

- [ARCHITECTURE.md Chapter 4, §§4.1–4.7.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:700)
- Every heading, paragraph, table, formula, code block, state diagram, namespace rule, protocol, and explicit cross-reference in that range

Context-only architecture consulted:

- Front matter and Chapters 1–3
- §§4.8–4.15
- Relevant portions of Chapters 5–8, 12–14, 16, 21, 24, 39, and 41
- Appendices B–D

Other live documents consulted:

- [AGENTS.md](/home/yghtso/Projects/DBlusBlus/AGENTS.md)
- [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:236), only for the two recorded implementation mismatches
- [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:90), only for verification-document ownership
- `DEVELOPMENT.md` was not needed.

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 2 |
| MINOR | 3 |
| EDITORIAL | 0 |

The two MAJOR findings are frozen semantic questions concerning database-root lifecycle coordination and removal. No persistent byte layout is contradictory.

## 9. Section-by-section review

Legend: **C** = clear/consistent; **N/A** = not locally applicable.

| Section | Architectural role | Timelessness | Encoding clarity | Identity clarity | Validation clarity | Namespace/durability clarity | Normative clarity | Terminology | Cross-references | Analytical sufficiency | Semantic consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| §4.1 Scope | Defines Chapter-4 ownership | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.2 Page persistence | Page size and serialization baseline | C | C | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3 Identifier types | Widths and sentinels | C | C | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3.1 File identity | Separates FileId from OS descriptors | C | N/A | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3.2 Exhaustion | Canonical checked-next contract | Finding | C | C | C | C | C | Finding | C | C | C | FINDING |
| §4.3.2.1 ID allocation | Durable high-water allocation | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.3.2.2 CommandId | Terminal CommandId behavior | C | C | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3.2.3 Page/file bounds | PageNo and signed-offset limits | C | C | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3.2.4 WAL positions | WAL bounds and terminal credit | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.3.2.5 Counts/tokens | Encoded bounds and runtime generations | C | C | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.3.2.6 Failure/verification | Exhaustion failure scope | C | C | C | C | C | C | C | C | Finding | C | FINDING |
| §4.4 Page identity | Defines persistent PageId meaning | C | Delegated | C | C | N/A | C | C | Adequate | C | C | CLEAN |
| §4.5 RID meaning | Physical tuple-version identity | C | Delegated | C | C | N/A | C | C | Adequate | C | C | CLEAN |
| §4.6 Index-to-heap | Physical index/RID semantics | C | Delegated | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.6.1 UPDATE | New-version/new-RID consequence | C | N/A | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.6.2 Index scan | Heap recheck and visibility ownership | C | N/A | C | C | N/A | C | C | C | C | C | CLEAN |
| §4.6.3 Deferred update | HOT-like scope classification | C | N/A | C | N/A | N/A | C | C | Appendix C consistent | C | C | CLEAN WITH NOTE |
| §4.7 File model/kinds | File-kind registry and page-zero rule | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.1 Namespace root | Managed namespace and safe lookup | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.2 POSIX durability | Required filesystem semantics | C | N/A | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.3 Object states | Persistent object-file state machine | C | N/A | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.4 Object publication | Private creation and durable rename | C | C | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.5 DDL prerequisite | Namespace before terminal commit | C | N/A | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.6 Orphans | Ownership classification and cleanup | C | N/A | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.7 Unlink/retirement | Object-level durable removal | C | N/A | C | C | C | C | C | C | C | C | CLEAN |
| §4.7.8 Root creation | Staged database-root publication | C | C | Finding | C | Finding | Finding | Finding | C | Finding | Finding | FINDING |

## 10–18. Identifier and representation assessment

### Identifier domains

The identifier contract is unusually complete. All principal persistent/cross-layer identifiers have fixed unsigned widths, contextual invalid values, checked advancement, and explicit exhaustion behavior.

No identifier may silently wrap. The only reuse paths are explicitly gated:

- unpublished append-tail PageNo retry;
- object-specific page reuse;
- RID reuse after Chapter-14 grace;
- runtime token reseeding after complete quiescence.

### Widths and signedness

All Chapter-4 fundamental identifiers are explicitly unsigned. The physical file-offset domain is separately constrained to nonnegative signed 64-bit positional I/O.

No use of implementation-defined C++ integer width was found.

### Sentinels, zero, and all-ones

Sentinel use is contextual and consistent:

- `FileId{0}`, `TxnId{0}`, and `Lsn{0}` are invalid.
- `PageNo{0}` is valid and reserved for the superblock.
- `SlotId{0}` and `CommandId{0}` are valid.
- `UINT64_MAX` is invalid for PageNo but valid for control generation and SchemaVer’s corresponding all-ones width where specified.
- `UINT16_MAX` is invalid SlotId but legal maximum tree height.

No value is simultaneously a valid ordinary identity and an invalid sentinel in the same field context.

### Exhaustion and reuse

The checked-next protocol prevents wrap, narrowing, increment-then-test, and premature publication. Durable high-water gaps remain consumed.

The TxnId block arithmetic is exact:

```text
(MAX_RESERVED_TXN_ID_END - 2) mod 2^20 = 0
MAX_ALLOCATABLE_TXN_ID = MAX_RESERVED_TXN_ID_END - 1
```

The unused terminal uint64 suffix is correctly excluded rather than allocated as a smaller block.

### Composite identity and RID

`PageId = (FileId, PageNo)` is independent of frames, addresses, and file descriptors.

`RID = (heap FileId, PageNo, SlotId)` denotes a physical tuple version. Its later persisted B+ representation is exactly 16 bytes, little-endian, with bytes 14–15 required zero. The logical and persisted contracts agree.

### Reserved bytes

All relevant later consumers apply strict `RESERVED_ZERO` behavior. Unknown nonzero material is rejected rather than ignored or round-tripped.

## 19–27. Encoding, arithmetic, checksum, and version assessment

### Integer encoding and host separation

All Chapter-4 multibyte persistent integers default to explicit little-endian unless a specific format states otherwise. Native structures, padding, alignment, pointer width, and endianness are forbidden as persistence contracts.

No generic persistent float encoding is defined in the primary slice. Type-specific scalar and memcomparable encodings remain owned by later chapters.

### Alignment

The only material alignment rule in the primary slice is WAL eight-byte alignment. It is a serialized/logical stream requirement, not a C++ ABI alignment assumption.

### Size and offset arithmetic

All recomputed arithmetic is consistent:

- `INT64_MAX / 8192 = 1,125,899,906,842,623` complete pages.
- Last valid page is `1,125,899,906,842,622`.
- Maximum aligned file length is `9,223,372,036,854,767,616`, below `INT64_MAX`.
- WAL payload bound is `67,108,864 - 48 = 67,108,816`.
- Maximum PAGE_IMAGE advancement is `2 × 8,264 - 8 = 16,520`.
- Maximum terminal-record advancement is `2 × 48 - 8 = 88`.
- Terminal headroom is `2 × 16,520 + 88 = 33,128`.
- Heap slot geometry gives maximum slot ordinal `1017` from at most `1018` slot descriptors.
- FSM’s 48-byte header leaves exactly 8,144 one-byte entries.
- The later RID layout sums to 16 bytes.
- The common FileSuperblock prefix sums to 72 bytes; the B+ extension is 56 bytes, producing a 128-byte header.

No unexpected overlap or inconsistent fixed-size arithmetic was found.

### Checksums

The checksum contract is exact in the detailed owner sections:

- CRC32C, not generic CRC32;
- uint32 little-endian result;
- bytes 16–19 logically zero;
- coverage exactly bytes 0–8191 for pages/superblocks;
- checksum validation precedes trust in persisted `page_lsn`.

Hardware acceleration remains implementation freedom.

### Magic and versioning

Architecture v1 is kept distinct from file/page/control/catalog format versions and generations. Unknown future required formats are rejected as unsupported; malformed recognized v1 structures are corruption.

No implicit old-format migration or preserve-and-round-trip behavior exists.

## 28–38. Generic/specialized metadata and page/file identity

- The generic FileSuperblock is one 8,192-byte page with a 72-byte common prefix.
- HEAP, FSM, CATALOG, and TXN_STATUS use `header_size=72`.
- BTREE selects `header_size=128` and extends bytes 72–127.
- Dispatch is selected by `file_kind`; a generic decoder may not treat BTREE extension bytes as generic reserved bytes.
- All generic FileSuperblock flag bits are unassigned in v1: writer zero, reader reject nonzero.
- FileKind codes 1–5 are complete; zero and every other v1 code are rejected.
- FileId is database-local, durable, nonzero, nonreusable, and independent of path and descriptor.
- PageNo is file-local; page zero is the superblock; ordinary pages begin at one.
- Page offset calculation is guarded by the exact signed-64-bit extent bound.
- Later ordinary page owner validation cross-checks managed name, FileId, object ID, FileKind, PageNo, and published page bounds.

These contracts are clear despite current implementation mismatches recorded in PROJECT_STATE.

## 39–64. Namespace, publication, durability, and crash assessment

### Namespace model

The managed namespace distinguishes:

- fixed singleton names;
- deterministic final object names;
- transaction-private pending names;
- WAL segment names;
- unknown entries;
- whole-root creation staging.

SQL names are not mapped directly to filenames.

No-follow, directory-relative access, regular-file/directory type checks, retained directory descriptors, and same-filesystem rename requirements are precise.

### Control file and owner lock

`database.control` is both a required control file and the inode carrying the process-associated advisory lock. Control updates occur in place through alternating slots; no normal replacement protocol changes the locked inode.

Object-file publication does not affect that lock identity.

### Managed versus unknown files

Unknown entries are preserved. They cannot become committed objects or orphan-cleanup targets by filename guesswork.

Orphan classification requires exact managed grammar plus proof of no committed/bootstrap owner.

### Object publication

Private object creation and final-name publication are crash-classifiable:

1. initialize and synchronize file contents;
2. synchronize `pending/`;
3. rename without replacement;
4. synchronize both root and pending directories;
5. only then permit catalog commitment.

A multi-file object cannot commit before every required physical file crosses the barrier.

### Root creation

The durable root-publication point itself is clear: staging-root rename followed by successful external-parent `fsync`.

However, the architecture does not state what prevents another opener or remover from acquiring the final control-file lock after rename but before parent-directory synchronization. This is MAJOR finding D-M1.

### Retirement and unlink

Object retirement is coherent:

- semantic owner proves the object obsolete;
- BufferPool prevents new ownership and drains;
- exact managed name is unlinked;
- owning directory is synchronized;
- only then is absence durable.

Failed unlink or directory synchronization does not reverse committed DROP.

### Whole-database removal

Chapter 3 says offline whole-root removal is supported and delegates durability to §4.7, but §4.7 defines no whole-root removal protocol. This is MAJOR finding D-M2.

Consequently partial-removal and retry/open classification are undefined for that operation.

## 65–83. Remaining analytical assessments

- **Filesystem errors:** Complete pwrite/fsync/rename/unlink outcomes are distinguished from uncertain outcomes. Underlying error context is retained.
- **Short I/O:** Correctly delegated to §7.4.2; complete transfer or structured failure is required.
- **Sparse files/holes:** The architecture does not rely on physical block allocation. Ordinary page writes cannot create logical sparse-page holes; WAL preallocation may use a fixed-length zero stream.
- **File growth:** Serialized append, checked offset arithmetic, and WAL-backed publication are consistent.
- **PageNo tail reuse:** Permitted only after exact unpublished-tail restoration or owning reclamation.
- **FileId allocation:** Persisted and synchronized through control slots before return; no reuse.
- **Validation:** Framing, checksum, identity, range, owner, and cross-field validation are complete in §§4.10, 4.13, and 4.14.
- **Validation order:** Family/version dispatch precedes version-owned checksum/layout interpretation.
- **System files:** `database.control`, `catalog.dat`, `txn_status.dat`, six system relations’ HEAP/FSM pairs, and WAL segment zero are mutually consistent.
- **Catalog/status/WAL namespaces:** Consistent with Chapters 9, 12, 13, and 16.
- **Temporality:** One noncanonical “currently” occurrence is a MINOR finding. Other temporal words describe runtime order, format evolution, or durable deferred scope.
- **Version terminology:** Architecture version, file/page format version, catalog schema version, key schema version, control generation, and creation epoch remain distinguishable.
- **Implementation status:** No implementation-completion narrative appears.
- **Source layout:** No source-directory/class-file coupling appears. Persistent filenames are legitimate architecture.
- **Development ownership:** No coding sequence appears.
- **Verification ownership:** The detailed boundary-injection paragraph in §4.3.2.6 belongs in Chapter 41/VERIFICATION; MINOR finding D-m2.
- **Examples/test vectors:** Numeric examples are normative bounds or protocol examples and match the formulas. No ambiguous pinned byte vector appears in the primary slice.
- **Duplication:** Chapter 4 provides acceptable foundational contracts and cross-cutting invariants. No conflicting duplicate byte format was found.
- **Analytical depth:** Strong overall. The rationale for nonreuse, checksums, staged publication, and orphan proof is sufficient. Root creation/removal coordination remains the material gap.

## 84. Identifier table

| Type/domain | Width/sign | Valid domain | Sentinel/reserved | Persistent? | Reuse | Exhaustion | Canonical owner | Consistent? | Finding? |
|---|---|---|---|---|---|---|---|---|---|
| FileId | uint32 | 1..MAX−1 | 0 invalid; MAX exhausted marker | Yes | Never | FILE_ID_EXHAUSTED | §§4.3, 13.2.5 | Yes | No |
| PageNo | uint64 | 0..physical cap | MAX invalid | Yes | Unpublished tail or owner reclamation only | PAGE_NUMBER_EXHAUSTED | §§4.3.2.3, 4.11 | Yes | No |
| published_page_count | uint64 runtime | 1..1,125,899,906,842,623 | 0 invalid initialized bound | No | Exact unpublished rollback | Per-file boundary | §§4.3.2.3, 4.11 | Yes | No |
| SlotId | uint16 | 0..1017 under v1 heap geometry | MAX invalid | Yes | Grace-gated | Page NO_SPACE | §§4.3, 5, 14 | Yes | D-m1 wording only |
| TxnId | uint64 | 2..18,446,744,073,708,503,041 | 0 invalid; 1 frozen | Yes | Never | TXN_ID_EXHAUSTED | §§4.3.2.1, 9 | Yes | No |
| CommandId | uint32 | 0..MAX | No sentinel | Yes in tuple/catalog carriers | Never per transaction | Later statement rejected | §§4.3.2.2, 9.6 | Yes | No |
| Lsn | uint64 | Aligned record starts 8..2^64−48 | 0 invalid | Yes | Never | WAL_POSITION_EXHAUSTED | §§4.3.2.4, 12 | Yes | No |
| TableId | uint64 | 1..MAX−1; 1–6 built-ins | 0 invalid | Yes | Never | ID_EXHAUSTED | §§4.3.2.1, 13.2.6, 16 | Yes | No |
| ColumnId | uint32 | 1..MAX, schema-bounded | 0 invalid but no named sentinel | Yes | Not within schema history | Schema rejected | §§4.3, 16.3 | Yes | No |
| IndexId | uint64 | ≥7..MAX−1 | 0 invalid | Yes | Never | ID_EXHAUSTED | §§4.3.2.1, 13.2.6 | Yes | No |
| ConstraintId | uint64 | ≥7..MAX−1 | 0 invalid | Yes | Never | ID_EXHAUSTED | Same | Yes | No |
| SchemaVer | uint32 | 1..MAX; v1 emits 1 | 0 invalid | Yes | Never per table | DDL rejected | §§4.3, 5.13 | Yes | No |
| PageId | 32+64 conceptual | Valid FileId + PageNo | Contextual component sentinels | Cross-layer; codec owned later | Component rules | Component rules | §§4.4, 12.5 | Yes | No |
| RID | 32+64+16 logical; 16-byte codec | Valid heap FileId/PageNo/SlotId | Two persisted reserve bytes zero | Yes in indexes | Grace-gated | Component rules | §§4.5, 8.4.1, 14.6 | Yes | No |
| Control generation | uint64 | 1..MAX | 0 invalid | Yes | Never | Next update fails | §13.2 | Yes | No |
| WAL segment index | Mathematical uint | 0..2^38−1 | None in domain | Filename | Never | WAL_POSITION_EXHAUSTED | §§4.3.2.4, 12.2 | Yes | No |
| B+ height/level | uint16 | height 1..MAX; level 0..MAX−1 | Height 0 invalid | Yes | Height may contract | Root growth fails | §§4.3.2, 8 | Yes | No |
| creation_epoch | uint64 opaque | Every bit pattern | None | Yes | Not an allocator | None | §§4.3.2, 4.10 | Yes | No |
| Runtime generation tokens | Width implementation-defined | Nonrepeating while stale observer exists | No reused value | No | Only after complete quiescence/reseed | Mutation rejected | §4.3.2.5 | Yes | No |
| Read epoch | uint64 runtime | 1..MAX | 0 invalid | No | Restart/quiescent reinit | Reuse disabled at MAX | §§4.3.2.5, 14.6 | Yes | No |

## 85–88. Format, arithmetic, reserved-field, and version tables

### Format table

| Format | Size | Magic/version | Identity | Flags/reserved | Checksum | Specialized? | Consumers | Arithmetic | Status |
|---|---:|---|---|---|---|---|---|---|---|
| Fundamental identifier scalars | 2/4/8 bytes | Enclosing format | Field-specific | Contextual sentinel | Enclosing format | No | All persistent chapters | Verified | CLEAN |
| FileKind | 2 bytes | Superblock v1 | Physical family | 0 invalid; others outside 1–5 corrupt | Superblock CRC | No | Page files | Verified | CLEAN |
| RID codec | 16 bytes | Enclosing B+ v1 | FileId/PageNo/SlotId | Bytes 14–15 zero | Page checksum | Later owner | B+ and DML | 4+8+2+2=16 | CLEAN |
| WAL PageId codec | 16 bytes | WAL v1 grammar | FileId/PageNo | Bytes 4–7 zero | WAL record CRC | Later owner | WAL/recovery | 4+4+8=16 | CLEAN |
| Common page header | 32 bytes | format_version 1 | PageNo | Known-mask/zero | CRC field at 16 | Chapter-4 later context | All pages | Verified | CLEAN |
| FileSuperblock | 8,192 bytes; prefix 72 | `DBLUSBLS`, version 1 | FileId/object_id | Strict zero | Whole-page CRC32C | Kind-dispatched | HEAP/FSM/CATALOG/STATUS | Verified | CLEAN |
| BTREE superblock | 8,192 bytes; header 128 | Common magic/version + key schema 1 | FileId/IndexId/TableId/root | Strict zero | Whole-page CRC32C | Yes | B+ tree | 72+56=128 | CLEAN |
| Managed namespace names | Variable ASCII grammar | Architecture v1 | Object/transaction/File IDs | Canonical decimal/hex | N/A | Kind suffix | Lifecycle/recovery | Grammar consistent | CLEAN |
| Root staging name | `D.dblusblus-creating` | Architecture v1 | Final basename D | Exact suffix | N/A | Root lifecycle | Create/maintenance | N/A | Semantic gate finding |

### Byte arithmetic table

| Format/calculation | Stated | Derived | Offsets valid? | Gaps intentional? | Overlap? | Checksum range | Result |
|---|---:|---:|---|---|---|---|---|
| Page size | 8,192 | 8,192 | Yes | N/A | No | 0..8191 | CLEAN |
| File page count cap | 1,125,899,906,842,623 | `floor(INT64_MAX/8192)` | Yes | Final 8,191-byte offset-domain remainder | No | N/A | CLEAN |
| Last PageNo | 1,125,899,906,842,622 | count−1 | Yes | N/A | No | N/A | CLEAN |
| TxnId terminal block | stated end | Exact multiple from 2 by 2^20 | Yes | Terminal suffix intentionally unused | No | N/A | CLEAN |
| WAL payload max | 67,108,816 | segment−48 | Yes | Alignment external | No | Record-owned | CLEAN |
| PAGE image advance | 16,520 | 2×8,264−8 | Yes | Tail padding | No | N/A | CLEAN |
| Terminal advance | 88 | 2×48−8 | Yes | Tail padding | No | N/A | CLEAN |
| Terminal credit | 33,128 | 2×16,520+88 | Yes | N/A | No | N/A | CLEAN |
| Heap slot directory | 1,018 slots | `(8192−48)/8` | Yes | Tuple region may be empty | No | Page-owned | CLEAN |
| FSM entries | 8,144 | 8192−48 | Yes | None | No | Page-owned | CLEAN |
| B+ slot directory bound | 1,016 | `(8192−64)/8` | Yes | Complete entry geometry further constrains it | No | Page-owned | CLEAN |
| RID | 16 | 4+8+2+2 | Yes | Final two bytes reserved | No | Enclosing page | CLEAN |
| FileSuperblock prefix | 72 | Sum of fields | Yes | None | No | Full 8192 | CLEAN |
| BTREE header | 128 | 72+56 | Yes | 128..8191 reserved | No | Full 8192 | CLEAN |

### Reserved-field table

| Format/field | Reserved value | Writer | Reader | Future behavior | Consistent? | Finding? |
|---|---|---|---|---|---|---|
| RID bytes 14–15 | Zero | Must zero | Reject nonzero | Requires format revision | Yes | No |
| WAL PageId bytes 4–7 | Zero | Must zero | Reject nonzero | New grammar/version | Yes | No |
| Common page flags/reserved | Zero unless assigned | Known-mask only | Reject unknown/nonzero | Explicit compatible format | Yes | No |
| FileSuperblock flags | Zero | Must zero | Reject every nonzero value | File-kind format revision | Yes | No |
| FileSuperblock named reserves | Zero | Must zero | Reject nonzero | Revision | Yes | No |
| Generic trailing region | Zero | Must zero | Reject nonzero | Revision | Yes | No |
| BTREE index flags | Zero | Must zero | Reject nonzero | Revision | Yes | No |
| BTREE trailing bytes | Zero | Must zero | Reject nonzero | Revision | Yes | No |

### Version/magic table

| Format | Magic | Version | Current | Unknown future | Corrupt versus unsupported | Status |
|---|---|---|---:|---|---|---|
| FileSuperblock | `DBLUSBLS` | uint16 `format_version` | 1 | `>1` unsupported | 0/malformed v1 corrupt | CLEAN |
| Common page | PageType discriminator | uint16 | 1 | `>1` unsupported | 0/malformed v1 corrupt | CLEAN |
| BTREE key schema | Common magic + fingerprint | uint32 | 1 | `>1` unsupported index | 0/fingerprint mismatch corrupt | CLEAN |
| Control slot | `DBLUSCTL` | uint16 | 1 | Blocks v1 fallback/open | Malformed v1 slot follows fallback/corruption | CLEAN |
| Catalog bootstrap | `DBLUSCAT` | bootstrap/catalog versions | 1 | Unsupported catalog schema | Malformed v1 corrupt | CLEAN |
| WAL | No stream magic/version | Exact v1 record grammar | v1 grammar | Unknown framed record type unsupported | Framing/CRC violation corrupt or torn-tail | CLEAN |

## 89–92. Namespace and crash tables

### Namespace objects

| Name/pattern | Type | Required? | Owner | State class | Creation owner | Cleanup/removal owner | Durability | Status |
|---|---|---|---|---|---|---|---|---|
| `database_root/` | Directory | Required | Database lifecycle | Live root | Root creator | Offline remover, underspecified | External parent fsync | FINDING |
| `database.control` | File | Required | Control/lifecycle | Live singleton/lock inode | Bootstrap | Whole-root removal underspecified | fdatasync + root namespace | CLEAN locally |
| `catalog.dat` | File | Required | Catalog bootstrap | Live singleton | Bootstrap | Whole-root removal underspecified | fdatasync/root fsync | CLEAN |
| `txn_status.dat` | File | Required | Transaction status | Live singleton | Bootstrap | Whole-root removal underspecified | fdatasync/root fsync | CLEAN |
| `pending/` | Directory | Required | Namespace owner | Staging/private | Bootstrap | Namespace cleanup | Directory fsync | CLEAN |
| `wal/` | Directory | Required | WAL | Live log namespace | Bootstrap | WAL retention/root remover | Directory fsync | CLEAN |
| `table_<id>.heap` | File pattern | Per table | Catalog/table | Live/final/retired/orphan | DDL | Retirement | Root fsync | CLEAN |
| `table_<id>.fsm` | File pattern | Per table | Catalog/FSM | Live/final/retired/orphan | DDL | Retirement | Root fsync | CLEAN |
| `index_<id>.btree` | File pattern | Per index | Catalog/index | Live/final/retired/orphan | DDL | Retirement | Root fsync | CLEAN |
| `pending/txn_<txn>_file_<file>.*` | File pattern | Transient | DDL transaction | Private/orphan | DDL | Orphan cleanup | File fdatasync + pending fsync | CLEAN |
| `<16hex>.wal` | File pattern | As needed | WAL | Live/recyclable | WAL owner | WAL retention | File fdatasync + wal fsync | CLEAN |
| `D.dblusblus-creating` | Directory | Transient | Root creator | Root staging/orphan | Root creator | Explicit create/maintenance | External parent fsync | D-M1 coordination gap |
| Unknown names | Any | Optional external material | External | Unknown | External | Never guessed/deleted | Preserved | CLEAN |

### Creation/publication

| Object | Staging | Preparation | Publication | Publication point | Required fsyncs | Crash before | Crash after | Retry/cleanup | Status |
|---|---|---|---|---|---|---|---|---|---|
| Private object file | Exact pending name | Initialize, pwrite, fdatasync | Pending entry | pending fsync | File + pending | No committed object; survivor orphan | PRIVATE_DURABLE | Orphan cleanup | CLEAN |
| Final object file set | Pending names | Flush/fdatasync all | No-replace rename | Root and pending fsyncs for complete set | Each file + both dirs | Pending/final survivors orphan | FINAL_DURABLE_UNCOMMITTED | Catalog commit or orphan cleanup | CLEAN |
| Committed CREATE | Final names | Complete physical barrier | Durable TXN_COMMIT/catalog publication | Commit durability after namespace barrier | Namespace + WAL | Aborted/unowned | Committed ownership | Recovery | CLEAN |
| Database root | `D.dblusblus-creating` | Complete bootstrap and validate | No-replace directory rename | External-parent fsync | Files, internal dirs, staging root, parent | Staging orphan/no final root | Durable valid root | Explicit cleanup/open | D-M1 |
| Whole database removal | None defined | Not defined | Not defined | Not defined | Not defined | Not classifiable | Not classifiable | Not defined | D-M2 |

### Namespace durability

| Operation | Content durability | Mutation | Directory durability | Success point | Failure survivor | Status |
|---|---|---|---|---|---|---|
| Private create | File fdatasync | Create pending entry | pending fsync | After both | Orphan/absent | CLEAN |
| Final publication | File fdatasync | Cross-dir no-replace rename | Root + pending fsync | After whole set | Pending/final/both orphan | CLEAN |
| Object unlink | No content flush after retirement proof | unlinkat | Owning directory fsync | ABSENT_DURABLE | Present/absent orphan, retry | CLEAN |
| WAL segment create | Segment fdatasync | Create final segment | wal fsync | Namespace durable before WAL acknowledgement | Empty/partial unacknowledged segment | CLEAN |
| Root create | Startup files and internal dirs | Rename staging root to D | External parent fsync | Creation success | Staging/final depending crash | Publication gate finding |
| Whole-root removal | Not specified | Not specified | Not specified | Not specified | Not specified | FINDING |

### Representative crash matrix

| Protocol | Crash boundary | Durable survivors | Next-open interpretation | Cleanup/retry | Ambiguous? | Finding? |
|---|---|---|---|---|---|---|
| Private file | Before pending fsync | Entry absent or incomplete | Orphan if present | Unlink + sync | No | No |
| Object rename | Before both directory syncs | Pending/final/both | No commit may require it | Orphan cleanup | No | No |
| DDL commit | After final publication, before commit | Durable final orphan | Unowned after recovery | Cleanup | No | No |
| DDL commit | After durable commit | Final files required | Committed object | Recovery republishes runtime state | No | No |
| Object unlink | Before directory fsync | Name may return or remain absent | Retired/unowned either way | Retry if present | No | No |
| Root creation | Before rename | Staging only or absent | Never opened as D | Proved-safe staging cleanup | No after crash | No |
| Root creation | After rename, before parent fsync | Final or staging may survive | Surviving final requires full validation | Cleanup/open | Crash state clear; concurrent admission gate unclear | D-M1 |
| Root creation | After parent fsync | Durable valid root | Normal open | None | No | No |
| Root removal | Any partial deletion | Undefined | Undefined | Undefined | Yes | D-M2 |

## 93–97. Cross-references, terminology, norms, temporality, consistency

### Explicit cross-reference audit

Repeated references to the same owner are grouped.

| Source | Target | Purpose | Exists? | Correct owner? | Precise? | Status |
|---|---|---|---|---|---|---|
| §4.3.2 | §12.12 | Append publication/tail rollback | Yes | Yes | Yes | CLEAN |
| §4.3.2 | §14.6 / §14.6.4 | RID grace and restart | Yes | Yes | Yes | CLEAN |
| §4.3.2.1 | §4.7 | Orphan namespace artifacts | Yes | Yes | Yes | CLEAN |
| §4.3.2.1 | §16.5 | Fixed built-in TableIds | Yes | Yes | Yes | CLEAN |
| §4.3.2.2 | §39.1 | Retry/error boundary | Yes | Yes | Yes | CLEAN |
| §4.3.2.4 | §12.13 | Durable-LSN meaning | Yes | Yes | Yes | CLEAN |
| §4.3.2.4 | §§12.10.5, 12.12, 13.13.2 | Terminal status image/retry/recovery | Yes | Yes | Yes | CLEAN |
| §4.3.2.4 | §39.1 / §39.1.5 | Terminal closure/client outcome | Yes | Yes | Yes | CLEAN |
| §4.3.2.4/.6 | Chapter 3 | Open/close lifecycle consequences | Yes | Yes | Adequate | CLEAN |
| §4.7 | §4.14.4 | FileKind compatibility | Yes | Yes | Yes | CLEAN |
| §4.7.1 | §12.2 | WAL names | Yes | Yes | Yes | CLEAN |
| §4.7.1 | Chapter 24 | Temporary spill namespace | Yes | Yes | Adequate | CLEAN |
| §4.7.1 | §3.3.2 | Control lock | Yes | Yes | Yes | CLEAN |
| §4.7.5 | §39.1.3–§39.1.5 | Namespace failure transaction effects | Yes | Yes | Yes | CLEAN |
| §4.7.6 | §4.7.5 | No private committed references | Yes | Yes | Yes | CLEAN |
| §4.7.6/.7 | §4.7.7 | Durable cleanup | Yes | Yes | Yes | CLEAN |
| §4.7.7 | Chapter 21 | Snapshot/descriptor retirement gates | Yes | Yes | Adequate | CLEAN |
| §4.7.7 | §7.12.5 | BufferPool file drain | Yes | Yes | Yes | CLEAN |
| §4.7.8 | §12.2.1 | Initial WAL segment | Yes | Yes | Yes | CLEAN |
| §4.7.8 | §3.3.7 | Create-to-open lifecycle | Yes | Yes | Yes, but target exposes D-M1 | FINDING |

### Terminology

| Term | Chapter-4 meaning | Canonical owner | Consistent? | Notes |
|---|---|---|---|---|
| FileId | Database-local persistent file identity | §§4.3, 13.2.5 | Yes | Not path or descriptor |
| PageNo | File-local page ordinal | §§4.3, 4.11 | Yes | Page zero is superblock |
| PageId | FileId + PageNo | §4.4 | Yes | Runtime frames excluded |
| RID | Physical heap version | §§4.5, 8.4.1 | Yes | Not logical row identity |
| Sentinel/invalid | Field-contextual known value | §§4.3, 4.14.4 | Yes | Not extension space |
| FileSuperblock | Page-zero file identity/format | §4.10 | Yes | BTREE extends common prefix |
| database_root | Caller-selected persistent namespace | §§4.7.1, 3.3 | Yes | Root removal incomplete |
| pending | Private DDL namespace | §4.7 | Yes | Never committed name |
| orphan | Exact managed entry proven unowned | §§4.7.6, 13.19 | Yes | Unknown names excluded |
| retired | Semantically unreachable but possibly still linked | §§4.7.3, 4.7.7, 21.9 | Yes | Distinct from absent |
| publication | Architecture-defined durable visibility barrier | §§4.7.4–.5 | Yes | Root admission gap |
| durable | Required file and directory synchronization completed | §§4.7.2–.8 | Yes | Not synonymous with syscall return alone |

### High-value normative rules

| Section | Requirement | Strength | Detailed consistency | Ambiguous? | Finding? |
|---|---|---|---|---|---|
| §4.2 | No native C++ representation persistence | MUST NOT | Consistent | No | No |
| §4.3.2 | Checked-next before publication | Canonical/MUST-level | Consistent | No | No |
| §4.3.2.1 | Durable high-water before ID return | MUST | Consistent | No | No |
| §4.3.2.4 | Preserve terminal WAL headroom | MUST | Consistent | No | No |
| §4.4 | Persistent identity excludes frame/address/fd | MUST NOT | Consistent | No | No |
| §4.7 | Unknown v1 FileKind rejected | MUST | Consistent | No | No |
| §4.7.2 | Propagate sync/open errors | MUST | Consistent | No | No |
| §4.7.4 | No-replace final publication and both directory syncs | MUST | Consistent | No | No |
| §4.7.5 | No terminal commit before namespace prerequisites | MUST NOT | Consistent | No | No |
| §4.7.6 | Delete only exact proven orphans | MUST-level rule | Consistent | No | No |
| §4.7.7 | Drain before unlink; directory sync before absent | MUST-level protocol | Consistent | No | No |
| §4.7.8 | Parent fsync before create success | Protocol/MUST-level | Consistent locally | Admission interval unclear | D-M1 |
| §3.3.7 + §4.7 | Offline root removal | Supported requirement | Missing detailed protocol | Yes | D-M2 |

### Temporality

| Section | Phrase | Classification | Finding? |
|---|---|---|---|
| §4.3.2 | “decode the current value” | Runtime algorithm | No |
| §4.3.2 | “heap geometry currently limits” | Noncanonical live-document scope wording | Yes, D-m1 |
| §4.3.2.1 | “later allocations” | Numeric sequence after built-ins | No |
| §4.3.2.1 | “any future increment” | Architecture evolution | No |
| §4.3.2.2 | “later ordinary statement” | Runtime transaction ordering | No |
| §4.3.2.4 | “current-segment tail” | Runtime WAL position | No |
| §4.3.2.4 | “later ordinary WAL-backed work” | Runtime ordering | No |
| §4.3.2.6 | “future-proof read-only open mode” | Durable v1 exclusion | No |
| §4.6.3 | “HOT-like future optimization” | Explicit deferred scope | No |
| §4.7 | “Future file kinds” | Architecture evolution | No |
| §4.7.8 | “initial WAL segment” | Bootstrap initialization | No |

### Cross-chapter consistency

| Chapter-4 contract | Later owner(s) | Result | Notes |
|---|---|---|---|
| RID meaning/encoding | Chapters 5, 8, 14, 15 | CONSISTENT | 16-byte persisted codec and grace reuse agree |
| FileId/PageNo/PageId | Chapters 7, 12, 13 | CONSISTENT | No descriptor/frame identity leakage |
| Generic superblock | Chapters 5, 6, 9, 16 | CONSISTENT | HEAP/FSM/STATUS/CATALOG use 72-byte prefix |
| BTREE metadata | Chapter 8 | CONSISTENT BUT SPECIALIZED | 128-byte header extends common prefix |
| Namespace/open/recovery | Chapters 3, 13 | FINDING | Root-create admission gate and whole-root removal |
| Control file | Chapters 3, 13 | CONSISTENT | In-place slots preserve lock inode |
| Catalog/status files | Chapters 9 and 16 | CONSISTENT | Fixed names/kinds/identity rules agree |
| WAL namespace | Chapters 12–13 | CONSISTENT | Exact names and file/directory durability agree |
| Object retirement | Chapters 7, 14, 21 | CONSISTENT | Semantic retirement precedes physical unlink |

## 98. High-priority consistency results

| # | Item | Result | Note |
|---:|---|---|---|
| 1 | Fixed-width identifiers | CONSISTENT | Explicit widths |
| 2 | Signedness | CONSISTENT | Unsigned logical domains |
| 3 | Sentinels | CONSISTENT | Contextual |
| 4 | Zero validity | CONSISTENT | Field-specific |
| 5 | All-ones validity | CONSISTENT | Field-specific |
| 6 | Exhaustion | CONSISTENT | Checked-next |
| 7 | Reuse | CONSISTENT | Explicit gates |
| 8 | Composite identity | CONSISTENT | PageId/RID |
| 9 | RID encoding | CONSISTENT BUT SPECIALIZED | Owned §8.4.1 |
| 10 | RID reserved bytes | CONSISTENT BUT SPECIALIZED | Strict zero |
| 11 | Integer byte order | CONSISTENT | Little-endian default |
| 12 | Native representation separation | CONSISTENT | Explicitly forbidden |
| 13 | Layout sizes | CONSISTENT | Arithmetic verified |
| 14 | Layout offsets | CONSISTENT | No overlap |
| 15 | Checksum algorithm | CONSISTENT | CRC32C |
| 16 | Checksum coverage | CONSISTENT | Whole page, checksum zeroed |
| 17 | Magic/version | CONSISTENT | Family-specific |
| 18 | Reserved flags | CONSISTENT | Known-mask zero |
| 19 | Generic FileSuperblock | CONSISTENT | 72-byte prefix |
| 20 | Specialized dispatch | CONSISTENT BUT SPECIALIZED | BTREE 128-byte header |
| 21 | FileId | CONSISTENT | Durable/nonreused |
| 22 | PageNo | CONSISTENT | File-local |
| 23 | PageId | CONSISTENT | Persistent logical |
| 24 | Page size | CONSISTENT | 8,192 |
| 25 | Offset arithmetic | CONSISTENT | Signed-64 bound |
| 26 | File length | CONSISTENT | Aligned whole pages |
| 27 | Namespace names | CONSISTENT | Exact managed grammar |
| 28 | No-follow/path safety | CONSISTENT | Descriptor-relative |
| 29 | Root identity | FINDING | Creation admission interval |
| 30 | Control-file role | CONSISTENT | Metadata plus lock inode |
| 31 | Owner-lock compatibility | FINDING | Root creator gate unspecified |
| 32 | Managed versus unknown | CONSISTENT | Unknown preserved |
| 33 | Orphan classification | CONSISTENT | Proof required |
| 34 | Pending namespace | CONSISTENT | Private only |
| 35 | Atomic publication | FINDING | Object publication clean; root admission gap |
| 36 | Rename semantics | CONSISTENT | Atomic runtime/no-replace |
| 37 | File fsync | CONSISTENT | fdatasync |
| 38 | Directory fsync | CONSISTENT for defined protocols | Root removal undefined |
| 39 | Creation protocol | FINDING | D-M1 |
| 40 | Create crash/failure | CONSISTENT after crash | Concurrent pre-fsync use unresolved |
| 41 | Managed-file publication | CONSISTENT | Complete barrier |
| 42 | File retirement | CONSISTENT | Lifetime-gated |
| 43 | Unlink durability | CONSISTENT | Parent fsync |
| 44 | Database removal | FINDING | No ordered protocol |
| 45 | Partial removal | FINDING | Survivor semantics undefined |
| 46 | Crash-prefix classification | FINDING | Whole removal only |
| 47 | System-file names | CONSISTENT | One wording issue D-m3 |
| 48 | WAL namespace | CONSISTENT | Chapter 12 agrees |
| 49 | Status/catalog namespace | CONSISTENT | Fixed singleton identities |
| 50 | Corruption versus unsupported | CONSISTENT | Exact classification |

## 99–102. Complete findings

### BLOCKING findings

None.

### MAJOR findings

#### D-M1 — Root publication lacks a pre-durability admission/ownership gate

- **Section:** §4.7.8, cross-section with §§3.3.2 and 3.3.7
- **Evidence:** [steps 7–9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1516) rename the staging root, then synchronize the external parent, then report success. The protocol does not require the creator to hold the `database.control` lock through that interval. [§3.3.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:624) only says a creator returning a handle *may* retain a lock established across rename.
- **Severity:** MAJOR
- **Type:** PUBLICATION
- **Scope:** Cross-section
- **Explanation:** After rename, the final root is visible to `OpenDatabase` even though the parent directory has not yet made that name crash-durable. Nothing explicitly prevents another process from acquiring the control-file lock and reaching READY before step 8. Such an owner could acknowledge durable work while the root entry itself remains vulnerable to disappearance on crash, conflicting with Appendix-B invariant 13.
- **Arithmetic:** N/A
- **Canonical comparison:** §§3.3.2–3.3.4, §4.7.2, §4.7.8, Appendix B invariant 13.
- **Consequence:** A competing opener or remover can potentially use the root in the rename-to-parent-fsync interval. The architecture does not specify the coordination mechanism needed to preserve root-name durability and one-owner semantics.
- **Future action:** **K. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED**

#### D-M2 — Whole-database removal is claimed but has no namespace protocol

- **Section:** §4.7 as a whole, cross-section with §3.3.7
- **Evidence:** [§3.3.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:632) states that offline root removal is supported and “follows §4.7 durability.” Sections 4.7.1–4.7.8 define object unlink and root creation, but no whole-root removal protocol.
- **Severity:** MAJOR
- **Type:** SEMANTIC COMPLETENESS
- **Scope:** Cross-section
- **Explanation:** The contract does not define managed/unknown-entry treatment, deletion order, when `database.control` may be unlinked while its descriptor carries exclusivity, subdirectory/root removal, external-parent synchronization, success publication, or partial-failure survivor classification.
- **Arithmetic:** N/A
- **Canonical comparison:** §3.3.7 and §§4.7.2, 4.7.7.
- **Consequence:** Implementations must invent crash-relevant policy. A failed removal could leave a root that is neither safely openable nor safely retryable, or could release/remove the lock identity before namespace work is complete.
- **Future action:** **K. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED**

### MINOR findings

#### D-m1 — Noncanonical “currently” in the v1 SlotId bound

- **Section:** §4.3.2
- **Evidence:** [SlotId row](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:815): “heap geometry currently limits an allocated slot to `1017`.”
- **Severity:** MINOR
- **Type:** TEMPORALITY
- **Scope:** Local
- **Explanation:** The limit is a durable v1 format fact, not implementation status.
- **Canonical comparison:** §§5.3–5.6 and §4.13.3.
- **Consequence:** No semantic ambiguity, but wording invites a project-state interpretation.
- **Future action:** **A. LOCAL WORDING FIX**

#### D-m2 — Detailed test-injection procedure is outside the local format owner

- **Section:** §4.3.2.6
- **Evidence:** [lines 1113–1117](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1113) require injection immediately below, at, and above each boundary and enumerate test cases.
- **Severity:** MINOR
- **Type:** OTHER
- **Scope:** Cross-document ownership
- **Explanation:** The exhaustion behavior belongs in §4.3.2. Detailed injection strategy belongs in Chapter 41 and `VERIFICATION.md` under the repository’s documentation-role rules.
- **Canonical comparison:** §41.1–§41.3 and `VERIFICATION.md`.
- **Consequence:** No architecture error; verification methodology is duplicated outside its canonical procedural owner.
- **Future action:** **J. MOVE/OWNERSHIP FIX**

#### D-m3 — Bootstrap file-count wording is grammatically ambiguous

- **Section:** §4.7.8
- **Evidence:** [line 1509](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1508): “all six system-relation heap/FSM files”.
- **Severity:** MINOR
- **Type:** TERMINOLOGY
- **Scope:** Cross-section
- **Explanation:** §16.9.2 defines six system relations, each with a distinct heap FileId and FSM FileId—twelve object files. The phrase can be read as six total files instead of heap and FSM files for all six relations.
- **Canonical comparison:** §16.9.2 bootstrap-entry layout and §16.5.
- **Consequence:** A reader relying only on the bootstrap list could miscount required startup files, although detailed bootstrap validation resolves the meaning.
- **Future action:** **A. LOCAL WORDING FIX**

### EDITORIAL findings

None.

## 103. Frozen architecture semantic questions

1. **What architecture-owned gate prevents open/removal or acknowledged database use after the staging-root rename but before external-parent `fsync` makes the final root durable?**

   The contract must select an existing-meaning coordination rule—such as creator-held control-file ownership or an equivalent publication/admission gate—without this audit choosing it.

2. **What is the canonical whole-database removal protocol?**

   It must define owner-lock lifetime, unknown-entry policy, deletion order, directory synchronization, final success point, and crash/partial-failure retry classification.

## 104. Out-of-scope observations

- **§7.5:** Known implementation-stage wording remains: “once the buffer layer exists.” Reserved for the Chapter-7 review.
- **§16.9.5:** Similar shorthand says “initializes the six system relation files,” despite each bootstrap entry naming both heap and FSM files. Review during the Chapter-16 pass.
- No other later-section issue was developed into a separate Pass-D finding.

## 105–116. Direct consistency answers

| Question | Answer |
|---|---|
| Any fixed-size layout arithmetic inconsistent? | No. |
| Any unexpected field overlap? | No. |
| Any reserved value with contradictory semantics? | No. |
| Any identifier can silently wrap/reuse contrary to contract? | No. |
| Any checksum contract ambiguous? | No. |
| Generic/specialized superblock dispatch ambiguous? | No. |
| Any namespace publication point crash-ambiguous? | The durable root point is clear, but admission/use during the pre-fsync visibility interval is unresolved (D-M1). |
| Any required directory fsync absent? | Not in defined object/create/unlink protocols; the entire whole-root removal fsync sequence is absent (D-M2). |
| Any crash prefix cannot be classified on next open? | Defined object/root-create prefixes are classifiable; partial whole-root removal is not. |
| Can unknown namespace entries be deleted without proof? | No under defined cleanup rules. Whole-root removal policy is unspecified. |
| Can control-file replacement break owner-lock identity? | Normal control updates do not replace it. Whole-root removal ordering could mishandle it because that protocol is undefined. |
| Any persistent contract forces correctness-relevant invention? | Yes: D-M1 and D-M2. |

## 117. PROJECT_STATE mismatch cross-check

- **Generic superblock flags architecture contract:** Clear. V1 assigns no bits; writers write zero and readers reject all nonzero flags.
- **Owner-validation architecture foundation:** Clear. §§4.13.1–4.13.8 require identity, range, reserved-state, owner-descriptor, and complete page-local validation before ordinary publication.
- No implementation defect was edited or treated as an architecture finding.

## 118–121. Regression, next action, and Pass E

Previous-pass regression check:

- **Pass A:** Canonical `v1` terminology, parallel-ready goal, and deferred-scope wording remain intact.
- **Pass B:** §2.1 flow/metadata clarification and §2.5 provider-to-consumer/timeless BufferPool wording remain intact.
- **Pass C:** §3.1 v1 platform wording, §3.2 architecture-revision authority, DRAINING→CLOSING boundary, CLOSING→NONCONTINUABLE edge, and clean-versus-uncertain failed-open distinction remain intact.

Known §7.5 observation: **unchanged and out of scope**.

Recommended next action: **frozen semantic decision required** for D-M1 and D-M2 before proceeding as though the root-lifecycle contract is complete. The three MINOR findings may be handled as targeted documentation fixes after or alongside that decision.

Recommended Pass-E scope, based on the actual boundary:

- Chapter 4 §§4.8–4.15 in full, covering the common page header, page-type registry, FileSuperblock, append publication, checksums, structural validation, compatibility policy, and storage-foundation invariants.
- Chapter 5 should begin only after that slice.

## 122–127. Repository state

Files modified by audit: **NONE**

Initial state:

```text
git status --short:
?? "docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 4/"

cached index: empty
HEAD: e4cd45a83b6394c1a5e6711d2300bfa980b65251
```

Final state: identical.

`git diff --check`: **PASS** with no output.

Repository state changed during audit: **No**.

Any change caused by this audit: **No**.

The pre-existing untracked Chapter-4 review directory remained unmodified and unstaged.