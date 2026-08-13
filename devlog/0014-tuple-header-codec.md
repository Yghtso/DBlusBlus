# 0014 — Tuple Header Codec

Date: 2026-08-14

## Milestone/task

Phase 1: fixed 48-byte persisted physical tuple-header representation and explicit codec.

## Scope

Added a standalone tuple-header semantic representation, exact persisted offsets and size, two
physical-layout tuple-flag masks, explicit little-endian encoding, structural decoding, and focused
tests. The codec composes with the existing opaque `HeapPage::Insert` and `TupleBytes` APIs without
making `HeapPage` aware of tuple metadata.

No schema-directed tuple codec, null-bitmap generation, fixed/variable column layout, schema lookup,
transaction status, MVCC visibility, update/delete behavior, HeapFile, FSM, BufferPool, WAL, or
recovery behavior was added.

## Files changed

- `src/storage/tuple_header.h` — defines the semantic header, persisted constants, tuple-flag masks,
  decode errors/results, and codec declarations.
- `src/storage/tuple_header.cpp` — implements structural validation and explicit little-endian
  encoding/decoding.
- `src/CMakeLists.txt` — registers the tuple-header source and public header with
  `dblusblus_core`.
- `tests/tuple_header_test.cpp` — adds exact-layout, boundary, sentinel, validation, buffer, and
  HeapPage-composition tests.
- `tests/CMakeLists.txt` — registers the focused tuple-header tests.
- `devlog/0014-tuple-header-codec.md` — records this task.

No existing storage API or older devlog entry was modified.

## Architecture sections used

- §9 — explicit persisted binary serialization and little-endian integers
- §49 — Phase 1 raw-storage implementation order
- §54 — fixed-width identifier types and sentinel values
- §56 — physical tuple-version `Rid` semantics
- §70 — fixed 48-byte tuple header, command fields, and same-file previous-version pointer
- §71 — physical tuple flags and the requirement to avoid ad-hoc transaction-status flags
- §72 — tuple header followed by null bitmap, fixed area, and variable payload
- §76 — compact persisted layout and safe unaligned encoding
- §77 and §310 — persisted schema version and future schema interpretation
- §78 — encoded tuple bytes entering the heap insertion path
- §186 — `CommandId` starts at zero and is persisted as `cmin`/`cmax`

`ARCHITECTURE.md` is the authoritative architecture contract used for this task.

## Exact public API introduced

Declared in `storage/tuple_header.h`:

```cpp
using TupleFlags = std::uint16_t;

inline constexpr TupleFlags TUPLE_FLAG_HAS_NULLS = 0x0001U;
inline constexpr TupleFlags TUPLE_FLAG_HAS_VARLEN = 0x0002U;
inline constexpr TupleFlags TUPLE_FLAGS_KNOWN_MASK = 0x0003U;

inline constexpr std::size_t TUPLE_HEADER_XMIN_OFFSET = 0;
inline constexpr std::size_t TUPLE_HEADER_XMAX_OFFSET = 8;
inline constexpr std::size_t TUPLE_HEADER_CMIN_OFFSET = 16;
inline constexpr std::size_t TUPLE_HEADER_CMAX_OFFSET = 20;
inline constexpr std::size_t TUPLE_HEADER_PREV_PAGE_NO_OFFSET = 24;
inline constexpr std::size_t TUPLE_HEADER_PREV_SLOT_OFFSET = 32;
inline constexpr std::size_t TUPLE_HEADER_FLAGS_OFFSET = 34;
inline constexpr std::size_t TUPLE_HEADER_HEADER_BYTES_OFFSET = 36;
inline constexpr std::size_t TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET = 38;
inline constexpr std::size_t TUPLE_HEADER_SCHEMA_VERSION_OFFSET = 40;
inline constexpr std::size_t TUPLE_HEADER_RESERVED_OFFSET = 44;
inline constexpr std::size_t TUPLE_HEADER_ENCODED_SIZE = 48;

struct TupleHeader {
    TxnId xmin;
    TxnId xmax;
    CommandId cmin;
    CommandId cmax;
    PageNo prev_page_no;
    SlotId prev_slot;
    TupleFlags tuple_flags;
    std::uint16_t header_bytes;
    std::uint16_t null_bitmap_bytes;
    SchemaVer schema_version;
    std::uint32_t reserved;
    bool operator==(const TupleHeader&) const = default;
};

enum class TupleHeaderDecodeError : std::uint8_t {
    NONE,
    SOURCE_TOO_SMALL,
    INVALID_PREVIOUS_VERSION_POINTER,
    INVALID_FLAGS,
    INVALID_HEADER_SIZE,
    NONZERO_RESERVED,
};

struct TupleHeaderDecodeResult {
    std::optional<TupleHeader> header;
    TupleHeaderDecodeError error;
    explicit operator bool() const noexcept;
};

bool EncodeTupleHeader(std::span<std::byte> destination,
                       const TupleHeader& header) noexcept;
TupleHeaderDecodeResult DecodeTupleHeader(std::span<const std::byte> source) noexcept;
```

The header also has compile-time width and end-offset assertions for the persisted identifier and
field types.

## Exact 48-byte persisted layout

All fields are encoded explicitly in little-endian order:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | `xmin` (`TxnId`) |
| 8 | 8 | `xmax` (`TxnId`) |
| 16 | 4 | `cmin` (`CommandId`) |
| 20 | 4 | `cmax` (`CommandId`) |
| 24 | 8 | `prev_page_no` (`PageNo`) |
| 32 | 2 | `prev_slot` (`SlotId`) |
| 34 | 2 | `tuple_flags` (`TupleFlags`) |
| 36 | 2 | `header_bytes` |
| 38 | 2 | `null_bitmap_bytes` |
| 40 | 4 | `schema_version` (`SchemaVer`) |
| 44 | 4 | zero `reserved` |

The codec uses the existing primitive little-endian helpers, accepts unaligned spans, performs no
raw-struct persistence or pointer reinterpretation, and allocates no heap memory. Encoding first
validates the semantic header and stages all 48 encoded bytes in a fixed stack array. An undersized
or structurally invalid destination is therefore not partially modified.

## Tuple-flag masks introduced

Only flags with immediate physical-layout meaning were introduced:

```text
HAS_NULLS  = 0x0001
HAS_VARLEN = 0x0002
```

`IS_DELETED_HINT`, `CHAIN_ROOT`, and `CHAIN_MEMBER` remain deferred because their operational,
visibility, and recovery semantics are not yet implemented.

## Unknown-flag policy

Version 1 rejects any encoded or semantic `tuple_flags` bits outside mask `0x0003`. Decode reports
`INVALID_FLAGS`; encode returns `false` without modifying the destination.

## Previous-version sentinel/pair validation

The codec accepts either:

- both `prev_page_no == INVALID_PAGE_NO` and `prev_slot == INVALID_SLOT_ID`, meaning no previous
  version, or
- both fields different from their invalid sentinels, meaning a same-heap-file previous-version
  address.

A mixed sentinel/valid pair is rejected as `INVALID_PREVIOUS_VERSION_POINTER`. No `FileId` is
persisted in this pointer.

## `header_bytes` interpretation

For this initial format, `header_bytes` is required to equal 48 and describes only the fixed tuple
header prefix. It does not include the following null bitmap. Any other value is rejected as
`INVALID_HEADER_SIZE`; variable tuple-header extensions remain deferred.

## `null_bitmap_bytes` behavior

The codec persists and round-trips the full representable `std::uint16_t` value without deriving it
from a schema or interpreting bitmap contents. The future complete tuple codec must calculate and
validate this value against the schema and total tuple size.

## Schema-version validation behavior

The low-level header codec accepts every representable `SchemaVer`, including zero and values other
than one. Catalog/schema support will decide whether a stored version is available and supported;
that policy is not embedded in this primitive persistence layer.

## Reserved-field behavior

The semantic field must be zero for encoding, the encoder writes four deterministic zero bytes,
and decoding rejects a nonzero value as `NONZERO_RESERVED`.

## HeapPage composition test

The focused integration test encodes a `TupleHeader` into the first 48 bytes of a fixed stack array,
appends arbitrary opaque payload bytes, inserts that array with `HeapPage::Insert`, retrieves it with
`HeapPage::TupleBytes`, and decodes the same header from the returned byte span. The complete stored
bytes and decoded header match exactly. `HeapPage` source and API were not changed.

## Tests/checks run

- Focused Clang build and nine `TupleHeaderCodecTest` tests: passed.
- Full `clang-debug` build and CTest suite: 116/116 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 116/116 passed with
  `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer was disabled because the execution environment runs
  under ptrace and LSan aborts during GoogleTest discovery. ASan and UBSan remained enabled.
- Focused `gcc-debug` build and nine tuple-header tests: passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.

## Assumptions

- `xmin`, `xmax`, `cmin`, and `cmax` are opaque persisted identifier values at this layer; no
  transaction or command visibility conclusions are drawn from them.
- `INVALID_TXN_ID == 0` remains the locked no-`xmax` value. `CommandId` zero remains valid and no
  invalid command sentinel was introduced.
- The default semantic object is deterministic and unassigned: invalid transaction identifiers,
  zero command identifiers, the no-previous sentinel pair, no flags, a 48-byte fixed header, zero
  bitmap length, schema version zero, and zero reserved data. Callers must explicitly provide
  meaningful transaction/schema fields when constructing a physical tuple version.
- A valid previous-version pair is implicitly relative to the tuple's heap file, as locked by the
  architecture.

## Known limitations/deferred work

- No complete tuple codec, schema-directed size planning, null-bitmap generation or validation,
  fixed-layout field encoding, varlen descriptors, or variable payload construction.
- No schema lookup or rejection of unavailable schema versions.
- No transaction-status lookup, snapshot visibility, MVCC insert/update/delete semantics,
  previous-version traversal, or command visibility.
- No `IS_DELETED_HINT`, `CHAIN_ROOT`, or `CHAIN_MEMBER` persisted masks.
- No HeapFile, FSM, BufferPool, page latch, dirty tracking, WAL, recovery, page-LSN, or checksum
  integration.

## Architecture questions discovered

1. Should v1 lock `HAS_NULLS = 0x0001` and `HAS_VARLEN = 0x0002` while deferring all other listed
   tuple-flag candidates? This implementation assigns and tests those two persisted masks.
2. Should v1 reject every unknown tuple-flag bit instead of preserving it? This implementation
   rejects unknown bits on encode and decode.
3. Should v1 explicitly require the previous-version pointer to use either both invalid sentinels or
   two non-sentinel fields? This implementation rejects mixed pairs.
4. Should `header_bytes` be locked as the fixed 48-byte tuple-header prefix, excluding the following
   null bitmap? This implementation enforces that interpretation.
5. Should tuple-header v1 require all four reserved bytes to be zero on both encode and decode? This
   implementation enforces strict zero-reserved behavior.
