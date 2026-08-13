• Implemented the scoped 32-byte common page header and explicit codec. No later
storage, checksum, WAL, or file functionality was added.

### Files changed

- src/common/page_header.h:1 — PageType, CommonPageHeader, encoded-size constant, and
  codec.

- tests/page_header_test.cpp:1 — 8 focused codec tests.
- src/CMakeLists.txt:10 — registered the public header.
- tests/CMakeLists.txt:11 — registered the test source.

### Architecture sections used

- §9 — On-Disk Serialization
- §58 — File Kinds and page-file conventions
- §59 — File Superblock context
- §61 — Common Page Header
- §62 — Page Types
- §63 — Page Checksums

### In-memory representation

PageType is a fixed std::uint16_t enum containing:

SUPERBLOCK, HEAP_DATA, FSM_DATA, BTREE_INTERNAL, BTREE_LEAF, BTREE_FREE, and
CATALOG_DATA.

CommonPageHeader contains:

- PageType page_type
- std::uint16_t format_version
- std::uint32_t flags
- Lsn page_lsn
- std::uint32_t checksum_crc32c
- std::uint16_t header_size
- std::uint16_t reserved16
- PageNo page_no

Defaults include INVALID_LSN, INVALID_PAGE_NO, a 32-byte header_size, and zeroed
flags/checksum/reserved field. Equality is supported; no ordering semantics were
added.

### Persisted layout

Offset Size Field
━━━━━━━━ ━━━━━━ ━━━━━━━━━━━━━━━━━
0 2 page_type
──────── ────── ─────────────────
2 2 format_version
──────── ────── ─────────────────
4 4 flags
──────── ────── ─────────────────
8 8 page_lsn
──────── ────── ─────────────────
16 4 checksum_crc32c
──────── ────── ─────────────────
20 2 header_size
──────── ────── ─────────────────
22 2 reserved16
──────── ────── ─────────────────
24 8 page_no

All fields use the existing explicit little-endian helpers. No struct memory, padding,
alignment, or host byte order is persisted.

### Public API

inline constexpr std::size_t COMMON_PAGE_HEADER_ENCODED_SIZE = 32;

enum class PageType : std::uint16_t;

struct CommonPageHeader;

bool EncodeCommonPageHeader(
std::span<std::byte> destination,
const CommonPageHeader& header) noexcept;

std::optional<CommonPageHeader> DecodeCommonPageHeader(
std::span<const std::byte> source) noexcept;

### Validation

- Encoding rejects destinations smaller than 32 bytes before writing anything.
- Decoding rejects sources smaller than 32 bytes with std::nullopt.
- Unknown flag bits and explicit reserved values are preserved.
- The default reserved value encodes deterministically as zero.
- No version, checksum, expected page type, header_size, or expected page_no
  validation was invented; those require page-type/file context in later subsystems.

### Checks run

- Clang focused page-header suite: 8/8 passed
- Full clang-debug CTest suite: 32/32 passed
- GCC focused page-header suite: 8/8 passed
- Clang-tidy preset, verbose: clean
- Clang-format dry-run: clean
- git diff --check: clean

The initial format dry-run found formatting differences in the two new files; clang-
format was applied only to those files before final verification.

### Assumption

The architecture names page types but does not assign numeric codes. They are encoded
sequentially in listed order: SUPERBLOCK = 0 through CATALOG_DATA = 6. Tests now pin
every code to prevent persisted-format drift.
