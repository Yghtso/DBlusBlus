• Implemented explicit persisted codecs for PageId and Rid only.

### Files changed

- src/common/encoding.h:15
  - Added encoded-size constants and four codec functions.

- tests/encoding_test.cpp:13
  - Added focused PageId/Rid codec tests.

### Architecture sections used

- §9 — On-Disk Serialization
- §54 — Fundamental Identifier Types
- §55 — PageId
- §56 — Rid
- §61 — little-endian persisted integers
- §113 — 16-byte persisted RID layout

### Byte layouts

PageId, 12 bytes:

Offset Size Field
━━━━━━━━ ━━━━━━ ━━━━━━━━━━━━━━━━━━━━━━━━
0 4 file_id, little-endian
──────── ────── ────────────────────────
4 8 page_no, little-endian

Rid, 16 bytes:

Offset Size Field
━━━━━━━━ ━━━━━━ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0 4 page.file_id, little-endian
──────── ────── ─────────────────────────────
4 8 page.page_no, little-endian
──────── ────── ─────────────────────────────
12 2 slot, little-endian
──────── ────── ─────────────────────────────
14 2 reserved, encoded as zero

No in-memory struct size, alignment, or padding participates in either format.

### Public API

inline constexpr std::size_t PAGE_ID_ENCODED_SIZE = 12;
inline constexpr std::size_t RID_ENCODED_SIZE = 16;

[[nodiscard]] constexpr bool
EncodePageId(std::span<std::byte> destination,
const PageId& page_id) noexcept;

[[nodiscard]] constexpr std::optional<PageId>
DecodePageId(std::span<const std::byte> source) noexcept;

[[nodiscard]] constexpr bool
EncodeRid(std::span<std::byte> destination,
const Rid& rid) noexcept;

[[nodiscard]] constexpr std::optional<Rid>
DecodeRid(std::span<const std::byte> source) noexcept;

Undersized encodes return false before modifying the destination. Undersized decodes return std::nullopt.

### Checks run

- Focused Clang codec tests: 10/10 passed.
- Full clang-debug suite: 24/24 passed.
- Clang-tidy: clean; focused tests passed.
- GCC focused tests: 10/10 passed.
- Clang-format check: passed.
- git diff --check: passed.

### Assumptions

- The 12-byte PageId format is the first 12 bytes of the locked RID layout.
- RID reserved bytes are always zeroed when encoding. Decoding currently ignores their value because the contract does not require rejecting nonzero reserved bytes.
- Inputs larger than the encoded size are accepted; codecs consume only the leading fixed-size representation.
