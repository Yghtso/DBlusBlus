# 0025 — HEAP_DATA Ordinary Page-Number Validation

- Date: 2026-08-15
- Milestone: Phase 1 sanity finding F1 — HEAP_DATA ordinary page-number conformance

## Scope

Corrected the page-local `HeapPage` controller so it cannot initialize or validate an ordinary
`HEAP_DATA` page at file page `0` or at `INVALID_PAGE_NO`.

This task did not change page layout, PageNo width or sentinel values, superblock behavior, FSM
behavior, file allocation, BufferPool, HeapFile, or any other Phase 2 or later subsystem.

## Architecture followed

- `docs/ARCHITECTURE.md` §4.3, fundamental PageNo width and `INVALID_PAGE_NO`
- `docs/ARCHITECTURE.md` §4.13 invariant 5, page `0` is the file superblock and ordinary object
  pages begin at page `1`
- `docs/ARCHITECTURE.md` §5.1, heap page `0` superblock and pages `1..N` HEAP_DATA
- `docs/ARCHITECTURE.md` §5.3, HEAP_DATA common-header identity requirements

The architecture was already settled and was not modified.

## Previous behavior

`HeapPage::Initialize` copied `Page::Id().page_no` into the common header without rejecting page
`0` or `INVALID_PAGE_NO`. `HeapPage::Validate` checked equality between the persisted common-header
page number and the owning `PageId`, but an equal invalid ordinary page number passed that check.

A directly constructed `Page` could therefore initialize and validate as HEAP_DATA using either
invalid ordinary page number.

## Files changed

- `src/storage/heap_page.h`
  - added `HeapPageValidationError::INVALID_PAGE_NUMBER`
- `src/storage/heap_page.cpp`
  - added the local ordinary HeapPage PageNo predicate
  - guarded initialization before any page mutation
  - added structural validation of the owning PageNo
- `tests/heap_page_test.cpp`
  - added valid lower-bound, atomic invalid-initialization, and invalid-owner validation tests
- `docs/PROJECT_STATE.md`
  - advanced the current checkpoint, recorded the enforced ordinary PageNo boundary, and updated
    the verified test totals
- `devlog/0025-heap-page-number-validation.md`
  - recorded this milestone

No older devlog was modified.

## Initialization behavior

`HeapPage::Initialize` now checks the owning `Page::Id().page_no` before constructing or writing
headers.

It returns `false` when:

```text
page_no == 0
or
page_no == INVALID_PAGE_NO
```

Every valid ordinary PageNo beginning at `1` retains the existing deterministic initialization
behavior.

## Atomic failure behavior

The new initialization guard executes before `Page::Initialize`, page zeroing, or any header
encoding. Rejection therefore leaves all 8192 caller-owned page bytes unchanged.

Focused tests prefill each invalid page with `0xA5`, retain a full byte copy, invoke
`HeapPage::Initialize`, and require byte-for-byte equality after failure.

## Validation behavior

`HeapPage::Validate` retains the existing persisted/in-memory page-number equality check. Once
that identity agrees, it rejects an owning page number of `0` or `INVALID_PAGE_NO` with:

```text
HeapPageValidationError::INVALID_PAGE_NUMBER
```

All existing page type, format version, header size, zero-only flags, reserved fields, heap
geometry, slot-state, NORMAL range, UNUSED/free-list, and corruption checks remain unchanged.

## Tests added

- PageNo `1` initializes and validates as the lowest valid ordinary heap page.
- Initialization rejects page `0` without changing any page byte.
- Initialization rejects `INVALID_PAGE_NO` without changing any page byte.
- Otherwise canonical HEAP_DATA bytes associated with page `0` fail validation.
- Otherwise canonical HEAP_DATA bytes associated with `INVALID_PAGE_NO` fail validation.
- Existing persisted/in-memory page-number mismatch coverage remains passing.

## Verification

- `cmake --build --preset clang-debug`: passed without project warnings.
- `ctest --test-dir build/clang-debug -R '^Heap(Page|Slot)' --output-on-failure`: 51/51 passed on
  the final focused run.
- `ctest --preset clang-debug --output-on-failure`: 208/208 passed.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built
  successfully without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 208/208 passed.
- `cmake --preset clang-asan`: configured successfully.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built with AddressSanitizer and
  UndefinedBehaviorSanitizer enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 208/208 passed with
  no ASan or UBSan findings. LeakSanitizer alone remained disabled for the documented ptrace
  environment limitation.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without diagnostics on
  the final run.
- `clang-format --dry-run --Werror src/storage/heap_page.h src/storage/heap_page.cpp
  tests/heap_page_test.cpp`: passed.
- `git diff --check`: passed.

No benchmark was run because this is a narrow invalid-input conformance guard, not a
performance-sensitive change.

## Persisted-format and phase boundaries

No persisted field, offset, width, numeric code, sentinel, page format version, or valid emitted
HEAP_DATA encoding changed. The correction prevents invalid contextual use of existing PageNo
values.

The concurrent `ExtendFile` test and `PageFile` lifetime documentation findings were not addressed.
No BufferPool, frame, page guard, HeapFile, or other Implementation Phase 2 work was performed.

## Current consistency state

Currently implemented Phase 1 functionality has no known architecture mismatches after this fix.

## Architecture questions discovered

None.
