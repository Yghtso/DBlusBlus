## Chapter 34 Architecture Fix A report

1. **Initial HEAD/status**

   - HEAD: `d59006eccfb63e269caca82f03195fe971d17f06`
   - Commit: `d59006e chapter 34 ARCHITECTURE analysis`
   - Worktree: clean
   - Index: clean

2. **Final HEAD/status**

   - HEAD unchanged: `d59006eccfb63e269caca82f03195fe971d17f06`
   - Worktree: `M docs/ARCHITECTURE.md`
   - Index: clean

3. **Files modified**

   - [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

   No other file changed.

4. **Exact Chapter-34 sections changed**

   - [§34.2 ANALYZE](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24377)
   - [§34.6 IndexStatistics](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24615)
   - [§34.7 V1 collection strategy](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24684)
   - [§34.9 HyperLogLog NDV estimation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24745)
   - [§34.10 Most-common values](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24767)
   - [§34.11 Histogram collection](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24796)
   - [§34.17 invariant 6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25498)

   Every changed hunk lies within Chapter 34. Chapter 35 now begins at line 25519 solely because the Chapter-34 edit removed six lines.

5. **N34-1 defect and canonical owner**

   The original invariant could be read as requiring one StatsVersion across every table used by an optimization.

   The canonical owners instead establish:

   - per-table complete-generation selection in §§34.3–34.3.1;
   - immutable per-statistics-object retention in §33.4;
   - stable descriptor lifetime and fallback in §34.15;
   - TABLE-manifest coherence in §§34.14–34.14.6.

6. **Final invariant 6**

   > For each table statistics object used by one optimizer invocation, the invocation retains one complete compatible TABLE-manifest generation under §§33.4, 34.3.1, and 34.15; all required TABLE/COLUMN/INDEX members have that generation's StatsVersion, and later publication cannot mutate or mix it. Different tables may retain different valid StatsVersions; when no complete compatible generation is available, valid-old or missing-statistics fallback applies.

7. **Per-object/table-generation scope**

   The invariant now explicitly selects and retains one complete compatible generation for each table statistics object. It does not define a database-global generation.

8. **Cross-table version independence**

   Different tables in one optimization may use different valid StatsVersions. No equality comparison between those versions is required.

9. **Same-table manifest coherence**

   The selected TABLE payload and every manifest-required COLUMN and INDEX payload must carry the selected generation’s StatsVersion. Cross-generation completion or partial salvage remains forbidden.

10. **Repeated table references**

    Multiple aliases of the same underlying table refer to the same table statistics object for the invocation and therefore retain the same stable generation. Aliasing does not create permission to select inconsistent versions.

11. **Stable planner retention**

    Once a planner retains S1, publication of S2 cannot mutate or partially replace S1. Later permitted invocations may select S2.

12. **StatsVersion identity/comparison**

    Unchanged:

    - `StatsVersion = (TxnId, CommandId)`
    - unsigned-lexicographic comparison
    - no global version allocator
    - status-independent identity after committed publication

13. **Catalog/schema/index compatibility**

    The repaired wording requires a “complete compatible” generation and delegates compatibility to the existing TableId, SchemaVer, ColumnId, IndexId and manifest owners. Names or numerically equal counters remain insufficient.

14. **Complete-generation publication**

    Unchanged:

    - one table manifest is the publication unit;
    - validation precedes the first statistics row;
    - one ANALYZE coordinator owns publication;
    - incomplete generations remain unusable;
    - different tables publish independently.

15. **Older-generation/fallback behavior**

    If no complete compatible selected generation exists, the canonical older-valid or missing-statistics fallback applies. No hybrid descriptor is fabricated.

16. **N34-2 phrase-by-phrase edits**

| Previous wording | Final timeless contract |
|---|---|
| SQL interface “may later include” targetless `ANALYZE` | Targetless all-table `ANALYZE` is outside the v1 baseline |
| “Version 1 uses explicit/manual ANALYZE” | The v1 baseline requires an explicit/manual ANALYZE path |
| Automatic analyze “is deferred until…” | Automatic scheduling is not required; optional §14.16 maintenance triggers may request ANALYZE without publication ownership |
| “even if a later maintenance command…” | VACUUM and ANALYZE remain distinct when one scheduler or combined invocation requests both |
| “practical initial estimate” | “baseline fallback estimate” |
| “complex initial page sampler” / “future performance optimization” | Full heap scan is the v1 baseline; replacement page/block sampling is outside that baseline |
| “Initial precision” | “Baseline precision” |
| Persisted sketches “are deferred” | Persisted incremental sketches are outside the v1 payload contract |
| MCV “Initial target” | “Baseline target” |
| Reservoir “Initial target” | “Baseline target” |

17. **Roadmap versus runtime chronology**

    Project-roadmap wording was removed. Legitimate runtime ordering remains intact, including:

    - own later statements using transaction-local statistics;
    - later publication of S2 while a planner retains S1;
    - later freezing of catalog tuple metadata;
    - later-aborted ANALYZE not resetting global counters.

18. **Full-scan baseline**

    Preserved. V1 still requires a full vectorized heap scan. Page/block sampling cannot silently replace it.

19. **Bounded algorithms**

    Unchanged:

    - HLL `p=14`, 16,384 registers;
    - bounded MCV target of 64;
    - bounded reservoir target of 100,000;
    - approximately 100 histogram bins;
    - configurable small-table exact threshold;
    - existing memory-pressure behavior.

20. **Persisted-sketch scope**

    The v1 payload persists the NDV estimate, not the HLL sketch. Persisted incremental sketches remain outside the v1 payload contract without being made universally prohibited for a separately authorized extension.

21. **Persisted payload and validation**

    No changes were made to:

    - payload version or framing;
    - TABLE/COLUMN/INDEX field sets;
    - chunk layout;
    - CRC;
    - scalar codec;
    - §34.14.6 validation;
    - normalization;
    - generation-atomic rejection.

22. **ANALYZE transaction/publication**

    Transactional statistics rows, terminal-COMMITTED global visibility, C5 installation/fallback, concurrent ANALYZE selection and publication claims are unchanged.

23. **Failure and cancellation**

    Unchanged:

    - pre-row collection/construction failure remains prepublication;
    - failure after the first statistics row requires the canonical abort outcome;
    - incomplete generations remain globally unusable;
    - commit remains uncancellable after its authorizing append;
    - post-commit cache failure cannot reverse COMMITTED.

24. **Chapter 31 regression**

    One ANALYZE publication coordinator and the existing control-operator ownership remain intact. No DML W/C/R semantics were imported.

25. **Chapter 32 regression**

    No parallel ANALYZE capability or helper publication authority was introduced. Worker scheduling cannot change publication ownership.

26. **Chapter 33 regression**

    The edit now mirrors §33.4’s per-statistics-object stable-view rule. It does not impose one cross-table StatsVersion or allow mid-invocation replacement.

27. **Statistics versus semantic proof**

    Unchanged. Statistics—including an exact-at-ANALYZE zero—cannot establish semantic emptiness, suppress demanded errors or eliminate required execution.

28. **Post-edit thought experiments**

| Case | Owner | Required outcome | Result |
|---|---|---|---|
| A. T1=S1, T2=S2 | §§33.4, 34.15 | Permitted; versions need not match | Resolved |
| B. TABLE S2 + COLUMN S1 | §§34.3.1, 34.14.6 | Reject mixed generation | Resolved |
| C. INDEX member from later generation | Same | Reject mixing | Resolved |
| D. Same table through two aliases | §33.4, invariant 6 | One stable per-object generation | Resolved |
| E. P retains S1 while S2 publishes | §§34.15, 16.10 | P remains on S1 | Resolved |
| F. Two planners retain different generations | §§33.4, 34.15 | Permitted independently | Resolved |
| G. No compatible generation | §§34.3.1, 34.15 | Older-valid or missing fallback | Resolved |
| H. Different-table ANALYZE | §14.17.1 | Independent publication | Preserved |
| I. Same-table concurrent ANALYZE | §§14.17.1, 34.3.1 | Canonical greatest applicable StatsVersion | Preserved |
| J. Numeric version equality used as compatibility | Chapters 16/34 | Invalid; IDs/schema/manifest govern | Preserved |
| K. Empty-table statistic used as proof | §§34.1, 35.2 | Forbidden | Preserved |
| L. Full scan replaced by mandatory sampling | §34.7 | Forbidden by baseline | Resolved |
| M. Persisted sketch made mandatory | §§34.9, 34.14 | Not part of v1 payload | Resolved |
| N. Mathematical initialization mistaken for chronology | §34.14.6.5 | Register initialization remains unchanged | Preserved |
| O. Runtime maintenance ordering lost | §§14.16–14.17.1, 34.2 | VACUUM/ANALYZE remain distinct | Preserved |

29. **Global contradiction search**

    - **TRUE CONTRADICTION:** none
    - **VALID DIFFERENT OWNER:** Chapters 14, 16, 31, 33 and 35–38
    - **VALID DIFFERENT STAGE:** transaction-local versus globally committed descriptors
    - **VALID DIFFERENT OBJECT:** independently analyzed tables
    - **VALID DIFFERENT VERSION:** retained S1 and subsequently published S2
    - **VALID OPTIONAL CAPABILITY:** optional §14.16 maintenance trigger
    - **NON-NORMATIVE:** none remaining among the targeted roadmap phrases

    No language was found requiring equal cross-table StatsVersions, mixed generation repair, mid-invocation switching, or mandatory optional sampling/sketch persistence.

30. **Document-role assessment**

    Chapter 34 remains timeless, implementation-independent and canonical. The edit introduces no implementation status, review history, sampler design, persisted field, allocator, publication protocol or test-result claim.

31. **New semantic questions**

    **NONE**

32. **`git diff --check`**

    **PASS**

33. **Repair-only diff summary**

    - `docs/ARCHITECTURE.md`
    - 17 insertions
    - 23 deletions
    - All hunks within Chapter 34
    - No persistence-layout or algorithmic-value changes

34. **Final Git status**

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

35. **Confirmations**

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
CHAPTERS 31–33 NOT MODIFIED
CHAPTER 35 NOT MODIFIED
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
CHAPTER 35 REVIEW NOT STARTED
```

```text
N34-1: CLOSED
N34-2: CLOSED
```

**CHAPTER 34 ARCHITECTURE FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT**