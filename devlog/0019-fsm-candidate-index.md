# 0019 — In-Memory FSM Candidate Index

- Date: 2026-08-14
- Milestone: Phase 1, in-memory Free-Space Map candidate index

## Scope

Implemented a purely in-memory, relation-local accelerator that tracks caller-supplied heap
`PageNo`/FSM-category metadata and returns advisory insertion candidates. The implementation has no
storage I/O, does not inspect heap pages, does not own persisted FSM pages, and does not introduce
`HeapFile`, `BufferPool`, buffer frames, page guards, CLOCK, dirty flushing, latches, WAL, recovery,
or any other Phase 2 type. The milestone 0018 FSM_DATA persisted page format is unchanged.

## Files changed

- `src/storage/fsm_candidate_index.h`
- `src/storage/fsm_candidate_index.cpp`
- `src/storage/fsm_page.h`
- `src/storage/fsm_page.cpp`
- `src/CMakeLists.txt`
- `tests/fsm_candidate_index_test.cpp`
- `tests/CMakeLists.txt`
- `devlog/0019-fsm-candidate-index.md`

No older devlog entry or `ARCHITECTURE.md` was modified.

## Architecture sections used

- §49, Suggested Implementation Order, especially the Phase 1/Phase 2 boundary
- §78, INSERT Path
- §82, Free-Space Map
- §82.1, v1 free-space category semantics
- §82.2, Category lower-bound interpretation
- §82.7, In-memory accelerator
- §82.8, FSM is advisory
- §83, FSM Persistence Semantics
- §96, Storage Object Boundaries

## Public API introduced

In `storage/fsm_page.h`:

```cpp
enum class FsmTupleRequestError : std::uint8_t {
    NONE,
    TUPLE_TOO_LARGE,
};

struct FsmMinimumCategoryResult {
    std::optional<std::uint8_t> category;
    FsmTupleRequestError error;
    explicit operator bool() const noexcept;
};

FsmMinimumCategoryResult
MinimumFsmCategoryForTupleBytes(std::size_t required_tuple_bytes) noexcept;
```

In `storage/fsm_candidate_index.h`:

```cpp
enum class FsmCandidateIndexError : std::uint8_t {
    NONE,
    INVALID_PAGE_NUMBER,
    TUPLE_REQUEST_TOO_LARGE,
};

struct FsmCandidateResult {
    std::optional<PageNo> page_no;
    FsmCandidateIndexError error;
};

class FsmCandidateIndex {
  public:
    FsmCandidateIndexError Upsert(PageNo page_no, std::uint8_t category);
    FsmCandidateIndexError Remove(PageNo page_no);
    FsmCandidateResult FindCandidate(std::size_t required_tuple_bytes) const noexcept;
    void Clear() noexcept;
    std::size_t Size() const noexcept;
};
```

No generic result framework was added.

## Runtime representation and invariants

The index uses:

```text
std::array<std::set<PageNo>, 256>       ordered category buckets
std::unordered_map<PageNo, uint8_t>     reverse page-to-category membership
```

The ordered sets make the smallest `PageNo` directly available in each category. The reverse map
allows category changes and removals without scanning 256 buckets. Both structures are rebuildable
runtime metadata and have no persisted representation.

Maintained membership invariants are:

1. every tracked page is an ordinary heap page (`PageNo != 0` and
   `PageNo != INVALID_PAGE_NO`),
2. each tracked page has exactly one category,
3. each reverse-map entry has one matching bucket member,
4. no bucket contains duplicate pages, and
5. `Size()` equals the number of unique reverse-map entries.

Targeted debug assertions check bucket/reverse-map agreement during same-category updates,
category migration, and removal. New-page insertion rolls back reverse membership if bucket
allocation throws, so allocation failure does not leave split membership.

## Tuple request to minimum category

The helper reuses milestone 0018's locked category lower bound:

```text
minimum_usable(c) = ceil(c * 8135 / 255)
```

For raw tuple request `R`, the smallest category whose represented lower bound is at least `R` is:

```text
C(0) = 0

C(R) = floor((R - 1) * 255 / 8135) + 1, for 1 <= R <= 8135
```

The calculation is allocation-free, integer-only, deterministic, and centralized in
`MinimumFsmCategoryForTupleBytes`; the candidate index does not duplicate the category
mathematics. The exhaustive test confirms for every request `0..8135` that
`minimum_usable(C(R)) >= R` and, when `C(R) > 0`, the immediately preceding category's lower bound
is less than `R`.

The request is the raw encoded tuple byte count. The existing persisted category semantics already
account for the mandatory 8-byte append-slot cost when converting physical heap free gap to usable
insertion bytes.

## Candidate policy

`FindCandidate` probes categories in ascending order from the minimum sufficient category through
255. It returns from the first nonempty bucket, producing the initial best-sufficient-category
policy. Within that bucket it returns the smallest `PageNo`.

The result remains advisory: it means only that currently known FSM metadata makes the page worth
checking. A future insertion owner must fetch the actual heap page and verify current free space.

A successful result with an empty `page_no` is the normal "no known candidate" outcome and is not
an error. Requests greater than 8135 return an empty candidate with
`TUPLE_REQUEST_TOO_LARGE`; values are not clamped. Zero-byte requests are valid and begin searching
at category 0.

## Mutation and rebuild behavior

- `Upsert` rejects page 0 and `INVALID_PAGE_NO`. A new page is inserted once; a same-category update
  is a no-op; a changed category moves the page directly from its old bucket to the new bucket while
  preserving `Size()`.
- `Remove` rejects page 0 and `INVALID_PAGE_NO`. Removing a valid untracked page is an idempotent
  successful no-op. Removing a tracked page erases only that page's bucket and reverse memberships.
- `Clear` removes every bucket and reverse-map entry. A future owner can rebuild by repeatedly
  calling `Upsert` with metadata it reads elsewhere.
- Stale-metadata repair is caller-driven: after a future caller checks the real page, it can call
  `Upsert(page_no, corrected_category)`. Candidate results reflect downward or upward repair
  immediately. The index never computes a category from `HeapPage` itself.

## Thread safety and complexity

`FsmCandidateIndex` is internally unsynchronized and is not thread-safe.

- `Upsert`: average O(1) reverse lookup plus O(log pages-in-bucket) ordered-set work
- `Remove`: average O(1) reverse lookup plus O(log pages-in-bucket) erase
- `FindCandidate`: at most 256 constant category probes plus O(1) access to the selected set's first
  element
- `Clear`: O(256 + tracked pages)
- `Size`: O(1)

The object performs no storage I/O. Runtime tracking allocates STL nodes as pages are tracked, but
candidate lookup and the category calculation allocate nothing.

## Tests and checks run

- Focused Clang debug candidate-index and existing FSM tests: 22/22 passed.
- Full Clang debug suite: 183/183 passed.
- Full Clang ASan/UBSan suite: 183/183 passed with `ASAN_OPTIONS=detect_leaks=0`. The first sanitized
  test-discovery attempt was blocked before test execution because LeakSanitizer cannot operate
  under the sandbox's ptrace environment; address and undefined-behavior instrumentation remained
  enabled for the successful full run.
- Focused GCC debug candidate-index and existing FSM tests: 22/22 passed.
- `clang-tidy` preset build: passed without diagnostics after the final rerun. The fixed-seed RNG
  warning in the reproducible property test is locally documented because a fixed seed is an
  explicit test requirement.
- `clang-format --dry-run --Werror` on all changed C++ files: passed.
- `git diff --check`: passed.

Focused coverage includes initial/empty state, invalid page numbers, idempotent same-category
upsert, upward/downward category migration, multiple buckets, duplicate prevention through public
behavior, deterministic smallest-page selection, best sufficient category, higher-category
fallback, no-candidate behavior, request boundaries, exhaustive minimum-category mathematics over
`0..8135`, category repair, idempotent removal, clear/rebuild, and a fixed-seed 20,000-operation
randomized comparison against a simple reference model.

No benchmark was run because this milestone did not make a benchmark-dependent optimization.

## Assumptions

- One index instance belongs to one heap relation; relation identity is owned by a future outer
  layer and is not repeated per entry.
- Callers supply FSM category metadata. This object neither reads persisted FSM_DATA pages nor
  validates categories against live heap pages.
- A valid but absent candidate is expected and allows a future owner to allocate, retry, or repair
  metadata.
- The locked milestone 0018 lower-bound interpretation is the conservative basis for request
  filtering.

## Known limitations and deferred work

- No relation-wide `FreeSpaceMap` file owner or `HeapFile` exists yet.
- No FSM_DATA loading/writing, candidate-index persistence, heap-page allocation, automatic repair,
  or entry-count synchronization is implemented.
- No actual heap-page fetch or free-space verification is performed.
- No concurrency control, mutex, latch, or atomic maintenance policy is implemented.
- No BufferPool, buffer frame, page guard, CLOCK, dirty flushing, WAL, recovery, MVCC, transaction,
  or B+ tree work was introduced.
- The simple scan of up to 256 buckets is intentionally retained; no nonempty-bucket bitmap or tree
  was added without benchmark evidence.

## Architecture questions discovered

None. The tuple-request inverse is a direct runtime derivation of the locked milestone 0018 category
lower-bound semantics. The bucket search order and smallest-page tie-break are rebuildable runtime
policies, not persisted-format changes. The FSM_DATA format, version, headers, entry mapping, and
category bytes remain unchanged.
