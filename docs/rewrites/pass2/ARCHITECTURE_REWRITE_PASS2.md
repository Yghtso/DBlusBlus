# Rewrite Pass 2 — Persistent Storage Foundations

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `53..63`
- processed source lines: `1425..1935`
- legacy architecture modified: **no**
- production code modified: **no**

Pass 2 replaces the high-level Chapter 4 baseline created in Pass 1 with the canonical persistent-storage-foundation contract.

## Canonicalized material

### Identifier widths

| Type | Width |
|---|---:|
| FileId | 32 bits |
| PageNo | 64 bits |
| SlotId | 16 bits |
| TxnId | 64 bits |
| CommandId | 32 bits |
| Lsn | 64 bits |
| TableId | 64 bits |
| IndexId | 64 bits |
| SchemaVer | 32 bits |

### Sentinel values

```text
INVALID_FILE_ID = 0
INVALID_PAGE_NO = UINT64_MAX
INVALID_SLOT_ID = UINT16_MAX
INVALID_TXN_ID  = 0
INVALID_LSN     = 0
```

The legacy source labels these “Recommended sentinels” inside a LOCKED section. Later source sections use the values as fixed structural semantics. Pass 2 therefore expresses the listed values normatively without changing any value.

Pass 2 does not invent a sentinel for CommandId, TableId, IndexId, or SchemaVer.

### Page identity

Preserved:

```text
PageId = (FileId, PageNo)
byte_offset = page_no * PAGE_SIZE
```

and the distinction between persistent page identity and:

```text
buffer frame
memory address
OS file descriptor
```

### RID semantics

Preserved:

```text
RID = (PageId, SlotId)
```

with RID identifying a physical heap tuple version rather than a permanent logical SQL row.

The index-to-heap consequences from legacy §57 are canonicalized in §4.6:

- B+ tree leaves reference physical RIDs,
- non-unique physical ordering uses `(index key, RID)`,
- UPDATE normally creates a new RID and new index references,
- old physical index entries may remain until vacuum,
- index scans fetch the heap version and apply MVCC visibility,
- HOT-like optimization remains deferred.

### FileKind persisted codes

```text
0 INVALID / unassigned
1 HEAP
2 BTREE
3 FSM
4 CATALOG
```

The field is exactly unsigned 16-bit little-endian. Existing codes cannot be renumbered. Code 0 is rejected for a persistent random-access file.

WAL remains a distinct append-only format rather than a random-access FileKind.

### Random-access file page numbering

```text
page 0 = superblock
page 1+ = ordinary object/data pages
```

### Common page header

Canonical size:

```text
32 bytes
```

Exact layout retained:

```text
0..1    page_type
2..3    format_version
4..7    flags
8..15   page_lsn
16..19  checksum_crc32c
20..21  header_size
22..23  reserved16
24..31  page_no
```

All multi-byte fields are little-endian.

The source does not globally require the common `reserved16` field to be zero for every page type in legacy §61; Pass 2 therefore does not invent such a universal rule. The superblock-specific contract does require it to be zero.

### PageType persisted codes

```text
0 SUPERBLOCK
1 HEAP_DATA
2 FSM_DATA
3 BTREE_INTERNAL
4 BTREE_LEAF
5 BTREE_FREE
6 CATALOG_DATA
```

The field is exactly unsigned 16-bit little-endian. Page-specific parsing must validate the expected type.

### File superblock v1

Canonical total size:

```text
8192 bytes
```

Canonical logical header size:

```text
72 bytes
```

Constants retained exactly:

```text
magic          = "DBLUSBLS"
format_version = 1
page_size      = 8192
page_type      = SUPERBLOCK
page_no        = 0
header_size    = 72
```

The complete byte table from legacy §59 is preserved in Chapter 4.

The strict v1 validation contract remains:

- one-page minimum input,
- CRC32C verification,
- exact page type/version/header size/page number/magic/page size,
- valid known FileKind,
- every reserved field and trailing reserved byte zero.

Unknown flag bits remain preserved raw bits.

Buffers larger than 8192 bytes may be accepted by a codec, but only the first page participates in the v1 format.

### Superblock checksum

Preserved exactly:

```text
zero bytes 16..19 logically
CRC32C over bytes 0..8191
store uint32 little-endian at 16..19
```

### Page allocation

Preserved:

- a raw newly created random-access file starts at zero bytes / zero pages,
- higher-level file creation explicitly allocates/initializes page 0,
- append-first allocation uses current page count as new PageNo,
- append extends exactly one page,
- ordinary writes to unallocated pages fail,
- writes do not implicitly extend or create sparse pages,
- concurrent append allocation of the same managed storage is serialized,
- whole-file shrinking is not a baseline requirement,
- object-specific page recycling may be added later,
- a general-purpose extent allocator is deferred.

## Deduplication performed

The Pass 1 Chapter 4 overview was replaced rather than retained beside the concrete contract.

The resulting Chapter 4 now has one canonical definition for:

- page size,
- explicit serialization,
- identifiers/sentinels,
- PageId,
- RID,
- FileKind codes,
- common page header,
- PageType codes,
- superblock,
- allocation,
- checksum rules.

Appendix A now indexes these definitions instead of becoming a second normative copy.

## New issue recorded

### R-010 — ordinary-page checksum coverage

The source gives an exact complete-page checksum range for superblocks but does not clearly define one universal exact byte range for every other checksummed random-access page.

Pass 2 does not guess.

Chapter 4 preserves:

- CRC32C,
- checksum-field-zero treatment,
- staged enablement,
- purpose and limitations.

R-010 must be resolved by later source material or an explicit architecture decision before final cutover if no later section supplies a universal exact rule.

## Coverage ledger result

Legacy sections `53..63` are complete.

Current totals:

```text
legacy §§0..63     migrated / explicitly disposed
legacy §§64..725  pending
```

No later source section was marked complete by Pass 2.

## Additional safeguards applied

Pass 2 also verified that:

1. FileKind code `0` and PageType code `0` belong to different enums and retain their different meanings.
2. `page_lsn` is not given semantics beyond “newest WAL-protected modification reflected in the page”.
3. The common `reserved16` field is not globally strengthened beyond what the source states; strict zero is applied where the owning format explicitly requires it.
4. No persisted PageId or RID byte encoding was invented from implementation history. Pass 2 preserves logical identity only; persisted RID encoding remains owned by the later B+ tree contract.
5. No checked-offset overflow rule was invented in §4.4; checked physical-I/O arithmetic remains for the I/O pass.
6. Superblock `creation_epoch` remains opaque rather than being assigned an unstated time unit.
7. Superblock unknown flag bits remain preserved rather than rejected.
8. Future use of zero-reserved superblock bytes remains format-versioned.

## Validation

Mechanical checks confirmed:

- the legacy source still matches the Pass 0 SHA-256,
- source §53 begins at line 1425,
- source §64 begins at line 1936,
- all sections 53 through 63 exist,
- every coverage row through §63 is non-PENDING,
- every row from §64 onward remains PENDING,
- Chapter 4 contains every exact persisted code and size listed above,
- no production source file was changed,
- legacy `ARCHITECTURE.md` was not changed.

## Pass 2 exit status

**COMPLETE.**

Pass 3 should process only legacy §§64–81 and replace the current Chapter 5 baseline with the canonical heap-storage and tuple-format contract.
