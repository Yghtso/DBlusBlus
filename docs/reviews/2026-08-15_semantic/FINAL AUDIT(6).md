• # ARCHITECTURE SEMANTIC ACCEPTANCE: PASS

  1. Verdict: PASS
  2. BLOCKING: 0
  3. MAJOR: 0
  4. MINOR: 0
  5. EDITORIAL: 0
  6. Executive assessment

  The current docs/ARCHITECTURE.md is sufficiently complete, coherent, deterministic, and self-
  contained to freeze as the v1 architecture contract.

  Two competent teams can independently implement v1 without inventing correctness-relevant
  semantics. Remaining freedoms concern implementation strategy, scheduling, data structures,
  tuning, and cache organization—not observable SQL, persistence, recovery, transaction, or
  concurrency behavior.

  7. Full findings

  NONE.

  ## Regression assessments

  8. SA-001 — synchronization/deadlock: RESOLVED

  docs/ARCHITECTURE.md:8089 defines one database-local wait-for graph covering:

  - SchemaLock
  - both TableWriterGate modes
  - STATS_PUBLISH
  - MANIFEST_CHANGE
  - TUPLE_WRITE
  - UNIQUE_KEY
  - fairness-queue predecessors

  Edges are installed before sleeping, cycle detection is synchronous, and the highest normal
  TxnId in each cyclic SCC is the deterministic victim. Victims enter MUST_ABORT, retain
  correctness-critical ownership through ABORTED terminal publication, and release at A3. Wakeups
  rebuild edges and revalidate invalidatable state.

  The required adversarial cycles are observable by the graph:

  - UPDATE A / CREATE INDEX A / CREATE TABLE B
  - ANALYZE A / manifest-changing DDL A / unrelated DDL B
  - cross-table UPDATE followed by opposing DDL
  - mixed schema/writer/publication/logical-lock cycles

  Same-transaction shared-to-exclusive writer upgrades and published-statistics-to-conflicting-
  manifest changes are rejected before waiting. Nontransaction maintenance resources cannot close
  an invisible mixed cycle because they neither wait back on transaction gates while holding short
  ownership nor force DROP to wait before terminal publication.

  9. SA-002 — aggregate determinism: RESOLVED

  docs/ARCHITECTURE.md:17973 provides a closed aggregate registry and exact semantic states:

  - COUNT uses an exact nonnegative count and overflows only above INT64_MAX.
  - Integer SUM uses an exact mathematical signed sum and range-checks only at Finalize.
  - FLOAT64 SUM exactly accumulates dyadic inputs and rounds once, ties-to-even.
  - AVG uses exact sum plus exact count and rounds the exact rational once.
  - NaN, infinities, and signed zero have commutative canonical rules.
  - MIN/MAX retain canonical scalar representatives.

  Serial, vectorized, parallel, hash, ordered, and spilled execution must produce identical values
  and aggregate numerical errors. SUM(1e16,-1e16,1) is exactly 1.0 for every permutation and merge
  tree. Multiple numerical finalization failures use the lowest semantic aggregate ordinal; no
  result row is exposed first.

  10. SA-003 — SELECT without FROM: RESOLVED

  docs/ARCHITECTURE.md:13416 makes FROM optional. docs/ARCHITECTURE.md:13454 and docs/
  ARCHITECTURE.md:14135 define one zero-column LogicalValues input row.

  Consequences are exact:

  - SELECT 1 returns one INT32 row.
  - SELECT TRUE returns one TRUE row.
  - SELECT COUNT(*) returns INT64 1.
  - SELECT * without FROM is a bind error.
  - Unbound columns are bind errors.
  - WHERE uses ordinary TRUE-only filtering.
  - Subqueries receive independent no-FROM inputs without correlation.
  - No DUAL or pseudo-relation exists.

  No remaining text makes FROM mandatory.

  11. SA-004 — authority/provenance cleanup: RESOLVED

  No stale audit IDs, architecture-rewrite history, legacy-authority dependency, pass-history
  framing, or SOLVED-report dependency remains. Matches for terms such as “rewrite,” “pass,”
  “previous,” and “provenance” are present-tense technical uses involving optimizer rewrites,
  vacuum passes, previous WAL/version state, or semantic-proof provenance.

  ## Subsystem assessments

  12. Persisted formats: coherent

  The common page header, generic and B+ superblocks, control slots, WAL framing, heap/FSM/B+
  pages, transaction-status pages, catalog bootstrap, six system relations, PersistedScalarV1,
  DefaultValueBlob, and statistics payloads agree on widths, offsets, endian rules, sentinels,
  checksums, versions, reserved-zero regions, and strict readers.

  No later semantic repair changed a byte layout. Important derived bounds remain coherent,
  including the signed-64-bit page-file maximum and the 33,128-byte terminal WAL credit (2 ×
  16,520 + 88).

  13. BufferPool/storage: coherent

  The architecture defines one owner for first-miss loading, shared load failure, eviction
  reservation, copied writeback, dirty-generation reconciliation, no-flush publication, stale
  completion rejection, file retirement, and shutdown drain.

  Fetch, eviction, writeback, and retirement cannot publish stale frame state or bypass pins/
  generations. Transaction-level waits are forbidden under page latches.

  14. WAL/recovery/checkpoint: coherent

  LSN reservation does not publish a hole. WAL append is one complete framed-prefix publication;
  physical short writes cannot retroactively change the logical append result. WAL-before-data,
  namespace durability, group commit, terminal status records, FPI epochs, DPT capture, checkpoint
  installation, control publication, tail validation, redo, and loser resolution have explicit
  boundaries.

  No acknowledged commit can lack durable terminal authority. Partial WAL cannot be interpreted as
  complete, and numeric WAL position never wraps.

  15. Transaction/MVCC: coherent

  TxnId and CommandId allocation, snapshots, READ COMMITTED, REPEATABLE READ, current-command
  visibility, terminal status, MUST_ABORT, retry, autocommit, RETURNING, freezing, and status
  reclamation are mutually consistent.

  The runtime terminal-outcome cache is explicitly distinct from BufferPool transaction-status
  pages. Terminal publication is the common visibility, active-registry, and lock-release
  boundary.

  16. Combined deadlock/locking: coherent

  The global partial order separates:

  - lifecycle admission
  - transaction wait-for resources
  - maintenance/object claims
  - status-history coordination
  - ReadEpoch
  - BufferPool/page ownership
  - B+ structural latches
  - retirement drain

  Every transaction-to-transaction blocking wait capable of closing a cycle enters the unified
  graph. Page latches, guards, and maintenance claims remain outside only under protocols that
  prevent reverse waits into the transaction graph.

  17. UNIQUE: coherent

  The current-owner predicate is distinct from ordinary snapshot visibility. Same-command
  duplicates, prior-command ownership, self-delete/reuse, retaining/changing-key updates, swaps,
  concurrent creators/deleters, aborted versions, stale entries, RID protection, retries, and
  recovery are closed.

  Canonical key encoding aligns UNIQUE locks, B+ key equality, FLOAT64 normalization, and non-NULL
  equality. Any NULL component bypasses ordinary UNIQUE duplicate rejection; PRIMARY KEY rejects
  NULL first.

  18. Heap/vacuum/RID reuse: coherent

  The required order is exact:

  logical reclaimability
  → all stale index entries absent
  → NORMAL to DEAD
  → ReadEpoch retirement/grace
  → version-chain proof
  → DEAD to UNUSED
  → RID reuse

  All indexes participate. Status-history guards prevent cutoff advancement past vacuum
  dependencies. DROP can commit before maintenance drains, but unlink cannot occur until every
  relevant owner, guard, frame, and descriptor is gone.

  19. B+ tree: coherent

  Physical (user_key,RID) order, separator routing, splits, redistribution, merge, root growth/
  contraction, siblings, free pages, exact deletion, MTR publication, recovery, and height
  exhaustion are defined.

  Ordinary traversal performs bounded local and cross-page validation; the full verifier owns
  global reachability, unique-parent, sibling, free/live, and orphan checks. A malformed checksum-
  valid page cannot be silently trusted merely because checksum validation succeeded.

  20. Catalog: coherent

  All six built-in descriptors are reconstructible exactly:

  - sys_tables
  - sys_columns
  - sys_indexes
  - sys_index_columns
  - sys_constraints
  - sys_statistics

  IDs, ColumnIds, ordinals, TypeIds, nullability, defaults, ownership, references, self-
  description, bootstrap fixed point, MVCC, DROP, and cache reconstruction are explicit.

  21. Scalar semantics: coherent

  The closed v1 registry fixes literals, operators, promotion, overflow, integer division/
  remainder, FLOAT64 behavior, comparisons, NULL/3VL, casts, VARCHAR, DATE/TIMESTAMP, CASE, IN-
  list, formatting/parsing, constant folding, hashing, grouping, joins, and UNIQUE normalization.

  Scalar binary FLOAT64 operations retain expression-tree rounding points; aggregate SUM/AVG
  intentionally use separate exact n-ary semantics.

  22. Aggregate semantics: coherent

  The overload registry, NULL/empty behavior, result TypeIds, states, Merge, Finalize, overflow,
  special values, canonical representations, spill state, and execution-shape equivalence are
  closed.

  Argument-expression errors remain Chapter-17 errors before value admission. COUNT overflow
  cannot suppress later demanded argument errors. Aggregate numerical finalization is
  deterministic across groups, workers, and physical algorithms.

  23. SELECT/no-FROM: coherent

  Optional FROM, the one-row zero-column source, name scope, wildcard rejection, WHERE,
  aggregation, GROUP BY/HAVING, DISTINCT, ORDER BY, LIMIT/OFFSET, derived tables, and supported
  subqueries compose through the ordinary canonical SELECT plan.

  No unsupported syntax or implicit relation was introduced.

  24. Subqueries: coherent

  The supported set remains:

  - uncorrelated scalar
  - EXISTS / NOT EXISTS
  - single-column IN / NOT IN
  - derived tables

  Correlation, OuterRef/Apply, row-valued forms, quantified comparisons, LATERAL, CTE/set
  operations, and data-modifying subqueries remain unsupported.

  Lazy at-most-once evaluation, snapshot/CommandId sharing, cardinality errors, EXISTS projection
  suppression, NOT IN 3VL, spill, and rewrite legality are explicit.

  25. Optimizer: coherent

  Estimated zero and semantic emptiness remain separate. Only approved exact proof provenance
  permits elimination. Rewrites preserve 3VL, short-circuit errors, outer-join preservation,
  global aggregate behavior, subquery demand, and exact aggregate semantics.

  required_rows remains costing/search metadata, not a semantic LIMIT.

  26. Execution/memory/spill: coherent

  Vector ownership, borrowed VARCHAR lifetime, selection masks, pipeline dependencies, blocking
  finalization, joins, aggregation, sorting, subquery side plans, DML spooling, RETURNING,
  cancellation, and error cleanup are explicit.

  Exact aggregate state may be variable-sized and query-accounted. Resource exhaustion produces a
  controlled query failure; it never authorizes approximation.

  27. Statistics: coherent

  The complete ANALYZE-to-estimator path is deterministic:

  - fixed snapshot and immutable manifest
  - exact 2^-40 probability tolerance
  - exact dyadic summation
  - strict individual domains
  - MCV uniqueness/canonical order
  - histogram ordering and degeneracies
  - NDV/width/count constraints
  - generation-atomic validation
  - StatsVersion/cache monotonicity
  - DROP-safe publication
  - older-generation or missing-statistics fallback

  Validated statistics remain costing-only and cannot prove semantic emptiness.

  28. Maintenance/retirement: coherent

  Same-table VACUUM is serialized; cross-table VACUUM and VACUUM/ANALYZE may run concurrently
  under their documented protections. ANALYZE/DROP and manifest changes linearize through
  STATS_PUBLISH/MANIFEST_CHANGE. Status reclamation uses one guarded cutoff coordinator.

  DRAINING and NONCONTINUABLE rules prevent new maintenance mutation while allowing required
  terminal cleanup.

  29. Exhaustion: coherent

  Checked-next semantics cover TxnId, CommandId, object IDs, FileId, PageNo, SlotId, LSN, WAL
  records/segments, terminal credits, schema versions, runtime generations, read epochs,
  statistics chunks, and B+ height.

  No checked boundary requires wrap, sentinel reuse, host overflow, or a value wider than its
  persisted carrier.

  30. Lifecycle: coherent

  CLOSED, OPENING, RECOVERING, READY, DRAINING, CLOSING, and NONCONTINUABLE have exact admission
  and transition rules.

  The process-associated fcntl restriction is explicitly handled: one retained control descriptor
  owns the lock, no second same-inode descriptor is independently opened, same-process aliases are
  blocked, and lock release occurs after all database users/descriptors drain. Fork/exec behavior
  is also explicit.

  31. Crash matrix: coherent

   Crash point                           Next-open interpretation
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Ordinary DML before durable commit    loser/ABORTED; physical garbage permitted
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Durable COMMIT                        COMMITTED, even if client acknowledgement was absent
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   ABORT                                 ABORTED directly or through loser resolution
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Heap append before publication        unpublished tail removed/reconciled
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Complete B+ MTR                       replayed atomically; incomplete record ignored as tail
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Status mutation                       image plus terminal record ordering reconstructs one
                                         outcome
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Checkpoint/control update             newest independently usable generation selected
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   CREATE publication                    committed catalog requires already-durable final files;
                                         otherwise orphan
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   DROP                                  catalog liveness decides semantics; physical unlink
                                         remains delayed/retryable
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   ANALYZE                               only complete committed StatsVersion is selectable
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   VACUUM                                completed units survive; incomplete provisional unit
                                         disappears/restores
  ────────────────────────────────────  ──────────────────────────────────────────────────────────
   Shutdown                              failure never claims successful clean close; next open
                                         performs recovery

  32. Error taxonomy: coherent

  Bind/type errors, scalar arithmetic failures, aggregate overflow, deadlock victim, statement-
  recoverable failure, MUST_ABORT, uncertain commit outcome, corruption, unsupported format,
  resource exhaustion, disk full, and database noncontinuability remain distinct.

  Ordinary deadlock is transaction-fatal, not database-fatal. Aggregate overflow is a numerical
  execution failure, not corruption.

  ## Historical issue-family status

  33. A-issue status

   Issue    Status
  ━━━━━━━  ━━━━━━━━━━
   A-001    RESOLVED
  ───────  ──────────
   A-002    RESOLVED
  ───────  ──────────
   A-003    RESOLVED

  34. M-issue status

   Issues                 Status
  ━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━
   M-001 through M-014    RESOLVED

  The current architecture independently contains the required object lifetime, catalog, UNIQUE,
  failure, scalar, subquery, exhaustion, statistics-version, maintenance, fallback, numerical-
  validation, and lifecycle contracts.

  35. D-issue status

   Issue    Status
  ━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   D-001    RESOLVED — terminal cache and TXN_STATUS page authority are distinct
  ───────  ──────────────────────────────────────────────────────────────────────────────
   D-002    RESOLVED — prune_hint initializes to zero; every uint32 is non-authoritative
  ───────  ──────────────────────────────────────────────────────────────────────────────
   D-003    OBSOLETE — creation_epoch is explicitly opaque and correctness-independent
  ───────  ──────────────────────────────────────────────────────────────────────────────
   D-004    RESOLVED — no stale rewrite/provenance authority remains

  36. SA-issue status

   Issue     Status
  ━━━━━━━━  ━━━━━━━━━━
   SA-001    RESOLVED
  ────────  ──────────
   SA-002    RESOLVED
  ────────  ──────────
   SA-003    RESOLVED
  ────────  ──────────
   SA-004    RESOLVED

  37. Remaining correctness-relevant implementation choices: NONE.

  Remaining choices concern internal containers, mutex organization, scheduling/fairness policy,
  worker count, cache layout, exact-accumulator representation, spill strategy, and calibrated
  costing. All are constrained to preserve the defined observable behavior.

  38. Sections requiring repair before freeze: NONE.
  39. Fresh-team implementability: YES. A fresh implementation team can proceed from the
     architecture without inventing correctness-relevant semantics.

  40. Files modified: NONE. Final git status --short produced no output; git diff --check passed
     with no output.

  41. Production code/tests: untouched.
  42. Phase boundary: Implementation Phase 2 was NOT entered or authorized. It still requires
     separate explicit user authorization.