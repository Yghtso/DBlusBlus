# Chapter 20 review verdict

**CHAPTER 20 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 20 is substantially developed, especially for subqueries, grouping equivalence, NULL ordering, exact-emptiness proofs, and optimizer boundaries. It is not clean because two rewrite rules can conflict with frozen scalar evaluation/error semantics, and several logical-identity and relational contracts remain incomplete.

- BLOCKING: **2**
- MAJOR: **6**
- MINOR: **6**
- EDITORIAL: **0**

No files were modified.

## Scope and boundaries

- Primary scope: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16261)
- Exact title: **20. Logical Plans, Properties, and Rewrites**
- Start: line 16261
- End: line 17432, immediately before Chapter 21
- Chapter 21 boundary: line 17433, **21. DDL/DML Semantic Planning and SQL v1 Scope**
- Chapter 19 → 20 handoff: immutable, fully bound statements and expressions, including `BindingId`, resolved types/casts, output metadata, aggregate classification/ordinal, resolved ORDER identities, and folded/residual LIMIT/OFFSET expressions.
- Chapter 20 → downstream handoff:
  - Chapter 21: DDL/DML publication and mutation semantics.
  - Chapters 22–31: physical planning and execution.
  - Chapter 33: optimizer search and physical selection.
  - Chapter 35: exact semantic proofs versus estimates.
  - Chapter 29: aggregate descriptor/value semantics.
  - §39: runtime errors and transaction consequences.

Context consulted without reviewing as new scope: Chapters 17–19, selected Chapter 21 boundary material, Chapters 22, 25, 27–30, 33, 35, §§39 and 41, plus relevant portions of `docs/VERIFICATION.md`.

## Findings

### BLOCKING

#### F20-B1 — Ordinary relational rewrites lack an error-demand contract

- Sections: §§20.6–20.7, 20.17.3–20.17.4, 20.17.8–20.17.10
- Type: **REWRITE SEMANTICS**
- Evidence:
  - Predicate pushdown is authorized when referenced slots and join semantics permit it.
  - Projection pruning removes values not listed as ancestor requirements.
  - The rules do not define when ordinary row-dependent expressions are semantically demanded or require preservation of their errors.
- Frozen comparison: §§17.6 and 17.7.3 make child evaluation order, short-circuiting, and raised errors observable.
- Concrete consequence:
  - A left-local erroring predicate above an INNER JOIN may not be evaluated for a left row that has no matching right row.
  - Pushing it below the join can evaluate that row and raise an error.
  - Pruning an unused derived projection can similarly suppress an error unless ordinary projection demand is defined.
- Correct owner: Chapter 20.
- Smallest decision required: define ordinary relational expression demand and require every rewrite to preserve demanded evaluations, skipped evaluations, error category, responsible `SourceSpan`, and error precedence.

#### F20-B2 — Commutative operand canonicalization conflicts with left-to-right scalar errors

- Section: §20.17.5
- Type: **ERROR SEMANTICS**
- Evidence: “canonical operand ordering for truly commutative operators” is allowed, while preservation mentions NULL, FLOAT64, volatility, and evaluation count—but not evaluation order or errors.
- Frozen comparison: §17.6 requires left-to-right scalar-child evaluation and preserves errors raised by children.
- Consequence: reordering two error-capable operands can change which error and `SourceSpan` is observed.
- Correct owner: Chapter 20 composing with Chapter 17.
- Smallest decision required: either forbid operand reordering whenever child evaluation can observably differ, or define exact proof conditions under which both operand evaluations are total and error-free.

### MAJOR

#### F20-M1 — `LogicalSlotId` domain and reuse are incomplete

- Section: §20.2
- Type: **SLOT IDENTITY**
- Evidence: “query-local” is not defined as query-block-local or top-level-statement-local; direct pass-through uses `SHOULD`; duplicate projection outputs and reuse are not resolved.
- Missing:
  - uniqueness domain across nested blocks;
  - nonreuse lifetime;
  - behavior for `SELECT a, a`;
  - derived-boundary allocation;
  - persistence/non-persistence;
  - mandatory rewrite stability.
- Consequence: conforming implementations can assign materially different logical identities and derived slot maps.

#### F20-M2 — Core bag/multiplicity semantics are not stated

- Sections: §§20.3–20.7
- Type: **BAG SEMANTICS**
- Missing exact contracts for:
  - `LogicalGet` row multiplicity;
  - duplicate `LogicalValues` rows;
  - one output per retained `LogicalFilter` row;
  - one `LogicalProject` row per input row;
  - duplicate preservation absent explicit `LogicalDistinct`.
- DISTINCT implies bag input but does not establish a chapter-wide default.

#### F20-M3 — Join multiplicity and output schema order are incomplete

- Sections: §§20.8.1–20.8.4
- Type: **JOIN SEMANTICS**
- Defined: supported join kinds, CROSS condition absence, LEFT null extension, ON/WHERE distinction.
- Missing:
  - every-TRUE-pair multiplicity;
  - duplicate-match preservation;
  - exact left-then-right output order;
  - CROSS Cartesian multiplicity and empty-side behavior;
  - LEFT one-null-extended-row rule when no TRUE match.
- Consequence: row multiset and slot ordering require implementer inference.

#### F20-M4 — `LogicalLimit` does not locally define its relational contract

- Section: §20.12
- Type: **LIMIT SEMANTICS**
- Missing locally:
  - OFFSET before LIMIT;
  - exact prefix/cardinality rule;
  - order preservation;
  - zero and over-cardinality counts.
- §§27.9 and 35.20 supply compatible downstream rules, and §20.14.10 resolves unordered input, but Chapter 20’s canonical node owner does not cite or state the full rule.

#### F20-M5 — Aggregate ordinal handoff is absent

- Sections: §§20.9, 20.15, 20.20
- Type: **LOGICAL IDENTITY**
- Chapter 19 supplies a canonical per-query-block aggregate ordinal, but Chapter 20 does not say how it is carried into `LogicalAggregate`, whether rewrites preserve it, or whether equivalent aggregate expressions may be merged.
- Consequence: optimizer traversal or deduplication can replace the frozen aggregate-occurrence identity.

#### F20-M6 — Runtime diagnostic provenance through rewrites is incomplete

- Sections: §§20.1–20.2, 20.17.5, 20.19
- Type: **SOURCE SPAN**
- Evidence: source/display metadata “may” be retained for readable EXPLAIN, but runtime scalar errors depend on canonical expression provenance.
- Missing:
  - mandatory preservation/remapping of `SourceSpan`;
  - provenance for synthesized predicates;
  - error responsibility after canonicalization.
- Consequence: logically equivalent rewrites can report different or missing diagnostic spans.

### MINOR

| ID | Section | Type | Finding |
|---|---|---|---|
| F20-N1 | §§20.3, 20.11, 20.14.2, 20.17.1, 20.17.9 | TEMPORALITY | “Initial family,” “added later,” “future architecture/functions,” and “remain deferred” use roadmap framing. |
| F20-N2 | §20.8.2 | DOCUMENT OWNERSHIP | Repeats Chapter-19 binder sequencing in the logical-plan chapter. |
| F20-N3 | §20.14.7 | IMPLEMENTATION COUPLING | Physical side-plan roles, pipeline graph, arena, spill, vectorization, and byte ownership substantially duplicate physical/execution chapters. |
| F20-N4 | §§20.5, 20.15 | CROSS-REFERENCE | Says §18.11.1 owns no-FROM observable semantics, while §18.11.1 explicitly delegates them to §§20.5/20.15. |
| F20-N5 | §§20.11–20.12 | CROSS-REFERENCE | Omits precise references to §30.1/§30.3 for ties/comparators and §§27.9/35.20 for LIMIT cardinality/composition. |
| F20-N6 | §20.19 | CURRENT-STATE LEAKAGE | “Before physical planning exists” narrates implementation state rather than defining timeless logical EXPLAIN semantics. |

## Section-by-section review matrix

Codes: `T` timeless, `TP` temporal issue; `A` architecture-owned, `O` ownership issue, `L` leakage; `S` sufficient, `P` partial; `C` consistent, `F` finding; `—` not central.

| Section | Exact heading | Role | Time | Owner | Depth | Term | Identity | Bag | Order | NULL | Card. | Errors | Determ. | X-ref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 20.1 | Logical-plan node contract | Logical/physical boundary | T | A | S | C | P | P | P | — | P | P | C | C | C | M2/M6 |
| 20.2 | Logical output schema and `LogicalSlotId` | Slot identity/schema | T | A | P | C | F | — | C | C | — | P | P | C | P | M1/M6 |
| 20.3 | Initial logical operator family | Node inventory | TP | A | P | C | P | P | P | P | P | — | P | C | P | N1/M2 |
| 20.4 | `LogicalGet` | Base relation | T | A | P | C | C | F | P | C | P | P | P | C | P | M2 |
| 20.5 | `LogicalValues` | Literal/no-FROM relation | T | A | S | C | C | P | P | C | C | P | C | F | C | N4 |
| 20.6 | `LogicalFilter` | Row retention | T | A | P | C | C | F | P | P | P | F | F | C | F | B1/M2 |
| 20.7 | `LogicalProject` | Output formation | T | A | P | C | F | F | C | C | P | F | F | C | F | B1/M1/M2 |
| 20.8 | Logical joins | Join family | T | A | P | C | P | F | P | P | F | P | P | C | P | M3 |
| 20.8.1 | Join node | Join shape | T | A | P | C | P | F | P | P | F | P | P | C | P | M3 |
| 20.8.2 | Binding scope | Binder scope recap | T | O | S | C | C | — | — | — | — | C | C | P | C | N2 |
| 20.8.3 | LEFT JOIN null extension | Outer nullability | T | A | S | C | C | P | P | C | P | C | C | C | C | M3 |
| 20.8.4 | Predicate decomposition | Join metadata | T | A | P | C | C | P | P | C | P | F | P | C | P | B1 |
| 20.9 | `LogicalAggregate` and grouping equality | Group relation | T | A | P | C | F | C | P | C | C | P | C | C | P | M5 |
| 20.10 | `LogicalDistinct` | Duplicate elimination | T | A | S | C | C | C | C | C | C | P | C | C | C | Clean |
| 20.11 | `LogicalSort` | Semantic ordering | TP | A | S | C | C | C | C | C | C | P | C | P | C | N1/N5 |
| 20.12 | `LogicalLimit` | Prefix selection | T | A | P | C | C | C | P | C | F | P | P | P | P | M4/N5 |
| 20.13 | DML logical nodes and hidden system slots | DML handoff | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.13.1 | Hidden system values | Internal slots | T | A | S | C | C | — | — | — | — | C | C | C | C | Clean |
| 20.13.2 | `LogicalInsert` | Insert input | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.13.3 | `LogicalUpdate` | Update target stream | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.13.4 | `LogicalDelete` | Delete target stream | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.13.5 | `LogicalAnalyze` | Statistics statement | T | A | S | C | C | — | — | — | C | C | C | C | C | Clean |
| 20.14 | V1 subquery semantics and support | Closed subquery contract | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.1 | Closed support matrix | Supported forms | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.2 | Query blocks, scopes, and correlation rejection | Scope boundary | TP | A | S | C | C | — | — | — | C | C | C | C | C | N1 |
| 20.14.3 | Derived tables | Relational boundary | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.4 | Scalar subquery semantics | Scalar cardinality | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.5 | EXISTS and NOT EXISTS | Existence semantics | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.6 | IN and NOT IN | Membership 3VL | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.7 | Canonical logical and physical fallback | Physical fallback | T | L | S | C | C | C | C | C | C | C | C | C | C | N3 |
| 20.14.8 | Lazy statement-attempt ownership, snapshot, and CommandId | Evaluation lifetime | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.9 | Legal expression/statement contexts | Context composition | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.10 | Aggregation, DISTINCT, ORDER BY, LIMIT, and OFFSET | Clause composition | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.11 | Rewrite and proof boundary | Subquery rewrites | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.12 | Error precedence, EXPLAIN, and persistence | Subquery errors/state | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.14.13 | Forbidden subquery implementations | Negative contract | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.15 | Canonical SELECT logical shape | Clause pipeline | T | A | S | C | P | C | C | C | C | P | C | F | C | N4/M5 |
| 20.16 | Logical properties | Proof/property boundary | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.17 | Logical rewrites | Rewrite phase | T | A | P | C | P | P | P | P | P | F | F | C | F | B1/B2 |
| 20.17.1 | Constant folding | Chapter-17 folding composition | TP | A | S | C | C | — | — | C | — | C | C | C | C | N1 |
| 20.17.2 | Boolean simplification | 3VL rewrites | T | A | S | C | C | C | C | C | C | C | C | C | C | Clean |
| 20.17.3 | Predicate pushdown | Filter rewrite | T | A | P | C | C | C | C | C | C | F | F | C | F | B1 |
| 20.17.4 | Projection pruning | Column rewrite | T | A | P | C | F | C | C | C | C | F | F | C | F | B1 |
| 20.17.5 | Expression canonicalization | Scalar normalization | T | A | P | C | C | — | — | C | — | F | F | C | F | B2/M6 |
| 20.17.6 | Join graph extraction | Optimizer metadata | T | A | S | C | C | C | C | C | C | P | C | C | C | Clean |
| 20.17.7 | Inner-join equality equivalence classes | Equality inference | T | A | S | C | C | C | C | C | C | P | C | C | C | Clean |
| 20.17.8 | Constant propagation and contradiction detection | Exact inference | T | A | P | C | C | C | C | C | C | F | P | C | P | B1 |
| 20.17.9 | Trusted key metadata | Constraint proofs | TP | A | S | C | C | C | C | C | C | C | C | C | C | N1 |
| 20.17.10 | Semantic-emptiness propagation | Exact empty proofs | T | A | S | C | C | C | C | C | C | P | C | C | P | B1 |
| 20.18 | Logical-plan validation | Structural validation | T | A | S | C | P | C | C | C | C | P | C | C | P | M1/M5/M6 |
| 20.19 | Logical EXPLAIN | Logical presentation | TP | A | S | C | C | — | C | C | — | — | C | C | C | N6 |
| 20.20 | Logical-plan/rewrite invariants | Consolidated invariants | T | A | P | C | P | P | C | C | C | P | P | C | P | B1/M1/M2/M5/M6 |

## Canonical owner map

| Mechanism | Owner classification | Canonical owner |
|---|---|---|
| Scalar values, casts, 3VL, comparator behavior, evaluation order | Earlier owner | Chapter 17 |
| Raw syntax, source order, `SourceSpan` | Earlier owner | Chapter 18 |
| Names, `BindingId`, bound expressions, aliases, grouping legality, ORDER resolution, LIMIT admissibility | Earlier owner | Chapter 19 |
| Logical trees and schemas | Chapter 20 owns | §§20.1–20.3 |
| `LogicalSlotId` | Chapter 20 owns, incomplete | §20.2 |
| Scan/Values/Filter/Project/Join/Aggregate/Distinct/Sort/Limit | Chapter 20 owns | §§20.4–20.12 |
| No-FROM relational source | Chapter 20 owns; circular citation | §§20.5/20.15 |
| Runtime grouping/DISTINCT equivalence | Chapter 20 owns | §§20.9–20.10 |
| Subquery relational/cardinality semantics | Chapter 20 owns | §20.14 |
| DML logical streams and hidden RID | Chapter 20 owns | §20.13 |
| Mutation publication and row images | Later owner | Chapters 15 and 21 |
| Aggregate descriptor/value semantics | Later/reference owner | Chapter 29 |
| Physical operators and vector execution | Later owner | Chapters 22–31 |
| Optimizer search and alternatives | Later owner | Chapter 33 |
| Exact proof versus estimate | Later/reference owner | §§34–35 |
| Error consequences | Referenced owner | §39 |
| Verification methodology | Referenced only | §41 / `VERIFICATION.md` |

## Logical identity assessment

| Identity | Domain | Persistence | Rewrite rule | Assessment |
|---|---|---|---|---|
| `TableId` | Catalog object | Persistent | Stable descriptor identity | Clear |
| `ColumnId` | Catalog column | Persistent | Stable catalog identity | Clear |
| `BindingId` | Relation occurrence in top-level bound statement | Runtime-only | Surviving occurrence preserves it | Frozen in Chapter 19 |
| `LogicalSlotId` | “query-local” semantic value | Runtime-only implied | Pass-through `SHOULD` preserve | **Ambiguous** |
| Aggregate ordinal | Query-block aggregate occurrence | Runtime-only | Not stated in Chapter 20 | **Missing handoff** |
| Output ordinal | Ordered result position | Runtime-only | Projection order carries it | Mostly clear |
| Subquery occurrence identity | Bound occurrence per statement attempt | Runtime-only | Independently cached | Clear |
| Heap `SlotId` | Physical heap slot | Persistent-format identity | Unrelated | Clear |
| `SourceSpan` | Source diagnostic provenance | Runtime-only | Rewrite behavior unspecified | **Ambiguous** |

Self-joins remain distinguishable at `LogicalGet` because it carries both `TableId` and `BindingId`. Collapse to `TableId + ColumnId` is prohibited in substance, but the exact base-column-to-`LogicalSlotId` allocation map should be explicit.

## Relational semantics matrices

### Relational-node matrix

| Node | Inputs | Output/cardinality | Bag behavior | Order | NULL/errors |
|---|---|---|---|---|---|
| Get | Base descriptor | Visible logical rows | Not explicit | No semantic access order | Catalog nullability |
| Values | Bound rows | Listed rows; no-FROM has one zero-column row | Duplicate behavior implicit | Source order not explicitly contractual | Expression errors |
| Filter | One child | Retains TRUE rows | One-per-retained-row implicit | Preservation unspecified | FALSE/UNKNOWN rejection established in §20.17.10 |
| Project | One child | Ordered expressions | One-per-input implicit | Output columns ordered; row order unspecified | Evaluation demand unclear |
| INNER Join | Two children | Matching pairs | Exact multiplicity unstated | Unspecified | TRUE-only matching implied |
| LEFT Join | Two children | Matches plus null extension | Exact multiplicity unstated | Unspecified | Right slots nullable |
| CROSS Join | Two children | Cartesian relation | Exact formula unstated | Unspecified | No ON |
| Aggregate | One child | One row/group | Groups duplicates | Unspecified | Dedicated grouping equivalence |
| Distinct | One child | One representative/class | Collapses duplicates | No preservation guarantee | Dedicated equivalence |
| Sort | One child | Same rows | Preserves multiplicity | Establishes key order | ASC NULL-first; DESC NULL-last |
| Limit | One child | Prefix after offset | Preserves selected multiplicity | Inherits semantic order if present | Final count errors remain Chapter 19 |
| SubqueryScan | Child query | Child relation with remapped boundary slots | Preserves rows | Advertises no outer ordering | Child output nullability |
| Insert/Update/Delete | Relational/DML child | Statement-specific | Mutation owner downstream | Not user-ordering owner | Hidden RID preserved |
| Analyze | Resolved target | No ordinary relational output | N/A | N/A | Publication downstream |

### Join matrix

| Join | Pairing | Predicate | Unmatched | Null extension | Schema/order | Empty sides |
|---|---|---|---|---|---|---|
| INNER | Intended all matching pairs; not stated exactly | TRUE matches; FALSE/UNKNOWN reject | None | None | Exact left/right slot order missing | Either empty implies empty via §20.17.10 |
| LEFT | Intended all matches, else one extended row; multiplicity not stated exactly | TRUE matches | Left row preserved when no TRUE match | All right outputs nullable | Exact slot order missing | Empty left empty; empty right preserves left |
| CROSS | Intended Cartesian product; formula not stated | None | N/A | None | Exact slot order missing | Either empty implies empty |

### Duplicate/bag matrix

| Operator | Duplicate rule | Status |
|---|---|---|
| Get | Should expose base-row multiplicity | Missing explicit rule |
| Values | Should retain listed duplicate rows | Missing explicit rule |
| Filter | Should preserve multiplicity of retained rows | Implicit |
| Project | Should preserve row multiplicity even for duplicate values | Implicit |
| INNER/LEFT/CROSS | Should preserve pair multiplicity | Incomplete |
| Aggregate | Produces one row per runtime group | Clear |
| Distinct | One row per grouping-equivalence class | Clear |
| Sort | Same multiset | Clear by role, not explicit |
| Limit | Selected prefix multiplicity | Downstream-composed |
| Derived table | Child duplicates retained absent DISTINCT | Clear in context |
| IN build | May deduplicate because multiplicity is truth-irrelevant | Explicit |
| Scalar subquery | DISTINCT only when requested | Explicit |

### Order-guarantee matrix

| Construct | Semantic order |
|---|---|
| Get | None; no physical scan-order contract |
| Values | Row order not explicitly frozen |
| Filter/Project | Preservation not explicitly stated |
| Join | None stated |
| Aggregate/Distinct | None |
| Sort | Establishes lexicographic key order |
| Equal sort keys | Unspecified relative order via §30.1 |
| ASC NULL | First |
| DESC NULL | Last |
| NaN | Chapter-17/§30.3 total comparator |
| Limit with ORDER | Selects ordered prefix |
| Limit without ORDER | Any rows allowed by unordered child; explicit in §20.14.10 |
| Derived child ORDER | Not advertised outside boundary |
| Derived ORDER+LIMIT | ORDER determines child selection; outer order still unadvertised |
| Top-level without ORDER | Unordered relational result |

### NULLability matrix

| Construct | Rule |
|---|---|
| Base output | Descriptor nullability |
| Project | Expression nullability |
| Filter | Same schema as child |
| INNER/CROSS | Child nullability retained |
| LEFT right side | Forced nullable |
| Aggregate | Chapter-29 result nullability |
| Scalar subquery | Always nullable because zero rows → typed NULL |
| EXISTS/NOT EXISTS | Non-null BOOLEAN |
| IN/NOT IN | Nullable BOOLEAN |
| Derived output | Child output nullability remapped |
| NULL grouping/DISTINCT | One equivalence class |
| Filter/HAVING/ON UNKNOWN | Rejected |
| ORDER NULL | ASC first; DESC last |

### Empty-input matrix

| Operation | Empty input result |
|---|---|
| Project | Empty |
| Filter | Empty |
| INNER/CROSS | Empty if either required child empty |
| LEFT JOIN | Empty only if left empty; empty right null-extends each left row |
| Distinct | Empty |
| Explicit GROUP BY | Zero groups |
| Global aggregate | Exactly one row |
| HAVING | Retains global/group row only when TRUE |
| Sort | Empty |
| Limit/Offset | Empty |
| Scalar subquery | Typed NULL |
| EXISTS | FALSE |
| NOT EXISTS | TRUE |
| IN | FALSE |
| NOT IN | TRUE |
| No-FROM source | Not empty: exactly one zero-column row |

### DISTINCT matrix

| Values | Duplicate-equivalent? | Retention/order |
|---|---|---|
| Equal non-NULL scalar | Yes | One representative; no order guarantee |
| NULL/NULL | Yes | One |
| Canonical NaN/NaN | Yes | One |
| +0.0/-0.0 | Yes | One |
| Byte-equal VARCHAR | Yes | One |
| Byte-distinct VARCHAR | No | Both |
| Multi-column row | Per-column grouping equivalence | One per complete class |

### Grouping matrix

| Case | Groups/output |
|---|---|
| Explicit keys, nonempty | One group per grouping-equivalence class |
| Explicit keys, empty | Zero |
| No keys, aggregate query, nonempty | One global group |
| No keys, empty | One global group |
| NULL key rows | Same group |
| Canonical NaNs | Same group |
| +0/-0 | Same group |
| HAVING TRUE | Retain group |
| HAVING FALSE/UNKNOWN | Remove group |
| Aggregate value | Chapter 29 |
| Binder structural equality | Legality only; distinct from runtime grouping |

### ORDER BY matrix

| Case | Comparator/order | Tie |
|---|---|---|
| ASC | Chapter-17 SQL order | Unspecified |
| DESC | Reverse canonical order | Unspecified |
| ASC NULL | NULLS FIRST | NULL ties unspecified |
| DESC NULL | NULLS LAST | NULL ties unspecified |
| NaN | Canonical total FLOAT order | Equivalent NaNs tie |
| Multiple keys | Lexicographic key sequence | Unspecified after all keys tie |
| No ORDER BY | Unordered result | Any sequence equivalent |
| DISTINCT + ORDER | DISTINCT then Sort | Keys bind to final logical identities |
| Group + ORDER | Sort final group rows | Aggregate/group keys already bound |

### LIMIT/OFFSET matrix

| Case | Semantics | Assessment |
|---|---|---|
| LIMIT 0 | Empty result | Clear through exact-emptiness rule |
| LIMIT N | At most N rows | Downstream owner |
| OFFSET 0 | No rows skipped | Downstream owner |
| OFFSET N | Skip first N semantic input rows | Downstream owner |
| LIMIT+OFFSET | OFFSET then LIMIT | Defined in §27.9, absent locally |
| Count > cardinality | Remaining/all available rows | Downstream-composed |
| Offset ≥ cardinality | Empty | Downstream-composed |
| Ordered input | Ordered suffix prefix | Clear composition |
| Unordered input | Any allowed unordered-child selection | Explicit |
| Empty input | Empty | Clear |
| Count validation | Execution-start Chapter-19 contract | Correctly not redefined |

### Subquery matrix

| Form | Zero | One | More | Order/error |
|---|---|---|---|---|
| Scalar | Typed NULL | Selected scalar | Second completed row → `CardinalityViolation` | Final rows after all child clauses |
| EXISTS | FALSE | TRUE | Later rows not demanded | Projection values irrelevant |
| NOT EXISTS | TRUE | FALSE | Same demand | Ordinary NOT |
| IN | FALSE | Repeated-equality 3VL | Full final build | Build errors precede probe result |
| NOT IN | TRUE | NOT of IN | Full final build | NULL-aware |
| Derived | Empty relation | Ordinary row | Ordinary rows | No outer ordering advertised |
| Correlated | Bind-time rejection | — | — | No executable form |

### Expression-evaluation matrix

| Context | Required rows | May skip? | Ambiguity |
|---|---|---|---|
| Projection | Surviving input rows assumed | EXISTS projection explicitly skip | Ordinary unused projection demand undefined |
| Filter | Child rows assumed | Empty child | Pushdown can change demanded rows |
| JOIN ON | Candidate pairs | Noncandidate pairs | Exact pair iteration unspecified |
| Group key | Aggregate input rows | Empty input | Clear relational need |
| Aggregate argument | Input rows participating in aggregate | Chapter-29 rules | Ordinal preservation missing |
| HAVING | Produced groups | No group | TRUE-only retention clear |
| ORDER key | Rows reaching Sort | EXISTS may remove irrelevant ORDER | General rewrite error preservation incomplete |
| Scalar subquery projection | First and second final rows | Later rows after second | Exact |
| EXISTS projection | Never value-demanded solely for existence | Yes | Exact |
| IN child output | Entire final child build | No after demand | Exact |
| LIMIT count | Once at execution start | Original folded work not repeated | Chapter 19 exact |

### Error matrix

| Condition | Category | Logical detection | Owner/status |
|---|---|---|---|
| Scalar subquery second row | `CardinalityViolation` / `CardinalityError` | Second completed final row | Clear |
| Scalar child expression error | Chapter-17 category | When demanded | Clear in subquery; ordinary rewrite demand incomplete |
| EXISTS demanded relational error | Lower-layer category | Until existence known | Clear |
| IN build error | Scalar/resource category | Complete lazy build | Clear |
| Unsupported correlation | Bind-time unsupported correlation | Chapter 19 | Clear |
| Malformed logical plan | Internal architecture error | Validation | Clear |
| Spill/OOM | §39 resource error | Physical execution | Correctly downstream |
| Rewrite-changed child error | Existing scalar error, but different demand/span | Rewrite phase consequence | **Blocking** |
| Reordered operand errors | Existing scalar error | Rewritten expression | **Blocking** |

### Rewrite-safety matrix

| Rewrite | NULL | Duplicates | Order | Errors | Outer join | Identity | Status |
|---|---|---|---|---|---|---|---|
| Constant folding | Safe by §17.10.2 | N/A | N/A | Explicit | N/A | Provenance unclear | Partial |
| Boolean identities | 3VL-safe | N/A | N/A | Conservative | N/A | Clear | Good |
| Predicate pushdown | Conditions stated | Intended | Intended | **Incomplete** | Guarded | Slots considered | BLOCKING |
| Projection pruning | N/A | Intended | Intended | **Incomplete** | N/A | Retained slots | BLOCKING |
| Operand canonicalization | Claimed | N/A | N/A | **Conflicts with §17.6** | N/A | Provenance unclear | BLOCKING |
| Join graph extraction | Intended | Intended | No result-order promise | Not explicit | Guarded | BindingIds retained | Partial |
| Equality propagation | NULL constraints stated | Intended | N/A | Demand incomplete | Guarded | Slots | Partial |
| Contradiction → empty | 3VL exact | Safe if proof exact | N/A | Other demanded errors not fully addressed | Guarded | Schema retained | Partial |
| Exact-empty propagation | Strong operator rules | Strong | Strong | General ordinary-expression demand incomplete | Strong | Schema retained | Partial |
| Subquery semi/anti/marker | Strong | Strong | Strong | Strong | Conditions exact | Occurrence semantics retained | Good |

## Optimizer and execution boundaries

- Logical/physical distinction is generally strong.
- Logical nodes do not freeze scan, join, sort, or aggregation algorithms.
- Statistics are not permitted to act as semantic proof.
- `required_rows` is correctly separated from SQL LIMIT and scalar cardinality.
- Chapter 33 owns cost-based selection.
- Chapters 22–31 own physical algorithms.
- §20.14.7 is the notable leakage: it specifies physical role names, pipeline state, memory/spill machinery, vectorized probing, ownership of cached bytes, and execution-context placement. The semantic fallback requirement belongs in Chapter 20; most mechanics belong downstream.

## Cross-chapter matrix

| Owner | Input/handoff | Chapter-20 responsibility | Downstream | Consistency |
|---|---|---|---|---|
| Ch17 | Typed scalar expressions, 3VL, comparison, errors | Relational demand/composition | Ch25+ | Conflict in §20.17.5; incomplete ordinary demand |
| Ch18 | Raw provenance and no-FROM syntax | Logical no-FROM relation | Ch27 | Circular no-FROM citation |
| Ch19 | Fully bound query, identities, ordinals, count form | Canonical logical semantics | Optimizer | Aggregate ordinal handoff missing |
| Ch20 | Logical nodes/properties/rewrites | Canonical relational meaning | Ch21–35 | Primary scope |
| Ch21 | DML/DDL semantics | Receives relational sources/target streams | Ch31 | Compatible |
| Ch29 | Aggregate values/signatures | Receives groups/aggregate inputs | Execution | Compatible except ordinal handoff |
| Ch30 | Sort comparator/ties | Executes `LogicalSort` requirements | Execution | Compatible; precise citation missing |
| Ch33 | Search/physical selection | Consumes validated logical plan | Physical plan | Clear |
| Ch35 | Proof/estimation boundary | Supplies exact proof authority | Rewrites/search | Clear |
| §39 | Error/resource consequences | Receives logical/runtime failures | Transaction layer | Generally clear |

## Explicit cross-reference audit

| Source | Target | Purpose | Exists/owner | Status |
|---|---|---|---|---|
| 20.5 | §18.11.1 | No-FROM semantics | Exists, but delegates back to Ch20 | **Circular** |
| 20.13.3 | Ch15 | Update RID/write protocol | Correct | Good |
| 20.14.1 | §20.14.7 | Physical fallback definition | Correct internal target | Good |
| 20.14.2 | Ch19 | Query-block binding | Correct but broad | Good |
| 20.14.2 | §18.11.1 | Nested no-FROM source | Exists | Circular ownership wording |
| 20.14.3 | §19.5 | Derived output names | Correct | Good |
| 20.14.4 | Ch17 | Scalar typing | Correct but broad | Good |
| 20.14.6 | Ch17 | Equality/coercion | Correct but broad | Good |
| 20.14.6 | §17.10.3 | Hash/equality normalization | Correct | Good |
| 20.14.8 | Ch17 | Evaluation order | Correct but broad | Good |
| 20.14.8 | Ch10 | CommandId | Correct owner | Good |
| 20.14.9 | §17.7.3 | Short-circuit | Correct | Good |
| 20.14.9 | §29.3 | Aggregate signatures | Correct | Good |
| 20.14.9 | Ch21 | DML assignment/row image | Correct | Good |
| 20.14.9 | Ch19 | LIMIT admissibility | Correct | Good |
| 20.14.9 | §39.1 | DML error consequences | Correct | Good |
| 20.14.10 | §20.14.5 | EXISTS projection irrelevance | Correct | Good |
| 20.14.11 | §§34.1, 35.2 | Trusted proof | Correct | Good |
| 20.14.11 | §§20.17.10, 35.2 | Empty proof | Correct | Good |
| 20.14.12 | Ch17 | Skipped branches | Correct but broad | Good |
| 20.14.12 | §39 | Fatal errors | Correct but broad | Good |
| 20.15 | §18.11.1 | No-FROM edge cases | Circular ownership | Finding |
| 20.16 | §35.2 | Estimate/proof distinction | Correct | Good |
| 20.16 | §20.17.10 | Proof propagation | Correct | Good |
| 20.17.1 | §17.10.2 | Folding semantics | Correct | Good |
| 20.17.10 | §35.2 | Exact-proof whitelist | Correct | Good |
| 20.17.10 | §29.5 | Global aggregate row | Correct | Good |
| 20.18 | §§20.17.10, 35.2 | Proof validation | Correct | Good |
| 20.18 | §20.14 | Subquery validation | Correct | Good |
| 20.20 | §§17.2–17.10 | Closed scalar registry | Correct | Good |
| 20.20 | §§34.1, 35.2 | Subquery proof provenance | Correct | Good |

Missing precision: §§20.11–20.12 should cite §30.1/§30.3 and §§27.9/35.20 or state those semantics locally.

## Temporal-language audit

| Occurrence | Classification |
|---|---|
| “later planning” §20.1 | D — downstream navigation; valid |
| “Initial logical operator family” §20.3 | E — project chronology; finding |
| “initial logical family” §20.3 | E — project chronology; finding |
| “later optimizer” §20.4 | D — downstream navigation |
| “later be vectorized” §20.6 | D — downstream execution navigation |
| “used later for aggregate/DISTINCT” §20.9 | D — downstream navigation |
| “added later” §20.11 | E — roadmap; finding |
| “optimizer may later” §20.12 | D — downstream navigation |
| “later rows not demanded” §20.14.1 | A — runtime ordering |
| “future architecture” §20.14.2 | E — roadmap; finding |
| “later streaming work” §20.14.4 | A — runtime ordering |
| “later rows” §20.14.5 | A — runtime ordering |
| “initial logical representation” §20.14.7 | A — pipeline state |
| “currently demanded outer selection” §20.14.7 | A — runtime state |
| “later outer rows” §20.14.8 | A — runtime ordering |
| “current-command rules” §20.14.8 | B — technical CommandId term |
| “later work/rows/left errors” §20.14.12 | A — runtime precedence |
| “later optimization” §20.16 | D — downstream navigation |
| “later physical-property system” §20.16 | D — downstream navigation |
| “Future VOLATILE/STABLE functions” §20.17.1 | E — roadmap; finding |
| “currently enforced constraints” §20.17.8 | C — durable constraint status, acceptable but rewriteable |
| “remain deferred” §20.17.9 | E — roadmap; finding |
| “initial logical planning” §20.18 | A — pipeline stage |
| “Before physical planning exists” §20.19 | E — current-state narration; finding |

## Documentation-model matrix

| Check | Result |
|---|---|
| No chronology | FINDING |
| No current implementation state | FINDING |
| No DEVELOPMENT sequencing | CONSISTENT |
| No VERIFICATION recipes | CONSISTENT |
| No PROJECT_STATE leakage | CONSISTENT |
| No devlog/history | CONSISTENT |
| Logical semantics precise | FINDING |
| Bag/set semantics precise | FINDING |
| Order semantics precise | CONSISTENT, with local reference gap |
| NULL semantics precise | CONSISTENT |
| Cardinality precise | FINDING for core joins/Limit |
| Slot identity precise | FINDING |
| Grouping precise | CONSISTENT |
| DISTINCT precise | CONSISTENT |
| LIMIT precise | CONSISTENT cross-chapter, incomplete locally |
| Subquery precise | CONSISTENT |
| Error ownership precise | FINDING for rewrites |
| Optimizer boundary precise | CONSISTENT |
| Execution boundary precise | Mostly consistent; §20.14.7 leakage |
| Implementation freedom preserved | Mostly consistent |
| Deterministic semantics | FINDING for rewrite errors |
| Exact references | FINDING |
| Analytical rationale | Strong overall |
| Timelessness | FINDING |

## Technical consistency matrix — 180 actual questions

Status: `C` consistent, `S` consistent through a specialized owner, `F` finding.

| # | Question | Status |
|---:|---|:---:|
| 1 | Does Chapter 20 consume bound Chapter-19 expressions? | C |
| 2 | Does it avoid rebinding names? | C |
| 3 | Are physical algorithms excluded from logical nodes? | C |
| 4 | Is the logical tree immutable once published? | C |
| 5 | Is `BindingId` carried by `LogicalGet`? | C |
| 6 | Is `ColumnId` distinct from `BindingId`? | C |
| 7 | Is heap `SlotId` distinct from `LogicalSlotId`? | C |
| 8 | Is self-join base identity preserved? | C |
| 9 | Is `LogicalSlotId` uniqueness domain exact? | F |
| 10 | Are nested-block slot domains exact? | F |
| 11 | Is slot nonreuse exact? | F |
| 12 | Is duplicate-projection slot identity exact? | F |
| 13 | Is pass-through preservation mandatory? | F |
| 14 | Is slot persistence excluded? | S |
| 15 | Is pointer identity excluded? | S |
| 16 | Does each node expose output schema? | C |
| 17 | Does output schema include display name? | C |
| 18 | Does it include logical type? | C |
| 19 | Does it include nullability? | C |
| 20 | Does it include lineage? | C |
| 21 | Does it include internal visibility? | C |
| 22 | Is output-slot order deterministic? | C |
| 23 | Is base-column-to-slot allocation exact? | F |
| 24 | Are computed outputs fresh? | C |
| 25 | Are aggregate outputs fresh? | C |
| 26 | Are derived boundary slots mapped? | C |
| 27 | Is aggregate ordinal retained? | F |
| 28 | Is `SourceSpan` retained through planning? | F |
| 29 | Is synthesized-expression provenance defined? | F |
| 30 | Does validation cover provenance? | F |
| 31 | Is Get logically access-path independent? | C |
| 32 | Does Get identify immutable descriptor/SchemaVer? | C |
| 33 | Does Get expose exact base multiplicity? | F |
| 34 | Does Get avoid hidden physical order? | C |
| 35 | Does Values contain typed bound expressions? | C |
| 36 | Does Values retain duplicate rows explicitly? | F |
| 37 | Is Values source-row order semantic? | F |
| 38 | Is no-FROM exactly one zero-column row? | C |
| 39 | Does no-FROM use ordinary operators? | C |
| 40 | Is the no-FROM owner reference correct? | F |
| 41 | Does Filter preserve child schema? | C |
| 42 | Does Filter retain only TRUE? | C |
| 43 | Does Filter reject FALSE? | C |
| 44 | Does Filter reject UNKNOWN? | C |
| 45 | Is one output per retained input explicit? | F |
| 46 | Does Project preserve row multiplicity explicitly? | F |
| 47 | Can Project reorder columns? | C |
| 48 | Can Project duplicate source values? | C |
| 49 | Are duplicate output values retained? | S |
| 50 | Is ordinary projection demand exact? | F |
| 51 | Are projection errors skipped on no input? | S |
| 52 | Are unused derived projection errors defined? | F |
| 53 | Is INNER supported? | C |
| 54 | Is LEFT supported? | C |
| 55 | Is CROSS supported? | C |
| 56 | Is CROSS condition forbidden? | C |
| 57 | Is every TRUE INNER pair emitted? | F |
| 58 | Are duplicate matching pairs preserved? | F |
| 59 | Is INNER output left-then-right? | F |
| 60 | Is INNER empty-side behavior derivable? | C |
| 61 | Is LEFT right-side nullability explicit? | C |
| 62 | Is every matching LEFT pair emitted? | F |
| 63 | Is one extended row emitted for no match? | F |
| 64 | Do FALSE and UNKNOWN both mean no match? | C |
| 65 | Is LEFT output slot order exact? | F |
| 66 | Does empty left yield empty LEFT result? | C |
| 67 | Does empty right preserve every left row? | C |
| 68 | Is CROSS Cartesian multiplicity exact? | F |
| 69 | Does either empty CROSS side yield empty? | C |
| 70 | Is ON distinct from WHERE? | C |
| 71 | Is predicate decomposition nonphysical? | C |
| 72 | Are outer-join pushdowns constrained? | C |
| 73 | Is filter pushdown error-safe? | F |
| 74 | Is join predicate evaluation demand exact? | F |
| 75 | Can physical pair order affect logical errors? | F |
| 76 | Is grouping equivalence distinct from predicate `=`? | C |
| 77 | Do grouping NULLs share a class? | C |
| 78 | Are +0/-0 equivalent? | C |
| 79 | Are canonical NaNs equivalent? | C |
| 80 | Is VARCHAR grouping byte-based? | C |
| 81 | Does explicit grouping on empty input yield zero groups? | C |
| 82 | Does global aggregation on empty input yield one row? | C |
| 83 | Is runtime grouping distinct from binder structural equality? | C |
| 84 | Are group outputs only keys/aggregates? | C |
| 85 | Are aggregate result values delegated to Chapter 29? | C |
| 86 | Is aggregate argument demand clear? | S |
| 87 | Is aggregate occurrence ordinal consumed? | F |
| 88 | Can rewrites merge aggregate occurrences? | F |
| 89 | Does HAVING retain TRUE only? | C |
| 90 | Does child aggregate remain block-local? | C |
| 91 | Is DISTINCT explicit rather than projection flag? | C |
| 92 | Does DISTINCT use grouping equivalence? | C |
| 93 | Are NULL duplicate rows collapsed? | C |
| 94 | Are NaN duplicate rows collapsed? | C |
| 95 | Are +0/-0 duplicate rows collapsed? | C |
| 96 | Are multi-column classes component-wise? | S |
| 97 | Is DISTINCT order preservation excluded? | S |
| 98 | Does DISTINCT preserve one representative only? | C |
| 99 | Is DISTINCT before ORDER? | C |
| 100 | Is DISTINCT before scalar cardinality checking? | C |
| 101 | Does Sort carry ordered keys? | C |
| 102 | Does Sort carry ASC/DESC? | C |
| 103 | Does Sort carry resolved NULL order? | C |
| 104 | Is ASC NULLS FIRST? | C |
| 105 | Is DESC NULLS LAST? | C |
| 106 | Is NaN comparison delegated exactly? | S |
| 107 | Is multi-key ordering lexicographic? | S |
| 108 | Is equal-key tie order unspecified? | S |
| 109 | Is final top-level ORDER respected? | C |
| 110 | Is unordered top-level output allowed? | C |
| 111 | Does Limit carry optional limit and offset? | C |
| 112 | Is count admissibility left to Chapter 19? | C |
| 113 | Is OFFSET-before-LIMIT local? | F |
| 114 | Is LIMIT cardinality local? | F |
| 115 | Is LIMIT 0 exact? | C |
| 116 | Are large counts nonerrors? | S |
| 117 | Is order preserved through Limit? | S |
| 118 | Is LIMIT without ORDER explicitly unordered? | C |
| 119 | Is OFFSET without ORDER likewise unordered? | C |
| 120 | Is hidden heap/index first-row order forbidden? | C |
| 121 | Is scalar subquery arity exactly one? | C |
| 122 | Do zero scalar rows yield typed NULL? | C |
| 123 | Does one scalar row yield its value? | C |
| 124 | Does the second completed row raise cardinality error? | C |
| 125 | Is scalar result always nullable? | C |
| 126 | Are child clauses applied before scalar cardinality? | C |
| 127 | Are later rows skipped after the second? | C |
| 128 | Does a first/second projection error precede cardinality? | C |
| 129 | Is arbitrary first-row fallback forbidden? | C |
| 130 | Can LIMIT 1 prove at-most-one? | C |
| 131 | Does EXISTS zero yield FALSE? | C |
| 132 | Does EXISTS nonzero yield TRUE? | C |
| 133 | Is EXISTS non-null? | C |
| 134 | Are projection-only values irrelevant to EXISTS? | C |
| 135 | Are later EXISTS rows undemanded? | C |
| 136 | Is global aggregate EXISTS behavior exact? | C |
| 137 | Does IN empty yield FALSE, even for NULL left? | C |
| 138 | Does NOT IN empty yield TRUE? | C |
| 139 | Does NULL left on nonempty build yield UNKNOWN? | C |
| 140 | Does a matching non-NULL value yield TRUE? | C |
| 141 | Does no match plus RHS NULL yield UNKNOWN? | C |
| 142 | Are IN duplicates irrelevant? | C |
| 143 | Is the complete IN build demanded once? | C |
| 144 | Does the left error precede dormant build initialization? | C |
| 145 | Does derived-table alias form a slot boundary? | C |
| 146 | Are child hidden aliases excluded? | C |
| 147 | Is derived order not advertised outside? | C |
| 148 | Does child ORDER still govern child LIMIT? | C |
| 149 | Is correlation rejected? | C |
| 150 | Is subquery state per occurrence/attempt? | C |
| 151 | Is subquery evaluation lazy? | C |
| 152 | Is successful subquery state reused? | C |
| 153 | Does retry discard prior-attempt state? | C |
| 154 | Is snapshot shared with containing statement? | C |
| 155 | Is CommandId shared? | C |
| 156 | Are skipped CASE/AND/OR subqueries undemanded? | C |
| 157 | Is subquery rewrite proof exact rather than estimated? | C |
| 158 | Is NOT IN rewrite NULL-aware? | C |
| 159 | Is scalar cardinality removal proof-based? | C |
| 160 | Is EXISTS projection error suppression explicit? | C |
| 161 | Does constant folding preserve §17.10.2? | C |
| 162 | Is Boolean simplification 3VL-aware? | C |
| 163 | Does it preserve erroring short-circuit branches? | C |
| 164 | Is ordinary predicate pushdown demand-safe? | F |
| 165 | Is projection pruning demand-safe? | F |
| 166 | Is operand canonicalization left-to-right safe? | F |
| 167 | Is comparison reversal error-safe? | F |
| 168 | Is contradiction replacement demand-safe? | F |
| 169 | Are LEFT rewrites null-safe? | C |
| 170 | Are estimates forbidden as semantic proof? | C |
| 171 | Is exact emptiness operator-specific? | C |
| 172 | Does empty global aggregate remain one row? | C |
| 173 | Does empty DML preserve statement completion? | C |
| 174 | Does validator check slots and schemas? | C |
| 175 | Does validator define “unique where required”? | F |
| 176 | Does validator preserve aggregate ordinal? | F |
| 177 | Does validator preserve runtime spans? | F |
| 178 | Is logical EXPLAIN AST-independent? | C |
| 179 | Is logical EXPLAIN timelessly worded? | F |
| 180 | Can identical bound input have one error result under all permitted rewrites? | F |

## Implementer-invention assessment

A conforming implementer still must invent policy for:

- exact `LogicalSlotId` domain and duplicate projections;
- base-column-to-slot allocation;
- core bag preservation;
- join multiplicity and output slot order;
- aggregate ordinal preservation;
- ordinary projection/filter/join expression demand;
- error-safe predicate pushdown and projection pruning;
- error-safe operand canonicalization;
- runtime `SourceSpan` preservation through rewrites.

The following high-risk areas are already exact:

- no-FROM cardinality;
- DISTINCT/grouping equivalence, including NULL/NaN/±0;
- global aggregate and explicit-group empty-input behavior;
- HAVING UNKNOWN rejection;
- NULL sort placement;
- tie-order non-guarantee through downstream owner;
- unordered LIMIT behavior;
- scalar-subquery zero/one/many behavior;
- EXISTS projection irrelevance;
- IN-subquery 3VL;
- derived-table order boundary;
- unsupported correlation;
- proof versus estimate separation.

Therefore Chapter 20 cannot yet stand alone as canonical v1 logical architecture.

## Follow-up verification gaps

Existing verification is strongest for subqueries, logical-plan validation, exact emptiness, and structural rewrite fixtures.

| Architecture area | Coverage | Gap |
|---|---|---|
| Subquery semantics | COMPLETE | Existing detailed family is reusable |
| Logical validator | COMPLETE for stated invariants | Blocked for undefined slot/ordinal/span invariants |
| Canonical plan shapes | PARTIAL | Needs exact schema, bag, slot, and ordering oracles |
| `LogicalSlotId` | BLOCKED | Architecture must define domain/reuse |
| Core bag semantics | MISSING/BLOCKED | Need per-node multiplicity oracle |
| Join semantics | PARTIAL/BLOCKED | Need matching-pair and output-schema oracle |
| DISTINCT/group equivalence | PARTIAL | Reuse Chapter-17 key normalization oracle |
| ORDER/NULL/ties | PARTIAL | Add semantic comparator/order-equivalence matrix |
| LIMIT ordered/unordered | PARTIAL | Add prefix/cardinality and unordered-equivalence oracle |
| Aggregate ordinal | MISSING/BLOCKED | Architecture must define logical handoff |
| Rewrite error preservation | BLOCKED | No architecture oracle for ordinary demand |
| SourceSpan through rewrites | MISSING/BLOCKED | Architecture must define provenance |
| Resource/physical algorithms | Existing downstream methodology | No Chapter-20 duplication needed |

## Frozen architecture semantic questions

1. **Ordinary expression demand**
   - Sections: §§20.6–20.7, 20.17.3–20.17.4.
   - Undefined: which relational rows demand project/filter/join/group/order expressions, especially after rewrites.
   - Observable consequence: error versus no error.
   - Smallest decision: define demand and rewrite-preservation rules.

2. **Operand canonicalization**
   - Section: §20.17.5.
   - Conflict: canonical commutative ordering versus §17.6 left-to-right errors.
   - Smallest decision: total/error-free proof condition or prohibition.

3. **Logical slot identity**
   - Section: §20.2.
   - Undefined: uniqueness domain, duplicate outputs, derived-boundary allocation, nonreuse.
   - Smallest decision: exact `LogicalSlotId` lifecycle.

4. **Core bag and join contract**
   - Sections: §§20.4–20.8.
   - Undefined: duplicate preservation, pair multiplicity, output slot order.
   - Smallest decision: explicit bag semantics and formulas.

5. **Aggregate ordinal handoff**
   - Sections: §§20.9, 20.15, 20.20.
   - Undefined: preservation through logical planning and rewrites.
   - Smallest decision: attach ordinal to logical aggregate occurrence and state rewrite rules.

6. **Rewrite diagnostic provenance**
   - Section: §20.17.5.
   - Undefined: required `SourceSpan` for retained and synthesized expressions.
   - Smallest decision: mandatory provenance mapping.

## Direct answers to closing questions

| Question | Answer |
|---|---|
| Project-time/current-state wording? | Yes; F20-N1/N6 |
| DEVELOPMENT-owned material? | No material sequencing; some optimizer navigation is valid |
| VERIFICATION recipe? | No |
| PROJECT_STATE material? | One current-state EXPLAIN sentence |
| History/devlog material? | No |
| LogicalSlotId ambiguity? | Yes |
| Logical output-schema ambiguity? | Yes, identity/rewrite aspects |
| Bag/multiplicity ambiguity? | Yes |
| No-FROM ambiguity? | No semantic ambiguity; owner citation is circular |
| JOIN ambiguity? | Yes |
| LEFT JOIN nullability ambiguity? | No |
| DISTINCT-equivalence ambiguity? | No |
| Grouping-equivalence ambiguity? | No |
| Empty-input aggregate ambiguity? | No |
| HAVING ambiguity? | No |
| NULL-order ambiguity? | No |
| ORDER-tie ambiguity? | No when downstream §30.1 is included |
| Unordered-result ambiguity? | No at the major result boundary |
| LIMIT-without-ORDER ambiguity? | No |
| OFFSET-without-ORDER ambiguity? | No |
| Scalar-zero-row ambiguity? | No |
| Scalar-cardinality ambiguity? | No |
| EXISTS evaluation ambiguity? | No |
| IN-subquery ambiguity? | No |
| Derived-order ambiguity? | No |
| Expression-evaluation/error ambiguity? | Yes, outside the specialized subquery contract |
| Unsafe rewrite rule? | Yes |
| Optimizer/execution ownership ambiguity? | Limited physical leakage, not fundamental |
| Catalog/physical-order leakage? | No hidden physical result-order contract |
| Implementation-technique overconstraint? | Yes, locally in §20.14.7 |
| Correctness-relevant implementer invention? | Yes |
| Can Chapter 20 stand as timeless canonical v1 architecture? | No |

## Previous-chapter regression and boundaries

- Chapters 17–19 were not reopened or modified.
- Chapter 20 is compatible with frozen Chapter-19 binding for names, aliases, grouping legality, ORDER resolution, LIMIT counts, and subquery correlation rejection.
- Compatibility gaps are downstream handoffs: aggregate ordinal, logical slots, provenance, and rewrite error preservation.
- Chapter 21 review was not started. Only its heading and §21.1 ownership boundary were inspected.
- Recommended future Chapter-21 scope: DDL/DML publication, catalog visibility, defaults, RETURNING row images, index construction, DROP retirement, and the exact consumption of Chapter-20 DML logical streams.

## Recommended next action

**Frozen semantic architecture review required** for F20-B1, F20-B2, and the six major questions. After those decisions are integrated:

1. perform targeted Chapter-20 document cleanup for F20-N1–N6;
2. synchronize Chapter-20 verification;
3. only then begin Chapter-21 direct review.

## Repository and read-only confirmation

- Initial Git status: clean
- Initial index: clean
- Initial HEAD: `23c7df5f599907918966a6c4d3ecfa6c3180517d`
- Final Git status: clean
- Final index: clean
- Final HEAD: `23c7df5f599907918966a6c4d3ecfa6c3180517d`
- `git diff --check`: passed with no output
- Files modified by audit: **NONE**
- Repository-state change: **NONE**
- No review artifact was read, modified, created, or staged.
- No build, test, benchmark, implementation, staging, or commit occurred.
- Chapter 21 direct review: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
