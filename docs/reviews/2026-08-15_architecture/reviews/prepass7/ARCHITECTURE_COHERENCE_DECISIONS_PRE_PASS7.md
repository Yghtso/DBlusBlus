# Pre-Pass-7 Coherence Decisions

## Owner-delegated criterion

The project owner delegated selection to the architecture option that provides the strongest learning value while remaining appropriate for a serious performance-oriented single-node database.

The chosen package is:

```text
D1-A  file-kind-specific BTREE superblock extension
D2-A  strict zero-only unassigned v1 ordinary-page flags
D3-A  compact schema-order no-padding tuple physical layout
D4-A  global INVALID_PAGE_NO sentinels + canonical B+ blank-page state
D5-B  optimistic root-generation validation/restart
D6-A  whole-page CRC32C
D7-A  exact conventional FLOAT64 sortable transform
```

The deliberate educational deviation from the earlier recommendation is `D5-B`.

## Why D5-B was selected

`D5-A` would have imposed a simpler metadata-before-page latch hierarchy.

`D5-B` instead teaches an important database concurrency technique:

```text
observe metadata
pin object
release metadata
latch object
revalidate generation/identity
restart on change
```

This creates useful exposure to:

- optimistic validation,
- structural generations,
- restartable algorithms,
- publication versus observation,
- avoiding latch-order cycles without a coarse tree-wide lock.

The extra complexity is localized and does not require a lock-free B+ tree.

## Exact decisions

### D1-A — BTREE superblock extension

The common FileSuperblock prefix remains bytes `0..71`.

V1 complete header sizes are:

```text
HEAP/FSM/CATALOG = 72
BTREE            = 128
```

BTREE extension:

```text
72..79    table_id
80..87    root_page_no
88..95    first_leaf_page_no
96..103   last_leaf_page_no
104..111  free_page_head
112..119  key_schema_fingerprint
120..123  key_schema_version = 1
124..125  tree_height
126..127  index_flags = 0
128..8191 zero
```

`object_id` in the common prefix is the `IndexId`.

### D2-A — strict ordinary-page flags

For a v1 page format with no assigned common flag bits:

```text
flags = 0
write zero
reject nonzero
```

The same zero/reject rule is used for currently unassigned B+ node-header, slot, and `index_flags` fields.

### D3-A — tuple physical layout

```text
fixed_area_offset = 48 + null_bitmap_bytes
fixed fields/descriptors packed in physical schema order
no padding
VARCHAR fixed width = 8-byte descriptor
VarlenPayloadOffset = final fixed cursor
MinimumTupleSize = VarlenPayloadOffset
```

### D4-A — B+ sentinels/canonical pages

```text
no prev/next leaf       = INVALID_PAGE_NO
empty free_page_head    = INVALID_PAGE_NO
terminal next_free      = INVALID_PAGE_NO

blank leaf/internal:
    slot_count = 0
    lower      = 64
    upper      = 8192
```

BTREE_FREE v1 uses a 40-byte header and zero suffix.

Persisted leaf RID sentinels are rejected; relation FileId ownership is validated by the index/tree context when available.

### D5-B — optimistic root metadata

Runtime-only:

```text
root_generation
```

increments on root/height publication.

Root acquisition snapshots/pins under metadata, releases metadata before waiting on page latch, then validates root id/generation while holding the page latch.

A failed validation restarts.

Structural metadata publication may acquire metadata while already holding the required page latch(es).

Free-list and endpoint metadata use the same snapshot/latch/validate/update pattern.

### D6-A — whole-page checksum

Every checksummed random-access page uses:

```text
zero bytes 16..19 logically
CRC32C bytes 0..8191
store uint32 little-endian at 16..19
```

### D7-A — FLOAT64 memcomparable bytes

Normalize:

```text
±0 -> 0x0000000000000000
NaN -> 0x7ff8000000000000
```

Transform normalized bits `u`:

```text
negative    -> ~u
nonnegative -> u XOR 0x8000000000000000
```

Then encode big-endian.

## Key-schema fingerprint

The previously incomplete B+ fingerprint is now also made exact so the BTREE superblock is actually self-validating.

It uses FNV-1a-64 over the canonical key-schema descriptor in §8.3.1.

Example fingerprint for:

```text
(INT64 ASC NULLS_FIRST, VARCHAR ASC NULLS_FIRST BINARY)
```

under the locked descriptor encoding is:

```text
0xf70f943b28325b01
```

This example is informational; the algorithm and descriptor are normative.

## Scope

Pass 7 was not performed.

No production code was modified.

The legacy `ARCHITECTURE.md` was not modified.
