**Initial verdict: NEEDS ARCHITECTURE FIX.** Chapter 40 has a sound, appropriately educational observability design, but three mandatory boundaries are not yet precise enough for independent implementations: counter meaning, ordinary `EXPLAIN` execution, and the interpretation of incomplete execution profiles and q-error. This is an initial review, not an Architecture closure audit. No files were changed.

## Repository and chapter baseline

Initial and final HEAD: `637520cb9f22f7aee7a7fb4b3058ef386b9e48c6` — `synced VERIFICATION after chapter 39 ARCHITECTURE fix`. The worktree and index were clean at both checks. `git diff --check` passed; audit-created changes: **none**.

The live chapter is [“40. Observability and EXPLAIN”](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29605), lines 29605–29897; [Chapter 41](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29898) begins at 29898. Its complete inventory is §§40.1 Internal metrics; 40.2 EXPLAIN; 40.3 transaction/WAL/recovery/vacuum metrics; 40.4 debug introspection; 40.5 logical EXPLAIN/front-end diagnostics; 40.6 execution profiling, with 40.6.1 EXPLAIN ANALYZE and 40.6.2 pipeline profiling; 40.7 statistics/estimation/base-access diagnostics; and 40.8 optimizer trace/fingerprint/estimate error. There is no final invariant subsection.

Prior independent reviews in this workflow closed Chapters 31–39 at the documentation level; the live files contain the repaired Chapter-39 contract and V39 procedures, but `PROJECT_STATE.md` is not a review-closure register. This review did not re-audit those closures. The *actual implementation state* is not “nothing implemented”: [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:9) records an implemented storage foundation and an authorization-gated, unimplemented BufferPool; SQL, execution, optimizer, and Chapter-40 facilities are absent. It also records two storage implementation mismatches against settled Architecture. None is a Chapter-40 Architecture defect.

## Findings

| Finding | Classification and precise defect | Reproducer and smallest repair |
|---|---|---|
| **N40-1** | **MAJOR — mandatory metric identity is underspecified.** [§40.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29607) requires logical/physical page reads and buffer hits/misses; [§40.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29641) mixes event totals, instantaneous gauges, durations, and approximate maintenance quantities without a minimum unit, scope, or failed-event rule. §7.6 owns fetch outcomes; §§9–15 own transaction truth; §§12–13 own WAL/recovery. Those owners do not define how Chapter-40 counters count them. | Two concurrent fetches coalesce into one page load: implementations may report one or two “logical page reads,” and may count a failed load as a miss or not. A committed transaction with lost C6 acknowledgement may or may not increment “committed” if that counter is interpreted as responses. Define a small diagnostic vocabulary in §§40.1/40.3: event total versus gauge versus duration; relevant scope; successful/attempted event boundaries; semantic COMMITTED/ABORTED rather than acknowledgement; approximate/saturated/unavailable reporting. Do **not** prescribe a metrics registry or fixed names. |
| **N40-2** | **MAJOR — ordinary `EXPLAIN` is not expressly nonexecuting.** [§40.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29628) requires plan information; [§40.6.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29781) expressly says `EXPLAIN ANALYZE` executes. [§18.10.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15022) owns the SELECT-only syntax, but no live passage explicitly states whether ordinary `EXPLAIN` may start physical execution. | `EXPLAIN SELECT 1 / 0` can either return a plan without runtime arithmetic or execute and raise a Chapter-39 execution error. That is a client-observable semantic difference, not a Verification fixture choice. Add one normative distinction in §40.2 or §40.6.1: ordinary EXPLAIN binds/plans/validates but does not execute the SELECT; EXPLAIN ANALYZE executes under ordinary SELECT error, cancellation, snapshot, and cleanup ownership. |
| **N40-3** | **MAJOR — incomplete and unexecuted work has no profile/q-error applicability rule.** [§40.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29729) says every executed operator records metrics; [§40.6.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29781) reports actuals; [§40.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29862) requires estimated rows, actual rows, and q-error for *every* physical node. [§26.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21538) permits lawful early stop, while §§26.7 and 39.3 permit failure/cancellation. The chapter does not distinguish a fully consumed node from a demand-censored, never-started, failed, or cancelled one. | A scan estimated at one million rows supplies one row to `LIMIT 1`; its recorded actual output can be one although the full-consumption estimate was accurate. Reporting q-error of one million as estimate quality is misleading. A never-started side plan can appear as actual zero and infinite q-error; a worker failure before merge can appear as a successful zero profile. Clarify in §§40.6–40.8 the profile completion/applicability state, actual-work-only counting and worker teardown merge, and when q-error is comparable versus marked unavailable/partial. If a failed/cancelled profile is exposed, it must not imply successful completion. No persistent profile history or new execution state is needed. |

Counts: **0 BLOCKING; 3 MAJOR; 0 MINOR; 0 EDITORIAL; 0 DESIGN-SCOPE questions; 0 FROZEN SEMANTIC questions.** These are Chapter-40 contract gaps, not missing implementation or merely missing V40 tests. No frozen-owner contradiction was found.

## Owner and contract assessment

| Diagnostic scope | Required families and canonical fact owner | Assessment |
|---|---|---|
| Database/storage | Page requests, physical transfers, writes, BufferPool hits/misses (§§4–8); WAL append/durability/checkpoint/recovery (§§12–13); lifecycle (§3); vacuum/RID reclamation (§14) | Useful core counters. §40 should define their diagnostic event/gauge units; counters must not become storage authority. |
| Transaction/concurrency | Begin, semantic commit/abort, MUST_ABORT/ABORTING, retries, deadlocks, snapshots and locks (§§9–11, 15, 39) | Semantic outcome is separate from response delivery. A lost acknowledgement cannot decrement committed or increment aborted. |
| Query/operator | Rows, chunks, timing, charged/attributed memory and spill (§§22–30; QueryMemoryManager §24.4) | Actuals belong to executed operators, not estimates. Memory should mean Chapter-24 logically accounted query capacity, not exact RSS or BufferPool memory. |
| Pipeline/worker | Dependencies, wait/work time, task/morsel counts, worker-local aggregation (§§26, 32) | Local/batched counting is suitable; failure and early-stop finalization need N40-3’s clarification. Pipeline IDs need only query-local diagnostic identity. |
| Optimizer invocation | Statistics/provenance (§34), estimates versus proof (§35), cost/properties (§§36–37), chosen plan/trace/fingerprint (§38) | Diagnostic values may explain a choice but cannot become semantic proof or persistent-statistics feedback. |

The SeqScan, IndexScan, HashJoin, Aggregate, and Sort counters in §40.6 are mostly well chosen: they expose visibility filtering, index pressure, join build/probe and skew, grouping, and external-sort work. HashJoin’s directory load factor fits [§28.6’s actual directory design](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22012); it does not impose an unrelated algorithm. “Collisions,” comparisons, and physical spill bytes may remain implementation-specific diagnostic details. No exact wall/CPU-time equality across parent/child operators is required. CPU time is expressly conditional on availability; wait, wall, and CPU times can overlap and must not be summed as a conservation equation.

[§24.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20133) already gives the appropriate memory owner: charged query-owned capacity, with operator ownership and peak query bytes; it excludes exact OS RSS and separately bounded BufferPool frames. §40 need not build a second allocator ledger. Spill bytes and partitions/runs are likewise diagnostic observations of §24 SpillManager and operator work, not WAL or persistent corruption evidence.

Worker-local counters and batch/finalization aggregation in §§40.6 and 32.3 are performance-conscious. A global per-tuple atomic, per-tuple timestamp syscall, or allocation per metric event is a poor parallel hot-path design; the text does not demand one. An ordinary lifecycle timestamp and query-local profile object are sufficient. Overflow of a diagnostic counter should never become a Chapter-39 SQL arithmetic error; N40-1 should choose a controlled diagnostic saturation/unavailable indication rather than require a persisted metric type.

Debug hooks in [§40.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29685) are `SHOULD`, not a stable SQL API. Their transaction, WAL, DPT, snapshot, lock, lifecycle, horizon, and retirement facts have upstream owners. A concurrent read need not be one globally atomic database snapshot unless explicitly advertised as such; it must not be used as a transaction synchronization mechanism.

Logical EXPLAIN correctly describes the validated bound logical representation, not AST pretty-printing ([§20.19](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18081)). Physical EXPLAIN requires the selected operator, estimates, costs, relevant memory/spill predictions, and ordering; §38’s final plan and cost model own those facts. Selected-plan fields must come from one retained, validated invocation. Rejected alternatives belong in optional verbose/debug trace, not mandatory ordinary output. Source spans and errors remain Chapters 18/39-owned. Presentation whitespace, drawing, and text/structured format remain free; semantic fields are testable without whole-string snapshots.

Statistics diagnostics show StatsVersion, analyzed schema, staleness, live/physical pressure, NDV/MCV/histogram presence, scan/index inputs, costs, and fallback assumptions. [§35.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25544) makes estimated zero and approved `is_provably_empty` separate; §40 preserves that distinction and proof provenance. Actual EXPLAIN ANALYZE rows do not automatically rewrite statistics. “Earliest material divergence” is `should`-level diagnostic guidance, not a mandated automatic root-cause engine or fixed threshold.

For fully comparable executed nodes, §40.8’s q-error is coherent: positive `E,A` use `max(E/A,A/E)`; `0,0 → 1`; exactly one zero → an explicit infinite marker. That marker is diagnostic, not SQL FLOAT64 arithmetic. N40-3 concerns **whether the node was fully demanded and completed**, not the formula itself. [§38.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27988) correctly makes the complete structural key authoritative and the hash only a compact regression/debug aid; collisions and serialization-version changes cannot become correctness identities.

## Adversarial review

Status below means Chapter-40 Architecture is complete for that fixture, or has the cited gap. “Exact” describes an event/count when its boundary is defined; timing and statistics remain approximate/diagnostic.

| Metrics case | Owner, expected observation and scope | Status |
|---|---|---|
| A–C: hit, physical miss, failed page request | §7 BufferPool/raw I/O; request versus transfer counts, database scope | **GAP N40-1:** logical read, miss, and failed-transfer counting are not tied to distinct events. |
| D–E: append without durability; shared group flush | §§12–13; append and durable-prefix advancement, database scope | **GAP N40-1:** “bytes synced”/flush counting lacks a unit and advancement boundary; group waiters must not be counted as separate physical flushes by implication. |
| F–G: durable COMMIT/lost ack; automatic ABORT | §§9, 15, 39; semantic transaction outcomes versus transport/cleanup | **GAP N40-1:** counter-family wording does not expressly distinguish terminal transitions from acknowledged responses or sampled states. |
| H: deadlock victim | §11; victim event, transaction/database diagnostic | COMPLETE; ordinary victim is not corruption. |
| I: scanned recovery record, redo skipped | §13; records scanned versus pages redone | COMPLETE as distinct dimensions. |
| J: examined but unreclaimed version | §14; examined versus reclaimed | COMPLETE as distinct dimensions. |
| K: worker-local merge after success | §§32, 40.6; query/operator totals | COMPLETE. |
| L: worker fails before merge | §§26, 32, 39; partial actual work | **GAP N40-3.** |
| M: inspect while active | §§9, 40.4; instantaneous diagnostic, not global authority | COMPLETE as best-effort debug introspection; N40-1 still affects named gauge scope. |
| N: diagnostic counter overflow | §40.1; diagnostic-only count | **GAP N40-1** for observable overflow/unavailable status. |
| O: optional CPU metric unavailable | §40.6; operator timing | COMPLETE: “where available” permits explicit absence. |

| EXPLAIN case | Required result | Status |
|---|---|---|
| A: logical plan before physical planning | Bound, validated logical tree; no AST-only substitute | COMPLETE. |
| B: ordinary EXPLAIN SELECT | Plan without running SELECT | **GAP N40-2.** |
| C–D: inner parser/binder error | Original Chapter-18/39 cause, no invented plan | COMPLETE for error ownership; nonexecution needs N40-2. |
| E–F: optimizer/final-validation failure | Canonical §39.4/§38 cause; invalid plan does not run | COMPLETE. |
| G–H: estimated zero without/with proof | Estimate and approved proof remain separate | COMPLETE. |
| I–J: index order versus Sort enforcement | Finalized required/provided properties shown | COMPLETE. |
| K: equal estimated cost | §38 full-key tie rule; cost alone not identity | COMPLETE. |
| L: rejected alternative trace | Optional debug trace, not ordinary-output requirement | COMPLETE. |
| M: fingerprint collision | Full structural key still authoritative | COMPLETE. |
| N: formatting change | Semantic fields remain verifiable | COMPLETE. |

| EXPLAIN ANALYZE case | Required result | Status |
|---|---|---|
| A: 100/100 | q-error 1 | COMPLETE for completed node. |
| B: 100/1 | q-error 100 | COMPLETE for completed node. |
| C: 0/0 | q-error 1 | COMPLETE only if actual zero is an observed completed boundary; **GAP N40-3** if unexecuted. |
| D–E: exactly one zero | Explicit infinite marker | COMPLETE for completed node; **GAP N40-3** if demand-censored. |
| F–H: cancellation, child failure, failed spill | Actual work only; no successful-final profile | **GAP N40-3** on exposure/completion status and merge. |
| I–J: skew repartition, multiple sort runs | Operator-specific actual counters | COMPLETE after successful execution. |
| K: multiple workers | Local counters combined at boundary | COMPLETE on success; failure merge is N40-3. |
| L–M: profile cleanup failure, useful partial counters | Preserve Chapter-39 query outcome; do not fabricate completed profile | **GAP N40-3.** |
| N: large divergence | Show estimate, actual, provenance; attribution is advisory | COMPLETE for comparable node. |
| O: actuals suggest newer statistics | No automatic persistent feedback | COMPLETE. |

| Profiling-overhead case | Assessment |
|---|---|
| A global atomic per scanned tuple; C timestamp syscall per tuple; E heap allocation per event | Discouraged, and per-tuple shared contention conflicts with the worker-local intent of §§32.3/40.6; no telemetry framework is justified. |
| B worker-local count merged once; D lifecycle-boundary clock; F query-local profile state | Conforming, instructive baseline. |
| G cache-line contention; H false sharing | Performance defects to measure and avoid, not new correctness policies. |
| I debug output not requested | Formatting/verbose trace may be omitted; the unconditional “every executed operator records” wording should not be expanded into expensive always-on tracing. |

## Verification readiness, complexity, and next step

Live reusable procedures include [EXPLAIN syntax fixtures](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7873), V20-22 logical EXPLAIN ownership, V33-040/041/056 selected-plan diagnostics, V34-073 statistics/provenance, V36-068 cost diagnostics, V38-023 fingerprint collision and V38-064 trace, plus the existing [EXPLAIN ANALYZE and Profiling Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25479) and [Optimizer Diagnostics Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26303). Their bodies provide reusable component oracles, not an independently synchronized V40 section. The profiling test text already asks for failed/cancelled marking, but Verification cannot supply the missing Architecture policy itself.

Future V40 integration should directly test counter event/gauge definitions under concurrent fetch, WAL/group commit, lost acknowledgement, reset/overflow and concurrent inspection; ordinary EXPLAIN nonexecution versus EXPLAIN ANALYZE execution; partial/never-started/early-stopped nodes and q-error applicability; failed worker merge and cleanup; memory/spill attribution; trace noninterference; estimate/proof separation; and fingerprint collisions. Use independent owner observations, not printed EXPLAIN values as their own oracle. Do not assert exact timings.

Core mechanisms are counters, logical/physical EXPLAIN, real operator actuals, memory/spill attribution, estimate/proof distinction, and q-error. Pipeline profiling, optimizer provenance/trace, and compact fingerprinting are justified advanced educational mechanisms. A full telemetry registry, persistent profile repository, remote exporter, automatic runtime-statistics learning, or root-cause engine would be overengineering; Chapter 40 does not require them. Its requirements belong in Architecture, while detailed fixtures belong in Verification. The phrase “eventually” in §40.2 and “before physical planning exists” in §40.5 are staging-oriented, but do not by themselves create a separate finding: the actual v1 syntax and final-plan owners resolve the intended facilities. Development sequencing is compatible and authorizes no implementation.

The global search found valid differences in owner, scope, stage, and approximation—not a frozen-chapter contradiction. The three gaps above are omissions in Chapter 40’s own mandatory observability contract. No source code or tests were run.

**Next Architecture action:** one minimal Chapter-40 Fix A, ordered within that single repair: (1) define counter units/scopes and diagnostic overflow/unavailability without fixing names or APIs; (2) state ordinary EXPLAIN’s nonexecution boundary; (3) define complete/partial/not-started profile applicability and q-error treatment. Then conduct a separate independent read-only Architecture closure audit. Do not synchronize V40 Verification before Architecture closes.

```text
PRECEDING CLOSED CHAPTERS:
    CHAPTERS 31–39 ARCHITECTURE/VERIFICATION CLOSED
    AT THE DOCUMENTATION LEVEL BY PRIOR INDEPENDENT AUDITS;
    UNMODIFIED IN THIS REVIEW

CHAPTER 40 ARCHITECTURE:
    NEEDS ARCHITECTURE FIX

CHAPTER 40 VERIFICATION:
    NOT SYNCHRONIZED

IMPLEMENTATION:
    STORAGE FOUNDATION PARTLY IMPLEMENTED;
    CHAPTER-40 FACILITIES NOT IMPLEMENTED /
    NOT AUTHORIZED BY THIS TASK

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE

END CHAPTER-40 INITIAL READ-ONLY
ARCHITECTURE REVIEW.
```