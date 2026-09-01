# Chapter 22 verdict

**CHAPTER 22 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 22 is largely coherent, but one blocking contradiction exists in its physical operator family:

- Frozen `LogicalLimit` semantics forbid an `OFFSET + LIMIT` overflow dependency.
- `PhysicalTopN` explicitly makes that same overflow a planning/execution error.
- Therefore equivalent physical plans can produce success versus error for the same valid logical plan.

Finding totals:

| Severity | Count |
|---|---:|
| BLOCKING | 1 |
| MAJOR | 1 |
| MINOR | 2 |
| EDITORIAL | 0 |

Frozen Chapter-22 semantic questions: **1**.

## Repository state

Initial and final state were identical:

- HEAD: `70f4f7c6cad6e38336776a844d4931831af0c1c5`
- Index: clean
- Tracked working tree: clean
- Pre-existing untracked path:
  `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 22/`

Audit-created changes: **NONE**.

`git diff --check`: passed with no output.

The untracked historical-review directory was not read, modified, moved, or staged.

## Chapter boundaries

- Exact title: `# 22. Physical Plan and Runtime Operator Model`
- Start: [ARCHITECTURE.md:18807](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18807)
- End: line 19098, immediately before Chapter 23
- Chapter 23: `# 23. Vectorized Data and String Representation`
- Chapter-23 heading: [ARCHITECTURE.md:19099](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19099)

### Heading inventory

| Section | Exact heading | Responsibility | Status |
|---|---|---|---|
| 22 | Physical Plan and Runtime Operator Model | Physical-plan/runtime foundation | Architecture-appropriate |
| 22.1 | Execution architecture | Production execution shape and exclusions | Document-role issue |
| 22.2 | Immutable physical plan versus execution state | Plan/runtime-state lifetime split | Document-role issue |
| 22.3 | Physical operator contract | Minimum physical-node metadata | Document-role issue |
| 22.4 | Initial physical operator family | Physical algorithm/control vocabulary | Document-role issue |
| 22.4.1 | Physical implementation availability | Capability gating | Document-role issue |
| 22.5 | Query execution context | Per-execution transaction/resource context | Architecture-appropriate |
| 22.6 | Runtime ownership | Immutable configuration versus mutable work | Architecture-appropriate |
| 22.7 | Physical properties | Physical metadata and ordering summary | **Major property-owner drift** |
| 22.8 | Foundation invariants | Chapter-wide invariants | Document-role issue |

## Context consulted

Architecture context was limited to relevant owner contracts:

- Front matter and documentation authority.
- Chapters 7–10: page/index ownership, snapshots, CommandId, MVCC.
- Chapter 14 §§14.5–14.7: read epochs and RID reuse.
- Chapter 16: immutable descriptors, stable IDs, SchemaVer.
- Chapter 17: scalar, NULL, FLOAT64, VARCHAR, evaluation order.
- Chapter 19: BindingId, aggregate ordinal, SourceSpan/cast provenance.
- Chapter 20 in full where needed, particularly §§20.1–20.20.
- Chapter 21 §§21.8–21.17.
- Chapter 23 §§23.1–23.14.
- Chapter 24 §§24.1–24.11.
- Chapter 25 §§25.1–25.8.
- Chapter 26 §§26.1–26.10.
- Chapter 27 §§27.1–27.12.
- Chapter 28 §§28.1–28.13.
- Chapter 29 aggregate/DISTINCT execution contracts.
- Chapter 30 §§30.1–30.8.
- Chapter 31 §§31.1–31.13.
- Chapter 36 base-access legality.
- Chapter 37 physical properties and subquery planning.
- Chapter 38 cost/search and final validation.
- §§39.2–39.4, §40, and §41.5.

Verification sections consulted:

- `Execution Verification` and `Execution Testing Strategy`, [VERIFICATION.md:14787](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:14787)
- `Physical-Plan Validator Tests`, [VERIFICATION.md:14853](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:14853)
- Pipeline/resource, scan, join, aggregate, sort, DML, control-operator, and profiling tests
- Physical-property, memory/spill, memo, cost, optimizer-determinism, and final-validation tests

# Complete findings

## B22-1 — `PhysicalTopN` introduces a forbidden overflow error

- Severity: **BLOCKING**
- Type: **LIMIT PHYSICALIZATION**
- Scope: Cross-section, Chapters 20/22/30/38
- Canonical logical owner: §20.12
- Physical operator owner: §30.7
- Optimization owner: §§38.15–38.16

Evidence:

- `LogicalLimit` applies OFFSET first and LIMIT second using subtraction/minimum and explicitly states that the contract never requires `o + l`, introduces no overflow path, and that these boundary cases are not errors. [ARCHITECTURE.md:16775](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16775)
- `PhysicalTopN`, which Chapter 22 includes as a baseline physical implementation, defines `K = N + OFFSET` and states that overflow is a planning/execution error. [ARCHITECTURE.md:21443](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21443)
- §38.16 also computes `required_rows = LIMIT + OFFSET` with checked arithmetic but says the objective affects cost/search only and never changes result semantics. [ARCHITECTURE.md:25798](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25798)

Valid input:

```sql
SELECT ...
ORDER BY ...
LIMIT 9223372036854775807
OFFSET 1;
```

Competing physical outcomes:

1. `PhysicalSort -> PhysicalLimit` succeeds and applies the frozen logical cardinality rule.
2. `PhysicalTopN` reports an overflow error solely because that algorithm was chosen.

Observable consequence:

- success versus planning/execution error;
- physical plan selection changes SQL correctness;
- cost or capability state can select the user-visible outcome.

Smallest required decision:

- Freeze that `N + OFFSET` overflow is **not a public query error**.
- On overflow, either:
  - make Top-N/finite-first-K optimization ineligible and use another conforming plan; or
  - define a semantics-preserving saturated/domain-aware internal bound.
- Align §§30.7, 30.8, 38.16, and Verification with that decision.

The frozen upstream rule strongly favors the first interpretation, but the contradictory normative text requires an explicit Chapter-22 semantic decision package before editing.

## M22-1 — Chapter 22’s property summary conflicts with Chapter 37

- Severity: **MAJOR**
- Type: **PROPERTY SEMANTICS**
- Scope: Cross-section, §§22.7 and 37.1–37.4
- Canonical owner: Chapter 37

Evidence:

§22.7 lists as physical properties:

```text
output ordering
candidate/unique keys
partitioning
rewindability
blocking/streaming nature
estimated cardinality
estimated memory
spill capability
```

and permits an ordering descriptor to identify a:

```text
LogicalSlotId / resolved expression
```

See [ARCHITECTURE.md:19040](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19040).

Chapter 37 instead freezes:

- the v1 property system as `OrderingProperty` plus `RequiredSlotSet`;
- partitioning, rewindability, and materialization as reserved extension points;
- `required_rows` as an objective, not a physical property;
- ordering-property reasoning as strictly `LogicalSlotId`-based;
- computed expressions as hidden `LogicalSlotId` values before comparison.

See [ARCHITECTURE.md:24821](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24821).

Consequences:

- estimates and execution traits can be mistaken for exact required/provided properties;
- expression equivalence could be substituted for slot identity;
- duplicate outputs, self-joins, or equivalent computed expressions could falsely satisfy ordering;
- an implementation could build a larger property lattice than the frozen v1 owner permits.

Chapter 37 makes the intended semantics determinate, so this does not require a new semantic decision. It requires a targeted document correction:

- make Chapter 37 explicitly canonical;
- classify exact property, search requirement/objective, estimate, execution trait, and future extension separately;
- make ordering descriptors slot-only.

## N22-1 — Project chronology and current-implementation wording

- Severity: **MINOR**
- Type: **TEMPORALITY**
- Scope: §§22.2–22.4.1, 22.7–22.8

Project-time/current-state phrases include:

- “may later be cached or reused” — line 18860
- “estimated row/cost placeholders” — line 18876
- “Initial physical operator family” — line 18894
- “currently eligible” — line 18945
- “implementation is available and validated” — line 18949
- “later/conditional” — line 18969
- “current single-threaded implementation” — line 19082
- “from day one” — line 19089

These should be rewritten as timeless v1 rules and capability conditions. Runtime phrases such as “current command boundary” are legitimate and are not findings.

## N22-2 — Verification-owned reference-executor prose

- Severity: **MINOR**
- Type: **DOCUMENT OWNERSHIP**
- Scope: §22.1, line 18843

The optional row-at-a-time executor is described specifically as existing for “differential/correctness testing.” The production/non-production boundary is architectural, but the test motivation belongs in Verification.

Smallest correction:

- retain only the production conformance boundary in Architecture;
- keep differential/reference-executor procedure in Verification.

# Section-by-section review matrix

Legend: `OK` clean, `DOC` document-only issue, `PROP` M22-1, `B` B22-1 downstream composition.

| Section | Role | Timelessness | Owner | Depth | Handoff/schema/slots | Bag/order | Properties/exactness | Errors/determinism | Xrefs | Status |
|---|---|---|---|---|---|---|---|---|---|---|
| 22.1 | Production execution shape | DOC | Mostly Architecture | Adequate | Resolved input | Neutral | N/A | Vectorization nonsemantic | Implicit Ch23–26 | DOC |
| 22.2 | Plan/state split | DOC | Chapter 22 | Adequate | Immutable plan | Neutral | Plan metadata only | Per-execution state isolated | None | DOC |
| 22.3 | Operator contract | DOC (“placeholders”) | Chapter 22 | Adequate with downstream owners | LogicalSlotIds retained | No new order | Delegates optimizer details | Validator downstream | “optimizer chapters” vague | DOC |
| 22.4 | Operator vocabulary | DOC | Chapter 22 | Adequate | Algorithms do not rebind | Delegated | Capability-gated | Downstream semantics | §20.14.7; Ch28–31 | DOC |
| 22.4.1 | Capability gating | DOC | Ch22/37/38 | Semantically adequate | Legal algorithms only | Neutral | Capability, not proof | Unavailable plan rejected | §37 implied | DOC |
| 22.5 | Execution context | OK | Ch22 with Ch9–15 | Strong | Txn/snapshot/attempt context | Neutral | N/A | Correct retry/runtime identity | Good broad refs | OK |
| 22.6 | Runtime ownership | OK | Ch22–26 | Strong | Config/state separation | Neutral | Properties immutable | No persistence of pointers | Implicit Ch23–26 | OK |
| 22.7 | Property summary | DOC | **Drifts from Ch37** | Insufficient taxonomy | Expression/slot ambiguity | Ordering examples correct | **PROP** | Incidental order rejected | Missing Ch37 citation | MAJOR |
| 22.8 | Foundation invariants | DOC (“day one”) | Chapter 22 | Adequate | Resolved slots/IDs | No inferred order | Explicit metadata | State separation deterministic | Good summaries | DOC |
| Ch30 Top-N composition | Physical ORDER/LIMIT | OK wording locally | Ch30 under Ch20 | Precise but contradictory | Schema preserved | Ordered result | Cost alternative | **Overflow changes outcome** | Ch20 handoff violated | **B** |

# Canonical owner map

| Mechanism | Canonical owner |
|---|---|
| Logical relational meaning, bags, demanded evaluation | Chapter 20 |
| DDL/DML operation semantics and D21-S1–S6 | Chapter 21 |
| Physical-plan/runtime foundation | Chapter 22 |
| DataChunk, vectors, selection, string ownership | Chapter 23 |
| Query memory, temporary rows, spill | Chapter 24 |
| Vector expression execution | Chapter 25 |
| Pipelines, source/streaming/sink, early stop | Chapter 26 |
| SeqScan, IndexScan, Filter, Project, Limit, Values | Chapter 27 |
| Join execution | Chapter 28 |
| Aggregation and DISTINCT | Chapter 29 |
| Sort and Top-N | Chapter 30 |
| DML/DDL/VACUUM/result execution | Chapter 31 |
| Parallel scheduling | Chapter 32 |
| Statistics and exact-proof provenance | Chapters 34–35 |
| Cost and base access legality | Chapter 36 |
| Physical properties and join enumeration | Chapter 37 |
| Memo/search/final optimizer validation | Chapter 38 |
| Error categories and transaction consequences | Chapter 39 |
| EXPLAIN presentation and observability | Chapter 40 |
| Verification methodology requirements | Chapter 41 |

No mechanism requires Chapter 22 to duplicate downstream execution algorithms.

# Handoff assessment

## Chapter 20 → 22

Chapter 22 consumes immutable, validated logical semantics through physical search:

- statement-wide `LogicalSlotId`;
- typed executable expressions;
- resolved nullability and lineage;
- aggregate ordinals;
- SourceSpan/provenance;
- bag multiplicity;
- semantic order requirements;
- hidden DML RID/system slots;
- subquery occurrence identities and lazy modes.

It does not rebind names or redefine logical operators.

Status: **clean except for the Top-N realization contradiction**.

## Chapter 21 → 22

Physical DML/DDL roles consume:

- finalized target identity and deduplication;
- statement attempt, CommandId, snapshot and retry boundary;
- row-image rules;
- D21-S1 NOT NULL;
- D21-S2 backing-index protection;
- D21-S3 current-owner CREATE INDEX build;
- D21-S4 error precedence;
- D21-S5 unordered RETURNING;
- D21-S6 name reservation.

Chapter 31 remains the detailed execution owner. No second DML/DDL model appears in Chapter 22.

Status: **clean**.

## Chapter 22 → 23

Chapter 22 requires vectorized/chunk-at-a-time execution and names reusable `DataChunk` state. Chapter 23 immediately defines `DataChunk`, vector capacity, physical positions, selection, validity, string references, and ownership.

There is no explicit Chapter-23 reference in Chapter 22, but the immediate handoff is unambiguous.

Status: **ready; optional navigation improvement only**.

# Physical-plan and output model

## Physical plan

A physical plan is:

- immutable after publication;
- composed of resolved physical operators;
- free of transaction-, snapshot-, cursor-, cancellation-, memory-, and worker-specific mutable state;
- validated before pipeline construction and side effects;
- paired with per-execution global/local state.

Plan shape is not SQL semantics. A conforming algorithm must preserve the logical and statement contract.

## Physical output schema and slot mapping

No new semantic physical-output identity is needed.

The model is:

```text
ordered physical output position
    ↔ one output-schema entry
    ↔ one LogicalSlotId
    ↔ resolved type/nullability/lineage metadata
```

`LogicalSlotId` remains semantic identity; vector position is a transient physical location.

| Case | Required mapping |
|---|---|
| `SELECT a,a` | Two physical positions, two distinct Project `LogicalSlotId`s |
| Same expression twice | Distinct logical outputs; computation may be shared behind a two-output mapping |
| Self-join | Distinct BindingIds and slots for the two relation occurrences |
| Filter/Sort/Limit/Distinct | Preserve child slots |
| Project | Fresh slot per declared output position |
| Join | Left slots then right slots |
| Aggregate | Fresh group/aggregate output slots; aggregate ordinal retained separately |
| Derived table | Explicit child-slot to fresh outer-slot map |
| Hidden DML RID | Preserved while required; never exposed as user output |

Pointer identity, allocation order, output ordinal, `BindingId`, `ColumnId`, heap `SlotId`, and aggregate ordinal do not replace `LogicalSlotId`.

# Physical operator matrix

| Operator | Role | Canonical details | Order | Correctness notes |
|---|---|---|---|---|
| PhysicalSeqScan | Source | Ch27 | None | Heap MVCC; one logical visible row per owner |
| PhysicalIndexScan | Source | Ch27/36 | Conditional ASC/NULLS FIRST | Entry is candidate; heap MVCC required |
| PhysicalValues | Source | Ch27 | No semantic order unless upstream establishes it | Typed/bound input only |
| PhysicalFilter | Streaming | Ch27 | Preserves provided input order | TRUE retains; FALSE/UNKNOWN reject |
| PhysicalProject | Streaming | Ch27 | Preserves only surviving compatible order | Fresh logical outputs remain distinct |
| PhysicalNestedLoopJoin | Join | Ch28 | No automatic guarantee | INNER/LEFT/CROSS baseline |
| PhysicalHashJoin | Join | Ch28 | None | INNER/LEFT; multiplicity/residual exact |
| PhysicalIndexNestedLoopJoin | Join | Ch28 | Only if separately guaranteed | Heap MVCC and residual required |
| PhysicalMergeJoin | Conditional join | Ch28 | Capability-specific | Exact comparator/order prerequisites |
| PhysicalHashAggregate | Aggregate | Ch29 | None | Exact grouping and aggregate semantics |
| PhysicalSortAggregate | Conditional aggregate | Ch29 | Capability-specific | Same groups/values/errors |
| PhysicalDistinct | Duplicate elimination | Ch29 | Hash baseline none | Grouping equivalence |
| PhysicalSort | Blocking order enforcer | Ch30 | Exact requested order | Unstable ties permitted |
| PhysicalTopN | Ordered bounded selection | Ch30 | Exact requested order | **B22-1 overflow contradiction** |
| PhysicalLimit | Streaming selector | Ch27 | Preserves child semantic order | OFFSET then LIMIT |
| Scalar/Exists/In side plans | Lazy side roles | §§20.14, 37.17 | Child-specific | Once per occurrence/attempt |
| Insert/Update/Delete | DML control/sinks | Ch31 | RETURNING unordered | Ch21 semantics remain canonical |
| Create/Index/Drop | DDL control | Ch31 | N/A | Invoke Ch21 protocols |
| Vacuum/Analyze | Maintenance control | Ch31 | N/A | Invoke Ch14/34 |
| Explain | Observation | Ch40 | N/A | Presentation not semantic |
| ResultSink | Result boundary | Ch31 | Preserves required result order | Owns returned value lifetime |

# Access, scan, and index matrices

## Access-path legality

| Path | Preconditions | Residual | MVCC | Order |
|---|---|---|---|---|
| SeqScan | Resolved table/descriptor | Pushed/filter predicates as assigned | Required | None |
| Point IndexScan | Exact usable key prefix/bounds | Anything not proven by bounds | Required | Compatible forward order only |
| Range IndexScan | Leftmost equality prefix plus one range | Remaining predicates | Required | Compatible ASC/NULLS FIRST |
| Unique lookup | Usable unique index | Visibility/current-owner checks remain | Required | No single-row proof from physical entry count |
| Index-only scan | No v1 visibility mechanism | N/A | Cannot be bypassed | Not a v1 path |

## Index semantics

- Stale/obsolete/aborted index entries are candidates only.
- Candidate RIDs remain read-epoch protected.
- Heap fetch and exact MVCC visibility are mandatory.
- Residual predicates remain unless exact bounds prove them.
- SQL `= NULL` remains UNKNOWN and is not converted to index NULL equality.
- `IS NULL` may use the exact NULL key representation.
- FLOAT64 NaN and ±0 follow Chapters 8/17.
- VARCHAR comparison is unsigned binary byte order.
- Forward scan cannot claim DESC.
- Equal user keys have no SQL-visible RID tie order.

# Physical property and order matrices

## Canonical taxonomy

| Concept | Actual category | Exact? | Owner |
|---|---|---:|---|
| OrderingProperty | Required/provided physical property | Yes | Chapter 37 |
| RequiredSlotSet | Search/output requirement | Yes | Chapter 37 |
| RequiredRowsObjective | Cost/search objective | No semantic proof | Chapter 38 |
| Candidate/unique key | Logical exact fact when trusted | Potentially exact | Chapter 20 |
| Estimated cardinality | Estimate | No | Chapters 35–38 |
| Estimated memory/spill | Estimate | No | Chapters 36–38 |
| Blocking/streaming | Execution trait | Exact for selected operator | Chapters 26–31 |
| Spill capability | Runtime capability/trait | Exact declaration | Chapters 24–31 |
| Partitioning | Reserved future property extension | Not v1 property | Chapter 37 |
| Rewindability/materialization | Reserved extension / algorithm requirement | Not general v1 property | Chapters 24, 28, 37 |

## Ordering satisfaction

An available ordering satisfies a requirement only by exact prefix match on:

- `LogicalSlotId`;
- direction;
- NULL order;
- collation.

Computed expressions require a hidden `LogicalSlotId` before comparison. Expression text or structural equivalence is not enough.

## Order-preservation matrix

| Operator | Classification |
|---|---|
| SeqScan | No semantic/provided SQL order |
| Forward IndexScan | Establishes compatible ASC/NULLS FIRST property |
| Filter | Preserves input order |
| Project | Preserves retained unchanged ordering keys only |
| Limit | Preserves child semantic order |
| HashJoin | Destroys/does not provide order |
| HashAggregate/HashDistinct | Does not provide order |
| Sort | Establishes exact order |
| Top-N | Establishes exact order if semantically legal |
| Nested/merge/ordered aggregate | Only capability-specific explicit guarantees |
| Materialization | Preserves order only when its contract explicitly does so |

Physical PageNo, SlotId, RID, vector lane, hash bucket, worker, spill partition, or pointer order is never SQL order.

# Unary, join, aggregate, DISTINCT, sort, and subquery matrices

## Filter/Project

| Property | Filter | Project |
|---|---|---|
| Multiplicity | At most one output per input occurrence | Exactly one output per producing input |
| Duplicate collapse | Never | Never |
| Slots | Pass-through | Fresh per output position |
| Demand | Predicate demanded for relevant rows | Declared expressions demanded unless specialized rule |
| NULL predicate | UNKNOWN rejects | Ordinary scalar NULL preserved |
| Provenance | Predicate origin preserved | Expression/output origins preserved |
| Order | Preserves input | Preserves compatible surviving keys |

## Join

| Algorithm | INNER | LEFT | CROSS | MVCC/index | Order |
|---|---:|---:|---:|---|---|
| Nested loop | Yes | Yes | Yes | Materialized child already semantically valid | None implied |
| Hash | Yes | Yes | No separate CROSS role | Full key/residual | None |
| Index nested loop | Yes | Yes | Not needed | Heap MVCC required | Only explicit capability |
| Merge | Conditional | Conditional | No | Exact comparator/residual | Only explicit capability |

All algorithms preserve:

- candidate-pair completeness;
- duplicate multiplicity;
- FALSE/UNKNOWN nonmatch;
- exactly one LEFT null extension when no TRUE match;
- left-then-right output schema;
- right-side logical nullability;
- slot and lineage mapping.

Build/probe choice is nonsemantic except the frozen LEFT orientation.

## Aggregate and DISTINCT

| Mechanism | Equality | Empty input | Order | Error rule |
|---|---|---|---|---|
| Global aggregate | N/A group key | One group | At most one row | Exact aggregate finalization |
| Grouped aggregate | Grouping equivalence | No groups | Unspecified unless provided | Lowest aggregate ordinal where frozen |
| Hash aggregate | Same | Same | None | Worker/spill/hash invariant |
| Sort aggregate | Same | Same | Capability order only | Same values/errors |
| Hash DISTINCT | Grouping equivalence | Empty | None | No hidden error/order |
| Streaming DISTINCT | Same | Empty | Requires compatible input order | Same duplicate classes |

Grouping/DISTINCT equivalence remains:

- NULLs equivalent;
- NaNs canonical-equivalent;
- `-0.0 == +0.0`;
- VARCHAR exact bytes;
- component-wise for composite keys.

## Sort/Limit

| Mechanism | Contract | Status |
|---|---|---|
| Sort comparator | Type order, direction, NULL placement, collation | Clean |
| Equal-key ties | No stable order | Clean |
| Full/external/merge | Same comparator | Clean |
| Limit | OFFSET first, LIMIT second | Clean |
| Unordered Limit | Any exact-cardinality sub-bag | Clean |
| Top-N comparator/result | Same as Sort, then OFFSET/LIMIT | Clean except overflow |
| `N + OFFSET` overflow | Physical error in §30.7 | **Blocking contradiction** |

## Subqueries and early termination

| Form | Physical demand |
|---|---|
| Scalar | Lazy once; consume at most two final rows; second successful row errors |
| EXISTS | Lazy once; projection values irrelevant; stop after existence |
| IN | Lazy complete build; set plus NULL/empty semantics |
| NOT EXISTS/NOT IN | Ordinary 3VL NOT wrappers |
| Derived table | Ordinary relational child; no mandatory materialization |
| Correlated forms | No v1 physical alternative |
| Limit | Stops only semantically unnecessary upstream work |
| Cancellation | Distinct from early stop |

# Materialization, memory, and expression model

## Materialization/rescan

- Materialization preserves row values, multiplicity, slot mapping, owned VARCHAR bytes, and any explicitly required order.
- Temporary row handles are not RIDs.
- Retained values are deep-copied.
- Nested-loop right input and DML target spools are materialized for their respective physical requirements.
- V1 has no general rewindability property lattice; algorithm-specific materialization supplies required replay.
- Rescan must not obtain a new snapshot or rebind names.

## Blocking/streaming

| Class | Operators |
|---|---|
| Sources | SeqScan, IndexScan, Values, finalized blocker sources |
| Streaming | Filter, Project, Limit |
| Breakers/sinks | Hash build, Aggregate, Sort, DML target spool, IN build |
| Control roles | DDL, VACUUM, ANALYZE |
| Finalization rule | Dependents remain non-runnable until successful Finalize |

## Memory/spill

Chapter 22 correctly hands off:

- query accounting to `QueryMemoryManager`;
- temporary retained rows to `RowCollection`;
- small execution metadata to `QueryArena`;
- spill lifecycle to `SpillManager`;
- operator-specific spill formats to Chapters 28–30.

Spill may alter resource use and unspecified physical order, never bags, semantic order, values, aggregate errors, transaction state, or DML precedence.

## Physical expressions and vectorization

- Bound meaning and child order remain immutable.
- AND/OR and CASE preserve per-row short-circuit demand.
- Vector representation cannot change values, NULLs, errors, or provenance.
- Lane/batch order does not select D21-S4 errors.
- Synthetic error-capable expressions inherit a canonical source occurrence.
- Non-erroring internal hash/range/helper metadata need no SourceSpan.
- Source-derived physical expressions retain SourceSpan and cast provenance.

# DML/DDL physicalization matrices

## DML

| Concern | Physical rule |
|---|---|
| Target identity | Hidden physical RID retained |
| Deduplication | One finalized target RID at most once |
| Halloween protection | Finalize target spool before mutation |
| Stale target | Revalidate after logical lock |
| Retry | RC only before first published write |
| CommandId | Same admitted statement/retry CommandId |
| UPDATE row image | SET reads complete old row; simultaneous assignments |
| D21-S1 | Complete candidate row gets descriptor-wide NOT NULL validation |
| Uniqueness | Current-owner semantics; exact old RID self-exclusion |
| D21-S4 | SourceSpan/specificity/phase; no physical tie-break |
| D21-S5 | RETURNING remains unordered bag |
| Results | No failed/retried partial prefix |

## DDL

| Concern | Physical rule |
|---|---|
| CREATE TABLE | Invoke Chapter-21 private-file/catalog protocol |
| CREATE INDEX | Writer gate/drain and exact current-owner build set |
| D21-S2 | Backing-index dependency checked before mutation/retirement |
| D21-S3 | No RR snapshot/raw tuple substitute |
| D21-S6 | Name visibility and same-transaction reservation remain distinct |
| DROP | Catalog visibility and file retirement remain separate |
| Plan identity | Stable IDs/descriptors, not names |
| Execution | Control role; no rebinding |

# Error, proof, validation, and determinism matrices

## Exact proof versus estimate

| Input | May affect cost? | May establish correctness? |
|---|---:|---:|
| Statistics/estimated rows | Yes | No |
| Estimated zero | Yes | No |
| NDV/null fraction/histogram | Yes | No |
| Trusted enforced constraint | Yes | Yes, within approved proof |
| Exact typed contradiction | Yes | Yes |
| Actual SQL LIMIT 0 | Yes | Yes |
| `required_rows=0` objective | Yes | No |
| Physical operator name | Yes | No |
| Runtime capability | Legality only | No semantic proof |

## Physical-plan validation

Before execution, validation covers:

- child count and schema;
- output and required `LogicalSlotId`s;
- expression types;
- hidden RID/system slots;
- join type/key/orientation;
- required/provided order;
- pipeline legality;
- memory/spill declarations;
- query context;
- capability eligibility;
- exact-proof provenance;
- subquery mode/arity/lazy behavior;
- derived-table slot mapping;
- no data-changing side effects before successful validation.

## Planning/execution errors

- Bind/type/catalog errors are not reclassified by physical planning.
- Unsupported legal SQL because no baseline plan exists would be an architecture defect.
- `OptimizerResourceLimit` is controlled planning resource failure.
- Final-plan validation failure is an internal invariant failure.
- Execution preserves structured lower-layer causes.
- Transaction consequence remains §39.1-owned.
- B22-1 incorrectly introduces an algorithm-specific public overflow error.

## Determinism

| Perturbation | Plan shape | Output bag | Required order | Public semantic error | DML state/RETURNING |
|---|---|---|---|---|---|
| Hash seed | May not affect deterministic tie choice | No effect | No effect | No effect | No effect |
| Pointer/allocation | No semantic effect | No effect | No effect | No effect | No effect |
| RID/PageId order | Access behavior only | Same bag | Not SQL order | Not D21-S4 tie | Same state/bag |
| Vector lane/batch | Physical only | Same bag | Preserved if required | Same | Same |
| Worker schedule | Physical only | Same bag | Enforced if required | Same | Same |
| Hash bucket | Physical only | Same bag | None unless enforced | Same | Same |
| Spill partition | Physical only | Same bag | Same explicit order | Same | Same |
| Cost tie | Deterministic structural choice | Same semantics | Same requirement | Same | Same |
| Top-N overflow | **Currently changes outcome** | — | — | **Error versus success** | N/A |

# Cross-chapter and documentation matrices

## Cross-chapter composition

| Boundary | Result |
|---|---|
| Ch7/8 → Ch22 scans | Clean |
| Ch9/10 → Ch22 context/MVCC | Clean |
| Ch14 → Ch22 read epoch | Clean |
| Ch16 → Ch22 descriptors/SchemaVer | Clean |
| Ch17 → Ch22 expressions/order/hash | Clean |
| Ch19 → Ch22 identity/provenance | Clean |
| Ch20 → Ch22 logical realization | **B22-1 through Top-N** |
| Ch21 → Ch22 DML/DDL | Clean |
| Ch22 → Ch23 vectors | Clean |
| Ch22 → Ch24–26 runtime | Clean |
| Ch22 → Ch27–31 operators | Clean except Top-N |
| Ch22 → Ch37 properties | **M22-1** |
| Ch22 → Ch38 validation | Clean |
| Ch22 → Ch39 errors | B22-1 violates error boundary |
| Ch22 → Ch40 EXPLAIN | Clean |

## Documentation model

| Material | Assessment |
|---|---|
| Architecture mechanism | Predominant |
| DEVELOPMENT chronology | Present in N22-1 |
| VERIFICATION recipe | Present in N22-2 |
| PROJECT_STATE/current implementation | Present in capability/validation wording |
| Devlog/history | None |
| Source/class layout | None |
| Benchmark results | None |
| Current test results | None |

# Cross-reference table

| Source | Target | Purpose | Exists/owner | Classification |
|---|---|---|---|---|
| §22.3 | “optimizer chapters” | Physical-choice ownership | Ch36–38 exist | VAGUE BUT HARMLESS |
| §22.4 | §20.14.7 | Mandatory subquery fallback roles | Exact owner | GOOD |
| §22.4 | Chapters 28–31 | Join/aggregate/sort/DML algorithms | Exact owners | GOOD |
| §22.5 | Chapter 14 | RID reuse/read epoch | Exact owner | GOOD |
| §22.5 | Chapters 9–15 | Snapshot/lock lifetime | Broad but correct | GOOD |
| §22.7 | No Chapter-37 citation | Property summary | Missing canonical-owner navigation | M22-1 |
| Ch22→23 | No explicit reference | DataChunk handoff | Immediate heading defines it | VAGUE BUT HARMLESS |

No stale, circular, or wrong-owner explicit references were found.

# Temporality classification

| Wording | Class |
|---|---|
| “reference executor … testing” | Verification ownership, not temporality |
| “may later be cached” | Project chronology |
| “placeholders” | Implementation-state/scaffolding wording |
| “Initial physical operator family” | Project chronology |
| “currently eligible” | Current implementation/capability state |
| “available and validated” | Project state plus verification evidence |
| “later/conditional” | Project chronology |
| “after capability is enabled” | Legitimate capability condition if rewritten timelessly |
| “current command boundary” | Transaction runtime semantics |
| “later components” | Downstream navigation |
| “current single-threaded implementation” | Current implementation narration |
| “from day one” | Development sequencing |

Project-chronology occurrences: **7**.

Current-state narration: **yes**.

# Implementation coupling, terminology, and normative language

Implementation coupling is generally appropriate:

- `PhysicalOperator`, `GlobalOperatorState`, and `LocalOperatorState` are conceptual ownership roles, not mandated C++ layouts.
- `DataChunk`, `QueryMemoryManager`, `SpillManager`, and `QueryArena` are downstream architecture contracts.
- No container, allocator implementation, source file, pointer-based identity, factory, visitor, or class hierarchy is mandated.

Terminology is coherent except for §22.7’s overloading of “physical property.”

Normative language is adequate for:

- no parsing/rebinding/cost search in execution;
- immutable plan versus runtime state;
- property advertisement;
- process-local runtime state.

The property taxonomy requires stronger and more precise normative ownership.

# Analytical depth and implementation freedom

Analytically sufficient:

- vectorized production rationale;
- immutable-plan/runtime-state separation;
- execution context ownership;
- read-epoch rationale;
- no incidental order;
- capability gating;
- no persistent runtime pointers.

Thin or problematic:

- exact property taxonomy;
- future cache/reuse statement;
- distinction between v1-required and conditionally available algorithms;
- testing-specific reference executor prose.

Implementation freedom is otherwise preserved. Algorithms may vary when they preserve frozen logical semantics, exact required properties, provenance, transaction contracts, and validation.

# Technical consistency question matrix — 225 actual questions

Legend:

- `C` — CONSISTENT
- `CS` — CONSISTENT BUT SPECIALIZED downstream
- `F` — FINDING
- `N/A` — not part of the v1 Chapter-22 surface

## Logical handoff — 1–15

1. Does Chapter 22 consume resolved plans? — C
2. Does execution avoid parsing? — C
3. Does execution avoid SQL name resolution? — C
4. Does execution avoid hot-loop catalog-name lookup? — C
5. Does execution avoid cost search? — C
6. Are bound types retained? — C
7. Are stable object IDs retained? — C
8. Are immutable descriptors consumed? — C
9. Does physical planning avoid rebinding? — C
10. Does logical bag meaning remain upstream-owned? — C
11. Does statement publication remain Chapter-21-owned? — C
12. Are hidden DML slots retained? — C
13. Are subquery modes consumed unchanged? — C
14. Are unsupported correlated forms absent? — C
15. Can downstream implementation proceed without SQL-policy invention? — C

## Plan and runtime ownership — 16–30

16. Is the physical plan immutable after publication? — C
17. Is runtime state per execution? — C
18. Is global state distinct from plan state? — C
19. Is local worker state distinct from global state? — C
20. Is transaction state excluded from plan nodes? — C
21. Is snapshot state excluded from plan nodes? — C
22. Is read-epoch registration excluded from plan nodes? — C
23. Is cancellation state excluded from plan nodes? — C
24. Are cursors runtime-owned? — C
25. Are chunks runtime-owned? — C
26. Are memory reservations runtime-owned? — C
27. Are spill runs runtime-owned? — C
28. Are runtime pointers forbidden from persistence? — C
29. Is cache/reuse validity fully specified? — F, document/future-policy issue
30. Does plan reuse currently define a v1 feature? — N/A; wording is chronological

## Slots and output schema — 31–45

31. Does each physical operator carry output schema/slots? — C
32. Is `LogicalSlotId` preserved as semantic identity? — C
33. Is physical vector position distinct? — C
34. Is BindingId distinct from output slot? — C
35. Is ColumnId distinct from output slot? — C
36. Is heap SlotId distinct? — C
37. Is aggregate ordinal distinct? — C
38. Does `SELECT a,a` keep two outputs? — C
39. Do equal expressions remain distinct outputs? — C
40. Do self-join columns remain distinct? — C
41. Is join schema left then right? — C
42. Does Project create declared outputs in order? — C
43. Do schema-preserving nodes retain slots? — C
44. Does derived-table remapping remain explicit? — C
45. Are pointers excluded as slot identity? — C

## Bags and order — 46–60

46. Do scans preserve visible-row multiplicity? — C
47. Does Filter avoid duplicate elimination? — C
48. Does Project avoid duplicate elimination? — C
49. Do joins preserve pair multiplicity? — C
50. Does aggregate collapse only groups? — C
51. Does DISTINCT alone collapse duplicate classes? — C
52. Does Sort preserve the input bag? — C
53. Does Limit select without deduplicating? — C
54. Does physical heap order remain nonsemantic? — C
55. Does RID order remain nonsemantic? — C
56. Does hash order remain nonsemantic? — C
57. Does vector lane order remain nonsemantic? — C
58. Does worker order remain nonsemantic? — C
59. Does RETURNING remain unordered? — C
60. Does DML error precedence ignore physical order? — C

## Properties and proof — 61–75

61. Is §22’s exact v1 property set consistent with §37? — F
62. Is order matching strictly slot-based in §22? — F
63. Is Chapter 37’s slot-based rule determinate? — C
64. Is exact-prefix order satisfaction defined? — C
65. Are direction mismatches rejected? — C
66. Are NULL-order mismatches rejected? — C
67. Are collation mismatches rejected? — C
68. Are estimates separated from exact proof downstream? — C
69. Can estimated zero prove emptiness? — C, no
70. Can cost establish uniqueness? — C, no
71. Can statistics establish scalar cardinality ≤1? — C, no
72. Is RequiredRowsObjective nonsemantic? — C
73. Is RequiredSlotSet exact? — C
74. Are future partition/rewind properties excluded from v1’s lattice? — C in Ch37, F in Ch22 summary
75. Is a false property claim rejected before execution? — C

## Scans and indexes — 76–90

76. Does SeqScan obtain snapshot from execution context? — C
77. Does SeqScan perform exact MVCC? — C
78. Does SeqScan emit one occurrence per visible logical row? — C
79. Does SeqScan avoid SQL-order claims? — C
80. Does IndexScan treat entries as candidates? — C
81. Does IndexScan fetch the heap? — C
82. Does IndexScan perform heap MVCC? — C
83. Are index RIDs read-epoch protected? — C
84. Are stale entries safely rejected? — C
85. Are residual predicates retained? — C
86. Can exact index bounds discharge a predicate? — C
87. Is `= NULL` prevented from becoming NULL-key lookup? — C
88. Can `IS NULL` use exact key representation? — C
89. Is forward order limited to compatible ASC/NULLS FIRST? — C
90. Is index-only scan available? — N/A; no v1 visibility path

## Filter, Project, expressions — 91–105

91. Does Filter retain TRUE rows? — C
92. Does Filter reject FALSE? — C
93. Does Filter reject UNKNOWN? — C
94. Does Filter preserve input order? — C
95. Does Filter preserve slots? — C
96. Does Project emit one row per input occurrence? — C
97. Does Project keep duplicate output positions distinct? — C
98. Can physical sharing preserve distinct output mappings? — C
99. Are computed VARCHAR results owned? — C
100. Is AND left-first? — C
101. Is OR left-first? — C
102. Are CASE branch demands preserved? — C
103. Are skipped vector lanes unevaluated where required? — C
104. Is physical expression state per execution? — C
105. Is SourceSpan/provenance preserved through physicalization? — C via Ch20/25

## Joins — 106–120

106. Is NestedLoop a general correctness baseline? — C
107. Does NestedLoop support INNER? — C
108. Does NestedLoop support LEFT? — C
109. Does NestedLoop support CROSS? — C
110. Does HashJoin preserve duplicates? — C
111. Does HashJoin reject NULL equi-keys as matches? — C
112. Are hash collisions fully compared? — C
113. Are residuals evaluated after key match? — C
114. Does LEFT matchedness include residual truth? — C
115. Is one unmatched LEFT row emitted? — C
116. Is LEFT hash orientation fixed? — C
117. Does INLJ recheck heap MVCC? — C
118. Does merge join require exact compatible order? — CS
119. Can spill/repartition change only unspecified physical order? — C
120. Are join algorithms bag/error-equivalent? — C

## Aggregate and DISTINCT — 121–135

121. Does global aggregate produce one row on empty input? — C
122. Does grouped aggregate produce no groups on empty input? — C
123. Do grouping NULLs form one class? — C
124. Are NaNs grouping-equivalent? — C
125. Are ±0 grouping-equivalent? — C
126. Is VARCHAR grouping byte-exact? — C
127. Does hash equality agree with grouping equality? — C
128. Do aggregate ordinals survive physicalization? — C
129. Can equal aggregate expressions remain distinct occurrences? — C
130. Does sharing preserve separate output/provenance mappings? — C
131. Does hash aggregate advertise no order? — C
132. Does hash DISTINCT advertise no order? — C
133. Does sort aggregate use the same groups/values? — CS
134. Does spill preserve exact aggregate state? — C
135. Does physical order avoid selecting aggregate errors? — C

## Sort and Limit — 136–150

136. Does Sort use resolved key sequence? — C
137. Does Sort honor ASC/DESC? — C
138. Does Sort honor resolved NULL placement? — C
139. Does Sort use binary VARCHAR collation? — C
140. Does Sort use the FLOAT64 total order? — C
141. Do prefix ties fall back to full comparison? — C
142. Are equal-key ties unstable unless otherwise required? — C
143. Does Limit apply OFFSET first? — C
144. Does Limit then apply LIMIT? — C
145. Does ordered Limit preserve surviving order? — C
146. Does unordered Limit avoid creating canonical first rows? — C
147. Does Limit avoid `o+l` logically? — C
148. Does Top-N compute `N+OFFSET`? — C as text
149. Can Top-N overflow produce a new error? — **F, B22-1**
150. Are Sort+Limit and Top-N always observationally equivalent? — **F**

## Subqueries/materialization — 151–165

151. Is scalar side execution lazy? — C
152. Is it once per occurrence/attempt? — C
153. Does zero scalar rows produce typed NULL? — C
154. Does the second completed scalar row error? — C
155. Does EXISTS skip projection value evaluation? — C
156. Can EXISTS stop after one row? — C
157. Does IN consume the complete final child? — C
158. Does IN retain a NULL marker? — C
159. Does NOT IN remain 3VL NOT? — C
160. Is derived-table materialization optional? — C
161. Does derived mapping preserve bag multiplicity? — C
162. Does materialization own retained VARCHAR bytes? — C
163. Does rescan retain the same snapshot? — C
164. Are correlated rescans absent? — N/A/unsupported
165. Are lazy side states discarded on retry? — C

## DML — 166–180

166. Does UPDATE/DELETE finalize targets before writes? — C
167. Is each target RID included at most once? — C
168. Is Halloween rediscovery prevented? — C
169. Is each target revalidated after lock acquisition? — C
170. Does RC retry occur only pre-write? — C
171. Does retry reuse CommandId? — C
172. Does retry refresh statement snapshot? — C
173. Does UPDATE evaluate SET from old row? — C
174. Are assignments simultaneous? — C
175. Is descriptor-wide NOT NULL preserved? — C
176. Is immediate uniqueness current-owner-based? — C
177. Does D21-S4 ignore spool/vector order? — C
178. Does D21-S5 ignore mutation/RID order? — C
179. Is failed/retried RETURNING hidden? — C
180. Does post-write failure require abort? — C

## DDL, context, and reuse — 181–195

181. Does CREATE TABLE invoke Chapter 21? — C
182. Does CREATE INDEX use exact current-owner input? — C
183. Is writer drain preserved? — C
184. Is ordinary RR visibility not substituted? — C
185. Is backing-index DROP protection preserved? — C
186. Is dropped-name reservation preserved? — C
187. Are names not used as stable identity? — C
188. Does each execution use one context? — C
189. Does RC use the statement snapshot? — C
190. Does RR use transaction snapshot plus command boundary? — C
191. Does context preserve lock lifetimes? — C
192. Is one ReadEpochGuard sufficient for retained index RIDs? — C |
193. Can active plan caching ignore SchemaVer/object replacement? — C, no |
194. Is a plan-cache invalidation policy defined? — N/A/future wording |
195. Can retries reuse immutable shape with refreshed runtime context? — C if descriptors remain valid |

## Cost, validation, determinism — 196–210

196. Does cost influence preference only? — C
197. Can a cost mistake change SQL meaning? — C, forbidden
198. Can estimated rows remove all runtime paths? — C, no
199. Is optimizer capability checked? — C
200. Are required slots validated? — C
201. Is required order validated/enforced? — C
202. Are illegal predicates rejected? — C
203. Are hidden DML slots validated? — C
204. Is exact-proof provenance validated? — C
205. Is validation before data effects? — C
206. Can hash iteration choose a plan tie? — C, no
207. Can pointer address choose a plan tie? — C, no
208. Can worker order change ordered results? — C, no
209. Can vector width change scalar/subquery errors? — C, no
210. Can capability state admit an unimplemented operator? — C, no

## Documentation and regression — 211–225

211. Is Chapter 22 wholly timeless? — F
212. Does it contain current-implementation wording? — F
213. Does it contain project chronology? — F
214. Does it contain a verification recipe? — F
215. Does it contain source-layout coupling? — C, no
216. Does it contain benchmark results? — C, no
217. Is “optimizer chapters” precise? — CS, vague but harmless
218. Is §20.14.7 correctly referenced? — C
219. Are Chapters 28–31 correctly referenced? — C
220. Is Chapter 14 correctly referenced? — C
221. Are Chapters 9–15 correctly referenced? — C
222. Does Chapter 22 cite Chapter 37 as property owner? — F
223. Does Chapter 22 preserve D19 identities/provenance? — C
224. Does Chapter 22 preserve D20 and D21 decisions? — F only for D20-M4/Top-N
225. Can Chapter 22 currently stand unchanged as final canonical v1 Architecture? — F

# Implementer-invention assessment

All major physical contracts are implementable without new policy except:

1. `PhysicalTopN` overflow behavior must be reconciled.
2. Chapter 22’s property taxonomy must be aligned with Chapter 37.
3. Cache/reuse wording should either be removed from the v1 contract or given a timeless, identity-safe scope.

No invention is needed for:

- slot mapping;
- scan MVCC;
- stale index handling;
- residual predicates;
- join bags/null extension;
- grouping/DISTINCT;
- sort comparator/ties;
- subqueries;
- materialization;
- DML/DDL;
- statement attempts;
- provenance;
- validation.

# Frozen-decision regression

## D19

- BindingId: preserved.
- Aggregate ordinal: preserved.
- SourceSpan/cast provenance: preserved.
- Deterministic semantic errors: preserved except B22-1 introduces an unrelated physical error.

## D20

- D20-B1 demanded evaluation: preserved.
- D20-B2 executable operand order: preserved.
- D20-M1 LogicalSlotId: preserved, but §22.7’s expression alternative should be removed.
- D20-M2 bag semantics: preserved.
- D20-M3 joins: preserved.
- D20-M4 Limit: **violated by Top-N overflow**.
- D20-M5 aggregate occurrence identity: preserved.
- D20-M6 provenance: preserved.

## D21

- D21-S1: preserved.
- D21-S2: preserved.
- D21-S3: preserved.
- D21-S4: preserved.
- D21-S5: preserved.
- D21-S6: preserved.

# Frozen Chapter-22 semantic question

## Q22-1 — Overflow while deriving Top-N/first-K bound

Sections:

- §20.12
- §§22.4–22.4.1
- §30.7
- §§38.15–38.16

State:

```text
validated nonnegative INT64 LIMIT N
validated nonnegative INT64 OFFSET M
N + M exceeds INT64_MAX
logical query otherwise valid
ORDER BY permits either Sort+Limit or Top-N
```

Interpretation A:

- `o+l` is not a semantic computation;
- no SQL error occurs;
- Top-N/finite-first-K optimization becomes ineligible or uses a non-erroring saturated/domain-safe internal bound;
- execute another conforming plan.

Interpretation B:

- checked `N+OFFSET` overflow is a planning/execution error;
- physical Top-N choice can make the query fail.

Observable consequence:

- successful result versus public error;
- plan choice becomes correctness-visible.

Frozen upstream constraints:

- §20.12 explicitly says there is no such overflow path and the boundary is not an error.
- §38.16 says the objective is costing-only and never changes semantics.
- Chapter 22 requires physical algorithms to realize resolved semantics, not redefine them.

Smallest decision:

- explicitly select the no-new-error behavior and define Top-N/objective fallback on overflow.

# Follow-up Verification gaps

| Architecture owner | Existing methodology | Status | Needed future action |
|---|---|---|---|
| §22.1 vectorized execution | Operator/vector/pipeline tests | COMPLETE | Reuse |
| §22.2 plan/runtime split | Validator, pipeline, parallel-state tests | COMPLETE | Reuse |
| §22.3 operator contract | Physical-plan validator | COMPLETE | Reuse |
| §22.4 vocabulary | Positive corpus/control-operator tests | PARTIAL | Add closed vocabulary/capability matrix |
| §22.4.1 capability registry | Validator/final optimizer validation | COMPLETE | Reuse |
| §22.5 context | Context validator, retry, scan/read-epoch tests | COMPLETE | Reuse |
| §22.6 runtime ownership | Pipeline, lifetime, spill/persistence tests | COMPLETE | Reuse |
| §22.7 ordering | Physical-property tests | COMPLETE under Chapter 37 | Remap after document correction |
| §22.7 broad property list | No coherent single oracle while wording drifts | PARTIAL | Fix Architecture taxonomy, then synchronize |
| §22.8 invariants | Reusable execution methods | COMPLETE | Add direct V22 ledger |
| Top-N overflow | “large checked K” test does not choose success/error | BLOCKED BY Q22-1 | Freeze decision first |
| Plan cache/reuse | No active v1 policy | MISSING if retained as active feature | Remove future wording or define then verify |

No Verification file was modified.

# Chapter-23 boundary

The exact next chapter is:

`# 23. Vectorized Data and String Representation`

Chapter 22 provides the execution/operator ownership model; Chapter 23 defines:

- `DataChunk`;
- vector capacity/cardinality;
- vector representations;
- validity and selection;
- logical versus physical positions;
- VARCHAR `StringRef`;
- borrowed/owned string lifetime;
- chunk reuse.

Recommended future Chapter-23 review scope:

- physical position versus logical row identity;
- selection/dictionary composition;
- empty chunk versus end-of-stream;
- validity invariants;
- StringRef byte semantics and ownership;
- borrowed-data lifetime;
- vector-capacity arithmetic;
- representation-independent values/errors;
- implementation-era wording in the chapter.

Chapter 23 itself was **not reviewed**.

# Final answers

- Project chronology present? **Yes — seven Chapter-22 occurrences.**
- Current-state narration present? **Yes.**
- DEVELOPMENT-owned material present? **Yes, limited to chronology/capability wording.**
- VERIFICATION recipe present? **Yes, the reference-executor testing sentence.**
- PROJECT_STATE leakage present? **Yes, “available and validated/currently eligible.”**
- History/devlog leakage present? **No.**
- Logical→physical handoff ambiguity? **No, apart from Top-N’s contradictory realization.**
- Output-schema ambiguity? **No.**
- LogicalSlotId mapping ambiguity? **No in canonical downstream rules; §22.7 needs correction.**
- Bag ambiguity? **No.**
- Physical/semantic order ambiguity? **No generally.**
- Property ambiguity? **Yes, M22-1.**
- Exact/estimate ambiguity? **Yes in §22.7 summary; canonical Chapters 35–38 resolve it.**
- Scan/MVCC ambiguity? **No.**
- Stale-index ambiguity? **No.**
- Residual-predicate ambiguity? **No.**
- Index-order ambiguity? **No.**
- Join ambiguity? **No.**
- Aggregate/grouping ambiguity? **No.**
- Aggregate-ordinal ambiguity? **No.**
- DISTINCT ambiguity? **No.**
- Sort/tie ambiguity? **No.**
- Limit/early-stop ambiguity? **Yes: Top-N overflow only.**
- Subquery ambiguity? **No.**
- Materialization/rescan ambiguity? **No correctness gap.**
- Expression/provenance ambiguity? **No.**
- Vector/lane-order ambiguity? **No.**
- DML physicalization ambiguity? **No.**
- D21-S4 ambiguity? **No.**
- D21-S5 ambiguity? **No.**
- DDL physicalization ambiguity? **No.**
- D21-S3 ambiguity? **No.**
- Snapshot/attempt ambiguity? **No.**
- Plan reuse/cache ambiguity? **Future wording lacks an active policy; document cleanup required.**
- Cost/correctness ambiguity? **No, except Top-N overflow.**
- Estimate/correctness ambiguity? **Canonical owner is clear; §22.7 summary needs repair.**
- Physical-plan validator ambiguity? **No.**
- Deterministic-user-error ambiguity? **Yes, B22-1.**
- Correctness-relevant implementer invention required? **Yes, only until Q22-1 is frozen.**
- Can Chapter 22 stand unchanged years later as canonical v1 Architecture? **No.**

# Recommended next action

**FROZEN CHAPTER-22 SEMANTIC REVIEW / DECISION PACKAGE**

Scope it narrowly to:

1. Top-N and `RequiredRowsObjective` overflow behavior;
2. preservation of D20-M4’s no-overflow semantics;
3. the exact fallback/ineligibility rule for Top-N.

Do not perform Chapter-22 document cleanup or Verification synchronization before that decision is frozen.

Afterward:

1. targeted Chapter-22 document cleanup for M22-1/N22-1/N22-2;
2. Chapter-22 Verification synchronization;
3. only then declare Chapter 22 fully reviewed and closed;
4. then begin Chapter 23 direct review.

Chapter 23 review: **NOT STARTED**.
Verification synchronization: **NOT PERFORMED**.
Implementation: **NONE**.
Build/tests/benchmarks: **NONE**.
Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.