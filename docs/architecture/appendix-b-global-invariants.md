# Appendix B. Global Invariants

The following global invariants apply across subsystem boundaries and MUST NOT be weakened without an explicit architecture revision.

1. Persistent structures use logical persistent identifiers such as page IDs, never process memory pointers.
2. On-disk formats use explicit serialization.
3. WAL required to recover a persistent data-page modification becomes durable before that page modification becomes durable.
4. Buffer pins are eventually released according to the page-lifetime contract.
5. Transaction-level logical locks and in-memory latches are distinct mechanisms.
6. Logical plans do not encode a physical implementation choice.
7. The optimizer may choose among multiple physical algorithms for one logical operation.
8. Performance-critical execution paths avoid unnecessary per-cell or per-row object allocation.
9. Lower layers do not depend on SQL syntax objects.
10. Verification can force constrained resource configurations, including tiny buffer pools, to exercise eviction.
11. Recovery is verified with simulated crashes.
12. Performance-sensitive changes require measurement.
13. Acknowledged durable state never depends on a file, WAL-segment, or final database-root namespace entry whose required parent-directory synchronization has not succeeded.
14. A failed statement that published any transaction-owned database mutation cannot leave that transaction commit-eligible in the defined architecture.
15. Durable `TXN_COMMIT` is irreversible and cannot be reclassified ABORTED by any later runtime, cache, cleanup, or transport failure.
16. Successful COMMIT acknowledgement follows required runtime terminal publication and coherent ownership/cache cleanup.
17. UNIQUE/PRIMARY KEY ownership is decided by §11.10's serialized current-state predicate, not ordinary snapshot visibility or physical index presence alone.
18. One actual database root has at most one process-local owner and one
    OS-exclusive process owner; creation publication, ordinary open, and offline
    removal use the same stable root/control claim and `database.control` lock,
    and recovery or ordinary mutation never runs without that ownership.
19. Ordinary work begins only at §3.3's READY publication, and the exclusive
    lock is released only after every database user/manager has stopped.
20. Successful controlled close drains transaction and BufferPool ownership,
    keeps WAL durability available through page flush/final checkpoint, and
    completes required namespace durability before reporting success.
21. A creator owns the ordinary root/control gate across final-root rename and
    external-parent synchronization; after prior-owner death, an independent
    owner establishes and revalidates that parent durability before database
    inspection, recovery, removal, READY, or acknowledged work.
22. Whole-database removal deletes no internal content while the captured old
    root occupies live final name `D`; removal is acknowledged only after the
    no-replace retirement rename and ordered external-parent synchronization
    make old `D` durably absent.
23. The ordinary control-lock inode is not unlinked or replaced while its old
    root remains live, and a durably retired root is never an ordinary open
    target.
24. Physical reclamation failure after durable whole-root retirement cannot
    restore old `D`, reverse semantic removal, or authorize cleanup outside the
    exact retained retired root.

Subsystem invariant sets are canonical in their owning chapters. Heap/tuple invariants are listed in §5.21; FSM/reclamation invariants are listed in §6.13; I/O/buffer invariants are listed in §7.13; B+ tree invariants are listed in §8.29; transaction/snapshot invariants are listed in §9.16; MVCC invariants are listed in §10.6; logical-locking invariants are listed in §11.15; WAL/commit invariants are listed in §12.18; recovery invariants are listed in §13.21; vacuum/reclamation invariants are listed in §14.18; end-to-end write invariants are listed in §15.9. Catalog invariants are listed in §16.11; type/value invariants in §17.12 and persisted-scalar invariants in §17.13.5; lexer/parser/AST invariants in §18.16; binder/expression invariants in §19.20; logical-plan/rewrite invariants in §20.20; upper semantic-layer invariants in §21.20; physical-plan/runtime invariants in §22.8; vector/string invariants in §23.14; memory/spill invariants in §24.11; expression-execution invariants in §25.8; pipeline invariants in §26.10; scan/unary invariants in §27.12; join invariants in §28.13; aggregation invariants in §29.9; sorting invariants in §30.8; DML/result invariants in §31.13; parallel-runtime invariants in §32.13; optimizer invariants in §33.7; statistics invariants in §34.17; estimation invariants in §35.27; base-access/cost invariants in §36.19; join/property invariants in §37.18; memo/search invariants in §38.25.
