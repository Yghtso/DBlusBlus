• M-014 is resolved in docs/ARCHITECTURE.md:305.

  1. Runtime states: CLOSED, OPENING, RECOVERING, READY, DRAINING, CLOSING, and NONCONTINUABLE, with complete legal transitions.
  2. Process exclusivity: actively enforced, not an external deployment assumption.
  3. Lock lifetime: acquired before control/WAL inspection or recovery and retained until every worker, guard, manager, frame, and registration is gone. OS lock release is the final external
     ownership transition.

  4. Lock object: nonblocking, process-associated, whole-file POSIX fcntl(F_SETLK) write lock on database.control. No separate lock file or persisted lock metadata exists.
  5. Multiple handles: a process-local actual-root/inode registry prohibits a second independent owner, including path aliases.
  6. Open sequence: final-root validation → owner claim/lock → managed directories → control/bootstrap/status validation → WAL/checkpoint validation → recovery-scoped managers → analysis/redo →
     catalog ownership and deferred redo → recovery checkpoint → orphan classification → caches/services → atomic READY.

  7. Recovery entry: every open performs recovery under exclusive ownership. Ordinary and background work remain forbidden throughout RECOVERING.
  8. Clean-shutdown decision: v1 has no clean-shutdown marker or recovery fast path. Every open runs recovery; a final checkpoint only bounds its cost.
  9. Required files: bootstrap roots are required before recovery; committed catalog-owned files are required after catalog reconstruction. Missing required files prevent READY.
  10. WAL startup: exact segment grammar, contiguous required range, complete valid-record prefix, legal torn-tail exclusion, and controlled treatment of empty next/old recyclable segments.
  11. Transaction status: raw status pages are recovery-private and untrusted until A-001 reconstruction, loser resolution, and status repair complete.
  12. Catalog/bootstrap: immutable bootstrap identities locate recovery storage; recovered self-hosted catalog rows become semantic authority only after post-recovery cross-checking.
  13. Orphans: exact managed survivors must be classified before READY. Physical deletion may be deferred while they remain catalog-inaccessible; unknown names are untouched.
  14. READY point: one atomic publication after recovery, status, catalog, files, BufferPool, caches, and service prerequisites are coherent.
  15. Failed-open cleanup: workers and partial BufferPool state are quiesced, descriptors close, then the OS lock and process claim are released. Uncertain ownership leaves the owner
     NONCONTINUABLE.

  16. NONCONTINUABLE: immediately closes admission and forbids ordinary transactions, mutations, WAL append, checkpoint, writeback, and successful new commit acknowledgements. Only safe non-clean
     teardown is legal.

  17. Shutdown gate: READY -> DRAINING rejects all new ordinary work while allowing required internal completion of already-admitted terminal protocols.
  18. Active transactions: ACTIVE and MUST_ABORT are aborted; already COMMITTING or ABORTING transactions complete their terminal protocols.
  19. Background ordering: stop new maintenance/checkpoints, drain transactions, join query/maintenance workers, then drain BufferPool while WAL services remain available.
  20. BufferPool drain: reject new external fetch/allocation, drain all ownership, and stably flush required dirty pages. Only proven retired/orphan frames may be discarded.
  21. WAL/data ordering: WAL durability remains available through terminal transactions, page flushes, and final checkpoint/control publication. WAL shuts down afterward.
  22. Final checkpoint: mandatory for successful controlled close, with no active writers and an empty DPT. It is not required for crash correctness and is not a clean marker.
  23. Namespace durability: all owned cleanup tasks and initiated namespace mutations require the relevant parent-directory fsync; close() is never durability.
  24. Successful close: reported only after transaction drain, data/WAL durability, final checkpoint, namespace work, worker shutdown, manager destruction, and exclusivity release.
  25. Shutdown failure: never reports success. The owner becomes NONCONTINUABLE; safe non-clean teardown may reach CLOSED, but the next open must recover.
  26. Process/machine crash: OS locks disappear automatically, volatile state is lost, and the next exclusive opener performs full recovery. No stale-lock-file cleanup is needed.
  27. Database creation: an opened post-create handle must pass through the same ownership, validation, recovery, cache, and READY gates.
  28. Whole-database removal: not an online v1 operation. It is external/offline, requires CLOSED, and must acquire the same exclusive control-file lock.
  29. Lifecycle errors: distinct DATABASE_BUSY, NOT_A_DATABASE, UNSUPPORTED_FORMAT, CORRUPT_DATABASE, RECOVERY_FAILED, I/O/durability, DATABASE_NONCONTINUABLE, and SHUTDOWN_FAILED classes.
  30. State-transition table: normatively covers CLOSED → OPENING → RECOVERING → READY, graceful READY → DRAINING → CLOSING → CLOSED, failure transitions to NONCONTINUABLE, and failed-open cleanup
     to CLOSED.

  31. Shutdown sequence: admission stop → transaction terminal handling → background drain → BufferPool flush → final checkpoint → namespace cleanup → BufferPool/WAL shutdown → managers close →
     lock release.

  32. Crash points: open and shutdown crash boundaries now all lead to deterministic next-open recovery using valid control/WAL/data/namespace state.
  33. Forbidden implementations: convention-only exclusivity, recovery before locking, stale-file ownership, duplicate owners, pre-READY admission, pre-recovery status trust, ignored required
     files, arbitrary orphan deletion, early WAL shutdown, dirty-page loss on close, early lock release, reversing durable commits, invented clean state, post-noncontinuable work, close()
     durability, and normal work during recovery.

  34. Sections changed: §§2.4, 3.1, new 3.3–3.3.7, 4.7.1, 4.7.8, 7.12.6, 9.4, 12.12.4, 13.2.3, 13.5, 13.11, 13.15, 13.19, 13.21, 16.9.4, 39.1, 40.3–40.4, 41.3, and Appendix B.
  35. Persisted formats: no change. No page, WAL, control, catalog, status, or namespace format was added or versioned.
  36. Remaining M-014 questions: none. The two-implementer test passes without correctness-relevant optional behavior.
  37. New architecture questions: none introduced.
  38. Production code and tests: untouched. Only docs/ARCHITECTURE.md changed; git diff --check passes.
  39. Implementation Phase 2: NOT entered. docs/PROJECT_STATE.md was untouched.