# Chapter 19 semantic-integration verdict

**PARTIALLY INTEGRATED — TWO NEW FROZEN ARCHITECTURE SEMANTIC CONFLICTS**

Fifteen decisions were fully integrated. D19-S10, D19-S15, and D19-S16 are partially integrated but cannot be closed without resolving two direct conflicts with frozen Chapter 17 rules.

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15419) was modified, exclusively within Chapter 19.

## Frozen semantic conflicts

### Conflict C19-1 — Unknown call error category

- Accepted decisions: D19-S15/D19-S16 require missing function name/shape/arity, including `foo(*)`, to produce `BindError`.
- Frozen owner: [§17.9.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13888) requires every unsupported scalar-function call to fail binding with `TYPE_ERROR`.
- Consequence: `foo()`, `foo(*)`, and potentially an aggregate-shaped call with no matching descriptor cannot receive both categories.
- Integrated compatible portion: generic registry lookup, star authorization, unique descriptor requirement, argument-type `TypeError`, aggregate lookup, and parser independence.
- Smallest decision required: either revise §17.9.3 to distinguish missing name/shape/arity (`BindError`) from argument incompatibility (`TypeError`), or revise D19-S15/S16 to retain `TypeError` for unsupported scalar calls.

### Conflict C19-2 — LIMIT/OFFSET evaluation timing

- Accepted D19-S10 requires the expression to be evaluated exactly once at execution start.
- Frozen owner: [§17.10.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13929) mandates folding every fully constant v1 scalar subtree and raises dominating constant errors such as root `1/0` during binding.
- Consequence: the original constant expression may be evaluated or fail during binding rather than solely at execution start.
- Integrated compatible portion: closed constant domain, INT32/INT64 typing, INT64 normalization, NULL context, dependency rejection, residual execution-start evaluation, and final NULL/negative `ExecutionError`.
- Current Chapter-19 wording preserves §17.10.2: after successful required binding/folding, the residual count expression is obtained once before relational row processing.
- Smallest decision required: exempt LIMIT/OFFSET from §17.10.2 folding/error timing, or explicitly approve the residual-expression formulation now documented.

No other cross-owner conflict was found.

## Initial repository state

- Status: clean
- Index: clean
- HEAD: `11cb88e9874fbd76317e4ea778d39e96f94a23a0`
- Pre-existing tracked/untracked changes: none
- Review artifacts: not read, modified, moved, created, or staged

## Sections modified

- §19.1
- §19.2
- New §§19.2.1–19.2.2
- §19.3
- §§19.4.1–19.4.2
- New §§19.4.3–19.4.4
- §§19.5–19.6
- §§19.9–19.15
- §19.18
- §19.20
- New §§19.20.1–19.20.3

Sections 19.7, 19.8, 19.16, 19.17, and 19.19 were left substantively unchanged.

## Integrated binding semantics

| Decision | Integrated result | Status |
|---|---|---|
| D19-S1 | One canonical local qualifier per relation; aliases hide base names; duplicate qualifiers are `BindError`; aliased self-joins remain legal | CLOSED |
| D19-S2 | Local match/ambiguity dominates; outer lookup is diagnostic-only after zero local candidates; local qualifier shadows outer qualifier | CLOSED |
| D19-S3 | Duplicate top-level names/aliases legal as distinct slots; ORDER BY searches explicit `AS` aliases only; multiple matches are `BindError` | CLOSED |
| D19-S4 | SELECT aliases visible only in ORDER BY; no GROUP BY alias or ordinal shortcut | CLOSED |
| D19-S5 | Derived tables are non-lateral; JOIN ON sees accumulated left plus current right; completed FROM scope is precisely defined | CLOSED |
| D19-S6 | Aggregate-query classification, global group, HAVING legality, aggregate placement, and same-block nesting rules closed | CLOSED |
| D19-S7 | Group-key matching uses fully bound structural equality; no algebraic or functional-dependency inference | CLOSED |
| D19-S8 | Only a complete bare unsigned integer raw node is an ORDER ordinal; range uses lossless magnitude; resolution order frozen | CLOSED |
| D19-S9 | Execution-start constant domain and forbidden dependencies closed | CLOSED |
| D19-S10 | Typing, NULL context, final validation, and residual timing integrated; exclusive execution-start evaluation conflicts with §17.10.2 | PARTIAL/BLOCKED |
| D19-S11 | BindingId unique across the whole bound statement, nonreusable within it, runtime-only, rewrite-stable | CLOSED |
| D19-S12 | Generated display name is the exact original source slice; not an alias; D18-L1 governs backing | CLOSED |
| D19-S13 | INSERT, UPDATE, DELETE, and RETURNING namespaces closed | CLOSED |
| D19-S14 | CREATE TABLE declaration namespace, CREATE INDEX key binding, and DROP object-kind resolution closed | CLOSED |
| D19-S15 | Generic lookup/star/descriptor semantics integrated; missing-call category conflicts with §17.9.3 | PARTIAL/BLOCKED |
| D19-S16 | All non-call category mappings integrated; unsupported-call category conflicts with §17.9.3 | PARTIAL/BLOCKED |
| D19-S17 | Prerequisite-aware error candidates, SourceSpan ordering, tie classes, and iteration independence closed | CLOSED |
| D19-S18 | Explicit/implicit provenance and exact spans closed | CLOSED |

## Qualifiers, shadowing, and FROM visibility

Every relation occurrence exposes exactly one qualifier:

- `main.t` exposes `t`.
- `t AS x` exposes only `x`; `t` is hidden.
- A derived table exposes only its mandatory alias.
- Duplicate local qualifiers fail at the later conflicting qualifier span.
- An unaliased self-join of `t` fails; `t AS a JOIN t AS b` uses the same TableId and distinct BindingIds.

For `q.col`, a local `q` is authoritative. A missing `col` does not trigger outer fallback. For unqualified names, outer diagnostic lookup occurs only after zero local candidates; local ambiguity remains a local `BindError`.

Derived tables cannot see sibling FROM relations. JOIN ON visibility is accumulated-left plus current-right, never future relations.

## Outputs, aliases, and ORDER BY

Duplicate top-level display names and explicit aliases are legal and remain distinct ordered outputs.

ORDER BY resolution is:

1. complete bare unsigned integer literal → ordinal;
2. complete unqualified identifier → explicit `AS` alias lookup;
3. otherwise → ordinary input-scope expression.

Only explicit `AS` aliases enter the alias namespace. Generated names and unaliased direct-column display names do not.

A single alias match references the existing output identity. Multiple matches produce `BindError`; zero matches fall back to input binding.

## Grouping and aggregates

A block is aggregate when it has GROUP BY or a block-owned aggregate in SELECT, HAVING, or ORDER BY. Aggregate-without-GROUP forms one global group. HAVING alone does not.

Direct aggregate expressions are permitted only in SELECT, HAVING, and ORDER BY. Same-block nested aggregates are rejected. Aggregates in child subqueries remain child-owned.

Grouping legality uses fully bound structural equality:

- ignores SourceSpan, parenthesis-only provenance, and display aliases;
- includes BindingId, catalog identity, types, operators, casts, descriptors, values, and child order;
- does not infer commutativity, functional dependencies, keys, or algebraic equivalence.

The exact §29.3.7 aggregate-ordinal rule is now referenced.

## LIMIT/OFFSET

Closed constant domain:

- literals, contextual NULL, casts, operators, comparisons, CASE, and recursively constant scalar constructs;
- immutable scalar calls only, though the v1 scalar registry is empty;
- no columns, aliases, aggregates, subqueries, relation references, or row-dependent values.

Typing:

- INT32 and INT64 only;
- INT32 normalizes to INT64;
- raw NULL receives `INT64 NULL`;
- nonconstant → `BindError`;
- nonintegral → `TypeError`;
- final NULL or negative → `ExecutionError`;
- no clamping or implementation-sized SQL maximum.

Required Chapter-17 folding remains authoritative pending C19-2. The residual count is obtained once before relational row processing.

## BindingId and output metadata

BindingId is unique across the entire bound top-level statement, including nested blocks and derived relations. It is not reused while that statement survives and is nonpersistent, non-WAL, and noncatalog.

A preserved relation occurrence retains its BindingId through logical rewrites. Self-join columns are conceptually distinguished by BindingId + TableId + ColumnId.

Generated expression display names use the exact original source-byte slice selected by the expression SourceSpan—no pretty printing, normalization, or synthetic cast text. They never become aliases and obey D18-L1 retain-or-materialize lifetime rules.

## DML and DDL

- INSERT VALUES has no target-row scope.
- INSERT SELECT binds as an independent SELECT.
- UPDATE exposes one target-row binding to SET RHS, WHERE, and RETURNING.
- DELETE exposes one target-row binding to WHERE and RETURNING.
- UPDATE/DELETE expose the target’s final table-name component as qualifier.
- RETURNING preserves INSERT-new, UPDATE-new, and DELETE-old row images.
- Direct aggregates remain forbidden in DML expressions.

CREATE TABLE constraints resolve against the statement-local column declarations. Unknown/repeated members and a second primary key produce `ConstraintDefinitionError`.

CREATE INDEX preserves key order; duplicate keys are `BindError`; ineligible key types are `TypeError`.

DROP requires the requested object kind; missing or wrong-kind objects are `CatalogError`. Publication, dependency handling, and physical cleanup remain Chapter 21/storage-owned.

## Function and aggregate calls

Generic parser call shapes remain unchanged and registry-independent. Star calls consider only descriptors authorizing star syntax. `COUNT(*)` resolves through Chapter 29; `SUM(*)` and `foo(*)` have no legal star descriptor.

Argument-type incompatibility is `TypeError`. The no-descriptor category remains blocked by C19-1.

## Error precedence and cast provenance

Integrated non-conflicting category map:

- `CatalogError`: object lookup/name/kind failures.
- `BindError`: names/scopes, qualifiers, aliases, ordinals, grouping/placement, nonconstant counts, and duplicate DML/index targets.
- `TypeError`: types, overload/coercion, Boolean contexts, resolved-call argument mismatch, nonintegral counts, index-ineligible type.
- `ConstraintDefinitionError`: invalid CREATE TABLE constraints/default definitions.
- `UnsupportedFeature`: unsupported correlation and frozen unsupported semantics.
- `CardinalityError`: remains with downstream cardinality owners.
- Resource categories remain unchanged.

Ordinary independent errors compare only after prerequisites resolve:

1. earliest SourceSpan start;
2. shorter span on equal start;
3. identical-span class order: name/catalog, placement/shape, constraint, type/coercion, cardinality.

Operational resource/cancellation/runtime timing is not reordered.

Implicit casts are marked implicit and carry the coerced operand span. Explicit casts are marked explicit and carry the complete CAST-expression span. Provenance is runtime-only metadata governed by D18-L1 when borrowed.

## Regression and documentation-model audit

Confirmed unchanged:

- Chapter-16 canonical names, IDs, SchemaVer, descriptors, and snapshot rules
- Chapter-17 TypeIds, NULL, cast/coercion graph, operators, CASE, IN, and scalar registry
- Chapter-18 grammar, raw AST, spans, source order, lifetime, and resource rules
- Chapter-20 logical/subquery/DISTINCT semantics
- Chapter-21 publication, DML row images, defaults, and transactional DDL
- Chapter-29 aggregate signatures/execution/ordinal owner
- §39.1 transaction consequences
- persistent formats and recovery

No implementation chronology, current-state narration, development sequencing, verification procedure, PROJECT_STATE content, or history was introduced. Representation and catalog/container-order independence are preserved.

Protected findings:

- F19-N1: **PENDING / UNCHANGED**
- F19-N2: **PENDING / UNCHANGED**
- F19-N3: **PENDING / UNCHANGED**
- F19-N4: **CLOSED**

## Reread questions 1–228

All questions have their expected answers except these conflict-affected items:

| Question | Result |
|---:|---|
| 101 | Qualified: the residual expression is obtained once at execution start, but §17.10.2 may fold/evaluate or fail earlier |
| 157 | **NO** — requested no-name/shape/arity `BindError` conflicts with §17.9.3 `TYPE_ERROR` |
| 161 | Rejection is defined; requested `BindError` category remains blocked |
| 162 | `foo(*)` is rejected, but its requested category conflicts |
| 167 | **NO** — BindError map cannot be complete until C19-1 is resolved |
| 168 | **NO** — TypeError map conflicts with the approved no-descriptor split |
| 217 | **YES, contrary to expectation** — two frozen semantic conflicts arose |
| 225 | **NO, not completely** — deterministic semantics are blocked by the call-category conflict |
| 228 | **NO under the approved package** until both conflicts are resolved |

Questions 1–100, 102–156, 158–160, 163–166, 169–216, 218–224, 226, and 227 have their requested expected results. Questions 218–223 are all **NO** as expected.

## Finding status

| Finding | Status |
|---|---|
| F19-B1 | CLOSED |
| F19-B2 | CLOSED |
| F19-B3 | CLOSED |
| F19-B4 | CLOSED |
| F19-B5 | CLOSED |
| F19-B6 | BLOCKED/PARTIAL by C19-2 |
| F19-B7 | CLOSED |
| F19-M1 | CLOSED |
| F19-M2 | CLOSED |
| F19-M3 | CLOSED |
| F19-M4 | CLOSED |
| F19-M5 | BLOCKED/PARTIAL by C19-1 |
| F19-N4 | CLOSED |
| F19-N1–N3 | PENDING / UNCHANGED |

Chapter 19 therefore cannot yet be declared semantically clean or ready for document-only cleanup.

Recommended next action:

1. Resolve C19-1 and C19-2.
2. Complete D19-S10/S15/S16.
3. Then perform Chapter-19 document-only cleanup F19-N1–N3.
4. Verification synchronization remains later.

Chapter-20 direct review remains **NOT STARTED**.

## Hunk classification

| Class | Result |
|---|---|
| A–C | Qualifier namespace, alias uniqueness/hiding, local/outer shadowing |
| D–F | Duplicate outputs, ORDER aliases, complete alias visibility |
| G | FROM/JOIN/derived-table visibility |
| H–J | Aggregate classification/placement/HAVING and structural grouping |
| K–L | Ordinal syntax/range |
| M–O | Constant domain, typing, residual evaluation timing; O conflict-qualified |
| P–Q | BindingId whole-statement domain and rewrite lifetime |
| R | Exact-source display names |
| S–T | INSERT and UPDATE/DELETE/RETURNING namespaces |
| U–V | CREATE TABLE constraints and CREATE INDEX/DROP |
| W | Generic/star calls; category branch blocked |
| X | Exact §29.3.7 ordinal handoff |
| Y | Error map; call branch blocked |
| Z | Prerequisite-aware error precedence |
| AA | Explicit/implicit cast provenance |
| AB | Output schema and invariant synchronization |
| AC | Required semantic-owner references only |
| AD | Local analytical rationale |
| AE | Local wrapping |

## Final Git state

- Status: `M docs/ARCHITECTURE.md`
- Index: clean
- HEAD: `11cb88e9874fbd76317e4ea778d39e96f94a23a0`
- Task diff: 437 insertions, 42 deletions
- `git diff --check`: passed
- Files task-modified: only `docs/ARCHITECTURE.md`
- External repository changes observed: none
- No staging or commit
- No build, tests, benchmarks, implementation, devlog, or review artifact
- Phase 2 remains **NOT STARTED / NOT AUTHORIZED**
