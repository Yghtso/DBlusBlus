# D26-S1/D26-S2 — SEMANTIC INTEGRATION COMPLETE

Integrated both approved decisions into Chapter 26 only. **D26-S1, D26-S2, Q26-1, Q26-2, B26-1, and B26-2 are CLOSED.**

No new frozen cross-owner semantic conflict was found.

## Scope and Git state

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | `M docs/ARCHITECTURE.md` |
| Index | Clean | Clean |
| HEAD | `c06b29e16a6c9d55dcdd4dd8784f298e4b83bf48` | Unchanged |
| Architecture diff | None | 191 insertions, 3 deletions |
| `git diff --check` | — | Passed |

Only `docs/ARCHITECTURE.md` was task-modified. There were no pre-existing working-tree changes; no unexpected external changes were observed.

Historical review artifacts remained unread, unmodified, unmoved, and unstaged.

## Exact sections modified

| Section | Integration |
|---|---|
| [§26.4 Runtime interfaces](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20733) | Conceptual protocol and API freedom |
| [§26.4.1 Output availability and terminal completion](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20744) | New subsection integrating D26-S1 |
| [§26.4.2 Accepted-input, continuation, and progress lifecycle](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20808) | New subsection integrating D26-S2 |
| [§26.4.3 Sink acceptance](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20918) | Complete successful sink acknowledgment |
| [§26.5 Global and local state](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20944) | Continuation-state ownership and accounting |
| [§26.6 Borrowed-data lifetime](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20970) | Release/resolution versus physical reset |
| [§26.8 Pipeline early stop](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21015) | Safe input resolution and final-output handoff |

Chapters 1–25 and Chapter 27 onward were verified byte-identical to HEAD. Within Chapter 26, §§26.1–26.3, 26.7, 26.9, and 26.10 also remain unchanged.

## D26-S1 result

The integrated source and streaming-operator convention is:

```text
final output available
    → downstream consumption or valid ownership handoff
    → terminal completion with no consumable output
```

| Obligation | Integrated result |
|---|---|
| Output availability | Explicit semantic transition, independent of cardinality |
| `HAVE_MORE` | Nonterminal output path; may be empty with finite progress |
| `FINISHED` | Local source completion; no consumable output |
| Reusable terminal output storage | May exist physically; contents are not that invocation’s result |
| Empty source | May finish immediately without a fake empty batch |
| Empty, short, or full batch | None establishes EOS, finality, or pending terminality |
| Final nonempty batch | Offered before terminal completion |
| Terminal monotonicity | No further logical output from the completed execution state |
| Post-terminal caller behavior | Driver must not request more data; request is outside the valid protocol |
| Rewind/repeated FINISHED | Neither rewindability nor idempotent repeated calls is required |
| Streaming operators | Same output-before-terminal convention |
| Early-stop final output | Readiness withheld immediately; terminal-control transition fetches no upstream input |
| Local completion | Does not establish query/statement success, transaction completion, or required finalization |
| Cursor EOS | Remains Chapter-31-owned |

The text freezes ordered semantic transitions, not two mandatory C++ calls.

## D26-S2 result

| Obligation | Integrated result |
|---|---|
| Acceptance | Each submitted logical input-occurrence domain is accepted at most once |
| Per-local-state admission | At most one unresolved accepted-input lifecycle |
| Continuation | Resumes existing acceptance or derived state; never reaccepts input |
| Physical buffer reuse | Same object may carry a distinct submission after lifecycle and borrow requirements permit reuse |
| Semantic identity | No pointer-based identity or new row/chunk/pipeline semantic ID |
| Lifecycle resolution | Required processing/output obligations discharged, or remainder safely unnecessary under owning early-stop semantics |
| Copying input | Does not itself resolve the lifecycle |
| Operator release acknowledgment | Original backing no longer needed after dependence ends or is independently preserved |
| Release before resolution | Explicitly allowed |
| Pending output after release | Allowed when required state is independently preserved |
| Downstream borrowing | May outlive lifecycle resolution |
| Physical reset | Requires ended operator/downstream dependencies or exact preservation of every required view |
| Retained-input continuation | Allowed with value-stable backing |
| Copied/buffered continuation | Allowed; growing storage remains accounted |
| One input → multiple outputs | Explicitly supported, including probe continuation |
| One input → zero occurrences | Successful resolution and valid progress; not EOS or failure |
| Multiplicity | Required output occurrences offered exactly once; no generic duplication, dropping, reacceptance, or replay |

New-input admission requires all applicable conditions:

```text
preceding accepted-input lifecycle resolved
no continuation must precede new input
outstanding output handoff completed
operator not terminal
valid early stop has not forbidden input
```

### Progress, failure, and sink acknowledgment

| Area | Integrated result |
|---|---|
| Output-free nonterminal step | Resolves input, releases operator dependence, or advances finite relevant continuation state |
| No-progress loop | Equivalent-state repetition without relevant advancement is an internal protocol/liveness violation |
| Progress mechanism | No timer, sleep, numeric counter, or mandatory suspension enum |
| Resource progress | §24.6 remains separate; neither progress model excuses violations of the other |
| Early stop | Safely unnecessary remainder need not be processed merely to acknowledge consumption; cleanup remains required |
| Terminal failure/cancellation | Pending continuation is cleaned as failed state, not resumed successfully |
| Retry | Chapter-21-authorized retry uses fresh attempt-local state |
| Error candidate discovery | Does not itself authorize discarding required candidate-establishing work |
| Error selection | D25-S1/D21-S4 remain owners; no first-physical-error rule |
| Failed invocation output | Not successful consumable output; independently completed output transitions remain distinct |
| Successful Sink call | Accepts the complete submitted domain exactly once |
| Partial sink success | No generic partial-success acknowledgment |
| Failed/incomplete Sink call | Not complete successful acceptance; no blind replay |
| Sink retention | Requires stable ownership |
| Sink failure effects | No claim that failure means no physical work occurred |
| Sink success | Not query/statement success, Combine/Finalize success, commit, or client publication |

## Owner and regression audit

| Owner | Result |
|---|---|
| D20-B1 | Demand unchanged; no protocol-driven visible unnecessary work |
| D20-B2 | Executable scalar order unchanged |
| LogicalLimit / EXISTS | Existing demand and safe early-stop ownership preserved |
| Bag/order semantics | No generic cardinality equality or SQL ordering introduced |
| D21-S4 | Candidate eligibility and ranking unchanged |
| D21-S5 | RETURNING bag and publication envelope unchanged |
| Chapter 22 | Mutable continuation remains execution/local state, outside immutable plans |
| Chapter 23 | Capacity/cardinality, active domain, empty-batch distinction, borrowing, and reset rules unchanged |
| Chapter 24 | Accounting, resource progress, classifications, and cleanup unchanged |
| D25-S1 | Candidate domain, preorder, equivalence, and error-selection ownership unchanged |
| Failed Evaluate | Nonpublication rule unchanged |
| Chapter 31 | Internal handoff remains distinct from external publication; returned-prefix rules unchanged |
| §39 | Resource/cancellation categories, internal-state consequences, and transaction effects unchanged |
| Persistence/transactions | No format, identity, retry-admission, commit, or transaction-semantic change |

No external owner required editing.

## Implementation freedom and document quality

The integrated text permits pull, push/callback, state-based, retained-input, and copied/buffered realizations. It requires no particular return type, enum, continuation structure, callback ABI, coroutine, future, queue, or scheduler algorithm.

Per-local-state admission does not serialize independent workers or pipelines. Chapter-23 legal capacities remain unchanged.

Rationale explains final-batch safety, cardinality versus completion, release versus resolution, borrow-safe reset, and finite continuation progress.

New text contains no project chronology, implementation-status narration, development sequencing, or Verification procedure. Existing N26-1 chronology was deliberately preserved; the N26-2 heading was not renamed.

## Reread answers 1–130

Every question in each range was checked individually.

| Questions | Result |
|---|---|
| 1–18: D26-S1 | **YES all** |
| 19–27: Acceptance | **YES all** |
| 28–39: Lifecycle/release | **YES all** |
| 40–53: Continuation/readiness | **YES all** |
| 54–63: Progress | **YES all** |
| 64–71: Early stop | **YES all** |
| 72–83: Sink | **YES all** |
| 84–95: Error/retry | **YES all** |
| 96–112: Owner preservation/regression | **YES all** |
| 113–119: Decisions/questions/blockers closed; no frozen questions | **YES all** |
| 120–123: M26-1/M26-2/N26-1/N26-2 remain open | **YES all** |
| 124: Chapter 26 semantically clean | **YES** |
| 125: Document/integration clean | **NO** |
| 126: Verification synchronized | **NO** |
| 127: Chapter 26 fully closed | **NO** |
| 128: Chapter-27 review started | **NO** |
| 129: Phase 2 started | **NO** |
| 130: Phase 2 authorized | **NO** |

## Diff ownership and closure

Task-created hunk classes:

| Classes | Content |
|---|---|
| A–F | Output/terminal distinction, FINISHED, final batch, cardinality, monotonicity, uniform convention |
| G–M | Acceptance, local-state limit, continuation, resolution, backing release, pending work, readiness |
| N–S | Borrow/reset composition, multi-output, empty success, progress, early stop, multiplicity |
| T–U | Minimal failure/retry and candidate-discovery boundaries |
| V–W | Complete sink acknowledgment, retention, no blind replay |
| X–Y | Technical rationale and associated Markdown wrapping |

No full M26-1 lifecycle cleanup, full M26-2 error integration, N26-1 chronology cleanup, or N26-2 terminology cleanup was performed.

| Item | Status |
|---|---|
| D26-S1 / D26-S2 | **CLOSED** |
| Q26-1 / Q26-2 | **CLOSED** |
| B26-1 / B26-2 | **CLOSED** |
| Frozen Chapter-26 semantic questions | **NONE** |
| M26-1 / M26-2 | **OPEN — nonsemantic integration** |
| N26-1 / N26-2 | **OPEN — document-only** |
| Chapter-26 Architecture | **SEMANTICALLY CLEAN; NOT YET DOCUMENT/INTEGRATION CLEAN; NOT FULLY CLOSED** |
| Chapter-26 Verification | **Synchronization pending; unchanged by this task** |
| Chapter-27 review | **NOT STARTED** |

Next task: **TARGETED CHAPTER-26 DOCUMENT / INTEGRATION CLEANUP**.

No implementation, build, tests, sanitizers, benchmarks, staging, commit, devlog, or review-artifact creation occurred. **Phase 2 remains NOT STARTED / NOT AUTHORIZED.**