# Chapter 32 — Parallel Execution and Scheduling Verification

This family verifies Chapter 32's scheduler and parallel-runtime contracts without choosing
a production queue, synchronization primitive, or tracing implementation. It reuses the
operator-specific oracles in V26–V30 and adds the source-domain, morsel-claim, completion,
and cross-operator integration checks that those component families do not own.

Every required V32 observation may be supplied by deterministic test hooks, trace events,
or equivalent inspectable state. If a procedure cannot observe an event on which its verdict
depends, its result is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, not `PASS`.

## V32-A — Source-domain, claim, and event oracle

Use one trace schema for the V32 family. Each record contains:

```text
monotonic event ordinal
source/operator instance identity
execution-attempt identity
required algorithmic-pass identity
morsel identity
exact work-domain identity or range
worker/current claim-owner identity, if any
event category and outcome
logical-occurrence identity, for acceptance events
completion/readiness state
resource/cleanup owner when relevant
```

The required event categories are `source-domain-established`,
`required-domain-established`, `partition-created`, `morsel-domain-assigned`,
`claim-attempted`, `claim-succeeded`, `claim-rejected`, `occurrence-accepted`,
`morsel-completed`, `morsel-canceled`, `morsel-failed`,
`source-completion-attempted`, `source-completion-succeeded`,
`source-completion-failed`, `finalize-entered`, `finalize-completed`, and
`dependent-ready` or `dependent-blocked`. Equivalent names are acceptable when these
distinctions and their ordering remain reconstructable.

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-001 | Establish one source/operator instance, attempt, and pass with distinguishable required units and occurrences. | The canonical operator/demand owner establishes the required domain before partitioning; the scheduler cannot add, omit, or redefine semantic demand. |
| V32-002 | Run one nonempty morsel through claim, acceptance, and successful completion. | The trace correlates the same source, attempt, pass, morsel domain, owner, accepted occurrences, and completion in event order. |
| V32-003 | Force one worker to lose a claim while another wins it. | Both attempts are visible, exactly one success is visible, and the losing worker has no acceptance or completion event for that claim. |
| V32-004 | Disable or suppress each required event class in turn in the verification adapter. | The affected test reports `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`; the positive V32-002/V32-003 controls prevent an empty trace from passing. |

## V32-B — Complete and disjoint source coverage

Partition checks compare normalized source-domain units, not SQL output rows. For a source
whose current demand requires complete consumption, compute the union and pairwise
intersections of all assigned morsel domains and compare the accepted-occurrence ledger with
an independently enumerated required-occurrence model.

| ID | Setup and controlled fault | Required observation and outcome |
|---|---|---|
| V32-005 | Produce a valid partition over several distinguishable required units, including one unit with no visible occurrence and one with several occurrences where the source permits it. | The union equals the required domain, pairwise intersections are empty, and every required logical occurrence is accepted exactly once. Output cardinality is not used as the coverage oracle. |
| V32-006 | Omit one known-required domain unit by suppressing its morsel creation. | The hook proves the unit belonged to the required domain, the coverage ledger identifies the exact gap, source completion fails, complete-input Finalize cannot succeed, and dependents remain blocked. Use the existing invariant/failure owner; do not invent a SQL error code. |
| V32-007 | Create two morsels with a known overlapping domain containing distinguishable occurrences. | Partition validation or acceptance accounting identifies the overlap; both domains cannot become successful owners of the shared work, duplicate acceptance cannot produce success, and any produced duplicate rows are diagnostic evidence rather than an accepted result. |
| V32-008 | Attempt source completion with an unassigned, multiply assigned, or unresolved required unit. | No successful source-completion event, complete-input Finalize, or successful dependent readiness is permitted. The test fails setup if the unit was not actually required. |

## V32-C — Exclusive claims and scheduling independence

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-009 | Place two workers at a barrier before they claim the same available morsel; release them together and repeat with reversed scheduling bias. | Exactly one claim linearizes successfully. The winner is the only current execution owner; the loser cannot accept an occurrence or record completion. |
| V32-010 | Delay the winning worker before completion, then attempt a second claim and two completion records. | The active claim cannot disappear or be concurrently reacquired, and at most one successful completion is recorded. Departure must lead to an owned failure/cancellation outcome rather than silent loss. |
| V32-011 | Run the same immutable plan and source with one, two, and several workers; vary morsel sizes, claim order, and worker speed. | Each attempt/pass has the same required-domain membership, exactly-once accepted-occurrence multiset, snapshot visibility, canonical result, and prescribed error behavior. Where dynamic-failure arbitration permits several outcomes, each must belong to the owner-defined family; worker IDs, chunking, claim sequence, and unordered presentation may differ. |
| V32-012 | Feed an unordered parallel scan directly and through an ordering-providing plan. | The scan advertises no SQL ordering; only the ordering-preserving/finalized owner establishes the required order. Scheduling order is never the SQL-order oracle. |

## V32-D — VALUES and heap page-range integration

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-013 | Partition a bound PhysicalValues source into adjacent ranges containing equal-valued but occurrence-distinct rows. | Range union is complete and disjoint; occurrence identity, not value equality, proves every required VALUES occurrence is accepted once. No scheduling-derived SQL order is asserted. Reuse `V27-L — PhysicalValues`. |
| V32-014 | Omit and then overlap a VALUES range; separately include a zero-length range. | Omission and overlap prevent successful complete coverage; the zero-length range owns no occurrence and neither duplicates work nor hides a nonempty gap. |
| V32-015 | Partition a SeqScan required page domain into several ranges and perturb range and worker order. | `V27-R04` and V32-B jointly prove complete, gap-free, non-overlapping page-domain ownership and exactly-once candidate traversal for the current scan pass. |
| V32-016 | Pause after a page-range claim, during tuple visibility evaluation, and during worker handoff. | All workers use one transaction identity, statement snapshot, and CommandId; page guards, read epoch, and RID protection remain valid through their owning use. A claim does not substitute for those lifetime owners. Reuse V27-B–V27-D and V27-O. |
| V32-017 | Suppress one required page range, and separately allow a complete scan whose visible rows are empty. | The missing required range prevents successful scan completion and dependent readiness; the fully covered no-visible-row scan may succeed with an empty bag. Parallel SeqScan fabricates no ordering or row. |

## V32-E — Spill domains and repeated algorithmic passes

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-018 | For a forced-spill join, aggregate, or sort pass, enumerate required partition/run segments and assign morsels. | Within that pass the domains cover exactly the required work without overlap, handles/backing remain valid, and each required occurrence or run segment is accepted under the owning V28, V29, or V30 protocol. |
| V32-019 | Omit one required spill segment, then create overlapping segment ownership. | The exact missing/overlapping range is observed; complete pass/Finalize/readiness cannot succeed, and no partial result is accepted. Reuse V24-J/K/M for framing, namespace, lifetime, and cleanup. |
| V32-020 | Execute legitimate hash partition, aggregate repartition, sort run-generation, and merge passes that reread physical backing. | Each pass has a distinct pass identity and complete/disjoint ledger. A physical page/run may be read in different required passes; duplicate acceptance inside one pass remains forbidden. |
| V32-021 | Cancel or inject the owning SpillIO failure during claimed spill work and, separately, a later Chapter-30 Source read. | The claimed morsel/pass does not complete falsely; handles/accounting clean under V24-M and operator owners. The later SELECT/Sort read remains a V30-G Source failure and is not DML result publication `R`. |

## V32-F — Optional splitting and coalescing

Run these procedures only when the implementation supports the corresponding optional
operation. Absence of dynamic splitting/coalescing is not a failure; the supported static
partition must still pass V32-B.

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-022 | Split an unclaimed parent morsel at deterministic boundaries. | Child-domain union equals the parent, children are pairwise disjoint, the parent is no longer separately consumable, and final acceptance covers each required occurrence once. |
| V32-023 | After a worker has accepted a known prefix, attempt to split or reassign the original whole domain. | Previously accepted work cannot be silently reassigned or accepted again. Any allowed remainder transfer has an exact non-overlapping domain and explicit owner transition; otherwise the owning attempt fails. |
| V32-024 | Coalesce several adjacent available morsels. | The merged domain covers every constituent unit exactly once, no child remains independently claimable, and neither omission nor duplicate acceptance occurs. |

## V32-G — Empty domains, empty morsels, and zero output

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-025 | Use an empty required source domain and its empty partition. | Source completion may succeed with no claims, acceptances, or fabricated output; the empty required-domain oracle, not absence of trace instrumentation, proves the case. |
| V32-026 | Include and claim a represented zero-length morsel beside valid nonempty morsels. | It owns and accepts no occurrence, may complete without output, and cannot cover or duplicate another unit. |
| V32-027 | Use nonempty morsels with no visible tuples, all rows rejected by Filter, and an owner-valid no-output operator path. | Domain claim and successful consumption remain observable even though no output chunk is produced; complete coverage may succeed without a fabricated row. Reuse V26-D and the Scan and Unary Operator Tests. |
| V32-028 | Use join and aggregate fixtures whose input/output cardinalities differ. | Exactly-once source acceptance is checked independently from zero/one/many join outputs and aggregate group counts; one-output-row-per-input is never an oracle. |

## V32-H — Worker failure, cancellation, and unsafe replay

For every injected condition, record claim owner, accepted prefix, emitted output/shared
state effects, completion state, source/stage outcome, worker quiescence, downstream
readiness, and cleanup owner. V26-I/K and Chapter 39 determine the exposed error and
transaction consequence.

| ID | Setup and controlled point | Required observation and outcome |
|---|---|---|
| V32-029 | Fail a worker before a claim and immediately after a successful claim but before acceptance. | Unclaimed work remains accounted as required; a claimed failure is not completion. The query follows its failure/quiescence owner and cannot report complete success. |
| V32-030 | Fail after a distinguishable accepted prefix and after emitted output. | The prefix/effects are recorded, the morsel is not successful, and the whole morsel is not invisibly replayed into a successful duplicate result. No physical undo is invented. |
| V32-031 | Fail after updating shared aggregate state and during spill I/O. | Partial shared/spill state is not finalized or published as complete; successors remain blocked and V24-M, V26-K, and V29-Q/V30-H own quiescence and cleanup. |
| V32-032 | Fail after local work ends but before successful completion recording. | Completion uncertainty cannot be guessed as success or solved by blind whole-morsel replay. The existing attempt fails unless an explicit exact, attempt-isolated canonical retry owner proves otherwise. |
| V32-033 | Cancel before/during claim, after claim, after partial acceptance, after output, while holding a read epoch, and while another worker runs. | No canceled morsel is marked successful; no unnecessary new work is scheduled; workers quiesce before query-owned backing/epochs are released; transaction locks remain with their owner. Full source consumption is not required after legitimate cancellation. |
| V32-034 | Attempt to return a partially accepted, output-producing, shared-state-updating, or mutation-related morsel to the available queue as untouched. | The trace detects prior acceptance/effects and rejects silent replay. A fresh retry is allowed only under V26-L and its canonical owner with discarded attempt-local state; same-TxnId DML retry after `W` and multiple mutation publishers remain forbidden. |

## V32-I — Completion, Finalize, dependencies, and early stop

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-035 | Attempt source completion with, in turn, one required morsel unclaimed, actively claimed, failed, canceled, multiply owned, or absent from the partition. | Source completion fails or remains pending; no complete-input Finalize or successful dependent readiness occurs. |
| V32-036 | Place barriers before final source completion, during Combine/Finalize, and before dependent publication; queue a successor early. | V26-G and Pipeline Finalization and Resource Tests show the successor non-runnable and partial shared state unpublished until successful predecessor Finalize. A failed Finalize never decrements readiness as success. |
| V32-037 | Complete every required morsel exactly once, then run Combine/Finalize. | The coverage precondition is satisfied and Chapter 26 may perform the single canonical successful Finalize/readiness transition; Chapter 32 adds no second Finalize authority. |
| V32-038 | Exercise an owner-authorized early-stop or pruned-demand plan under V26-N and its operator owner. | The trace records the demand-domain change or early-stop authority; all work still required is covered, work no longer demanded does not block success, and no false missing-morsel failure occurs. |
| V32-039 | Suppress a required morsel without an owner-authorized early-stop event. | The scheduler cannot relabel the gap as pruning; incomplete coverage is detected and successful completion/Finalize/readiness are forbidden. |

## V32-J — Parallel hash-join integration

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-040 | Partition build input across workers, including duplicate and NULL keys, and barrier before final build partition completion. | Build occurrences are completely and exactly accepted; V28-H keeps probe tasks blocked until required build partitions Finalize and publishes immutable state. |
| V32-041 | Probe finalized state with varied worker count, morsel size, continuation boundaries, and one probe row producing multiple chunks. | Probe occurrences are accepted exactly once, continuation is worker-local, directory state is immutable, and V28-B/C/I preserve exact join-bag multiplicity. |
| V32-042 | Exercise LEFT JOIN matched/unmatched rows, NULL keys, and duplicate matches under reordered claims. | V28-J's matchedness oracle yields each required unmatched extension exactly once and preserves many-match multiplicity; worker scheduling cannot lose or duplicate preserved-side results. |
| V32-043 | Fail/cancel during build, Finalize, probe, and Grace spill after retaining VARCHAR values. | Probe cannot observe unfinished build state; V23-G/H, V28-K/R/S, and V26-K prove backing lifetime, no dangling worker-local pointer, blocked dependents, and complete cleanup. |

## V32-K — Parallel aggregate integration

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-044 | Partition distinguishable aggregate input occurrences across worker-local states, including empty and all-NULL groups. | Every required input occurrence contributes exactly once; no duplicate/missing contribution is hidden by a plausible group count. Reuse V29-B/C. |
| V32-045 | Vary worker count, partitioning, Combine order, and merge-tree shape; barrier before all local states complete. | Local state has one owner, V29-K Combine consumes only complete states, and Finalize/readiness waits for required inputs. Integer/count and MIN/MAX results remain exact. |
| V32-046 | Inject worker error, overflow/resource failure, and cancellation before Combine, during Combine, and before Finalize. | V29-L/M/Q selects the canonical error, publishes no successful partial group prefix, blocks successors, and destroys/account-cleans every local/global state once. |
| V32-047 | Run FLOAT64 SUM/AVG with varied legal reduction trees. | Each result is independently reproducible by an admitted V29-H/I tree over exactly the required occurrences with exact AVG count and flags; bitwise equality across different legal trees is not required. |

## V32-L — Parallel sort integration

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-048 | Generate local runs from distinguishable input occurrences under varied worker/morsel partitions. | The run-input ledger is complete and exactly once, every run is internally ordered under V30-A, and no run becomes ready from incomplete source work. |
| V32-049 | Force in-memory and spilled runs, multiple merge passes, and barriers before run and merge Finalize. | V30-F/H and V32-E prove complete pass domains, valid run ownership/backing, and readiness only after successful required finalization. |
| V32-050 | Emit final output through the merge source while workers finish in adversarial order. | Final output obeys the semantic comparator and declared OrderingProperty; arbitrary worker interleaving cannot replace the order-preserving owner, and permitted ties use V30-B's outcome family. |
| V32-051 | Cancel or inject a later spill-read failure after readiness and, separately, while generating runs. | Each failure remains at its V30-G/H owner, cleanup is complete, and a later SELECT Source failure is not confused with Chapter-31 DML `R` or delivery. |

## V32-M — Snapshot, resources, lifetime, and error ownership

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-052 | Hold workers at claim, acceptance, handoff, and completion barriers while inspecting execution context. | Every worker observes one transaction identity, statement snapshot, CommandId, read-epoch policy, and cancellation state; terminal transaction transitions remain outside arbitrary read workers. |
| V32-053 | Allocate worker-local and shared chunks, RowCollections, aggregate tables, sort runs, spill buffers, and retained VARCHAR/dictionary backing; fail one worker while others are active. | V23-G/H/K, V24-B–D/F/M, and V26-M show continuous accounting, valid transfer, no dangling local backing, no double release, and no teardown before worker quiescence. |
| V32-054 | Cause concurrent worker failures of different canonical classes and vary which reports first. | V26-I/K and Chapter 39 preserve each owner-defined arbitration and transaction consequence; dynamic failures are not source-span ranked and corruption/invariant failures are not flattened into ordinary errors. |
| V32-055 | Evaluate DML ordinary candidates in parallel under varied visit/worker order, then inject a distinct dynamic failure in a separate case. | `V31-B — Ordinary DML candidate closure and provenance` and `V31-G — READ COMMITTED retry and failure-class separation` preserve canonical ordinary reduction before `W`; dynamic failure keeps its actual owner and boundary. No worker visitation order becomes DML semantics. |

## V32-N — Control-operator and optional-policy noninterference

| ID | Setup and action | Required observation and outcome |
|---|---|---|
| V32-056 | Parallelize only the permitted scan/materialization/evaluation portion of INSERT, UPDATE, and DELETE and attempt mutation from an evaluation worker. | The attempt is rejected as nonconforming; V31-A, V31-B, and V31-G retain one mutation/write publisher, ordinary closure before `W`, W/C/R ownership, and retry boundaries. |
| V32-057 | Exercise DDL, VACUUM, and ANALYZE alongside permitted helper work and table-level concurrency. | `Control-Operator Tests`, `Vacuum and Reclamation Tests`, `Statistics Publication and Versioning Tests`, and V31-N preserve the DDL coordinator, one VACUUM pass owner with its table rules, and one ANALYZE publication coordinator. |
| V32-058 | When supported, run with and without work stealing, NUMA placement, explicit SIMD kernels, prefetch, and multi-query scheduling controls. | Values, NULL behavior, visibility, required ordering, accepted-occurrence multiplicity, error ownership, and lifetime remain within the same canonical outcome family. Unsupported optional policies are not failures and no numerical fairness/performance threshold is invented. |
| V32-059 | Configure a worker-pool size, submit more runnable query/pipeline tasks than that size, and observe worker creation and task ownership through completion and cancellation. | Execution uses only the configured fixed pool, creates no arbitrary thread per query/pipeline, and releases each task/query reference only after worker departure is accounted for. |
| V32-060 | Run a long task with morsel/task boundaries, queue another runnable task in the same query and, when supported, another query; inject cancellation at each defined boundary. | The architecture's yield/cancellation control points remain observable and cancellation makes progress without inventing a numeric fairness bound or requiring an optional multi-query policy. |

## Chapter 32 subsection coverage map

| Architecture subsection | New V32 coverage | Exact reusable coverage |
|---|---|---|
| §32.1 Worker model | V32-002–V32-004, V32-011, V32-052, V32-059 | `Parallel Execution Tests`; V26-K/Q |
| §32.2 Morsels | V32-001–V32-039 | V26-B–V26-D, V26-K/L/N/Q; V27-R04 |
| §32.3 Worker-local state | V32-010, V32-041, V32-045, V32-053 | `Parallel Execution Tests`; V23-G/H/K; V26-M; V29-K |
| §32.4 Parallel sequential scan | V32-015–V32-017, V32-052 | V27-B–V27-D, V27-O, V27-R04; `Scan and Unary Operator Tests` |
| §32.5 Parallel hash join | V32-040–V32-043 | V28-B/C/H/I/J/K/R/S/T; `Hash Join Tests` |
| §32.6 Parallel hash aggregate | V32-044–V32-047 | V29-B/C/H/I/K/L/M/Q/R; `Aggregate Tests` |
| §32.7 Parallel sort | V32-018–V32-021, V32-048–V32-051 | V30-A/B/F/G/H/K; `Sort Tests` |
| §32.8 Scheduler/dependencies | V32-009–V32-010, V32-029–V32-039 | V26-G/K/N; `Pipeline Finalization and Resource Tests` |
| §32.9 Fairness/control points | V32-011, V32-033, V32-058, V32-060 | V26-K/Q; `Parallel Execution Tests` |
| §32.10 NUMA policy | V32-053, V32-058 | V24-A–V24-D, V24-N |
| §32.11 SIMD/hot loops | V32-011, V32-047, V32-058 | V23-C–V23-E, V23-L; V25-F, V25-G, V25-P |
| §32.12 Prefetch | V32-016, V32-052–V32-053, V32-058 | V27-C/D/O; V23-G/K |
| §32.13 invariants | V32-001–V32-060 | V26-S, V27-T, V28-V, V29-U, V30-N; `V31 atomic architecture-obligation ledger` |

## Chapter 41 Chapter-32 obligation coverage map

| Architecture owner and obligation | Procedure and interleaving/fault | Expected observable outcome |
|---|---|---|
| §41.3 failure/cancellation and transaction ownership | V32-029–V32-034, V32-052, V32-054–V32-056; fail/cancel before and after claim/acceptance/effects | No false successful coverage, unsafe replay, arbitrary terminal transition, or post-`W` DML retry; workers quiesce and the canonical error owner remains visible. |
| §41.5 single-worker/parallel equivalence | V32-011–V32-012; vary workers, ranges, ordering, and speed | Same required occurrences, visibility, canonical result/error family, and only owner-permitted presentation/tree variation. |
| §41.5 physical-plan/pipeline legality | V32-001, V32-035–V32-039; establish demand and barrier completion/Finalize | Scheduler consumes the canonical required domain; incomplete predecessors do not Finalize or publish readiness; canonical early stop remains valid. |
| §41.5 scan and occurrence conservation | V32-005–V32-017; omit/overlap/dual-claim page and VALUES ranges | Complete/disjoint domains, one claim owner, exactly-once occurrence acceptance, stable snapshot/CommandId/read epoch, and no fabricated order. |
| §41.5 string/chunk lifetime | V32-016, V32-043, V32-052–V32-053; pause handoff and retire local state | Retained backing outlives use, no page/local pointer dangles, and cleanup waits for quiescence. |
| §41.5 join obligations | V32-040–V32-043; build/probe barriers, LEFT/NULL/multiplicity, spill failure | Probe waits for immutable finalized build state and preserves exact join bags and unmatched multiplicity. |
| §41.5 aggregate obligations | V32-044–V32-047; vary workers/trees and inject Combine/Finalize failure | Exact occurrence contribution and exact states; admitted FLOAT64 tree result only; no partial publication. |
| §41.5 sort obligations | V32-018–V32-021, V32-048–V32-051; omit run work, multi-pass merge, later read failure | Complete pass coverage, readiness after successful Finalize, ordered merge output, and owner-correct failure/cleanup. |
| §41.5 forced-spill/resource cleanup | V32-018–V32-021, V32-031–V32-033, V32-053; fail spill/worker/cancel | Continuous accounting, one cleanup owner, no leaked handle/run, and no resource destruction before quiescence. |
| §§41.3/41.5 DML/control regressions | V32-055–V32-057; vary DML evaluation workers and attempt unauthorized publication | Canonical ordinary-error reduction and one DML/DDL/VACUUM/ANALYZE publication owner remain intact. |
| §32 optional-policy semantic noninterference | V32-058; compare supported policy-on/off executions | Optional scheduling, placement, SIMD, and prefetch alter no canonical semantic or lifetime outcome. |
| §§32.1, 32.9 fixed-pool and control-point obligations | V32-059–V32-060; oversubscribe configured workers and cancel at task/morsel boundaries | No unbounded thread creation or stale task owner; required control points remain observable without a numerical fairness promise. |

## V32 negative-case consistency checks

The live Verification procedures must contain no successful complete-source result with a
missing, overlapping, failed, canceled, or unresolved required morsel; no dual successful
claim; no whole-morsel replay after accepted effects; no inference of coverage from output
row count; no one-output-row-per-input rule; no deduplication of equal-valued VALUES
occurrences; no global prohibition on valid repeated passes; no successful Finalize or
dependent readiness from partial predecessors; no claim substituted for snapshot/read-epoch
protection; no source-span ordering for dynamic failures; no parallel DML mutation
publisher; no unconditional FLOAT64 bit identity across admitted trees; and no mandatory
NUMA, SIMD, prefetch, work-stealing, or multi-query policy. An apparent hit must be read in
its source/attempt/pass and operator context before being classified as stale.
