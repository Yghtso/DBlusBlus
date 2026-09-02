# Chapter 25 review verdict

**CHAPTER 25 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED.**

One correctness-observable semantic question remains undefined: selection of the public ordinary expression error for non-DML execution when multiple demanded row/expression occurrences fail.

Finding totals:

| Severity | Count |
|---|---:|
| BLOCKING | 1 |
| MAJOR | 4 |
| MINOR | 3 |
| EDITORIAL | 0 |

## Repository preservation

| State | Branch | HEAD | Working tree | Index |
|---|---|---|---|---|
| Initial | `main` | `547a2e03f32c383d37ade42a1fed64d284a8f89f` | clean | clean |
| Final | `main` | `547a2e03f32c383d37ade42a1fed64d284a8f89f` | clean | clean |

- `git diff --check`: passed
- Audit-created changes: **NONE**
- Historical review artifacts: unread, unmodified, unstaged
- No files were modified, staged, or created.

# Scope and structure

- Exact title: `# 25. Vectorized Expression Execution`
- Start: [docs/ARCHITECTURE.md:20222](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20222)
- Last substantive line: 20356
- Chapter separator: 20358
- Review boundary ends: 20359
- Chapter 26 heading: `# 26. Pipeline Execution Model`, line 20360

## Section-review matrix

| Section | Exact heading | Responsibility | Upstream owner | Downstream consumer | Document role |
|---|---|---|---|---|---|
| 25.1 | Expression execution state | Per-execution expression state and batch evaluation interface | Ch19–24 | Ch26–31 | ARCHITECTURE-APPROPRIATE |
| 25.2 | Input normalization | Unified representation consumption and active-row iteration | Ch23 | Kernels/operators | ARCHITECTURE-APPROPRIATE |
| 25.3 | Arithmetic kernels | Batch dispatch, validity paths, resolved physical arithmetic | Ch17/19/39 | Ch27–31 | ARCHITECTURE-APPROPRIATE |
| 25.4 | Comparison kernels | Nullable BOOLEAN results and direct filter selections | Ch17/23 | PhysicalFilter/joins | ARCHITECTURE-APPROPRIATE |
| 25.5 | Vectorized AND short-circuit | Per-lane AND demand subset | §17.7.3, Ch20 | Filter/Project/DML | ARCHITECTURE-APPROPRIATE |
| 25.6 | Vectorized OR short-circuit | Per-lane OR demand subset | §17.7.3, Ch20 | Filter/Project/DML | ARCHITECTURE-APPROPRIATE |
| 25.7 | Result ownership | Fixed-width, VARCHAR, and borrowed result lifetime | Ch23/24 | Ch26/27/31 | ARCHITECTURE-APPROPRIATE |
| 25.8 | Expression invariants | Consolidated execution constraints | Ch17–24, §29.3 | All physical consumers | ARCHITECTURE-APPROPRIATE |

No Development, Verification, Project State, or historical material appears in Chapter 25.

# Canonical owner map

| Concept | Classification | Canonical owner/result |
|---|---|---|
| Batch `Evaluate` interface | CHAPTER 25 OWNS | Expression state + chunk + active selection → vector |
| Batch/kernel dispatch | CHAPTER 25 OWNS | Per expression/vector batch |
| Vector normalization consumption | CHAPTER 25 OWNS | Uses Ch23 `UnifiedVectorFormat` |
| AND/OR physical demand subsets | CHAPTER 25 OWNS | Physical realization of §17.7.3 |
| Computed result-vector ownership | CHAPTER 25 OWNS | Subject to Ch23/24/26 |
| Scalar type/value/operator meaning | EARLIER OWNER | Chapter 17 |
| Syntax and direct-negative provenance | EARLIER OWNER | Chapter 18 |
| Bound type, overload, cast, and function identity | EARLIER OWNER | Chapter 19 |
| Demand and executable scalar order | EARLIER OWNER | Chapter 20 |
| DML candidate precedence | EARLIER OWNER | §21.16.1 |
| Physical output `LogicalSlotId` schema | EARLIER OWNER | Chapter 22 |
| FLAT/CONSTANT/DICTIONARY and active domain | EARLIER OWNER | Chapter 23 |
| Borrow/value stability | EARLIER OWNER | §§23.10–23.13 |
| Memory/resource accounting | EARLIER OWNER | Chapter 24 |
| Pipeline scheduling and cancellation | LATER OWNER | Chapter 26 |
| Filtering/project cardinality | LATER OWNER | §§27.7–27.8 |
| Aggregate state/reduction | LATER OWNER | §29.3 |
| DML/result publication | LATER OWNER | Chapters 21/31 |
| Public resource taxonomy | EARLIER OWNER | §39 |
| Ordinary non-DML multi-candidate error winner | AMBIGUOUS OWNER | No complete owner found |
| Physical slot-to-chunk mapping | AMBIGUOUS HANDOFF | Ch20/22/23 constrain it; §25.1 omits it |
| DML expression-candidate/provenance transport | AMBIGUOUS HANDOFF | Ch20/21 require it; Ch25 omits local contract |

# Cross-chapter handoffs

| Handoff | Contract | Result |
|---|---|---|
| Ch17→25 | Exact scalar values, NULL/UNKNOWN, arithmetic, comparison, casts, child order | Preserved; local generic handoff incomplete |
| Ch18→25 | SourceSpan and direct-negative provenance | Executor must not reinterpret syntax; physical provenance handoff unstated |
| Ch19→25 | Resolved bound expression, type, nullability, cast provenance, operator ID | No executor-time resolution; consistent |
| Ch20→25 | LogicalSlotId, semantic demand, child order, provenance | Semantics exact; demand/mapping integration incomplete |
| Ch21→25 | DML ordinary candidate set and D21-S4 ranking | Outcome frozen; Chapter-25 candidate handoff unstated |
| Ch22→25 | Immutable physical expressions versus per-execution mutable state | Consistent |
| Ch23→25 | Active domain, normalized vectors, validity, borrowing | Consistent |
| Ch24→25 | Accounting, exact representability, OOM/resource distinctions | Globally consistent; local navigation incomplete |
| Ch25→26 | Expression vectors/borrowed views enter synchronous pipeline execution | Immediate boundary confirmed |

# Expression execution model

## Identity and state

The final semantic input chain is:

```text
bound occurrence
  -> logical expression referencing LogicalSlotId
  -> physical expression with resolved child-schema mapping
  -> physical schema entry S[j]
  -> DataChunk column j
```

`LogicalSlotId`, not vector address, lane, pointer, or unrelated ordinal, is semantic identity. The physical schema ordinal is only the resolved addressing mechanism.

Chapter 25 does not explicitly state that resolved slot-to-ordinal mapping, producing M25-3.

Output `LogicalSlotId` creation belongs to LogicalProject/other logical owners. Chapter 25 computes a vector for an already-owned output occurrence; it does not create BindingId, LogicalSlotId, RID, or row identity.

Physical expression state is per execution. Immutable meaning remains in the bound/physical plan; mutable scratch belongs to query/local execution state under Chapters 22, 24, and 26.

## Vector and selection model

- FLAT, CONSTANT, and DICTIONARY equivalent logical inputs must produce equivalent values, NULLs, and errors.
- `UnifiedVectorFormat` resolves the effective data, selection, and validity views.
- Only active logical positions may be evaluated or accessed.
- Repeated selection indices remain repeated occurrences.
- Unsorted selections preserve listed occurrence sequence.
- Inactive capacity and stale payload are inaccessible.
- A selection is an execution domain; it is semantically a demand mask only when constructed from Chapter-20 demand rules.
- PhysicalFilter owns row removal; expression evaluation does not independently change cardinality.

## Demand and child order

Chapter 17/20 determine:

- ordinary scalar children: left-to-right;
- ordinary strict operators: evaluate demanded children first, then NULL may suppress only the non-NULL operation;
- AND: skip RHS only when LHS is FALSE;
- OR: skip RHS only when LHS is TRUE;
- CASE: source-ordered WHENs, first TRUE result only, ELSE only if no TRUE;
- IN list: left once, items left-to-right until TRUE, continuing after UNKNOWN where necessary;
- CAST/IS NULL/NOT: child demanded when parent occurrence is demanded;
- subqueries: lazy once per bound occurrence per statement attempt.

Chapter 25 correctly implements AND/OR and lazy side plans, but does not establish one generic nested-demand-mask rule for CASE, IN, strict ordinary operators, or nested control forms. That is M25-1.

## Zero rows and constants

- A zero-cardinality input creates no per-row Project or Filter demand.
- Empty/skipped subquery selections do not initialize side-plan state.
- A fully constant dominating error is handled by mandatory Chapter-17 folding, independent of estimated or actual row count.
- A constant error in a skipped branch is suppressed.
- A residual runtime expression is evaluated only if runtime control flow demands it.
- A deterministic folded constant may be represented by CONSTANT storage without collapsing logical result multiplicity.

No separate zero-row or erroring-constant semantic question was found.

# Scalar forms

| Form | Semantic owner | Demand/order | Runtime treatment | Status |
|---|---|---|---|---|
| Constant | Ch17/19 | Per demanded occurrence after folding | CONSTANT or equivalent vector | CONSISTENT |
| Column reference | Ch19/20/22 | Parent demand | Borrow/reference when lifetime permits | HANDOFF GAP |
| Unary `+/-` | Ch17 | Operand demanded | Resolved typed kernel | CONSISTENT |
| `NOT` | §17.7 | Operand demanded | 3VL vector operation | CONSISTENT |
| Arithmetic | §§17.6,39.3 | Left then right; strict after child evaluation | Fixed-width kernel | CONSISTENT |
| Comparison | §17.7 | Left then right; strict | Nullable BOOLEAN or filter selection | CONSISTENT |
| `IS NULL`/`IS NOT NULL` | §17.7.2 | Child demanded | Non-NULL BOOLEAN | CONSISTENT |
| AND | §17.7.3 | LHS first; selective RHS | Explicit §25.5 subset | CONSISTENT |
| OR | §17.7.3 | LHS first; selective RHS | Explicit §25.6 subset | CONSISTENT |
| CAST | §§17.8,19.6 | Child demanded | Resolved cast kernel | CONSISTENT |
| Searched CASE | §§17.7.3,17.9.1,19.16 | Ordered per-row branch demand | Required but not locally described | DOCUMENT GAP |
| IN list | §§17.9.2,19.17 | Left once, items ordered | Required but not locally described | DOCUMENT GAP |
| Scalar subquery | §20.14 | Lazy side plan, 0/1/2 rows | §25.1 specialized state | CONSISTENT BUT SPECIALIZED |
| EXISTS | §20.14.5 | Stop after first final row | Specialized side plan | CONSISTENT BUT SPECIALIZED |
| IN subquery | §20.14.6 | Left first; complete lazy build | Specialized state/build | CONSISTENT BUT SPECIALIZED |
| Named scalar function | §§17.9.3,19.9 | No legal v1 instance | N/A |
| Aggregate | §29.3 | Argument expression only in Ch25 | Reduction is later owner | CONSISTENT BUT SPECIALIZED |

# Demand and mask matrices

## AND/OR/CASE

| Form/state | Child demanded | Error visible? |
|---|---|---|
| `FALSE AND rhs` | RHS no | no |
| `TRUE AND rhs` | RHS yes | yes |
| `NULL AND rhs` | RHS yes | yes |
| `TRUE OR rhs` | RHS no | no |
| `FALSE OR rhs` | RHS yes | yes |
| `NULL OR rhs` | RHS yes | yes |
| CASE WHEN TRUE | selected THEN only | selected path only |
| CASE WHEN FALSE | continue | condition errors only |
| CASE WHEN NULL | continue | condition errors only |
| Unselected THEN | no | no |
| ELSE with no TRUE | yes | yes |
| ELSE after TRUE | no | no |
| Branch with zero demanded lanes | no | no |

## Child order

| Form | Order |
|---|---|
| Unary/NOT/CAST/IS NULL | sole child |
| Arithmetic/comparison | left then right |
| AND/OR | left then conditionally right |
| CASE | WHEN conditions in order; selected result only |
| IN list | left once; items left-to-right |
| Subquery IN | left before dormant build; complete build before probe |
| Ordinary Project outputs | Ordered output positions exist, but public cross-output error ranking is undefined |
| Named scalar call | N/A in v1 |

## NULL propagation

| Form | Rule |
|---|---|
| Ordinary arithmetic | Strict typed NULL after demanded children complete |
| Comparison | Any NULL operand → UNKNOWN |
| NOT | NULL → NULL BOOLEAN |
| AND/OR | Exact 3VL tables |
| CAST | NULL → typed NULL without payload conversion |
| IS NULL | Always non-NULL BOOLEAN |
| CASE condition NULL | Not TRUE; continue |
| IN | Exact TRUE/UNKNOWN/FALSE accumulation |
| Filter result NULL | Reject row, like FALSE |
| CONSTANT NULL | Repeated NULL occurrence; multiplicity retained |

# String and ownership model

- VARCHAR uses exact bytes and exact length.
- StringRef prefix is only a rejection/filter cache; equal prefix never proves equality or ordering.
- Embedded NUL is ordinary data.
- Empty non-NULL VARCHAR is distinct from NULL.
- Computed string bytes require a valid result owner, normally output `StringHeap`.
- Borrowed results require a value-stable view through the borrow interval.
- Input/output aliasing is legal only if every live alias retains its required value.
- In-place writes cannot overwrite still-needed inputs or shared CONSTANT/DICTIONARY views.
- Retaining boundaries obtain stable ownership or copy.
- Large VARCHAR follows D23-S3/D24-S4/S5: exact alternate representation or controlled representability/resource failure; never truncation.
- All execution-dependent result/scratch capacity remains Chapter-24-accounted.

Chapter 25 states lifetime ownership but does not directly navigate to the large-value/accounting rules, producing N25-3.

# Error model

## Candidate and precedence matrices

| Scenario | Candidate? | Winner owner | Physical lane relevant? | Status |
|---|---:|---|---:|---|
| One demanded failing lane | yes | scalar owner/category | no | CONSISTENT |
| Two demanded lanes, non-DML | yes, both semantically reached | undefined | must not be | BLOCKING |
| Repeated DICTIONARY occurrence | one occurrence per selection entry | undefined for non-DML tie | child index no | BLOCKING |
| Failing folded constant | binding/folding owner | §17.10.2 | no | CONSISTENT |
| Two failing child expressions in one scalar occurrence | first demanded by Ch17 order | Ch17/20 | no | CONSISTENT |
| Two failing Project outputs | both demanded | undefined for SELECT | output evaluation order must not be accidental | BLOCKING |
| Multiple failing Filter rows | predicate demanded per row | undefined for SELECT | no | BLOCKING |
| DML expression candidates | yes | §21.16.1 | no | OUTCOME DEFINED; HANDOFF GAP |
| Zero-row input | no per-row candidate | Ch17 folding still applies | no | CONSISTENT |
| Undemanded branch error | no candidate | Ch17/20 | no | CONSISTENT |
| Resource failure | not ordinary candidate | Ch24/39 | execution timing | CONSISTENT |
| Cancellation | not ordinary candidate | Ch26/39 | schedule only as operational event | CONSISTENT |

One failing scalar occurrence retains its responsible SourceSpan and expression phase. Exact diagnostic message text is not frozen.

Chapter 20 preserves SourceSpan/provenance through logical rewrites, and Chapter 21 requires it for DML precedence. Chapter 25 does not state that physical expression state/error objects retain it, producing M25-2.

A failed `Evaluate` must not expose stale or partially initialized active output as successful input to downstream execution, but Chapter 25 does not state the all-or-error vector publication boundary or malformed expression-state classifications. That is M25-4.

# Resource and invalid-state matrices

## Resource failures

| Cause | Category |
|---|---|
| Arithmetic overflow/division error | `ArithmeticError` |
| Supported cast value failure | `CastError`/owned semantic subtype |
| No exact runtime string/extent form | representability/resource `ExecutionError` |
| Supported exact allocation denied | `OutOfMemory` |
| Cancellation | `QueryCancelled` |
| Wrong validated kernel/type/child/slot state | internal invariant |
| Expired borrowed result | internal lifetime invariant |
| Resource versus semantic failure | No invented universal precedence; actual frozen demand and operational failure govern |
| Transaction consequence | §39.1 publication boundary |

## Invalid states

| State | Classification | Required protection |
|---|---|---|
| Input slot absent/wrong mapping | internal invalid physical plan | pre-execution validation |
| Wrong input/output TypeId | internal invalid plan/runtime state | before kernel use |
| Wrong child count | internal invalid expression state | before evaluation |
| Missing kernel for validated expression | internal capability/plan defect | before execution |
| Selection outside active domain | internal vector state | before access |
| Stale borrow | internal lifetime violation | before dereference |
| NULL lane payload garbage | valid if not inspected | validity controls |
| Uninitialized active output | internal result-state violation | before publication |
| Partially written vector after error | not a successful result | downstream must not consume |
| Narrowed extent | forbidden | checked exact-before-use |
| StringRef prefix used as equality | incorrect semantic implementation | full bytes required |

# Determinism matrix

| Perturbation | Successful value/NULL/demand/bag/order | Ordinary selected error | Resource failure |
|---|---|---|---|
| Vector capacity | invariant | unresolved non-DML | may affect feasibility only |
| Chunk boundary | invariant | unresolved non-DML | may affect resource timing |
| FLAT/CONSTANT/DICTIONARY | invariant | must be invariant | capability may differ |
| Dictionary child index | invariant | nonsemantic | no semantic effect |
| Selection layout for same sequence | invariant | must be invariant | no semantic effect |
| Repeated selection | multiplicity retained | repeated candidates | no dedup |
| SIMD width/lane completion | invariant | must not select winner | operational only |
| Worker schedule | invariant | must not select winner | operational only |
| Pointer/buffer address | invariant | nonsemantic | no semantic effect |
| Temporary allocation order | successful result invariant | no universal semantic/resource precedence | may alter legitimate OOM point |
| Memory budget | successful result invariant | semantic candidate set unchanged if reached | may change success/resource failure |

# Documentation and temporality audit

## Temporal inventory

The only meaningful target-like occurrence is:

- §25.1, “first demands an uninitialized occurrence” — runtime/lifetime ordering, classification A.

There is no project chronology.

```text
A runtime/lifetime       1
B transaction state      0
C compatibility scope    0
D durable v1 scope       0
E project chronology     0
```

No Chapter-25 occurrence of `initial`, `currently`, `later`, `future`, `deferred`, `planned`, `roadmap`, `Phase`, prototype language, or implementation-status narration was found.

## Document-owner matrix

| Owner category | Leakage |
|---|---:|
| Architecture | none |
| Development sequencing | 0 |
| Verification procedure | 0 |
| Project State narration | 0 |
| History/devlog | 0 |
| Current implementation state | 0 |

## Implementation coupling

Conceptual names `DataChunk`, `Vector`, `SelectionVector`, `UnifiedVectorFormat`, and the `Evaluate` signature describe architectural interfaces, not mandatory C++ layouts.

No AVX/SSE width, template API, function-pointer table, `std::variant`, `std::function`, exception ABI, raw struct serialization, or pointer-width dependency is frozen.

`FastKernel` and `NullableKernel` are conceptual strategy names. The all-valid specialization requirement is a performance architecture choice, not a SQL semantic mechanism.

## Terminology dictionary

| Term | Meaning/owner | Status |
|---|---|---|
| bound expression | Immutable typed semantic node, Ch19 | precise |
| physical expression | Resolved executable expression config, Ch22 | locally underspecified |
| expression state | Mutable per-execution execution state, Ch25/22 | precise |
| kernel | Typed vector-batch operation, Ch25 | precise |
| active selection | Ordered active logical occurrence domain, Ch23 | precise |
| demand | Semantic requirement to evaluate an occurrence, Ch20 | local handoff incomplete |
| mask/subset | Physical realization of demand for conditional child | implicit except AND/OR |
| logical occurrence | Bag occurrence, Ch20/23 | precise |
| lane | Physical vector position, nonsemantic | not explicitly defined in Ch25 |
| UnifiedVectorFormat | Ch23 normalized adapter | precise |
| error candidate | Ordinary established semantic failure | absent from Ch25 |
| SourceSpan | Ch18 diagnostic interval/provenance | absent from Ch25 |
| result vector | Vector for demanded active occurrences | cardinality/publication details incomplete |
| function state | No legal named scalar-function instance in v1 | N/A |

## Normative and analytical assessment

The AND/OR requirements are strong and correctly normative. Representation independence and result ownership are stated declaratively.

Missing normative integration concerns:

- generic demand-mask equivalence;
- preservation of left-to-right child order by the executor;
- per-active-occurrence output completeness;
- DML candidate/provenance handoff;
- non-DML error selection;
- invalid physical-expression state rejection.

Rationale is good for AND/OR and vector normalization, but insufficient for why CASE/IN/strict NULL masks and physical lane completion cannot determine errors.

Implementation freedom remains appropriate for kernel dispatch, output representation, borrowing mechanism, and vector specialization.

# Explicit cross-reference audit

| Source | Target | Purpose | Quality |
|---|---|---|---|
| §25.1 | §20.14.7 | Occurrence-keyed subquery side plan | GOOD |
| §25.3 | Chapter 17 | Result validity/scalar semantics | GOOD but broad |
| §25.3 | §39.3.1 | Integer arithmetic enforcement | VAGUE/WRONG EMPHASIS |
| §25.4 | Chapter 17/Chapter 8 | FLOAT/VARCHAR comparison agreement | GOOD |
| §25.5 | §17.7.3 | AND order/demand | GOOD |
| §25.6 | §17.7.3 | OR order/demand | GOOD |
| §25.7 | “pipeline lifetime rules” | Borrow permission/lifetime | VAGUE BUT HARMFUL |
| §25.8 | §29.3 | Aggregate argument handoff | GOOD |

No circular reference was found.

# Findings

## BLOCKING

### B25-1 — Ordinary non-DML runtime expression-error selection is undefined

- Sections: §§25.1, 25.8; interaction with §§17.6–17.10, 20.6–20.7, 20.17, 27.7–27.8, §39.3
- Type: **ERROR PRECEDENCE**
- Evidence: Chapter 25 defines vector evaluation but no candidate/winner rule when multiple demanded rows or Project expressions fail. D21-S4 covers DML only; §29.3 and §20.14 cover specialized aggregate/subquery cases.
- Valid state: a SELECT batch contains several demanded occurrences producing different arithmetic/cast errors, or two demanded Project outputs fail.
- Competing interpretations:
  1. first physical lane/chunk/expression evaluated wins;
  2. responsible SourceSpan ranks;
  3. logical occurrence sequence ranks;
  4. all such errors are allowed alternatives;
  5. another deterministic category/occurrence ranking applies.
- Consequence: two conforming vector widths, chunkings, output evaluation orders, or worker schedules can return different public categories/spans.
- Frozen constraints: demand and child order from Ch17/20; unordered bags cannot gain heap/lane order; physical addresses and chunk layout are nonsemantic; §39 owns categories.
- Smallest future action: freeze one ordinary non-DML runtime candidate domain and deterministic selection/equivalence rule independent of physical lane, chunk, representation, and worker schedule.

## MAJOR

### M25-1 — Generic demand-mask execution handoff is incomplete

- Sections: §§25.1–25.6, 25.8
- Type: **DEMANDED EVALUATION**
- Evidence: side plans and AND/OR are covered, but CASE, IN-list, strict ordinary operators, nested mask composition, and zero-lane children are not.
- Owner: Ch17/19/20.
- Consequence: eager branch evaluation or NULL-based child suppression could surface undemanded errors or hide demanded ones.
- Future action: integrate the already-frozen generic demand-domain rule; no new semantic decision is needed.

### M25-2 — DML candidate/provenance handoff is unstated

- Sections: §§25.1, 25.8
- Type: **CROSS-CHAPTER HANDOFF**
- Evidence: no SourceSpan, expression-phase, or candidate transport contract appears.
- Owner: §§18.8, 20.17, 21.16.1.
- Consequence: first-lane failure could discard a higher-priority D21-S4 candidate.
- Future action: require physical expression errors to preserve canonical origin/phase and remain available to the D21 owner by any conforming mechanism.

### M25-3 — Physical input-slot mapping and result occurrence contract are implicit

- Sections: §§25.1–25.2
- Type: **EXPRESSION IDENTITY**
- Evidence: `Evaluate` accepts a DataChunk but does not state how a physical expression maps LogicalSlotId dependencies to physical schema ordinals or that the result aligns one-for-one with the active logical sequence.
- Owner: §§20.2, 22.3, 23.1.
- Consequence: stale/unrelated ordinal mapping, repeated-selection collapse, or output misalignment.
- Future action: state the resolved slot/schema mapping and active-sequence result correspondence.

### M25-4 — Invalid expression state and failed-result publication are incomplete

- Sections: §§25.1, 25.8
- Type: **INVALID RUNTIME STATE**
- Evidence: no local classification for wrong child count/type/kernel/slot mapping or uninitialized/partially written active output.
- Owner: Ch20/22 validation, Ch23 safety, §39.
- Consequence: OOB access, stale output consumption, undefined behavior, or incorrect public error.
- Future action: define invalid validated-plan/runtime states as internal and require rejection before unsafe access or successful result publication.

## MINOR

### N25-1 — Arithmetic cross-reference reverses semantic ownership

- Section: §25.3
- Type: **CROSS-REFERENCE**
- Evidence: arithmetic behavior is said to be “defined by §39.3.1”; that section itself says §§17.6.1–17.6.2 are canonical, and FLOAT64 enforcement is §39.3.2.
- Future action: point semantic meaning to Ch17 and runtime enforcement/categories to §§39.3.1–39.3.2.

### N25-2 — Borrowed-result reference is vague

- Section: §25.7
- Type: **CROSS-REFERENCE**
- Evidence: “pipeline lifetime rules” has no exact target.
- Future action: cite §§23.10–23.13 and §26.6.

### N25-3 — Resource/large-VARCHAR handoff lacks local navigation and rationale

- Sections: §§25.7–25.8
- Type: **ANALYTICAL DEPTH**
- Evidence: result lifetime is covered, but exact large-value applicability, checked size arithmetic, query accounting, and supported-form OOM distinction are not locally connected.
- Future action: concise references to D23-S3 and Chapter-24 accounting/representability rules.

No EDITORIAL findings.

# Frozen Chapter-25 semantic question

## Q25-1 — Ordinary non-DML expression error candidates and winner

- Exact sections: §§25.1, 25.8; Ch20 LogicalFilter/LogicalProject handoff
- Valid state: two or more demanded non-DML expression occurrences fail before query completion.
- Competing interpretations: physical first failure; source-span order; expression-output order; logical occurrence order; category order; explicitly unspecified alternative.
- Observable consequence: differing public category, SourceSpan, diagnostic origin, and potentially result-stream termination point.
- Frozen constraints:
  - Ch17 child order and short-circuit remain exact;
  - Ch20 demand and provenance remain exact;
  - unordered bag rows have no hidden physical order;
  - lane, chunk, pointer, dictionary child index, and worker schedule are nonsemantic;
  - resource failures remain separately owned.
- Smallest decision: define the non-DML candidate set, candidate identity, deterministic winner/equivalence relation, and its interaction with already-emitted SELECT batches without choosing a kernel mechanism.

No separate frozen question is required for CASE masks, DML candidate transport, slot mapping, ownership, or invalid state: those semantics are already constrained by upstream owners.

# Follow-up Verification gaps

| Family | Current status | Gap | Reusable oracle | Blocked? |
|---|---|---|---|---:|
| Expression forms | PARTIAL | CASE, IN-list, IS NULL, unary, output mapping not one closed family | Ch17 scalar oracle | no |
| Vector representation | COMPLETE | Existing Ch23/expression matrix adequate | logical-value vector oracle | no |
| Active logical domain | COMPLETE | Existing V23 poison/index procedures | active-domain oracle | no |
| Demand masks | PARTIAL | Nested CASE/AND/OR/IN mask composition | semantic-demand relation | no |
| AND/OR | COMPLETE | Existing 3VL/short-circuit matrix | Ch17 oracle | no |
| CASE | PARTIAL | Mixed-lane selected/unselected branches | per-occurrence branch oracle | no |
| NULL propagation | PARTIAL | Strict-child errors versus NULL payload | typed NULL oracle | no |
| Cast/runtime equivalence | COMPLETE | Existing registry/boundary coverage | Ch17 folding oracle | no |
| Arithmetic/comparison | COMPLETE | Existing checked arithmetic/FLOAT/VARCHAR coverage | exact scalar oracle | no |
| Function dispatch | N/A | Named scalar registry empty | registry closed-set oracle | no |
| VARCHAR lifetime | COMPLETE | V23 StringRef/borrow tests reusable | generation-tagged owner graph | no |
| Input/output aliasing | COMPLETE | V23 value-stable alias matrix | borrow graph | no |
| Error candidates | BLOCKED | Non-DML candidate identity/ranking absent | candidate multiset | **yes** |
| Error winner | BLOCKED | No ordinary SELECT winner oracle possible | deterministic precedence oracle | **yes** |
| D21-S4 handoff | PARTIAL | D21 is complete; V25 transport/origin integration absent | V21-13 candidate set | no |
| Width/chunk determinism | BLOCKED | Values complete; selected non-DML error undefined | rechunking oracle | **yes** |
| Resource distinction | COMPLETE | V24-L and expression cleanup reusable | cause classifier | no |
| Invalid expression state | PARTIAL | Missing wrong-child/kernel/slot/output matrix | physical-expression validator oracle | no |

# Technical consistency question matrix

Legend: `C` = CONSISTENT, `CS` = CONSISTENT BUT SPECIALIZED, `F` = FINDING, `NA` = N/A.

## Identity/state — Q001–Q020

| ID | Question | Status |
|---|---|---|
| Q001 | Are bound expressions immutable? | C |
| Q002 | Is mutable expression state per execution? | C |
| Q003 | Is plan meaning kept outside mutable state? | C |
| Q004 | Does execution avoid SQL name resolution? | C |
| Q005 | Are input types resolved before execution? | C |
| Q006 | Are operator overloads resolved before execution? | C |
| Q007 | Is pointer identity nonsemantic? | C |
| Q008 | Is vector address nonsemantic? | C |
| Q009 | Is lane position nonsemantic identity? | C |
| Q010 | Is LogicalSlotId-to-input-ordinal mapping explicit? | F |
| Q011 | Does a DataChunk realize an ordered physical schema? | C |
| Q012 | Is schema ordinal only physical addressing? | C |
| Q013 | Does Chapter 25 avoid inventing LogicalSlotId? | C |
| Q014 | Does Project own fresh output identity? | C |
| Q015 | Does Filter preserve child slot identity? | C |
| Q016 | Is one expression state reusable across batches only under safe reset? | C |
| Q017 | Is one result occurrence per active logical occurrence explicit locally? | F |
| Q018 | Is result cardinality owned by relational operators? | C |
| Q019 | Is row filtering owned by PhysicalFilter? | C |
| Q020 | Are malformed expression-state transitions locally classified? | F |

## Vector representation — Q021–Q040

| ID | Question | Status |
|---|---|---|
| Q021 | Are FLAT inputs supported? | C |
| Q022 | Are CONSTANT inputs supported? | C |
| Q023 | Are DICTIONARY inputs supported? | C |
| Q024 | Are equivalent representations value-equivalent? | C |
| Q025 | Are equivalent representations NULL-equivalent? | C |
| Q026 | Must equivalent representations preserve errors? | C |
| Q027 | Is representation dispatch batch-level? | C |
| Q028 | Is per-row representation branching avoided? | C |
| Q029 | Does UnifiedVectorFormat remain an adapter? | C |
| Q030 | Does CONSTANT map every active occurrence to position zero? | C |
| Q031 | Does CONSTANT preserve occurrence multiplicity? | C |
| Q032 | Does DICTIONARY preserve repeated occurrences? | C |
| Q033 | Does DICTIONARY preserve listed order? | C |
| Q034 | Are nested dictionaries normalized safely? | C |
| Q035 | Are intermediate dictionary indices validated? | C |
| Q036 | Are inactive positions excluded? | C |
| Q037 | Is stale allocated capacity semantically irrelevant? | C |
| Q038 | Can computed output be FLAT? | C |
| Q039 | Can a simple reference borrow? | C |
| Q040 | Can an exact implementation choose another result representation? | C |

## Selection and domain — Q041–Q060

| ID | Question | Status |
|---|---|---|
| Q041 | Does evaluation iterate only active logical rows? | C |
| Q042 | Does effective input index come from normalized selection? | C |
| Q043 | Is validity obtained from the normalized view? | C |
| Q044 | Is active selection explicitly required to equal semantic demand? | F |
| Q045 | Can selected indices address inactive capacity? | C |
| Q046 | Are repeated indices deduplicated? | C |
| Q047 | Are unsorted indices reordered? | C |
| Q048 | Does empty cardinality remain an ordinary batch? | C |
| Q049 | Does empty selection suppress side-plan initialization? | C |
| Q050 | Does empty input create per-row Project demand? | C |
| Q051 | Does empty input create Filter predicate demand? | C |
| Q052 | Can Filter return an empty batch without FINISHED? | C |
| Q053 | Does direct filter selection retain TRUE only? | C |
| Q054 | Are FALSE and UNKNOWN rejected by Filter? | C |
| Q055 | Can a comparison produce a SelectionVector directly? | C |
| Q056 | Must direct selection preserve logical occurrence order? | C |
| Q057 | Must direct selection preserve repeats? | C |
| Q058 | Can capacity determine semantic cardinality? | C |
| Q059 | Can chunk position become row identity? | C |
| Q060 | Is result active-domain initialization locally complete? | F |

## Demand/order — Q061–Q080

| ID | Question | Status |
|---|---|---|
| Q061 | Is scalar demand defined semantically upstream? | C |
| Q062 | Is demand independent of optimizer schedule? | C |
| Q063 | Is demand independent of vector lane? | C |
| Q064 | Does Chapter 25 state one generic demand-mask rule? | F |
| Q065 | Are ordinary binary children left-to-right? | C |
| Q066 | May arithmetic operands be swapped? | C |
| Q067 | May comparisons be reversed executably? | C |
| Q068 | May executable arithmetic reassociate? | C |
| Q069 | May AND/OR reorder operands? | C |
| Q070 | May a kernel eagerly evaluate every child? | C |
| Q071 | Does strict NULL handling suppress child evaluation? | C |
| Q072 | Are demanded children evaluated before strict NULL result formation? | C |
| Q073 | Is a unary operand demanded when its parent is demanded? | C |
| Q074 | Is a CAST child demanded? | C |
| Q075 | Is an IS NULL child demanded? | C |
| Q076 | Is a NOT child demanded? | C |
| Q077 | Are Project expressions demanded per output row? | C |
| Q078 | Is Filter predicate demanded per retention decision? | C |
| Q079 | Can exact proof alter demand? | C |
| Q080 | Can cost/statistics alone alter demand? | C |

## Boolean/CASE/IN — Q081–Q110

| ID | Question | Status |
|---|---|---|
| Q081 | Does FALSE AND skip RHS? | C |
| Q082 | Does TRUE AND demand RHS? | C |
| Q083 | Does NULL AND demand RHS? | C |
| Q084 | Does TRUE OR skip RHS? | C |
| Q085 | Does FALSE OR demand RHS? | C |
| Q086 | Does NULL OR demand RHS? | C |
| Q087 | Is UNKNOWN represented by BOOLEAN NULL? | C |
| Q088 | Is there a third Boolean payload state? | C |
| Q089 | Does NOT preserve UNKNOWN? | C |
| Q090 | Are AND/OR masks per logical occurrence? | C |
| Q091 | Can a skipped RHS error surface? | C |
| Q092 | Does CASE inspect WHENs in source order? | C |
| Q093 | Does CASE stop at first TRUE? | C |
| Q094 | Does FALSE CASE condition continue? | C |
| Q095 | Does UNKNOWN CASE condition continue? | C |
| Q096 | Is only selected THEN demanded? | C |
| Q097 | Is ELSE demanded only after no TRUE? | C |
| Q098 | Can an unselected CASE error surface? | C |
| Q099 | Does Chapter 25 explain CASE mask mechanics? | F |
| Q100 | Do nested demand masks compose? | C |
| Q101 | Is nested mask composition explicit locally? | F |
| Q102 | Is IN left expression evaluated once? | C |
| Q103 | Are IN-list items evaluated left-to-right? | C |
| Q104 | Does IN stop after TRUE? | C |
| Q105 | Must IN continue after UNKNOWN when needed? | C |
| Q106 | Is NOT IN ordinary 3VL NOT? | C |
| Q107 | Is IN-list execution locally described? | F |
| Q108 | Is branchless eager evaluation allowed to change errors? | C |
| Q109 | Can a zero-lane branch execute per-row work? | C |
| Q110 | Are zero-lane branch rules explicit locally? | F |

## NULL/type/arithmetic/casts — Q111–Q140

| ID | Question | Status |
|---|---|---|
| Q111 | Does validity alone define NULL? | C |
| Q112 | Is NULL payload semantically ignored? | C |
| Q113 | Can a NULL StringRef payload be dereferenced? | C |
| Q114 | Does ordinary arithmetic with NULL return typed NULL? | C |
| Q115 | Can garbage divisor payload under NULL raise division error? | C |
| Q116 | Does NULL comparison return UNKNOWN? | C |
| Q117 | Does NULL CAST remain typed NULL? | C |
| Q118 | Is empty string distinct from NULL? | C |
| Q119 | Is CONSTANT NULL repeated N times? | C |
| Q120 | Does batch code use resolved physical types? | C |
| Q121 | Can executor invent an implicit cast? | C |
| Q122 | Can executor redo overload resolution? | C |
| Q123 | Is INT32 arithmetic checked? | C |
| Q124 | Is INT64 arithmetic checked? | C |
| Q125 | Does integer division truncate toward zero? | C |
| Q126 | Does integer division by zero error? | C |
| Q127 | Does modulo zero error? | C |
| Q128 | Does MIN/-1 overflow error? | C |
| Q129 | Does unary minimum negation error? | C |
| Q130 | Does direct-negative literal provenance remain binder-owned? | C |
| Q131 | Is FLOAT64 division by zero non-error IEEE behavior? | C |
| Q132 | Is FLOAT64 rounding per operator boundary? | C |
| Q133 | Is FMA/contraction forbidden when observable? | C |
| Q134 | Are NaNs canonicalized as required? | C |
| Q135 | Is signed zero preserved? | C |
| Q136 | Are DATE/TIMESTAMP operations limited to registered forms? | C |
| Q137 | Does wrong runtime input TypeId remain internal? | C |
| Q138 | Is supported cast-value failure semantic rather than internal? | C |
| Q139 | Does §25.3 point to the canonical arithmetic owner accurately? | F |
| Q140 | Does runtime evaluation match constant folding? | C |

## Comparison/strings — Q141–Q160

| ID | Question | Status |
|---|---|---|
| Q141 | Do comparisons return nullable BOOLEAN? | C |
| Q142 | Are BOOLEAN order comparisons absent? | C |
| Q143 | Are numeric promotions pre-resolved? | C |
| Q144 | Are FLOAT64 comparisons canonical-total semantics? | C |
| Q145 | Do NaNs compare equal under SQL v1? | C |
| Q146 | Do signed zeros compare equal? | C |
| Q147 | Is VARCHAR equality exact-byte equality? | C |
| Q148 | Is VARCHAR ordering unsigned lexicographic? | C |
| Q149 | Is StringRef prefix only a rejection cache? | C |
| Q150 | Can equal prefix prove equality? | C |
| Q151 | Can prefix alone prove order? | C |
| Q152 | Is exact length considered? | C |
| Q153 | Are embedded NUL bytes preserved? | C |
| Q154 | Are C-string semantics forbidden? | C |
| Q155 | Does computed VARCHAR have a valid owner? | C |
| Q156 | Can a stack-local buffer back a returned StringRef? | C |
| Q157 | Is >UINT32_MAX exact representation composed correctly? | C |
| Q158 | Is truncation/wrap forbidden? | C |
| Q159 | Is allocation failure distinct from unsupported representation? | C |
| Q160 | Is the large-value/accounting handoff locally explicit? | F |

## Error candidates/selection — Q161–Q190

| ID | Question | Status |
|---|---|---|
| Q161 | Does an undemanded lane create a semantic error candidate? | C |
| Q162 | Does one demanded failing occurrence create a candidate? | C |
| Q163 | If two non-DML lanes fail, is the winner defined? | F |
| Q164 | Can first SIMD lane choose the error? | F |
| Q165 | Can first chunk choose the error? | F |
| Q166 | Can worker completion choose the error? | F |
| Q167 | Can dictionary child index choose the error? | F |
| Q168 | Can vector width change the public error? | F |
| Q169 | Can chunk boundary change the public error? | F |
| Q170 | If two Project outputs fail, is winner defined? | F |
| Q171 | If multiple Filter rows fail, is winner defined? | F |
| Q172 | Does Ch17 define within-tree child-error order? | C |
| Q173 | Does CASE suppress unselected errors? | C |
| Q174 | Does IN preserve item error order? | C |
| Q175 | Does scalar subquery have specialized precedence? | CS |
| Q176 | Does EXISTS suppress later-row errors? | CS |
| Q177 | Does IN-subquery build precede probing after left evaluation? | CS |
| Q178 | Does aggregate finalization have specialized error order? | CS |
| Q179 | Does D21 define DML candidate ranking? | CS |
| Q180 | Does Chapter 25 state candidate transport to D21? | F |
| Q181 | Is SourceSpan retained in bound expressions? | C |
| Q182 | Is SourceSpan preserved through logical rewrites? | C |
| Q183 | Is SourceSpan preservation explicit through physical expression state? | F |
| Q184 | Is expression phase known for D21 ranking? | C |
| Q185 | Is phase transport explicit in Chapter 25? | F |
| Q186 | Is exact diagnostic prose frozen? | NA |
| Q187 | Are categories and origins frozen where specified? | C |
| Q188 | Is resource failure an ordinary D21 candidate? | C |
| Q189 | Is cancellation an ordinary D21 candidate? | C |
| Q190 | Is non-DML ordinary candidate equivalence defined? | F |

## Output/error publication — Q191–Q210

| ID | Question | Status |
|---|---|---|
| Q191 | Can a failed Evaluate return a successful partial vector? | F |
| Q192 | Can stale active slots be consumed after failure? | F |
| Q193 | Must non-NULL active payload be initialized? | C |
| Q194 | Must active validity be initialized? | C |
| Q195 | Are inactive result slots irrelevant? | C |
| Q196 | Can prior successful SELECT chunks already be client-visible? | CS |
| Q197 | Does Chapter 25 own client cursor buffering? | NA |
| Q198 | Does DML RETURNING suppress failed prefixes? | CS |
| Q199 | Does expression evaluation mutate persistent state directly? | C |
| Q200 | Does expression failure choose transaction outcome? | C |
| Q201 | Does §39.1 own transaction effect? | C |
| Q202 | Are ordinary SELECT results bags unless ordered? | C |
| Q203 | Does Project preserve one occurrence per input? | C |
| Q204 | Does Filter alone remove rows? | C |
| Q205 | Can expression evaluation deduplicate equal rows? | C |
| Q206 | Can output-buffer address affect values? | C |
| Q207 | Can result reuse occur before borrowers finish? | C |
| Q208 | Does error-object backing outlive reporting? | C |
| Q209 | Is error-object lifetime explicit in Chapter 25? | F |
| Q210 | Can source text be released while numeric SourceSpan remains? | C |

## Subquery/aggregate/DML — Q211–Q230

| ID | Question | Status |
|---|---|---|
| Q211 | Is scalar subquery occurrence state query-local? | CS |
| Q212 | Is it initialized only on first demand? | CS |
| Q213 | Is it evaluated at most once per attempt? | CS |
| Q214 | Does retry discard subquery state? | CS |
| Q215 | Does scalar 0 rows yield typed NULL? | CS |
| Q216 | Does scalar second row cause cardinality failure? | CS |
| Q217 | Does EXISTS avoid projection-only errors? | CS |
| Q218 | Does IN consume the full final build? | CS |
| Q219 | Are correlated subqueries executable? | NA |
| Q220 | Are aggregate functions scalar kernels? | NA |
| Q221 | Are aggregate argument vectors evaluated by Chapter 25? | CS |
| Q222 | Does §29.3 own reduction semantics? | CS |
| Q223 | Can vector size alter aggregate reduction semantics? | C |
| Q224 | Can representation alter aggregate reduction semantics? | C |
| Q225 | Do DML assignments use generic scalar execution? | C |
| Q226 | Does D21 own final DML candidate ranking? | CS |
| Q227 | Does RETURNING preserve its unordered bag? | C |
| Q228 | Can mutation order rank RETURNING errors? | C |
| Q229 | Can expression evaluation publish a DML write itself? | C |
| Q230 | Does retry clear stale expression scratch/errors? | C |

## Ownership/resources/invalid states — Q231–Q250

| ID | Question | Status |
|---|---|---|
| Q231 | Is expression state query-owned memory? | C |
| Q232 | Is unbounded expression scratch accounted? | C |
| Q233 | Are owned vector capacities accounted? | C |
| Q234 | Is StringHeap capacity accounted? | C |
| Q235 | Can QueryArena hold unbounded row-dependent expression output? | C |
| Q236 | Is allocator rounding fully charged? | C |
| Q237 | Is size arithmetic exact-before-use? | C |
| Q238 | Does unsupported address extent yield ExecutionError? | C |
| Q239 | Does supported allocation denial yield OOM? | C |
| Q240 | Does cancellation remain QueryCancelled? | C |
| Q241 | Is universal semantic-error-over-OOM precedence defined? | NA |
| Q242 | Can successful memory paths differ semantically? | C |
| Q243 | Is missing kernel in a validated plan internal? | C |
| Q244 | Is wrong child count internal? | C |
| Q245 | Is wrong slot mapping internal? | C |
| Q246 | Is stale borrowed state internal? | C |
| Q247 | Is out-of-domain selection internal? | C |
| Q248 | Are these invalid-state checks explicit in Chapter 25? | F |
| Q249 | Can malformed state cause OOB access? | C |
| Q250 | Is one universal validator required? | C |

## Determinism/document ownership — Q251–Q260

| ID | Question | Status |
|---|---|---|
| Q251 | Can allocation address alter scalar output? | C |
| Q252 | Can pointer identity alter VARCHAR comparison? | C |
| Q253 | Can SIMD reassociate scalar FLOAT operations? | C |
| Q254 | Can lane completion select non-DML error? | F |
| Q255 | Can resource policy alter successful semantics? | C |
| Q256 | Is Chapter 25 free of project chronology? | C |
| Q257 | Is it free of Development sequencing? | C |
| Q258 | Is it free of Verification procedures? | C |
| Q259 | Are explicit cross-references owner-correct? | F |
| Q260 | Can Chapter 25 be implemented without inventing non-DML error precedence? | F |

Totals:

| Status | Count |
|---|---:|
| CONSISTENT | 199 |
| CONSISTENT BUT SPECIALIZED | 19 |
| FINDING | 34 |
| N/A | 8 |
| Total | 260 |

The FINDING count here is question-level evidence, not the count of independently consolidated architecture findings.

# Implementer-invention assessment

An implementer can derive scalar values, NULL behavior, child order, conditional demand, vector legality, lifetime, resource categories, and DML precedence from frozen owners.

An implementer **cannot** derive one canonical public ordinary non-DML error when multiple demanded expression occurrences fail. Physical lane, chunk, output-expression order, and worker schedule therefore remain potential accidental policy. Correctness-relevant invention is required.

# Previous-chapter regression

| Owner | Result |
|---|---|
| Chapter 17 scalar semantics | No contradiction |
| Chapter 18 syntax/provenance | No re-parse; physical provenance handoff incomplete |
| Chapter 19 binding/function identity | No runtime resolution; scalar registry remains empty |
| Chapter 20 demand/order | AND/OR and side-plan behavior compatible; generic integration incomplete |
| Chapter 21 DML precedence | No contradictory rule; candidate/provenance handoff incomplete |
| Chapter 22 slot/runtime ownership | State split compatible; slot mapping implicit |
| Chapter 23 vector/borrow/StringRef | No contradiction |
| Chapter 24 resource policy | No contradiction; local navigation incomplete |

Chapter 24 remains closed.

# Chapter 26 boundary

- Heading: `# 26. Pipeline Execution Model`
- Start: line 20360
- Immediate handoff:
  - Chapter 25 returns exact result vectors or borrowed views.
  - Chapter 26 schedules Source → streaming operators → Sink.
  - Chapter 26 preserves synchronous borrow lifetimes, dormant side-plan demand, cancellation, and pipeline finalization.
- Explicit Chapter-26 references inside Chapter 25: none by section number.
- Informal reference: §25.7 “pipeline lifetime rules,” which should point to §26.6 and §§23.10–23.13.

Recommended future Chapter-26 review scope, not performed: pipeline DAG dependencies, dormant subquery scheduling, source statuses/empty-batch progress, global/local state, borrowing, cancellation, and early stop.

# Direct answers 126–149

| Question | Answer |
|---|---|
| Any project chronology? | NO |
| Current implementation narration? | NO |
| DEVELOPMENT-owned material? | NO |
| VERIFICATION procedure? | NO |
| PROJECT_STATE leakage? | NO |
| History/devlog leakage? | NO |
| Demanded-evaluation ambiguity? | No upstream semantic ambiguity; Chapter-25 integration gap exists |
| Child-order ambiguity? | NO globally; local restatement gap |
| AND/OR ambiguity? | NO |
| CASE ambiguity? | NO globally; execution-mask documentation gap |
| Zero-row-expression ambiguity? | NO |
| Erroring-constant ambiguity? | NO |
| Vector-lane error ambiguity? | **YES — Q25-1** |
| Cross-expression error ambiguity? | **YES — Q25-1** |
| SELECT error-precedence ambiguity? | **YES — Q25-1** |
| D21-S4 handoff ambiguity? | Outcome NO; transport/provenance documentation YES |
| SourceSpan/provenance ambiguity? | Semantics frozen; physical handoff incomplete |
| Input/output aliasing ambiguity? | NO; Ch23 governs |
| Computed-string ownership ambiguity? | NO; local navigation gap |
| Function-dispatch ambiguity? | NO; named scalar registry is empty |
| Runtime resource/error ambiguity? | NO |
| Width/chunk-boundary semantic ambiguity? | Values NO; selected non-DML error YES |
| Correctness-relevant implementer invention? | **YES** |
| Can Chapter 25 stand years later as canonical v1 Architecture? | **Not yet**; Q25-1 must be frozen and integration gaps repaired |

# Recommended next action

**FROZEN CHAPTER-25 SEMANTIC REVIEW / DECISION PACKAGE** for Q25-1:

```text
ordinary non-DML runtime expression-error candidate identity,
candidate domain,
deterministic winner/equivalence,
chunk/vector/worker independence,
and already-emitted SELECT result boundary.
```

Do not perform Chapter-25 document cleanup or Verification synchronization before that decision.

- Chapter 26 review: **NOT STARTED**
- Verification synchronization: **NOT PERFORMED**
- Implementation: **NONE**
- Build/tests/sanitizers/benchmarks: **NONE**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
