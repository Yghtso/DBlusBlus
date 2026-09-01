# Chapter-24 semantic integration verdict

**D24-S1–D24-S6 — SUCCESSFULLY INTEGRATED.**

Chapter 24 is now **SEMANTICALLY CLEAN**, but remains **NOT DOCUMENT-CLEAN** and **NOT FULLY CLOSED** pending N24-1/N24-2 cleanup and Verification synchronization.

Modified only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19664).

## Repository state

- Initial branch: `main`
- Initial HEAD: `f7fa1d1ef56d4506e09ab2eceaefbc05c20502b5`
- Initial working tree: clean
- Initial index: clean
- Initial Architecture diff: none
- Final working tree: `M docs/ARCHITECTURE.md`
- Final index: clean
- Final HEAD: unchanged
- Final diff: 326 insertions, 38 deletions
- `git diff --check`: passed
- No external repository changes were observed during the task.
- Historical review artifacts were unread, unmodified, and unstaged.

## Architecture sections modified

- [§24.1 Execution RowLayout](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19666)
- §§24.2–24.8
- §§24.10–24.11
- [§39.3 Execution errors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27009)

§24.9 was not semantically changed. Its protected “initial operator-level target” wording remains. Chapters 17, 20–23, 25–26 and §39.1 are unchanged.

## D24-S1 — Complete conservative accounting

The final model now establishes:

- Every potentially unbounded query-owned process-memory region has one conceptual accounted owner in every applicable ledger.
- Accounting measures implementation-controlled committed capacity, not exact RSS.
- Conservative over-accounting is allowed; systematic under-accounting is forbidden.
- Many individually small allocations and dynamically growing metadata cannot bypass accounting.
- QueryArena backing capacity is accounted and cannot hold unbounded row-, group-, run-, partition-, or result-dependent state.
- Parent pages, blocks, arenas, or collections may account for contained objects without duplicate per-object charging.
- Query-owned chunks, vectors, StringHeaps, selections, retained rows, varlen storage, metadata directories, operator state, spill buffers, reload blocks, spools, and worker-local dynamic state are covered where applicable.
- BufferPool capacity remains under its separate bounded owner.
- Query/global hard-gate checks and updates form one logically atomic transition; stale concurrent checks cannot oversubscribe either gate.

**D24-S1: CLOSED.**

## D24-S2 — Continuously accounted ownership

The Architecture now distinguishes:

1. request;
2. accounting grant;
3. physical allocation;
4. live query-owned memory.

The central invariant is that capacity cannot become live query-owned state before sufficient accounting exists.

Additional rules include:

- Allocate-first remains possible only for private, bounded/already-covered provisional storage.
- Allocator rounding and in-place capacity growth must be fully charged before exposure.
- Catchable allocation denial for an exact supported representable form becomes `OutOfMemory`.
- A failed post-grant allocation releases the unused grant and exposes no partial state.
- Runtime size/address unrepresentability is not OOM.
- Ownership transfers preserve continuous accounting, allowing only atomic handoff or conservative overlap.
- Release occurs exactly once.
- Double release, underflow, and release of unowned capacity are internal invariant violations.
- Non-catchable OS/process termination is outside the controlled-return guarantee.

This removes the former “where possible” ambiguity.

**D24-S2: CLOSED. M24-5: CLOSED.**

## D24-S3 — Finite pressure progress

A denied hard-gate request may be retried only after relevant progress, such as:

- releasing accounted capacity;
- reducing the requested charge;
- freeing an eligible owner;
- adopting an exact lower-memory representation;
- advancing a well-founded finite spill/repartition state.

Repeating an equivalent denied request, cycling equivalent states, or spilling without freeing useful capacity is not progress.

When no exact progress-producing action remains:

- terminal result: `OutOfMemory`;
- actual spill I/O, creation, capacity, or `ENOSPC` failure: `SpillIOError`;
- cancellation: `QueryCancelled`.

No fixed retry count or runtime optimizer re-entry was introduced. Pressure handling must preserve values, NULLs, multiplicity, required order, demanded errors, and transaction behavior.

**D24-S3: CLOSED.**

## D24-S4 — Exact retained-row applicability

The ordinary 256 KiB target, `RowLayout`, block form, and descriptor domains are now explicitly runtime representations—not SQL, `VARCHAR`, cardinality, heap-tuple, or correctness limits.

An oversized row/value may use:

- dedicated oversized allocation;
- alternate exact layout;
- exact physical segmentation;
- operator-specific exact retained form;
- another exact representation.

Physical segmentation cannot semantically divide a row occurrence or scalar value. Truncation, clipping, wrapping, and semantic splitting are forbidden.

Failure classification:

- no supported exact retained form: `ExecutionError` with representability/resource cause;
- supported exact form but allocation unavailable: `OutOfMemory`.

The persistent heap-tuple maximum is not generalized. A D23-S3-compatible large runtime `VARCHAR` still requires a separately exact retained representation.

**D24-S4: CLOSED.**

## D24-S5 — Exact extents and safe spill validation

Every Chapter-24 size, count, capacity, offset, reservation total, extent, and count-derived byte calculation is now an exact mathematical nonnegative value.

The rule explicitly covers:

- `count * width`;
- fixed plus varlen bytes;
- `offset + length`;
- row/block extents;
- accounting additions and subtractions;
- block/run/partition growth;
- spill payload and record lengths;
- file offset plus I/O length;
- allocation extents;
- capacity rounding.

Forbidden behavior includes signed overflow, unsigned wrap, narrowing, modulo reduction, pointer-range overflow, and file-offset truncation.

Spill metadata must be validated before controlling allocation, pointer arithmetic, dereference, range construction, or I/O. Applicable framing, version, lengths, counts, ranges, CRC, and owner structure are checked in a safe dependency order.

Classifications:

- unsupported in-memory size/address domain: `ExecutionError`;
- supported exact allocation denied: `OutOfMemory`;
- unsupported spill range/file-offset domain: `SpillIOError`;
- malformed temporary spill: `SpillIOError`, not persistent corruption;
- self-generated in-memory construction violation: internal invariant defect.

Self-generated spill remains validated against partial I/O, crashes, stale files, disk faults, external modification, and defects.

**D24-S5: CLOSED.**

## D24-S6 — Spill namespace and crash leftovers

Every live spill resource now requires fresh, exclusive temporary ownership that isolates:

- queries;
- statement attempts;
- processes/database instances sharing temporary storage;
- stale crash leftovers.

A new owner cannot adopt, trust, or blindly overwrite an existing object because a path or identifier collides. It must establish another fresh identity or return `SpillIOError`.

Normal teardown deletes owned spill resources after success, ordinary errors, resource errors, spill errors, cancellation, and abandoned-attempt retry cleanup. Retried attempts receive fresh ownership.

Crash leftovers are garbage only:

- not WAL logged;
- not recovered;
- not database state;
- not new-query state;
- no fsync or durable spill identity required.

During healthy managed-temp operation, proven stale managed spill must eventually be reclaimed. Synchronous cleanup on every database open is not required. Only files proven managed and without a live owner may be deleted; unrelated and live-owned files remain protected.

Filename syntax, namespace encoding, directory layout, identifiers, and reclamation schedule remain implementation-defined.

**D24-S6: CLOSED.**

## Final resource/error taxonomy

| Condition | Classification |
|---|---|
| Hard-gate denial with no exact progress path | `OutOfMemory` |
| Catchable allocation denial for supported exact representable form | `OutOfMemory` |
| Unsupported in-memory size/address extent | `ExecutionError` — representability/resource |
| No exact retained-row/value form | `ExecutionError` — representability/resource |
| Spill read/write or creation failure | `SpillIOError` |
| Temp capacity exhaustion / `ENOSPC` | `SpillIOError` |
| Spill range/file-offset/addressability failure | `SpillIOError` |
| Malformed spill framing/CRC/count/offset/range | `SpillIOError` |
| Cancellation | `QueryCancelled` |
| Reservation double release/underflow | Internal invariant violation |
| Blind stale-spill adoption/overwrite | Internal invariant violation |
| Non-catchable OS/process termination | Outside controlled-return guarantee |

§39.1 remains the sole transaction-consequence owner.

## Decision composition

- D24-S1 defines what is accounted; D24-S2 defines continuous coverage.
- D24-S5 protects D24-S1 gate arithmetic and D24-S2 lifecycle arithmetic.
- D24-S3 begins from a coherent D24-S2 denial and leaves no provisional charge behind.
- D24-S6 governs spill resources created during D24-S3 pressure handling.
- D24-S4 uses D24-S5 exact row/descriptor calculations.
- D24-S4 composes with D23-S3 without assuming that runtime scalar representability implies retained-row representability.

## Regression assessment

- Chapter 17 scalar and arbitrary-byte `VARCHAR` semantics: unchanged.
- Chapter 20 bag, ordering, and demanded-evaluation semantics: unchanged.
- Chapter 21 attempt, retry, publication, and error consequences: unchanged.
- Chapter 22 execution-context ownership and capability rules: unchanged.
- Chapter 23 borrowing, `StringRef`, exact runtime representation, and persistence boundary: unchanged.
- Chapters 25 and 26: unchanged.
- Transaction and persistent-format semantics: unchanged.
- No SQL row/value maximum, row identity, persistent FileId, runtime reoptimizer, concrete allocator, or concrete integer width was introduced.
- New wording is timeless. Existing N24-1 chronology wording was deliberately preserved.

## Reread questions 1–155

- Questions 1–18: **YES**.
- Questions 19–34: **YES**.
- Questions 35–45: **YES**.
- Questions 46–61: **YES**.
- Questions 62–88: **YES**.
- Questions 89–110: **YES**.
- Questions 111–134: **YES**.
- Question 135, new semantic question: **NO**.
- Questions 136–149: **YES**.
- Question 150, N24-1 remains open: **YES**.
- Question 151, N24-2 remains open: **YES**.
- Question 152, Chapter 24 document-clean: **NO**.
- Question 153, Verification synchronized: **NO**.
- Question 154, Chapter 24 fully closed: **NO**.
- Question 155, Chapter 25 reviewed: **NO**.

No new frozen cross-owner semantic conflict was found.

## Closure status

- D24-S1 through D24-S6: **CLOSED**
- Q24-1 through Q24-6: **CLOSED**
- B24-1 and B24-2: **CLOSED**
- M24-1 through M24-5: **CLOSED**
- Frozen Chapter-24 semantic questions: **NONE**
- N24-1: **OPEN / DOCUMENT-ONLY**
- N24-2: **OPEN / DOCUMENT-ONLY**
- Chapter-24 Architecture: **SEMANTICALLY CLEAN**
- Chapter 24: **NOT YET DOCUMENT-CLEAN / NOT FULLY CLOSED**
- Chapter-24 Verification: **NOT SYNCHRONIZED BY THIS TASK**
- Chapter-25 review: **NOT STARTED**

The task-created hunks cover classes A–AC: accounting universe and granularity; metadata/QueryArena bypass; hard-gate atomicity; lifecycle, rounding, failure, transfer, and release; finite progress; retained-row applicability; exact arithmetic and spill validation; namespace ownership and reclamation; §39.3 taxonomy; cross-owner navigation, rationale, and necessary Markdown wrapping.

No build, test, sanitizer, benchmark, implementation, staging, commit, devlog, or review artifact occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.

Recommended next task: **TARGETED CHAPTER-24 DOCUMENT-ONLY CLEANUP** for N24-1 and N24-2.
