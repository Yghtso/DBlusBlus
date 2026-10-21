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

The executor applies MVCC visibility after obtaining physical tuple versions. Physical heap scan order is **not** SQL result ordering. Without an explicit `ORDER BY`, SQL result ordering remains semantically unspecified even if a particular execution happens to follow physical page/slot order.

## 5.3 HEAP_DATA page format v1

A heap data page uses the 32-byte common page header defined in Chapter 4 followed immediately by a 16-byte heap-specific header.

For the defined architecture:

```text
page_type          = HEAP_DATA
format_version     = 1
header_size        = 48
common flags       = 0
common reserved16  = 0
```

The persisted common-header `page_no` MUST equal the logical `PageId.page_no` of the page being interpreted. The total heap-page header is exactly: `48 bytes`.

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

All multi-byte fields are little-endian. `lower` is the first byte after the heap header and slot directory. `upper` is the first byte of the tuple-data region. The current contiguous free-space interval is: `[lower, upper)` with size: `free_bytes = upper - lower`.

Because every slot entry is 8 bytes: `lower = 48 + slot_count * 8`.

Valid page geometry requires: `48 <= lower <= upper <= PAGE_SIZE`.

Slot entries grow upward from the fixed heap header. Tuple bytes grow downward from the end of the page.

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

Once Chapter 14 has made a slot safely reusable, the architecture uses an intrusive singly linked free-slot list.

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

Every safely reusable `UNUSED` slot MUST appear exactly once in this list. `DEAD` slots are never linked into the free-slot list. The existence of `free_slot_head` therefore does not authorize immediate `DEAD` reuse; Chapter 14 controls the only transition that can add a retired slot to this list.

### 5.3.3 Reserved and hint fields

For HEAP_DATA format v1:

```text
common flags      = 0
common reserved16 = 0
heap reserved     = 0
```

Structural decoding/validation MUST reject a nonzero value in any of these unassigned/reserved fields. Assigning semantics to either reserved field requires compatible version handling. `prune_hint` is initialized to zero.

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

In the heap-page format, the 16-bit field at slot offset `4` is interpreted using these persisted slot-state codes:

| Code | State |
|---:|---|
| `0` | `UNUSED` |
| `1` | `NORMAL` |
| `2` | `DEAD` |
| `3` | `REDIRECT_RESERVED` |

These numeric codes are persistent-format values. Existing codes MUST NOT be renumbered because of source-language enum order. Future states require new explicit numeric codes or an explicit compatible format revision. A persisted slot-state value outside this set is structurally invalid in the heap-page format.

### 5.4.1 NORMAL slots

A newly created `NORMAL` slot MUST persist: `aux = 0`.

The baseline insertion path assigns no other meaning to `aux` for `NORMAL` slots. Giving `aux` operational meaning for `NORMAL` slots requires an explicitly compatible rule or coordinated page-format revision. A `NORMAL` slot MUST reference tuple bytes wholly inside the tuple-data region and inside the page.

### 5.4.2 UNUSED and REDIRECT_RESERVED slots

For the defined architecture, `UNUSED` slots have `aux` as the free-slot next pointer defined by §5.3.2 and tuple coordinates exactly zero.

`REDIRECT_RESERVED` is the recognized numeric reservation defined by §4.13.3. V1 assigns it no tuple-range or `aux` semantics, writers MUST NOT emit it, and ordinary owner validation returns `UNSUPPORTED_RESERVED_STATE` rather than interpreting or skipping it.

### 5.4.3 DEAD slots and physical reclamation

A `NORMAL -> DEAD` transition does not itself reclaim tuple bytes or make the SlotId reusable.

Before compaction, a `DEAD` slot MAY retain its previous:

```text
tuple_offset
tuple_length
```

while those tuple bytes remain physically present and continue to pass the complete retained-tuple validation in §4.13.3. Because the originating NORMAL form has `aux=0`, the DEAD form also has `aux=0`.

After compaction has physically discarded the payload of a `DEAD` slot, the canonical persisted coordinates are:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = 0
```

Clearing the coordinates prevents the reclaimed `DEAD` slot from pointing into newly free space or bytes moved for another tuple. Compaction alone does **not** change `DEAD` to `UNUSED` and does not link the slot into the free list.

Only the Chapter-14 grace-complete reclamation protocol may atomically convert the canonical zero-coordinate `DEAD` slot to `UNUSED` and install the free-list link.

### 5.4.4 Stable SlotId

Heap-page compaction MAY change `slot.tuple_offset` but MUST NOT change `SlotId`.

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

Popping the free-slot list and installing the new NORMAL slot occur in the same page mutation/WAL unit. Page compaction MAY be used when total reclaimable space is sufficient but the current contiguous free-space interval is not.

Compaction MUST preserve stable SlotId values and the canonical DEAD-slot rules in §5.4.3. Compaction may discard payload bytes for a `DEAD` slot before its final transition to `UNUSED`; it does not independently decide that the RID reuse grace period has completed.

## 5.6 Maximum inline tuple size

Tuple storage is inline-only: one tuple must fit entirely inside one heap page. Overflow/TOAST-style tuple storage is not part of the heap format.

For the page geometry:

```text
PAGE_SIZE         = 8192
heap total header = 48
slot entry        = 8
```

the maximum accepted complete encoded tuple length is: `8135 bytes`.

The bound is intentionally strict. A complete encoded tuple length of: `8136 bytes`.

MUST be rejected even though that size would make the slot directory and tuple region meet exactly with zero bytes remaining. The 8135-byte bound is a complete encoded tuple length limit for heap-page storage. It is not a promise that every byte string up to that size is a semantically valid encoded SQL tuple.

Schema-directed tuple encoding/validation may reject values for independent tuple-format reasons. An oversized tuple insertion MUST produce `ROW_TOO_LARGE` rather than truncating or corrupting the page. Overflow/large-object storage is deferred from the architecture baseline.

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

`header_bytes` covers only the fixed 48-byte tuple-header prefix. It does not include the null bitmap. Tuple-header bytes `44..47` MUST be written as zero and MUST be zero when decoding the tuple header. Assigning them semantics requires a coordinated format revision.

### 5.7.1 xmin

`xmin` identifies the transaction that created the physical tuple version.

### 5.7.2 xmax

`xmax` identifies the transaction that invalidated, deleted, or superseded the physical tuple version. The representation for no invalidating transaction is: `xmax = INVALID_TXN_ID = 0`.

### 5.7.3 cmin and cmax

`cmin` and `cmax` are command identifiers within a transaction. They exist because transaction identity and statement/command visibility are distinct concepts. `CommandId` value `0` is valid and MUST NOT be interpreted as an invalid sentinel.

For every persisted tuple header, the canonical no-deleter representation is:

```text
xmax = INVALID_TXN_ID
cmax = 0
```

Therefore `xmax == INVALID_TXN_ID` requires `cmax == 0`. `xmax` is the
discriminator for whether a deleting transaction exists, so `cmax` carries no
independent MVCC meaning in this branch. The zero does not mean deletion by
`CommandId{0}`; it is the canonical value of the semantically inactive field.

Writers MUST establish this pair before publishing a recognized-v1 tuple.
A persisted tuple with `xmax == INVALID_TXN_ID` and nonzero `cmax` is
noncanonical heap metadata and MUST be classified as `CORRUPT_HEAP`; ordinary
reads neither normalize it nor treat it as an unsupported future format.
Requiring one representation prevents stale command bytes and makes page
bytes, checksums, WAL page images, and structural validation deterministic.

The exact visibility interpretation of `xmin`, `xmax`, `cmin`, and `cmax` is defined by §§9.6–9.7 and 10.2–10.4.

### 5.7.4 Previous-version pointer

The pair: `(prev_page_no, prev_slot)` points to the immediately preceding physical tuple version in the **same heap file**.

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

A mixed sentinel/non-sentinel pair is structurally invalid. The pointer creates a backward physical version chain. That chain supports vacuum and version-history inspection without placing relation `FileId` into every tuple header. HOT-style optimization remains outside the supported scope.

## 5.8 Tuple flags v1

`tuple_flags` stores physical tuple-version/layout facts.

Tuple-header v1 assigns these bits:

| Bit mask | Meaning |
|---:|---|
| `0x0001` | `HAS_NULLS` |
| `0x0002` | `HAS_VARLEN` |

The known mask is `0x0003`.

Every other bit is invalid in tuple-header v1. Encoding and decoding MUST reject unknown bits rather than silently preserving them.

Existing bit assignments MUST NOT be renumbered. Future flags require a new explicitly compatible owning tuple grammar/version under §4.14; compaction/vacuum never clears an unknown bit and rewrites the tuple as known state.

### 5.8.1 HAS_VARLEN

For tuple format v1:

```text
HAS_VARLEN is set
    iff
the interpreting physical schema contains at least one VARCHAR column
```

This is a property of the tuple's physical schema/layout. It does not depend on the number of variable payload bytes in a particular tuple.

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

They MUST NOT be persisted as ad-hoc substitutes for transaction status. A future physical tuple flag requires well-defined physical, recovery, and visibility semantics before it becomes part of the persisted format.

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

The interpreting schema owns a precomputed physical layout description. For tuple format v1, that physical layout is derived deterministically from the physical schema.

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

There is no alignment padding and no gap between fixed fields/descriptors. Every fixed offset is tuple-relative. The same physical schema therefore derives the same fixed offsets independently of compiler ABI or host alignment.

Execution SHOULD NOT rediscover one column's physical offset by repeatedly walking all preceding columns. The persisted layout is compact and MAY contain fields that are not naturally aligned for the host machine. Serialization/deserialization MUST use unaligned-safe byte operations such as explicit endian helpers and `memcpy`-style bit transfer rather than undefined-behavior pointer casts.

The execution engine MAY decode persisted values into naturally aligned vectors. Persisted storage layout and execution layout are intentionally distinct representations.

### 5.9.1 Exact tuple length

For tuple format v1, physical length is canonical and exact:

```text
tuple_size =
    MinimumTupleSize()
    + sum(payload_length of each non-NULL VARCHAR)
```

No unreferenced trailing bytes are permitted. Schema-directed validation MUST finish with the canonical varlen payload cursor exactly equal to the supplied tuple byte length. This rule also applies to fixed-only schemas: a fixed-only tuple has exactly `MinimumTupleSize()` bytes.

## 5.10 Null bitmap

Tuple format v1 allocates exactly one null bit for **every physical schema column**, including a column declared `NOT NULL`. For physical column index `i`: `null_bit_index(i) = i`.

The byte count is:

```text
null_bitmap_bytes =
    column_count / 8
    + (column_count % 8 != 0 ? 1 : 0)
```

equivalently: `ceil(column_count / 8)`.

The resulting byte count MUST fit the persisted 16-bit `null_bitmap_bytes` tuple-header field. A zero-column physical schema has: `null_bitmap_bytes = 0`.

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

Bitmap allocation is independent of whether a column is declared nullable. A `NOT NULL` column still owns a physical null bit. Low-level bitmap primitives do not enforce SQL `NOT NULL` constraints. Schema-directed tuple validation MUST reject a persisted NULL bit for a column declared `NOT NULL` in the schema version interpreting that tuple.

Such a supported-v1 tuple is malformed heap data and MUST be classified as `CORRUPT_HEAP`.

### 5.10.4 Unused bitmap bits

Unused high bits in the final bitmap byte MUST be zero. Schema-directed validation MUST reject nonzero unused high bits.

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

A v1 writer MUST deterministically write zero into the fixed-area bytes reserved for a NULL fixed-width column. While the null bit is set, those bytes are semantically unread. A v1 reader is **not** required to reject nonzero bytes beneath a NULL bit.

This distinguishes canonical writer output from the minimum reader-validity contract.

## 5.11 Fixed-width physical values

The scalar widths are:

| Physical type | Width | Persisted representation |
|---|---:|---|
| `BOOLEAN` | 1 byte | exact byte `0x00` or `0x01` |
| `INT32` | 4 bytes | signed two's-complement bit pattern |
| `INT64` | 8 bytes | signed two's-complement bit pattern |
| `FLOAT64` | 8 bytes | exact IEEE-754 binary64 payload bits |
| `DATE` | 4 bytes | signed physical scalar |
| `TIMESTAMP` | 8 bytes | signed physical scalar |

Multi-byte values are encoded little-endian. Persistent scalar encoding MUST NOT depend on native C++ object layout or alignment.

### 5.11.1 BOOLEAN

The only valid persisted BOOLEAN bytes are:

```text
false = 0x00
true  = 0x01
```

Any other byte is invalid BOOLEAN data in tuple format v1.

### 5.11.2 Signed scalar representations

`INT32`, `INT64`, `DATE`, and `TIMESTAMP` persist their fixed-width signed two's-complement bit patterns and then encode those bits little-endian. `DATE` is physically a signed 32-bit scalar. `TIMESTAMP` is physically a signed 64-bit scalar.

Their SQL-level epoch, unit, precision, calendar, and time-zone semantics are not defined by the storage codec and remain owned by the SQL type layer.

### 5.11.3 FLOAT64

`FLOAT64` persists the exact IEEE-754 binary64 payload bits as a 64-bit value encoded little-endian.

The storage codec MUST preserve all 64 payload bits, including:

- `+0.0` versus `-0.0`,
- positive and negative infinity,
- NaN sign,
- quiet/signaling state,
- NaN payload bits.

Tuple format v1 does not canonicalize NaNs. Bit-preserving conversion plus explicit integer-endian encoding is appropriate; raw ABI-dependent floating-point object serialization is not.

## 5.12 VARCHAR representation

Every physical VARCHAR column owns an 8-byte descriptor in the tuple fixed area:

| Descriptor-relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `payload_offset` |
| `4` | 4 | `payload_length` |

Both fields are unsigned 32-bit little-endian integers. `payload_offset` is absolute relative to tuple byte `0`. VARCHAR payload bytes are stored inline in the same tuple. No terminator byte is stored; length is explicit.

For every non-NULL VARCHAR, the descriptor MUST point at or after `VarlenPayloadOffset()` and checked evaluation of: `payload_offset + payload_length`.

MUST remain within the exact physical tuple length.

### 5.12.1 NULL VARCHAR

A NULL VARCHAR is identified by its null bit and MUST persist the canonical descriptor:

```text
payload_offset = 0
payload_length = 0
```

Schema-directed validation MUST reject any other descriptor under a set NULL bit. A NULL VARCHAR contributes zero payload bytes.

### 5.12.2 Present VARCHAR packing

Present VARCHAR payloads are packed consecutively in physical schema-column order beginning at `VarlenPayloadOffset()`.

Validation maintains an expected payload cursor.

For each present VARCHAR:

```text
descriptor.payload_offset = expected_payload_offset
expected_payload_offset += descriptor.payload_length
```

Therefore the architecture permits no:

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

Those semantics belong to higher layers. Large/overflow values remain deferred; the complete tuple must satisfy the inline tuple-size limit.

## 5.13 Schema versioning

Every physical tuple version stores `schema_version`.

Tuple interpretation uses the catalog contract:

```text
ResolveSchema(TableId, tuple.schema_version)
    -> immutable SchemaDescriptor
```

defined in Chapter 16. Tuple writers emit `SchemaVer{1}`, and `SchemaVer{0}` is invalid. Any persisted schema version that may still occur in tuple storage MUST remain historically resolvable. The field is present in tuple format v1 so an explicit architecture revision can define schema evolution without silently changing the tuple representation.

Schema-changing ALTER and version translation/tuple rewriting are deferred from the architecture baseline. Producing additional SchemaVer values requires an explicit architecture revision.

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

The FSM is advisory. A stale FSM candidate MUST NOT make insertion incorrect; actual heap-page free space is checked after fetching/latching the page. The exact ordering of WAL creation, page mutation, dirty-state publication, and page-LSN updates is defined by §§7.10.1, 12.11–12.12, and 15.2. This chapter does not preempt that ordering.

## 5.15 UPDATE protocol boundary

The UPDATE storage model appends a new physical tuple version.

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

User-visible tuple bytes are not updated in place in the defined architecture. The old version MUST remain physically available while a legal snapshot may still require it. The heap layer MAY prefer placing the new version on the same page when sufficient space exists.

Same-page placement is a locality optimization, not an invariant. The detailed transaction visibility, command-ID handling, locking, WAL, and abort behavior is defined by §§9.6, 10.2–10.4, 12.12, 15.3, 15.6, and 39.1.

## 5.16 DELETE protocol boundary

The baseline DELETE operation is logical before physical reclamation. Conceptually: `set the visible tuple version's xmax`.

The tuple bytes and corresponding physical index entries remain present. Physical reclamation occurs during vacuum only after the global visibility/reclamation rules determine that no legal snapshot can require the version.

## 5.17 MVCC visibility boundary

`HeapPage` does **not** decide SQL visibility. `HeapPage` exposes physical tuple versions and their physical metadata.

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

`HeapPage` is a lightweight page-format view over caller-owned page bytes. It MUST NOT independently own another 8192-byte page allocation merely to interpret a page already resident in the BufferPool.

Architecturally, `HeapPage` provides responsibilities equivalent to:

- formatting/initializing a heap page over supplied page bytes,
- opening/validating an existing heap page,
- inserting raw tuple bytes into a slot,
- obtaining a physical tuple view for a slot,
- marking a slot `DEAD` when a higher layer has authorized that transition,
- compacting already-reclaimable physical payload,
- reporting contiguous/reclaimable page-local space,
- iterating physical slots.

The exact C++ method names and signatures are not part of the persistent or subsystem contract. Page-format algorithms operate on caller-owned resident bytes. When those bytes come from BufferPool, every `HeapPage`/tuple/VARCHAR view remains borrowed under the guard-lifetime contract in §7.7.2.

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

`HeapPage` MUST NOT need to know that a particular physical column is, for example, a VARCHAR. Schema-directed tuple interpretation belongs to the tuple codec/layout layer.

## 5.20 Tuple views and execution decoding

Storage code SHOULD avoid unnecessary copies by exposing immutable tuple views into resident page bytes when lifetime safety can be maintained. A tuple view into BufferPool-backed storage MUST NOT outlive the page guard/pin that keeps its backing frame resident.

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
2. Heap common `reserved16` and heap-specific `reserved` are zero in the defined architecture.
3. `lower = 48 + slot_count * 8`.
4. Valid geometry satisfies `48 <= lower <= upper <= PAGE_SIZE`.
5. A slot entry is exactly 8 bytes.
6. Persisted slot-state codes are fixed as `UNUSED=0`, `NORMAL=1`, `DEAD=2`, `REDIRECT_RESERVED=3`.
7. Unknown persisted slot-state codes are invalid in the defined architecture.
8. A new `NORMAL` slot has `aux=0`.
9. Every `UNUSED` slot has zero tuple coordinates and uses `aux` as the next free SlotId/sentinel.
10. `free_slot_head` is either `INVALID_SLOT_ID` or the head of an acyclic in-range chain containing every reusable UNUSED slot exactly once.
11. Every retained NORMAL/DEAD tuple range passes complete tuple validation and is disjoint from every other retained tuple range; §4.13.3 is the canonical page-local contract.
12. `REDIRECT_RESERVED` is recognized but unsupported: v1 writers do not emit it and ordinary readers reject the containing page without guessing semantics.
13. Page compaction does not change `SlotId`.
14. Physically reclaimed `DEAD` slots canonicalize tuple coordinates to `(0,0)` and `aux=0` but remain `DEAD` until the grace-complete reuse transition.
15. A `DEAD` slot is never linked into the reusable free-slot list.
16. The maximum accepted raw tuple payload is `8135` bytes; `8136` is rejected.
17. A tuple header is exactly 48 bytes; `header_bytes=48` and tuple reserved bytes `44..47` are zero.
18. `CommandId{0}` is valid.
19. The previous-version pointer is either two invalid sentinels or two non-sentinels, and always refers within the same heap file.
20. The tuple-flags known mask is `0x0003`; unknown bits are invalid.
21. `HAS_VARLEN` exactly reflects whether the interpreting physical schema contains VARCHAR.
22. Tuple physical length is exact; trailing unreferenced bytes are invalid.
23. Every physical schema column owns one LSB-first null bit.
24. Unused high null-bitmap bits are zero.
25. `HAS_NULLS` exactly reflects whether any used null bit is set.
26. Schema-directed validation rejects a NULL bit for a `NOT NULL` column.
27. BOOLEAN accepts only `0x00` and `0x01`.
28. FLOAT64 preserves exact IEEE-754 binary64 payload bits and does not canonicalize NaNs.
29. A NULL VARCHAR descriptor is exactly `(0,0)`.
30. Present VARCHAR payloads are packed consecutively in physical schema order with no gaps or overlaps.
31. Present empty VARCHAR is distinct from NULL.
32. A persisted no-deleter tuple header has `xmax=INVALID_TXN_ID` and `cmax=0`; a nonzero `cmax` in that branch is `CORRUPT_HEAP`.
33. `HeapPage` owns physical page mechanics, not SQL visibility.
34. Physical heap scan order does not imply SQL result ordering.
