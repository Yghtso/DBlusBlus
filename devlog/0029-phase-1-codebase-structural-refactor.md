# 0029 — Phase-1 Codebase Structural Refactor

- Date: 2026-08-21
- Milestone: Behavior-preserving Phase-1 source, test, and build organization

## Purpose and scope

Reorganized the accepted Phase-1 implementation around explicit current subsystem ownership before
any Phase-2 work begins. The change improves navigation and dependency visibility while preserving
the same APIs, algorithms, validation, persisted formats, tests, and runtime behavior.

The frozen `docs/ARCHITECTURE.md` was followed unchanged. No BufferPool, HeapFile, future subsystem
directory, placeholder, or Phase-2 implementation was introduced.

## Source organization

The production tree now uses these implemented ownership groups:

```text
src/common/
src/storage/disk/
src/storage/file/
src/storage/page/
src/storage/heap/
src/storage/tuple/
```

Important moves include:

- `DiskManager` under `storage/disk/`;
- generic file-superblock and `PageFile` ownership under `storage/file/`;
- the raw `Page` and common persisted page header under `storage/page/`;
- heap-page, FSM page, and rebuildable FSM candidate-index code under `storage/heap/`;
- tuple header, physical layout, and codec code under `storage/tuple/`.

All source and test includes use project-relative paths rooted at `src/`.

## Heap-page responsibility split

The persisted heap-page declarations and codec definitions were extracted without semantic changes
into:

```text
storage/heap/heap_page_format.h
storage/heap/heap_page_format.cpp
```

This unit owns the existing heap format constants, geometry, `HeapSlotState`, `HeapPageHeader`,
`HeapSlotEntry`, codec result/error types, and the four heap header/slot encode/decode functions.

`heap_page.h/.cpp` continues to own the mutable `HeapPage` controller, initialization, structural
validation, insertion, NORMAL-to-DEAD mutation, compaction, tuple lookup, and page mutation logic.
`FsmPage`, `TuplePhysicalLayout`, and `TupleCodec` now depend directly on the format header when they
need only heap geometry.

The empty `common/types.cpp` translation unit was removed; identifier definitions remain header-only
in `common/types.h`.

## Test organization

Tests now mirror production ownership under `tests/common/` and `tests/storage/`. The project smoke
test remains at the test root.

Identifier, `PageId`, and `Rid` tests moved from `smoke_test.cpp` to `common/types_test.cpp`.
The monolithic heap-page test source was split by existing responsibility into format, validation,
insertion, reclamation, and persistence translation units. Every original GTest suite and case name
is present exactly once; no assertion or test data semantics changed.

The discovered test population remains exactly 209.

## CMake and documentation

- `src/CMakeLists.txt` remains the single production target source list, uses the new paths, lists
  headers as private source entries, adds `heap_page_format.cpp`, and removes `types.cpp`.
- `tests/CMakeLists.txt` remains one `dblusblus_tests` executable with GoogleTest discovery and uses
  the reorganized paths.
- Root and benchmark CMake files are unchanged.
- `docs/DEVELOPMENT.md` now distinguishes the current implemented layout from authorization-gated
  future expansion guidance.
- `docs/PROJECT_STATE.md` records this completed pre-Phase-2 milestone.
- `AGENTS.md`, `docs/ARCHITECTURE.md`, and `docs/VERIFICATION.md` are unchanged.

## Verification

The final reorganized tree passed:

- `cmake --preset clang-debug`
- `cmake --build --preset clang-debug`
- `ctest --preset clang-debug -N`: 209 tests discovered
- `ctest --preset clang-debug`: 209/209 passed
- `cmake --preset clang-asan`
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan`: 209/209 passed
- `cmake --preset gcc-debug`
- `cmake --build --preset gcc-debug`
- `ctest --preset gcc-debug`: 209/209 passed
- `cmake --preset clang-tidy`
- `cmake --build --preset clang-tidy`: clean
- `cmake --preset clang-debug -DDBLUSBLUS_WARNINGS_AS_ERRORS=ON`
- `cmake --build --preset clang-debug`: passed
- project-wide C++ `clang-format --dry-run --Werror`: passed
- `cmake --preset clang-bench`
- `cmake --build --preset clang-bench`: passed
- `git diff --check`: passed

LeakSanitizer was disabled because it is unsupported in the ptrace execution environment. AddressSanitizer
and UndefinedBehaviorSanitizer remained enabled and reported no errors.

## Compatibility and phase status

Persisted bytes, production behavior, validation outcomes, and error behavior are unchanged.

Phase 1 remains complete. Implementation Phase 2 remains not started and was not authorized by this
refactor. BufferPool remains not implemented and not in progress; HeapFile remains deferred.

## Architecture questions discovered

None.
