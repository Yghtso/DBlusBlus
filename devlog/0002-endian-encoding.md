• Implemented only foundational fixed-width little-endian encoding helpers.

### Files changed

- src/common/encoding.h:1 — new header-only primitive codec.
- src/CMakeLists.txt:9 — registers the public header.
- tests/encoding_test.cpp:1 — focused codec tests.
- tests/CMakeLists.txt:10 — adds the test source.

### Architecture sections used

- §9 — On-Disk Serialization
- §54 — Fundamental Identifier Types
- §§55–56 — PageId and Rid
- §59 — explicit superblock serialization
- §61 — little-endian page-header fields
- §74 — Fixed-Width Values
- §76 — safe unaligned field access
- §113 — persisted RID field widths

### Public API

Declared in common/encoding.h:

template <detail::FixedWidthInteger Integer>
[[nodiscard]] constexpr bool
EncodeLittleEndian(std::span<std::byte> destination, Integer value) noexcept;

template <detail::FixedWidthInteger Integer>
[[nodiscard]] constexpr std::optional<Integer>
DecodeLittleEndian(std::span<const std::byte> source) noexcept;

Supported types are signed and unsigned 8-, 16-, 32-, and 64-bit integers.

Behavior:

- Encoding writes exactly sizeof(Integer) bytes at the beginning of the span.
- An undersized destination returns false without modifying it.
- An undersized source returns std::nullopt.
- Operations are byte-based and safe at unaligned offsets.
- Signed values preserve their fixed-width two’s-complement representation.
- No pointer reinterpretation, raw struct serialization, or heap allocation is used.

### Checks run

- Encoding-focused Clang tests: 7/7 passed.
- Full clang-debug build and tests: 14/14 passed.
- Fresh clang-tidy build: clean, with focused tests passing.
- GCC focused build and tests: 7/7 passed.
- clang-format --dry-run --Werror: passed.
- git diff --check: passed.

### Assumptions

- Supporting the complete fixed-width integer family keeps the primitive API uniform without introducing a serialization framework.
- Larger spans are accepted; only the first required bytes are consumed.
- A binary reader/writer abstraction is deferred until sequential composite formats demonstrate a need.
- PageId and Rid persisted codecs remain deferred as requested.
