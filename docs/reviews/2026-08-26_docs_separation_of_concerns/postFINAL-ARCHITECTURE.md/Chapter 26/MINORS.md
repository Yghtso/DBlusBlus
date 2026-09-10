# Chapter 26 — Architecture CLEAN

Completed the targeted cleanup. **M26-1, M26-2, N26-1, and N26-2 are CLOSED.** D26-S1/D26-S2 remain unchanged. No new frozen semantic question was discovered.

Chapter 26 is **not fully closed**: Verification synchronization remains pending.

## Repository and scope

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | `M docs/ARCHITECTURE.md` |
| Index | Clean | Clean |
| HEAD | `014227215a55202b22249e0180bb8cda7d7fc8b4` | Unchanged |
| Architecture diff | None | 163 insertions, 10 deletions |
| `git diff --check` | — | Passed |

The preceding D26-S1/D26-S2 integration was already committed; there were no pre-existing working-tree hunks to preserve.

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) was modified. A byte comparison against HEAD confirmed:

- Chapters 1–25 and Chapter 27 onward are unchanged.
- §26.4, containing the frozen source, streaming, and sink protocols, is unchanged.
- §§26.6–26.8, covering borrowing, cancellation, and early stop, are unchanged.

Historical review artifacts were **unread, unmodified, unmoved, and unstaged**. No external repository changes were observed.

### Sections modified

| Section | Change |
|---|---|
| [§26.2 Pipeline roles](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20658) | Distinguished sinks from pipeline breakers; removed ambiguous “Later” navigation |
| [§26.3.1 Execution lifecycle and completion](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20750) | Added generic lifecycle and completion handoffs |
| [§26.3.2 Error ownership and result handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20823) | Added canonical error-owner and publication handoffs |
| [§26.5 Global and local state](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21095) | Removed production-sequencing language |
| [§26.9 Worker and scheduling boundary](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21193) | Replaced chronological heading and worker wording |
| [§26.10 Pipeline invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21208) | Removed “Later” from parallel-execution invariant |

Final Chapter 26 occupies lines **20606–21222**. Chapter 27 begins at **21223**, titled **“Scans and Unary Physical Operators.”**

## M26-1 — Lifecycle and completion

The original gap was the missing connection between local invocation protocols and execution-wide initialization, completion, failure, cleanup, and retry.

The integrated lifecycle now states:

1. Execution starts from a validated immutable physical plan.
2. Mutable execution/attempt-local state is initialized before use.
3. Required source/operator/sink work executes under the frozen handoff protocol.
4. Required dependencies and operator-owned Combine/Finalize steps complete successfully.
5. Completion passes to the root/result owner.
6. Terminal failure or cancellation instead leads to error propagation, quiescence, and cleanup.

Key boundaries are explicit:

- Local `FINISHED`, one completed pipeline, or successful sink acceptance does **not** establish whole-query success.
- Unready, unfinalized, failed, or canceled prerequisite state cannot be consumed as successful input.
- Required semantic finalization gates success. It is distinct from destruction and resource cleanup.
- Finalization failure prevents success; cleanup remains required.
- Valid early stop need not exhaust unread input, but remaining demanded work and its finalization still matter.
- Internal/root completion is distinct from Chapter-31 result publication and cursor EOS.
- Success cannot be declared while required execution or error-producing finalization work remains outstanding.
- Active users must quiesce before their backing is destroyed. No particular synchronization primitive is prescribed.
- Terminal failure/cancellation cannot become successful completion in the same runtime instance.
- An authorized retry receives fresh mutable state, including source positions, continuation, candidates, sink/finalization state, temporary output, and scratch.
- Lifecycle misuse remains internal under §39.1.3; no public error category was added.

Cleanup applies on success, failure, cancellation, and valid early termination, while respecting live borrows, accounting, transaction-owned resources, and valid longer-lived result ownership.

### Lifecycle audit matrix

“Owner-required” means only steps required by the relevant operator contract—not mandatory Combine and Finalize for every sink. Cleanup never permits premature destruction of live backing.

| Event | Local completion | Query success by itself? | Remaining semantic work/finalization | Publication owner | Cleanup | Failed state reusable? | Canonical owners |
|---|---|---|---|---|---|---|---|
| Initialization | No | No | Required execution remains | §31 | Required | No | §§22.2–22.6 |
| Normal streaming | Per invocation | No | Continuation/downstream work may remain | §31 | Required | No | §26.4 |
| Source `FINISHED` | Source complete | No | Other pipelines/finalizers may remain | §31 | Required | No rewind | §26.4.1 |
| Operator terminal | Operator complete | No | Other demanded work may remain | §31 | Required | No resurrection | §§26.4, 26.8 |
| Sink input accepted | Call accepted | No | Owner-required build/finalization may remain | §31 | Required | No blind replay | §26.4.3 |
| Required Combine | Step complete | No | Finalize/dependencies may remain | §31 | Required | No | §§29.2, 32 |
| Required Finalize | Prerequisite complete if successful | No | Root/dependent work may remain | §31 | Required even on failure | No | §§29.3.7, 30.1, 32 |
| Valid early stop | Unnecessary work closed | No | Still-demanded work only | §31 | Required | Not retry/resume permission | §§20, 26.8 |
| Root completion | Required internal work complete | Only with all success prerequisites | No outstanding required work | §§31.9–31.10 | Required/transferred | No failed state | §26.3.1 |
| Successful cursor EOS | External exhaustion | Successful result completion under its owner | Not transaction commit | §31.10 | Lifetime-owned | No | §31 |
| Terminal semantic error | Unsuccessful terminal path | No | Propagation and cleanup | §§31, 39 | Required | No | Error owner; §39 |
| Resource error | Unsuccessful terminal path | No | Propagation and cleanup | §§31, 39 | Required | No | §§24.10, 39 |
| Cancellation | Unsuccessful terminal path | No | Quiescence and cleanup | §§31, 39 | Required | No | §§26.7, 32.8, 39 |
| Cleanup | Resource release | Not evidence of success | Cannot substitute for Finalize | §31 | This obligation | No | §§23–24, 39 |
| Authorized retry | Fresh execution instance | No | Re-executes required work | §31 | Failed attempt cleaned | No inheritance | §§31.5, 39.1.4 |

## M26-2 — Error ownership and result handoff

The original gap was the missing execution-wide owner map connecting discovered candidates, selected errors, failed output, prior handoffs, and external results.

The added text makes these distinctions explicit:

- **Candidate discovery is not terminal-error selection.**
- D25-S1 remains the ordinary non-DML expression-error owner.
- D21-S4 remains the DML owner; eligible candidates, provenance, category/cause, and phase are preserved.
- DML cannot be pre-ranked through D25-S1.
- First source, lane, chunk, worker, pipeline, or callback discovery cannot replace either semantic owner.
- Aggregate-finalization and subquery errors retain their specialized owners.
- Resource failures and cancellation retain Chapters 24/39; no global cross-class precedence was introduced.
- Canonical diagnostic fields survive transport. Physical IDs and callback order cannot replace semantic provenance.
- Diagnostic backing remains valid while reporting depends on it.
- Owner-established terminal failure permits propagation, quiescence, and cleanup—not successful continuation, new input, or generic replay.
- Failed current output, prior completed internal output, and prior cursor-returned output remain distinct.

### Error-owner audit matrix

For every row, Chapter 26 preserves owner-required diagnostic provenance. A failed current invocation supplies no successful consumable output; an earlier independently completed transition is separate. Transaction consequences remain §39.1-owned.

| Error/event | Candidate or terminal? | Selection/classification owner | Chapter-26 responsibility | May physical discovery order select it? |
|---|---|---|---|---|
| Ordinary non-DML expression failure discovered | Candidate | §25.1.1 / D25-S1 | Preserve/reduce owner-correctly | No |
| D25-S1 minimum established | Terminal ordinary expression error | §25.1.1 | Propagate selected error | No replacement ranking |
| DML failure discovered | Eligible candidate under DML rules | §§21.16.1, 25.1.2 | Preserve required candidates and provenance | No |
| D21-S4 selection established | Terminal statement error | §21.16.1 | Propagate without D25 pre-ranking | No |
| Aggregate-finalization error | Specialized selection, then terminal | §29.3.7 | Preserve numerical/category/ordinal contract | No physical group/reduction ranking |
| Scalar-subquery cardinality error | Specialized terminal outcome | §§20.14.4, 20.14.12 | Preserve child/cardinality precedence | No generic discovery ranking |
| `OutOfMemory` | Operational terminal failure when established | §§24.10, 39.3 | Preserve classification and cleanup | No new cross-class rule |
| Representability `ExecutionError` | Operational terminal failure | §§24.10, 39.3 | Preserve resource/representability cause | No new cross-class rule |
| `SpillIOError` | Operational terminal failure | §§24.10, 39.3 | Preserve temporary-storage classification | No new cross-class rule |
| `QueryCancelled` | Unsuccessful terminal path | §§26.7, 32.8, 39 | Stop successful processing; quiesce/clean | Outside candidate ranking |
| Persistent-page corruption | Lower-layer classified failure | §39.1.3 | Preserve noncontinuable classification | No downgrade or component ranking |
| Internal protocol violation | Invalid runtime state | §§26.4, 39.1.3 | Reject/prevent invalid execution | Not a SQL-error candidate |

Operational failures may prevent further candidate establishment under their owners. This does not define whether a semantic candidate “beats” OOM or cancellation.

### Result-publication audit matrix

| State/event | Consumable output? | Client-visible by itself? | Effect on prior delivery | Evidence of whole-query success? | Owner |
|---|---|---|---|---|---|
| Current partial/uninitialized output | No | No | None | No | §§25.7.1, 26.4 |
| Successful output transition | Yes, under handoff/lifetime rules | No | Internal availability | No | §26.4 |
| Prior completed downstream handoff | Completed internal handoff | Not necessarily | Remains completed | No | §§26.3.2, 26.4 |
| Prior completed cursor return | Valid within cursor lifetime | Yes | Not retroactively retracted | No | §31.10 |
| Successful cursor EOS | No new data batch implied | External completion | Does not change prior lifetime | Successful result completion, not commit | §31 |
| Terminal error | Failed current output unavailable | Error reporting is separate | Earlier completed deliveries remain distinct | No | Error owner; §§31, 39 |
| Cancellation | No successful failed-invocation output | Cancellation reporting is separate | Earlier completed deliveries remain distinct | No | §§26.7, 31, 39 |

DML RETURNING publication remains specifically §31.9-owned. No whole-query buffering requirement, cursor-lifetime extension, transaction rollback rule, or new publication strategy was added.

## N26-1 — Chronology cleanup

### Exact rewrites

| Section | Original | Result |
|---|---|---|
| §26.2 | “Later blocking operators may expose a source after finalization.” | “Blocking operators may expose a source after the finalization required by their operator contract.” |
| §26.5 | “Even when one worker executes the first production version…” | State separation applies “including in single-worker execution.” |
| §26.5 | “This lets later scheduling parallelize pipelines…” | Explains worker-local execution and unchanged ownership |
| §26.9 heading | “Single-thread first, parallel-ready” | “Worker and scheduling boundary” |
| §26.9 | “The first production executor may run one query with one worker.” | “A query may execute with one worker under Chapter 32’s worker model.” |
| §26.9 | “The architecture nevertheless requires” | “Independent of worker count, execution requires” |
| §26.9 | “source state that can later be partitioned where valid” | “source state that can be partitioned where valid” |
| §26.10 | “Later parallel execution must preserve…” | “Parallel execution must preserve…” |

Development-sequencing leakage was removed from Architecture; nothing was copied into Development.

### Temporal-language post-audit

Classification: **A** runtime/lifetime sequence; **B** execution state; **C** compatibility/version scope; **D** durable scope/navigation; **E** project chronology.

The surviving matches for the requested temporal search terms are:

| Phrase | Section | Class | Reason |
|---|---|---|---|
| “initialized IN-subquery probe” | §26.1 | B | Lazy-build runtime state |
| “first demand their occurrence” | §26.1 | A | Semantic demand point |
| “all future input” | §26.2 | A | Remaining stream input |
| “initialized before use” | §26.3.1 | A | Runtime precondition |
| “partially initialized” prerequisite | §26.3.1 | B | Invalid dependency state |
| “eventually released” | §26.3.1 | A | Cleanup/lifetime obligation; no deadline |
| “expression phase until…” | §26.3.2 | B/A | DML phase metadata and selection interval |
| “first physical discovery” | §26.3.2 | A | Prohibited discovery-order selection |
| “phase/eligibility” | §26.3.2 | B | Candidate metadata, not development phase |
| “partial or uninitialized output” | §26.3.2 | B | Runtime output validity |
| “first-physical-error precedence” | §26.4.2 | A | Explicit prohibition |
| “current batch” | §26.6 | B | Active consumption interval |
| “later execution after its owner has been recycled” | §26.6 | A | Invalid deferred borrow use |

Additional runtime-order wording was retained and checked:

| Sections | Surviving phrase families | Class |
|---|---|---|
| §26.1 | Before/after finalization; dormant until demand; a group finishing earlier | A |
| §26.2 | Source after finalization; input accumulation before output; readiness before dependent execution | A |
| §26.3.1 | Before/during cursor servicing; quiescence before destruction; longer-lived owner; after local terminal completion; before claiming success | A |
| §26.3.2 | Until canonical selection/reporting completes; failure before offering output; prior completed handoff; subsequent failure; longer-lived transfer | A |
| §§26.4–26.4.1 | Output before terminal; final batch preceding terminal probe; pending-terminal state; repeated terminal request | A/B |
| §26.4.2 | Preceding lifecycle; not-yet-offered output; release preceding resolution; pending continuation; outstanding handoff; failed continuation | A/B |
| §26.6 | Before reset/reuse; retention across calls | A |
| §26.8 | Subsequently undemanded work; final handoff before terminal; pending terminal-control transition | A/B |
| §26.10 | Consumption before recycling | A |

“Existing owner” denotes canonical ownership, not implementation status. “Foundation examples” is durable architectural scope. `Next()` is an API name; “exactly once” is multiplicity, not project chronology.

**Final E count: 0.** Current-implementation narration, Development sequencing, Verification procedure leakage, Project-State leakage, and history/devlog leakage are all **0**.

## N26-2 — Sink versus pipeline breaker

The original heading **“Sink / pipeline breaker”** conflated input acceptance with blocking/dependency behavior.

The final text distinguishes:

- **Sink:** the execution role consuming input under §26.4.3.
- **Pipeline breaker / blocking boundary:** a dependency boundary where blocking state must reach its owner-defined readiness point before dependent execution.
- An operator can be a sink during build and expose a source or ready state afterward.
- Neither term is a synonym for the other.
- Blocking, streaming, and spillability remain execution traits/capabilities—not new Chapter-37 physical properties.

The existing breaker declarations and examples remain. D26-S2 sink acceptance is byte-for-byte unchanged.

### Sink/breaker role matrix

| Role/trait | Consumes input? | Produces output? | Retains/builds state? | Requires complete input? | Dependency boundary? | External publication | Owner |
|---|---|---|---|---|---|---|---|
| Streaming operator | Yes | Zero/one/multiple batches | May retain continuation | Not all future input | Not inherently | Not by itself | §§26.4, 27 |
| Sink | Yes | Through its owning contract | May | Not inherently | Not inherently | Result owner, if applicable | §26.4.3 |
| Blocking operator | Build/input phase | After required readiness | Yes | Its required blocking input | As defined by owner | Not by itself | §§29–30 |
| Pipeline breaker | Trait, not a separate invocation role | Owner-defined | Blocking state governs readiness | Owner-defined | Yes | Not by itself | §§26.1–26.2 |
| Source after finalization | No new build input in source role | Yes | Uses ready state | Build prerequisites already satisfied | Consumes successful prerequisite | Not by itself | §§26.2, 29–30, 32 |
| ResultSink | Yes | Hands results to result owner | Synchronous or safely retained | Strategy-dependent; RETURNING has §31.9 gate | Not inherently | Chapter 31 | §§27.11, 31 |

`OrderingProperty` and `RequiredSlotSet` remain the Chapter-37 property system. No property, subsystem, or sink class hierarchy was added.

## Cross-reference audit

The table covers all reference targets added or changed by this task, grouping repeated uses within the same source section.

For every row: **target exists; canonical owner is correct; reference is precise for its stated purpose; no circular normative definition was introduced; status GOOD.** Internal references compose distinct owner responsibilities rather than redefining them.

| Source | Target(s) | Purpose |
|---|---|---|
| §26.2 | §26.4.3 | Sink acceptance and retention |
| §26.2 | §§27.11, 31.9–31.10 | Result-sink/result-owner handoff |
| §26.2 | §§22.7, 37.1 | Execution traits versus physical properties |
| §26.2 | §§29.2–29.3, 30.1 | Blocking/finalization owners |
| §26.3.1 | §§22.2–22.3, 22.8, 38.24 | Immutable validated plan |
| §26.3.1 | §§22.5–22.6 | Execution context and mutable runtime ownership |
| §26.3.1 | §26.4 | Frozen invocation protocol |
| §26.3.1 | §§29.2, 29.3.7 | Aggregate semantic Combine/Finalize and validation |
| §26.3.1 | §30.1 | Sort build/output lifecycle |
| §26.3.1 | §§32.5, 32.7, 32.8 | Parallel barriers and task readiness |
| §26.3.1 | §§26.8, 20.12, 20.14.5, 20.17 | Valid early stop and remaining demand |
| §26.3.1 | §§31.9–31.10 | External publication, returned lifetime, EOS |
| §26.3.1 | §§21.15, 31.9, 39.1 | DML statement/publication versus transaction outcome |
| §26.3.1 | §§32.8, 39.3 | Failure/cancellation quiescence |
| §26.3.1 | §§23.10–23.13, 26.6 | Borrow-safe cleanup |
| §26.3.1 | §§24.4–24.5, 24.10 | Accounting and temporary-resource cleanup |
| §26.3.1 | §31.10 | Retained-result ownership |
| §26.3.1 | §39.1 | Transaction resources and consequences |
| §26.3.1 | §39.3 | Terminal-failure propagation and cleanup |
| §26.3.1 | §§31.5, 39.1.4 | Authorized fresh-attempt retry |
| §26.3.1 | §§26.4, 39.1.3 | Internal lifecycle misuse |
| §26.3.2 | §25.1.1 / D25-S1 | Ordinary non-DML selection |
| §26.3.2 | §21.16.1 / D21-S4; §25.1.2 | DML eligibility, provenance, and ranking |
| §26.3.2 | Chapter 32 | Scheduling does not replace error owners |
| §26.3.2 | §29.3.7 | Aggregate-finalization errors |
| §26.3.2 | §§20.14.4, 20.14.12 | Scalar cardinality and specialized subquery precedence |
| §26.3.2 | §39.1.3 | Persistent corruption classification |
| §26.3.2 | §§24.10, 39.3 | Resource/cancellation categories |
| §26.3.2 | §39.3 | Structured diagnostic preservation |
| §26.3.2 | §§23.10–23.13, 24.4–24.5 | Diagnostic backing lifetime/accounting |
| §26.3.2 | §§26.4, 25.7.1 | Failed current-output nonconsumption |
| §26.3.2 | §§31.9–31.10 | External delivery versus internal handoff |
| §26.3.2 | §26.3.1 | Execution-wide success barrier |
| §26.3.2 | §§39.1.3–39.1.4 | Failure consequences and retry admission |
| §26.3.2 | §39.1.7 | Original causal error and cleanup-error preservation |
| §26.9 | Chapter 32 | Worker-count and scheduling owner |

Verification was consulted read-only for **Pipeline Finalization and Resource Tests** and **Parallel Execution Tests**. No methodology was imported into Architecture, and no coverage synchronization was performed.

## Regression and document-quality assessment

| Protected contract | Result |
|---|---|
| D26-S1 | Unchanged: output before terminal; `FINISHED` carries no output; cardinality is not EOS; terminality remains local and monotonic |
| D26-S2 | Unchanged: acceptance, continuation, readiness, release versus resolution, progress, multiplicity, failure/retry, and sink acknowledgment |
| D20-B1 | Demand remains semantic; no unnecessary fetch to finish a protocol |
| D20-B2 | Executable scalar order unchanged |
| LogicalLimit / EXISTS | Valid early-stop owners unchanged; no forced source exhaustion |
| Bag/order semantics | No row order, multiplicity change, or physical ordering key introduced |
| D21-S4 | Eligible DML candidate selection preserved; no D25 pre-ranking |
| D21-S5 / RETURNING | Publication and statement/transaction distinctions unchanged |
| Chapter 22 | Immutable plan and mutable runtime ownership unchanged |
| Chapter 23 | Capacity/cardinality, empty-batch semantics, stable borrowing, and reset restrictions unchanged |
| Chapter 24 | Accounting, resource progress, categories, and cleanup unchanged |
| D25-S1 | Canonical minimum unchanged; physical discovery order excluded |
| Failed Evaluate | Failed current result remains nonconsumable |
| Chapter 31 | Internal handoff, external delivery, returned lifetime, and successful completion remain distinct |
| §39 | Categories, causal diagnostics, transaction consequences, and internal-failure consequences unchanged |
| Persistence/transactions | No format, identity, commit, rollback, retry-admission, or transaction-semantic change |

Normative wording makes existing owner constraints explicit; it introduces no new scheduler, state enum, return type, queue, error accumulator, synchronization primitive, or mandatory finalizer shape.

Pull, callback, state-based, retained-input, copied/buffered, and parallel local-state implementations remain permitted. The rationale explains the relevant correctness boundaries without prescribing implementation procedures.

### Reread answers, questions 1–121

Each range below gives the answer to **every question in that range**.

| Questions | Answer |
|---|---|
| 1–12 — D26-S1 | YES |
| 13–35 — D26-S2 | YES |
| 36–55 — M26-1 lifecycle | YES |
| 56–78 — M26-2 error handoff | YES |
| 79–84 — N26-1 temporality | YES |
| 85–93 — N26-2 role distinction | YES |
| 94–99 — Any documentation-role leakage? | NO |
| 100–103 — Analytical, timeless, implementation-neutral, precise ownership? | YES |
| 104 — New frozen semantic question? | NO |
| 105–116 — Findings closed, decisions preserved, Architecture clean? | YES |
| 117 — Verification synchronized? | NO |
| 118 — Chapter 26 fully closed? | NO |
| 119 — Chapter 27 review started? | NO |
| 120 — Phase 2 started? | NO |
| 121 — Phase 2 authorized? | NO |

## Final status and next action

| Item | Status |
|---|---|
| M26-1 | CLOSED |
| M26-2 | CLOSED |
| N26-1 | CLOSED |
| N26-2 | CLOSED |
| D26-S1 / D26-S2 | Remain CLOSED |
| Q26-1 / Q26-2 | Remain CLOSED |
| B26-1 / B26-2 | Remain CLOSED |
| Frozen Chapter-26 semantic questions | NONE |
| Chapter-26 Architecture | CLEAN |
| Chapter-26 Verification | SYNCHRONIZATION PENDING |
| Chapter 26 fully closed | NO |
| Chapter-27 direct review | NOT STARTED |
| Phase 2 | NOT STARTED / NOT AUTHORIZED |

Task-created hunks map to the authorized classes:

- **A–L:** lifecycle, ownership, completion, dependencies, finalization, cleanup, quiescence, retry, and internal misuse.
- **M–V:** error owners, candidate transport, output/publication distinctions, diagnostics, and success barrier.
- **W–Y:** chronology removal and sink/breaker distinction.
- **Z–AC:** precise references, terminology, rationale, and Markdown wrapping.

**Next task: CHAPTER-26 VERIFICATION SYNCHRONIZATION.** It was not performed here.

No implementation, build, test, sanitizer, benchmark, staging, commit, devlog, or review-artifact creation occurred.