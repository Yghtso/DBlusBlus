# 0033 — Code Style Refactor and Heap Operation Validation Cost

- Date: 2026-10-21
- Milestone: Code style review follow-up (all recommendations accepted by the project owner)

## Scope

Five behavior-preserving refactor batches over `src/` and `tests/`, plus one deliberate change
to what `HeapPage` operations re-check. No persistent format changed.

## Batches

1. Header guards and naming. `#pragma once` everywhere (16 of 22 path-based guards no longer
   matched their files); `readability-identifier-naming` in `.clang-tidy`; shared test helpers
   (`TemporaryDirectory`, heap page byte helpers) moved to `tests/support` and
   `heap_test_support.h`; BufferPool frames stored in `std::vector`.
2. Error types. `Result<T, E>`, `Status<E>` and `Unexpected<E>` in `src/common/result.h`,
   modeled on `std::expected`, replace about twenty per-module result structs that combined a
   `NONE` code with optional fields. `DBLUS_CHECK` (always on) and `DBLUS_ASSERT` (debug only)
   in `src/common/assert.h` replace `assert`. A missing BufferPool page validator and a failed
   encode of a header that always fits are now internal checks instead of returned errors.
   `FsmPage::GetCategory` became `Category`.
3. Structure. `HeapPage::Validate` (198 lines) and `Compact` split into named passes;
   `BufferPool::PinPage`, `LoadPage` and `NewPage` split into helpers;
   `detail::PageGuardBase` holds the code shared by the two page guards; the test-only
   inspection methods moved from `BufferPool` to `BufferPoolInspector`.
4. Types. `StrongId<Tag, Rep>` for `FileId`, `TxnId`, `CommandId`, `Lsn`, `TableId`,
   `IndexId` and `SchemaVer`. `PageNo` and `SlotId` stay integers because they are counted
   and used as indexes. Storage code moved to `dblusblus::storage`.
5. Heap operation validation cost, below.

## Heap operation validation cost

Before: `Insert`, `MarkDead`, `Compact` and `TupleBytes` each ran the complete `Validate()`,
which decodes every retained tuple under its schema. That made every operation cost
O(retained tuple bytes), on pages the BufferPool had already validated on load.

Architecture §4.13.1 already places complete L1 validation at BufferPool load, says L0/L1 are
already established for ordinary scan and update, and requires the after-image of a mutation to
pass L1. It does not require re-validating the before-image on every operation. No
architecture text changed.

After: those operations run `ValidateStructure()`, which is `Validate()` without decoding
stored tuples (headers, slot descriptors, range bounds, overlap, free chain; O(slots)). This
keeps every byte write inside the page and keeps rejected operations non-mutating. The
after-image stays L1-valid by construction: `Insert` fully validates the new tuple, and
`MarkDead` and `Compact` never change tuple bytes. Behavior is the same in debug and release
builds.

The project owner accepted the review option "validate on load and in debug builds, trust
resident pages in release". The change above keeps the load-time half and replaces the
debug/release split with one rule for both builds, so tests do not depend on the build type
and release builds keep the structural checks that guard memory safety.

One test changed meaning: `MarkDead` on a page whose only defect is damaged tuple bytes now
succeeds, and the test asserts that `Validate()` still reports `INVALID_TUPLE` while
`ValidateStructure()` accepts the page.

Measured once with a throwaway program (clang-release, -O2, not a committed benchmark) on a
page filled with one-column VARCHAR tuples:

| Tuples on page | `Validate` | `ValidateStructure` | Filling the page, before → after |
|---|---|---|---|
| 111 (8-byte payload) | about 6.2 µs | about 1.3 µs | about 380 µs → 120 µs |
| 63 (64-byte payload) | about 3.4 µs | about 0.7 µs | about 120 µs → 45 µs |

## Verification

All 290 tests pass under `clang-debug`, `gcc-debug` and `clang-asan`; `clang-tidy` and
`clang-format --dry-run --Werror` are clean.

## Corrections to the style review

The review said some `noexcept` functions allocate. That was wrong: `EncodeTuple`, the
allocating one, is not `noexcept`.
