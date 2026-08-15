# Rewrite Pass 3 — Heap Storage and Tuple Format

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `64..81`
- processed source lines: `1936..2944`
- legacy architecture modified: **no**
- production code modified: **no**
- FSM §82+ migrated: **no**

Pass 3 replaces the high-level Chapter 5 baseline with the canonical heap-page and tuple-format contract.

## Canonicalized heap storage

### Heap relation structure

Preserved:

```text
table_<table_id>.heap
table_<table_id>.fsm
```

and:

```text
heap page 0    superblock
heap page 1..N data pages
```

The heap remains optimized for straightforward physical sequential scanning; free-space metadata stays separate.

### Physical scan order

A full heap scan visits:

```text
ascending page_no
then ascending SlotId
```

MVCC visibility is applied afterward.

This physical traversal order does not create SQL ordering semantics.

### HEAP_DATA v1

Canonical common-header requirements:

```text
page_type      = HEAP_DATA
format_version = 1
header_size    = 48
reserved16     = 0
page_no        = logical PageId.page_no
```

Exact heap-specific fields:

```text
32..33 slot_count
34..35 free_slot_head
36..37 lower
38..39 upper
40..43 prune_hint
44..47 reserved
```

All multi-byte fields are little-endian.

Canonical geometry:

```text
lower = 48 + slot_count * 8
48 <= lower <= upper <= PAGE_SIZE
free_bytes = upper - lower
```

The v1 heap-specific reserved field and common reserved16 are zero and rejected if nonzero.

`prune_hint` begins at zero and remains semantically deferred.

### Slot directory

Exact slot width:

```text
8 bytes
```

Exact fields:

```text
0..1 tuple_offset
2..3 tuple_length
4..5 persisted slot state
6..7 aux
```

Persisted state codes:

```text
0 UNUSED
1 NORMAL
2 DEAD
3 REDIRECT_RESERVED
```

Unknown v1 state codes are invalid.

New NORMAL slots have:

```text
aux = 0
```

A NORMAL->DEAD transition does not reclaim payload bytes.

After compaction discards DEAD payload:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved
```

This does not produce UNUSED/reusable state.

SlotId remains stable across compaction.

### Free-space geometry

Slot directory grows upward; tuple bytes grow downward.

Insertion must account for tuple bytes plus a new slot entry when no separately safe reusable slot exists.

Compaction can restore contiguous free space without changing SlotIds.

### Raw tuple maximum

Canonical v1 bound:

```text
8135 bytes accepted maximum
8136 bytes rejected
```

The strict one-byte gap versus exact physical meeting is preserved.

This is a raw heap-storage bound rather than semantic tuple validity.

Overflow/TOAST-style storage is deferred.

## Canonicalized tuple format

### Tuple header

Exact width:

```text
48 bytes
```

Exact layout:

```text
0..7    xmin
8..15   xmax
16..19  cmin
20..23  cmax
24..31  prev_page_no
32..33  prev_slot
34..35  tuple_flags
36..37  header_bytes
38..39  null_bitmap_bytes
40..43  schema_version
44..47  reserved
```

All multi-byte fields are little-endian.

Canonical v1 fields:

```text
header_bytes = 48
reserved     = 0
```

`CommandId0` remains valid.

No-xmax uses:

```text
INVALID_TXN_ID = 0
```

The previous-version pointer references the same heap file and is valid only as either:

```text
(INVALID_PAGE_NO, INVALID_SLOT_ID)
```

or a pair of two non-sentinel values.

### Tuple flags

```text
HAS_NULLS  = 0x0001
HAS_VARLEN = 0x0002
known mask = 0x0003
```

Unknown bits are invalid.

`HAS_VARLEN` is canonical from physical schema shape, not actual payload length.

Deferred flags are preserved as deferred rather than assigned persisted codes.

### Physical body layout and exact length

Canonical order:

```text
48-byte header
null bitmap
fixed area
varlen payload
```

Exact length:

```text
MinimumTupleSize()
+ sum(non-NULL VARCHAR payload lengths)
```

Trailing unreferenced bytes are invalid.

Persisted fields may be unaligned; storage access must be unaligned-safe and explicit.

### Null bitmap

One bit exists for every physical column, including NOT NULL columns.

```text
null_bitmap_bytes = ceil(column_count / 8)
```

with a zero-column schema using zero bytes.

Bits are LSB-first:

```text
byte = column_index / 8
bit  = column_index % 8
```

Bit meaning:

```text
1 NULL
0 present
```

Unused high bits are zero.

`HAS_NULLS` is set iff at least one used null bit is set.

Schema-directed validation rejects NULL for a NOT NULL column.

Canonical writers zero fixed bytes hidden under a NULL bit; readers need not reject nonzero hidden bytes.

### Fixed values

```text
BOOLEAN    1 byte
INT32      4 bytes
INT64      8 bytes
FLOAT64    8 bytes
DATE       4 bytes
TIMESTAMP  8 bytes
```

BOOLEAN:

```text
false 0x00
true  0x01
```

Other BOOLEAN bytes are invalid.

INT32/INT64/DATE/TIMESTAMP persist fixed-width signed two's-complement bits little-endian.

DATE/TIMESTAMP SQL semantic units remain outside storage.

FLOAT64 preserves exact IEEE-754 binary64 payload bits including signed zero, infinities, NaN sign/state/payload; NaNs are not canonicalized.

### VARCHAR

Every VARCHAR has an 8-byte fixed-area descriptor:

```text
uint32 payload_offset
uint32 payload_length
```

Both little-endian.

Offset is tuple-relative.

NULL descriptor:

```text
(0, 0)
```

Present payloads are packed consecutively in physical schema order starting at `VarlenPayloadOffset()`.

No gaps, overlaps, backtracking, reordering, or references into non-payload regions are permitted.

Present empty VARCHAR is distinct from NULL and may share the current zero-length cursor with another empty value.

Payload bytes are opaque to storage; UTF-8, collation, locale, character-count, VARCHAR(n), and terminator semantics are not storage-layer rules.

### Schema version

Every tuple carries `schema_version`.

The first implementation may support only version `1`; schema-changing DDL may remain unsupported until version translation exists.

## Page-local / cross-subsystem write boundaries

### INSERT

The complete conceptual sequence from legacy §78 is preserved, including:

- tuple sizing,
- FSM candidate lookup,
- BufferPool fetch,
- exclusive page latch,
- actual free-space verification,
- optional compaction,
- slot allocation,
- tuple/slot mutation,
- later page-LSN handling,
- dirty marking,
- guard/latch release,
- FSM estimate update,
- index entry creation.

FSM remains advisory.

Pass 3 explicitly does not grant immediate DEAD-slot reuse merely because §78 says “allocate/reuse slot”; §§66–67 reserve actual reuse semantics.

### UPDATE

V1 creates a complete new physical tuple version with:

```text
new.xmin
new.prev = old RID
old.xmax
new RID
```

and new index entries as needed.

Old tuple/index versions remain until vacuum.

User-visible tuple bytes are not updated in place.

Same-page placement is optional locality only.

### DELETE

DELETE first sets the visible version's `xmax`.

Physical tuple bytes and index entries remain until safe vacuum reclamation.

### Visibility boundary

HeapPage exposes physical bytes and tuple metadata.

It does not decide snapshot visibility.

Visibility is determined by transaction/MVCC state using tuple header metadata.

## Editorial/refinement issue recorded

R-011 records that legacy §78 uses the phrase “allocate/reuse slot” before the page-format sections define reuse eligibility.

The rewrite resolves this without a new architecture decision:

```text
allocate a slot
reuse only when a separately defined safe-reuse protocol says a slot is eligible
```

No DEAD->UNUSED policy is invented in Pass 3.

## Coverage result

```text
legacy §§0..81     migrated / explicitly disposed
legacy §§82..725   pending
```

All 18 Pass-3 sections have non-PENDING status.

No FSM §82+ section was marked migrated.

## Validation

Mechanical checks verify:

- the pinned legacy SHA-256 is unchanged,
- §64 starts at line 1936,
- §82 starts at line 2945,
- every source section 64..81 exists,
- exact heap/page/slot/tuple widths and constants appear in Chapter 5,
- all legacy rows through §81 are complete,
- all rows from §82 onward remain pending,
- Chapter 5 appears exactly once,
- no production source file was modified,
- legacy `ARCHITECTURE.md` was not modified.

## Pass 3 exit status

**COMPLETE.**

Pass 4 should process only legacy §§82–85 and turn Chapter 6 into the canonical Free-Space Management and Physical Reclamation contract.
