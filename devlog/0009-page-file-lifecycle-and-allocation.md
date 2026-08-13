# 0009 — Page-File Lifecycle and Allocation

Date: 2026-08-14

## Milestone/task

Phase 1: page-file lifecycle, superblock validation, and append-first ordinary-page allocation.

## Scope

Implemented one higher-level `PageFile` abstraction above `DiskManager`. It creates and registers
new random-access database files, initializes and durably synchronizes page zero with the existing
`FileSuperblock` codec, reopens files only after validating their persistent superblock and caller
expectations, and allocates ordinary pages with the locked append-first policy.

The implementation uses the existing raw `Page` abstraction for page-zero I/O and returns a
`PageId` for each ordinary allocation. It does not interpret or initialize heap, FSM, B+ tree, or
catalog data pages. No BufferPool, frame, free-page reuse, WAL, or recovery behavior was added.

## Files changed

- `src/storage/page_file.h` — defines the move-only `PageFile` handle, scoped result types, and
  page-file error model.
- `src/storage/page_file.cpp` — implements create/open validation, RAII registration cleanup, and
  append-first allocation.
- `tests/page_file_test.cpp` — adds focused temporary-file lifecycle, identity, corruption,
  cleanup, allocation, and multi-file tests.
- `src/CMakeLists.txt` — registers the new production source and public header.
- `tests/CMakeLists.txt` — registers the focused PageFile tests.
- `devlog/0009-page-file-lifecycle-and-allocation.md` — records this task.

## Architecture sections used

- §6 — fixed-size persistent pages
- §44 — distinction between I/O, corruption, and invariant failures
- §49 — Phase 1 raw-storage implementation order
- §54–§55 — persistent `FileId` and `PageId` semantics
- §58 — file kinds, page-zero reservation, and data pages beginning at page one
- §59 — locked FileSuperblock representation, checksum, and validation
- §60 — append-first page allocation
- §61–§63 — common page header, page types, and checksum staging
- §86–§87 — DiskManager responsibility and positional-I/O boundary
- §88–§89 — BufferPool/frame state excluded from this layer
- §96 — storage-object responsibility boundaries

`ARCHITECTURE.md` is the authoritative architecture file used for this task.

## Public API introduced

Declared in `storage/page_file.h`:

```cpp
enum class PageFileOperation : std::uint8_t {
    CREATE,
    OPEN,
    ALLOCATE_PAGE,
};

enum class PageFileErrorCode : std::uint8_t {
    NONE,
    DISK_ERROR,
    SUPERBLOCK_ENCODING_FAILED,
    SUPERBLOCK_INVALID,
    FILE_ID_MISMATCH,
    FILE_KIND_MISMATCH,
    OBJECT_ID_MISMATCH,
    UNEXPECTED_INITIAL_PAGE_COUNT,
    MISSING_SUPERBLOCK,
    UNEXPECTED_SUPERBLOCK_PAGE_NUMBER,
    DATA_PAGE_ZERO_ALLOCATED,
    NOT_OPEN,
};

struct PageFileError {
    PageFileErrorCode code;
    PageFileOperation operation;
    DiskError disk_error;
    FileSuperblockDecodeError superblock_error;
    FileId expected_file_id;
    FileId actual_file_id;
    FileKind expected_file_kind;
    FileKind actual_file_kind;
    std::uint64_t expected_object_id;
    std::uint64_t actual_object_id;
    PageNo actual_page_count;
    PageNo actual_page_no;
};

class PageFile {
public:
    ~PageFile();

    PageFile(const PageFile&) = delete;
    PageFile& operator=(const PageFile&) = delete;
    PageFile(PageFile&& other) noexcept;
    PageFile& operator=(PageFile&& other) noexcept;

    static PageFileResult Create(DiskManager& disk_manager,
                                 const std::filesystem::path& path,
                                 const FileSuperblock& superblock);

    static PageFileResult Open(
        DiskManager& disk_manager,
        const std::filesystem::path& path,
        FileId expected_file_id,
        FileKind expected_file_kind,
        std::optional<std::uint64_t> expected_object_id = std::nullopt);

    const FileSuperblock& Superblock() const noexcept;
    PageAllocationResult AllocatePage();
};

struct PageFileResult {
    std::optional<PageFile> page_file;
    PageFileError error;
    explicit operator bool() const noexcept;
};

struct PageAllocationResult {
    std::optional<PageId> page_id;
    PageFileError error;
    explicit operator bool() const noexcept;
};
```

These result types are specific to this layer. No project-wide generic result framework was
introduced.

## Abstraction/name chosen and why

The abstraction is named `PageFile` because one instance represents one validated, currently
registered page-based database file. It combines only the lifecycle and allocation semantics that
must sit above the deliberately dumb DiskManager. A separate PageFileManager and PageAllocator
would duplicate ownership and identity state without serving a second current use case.

`PageFile` is move-only and holds a non-owning pointer to the caller-owned `DiskManager` plus the
validated semantic `FileSuperblock`. It owns the successful DiskManager registration logically:
destruction or move replacement closes/unregisters that FileId. POSIX descriptors remain private
to DiskManager.

## Creation sequence

`PageFile::Create` performs:

1. `DiskManager::CreateFile` using the superblock's persistent FileId.
2. `PageCount` verification that the new file has zero pages.
3. one `ExtendFile` call.
4. verification that the returned page number is exactly zero.
5. construction of a raw `Page` associated with `(file_id, 0)`.
6. `EncodeFileSuperblock` into that page's exact byte span.
7. `DiskManager::WritePage` for page zero.
8. `DiskManager::SyncFile`, which uses `fdatasync`.
9. construction of the successful `PageFile` handle only after synchronization succeeds.

The existing codec remains solely responsible for superblock magic, format fields, explicit byte
layout, reserved bytes, common-header invariants, and CRC32C generation.

## Open/validation sequence

`PageFile::Open` performs:

1. `DiskManager::OpenFile` under the expected persistent FileId.
2. page-count discovery and rejection of a zero-page file.
3. a fixed-size page-zero read into a raw `Page`.
4. `DecodeFileSuperblock`, including checksum and all locked format validation.
5. explicit persisted FileId comparison.
6. explicit FileKind comparison.
7. object-ID comparison when the caller supplies an expected object ID.
8. construction of a successful `PageFile` containing the decoded persistent superblock.

DiskManager continues to reject misaligned files before registration. PageFile reports that as a
`DISK_ERROR` while retaining the complete `DiskError` context.

## Persistent identity validation

The decoded superblock is authoritative. Open reports distinct errors for FileId, FileKind, and
optional object-ID mismatches. Mismatch errors retain expected and actual values. When no expected
object ID is supplied, object identity is not compared, but the decoded value remains available
through `Superblock()`.

No inode, hard-link, or path-alias identity checks were added.

## Page-zero handling

Creation allocates page zero exactly once and writes only the encoded superblock there. Ordinary
allocation rejects a returned page number of zero as `DATA_PAGE_ZERO_ALLOCATED`; page zero is never
reported to the caller as a data-page allocation.

The focused tests externally truncate a valid open file to zero bytes before allocation to verify
this defensive check. That deliberately corrupted file is extended to one zero page by the lower
append operation, but allocation still reports failure rather than returning page zero.

## Ordinary allocation semantics

`AllocatePage` calls `DiskManager::ExtendFile(superblock.file_id)` once and returns:

```text
PageId {
    file_id = persistent superblock FileId,
    page_no = previous aligned file page count
}
```

For a valid initialized file, allocations therefore return page numbers `1, 2, 3, ...`, and each
call grows the file by exactly `PAGE_SIZE`. There is no free-page reuse, sparse allocation, extent
allocation, page deletion, page-type selection, or additional allocator mutex. DiskManager's
existing extension serialization remains the sole concurrency mechanism.

## Returned allocation representation

Allocation returns `PageId`, not an 8 KiB `Page`. This is the smallest representation needed to
identify the newly allocated storage and avoids an unnecessary whole-page value transfer. A caller
can construct `Page{page_id}` when it needs an owning zeroed in-memory buffer and then initialize
the appropriate page-specific header.

The newly extended physical page reads as all zero bytes due to the current POSIX extension path,
but the generic allocator does not encode a common header or claim any `PageType` for it.

## Durability/sync behavior

Successful creation calls `DiskManager::SyncFile` after writing the valid checksummed superblock
and reports success only when `fdatasync` succeeds. Ordinary allocations do not synchronize after
every extension. No WAL ordering or new durability policy was introduced.

## Failure cleanup behavior

If create/open registration succeeds and a later operation fails, PageFile calls
`DiskManager::CloseFile` to remove the registry entry. Cleanup failure does not replace the primary
page-file, validation, or I/O error.

Creation never unlinks or rolls back the OS file. A post-creation failure may leave a zero-length,
one-page, partially written, fully written but unsynchronized, or otherwise incomplete path,
depending on the failed step. The invalid-FileKind test establishes the current concrete case of a
one-page zero-filled file remaining after superblock encoding fails. Such a path must not be
treated as successfully created.

Failed open validation leaves the physical file unchanged and unregisters the expected FileId.
Successful `PageFile` destruction also unregisters its FileId through RAII.

## Error model

`PageFileErrorCode` distinguishes lower disk failure, superblock encoding/validation failure,
persistent identity mismatches, missing page zero, creation/allocation invariants, and use of a
moved-from/not-open handle. `PageFileError` embeds the original `DiskError` or
`FileSuperblockDecodeError` and includes expected/actual identity and observed page values where
relevant.

Errors use explicit return values. Ordinary file, validation, and corruption failures do not abort
or use exceptions as control flow.

## Tests/checks run

- Focused Clang PageFile tests: 11/11 passed.
- Full `clang-debug` build and CTest suite: 73/73 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 73/73 passed with
  `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot run under the environment's ptrace
  setup.
- GCC focused PageFile tests: 11/11 passed.
- `clang-tidy` preset build for the new production and test code: passed without warnings after
  correcting three analyzer-visible optional guards in the initial run.
- `clang-format --dry-run --Werror` on `src/storage/page_file.h`,
  `src/storage/page_file.cpp`, and `tests/page_file_test.cpp`: passed.
- `git diff --check`: passed.

The focused suite covers a valid HEAP creation and persisted superblock fields, exact one-page
initial size, common page-zero invariants, checksum validation, creation epoch, close/reopen,
preserved page count after allocations, FileId/FileKind/object-ID mismatch context, optional
object checking, corrupt checksum, zero-page files, failed-open cleanup, misalignment propagation,
allocations `1..3`, exact size growth, zeroed allocated storage, page-zero defense, independent
files, all four locked random-access FileKinds, duplicate registration, non-truncating existing
paths, and concrete partial-file behavior after a post-creation failure.

## Assumptions

- The caller-owned `DiskManager` outlives every `PageFile` referring to it.
- Callers do not directly close a DiskManager registration while its `PageFile` handle remains
  active.
- The existing POSIX `ftruncate` extension path supplies deterministic zero bytes for the newly
  extended region.
- Omitting `expected_object_id` means the caller intentionally does not know or check object
  identity during that open.
- A successful creation may retain the semantic input superblock in memory because the existing
  codec deterministically writes those same fields; reopen always retains the decoded persistent
  superblock.

## Known limitations/deferred work

- There is no automatic unlink, rollback, repair, or temporary-file protocol for failed creation.
- Destructor-time close errors cannot be reported; explicit DiskManager errors during create,
  open, allocation, write, and sync remain reportable.
- There is no free-page reuse, extent allocation, page deletion, file shrinking, multi-process
  allocation coordination, or file-kind-specific extension metadata.
- Ordinary allocation returns untyped storage and does not initialize a common or page-specific
  header.
- Heap pages, FSM pages, B+ tree pages, catalog page formats, BufferPool frames/guards, latching,
  dirty tracking, WAL, recovery, and transactions remain deferred.

## Architecture questions discovered

None. This task introduced no persistent-format change. The PageFile name, RAII registration
lifetime, PageId-only allocation result, and partial-file cleanup behavior are implementation-level
choices within the locked lifecycle and allocation boundaries.
