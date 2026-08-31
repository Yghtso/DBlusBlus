# Chapter 19 review verdict

**CHAPTER 19 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 19 is generally analytical, timeless, and compatible with Chapters 16–18, but it does not yet define a unique canonical binding result for every valid raw AST/catalog snapshot. The principal gaps concern alias namespaces, output-alias ambiguity, clause visibility, grouped-query legality, ordinal interpretation, execution-start constants, and deterministic error precedence.

| Severity | Count |
|---|---:|
| BLOCKING | 7 |
| MAJOR | 5 |
| MINOR | 4 |
| EDITORIAL | 0 |

No file was modified.

## Scope and structure

Primary scope read: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15419), lines 15419–15811.

- Exact title: `19. Binding and Expression Semantics`
- Start: line 15419
- End: line 15811
- Chapter 20 begins at line 15812: `20. Logical Plans, Properties, and Rewrites`

Context consulted:

- Architecture front matter
- Chapters 16–18 as frozen direct owners
- Chapters 20, 21, and 29 as context-only downstream owners
- §§39.1–39.3 and relevant Chapter 41 material
- [VERIFICATION.md binder coverage](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7691)
- DEVELOPMENT and PROJECT_STATE only for document-role classification
- No archival review artifact or devlog was read

## Complete heading and section-review inventory

Legend: `OK` = sufficient; `Gap` = semantic finding; `Thin` = clear but rationale/delegation could improve; `N/A` = domain not owned by the section.

| Section | Exact heading | Architectural role | Timelessness | Document ownership | Analytical depth | Terminology | Binding identity | Scope | Names | Types | Visibility | Subqueries | Errors | Determinism | Cross-refs | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 19 | Binding and Expression Semantics | Chapter boundary | OK | Appropriate | Thin | OK | Gap | Gap | Gap | OK | Gap | Gap | Gap | Gap | Thin | OK | Finding |
| 19.1 | Binder role | Binder/later-layer boundary | OK | Appropriate | OK | OK | Thin | Thin | OK | OK | Thin | Thin | Gap | Thin | Thin | OK | Finding |
| 19.2 | Binding scopes and BindingId | Scope and relation-occurrence identity | Temporal issue | Appropriate | Thin | OK | Gap M1 | Gap B1 | Gap B1 | N/A | Gap B3 | Gap | Thin | Gap | Self-ref only | OK | Finding |
| 19.3 | Bound column references | Resolved column identity | OK | Appropriate | OK | OK | OK | OK | OK | OK | N/A | N/A | Thin | OK | Thin | OK | Mostly clean |
| 19.4 | Column name resolution | Lookup contract | OK | Appropriate | Thin | OK | OK | Gap B1 | Gap B1 | N/A | N/A | Gap for outer lookup | Gap M5/B7 | Gap | None | OK | Finding |
| 19.4.1 | Unqualified names | Candidate cardinality | OK | Appropriate | OK | OK | OK | Thin | OK | N/A | N/A | Thin | Gap category | OK | None | OK | Mostly clean |
| 19.4.2 | Qualified names | Qualifier/member lookup | OK | Appropriate | Thin | OK | OK | Gap B1 | Gap B1 | N/A | N/A | Thin | Gap category | Gap | None | OK | Finding |
| 19.5 | Wildcards and output names | Expansion and output metadata | OK | Appropriate | Thin | OK | OK | Gap B1 | Mostly OK | OK | Gap B2 | Derived rules delegated | Gap | Expansion order OK | Thin | OK | Finding |
| 19.6 | Bound-expression IR | Bound semantic node contract | Temporal issue | Appropriate | Thin | OK | OK | OK | OK | OK | N/A | OK | Thin | OK | Thin | OK | Finding |
| 19.7 | Expression immutability and ownership | Lifetime and representation freedom | OK | Role issue N2 | Thin | OK | N/A | N/A | N/A | N/A | N/A | N/A | N/A | OK | None | OK | Minor |
| 19.8 | Operator registry | TypeResolver/operator handoff | OK | Appropriate | OK | OK | N/A | N/A | N/A | OK | N/A | N/A | TypeError delegated | OK | §§17.6–17.7 | OK | Clean |
| 19.9 | Function registry and volatility | Generic-call binding | Temporal issue | Future-model leakage | Thin | OK | N/A | N/A | Function names mostly clear | OK | N/A | N/A | Gap M5 | OK | §§17.9.3, 21.12 | OK | Finding |
| 19.10 | Aggregate expressions | Aggregate classification and typing | Temporal issue | Appropriate | Thin | OK | N/A | Query-block scope | Registry clear | OK | Placement incomplete | Subquery boundary thin | Gap | Ordinal delegated but uncited | Ch29/§29.3 | OK | Finding |
| 19.11 | Aggregate query semantics | Grouped-expression legality | OK | Appropriate | Thin | OK | N/A | Query block | N/A | OK | Gap B4 | Boundary unclear | Gap | Gap | None | Incomplete | Finding |
| 19.12 | HAVING | HAVING binding and visibility | OK | Appropriate | Thin | OK | N/A | Query block | Input names | Boolean clear | Gap B4 | Thin | Gap | Gap | None | Incomplete | Finding |
| 19.13 | ORDER BY resolution | Alias/ordinal/expression binding | OK | Appropriate | Thin | OK | Slot identity later | Query block | Gap B2 | Orderability clear | Gaps B2/B5 | N/A | Gap | Gap | Vague Ch17 | Incomplete | Finding |
| 19.14 | LIMIT and OFFSET | Count-expression binding | OK | Appropriate | Thin | OK | N/A | Gap B3 | N/A | Integral only | Gap B3 | Subquery prohibition implied elsewhere | Gap | Gap B6 | None | Incomplete | Finding |
| 19.15 | DISTINCT | Bound/logical DISTINCT handoff | OK | Mostly later-owned | Thin | OK | N/A | Query block | N/A | Thin | N/A | N/A | Type capability unclear | OK | No §20.10 ref | Consistent | Minor |
| 19.16 | Searched CASE | Chapter-17 composition | OK | Appropriate | OK | OK | N/A | Expression scope | N/A | OK | N/A | N/A | TypeError delegated | OK | Precise | OK | Clean |
| 19.17 | IN-list semantics | Chapter-17 composition | OK | Appropriate | OK | OK | N/A | Expression scope | N/A | OK | N/A | Subquery form elsewhere | TypeError delegated | Source order OK | Precise | OK | Clean |
| 19.18 | Subquery binding boundary | Local scopes and unsupported correlation | OK | Appropriate | Thin | OK | Gap M1 | Mostly clear | Local/outer distinction clear | Typed child delegated | N/A | Mostly clear | Category partly explicit | Mostly clear | Precise §20.14 | OK | Mostly clean |
| 19.19 | Parameters | Excluded syntax/future compatibility | Temporal issue | Non-v1 roadmap leakage | Thin | OK | N/A | N/A | N/A | Future-only | N/A | N/A | N/A | N/A | None | No v1 conflict | Minor |
| 19.20 | Binder/expression invariants | Consolidated correctness contract | Temporal issue | Appropriate | Thin | OK | Gap M1 | Gap B1 | Gap B1 | OK | Gaps B2–B6 | Mostly clear | Gaps M5/B7 | Gap | Broad §17 refs | Incomplete | Finding |

## Canonical owner map

| Concept | Chapter-19 role | Canonical owner | Duplication/ambiguity | Status |
|---|---|---|---|---|
| Raw SQL grammar/raw AST | Consumer | Chapter 18 | No redefinition | Consistent |
| Canonical identifier bytes | Consumer | Chapters 16/18 | No refolding authorized | Consistent |
| TableId/ColumnId/SchemaVer | Resolves references to IDs | Chapter 16 | BindingId relation partly local | Mostly consistent |
| Catalog snapshot/DDL visibility | Consumer | Chapters 9, 16, 21 | Delegation insufficiently explicit | Thin |
| BindingId | Defines relation-occurrence identity | Chapter 19 | Domain/stability incomplete | Ambiguous owner detail |
| Query-block scopes | Defines local binding namespace | Chapter 19 | Alias rules incomplete | Ambiguous |
| Table/index lookup | Binder operation | Chapters 16, 19, 21 | Index/drop details incomplete | Partial |
| Column resolution | Owns candidate cardinality | Chapter 19 | Alias qualifier namespace incomplete | Partial |
| Wildcard expansion | Owns binding expansion | Chapter 19 + Ch16 order | Deterministic order defined | Consistent |
| Output names/schema | Owns bound output metadata | Chapter 19; derived constraints Ch20 | Generated/duplicate names incomplete | Partial |
| Scalar types/NULL/casts | Applies | Chapter 17 | Correct delegation | Consistent |
| Operator resolution | Applies registry | Chapter 17 | Correct delegation | Consistent |
| Function resolution | Applies registries | Chapters 17/29 | Future descriptor prose unnecessary | Mostly consistent |
| Aggregate classification | Binder integration | Chapters 19/29 | Placement incomplete; ordinal owner uncited | Partial |
| GROUP BY/HAVING legality | Binder semantic owner | Chapter 19 | Incomplete | Ambiguous |
| ORDER BY binding | Binder semantic owner | Chapter 19 | Alias collision/ordinal syntax incomplete | Ambiguous |
| LIMIT/OFFSET binding | Binder semantic owner | Chapter 19 | Constant domain incomplete | Ambiguous |
| DISTINCT logical semantics | Records bound intent | Chapter 20 | Repeated summary | Referenced/partly duplicated |
| DML binding | Resolves targets/expressions | Chapters 19/21 | Namespace details incomplete | Partial |
| DDL binding/defaults | Validates declarations | Chapter 21, consumed by Ch19 | Some lookup/error boundaries incomplete | Partial |
| Subquery local binding | Owns lookup boundary | Chapter 19 | Mostly precise | Consistent |
| Subquery shape/derived output | Consumer | Chapter 20 | Precise references | Consistent |
| Aggregate ordinal | Consumer/producer boundary | Chapter 29 | Missing direct reference | Thin |
| SourceSpan selection | Preserves responsible syntax | Chapter 18/§39.2 | Inserted-cast provenance thin | Partial |
| Error categories/consequences | Emits cause; transaction elsewhere | §39 | Exact mapping/priority incomplete | Ambiguous |
| Lifetime/resource behavior | Applies frozen contract | Chapter 18/§39.3 | Representation freedom preserved | Consistent |

## Binding and scope assessment

### Raw AST → bound representation

The chapter establishes the main transformation correctly:

- Raw object and column names become catalog identities and relation-occurrence identities.
- Bound columns contain at least `BindingId`, `TableId`, `ColumnId`, logical type, nullability, and `SourceSpan`.
- Literals, operators, casts, CASE, IN, and calls become typed bound expressions.
- TypeResolver-selected implicit casts are inserted before planning.
- Parenthesized/source provenance is available from Chapter 18, but Chapter 19 does not precisely say which provenance survives normalization.
- Bound expressions are immutable and may retain backing under D18-L1’s retain-or-materialize contract.
- No physical operators, pages, RIDs, or persistent object allocation are introduced.

The boundary is sound, but not complete enough to yield one canonical bound tree in all alias, clause, and error cases.

### BindingId/TableId/ColumnId

| Entity | TableId | BindingId | ColumnId | Persistent? | Scope |
|---|---|---|---|---|---|
| Base-table occurrence | Yes | Yes | Per column | Only catalog IDs | Query-local, exact domain unclear |
| Self-join occurrence | Same TableId | Distinct BindingId | Same source ColumnIds | BindingId no | Same query |
| Derived table | No base TableId necessarily | Implied relation identity, not explicit enough | Output slots later | No | Child/parent query block |
| Repeated subquery | Child identities implied | Collision policy unspecified | Child output | No | Nested query blocks |
| Bound column reference | Yes | Yes | Yes | Catalog IDs only | Refers to one visible occurrence |

The self-join distinction is explicit and correct. The unresolved issue is whether BindingIds are unique per query block, per whole statement, or per bound query graph, whether they can be reused across nested scopes, and what stability rewrites require.

### Scope and visibility

| Context | Visible table bindings | Table aliases | SELECT aliases | Outer scope | Aggregates | Status |
|---|---|---|---|---|---|---|
| FROM item | Prior/current visibility not specified | Alias creation incomplete | No | No for subqueries | N/A | Finding |
| JOIN ON | Joined bindings implied | Yes | No implied | No | Explicitly forbidden at level | Mostly clear |
| WHERE | Current FROM bindings | Yes | Not specified | No | Forbidden | Finding |
| GROUP BY | Current FROM bindings | Yes | Not specified | No | Placement undefined | Finding |
| HAVING | Current/grouped bindings | Yes | Explicitly no SELECT aliases | No | Yes | HAVING-alone gap |
| SELECT list | Current FROM bindings | Yes | Same-list aliases unspecified | No | Yes subject to grouping | Finding |
| ORDER BY | Input expressions and output aliases | Yes | Yes | No | Implied allowed | Alias collision/ordinal gap |
| LIMIT/OFFSET | Unspecified | Unspecified | Unspecified | Subqueries prohibited elsewhere | Unspecified | Finding |
| RETURNING | Target row image via Ch21 | Target qualifier exposure unclear | No | No | Placement unclear | Finding |
| DDL default | Closed expression set via §21.12 | No relation scope intended | No | No | No | Semantics exist, delegation thin |
| Subquery | Local child bindings only | Child aliases | Child-local rules | Lookup detects but does not bind outer | Per child query | Mostly clear |

### Name resolution

| Form | Lookup domain | Zero matches | One match | Multiple matches | Owner/status |
|---|---|---|---|---|---|
| `col` | Visible local relation columns | Unknown column | Bind | Ambiguous column | Ch19; clear |
| `alias.col` | Relation qualifier, then member | Unknown qualifier/column distinguished | Bind | Duplicate qualifier behavior undefined | Ch19; finding |
| `table.col` | Alias or base-name policy undefined | Unknown qualifier | Bind if exposed | Collision policy undefined | Ch19; finding |
| Table name | Snapshot-visible `main` catalog | Catalog/name error | Table descriptor | Catalog uniqueness prevents many | Ch16/21 |
| Index name | Snapshot-visible index namespace | Error mapping unclear | Index descriptor | Catalog uniqueness | Ch16/21; thin |
| Type name | Closed type registry | Type resolution error | TypeId | Registry unique | Ch17; clear |
| Function name | Scalar and aggregate registries | Unsupported/unknown category unclear | Resolve descriptor | Scalar/aggregate conflict rule implicit only | Ch17/29; thin |

### Aliases and ambiguity

The architecture does not answer:

- whether `FROM t AS x` hides qualifier `t`;
- whether two relation occurrences may expose the same alias;
- whether an alias conflicts with an unaliased table qualifier;
- whether inner aliases shadow outer qualifiers for unsupported-correlation detection;
- how duplicate output aliases are handled;
- what happens when `ORDER BY x` matches multiple output aliases.

Unqualified-column cardinality itself is correctly deterministic: zero, one, or multiple candidates produce unknown, resolved, or ambiguous outcomes.

## Wildcards and output schema

| Case | Resolution | Expansion order | Output names | Error/status |
|---|---|---|---|---|
| `*` | All visible FROM relations | Source relation order, then descriptor presentation order | Source column names | Clear |
| `x.*` | One qualifier binding | That binding’s presentation order | Source column names | Qualifier ambiguity incomplete |
| Duplicate names | Preserved by expansion | Stable | Duplicate top-level behavior unspecified | Finding |
| Self-join | Both occurrences | Source occurrence order | Repeated source names | Expansion clear |
| Derived table | Child exported schema | Child projection order | §20.14.3 rules | Precise delegation |

Output metadata contains expression, display name, type, and nullability. Name priority is:

1. explicit `AS`;
2. direct source-column name;
3. generated expression display name.

The third rule is not deterministic because the generated label algorithm is unspecified. Derived tables avoid relying on it by requiring aliases for generated expressions, but top-level result metadata can still differ across implementations.

## Type and expression resolution

| Expression family | Raw state | TypeResolver/registry role | Casts | Result/error owner | Status |
|---|---|---|---|---|---|
| Integer/FLOAT/string/Boolean | Raw literal provenance | Chapter 17 | Contextual where allowed | Ch17 | Clear |
| NULL | Unresolved marker | Context matrix §17.5 | Contextual | TypeError if underconstrained | Clear |
| Unary/arithmetic/comparison | Typed operands | §§17.6–17.7 | Closed implicit edges | Ch17 | Clear |
| CASE | Branch/condition expressions | §17.9.1 | Common-type casts | Ch17 | Clear |
| IN list | Left/list expressions | §17.9.2 | Common comparison type | Ch17 | Clear |
| CAST | Explicit target type | Cast registry | Explicit cast | Ch17 | Clear |
| Scalar call | Generic raw call | Empty v1 scalar registry | Registry-selected | Ch17 | Clear result, category thin |
| Aggregate | Generic call then aggregate registry | Ch29 | Registry-selected | Ch19/29 | Placement incomplete |
| Assignment | Source + target type | Assignment matrix | Closed assignment cast | Ch17/21 | Clear |
| LIMIT/OFFSET | Expression | Integral/context rule | Exact cast policy unspecified | Ch19 | Finding |

### NULL, casts, operators, CASE, and IN

These compose correctly with Chapter 17:

- No persistent UNKNOWN TypeId is introduced.
- Standalone underconstrained NULL remains a TypeError.
- Boolean contexts do not permit integer truthiness.
- CASE and IN preserve source order and use the frozen common-type rules.
- Operator selection occurs after inserting only registry-authorized implicit casts.
- Assignment coercion remains distinct from general explicit casts.

The remaining source-span issue is that Chapter 19 does not define the span and explicit/implicit provenance retained on a binder-inserted cast node.

## Functions and aggregates

The parser/binder split remains correct:

- `count(*)`, `sum(*)`, and `foo(*)` all reach binding as generic star calls.
- The scalar registry is empty in v1.
- Aggregate resolution uses Chapter 29.
- Parser behavior remains registry-independent.

Missing or insufficiently delegated rules include:

- nested aggregate classification;
- aggregate legality in GROUP BY, ORDER BY, RETURNING, DML, and schema-default contexts;
- exact scalar-versus-aggregate conflict policy should registries ever overlap;
- direct reference to Chapter 29’s aggregate-ordinal rule.

Chapter 29 does define aggregate ordinals: ascending first aggregate source-byte occurrence per query block, retained through rewrites. Chapter 19 should reference that owner rather than leaving aggregate identity/order implicit.

## SELECT, aliases, grouping, and ordering

### Clause-alias matrix

| Clause | Input columns | Table aliases | Output aliases | Ordinals | Ambiguity policy |
|---|---|---|---|---|---|
| SELECT list | Yes | Yes | Same-list visibility undefined | No rule | Finding |
| WHERE | Yes | Yes | Undefined | No rule | Finding |
| GROUP BY | Yes | Yes | Undefined | Undefined | Finding |
| HAVING | Yes/grouped | Yes | Explicitly no | Undefined | Input ambiguity rules apply |
| ORDER BY | Yes | Yes | Yes, preferred over input | 1-based supported | Duplicate alias and ordinal syntax undefined |
| LIMIT/OFFSET | Undefined namespace, intended constant | Undefined | Undefined | No | Finding |

### Grouped-query semantics

Chapter 19 says a query is aggregate when it contains GROUP BY or aggregate expressions, and requires SELECT/HAVING expressions to derive from grouping keys, aggregates, or constants. That does not define:

- expression equivalence against a grouping key;
- whether `GROUP BY` aliases or ordinals are recognized;
- whether aggregates are forbidden in GROUP BY;
- nested aggregate rejection;
- whether HAVING without GROUP BY or any aggregate creates a global group or is invalid;
- whether functional dependencies beyond exact expressions exist.

These choices change accepted queries and bound expression identity.

### ORDER BY

The architecture clearly establishes:

- expressions, SELECT aliases, and 1-based ordinals;
- aliases take priority over input columns;
- invalid ordinals are semantic errors;
- BOOLEAN is not orderable.

It does not establish:

- which raw syntax denotes an ordinal: bare integer token, parenthesized integer, unary `+1`, constant expression, etc.;
- whether `0` and negative integer syntax are ordinal attempts or ordinary expressions;
- how duplicate output aliases are resolved.

### DISTINCT

Binding records DISTINCT as explicit logical semantics. Physical duplicate elimination belongs to Chapter 20. The binding-time type/capability requirements are not cross-referenced precisely, but no physical algorithm leaks into Chapter 19.

## LIMIT/OFFSET

The rule requires integral, nonnegative expressions constant at execution start, but “constant at execution start” is undefined.

A frozen decision must define at least:

- allowed expression classes;
- whether column-free arithmetic qualifies;
- whether immutable scalar calls would qualify;
- whether aliases/input columns are visible;
- whether NULL is rejected and by which category;
- accepted integral types and implicit-cast policy;
- overflow/range evaluation point;
- exact subquery prohibition;
- whether invalid values are binding-time or execution-start failures.

The parser correctly remains expression-general.

## DML matrix

| Statement | Target scope | Expression scope | Coercion | Duplicates | Owner/status |
|---|---|---|---|---|---|
| INSERT | Target table/columns | VALUES/SELECT; target-column visibility not explicit | Assignment rules | Duplicate targets rejected in Ch21 | Ch19/21, namespace gap |
| UPDATE | Target relation | Target row intended; qualifier exposure unclear | Assignment rules | Duplicate assignments rejected | Ch19/21, partial |
| DELETE | Target relation | Target row intended | WHERE Boolean | N/A | Ch19/21, partial |
| RETURNING | INSERT/UPDATE new row; DELETE old row | Target row image | Normal expression typing | Output duplicates unspecified | Ch15/21, binding namespace thin |

No writes or persistent IDs occur during binding, which is correct.

## DDL matrix

| Form | Name resolution | Type resolution | Duplicate policy | Semantic owner/status |
|---|---|---|---|---|
| CREATE TABLE column | New object namespace | Closed type registry | Duplicate columns rejected | Ch21; clear |
| DEFAULT | No relation references; closed immutable expression | Contextual assignment | N/A | §21.12; clear |
| PRIMARY KEY | Column lookup | Column types | Duplicate members/constraints need exact mapping | Ch21; partial |
| UNIQUE | Column lookup | Key capability | Duplicate members need exact mapping | Ch21; partial |
| CREATE INDEX | Table + ordered key columns | Indexable type capability | Duplicate key behavior preserved from parser but exact error mapping thin | Ch21; partial |
| DROP | Object namespace/kind | N/A | N/A | Lookup/kind/dependency binding insufficiently consolidated |

Transaction commands bypass relational name/type binding. VACUUM/ANALYZE resolve their target table under the catalog snapshot. EXPLAIN must bind its inner SELECT identically to an ordinary SELECT; Chapter 19 does not say this explicitly but does not contradict it.

## Subquery matrix

| Form | Child scope | Outer visibility | Output arity | Output type | Error owner |
|---|---|---|---|---|---|
| Scalar | Local child query block | Lookup detects outer but does not bind | Exactly one, Ch20 | Child column type/nullable | Bind/UnsupportedCorrelation; runtime cardinality Ch20 |
| EXISTS | Local child | Same | Any | Nonnullable BOOLEAN, Ch20 | Bind/correlation |
| IN subquery | Local child | Same | Exactly one | Common comparison type | Bind/type |
| Derived table | Local child; alias required | Same | Projection schema | Child outputs | §20.14.3 |
| Correlated | Local lookup fails, outer diagnostic lookup | Unsupported | N/A | N/A | `UnsupportedCorrelation` |

This is one of Chapter 19’s strongest areas. Remaining ambiguity concerns BindingId scope and alias shadowing during the diagnostic outer lookup.

## Catalog, SchemaVer, spans, and resources

- Binding uses the transaction-visible catalog snapshot from Chapters 9, 16, and 21.
- Own completed earlier DDL is visible through CommandId/MVCC rules.
- Other transactions’ uncommitted DDL is invisible.
- Immutable descriptors and selected SchemaVer are carried into logical planning.
- Cache order/layout cannot be semantic authority.
- Bound expressions carry SourceSpan and may retain source backing only under D18-L1.
- OutOfMemory remains distinct from source errors.
- No partial bound object may become executable.
- Binding performs no persistent mutation; §39.1 remains the transaction-consequence owner.

## Error matrix

| Condition | Required category/state | SourceSpan | Transaction owner | Priority status |
|---|---|---|---|---|
| Unknown/ambiguous column | Category not made exact | Reference span | §39.1 | Finding |
| Unknown qualifier/member | Distinction explicit; category not | Qualifier/member span | §39.1 | Finding |
| Unknown table/index/object kind | Catalog/Bind split unclear | Object-name span | §39.1 | Finding |
| Unknown type | TypeError expected from Ch17 | Type-name span | §39.1 | Mostly clear |
| Invalid operator/cast/common type | TypeError | Responsible expression | §39.1 | Clear locally |
| Aggregate placement/grouping | Exact category unclear | Aggregate/group expression | §39.1 | Finding |
| Invalid ORDER ordinal | “Semantic error” only | Ordinal span | §39.1 | Finding |
| Invalid LIMIT/OFFSET | SQL error category/timing unclear | Count expression | §39.1 | Finding |
| Unsupported correlation | UnsupportedCorrelation | Outer-reference span | §39.1 | Clear |
| Derived output-shape failure | Bind/semantic error via Ch20 | Responsible output/subquery | §39.1 | Mostly clear |
| Allocation failure | OutOfMemory | Existing originating syntax if applicable | §39.1 | Clear |
| Operational guard | FrontEndResourceLimit where applicable | Existing originating syntax | §39.1 | Clear |

The architecture does not define which error wins when multiple independently detectable binding failures exist. Source-order traversal is plausible but not frozen; catalog/hash iteration must not decide it.

## Determinism and implementation coupling

Same AST plus catalog snapshot is intended to bind deterministically, and wildcard/column candidate rules support that intent. The unresolved alias, grouping, ordinal, and error-priority policies prevent the invariant from being fully true.

No mandate was found for visitors, recursive walks, hash maps, symbol-table classes, multi-pass binding, or in-place annotation.

One local implementation-coupling issue remains in §19.7:

- arena and shared immutable nodes are correctly allowed;
- advice to avoid per-node ref-count/allocation churn based on measurement belongs more naturally in DEVELOPMENT or VERIFICATION unless retained solely as non-normative performance rationale.

Locale, Unicode-library, pointer-address, allocator-layout, and native catalog-iteration order must not affect binding. Canonical bytes are already frozen by Chapters 16/18.

## Temporality audit

Complete Chapter-19 project-time classifications:

| Phrase | Section | Class | Assessment |
|---|---|---|---|
| “future architecture” | 19.2 | E — project chronology | Finding |
| “planning later assigns” | 19.3 | D — cross-layer navigation | Valid |
| “Initial kinds include” | 19.6 | E — implementation/scope chronology | Finding |
| “later vectorized evaluation” | 19.6 | D — downstream compatibility | Valid |
| “retained for later versions” | 19.9 | E — roadmap | Finding |
| “When later registered” | 19.9 | E — roadmap | Finding |
| “Initial aggregate expressions” | 19.10 | E — temporal v1 framing | Finding |
| “initial … signatures” | 19.10 | E — temporal v1 framing | Finding |
| “execution does not choose … later” | 19.10 | D — stage ordering | Valid |
| “later planning/execution” | 19.13 | D — ownership navigation | Valid |
| “planner later represents” | 19.14 | D — ownership navigation | Valid |
| “Later execution may choose” | 19.17 | D — runtime ownership | Valid |
| “deferred from the initial parser target” | 19.19 | E — project chronology | Finding |
| “future parameter typing” | 19.19 | E — roadmap | Finding |
| “Future parameter typing…” | 19.20 | E — roadmap | Finding |

Result: meaningful project chronology remains. Chapter 19 is mostly timeless but does not yet pass the strict time-independence objective.

## Document ownership assessment

| Check | Result |
|---|---|
| Current implementation narration | None |
| Phase-2 narration | None |
| Development sequencing | No sequence, but §19.7 contains implementation guidance |
| Verification recipes | None |
| PROJECT_STATE leakage | None |
| Review/devlog history | None |
| Presentation/UI leakage | Generated display-name semantics are underspecified, not historical |
| External SQL dialect authority | None |
| Parser-algorithm mandate | None |
| Bound representation freedom | Preserved |
| Allocation representation freedom | Preserved |
| Persistent-layout leakage | None |
| Physical-plan algorithm leakage | None |
| Correct semantic owner references | Mixed; several missing/loose references |
| Architecture remains analytical | Mostly |
| Architecture remains timeless | Not completely |
| Canonical v1 readability | Blocked by semantic gaps |
| Current source layout dependency | None |
| Current tests dependency | None |
| Milestone dependency | None |
| History dependency | None |

## Analytical depth

| Mechanism | Assessment |
|---|---|
| BindingId vs TableId | Rationale present, exact domain incomplete |
| Unqualified ambiguity | Analytically sufficient |
| Qualified lookup | Semantically incomplete |
| Wildcard order | Analytically sufficient |
| Alias namespace | Analytical-depth finding |
| Output names | Semantically clear except generated/duplicate cases |
| TypeResolver centralization | Analytically sufficient |
| NULL contextual resolution | Sufficient through Chapter 17 |
| Aggregate registry | Clear composition, placement thin |
| Grouped-query legality | Analytical-depth finding |
| ORDER alias priority | Clear, collision handling missing |
| LIMIT constant | Analytical-depth finding |
| Correlation classification | Analytically sufficient |
| DML/DDL scopes | Semantically thin |
| Error determinism | Analytical-depth finding |

## Terminology

| Term | Canonical meaning | Assessment |
|---|---|---|
| raw AST | Chapter-18 unbound syntax/provenance | Clear |
| bind | Resolve names/types/semantic form without planning | Clear |
| scope | Visible relation/alias namespace in a query block | Alias details incomplete |
| relation binding | One visible relation occurrence | Clear concept |
| BindingId | Query-local occurrence identity | Domain incomplete |
| TableId | Persistent catalog table identity | Clear |
| ColumnId | Persistent column identity within descriptor | Clear |
| alias | Source-defined alternate visible name | Visibility/hiding incomplete |
| qualifier | First component of qualified column/star | Candidate namespace incomplete |
| output alias | Explicit SELECT output name used by ORDER BY | Duplicate handling incomplete |
| output name | Result metadata/derived-table export name | Generated rule incomplete |
| query block | Local SELECT binding domain | Mostly clear |
| correlated/outer reference | Name requiring parent scope | Clear classification |
| unresolved type marker | Nonpersistent contextual NULL state | Clear |
| TypeId | Closed persistent semantic type identity | Clear |
| wildcard | `*` or qualified `x.*` expansion | Clear |
| bound expression | Immutable typed semantic expression | Clear |
| schema | Catalog schema or query output schema depending context | Understandable but overloaded |

Normative language is strong where used, but several correctness-significant rules are descriptive rather than closed: alias uniqueness, visibility, group equivalence, ordinal recognition, constants, and error priority.

## Explicit Chapter-19 cross-references

| Source | Target | Purpose | Exists/correct owner | Precision | Status |
|---|---|---|---|---|---|
| 19.2 | §19.18 | Correlation diagnostic | Yes | Precise | Good |
| 19.8 | §§17.6–17.7 | Operator/cast registry | Yes | Precise | Good |
| 19.9 | §17.9.3 | Empty scalar-function registry | Yes | Precise | Good |
| 19.9 | §21.12 | Persisted defaults | Yes | Precise | Good |
| 19.10 | Chapter 29 | Aggregate registry/execution | Yes | Broad | Acceptable but improve |
| 19.10 | §29.3 | Aggregate signatures/nullability | Yes | Precise | Good |
| 19.13 | “Chapter-17 SQL-orderable types” | ORDER type domain | Owner exists | Vague | Minor issue |
| 19.16 | §17.9.1 | CASE semantics | Yes | Precise | Good |
| 19.16 | §17.7.3 | Evaluation order | Yes | Precise | Good |
| 19.17 | §17.9.2 | IN semantics | Yes | Precise | Good |
| 19.17 | §17.7.3 | Once-only/source-order evaluation | Yes | Precise | Good |
| 19.18 | §20.14 | Closed subquery forms | Yes | Precise | Good |
| 19.18 | §20.14.3 | Derived-table output names | Yes | Precise | Good |
| 19.20 | §§17.2–17.10 | Closed scalar semantics | Yes | Broad but stable | Acceptable |

Missing useful exact references include Chapter 29’s aggregate-ordinal rule, Chapter 21’s DML/DDL binding sections, §21.12 from relevant DDL prose, Chapter 16’s snapshot/descriptor rules, §20.10 for DISTINCT, and §39.2–§39.3 for exact error/resource ownership.

## Cross-chapter composition

| Owner | Chapter-19 handoff | Status |
|---|---|---|
| Chapter 16 | Names → stable IDs/descriptors/SchemaVer | Compatible |
| Chapter 17 | Literal/type/operator/cast/function semantics | Compatible |
| Chapter 18 | Raw AST, canonical bytes, spans, lifetime | Compatible |
| Chapter 19 | Binding identity/scopes/resolution | Incomplete |
| Chapter 20 | Logical slots/plans/subquery semantics | Compatible where referenced |
| Chapter 21 | DDL/DML binding/publication/defaults | Compatible but under-referenced |
| Chapter 29 | Aggregate descriptors/ordinals/execution | Compatible but ordinal handoff under-referenced |
| §39 | Error categories/transaction consequences/resources | Category/priority handoff incomplete |

## Technical-consistency matrix — 160 live questions

Status: `C` consistent; `S` consistent but delegated/specialized; `F` finding.

### 1–40: input, identity, scopes, and names

| # | Correctness question | Status |
|---:|---|---|
| 1 | Does binding consume Chapter-18 raw AST without redefining grammar? | C |
| 2 | Does raw AST remain unbound before Chapter 19? | C |
| 3 | Are source spans available to binding? | C |
| 4 | Are canonical identifier bytes preserved? | C |
| 5 | Does binding avoid physical-plan selection? | C |
| 6 | Does binding avoid page/RID access? | C |
| 7 | Are typed bound expressions produced before planning? | C |
| 8 | Are implicit casts represented semantically? | C |
| 9 | Is bound expression immutability required? | C |
| 10 | Is retained backing permitted under D18-L1? | C |
| 11 | Is BindingId distinct from TableId? | C |
| 12 | Does each self-join occurrence receive a distinct BindingId? | C |
| 13 | Can two references share TableId but differ in BindingId? | C |
| 14 | Is BindingId nonpersistent? | C |
| 15 | Is BindingId uniqueness domain exact? | F:M1 |
| 16 | Is BindingId reuse across nested blocks defined? | F:M1 |
| 17 | Is BindingId stable through logical rewrites? | F:M1 |
| 18 | Is native pointer identity excluded? | S |
| 19 | Does a bound column carry BindingId and ColumnId? | C |
| 20 | Can ColumnId alone distinguish self-join occurrences? | C: explicitly no by model |
| 21 | Does every SELECT create a local binding scope? | C |
| 22 | Are parent links nonbinding in v1? | C |
| 23 | Is outer lookup used only for correlation diagnosis? | C |
| 24 | Is relation-alias uniqueness defined? | F:B1 |
| 25 | Is alias shadowing defined? | F:B1 |
| 26 | Does an alias hide the base table qualifier? | F:B1 |
| 27 | Are aliases compared using canonical bytes? | S |
| 28 | Can alias and unaliased table names collide deterministically? | F:B1 |
| 29 | Is same-SELECT-list alias visibility defined? | F:B3 |
| 30 | Is subquery alias scope local? | C |
| 31 | Do unqualified names use all visible local bindings? | C |
| 32 | Does zero-match produce unknown column? | C |
| 33 | Does one-match bind deterministically? | C |
| 34 | Does multiple-match produce ambiguity? | C |
| 35 | Is first-match behavior excluded? | C |
| 36 | Does qualified lookup resolve qualifier before member? | C |
| 37 | Are unknown qualifier and unknown member distinct? | C |
| 38 | Is qualifier candidate namespace exact? | F:B1 |
| 39 | Is alias-versus-base-name precedence exact? | F:B1 |
| 40 | Is catalog iteration prevented from selecting a column candidate? | C |

### 41–80: wildcards, output, types, operators, and calls

| # | Correctness question | Status |
|---:|---|---|
| 41 | Is unqualified wildcard expansion order exact? | C |
| 42 | Is relation order based on source-visible FROM order? | C |
| 43 | Is column order descriptor presentation order? | C |
| 44 | Is ColumnId numeric order rejected as wildcard order? | S |
| 45 | Does qualified star expand one binding? | C |
| 46 | Is no-FROM star rejected? | C |
| 47 | Are duplicate wildcard output names preserved? | S |
| 48 | Is duplicate top-level output-name legality explicit? | F:B2 |
| 49 | Is explicit AS the first output-name rule? | C |
| 50 | Is direct-column source name the second rule? | C |
| 51 | Is generated-expression display naming deterministic? | F:M2 |
| 52 | Are generated labels nonsemantic for derived-table lookup? | S |
| 53 | Does output metadata include type and nullability? | C |
| 54 | Are derived-table names delegated to §20.14.3? | C |
| 55 | Are duplicate derived output names rejected? | C |
| 56 | Must generated derived outputs receive explicit aliases? | C |
| 57 | Are integer literals delegated to Chapter 17? | C |
| 58 | Are FLOAT literals delegated to Chapter 17? | C |
| 59 | Are string/Boolean literals delegated? | C |
| 60 | Does NULL begin unresolved rather than with a persistent TypeId? | C |
| 61 | Is standalone underconstrained NULL rejected? | C |
| 62 | Can assignment context resolve NULL? | C |
| 63 | Can CASE/IN common type resolve NULL? | C |
| 64 | Is `NULL = NULL` rejected as underconstrained? | C |
| 65 | Is UNKNOWN excluded from persistence? | C |
| 66 | Does TypeResolver own overload selection? | C |
| 67 | Are implicit casts restricted to the closed graph? | C |
| 68 | Are assignment casts distinct from general casts? | C |
| 69 | Is inserted-cast target type exact? | C |
| 70 | Is inserted-cast source span exact? | F:N4 |
| 71 | Are unary operators registry-resolved? | C |
| 72 | Are arithmetic operators registry-resolved? | C |
| 73 | Are comparisons registry-resolved? | C |
| 74 | Is integer truthiness excluded? | C |
| 75 | Must WHERE/JOIN/HAVING/CASE WHEN bind BOOLEAN? | C |
| 76 | Is CASE common typing delegated precisely? | C |
| 77 | Is IN common comparison typing delegated precisely? | C |
| 78 | Is the scalar function registry empty in v1? | C |
| 79 | Does generic call syntax remain parser-independent? | C |
| 80 | Is unknown/arity/star-call error categorization exact? | F:M5 |

### 81–120: aggregates, SELECT clauses, LIMIT, and DML

| # | Correctness question | Status |
|---:|---|---|
| 81 | Are COUNT/SUM/MIN/MAX/AVG delegated to Chapter 29? | C |
| 82 | Is COUNT(*) distinguished during binding? | C |
| 83 | Is SUM(*) routed to semantic rejection rather than parser rejection? | C |
| 84 | Is `foo(*)` routed to registry failure? | C |
| 85 | Is scalar/aggregate classification deterministic for v1 registries? | S |
| 86 | Are aggregates forbidden in WHERE? | C |
| 87 | Are aggregates forbidden in JOIN ON? | C |
| 88 | Are nested aggregates explicitly rejected? | F:B4 |
| 89 | Are aggregates in GROUP BY explicitly rejected or defined? | F:B4 |
| 90 | Are aggregates in RETURNING/default/DML contexts explicit? | F:B4/M3 |
| 91 | Is aggregate-query classification defined for GROUP BY? | C |
| 92 | Is aggregate-query classification defined for aggregate calls? | C |
| 93 | Is HAVING-only query classification defined? | F:B4 |
| 94 | Is grouping-key expression equivalence exact? | F:B4 |
| 95 | Are functional dependencies supported or excluded explicitly? | F:B4 |
| 96 | Are grouping aliases defined? | F:B3/B4 |
| 97 | Are grouping ordinals defined? | F:B4 |
| 98 | Is HAVING SELECT-alias visibility explicitly denied? | C |
| 99 | Is HAVING without GROUP/aggregate defined? | F:B4 |
| 100 | Is aggregate ordinal assigned by the canonical Ch29 rule? | S:N3 missing reference |
| 101 | Is ORDER BY expression binding allowed? | C |
| 102 | Are explicit output aliases visible in ORDER BY? | C |
| 103 | Does output alias beat input column on a unique match? | C |
| 104 | Are duplicate output-alias matches deterministic? | F:B2 |
| 105 | Are 1-based ordinals supported? | C |
| 106 | Is ordinal raw-syntax recognition exact? | F:B5 |
| 107 | Is `ORDER BY 0` classified exactly? | F:B5 |
| 108 | Is `ORDER BY -1` classified exactly? | F:B5 |
| 109 | Is `ORDER BY (1)` classified exactly? | F:B5 |
| 110 | Is BOOLEAN ordering rejected? | C |
| 111 | Is DISTINCT retained as logical semantics? | C |
| 112 | Is DISTINCT execution delegated to Chapter 20? | C |
| 113 | Are LIMIT/OFFSET expressions accepted syntactically? | C |
| 114 | Must LIMIT/OFFSET be integral? | C |
| 115 | Must LIMIT/OFFSET be nonnegative? | C |
| 116 | Is execution-start constant defined structurally? | F:B6 |
| 117 | Are column-free arithmetic expressions classified? | F:B6 |
| 118 | Are NULL and overflow outcomes classified? | F:B6/M5 |
| 119 | Are LIMIT/OFFSET namespaces and aliases defined? | F:B3/B6 |
| 120 | Is subquery prohibition in count expressions precisely owned? | F:B6 |

### 121–160: DML/DDL, subqueries, catalog, errors, and document model

| # | Correctness question | Status |
|---:|---|---|
| 121 | Does INSERT resolve target table and columns? | C |
| 122 | Are duplicate INSERT targets rejected? | C |
| 123 | Are omitted columns/defaults handled by Ch21? | C |
| 124 | Is VALUES target-row column visibility explicit? | F:M3 |
| 125 | Is INSERT SELECT assignment coercion defined? | C |
| 126 | Does UPDATE resolve each assignment target? | C |
| 127 | Are duplicate UPDATE targets rejected? | C |
| 128 | Is UPDATE target qualifier visibility exact? | F:M3 |
| 129 | Does DELETE expose only its target row? | S |
| 130 | Is DELETE target qualifier visibility exact? | F:M3 |
| 131 | Are RETURNING row images defined? | C |
| 132 | Is RETURNING qualifier namespace exact? | F:M3 |
| 133 | Does CREATE TABLE resolve type names through Ch17? | C |
| 134 | Are duplicate column names rejected? | C |
| 135 | Are PK/UNIQUE member lookups defined? | C |
| 136 | Are duplicate constraint-member outcomes categorized? | F:M4/M5 |
| 137 | Is DEFAULT’s closed expression set defined? | C |
| 138 | Does CREATE INDEX preserve key order? | C |
| 139 | Is duplicate index-key policy/error exact? | F:M4 |
| 140 | Is DROP object-kind/dependency binding exact? | F:M4 |
| 141 | Do scalar subqueries bind in local child scope? | C |
| 142 | Is scalar subquery one-column shape delegated? | C |
| 143 | Is scalar runtime row cardinality left to Ch20? | C |
| 144 | Does EXISTS produce the Ch20 Boolean result? | C |
| 145 | Is IN-subquery one-column/common-type binding delegated? | C |
| 146 | Are derived-table output rules delegated? | C |
| 147 | Are data-modifying subqueries already parser-rejected? | C |
| 148 | Does local failure plus outer match produce UnsupportedCorrelation? | C |
| 149 | Does absence everywhere produce ordinary unknown-name error? | C |
| 150 | Is alias shadowing during correlation diagnosis exact? | F:B1 |
| 151 | Is binding snapshot visibility frozen elsewhere? | C |
| 152 | Is own earlier DDL visibility frozen elsewhere? | C |
| 153 | Is descriptor/SchemaVer identity stable for the statement? | S |
| 154 | Can catalog cache order redefine binding? | C: prohibited by owners |
| 155 | Are exact binder error categories mapped? | F:M5 |
| 156 | Is multi-error precedence deterministic? | F:B7 |
| 157 | Is OutOfMemory preserved as cross-layer category? | C |
| 158 | Is partial executable bound output prohibited? | C |
| 159 | Is Chapter 19 free of project chronology? | F:N1 |
| 160 | Is Chapter 19 implementation-technique independent? | F:N2, otherwise mostly yes |

Totals:

| Result | Count |
|---|---:|
| CONSISTENT | 96 |
| CONSISTENT BUT SPECIALIZED/DELEGATED | 18 |
| FINDING | 46 |
| N/A | 0 |

The 46 finding-marked questions collapse into the 16 non-overlapping findings below; they are not 46 separate architecture issues.

## Findings

### BLOCKING

#### F19-B1 — Alias and qualifier namespace is not closed

- Sections: §§19.2, 19.4.2, 19.5, 19.18
- Evidence: scopes contain aliases and qualified references first resolve a qualifier, but alias uniqueness, base-name hiding, collisions, and shadowing are unspecified.
- Type: ALIAS SEMANTICS
- Scope: Cross-section
- Consequence: `FROM t AS x`, duplicate aliases, self-joins, and correlation diagnosis may accept/reject or resolve differently.
- Canonical comparison: Chapter 18 freezes alias syntax, but not binding meaning.
- Correct owner: Chapter 19.
- Future action: Freeze qualifier namespace, alias uniqueness, hiding, collision, and shadowing rules.

#### F19-B2 — Duplicate output aliases and ORDER BY resolution are ambiguous

- Sections: §§19.5, 19.13
- Evidence: output aliases are accepted and take priority in ORDER BY, but multiple matching output aliases have no result.
- Type: OUTPUT SCHEMA
- Scope: Cross-section
- Consequence: Same query can bind ORDER BY to different output slots or fail depending on implementation.
- Correct owner: Chapter 19.
- Future action: Freeze duplicate-output-name legality and ORDER BY multiple-alias policy.

#### F19-B3 — SELECT alias visibility is incomplete

- Sections: §§19.2, 19.11–19.14
- Evidence: aliases are visible “where SQL semantics allow”; only HAVING denial and ORDER BY visibility are explicit.
- Type: CLAUSE VISIBILITY
- Scope: Cross-section
- Consequence: Same AST can bind differently in SELECT, WHERE, GROUP BY, LIMIT, or OFFSET.
- Correct owner: Chapter 19.
- Future action: Freeze a complete clause-visibility matrix.

#### F19-B4 — Grouped-query and HAVING legality is incomplete

- Sections: §§19.10–19.12
- Evidence: “derivable from grouping keys” lacks exact equivalence; GROUP alias/ordinal, nested aggregates, aggregate placement, functional dependency, and HAVING-only cases are undefined.
- Type: AGGREGATE RESOLUTION
- Scope: Cross-section
- Consequence: Different accepted languages and bound trees.
- Correct owner: Chapter 19, with Chapter 29 referenced for aggregate descriptors.
- Future action: Freeze grouped-expression equivalence, placement, aliases/ordinals, and HAVING-only behavior.

#### F19-B5 — ORDER BY ordinal recognition is undefined

- Section: §19.13
- Evidence: 1-based ordinals are supported, but the raw syntax recognized as an ordinal is not identified.
- Type: CLAUSE VISIBILITY
- Scope: Local
- Consequence: `1`, `(1)`, `+1`, `0`, and `-1` can receive different meanings.
- Correct owner: Chapter 19.
- Future action: Freeze the exact ordinal syntactic form and invalid-range behavior.

#### F19-B6 — Execution-start constant is undefined

- Section: §19.14
- Evidence: LIMIT/OFFSET must be “constant at execution start,” with no closed expression domain or failure timing.
- Type: SEMANTIC COMPLETENESS
- Scope: Local/cross-layer
- Consequence: Different binders accept different count expressions and classify errors at different stages.
- Correct owner: Chapter 19, with execution timing delegated precisely.
- Future action: Freeze qualifying expressions, type/cast/NULL/range rules, and error timing.

#### F19-B7 — Observable binding-error precedence is undefined

- Sections: Chapter-wide, especially §§19.4, 19.10–19.14
- Evidence: no rule determines which independent binding error wins.
- Type: ERROR SEMANTICS
- Scope: Cross-section
- Consequence: Traversal, hash iteration, or catalog layout can change the returned error.
- Correct owner: Chapter 19 with §39 categories.
- Future action: Freeze a deterministic priority, likely responsible source order plus explicit structural precedence where necessary.

### MAJOR

#### F19-M1 — BindingId domain and stability are incomplete

- Section: §19.2
- Evidence: “query-local” does not identify query block versus whole statement, reuse, or rewrite stability.
- Type: BINDING IDENTITY
- Consequence: Nested scopes and downstream slot mapping require invention.
- Future action: Define allocation domain, uniqueness, reuse prohibition, and semantic lifetime.

#### F19-M2 — Generated output display names are nondeterministic

- Section: §19.5
- Evidence: “generated expression display name” has no derivation rule.
- Type: OUTPUT SCHEMA
- Consequence: Observable top-level metadata can vary.
- Future action: Define deterministic labels or explicitly delegate them to a presentation contract while excluding them from semantic lookup.

#### F19-M3 — DML expression and target qualifier scopes are incomplete

- Sections: §19.1 plus Chapter 21 handoff
- Evidence: target/column binding is claimed, but target-name visibility in VALUES, UPDATE/DELETE predicates, and RETURNING is not closed.
- Type: DML BINDING
- Consequence: Qualified and target-referencing expressions can bind differently.
- Future action: Freeze each DML scope and reference Chapter 15/21 row-image semantics.

#### F19-M4 — DDL/index/drop binding is not fully closed

- Sections: §19.1 and delegated Chapter 21 areas
- Evidence: duplicate index keys, constraint-member duplication, DROP object-kind/dependency lookup, and related semantic error ownership are not consolidated.
- Type: DDL BINDING
- Consequence: Different binders can reject at different stages/categories.
- Future action: Close the DDL binding matrix in Chapter 19 or delegate each row precisely to Chapter 21.

#### F19-M5 — Binder error categories are not mapped exactly

- Sections: Chapter-wide
- Evidence: “unknown,” “ambiguous,” “semantic error,” and invalid count/group cases are not consistently assigned to §39 categories.
- Type: ERROR SEMANTICS
- Consequence: BindError/TypeError/CatalogError/ConstraintDefinitionError boundaries vary.
- Future action: Add an exact binder error-category table referencing §39.

### MINOR

#### F19-N1 — Project-time and roadmap wording remains

- Sections: §§19.2, 19.6, 19.9, 19.10, 19.19, 19.20
- Type: TEMPORALITY
- Consequence: Architecture reads partly as project sequencing rather than canonical v1.
- Future action: Replace with timeless v1 scope or remove speculation.

#### F19-N2 — Local implementation/performance guidance leaks into architecture

- Section: §19.7
- Evidence: per-node allocation/ref-count advice.
- Type: IMPLEMENTATION COUPLING
- Consequence: Blurs WHAT versus HOW; no semantic defect.
- Future action: Retain only lifetime/immutability freedom; move durable optimization guidance to DEVELOPMENT/VERIFICATION if desired.

#### F19-N3 — Cross-references are incomplete or overly broad

- Sections: §§19.3, 19.10, 19.13–19.15, 19.20
- Type: CROSS-REFERENCE
- Consequence: Owners such as aggregate ordinal, DISTINCT, catalog snapshot, and DML/DDL binding are harder to identify.
- Future action: Add exact stable references without duplicating their semantics.

#### F19-N4 — Binder-inserted cast span/provenance is underspecified

- Sections: §§19.6, 19.8
- Type: SOURCE SPAN
- Consequence: Diagnostics or later semantic rewrites may choose differing responsible spans.
- Future action: State which represented source construct supplies an implicit cast’s span and whether implicit origin is retained.

No optional editorial-only findings were identified.

## Frozen architecture semantic questions

1. What is the exact alias/qualifier namespace, including duplicate aliases, base-name hiding, collisions, and shadowing?
2. Are duplicate SELECT output names allowed, and how does ORDER BY handle multiple matching aliases?
3. Which clauses can see SELECT output aliases?
4. What constitutes equality with a GROUP BY key, and are GROUP aliases or ordinals supported?
5. Is HAVING without GROUP BY or an aggregate valid?
6. Where are aggregates and nested aggregates legal within one query block?
7. Which exact raw syntax denotes an ORDER BY ordinal?
8. What is the closed execution-start-constant domain for LIMIT/OFFSET?
9. What is BindingId’s uniqueness domain and semantic lifetime?
10. How are generated output display names derived?
11. What are the exact DML relation/row-image namespaces?
12. Which §39 error category owns each binder failure?
13. Which error wins when multiple binder failures are independently present?
14. What span/provenance does an inserted implicit cast retain?

These are not safely repairable as document-only wording choices.

## Verification cross-check and follow-up gaps

Existing binder verification is useful but cannot serve as architecture authority.

| Verification family | Status |
|---|---|
| BindingId self-join distinction | PARTIAL — uniqueness domain blocked |
| Unqualified unknown/ambiguous column | COMPLETE |
| Qualified alias/base-name resolution | BLOCKED BY B1 |
| Wildcard expansion order | COMPLETE |
| Output-name generation/duplicates | BLOCKED BY B2/M2 |
| Type-name/literal/NULL resolution | COMPLETE |
| Cast/operator registry binding | COMPLETE, inserted-span detail partial |
| Function/star-call handoff | PARTIAL — exact error categories missing |
| Aggregate signatures/types | COMPLETE through Ch29 |
| Aggregate placement/grouping/HAVING | BLOCKED BY B4 |
| Aggregate ordinal | PARTIAL — direct binder handoff not explicit |
| Clause alias visibility | BLOCKED BY B3 |
| ORDER alias/ordinal | BLOCKED BY B2/B5 |
| LIMIT/OFFSET | BLOCKED BY B6 |
| Basic DML target/coercion/default binding | PARTIAL |
| Exact DML namespaces | BLOCKED BY M3 |
| DDL/default/index/drop binding | PARTIAL/MISSING |
| Subquery shape/correlation | Mostly COMPLETE |
| Error SourceSpan selection | COMPLETE except inserted-cast detail |
| Error category matrix | BLOCKED BY M5 |
| Multi-error deterministic precedence | MISSING/BLOCKED BY B7 |
| Catalog/hash-order independence | PARTIAL |

Reusable methodology already exists for immutable mock descriptors, snapshot-aware catalogs, typed-AST inspection, independent SourceSpan oracles, subquery matrices, and deterministic injected resource failures.

## Previous-chapter regression

- Chapter 16 canonical names, stable IDs, SchemaVer, and descriptors remain unchanged.
- Chapter 17 TypeIds 1–7, unresolved NULL marker, casts, operators, and function semantics remain unchanged.
- Chapter 18 parser/catalog independence, raw unbound AST, object/type syntax, SourceSpan, grammar, lifetime, and resource semantics remain unchanged.
- Chapter 19 does not retroactively redefine the frozen grammar.
- No Chapter-18 regression was found.

## Chapter 20 boundary

Chapter 20 begins at [line 15812](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15812) with:

`20. Logical Plans, Properties, and Rewrites`

Principal handoff:

- Chapter 19 produces typed immutable bound expressions, relation occurrences, and output metadata.
- Chapter 20 assigns logical slots, constructs immutable logical relational operators, owns subquery result-shape semantics, and applies logical rewrites.

Explicit Chapter-19 → Chapter-20 references:

- §19.18 → §20.14 for closed uncorrelated subquery forms.
- §19.18 → §20.14.3 for derived-table naming and output binding.

A future Chapter-20 review should cover logical slot identity, operator schemas/properties, rewrite semantic preservation, outer-join nullability, no-FROM plans, subquery operators/cardinality, DISTINCT, LIMIT/OFFSET logical placement, and rewrite determinism. That review was not started.

## Direct answers

| Question | Answer |
|---|---|
| Any project-time/current-state wording? | Yes: project-time wording; no current implementation narration |
| Any DEVELOPMENT-owned material? | Yes, limited §19.7 allocation guidance |
| Any VERIFICATION recipe? | No |
| Any PROJECT_STATE material? | No |
| Any history/devlog material? | No |
| Any BindingId ambiguity? | Yes |
| Any scope ambiguity? | Yes |
| Any alias visibility ambiguity? | Yes |
| Any column-resolution ambiguity? | Qualified alias/base-name namespace only |
| Any wildcard-order ambiguity? | No; order is deterministic |
| Any output-name ambiguity? | Yes |
| Any TypeResolver ownership ambiguity? | No material owner ambiguity |
| Any NULL/context ambiguity? | No material ambiguity |
| Any function/aggregate ambiguity? | Aggregate placement yes; registry handoff mostly clear |
| Any SELECT alias visibility ambiguity? | Yes |
| Any ORDER BY alias/ordinal ambiguity? | Yes |
| Any GROUP BY/HAVING ambiguity? | Yes |
| Any LIMIT/OFFSET constant ambiguity? | Yes |
| Any DML binding ambiguity? | Yes |
| Any DDL/default ambiguity? | DDL details yes; DEFAULT expression domain itself is defined |
| Any subquery/correlation ambiguity? | Mostly no; alias shadowing/BindingId scope remain |
| Any error-category/precedence ambiguity? | Yes |
| Any catalog-order nondeterminism? | Architecture intends no, but error priority/candidate namespaces remain incomplete |
| Any implementation-technique overconstraint? | Minor §19.7 guidance only |
| Any correctness-relevant implementer invention? | Yes |
| Can Chapter 19 stand as timeless canonical v1 architecture? | Not yet |

Recommended next action: **frozen semantic architecture review required**, followed by targeted document cleanup and then Chapter-19 verification synchronization.

## Repository and read-only guarantee

Initial state:

- Status: clean
- Index: clean
- HEAD: `99c77a2cef6f5a742121c358ce813b045ccc3ae7`

Final state:

- Status: clean
- Index: clean
- HEAD: `99c77a2cef6f5a742121c358ce813b045ccc3ae7`
- `git diff --check`: passed with no output

Confirmations:

- Files modified by audit: **NONE**
- Repository-state change caused by audit: **NONE**
- No review artifact was read, modified, created, or staged
- No build, test, benchmark, staging, commit, or implementation work occurred
- Chapter 20 direct review: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
