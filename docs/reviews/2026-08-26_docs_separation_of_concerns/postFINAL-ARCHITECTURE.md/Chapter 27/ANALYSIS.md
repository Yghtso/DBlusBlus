# CHAPTER 27 — TARGETED DOCUMENT FIXES RECOMMENDED

The architectural semantics are consistent with their canonical owners. I found **two MINOR, DOCUMENT-ONLY findings**, no blocking or major defects, and no frozen semantic question.

Chapter 27 is **not yet fully closed**: targeted wording cleanup should precede Chapter-27 Verification synchronization.

## 1. Repository and scope

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | Clean |
| Index | Clean | Clean |
| HEAD | `fa253734c2161fefb938c83dbcb5a436bf509b7b` | Same |
| Audit-created changes | None | None |
| `git diff --check` | — | Passed |

Historical review artifacts were unread, unmodified, unmoved, and unstaged. No external repository change was observed through these checks.

No file was modified. No implementation, build, test, sanitizer, benchmark, staging, commit, devlog, report-file creation, or Verification synchronization occurred.

## 2. Exact live scope

[Chapter 27](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21223):

- Exact title: **27. Scans and Unary Physical Operators**
- Start: **21223**
- End: **21481**, immediately before Chapter 28
- Next heading: **# 28. Join Execution**, line **21482**
- Immediate downstream handoff: unary/source output becomes input to the join family.
- Chapter 27 contains **no explicit Chapter-28 reference**.

Only Chapter 28’s heading and introductory operator-family boundary were inspected—not its algorithms or correctness contracts.

### Context consulted

The review used the relevant ownership passages in Chapters 5, 8–11, 14, 16–17, 19–26, 31–32, 37, and §39, including Chapter-15 statement semantics through their canonical Chapter-9/21/39 handoffs.

Necessary referenced context also included:

- §4.13: mandatory page, tuple, and followed-reference validation;
- §36.11–§36.16: actual access-predicate, NULL-bound, composite-bound, and residual owners;
- §38.24: final physical-plan validation.

These were dependency checks, not reviews reopening those chapters.

### Verification consulted

Read-only inspection covered:

- Heap page/free-list/tuple-format verification;
- IndexKeyCodec, physical order, routing, duplicate ranges, and cursor lifetime;
- Heap/index visibility-error propagation;
- Read-epoch, index-cleanup, and RID-reuse methodology;
- V20-4/5/10/11 and demanded-evaluation references;
- V22-A–G;
- V23-A/B and borrowing/schema references;
- V25-J/K/O/P;
- V26-A/C/D/J/K/L/M/N/Q and its oracle registry;
- **Scan and Unary Operator Tests**;
- **Access Path Tests**;
- **Physical Property and Enforcement Tests**.

## 3. Findings

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 2 |
| EDITORIAL | 0 |

### N27-1 — RID batching contains implementation chronology

- **Sections:** §27.5; §27.12 invariant 8.
- **Evidence:** [lines 21339–21347](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21339) contain “Initial target,” “The initial implementation,” and “A future implementation”; [line 21473](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21473) repeats “Initial index RID batching.”
- **Severity:** MINOR.
- **Primary type:** TEMPORALITY.
- **Classification:** DOCUMENT-ONLY.
- **Affected handoff:** RID batching → ordering preservation/property advertisement.
- **Comparison:** §§8.20, 22.7, and 37.5 already establish cursor direction, execution/property separation, and truthful ordering advertisement.
- **Consequence:** Development chronology obscures the durable batching policy and ordering condition. It does not override those canonical owners.
- **Smallest future action:** Replace the four chronology-bearing occurrences with timeless batching-policy and ordering-preservation wording. Preserve the small-batch design, tuning character of the capacity target, and property constraint.

### N27-2 — Project “output order” conflates column order with row order

- **Section:** §27.8.
- **Evidence:** [line 21425](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21425): “Its output order is the projection’s LogicalSlotId order.”
- **Severity:** MINOR.
- **Primary type:** TERMINOLOGY.
- **Classification:** DOCUMENT-ONLY.
- **Affected handoff:** Project → physical output schema → DataChunk columns.
- **Comparison:** §§20.7, 22.3, and 23.1 determine declared output-column sequence; §37.5 separately governs retained ordering keys.
- **Consequence:** Read alone, the sentence can suggest numeric LogicalSlotId sorting or a row-order guarantee. Neither interpretation survives the canonical schema contract.
- **Smallest future action:** Say that output **columns follow the declared ordered physical output schema**, distinguishing that from preservation of row-occurrence sequence and applicable ordering properties.

There are no other findings. In particular, absent repeated explanations of upstream rules were not counted as architectural defects.

# 4. Required review matrices

In the matrices below, **consistent** means architecturally determined, not implemented or tested. Canonical owner references are Architecture sections.

## A. Section-review matrix

All subsection headings are reproduced exactly.

| Section and lines | Exact heading | Responsibility | Upstream owner | Runtime owner → consumer | Documentation classification |
|---|---|---|---|---|---|
| 27.1, 21225–21265 | Physical sequential scan | Heap access, visibility, selective decoding | 5, 9–10, 16, 20, 22 | Local source/context → pipeline | ARCHITECTURE-APPROPRIATE |
| 27.2, 21266–21284 | Scan page and string lifetime | Page-to-chunk ownership | 5, 23 | Page guard/chunk → downstream | ARCHITECTURE-APPROPRIATE |
| 27.3, 21285–21298 | Scan predicate pushdown boundary | Execute approved pushed predicates | 17, 20, 22 | Scan expression state → survivors | ARCHITECTURE-APPROPRIATE |
| 27.4, 21299–21334 | Physical index scan | Bounds/cursor/RID/heap/MVCC chain | 8, 10, 14, 22 | Source/context → pipeline | ARCHITECTURE-APPROPRIATE |
| 27.5, 21335–21350 | RID batching | Candidate batching and order | 8, 24, 37 | Query-local batch → heap fetch | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 27.6, 21351–21378 | Index ordering property | Forward provider eligibility | 8, 17, 22, 37 | Cursor → property consumer | ARCHITECTURE-APPROPRIATE |
| 27.7, 21379–21398 | PhysicalFilter | Predicate selection and stable views | 17, 20, 23–26 | Local operator → downstream | ARCHITECTURE-APPROPRIATE |
| 27.8, 21399–21426 | PhysicalProject | Expression vectors and output ownership | 20, 22–26 | Expression/local state → downstream | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 27.9, 21427–21445 | PhysicalLimit | Offset/count selection and early stop | 19–20, 26 | Local counters → downstream | ARCHITECTURE-APPROPRIATE |
| 27.10, 21446–21455 | PhysicalValues | Bound typed literal source | 19–20, 22–23 | Local source/plan payload → pipeline | ARCHITECTURE-APPROPRIATE |
| 27.11, 21456–21463 | PhysicalResultSink foundation | Final-chunk consumption/lifetime handoff | 23–26 | Sink → Ch31 result owner | ARCHITECTURE-APPROPRIATE |
| 27.12, 21464–21481 | Scan/unary invariants | Cross-section invariant summary | Detailed owners above | All roles → consumers | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |

The chronology-bearing portions of §27.5 are development-style material leakage; the section as a whole remains architectural.

## B. Canonical-owner matrix

| Mechanism | Classification | Canonical owner | Chapter-27 responsibility |
|---|---|---|---|
| Sequential physical traversal | EARLIER OWNER | §5.2 | Execute page/slot traversal |
| Page/tuple structural validity | EARLIER OWNER | §4.13; §§5.3–5.19 | Consume validated storage; propagate failure |
| MVCC visibility | EARLIER OWNER | Ch10 | Apply exact visibility |
| Snapshot/CommandId | EARLIER OWNER | Ch9; §22.5 | Use execution context |
| Catalog identity/history | EARLIER OWNER | §§16.6–16.10 | Decode using resolved descriptors |
| B+ structure/comparator/cursor | EARLIER OWNER | Ch8 | Consume its candidate stream |
| Access-path eligibility | REFERENCED ONLY | §22.4.1; §§36.11–36.16 | Execute selected legal path |
| Scan materialization | CHAPTER 27 OWNS | §§27.1–27.4 | Produce exact chunk values |
| Pushed-predicate execution | CHAPTER 27 OWNS | §27.3 | Execute approved predicate/dependencies |
| Residual semantics | EARLIER OWNER | §17.10.3; Ch20; §36.14 | Preserve residual qualification |
| RID batching | CHAPTER 27 OWNS | §27.5 | Temporary candidate batches |
| RID reuse/retention | EARLIER OWNER | Ch14 | Hold protection through use |
| Filter physical transformation | CHAPTER 27 OWNS | §27.7 | Realize TRUE-only subset |
| Project physical transformation | CHAPTER 27 OWNS | §27.8 | Realize declared outputs |
| Limit physical transformation | CHAPTER 27 OWNS | §27.9 | Realize logical slicing/early stop |
| Values physical source | CHAPTER 27 OWNS | §27.10 | Emit typed source occurrences |
| Generic source/streaming/sink protocol | EARLIER OWNER | Ch26 | Conform without alternate protocol |
| Vector legality/borrowing | EARLIER OWNER | Ch23 | Preserve active domain/lifetime |
| Expression demand/error selection | EARLIER OWNER | Ch20; Ch25; §21.16.1 | Preserve owners and provenance |
| Resource accounting | EARLIER OWNER | Ch24 | Account retained/growing state |
| External result publication | LATER OWNER | §§31.9–31.10 | Hand off; do not redefine |
| Worker mechanics | LATER OWNER | Ch32 | Supply appropriate local state |
| Physical properties | LATER OWNER | Ch37 | Advertise only guaranteed order |
| Transaction/error consequences | LATER OWNER | §39 | Propagate, not independently decide |

No duplicated or ambiguous **normative owner** was found. N27-2 is local terminology, not an unresolved ownership assignment.

## C–D. Complete operator inventory and pipeline contracts

| Exact operator | Input | Output schema | Role | Mutable state | Multiplicity/order | Expressions | Terminal condition |
|---|---|---|---|---|---|---|---|
| `PhysicalSeqScan` | Resolved heap relation | Declared scan slots/types | Source | Page/slot progress, chunk state | One qualifying visible occurrence; physical page/slot traversal, no SQL order | Approved pushed predicates | Candidate domain exhausted, or valid stop |
| `PhysicalIndexScan` | Encoded selected bounds/index descriptor | Declared scan slots/types | Source | B+ cursor, RID batch, heap/output progress | Qualifying visible occurrences; compatible forward order | Required residual/predicate plan | Bound/cursor exhaustion, or valid stop |
| `PhysicalFilter` | Child chunk/active domain | Child schema unchanged | Streaming | Predicate/selection/output state | TRUE subset; survivor sequence preserved | Bound BOOLEAN predicate | Upstream complete and obligations discharged |
| `PhysicalProject` | Child logical occurrences | Declared ordered output entries | Streaming | Expression/output state | One output row per demanded input row; sequence preserved | Declared expressions | Upstream complete and obligations discharged |
| `PhysicalLimit` | Child occurrence stream | Child schema unchanged | Streaming | `rows_skipped`, `rows_emitted` | Offset then limit; no deduplication | Validated count handoff, not per-chunk acquisition | Limit satisfied or child exhausted |
| `PhysicalValues` | Bound typed literal relation | Declared Values schema | Source | Row/chunk position | Listed multiplicity; no independent SQL order | No parsing/type resolution | All required source occurrences offered |
| `PhysicalResultSink` | Final chunks | Consumes result schema | Sink | Result handoff/retention state | Preserve submitted occurrences/required order | No binding/type resolution | Result-owner lifecycle |

All source/streaming final output precedes terminal/no-output. Streaming acceptance follows §26.4.2; successful sink acceptance covers the complete submitted domain under §26.4.3.

## E. Access-path substitutability

| Path | Candidate source | Visibility | Heap recheck | Predicate/residual | Ordering | Successful row bag/schema | Error owner | Physical-failure freedom |
|---|---|---|---|---|---|---|---|---|
| SeqScan | Relation’s published heap pages/NORMAL slots | Ch10/context | Storage validation before visibility | Approved pushed predicates; remaining Filter | None from scan | LogicalGet semantics; declared schema | D25 or D21; lower storage owner | Page work/resources may differ |
| IndexScan | Exact selected B+ range/RIDs | Same Ch10/context | Required heap fetch, identity/state checks, MVCC | Unproved predicate remains residual | Exact compatible forward property only | Same selected logical semantics; same slot mapping | Same semantic owner | Additional/different index/heap access may fail |

Both successful paths preserve values, NULLs, multiplicity, required order, and demanded semantic outcomes. Unordered LIMIT and order ties use the owner’s **allowed-result predicate**, not a fabricated canonical RID sequence.

Different physical paths may legitimately encounter different resource or storage failures. No generic runtime fallback-on-index-error contract is introduced.

## F. SeqScan candidate/visibility matrix

| Case | Candidate/validity | Visibility | Materialize/output | Progress/terminal | Owner/status |
|---|---|---|---|---|---|
| Ordinary visible NORMAL tuple | Valid candidate | Visible | Required values; one occurrence | Advance | Ch5/10; consistent |
| Invisible NORMAL tuple | Valid candidate | Invisible | No result | Advance | Ch10 |
| Deleted version | Valid candidate | Snapshot-dependent | Only if visible | Advance | §10.3 |
| Aborted creator | Structurally valid | Invisible | No result | Advance | §10.2 |
| UNUSED slot | No tuple candidate | Not evaluated | Never dereference | Advance | §4.13.3 |
| DEAD retained/reclaimed | Nonreturnable | No ordinary row | No output | Advance | §4.13.3/Ch14 |
| Empty page | Valid page | No candidates | None | Advance | Ch5/26 |
| All-invisible page | Valid candidates | All invisible | None | Finite advance, not EOS by size | Ch10/26 |
| Malformed page | Invalid | Not used | None | Failure | §4.13/39 |
| Malformed retained tuple | L1 failure | Not a skip | None | Failure | §4.13.3 |
| Final visible tuple | Valid/visible | Visible | Offer exactly once | Handoff, then terminal | §26.4.1 |
| No visible tuples | Valid empty result | None | No fake output required | Explicit completion | §26.4.1 |
| Cancellation | Invocation unsuccessful | — | No failed current output | Cleanup/quiescence | §26.7/39 |
| Allocation failure | Valid value, failed resource acquisition | — | No omitted-row success | Failure | §24.10 |

The candidate page domain is the relation-owned published heap-data range, not FSM discovery or filesystem enumeration. Page/slot order is frozen by §5.2, while physical tuple byte offsets are not traversal identity.

## G. IndexScan candidate/recheck matrix

| Case | Visit/fetch | Required check | Residual/output | Progress/terminal | Classification |
|---|---|---|---|---|---|
| Visible matching entry | Visit and heap fetch | Identity/state/MVCC | Emit if remaining qualification passes | Advance | Normal |
| Invisible matching tuple | Visit/fetch | MVCC | Skip | Advance | Normal |
| Equal user keys, different RIDs | Visit every required entry | Per-candidate checks | Preserve qualifying duplicates | Across leaves/chunks | Normal |
| Exact duplicate physical entry | Invalid stored structure | Strict physical-key checks | No valid duplicate output | Failure | `CORRUPT_INDEX` |
| Aborted/dead historical entry | Candidate, not visibility proof | Heap/state/MVCC | No visible occurrence | Advance | Normal garbage, not corruption merely by age |
| Captured RID subsequently retired to DEAD | Protected identity | State check before tuple access | Nonreturnable; never dereference reclaimed payload | Safe stale handling | Ch14 + §4.13 |
| Reused/UNUSED target under live protection | Violates reuse/target contract | Reject before access | No unrelated row | Failure | Invariant/corruption owner |
| Wrong relation/out-of-range RID | Reject/fetch only safely | Exact identity/domain | None | Failure | Storage owner |
| Residual TRUE | Fetch/recheck | Ch25 demand | Emit once | Advance | Normal |
| Residual FALSE/UNKNOWN | Fetch/recheck | Ch25 demand | Skip | Advance | Normal |
| Bound end | Cursor detects endpoint | Exact comparator/inclusivity | None beyond bound | Local terminal | Ch8 |
| Empty qualifying range | May traverse invisible/rejected entries | All applicable checks | None | Explicit completion | Ch8/26 |
| Final qualifying entry | Normal | All checks | Offer once | Then terminal | Ch26 |
| Malformed index node | No ordinary use | L0/L1/L2 | None | Failure | `CORRUPT_INDEX` |
| Heap corruption | Fetch fails validation | Heap owner | None | Failure | `CORRUPT_HEAP` |
| Cancellation/resource failure | No successful continuation | Canonical owner | Current failed output invalid | Cleanup | Ch24/26/39 |

A read epoch prevents **identity reuse**, not every NORMAL→DEAD transition or payload reclamation. Safe state checking is therefore still necessary. This is determined by the existing storage/reclamation contract, not a new generation field or mandatory key-copy design.

## H. Index range/bound matrix

| Bound case | Contract | Owner/status |
|---|---|---|
| Non-NULL equality | Entire matching user-key/RID range | Ch8; §§36.12–36.13 |
| `IS NULL` | Exact nullable-key NULL range permitted | §36.12 |
| Ordinary `= NULL` | UNKNOWN; not NULL equality lookup | §17.10.3; §36.12 |
| Inclusive lower/upper | Include exact endpoint class as specified | §36.13 |
| Exclusive lower/upper | Exclude specified endpoint class | §36.13 |
| Equality prefix then range | Legal tight composite prefix | §36.12 |
| Constraint beyond range component | Residual, not falsely tight suffix | §36.12 |
| Unspecified trailing component | Transient low/high search sentinel | §36.13 |
| Duplicate endpoint | Transient RID-bound sentinel | Ch8; §36.13 |
| Bound outside supported applicability | Planner retains conforming alternative | §22.4.1 |
| Runtime bound handling | Consume selected encoded bound; no independent coercion policy | §§17/22/27.4 |
| Predicate not exactly represented | Preserve residual | §17.10.3; §36.14 |

The chapter receives encoded bounds; it does not authorize per-chunk SQL-expression reacquisition or parameterized/correlated scan semantics.

## I. Index ordering/property matrix

| Case | Provided ordering | Consequence |
|---|---|---|
| Compatible forward scan | ASC/NULLS FIRST matching key prefix, type/collation/slots | May satisfy exact required prefix |
| Requested DESC | Not provided by forward traversal | Another provider/enforcer required |
| Different NULL placement | Not equivalent | Cannot advertise satisfaction |
| Equal user keys | Physical RID tie order | Not an additional SQL tie guarantee |
| MVCC/residual filtering | Removes candidates without reordering survivors | Compatible ordering preserved |
| RID fetch regrouping | Cannot violate promised/needed order | §27.5 wording needs timeless cleanup |
| Missing/remapped ordered output key | Property cannot be blindly carried forward | Ch37 slot-based proof |
| Parallel interleaving | No order from incidental completion | Valid provider/merge required |

## J. Filter semantics matrix

| Case | Demand | Output | Order/representation/lifetime | Error owner |
|---|---|---|---|---|
| TRUE | Predicate demanded | One occurrence | Survivor order | — |
| FALSE | Predicate demanded | Zero | No deduplication operation | — |
| UNKNOWN/NULL | Predicate demanded | Zero | No truthiness coercion | — |
| Empty input | Empty per-row demand | Zero | Valid resolution, not EOS | Binding-time rules separate |
| Repeated dictionary occurrence | Each logical occurrence | Each TRUE occurrence retained | Repetition preserved | D25/D21 |
| Predicate error | Only semantic demand | Failed current output invalid | No first-lane selection | D25 or D21 |
| Borrowed VARCHAR | Predicate/outputs as required | Stable view permitted | Bytes and view metadata remain stable | Ch23/26 |
| All rejected | All required predicates | Zero | Finite input resolution | Ch26 |
| Mixed selection | Active logical domain only | Exact TRUE subsequence | Dictionary/reference or valid materialization | Ch23/25 |

Filter preserves child LogicalSlotIds. It does not create SQL ordering.

## K. Project schema/occurrence matrix

| Case | Occurrences/slots | Ownership/position | Demand/provenance |
|---|---|---|---|
| Direct column | One output row/input row | Borrow permitted; declared output position | Source mapping preserved |
| Computed scalar | Same row count | Initialized result vector | Declared expression demanded |
| NULL result | Same row count | Correct validity; inactive payload not a value | Exact NULL semantics |
| Constant result | Logical cardinality unchanged | CONSTANT representation permitted by vector owner | Not one row merely because one payload |
| Equal expressions twice | Two declared output columns | Distinct output IDs | Sharing cannot erase origins |
| Equal values/distinct IDs | Both outputs remain | No identity from equality | Exact schema |
| Computed VARCHAR | Same row count | Output/valid result owner | Exact bytes, no scratch escape |
| Borrowed VARCHAR | Same row count | Full stable borrow interval | No early reset |
| Empty input | Zero per-row results | No fake row | No per-row scalar demand |
| Expression failure | No successful current result | Partial output unusable | D25/D21 owner |
| Required-slot pruning | Only semantically legal rewrite | Required mapping retained | RequiredSlotSet alone is not error-erasure proof |

Ordinary declared Project expressions remain demanded. EXISTS projection irrelevance is a specialized owner rule, not a general pruning license.

## L. LIMIT/OFFSET matrix

Let child sequence contain `n` qualifying logical occurrences. For fixed sequence, offset `o`, and finite limit `l`, the selected slice is `child[o : o+l]` **mathematically**, without requiring finite-width computation of `o+l`.

| Case | Skipped/emitted | Child demand/remainder | Terminal/error/order |
|---|---|---|---|
| OFFSET 0 / LIMIT 0 | 0 / 0 | No relational rows needed | Immediate satisfied-limit path |
| OFFSET 0 / LIMIT 1 | 0 / at most 1 | Stop after required occurrence | Preserve child sequence |
| OFFSET 1 / LIMIT 0 | 0 physically required / 0 | OFFSET does not force fetching rows for an already-empty result | Count acquisition still required |
| OFFSET 1 / LIMIT 1 | First 1 / next at most 1 | Stop once satisfied | Normal early stop |
| OFFSET > input | All available / 0 | Child may exhaust before skip completes | Success, not overflow |
| LIMIT > remaining | Offset / all remaining | Exhaust required child | Success |
| Offset inside chunk | Prefix skipped | Remaining slice eligible | No gap/duplicate |
| Limit inside chunk | Required prefix emitted | Rest safely unnecessary | Final offer before terminal |
| Empty child batch | 0 count change | Finite progress only | Not EOS |
| Invisible scan candidates | Do not reach counting domain | Do not count | Ch10 |
| Residual failures | Do not reach counting domain | Do not count | Ch20/25 |
| Unordered child | Allowed exact-cardinality subbag | No canonical RID prefix | Ch20 allowed-result semantics |
| Ordered child | Exact ordered slice | Required ordering must already hold | Ch20/37 |
| INT64_MAX offset | Exact skip logic | No wrap | No new count error |
| INT64_MAX limit | Exact bounded emission | No wrap | No new count error |
| Final output | Offered once | No unnecessary source probe | Ch26 |
| No finite LIMIT | Skip offset, stream remainder | No count-based early stop | Child completion |
| Early stop | Required output discharged | No new unnecessary input | Not QueryCancelled |

**LIMIT-zero conclusion:** after mandatory binding/count acquisition, zero required rows mean no relational child fetch merely to consume OFFSET or obtain EOS. This does **not** promise absence of every initialization/resource operation, suppress binding-time errors, or erase separately required statement work.

Counter representation is free; silent wrap or a new SQL count-overflow error is not.

## M. Limit chunk-boundary/early-stop matrix

| Boundary | Required behavior | Lifecycle/borrow consequence |
|---|---|---|
| OFFSET skips whole chunk | Zero output; count actual occurrences | Resolve input with progress |
| OFFSET ends inside chunk | Start output at exact remaining occurrence | View/copy both legal |
| LIMIT ends inside chunk | Offer only required prefix | Remaining work may be abandoned semantically |
| Final output still downstream | No new upstream input | Preserve borrowed backing |
| Empty nonterminal input | No skip/emission count increment | Progress is not count advancement |
| Limit satisfied | No fetch merely to see source FINISHED | Output handoff precedes terminal |
| Required independent branch remains | Do not cancel it | Query success still gated |
| Later undemanded scalar error | No candidate/public error | D20/25 owner |

## N. ResultSink handoff matrix

| Case | Acceptance | Ownership | Client visibility/query success | Replay/error |
|---|---|---|---|---|
| SELECT chunk | Whole submitted domain once | Sync or retained | Neither implied by acceptance | No duplicate replay |
| Ordered SELECT chunk | Same | Preserve required order | Ch31 controls exposure | — |
| Empty input | Empty submitted domain | Valid empty handling | Not EOS by cardinality | — |
| Retained chunk | Complete success only with valid retention | Own/copy/safely retain | Ch31 lifetime | Ch24 on failure |
| Synchronous handoff | Consume within valid interval | No escaping borrow | External event separate | — |
| Retention allocation failure | Not successful acceptance | Cleanup partial state | No failed-current publication | OOM; no blind replay |
| Cancellation | Unsuccessful path | Quiesce/release | No later success from same instance | QueryCancelled owner |
| RETURNING | Statement-owned envelope | Spool/result owner | §31.9 gates exposure | D21/39 |
| Prior cursor chunk | Prior completed delivery remains | Declared cursor interval | Prefix is not complete success | Later failure does not retract delivery |

## O. Snapshot/MVCC handoff matrix

Both scans apply the same result.

| Creator/deleter case | Command/snapshot condition | Scan result | Owner |
|---|---|---|---|
| Frozen creator | Valid structure | Creator visible | §10.2 |
| Self creator, earlier command | `cmin < command` | Creator visible | §10.2 |
| Self creator, same command | `cmin == command` | Invisible | §10.2 |
| Self future command | Impossible causal metadata | Error, not invisible | §10.4 |
| Other committed creator | Before horizon, absent active set | Creator visible | §10.2 |
| Other in-progress/aborted creator | Valid state | Invisible | §10.2 |
| No deleter | Canonical `xmax=0,cmax=0` | Remains visible | §10.3.1 |
| Self deleter, earlier command | `cmax < command` | Invisible | §10.3.2 |
| Self deleter, same command | `cmax == command` | Remains visible | §10.3.2 |
| Other aborted/in-progress deleter | Valid state | Remains visible; no read lock wait | §10.3.3 |
| Other committed deleter | Visible to snapshot | Invisible | §10.3.3 |
| Status lookup failure/impossible status | Error-capable visibility | Propagate, never Boolean fallback | §10.4 |
| READ COMMITTED | One snapshot per attempt | No page/chunk refresh | §9.9 |
| REPEATABLE READ | Transaction horizon/current command | Same across read tasks | §9.10 |

## P. RID/version/retention matrix

| Case | Identity/output | Reuse guard | Normal skip versus failure |
|---|---|---|---|
| Visible RID | Physical version identity; eligible row | Page/epoch ownership | Normal output |
| Invisible old version | No output | Existing identity protection | Normal skip |
| Historical index entry | Legal physical garbage | Ch14 cleanup protocol | MVCC/state rejection |
| Captured RID retired to DEAD | Never reinterpret as a new row | Epoch prevents rebinding | Nonreturnable stale state |
| Reused slot with old live claimant | Forbidden | Epoch + claims + index/link barriers | Invariant failure |
| Retained DML target RID | Internal system slot | Gap-free epoch→TUPLE_WRITE handoff | Ch11/14 revalidation |
| Duplicate exact `(key,RID)` entry | Illegal duplicate identity | B+ structural contract | Corruption |
| Logical DELETE | Old entry may remain | Vacuum removes later | Snapshot decides |
| UPDATE | New physical version/new RID | Old/new identities separate | No latest-tuple shortcut |
| Heap compaction | Slot identity unchanged | Guard/latch protects bytes | Offset movement is not RID change |

No RID generation field is invented.

## Q. Scan continuation/progress matrix

| Source/event | Progress state | Output/completion rule |
|---|---|---|
| SeqScan slot boundary | Next candidate slot | No duplication/skip |
| SeqScan page boundary | Next published relation page | Page end is not query EOS |
| Empty/invisible page | Finite candidate advancement | Empty output legal |
| Output capacity boundary | Resume next unoffered occurrence | Capacity does not encode terminal |
| Index duplicate range | Next physical entry | Equal keys span leaves/chunks |
| Index leaf boundary | Ch8 latch-coupled handoff | Leaf end is not range end |
| Invisible index candidates | Cursor advances | No output required per candidate |
| Range end | Exact bound/cursor exhaustion | Explicit terminal |
| Values boundary | Next listed occurrence | Preserve listed multiplicity |
| Final nonempty batch | Output → handoff → terminal | Exactly once |
| Unchanged empty loop | No relevant advancement | Internal liveness violation |
| Post-terminal request | Outside protocol | No rewind/restart |

## R. Output-schema/LogicalSlotId matrix

| Operator | Mapping |
|---|---|
| SeqScan | Required relation ColumnIds resolved into declared physical output entries |
| IndexScan | Same semantic outputs independent of access path |
| Filter | Child IDs, types, metadata, positions pass through |
| Project | Declared output-column sequence; distinct IDs remain distinct |
| Limit | Child schema/IDs unchanged |
| Values | Bound typed relation’s declared schema |
| ResultSink | Complete required final result schema consumed |
| Hidden RID | Internal required slot; never ordinary `SELECT *` output |
| Predicate-only column | Retained while required, not accidentally exposed as final output |
| Reordered physical layout | Updated schema mapping, not runtime name lookup |

## S. Borrow/string ownership matrix

| Backing | Permitted use | Release/reset condition | Accounting |
|---|---|---|---|
| Heap fixed-width bytes | Decode/copy to output | Guard after storage use | Buffer owner |
| Heap VARCHAR | Copy exact bytes into chunk storage | No output page pointer escapes | Chunk/query owner |
| Filter view | Stable dictionary/reference | All live consumers end or preserve independently | Ch24 |
| Project pass-through | Stable borrow | Same | Ch24 |
| Project computed VARCHAR | Valid result owner | Result lifetime complete | Ch24 |
| Limit slice | Stable view or copy | Downstream borrow ends | Ch24 |
| Values plan payload | Borrow only if plan payload outlives every consumer | Otherwise copy | Existing plan/query owner |
| ResultSink retention | Own/copy/safely retain | Ch31 consumer lifetime | Ch24/31 |
| Diagnostic backing | Preserve until reporting ends | No stale chunk reference | Ch24/26 |
| Failed execution | Quiesce users before destruction | No failed continuation resumes | Ch24/26 |

## T. Error-owner matrix

| Failure | Chapter-27 role | Selection/category owner | First physical discovery sufficient? | Current failed output | Transaction owner |
|---|---|---|---|---|---|
| Ordinary scalar error | Transport provenance/candidate | D25-S1 | No | Invalid | §39 |
| DML expression candidate | Preserve eligibility/phase | D21-S4 | No | Invalid on failed invocation | §39 |
| Heap corruption | Stop/propagate | §4.13/Ch5/§39 | No invented cross-class ranking | Invalid | NC owner |
| Index corruption | Stop/propagate | §4.13/Ch8/§39 | Same | Invalid | NC owner |
| Invalid persisted scalar | Storage failure, not cast/default | Tuple/scalar codec | Same | Invalid | §39 |
| OutOfMemory | Propagate resource failure | §24.10 | Not D25/D21-ranked | Invalid | §39 |
| Representability failure | Preserve resource cause | §24.10 | Not semantic candidate | Invalid | §39 |
| QueryCancelled | Terminal unsuccessful path | Ch26/§39 | No global precedence invented | Invalid | §39 |
| ResultSink failure | No successful acceptance | Origin owner/Ch31 | No blind replay | Invalid | §39 |
| Internal invalid state | Prevent unsafe use | Ch22–26/§39 | Not public SQL error ranking | Invalid | NC owner |

## U. Invalid-runtime-state matrix

All runtime-only malformed cases below are internal, not new public SQL errors. Persisted malformed bytes retain storage classifications.

| Invalid state | Safe rejection point | Principal risk | Persistent effect allowed? | Owner |
|---|---|---|---|---|
| Wrong output schema | Before consumer access | Wrong value/slot | No | Ch22/23 |
| Wrong child schema | Before expression access | Wrong value/type | No | Ch22/25 |
| Invalid page/slot cursor | Before range access | Unsafe read/skip | No | Ch5/26 |
| Missing effective snapshot | Before visibility | Invisible exposure | No | Ch9/22 |
| Invalid index cursor | Before key/reference access | Duplication/unsafe read | No | Ch8/26 |
| Invalid residual mapping | Before evaluation | Wrong predicate | No | Ch22/25 |
| Filter non-BOOLEAN result | Before selection | Wrong retained set | No | Ch20/25 |
| Project TypeId mismatch | Before result consumption | Wrong decode | No | Ch23/25 |
| Negative already-validated Limit state | Before count logic | Wrong slice | No | Ch19/20/26 |
| Limit counter wrap | Prevent in exact count logic | Wrong cardinality | No | Ch20/27 |
| Expired ResultSink borrow | Before dereference/reset | Use-after-free | No | Ch23/26/31 |
| Output after terminal | Before offer | Duplicate result | No | Ch26 |
| Failed output consumed | Before handoff | Partial/stale result | No | Ch25/26 |
| Failed attempt reused | Before retry admission/use | Skipped/duplicated work | No | Ch21/26 |

## V. Retry/cancellation matrix

| State | Authorized fresh execution | Failure/cancellation behavior |
|---|---|---|
| SeqScan page/slot position | Reinitialize | No resume after terminal failure |
| Index cursor/RID batch | Fresh cursor/ownership | Release safely after quiescence |
| Filter selection/scratch | Fresh logical state | No stale mask |
| Project results/candidates | Fresh state | No partial successful result |
| Limit counters | Fresh counts | No inherited skipped/emitted totals |
| Values cursor | Fresh source state | No inherited exhaustion |
| ResultSink acceptance/buffers | Fresh attempt-owned state | No blind replay |
| Immutable plan | May be reused under owner | Never mutated into failed runtime state |

Retry admission stays Chapter-21/§39-owned. Cancellation checks use owner-defined block/chunk boundaries, not a new wall-clock interval.

## W. Determinism/chunking matrix

`=` means owner-equivalent values, NULLs, bag/cardinality, required order, eligible D25/D21 result, and transaction/result envelope—not identical physical traces.

| Perturbation | Successful semantics | Resource feasibility | Corruption/storage exposure |
|---|---|---|---|
| Chunk capacity | = | May differ | Physical access may differ |
| Chunk boundaries | = | May differ | No weakened validation |
| Empty progress batches | = | May differ | No added semantic rows |
| Page batching | = | May differ | Accessed pages still validated |
| Slot batching | = | May differ | Mandatory local validation retained |
| Index leaf boundaries | = | May differ | Every followed link validated |
| Selection representation | = | May differ | Runtime-invalid state stays internal |
| FLAT/CONSTANT/DICTIONARY | = | May differ | Same active logical domain |
| Materialization strategy | = | May differ | No stale borrowed views |
| Worker schedule | = under allowed-result semantics | May differ | No owner override |
| Valid scan partitioning | =; no gaps/overlap | May differ | Same validation obligations |
| Pointer/address | = | No semantic role | No identity oracle |
| Runtime IDs | = | No semantic role | Never persistent identity |
| Access path | = under selected logical semantics | May differ | Different accessed structures may fail |

For unordered LIMIT or equal-order-key ties, alternative permitted rows/sequences are not false determinism failures.

## X. Physical-property/execution-trait matrix

| Operator | Ordering output | RequiredSlotSet | Trait/role | Breaker? |
|---|---|---|---|---|
| SeqScan | None | Output + predicate dependencies | Source | Not inherently |
| IndexScan | Exact compatible forward property | Heap/output/residual needs | Source | Not inherently |
| Filter | Preserves supplied order | Predicate + retained outputs | Streaming | No |
| Project | Retained unchanged ordered keys only | Declared/required semantic outputs | Streaming | Explicitly no |
| Limit | Preserves selected child sequence | Pass-through | Streaming | No |
| Values | No independent SQL order | Declared typed schema | Source | No |
| ResultSink | Preserve required input order | Complete result schema | Sink | Not automatically |

Chapter 27 adds no property beyond Chapter 37’s `OrderingProperty` and `RequiredSlotSet`. Streaming/blocking/source/sink remain roles or traits.

## Y. Cross-chapter handoff matrix

| Handoff | Contract/owner | Chapter-27 action | Duplication/ambiguity |
|---|---|---|---|
| Ch5→27 | Heap domain/tuple format | Traverse/decode valid versions | None |
| Ch8→27 | Encoded range/cursor/duplicates | Consume candidate RIDs | None |
| Ch9→27 | Snapshot/CommandId | Use context | None |
| Ch10→27 | MVCC/error-capable visibility | Emit only visible versions | None |
| Ch11→27 | Read versus DML lock ownership | No scan-local lock policy | None |
| Ch12→27 | Published storage/error boundary | No executor WAL identity | None |
| Ch14→27 | RID retention/reuse | Guard retained candidates | None |
| Ch15→27 | Statement lifecycle | No local commit/retry policy | None |
| Ch16→27 | Immutable historical descriptors | Correct schema decode | None |
| Ch17→27 | Scalar/NULL/comparison | Exact values/predicates/keys | None |
| Ch19→27 | Bound slots/count acquisition | No runtime rebinding | None |
| Ch20→27 | Bag/demand/filter/project/limit | Physical realization | N27-2 wording only |
| Ch21→27 | DML candidates/publication | Preserve owner metadata | None |
| Ch22→27 | Valid physical plan/schema | Execute selected algorithm | None |
| Ch23→27 | Active domain/borrow | Valid chunks and lifetimes | None |
| Ch24→27 | Accounting/resource failure | Account retained state | None |
| Ch25→27 | Expression demand/error/output | Preserve handoff | None |
| Ch26→27 | Protocol/lifecycle/early stop | No local override | None |
| Ch27→28 | Typed chunks/ownership | Join input handoff | No explicit Ch28 reference |
| Ch27→31 | Result consumption | External publication delegated | None |
| Ch27→32 | Local source/operator state | Scheduling mechanics delegated | None |
| Ch27→37 | Truthful order provider | Exact property matching | N27-1 wording only |
| Ch27→39 | Failures/transaction effects | Propagate canonical causes | None |

## Z. Documentation-model matrix

| Audit | Result |
|---|---|
| Project chronology | Four occurrences, one consolidated finding N27-1 |
| Current implementation narration | §27.5’s initial/future implementation wording |
| Development-owned material | Same chronology, not a separate finding |
| Verification procedures | None in Chapter 27 |
| Project-state implementation inventory | None |
| Historical dates/results/commits | None |
| Canonical analytical architecture | Otherwise appropriate |
| Terminology | N27-2 column/row “output order” ambiguity |
| ABI/source-layout overfreeze | No finding |
| Unnecessary normative mechanisms | No finding |
| Missing rationale causing independent policy | No finding after owner composition |
| Timelessness | Achievable through targeted document-only cleanup |

# 5. Documentation audits and terminology

## Complete meaningful temporal-language inventory

| Location | Wording | Classification |
|---|---|---|
| 21258 | Historical tuple `schema_version` | Compatibility/schema-history semantics |
| 21339 | “Initial target” | Project chronology — N27-1 |
| 21345 | “The initial implementation” | Project chronology — N27-1 |
| 21347 | “A future implementation” | Project chronology — N27-1 |
| 21375 | Reverse scan “deferred”; “baseline” | Durable v1 scope, delegated to Ch8 |
| 21440 | “Once the limit is satisfied” | Runtime state |
| 21456 | “foundation” | Section scope/orientation, not a progress claim |
| 21458 | “later client/result-interface contract” | Forward document navigation to Ch31 |
| 21473 | “Initial index RID batching” | Project chronology — N27-1 |
| 21474 | “Forward baseline” | Durable v1 capability scope |

No `TODO`, “not implemented,” phase-status, test-result, milestone, or historical commit material appears in the reviewed chapter.

## Terminology dictionary

| Term | Meaning/owner | Assessment |
|---|---|---|
| Sequential scan | Physical heap page/slot traversal plus visibility | Precise through Ch5/10 |
| Index scan | Selected encoded range → RID candidates → heap/MVCC | Precise through Ch8/27 |
| Candidate | Physical item requiring owner checks; not promised output | Clear |
| Tuple/version | Stored physical version | Distinct from logical row |
| Row occurrence | Logical multiplicity unit | Ch20-owned |
| RID | Physical version location, not user row identity | Ch8/14 |
| Required columns | Output and predicate dependencies | Ch22/27/37 |
| Pushed predicate | Planner-approved scan-local predicate | §27.3 |
| Residual predicate | Condition not fully proved by bounds | §17.10.3/§36.14 |
| Valid slot | Owner-valid, returnable state where required | §4.13.3 |
| Snapshot/read epoch | Different visibility/identity protections | Ch9/14/22 |
| Output schema | Ordered typed LogicalSlotId entries | Ch22/23 |
| LogicalSlotId | Semantic output occurrence identity | Ch20/22 |
| Filter order | Survivor row-occurrence sequence | §27.7 |
| Project output order | Intended output-column schema sequence | N27-2 |
| Index ordering | Advertised compatible key property | Ch37 |
| Early stop | Eliminate safely unnecessary upstream demand | §26.8 |
| Terminal | Local protocol completion, not commit/query success | Ch26 |
| Result | Internal output or external delivery, distinguished by owner | Ch26/31 |
| Small RID batch | Query-local physical batching | §27.5; N27-1 |

### Normative language and implementation freedom

The two explicit `MUST NOT` statements protect escaped page/chunk borrows. Other requirements remain normative through ordinary contract language and upstream owners; absence of repeated uppercase keywords is not a defect.

Conceptual class names and counter names do not freeze a C++ ABI. No enum numeric values, ownership-pointer type, vtable layout, worker count, or exact cursor container is imposed here.

Freedom remains for legal capacities, batching, internal loops, safe selection/materialization, continuation representation, exact counter implementation, result retention, and valid scheduling. That freedom does not authorize changing frozen physical traversal rules, demanded errors, output identity, or promised ordering.

## Complete explicit cross-reference table

| Source | Target | Purpose | Exists/owner | Quality |
|---|---|---|---|---|
| §27.1, 21258 | Chapter 16, `ResolveSchema` | Historical tuple interpretation | Yes; §16.7 | GOOD |
| §27.6, 21375 | Chapter 8 | Forward-only/reverse deferral | Yes; §8.20.1 | GOOD |
| §27.9, 21440 | §26.8 | Limit early stop | Yes; canonical owner | GOOD |
| §27.11, 21462 | Chapter 31 | Cursor/client and RETURNING | Yes; §§31.9–31.10 | GOOD |

The broad Chapter-31 navigation could optionally name its subsections, but it is not stale, circular, or wrong-owner.

# 6. Technical consistency question matrix — 320 actual questions

Status legend:

- **C** = **CONSISTENT**
- **S** = **CONSISTENT BUT SPECIALIZED**
- **F** = **FINDING**
- **N/A** = **N/A**

Each question below concerns an actual operator or its necessary owner handoff. Specialized status denotes a valid qualified contract, not a defect.

## Questions 1–16: scope and roles

| # | Question and answer | Status |
|---:|---|:---:|
| 1 | Does Chapter 27 own seven named physical operators? Yes. | C |
| 2 | Is SeqScan a source? Yes. | C |
| 3 | Is IndexScan a source? Yes. | C |
| 4 | Is Values a source? Yes. | C |
| 5 | Is Filter streaming? Yes. | C |
| 6 | Is Project streaming? Explicitly yes. | C |
| 7 | Is Limit streaming? Yes. | C |
| 8 | Is ResultSink a sink? Yes. | C |
| 9 | Does the sink role itself imply blocking? No. | C |
| 10 | Is Project a pipeline breaker? Explicitly no. | C |
| 11 | Are cursor mechanics owned here? No, Ch31. | C |
| 12 | Are join algorithms owned here? No. | C |
| 13 | Are B+ structural algorithms owned here? No, Ch8. | C |
| 14 | Is runtime SQL binding authorized? No. | C |
| 15 | Does immutable configuration contain mutable execution progress? No. | C |
| 16 | Is implementation absence an architectural defect? No. | C |

## Questions 17–32: sequential candidate domain

| # | Question and answer | Status |
|---:|---|:---:|
| 17 | Does SeqScan identify its relation through resolved identity? Yes. | C |
| 18 | Is page zero an ordinary tuple page? No, superblock. | C |
| 19 | Are published heap-data pages the physical domain? Yes. | C |
| 20 | Does FSM discovery define the full scan domain? No. | C |
| 21 | Does filesystem enumeration define membership? No. | C |
| 22 | Is page-number traversal ascending? Yes, §5.2. | C |
| 23 | Is slot traversal ascending? Yes. | C |
| 24 | Do tuple byte offsets determine row visitation? No. | C |
| 25 | Are NORMAL slots ordinary tuple candidates? Yes. | C |
| 26 | Are UNUSED coordinates dereferenced? No. | C |
| 27 | Are retained DEAD tuples returned? No. | C |
| 28 | Are reclaimed DEAD slots reusable immediately? No. | C |
| 29 | Is REDIRECT_RESERVED silently skipped? No. | S |
| 30 | Can an unpublished zero append tail become scan input? No. | C |
| 31 | Can page order create SQL ORDER BY semantics? No. | C |
| 32 | Can equal-valued visible rows collapse? No. | C |

## Questions 33–48: structural validation

| # | Question and answer | Status |
|---:|---|:---:|
| 33 | Is checksum success sufficient for ordinary use? No. | C |
| 34 | Must loaded pages receive owner-local validation? Yes. | C |
| 35 | Must embedded page identity match? Yes. | C |
| 36 | Must expected heap ownership match? Yes. | C |
| 37 | Are slot ranges checked before dereference? Yes. | C |
| 38 | Are overlapping retained tuples valid? No. | C |
| 39 | Are all retained tuple encodings validated? Yes. | C |
| 40 | Does invisibility waive L1 tuple validation? No. | C |
| 41 | Does projection pruning waive scalar-format validation? No. | C |
| 42 | Can malformed unselected BOOLEAN bytes be ignored in a retained tuple? No. | C |
| 43 | Are semantically ignored NULL payload bytes governed by their codec rules? Yes. | S |
| 44 | Is `xmax=0,cmax!=0` invisible rather than corrupt? No. | C |
| 45 | Can malformed tuple length become a short row? No. | C |
| 46 | Does ordinary loading require an exhaustive whole-database verifier? No. | S |
| 47 | Can an unaccessed corrupt page escape discovery? Yes, path-local access rules. | S |
| 48 | Can known malformed page data be returned successfully? No. | C |

## Questions 49–64: snapshot and visibility

| # | Question and answer | Status |
|---:|---|:---:|
| 49 | Is the snapshot obtained through QueryExecutionContext? Yes. | C |
| 50 | May each chunk refresh READ COMMITTED visibility independently? No. | C |
| 51 | Does one attempt use one stable statement snapshot? Yes. | C |
| 52 | Does RR retain its transaction horizon? Yes. | S |
| 53 | Does RR still use the current command boundary? Yes. | S |
| 54 | Does SeqScan allocate CommandIds? No. | C |
| 55 | Does IndexScan allocate CommandIds? No. | C |
| 56 | Is CommandId zero valid? Yes. | C |
| 57 | Is self-created current-command data ordinarily rediscovered? No. | C |
| 58 | Is a self-delete from the current command visible to that snapshot? Yes, subject to creator visibility. | S |
| 59 | Is an aborted creator visible? No. | C |
| 60 | Does an aborted deleter remove the row? No. | C |
| 61 | Does an in-progress deleter force a read tuple-lock wait? No. | C |
| 62 | Are future-self-command fields ordinary invisibility? No, error. | C |
| 63 | Can status lookup failure become FALSE? No. | C |
| 64 | Do both access paths use the same visibility owner? Yes. | C |

## Questions 65–80: heap decoding and schemas

| # | Question and answer | Status |
|---:|---|:---:|
| 65 | Does persisted schema_version choose tuple interpretation? Yes. | C |
| 66 | May scan decode using the latest schema indiscriminately? No. | C |
| 67 | Is ResolveSchema owned by Chapter 16? Yes. | C |
| 68 | Is ColumnId a storage ordinal? No. | C |
| 69 | Is ColumnId a SELECT display ordinal? No. | C |
| 70 | Can physical output order differ from table-column order? Yes, with exact mapping. | C |
| 71 | Are predicate-dependent columns retained even if not final outputs? Yes. | C |
| 72 | Must every table column be materialized? No. | C |
| 73 | Can predicate columns be decoded before output-only columns? Yes. | C |
| 74 | Are fixed-width scan results copied out of page storage? Yes. | C |
| 75 | Are heap VARCHAR bytes copied into chunk-owned storage? Yes. | C |
| 76 | Can StringRef point into released page storage? No. | C |
| 77 | Are embedded NUL bytes preserved? Yes. | C |
| 78 | Are FLOAT heap payloads normalized as index keys? No. | C |
| 79 | Are DATE/TIMESTAMP values reparsed as text during scan? No. | C |
| 80 | Can corrupt scalar payload become NULL/default? No. | C |

## Questions 81–96: index applicability and bounds

| # | Question and answer | Status |
|---:|---|:---:|
| 81 | Does the physical planner select IndexScan applicability? Yes. | C |
| 82 | Does IndexScan receive an immutable IndexDescriptor? Yes. | C |
| 83 | Are lower and upper bounds encoded inputs? Yes. | C |
| 84 | Are endpoint inclusivity flags explicit? Yes. | C |
| 85 | Is the exact key codec Chapter-8-owned? Yes. | C |
| 86 | Can runtime invent a new cast for a bound? No. | C |
| 87 | Can a later-key constraint bypass an unconstrained leading prefix? Not as a tight v1 prefix. | S |
| 88 | May an equality prefix precede one range component? Yes. | C |
| 89 | Must conditions beyond that tight range remain residual? Yes. | C |
| 90 | Are low/high suffix sentinels persistent values? No. | C |
| 91 | Are minimum/maximum RID bounds real persisted RIDs? No. | C |
| 92 | Must duplicate endpoint inclusion be exact? Yes. | C |
| 93 | Can execution silently narrow selected bounds? No. | C |
| 94 | Can cost legalize a semantically invalid range? No. | C |
| 95 | Does an index’s existence alone establish applicability? No. | C |
| 96 | Is per-chunk SQL bound reevaluation introduced? No. | C |

## Questions 97–112: key semantics and duplicates

| # | Question and answer | Status |
|---:|---|:---:|
| 97 | Are NULL-containing keys physically stored? Yes. | C |
| 98 | Does ordinary `= NULL` become an IS NULL lookup? No. | C |
| 99 | May IS NULL use a nullable index component? Yes. | C |
| 100 | Is forward NULL ordering FIRST? Yes. | C |
| 101 | Are duplicate user keys legal? Yes. | C |
| 102 | Are duplicate exact physical keys legal stored entries? No. | C |
| 103 | Can UNIQUE indexes retain multiple physical historical entries? Yes. | S |
| 104 | May an invisible first equality hit terminate a UNIQUE scan? No. | C |
| 105 | Must nonunique equality cover the full demanded duplicate range? Yes. | C |
| 106 | Can duplicates span leaves? Yes. | C |
| 107 | Does physical RID tie order create SQL tie order? No. | C |
| 108 | Do index FLOAT zeros normalize together? Yes. | C |
| 109 | Do index NaNs normalize to one comparison class? Yes. | C |
| 110 | Must SQL FLOAT comparison agree with that value order? Yes. | C |
| 111 | Does VARCHAR index encoding preserve embedded zero bytes? Yes. | C |
| 112 | Does BOOLEAN physical ordering authorize SQL BOOLEAN ORDER BY? No. | S |

## Questions 113–128: heap rechecks and retention

| # | Question and answer | Status |
|---:|---|:---:|
| 113 | Is an index hit only a candidate? Yes. | C |
| 114 | Is heap fetch required for ordinary IndexScan? Yes. | C |
| 115 | Is heap MVCC required after the hit? Yes. | C |
| 116 | Does read-epoch protection prove visibility? No. | C |
| 117 | Does visibility alone protect RID reuse? No. | C |
| 118 | Must a retained index RID remain epoch-protected? Yes. | C |
| 119 | Does the RID contain a reuse generation? No. | C |
| 120 | Must all index references be removed before retirement? Yes. | C |
| 121 | Can old epoch protection coexist with a DEAD slot? Yes. | S |
| 122 | May a reclaimed DEAD payload be dereferenced as NORMAL? No. | C |
| 123 | Can a protected old RID name a newly allocated unrelated tuple? No. | C |
| 124 | Are wrong-relation RIDs rejected? Yes. | C |
| 125 | Are slot bounds checked before access? Yes. | C |
| 126 | Must ordinary scans follow prev-version links on invisibility? No. | C |
| 127 | Does UPDATE create a new RID even with unchanged index values? Yes. | C |
| 128 | Does DELETE require immediate index-entry removal? No. | S |

## Questions 129–144: continuation and batching

| # | Question and answer | Status |
|---:|---|:---:|
| 129 | Must SeqScan resume without skipping the next candidate? Yes. | C |
| 130 | Must a full output chunk avoid replaying its final tuple? Yes. | C |
| 131 | Is an empty page terminal by itself? No. | C |
| 132 | Is an all-invisible page terminal by itself? No. | C |
| 133 | Can a finite empty HAVE_MORE step be legal? Yes. | C |
| 134 | Can equivalent empty state repeat forever? No. | C |
| 135 | Is index leaf end necessarily range end? No. | C |
| 136 | Is forward leaf handoff latch-coupled? Yes. | C |
| 137 | May a key view survive its leaf guard? No. | C |
| 138 | Must duplicate-range continuation cross output chunks correctly? Yes. | C |
| 139 | Is a candidate RID batch query-local? Yes. | C |
| 140 | May that batch become a persisted RID list? No. | C |
| 141 | Is the batching description timeless? No: N27-1. | F |
| 142 | Does short output prove source exhaustion? No. | C |
| 143 | Does full output promise another row? No. | C |
| 144 | Must final output precede terminal/no-output? Yes. | C |

## Questions 145–160: Filter

| # | Question and answer | Status |
|---:|---|:---:|
| 145 | Does TRUE retain the occurrence? Yes, once. | C |
| 146 | Does FALSE remove it? Yes. | C |
| 147 | Does UNKNOWN remove it? Yes. | C |
| 148 | Is arbitrary numeric truthiness allowed? No. | C |
| 149 | Is each required retention decision a predicate demand? Yes. | C |
| 150 | Is empty input a per-row scalar demand? No. | C |
| 151 | Can all-rejected input resolve successfully? Yes. | C |
| 152 | Does zero output mean EOS? No. | C |
| 153 | Are repeated dictionary occurrences preserved independently? Yes. | C |
| 154 | Does Filter deduplicate equal rows? No. | C |
| 155 | Does Filter preserve survivor sequence? Yes. | C |
| 156 | Does Filter generate an ordering property? No. | C |
| 157 | Are child LogicalSlotIds preserved? Yes. | C |
| 158 | May Filter return dictionary/reference vectors? Yes, safely. | C |
| 159 | May downstream retention require materialization? Yes. | C |
| 160 | May first physical predicate failure select the public error? No. | C |

## Questions 161–176: Project

| # | Question and answer | Status |
|---:|---|:---:|
| 161 | Does Project preserve row multiplicity? Yes. | C |
| 162 | Can equal projected rows collapse? No. | C |
| 163 | Does each declared output have its own semantic identity? Yes. | C |
| 164 | Can equal-valued output columns merge identities? No. | C |
| 165 | May Project reorder output columns according to its declaration? Yes. | C |
| 166 | Does numeric LogicalSlotId sorting define output columns? No. | C |
| 167 | Is §27.8’s “output order” wording precise? No: N27-2. | F |
| 168 | Does Project inherently sort rows? No. | C |
| 169 | Can a direct reference borrow input? Yes. | C |
| 170 | Must computed VARCHAR have a valid result owner? Yes. | C |
| 171 | Can scratch bytes escape without ownership? No. | C |
| 172 | Are declared ordinary Project expressions demanded? Yes. | C |
| 173 | Does RequiredSlotSet alone suppress erroring expressions? No. | C |
| 174 | Is EXISTS projection irrelevance a specialized exception? Yes. | S |
| 175 | May CSE share work while retaining identity/provenance? Only with semantic proof. | S |
| 176 | Does empty input force per-row expression execution? No. | C |

## Questions 177–192: Limit counts and demand

| # | Question and answer | Status |
|---:|---|:---:|
| 177 | Does OFFSET precede LIMIT in selection semantics? Yes. | C |
| 178 | Are count values validated nonnegative INT64? Yes. | C |
| 179 | Is count acquisition once at execution start? Yes. | C |
| 180 | Is count acquisition repeated per chunk? No. | C |
| 181 | Does OFFSET count child logical occurrences? Yes. | C |
| 182 | Do invisible heap candidates count? No. | C |
| 183 | Do residual-rejected index entries count? No. | C |
| 184 | Do FALSE/UNKNOWN Filter rows count downstream? No. | C |
| 185 | Does LIMIT zero emit rows? No. | C |
| 186 | Does positive OFFSET force row fetching with LIMIT zero? No required relational demand. | S |
| 187 | Does LIMIT zero suppress mandatory binding errors? No. | S |
| 188 | Does LIMIT zero waive count-domain validation? No. | C |
| 189 | Must every child runtime object be allocated for LIMIT zero? No such mechanism requirement. | S |
| 190 | Does a large OFFSET inherently error? No. | C |
| 191 | Does a large LIMIT inherently error? No. | C |
| 192 | Is finite-width `OFFSET+LIMIT` semantically required? No. | C |

## Questions 193–208: Limit boundaries and composition

| # | Question and answer | Status |
|---:|---|:---:|
| 193 | May count state silently wrap? No. | C |
| 194 | Is a particular counter type required? No. | C |
| 195 | Can OFFSET end inside a chunk? Yes. | C |
| 196 | Must the exact post-offset suffix be selected? Yes. | C |
| 197 | Can LIMIT end inside that suffix? Yes. | C |
| 198 | Can the unnecessary remainder be abandoned? Yes, under early stop. | C |
| 199 | Does empty input increment occurrence counters? No. | C |
| 200 | Can slicing replace cell copies? Yes, with safe lifetime. | C |
| 201 | Does Limit preserve child schema? Yes. | C |
| 202 | Does Limit create SQL order? No. | C |
| 203 | Does ordered input require exact ordered slicing? Yes. | C |
| 204 | May unordered plans select different allowed subbags? Yes. | S |
| 205 | May satisfied Limit fetch solely to observe EOS? No. | C |
| 206 | May undemanded upstream scalar errors surface? No. | C |
| 207 | Can early stop cancel required independent work? No. | C |
| 208 | Does Limit completion commit the transaction? No. | C |

## Questions 209–224: Values and ResultSink

| # | Question and answer | Status |
|---:|---|:---:|
| 209 | Is Values input already bound and typed? Yes. | C |
| 210 | Does Values parse SQL? No. | C |
| 211 | Does Values perform type resolution? No. | C |
| 212 | Are repeated listed Values rows preserved? Yes. | C |
| 213 | Does Values source order independently establish SQL order? No. | C |
| 214 | Is no-FROM input one zero-column occurrence? Yes. | S |
| 215 | May Values reference sufficiently long-lived plan payload? Yes. | C |
| 216 | Must shorter-lived payload be copied/preserved? Yes. | C |
| 217 | Does successful ResultSink accept its whole submitted domain? Yes. | C |
| 218 | Is generic partial successful sink acceptance allowed? No. | C |
| 219 | May ResultSink consume synchronously? Yes. | C |
| 220 | May it retain/materialize? Yes, with valid ownership. | C |
| 221 | Does sink acceptance itself mean external delivery? No. | C |
| 222 | Does sink acceptance itself mean query success? No. | C |
| 223 | May failed sink input be blindly replayed? No. | C |
| 224 | Does RETURNING use the ordinary streaming SELECT publication envelope? No. | S |

## Questions 225–240: vectors and ownership

| # | Question and answer | Status |
|---:|---|:---:|
| 225 | Must chunk width match its physical schema? Yes. | C |
| 226 | Must each vector TypeId match its schema entry? Yes. | C |
| 227 | Is column ordinal semantic identity? No. | C |
| 228 | Is active cardinality distinct from capacity? Yes. | C |
| 229 | May an operator evaluate inactive capacity? No. | C |
| 230 | Are legal capacities bounded by 1..65535? Yes. | C |
| 231 | Is standard 1024 a SQL row limit? No. | C |
| 232 | Does CONSTANT payload count determine row count? No. | C |
| 233 | Do repeated dictionary indices denote repeated occurrences? Yes. | C |
| 234 | Must every demanded output position be initialized? Yes. | C |
| 235 | Does an allocated owner alone prove value stability? No. | C |
| 236 | Must validity/selection/StringRef metadata remain stable too? Yes. | C |
| 237 | Does operator input release alone permit backing reset? No. | C |
| 238 | Does lifecycle resolution alone permit backing reset? No. | C |
| 239 | Can independent preservation permit earlier original-backing reuse? Yes. | C |
| 240 | Must query-owned growing buffers remain accounted? Yes. | C |

## Questions 241–256: errors and provenance

| # | Question and answer | Status |
|---:|---|:---:|
| 241 | Does D25-S1 select ordinary non-DML expression errors? Yes. | C |
| 242 | Does D21-S4 select ordinary DML candidates? Yes. | C |
| 243 | May D25 pre-ranking discard needed DML candidates? No. | C |
| 244 | Must SourceSpan survive predicate pushdown? Yes. | C |
| 245 | Must Project expression origin survive sharing? Yes. | C |
| 246 | Is worker identity an error tie-breaker? No. | C |
| 247 | Is scan/RID order an error tie-breaker? No. | C |
| 248 | Is candidate discovery automatically terminal selection? No. | C |
| 249 | May undemanded speculation create an ordinary candidate? No. | C |
| 250 | Is resource failure ranked in D25’s cause order? No. | C |
| 251 | Is cancellation universally higher priority than semantic errors? No such rule. | C |
| 252 | May lower-layer structured causes be erased? No. | C |
| 253 | Is partial failed output consumable? No. | C |
| 254 | Does later failure undo a completed internal handoff? No. | C |
| 255 | Does a later cursor failure retract completed delivery? No. | C |
| 256 | Does a returned prefix prove complete query success? No. | C |

## Questions 257–272: lifecycle and invalid states

| # | Question and answer | Status |
|---:|---|:---:|
| 257 | Must runtime state be initialized before use? Yes. | C |
| 258 | Is each submitted input accepted at most once? Yes. | C |
| 259 | Is continuation a fresh acceptance? No. | C |
| 260 | Is one unresolved acceptance permitted per local state? At most one. | C |
| 261 | Must output handoff complete before eligible new input? Yes. | C |
| 262 | Can terminal operators accept more input? No. | C |
| 263 | Can FINISHED carry consumable final rows? No. | C |
| 264 | Must stale terminal-buffer bytes be ignored as results? Yes. | C |
| 265 | Is repeated post-terminal GetData required? No; outside valid protocol. | C |
| 266 | Is source rewindability implied? No. | C |
| 267 | Can failed runtime state later succeed? No. | C |
| 268 | Must authorized retry use fresh logical state? Yes. | C |
| 269 | Can old Limit counters survive a fresh attempt? No. | C |
| 270 | Can old expression candidates survive a fresh attempt? No. | C |
| 271 | Can wrong-schema state become a user TypeError after validation? No. | C |
| 272 | Must unsafe access be prevented before malformed state is used? Yes. | C |

## Questions 273–288: properties, substitution, and fusion

| # | Question and answer | Status |
|---:|---|:---:|
| 273 | Does SeqScan advertise no order? Yes. | C |
| 274 | Does forward IndexScan require compatible key/type/collation? Yes. | C |
| 275 | Does forward traversal satisfy DESC automatically? No. | C |
| 276 | Does Filter preserve supplied ordering? Yes. | C |
| 277 | Does Project retain order only for surviving unchanged keys? Yes. | S |
| 278 | Does Limit preserve supplied ordering? Yes. | C |
| 279 | Is streaming a third Chapter-37 property? No. | C |
| 280 | Can runtime discover new SQL predicate rewrites? No. | C |
| 281 | Must scan pushdown preserve demanded errors? Yes. | C |
| 282 | Can an exact safe bound eliminate redundant predicate evaluation? Yes, with proof. | S |
| 283 | Can a broader bound omit its residual? No. | C |
| 284 | May Filter and Project freely swap? No. | C |
| 285 | May Limit move below Project without demand proof? No. | C |
| 286 | May Limit move below Filter merely for speed? No. | C |
| 287 | Must fused/unfused realizations preserve semantic origins? Yes. | C |
| 288 | May fused Limit count physical index candidates? No. | C |

## Questions 289–304: concurrency, storage, and publication

| # | Question and answer | Status |
|---:|---|:---:|
| 289 | Are worker cursors execution-local? Yes. | C |
| 290 | Must parallel partitions avoid duplicate/missing work? Yes. | C |
| 291 | Does one worker FINISHED complete all required scan work? No. | C |
| 292 | Must read tasks share immutable snapshot/command semantics? Yes. | C |
| 293 | Does arbitrary interleaving establish SQL order? No. | C |
| 294 | Can native reverse index traversal be inferred from prev links? No. | C |
| 295 | Can concurrent DROP immediately unlink a referenced object? No. | C |
| 296 | Are retained descriptors immutable? Yes. | C |
| 297 | Can an unqualified catalog-cache hit replace snapshot visibility? No. | C |
| 298 | Does SELECT acquire DML tuple locks merely for visibility? No. | C |
| 299 | Can executor cleanup release transaction locks independently? No. | C |
| 300 | Does local sink/source completion imply COMMIT? No. | C |
| 301 | Can scan cursors enter WAL semantic identity? No. | C |
| 302 | Can Limit counters enter catalog/recovery identity? No. | C |
| 303 | Can physical access paths encounter different corrupt structures? Yes. | S |
| 304 | Can physical access paths have different resource feasibility? Yes. | S |

## Questions 305–320: completeness and documentation

| # | Question and answer | Status |
|---:|---|:---:|
| 305 | Must both successful access paths satisfy the logical row bag? Yes. | C |
| 306 | Must both preserve exact NULL/value semantics? Yes. | C |
| 307 | Must both preserve required order rather than incidental order? Yes. | C |
| 308 | Must eligible semantic error selection remain owner-equivalent? Yes. | C |
| 309 | Can successful substitution change transaction semantics? No. | C |
| 310 | Is index-error fallback to SeqScan specified here? No. | C |
| 311 | Can missing descriptors trigger runtime name rebinding? No. | C |
| 312 | Does full tuple validation conflict with selected-column materialization? No. | C |
| 313 | Does page checksum validation disappear under LIMIT demand? No, for an accessed page. | C |
| 314 | Does LIMIT require reading an otherwise unnecessary next page? No. | C |
| 315 | Are explicit cross-references live and correctly owned? Yes. | C |
| 316 | Does Chapter 27 contain test recipes? No. | C |
| 317 | Does Chapter 27 contain historical test results? No. | C |
| 318 | Does Chapter 27 prescribe enum ABI values? No. | C |
| 319 | Is correctness-relevant implementer policy invention required? None identified. | C |
| 320 | Can the chapter be timeless after targeted wording cleanup? Yes. | C |

# 7. Verification coverage and follow-up gaps

These are **methodology classifications**, not implementation/test-pass claims. Existing COMPLETE labels were not treated as proof without examining their supporting procedure.

| Mechanism | Classification | Reusable methodology/oracle | Remaining Chapter-27 integration |
|---|---|---|---|
| SeqScan candidate traversal | COMPLETE | Heap physical-scan oracle; V22-F | Reference directly |
| SeqScan MVCC/commands | COMPLETE | Ch10 visibility/error matrix; V22-E/F | Reference directly |
| SeqScan page/slot/output continuation | PARTIAL | Physical scan oracle + V26-A/C/D | Explicit combined page/slot/capacity trace |
| Invisible-only/empty pages | PARTIAL | V26 finite-progress oracle | Instantiate actual page candidate transitions |
| Final output before FINISHED | COMPLETE | V26-A status/output oracle | Apply to all three sources |
| Heap structural corruption | COMPLETE | Heap/tuple byte oracle | Reference mandatory full-page validation |
| Projection/visibility versus corruption timing | PARTIAL | Full-tuple validator + V22-F | Direct unselected/invisible malformed-attribute composition |
| Index range endpoints | COMPLETE | Routing/duplicate range; Access Path Tests | Reference directly |
| Duplicate user/physical keys | COMPLETE | Independent physical-key ordering/range oracle | Reference directly |
| Heap visibility recheck | COMPLETE | Ch10 candidate matrix; V22-F | Reference directly |
| Stale/dead index candidates | PARTIAL | Ch10 + Ch14 state/reuse oracle | Captured batch → DEAD/reclaimed state handoff |
| RID reuse protection | COMPLETE | Complete Ch14 missing-barrier matrix | Reference directly |
| RID batch boundary/order | PARTIAL | B+ cursor oracle + V26-A/Q | Batch/leaf/output boundaries in one deterministic trace |
| Residual predicates | COMPLETE | V22-F; Access Path Tests; V25 | Preserve exact demand qualification |
| Index ordering | COMPLETE | V22-C; Physical Property Tests | Reference exact prefix/direction/NULL rules |
| Access-path successful equivalence | COMPLETE | V22-F/G bag/schema/comparator model | Qualify unordered allowed results |
| Access-path failure-surface qualification | PARTIAL | V24/V26-Q + storage error owners | Distinguish semantic equivalence from different accessed faults |
| Filter truth/multiplicity | COMPLETE | V20-5 independent predicate oracle | Reference directly |
| Filter empty input/output | COMPLETE | V25 demand + V26-D | Reference directly |
| Filter selection/borrow | COMPLETE | V23 + V26-M owner graph | Reference directly |
| Filter D25/D21 errors | COMPLETE | V25-J/K | Reference directly |
| Project schema/duplicate slots | COMPLETE | V22-B; V23-A | Avoid numeric-ID-order wording |
| Project varlen/borrowing | COMPLETE | V25-L/O; V26-M | Reference directly |
| Project required-output demand | COMPLETE | V20-5/15/16; V25 | Ordinary versus EXISTS distinction |
| Limit count-domain boundaries | COMPLETE | V20-11 mathematical oracle | Reference directly |
| Limit chunk crossings | PARTIAL | V20-11 + V26-C/N | Explicit offset/limit inside same chunk and borrowed slice |
| LIMIT zero with nonzero OFFSET | PARTIAL | V20-11; V26-N | Direct zero-fetch negative with count acquisition kept separate |
| Large INT64/no sum overflow | COMPLETE | V20-11; V22 exact-K methodology | Reference directly |
| Long-running counter safety | PARTIAL | Mathematical Limit oracle | Symbolic counts beyond naïve cumulative counter range |
| Qualifying-row count | PARTIAL | V22-F/G + V20-11 | Mixed invisible/residual/UNKNOWN candidates before Limit |
| Early stop/undemanded error | COMPLETE | V26-N independent demand model | Reference directly |
| Values occurrence/no rebinding | COMPLETE | V20-4; Scan and Unary Tests | Reference directly |
| Values plan-owned payload lifetime | PARTIAL | V23/V26 owner graphs | Explicit plan lifetime versus retained result |
| ResultSink complete acceptance | COMPLETE | V26-E | Reference directly |
| ResultSink retention/publication | COMPLETE | V26-J/M; V21 envelope | SELECT versus RETURNING |
| Retry/cancellation | COMPLETE | V26-K/L | Apply state-category mapping |
| Invalid runtime state | COMPLETE | V23-M; V25-N; V26-P | Apply actual operator adapters |
| Generic chunk/vector/worker determinism | COMPLETE | V25-P; V26-Q | Reference directly |

No listed family is wholly MISSING or blocked by a semantic question. Several Chapter-27-specific combinations remain PARTIAL; therefore this review does **not** declare Verification synchronized or Chapter 27 closed.

The future synchronization should create an explicit Chapter-27 coverage inventory and exact reuse map, rather than copying all upstream procedures.

# 8. Semantic questions and upstream regression assessment

**FROZEN CHAPTER-27 ARCHITECTURE SEMANTIC QUESTIONS: NONE identified.**

The high-priority candidates resolve as follows:

| Concern | Resolution |
|---|---|
| LIMIT 0 with nonzero OFFSET | No relational row demand merely to skip OFFSET; count acquisition/binding remain separate |
| Child initialization at LIMIT 0 | No prescribed allocation/initialization strategy; no license for observable undemanded scalar work |
| Invisible/unused-column corruption | Mandatory complete page-local retained-tuple validation precedes ordinary use |
| Projection pushdown versus corruption | Materialization pruning cannot weaken L1 validation |
| Stale/dead RID | Existing state/reuse protocol distinguishes nonreturnable garbage from unsafe or malformed targets |
| Reused RID alias | Forbidden by index cleanup, epoch, claim, and predecessor barriers |
| NULL index lookup | Stored NULL and IS NULL lookup are distinct from ordinary `= NULL` |
| Duplicate-key ordering | Physical RID tie order is not added SQL ordering |
| Residual omission | Only exact semantic proof; broader bounds retain residual |
| Project unused outputs | Ordinary declaration remains demanded; specialized/pruning proof required |
| Access-path equivalence | Successful logical semantics preserved; path-local resource/storage failures may differ |

### Frozen upstream regression

| Owner | Result |
|---|---|
| Ch5 heap/RID/tuple format | Unchanged |
| Ch8 key/comparator/entry/cursor | Unchanged |
| Ch9–10 snapshot/MVCC | Unchanged |
| Ch14 reuse/retention | Unchanged |
| Ch15 statement/DML lifecycle | Unchanged |
| Ch16 immutable descriptors/history | Unchanged |
| Ch17 scalar/NULL/comparison | Unchanged |
| Ch19 binding/provenance/count acquisition | Unchanged |
| Ch20 bag/order/demand/Filter/Project/Limit | Unchanged |
| Ch21 D21-S4/D21-S5/publication | Unchanged |
| Ch22 applicability/schema/immutable plan | Unchanged |
| Ch23 active domain/representation/borrowing | Unchanged |
| Ch24 resources/accounting | Unchanged |
| Ch25 expression demand/error/output validity | Unchanged |
| Ch26 protocol/lifecycle/early stop/sink/error transport | Unchanged |

No upstream reopening is required. The known implementation defects mentioned in the request were not treated as Architecture defects.

## 9. Final disposition

- **Chapter-27 Architecture:** targeted document-only fixes recommended.
- **Semantic decision package:** not required.
- **Correctness-relevant implementer invention:** none identified after owner composition.
- **Chapter-27 Verification:** substantial reusable coverage; synchronization still required.
- **Chapter 27:** not fully reviewed and closed.
- **Recommended next task:** **TARGETED CHAPTER-27 DOCUMENT-ONLY CLEANUP**, limited to N27-1 and N27-2.
- After clean Architecture: **CHAPTER-27 VERIFICATION SYNCHRONIZATION**.
- Chapter 28’s eventual review scope begins at **# 28. Join Execution**, line 21482; its review was **not started**.
- **Phase 2 remains NOT STARTED / NOT AUTHORIZED.**
