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
- checksum finalization and corruption-detection rules.

Heap-page internals, tuple bytes, the free-space map, BufferPool behavior, B+ tree node formats, WAL records, and recovery protocols are defined by their owning chapters.

The storage foundation is intentionally shared: independently implemented storage, buffer, index, and transaction components MUST agree on these identities and persistent formats.

## 4.2 Page-oriented persistence and serialization

Persistent random-access database structures are page-oriented.

The page size is:

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

The logical identifier widths are:

| Identifier | Width | Logical representation |
|---|---:|---|
| `FileId` | 32 bits | `uint32_t` |
| `PageNo` | 64 bits | `uint64_t` |
| `SlotId` | 16 bits | `uint16_t` |
| `TxnId` | 64 bits | `uint64_t` |
| `CommandId` | 32 bits | `uint32_t` |
| `Lsn` | 64 bits | `uint64_t` |
| `TableId` | 64 bits | `uint64_t` |
| `ColumnId` | 32 bits | `uint32_t` |
| `IndexId` | 64 bits | `uint64_t` |
| `ConstraintId` | 64 bits | `uint64_t` |
| `SchemaVer` | 32 bits | `uint32_t` |

The invalid sentinels defined at this layer are:

| Sentinel | Value |
|---|---:|
| `INVALID_FILE_ID` | `0` |
| `INVALID_PAGE_NO` | `UINT64_MAX` |
| `INVALID_SLOT_ID` | `UINT16_MAX` |
| `INVALID_TXN_ID` | `0` |
| `INVALID_LSN` | `0` |

These values are part of the architecture wherever the corresponding sentinel is used.

This chapter does not define an invalid sentinel for `CommandId`, `TableId`, `ColumnId`, `IndexId`, `ConstraintId`, or `SchemaVer`.

### 4.3.1 File identity versus operating-system handles

`FileId` is a database-level logical identifier.

An operating-system file descriptor is a process-local resource and MUST NOT be exposed or persisted as `FileId`.

Persistent identity MUST remain valid independently of the descriptor number chosen by a particular process execution.

### 4.3.2 Identifier, counter, position, and encoded-length exhaustion

This section is the canonical exhaustion contract. A finite persisted identity,
monotonic counter, logical position, or encoded length is advanced only by a
**checked-next** operation:

```text
1. decode the current value into its unsigned logical domain
2. compute the complete candidate/result with checked add/multiply/align
3. prove every resulting value is inside the field-specific legal domain and
   is not a sentinel, reserved value, prior logical position, or reused identity
4. acquire any required durable reservation/publication authority
5. only then expose or persist the allocation/mutation
```

Failure at steps 1--4 publishes no candidate identity or mutation. Native signed
overflow, unsigned wrap, truncation to a persisted field, and increment-then-test
are forbidden. Reaching a legal maximum is not corruption; requesting its
unrepresentable successor is a deterministic exhaustion error.

The canonical advancing-domain inventory is:

| Domain | Width / interpretation | Reserved or zero rule | First value | Last value a writer may allocate/emit | Reuse | Exhaustion scope |
|---|---|---|---:|---:|---|---|
| `FileId` | uint32 | `0` invalid | `1` | `UINT32_MAX-1 = 4,294,967,294` | never | `FILE_ID_EXHAUSTED`; fail the file/object creation before semantic publication |
| `PageNo` identity | uint64 | `UINT64_MAX` invalid; page `0` is the superblock | `0`; ordinary pages begin `1` | representable ordinary domain ends at `UINT64_MAX-1`; the physical-I/O cap below makes `1,125,899,906,842,622` the last allocatable ordinary page | unpublished serialized tail number may be retried; published reuse only through the owning free/reclamation protocol | `PAGE_NUMBER_EXHAUSTED` for that file |
| `published_page_count` / physical page count | process-local uint64 exclusive bound reconstructed from aligned file length | `0` cannot describe an initialized managed file | `1` | `1,125,899,906,842,623` | may return from `N+1` to `N` only during proven unpublished §12.12 tail rollback | same per-file `PAGE_NUMBER_EXHAUSTED`/I/O boundary |
| `SlotId` | uint16 | `UINT16_MAX` invalid; `0` legal | `0` | representable `UINT16_MAX-1`; the heap-page geometry limits an allocated slot to `1017` | only `DEAD -> UNUSED` after §14.6 grace | page-local `NO_SPACE`, not global ID exhaustion |
| `TxnId` | uint64 | `0` invalid, `1` frozen | `2` | `18,446,744,073,708,503,041` under the exact fixed-block reservation rule below | never | `TXN_ID_EXHAUSTED`; reject new transaction admission |
| `CommandId` | uint32 | no sentinel; `0` legal | `0` | `UINT32_MAX = 4,294,967,295` | never within a transaction | no later ordinary statement in that transaction; COMMIT/ROLLBACK remain legal |
| `TableId` | uint64 shared catalog-object allocator | `0` invalid; `1..6` fixed built-ins | built-ins `1`; later allocations at least `7` | `UINT64_MAX-1 = 18,446,744,073,709,551,614` | never | `ID_EXHAUSTED`; fail DDL before semantic publication |
| `ColumnId` | uint32 table-local | `0` invalid | `1` | `UINT32_MAX` subject to the much smaller complete-schema/tuple encoding bounds | not reused within a table's schema history | reject the schema construction before catalog publication |
| `IndexId` | uint64 shared catalog-object allocator | `0` invalid; later allocations at least `7` | `7` | `UINT64_MAX-1` | never | `ID_EXHAUSTED`; fail DDL before semantic publication |
| `ConstraintId` | uint64 shared catalog-object allocator | `0` invalid; later allocations at least `7` | `7` | `UINT64_MAX-1` | never | `ID_EXHAUSTED`; fail DDL before semantic publication |
| `SchemaVer` | uint32 | `0` invalid | `1` | `UINT32_MAX` | never for one table | fail a schema-changing DDL before publication; the architecture exposes no such ALTER and emits only `1` |
| control-slot generation | uint64 | `0` invalid | `1` | `UINT64_MAX` | never | the next control update fails; operations requiring it cannot proceed |
| WAL record-start `Lsn` | uint64 global unsigned byte address | `0` invalid; starts are 8-byte aligned | `8` | `2^64-48 = 18,446,744,073,709,551,568`, only for a minimum 48-byte record fitting the final segment | never | `WAL_POSITION_EXHAUSTED` before reservation/publication |
| WAL segment index | mathematical unsigned index derived from LSN | none inside its admitted domain | `0` | `2^38-1 = 274,877,906,943` | never; retention may remove an old segment file but never reassign its index to a later position | `WAL_POSITION_EXHAUSTED` before creating another segment |
| statistics `chunk_count` / `chunk_index` | catalog INT32 carriers, validated as nonnegative integers | count `0` invalid; index zero-based | count `1`, index `0` | count `1,048,576`, index `1,048,575`; total payload still at most `UINT32_MAX` | not within one generation | invalidate/fail that ANALYZE generation and use normal statistics fallback |
| B+ `tree_height` / node `level` | uint16 | height `0` invalid; leaf level `0` legal | height `1`, level `0` | height `UINT16_MAX`; valid node level at most `UINT16_MAX-1` | contraction may lower height; no numeric wrap | fail root-growth MTR before provisional mutation/publication |

`RID` has no independent allocator or counter. It is the exact composition of
the owning heap `FileId`, `PageNo`, and `SlotId`; exhaustion and legal reuse are
therefore exactly the component rules above plus §14.6 read-epoch protection.
Built-in `TypeId` values are a fixed registry, not an advancing allocator.
`creation_epoch` is a known opaque uint64 value, not a monotonic identity in the defined architecture,
so every bit pattern is data rather than an exhaustion state.

#### 4.3.2.1 Durable ID allocation and consumed gaps

`next_file_id` and `next_catalog_object_id` are persisted next-value high-water
marks. Their allocator must durably store `candidate + 1` before returning
`candidate`; consequently the carrier maximum itself cannot be returned and the
last values are the `MAX-1` values in the table. A dropped or aborted object does
not return a FileId/TableId/IndexId/ConstraintId to its allocator. Once the
durable next value passed it, the ID is consumed even if only a private/orphan
namespace artifact governed by §4.7 ever used it.

TxnId reservation remains in exact blocks of `2^20 = 1,048,576`, starting at
`2`. The greatest representable exclusive reservation end reachable by that
sequence is:

```text
MAX_RESERVED_TXN_ID_END = 18,446,744,073,708,503,042
MAX_ALLOCATABLE_TXN_ID   = MAX_RESERVED_TXN_ID_END - 1
                         = 18,446,744,073,708,503,041
```

The remaining larger uint64 values are not a partial final block and are not
legal normal TxnIds. The architecture does not shrink the block to consume them. If the
next exact block cannot be added, transaction admission returns
`TXN_ID_EXHAUSTED`. Status reclamation/freezing never permits reuse. A crash may
permanently skip the unused suffix of an already durable block; restart advances
from the durable/recovered high-water authority and never hands a possibly
reserved ID to a different transaction.

The first six shared catalog-object allocations are the fixed built-in TableIds
defined in §16.5.
Thereafter the common allocator hands out one globally increasing sequence to
TableId, IndexId, and ConstraintId wrappers. `ColumnId` is separately table-local:
CREATE TABLE assigns `1..N` in declaration order and rejects an unencodable
complete schema before any catalog row publishes. The architecture has no schema-changing
ALTER, so `SchemaVer` is `1`; any future increment must use this same checked-next
rule and cannot wrap.

#### 4.3.2.2 CommandId terminal boundary

An admitted ordinary statement may use `CommandId{UINT32_MAX}` exactly once.
When that statement ends, the transaction records that no next CommandId exists;
it does not compute a wrapped value. A later ordinary statement is rejected with
`COMMAND_ID_EXHAUSTED` before parsing/binding/execution-owned publication begins.
The transaction remains semantically eligible to COMMIT its already successful
work or to ROLLBACK; exhaustion alone does not force `MUST_ABORT`. Transaction
control consumes no CommandId. All attempts of an already admitted automatic
pre-write retry continue to use its one assigned CommandId as §39.1 requires.

#### 4.3.2.3 Page number, page count, and physical file length

The architecture's Linux/POSIX page-file contract uses a nonnegative signed 64-bit positional
I/O/file-length domain. Define:

```text
MAX_FILE_BYTES      = INT64_MAX
MAX_FILE_PAGE_COUNT = floor(INT64_MAX / 8192)
                    = 1,125,899,906,842,623
MAX_FILE_PAGE_NO    = MAX_FILE_PAGE_COUNT - 1
                    = 1,125,899,906,842,622
```

The count includes page `0`. Before append-first extension, the owner computes
the candidate PageNo, `new_page_count`, page byte offset, and
`new_file_length = new_page_count * PAGE_SIZE` with checked arithmetic and
requires all four to fit these exact domains. Failure occurs before `ftruncate`,
`pwrite`, frame publication, or WAL reservation. A filesystem/quota limit below
this architecture maximum is `RESOURCE_FULL`/I/O failure, not numeric exhaustion.

FileSuperblocks contain no independently advancing persisted page-count
field. `published_page_count` is reconstructed from aligned physical length plus
the WAL publication/recovery reconciliation in §12.12, then maintained as the process-local
exclusive bound. It cannot outrun either authority.

A page number governed by §12.12 whose physical extension and publication never completed may
be retried only after exact serialized tail restoration proves it was never
published. A published PageNo is not implicitly reusable because a relation or
transaction later disappears; only an already-defined heap/B+ reclamation rule
may reuse it.

#### 4.3.2.4 WAL positions, lengths, and terminal headroom

The WAL address space is the mathematical half-open interval `[0, 2^64)`. LSN
`0` is the invalid sentinel, so a fresh database has eight canonical zero prefix
bytes and its initial valid append end/first possible record start is `8`.
Record starts are nonzero uint64 multiples of eight. An **exclusive WAL end
position** is a mathematical boundary in `8..2^64`; `2^64` is the legal terminal
one-past address but is not encodable in any `Lsn` field. Reaching it sets an
explicit runtime/recovery exhausted condition; it never wraps the append end to
zero. `page_lsn`, `rec_lsn`, checkpoint LSNs, transaction-chain LSNs, and
`durable_lsn` always store record starts, never exclusive ends. `durable_lsn >= X`
retains §12.13's complete-record meaning.

For every reservation, checked arithmetic simulates the exact header, payload,
`align_up(...,8)`, current-segment tail/PAD, target segment, and exclusive end.
The complete sequence must fit at or below mathematical `2^64` and its record
start must fit uint64. The final segment index is `2^38-1` because the segment
size is `2^26`; a greater index is forbidden even though the 16-hex-digit filename
grammar could spell it. Formatting never truncates or wraps an index.

The WAL header's uint32 `total_length` includes the 48-byte header and exact
payload but excludes external zero alignment. Its field domain is
`48..UINT32_MAX`; the stronger no-cross-segment rule makes the emitted bound:

```text
MAX_WAL_TOTAL_LENGTH = 67,108,864
MAX_WAL_PAYLOAD       = 67,108,816
```

`physical_span = align_up(total_length,8)` must also fit one segment. Every
builder computes exact size before narrowing; it never truncates uint32. An
oversized `BTREE_MTR` fails as one operation under §12.12 before publication and
restores any provisional state. The architecture does not split one atomic MTR into independent
WAL records to evade this bound.

Numeric WAL capacity uses a **terminal headroom credit**, distinct from the LSN
reservation in §12.12. Before a transaction may append its first user-chain/persistent-write
WAL record, it must acquire and retain enough unconsumed numeric address space for
the worst terminal closure under §39.1. The architecture reserves exactly `33,128` bytes per such
nonterminal transaction. From an arbitrary aligned append end, one 8,264-byte
status `PAGE_INIT`/`PAGE_IMAGE` can advance at most `16,520` bytes including a
segment tail, and one 48-byte terminal record can advance at most `88` bytes.
The credit is therefore `2 * 16,520 + 88`: two independently positioned images
plus one `TXN_COMMIT`/`TXN_ABORT`. The second image covers a pre-authorizing
COMMIT attempt that appended its preparatory image but then must transition to
ABORT; the independent-position bound remains safe if unrelated WAL interleaves.
A read-only transaction with no persistent WAL needs no credit and elides
terminal WAL under §39.1.5.

After a terminal attempt's preparatory image validly appends, an immediate retry
under the retained status-page/WAL publication ownership in §§12.10.5, 12.12,
and 13.13.2 reuses that image/record position as the
physical base and retries the terminal append; it does not append an unbounded
series of redundant images. The one allowed COMMIT-failure-to-ABORT transition
may reevaluate and append the second accounted image. Inability to retain/reuse
the base or complete that bounded closure is the existing noncontinuable abort/
publication failure, not permission to consume further unreserved WAL.

Headroom is aggregate capacity accounting only: it assigns no LSN, advances no
valid end, and creates no hole. Every ordinary/user/system append admission must
prove that its complete span leaves all outstanding credits satisfiable. Actual
terminal/preparatory appends consume the owning credit; unused remainder is
released only when the publication-authorizing terminal record has validly
appended or a no-WAL read-only transaction terminates. Recovery reconstructs
loser closure obligations from the valid WAL prefix; the unused address space
guaranteed before crash remains available for their required status image and
`TXN_ABORT` sequence.

A credit acquired speculatively for a transaction's first persistent append may
be released after a known no-append exact rollback under §12.12 only if the transaction
still has no valid user-chain/persistent WAL record. Once any such record validly
appends, the credit remains through terminal closure. Append uncertainty follows
the noncontinuable rule and never releases capacity on a guess.

Consequently numeric inability to append COMMIT or ABORT for a transaction that
already owns credit is an internal accounting invariant failure, not an expected
boundary result, and enters the existing noncontinuable path. Near exhaustion:

- a transaction whose credit is intact may COMMIT or ABORT normally;
- a transaction without persistent WAL may still commit/abort without terminal WAL;
- a first WAL-backed mutation that cannot acquire credit fails before publication;
- later ordinary WAL-backed work may fail while the transaction retains its credit
  and can still terminate;
- a checkpoint or maintenance record may use only capacity not protecting terminal
  closure and fails before its BEGIN/publication if it does not fit;
- WAL recycling/disk-space cleanup cannot create a later numeric LSN.

The credit guarantees numeric address space, not memory, disk blocks, segment
namespace durability, or successful I/O. Those independent failures retain
the behavior in §§12.12 and 39.1. A final/recovery checkpoint can itself report
`WAL_POSITION_EXHAUSTED`; it cannot cure numeric exhaustion. Controlled close
therefore cannot report success if its required checkpoint under Chapter 3 cannot be
encoded, while already durable transaction outcomes remain unchanged.

#### 4.3.2.5 Other advancing counts, lengths, and runtime tokens

Every persisted builder computes exact canonical size/count before narrowing:

| Persisted producer field | Width / legal zero | First/last emitted value | Exhaustion/capacity outcome |
|---|---|---|---|
| heap `slot_count` | uint16; zero legal | `0..1018` | page `NO_SPACE`; split work to another/new page, never truncate |
| heap slot `tuple_length` | uint16; zero only for canonical reclaimed DEAD/UNUSED | schema-valid tuple through `8135` bytes | `ROW_TOO_LARGE` before tuple/page publication |
| FSM `entry_count` | uint16; zero legal | `0..8144` | allocate the next representable FSM page or fail the owning PageNo/file boundary |
| B+ node `slot_count` | uint16; zero legal | slot-only ceiling `1016`; populated capacity also includes every entry under §§8.8–8.10 | split/rebalance or fail before page/MTR publication |
| B+ `user_key_length` / `entry_length` | uint16; zero-length key invalid | key `1..1024`; leaf entry at most `1040`, internal entry at most `1048` | key-too-large/record construction failure; no overflow key in the defined architecture |
| WAL `total_length` / `payload_length` | uint32; total zero invalid, payload zero legal | total `48..67,108,864`; payload `0..67,108,816` | `WAL_RECORD_TOO_LARGE`/`ENCODED_LENGTH_EXCEEDED` before reservation |
| default-blob `total_length` | uint32; zero invalid | canonical blob through `4096` bytes | reject unsupported oversized default before catalog publication |
| `PersistedScalarV1.payload_length` | uint32; zero legal by type/NULL rules | exact `16 + payload_length`, then Align8, fitting the owning default/statistics envelope | enclosing builder fails; scalar length is never independently truncated |
| statistics-scope `total_length` | uint32; zero invalid by every scope grammar | exact grammar length through `UINT32_MAX`; chunk count/index additionally obey the main domain table | invalidate/fail that generation; use statistics fallback |
| statistics manifest/count fields | uint32 logical counts; zero field-specific | exact arrays/counts whose multiply/add and complete scope remain at most `UINT32_MAX` | invalidate/fail that generation before rows publish |
| checkpoint DATA `chunk_index` and END `data_record_count` | uint32; zero legal | zero DATA records or at most `UINT32_MAX` DATA records indexed `0..UINT32_MAX-1` | checkpoint remains uninstalled; inert earlier checkpoint WAL may remain |
| checkpoint DPT/writer counts and totals | uint32; zero legal | `0..UINT32_MAX`, with every DATA record also inside the WAL-record bound | checkpoint remains uninstalled; no count truncation |

The BufferPool `modification_generation`, copied-writeback token, checkpoint/FPI
epoch token, and B+ `root_generation` are process-local correctness identities,
not persisted fields. Their concrete width is not fixed, but they MUST NOT repeat
while a stale observer/completion could compare equal. A finite implementation
either:

```text
quiesces the complete owning frame/tree/checkpoint domain,
proves no old token or in-progress completion remains,
and atomically reseeds every compared token
```

or rejects the mutation/root publication/checkpoint before changing bytes. It may
not wrap. Frame identity plus generation remains nonrepeating across reassignment.
Pin/reference counters likewise use checked increment and fail the acquisition
without changing the count.

`ReadEpochManager.current_epoch` retains its exact uint64 rule: value `0` is
invalid and `UINT64_MAX` may be the current epoch, but no retirement is assigned
at that value and no increment is performed from it. Further RID retirement/reuse is disabled until a process
restart/quiescent manager reinitialization proves no old readers survive; normal
operations not requiring physical RID reuse may continue. Recovered DEAD slots
are re-enqueued under the new process epoch as §14.6.4 requires.

#### 4.3.2.6 Failure, lifecycle, crash, and verification rules

Numeric exhaustion is not corruption and is distinct from `RESOURCE_FULL`:

```text
NUMERIC_EXHAUSTED:
    no representable legal successor exists; deleting ordinary data does not
    help unless an owning protocol already permits reuse

RESOURCE_FULL:
    a representable operation exists but disk/quota/memory/platform allocation
    failed; retry may become possible after operational remediation
```

The conceptual deterministic boundary errors are `ID_EXHAUSTED`,
`TXN_ID_EXHAUSTED`, `COMMAND_ID_EXHAUSTED`, `PAGE_NUMBER_EXHAUSTED`,
`WAL_POSITION_EXHAUSTED`, and `ENCODED_LENGTH_EXCEEDED`. Exact public enum names
may differ, but diagnostics must preserve the exhausted domain and must not report
these outcomes as `CORRUPT_*` or generic disk-full.

§39.1 applies after the lower layer returns exhaustion. Before a statement's first
published persistent write, a local exhaustion may be a statement-only failure.
After earlier statement writes, the transaction follows `MUST_ABORT`. TxnId
exhaustion gates new transactions database-wide; CommandId exhaustion gates one
transaction's later statements; PageNo exhaustion gates append in one file; WAL
position exhaustion gates new WAL-backed mutation while preserving credited
terminal closure. None alone changes an already durable COMMIT or makes existing
maximum-valued bytes corrupt.

An already-READY owner may continue operations that do not need the exhausted
domain: for example reads after WAL/PageNo exhaustion and reuse from a legal page
free list. WAL-position exhaustion alone still permits a newly admitted
transaction to remain read-only and use the no-terminal-WAL path; its first
WAL-backed mutation fails before publication if no closure credit is available.
TxnId exhaustion still prevents a new transaction because the architecture has no anonymous
transaction identity. A fresh open under Chapter 3 performs normal validation and
recovery; if mandatory recovery/checkpoint WAL cannot fit, it returns the
exhaustion/open failure and does not claim READY. The architecture adds no separate
read-only open mode through this rule.

For persisted allocators, crash before durable consumption/publication may leave
an unused candidate only where the owning reservation protocol explicitly allows
it. Crash after durable high-water advance or semantic publication never permits
reuse. Recovery reconstructs FileId/catalog-object/TxnId high-water authority from
control/checkpoint/WAL, PageNo publication from reconciled file/WAL ownership, and
the WAL append end from the complete valid prefix. Consumed orphan/gap IDs remain
consumed; reaching the last legal value and crashing makes the next request fail,
not restart from an earlier value.

Implementations MUST forbid at least:

1. TxnId/CommandId/object-ID/PageNo/LSN wrap into zero, a sentinel, or prior value.
2. TxnId reuse after status reclamation or reuse of a consumed aborted-DDL ID.
3. extending a file before a known PageNo/page-count/file-offset overflow.
4. truncating WAL or other uint32 lengths, or overflowing aligned WAL end arithmetic.
5. formatting a wrapped/truncated WAL segment name.
6. splitting an atomic BTREE_MTR only because its record was sized too late.
7. consuming address space reserved for an existing writer's terminal closure.
8. allowing a wrapped modification/root/FPI/read-epoch token to equal a stale token.
9. truncating statistics chunk count/index or publishing an unrepresentable B+ level.

Exhaustion behavior MUST remain deterministic at every boundary. Consumed
high-water and gap state MUST survive restart, maximum-size record/page
construction MUST use checked arithmetic, and unrepresentable candidates MUST be
rejected before publication. Detailed verification and fault-injection procedures
for these requirements belong in `VERIFICATION.md`.

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

For ordinary heap-version UPDATE behavior:

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

### 4.6.3 Excluded HOT-like update optimization

HOT-like updates would avoid some new index entries when indexed columns are unchanged.
This optimization is outside the defined storage contract; introducing it requires an
architecture revision.

## 4.7 Random-access file model and file kinds

Major persistent objects use separate files rather than one monolithic database file.

The storage manager recognizes explicit random-access file kinds.

The persisted `file_kind` field is an unsigned 16-bit little-endian integer with these codes:

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

Code `0` MUST be rejected when decoding a random-access file superblock.

Under §4.14.4, codes `1..5` are the complete v1 registry. Any other code in a superblock claiming format version 1 is `CORRUPT_FILE`; no unknown FileKind is opened through a known codec. A known kind that disagrees with the expected managed basename/catalog descriptor is likewise corruption. `BTREE` dispatches only to its specialized codec.

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

That directory is the database's persistent namespace root. The architecture uses this exact managed layout:

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

Database-root lifecycle uses two reserved same-parent private sibling grammars
for final basename `D`:

```text
D.dblusblus-creating
D.dblusblus-removing-<token>
```

The first is §4.7.8's private creation staging root. The second is §4.7.9's
removal-retired root, where `<token>` is exactly 32 lowercase hexadecimal digits
encoding a collision-resistant 128-bit token. A token collision is resolved by
choosing another token; neither private grammar is an ordinary `OpenDatabase`
target, and a caller-selected final basename MUST NOT itself match either
reserved private grammar. Exact recognized private siblings are distinct from
unrelated/unknown external-parent entries, which are never deleted by loose
prefix or suffix matching.

While a database is open, the storage namespace owner keeps open directory descriptors for:

```text
database_root
database_root/pending
database_root/wal
```

and resolves managed entries relative to those descriptors. The external parent
directory descriptor is retained while creating/publishing a root, independently
adopting a final root under §3.3.2, or retiring/reclaiming a root under §4.7.9.
Root lifecycle operations resolve and revalidate the final/private entries
relative to that retained parent descriptor, within §4.7.10's cooperative
managed-namespace concurrency boundary.

`database.control` also supplies the process-associated advisory lock object in
§3.3.2. Lock ownership is operating-system state, not control-file contents or
filename existence; this adds no control-slot field or persistent-format value.

Opening an existing database uses no-follow directory-relative lookup for managed
names. `database_root`, `pending/`, and `wal/` must be directories;
control/object/segment names must be regular files. A managed-name symlink or
wrong filesystem object type is an open/corruption error, not an alternate path
to follow. Whole-root reclamation remains bound to its retained retired-root
descriptor and MUST NOT follow symlinks, traverse an unproven mount/filesystem
boundary, or touch external-parent siblings.

All three managed directories and the database root's external parent/destination are on one filesystem for operations that use rename. The architecture does not support cross-filesystem publication fallback.

### 4.7.2 Required Linux/POSIX durability semantics

The architecture requires a Linux filesystem/configuration with all of these semantics:

- `openat`/equivalent create with exclusive, no-follow behavior creates one direct managed directory entry without following a symlink,
- successful complete `pwrite` changes regular-file bytes but does not itself make them durable,
- successful `fdatasync(regular_file)` makes prior file data and retrieval-critical metadata such as file length durable,
- successful `fsync(directory_fd)` makes prior completed create, rename, and unlink namespace mutations in that directory durable,
- same-filesystem rename is atomic for runtime name lookup but is not crash-durable until every affected directory has been synchronized,
- unlink removes a runtime name but is not crash-durable until its parent directory has been synchronized,
- `close` releases a descriptor and is not a durability primitive,
- synchronizing a regular file does not substitute for synchronizing its parent directory.

If the selected filesystem does not support reliable directory `fsync` with these semantics, it is not a supported durable database filesystem. The engine MUST propagate directory-open/sync and regular-file sync failures rather than assuming persistence.

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

The architecture uses Linux `renameat2(..., RENAME_NOREPLACE)` or an equivalent same-filesystem no-clobber primitive. It MUST NOT overwrite an existing managed final name. An unexpected destination collision is an I/O/invariant/corruption error, not permission to replace another file.

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

A namespace-durability failure is an I/O/durability failure and MUST NOT be reported as successful publication. Its diagnostic retains the affected directory/path, namespace operation, and underlying error. Its statement/transaction behavior is §39.1.3–§39.1.5: a pre-catalog survivor that has deterministic orphan ownership is not a transaction-owned published write, while durable commit remains irreversible. Every CREATE namespace prerequisite completes before terminal COMMIT begins.

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

Exact `D.dblusblus-removing-<32-lowercase-hex>` siblings are instead
whole-root retirement artifacts owned by §4.7.9. They are never opened as
databases. Cleanup of such an artifact is not ordinary orphan cleanup: only
after supported-v1 root identity, exclusive root ownership, and durable
retirement have been established does §4.7.9 authorize deletion of every entry
contained beneath the retained retired root, including names unknown to ordinary
object classification. This authority never extends to an unrelated
external-parent sibling or through a followed link/mount boundary.
Section 4.7.9 separately permits empty-root finalization when control is absent;
that terminal exception grants no authority to recursively delete contents.

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
1. open the external parent directory, acquire the per-target lifecycle
   namespace gate under §4.7.10, then create the staging directory
   with exclusive no-follow semantics
2. open the staging root and create its pending/ and wal/ directories
3. create/initialize database.control, catalog.dat, txn_status.dat,
   the heap and FSM files for all six system relations, any other required built-in
   object files, and initial WAL segment 0
4. fdatasync every startup-critical regular file after its complete
   initialized contents/length are written
5. fsync staging/pending, staging/wal, and the staging root directory
6. validate the complete bootstrap using the normal open-time identity,
   checksum, and cross-reference rules
7. atomically establish §3.3.2's process-local root/control-inode claim
   and acquire the ordinary nonblocking exclusive database.control lock
8. revalidate that final D is absent, fsync the external parent to
   establish/reconcile any prior final-name absence, and revalidate D
   before publication
9. rename the staging directory to final basename D using no-replace semantics
10. fsync the external parent directory
11. only then publish durable database creation
```

The staging root as a whole is private; startup-critical files inside it use their final basenames and are not ordinary transaction-owned DDL files. The initial WAL segment follows §12.2.1 before step 5.

The creator retains the lifecycle namespace gate through bootstrap and durable
publication, or through its failed-creation cleanup/handoff. In particular, staging
cannot exist during a compliant creator's pre-control interval without that gate
being held. After publication the gate may be released while a returned handle
continues to hold the canonical control ownership described below.

A same-filesystem root rename preserves the root/control inodes, retained
descriptors, process claim, and control lock. The creator retains that ownership
through step 10. After step 9 a final-path opener reaches the same locked control
inode and returns `DATABASE_BUSY` before control-slot interpretation, WAL
inventory, recovery, or mutation. The lock is the ownership/admission gate;
step 10's external-parent `fsync`, not the lock, is the durable root-publication
point.

If creation returns an opened handle, the creator continues the same claim,
control descriptor, and lock without release/reacquire through §3.3.7's normal
recovery and READY gates; the handle is not published and admits or acknowledges
no ordinary work before READY. If creation returns only a durable-create result,
it releases/closes the control lock while retaining the process claim, removes
that claim, and only then returns success after step 10.

If step 9 succeeds but step 10 fails, creation reports no success, publishes no
READY handle, and retains publication ownership while retrying/reconciling. The
failure does not establish whether rename durability succeeded. If the creator
has begun constructing a DatabaseInstance, §3.3.4's failed-open versus
noncontinuable cleanup rules apply without admitting ordinary work. If the
creator dies, a later independent owner applies §3.3.2's adoption barrier before
inspection, recovery, removal, READY, or acknowledged work.

A crash before step 9 leaves no final database root and may leave only a
recognized staging-root orphan. A crash after rename but before step 10 is an
unacknowledged creation: the live creator's lock excludes competitors, and after
restart the final or staging name may survive; a surviving final root is usable
only after independent adoption and complete bootstrap validation, while a
staging root is never opened as a database. A crash after step 10 leaves a
durably named, fully initialized database. Explicit engine create/maintenance
logic MUST acquire the same §4.7.10 gate before classifying staging as abandoned
or deleting it. Exclusive gate ownership proves that no compliant creator remains
in the pre-control interval; age, PID values, elapsed time, staging contents, and
absence of a control lock do not supply that proof. If a control entry is present
and safely openable as a regular file, cleanup MUST reserve the process-local root
identity before opening it and acquire the ordinary control ownership before
interpreting its bytes, including bytes that prove partial or damaged. Control
validity is not established by an unowned preliminary read or duplicate-descriptor
open/close. Valid control additionally requires the ordinary destructive-identity
checks. Missing or unusable control does not
remove the gate requirement: the gate supplies the missing pre-control liveness
proof, and cleanup remains confined to the exact retained staging root under
§4.7.1's no-follow and containment rules. Recognizable unsupported control formats
retain §4.14.6's refusal outcome. Staging removal is synchronized in the external
parent before cleanup is called complete.

Opening an existing database never treats `D.dblusblus-creating` as an alias for `D`.

### 4.7.9 Whole-database retirement and removal

Whole-database removal is an offline root-lifecycle operation, not SQL DDL or an
ordinary database open. It begins only when no ordinary DatabaseInstance,
transaction, BufferPool/database manager, worker, or background producer exists.
The remover becomes the sole offline lifecycle owner without entering READY or
constructing ordinary runtime services.

Given final basename `D`, the canonical protocol is:

```text
1. retain the external parent, acquire D's lifecycle namespace gate under
   §4.7.10, and safely open exact final root D with descriptor-relative,
   no-follow, directory-type checks
2. atomically reserve the process-local identity of the retained root before
   opening database.control; an existing same-process owner returns
   DATABASE_BUSY without opening or closing another control descriptor
3. under that reservation, safely open D/database.control read/write as a
   no-follow regular file and complete the stable root/control-inode claim
4. acquire §3.3.2's ordinary nonblocking exclusive control lock;
   contention returns DATABASE_BUSY before destructive inspection/mutation
5. perform independent root adoption: verify parent entry D identifies the
   retained root, fsync the external parent, then revalidate/reconcile identity
6. establish supported-v1 destructive database-root identity as defined below
7. choose an absent same-parent reserved name
   D.dblusblus-removing-<token>; retry token selection on collision
8. rename D to that private sibling with same-filesystem no-replace semantics
9. delete no internal content before retirement publication
10. fsync the external parent
11. only after step 10 publish semantic whole-database removal
12. optionally reclaim the private retired tree under the contained U1 rules
13. if reclamation completes, rmdir the empty retired root and fsync the
    external parent before claiming physical residue absence
14. release the old control lock/descriptor while retaining the old process
    claim, then remove that claim after all owned use has stopped
```

Step 5 precedes control interpretation beyond minimum safe adoption identity and
all destructive mutation. Removal does not select a recovery control generation,
inventory WAL, run recovery, reconstruct transaction status/catalog state,
construct BufferPool/runtime services, or publish READY. Logical consistency is
not required when the exclusively owned supported-v1 root is destroyed as one
unit.

Steps 2–3 follow §3.3.3's claim-before-control-open ordering. A failed same-process
removal attempt MUST preserve the existing owner's control descriptor and POSIX
lock. It cannot open a second descriptor, discover the local owner, and close that
descriptor as error cleanup: §3.3.2's process-associated lock would be released by
that close. Failure of the first owner's control open releases its provisional
root reservation without exposing a second owner.

Destructive identity first applies §4.14.6's control-family/version dispatch to
both complete 4096-byte slots. Any exact `DBLUSCTL` slot claiming
`format_version > 1` returns `UNSUPPORTED_DATABASE_FORMAT` and forbids removal.
Otherwise supported-v1 identity requires at least one readable complete slot
whose bytes `0..7` are exact ASCII `DBLUSCTL`, whose little-endian bytes `8..9`
are `format_version = 1`, and whose little-endian bytes `10..11` are
`header_size = 88`. That slot's CRC, generation, range, flags/reserved, or
cross-field validation may fail: such corruption prevents normal open but does
not erase the positively identified supported-v1 family or prohibit explicit
whole-root destruction. Missing, unreadable, symlink/wrong-type control, absence
of this exact framing, or an unrelated root returns the existing missing/
`NOT_A_DATABASE`/I/O result without destructive mutation. The architecture supplies no
force-removal path for unrecognizable or future-format roots. The empty no-control
retired-root finalization defined below is a separate terminal operation, not
permission to remove an unrecognizable live root or a nonempty retired tree.

The durable whole-database removal publication point is step 8's retirement
rename plus step 10's successful external-parent `fsync`. That one parent sync
durably establishes both absence of captured old `D` and presence of the exact
private retired root. Until it succeeds, no removal success is reported and no
internal content is deleted. Once it succeeds, the captured old database is
semantically removed even if private physical reclamation remains pending; a
later cleanup failure never renames it back, resurrects it, or changes that
success.

After durable retirement, explicit whole-root cleanup has U1 authority to delete
every entry contained beneath the retained supported-v1 retired root, including
managed files and names unknown to ordinary orphan classification. Traversal is
descriptor-relative, no-follow, bottom-up where required, and never follows a
symlink, crosses an unproven mount/filesystem boundary, or touches an external-
parent sibling. A contained symlink is unlinked as an entry; a contained special
entry is unlinked only where the platform contract safely permits; an unsafe
mount/boundary leaves reclamation incomplete. WAL, pending, catalog, status, and
system files are reclamation content and need no recovery/catalog ownership
classification. Ordinary §4.7.6 orphan cleanup remains unable to delete unknown
names.

Recursive reclamation retains `database.control` as its destructive-identity and
exclusive-ownership authority. Its terminal ordering is mandatory:

1. Under the lifecycle namespace gate and old-root control ownership, remove all
   other entries belonging to this reclamation, including nested directories,
   and complete the required directory durability operations under §4.7.2.
   Synchronization may be batched only if it still establishes durable absence
   of every other entry before control unlink.
2. Immediately before control unlink, validate the retained retired root as
   containing only the expected `database.control` entry, bound to the owned
   control inode. A changed entry or additional contents prevent this transition.
3. Unlink that control entry and successfully `fsync` the retained retired-root
   directory. Keep the existing control descriptor/claim through their last owned
   use; unlink does not justify opening a replacement control file or prematurely
   closing a still-used owner descriptor.
4. Remove the empty retired root and `fsync` its external parent before claiming
   durable physical residue absence. A failed barrier leaves cleanup incomplete;
   it never reverses semantic removal.

A conforming cleanup can therefore reach a no-control state only as an empty
terminal residue. A restarted cleaner first acquires §4.7.10's gate for the original
target `D`, retains the external parent, and opens/revalidates the exact retired
entry with the same no-follow, type, filesystem, and containment checks. With
control present it must reserve the local root identity before opening control,
acquire control ownership, establish supported destructive identity, and synchronize
the external parent to establish/reconcile retirement before recursive mutation.
The same-process duplicate-descriptor restriction applies to this path as well.

With control absent, a restarted cleaner MAY finalize only an empty retired root.
It checks emptiness under the gate, synchronizes the retained root to cover the
possible interrupted control unlink, revalidates the exact parent-entry identity,
then attempts empty-directory removal and synchronizes the external parent.
Removal must refuse nonempty directories, including contents externally recreated
after the check; this path never falls back to recursive traversal. Observed path
rebinding or changed identity requires refusal of removal against that identity;
it never authorizes following a replacement tree. Reconciliation must establish
the required identity and authority before a new removal attempt. The supported
concurrency boundary is §4.7.10. An already absent exact retired entry is an idempotent outcome
with respect to another successful finalizer; synchronize/reconcile the parent
before reporting durable absence. Compliant finalizers serialize on the same gate.

A nonempty retired candidate without a control file satisfying the destructive-
identity rules MUST NOT be recursively reclaimed automatically. Its reserved
basename is not destructive authority. External mutation or loss/corruption of
that authority requires independent administrative or otherwise architecture-defined
authority; the engine does not broaden this terminal exception. No additional
durable cleanup-authority record is introduced.

The remover may retain gate and old-root control ownership through immediate
cleanup. If cleanup is deferred after step 11, it releases old control ownership
and the gate after owned use stops. A later invocation acquires gate before
control, never in reverse, and repeats the appropriate control-present or empty
terminal checks. No background cleanup service is promised.

Removal crash/failure outcomes are:

- before step 8, complete live `D` remains and no removal success exists;
- after step 8 but before step 10, publication is uncertain, no content is
  deleted, ownership is retained while possible for retry/reconciliation, and a
  crash may leave live `D` or the recognized retired root;
- after step 10, old `D` is durably absent and the retired root is durable;
- during recursive private cleanup before the terminal durability barrier,
  unsynchronized deletions may reappear, but control remains available to authorize
  retry; after that barrier no other reclamation entry may reappear;
- after control unlink, the control entry may survive or be absent until its
  directory sync completes; absence leaves only an empty terminal residue, and
  restart uses the corresponding authority path above;
- after retired-root `rmdir` but before its parent sync, the retired name may
  reappear without changing semantic removal; after that sync physical residue
  absence is durable.

`OpenDatabase` never targets the retired grammar. Once step 10 completes, a new
database may be created at `D` even while old retired residue remains: it has a
new root/control inode ownership domain, and database-local FileId values do not
collide with those in the retired root. Cleanup remains bound to the old retained
root and MUST NOT resolve through or touch the new `D`. A creator that observes
runtime absence while a prior retirement may be unsynchronized uses §4.7.8 step
8 to establish/reconcile old-name absence before publishing a new `D`. A fresh
removal invocation that cannot capture an identifiable live `D` returns the
existing missing/non-database result; a recognized retired artifact may still
be handled by explicit cleanup.

### 4.7.10 Lifecycle namespace coordination

Creation, stale-staging cleanup, whole-database retirement, and retired-root
finalization share an exclusive interprocess lifecycle namespace gate for the
target database namespace. Its stable key is the external parent-directory
identity plus final database basename `D`, covering `D`, its exact staging name,
and its retired-root names. Parent path aliases MUST resolve to the same gate;
unrelated targets need not serialize. Process-local coordination composes with
interprocess exclusion so same-process competitors cannot both own this gate.

The supported environment has a cooperative managed namespace. Administrative
tools and other external actors MUST refrain from renaming or replacing managed
entries during database lifecycle operations, or participate in the applicable
lifecycle coordination. This prerequisite covers root adoption, staging,
publication, retirement, and reclamation, including the root and control entries
whose identity those protocols retain. Arbitrary concurrent namespace rebinding
by actors that ignore this coordination is outside the supported concurrency
model. Retained descriptors and identity checks do not make a later name-based
mutation atomic with the preceding identity check, and the gate does not enforce
exclusion against nonparticipants.

Observed identity changes still require refusal of the affected operation until
its identity and authority can be reconciled. This environment prerequisite does
not relax containment, no-follow checks, control ownership, or the prohibition on
automatically reclaiming a nonempty retired root without destructive identity.
Empty-terminal finalization must refuse newly nonempty residue and must never
fall back to recursive deletion. External tools must preserve these boundaries
when arranging administrative maintenance.

The ownership order is:

```text
lifecycle namespace gate
    before
database.control owner lock
```

No path may acquire those domains in reverse order. Ordinary open uses the
control-ownership protocol without acquiring the namespace gate; an existing
database owner cannot acquire the gate while retaining its control lock to become
an offline remover. Deferred cleanup releases the old control ownership before a
later invocation acquires the gate and then, when control exists, the control
ownership again. Offline-removal gate contention preserves the nonwaiting
`DATABASE_BUSY` outcome; no failed acquisition authorizes inspection or mutation.

The gate is acquired before any staging basename can be created or published.
Namespace-mutating lifecycle operations retain it through their required directory
durability barriers or an explicit incomplete-operation handoff. On retry, retained
parent/root descriptors and exact entry identity are revalidated under the gate;
the basename alone does not authorize destructive traversal. The gate supplies
namespace exclusion and the pre-control creator-liveness proof, not supported
database-format identity or a substitute for control ownership/publication rules.

The operating-system representation is an implementation choice, provided it
supplies cross-process exclusion, crash release, stable per-target identity,
alias-safe scope, and this acquisition order. Backing-object lifetime must not
allow unlink/recreation to split one target into simultaneously owned gates.
Neither backing-object existence nor PID/age heuristics denote ownership. No
additional durable cleanup-authority record is introduced.

## 4.8 Common page header

Every persistent random-access page begins with a 32-byte common logical page header. WAL records use their separate log-record format.

The common page-header layout is:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 2 | `page_type` |
| `2` | 2 | `format_version` |
| `4` | 4 | `flags = 0` |
| `8` | 8 | `page_lsn` |
| `16` | 4 | `checksum_crc32c` |
| `20` | 2 | `header_size` |
| `22` | 2 | `reserved16` |
| `24` | 8 | `page_no` |
|  | **32** | **total** |

All multi-byte fields are little-endian.

The meaning of `format_version`, `header_size`, `flags`, and any reserved-field restrictions is completed by the specific page format. A page-format decoder MUST apply both the common-header rules and the rules of the requested page type.

For an ordinary page type whose owning format assigns no common-header flag bits:

```text
flags = 0
```

The encoder/initializer MUST write zero and the v1 decoder MUST reject nonzero common-header flags.

Future ordinary-page flag meanings require an explicit compatible format rule; unassigned bits are not silently preserved as unknown semantics.

Every current ordinary page type supports `format_version=1` only. After identifying the expected page family, version zero is corruption and a positive greater version is `UNSUPPORTED_PAGE_FORMAT`; §4.14 forbids applying v1 checksum/layout semantics beyond that discriminator.

### 4.8.1 Embedded page number

Although the physical file offset already implies the page number, `page_no` is stored inside the page so the database can detect conditions such as:

- misdirected writes,
- incorrect buffer mappings,
- file/page mixups,
- corrupted persistent metadata.

### 4.8.2 Page LSN

`page_lsn` is the LSN of the newest WAL-protected modification reflected in the page.

The sentinel for absence of a WAL position is:

```text
INVALID_LSN
```

Under §4.14's field-specific sentinel rule, it is permitted only where the
owning page contract allows it, such as the immutable catalog bootstrap page
in §16.9.2. WAL-backed page publication follows §§4.11.1 and 12.12;
Chapters 12–13 define advancement and recovery interpretation of `page_lsn`.

## 4.9 Persisted page-type registry

The common-header `page_type` field is an unsigned 16-bit little-endian integer.

The persisted codes are:

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

Codes `0..7` are the complete v1 registry. In a page claiming format version 1, an unlisted code or a listed code incompatible with the owning FileKind/page position is `CORRUPT_PAGE`. An implementation cannot preserve or traverse an unknown PageType as opaque state.

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

The superblock constants shared by every random-access file are:

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

| Offset | Size | Field / required meaning |
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

For `HEAP`, `FSM`, `CATALOG`, and `TXN_STATUS` superblocks using format version 1:

```text
header_size = 72
bytes 72..8191 = 0
```

For a `BTREE` superblock:

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

`creation_epoch` is an opaque persisted 64-bit value in the defined architecture.

Its generation procedure or time unit MAY be standardized later only while the field remains semantically opaque to correctness. Assigning interpretation that changes identity, visibility, recovery, or durability requires a new owning format/version under §4.14; this field is not generic extension storage.

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

Family/version dispatch precedes version-owned checksum/layout validation as §4.14.2 requires. Once `format_version=1` is selected, a v1 superblock decoder MUST reject:

- input shorter than 8192 bytes,
- CRC32C mismatch,
- `page_type != SUPERBLOCK`,
- `format_version=0` as corruption or `format_version>1` as unsupported under §4.14,
- a `header_size` inconsistent with the decoded `file_kind`,
- `page_no != 0`,
- magic different from `DBLUSBLS`,
- invalid or unknown `file_kind`,
- `page_size != 8192`,
- any nonzero reserved field required to be zero by the base or file-kind-specific format,
- any nonzero byte in the file-kind-specific trailing reserved region.

The architecture assigns no FileSuperblock flag bits. `flags` is RESERVED_ZERO: writers write zero and readers reject every nonzero value. No unknown superblock bit is preserved or round-tripped.

A codec MAY accept an input buffer larger than one page, but only the leading 8192 bytes participate in the superblock representation.

Because the architecture requires every reserved superblock field and every byte after the file-kind-specific header to be zero, assigning future meaning to those bytes requires an explicit compatible file-kind format revision.

The superblock MUST be encoded and decoded field-by-field. Raw serialization of a C++ superblock object is prohibited.

## 4.11 Append-first page allocation

Physical allocation of new pages is append-first.

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

Raw append is a lower-layer storage mechanism. A newly extended ordinary database
page is not database-visible merely because the file grew; publication requires
the following protocol.

Before any physical extension, allocation applies §4.3.2.3's exact PageNo,
page-count, signed-64-bit offset, and file-length checks. §12.12 tail rollback is
not permission to begin an extension whose candidate is already exhausted.

### 4.11.1 WAL-mode page publication

Each managed append-only page file has a process-local **append-publication lock** and a process-local:

```text
published_page_count
```

which is the exclusive upper PageNo bound visible to relation/index scans and ordinary allocation users.

For a new non-B+ ordinary page, publication is:

```text
1. acquire the file's append-publication lock
2. require physical_file_page_count == published_page_count and reserve
   page_no = that common count N
3. install the sole private new-page intent, reserve/bind its LOADING frame,
   and acquire the frame/DPT-transition ownership required by §7.12.4
4. extend the file from N to N+1 pages
5. create and validate the initialized logical page bytes except final
   page_lsn/checksum
6. obtain a candidate PAGE_INIT LSN reservation under §12.12.1
7. finalize the private canonical image with page_lsn = PAGE_INIT.lsn and
   valid checksum, construct/validate the complete record, and validly append
   it under §12.12.2
8. only after append success, install the canonical initialized image and
   atomically publish its page_lsn, first modification generation, dirty=true,
   rec_lsn=PAGE_INIT.lsn, DPT membership, and FPI-epoch state
9. as the same publication event with respect to scans/fetches:
       advance published_page_count = page_no + 1
       publish that frame RESIDENT with the creator's pin
10. release the append-publication lock
```

The next append to that same file MUST NOT reserve a later PageNo until the earlier publication has either completed step 9 or completed the failure cleanup below. A single B+ MTR may reserve/extend multiple consecutive trailing PageNos as one append-publication unit while holding the same lock; no competing allocator may interleave a later PageNo.

The PAGE_INIT record does not need to be durable merely to release the append-publication lock. WAL append order is enough: if a later page's initialization WAL becomes durable, every earlier WAL byte in that logical prefix is durable as well.

A new page MUST NOT become reachable from persistent relation/index metadata before its initializing WAL record has been completely appended.

For B+ pages allocated by file extension, the equivalent publication record is the complete `BTREE_MTR` that contains the new page as a full-image affected page. A newly appended B+ page is not tree-reachable before that MTR exists.

A page reused from an already-published object-specific free list does not extend the file and follows the owning reuse/MTR rules instead.

#### 4.11.1.1 Runtime failure before and after PAGE_INIT append

Every failure before the complete PAGE_INIT becomes part of the valid WAL stream follows one rollback path while the append-publication lock and private frame ownership remain held:

```text
1. cancel the unpublished WAL reservation under §12.12.1
2. leave published_page_count at N and publish no relation/FSM/index reference
3. remove the private LOADING mapping and reset its frame without exposing it
4. if extension began, ftruncate the file back to exactly N * PAGE_SIZE
5. verify the resulting physical page count is exactly N
6. only then release append-publication ownership and return the structured error
```

This applies to frame/resource reservation, physical extension, page initialization/validation, WAL construction/encoding, and a known append failure. If an extension syscall fails after possibly changing file length, the owner first inspects the aligned length and performs the same exact restoration. The failed PageNo `N` was never published and MAY be reused by the next successful append after cleanup.

Immediate truncation is safe because the append-publication lock excludes every later same-file append reservation; therefore no published or in-progress page can exist above `N`. Runtime failure does not leave an adopted unpublished tail hole. If exact length restoration or its verification fails, or the WAL append outcome is uncertain, the owner enters the §12.12.4 noncontinuable storage state and MUST NOT release the page as ordinary resident state or allocate another page from that file.

Once PAGE_INIT is validly appended, truncating/reusing the PageNo or returning an ordinary pre-publication failure is forbidden: the record may later become durable and recovery-visible. Steps 8–9 are designed as allocation-free/infallible publication under already-held ownership. A transient internal completion may be retried while ownership remains held; inability to establish the complete publication is noncontinuable under §12.12.4.

#### 4.11.1.2 Ordinary-append crash and runtime-failure outcomes

For a file initially containing/publishing `N` pages, the canonical outcomes are:

| Point | Non-crash operation failure | Process crash / recovery |
|---|---|---|
| before extension | release unchanged private intent, if any; length/count remain `N` | unchanged file |
| after extension, before provisional initialization | truncate/verify back to `N`, then PageNo `N` is reusable | the tail may survive or disappear; §4.11.3 removes any survivor without publication WAL |
| after provisional bytes, before valid PAGE_INIT append | discard private frame and truncate/verify to `N` | provisional memory vanishes; recovery removes an unpublished surviving tail |
| after valid append, before runtime publication | complete publication or enter noncontinuable state; do not truncate | if the complete record survived in the valid persisted WAL prefix, recovery reconstructs/publishes `N`; otherwise it removes the tail |
| after publication, before WAL durability | retain the legal dirty page; a durability failure does not roll it back | surviving complete WAL reconstructs it; absent WAL implies no data write was legal and recovery removes the tail |
| after WAL durability, before page write | retain the dirty page | redo reconstructs/re-extends page `N` if required |
| during page write | write failure leaves the resident page dirty; do not truncate | durable PAGE_INIT repairs a missing/torn page |
| after stable page write | page is durably published; normal generation reconciliation applies | valid page or PAGE_INIT redo supplies the same initialized state |

Physical extension by itself is never semantic publication. This content/length protocol concerns an already managed file and does not alter the filesystem-name publication rules in §4.7.

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

Whole-file shrinking remains unnecessary during normal execution except for the exact same-operation rollback in §4.11.1.1 while append serialization proves the suffix unpublished. The recovery truncation above is the corresponding post-crash cleanup. No other normal path truncates a managed page file merely to reclaim space.

### 4.11.4 Object-specific page reuse

Object-specific subsystems MAY recycle pages that are completely unused.

Examples include:

- a B+ tree free-page list,
- completely empty heap pages,
- unused FSM metadata pages.

Recycling or reinitializing a previously used heap `PageNo` MUST NOT bypass physical RID-reuse safety. If page recycling can make a previously used `(PageNo, SlotId)` identity reusable, it is subject to the global reclamation/read-epoch gate in Chapter 14.

The baseline does not require a general-purpose extent allocator.

General-purpose extent allocation is not a prerequisite for heap, index, or recovery correctness.

## 4.12 Page checksums

Persistent random-access page checksums use CRC32C.

For every persisted 8192-byte random-access page:

```text
1. logically treat bytes 16..19 as zero;
2. compute CRC32C over exactly bytes 0..8191;
3. store the resulting uint32_t little-endian in bytes 16..19.
```

The superblock uses this rule from initial file creation.

Every persisted ordinary random-access database page MUST carry a valid checksum
and MUST be checksum-verified before its stored `page_lsn` is trusted during
recovery or normal page open.

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

## 4.13 Persisted structural validation v1

Checksums establish byte integrity, not structural meaning. A checksummed page is ordinary database state only after the validation contract in this section accepts it under the owning file/page descriptor. This section centralizes the minimum v1 structural checks; the byte layouts remain owned by their format chapters.

### 4.13.1 Validation layers and required timing

The architecture uses four exact conceptual layers:

| Layer | Required meaning |
|---|---|
| `L0_RAW_PHYSICAL` | Exact transfer/size, whole-page checksum where applicable, universal common-header decoding, embedded PageId identity, and expected FileKind/PageType compatibility. No page-specific offset is dereferenced here. |
| `L1_PAGE_LOCAL` | Complete bounded validation of one page using its bytes, owning file metadata, and already-available immutable descriptor: header/count/offset geometry, canonical slot/entry encoding, local payload non-overlap, local key order, tuple/key codec validity, and every required-zero/reserved rule. |
| `L2_BOUNDED_REFERENCE` | Nonrecursive reference-domain checks plus checks made while following one actual reference: allocated/published PageNo domain, same-file/object identity, expected target type/level/state, sibling reciprocity/boundary order, RID target state, and bounded progress. |
| `L3_GLOBAL_GRAPH` | Exhaustive graph/object checks: complete B+ reachability and acyclicity, unique parentage, all-leaf depth, global separator/sibling consistency, complete free-list cycle/duplicate/disjointness, orphan detection, exhaustive heap/version-chain checks, and §16.5 catalog fixed-point validation. |

The required timing is:

| Consumer/event | Mandatory validation before ordinary use/publication |
|---|---|
| Existing-page BufferPool load | BufferPool completes L0, then invokes the registered owner for complete L1 and every nonfetching L2 domain check available from registered context before `LOADING -> RESIDENT`. |
| Page-controller construction | Only over a pinned, guarded `RESIDENT` page that passed the preceding checks. Construction does not weaken them; a controller additionally applies operation-specific L2 checks before following a reference. |
| Ordinary scan/search/update | L0/L1 are already established for each page; every followed RID/child/sibling/free-list reference receives the L2 checks below. |
| New or mutated writer state | The final page-local after-image passes L1 and nonfetching L2 before §12.12 WAL-backed publication and before exclusive mutation ownership is released. Dirty resident checksum bytes may be stale under §4.12.1, so this check excludes only recomputation of that checksum. |
| Recovery | Untrusted/torn bytes remain recovery-private. Complete redo results pass normal L0/L1 and applicable L2 publication checks before ordinary use; an atomic B+ MTR is validated as its complete reconstructed result. |
| Database open | Validates every control/bootstrap/superblock/root/catalog/status page it consumes. It does not run an implicit full user-heap/index L3 scan. Required startup objects that fail their required checks prevent READY. |
| Explicit verifier | Performs L0/L1/L2 exhaustively over the selected object and all applicable L3 checks. |

The registered owner validator is bounded by one 8192-byte page. It may use registered immutable file identity, published page count, B+ root/endpoint metadata, heap schema-history descriptors, index-key descriptor, and expected heap FileId. It MUST NOT recursively fetch, traverse a graph, query the catalog, consult MVCC status, or depend on the page becoming resident. If required static descriptor context is unavailable, the page cannot be ordinarily published under a less precise validator.

### 4.13.2 Universal deterministic rules

Every persisted count/offset/length calculation uses checked arithmetic before comparison or dereference. Overflow in calculations equivalent to:

```text
offset + length
base + count * entry_size
base + ordinal * width
page_no * PAGE_SIZE
```

is corruption. Validation order and result are deterministic for identical bytes plus identical descriptors; validity cannot depend on C++ padding, pointer values, unordered-container iteration, filesystem scan order, statistics, or optimizer state.

Every field/byte already specified as reserved-zero is required-zero. Every known code has only its defined v1 semantics. Unknown PageType/FileKind/slot codes are classified by §4.14.4. The one narrow recognized-but-unsupported heap state is §4.13.3's `REDIRECT_RESERVED`.

The FileKind/PageType combinations are exact:

```text
HEAP        page 0 SUPERBLOCK; page 1..N HEAP_DATA
FSM         page 0 SUPERBLOCK; page 1..N FSM_DATA
BTREE       page 0 specialized BTREE SUPERBLOCK;
            page 1..N BTREE_INTERNAL, BTREE_LEAF, or BTREE_FREE
CATALOG     page 0 SUPERBLOCK; page 1 CATALOG_DATA only
TXN_STATUS  page 0 SUPERBLOCK; page 1..N TXN_STATUS
```

Self-hosted catalog relation files are ordinary `HEAP` files and therefore contain `HEAP_DATA`, not `CATALOG_DATA`. A generic 72-byte non-BTREE PageFile superblock codec MUST reject/dispatch `FileKind::BTREE`; only the specialized B+ validator accepts its 128-byte header and extension. Every common superblock flag bit, ordinary-page unassigned flag bit, and specifically reserved field in the architecture follows §4.14's strict known-mask/RESERVED_ZERO rule.

An all-zero or otherwise uninitialized ordinary page inside an owning file's
published range is corruption even if physical extension produced those bytes or
a checksum happens to match. Every published ordinary page has a recognized
header and owning format. WAL-protected page publication uses PAGE_INIT/BTREE_MTR
under §§4.11.1 and 12.12; the immutable CATALOG_DATA bootstrap page instead
follows §16.9's bootstrap format and publication prerequisites, including its
required `INVALID_LSN`. Only the unpublished contiguous append tail permitted by
§12.12 may contain zero/uninitialized bytes, and it is inaccessible to ordinary
Fetch and reconciled before READY.

### 4.13.3 HEAP_DATA local validity

HEAP L1 validates, with checked arithmetic:

```text
page_type/format/header/common zero fields/PageNo
heap reserved = 0
lower = 48 + slot_count * 8
slot_count <= 1018
48 <= lower <= upper <= 8192
free_slot_head = INVALID_SLOT_ID or free_slot_head < slot_count
all slot descriptors
all retained tuple ranges and tuple encodings
the complete page-local UNUSED chain
```

`prune_hint` is a non-authoritative hint; every uint32 bit pattern is locally valid. Assigning further semantics requires an architecture revision. It cannot waive any structural check.

For every retained NORMAL or DEAD range `[tuple_offset, tuple_offset + tuple_length)`:

- addition does not overflow;
- the range is nonempty and lies wholly in `[upper, PAGE_SIZE)`;
- it does not intersect bytes `[0, lower)`, including headers/slot directory;
- it does not overlap any other retained NORMAL or DEAD tuple range;
- it contains one complete tuple that passes the exact §5.7–§5.13 header, flag, null-bitmap, schema-version, fixed-area, VARCHAR packing, scalar, and exact-length rules under the immutable historical SchemaDescriptor.

Validation may sort retained ranges by `(start,end,SlotId)` and reject when a preceding end exceeds the next start; any equivalent deterministic pairwise check is valid. Two slots never own any common payload byte. Unowned holes between valid retained ranges are structurally legal until compaction; dense packing is canonical compactor output, not a reader-validity requirement. Bytes in holes have no semantic owner and are ignored.

The exact slot-state table is:

| Persisted state | Canonical fields/payload | Free-list membership | Ordinary disposition |
|---|---|---|---|
| `NORMAL=1` | Nonzero retained coordinates satisfying the range rule; complete tuple validates; `aux=0`. | Never. | Ordinary physical/MVCC readers may inspect it; normal tuple-return paths require NORMAL. |
| `DEAD=2`, retained form | Nonzero retained coordinates satisfying the same range and tuple validation as NORMAL; `aux=0`. | Never. | Ordinary row-return paths skip it; vacuum/diagnostics may inspect the still-valid tuple. |
| `DEAD=2`, reclaimed form | `tuple_offset=0`, `tuple_length=0`, `aux=0`; owns no bytes. Mixed zero/nonzero coordinates are invalid. | Never. | Remains a nonreusable retired SlotId until Chapter 14 authorizes transition. |
| `UNUSED=0` | `tuple_offset=0`, `tuple_length=0`; `aux` is an in-range UNUSED SlotId or `INVALID_SLOT_ID`. | Exactly once. | Never dereferenced as a tuple; only grace-authorized reclamation emits it and insertion may pop it atomically. |
| `REDIRECT_RESERVED=3` | No v1 field/payload interpretation exists. | Forbidden. | Recognized numeric reservation but unsupported in v1; writers MUST NOT emit it and L1 returns `UNSUPPORTED_RESERVED_STATE`. The page is not ordinarily published, scanned, or repaired by guessing forwarding semantics. |
| Any other code | No interpretation. | Forbidden. | `CORRUPT_HEAP`. |

Thus a retained DEAD payload is not weakly validated merely because MVCC no longer returns it. Before `DEAD -> UNUSED`, either its complete original tuple remains structurally valid or compaction has produced the exact reclaimed form. Vacuum promptness and transaction visibility are not L1 facts.

Compaction may move a retained DEAD payload while atomically updating its slot coordinates, exactly as for retained NORMAL bytes, or may discard it and install the reclaimed DEAD form. It cannot leave stale coordinates or a partially decodable retained tuple.

The free-slot chain is mandatory L1 validation on every ordinary heap-page publication. Starting from `free_slot_head`, validation visits at most `slot_count` links and requires:

```text
each link in range or terminal INVALID_SLOT_ID
each visited slot canonical UNUSED
no repeated SlotId/cycle
canonical termination at INVALID_SLOT_ID
every UNUSED slot visited exactly once
no NORMAL/DEAD/REDIRECT slot visited
```

An INVALID head with any UNUSED slot, an unlisted UNUSED slot, duplicate membership, cycle, or noncanonical link is `CORRUPT_HEAP`; validation cannot loop indefinitely.

Tuple-header transaction fields receive their existing structural domain checks without performing status lookup: `xmin` is `FROZEN_TXN_ID` or a normal TxnId, `xmax` is `INVALID_TXN_ID` or a normal TxnId, `FROZEN_TXN_ID` is not a deleter, and `xmax == INVALID_TXN_ID` requires `cmax == 0`. The previous-version sentinel pair must be both invalid or both present. A present previous PageNo is an ordinary published page `>=1` in the same heap file and its SlotId is non-sentinel; an obvious self-reference to the current RID is invalid. Same-page targets can be bounded-checked immediately. Cross-page target state/version-chain acyclicity is L2/L3 and is checked when traversed or by the verifier.

RID dereference always occurs under the existing ReadEpochGuard/reuse protocol and checks expected heap FileId, published PageNo domain, embedded PageId, `slot < slot_count`, and caller-permitted state before exposing bytes. Query/index lookup requires NORMAL; vacuum/diagnostic operations may explicitly accept retained DEAD. UNUSED, reclaimed DEAD, REDIRECT_RESERVED, wrong-relation, or out-of-range targets return controlled stale/corruption classification according to the caller-specific protocol, never undefined access.

### 4.13.4 B+ local, superblock, and free-page validity

The specialized BTREE superblock validator first applies the §4.10 common-prefix rules and then requires:

```text
FileKind = BTREE
header_size = 128
object_id = expected IndexId
table_id = expected TableId
key_schema_version = 1
key_schema_fingerprint = the fingerprint of the catalog IndexDescriptor
tree_height >= 1
index_flags = 0
bytes 128..8191 = 0
root_page_no, first_leaf_page_no, last_leaf_page_no
    are distinct from INVALID_PAGE_NO and in [1, published_page_count)
free_page_head
    is INVALID_PAGE_NO or in [1, published_page_count)
free_page_head is not root_page_no, first_leaf_page_no, or last_leaf_page_no
```

The FileId from the safely opened managed file and its deterministic name defined by §4.7 must agree with the catalog IndexId/FileId mapping and FileSuperblock identity. Before an IndexDescriptor becomes usable, bounded root/endpoint loads additionally establish that the root belongs to this file, has `level = tree_height - 1`, and that both endpoints are leaves. If `tree_height = 1`, root/first/last are the same leaf; if `tree_height > 1`, the root is internal and `first_leaf_page_no != last_leaf_page_no`. The root has no persisted parent field. A published internal root with zero separators is invalid because §8.15.2 requires contraction before publication; the initialized empty tree is exactly the one empty root leaf. Non-root low/zero occupancy remains governed by §8.16's soft occupancy rule and is not by itself corruption.

Every BTREE_LEAF or BTREE_INTERNAL page receives the following complete L1 checks:

```text
format_version = 1
header_size = 64
common/node/slot flags and reserved fields = 0
slot_count <= 1016
lower = 64 + slot_count * 8
64 <= lower <= upper <= 8192
leaf level = 0; internal level > 0
every node level < registered tree_height
all slot descriptors and complete physical keys decode canonically
all entry ranges are valid, disjoint, and strictly ordered by physical key
```

Node pages have no persisted parent pointer, parent hint, page generation, or separate free-state flag. Tree-local free state is represented only by PageType BTREE_FREE; the process-local `root_generation` in §8.19.3 is not page bytes and is outside persisted validation.

For every slot, checked arithmetic establishes that `[entry_offset, entry_offset + entry_length)` is nonempty and wholly inside `[upper, PAGE_SIZE)`, does not intersect `[0, lower)`, and does not overlap another slot's independently owned entry range. Unowned holes are legal; packed/dense entry bytes are canonical compaction output, not a validity requirement. The architecture has no shared-prefix or overlapping-entry encoding.

Each `user_key_length` is in `1..1024` and the exact bytes decode as one complete canonical §8.5 key for the registered key schema. A leaf has `entry_length = user_key_length + 16` and a valid reserved-zero RID belonging to the indexed heap FileId. An internal entry has `entry_length = user_key_length + 24`, comprising that key, a valid reserved-zero separator RID belonging to the same indexed heap FileId, and one child PageNo. Trailing, truncated, noncanonical, wrong-schema, or overlong key bytes are corruption.

The slot sequence MUST be strictly increasing under canonical `(encoded_user_key,RID)` order on every ordinary load, not only in debug/verifier builds. Equal user-key bytes with different RIDs are structurally legal in all indexes, including UNIQUE indexes whose logical ownership is resolved by §11.10. Two exact physical `(user_key,RID)` entries in one page are corruption; the full L3 verifier also rejects the same physical key across pages. Recovery's proven replay of an already-present exact entry is an idempotent operation result, not authorization for duplicate stored copies.

An internal page has exactly `slot_count + 1` child references: the leftmost child plus one right child per entry. Every local child reference is in `[1, published_page_count)`, is not this page, is not the current root, and is distinct from every other child reference in that same parent. These are local/domain checks; target type, identity, level, and route checks occur when the child is fetched under §4.13.5. Full child-graph acyclicity and unique parentage are L3.

A leaf sibling field is either `INVALID_PAGE_NO` or lies in `[1, published_page_count)` in this B+ file. It cannot name the same leaf, the root when the root is not this leaf, or the current free-list head. Locally, `prev = INVALID_PAGE_NO` iff this is `first_leaf_page_no`, and `next = INVALID_PAGE_NO` iff this is `last_leaf_page_no`; the one-leaf tree therefore has neither sibling. Reciprocal and boundary checks require a bounded sibling fetch and are §4.13.5 L2 checks.

A BTREE_FREE page requires:

```text
format_version = 1
header_size = 40
common flags/reserved = 0
bytes 40..8191 = 0
next_free_page_no = INVALID_PAGE_NO
    or a PageNo in [1, published_page_count)
```

Its next link cannot name itself, the root, first leaf, or last leaf. The same exclusion applies to the superblock head. These checks do not by themselves prove the entire chain.

The following are unconditional structural invariants: the free list is acyclic, each free page occurs exactly once, and no free page is reachable from the live root or installed sibling chain. The architecture does not perform an O(file-size) free-list/tree graph traversal at every open or page load. The allocator instead serializes free-head access, validates the current head as BTREE_FREE and its next link before one atomic MTR pop/reinitialization, and revalidates the head before publication. A page already republished as a node cannot be popped as free. Any operation that walks more than one free link maintains a visited PageNo set and stops with `CORRUPT_INDEX` on repetition or after more than `published_page_count - 1` pages. The explicit verifier proves complete cycle/duplicate/free-live disjointness and orphan state. This lazy detection policy does not make cycles, duplicates, or free/live overlap valid.

### 4.13.5 Mandatory B+ traversal checks

Normal traversal supplies L2 information unavailable to an isolated owner validator. Parent-to-child handoff validates, before interpreting the target as a child:

- the parent reference remains installed under the existing latch-coupling protocol;
- target FileId/IndexId and embedded PageId identify the same index and requested PageNo;
- target is a leaf/internal node, never BTREE_FREE;
- `child.level + 1 = parent.level` without arithmetic overflow;
- descent reaches level zero after exactly the superblock `tree_height - 1` internal transitions, never earlier or later;
- the selected child is compatible with the immediate separator interval used for that routing decision.

For the last check, the parent supplies the exact half-open interval selected by §8.11.1: `C0` has `(-infinity,K1)`, `Ci` has `[Ki,K(i+1))`, and `Cn` has `[Kn,+infinity)`. Every physical key stored directly in the fetched target page—leaf entries or internal separator keys—must lie in that interval. Equality with a lower bound is legal; equality with an upper bound is not. An empty target supplies no local key fact. This check remains compatible with stale-low internal separators, but cannot prove the unseen leftmost descendant or complete subtree; complete descendant range/separator correctness and unique parentage are L3. A traversal keeps a visited PageNo set, or an equivalent exact depth/progress guard, and fails rather than following a cycle or exceeding the declared height.

Forward leaf handoff retains the current guard/latch while loading the next leaf and requires: same B+ owner, L0/L1 success, level zero, `next.prev = current`, and—when both pages are nonempty—`current.last_physical_key < next.first_physical_key`. The scan records visited leaf PageNos and may cross at most the number of published ordinary B+ pages; repetition, an impossible endpoint, wrong type/identity, or nonincreasing boundary is `CORRUPT_INDEX`. It must fail the scan rather than loop, duplicate rows, or silently omit rows. Reverse-link validation follows the same reciprocal/boundary rule when a maintenance/verifier path follows `prev`; native reverse query scans remain deferred.

The L3 B+ verifier proves, over a stable verifier-owned view:

- every live page is reachable exactly once from the root and the child graph is acyclic;
- all leaves occur at the declared depth, all separators bound their complete subtrees, and global physical-key order is strict;
- the sibling chain is reciprocal, acyclic, has exactly the root-reachable leaves in order, and agrees with first/last endpoints;
- the free chain is bounded, acyclic, duplicate-free, and disjoint from root/children/siblings;
- no allocated/published page is orphaned except an exact architecture-authorized recovery-private or unpublished-tail state.

Normal operation's bounded checks are never permission to return a known-wrong answer after detecting a global violation.

### 4.13.6 FSM, catalog, status, and superblock validity

FSM_DATA L1 requires the exact §6.5 header/type/version/zero fields, `entry_count <= 8144`, and the checked deterministic relation:

```text
first_heap_page_no = 1 + (fsm_page_no - 1) * 8144
represented initialized range =
    [first_heap_page_no, first_heap_page_no + entry_count)
```

The initialized range cannot contain `INVALID_PAGE_NO` or extend beyond the paired heap file's published page range available to the registered FSM owner. Every category is an arbitrary valid uint8 estimate; it need not equal current heap free space. Bytes in `[48 + entry_count, 8192)` are zero. A short initialized prefix may be repaired/rebuilt under §6.10; it cannot be indexed outside its current bound. Ordinary insertion still validates authoritative heap free space, so stale but structurally valid categories cannot cause unsafe access.

CATALOG_DATA page 1 receives the exact immutable §16.9 bootstrap-header and six-entry validation, including CRC/header/count/version/identity/reserved-zero rules. Self-hosted catalog relation pages receive ordinary HEAP L0/L1 validation under the canonical §16.5 built-in/user SchemaDescriptors. Section 16.5 scalar, cross-row, foreign-reference, fixed-point, and cache-reconstruction checks are catalog L3 and must complete during the open protocol in Chapter 3 before catalog authority is published; they are not recursively performed by a heap-page owner validator.

TXN_STATUS pages require the exact §9.12 32-byte header, PageId/type/version/zero fields, checksum, and 8160-byte two-bit array. All four assigned two-bit patterns decode structurally: INVALID, COMMITTED, ABORTED, and recognized nonterminal RESERVED. RESERVED retains §9.11.1 semantics and is not an unsupported unknown state. Given the durable transaction-allocation high-water mark, every represented entry for an unallocated TxnId at or above `reserved_txn_id_end` must be INVALID; entries in the allocated range may structurally be any assigned pattern, with terminal/reference semantics resolved by recovery and transaction-status logic. A required status page is not exposed to ordinary lookup until recovery under §§12.10.5, 13.13.2, and 13.17 has repaired it and L0/L1 have succeeded.

Every HEAP/FSM/CATALOG/TXN_STATUS FileSuperblock receives the exact §4.10 72-byte codec checks: singleton/object identity appropriate to FileKind, page size, format, header size, initialized FileId, checksum, required-zero reserved fields and trailing bytes, and file-kind/page-0 identity. The BTREE superblock receives the specialized checks in §4.13.4. File size, §12.12 published-page boundary, deterministic §4.7 name, catalog descriptor, and superblock FileId/object identity are bounded L2 cross-checks. A mismatch is corruption or missing-required-object failure, not filename-based inference.

Control slots use §13.2's own framing/CRC/generation validation rather than the common page header. The immutable bootstrap page uses its §16.9 codec. Neither is exempt from deterministic checked arithmetic merely because it is not an ordinary page type.

### 4.13.7 Minimum ordinary-read validation matrix

| Persisted object/page kind | L0 before consumption | L1 before consumption | Additional mandatory bounded L2 | Full L3 required before this page's normal use? |
|---|---|---|---|---|
| HEAP_DATA | Yes | §4.13.3, including all slot ranges, tuple codecs, and complete local free-slot chain | RID/version-link target checks when followed | No; full heap/version graph is verifier work. |
| FSM_DATA | Yes | §4.13.6 exact mapping/prefix/suffix | Paired heap FileId and published-range bound | No; category/heap comparison is repair/verifier work. |
| BTREE superblock | Exact page-0/superblock physical checks | §4.13.4 extension/descriptor checks | Bounded root/first/last target checks before index use | No whole-tree walk. |
| BTREE_INTERNAL | Yes | §4.13.4 ranges, codecs, strict order, children | §4.13.5 on each followed child | No. |
| BTREE_LEAF | Yes | §4.13.4 ranges, codecs, strict order, sibling domains | §4.13.5 on sibling handoff; RID L2 on heap recheck | No. |
| BTREE_FREE | Yes | §4.13.4 exact free format/link domain | Serialized head/pop recheck and bounded chain progress | No, but allocator never ignores a detected violation. |
| CATALOG_DATA bootstrap | Exact bootstrap framing/checksum/identity | Exact §16.9 header and six entries | Bootstrap/superblock/system-root identity cross-check | Yes: §16.5 fixed point before READY. |
| Catalog relation HEAP_DATA | Yes | Ordinary HEAP under canonical catalog descriptor | §16.5 scalar/reference checks during reconstruction | Yes for committed catalog authority before READY. |
| TXN_STATUS | Yes | §4.13.6 two-bit/header/high-water rules | WAL/status reconciliation under §§12.10.5, 13.13.2, and 13.17 before lookup | No independent full status-file scan beyond recovery/open requirements. |
| HEAP/FSM/CATALOG/TXN_STATUS superblock | Exact §4.10 page-0 codec | File-kind-specific local fields | File registry/name/catalog/published-length identity | Only where required by Chapter 3 startup ownership. |
| database.control | Its own exact dual-slot CRC/framing | §13.2 field/range/reserved rules | Selected checkpoint/WAL availability | Both slots are evaluated before recovery entry. |

“No L3 before page use” never means an L3 invariant is optional. It means ordinary bounded operation detects violations it encounters and the explicit verifier provides exhaustive detection without imposing an O(database-size) fetch path or open scan.

### 4.13.8 Writer, redo, and failure contract

A writer holds the page's exclusive mutation/no-flush ownership while constructing its final bytes. Before §12.12 publication, every affected after-image must pass its complete L1 and available nonfetching L2 checks under the same descriptor that future readers use. For a multi-page B+ MTR, all final pages and changed superblock/root/free-list metadata validate as one provisional set before WAL append/publication. A writer MUST NOT knowingly publish a page the canonical reader would reject; failure restores the exact prior runtime state under §12.12 or escalates to DATABASE_NONCONTINUABLE if restoration/invariants cannot be established.

Recovery may safely identify a target and install PAGE_IMAGE/PAGE_INIT/redo bytes under recovery-private ownership even when the old data page fails checksum. It does not perform ordinary field parsing on torn bytes before an applicable full image establishes trusted framing. Individual redo actions may have private intermediate forms, but a complete page result—and the complete result set of an atomic BTREE_MTR—passes normal L0/L1 and applicable L2 before ordinary BufferPool publication or Chapter 3 READY. A valid WAL action whose complete result remains structurally invalid is `RECOVERY_FAILED`/persisted corruption; recovery never grants a permanent validation waiver.

Validation failures have these conceptual classifications:

| Condition | Classification and lifecycle effect |
|---|---|
| Checksum/header/PageId/type/range/local codec failure on one data page | `CORRUPT_PAGE`, refined to `CORRUPT_HEAP` or `CORRUPT_INDEX`; ordinary publication/access fails. |
| `REDIRECT_RESERVED` in a heap page | `UNSUPPORTED_RESERVED_STATE`; no ordinary publication. It is not guessed as another slot form. |
| B+ traversal/free-list/global structural failure | `CORRUPT_INDEX`; the operation/verifier fails and no known-malformed route is used. |
| Required control/bootstrap/catalog/status/root object invalid during open or after recovery | `CORRUPT_DATABASE` or `RECOVERY_FAILED`; Chapter 3 cannot publish READY. |
| Failure to restore a provisional writer state or uncertainty about installed runtime ownership | §12.12 `DATABASE_NONCONTINUABLE`. |

A malformed non-required user heap/index page may be reported only when accessed; open need not pre-scan it. Once detected, there is no automatic repair except an already-defined WAL/FPI recovery action or authoritative FSM rebuild. Structurally valid stale MVCC/index entries, DEAD tuples, and safely retired pages are not corruption merely because vacuum has not removed them.

### 4.13.9 Explicit verifier and bounded-progress requirements

The explicit verifier performs deterministic stable-view checks beyond ordinary page loading:

- HEAP: L0/L1 for every published page; exhaustive SlotId/RID and previous-version-link domain/acyclicity checks available under the selected verification scope; no overlapping retained payloads;
- FSM: all local mappings/prefixes plus optional exact comparison of categories with authoritative heap free space, reporting stale estimates separately from malformed structure;
- B+: the complete §4.13.5 root, child, leaf, free, and allocated-page graphs, including strict global order, all-leaf depth, separator bounds, sibling coherence, cycles, duplicate membership, free/live disjointness, and orphans;
- CATALOG: the §16.5 cross-row/self-description fixed point;
- TXN_STATUS: all present local pages plus transaction-reference/status consistency available under the recovery horizon.

Every persisted-chain traversal has a finite architecture-derived progress bound. Heap free slots visit at most `slot_count`; B+ root descent uses declared `tree_height` and rejects repeated pages; sibling and free-list walks visit at most `published_page_count - 1` ordinary pages and reject repeats; version/RID verification uses a visited RID set and cannot exceed the selected heap's finite published slot population. An implementation may use an equivalent lower bound, but it cannot omit duplicate/cycle detection or rely on eventual integer wraparound.

### 4.13.10 Forbidden interpretations

The architecture specifically forbids:

1. accepting overlapping retained NORMAL or DEAD tuple ranges;
2. checking tuple/entry bounds without checking pairwise owned-range overlap;
3. trusting B+ key order because a checksum passed or deferring local order checks to the verifier;
4. accepting overlapping B+ entry payloads or exact duplicate stored physical keys;
5. following an invalid/out-of-range/wrong-owner child, sibling, RID, or free-page reference;
6. looping indefinitely on a corrupt heap chain, B+ traversal, sibling chain, free list, or version chain;
7. allowing duplicate B+ free-list membership or a page that is both free and live-tree reachable;
8. interpreting REDIRECT_RESERVED as NORMAL, DEAD, UNUSED, or an invented forwarding tuple;
9. accepting unknown slot states or zero-filled published ordinary pages;
10. allowing recovery to expose a structurally invalid completed result;
11. using unchecked persisted offset/count/length arithmetic;
12. treating valid stale MVCC/index/FSM garbage as structural corruption merely because maintenance is delayed;
13. returning silent partial/duplicate/missing query results after detecting a malformed B+ sibling or route;
14. making an expensive verifier the sole validator for any invariant needed to parse one page safely.

## 4.14 V1 persisted-format compatibility and reserved-state policy

V1 is strict by default. A reader may interpret, rewrite, or publish persisted bytes only under semantics explicitly defined by the owning v1 format. Field boundaries, a checksum, total length, or the ability to copy bytes do not make unknown semantics safe to ignore.

This section is the canonical compatibility policy for every independently versioned page, file, control, WAL, catalog, scalar/default, and statistics grammar. Format-specific chapters supply exact codes and byte layouts; they may define a narrower safe fallback only when this section lists it.

### 4.14.1 Terminology and default classification

| Term | Canonical meaning |
|---|---|
| `KNOWN` | A value, bit, sentinel, or version whose complete v1 semantics are defined for this exact field. |
| `RESERVED_ZERO` | Space reserved for possible future use but required to be zero in the owning version-1 grammar. It has no v1 payload semantics. |
| `KNOWN_UNSUPPORTED` | A numeric value intentionally named by v1, but whose feature has no ordinary v1 execution semantics. The format-specific rule defines its controlled rejection. |
| `UNKNOWN` | A value/code/bit not assigned by the v1 specification for that registry or field. |
| `IGNORABLE_EXTENSION` | A field/value whose owning grammar explicitly proves that an older reader may omit it without changing framing, known-field meaning, query results, visibility, recovery, durability, or future rewrites. |
| `UNSUPPORTED_FORMAT` | A recognizable owning format/version/state requires semantics beyond v1. The reader stops at the stable discriminator and does not parse version-owned fields as v1. |
| `CORRUPTION` | Bytes claim a supported v1 grammar but violate its canonical values, masks, lengths, identities, checksums, or required-zero rules. |

The exact default is:

```text
supported owning version + UNKNOWN enum/flag/nonzero RESERVED_ZERO
    -> CORRUPTION unless an exact registry rule below classifies that field

recognized owning family + newer unsupported version
    -> UNSUPPORTED_FORMAT for that family

explicit KNOWN_UNSUPPORTED state
    -> its named unsupported-state result

explicit IGNORABLE_EXTENSION
    -> only the format-specific listed fallback
```

The only field-level departures from the first line are the exact unknown-WAL-type unsupported result in §4.14.4 and the rebuildable-statistics scope/payload fallback in §4.14.5. They create no generic inference rule for another enum.

Implementations do not invent a different classification because one error is more convenient. The public/API layer may group these under a generic open/access failure while retaining the conceptual distinction and diagnostic; recovery and lifecycle decisions use the exact class.

### 4.14.2 Version dispatch and framing

Each version domain is independent. The database/control format, FileSuperblock/page format, catalog schema, B+ key schema, DefaultValueBlob, statistics payload, and WAL grammar do not inherit support from one global version number.

For a structure with a v1-defined stable family discriminator and version field:

1. read only the bytes needed to identify that family and version using checked fixed offsets;
2. version `1` selects the exact v1 grammar and all of its checksum, length, mask, enum, and reserved-zero validation;
3. version `0`, an invalid sentinel, or a value below the first defined version is corruption for a claimed member of that family;
4. a positive version greater than `1` is unsupported for v1;
5. do not apply v1 layout, checksum offsets/algorithm, flags, trailing-region, or rewrite rules after selecting an unsupported version.

The stable discriminator is only a safe stop boundary; it is not proof that the newer object is intact. V1 need not and generally cannot validate a future version's checksum or remaining layout before returning unsupported. A reader MUST NOT treat `version != 1` as “parse as v1 and hope.”

V1 is the first supported persisted format and has no earlier page/catalog/WAL/default format to read. A per-table `schema_version`, StatsVersion, control generation, bootstrap generation, root generation, creation epoch, or transaction identity is not automatically a format-version field; those values obey their own semantic domains.

The supported format-version domains are exact:

| Domain | V1 dispatch and non-v1 result |
|---|---|
| Common ordinary page and FileSuperblock `format_version` | Expected PageType/FileSuperblock family plus `1`; `0` is corruption, `>1` is `UNSUPPORTED_PAGE_FORMAT` or `UNSUPPORTED_FILE_FORMAT`. Only the v1 CRC32C rule is applied to version 1. |
| `database.control` slot | Exact `DBLUSCTL` plus version `1`; zero is an invalid/corrupt slot, while any exact-magic `>1` slot blocks v1 open as `UNSUPPORTED_DATABASE_FORMAT`. |
| CATALOG_DATA bootstrap | Common page version, `bootstrap_version`, `catalog_schema_version`, and every built-in `relation_schema_version` are `1`; zero is corruption and a recognizable greater version is `UNSUPPORTED_CATALOG_SCHEMA`/format. |
| BTREE/catalog `key_schema_version` | Exactly `1`; a catalog carrier value less than or equal to zero (the persisted BTREE field can represent only zero of these) is corruption, while a positive greater value makes the committed index unsupported and unusable by v1. |
| DefaultValueBlob | Exact `DBLUSDEF`, version `1`; zero is malformed/corrupt required default metadata, greater values are an unsupported default format. |
| StatisticsPayload | Exact `DBLUSSTA`, payload version `1`; zero/malformed v1 is an invalid statistics generation, and a well-framed greater version is unsupported advisory metadata. Both use statistics fallback. |
| PersistedScalarV1 | No embedded version field exists. The enclosing supported grammar selects exactly the 16-byte v1 scalar codec; evolution requires a new enclosing grammar/version rather than guessing a hidden scalar version. |
| WAL record grammar | No stream/record-header version field exists. The exact 48-byte v1 framing and record registry define v1; an unknown fully framed record type is `UNSUPPORTED_WAL_FORMAT`, while malformed required framing is corruption/torn-tail handling under Chapter 13. |

Checksum algorithms are owned by these versions. V1 page, superblock, control, WAL-record, DefaultValueBlob, and statistics checksums use their specified CRC32C coverage; there is no persisted generic algorithm selector. A future algorithm requires a future owning version. V1 never verifies a newer object with the v1 checksum and continues.

### 4.14.3 Flags, reserved bytes, padding, and trailing data

For every v1 field or byte range described as reserved, reserved-zero, unassigned, padding, or “all other bits”:

```text
writer: write zero
reader: require zero
rewriter: do not preserve a nonzero input
```

Nonzero RESERVED_ZERO material in a structure claiming version 1 is corruption, not an ignorable extension and not an unsupported newer version. This applies to explicit serialized padding as well as named fields. In-memory C++/ABI padding has no persisted meaning and is never serialized; persisted objects are field codecs, not raw struct dumps.

Every v1 flag field has an exact known mask. Writers set only semantically required known bits; readers require:

```text
flags & ~KNOWN_MASK == 0
```

and enforce every bidirectional canonical known-bit rule. A field with no assigned v1 bits has `KNOWN_MASK=0` and must equal zero. In particular, all common ordinary-page flags, FileSuperblock flags, control flags, WAL-header flags, FSM flags, B+ superblock/node/slot flags, catalog-entry flags, DefaultValueBlob flags, and statistics common flags are zero. Tuple flags have known mask `0x0003`; PersistedScalarV1 flags have known mask `0x00000001`; statistics COLUMN `value_flags` have known mask `0x00000003`.

Length delimiting alone never authorizes unknown trailing bytes. A v1 object consumes exactly its grammar-defined length, including canonical zero alignment or fixed trailing reserved area. Extra/nonzero trailing bytes are corruption unless a specific grammar explicitly designates them as an IGNORABLE_EXTENSION. No current required v1 format does so.

V1 has no generic preserve-and-round-trip capability. An old writer must understand every semantic field it rewrites. Known opaque or advisory fields—such as `creation_epoch`, `prune_hint`, and valid FSM category bytes—remain KNOWN v1 values under their documented semantics; they are not storage for unknown extensions. No arbitrary unknown bits/bytes may be copied forward merely because their positions are known.

### 4.14.4 Numeric registries, sentinels, and exact outcomes

The architecture's numeric registry tables are normative and never derive values from source enum order.

| Registry/field | V1 outcome |
|---|---|
| FileKind | `1..5` are KNOWN and dispatched to their exact owner; `0` is the defined invalid value and is illegal in a persisted file; every other code in a version-1 superblock is `CORRUPT_FILE`. BTREE is supported only by the specialized 128-byte codec; the generic 72-byte codec rejects/dispatches it rather than parsing it generically. |
| PageType | `0..7` are KNOWN only in their permitted FileKind/page-position contexts. A known wrong type or any other code in a version-1 page is `CORRUPT_PAGE`; a recognized expected page family with `format_version>1` is instead unsupported. |
| Heap slot state | UNUSED, NORMAL, and DEAD are KNOWN. REDIRECT_RESERVED is KNOWN_UNSUPPORTED exactly as §4.13.3 defines: v1 writers never emit it and ordinary readers return `UNSUPPORTED_RESERVED_STATE`. Every other code is `CORRUPT_HEAP`. |
| Tuple flags | `HAS_NULLS=0x0001` and `HAS_VARLEN=0x0002`; every other bit is corruption in the v1 tuple grammar. |
| TXN_STATUS two-bit code | All four bit patterns are KNOWN: INVALID, COMMITTED, ABORTED, RESERVED. RESERVED has the established nonterminal §9.11.1 semantics and is not rejected as unknown. Contextually impossible status references remain corruption. |
| WAL record type | Codes `0..9` are KNOWN. Any other type in an otherwise complete framed/CRC-valid required record is `UNSUPPORTED_WAL_FORMAT`; it is never skipped merely because `total_length` is known. |
| Persisted TypeId | `1..7` are KNOWN stored scalar types; `0` is the known invalid/not-stored sentinel. Any other TypeId inside a required v1 descriptor/scalar is catalog/scalar corruption. In advisory statistics it invalidates that statistics generation. |
| Catalog constraint kind | `1=UNIQUE`, `2=PRIMARY_KEY`; any other value in catalog schema version 1 is `CORRUPT_CATALOG` and prevents descriptor publication/READY. A newer catalog schema is rejected before its rows are interpreted. |
| Statistics scope kind | `1=TABLE`, `2=COLUMN`, `3=INDEX`; any other value invalidates only the decodable statistics scope/version and uses the whitelist fallback in §4.14.5. |

Canonical BOOLEAN fields accept only the ordinary BOOLEAN values. No catalog boolean or integer enum has spare truth values or implicit flag bits.

An identifier sentinel is a KNOWN field-specific semantic value, not unassigned enum extension space. `INVALID_PAGE_NO`, `INVALID_FILE_ID`, `INVALID_SLOT_ID`, `INVALID_TXN_ID`, `INVALID_LSN`, and other exact zero/invalid object rules are legal only where the owning field explicitly permits that sentinel—for example an absent sibling/free link or no checkpoint. The same value is corruption where a real identity is required. Implementations do not globally reject or globally tolerate a sentinel independent of field context.

### 4.14.5 Required state, rebuildable state, and the extension whitelist

Required correctness state includes control/checkpoint/WAL, bootstrap and core catalog descriptors, table and index schemas, heap/B+ page formats, transaction status, defaults, visibility/version metadata, and every committed object's required file/superblock identity. Unknown or unsupported semantics in required state prevent that state's use and, when required during open/recovery, prevent READY.

Rebuildable advisory state may fall back only where an existing architecture supplies a correctness-independent source. The complete v1 IGNORABLE_EXTENSION whitelist is:

1. a decodable `sys_statistics` row/chunk identity whose scope/version is incomplete or invalid;
2. a complete decodable catalog chunk set whose reassembled bytes contain the exact stable `DBLUSSTA` magic and a readable positive `payload_version>1`;
3. a malformed known-version statistics payload, including malformed nested PersistedScalarV1 data.

Each case rejects the entire affected statistics scope/version, records a diagnostic, and selects an older complete valid descriptor or missing-statistics fallback. V1 does not apply version-1 total-length, checksum-offset, flags, or payload parsing to the greater version; the already-decoded catalog chunk envelope is sufficient to discard it. Rows/versions are never mixed or partially salvaged. Sections 34.1 and 35.2 guarantee that this affects plan quality only, never semantic emptiness or query correctness.

The whitelist does not include malformed ordinary heap-tuple/page encoding of the `sys_statistics` relation. The §§4.13–4.14 page/tuple decoder must first safely produce a decodable catalog tuple. Nor does it include malformed core catalog rows, unknown constraints/types, required defaults, FSM page formats, B+ key formats, or transaction status. Stale but structurally valid FSM categories and statistics estimates are KNOWN approximate values, not ignored extensions.

Unknown unrelated filesystem names are left untouched under §4.7. That namespace safety rule is not persisted-format acceptance: an unknown file is never opened as a database object, and a bootstrap/catalog-required file with unknown FileKind/version cannot be published merely because its directory entry is preserved.

### 4.14.6 Control, open, index, and nested-version decisions

Control-slot family dispatch occurs before normal dual-slot validity/fallback. If either 4096-byte slot has exact `DBLUSCTL` magic and a positive unsupported format version, v1 returns `UNSUPPORTED_DATABASE_FORMAT` and does not select an older v1 slot. This deliberately covers the generation-10-v1/generation-11-v2 case: v1 cannot validate or compare the v2 generation/checksum/layout safely and cannot time-travel to generation 10. A version-0, wrong-magic, checksum-bad, or otherwise corrupt v1 candidate remains governed by the existing independent-slot/torn-update fallback; it is not evidence of a supported newer format.

Before READY publication under Chapter 3, every deterministically reachable required control, bootstrap, WAL, catalog descriptor, FileSuperblock, B+ key descriptor, and transaction-status root uses supported semantics. An unsupported required object prevents READY. Sections 4.13–4.14 still do not require an exhaustive user-page L3 scan: an unsupported/corrupt user heap or B+ page not touched during open fails its later ordinary access without reinterpretation. If that failure renders a required semantic object unusable, the escalation rules in §§4.13–4.14, §39.1, and Chapter 3 apply.

Every committed catalog index is required state in v1. V1 defines neither an “unusable index” catalog state nor automatic rebuild-at-open. Therefore a `sys_indexes.key_schema_version` or BTREE superblock `key_schema_version` other than `1` prevents index descriptor publication and READY; a nonpositive catalog value is corruption and a positive greater value is unsupported. The index cannot be traversed with v1 comparison rules. For version `1`, a schema fingerprint mismatch is `CORRUPT_INDEX`, not a future extension.

Nested failure follows semantic criticality, not depth. A supported catalog containing an unsupported DefaultValueBlob cannot reconstruct its required column descriptor and therefore prevents READY/required use. A supported catalog containing an unsupported statistics payload uses §4.14.5 fallback. A future scalar form must be selected by a future enclosing default/statistics/catalog grammar; v1 never reinterprets it as PersistedScalarV1.

### 4.14.7 Recovery, writing, rewrites, and upgrade boundary

Recovery is not best effort. An unknown complete WAL record type may carry required redo or transaction semantics, so recovery stops with `UNSUPPORTED_WAL_FORMAT` and cannot reach READY. A record with v1 framing/flags/reserved/CRC violations follows §13.11's torn-tail versus required-prefix corruption rules. Recovery cannot guess an unsupported page format required by redo; completed redo output must still pass §§4.13–4.14 under a supported owner format. Statistics fallback remains outside crash-correctness authority.

Every v1 writer emits only supported versions, KNOWN enum/state values, semantically correct known flag bits, field-legal sentinels, exact lengths, and zero RESERVED_ZERO/padding/trailing bytes. A v1 writer does not copy unknown input material into a rewritten object.

Vacuum, heap compaction, B+ rewrite/MTR, catalog update, checkpointing, recovery rewrite, and any other page/object rewrite first require supported readable input semantics. They refuse an unsupported source rather than normalize it to v1, clear unknown bits, or discard unknown trailing data. This rule prevents silent downgrade/information loss.

V1 has no cross-version read-only compatibility mode, no format migration, and no online/offline upgrade machinery. A recognizable newer database is not opened for read or write and is never downgraded. A newer version number is an unsupported-format result, not an invitation to upgrade. Future compatibility requires an explicitly designed reader/migration and, where needed, a new owning version.

Conceptual errors remain distinguishable as:

```text
UNSUPPORTED_DATABASE_FORMAT
UNSUPPORTED_FILE_FORMAT
UNSUPPORTED_PAGE_FORMAT
UNSUPPORTED_WAL_FORMAT
UNSUPPORTED_CATALOG_SCHEMA
UNSUPPORTED_DEFAULT_FORMAT
UNSUPPORTED_RESERVED_STATE
CORRUPT_WAL / CORRUPT_PAGE / CORRUPT_HEAP / CORRUPT_INDEX / CORRUPT_CATALOG / CORRUPT_DATABASE
```

Exact source enum names are optional. Unsupported means recognizable semantics beyond v1; corruption means a claimed supported grammar is invalid. Either may surface as a generic failed open/access to a simple client, but diagnostics and lifecycle behavior retain the distinction.

### 4.14.8 Forbidden compatibility shortcuts

V1 specifically forbids:

1. preserving unknown FileSuperblock/common-page flags and continuing;
2. ignoring or round-tripping nonzero reserved/padding bytes;
3. parsing any version-2 structure as version 1;
4. selecting an older control slot because another exact-magic slot advertises a positive unsupported version;
5. skipping an unknown complete WAL record because its length is known;
6. interpreting REDIRECT_RESERVED as NORMAL or any invented forwarding state;
7. treating an unknown PageType as an opaque traversable page;
8. clearing/dropping unknown tuple flags during compaction and publishing the result;
9. ignoring an unknown catalog constraint kind as if no constraint existed;
10. using an unsupported B+ key schema with v1 comparator/key decoding;
11. rejecting stale but structurally valid FSM categories as future-format corruption;
12. failing the database solely for an unsupported/malformed advisory statistics payload when the exact fallback applies;
13. treating preservation of an unknown filesystem name as evidence of file-format compatibility;
14. rewriting or downgrading a newer-format database as v1;
15. using host-struct padding or known reserved bytes as persisted extension storage;
16. collapsing all unknown material into corruption without preserving the unsupported-versus-corrupt distinctions above.

## 4.15 Storage-foundation invariants

1. `FileId` is a database identity, never an operating-system file descriptor.
2. `PageId` is a persistent logical identity, never a frame index or memory address.
3. RID denotes a physical heap tuple version, not a permanent logical SQL row.
4. Persistent numeric file-kind and page-type codes are explicit and never implicitly renumbered.
5. Random-access database file page `0` is the superblock; ordinary object pages begin at page `1`.
6. Persistent multi-byte fields defined by this chapter use explicit little-endian serialization.
7. The common page header is 32 bytes.
8. Page-type-specific parsing validates the expected persisted page type before type-specific interpretation.
9. A superblock is exactly 8192 bytes; `header_size=72` for HEAP/FSM/CATALOG/TXN_STATUS and `128` for BTREE.
10. Every superblock reserved field and every byte after its file-kind-specific header is zero.
11. The superblock CRC32C covers exactly bytes `0..8191` with bytes `16..19` logically zero.
12. In the WAL/recovery-enabled architecture, every persisted ordinary random-access page also carries a valid whole-page CRC32C.
13. A dirty resident checksum may be stale; only a canonical stable flush/full-image copy supplies the durable checksum.
14. A write to an unallocated page does not allocate or sparsely extend the file.
15. Concurrent raw append allocation of one managed file cannot return the same new `PageNo` twice.
16. WAL-mode append publication serializes initialization-record append before `published_page_count`/reachability advances.
17. A known pre-append new-page failure restores and verifies the pre-extension length before append serialization releases; the unpublished trailing PageNo may then be reused.
18. Recovery may remove only the contiguous unpublished append suffix after reconstructing every durable page publication.
19. `page_lsn` is the newest validly appended WAL-protected modification reflected in a published page once WAL is active; a candidate reservation is insufficient.
20. Page checksums detect torn/corrupt writes; retained full-image WAL supplies repair.
21. Regular-file synchronization does not durably publish create/rename/unlink directory entries; the required owning directories are synchronized explicitly.
22. Committed catalog ownership never names a private DDL basename and never precedes durable final-name publication of every required physical file.
23. A required committed object file is never reconstructed from filename guesswork when its durable final entry is missing.
24. A checksum-valid page is not ordinary state until §4.13's required structural layer accepts it; writers and recovery publish only locally valid completed results.
25. Readers/writers follow §4.14's strict version, known-mask, reserved-zero, enum, trailing-byte, and no-downgrade rules; only the explicit statistics whitelist may ignore unsupported persisted metadata.
26. Database-root creation owns the ordinary process-local root/control claim and `database.control` lock before final-root rename and retains both through successful external-parent synchronization; the lock excludes competing use but does not make the root durable.
27. After acquiring ordinary ownership of an existing final root, an independent opener or remover verifies the retained parent entry, synchronizes the external parent, and revalidates root identity before database inspection, recovery, mutation, removal, READY, or acknowledged work.
28. Whole-database removal performs no internal content deletion while the captured old database occupies live final name `D`; semantic removal is published only by the no-replace retirement rename followed by successful external-parent synchronization.
29. While old `D` remains the live database root, its ordinary `database.control` lock inode is not unlinked, replaced, or made independently recreatable.
30. A durably retired root is private reclamation state, never an ordinary open target; conforming reclamation failure or crash cannot resurrect old `D`. Section 4.7.9 distinguishes control-authorized recursive retry from empty terminal finalization; nonempty residue without destructive control identity is refused.
31. Ordinary orphan cleanup never guesses unknown names, while explicit whole-root reclamation may remove unknown contained entries only after supported-v1 identity, exclusive ownership, and durable root retirement have been established.
