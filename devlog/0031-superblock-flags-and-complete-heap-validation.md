# 0031 — Zero Superblock Flags and Complete HEAP Owner Validation

- Date: 2026-10-21
- Milestone: Close the two recorded storage-layer architecture mismatches

## Scope

Repaired the two implementation defects that `docs/PROJECT_STATE.md` listed against settled
architecture rules:

1. the generic `FileSuperblock` codec preserved nonzero common `flags` (§4.10.4 requires
   RESERVED_ZERO);
2. `HeapPage::Validate` was not the complete page-local HEAP owner validation of §§4.13.3 and
   5.21.

No architecture rule changed. No buffer pool, HeapFile, catalog, or other gated subsystem was
started.

## Superblock flags

- `FileSuperblock` no longer has a `flags` member. V1 assigns no flag bits, so there is nothing
  to carry or round-trip.
- `EncodeFileSuperblock` always writes `flags = 0`.
- `DecodeFileSuperblock` rejects any nonzero flags value with the new
  `FileSuperblockDecodeError::NONZERO_FLAGS`, checked after file-kind/header-size/page-size
  dispatch and before the reserved-byte check.
- The exact-layout golden test now expects zero flag bytes; its CRC32C changed from
  `0xFE76015C` to `0x477A0961`. Both values were recomputed with an independent Python
  CRC32C implementation over the same 8192-byte image.

## HEAP owner validation

### Schema context

`HeapPage` is now constructed as `HeapPage(Page&, HeapTupleSchemaHistory)`.
`HeapTupleSchemaHistory` is a non-owning span of immutable `TuplePhysicalLayout`s with a
`Resolve(SchemaVer)` lookup; `SchemaVer{0}` never resolves. This is the "heap schema-history
descriptor" that §4.13.1 allows the registered owner validator to use. `HeapPage` still never
interprets column types (§5.19): it resolves the layout from the tuple header and delegates to
`ValidateTuple`.

The alternative of keeping a schema-free structural `Validate` beside a schema-aware one was
rejected because §4.13.1 forbids publishing a page under a less precise validator, and every
existing caller (insert, mark-dead, compact, tuple lookup) runs validation first.

### Validation order

1. Common header and heap header checks (unchanged).
2. Per-slot canonical descriptors: unknown code -> `INVALID_SLOT_STATE`;
   `REDIRECT_RESERVED` -> `UNSUPPORTED_RESERVED_STATE`; `UNUSED` coordinates `(0,0)`;
   `NORMAL`/`DEAD` `aux != 0` -> `NONZERO_SLOT_AUX`; `NORMAL` length 0 ->
   `EMPTY_NORMAL_TUPLE`; `DEAD` with exactly one zero coordinate ->
   `NONCANONICAL_DEAD_SLOT`; retained range outside `[upper, PAGE_SIZE)` ->
   `RETAINED_TUPLE_OUT_OF_BOUNDS` (renamed from `NORMAL_TUPLE_OUT_OF_BOUNDS`, now covering
   DEAD too).
3. Pairwise non-overlap of every retained NORMAL/DEAD range, by sorting on
   `(start, end, SlotId)` -> `OVERLAPPING_TUPLE_RANGES` with both slot ids reported.
4. Per retained tuple, in SlotId order: header decode, schema resolution
   (`UNRESOLVED_SCHEMA_VERSION`), `ValidateTuple` (`INVALID_TUPLE` with the codec error and
   header error reported), transaction-field domains
   (`INVALID_TUPLE_TRANSACTION_FIELDS`), and bounded previous-version checks
   (`INVALID_PREVIOUS_VERSION_POINTER`: page 0, self-reference, or same-page slot beyond
   `slot_count`).
5. Free-slot chain checks (unchanged).

The transaction-field and previous-pointer checks live in the heap validator, as §4.13.3 places
them, rather than in the tuple-header codec; moving them into the codec would have rejected the
default `TupleVersionMetadata` used by codec-only tests.

### Writers

- `Insert` validates the incoming tuple with the same per-tuple rules, using the RID it would
  receive, before any mutation; failures return `HeapPageInsertError::INVALID_TUPLE` with
  `tuple_error` detail. Zero-length inserts are therefore rejected.
- `Compact` dropped its private DEAD-range and overlap checks, now guaranteed by `Validate`.
  `HeapPageCompactError::TUPLE_RANGE_OUT_OF_BOUNDS` and `OVERLAPPING_TUPLE_RANGES` and
  `HeapPageCompactResult::other_slot_id` were removed. The packing loop keeps a debug
  `assert` for the non-overlap invariant.

## Tests

- New `tests/storage/heap/heap_test_tuples.h` builds real tuples of any exact size >= 57 bytes
  under a one-column NOT NULL VARCHAR schema (SchemaVer 1); `tests/` was added as a private
  include directory of `dblusblus_tests`.
- Heap insert, reclamation, persistence, and validation tests now store encoded tuples instead
  of opaque byte arrays. Tests that set nonzero `aux` on NORMAL/DEAD slots or inserted
  zero-length tuples asserted behavior that §4.13.3 forbids and were rewritten.
- New validation tests cover: REDIRECT_RESERVED, nonzero aux, empty NORMAL, mixed DEAD
  coordinates, DEAD out-of-bounds, three overlap shapes, adjacent ranges and unowned holes,
  malformed/short/mis-sized/schema-invalid tuple bytes in NORMAL and DEAD slots, unknown and
  zero schema versions, empty schema history, transaction-field domains, previous-pointer
  cases, multi-version schema resolution, and deterministic first-failure slot reporting.
- New insert tests cover rejected tuples without mutation and a valid same-page previous
  pointer. A new compaction test moves NORMAL payloads around a retained DEAD range and a hole.
- New superblock tests cover zero-written flags and rejection of each of the 32 flag bits.
- Three tuple-layer composition tests now pass a schema history and a valid `xmin`.

Test population: 230 (was 209).

## Checks run

Ubuntu 24.04 container, clang 18.1.3, GCC 13.3.0, CMake 3.28.3, GoogleTest 1.14.0:

- `clang-debug`: 230/230 passed.
- `gcc-debug`: 230/230 passed, no warnings.
- `clang-asan` (ASan + UBSan): 230/230 passed.
- `clang-tidy` preset: no diagnostics.
- `clang-format --dry-run` clean on every touched file.

## Documentation

`docs/PROJECT_STATE.md` was updated because its mismatch section and its heap/superblock
descriptions became false. `docs/ARCHITECTURE.md`, `docs/DEVELOPMENT.md`, and
`docs/VERIFICATION.md` were not changed.

## Architecture questions

None. Both changes implement settled rules.
