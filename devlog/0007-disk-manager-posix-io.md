# 0007 — DiskManager POSIX I/O

Date: 2026-08-13

## Milestone/task

Phase 1: low-level POSIX DiskManager and random-access page-file I/O.

## Scope

Implemented a deliberately storage-agnostic `DiskManager` for Linux page files. It owns open POSIX
file descriptors, maps persistent `FileId` values to process-local file resources, creates and
opens files, discovers aligned file sizes/page counts, performs fixed-size positional page I/O,
extends files by one page, closes files, and durably synchronizes files.

No superblock interpretation, file-kind behavior, directory management, buffer management, WAL,
recovery, tuple/page internals, asynchronous I/O, `mmap`, `O_DIRECT`, or background work was added.

## Files changed

- `src/storage/disk_manager.h` — defines the disk-specific error/result types and public
  `DiskManager` API.
- `src/storage/disk_manager.cpp` — implements RAII descriptor ownership and POSIX page-file
  operations.
- `tests/disk_manager_test.cpp` — adds focused temporary-file tests.
- `src/CMakeLists.txt` — registers the new storage source and public header.
- `tests/CMakeLists.txt` — registers the focused test source.
- `devlog/0007-disk-manager-posix-io.md` — records this task.

The pre-existing unrelated formatting change in `devlog/0006-file-superblock.md` was preserved and
not modified by this task.

## Architecture sections used

- §4 — Linux-first platform
- §6 — Persistent Storage Model and `PAGE_SIZE`
- §8 — explicit I/O instead of `mmap`
- §44 — Error Handling
- §54 — Fundamental Identifier Types and FileId semantics
- §55 — PageId and physical byte addressing
- §58 — File Kinds and page-zero reservation boundary
- §59 — File Superblock boundary
- §60 — append-first page allocation
- §86 — DiskManager Responsibilities
- §87 — File I/O Semantics

`ARCHITECTURE.md` is the authoritative architecture file used for this task.

## Public API introduced

Declared in `storage/disk_manager.h`:

```cpp
enum class DiskOperation : std::uint8_t {
    CREATE_FILE,
    OPEN_FILE,
    CLOSE_FILE,
    LOOKUP_FILE,
    FILE_SIZE,
    READ_PAGE,
    WRITE_PAGE,
    EXTEND_FILE,
    SYNC_FILE,
};

enum class DiskErrorCode : std::uint8_t {
    NONE,
    INVALID_FILE_IDENTIFIER,
    DUPLICATE_FILE_ID,
    FILE_NOT_REGISTERED,
    FILE_SIZE_NOT_PAGE_ALIGNED,
    OFFSET_OVERFLOW,
    PAGE_NOT_FOUND,
    SHORT_READ,
    SYSTEM_ERROR,
};

struct DiskError {
    DiskErrorCode code;
    DiskOperation operation;
    FileId file_id;
    PageNo page_no;
    std::filesystem::path path;
    int system_error;
    std::string message;
};

struct DiskStatus;

template <typename Value>
struct DiskResult;

class DiskManager {
public:
    DiskStatus CreateFile(FileId file_id, const std::filesystem::path& path);
    DiskStatus OpenFile(FileId file_id, const std::filesystem::path& path);
    DiskStatus CloseFile(FileId file_id);

    DiskResult<std::uint64_t> FileSize(FileId file_id) const;
    DiskResult<PageNo> PageCount(FileId file_id) const;

    DiskStatus ReadPage(PageId page_id,
                        std::span<std::byte, PAGE_SIZE> output) const;
    DiskStatus WritePage(PageId page_id,
                         std::span<const std::byte, PAGE_SIZE> data) const;

    DiskResult<PageNo> ExtendFile(FileId file_id);
    DiskStatus SyncFile(FileId file_id) const;
};
```

`DiskStatus` and `DiskResult<Value>` expose an explicit success flag, contextual `DiskError`, and,
for value operations, the returned value. They are local to the disk layer rather than a generic
project-wide result framework.

## File-descriptor ownership and lifetime

Each registered file is represented by a private move-only `FileEntry` containing the descriptor
and path. `FileEntry` closes its descriptor in its destructor. `DiskManager` is non-copyable and
non-movable, so descriptor-map ownership remains stable. `CloseFile` removes and closes one entry;
destruction closes all entries still registered. No descriptor appears in the public API or in a
`FileId`/`PageId`.

Registration/open/close are lifecycle operations and must not race active I/O. Normal `pread` and
`pwrite` operations do not acquire a manager-wide mutex. Append extension is serialized by one
mutex because page-count discovery plus `ftruncate` is a shared metadata operation.

## FileId registration semantics

- `INVALID_FILE_ID` cannot be registered.
- A `FileId` may have at most one live registry entry.
- Registering an already registered `FileId` returns `DUPLICATE_FILE_ID` before opening/creating a
  second path.
- `CreateFile` uses `O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC` with requested mode `0660`; it never
  truncates an existing path.
- `OpenFile` uses `O_RDWR | O_CLOEXEC`, opens only an existing path, and rejects a size not aligned
  to `PAGE_SIZE`.
- New files remain empty. Reserving/encoding page zero as a superblock belongs to higher-level
  creation logic.
- The registry enforces FileId uniqueness. It does not inspect superblocks or attempt to detect
  multiple path aliases that refer to the same inode.

## Read/write semantics

- Each operation accepts an exact static-extent `PAGE_SIZE` span.
- Physical offset is `page_no * PAGE_SIZE` after full-page overflow validation.
- `ReadPage` uses `pread` and requests remaining bytes until exactly one page is read.
- Reads stage into a local page array and copy to the caller only after success. EOF or error never
  returns a partly initialized page.
- EOF before any byte returns `PAGE_NOT_FOUND`; EOF after some bytes returns `SHORT_READ`.
- `WritePage` uses `pwrite` until exactly one page is written.
- `WritePage` first verifies that the target page already exists. It does not allocate sparse or
  beyond-EOF pages; callers must use `ExtendFile`.
- Positional I/O is independent from a process-shared seek offset.

## Short-I/O and EINTR behavior

`pread` and `pwrite` maintain a transferred-byte count and continue at the next positional offset
after a positive short transfer. Both retry unchanged after `EINTR`. A zero-byte `pwrite` is
reported as `SYSTEM_ERROR` with `EIO` because no progress is possible.

`open`, `fstat`, `ftruncate`, and `fdatasync` retry on `EINTR`. `close` is not retried because
retrying `close` after `EINTR` can target a reused descriptor on Linux; explicit close reports its
errno, while destructor cleanup cannot report errors.

No syscall-mocking abstraction was introduced. Tests force a real short read by externally
truncating an already-open file to a partial-page boundary. Forced short-write and forced-EINTR
paths were not injected.

## Page-offset overflow handling

Before positional I/O, the implementation verifies that the complete page extent, including its
last byte, fits in positive `off_t`. Overflow returns `OFFSET_OVERFLOW` before `pread` or `pwrite`.
Extension separately verifies that the new aligned file size fits in `off_t` before `ftruncate`.

## File-size and alignment validation

`FileSize` uses `fstat` and returns an error when `st_size` is negative or not an exact multiple of
`PAGE_SIZE`. `PageCount` divides only a validated aligned size. `OpenFile` performs the same
alignment check before registration. No size is rounded and no repair is attempted.

## Extension semantics

`ExtendFile` serializes extension calls, obtains the current aligned page count, computes:

```text
new_page_no = current_page_count
new_file_size = (current_page_count + 1) * PAGE_SIZE
```

It then uses `ftruncate` to extend by exactly one zero-filled logical page and returns
`new_page_no`. It does not choose page types, initialize superblocks, or reuse free pages.

## Sync primitive

`SyncFile` uses `fdatasync`, retrying on `EINTR`, to request durable synchronization of page-file
data and size metadata needed for subsequent reads.

## Error model

Disk failures return `DiskStatus` or `DiskResult<Value>` and do not abort or throw for ordinary
POSIX failures. `DiskError` records:

- disk operation,
- database `FileId`,
- `PageNo` when page-specific,
- path when a registered or requested path is available,
- raw errno in `system_error` for syscall failures,
- a short operation-specific message.

Logical errors such as unregistered IDs, misalignment, overflow, missing pages, and short reads
have distinct `DiskErrorCode` values.

## Tests and checks run

- Focused Clang DiskManager tests: 8/8 passed.
- Full `clang-debug` CTest suite: 55/55 passed.
- Full `clang-asan` CTest suite with AddressSanitizer and UndefinedBehaviorSanitizer: 55/55 passed
  with `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer is disabled because it cannot run under the
  environment's `ptrace`.
- GCC focused DiskManager tests: 8/8 passed with the full project warning set and no warnings.
- Clang-tidy ran on the implementation and focused tests: clean.
- Clang-format dry-run on changed C++ files: passed.
- `git diff --check`: passed.

The focused tests use independent temporary directories and cover create/open/close, initial size
and page count, duplicate FileId behavior, non-truncating creation, exact repeated extension,
physical size growth, two-file mapping, full-page round trips, independent pages and out-of-order
positional operations, missing reads/writes, unchanged failed-read destinations, misaligned sizes,
real mid-page short reads, unregistered IDs, offset overflow, reopening, `fdatasync`, invalid IDs,
missing paths, errno, path, operation, FileId, and PageNo context.

## Assumptions

- The locked Linux x86-64/ARM64 environments provide 64-bit `off_t` and the specified POSIX APIs.
- File creation and page-zero superblock initialization remain separate operations; a newly
  created file has page count zero until explicitly extended.
- Page writes target already allocated pages; allocation occurs only through `ExtendFile`.
- Registration/open/close do not race active I/O. Concurrent positional reads/writes are allowed,
  and extensions are serialized.
- Requested creation mode `0660` remains subject to the process umask.

## Known limitations and deferred work

- `DiskManager` does not validate superblock contents or FileId/path identity on open.
- Exact path aliases or hard links can be registered under different FileIds; later superblock
  validation is expected to detect persistent identity mismatches.
- Explicit close errors can be returned; destructor-time close errors cannot be reported.
- Directory creation, atomic file-initialization protocols, permissions policy, read-only open,
  file deletion, file-kind-specific handling, free-page reuse, and multi-process coordination are
  deferred.
- Forced short-write/EINTR tests and a syscall test seam are deferred until evidence justifies the
  abstraction.
- BufferPool, page guards, dirty-page policy, WAL ordering, asynchronous I/O, direct I/O, and
  background flushing are deferred.

## Architecture questions discovered

No persistent-format decision was introduced. The following operational policies are implemented
but are not currently detailed in the locked architecture text:

- newly created files start at zero bytes and higher-level creation allocates/encodes page zero;
- `WritePage` rejects unallocated pages rather than extending implicitly;
- append extension is serialized while file registration/close remains externally synchronized;
- creation requests mode `0660`, subject to umask;
- `fdatasync` is selected from the architecture's allowed `fdatasync/fsync` choices.

These may be locked later if the project wants a stricter file-lifecycle contract; none changes a
persistent byte format.
