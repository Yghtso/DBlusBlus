# 19. Binding and Expression Semantics

## 19.1 Binder role

The Binder converts AST syntax into resolved, typed semantic structures.

It owns:

```text
table lookup
column lookup
alias scopes
ambiguity detection
wildcard expansion
type resolution
implicit cast insertion
aggregate validation
GROUP BY validation
ORDER BY resolution
function/operator resolution
subquery scope creation
DML target resolution
constraint/type validation
```

It does not choose physical access/join algorithms, estimate cardinality, or access heap/B+ pages directly.

Catalog-backed binding consumes the transaction-visible immutable descriptors
selected under §§16.6, 16.10, and 21.3. It does not redefine
statement/transaction snapshot visibility, own earlier DDL visibility,
SchemaVer selection, or descriptor lifetime, and a catalog cache cannot
override those owners.

## 19.2 Binding scopes and BindingId

Binding uses explicit nested scopes.

A scope contains conceptually:

```text
one local relation-qualifier namespace
visible relation bindings in source relation order
the SELECT output-alias namespace where applicable
parent scope link
```

Every query block owns exactly one local relation-qualifier namespace keyed by
the canonical identifier bytes defined by §§16.2 and 18.4. Every visible
relation occurrence exposes exactly one qualifier in that namespace:

- an unaliased base table exposes the final component of its object name, so
  `main.t` exposes `t`;
- an explicitly aliased base table exposes only its alias, so `t AS x` exposes
  `x` and hides `t` in that query block;
- a derived table exposes only its mandatory alias.

All exposed local qualifiers MUST be unique. A later relation occurrence that
would expose an existing qualifier fails with `BindError` at its conflicting
qualifier span; no occurrence wins, receives an automatic name, or exposes a
hidden numeric disambiguator. Thus an unaliased self-join of `t` is invalid,
while `t AS a INNER JOIN t AS b ...` is valid.

Every visible base-table or derived-table relation occurrence receives an
opaque runtime `BindingId` distinct from `TableId`. Its uniqueness domain is
the entire bound top-level SQL statement, including every nested query block
and derived table. Distinct relation occurrences in that statement MUST have
distinct BindingIds; binding a child block does not create a reusable
per-block identity domain. Different top-level statements MAY reuse the same
opaque values.

A self-join therefore has the same TableId but different BindingIds.

`BindingId` is runtime-only and is not catalog state, WAL state, or a
persistent/recovered identity. It remains the identity of its relation
occurrence through binding, logical-plan construction, and logical rewrites
for as long as that semantic occurrence survives. A rewrite preserving an
occurrence preserves its BindingId; any semantically distinct relation
occurrence introduced by a downstream owner requires a distinct identity.
The representation, width, and allocation technique are implementation-
defined.

### 19.2.1 FROM and JOIN visibility

The architecture has no lateral table-reference semantics. A derived table binds as an
uncorrelated child query block and receives no ordinary visibility into any
earlier or later sibling in the containing FROM clause. A sibling reference
is classified through the unsupported-correlation rule in §19.18.

For `left_subtree JOIN right_relation ON predicate`, the ON predicate sees
exactly every relation binding in the accumulated left subtree plus the
current right relation. It does not see any later join relation. This follows
the left-associated §18.11 join structure: in
`a INNER JOIN b ON p LEFT JOIN c ON q`, `p` sees `a,b`, while `q` sees
`a,b,c`.

After the complete FROM/JOIN tree has bound, its local relation bindings are
visible to WHERE, GROUP BY, HAVING, the SELECT list, and the ordinary input-
expression fallback for ORDER BY, subject to the clause-specific aggregate
and output-alias rules below. LIMIT/OFFSET has no relation-column namespace
under §19.14.

### 19.2.2 SELECT output-alias visibility

SELECT output aliases are visible only to ORDER BY. They are not visible in
the same SELECT list, JOIN ON, WHERE, GROUP BY, HAVING, LIMIT, or OFFSET.
GROUP BY has neither an output-alias shortcut nor an ordinal shortcut: each
item binds as an ordinary input-scope expression, so `GROUP BY 1` groups by the
integer constant expression rather than projection ordinal 1.

The parent-scope structure exists only for §19.18's correlation diagnostic.
Resolution never searches it for a subquery column and never emits
executable OuterRef state.

## 19.3 Bound column references

A bound column expression contains at least:

```text
BindingId
TableId
ColumnId
LogicalType
nullable
source span
```

Logical-plan construction assigns output-slot identity under §20.2.

Bound column semantics are never represented solely by the original textual column name.
For a base-table column, relation-occurrence identity and catalog-column
identity are both semantic: self-join references may have the same TableId and
ColumnId but MUST have different BindingIds.

## 19.4 Column name resolution

### 19.4.1 Unqualified names

The binder first computes the complete candidate set in the current query
block. Exactly:

```text
0 matches -> unknown-column error
1 match   -> bind that column
>1 match  -> ambiguous-column error
```

Never silently choose the leftmost match.

Only after zero local candidates may enclosing scopes be inspected, and then
only to distinguish unsupported correlation from an ordinary unknown name as
specified by §19.18. An outer match never overrides one local match or rescues
a locally ambiguous reference.

### 19.4.2 Qualified names

For `u.id`, first search the current block's one relation-qualifier namespace.
If `u` exists locally, that binding is authoritative and `id` is resolved only
within it. A missing local member is an ordinary local unknown-member
`BindError`; binding does not fall through to an outer `u`.

Only if no local qualifier `u` exists may outer scopes be inspected for the
§19.18 unsupported-correlation diagnostic. Local qualifier presence therefore
fully shadows an outer qualifier.

Unknown qualifier and unknown column are distinct semantic errors.

### 19.4.3 DML relation namespaces

INSERT target-table identity and target-column mapping do not create a target-
row relation scope for VALUES expressions. For example,
`INSERT INTO t(a) VALUES(a)` does not bind `a` as target column `t.a` merely
because `t` is the INSERT target. An INSERT SELECT source binds as an ordinary
independent SELECT query block and likewise receives no relation visibility
from the INSERT target; §21.11 target mapping and §17.8.5 assignment coercion
occur after source binding.

UPDATE and DELETE each create one target-row relation binding. Because
§18.10.3 excludes DML target aliases, its sole exposed qualifier is the target
table's canonical final name component. UPDATE target columns are visible,
unqualified or through that qualifier, to every SET right-hand expression,
WHERE, and RETURNING. Each SET left-hand name resolves directly against the
target descriptor rather than through multi-relation expression ambiguity.
DELETE exposes its target columns to WHERE and RETURNING under the same
unaliased qualifier rule. No other ordinary relation binding is introduced.

RETURNING binds names/types against the §21.15 row image:

```text
INSERT -> inserted/new row
UPDATE -> updated/final new row
DELETE -> deleted/old row
```

This namespace rule does not redefine physical row-version creation,
publication, retry, or result buffering owned by §§21.11 and 21.13–21.15.
Direct aggregate expressions remain forbidden in DML expressions, including
RETURNING, under §19.10; aggregates inside an independently bound supported
child SELECT remain owned by that child query block.

### 19.4.4 DDL declaration and object binding

CREATE TABLE declared columns form one statement-local declaration namespace.
Table-level PRIMARY KEY and UNIQUE member names resolve only against that
namespace. An unknown member, a repeated member within one constraint, or a
second PRIMARY KEY declaration produces `ConstraintDefinitionError` at the
responsible declaration span, subject to the constraints in §§21.6.1 and 21.7.

CREATE INDEX binding composes with §21.8.1: it resolves its target table in the
§21.3 catalog snapshot, then resolves each key name against that descriptor
while preserving §18.13 source order. A repeated key column produces
`BindError`; a key whose type is not index-eligible produces `TypeError`.
Physical construction and UNIQUE enforcement remain owned by §§21.8.2 and
11.10, respectively.

DROP binding resolves exactly the requested object kind. A missing object, or
a name visible only as the wrong requested kind, produces `CatalogError`.
Dependency checks, transactional publication, file retirement, and physical
cleanup remain §21.9 and storage-layer responsibilities.

## 19.5 Wildcards and output names

`SELECT *` expands during binding into explicit bound column references in visible FROM-relation order and each relation's logical presentation order.

`SELECT u.*` expands one relation binding.

Unqualified `*` requires at least one visible FROM relation; in a no-FROM
query block it is a bind error rather than an empty expansion. Qualified
`u.*` with no matching visible relation, including every no-FROM query block,
is an unknown-qualifier bind error.

Expansion order remains visible relation-occurrence source order followed by
each descriptor's §16.8 logical presentation column order. Qualified `u.*`
resolves exactly one local qualifier; duplicate qualifiers cannot exist
because §19.2 rejects them.

Execution never performs wildcard expansion.

Each SELECT output has:

```text
expression
display name
logical type
nullable
```

Display-name priority is:

1. explicit `AS alias`,
2. simple source column name for a direct column reference,
3. for every other expression, the exact original SQL source-byte slice
   designated by that expression's canonical SourceSpan.

The third form is not normalized, pretty-printed, or reconstructed. It retains
the original bytes, including source whitespace inside the expression, and
does not expose binder-inserted implicit-cast text. If result metadata outlives
the source backing, §18.14's retain-or-materialize rule applies.

Duplicate top-level result display names, including duplicate explicit AS
aliases, are legal. They remain distinct ordered output slots and are not
deduplicated, renamed, suffixed, or rejected merely for sharing a display
name. Derived-table exported-name restrictions remain §20.14.3-owned.

Only explicit SELECT-list AS aliases enter the SELECT output-alias namespace.
Generated display names and direct-column display names without AS do not.
For an ORDER BY item that is one unqualified identifier, the binder first
forms the exact-match candidate set in that explicit-alias namespace:

```text
0 matches -> bind as an ordinary input-scope expression
1 match   -> reference that existing SELECT output slot
>1 match  -> ambiguous-output-alias BindError
```

The one-match result preserves output-slot semantic identity; it does not
rebind or independently re-evaluate the aliased raw expression. The concrete
LogicalSlotId or equivalent downstream identity is owned by §20.2.

Generated display names are presentation metadata, not stable catalog identity.
They never become binder lookup entries or ORDER BY alias candidates, may
duplicate freely at top level, and define no persistent format.

## 19.6 Bound-expression IR

After binding, expressions are typed semantic nodes.

The bound-expression kinds include:

```text
BoundConstant
BoundColumnRef
BoundUnary
BoundBinary
BoundComparison
BoundBoolean
BoundCast
BoundFunction
BoundAggregate
BoundCase
BoundIsNull
BoundInList
BoundSubquery
```

Every bound expression exposes:

```text
LogicalType return_type
bool nullable
source span
```

A bound cast additionally retains explicit-versus-implicit provenance. An
explicit source `CAST(expr AS T)` is marked explicit and carries the complete
raw CAST-expression SourceSpan under §§18.8 and 18.13. A binder-inserted
implicit cast is a synthetic semantic node marked implicit and carries the
SourceSpan of the operand expression being coerced, not the span of the parent
operator or call whose resolution selected the cast. The distinction is
retained wherever downstream diagnostics or semantic analysis observes cast
provenance.

Cast provenance is runtime bound-expression metadata, not a TypeId, catalog
field, WAL field, or persisted tuple value. Borrowed source-derived metadata
obeys §18.14; its concrete representation is implementation-defined.

The executor receives resolved types and does not redo SQL type inference.

Bound expressions satisfy the §25.1 vectorized expression-execution handoff,
conceptually of the form:

```text
Evaluate(input DataChunk, selection) -> output Vector
```

Expression nodes do not own operator scheduling or pipeline state.

Nullability is conservative and semantic. Catalog NOT NULL alone does not prove
a column reference non-nullable after a LEFT JOIN; §20.8.3 owns contextual
null extension and its distinction from catalog metadata. Required examples include:

```text
NOT NULL column ref without contextual null extension -> non-nullable
nullable column ref -> nullable
x + y -> nullable if either input nullable
x = y -> nullable if either input nullable
IS NULL(x) -> non-nullable BOOLEAN
COUNT(*) -> non-nullable INT64
SUM(nullable input) -> nullable
scalar subquery -> nullable even when its output expression is NOT NULL
EXISTS / NOT EXISTS -> non-nullable BOOLEAN
IN / NOT IN subquery -> conservatively nullable BOOLEAN
```

## 19.7 Expression immutability and ownership

Bound expressions are immutable after construction.

A rewrite creates a new expression or structurally shares immutable children; it does not change the meaning of an expression object that other plans may already reference.

Expression lifetime is query/plan scoped.

Bound-expression allocation, ownership, sharing, and storage representation
are implementation-defined. Any representation is conforming if it preserves
the immutable semantic structure, the §18.14 backing-lifetime contract, and
the §19.2 BindingId identity rules, and if pointer or allocation identity is
not observable query semantics.

## 19.8 Operator registry

Arithmetic/comparison resolution is centralized and uses exactly the closed
§17.6 operator and §17.7.1 comparison tables. No
representative/polymorphic signature expands that
registry.

The Binder first applies the §17.10.1 TypeResolver coercion/promotion rules,
inserts required implicit casts, then selects the final operator
implementation/signature.

Raw-AST node kinds are not independent type-rule authorities.

## 19.9 Function registry and volatility

The named scalar-function registry is empty under §17.9.3. The descriptor
model below defines bound-call metadata but does not authorize any scalar
function call.

The generic `f()`, `f(expr,...)`, and `f(*)` shapes are supplied by §18.12.4.
Call binding considers registered scalar and aggregate descriptors by
canonical name and argument shape without parser special-casing. Ordinary
argument calls consider descriptors whose arity/shape can match; a star call
considers only a descriptor explicitly authorizing star syntax. Exactly one
semantic descriptor must resolve under the closed registries and TypeResolver
rules. Thus `COUNT(*)` resolves only through the §29.3.2 star-capable
descriptor, while `SUM(*)` and `foo(*)` have no legal star descriptor. Bound
argument types never authorize an unregistered coercion or overload.

For any registry state admitting multiple equally legal descriptors, the
registry and TypeResolver must define one deterministic winner. If no legal
descriptor remains for the canonical name, call shape, arity, star/nonstar
form, or bound argument types—or if equally legal descriptors remain without
an owner-defined winner—binding produces `TypeError` under §§17.9.3 and
17.10.1. Thus unknown calls, wrong arity, unauthorized star forms including
`SUM(*)` and `foo(*)`, argument-type incompatibility, and unresolved overload
ambiguity are TypeError rather than BindError or ParserError.

Call arguments are prerequisites for registry/type resolution. If an argument
itself fails to bind, that prerequisite error is returned without fabricating
a call TypeError from a nonexistent argument type. Conversely, a successfully
resolved aggregate used in a forbidden context is an aggregate-placement
`BindError` under §19.10, not a registry TypeError. Descriptor/container
iteration order is never semantic authority.

Functions resolve through descriptors containing at least:

```text
name
argument-type signature / polymorphic rule
return-type rule
volatility class
null-handling rule
implementation ID
```

`implementation ID` is a process/runtime dispatch identity unless another architecture contract explicitly assigns a stable persistent code.

Catalog defaults do **not** persist this implementation ID: permitted closed immutable default expressions are evaluated to one typed scalar before persistence as defined in §21.12.

Volatility classes are:

```text
IMMUTABLE
STABLE
VOLATILE
```

Only an IMMUTABLE function with constant arguments is eligible for ordinary
compile/bind-time constant folding. The empty registry admits no such
named scalar call.

VOLATILE expressions are never constant-folded as though their result were stable.

Bound function nodes retain the resolved semantic implementation identity rather than requiring executor-time name lookup.

## 19.10 Aggregate expressions

The aggregate expressions are:

```text
COUNT(*)
COUNT(expr)
SUM(expr)
MIN(expr)
MAX(expr)
AVG(expr)
```

A bound aggregate is semantically distinct from a scalar function call.

The Binder resolves aggregate argument coercions, return type, and nullability
from the aggregate registry semantics in §29.3.

The COUNT/SUM/MIN/MAX/AVG signatures and empty-input/nullability rules in
§29.3.2 are semantic binding rules as well as execution rules; execution uses
the same return type.

Aggregate DISTINCT and FILTER syntax are not registry entries; parser
recognition, generic function-call AST support, or a physical deduplication
operator does not make those aggregate forms legal.

For each query block, aggregate expressions are permitted only in its SELECT
list, HAVING, and ORDER BY. They are forbidden in JOIN ON, WHERE, GROUP BY,
LIMIT, OFFSET, INSERT VALUES expressions, UPDATE SET/WHERE, DELETE WHERE, DML
RETURNING, CREATE TABLE defaults, and every other schema-default expression.
An aggregate within an independently bound child SELECT belongs to that child
query block and does not become an aggregate of an enclosing expression merely
because the subquery occurs in one of those contexts.

An aggregate expression MUST NOT contain another aggregate expression owned by
the same query block. `SUM(COUNT(*))` and `MAX(SUM(x))` therefore produce
`BindError`; an aggregate inside a nested child query block is not same-block
nesting.

The binder assigns each aggregate occurrence the canonical semantic ordinal
defined by §29.3.7: first aggregate occurrence in ascending source-byte
position within that query block. Repeated shared occurrences retain the first
ordinal, and the ordinal remains attached through rewrites as that section
requires.

Binder validates aggregate placement before execution.

## 19.11 Aggregate query semantics

A query block is an aggregate query exactly when it contains a GROUP BY clause
or a bound aggregate expression owned by that block in SELECT, HAVING, or ORDER
BY. An aggregate in a nested child block does not classify the parent. An
aggregate query with no GROUP BY has one global group.

For binding legality, two grouping-key expressions are equal only when their
fully bound semantic expression trees are structurally equal. The comparison
ignores SourceSpan, parenthesis-only raw provenance, and output alias/display-
name metadata. It includes every semantic identity, including BindingId,
TableId/ColumnId where represented, literal value/type, resolved operator,
TypeId, explicit and inserted implicit casts, function/aggregate descriptor,
and child order.

This structural rule does not infer commutativity, associativity, arithmetic
or constant-folding equivalence, predicates, keys, uniqueness, or functional
dependencies. `GROUP BY a+b` therefore does not admit `b+a`, and grouping by a
primary/unique key does not admit unrelated ungrouped columns. This binding-
time expression-identity rule is distinct from §20.9's runtime value
equivalence among rows already assigned to a grouping key.

In an aggregate query, a SELECT, HAVING, or ORDER BY expression is group-valid
only when it is recursively composed from:

1. aggregates owned by the current query block;
2. complete exact grouping-key subtrees;
3. execution-independent scalar expressions containing no row-varying bound
   column outside such a complete key subtree; and
4. scalar operators, casts, CASE, and equivalent scalar nodes whose row-
   dependent children are themselves group-valid.

Thus a query with `GROUP BY a` may project `a+1`, while a query with
`GROUP BY a+1` may not project `a` merely because `a` is a subexpression of
the grouping key.

The architecture uses strict SQL-style grouping validation.

Arbitrary selection of non-grouped columns is rejected.

## 19.12 HAVING

The semantic clause order is:

```text
FROM / JOIN
WHERE
GROUP BY / aggregate
HAVING
SELECT projection
ORDER BY
LIMIT
```

HAVING may reference grouping expressions, aggregate expressions, and expressions derivable from them.

HAVING is legal only when the block is already an aggregate query by GROUP BY
presence or by an aggregate occurrence in SELECT, HAVING, or ORDER BY. HAVING
with no GROUP BY and no such aggregate produces `BindError`; HAVING alone does
not create a global group.

The architecture does **not** make SELECT aliases visible inside HAVING.

HAVING predicates must type-check as BOOLEAN.

## 19.13 ORDER BY resolution

ORDER BY may refer to:

```text
a bound expression
a SELECT-list alias
a 1-based SELECT-list ordinal
```

For a bare identifier, the binder resolves deterministically:

1. match only explicit SELECT-list AS aliases under §19.5,
2. bind the existing output slot on one match,
3. report ambiguous-output-alias `BindError` on multiple matches,
4. otherwise bind it as a normal input expression.

Only a bare, unparenthesized, unsigned integer-literal raw-AST node occupying
the complete ORDER BY expression is an ordinal attempt. Parenthesized,
explicitly signed, compound, cast, and FLOAT forms—including `(1)`, `+1`,
`-1`, `1+0`, `CAST(1 AS INT32)`, and `1.0`—bind as ordinary expressions.
This classification uses §18.13 raw provenance rather than reconstructed
SQL or a host numeric parser.

Let `N` be the bound SELECT output count. A bare ordinal magnitude in `1..N`
references that existing ordered output slot. Zero or a magnitude greater than
`N` produces `BindError`. An arbitrarily large syntactically valid unsigned
integer magnitude remains an out-of-range ordinal attempt; host signed-integer
conversion overflow never turns it into an ordinary expression.

Each ORDER BY item therefore resolves in this semantic order:

1. complete bare unsigned integer literal -> ordinal resolution;
2. complete unqualified identifier -> explicit-output-alias lookup;
3. every other form, or zero alias matches -> ordinary input-scope binding.

Alias and ordinal binding reference existing output semantic identity rather
than independently rebinding or reevaluating the output expression.

The logical ordering requirement is owned by §20.11; physical sorting is owned
by §30.1.

ORDER BY accepts exactly the §17.7.1 SQL-orderable types: INT32, INT64,
FLOAT64, VARCHAR, DATE, and TIMESTAMP. BOOLEAN has physical key order but no
SQL ordering semantics and is rejected as an ORDER BY key unless explicitly
cast to another supported type.

## 19.14 LIMIT and OFFSET

LIMIT/OFFSET binds without a relation-column or SELECT-output-alias namespace.
Its expression must be execution-start constant: it contains no row-dependent
or relational dependency and every child is recursively execution-start
constant.

Eligible leaves are scalar literals, including raw NULL after the contextual
typing below. Eligible composed forms include parentheses, explicit and
binder-inserted casts, unary/binary scalar operators, comparisons/predicates,
searched CASE, and other scalar constructs whose children are recursively
constant. A scalar call qualifies only when its resolved descriptor is
IMMUTABLE and every argument qualifies; the scalar-function registry is
empty, so no generic named source call satisfies this rule.

The following make a count expression nonconstant:

```text
bound column or relation reference
SELECT output-alias reference
aggregate expression
scalar/EXISTS/IN subquery
row-dependent value
any other relational dependency
```

Binding supplies an integral target context. The only accepted final semantic
types are INT32 and INT64; INT32 is normalized to INT64 using the §17.8.5
implicit widening edge. The binder does not synthesize FLOAT64, VARCHAR,
BOOLEAN, DATE, or TIMESTAMP conversion to an integer. A raw underconstrained
NULL receives contextual type `INT64 NULL`, but a final NULL count is not
reinterpreted as zero.

Binding failure categories are:

```text
not execution-start constant -> BindError
final nonintegral type        -> TypeError
```

Subject to §19.20's prerequisite-aware ordering, any more specific frozen
unsupported-semantic category remains authoritative. Successful binding
produces one INT64-normalized count expression represented explicitly by
§20.12 rather than hidden inside a scan.

The canonical binding pipeline is:

1. bind the raw count expression;
2. enforce execution-start-constant eligibility;
3. enforce the integral target context and INT64 normalization;
4. apply every mandatory scalar fold and constant-error timing rule under
   §17.10.2; and
5. retain the resulting folded/residual bound count expression for execution.

The folded/residual expression is the bound scalar representation remaining
after mandatory folding. It may be a literal constant or another admitted
execution-start-constant form that §17.10.2 does not require to become a
literal. Execution does not retain and reevaluate the original pre-fold source
tree merely to obtain the count.

A dominating constant scalar error occurs during binding with its §17.10.2
category. For example, `LIMIT 1/0` produces `DIVISION_BY_ZERO`, and
`LIMIT 9223372036854775807+1` produces `NUMERIC_OVERFLOW`, during binding;
`LIMIT 1+2` instead folds to the semantic INT64 value `3`.

At execution start, before any relational operator in the statement produces
or consumes rows, the engine obtains the final count exactly once from the
folded/residual representation. This is one execution-start count acquisition,
not a claim that mandatory folding performed no earlier scalar work. Scalar
work already completed during binding is not repeated.

After that acquisition, NULL or a negative INT64 produces `ExecutionError` at
the count-expression SourceSpan; a nonnegative INT64 is accepted. Thus
`LIMIT NULL` binds/folds as typed `INT64 NULL` and fails final-domain validation
at execution start, while `LIMIT -1` may fold to INT64 `-1` and fails at that
same boundary. Final count-domain validation alone does not turn NULL or a
negative value into a binding-time error. Values are never clamped or
interpreted as zero.

The normalized valid domain is nonnegative INT64. No additional
implementation-sized row-count maximum is a SQL semantic limit; inability to
produce an INT64 under §§17.6 and 17.8 retains the corresponding
arithmetic/cast/overflow result.

## 19.15 DISTINCT

`SELECT DISTINCT` is a semantic duplicate-elimination requirement.

Logical planning MUST represent duplicate elimination explicitly under §20.10,
for example as `LogicalDistinct` or an equivalent aggregate-like logical operator.

It is not merely an opaque projection flag that physical planning cannot reason about.

## 19.16 Searched CASE

The architecture supports exactly §17.9.1 searched CASE, including its closed common-
type rule and §17.7.3 branch evaluation. Simple CASE is not a semantic form.

## 19.17 IN-list semantics

An expression IN list binds as its own semantic node and uses exactly §17.9.2's
common-type, three-valued, and short-circuit rules. NOT IN is 3VL NOT of that
result.

The architecture does not require rewriting IN lists into OR chains.

The physical IN-list evaluation strategy is implementation-defined, but it
MUST preserve §17.7.3's once-only left evaluation and left-to-right
error/short-circuit boundary. A physical strategy cannot evaluate
dynamic/erroring list expressions in another order.

## 19.18 Subquery binding boundary

The binder supports only §20.14's closed uncorrelated forms. Every SELECT
subquery is a new query block with its own local FROM bindings, aliases,
aggregate/GROUP BY namespace, output names, and ORDER BY namespace under the
ordinary rules of this chapter.

Column lookup inside that block searches the local block only. It never falls
through to an enclosing query block for binding. For an unqualified name, an
outer diagnostic search occurs only after the complete local candidate set is
empty; one local match binds locally and multiple local matches produce local
ambiguity without outer search. For a qualified name, one local qualifier is
authoritative: a missing member remains a local unknown-member error and does
not search an outer qualifier. Only absence of the local qualifier permits an
outer diagnostic search.

If such locally unresolved name or qualifier would resolve in an enclosing
block, binding reports canonical `UnsupportedCorrelation` under
`UnsupportedFeature`; if it would not resolve in any enclosing block, it
reports the ordinary local unknown-name `BindError`. Local qualifiers shadow
outer qualifiers during this diagnostic lookup. No executable
`OuterRef`/correlated-column expression is produced.

The bound expression records a stable query-local subquery identity, exact
subquery kind, typed logical child plan, result type/nullability, and source
span. Name lookup never occurs during optimization or execution. Derived-table
binding and output-name rules are §20.14.3.

## 19.19 Parameters

Prepared-statement parameter syntax such as `$1` or `?` is outside the SQL
grammar and therefore has no binder semantics.

## 19.20 Binder/expression invariants

### 19.20.1 Binder error categories

The binder uses the semantic categories owned by §39.2; execution-stage
categories referenced below remain §39.3-owned. It does not create a generic
semantic-error category or convert operational resource failures into user
errors.

| Category | Chapter-19 conditions |
|---|---|
| `CatalogError` | missing table/index, DROP wrong object kind, create-name/object collision, or another catalog-object lookup failure owned by catalog semantics |
| `BindError` | unknown/ambiguous column, unknown qualifier/member, duplicate local relation qualifier, ambiguous ORDER BY output alias, invalid/out-of-range ORDER BY ordinal, illegal aggregate placement or nesting, grouped-query/HAVING legality, nonconstant LIMIT/OFFSET, duplicate INSERT target, duplicate UPDATE assignment, duplicate CREATE INDEX key, and other name/scope/semantic-shape failures not assigned a more specific category |
| `TypeError` | unknown type name; operator/cast/coercion mismatch; non-BOOLEAN predicate; incompatible CASE/IN common type; generic scalar/aggregate call with no legal name/shape/arity/star descriptor; resolved-call argument mismatch; unresolved overload ambiguity; nonintegral LIMIT/OFFSET; or index-ineligible key type |
| `ConstraintDefinitionError` | invalid CREATE TABLE constraint definition, including missing/repeated PK/UNIQUE member, multiple PRIMARY KEY declarations, and §21.12 default-definition restrictions |
| `UnsupportedFeature` | `UnsupportedCorrelation` and every other explicitly frozen unsupported semantic surface |
| `CardinalityError` | runtime scalar-subquery row cardinality and other existing cardinality owners; binding does not move those runtime checks earlier |

An unknown type name that §18.12.5 accepted structurally produces
`TypeError` at the type-name SourceSpan, never ParserError or CatalogError. For
`q.col`, absence of local `q` with no outer diagnostic match produces
`BindError` at the qualifier span; a present `q` with no `col` produces
`BindError` at the member span. An ambiguous unqualified column uses the whole
column-reference span.

`OutOfMemory` and `FrontEndResourceLimit` retain their §§18.17 and 39.2
meanings. Cancellation and lower-layer operational failures likewise preserve
their existing category and immediate behavior.

### 19.20.2 Deterministic ordinary error precedence

When more than one independently detectable ordinary semantic binding failure
exists, only failures whose semantic prerequisites resolved successfully are
candidates. An unknown column does not manufacture an operator-type error from
a hypothetical type; an unresolved call descriptor does not manufacture a
result-type error; and an unknown type name does not manufacture a cast-value
failure.

Among genuine candidate errors, the binder returns exactly one using:

1. earliest responsible SourceSpan start byte;
2. for equal starts, the shorter and therefore more-specific represented
   SourceSpan;
3. for an identical responsible span, this validation-class order:

```text
1. catalog/name resolution
2. structural semantic placement/shape
3. constraint-definition legality
4. type/coercion resolution
5. cardinality legality
```

An existing more-specific owner keeps any priority it explicitly defines.
Call registry/type failures belong to class 4; a successfully resolved
aggregate's illegal placement belongs to class 2. The prerequisite rule keeps
an argument's name-resolution failure from fabricating a call TypeError.
This rule orders independent source-originating binding diagnostics; it does
not reorder resource failure, cancellation, or runtime failure timing. Binder
traversal, hash/catalog container iteration, pointer addresses, allocation
layout, and thread scheduling MUST NOT select the returned semantic error.

### 19.20.3 Invariants

1. Binder output contains resolved IDs, types, nullability, source spans, and required semantic provenance.
2. Binder does not choose physical access/join algorithms.
3. BindingId is distinct from TableId and unique across one whole bound top-level statement.
4. Self-joins use distinct BindingIds, and a surviving relation occurrence retains its identity through logical rewrites.
5. Every relation occurrence exposes exactly one unique local qualifier; an explicit alias hides its base-table qualifier.
6. Local matches and ambiguity dominate outer correlation-diagnostic lookup.
7. Unqualified ambiguous columns are errors.
8. Qualified lookup distinguishes unknown qualifier from unknown column.
9. FROM/JOIN visibility follows source-associated left-subtree/current-right semantics and never becomes lateral implicitly.
10. `SELECT *` is expanded during binding in relation-source and descriptor-presentation order.
11. Duplicate top-level output display names remain distinct ordered outputs.
12. Only explicit AS aliases enter the ORDER BY output-alias namespace; SELECT aliases are visible nowhere else.
13. ORDER BY ordinal, alias, and ordinary-expression priority is deterministic and provenance-based.
14. Generated expression display names are exact source slices and are never name-resolution entries.
15. Bound expressions are immutable.
16. Executor-time SQL name resolution is forbidden.
17. Every bound expression has a resolved return type and conservative nullability.
18. Type coercion/operator/function resolution is centralized.
19. Explicit and implicit casts retain distinct provenance; an implicit cast carries its operand span.
20. Non-BOOLEAN predicate contexts are rejected.
21. Aggregate query-block ownership, placement, nesting, and structural grouping legality are validated before execution.
22. Grouping legality does not infer algebraic equivalence or functional dependencies.
23. LIMIT/OFFSET is an INT64-normalized execution-start constant; §17.10.2 folds or reports mandatory constant errors during binding, and execution obtains the folded/residual count once before relational row processing without reevaluating the pre-fold tree.
24. DISTINCT remains an explicit relational semantic requirement under §20.10.
25. CASE and IN preserve SQL NULL/three-valued behavior.
26. INSERT, UPDATE, DELETE, and RETURNING use only their declared §19.4.3 namespaces and §21.15 row images.
27. CREATE TABLE declaration, CREATE INDEX key, and DROP object-kind binding follow §19.4.4.
28. Runtime function/operator implementation IDs are not persisted as default metadata.
29. The closed §§17.2–17.10 registry alone determines literal types, overloads, casts, CASE/IN common types, and the empty scalar-function set.
30. A no-FROM query block has an empty local relation namespace; omission never creates an implicit relation, column, qualifier, or outer reference.
31. Ordinary semantic errors use prerequisite-aware SourceSpan ordering and never catalog/hash iteration order.
32. Binding performs no persistent catalog/file publication and does not alter §39.1 transaction consequences.
