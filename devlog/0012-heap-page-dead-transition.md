# 0012 — Heap Page DEAD Transition

Date: 2026-08-14

## Milestone/task

Phase 1: physical heap-slot transition from `NORMAL` to persistent `DEAD`, without tuple-byte
reclamation, slot reuse, free-list linkage, or compaction.

## Scope

Added one narrow `HeapPage` operation that marks a caller-authorized `NORMAL` slot `DEAD`. The
page layer validates structural safety and the target state but does not decide MVCC visibility,
global death, index-cleanup completion, vacuum eligibility, or transaction outcome.

No SQL DELETE behavior, tuple-header update, vacuum horizon, retirement epoch, `UNUSED`
transition, slot reuse, free-list linkage, tuple-byte zeroing, compaction, FSM, HeapFile,
BufferPool, WAL, or recovery behavior was added.

## Files changed

- `src/storage/heap_page.h` — adds the DEAD-transition error/result types and `MarkDead` API.
- `src/storage/heap_page.cpp` — implements validated `NORMAL -> DEAD` mutation.
- `tests/heap_page_test.cpp` — adds focused transition, failure-atomicity, stable-RID,
  neighboring-slot, physical-byte, and reopen-persistence tests.
- `devlog/0012-heap-page-dead-transition.md` — records this task.

No CMake file or older devlog entry was modified.

## Architecture sections used

- §26 — vacuum/garbage-collection boundary
- §49 — Phase 1 raw-storage implementation order
- §54–§56 — `SlotId`, `PageId`, and physical-version `Rid` semantics
- §63 — staged page-checksum policy
- §67 — explicit slot-state codes, `aux`, and stable-slot semantics
- §68 — heap-page free-space geometry
- §80–§81 — logical DELETE and HeapPage visibility boundaries
- §84–§85 — compaction and vacuum physical-reclamation boundaries
- §96–§97 — HeapPage responsibility and conceptual `MarkSlotDead` API
- §260–§264 — two-phase physical RID reclamation, persistent DEAD state, and required
  index-cleanup-before-DEAD ordering

`ARCHITECTURE.md` is the authoritative architecture file used for this task, as specified by
`AGENTS.md`. The obsolete `ARCHITECTURE_LOCKED_V1.md` path named in the task does not exist in the
current repository.

## Exact public API introduced

Declared in `storage/heap_page.h`:

```cpp
enum class HeapPageMarkDeadError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    SLOT_OUT_OF_RANGE,
    INVALID_SLOT_STATE,
    ALREADY_DEAD,
};

struct HeapPageMarkDeadResult {
    HeapPageMarkDeadError error;
    HeapPageValidationError page_error;
    explicit operator bool() const noexcept;
};

class HeapPage {
public:
    HeapPageMarkDeadResult MarkDead(SlotId slot_id) noexcept;
};
```

`page_error` preserves the existing structural-validation category when the transition returns
`PAGE_INVALID`.

## Allowed state transition

The only successful transition is:

```text
NORMAL -> DEAD
```

`UNUSED` and `REDIRECT_RESERVED` return `INVALID_SLOT_STATE`. A target at or beyond `slot_count`,
including `INVALID_SLOT_ID`, returns `SLOT_OUT_OF_RANGE`. An invalid persisted state is rejected by
whole-page validation and returns `PAGE_INVALID` with `page_error = INVALID_SLOT_STATE`.

The operation trusts the caller to have established global death and completed required index
cleanup. It does not perform those decisions or checks itself.

## DEAD -> DEAD behavior

`DEAD -> DEAD` is not treated as idempotent success. It returns the explicit `ALREADY_DEAD` error
and leaves every page byte unchanged. This makes accidental duplicate physical transitions visible
to the current caller.

## Mutation algorithm

1. Run existing full heap-page structural validation.
2. Require `slot_id < slot_count`.
3. Decode the target 8-byte slot explicitly.
4. Require its current state to be `NORMAL`.
5. Copy the semantic slot entry locally, change only its state to `DEAD`, and encode it into a
   fixed 8-byte local array.
6. Rewrite only the target slot entry.

All validation and encoding complete before mutation. Every expected failure is therefore
byte-atomic. The operation performs no heap allocation and no whole-page copy on success. Tests
confirm that the only changed persisted value is the target state code (`1` to `2`).

## Fields intentionally preserved

The transition preserves:

- `PageId` and the target `SlotId`;
- `slot_count`, `free_slot_head`, `lower`, `upper`, `prune_hint`, and heap reserved bytes;
- the complete common page header;
- target `tuple_offset`, `tuple_length`, and `aux`;
- every neighboring slot entry;
- all tuple-region bytes.

No slot is linked into a free list or changed to `UNUSED`.

## Tuple-byte behavior

The old tuple range remains physically present and byte-for-byte unchanged. Marking the slot
`DEAD` does not increase contiguous free space, zero bytes, move tuples, or alter page geometry.
The bytes are only logically reclaimable for a later compaction/reuse milestone after the required
global protocols are implemented.

## TupleBytes behavior after DEAD

Before the transition, `TupleBytes(slot_id)` returns the `NORMAL` tuple payload. After the
transition it returns `std::nullopt`, because the helper is intentionally `NORMAL`-only. Direct
inspection of the persisted `tuple_offset`/`tuple_length` range confirms that the physical bytes
remain intact.

## Stable RID behavior

The original `Rid{Page::Id(), slot_id}` remains the physical identity of the tuple version after
the state transition. The operation does not renumber slots, modify `slot_count`, or invalidate and
replace the RID. Reuse of that physical RID remains prohibited until the later DEAD-to-UNUSED
grace-period protocol.

## Checksum/page-LSN behavior

`MarkDead` does not update CRC32C, `page_lsn`, or any WAL metadata. The existing staged zero
checksum remains unchanged. WAL logging, page-LSN advancement, and checksum refresh are deferred
to recovery integration.

## Tests/checks run

- Focused Clang build and four `HeapPageDeadTransition*` tests: passed.
- Full `clang-debug` build and CTest suite: 100/100 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 100/100 passed with
  `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer was disabled because the execution environment runs
  under ptrace and LSan aborts during GoogleTest discovery. ASan and UBSan remained enabled.
- Focused `gcc-debug` build and four DEAD-transition tests: passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.

## Assumptions

- `ARCHITECTURE.md` remains authoritative under `AGENTS.md`.
- The caller has already established that the tuple is globally reclaimable and that required
  index cleanup precedes `MarkDead`.
- Returning `ALREADY_DEAD` is preferable at this page-layer milestone to silently treating a
  duplicate call as success.
- A `DEAD` slot retains its tuple location and `aux` exactly until a later reclamation operation.

## Known limitations/deferred work

- No SQL DELETE, `xmax`, transaction visibility, vacuum eligibility, index cleanup, retirement
  epoch, or grace-period enforcement.
- No DEAD-to-UNUSED transition, free-list linkage, slot reuse, compaction, tuple-byte zeroing, or
  FSM update.
- No BufferPool integration, page guards, latches, dirty tracking, WAL record, page-LSN update,
  recovery, or checksum refresh.
- `MarkDead` cannot verify the global preconditions described by the vacuum protocol; this is
  intentionally a caller responsibility.

## Architecture questions discovered

No new persisted-format decision was required. One API-level question remains for WAL/recovery:
should replay or retry use a separate idempotent transition path, or should `MarkDead` itself later
treat `DEAD -> DEAD` as success? This milestone deliberately reports `ALREADY_DEAD` and does not
prejudge redo semantics.

