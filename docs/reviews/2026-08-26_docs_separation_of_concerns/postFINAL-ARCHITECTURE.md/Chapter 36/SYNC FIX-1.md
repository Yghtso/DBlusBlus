## Chapter 36 Verification Fix A report

1. **Initial HEAD/status**

   - HEAD: `0ba7068f4f6cceebecc0fe74522690309bc1390d`
   - Commit: `synced VERIFICATION after chapter 36 ARCHITECTURE fix`
   - Worktree: clean
   - Index: clean

2. **Final HEAD/status**

   - HEAD unchanged: `0ba7068f4f6cceebecc0fe74522690309bc1390d`
   - Worktree: `M docs/VERIFICATION.md`
   - Index: clean

3. **Task-modified files**

   - [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md)

4. **Exact V36 sections changed**

   - Added V36-077 at [line 24054](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:24054).
   - Added its atomic-ledger entry at [line 24114](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:24114).
   - Corrected the §36.4 subsection-coverage row at [line 24152](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:24152).

5. **Original F36-V1 defect**

   V36-029, V36-069 and `Cost Model Benchmarks` tested deterministic consumption of valid calibrations and relative operator ranking, but did not verify the mandatory calibration tool, its eight primitive measurements, its relative-weight proposal, or configuration-only ownership.

6. **Exact §36.4 primitive inventory**

   1. cached sequential scan throughput
   2. cold-ish sequential page reads
   3. random page reads
   4. integer predicate throughput
   5. VARCHAR comparison throughput
   6. hash throughput
   7. sort comparison throughput
   8. temporary spill read/write throughput

7. **§42.5 reinforcement**

   §42.5 requires calibration to measure §36.4’s primitive dimensions on the target deployment and record the resulting relative-weight configuration. Its base-access workloads also vary selectivity, heap density, correlation, cache budget, output/VARCHAR width and dead-version fraction without imposing a fixed selectivity cutoff.

8. **New procedure**

   `V36-077 — Calibration-tool primitive completeness and configuration ownership`

9. **Positive calibration fixture**

   One identified calibration invocation in a controlled target-deployment context must expose:

   - tool/equivalent identity;
   - invocation and deployment context;
   - actual execution of all eight measurement paths;
   - measured quantities and declared natural units;
   - an attributable relative-weight proposal;
   - validation and configuration ownership;
   - absence of database-format changes.

10. **Per-primitive observation matrix**

| Primitive | Controlled observation | Natural unit |
|---|---|---|
| Cached sequential scan | Cached-state sequential scan over known volume | pages/time or bytes/time |
| Cold-ish sequential reads | Observed cold-ish sequential page range | pages/time, bytes/time or time/page |
| Random page reads | Identified nonsequential page sequence | pages/time, bytes/time or time/page |
| Integer predicates | Known evaluation count | evaluations/time or time/evaluation |
| VARCHAR comparison | Known comparisons and operand widths/classes | comparisons/time or time/comparison |
| Hashing | Known operation count and payload size | operations/time or time/operation |
| Sort comparisons | Independently counted comparator calls | comparisons/time or time/comparison |
| Temporary spill I/O | Controlled temporary writes and reads | pages/time, bytes/time or time/page, by direction |

Each row also requires an identified proposed-weight contribution or declared derivation/aggregation path.

11. **Relative-weight proposal oracle**

   The proposal must be attributable to the same calibration invocation and account for every primitive measurement. The procedure permits Architecture-defined aggregation freedom and does not impose a one-to-one eight-measurement-to-seven-coefficient mapping.

12. **Configuration/deployment ownership**

   Applying a valid proposal may create a later retained configuration identity. It must not add database-file, catalog/schema, statistics, ANALYZE-payload, semantic-proof or PhysicalPlan-property fields. Saving ordinary external deployment configuration remains permitted.

13. **Missing-primitive controls**

   Each primitive path is independently suppressed after proving its workload was selected. A syntactically valid, finite proposal still fails calibration completeness when any required primitive was omitted.

   Additional failures cover:

   - all measurements but no proposal;
   - an untraceable proposal;
   - calibration output made into a persisted database-format requirement.

14. **Missing-evidence handling**

   Missing invocation, measurement event/unit, proposal-attribution or ownership evidence yields:

   `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`

   It cannot PASS from plausible coefficient values.

15. **Nonvacuity**

   V36-077 explicitly inherits V36-003. Every omission fixture must prove the selected workload, intended boundary and actual omission.

16. **Proposal versus configuration validation**

   Calibration completeness and CostConfig validity remain separate:

   - a complete calibration must measure all eight primitives and produce an attributable proposal;
   - proposed conversion values must pass §§36.2.1/36.3 validation before planning consumes them;
   - invalid consumed configuration retains existing `OptimizerError` ownership;
   - physical fallback assumptions need not be derived by calibration.

17. **Corrected §36.4 row**

   The row now names V36-077 as the primary oracle. V36-029/V36-069 are explicitly limited to determinism and legal calibration variation; `Cost Model Benchmarks` is explicitly supplementary operator-ranking methodology.

18. **Chapter-41 mapping**

   No Chapter-41 row changed. Live Chapter 41 does not independently define the missing calibration-tool contract, so adding a fabricated Chapter-41 obligation would be incorrect.

19. **§42.5 cross-reference**

   Added to both the V36-077 atomic-ledger entry and the corrected §36.4 subsection row.

20. **Atomic integrity**

   - Definitions: 77
   - Unique definitions: 77
   - Range: V36-001 through V36-077
   - Missing IDs: none
   - Duplicate definitions: none

21. **Family integrity**

   Ten families remain, exactly V36-A through V36-J. No new family was created.

22. **Existing atomics unchanged**

   V36-001 through V36-076 remain byte-for-byte unchanged. Their extracted SHA-256 remained:

   `528b3c60da6a2706d792e18680e291ddbaa6fec9305f36cfa799cc633ca57f6e`

23. **All-22-invariant matrix**

   Unchanged and still complete: 22 mapped invariants, no gap introduced.

24. **A–BC adversarial matrix**

   Unchanged and still complete for its defined cases. V36-077 owns its additional calibration-specific omission controls directly.

25. **Actual external-reference set**

   Remains 114 unique identifiers:

   - V20: `5, 11, 15, 16, 19`
   - V22: `C, D, F, G, I, J, K`
   - V24: `A, I, L, N`
   - V27: `B, D, E, F, H, I, J, K, L, N, Q`
   - V28: `E, F, K, L, O`
   - V29: `N, O`
   - V30: `E, F, I, J, K`
   - V31: `A, B, G, N`
   - V32: `B, C, H, I`
   - V33: `A, C, D, E, F, G, H, I, K, N`
   - V34: `B, C, F, G, J, K`
   - Listed V35 atomic ranges and family owners

26. **Inventory set**

   The declared inventory remains the same 114 identifiers. V36-077 added no external procedure ID.

27. **Set comparison**

   - Actual set: 114
   - Inventory set: 114
   - Actual minus inventory: empty
   - Inventory minus actual: empty
   - Inventory duplicates: none

28. **Named headings**

   All 12 named reusable headings still resolve. `Cost Model Benchmarks` remains valid but supplementary.

29. **Architecture**

   Unmodified.

30. **Frozen chapters**

   Chapters 31–35 Architecture and Verification are unmodified. Chapter 37 onward is unmodified.

31. **Diff scope**

   - `docs/VERIFICATION.md`: 48 insertions, 1 deletion
   - All hunks are inside V36.
   - No historical artifact changed.

32. **`git diff --check`**

   PASS.

33. **New semantic questions**

   None.

34. **Final repository status**

   - HEAD unchanged
   - `M docs/VERIFICATION.md`
   - Index clean
   - No unrelated changes observed

35. **Confirmations**

   - ARCHITECTURE NOT MODIFIED
   - DEVELOPMENT NOT MODIFIED
   - PROJECT_STATE NOT MODIFIED
   - CHAPTERS 31–35 NOT MODIFIED
   - EXISTING V36 ATOMICS NOT MODIFIED
   - HISTORICAL ARTIFACTS NOT MODIFIED
   - IMPLEMENTATION NOT STARTED
   - NO BUILD/TEST/SANITIZER/BENCHMARK
   - NO STAGING/COMMIT
   - CHAPTER 37 REVIEW NOT STARTED

**F36-V1: REPAIRED**

CHAPTER 36 VERIFICATION FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT