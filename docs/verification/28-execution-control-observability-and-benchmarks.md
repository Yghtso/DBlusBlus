# Execution Control, Observability, and Benchmarks

## Control-Operator Tests

Construct direct valid physical plans for architecture-supported resolved control roles:

```text
PhysicalCreateTable / PhysicalCreateIndex / PhysicalDrop
PhysicalVacuum
PhysicalAnalyze
PhysicalExplain
```

Assert that DDL dispatches to Chapter 21's catalog/storage protocol under its existing
transaction gates; VACUUM dispatches to Chapter 14 reclamation; and ANALYZE dispatches to
Chapter 34's resolved collection/publication protocol. These operators consume stable IDs
and immutable descriptors, perform no execution-time SQL name resolution, and do not enter
ordinary join-order/access-path enumeration to rediscover their dedicated work.

Run context validation, cancellation, structured failure propagation, single-coordinator
ownership, cleanup, and profiling for each role. Detailed catalog, vacuum, statistics, and
transaction semantics remain owned by their existing verification sections; this subsection
tests only physical dispatch and execution-path correctness.

---

## EXPLAIN ANALYZE and Profiling Tests

Build a small physical tree with controlled source rows, predicate rejections, join
multiplicity, aggregate groups, and spill behavior. Run ordinary EXPLAIN and EXPLAIN
ANALYZE, then compare the output with direct operator/pipeline instrumentation under
`ARCHITECTURE.md` §§40.2 and 40.6–40.8.

Assert:

- physical operator identity, child relationships, relevant required/provided properties,
  and estimates are inspectable without reparsing SQL names;
- actual input/output/filtered rows and invocation/chunk counts come from operators that
  executed rather than copied optimizer estimates;
- every selected physical node exposes its estimated rows and per-node profile
  applicability, checked against direct operator start/completion evidence; report
  actual rows only when observed, distinguish incomplete prefixes from comparable
  cardinality, and require numeric q-error with §40.8's exact zero/infinity handling
  only at the same completed output boundary; not-started or incomplete nodes expose
  nonapplicability instead of fabricated actual zero or numeric q-error;
- parent input counts agree with the rows/chunks emitted by the observed child boundary,
  subject to explicit pipeline buffering/early-stop semantics;
- operator spill bytes/partitions/runs and peak attributed memory agree with SpillManager
  and QueryMemoryManager observations;
- pipeline IDs, dependency wait/execution durations, worker counts, morsels/tasks, and
  chunks correspond to the task graph;
- parallel hot counters combine from worker-local state at synchronization/finalization
  boundaries without changing execution semantics.

Do not assert exact wall-clock or CPU durations. Check only nonnegative/finite values,
presence where supported, and internally consistent relationships.

Cancel or inject failure before execution, during streaming work, during blocking
finalization, and after partial output. Assert the original execution error or cancellation
remains primary and cleanup leaves no live profiling registration or runnable execution
work. If a partial profile is exposed through a diagnostic, debug, or test interface, assert
it identifies unsuccessful/incomplete execution as applicable, retains only safely observed
work, and neither fabricates a successful final row count nor treats an unobserved or
incomplete cardinality as comparable for numeric q-error. Exposed diagnostics must not
imply successful pipeline Finalize or transaction completion.

Semantic-empty proof kind and numerical estimate provenance must remain distinguishable at
the physical EXPLAIN/validator boundary. Detailed statistics provenance, optimizer search,
cost, memo, and q-error verification remains in the §41.6–§41.7 coverage areas.

---

## Execution Microbenchmarks

Measure:

```text
scan rows/sec
scan bytes/sec
filter rows/sec
projection arithmetic rows/sec
VARCHAR comparison rows/sec

hash build rows/sec
hash probe rows/sec
hash join output rows/sec

aggregate input rows/sec
groups/sec

in-memory sort rows/sec
external sort MB/sec

chunk allocations/query
general allocator calls/query
temporary bytes/query
```

Benchmark with NULL-free and NULL-heavy data.

Apply the shared Chapter-42 benchmark evidence policy. Use fixed-seed or explicit
tables/chunks with recorded schema, row count and width, selected columns, key/value and
NULL distributions, predicate selectivity, order, join relationship, vector capacity,
BufferPool and query-memory settings where material, and independent expected results.
Declare input rows/bytes, output rows, setup and timed windows, and the selected physical
operator. Count actual nonzero operator work with §40.6 owner events; a plan node that
never runs, or an empty-input control alone, does not activate its named throughput path.
Keep fixture preparation outside a kernel interval unless preparation is its subject.

- Scan throughput uses a deterministic heap population and an observed `PhysicalSeqScan`.
  Rows/sec divides owner-reported visited/processed scan rows by the measured interval,
  not downstream filtered output. Bytes/sec uses encoded tuple-body bytes of those
  visited rows, derived from validated slot lengths over the same work interval; report
  selected columns and BufferPool state separately. Do not silently change the byte
  definition across comparisons or treat this as the required-column-decode oracle.
- Filter rows/sec uses a declared predicate, input count, selectivity, NULL behavior,
  expected result count, and observed nonzero `PhysicalFilter` work; a folded-away
  predicate is not filter evidence. Projection arithmetic rows/sec uses resolved physical
  types, a deterministic expression and result checksum, and observed `PhysicalProject`
  kernel work without binder/type-resolution time. VARCHAR comparison rows/sec declares
  lengths, shared-prefix distribution, comparison operator, canonical binary/type
  semantics, NULL pattern, and expected Boolean/NULL results; it observes comparison
  kernel activity rather than inferring it from SQL syntax.
- Hash build rows/sec starts with a declared build relation, key types/widths,
  distinctness/duplicates, NULLs, and memory budget; §40.6 build activity and finalized
  hash state prove actual build work. Probe rows/sec uses a valid prepared build state
  outside the probe timing window and a fixed probe hit/miss/multiplicity sequence;
  owner probe-row events and independent matches give its denominator. Hash-join output
  rows/sec uses deterministic build/probe relations and expected joined-row count,
  requiring observed `PhysicalHashJoin` build, probe, and output activity. NestedLoop
  output does not activate this named hash-join metric.
- Aggregate fixtures declare input rows, group keys/count, skew, functions, NULLs, and
  independently expected groups/values. Owner aggregate input-row and completed-group
  events supply separate input rows/sec and groups/sec, not estimated statistics.
- In-memory sort uses deliberately disordered input within a declared valid memory
  regime, observed `PhysicalSort` row work, correct ordered output, and canonical zero
  spill bytes. External sort uses a deterministic input and valid lower memory budget
  that produce owner-observed runs/merge work and positive spill bytes; report decimal
  MB/sec from declared encoded input sort-row bytes over its measured interval. Size
  alone does not prove spill, and a non-spilling run cannot stand in for external sort.
- Report chunk allocations/query, general allocator calls/query, and temporary
  bytes/query from scoped execution owner/test instrumentation, separating unrelated
  process activity and identifying the query interval. These resource totals do not by
  themselves prove no per-row allocation or any other §42.4 structural hot-path rule;
  direct structural evidence belongs to its owning procedures.

For every applicable family, predeclare a NULL-free input and a reproducible NULL-heavy
pattern with a materially nonzero effective NULL count in the measured operator input.
Observe validity/active-row participation rather than relying solely on a schema label
or one isolated NULL. Keep other material factors comparable when studying NULL effects.

---

## Vector Size Benchmark

Benchmark at least:

```text
256
512
1024
2048
4096
```

rows/chunk on representative workloads.

The Architecture default is 1024. This Verification-owned candidate grid includes
sizes below, at, and above that default; it is not an Architecture-required grid.
Measurements may motivate, but do not themselves make, an explicit Architecture revision.

Do not assume a larger vector is always faster; cache footprint and branch behavior matter.

Apply the shared Chapter-42 comparison policy to a predeclared set of representative
scan/filter, arithmetic projection, hash-probe, and aggregate workloads (or equivalent
distinct kernel behaviors). Replay the same data, query/operator, expected result,
material memory budget, and environment across capacities, changing the vector capacity
as the intended variable. Record configured and effective `DataChunk` capacity (§23.1),
actual chunk cardinalities and owner-observed operator activity; a configuration value
that never reaches the executed path does not activate that candidate. Retain result
correctness and comparable work denominators at each size. No candidate is presumed
faster, and a faster candidate does not automatically change 1024.

---

## End-to-End Execution Benchmarks

Create repeatable workloads for:

```text
point lookup
selective index range
full table scan
filter + projection
small join
large hash join
group aggregate
ORDER BY
ORDER BY LIMIT
UPDATE by index
bulk INSERT
concurrent read/write
```

Track:

```text
latency
throughput
CPU
memory peak
buffer hits/misses
WAL bytes
spill bytes
```

Apply the shared Chapter-42 benchmark evidence policy. Each named workload has stable
SQL/operation text or a fixed-seed generator, table/schema and row/key/width/NULL
distributions, named scale with recorded counts, BufferPool and query-memory settings
where material, expected result/outcome, and observed physical plan and operator work.
The workload name is not a path oracle: retain validated plan identity plus nonzero
owner execution events for any mechanism it claims. Reuse the scan/expression, join,
aggregate, sort/TopN, spill, DML, and transaction correctness suites; a selected plan
without correct completed output or actual work is insufficient performance evidence.

- Indexed point lookup uses a known key/row and observed point `IndexScan`; selective
  index range uses declared bounds, qualifying count/selectivity, and an executed index
  range path. Full table scan uses known rows/pages and an observed full `SeqScan`, not
  an index-only substitute. Filter + projection declares input rows, selectivity,
  projected expressions/columns, and expected output, with both physical stages active
  unless the explicitly named comparison studies optimizer elision.
- Small join records its two relation sizes, key shape, relationship/selectivity,
  expected output, and the legal physical algorithm actually chosen; its name alone
  mandates no hash algorithm. Large hash join records larger deterministic build/probe
  relations, duplicate behavior, expected output, memory budget, and observed
  `PhysicalHashJoin` build/probe/output work; record actual in-memory or spill state
  without inferring spill from size. Group aggregate records input rows, group count,
  skew, functions, NULLs, expected groups/results, and observed aggregate work.
- ORDER BY uses input not already satisfying its declared order and verifies an
  applicable order-enforcement path and semantic result. ORDER BY LIMIT records order,
  LIMIT/OFFSET and expected permitted result sequence, plus the selected legal strategy;
  it does not require TopN solely from the query name. Index-driven UPDATE declares
  indexed target predicate/key, expected target RIDs and updated values, and proves
  index access supplied the targets before counting successful transaction results.
  Bulk INSERT replays a known row stream with row shape/count, index presence,
  transaction batching policy, and expected committed rows under ordinary WAL/durability
  semantics; failed or aborted rows are not successful insert throughput.
- Concurrent read/write uses a deterministic reader/writer transaction mix with recorded
  participant counts, isolation level, operation and key-overlap pattern, and expected
  success/conflict/retry/abort classes. Controlled barriers or owner activity establish
  actual read/write overlap within the measured window; merely starting two threads is
  insufficient. Report successful reads and acknowledged writes/commits separately from
  attempts, retries, and failures. This is transactional concurrency, not multi-worker
  scaling of one physical operator.

Declare each workload's start/end latency boundary and successful query, operation,
row, or transaction throughput denominator and unit. For writing transactions use the
canonical successful acknowledgement boundary, not earlier durable-WAL publication;
keep end-to-end latency distinct from operator time. CPU time identifies its measured
process/thread/worker scope and is not wall time. Peak accounted query bytes come from
QueryMemoryManager (§24.4), not RSS. Same-interval BufferPool owner hits/misses, valid
WAL-prefix appended bytes for applicable writes, and SpillManager/query-owner bytes for
actual temporary spill supply the remaining resource metrics; no spill is zero only
when owner observations prove none, and unavailable is not fabricated as zero. Do not
infer any resource counter from low latency, file size, or benchmark-wrapper labels.
These workloads do not establish parallel scaling or §42.4 hot-loop structural constraints.

---

## Parallel Execution Scaling Benchmark

Apply the shared Chapter-42 benchmark evidence policy only to physical paths whose
canonical Chapter-32 execution capability supports multiple workers. Inventory the
validated physical-path/task capabilities at the tested revision: for each supported
multi-worker family, provide a scaling fixture (or justify an equivalent fixture for the
same parallel mechanism); classify a genuinely single-worker-only path **NOT APPLICABLE —
NO MULTI-WORKER CAPABILITY**. A parallel-ready state split alone is not support. A path
claimed to support multiple workers but not exercisable/observable is **NOT VERIFIED /
TEST INFRASTRUCTURE INCOMPLETE**, or a recorded implementation-status mismatch, not N/A or
PASS. Inspect SeqScan, hash build/probe, hash aggregate, sort, and any other supported
family without assuming that all are implemented. Do not promote ordered B+ range
partitioning, the single-worker DML write phase, or DDL/VACUUM/ANALYZE control mutation
through this benchmark. Multi-query concurrency, including the read/write workload above,
is not one query's multi-worker scaling.

For each applicable family, predeclare a deterministic nonempty data/query fixture,
generator/seed and scale, expected result, and a validated immutable physical plan that
uses the target operator/task model. Record structural plan/path identity (operator and
algorithm, properties/order, and a canonical fingerprint where available), not just
textual EXPLAIN. Execute that *same* plan, data, transaction/snapshot, vector capacity,
query-memory budget, BufferPool configuration, and intended in-memory/spill and I/O
regime across a finite worker series. Use one actual query execution worker on the same
parallel-capable path as the baseline, then the minimum useful supported multi-worker
level and one or more higher supported levels where available, including a representative
high level for a wider range. Predeclare the concrete supported counts; a genuinely
limited range uses only its available points, whereas failure to exercise a predeclared
representative supported level is missing evidence. A one-worker special-case algorithm
or a worker-count-dependent plan change is not a pure scaling series. Keep setup outside
the timed window and use the same successful work denominator at every level; avoid
LIMIT/early-stop as the sole fixture.
Choose enough required tasks/morsels for distributed useful work without imposing a
universal row-count threshold. Record material background activity, affinity and NUMA
environment; neither affinity nor NUMA-specific testing is mandatory.

At each level record configured pool size, query worker limit, intended worker count,
actual participating execution-worker identities/count, required and claimed/completed
morsels/tasks, per-worker rows/bytes/chunks or equivalent useful work, and dependency/
finalization state. Use Chapter-40 operator/pipeline profiles and direct Chapter-32 task
observations, not the OS thread count, configured limit, plan label, or wrapper assertion
as proof. The one-worker run must open the target path and process nonzero expected work.
Each family's positive multi-worker control needs at least two workers with nonzero
meaningful target-path work and complete required task coverage. Created but idle workers,
or one worker doing essentially all useful work, do not establish an unqualified scaling
curve; record imbalance and strengthen the fixture when necessary. Reuse Parallel
Execution Tests and the relevant operator correctness owners at *every* level: compare
unordered SeqScan results as bags, required ordered results through the finalized order
owner, and parallel FLOAT64 aggregates under §29.3's admitted reduction-tree semantics.
Wrong results, incomplete required work, failures, or cancellation invalidate ordinary
scaling samples rather than supplying throughput points.

Report elapsed execution time and successful work rate (same declared rows/bytes/query
denominator), total query-scoped CPU time and per-worker CPU where supported, peak
QueryMemoryManager-accounted query bytes, participating workers, task/morsel and operator
work, and applicable BufferPool hits/misses/physical reads and spill-owner bytes for the
same measured interval. CPU is measured, not elapsed time multiplied by worker count.
Worker-local memory may grow with worker count; report the tradeoff rather than declaring
failure. A changed spill or resident/I/O regime is useful evidence but not an unqualified
worker-only comparison. Optional speedup is one-worker elapsed time divided by N-worker
elapsed time (or a stated comparable throughput ratio); optional efficiency is speedup/N.
Neither has a universal acceptance threshold.

For a scaling plateau, attempt causal attribution from direct evidence: ready-task queue
delay, idle workers with runnable work, dispatch/claim delay, and per-worker work skew for
scheduler/imbalance; dependency, barrier, latch/mutex, Combine/Finalize wait and work for
synchronization or serialized boundaries; and processed bytes, CPU utilization, working-
set controls, and available bandwidth counters for memory-bandwidth pressure. Distinguish
CPU-cache behavior from BufferPool residency and OS/file-cache warmth. Hardware cache or
bandwidth counters may strengthen a diagnosis but are not required; controlled working-
set comparisons may support a qualified **CACHE-SENSITIVE** observation. Rising BufferPool
misses/physical reads instead evidence an I/O effect, not CPU-cache misses. Record a
scheduler-, synchronization-, cache-, memory-bandwidth-, I/O-, or mixed *evidenced*
limit only with supporting observations. A plateau or high CPU alone does not identify a
cause; use **UNRESOLVED / INSUFFICIENT ATTRIBUTION EVIDENCE** when observations cannot
separate causes. Missing applicable owner metrics follow the shared evidence policy;
never invent zero or mandate PMU/perf/NUMA instrumentation.

Representative fixtures, when each path is capability-enabled, are:

- Parallel SeqScan over known pages/rows spanning multiple page-range morsels, with a
  known unordered result bag, stable BufferPool/I/O regime, and per-worker rows/bytes and
  morsel coverage; no cross-worker row-emission order is required.
- Parallel HashJoin with fixed build/probe relations and output bag, distributed build
  and probe work, finalized build state before probe, and recorded build barrier,
  memory, and spill regime.
- Parallel HashAggregate with controlled row/group distribution, worker-local input
  and state work, Combine/Finalize work, final groups, and accounted memory; use the
  aggregate owner's exact or admitted FLOAT64 result oracle as applicable.
- Parallel Sort of deterministic unordered input with the same required order and
  comparable in-memory or external regime, observing local-run distribution,
  merge/finalization work, ordering correctness, and spill bytes.

An additional supported path needs its own equivalent fixture. Keep diagnostic changes
to morsel size, data skew, memory budget, or working-set scale separate from the core
worker-count series; record the changed variable and use them only to support qualified
attribution. Poor scaling alone neither requires a scheduler redesign nor proves any
§42.4 structural hot-path constraint; those have separate evidence owners.

---

## Execution Hot-Path Structural Evidence

Apply the shared Chapter-42 evidence policy to §42.4 constraints 1–13. Correct output and
elapsed time are prerequisites/context, not structural oracles: fast execution does not
prove absent per-row allocation, redispatch, or generic `Value` construction; low memory
does not prove compact hash storage; good sort throughput does not prove large/sequential
spill requests; a fast scan does not prove required-column-only decode; and low latency
does not prove buffer reuse. Reuse G5's activated execution/spill fixtures and G6's
worker/profile procedure only where those paths apply. Test-only allocation/event scopes,
generation IDs, request traces, and owner profiles are permitted; no new SQL syntax,
persisted metadata, production ABI, specific allocator override, or always-on expensive
instrumentation is required. Source review may supplement but not replace occurrence
evidence.

| §42.4 constraint | Evidence / primary oracle | Applicability |
|---|---|---|
| 1. No inherent one-per-row/cell general-purpose heap allocation | Scoped steady-state allocation events versus fixture rows/cells; procedure below | Hot execution |
| 2. No per-row redispatch of batch-known choice | Batch selection versus row-loop dispatch events; procedure below | Resolved batch/kernel choice |
| 3. No generic `Value` per hot cell | Construction events versus typed hot cells; procedure below | Typed hot-cell execution |
| 4. No page pin merely for VARCHAR references | V23-G/H, V27-C/O page release/poison and owner-generation oracle | Page-backed VARCHAR scan |
| 5. Blocking owners retain varlen backing | V24-F, V26-M, V28-G, V29-G/N, V30-D producer reset/poison | Retaining operator |
| 6. Large operator memory accounted | V24-B/D/F committed-capacity `ML` and ownership ledger | Query-owned growing capacity |
| 7. Large/sequential spill I/O where practical | SpillManager request trace versus owner block/run design; procedure below | Spilling path |
| 8. DataChunk/vector backing reuse | Capacity/generation/reset trace plus V23-G/H | Repeated chunk path |
| 9. Decode only required scan columns | Independent required-slot oracle versus per-column decode trace; V27-C/N | Valid wide-tuple scan |
| 10. Compact/contiguous hash/directory where practical | Owner entry/region/metadata/allocation scale series; procedure below | Hash/directory owner |
| 11. Tiny-budget/forced-spill correctness | V24-H/J, V28-K, V29-N, V30-F/H semantic and pressure oracles | Resource-pressure path |
| 12. Aggressive optimization remains measurable | Chapter-40 / EXPLAIN ANALYZE and Profiling Tests, direct counters | Present optimized path; base profile always applies |
| 13. Profile-justified optional complexity; JIT deferred | Profile-to-change package and owner correctness; procedure below | Present permitted SIMD/radix/prefetch; absent option N/A |

For constraints 1–3, use deterministic typed INT64/FLOAT64 arithmetic, filter/project,
and a many-row vectorized output path, with nullable/VARCHAR variants where relevant.
Establish capacity and reusable state first, then process multiple predeclared scales of
many nonempty, multi-row chunks without deliberately changing capacity. Record actual
rows/cells/chunks, kernel invocations, and scoped events in the steady window. Attribute
general-purpose allocations to the hot query worker/owner, excluding harness and setup.
Keep setup, initial reserve, amortized arena/vector/StringHeap/RowCollection/block growth,
spill buffering, retained-datan ownership, and exceptional paths distinguishable; they
are not automatically violations. Reject an ordinary one-separate-general-purpose-
allocation-per-row or -cell occurrence pattern even if renamed output growth, delayed
allocation, or pool-node acquisition. One-row chunks, zero work, setup only, or G5's
aggregate allocation total cannot prove the property; the scale series aids diagnosis
without a universal slope threshold.

For constraint 2, independently establish the resolved operation, physical type, and
supported representation at setup/batch/kernel boundary; record those selections,
invocations, processed rows, and any virtual/type-switch event reselecting the *same*
choice inside the row loop. A test-only site marker may observe the decision without
changing it or adding a production per-row interface. Batch, normalization, and operator-
boundary dispatch is legal; row-dependent NULL, predicate, hash-match, selection,
comparison, and continuation branches are not redundant redispatch. Multi-row batches
make row-proportional reselects observable; CPU percentage is not the oracle. For
constraint 3, count generic `Value` constructions at their actual owner in the scoped
typed kernel and reject per-hot-cell construction. Separately scope legitimate client/
result conversion and ensure it neither contaminates nor disguises hot-kernel work.
Instrumentation itself must not construct Values or change their representation.

For constraints 4–5, reuse the named lifetime suites with page/input-chunk generation
poisoning. After scan VARCHAR materialization, release the heap guard and poison its page;
downstream bytes remain exact. A pin retained solely to keep downstream `StringRef` alive
fails, although another canonical pin reason may remain. For HashJoin build,
HashAggregate, Sort, and materialized spools, reset/poison the producer chunk or release
its page after lawful handoff; retained VARCHAR resolves through stable query-owned
backing or exact spill/reload ownership, not expired input, page, or expression scratch.
V23-H also checks cardinality, validity, selection, and StringHeap reset without mutating
a live borrower.

For constraint 6, V24-B/D's committed-capacity ledger covers growing hash, aggregate,
sort, RowCollection, result/DML spool, spill buffers, and worker-local state. Compare
owner charges with live committed regions, including positive aggregate-growth fixtures;
thousands of individually small allocations are not exempt. BufferPool frames remain
under their separate bounded owner, not the query ledger.

For constraint 7, activate real spill through G5's external-sort and Chapter-24/operator
fixtures. At the SpillManager/I/O helper boundary, retain ordered read/write requests
with resource/run/partition, offset, requested and completed lengths, and operation kind.
For a large sequential run, check ordinary middle requests against that owner's
configured block/transfer granularity (Chapter 24's configurable ~1 MiB target), and
check monotone contiguous/nonoverlapping run progress. A header, final partial block,
small total object, or fault boundary may explain short operations. Many row/cell-sized
requests throughout a large run without a concrete owner constraint argue against
“where practical”; one global byte or syscall threshold is not prescribed. Total spill
bytes, file name, or throughput alone do not reveal request shape. Retain the technical
reason for any exception.

For constraint 8, drive many same-shape chunks after initial capacity establishment.
Record logical chunk and backing owner/generation, capacity/growth events, emissions, and
resets. Reusable backing recurs across ordinary batches without requiring one permanent
address or one chunk object; fresh equivalent backing every chunk without an owner reason
is adverse evidence. Apply V23-G/H so reset clears logical state and StringHeap/selection
as needed and never invalidates a live borrower.

For constraint 9, build a valid wide tuple with independently specified output,
predicate-only, hidden/owner-required, and unrequired ColumnIds/LogicalSlotIds. Include
a narrow fixed-width projection, an unrequired wide VARCHAR, and alternate required sets.
Derive the expected physical required set from the validated plan/predicate/owner rules,
not final SELECT text or the scan's own claimed set. Trace scalar decode and StringHeap
materialization per column at the tuple/scan owner, separately from mandatory whole-
retained-tuple framing, offset, NULL-bitmap, and corruption validation (V27-C). Valid
required columns may decode; an unrequired user column has no ordinary scalar decode or
copy. A predicate-only column decodes when needed despite not being emitted; the
unrequired VARCHAR is not copied merely because it is stored. Malformed unselected data
remains subject to V27-C's independent corruption oracle—pruning never waives validation.

For constraint 10, inspect actual HashJoin, HashAggregate, DISTINCT, and other applicable
hash/directory owners across predeclared entry scales. Record entries, directory/bucket
capacity, retained payload and metadata bytes, block/region count, allocation and growth
events, and bytes/entry as a diagnostic, without making pointer addresses semantic.
Coarse blocks and compact directories can satisfy the guidance; one general-purpose node
allocation per entry, disproportionate pointer metadata, or avoidable fragmentation are
adverse evidence absent a concrete technical reason. A segmented alternative may conform
with that reason and measured structure while satisfying V24 accounting. No single hash
representation, bytes/entry maximum, or universal growth slope is mandated.

Constraint 11 uses V24-H/J and the named join/aggregate/sort spill owners for valid tiny
budgets, denied grants, one and multiple finite spills, reload, exact lower-memory paths,
errors, and result/order equivalence. Slow but correct forced spill may pass. For
constraint 12, compare applicable Chapter-40 operator/pipeline identity, rows/chunks/work,
CPU where available, memory, spill, and worker counters with direct owner observations on
optimized paths; SIMD, radix, or prefetch must not hide or fabricate enclosing work. If
no optional aggressive path exists, no extra optimized-path fixture is required, but the
base profile contract remains. Instrumentation may be test-enabled rather than always on.

For constraint 13, when explicit SIMD, an Architecture-permitted radix strategy, or
explicit prefetch is present, retain a review/evidence package linking a pre-change
Chapter-40 hotspot/profile to the exact path, G1-comparable baseline and optimized
measurements, semantic equivalence, and changed resource/profile behavior. No fixed
benefit percentage is required. SIMD uses the portable scalar/vector SQL, NULL, error,
FLOAT64, and lifetime oracles; radix is limited to owner-permitted equivalence (for
example §30.4's sound sort strategies), not a blanket mandate for radix join/sort;
prefetch preserves page/data lifetimes under §32.12. Absence of these optional
optimizations is N/A for this package. JIT remains outside the supported scope absent explicit
Architecture revision; favorable measurements alone authorize neither JIT nor any other
implementation or Architecture change.
