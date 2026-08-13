# 0008 — Raw Page Abstraction

Date: 2026-08-14

## Milestone/task

Phase 1: in-memory raw page abstraction between `DiskManager` and page-type-specific views.

## Scope

Implemented a value-owned representation of one database page. `Page` associates a `PageId` with
one contiguous `std::array<std::byte, PAGE_SIZE>`, exposes fixed-extent mutable and const spans,
and delegates common-header encoding and decoding to the existing explicit codec. It can zero and
initialize a page, update only the common header, and validate the persisted page type and page
number against caller expectations and the page's identity.

No buffer-frame metadata, page-type-specific structures, checksum policy, page allocation, WAL,
or recovery behavior was added. `DiskManager` was not redesigned or coupled to `Page`.

## Files changed

- `src/storage/page.h` — defines `Page`, its fixed byte storage, and focused common-header
  validation result types.
- `src/storage/page.cpp` — implements byte-span access, deterministic initialization, header codec
  delegation, and expected type/page-number validation.
- `tests/page_test.cpp` — adds focused value-layout, byte-access, header, corruption, and real
  DiskManager interoperability tests.
- `src/CMakeLists.txt` — registers the new production source and public header.
- `tests/CMakeLists.txt` — registers the focused Page tests.
- `devlog/0008-raw-page-abstraction.md` — records this task.

## Architecture sections used

- §6 — fixed 8192-byte persistent pages
- §9 — explicit on-disk serialization rather than raw C++ object persistence
- §14 — buffer-pool and frame-metadata boundary
- §49 — Phase 1 raw-storage implementation order
- §55 — `PageId` semantics and independence from frame identity
- §61 — common 32-byte page header and persisted `page_no` self-check
- §62 — locked page types and expected-type validation
- §63 — staged checksum policy
- §86 — dumb `DiskManager` responsibility boundary
- §88–§89 — BufferPool and buffer-frame responsibilities
- §96–§97 — storage object boundaries and lightweight page-type views over page bytes

`ARCHITECTURE.md` is the authoritative architecture file used for this task.

## Public API introduced

Declared in `storage/page.h`:

```cpp
enum class PageHeaderValidationError : std::uint8_t {
    NONE,
    DECODE_FAILED,
    UNEXPECTED_PAGE_TYPE,
    UNEXPECTED_PAGE_NUMBER,
};

struct PageHeaderValidationResult {
    std::optional<CommonPageHeader> header;
    PageHeaderValidationError error;
    explicit operator bool() const noexcept;
};

class Page {
public:
    using ByteStorage = std::array<std::byte, PAGE_SIZE>;

    Page();
    explicit Page(PageId page_id) noexcept;

    PageId Id() const noexcept;
    std::span<std::byte, PAGE_SIZE> Bytes() noexcept;
    std::span<const std::byte, PAGE_SIZE> Bytes() const noexcept;

    bool Initialize(const CommonPageHeader& header) noexcept;
    std::optional<CommonPageHeader> DecodeHeader() const noexcept;
    bool WriteHeader(const CommonPageHeader& header) noexcept;
    PageHeaderValidationResult ValidateHeader(PageType expected_type) const noexcept;
};
```

`PageHeaderValidationResult` is successful only when decoding succeeds and both the expected page
type and the `Page` object's `PageNo` match the common header.

## Ownership/layout model

`Page` owns one inline `std::array<std::byte, PAGE_SIZE>`. The byte storage is contiguous, has a
compile-time size assertion, starts zero-initialized, and performs no heap allocation during normal
construction or byte/header access. The `Page` object is larger than `PAGE_SIZE` because its
`PageId` is in-memory metadata; only the span returned by `Bytes()` is page-format data.

No C++ object layout, member padding, or `PageId` metadata is serialized. All common-header access
continues through `EncodeCommonPageHeader` and `DecodeCommonPageHeader`.

## PageId choice

`Page` stores `PageId`. This keeps the owned bytes associated with the persistent logical identity
needed by `DiskManager`, and it permits direct validation of the header's persisted `page_no`
self-check. `file_id` is retained only as in-memory identity because the common page header does not
persist it. This identity is not a frame ID and introduces no BufferPool state.

## Common-header integration

`DecodeHeader()` decodes the leading bytes with the existing common-header codec. `WriteHeader()`
encodes only the common-header region and leaves page-type-specific bytes unchanged.
`ValidateHeader(expected_type)` reports a type mismatch or a mismatch between the persisted
`page_no` and `Page::Id().page_no`; it does not apply page-type-specific format-version,
`header_size`, reserved-byte, or payload validation.

## Initialization semantics

`Initialize(header)` first fills all `PAGE_SIZE` bytes with zero and then explicitly encodes the
supplied `CommonPageHeader`. The supplied fields are preserved verbatim. No heap/FSM/B+ tree,
superblock, tuple, or other page-specific region is initialized.

## Checksum behavior

The raw page layer does not calculate or verify CRC32C. Initialization and header updates encode
the caller-supplied `checksum_crc32c` field exactly. A default common header therefore retains the
existing zero checksum value, without creating a new checksum enablement policy.

## DiskManager interoperability

No DiskManager overload was added. `Page::Id()` and the exact fixed-extent span from `Page::Bytes()`
can be passed directly to the existing `DiskManager::ReadPage` and `DiskManager::WritePage` APIs.
The focused test allocates a real temporary page file, writes a `Page`, reads it into another
`Page`, compares all bytes, and validates the common header.

## Tests/checks run

- Focused Clang Page tests: 7/7 passed.
- Full `clang-debug` build and CTest suite: 62/62 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 62/62 passed with
  `ASAN_OPTIONS=detect_leaks=0`.
- GCC focused Page tests: 7/7 passed.
- `clang-tidy` preset build for the changed production and test code: passed without warnings after
  correcting the findings from the initial run.
- `clang-format --dry-run --Werror` on `src/storage/page.h`, `src/storage/page.cpp`, and
  `tests/page_test.cpp`: passed.
- `git diff --check`: passed.

The first sanitizer link-time GoogleTest discovery attempt encountered the environment's known
LeakSanitizer/ptrace incompatibility before tests ran. Leak detection was disabled for the rerun;
AddressSanitizer and UndefinedBehaviorSanitizer instrumentation remained enabled.

## Assumptions

- Associating `PageId` with the byte owner is useful at this layer because page identity is needed
  for DiskManager I/O and `page_no` self-validation.
- Common-header semantic validation at this layer is limited to expected `PageType` and `PageNo`.
  Format-specific validation remains with superblock, heap, FSM, B+ tree, and catalog codecs.
- Value copies of `Page` copy all 8192 bytes; the implemented helpers do not make implicit
  whole-page copies.

## Known limitations/deferred work

- There is no pin count, dirty state, latch, frame ID, replacement metadata, I/O state, page guard,
  or WAL flush coordination.
- There is no page cache, allocator integration, page-type hierarchy, or page-type-specific view.
- Generic page checksum generation and verification remain deferred under the architecture's
  checksum staging. Superblock checksum handling remains in the superblock codec.
- Buffer-frame placement/alignment policy remains part of the future BufferPool/frame milestone.

## Architecture questions discovered

None. This task introduced no persisted-format choice and required no architecture revision.
