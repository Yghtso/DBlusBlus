• M-004 is resolved in docs/ARCHITECTURE.md:6588.

  1. Canonical predicate: after acquiring the logical key lock, every physical (K,RID) candidate is heap-rechecked using current xmin/cmin/xmax/cmax, terminal status, checking TxnId/CommandId, and
     exact operation context. Ordinary snapshot visibility is not used as the conflict decision.

  2. UNIQUE NULL rule: if any composite-key component is NULL, duplicate rejection and duplicate-prevention locking are skipped. The physical index entry is still stored.
  3. PRIMARY KEY remains UNIQUE + NOT NULL for every component and otherwise uses the identical current-state predicate.
  4. Lock identity is (IndexId, canonical encoded user-key bytes) without RID. It preserves canonical FLOAT64 NaN/signed-zero, binary VARCHAR, scalar, and composite equality.
  5. Ordering: INSERT evaluates/encodes keys, acquires locks, performs the complete post-lock scan, then publishes. UPDATE/DELETE first obtain TUPLE_WRITE, derive keys, acquire unique locks,
     revalidate again, scan, then publish.

  6. UNIQUE locks remain held through the §9.14 COMMITTED/ABORTED terminal-publication point, including while MUST_ABORT cleanup is pending.
  7. The B+ tree supplies physical candidates only. Presence of K is not proof of conflict; heap metadata and current transaction state determine ownership.
  8. The INSERT truth table is normative in docs/ARCHITECTURE.md:6697. Committed/frozen live creators conflict; committed deletion and aborted creation are stale; nonterminal transactions require
     waiting; impossible status/RID combinations are errors.

  9. Same-statement duplicates conflict. cmin == current CommandId counts as an owner even though ordinary snapshot visibility hides that version.
  10. A same-transaction earlier-command creator conflicts unless an earlier command deleted or superseded it.
  11. A self-delete with cmax < current CommandId frees the key. A current-command self-delete remains conflicting for another row operation; only the exact currently checked UPDATE target may
     self-exclude. This removes target-order-dependent outcomes.

  12. An active competing creator causes lock wait and complete recheck. Commit produces conflict; abort makes its physical candidate ignorable.
  13. Deleter behavior is exact:

  - committed deleter: old candidate is stale;
  - aborted deleter: old owner still conflicts;
  - nonterminal deleter: wait and recheck;
  - current transaction’s earlier-command deleter: stale;
  - current-command deleter from another row operation: still conflicts.

  14. UPDATE retaining the same key excludes only its exact revalidated old RID. No TxnId-wide or version-chain-wide exclusion exists.
  15. UPDATE changing key acquires the affected old/new locks, checks the new key against all other owners, then follows the existing new-version, old-xmax, and index-publication sequence.
  16. Multirow UPDATE is immediate and non-deferrable. Earlier current-command owners collide, and key swaps such as 1→2, 2→1 fail rather than being accepted as a deferred final permutation.
  17. Exact (key,RID) retry is idempotent only for the same in-process mutation owner retaining continuous operation and logical-lock provenance. An unproven executor duplicate is an invariant/
     corruption error. Recovery redo is separately page-LSN/MTR-idempotent.

  18. Stale entries are heap-rechecked and ignored only when their logical owner is aborted or terminally superseded. Duplicate stored copies of identical (key,RID), dangling entries, key/heap
     mismatches, or reused identities are corruption.

  19. UNIQUE candidate RIDs are protected by ReadEpochGuard through heap recheck. Waiting discards the guard and all candidates; wakeup starts a fresh protected range scan.
  20. UNIQUE ownership is snapshot-independent. The snapshot may select UPDATE/DELETE targets but cannot authorize a new key owner.
  21. After every wait, the implementation discards prior RID, status, visibility, and cursor conclusions and rescans from (K,MIN_RID).
  22. M-005 remains unchanged. UNIQUE_CONFLICT determines the violation; §39.1 decides whether the transaction remains active or automatically aborts based on the existing first-published-write
     boundary. Autocommit uses the same predicate and lock lifetime.

  23. Recovery never performs fresh SQL UNIQUE checks. It replays already-authorized heap/index WAL and classifies loser-created versions through existing transaction status.
  24. UNIQUE correctness is independent of prompt vacuum. Vacuum removes stale entries only under existing reclamation and RID-reuse rules.
  25. Offline CREATE UNIQUE INDEX uses the same encoding, NULL, and current-owner rules. Its exclusive writer gate permits bulk sorted/grouped duplicate detection without per-row key locks.
  26. The architecture now explicitly forbids snapshot-based enforcement, current-command duplicate omission, broad self-exclusion, uncommitted-delete key release, aborted-state inversion, index-
     presence-only decisions, pre-lock authorization, early lock release, redo-time SQL checking, inconsistent key normalization, unsafe RID reuse, and snapshot races producing two committed
     owners.

  27. Sections changed: §§8.21, 8.22, 8.29; §§11.8–11.11 and 11.15; §§13.13.3 and 13.21; §14.6.1; §§15.2–15.4 and 15.9; §§21.7–21.8; §§31.6–31.7 and 31.13; §§39.1.3–39.1.4; §§41.2–41.3; Appendix
     B.

  28. No persisted page, tuple, B+ tree, WAL, catalog, transaction-status, or identifier format changed.
  29. No correctness-relevant M-004 question remains open.
  30. No new unresolved architecture question was introduced.
  31. Production code and tests were untouched. Only docs/ARCHITECTURE.md changed. git diff --check passed; code tests were not run because this was architecture-only documentation work.
  32. Implementation Phase 2 was NOT entered.