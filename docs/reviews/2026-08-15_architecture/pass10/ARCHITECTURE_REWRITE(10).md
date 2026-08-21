# Rewrite Pass 10 — Catalog, Types, Lexer/Parser, Binder, and Expressions

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `301..358`
- processed source lines: `9654..11298`
- next untouched section: `359. Logical Plan Node Contract`
- legacy architecture modified: **no**
- production code modified: **no**
- logical-plan/DDL/rewrite material from §359+ migrated: **no**

Working architecture:

```text
before SHA-256: 093f38ba408bad061d32b4d03b29f363920c9ffcc3fa539a35f420380c8dd0a9
after  SHA-256: b8ef973ae7abec372434b07d0718c06eb3bc29ec866d2eb1813af3779c67e3ec
```

## Canonical chapters

Pass 10 replaces the Pass-1 baselines with:

```text
Chapter 16  Catalog and Schema Metadata
Chapter 17  SQL Type and Value System
Chapter 18  Lexer, Parser, and AST
Chapter 19  Binding and Expression Semantics
```

## Catalog

Canonicalized one v1 namespace `main`, stable TableId/ColumnId/IndexId/TypeId identities, relational `sys_*` metadata semantics, minimal bootstrap, immutable descriptors/cache, historical SchemaDescriptor lookup, and the distinction between ColumnId, physical position, logical position, BindingId, and plan slots.

Historical tuple interpretation is now an explicit cross-layer contract:

```text
tuple.schema_version
    ->
ResolveSchema(TableId, SchemaVer)
    ->
historical physical SchemaDescriptor
```

Vacuum uses the same historical descriptor when reconstructing exact old indexed keys.

`sys_columns` version membership is canonicalized as a half-open interval `added <= V < removed`, with absent removal meaning still present.

## Catalog TypeId completion

Persistent built-in TypeIds are now:

```text
0 invalid
1 BOOLEAN
2 INT32
3 INT64
4 FLOAT64
5 DATE
6 TIMESTAMP
7 VARCHAR
```

NULL and UNKNOWN are not persisted column TypeIds.

## SQL type/value semantics

Canonicalized logical scalar kinds, NULL/UNKNOWN roles, numeric widening, explicit/implicit casts, BOOLEAN-only predicates, SQL three-valued logic, NULL comparison, centralized TypeResolver ownership, and non-hot generic Value usage.

Cross-layer scalar semantics are now explicit:

```text
VARCHAR:
    opaque bytes
    binary bytewise collation

FLOAT64 comparison:
    Chapter-8 total order/equality

DATE:
    int32 days from 1970-01-01

TIMESTAMP:
    int64 microseconds from 1970-01-01 00:00:00
    timezone-naive v1
```

## Lexer/parser/AST

Canonicalized the handwritten lexer, recursive-descent statement parser, Pratt expression parser, token/source-span model, identifier normalization, SQL string escaping, numeric literal forms, comments, AST ownership, statement/SELECT/subquery surface, and expression precedence.

## Binder/expression IR

Canonicalized nested scopes, BindingId, name resolution, wildcard expansion, output naming, immutable typed bound-expression IR, centralized operator/function registries, function volatility, aggregate/grouping/HAVING semantics, ORDER BY resolution, LIMIT/OFFSET, DISTINCT, searched CASE, IN-list three-valued behavior, and future parameter typing compatibility.

## Later-source refinement check

Targeted §§376–383, 402–408, and 427 were inspected only to avoid contradiction.

They confirm that later DDL owns transactional catalog publication/concurrency, logical planning assigns output slots after binding, and upper-layer invariants agree with the Pass-10 parser/binder/type separation.

Those later section bodies were not migrated.

## New issues

- `R-036`: catalog bootstrap / CATALOG_DATA physical representation remains unspecified.
- `R-037`: transactional catalog-object ID allocation is intentionally deferred to Pass 11 DDL.
- `R-038`: built-in TypeId numeric registry resolved in Pass 10.
- `R-039`: DATE/TIMESTAMP epoch/unit semantics resolved in Pass 10.

## Coverage

```text
legacy §§0..358     complete / explicitly disposed
legacy §§359..725   pending
```

All 58 Pass-10 sections have non-PENDING dispositions.

No §359+ coverage row was changed.

## Validation

- pinned legacy SHA unchanged,
- §301 starts at line 9654,
- §359 starts at line 11299,
- Chapters 16–19 occur exactly once,
- historical schema lookup is cross-referenced by tuple and vacuum contracts,
- built-in TypeId codes appear in Appendix A,
- every coverage row through §358 is non-PENDING,
- every row from §359 remains PENDING,
- no production code was changed.

## Exit status

**PASS 10 COMPLETE.**

Open Pass-10 issues:

```text
R-036 bootstrap physical format
R-037 catalog-object ID allocation (Pass 11 owner)
```
