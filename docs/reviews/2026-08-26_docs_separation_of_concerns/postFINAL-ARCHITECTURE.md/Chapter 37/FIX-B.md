• # Chapter 37 Architecture Fix B report

  The previously applied, unstaged Fix-B repair remains intact. This repeated invocation made no
  additional edits.

  ## Repository and scope

  1. HEAD: 73b2957cc5f02c749a882e4d98fcb8b1b5dab9e6
  2. Commit: applied FIX-A 37 in ARCHITECTURE
  3. Initial status for this invocation: M docs/ARCHITECTURE.md
  4. Index: clean
  5. Historical review artifacts: unchanged
  6. Fix-B modified file: docs/ARCHITECTURE.md only
  7. Diff: 10 insertions, 12 deletions
  8. Additional changes from this repeated invocation: none

  Authorized locations changed:

  - §37.1 (docs/ARCHITECTURE.md:27243)
  - §37.5 (docs/ARCHITECTURE.md:27386)
  - §37.9 (docs/ARCHITECTURE.md:27526)
  - §38.21 (docs/ARCHITECTURE.md:28574)

  ## N37-1 — Threshold handoff

  The original contradiction allowed §38.21 to switch from exhaustive DP merely because search
  “approaches” exhaustive_join_limit, although §37.11 places equality on the exhaustive side.

  The final rule now states:

  - N <= exhaustive_join_limit: exhaustive DP is selected.
  - N > exhaustive_join_limit: bounded heuristic is selected from the outset.
  - Threshold proximity or equality does not interrupt admitted exhaustive DP.
  - Only the canonical planning-resource guard can trigger bounded fallback.
  - If bounded planning cannot fit, OptimizerResourceLimit remains authoritative.

  Boundary results:

   Case                              Result
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   N=9, limit 10                     Exhaustive DP
  ────────────────────────────────  ───────────────────────────────────────────────────
   N=10, limit 10                    Exhaustive DP
  ────────────────────────────────  ───────────────────────────────────────────────────
   N=11, limit 10                    Heuristic from outset
  ────────────────────────────────  ───────────────────────────────────────────────────
   N=1, limit 0                      Heuristic from outset
  ────────────────────────────────  ───────────────────────────────────────────────────
   Equality under any valid limit    Exhaustive unless resource guard triggers
  ────────────────────────────────  ───────────────────────────────────────────────────
   Near threshold, guard clear       No fallback
  ────────────────────────────────  ───────────────────────────────────────────────────
   Resource guard triggers           Bounded heuristic fallback
  ────────────────────────────────  ───────────────────────────────────────────────────
   Bounded fallback cannot fit       OptimizerResourceLimit
  ────────────────────────────────  ───────────────────────────────────────────────────
   Multiple regions                  Mode selected independently per region
  ────────────────────────────────  ───────────────────────────────────────────────────
   Mid-invocation config change      Retained configuration remains authoritative
  ────────────────────────────────  ───────────────────────────────────────────────────
   64/65+ bindings                   Exact RelationSet representation remains required

  Existing search-configuration domains, defaults, validation, invocation stability, local-pass
  behavior, and RelationSet capacity rules remain unchanged.

  N37-1: CLOSED

  ## N37-6 — Timeless wording

  The three replacements remain present:

  - §37.1 now describes partitioning, rewindability, and materialization as optional untracked
    extension categories.

  - §37.5 now refers to capability-enabled merge/ordered-aggregate implementations.
  - §37.9 now states that advanced outer-join reordering is outside the v1 baseline, while preserving
    independently proven Chapter-20 rewrites and capability gating.

  These edits preserve the small property system, runtime-guaranteed ordering, optional algorithms,
  LEFT JOIN nesting, and Chapter-20 semantic authority.

  - N37-5: PREVIOUSLY CLOSED / NOT REOPENED

  All 15 Chapter-37 invariants remain unchanged and coherent. Invariant 7 now agrees with §38.21;
  invariants 8, 9, and 15 remain intact.

  No regression was found in Chapters 31–36. Chapter 38 changes remain confined to §38.21. Chapter 39
  error/resource ownership is unchanged.

  Global consistency search found no remaining threshold-proximity fallback or related contradiction.

  ## Validation and final state

  - git diff --check: PASS
  - Worktree: M docs/ARCHITECTURE.md
  - Index: clean
  - No staging or commit
  - No tests, builds, sanitizers, benchmarks, or calibration
  - No new semantic question

  VERIFICATION NOT MODIFIED
  DEVELOPMENT NOT MODIFIED
  PROJECT_STATE NOT MODIFIED
  CHAPTERS 31–36 NOT MODIFIED
  CHAPTER 38:
      ONLY §38.21 THRESHOLD-HANDOFF TEXT MODIFIED
  CHAPTER 39 ONWARD NOT MODIFIED
  HISTORICAL ARTIFACTS NOT MODIFIED
  IMPLEMENTATION NOT STARTED
  NO BUILD/TEST/SANITIZER/BENCHMARK
  NO STAGING/COMMIT
  CHAPTER 38 INITIAL REVIEW NOT STARTED

  CHAPTER 37 ARCHITECTURE FIX B COMPLETE —
  READY FOR FOCUSED READ-ONLY CLOSURE AUDIT

  END CHAPTER-37 ARCHITECTURE FIX B.