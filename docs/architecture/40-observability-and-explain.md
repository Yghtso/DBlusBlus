# 40. Observability and EXPLAIN

## 40.1 Internal metrics

The engine must expose internal counters sufficient to reason about correctness and performance.

The metric families include:

- logical page reads,
- physical page reads,
- page writes,
- buffer hits/misses,
- WAL bytes,
- WAL flushes,
- B+ tree splits,
- rows scanned,
- rows filtered,
- hash build/probe activity,
- optimizer-estimated rows,
- actual rows.

Exact metric ownership and naming MAY evolve by subsystem.

A reported metric identifies its owner, unit, and scope/lifetime. An **event total**
counts occurrences at the named owner's defined boundary; attempted work,
completed work, and successful publication are distinct events and MUST NOT be
silently combined under one metric. A **gauge** samples a current owner state;
concurrent gauges need not form one globally atomic database snapshot. A
**duration/latency** measures an identified stage interval, not a sum that must
equal independently measured intervals. An **estimate** retains its upstream
approximate meaning. Cumulative totals identify their process/database-instance,
query, operator, pipeline, optimizer-invocation, or explicitly reset diagnostic
interval as applicable; no counter persistence or public reset operation is
required.

For the normal §7.6.3 BufferPool fetch, one **logical page read** is one
successful caller fetch claim converted to a public pin. A successful resident
pin is one **buffer hit**; a successful claim served by a `LOADING -> RESIDENT`
publication is one **buffer miss**, including each waiter sharing that load.
Thus successful logical reads equal hits plus misses within the same scope.
Internal eviction-race retries do not create another logical read: classify the
eventual successful pin by its actual resident-hit or load-publication path.
A failed/cancelled claim that never becomes a public pin, or an invalid or
unpublished PageId rejected before normal fetch admission, increments none of
these completed-fetch totals; attempted/failed fetches may be separate metrics.
One **physical page read** is one completed full-page storage transfer, even if
subsequent validation rejects the page; a partial/failed transfer is not a
completed physical read. One **page write** here is one completed full-page
physical write transfer, not a logical mutation or a stable/durable writeback;
later file synchronization and dirty-state reconciliation retain §7.10 ownership.
Short-I/O syscall retries do not multiply one completed transfer. Coalesced
fetch claims never multiply the one physical read, and new-page construction
does not invent a physical read.

Diagnostic arithmetic MUST NOT silently wrap or use signed-overflow undefined
behavior. A representation limit may saturate with an explicit saturation
indication or report overflow/unavailability; it does not produce a SQL
arithmetic error, change a transaction outcome, or make the database
noncontinuable solely because a diagnostic lost precision. Optional,
platform-dependent, or upstream-unavailable diagnostics identify absence
rather than substituting semantic zero. This is not permission to omit a
mandatory core event metric. Metrics observe canonical subsystem state; they
never determine page residency, WAL durability, transaction visibility or
outcome, recovery classification, query results, or semantic-empty proof.

## 40.2 EXPLAIN

The SQL interface exposes `EXPLAIN` information including:

- logical plan,
- selected physical plan,
- estimated cardinalities,
- estimated costs.

`EXPLAIN ANALYZE` additionally exposes actual execution information such as row counts and timing so estimates can be compared with observed behavior.

For the §18.10.4 SELECT-only forms, ordinary `EXPLAIN` MUST NOT execute the
inner SELECT's physical plan or return its result rows. It performs the
canonical parsing, binding, logical planning and validation, and, where
physical plan information is requested, optimization and final physical-plan
validation. Logical EXPLAIN is likewise nonexecuting. Those stages retain
their existing error and resource owners; an error independently required
there is still reported. Ordinary EXPLAIN MUST NOT start physical operators
or pipelines, perform execution-only memory or spill work, or evaluate an
expression solely to obtain runtime observations. It reports plan estimates
and properties, not fabricated actual rows, execution time, runtime memory,
spill behavior, or operator counters. An error arising only from executing
the inner SELECT is not induced by ordinary EXPLAIN. `EXPLAIN ANALYZE` uses
the same canonical planning and final-validation rules, then executes the
selected physical plan under ordinary SELECT transaction, snapshot, lock,
operator, cancellation, resource/error, and cleanup ownership; Chapter 40
observes that execution without changing its semantics. EXPLAIN presentation
does not select a different plan or define a separate transaction model.

These facilities are architectural observability requirements, not merely debugging conveniences.

## 40.3 Transaction, WAL, recovery, and vacuum metrics

The engine exposes enough counters to diagnose both correctness and performance, including:

```text
transactions begun / committed / aborted
statement failures leaving transaction active
statement failures requiring/triggering abort
transactions observed in MUST_ABORT / ABORTING
commit outcome-uncertain responses / connection losses
post-durable-commit completion failures
storage-noncontinuable transitions
database lifecycle transitions / time in OPENING, RECOVERING, DRAINING, CLOSING
database-busy open failures
failed opens / recovery failures / shutdown failures
deadlock victims
serialization failures
statement retries
active snapshot count
oldest snapshot xmin
snapshot active-set sizes
tuple-lock waits
unique-lock waits
deadlock cycles
WAL bytes appended / synced
WAL flush count
group-commit batches and group size
commit wait latency
checkpoint count / duration
dirty-page-table size
redo-start distance
recovery records scanned
pages redone
full-page images restored
loser transactions
vacuum tuples examined
versions reclaimed
index entries removed
RID retire queue size
frozen versions
```

Exact counter names may evolve, but these observability dimensions are architectural.

Live-transaction outcome totals cover this process's transaction protocol;
recovery-discovered winners and losers are separately identified recovery
events rather than retroactive live completions. Begin totals count successful
§9 registration/admission, not rejected requests. Committed totals count each
canonical semantic COMMITTED outcome once:
persistent COMMIT at durable C3, read-only COMMIT at authoritative C4, independent
of C5/C6 completion or client acknowledgement. Aborted totals count each
authoritative A2 ABORTED publication once. Statement-failure totals count failed
admitted statements rather than their cleanup steps. Commit-outcome-uncertain
responses/connection losses, post-durable failures, and noncontinuable
transitions are separate diagnostic events; they do not recategorize a semantic
COMMITTED transaction as ABORTED.
`MUST_ABORT`/`ABORTING` observations, active snapshot count, oldest snapshot
`xmin`, snapshot active-set size, dirty-page-table size, and RID retire-queue
size are sampled gauges; separately identified state-entry totals may also be
exposed. An absent oldest snapshot is unavailable, not a fabricated `xmin`.

WAL bytes appended count each new byte in §12.12's valid logical append prefix
once, including its published padding, not reservations or incomplete records.
WAL bytes synced count each newly established durable byte of that valid prefix
once under §12.13; this is not a raw subtraction of record-start `durable_lsn`
values and is never multiplied by commit waiters or repeated sync attempts.
WAL flush count is the number of WAL-flusher durability attempts, successful
or failed, not the number of waiters or constituent syscalls/segments; success
or failure may be distinguished separately. A group-commit batch counts one
successful shared durability advancement serving commit waiters, and its group
size is the number of distinct waiters it satisfies. Append, physical writing,
durability advancement, waiter completion, and C6 delivery remain distinct.

Lifecycle transitions and checkpoint completions are event totals; time in a
lifecycle state measures entry-to-exit intervals, checkpoint duration measures
the checkpoint's own start-to-completion/failure interval, and persistent
commit wait latency measures C3 waiter enrollment to durability satisfaction
or failure. Their elapsed values are diagnostic, need not be reproducible or
additive across independently measured stages, and may be explicitly
unavailable when timing cannot be obtained. Upstream approximate vacuum or
statistics pressure remains approximate when displayed here.

## 40.4 Transaction/recovery debug introspection

Internal/debug interfaces or test hooks SHOULD permit inspection of:

```text
active transactions
active snapshots
transaction status by TxnId
runtime transaction state and current COMMIT/ABORT stage
current statement CommandId and published-write boundary flag
logical lock table
wait-for graph
durable_lsn
current WAL end
dirty-page table
latest checkpoint
database lifecycle state and exclusive-owner identity/lock-held state
vacuum horizon
RID retirement epochs
```

These interfaces need not be stable SQL-user APIs.

## 40.5 Logical EXPLAIN and front-end diagnostics

Before physical planning exists, EXPLAIN can display the bound logical tree independently of AST pretty-printing.

Useful debug detail includes:

```text
LogicalSlotId
BindingId
TableId
resolved type
nullability
lineage
catalog schema version
logical predicate/join/aggregate structure
```

Front-end/catalog observability may additionally expose parser/binder latency, catalog cache hits/misses, descriptor-version lookups, and DDL/object-retirement queue size.

These diagnostics must not become a correctness dependency.

## 40.6 Execution profiling

Every executed physical operator records at least:

```text
input rows
output rows
chunks
wall time
CPU time where available
peak operator/query memory attribution
spill bytes
```

Operator-specific counters include useful dimensions such as:

```text
SeqScan:
    heap pages visited
    visible tuples
    MVCC rejects
    predicate rejects

IndexScan:
    index entries visited
    heap RIDs fetched
    invisible candidate RIDs

HashJoin:
    build/probe rows
    output matches
    hash collisions
    directory load factor
    spill partitions/bytes
    skew/repartition count

Aggregate:
    input rows
    groups
    collisions
    spill bytes

Sort:
    rows
    comparisons
    runs
    spill bytes
    merge passes
```

Parallel operators accumulate hot counters in worker-local state and combine them at synchronization/finalization boundaries rather than requiring one atomic increment per row.

Profile applicability is diagnostic and per physical node and measured output
boundary, not a new operator lifecycle or transaction state. A node that never
starts retains its plan estimate but has no observed actual rows; nonexecution
MUST NOT be represented as a completed zero. A started node records only work
actually observed. Its row count is **cardinality-comparable** only when the
same output population described by its optimizer estimate has been observed
through that node's cardinality-complete boundary. Opening, clean teardown,
or local `FINISHED` alone does not establish that boundary. Failure,
cancellation, or lawful upstream early stop may leave a started node partial
or demand-censored even when the query succeeds. A parent may be comparable
while its child is not. Fully consumed input to a blocking operator remains
complete input-work evidence; its output is comparable only if the matching
estimated output boundary was also completely observed. Actual work
counters, including rows, chunks, time, attributed memory, and spill, are not
extrapolated into work that did not occur. Applicability and any unavailable
measurement remain distinguishable from a genuine completed zero.

### 40.6.1 EXPLAIN ANALYZE

`EXPLAIN ANALYZE` executes the selected physical plan and reports both estimates and actual execution information, including:

```text
estimated rows
actual rows
operator time
memory
spill behavior
important operator-specific counters
```

Actual counters come from the physical operators that really executed, not from logical estimates copied into an output template.

For each selected node, the diagnostic distinguishes not started, started with
incomplete cardinality observation, and cardinality-comparable completion;
exact display names are not prescribed. Successful query completion does not
make every descendant comparable. If a failed or cancelled execution's
partial profile is exposed through a diagnostic, debug, or test interface,
it identifies that execution did not complete successfully and includes only
safely observed work. The original execution error or cancellation remains
primary under Chapters 26 and 39; profiling neither fabricates finalization
nor changes query or transaction outcome. No particular SQL error-plus-profile
wire format or crash-surviving profile is required.

### 40.6.2 Pipeline profiling

Pipeline-level profiling records at least:

```text
pipeline ID
dependency wait time
execution time
chunks processed
worker count
morsels/tasks
```

This distinguishes CPU work from blocker/dependency waiting and scheduler imbalance.

Successful profiles combine applicable worker-local operator and pipeline
measurements without double counting. On failure or cancellation, after the
required worker quiescence and before local state is discarded, safely
available measurements MUST be retained if a partial profile is exposed;
missing or unrecoverable measurements are marked incomplete/unavailable,
not fabricated. Pipeline counters and elapsed intervals describe only
observed work and do not imply successful pipeline finalization. Diagnostic
collection or merge failure may make affected fields unavailable, but cannot
keep tasks runnable, impede required cleanup indefinitely, replace the
original error, or change the canonical query/transaction outcome.

## 40.7 Statistics, estimation, and base-access diagnostics

Optimizer diagnostics expose enough information to explain base-plan choices, including:

```text
StatsVersion / analyzed schema version
stats staleness ratio
table live rows / physical pages / dead-version estimate
column null fraction / NDV / MCV / histogram availability
index physical/live/invisible entry pressure
index leaf occupancy / heap correlation when available
predicate truth/selectivity estimate
provably-empty flag and approved semantic proof kind, distinct from estimate provenance
estimated rows and row width
SeqScan and candidate IndexScan cost components
chosen search bounds / residual predicates
provided index ordering property
```

EXPLAIN should show the chosen estimated rows/cost and may show rejected base alternatives in verbose/debug optimizer trace mode.

Diagnostics identify when important fallback assumptions are used, including:

```text
stale/missing statistics
independence assumption
multi-column NDV damping
missing index/heap correlation
```

These diagnostics explain optimizer behavior; they do not change semantics.

When estimated rows or estimated TRUE fraction is zero, EXPLAIN/trace must remain able to show whether `is_provably_empty` is independently true or false. No mandatory user-facing text format is fixed, but the distinction and proof provenance must be observable enough for validation and regression tests.

## 40.8 Optimizer trace, plan fingerprint, and estimate error

Normal physical EXPLAIN shows at least:

```text
physical operator
estimated rows
estimated width
startup cost
total cost
estimated peak/operator memory where relevant
estimated spill where relevant
provided/required ordering where useful
```

Verbose/debug optimizer trace additionally exposes the search/estimate information from §38.23.

`EXPLAIN ANALYZE` retains the estimate for every selected physical node and
distinguishes observed actual rows and q-error according to §40.6's per-node
applicability. Numeric q-error applies only when estimate `E` and actual `A`
refer to the same physical-node output boundary and `A` is a completed,
cardinality-comparable observation. The diagnostic fields are:

```text
estimated rows
observed actual rows, when available, with completeness/applicability
q-error, when comparable; otherwise explicit unavailability
```

For positive estimate `E` and actual `A`:

```text
q_error = max(E / A, A / E)
```

Zero handling is exact:

```text
E = 0 and A = 0:
    q_error = 1

exactly one of E/A is 0:
    q_error = infinity / explicit infinite marker
```

For a not-started node, or a partial, demand-censored, failed, or cancelled
node without a complete comparable cardinality, q-error is explicitly
unavailable/not comparable. Neither an absent actual nor an observed prefix
is substituted as zero, infinity, or an extrapolated full cardinality; even
an observed prefix numerically equal to the estimate does not establish
completion. A genuine completed zero retains the zero rules above. Planning
estimate provenance, semantic-empty proof provenance, and actual observation
remain separate; q-error availability is not semantic-empty proof.

Estimate-attribution output should make the earliest material divergence visible together with confidence/provenance, because an upper join-order failure may originate in one lower predicate estimate.

Estimate provenance and semantic-empty proof provenance are displayed as separate concepts. An estimate of zero with no approved proof remains an executable plan subtree.

Runtime actual rows are diagnostic only.

The architecture does not automatically write persistent statistics from one query's actual cardinalities.

The canonical plan fingerprint is the compact debug hash defined by §38.5 and is suitable for regression comparison, not as a collision-free semantic identity.
