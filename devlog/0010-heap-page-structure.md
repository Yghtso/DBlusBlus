# 0010 — Heap Page Structure

Date: 2026-08-14

## Milestone/task

Phase 1: persisted heap-data-page header, slot-directory entry format, blank-page initialization,
and structural validation.

## Scope

Implemented explicit little-endian codecs for the locked 16-byte heap-specific header and 8-byte
slot entry. Added a lightweight `HeapPage` controller over an existing raw `Page` for deterministic
blank `HEAP_DATA` initialization, metadata decoding, and structural validation.

No tuple header or tuple codec, insertion, retrieval, deletion, slot reuse, compaction, MVCC,
HeapFile, FSM, BufferPool, WAL, or recovery behavior was added.

## Files changed

- `src/storage/heap_page.h` — defines persisted constants, semantic header and slot types, codec
  APIs, validation results, and the lightweight `HeapPage` controller.
- `src/storage/heap_page.cpp` — implements explicit codecs, blank initialization, and structural
  validation.
- `tests/heap_page_test.cpp` — adds focused codec, initialization, corruption, geometry, and
  PageFile/DiskManager integration tests.
- `src/CMakeLists.txt` — registers the production source and public header.
- `tests/CMakeLists.txt` — registers the focused heap-page tests.
- `devlog/0010-heap-page-structure.md` — records this task.

No older devlog entry was modified.

## Architecture sections used

- §9 — explicit little-endian persisted serialization
- §11 — slotted heap-page layout and stable RID intent
- §44 — explicit I/O/corruption/invariant error distinction
- §49 — Phase 1 raw-storage implementation order
- §54–§56 — `SlotId`, `PageId`, and stable physical RID semantics
- §61 — common 32-byte page header
- §62 — persisted `PageType::HEAP_DATA` code and expected-type validation
- §63 — staged page-checksum policy
- §64–§65 — heap file page range and physical page/slot order
- §66 — locked heap-specific header layout
- §67 — locked 8-byte slot-directory entry layout and named states
- §68 — heap-page free-space geometry
- §69 — v1 inline-tuple boundary
- §96–§97 — HeapPage responsibility and lightweight caller-owned page view

`ARCHITECTURE.md` is the authoritative architecture file used for this task, as specified by
`AGENTS.md`.

## Public API introduced

Declared in `storage/heap_page.h`:

```cpp
inline constexpr std::uint16_t HEAP_PAGE_FORMAT_VERSION = 1;
inline constexpr std::size_t HEAP_PAGE_HEADER_OFFSET = 32;
inline constexpr std::size_t HEAP_PAGE_HEADER_ENCODED_SIZE = 16;
inline constexpr std::uint16_t HEAP_PAGE_TOTAL_HEADER_SIZE = 48;
inline constexpr std::size_t HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE = 8;
inline constexpr std::size_t HEAP_PAGE_SLOT_DIRECTORY_OFFSET = 48;

inline constexpr std::size_t HEAP_PAGE_SLOT_COUNT_OFFSET = 32;
inline constexpr std::size_t HEAP_PAGE_FREE_SLOT_HEAD_OFFSET = 34;
inline constexpr std::size_t HEAP_PAGE_LOWER_OFFSET = 36;
inline constexpr std::size_t HEAP_PAGE_UPPER_OFFSET = 38;
inline constexpr std::size_t HEAP_PAGE_PRUNE_HINT_OFFSET = 40;
inline constexpr std::size_t HEAP_PAGE_RESERVED_OFFSET = 44;

inline constexpr std::size_t HEAP_SLOT_TUPLE_OFFSET_OFFSET = 0;
inline constexpr std::size_t HEAP_SLOT_TUPLE_LENGTH_OFFSET = 2;
inline constexpr std::size_t HEAP_SLOT_FLAGS_OFFSET = 4;
inline constexpr std::size_t HEAP_SLOT_AUX_OFFSET = 6;

enum class HeapSlotState : std::uint16_t {
    UNUSED = 0,
    NORMAL = 1,
    DEAD = 2,
    REDIRECT_RESERVED = 3,
};

struct HeapPageHeader {
    std::uint16_t slot_count;
    SlotId free_slot_head;
    std::uint16_t lower;
    std::uint16_t upper;
    std::uint32_t prune_hint;
    std::uint32_t reserved;
};

struct HeapSlotEntry {
    std::uint16_t tuple_offset;
    std::uint16_t tuple_length;
    HeapSlotState state;
    std::uint16_t aux;
};

enum class HeapSlotEntryDecodeError : std::uint8_t;

struct HeapSlotEntryDecodeResult {
    std::optional<HeapSlotEntry> entry;
    HeapSlotEntryDecodeError error;
};

bool EncodeHeapPageHeader(std::span<std::byte> destination,
                          const HeapPageHeader& header) noexcept;
std::optional<HeapPageHeader>
DecodeHeapPageHeader(std::span<const std::byte> source) noexcept;

bool EncodeHeapSlotEntry(std::span<std::byte> destination,
                         const HeapSlotEntry& entry) noexcept;
HeapSlotEntryDecodeResult
DecodeHeapSlotEntry(std::span<const std::byte> source) noexcept;

enum class HeapPageValidationError : std::uint8_t;

struct HeapPageValidationResult {
    std::optional<CommonPageHeader> common_header;
    std::optional<HeapPageHeader> heap_header;
    HeapPageValidationError error;
    SlotId slot_id;
    explicit operator bool() const noexcept;
};

class HeapPage {
public:
    explicit HeapPage(Page& page) noexcept;

    bool Initialize(std::uint32_t flags = 0,
                    Lsn page_lsn = INVALID_LSN) noexcept;
    std::optional<HeapPageHeader> Header() const noexcept;
    HeapPageValidationResult Validate() const noexcept;
};
```

`HeapSlotEntryDecodeError` distinguishes an undersized source from an invalid persisted state.
`HeapPageValidationError` distinguishes common-header identity/format failures, reserved-field and
geometry failures, invalid slot states, and invalid `NORMAL` tuple ranges. Validation reports the
failing `SlotId` for slot-specific failures.

The header codec reads/writes a 16-byte span whose byte zero corresponds to page offset 32. The
slot codec reads/writes one 8-byte slot span. `HeapPage` places those spans at the published absolute
page offsets.

## Exact persisted heap-header byte layout

All values are little-endian:

```text
page offset  size  field
-----------  ----  ----------------
32           2     slot_count
34           2     free_slot_head
36           2     lower
38           2     upper
40           4     prune_hint
44           4     reserved
------------------------------------
heap-specific header = 16 bytes
total header         = 48 bytes
```

The common page header remains at offsets `0..31`; the first slot entry begins at page offset 48.
No C++ object layout or padding is persisted.

## Exact slot-entry byte layout

All values are little-endian:

```text
slot offset  size  field
-----------  ----  ------------
0            2     tuple_offset
2            2     tuple_length
4            2     slot_flags/state
6            2     aux
---------------------------------
total              8 bytes
```

Slot entries are encoded individually. No slot-directory container or allocation/reuse behavior
was introduced.

## Heap-page format-version value chosen

`HEAP_PAGE_FORMAT_VERSION` is explicitly `1` and is persisted in the common header's 16-bit
`format_version` field. The current architecture names per-page format versioning but does not yet
assign a numeric HEAP_DATA version; this is a new persisted-format choice requiring architecture
synchronization.

## Slot-state numeric values chosen

The persisted 16-bit values are:

```text
0 = UNUSED
1 = NORMAL
2 = DEAD
3 = REDIRECT_RESERVED
```

The architecture names these states but does not assign numeric codes. These explicit codes are a
new persisted-format choice requiring architecture synchronization. Encoding rejects an enum value
outside this set before modifying the destination. Decoding reports `INVALID_SLOT_STATE` for any
other persisted value.

## free_slot_head empty-page convention

A blank page stores:

```text
free_slot_head = INVALID_SLOT_ID = UINT16_MAX = 0xFFFF
```

The architecture reserves `free_slot_head` but does not lock its empty-list sentinel. Reusing the
already locked invalid SlotId sentinel is a new persisted-format convention requiring architecture
synchronization. Structural validation requires this value when `slot_count == 0`.

For nonempty pages, this milestone does not validate or interpret free-list linkage because slot
reuse and its link representation remain deferred.

## Blank-page initialization state

`HeapPage::Initialize` first asks the raw `Page` to zero all 8192 bytes and explicitly encode:

```text
common.page_type       = HEAP_DATA
common.format_version  = 1
common.flags           = caller value, default 0
common.page_lsn        = caller value, default INVALID_LSN
common.checksum_crc32c = 0
common.header_size     = 48
common.reserved16      = 0
common.page_no         = Page::Id().page_no

heap.slot_count        = 0
heap.free_slot_head    = INVALID_SLOT_ID
heap.lower             = 48
heap.upper             = PAGE_SIZE (8192)
heap.prune_hint        = 0
heap.reserved          = 0
```

Every byte after offset 47 remains zero. Initialization does not invent a slot, tuple, or
page-type-specific payload beyond the heap structural header.

## Structural validation rules

Validation rejects:

- a common header that cannot decode;
- `page_type` other than `HEAP_DATA`;
- persisted `page_no` different from `Page::Id().page_no`;
- `header_size` other than 48;
- `format_version` other than 1;
- nonzero common `reserved16`;
- a heap-specific header that cannot decode;
- nonzero heap `reserved`;
- `lower < 48`;
- `upper > PAGE_SIZE`;
- `lower > upper`;
- slot-count multiplication/directory geometry beyond the page;
- `lower != 48 + slot_count * 8`;
- a blank page whose `free_slot_head` is not `INVALID_SLOT_ID`;
- any slot entry with a persisted state outside `0..3`;
- a `NORMAL` slot whose tuple starts before `upper`, starts after the page, or whose
  `tuple_offset + tuple_length` would exceed `PAGE_SIZE`.

Slot-directory arithmetic is performed in `std::size_t` and bounded before entries are read.
Together with exact `lower`, `lower <= upper`, and `upper <= PAGE_SIZE`, this proves every declared
slot entry lies fully within the page and before tuple data.

No range or `aux` semantics are inferred for `UNUSED`, `DEAD`, or `REDIRECT_RESERVED`. Tuple-byte
contents, tuple headers, overlap between NORMAL tuple ranges, MVCC visibility, and free-list links
are not validated in this milestone.

The implementation initializes and requires both reserved fields to be zero. Assigning future
meaning to either field requires coordinated format-version handling.

## Checksum behavior

Blank heap-page initialization stores `checksum_crc32c = 0`. This milestone neither computes nor
verifies a whole-page heap checksum, consistent with the pre-recovery checksum staging. It does not
change the existing CRC32C primitive or superblock checksum behavior.

## Page/PageFile/DiskManager integration

`HeapPage` stores only a non-owning pointer to a caller-owned `Page`; it owns no second page buffer,
performs no heap allocation, and has no frame, latch, dirty, pin, or replacement state.

The integration test creates a HEAP `PageFile`, allocates page 1 through append-first allocation,
constructs a raw `Page` for the returned `PageId`, initializes it through `HeapPage`, writes and
reads the exact page span through `DiskManager`, and validates the read-back structure. `HeapPage`
does not call DiskManager or PageFile directly.

## Tests/checks run

- Focused Clang heap-page format tests: 17/17 passed.
- Full `clang-debug` build and CTest suite: 90/90 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 90/90 passed with
  `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot run under the environment's ptrace
  setup.
- GCC focused heap-page format tests: 17/17 passed.
- `clang-tidy` preset build for the new production and test code: passed without warnings after
  adding explicit arithmetic grouping and analyzer-visible optional guards from the initial run.
- `clang-format --dry-run --Werror` on `src/storage/heap_page.h`,
  `src/storage/heap_page.cpp`, and `tests/heap_page_test.cpp`: passed.
- `git diff --check`: passed.

Tests cover exact little-endian bytes, all offsets and encoded sizes, integer boundaries, unaligned
spans, undersized atomic encode behavior, all four slot codes, invalid state encode/decode,
deterministic blank bytes, explicit flags/LSN, valid NORMAL geometry, every requested common and
heap-header corruption, excessive slot-count geometry, empty-list sentinel, invalid slot state,
tuple-data-region bounds, overflow-style tuple ranges, conservative reserved-state handling, and a
real PageFile/DiskManager round trip.

## Assumptions

- The caller-owned `Page` outlives its lightweight `HeapPage` controller.
- A zero-length `NORMAL` tuple range is not rejected structurally if its offset lies in the tuple
  region; tuple-format validation is deferred to the tuple milestone.
- Nonempty `free_slot_head` values and free-list linkage are intentionally uninterpreted until
  slot reuse is implemented.
- The maximum inline tuple policy is not exposed as a separate API yet because insertion and the
  tuple header/codec are outside this milestone. Structural validation only requires persisted
  NORMAL ranges to fit in the current page.

## Known limitations/deferred work

- No tuple insertion, retrieval, deletion, DEAD transition, slot reuse, free-list maintenance,
  compaction, or overlap detection is implemented.
- No tuple header, schema-directed tuple representation, maximum-row error, MVCC visibility, or
  pruning logic is implemented.
- `prune_hint`, `aux`, `DEAD`, and `REDIRECT_RESERVED` are represented but have no operational
  behavior.
- No generic heap-page checksum generation/verification, BufferPool integration, page guards,
  latches, dirty tracking, WAL, recovery, FSM, or HeapFile exists yet.

## Architecture questions discovered

The following persisted v1 choices were required but are not currently assigned by
`ARCHITECTURE.md` and should be accepted and synchronized into the architecture contract or revised
before persistent compatibility is treated as locked:

1. HEAP_DATA format version is `1`.
2. Slot-state codes are `UNUSED=0`, `NORMAL=1`, `DEAD=2`, and `REDIRECT_RESERVED=3`.
3. Empty `free_slot_head` is `INVALID_SLOT_ID` (`UINT16_MAX`, encoded as `0xFFFF`).
4. v1 structural decoding requires common `reserved16` and heap-specific `reserved` to be zero.

No other persistent byte-layout choice was introduced; the 48-byte total header and 8-byte slot
entry offsets and widths were already locked.
