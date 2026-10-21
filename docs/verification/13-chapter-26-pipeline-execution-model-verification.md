# Chapter 26 — Pipeline Execution Model Verification

This family verifies `ARCHITECTURE.md` Chapter 26. Architecture owns the protocol;
the conceptual states, tokens, counters, and adapters below are test-side instruments,
not required C++ interfaces. V26-R names reusable owner procedures;
V26-S records each independently falsifiable obligation without recounting §26.10 summaries.

## V26-A — Source, output, and terminal protocol

Construct finite typed occurrence fixtures independently of the production source and
driver. Give equal-valued duplicate occurrences distinct fixture-only tags. Record offers,
downstream handoff completion, terminal control, failures, and attempted requests. Use:

| Oracle | Independent construction |
|---|---|
| ST | Source/streaming output-control trace over fixture occurrences; output and terminal are separate ordered events |
| AL | Per-local-state accepted-input automaton with independent release, resolution, handoff, pending-work, and terminal facts |
| OC | Expected output occurrence multiset/required sequence from a declarative operator relation, never production output |
| PG | Finite relevant-state graph with well-founded continuation edges and explicit dependency suspension |
| SK | Submitted-domain versus successful full-call sink acknowledgment ledger |
| EL | Plan-validation, initialization, required-work, root-completion, failure, and cleanup event predicate |
| DG | Declarative DAG with independently assigned demanded nodes and successful prerequisite-ready points |
| CL | Ending resources and active users from owner/borrow graphs plus V24 accounting/spill ledgers |
| ER | Owner-routing table plus V25-J's non-DML and V21-13's DML candidate oracles |
| PB | Partial output, successful offer, completed internal handoff, external delivery, and successful EOS history |
| AF | Statement/attempt generation ledger derived from V21-2, not a driver retry decision |
| OG | V23-G's generation-tagged, value-stable owner graph extended with continuation and diagnostic borrowers |
| DM | V20-11–17's semantic demand/early-stop and allowed-result predicates |
| RT | Role/trait and physical-property declarations from §§22.7/26.2/37.1 |
| IX | One-defect invalid-state predicates with forbidden-access/effect counters |
| EQ | Comparison of owner-defined values, NULLs, bags, required order, diagnostics, and transaction/result envelopes |
| PF | V22-E/V25-P field-provenance registry for runtime versus persistent identity |

Adapters observe semantic boundaries in pull, callback, or state-based implementations.
They must not repair a bad trace: a terminal outcome claiming consumable output cannot be
normalized into a valid output-then-terminal sequence. Conversely, one concrete call may
contain correctly ordered output/handoff/terminal events. Do not require two method calls,
enum numeric values, a return struct, `NEED_INPUT`, `READY_FOR_INPUT`, or `BLOCKED`.
Production driver status, scheduler counters, error reduction, and allocator behavior are
observations, never the expected-result generators. Release deterministic events/barriers;
use recorded seeds for finite trace variants, symbolic faults and owner generations, not
sleeps, wall-clock deadlines, pointer luck, host iteration order, or OS scheduling races.

The source oracle permits `OUTPUT_AVAILABLE` (the `HAVE_MORE` path) and terminal
`FINISHED` with no consumable output. Enumerate empty sources, one row, duplicate rows,
and several chunk partitions. A final nonempty offer must occur exactly once before its
handoff and terminal control. With already-known exhaustion, arm a forbidden logical-row
evaluation counter during the terminal probe. Empty sources may finish immediately.
At terminal control poison reusable storage with old rows, nonzero or arbitrary stale
cardinality, validity, and StringRefs: require zero output/payload consumption, not a
particular clearing operation. Storage not offered as a batch is not validated as one.

A concrete positive trace uses fixture tags a,b,c with values NULL,5,5 and capacity 2:
offer [a,b], complete handoff, offer [c], complete handoff, FINISHED/no output. Compare
the offered occurrence bag to {a,b,c}; stale [c] storage at FINISHED contributes nothing.
Prepend one empty HAVE_MORE whose finite segment cursor advances, then repeat. Both
traces have the same required occurrences and no cardinality-derived completion.

### Source and status × output matrix

EOS here means local source completion, never external cursor EOS. All rows use ST/OC
and V23-B; include negative cases rejected by IX without legalizing their traces.

| Source case | Conceptual status | Legal? | Consumable output? | Local EOS? | Progress | Next valid action |
|---|---|---|---|---|---|---|
| empty source | FINISHED | yes | no | yes | terminal | no further data request |
| nonempty nonfinal batch | HAVE_MORE | yes | yes | no | new occurrences | handoff then request |
| short nonfinal batch | HAVE_MORE | yes | yes | no | new occurrences | handoff; allow later rows |
| full batch, more rows or exhaustion next | HAVE_MORE | yes | yes | no | new occurrences | handoff; inspect next control event |
| empty progress batch | HAVE_MORE | yes with finite advance | valid empty batch | no | finite source state | continue, not EOS |
| empty unchanged-state batch | HAVE_MORE | no | no new occurrences | no | none | reject liveness violation |
| final nonempty batch | output available | yes | exactly once | no | final occurrences | handoff then terminal control |
| terminal observation | FINISHED | yes | no | yes | terminal | stop requesting |
| nonempty/stale storage at terminal | FINISHED, no output offer | yes | no, regardless of bytes | yes | terminal | ignore storage as a result |
| terminal marked with consumable output | terminal + offer | no | no | invalid transition | invalid | prevent consumption |
| post-terminal request | outside valid domain | no | no | already terminal | none | prevent/reject request; no rewind |
| output after terminal | forbidden offer | no | no | already terminal | invalid | reject, no extra occurrence |

Test source and streaming final output with the same ST rule. Negative post-terminal
traces are rejected by the oracle and prevented/rejected at an appropriate production
boundary; do not require a specific defensive response, repeated FINISHED, or rewind API.

## V26-B — Accepted-input lifecycle

AL assigns a fixture-only submission token to one logical input-occurrence domain, not
to a pointer or SQL-visible chunk ID. Start each local state ready with no unresolved
acceptance. Accept D once; continuation resumes D, even if a concrete API passes the same
input reference again. Attempt a second fresh acceptance of D and acceptance of distinct
E while D is unresolved. Neither may be admitted. Run E on an independent local state as
a positive parallel control. After legal readiness and OG release, reuse the same physical
chunk object for distinct E and require correct independent output.

Track resolution independently: all semantically required processing and not-yet-offered
output obligations must be discharged, or DM must establish valid early stop making the
remainder unnecessary. Already-offered output can still have outstanding handoff/borrow
obligations. Merely returning, offering one batch, copying input, or observing any output
cardinality is not resolution. Mutate each of those observations while holding unresolved
oracle work constant and reject false resolution. Acceptance itself must not imply that
processing, output delivery, or original-backing release has completed.

AL's conceptual lifecycle is ready -> accepted/pending -> resolved, with valid early-stop
resolution and separate unsuccessful terminal failure. Release acknowledgment is a
separate fact: it is allowed only after operator dependence on original backing has ended
or been independently preserved. Resolve, release, offer, and handoff may be separately
observable events; the oracle does not impose their simultaneous completion.

## V26-C — Continuation, readiness, and occurrence conservation

Define a finite declarative relation mapping one input occurrence to zero, one, or several
tagged output occurrences. For a multi-output case choose more outputs than capacity,
including duplicate values; compare every output against OC, with acceptance count one.
Compose one §28.8 probe case with explicit build-match enumeration and residual TRUE
selection, preserving matched-state/continuation across chunk splits. Do not use a second
production join as the sole expected-result oracle.

For the probe fixture use one probe key 7 and five build matches with payloads
10,10,20,30,40, all passing the residual predicate. With capacity 2, drain five tagged
pair occurrences over three offers (2+2+1); repeat as 1+1+1+1+1 and other legal partitions.
Compare pair multiplicity rather than requiring hash-join output order. In both retained
and copied realizations, acceptance remains one and no pending pair may disappear.

Exercise retained-input continuation and independently owned copied/buffered continuation
as separate valid realizations. In the copied case acknowledge original-backing release
while processing/output remains pending; assert unresolved lifecycle and no new-input
admission. Retained input stays value-stable under OG. Required output is offered exactly
once; neither replay nor generic input/output cardinality equality is an oracle.

Compute readiness as the conjunction of: preceding lifecycle resolved; no continuation
required before new input; outstanding output handoff completed; not terminal; no valid
early stop forbidding input. Start with a ready positive control, then falsify each term
independently and prove no new acceptance. Include a pending terminal-control transition
with no remaining row work. Derive pending continuation from remaining required processing,
not-yet-offered output, or that terminal-control event, not from whether caller input is
still borrowed. Input release alone cannot authorize another submission.

### Streaming-lifecycle matrix

Y/N are the indicated fixture facts; conditional means evaluate AL/OG, not implementation
guesswork. Every successful row has one acceptance. Reset means reuse without independently
preserving the affected view. All rows use AL/OC/OG.

| Fixture | Accepted / once? | Resolved? | Operator dependency live? | Output pending? | Downstream borrow live? | New input? | Reset original backing? | Terminal? |
|---|---|---|---|---|---|---|---|---|
| one input -> one output, handed off | Y/Y | Y | N | N | N | Y if otherwise ready | Y | N |
| one input -> zero output | Y/Y | Y | N | N | N | Y | Y | N |
| one input -> multiple outputs, first offered | Y/Y | N | conditional | Y | conditional | N | OG decides | N |
| retained-input continuation | Y/Y | N | Y | Y | conditional | N | N | N |
| copied/buffered continuation | Y/Y | N | N | Y | N | N | Y | N |
| released input, pending processing/output | Y/Y | N | N | Y | conditional | N | only without remaining borrow | N |
| resolved, output still borrowed downstream | Y/Y | Y | N | N, already offered | Y | N until handoff complete | N | N |
| early stop, final output awaiting handoff | Y/Y | Y if no required unoffered work | conditional | terminal control | conditional | N | OG decides | pending |
| failure during continuation | Y/Y | not successful resolution | until safe cleanup | unusable failed work | until quiescent | N | after dependencies end | failed |
| cancellation during continuation | Y/Y | not successful resolution | until safe cleanup | unusable failed work | until quiescent | N | after dependencies end | canceled |

## V26-D — Empty batches, shape, and finite progress

Reuse V23-A/B/C/D/E/M for legal DataChunk capacity/cardinality, active-domain schema,
validity, and representation checks. Insert short and full batches both before more rows
and before exhaustion; neither size predicts terminality or pending finality. Empty
Filter-like output can resolve an input with no fake acknowledgment batch. An empty source
HAVE_MORE requires finite state advancement and does not itself acknowledge exhaustion.

PG is built from fixture work, remaining output, dependency edges, and ownership events.
For no-new-occurrence processing steps, require input resolution, a genuine operator
dependency release, or finite relevant advancement toward output/resolution/terminal.
Distinguish an empty offered batch from producing a new output occurrence. Enumerate a
finite advancing path, an unchanged self-loop, and a cycle that merely renames equivalent
state. Reject both non-progress cycles without executing an infinite loop or imposing a
production counter/timeout. Include an irrelevant counter increment negative control.
Legitimate dependency suspension has an explicit prerequisite event and resumes only after
readiness; it is not immediate retry of unchanged work.

Cross PG with V24-H's separate denied-resource graph: resource reclaim cannot excuse a
stalled operator, and an advancing operator cursor cannot authorize an unchanged denied
allocation retry. Accept both models only when each applicable progress predicate holds.

### Progress matrix

| Step | Ch26 progress? | Legal nonterminal? | May continue? | Must suspend? | Invalid? | Oracle / owner |
|---|---|---|---|---|---|---|
| new required output occurrence | Y | Y | after handoff | N | N | OC/PG, §26.4 |
| accepted input resolved | Y | Y | when ready | N | N | AL/PG |
| operator dependency released | Y if genuine | Y | remaining work | N | N | OG/PG |
| finite relevant continuation advance | Y | Y | Y | N | N | PG |
| empty output + resolution | Y | Y | when ready | N | N | AL/PG |
| empty output + finite advancement | Y | Y | Y | N | N | ST/PG |
| equivalent state, no output/advance | N | N | N | not a wait | Y | IX/PG |
| legitimate dependency not ready | not processing progress | suspended | after prerequisite | Y | N | DG, §32.8 |
| resource-pressure progress alone | insufficient | only with valid operator step | both predicates | owner-dependent | stalled PG still invalid | PG + V24-H |
| cancellation established | terminal path | N | cleanup only | quiesce users | N | ER/CL |
| local terminal completion | terminal | N | no data request | N | N | ST/AL |

## V26-E — Generic sink acceptance

SK records the submitted logical domain before a sink call. On successful completed
acceptance require the entire domain once, not a subset or duplicate. Include empty and
duplicate-valued domains. Hold barriers after arbitrary physical work but before success;
inject failure/cancellation there and reject a full-success acknowledgment. Do not infer
that no physical work occurred. Prohibit generic replay into the failed instance and route
any admitted statement retry through AF. Retaining sinks use OG/V24-D; poison caller
backing after legal release and require retained exact values. Place each success flag
(query, statement, Combine, Finalize, commit, client exposure) behind a separate oracle
gate and prove sink acceptance alone cannot set it.

### Sink matrix

All rows use SK/EL/PB. Publication is §§31.9–31.10-owned; Chapter-21/39 envelopes govern
statement/commit outcomes. Y for stable retention is conditional on retaining values.

| Event | Input accepted? | Complete domain? | Stable retention? | Blind replay? | Query success? | Statement success? | Finalization still required? |
|---|---|---|---|---|---|---|---|
| successful Sink | Y | Y exactly once | if retained | N | not implied | not implied | owner-dependent |
| retaining Sink | Y on success | Y | Y | N | not implied | not implied | owner-dependent |
| blocking sink phase | Y on success | Y for this call | Y | N | not implied | not implied | required build contract |
| failed Sink | no successful acknowledgment | not certified | until safe cleanup | N | N | N | cannot fabricate success |
| canceled Sink | no successful acknowledgment | not certified | until quiescent | N | N | N | cleanup remains |
| subsequent Combine | no new Sink acceptance | not an acceptance event | preserve owned state | N | not implied | not implied | owner-dependent |
| subsequent Finalize | no new Sink acceptance | not an acceptance event | preserve output ownership | N | not implied | not implied | dependent/root work may remain |
| ResultSink | Y on successful call | Y | Y if retained | N | not implied | not implied | result strategy/envelope |

## V26-F — Generic execution lifecycle

EL consumes a manually declared valid plan, initialized runtime graph, demanded-work set,
DG prerequisites, semantic finalizers, and root/result obligations. Its success predicate
requires all semantically required work and owner-required finalization to have succeeded,
root obligations discharged, and no established terminal failure/cancellation. It never
reads a production success flag as proof. Normalize execution traces to validation ->
initialization -> required work -> required finalization/dependent work -> root completion,
or terminal failure -> quiescence/cleanup; streaming/result servicing can interleave where
the owner permits. This is a partial-order model, not a mandated serial phase enum.

Reuse V22-A/E/K: record a semantic plan digest, reject malformed plans before execution,
and run two distinct contexts against one immutable plan. Poison stale runtime generations
between runs. Check initialization before first use and plan immutability after every
graph/state construction event. Map source cursors, chunks, scratch, local buffers, and
continuation to worker/task-local state where practical under the owning contract; map
genuinely shared/finalized state to one
execution's global owner. Interleave two queries on the same worker and one query across
local states to detect implicit process thread-local singleton dependence. Per-local
serialization does not become global serialization. Verify shared transaction/snapshot/
CommandId/read-epoch identity through V22-E and Parallel Execution Tests.

### Execution lifecycle matrix

The oracle column identifies the canonical Architecture owner. Publication throughout is
Chapter 31; cleanup is required for every acquired resource, allowing valid transfers.
No failed runtime state is reusable in any row. These shared columns apply to every row.

| Event | Local completion? | Query success by itself? | Required semantic work remaining? | Finalization remaining? | Oracle / owner |
|---|---|---|---|---|---|
| validated plan | not runtime completion | N | Y | as declared | EL/V22-K, §§22/38.24 |
| runtime initialization | initialized only | N | Y | as declared | EL/OG, §22.6 |
| streaming execution | invocation-dependent | N | possibly | as declared | AL/EL, §26.4 |
| local source FINISHED | source Y | N | other work may remain | possibly | ST/EL, §26.4.1 |
| local operator terminal | operator Y | N | other work may remain | possibly | AL/EL |
| sink acceptance | call Y | N | build/root may remain | possibly | SK/EL |
| required Combine complete | step Y | N | dependent/root may remain | possibly | DG/EL, §§29/32 |
| required Finalize complete | prerequisite Y | N | output/root may remain | other owners may remain | DG/EL |
| valid early stop | unnecessary portion closed | N | still-demanded work | still-required steps | DM/EL, §§20/26.8 |
| root completion | internal Y | only full EL predicate | N if EL satisfied | N if EL satisfied | EL/PB, §26.3.1 |
| successful external EOS | result Y | owner-authorized success | none required outstanding | none required outstanding | PB, §31.10 |
| terminal semantic error | unsuccessful terminal | N | no successful resume | no fabricated success | ER/EL, §§25/21/39 |
| resource error | unsuccessful terminal | N | cleanup/propagation | success unavailable | ER/CL, §§24/39 |
| cancellation | unsuccessful terminal | N | quiescence/cleanup | success unavailable | ER/CL, §§26.7/39 |
| cleanup | ownership ending | not proof | cannot replace required work | cannot substitute | CL, §§23/24/39 |
| authorized retry | fresh instance | N | fresh attempt work | fresh owner state | AF, §§31.5/39.1.4 |

## V26-G — Dependency readiness, Combine, and Finalize

Declare DG nodes/edges from fixture operator contracts, not the emitted scheduler graph.
Cover Source -> Sink and Source -> Filter -> Project -> Sink, plus hash-build/probe,
aggregate build/output, sort input/output, DML target-spool/write, and lazy IN build/probe.
Check acyclicity, source/streaming/sink identification, and the absence of missing or
backward prerequisites. Keep scalar/EXISTS/IN side plans dormant while DM excludes them;
release first demand, run the required child to completion/valid early stop, then resume
the outer consumer. Eager execution of every known side plan is a negative fixture.

Specialize Blocking-state publication below with DG. Stop before final input, during each
required Combine/Finalize, after successful Finalize but before dependent output, and with
one of several prerequisites unfinished. Compare actual runnable/consumable state with
DG, not a production dependency counter. Independently inject uninitialized, unready,
unfinalized, failed, and canceled predecessor states; none authorizes successful use.
After successful readiness require the dependent positive control to proceed. Finalize
failure supplies no successful readiness event and still requires CL cleanup.

Use Aggregate Tests' exact state/lowest-semantic-ordinal oracle for all-group validation
before the first aggregate result, including an early completed group with another failing
group. Use Sort Tests' comparator and explicit complete run/input sets for sort output.
Hash probe consumes only its required finalized build partitions; DML writes wait for the
target-spool prerequisite. Do not require every sink to implement Combine or Finalize, or
require blocking work not demanded by its semantic owner. Successful Finalize is not root
success if a dependent output source still has required rows or error-producing work.

## V26-H — Semantic finalization versus cleanup

CL tracks semantic finalization separately from resource release. For each owner requiring
Finalize, omit it after input exhaustion and attempt SUCCESS: EL must reject. Then inject
Finalize failure and require cleanup without pretending Finalize succeeded. Include a sink
whose contract has no such step as a positive control against over-required finalization.
Extend V24-D/M and Cancellation resource matrix below to success, terminal semantic/resource
failure, cancellation, valid early stop, and abandoned retry at each ownership boundary.
Track page guards, chunks, row collections, arenas, reservations, spill buffers/files,
pipeline tasks, local/global states, and read-epoch protection. Every ending owner releases
exactly once; only explicit valid transfers survive. OG must permit destruction first.
Observe terminal cleanup events and absence of cycles/orphan owners, not elapsed time.
No destructor ordering, mandatory deep-copy strategy, or cleanup-as-Finalize is imposed.

## V26-I — Canonical error-owner transport

Build ER from V25-I/J/K, V21-13, V24-L, V20-12/13, and Aggregate Tests. Partition demanded
candidates across allowed source/chunk/lane/worker/pipeline/callback discovery points.
Release a worse candidate first while an independently known better candidate remains.
Then release the better one; require the owner's selected minimum, or an independently
valid unbeatable-minimum proof. Repeat merge trees/permutations and equivalent candidate
representatives. Do not require all-candidate storage or undemanded candidate work.

For non-DML use V25-J's same-origin `(1,0)` versus `(INT32_MIN,-1)` division and
`"x"` versus `"2147483648"` INT32-cast cases, plus distinct SourceSpans. For DML use
V25-K's abandoned-attempt discriminator and V21-13's same-span phase/eligibility fixtures;
do not fabricate a new DML cause tie-break. Compare responsible span, semantic origin,
cause/category, phase, eligibility, and every other frozen diagnostic field before and
after transport. Wrappers may add context but cannot erase structured causes. Relabel
pipeline/chunk/worker IDs, pointers, and callback order without changing expected records.

Use symbolic resource/cancellation faults at declared semantic boundaries. Check category
and §39.1 consequences rather than asserting a universal winner against ordinary candidates.
Include pending candidates when a resource event prevents further establishment. Preserve
the owner-established causal error through cleanup, with fatal/cleanup diagnostics chained
under §39.1.7. Chapter-26 routing must not add a global cross-class comparator.

### Error-owner matrix

All rows preserve provenance and use §39.1 for transaction consequences. Failed current
invocation output is nonconsumable; prior completed transitions are PB-owned and separate.
These shared columns apply to every row, including correct exclusion from ranking.

| Event | Candidate/terminal | Selection owner | Pipeline observation | First physical discovery selects? | Oracle |
|---|---|---|---|---|---|
| ordinary non-DML candidate | candidate | §25.1.1 | retain/reduce owner-correctly | N | V25-J/ER |
| D25 selected minimum | terminal expression error | §25.1.1 | propagate minimum | N | V25-J/EL |
| eligible DML candidate | candidate | §21.16.1 | preserve metadata/eligibility | N | V21-13/V25-K |
| D21 selected error | terminal statement error | §21.16.1 | no V25-J pre-rankinging | N | V21-13/EL |
| aggregate finalization | specialized selected error | §29.3.7 | preserve ordinal/cause | N | exact aggregate oracle |
| scalar cardinality | specialized terminal error | §§20.14.4/20.14.12 | preserve child/cardinality precedence | not a generic ranking | V20-12/ER |
| OutOfMemory | operational terminal | §§24.10/39.3 | preserve cause | no new cross-class rule | V24-L |
| representability ExecutionError | operational terminal | §§24.10/39.3 | not OOM/scalar cast relabeling | no new cross-class rule | V24-L |
| SpillIOError | operational terminal | §§24.10/39.3 | temporary failure category | no new cross-class rule | V24-L |
| QueryCancelled | unsuccessful terminal | §§26.7/39 | outside D25/D21 ranking | no new cross-class rule | ER/CL |
| persistent-page corruption | lower-layer terminal classification | §39.1.3 | preserve noncontinuable cause | no downgrade | ER/IX |
| internal protocol violation | invalid runtime | §39.1.3 | no public SQL-error candidate | not applicable | IX |

## V26-J — Internal output and external publication

PB keeps distinct events for invocation start, partial writes, successful offer, completed
downstream handoff, completed cursor return, successful EOS, and terminal failure. Inject
failure after half an output domain is initialized; poison remaining payload/validity and
require no consumption of that invocation's output. A buffer containing rows is not an
offer. First complete internal handoff A, then fail B: A remains recorded, but client
visibility must still come from an independent cursor event.

Extend V25-O: return A through the cursor, record consumption or a client copy within
§31.10's lifetime, then establish ordinary error, OOM, or cancellation on subsequent work.
No delivery retraction and no successful-complete-query result are permitted. Do not read
A after the next Next()/destruction unless independently retained. Exercise both streaming
cursor servicing and pre-materialized internal completion without requiring either strategy.
Attempt successful EOS while a required Finalize, dependency, or error-producing task is
outstanding; EL/PB reject it. Reuse V21-3/14 for DML RETURNING: statement and autocommit
publication envelopes remain separate from SELECT's already-returned prefix.

### Publication matrix

| Event | Consumable? | Client-visible by itself? | Prior completed delivery retractable? | Query complete? | Error owner | Lifetime owner |
|---|---|---|---|---|---|---|
| partial current output | N | N | N | N | if failing, ER | OG |
| successful current offer | Y under handoff | N | N | not implied | none implied | §§23/26.6 |
| prior completed internal handoff | completed internally | not implied | N | not implied | none implied | receiving owner |
| prior cursor-returned chunk | within declared lifetime | Y | N | prefix only | none implied | §31.10 |
| terminal semantic error | failed invocation N | error response only | N | unsuccessful | D25/D21/specialized | OG/§31 |
| OOM | failed invocation N | error response only | N | unsuccessful | §§24/39 | OG/§31 |
| cancellation | failed invocation N | cancel response only | N | unsuccessful | §39 | OG/§31 |
| successful external EOS | no new data implied | completion response | N | owner-authorized success | none pending | §31 |

## V26-K — Cancellation, failure, and quiescence

Set the shared QueryExecutionContext cancellation token at controlled chunk/block checks,
including long source, continuation, build, and finalization loops and a suspended task.
Check that normal owner-defined observation points see it; no fixed polling cadence or
wall-clock bound is an oracle. After canonical terminal error/cancellation, attempt output,
new input, continuation, or SUCCESS in the same instance: reject all successful processing.
The failure event is distinct from ordinary local FINISHED and valid early stop.

Hold an active writer and a borrowed consumer at separate barriers when failure is
established. Do not release their backing until the users quiesce or valid ownership is
preserved; afterward no partially active successful task graph or orphan continuation
remains. Cancellation prevents new unnecessary tasks. For success, hold a second required
error-producing worker/finalizer after the first pipeline completes. SUCCESS is withheld;
release success to permit completion or release an error to require the failure path.
Reuse V24-M and Pipeline Finalization and Resource Tests for the resource ledger. Verify
transaction locks remain owned until the command/transaction terminal path, not an
executor-local release. Preserve §39.1's pre/post-write consequence and original causal
diagnostic. These barriers specify ordering observations, not a scheduler primitive.

## V26-L — Retry and fresh attempt-local state

AF derives retry admission from V21-2 and the first-published-write model, never from
generic continuation. At an admitted pre-write retry, poison failed source position,
accepted domain, continuation, candidate accumulator, sink acceptance, Combine/Finalize
flags, temporary output, and scratch with distinct generations. Start the fresh attempt
and compare against its independent source-start and semantic oracle: no work may be
skipped by stale flags, duplicated by stale acceptance, or ranked using abandoned errors.
Snapshot freshness and retained CommandId follow the statement owner. Positive controls
reuse immutable plan configuration and legally reusable capacity only after new logical
initialization/ownership; physical allocation reuse is not failed semantic-state reuse.

### Retry matrix

In every row a new execution inherits none of the old source position, acceptance,
pending output/continuation, candidate, sink/Finalize, or scratch contents. This supplies
each state-category column without implying a ban on safely reinitialized storage.

| Ending condition | Same failed runtime may resume? | Fresh attempt required if retry admitted? | Retry automatically admitted? | Oracle / owner |
|---|---|---|---|---|
| successful execution | not a failed runtime; no rewind | new execution has fresh logical state | N | AF/V22-A |
| retry-eligible failed attempt | N | Y | only V21 admission | AF/V21-2 |
| terminal semantic error | N | Y | N | ER/§39.1 |
| resource error | N | Y | N | V24-M/§39.1 |
| cancellation | N | Y | N | CL/§39.1 |
| failed sink | N | Y | N | SK/AF |
| failed Finalize | N | Y | N | DG/AF |
| partial internal progress then failed attempt | N | Y | only owning retry rule | AL/AF |

Verify retry rows through AF/V21-2/V24-M. Add post-write and RR negative admission
controls; neither a generic sink failure nor cleanup creates retry permission.

## V26-M — Backing release, borrowing, and reset

Extend V23-G/H with separate operator-dependency, outstanding-output, downstream-borrow,
retained-continuation, result-owner, and diagnostic edges. At every AL release/resolution
event attempt reset or one reachable-state mutation. Reset is legal only after all affected
dependencies end or an exact mechanism independently preserves each view. Include a
multi-operator synchronous chain held inside its last consumer: an intermediate Execute
return alone cannot end a live downstream borrow. Queue a borrowed chunk, recycle its
producer, and release the queued consumer only after a barrier; require prevention or
independent ownership before recycling, not a stale access discovered by allocator luck.

Use copied continuation to release original input before resolution; separately resolve
input while a downstream output view remains live. Both must follow OG, not one completion
flag. Cover StringRef bytes/metadata, validity, selections, dictionary bases, and active
cardinality, not merely owner allocation. Extend OG to pending/selected diagnostic backing;
destroy/reset attempts before reporting completes must preserve the referenced data. Reuse
V24-B/D/M for accounted retained/copied/buffered/diagnostic storage and continuous transfers.

### Borrow matrix

All rows use OG/AL/CL and Chapter-24 accounting for query-owned growing storage. Transfer
means independent valid preservation if original backing is reset before a dependency ends.

| Backing/user | Operator dependency? | Downstream dependency? | Lifecycle resolved? | Operator release? | Physical reset? | Transfer needed for early reset? |
|---|---|---|---|---|---|---|
| accepted caller input still needed | Y | optional | N | N | N | Y |
| retained-input continuation | Y | optional | N | N | N | Y |
| independently copied continuation | N on original | optional | may be N | Y | only if no remaining borrow | if a borrow remains |
| output borrowing input | may be N | Y | may be Y | may be Y | N | Y |
| active downstream consumer | may be N | Y | may be Y | may be Y | N | Y |
| sink-retained input | receiving owner required | Y until ownership split | call accepted | caller release only if safe | OG decides | if original still needed |
| cursor-retained result | receiving owner | client lifetime | upstream may resolve | owner-dependent | not while required | if producer ends first |
| error diagnostic backing | diagnostic user | reporting lifetime | success resolution irrelevant | only without dependency | not before safe reporting | if backing ends first |
| success cleanup | ending dependencies checked | live transfers allowed | may be Y | OG decides | only safe owners | if retaining beyond producer |
| failure cleanup | quiesce users first | valid transfers survive | not successful resolution | after users/preservation | only safe owners | if retaining beyond producer |

## V26-N — Early stop and demanded upstream work

Use DM rather than raw-source row counts. Include OFFSET then LIMIT, LIMIT 0, absent
finite LIMIT, filters rejecting initial rows, and blocking work needed to produce final
rows. Once the owning Limit or EXISTS predicate is satisfied, arm an otherwise erroring
upstream occurrence beyond the demanded domain and a source-request counter: the protocol
must not fetch it merely for exhaustion or terminal control. Keep a separate required
blocking/side-effect branch to prove early stop prunes only safely unnecessary work.

For final output plus early stop, require output offer -> completed handoff/valid ownership
transfer -> terminal/no-output, with readiness withheld immediately and zero new upstream
input between those events. An unnecessary remainder may resolve by DM-authorized
abandonment, not by error/cancellation relabeling. No QueryCancelled or executor-induced
transaction abort is allowed for ordinary early stop; CL cleanup still applies. Failure,
cancellation, and early stop require no otherwise unnecessary source terminal probe.

### Early-stop matrix

| Case | More new input demanded? | Accepted remainder needed? | Successful lifecycle resolution? | Subsequent upstream error visible? | Cleanup? | Classification / owner |
|---|---|---|---|---|---|---|
| ordinary source execution | DM decides | required work Y | after obligations | only demanded error | Y | ST/DM, §§20/26.4 |
| Limit satisfied | N for stopped path | unnecessary remainder N | valid abandonment | undemanded N | Y | normal early stop, §§20.12/26.8 |
| EXISTS satisfied | N for existence path | unnecessary remainder N | valid abandonment | undemanded N | Y | §§20.14.5/26.8 |
| valid operator terminal | N | none still required by that owner | owner-defined completion | no newly demanded work | Y | AL/ST |
| terminal error | no successful resume | no demand to finish protocol | N, failed | ER governs established causes | Y | ER/§39 |
| cancellation | no successful resume | no demand to finish protocol | N, canceled | no new candidate ranking | Y | CL/§39 |

Verify these rows using DM/AL/ER/CL and V20-11–17. Preserve specialized scalar-two-row
and lazy IN-complete-build demand where composing side plans; do not apply EXISTS rules
to those modes.

## V26-O — Sink roles, breakers, and physical properties

Construct RT from literal role responsibilities, not an operator class name. Check the
source/streaming/sink decomposition, source DataChunk production, and streaming without
requiring all future input. Inspect each breaker declaration independently for resident
state, spillability, Finalize transition, post-finalize source/ready state, and dependent
pipelines. Omit one field at a time and reject the incomplete declaration. Compare build
and source roles of one blocking operator without treating them as the same invocation.

Use §27.11's synchronous ResultSink option as a sink not inherently requiring full-input
blocking; use sort/aggregate and hash-build readiness as blocking examples. Preserve each
owner's retention/finalization requirements. RT/V22-C compares physical-property sets to
OrderingProperty and RequiredSlotSet; sink/breaker/blocking/streaming/spillability cannot
become an additional property or imply SQL ordering.

### Sink/breaker role matrix

| Role/trait | Consumes input? | Streaming output? | Retains state? | Complete-input prerequisite? | Dependency boundary? | Sink acknowledgment? | External publication owner | Canonical owner / status |
|---|---|---|---|---|---|---|---|---|
| streaming operator | Y | zero/one/multiple batches | continuation may | not all future input | not inherent | Execute lifecycle, not Sink | §31 if reached | §§26.2/26.4 |
| sink | Y | owner-dependent | may | not inherent | not inherent | full successful call | §31 if result role | §26.4.3 |
| blocking operator | build phase | after required readiness | Y | owning build contract | owner-defined | when in sink role | §31 if reached | §§29/30 |
| pipeline breaker/boundary | trait, not separate call | owner-defined | readiness state | owner-defined | Y | does not redefine Sink | not by itself | §26.2 |
| source after finalization | no build input in source phase | Y | ready state | prerequisite satisfied | consumes ready state | not source acknowledgment | §31 if reached | §§26.2/29/30/32 |
| ResultSink | Y | result strategy | safely retained if needed | not inherent; RETURNING has own gate | not inherent | full successful call | §§31.9–31.10 | §§27.11/31 |
## V26-P — Invalid protocol states and persistence negatives

IX changes one precondition in a valid trace and includes positive controls. Observe
construction prevention, admission guards, or safe boundary rejection; require no specific
assertion/API or full-tree revalidation per chunk. For every invalid row below: valid=N,
public SQL semantic error=N, internal=Y, and no persistent effect is permitted from the
invalid transition. This does not undo legitimate writes preceding discovery; §39.1 owns
the resulting noncontinuable/transaction consequence. Instrument unsafe reads, writes,
offers, duplicate acceptances, and persistent writers before releasing the bad event.

### Invalid-state matrix

| Invalid transition/state | Safe prevention/rejection point | Multiplicity risk | Memory-safety risk | Owner / oracle |
|---|---|---|---|---|
| FINISHED claiming consumable output | before output consumption | final-row loss/duplication | stale backing | ST/IX, §26.4.1 |
| output after terminal | before offer | extra occurrences | reused state | ST/IX |
| post-terminal data request | before restart/data work | replay | expired state | ST/IX |
| new input before resolution | before acceptance | dropped/replayed work | overwritten pending input | AL/IX |
| duplicate logical acceptance | before second acceptance | duplication | possible aliasing | AL/OC/IX |
| continuation as fresh input | before reacceptance | duplication | possible aliasing | AL/IX |
| reset with live dependency | before mutation/dereference | stale values | Y | OG/IX, §23.10 |
| handoff incomplete, next input admitted | before admission | lost output | overwrite | AL/OG/IX |
| equivalent no-progress state | finite trace/cycle admission | missing completion | unbounded retention possible | PG/IX |
| failed output consumed | before payload/validity access | invalid rows | uninitialized/dangling access | PB/IX |
| dependency consumed before ready | before consumption | partial results | partial state access | DG/IX |
| success before required Finalize | before success/EOS response | missing results/errors | lifetime misuse possible | EL/PB/IX |
| failed state reused by retry | before fresh-attempt work | skips/replay | stale owners | AF/IX |
| invalid schema/capacity/cardinality/selection | before typed/range access | wrong domain | bounds/type violation | V22-K/V23-M/V25-N |

Extend PF to pipeline/global/local state, source positions, acceptance state, continuation,
output buffers, worker/pipeline IDs, sink acceptance, candidates, dependency-ready flags,
and cleanup state. Trace field provenance into page/WAL/catalog/recovery codecs: none
becomes persistent semantic identity, TxnId, or CommandId. Do not mistake legitimate scalar
payload encoding or disposable query-temporary spill for persisting runtime identity;
byte-pattern absence is not an oracle. Reuse V22-E/V25-P and V21-3 for transaction effects.
Attempt COMMIT inference from local FINISHED, pipeline completion, sink acceptance, root
completion, and cursor EOS separately: none can replace the command/transaction envelope.

## V26-Q — Chunking, scheduling, and representation determinism

Replay the same semantic fixture with capacities 1, 2, 7, 1024, and symbolic/feasible 65535;
single-row, irregular, short, full, and multi-output partitions; inserted empty progress
events; legal local-state/worker/pipeline schedules; and relocated/relabelled runtime
objects. Check every normalized trace against ST/AL/DG/OG before comparing EQ. Keep
transaction, effective snapshot, CommandId, read epoch, and semantic plan fixed. Physical
evaluation count, pipeline trace shape, allocation count, and unordered presentation order
need not be equal. Use fixed event-release permutations, not actual completion luck.

For deterministic-output domains EQ compares exact values/NULLs, multiplicity, required
order, D25 minimum or D21 winner in its eligible domain, transaction effect, and successful
external completion. Where an owner admits several results (unordered LIMIT subbags or
ORDER BY ties), compare its allowed-result predicate, not one arbitrarily chosen sequence
or subbag. Required semantic demand is checked against DM for the same admitted result
context; it is not physical instruction count. This prevents Verification from freezing
SQL order absent in Chapter 20. Resource feasibility may differ across exact realizations;
compare successful outcomes, or same-domain canonical errors absent a different legitimate
resource/cancellation event. Separately make one budget succeed and another legitimately
OOM, and check V24-L classification instead of demanding equal outcomes.

### Determinism matrix

`=` means the owner-defined equality/allowed-result predicate above. For each row values,
NULLs, bag multiplicity, required ordering, semantic demand, transaction effect, and
external completion use that predicate. D25 and D21 columns apply only in their respective
eligible domains. Resource feasibility is not asserted invariant.

| Perturbation | Values/NULL/bag/order/demand | D25 winner | D21 winner | Resource feasibility | Transaction/completion | Oracle |
|---|---|---|---|---|---|---|
| capacity | = | = | = | may differ | owner-equivalent | EQ/ST |
| chunk boundary/short chunks | = | = | = | may differ | owner-equivalent | EQ/ST |
| empty progress insertion | = | = | = | may differ | owner-equivalent | EQ/PG |
| source batching | = | = | = | may differ | owner-equivalent | EQ/ST |
| continuation batching | = | = | = | may differ | owner-equivalent | EQ/AL |
| one-input/multi-output partition | = | = | = | may differ | owner-equivalent | EQ/OC |
| worker completion | = | = | = | controlled faults may differ | owner-equivalent | EQ/DG/ER |
| pipeline schedule | = | = | = | controlled faults may differ | owner-equivalent | EQ/DG/ER |
| pointer/address relocation | = | = | = | no semantic role | owner-equivalent | EQ/PF |
| runtime ID relabeling | = | = | = | no semantic role | owner-equivalent | EQ/PF |
| allocation/temporary layout | = | = | = | may differ | owner-equivalent | EQ/OG/V24-L |

## V26-R — Cross-chapter reuse map

Each reference names a reusable deterministic procedure and its independent oracle.
V26 adds the pipeline boundary observations above; it does not duplicate normative owners.
Compact V-family references denote the uniquely named headings in this document.

| Handoff | Contract / Architecture owner | V26 family | Reusable Verification methodology | Independent oracle |
|---|---|---|---|---|
| Ch20→26 | bags/order/demand/Limit/side plans, §§20.12/20.14/20.17 | C/G/N/Q | V20-4/5/10–18 | bag, allowed subbag, DM, scalar order |
| Ch21→26 | attempts/DML rank/RETURNING, §§21.15–21.16.1 | E/I/J/L | V21-2/3/13/14 | attempt, candidate, result envelope |
| Ch22→26 | immutable plan/schema/context/validation, §§22.2–22.8 | D/F/O/P | V22-A/B/C/E/K | plan digest, schema, ownership, legality |
| Ch23→26 | chunks/empty/borrowing/reset, §§23.1/23.10–23.13 | A/D/M/P | V23-A/B/C/D/E/G/H/K/M | active domain, OG, generation mutations |
| Ch24→26 | accounting/progress/cleanup/resources, §§24.4–24.6/24.10 | D/H/I/K/L/M | V24-B/D/H/L/M/N | charge/pressure/cleanup/category ledgers |
| Ch25→26 | demand/results/D25/DML/failed output, §§25.1.1–25.1.2/25.7 | C/I/J/M/Q | V25-B/E/I/J/K/L/N/O/P | occurrence, candidate, ownership, PB |
| Ch26→27 | immediate unary/result handoff, §§27.7–27.11 | A/C/E/M/N/O | V20-5; V23-K; Scan and Unary Operator Tests | scalar predicate/projection, OG, DM |
| Ch26→28 | probe continuation, §28.8 | C/Q | V26-C explicit match enumeration; Hash Join Tests case set | OC Cartesian candidate/residual model |
| Ch26→29 | Combine/Finalize/all-group validation, §§29.2/29.3.7 | G/H/I | V29-A/M/N/O/Q; Aggregate Tests smoke index | PL/IO/FT/ER/RO |
| Ch26→30 | blocking sort/output, §30.1 | G/O/Q | Sort Tests; Blocking-state publication; V26-G | comparator, complete-input/run DG |
| Ch26→31 | RETURNING/cursor/prefix/EOS, §§31.9–31.10 | E/F/J/M | V21-3/14; V25-O; V23-G/K; V26-J | result envelope, PB, OG |
| Ch26→32 | workers/dependencies/cancellation, §§32.3–32.8 | F/G/K/Q | Parallel Execution Tests; Blocking-state publication; V26-K | context, DG, CL |
| Ch26→37 | properties distinct from traits, §37.1 | O | V22-C; V26-O | literal property/trait RT |
| Ch26→39 | categories/internal/transaction consequences, §§39.1/39.3 | I/J/K/L/P | V24-L/M; V21-2/3; V25-N/O; V26-I | ER, publication-boundary, IX |

## V26-S — Atomic architecture-obligation ledger

The ledger below counts independently falsifiable rules, including permitted alternatives
that a too-restrictive protocol would reject. Repeated invariants and illustrative examples
map to their existing rows, not extra counts. Pure explanatory rationale and navigation
are not atomic obligations. Each mapping requires the named deterministic fixture and independent
oracle, directly or through the exact V26-R reuse link. Repair a mapping whose linked
methodology no longer checks its obligation. No unavailable runtime category requires an N/A row in this Chapter-26 inventory.

| ID | Architecture section | Atomic obligation | Verification section | Independent oracle | Reused methodology |
|---|---|---|---|---|---|
| V26-A01 | 26.4; 26.4.1 | The generic handoff exposes semantic facts without producer-specific conventions. | V26-A | ST/OC | V23-B |
| V26-A02 | 26.4; 26.4.1 | Pull, callback, and state-based encodings remain legal without mandatory return types or new enums. | V26-A | ST/OC | V23-B |
| V26-A03 | 26.4; 26.4.1 | HAVE_MORE is the source's nonterminal output path. | V26-A | ST/OC | V23-B |
| V26-A04 | 26.4; 26.4.1 | Terminal completion carries no consumable output for sources and streaming operators. | V26-A | ST/OC | V23-B |
| V26-A05 | 26.4; 26.4.1 | Reusable storage present at FINISHED is not consumed as this invocation's output. | V26-A | ST/OC | V23-B |
| V26-A06 | 26.4; 26.4.1 | Every final consumable batch is offered before terminal completion. | V26-A | ST/OC | V23-B |
| V26-A07 | 26.4; 26.4.1 | The final output's consumption or valid ownership handoff precedes terminal control. | V26-A | ST/OC | V23-B |
| V26-A08 | 26.4; 26.4.1 | Ordered output/terminal transitions do not require two particular method calls. | V26-A | ST/OC | V23-B |
| V26-A09 | 26.4; 26.4.1 | Known-exhaustion terminal control performs no additional logical-row evaluation. | V26-A | ST/OC | V23-B |
| V26-A10 | 26.4; 26.4.1 | An empty source may finish immediately without a fake empty batch. | V26-A | ST/OC | V23-B |
| V26-A11 | 26.4; 26.4.1 | HAVE_MORE does not promise a subsequent nonempty batch. | V26-A | ST/OC | V23-B |
| V26-A12 | 26.4; 26.4.1 | Zero cardinality does not establish EOS. | V26-A | ST/OC | V23-B |
| V26-A13 | 26.4; 26.4.1 | Short cardinality does not establish EOS. | V26-A | ST/OC | V23-B |
| V26-A14 | 26.4; 26.4.1 | Full cardinality does not establish EOS. | V26-A | ST/OC | V23-B |
| V26-A15 | 26.4; 26.4.1 | Cardinality establishes neither pending terminality nor final-batch identity. | V26-A | ST/OC | V23-B |
| V26-A16 | 26.4; 26.4.1 | Completion is not encoded by submitting an empty input batch. | V26-A | ST/OC | V23-B |
| V26-A17 | 26.4; 26.4.1 | Empty source HAVE_MORE advances finite source/operator state. | V26-A | ST/OC | V23-B |
| V26-A18 | 26.4; 26.4.1 | No logical occurrence is offered after local terminal completion. | V26-A | ST/OC | V23-B |
| V26-A19 | 26.4; 26.4.1 | The driver requests no further data from a terminal execution state. | V26-A | ST/OC | V23-B |
| V26-A20 | 26.4; 26.4.1 | Post-terminal requests are invalid, not a rewind or required idempotent-FINISHED protocol. | V26-A | ST/OC | V23-B |
| V26-B01 | 26.4.2 | A logical input submission is accepted at most once within an execution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B02 | 26.4.2 | Acceptance does not imply all required processing completed. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B03 | 26.4.2 | Acceptance does not imply all output has been offered. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B04 | 26.4.2 | Acceptance does not imply original caller backing has been released. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B05 | 26.4.2 | Submission identity is not pointer, DataChunk-object, or buffer identity. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B06 | 26.4.2 | The same physical chunk can carry a distinct submission after lifecycle and borrow obligations permit. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B07 | 26.4.2 | At most one accepted-input lifecycle is unresolved per LocalOperatorState. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B08 | 26.4.2 | Independent local states, workers, and pipelines are not globally serialized by that rule. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B09 | 26.4.2 | Continuation resumes accepted work without fresh acceptance. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B10 | 26.4.2 | A continuation API may pass the same input reference without a second submission. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B11 | 26.4.2 | Normal resolution requires discharge of all attributable required processing and output obligations. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B12 | 26.4.2 | Valid owning early stop may resolve a semantically unnecessary remainder. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B13 | 26.4.2 | An invocation return alone does not establish resolution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B14 | 26.4.2 | Offering one output batch alone does not establish resolution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B15 | 26.4.2 | Copying input alone does not establish resolution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B16 | 26.4.2 | Output cardinality alone does not establish resolution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-B17 | 26.4.2 | Already-offered output may remain subject to consumption/borrowing after resolution. | V26-B | AL/OC/OG | V22-A/E; V23-G |
| V26-C01 | 26.4.2 | Value-stable retained-input continuation is permitted. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C02 | 26.4.2 | Independently copied/transferred/materialized continuation is permitted. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C03 | 26.4.2 | Backing-release acknowledgment requires ended or independently preserved operator dependencies. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C04 | 26.4.2 | Original-backing release may precede lifecycle resolution. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C05 | 26.4.2 | Required output may remain pending after original backing is releasable. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C06 | 26.4.2 | Pending continuation includes required processing before new-input admission. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C07 | 26.4.2 | Pending continuation includes not-yet-offered required output. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C08 | 26.4.2 | Pending continuation includes a required terminal-control transition. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C09 | 26.4.2 | New input requires the preceding accepted lifecycle to be resolved. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C10 | 26.4.2 | New input requires no continuation that must precede it. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C11 | 26.4.2 | New input requires completed outstanding output handoff. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C12 | 26.4.2 | New input is forbidden after operator terminal completion. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C13 | 26.4.2 | New input is forbidden once valid early stop withholds it. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C14 | 26.4.2 | Output availability, pending continuation, backing release, and readiness are independently determinable. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C15 | 26.4.2 | One accepted input may yield multiple output batches without resubmission. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C16 | 26.4.2 | One accepted input may resolve successfully with zero output occurrences. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C17 | 26.4.2 | Zero-output resolution requires no fake output acknowledgment batch. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C18 | 26.4.2 | Every owner-required output occurrence is offered exactly once without protocol loss or replay. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C19 | 26.4.2 | The generic protocol imposes no equality of input/output cardinality. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-C20 | 26.4.2 | The protocol establishes no SQL row ordering. | V26-C | AL/OC/OG | V23-G; V25-E; V26-C match enumeration |
| V26-D01 | 26.4.1; 26.4.2; 26.10(3) | A nonterminal processing step without new output resolves input, releases a dependency, or advances finite relevant continuation state. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D02 | 26.4.1; 26.4.2; 26.10(3) | Input resolution without output is valid progress. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D03 | 26.4.1; 26.4.2; 26.10(3) | Genuine operator-dependency release without output is valid progress. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D04 | 26.4.1; 26.4.2; 26.10(3) | Finite relevant continuation advancement without output is valid progress. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D05 | 26.4.1; 26.4.2; 26.10(3) | Finite input/output continuation is well-founded rather than an equivalent-state cycle. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D06 | 26.4.1; 26.4.2; 26.10(3) | Unchanged empty HAVE_MORE cannot substitute indefinitely for terminal completion. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D07 | 26.4.1; 26.4.2; 26.10(3) | Progress does not require a production numeric counter, timeout, or sleep. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D08 | 26.4.1; 26.4.2; 26.10(3) | Legitimate dependency waiting suspends rather than busy-retrying unchanged processing. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D09 | 26.4.1; 26.4.2; 26.10(3) | Resource progress alone cannot excuse stalled operator continuation. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-D10 | 26.4.1; 26.4.2; 26.10(3) | Operator-state mutation cannot excuse resource retry without Chapter-24 progress. | V26-D | PG/AL/OG | V23-B; V24-H |
| V26-E01 | 26.4.3 | Completed successful Sink accepts the complete submitted domain, with no generic partial-success acknowledgment. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E02 | 26.4.3 | Successful sink acceptance counts the submitted domain exactly once. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E03 | 26.4.3 | Failed or incomplete Sink is not represented as complete successful acceptance. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E04 | 26.4.3 | Failed sink input is not blindly replayed by the generic driver. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E05 | 26.4.3 | Sink failure is not interpreted as proof that no physical work occurred. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E06 | 26.4.3 | Sink acceptance alone does not establish query success. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E07 | 26.4.3 | Sink acceptance alone does not establish statement success. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E08 | 26.4.3 | Sink acceptance alone does not establish Combine success. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E09 | 26.4.3 | Sink acceptance alone does not establish Finalize success. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E10 | 26.4.3 | Sink acceptance alone does not establish commit. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-E11 | 26.4.3 | Sink acceptance alone does not establish client publication. | V26-E | SK/EL/PB/AF | V21-2/3/14; V23-G; V24-M |
| V26-F01 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Runtime execution starts from the validated immutable physical plan. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F02 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Mutable runtime state is initialized before use. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F03 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Source/operator/sink/pipeline mutable state belongs to the execution/statement attempt. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F04 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Execution cursors and other mutable work do not mutate immutable plan configuration. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F05 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Shared immutable configuration and more specifically owned state retain their canonical owners. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F06 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | The builder identifies sources, streaming chains, sinks, and required dependencies from the finalized plan. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F07 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Pipeline construction does not mandate a recursive row-at-a-time Next chain. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F08 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Whole-execution success requires all semantically required execution work to succeed. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F09 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Whole-execution success requires operator-owned finalization/publication prerequisites to succeed. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F10 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Local source/operator FINISHED alone cannot establish whole-execution success. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F11 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | One completed pipeline alone cannot establish whole-execution success. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F12 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Root/internal completion is distinct from external result exposure and cursor EOS. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F13 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Internal completion before or during cursor servicing remains result-strategy dependent. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F14 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Successful internal execution does not independently establish transaction commit. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F15 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Immutable configuration, per-execution global state, and worker/task-local state remain distinct even with one worker. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F16 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Hot cursors/chunks/scratch/buffers/continuation use local ownership where practical, with global shared state only where the owner requires. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-F17 | 26.1; 26.3; 26.3.1; 26.5; 26.9; 26.10(4,7) | Query correctness does not depend on an implicit process thread-local singleton. | V26-F | EL/OG/DG | V22-A/E/K; Parallel Execution Tests |
| V26-G01 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Pipeline structure is Source, zero or more streaming operators, then Sink. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G02 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Blocking/finalization dependencies form an explicit DAG. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G03 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Hash probe waits for required hash-build finalization. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G04 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Sort output waits for input/run finalization. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G05 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Aggregate output waits for aggregate finalization. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G06 | 26.1; 26.2; 26.3.1; 26.10(1–2) | DML write waits for target-spool finalization. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G07 | 26.1; 26.2; 26.3.1; 26.10(1–2) | An initialized lazy IN probe waits for its build finalization. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G08 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Scalar/EXISTS/IN side plans remain dormant until semantically demanded. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G09 | 26.1; 26.2; 26.3.1; 26.10(1–2) | An outer pipeline resumes only after its demanded side-plan completion/valid early stop. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G10 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Knowing the DAG does not authorize eager execution of dormant side plans. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G11 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Dependent execution consumes only successfully ready required predecessors, never partial/unready/unfinalized/failed/canceled state. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G12 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Aggregate readiness includes numerical validation of every group/aggregate state before any aggregate row is exposed. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G13 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Each owner-required Combine is semantic work gating success. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G14 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Each owner-required Finalize is semantic work gating success. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G15 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Operators without an owning Combine/Finalize requirement are not forced to perform those steps. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G16 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Required predecessor/finalizer failure cannot authorize partial-state fallback or successful completion. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-G17 | 26.1; 26.2; 26.3.1; 26.10(1–2) | Successful Finalize alone is insufficient while required dependent/root work remains. | V26-G | DG/DM/EL | Blocking-state publication; Aggregate Tests; Sort Tests; V20-12–14 |
| V26-H01 | 26.3.1; 26.7; 26.8 | Semantic Finalize is distinct from resource cleanup/destruction. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H02 | 26.3.1; 26.7; 26.8 | Cleanup remains required when semantic finalization fails. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H03 | 26.3.1; 26.7; 26.8 | Ending execution resources are released on success. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H04 | 26.3.1; 26.7; 26.8 | Ending execution resources are released on terminal failure. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H05 | 26.3.1; 26.7; 26.8 | Ending execution resources are released on cancellation. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H06 | 26.3.1; 26.7; 26.8 | Ending execution resources are released on valid early termination. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H07 | 26.3.1; 26.7; 26.8 | Valid ownership transferred to longer-lived owners survives producer cleanup. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-H08 | 26.3.1; 26.7; 26.8 | Cleanup cannot fabricate successful semantic finalization. | V26-H | CL/EL/OG | V24-D/M; Cancellation resource matrix; Execution-failure cleanup |
| V26-I01 | 26.3.2; 26.4.2 | Candidate discovery is distinct from establishment of a terminal public error. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I02 | 26.3.2; 26.4.2 | Execution preserves owner-required candidate state until canonical selection or a valid unbeatable-candidate proof. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I03 | 26.3.2; 26.4.2 | Owner-correct local reduction/merging is permitted without full candidate materialization. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I04 | 26.3.2; 26.4.2 | Error establishment does not demand upstream-skipped work. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I05 | 26.3.2; 26.4.2 | Ordinary non-DML terminal expression errors are selected by V25-J. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I06 | 26.3.2; 26.4.2 | Source/lane/chunk/worker/pipeline/callback discovery order cannot replace canonical candidate selection. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I07 | 26.3.2; 26.4.2 | DML candidate eligibility and selection remain V21-13-owned. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I08 | 26.3.2; 26.4.2 | Required eligible DML candidates cannot be discarded by V25-J pre-ranking. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I09 | 26.3.2; 26.4.2 | Transport preserves responsible SourceSpan. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I10 | 26.3.2; 26.4.2 | Transport preserves semantic diagnostic origin. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I11 | 26.3.2; 26.4.2 | Transport preserves public category. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I12 | 26.3.2; 26.4.2 | Transport preserves conceptual/owning cause. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I13 | 26.3.2; 26.4.2 | Transport preserves applicable DML expression phase and eligibility. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I14 | 26.3.2; 26.4.2 | Transport preserves every other owner-frozen diagnostic field. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I15 | 26.3.2; 26.4.2 | Diagnostic wrappers may add context but cannot erase lower-layer structured causes. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I16 | 26.3.2; 26.4.2 | Aggregate-finalization error selection remains with its specialized owner. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I17 | 26.3.2; 26.4.2 | Scalar-subquery cardinality and child/build precedence remain with their specialized owners. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I18 | 26.3.2; 26.4.2 | Persistent-page corruption retains its lower-layer noncontinuable classification. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I19 | 26.3.2; 26.4.2 | OutOfMemory retains Chapter-24/39 classification. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I20 | 26.3.2; 26.4.2 | Runtime representability/resource ExecutionError retains its classification. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I21 | 26.3.2; 26.4.2 | SpillIOError retains temporary-resource classification. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I22 | 26.3.2; 26.4.2 | QueryCancelled remains outside ordinary candidate ranking. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I23 | 26.3.2; 26.4.2 | Resource/cancellation events may prevent further candidate establishment under their owners. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I24 | 26.3.2; 26.4.2 | Chapter 26 introduces no global precedence between independently owned failure classes/components. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-I25 | 26.3.2; 26.4.2 | Original causal-error preservation and chained cleanup/fatal diagnostics remain §39.1.7-owned. | V26-I | ER/DM/AF | V25-I/J/K; V21-13; V24-L; V20-12/13; Aggregate Tests |
| V26-J01 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | An invocation failing before a successful output transition exposes no consumable partial/uninitialized current output. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J02 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | Rows merely written into a buffer do not establish output availability. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J03 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | A prior independently completed internal output handoff remains completed after a subsequent failure. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J04 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | Completed internal handoff does not by itself imply client visibility. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J05 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | A prior completed cursor delivery is not retroactively retracted by a subsequent terminal error. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J06 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | A prior completed cursor delivery is not retroactively retracted by subsequent cancellation. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J07 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | An externally returned prefix is not successful whole-query completion. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J08 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | The handoff does not mandate whole-query buffering or a new cursor protocol. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J09 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | Successful external EOS remains result-owner controlled and cannot bypass required execution/finalization. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J10 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | DML RETURNING exposure retains its successful-statement envelope. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-J11 | 26.3.1; 26.3.2; 26.4.1–26.4.3 | Autocommit result exposure retains its separate commit envelope. | V26-J | PB/EL/ER | V25-O; V21-3/14; V23-G/K |
| V26-K01 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Success is withheld while required execution/error-producing finalization work is outstanding. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K02 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | No partially active successful pipeline/task graph remains runnable after terminal failure/cancellation. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K03 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Active writers/users quiesce before required shared backing is destroyed. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K04 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Terminal cleanup leaves no orphaned continuation using destroyed state. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K05 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | An owner-established terminal error cannot become success in the same runtime instance. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K06 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Established cancellation cannot become success in the same runtime instance. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K07 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Terminal failure permits propagation/quiescence/cleanup, not successful continuation or new-input admission. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K08 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Terminal error is distinct from ordinary FINISHED. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K09 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Cleanup does not turn failed accepted-input state into successful resolution. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K10 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Cancellation is carried by the query-wide execution-context token. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K11 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Long loops observe cancellation at chunk or reasonable block boundaries. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K12 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Cancellation prevents scheduling new unnecessary work and stops running work at its observation points. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K13 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Executor cleanup does not independently release transaction-owned logical locks. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K14 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Execution failure/cleanup does not choose independent statement/transaction consequences. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-K15 | 26.3.1; 26.3.2; 26.4.2; 26.7; 26.10(8–9) | Quiescence requirements do not mandate a specific scheduler or synchronization primitive. | V26-K | EL/ER/DG/CL/OG | V24-M; V21-3; Parallel Execution Tests; Execution-failure cleanup |
| V26-L01 | 26.3.1; 26.4.2–26.4.3 | Only the statement owner admits retry; continuation/cleanup does not create retry permission. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L02 | 26.3.1; 26.4.2–26.4.3 | An admitted retry uses fresh source-position state rather than resuming the failed cursor. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L03 | 26.3.1; 26.4.2–26.4.3 | An admitted retry uses fresh accepted-input lifecycle state. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L04 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit pending continuation. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L05 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit local error candidates. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L06 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit sink acceptance state. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L07 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit Combine/Finalize state. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L08 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit retained temporary output. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L09 | 26.3.1; 26.4.2–26.4.3 | An admitted retry does not inherit query scratch contents. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-L10 | 26.3.1; 26.4.2–26.4.3 | Retry preserves the owner's snapshot/CommandId/transaction boundary and permitted immutable sharing. | V26-L | AF/EL/OG | V21-2; V22-A/E; V24-M |
| V26-M01 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Borrowed views remain value-stable over their complete downstream consumption interval. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M02 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Keeping a backing owner merely allocated is insufficient for borrow validity. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M03 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Operator backing-release acknowledgment alone does not permit physical reset. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M04 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Accepted-input resolution alone does not permit physical reset. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M05 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Reset/reuse waits for both operator and downstream dependencies to end or be independently preserved. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M06 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | All immediate downstream users complete their required borrow before unpreserved producer reuse. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M07 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | A retained sink/result view acquires valid ownership beyond the input interval. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M08 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Queued asynchronous borrowed data cannot outlive recycled backing without valid preservation. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M09 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Pending/selected diagnostic backing remains valid until reporting no longer depends on it. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M10 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Growing retained/copied/buffered continuation and diagnostic storage remain covered by Chapter-24 accounted owners. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M11 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Borrow correctness permits exact independent ownership mechanisms without mandating copying or zero-copy. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-M12 | 26.3.1; 26.3.2; 26.4.2–26.4.3; 26.5–26.6; 26.10(5–6) | Returned cursor values retain Chapter-31 lifetime rather than an extended pipeline-local lifetime. | V26-M | OG/AL/CL | V23-G/H/K; V24-B/D/M; V25-L |
| V26-N01 | 26.1; 26.3.1; 26.8; 26.10(8) | Early stop applies only where the owning demand rule makes upstream work unnecessary. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N02 | 26.1; 26.3.1; 26.8; 26.10(8) | Early stop is distinct from query cancellation and does not report QueryCancelled. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N03 | 26.1; 26.3.1; 26.8; 26.10(8) | Ordinary early stop does not abort the transaction. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N04 | 26.1; 26.3.1; 26.8; 26.10(8) | Early stop cannot suppress required blocking/side-effect work elsewhere in the graph. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N05 | 26.1; 26.3.1; 26.8; 26.10(8) | Valid early success does not require exhaustion of unread upstream input. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N06 | 26.1; 26.3.1; 26.8; 26.10(8) | Semantically unnecessary remaining input need not be processed merely to acknowledge consumption. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N07 | 26.1; 26.3.1; 26.8; 26.10(8) | Protocol completion cannot expose errors from newly undemanded upstream work. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N08 | 26.1; 26.3.1; 26.8; 26.10(8) | Early stop established with final output withholds new-input readiness immediately. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N09 | 26.1; 26.3.1; 26.8; 26.10(8) | The final-output early-stop terminal-control transition fetches no new upstream input. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N10 | 26.1; 26.3.1; 26.8; 26.10(8) | Failure, cancellation, and early stop require no otherwise unnecessary source terminal probe. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-N11 | 26.1; 26.3.1; 26.8; 26.10(8) | Limit and EXISTS keep their specialized final-row demand rules rather than raw-source-count rules. | V26-N | DM/AL/ST/CL | V20-11–17; V21-3 |
| V26-O01 | 26.2; 26.9 | A source produces executable DataChunks under the physical scheman owner. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O02 | 26.2; 26.9 | A streaming operator does not require all future input to transform its accepted batch. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O03 | 26.2; 26.9 | A sink is an input-consumption role governed by generic sink acceptance. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O04 | 26.2; 26.9 | A blocking operator may expose a source only after required finalization. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O05 | 26.2; 26.9 | A sink is not automatically a pipeline breaker or whole-input accumulator. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O06 | 26.2; 26.9 | A breaker is not a synonym for generic sink acknowledgment. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O07 | 26.2; 26.9 | Blocking/streaming/spillability remain execution traits, not extra Chapter-37 properties. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O08 | 26.2; 26.9 | Breaker classification describes decomposition/readiness dependencies without changing operator semantics. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O09 | 26.2; 26.9 | A breaker declares its memory-resident state. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O10 | 26.2; 26.9 | A breaker declares its spillability. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O11 | 26.2; 26.9 | A breaker declares its Finalize transition. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O12 | 26.2; 26.9 | A breaker declares its post-finalize output/source state. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O13 | 26.2; 26.9 | A breaker declares dependent pipelines that must wait. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O14 | 26.2; 26.9 | Source/streaming/sink roles remain explicit without requiring a particular iterator hierarchy. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-O15 | 26.2; 26.9 | ResultSink delegates external exposure to the Chapter-31 result owner. | V26-O | RT/DG/SK | V22-C; Scan and Unary Operator Tests; V26-G |
| V26-P01 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Terminal/acceptance/continuation/readiness/lifetime misuse is internal, not a new public SQL error. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-P02 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Invalid runtime state is prevented/rejected before unsafe access, stale output, or invalid mutation. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-P03 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Executable output/input chunks satisfy their physical schema, cardinality/capacity, active-domain, and initialization owners. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-P04 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Runtime pipeline/local/global/continuation/source/sink/dependency/error state is not persisted as page/WAL/catalog/recovery identity. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-P05 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Fixture/runtime submission, chunk, pipeline, or worker labels do not create semantic row identities, TxnIds, or CommandIds. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-P06 | 26.3.1; 26.3.2; 26.4.2; 26.5–26.6 | Invalid-transition rejection does not authorize erasing legitimate prior database writes or altering §39 consequences. | V26-P | IX/PF/OG/EL | V22-E/K; V23-M; V25-N/P; V21-3 |
| V26-Q01 | 26.4; 26.5; 26.9; 26.10(10) | Single-worker execution remains permitted by the worker owner. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q02 | 26.4; 26.5; 26.9; 26.10(10) | Source partitioning is permitted only where valid under the source/parallel contract. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q03 | 26.4; 26.5; 26.9; 26.10(10) | Parallel execution preserves effective transaction/snapshot/CommandId/read-epoch semantics. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q04 | 26.4; 26.5; 26.9; 26.10(10) | Worker count does not remove required global/local state separation or dependencies. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q05 | 26.4; 26.5; 26.9; 26.10(10) | Legal batching/continuation/scheduling/representation choices preserve owner-defined successful values, NULLs, multiplicity, and required ordering. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q06 | 26.4; 26.5; 26.9; 26.10(10) | Legal physical perturbations preserve semantic demand and owner-selected ordinary errors without physical-ID tie-breakers. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q07 | 26.4; 26.5; 26.9; 26.10(10) | Owner-admitted unordered results are compared by their allowed-result predicate, not newly imposed SQL ordering. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q08 | 26.4; 26.5; 26.9; 26.10(10) | Resource feasibility differences remain legitimate without changing successful semantics. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |
| V26-Q09 | 26.4; 26.5; 26.9; 26.10(10) | Chapter 32 retains worker-pool/morsel/combining/scheduling algorithm ownership. | V26-Q | EQ/EL/DG/OG | V20-23; V22-C/E; V23-L; V25-P; Parallel Execution Tests |


### V26 procedure coverage and negative-case checks

For every ledger row, check the named fixture's positive case, one-defect negative case,
and independent expected predicate. Read the whole Chapter-26 source against this ledger,
including §26.1 graph/lazy-demand rules, §26.2 declarations, §26.3 lifecycle/error handoffs,
§26.4 protocols, and §§26.5–26.10 ownership/cancellation/worker summaries. Repeated rules
reuse rows rather than creating duplicate counts. A verification reference must resolve to
an actual deterministic procedure; a heading name alone is insufficient.

Audit Chapter-26-relevant Verification wording for these stale rules and require their
rejection: terminal with consumable data; empty/short-derived EOS; mandatory full Execute
consumption; continuation as resubmission; copied input treated as resolved; pending output
always requiring original input; mandatory NEED_INPUT/BLOCKED; one-input/one-output caps;
empty output always invalid; equivalent-state retry; local FINISHED/sink acceptance/input
exhaustion treated as query success; cleanup replacing Finalize; first physical candidate
selection; D25 pre-ranking DML; internal output equated with client visibility; retracting
returned prefixes; failed-state retry reuse; sink equated with breaker. Negative examples
that reject these behaviors are not contradictory methodology.

Use deterministic event order rather than sleeps or timing-based liveness oracles.
Expected results remain independent of production behavior and of ABI, enum, or storage
representation choices.
