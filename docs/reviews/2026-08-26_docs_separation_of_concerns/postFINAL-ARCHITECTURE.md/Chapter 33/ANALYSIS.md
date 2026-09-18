# Chapter 33 Initial Architecture Review

  1. Initial review verdict: NEEDS ARCHITECTURE FIX

  Chapter 33 is broadly coherent, but it contains one material conflict with bounded optimizer search, one localized planning-input omission, and one editorial chronology cluster.

  2. Initial HEAD/status

  - HEAD: 2347c550d2e2c9d2d5911d183a4fe140c43f0911
  - Commit: 2347c55 synced VERIFICATION after chapter 32 ARCHITECTURE fix
  - Working tree: clean
  - Index: clean

  The HEAD commit contains:

  - M docs/VERIFICATION.md
  - A docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 32/SYNC.md

  Those are pre-existing committed changes, not audit changes.

  3. Final HEAD/status

  - HEAD unchanged: 2347c550d2e2c9d2d5911d183a4fe140c43f0911
  - Working tree: clean
  - Index: clean

  4. Audit-created changes: NONE.
  5. Chapter 33 boundaries and inventory

  Chapter 33 occupies docs/ARCHITECTURE.md:24121 lines 24121–24320. Chapter 34 begins at line 24322.

   Section                                  Line
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━
   # 33. Optimizer Architecture            24121
  ──────────────────────────────────────  ───────
   §33.1 Role                              24123
  ──────────────────────────────────────  ───────
   §33.2 Layering                          24179
  ──────────────────────────────────────  ───────
   §33.3 Planning inputs                   24217
  ──────────────────────────────────────  ───────
   §33.4 Stable catalog/statistics view    24241
  ──────────────────────────────────────  ───────
   §33.5 PhysicalPlan output               24253
  ──────────────────────────────────────  ───────
   §33.6 Cost-based choice rule            24283
  ──────────────────────────────────────  ───────
   §33.7 Optimizer invariants              24308
  ──────────────────────────────────────  ───────
   Chapter 34 boundary                     24322

  §33.7 contains nine numbered invariants.

  6. Canonical owner matrix

   Concern                                                           Canonical owner
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━
   Catalog identity, visibility, descriptor lifetime                 Chapter 16
  ────────────────────────────────────────────────────────────────  ─────────────────
   SQL types, NULL, comparison, coercion                             Chapter 17
  ────────────────────────────────────────────────────────────────  ─────────────────
   Binding and resolved identity                                     Chapter 19
  ────────────────────────────────────────────────────────────────  ─────────────────
   Logical bags, demand, rewrites, semantic emptiness propagation    Chapter 20
  ────────────────────────────────────────────────────────────────  ─────────────────
   DML/control semantics                                             Chapter 21
  ────────────────────────────────────────────────────────────────  ─────────────────
   Immutable physical plans and capability registry                  Chapter 22
  ────────────────────────────────────────────────────────────────  ─────────────────
   Values, memory, spill, execution lifecycle                        Chapters 23–26
  ────────────────────────────────────────────────────────────────  ─────────────────
   Operator semantics                                                Chapters 27–30
  ────────────────────────────────────────────────────────────────  ─────────────────
   DML/W/C/R and control coordinators                                Chapter 31
  ────────────────────────────────────────────────────────────────  ─────────────────
   Parallel execution                                                Chapter 32
  ────────────────────────────────────────────────────────────────  ─────────────────
   Statistics versions and publication                               Chapter 34
  ────────────────────────────────────────────────────────────────  ─────────────────
   Cardinality, width, fallback, semantic-emptiness boundary         Chapter 35
  ────────────────────────────────────────────────────────────────  ─────────────────
   Costs and base access paths                                       Chapter 36
  ────────────────────────────────────────────────────────────────  ─────────────────
   Properties and join enumeration                                   Chapter 37
  ────────────────────────────────────────────────────────────────  ─────────────────
   Memo, dominance, bounded search, final validation                 Chapter 38
  ────────────────────────────────────────────────────────────────  ─────────────────
   Optimizer errors                                                  Chapter 39
  ────────────────────────────────────────────────────────────────  ─────────────────
   EXPLAIN and diagnostics                                           Chapter 40
  ────────────────────────────────────────────────────────────────  ─────────────────
   Verification obligations                                          Chapter 41

  7. Optimizer role assessment

  Mostly complete. Chapter 33 correctly requires a typed logical-plan input, forbids reparsing and execution by the optimizer, and produces one immutable PhysicalPlan. It does not mandate Cascades, Volcano, or
  a concrete class hierarchy.

  8. Layering and planning-stage assessment

  The conceptual flow is coherent:

  logical plan → normalization → analysis → statistics → estimates → access paths → join search → alternatives → property enforcement → final plan → pipeline builder.

  Detailed stage responsibilities remain correctly delegated to Chapters 20 and 35–38. No circular dependency was found.

  The final “lowest-cost” stage is overstrong; see N33-1.

  9. Logical semantic-preservation assessment

  Complete through Chapters 17, 20, 22, and 38. Legal plans must preserve:

  - types and NULL semantics;
  - bag multiplicity and occurrence identity;
  - required slots and ordering;
  - LEFT JOIN and aggregate behavior;
  - logical demand and observable errors;
  - snapshot and transaction semantics.

  Chapter 33 does not create a conflicting semantic model.

  10. Semantic-demand and error assessment

  Complete by delegation to §§20.17–20.20 and Chapter 39. Optimization cannot remove, duplicate, or move potentially erroring demanded evaluations without an exact demand-insensitivity proof. Statistics and
  cost are not sufficient proof.

  This preserves Chapter-31 ordinary DML error closure before W.

  11. Statistics-versus-semantic-facts assessment

  Complete. §33.4 explicitly prohibits deriving semantic emptiness from:

  - estimated rows == 0;
  - zero TRUE fraction;
  - a statistics descriptor.

  Chapters 20, 35, and 38 retain exact proof ownership.

  12. Stable catalog descriptor assessment

  Complete. §33.4 uses the caller’s catalog snapshot, while §16.10 guarantees immutable descriptor lifetime and permits older descriptors to remain alive. Concurrent DDL cannot mutate a descriptor already
  retained by a planner.

  13. Stable statistics snapshot assessment

  Complete through §33.4 and §§34.15–34.17. One optimization retains one immutable complete statistics descriptor generation. Concurrent ANALYZE may affect later planners but cannot produce a mixed StatsVersion
  plan.

  14. Planning input assessment

  One localized omission exists: §33.3 lists execution-memory budget and cost configuration but not the dedicated optimizer planning/search resource configuration required by §38.21. See N33-2.

  Statistics and runtime-memory estimates are otherwise correctly treated as planning inputs, not allocation guarantees.

  15. Physical property and RequiredSlotSet assessment

  Complete. Chapter 33 correctly delegates the exact taxonomy to Chapters 22 and 37:

  - OrderingProperty
  - RequiredSlotSet

  It introduces no competing property system. Required output slots and final ORDER BY are explicit inputs. Final validation in §38.24 closes missing-slot and unsatisfied-order cases.

  16. Operator eligibility assessment

  Complete through §§22.4.1, 37.14, and 38.24. Cost may choose only implemented, capability-enabled, instance-eligible alternatives. Top-N must represent exact mathematical K; unavailable MergeJoin or
  SortAggregate cannot enter a final plan.

  17. Control-operator fast-path assessment

  Complete. §33.1 allows dedicated lowering for DDL, VACUUM, and ANALYZE only when no relational alternatives require search. Their canonical coordinators and publication protocols remain in Chapters 21, 31,
  and 34.

  18. Immutable PhysicalPlan assessment

  Complete with Chapters 22 and 38. The plan carries or exposes operator, child, side-plan, access-path, join, estimate, cost, memory, spill, and property metadata. Final validation prevents unavailable
  operators or unsatisfied requirements from reaching execution.

  19. Plan/execution-state separation

  Complete. §22.2 expressly excludes transaction state, snapshots, epochs, cursors, reservations, spill runs, cancellation, and worker-local state from immutable plans.

  20. Cost-based choice-rule assessment

  The fixed-threshold prohibition and cost-factor list are correct. The unqualified final stage “lowest-cost valid PhysicalPlan” conflicts with canonical bounded search. See N33-1.

  21. Base access-path assessment

  Complete through Chapter 36. Every non-proven-empty LogicalGet retains SeqScan and each semantically usable IndexScan alternative. Selection is cost-driven; an index’s existence does not force its use.

  22. Missing/stale-statistics assessment

  Complete. Missing, invalid, incomplete, or stale statistics use the canonical fallback. They may degrade plan quality but cannot change SQL correctness, establish emptiness, or make ANALYZE a query
  prerequisite.

  23. Join-enumeration assessment

  Semantically complete through Chapter 37:

  - INNER/CROSS reorderable regions;
  - constrained LEFT JOIN boundaries;
  - required Cartesian products;
  - bushy DP below the active bound;
  - deterministic heuristic search above it;
  - algorithm and orientation eligibility.

  Chapter 33’s global-looking lowest-cost phrase is the only inconsistency.

  24. Memo/dominance/search-budget assessment

  Memo identity, dominance, ties, interesting orders, objectives, and bounded search are fully specified by Chapter 38. Chapter 33 should explicitly defer final choice to the alternatives retained by that
  canonical bounded search.

  25. Lazy-subquery assessment

  Complete through Chapters 20, 22, 26, and 37. Scalar, EXISTS, and IN modes remain distinct and lazy. Costing metadata cannot force eager execution or erase cardinality/error behavior.

  26. Optimizer resource/error ownership assessment

  §§38.21 and 39.4 clearly distinguish:

  - planner arena/search limits;
  - execution-memory estimates and budget;
  - actual runtime allocation;
  - OptimizerResourceLimit;
  - ordinary OOM and runtime spill failures.

  Chapter 33’s input list should expose the planning-resource input explicitly, but no new error policy is needed.

  27. Executor handoff assessment

  Complete. The executor consumes the finalized immutable plan, constructs per-execution state separately, and does not silently reinterpret or ordinarily replan the query.

  28. Parallel-runtime regression

  No conflict found. Chapter 32 continues to own source-domain coverage, exclusive claims, worker-local state, and scheduling. Worker count cannot change SQL semantics or logical occurrence multiplicity.

  29. Chapter-31 DML regression

  No conflict found. Planning may parallelize eligible relational evaluation, but:

  - mutation publication remains single-worker;
  - ordinary DML errors close before W;
  - no post-W retry is introduced;
  - W/C/R remains unchanged.

  30. DDL/VACUUM/ANALYZE regression

  No conflict found. Dedicated lowering does not replace:

  - DDL coordination;
  - VACUUM maintenance ownership;
  - ANALYZE’s single publication coordinator.

  31. Document-role assessment

  Chapter 33 is mostly timeless Architecture. N33-3 identifies several roadmap-style formulations that should be recast as stable capability boundaries.

  32. Complexity assessment

   Area                                           Classification
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Typed-plan-to-physical-plan boundary           CORE
  ─────────────────────────────────────────────  ──────────────────────────────────
   Stable descriptors, properties, eligibility    CORE
  ─────────────────────────────────────────────  ──────────────────────────────────
   System-R-style DP and small memo               JUSTIFIED ADVANCED
  ─────────────────────────────────────────────  ──────────────────────────────────
   Bounded large-join heuristic                   JUSTIFIED ADVANCED
  ─────────────────────────────────────────────  ──────────────────────────────────
   Full Cascades/Volcano framework                Correctly excluded from baseline
  ─────────────────────────────────────────────  ──────────────────────────────────
   Runtime adaptive replanning                    Correctly excluded from baseline
  ─────────────────────────────────────────────  ──────────────────────────────────
   Possible overengineering                       None identified

  33. Mandatory thought-experiment matrix

   Case                                                Owner and required outcome                                                                   Assessment
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   A. ANALYZE publishes mid-plan                       §§34.15–34.17: current planner retains old complete descriptor; later planner may use new    Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   B. DDL commits mid-plan                             §16.10/Ch21: retained caller-visible descriptor remains valid                                Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   C. Estimate zero, actual nonzero                    §§35.2, 38.24: retain executable path; no empty proof                                        Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   D. Exact empty proof                                §§20.17.10, 35.2: empty replacement allowed only with valid provenance and demand safety     Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   E. Missing/invalid stats                            Ch34/35/38: older valid or missing fallback; correctness unchanged                           Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   F. Top-N cannot represent K                         §§22.4.1, 30.7, 38.15: Top-N ineligible; exact fallback retained                             Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   G. MergeJoin/SortAggregate absent                   §§22.4.1, 37.14: omit alternative; retain baseline                                           Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   H. Index exists, SeqScan cheaper                    Ch36: choose SeqScan by cost                                                                 Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   I. Index chosen from live residency                 §§33.3, 36.1: forbidden                                                                      Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   J. Ordering met but slot dropped                    §§37.4, 38.24: reject final plan                                                             Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   K. Parallel SeqScan treated ordered                 §§32.4, 37.5: no order claim; enforce order separately                                       Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   L. HashJoin changes LEFT multiplicity               Ch20/28/38: illegal and rejected                                                             Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   M. Outer-join boundary crossed                      §37.9: forbidden absent an exact prior rewrite                                               Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   N. Lazy subquery eagerly executed                   §§20.14, 37.17: preserve lazy fallback/demand                                                Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   O. Demanded error dropped                           §20.17: rewrite rejected without exact proof                                                 Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   P. Planner exceeds resource budget                  §38.21/§39.4: bounded fallback, then OptimizerResourceLimit                                  Semantics resolved; §33.3 input omission
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   Q. Runtime exceeds estimate                         Ch24/39: actual allocation/spill owner applies; estimate is no guarantee                     Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   R. Final plan uses unavailable operator             §§22.4.1, 38.24: reject before execution                                                     Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   S. Plan embeds snapshot/cursor                      §22.2: invalid immutable-plan state                                                          Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   T. Alternatives ranked differently                  §§38.3–38.4/38.21: active objective, deterministic ties, bounded explored set                N33-1 conflict
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   U. Control statement enters needless join search    §33.1: dedicated lowering permitted                                                          Resolved
  ──────────────────────────────────────────────────  ───────────────────────────────────────────────────────────────────────────────────────────  ──────────────────────────────────────────
   V. Parallel DML worker publishes mutation           Chapters 31–32: forbidden; single mutation coordinator                                       Resolved

  34. Global contradiction-search results

  - TRUE CONTRADICTION: §33.1’s unqualified “lowest-cost valid PhysicalPlan” versus §§37.11, 37.13, and 38.21 bounded search.
  - VALID DIFFERENT OWNER: catalog/statistics consistency in Chapters 16 and 34.
  - VALID DIFFERENT STAGE: logical normalization versus physical selection; planning estimates versus runtime allocation.
  - VALID OPTIONAL CAPABILITY: MergeJoin, SortAggregate, work beyond the baseline property system, and adaptive optimization.
  - NON-NORMATIVE/EDITORIAL: roadmap phrasing identified by N33-3.
  - No contradiction with Chapters 31 or 32 was found.

  35. Existing Verification reuse inventory

  Applicable live procedures and headings include:

  - V20-15 through V20-20, V20-22: demand, rewrite safety, proof, logical validation, logical/physical ownership.
  - V22-A through V22-E, V22-H through V22-L: immutable handoff, slots, properties, capabilities, runtime ownership, control roles, proof/estimate separation, Top-N, final validation.
  - Physical-Plan Validator Tests.
  - Control-Operator Tests.
  - Statistics Publication and Versioning Tests, especially “One stable statistics snapshot per optimization.”
  - Semantic Emptiness Tests.
  - Access Path Tests.
  - Join-Order Tests.
  - Physical Property and Enforcement Tests.
  - Memory/Spill Plan Tests.
  - Memo and Pruning Tests.
  - Cost Model Tests.
  - Optimizer Determinism and Resource-Limit Tests.
  - Final Optimizer Validation Tests.
  - Optimizer Differential Correctness Tests.
  - Optimizer Fuzzing.
  - Optimizer Diagnostics Tests.
  - Parallel Execution Tests.
  - Closed V31/V32 procedures for DML publication and parallel-runtime regression.

  No relevant broken reference was observed in this read-only readiness inspection.

  36. Missing Chapter-33 Verification inventory

  A later V33 synchronization should add focused integration procedures for:

  - complete bound-logical-plan-to-immutable-physical-plan stage handoff;
  - combined catalog/statistics snapshot coherence during concurrent DDL and ANALYZE;
  - planning-resource budget versus execution-memory budget;
  - bounded-search result selection versus global-optimum claims;
  - complete PhysicalPlan inspectable metadata;
  - absence of mutable execution state in optimizer output;
  - poisoned optimizer execution and executor replanning entry points;
  - dedicated control lowering without bypassing relational-child validation;
  - lazy side-plan metadata and demand preservation;
  - end-to-end capability/property/final-validation composition.

  37. BLOCKING findings: 0.
  38. MAJOR findings: 1.

  N33-1 — MAJOR

  - Location: §33.1, line 24160.
  - Canonical owners: §§37.11, 37.13, 38.3–38.4, 38.15, and 38.21.
  - Defect: “lowest-cost valid PhysicalPlan” is unqualified and reads as requiring a global minimum across all legal plans, while the canonical optimizer deliberately switches to bounded heuristic search.
  - Scenario: a join region exceeds the exhaustive limit and a lower-cost legal plan exists outside the heuristic’s explored alternatives.
  - Consequence: one implementation may exhaustively search to satisfy §33.1, violating resource bounds; another may return the best retained heuristic plan, apparently violating §33.1.
  - Smallest repair: qualify the output as the lowest-cost legal retained alternative under the active objective among alternatives admitted/explored by the canonical Chapter-37/38 search, including bounded
    fallback.

  39. MINOR findings: 1.

  N33-2 — MINOR

  - Location: §33.3, lines 24219–24228.
  - Canonical owners: §38.21 and §39.4.
  - Defect: the planning input list includes query execution-memory budget but omits the separate optimizer planning/search resource configuration.
  - Scenario: execution memory is fixed while the planning-arena bound is reduced.
  - Consequence: implementations may conflate budgets, ignore the planning bound, or disagree about heuristic fallback and OptimizerResourceLimit.
  - Smallest repair: add the dedicated planning-time/memory/search-budget configuration and explicitly distinguish it from execution memory and runtime estimates.

  40. EDITORIAL findings: 1.

  N33-3 — EDITORIAL

  - Locations: §33.1 lines 24175–24177; §33.2 line 24215; §33.3 line 24237; §33.7 line 24317.
  - Canonical owner: Chapter 33’s stable baseline capability boundary, with Chapter 37’s property taxonomy.
  - Defect: phrases such as “deferred,” “before introducing,” “may be added later,” and “future work” describe roadmap chronology.
  - Consequence: no semantic ambiguity, but the live Architecture becomes a development roadmap.
  - Smallest repair: state timelessly that the baseline does not require full Cascades/Volcano, adaptive reoptimization, or additional tracked properties; those capabilities are outside the defined baseline.

  41. DESIGN-SCOPE questions: 0.
  42. FROZEN SEMANTIC questions: 0.

  No Chapter-31 or Chapter-32 contract needs reopening.

  43. Exact next Architecture action

  Perform one narrowly scoped Chapter-33 Architecture repair:

  - qualify §33.1’s cost-choice endpoint against bounded canonical search;
  - add the distinct planning-resource input in §33.3;
  - replace the identified roadmap phrases with timeless baseline/optionality wording.

  No cross-chapter edit is required.

  44. Recommended next authorized task

  CHAPTER 33 — ARCHITECTURE FIX A, limited to N33-1 through N33-3. After that repair, conduct a focused read-only Chapter-33 closure audit. Do not synchronize Chapter-33 Verification until Architecture closes.

  45. git diff --check result: PASS; no output.
  46. Final repository-state confirmation
  CHAPTER 31 ARCHITECTURE:
  CLOSED / UNMODIFIED

  CHAPTER 31 VERIFICATION:
  CLOSED / UNMODIFIED

  CHAPTER 32 ARCHITECTURE:
  CLOSED / UNMODIFIED

  CHAPTER 32 VERIFICATION:
  CLOSED / UNMODIFIED

  CHAPTER 33 ARCHITECTURE:
  NEEDS ARCHITECTURE FIX

  CHAPTER 33 VERIFICATION:
  NOT SYNCHRONIZED

  CHAPTER 34 REVIEW:
  NOT STARTED

  IMPLEMENTATION:
  NOT STARTED

  BUILD/TEST/SANITIZER/BENCHMARK:
  NOT RUN

  AUDIT-CREATED CHANGES:
  NONE

  END CHAPTER-33 INITIAL READ-ONLY ARCHITECTURE REVIEW.
