• # ARCHITECTURE SEMANTIC ACCEPTANCE: FAIL

  ## 1–3. Verdict and executive assessment

   Severity     Count
  ━━━━━━━━━━━  ━━━━━━━
   BLOCKING         0
  ───────────  ───────
   MAJOR            2
  ───────────  ───────
   MINOR            1
  ───────────  ───────
   EDITORIAL        1

  The current architecture is substantially complete and notably strong in persisted formats,
  BufferPool ownership, WAL/recovery, MVCC, UNIQUE enforcement, catalog bootstrap, scalar
  semantics, subqueries, statistics validation, exhaustion, and lifecycle management.

  However, it is not yet safe to freeze as the complete v1 contract:

  - Transaction-lifetime maintenance/DDL gates admit legal acquisition cycles without one complete
    deadlock/admission rule.

  - FLOAT64 aggregate results can depend on worker, chunk, spill, and combine order; integer
    aggregate boundary behavior is also not completely closed.

  - The parser grammar contradicts the explicitly supported SELECT-without-FROM form.

  Two competent teams could therefore produce different concurrency behavior and SQL results while
  reasonably claiming conformance.

  ## 4. Full findings

  ### BLOCKING findings

  NONE.

  ### SA-001 — Combined DDL, writer, and statistics-publication gate order is cyclic

  - Severity: MAJOR
  - Canonical sections:
      - docs/ARCHITECTURE.md:8079
      - docs/ARCHITECTURE.md:10493
      - docs/ARCHITECTURE.md:10795
      - docs/ARCHITECTURE.md:10925
      - docs/ARCHITECTURE.md:14777
      - docs/ARCHITECTURE.md:14985

  Exact conflicting propositions:

  1. Persistent DML holds a shared TableWriterGate(T) until transaction terminal publication.
  2. Schema-changing DDL acquires SchemaLock before any required exclusive TableWriterGate.
  3. Only a same-transaction upgrade of the target table’s shared gate to exclusive is explicitly
     rejected.

  4. STATS_PUBLISH is also transaction-lifetime ownership after statistics rows publish.
  5. DDL manifest changes may acquire SchemaLock/exclusive writer ownership and then wait on
     STATS_PUBLISH.

  6. Only STATS_PUBLISH waits are explicitly added to the existing wait-for graph; the combined
     participation of SchemaLock and TableWriterGate waits is not established.

  Adversarial scenario:

  T1: UPDATE table_a
      -> holds shared TableWriterGate(table_a)

  T2: CREATE INDEX ON table_a
      -> holds SchemaLock
      -> waits for exclusive TableWriterGate(table_a)

  T1: CREATE TABLE table_b
      -> waits for SchemaLock

  This is not the explicitly forbidden same-table gate upgrade: T1’s DDL concerns table_b. The
  resulting cycle has no canonical outcome.

  A second cycle is possible when one transaction retains STATS_PUBLISH after ANALYZE and later
  requests SchemaLock, while a DDL transaction already holding SchemaLock waits to publish a
  manifest change through the statistics gate.

  Why implementations could differ:

  - One could hang indefinitely.
  - One could reject all DDL-after-DML or DDL-after-ANALYZE.
  - One could put every gate in a common deadlock graph and abort the youngest transaction.
  - One could release transaction-lifetime ownership early, violating existing publication rules.

  Smallest architectural decision needed:

  Define one combined acquisition/admission rule for SchemaLock, TableWriterGate, and
  STATS_PUBLISH, including whether every blocking edge participates in the deterministic
  transaction deadlock graph. Legal multi-statement sequences that would invert the order must
  have one exact rejection or victim outcome.

  ### SA-002 — Numeric aggregate accumulation is not deterministic or fully boundary-closed

  - Severity: MAJOR
  - Canonical sections:
      - docs/ARCHITECTURE.md:17470
      - docs/ARCHITECTURE.md:17513
      - docs/ARCHITECTURE.md:18369

  Exact underspecification:

  - SUM(FLOAT64) “follows binary64 arithmetic semantics,” but the input accumulation and partial-
    state combine order is not defined.

  - Parallel aggregation may use worker-local states combined at finalization.
  - Spill/repartition and different chunking may create different reduction trees.
  - Binary64 addition is non-associative.
  - Integer SUM refers to a wider checked accumulator “where available,” leaving the exact
    implementation-independent accumulator requirement unclear.

  - COUNT has an INT64 result but no explicit result-overflow rule.

  Concrete scenario:

  For SUM(FLOAT64) over:

  1e16, -1e16, 1

  one legal reduction gives:

  (1e16 + -1e16) + 1 = 1

  A partitioned reduction can give:

  (1e16 + 1) + -1e16 = 0

  Both follow individual binary64 arithmetic and the currently allowed local-state/Combine model.

  Why implementations could differ:

  Worker count, hash partitioning, chunk size, spill behavior, and finalization order can change a
  query’s returned scalar value. Independent serial and parallel implementations can therefore
  disagree without violating the current prose.

  Smallest architectural decision needed:

  Define canonical observable accumulation/finalization semantics for SUM and AVG, including
  partial-state combination, independent of chunk/worker/spill order, and close aggregate result-
  overflow behavior including COUNT. The decision need not prescribe a class or container, but it
  must determine the same result/error.

  ### SA-003 — SELECT-without-FROM is both forbidden and required

  - Severity: MINOR
  - Canonical sections:
      - docs/ARCHITECTURE.md:13083
      - docs/ARCHITECTURE.md:13719
      - docs/ARCHITECTURE.md:14444

  Exact conflict:

  The grammar spells:

  SELECT ...
  FROM table_reference

  with mandatory FROM, while logical planning explicitly supports:

  SELECT 1;

  as one empty logical input row followed by projection.

  Concrete divergence:

  One parser rejects SELECT 1; another accepts it and produces the mandated LogicalValues(single
  empty row) plan.

  Smallest decision needed:

  Make FROM explicitly optional in the canonical v1 grammar or remove the no-FROM semantic
  commitment.

  ### SA-004 — Historical rewrite/provenance language remains live

  - Severity: EDITORIAL
  - Current occurrences:
      - docs/ARCHITECTURE.md:14854
      - docs/ARCHITECTURE.md:16365
      - docs/ARCHITECTURE.md:24039

  These phrases do not create a semantic ambiguity because the owning live sections are otherwise
  identifiable. They are stale provenance rather than architecture.

  Smallest decision needed: replace them with direct live-document statements. This cleanup is not
  correctness-blocking.

  ## 5. Original A-001 through A-003 status

   Issue                                   Current status    Current authority
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   A-001 status-page FPI/terminal          RESOLVED          §§12.10.5, 13.13.2, 13.17 define
   mutation                                                  preparatory full images, terminal
                                                             authorization, redo, and torn-page
                                                             handling.
  ──────────────────────────────────────  ────────────────  ──────────────────────────────────────
   A-002 statistics as semantic proof      RESOLVED          §§34.1, 35.2, and optimizer final
                                                             validation keep statistics costing-
                                                             only and separate estimated_rows=0
                                                             from proven emptiness.
  ──────────────────────────────────────  ────────────────  ──────────────────────────────────────
   A-003 namespace durability              RESOLVED          §§4.7.1–4.7.7 and 12.2.1 define
                                                             pending/final names, directory
                                                             synchronization, WAL-segment
                                                             publication, orphan classification,
                                                             and retirement.

  ## 6. Original M-001 through M-014 status

   Issue                                    Current status
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   M-001 BufferPool state machine           RESOLVED by §§7.5–7.12.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-002 catalog schema                     RESOLVED by §§16.5 and 16.9.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-003 page/MTR publication failure       RESOLVED by §§4.11.1 and 12.12.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-004 UNIQUE current-owner semantics     RESOLVED by §11.10.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-005 statement/transaction failure      RESOLVED by §39.1 and §§15.5–15.6.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-006 scalar registry                    REOPENED AT AGGREGATE BOUNDARY by SA-002. The scalar
                                            operator/cast registry in Chapter 17 is closed, but
                                            observable numeric aggregate reduction is not.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-007 subqueries                         RESOLVED by §20.14. SA-003 is a parser grammar
                                            inconsistency, not a missing subquery execution
                                            contract.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-008 structural validation              RESOLVED by §4.13 and owner-specific cross-
                                            references.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-009 StatsVersion/status reclamation    RESOLVED by §34.3.1.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-010 exhaustion                         RESOLVED by §4.3.2.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-011 maintenance coordination           REOPENED by SA-001 at the combined gate-order
                                            boundary.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-012 compatibility strictness           RESOLVED by §4.14.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-013 statistics numerical validation    RESOLVED by §34.14.6.
  ───────────────────────────────────────  ───────────────────────────────────────────────────────
   M-014 database lifecycle                 RESOLVED by §3.3.

  ## 7. Original D-001 through D-004 status

   Issue                                             Status                  Current reference
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━
   D-001 transaction-status page/cache conflation    RESOLVED BY CURRENT     §§9.14, 12.10.5, and
                                                     TEXT                    docs/
                                                                             ARCHITECTURE.md:1882
                                                                             5 distinguish
                                                                             resident status-page
                                                                             state, outer tuple
                                                                             MVCC, terminal cache
                                                                             publication, and
                                                                             opaque StatsVersion
                                                                             identity.
  ────────────────────────────────────────────────  ──────────────────────  ──────────────────────
   D-002 prune_hint                                  RESOLVED BY CURRENT     docs/
                                                     TEXT                    ARCHITECTURE.md:2075
                                                                             accepts every uint32
                                                                             pattern as advisory;
                                                                             §5.3.3 requires zero
                                                                             initialization
                                                                             without correctness
                                                                             semantics.
  ────────────────────────────────────────────────  ──────────────────────  ──────────────────────
   D-003 creation_epoch                              RESOLVED BY CURRENT     docs/
                                                     TEXT                    ARCHITECTURE.md:821
                                                                             and §4.10.2 define
                                                                             every uint64 value
                                                                             as known opaque v1
                                                                             data, not a
                                                                             monotonic identity.
  ────────────────────────────────────────────────  ──────────────────────  ──────────────────────
   D-004 rewrite provenance                          STILL VALID,            SA-004.
                                                     editorial only

  ## 8. Persisted-format coherence assessment

  PASS.

  The common page header, both superblocks, heap/slot/tuple formats, FSM, B+ node/free formats,
  transaction-status pages, WAL records, control slots, catalog bootstrap, six catalog schemas,
  scalar/default codecs, and statistics payloads form a coherent little-endian codec system.

  Important derived boundaries are mutually compatible:

  - Transaction-status page capacity is 8160 × 4 = 32,640 entries.
  - The signed 64-bit file limit gives 2^50-1 pages and final ordinary PageNo 2^50-2.
  - A 64 MiB WAL segment over a 64-bit byte address yields final segment index 2^38-1.
  - WAL total_length, alignment, segment boundaries, and mathematical one-past 2^64 end are
    distinguished correctly.

  - Terminal headroom’s 2 × 16,520 + 88 = 33,128 bound accounts for worst-case segment-tail
    consumption.

  - Reserved-zero, unknown-version, enum, flag, sentinel, checksum, and writer-canonicality
    policies are consistent.

  No persisted byte layout, code, version, or sentinel contradiction was found.

  ## 9. Transaction/MVCC coherence assessment

  PASS.

  TxnId/CommandId allocation, snapshots, READ COMMITTED and REPEATABLE READ behavior, xmin/xmax/
  cmin/cmax, current-command visibility, terminal status, freezing, and status reclamation agree.

  UNIQUE correctly uses a separate serialized current-owner predicate rather than snapshot
  visibility. Same-command duplicates, self-update exclusion, self-delete reuse, wait/recheck,
  stale index entries, and RID protection are closed.

  The error-state matrix consistently separates recoverable statement failure, MUST_ABORT, durable
  commit, outcome uncertainty, and database noncontinuability.

  ## 10. BufferPool/storage coherence assessment

  PASS.

  The architecture defines:

  - one loader and many waiters,
  - failed-load cleanup,
  - validation before resident publication,
  - pin/latch separation,
  - eviction reservation,
  - copied writeback and generation reconciliation,
  - dirty preservation after failure,
  - DPT/checkpoint serialization,
  - no-flush ownership,
  - append-first private publication,
  - file-retirement draining,
  - controlled shutdown.

  No race was found where a malformed or unvalidated page can become ordinarily resident, or where
  an old writeback completion can clean a newer generation.

  ## 11. WAL/recovery/checkpoint coherence assessment

  PASS.

  LSN reservation, no-hole append, segment creation, alignment, WAL-before-data, FPI epochs,
  status mutation, atomic B+ MTR replay, checkpoint capture, dual control publication, valid-
  prefix recovery, loser classification, and post-redo validation are coherent.

  Unknown WAL record types cannot be skipped. Partial records are never treated as complete.
  Durable commit authority is not reversed by cache or cleanup failure.

  ## 12. Catalog coherence assessment

  PASS.

  All six descriptors are reconstructible without invention:

  - stable TableIds and ColumnIds,
  - physical/logical ordinals,
  - types and nullability,
  - default representation,
  - exact constraint/index linkage,
  - statistics rows,
  - bootstrap fixed point,
  - file ownership.

  Catalog corruption prevents descriptor publication rather than selecting arbitrary duplicate
  rows. DROP and cache lifetime remain MVCC- and stable-ID-based.

  ## 13. SQL scalar semantics coherence assessment

  The closed Chapter-17 scalar registry itself passes:

  - literals and untyped NULL,
  - checked integer operations,
  - division/remainder,
  - binary64 behavior,
  - comparisons and 3VL,
  - VARCHAR/date/timestamp semantics,
  - casts and assignment coercion,
  - CASE and IN-list,
  - empty function registry,
  - folding and locale independence,
  - hash/group/join/key equality.

  The overall numerical SQL contract does not pass freeze because aggregate accumulation remains
  unresolved under SA-002.

  ## 14. Subquery coherence assessment

  PASS, aside from the no-FROM parser conflict in SA-003.

  V1 is uncorrelated-only. Scalar, EXISTS/NOT EXISTS, one-column IN/NOT IN, and derived-table
  forms have exact scope, cardinality, NULL, snapshot, CommandId, lazy evaluation, spill, physical
  fallback, and rewrite rules. Correlated, quantified, row-valued, LATERAL, set, and data-
  modifying forms are explicitly unsupported.

  ## 15. Optimizer correctness assessment

  PASS.

  The optimizer separates estimates from semantic proofs, preserves outer-join rows, does not
  treat required_rows as LIMIT, respects short-circuit/error behavior, protects NOT IN’s NULL
  semantics, and validates final empty replacements by approved proof provenance.

  Cost/search freedom may choose a slower plan but cannot legally change results.

  ## 16. Execution and lifetime assessment

  String/VARCHAR ownership, chunk reuse, blocking-operator ownership, spill lifetime, DML target
  spooling, retry boundaries, and RETURNING exposure are coherent.

  The only major execution-semantic defect found is SA-002: aggregate reduction order is
  observable but unspecified.

  ## 17. Statistics assessment

  PASS.

  The current text provides:

  - exact 2^-40 dyadic epsilon,
  - exact dyadic summation,
  - strict field domains,
  - signed-zero normalization,
  - unique canonical MCV values,
  - canonical MCV ordering,
  - deterministic residual mass,
  - histogram order and mass checks,
  - NDV/HLL constraints,
  - empty/all-null/MCV-exhausted forms,
  - generation-atomic rejection and fallback,
  - cache validation and monotonicity.

  Validated statistics remain advisory and cannot prove semantic emptiness.

  ## 18. Maintenance and retirement assessment

  The reclamation, object-lifetime, ANALYZE/DROP, status-history, cache, statistics-GC, RID-reuse,
  DRAINING, and NONCONTINUABLE rules are individually strong.

  The combined transaction-level gate order fails under SA-001. Physical unlink still correctly
  waits for maintenance, descriptor, BufferPool, frame, and guard ownership to drain.

  ## 19. Exhaustion and boundary assessment

  PASS.

  The checked-advance policy covers identifiers, PageNo/file length, CommandId, TxnId, LSN/end
  positions, segment indexes, WAL lengths, terminal credits, B+ height, statistics chunks, schema
  versions, and runtime generation/epoch tokens.

  Numeric exhaustion is correctly distinguished from corruption and disk/resource exhaustion. No
  allocator is permitted to wrap or reuse a consumed identity.

  ## 20. Combined locking/deadlock assessment

  FAIL due to SA-001.

  Within individual subsystems, orders are sound:

  logical locks before physical latches
  B+ parent/child and left/right orders
  UNIQUE key total order
  OBJECT_USE before page/epoch ownership
  SchemaLock before exclusive TableWriterGate

  The combined transaction-level order is not acyclic because a transaction may retain one gate
  across statements and later request a gate that another transaction acquired in the opposite
  order. The architecture’s deterministic deadlock fallback is not explicitly extended to every
  involved gate.

  ## 21. Database lifecycle assessment

  PASS.

  CLOSED → OPENING → RECOVERING → READY → DRAINING → CLOSING and NONCONTINUABLE transitions are
  exact.

  The process-associated fcntl hazard is addressed: the architecture requires one independently
  opened descriptor for the lock inode and forbids unrelated closes that would release the process
  lock. Same-process aliases, close-on-exec, unsupported forked-handle use, failed-open cleanup,
  recovery failure, shutdown failure, and lock-release timing are specified.

  ## 22. Crash-matrix assessment

   Crash point                                     Determined next-open interpretation
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   DML before publication-authorizing WAL          No logical publication; private/provisional
                                                   state disappears or is restored.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   DML after valid append but before page write    Redo reconstructs; loser-created data remains
                                                   invisible if not committed.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   COMMIT before durable C3                        No committed outcome; abort/loser handling
                                                   applies.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   COMMIT after durable C3                         COMMITTED irreversibly.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   ABORT/status mutation                           Full-image plus terminal redo establishes
                                                   ABORTED; incomplete tail is ignored.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   Heap append                                     Valid PAGE_INIT publishes; otherwise
                                                   serialized unpublished tail is truncated/
                                                   reconciled.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   B+ MTR                                          Complete record redoes atomically; incomplete
                                                   record has no effect.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   Checkpoint before control update                Previous valid control generation remains
                                                   authority.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   Checkpoint after control update                 Newest complete supported generation is
                                                   authority.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   DDL before catalog publication                  Files/IDs are private or orphan-cleanup
                                                   artifacts.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   DDL after durable commit                        Catalog object is authoritative; required
                                                   filenames were already durably published.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   ANALYZE                                         Only committed complete generations are
                                                   selectable.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   VACUUM                                          Completed WAL-backed units remain valid;
                                                   provisional current unit restores or redoes.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   DROP                                            Catalog commit makes object inapplicable;
                                                   physical unlink remains delayed until owners
                                                   drain.
  ──────────────────────────────────────────────  ────────────────────────────────────────────────
   Shutdown before successful-close point          Next open performs normal recovery.

  No crash interpretation defect was found.

  ## 23. Remaining correctness-relevant implementation choices

  Three remain:

  1. Combined SchemaLock/TableWriterGate/STATS_PUBLISH deadlock and admission behavior.
  2. Canonical numeric aggregate accumulation/combine and overflow behavior.
  3. Whether FROM is optional in v1 SELECT.

  Other remaining choices—container types, frame tables, thread counts, B+ split balance, cache
  structure, optimizer heuristics, spill algorithms, and scheduling—are performance or
  implementation freedom with fixed observable semantics.

  ## 24. Sections requiring repair before freeze

  Correctness-relevant repair is required in:

  - §§14.17.1, 15.1.1, 21.2, and 21.8 for the combined gate order/deadlock protocol.
  - §§29.2–29.3 and 32.6 for aggregate accumulation/finalization semantics.
  - §18.11 for the narrow no-FROM grammar conflict.

  SA-004 is optional editorial cleanup and does not itself block freeze.

  ## 25. Fresh-team implementability

  A fresh team could implement most individual subsystems without inventing semantics, including
  the Phase-1 storage boundary and the specified BufferPool contract.

  Two independent teams could not yet implement the complete v1 and reliably agree on all
  correctness-relevant behavior because of SA-001 and SA-002. Therefore the architecture should
  not yet be frozen as the complete v1 contract.

  ## 26–28. Read-only and phase confirmation

  - No files were modified. The final git status, git diff --stat, and targeted documentation diff
    were empty.

  - Production code and tests were untouched.
  - No devlog or project-state document was created or updated.
  - Implementation Phase 2 was NOT entered and is NOT authorized by this audit.