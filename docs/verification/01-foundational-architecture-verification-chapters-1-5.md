# Foundational Architecture Verification — Chapters 1–5

This section is the direct procedural owner for the foundational capability, dependency,
platform, generic page-format, heap-page, and ordinary-tuple obligations in Chapters 1–5 of
[`ARCHITECTURE.md`](../ARCHITECTURE.md). It composes with the complete exhaustion, BufferPool,
FSM, B+ tree, transaction, WAL, lifecycle, MVCC, reclamation, catalog, type, and DML
procedures below. A reference to one of those procedures is valid only when its independent
oracle proves the cited obligation; architecture prose or production validation code is not
an oracle.

All static and runtime checks in this section are deterministic. Concurrency uses barriers
or coordinated processes, persistent bytes use independent field codecs, and graph checks
use test-side sets and bounds. No procedure depends on sleeps, path enumeration order,
pointer values, allocator placement, native object representation, or source-language enum
ordinal values.

## Capability, dependency, language, and platform manifest

Maintain one machine-readable semantic manifest whose entries are architectural roles and
capabilities rather than source paths or class names. Each entry records:

```text
name
Architecture owner
supported / excluded / required implementation-owned mechanism
public registration or entry surface, if any
allowed external dependency role
forbidden delegated role
verification evidence kind
```

The harness independently derives its expected rows from Chapters 1–3 and compares them
with four implementation-produced inventories:

1. declared build/link dependencies and their transitive dependency graph;
2. public subsystem/service registrations and exported API capabilities;
3. the closed SQL statement/feature registry and other externally selectable feature
   registries;
4. the language/platform build configuration manifest.

Absence is not established by one textual search. A feature is absent only when it has no
public registration, no selectable grammar/API capability, no build target or linked service
that supplies it, and no dependency edge that delegates it. Generated dependency and symbol
inventories may be normalized into architectural roles before comparison, so moving files or
renaming C++ types does not change the oracle.

The required capability/non-goal matrix is:

| Capability or mechanism | classification | Deterministic verification oracle |
|---|---|---|
| Core page management, replacement, tuple storage, B+ tree, MVCC, WAL/recovery, execution, and relational optimization | Implemented within DBlusBlus; not delegated to an external database engine/framework | Build/link and service-registration graph contains DBlusBlus-owned roles and no external provider for a complete core role |
| Distributed consensus | Excluded | No public service, protocol, configuration, registration, or dependency provider |
| Replication | Excluded | Same closed-capability manifest check |
| Sharding | Excluded | Same closed-capability manifest check |
| Multi-node execution | Excluded | Same closed-capability manifest check |
| Cloud-native storage | Excluded | Same closed-capability manifest check |
| Full PostgreSQL/MySQL compatibility | Excluded | No compatibility-mode claim/registration; the closed SQL registry exposes only the architecture-owned v1 grammar |
| Stored procedures | Excluded | Closed SQL/API registry has no procedure-definition or invocation surface |
| Triggers | Excluded | Closed SQL/catalog/operation registry has no trigger object or execution surface |
| User-defined extensions | Excluded | No extension-loading/registration surface |
| Sophisticated authentication/authorization | Excluded | No public claim or registration makes a sophisticated authentication/authorization service part of the contract; any narrower incidental mechanism cannot expand that contract |
| Primary columnar storage | Excluded | Table-storage registration exposes the row-oriented heap owner, not a columnar primary store |
| JIT query compilation | Excluded | No JIT provider or selectable execution capability |
| GPU execution | Excluded | No GPU provider or selectable execution capability |

General-purpose libraries are permitted when they provide utilities rather than a complete
database mechanism. The dependency oracle rejects a library if an external component owns
page replacement, persistent tuple/page management, B+ tree semantics, MVCC decisions,
transaction locking, WAL/recovery, relational execution, or relational optimization behind
an embedded-database/framework interface. A checksum, compression, test, allocator, or OS
binding library is not prohibited merely because a database subsystem calls it; the manifest
records the narrow utility contract and proves the DBlusBlus subsystem retains semantic
ownership.

The language-mode oracle inspects every production target's effective compiler invocation
and build metadata, not a historical build result. Each target must select standard C++20
semantics, must not select a pre-C++20 or newer-language-only contract, and must disable or
diagnose compiler-extension mode. A compile-only conformance corpus contains one standard
C++20 positive feature from each language area used by the project and isolated negative
translation units that compile only under an enabled vendor extension. The positive corpus
must be accepted and every negative extension corpus rejected on both supported architecture
build configurations. Dependency headers visible to production targets are included in the
effective-mode inspection.

The platform matrix is:

| OS | ISA | Language/ABI baseline | Required API capability probes | Conformance oracle |
|---|---|---|---|---|
| Linux | x86-64 | Standard C++20; fixed-width integer and explicit-byte codecs; no native persisted layout | POSIX descriptor-relative/no-follow file operations, `pread`/`pwrite`, `fdatasync`, directory `fsync`, same-filesystem rename/unlink semantics, process-associated record locking | Compile-mode manifest plus isolated API/behavior probes and byte-codec corpus |
| Linux | ARM64 | Same | Same | Same fixtures and expected semantic results; no host-endian/alignment-dependent expected bytes |

The probes verify required API semantics directly and classify an environment lacking a
required behavior as unsupported. Optional facilities such as `io_uring`, direct I/O, huge
pages, and NUMA APIs are absent from the required-capability column and cannot become an
implicit conformance prerequisite.

## Architectural dependency and metadata-crossing oracle

Build a normalized directed graph whose vertices are the Chapter-2 architectural roles and
whose edges mean “consumes the public contract of.” Edges are derived from public type/API
references, build dependencies, link dependencies, registrations, and serialized boundary
manifests. The test-side expected graph is independent of implementation directory layout.
It permits an implementation to split or combine internal modules only when the resulting
public dependency graph preserves the same ownership direction.

Every cross-layer metadata carrier has a separate manifest row:

| Metadata carrier | Canonical owner | Permitted lower-layer consumption | Forbidden inversion |
|---|---|---|---|
| Parsed/raw SQL syntax | parser/front end | binder only through the raw-AST boundary | storage, BufferPool, B+ tree, TupleCodec, and WAL depend on parser objects |
| Bound identities and immutable descriptors | binder/catalog | explicitly named planning, execution, tuple-layout, index-key, and owner-validation consumers | consumer re-resolves names or assumes ownership of catalog policy |
| Physical schema/layout/type metadata | catalog/type/tuple-layout owner | TupleCodec and registered heap validation | BufferPool interprets columns or SQL constraints |
| PageId/FileId and page-format metadata | storage owner | BufferPool and physical page owners | parser/binder acquires page-format responsibility |
| Tuple headers | heap format owner | visibility and transaction consumers | HeapPage decides SQL visibility |
| Logical plan properties | logical planner | optimizer/physical planner | page, WAL, or raw storage depends on logical operators |
| Execution metrics | execution owner | profiling/optimizer feedback owners where explicitly permitted | optimizer estimates become storage/page-format policy |

For every forbidden direct edge, also compute transitive reachability after removing
explicitly allowed immutable-carrier edges. The required negative cases are:

| Consumer role | Forbidden dependency or knowledge |
|---|---|
| B+ tree | SQL syntax, parser objects, or table-statement policy |
| BufferPool | tuple schema, SQL column meaning, heap tuple semantics, or B+ key semantics |
| WAL | SQL statement kinds such as SELECT |
| Parser | physical page layout |
| HeapPage/page parser | transaction-visibility policy |
| TupleCodec | page management, BufferPool ownership, parser, or raw AST |
| Execution hot path | SQL name resolution |

Mutation tests add one forbidden direct edge and one transitive edge through an otherwise
innocent adapter; both must fail with a path identifying the ownership inversion. Positive
tests pass immutable descriptors through each allowed crossing and prove the consumer can
use resolved IDs/layout without importing the owner's syntax or policy interface.

For the HeapPage ownership boundary, allocate one guarded 8192-byte backing image through a
test allocator that records allocation size, owner, and lifetime. Construct, move, use, and
destroy any number of heap-page views/controllers over that image. The oracle requires all
page bytes observed by the view to alias the caller-owned image, no second page-sized
allocation attributable to the view/controller, and no ownership release of the backing
image by the view. Small bookkeeping allocations are not prohibited. Repeat with stack,
test-owned, and BufferPool-owned backing storage; poison the backing lifetime after guard
release and require the existing borrowed-view procedure to reject stale use.

## POSIX database-owner lock verification

Interpose or instrument the record-lock operation at the syscall boundary while a separate
process supplies the conflict oracle. The successful acquisition request must be exactly:

```text
operation   = fcntl
command     = F_SETLK
lock type   = F_WRLCK
l_whence    = SEEK_SET
l_start     = 0
l_len       = 0
behavior    = nonblocking whole-file lock
object      = the retained no-follow regular database.control inode
```

Run one mutation for `F_SETLKW`, `flock`, read lock, nonzero start, finite/nonzero length,
wrong `whence`, and wrong inode. The syscall-event oracle rejects each even if a simple
two-process contention test might accidentally look exclusive. A held correct lock causes a
second process to receive the architecture-owned nonwaiting `DATABASE_BUSY` result before
control/WAL/recovery inspection; releasing the owner or terminating its process permits an
independent acquisition.

The descriptor-lifetime fixture records every open/dup/close event with `(device,inode)`,
descriptor provenance, close-on-exec state, and owner phase. After acquisition, the control
descriptor is the sole independently opened descriptor for that inode under the database
owner; all control reads/writes borrow it, and no owner-process duplicate/alias may create
another independently closable control descriptor while the lock is held. Inject an
attempted second independent open and require rejection before use. In a negative harness,
deliberately open and close another descriptor for the inode and demonstrate with a waiting
child that POSIX process-lock ownership can be lost; the conforming event trace must contain
no such operation. Ordinary
cleanup closes all other database users/descriptors first, closes/releases this descriptor
last, and removes the process-local claim only afterward. No scheduler timing is used.

Compose this fixture with §4.7.9 removal admission. Hold an ordinary database owner
in one process, then call removal from that same process using both its original
path and a path alias that resolves to the same retained root. Before and after
each rejected call, have an independent process attempt the exact nonblocking
control lock. Both attempts must observe contention, and removal must return
`DATABASE_BUSY` without another control-inode open/close event or destructive
inspection. Use barriers and record root identity, local reservation, descriptor,
and lock events; prove the existing owner is still live when each probe executes.
The negative harness opens and closes the competing control descriptor before
probing again, which must make the independent probe succeed and expose the lost
lock. A fixture in which the original owner has already closed is not evidence
for this case. This procedure composes §§3.3.2–3.3.3 with the removal admission
tests and does not authorize changing the lock primitive.

The existing Database Lifecycle Tests remain the owner for lifecycle states, every-open
recovery, READY admission, failed-open cleanup, shutdown, NONCONTINUABLE, and durable-COMMIT
preservation. `OPEN_FAILED` and `SHUTDOWN_FAILED` remain results rather than states.

## Generic page-format verification families

### Canonical PAGE_SIZE identity

Generate a semantic inventory of every production consumer that allocates, offsets, reads,
writes, checksums, WAL-embeds, validates, or bounds an ordinary page. Resolve each consumer's
effective page-size expression to the one canonical `PAGE_SIZE` interface and compare its
value with an independent literal architecture oracle of 8192. Reject a duplicated local
constant even when it presently equals 8192, and reject any consumer whose expression can
diverge by configuration. Literal 8192 values used solely as fixed persisted-format test
vectors are permitted and identified as oracle data rather than production configuration.

### Common page header

Construct and decode the 32-byte common header independently, using explicit little-endian
test helpers and a distinctive nonpalindromic value for every multibyte field:

| Offset | Width | Field | Generic oracle |
|---:|---:|---|---|
| 0 | 2 | `page_type` | literal PageType code selected by the owning specialization |
| 2 | 2 | `format_version` | `1`; zero corrupt, positive greater unsupported after family recognition |
| 4 | 4 | `flags` | zero for every current ordinary page and FileSuperblock |
| 8 | 8 | `page_lsn` | exact little-endian LSN/sentinel under the owning WAL state |
| 16 | 4 | `checksum_crc32c` | independent CRC32C over the owning 8192-byte image with bytes 16..19 logically zero |
| 20 | 2 | `header_size` | exact owner-selected complete header size |
| 22 | 2 | `reserved16` | zero |
| 24 | 8 | `page_no` | exact embedded logical PageNo |

Compare every byte, then mutate each field independently with a recomputed checksum so the
targeted structural classifier—not incidental CRC failure—selects the result. Also test
short input, overlong page transfer, wrong embedded PageNo, wrong owner, unknown PageType,
wrong-context known PageType, version zero, future version, each nonzero flag bit, and
nonzero reserved16. Specialized page procedures must first pass this family and then their
local header/body oracle; they cannot replace the generic check.

### Generic 72-byte FileSuperblock and HEAP specialization

The generic fixture is exactly one 8192-byte page and independently constructs:

| Offset | Width | Field / rule |
|---:|---:|---|
| 0..31 | 32 | common header: SUPERBLOCK, version 1, flags zero, owner-legal page LSN, CRC, kind-selected header size, reserved zero, PageNo 0 |
| 32 | 8 | exact ASCII `DBLUSBLS` |
| 40 | 2 | FileKind |
| 42 | 2 | zero |
| 44 | 4 | page size 8192 |
| 48 | 4 | initialized nonzero FileId |
| 52 | 4 | zero |
| 56 | 8 | kind-specific object identity |
| 64 | 8 | opaque creation epoch; every bit pattern remains data |
| 72..8191 | 8120 | zero for HEAP, FSM, CATALOG, and TXN_STATUS |

For every field use a distinctive positive vector, minimum/maximum legal values where the
owner permits them, one-field corruption with recomputed CRC, truncated lengths 0/71/8191,
and a separate overlong-file/page framing fixture. A decoder presented a larger caller
buffer may consume only the first page. The owning managed-file procedure independently
validates page alignment and the selected family's whole-file bounds; complete pages after
page 0 are not superblock trailing bytes. `FileKind::BTREE` must dispatch to the existing
128-byte specialized family and must not be parsed as a generic 72-byte superblock.

Instantiate the generic family for HEAP, FSM, CATALOG, and TXN_STATUS. The HEAP row requires
literal `FileKind::HEAP=1`, `header_size=72`, the expected heap FileId,
`object_id=TableId`, exact managed
basename/catalog identity, zero flags/reserved/trailing bytes, and valid CRC. Mutate kind,
FileId, TableId, basename/descriptor relation, flags, every reserved field/range, page size,
version, header size, magic, PageNo, truncation, a larger caller buffer, and nonzero bytes
within the reserved page-0 suffix independently. Additional aligned HEAP data pages are
validated by the published-page/file-length owner rather than classified as superblock
overlength. Expected results preserve the §4.14 corruption-versus-unsupported distinction.

The test-side FileKind registry is also literal: `0=INVALID`, `1=HEAP`, `2=BTREE`, `3=FSM`,
`4=CATALOG`, and `5=TXN_STATUS`. Sweep every legal code in its matching and wrong managed
context, reject zero and representative unassigned uint16 values as `CORRUPT_FILE`, and
dispatch code 2 exclusively to the B+ specialized superblock family.

### PageType and context registry

Use the complete closed registry below. For each row, create one canonical page in every
allowed context and one valid-checksum fixture in every wrong FileKind/page-position
context. Wrong context and every unlisted uint16 code are `CORRUPT_PAGE`; a recognized
expected family with positive future version is `UNSUPPORTED_PAGE_FORMAT`.

| Code | PageType | Allowed context |
|---:|---|---|
| 0 | SUPERBLOCK | page 0 of HEAP, FSM, BTREE, CATALOG, or TXN_STATUS, with kind-specific superblock codec |
| 1 | HEAP_DATA | HEAP page 1 through the published bound, including catalog-relation heaps |
| 2 | FSM_DATA | FSM page 1 through the published bound |
| 3 | BTREE_INTERNAL | BTREE ordinary published page |
| 4 | BTREE_LEAF | BTREE ordinary published page |
| 5 | BTREE_FREE | BTREE ordinary published page |
| 6 | CATALOG_DATA | CATALOG page 1 immutable bootstrap page |
| 7 | TXN_STATUS | TXN_STATUS page 1 through the published bound |

The registry oracle uses literal numeric values, not production enum iteration. It sweeps
codes 8, 9, 255, 256, and 65535 plus a generated representative from every unassigned range.

### Writer, redo, and compatibility composition

For each page family below, inject one otherwise valid after-image and each local structural
defect. Observe the publication boundary rather than a particular implementation method:

| Family | Ordinary writer owner | Redo owner | Required prepublication validator |
|---|---|---|---|
| HEAP_DATA | heap/DML/vacuum | PAGE_INIT/PAGE_DELTA/PAGE_IMAGE | generic common header plus complete HEAP L1 and available nonfetching L2 |
| FSM_DATA | FSM mutation/rebuild | PAGE_INIT/PAGE_DELTA/PAGE_IMAGE | generic header plus Chapter-6 L1/prefix/owner bounds |
| BTREE pages | B+ MTR | BTREE_MTR/full image | generic header plus specialized B+ complete reconstructed-result validation |
| CATALOG_DATA | bootstrap creation only | bootstrap/recovery owner | generic header plus immutable catalog bootstrap validator |
| Catalog HEAP_DATA | catalog heap/DML | ordinary page WAL | HEAP L1 under canonical catalog descriptor |
| TXN_STATUS | terminal/status owner | status PAGE_INIT/image/delta | generic header plus status L1/high-water rules |
| Generic/specialized superblocks | file/bootstrap/index owner | owning creation/recovery protocol | generic or B+ superblock byte/owner validator |

Before the listed validation succeeds, no ordinary guard, published bound, descriptor,
catalog authority, index root, status lookup, or writer-latch release may expose the image.
Fault each validator conjunct and require no publication. Successful redo and ordinary
writer output must be observationally identical under the same local validator, except for
owner-defined WAL/page-LSN state. Existing BufferPool, PAGE_INIT/MTR, WAL, DML, catalog, and
recovery procedures supply the publication event and durable-prefix oracles.

The consolidated compatibility matrix is:

| Persisted family/state | Criticality | Recognizable newer form | Malformed v1 | Permitted fallback |
|---|---|---|---|---|
| database.control, required WAL, bootstrap/core catalog, required FileSuperblock/page, heap tuple, B+ key/page, TXN_STATUS, required default | required | exact owner-specific unsupported-format result; no reinterpretation | owner-specific corruption/open failure | none |
| FSM category values | known advisory data | future FSM page/file format is unsupported, not guessed | malformed FSM structure rejected/rebuilt only under Chapter 6's exact derived-state policy | authoritative heap reconstruction only |
| Statistics payload/envelope cases listed in §4.14.5 | rebuildable advisory whitelist only | discard complete unsupported generation/scope | discard malformed known generation/scope after safe catalog tuple decode | older complete valid descriptor or missing-statistics fallback |
| Unknown filesystem name | not a database object | not opened as a format | not inferred | preserve/ignore under namespace owner; never evidence of compatibility |
| REDIRECT_RESERVED | known unsupported heap slot state | N/A | N/A | `UNSUPPORTED_RESERVED_STATE`; never reinterpret |
| Unknown FileKind/PageType/slot/required enum | required registry violation | future version only when selected by an owning version discriminator | corruption in claimed | none |

For each row pair an unsupported fixture with a malformed-v1 fixture and assert distinct
classification. Required state is never silently rebuilt, advisory fallback never publishes
partial mixed versions, and no rewrite/vacuum/recovery path clears unknown material and
publishes a downgraded object.

## Heap page, free-list, and tuple-format verification

### Physical scan and HEAP_DATA header

Create pages whose PageNos, SlotIds, tuple values, insertion order, and physical tuple byte
offsets all induce different orders. The physical heap scan oracle emits NORMAL physical
versions by ascending PageNo and then ascending SlotId, independent of tuple values and byte
offsets. Compare the physical sequence exactly. Feed the same rows through an unordered SQL
plan and compare only bags; the physical order creates no SQL ordering property.

The standalone HEAP_DATA byte fixture first applies the generic common-header family and
then checks:

| Offset | Width | Field / canonical rule |
|---:|---:|---|
| 32 | 2 | `slot_count` |
| 34 | 2 | `free_slot_head` |
| 36 | 2 | `lower = 48 + slot_count * 8` |
| 38 | 2 | `upper`, with `48 <= lower <= upper <= 8192` |
| 40 | 4 | `prune_hint`, arbitrary uint32 advisory value |
| 44 | 4 | zero heap reserved field |

The canonical blank page has HEAP_DATA/version 1/header size 48, PageNo/owner selected by
the fixture, `slot_count=0`, `free_slot_head=INVALID_SLOT_ID`, `lower=48`, `upper=8192`,
`prune_hint=0`, all flags/reserved fields zero, and a valid checksum. Compare all 8192 bytes.
Test `prune_hint` values 0, 1, a distinctive nonzero value, and UINT32_MAX while keeping the
page otherwise identical; all are structurally valid and produce the same authoritative
free-space and tuple results. No test assigns pruning permission to the hint.

### Slot-state and free-list graph oracle

Decode each 8-byte slot with a literal code registry:

| Code/state | Coordinates | `aux` | Free-list membership | Result |
|---:|---|---|---|---|
| 0 UNUSED | exactly `(0,0)` | next in-range UNUSED SlotId or INVALID_SLOT_ID | exactly once | valid reusable state |
| 1 NORMAL | nonzero complete retained tuple range | 0 | never | valid ordinary physical tuple |
| 2 DEAD retained | nonzero complete retained tuple range | 0 | never | valid retained nonreturnable tuple |
| 2 DEAD reclaimed | exactly `(0,0)` | 0 | never | valid nonreusable retired SlotId |
| 3 REDIRECT_RESERVED | uninterpreted | uninterpreted | never | `UNSUPPORTED_RESERVED_STATE` |
| 4..65535 | none | none | never | `CORRUPT_HEAP` |

Mutate NORMAL `aux`, mixed-zero DEAD coordinates, UNUSED coordinates, state code, tuple
range, and retained tuple grammar independently. The free-list oracle builds the expected
set of every UNUSED slot and traverses from `free_slot_head` for at most `slot_count` links
using an independent visited set. Required cases are empty list, complete list in non-SlotId
order, missing UNUSED, duplicate predecessor/member, NORMAL member, either DEAD form,
REDIRECT_RESERVED, out-of-range link, self-cycle, multi-node cycle, and disconnected cycle.
Every invalid case terminates and is `CORRUPT_HEAP` except the direct recognized reserved
state, which retains its exact unsupported classification.

For insertion reuse, pause around one page mutation/WAL unit. Before publication the old
head and UNUSED slot are authoritative; after publication `free_slot_head=old.aux`, the
selected slot is canonical NORMAL with `aux=0`, and its complete tuple/range is installed.
No observer may see a popped-but-UNUSED or linked-but-NORMAL intermediate. DEAD becomes
UNUSED only through the existing Chapter-14 grace-complete fixture.

### Tuple header and predecessor graph

Construct all 48 tuple-header bytes independently:

| Offset | Width | Field | Required corpus |
|---:|---:|---|---|
| 0 | 8 | `xmin` | FROZEN or normal minimum/distinctive/maximum legal owner; invalid/context defects |
| 8 | 8 | `xmax` | INVALID or normal deleter; FROZEN forbidden as deleter |
| 16 | 4 | `cmin` | 0, distinctive, UINT32_MAX under owner causality |
| 20 | 4 | `cmax` | canonical 0 when xmax invalid; 0/distinctive/UINT32_MAX for actual deleter as owner permits |
| 24 | 8 | `prev_page_no` | INVALID or published ordinary same-heap PageNo |
| 32 | 2 | `prev_slot` | INVALID paired with invalid PageNo or legal SlotId |
| 34 | 2 | `tuple_flags` | exact known mask 0x0003 and canonical relationships below |
| 36 | 2 | `header_bytes` | exactly 48 |
| 38 | 2 | `null_bitmap_bytes` | exact schema-derived value |
| 40 | 4 | `schema_version` | 1 for v1 writers; nonzero historically resolvable value for readers |
| 44 | 4 | `reserved` | zero |

Every multibyte field uses distinctive little-endian bytes; mutate every field and reserved
byte independently. Preserve the existing DML vectors proving `CommandId{0}` legal and
`xmax=INVALID_TXN_ID` requires `cmax=0`; invalid xmax with any nonzero cmax remains
`CORRUPT_HEAP` and is never normalized by ordinary read.

The predecessor oracle models a directed graph keyed by `(heap FileId,PageNo,SlotId)`.
Positive cases cover no predecessor, same-page earlier version, and cross-page earlier
version in the same heap. Negative cases cover each mixed sentinel pair, invalid/superblock/
unpublished PageNo, invalid or out-of-range SlotId, wrong heap FileId supplied by the
traversal owner, direct self-reference, two-node and longer cycles, target wrong state, and
rebound/stale RID under the existing ReadEpochGuard rule. Local validation rejects mixed
sentinels and obvious self-reference; followed links receive L2 owner/state checks; the
explicit verifier traverses with a visited set and a bound no greater than the selected
heap's retained slots. It never consults MVCC visibility to establish structural validity.

### Tuple flags, body layout, NULL, and VARCHAR

The literal tuple-flag registry is:

```text
HAS_NULLS  = 0x0001
HAS_VARLEN = 0x0002
KNOWN_MASK = 0x0003
```

Exercise all four valid bit combinations with schemas/values that make each canonical.
Set every other individual bit and representative combinations; recognized v1 decode must
return `CORRUPT_HEAP`. `HAS_VARLEN` is set iff the interpreting physical schema contains a
VARCHAR column, including all-NULL and all-empty VARCHAR rows; it is clear for every
fixed-only schema. `HAS_NULLS` is set iff at least one used bitmap bit is one. Both mismatch
directions for each flag are separate corruption fixtures.

The independent tuple-layout oracle computes, with widened checked arithmetic:

```text
null_bitmap_bytes = column_count / 8 + (column_count % 8 != 0)
fixed_area_offset = 48 + null_bitmap_bytes
fixed offsets     = schema-order prefix sums of widths 1,4,8,8,4,8,8
varlen start      = end of the fixed area
tuple size        = varlen start + sum(non-NULL VARCHAR payload lengths)
```

There is no alignment or padding. Test zero columns and counts 1, 7, 8, 9, 15, 16, 17 plus
the maximum complete schema admitted by the owning format. Use permutations that place
4-byte and 8-byte fields at deliberately unaligned offsets. Compare every offset and exact
final length without calling production TupleCodec. For fixed-only and varlen schemas test
one byte short, exact length, one trailing byte, truncated descriptor/payload, arithmetic
overflow, and a cursor that ends before or after the supplied extent.

Null bits use LSB-first schema order. For every boundary count, set each used bit alone and
compare its exact byte/mask. For a nonmultiple-of-eight count, set each unused high bit
independently and require `CORRUPT_HEAP`. A persisted NULL bit for a descriptor-declared
NOT NULL column is malformed heap data and is `CORRUPT_HEAP`, distinct from a runtime
constraint violation while constructing a new row.

For NULL fixed-width values, compare two reader fixtures differing only in ignored fixed
bytes: canonical writer output is all zero, while nonzero historical ignored bytes remain
reader-valid when every other rule is valid and must decode to the same NULL. A writer
mutation that emits nonzero ignored bytes fails the canonical-output oracle. NULL VARCHAR
remains stricter: its descriptor is exactly `(0,0)`.

For every VARCHAR schema order, the test-side cursor starts at the independently computed
varlen start. A present descriptor contains an absolute tuple-relative uint32 offset equal
to the current cursor and a uint32 length; the payload advances the cursor exactly. Cover
NULL, present empty, ASCII, embedded NUL, bytes 0x80..0xff, multiple empty values sharing the
current cursor, maximum inline payload, and several columns. Reject a gap, overlap, backward
offset, payload reordering, reference into header/bitmap/fixed area, checked end overflow,
out-of-bounds end, trailing unreferenced bytes, and final cursor mismatch. Payload bytes are
opaque; UTF-8, collation, terminators, and character counts are not storage validators.

The complete HEAP L1 mutation matrix starts with one canonical page and injects one defect
at a time across common header, heap header, geometry, free-list graph, every slot-state
form, retained-range bounds/overlap, complete tuple header, flags, bitmap, schema resolution,
fixed values, VARCHAR packing, and exact length. Expected results come from the byte, range,
set, and graph oracles above rather than the production page validator. This family is the
direct §41.1 owner for complete retained-DEAD validation, exact UNUSED membership/cycle
rejection, tuple serialization, and tuple corruption coverage.

HeapPage ownership and TupleCodec dependency checks reuse the Chapter-2 allocation and
dependency-graph procedures. Borrowed tuple/VARCHAR lifetime reuses Buffer management
verification. INSERT/UPDATE/DELETE physical-version rules, the old/new RID distinction,
canonical command metadata, and the 8135/8136 inline boundary retain their existing DML,
MVCC, reclamation, and exhaustion owners.

## Foundational mandatory matrices

### Chapter 1 capability matrix

| Atomic group | Architecture owner | Procedure/oracle |
|---|---|---|
| End-to-end scope inventory | §1.1 | Pure scope/navigation; detailed semantics belong to later owners |
| Design-objective and rationale statements without an independently falsifiable conformance result | §§1.2, 1.4–1.5 | Documentation-role classification |
| Direct implementation of core database mechanisms | §§1.1–1.2 | capability/dependency manifest and linkage/service graph |
| Thirteen non-goals | §1.3 | Closed capability/non-goal manifest |
| C++20 language baseline | §1.4 | Effective language-mode and extension corpus |
| Heap-version MVCC foundational choice | §1.4 | MVCC Visibility Tests plus DML version-pair procedures |

### Chapter 2 dependency matrix

| Atomic group | Architecture owner | Procedure/oracle |
|---|---|---|
| Explanatory layer diagram, performance considerations, and nonnormative organization guidance | §§2.1, 2.3–2.5 | Documentation-role classification |
| Lower-layer direction, permitted immutable crossings, and tuple/page separation | §§2.1–2.2 | Dependency graph plus metadata-crossing manifest |
| Six explicit forbidden-dependency examples | §2.1 | Direct/transitive forbidden-edge mutation matrix |
| Six logical/physical ownership distinctions | §2.2 | Crossing manifest plus existing planner/lock/page/metric owners |
| Seven critical storage dependency rules | §2.5 | Dependency graph, HeapPage allocation fixture, and BufferPool raw-I/O inventory |

### Chapter 3 platform and lifecycle matrix

| Atomic group | Architecture owner | Procedure/oracle |
|---|---|---|
| Linux x86-64/ARM64, POSIX API, C++20/no-extension baseline | §§3.1–3.2 | Platform matrix and language-mode manifest |
| Lifecycle states/results/transitions/admission | §3.3.1 | Database Lifecycle Tests and transition/fault matrix |
| Process ownership, exact record lock, registry, inode, and descriptor lifetime | §3.3.2 | Exact POSIX lock and descriptor/inode procedures plus root harness |
| Ordered open and mandatory recovery | §3.3.3 | Open fault matrix and required-input procedures |
| READY publication and failed-open cleanup | §3.3.4 | READY/admission and cleanup event oracle |
| NONCONTINUABLE behavior | §3.3.5 | NONCONTINUABLE and durable-COMMIT procedure |
| Controlled shutdown | §3.3.6 | Instrumented shutdown sequence |
| Create/removal/crash/error integration | §3.3.7 | Root publication/removal and crash matrices |

### Chapter 4 storage-foundation matrix

| Atomic group | Architecture owner | Procedure/oracle |
|---|---|---|
| Page serialization and canonical PAGE_SIZE | §§4.1–4.2 | PAGE_SIZE identity, common-byte, and no-native-layout procedures |
| Identifier widths/sentinels/FileId identity | §§4.3–4.3.1 | Exhaustion inventory, BufferPool FileId registration, and byte fixtures |
| Checked-next and exhaustion | §4.3.2 | Numeric Exhaustion and Terminal-Boundary Verification |
| PageId/RID/index-to-heap composition | §§4.4–4.6 | BufferPool, B+ RID, MVCC, and DML old/new-version procedures |
| File kinds, namespace, object/root lifecycle | §4.7 | Generic superblock plus Database Lifecycle and DDL/recovery procedures |
| Common header, PageType, and FileSuperblock | §§4.8–4.10 | Generic header/superblock, HEAP specialization, and closed registry |
| Append-first allocation and checksums | §§4.11–4.12 | BufferPool new-page, PAGE_INIT/MTR, stable-flush, and CRC procedures |
| L0–L3 structural validation | §4.13 | Family validators, free-list and predecessor graph, publication matrix |
| Compatibility and reserved-state policy | §4.14 | Consolidated compatibility matrix plus family format fixtures |

### Chapter 5 heap and tuple matrix

| Atomic group | Architecture owner | Procedure/oracle |
|---|---|---|
| Heap organization and physical scan order | §§5.1–5.2 | Physical scan fixture and unordered-SQL contrast |
| HEAP_DATA header and free-slot head | §5.3 | Exact header/blank-page byte fixtures |
| Slot states, DEAD/UNUSED, compaction, and free-list publication | §§5.4–5.5 | Literal slot registry and graph/publication oracle |
| Inline tuple bound | §5.6 | Numeric exhaustion and 8135/8136 fixtures |
| Complete tuple header and predecessor graph | §5.7 | 48-byte corpus, DML header vectors, and graph oracle |
| Tuple flags | §5.8 | Literal known-mask and bidirectional flag matrix |
| Canonical body geometry and exact length | §5.9 | Independent schema-prefix-sum layout oracle |
| Null bitmap and canonicality | §5.10 | Bitmap formula/order/high-bit/NOT-NULL/NULL-byte matrix |
| Fixed-width values | §5.11 | Tuple codec and Chapter-17 scalar handoff fixtures |
| VARCHAR | §5.12 | Independent cursor/descriptor/payload matrix |
| Schema version | §5.13 | Historical-schema descriptor oracle |
| INSERT/UPDATE/DELETE/MVCC boundaries | §§5.14–5.17 | DML, MVCC, WAL, and reclamation procedures |
| HeapPage/TupleCodec/view ownership | §§5.18–5.20 | Dependency/allocation graph and borrowed-view procedure |

### Foundational obligation traceability

The following rows map foundational obligations to their deterministic procedures and
independent oracles. They index the families above without defining additional obligations.

| Chapter 1 obligation | Architecture owner | Procedure/reference | Independent oracle |
|---|---|---|---|
| Core mechanisms are DBlusBlus-owned | §§1.1–1.2 | Capability/dependency manifest | Normalized build/link/service-role graph |
| Distributed consensus excluded | §1.3 | Closed non-goal row | Capability-surface inventory |
| Replication excluded | §1.3 | Closed non-goal row | Capability-surface inventory |
| Sharding excluded | §1.3 | Closed non-goal row | Capability-surface inventory |
| Multi-node execution excluded | §1.3 | Closed non-goal row | Capability-surface inventory |
| Cloud-native storage excluded | §1.3 | Closed non-goal row | Capability-surface inventory |
| Full PostgreSQL/MySQL compatibility excluded | §1.3 | Closed non-goal row | SQL/API capability registry |
| Stored procedures excluded | §1.3 | Closed non-goal row | SQL/API capability registry |
| Triggers excluded | §1.3 | Closed non-goal row | SQL/catalog/operation registry |
| User-defined extensions excluded | §1.3 | Closed non-goal row | Extension-registration inventory |
| Sophisticated authentication/authorization excluded | §1.3 | Closed non-goal row | Public-contract capability manifest |
| Primary columnar storage excluded | §1.3 | Closed non-goal row | Table-storage role registry |
| JIT compilation excluded | §1.3 | Closed non-goal row | Execution-provider inventory |
| GPU execution excluded | §1.3 | Closed non-goal row | Execution-provider inventory |
| Standard C++20 baseline | §1.4 | Effective language-mode procedure | Compiler-mode manifest and extension-negative corpus |

| Chapter 2 obligation | Architecture owner | Procedure/reference | Independent oracle |
|---|---|---|---|
| Lower layers do not depend on unresolved syntax/policy | §2.1 | Direct/transitive dependency audit | Normalized architectural-role graph |
| Immutable metadata crosses only canonical owner interfaces | §§2.1–2.2 | Metadata-crossing manifest | Expected owner/consumer/reverse-edge table |
| Tuple-format interpretation is separate from page-local management | §§2.1–2.2 | Boundary graph and positive carrier fixtures | Forbidden-edge reachability oracle |
| B+ tree does not require SQL/parser objects | §2.1 | Forbidden dependency mutation | Direct/transitive path oracle |
| Buffer management does not require tuple schema | §§2.1, 2.5 | Forbidden dependency mutation | Public-type/API graph |
| Buffer management does not require heap/B+ semantics | §2.5 | Forbidden dependency mutation | Public-type/API graph |
| HeapPage owns no second page image | §2.5 | Guarded backing-image fixture | Allocation/alias/lifetime event oracle |
| TupleCodec is independent of page management | §2.5 | Forbidden dependency mutation | Public-type/API graph |
| TupleCodec is independent of parser/raw AST | §2.5 | Forbidden dependency mutation | Public-type/API graph |

| Chapter 3 obligation | Architecture owner | Procedure/reference | Independent oracle |
|---|---|---|---|
| Linux x86-64 supported baseline | §3.1 | Platform matrix row | Compile/API/byte-behavior probe corpus |
| Linux ARM64 supported baseline | §3.1 | Platform matrix row | Same architecture-neutral corpus |
| POSIX file API baseline | §3.1 | Required API capability probes | Isolated syscall/filesystem behavior oracle |
| C++20 without compiler-extension dependence | §3.2 | Chapter-1 language-mode family | Effective compiler-mode and negative corpus |
| Exact nonblocking whole-file `fcntl(F_SETLK)` write lock | §3.3.2 | Syscall-boundary lock matrix | Captured lock tuple plus independent process conflict |
| Sole independently opened control descriptor | §3.3.2 | Descriptor/inode event fixture | `(device,inode)` provenance trace |

| Chapter 4 obligation | Architecture owner | Procedure/reference | Independent oracle |
|---|---|---|---|
| One canonical PAGE_SIZE source | §4.2 | Semantic consumer inventory | Expression-identity graph plus literal 8192 oracle |
| Complete generic common header | §4.8 | 32-byte family | Independent endian byte constructor/parser |
| Complete generic FileSuperblock prefix | §4.10 | 72-byte family | Independent endian byte constructor/parser |
| HEAP FileSuperblock specialization | §§4.10, 4.13.6 | HEAP mutation corpus | Byte/identity/name/CRC oracle |
| Closed PageType/context registry | §§4.9, 4.13.2 | Registry/context sweep | Literal code/context table |
| Complete HEAP local validation | §4.13.3 | HEAP L1 one-defect corpus | Independent byte/range/set/graph oracles |
| Exact UNUSED membership | §4.13.3 | Free-list graph family | Expected-set equality |
| Free-list bounded progress and cycle rejection | §§4.13.3, 4.13.9 | Cycle/out-of-range corpus | Visited set and `slot_count` bound |
| Version/predecessor-chain L3 validity | §§4.13.9, 5.7.4 | Predecessor graph family | Selected-heap RID graph |
| Writer/redo publication validity | §4.13.8 | Publication composition matrix | Validator result plus publication-event trace |
| Compatibility policy is closed by family/criticality | §4.14 | Compatibility registry | Paired unsupported/malformed/fallback fixtures |

| Chapter 5 obligation | Architecture owner | Procedure/reference | Independent oracle |
|---|---|---|---|
| Ascending heap PageNo scan | §5.2 | Physical scan fixture | Literal sorted `(PageNo,SlotId)` sequence |
| Ascending SlotId within page | §5.2 | Physical scan fixture | Literal sorted `(PageNo,SlotId)` sequence |
| Exact HEAP_DATA local-header bytes | §5.3.1 | Header byte matrix | Independent endian constructor/parser |
| Canonical blank page/free head | §§5.3.2–5.3.3 | Blank 8192-byte image | Whole-image byte oracle |
| Every UNUSED slot appears exactly once | §5.3.2 | Free-list graph matrix | UNUSED-set equality |
| Free-list cycle/duplicate traversal rejected boundedly | §§5.3.2, 4.13.9 | Graph corruption corpus | Visited set and `slot_count` bound |
| `prune_hint` has no correctness meaning | §5.3.3 | 0/1/distinctive/max matrix | Equal authoritative page result |
| Literal slot-state codes | §5.4 | Closed state-code sweep | Test-side literal registry |
| NORMAL `aux=0` | §5.4.1 | One-field mutation | Canonical slot-byte oracle |
| State-specific slot fields are canonical | §§5.4.1–5.4.3 | Slot-state matrix | Byte/range/state table |
| Retained DEAD tuple is completely validated | §§5.4.3, 4.13.3 | HEAP L1 retained-DEAD corpus | Tuple grammar oracle |
| Free-list pop/NORMAL install is one publication | §5.5 | Paused mutation/WAL fixture | Before/after page-state oracle |
| Complete 48-byte tuple-header corpus | §5.7 | Header field/boundary matrix | Independent endian byte oracle |
| Canonical predecessor sentinel pair | §5.7.4 | Predecessor matrix | Literal pair classifier |
| Previous version remains in same heap | §5.7.4 | Selected-heap graph fixture | FileId-scoped RID graph |
| Self-reference and cross-page/link defects | §§5.7.4, 4.13.9 | Predecessor graph mutations | Visited set/domain oracle |
| Positive and boundary predecessor links | §5.7.4 | Same-page/cross-page positive corpus | FileId/PageNo/SlotId domain oracle |
| Closed tuple-flag registry | §5.8 | Known-mask sweep | Literal mask `0x0003` |
| Unknown tuple flags rejected | §5.8 | Single-bit/composite mutations | Known-mask oracle |
| HAS_VARLEN is bidirectional | §5.8.1 | Four schema/value combinations | Physical-schema oracle |
| Exact fixed offsets and no padding | §5.9 | Prefix-sum layout corpus | Independent checked-arithmetic model |
| Exact fixed-only and varlen tuple length | §5.9.1 | Short/exact/trailing/cursor corpus | Independent endpoint oracle |
| Null-bitmap byte formula | §5.10 | Boundary column counts | Independent ceiling formula |
| Null bits are LSB-first | §5.10.2 | Per-bit cross-byte corpus | Literal byte/mask oracle |
| Unused high null bits are zero | §5.10.4 | Each high-bit mutation | Used-bit mask oracle |
| HAS_NULLS is bidirectional | §5.10.5 | Four flag/bitmap combinations | Used-bit reduction oracle |
| NOT NULL persisted bit is corruption | §5.10.3 | Descriptor-directed malformed tuple | Independent schema/bitmap oracle |
| NULL fixed bytes: canonical writer, compatible reader | §5.10.6 | Paired writer/reader fixtures | Byte equality plus decoded-value oracle |
| VARCHAR uses exact strict packing | §§5.9.1, 5.12 | Gap/overlap/backward/trailing corpus | Independent cursor/range oracle |
| VARCHAR zero length and arbitrary bytes | §5.12 | Empty/NUL/high-byte corpus | Opaque payload byte oracle |
| HeapPage is a nonowning page view | §5.18 | Chapter-2 allocation fixture | Allocation/alias/lifetime event oracle |
| TupleCodec is independent of page management | §5.19 | Chapter-2 dependency graph | Forbidden-edge reachability oracle |
| TupleCodec is independent of parser/raw AST | §5.19 | Chapter-2 dependency graph | Forbidden-edge reachability oracle |

### Architecture verification-requirement composition

The HEAP L1, free-list, tuple-header, body-layout, NULL, and VARCHAR families above are the
direct procedures required by §41.1; scattered catalog, type, and DML examples may reuse
them but cannot substitute for their generic byte and corruption corpus. Database Lifecycle
Tests remain the §41.3 owner and compose with only the exact lock-call and descriptor-life
extensions above. Sections 41.4–41.7 continue to own their transaction, WAL/recovery,
MVCC/reclamation, and later-subsystem scenarios; they reuse canonical foundational bytes,
identities, and bounds without moving those rules out of Chapters 1–5.

| Methodology property | Deterministic check |
|---|---|
| Implementation independent | Reject source-path/class-layout dependencies; permit normalized semantic manifests |
| Independent persistent-format oracles | Require test-side endian/CRC/range/graph construction rather than production codecs/validators |
| Deterministic concurrency and lifecycle | Require barriers, coordinated processes, captured events, and explicit fault points; reject sleeps |
| Architecture remains authoritative | Every expected result cites a live Architecture owner or a reusable procedure with an independent oracle |

### Chapters 1–6 reusable-owner map


Chapter 6 is covered by Free-space map verification and its obligation map. The
generic common-header and FileSuperblock procedures above are reusable prerequisites for
its canonical fixtures; they do not change the Chapter-6 category, mapping, stale-state,
repair, rebuild, WAL, PAGE_INIT, or reclamation oracles.

The foundational composition preserves these exact regressions: every open performs
recovery; only READY admits ordinary work; the owner lock is released last; durable COMMIT
never becomes ABORTED; `OPEN_FAILED` and `SHUTDOWN_FAILED` are results, not states;
`CommandId{0}` is legal; invalid xmax with nonzero cmax is `CORRUPT_HEAP`; native structs
are never persistent codecs; stale FSM state is advisory; and root creation/removal use the
durable final publication/retirement model in §§4.7.8–4.7.9.
