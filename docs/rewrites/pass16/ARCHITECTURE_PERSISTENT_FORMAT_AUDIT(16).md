# Pass 16 — Persistent Format / Numeric Contract Audit

## Result

**PASS — 144/144 automated contract checks passed.**

This audit validates the final reconciled architecture candidate before cutover.

## Source identity

```text
legacy ARCHITECTURE(4).md SHA-256
2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86

final reconciled candidate SHA-256
0365df9e9aa094f4e91e15cbddcedf8fc44d3dd1b75e781ec51eda5fcb4b8a82
```

## Registry checks

### FileKind

```text
0 INVALID
1 HEAP
2 BTREE
3 FSM
4 CATALOG
5 TXN_STATUS
```

### PageType

```text
0 SUPERBLOCK
1 HEAP_DATA
2 FSM_DATA
3 BTREE_INTERNAL
4 BTREE_LEAF
5 BTREE_FREE
6 CATALOG_DATA
7 TXN_STATUS
```

### WAL record_type

```text
0 WAL_PAD
1 PAGE_INIT
2 PAGE_DELTA
3 PAGE_IMAGE
4 BTREE_MTR
5 TXN_COMMIT
6 TXN_ABORT
7 CHECKPOINT_BEGIN
8 CHECKPOINT_DATA
9 CHECKPOINT_END
```

### Built-in TypeId

```text
0 invalid
1 BOOLEAN
2 INT32
3 INT64
4 FLOAT64
5 DATE
6 TIMESTAMP
7 VARCHAR
```

### Heap slot states

```text
0 UNUSED
1 NORMAL
2 DEAD
3 REDIRECT_RESERVED
```

The tuple-flag, WAL/MTR encoding, statistics scope/value-flag, and other local code sets were also checked against their owning definitions.

## Fixed-layout arithmetic

| Format | Verified geometry |
|---|---|
| Common page header | 32 bytes |
| FileSuperblock common prefix | 72 bytes |
| BTREE FileSuperblock header | 128 bytes = 72 + 56-byte extension |
| HEAP_DATA total header | 48 bytes |
| Heap slot entry | 8 bytes |
| Tuple header | 48 bytes |
| VARCHAR tuple descriptor | 8 bytes |
| FSM_DATA | 48-byte header + 8144 one-byte entries = 8192 |
| Persisted index RID | 16 bytes |
| B+ leaf/internal node header | 64 bytes |
| B+ node slot descriptor | 8 bytes |
| BTREE_FREE header | 40 bytes |
| TXN_STATUS page | 32-byte header + 8160 payload = 8192 |
| WAL ordinary record header | 48 bytes |
| WAL PageId | 16 bytes |
| PAGE_DELTA prefix | 24 bytes |
| PAGE_INIT/PAGE_IMAGE payload | 24 + 8192 = 8216 bytes |
| BTREE_MTR prefix | 16 bytes |
| BTREE_MTR affected-page prefix | 24 bytes |
| database.control | 2 × 4096-byte slots = 8192 |
| database.control v1 slot header | 88 bytes |
| CHECKPOINT_BEGIN | 32-byte payload |
| CHECKPOINT_DATA prefix | 24 bytes |
| DPT entry | 24 bytes |
| checkpoint writer entry | 16 bytes |
| CHECKPOINT_END | 32-byte payload |
| CATALOG_DATA bootstrap page | 64 + 6×32 + 7936 = 8192 |
| CATALOG bootstrap entry | 32 bytes |
| PersistedScalarV1 header | 16 bytes |
| DefaultValueBlob header | 24 bytes |
| StatisticsPayloadV1 common header | 40 bytes |
| Statistics TABLE fixed prefix | 104 bytes |
| Statistics COLUMN fixed prefix | 104 bytes |
| Statistics INDEX payload | 112 bytes |

No byte ranges overlap and no fixed-layout total exceeds or undershoots its owning object.

## Formula checks

### Heap/FSM

Verified:

```text
lower = 48 + slot_count * 8

usable =
    min(max(min(free_bytes, 8144) - 8, 0), 8135)

category =
    floor(usable * 255 / 8135)

minimum_usable(c) =
    (c * 8135 + 254) / 255
```

Representative FSM boundary values were recomputed and match the contract:

```text
free 0,1,8,9,39 -> 0
40                -> 1
4075              -> 127
8142              -> 254
8143,8144         -> 255
```

Heap-page to FSM mapping is consistent with 8144 entries/page.

### Transaction status

Verified:

```text
payload = 8192 - 32 = 8160 bytes
entries/page = 8160 * 4 = 32640

ordinal = txn_id - FIRST_NORMAL_TXN_ID
status_page_no = 1 + ordinal / 32640
entry_in_page = ordinal % 32640
payload_byte_index = entry_in_page / 4
two_bit_index = entry_in_page % 4
page_byte_offset = 32 + payload_byte_index
```

### Checksums

Whole-page and record checksum contracts were checked for their exact checksum-field-zeroing rules:

```text
random-access page/superblock CRC:
    bytes 16..19 logically zero

WAL record CRC:
    header bytes 44..47 logically zero

database.control slot CRC:
    bytes 80..83 logically zero

DefaultValueBlob / StatisticsPayloadV1:
    bytes 16..19 logically zero
```

### B+ keys

Verified the persisted physical-key and ordering rules:

```text
physical key = (encoded user key, RID)
max encoded user key = 1024 bytes

INT32/DATE:
    sign-bit flip, big-endian

INT64/TIMESTAMP:
    sign-bit flip, big-endian

FLOAT64:
    ±0 -> canonical zero for key ordering
    all NaNs -> 0x7ff8000000000000
    negative -> bitwise_not(u)
    nonnegative -> u XOR 0x8000000000000000
    big-endian output
```

The standalone persisted RID remains 16 bytes with reserved bytes 14..15 required zero.

## Persistent-format registry reconciliation

Appendix A indexes the owning definitions and does not create independent byte rules.

Every registry item points to an existing canonical section.

No stale 80-byte `database.control` header remains; the canonical v1 control-slot header is 88 bytes.

No persistent search sentinel is confused with a legal RID/PageNo value.

## Invariant audit

All subsystem invariant-section references in Appendix B resolve.

The key cross-persistence invariants remain consistent:

```text
explicit serialization, no ABI persistence
WAL before data
NO-FORCE commit
page_lsn / rec_lsn recovery discipline
strict reserved-byte validation
MVCC visibility from heap/status, not indexes
physical RID reuse only after Chapter-14 grace
immutable catalog/bootstrap identity rules
stable explicit numeric codes
```

## Conclusion

The final candidate contains no unresolved persisted numeric-code, offset, size, mapping, checksum, or formula contradiction found by the Pass-16 audit.
