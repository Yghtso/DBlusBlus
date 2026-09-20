# Chapter 37 Verification synchronization report

  ## 1–8. Repository state and synchronization scope

  - Initial/final HEAD: dbb98762d4cf55f94de8248ad596595234c15333
  - Commit: applied FIX-b 37 in ARCHITECTURE
  - Initial worktree/index: clean
  - Final worktree: M docs/VERIFICATION.md
  - Final index: clean
  - Modified files: docs/VERIFICATION.md only
  - Diff: 296 insertions, 0 deletions
  - Single diff hunk: insertion after V36 at former line 24259
  - No existing procedure was rewritten

  Live V37 section:

  - Title: Chapter 37 — Physical Properties, Join Enumeration, and Subquery-Planning Verification
  - Start: docs/VERIFICATION.md:24260
  - End: line 24555
  - Existing Control-Operator Tests resume at line 24556

  Families:

  1. V37-A — Search event, evidence, and nonvacuity
  2. V37-B — Ordering identity, normalization, providers, and interesting orders
  3. V37-C — Required slots, memo identity, BindingId, and RelationSet
  4. V37-D — Search configuration, threshold, and resource handoff
  5. V37-E — Join graph, predicate ownership, and Cartesian admission
  6. V37-F — Exhaustive and bounded join search
  7. V37-G — Algorithms, orientations, cardinality, and cost handoff
  8. V37-H — Expression-subquery physical planning
  9. V37-I — Errors, validation, differential correctness, and frozen regressions
  10. V37-J — Static integrity and complete evidence

  Atomic definitions:

  - IDs: V37-001 through V37-070
  - Count: 70
  - Unique: 70
  - Missing IDs: none
  - Duplicate definitions: none

  ## 9–12. Instrumentation and nonvacuity

  The central V37 ledger correlates:

  - optimizer and logical-subproblem identity;
  - BindingIds, RelationSets, graph predicates, and regions;
  - descriptors and StatsVersions;
  - slots, raw/normalized ordering, and row objectives;
  - retained search configuration and capabilities;
  - modes, subsets, partitions, Cartesian decisions, and predicate ownership;
  - algorithms, orientations, cardinality, cost, dominance, and ties;
  - resource guards, fallback, selection, and validation;
  - subquery occurrence, attempt, initialization, demand, and consumption.

  V37-001 supplies the positive typed-query integration control. V37-002 compares it against an
  independent set/graph/property model.

  V37-003 suppresses each essential evidence category independently. Missing evidence produces:

  NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE

  V37-004 requires proof that every injected fault, race, boundary, omission, or validator path was
  selected, reached, and triggered. Untriggered cases are failed setup.

  ## 13–25. Properties, identity, and search boundaries

  Coverage added for:

  - Four-field OrderKey identity.
  - Exact normalized-prefix satisfaction.
  - Left-to-right exact-duplicate normalization.
  - Metadata-only normalization and demanded-expression preservation.
  - Provided ordering for every baseline operator.
  - Capability-conditional ordering for optional algorithms.
  - Interesting-order retention and bounded property classes.
  - Required-slot closure, including hidden and DML slots.
  - Chapter-38 memo identity and exact row objectives.
  - Distinct self-join BindingIds.
  - Mathematical RelationSet operations.
  - 63-, 64-, 65-, and wider-than-native-word regions.
  - Deterministic partition symmetry.

  Search configuration procedures verify:

   Field                          Domain/default
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   exhaustive_join_limit          integer ≥ 0; default 10; zero valid
  ─────────────────────────────  ─────────────────────────────────────
   large_join_max_local_passes    integer ≥ 0; default 4; zero valid

  Missing, negative, wrapped, clamped, or unrepresentable values cannot enter search.

  The exact threshold/resource matrix includes:

  - N=9, limit 10 → exhaustive.
  - N=10, limit 10 → exhaustive.
  - N=11, limit 10 → heuristic from outset.
  - N=1, limit 0 → heuristic.
  - Proximity alone → no fallback.
  - Canonical guard trigger → bounded fallback.
  - Bounded fallback exhaustion → OptimizerResourceLimit.
  - Separate regions → independently selected modes.
  - Mid-invocation external changes → retained configuration remains authoritative.

  Configuration, planning resources, plan cost, RelationSet defects, and runtime memory failures
  remain separate owners.

  ## 26–40. Graph, search, algorithms, and cardinality

  The new procedures cover:

  - One graph vertex per BindingId.
  - Exact referenced sets for local, binary, and multi-relation predicates.
  - Safe versus unsafe equality derivation.
  - R ⊆ S activation.
  - Crossing predicates intersecting both children.
  - One lowest legal predicate owner per candidate tree.
  - Error, UNKNOWN, and LEFT-boundary demand safety.
  - Chapter-35 selectivity and Chapter-36/38 work ownership.

  Cartesian cases include:

  - Connected chain exclusion of {A,C}.
  - Hyperedge-only prerequisite assembly.
  - Overlapping hyperedges.
  - Disconnected complete-component assembly.
  - Explicit CROSS JOIN.
  - LEFT-boundary rejection.

  Search procedures independently observe:

  - singleton initialization;
  - every admitted subset and partition;
  - bushy alternatives;
  - deterministic symmetry elimination;
  - greedy construction;
  - stable ties;
  - zero/positive pass budgets;
  - first non-improving-pass termination;
  - exact pass-budget exhaustion;
  - optional beam absence;
  - legal bounded approximation.

  Algorithm coverage includes:

  - joint tree/algorithm enumeration;
  - HashJoin, NLJ, eligible INLJ, and capability-enabled MergeJoin;
  - both supported INNER hash orientations;
  - canonical LEFT hash orientation;
  - capability rejection despite attractive cost;
  - one algorithm-independent logical cardinality;
  - estimated zero remaining non-semantic;
  - complete Chapter-35/36/38 handoff.

  ## 41–47. Subquery planning

  Procedures cover:

  - Distinct same-text occurrences.
  - At-most-once initialization per occurrence and attempt.
  - Demand-conditional execution.
  - Reuse within an attempt and fresh retry state.
  - Scalar zero/one/two-row behavior and demanded second-row errors.
  - EXISTS first-row demand and legal early stop.
  - IN/NOT IN complete build, markers, duplicates, spill, errors, and 3VL.
  - Exact-proof plus capability gating for optional rewrites.
  - Mandatory independent fallback.
  - Correlation rejection before physical costing.
  - Literal-sensitive planning without prepared-statement requirements.

  ## 48–50. Errors, validation, differential and frozen regressions

  V37-063 distinguishes configuration, invariant, eligibility, optimizer-resource, runtime-resource,
  scalar-cardinality, and ordinary SQL errors.

  V37-064 requires the final validator to observe and reject actual offered candidates with:

  - false ordering;
  - missing slots;
  - illegal orientation;
  - unavailable capability;
  - unowned predicates;
  - invalid objective metadata.

  V37-065 provides differential correctness over bags, NULL/UNKNOWN, ordering, visibility, demanded
  errors, scalar cardinality, subquery state, and DML effects.

  Frozen integration:

  - Chapter 31: V37-066
  - Chapter 32: V37-067
  - Chapters 33–34: V37-068
  - Chapters 35–36: V37-069

  No frozen suite was duplicated or rewritten.

  ## 51. Adversarial matrix

  All 52 cases A–AZ have explicit procedure, owner, fixture, result, reuse, and COMPLETE status.

   Range    Coverage
  ━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   A–K      Slot/order identity, prefix, providers, normalization, demand
  ───────  ────────────────────────────────────────────────────────────────────────────
   L–O      Required slots, DML RID, memo objective, native-word capacity
  ───────  ────────────────────────────────────────────────────────────────────────────
   P–V      Configuration, threshold, resource guard, regional modes
  ───────  ────────────────────────────────────────────────────────────────────────────
   W–Z      Self-joins, activation, ownership, safe derivation
  ───────  ────────────────────────────────────────────────────────────────────────────
   AA–AD    Connected, hyperedge, overlapping, disconnected Cartesian cases
  ───────  ────────────────────────────────────────────────────────────────────────────
   AE–AJ    Bushy completeness, symmetry, deterministic heuristic, passes, outer moves
  ───────  ────────────────────────────────────────────────────────────────────────────
   AK–AP    Capabilities, joint algorithms, orientations, cardinality, estimated zero
  ───────  ────────────────────────────────────────────────────────────────────────────
   AQ–AW    Subquery occurrence, retry, scalar, EXISTS, IN, rewrites, correlation
  ───────  ────────────────────────────────────────────────────────────────────────────
   AX–AZ    Slots/properties, final validation, fixed-input determinism

  Capability-conditional cases remain conditional. No global-optimum or mandatory optional-algorithm
  requirement was introduced.

  ## 52. All-15-invariant matrix

   Invariant                                V37 coverage        Status
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━
   1. Small property system                 005, 015, 017       COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   2. Four-part prefix                      005–007             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   3. Runtime-guaranteed ordering           009–011             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   4. Normalization/interesting orders      007–008, 012–014    COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   5. BindingId identity                    019–022             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   6. Exhaustive bushy search               041–043             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   7. Threshold/default/resource handoff    023, 025–030        COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   8. Greedy/pass budget                    024, 044–047        COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   9. Necessary Cartesian admission         037–040, 046        COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   10. Joint order/algorithm/orientation    048–050             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   11. LEFT boundaries/orientation          036, 046, 051       COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   12. Algorithm-independent cardinality    052–053             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   13. Capability gating                    010–011, 049        COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   14. Correlation/fallback                 055–061             COMPLETE
  ───────────────────────────────────────  ──────────────────  ──────────
   15. Predicate availability/ownership     033–036             COMPLETE

  ## 53–55. Subsection and downstream matrices

  All §§37.1–37.18 are covered. Combined rows for §§37.2–37.3 and §§37.14–37.15 account for all 18
  subsections.

  Chapter-38 handoffs covered:

  - §38.1 and §§38.3–38.7: memo identity, dominance, ties, and DP.
  - §38.21: threshold/resource handoff.
  - §38.24: final validation.

  Chapter 41/42 treatment:

  - §41.7 controlled join-search/property scenarios are mapped to V37 procedures.
  - §42.6 is used only as measurement context.
  - No benchmark accuracy threshold or benchmark execution claim was added.

  ## 56–58. Reuse and integrity

  External procedure-reference sets:

  - Actual used set: 70 unique IDs
  - Declared inventory: 70 unique IDs
  - Actual minus inventory: none
  - Inventory minus actual: none
  - Duplicate inventory entries: none
  - Broken IDs: none

  Exact inventory:

  V19-2 V19-14 V19-18
  V20-6 V20-10 V20-12 V20-13 V20-14 V20-15 V20-16 V20-20 V20-21
  V22-B V22-C V22-D V22-H V22-I V22-J V22-K V22-L
  V27-G V27-N
  V28-B V28-G V28-M V28-N V28-O V28-T
  V29-O
  V30-C V30-D V30-K
  V31-A V31-B V31-E V31-G V31-H V31-N
  V32-A V32-B V32-C V32-H V32-I V32-J
  V33-A V33-C V33-D V33-E V33-F V33-G V33-H V33-I V33-K V33-N
  V34-B V34-C V34-D V34-J V34-K
  V35-A V35-B V35-E V35-G V35-H V35-I V35-J
  V36-A V36-H V36-I V36-J

  Named reusable headings:

  - Actual used: 12
  - Declared: 12
  - Missing/extras: none
  - All resolve exactly once.

  Static checks:

  - Families: 10 unique.
  - Atomic definitions: 70 unique and contiguous.
  - Invariant rows: 15.
  - Adversarial rows: 52.
  - Orphan definitions or matrices: none found.
  - V37-070 requires future static validation to fail on missing evidence or false coverage.

  ## 59–63. Document role and final checks

  The added section is:

  - timeless;
  - procedural;
  - independently falsifiable;
  - implementation-independent;
  - faithful to Architecture.

  It introduces no new SQL semantics, operators, property dimensions, persisted metadata, comparator,
  cost formula, configuration field, error enum, proof authority, beam requirement, global-optimality
  guarantee, concrete API, or execution claim.

  Remaining coverage gaps: none identified

  New Architecture or semantic questions: none

  git diff --check: PASS

  Final repository state:

  HEAD:
      dbb98762d4cf55f94de8248ad596595234c15333

  WORKTREE:
       M docs/VERIFICATION.md

  INDEX:
      clean

  ## 64. Explicit confirmations

  ARCHITECTURE NOT MODIFIED
  DEVELOPMENT NOT MODIFIED
  PROJECT_STATE NOT MODIFIED
  CHAPTERS 31–36 NOT MODIFIED
  CHAPTER 38 NOT MODIFIED
  HISTORICAL ARTIFACTS NOT MODIFIED
  IMPLEMENTATION NOT STARTED
  NO BUILD/TEST/SANITIZER/BENCHMARK
  NO STAGING/COMMIT
  CHAPTER 38 REVIEW NOT STARTED

  No procedure was executed, and this synchronization does not independently close Chapter-37
  Verification.

  CHAPTER 37 VERIFICATION SYNCHRONIZATION COMPLETE —
  READY FOR INDEPENDENT READ-ONLY VERIFICATION
  CLOSURE AUDIT

  END CHAPTER-37 VERIFICATION SYNCHRONIZATION.
