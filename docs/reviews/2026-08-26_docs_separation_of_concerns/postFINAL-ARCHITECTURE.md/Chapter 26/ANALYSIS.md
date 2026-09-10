# CHAPTER 26 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED

The review found **2 BLOCKING, 2 MAJOR, 2 MINOR, and 0 EDITORIAL findings**.

The blockers concern the runtime handoff protocol—not new SQL semantics:

- Whether source `FINISHED` can accompany consumable final output.
- How streaming execution communicates input consumption, pending output, and continuation.

Chapter 26 already correctly distinguishes empty batches from EOS, early stop from cancellation, and borrowed-view stability from mere owner liveness. No contradiction requires reopening Chapter 25.

## 1. Repository and scope

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | Clean |
| Index | Clean | Clean |
| HEAD | `0ef3f09ff79f94220e5ebde24ed5be487f25092c` | Unchanged |
| `git diff --check` | — | Passed |
| Audit-created changes | None | None |

Historical review artifacts remained **unread, unmodified, unmoved, and unstaged**. No report file was created.

### Exact primary boundary

- Title: [# 26. Pipeline Execution Model](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20606)
- Start: **20606**
- End: **20881**, including the closing separator and blank line.
- Next heading: [# 27. Scans and Unary Physical Operators](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20882)
- Chapter 27 was consulted only for immediate execution handoffs; its review was **not started**.

Context consulted:

- §§20.2, 20.4–20.7, 20.12, 20.14, 20.17, 20.17.5.
- §§21.13–21.16.1.
- §§22.1–22.8.
- §§23.1, 23.10–23.14 and relevant representation/domain rules.
- §§24.4–24.6, 24.10.
- §§25.1–25.2, 25.7–25.8, including D25-S1 and DML transport.
- §§27.1–27.2, 27.7–27.11, handoff only.
- §§28.3, 28.7–28.8; §§29.2–29.3.7; §30.1.
- §§31.5, 31.9–31.10.
- Chapter 32’s delegated worker/state/dependency/fairness contracts.
- §38.24; §§39.1.3–39.1.7, 39.3; §41.5.

Verification context included V21-2/13/14, V22 ownership/validation, V23-B/G/H/K/L/M, V24 resource/cleanup families, V25-I–Q, Pipeline Finalization and Resource Tests, Parallel Execution Tests, and Scan and Unary Operator Tests.

## 2. Findings and frozen questions

### B26-1 — Source terminal status does not define final-output validity

**Severity:** BLOCKING
**Primary type:** EXECUTION STATUS
**Location:** [§26.4, lines 20735–20758](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20735)
**Affected handoff:** Source → pipeline driver → downstream consumers.

Evidence:

- `GetData(..., output DataChunk) -> source status`.
- The minimum statuses are `HAVE_MORE` and `FINISHED`.
- Empty chunks are explicitly not EOS.
- The text does not say whether `FINISHED` makes the output argument invalid, permits a final valid batch, or requires that batch to have been returned earlier.

**FROZEN CHAPTER-26 ARCHITECTURE SEMANTIC QUESTION — Q26-1**

- **Valid runtime state:** A source has one final nonempty batch and reaches exhaustion while constructing it.
- **Competing interpretations:**
  1. Return that batch with `HAVE_MORE`; return `FINISHED` without output on a subsequent call.
  2. Return that batch with `FINISHED`; the caller consumes it before honoring termination.
- **Consequence:** Independently implemented producers and drivers can disagree about the final batch, losing or duplicating valid occurrences.
- **Frozen constraints:** §§20.4–20.7 preserve occurrences; §23.1 separates cardinality from completion; output must satisfy the physical schema and vector invariants.
- **Smallest decision:** Define the conceptual relationship between output availability and terminal status, including final nonempty output and permitted post-terminal caller actions. This need not prescribe a C++ return type or enum representation.

Neither interpretation independently licenses row loss. The blocker is the missing common handoff contract; end-to-end row loss would still violate upstream semantics.

### B26-2 — Streaming invocation lacks consumption/continuation semantics

**Severity:** BLOCKING
**Primary type:** INPUT CONSUMPTION
**Location:** [§26.4, lines 20760–20780](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20760), with §26.5 continuation counters.
**Affected handoff:** Pipeline driver ↔ streaming operator ↔ downstream consumer.

Evidence:

- `Execute(input, output, LocalOperatorState)` has no conceptual outcome specification.
- No distinction is stated between consumed input, retained input, pending output, or requesting another input.
- Actual downstream execution supports expansion: §28.3 permits one probe row to emit across multiple chunks, and §28.8 explicitly retains probe continuation.

**FROZEN CHAPTER-26 ARCHITECTURE SEMANTIC QUESTION — Q26-2**

- **Valid runtime state:** One probe input chunk produces more output occurrences than fit in one legal output chunk.
- **Competing interpretations:**
  1. Each return consumes the submitted input completely; further output is independently buffered/drained.
  2. A return may leave the input logically pending, requiring continuation before new input or reset.
- **Consequence:** Without an explicit common acknowledgment, a driver can replace still-needed input, resubmit consumed input, lose continuation output, duplicate matches, or repeatedly invoke a non-progressing continuation.
- **Frozen constraints:** Operator-specific multiplicity, §§23.10–23.13 value-stable borrowing, Chapter-25 demanded occurrence mapping, and Chapter-20 demand/order semantics.
- **Smallest decision:** Define conceptual input acceptance/consumption, output availability, pending continuation, completion/early-stop signaling, and progress requirements. Preserve freedom to implement these through callbacks, state, statuses, or another exact protocol.

`NEED_INPUT` is **not** an existing Chapter-26 status and must not be silently introduced as the answer.

### M26-1 — Generic lifecycle and completion handoff is incomplete

**Severity:** MAJOR
**Primary type:** PIPELINE LIFECYCLE
**Locations:** §§26.1–26.5, 26.7–26.8.
**Classification:** NONSEMANTIC INTEGRATION, partly dependent on Q26-1/Q26-2.

Chapter 26 establishes successful-predecessor finalization barriers, but does not assemble the complete driver lifecycle:

- Initialization/preconditions for runtime state.
- Completion of accepted input and local contributions before successful finalization.
- Required finalization versus resource destruction.
- Root-pipeline completion versus externally successful cursor exhaustion.
- Terminal failure, quiescence, and fresh-attempt reconstruction.
- Dynamic misuse classification after completion.

The necessary semantic constraints already exist in Chapters 20–25, operator owners, §31.9, and §39.3. In particular, §39.3 forbids a runnable partial graph after terminal failure.

**Consequence:** An implementer must reconstruct essential lifecycle sequencing across owners; source completion can too easily be mistaken for whole-query success.

**Smallest future action:** After the protocol decisions, add one owner-precise lifecycle/completion handoff. Do not mandate an exact number of method calls where idempotent or construction-based realizations are equivalent.

### M26-2 — Canonical error-selection and failure-publication handoff is unstated

**Severity:** MAJOR
**Primary type:** ERROR PROPAGATION
**Locations:** §§26.4, 26.7, 26.10.
**Classification:** NONSEMANTIC INTEGRATION.

Chapter 26 has no explicit bridge distinguishing:

- A physically discovered ordinary candidate.
- An owner-selected terminal error.
- A resource/fatal failure.
- A failed invocation’s unusable output.

D25-S1, D21-S4, specialized aggregate/subquery precedence, and §39 already determine the respective outcomes. Chapter 26 contains **no conflicting “first physical error wins” sentence**, but its generic interfaces do not explain their propagation.

**Consequence:** A straightforward stop-on-first-callback-error implementation could discard a smaller ordinary candidate, erase DML phase/provenance, or expose incomplete output.

**Smallest future action:** Delegate candidate selection and terminal propagation explicitly; preserve diagnostic backing through reporting; prohibit successful publication of failed invocation output using the existing owners. Do not create cross-class precedence.

### N26-1 — Production-sequencing language in live Architecture

**Severity:** MINOR
**Primary type:** TEMPORALITY
**Locations:** §§26.5, 26.9, 26.10.
**Classification:** DOCUMENT-ONLY.

Six chronology-bearing locations describe a “first production” executor or “later” parallelization rather than durable runtime rules.

**Consequence:** The chapter is not fully time-independent and obscures the distinction between permitted single-worker execution and development sequencing.

**Smallest future action:** Rewrite these passages as durable worker/state invariants, preserving Chapter 32 delegation and permitted implementation scope.

### N26-2 — “Sink / pipeline breaker” conflates roles

**Severity:** MINOR
**Primary type:** TERMINOLOGY
**Location:** [§26.2, line 20686](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20686).
**Classification:** DOCUMENT-ONLY.

The combined heading describes finalization-dependent blocking state, while §26.1’s example terminates at `ResultSink`, whose immediate owner permits synchronous consumption/materialization.

**Consequence:** Readers may incorrectly treat every terminal consumer as a blocking operator exposing a post-finalization source.

**Smallest future action:** Distinguish the generic sink role from the pipeline-breaker trait. No buffering or result-publication change is needed.

**EDITORIAL findings: none.**

## 3. Section-review matrix — A

“Appropriate” below means ARCHITECTURE-APPROPRIATE; “role issue” means ARCHITECTURE WITH DOCUMENT-ROLE ISSUE.

| Section / exact heading | Lines | Responsibility | Upstream owner | Downstream consumer | Role status |
|---|---:|---|---|---|---|
| 26.1 Pipeline graph | 20608–20657 | Work graph, dependency readiness, lazy side plans | Ch20–22, §29.3 | Builder/scheduler/dependents | Appropriate; M26-1 |
| 26.2 Pipeline roles | 20658–20717 | Role taxonomy | Ch22 | Concrete operators | Appropriate; N26-2 terminology |
| Source | 20660 | Chunk production | Ch22–23 | Driver | Appropriate |
| Streaming operator | 20674 | Batch transformation | Ch20, Ch25 | Driver/downstream chain | Appropriate; Q26-2 |
| Sink / pipeline breaker | 20686 | Accumulation/finalization dependency | Ch24, operator owners | Dependent source | Appropriate; N26-2 |
| 26.3 Pipeline construction | 20718–20732 | Derive execution graph from finalized plan | Ch22, §38.24 | Runtime driver | Appropriate |
| 26.4 Runtime interfaces | 20733–20781 | Source/Execute/Sink/Combine/Finalize handoffs | Ch22–25 | Driver/operator adapters | Appropriate; Q26-1/Q26-2, M26-1/M26-2 |
| 26.5 Global and local state | 20782–20801 | State separation and ownership | Ch22 | Workers/operators | Role issue; N26-1 |
| 26.6 Borrowed-data lifetime in a pipeline | 20802–20821 | Synchronous value-stable consumption | §§23.10–23.13, 25.7 | Streaming/retaining consumers | Appropriate |
| 26.7 Query cancellation | 20822–20841 | Observation and query-resource unwind | Ch22, Ch24, §39 | Driver/workers/transaction layer | Appropriate; integration M26-1/M26-2 |
| 26.8 Pipeline early stop | 20842–20851 | Stop only unnecessary upstream work | Ch20, Ch21 | Driver/scheduler | Appropriate |
| 26.9 Single-thread first, parallel-ready | 20852–20866 | Parallel-ready state/delegation | Ch22 | Ch32 | Role issue; N26-1 |
| 26.10 Pipeline invariants | 20867–20881 | Summary constraints | Detailed owners above | All runtime components | Role issue in item 10; N26-1 |

No subsection consists wholly of non-Architecture material.

## 4. Canonical owner and handoff matrix — B/Y

| Concept/handoff | Canonical owner | Chapter-26 role | Assessment |
|---|---|---|---|
| Pipeline graph and construction | Ch26 | Owns execution DAG construction | Clear |
| Source/streaming/sink roles | Ch26 | Owns generic roles | Sink/breaker terminology needs separation |
| Source status/output protocol | Ch26 | Owns interface contract | Q26-1 |
| Streaming consumption/continuation | Ch26 | Owns driver handoff | Q26-2 |
| Semantic values, bags, demand, ordering | Ch20 and scalar owners | Preserve during scheduling | No new semantic ownership |
| Statement attempts/DML publication | Ch21, transaction owners | Execute/discard attempt state | M26-1 integration |
| DML ranking | §21.16.1 | Transport eligible candidates | M26-2 integration |
| Physical plan/global/local state | Ch22 | Realize runtime ownership | Consistent restatement |
| DataChunk schema/domain/representations | Ch23 | Exchange valid chunks | Earlier owner |
| Borrow stability/reset | Ch23 | Own synchronous consumption interval | Consistent |
| Query accounting/resource progress | Ch24 | Account and unwind runtime state | Earlier owner |
| Expression demand/results/errors | Ch25 | Schedule and transport | M26-2 integration |
| Ch26→27 | Concrete scan/Filter/Project/Limit/ResultSink | Generic driver → concrete behavior | Context only |
| Ch26→28 | Join build barrier/probe continuation | Gate build; support continuation | Q26-2 at generic adapter |
| Ch26→29 | Aggregate finalization | Wait for complete numerical validation | Explicit, consistent |
| Ch26→30 | Sort build/output | Gate output on successful finalization | Consistent |
| Ch26→31 | Cursor and DML result envelopes | Internal output → result owner | M26-1 bridge |
| Ch26→32 | Workers, morsels, combining, scheduling | Explicit delegation | Later owner; no single-worker exclusion |
| Ch26→39 | Categories, terminal failure, transaction consequences | Preserve and propagate | M26-2 bridge |

No conflicting duplicate normative owner was found.

## 5. Execution, role, lifecycle, and consumption matrices — C/G/H/I/J/K/L

The model is **pipeline-driven hybrid execution**: sources are requested through `GetData`; produced batches flow through streaming `Execute` calls into sinks. The builder creates a dependency DAG. Chapter 32 owns concrete scheduling. It is not a mandatory recursive row-at-a-time pull chain.

| Role | Input | Output | Mutable state owner | Consumption/completion contract | Status |
|---|---|---|---|---|---|
| Source | No streaming input argument | DataChunk | Local source plus execution state | `HAVE_MORE`/`FINISHED`; final-output pairing unspecified | Q26-1 |
| Streaming operator | One input chunk per conceptual invocation | Output chunk | LocalOperatorState | Full/partial consumption and continuation acknowledgment unspecified | Q26-2 |
| Sink | Input chunk | Accumulated state; not necessarily immediate row output | Local/global sink state | Acceptance/continuation result unstated | Q26-2 where resumable; M26-1 lifecycle |
| Breaker | Many batches | Finalized state/post-finalize source | Operator execution state, Ch24 storage | Required dependencies wait for successful Finalize | Clear barrier |
| Post-finalize source | Finalized blocking state | DataChunks | Operator execution state | Uses source handoff | Q26-1 applies |
| ResultSink | Final internal chunks | Result-owner handoff | Execution/result owner | Synchronous consume or safe retention | §27.11/§31.10 |
| DML target-spool breaker | Candidate targets | Finalized write source | Attempt-local spool | Finalize before mutation | Ch21/31 specialization |

### Finalization versus teardown

| Operation | Meaning | Required ordering | Failure/cleanup result |
|---|---|---|---|
| `Sink` | Accumulate owner-defined input | Before successful completion of required build | No implicit commit |
| `Combine` | Incorporate local contributions | Required contributions precede dependent-ready publication | Meaning owned by operator; not universally commutative |
| `Finalize` | Establish successful finalized state | Every required predecessor must succeed before dependent runs | Failure cannot publish readiness |
| Resource destruction | Release ending ownership | Preserve live borrowers first | Required on failure/cancellation |
| Query success | Complete required semantic work/result envelope | Cannot substitute one source’s EOS for all required work | Generic bridge incomplete: M26-1 |

Exact call counts and idempotence are not frozen universally. Reapplying semantic contributions or exposing unfinalized state is not permitted.

### Input/output conservation

| Case | Required outcome | Missing generic detail |
|---|---|---|
| Filter rejects a complete input batch | Empty output; consumed input is legitimate progress | Invocation acknowledgment, Q26-2 |
| Project evaluates a batch | Per-demanded-occurrence values; output schema placement | No new semantic gap |
| One probe input produces several output chunks | Preserve all matches and pending input/backing | Drain/continuation acknowledgment, Q26-2 |
| Many inputs build one blocking result | Retain exact owner-defined state | Generic lifecycle bridge, M26-1 |
| Sink fails after partial internal work | Do not blindly replay chunk; terminate through owner | Explicit failed-handoff bridge, M26-2 |
| Valid output produced | Ch22 schema and Ch23 active domain; initialized values | Q26-1 determines source terminal availability |
| Internal output reaches ResultSink | Not automatically client-visible | §31.10 owns external return |

## 6. Status and transition matrices — D/E/F

Only **`HAVE_MORE` and `FINISHED`** are named source statuses. `pipeline early stop` is a separate signal. Errors are not specified as members of that status enum. `NEED_INPUT` and `BLOCKED` are absent.

| Actual status/signal | Producer | Terminal? | Input consumed? | Output valid? | Empty/nonempty? | Next action/progress |
|---|---|---|---|---|---|---|
| `HAVE_MORE` | Source | Nonterminal source outcome | No streaming input argument | Executable output contemplated | Both allowed | Consume applicable batch; request more; empty requires finite state advancement |
| `FINISHED` | Source | Source completion | N/A | **Unspecified** | Final nonempty pairing **unspecified** | Recognize EOS; output handling/post-terminal calls Q26-1 |
| `pipeline early stop` | Streaming operator, e.g. Limit | Ends safely unnecessary upstream work, not necessarily query | Exact consumed/produced boundary unstated | Must preserve required final output | Owner-dependent | Stop only unnecessary graph portion; Q26-2 acknowledgment |
| Terminal error | Source/operator/sink owner | Failed execution terminal | Do not infer safe replay | Failed current output not successful | Partial storage may exist | Quiesce/unwind; canonical error owner |
| Query cancellation | Query context | Terminal failed execution when acted upon | No continuation of failed attempt | No successful failing output | N/A | Stop unnecessary work and unwind |

### State transitions established versus missing

These are descriptive states, not proposed enum additions.

| Transition | Existing rule | Assessment |
|---|---|---|
| Finalized physical plan → pipeline graph | Builder creates runtime references without semantic mutation | Defined |
| Dormant side plan → runnable work | First semantic demand only | Defined |
| Source call → nonempty `HAVE_MORE` | Batch production | Defined |
| Source call → empty `HAVE_MORE` with advancement | Allowed | Defined |
| Source call → empty `HAVE_MORE` without advancement | Invalid/liveness violation | Defined |
| Source call → `FINISHED` with final data | Availability/consumption not defined | Q26-1 |
| Streaming call → another output from same input | Actual continuation exists; acknowledgment absent | Q26-2 |
| Streaming call → ready for new input | Consumed-input condition absent | Q26-2 |
| Build → successful Finalize → dependent runnable | Required predecessor success | Defined |
| Failed Finalize → dependent runnable | Forbidden | Defined by successful-finalization prerequisite |
| Terminal execution failure → runnable graph | Forbidden by §39.3 | Missing local navigation, M26-1 |
| Failed attempt → fresh admitted retry | Discard old attempt state; owner-controlled retry | Defined elsewhere; M26-1 |
| Completed source/operator → repeated invocation | Generic idempotence/rejection convention absent | Q26-1/Q26-2; do not assume rewind |

A complete concrete state machine cannot be supplied without resolving the two questions.

## 7. Empty batches, EOS, and progress — M/N

| Case | Valid? | Consume output? | EOS? | Progress/next action |
|---|---|---|---|---|
| Cardinality 0 + `HAVE_MORE` | Yes, with advancement | Empty logical batch; no scalar occurrences | No | Continue after finite state progress |
| Cardinality >0 + `HAVE_MORE` | Yes | Yes | No terminal status | Continue |
| Cardinality 0 + `FINISHED` | Completion can carry no batch | No rows | Yes from status, not cardinality | Stop source |
| Cardinality >0 + `FINISHED` | **Undetermined** | **Undetermined** | Terminal status present | Q26-1 |
| Short nonterminal chunk | Yes | Its active rows | No | Do not infer exhaustion |
| Full chunk | Yes | Its active rows | Not implied | Status controls completion |
| Exhausted source | Must report explicit completion | Terminal-data convention unresolved | Yes | Q26-1 for final delivery |
| Filter rejects every input row | Yes | Empty result | No | Input consumption is progress |
| `BLOCKED` with no output | No named status | N/A | N/A | Do not invent protocol |

| Progress case | Architectural result | Owner/status |
|---|---|---|
| Input consumed, no output | Legitimate operator progress | Operator semantics; Q26-2 acknowledgment |
| Output produced without new input | Legitimate continuation | §28.8; Q26-2 |
| Source cursor advances, no output | Permitted empty `HAVE_MORE` | §26.4 |
| Finite internal source/operator state advances | Permitted source progress | §26.4 |
| Empty `HAVE_MORE`, unchanged state | Forbidden | Explicit liveness rule |
| General streaming continuation with no advancement | Generic outcome/progress contract absent | Q26-2 |
| Real predecessor dependency pending | Not runnable until successful prerequisite | §§26.1/32.8 |
| Equivalent denied-allocation retry | Not progress; cannot repeat indefinitely | §24.6 |
| Cancellation | Stop/unwind at observation boundaries | §§26.7/32.8/39 |
| Successful terminal outcome | Required work completed | M26-1 bridge |
| Terminal failure | No runnable partial graph | §39.3 |

Source empty-batch liveness is already exact. There is no separate frozen question about empty-as-EOS. General continuation progress belongs with Q26-2 rather than a duplicate finding.

## 8. Demand and early termination — O

| Situation | Upstream work demanded? | Stop permitted? | Error from genuinely unnecessary work visible? | Cleanup/classification |
|---|---|---|---|---|
| Ordinary scan | Until required consumer work satisfied | Only through valid owner termination | No invented suppression of demanded errors | Normal lifecycle |
| Filter | Predicate for required retention decisions | Not merely because one batch is empty | No undemanded candidate | Continue/finish per consumer |
| Project | Declared demanded outputs | Per relational consumer demand | No undemanded candidate | Preserve Chapter-25 results |
| Limit satisfied | Safely unnecessary portion is no longer needed | Yes | No ordinary error from genuinely undemanded continuation | Early success, not cancellation |
| EXISTS satisfied | Specialized later work/projection values not demanded | Yes | Specialized suppression applies | §20.14 |
| Downstream early stop | Only safely unnecessary upstream portion ends | Yes | No new demand through scheduling | Required work elsewhere continues |
| Cursor close before exhaustion | Not specified as a Chapter-26 protocol | Result-owner matter | Do not invent policy | No separate question needed for this review |
| Cancellation | Normal successful continuation ends when observed | Yes | Resource/cancellation owner applies | QueryCancelled; unwind |
| Canonical terminal error | No further work needed except owner-required handling/cleanup | Yes | Candidate discovery alone may be insufficient | D25/D21/specialized owner |

The Limit rule does **not** justify stopping after K raw source rows in an ordered or blocking plan. Nor does it prove that every row beyond a physically observed prefix was semantically undemanded.

## 9. Borrowing and resource ownership — P/V

| Handoff | Producer owner | Borrow allowed? | Required stability | Reset/release point |
|---|---|---|---|---|
| Source output | Source/execution chunk owner | Yes | Entire reachable view | After consumers finish or gain independent ownership |
| Streaming pass-through | Upstream backing owner | Yes | Payload, validity, selection, representation | Not merely first `Execute` return |
| Project result | Borrowed input or result storage | Yes, where exact | Chapter-25 full result lifetime | After all relevant consumers |
| Filter pass-through | Input/reference/dictionary backing | Yes | Selected logical view | After dependent consumption |
| Synchronous sink input | Producer backing | Yes | Full sink consumption interval | After consumption completes |
| Retaining sink | Retained/new valid owner | Only with stable ownership | Across calls and retention interval | Receiving owner’s lifetime |
| Queued chunk | Valid retained owner | Not an expired naked producer borrow | Stable through queued execution | After queue consumers finish |
| Cursor-returned chunk | Cursor/result owner | Safely retained/materialized | §31.10 declared lifetime | Next `Next()`/destruction unless client copies |
| Failure unwind | Existing owner graph | No dangling use | Borrow-safe quiescence | End borrowers before releasing backing |
| Early stop | Same owner graph | Existing live views remain valid | No premature recycle | Cleanup without consuming unnecessary rows |

The synchronous interval is **the complete downstream consumption dependency**, not automatically a single immediate function return. Chapters 23 and 25 prohibit resetting backing while a downstream view remains live. No borrowing semantic question was found.

| Runtime resource | Owner | Accounting/cleanup |
|---|---|---|
| Local chunks/cursors/scratch | Worker/task execution state | Ch24 where execution-dependent capacity grows |
| Global build/finalized state | Operator execution state | Ch24, operator retention contract |
| Ready-task/dependency metadata | Query scheduler/runtime | Account unbounded execution-dependent growth |
| Queued/retained chunks | Live query/result owner | No unowned or unaccounted gap |
| Memory reservations | Query/operator ownership | Release ending ownership on success/error/cancel |
| Spill files/buffers | SpillManager/query owner | Ch24 cleanup; not recovered query state |
| Page guards | Execution access owner | Release safely; not through dangling views |
| ReadEpochGuard | Query context | Retain while execution may use protected RIDs |
| Transaction locks | Transaction owner | Not released independently by pipeline cleanup |
| Cursor-transferred memory | Result owner | May outlive pipeline with valid ownership/accounting |

## 10. Error propagation and precedence — Q/R

| Error/candidate | Origin | Chapter-26 role | Ranking owner | First physical wins? | Terminal/current output |
|---|---|---|---|---|---|
| Ordinary non-DML candidate | Demanded expression | Preserve/coordinate reduction | D25-S1 | No | Candidate alone need not terminate; failed result not successful |
| DML candidate | Eligible finalized-attempt work | Preserve required candidates/phase | D21-S4 | No | DML owner decides selected failure |
| Aggregate finalization error | Final aggregate states | Gate dependent output | §29.3.7 | No physical group/worker priority | No successful aggregate prefix |
| Scalar-subquery error | Side plan/consumer | Preserve specialized demand/precedence | §20.14.12 | Not a generic physical rule | Specialized terminal outcome |
| OutOfMemory | Allocation/hard gate | Propagate unchanged | Ch24/§39 | No new general ranking | Terminal failing invocation; no success output |
| Representability ExecutionError | Exact runtime domain | Propagate unchanged | Ch24/§39 | Same | Same |
| SpillIOError | Temporary storage | Preserve structured cause | Ch24/§39 | Same | Same |
| QueryCancelled | Cancellation observation | Stop/unwind | §39 | No invented cross-class order | Failed execution terminal |
| Corruption | Storage/source owner | Preserve fatal classification | Storage/§39 | Cannot downgrade | No ordinary continuation where fatal |
| Internal invalid state | Plan/runtime invariant | Safe rejection/quiescence | §39.1 | Not an ordinary candidate | No successful output/unsafe continuation |

Transaction consequences remain §39.1-owned in every row.

Multiple workers are permitted. Same-domain ordinary errors remain D25-S1- or D21-S4-owned across workers/pipelines. Specialized errors retain their scopes. Different legitimate resource/cancellation events do not acquire a new deterministic global hierarchy merely because execution is parallel. No additional cross-class semantic question was established.

Selected diagnostic origin, span, cause, DML phase, and required backing must survive propagation; M26-2 is the missing Chapter-26 bridge.

## 11. Cancellation, retry, and publication — S/T/U

### Cancellation

| Event | Existing contract | Assessment |
|---|---|---|
| Query cancellation requested | Query-wide flag/token | Clear |
| Long-running loop | Check at chunk or reasonable block boundaries | Cooperative observation; no fixed polling count |
| Dependency not ready | No runnable dependent before successful prerequisite | No `BLOCKED` enum required |
| Cancellation observed | Stop unnecessary tasks; running tasks reach cancellation points | Ch32 delegation |
| Resource unwind | Release query-owned guards/reservations/spill/state/read epoch safely | Clear |
| Transaction locks | Normal transaction terminal/abort path | Clear |
| Resume canceled execution | Not ordinary successful continuation of failed graph | §39.3; M26-1 local bridge |

### Retry/attempt state

| Outcome | Old semantic state reusable? | Source/candidate/result state | New attempt |
|---|---|---|---|
| Successful attempt | No automatic replay | End or transfer valid ownership | Not implied |
| Admitted pre-write RC retry | No stale semantic reuse | Discard/rebuild cursor, spool, candidates, output | Fresh snapshot; same statement CommandId |
| Ordinary semantic failure | No generic same-chunk retry | Terminal owner path | Only an independently authorized retry |
| Resource failure | No implicit replay | Cleanup ending resources | Same |
| Cancellation | No resumption as success | Quiesce and discard failed state | Same |
| Finalize failure | No successful dependency publication | Cleanup partial state | Owner-controlled only |
| Source failure | No rewind assumption | Preserve error, end failed execution | Owner-controlled only |
| Partial internal progress | Not itself restart permission | Must not duplicate accepted effects | Fresh-attempt rules apply |

Allocated capacity may be reused safely after reset; that is not permission to retain old attempt semantics.

### Result publication

| Event | Client-visible? | Prior delivery retracted? | Query successfully complete? | Owner |
|---|---|---|---|---|
| Internal operator output | Not by itself | N/A | No | Ch26 + Ch23/25 |
| Sink accepts chunk | Not necessarily | N/A | No | Sink/result owner |
| Cursor returns chunk | Yes | Delivered observation remains | Not implied | §31.10 |
| Later ordinary expression error | Error reported | No | No | D25-S1/§31.10 |
| Later resource failure | Failure reported | No retroactive delivery erasure or lifetime extension | No | Ch24/§39/result owner |
| Later cancellation | QueryCancelled path | Same distinction | No | §39/result owner |
| Successful cursor `FINISHED` | Completion outcome | Prior returns stand | Requires successful required work | Generic driver bridge M26-1 |
| Close before exhaustion | No Chapter-26 close protocol | Do not invent retraction | Do not infer successful exhaustion | Result/client owner |

DML completion is stronger: §31.9 withholds RETURNING through successful statement completion, and through the specified commit boundary for autocommit. Sink completion is not COMMIT.

## 12. Invalid states and determinism — W/X

| State | Classification | Safe boundary / assessment |
|---|---|---|
| Source terminal/output mismatch | Cannot fully classify until protocol chosen | Q26-1 |
| Streaming input/continuation mismatch | Cannot fully classify until protocol chosen | Q26-2 |
| Empty `HAVE_MORE` without advancement | Invalid pipeline/liveness behavior | Reject/prevent non-progress loop |
| Cardinality above capacity | Internal invalid representation | Before active access |
| Wrong schema/TypeId/column count | Internal invalid runtime/plan state | Before typed consumer access |
| Expired borrow | Internal lifetime violation | Before dereference |
| Reset while borrower depends on backing | Invalid absent exact preservation | Before destructive reuse |
| Uninitialized required state/output | Internal invariant violation | Before execution/publication |
| Failed invocation output treated as success | Invalid | No downstream successful consumption |
| Dependent uses missing/unfinalized state | Invalid dependency execution | Before dependent becomes runnable |
| Double Finalize | Generic invocation policy not frozen | No duplicate semantic effect; M26-1 clarification |
| Input/output after completed operator | Exact caller convention incomplete | Q26-1/Q26-2 |
| Required work after terminal query failure | Forbidden runnable graph | §39.3 |

| Variation | Values/NULL/bag/required order | Demand and canonical ordinary/DML winner | Resource feasibility | Completion/identity |
|---|---|---|---|---|
| Vector capacity | Preserve applicable semantics | Preserve | May differ | No size-derived identity |
| Chunk boundaries/short chunks | Preserve | Preserve | May differ | No cardinality-derived EOS |
| Empty batches | No extra rows/evaluations | No extra candidates | Operational work may differ | Must progress |
| Source/operator invocation count | No semantic repetition | No candidate leakage | May differ | Q26 protocols must conserve transfer |
| Pipeline schedule | Preserve required properties | Preserve owner rules | May differ | Dependency gates remain |
| Worker schedule | Bag may interleave if unordered | No physical winner | May differ | Required work cannot be omitted |
| Internal buffering | Preserve | Preserve | May differ | Valid retained ownership |
| Queue partition/layout | Same where queues are used | Same | May differ | No queue-position identity |
| Pointer/allocation addresses | No semantic effect | Never ranking keys | Allocation outcome separately owned | No slot/row identity |
| Runtime pipeline labels, if used | No SQL ordering | No winner key | None intrinsically | No persistent semantic identity |
| Resource allocation order | Matching successful semantics | Applicable candidate domain only | May differ | §39 outcome follows actual owned failure |

For unordered LIMIT, compare the allowed sub-bag contract—not an invented fixed physical row sequence.

## 13. Documentation, terminology, and references — Z

### Temporal audit

| Line | Phrase | Class | Assessment |
|---:|---|---|---|
| 20638 | “initialized IN-subquery” | A: runtime | Initialization state |
| 20642 | “first demand” | A: runtime | Lazy occurrence activation |
| 20652 | group “finish earlier” | A: runtime | Finalization order |
| 20672 | “Later blocking operators” | D: navigation | Read as downstream operator exposition; could be clearer |
| 20676 | “all future input” | A: runtime | Streaming versus blocking |
| 20784 | “first production version” | E: chronology | N26-1 |
| 20800 | “later scheduling parallelize” | E: chronology | N26-1 |
| 20817 | “current batch” | A: runtime | Borrow interval |
| 20820 | “later execution” | A: runtime | Unsafe queued borrow |
| 20852 | “Single-thread first” | E: chronology | N26-1 |
| 20854 | “first production executor” | E: chronology | N26-1 |
| 20861 | “later be partitioned” | E: chronology | N26-1 |
| 20878 | “Later parallel execution” | E: chronology | N26-1 |

**Project-chronology locations: 6**, consolidated into one finding.

| Document-role audit | Result |
|---|---|
| DEVELOPMENT sequencing | Present in N26-1 passages |
| Current implementation factual narration | None established; “first production” is sequencing, not evidence of implementation |
| VERIFICATION procedures | None |
| PROJECT_STATE leakage | None |
| Historical results/devlog narration | None |
| Timelessness | Incomplete until N26-1 |
| Exact C++ ABI overfreeze | None: interfaces are explicitly conceptual |
| Mandatory queue/container/lock/coroutine design | None |
| Normative strength | Strong where stated; missing handoff definitions are the issue |
| Analytical depth | Good for empty/EOS and borrowing; insufficient for consumption, completion, and canonical-error transport |

### Terminology dictionary

| Term | Live meaning / owner | Assessment |
|---|---|---|
| Pipeline | Source → zero or more streaming operators → sink | Clear |
| Dependency DAG | Blocking/finalization prerequisites | Clear |
| Source | Produces DataChunks | Status/output edge incomplete |
| Streaming operator | Transforms an input batch without all subsequent input | Consumption/continuation incomplete |
| Sink | Terminal consumer/accumulator | Conflated with breaker |
| Pipeline breaker | Finalized blocking state gates dependent work | Clear trait |
| Global state | Shared/finalized per-execution state | Ch22/26 |
| Local state | Per-worker/task hot mutable state | Ch22/26 |
| `HAVE_MORE` | Nonterminal source outcome, possibly empty with progress | Final-batch boundary incomplete |
| `FINISHED` | Explicit source completion | Output co-validity incomplete |
| EOS | Explicit completion, not empty cardinality | Clear distinction |
| `Finalize` | Successful state transition enabling dependencies | Generic driver lifecycle incomplete |
| Teardown | Ownership unwind, not semantic finalization | Owner-defined; missing local bridge |
| Early stop | Stop only safely unnecessary upstream work | Clear |
| Cancellation | Query-wide failure/termination mechanism | Clear |
| Synchronous consumption | Full value-stable downstream interval | Clear through Ch23/25 |
| `NEED_INPUT` / `BLOCKED` | Not defined Chapter-26 statuses | N/A |
| Pipeline ID | Not defined as an architectural identifier | N/A |

### Every explicit Chapter-26 cross-reference

| Source | Target | Purpose | Exists / owner-correct? | Classification |
|---|---|---|---|---|
| §26.1, line 20641 | Chapter 17 | Scalar control-flow demand | Yes | GOOD, broad |
| §26.1, line 20649 | §29.3 | Complete aggregate numerical validation | Yes | GOOD |
| §26.6, line 20806 | §23.10 | Value-stable borrowing | Yes | GOOD |
| §26.9, line 20865 | Chapter 32 | Scheduling/morsels/combining/parallel algorithms | Yes | GOOD |

No explicit Chapter-27 citation occurs in Chapter 26. No stale, wrong-owner, circular, or conflicting duplicate reference was found. Missing navigation is captured by the integration findings, not mislabeled as a stale citation.

## 14. Follow-up Verification coverage

This is a coverage cross-check, not synchronization or an atomic Chapter-26 closure count.

| Mechanism | Existing status | Reuse / missing work | Semantic blocker? |
|---|---|---|---|
| Full source status state machine | BLOCKED BY SEMANTIC QUESTION | V23-B source model needs final-output convention | Q26-1 |
| Empty batch versus EOS | COMPLETE | V23-B/K/M | No |
| Final nonempty output | BLOCKED BY SEMANTIC QUESTION | Add exact terminal-data cases after decision | Q26-1 |
| Source empty-output progress | COMPLETE | V23-B/M | No |
| Streaming continuation progress | BLOCKED BY SEMANTIC QUESTION | Add consumption/drain state model | Q26-2 |
| Sink acceptance/continuation | BLOCKED BY SEMANTIC QUESTION where resumable | Generic acknowledgment cases absent | Q26-2 |
| Input consumption/multi-output-per-input | PARTIAL / BLOCKED | Join occurrence oracle exists; generic protocol absent | Q26-2 |
| Completion/post-terminal invocation | PARTIAL / BLOCKED | Failure terminality covered; source/operator convention incomplete | Q26-1/Q26-2 |
| Blocking finalization readiness | COMPLETE | Blocking-state publication barriers | No |
| Root completion/required-finalizer bridge | PARTIAL | Add driver/result completion state model | M26-1 integration |
| Error/cancel teardown | COMPLETE | Pipeline cleanup, V24-M, Parallel Execution Tests | No |
| Cancellation observation | PARTIAL | Boundary cases exist; broaden long-loop driver composition | No new semantic question |
| Retry freshness | COMPLETE for owner contract; pipeline integration partial | V21-2, V24-M, V25-K/P | No |
| Synchronous borrowing | COMPLETE | V23-G/H/K, V25-L | No |
| Retaining-owner transfer | COMPLETE | Owner graph and reset cases | No |
| D25-S1 propagation through graph | PARTIAL | V25-I/J/P complete reduction; add driver transport | No |
| D21-S4 graph handoff | PARTIAL | V21-13/V25-K; add generic driver interaction | No |
| Limit/EXISTS early stop | COMPLETE specialized; generic composition partial | Scan/unary and subquery procedures | No |
| External prefix/lifetime | COMPLETE for frozen ordinary-error boundary | V25-O, V23 ownership | No |
| Invalid runtime states | PARTIAL | V22/V23/V25 cover data states; protocol states blocked | Q26-1/Q26-2 partly |
| Batching/worker determinism | COMPLETE semantic oracles; protocol composition pending | V23-L, V25-P, Parallel Execution Tests | Q26-1/Q26-2 partly |

The V23 example `(FINISHED, no batch)` is a valid fixture, **not Architecture authority proving that every `FINISHED` forbids a final batch**.

## 15. Technical consistency matrix — 300 questions

Status legend:

- **C — CONSISTENT**
- **S — CONSISTENT BUT SPECIALIZED**
- **F — FINDING**, with finding/question identifier
- **N — N/A**, absent concept or out-of-scope owner behavior

Questions 1–200 follow the implementer-invention checklist. Questions 201–300 add concrete checks against the live chapter and its actual handoffs.

### 1–50: Model, statuses, consumption, finalization

| # | Question | Determination | Status |
|---:|---|---|---|
| 1 | Exact chapter heading? | Pipeline Execution Model | C |
| 2 | What is a pipeline? | Source → streaming chain → sink | C |
| 3 | Who owns pipeline state? | Execution runtime; driver detail incomplete | F M26-1 |
| 4 | Which roles exist? | Source, streaming, sink/breaker | C |
| 5 | Which statuses exist? | Source HAVE_MORE and FINISHED | C |
| 6 | Which named status is terminal? | FINISHED denotes source completion | C |
| 7 | What means EOS? | Explicit source completion | C |
| 8 | Is cardinality zero EOS? | No | C |
| 9 | Can empty nonterminal batches exist? | Yes | C |
| 10 | Required empty-batch progress? | Finite source/operator state advancement | C |
| 11 | Can short source output be nonterminal? | Yes; cardinality is independent | S |
| 12 | Does a short chunk mean EOS? | No | S |
| 13 | Can FINISHED accompany nonempty output? | Not defined | F Q26-1 |
| 14 | Is such terminal output consumable? | Not defined | F Q26-1 |
| 15 | Must final data precede FINISHED? | Not defined | F Q26-1 |
| 16 | Can HAVE_MORE be empty? | Yes, with progress | C |
| 17 | What means NEED_INPUT? | No such defined status | N |
| 18 | Does NEED_INPUT prove consumption? | No defined protocol to inspect | N |
| 19 | Can HAVE_MORE retain operator input? | It is defined only for sources | F Q26-2 |
| 20 | Can one input produce multiple outputs? | Yes, join continuation | S |
| 21 | How long must pending input live? | Until no live dependency remains | S |
| 22 | When can producer reset output? | After consumption or exact preservation | C |
| 23 | What ends synchronous borrowing? | Completion of all dependent consumption | C |
| 24 | Can intermediate output borrow input? | Yes, value-stably | C |
| 25 | Can a sink retain input? | Yes | C |
| 26 | What must a retaining sink do? | Retain valid ownership or materialize | C |
| 27 | May queued work use recycled backing? | No | C |
| 28 | Are growing queues accounted? | Yes, through Ch24 owner regions | S |
| 29 | What counts as generic pipeline progress? | Source case explicit; continuation incomplete | F Q26-2 |
| 30 | Is generic no-progress continuation invalid? | Missing complete continuation contract | F Q26-2 |
| 31 | Can empty HAVE_MORE spin unchanged? | Explicitly forbidden | C |
| 32 | What permits BLOCKED status? | Status absent | N |
| 33 | Who reschedules BLOCKED returns? | No such return protocol | N |
| 34 | Is BLOCKED cancellation defined? | No status; dependency cancellation delegated | N |
| 35 | Can FINISHED transition back? | Post-terminal call convention absent | F Q26-1 |
| 36 | May finished operators receive input? | Generic terminal invocation convention absent | F Q26-2 |
| 37 | Can completed operators emit new output? | Final-output/continuation boundary incomplete | F Q26-1/Q26-2 |
| 38 | Is required Finalize necessary? | Yes for declared dependencies | C |
| 39 | Who invokes required Finalize? | Generic driver sequencing unstated | F M26-1 |
| 40 | Is Finalize called exactly once? | Call policy not universal; effects must remain exact | S |
| 41 | Can Finalize fail? | Yes; success is a readiness prerequisite | S |
| 42 | Can Finalize establish output state? | Yes; dependent source/state follows | C |
| 43 | Is Finalize distinct from cleanup? | Yes by owner semantics; bridge omitted | F M26-1 |
| 44 | Does cleanup follow Finalize failure? | Yes under Ch24/§39 | S |
| 45 | When is query success established? | Required-work/result-envelope bridge incomplete | F M26-1 |
| 46 | Must required finalization succeed first? | Yes; source EOS is insufficient | S |
| 47 | Who exposes cursor EOS? | Result/cursor owner | S |
| 48 | May EOS bypass required finalization? | No; local bridge missing | F M26-1 |
| 49 | What late error is valid after false success? | False success is not conforming | S |
| 50 | Can ordinary-error delivery retract prior chunks? | No | S |

### 51–100: Demand, errors, state, reuse

| # | Question | Determination | Status |
|---:|---|---|---|
| 51 | Does a returned prefix prove success? | No | S |
| 52 | What does client close-before-EOS mean? | Not a Chapter-26 protocol | N |
| 53 | Can Limit stop upstream early? | Yes, when safely unnecessary | C |
| 54 | Is Limit early stop cancellation? | No | C |
| 55 | Can genuinely undemanded later source errors surface? | No ordinary semantic candidate | S |
| 56 | May scheduler create extra semantic demand? | No | S |
| 57 | Can speculative undemanded errors become public? | No ordinary candidate | S |
| 58 | Does Ch26 own safe upstream stopping? | Yes, §26.8 | C |
| 59 | Must stopping preserve D20-B1? | Yes | S |
| 60 | Are pipeline stopping and expression masks identical? | No; they compose | S |
| 61 | Who selects ordinary non-DML errors? | D25-S1 | S |
| 62 | May first physical discovery win by itself? | No | S |
| 63 | May first chunk determine the winner? | No | S |
| 64 | May first worker determine the winner? | No | S |
| 65 | May candidate collection stop too early? | Not before owner permits termination | S |
| 66 | How do DML candidates cross pipelines? | Required owner transport bridge missing | F M26-2 |
| 67 | May first physical DML candidate force abandonment? | Not if required candidates are lost | S |
| 68 | Does D21-S4 retain ranking ownership? | Yes | S |
| 69 | Who owns resource errors? | Ch24/§39 | S |
| 70 | Is semantic-versus-resource precedence universal? | No | S |
| 71 | Is cancellation given a new global priority? | No | S |
| 72 | Who arbitrates concurrent same-domain errors? | Existing semantic owner | S |
| 73 | Who coordinates parallel reduction? | Runtime/scheduler under semantic owner | F M26-2 |
| 74 | May scheduling alter an ordinary minimum? | No | S |
| 75 | Is v1 categorically single-threaded? | No; Ch32 permits workers | S |
| 76 | Is “first production” durable scope wording? | No; chronology | F N26-1 |
| 77 | What state is per execution? | Context and global execution state | S |
| 78 | What state is attempt-local? | Discardable cursors/candidates/spools/side state | S |
| 79 | What is per pipeline? | Graph/state references; lifecycle detail incomplete | F M26-1 |
| 80 | What is per operator? | Declared local/global runtime state | C |
| 81 | What is per worker/task? | Hot mutable local state | C |
| 82 | May attempt semantics leak across retries? | No | S |
| 83 | What happens on admitted retry? | Discard old attempt; rebuild fresh semantics | S |
| 84 | May terminal failed pipeline resume normally? | No runnable failed graph | S |
| 85 | May stale source position survive retry? | Not as the new attempt’s semantic cursor | S |
| 86 | Is fresh attempt state required? | Yes; capacity reuse is distinct | S |
| 87 | What happens to live borrows on failure? | Quiesce before backing destruction | S |
| 88 | What happens to reservations? | Ending ownership releases them | S |
| 89 | What happens to spill resources? | Ch24 cleanup | S |
| 90 | What happens to pending task/queue state? | Quiesce and release ending ownership | S |
| 91 | Does cancellation clean query state? | Yes | C |
| 92 | Does successful completion clean ending state? | Yes; transferred ownership may survive | S |
| 93 | Can result memory outlive pipelines? | Yes under result ownership | S |
| 94 | Is one transfer mechanism required? | No; exact ownership is required | S |
| 95 | Is runtime pipeline state persistent? | No | S |
| 96 | Is a pipeline ID defined? | No architectural identifier specified | N |
| 97 | Do chunk boundaries change semantics? | No legitimate semantic change | S |
| 98 | Does capacity establish SQL meaning? | No | S |
| 99 | Do extra empty calls create scalar occurrences? | No | S |
| 100 | May worker interleaving change required results? | No; unordered presentation may vary | S |

### 101–150: Validity, completion, dependencies

| # | Question | Determination | Status |
|---:|---|---|---|
| 101 | Does buffering establish SQL order? | No | S |
| 102 | Does scan traversal establish SQL order? | No | S |
| 103 | Who owns required order? | Logical/physical ordering owners | S |
| 104 | May framework duplicate occurrences? | No | S |
| 105 | May framework drop required occurrences? | No | S |
| 106 | May framework deduplicate equal values? | No | S |
| 107 | How is partial consumption represented? | Not specified | F Q26-2 |
| 108 | Is partial sink acceptance communicated? | No generic acknowledgment specified | F Q26-2 |
| 109 | Is replay after partial failure generally safe? | No implicit replay permission | S |
| 110 | Could blind replay duplicate DML effects? | Yes; prohibited by statement owners | S |
| 111 | Are all dynamic protocol-invalid states classified? | Not until handoff contract is complete | F Q26-1/Q26-2 |
| 112 | Is wrong input/output schema invalid? | Yes, internal | S |
| 113 | Is cardinality above capacity invalid? | Yes, internal | S |
| 114 | Is expired borrowing invalid? | Yes, before dereference | S |
| 115 | Can status/output mismatch be fully checked? | Not yet defined | F Q26-1 |
| 116 | Is every double Finalize necessarily invalid? | Universal call policy not specified | S |
| 117 | Is failed output successful output? | No | S |
| 118 | Are malformed runtime states ordinary SQL errors? | No | S |
| 119 | Are they internal invariants? | Yes, once the precondition is defined | S |
| 120 | Must unsafe access be prevented? | Yes | S |
| 121 | Must output match its physical schema? | Yes | S |
| 122 | May schema drift between chunks? | Not for the fixed validated operator | S |
| 123 | Is capacity exactly 1024 mandatory? | No | S |
| 124 | Are legal capacities 1–65535 preserved? | Yes | S |
| 125 | Are partial chunks legal? | Yes | S |
| 126 | Is executable empty input legal? | Yes | S |
| 127 | Must every empty batch invoke every operator? | No fixed invocation strategy; preserve semantics | S |
| 128 | Can empty per-row demand raise runtime scalar errors? | No | S |
| 129 | Can empty input advance legitimate state? | Yes where the operator owner permits | S |
| 130 | Is post-source-completion behavior complete? | Repeated-call/final-data convention missing | F Q26-1 |
| 131 | Is generic pipeline terminality fully described? | Lifecycle bridge incomplete | F M26-1 |
| 132 | Can terminal failure later become normal success? | No | S |
| 133 | Can canceled failed execution simply be revived? | No; fresh execution is distinct | S |
| 134 | May success omit required work? | No | S |
| 135 | May required background work invalidate earlier success? | Success cannot precede required completion | S |
| 136 | Is a late-success-invalidating protocol defined? | No such authorized protocol | N |
| 137 | Are dependencies explicit? | Yes, DAG | C |
| 138 | Can consumers use unfinalized prerequisites? | No | C |
| 139 | What if a prerequisite fails? | Dependent cannot become runnable | C |
| 140 | Can a valid dependency graph contain a cycle? | No; DAG required | C |
| 141 | Does fairness have an owner? | Ch32 task/yield boundaries | S |
| 142 | Who owns local/global merge meaning? | Concrete operator, scheduled under Ch32 | S |
| 143 | Must contributions be incorporated exactly? | Yes; call count itself is not universal | S |
| 144 | Is every Combine commutative? | No generic assertion | S |
| 145 | Who owns aggregate merge laws? | §29.3 | S |
| 146 | How are source errors propagated? | Preserve canonical cause; local bridge omitted | F M26-2 |
| 147 | How are sink errors propagated? | Same | F M26-2 |
| 148 | How are expression candidates propagated? | D25/D21 owner transport required | F M26-2 |
| 149 | May corruption become generic ExecutionError? | No | S |
| 150 | May OOM lose its category? | No | S |

### 151–200: Errors, persistence, document model

| # | Question | Determination | Status |
|---:|---|---|---|
| 151 | Who classifies runtime representability failure? | Ch24/§39 | S |
| 152 | Who classifies SpillIOError? | Ch24/§39 | S |
| 153 | Who classifies cancellation? | §39 | S |
| 154 | Who owns invariant-failure consequences? | §39.1 | S |
| 155 | May wrappers erase canonical category? | No | S |
| 156 | Must responsible SourceSpan survive? | Yes | S |
| 157 | Must D25 conceptual cause survive? | Yes | S |
| 158 | Must DML expression phase survive? | Yes | S |
| 159 | Must diagnostic backing survive reporting? | Yes; transport handoff missing locally | F M26-2 |
| 160 | What happens to current failing output? | Not successful output | S |
| 161 | May incomplete active values reach downstream? | No | S |
| 162 | What about an earlier client-visible chunk? | No ordinary-error retraction | S |
| 163 | Is internal production external publication? | No | S |
| 164 | Does sink acceptance imply client visibility? | No | S |
| 165 | Who owns external visibility? | Result/cursor or DML envelope owner | S |
| 166 | When is cursor chunk lifetime established? | On return under §31.10 | S |
| 167 | May teardown invalidate a still-valid returned chunk? | No | S |
| 168 | Does Ch31 own retained result backing? | Yes | S |
| 169 | May scan output retain expired page pointers? | No | S |
| 170 | Is page-to-chunk VARCHAR copying preserved? | Yes | S |
| 171 | Must queued string views remain stable? | Yes | S |
| 172 | Is growing pipeline scratch accounted? | Yes | S |
| 173 | Are growing queued chunks accounted? | Yes | S |
| 174 | Are growing worker-local queues accounted? | Yes | S |
| 175 | Must pressure retry respect D24-S3? | Yes | S |
| 176 | Can denied allocation retry unchanged forever? | No | S |
| 177 | Is resource progress enough to define streaming continuation? | No; distinct protocol gap | F Q26-2 |
| 178 | Is repeated irrelevant state change sufficient progress? | No for pressure; generic continuation needs definition | F Q26-2 |
| 179 | Is general continuation well-foundedness explicit? | Not in generic Execute contract | F Q26-2 |
| 180 | Is query pipeline state crash-recoverable? | No | S |
| 181 | Is “not recovered” the required result? | Yes | S |
| 182 | Are spill leftovers temporary resources? | Yes | S |
| 183 | Does transaction recovery depend on pipeline state? | No | S |
| 184 | Is that absence required? | Yes | S |
| 185 | Does DML pipeline completion commit? | No | S |
| 186 | Is “not independently COMMIT” required? | Yes | S |
| 187 | Who owns autocommit envelope? | Transaction/statement owners | S |
| 188 | May query success precede required DML publication work? | No | S |
| 189 | Can SELECT chunks precede final query success? | Yes | S |
| 190 | Is cursor FINISHED an external completion outcome? | Yes; driver bridge incomplete | F M26-1 |
| 191 | May failure masquerade as successful EOS? | No | S |
| 192 | Are all actual invocation transitions defined? | No | F Q26-1/Q26-2 |
| 193 | Can unresolved transfer convention affect multiplicity? | Yes | F Q26-1/Q26-2 |
| 194 | Is canonical error transport locally explicit? | No | F M26-2 |
| 195 | Is consumption-completion acknowledgment explicit? | No | F Q26-2 |
| 196 | Is continuation liveness fully specified? | No | F Q26-2 |
| 197 | Can this stand unchanged as timeless canonical Architecture? | Not yet | F N26-1/B26-1/B26-2 |
| 198 | Does it narrate actual current implementation facts? | No such factual claim established | C |
| 199 | Does it contain development sequencing? | Yes | F N26-1 |
| 200 | Does it leak Verification recipes? | No | C |

### 201–220: Graph and role-specific checks

| # | Question | Determination | Status |
|---:|---|---|---|
| 201 | May a pipeline have zero streaming operators? | Yes | C |
| 202 | Is the example SeqScan→Filter→Project→ResultSink explicit? | Yes | C |
| 203 | Is the execution graph built after physical-plan finalization? | Yes | C |
| 204 | May graph construction mutate semantic plan meaning? | No | C |
| 205 | Does the immutable operator tree remain available for EXPLAIN? | Yes | C |
| 206 | Are all dormant side plans eagerly scheduled? | Forbidden | C |
| 207 | Is side-plan activation tied to semantic occurrence demand? | Yes | C |
| 208 | May an outer pipeline suspend for a demanded side plan? | Yes | C |
| 209 | Must every required predecessor finalize successfully? | Yes | C |
| 210 | Can one finished aggregate partition publish early? | No | C |
| 211 | Does aggregate readiness include every group’s numerical validation? | Yes | C |
| 212 | Does hash probe wait for build Finalize? | Yes | C |
| 213 | Does sort output wait for input/run Finalize? | Yes | C |
| 214 | Does DML write wait for target-spool Finalize? | Yes | C |
| 215 | Does initialized IN probe wait for lazy build Finalize? | Yes | C |
| 216 | Does each breaker declare memory-resident state? | Yes | C |
| 217 | Does each breaker declare spillability? | Yes | C |
| 218 | Does each breaker declare post-finalize output/source state? | Yes | C |
| 219 | Does every sink necessarily expose a dependent source? | Terminology obscures the distinction | F N26-2 |
| 220 | Is recursive row-at-a-time Next mandatory? | No | C |

### 221–240: Runtime state and borrowing

| # | Question | Determination | Status |
|---:|---|---|---|
| 221 | Are C++ virtual/template mechanics prescribed? | No | C |
| 222 | Is semantic state/lifetime separation optional? | No | C |
| 223 | Are source cursors hot local-state examples? | Yes | C |
| 224 | Are reusable chunks local-state examples? | Yes | C |
| 225 | Are expression scratch and local buffers execution-owned? | Yes | C |
| 226 | Are continuation counters explicitly anticipated? | Yes | C |
| 227 | Is an implicit thread-local singleton a correctness dependency? | Forbidden | C |
| 228 | Can global state contain finalized shared structures? | Yes where required | C |
| 229 | Is mutable query state allowed inside immutable plans? | No | C |
| 230 | Is source partitionability conditioned on validity? | Yes; wording is chronological | F N26-1 |
| 231 | Does owner allocation alone validate a borrow? | No | C |
| 232 | Must reachable validity remain stable? | Yes | S |
| 233 | Must reachable selection entries remain stable? | Yes | S |
| 234 | Must dictionary child relationships remain stable? | Yes | S |
| 235 | Must StringRef length/prefix/data remain stable? | Yes | S |
| 236 | Must referenced VARCHAR bytes remain stable? | Yes | S |
| 237 | May unrelated unreachable inactive storage change? | Yes | S |
| 238 | Does immediate consumer return necessarily end transitive borrowing? | No | S |
| 239 | Can exact ownership transfer preserve zero-copy retention? | Yes | S |
| 240 | Must all retained results be deep-copied universally? | No | S |

### 241–260: Cancellation, errors, demand

| # | Question | Determination | Status |
|---:|---|---|---|
| 241 | Is the cancellation token query-wide? | Yes | C |
| 242 | Are long-loop cancellation checks required? | Yes | C |
| 243 | Is a fixed millisecond polling interval specified? | No | C |
| 244 | Are page guards included in cancellation unwind? | Yes | C |
| 245 | Is the read-epoch guard included? | Yes, subject to safe lifetime | C |
| 246 | Are local and global states both unwound? | Yes | C |
| 247 | May pipeline cancellation release transaction locks directly? | No | C |
| 248 | May early stop report QueryCancelled? | No | C |
| 249 | May early stop abort the transaction? | No | C |
| 250 | May early stop cancel required side-effect work elsewhere? | No | C |
| 251 | Does early stop authorize arbitrary raw-source truncation? | No | S |
| 252 | Does an empty Project demand initialize a side plan? | No | S |
| 253 | Does skipped AND/OR side work become a candidate? | No | S |
| 254 | Is D25’s source-span/cause preorder unchanged? | Yes | S |
| 255 | Is DML expression phase replaced by pipeline stage? | No | S |
| 256 | Is aggregate ordinal replaced by pipeline ID? | No | S |
| 257 | Is scalar-subquery cardinality replaced by first-row behavior? | No | S |
| 258 | Can a source’s pushed predicate produce expression candidates? | Yes under assigned predicate/demand owners | S |
| 259 | May first terminal callback erase a needed smaller candidate? | No; bridge needed | F M26-2 |
| 260 | Must fatal cleanup preserve original causal diagnostics? | Yes under §39.1.7 | S |

### 261–280: Resource, publication, validity

| # | Question | Determination | Status |
|---:|---|---|---|
| 261 | Does sink Finalize imply transaction commit? | No | S |
| 262 | Can RETURNING stream while its attempt may fail? | No | S |
| 263 | Does autocommit RETURNING await its commit envelope? | Yes | S |
| 264 | Does internal spool construction alone establish statement success? | No | S |
| 265 | Can returned SELECT chunks be retained by client copying? | Yes | S |
| 266 | Does an old chunk automatically remain valid after next Next()? | No | S |
| 267 | Can candidate selection validate a partial expression vector? | No | S |
| 268 | Does OOM during output growth permit partial success? | No | S |
| 269 | Does unsupported exact representation become ArithmeticError? | No | S |
| 270 | Can spill failure be silently converted into OOM? | No | S |
| 271 | Can pressure handling approximate values to progress? | No | S |
| 272 | Does each denied-request retry need relevant changed state? | Yes | S |
| 273 | Is writing useless spill data sufficient pressure progress? | No | S |
| 274 | Are exact runtime lengths checked before pointer/range use? | Yes | S |
| 275 | Can many small task allocations bypass accounting? | No | S |
| 276 | Is parent-region accounting allowed without duplicate charges? | Yes | S |
| 277 | Can a zero-capacity bookkeeping object be emitted as a batch? | No | S |
| 278 | Can selection address merely allocated inactive capacity? | No | S |
| 279 | May ordinary operator handoff reinterpret schema names? | No runtime rebinding | S |
| 280 | Does final-plan validation replace dynamic borrow checks? | No | S |

### 281–300: Parallelism, conservation, documentation

| # | Question | Determination | Status |
|---:|---|---|---|
| 281 | Is arbitrary OS-thread creation required per pipeline? | No; delegated configured pool | S |
| 282 | Do parallel read workers share the effective snapshot? | Yes | S |
| 283 | Do parallel read workers share CommandId semantics? | Yes | S |
| 284 | Does parallel SeqScan advertise SQL ordering? | No | S |
| 285 | Must required ordering be enforced by a capable plan? | Yes | S |
| 286 | May finalized hash-probe state be mutated by ordinary probes? | No | S |
| 287 | May local aggregate state use inexact physical-order subtotals? | No | S |
| 288 | Can arbitrary worker order emit required sorted output? | No; order-preserving merge/source required | S |
| 289 | Does successful predecessor Finalize control task readiness? | Yes | S |
| 290 | Is lock-free work stealing mandatory? | No | S |
| 291 | Does cancellation prevent new unnecessary tasks? | Yes | S |
| 292 | Is one scheduler fairness algorithm frozen? | No; control boundaries are delegated | S |
| 293 | May source batch reshaping duplicate a visible base row? | No | S |
| 294 | May generic scheduling collapse equal Values rows? | No | S |
| 295 | May empty Filter output terminate an unfinished source? | No | S |
| 296 | Does Chapter 26 explicitly cite Chapter 27? | No | C |
| 297 | Are all four explicit cross-reference targets live? | Yes | C |
| 298 | Does Chapter 26 prescribe a benchmark or fault harness? | No | C |
| 299 | Are six production-sequencing locations timeless? | No | F N26-1 |
| 300 | Can protocol conformance be completed without Q26 decisions? | No | F Q26-1/Q26-2 |

## 16. Regression and final ambiguity assessment

| Area | Result |
|---|---|
| Ch20 bag/order/demand/Limit | No contradiction; generic scheduling must preserve them |
| Ch21 attempts/retry/DML ranking/publication | No contradiction; lifecycle/error bridges incomplete |
| Ch22 plan/runtime ownership | Preserved |
| Ch23 capacity/empty/borrow/reset | Preserved; terminal-data pairing remains Ch26 question |
| Ch24 accounting/pressure/cleanup | Preserved |
| Ch25 demand/results/D25/DML/nonpublication/borrowing | Preserved; no reopening required |
| Empty-as-EOS ambiguity | **No** |
| Short-chunk-as-EOS ambiguity | **No** |
| Final nonempty terminal output ambiguity | **Yes — Q26-1** |
| Input-consumption/multi-output continuation ambiguity | **Yes — Q26-2** |
| Source empty-no-progress ambiguity | **No** |
| General continuation-progress incompleteness | **Yes — Q26-2** |
| Finalization/query-success bridge | **Integration gap — M26-1** |
| Error/candidate propagation bridge | **Integration gap — M26-2** |
| Value-stable borrow/retaining-owner ambiguity | **No new semantic question** |
| First-physical-error permission | **No; prohibited by existing owners** |
| Cancellation/early-stop confusion | **No** |
| Fresh-attempt semantics | **Determinate upstream; local integration incomplete** |
| Undemanded upstream ordinary errors | **Cannot become candidates** |
| Multiple-worker global error hierarchy | **No new hierarchy justified or required by this review** |
| Invalid protocol combinations | **Partly blocked by Q26-1/Q26-2** |
| Correctness-relevant implementer invention | **Yes, at the two protocol boundaries** |
| Can Chapter 26 stand unchanged years later? | **Not yet** |

## 17. Next action and gates

Next task: **FROZEN CHAPTER-26 ARCHITECTURE SEMANTIC REVIEW / DECISION PACKAGE**, resolving Q26-1 and Q26-2 without changing frozen upstream semantics.

Do not perform document cleanup or Verification synchronization first. After approved integration, address M26-1/M26-2 and N26-1/N26-2, then synchronize Chapter-26 Verification.

Chapter 26 is **not clean and not fully closed**.

The eventual Chapter-27 review should cover its complete concrete scan/unary contract, after Chapter 26 closes. Only its immediate source, Filter, Project, Limit, and ResultSink handoffs were consulted here.

Confirmed:

- Chapter 27 review: **NOT STARTED**.
- Verification synchronization: **NOT PERFORMED**.
- Files modified/generated: **NONE**.
- Implementation, build, tests, sanitizers, benchmarks: **NONE**.
- Staging, commit, devlog, review artifacts: **NONE**.
- Phase 2: **NOT STARTED / NOT AUTHORIZED**.

# Q26-1 / Q26-2 — DECISION PACKAGE COMPLETE

Recommend two complementary decisions, pending Architecture-owner approval:

- **D26-S1:** Deliver output before terminal completion. A terminal outcome carries no consumable output.
- **D26-S2:** Accept each streaming input once; expose continuation, input-backing release, and new-input readiness as distinct protocol facts.

No additional frozen semantic question was found. These are recommendations—not accepted or integrated Architecture.

## 1. Repository state and scope

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | Clean |
| Index | Clean | Clean |
| HEAD | `1bc08624f9a1e70ee90d8f376456d2658b511c4a` | Same |
| `git diff --check` | — | Passed |
| Task-created changes | None | None |

HEAD differs from the preceding review’s reported HEAD, but did not change during this task. Historical review artifacts remained unread, unmodified, unmoved, and unstaged.

Read Chapter 26 completely: [§26.1–§26.10](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20606), lines **20606–20881**. Primary decision surface: [§26.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20733), with §§26.5–26.8 for state, borrowing, cancellation, and early stop.

Context consulted:

| Owner | Sections |
|---|---|
| Documentation authority | AGENTS.md; Architecture front matter |
| Logical semantics | §§20.4–20.7, 20.11–20.12, 20.14.4–20.14.8, 20.17, 20.17.5 |
| DML | §§21.13–21.16.1 |
| Physical/runtime ownership | §§22.1–22.3, 22.5–22.8; §38.24 validation handoff |
| Chunks and borrowing | §23.1; §§23.10–23.13 |
| Resources | §§24.4–24.6, 24.10 |
| Expressions | Chapter 25 completely |
| Immediate unary handoff | §§27.1–27.2, 27.7–27.11 |
| Join continuation | §§28.3, 28.7–28.8 |
| Aggregate finalization | §§29.2, 29.3.7 |
| Sort handoff | §30.1 |
| Retry/results | §§31.5, 31.9–31.10 |
| Workers/dependencies | §§32.1–32.9, context only |
| Failure ownership | §§39.1.1, 39.1.3–39.1.4, 39.1.7, 39.3 |

Verification consultation covered V23-B; V25-I–L and N–P; Pipeline Finalization and Resource Tests; Parallel Execution Tests; Scan and Unary Operator Tests; and the join multi-output coverage. No Verification synchronization or Chapter-27 review occurred.

## 2. Q26-1: output versus terminal completion

### Existing ambiguity

[§26.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20733) names `HAVE_MORE` and `FINISHED` and explicitly separates empty chunks from EOS. It does not say whether `FINISHED` can accompany consumable final data.

Both separate-event and combined-event designs can preserve SQL semantics. The defect is the missing shared producer/driver interpretation, not that combined completion is inherently unsafe.

### Alternatives

The table is transposed to keep every comparison readable.

| Criterion | A: output, then FINISHED | B: FINISHED can carry output | C: explicit orthogonal dimensions | D: recommended uniform A plus explicit readiness |
|---|---|---|---|---|
| Final-row safety | Exact with defined ordering | Exact if final data consumed first | Exact if both facts always exposed | Exact |
| Empty/EOS clarity | Strong distinction | Requires output-presence distinction | Explicit, including empty-present versus absent | Strong distinction |
| Driver simplicity | Separate data/completion handling | Combined-case handling everywhere | More legal combinations | Separate handling plus continuation/readiness facts |
| Extra invocation | Usually one terminal probe | Not inherently | Depends on transition | One extra logical transition; not necessarily a separate C++ call |
| Post-terminal clarity | Must specify | Must specify | Must specify | Driver makes no further data request |
| Borrowing simplicity | Consume/preserve output before completion | Terminal must not trigger premature teardown | Lifetime independent of terminal bit | Same clear consumption boundary |
| Operator uniformity | Can apply to sources and streaming operators | Can apply uniformly | Can apply uniformly | Uniform source/streaming convention |
| Cursor composition | Naturally output then completion | Adapter must deliver final batch before EOS | Explicit adapter required | Local completion remains separate from cursor EOS |
| Implementation freedom | Encoding free; conceptual events fixed | Encoding free; combined case supported | Greatest transition freedom | Encoding, callbacks, and scheduling remain free |
| Proof burden | Relatively small | Must prove both terminal cases | Must prove combinations and adapter mapping | Small terminal matrix; explicit early-stop gate |
| Recommendation | Sound foundation | Sound but unnecessary additional case | Sound only with mandatory explicit facts | **Recommended** |

Alternative C would resolve interoperability if every boundary explicitly supplied both output availability and terminality. Merely allowing either A or B without those facts would not.

I recommend A’s conceptual separation uniformly across sources and streaming operators, supplemented by D26-S2’s explicit readiness information. This reduces the number of legal terminal/output combinations without fixing an ABI.

### Exact recommended source behavior

| Source case | Conceptual outcome | Consumable output? | Local EOS? | Progress | Caller action | Legal? |
|---|---|---:|---:|---|---|---:|
| Empty source on first call | FINISHED | No | Yes | Terminal transition | Stop requesting data | Yes |
| Nonfinal nonempty batch | HAVE_MORE/output available | Yes | No | New output | Consume, then request more if demanded | Yes |
| Short nonfinal batch | Same | Yes | No | New output | Same; do not infer exhaustion | Yes |
| Empty nonterminal batch | HAVE_MORE/output available | Yes, zero occurrences | No | Finite relevant state must advance | Continue if demanded | Yes |
| Final nonempty batch | HAVE_MORE/output available | Yes | No | Final occurrences delivered | Consume/preserve, then obtain terminal outcome | Yes |
| Terminal observation | FINISHED | No | Yes | Completion established | No more data requests | Yes |
| Data request after FINISHED | Outside valid invocation domain | No new output permitted | Already terminal | None required | Prevent/reject misuse | No |

The final sequence is:

```text
final output-available transition
    → downstream consumption or valid ownership handoff
    → terminal observation with no consumable output
```

For a pull source, this normally means call N returns final data and call N+1 returns `FINISHED`. If exhaustion is already known, the latter is a control transition—not permission to perform additional row evaluation.

If execution is canceled, fails, or legitimately stops upstream early, cleanup does not require issuing an otherwise unnecessary terminal probe.

Important consequences:

- `HAVE_MORE` does not promise another nonempty batch.
- Neither zero nor short cardinality indicates completion.
- A terminal invocation may receive reusable output storage, but that storage contains **no consumable result for that invocation**.
- Drivers MUST NOT request more data after terminal completion.
- Repeated terminal calls need not be an architectural idempotent operation. They are outside the valid driver protocol; defensive handling remains implementation-specific.
- No post-terminal call can restart the state or emit another occurrence.
- Source FINISHED is local completion, not whole-query success or Chapter-31 cursor EOS.

The extra transition retains only required runtime state and respects existing borrowing, cancellation, resource, blocking-finalization, and parallel-source ownership.

## 3. Q26-2: acceptance, continuation, release, and readiness

### Existing ambiguity

The conceptual `Execute(input, output, LocalOperatorState)` interface lacks a consumption/continuation acknowledgment. Meanwhile, [§28.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21387) explicitly requires one probe chunk to produce multiple output chunks.

An independently implemented driver cannot presently determine whether to resume that input, replace it, or drain output already buffered independently.

### Recommended vocabulary

| Concept | Exact recommended meaning |
|---|---|
| Input acceptance | The operator takes responsibility for processing one submitted logical-occurrence domain once. Passing the same buffer again for continuation does not constitute new acceptance. |
| Input consumption | Processing/ingestion of the submitted occurrences according to the owning operator. It need not coincide with delivery of all resulting output. |
| Accepted-input lifecycle resolution | All required work and output delivery attributable to that acceptance have been discharged, or the remaining work has become safely unnecessary under an owning early-stop rule. |
| Operator release acknowledgment | The operator no longer requires the caller to preserve the original input backing. Required continuation state is either finished or independently preserved. |
| Pending continuation | Required processing, not-yet-offered output, or a pending terminal-control transition remains before new-input admission. |
| Output availability | This transition offers a valid active output domain, possibly empty. Availability is explicit, not inferred from cardinality. |
| New-input readiness | The previous acceptance is resolved, no continuation must precede another input, the current output handoff is complete, and neither terminal completion nor an established early stop forbids new input. |
| Physical reset permission | No operator dependency and no downstream borrow still require the original backing, unless an exact ownership mechanism independently preserves every required view. |

A crucial refinement to the suggested wording is necessary:

**Copying input can finish ingestion and release caller backing while the accepted-input lifecycle remains unresolved because required output is still pending.**

Thus:

```text
input backing releasable
    does not imply
accepted-input lifecycle resolved
    does not by itself imply
physical backing reset permitted
```

An already-offered final batch may still be undergoing downstream consumption after its input lifecycle resolves. That remaining borrow is separate from *not-yet-offered* continuation output.

This distinction avoids calling an input “resolved” while still requiring the driver to drain undisclosed work from it.

### Alternatives

Here “safe” assumes an explicit acknowledgment contract.

| Alternative | Multi-output | Borrow safety | Duplicate-input prevention | Lost-output prevention | Buffering | Zero-copy continuation |
|---|---|---|---|---|---|---|
| A. Execute fully consumes before return | Requires buffering or emission during call | Safe with lifetime rule | Return can acknowledge acceptance | Needs complete emission/drain contract | Yes | Only within call unless ownership retained |
| B. Retained-input continuation | Yes | Requires stable retained dependency | Explicit resume required | Explicit drain required | Optional | Yes |
| C. Consume and buffer | Yes | Caller can release after preservation | Explicit acceptance required | Buffered output must be drained | Yes | Not required |
| D. Explicit lifecycle facts | Yes | Dependencies explicit | Acceptance distinct from resume | Pending work explicit | Yes | Yes |
| E. Mandatory callback/push | Yes | Natural synchronous interval | One submission | Every callback output tracked | Optional | Yes |
| F. D plus one outstanding acceptance per local state | Yes | Explicit release plus downstream lifetime | Unambiguous | Drain before next admission | Yes | Yes |

| Alternative | Parallel local states | Progress definable | Implementation freedom | Requires new enum? | Recommendation |
|---|---|---|---|---|---|
| A | Yes | Yes | Restricts suspension/return behavior | No | Not mandatory |
| B | Yes | Yes | Unnecessarily fixes backing strategy alone | No | Allowed realization |
| C | Yes | Yes | Unnecessarily fixes buffering alone | No | Allowed realization |
| D | Yes | Yes | High | No | Foundation |
| E | Yes | Yes | Forces execution style | No | Allowed realization, not required |
| F | Yes | Yes | High; generic per-state admission serialized | No | **Recommended** |

The generic driver admits one accepted-input lifecycle at a time per `LocalOperatorState`. It drains that lifecycle before admitting another input, even when copying has already made the original backing releasable.

This does not require one query worker, prohibit separate local states, or forbid buffering. It does not introduce a generic multi-input queue contract.

### Compatibility with actual operators

- **Filter:** Rejecting every row resolves its input with no output occurrences. It may offer a valid empty batch or report completion without manufacturing a batch.
- **Project:** Offers its output and resolves processing; borrowed result vectors can still delay backing reset.
- **Join probe:** Accepts input once and resumes the stored probe/build-chain position until required matches are offered exactly once.
- **Buffered continuation:** Releases the original input dependency after exact preservation, then drains independently owned output before new-input admission.
- **Limit:** May close the input lifecycle by making its remainder safely unnecessary. Its final output is delivered before terminal completion.
- **Callback execution:** Can implement the same logical transitions within one outer call. No callback-only design is mandated.

Buffering remains subject to semantic demand. Copying data or precomputing output is not permission to expose errors from work the semantic owner does not demand. Nor does the proposal require materializing an entire join cross product; §28.3’s continuation capability remains intact.

## 4. Combined protocol

### Legal-state constraints

These constraints define the combinations; they are not required enum or struct fields:

1. Consumable output and observed terminal completion are mutually exclusive.
2. New-input readiness excludes unresolved accepted input and pending continuation.
3. Caller-backing release can precede lifecycle resolution.
4. Lifecycle resolution can precede downstream borrow completion.
5. Established early stop immediately prevents new-input readiness, even while final output awaits consumption.
6. Continuation resumes existing acceptance, never accepts the same input again.
7. A pending terminal-control transition takes no new upstream input.
8. Terminal failure/cancellation is not successful FINISHED.
9. No required output occurrence is offered twice.
10. An output-free nonterminal processing step must make relevant finite progress; a scheduler suspension is a separate condition.

### Combined legal-outcome table

“Reset condition” means both operator dependence and downstream borrowing have ended or been independently preserved.

| Outcome | Output consumable? | Old input lifecycle | Backing resettable? | Continuation required? | New input allowed? | Terminal observed? | Progress / next action |
|---|---|---|---|---|---|---|---|
| Nonempty output; retained input pending | Yes | Pending | No while needed | Yes | No | No | Deliver once; resume |
| Nonempty output; independently preserved pending work | Yes | Pending | Only under reset condition | Yes | No | No | Deliver once; drain owned state |
| Nonempty output; input resolved | Yes | Resolved | Only under reset condition | No, unless terminal control pending | After handoff and readiness | No | Deliver once; then follow readiness |
| Empty output; input resolved | Valid empty domain | Resolved | Only under reset condition | Usually no | After handoff/readiness | No | Resolution is progress |
| Empty output; pending input advances | Valid empty domain | Pending | Only under reset condition | Yes | No | No | Finite advancement; resume |
| No output; pending continuation advances | No | Pending | Only under reset condition | Yes | No | No | Finite advancement; resume |
| No output; ready for input | No | Resolved or no prior input | Only under reset condition | No | Yes | No | Await/accept new input |
| FINISHED; no output | No | Resolved or no input | Only under reset condition | No | No | Yes | Stop data requests |
| Valid early-stop completion; no output | No | Resolved by safe abandonment | Only under reset condition | No | No | Yes | Stop unnecessary upstream; clean up |
| Final output immediately before terminal | Yes | Required work resolved or safely abandoned | Only under reset condition | Terminal control only | **No** | No | Consume/preserve, then observe terminal |
| Terminal failure | No output from failed invocation | Failed/discarded, not successful resolution | Safe unwind only | No successful resume | No | Failure terminal | Owner-directed cleanup |
| Observed cancellation | No output from canceled invocation | Canceled/discarded | Safe unwind only | No successful resume | No | Cancellation terminal | Cleanup under existing owners |
| Terminal plus consumable output | **Illegal** | — | — | — | No | — | Violates D26-S1 |
| No output, no resolution, unchanged continuation | **Illegal processing step** | Pending | — | — | No | No | No-progress violation |

“Ready” is a state, not permission to repeatedly invoke processing with neither input nor continuation.

### Source and streaming terminal uniformity

The same convention applies to both:

```text
output available
    → consume or preserve that output
    → terminal completion without output
```

For a Limit final batch, the output-available transition also communicates **no new input permitted**. The next action is the terminal-control transition, not another source fetch.

For ordinary source exhaustion, another source request is permitted while execution still demands it; it establishes local completion. A short batch alone supplies no evidence that this will be the next outcome.

An upstream source’s explicit completion is communicated as control information after its outstanding output has been handled. It is not converted into an empty input batch. Streaming operators drain any remaining required continuation before their own completion. This specifies the handoff without prescribing an end-of-input method.

## 5. Role-specific matrices

### Streaming operators

| Case | Accepted? | Lifecycle resolved? | Operator releases original backing? | Output available? | Continuation pending? | New input? | Terminal? | Next action |
|---|---|---|---|---|---|---|---|---|
| One input → one output | Once | After required work/output offering | When no further operator dependency | Yes | No | After output handoff/readiness | No | Consume output |
| One input → no occurrences | Once | Yes | Normally yes | Empty or absent | No | Yes after handoff | No | Accept next input |
| One input → multiple outputs | Once | Only after draining required work | Depends on ownership | Per output step | Yes until drained | No while pending | No | Resume existing acceptance |
| Input copied; buffered output pending | Once | No while required output remains unoffered | Yes, after exact preservation | Per drain step | Yes | No | No | Drain owned output |
| Input borrowed; continuation pending | Once | No | No while dependence remains | Per step | Yes | No | No | Preserve input; resume |
| Early stop with final output | Once if input needed | Resolved by completion/safe abandonment | Subject to remaining dependencies | Yes | Terminal control | No | Not yet | Deliver final output, then terminal |
| Early stop without output | If applicable | Resolved by safe abandonment | Subject to borrows | No | No | No | Yes | Stop unnecessary upstream |
| Failure during continuation | Possibly partial processing | Failed, not successful resolution | Through safe cleanup | Failed invocation: no | No successful resume after terminal failure | No | Failure | Cleanup |
| Cancellation during continuation | Accepted state may exist | Canceled | Through safe cleanup | Canceled invocation: no | No successful resume | No | Cancellation | Cleanup |

### Sinks

The actual sink contracts support complete-call acknowledgment: §28.7 describes per-batch build ingestion; §29.2 separates Update from Combine/Finalize; §30.1 accumulates sort state; §§26.6 and 27.11 require safe retained ownership.

No inspected v1 sink requires exposing partial successful input acceptance to the generic driver.

| Sink case | Acceptance | Public partial-success acknowledgment? | Caller replay? | Ownership | Continuation | Finalization owner / result |
|---|---|---|---|---|---|---|
| Successful full call | Complete submitted domain accepted once | No | No | Consumed or preserved | Internal work may occur within call | Success of this call only |
| Retaining sink | Complete domain on success | No | No | Stable retained owner required | Retained state is not caller resubmission | Retaining owner |
| Blocking sink | Complete domain on success | No | No | Accounted build state | Many successful input calls may accumulate | Specialized Combine/Finalize |
| Sink failure | A prefix may already have been processed internally | No successful acknowledgment | **No blind replay** | Preserve until safe unwind | No successful continuation after terminal failure | §39 and statement/operator owner |
| Cancellation | Processing may be partial | No | No | Safe cleanup | No successful resume | QueryCancelled owner |
| Finalization | Not another acceptance of an input batch | N/A | No input replay | Existing state | Specialized finalization | Required owner decides success/output |

This full-call rule belongs in D26-S2’s sink boundary. It is not a third decision about transactional atomicity: successful sink ingestion does not imply statement success, commit, finalization, or external publication. Failure does not promise that no physical DML work occurred.

A concrete implementation may suspend a sink operation internally, but cannot present an incomplete acceptance as a completed successful `Sink` operation.

## 6. Progress and ownership

### Progress matrix

| Event | Operator progress? | Legal nonterminal? | Immediate continuation? | Suspension? | Terminal? | Owner |
|---|---|---|---|---|---|---|
| New required output occurrence offered | Yes | Yes | After output handoff, if pending | Not inherently | No | Operator semantics + D26-S2 |
| Accepted input consumed/resolved | Yes | Yes | Only if another required action exists | Await input when ready | Not necessarily | D26-S2 |
| Original input dependence released | Finite ownership progress | Yes | If relevant work remains | Not inherently | No | D26-S2 / D23-S2 |
| Continuation cursor advances toward completion | Yes | Yes | Yes if runnable | Not inherently | No | D26-S2 |
| Empty output with finite relevant advancement | Yes | Yes | Yes if runnable | Not inherently | No | §§23.1, 26.4 / D26-S2 |
| Same-state, no-output processing return | No | No | No spinning | Cannot invent a wait | No | Internal protocol boundary |
| Real dependency not ready | Not operator progress | Waiting is legal | Not blind retry | Suspend under scheduler owner | No | §§26.1, 32.8 |
| Resource-pressure action | Resource progress, not automatically operator progress | Only under D24-S3 | Only after relevant request-state change | Owner-dependent | May fail | §24.6 |
| Cancellation observed | Terminates successful processing | No successful continuation | No | Unwind | Cancellation | §§26.7, 39 |
| Terminal completion | Yes, completion transition | No further processing | No | No | Yes | D26-S1 |

For finite input and finite required output, repeated continuation must be well-founded: it cannot cycle indefinitely through equivalent states while claiming progress. Producing the same output again is duplication, not progress.

No numeric counter, timeout, sleep, or scheduler algorithm is required. Resource-pressure progress does not excuse an infinite operator loop, and operator activity does not excuse a non-progressing resource retry.

### Ownership matrix

| State | Owner | Operator dependency live? | Downstream dependency live? | Replace/rebind caller input storage? | Reset original backing? | Accounting / cleanup |
|---|---|---|---|---|---|---|
| Accepted caller-owned input | Caller/producer | Possibly | Possibly | Not while needed | No while either dependency remains | Ch24 where query-owned; normal lifetime |
| Output borrowing input | Input backing owner | May be finished | Yes | Only without invalidating view | No until borrower preserved/finished | D23-S2 |
| Retained-input continuation | Caller or valid retained owner | Yes | Possibly | Only through exact preservation | No destructive reset while needed | Ch24 + operator cleanup |
| Copied continuation | Operator owner | No original-backing dependency | Possibly | Yes if all old views preserved | Only if downstream also independent | Ch24 accounted state |
| Buffered output | Operator/result owner | Independent state remains | Possibly | Original input may be releasable | Reset original only under full condition | Ch24 |
| Downstream borrowed view | Original or transferred owner | May be none | Yes | May rebind storage, not invalidate backing | No until stable view preserved/ended | D23-S2 |
| Retaining sink input | Sink’s valid owner | Sink owns retained dependence | Owner-dependent | Yes after valid handoff | Only when original backing no longer needed | Ch24 / retaining owner |
| Terminal state | Execution owner | No successful future processing | Existing borrows may remain | No new input admission | Not automatically | Safe lifetime teardown |
| Failure unwind | Existing owners | Ends through quiescence/unwind | Must remain safe during unwind | Not a new processing operation | Only after dependencies safe | §§24.10, 39.3 |

Rebinding a container to new storage is not the same as overwriting its old backing. The protocol permits the former when exact ownership keeps existing views valid.

## 7. Errors, early stop, and invalid states

### Error-interaction matrix

“Current output” below means the output of the failed evaluation/invocation, not an earlier successful output transition.

| Event | Input lifecycle | Pending output usable? | Current failed output publishable? | Continue? | Selection owner | Cleanup / replay |
|---|---|---|---|---|---|---|
| D25 ordinary candidate discovered | Required owner-directed work may remain | Only independently valid successful output under its owner | No | Candidate-establishing/reduction work may remain | §25.1.1 | No automatic abandonment or replay |
| D25 terminal error established | Failed execution; discard pending lifecycle | No new successful delivery from failed execution | No | No successful resume | D25-S1 | §§24/39; no replay |
| D21 candidate discovered | Preserve finalized-attempt eligibility and required work | Only as DML owner permits; no premature RETURNING | No | Required candidate work may remain | §21.16.1 | No generic first-error abort/replay |
| D21 terminal error established | Failed attempt | No successful unpublished DML output | No | No successful resume | D21-S4 | Ch21/§39; retry only if admitted |
| OutOfMemory | Failed runtime/attempt | No new successful output from failure | No | No forced draining | §24.10 | Safe cleanup; no generic replay |
| Representability ExecutionError | Same | Same | No | No | §§23/24/39 | Same |
| SpillIOError | Same | Same | No | No | §24.10 | Same |
| QueryCancelled | Canceled runtime | No new successful output from canceled invocation | No | No successful resume | §§26.7, 39 | Safe cleanup; no generic replay |
| Internal protocol violation | Invalid runtime | Invalid results inaccessible | No | No invalid-state continuation | §39.1 | Internal-failure consequences |

Candidate discovery is not automatically terminal query failure. Owner-directed candidate collection can advance or finish accounting for an input without pretending that failed scalar evaluation produced a successful value.

The proposed protocol does not require ordinary output production before candidate collection can proceed, nor allow physical first discovery to discard work needed by D25-S1 or D21-S4. Detailed transport and publication integration remains **M26-2 OPEN**.

Previously successful output transitions are not replayed merely because later processing fails. Their internal retention/discard and external visibility follow the existing operator, DML, and cursor owners.

### Early-stop matrix

| Event | Remaining accepted input required? | New input allowed? | Upstream demand | Pending output | Cleanup | Classification |
|---|---|---|---|---|---|---|
| LIMIT satisfied | Only work not proven safely unnecessary | No for stopped local path | Stop only unnecessary portion | Deliver required final output first | Required | Successful local early stop |
| EXISTS satisfied | Later rows unnecessary under §20.14.5 | No for existence consumer | Stop specialized child demand | Preserve required existence result | Required | Specialized successful stop |
| Generic operator terminal | No further work in completed local state | No | Other consumers/required graph work remain separate | None unoffered | Required | Local completion |
| Source exhausted | No remaining source occurrences | No more source data requests | Downstream may still have required work | Final batch already delivered | Required | Local completion |
| Terminal failure | Successful processing ends; prior candidate obligations already owner-governed | No | Quiesce failed execution | No new successful failed output | Required | Failure |
| Cancellation | No successful continuation | No | Stop under cancellation owner | Canceled invocation output invalid | Required | QueryCancelled |

A final output transition that establishes safe early stop must withhold new-input readiness immediately. The subsequent no-output terminal transition must not fetch upstream.

This does not define “take K raw source rows.” Ordering, OFFSET, blocking work, and semantic demand remain upstream decisions.

### Invalid-state matrix

All rows are internal protocol/runtime violations, not new public SQL errors.

| Invalid state | Public SQL error? | Internal? | Required prevention/rejection point | Consequence prevented |
|---|---:|---:|---|---|
| Source terminal with consumable output | No | Yes | Before downstream interprets outcome | Final-batch disagreement |
| Source output after terminal | No | Yes | Before emission | Duplicate/new post-EOS rows |
| Driver data request after terminal | No | Yes; outside valid domain | Before re-entering data processing | Accidental restart |
| New input while prior lifecycle unresolved | No | Yes | Before acceptance | Lost continuation |
| Same input accepted twice | No | Yes | Before second acceptance | Duplicate occurrences |
| Input reset while operator still depends on it | No | Yes | Before mutation/dereference | UAF/stale values |
| Continuation requested with none pending | No | Yes for a processing request | Before resuming nonexistent work | Stale output/state reuse |
| New input before pending continuation drains | No | Yes | Before admission | Lost/reordered required output |
| Nonterminal same-state no-progress processing loop | No | Yes | Prevent invalid loop; stop detected violation | Livelock |
| Available output invalid/uninitialized | No | Yes | Before active output access | Invalid values/OOB access |
| Failed invocation output consumed | No | Yes | Before publication/consumption | Partial/stale successful result |
| Terminal operator later emits output | No | Yes | Before emission | Terminality/multiplicity violation |

Construction invariants and local guards can enforce these rules. No mandatory validation API or full-plan revalidation on every call is proposed.

## 8. Proposed normative Architecture wording

The following is proposed text only. It has not been added to any file.

### D26-S1 — Output availability and terminal completion

> Source and streaming-operator handoffs distinguish an output-available transition from terminal completion. A terminal completion outcome, including a source `FINISHED` outcome, MUST carry no consumable output for that invocation. Reusable output storage may still exist, but neither its contents nor its cardinality make it a result of the terminal invocation.
>
> Every consumable output batch, including the final nonempty batch, MUST be offered through an output-available transition before terminal completion is observed. The caller consumes that output exactly once or establishes the valid downstream ownership required by §§23.10–23.13 before completing its handoff. Concrete execution interfaces MAY implement these ordered transitions through separate calls, callbacks, or another exact mechanism; they MUST NOT make output validity depend on an undocumented producer-specific convention.
>
> For a source, `HAVE_MORE` identifies the nonterminal output path and does not promise that another nonempty batch exists. An empty executable batch remains valid only with the finite-progress requirement of this section. Neither empty cardinality nor cardinality smaller than capacity establishes exhaustion, finality, or a pending terminal outcome.
>
> An exhausted source with no output to deliver MAY report `FINISHED` on its first invocation. A source producing its final batch reports that batch through the output path and subsequently reports terminal completion without another batch. If exhaustion is already established, the terminal transition requires no additional logical-row evaluation.
>
> After terminal completion, the driver MUST NOT request further data from that execution state, and the state MUST NOT produce another logical occurrence. A post-terminal data request is outside the valid invocation domain; the contract requires neither rewindability nor an idempotent repeated-terminal API.
>
> When a streaming operator establishes valid early stop while offering final output, it MUST withhold new-input readiness immediately. After the final output handoff, the remaining terminal transition consumes no new upstream input. Failure, cancellation, or an owning early-stop rule may instead terminate the execution path without an otherwise unnecessary source-exhaustion probe.
>
> These are local execution transitions. They do not establish whole-query success, discharge required Combine/Finalize dependencies, or define external cursor EOS. Those obligations remain with the pipeline, specialized operator, statement, and Chapter-31 result owners.

### D26-S2 — Accepted-input, continuation, and progress lifecycle

> A streaming operator accepts each submitted logical input-occurrence domain at most once within its execution. Acceptance transfers responsibility for processing that domain to the operator; it does not imply that one invocation has completed all processing or output. Equal-valued inputs and successive uses of the same physical buffer remain distinct submissions when they represent distinct required input occurrences. No new semantic row, chunk, or pipeline identifier is introduced.
>
> Within one `LocalOperatorState`, the generic handoff admits at most one accepted-input lifecycle at a time. Continuation resumes that acceptance or its operator-owned state without accepting the input again. Passing an input reference to a continuation call does not resubmit its occurrences as new work.
>
> An accepted-input lifecycle is resolved when all processing and output obligations required from that acceptance have been discharged, or when the owning semantics establish that its remaining work is safely unnecessary because of valid terminal early stop. Resolution is not inferred from an invocation returning, from output cardinality, or from copying the input. An output already offered may remain subject to downstream consumption and borrowing after the accepted-input lifecycle resolves.
>
> The handoff MUST make independently determinable whether output is available, whether continuation remains before another input may be accepted, whether the operator still requires the caller to preserve the original input backing, and whether the operator is ready for new input or has reached terminal completion. These facts need not be represented by separate fields or a particular status enum.
>
> The operator MAY retain a value-stable dependency on the accepted input or copy, transfer, or otherwise independently preserve the state required for continuation. Once that preservation removes the operator’s dependence on the original caller backing, the operator MAY acknowledge its release even while processing or buffered output remains pending. Such release does not authorize accepting another input before the outstanding lifecycle and continuation permit it.
>
> The driver MUST NOT submit another input until the previous accepted-input lifecycle is resolved, no continuation must precede new input, and the outstanding output handoff has completed. Terminal completion or an established early stop forbids new input. A final output followed only by a terminal-control transition therefore does not request replacement input.
>
> Physical backing reset additionally obeys §§23.10–23.13 and §26.6. The caller MUST NOT reset, reuse, or incompatibly mutate backing still required by the operator or any downstream borrowed view unless an exact ownership mechanism preserves every required logical view. Input processing completion, operator release acknowledgment, and downstream borrow completion are distinct boundaries.
>
> Every required output occurrence MUST be offered exactly once according to the owning operator’s multiplicity and ordering semantics. A streaming step may offer nonempty output, offer an empty valid batch, or offer no batch while resolving input or advancing continuation. Producing no output is not completion. Upstream completion is explicit control information, not an empty input batch; outstanding required continuation is handled before local terminal completion.
>
> Every nonterminal processing step without new output MUST resolve or release accepted-input dependence, or advance finite relevant continuation state toward required output, input resolution, or terminal completion. For finite input and finite required output, continuation MUST be well-founded. Repeated equivalent state with no output and no relevant advancement is an internal protocol/liveness violation. Dependency suspension remains scheduler-owned, and resource-pressure progress remains §24.6-owned; neither permits a busy no-progress continuation loop. No numeric progress counter, timeout, or particular suspension API is required.
>
> Valid early stop resolves safely unnecessary remaining work without evaluating it merely to acknowledge consumption. It does not authorize abandoning required blocking work, side effects, or other consumers’ demanded work. The final-output and terminal transitions follow D26-S1.
>
> Discovery of an ordinary expression or DML error candidate does not by itself authorize terminal abandonment. Sections 25.1.1, 25.1.2, and 21.16.1 retain candidate eligibility, preservation, and selection ownership. A failed evaluation supplies no successful output from that invocation; any further candidate-establishing work follows its semantic owner rather than generic replay. Terminal failure or observed cancellation ends successful continuation and unwinds pending state under Chapters 24 and 39. A statement retry is admitted only by its existing owner and uses fresh attempt-local execution state.
>
> A completed successful generic `Sink` invocation accepts its complete submitted logical-occurrence domain once for that invocation. Retention obtains stable ownership under §26.6 and Chapters 23–24; accumulated state may still require its separately owned Combine/Finalize work. An incomplete or failed sink invocation MUST NOT be represented as complete successful acceptance or blindly replayed by the generic driver. This acknowledgment neither promises absence of partial work on failure nor establishes statement atomicity, commit, or external result publication.
>
> Continuation, retained input, and buffered output remain execution-owned, memory-accounted state under Chapters 22–24, separate from immutable plan configuration. Distinct worker/local states may execute independently under Chapter 32. This protocol introduces no SQL ordering, error precedence, resource limit, persistence format, or required C++ API.

### Approval granularity

D26-S1 and D26-S2 are independently understandable decisions, but should be approved together as one interoperable protocol.

Recommended owner response:

```text
accept D26-S1 D26-S2
```

That accepts the semantic decisions; integration remains a separate authorized task.

## 9. Regression and implementation-freedom assessment

| Frozen contract | Result |
|---|---|
| Chapter 20 values, NULLs, bags | Preserved; generic handoff neither invents nor drops occurrences |
| Required ordering | Preserved; no order inferred from source, worker, chunk, or continuation |
| D20-B1/D20-B2 | Preserved; scheduling and copying cannot create visible undemanded work |
| LogicalLimit | Preserved; early stop only when its owner establishes no further demand |
| EXISTS | Preserved; no later-row work after its specialized demand is satisfied |
| Chapter 21 attempts/retry | Preserved; continuation is not statement replay |
| D21-S4/D21-S5 | Preserved; no physical candidate reduction or RETURNING ordering |
| Chapter 22 | Immutable plan and execution/local-state separation preserved |
| Chapter 23 | Capacity 1–65535; cardinality 0–capacity; empty/short not EOS |
| D23-S2 | Full value-stable borrow interval preserved |
| Chapter 24 | All growing continuation/buffer state accounted; no new limits |
| Chapter 25 result occurrences | Preserved; scalar results retain demanded correspondence |
| D25-S1 | Unchanged candidate domain, preorder, equivalence, and early-stop proof |
| Failed Evaluate | No successful output from failed invocation |
| Chapter 31 | Internal completion is not external publication; prior returned prefix is not successful whole-query execution |
| §39 | Categories, cleanup, terminal failure, and transaction consequences unchanged |
| Finalization | Local terminal status does not bypass required barriers |
| Persistence | No runtime continuation, pointer, or acknowledgment becomes persistent identity |

Permitted implementation choices include retained-input continuation, copied/buffered continuation, callback emission, separate drain methods, state-based acknowledgments, multiple workers, independent source partitions, and any conforming scheduler.

Not prescribed: C++ return types, enum numeric values, `NEED_INPUT`, `BLOCKED`, coroutines, futures, queues, locks, a numeric progress counter, or one globally serial pipeline.

## 10. Future integration and Verification consequences

### Smallest semantic-integration surface

| Section | Proposed integration |
|---|---|
| §26.4 Runtime interfaces | Canonical output/terminal convention; acceptance, continuation, readiness, sink acknowledgment, progress |
| §26.5 Global and local state | Reference execution-local continuation ownership and per-local-state admission |
| §26.8 Pipeline early stop | Resolve safely unnecessary input; final output before terminal; no intervening upstream fetch |
| §26.6 | At most a precise reference if needed; existing borrow semantics remain authoritative |

No cross-owner change is presently necessary.

The following findings remain **OPEN**, not closed by this package:

- **M26-1:** Initialization, finalization, root completion, failure quiescence, and fresh-attempt lifecycle integration.
- **M26-2:** Candidate transport, owner-selected terminal error, diagnostic lifetime, and failed-output integration.
- **N26-1:** Production-sequencing language.
- **N26-2:** Sink/pipeline-breaker terminology.

### Future Verification obligations

| Family | Existing reusable methodology | Required addition after approval/integration |
|---|---|---|
| Source final output | V23-B explicit status model | Final batch offered once, followed by output-free terminal |
| Empty source | V23-B | First-call FINISHED without fake empty output |
| Empty/short nonterminal | V23-B; Scan/Unary tests | Cardinality cannot predict finality; empty progress remains explicit |
| Post-terminal behavior | Partial generic coverage | Reject further data requests/output; no implicit rewind |
| Single-output/empty-output operators | Scan/Unary; V25 result oracle | Acceptance and readiness acknowledgment |
| Multi-output input | Join output-larger-than-chunk cases | Independent acceptance/continuation trace; no resubmission |
| Retained-input continuation | V23 borrow graph; V25-L | Backing stable across every continuation |
| Copied/buffered continuation | Ch24 ownership/accounting | Release original backing while output remains pending; no new-input admission |
| Final Limit output | Limit and semantic-demand oracles | Readiness false immediately; terminal transition performs no source fetch |
| Progress | V23-B; D24-S3 methodology | Well-founded operator continuation; resource progress separate |
| Sink acknowledgment | Blocking-state tests | Full successful call acceptance; failed call not blindly replayed |
| Failure/cancellation | Pipeline cleanup; V25-N/O | Pending lifecycle teardown; no successful failed output |
| Candidate handling | V25-I/J/K | Candidate discovery does not automatically close the execution path |
| Retry freshness | Attempt and V25-P reuse methods | No stale acceptance, cursor, buffered output, or candidate state |
| Parallelism | Parallel Execution Tests | Same per-local-state protocol under deterministic interleavings |
| Cursor boundary | V25-O; §31.10 model | Internal terminal observation does not prematurely expose successful EOS |

Use independent occurrence ledgers, explicit transition models, owner/borrow graphs, semantic demand/error oracles, and deterministic barriers. No timeout-based progress proof or production executor as its own oracle is needed.

These are future obligations—not a claim that Chapter-26 Verification is synchronized.

## 11. Reread answers 1–118

The following answers assess the **recommended protocol**, not approval or integration status. Ranges enumerate every question in the indicated range.

| Questions | Answer |
|---|---|
| 1–8 | **YES** |
| 9: output after terminal? | **NO** |
| 10–15 | **YES** |
| 16: accept one logical input twice? | **NO** |
| 17–23 | **YES** |
| 24: replace input while unresolved dependency remains? | **NO** |
| 25: resubmit input as new continuation work? | **NO** |
| 26–43 | **YES** |
| 44–46 | **YES** |
| 47: expose subsequently undemanded upstream errors? | **NO** |
| 48–51 | **YES** |
| 52: reset backing while downstream still borrows? | **NO** |
| 53–70 | **YES** |
| 71–73 | **YES** |
| 74: blindly replay failed sink input? | **NO** |
| 75–77 | **YES**; no third question needed |
| 78–104 | **YES** |
| 105–106 | **YES—complete recommendations supplied; owner acceptance pending** |
| 107: additional frozen question? | **NO** |
| 108–111: M26-1/M26-2/N26-1/N26-2 still open? | **YES** |
| 112: semantic questions resolved after owner approval? | **YES for the two frozen questions; live integration still required** |
| 113: document/integration clean now? | **NO** |
| 114: Verification synchronized? | **NO** |
| 115: Chapter 26 fully closed? | **NO** |
| 116: Chapter-27 review started? | **NO** |
| 117: Phase 2 started? | **NO** |
| 118: Phase 2 authorized? | **NO** |

## 12. Status and next action

**Chapter 26: SEMANTIC DECISIONS PENDING ARCHITECTURE-OWNER APPROVAL — NOT CLEAN — NOT FULLY CLOSED.**

No new frozen Chapter-26 semantic question.

Await approval of **D26-S1 and D26-S2**. After approval, the next task is **CHAPTER-26 SEMANTIC INTEGRATION**, followed separately by M/N cleanup and Verification synchronization.

No Architecture or Verification edits, implementation, build, tests, sanitizers, benchmarks, staging, commits, devlog, or generated review artifacts occurred.

**Chapter-27 review remains NOT STARTED. Phase 2 remains NOT STARTED / NOT AUTHORIZED.**