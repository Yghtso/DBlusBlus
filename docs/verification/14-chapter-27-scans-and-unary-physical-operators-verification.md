# Chapter 27 — Scans and Unary Physical Operators Verification

This family verifies `ARCHITECTURE.md` Chapter 27. Architecture owns operator meaning; the
models below are test-side instruments. Verify each rule through a deterministic procedure and
independent oracle, directly or through V27-S.

Use these independent oracles throughout:

| Oracle | Independent construction |
|---|---|
| `HT` | Published relation-page and slot-state model ordered by `(PageNo,SlotId)` |
| `SV` | Byte-built §4.13/Chapter-5 structural-validity model, independent of production decoders |
| `MV` | Chapter-10 snapshot/status/command truth-and-error model |
| `BR` | Chapter-8 encoded-key, physical-key, range, and forward-cursor model |
| `RB` | Captured-RID state and Chapter-14 epoch/reuse-barrier model |
| `AP` | LogicalGet result bag plus required-order and path-local-failure classifier |
| `FO` | Chapter-17/20 TRUE-only Filter occurrence model |
| `PS` | Ordered physical-schema/LogicalSlotId and Project occurrence model |
| `LS` | Arbitrary-precision OFFSET-then-LIMIT sequence-slice model |
| `VO` | Bound typed Values occurrence relation |
| `BO` | V23-G/V26-M value-stable borrow-owner graph |
| `ST` | V26-A output-offer/handoff/terminal trace |
| `AL` | V26-C accepted-input/readiness/continuation automaton |
| `ER` | V25-J/V21-13 owner-selected error and provenance model |
| `PB` | V26-J/Chapter-31 internal/external publication history |
| `AF` | V26-L fresh-attempt generation ledger |
| `IX` | One-defect invalid-state model with forbidden-access/effect counters |
| `EQ` | Owner-defined value, NULL, bag, order, error, and transaction equivalence |

Production scans, codecs, visibility routines, operators, statuses, and result sinks are
observations, never expected-result generators. Generate fixed logical data, symbolic page
and index layouts, explicit status tables, finite traces, symbolic failures, and enumerated
worker-release orders. Use no sleeps, wall-clock races, allocator placement, pointer order,
filesystem enumeration, hash iteration, or arbitrary B+ split history as an oracle.

## V27-A — Operator inventory, owners, roles, and schemas

Instantiate every final operator and compare its declared plan contract to this inventory.
Poison runtime name lookup and plan mutation; all slot resolution must use the physical
schema. Mutable state gets a distinct attempt generation.

| Operator | Role/input | Output schema and occurrence transform | Mutable state / terminal rule | Order/expression owner |
|---|---|---|---|---|
| `PhysicalSeqScan` | source; resolved heap relation | declared scan slots; each surviving visible occurrence once | page/slot/output cursor; exact exhaustion/early stop | no property; Ch10/20/25 |
| `PhysicalIndexScan` | source; selected index range | same LogicalGet slots; each visible residual-TRUE candidate once | B+ cursor/RID batch/output; bound exhaustion/early stop | compatible Ch37 property; Ch8/10/25 |
| `PhysicalFilter` | streaming; child chunk | child schema; TRUE-only occurrence subset | expression/selection and AL; child terminal after pending output | preserves supplied sequence; Ch17/20/25 |
| `PhysicalProject` | streaming; child chunk | declared ordered output schema; one row per demanded input | expression/output and AL; child terminal after pending output | §37.5; Ch20/25 |
| `PhysicalLimit` | streaming; child stream | child schema; mathematical slice | skipped/emitted and AL; child exhaustion or satisfied limit | preserves supplied order; Ch19/20/26 |
| `PhysicalValues` | source; bound typed rows | declared Values schema and listed multiplicity | row/output cursor; source exhaustion | no independent SQL order; Ch19/20 |
| `PhysicalResultSink` | sink; final chunk | consumes complete submitted schema/domain | acceptance/retention; result-owner completion | preserves required sequence; Ch26/31 |

`PS` checks width, TypeId, declared column sequence, and each entry's LogicalSlotId. Vector
ordinal is only addressing; names, pointers, values, and numeric slot-ID order are rejected
as identity. Filter and Limit pass slots through, Project uses declared outputs, access-path
choice preserves scan outputs, and predicate-only/hidden slots are retained only where their
owner requires them. Final-plan static faults reuse V22-K/§38.24 rather than per-chunk full
plan validation.

## V27-B — Sequential-scan candidate traversal

Build heap files from independent page/slot fixtures. `HT` includes every published ordinary
heap page exactly once and orders candidate coordinates by ascending PageNo then SlotId;
FSM contents are independently randomized and never define the full-scan domain. Tag every
NORMAL occurrence, including equal-valued duplicates. Record each candidate visit and
require exact equality with `HT`: no page/slot omission or repetition.

| Slot/page case | Structural candidate? | Ordinary disposition | Progress/result | Oracle / status |
|---|---:|---|---|---|
| visible NORMAL | yes | visibility then qualification | emit once if surviving | HT/MV/FO |
| invisible/deleted/aborted NORMAL | yes | validate, then skip by MV | advance, no output | HT/SV/MV |
| UNUSED | no tuple | never dereference payload | advance | HT/SV/IX |
| retained DEAD | retained structure, not returnable | validate; no row return | advance | SV/RB |
| reclaimed DEAD | no tuple payload, not reusable by scan | never dereference | advance | SV/RB |
| REDIRECT_RESERVED/unknown state | invalid ordinary page | no guessed skip | controlled owner failure | SV |
| empty page | no candidates | valid page | finite page advance | HT/ST |
| malformed page/slot | invalid | no ordinary use | canonical failure | SV/IX |
Compare the same physical sequence under an unordered SQL plan only as an occurrence bag;
an exact `(PageNo,SlotId)` sequence is not an `OrderingProperty`.

## V27-C — Storage validation, decoding, and page lifetime

Reuse the byte-built generic header, HEAP_DATA, slot graph, and TupleCodec procedures. For
each accessed page, independently mutate checksum, PageId/owner/type, geometry, range,
overlap, free-list, header, null bitmap, scalar encoding, VARCHAR packing, and exact length.
Require L0/L1 before ordinary unsafe use and the canonical corruption/unsupported result.

The decisive composition fixture has a valid selected column A and malformed unselected
column B. Run all-column and A-only physical schemas, with the tuple both visible and MVCC
invisible. Because `SV` validates every retained NORMAL/retained-DEAD tuple before ordinary
use, all four accessed-page cases fail identically; pruning changes materialization, not
storage validity. Repeat with malformed selected scalar as a control.

| Storage case | Accessed? | Mandatory validation | Output | Demand relevance |
|---|---:|---|---|---|
| valid page/tuple | yes | L0/L1 | owner-qualified values | row demand follows later |
| bad checksum/identity/slot extent | yes | yes | none | structural prerequisite |
| malformed selected scalar | yes | complete tuple | none | structural prerequisite |
| malformed unselected scalar | yes | complete tuple | none | pruning cannot waive it |
| malformed invisible retained tuple | yes | complete tuple before MV | none | invisibility cannot waive it |
| corrupt page beyond satisfied Limit | no | none merely to probe EOS | none | undemanded path |
| corrupt page actually fetched | yes | yes | none | structural prerequisite |

For valid tuples compare fixed-width/NULL/FLOAT64/DATE/TIMESTAMP values with the independent
persisted codec oracle. Include VARCHAR empty/NULL, embedded NUL, and high bytes. Poison page
memory after guard release: fixed-width output remains copied and each scan StringRef still
resolves exact bytes from the output chunk's StringHeap. No page pointer may remain reachable.

## V27-D — Sequential-scan snapshot, visibility, continuation, and completion

Feed `MV` a fixed snapshot and status map, then run both scans over committed, in-progress,
aborted, self-earlier, self-current, deleted, and impossible future-command versions. Add
status lookup failure. Every page/chunk in one attempt must use the context's same effective
snapshot/command; a poisoned alternate snapshot catches scan-local refresh. READ COMMITTED
retry gets a fresh attempt snapshot, while REPEATABLE READ retains its horizon and uses the
current command boundary through the existing owner.

For continuation use pages P1..P5: P1 empty; P2 has invisible and UNUSED slots; P3 has more
visible tagged occurrences than capacity; P4 is all invisible; P5 has the final visible tag.
Run capacities 1, 2, and 5 and insert legal empty progress offers. `HT+MV+FO` computes the
survivor sequence before chunking. Every candidate and survivor is visited/offered exactly
once, resume coordinates equal the first unvisited candidate, and empty pages advance a
finite cursor. P5's final tag is offered and handed off before FINISHED/no output. An
all-invisible relation may finish without a fake row or cardinality-derived EOS.

### MVCC/snapshot matrix

| Creator/deleter state | Command relation | Expected visibility/result | SeqScan / IndexScan | Oracle / status |
|---|---|---|---|---|
| committed creator, no effective deleter | snapshot-visible | visible | equal | MV |
| in-progress/aborted creator | other | invisible | equal | MV |
| self creator | earlier / same / future | visible / invisible / error | equal | MV |
| committed deleter | snapshot-visible / too new | invisible / visible | equal | MV |
| in-progress/aborted deleter | other | visible | equal | MV |
| self deleter | earlier / same / future | invisible / visible / error | equal | MV |
| status lookup failure | applicable | exact failure | equal owner | MV/ER |
## V27-E — Index bounds, cursor, duplicate keys, and range completion

Reuse the independent Chapter-8 semantic comparator and encoded-key property suite, then
construct BR ranges without asking production bound code for expected endpoints. Cover
unbounded sides, all inclusive/exclusive combinations, exact equality, equality prefixes,
one range component, transient trailing low/high components, and MIN/MAX RID endpoint
sentinels. Chapters 17/36 decide planner legality: nullable `IS NULL` may form an exact NULL
range, while ordinary `= NULL` remains UNKNOWN and cannot be rewritten to it.

| Bound case | Planner/runtime oracle | Residual |
|---|---|---|
| equality; inclusive/exclusive range | exact BR endpoint membership | only unrepresented conditions |
| `IS NULL` nullable key | exact encoded NULL class | none for proven exact predicate |
| ordinary `= NULL` | never transformed into IS NULL | SQL predicate remains owner |
| composite equality prefix + one range | exact leftmost-prefix interval | later unsupported conjuncts |
| trailing low/high and RID sentinels | search-only endpoint objects | owner-dependent |
| inapplicable bound/algorithm | excluded before final plan | N/A at runtime |

Create one equal-user-key run spanning three leaves and multiple output chunks. Every legal
distinct `(key,RID)` appears once; a duplicate exact physical key is `CORRUPT_INDEX`. For a
UNIQUE index put invisible/aborted historical entries before a later visible entry: scanning
cannot stop at the first physical entry. A nonunique equality scan exhausts its demanded
range unless downstream early stop applies. Leaf end, empty/short output, or an exhausted
RID batch is never range terminal; exact upper-bound or cursor exhaustion is.

## V27-F — Index heap recheck, residual qualification, and RID safety

For every BR candidate, record reference-domain validation, heap fetch, returnable NORMAL
state, MV result, residual result, and output. Index membership alone never supplies any of
them. Use wrong relation, out-of-range page/slot, invalid target state, malformed heap page,
visible and invisible versions, and FALSE/UNKNOWN/error residuals. Only visible residual
TRUE candidates emit.

Capture candidate R into an RID batch, then under deterministic barriers complete legal
index cleanup and transition R to retained DEAD and reclaimed DEAD while its read epoch
continues to prohibit rebinding. Consumption returns no unrelated row and never dereferences
reclaimed payload. Structurally valid historical garbage follows the owner skip path;
wrong-owner/out-of-domain/malformed states fail. Separately attempt same-RID allocation while
one Chapter-14 barrier is held: `RB` rejects reuse. No generation field is required.

| RID/index case | Candidate legal? | Heap action/output | Reuse/skip classification |
|---|---:|---|---|
| visible NORMAL RID | yes | validate, MV, residual; maybe emit | no reuse issue |
| invisible old/historical entry | yes | validate/MV; no output | normal skip |
| captured RID later DEAD | candidate was legal | state check; no reclaimed dereference | historical skip under owner |
| protected old RID | yes | identity cannot change | reuse forbidden |
| attempted reuse with live barrier | no new binding | no unrelated output | invariant rejection |
| wrong relation/range/state | invalid reference | safe rejection before bytes | lower-layer failure |
| UPDATE new RID / DELETE old entry | owner-valid history | snapshot decides occurrences | cleanup remains Ch14 |

For exact-bound predicate elision, compare bound membership with the full semantic predicate
using V20-15's total/error-free proof. Elision is accepted only for the proven occurrence
domain; a broader bound retains and evaluates its residual with original provenance.

## V27-G — RID batching and IndexScan ordering

Build candidate streams of length 0, 1, capacity, and capacity+1. Vary internal batch sizes
without requiring a container type. The stated target remains up to one DataChunk capacity,
a physical tuning target—not a SQL count or persistent limit. Cross a leaf boundary, RID-
batch boundary, heap-fetch grouping boundary, and output boundary at different positions;
BR records the next candidate and AP records emitted tagged occurrences. Require no loss,
duplication, or restart and final output before terminal.

| Case | Property advertised? | Legal batching/order | SQL tie order? |
|---|---:|---|---:|
| compatible forward prefix | yes | preserve promised candidate/survivor sequence | no extra tie rule |
| DESC or incompatible NULL/collation/slot | no | cannot claim satisfaction | no |
| ordered PageId regrouping that changes sequence | yes | forbidden | no |
| PageId grouping without promised/needed order | no | allowed if AP bag exact | no |
| duplicate user-key RID suffix | owner-compatible property | physical order observable to executor only | no |
| leaf/batch/output boundary | as selected | exact continuation and multiplicity | no |

Property checks reuse Ch37's exact LogicalSlotId prefix, direction, NULL placement, type,
and binary collation oracle. Native reverse execution is N/A because the owner excludes
it; verify that forward traversal never advertises DESC. This is the sole N/A classification.

## V27-H — Access-path substitutability and path-local failures

Generate one logical relation, predicate, schema, snapshot, and required ordering, then run
legal SeqScan and IndexScan plans. `AP` derives visible qualifying occurrences from HT/MV/FO
and uses Ch20's bag/allowed-order rule. Where both runs succeed, compare exact values, NULLs,
tagged multiplicity, required-order class, schema/LogicalSlotIds, demanded semantic error
owner, and transaction/result envelope.

| Scenario | Semantic equality required? | Resource equality? | Corruption exposure equality? |
|---|---:|---:|---:|
| SeqScan / IndexScan both succeed | yes | no | no path-local access equivalence required |
| one path symbolically OOM, other succeeds | successful result still owner-correct | no | unaffected |
| corruption only in structure one path accesses | no wrong successful rows | no | touching path fails; untouched path need not |
| demanded residual/Project error | same canonical owner/class | no | independent |
| ordered result | same required-order equivalence class | no | independent |
| unordered result/Limit | same owner-allowed bag predicate | no | independent |

A selected-path storage/resource failure propagates; do not silently execute an alternate
path unless an independent Architecture rule authorizes it. Verification introduces no
dynamic-fallback protocol.

## V27-I — PhysicalFilter

`FO` evaluates the bound BOOLEAN predicate once for each demanded active occurrence using
Chapter-17 3VL: TRUE retains exactly one tagged occurrence; FALSE and UNKNOWN retain none.
Use duplicate equal values, repeated dictionary indices, mixed selections, empty input, and
all-rejected input. Compare FLAT materialization with valid selection/dictionary views.

| Case | Demand/retention | Sequence/representation | Error/lifetime |
|---|---|---|---|
| TRUE / FALSE / UNKNOWN | demanded; 1 / 0 / 0 | surviving order | owner scalar result |
| empty input | no per-row demand; zero output | valid resolution, not EOS | no per-row error |
| all rejected | all predicates demanded; zero output | finite AL progress | later input remains legal |
| repeated dictionary occurrence | independently demanded/retained | repetition preserved | BO-valid view |
| FLAT vs dictionary | same tagged survivors | representation-free | BO governs |
| SELECT/DML predicate error | failed output | no first lane/row | ER: D25 / D21 |

Filter preserves child schema and survivor sequence but creates no SQL order. A live borrowed
selection/base/StringRef remains value-stable until downstream handoff or independent copy.

## V27-J — PhysicalProject

`PS` lists declared physical output-schema entries in their declared column sequence, never
numeric LogicalSlotId order. For each demanded input occurrence, independently evaluate the
ordered logical Project expressions through V20-5/V25 and produce one output row. Use the
same source column twice, equal expressions with distinct semantic occurrences, equal values,
reordered output columns, NULL/CONSTANT results, and physically shared computation.

| Case | Row/column/identity oracle | Demand/provenance | Ownership |
|---|---|---|---|
| direct pass-through | one row; declared entry/ID | declared occurrence | BO borrow allowed |
| computed scalar/NULL/CONSTANT | one row; declared sequence | demanded ordinary expression | exact vector/validity |
| duplicate expression/equal value | one row; distinct declared IDs | origins retained | sharing behind PS |
| computed/borrowed VARCHAR | one row; exact bytes | owner-correct candidate | result owner / BO |
| empty input | zero rows/evaluations | no per-row candidate | no output borrow |
| ordinary unused-looking output | still declared/demanded | cannot erase possible error | owner-valid result |
| EXISTS projection-irrelevant case | specialized zero value demand | Ch20 proof, not RequiredSlotSet alone | no fabricated evaluation |

Project preserves input row-occurrence sequence; §37.5 alone decides retained order keys.
CSE may share work only while every output ID, origin, D25/D21 result, and observable value
remains equivalent. RequiredSlotSet controls physical need but is not an error-erasure proof.

## V27-K — PhysicalLimit

`LS` uses arbitrary-precision indices over an independently tagged child sequence. For
validated `o>=0` and optional `l>=0`, it computes `C[o:o+l]` mathematically or `C[o:]`; it
never forms a finite-width `o+l`. Compare production output, counters, AL state, and ST trace
to that slice.

Construct the child from invisible heap versions, index candidates rejected by MV/residual,
Filter FALSE/UNKNOWN rows, and qualifying outputs. Only actual child output occurrences
enter LS. Raw slots, RIDs, and rejected rows do not increment offset or limit.

| Case | Child-row demand / slice | Completion/error |
|---|---|---|
| OFFSET 0, LIMIT 0 | no relational fetch after count acquisition; empty | valid early stop |
| OFFSET positive, LIMIT 0 | no fetch merely to consume OFFSET/EOS | binding/count/other required work retained |
| OFFSET 0/1, LIMIT 1 | exact next occurrence | stop when satisfied |
| OFFSET exceeds input | consume available child outputs; empty | successful child exhaustion |
| LIMIT exceeds remainder | all remaining outputs | successful exhaustion |
| OFFSET boundary in chunk | exact suffix | no loss/duplication |
| LIMIT boundary in chunk | exact prefix | remainder undemanded |
| both boundaries in one chunk | exact interior slice | final offer then terminal |
| empty progress child batch | counts unchanged | progress may advance; not EOS |
| ordered child | exact ordered slice | property preserved |
| unordered child | exact selected child-sequence slice; cross-plan allowed subbag | no hidden RID order |
| INT64_MAX counts / mathematical sum beyond it | arbitrary-precision result | no sum-overflow SQL error |

For long-counter safety, use a symbolic run-length stream with segment lengths beyond a
naive signed cumulative counter while keeping only boundary tags materialized. Compare
saturating/count-down/wider observed strategies to LS. Wrap, narrowing, wrong rows, or a new
SQL count-overflow error fails; no concrete counter type is required.

Arm a scalar error and corrupt page after the limit boundary. They remain unrequested after
valid early stop, while an accessed page still receives SV. Count acquisition and separately
required statement/finalization work remain demanded. Verification does not require zero
child objects or allocations. Borrowed interior slices follow BO; lifecycle resolution does
not authorize reset before downstream handoff.

## V27-L — PhysicalValues

`VO` is a bound typed row list, including empty, duplicates, and the Chapter-20 one zero-
column no-FROM occurrence. Poison parser, name lookup, and TypeResolver entry points during
execution. Require listed multiplicity, valid chunks, and ST completion; source batching does
not create semantic order.

For lifetime, run (A) a plan-owned scalar/string payload whose immutable owner outlives every
synchronous consumer and (B) a downstream result retention extending beyond that interval.
Borrowing is legal in A. In B, poison/destroy the shorter-lived owner after handoff and require
independent copy/transfer/materialization before retention. Exact bytes, NULLs, and duplicate
occurrences remain unchanged; growing retained storage is accounted.

| Values case | Runtime binding/type work | Multiplicity/sequence | Ownership/terminal |
|---|---:|---|---|
| one/duplicate/empty rows | none | listed occurrences; no SQL order invented | ST |
| zero-column no-FROM | none | exactly one occurrence | ST |
| sufficiently long-lived plan payload | none | unchanged | valid borrow |
| longer downstream retention | none | unchanged | independent owner required |

## V27-M — PhysicalResultSink

Reuse V26-E's submitted-domain ledger: successful Sink accepts the complete submitted domain
exactly once and exposes no generic partial success. Exercise empty, ordinary, and ordered
chunks; synchronous consumption; retained copy; allocation failure halfway through retention;
cancellation; DML RETURNING; and a prior externally returned SELECT chunk.

| Case | Acceptance/ownership | Publication/completion | Replay/error |
|---|---|---|---|
| synchronous/retained SELECT chunk | whole domain once; BO-valid | internal handoff only | no replay |
| empty submitted chunk | complete empty domain | not EOS by cardinality | no replay |
| retention allocation failure | not successful acceptance | no client success | OOM; no blind replay |
| cancellation | failed/incomplete path | no later same-instance success | cleanup |
| RETURNING | D21/§31.9 spool owner | statement/autocommit gate | DML envelope |
| prior cursor chunk then later failure | prior delivery retained for interval | prefix is not query success | PB owner |

Required child occurrence sequence and multiplicity are preserved; the sink invents neither
deduplication nor order. Chapter 31 owns client-visible return, returned lifetime, and EOS.

## V27-N — Schema, LogicalSlotId, and RequiredSlotSet

### Schema/slot matrix

| Operator/case | Output schema / LogicalSlotId behavior | Ordinal/name rule | RequiredSlotSet |
|---|---|---|---|
| SeqScan / IndexScan | same declared logical Get outputs | ordinal addresses schema only; no runtime names | output+predicate needs |
| Filter / Limit | child schema and IDs unchanged | no new identity | pass-through plus local need |
| Project | declared ordered output entries and IDs | never numeric-ID sorting | demanded declared outputs/proven exceptions |
| Values | bound typed declared schema | no rebinding | declared outputs |
| ResultSink | consumes complete required result schema | no name resolution | final required slots |
| predicate-only column | intermediate dependency only | not exposed as final column | retained until predicate |
| hidden RID/internal slot | internal semantic identity | never user row identity | retained for DML owner |
| reordered output schema | mapping follows new declared entries | prior ordinal rejected | exact surviving requirements |

## V27-O — Borrowing, VARCHAR, and ownership

Use generation-tagged BO nodes and mutate/reset each backing at every boundary. A test passes
only when all reachable value, validity, selection, dictionary, StringRef metadata, and bytes
remain stable or have an independent owner.

| Backing/use | Required owner transition | Reset/reuse | Accounting / status |
|---|---|---|---|
| heap page → scan VARCHAR | copy to chunk StringHeap before guard release | page may release after copy | BufferPool/chunk |
| Filter dictionary/view | downstream borrows input graph | only after dependencies/preservation | query owner |
| Project pass-through/computed VARCHAR | borrow / exact output owner | same BO rule | query owner |
| Limit slice | child backing retained or copied | not at lifecycle resolution alone | query owner |
| Values payload | plan borrow or independent transfer | according to longest consumer | plan/query owner |
| ResultSink/cursor | result owner safely retains | pipeline cleanup cannot invalidate | Ch24/31 |
| diagnostic backing | reporting owner retains | no stale chunk reset | Ch24/26 |
| failure cleanup | quiesce or preserve every user | then release | Ch24/26 |
## V27-P — Errors, cancellation, retry, and persistence boundary

### Error-owner matrix

| Failure | Candidate/terminal owner | First physical discovery? | Current output | Provenance/transaction |
|---|---|---:|---|---|
| ordinary non-DML candidate / winner | V25-J | no | invalid on terminal failure | origin retained; §39 |
| DML candidate / winner | V21-13 | no; no V25-J pre-ranking | invalid on failure | phase/eligibility retained; §39 |
| heap/index/invalid scalar corruption | storage owner/§39 | no new cross-class rank | invalid | lower cause retained; §39 |
| OOM/representability | Ch24/§39 | not D25/D21 | invalid | structured cause; §39 |
| QueryCancelled | Ch26/§39 | no universal rank | invalid | cleanup/quiescence; §39 |
| ResultSink failure | cause owner/Ch31 | no replay rule | invalid | acceptance incomplete; §39 |
| internal invalid runtime state | Ch22–26/§39 | not public ranking | invalid | no persistent effect |

Permute tuple, RID, Filter lane, Project expression, chunk, and worker discovery/reduction;
ER must select the same D25 or D21 result. No test invents global precedence among semantic,
resource, cancellation, and corruption classes: each controlled trace establishes only its
owner-defined cause. Partially written current output is poisoned and never consumed; prior
internal handoff and prior cursor return remain distinct PB events.

Set cancellation at explicit chunk/block boundaries for every operator-state category and
require no later success from the same runtime. At an owner-admitted retry, AF requires fresh
Seq page/slot cursor, index cursor/RID batch, Filter selection, Project state, Limit counters,
Values position, ResultSink acceptance, and candidate state. Immutable plan configuration
may be reused. None of these runtime artifacts, worker IDs, selections, buffers, or counters
may enter page/WAL/catalog/recovery identity or independently commit/abort a transaction.

## V27-Q — Invalid runtime states

Static plan faults use V22-K/§38.24. For dynamic faults, arm IX's forbidden dereference,
output, multiplicity, and persistent-effect counters and require safe rejection first.

| Invalid state | Public SQL? / classification | Safe rejection and principal risk |
|---|---|---|
| wrong child/output schema or Project TypeId | no; internal | before vector access; wrong value/slot |
| invalid page/slot cursor | no; internal unless persisted bytes corrupt | before range access; skip/OOB |
| missing snapshot | no; internal | before MV; invisible exposure |
| invalid index cursor or wrong relation RID | internal or lower-layer corruption | before fetch/dereference |
| invalid residual mapping / Filter non-BOOLEAN | no; internal after validation | before expression/selection |
| negative validated Limit state / counter wrap | no; internal | before slicing; wrong multiplicity |
| expired borrow | no; internal | before dereference/reset |
| output after terminal / failed output consumed | no; protocol internal | before handoff; duplicate/stale output |
| stale failed-attempt state | no; internal | before retry use |

## V27-R — Representation, batching, worker, and path determinism

Replay fixed fixtures with capacities 1, 7, 1024, and symbolic/feasible 65535; different
chunk/page/slot/leaf/RID-batch boundaries; inserted finite empty progress events; FLAT,
CONSTANT, DICTIONARY, selection and materialized forms; valid worker partitions/release
orders; relocated objects; and renamed runtime IDs. Compare with EQ, not trace identity.

| Perturbation | Values/NULL/bag/order/demand/errors/result | Resource feasibility | Corruption exposure |
|---|---|---|---|
| chunk capacity/boundary/empty progress | owner-equivalent | may differ | same accessed bytes |
| page/slot batching | owner-equivalent | may differ | same accessed pages validate |
| leaf/RID-batch size and boundary | owner-equivalent | may differ | followed pages validate |
| selection/FLAT/CONSTANT/DICTIONARY | owner-equivalent | may differ | runtime-invalid forms rejected |
| borrowed versus materialized exact form | owner-equivalent | may differ | lifetime invariant |
| worker partition/order | owner-equivalent; no gap/overlap/first-worker rank | may differ | touched storage may differ |
| pointer/address/runtime ID | owner-equivalent | no semantic role | no identity role |
| legal access path | both-success semantics equivalent | may differ | path-local exposure may differ |

### Retry/cancellation matrix

| Operator | Attempt-local state | Survives failure/cancel? | Fresh retry / immutable plan | Failed output |
|---|---|---:|---|---|
| SeqScan | page/slot/output cursor | no | fresh / reusable | invalid |
| IndexScan | range cursor/RID batch/output | no | fresh / reusable | invalid |
| Filter | selection/expression/AL | no | fresh / reusable | invalid |
| Project | expression/output/AL | no | fresh / reusable | invalid |
| Limit | skipped/emitted/AL | no | fresh / reusable | invalid |
| Values | row/output cursor | no | fresh / reusable | invalid |
| ResultSink | acceptance/retained attempt state | no | fresh / reusable | invalid |

### Physical-property and trait matrix

| Operator | OrderingProperty input/output | RequiredSlotSet | Role/trait/breaker | Owner / status |
|---|---|---|---|---|
| SeqScan | none provided | output+predicate columns | source; not inherently blocking/breaker | Ch26/37 |
| IndexScan | compatible forward property only | output+heap/residual columns | source; not inherently breaker | Ch8/37 |
| Filter | preserves input | predicate+outputs | streaming; no breaker | Ch26/37 |
| Project | retained unchanged keys only | demanded declared outputs | streaming; explicitly no breaker | Ch20/37 |
| Limit | preserves input | pass-through | streaming; early stop, no breaker | Ch20/26/37 |
| Values | no independent SQL order | declared outputs | source | Ch20/26/37 |
| ResultSink | consumes/preserves required sequence | complete result | sink; not automatically breaker | Ch26/31/37 |
## V27-S — Cross-chapter reuse map

| Handoff | Contract / canonical owner | V27 responsibility | Reusable methodology | Oracle |
|---|---|---|---|---|
| Ch4→27 | accessed-page L0/L1/L2, §4.13 | invoke/propagate before use | persisted validation suites | SV |
| Ch5→27 | heap domain/slots/tuple codec/order | traversal/materialization | Heap page and Tuple codec tests | HT/SV |
| Ch8→27 | keys/ranges/duplicates/cursor/corruption | consume candidate range | IndexKeyCodec/routing/cursor tests | BR |
| Ch9→27 | snapshots/CommandId | context handoff | snapshot lifecycle tests; V22-E | MV/AF |
| Ch10→27 | visibility/errors | apply to candidates | visibility truth/error matrices | MV |
| Ch11→27 | logical locks for DML owners | no scan-local policy | lock/revalidation tests | owner state |
| Ch14→27 | epochs/RID reuse | retain candidate identity | epoch/reuse matrix | RB |
| Ch15→27 | statement/DML envelope | preserve handoff | DML execution tests | AF/PB |
| Ch16→27 | immutable/historical descriptors | ResolveSchema decoding | catalog/schema handoff tests | schema model |
| Ch17→27 | scalar/NULL/comparison/bytes | exact values/predicates | type/key property tests | FO/BR |
| Ch19→27 | binding/slots/count acquisition | consume resolved state | front-end/V19 tests | PS/LS |
| Ch20→27 | bags/order/demand/operators | physical conformance | V20-4/5/10–17 | AP/FO/PS/LS/VO |
| Ch21→27 | V21-13/14/retry | transport candidates/results | V21-2/3/13/14 | ER/AF/PB |
| Ch22→27 | vocabulary/schema/plan/applicability | execute selected plan | V22-A–G/K | PS/AP/IX |
| Ch23→27 | chunks/representations/borrows | valid outputs/views | V23-A–M | BO/PS/IX |
| Ch24→27 | accounting/resources/cleanup | charge/fail/clean | V24-B/D/H/L/M | BO/ER |
| Ch25→27 | demand/D25/DML/results | expression handoff | V25-B/D/J/K/L/O/P | ER/PS/PB |
| Ch26→27 | source/stream/sink protocol | local operator realization | V26-A–Q | ST/AL/BO/PB/AF |
| Ch27→31 | result retention/publication | safe sink handoff | V21-14; V25-O; V26-J/M | PB/BO |
| Ch27→32 | worker-local scan state | partition without semantic change | Parallel Execution Tests; V26-Q | HT/EQ |
| Ch27→36 | access predicates/bounds/residual | execute approved classification | Access Path Tests | BR/AP |
| Ch27→37 | ordering/required slots | truthful provider/consumer | V22-C; property tests | PS/AP |
| Ch27→38 | final plan validation | rely on validated static state | V22-K; final optimizer validation | IX |
| Ch27→39 | categories/transaction effects | preserve owner failures | V21/V24/V25/V26 error tests | ER/AF |

## V27-T — Atomic architecture-obligation ledger

Each row is independently falsifiable. Repeated §27.12 summaries reuse the detailed row;
explanatory rationale and navigation are not recounted. Reverse execution is the only N/A:
the architecture expressly excludes that capability while requiring a negative property
check. All other rows name a deterministic method and independent oracle.

| ID | Architecture section | Atomic obligation | Verification section | Oracle | Reuse |
|---|---|---|---|---|---|
| V27-A01 | 27.1–27.11 | The final seven operators are classified by exact source/streaming/sink role. | V27-A | PS | V22-L/V26-O |
| V27-A02 | 27.1–27.11 | Every operator consumes its declared input/child shape. | V27-A | PS/IX | V22-K |
| V27-A03 | 27.1–27.11 | Every produced chunk matches the declared physical output schema. | V27-A/N | PS | V23-A |
| V27-A04 | 27.1–27.11 | Mutable operator state is attempt-local, not plan mutation. | V27-A/P | AF | V22-A/E/V26-L |
| V27-A05 | 27.1–27.11 | Sources use explicit exhaustion rather than chunk cardinality. | V27-A | ST | V26-A |
| V27-A06 | 27.7–27.9 | Streaming operators use accepted-input lifecycle/continuation. | V27-A | AL | V26-B–D |
| V27-A07 | 27.11 | ResultSink uses complete-domain sink acceptance. | V27-A/M | PB | V26-E |
| V27-A08 | 27.1–27.11 | Operator multiplicity follows its logical owner. | V27-A | AP/FO/PS/LS/VO | V20-4/5/11 |
| V27-A09 | 27.1–27.11 | Operator ordering behavior follows Ch20/37, not incidental execution. | V27-A/R | AP/PS | V22-C |
| V27-A10 | 27.1–27.11 | Expression behavior remains Ch17/20/25-owned. | V27-A | ER | V25 |
| V27-A11 | 27.1–27.11 | Runtime performs no name binding or type resolution. | V27-A/L/N | PS/IX | V22-A |
| V27-A12 | 27.1–27.11 | Physical ordinal addresses schema but is not semantic identity. | V27-A/N | PS | V23-A |
| V27-A13 | 27.1–27.11 | Pointer/value equality is not LogicalSlotId identity. | V27-A/N | PS | V22-B |
| V27-A14 | 27.1–27.11 | Static malformed plans are rejected before execution effects. | V27-A/Q | IX | V22-K/§38.24 |
| V27-B01 | 27.1; 27.12(1) | SeqScan covers every published relation heap-data page exactly once. | V27-B | HT | heap scan tests |
| V27-B02 | 27.1; 27.12(1) | SeqScan visits pages in ascending PageNo physical order. | V27-B | HT | heap scan tests |
| V27-B03 | 27.1; 27.12(1) | SeqScan visits slots in ascending SlotId physical order. | V27-B | HT | heap scan tests |
| V27-B04 | 27.1 | FSM contents do not define full-scan membership. | V27-B | HT | Ch5/6 tests |
| V27-B05 | 27.1 | NORMAL is the ordinary tuple candidate state. | V27-B | HT/SV | slot-state tests |
| V27-B06 | 27.1 | UNUSED is never dereferenced as tuple data. | V27-B | SV/IX | slot-state tests |
| V27-B07 | 27.1 | Retained/reclaimed DEAD is not returned as an ordinary row. | V27-B | SV/RB | slot/reuse tests |
| V27-B08 | 27.1 | Reserved/unknown slot states are not guessed as absent. | V27-B | SV | validation tests |
| V27-B09 | 27.1 | Equal-valued NORMAL occurrences remain distinct candidates. | V27-B | HT/AP | V20-4 |
| V27-B10 | 27.1 | Candidate traversal has no page or slot omission. | V27-B/D | HT | V26-Q |
| V27-B11 | 27.1 | Candidate traversal has no page or slot duplication. | V27-B/D | HT | V26-Q |
| V27-B12 | 27.1; 27.12(1) | Physical page/slot determinism creates no SQL OrderingProperty. | V27-B | AP | V22-C/Ch37 tests |
| V27-C01 | 27.1–27.2 | Every accessed heap page passes required L0/L1 before ordinary use. | V27-C | SV | Ch4/5 validation tests |
| V27-C02 | 27.1 | Page checksum, identity, owner, type, and geometry failures propagate canonically. | V27-C | SV | Ch4 validation |
| V27-C03 | 27.1 | Slot range/non-overlap/free-list failures propagate canonically. | V27-C | SV | heap-page tests |
| V27-C04 | 27.1 | Every retained NORMAL/retained-DEAD tuple receives complete validation. | V27-C | SV | tuple tests |
| V27-C05 | 27.1 | Malformed selected persisted scalar prevents output. | V27-C | SV | TupleCodec tests |
| V27-C06 | 27.1; 27.12(3) | Malformed unselected retained scalar still fails accessed-page validation. | V27-C | SV | tuple tests |
| V27-C07 | 27.1; 27.12(2–3) | MVCC invisibility does not bypass retained-tuple structural validation. | V27-C | SV/MV | visibility tests |
| V27-C08 | 27.1; 27.12(3) | Projection/predicate pruning cannot weaken structural validity. | V27-C | SV/PS | V22-F |
| V27-C09 | 27.1 | A page beyond valid early-stop demand need not be fetched merely for EOS. | V27-C/K | LS/SV | V26-N |
| V27-C10 | 27.1 | Any page actually fetched remains fully validated despite row demand. | V27-C | SV | Ch4 validation |
| V27-C11 | 27.1–27.2 | Fixed-width, NULL, FLOAT, DATE, and TIMESTAMP decode exactly. | V27-C | SV | scalar/tuple tests |
| V27-C12 | 27.2; 27.12(5) | Scan VARCHAR bytes are copied to output-owned storage before page release. | V27-C/O | BO | V23-F/G/H |
| V27-D01 | 27.1; 27.12(2) | SeqScan uses the QueryExecutionContext effective snapshot/read epoch. | V27-D | MV | V22-E |
| V27-D02 | 27.1 | One attempt does not refresh snapshot per page/chunk. | V27-D | MV | snapshot tests |
| V27-D03 | 27.1 | READ COMMITTED uses one statement snapshot per attempt. | V27-D | MV/AF | Ch9 tests |
| V27-D04 | 27.1 | REPEATABLE READ uses transaction horizon plus current command. | V27-D | MV | Ch9 tests |
| V27-D05 | 27.1 | Visible surviving SeqScan tuples emit exactly once. | V27-D | HT/MV/AP | V20-4 |
| V27-D06 | 27.1 | Invisible versions emit no row. | V27-D | MV | Ch10 tests |
| V27-D07 | 27.1 | Visibility/status errors propagate rather than becoming invisible. | V27-D/P | MV/ER | Ch10 tests |
| V27-D08 | 27.1 | Page/slot/output-capacity continuation resumes at the first unvisited candidate. | V27-D | HT/ST | V26-A/C |
| V27-D09 | 27.1 | Empty and all-invisible pages advance finite state without false EOS. | V27-D | HT/ST | V26-D |
| V27-D10 | 27.1 | Final visible output is offered once before FINISHED/no output. | V27-D | ST | V26-A |
| V27-E01 | 27.4 | IndexScan consumes planner-selected encoded lower/upper bounds and inclusivity. | V27-E | BR | Access Path Tests |
| V27-E02 | 27.4 | Equality/unbounded/inclusive/exclusive ranges have exact BR membership. | V27-E | BR | B+ tests |
| V27-E03 | 27.4 | Composite bounds use exact equality prefix plus at most one range component. | V27-E | BR | Access Path Tests |
| V27-E04 | 27.4 | Trailing and RID endpoint sentinels remain transient exact search objects. | V27-E | BR | B+ tests |
| V27-E05 | 27.4 | Nullable IS NULL may use exact NULL bounds when proved. | V27-E | BR/FO | Access Path Tests |
| V27-E06 | 27.4 | Ordinary `= NULL` is not rewritten as IS NULL. | V27-E | FO | V20-5 |
| V27-E07 | 27.4 | Every legal distinct duplicate user-key entry is visited. | V27-E | BR | duplicate-range tests |
| V27-E08 | 27.4 | Exact duplicate physical `(key,RID)` entries are corruption. | V27-E | BR/SV | B+ validation |
| V27-E09 | 27.4 | UNIQUE historical entries do not authorize first-physical-entry termination. | V27-E | BR/MV | Ch8/10 tests |
| V27-E10 | 27.4 | Nonunique equality exhausts the demanded equal-key range. | V27-E | BR | duplicate-range tests |
| V27-E11 | 27.4 | Leaf boundary is not range exhaustion. | V27-E/G | BR | cursor tests |
| V27-E12 | 27.4 | Empty/short output and RID-batch end do not establish range exhaustion. | V27-E/G | BR/ST | V26-A |
| V27-E13 | 27.4 | Exact bound/cursor exhaustion or valid early stop establishes terminality. | V27-E | BR/ST | V26-A/N |
| V27-E14 | 27.4 | IndexScan final qualifying output precedes terminal completion. | V27-E/G | ST | V26-A |
| V27-F01 | 27.4; 27.12(6) | Every index hit is only a candidate. | V27-F | BR/AP | V22-F |
| V27-F02 | 27.4; 27.12(6) | Ordinary candidate handling fetches and validates the heap reference/tuple. | V27-F | SV/RB | Ch4/14 tests |
| V27-F03 | 27.4; 27.12(6) | Candidate visibility is rechecked in the heap. | V27-F | MV | Ch10 tests |
| V27-F04 | 27.4 | Visible candidates emit only after required residual TRUE. | V27-F | MV/FO | V25 |
| V27-F05 | 27.4 | Residual FALSE/UNKNOWN emits no occurrence. | V27-F | FO | V20-5 |
| V27-F06 | 27.4 | Residual errors retain D25/D21 ownership. | V27-F/P | ER | V25-J/K |
| V27-F07 | 27.4 | Exact predicate elision requires the upstream semantic proof. | V27-F | FO/AP | V20-15/16 |
| V27-F08 | 27.4 | Captured RID later DEAD never returns an unrelated tuple or reclaimed payload. | V27-F | RB/SV | Ch14 reuse tests |
| V27-F09 | 27.4 | Structurally valid historical garbage may follow its canonical skip path. | V27-F | RB | Ch14 tests |
| V27-F10 | 27.4 | Wrong relation/range/state is safely rejected, not ordinary invisibility. | V27-F/Q | SV/IX | Ch4/14 tests |
| V27-F11 | 27.4; 27.12(7) | Read-epoch protection covers retained index-derived RID identity. | V27-F | RB | Ch14 tests |
| V27-F12 | 27.4; 27.12(7) | Old protected RID cannot alias a newly allocated unrelated tuple. | V27-F | RB | Ch14 reuse matrix |
| V27-G01 | 27.5 | RID batching uses a physical target up to one DataChunk capacity. | V27-G | BR | V23-B |
| V27-G02 | 27.5 | The target is not a SQL or persistent row limit. | V27-G | AP | V20/V23 |
| V27-G03 | 27.5 | RID batches are query-local temporary state. | V27-G/P | AF | V22-E |
| V27-G04 | 27.5 | Batch/leaf/output continuation consumes every required candidate once. | V27-G | BR/AP | V26-C |
| V27-G05 | 27.5 | Final candidate/output is not lost at any composed boundary. | V27-G | BR/ST | V26-A |
| V27-G06 | 27.5; 27.12(8) | Batching preserves every ordering advertised by IndexScan. | V27-G | BR/AP | Ch37 tests |
| V27-G07 | 27.5 | PageId regrouping is rejected when it would violate promised/needed ordering. | V27-G | AP | V22-C |
| V27-G08 | 27.5 | PageId grouping may preserve an unordered exact bag when no original order is needed. | V27-G | AP | V20-10 |
| V27-G09 | 27.6; 27.12(9) | Forward property requires compatible prefix/type/collation/NULL/direction/slots. | V27-G/R | AP/PS | Ch37 tests |
| V27-G10 | 27.6 | Physical duplicate-key RID tie order creates no extra SQL tie semantics. | V27-G | AP | V20-10 |
| V27-H01 | 27.1; 27.4 | Both-successful legal access paths preserve exact values and NULLs. | V27-H | AP/EQ | V22-F/G |
| V27-H02 | 27.1; 27.4 | Both-successful legal access paths preserve the visible qualifying occurrence bag. | V27-H | AP/EQ | V20-4 |
| V27-H03 | 27.1; 27.4 | Both-successful legal access paths preserve required ordering equivalence. | V27-H | AP/EQ | V22-C |
| V27-H04 | 27.1; 27.4 | Access-path choice preserves schema and LogicalSlotIds. | V27-H/N | PS/EQ | V22-B |
| V27-H05 | 27.1; 27.4 | Demanded semantic errors retain D25/D21 ownership across paths. | V27-H/P | ER/EQ | V25-J/K |
| V27-H06 | 27.1; 27.4 | Resource feasibility may differ between physical paths. | V27-H/R | EQ | V24-L |
| V27-H07 | 27.1; 27.4 | Accessed-corruption surface may differ with physical work. | V27-H/R | SV/EQ | Ch4 tests |
| V27-H08 | 27.1; 27.4 | Runtime path failure does not invent silent dynamic fallback. | V27-H | AP/ER | V22-D |
| V27-I01 | 27.7 | Filter retains TRUE occurrences exactly once. | V27-I | FO | V20-5 |
| V27-I02 | 27.7 | Filter removes FALSE occurrences. | V27-I | FO | V20-5 |
| V27-I03 | 27.7 | Filter removes UNKNOWN occurrences. | V27-I | FO | V20-5 |
| V27-I04 | 27.7 | Filter preserves duplicate occurrence multiplicity. | V27-I | FO | V20-4/5 |
| V27-I05 | 27.7 | Filter preserves survivor row-occurrence sequence without creating SQL order. | V27-I | FO/AP | V22-C |
| V27-I06 | 27.7 | Empty input has zero per-row demand and can resolve without EOS inference. | V27-I | FO/AL | V25-B/V26-D |
| V27-I07 | 27.7 | All-rejected input is successful zero-output progress, not terminal. | V27-I | FO/AL | V26-C/D |
| V27-I08 | 27.7; 27.12(10) | Materialized and valid dictionary/reference results are semantically equivalent. | V27-I | FO/BO | V23-C/D |
| V27-I09 | 27.7 | Filter borrowed results remain stable through downstream use. | V27-I/O | BO | V23-G/V26-M |
| V27-I10 | 27.7 | Filter error selection is D25/D21-owned, never first lane/row. | V27-I/P | ER | V25-J/K |
| V27-J01 | 27.8 | Project output columns follow the declared ordered physical output schema. | V27-J | PS | V22-B/V23-A |
| V27-J02 | 27.8 | Numeric LogicalSlotId sorting does not determine output columns. | V27-J | PS | V22-B |
| V27-J03 | 27.8 | LogicalSlotId remains semantic identity and ordinal remains addressing. | V27-J/N | PS | V23-A |
| V27-J04 | 27.8 | Ordinary Project emits one row per demanded input occurrence. | V27-J | PS | V20-5 |
| V27-J05 | 27.8 | Project preserves input row-occurrence sequence without creating SQL order. | V27-J | PS/AP | Ch37 tests |
| V27-J06 | 27.8 | Duplicate/equal outputs retain distinct declared identities. | V27-J | PS | V22-B |
| V27-J07 | 27.8 | CSE cannot collapse identity, provenance, or owner-selected errors. | V27-J | PS/ER | V20-18/V25-J/K |
| V27-J08 | 27.8 | Direct input references may borrow only under value-stable lifetime. | V27-J/O | BO | V23-G/V26-M |
| V27-J09 | 27.8; 27.12(11) | Computed VARCHAR has exact stable output ownership. | V27-J/O | BO | V25-L |
| V27-J10 | 27.8 | Empty input produces no per-row Project evaluation or output row. | V27-J | PS | V25-B |
| V27-J11 | 27.8 | Ordinary declared Project expressions remain demanded. | V27-J | PS/ER | V20-5/15 |
| V27-J12 | 27.8 | RequiredSlotSet alone does not authorize suppressing observable errors. | V27-J/N | PS/ER | V20-16/Ch37 |
| V27-K01 | 27.9 | PhysicalLimit applies OFFSET before LIMIT. | V27-K | LS | V20-11 |
| V27-K02 | 27.9 | Limit counts only child logical output occurrences. | V27-K | LS/AP | V20-11 |
| V27-K03 | 27.9 | Invisible physical candidates do not count. | V27-K | LS/MV | V22-F |
| V27-K04 | 27.9 | Residual-rejected and Filter FALSE/UNKNOWN occurrences do not count. | V27-K | LS/FO | V20-5 |
| V27-K05 | 27.9 | LIMIT zero emits zero rows. | V27-K | LS | V20-11 |
| V27-K06 | 27.9 | Positive OFFSET with LIMIT zero requires no child-row fetch merely for OFFSET/EOS. | V27-K | LS | V26-N |
| V27-K07 | 27.9 | LIMIT zero retains binding/count acquisition and separately required work. | V27-K | LS/ER | V19/V20 |
| V27-K08 | 27.9 | LIMIT zero creates no promise of zero object construction/allocation. | V27-K | LS/EQ | V24 |
| V27-K09 | 27.9 | OFFSET beyond input and LIMIT beyond remainder complete without count error. | V27-K | LS | V20-11 |
| V27-K10 | 27.9 | Exact semantics never require finite-width OFFSET+LIMIT. | V27-K | LS | V20-11 |
| V27-K11 | 27.9 | Long-running counters neither wrap/narrow nor create a new SQL overflow error. | V27-K | LS | V20-11 |
| V27-K12 | 27.9 | OFFSET boundary inside a chunk selects the exact suffix. | V27-K | LS/BO | V23-D |
| V27-K13 | 27.9 | LIMIT boundary inside a chunk selects the exact prefix. | V27-K | LS/BO | V23-D |
| V27-K14 | 27.9 | OFFSET and LIMIT boundaries in one chunk select the exact interior slice. | V27-K | LS/BO | V23-D |
| V27-K15 | 27.9 | Empty child batches change neither skipped nor emitted occurrence counts. | V27-K | LS/AL | V26-D |
| V27-K16 | 27.9 | Ordered and unordered results use their distinct owner-correct slice predicates. | V27-K | LS/AP | V20-10/11 |
| V27-K17 | 27.9; 27.12(12) | Satisfied Limit stops new undemanded child-row work without QueryCancelled. | V27-K | LS/AL | V26-N |
| V27-K18 | 27.9 | Limit final output is offered/handed off before terminal completion. | V27-K | ST/AL | V26-A/N |
| V27-L01 | 27.10 | Values consumes already-bound typed rows without parsing/name/type resolution. | V27-L | VO/IX | V20-4 |
| V27-L02 | 27.10 | Repeated listed Values rows remain repeated occurrences. | V27-L | VO | V20-4 |
| V27-L03 | 27.10 | Values batching creates no independent SQL ordering. | V27-L | VO/AP | V20-4/10 |
| V27-L04 | 27.10 | Empty Values and one zero-column no-FROM occurrence retain owner semantics. | V27-L | VO | V20-4 |
| V27-L05 | 27.10 | Plan payload may be borrowed only while it outlives every consumer. | V27-L/O | BO | V23-G |
| V27-L06 | 27.10 | Longer retention obtains independent ownership without changing exact values. | V27-L/O | BO | V26-M |
| V27-M01 | 27.11 | Successful ResultSink accepts its complete submitted domain exactly once. | V27-M | PB | V26-E |
| V27-M02 | 27.11 | Synchronous and retained strategies are both allowed with valid ownership. | V27-M | BO/PB | V26-E/M |
| V27-M03 | 27.11; 27.12(14) | ResultSink never exposes an expired borrowed producer chunk. | V27-M/O | BO | V23-G/Ch31 |
| V27-M04 | 27.11 | Failed retention is not successful acceptance. | V27-M | PB | V26-E/J |
| V27-M05 | 27.11 | Failed sink input is not blindly replayed. | V27-M | AF/PB | V26-E/L |
| V27-M06 | 27.11 | Internal sink handoff is not client publication or query success. | V27-M | PB | V26-J |
| V27-M07 | 27.11 | DML RETURNING keeps its statement/autocommit publication envelope. | V27-M | PB | V21-14/Ch31 |
| V27-M08 | 27.11 | ResultSink preserves required occurrence sequence and multiplicity without inventing order. | V27-M | AP/PB | V20-10 |
| V27-N01 | 27.1; 27.4 | SeqScan and IndexScan preserve access-path-independent scan output IDs. | V27-N | PS | V22-B |
| V27-N02 | 27.7 | Filter preserves child LogicalSlotIds. | V27-N | PS | V23-A |
| V27-N03 | 27.9 | Limit preserves child LogicalSlotIds. | V27-N | PS | V20-11 |
| V27-N04 | 27.8 | Project uses every declared output entry's exact LogicalSlotId. | V27-N | PS | V22-B |
| V27-N05 | 27.1; 27.3 | Predicate-dependent columns are available but not accidentally exposed. | V27-N | PS | Ch37 tests |
| V27-N06 | 27.1–27.11 | Runtime names are not used for slot resolution. | V27-N | PS/IX | V22-A |
| V27-N07 | 27.1–27.11 | Active cardinality, not capacity or CONSTANT payload count, defines row occurrences. | V27-N | PS | V23-A/B |
| V27-N08 | 27.1–27.11 | Repeated DICTIONARY mappings preserve repeated occurrences. | V27-N | PS | V23-C/D |
| V27-N09 | 27.1; 27.4 | Hidden RID/internal slots never become user row identity. | V27-N | PS | V20-21 |
| V27-O01 | 27.2 | No scan output points into an unpinned heap page. | V27-O | BO | V23-G/H |
| V27-O02 | 27.7 | Filter view backing is stable through downstream use. | V27-O | BO | V26-M |
| V27-O03 | 27.8 | Project borrowed and computed backing each retain exact ownership. | V27-O | BO | V25-L |
| V27-O04 | 27.9 | Borrowed Limit slices remain stable through handoff. | V27-O | BO | V26-M |
| V27-O05 | 27.10 | Values payload lifetime covers every borrower or transfers ownership. | V27-O | BO | V23-G |
| V27-O06 | 27.11 | Result/cursor retention outlives pipeline backing safely. | V27-O | BO/PB | Ch31 |
| V27-O07 | 27.1–27.11 | Lifecycle resolution alone never authorizes resetting live borrowed backing. | V27-O | BO/AL | V26-M |
| V27-O08 | 27.1–27.11 | Diagnostic backing remains valid and growing owned storage is accounted. | V27-O/P | BO/ER | V24/V26-M |
| V27-P01 | 27.1–27.11 | V25-J remains ordinary non-DML expression-error owner. | V27-P | ER | V25-J |
| V27-P02 | 27.1–27.11 | V21-13 remains DML candidate owner with required metadata. | V27-P | ER | V21-13/V25-K |
| V27-P03 | 27.1–27.11 | Tuple/RID/chunk/worker physical order never ranks semantic candidates. | V27-P | ER | V25-J/K |
| V27-P04 | 27.1–27.11 | Heap/index/invalid-scalar corruption retains storage classification. | V27-P | SV/ER | Ch4/5/8 tests |
| V27-P05 | 27.1–27.11 | OOM, representability, and cancellation retain Ch24/39 ownership. | V27-P | ER | V24-L |
| V27-P06 | 27.1–27.11 | Chapter 27 introduces no global cross-class error precedence. | V27-P | ER | V26-I |
| V27-P07 | 27.1–27.11 | Failed current output is nonconsumable while prior handoffs/prefixes remain distinct. | V27-P | PB | V25-O/V26-J |
| V27-P08 | 27.1–27.11 | Cancellation prevents later same-runtime success and requires cleanup/quiescence. | V27-P | AF/PB | V26-K |
| V27-P09 | 27.1–27.11 | Authorized retry uses fresh operator/candidate state and no local transaction policy. | V27-P/R | AF | V26-L |
| V27-Q01 | 27.1–27.11 | Wrong child/output schema or Project result type is safely internal. | V27-Q | IX | V22-K/V25-N |
| V27-Q02 | 27.1 | Invalid page/slot cursor is rejected before unsafe range access. | V27-Q | IX | V23-M |
| V27-Q03 | 27.1; 27.4 | Missing snapshot/invalid index cursor/wrong relation RID is safely rejected. | V27-Q | IX | V22-K/Ch4 |
| V27-Q04 | 27.3–27.8 | Invalid residual mapping or non-BOOLEAN Filter result is internal after validation. | V27-Q | IX | V25-N |
| V27-Q05 | 27.9 | Negative validated Limit state or counter wrap is internal, not a new SQL error. | V27-Q | IX/LS | V20-11 |
| V27-Q06 | 27.1–27.11 | Expired borrows are rejected before dereference. | V27-Q | IX/BO | V23-M |
| V27-Q07 | 27.1–27.11 | Output after terminal or failed-output consumption is rejected before handoff. | V27-Q | IX/ST | V26-P |
| V27-Q08 | 27.1–27.11 | Failed-attempt state is rejected before reuse or persistent effect. | V27-Q | IX/AF | V26-L/P |
| V27-R01 | 27.1–27.11 | Legal chunk capacities/boundaries and empty progress preserve semantics. | V27-R | EQ/ST | V26-Q |
| V27-R02 | 27.1–27.5 | Page/slot/leaf/RID-batch boundary changes preserve semantics. | V27-R | EQ/HT/BR | scan/cursor tests |
| V27-R03 | 27.7–27.10 | Legal vector/selection/materialized/borrowed forms preserve semantics. | V27-R | EQ/BO | V23-C/D/L |
| V27-R04 | 27.1–27.11 | Valid worker partitions cover candidates without gaps/overlap or first-worker rank. | V27-R | EQ/HT/ER | Parallel Tests |
| V27-R05 | 27.1–27.11 | Pointer/address/runtime-ID perturbation has no semantic or persistent role. | V27-R | EQ/AF | V25-P/V26-P |
| V27-R06 | 27.1; 27.4 | Legal access-path changes preserve both-successful semantics. | V27-R | EQ/AP | V22-F/G |
| V27-R07 | 27.1–27.11 | Resource feasibility may differ across legal physical realizations. | V27-R | EQ | V24-L/V26-Q |
| V27-R08 | 27.1; 27.4 | Corruption exposure may differ only with actually accessed physical storage. | V27-R | EQ/SV | Ch4 tests |


### V27 negative-case and oracle checks

Search Chapter-27-relevant Verification text and reject these stale rules: FSM defines full
scan domain; selected columns alone receive structural validation; malformed invisible tuple
is always ignored; empty/short page or batch is EOS; snapshot refreshes per chunk; index hit
proves visibility; UNIQUE means first physical entry; duplicate user keys are deduplicated;
stale RID may name replacement data; leaf/RID-batch end is range end; `= NULL` means IS NULL;
residual omission needs no proof; RID batching is chronology-bound implementation sequencing or always
creates order; PageId grouping is always legal; Project columns sort by numeric LogicalSlotId;
OFFSET must scan despite LIMIT zero; Limit counts physical candidates; finite `o+l` overflow
is SQL error; counter wrap is acceptable; satisfied Limit probes EOS; sink acceptance equals
publication/success; failed sink input is replayed; and first tuple/RID/lane/chunk/worker is
the semantic error. Negative fixtures above make every stale form nonconforming.

Use the RID-batch and Project-column fixtures under §§27.5 and 27.8 across permitted
containers, counters, batching, vector forms, safe ownership mechanisms, and worker
schedules. Use deterministic controls and independent oracles rather than sleeps or
production-derived expected results.
