• Resolved M-009 in docs/ARCHITECTURE.md:17148.

  1. StatsVersion meaning: (TxnId, CommandId) identifies the ANALYZE publication and provides deterministic generation ordering. After committed publication it is opaque immutable identity.
  2. Publication-time dependency: Until terminal publication, ordinary transaction status and catalog MVCC determine whether ANALYZE rows publish. Global eligibility begins at COMMIT C4 after
     durable C3; cache coherence follows at C5.

  3. Post-publication independence: Once catalog MVCC admits a row as committed, consumers MUST NOT query transaction status using payload stats_txn_id.
  4. Complete-generation selection: Load visible catalog rows, verify object applicability, group by exact StatsVersion, validate chunks/manifest/payload, and select the greatest complete
     generation. Aborted and uncommitted outer tuple versions are excluded by ordinary catalog MVCC.

  5. Ordering: StatsVersions compare as unsigned lexicographic (txn_id, command_id). This is a freshness preference, not semantic truth or object-liveness evidence.
  6. CommandId role: It distinguishes and orders multiple ANALYZE statements from one transaction, including repeated analysis of the same table. CommandIds remain monotonic and unreused.
  7. Status-reclamation invariant: Reclamation requires that no persisted correctness state still needs transaction-outcome lookup below the cutoff. Merely containing an old TxnId numerically does
     not create that dependency.

  8. Cutoff effect: MIN(sys_statistics.stats_txn_id) does not constrain txn_status_reclaim_before. No statistics rewrite, deletion, replacement version, or status-page retention is required.
  9. Very old statistics: A complete generation remains structurally usable indefinitely despite staleness or reclaimed creator status.
  10. Restart after reclamation: Recovery reconstructs catalog MVCC state; frozen/otherwise valid outer rows establish visibility; the statistics loader accepts the old generation without creator-
     status lookup.

  11. Outer tuple versus payload: Outer sys_statistics tuple xmin/cmin follows ordinary MVCC and freezing. Payload stats_txn_id/stats_command_id remains unchanged and becomes status-independent
     after publication.

  12. Statistics cache: Cache keys compare the numeric pair directly and retain no status-page pin, status object, or reclamation guard solely for StatsVersion.
  13. Terminal cache: Loading statistics neither depends on nor repopulates ancient entries in the globally observable transaction-outcome cache.
  14. Malformed StatsVersion: Invalid TxnId/CommandId domains, row/payload mismatch, cross-version chunks, duplicate identities, or malformed representations invalidate the generation. Missing/
     reclaimed status does not.

  15. ANALYZE crash behavior: Pre-commit crashes leave loser/aborted rows unselected. A surviving durable commit is reconstructed even if C4/C5 cache publication did not occur. Later status
     reclamation and subsequent restart do not invalidate the generation.

  16. Statistics GC: Old generation deletion remains ordinary catalog MVCC/maintenance work. Embedded TxnIds require no special status coordination; M-011 remains untouched.
  17. Forbidden implementations: The architecture now forbids status retention for old stats, below-cutoff invalidation, treating missing status as aborted, status lookup during cache load,
     StatsVersion rewrites for reclamation, confusing payload identity with outer xmin, using StatsVersion for object liveness, selecting uncommitted rows by ordering, coupling statistics lifetime
     to status history, or applying this exception to ordinary MVCC TxnIds.

  18. Sections changed: §§9.11.1, 9.13, 14.14, 14.18, 16.5.7, 31.12.1, 34.3 with new §34.3.1, 34.14.5, 34.15, and 34.17.
  19. Persisted formats: No persisted byte format changed.
  20. Catalog schema version: Remains 1.
  21. Remaining M-009 questions: None.
  22. Prerequisite dependencies: None discovered.
  23. New architecture questions: None introduced. M-010, M-011, and M-013 remain untouched.
  24. Production code: Untouched. Tests and other requested documentation were untouched. git diff --check passed. The pre-existing untracked SOLVED M-012.md was preserved.
  25. Implementation Phase 2: NOT entered.