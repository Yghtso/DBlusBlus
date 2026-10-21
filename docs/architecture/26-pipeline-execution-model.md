# 26. Pipeline Execution Model

## 26.1 Pipeline graph

Execution is organized into pipelines:

```text
Source
  ↓
zero or more Streaming Operators
  ↓
Sink
```

Example:

```text
SeqScan
  -> Filter
  -> Project
  -> ResultSink
```

Pipelines form a dependency DAG when blocking state must be finalized before another pipeline can run.

Canonical dependencies include:

```text
hash probe waits for hash-build Finalize
sort output waits for input/run Finalize
aggregate output waits for aggregate Finalize
DML write waits for target-spool Finalize
an initialized IN-subquery probe waits for its lazy build Finalize
```

Scalar/EXISTS/IN side-plan dependencies are dormant until Chapter 17's
control-flow rules first demand their occurrence. The scheduler may then run the independent
child pipelines to completion/early-stop and resume the suspended outer
pipeline. It MUST NOT execute every dormant side plan eagerly merely because
the dependency DAG is known.

A dependent pipeline becomes runnable only after every required predecessor has reached its successful finalization state.

For aggregation, successful Finalize includes §29.3's complete numerical
validation of every group/aggregate state. No dependent result pipeline exposes
an aggregate row before that validation succeeds, even if a hash partition or
ordered group happened to finish earlier.

The immutable physical operator tree remains useful for optimizer output, EXPLAIN, and plan validation.

The pipeline graph is the execution-time work representation built after the physical plan is finalized.

## 26.2 Pipeline roles

### Source

Produces DataChunks.

Foundation examples:

```text
SeqScan
IndexScan
Values
```

Blocking operators may expose a source after the finalization required by their
operator contract.

### Streaming operator

Transforms one input batch without requiring all future input.

Foundation examples:

```text
Filter
Project
Limit
```

### Sink

Consumes pipeline input according to §26.4.3's acceptance and retention contract.
A sink is an input-consumption role, not by itself a requirement to accumulate
the entire input before producing results. The result sink hands data to the
cursor/result owner under §§27.11 and 31.9–31.10.

### Pipeline breaker / blocking boundary

A pipeline breaker establishes a boundary where blocking state must become
ready before dependent execution continues. Sink acceptance and this dependency
trait are distinct: an operator may consume input as a sink in one pipeline and
expose a source or ready state to another. A breaker is not a synonym for the
generic sink role, and a sink is not automatically a breaker.

Blocking, streaming, and spillability are execution traits/capabilities under
§22.7, not additional physical properties in §37.1. Breaker classification
governs pipeline decomposition and dependencies; the owning operator defines
its blocking/finalization behavior, including §§29.2–29.3 and 30.1.

Every breaker declares:

```text
memory-resident state
spillability
Finalize transition
post-finalize output/source state
dependent pipelines that must wait
```

Examples include:

```text
HashJoin build:
    build rows/directory -> finalized immutable probe state

Aggregate:
    group state -> finalized group source

Sort:
    input rows -> sorted in-memory/run state -> merge/output source

DML target materialization:
    target spool -> finalized write source
```

The source/streaming/sink role is explicit rather than hidden inside recursive iterator behavior.

## 26.3 Pipeline construction

The pipeline builder walks the finalized physical plan and identifies:

```text
sources
streaming chains
sinks
dependencies
```

It does not force every physical operator into a recursive row-at-a-time `Next()` chain.

The builder creates execution graph/state references without mutating the semantic meaning of the immutable physical plan.

### 26.3.1 Execution lifecycle and completion

Execution starts from the validated immutable physical plan under §§22.2–22.3,
22.8, and 38.24. Mutable source, operator, sink, and pipeline state is created
and initialized before use for that execution/statement attempt under
§§22.5–22.6. Shared immutable configuration and any more specifically owned
state retain their canonical owners; execution cursors do not mutate the plan.

The execution graph runs semantically required source/streaming/sink work under
§26.4, completes the operator-owned dependencies and semantic finalization that
work requires, and hands completion to the root/result owner. Successful
execution completion requires all such work and prerequisites to have succeeded
with no established terminal error or cancellation. A local source/operator
`FINISHED`, one completed pipeline, or a successful sink acceptance cannot
establish that condition for the whole execution.

A dependent stage MUST NOT consume partially initialized, unready, unfinalized,
failed, or canceled prerequisite state as successful input. It becomes runnable
only at the owning prerequisite's successful readiness point. Required
`Combine`/`Finalize` steps are semantic work, not resource destruction: aggregate
state and numerical validation follow §§29.2 and 29.3.7; sort build/output
follows §30.1; parallel build/output barriers and task readiness follow §§32.5,
32.7, and 32.8. These requirements apply where the operator owns those steps,
not as a requirement that every sink perform both operations. Failure of a
required finalizer or predecessor prevents successful completion and cannot
make its partial state a valid fallback input. Cleanup remains required even
when successful semantic finalization is impossible.

Valid early stop under §26.8 does not require exhaustion of unread upstream
input. It leaves only the work still demanded under §§20.12, 20.14.5, and 20.17
as a success prerequisite, together with that work's required finalization and
the execution's cleanup obligations. The driver cannot fetch unnecessary input
merely to complete a local protocol.

Root/internal execution completion and external result exposure are separate
boundaries. Sections 31.9–31.10 own result publication, returned-chunk lifetime,
and cursor exhaustion/EOS; local `FINISHED` is not cursor EOS. Internal work may
complete before or during cursor servicing according to the result owner's
strategy. Successful execution does not by itself mean transaction commit;
DML statement/publication and transaction consequences remain §§21.15, 31.9,
and 39.1-owned.

For DML, internal graph completion, operator readiness, spool finalization, or
allocation of an unpublished cursor is not §31.9's successful statement-result
publication boundary `R`. The Chapter-31 result owner alone performs the atomic
successful-envelope publication and any result-spool ownership transfer. Once
`R` succeeds, a later cursor delivery error does not revive this execution
instance or reopen the completed DML statement. This DML-specific handoff does
not change SELECT completion or Chapter-30 external-sort Source semantics.

Successful autocommit DML execution also completes every fallible operation
required to prepare its unpublished result envelope and make `R` possible
before the command layer admits implicit COMMIT at §15.5 C0. Pipeline readiness,
spool finalization, an accounting grant, and an unbacked cursor descriptor are
insufficient if required physical allocation, validation, spill-handle
preparation, resource registration, or cleanup-owner preparation remains. The
prepared state survives C5 under its independent request/result-preparation
owner. After successful C4–C5, the result owner performs `R` from that state
without another ordinary fallible publication prerequisite.

Success cannot be declared while any semantically required execution or
error-producing finalization task remains outstanding. Under §§32.8 and 39.3,
terminal failure/cancellation leaves no partially active task graph runnable as
successful execution. Active users of shared attempt state must quiesce before
that state is destroyed; no writer, borrower, or continuation may survive the
backing it requires. This is a lifetime/dependency obligation, not a scheduler
algorithm or synchronization primitive.

On success, terminal failure, cancellation, or valid early termination,
execution-owned resources are eventually released unless valid ownership has
transferred to a longer-lived owner. Sections 23.10–23.13 and 26.6 govern live
borrows; §§24.4–24.5 and 24.10 govern accounting and temporary-resource cleanup;
§31.10 governs retained results. Cleanup/destruction is distinct from semantic
`Finalize` and cannot fabricate its successful result. Transaction-owned locks
and statement/transaction outcomes remain with §39.1, not executor cleanup.

Once a canonical terminal error or cancellation is established, that runtime
instance cannot resume successful processing or become a successful execution.
Error propagation, quiescence, and cleanup continue under §39.3. A retry admitted
by the statement owner uses a fresh execution instance under §§31.5 and 39.1.4:
source positions, accepted-input/continuation state, local error candidates,
sink acceptance/finalization state, temporary output, and query scratch from
the failed attempt are not inherited. Reuse of immutable/shared data is only
under its existing owner; this rule neither admits a retry nor changes its
snapshot, CommandId, or transaction boundary.

Requesting data after local terminal completion, admitting input to a terminal
operator, consuming an unready/failed dependency, skipping required finalization
before claiming success, or reviving failed runtime state is internal
runtime/protocol misuse under §§26.4 and 39.1.3, not a new public SQL error.

### 26.3.2 Error ownership and result handoff

Candidate discovery is not selection of a terminal public error. Pipeline
execution preserves the candidate/provenance information required by the
canonical owner until that owner can establish its selected error, or prove
that no remaining semantically required candidate can beat it. This permits
owner-correct local reduction and merging without requiring full candidate
materialization or evaluating undemanded work.

Ordinary non-DML expression candidates follow §25.1.1. Source, lane,
chunk, worker, pipeline, or callback discovery/completion order MUST NOT replace
that semantic minimum. Once the minimum is established, it is the terminal
ordinary non-DML expression error for that execution path. DML instead follows
§21.16.1 through §25.1.2: execution preserves required eligible
candidates, their responsible `SourceSpan`, semantic expression origin, owning
cause/category, and expression phase until the DML owner can select the
statement error. It MUST NOT pre-rank DML through §25.1.1 or discard a required
candidate on first physical discovery. Parallel scheduling under Chapter 32
does not change either owner's reduction.

Specialized errors retain their owners: aggregate-finalization selection is
§29.3.7-owned; scalar-subquery cardinality and child/build precedence are
§§20.14.4 and 20.14.12-owned; persistent-page corruption, including scan fetch
failures, retains §39.1.3's lower-layer classification. `OutOfMemory`,
representability/resource `ExecutionError`, `SpillIOError`, and `QueryCancelled`
retain §§24.10 and 39.3. Resource/cancellation events may prevent further
candidate establishment under those owners; they do not enter the §25.1.1 or
§21.16.1 candidate ranking. Chapter 26 defines no global precedence between these
independently owned failure classes or between physical components.

Pipeline transport preserves every owner-required diagnostic field, including
semantic origin, `SourceSpan`, conceptual cause, category, and applicable DML
phase/eligibility. Pipeline IDs, chunk numbers, worker IDs, operator pointers,
and callback order cannot replace that provenance. Diagnostic wrappers may add
context without changing canonical public fields or erasing structured causes
under §39.3. Backing referenced by an error/report remains valid until reporting
no longer depends on it, using the ownership rules of §§23.10–23.13 and
24.4–24.5; a diagnostic cannot point into reset/reused chunk storage unless its
required data has independently valid ownership.

If a source/operator/expression invocation fails before successfully offering
its output transition, its partial or uninitialized output is not consumable
successful output under §§26.4 and 25.7.1. Merely placing rows in an output buffer
does not offer them. A prior independently completed output transition remains
a completed internal handoff; it does not by itself imply client visibility.
An already completed cursor return is an external delivery, not retroactively
retracted by failure/cancellation of subsequent execution. That returned prefix
is not by itself evidence of successful complete query execution unless a more
specific owner has already published that result, as §31.9 `R` does for DML.
External exposure and returned-value lifetime remain §§31.9–31.10-owned; this
distinction requires neither whole-query buffering nor a different cursor
protocol.

For a DML result whose §31.9 boundary `R` has succeeded, later result-spool
reading, delivery-chunk construction, cursor advancement, or transport belongs
to the result owner rather than this statement-execution error path. A terminal
delivery error remains distinct from successful client `FINISHED`; it does not
retroactively create a DML execution failure. Internal source/operator
`FINISHED` remains distinct from both public outcomes.

For autocommit DML whose publication-authorizing COMMIT record has appended,
pending cancellation cannot reopen execution or redirect COMMIT to ABORT. Once
C4–C5 complete, the already-prepared non-failing `R` transition completes before
that cancellation is applied to remaining delivery. Process crash or actual
session loss may prevent observation or execution of `R`; those events retain
§§15.5 and 39.1.7 ownership and do not create a fallible pipeline handoff.

An owner-established terminal error is an unsuccessful path, not ordinary
`FINISHED`. It permits error propagation, quiescence, and cleanup, not further
successful continuation, new-input admission, or generic replay in the failed
instance. Section 26.3.1's success barrier includes required error-producing
work across pipelines and finalizers. Failed state and invalid output are
cleaned without invalidating valid longer-lived ownership transfers; retry
admission and transaction consequences remain §§39.1.3–39.1.4, and causal-error
preservation through cleanup remains §39.1.7. Cleanup cannot turn a failed
accepted-input lifecycle into successful resolution.

## 26.4 Runtime interfaces

The generic handoff defines semantic transitions, not a C++ ABI. Sources and
streaming operators deliver consumable output before terminal completion.
Input acceptance, continuation, backing-release acknowledgment, and readiness
for another input are distinct facts that the driver can determine without
producer-specific conventions. Pull calls, push/callback execution, state-machine
returns, and other exact representations MAY encode these facts; no particular
return structure or enum such as `NEED_INPUT`, `READY_FOR_INPUT`, or `BLOCKED`
is required.

### 26.4.1 Output availability and terminal completion

Conceptually, a source supports:

```text
GetData(
    QueryExecutionContext,
    LocalSourceState,
    output DataChunk
)
    -> source status
```

The minimum source statuses distinguish:

```text
HAVE_MORE -> nonterminal output-available path
FINISHED  -> local source completion; no consumable output
```

A terminal completion outcome MUST carry no consumable output for that
invocation. This rule applies equally to sources and streaming operators.
Reusable output storage may physically exist during a terminal invocation, but
its contents are not a result of that invocation and MUST NOT be consumed as
one. Separating output from completion prevents a driver from having to guess
whether terminal status carries final rows.

Every consumable batch, including the final nonempty batch, is offered through
an output-available transition. For the final batch, the conceptual sequence is:

```text
output available
    -> downstream consumption or valid ownership handoff
    -> terminal completion with no consumable output
```

For a pull-style source, the final batch normally precedes a separate terminal
probe; the ordered transitions do not require two particular C++ method calls.
If exhaustion is already established, the terminal transition requires no
additional logical-row evaluation. A source with no output MAY report
`FINISHED` immediately, without a fake empty nonterminal batch.

`HAVE_MORE` does not promise another nonempty batch. Under §23.1, cardinality
describes the active data domain, not completion: zero, short, and full batches
never establish EOS, terminal or pending-terminal state, or final-batch status.
A short batch may be followed by more output. Completion is explicit control
information and MUST NOT be encoded as an empty input batch.

When a source returns `HAVE_MORE` with an empty executable chunk, it MUST still
advance finite source or operator state. An unbounded sequence of empty
`HAVE_MORE` batches without state progress MUST NOT substitute for `FINISHED`;
producing one is an invalid pipeline implementation/liveness violation.

Once an execution state reaches terminal completion, no further logical output
exists from that state. The driver MUST NOT request more data from it. A
post-terminal data request is outside the valid invocation protocol; it cannot
restart execution or emit another occurrence. No rewindability or idempotent
repeated-`FINISHED` API is required.

Local source/operator completion does not by itself establish whole-query or
statement success, transaction completion, completion of other pipelines or
required Combine/Finalize work, or result publication. Sections 31.9–31.10
remain authoritative for DML result publication and external cursor EOS.

### 26.4.2 Accepted-input, continuation, and progress lifecycle

A streaming operator supports conceptually:

```text
Execute(
    input DataChunk,
    output DataChunk,
    LocalOperatorState
)
```

A streaming operator accepts each submitted logical input-occurrence domain at
most once within its execution. Acceptance takes responsibility for processing
that submission; it does not mean that one invocation has completed all work,
offered all output, or released caller backing. Submission identity is not a
pointer, DataChunk object, or buffer identity. After the preceding lifecycle and
borrow requirements permit reuse, the same physical object may carry a distinct
input submission. No semantic row, chunk, or pipeline ID is introduced.

Within one `LocalOperatorState`, at most one accepted-input lifecycle is
unresolved at a time. Continuation resumes that acceptance or operator-owned
state derived from it; it MUST NOT reaccept the input as new work. Passing the
same input reference during a continuation call is not a second submission.
This per-local-state admission rule prevents replay/duplication without
serializing independent local states, workers, or pipelines under Chapter 32.

An accepted-input lifecycle is resolved only when either:

1. all semantically required processing and output obligations attributable to
   that acceptance have been discharged; or
2. the owning operator semantics establish valid terminal/early stop under
   §26.8, making the remaining work safely unnecessary.

Resolution is not inferred from an invocation returning, one batch being
offered, copying input, or any output cardinality. Required not-yet-offered
output keeps the lifecycle unresolved. An already-offered output may remain
subject to downstream consumption and borrowing after lifecycle resolution.

The operator MAY retain a value-stable dependency on caller input across
continuation steps under §§23.10–23.13, or copy, transfer, materialize, or
otherwise independently preserve required continuation state. It may
acknowledge release of the original caller backing only when every operator
dependency on that backing has completed or been independently preserved.
This acknowledgment can precede lifecycle resolution: copied input may no
longer require caller backing while required processing or buffered output
remains pending. Neither strategy is mandatory. Operator release, lifecycle
resolution, and physical reset permission remain separate because output or
downstream borrowers may still depend on that backing; §26.6 composes these
boundaries with Chapter 23.

Pending continuation means required processing, not-yet-offered required output,
or a pending terminal-control transition remains before new-input admission.
The driver may submit another input only when all applicable conditions hold:

```text
the preceding accepted-input lifecycle is resolved
no continuation must precede new input
the outstanding output handoff has completed
the operator has not reached terminal completion
valid early stop has not forbidden new input
```

The handoff MUST make output availability, pending continuation, operator
backing-release acknowledgment, and new-input readiness independently
determinable. It supports one accepted input producing multiple output batches,
including §28.8's probe continuation, without resubmitting input. It also
permits a successful lifecycle with zero output occurrences, as when Filter
rejects every row. Resolving that input is progress, not EOS or failure; no
fake output batch is required to acknowledge resolution.

Every output occurrence required by the owning operator semantics MUST be
offered exactly once. The generic handoff MUST NOT duplicate, drop, reaccept,
or replay required occurrences. This does not impose equal input/output
cardinality on relational operators or establish SQL row order. Chapter 20's
bag, ordering, demand, and executable scalar-order rules remain authoritative.

Every nonterminal processing step without new output MUST resolve accepted
input, release an operator dependency on accepted input, or advance finite
relevant continuation state toward required output, lifecycle resolution, or
terminal completion. For finite input and finite required output, continuation
MUST be well-founded: repeated equivalent state with no output, resolution,
release, or relevant advancement is an internal protocol/liveness violation.
This prevents continuation from encoding an endless no-progress execution;
no numeric progress counter, timeout, or sleep is required. A legitimate
dependency wait is scheduler-owned suspension, not permission to busy-retry
unchanged processing state.

Resource-pressure progress remains §24.6-owned and is distinct from operator
continuation progress. Resource activity cannot excuse endless continuation,
and operator-local mutation cannot excuse a denied resource retry without that
owner's required relevant progress.

Discovery of a §25.1.1 ordinary candidate or §21.16.1 DML candidate does not
itself authorize discarding remaining semantically required candidate-
establishing work: the owning rule determines when terminal error is
established. No first-physical-error precedence follows from this protocol.
On terminal failure or observed cancellation, pending continuation is failed
execution state, cleaned under §§24.10 and 39.3 rather than resumed as
successful processing or blindly replayed. The failed invocation provides no
successful consumable output, consistently with §25.7.1; independently completed
output transitions remain distinct. A Chapter-21-authorized retry uses fresh
attempt-local execution state, not the failed continuation. Resource categories
and transaction consequences remain with §§24.10 and 39.1.

Violations of this handoff, including terminal output, reacceptance, premature
new-input admission or backing reset, skipped required continuation, and
consumption of failed-invocation output, are internal invalid runtime/protocol
states under §39.1, not new public SQL errors.

### 26.4.3 Sink acceptance

A sink supports conceptually:

```text
Sink(input, local_sink_state, global_sink_state)
Combine(local_sink_state, global_sink_state)
Finalize(global_sink_state)
```

A completed successful generic `Sink` invocation accepts its complete submitted
logical-occurrence domain exactly once. The generic contract exposes no partial
successful acceptance. Retaining values requires stable ownership under §26.6
and Chapters 23–24. A failed or incomplete invocation MUST NOT be represented
as complete successful acceptance, and the driver MUST NOT blindly replay its
input. Failure does not imply that no physical work occurred; statement and
retry consequences remain Chapter-21/§39-owned.

Successful sink acceptance alone establishes neither query/statement success,
Combine/Finalize success, commit, nor client publication. Those remain separate
owner obligations, including §§31.9–31.10 for result publication.

Exact C++ virtual/template mechanics are implementation-specific.

The semantic state/lifetime separation is not.

## 26.5 Global and local state

Every operator separates immutable plan configuration, global per-execution
state, and local per-worker/task state, including in single-worker execution.

Local state owns hot mutable items where practical:

```text
cursors
reusable chunks
expression scratch
local buffers
continuation counters
```

The accepted-input and continuation state of §26.4.2 belongs to mutable
execution/local state, not the immutable plan, under §§22.2 and 22.6. Growing
retained/copied input, buffered output, and continuation storage remain covered
by live accounted owners under §§24.4–24.5; buffering creates no accounting
exception.

Global state owns shared/finalized state where the operator requires it.

No operator depends on an implicit process thread-local singleton for query correctness.

This separation permits worker-local execution without changing operator-state
ownership or embedding mutable query state in shared plans.

## 26.6 Borrowed-data lifetime in a pipeline

The executor may pass borrowed vectors/chunks synchronously across streaming operators.

Such borrowed data remains a value-stable logical view under §23.10 for the
entire downstream consumption interval; keeping the owner merely allocated is
not sufficient.

Under §26.4.2, lifecycle resolution or an operator's backing-release
acknowledgment alone does not permit physical reset/reuse: both operator and
downstream borrow dependencies must have ended, or an exact §§23.10–23.13
ownership mechanism must independently preserve every required view.

Before the upstream owner is reset/reused:

```text
all immediate downstream users of that borrowed data
must have completed consumption
```

A sink that retains data across calls or beyond the current batch retains a
valid owner or copies/materializes it.

Pipeline scheduling MUST NOT asynchronously queue a borrowed DataChunk for later execution after its owner has been recycled.

## 26.7 Query cancellation

`QueryExecutionContext` contains a query-wide cancellation flag/token.

Long loops check cancellation at chunk or reasonable block boundaries.

Cancellation unwinds query-owned resources through RAII/ownership:

```text
page guards
memory reservations
spill files
local/global operator state
read-epoch guard
```

Transaction-owned logical locks are released only through the transaction's normal terminal/abort path.

A query execution layer does not silently release transaction locks independently.

## 26.8 Pipeline early stop

A streaming operator such as LIMIT may signal `pipeline early stop` when no further upstream rows are semantically needed.

Pipeline early stop is distinct from `query cancellation`.

It stops only the safely unnecessary upstream portion of the execution graph.

It MUST NOT abort the transaction, report QueryCancelled, or prevent required blocking/side-effect work elsewhere in the query from completing.

Under §§20.12 and 20.14.5's owning demand rules, valid early stop may resolve
§26.4.2's accepted-input lifecycle by safely abandoning unnecessary remainder.
That remainder need not be processed merely to acknowledge consumption or
exhaust a source; cleanup is still required. No new input is admitted, and
subsequently undemanded upstream work cannot become visible merely to complete
the handoff protocol.

If early stop is established while offering final output, the operator
withholds new-input readiness immediately. Section 26.4.1 still requires the
output-available transition and its downstream consumption or valid ownership
handoff before terminal completion with no consumable output. The pending
terminal-control transition MUST NOT request another upstream input. Failure,
cancellation, or valid early stop requires no otherwise unnecessary source
terminal probe.

## 26.9 Worker and scheduling boundary

A query may execute with one worker under Chapter 32's worker model.

Independent of worker count, execution requires:

```text
global/local state split
pipeline dependency representation
source state that can be partitioned where valid
no mutable query state embedded in immutable plan nodes
```

Worker-pool scheduling, morsels, local-state combining, and concrete parallel operator algorithms are defined in Chapter 32.

## 26.10 Pipeline invariants

1. A pipeline is Source -> streaming operators -> Sink.
2. Blocking/finalization dependencies form an explicit DAG.
3. Empty chunks are not used as end-of-stream, and empty `HAVE_MORE` output advances finite execution state.
4. Physical-plan construction and runtime pipeline state are distinct.
5. Borrowed batch data remains value-stable and is consumed before the owner is recycled.
6. A retaining sink retains valid ownership or owns/deep-copies retained values.
7. Global and local runtime state remain distinct even in single-worker execution.
8. Query cancellation and safe pipeline early-stop are different mechanisms.
9. Cancellation releases query resources but transaction locks follow the transaction terminal path.
10. Parallel execution must preserve the same transaction/snapshot/read-epoch semantics.
