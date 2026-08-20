• M-003 is resolved in docs/ARCHITECTURE.md:7350.

  1. Canonical provisional-mutation model: mutations remain private, no-flush protected, and exactly rollback-capable until their publication-authorizing WAL record validly appends.
  2. WAL append success: the complete framed/CRC-valid record and alignment bytes are installed in append-owned memory and the contiguous valid append end advances atomically. Records are all-or-
     nothing.

  3. LSN reservation: reservation consumes no WAL position, creates no holes, and cannot update page_lsn, DPT metadata, dirty generation, or last_wal_lsn. Retained retries may reuse the candidate;
     later retries obtain the current end.

  4. No-flush barrier: installed through BufferPool ownership before provisional resident mutation; excludes copied writeback and eviction until complete publication or validated rollback.
  5. Publication point: after the final publication-authorizing append, page bytes, page_lsn, generation, dirty/rec_lsn/DPT/FPI state, and structural metadata become visible together.
  6. B+ MTR success: acquire the complete ownership set, resolve writeback, capture exact rollback state, install barriers, build provisional final pages, append one complete BTREE_MTR, publish
     all page/root/free-list effects, then release ownership.

  7. B+ MTR append failure: a known no-append result restores every affected page and structural metadata item exactly before releasing barriers. Append uncertainty is noncontinuable.
  8. Previously dirty pages: rollback restores their exact prior bytes, generation, page_lsn, rec_lsn, dirty flag, DPT membership, and FPI state. They never become spuriously clean.
  9. Copied writeback interaction: writeback and no-flush acquisition are competing transitions. Writeback-first makes mutation wait for reconciliation; no-flush-first prevents writeback from
     starting.

  10. Checkpoint/DPT interaction: provisional changes are invisible. Multi-page MTR publication and checkpoint capture share one conceptual DPT-publication gate, producing a wholly old or wholly
     new affected set.

  11. Ordinary append success: serialize append, reserve the sole private frame, extend, initialize and validate, append PAGE_INIT, then atomically publish frame metadata and published_page_count.
  12. PAGE_INIT append failure: no resident page, page reference, published count, or invalid page_lsn survives a known failure.
  13. Tail cleanup/PageNo reuse: while append serialization remains held, truncate and verify the file back to its exact prior length. The unpublished PageNo may then be reused. Cleanup
     uncertainty is fatal.

  14. Append versus flush failure: append success creates legal volatile dirty state. Later WAL pwrite/fdatasync failure leaves that state intact, does not advance durable_lsn, and blocks
     dependent data-page writeback.

  15. Newly allocated B+ pages: known pre-append failure removes reachability/private frames and truncates the consecutive unpublished tail as one unit. Reused free-list pages return to their
     exact prior free state.

  16. Recoverable versus fatal: failure is local only when unchanged or exact restoration is proven. Uncertain append, failed restoration, failed tail cleanup, or failed post-authorizing-append
     publication makes the storage owner noncontinuable.

  17. Ordinary append crash points: all requested boundaries now specify whether recovery removes an unpublished tail, reconstructs from surviving valid PAGE_INIT, or repairs a torn/stale page
     after durable WAL.

  18. B+ MTR crash/failure points: every requested stage now resolves deterministically to unchanged/restored runtime state, complete atomic redo, legal dirty publication, or noncontinuable
     escalation.

  19. Root/free-list restoration: root identity, height, generation, parent/sibling links, allocation state, and persistent/process-local free-list state participate in the same publication/
     rollback domain.

  20. Observer visibility: only participants inside the exclusive provisional ownership domain may inspect provisional bytes. Fetch guards, scans, tree searches, checkpoint, flush, and eviction
     see pre-operation or fully published state.

  21. A-001 consistency: preparatory status-page images remain redo-safe and semantically inert. Failure before the terminal publication-authorizing record leaves exact pre-terminal state;
     terminal append success requires completion or fatal escalation.

  22. M-001 consistency: copied writeback, generation reconciliation, DPT synchronization, frame identity, eviction, and no-flush rules are refined without reopening the BufferPool state machine.
  23. A-003 consistency: physical page extension remains separate from filesystem-name publication. Canceled WAL reservations may leave only the already-permitted next-contiguous empty segment
     artifact.

  24. Sections changed: §§4.11.1–4.11.3, 4.13; §§7.6.1–7.6.2, 7.10.1, 7.10.5, 7.12.4, 7.12.7, 7.13; §§8.25, 8.26, 8.29; §§12.10.3, 12.10.5.1, 12.12, 12.17, 12.18; §§13.11, 13.13.3, 13.21; §39.1;
     §41.3.

  25. Persisted formats: none changed. No page, WAL, catalog, checksum, numeric code, or version layout changed.
  26. Unresolved M-003 questions: none.
  27. New architecture questions introduced: none. M-005 transaction/statement error policy and M-004 UNIQUE semantics remain explicitly untouched.
  28. Production code and tests: untouched. Only docs/ARCHITECTURE.md changed. git diff --check passed.
  29. Implementation Phase 2: NOT entered; docs/PROJECT_STATE.md still records Phase 2 as not started.