# 0013 — Heap Page Compaction

Date: 2026-08-14

## Milestone/task

Phase 1: page-local physical compaction of tuple bytes belonging to persistent `DEAD` heap slots,
without making those slots reusable.

## Scope

Added one `HeapPage::Compact` operation that validates and plans the complete page-local rewrite,
discards DEAD payload ranges from the logical packed tuple region, repacks all `NORMAL` payloads
toward `PAGE_SIZE`, updates their persisted offsets, clears reclaimed DEAD coordinates, and moves
`upper` to the beginning of the new packed region.

No DEAD-to-UNUSED transition, slot reuse, free-list linkage, slot-directory shrinking, visibility
decision, vacuum horizon, index cleanup, grace-period handling, FSM, HeapFile, tuple codec,
BufferPool, WAL, or recovery behavior was added.

## Files changed

- `src/storage/heap_page.h` — adds compaction error/result types and `HeapPage::Compact`.
- `src/storage/heap_page.cpp` — implements validation, overlap detection, fixed-size relocation
  planning, safe byte movement, slot rewrites, and geometry update.
- `tests/heap_page_test.cpp` — adds focused compaction, corruption, idempotence, stable-RID,
  post-compaction insertion, and reopen-persistence tests.
- `devlog/0013-heap-page-compaction.md` — records this task.

No CMake file or older devlog entry was modified.

## Architecture sections used

- §49 — Phase 1 raw-storage implementation order
- §54–§56 — stable `SlotId`, `PageId`, and physical-version `Rid` semantics
- §63 — staged page-checksum policy
- §67 — explicit slot states, tuple coordinates, `aux`, and stable slots
- §68 — lower/upper free-space geometry
- §80–§81 — logical DELETE and page-layer visibility boundaries
- §84 — physical heap-page compaction
- §85 — vacuum physical-reclamation sequence
- §96–§97 — HeapPage responsibility and conceptual `Compact` API
- §260–§264 — delayed RID reuse, persistent DEAD state, and index-cleanup-before-DEAD ordering

`ARCHITECTURE.md` is the authoritative architecture contract used for this task.

## Exact public API introduced

Declared in `storage/heap_page.h`:

```cpp
enum class HeapPageCompactError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    UNSUPPORTED_SLOT_STATE,
    TUPLE_RANGE_OUT_OF_BOUNDS,
    OVERLAPPING_TUPLE_RANGES,
};

struct HeapPageCompactResult {
    HeapPageCompactError error;
    HeapPageValidationError page_error;
    SlotId slot_id;
    SlotId other_slot_id;
    explicit operator bool() const noexcept;
};

class HeapPage {
public:
    HeapPageCompactResult Compact() noexcept;
};
```

`page_error` preserves the existing structural-validation category for `PAGE_INVALID`.
`slot_id` identifies a slot responsible for a compact-specific failure; `other_slot_id` identifies
the other range for a detected overlap.

## Compaction algorithm

1. Run the existing complete heap-page structural validation.
2. Decode every slot into fixed-size stack arrays.
3. Accept `NORMAL` and `DEAD`; reject `UNUSED` and `REDIRECT_RESERVED` because their byte semantics
   remain undefined.
4. Validate positive-length DEAD ranges against the current tuple-data region and page bounds.
5. Sort all positive-length NORMAL/DEAD ranges by source offset and reject any overlap.
6. Sort NORMAL slots by descending original tuple offset, with ascending `SlotId` as the tie-break.
7. Starting at `PAGE_SIZE`, subtract each NORMAL length and assign its planned destination.
8. Set each planned DEAD entry to `tuple_offset = 0`, `tuple_length = 0`, preserving state and
   `aux`.
9. Encode every planned slot and the updated heap header into fixed-size stack arrays.
10. Move NORMAL bytes in the planned descending-source order using `std::memmove`.
11. Rewrite the slot entries and heap-specific header.

No expected failure remains after step 9. The normal path uses no dynamic allocation and no full
8 KiB page scratch copy. At the v1 geometry maximum, the fixed arrays use approximately 24 KiB of
stack storage.

## Deterministic packing order

NORMAL payloads are packed in descending original `tuple_offset` order. Slots with equal offsets,
which can occur for zero-length payloads, use ascending `SlotId` as a deterministic tie-break.

This preserves the pre-compaction relative physical order of positive-length live payloads. The
highest original live range remains nearest `PAGE_SIZE`; lower original ranges follow below it.
SQL order is not inferred from this physical order.

Descending source order also makes in-place movement safe after overlap validation: each retained
payload moves only toward a same-or-higher address, and a move cannot overwrite the unread source
of a lower range. `std::memmove` handles overlap between each individual source and destination.

## Mutation/failure atomicity

Structural validation, state checks, DEAD range checks, overlap detection, destination arithmetic,
and all slot/header encodes complete before any page byte changes. `PAGE_INVALID`,
`UNSUPPORTED_SLOT_STATE`, `TUPLE_RANGE_OUT_OF_BOUNDS`, and `OVERLAPPING_TUPLE_RANGES` therefore
leave every page byte unchanged.

Once mutation starts, only in-memory byte moves and copies remain. This is page-operation
atomicity for ordinary expected failures; crash atomicity and concurrent access still require the
future latch/WAL integration required by the architecture.

## Overlap/corruption handling

`Compact` locally checks positive-length ranges from both NORMAL and DEAD slots. It rejects any
pair whose byte intervals overlap, including NORMAL/NORMAL, NORMAL/DEAD, and DEAD/DEAD overlap.
Positive-length DEAD ranges must begin at or above the current `upper` and end at or before
`PAGE_SIZE`.

Zero-length ranges own no bytes and therefore do not overlap positive ranges. Existing structural
validation still handles malformed NORMAL ranges and invalid persisted state codes before the
compact-specific checks run.

Overlap validation remains inside `Compact` rather than expanding general `HeapPage::Validate`,
because this milestone only requires the stronger invariant before physical relocation and the
architecture has not yet locked a broader validation policy for all non-NORMAL states.

## DEAD slot metadata after reclamation

After successful compaction, every DEAD slot persists:

```text
tuple_offset = 0
tuple_length = 0
state        = DEAD
aux          = preserved original value
```

This avoids leaving coordinates that could point to free space or unrelated moved tuple bytes.
It is a new persisted-format convention not currently locked by `ARCHITECTURE.md` and requires
architecture synchronization. It does not make the slot UNUSED or reusable and does not create
free-list linkage.

## Free-space effect

Compaction computes:

```text
new_upper = PAGE_SIZE - sum(NORMAL tuple_length)
contiguous_free_bytes = new_upper - lower
```

`lower`, `slot_count`, and the slot-directory positions remain unchanged. In pages produced by the
current insertion path, `upper` increases by the sum of discarded positive DEAD lengths. Any
pre-existing holes in an otherwise valid non-overlapping layout are also closed.

The old source bytes are not explicitly zeroed. Bytes in the contiguous free region
`[lower, new_upper)` may contain stale remnants until overwritten; no slot references those
remnants.

## Stable RID guarantees

Compaction never reorders, removes, or renumbers slot entries. Every NORMAL tuple retains its
original `SlotId` and therefore its original `Rid{Page::Id(), SlotId}`. Only its persisted
`tuple_offset` may change. Tests verify the original SlotIds retrieve byte-identical payloads after
compaction.

DEAD slots retain their SlotIds and persistent DEAD state. They are not candidates for the existing
append-only insertion path, which continues allocating `slot_id = slot_count`.

## All-DEAD behavior

When all slots are DEAD:

```text
upper          = PAGE_SIZE
slot_count     = unchanged
lower          = 48 + slot_count * 8
free_slot_head = unchanged
every state    = DEAD
every DEAD coordinate = {0, 0}
```

The page remains a nonblank HEAP_DATA page with its complete slot directory and validates under the
current structural rules.

## Idempotence behavior

`Compact` is idempotent success. Running it on an already compacted page returns success and
produces byte-for-byte identical page contents. Cleared DEAD coordinates, deterministic live order,
and canonical packed geometry make the second relocation plan identical to the persisted layout.

## Checksum/page-LSN behavior

Compaction does not update CRC32C, `page_lsn`, or any WAL metadata. The complete common header is
preserved. Checksum refresh, page-LSN advancement, latching, and crash-safe logging remain deferred
to BufferPool/WAL integration.

## Tests/checks run

- Focused Clang build and seven `HeapPageCompaction*` tests: passed.
- Full `clang-debug` build and CTest suite: 107/107 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 107/107 passed with
  `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer was disabled because the execution environment runs
  under ptrace and LSan aborts during GoogleTest discovery. ASan and UBSan remained enabled.
- Focused `gcc-debug` build and seven compaction tests: passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final Git status and diff inspected.

## Assumptions

- Every slot already marked DEAD was made globally reclaimable by a higher layer after required
  index cleanup; `Compact` does not verify that global precondition.
- A positive-length DEAD entry still describes its pre-compaction physical payload until
  compaction clears its coordinates.
- Zero-length NORMAL tuples remain valid and addressable under the existing raw-page convention.
- It is acceptable for unreferenced free-space bytes to retain stale data because tuple access is
  governed exclusively by validated NORMAL slots.

## Known limitations/deferred work

- No DEAD-to-UNUSED transition, retirement epoch, grace-period check, slot reuse, free-list
  linkage, slot-directory compaction, or `slot_count` reduction.
- No SQL DELETE, MVCC visibility, vacuum eligibility, index cleanup, FSM update, HeapFile, tuple
  schema/header behavior, or logical row operation.
- No BufferPool page latch, dirty state, page guard, WAL record, recovery behavior, page-LSN update,
  or checksum refresh.
- `UNUSED` and `REDIRECT_RESERVED` pages are rejected until their physical-byte semantics are
  defined.
- Free-space remnants are not zeroed.

## Architecture questions discovered

1. Should heap-page v1 lock `{tuple_offset = 0, tuple_length = 0}` as the canonical metadata for a
   compacted DEAD slot while requiring its `aux` value to be preserved? This implementation makes
   that persisted choice.
2. Should non-overlapping NORMAL tuple ranges become a general `HeapPage::Validate` invariant, or
   remain an operation-specific requirement until broader page-format validation is specified?
   This milestone keeps the check local to `Compact`.
