CHAPTER 42 — VERIFICATION FIX V42-G1: SHARED BENCHMARK EVIDENCE, REPRODUCIBILITY, AND APPLICABILITY POLICY

**CLOSED V42-G1 at repair level.** I added one shared policy under [Benchmarking Philosophy](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:88). The original benchmark-family list, metric list, and micro/end-to-end guidance remain unchanged. No subsystem benchmark section, fixture grid, or other document was edited.

The policy makes a performance sample usable only when its declared workload and target regime were reached, its required operation completed, and the result satisfies the independent correctness owner. Incorrect, failed, or unintended partial runs retain their actual owner failure and are not converted into throughput, latency, or a performance verdict. A specialized startup/cancellation/partial-progress benchmark may measure only that explicitly declared behavior.

Each benchmark family must define its workload, work denominator, measured interval, setup/teardown treatment, and warmup or initial-state policy. Deliberate less-warm, miss, and recovery cases retain their intended initial state. A performance *claim* needs repeated samples, a predeclared summary and variability method, auditable raw results, and reported exclusions; a single diagnostic run may still be recorded without supporting a speedup claim. Unstable or incomparable evidence is **INCONCLUSIVE / INSUFFICIENT EVIDENCE**, not a post-hoc PASS or regression.

Comparisons identify the intended changed variables and keep other material workload, data, result, configuration, environment, and measurement conditions comparable. They need not vary exactly one factor, but conclusions must stay within the measured scope. The run record identifies workload/seed/scale, revision and build, relevant configuration and environment, regime activation, owner metrics, sampling/window/units, exclusions, correctness, and evidence disposition. Run-specific values remain in devlogs, CI/benchmark artifacts, or task reports.

Canonical owner counters retain their Chapter-40 or subsystem unit, boundary, scope, and availability; the harness may directly measure elapsed time but cannot self-certify a BufferPool, WAL, spill, memory, worker, or optimizer counter. Missing required evidence is **NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE**. True optional/conditional absence is **NOT APPLICABLE**, distinct from an applicable but unavailable metric. Input size or thread count alone cannot certify a miss, batching, split, spill, heuristic search, or parallel execution.

This introduces **no universal speed threshold, fixture grid, repetition count, warmup count, percentile algorithm, or benchmark file format**. It neither redefines Chapter-40 metrics nor duplicates correctness suites. Evidence can motivate—but cannot enact—an Architecture revision, including a change to the 1024 default or promotion of deferred JIT.

The requested semantic controls resolve as follows:

| Cases | Policy disposition |
|---|---|
| A, G | Wrong result or failed/incomplete ordinary run: invalid performance sample; preserve the owner cause. |
| B | One noisy run: useful diagnostic only, insufficient for an improvement claim. |
| C, I, U | Undisclosed dataset, configuration, or multiple material changes: not a narrow comparable claim. |
| D, E | Hit or heuristic regime not observed: required target activation missing; not verified. |
| F, H | Unavailable metric recorded as zero, or wrapper value conflicting with owner evidence: invalid/incomplete evidence. |
| J | Slow sample dropped after inspection: prohibited. |
| K | Genuinely unavailable optional latch-wait metric: N/A, with absence identified. |
| L, M | Single-worker-only path or absent optional SIMD: no fabricated scaling/SIMD sample. |
| N, O | Declared recovery startup or evidenced less-warm state: valid intended initial-state policy. |
| P | One retained diagnostic run: permitted, not sufficient for a performance claim. |
| Q | Comparable before/after with intended code change: valid comparison basis. |
| R | Different machine: potentially valid standalone run; direct paired claim needs qualification or comparable evidence. |
| S, T | Declared vector-size or memory-budget change: valid comparison variable, not an automatic preferred outcome. |

The policy composes with storage, B+, transaction/recovery/vacuum, front-end, execution, statistics, and optimizer benchmark sections without requiring them to repeat it. Those owner sections still need their own fixture and activation repairs. The stale-assertion review found no new claim that Architecture owns the existing exact group-commit, 100-column, vector-size, or join-count fixtures; none was changed.

Repository state: initial and final HEAD were `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). At entry, the index was clean and an untracked Chapter-42 `SYNC.md` review artifact was present. At exit, that artifact remained untouched, the index was still clean, and the sole task-created change was an **unstaged +84/−0** edit to `docs/VERIFICATION.md`. The cached-diff digest remained `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. No external state change was observed during the task. `git diff --check` passed.

G2–G11 remain **open and unmodified**—ten grouped Verification gaps. The analysis-only O42 count remains **186**. A dedicated V42 suite is not needed; repairs belong in the existing owner-suite benchmark sections. The exact next task is **CHAPTER 42 — VERIFICATION FIX V42-G2: STORAGE AND B+ BENCHMARK FIXTURES, REGIME ACTIVATION, AND METRIC PROVENANCE**.

    CHAPTER 41 ARCHITECTURE:
        CLEAN — CLOSED

    CHAPTER 41 VERIFICATION:
        CLEAN — CLOSED

    CHAPTER 42 ARCHITECTURE:
        CLEAN — CLOSED

    ATOMIC O42 OBLIGATIONS:
        186

    V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
        CLOSED

    V42-G2 — STORAGE/B+:
        OPEN / UNMODIFIED

    V42-G3 — TRANSACTION/RECOVERY/VACUUM:
        OPEN / UNMODIFIED

    V42-G4 — FRONT-END/PLANNING:
        OPEN / UNMODIFIED

    V42-G5 — EXECUTION:
        OPEN / UNMODIFIED

    V42-G6 — PARALLEL SCALING:
        OPEN / UNMODIFIED

    V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
        OPEN / UNMODIFIED

    V42-G8 — ANALYZE PERFORMANCE:
        OPEN / UNMODIFIED

    V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
        OPEN / UNMODIFIED

    V42-G10 — COST/OPTIMIZER PERFORMANCE:
        OPEN / UNMODIFIED

    V42-G11 — BENCHMARK-GAMING DETECTION:
        OPEN / UNMODIFIED

    P42-7 — VERIFICATION SYNCHRONIZATION:
        IN PROGRESS

    REMAINING GROUPED VERIFICATION GAPS:
        10

    DEDICATED V42 SUITE:
        NOT NEEDED —
        OWNER-SUITE BENCHMARK REPAIRS ONLY

    CHAPTER 42 VERIFICATION:
        NOT SYNCHRONIZED

    INDEPENDENT VERIFICATION CLOSURE:
        NOT PERFORMED

    IMPLEMENTATION:
        NOT AUTHORIZED

    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN

    INDEX:
        CLEAN; NO STAGED FILES
        TASK MODIFIED: NO

    REVIEW ARTIFACTS:
        PRE-EXISTING UNTRACKED Chapter-42/SYNC.md
        TASK MODIFIED: NO

    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

    NEXT AUTHORIZED TASK:
        CHAPTER 42 — VERIFICATION FIX V42-G2:
        STORAGE AND B+ BENCHMARK FIXTURES,
        REGIME ACTIVATION, AND METRIC PROVENANCE

END CHAPTER-42 VERIFICATION FIX V42-G1 —
SHARED BENCHMARK EVIDENCE,
REPRODUCIBILITY, AND APPLICABILITY POLICY.

CHAPTER 42 — VERIFICATION FIX V42-G2: STORAGE AND B+ BENCHMARK FIXTURES, REGIME ACTIVATION, AND METRIC PROVENANCE

**Result: CLOSED V42-G2 at repair level.** The edit supplies procedures for replayable workloads, proves the intended BufferPool and B+ regimes through owner observations, and separates logical work from physical I/O and structural metrics. No benchmark was run; this is a documentation-coverage result, not measured performance evidence.

### Repository and scope

| Item | Entry | Final |
|---|---|---|
| HEAD | `500cde09c7b9570fe8dd0dcd575b4d4de352b598` — `applied FIX-A4 42 in ARCHITECTURE` | Unchanged |
| Worktree | Unstaged `docs/VERIFICATION.md` G1 delta, +84/−0; pre-existing untracked `Chapter 42/SYNC.md` | Unstaged `docs/VERIFICATION.md`, cumulative +181/−0; same untracked artifact |
| Index | Clean; no staged files | Clean; no staged files |
| Cached-diff SHA-256 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | Unchanged |

G2 changed only [Storage Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2403) and [B+ Tree Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:3266): **97 insertions, 0 deletions**. The pre-existing G1 policy remains unchanged; the cumulative Verification diff is **181 insertions, 0 deletions**. No external repository-state change was observed between entry and final checks. The untracked review artifact was preserved and not modified by this task.

### Storage procedure

The fixture now records page/file population or generator and seed, page extent, access order, work and byte denominators, target regime, and actual BufferPool frame capacity. It relates working-set pages to capacity rather than relying on “tiny,” “large,” or “hot” labels.

Resident-hit samples require fitting and preconditioned pages plus canonical hit-path evidence. Miss-plus-read samples require controlled nonresidency, a completed BufferPool miss claim, and a completed physical read; probabilistic eviction is insufficient. Tiny-pool runs must demonstrate pressure, while large-pool resident runs must demonstrate residency and hits. A deliberately less-warm file-cache method and its observed state are recorded without claiming a perfectly cold OS cache.

Sequential reads use an ordered valid-page extent and content oracle; sequential writes use deterministic valid contents and validation with a declared timed window. Both retain exact work denominators and owner-sourced physical-I/O counts. Neither changes WAL or durability semantics. Heap insertion uses a replayable tuple stream; scans use a known population and independent expected result. Tuple encode/decode uses nontrivial fixed-width, nullable, and variable-width values, recording tuples, bytes, shape, and throughput unit.

### B+ procedure

The key fixtures cover INT64 ranks, short 16-byte hexadecimal VARCHAR keys, long 256-byte VARCHAR keys, and composite INT64/VARCHAR keys. Those lengths are **Verification fixtures**, not Architecture constants. Each run records cardinality, distribution, RID construction, seed, and order. Random lookups and insertions are seeded/replayable; sorted insertion, exact deletion, short and long ranges, and duplicate-heavy equality each require declared inputs and independently checked expected results. The duplicate-heavy fixture uses 32 distinct RIDs per declared logical key.

Hot-tree evidence requires an accessed tree-page working set fitting the configured BufferPool and observed residency/hits. Larger-than-buffer evidence requires the *accessed* physical tree-page working set to exceed capacity and observed misses, replacements, and completed physical reads. Canonical tree state supplies reachable pages and height. A dedicated insertion fixture must observe at least one owner-reported split, reported against successful inserts. Leaf and internal byte occupancy are separately aggregated from validated reachable pages using §8.16 geometry. L3 may validate structure at safe quiescence, but its time is excluded from operation throughput unless explicitly measured. B+ hit rate uses same-interval owner hits/(hits + misses); latch wait uses canonical instrumentation when available, otherwise honest N/A or missing-evidence disposition. BufferPool residency, OS/file-cache warmth, and CPU-cache behavior remain distinct.

### Self-review

| Check | Disposition |
|---|---|
| Storage negative cases A–H | A invalid setup; B not verified; C invalid regime; D valid miss/read evidence; E insufficient; F report limitation/N/A, never “perfectly cold”; G invalid sample; H insufficient fixture. |
| B+ negative cases I–T | I nonreplayable; J regime not reached; K split unverified; L insufficient; M valid larger-than-buffer regime; N–O invalid provenance; P valid occupancy aggregation; Q invalid zero; R valid N/A; S invalid timed window; T allowed when fixture identity and key class are retained. |
| Regime activation A–L | **Covered procedurally**: storage warm, miss, tiny, large, less-warm; B+ hot, capacity-exceeding, miss/read/replacement, split, duplicates, short range, long range. No run is claimed to have activated them. |
| Metric provenance A–L | **Covered procedurally**: logical work, physical reads/writes, hits/misses, split events, height, separate leaf/internal occupancy, optional latch wait, heap work, tuple denominator. Owner metrics are not inferred from wrapper output. |
| Replayability A–L | **Covered procedurally**: storage pages, heap tuples, tuple values, four key classes, random insertion/lookup, duplicates, and short/long bounds. |
| G1 composition | G1 sample validity, repetition, noise, applicability, and run records remain authoritative; G2 specializes fixtures and activation only. |

The affected analysis-only O42 families are opening storage coverage (approximately O42-002–009), storage residency (O42-017), and B+ measurements (O42-019–037). Those IDs are not canonical Verification IDs; the live procedures, not an inserted ledger, were changed. No new G2 finding was identified. No dedicated V42 suite is needed.

`git diff --check` passed. The cached diff and clean index were preserved. No file outside the authorized Verification edit was changed by this task; no implementation, build, test, sanitizer, or benchmark was run.

CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED

CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED

CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED

ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED

V42-G2 — STORAGE/B+:
    CLOSED

V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    OPEN / UNMODIFIED

V42-G4 — FRONT-END/PLANNING:
    OPEN / UNMODIFIED

V42-G5 — EXECUTION:
    OPEN / UNMODIFIED

V42-G6 — PARALLEL SCALING:
    OPEN / UNMODIFIED

V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    OPEN / UNMODIFIED

V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED

V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED

V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED

V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS

REMAINING GROUPED VERIFICATION GAPS:
    9

DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY

CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED

INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO

REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G3:
    TRANSACTION, RECOVERY, AND VACUUM
    BENCHMARK REGIME ACTIVATION AND METRIC PROVENANCE

END CHAPTER-42 VERIFICATION FIX V42-G2 —
STORAGE AND B+ BENCHMARK FIXTURES,
REGIME ACTIVATION, AND METRIC PROVENANCE.

CHAPTER 42 — VERIFICATION FIX V42-G3: TRANSACTION, RECOVERY, AND VACUUM BENCHMARK REGIME ACTIVATION AND METRIC PROVENANCE

**Result: CLOSED V42-G3 at repair level.** The three benchmark procedures now specify replayable workloads, owner-evidenced regime activation, measured boundaries, and metric provenance. This closes a *methodology* gap; it does not claim that any benchmark has run or produced a result.

### Repository and edit scope

| Item | Entry | Final |
|---|---|---|
| HEAD | `500cde09c7b9570fe8dd0dcd575b4d4de352b598` — `applied FIX-A4 42 in ARCHITECTURE` | Unchanged |
| Worktree | Unstaged `docs/VERIFICATION.md`, pre-existing G1+G2 **+181/−0** | Unstaged `docs/VERIFICATION.md`, cumulative **+305/−0** |
| Index | Clean; no staged files | Clean; no staged files |
| Cached-diff SHA-256 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | Unchanged |
| Review artifact | Pre-existing untracked `Chapter 42/SYNC.md` | Preserved, unmodified |

G3 added **124 lines and deleted none**, only in [Group Commit Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7435), [Checkpoint/Recovery Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7499), and [Vacuum Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7555). G1, G2, and the correctness suites remain unchanged. No external state change was observed.

### What the procedures now establish

- **Group commit:** A fixed-seed persistent-write transaction stream records operation shape, workers, expected commits, start coordination, and timing window. The one-committer case is a genuine synchronous-commit baseline. Multi-committer cases must observe overlapping requests; a batching case must observe one durability advancement covering multiple waiters, and contention must have queue/overlap evidence. Successfully C6-acknowledged commits supply benchmark TPS and request-to-acknowledgement latency; this remains distinct from the §40.3 semantic COMMITTED count at C3. Actual WAL-owner `fdatasync` calls, successful durability-group coverage, and valid-prefix WAL append bytes supply the other metrics. Failures and uncertain outcomes are not successful samples. The one-fsync-per-commit mode remains a comparable, test-only diagnostic—not a production durability mode or a speedup requirement. The existing thread-count grid is unchanged and Verification-owned.

- **Checkpoint and recovery:** Replayable mostly-clean and heavily-dirty fixtures use declared page populations and materially different, owner-observed dirty/DPT states. Checkpoint duration ends at §13.5 completion, including the control durability and in-memory completion step. WAL and FPI bytes come from canonical WAL records, captured DPT size from checkpoint state, and retained WAL from retention-floor/LSN accounting. Recovery fixtures record the selected control/checkpoint, valid persisted WAL prefix, acknowledged durability frontier, page/status state, and expected work. Analysis and redo use their owner phase boundaries; total recovery ends at READY. Owner redo-application events supply pages redone. A nonzero-redo positive control is required, and correctness at READY is a prerequisite for timing evidence.

- **Vacuum:** Fixed-seed fixtures separately activate old updated versions, committed deletions, aborted-created versions, duplicate secondary `(key,RID)` entries, and a registered long-running snapshot with a follow-up after release. Owner scan events, newly removed exact physical index entries, canonical heap free-space changes, B+ structural events, FSM observations, and freeze rewrites supply their respective metrics and denominators. Positive controls cover exact cleanup, heap reclamation, B+ structural activity, and freezing; a valid ordinary workload may still report zero structural events. RID grace is measured from retirement after persisted `DEAD` to the read-epoch predicate becoming true, separately from later RID reuse and `TUPLE_WRITE` or predecessor barriers. Foreground-latency comparison requires observed overlap with vacuum. Owner failures are preserved, not converted to zero rates.

The shared G1 rules continue to govern correctness-valid samples, repetition, variability, comparability, missing evidence, and run records. Existing synchronous-commit, recovery, vacuum, B+, RID, freezing, and FSM correctness procedures remain the oracles; benchmark summaries do not certify themselves. No Architecture semantics, production diagnostic API, or hard performance threshold was added.

### Self-review

| Required review | Disposition |
|---|---|
| Group-commit activation A–J | **Covered procedurally:** single committer; overlapping committers; shared batch; contention; acknowledged-commit denominator; owner `fdatasync`, commits/sync, and WAL bytes; C6 latency boundary; diagnostic-mode separation. |
| Checkpoint/recovery activation A–L | **Covered procedurally:** proved clean/dirty states and capture DPT; completed checkpoint; WAL/FPI and retention observations; replayable crash prefix; correct READY; analysis/redo boundaries; nonzero redo; owner pages-redone count. |
| Vacuum activation A–L | **Covered procedurally:** five workload classes; positive exact cleanup, reclaim, structural-event, and freeze controls; blocked/released RID grace; FSM before/after; observed foreground overlap. |
| Group-commit negatives A–H | A–B insufficient target evidence; C valid batching; D–E invalid sample/provenance; F valid WAL provenance; G invalid early latency endpoint; H valid supplementary diagnostic when durability semantics match. |
| Checkpoint/recovery negatives I–P | I unverified regime; J valid proved clean fixture; K invalid FPI inference; L valid WAL-record provenance; M invalid redo inference; N valid owner count; O useful but insufficient sole baseline; P invalid performance sample. |
| Vacuum negatives Q–AB | Q invalid cleanup count; R valid exact removals; S unchanged file size is not failure; T reclamation can precede RID reuse; U valid observed zero; V missing positive control; W invalid sleep-based grace claim; X valid epoch guard; Y insufficient FSM inference; Z valid owner observation; AA valid zero but inadequate positive control; AB overlap unverified. |

The affected analysis-only obligations are the supplied group-commit **O42-038–043**, checkpoint/recovery **O42-044–054**, and vacuum **O42-055–067** families. No O42 ledger was added to Verification, and no new G3 finding was identified. `git diff --check` passed; the index and cached-diff digest are unchanged. No changes outside the authorized edit were created.

CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED

CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED

CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED

ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED

V42-G2 — STORAGE/B+:
    CLOSED

V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED

V42-G4 — FRONT-END/PLANNING:
    OPEN / UNMODIFIED

V42-G5 — EXECUTION:
    OPEN / UNMODIFIED

V42-G6 — PARALLEL SCALING:
    OPEN / UNMODIFIED

V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    OPEN / UNMODIFIED

V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED

V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED

V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED

V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS

REMAINING GROUPED VERIFICATION GAPS:
    8

DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY

CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED

INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO

REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G4:
    FRONT-END AND LOGICAL-PLANNING
    BENCHMARK COVERAGE, ALLOCATION,
    AND METRIC PROVENANCE

END CHAPTER-42 VERIFICATION FIX V42-G3 —
TRANSACTION, RECOVERY, AND VACUUM
BENCHMARK REGIME ACTIVATION AND
METRIC PROVENANCE.

CHAPTER 42 — VERIFICATION FIX V42-G4: FRONT-END AND LOGICAL-PLANNING BENCHMARK COVERAGE, ALLOCATION, AND METRIC PROVENANCE

**Result: CLOSED V42-G4 at repair level.** The live Verification procedures now cover the complete §42.3 measurement inventory. This is a methodology repair; no benchmark was run and no performance result is claimed.

### Repository and scope

| Item | Entry | Final |
|---|---|---|
| HEAD | `500cde09c7b9570fe8dd0dcd575b4d4de352b598` — `applied FIX-A4 42 in ARCHITECTURE` | Unchanged |
| Worktree | Unstaged `docs/VERIFICATION.md`, pre-existing G1+G2+G3 **+305/−0** | Unstaged `docs/VERIFICATION.md`, cumulative **+393/−1** |
| Index | Clean; no staged files | Clean; no staged files |
| Cached-diff SHA-256 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | Unchanged |
| Review artifact | Pre-existing untracked `Chapter 42/SYNC.md` | Preserved, unmodified |

G4 changed only [Catalog / Front-End Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:15049) and [Parser/AST Memory Benchmark](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:15143): **88 insertions, 1 deletion**. The deletion replaces the brief optimizer-benchmark sentence with a scoped planning-arena measurement procedure; full optimizer search/cost methodology remains elsewhere. G1–G3 are unchanged. No external repository-state change was observed.

### §42.3 coverage

The live §42.3 inventory is: lexer MB/s; parser statements/s; binder latency; catalog name/ID lookup latency; historical-schema lookup latency; catalog cache hit rate; large SELECT-list, VALUES, expression-tree, and multi-join binding; logical-plan construction time; rewrite-phase time; front-end allocation calls/bytes; and optimizer planning-arena allocations/bytes. **All are now covered procedurally.**

- **Stages and fixtures:** Stable SQL/input or generators, seeds, catalog snapshots, scale, expected results, and work denominators identify each run. Lexer timing uses known input bytes and token outcomes; parser timing uses known successful statements and raw-AST outcomes, declaring whether tokenization is included. Binder timing runs from a replayable raw AST to a completed typed/bound result under controlled catalog/cache state. Logical construction runs from validated bound input to a validated pre-rewrite plan. Rewrite timing runs from a validated input plan through the owned phase and post-phase validation; at least one fixture must show actual rule/phase work. Physical optimization and execution are excluded from those stage timings.

- **Catalog:** Separate name-keyed and stable-ID-keyed lookup series check immutable descriptor identity; an ID sample cannot first resolve a name. Historical lookup requests a legally retained `SchemaVer` different from the current one and checks the exact returned descriptor. Catalog cache hit rate uses same-interval owner hits over applicable lookup attempts, with a proved positive hit and a controlled miss/bypass plus snapshot-aware fallback. Snapshot-invisible or uncommitted descriptors invalidate performance evidence. No hit-rate target or cache implementation policy was introduced.

- **Large binding cases:** The retained 100-column SELECT is one Verification-owned *wide SELECT* fixture, not an Architecture width constant. Declared VALUES dimensions/types/NULLs, expression node count/depth/types, and distinct multi-join relation occurrences and BindingIds each receive actual binder timing. Multi-join binding is not optimizer join search.

- **Allocation:** The Parser/AST procedure records front-end-attributed calls and bytes, peak live bytes, and retained/materialized backing. A predeclared scale series increases independently known semantic nodes/elements to reveal one-for-one general-purpose heap-allocation churn versus coarser storage, without imposing a slope limit or AST arena. The dedicated §38.21 optimizer planning arena is measured separately through its own allocation/reservation or growth events, charged bytes, and peak live bytes. Neither RSS, plan-node count, nor execution QueryMemoryManager memory substitutes for those owner measurements. Existing §18.14 lifetime, §18.17 resource, and §38.21 budget/failure oracles remain authoritative.

### Self-review

| Review | Disposition |
|---|---|
| Front-end negatives A–H | A valid fixture; B stale/invalid ownership claim; C incomplete without VALUES binder timing; D valid known structural fixture; E wrong stage; F invalid stage boundary; G useful baseline but insufficient rewrite activation; H invalid sample. |
| Catalog negatives I–Q | I valid name result; J invalid ID-path evidence; K historical target absent; L valid historical result; M insufficient assumed hit; N valid owner hit; O correctness failure/invalid sample; P valid fallback control; Q invalid fabricated zero. |
| Allocation negatives R–Y | R insufficient RSS-only evidence; S valid attributed calls/bytes; T permitted non-arena AST; U invalid arena mandate; V valid churn-pattern evidence, not an automatic numeric failure; W invalid unchanged-node scale test; X invalid execution-memory proxy; Y valid planning-arena owner evidence. |
| Coverage A–O | **Covered procedurally**, including all timing, catalog, large-case, rewrite, front-end allocation, and planning-arena dimensions. |
| Activation A–M | **Covered procedurally**: distinct lookup paths, non-current historical version, hit and fallback, four large binding cases, nontrivial construction/rewrite, allocation scale series, and arena activity. |
| Provenance A–L | **Covered procedurally**: bytes/statements, stage boundaries, lookup mode, owner cache events, attributed front-end calls/bytes, and dedicated arena events/bytes/peak. No wrapper, RSS, or end-to-end-time self-oracle is accepted. |

Chapter-16/18/19/20/38 correctness suites are reused rather than duplicated. G1 continues to govern sample validity, repetition, comparability, and missing evidence. The affected analysis-only area is **O42-068–087**; no O42 ledger was inserted into Verification. No new G4 finding or Architecture contradiction was identified. No hard performance threshold, allocator mandate, Architecture change, or implementation authorization was added.

`git diff --check` passed. The cached diff and clean index were preserved. No file outside the authorized Verification edit was changed by this task.

CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED

CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED

CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED

ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED

V42-G2 — STORAGE/B+:
    CLOSED

V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED

V42-G4 — FRONT-END/PLANNING:
    CLOSED

V42-G5 — EXECUTION:
    OPEN / UNMODIFIED

V42-G6 — PARALLEL SCALING:
    OPEN / UNMODIFIED

V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    OPEN / UNMODIFIED

V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED

V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED

V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED

V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS

REMAINING GROUPED VERIFICATION GAPS:
    7

DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY

CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED

INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO

REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G5:
    EXECUTION MICROBENCHMARK AND
    END-TO-END WORKLOAD ACTIVATION,
    REPLAYABILITY, AND METRIC PROVENANCE

END CHAPTER-42 VERIFICATION FIX V42-G4 —
FRONT-END AND LOGICAL-PLANNING
BENCHMARK COVERAGE, ALLOCATION,
AND METRIC PROVENANCE.

CHAPTER 42 — VERIFICATION FIX V42-G5: EXECUTION MICROBENCHMARK AND END-TO-END WORKLOAD ACTIVATION, REPLAYABILITY, AND METRIC PROVENANCE

**Result: CLOSED V42-G5 at repair level.** The three execution benchmark sections now define replayable workloads, measured-work denominators, physical-path activation, NULL and spill regimes, comparable vector-size runs, and owner-sourced resource metrics. No benchmark was run, so this is a methodology result—not performance evidence.

### Repository and scope

| Item | Entry | Final |
|---|---|---|
| HEAD | `500cde09c7b9570fe8dd0dcd575b4d4de352b598` — `applied FIX-A4 42 in ARCHITECTURE` | Unchanged |
| Worktree | Unstaged `docs/VERIFICATION.md`, pre-existing G1–G4 **+393/−1** | Unstaged `docs/VERIFICATION.md`, cumulative **+508/−2** |
| Index | Clean; no staged files | Clean; no staged files |
| Cached-diff SHA-256 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | Unchanged |
| Review artifact | Pre-existing untracked `Chapter 42/SYNC.md` | Preserved, unmodified |

G5 changed only [Execution Microbenchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25942), [Vector Size Benchmark](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26023), and [End-to-End Execution Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26055): **115 insertions, 1 deletion**. The deletion replaces vector-default wording that could imply measurements alone change Architecture. G1–G4 are unchanged. No external repository-state change was observed.

### Live §42.4 measurements and G5 procedure

The live measurement inventory is scan rows/bytes per second; filter and arithmetic-projection rows per second; VARCHAR comparison rows per second; hash build, probe, and output rows per second; aggregate input rows and groups per second; in-memory sort rows per second; external-sort MB per second; allocator calls, chunk allocations, and temporary bytes per query; NULL-free and NULL-heavy regimes; a below/at/above-default vector study; the twelve listed end-to-end workload families; and applicable latency, throughput, CPU, peak query memory, BufferPool hits/misses, WAL bytes, and spill bytes.

- **Common and micro fixtures:** Fixed-seed or explicit inputs record schema, row/byte scale, widths, distributions, selected columns, predicates, joins, NULLs, capacity, memory settings, and independent expected results. Micro timing excludes setup unless setup is the subject. Scan rows are owner-observed visited work, not filtered output; scan bytes are consistently defined as encoded tuple-body bytes of visited rows. Filter, arithmetic projection, and VARCHAR comparison require actual operator/kernel work. Hash build, prepared-state probe, and joined-output rates have separate measured windows and denominators. Aggregate input rows and completed groups are separate. In-memory sort requires owner-proved zero spill; external sort requires observed runs/merge work and positive spill, with declared encoded-input-byte MB/sec. Scoped instrumentation supplies allocator, chunk, and temporary-byte totals. Those totals do **not** prove G7 structural constraints.

- **NULL and vector regimes:** Applicable micro families declare a proved NULL-free input and a reproducible, materially NULL-heavy *effective operator input*. The retained **256/512/1024/2048/4096** grid is Verification-owned and spans below, at, and above the v1 **1024** default. Comparisons replay the same data, operator/query, result, material budget, and environment; they record effective `DataChunk` capacity, actual cardinalities, and operator activity. A configuration value that never reaches the path is not an activated candidate. Results may motivate an explicit Architecture revision but do not change the default automatically.

- **End-to-end workloads:** Each case records SQL/operation identity, named scale with actual values, deterministic data, expected outcome, material configuration, validated physical plan, and nonzero relevant operator work. Indexed point, index range, full scan, large HashJoin, and index-driven UPDATE must activate their named paths. Small join may use any legal recorded join strategy; ORDER BY LIMIT does not mandate TopN. Group aggregate, ORDER BY, filter/project, and bulk INSERT have stated input/result and activity checks. Concurrent read/write declares transaction mix, isolation, key overlap, outcome classes, and observed read/write overlap—not merely two started threads.

- **Metric provenance:** Workload-specific successful queries, operations, rows, or transactions define throughput units and latency endpoints; writing transactions use successful acknowledgement. CPU has a declared process/thread/worker scope and is not wall time. QueryMemoryManager owns peak accounted query bytes; BufferPool owns hits/misses; valid-prefix WAL append accounting supplies applicable write bytes; the spill owner supplies spill bytes. Zero spill requires owner evidence. Correctness suites remain the result oracles; a selected plan or wrapper label alone proves neither correct output nor completed operator work.

G5 does not add G6 parallel-worker scaling or claim G7 hot-loop structural conformance. It adds no hard speed, latency, memory, or spill-bandwidth target and changes no Architecture semantics.

### Self-review

| Review | Disposition |
|---|---|
| Micro negative cases A–K | A uses visited scan work; B target inactive; C invalid kernel window; D valid deterministic VARCHAR fixture; E invalid probe-only timing; F hash target inactive; G valid known groups; H–I wrong sort regime; J invalid allocation provenance; K insufficient NULL-heavy regime. |
| Vector cases L–Q | L valid Verification grid; M stale Architecture claim; N insufficient without 1024; O configured-but-unused target inactive; P incomparable datasets unless intentionally a different question; Q evidence only, no automatic default change. |
| End-to-end cases R–AD | R/T/X/Z target inactive; S/V/AA/AD valid with the stated owner evidence; U valid legal small join; W full sort may validly serve ORDER BY LIMIT; Y invalid success count; AB invalid inferred hit; AC invalid RSS memory proxy. |
| Micro coverage A–Q | **Covered procedurally:** all scan, filter, project, VARCHAR, hash, aggregate, sort, allocation/chunk/temp, and NULL-regime dimensions. |
| End-to-end coverage A–L | **Covered procedurally:** all twelve listed workload families. |
| Metric provenance A–L | **Covered procedurally:** latency, throughput, CPU, accounted peak memory, hits/misses, WAL/spill, input/output rows, scan bytes, and temporary bytes. |
| Activation A–P | **Covered procedurally:** named scan/filter/project/VARCHAR/hash/aggregate/sort paths, effective vector capacity, index point/range, full scan, large HashJoin, index UPDATE, and concurrent read/write overlap. |

The shared G1 evidence policy composes with these procedures; G2–G4 text is preserved. The affected analysis-only area is **O42-088–126** for execution measurements, NULL regimes, vector study, end-to-end workloads, and resource metrics. G6 and G7 obligations remain open. No new G5 finding was identified.

`git diff --check` passed. The cached-diff digest and clean index were preserved. No change outside the authorized Verification edit was created; no implementation, build, test, sanitizer, or benchmark was run.

CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED

CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED

CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED

ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED

V42-G2 — STORAGE/B+:
    CLOSED

V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED

V42-G4 — FRONT-END/PLANNING:
    CLOSED

V42-G5 — EXECUTION:
    CLOSED

V42-G6 — PARALLEL SCALING:
    OPEN / UNMODIFIED

V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    OPEN / UNMODIFIED

V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED

V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED

V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED

V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS

REMAINING GROUPED VERIFICATION GAPS:
    6

DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY

CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED

INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO

REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G6:
    CAPABILITY-CONDITIONAL PARALLEL
    EXECUTION SCALING METHODOLOGY

END CHAPTER-42 VERIFICATION FIX V42-G5 —
EXECUTION MICROBENCHMARK AND
END-TO-END WORKLOAD ACTIVATION,
REPLAYABILITY, AND METRIC PROVENANCE.

CHAPTER 42 — VERIFICATION FIX V42-G6: CAPABILITY-CONDITIONAL PARALLEL EXECUTION SCALING METHODOLOGY

**CLOSED V42-G6 at the documentation-repair level.** I added only [Parallel Execution Scaling Benchmark](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26139). No benchmark was run, and no implemented parallel capability is claimed.

The procedure follows the live §42.4 rule: a physical path that *supports* multi-worker execution is measured against a one-worker baseline at representative supported levels. It inventories capability through the Chapter-32 execution owner; a parallel-ready but single-worker-only path is N/A, while a claimed multi-worker path that cannot be exercised or observed is not verified. Each applicable series uses the same immutable physical plan and replayable workload, records configured versus actual workers and task/morsel coverage, and requires useful work on at least two workers for its positive control.

It reuses Parallel Execution Tests for correctness and Chapter-40 profiles for work and resource observations. Elapsed time, work rate, measured CPU, query memory, spill, and applicable BufferPool/I/O evidence are recorded at each level. Speedup and efficiency are optional diagnostics without pass thresholds. Scheduler, synchronization, CPU-cache, memory-bandwidth, and I/O attributions require supporting observations; an unexplained plateau remains **UNRESOLVED**. Conditional fixtures cover SeqScan, HashJoin build/probe, HashAggregate, Sort, and any other capability-enabled family. Multi-query concurrency is not substituted for one-query scaling, and the section makes no G7 structural-hot-path claim.

The self-review is **COVERED** for capability A–I, scaling evidence A–L, and limit attribution A–I as *procedures*. For the four named path families, the fixture, correctness owner, measures, and diagnostic method are specified; one-worker and multi-worker results remain unrun. [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:303) reports no parallel execution runtime, so current measured path support and positive controls are not asserted. The negative cases A–X are classified by the procedure: unsupported paths are N/A; idle workers, changed paths, missing participation, or wrong results do not prove scaling; changed spill/I/O regimes require qualification; and a plateau without causal evidence remains unresolved. No new G6 finding was identified.

Repository audit: initial and final HEAD are `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). The pre-existing unstaged G1–G5 Verification delta was `+508/-2`; G6 added `+100/-0`, giving a cumulative `+608/-2`. The index remains clean with no staged files and the unchanged cached-diff SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. The pre-existing untracked `Chapter 42/SYNC.md` was preserved. No external repository-state change was observed; no other file was changed by this task. `git diff --check` passed. G1–G5 and G7–G11 sections were not edited.

G6 addresses the analysis-only parallel-scaling obligation family following G5’s O42-088–126 execution area. The available review artifact does not enumerate exact G6 O42 row IDs, so I have not guessed them or inserted a ledger into Verification.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    OPEN / UNMODIFIED
V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED
V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS
REMAINING GROUPED VERIFICATION GAPS:
    5
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G7:
    STRUCTURAL HOT-PATH CONSTRAINT
    EVIDENCE AND NONVACUITY ORACLES
```

CHAPTER 42 — VERIFICATION FIX V42-G7: STRUCTURAL HOT-PATH CONSTRAINT EVIDENCE AND NONVACUITY ORACLES

**CLOSED V42-G7 at the documentation-repair level.** I added only [Execution Hot-Path Structural Evidence](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26239). It maps all 13 live §42.4 constraints to direct structural procedures or named correctness owners. No procedure was executed.

The central rule is explicit: throughput, elapsed time, memory totals, and benchmark labels cannot prove hot-path structure. Test-only occurrence traces are permitted without requiring a production API. The new procedures require substantial steady-state, multi-chunk work for allocation, dispatch, and `Value` construction; distinguish setup and amortized growth from one-allocation-per-row/cell behavior; trace spill request order and size; observe backing reuse; compare an independently derived required-column set with per-column decode events; and measure actual hash/directory layout across scales.

| Constraints | Disposition | Primary evidence |
|---|---|---|
| 1–3 | COVERED — DIRECT G7 | Scoped allocation, redundant-dispatch, and generic-`Value` construction events over nonvacuous typed hot work |
| 4–5 | COVERED — CROSS-SUITE | V23-G/H, V27-C/O and V24-F, V26-M, V28-G, V29-G/N, V30-D lifetime and producer-poison oracles |
| 6 | COVERED — CROSS-SUITE | V24-B/D/F committed-capacity and ownership ledger, including aggregate growth of small allocations |
| 7–10 | COVERED — DIRECT G7 | Spill-I/O request trace; chunk/vector backing generations; independently expected scan columns versus decode events; hash/directory structure scale series |
| 11 | COVERED — CROSS-SUITE | V24-H/J and V28-K, V29-N, V30-F/H resource-pressure and forced-spill correctness |
| 12 | COVERED — CROSS-SUITE | Chapter-40 and EXPLAIN ANALYZE/profile owner observations; optimized work must remain attributable |
| 13 | COVERED — CONDITIONAL | Profile-to-change evidence when permitted explicit SIMD, radix, or prefetch is present; absent options are N/A, and JIT remains deferred |

The procedures preserve storage corruption validation while prohibiting unnecessary scalar decode of valid unrequired columns; include an unrequired wide VARCHAR and a predicate-only required column; reject dangling retained varlen references; and assess “large/sequential where practical” and “compact/contiguous where practical” against owner structure and documented technical reasons, without universal numeric thresholds or algorithm mandates. Optional optimization evidence includes a motivating profile, comparable baseline, correctness equivalence, and resource/profile effects—not an implementation authorization.

Nonvacuity review: constraints 1–3 A–H, constraints 4–11 A–J, and constraints 12–13 A–H are **COVERED as methodology**, including positive work, page/producer poisoning, actual spill, multiple requests and chunk resets, wide-column and hash-scale controls, and optional-feature applicability. The negative matrices A–AF are classified by those oracles: one-per-row allocation, redundant row dispatch, hot-cell `Value`, pin-for-string lifetime, expired blocking references, unaccounted growth, pathological tiny spill I/O, needless full-column decode, and unsupported profile-free optimization are adverse evidence; lawful setup growth, data-dependent branching, separate result conversion, owner-accounted memory, valid reset/reuse, and documented where-practical exceptions remain permissible. No new G7 finding was identified.

Repository audit: initial and final HEAD are `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). The pre-existing G1–G6 unstaged Verification delta was `+608/-2`; G7 added `+137/-0`, giving `+745/-2` cumulative. The index remains clean with no staged files and cached-diff SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. The pre-existing untracked `Chapter 42/SYNC.md` was preserved. No external repository-state change was observed, and no file outside `docs/VERIFICATION.md` was changed by this task. `git diff --check` passed. G1–G6 and G8–G11 sections were not edited.

G7 addresses the Chapter-42 structural-constraint O42 family. The available review artifact does not enumerate its exact row IDs, so I have not guessed them or inserted an O42 ledger into Verification.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    CLOSED
V42-G8 — ANALYZE PERFORMANCE:
    OPEN / UNMODIFIED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED
V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS
REMAINING GROUPED VERIFICATION GAPS:
    4
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G8:
    ANALYZE PERFORMANCE BENCHMARK
    COVERAGE, MEMORY, I/O, AND
    METRIC PROVENANCE
```

CHAPTER 42 — VERIFICATION FIX V42-G8: ANALYZE PERFORMANCE BENCHMARK COVERAGE, MEMORY, I/O, AND METRIC PROVENANCE

**CLOSED V42-G8 at the documentation-repair level.** I added only [ANALYZE Performance Benchmark](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26416). It supplies procedures for all six live §42.5 measurements—rows/second, columns analyzed, peak maintenance memory, HLL/MCV/sample memory, statistics payload bytes, and full-scan I/O—without running ANALYZE or changing its architecture.

The benchmark uses replayable table fixtures and separate row-scale and column-scale series. Its primary interval runs from resolved `PhysicalAnalyze` collection through successful *statement* completion, including required collection, validation, encoding, and transactional statistics-row writes; terminal transaction COMMIT and global C4/C5 cache publication are separate. A sample needs a complete, correctness-valid generation. Rows/sec divides directly observed, snapshot-visible rows accepted by the full-scan collector by that interval. Physical versions and heap pages visited are recorded separately. One analyzed column means one distinct base `ColumnId` that entered collection and has a valid required column-statistics member—not one schema position assumed active or one count per HLL/MCV/histogram structure.

The procedure sources peak simultaneous ANALYZE-owned memory from the `PhysicalAnalyze`/QueryMemoryManager and collection-owner ledger, with HLL, MCV, and reservoir memory reported separately. It uses §34.14’s validated, reassembled TABLE/COLUMN/INDEX payload lengths for payload bytes. Full-scan I/O separates observed logical heap-page visits, BufferPool hits/misses, and completed physical heap reads; index-statistics reads and publication WAL/writes are separately labeled. Positive controls require a multi-page full scan, active HLL/MCV/sample states, and a nonempty valid payload. Existing Chapter-34/V34 suites remain the statistical-quality, visibility, persistence, and publication oracles.

The special ambiguity gates are resolved by the live owners: §34.7 and V34-036 establish the visible analyzed-row work population; §31.12.1 identifies `PhysicalAnalyze`’s QueryMemoryManager accounting; and §34.14 defines exact scope-payload lengths. No new G8 finding was identified. The negative cases A–T are classified by these boundaries: estimates, schema width, RSS, component-peak sums, file growth, or page-count inference are insufficient substitutes for owner observations; failed/partial ANALYZE is not a successful sample; and a sampled heap subset does not activate the required v1 full scan. Nonvacuity A–M, provenance A–N, and scope separation A–I are **COVERED as methodology**.

Initial and final HEAD: `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). The pre-existing G1–G7 unstaged Verification delta was `+745/-2`; G8 added `+103/-0`, giving a cumulative `+848/-2`. The index remains clean, with no staged files and unchanged cached-diff SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. The pre-existing untracked `Chapter 42/SYNC.md` was preserved. No external repository-state change was observed. Only `docs/VERIFICATION.md` was edited; G1–G7 and G9–G11 sections were not changed. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

Analysis-only obligations **O42-141–O42-146** now have the corresponding six benchmark procedures; these IDs were not inserted into Verification.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    CLOSED
V42-G8 — ANALYZE PERFORMANCE:
    CLOSED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    OPEN / UNMODIFIED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED
V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS
REMAINING GROUPED VERIFICATION GAPS:
    3
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G9:
    BASE-ACCESS AND CARDINALITY-ESTIMATOR
    CALIBRATION WORKLOADS, PLAN-SWITCH
    DEMONSTRATION, AND ERROR DISTRIBUTIONS
```

CHAPTER 42 — VERIFICATION FIX V42-G9: BASE-ACCESS AND CARDINALITY-ESTIMATOR CALIBRATION WORKLOADS, PLAN-SWITCH DEMONSTRATION, AND ERROR DISTRIBUTIONS

**CLOSED V42-G9 at the documentation-repair level.** I added [Cardinality-Estimator Quality Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27021) and expanded [Access Path Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27079). No benchmark was run.

The access-path procedure now requires comparable, correctness-valid measurements of *both* `PhysicalSeqScan` and a legal `PhysicalIndexScan`, including the alternative the optimizer did not select. It defines controlled series for all seven live §42.5 dimensions: selectivity; heap pages/rows per page; index/heap correlation; cache budget; required output width; VARCHAR width; and dead-version fraction. Owner observations establish actual page, tuple-version, candidate/RID, MVCC, decode, BufferPool, I/O, CPU, and output work. It records the validated CostConfig, statistics identity, both component costs, and selected path. A positive control must show the same logical query shape switching between legal SeqScan and IndexScan choices as measured conditions cross a cost break-even—without a universal selectivity cutoff or a claim that abstract cost predicts milliseconds. Measured disagreement is retained for the existing §36.4 calibration owner.

The estimator procedure uses independent actual cardinalities and Chapter 40’s q-error definition, including exact zero and infinity handling. It retains raw case results, provenance, quantiles, and identified tail cases, stratified across baseline, skew, MCV, NULL, and inter-column correlation-limitation workloads. Model-limitation cases remain in the distribution; they are not silently discarded or treated as semantic-empty proofs. Index/heap correlation for access costing remains distinct from inter-column correlation for estimator quality. No q-error target or extended-statistics capability was added.

The requested self-reviews are **COVERED as methodology**: base-access dimensions A–H, plan switch A–J, estimator distribution A–L, metric provenance A–P, and scope separation A–I. The negative matrices A–AB follow those procedures: unrelated queries, estimate-as-truth, guessed density/correlation/width, average-only q-error, omitted infinities, and hidden limitation cases are insufficient; independently observed work, both-path measurements, and auditable distribution tails are valid evidence. No new G9 finding was identified.

Repository audit: initial and final HEAD are `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). The pre-existing G1–G8 unstaged Verification delta was `+848/-2`; G9 added `+114/-0`, giving `+962/-2` cumulative. The index remains clean, with no staged files and unchanged cached-diff SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. The pre-existing untracked `Chapter 42/SYNC.md` was preserved. No external repository-state change was observed; only `docs/VERIFICATION.md` was edited. G1–G8 and G10–G11 were not changed. `git diff --check` passed.

Analysis-only **O42-147** remains complete through the existing §36.4 cross-suite procedure. This repair supplies methodology for **O42-148–O42-160** without adding the ledger to Verification.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    CLOSED
V42-G8 — ANALYZE PERFORMANCE:
    CLOSED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    CLOSED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    OPEN / UNMODIFIED
V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS
REMAINING GROUPED VERIFICATION GAPS:
    2
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G10:
    OPERATOR COST CALIBRATION, OPTIMIZER
    SEARCH PERFORMANCE, INTERESTING-ORDER,
    AND MEMORY-AWARE BENCHMARK METHODOLOGY
```
CHAPTER 42 — VERIFICATION FIX V42-G10: OPERATOR COST CALIBRATION, OPTIMIZER SEARCH PERFORMANCE, INTERESTING-ORDER, AND MEMORY-AWARE BENCHMARK METHODOLOGY

**CLOSED V42-G10 at the documentation-repair level.** The live §42.6 text uses a configured `exhaustive_join_limit` and cases on both sides of it; it does not mandate the old exact join-count grid. I edited only [Cost Model Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27530), [Optimizer Performance Benchmarks](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27613), and focused measured paragraphs in [Physical Property and Enforcement Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27220) and [Memory/Spill Plan Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27288).

The cost procedure now covers all nine §42.6 categories: SeqScan, IndexScan, HashJoin, NestedLoopJoin, IndexNestedLoopJoin, HashAggregate, Sort, TopN, and spill paths. It reuses G9’s directly measured base-access alternatives, requires correctness-valid direct measurement of other legal alternatives, and compares structured predicted costs with measured runtime *and separate resource dimensions*. Ties and noisy overlap remain inconclusive; persistent disagreement is calibration evidence. The former unspecified “usually faster” criterion was removed. Abstract cost is not milliseconds, and no ranking-agreement percentage was added.

The search procedure retains the Verification-owned join-count grid but requires a recorded valid threshold and below/at/immediately-above/larger cases. Under the live owner, equality is exhaustive; above-threshold search is heuristic. Direct trace events—not planning time or relation count alone—must prove the mode transition and distinguish it from planning-arena fallback. One invocation supplies wall time, subset and partition counts, alternatives actually costed, separately scoped memo retained/pruned counts, planning-arena peak, and greedy/local-improvement work.

The interesting-order positive control compares two complete, correct plans: a locally cheaper unordered provider plus Sort and a locally dearer ordered provider whose complete plan costs less because enforcement is avoided. Both are directly measured. The memory-aware control holds statistics and CostConfig fixed while changing §38.19 execution-memory budget, observes estimated spill and a supported plan-choice change, and checks actual QueryMemoryManager and spill evidence. It keeps §38.21 planning-arena budget separate. The star-schema workload remains SHOULD-level supplementary stress; [No Benchmark Gaming](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27698) was not edited.

The requested self-reviews are **COVERED as methodology**: operator A–I, ranking A–J, search boundary A–J, search metrics A–J, interesting order A–J, memory-aware A–N, and scope separation A–I. The negative matrices A–AP are classified by those procedures: unmeasured alternatives, invented scalar “actual cost,” forced noisy winners, guessed search counters, resource-fallback-as-threshold, property-free Sort comparisons, and plan changes caused by changing statistics do not establish the corresponding target. No new G10 finding was identified.

Repository audit: initial and final HEAD are `500cde09c7b9570fe8dd0dcd575b4d4de352b598` (`applied FIX-A4 42 in ARCHITECTURE`). The pre-existing G1–G9 unstaged Verification delta was `+962/-2`; G10 contributed `+128/-2`, giving `+1090/-4` cumulative. The index remains clean with no staged files and unchanged cached-diff SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. The pre-existing untracked `Chapter 42/SYNC.md` was preserved. No external repository-state change was observed, and only `docs/VERIFICATION.md` was edited. G1–G9 and G11 were not changed. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

Analysis-only **O42-161–170 and O42-172–183** now have corresponding procedures. **O42-171** remains nonnormative/no procedure required; **O42-184** remains complete at SHOULD level. These IDs were not added to Verification.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186

V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    CLOSED
V42-G8 — ANALYZE PERFORMANCE:
    CLOSED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    CLOSED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    CLOSED
V42-G11 — BENCHMARK-GAMING DETECTION:
    OPEN / UNMODIFIED

P42-7 — VERIFICATION SYNCHRONIZATION:
    IN PROGRESS
REMAINING GROUPED VERIFICATION GAPS:
    1
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    NOT SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — VERIFICATION FIX V42-G11:
    BENCHMARK-FINGERPRINT DETECTION,
    METAMORPHIC ANTI-GAMING ORACLES,
    AND GENERALITY EVIDENCE
```

CHAPTER 42 — VERIFICATION FIX V42-G11: BENCHMARK-FINGERPRINT DETECTION, METAMORPHIC ANTI-GAMING ORACLES, AND GENERALITY EVIDENCE

**Result: CLOSED V42-G11 at repair level.** The planned Chapter-42 Verification repairs are complete at fix level; the independent read-only closure audit has not been performed.

Only [docs/VERIFICATION.md](</home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:27698>) was edited. The `### No Benchmark Gaming` section now defines identity-only invariance tests, general-input sensitivity tests, generated holdouts, structured plan and trace comparisons, and a supplementary production-decision-code review. It reuses the existing optimizer correctness, diagnostics, fingerprint-collision, G9, and G10 procedures without changing them.

The live §42.6 contract prohibits hard-coding benchmark-specific query text or table names into optimizer decisions and requires improvements to arise from general statistics, semantic rewrites, properties, costing, and search. The new procedure directly tests both sides: changing only benchmark identity must not change a decision, while crossing an owner-supported cost, property, memory, or search boundary must produce the corresponding decision or mode change. It distinguishes legitimate names and IDs used for binding, lookup, diagnostics, and structural ties from benchmark-identity selectors. `PlanFingerprint` remains diagnostic, not a plan-identity or selection oracle.

The controls cover raw-SQL byte and formatting variants; certified logical equivalences; aliases; ordinary table, index, and column names; benchmark-like name sentinels and name swaps; semantic-ID rekeying; and isomorphic many-join/star fixtures. Comparisons use structured plans, candidates, costs, and search evidence modulo a declared identity mapping. Strict-winner cases avoid tie ambiguity; exact ties and bounded heuristic ordering follow their existing owners. Fixed-seed holdouts test both stable cost regions and boundary crossings. Every variant retains correctness and decision-provenance evidence. A static string scan is supplementary, not a substitute for behavioral tests. No secret tests, persistent anti-gaming format, new public API, universal speedup target, or external benchmark-compliance claim was added.

The requested negative-case classifications and self-reviews are covered by the procedure: name cases A–E; query-text F–J; IDs/fingerprints K–P; shape/generality Q–V; metamorphic validity W–AD; static review AE–AK. Invariance A–J, sensitivity A–H, generality A–H, fingerprint A–G, and decision-basis A–I are **COVERED** at methodology level. O42-185 and O42-186 are repaired at fix level; no new G11 finding was identified. G1–G10 remain unchanged.

Repository record:

- Initial and final HEAD: `500cde09c7b9570fe8dd0dcd575b4d4de352b598` — `applied FIX-A4 42 in ARCHITECTURE`.
- Initial worktree: modified `docs/VERIFICATION.md` with pre-existing G1–G10 delta `+1090/-4`; pre-existing untracked `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 42/SYNC.md`.
- G11-only delta: `+104/-7`. Final cumulative Verification diff: `+1194/-11`.
- Initial and final index: clean, no staged files. Cached-diff SHA-256: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
- No external repository-state change was observed. The review artifact was preserved. `git diff --check` passed.
- No implementation, build, test, sanitizer, or benchmark run occurred; no changes were created outside the authorized edit.

```text
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE:
    CLEAN — CLOSED
ATOMIC O42 OBLIGATIONS:
    186
V42-G1 — SHARED BENCHMARK EVIDENCE POLICY:
    CLOSED
V42-G2 — STORAGE/B+:
    CLOSED
V42-G3 — TRANSACTION/RECOVERY/VACUUM:
    CLOSED
V42-G4 — FRONT-END/PLANNING:
    CLOSED
V42-G5 — EXECUTION:
    CLOSED
V42-G6 — PARALLEL SCALING:
    CLOSED
V42-G7 — STRUCTURAL HOT-PATH EVIDENCE:
    CLOSED
V42-G8 — ANALYZE PERFORMANCE:
    CLOSED
V42-G9 — BASE-ACCESS/ESTIMATOR CALIBRATION:
    CLOSED
V42-G10 — COST/OPTIMIZER PERFORMANCE:
    CLOSED
V42-G11 — BENCHMARK-GAMING DETECTION:
    CLOSED
P42-7 — VERIFICATION SYNCHRONIZATION:
    REPAIRS COMPLETE AT FIX LEVEL —
    INDEPENDENT CLOSURE PENDING
REMAINING GROUPED VERIFICATION GAPS:
    0
DEDICATED V42 SUITE:
    NOT NEEDED —
    OWNER-SUITE BENCHMARK REPAIRS ONLY
CHAPTER 42 VERIFICATION:
    REPAIRS COMPLETE AT FIX LEVEL —
    INDEPENDENT CLOSURE PENDING
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    CLEAN; NO STAGED FILES
    TASK MODIFIED: NO
REVIEW ARTIFACTS:
    PRE-EXISTING UNTRACKED Chapter 42/SYNC.md
    TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
NEXT AUTHORIZED TASK:
    CHAPTER 42 — INDEPENDENT READ-ONLY
    VERIFICATION CLOSURE AUDIT
```

END CHAPTER-42 VERIFICATION FIX V42-G11 — BENCHMARK-FINGERPRINT DETECTION, METAMORPHIC ANTI-GAMING ORACLES, AND GENERALITY EVIDENCE.