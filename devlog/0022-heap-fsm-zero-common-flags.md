# 0022 — HEAP_DATA/FSM_DATA Zero-Only Common Flags

- Date: 2026-08-15
- Milestone: HEAP_DATA and FSM_DATA v1 common-header flag conformance

## Scope

Reconciled the Phase 1 HEAP_DATA and FSM_DATA page controllers with the settled v1 zero-only
common-header flag contract. The change is limited to page-format-specific initialization,
validation, APIs, tests, and current-state documentation.

This task did not change the generic common page-header codec, heap UNUSED/free-list handling,
BTREE FileSuperblock handling, or any Phase 2 or later subsystem.

## Architecture followed

- `docs/ARCHITECTURE.md` §4.8, Common page header
- `docs/ARCHITECTURE.md` §5.3, HEAP_DATA page format v1
- `docs/ARCHITECTURE.md` §5.3.3, Reserved and hint fields
- `docs/ARCHITECTURE.md` §6.5, FSM_DATA page format v1
- `docs/ARCHITECTURE.md` §6.5.1, FSM_DATA byte layout
- `docs/ARCHITECTURE.md` §6.8, Blank FSM_DATA page initialization

The v1 owning formats assign no HEAP_DATA or FSM_DATA common-header flag bits. Their initializers
must write zero and their validators must reject every nonzero 32-bit flag pattern.

## Previous mismatch

`HeapPage::Initialize` accepted a caller-supplied `std::uint32_t flags` argument and persisted it.
`FsmPageInitialization` exposed the same caller-controlled field. Both page validators checked the
common reserved field but accepted nonzero common flags. Existing initialization tests explicitly
exercised and accepted that stale behavior.

## Files changed

- `src/storage/heap_page.h`
  - removed the caller-controlled flag argument from `HeapPage::Initialize`
  - added `HeapPageValidationError::NONZERO_COMMON_FLAGS`
- `src/storage/heap_page.cpp`
  - made initialization write `flags = 0`
  - made validation reject `common_header.flags != 0`
- `src/storage/fsm_page.h`
  - removed `flags` from `FsmPageInitialization`
  - added `FsmPageValidationError::NONZERO_COMMON_FLAGS`
- `src/storage/fsm_page.cpp`
  - made initialization write `flags = 0`
  - made validation reject `common_header.flags != 0`
- `tests/heap_page_test.cpp`
  - replaced the stale nonzero-flag initialization expectation and added corruption rejection
    coverage
- `tests/fsm_page_test.cpp`
  - replaced the stale nonzero-flag initialization expectation and added corruption rejection
    coverage
- `docs/PROJECT_STATE.md`
  - removed the resolved HEAP_DATA/FSM_DATA flag mismatch, advanced the checkpoint, and recorded
    the current verification totals
- `devlog/0022-heap-fsm-zero-common-flags.md`
  - recorded this milestone

No architecture document or older devlog was modified.

## Public API changes

Heap initialization is now:

```cpp
bool HeapPage::Initialize(Lsn page_lsn = INVALID_LSN) noexcept;
```

The stale caller-controlled flag argument was removed. Explicit pre-WAL page-LSN initialization
remains available.

`FsmPageInitialization` now contains only:

```cpp
std::uint16_t entry_count;
Lsn page_lsn;
```

Its stale `flags` field was removed. No production caller required either removed control.

## Exact initialization behavior

- `HeapPage::Initialize` always encodes HEAP_DATA common-header `flags = 0`.
- `FsmPage::Initialize` always encodes FSM_DATA common-header `flags = 0`.
- Caller-controlled page LSN and FSM initialized-prefix behavior are unchanged.
- Page types, format versions, header sizes, checksums, reserved fields, page numbers, and all
  page-specific bytes retain their existing encoding.

## Exact validation behavior

- `HeapPage::Validate` rejects any persisted HEAP_DATA page whose decoded common-header flags are
  nonzero with `HeapPageValidationError::NONZERO_COMMON_FLAGS`.
- `FsmPage::Validate` rejects any persisted FSM_DATA page whose decoded common-header flags are
  nonzero with `FsmPageValidationError::NONZERO_COMMON_FLAGS`.
- Validation does not normalize or mutate malformed pages.

The generic `CommonPageHeader` encoder/decoder remains capable of preserving raw flag bits. The
zero-only rule is enforced only by the HEAP_DATA and FSM_DATA owners, leaving other page formats
free to apply their own architecture-defined semantics.

## Tests changed and added

- Replaced `HeapPageTest.InitializationPreservesExplicitFlagsAndPageLsn` with a test requiring zero
  flags while preserving an explicit page LSN.
- Updated deterministic FSM initialization to require zero flags while preserving its explicit page
  LSN and initialized entry count.
- Added dedicated HeapPage and FsmPage validation tests that reject both `0x00000001` and
  `0xFFFFFFFF` flag patterns.
- Existing common-header codec tests continue to verify generic raw flag preservation.
- Existing page type, version, header-size, checksum/reserved, heap slot, FSM mapping/category,
  persistence, DEAD transition, and compaction tests remain passing.

## Verification

- `cmake --preset clang-debug`: configured successfully.
- `cmake --build --preset clang-debug`: built successfully without project warnings.
- `ctest --preset clang-debug -R '^(HeapPage|FsmPage)' --output-on-failure`: 43/43 passed.
- `ctest --preset clang-debug --output-on-failure`: 189/189 passed.
- `cmake --preset clang-asan`: configured successfully.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built successfully with ASan and
  UBSan enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 189/189 passed with
  no AddressSanitizer or UndefinedBehaviorSanitizer findings. LeakSanitizer alone remained disabled
  for the documented ptrace environment limitation.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built successfully
  without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 189/189 passed.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without final
  diagnostics after correcting a test-only optional-access warning.
- `clang-format --dry-run --Werror` on all touched C++ files: passed.
- `git diff --check`: passed on the final diff.

No benchmark was run because this is a persisted-format conformance correction with no
performance-policy change.

## Persisted-format compatibility

No offset, width, numeric code, page size, header size, checksum rule, format version, or valid
emitted encoding changed. Initializers now emit only the already-canonical zero flag value, and
validators reject previously tolerated bytes that v1 declares invalid. No persistent-format
version bump was required.

## Heap UNUSED/free-list behavior

Heap slot-state, tuple-coordinate, free-list, DEAD transition, compaction, and slot-reuse behavior
were not changed. In particular, this task did not implement DEAD-to-UNUSED reclamation, immediate
slot reuse, or the separate known UNUSED/free-list validation correction.

## Current state and deferred work

The resolved HEAP_DATA/FSM_DATA flag item was removed from `docs/PROJECT_STATE.md`. The remaining
verified Phase 1 implementation/architecture mismatch set is:

1. BTREE FileSuperblock codec,
2. heap UNUSED/free-list structural validation.

Those mismatches were not changed by this task. BufferPool, HeapFile, B+ tree, WAL, MVCC, catalog,
and every other later subsystem remain deferred. Implementation Phase 2 was not entered.

## Architecture questions discovered

None. This task enforced already-settled v1 page-format requirements.
