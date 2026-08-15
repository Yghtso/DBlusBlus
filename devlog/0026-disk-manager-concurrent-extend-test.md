# 0026 — Concurrent DiskManager File-Extension Verification

- Date: 2026-08-15
- Milestone: Pre-Phase-2 sanity finding F2 — direct concurrent `DiskManager::ExtendFile` test

## Scope

Added deterministic concurrent test coverage for multiple workers extending the same registered
raw page file through `DiskManager::ExtendFile`.

This task changed no production behavior, persisted format, architecture, PageFile lifetime
contract, or Phase 2 subsystem.

## Architecture followed

- `docs/ARCHITECTURE.md` §4.2, canonical `PAGE_SIZE = 8192`
- `docs/ARCHITECTURE.md` §4.11, append-first allocation and serialized count-plus-extend
- `docs/ARCHITECTURE.md` §7.3, `DiskManager` responsibility boundary
- `docs/ARCHITECTURE.md` §7.4.5, exact page-aligned file sizes

The architecture was already settled and was not modified.

## Previous test gap

Sequential extension tests established one-page growth and monotonic PageNos, but no direct test
exercised simultaneous `ExtendFile` requests against one registered file. This was a verification
gap, not a known implementation mismatch.

## Existing synchronization observed

`DiskManager::ExtendFile` holds `extension_mutex_` across file lookup, aligned page-count discovery,
checked size calculation, and `ftruncate`. The new test exercises that existing synchronization;
production code was not changed.

## Files changed

- `tests/disk_manager_test.cpp`
  - added the concurrent same-file extension test
- `docs/PROJECT_STATE.md`
  - advanced the checkpoint, recorded the concurrent coverage, and updated test totals
- `devlog/0026-disk-manager-concurrent-extend-test.md`
  - recorded this milestone

No production source, older devlog, architecture document, or other verification document changed.

## Test concurrency shape

The test creates and registers one empty file, reads its actual initial page count, and starts:

```text
8 worker threads
64 ExtendFile calls per worker
512 total concurrent extension requests
```

A C++20 `std::barrier` releases all eight workers from one start point. Each worker writes only to
its own preassigned result indices, so result collection contains no data race and no GoogleTest
assertion executes on a worker thread.

The main test thread joins every worker before examining results. It does not assert thread
interleaving or per-thread allocation order.

## Results established by the test

All 512 `ExtendFile` calls succeeded.

After sorting, the returned PageNos:

- contained no duplicates,
- began at the actual pre-test page count returned by `DiskManager::PageCount`,
- formed one contiguous 512-page range without gaps.

The empty test file began at page count `0`, so the final count was `512`. Both the DiskManager size
query and operating-system physical file size were exactly:

```text
512 * 8192 = 4,194,304 bytes
```

The final size was exactly page aligned. Existing sequential extension behavior remained passing.

## Verification

- `cmake --build --preset clang-debug`: passed.
- `ctest --test-dir build/clang-debug -R
  '^DiskManagerTest.ConcurrentExtensionsAllocateUniqueContiguousPageNumbers$' --repeat
  until-fail:20 --output-on-failure`: passed 20 consecutive runs.
- `ctest --test-dir build/clang-debug -R '^DiskManagerTest\.' --output-on-failure`: 9/9 passed
  on the final focused run.
- `ctest --preset clang-debug --output-on-failure`: 209/209 passed.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built
  successfully without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 209/209 passed.
- `cmake --preset clang-asan`: configured successfully.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built with AddressSanitizer and
  UndefinedBehaviorSanitizer enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 209/209 passed with
  no ASan or UBSan findings. LeakSanitizer alone remained disabled for the documented ptrace
  environment limitation.
- `cmake --preset clang-tidy`: configured successfully.
- `cmake --build --preset clang-tidy`: passed without diagnostics on the final run.
- `clang-format --dry-run --Werror tests/disk_manager_test.cpp`: passed.
- `git diff --check`: passed.

No ThreadSanitizer preset or existing TSan workflow is configured in the repository, so TSan was
not run and no new configuration was introduced.

## Production and phase boundaries

No production code changed because the new test passed against the existing implementation.

The remaining pre-Phase-2 sanity item is the separate PageFile lifetime-contract documentation
review. It was not addressed here.

No BufferPool, BufferFrame, page guard, HeapFile, PageFile lifetime change, or other Implementation
Phase 2 work was performed.

## Architecture questions discovered

None.
