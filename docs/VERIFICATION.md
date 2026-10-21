# DBlusBlus Verification and Benchmark Guide

## Purpose and authority

This document defines detailed testing, crash-injection, fuzzing, regression, and benchmark procedures.

For fresh-machine toolchain setup and normal build/check invocation, see
[`DEVELOPMENT.md` — Development baseline](DEVELOPMENT.md#development-baseline).

[`ARCHITECTURE.md`](ARCHITECTURE.md) defines the correctness/performance obligations. This
guide describes practical ways to verify those obligations. A test recipe does not weaken
or replace an architectural invariant, and benchmark numbers are measurements rather than
persistent architecture constants.

Procedures are organized by subsystem capability and apply whenever that capability is
under verification. Implementation status belongs in `PROJECT_STATE.md`.

The owner-to-procedure tables are maintained verification indexes. Their entries identify
fixtures and independent oracles to exercise; they do not report implementation status,
test execution, or completed review coverage. Architecture owns every expected behavior.

---

## Testing Philosophy

Use complementary verification methods for each subsystem:

- **Unit tests:** check local invariants.
- **Property/randomized tests:** exercise B+ tree operations, page compaction, tuple
  serialization, transaction visibility, and recovery.
- **Crash tests:** simulate process failure at WAL/page-write boundaries.
- **Differential tests:** where practical, compare supported SQL results against a
  reference database under the same supported semantics.
- **Concurrency stress tests:** exercise lock ordering, B+ tree splits, buffer
  eviction, and transaction conflicts.

---

## Benchmarking Philosophy

Performance claims require measurements.

At minimum benchmark:

- sequential scan throughput,
- indexed point lookup,
- B+ tree insertion,
- hash join throughput,
- group commit throughput,
- buffer-pool hit/miss behavior,
- concurrent transaction throughput.

Track:

```text
rows/sec
queries/sec
transactions/sec
p50 latency
p95 latency
p99 latency
CPU time
page reads/writes
WAL bytes
cache hit rate
```

Microbenchmarks and end-to-end benchmarks should both exist.

### Shared Chapter-42 benchmark evidence policy

This policy applies to the subsystem benchmark procedures below. Architecture owns the
required performance dimensions and capability scope; each benchmark procedure owns its
fixture, measured interval, applicable metrics, and sampling method. A recorded single
diagnostic run is useful, but by itself cannot support a performance comparison, regression
claim, optimization justification, or proposal to change an Architecture default.

Count a measured sample as performance evidence only when its setup reached the declared
workload and target regime, the operation completed as that workload requires, and its
result satisfies the applicable independent correctness owner. Reuse the owner procedure;
do not substitute timing, throughput, a selected plan, or a benchmark summary for SQL,
storage, transaction, recovery, or resource correctness. An intentional cancellation,
startup-failure, or partial-progress benchmark may measure that declared behavior, but
cannot report its incomplete work as a successful full-operation sample. Preserve any
actual owner failure and its cause. Incorrect output, failed setup/execution, or an
unintended partial run is invalid performance evidence, not zero throughput, infinite
latency, an ordinary regression, or a new database error.

Each benchmark family defines its workload and measured work/elapsed interval, including
whether generation, setup, reopen, recovery, and teardown are inside that interval. Use a
suitable monotonic elapsed-time source for duration and state units and denominators for
throughput. Define warmup or other preconditioning and keep its observations separate
from steady-state measured samples unless startup is the declared subject. A deliberate
less-warm, miss, or recovery case instead defines and evidences its intended initial state;
it need not be warmed into a different regime. Concurrent cases define start coordination
and observe overlapping activity when concurrency is the subject.

Before interpreting results, establish the intended mechanism from its owner or a direct
controlled state: input size or thread count alone does not prove a BufferPool miss,
batching/contention, B+ split, spill, heuristic search, or multi-worker execution. Where a
benchmark consumes Chapter-40 or other owner metrics (for example BufferPool hits/misses,
page I/O, WAL or spill bytes, peak query memory, worker counts, or optimizer search
counters), retain their canonical unit, event boundary, scope, and availability. Compare
them with owner observations; a benchmark wrapper's printed value is not an independent
metric oracle. The harness may directly measure its own elapsed wall time. An owner-metric
conflict makes the claimed evidence invalid; unavailable is never fabricated as zero.

For a performance claim, use repeated measured samples under a predeclared family-specific
sampling, summary, and variability/noise method; retain per-sample results or raw aggregates
sufficient to audit the summary. Predeclare exclusions where feasible and report every
excluded, failed, or noisy sample with its reason. Do not silently discard slow samples,
select favorable runs, or revise exclusion rules after seeing results. If variability or
incomparability prevents the requested conclusion, report **INCONCLUSIVE / INSUFFICIENT
EVIDENCE**, not a speedup, regression, or PASS. No universal repetition count, warmup count,
percentile algorithm, confidence method, speed threshold, or regression percentage is set
here. A reported p50/p95/p99 needs an appropriate sample population under its declared
method; an unsupported percentile is unavailable, not invented.

Before/after and alternative comparisons identify the intended changed variables and keep
workload, data, correctness result, material configuration, environment, and measurement
method comparable otherwise. Coupled changes are permitted when declared, but conclusions
must be limited to what that comparison can attribute. A valid run on a different machine
may stand alone; it does not silently become a paired before/after result. Benchmark
evidence is specific to its measured workload, configuration, and environment, not a
machine-independent Architecture constant or a presumption that newer, larger, or more
complex is faster.

The run record identifies the benchmark/workload and target regime; generator and seed
where synthetic, data scale and relevant key/query shape; implementation revision and
binary/build mode; materially relevant configuration (for example BufferPool/query-memory
budget, workers, vector size, cost/search settings, or diagnostic mode); and relevant
machine/CPU, OS/platform, compiler/build, and storage/filesystem context. It also retains
the initial-state/activation evidence, warmup policy, measured window and units, applicable
owner metrics, measured and excluded/failed samples, summary/variability method,
correctness disposition, and final evidence disposition. This is a conceptual report, not
a database persisted format, catalog object, production telemetry protocol, or prescribed
file format. Run-specific values belong in devlogs, CI/benchmark artifacts, or task reports
under the document-maintenance guidance in `DEVELOPMENT.md`, not in Architecture or a
rolling Verification results table.

For each metric and capability distinguish **APPLICABLE AND MEASURED**, **NOT APPLICABLE**
(only under the owning optional/conditional scope), and **APPLICABLE BUT UNAVAILABLE**.
Missing required procedure, instrumentation, sample, correctness oracle, or regime proof is
**NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE**, never PASS, zero, or convenient N/A.
Genuinely unavailable optional latch-wait data or an absent optional SIMD/radix/prefetch
path does not require a fabricated measurement; a single-worker-only physical path has no
multi-worker scaling sample. An Architecture-required capability absent from the current
implementation remains an implementation-state limitation, not a false benchmark PASS.
Successful controlled fallback under a small resource budget may be valid evidence;
actual OutOfMemory, WAL, transaction, corruption, or optimizer-resource failure retains its
owner classification and is not a successful sample. Evidence may motivate an explicit
Architecture revision (including the 1024 vector default or a deferred-feature proposal),
but neither a benchmark nor a profile authorizes that revision or promotes JIT into the supported scope.

---

## Sections

Each section is its own file under `verification/`.

- [Foundational Architecture Verification — Chapters 1–5](verification/01-foundational-architecture-verification-chapters-1-5.md)
- [Numeric Exhaustion and Terminal-Boundary Verification](verification/02-numeric-exhaustion-and-terminal-boundary-verification.md)
- [Storage Verification](verification/03-storage-verification.md)
- [Storage Benchmarks](verification/04-storage-benchmarks.md)
- [B+ Tree Verification](verification/05-b-tree-verification.md)
- [Transaction, Durability, and Reclamation Verification](verification/06-transaction-durability-and-reclamation-verification.md)
- [Catalog, SQL, and Logical-Plan Verification](verification/07-catalog-sql-and-logical-plan-verification.md)
- [Chapter 22 — Physical Plan and Runtime Operator Verification](verification/08-chapter-22-physical-plan-and-runtime-operator-verification.md)
- [Chapter 23 — Vectorized Data and String Representation Verification](verification/09-chapter-23-vectorized-data-and-string-representation-verification.md)
- [Chapter 24 — Query Memory, Row Storage, and Spill Verification](verification/10-chapter-24-query-memory-row-storage-and-spill.md)
- [Execution Verification](verification/11-execution-verification.md)
- [Chapter 25 — Vectorized Expression Execution Verification](verification/12-chapter-25-vectorized-expression-execution-verification.md)
- [Chapter 26 — Pipeline Execution Model Verification](verification/13-chapter-26-pipeline-execution-model-verification.md)
- [Chapter 27 — Scans and Unary Physical Operators Verification](verification/14-chapter-27-scans-and-unary-physical-operators-verification.md)
- [Chapter 28 — Join Execution Verification](verification/15-chapter-28-join-execution-verification.md)
- [Chapter 29 — Aggregation and DISTINCT Verification](verification/16-chapter-29-aggregation-and-distinct-verification.md)
- [Chapter 30 — Sorting and Top-N Verification](verification/17-chapter-30-sorting-and-top-n-verification.md)
- [Execution Integration and Transactional Write Verification](verification/18-execution-integration-and-transactional-write-verification.md)
- [Chapter 31 — DML, DDL, VACUUM, and Result Interface Verification](verification/19-chapter-31-dml-ddl-vacuum-and-result-interface.md)
- [Chapter 32 — Parallel Execution and Scheduling Verification](verification/20-chapter-32-parallel-execution-and-scheduling-verification.md)
- [Chapter 33 — Optimizer Architecture Integration Verification](verification/21-chapter-33-optimizer-architecture-integration-verification.md)
- [Chapter 34 — Statistics Collection, Publication, and Planner Visibility Verification](verification/22-chapter-34-statistics-collection-publication-and-planner-visibility.md)
- [Chapter 35 — Cardinality Estimation and Semantic-Proof Verification](verification/23-chapter-35-cardinality-estimation-and-semantic-proof-verification.md)
- [Chapter 36 — Cost Model and Base-Access Verification](verification/24-chapter-36-cost-model-and-base-access-verification.md)
- [Chapter 37 — Physical Properties, Join Enumeration, and Subquery-Planning Verification](verification/25-chapter-37-physical-properties-join-enumeration-and-subquery.md)
- [Chapter 38 — Memo, Costed Physical Search, Memory-Aware Optimization, and Resource-Ownership Verification](verification/26-chapter-38-memo-costed-physical-search-memory-aware.md)
- [Chapter 39 — Error, Corruption, Transaction-Outcome, and Recovery Verification](verification/27-chapter-39-error-corruption-transaction-outcome-and-recovery.md)
- [Execution Control, Observability, and Benchmarks](verification/28-execution-control-observability-and-benchmarks.md)
- [Statistics and Optimizer Verification](verification/29-statistics-and-optimizer-verification.md)
- [Chapter 40 — Observability and EXPLAIN Verification](verification/30-chapter-40-observability-and-explain-verification.md)
