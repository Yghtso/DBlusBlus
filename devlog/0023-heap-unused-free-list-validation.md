# 0023 — Heap UNUSED/Free-List Structural Validation

- Date: 2026-08-15
- Milestone: Phase 1 HeapPage UNUSED-slot and free-list validation conformance

## Scope

Completed the missing read-only structural validation for the canonical persisted `UNUSED`
slot representation and page-local free-slot list. The task changed validation and corruption
tests only; it did not add a `DEAD -> UNUSED` transition, reclamation eligibility, vacuum, slot
allocation from the list, or slot reuse.

The only remaining verified Phase 1 implementation/architecture mismatch is the BTREE
FileSuperblock codec. That mismatch and all Phase 2 or later subsystems were outside this task.

## Architecture followed

- `docs/ARCHITECTURE.md` §5.3.2, HEAP_DATA-specific header and `free_slot_head`
- `docs/ARCHITECTURE.md` §5.4.2, canonical `UNUSED` slot representation
- `docs/ARCHITECTURE.md` §5.4.3, `DEAD` slot treatment
- `docs/ARCHITECTURE.md` §5.21, heap-page structural invariants
- `docs/ARCHITECTURE.md` §14.12, delayed RID-slot reclamation and reuse boundary

The architecture is unambiguous for v1: every persisted `UNUSED` slot is in canonical reusable
form and appears exactly once in the page's free list. Chapter 14 remains the sole authority for
when a `DEAD` slot may later transition into that form.

## Previous validation gap

`HeapPage::Validate` decoded all slot states and checked tuple coordinates for `NORMAL` slots, but
it checked `free_slot_head` only for an empty slot directory. It could accept nonzero tuple
coordinates on an `UNUSED` slot, invalid or non-`UNUSED` links, cycles, and `UNUSED` slots omitted
from the list.

## Files changed

- `src/storage/heap_page.h`
  - replaced the narrow empty-page head error with focused canonical-slot and free-list validation
    errors
- `src/storage/heap_page.cpp`
  - added bounded canonical `UNUSED`, link, head, traversal, cycle, and membership validation
- `tests/heap_page_test.cpp`
  - replaced stale permissive expectations and added canonical and corrupt free-list cases
- `docs/PROJECT_STATE.md`
  - removed the resolved mismatch, advanced the checkpoint, and recorded the verified test totals
- `devlog/0023-heap-unused-free-list-validation.md`
  - recorded this milestone

`docs/ARCHITECTURE.md` and all older devlogs were unchanged.

## Canonical UNUSED field validation

For every decoded `HeapSlotState::UNUSED` entry, validation now requires:

```text
tuple_offset = 0
tuple_length = 0
aux          = INVALID_SLOT_ID or an in-range SlotId naming another UNUSED slot
```

Nonzero tuple coordinates return `NONCANONICAL_UNUSED_SLOT`. An invalid or non-`UNUSED` `aux`
target returns `INVALID_FREE_SLOT_LINK` and identifies the source slot.

Existing state-local handling for `NORMAL`, `DEAD`, and `REDIRECT_RESERVED` slots remains intact.
In particular, this task did not add tuple-range semantics for pre-compaction `DEAD` coordinates.

## free_slot_head validation

`free_slot_head` must be `INVALID_SLOT_ID` or an in-range SlotId naming an `UNUSED` slot. This rule
is applied for every slot count, including an empty directory. An invalid, `NORMAL`, or `DEAD`
head returns `INVALID_FREE_SLOT_HEAD`.

## Traversal, links, cycles, and membership

After all slots and links have been validated, `HeapPage::Validate` follows the list from
`free_slot_head` until `INVALID_SLOT_ID`:

- every traversed slot is already known to be in range and `UNUSED`,
- a repeated SlotId returns `FREE_SLOT_CYCLE`, covering self-loops and longer cycles,
- traversal must terminate at the canonical sentinel,
- the visited membership must exactly equal the set of all persisted `UNUSED` slots,
- an omitted `UNUSED` slot returns `FREE_SLOT_MEMBERSHIP_MISMATCH`.

These checks imply that no `NORMAL` or `DEAD` slot can be a list member and that every `UNUSED`
slot appears exactly once.

## Bounded validation structure and mutation behavior

Validation uses one fixed `std::array<HeapSlotEntry, 1018>` and two fixed
`std::bitset<1018>` instances. The bound is the maximum slot count representable by an 8192-byte
page with a 48-byte heap header and 8-byte slot entries. Corrupt persisted counts are rejected by
the existing geometry checks before these structures are indexed.

The algorithm is O(`slot_count`), performs no heap allocation, and does not mutate the page.

## Validation errors

The focused `HeapPageValidationError` categories are now:

- `NONCANONICAL_UNUSED_SLOT`
- `INVALID_FREE_SLOT_HEAD`
- `INVALID_FREE_SLOT_LINK`
- `FREE_SLOT_CYCLE`
- `FREE_SLOT_MEMBERSHIP_MISMATCH`

The obsolete empty-directory-only `INVALID_EMPTY_FREE_SLOT_HEAD` category was removed.

## Tests changed and added

The previous test that accepted arbitrary tuple coordinates for all non-`NORMAL` states was
narrowed to `DEAD` and `REDIRECT_RESERVED`; `UNUSED` now follows its settled canonical contract.
Tests that exercise MarkDead and Compact rejection now construct a canonical persisted `UNUSED`
list directly instead of relying on malformed state mutation.

Focused coverage now proves:

- canonical pages with no `UNUSED` slots validate,
- canonical one-element and multi-element lists validate and terminate at the sentinel,
- nonzero `UNUSED` tuple offset or length is rejected,
- out-of-range, `NORMAL`, and `DEAD` heads are rejected,
- out-of-range, `NORMAL`, and `DEAD` next links are rejected,
- self-cycles and multi-slot cycles are rejected,
- an `UNUSED` slot omitted from the list is rejected,
- existing insertion, DEAD transition, compaction, and persistence behavior remains passing.

No production transition was added merely to construct test fixtures; canonical and malformed
persisted states are encoded directly in tests.

## Verification

- `cmake --preset clang-debug && cmake --build --preset clang-debug`: configured and built
  successfully without project warnings.
- `ctest --test-dir build/clang-debug -R '^HeapPage' --output-on-failure`: 44/44 passed.
- `ctest --preset clang-debug --output-on-failure`: 202/202 passed.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built
  successfully without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 202/202 passed.
- `cmake --preset clang-asan`: configured successfully.
- The first sanitizer link-time test discovery encountered the documented LeakSanitizer-under-
  ptrace failure.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built successfully with
  AddressSanitizer and UndefinedBehaviorSanitizer enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 202/202 passed with
  no ASan or UBSan findings. LeakSanitizer alone was disabled for the documented ptrace environment
  limitation.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without diagnostics.
- `clang-format --dry-run --Werror` on all touched C++ files: passed.
- `git diff --check`: passed.

No benchmark was run because this task changes corruption validation, not runtime mutation or
performance policy.

## Persisted format and behavior boundaries

No slot offset, width, state code, sentinel, page layout, header size, format version, or valid
persisted encoding changed. The validator now rejects malformed bytes that v1 already declared
invalid.

Validation still does not create `UNUSED` entries. No `DEAD -> UNUSED` transition, vacuum or
read-epoch decision, free-list insertion mutation, allocation from the free list, immediate slot
reuse, or SlotId reuse policy was implemented. Existing append-only insertion behavior is
unchanged.

## Current state and deferred work

The resolved heap `UNUSED`/free-list item was removed from `docs/PROJECT_STATE.md`. The remaining
verified Phase 1 implementation/architecture mismatch set is:

1. BTREE FileSuperblock codec.

BufferPool, HeapFile, B+ tree, WAL, MVCC, vacuum, catalog, and all other later subsystems remain
deferred. Implementation Phase 2 was not entered.

## Architecture questions discovered

None. This task enforces already-settled v1 HEAP_DATA invariants.
