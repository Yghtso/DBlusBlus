# 0015 — Tuple Physical Layout

Date: 2026-08-14

## Milestone/task

Phase 1: null-bitmap primitives, schema-version physical column layout, and deterministic encoded
tuple-size planning without value serialization.

## Scope

Added checked null-bitmap sizing and bit access, the initial storage physical-type classification,
precomputed per-column fixed-area metadata, schema-level physical layout construction, and an
allocation-free per-tuple size planner for VARCHAR payload lengths/null states.

No SQL value representation, tuple construction, null-bitmap generation from values, scalar value
encoding, VARCHAR descriptor writing, payload copying, schema catalog, HeapFile, FSM, MVCC,
BufferPool, WAL, or recovery behavior was added.

## Files changed

- `src/storage/tuple_layout.h` — defines physical widths/types, null-bitmap API, physical column and
  layout metadata, build errors/results, and tuple-size planning API.
- `src/storage/tuple_layout.cpp` — implements checked bitmap operations, type widths, layout
  precomputation, and size planning.
- `src/CMakeLists.txt` — registers the layout source and public header with `dblusblus_core`.
- `tests/tuple_layout_test.cpp` — adds focused bitmap, fixed-layout, boundary, planning, and
  TupleHeader-composition tests.
- `tests/CMakeLists.txt` — registers the focused layout test source.
- `devlog/0015-tuple-physical-layout.md` — records this task.

No existing storage implementation or older devlog entry was modified.

## Architecture sections used

- §49 — Phase 1 raw-storage implementation order
- §69 — strict v1 maximum raw tuple size of 8135 bytes
- §70–§71 — fixed 48-byte tuple header and physical-layout tuple flags
- §72 — tuple order: header, null bitmap, fixed area, variable payload
- §73 — null bitmap meaning and candidate bit/allocation conventions
- §74 — locked initial fixed-width physical values
- §75 — 8-byte inline VARCHAR descriptor and tuple-relative payload offsets
- §76 — compact persisted layout without ABI alignment padding
- §77 — persisted schema version

`ARCHITECTURE.md` is the authoritative architecture contract used for this task.

## Exact public API introduced

Declared in `storage/tuple_layout.h`:

```cpp
inline constexpr std::size_t BOOLEAN_PHYSICAL_SIZE = 1;
inline constexpr std::size_t INT32_PHYSICAL_SIZE = 4;
inline constexpr std::size_t INT64_PHYSICAL_SIZE = 8;
inline constexpr std::size_t FLOAT64_PHYSICAL_SIZE = 8;
inline constexpr std::size_t DATE_PHYSICAL_SIZE = 4;
inline constexpr std::size_t TIMESTAMP_PHYSICAL_SIZE = 8;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_OFFSET_OFFSET = 0;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_LENGTH_OFFSET = 4;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_ENCODED_SIZE = 8;

enum class PhysicalType : std::uint8_t {
    BOOLEAN,
    INT32,
    INT64,
    FLOAT64,
    DATE,
    TIMESTAMP,
    VARCHAR,
};

std::optional<std::size_t> PhysicalTypeWidth(PhysicalType type) noexcept;
std::optional<std::uint16_t> NullBitmapBytes(std::size_t column_count) noexcept;

enum class NullBitmapError : std::uint8_t {
    NONE,
    COLUMN_COUNT_TOO_LARGE,
    COLUMN_OUT_OF_RANGE,
    BITMAP_TOO_SMALL,
};

struct NullBitmapReadResult {
    bool is_null;
    NullBitmapError error;
    explicit operator bool() const noexcept;
};

NullBitmapReadResult IsNull(std::span<const std::byte> bitmap,
                            std::size_t column_count,
                            std::size_t column_index) noexcept;
NullBitmapError SetNull(std::span<std::byte> bitmap,
                        std::size_t column_count,
                        std::size_t column_index) noexcept;
NullBitmapError ClearNull(std::span<std::byte> bitmap,
                          std::size_t column_count,
                          std::size_t column_index) noexcept;

struct PhysicalColumnSpec {
    PhysicalType type;
    bool nullable;
};

struct PhysicalColumnLayout {
    PhysicalType type;
    bool nullable;
    std::size_t fixed_offset;
    std::size_t fixed_width;
    std::size_t null_bit_index;
};

struct VarlenValueSize {
    std::size_t length;
    bool is_null;
};

struct TupleSizePlan {
    std::size_t varlen_payload_size;
    std::size_t total_size;
};

enum class TupleSizePlanningError : std::uint8_t {
    NONE,
    VARLEN_VALUE_COUNT_MISMATCH,
    VARCHAR_LENGTH_TOO_LARGE,
    SIZE_OVERFLOW,
    TUPLE_TOO_LARGE,
};

struct TupleSizePlanningResult {
    std::optional<TupleSizePlan> plan;
    TupleSizePlanningError error;
    std::size_t varlen_index;
    explicit operator bool() const noexcept;
};

enum class TuplePhysicalLayoutBuildError : std::uint8_t {
    NONE,
    INVALID_PHYSICAL_TYPE,
    NULL_BITMAP_TOO_LARGE,
    SIZE_OVERFLOW,
    TUPLE_TOO_LARGE,
};

class TuplePhysicalLayout {
public:
    SchemaVer SchemaVersion() const noexcept;
    std::size_t ColumnCount() const noexcept;
    std::span<const PhysicalColumnLayout> Columns() const noexcept;
    const PhysicalColumnLayout* Column(std::size_t column_index) const noexcept;
    std::uint16_t NullBitmapSize() const noexcept;
    std::size_t FixedAreaOffset() const noexcept;
    std::size_t FixedAreaSize() const noexcept;
    std::size_t VarlenPayloadOffset() const noexcept;
    std::size_t MinimumTupleSize() const noexcept;
    std::size_t VarlenColumnCount() const noexcept;
    bool HasNullableColumns() const noexcept;
    bool HasVarlenColumns() const noexcept;
    TupleSizePlanningResult
    PlanTupleSize(std::span<const VarlenValueSize> varlen_values) const noexcept;
};

struct TuplePhysicalLayoutBuildResult {
    std::optional<TuplePhysicalLayout> layout;
    TuplePhysicalLayoutBuildError error;
    std::size_t column_index;
    explicit operator bool() const noexcept;
};

TuplePhysicalLayoutBuildResult
BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec> columns,
                         SchemaVer schema_version);
```

The physical layout owns one `std::vector<PhysicalColumnLayout>` as schema-lifetime metadata.
Bitmap access and per-tuple size planning allocate no memory.

## Physical type enum/codes

`PhysicalType` contains `BOOLEAN`, `INT32`, `INT64`, `FLOAT64`, `DATE`, `TIMESTAMP`, and `VARCHAR`.
It is explicitly an in-memory storage-layout classification. No persisted numeric type codes were
assigned, exposed, or tested in this milestone; persisted interpretation remains schema-directed.

Invalid cast enum values are rejected by `PhysicalTypeWidth` and layout construction.

## Exact type widths

```text
BOOLEAN      1 byte
INT32        4 bytes
INT64        8 bytes
FLOAT64      8 bytes
DATE         4 bytes
TIMESTAMP    8 bytes
VARCHAR      8-byte fixed-area descriptor
```

The VARCHAR descriptor reserves bytes `0..3` for its future tuple-relative `uint32_t offset` and
bytes `4..7` for its future `uint32_t length`. This milestone does not write either field.

## Null-bitmap bit ordering

The bitmap is LSB-first by physical schema-column index:

```text
column 0  -> byte 0 bit 0
column 7  -> byte 0 bit 7
column 8  -> byte 1 bit 0
column 15 -> byte 1 bit 7
```

A set bit means NULL; a clear bit means present. Tests pin columns 0, 7, 8, and 15 to exact bytes
`{0x81, 0x81}`. Set/clear operations preserve every unrelated bit.

## Bitmap allocation policy

V1 allocates one bit for every physical schema column, regardless of its `nullable` declaration.
Therefore a column's `null_bit_index` always equals its schema-column index. The layout separately
retains `nullable` and `HasNullableColumns()` as schema facts; bitmap primitives do not enforce SQL
`NOT NULL` constraints.

`HAS_NULLS` is not derived by this layer. It remains a per-tuple fact for the future encoder rather
than a synonym for a schema containing nullable columns.

## Bitmap-size calculation

```text
bitmap_bytes = column_count / 8 + (column_count % 8 != 0)
```

The result must fit `TupleHeader::null_bitmap_bytes` (`uint16_t`). Zero columns yield zero bytes;
524,280 columns yield 65,535 bytes; a larger count is rejected. Access APIs also reject an invalid
column index, a truncated bitmap, or an unrepresentable column count without reading or modifying
the span.

## Fixed-area packing rule

The fixed area begins at:

```text
48 + null_bitmap_bytes
```

Columns are packed consecutively in schema order using their locked physical widths with no
machine-alignment padding. Every `fixed_offset` is an absolute offset from tuple byte zero.

The pinned mixed example `INT32, VARCHAR, INT64, BOOLEAN` has:

```text
null bitmap bytes     = 1
fixed area begins     = 49
INT32 offset/width    = 49 / 4
VARCHAR offset/width  = 53 / 8
INT64 offset/width    = 61 / 8
BOOLEAN offset/width  = 69 / 1
fixed area size       = 21
minimum tuple size    = 70
```

Repeated construction from the same schema metadata produces equal layout objects.

## VARCHAR descriptor placement

Each VARCHAR column consumes eight tightly packed fixed-area bytes at its precomputed
`fixed_offset`. The variable payload begins immediately after the complete fixed area. Descriptor
offset/length encoding and per-value payload offsets remain deferred to the complete tuple codec.

## Schema-version handling

`TuplePhysicalLayout` stores and returns the supplied `SchemaVer` unchanged. The low-level builder
accepts every representable value, including zero and `UINT32_MAX`, and performs no catalog lookup
or version-availability policy.

## Tuple-size planning behavior

`PlanTupleSize` accepts one `VarlenValueSize` per VARCHAR column in schema order. Fixed-width columns
do not appear in this compact input.

For each non-NULL VARCHAR, its length contributes to payload bytes. NULL VARCHARs contribute zero
bytes and their supplied length is ignored. Zero-length present VARCHARs are accepted and add zero.

The returned plan is:

```text
varlen_payload_size = sum(non-NULL VARCHAR lengths)
total_size = minimum_tuple_size + varlen_payload_size
```

No tuple bytes or descriptor offsets are allocated or written. An incorrect varlen entry count,
present length above `UINT32_MAX`, checked arithmetic overflow, or total above 8135 returns a local
error. A one-VARCHAR layout accepts a total exactly equal to 8135 and rejects 8136.

The layout exposes `HasVarlenColumns()` as a schema/layout fact but does not derive runtime
`HAS_VARLEN` tuple flags.

## Zero-column behavior

A zero-column low-level schema is accepted and produces:

```text
column count          = 0
null bitmap bytes     = 0
fixed area size       = 0
variable payload start/minimum tuple size = 48
```

Planning with an empty varlen span returns a 48-byte total. Any SQL/catalog prohibition of
zero-column tables remains a higher-layer policy.

## Overflow/limit behavior

- Bitmap sizing avoids `column_count + 7` overflow and rejects byte counts above `UINT16_MAX`.
- Fixed offsets and sizes use checked addition. Under the representable bitmap bound and fixed
  locked widths, the 8135-byte raw tuple limit is reached before native `size_t` overflow; tests
  verify an oversized 1000-`INT64` layout is rejected.
- Present VARCHAR lengths must fit the persisted `uint32_t` descriptor length before summation.
- Payload and final total additions are checked for native overflow.
- Minimum and planned total tuple sizes must be at most `HEAP_PAGE_MAX_RAW_TUPLE_SIZE` (8135).
- No unchecked narrowing conversion is used.

## TupleHeader composition

A focused test builds a layout, copies its `NullBitmapSize()` and `SchemaVersion()` into a semantic
`TupleHeader`, encodes the 48-byte header, decodes it, and verifies both fields. No value bytes,
bitmap bytes, fixed fields, descriptor bytes, or payload bytes are serialized by this milestone.

## Tests/checks run

- Focused Clang build and 13 null-bitmap/physical-layout/size-planning tests: passed.
- Full `clang-debug` build and CTest suite: 129/129 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 129/129 passed with
  `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer was disabled because the execution environment runs
  under ptrace and LSan aborts during GoogleTest discovery. ASan and UBSan remained enabled.
- Focused `gcc-debug` build and 13 layout tests: passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.

## Assumptions

- Physical column order is the schema's persisted tuple order for the supplied schema version.
- Fixed offsets are absolute tuple-relative offsets; future VARCHAR descriptors also store
  tuple-relative payload offsets as required by the architecture.
- A NULL VARCHAR's input length is irrelevant because no descriptor payload bytes are retained for
  it by size planning.
- The future encoder, not this schema-level layout object, derives per-tuple `HAS_NULLS` and
  `HAS_VARLEN` flags.

## Known limitations/deferred work

- No SQL values, tuple buffer construction, null-bitmap population from values, or `NOT NULL`
  enforcement.
- No BOOLEAN/integer/FLOAT64/DATE/TIMESTAMP encoding or semantic validation.
- No VARCHAR descriptor writes, payload offset planning per value, or payload copies.
- No tuple-header construction policy, schema lookup, catalog ownership, schema migration, or
  historical schema resolution.
- No HeapFile, FSM, MVCC, BufferPool, WAL, recovery, or execution representation.

## Architecture questions discovered

1. Should tuple format v1 lock LSB-first null-bit ordering by physical column index (`column 0` is
   byte 0 bit 0, `column 8` is byte 1 bit 0)? This implementation uses and tests that persisted
   ordering.
2. Should tuple format v1 lock one allocated null bit for every physical schema column, including
   columns declared `NOT NULL`? This implementation uses and tests that persisted allocation
   convention.

No persisted numeric `PhysicalType` codes were introduced; the enum is schema-lifetime in-memory
metadata only.
