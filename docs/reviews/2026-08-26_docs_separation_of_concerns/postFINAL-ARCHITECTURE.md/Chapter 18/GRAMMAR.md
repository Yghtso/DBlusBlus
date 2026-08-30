## 1. Integration verdict

**D18-G1 through D18-G14 are integrated and closed.**

The resulting Chapter-18 grammar is closed, deterministic, catalog-independent, and TypeResolver-independent.

- F18-B5: **CLOSED**
- F18-B6: **CLOSED**
- F18-M1: **CLOSED** — existing `ParserError`, `UnsupportedCorrelation`, `UnsupportedFeature`, type, and cardinality owners support D18-G14.
- Chapter 18: **SEMANTICALLY CLEAN, NOT YET DOCUMENT-CLEAN, NOT CLOSED**

No new frozen semantic conflict arose.

## 2. Git and scope

Initial state:

- HEAD: `88f44dd4c12b2e8b6a094e8ef787b8eef3f74c1d`
- Working tree: clean
- Index: clean

Final state:

- HEAD: `88f44dd4c12b2e8b6a094e8ef787b8eef3f74c1d`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- `git diff --check`: passed

Only [docs/ARCHITECTURE.md](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14461>) was modified, exclusively within Chapter 18. No external repository changes occurred during the task.

No review artifact was read, modified, or staged.

## 3. Sections modified

- [§18.10 Closed v1 statement grammar](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14461>)
- §18.10.1 request framing
- §18.10.2 DDL statements
- §18.10.3 DML and RETURNING
- §18.10.4 transaction, maintenance, and EXPLAIN
- [§18.11 SELECT grammar](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14712>)
- [§18.12 Closed v1 expression grammar](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14844>)
- §§18.12.1–18.12.5 primary expressions, CAST/CASE, subqueries, calls, and binding boundary
- [§18.13 AST contract](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15057>)
- [§18.15 precedence](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15186>)
- [§18.16 invariants](</home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15217>)

§18.11.1 no-FROM semantics and §18.14 AST ownership were unchanged.

## 4. D18-G1–G14 results

| Decision | Integrated rule | Status |
|---|---|---|
| G1 | Type name is an unquoted identifier; binder accepts seven canonical types | CLOSED |
| G2 | Table/index names have one or two components; binder requires `main` | CLOSED |
| G3 | Exact CREATE TABLE elements; table-level PK/UNIQUE only | CLOSED |
| G4 | Required lists nonempty; trailing commas universally forbidden | CLOSED |
| G5 | Projection/base/RETURNING aliases require AS; derived AS optional | CLOSED |
| G6 | Exact INNER/LEFT/CROSS grammar; left-associated join chain | CLOSED |
| G7 | No DML DEFAULT sentinel syntax | CLOSED |
| G8 | RETURNING is a nonempty expression list with optional explicit aliases | CLOSED |
| G9 | LIMIT/OFFSET operands are expressions; Chapters 19/20 own validation | CLOSED |
| G10 | Generic zero/multiple/star calls; no parser function-name lookup | CLOSED |
| G11 | VACUUM requires one table | CLOSED |
| G12 | EXPLAIN and EXPLAIN ANALYZE accept SELECT only | CLOSED |
| G13 | UPDATE/DELETE targets cannot have aliases | CLOSED |
| G14 | Structural subquery rejection separated from semantic rejection | CLOSED |

## 5. Names, types, and aliases

Type syntax is:

```ebnf
<type-name> ::= <unquoted-identifier>
```

Consequences:

- `INT32` and `int32` parse identically after ASCII canonicalization.
- `potato` parses in type position, then fails type resolution.
- `"INT32"` is a syntax error in type position.
- No type alias, `VARCHAR(n)`, or raw-AST TypeId exists.

Object names are:

```ebnf
<object-name> ::=
      <identifier>
    | <identifier> "." <identifier>
```

Tables and indexes use this form. For two components, the binder requires the first canonical component to be `main`. Three-part names are syntax errors.

Alias rules:

- projection: `<expression> [ "AS" <alias> ]`
- base table: `<table-name> [ "AS" <alias> ]`
- derived table: `"(" <select-statement> ")" [ "AS" ] <alias>`
- UPDATE/DELETE target: no alias

## 6. Final statement inventory

The exact top-level forms are:

1. CREATE TABLE
2. CREATE INDEX
3. CREATE UNIQUE INDEX
4. DROP TABLE
5. DROP INDEX
6. INSERT
7. UPDATE
8. DELETE
9. SELECT
10. BEGIN
11. COMMIT
12. ROLLBACK
13. VACUUM
14. ANALYZE
15. EXPLAIN SELECT
16. EXPLAIN ANALYZE SELECT

No generic statement node admits any other start.

## 7. Final statement EBNF

```ebnf
<sql-request> ::=
    <statement> { ";" <statement> } [ ";" ] <end-of-input>

<statement> ::=
      <create-table-statement>
    | <create-index-statement>
    | <drop-statement>
    | <insert-statement>
    | <update-statement>
    | <delete-statement>
    | <select-statement>
    | "BEGIN"
    | "COMMIT"
    | "ROLLBACK"
    | <vacuum-statement>
    | <analyze-statement>
    | <explain-statement>

<create-table-statement> ::=
    "CREATE" "TABLE" <table-name>
    "(" <table-element> { "," <table-element> } ")"

<table-element> ::=
      <column-definition>
    | <table-constraint>

<column-definition> ::=
    <column-name> <type-name>
    [ "NOT" "NULL" ]
    [ "DEFAULT" <expression> ]

<table-constraint> ::=
      "PRIMARY" "KEY"
        "(" <column-name> { "," <column-name> } ")"
    | "UNIQUE"
        "(" <column-name> { "," <column-name> } ")"

<create-index-statement> ::=
    "CREATE" [ "UNIQUE" ] "INDEX" <index-name>
    "ON" <table-name>
    "(" <column-name> { "," <column-name> } ")"

<drop-statement> ::=
      "DROP" "TABLE" <table-name>
    | "DROP" "INDEX" <index-name>

<insert-statement> ::=
    "INSERT" "INTO" <table-name>
    [ "(" <column-name> { "," <column-name> } ")" ]
    <insert-source>
    [ <returning-clause> ]

<insert-source> ::=
      "VALUES" <values-row> { "," <values-row> }
    | <select-statement>

<values-row> ::=
    "(" <expression> { "," <expression> } ")"

<update-statement> ::=
    "UPDATE" <table-name>
    "SET" <assignment> { "," <assignment> }
    [ "WHERE" <expression> ]
    [ <returning-clause> ]

<assignment> ::=
    <column-name> "=" <expression>

<delete-statement> ::=
    "DELETE" "FROM" <table-name>
    [ "WHERE" <expression> ]
    [ <returning-clause> ]

<returning-clause> ::=
    "RETURNING" <returning-item> { "," <returning-item> }

<returning-item> ::=
    <expression> [ "AS" <alias> ]

<select-statement> ::=
    "SELECT" [ "DISTINCT" ]
    <select-item> { "," <select-item> }
    [ <from-clause> ]
    [ "WHERE" <expression> ]
    [ "GROUP" "BY" <expression> { "," <expression> } ]
    [ "HAVING" <expression> ]
    [ "ORDER" "BY" <order-item> { "," <order-item> } ]
    [ "LIMIT" <expression> ]
    [ "OFFSET" <expression> ]

<select-item> ::=
      "*"
    | <identifier> "." "*"
    | <expression> [ "AS" <alias> ]

<from-clause> ::=
    "FROM" <joined-table>

<joined-table> ::=
    <table-primary> { <join-tail> }

<table-primary> ::=
      <table-name> [ "AS" <alias> ]
    | "(" <select-statement> ")" [ "AS" ] <alias>

<join-tail> ::=
      "INNER" "JOIN" <table-primary> "ON" <expression>
    | "LEFT" "JOIN" <table-primary> "ON" <expression>
    | "CROSS" "JOIN" <table-primary>

<order-item> ::=
    <expression> [ "ASC" | "DESC" ]

<vacuum-statement>  ::= "VACUUM" <table-name>
<analyze-statement> ::= "ANALYZE" <table-name>

<explain-statement> ::=
      "EXPLAIN" <select-statement>
    | "EXPLAIN" "ANALYZE" <select-statement>
```

## 8. Statement-specific outcomes

CREATE TABLE:

- Table elements are nonempty and ordered.
- Column modifiers are fixed as `[NOT NULL] [DEFAULT expression]`.
- Explicit `NULL`, column-level PK/UNIQUE, flexible modifier order, named constraints, CHECK, and FK are excluded.
- Composite table-level PK/UNIQUE are supported.

CREATE INDEX:

- Nonempty ordered base-column list.
- Duplicates preserved for binder rejection.
- No expressions, ASC/DESC, INCLUDE, partial predicate, or IF NOT EXISTS.

INSERT:

- Optional nonempty target-column list.
- Nonempty VALUES rows and nonempty row items.
- INSERT SELECT supported.
- No `DEFAULT VALUES` or per-cell DEFAULT.

UPDATE/DELETE:

- No target aliases.
- No UPDATE FROM, tuple assignments, DELETE USING, DML ORDER BY/LIMIT, or UPDATE DEFAULT.
- Duplicate UPDATE assignments remain in source order for binder rejection.

RETURNING:

- DML only.
- Nonempty expression list.
- Optional explicit AS aliases.
- No wildcard or qualified wildcard.

VACUUM/ANALYZE:

- Exactly one table target.
- No targetless or option forms.

EXPLAIN:

- SELECT only.
- EXPLAIN ANALYZE SELECT only.
- No DML/DDL/maintenance targets.

## 9. JOIN and LIMIT/OFFSET results

JOINs are exactly:

```ebnf
"INNER" "JOIN" <table-primary> "ON" <expression>
"LEFT"  "JOIN" <table-primary> "ON" <expression>
"CROSS" "JOIN" <table-primary>
```

Repeated tails form a left-associated raw tree. Bare JOIN, comma joins, parenthesized join groups, RIGHT/FULL/NATURAL/USING/LATERAL joins are excluded.

LIMIT/OFFSET are now expression syntax:

```ebnf
[ "LIMIT" <expression> ]
[ "OFFSET" <expression> ]
```

The previous integer-token implication was removed. Chapters 19 and 20 retain authority for:

- integral type
- nonnegative value
- execution-start constancy
- no subqueries
- runtime range

This resolves the prior owner conflict without editing Chapters 19 or 20.

## 10. Final expression EBNF

```ebnf
<expression> ::=
    <or-expression>

<or-expression> ::=
    <and-expression> { "OR" <and-expression> }

<and-expression> ::=
    <not-expression> { "AND" <not-expression> }

<not-expression> ::=
      "NOT" <not-expression>
    | <predicate-expression>

<predicate-expression> ::=
    <additive-expression>
    [
          <comparison-operator> <additive-expression>
        | "IS" [ "NOT" ] "NULL"
        | [ "NOT" ] "IN" "(" <in-right-hand-side> ")"
    ]

<comparison-operator> ::=
      "=" | "<>" | "<" | "<=" | ">" | ">="

<in-right-hand-side> ::=
      <expression> { "," <expression> }
    | <select-statement>

<additive-expression> ::=
    <multiplicative-expression>
    { ( "+" | "-" ) <multiplicative-expression> }

<multiplicative-expression> ::=
    <unary-expression>
    { ( "*" | "/" | "%" ) <unary-expression> }

<unary-expression> ::=
      "+" <unary-expression>
    | "-" <unary-expression>
    | <primary-expression>

<primary-expression> ::=
      <literal>
    | <column-reference>
    | <parenthesized-expression>
    | <cast-expression>
    | <case-expression>
    | <exists-expression>
    | <scalar-subquery-expression>
    | <function-call>

<column-reference> ::=
      <identifier>
    | <identifier> "." <identifier>

<parenthesized-expression> ::=
    "(" <expression> ")"

<cast-expression> ::=
    "CAST" "(" <expression> "AS" <type-name> ")"

<case-expression> ::=
    "CASE"
    <when-clause> { <when-clause> }
    [ "ELSE" <expression> ]
    "END"

<when-clause> ::=
    "WHEN" <expression> "THEN" <expression>

<exists-expression> ::=
    "EXISTS" "(" <select-statement> ")"

<scalar-subquery-expression> ::=
    "(" <select-statement> ")"

<function-call> ::=
      <identifier> "(" ")"
    | <identifier> "(" <expression> { "," <expression> } ")"
    | <identifier> "(" "*" ")"

<literal> ::=
      <integer-literal-token>
    | <floating-literal-token>
    | <string-literal-token>
    | "TRUE"
    | "FALSE"
    | "NULL"
```

## 11. Precedence and parse results

Precedence remains, low to high:

1. OR
2. AND
3. NOT
4. comparison / IS NULL / IN
5. additive
6. multiplicative
7. unary signs
8. primary

Results:

- `a < b < c`: syntax error
- `a = b = c`: syntax error
- `NOT a = b`: `NOT (a = b)`
- `NOT a IN (x)`: `NOT (a IN (x))`
- `a NOT IN (x)`: one NOT-IN predicate
- `a IS NOT NULL`: one null-predicate
- additive/multiplicative/Boolean infix: left-associated
- prefix NOT and signs: nest toward the operand

Parenthesized-expression provenance remains mandatory, preserving:

```text
UnaryMinus(NumericLiteral)
```

versus:

```text
UnaryMinus(ParenthesizedExpression(NumericLiteral))
```

Chapter 17 remains the direct-negative semantic owner.

## 12. Function and star-call boundary

The parser accepts structurally:

```text
f()
f(a)
f(a,b)
f(*)
count(*)
sum(*)
foo(*)
```

It does not special-case any function name.

The raw AST distinguishes ordinary argument calls from star-argument calls. Chapter 17/29 decides:

- scalar versus aggregate
- existence
- arity
- argument types
- whether star is legal

The four star roles remain distinct:

1. `SELECT *`
2. `identifier.*`
3. multiplication
4. function-call star argument

## 13. Subquery boundary and F18-M1

Structural parser-owned forms:

- scalar `(SELECT ...)`
- EXISTS/NOT EXISTS
- IN/NOT IN SELECT
- derived SELECT with mandatory alias

Parser errors include:

- parenthesized INSERT/UPDATE/DELETE/DDL
- unsupported row constructors
- unsupported wrappers/operators
- any non-SELECT subquery body

Binder/Chapter-20 errors include:

- correlation
- scalar output arity
- IN output arity/type
- name resolution
- bound-schema/type-dependent unsupported properties

Thus:

- correlated SELECT: parses, then `UnsupportedCorrelation`
- multi-column scalar SELECT: parses, then shape/cardinality/type rejection
- incompatible IN SELECT: parses, then binder/TypeResolver rejection
- data-modifying subquery: structural `ParserError`

F18-M1 is therefore **CLOSED** using existing error owners.

## 14. Raw-AST contract

The raw AST now explicitly preserves:

- statement request order
- every clause’s presence
- one/two-part object-name syntax
- type-name identifier bytes without TypeId
- explicit aliases
- literal provenance
- parenthesized-expression boundaries
- ordered JOIN left/right/ON structure
- ordered CASE arms and IN items
- ordered VALUES rows/items
- ordered DDL declarations/index keys
- ordered RETURNING items
- normal-call versus star-call shape
- all four star roles
- exact multiplicity and duplicates

It excludes:

- TableId
- ColumnId
- IndexId
- ConstraintId
- BindingId
- resolved TypeId
- catalog descriptors
- parser-inserted semantic casts
- logical or physical plans

No container or concrete AST class implementation was prescribed.

## 15. Registry regression

Keyword registry:

- unchanged
- every existing reserved keyword remains used
- no contextual keyword introduced
- no type names converted into keywords

Symbol registry:

- unchanged
- every symbol is accounted for
- no `!=`, `||`, `::`, parameters, brackets, or generic operator token added

## 16. Unsupported v1 syntax affected

The closed grammar excludes:

- ALTER TABLE, MERGE, ON CONFLICT/UPSERT
- WITH/CTEs and set operations
- RIGHT/FULL/NATURAL/USING/LATERAL joins
- windows, FILTER, OVER, aggregate DISTINCT
- CHECK, foreign keys, named constraints
- parameters and qualified function names
- VARCHAR(n), DECIMAL, INTERVAL, JSON, ARRAY
- DATE/TIMESTAMP typed literals
- UPDATE FROM and DELETE USING
- DML DEFAULT sentinels
- RETURNING outside DML or with wildcard
- DML target aliases
- `!=`, `||`, and `::`

## 17. Closedness and reread results

Closedness questions A–N: **YES to all**.

Numbered local reread:

| Questions | Result |
|---|---|
| 1–114 | YES |
| 115–120 | NO — no chronology, implementation narration, sequencing, verification procedure, state leakage, or history introduced |
| 121–133 | YES |
| 134 | NO — no new frozen semantic question |

Documentation model:

- Analytical and timeless task-created prose: yes
- Parser/catalog separation: precise
- Parser/TypeResolver separation: precise
- Parse success versus bind success: explicit
- Runtime/container implementation freedom: preserved
- Current-state or Phase narration: none
- Verification methodology: none
- Development sequencing/history: none

Only grammar-conflicting local “initial/future” wording was replaced. The broader F18-N1 cleanup remains pending.

## 18. Protected findings and chapter status

| Finding | Status |
|---|---|
| F18-B5 | CLOSED |
| F18-B6 | CLOSED |
| F18-M1 | CLOSED |
| F18-M2 payload/source lifetime | OPEN / unchanged |
| F18-M3 resource exhaustion | OPEN / unchanged |
| F18-N1 temporality cleanup | PENDING / unchanged except unavoidable grammar-local wording |
| F18-N2 implementation coupling | PENDING / unchanged |
| F18-N3 no-FROM ownership duplication | PENDING / unchanged |
| F18-N4 diagnostic-span wording | PENDING / unchanged |

Overall:

```text
Chapter 18:
    SEMANTICALLY CLEAN
    NOT YET DOCUMENT-CLEAN
    NOT CLOSED
```

The next task is to resolve F18-M2 and F18-M3. Verification synchronization was not started.

Chapter 19 direct review remains **NOT STARTED**.

## 19. Diff classification A–Z

- A–B: type and object-name grammar
- C–D: CREATE TABLE and global list policy
- E–F: aliases and JOINs
- G–H: DML DEFAULT and RETURNING
- I: LIMIT/OFFSET owner synchronization
- J–K: generic calls and star-call distinction
- L–N: VACUUM, EXPLAIN, DML target aliases
- O–P: structural versus semantic subquery rejection
- Q–T: complete statement/expression/SELECT/predicate grammar
- U: raw-AST contract
- V: keyword/symbol audit; no registry edit required
- W–X: exact owner references and analytical rationale
- Y: invariant synchronization
- Z: local wrapping only

No unrelated cleanup was performed.

## 20. Final confirmations

- Files changed: `docs/ARCHITECTURE.md` only
- Chapter 18 substantive edits only: yes
- Chapters 1–17 and 19+: unchanged
- VERIFICATION/DEVELOPMENT/PROJECT_STATE: unchanged
- Source/tests/build files: unchanged
- Review artifacts: untouched and unstaged
- Builds/tests/benchmarks: not run, as required
- Staging/commit/devlog/review artifact: none
- Implementation work: none
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
