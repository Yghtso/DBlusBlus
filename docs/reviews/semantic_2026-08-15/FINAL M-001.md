• # M-001 ACCEPTED

  The current architecture defines a complete, internally coherent, and implementable BufferPool semantic
  contract. Two competent engineers can independently implement it without inventing correctness-relevant
  behavior. Differences may remain only in harmless implementation details such as synchronization
  primitives, containers, identifiers, and scheduling policy.

  ## Finding classification

  - BLOCKING M-001 DEFECT: None.
  - MAJOR M-001 UNDERSPECIFICATION: None.
  - MINOR CLARIFICATION: None required.
  - NON-BLOCKING DESIGN RISK: The protocols are synchronization-dense and will require strong race,
    fault-injection, and tiny-pool testing. Explicit current-generation flushes may also repeatedly retry
    under sustained mutation. These are implementation/liveness risks, not missing semantics.

  - NO ISSUE: All audited contract areas and adversarial scenarios.

  ## Two-implementers test

   Protocol                    Required correctness behavior                                   Agreement
  ━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━
   Frame model                 Every frame maps to exactly one of FREE, LOADING, RESIDENT,     Yes
                               or EVICTING; I/O is an orthogonal constrained substate.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Load coalescing             The first miss publishes the sole LOAD_INTENT before frame      Yes
                               selection; later fetches register claims and wait.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Load publication            No guard sees bytes before complete read, checksum/header/      Yes
                               identity validation, owner validation, and atomic LOADING ->
                               RESIDENT publication.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Fetch and eviction          Pin acquisition and victim reservation are competing atomic     Yes
                               linearization points. Whichever wins determines whether
                               eviction proceeds.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Pin and guards              Every guard owns one checked pin and one appropriate latch;     Yes
                               latch releases before pin; overflow changes nothing.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Borrowed data               Every page/tuple/VARCHAR reference becomes invalid when its     Yes
                               supplying guard releases, regardless of other pins.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Mutation publication        WAL record, page bytes, page_lsn, generation, dirty state,      Yes
                               rec_lsn, DPT, and FPI metadata are published as one
                               protected protocol.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Copied writeback            A stable image is copied under the read latch, checksum-        Yes
                               finalized privately, WAL-flushed before pwrite, then file-
                               synchronized.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Writeback reconciliation    Dirty/DPT state clears only after successful write plus         Yes
                               fdatasync, matching PageId, and matching generation.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Eviction                    Old mapping remains non-pinnable through clean removal or       Yes
                               dirty stable writeback; reassignment occurs only after
                               removal and complete reset.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   New pages                   A private new-page intent constructs and validates the page     Yes
                               without reading disk; page-count and resident publication
                               are atomic.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Retirement/shutdown         Retirement blocks new pins/loads and drains existing            Yes
                               ownership before close/unlink; controlled shutdown cannot
                               claim success after required flush failure.
  ──────────────────────────  ──────────────────────────────────────────────────────────────  ───────────
   Checkpoint/recovery         Dirty publication, stable-clean removal, and DPT capture are    Yes
                               ordered so required recovery WAL cannot be missed or
                               recycled.

  These outcomes are fixed by the resident state machine and page table contract in docs/
  ARCHITECTURE.md:3080, guard rules in docs/ARCHITECTURE.md:3224, writeback rules in docs/
  ARCHITECTURE.md:3350, and replacement/retirement rules in docs/ARCHITECTURE.md:3500.

  ## Adversarial results

  1. Duplicate usable residents — NO ISSUE. LOAD_INTENT is installed before victim selection or I/O, and
     only its bound LOADING frame can become resident.

  2. Reassigning a pinned frame — NO ISSUE. Victim reservation requires an atomic final pin_count == 0
     recheck; an existing pin categorically forbids reuse.

  3. Mapping/byte identity disagreement — NO ISSUE. The old mapping is removed before reset, reset
     completes before the new binding, and partially loaded bytes remain non-usable under LOADING.

  4. Lost dirty state after copied writeback — NO ISSUE. Every persistent mutation advances a
     nonrepeating generation. A generation mismatch preserves dirty, rec_lsn, FPI metadata, and DPT
     membership.

  5. Early rec_lsn/DPT clearing — NO ISSUE. Clearing requires matching identity and generation after
     complete write and successful owning-file fdatasync, synchronized against checkpoint capture.

  6. Data before required WAL — NO ISSUE. WAL durability through the copied page_lsn is established
     before any page pwrite; WAL-flush failure prevents the write.

  7. Partial or unvalidated load exposure — NO ISSUE. Exact transfer, checksum/common-header validation,
     PageId validation, and owner structural validation all precede resident publication.

  8. Poisoned mapping after failed I/O — NO ISSUE. Load failure removes the intent and resets the frame;
     flush failure preserves the resident identity and all dirty/recovery metadata.

  9. Eviction failure losing the old page — NO ISSUE. Failure restores the old mapping to pinnable
     RESIDENT, keeps it dirty and retryable, and fails the requesting load intent.

  10. File retirement racing fetch — NO ISSUE. ACTIVE -> RETIRING is the gate linearization point. A
     fetch either acquires its pin before the gate and drains normally, or fails afterward. In-progress
     loads cannot publish.

  11. Checkpoint missing required dirty recovery state — NO ISSUE. The DPT transition reservation spans
     the clean/FPI decision, WAL append, and dirty publication. Checkpoint therefore captures the page,
     observes a durably clean page, or retains the post-capture WAL image.

  12. Borrowed view surviving its guard — NO ISSUE. The contract explicitly invalidates all non-owning
     page, tuple, VARCHAR, iterator, and controller views immediately upon guard release.

  13. Unimplementable state or transition — NO ISSUE. LOAD_INTENT is explicitly a page-table state rather
     than a frame ownership state, while every frame state and legal I/O combination has a realizable
     transition and failure disposition.

  ## Cross-subsystem consistency

  A-001 remains coherent. The TXN_STATUS protocol keeps the physical full-image LSN in rec_lsn, advances
  page_lsn to the later semantic terminal-record LSN, and uses the same BufferPool generation, DPT,
  stable-writeback, checksum, and WAL-before-data rules. See docs/ARCHITECTURE.md:7099.

  A-003 remains coherent. Regular-file fdatasync governs page-content durability, while directory fsync
  separately governs create/rename/unlink durability. Retirement drains BufferPool ownership before close
  and durable unlink. See docs/ARCHITECTURE.md:596 and docs/ARCHITECTURE.md:3598.

  The checksum lifecycle, full-page-image invariant, WAL retention, and fuzzy-checkpoint protocol are
  mutually consistent across docs/ARCHITECTURE.md:1147, docs/ARCHITECTURE.md:6975, and docs/
  ARCHITECTURE.md:7685.

  Therefore:

  - M-001 no longer blocks future BufferPool implementation.
  - No material BufferPool semantic decision remains open.
  - A-001 and A-003 remain coherent with the BufferPool rules.
  - No persisted format change is required.
  - Implementation Phase 2 has NOT been entered; docs/PROJECT_STATE.md:9 still records it as not started.
  - No files were modified and no patches were applied.