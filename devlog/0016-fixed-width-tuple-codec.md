# 0016 — Fixed-Width Physical Tuple Codec

Date: 2026-08-14

## Milestone/task

Phase 1: fixed-width physical scalar encoding/decoding and fixed-only tuple construction/decoding
against `TuplePhysicalLayout`.

## Scope

Added explicit physical codecs for `BOOLEAN`, `INT32`, `INT64`, `FLOAT64`, `DATE`, and
`TIMESTAMP`; a narrow owning encoder for fixed-width-only tuples; structural validation; and
schema-directed fixed-column decoding. The tuple encoder writes the existing 48-byte
`TupleHeader`, one null bit per physical column, and the precomputed fixed area. It enforces column
count, exact physical type, schema nullability, previous-version metadata consistency, the heap
inline-size bound, and fixed-only scope.

No VARCHAR descriptor or payload bytes, general varlen tuple construction, VARCHAR decode, SQL
coercion, catalog lookup, parser/execution `Value`, execution vectors, visibility logic,
UPDATE/DELETE, HeapFile, FSM, BufferPool, WAL, or recovery behavior was added.

## Files changed

- `src/storage/tuple_codec.h` — declares the fixed construction representation, scalar codec API,
  tuple-version metadata, local errors/results, fixed tuple encoder, validator, and decoder.
- `src/storage/tuple_codec.cpp` — implements exact scalar persistence, fixed tuple construction,
  canonical null/flag validation, schema-version validation, and per-column decode.
- `src/CMakeLists.txt` — registers the new codec source and public header with `dblusblus_core`.
- `tests/tuple_codec_test.cpp` — adds focused exact-byte, boundary, corruption, nullability,
  canonicalization, zero-column, type-safety, and HeapPage composition tests.
- `tests/CMakeLists.txt` — registers the focused codec test source.
- `devlog/0016-fixed-width-tuple-codec.md` — records this completed milestone.

No older devlog entry, `ARCHITECTURE.md`, existing tuple-header/layout implementation, or HeapPage
implementation was modified.

## Architecture sections used

- §49 — Phase 1 raw-storage implementation order
- §69 — strict v1 maximum inline raw tuple payload of 8135 bytes
- §70 — fixed 48-byte tuple header and previous-version pointer invariants
- §71 — `HAS_NULLS`, `HAS_VARLEN`, and the v1 known tuple-flag mask
- §72 — schema-directed compact tuple layout
- §73 — one LSB-first null bit per physical column and `NOT NULL` enforcement responsibility
- §74 — locked fixed physical widths and little-endian scalar encoding
- §76 — compact unaligned persistence and explicit endian helpers
- §77 — tuple-carried schema version
- §96 — `TupleCodec` as the schema-directed tuple encode/decode storage boundary

`ARCHITECTURE.md` remains authoritative. New codec-level persisted-format policies discovered in
this task are listed as architecture questions below; they were not written into the architecture
contract.

## Exact public API introduced

Declared in `storage/tuple_codec.h`:

```cpp
struct Float64PhysicalValue {
    std::uint64_t bits{0};
    static constexpr Float64PhysicalValue FromDouble(double value) noexcept;
    constexpr double ToDouble() const noexcept;
    bool operator==(const Float64PhysicalValue&) const = default;
};

struct DatePhysicalValue {
    std::int32_t value{0};
    bool operator==(const DatePhysicalValue&) const = default;
};

struct TimestampPhysicalValue {
    std::int64_t value{0};
    bool operator==(const TimestampPhysicalValue&) const = default;
};

using FixedTupleValue = std::variant<std::monostate,
                                     bool,
                                     std::int32_t,
                                     std::int64_t,
                                     Float64PhysicalValue,
                                     DatePhysicalValue,
                                     TimestampPhysicalValue>;

enum class FixedScalarCodecError : std::uint8_t {
    NONE,
    DESTINATION_TOO_SMALL,
    SOURCE_TOO_SMALL,
    TYPE_MISMATCH,
    UNSUPPORTED_VARLEN_TYPE,
    INVALID_PHYSICAL_TYPE,
    INVALID_BOOLEAN,
};

struct FixedScalarDecodeResult {
    std::optional<FixedTupleValue> value;
    FixedScalarCodecError error{FixedScalarCodecError::NONE};
    explicit operator bool() const noexcept;
};

FixedScalarCodecError EncodeFixedScalar(std::span<std::byte> destination,
                                        PhysicalType type,
                                        const FixedTupleValue& value) noexcept;
FixedScalarDecodeResult DecodeFixedScalar(PhysicalType type,
                                          std::span<const std::byte> source) noexcept;

struct TupleVersionMetadata {
    TxnId xmin{INVALID_TXN_ID};
    TxnId xmax{INVALID_TXN_ID};
    CommandId cmin{0};
    CommandId cmax{0};
    PageNo prev_page_no{INVALID_PAGE_NO};
    SlotId prev_slot{INVALID_SLOT_ID};
    bool operator==(const TupleVersionMetadata&) const = default;
};

enum class FixedTupleCodecError : std::uint8_t {
    NONE,
    COLUMN_COUNT_MISMATCH,
    COLUMN_OUT_OF_RANGE,
    TYPE_MISMATCH,
    NULL_NOT_ALLOWED,
    UNSUPPORTED_VARLEN_TYPE,
    TUPLE_TOO_LARGE,
    INVALID_HEADER_METADATA,
    MALFORMED_TUPLE,
    SCHEMA_VERSION_MISMATCH,
    INVALID_BOOLEAN,
    FLAG_BITMAP_MISMATCH,
};

struct FixedTupleEncodeResult {
    std::optional<std::vector<std::byte>> tuple;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    std::size_t column_index{0};
    explicit operator bool() const noexcept;
};

struct FixedTupleValidationResult {
    std::optional<TupleHeader> header;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    std::size_t column_index{0};
    explicit operator bool() const noexcept;
};

struct FixedTupleDecodeResult {
    std::optional<FixedTupleValue> value;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    explicit operator bool() const noexcept;
};

FixedTupleEncodeResult EncodeFixedTuple(const TuplePhysicalLayout& layout,
                                        const TupleVersionMetadata& metadata,
                                        std::span<const FixedTupleValue> values);
FixedTupleValidationResult ValidateFixedTuple(const TuplePhysicalLayout& layout,
                                              std::span<const std::byte> tuple) noexcept;
FixedTupleDecodeResult DecodeFixedTupleValue(const TuplePhysicalLayout& layout,
                                             std::span<const std::byte> tuple,
                                             std::size_t column_index) noexcept;
```

## Fixed-value representation chosen

`FixedTupleValue` is a small, non-polymorphic `std::variant` used only for storage construction,
decode results, and tests. `std::monostate` means NULL. BOOLEAN and signed integers are direct fixed
alternatives. Dedicated wrappers keep DATE and TIMESTAMP physically distinct from INT32 and INT64,
so the codec cannot silently coerce them. `Float64PhysicalValue` retains the raw 64-bit payload.

All alternatives live inside the variant; there is no per-column heap allocation. This type is
explicitly not the future execution-engine value or vector representation and is not assumed to be
appropriate for hot tuple/vector loops.

## Exact BOOLEAN representation

Persisted BOOLEAN is exactly one byte:

```text
false = 0x00
true  = 0x01
```

Decode rejects every other byte, including `0x02`, with `INVALID_BOOLEAN`. This representation was
already locked by architecture §74 and did not require a new local choice.

## Signed integer representation

INT32 uses exactly four bytes and INT64 uses exactly eight bytes. Values persist as their signed
two's-complement bit patterns in little-endian byte order through the existing explicit endian
helpers. No pointer cast, struct serialization, native-endian write, or SQL coercion is used.

DATE and TIMESTAMP use the same signed physical integer encoding at their respective widths, while
remaining distinct construction tags.

## FLOAT64 bit representation and special values

FLOAT64 uses exactly eight bytes. Construction uses `std::bit_cast<std::uint64_t>(double)` and then
explicit little-endian integer encoding; decode reverses those operations. The codec preserves all
64 payload bits exactly:

- `+0.0` remains bits `0x0000000000000000`.
- `-0.0` remains bits `0x8000000000000000`.
- positive and negative infinities remain their IEEE-754 binary64 bit patterns.
- NaN sign, quiet/signaling field, and payload bits are not canonicalized by this codec.

Tests include `+0.0`, `-0.0`, `1.0`, `-2.5`, both infinities, and a NaN with payload
`0x7FF8123456789ABC`, with exact bitwise round trips.

## DATE/TIMESTAMP physical treatment

DATE is a raw signed 32-bit physical scalar; TIMESTAMP is a raw signed 64-bit physical scalar. Both
are persisted little-endian and tests pin representative positive and negative exact bytes. This
milestone does not invent an epoch, time zone, calendar, precision, or semantic unit. Architecture
§74 assigns those semantics to the later SQL type layer.

## NULL fixed-area byte convention

The owning tuple vector is initialized entirely to zero. For a NULL column, the encoder sets its
LSB-first bitmap bit and intentionally skips scalar encoding, leaving every reserved fixed-area byte
for that column zero. The fixed bytes remain reserved because offsets come from
`TuplePhysicalLayout`; no compacting occurs around NULL values.

Decode reports `std::monostate` after reading the null bit and does not interpret the fixed bytes as
a semantic scalar. It currently does not reject a nonzero fixed byte under a NULL bit; zero is the
encoder's deterministic convention.

## `HAS_NULLS` derivation and validation

The encoder derives `HAS_NULLS` from actual values, setting it if and only if at least one supplied
column is NULL. Nullable schema declarations alone never set the flag. Fixed-only encoding always
leaves `HAS_VARLEN` clear and sets no other flags.

Validation is canonically strict: `HAS_NULLS` must be set if and only if at least one physical
column bit is set. A set flag with no NULL bit and a NULL bit with a clear flag both return
`FLAG_BITMAP_MISMATCH`. The existing tuple-header decoder rejects unknown flag bits, and fixed-only
validation rejects `HAS_VARLEN`.

The encoder rejects NULL for a `nullable == false` column before allocation. Validation also treats
a persisted NULL bit for a `NOT NULL` column as `NULL_NOT_ALLOWED` corruption relative to the
supplied layout.

## Unused bitmap-bit policy

Because the tuple vector and bitmap start zeroed, unused high bits in the final bitmap byte are
always zero. Fixed tuple validation rejects a nonzero unused high bit as `MALFORMED_TUPLE`. Used bits
retain the architecture-locked LSB-first mapping and set-bit-means-NULL meaning.

## Schema-version and structural validation

The encoder does not accept an independent schema version. It always writes
`TuplePhysicalLayout::SchemaVersion()`, writes `header_bytes = 48`, derives
`null_bitmap_bytes` from `TuplePhysicalLayout::NullBitmapSize()`, derives tuple flags, and writes
`reserved = 0`.

Validation requires enough bytes for `layout.MinimumTupleSize()`, successfully decodes the existing
tuple header, and verifies exact layout schema version and null-bitmap byte count. It propagates the
local tuple-header error detail for wrong header size, unknown flags, nonzero reserved bytes, and a
malformed previous-version sentinel pair. A different layout schema version returns
`SCHEMA_VERSION_MISMATCH`; a truncated tuple returns `MALFORMED_TUPLE`. A fixed-only layout rejects
VARCHAR and `HAS_VARLEN`.

For fixed-only layouts the validator currently requires a sufficient span, not exact equality with
`MinimumTupleSize()`; trailing bytes are ignored by this milestone.

## Encoder ownership/allocation model

`EncodeFixedTuple` returns an owning `std::vector<std::byte>` sized exactly to
`layout.MinimumTupleSize()`. This is one explicit allocation per constructed tuple and is acceptable
for the current storage construction/test path. Scalar encoding, scalar decoding, tuple validation,
and single-column tuple decoding allocate no memory. This API is not the future vectorized executor
representation.

## Failure atomicity

Before allocating output, the encoder validates column count, fixed-only schema scope, every NULL
and nullability state, every supplied physical tag, the inline-size bound, and caller-supplied
previous-version metadata. It stages and validates the complete derived `TupleHeader` in a 48-byte
stack array. An ordinary expected failure therefore returns no tuple. Any defensive failure after
allocation destroys the local vector and still returns no partial tuple to the caller.

## HeapPage composition

The composition test builds a tuple containing BOOLEAN, NULL INT32, and TIMESTAMP values; inserts
the complete encoded bytes through `HeapPage::Insert`; retrieves them through
`HeapPage::TupleBytes`; verifies exact byte equality; and decodes all three columns through the same
`TuplePhysicalLayout`. HeapPage remains an opaque tuple-byte container and its implementation was
not changed.

## Tests/checks run

- Focused Clang fixed-scalar/fixed-tuple codec suite: 17/17 passed.
- Full `clang-debug` build and CTest suite: 146/146 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 146/146 passed with
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer was disabled because this execution environment runs
  under ptrace and LSan aborts during GoogleTest discovery; ASan and UBSan remained enabled.
- Focused `gcc-debug` fixed-scalar/fixed-tuple codec suite: 17/17 passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics after replacing
  potentially throwing variant access and making test optional guards explicit.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.
- No benchmarks were run because this milestone establishes correctness and a construction-time
  storage primitive rather than a measured hot-path optimization.

## Assumptions

- `TuplePhysicalLayout` is valid schema-lifetime metadata produced by
  `BuildTuplePhysicalLayout`; callers do not fabricate its private internal state.
- DATE and TIMESTAMP are opaque signed storage scalars until the SQL type layer defines epoch,
  unit, precision, calendar, and time-zone semantics.
- FLOAT64 support assumes the architecture-required IEEE-754 binary64 representation; the
  compile-time width assertion pins `double` to 64 bits.
- Tuple spans supplied for decoding remain alive for the duration of the call. Decode results own
  only a fixed scalar value, not tuple storage.
- Allocation failure retains standard `std::vector` exception behavior; expected codec validation
  failures use local result enums and return no tuple.
- `ARCHITECTURE.md` remains authoritative; this devlog is an append-only implementation record.

## Known limitations/deferred work

- VARCHAR descriptors, VARCHAR payload copying, VARCHAR decode, and general varlen tuple
  construction are deferred.
- No SQL implicit casts, type semantics, catalog/schema lookup, parser value, execution value,
  execution vector, or vectorized/hot decode representation exists here.
- The per-column decode entry point revalidates tuple structure each call. A future scan path may
  validate once and decode into typed vectors, but that execution representation is deliberately
  outside this milestone.
- Decode accepts trailing bytes after the fixed-only minimum tuple size and does not canonicalize or
  reject fixed-area bytes beneath NULL bits.
- No MVCC visibility, UPDATE/DELETE behavior, HeapFile, FSM, BufferPool, WAL, recovery, or overflow
  storage was added.

## Architecture questions discovered

The implementation follows the requested v1 policies, but the following persisted-format or
validation rules are not all explicitly locked in `ARCHITECTURE.md` and should be synchronized
before format compatibility depends on them:

1. Should v1 explicitly lock that every fixed-area byte reserved for a NULL fixed-width value is
   encoded as zero? Should decode also reject nonzero bytes there, or continue treating them as
   semantically unread padding under the null bit?
2. Should v1 explicitly lock canonical equivalence `HAS_NULLS iff at least one used null-bitmap bit
   is set`, with both mismatch directions rejected?
3. Should v1 explicitly lock unused high bits in the final null-bitmap byte to zero and require
   decoders to reject nonzero unused bits?
4. Should §74 explicitly name two's-complement as the signed persisted representation for INT32,
   INT64, DATE, and TIMESTAMP, rather than specifying only widths, explicit integers, and
   little-endian byte order?
5. Should §74 explicitly lock exact FLOAT64 payload-bit preservation, including signed zero,
   infinities, and noncanonicalized NaN payloads?
6. Should DATE and TIMESTAMP be explicitly described as signed 32-bit and signed 64-bit physical
   scalars now, while their epoch/unit/calendar/time-zone semantics remain deferred to the SQL type
   layer?
7. Should schema-directed validation reject a persisted NULL bit for a `NOT NULL` column, as this
   codec does, or should that check exist only at construction/catalog boundaries?
8. For a fixed-only schema, should a canonical physical tuple length equal
   `TuplePhysicalLayout::MinimumTupleSize()` exactly, or should validators continue accepting a
   sufficient span with trailing bytes as this milestone does?

BOOLEAN `0x00`/`0x01` is not a new question: it is already explicitly locked by architecture §74.
