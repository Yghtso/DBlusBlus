# 0011 — Heap Page Raw Insertion

Date: 2026-08-14

## Milestone/task

Phase 1: insert opaque raw tuple bytes into one structurally valid `HEAP_DATA` page, append a
`NORMAL` slot entry, and return the stable physical `Rid`.

## Scope

Added no-reuse slotted-page insertion to the existing lightweight `HeapPage` controller. The
implementation validates the page, checks the v1 inline limit and contiguous free space, stages
the new metadata, copies the opaque bytes into the tuple region, appends one slot entry, updates
the heap header, and returns a `Rid`. Added a read-only, non-owning physical byte lookup for
`NORMAL` slots.

No tuple decoding, tuple-header format, schema layout, MVCC, deletion, state transitions, slot
reuse, compaction, FSM, HeapFile, BufferPool, latching, dirty tracking, WAL, or recovery behavior
was added.

## Files changed

- `src/storage/heap_page.h` — adds the raw inline-size constant, insertion error/result types, and
  insertion and physical byte-view APIs.
- `src/storage/heap_page.cpp` — implements validated, no-reuse raw insertion and `NORMAL` slot byte
  lookup.
- `tests/heap_page_test.cpp` — adds focused insertion, boundary, atomic-failure, corruption,
  retrieval, stable-RID, and persistence tests.
- `devlog/0011-heap-page-raw-insertion.md` — records this task.

No CMake file or older devlog entry was modified.

## Architecture sections used

- §9 and §11 — explicit persisted bytes and slotted heap-page layout
- §44 — explicit runtime/corruption error handling
- §49 — Phase 1 raw-storage implementation order
- §54–§56 — `SlotId`, `PageId`, and physical-version `Rid` semantics
- §61–§63 — common page header, `HEAP_DATA` page type, and staged checksum policy
- §66 — 48-byte heap page header
- §67 — stable slot directory and named slot states
- §68 — lower/upper free-space geometry and slot-cost rule
- §69 — strict v1 inline-tuple size bound
- §78 — conceptual INSERT page-selection/verification/write sequence
- §96–§97 — HeapPage responsibility and caller-owned raw Page bytes

`ARCHITECTURE.md` is the authoritative architecture file used for this task, as specified by
`AGENTS.md`.

## Exact public API introduced

Declared in `storage/heap_page.h`:

```cpp
inline constexpr std::size_t HEAP_PAGE_MAX_RAW_TUPLE_SIZE = 8135;

enum class HeapPageInsertError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    TUPLE_TOO_LARGE,
    INSUFFICIENT_SPACE,
    SLOT_ID_EXHAUSTED,
};

struct HeapPageInsertResult {
    std::optional<Rid> rid;
    HeapPageInsertError error;
    HeapPageValidationError page_error;
    explicit operator bool() const noexcept;
};

class HeapPage {
public:
    HeapPageInsertResult Insert(std::span<const std::byte> tuple) noexcept;
    std::optional<std::span<const std::byte>>
    TupleBytes(SlotId slot_id) const noexcept;
};
```

`page_error` preserves the existing structural validation failure when `error` is
`PAGE_INVALID`. `TupleBytes` returns a view into the underlying `Page`; its lifetime and mutation
validity therefore remain tied to that page.

## Insertion algorithm

1. Run the existing full structural validation and reject invalid page bytes.
2. Reject payloads exceeding the persisted 16-bit length representation or the strict v1 inline
   maximum.
3. Check `slot_count` increment safety.
4. Compute `available = upper - lower` and `required = tuple.size() + 8`.
5. Choose `slot_id = slot_count`, `new_lower = lower + 8`, and
   `new_upper = upper - tuple.size()`.
6. Encode the updated 16-byte heap header and new 8-byte slot into local fixed-size arrays.
7. Copy the tuple bytes to `new_upper`, the slot bytes to the old `lower`, and the header bytes to
   page offset 32.
8. Return `{Page::Id(), slot_id}`.

The insertion path performs no heap allocation and does not copy the whole page on success.

## Free-space calculation

With slot reuse and compaction deferred:

```text
available_contiguous_bytes = upper - lower
required_bytes             = tuple_length + 8
success                     = required_bytes <= available_contiguous_bytes
```

Dead or unused slots are not scanned or counted as reusable. The architecture's strict v1 bound
is implemented literally: `tuple_size < 8192 - 48 - 8`, so the largest accepted raw payload is
8135 bytes. On a blank page that leaves one byte between the new slot directory and tuple region.

## Slot-allocation semantics

Insertion always uses `new_slot_id = slot_count` and then increments the persisted count. Existing
slots are never reused. `free_slot_head` is preserved unchanged and remains `INVALID_SLOT_ID` for
pages produced by the current no-reuse path. Slot-count and persisted-field conversions are
checked before mutation.

## Tuple-placement semantics

Opaque bytes are copied contiguously to `[new_upper, old_upper)`, with no alignment padding. Tuple
storage grows downward while the slot directory grows upward by exactly 8 bytes. Existing tuple
offsets and bytes are not changed.

The new persisted slot contains:

```text
tuple_offset = new_upper
tuple_length = tuple.size()
state        = NORMAL
aux          = 0
```

## Mutation atomicity behavior

Structural validation, all bounds/conversion checks, offset calculations, and both metadata
encodes finish before page bytes are modified. `PAGE_INVALID`, `TUPLE_TOO_LARGE`,
`INSUFFICIENT_SPACE`, and representation-exhaustion failures therefore leave every page byte
unchanged. Once mutation starts, only infallible in-memory byte copies remain.

## Returned RID semantics

Successful insertion returns:

```text
Rid.page = Page::Id()
Rid.slot = previous slot_count
```

Later movement of tuple bytes may change a slot's `tuple_offset`, but this insertion path does not
move prior tuples or alter their `SlotId` values.

## Raw tuple retrieval API

`TupleBytes(SlotId)` validates the whole page, checks that the slot is in range, decodes the slot
explicitly, and returns a `std::span<const std::byte>` only for `NORMAL`. Invalid, corrupt,
out-of-range, and non-`NORMAL` slots return `std::nullopt`. The operation allocates and copies
nothing and performs no visibility or schema checks.

## Zero-length tuple behavior

The raw structural layer accepts a zero-length opaque payload because the architecture does not
forbid it at this layer. It still consumes one 8-byte slot, receives a stable `Rid`, stores
`tuple_length = 0`, and leaves `upper` unchanged. A later tuple codec may impose semantic minimums.

## Aux-field behavior

New `NORMAL` slots persist `aux = 0` deterministically. This milestone does not interpret `aux` or
use it for free-list linkage or redirect behavior.

## Checksum behavior

Insertion does not generate or update the page checksum. A blank heap page's staged
`checksum_crc32c = 0` remains zero after insertion. WAL-era checksum and page-LSN update policy is
deferred.

## Tests/checks run

- Focused Clang build and seven `HeapPageInsertionTest|HeapPageIntegrationTest` tests: passed.
- Full `clang-debug` build and CTest suite: 96/96 passed.
- Full `clang-asan` ASan+UBSan build and CTest suite: 96/96 passed with
  `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer was disabled because the execution environment runs
  under ptrace and LSan aborts before GoogleTest discovery. ASan and UBSan remained enabled.
- Focused `gcc-debug` build and seven insertion/integration tests: passed.
- `clang-tidy` preset build for production and tests: passed without diagnostics after test
  optional-access guards were made explicit.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.
- Final diff and Git status inspected.

## Assumptions

- `ARCHITECTURE.md` remains authoritative under `AGENTS.md`.
- Input bytes are one complete opaque physical payload; this layer does not decide whether that
  payload is a semantically valid database tuple.
- The locked strict less-than expression in §69 is intentional, so 8136 bytes is rejected even
  though it would exactly meet the blank-page slot and tuple regions.
- A returned `TupleBytes` span is invalidated by later mutation that changes or moves that slot's
  bytes; no ownership is transferred.

## Known limitations/deferred work

- No slot reuse, free-list linkage, deletion, state transition, or compaction.
- No tuple header/schema encoding, tuple decoding, MVCC visibility, transaction behavior, FSM, or
  HeapFile logic.
- No BufferPool frame state, latching, dirty tracking, page-LSN update, WAL, recovery, or checksum
  refresh.
- `SLOT_ID_EXHAUSTED` is retained as an explicit representation guard, although the current 8 KiB
  geometry makes an otherwise valid page reach a space/geometry limit first.
- Retrieval is deliberately physical and `NORMAL`-only.

## Architecture questions discovered

1. Should `aux = 0` for every newly inserted `NORMAL` slot be locked as a persisted v1
   convention? The implementation now relies on it for deterministic insertion.
2. Should the strict `<` in §69 be confirmed, or should a tuple that exactly consumes the blank
   page's tuple-plus-slot capacity (8136 payload bytes) be allowed? This implementation follows the
   current locked strict inequality and accepts at most 8135 bytes.
3. Should the future tuple codec reject zero-length physical payloads? The raw page layer currently
   accepts them because no such restriction is locked here.
