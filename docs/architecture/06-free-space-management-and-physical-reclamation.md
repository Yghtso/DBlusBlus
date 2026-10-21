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

- the persisted free-space category,
- the exact category mapping and inverse lower-bound interpretation,
- the `FSM_DATA` page format,
- deterministic heap-page-to-FSM-entry addressing,
- initialized-prefix semantics,
- FSM page initialization,
- runtime acceleration boundaries,
- staleness, repair, and rebuild behavior,
- heap-page compaction constraints,
- vacuum's physical-reclamation boundary.

I/O ownership and BufferPool mechanics are defined by §§7.3–7.12, WAL-backed
page-mutation publication by §12.12, and crash recovery by §§13.11–13.19.

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

Larger category values represent greater complete encoded tuple insertion capacity under the mapping.

All byte values `0..255` are valid categories for initialized entries.

## 6.3 Free-space category semantics

The category conversion input is the current contiguous free-space gap of a structurally valid heap page:

```text
free_bytes = upper - lower
```

For the heap-page layout:

```text
maximum contiguous gap
    = PAGE_SIZE - HEAP_PAGE_TOTAL_HEADER_SIZE
    = 8192 - 48
    = 8144 bytes
```

The persisted category represents **conservative complete encoded tuple insertion capacity assuming that a new 8-byte slot entry is required**.

The mapping first computes:

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
8135 = maximum v1 complete encoded tuple length
```

The category is then:

```text
category =
    floor(usable_insertion_bytes * 255 / 8135)
```

The conversion uses integer arithmetic only.

Inputs above the physical 8144-byte contiguous-gap maximum are clamped to 8144 before the slot-cost deduction.

The mapping is monotonic.

The mapping deliberately reserves the 8-byte new-slot cost even when insertion
can reuse an architecture-authorized `UNUSED` slot. A reusable slot can
therefore make actual insertion capacity greater than the category's guarantee.
This conservative underestimation is safe because actual heap geometry remains
authoritative and every FSM candidate is rechecked before mutation. Persisted
FSM metadata MUST NOT overstate guaranteed insertion capacity based on slot
reuse.

### 6.3.1 Representative boundaries

The following values are part of the mapping contract:

```text
free_bytes 0, 1, 8, 9, 39 -> category 0
free_bytes 40             -> category 1
free_bytes 4075           -> category 127
free_bytes 8142           -> category 254
free_bytes 8143, 8144     -> category 255
```

## 6.4 Category lower-bound interpretation

For a persisted category `c`, the inverse helper is an **inclusive lower bound** on represented usable complete encoded tuple bytes.

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

The architecture does not prescribe a specific runtime tie-breaker, exact bucket-search algorithm, or in-memory candidate-index representation.

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

The architecture has no hierarchical FSM level, tree node, persisted pointer, or per-entry metadata.

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

The initialized prefix represents a contiguous range of published heap pages
beginning at the page's persisted `first_heap_page_no`. It need not cover every
published heap page; a shorter prefix is structurally valid. The initialized
range MUST NOT extend beyond the paired heap file's authoritative published
page bound.

Entries satisfying:

```text
entry_index >= entry_count
```

are uninitialized entry storage.

In FSM_DATA v1, every byte in that uninitialized suffix MUST persist as zero.

Structural validation MUST reject a nonzero byte in the uninitialized suffix.

Entry access/update operations MUST reject an index outside the initialized prefix.

Architecture-authorized FSM maintenance MAY extend or repair the initialized
prefix as heap pages are published. Entries outside the prefix remain absent
from FSM candidate selection until that maintenance publishes them. This
ownership/lifecycle policy is separate from the persisted page format.

## 6.8 Blank FSM_DATA page initialization

Initializing a new FSM_DATA page first deterministically zeroes the entire
8192-byte page and then constructs the explicit common and FSM-specific headers
as a private page image under §§4.11.1 and 12.12.

The common-header fields are:

```text
page_type       = FSM_DATA
format_version  = 1
header_size     = 48
page_no         = actual FSM PageId.page_no
flags           = 0
reserved16      = 0
```

FSM-specific initialization writes:

```text
first_heap_page_no =
    deterministic value derived from FSM page number

entry_count =
    owner-selected initialized prefix length

reserved16 = 0
reserved32 = 0
```

`entry_count` is subject to the `0..8144` invariant in §6.7.

All category bytes initially remain zero. A zero byte inside the initialized
prefix is category `0`; bytes in the uninitialized suffix are canonical zero.

Before ordinary publication, the owning `PAGE_INIT` assigns `page_lsn` to its
record LSN and finalizes the valid whole-page checksum under §§4.11.1 and 12.9.
The page is not ordinarily searchable until that PAGE_INIT and the owning
published-page bound complete the §12.12 publication protocol. Subsequent
persistent FSM mutations and writeback obey §§7.10–7.11, 12.12, and 12.17.

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

The flat persisted FSM is authoritative only for:

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
aux          = 0
```

The following remain unchanged:

```text
slot_count
slot-directory positions
SlotId values
```

Compaction increases contiguous free space but does not make a `DEAD` slot reusable.

All retained NORMAL and DEAD ranges already satisfy §4.13.3's universal bounds, tuple-decoding, and pairwise non-overlap rules. Compaction revalidates the final local page state before publication.

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

The exact WAL publication and recovery ordering is owned by §§12.12 and 13.13;
vacuum reclamation and FSM maintenance are owned by §§14.5–14.12 and 14.16.

A heap page that becomes completely empty remains reusable database space.

The engine is not required to shrink the underlying operating-system file merely because a page becomes empty.

Physical slot reuse remains subject to the RID-reuse protocol in §§14.5–14.12;
vacuum eligibility and page compaction alone do not implicitly authorize
immediate RID reuse.

The same rule applies to whole-page recycling: an empty heap page may remain
reusable database space, but reinitializing its `PageNo` in a way that reuses
former `(PageNo, SlotId)` identities requires the physical RID-reuse safety
protocol in §§14.5–14.12.

## 6.13 FSM and reclamation invariants

1. Each heap relation has a separate page-based FSM file.
2. FSM page `0` is an FSM FileSuperblock; `FSM_DATA` pages begin at page `1`.
3. Each ordinary heap data page maps deterministically to one one-byte FSM category.
4. Category values are exactly `0..255`, with larger values representing greater conservative insertion capacity.
5. The category assumes an 8-byte new-slot cost and caps represented complete encoded tuple length at 8135 bytes.
6. `category = floor(usable_insertion_bytes * 255 / 8135)`.
7. `minimum_usable(c) = ceil(c * 8135 / 255)`.
8. FSM_DATA v1 is exactly 8192 bytes with a 48-byte total header and 8144 one-byte entries.
9. FSM common `reserved16`, FSM `reserved16`, and FSM `reserved32` are zero in the defined architecture.
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
21. FSM owner validation applies §4.13.6: deterministic mapping/prefix/suffix rules are structural, while category accuracy remains advisory.
