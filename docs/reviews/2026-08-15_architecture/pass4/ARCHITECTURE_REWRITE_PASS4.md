# Rewrite Pass 4 — Free-Space Management and Physical Reclamation

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `82..85`
- processed source lines: `2945..3396`
- legacy architecture modified: **no**
- production code modified: **no**
- I/O/BufferPool §86+ migrated: **no**

Pass 4 replaces the Chapter 6 baseline with the canonical FSM and page-local physical-reclamation contract.

## Canonicalized FSM contract

### File organization

Each heap relation has:

```text
table_<table_id>.fsm
```

with:

```text
page 0       FileSuperblock / FileKind::FSM
page 1..N    FSM_DATA
```

The FSM stores one persisted one-byte category for every ordinary heap data page.

### Category semantics

The category input is:

```text
free_bytes = upper - lower
```

The v1 maximum contiguous gap is:

```text
8144 bytes
```

The category deliberately assumes that a new 8-byte slot entry is required:

```text
bounded_free =
    min(free_bytes, 8144)

usable_insertion_bytes =
    min(max(bounded_free - 8, 0), 8135)

category =
    floor(usable_insertion_bytes * 255 / 8135)
```

All arithmetic is integer arithmetic.

The exact representative boundaries are retained:

```text
free 0,1,8,9,39 -> 0
free 40         -> 1
free 4075       -> 127
free 8142       -> 254
free 8143,8144  -> 255
```

The conservative 8-byte deduction remains even if a later insertion implementation sometimes reuses a slot.

### Inverse lower bound

Canonical inverse:

```text
minimum_usable(c) =
    ceil(c * 8135 / 255)
```

or:

```text
(c * 8135 + 254) / 255
```

Representative values:

```text
0   -> 0
1   -> 32
127 -> 4052
254 -> 8104
255 -> 8135
```

This remains explicitly an inclusive lower bound, not a bucket midpoint or upper bound.

### Candidate-search boundary

Legacy §§82–85 permit a runtime bucketed candidate accelerator and state that insertion asks for a candidate capable of the requested size.

They do not lock the exact runtime data structure, tie-breaker, or request-to-category search formula.

Pass 4 does not import the current implementation's in-memory candidate-index details from devlogs/PROJECT_STATE into the architecture rewrite.

The only mandatory correctness behavior is:

```text
candidate
-> fetch + latch
-> verify actual heap geometry
```

with repair/retry when stale.

### FSM_DATA v1

Canonical layout:

```text
0..31     common page header
32..39    first_heap_page_no  uint64 LE
40..41    entry_count         uint16 LE
42..43    reserved16 = 0
44..47    reserved32 = 0
48..8191  8144 category bytes
```

Total:

```text
8192 bytes
```

Header:

```text
48 bytes total
16 bytes FSM-specific
```

All FSM/common reserved fields identified by §82 are zero and rejected if nonzero.

Every initialized category byte `0..255` is valid.

There is no persisted hierarchy, tree, pointer, or per-entry metadata in v1.

### Mapping

For heap page H:

```text
heap_data_index = H - 1
fsm_page_no     = 1 + heap_data_index / 8144
entry_index     = heap_data_index % 8144
```

Examples:

```text
heap 1    -> FSM 1 / entry 0
heap 8144 -> FSM 1 / entry 8143
heap 8145 -> FSM 2 / entry 0
```

For FSM page P:

```text
first_heap_page_no =
    1 + (P - 1) * 8144
```

That persisted field must equal the deterministic mapping.

Mapping/range arithmetic is checked for overflow.

FSM page 0 and INVALID_PAGE_NO are invalid FSM_DATA page numbers; heap page 0 and INVALID_PAGE_NO are invalid ordinary mapped heap targets.

### entry_count

```text
0 <= entry_count <= 8144
```

Initialized entries are exactly:

```text
[0, entry_count)
```

Every suffix byte at:

```text
index >= entry_count
```

is uninitialized and must remain zero.

Nonzero suffix bytes are structurally invalid.

Access/update outside the initialized prefix is rejected.

### Initialization

Blank FSM_DATA initialization zeroes the whole page first.

Before WAL:

```text
page_type       = FSM_DATA
format_version  = 1
header_size     = 48
page_no         = actual PageId.page_no
flags           = 0 unless explicitly supplied
page_lsn        = INVALID_LSN unless explicitly supplied
checksum_crc32c = 0
reserved16      = 0
```

FSM-specific fields receive deterministic first_heap_page_no, caller-selected valid entry_count, and zero reserved fields.

All category bytes begin zero.

Ordinary pre-WAL FSM mutation does not advance page_lsn or update/generate whole-page CRC32C.

## Persistence and rebuild semantics

FSM metadata is advisory.

A stale category may create extra I/O, miss a currently usable candidate, or require later repair; it must never bypass actual HeapPage verification.

The engine must be able to rebuild/repair FSM state by scanning heap-page headers.

Persisted FSM authority is limited to:

- deterministic addressing,
- initialized-prefix metadata,
- category-byte interpretation,
- page-format validity.

It is not authoritative for current insertion success.

## Compaction boundary

Heap-page compaction may move tuple bytes only while holding the exclusive page latch and preserving SlotIds/RIDs.

Only already-persisted DEAD payloads may be physically discarded by compaction.

After discard:

```text
offset = 0
length = 0
state  = DEAD
aux    = preserved
```

DEAD remains non-reusable.

Retained NORMAL ranges must not overlap for compaction to proceed.

The source permits this as a compaction precondition rather than requiring it as a universal HeapPage validation rule at this point.

Page-local compaction cannot decide global MVCC reclamation safety.

## Vacuum physical-reclamation boundary

The conceptual responsibility sequence is preserved:

```text
1. safe visibility horizon
2. globally dead tuple-version identification
3. required index cleanup
4. mark heap slots reclaimable/dead
5. compact pages
6. update FSM
```

The crash-safe WAL ordering remains deferred to later transaction/recovery architecture.

A fully empty heap page remains reusable database space; shrinking the OS file is not required.

Pass 4 does not define physical RID reuse beyond the already-established later-safe-reuse boundary.

## Issue register

No new unresolved architecture issue was created.

R-010 was updated because §82.6 explicitly refers to a future FSM “whole-page CRC32C checksum”. This strengthens the evidence for whole-page FSM coverage but still does not establish the universal checksum byte-range contract for every ordinary page type, so R-010 remains open.

## Coverage result

```text
legacy §§0..85     migrated / explicitly disposed
legacy §§86..725   pending
```

All four Pass-4 sections are complete.

No §86+ row was marked migrated.

## Validation

Mechanical validation confirmed:

- pinned legacy SHA-256 unchanged,
- §82 starts at line 2945,
- §86 starts at line 3397,
- sections 82–85 all exist,
- all coverage rows through §85 are non-PENDING,
- all rows from §86 onward remain PENDING,
- exact category formulas/boundaries and FSM layout values exist in Chapter 6,
- Chapter 6 occurs once,
- legacy ARCHITECTURE.md was not modified,
- no production source file was modified.

## Pass 4 exit status

**COMPLETE.**

Pass 5 should process only legacy §§86–108 and replace Chapter 7 with the canonical I/O, BufferPool, page-lifetime, ownership, and dependency-boundary contract while separating implementation-roadmap/module-layout material from architecture.
