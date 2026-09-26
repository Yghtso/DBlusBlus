**Initial verdict: NEEDS ARCHITECTURE FIX.** Chapter 42 has a coherent performance-aware design and no need for invented throughput targets, but its opening coverage requirement is temporally open-ended. Several exact benchmark fixtures are also frozen in Architecture despite Verification already owning their procedures. This is an initial analysis, not independent Chapter-42 closure.

### Repository and chapter bounds

Initial and final HEAD: `bab7ba110e0baa27a37c5e683d77a90091679d01` — `synced VERIFICATION after chapter 41 ARCHITECTURE fix`. At entry and exit, the index contained staged `docs/VERIFICATION.md` and Chapter-41 `POST-SYNC.md`; there were no unstaged tracked changes. The staged diff was 193 insertions and 39 deletions across those two files. Its digest, `eac5f95f473c42a117ae20a17372290394b4463a01aeb03ad6c53638d03e2202`, was unchanged. No external change was observed during this audit. I made none. `git diff --check` passed.

Live [Chapter 42](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30654) is **“Performance Requirements,” lines 30654–30933**, immediately after Chapter 41. Its subsections are §42.1 B+ tree; §42.2 transaction/durability/recovery/vacuum; §42.3 catalog/front-end/logical planning; §42.4 execution/hot paths; §42.5 statistics/calibration; and §42.6 optimizer search/cost. [Appendix A](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30934) begins at line 30934.

### Contract assessment

The absence of machine-independent TPS, latency, or recovery-time targets is sound. Chapter 42 chiefly requires **measurement coverage and explainable performance choices**, with a few genuine implementation constraints. Missing required coverage, violating a hot-path owner rule, or claiming an Architecture-sensitive improvement without adequate evidence can be assessed. Merely measuring a slow result, absent a numerical owner target, is evidence for optimization—not automatically database nonconformance or a runtime SQL error. Correctness remains a prerequisite for counting a benchmark result.

The opening’s page/BufferPool/heap/tuple/B+/join/group-commit/concurrency coverage and micro-versus-end-to-end distinction are stable performance dimensions. “Relevant measurements include” is appropriately contextual: p50/p95/p99, CPU, I/O, WAL, and cache metrics need meaningful applicability, not fabricated values for every microbenchmark. Warm versus deliberately *less-warm* conditions avoids promising a perfectly cold OS cache. Repeated, representative evidence is a legitimate SHOULD; warmup, repetitions, machine capture, baseline pairing, noise treatment, and regression thresholds belong in [Verification’s benchmark procedures](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:57), not Architecture.

The normative inventory is: one explicit `MUST NOT` against optimizer benchmark fingerprints; four uppercase `SHOULD` uses for storage-residency evidence, performance-change evidence, B+ residency comparison, and group-commit concurrency; plus operative lower-case requirements such as “must eventually cover,” “are required,” “measure at least,” the §42.4 strong constraints, and §42.6 “must demonstrate.” “Useful,” “representative,” “when available,” “where practical,” and “recommended” generally preserve scope or instrumentation flexibility rather than imposing universal hard gates.

### Subsystem and owner check

| Area | Assessment |
|---|---|
| §42.1 B+ | Lookup, insert, erase, scans, duplicate behavior, split/height/occupancy, BufferPool and optional latch-wait measurements cover distinct mechanisms. INT64, short/long VARCHAR and composite keys are legitimate risk classes; exact key fixtures belong to Verification. Hot/resident versus BufferPool-exceeding is a valuable Architecture distinction, not a demand for uncontrolled OS-cache state or a particular B+ optimization. |
| §42.2 durability | Group-commit throughput, latency, sync and WAL measures do not relax the [durable-commit owner](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9932). Checkpoint/recovery clean/dirty states expose mechanism differences without redefining recovery correctness. Vacuum’s “exact index-cleanup rate” is best read as the rate of required exact-entry cleanup, not permission or a mandate to retire every entry immediately; Chapters 14 and 41 retain the cleanup/grace owner. The update/delete/abort/duplicate/long-snapshot classes are stable risks. |
| §42.3 front end | Lexer/parser/binder/catalog/schema/rewrite and allocation dimensions are useful without mandating a parser algorithm. But “AST/plan arena” and “Arena-based ownership should…” can be read as prescribing an AST allocator. [§18.14](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15457) leaves concrete AST allocation open; the [parser benchmark](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:14764) expressly compares representations without preferring an arena. Planning-arena ownership is separately fixed by §38.21. |
| §42.4 execution | Scan, expression, hash, aggregate, sort, allocation, NULL-free/heavy, end-to-end and applicable memory/I/O measures cover the important Chapter-22–32 risks. The **1024** v1 standard is genuinely owned by [§23.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19449); its benchmark candidate grid is methodology. Changing the default requires an Architecture revision, not merely a favorable local run. Parallel scaling is meaningful when that capability is exercised; an initial single-worker baseline need not fabricate multi-worker measurements. |
| §42.5 statistics/cost | ANALYZE memory/I/O/payload measures do not alter HLL quality or publication correctness. [§36.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26715) already owns calibration primitives and relative-weight proposals as deployment/configuration data, not persisted database format. Varying selectivity, correlation, width, cache and dead-version inputs to expose SeqScan/IndexScan break-even is falsifiable and consistent with [§36.17](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27210). It forbids a universal hard-coded selectivity percentage, not an equivalent derived fast path. Error distributions here do not create another HLL acceptance threshold. |
| §42.6 optimizer | Comparing *relative* predicted and measured rankings preserves abstract cost units; it does not require cost to equal milliseconds or perfect ranking. Search counters consume [§38.21](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28803). The exhaustive/heuristic transition is owned by [§37.11–§37.13](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27608), with a configurable threshold initially defaulting to 10. A benchmark should straddle the **configured** threshold, not assume a fixed fixture list always does. Interesting-order and memory-aware cases add measured cost evidence to correctness procedures. The star schema is recommended, not mandatory architecture. The fingerprint `MUST NOT` is a sound anti-gaming rule; general statistics, rewrites, properties, costing, search, and legitimate semantic inputs remain available. |

The thirteen §42.4 strong constraints were checked individually against their owners:

| # | Result |
|---:|---|
| 1 | Avoid per-row/cell heap allocation in steady hot execution; intent matches batching, but literal wording needs an exceptional-growth/steady-path distinction. |
| 2 | Batch specialization matches §§25.3 and 32.11; “when available” leaves a small applicability ambiguity, not a mandate for one dispatch mechanism. |
| 3 | Generic `Value` is not a hot-cell representation under §§17.11/25; constants, catalog and diagnostics remain legal. |
| 4–5 | Heap-page pins solely for VARCHAR retention are avoided; blocking varlen ownership matches §23.11–§23.12. |
| 6 | §24.4’s precise accounting rule controls; “large” in §42 is only a weaker summary, not an exemption for many small unbounded allocations. |
| 7 | Large/sequential spill I/O is an intentionally practical preference, consistent with §24.9’s configurable target; no exact layout is mandated here. |
| 8 | Chunk/vector reuse matches §23.13 and remains subject to borrowed-view lifetime. |
| 9 | Required/predicate-dependent column decoding matches §27.12; locating offsets is not the same as materializing unneeded values. |
| 10 | Compact/contiguous hash storage “where practical” permits a measured alternative. |
| 11 | Tiny-budget/forced-spill correctness is a cross-cutting reminder, not a performance exception to Chapter 41. |
| 12 | “Instrumentation exists before aggressive optimization” expresses a valid [Chapter-40](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29653) dependency but is phrased as project chronology; measurable/profilable optimization is the timeless contract. |
| 13 | Profile evidence appropriately gates complexity. Naming JIT must not override the explicit [v1 JIT non-goal](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:67) and its revision requirement. |

No Chapter-42 text requires an online autotuner, persistent profiler, hardware-counter framework, benchmark-specific plan winner, or absolute regression percentage. Chapter 40 owns metric identity/applicability; Chapter 41 owns correctness, faults and small-resource behavior. Chapter 42 consumes both without changing transaction, WAL, MVCC, SQL, q-error, or publication semantics. [Development](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md:242) properly owns the `clang-bench` build invocation. [Project State](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:258) confirms most Chapter-42 subsystems do not yet exist; that is not an Architecture defect or implementation authorization.

### Findings and repair order

| ID / class | Live location and consequence | Smallest repair |
|---|---|---|
| **P42-1 — MAJOR** | [Opening](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30656): “benchmark program must **eventually** cover at least” is an indefinitely postponable, project-sequencing formulation for an otherwise mandatory Architecture evidence obligation. §42.4’s “instrumentation exists before…” has the same temporal tendency. | State the timeless required performance-evaluation coverage and Chapter-40 measurability condition; leave implementation timing and run procedure elsewhere. **Blocks initial-review passage.** |
| **P42-2 — BENCHMARK-METHODOLOGY-PLACEMENT** | [§§42.2–42.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30732): exact `1,2,4,8,16,32+` threads, `256…4096` vector study, `2…30` join-count grid, and 100-column SELECT freeze fixture choices already repeated in Verification. The join grid may miss a changed configured threshold. | Retain representative concurrency/vector/key-size/search-regime *dimensions* and the 1024 owner default; let Verification own exact grids and choose join counts on both sides of the retained configured threshold. |
| **P42-3 — MINOR** | [§42.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30748): AST/plan “arena” wording can be mistaken for an AST allocation-strategy requirement, unlike §18.14 and the current benchmark oracle. | Measure front-end allocation count/bytes regardless of representation; name an arena only when actually used. Preserve §38’s mandatory planning arena. |
| **P42-4 — MINOR** | [§42.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30805): unqualified parallel-scaling measurement can be read as requiring multi-worker data when only the permitted initial single-worker path exists. | Condition the scaling dimension on a supported parallel capability. |
| **P42-5 — MINOR** | [§42.4 constraint 13](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30827): profile evidence could be misread as sufficient to introduce JIT despite §1.3’s v1 non-goal. | Qualify advanced options by existing capability scope and explicit Architecture revision for deferred JIT. |
| **P42-6 — MINOR** | [§42.4 constraints 1–2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30827): literal “no one heap allocation per row/cell” and undefined specialization “available” may overstate the amortized hot-path intent. | Tie them to the steady hot path and the existing batch-specialization owners; do not prohibit owner-required exceptional allocation or prescribe an unmeasured algorithm. |
| **P42-7 — VERIFICATION-SYNC-ONLY** | Existing benchmark sections cover many lists, but do not yet form a complete Chapter-42 procedure for ANALYZE/calibration, applicable metric handling, environment/repetition controls and threshold-relative join search. | Synchronize Verification **after** Architecture repair and independent closure; do not edit it now. |

Counts: **BLOCKING 0; MAJOR 1; MINOR 4; EDITORIAL 0; DESIGN-SCOPE 0; VERIFICATION-SYNC-ONLY 1; BENCHMARK-METHODOLOGY-PLACEMENT 1.** I found no hard contradiction in Chapters 1–41. The role and wording issues are sufficient that Chapter 42 is **not yet ready** for an independent closure audit.

Repair order: **A1** make performance-evidence coverage timeless and falsifiable (P42-1); **A2** remove exact fixture-grid ownership while retaining architectural dimensions (P42-2); **A3** clarify representation and hot-path scope (P42-3/P42-6); **A4** condition parallel and deferred advanced capability wording (P42-4/P42-5). Only then perform an independent read-only Architecture closure audit. Verification synchronization is a separate later task.

CHAPTER 41 ARCHITECTURE: CLEAN — CLOSED
CHAPTER 41 VERIFICATION: CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE: NEEDS ARCHITECTURE FIX
CHAPTER 42 ARCHITECTURE FINDINGS:
 BLOCKING: 0
 MAJOR: 1
 MINOR: 4
 EDITORIAL: 0
 DESIGN-SCOPE: 0
 VERIFICATION-SYNC-ONLY: 1
 BENCHMARK-METHODOLOGY-PLACEMENT: 1
CHAPTER 42 VERIFICATION: NOT SYNCHRONIZED
INDEPENDENT CHAPTER-42 ARCHITECTURE CLOSURE: NOT PERFORMED
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: STAGED `docs/VERIFICATION.md` AND CHAPTER-41 `POST-SYNC.md`; AUDIT MODIFIED: NO
REVIEW ARTIFACTS: CHAPTER-41 `POST-SYNC.md` STAGED; AUDIT MODIFIED: NO
AUDIT-CREATED CHANGES: NONE
NEXT AUTHORIZED TASK: **CHAPTER 42 — ARCHITECTURE FIX A1: TIMELESS PERFORMANCE-EVIDENCE COVERAGE**

END CHAPTER-42 INITIAL READ-ONLY ARCHITECTURE ANALYSIS.