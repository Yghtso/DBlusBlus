# 0030 — BufferPool: Frames, Page Guards, CLOCK, and Dirty-Page Flushing

- Date: 2026-10-21
- Milestone: Phase 2 items 8–11 (buffer frames, page guards, CLOCK replacement, dirty-page flushing)
- Authorization: explicitly approved by the project owner after review of a written plan

## Scope

Implemented the ARCHITECTURE.md Chapter 7 BufferPool mechanics that do not depend on the WAL:
frame ownership/I/O states, the load-intent page table, RAII read/write guards, validation before
residency, copied stable writeback, CLOCK replacement with victim reservation, new-page
publication, file retirement, and controlled shutdown.

Not implemented, because their owners do not exist: WAL-before-data, `rec_lsn`, DPT,
checkpoint/FPI epochs, MTR no-flush barriers, `PAGE_INIT`, and the WAL error categories. HeapFile,
relation-wide FSM integration, the Storage Milestone 1 scenario, and buffer-pool benchmarks
(Phase 2 items 12–13) were left for later milestones.

## Files changed

- `src/storage/buffer/buffer_error.h` — `BufferErrorCode`/`BufferError`/`BufferResult`/`BufferStatus`.
- `src/storage/buffer/buffer_frame.h` — `BufferFrame`, `FrameState`, `FrameIoState`.
- `src/storage/buffer/clock_replacer.{h,cpp}` — CLOCK hand and reference bits.
- `src/storage/buffer/page_guard.{h,cpp}` — `ReadPageGuard`, `WritePageGuard`.
- `src/storage/buffer/page_io.h` — `PageIo` interface and `DiskPageIo`.
- `src/storage/buffer/buffer_pool.{h,cpp}` — `BufferPool`, observer interface, diagnostics.
- `src/storage/page/page_checksum.{h,cpp}` — §4.12 page checksum compute/finalize/verify.
- `src/CMakeLists.txt`, `tests/CMakeLists.txt` — new sources and tests.
- `tests/storage/buffer/buffer_test_support.h` — fault-injecting `PageIo`, pausing observer,
  heap-file fixture, non-optional accessors.
- `tests/storage/buffer/clock_replacer_test.cpp`, `buffer_pool_test.cpp`,
  `buffer_pool_concurrency_test.cpp`, `buffer_pool_persistence_test.cpp`,
  `tests/storage/page/page_checksum_test.cpp`.
- `docs/PROJECT_STATE.md` — buffer management moved from unimplemented to implemented, with
  its limitations; checksum and HeapFile descriptions corrected.

## Architecture sections used

§4.11.1, §4.12–4.12.2, §7.1–7.13; VERIFICATION.md "Buffer management verification".

## Design decisions

1. **One pool mutex plus per-frame latches.** The mutex protects the page table, file registry,
   frame metadata, and CLOCK hand. All disk I/O, owner validation, and page initialization run
   without it. Page latches are `std::shared_mutex`. Lock order: a page latch may be taken before
   the pool mutex (a writer's `MarkDirty`), never after. Partitioning is deferred until a
   benchmark shows contention.
2. **No WAL placeholder.** No `flush_through` hook or recovery fields were added; the flush path
   documents where WAL-before-data will sit.
3. **Checksums are live.** Loads verify CRC32C before validation; flushes finalize it in the
   private image. The resident frame's checksum bytes are left stale, as §4.12.1 permits.
4. **Validation layering.** BufferPool checks checksum and common-header PageNo only. Page type,
   format version, header geometry, and family structure belong to the owner validator
   registered per file (`PageValidationOutcome::{VALID, CORRUPT, UNSUPPORTED_FORMAT}`).
   An early draft also required `header_size == 32`; that was wrong (heap pages store their
   48-byte total header size there) and was removed.
5. **Mutation publication.** `WritePageGuard::MarkDirty()` increments the generation and sets
   dirty under the pool mutex while the exclusive latch is held. In debug builds the guard keeps
   a CRC of the bytes as of the first mutable access or last `MarkDirty()` and aborts on release
   if they differ.
6. **Pins and claims.** `BufferPoolOptions::max_pin_count` bounds pins and pending load claims,
   so tests reach the overflow boundary deterministically.
7. **CLOCK attempt.** One attempt visits each frame at most twice from the hand (clear on the
   first rotation, choose on the second); no eligible frame returns `NO_REPLACEABLE_FRAME`.
8. **Victim reservation.** Without an observer, candidate selection and reservation happen in one
   critical section. With an observer the lock is released at `VICTIM_CANDIDATE_OBSERVED` and
   eligibility is rechecked, which exercises the §7.12.1 final recheck.
9. **New pages.** The frame is reserved before extending the file, and NewPage is serialized per
   file so the published bound stays contiguous. If an earlier creation failed after extension,
   the unpublished tail PageNo is reused instead of extending again (no truncation primitive
   exists).
10. **Residency identity.** `modification_generation` and a per-frame `binding_epoch` are never
    reset. Writeback reconciliation compares PageId, epoch, and generation. Debug builds fill
    reset frames with `0xDB`.
11. **Observer.** Always invoked without the pool mutex held, so tests can block threads at
    semantic events. Production pools pass no observer and never release the lock for it.

## Verification

Environment: Ubuntu 24.04.5 LTS, kernel 6.18.44, Ubuntu clang 18.1.3, GCC 13.3.0, CMake 3.28.3,
GoogleTest 1.14.0 (`libgtest-dev`; `libclang-rt-18-dev` installed for sanitizer runtimes).

- `clang-debug`: 260/260 tests pass (209 existing + 51 new, counting parameterized instances).
  The buffer/CLOCK subset passed 30 consecutive repeated runs.
- `gcc-debug`: 260/260 pass, no warnings.
- `clang-asan` (ASan + UBSan): 260/260 pass, no reports.
- ThreadSanitizer (ad hoc `build/clang-tsan`, `-fsanitize=thread`, Debug): 260/260 pass with no
  reports across repeated runs. An initial lock-order-inversion report came from one test
  holding several shared page latches and taking another out of frame order on the same thread;
  the test now performs that fetch on a separate thread. Shared/shared ordering is not a
  deadlock, but multi-page latch ordering is a rule later callers (B+ tree) must define.
- `clang-tidy` (project `.clang-tidy`): no warnings on new production or test files.
- `clang-format`: applied to new files only.

No benchmarks were run; the bitwise CRC32C makes debug load/flush noticeably slow and is a
candidate for measurement under Phase 2 item 13.

## Limitations

See `docs/PROJECT_STATE.md` "Buffer management". In short: no WAL integration, no tail truncation
after failed new-page construction, no background writer or batched `fdatasync`, closed FileIds
report `FILE_OR_PAGE_NOT_FOUND`, and the destructor performs no I/O.

## Architecture questions discovered

- §7.12.4 requires new-page failure before WAL append to perform serialized tail restoration
  (truncation). `DiskManager` has no truncate primitive. PageNo reuse of the unpublished tail page
  is used instead. A truncate primitive, or explicit acceptance of reuse, should be decided before
  WAL/PAGE_INIT work.
- §7.12.5 distinguishes `CLOSED` owners with `FILE_RETIRED_OR_CLOSING`. Without a persistent
  record of closed FileIds, the pool cannot distinguish "closed" from "never registered".
