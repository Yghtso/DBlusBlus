• Resolved M-001 in docs/ARCHITECTURE.md:3055.

1. Chosen frame-state model

Exclusive ownership states:

- FREE
- LOADING
- RESIDENT
- EVICTING

Orthogonal I/O substates:

- NONE
- READ_IN_PROGRESS
- WRITEBACK_IN_PROGRESS

FLUSHING is defined as RESIDENT + WRITEBACK_IN_PROGRESS. Eviction writeback uses EVICTING + WRITEBACK_IN_PROGRESS.

2. State-transition summary

Current state Event Result
━━━━━━━━━━━━━━━━━━━ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
FREE Existing-page load binds frame LOADING + READ_IN_PROGRESS
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
FREE New-page publication binds frame LOADING + NONE
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
LOADING Read/construction and validation succeed RESIDENT, pending claims become pins
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
LOADING Load or validation fails Mapping removed, frame FREE, all waiters receive same error
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
RESIDENT Persistent mutation publishes Generation increments; dirty/recLSN/DPT state published
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
RESIDENT Copied flush begins RESIDENT + WRITEBACK_IN_PROGRESS
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Flushing resident Stable write succeeds, generation unchanged RESIDENT, clean, recLSN cleared
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Flushing resident Stable write succeeds, generation changed RESIDENT, remains dirty
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Flushing resident WAL/write/sync failure RESIDENT, remains dirty and retryable
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
RESIDENT Victim reservation wins EVICTING, old mapping becomes non-pinnable
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
EVICTING Clean eviction succeeds FREE or reserved handoff to requester’s LOADING
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Dirty EVICTING Stable writeback succeeds Old mapping removed; frame reset and freed/rebound
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Dirty EVICTING Writeback fails Old RESIDENT mapping restored; requesting load fails
─────────────────── ───────────────────────────────────────────── ─────────────────────────────────────────────────────────────
Drained resident Authorized file retirement Mapping/DPT removed; frame reset

3. One-loader/many-waiter rule

The page table now has explicit logical entries:

LOAD_INTENT
LOADING
RESIDENT
EVICTING

The first miss atomically installs the sole load intent. Concurrent fetches register pending claims and wait; they cannot start duplicate reads.

4. Page-table publication

A logical load intent exists before frame selection. PageId -> FrameId becomes visible as LOADING before disk I/O, but is unusable.

Successful LOADING -> RESIDENT publication atomically:

- publishes validated bytes,
- assigns one pin to each pending claim,
- wakes waiters.

5. Pin semantics

A pin is one caller lifetime claim on the current frame/PageId binding. It prevents reassignment but is not a latch.

Pin counts cannot wrap. Load claims that could exceed the representable pin count are rejected before joining.

6. Latch semantics

- Read guard: pin plus shared latch.
- Write guard: pin plus exclusive latch.
- Acquisition order: pin, then latch.
- Release order: latch, then pin.

Transaction locks remain separate and cannot be awaited while holding short-lived page latches.

7. Guard lifetime

Guards are single-owner and movable; moved-from guards are inert. Explicit release is allowed.

Tuple, VARCHAR, page-controller, span, pointer, and iterator views become invalid when their guard releases, even if another caller still pins the frame.

8. Load validation

Before ordinary resident publication:

1. Exact 8192-byte read succeeds.
2. Checksum and universal common-header validation succeed.
3. embedded PageNo and registered FileId identity agree.
4. The registered storage owner’s bounded, nonmutating page-format validator succeeds.

The format validator cannot recursively fetch pages or require catalog/query interpretation while loading. Recovery has an explicit private reconstruction
path, but reconstructed pages must validate before ordinary publication.

9. Dirty transition

A write guard is not automatically dirty merely because it was acquired.

Persistent mutation publication now requires matching:

- final page bytes,
- owning page_lsn,
- incremented modification generation,
- dirty state,
- full-image rec_lsn,
- DPT membership,
- checkpoint/FPI metadata.

The frame/DPT transition reservation begins before the FPI decision and remains through publication.

10. Flush concurrency model

V1 canonically uses copied writeback:

- copy under the read latch,
- release the latch,
- finalize checksum and perform I/O from the private copy,
- allow later readers/writers on the resident frame,
- reconcile using PageId plus generation.

No-flush barriers and active writebacks are mutually exclusive.

11. WAL-before-data protocol

For copied page_lsn=L:

copy stable image
finalize checksum
ensure durable_lsn >= L
pwrite exactly 8192 bytes
fdatasync owning page file
reconcile dirty/DPT state

12. Dirty-generation rule

A successful writeback clears dirty state only if both PageId and generation still match.

If a newer mutation raced, the older image is durable but the resident page remains dirty with its existing dirty-interval recovery metadata.

Generation comparison tokens may not silently wrap or repeat while a stale completion could match them.

13. Flush failures

WAL, write, short-transfer, or fdatasync failure:

- preserves dirty state, rec_lsn, generation, FPI metadata, and DPT membership;
- preserves the mapping and PageId;
- leaves ordinary resident frames retryable;
- restores failed eviction victims to RESIDENT;
- never permits reassignment based on presumed persistence.

14. Victim reservation and eviction

The final atomic RESIDENT -> EVICTING reservation is the eviction linearization point.

A fetch either:

- pins before reservation and defeats that victim choice, or
- observes EVICTING, waits, and retries after eviction succeeds/fails.

The reservation belongs to one requesting load through old-mapping removal, metadata reset, and new LOADING binding.

15. Load failures

Failed loads are not poison-cached:

- no resident page is published,
- all current waiters receive the same captured error,
- the in-progress mapping is removed,
- the frame becomes reusable,
- a later independent fetch may retry.

16. Frame reassignment

Before binding another PageId, BufferPool resets or invalidates all old identity-specific state, including dirty/recLSN, DPT/FPI, I/O, no-flush, checksum/
writeback, replacement, pin/claim, and generation-comparison metadata.

Old mapping removal precedes reset; reset precedes new binding.

17. Checkpoint/DPT synchronization

Dirty publication, stable-clean removal, and checkpoint DPT capture now share one conceptual synchronization order.

A checkpoint therefore either:

- captures the dirty page,
- observes it already durably clean, or
- orders before a new dirty transition whose WAL remains inside its recovery scan/retention range.

18. File retirement

BufferPool-visible FileIds have conceptual ACTIVE, RETIRING, and CLOSED states.

ACTIVE -> RETIRING blocks new loads/pins. Existing guards and I/O drain before mappings, DPT entries, frames, and file ownership are removed. Proven-obsolete
retired-file dirty pages may be discarded only under the established retirement protocol.

19. Crash/failure reasoning

The architecture now gives deterministic outcomes:

- Before WAL durability: no page write begins.
- After WAL durability, before page write: redo applies WAL.
- During/torn page write: retained full-page image repairs it.
- After fdatasync, before runtime clean publication: disk page is durable; lost dirty metadata is only conservative.
- Flush failure without crash: resident page remains dirty/retryable.
- Load failure: unpublished mapping is removed.
- Eviction failure: old resident page is restored unchanged in ownership and recovery state.

20. Sections changed

- docs/ARCHITECTURE.md:739
- docs/ARCHITECTURE.md:1046
- docs/ARCHITECTURE.md:1171
- docs/ARCHITECTURE.md:2248
- docs/ARCHITECTURE.md:3055
- docs/ARCHITECTURE.md:7394
- docs/ARCHITECTURE.md:7727
- docs/ARCHITECTURE.md:7935
- docs/ARCHITECTURE.md:11731

21. Persisted formats

No persisted field, numeric code, byte layout, checksum layout, WAL record format, or format version changed.

22. Remaining BufferPool questions

No material M-001 BufferPool state-machine decision remains open. Separate existing issues such as B+ MTR failure disposition and broader SQL/storage error
policy were not resolved or altered.

23. Verification and file scope

- git diff --check: passed.
- Architecture § references: all resolve.
- Duplicate numbered headings: none found.
- Only docs/ARCHITECTURE.md is modified.
- Production code, tests, PROJECT_STATE, DEVELOPMENT, VERIFICATION, and devlogs were untouched.

24. Phase boundary

Implementation Phase 2 was not entered. No BufferPool code was created or implemented.
