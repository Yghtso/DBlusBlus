# 0018 — Persisted FSM Page Format

Date: 2026-08-14

## Milestone/task

Phase 1: deterministic free-space categories and the initial persisted flat `FSM_DATA` page
format.

## Scope

Added integer-only conversion between current heap-page contiguous free bytes and a persisted
one-byte approximate free-space category, a conservative category lower-bound interpretation, O(1)
heap-page-to-FSM-entry mapping, explicit FSM-specific header codecs, and a lightweight `FsmPage`
view over an existing `Page`. The page view supports deterministic initialization, structural
validation, and allocation-free reads/updates of already initialized entries.

Added focused exact-byte, boundary, exhaustive monotonicity, corruption, failure-atomicity, and
PageFile/DiskManager persistence tests.

No HeapFile, relation-wide candidate search, runtime bucketed candidate sets, stale-entry retry or
repair, heap scan/rebuild, automatic HeapPage integration, concurrent maintenance, BufferPool,
buffer frames, page guards, CLOCK, latches, dirty tracking, WAL, recovery, transactions, MVCC, or
B+ tree behavior was added.

## Files changed

- `src/storage/fsm_page.h` — declares the category conversion, flat mapping, persisted FSM header,
  local result/error types, format constants, and lightweight `FsmPage` API.
- `src/storage/fsm_page.cpp` — implements explicit header encoding, checked mapping arithmetic,
  page initialization, structural validation, and entry access/update.
- `src/CMakeLists.txt` — registers the new production source and public header.
- `tests/fsm_page_test.cpp` — adds category, mapping, header, initialization, corruption,
  atomicity, and persistence tests.
- `tests/CMakeLists.txt` — registers the FSM test source.
- `devlog/0018-fsm-page-format.md` — records this milestone.

No older devlog entry, `ARCHITECTURE.md`, Page, PageFile, HeapPage, common-header, DiskManager, or
encoding-helper implementation was modified.

## Architecture sections used

- §9 — explicit little-endian persisted encoding and per-page format versions
- §13 — free-space discovery as a separate storage subsystem
- §49 — Phase 1 raw-storage implementation order
- §58 — persisted `FileKind::FSM = 3`, page 0 superblock, and data pages beginning at page 1
- §59 — common-header-backed file superblock and FSM object identity
- §60 — append-first data-page allocation
- §61 — locked 32-byte common page header and pre-WAL `page_lsn` behavior
- §62 — persisted `PageType::FSM_DATA = 2`
- §63 — staged page checksum behavior before WAL/recovery
- §64 — separate heap and FSM relation files
- §66 and §68 — heap `upper - lower` free-space geometry and per-insert slot cost
- §69 — strict 8135-byte maximum raw inline tuple payload
- §82 — separate approximate advisory FSM and conceptual 0–255 mapping
- §83 — stale FSM persistence and rebuild tolerance
- §96 — FreeSpaceMap storage-object responsibility and page-layer boundaries

`ARCHITECTURE.md` remains authoritative. The exact persisted FSM conventions selected here but not
yet locked there are listed as architecture questions below.

## Exact public API introduced

The primary constants and conversion/mapping API in `storage/fsm_page.h` are:

```cpp
inline constexpr std::uint16_t FSM_PAGE_FORMAT_VERSION = 1;
inline constexpr std::size_t FSM_PAGE_HEADER_OFFSET = 32;
inline constexpr std::size_t FSM_PAGE_HEADER_ENCODED_SIZE = 16;
inline constexpr std::uint16_t FSM_PAGE_TOTAL_HEADER_SIZE = 48;
inline constexpr std::size_t FSM_PAGE_ENTRIES_OFFSET = 48;
inline constexpr std::size_t FSM_PAGE_ENTRY_CAPACITY = 8144;
inline constexpr std::size_t FSM_MAX_CONTIGUOUS_FREE_BYTES = 8144;
inline constexpr std::size_t FSM_MAX_USABLE_INSERTION_BYTES = 8135;

std::uint8_t FsmCategoryForFreeBytes(std::size_t free_bytes) noexcept;
std::size_t FsmCategoryMinimumUsableBytes(std::uint8_t category) noexcept;

struct FsmEntryLocation {
    PageNo fsm_page_no{INVALID_PAGE_NO};
    std::uint16_t entry_index{0};
};

enum class FsmPageMappingError : std::uint8_t {
    NONE,
    INVALID_HEAP_PAGE_NUMBER,
    OVERFLOW,
};

struct FsmPageMappingResult {
    std::optional<FsmEntryLocation> location;
    FsmPageMappingError error{FsmPageMappingError::NONE};
    explicit operator bool() const noexcept;
};

FsmPageMappingResult FsmLocationForHeapPage(PageNo heap_page_no) noexcept;
```

The persisted header and explicit codec API are:

```cpp
struct FsmPageHeader {
    PageNo first_heap_page_no{1};
    std::uint16_t entry_count{0};
    std::uint16_t reserved16{0};
    std::uint32_t reserved32{0};
};

bool EncodeFsmPageHeader(std::span<std::byte> destination,
                         const FsmPageHeader& header) noexcept;
std::optional<FsmPageHeader>
DecodeFsmPageHeader(std::span<const std::byte> source) noexcept;
```

The page-view API is:

```cpp
struct FsmPageInitialization {
    std::uint16_t entry_count{0};
    std::uint32_t flags{0};
    Lsn page_lsn{INVALID_LSN};
};

class FsmPage {
  public:
    explicit FsmPage(Page& page) noexcept;

    FsmPageInitializeResult
    Initialize(const FsmPageInitialization& initialization = {}) noexcept;
    std::optional<FsmPageHeader> Header() const noexcept;
    FsmPageValidationResult Validate() const noexcept;
    FsmPageCategoryResult GetCategory(std::size_t entry_index) const noexcept;
    FsmPageUpdateResult SetCategory(std::size_t entry_index,
                                    std::uint8_t category) noexcept;
};
```

The local initialization/validation/entry result API is:

```cpp
enum class FsmPageInitializeError : std::uint8_t {
    NONE,
    INVALID_FSM_PAGE_NUMBER,
    HEAP_PAGE_RANGE_OVERFLOW,
    ENTRY_COUNT_OUT_OF_RANGE,
    ENCODING_FAILED,
};

struct FsmPageInitializeResult {
    FsmPageInitializeError error{FsmPageInitializeError::NONE};
    explicit operator bool() const noexcept;
};

enum class FsmPageValidationError : std::uint8_t {
    NONE,
    COMMON_HEADER_DECODE_FAILED,
    WRONG_PAGE_TYPE,
    WRONG_PAGE_NUMBER,
    WRONG_HEADER_SIZE,
    UNSUPPORTED_FORMAT_VERSION,
    NONZERO_COMMON_RESERVED,
    FSM_HEADER_DECODE_FAILED,
    INVALID_FSM_PAGE_NUMBER,
    FIRST_HEAP_PAGE_OVERFLOW,
    FIRST_HEAP_PAGE_MISMATCH,
    ENTRY_COUNT_OUT_OF_RANGE,
    INITIALIZED_HEAP_RANGE_OVERFLOW,
    NONZERO_UNINITIALIZED_ENTRY,
    NONZERO_FSM_RESERVED,
};

struct FsmPageValidationResult {
    std::optional<CommonPageHeader> common_header;
    std::optional<FsmPageHeader> fsm_header;
    FsmPageValidationError error{FsmPageValidationError::NONE};
    std::size_t entry_index{0};
    explicit operator bool() const noexcept;
};

enum class FsmPageEntryError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    ENTRY_OUT_OF_RANGE,
};

struct FsmPageCategoryResult {
    std::optional<std::uint8_t> category;
    FsmPageEntryError error{FsmPageEntryError::NONE};
    FsmPageValidationError page_error{FsmPageValidationError::NONE};
    explicit operator bool() const noexcept;
};

struct FsmPageUpdateResult {
    FsmPageEntryError error{FsmPageEntryError::NONE};
    FsmPageValidationError page_error{FsmPageValidationError::NONE};
    explicit operator bool() const noexcept;
};
```

These types provide local, non-throwing error details for invalid page numbers/ranges, corrupt
structure, and out-of-range entry access.

## Exact category mapping formula

The input to `FsmCategoryForFreeBytes` is a validated heap page's current contiguous physical gap:

```text
free_bytes = upper - lower
```

The v1 conversion is:

```text
bounded_free = min(free_bytes, 8144)

usable_insertion_bytes =
    min(max(bounded_free - 8, 0), 8135)

category =
    floor(usable_insertion_bytes * 255 / 8135)
```

All operations use integer arithmetic. The operands are bounded before multiplication, so
`usable_insertion_bytes * 255` cannot overflow `std::size_t` on a conforming target capable of
holding an 8192-byte page. Values above the physical 8144-byte free-gap maximum are deliberately
clamped. Category 0 is least space and category 255 is greatest space.

Pinned boundaries include:

```text
free bytes 0, 1, 8, 9, 39 -> category 0
free bytes 40             -> category 1
free bytes 4075           -> category 127
free bytes 8142           -> category 254
free bytes 8143, 8144     -> category 255
```

The complete practical free-byte domain `0..8144` is tested exhaustively for monotonicity.

## Category inverse/lower-bound semantics

`FsmCategoryMinimumUsableBytes(category)` returns the inclusive lower bound, in tuple payload
bytes after the mandatory slot deduction, for every page that maps to that category:

```text
minimum_usable(category) = ceil(category * 8135 / 255)

implemented as:

(category * 8135 + 254) / 255
```

This is neither an upper bound nor a midpoint. It never overstates the represented lower bound for
the deterministic conversion. Examples are category 0 -> 0 bytes, 1 -> 32, 127 -> 4052,
254 -> 8104, and 255 -> 8135.

A later candidate search may compare this conservative represented value with requested tuple
bytes, but stale FSM state still means the real page must be fetched and checked before insertion.

## Definition of usable insertion bytes

The raw gap domain is:

```text
PAGE_SIZE - HEAP_PAGE_TOTAL_HEADER_SIZE
= 8192 - 48
= 8144 bytes
```

The FSM's useful insertion-capacity domain always reserves the current append-path slot cost:

```text
HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE = 8 bytes
```

It then clamps tuple capacity to the locked strict maximum:

```text
HEAP_PAGE_MAX_RAW_TUPLE_SIZE = 8135 bytes
```

Thus the category describes conservative tuple payload capacity assuming a new slot is required,
not the raw `upper - lower` byte count. This matches the current append-only HeapPage insertion path.
A future reusable-slot path may have eight additional usable bytes, but this persisted interpretation
remains conservative rather than overstating space.

## Exact `FSM_DATA` page format

The complete 8192-byte page is:

```text
offset  size  field
------  ----  -------------------------------------------------
0       32    common page header
32      8     first_heap_page_no, uint64 little-endian
40      2     entry_count, uint16 little-endian
42      2     FSM reserved16 = 0
44      4     FSM reserved32 = 0
48      8144  one-byte category entries
---------------------------------------------------------------
total   8192
```

The entry region has no C++ array/struct serialization. Individual categories are exact bytes, and
all multi-byte fields use the existing explicit little-endian helpers. The header codec stages all
16 bytes before copying to its destination, so an undersized expected failure does not mutate the
destination.

## FSM format version and header size

The chosen persisted values are:

```text
page_type      = FSM_DATA (persisted code 2)
format_version = 1
header_size    = 48
```

`header_size` includes the 32-byte common header and 16-byte FSM-specific header. The entry region
starts at byte 48.

## FSM-specific field offsets

Relative to tuple byte zero, the FSM-specific fields are:

```text
first_heap_page_no = offset 32, width 8
entry_count        = offset 40, width 2
reserved16         = offset 42, width 2
reserved32         = offset 44, width 4
entries            = offset 48
```

Both reserved fields are written as zero and must be zero during v1 validation.

## Entries per page

There are exactly:

```text
8192 - 48 = 8144
```

one-byte entry slots in each `FSM_DATA` page. Each byte value from 0 through 255 is valid for an
initialized entry. There is no hierarchical level, tree node, pointer, or per-entry metadata.

## Heap PageNo to FSM page/entry mapping

Heap page 0 and `INVALID_PAGE_NO` are rejected. For every ordinary heap data page:

```text
heap_data_index = heap_page_no - 1
fsm_page_index  = heap_data_index / 8144
fsm_page_no     = 1 + fsm_page_index
entry_index     = heap_data_index % 8144
```

FSM file page 0 remains the FileSuperblock; `FSM_DATA` pages begin at page 1. Mapping arithmetic is
checked and returns a local error rather than wrapping. Tests pin page 1 -> FSM page 1/entry 0,
heap page 8144 -> FSM page 1/entry 8143, heap page 8145 -> FSM page 2/entry 0, later boundaries, and
the maximum non-sentinel heap PageNo.

For an FSM file page number `P`, initialization derives and persists:

```text
first_heap_page_no = 1 + (P - 1) * 8144
```

It rejects FSM page 0, `INVALID_PAGE_NO`, multiplication/addition overflow, and initialized ranges
that would include the invalid heap-page sentinel.

## `entry_count` semantics

`entry_count` is the number of currently initialized entries in a contiguous prefix of the page:

```text
valid entry indices = [0, entry_count)
```

The prefix corresponds to currently represented existing heap pages beginning at
`first_heap_page_no`. `entry_count` may be zero and may not exceed 8144. Every entry at or beyond
`entry_count` must be zero; validation rejects a nonzero uninitialized suffix byte. `GetCategory`
and `SetCategory` reject indices outside the initialized prefix. This milestone does not add the
later operation that grows the prefix as new heap pages are allocated.

## Initialization state

`FsmPage::Initialize` validates the FSM page number, derived heap range, and entry count before
mutating the page. It stages the common and FSM-specific headers, then zeroes all 8192 bytes and
writes those headers. Therefore every initialized category and every uninitialized suffix byte
starts at category byte 0.

The common header derives `page_no` from `Page::Id().page_no`, sets `FSM_DATA`, format version 1,
header size 48, `reserved16 = 0`, and checksum 0. Flags and page LSN come from the explicit
`FsmPageInitialization`; their defaults are 0 and `INVALID_LSN`.

Invalid page number, count, or represented range leaves the original page bytes unchanged. Entry
updates validate the page and bounds first, then mutate exactly the selected one-byte entry.

## Structural validation

Validation checks:

- the common header decodes, has `PageType::FSM_DATA`, and stores the actual `Page::Id().page_no`;
- format version is 1, total header size is 48, and common `reserved16` is zero;
- the FSM-specific header decodes and both reserved fields are zero;
- the FSM page number is an ordinary data-page number and its derived first heap page is
  representable;
- persisted `first_heap_page_no` exactly matches the deterministic mapping for that FSM page;
- `entry_count <= 8144` and its initialized heap-page range avoids overflow/sentinel values;
- the fixed header and entry geometry exactly fill `PAGE_SIZE`;
- every uninitialized suffix entry is zero.

All byte values are valid categories for initialized entries, so no additional per-entry semantic
corruption state exists. Validation performs no heap-file I/O and allocates no memory.

## Checksum and page-LSN behavior

Initialization writes `checksum_crc32c = 0`, matching current pre-WAL ordinary-page staging. This
milestone does not calculate or verify whole-page CRC32C for `FSM_DATA` pages. Page LSN defaults to
`INVALID_LSN`; a caller may provide an initial value, but `SetCategory` does not advance it because
WAL integration is deferred.

## Persistence integration

The integration test creates a `FileKind::FSM` PageFile, allocates FSM file page 1 through the
existing append-first allocator, initializes it with three entries, writes categories 0, 127, and
255, persists and syncs it through DiskManager, reopens the PageFile, reads page 1, validates the
complete structure, and retrieves the exact categories.

`FsmPage` itself never calls PageFile or DiskManager and does not own a second page buffer.

## Tests/checks run

- Focused Clang FSM/category/header/mapping/persistence suite: 13/13 passed.
- Full `clang-debug` CTest suite: 171/171 passed.
- Full `clang-asan` ASan+UBSan CTest suite: 171/171 passed with
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer was disabled for the ptrace execution environment;
  AddressSanitizer and UndefinedBehaviorSanitizer remained enabled.
- Focused `gcc-debug` FSM/category/header/mapping/persistence suite: 13/13 passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.
- No benchmarks were run because this milestone establishes a persistent format and deterministic
  arithmetic rather than a runtime candidate-search optimization.

## Assumptions

- `free_bytes` supplied to the category converter is the validated current contiguous heap-page gap
  `upper - lower`, not total reclaimable space hidden in dead tuples.
- The current insertion path allocates a new eight-byte slot for every insert. Always deducting that
  cost remains safe and conservative if reusable slots are introduced later.
- `PageNo{0}` is a file superblock page and `INVALID_PAGE_NO` is not an ordinary heap or FSM data
  page.
- A future relation-wide FSM owner will know the heap's allocated page count and supply the correct
  prefix `entry_count`; `FsmPage` performs no cross-file lookup.
- FSM hints may be stale. No result from this page format replaces validation of actual HeapPage
  free-space geometry before insertion.
- `ARCHITECTURE.md` remains authoritative; this devlog is an append-only factual record.

## Known limitations/deferred work

- There is no HeapFile or relation-wide FreeSpaceMap owner, candidate search, in-memory bucket
  accelerator, or search across FSM pages.
- There is no operation yet to extend an existing page's initialized `entry_count` prefix when the
  heap grows; this milestone only initializes pages and reads/updates existing entries.
- FSM entries are not automatically updated after HeapPage insert, mark-dead, or compact operations.
- No stale-entry retry, repair, rebuild-by-heap-scan, or startup maintenance behavior exists.
- No concurrent maintenance, latches, BufferPool integration, dirty tracking, page guards, CLOCK,
  WAL/page-LSN advancement, checksummed FSM data pages, or recovery exists.
- The flat format intentionally has no hierarchy and no overflow metadata.

## Architecture questions discovered

The architecture locks a separate approximate advisory FSM, one conceptual category per heap page,
and 0–255 as the intended category range, but it does not yet lock these exact v1 conventions:

1. Should FSM category input be the contiguous `upper - lower` gap converted to conservative tuple
   capacity by always deducting one eight-byte append slot and clamping to the strict 8135-byte raw
   tuple limit?
2. Should category conversion be locked to
   `floor(usable_insertion_bytes * 255 / 8135)`, with free-byte inputs above 8144 clamped?
3. Should the category inverse be locked as the inclusive lower bound
   `ceil(category * 8135 / 255)` rather than an upper bound or representative midpoint?
4. Should `FSM_DATA` format version 1 use a 48-byte total header: the common 32 bytes followed by
   `uint64 first_heap_page_no`, `uint16 entry_count`, `uint16 reserved16`, and `uint32 reserved32`?
5. Should this header leave exactly 8144 one-byte entries per FSM page and map heap page `H` through
   `(H - 1) / 8144` and `(H - 1) % 8144`, with FSM file page 0 reserved for the superblock?
6. Should `entry_count` mean a contiguous initialized prefix corresponding only to currently
   represented heap pages, with all bytes at indices `>= entry_count` required to remain zero?
7. Should blank pre-WAL FSM pages persist checksum 0, default to `INVALID_LSN`, and leave page-LSN
   advancement/checksum generation to the later WAL/recovery integration?

These choices should be synchronized into `ARCHITECTURE.md` before persistent-format compatibility
depends on them.
