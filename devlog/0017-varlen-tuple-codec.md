# 0017 — VARCHAR and General Varlen Tuple Codec

Date: 2026-08-14

## Milestone/task

Phase 1: VARCHAR descriptor/payload support and general fixed/varlen physical tuple
construction, validation, and decoding against `TuplePhysicalLayout`.

## Scope

Extended the storage tuple codec so one physical tuple can contain any mixture of `BOOLEAN`,
`INT32`, `INT64`, `FLOAT64`, `DATE`, `TIMESTAMP`, `VARCHAR`, and NULL. Added explicit VARCHAR
descriptor encoding/decoding, canonical schema-order payload construction, strict descriptor and
tuple-length validation, non-owning VARCHAR input/decode views, and mixed-tuple HeapPage and durable
PageFile composition tests.

No HeapFile, FSM, MVCC visibility, UPDATE/DELETE semantics, BufferPool, page latches, WAL, recovery,
catalog lookup, SQL `VARCHAR(n)` enforcement, UTF-8 validation, collation, text comparison,
overflow/TOAST storage, parser/binder value, execution value, or execution `StringRef` was added.

## Files changed

- `src/storage/tuple_codec.h` — adds the descriptor codec API and non-owning VARCHAR value; renames
  the fixed-only construction/result API to the general tuple codec API; adds varlen validation
  errors and documents view lifetimes/allocation intent.
- `src/storage/tuple_codec.cpp` — implements descriptor serialization, mixed tuple size planning and
  construction, payload copying, canonical flag/descriptor/length validation, and borrowed VARCHAR
  decode.
- `tests/tuple_codec_test.cpp` — updates fixed-width regression tests to the generalized tuple API.
- `tests/tuple_varlen_codec_test.cpp` — adds descriptor, mixed tuple, limit, corruption, HeapPage,
  and persistence tests.
- `tests/CMakeLists.txt` — registers the varlen codec test source.
- `devlog/0017-varlen-tuple-codec.md` — records this milestone.

No older devlog entry, `ARCHITECTURE.md`, TuplePhysicalLayout, TupleHeader, HeapPage, PageFile, or
DiskManager implementation was modified.

## Architecture sections used

- §49 — Phase 1 raw-storage implementation order
- §69 — strict v1 maximum inline raw tuple size of 8135 bytes
- §70 — fixed 48-byte TupleHeader and previous-version pointer invariants
- §71 — `HAS_NULLS`, `HAS_VARLEN`, and the known v1 tuple-flag mask
- §72 — schema-directed compact tuple data layout
- §73 — null bitmap, canonical `HAS_NULLS`, unused bits, and nullability validation
- §74 — locked fixed-width scalar representations
- §75 — inline VARCHAR descriptor and tuple-relative offsets
- §76 — compact unaligned persistence and explicit endian access
- §77 — tuple-carried schema version
- §96 — TupleCodec as the schema-directed storage encode/decode boundary

`ARCHITECTURE.md` remains authoritative. The varlen-specific persisted policies selected by this
task but not yet locked there are listed as architecture questions below.

## Exact public API introduced or generalized

The descriptor API added in `storage/tuple_codec.h` is:

```cpp
struct VarcharDescriptor {
    std::uint32_t payload_offset{0};
    std::uint32_t payload_length{0};
    bool operator==(const VarcharDescriptor&) const = default;
};

enum class VarcharDescriptorCodecError : std::uint8_t {
    NONE,
    BUFFER_TOO_SMALL,
};

struct VarcharDescriptorDecodeResult {
    std::optional<VarcharDescriptor> descriptor;
    VarcharDescriptorCodecError error{VarcharDescriptorCodecError::NONE};
    explicit operator bool() const noexcept;
};

VarcharDescriptorCodecError
EncodeVarcharDescriptor(std::span<std::byte> destination,
                        const VarcharDescriptor& descriptor) noexcept;
VarcharDescriptorDecodeResult
DecodeVarcharDescriptor(std::span<const std::byte> source) noexcept;
```

The construction/decode representation was generalized from `FixedTupleValue` to:

```cpp
struct VarcharValue {
    std::span<const std::byte> bytes;
    bool operator==(const VarcharValue& other) const noexcept;
};

using TupleValue = std::variant<std::monostate,
                                bool,
                                std::int32_t,
                                std::int64_t,
                                Float64PhysicalValue,
                                DatePhysicalValue,
                                TimestampPhysicalValue,
                                VarcharValue>;
```

The fixed-only tuple API and result names were generalized to:

```cpp
enum class TupleCodecError : std::uint8_t {
    NONE,
    COLUMN_COUNT_MISMATCH,
    COLUMN_OUT_OF_RANGE,
    TYPE_MISMATCH,
    NULL_NOT_ALLOWED,
    VARCHAR_LENGTH_TOO_LARGE,
    TUPLE_TOO_LARGE,
    INVALID_HEADER_METADATA,
    MALFORMED_TUPLE,
    SCHEMA_VERSION_MISMATCH,
    INVALID_BOOLEAN,
    FLAG_BITMAP_MISMATCH,
    VARLEN_FLAG_MISMATCH,
    INVALID_VARLEN_DESCRIPTOR,
    VARLEN_OFFSET_MISMATCH,
    TRAILING_BYTES,
};

struct TupleEncodeResult {
    std::optional<std::vector<std::byte>> tuple;
    TupleCodecError error{TupleCodecError::NONE};
    std::size_t column_index{0};
    explicit operator bool() const noexcept;
};

struct TupleValidationResult {
    std::optional<TupleHeader> header;
    TupleCodecError error{TupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    std::size_t column_index{0};
    explicit operator bool() const noexcept;
};

struct TupleDecodeResult {
    std::optional<TupleValue> value;
    TupleCodecError error{TupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    explicit operator bool() const noexcept;
};

TupleEncodeResult EncodeTuple(const TuplePhysicalLayout& layout,
                              const TupleVersionMetadata& metadata,
                              std::span<const TupleValue> values);
TupleValidationResult ValidateTuple(const TuplePhysicalLayout& layout,
                                    std::span<const std::byte> tuple) noexcept;
TupleDecodeResult DecodeTupleValue(const TuplePhysicalLayout& layout,
                                   std::span<const std::byte> tuple,
                                   std::size_t column_index) noexcept;
```

`FixedScalarDecodeResult` and the fixed scalar codec signatures now use `TupleValue`; their fixed
scalar behavior and `FixedScalarCodecError` contract remain otherwise unchanged.

## VARCHAR input representation

`VarcharValue` contains `std::span<const std::byte>`. It is a non-owning opaque byte view whose
backing storage only needs to remain valid during `EncodeTuple`. It performs no UTF-8, collation,
locale, character-count, or null-terminator processing. `TupleValue` remains a small
construction/testing and storage-decode variant, not an execution-engine value abstraction.

## VARCHAR decode/view representation

`DecodeTupleValue` returns a `TupleValue` containing `VarcharValue` for a present VARCHAR. Its span
points directly into the caller-provided tuple bytes; no string or payload allocation/copy occurs.
The view must not outlive the tuple span's backing storage. NULL remains `std::monostate` and does
not expose descriptor or payload bytes semantically.

## Descriptor byte layout and offset semantics

Every VARCHAR owns exactly eight bytes at its precomputed fixed-area offset:

```text
descriptor +0..3 = uint32 payload_offset, little-endian
descriptor +4..7 = uint32 payload_length, little-endian
```

Descriptor helpers use existing explicit endian functions and support unaligned spans; no C++
struct representation is serialized. `payload_offset` is absolute relative to tuple byte zero and
must point at or after `TuplePhysicalLayout::VarlenPayloadOffset()`. Checked 32-bit addition is used
before comparing the descriptor end with the tuple size.

## Canonical payload packing rule

Present VARCHAR payloads are copied consecutively in physical schema-column order, beginning at
`layout.VarlenPayloadOffset()`. Every present descriptor offset must equal the current expected
payload cursor. The cursor advances by that descriptor's length, so gaps, backward offsets,
overlaps, reordered ranges, fixed-area references, and out-of-bounds ranges are rejected.

Payload bytes are opaque and copied exactly. NULL VARCHAR values copy no bytes and do not advance
the cursor.

## NULL VARCHAR descriptor convention

A NULL VARCHAR sets its LSB-first null bit, retains its eight-byte fixed slot, and writes the
canonical descriptor `(payload_offset = 0, payload_length = 0)`. Strict validation rejects any
other descriptor under a NULL VARCHAR bit. This is distinct from the architecture's fixed-width
NULL rule and is a new persisted-format policy question.

## Zero-length non-NULL VARCHAR behavior

A present empty VARCHAR has a clear null bit, `payload_length = 0`, and `payload_offset` equal to
the current canonical payload cursor. It copies no bytes and leaves the cursor unchanged. Multiple
empty present values may therefore point at the same cursor. Empty and NULL remain distinguishable
through the null bitmap and the NULL descriptor convention.

## `HAS_VARLEN` definition and validation

The encoder sets `HAS_VARLEN` if and only if `layout.HasVarlenColumns()` is true. The flag therefore
describes the tuple's physical schema/layout, not whether this tuple happens to contain nonzero
payload bytes. It remains set when every VARCHAR is NULL or empty and is clear for a fixed-only
schema. Validation requires the same exact equivalence and returns `VARLEN_FLAG_MISMATCH` in either
direction.

`HAS_NULLS` retains the architecture-locked per-tuple rule: it is set if and only if at least one
used bitmap bit is set. Nullable declarations alone do not set it. Unknown flags, unused bitmap
bits, and NULL bits on `NOT NULL` columns remain invalid.

## Exact tuple-length policy

Canonical tuple length is exactly:

```text
layout.MinimumTupleSize()
+ sum(length of each non-NULL VARCHAR in schema order)
```

The encoder calls `TuplePhysicalLayout::PlanTupleSize`, allocates exactly that total, and requires
its final payload cursor to equal the plan. Validation requires the final descriptor-walk cursor to
equal `tuple.size()`. It rejects unreferenced trailing bytes with `TRAILING_BYTES`, including for a
fixed-only schema, and rejects truncated/referenced-out-of-bounds payloads. The 8135-byte tuple size
is accepted; 8136 bytes are rejected.

## Descriptor and corruption validation

`ValidateTuple` requires a decodable canonical TupleHeader, matching schema version and null bitmap
size, known flags, exact `HAS_NULLS`/bitmap and `HAS_VARLEN`/layout equivalence, zero unused bitmap
bits, no persisted NULL on a `NOT NULL` column, enough bytes for the header/bitmap/fixed area, and a
decodable descriptor for every VARCHAR. For present VARCHAR values it checks payload-region lower
bounds, checked `offset + length`, tuple upper bounds, exact schema-order cursor equality, and final
exact length. For NULL VARCHAR values it requires `(0,0)`.

Tests exercise offsets before the payload, beyond tuple end, overflow-style descriptors, overlaps,
gaps, schema-order mismatches, noncanonical NULL descriptors, wrong flags, malformed bitmaps,
trailing bytes, and truncation. Corruption returns local result errors without exposing a value or
partially decoded payload.

## Allocation and copy behavior

Encoding validates column count, type tags, nullability, and VARCHAR input lengths while collecting
one `VarlenValueSize` per VARCHAR in a single temporary vector reserved to the layout's exact varlen
column count. It calls `PlanTupleSize` before output allocation, stages and validates the derived
header, then performs one allocation for an output vector of the exact planned size. The temporary
planner vector is one contiguous scratch allocation when the layout has VARCHAR columns; there are
no per-column allocations.

Each present VARCHAR is copied exactly once, directly from its input span into its final tuple
payload position. The output vector is zero-initialized, so fixed-width NULL slots remain zero;
VARCHAR NULL descriptors are explicitly encoded as `(0,0)`. Descriptor codecs, validation, and
decode are allocation-free. This owning construction API is not intended as the future vectorized
execution hot path.

## Failure atomicity

Expected failures in column count, type matching, nullability, VARCHAR representation, tuple size,
or header metadata return no tuple. Size planning completes before output allocation. Once the
exact output vector exists, encoding consists of deterministic descriptor/scalar writes and direct
payload copies plus defensive invariant checks; a defensive failure destroys the local vector and
still returns no partial tuple. Allocation failure retains standard `std::vector` exception
behavior.

## HeapPage persistence composition

The HeapPage composition test encodes a mixed INT64/VARCHAR/BOOLEAN tuple, inserts it with
`HeapPage::Insert`, retrieves it by the original `SlotId`, validates it, decodes every column, and
verifies the VARCHAR result borrows from the stored tuple bytes.

The persistence test creates a HEAP PageFile, allocates and initializes a page, inserts an
INT32/VARCHAR/TIMESTAMP tuple, writes and syncs it through DiskManager, reopens the PageFile, reads
and validates the HeapPage, retrieves the original slot, and decodes the exact fixed and VARCHAR
values. HeapPage, PageFile, and DiskManager remain unchanged and treat the encoded tuple as opaque
bytes.

## Tests/checks run

- Focused Clang scalar/fixed/general-varlen codec and persistence suite: 29/29 passed.
- Full `clang-debug` CTest suite: 158/158 passed.
- Full `clang-asan` ASan+UBSan CTest suite: 158/158 passed with
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer was disabled for this ptrace execution environment;
  AddressSanitizer and UndefinedBehaviorSanitizer remained enabled.
- Focused `gcc-debug` scalar/fixed/general-varlen codec and persistence suite: 29/29 passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.
- No benchmarks were run because this milestone is a correctness-focused storage construction
  primitive, not a measured execution-path optimization.

## Assumptions

- `TuplePhysicalLayout` is valid schema-lifetime metadata produced by
  `BuildTuplePhysicalLayout`; callers do not fabricate its private state.
- Input `VarcharValue` spans remain valid for the duration of `EncodeTuple`.
- Decoded VARCHAR views do not outlive the backing tuple storage and callers do not mutate/reallocate
  that storage while using the view.
- VARCHAR bytes are opaque storage bytes; SQL text semantics are intentionally absent.
- DATE and TIMESTAMP remain the architecture-locked raw signed physical scalars; their SQL epoch,
  unit, precision, calendar, and time-zone semantics remain deferred.
- `ARCHITECTURE.md` remains authoritative; this devlog is an append-only implementation record.

## Known limitations/deferred work

- No SQL `VARCHAR(n)` length semantics, UTF-8 validation, collation, locale, text comparison, or
  terminator convention is implemented.
- No overflow/TOAST pages exist; every complete tuple must fit within 8135 bytes.
- No execution-engine `StringRef`, parser/binder value, typed vector, SQL coercion, catalog lookup,
  or schema-version translation is implemented.
- Per-column decode currently validates the complete tuple on each call. A future scan path can
  validate once and decode into typed execution vectors; that representation is outside this
  milestone.
- No HeapFile, FSM, MVCC visibility, UPDATE/DELETE semantics, BufferPool, page latches, WAL, or
  recovery was added.

## Architecture questions discovered

Architecture §75 locks the eight-byte descriptor, tuple-relative offsets, inline payload, and lack
of a terminator, but does not yet explicitly lock the following persisted v1 rules selected here:

1. Should `HAS_VARLEN` be locked as set if and only if the interpreting physical schema contains at
   least one VARCHAR column, including tuples where all VARCHAR values are NULL or empty?
2. Should a NULL VARCHAR descriptor be locked to exactly `(offset = 0, length = 0)`, with strict
   readers rejecting every other descriptor under a set NULL bit?
3. Should all present VARCHAR payloads be locked to canonical schema-column-order, no-gap packing
   beginning at `TuplePhysicalLayout::VarlenPayloadOffset()`, with empty present values pointing at
   the current cursor?
4. Should canonical physical tuple length be locked to exactly `MinimumTupleSize() +` the sum of
   non-NULL VARCHAR lengths, with trailing unreferenced bytes rejected for both varlen and
   fixed-only schemas?

These policies should be synchronized into `ARCHITECTURE.md` before persistent-format compatibility
depends on them.
