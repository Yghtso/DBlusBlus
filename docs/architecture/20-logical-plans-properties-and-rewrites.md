# 20. Logical Plans, Properties, and Rewrites

## 20.1 Logical-plan node contract

A logical plan is an immutable relational-semantic tree.

Every logical node exposes at least:

```text
operator kind
children
output schema
derived/estimated logical-property placeholder
debug representation
```

Logical operators describe **what relation/result is required**.

They do not encode the physical algorithm used to produce it.

For example:

```text
LogicalGet    != SeqScan
LogicalJoin   != HashJoin
LogicalSort   != one particular sorting algorithm
```

Physical choices belong to downstream physical planning.

The canonical relational model is a **bag of row occurrences** unless a named
logical operator explicitly changes multiplicity or forms equivalence classes.
Two row occurrences remain distinct even when all of their SQL values are
equal. Multiplicity and semantic row order are separate dimensions: a bag does
not by itself establish any ordering among its occurrences.

`LogicalDistinct` and grouping explicitly collapse value-equivalence classes.
`LogicalFilter`, `LogicalJoin`, and `LogicalLimit` may remove or produce
occurrences under their own contracts, but they do not perform implicit
duplicate-equivalence collapse. No relational operator changes occurrence
multiplicity except as its named contract states.

## 20.2 Logical output schema and `LogicalSlotId`

Every logical output occurrence has one opaque runtime identity:

```text
LogicalSlotId
```

The canonical name deliberately distinguishes it from the persisted 16-bit heap `SlotId`.

A logical output slot contains metadata such as:

```text
LogicalSlotId
display name
LogicalType
nullable
lineage
system/internal flag
```

One `LogicalSlotId` identifies exactly one logical value occurrence flowing
through the logical plan. It is not:

```text
ColumnId
BindingId
heap SlotId
output ordinal
aggregate ordinal
an expression-equivalence class
a pointer or object address
a permanent physical vector position
```

`LogicalSlotId` values are unique across the entire top-level logical statement,
including nested query blocks, subquery side plans, derived-table children, DML
relational children, and every logical node belonging to that statement. Two
distinct logical output occurrences in that domain MUST NOT share an ID. An ID
MUST NOT be reused during the statement lifetime after its occurrence
disappears. Different top-level statements may reuse the same opaque
representation.

The identity is runtime-only. It is not catalog or WAL state, is not recovered,
and is not stored in tuple or page formats. Its numeric width, allocation order,
and concrete representation are not normative.

Slot creation and propagation obey these rules:

```text
LogicalGet          fresh slot per exposed BindingId + ColumnId occurrence
LogicalValues       fresh slot per output column position
LogicalProject      fresh slot per declared output position
LogicalAggregate    fresh slot per grouping-key and aggregate output occurrence
derived boundary    fresh outer slot per exported child output

LogicalFilter       preserve child slots
LogicalJoin         preserve child slots, left outputs then right outputs
LogicalDistinct     preserve child slots
LogicalSort         preserve child slots
LogicalLimit        preserve child slots
```

Any other node explicitly defined as schema-preserving follows the same
pass-through rule.

Every `LogicalProject` output position is a distinct occurrence even when two
positions reference the same child slot, contain equivalent expressions, or
produce equal values. Thus `SELECT a,a`, `SELECT a AS x,a AS y`, and
`SELECT a+1,a+1` each have two distinct Project output `LogicalSlotId` values.
Physical computation sharing does not merge those identities.

A derived-table boundary records a semantic mapping from each child
`LogicalSlotId` to a fresh outer `LogicalSlotId`, the derived relation's
`BindingId`, and its exported name and metadata. Child IDs do not become the
outer query block's exported identities directly.

A rewrite preserves an ID exactly when the same logical output occurrence,
with the same consumer-visible definition, lineage, and semantic boundary,
survives. A newly introduced or duplicated occurrence receives a fresh ID; an
eliminated occurrence's ID disappears and is not reassigned within the same
statement. Boundary elimination or substitution preserves an externally
visible surviving identity through an explicit semantic mapping. Equivalent
value computation alone does not establish occurrence identity.

These rules are explicit semantic metadata and never depend on pointer identity,
node allocation, container position, or traversal order. They preserve
self-join, alias, duplicate-column, computed-expression, rewrite, and hidden
system-value distinctions without prescribing an allocator or node
representation.

## 20.3 Logical operator family

The canonical logical operator family is:

```text
LogicalGet
LogicalValues
LogicalFilter
LogicalProject
LogicalJoin
LogicalAggregate
LogicalDistinct
LogicalSort
LogicalLimit
LogicalInsert
LogicalUpdate
LogicalDelete
LogicalCreateTable
LogicalCreateIndex
LogicalDrop
LogicalVacuum
LogicalAnalyze
LogicalExplain
```

Additional helper nodes are allowed when they represent real relational or statement semantics rather than a physical implementation algorithm.

## 20.4 `LogicalGet`

`LogicalGet` represents access to one logical base relation and contains at least:

```text
TableId
BindingId
required ColumnIds / output LogicalSlotIds
optional logical predicate set after rewrites
immutable table/schema descriptor reference
```

Its meaning is:

```text
produce rows of this logical relation
```

For each exposed column occurrence under the resolved descriptor,
`LogicalGet` creates a fresh `LogicalSlotId` and retains lineage equivalent to
`BindingId + TableId + ColumnId + SchemaVer/descriptor identity`. Self-joins
therefore have distinct `BindingId` and `LogicalSlotId` values even when they
refer to the same `TableId` and source `ColumnId` values.

Under the frozen transaction snapshot, `LogicalGet` emits exactly one logical
row occurrence per visible logical base row. Physical historical tuple versions
that are not independently visible logical rows do not add occurrences. Two
distinct visible rows with equal values remain two occurrences. `LogicalGet`
neither eliminates duplicates nor establishes a physical scan order.

It does not decide sequential versus index access.

Logical predicate attachment or pushdown may place predicates on the Get only
while preserving the original query semantics.

## 20.5 `LogicalValues`

`LogicalValues` represents a typed literal relation.

It supports cases such as:

```sql
INSERT INTO t VALUES (1, 'a'), (2, 'b');
SELECT 1;
```

Every row contains typed bound expressions.

`LogicalValues` creates one fresh output `LogicalSlotId` per logical column
position and emits exactly one row occurrence per listed row. Repeated listed
rows remain repeated occurrences. Their source order and provenance do not
become semantic result ordering unless another frozen rule explicitly
establishes such an order.

Every SELECT query block without FROM uses exactly one zero-column logical
input row. Ordinary filtering, aggregation, projection, ordering, and limiting
then apply; `SELECT 1` is the simplest
`LogicalValues(single zero-column row) -> LogicalProject` instance. §18.11.1
owns the optional-FROM syntax; this section owns the resulting logical source
and row-occurrence semantics.

## 20.6 `LogicalFilter`

`LogicalFilter` contains:

```text
child
BOOLEAN predicate
```

Its output schema, including all child `LogicalSlotId` values, is identical to
its child's schema.

For every child row whose retention decision the filter must determine, the
predicate evaluation is **demanded**. For each input row occurrence, TRUE
produces exactly one output occurrence, while FALSE and UNKNOWN produce none.
The filter never performs duplicate elimination. A logically equivalent form
may not demand the predicate for additional rows or suppress it for such a row
when that change could introduce or hide an observable error.

It does not prescribe a physical predicate-evaluation strategy.

## 20.7 `LogicalProject`

`LogicalProject` contains ordered output expressions:

```text
expr_0 -> output LogicalSlotId
expr_1 -> output LogicalSlotId
...
```

It creates one fresh `LogicalSlotId` for every declared output position. For
each input row occurrence it produces exactly one output row occurrence; equal
projected rows remain distinct, and projection performs no duplicate
elimination.

Projection may reorder values, duplicate a source value, calculate expressions, rename outputs, or drop unused input values.

For an ordinary `LogicalProject`, every declared projected expression is
required to form that Project output and is demanded for every input row that
produces an output row. An explicitly specialized semantic rule may establish
that a value is not demanded; in particular, §20.14's scalar-subquery rules and
EXISTS projection-value irrelevance remain authoritative. Such an exception
does not make arbitrary ordinary Project expressions optional.

It is the semantic boundary between an internal relation shape and a SELECT output shape.

## 20.8 Logical joins

### 20.8.1 Join node

`LogicalJoin` contains:

```text
join type
left child
right child
bound BOOLEAN condition when applicable
```

Join types are:

```text
INNER
LEFT
CROSS
```

CROSS has no join condition.

Let `L` and `R` be the left and right input bags of row occurrences. INNER and
LEFT joins conceptually consider occurrence pairs from `L x R`; this specifies
semantic multiplicity and does not require materializing a Cartesian product or
using any particular join algorithm.

INNER JOIN evaluates its ON predicate for each semantically required pair under
Chapter 17 and §20.17. A TRUE result emits exactly one joined occurrence;
FALSE or UNKNOWN emits none. Every TRUE pair is preserved, so duplicate input
occurrences and duplicate matching values contribute their full multiplicative
occurrence count. This is not first-match or existence semantics.

For each left occurrence, LEFT JOIN emits one joined occurrence for every right
occurrence whose ON result is TRUE. FALSE and UNKNOWN are nonmatches. If no
right occurrence produces TRUE for that left occurrence, the join emits exactly
one occurrence consisting of the left occurrence plus NULL for every right-side
output. It emits no additional NULL-extended occurrence when at least one TRUE
match exists.

CROSS JOIN emits exactly one occurrence for every pair in `L x R`. If
equivalent left and right values have multiplicities `m` and `n`, the paired
output multiplicity is `m*n`.

Empty inputs have these exact results:

```text
INNER: left empty or right empty -> empty
CROSS: left empty or right empty -> empty
LEFT:  left empty                -> empty
LEFT:  right empty               -> one NULL-extended occurrence per left occurrence
```

Every join output schema contains all left-child outputs in their existing
schema order followed by all right-child outputs in their existing schema
order. Child `LogicalSlotId` values pass through unchanged; no fresh IDs are
created merely by crossing the join boundary.

No logical join establishes semantic row order. In particular, it promises no
left-major, right-major, nested-loop, hash, merge, RID, or scan order. Output
schema order is defined; result-row order is not.

For a join with a condition, predicate evaluation is demanded only for
candidate row pairs that the logical join semantics require to test. Logical
semantics do not prescribe a physical pair-generation order. Moving a
potentially erroring predicate to rows or pairs outside that demand domain
requires the exact rewrite-safety proof defined by §20.17.

The node never selects HashJoin/NestedLoop/MergeJoin.

### 20.8.2 Bound-predicate handoff

Join predicates arrive fully bound under §§19.2.1 and 19.3. Every bound column
reference already identifies its relation occurrence, and logical-plan
construction maps that identity to the corresponding `LogicalSlotId` under
§20.2. `LogicalJoin` consumes the resolved BOOLEAN expression without
rebinding names, qualifiers, or correlation.

ON and WHERE remain semantically distinct.

### 20.8.3 LEFT JOIN null extension

Every right-side output of a LEFT JOIN becomes logically nullable in the join
output even when the base column is `NOT NULL`. Left-side nullability remains
that of the left child.

The logical schema records that contextual nullability change without altering
the catalog `NOT NULL` metadata, source descriptor, `TypeId`, `BindingId`,
`LogicalSlotId`, or lineage. Catalog nullability and logical output nullability
are distinct concepts.

### 20.8.4 Predicate decomposition

For INNER/CROSS joins, optimizer metadata may decompose conjunctions into:

```text
equi-join predicates
left-local predicates
right-local predicates
residual predicates
```

The original semantics remain reconstructible/debuggable.

Outer-join decomposition/pushdown remains constrained by null-extension semantics.

## 20.9 `LogicalAggregate` and grouping equality

`LogicalAggregate` contains:

```text
grouping expressions
aggregate expressions
child
```

Its outputs consist only of group-key outputs and aggregate outputs. It creates
a fresh `LogicalSlotId` for every grouping-key output and every logical
aggregate occurrence output.

`LogicalAggregate` emits exactly one row occurrence per runtime group under the
existing grouping-equivalence contract. A no-key global aggregate forms its
one group even over empty input, while an empty input with explicit grouping
keys forms no groups, as frozen by §29.5 and §20.14.10.

Every distinct bound source aggregate occurrence becomes one distinct logical
aggregate occurrence, and each aggregate entry carries the canonical aggregate
ordinal assigned by Chapter 19 under §29.3.7. Chapter 20 neither derives nor
renumbers that ordinal from logical traversal. Occurrences with identical
descriptors, arguments, output types, and values remain logically distinct when
their ordinals differ, and their output `LogicalSlotId` values are distinct.
The ordinal identifies a source aggregate occurrence within its query block;
the slot identifies that occurrence's logical output within the statement.

A downstream reference to an existing aggregate output reuses that logical
output identity and creates no aggregate occurrence or ordinal. Thus the
`ORDER BY s` in `SELECT SUM(x) AS s ORDER BY s` consumes the existing output
selected by Chapter 19 alias resolution.

Logical rewrites MUST NOT renumber or compact aggregate ordinals, reuse a
removed ordinal, merge distinct logical aggregate occurrences, duplicate one
logical occurrence under the same ordinal, or move an occurrence to another
query block. If an occurrence is legitimately removed under §20.17, its
ordinal disappears and every remaining ordinal stays unchanged.

Physical execution may share computation for equivalent logical aggregate
occurrences only behind a semantic mapping that preserves every distinct
logical occurrence, `LogicalSlotId`, aggregate ordinal, diagnostic provenance,
output mapping, and Chapter 29's lowest-ordinal aggregate-error behavior. This
permission does not merge logical occurrence identity or require a particular
sharing strategy.

Expressions above the aggregate refer to those outputs, not arbitrary pre-aggregate columns.

Grouping keys and aggregate arguments are demanded only for the logical input
rows to which the aggregate applies. A HAVING predicate is demanded for the
groups whose retention its `LogicalFilter` must determine. Chapter 29 and
§20.14 retain their more specific aggregate and subquery demand rules.

Grouping and DISTINCT use SQL grouping-equivalence semantics:

```text
NULL values belong to the same group / duplicate class
-0.0 and +0.0 are equivalent
all canonical NaNs are equivalent under the FLOAT64 semantics
VARCHAR uses binary byte equality
```

This grouping equivalence is intentionally different from ordinary SQL `=` with NULL, which returns NULL.

Every grouping-key component emitted by `LogicalAggregate` is materialized as
the §17.10.3 `CanonicalGroupingRepresentative` of its class. Thus a group
containing `-0.0` and `+0.0` emits `+0.0`, and a group containing multiple
grouping-equivalent NaN encodings emits the canonical Chapter-17 NaN. Composite
group keys apply the mapping independently to each component. This selects the
output value representation without changing grouping equivalence, group or
aggregate multiplicity, output schema, `LogicalSlotId`, or ordering semantics.

Every hashing or comparison implementation for aggregate/DISTINCT must agree
with this contract.

## 20.10 `LogicalDistinct`

`SELECT DISTINCT` is represented as explicit duplicate-elimination semantics using `LogicalDistinct` or an explicitly equivalent aggregate-like node.

`LogicalDistinct` preserves the child schema and `LogicalSlotId` values while
collapsing each class under the grouping-equivalence rule to one row
occurrence. The emitted row is the componentwise
§17.10.3 `CanonicalGroupingRepresentative` of that class. Consequently,
DISTINCT over `-0.0` and `+0.0` emits one row containing `+0.0`, and DISTINCT
over grouping-equivalent NaN encodings emits one row containing the canonical
Chapter-17 NaN. No first input, hash insertion, worker, spill replay, or sorted
physical class member defines the result representation. This canonicalization
changes neither the preserved child schema nor its `LogicalSlotId` values.
`LogicalDistinct` is an explicit exception to the bag multiplicity baseline.

DISTINCT is not hidden as an opaque projection flag.

The optimizer and physical planner must be able to reason about its cost, keys, ordering opportunities, and memory requirements.

## 20.11 `LogicalSort`

`LogicalSort` contains ordered sort keys:

```text
expression / LogicalSlotId
ASC | DESC
resolved NULL order
```

It preserves the child schema, all child `LogicalSlotId` values, and exactly
the same bag of row occurrences. It changes only semantic ordering and performs
no duplicate elimination.

Default ordering is:

```text
ASC  -> NULLS FIRST
DESC -> NULLS LAST
```

This matches the existing ascending NULL-first B+ key order and its natural reverse.

The resolved NULL-order choice is stored directly in each logical sort key;
physical sorting and index-order matching never infer it.

`LogicalSort` establishes the resolved key-sequence order. Rows equal on every
ORDER BY key have no stable relative-order guarantee under §30.1. Section 30.3
owns the canonical physical comparator, including SQL type and FLOAT64 ordering,
that realizes this logical requirement.

Each sort-key expression is demanded only for the logical rows to which the
sort applies. This demand rule does not establish a result-row order beyond the
resolved logical ordering contract.

## 20.12 `LogicalLimit`

`LogicalLimit` contains:

```text
optional limit
offset
```

Section 19.14 exclusively owns LIMIT/OFFSET admissibility, execution-start
constancy, type resolution, INT32-to-INT64 normalization, mandatory folding,
and NULL/negative count validation. The logical-plan representation carries
§19.14's folded/residual count expressions until their one execution-start
acquisition. The relational selection contract below receives only the
resulting validated nonnegative INT64 offset and optional validated nonnegative
INT64 limit. It introduces no count-expression error or reevaluation.

`LogicalLimit` applies OFFSET first and LIMIT second. For child occurrence
cardinality `n`, validated offset `o`, and validated limit `l` when present:

```text
after_offset = max(0, n - o)

without LIMIT: output cardinality = after_offset
with LIMIT:    output cardinality = min(after_offset, l)
```

The contract never requires computing `o + l`, so it introduces no arithmetic
overflow path. LIMIT alone means offset zero; OFFSET alone returns all
occurrences remaining after the offset. OFFSET zero removes nothing, LIMIT zero
returns an empty bag, a limit larger than the remaining cardinality returns all
remaining occurrences, and an offset at least as large as the child cardinality
returns an empty bag. None of these cases is an error.

For a semantically ordered child, the operator skips the first `o` ordered
occurrences, emits at most the next `l` occurrences when a limit exists, and
preserves the surviving order. For a child with no semantic row order, there is
no canonical first occurrence: it returns any sub-bag with the exact resulting
cardinality allowed by §20.14.10, and the output remains unordered. Heap, page,
RID, index, hash-iteration, thread, and physical scan order never become
semantic.

`LogicalLimit` preserves the child output schema, `LogicalSlotId` values,
display metadata, types, nullability, and lineage unchanged. It only removes
row occurrences and never deduplicates them.

Section 27.9 owns `PhysicalLimit` conformance to this logical contract, and
§35.20 owns its optimizer cardinality derivation. Any LIMIT pushdown or
alternative physical operator remains conforming only when it preserves the
semantics defined here.

## 20.13 DML logical nodes and hidden system slots

### 20.13.1 Hidden system values

Logical plans may carry internal values such as:

```text
target physical RID
```

Such slots are marked `system/internal`.

They never appear through `SELECT *` or ordinary user-visible column enumeration.

Projection pruning MUST preserve internal slots required by downstream DML.

### 20.13.2 `LogicalInsert`

Contains:

```text
target TableId
canonical target ColumnIds
input relation
column coercion/default expressions
optional RETURNING projection
```

Input may be VALUES or SELECT.

Both use the same logical insert contract.

### 20.13.3 `LogicalUpdate`

Contains:

```text
target TableId
target BindingId
child producing target RID + required old values
assignment expressions keyed by ColumnId
optional RETURNING
```

The child must preserve the exact target RID needed by Chapter 15's write protocol.

The planner requests old values required to construct the complete new physical tuple version and to evaluate assignments/RETURNING.

### 20.13.4 `LogicalDelete`

Contains:

```text
target TableId
child producing target RID
required old values
optional RETURNING
```

DELETE targets one base table.

The child preserves exact row identity through its supported filters/joins.

Physical index cleanup still belongs to vacuum, not LogicalDelete.

### 20.13.5 `LogicalAnalyze`

`LogicalAnalyze` is a maintenance statement node containing at least:

```text
target TableId
immutable target TableDescriptor
analyzed SchemaVer
visible analyzed ColumnIds
visible analyzed IndexIds
```

It has no ordinary relational output requirement in the defined architecture.

Its semantic meaning is:

```text
collect one new statistics version for this resolved table
```

It does not choose the physical statistics algorithms, mutate catalog descriptors in place, or bypass transaction/catalog visibility.

The resolved descriptor/schema version is fixed at binding/planning time for that statement attempt.

## 20.14 Subquery semantics and support

This section is the canonical closed subquery contract. Parser recognition,
generic AST capacity, or an optimizer rewrite does not add another form.
The architecture uses **uncorrelated query blocks only** and has no executable OuterRef, Apply,
parameterized-rescan, LATERAL, or decorrelation requirement.

### 20.14.1 Closed support matrix

`lazy side plan` below means the statement-attempt-scoped physical fallback in
§20.14.7; it is not a promise that an optimizer rewrite succeeds.

| Subquery form | Support | Correlation | Required output arity | Bound result | Zero rows | More rows | Canonical logical form | Required physical fallback |
|---|---|---|---:|---|---|---|---|---|
| scalar `(SELECT ...)` | supported | forbidden | exactly 1 column | that column's TypeId, always nullable | typed NULL | second successfully produced row raises `CardinalityViolation` | `BoundScalarSubquery` plus independent logical child | lazy scalar side plan, consume at most two final rows |
| `EXISTS (SELECT ...)` | supported | forbidden | any arity, including zero demanded value columns | non-null BOOLEAN | FALSE | TRUE after first row; later rows not demanded | `BoundExistsSubquery` plus independent logical child | lazy existence side plan |
| `NOT EXISTS (SELECT ...)` | supported | forbidden | as EXISTS | non-null BOOLEAN | TRUE | FALSE | `NOT(BoundExistsSubquery)` | EXISTS fallback then 3VL NOT (never UNKNOWN) |
| scalar `x IN (SELECT y ...)` | supported | forbidden | exactly 1 column | nullable BOOLEAN | FALSE | set/NULL-marker semantics below | `BoundInSubquery` plus independent logical child | lazy complete IN-build side plan |
| scalar `x NOT IN (SELECT y ...)` | supported | forbidden | exactly 1 column | nullable BOOLEAN | TRUE | 3VL NOT of IN | `NOT(BoundInSubquery)` | IN-build fallback then 3VL NOT |
| `FROM (SELECT ...) [AS] alias` | supported | forbidden | any positive relation arity | relation binding | empty relation | ordinary relation rows | `LogicalSubqueryScan` slot/name boundary over child | ordinary relational child; no mandatory materialization |
| correlated scalar/EXISTS/IN/NOT IN | unsupported | not applicable | — | bind-time `UnsupportedCorrelation` | — | — | no executable node | none |
| row-valued left operand, row-valued IN, or multi-column scalar subquery | unsupported | forbidden | — | bind-time unsupported feature/type error | — | — | none | none |
| `ANY`, `SOME`, `ALL`, or another quantified comparison | unsupported | forbidden | — | unsupported feature | — | — | none | none |
| `LATERAL` or any FROM-item outer reference | unsupported | forbidden | — | unsupported feature/correlation error | — | — | none | none |
| CTE, recursive CTE, UNION/INTERSECT/EXCEPT subquery, or data-modifying subquery | unsupported | forbidden | — | unsupported feature | — | — | none | none |

`NOT EXISTS` and `NOT IN` are canonical Boolean NOT wrappers rather than
separate truth systems. A parser may use dedicated AST spelling, but binding
normalizes to the forms in the table.

### 20.14.2 Query blocks, scopes, and correlation rejection

Every SELECT, including a nested SELECT, creates one lexical query block. Its
local FROM namespace, aliases, wildcard expansion, aggregation/GROUP BY/HAVING,
SELECT outputs, and ORDER BY aliases/ordinals follow Chapter 19. The local block
does not inherit enclosing relation bindings. Inner aliases shadow nothing in
the parent because parent lookup is not attempted; inner names also never leak
out except through a derived table's named output columns.

Parent links may exist for diagnostics, but binding uses them only to
distinguish an attempted outer capture from an ordinary unknown name. It MUST
NOT emit an executable
`BoundCorrelatedColumnRef`, infer correlation from a matching name at runtime,
or accept syntax on the assumption that the optimizer will decorrelate it.
Nested subqueries are legal only when each nested block is independently
uncorrelated from every lexical ancestor.

Correlation is outside the SQL architecture; supporting executable
correlated references requires an architecture revision.

A nested SELECT may omit FROM under §18.11.1 syntax. It then receives its own
§20.5 one-row, zero-column source and empty local relation namespace; no
enclosing relation binding becomes visible.

### 20.14.3 Derived tables

The syntax is:

```sql
FROM (SELECT ...) [AS] alias
```

The relation alias is mandatory; `AS` is optional. A derived-column alias
list such as `d(a,b)` is unsupported. The child output columns become exactly
one relation binding under that alias. A child output has a usable derived
column name only when §19.5 gives it an explicit SELECT alias or it is a direct
column reference with its source-column name. An expression with only a
generated presentation name must be explicitly aliased before it can define a
derived table. Derived output names must be unique; duplicates are a bind error
at the derived-table boundary rather than an implementation-chosen reference.

The boundary maps each child output `LogicalSlotId` to one fresh outer
`LogicalSlotId`, the derived relation `BindingId`, and its exported name and
metadata. Child IDs do not become outer exported identities directly. Child
table aliases and hidden columns are not visible outside. `SELECT d.*` expands
the unique named outputs in child projection order.

The derived boundary preserves the child's bag of row occurrences exactly;
slot and namespace remapping does not alter multiplicity. Only an explicit
operator inside the child, such as DISTINCT or grouping, may already have
changed that multiplicity.

`LogicalSubqueryScan` expresses that namespace/slot boundary. It is not a
materialization barrier. The optimizer may inline it, push safe predicates, or
reorder its relational child exactly when ordinary slot, outer-join,
aggregation, LIMIT/OFFSET, ordering, and error semantics remain equivalent. If
the boundary survives physical planning, its fallback is a streaming slot-map/
projection over the independently planned child.

A derived table is not a scalar once-result cache. It has no outer parameters
and is planned as one ordinary relation occurrence. A physical join may
materialize or rewind that child when its normal algorithm requires, but may
not parameterize the rescan by an outer row or obtain another snapshot.

### 20.14.4 Scalar subquery semantics

A scalar subquery binds only after its final SELECT output has exactly one
column. Its result TypeId is that column's bound type. The architecture conservatively marks
every scalar-subquery expression nullable even when the selected expression is
NOT NULL, because zero final rows produce one typed NULL scalar value.
The resulting typed expression participates in surrounding Chapter 17 operators,
CASE common-type resolution, assignment coercion, and aggregate signatures
without any special subquery cast.

Runtime consumes the subquery's **final** rows after its WHERE, aggregation,
HAVING, projection, DISTINCT, ORDER BY, OFFSET, and LIMIT semantics:

```text
0 final rows -> typed NULL
1 final row  -> that row's selected scalar, including SQL NULL
2nd final row -> CardinalityViolation / SQL CardinalityError
```

The empty subquery does not remove an outer row. For example, the no-FROM outer
query `SELECT (SELECT x FROM t WHERE FALSE)` emits its normal one row containing
NULL.

The scalar consumer has a semantic final-row demand of two, independent of the
optimizer's costing-only `required_rows`. Blocking child operators may consume
all input needed to establish their final rows, but once two final output rows
have been successfully constructed no third final projection row or later
streaming work is evaluated. Producing a final row includes evaluation of its
selected expression. Therefore an error while constructing the first or second
final selected value precedes cardinality failure; after two successfully
constructed rows, cardinality failure precedes every later-row error. No first,
last, or arbitrary row is returned.

An optimizer may remove the cardinality check only from exact logical proof that
the final subquery cardinality is at most one. `LIMIT 1` supplies such a cap;
estimated cardinality, `required_rows=1`, uniqueness statistics, or a plan that
happens to stop early do not.

### 20.14.5 EXISTS and NOT EXISTS

EXISTS returns non-null BOOLEAN TRUE iff the subquery has at least one final
relational row after WHERE/grouping/HAVING/OFFSET/LIMIT; otherwise it returns
FALSE. NOT EXISTS is ordinary Boolean NOT and is also never UNKNOWN. The
existence consumer stops at the first demanded row and does not evaluate later
rows.

SELECT projection values inside EXISTS are bound for valid names/types and for
determining query-block aggregation shape. A value is projection-only only when
it is unnecessary to determine final-row existence. Such projection-only
expressions and aggregate arguments/results, irrelevant DISTINCT value
construction, and ORDER BY key expressions may be removed without demanding
their runtime/constant errors. The binder must not eagerly fold an irrelevant
projection to an error. FROM, WHERE, grouping keys, HAVING, and OFFSET/LIMIT
work that determines whether a row exists remains demanded and propagates its
errors until existence is known.

DISTINCT retains its ordinary projected-value equivalence classes when their
count affects whether a row remains after OFFSET. In that case, required
projected values, including aggregate inputs/results used by those values,
are demanded to construct the classes, and their errors retain their existing
owners. An optimizer may remove that work only with an exact proof preserving
final-row existence and every otherwise demanded error. Estimated NDV or row
count is insufficient. With two rows whose projected k values are `(1),(1)`,
`EXISTS(SELECT DISTINCT k FROM t OFFSET 1)` is FALSE; with `(1),(2)` it is TRUE.

Absent a relevant OFFSET, DISTINCT does not change empty versus nonempty, so
its projection-only value work remains irrelevant. A zero OFFSET has the same
property; an actual LIMIT0 establishes no final row. These facts do not erase
independently demanded FROM/WHERE/grouping/HAVING work or change count-expression
validation. Demand-sensitive folding for a relevant projection follows §17.10.2's
relational-subquery rule; relevance alone is not proof that an input row exists. The architecture conservatively executes a global
aggregate's ordinary FROM/WHERE child work before that aggregate emits its one
row, even though row existence is known from the aggregate shape; an optimizer
may skip that work only with an exact proof that doing so suppresses no demanded
error.

Consequently:

```text
EXISTS(SELECT 1 / 0 FROM t)
    -> no division error; TRUE iff t's relational input yields a row

EXISTS(SELECT COUNT(*) FROM t)
    -> after successful ordinary child evaluation, TRUE even when t is empty,
       because a global aggregate emits one row
```

An aggregate referenced by HAVING remains demanded. A grouped aggregate may
produce zero or more groups. `LIMIT 0` makes EXISTS FALSE; a positive LIMIT does
not prevent first-row early stop after OFFSET has been satisfied.

### 20.14.6 IN and NOT IN

The left operand and subquery output are scalar only. Binding resolves exactly
one Chapter 17 equality overload/common numeric type and inserts only the casts that
registry permits. There is no subquery-specific coercion. Failure to resolve
ordinary `=` is a bind-time TYPE_ERROR.

For left value `x` and every final subquery value `y_i`, IN has repeated-equality
3VL semantics:

```text
any (x = y_i) is TRUE                         -> TRUE
no TRUE and at least one comparison UNKNOWN   -> UNKNOWN
otherwise                                     -> FALSE
```

Equivalently, after the canonical build records its non-NULL values,
`build_has_null`, and `build_is_empty`:

| Build state / probe | `x IN build` | `x NOT IN build` |
|---|---|---|
| empty, including `x IS NULL` | FALSE | TRUE |
| nonempty and `x IS NULL` | UNKNOWN | UNKNOWN |
| equal non-NULL value exists | TRUE | FALSE |
| no equal value, `build_has_null=true` | UNKNOWN | UNKNOWN |
| no equal value, no NULL | FALSE | TRUE |

NOT IN is exactly 3VL NOT of IN. WHERE/HAVING/ON retain only TRUE, but UNKNOWN
remains observable in projection and CASE. Subquery duplicates do not change
truth and may be deduplicated. Any number of NULL rows is one Boolean marker.
Equality/hash normalization is exactly §17.10.3, including signed-zero, NaN,
VARCHAR, and promoted numeric semantics.

Unlike Chapter 17's source-ordered expression-list IN, an uncorrelated IN subquery is
an unordered relational build. On first demand, it consumes and validates the
entire limited/final subquery result before any probe result is returned. A
subquery output-expression, memory, or spill error anywhere in that build
therefore precedes a possible matching probe value. Once build succeeds, left
values probe it vectorized.

For the occurrence's first demanded outer selection, Chapter 17's child-evaluation order first
evaluates the left scalar expression. If that evaluation fails, the still-
dormant build does not start. If at least one active row reaches the IN
operation, the complete build initializes once before any of those rows is
probed. Later selections reuse it.

### 20.14.7 Logical fallback and physical-planning handoff

Each bound expression-subquery occurrence has one stable query-local identity
and an independent logical child. Its logical representation preserves one of
three semantic modes:

```text
SCALAR_ONE_VALUE
EXISTS_BOOLEAN
IN_VALUE_SET
```

NOT wrappers remain ordinary bound Boolean nodes. The representation does not
pretend the child is an ordinary local column expression, merge it with a
different textual occurrence, or require a semi/anti join rewrite.

Every supported expression-subquery mode has a mandatory executable fallback
that does not depend on a successful semi/anti/marker rewrite:

```text
SCALAR_ONE_VALUE
    preserve lazy demand and the zero/one/more-than-one contract of §20.14.4

EXISTS_BOOLEAN
    preserve lazy existence demand and projection irrelevance under §20.14.5

IN_VALUE_SET
    preserve complete final-child consumption and the match/NULL/empty state
    required by §20.14.6
```

The fallback remains keyed by the bound occurrence and preserves the
statement-attempt identity, Chapter-17 evaluation order, cardinality, 3VL,
error, snapshot, and CommandId contracts in §§20.14.4–20.14.8. Section 22.4
owns the physical operator vocabulary, while §37.17 owns physical subquery
planning and selection among conforming fallback or exactly proven rewrite
alternatives. Chapters 24–26 own physical memory, expression-state, and
pipeline mechanics. Chapter 20 prescribes none of those implementation
representations or algorithms.

### 20.14.8 Lazy statement-attempt ownership, snapshot, and CommandId

An uncorrelated expression subquery is evaluated **at most once per bound
occurrence per statement attempt, when first semantically demanded**. It is not
globally hoisted to statement start. If no row/control-flow path demands it, it
does not execute. Once successfully evaluated, its value/build state is cached
for all later outer rows in that attempt. A failed side plan fails the statement;
it is never re-executed to seek a different outcome.

Chapter 17 evaluation order remains authoritative:

- a subquery in an unselected CASE arm does not run;
- a subquery in a skipped AND/OR right operand does not run;
- a subquery predicate over an empty outer input may remain undemanded;
- when a vector selection contains at least one row that demands the occurrence,
  it initializes once and serves exactly that demanded selection.

An internal READ COMMITTED whole-statement retry discards every prior attempt's
subquery state and reevaluates under the retry's fresh statement snapshot. It
does not share stale side state merely because the logical statement retains
its CommandId.

Every subquery and derived child uses the containing attempt's one effective
snapshot and ReadEpochGuard. It never captures a fresh snapshot. It consumes no
new CommandId: tuple visibility uses the containing statement's CommandId and
the ordinary Chapter-10 current-command rules. Query-local subquery execution
does not create a nested transaction or independently own logical locks.

### 20.14.9 Legal expression/statement contexts

Supported uncorrelated expression subqueries use the ordinary surrounding
Chapter 17 type rules in these contexts:

| Context | Rule |
|---|---|
| SELECT projection, WHERE, HAVING, JOIN ON | scalar/EXISTS/IN/NOT forms supported; predicates still require BOOLEAN |
| searched CASE and AND/OR | supported with §17.7.3 short-circuit/lazy demand |
| GROUP BY expression | supported when the resulting scalar is groupable under Chapter 20; it remains one uncorrelated once-result expression |
| outer ORDER BY expression | supported when the result is SQL-orderable under Chapter 17; BOOLEAN EXISTS/IN results are therefore not orderable without explicit legal cast |
| aggregate argument | supported when the resulting scalar TypeId matches an exact §29.3 signature; the subquery is a separate inner query block |
| nested subquery | supported only when independently uncorrelated at every boundary |
| UPDATE/DELETE WHERE | supported during the ordinary read/target-spool phase |
| UPDATE assignment, INSERT VALUES, INSERT SELECT expression, DML RETURNING | supported; assignment coercion and row-image scope remain Chapter 21 |
| default, generated/catalog expression, CREATE INDEX key, constraint definition | subqueries forbidden; defaults remain closed folded scalars |
| LIMIT/OFFSET expression | subqueries forbidden in the defined architecture; Chapter 19's bound integral execution-start constant does not include a runtime side plan |
| CHECK/foreign-key expression | not applicable because those constraints are unsupported |

Only SELECT query blocks may appear as subqueries. INSERT/UPDATE/DELETE/DDL,
data-modifying CTEs, and DML RETURNING used as a subquery source are unsupported.
Subquery errors in DML follow §39.1: before that statement's first published
write they may be statement-only; after publication they require MUST_ABORT.
Target spooling/retry and buffered RETURNING behavior are unchanged.

### 20.14.10 Aggregation, DISTINCT, ORDER BY, LIMIT, and OFFSET

A subquery may use the ordinary SELECT aggregation, DISTINCT, ORDER BY,
LIMIT, and OFFSET clauses. Global aggregate over empty input emits one row;
therefore `(SELECT COUNT(*) FROM empty)` returns `0`, while
`EXISTS(SELECT COUNT(*) FROM empty)` is TRUE. Grouped scalar subqueries remain
subject to zero/one/many final-row cardinality.

DISTINCT is applied before scalar cardinality checking and may legitimately
reduce duplicate final rows to one. No implementation may deduplicate scalar
rows without an explicit DISTINCT or exact equivalence proof. IN may deduplicate
because multiplicity never changes its result.

ORDER BY inside a subquery is accepted under the ordinary grammar. LIMIT/OFFSET
uses that order when present, but a derived table advertises no outer ordering
unless an ordinary physical property is explicitly preserved and consumed.
Without a consumer-visible order effect, an optimizer may remove the sort only
when removing ORDER BY-key evaluation cannot suppress a semantically demanded
error. EXISTS projection-irrelevance follows §20.14.5 instead.

The bound nonnegative constant LIMIT/OFFSET semantics are ordinary:

- actual `LIMIT 0` proves the subquery empty;
- scalar `LIMIT 1` proves at most one final row, including after OFFSET;
- EXISTS skips final relational rows, including required DISTINCT classes under
  §20.14.5, and stops at the first remaining row unless LIMIT 0;
- IN/NOT IN uses exactly the post-OFFSET/LIMIT result;
- OFFSET alone does not cap scalar cardinality.

LIMIT/OFFSET without ORDER BY does not create an ordering guarantee. Thus a
scalar subquery capped by `LIMIT 1` without ORDER BY may select any row allowed
by the ordinary unordered child relation, exactly like top-level SELECT; it
does not acquire a hidden heap/index/implementation “first row” contract.

Optimizer `required_rows` remains a cost/search objective, never a semantic
LIMIT. Scalar's two-row cardinality probe and EXISTS's one-row existence demand
are consumer semantics, not exact-emptiness proof for the child.

### 20.14.11 Rewrite and proof boundary

Semantic support never depends on decorrelation or join conversion. Rewrites
must preserve lazy demand, snapshot/CommandId, 3VL, duplicate behavior,
cardinality checks, and observable error precedence.

For a row-rejecting filter context, `x IN (subquery)` may become a semi-join
because both FALSE and UNKNOWN reject the outer row, provided equality casts,
subquery errors, and first-demand execution are preserved. Expression-valued IN
requires the equivalent of three marks—match, build_has_null, and build_empty—
or the canonical side build; a plain semi-join cannot produce UNKNOWN.

NOT EXISTS may become an anti-join/existence guard when the same demand/error
semantics are preserved. NOT IN may become an ordinary anti-semi join only in a
row-rejecting context after exact trusted facts prove the subquery comparison
value NOT NULL and prove the left value NOT NULL. A nullable left may instead
use a left `IS NOT NULL` rejection only when the RHS is exactly proven nonempty,
or when an explicit empty-build branch first preserves §20.14.6's TRUE result
for an empty RHS, including a NULL left. Unconditional NULL-left rejection is
not equivalent when the RHS may be empty. Otherwise it requires a NULL-aware
marker/build.
Statistics, estimated zero NULL fraction, sampled values, and required_rows are
not proof under §§34.1 and 35.2.

Exact `is_provably_empty` consequences are:

| Form | Exact empty-child rewrite |
|---|---|
| scalar | typed NULL |
| EXISTS | FALSE |
| NOT EXISTS | TRUE |
| IN | evaluate left once, then FALSE, even for NULL left |
| NOT IN | evaluate left once, then TRUE, even for NULL left |
| derived table | empty relation with the same output schema |

Only §§20.17.10/35.2 proof provenance authorizes these rewrites. A numerical
estimate of zero never does. The IN/NOT IN left evaluation may itself be
removed only when exact reasoning under Chapter 17 proves it cannot raise or carry another
observable demand; empty-build truth alone is not permission to suppress it.

### 20.14.12 Error precedence, EXPLAIN, and persistence

Observable precedence is limited and exact:

- scalar propagates errors required to produce its first/second final row; a
  successfully produced second row then raises cardinality before later work;
- EXISTS propagates demanded work until the first row, then suppresses later
  rows and irrelevant projection/ORDER BY value errors;
- IN/NOT IN completes the whole lazy build before a probe result, so any build
  error precedes a possible match; on first demand the left/probe expression
  error precedes initialization of a still-dormant build, and after READY each
  later left-expression error precedes that row's probe;
- branches skipped under Chapter 17 suppress the entire undemanded side plan;
- ordinary lower-layer fatal errors retain §39 precedence.

EXPLAIN exposes enough identity to distinguish scalar, EXISTS, IN-build, lazy
one-time state, and a surviving derived-table boundary, plus whether a proven
semi/anti/marker rewrite replaced the fallback. Exact output strings are not
part of the contract.

Subquery plans, cached values, sets, and spill files are query-local runtime
state. They add no WAL, page, catalog, TypeId, or long-lived persisted format;
spill state is disposable after error/crash.

### 20.14.13 Forbidden subquery implementations

The architecture forbids:

1. returning the first/last/arbitrary row of a multirow scalar subquery;
2. removing an outer row when a scalar subquery is empty;
3. reexecuting one uncorrelated expression-subquery occurrence for every outer row;
4. eagerly running an uncorrelated subquery in a path skipped or never demanded under Chapter 17;
5. implementing nullable NOT IN as an ordinary anti-join;
6. converting IN UNKNOWN to FALSE in projection/CASE;
7. obtaining a new READ COMMITTED snapshot or CommandId for a subquery;
8. capturing an outer column or accepting arbitrary correlation in the defined architecture;
9. depending on successful decorrelation for executable semantics;
10. using estimated zero rows, NULL fraction, or required_rows as an empty/NOT-NULL proof;
11. evaluating EXISTS projection-only values that §20.14.5 declares irrelevant,
    omitting DISTINCT-dependent values required there, or requesting later rows
    after existence is known;
12. treating global aggregate over empty input as an empty scalar/EXISTS subquery;
13. allowing subqueries in defaults, constraints, or data-modifying subqueries;
14. treating parser/AST capacity as permission to execute an unsupported form;
15. removing scalar cardinality checking without exact at-most-one proof;
16. letting vector batch size change scalar/EXISTS/IN error precedence;
17. leaking derived child aliases/unnamed generated columns into the outer namespace;
18. applying a rewrite that changes lazy demand, 3VL, snapshot, cardinality, or errors.

## 20.15 Canonical SELECT logical shape

Before optimization, SELECT planning uses the canonical shape:

```text
explicit FROM/JOIN tree
    or LogicalValues(one zero-column row) when FROM is omitted
    ↓
WHERE Filter
    ↓
Aggregate, if required
    ↓
HAVING Filter
    ↓
Projection
    ↓
Distinct
    ↓
Sort
    ↓
Limit/Offset
```

The optimizer may transform this only when it proves semantic equivalence.

Section 20.5 defines the no-FROM source and its exact row-occurrence semantics;
§18.11.1 owns only the syntax that permits omission of FROM. EXPLAIN exposes
that Values source and ordinary operators, or an optimizer-equivalent shape
with identical semantics; no dedicated no-FROM executor or user-visible
pseudo-relation exists.

## 20.16 Logical properties

Logical nodes expose or permit derivation of properties such as:

```text
output LogicalSlotIds
nullability
candidate keys
known constants
lineage
semantic ordering requirements
estimated-cardinality placeholder
semantic `is_provably_empty` state and exact proof provenance
```

Constraint-derived examples include:

```text
PRIMARY KEY           -> candidate key
UNIQUE + NOT NULL     -> candidate key
NOT NULL              -> non-nullable base output
LEFT JOIN right side  -> nullable output
```

Such properties may support optimization, but a rewrite based on them is not valid until its semantic proof exists.

Estimated cardinality and semantic emptiness are distinct properties. The canonical separation, approved proof sources, and statistics prohibition are defined by §35.2. Logical property derivation may preserve or compose a proof according to §20.17.10, but it MUST NOT derive one from a numerical estimate.

Logical properties do not substitute for the physical-property contracts in
§§22.7 and 37.2–37.4.

## 20.17 Logical rewrites

Logical rewrites occur after binding and before cost-based physical selection.

A scalar-expression evaluation is **demanded** when the unrevised logical
relational semantics require evaluating that expression for a particular
logical input row, candidate row pair, group, or other relational occurrence
to determine a required value, predicate result, grouping key, aggregate
argument, ordering key, or output. Demand is a semantic property, not an
optimizer schedule, physical visitation order, CPU evaluation count,
vector-lane detail, or page/tuple iteration rule.

Changing whether, where, or how an evaluation is demanded is
correctness-observable when it could change a produced value, SQL NULL/UNKNOWN
behavior, error versus success, error category, responsible `SourceSpan`,
Chapter-17 child order or short-circuit behavior, an already-frozen error
precedence, or another explicitly frozen semantic result. Relational result-row
order and physical resource usage remain separate contracts: allocation count,
CPU work, spill volume, addresses, and similar operational differences are not
logical demand effects. Scalar expressions have no ordinary user-visible
side effects beyond these frozen value, NULL, error, and demand semantics.

Every logical rewrite MUST satisfy one of these branches:

1. it preserves every semantically demanded evaluation that could have a
   correctness-observable effect, including its logical value context,
   Chapter-17 child order and short-circuit behavior, error category,
   responsible source-derived `SourceSpan`, and any frozen error precedence;
   or
2. an exact semantic proof establishes that adding, removing, duplicating, or
   moving the affected evaluation cannot alter any such result.

One sufficient proof for the second branch establishes over the complete newly
affected logical demand domain that the expression is total, error-free,
deterministic for the same bound values, and semantically insensitive to the
changed evaluation count/order. Total and error-free means every value
combination admitted by the proof completes normally with a scalar value or SQL
NULL and cannot raise a user-visible scalar/runtime semantic error. Exact
constants, frozen TypeResolver semantics, exact nullability, trusted enforced
constraints, and exact facts authorized by §§34.1 and 35.2 may participate.
Statistics, samples, selectivity/cardinality estimates, costs, heuristics, and
likely value ranges are never sufficient.

Absent such an exact proof, an expression is conservatively treated as
potentially erroring and correctness-observable, and its semantic demand MUST
be preserved. Existing specialized rules, including all §20.14 subquery demand
rules, remain authoritative within their domains. A rewrite does not create a
new physical row, RID, page, hash-bucket, or thread-order error precedence; it
preserves every precedence already defined by the owning semantic layer.
Consequently, a legal rewrite preserves relational values, NULL behavior,
multiplicity and result-order guarantees where defined, demanded scalar
evaluations, Chapter-17 evaluation ordering, and observable errors.

Rewrite output identity follows §20.2 independently of value equivalence. The
same surviving logical output occurrence preserves its `LogicalSlotId`; a new
or duplicated occurrence receives a fresh ID; an eliminated ID is never reused
in the same statement. Replacing or eliminating a semantic boundary must retain
an explicit mapping for every consumer-visible occurrence that survives.

Every executable logical scalar occurrence originating from a Chapter-19 bound
source expression retains its canonical diagnostic origin. This includes its
`SourceSpan`, explicit/implicit cast provenance, any more-specific error-origin
metadata already represented, and source-occurrence information required by a
frozen error-precedence rule. The representation of this origin is not
normative.

Diagnostic provenance transforms according to the following rules:

- moving an unchanged source-derived executable occurrence preserves its
  origin exactly and creates no new diagnostic origin;
- structural sharing by multiple consumers preserves that same semantic
  origin independently of pointer sharing;
- duplicating an occurrence copies its canonical origin to every duplicate,
  but §20.17 demand safety still determines whether duplication is legal;
- replacing a source-derived occurrence with an executable expression capable
  of a user-visible semantic error MUST preserve the replaced occurrence's
  observable error category, canonical diagnostic origin, and frozen
  precedence; otherwise the replacement is illegal;
- a synthesized executable expression with no canonical source diagnostic site
  is legal only when exact proof establishes that it is total and error-free
  over its complete demanded evaluation domain;
- an error-capable synthesized executable expression is legal only when it
  semantically implements exactly one identifiable source-derived occurrence
  and inherits that occurrence's canonical origin and precedence.

No rewrite invents a source-byte position, selects an arbitrary span from an
equivalent expression, or uses an optimizer-generated location as a
user-facing scalar diagnostic. Spanless synthesized expressions may include
exactly proven total/error-free constants, safe NULL markers, or direct slot
identities; the classification is proof-based, and does not presume that all
comparisons, casts, or arithmetic are safe.

Chapter 20 owns semantic demand, executable child order, and preservation of
source-derived diagnostic provenance. Section 25.1.1 owns deterministic selection
among multiple ordinary non-DML occurrence-level runtime expression-error
candidates after those Chapter-17/20 rules establish them. Ordinary DML
candidate selection remains the separate §21.16.1 contract.

Non-executable optimizer metadata—equivalence facts, lookup/search keys, proof
facts, estimates, memo metadata, or join-graph metadata—requires no diagnostic
`SourceSpan` because it is not executed and cannot itself raise a user-visible
scalar error. Such metadata MUST NOT become the origin of one. An
implementation may retain source origins in a set, list, graph, or another
lineage form, but a provenance collection is not itself a diagnostic-selection
rule: the canonical responsible source occurrence for any potentially erroring
executable expression must remain deterministically identifiable.

Section 17.10.2 remains the sole owner of folded value, binding-time error,
folded-expression provenance, and evaluation-equivalence semantics. The rules
above govern Chapter-20 rewrites outside that specialized contract. Numeric
`SourceSpan` values may outlive source backing under Chapter 18; retaining the
original SQL buffer is not required, and existing retain-or-materialize rules
govern any later rendered source text.

Each logical aggregate occurrence retains its own source diagnostic provenance
alongside its canonical ordinal. Provenance preservation never authorizes a
rewrite: §20.17 demand/error safety and §20.17.5 executable child-order safety
remain independent requirements.

### 20.17.1 Constant folding

Fold a subtree only when it is composed entirely of constants and uses entries
from the closed §17 scalar registry. Result, error, binary64 rounding, NULL/3VL,
and skipped-branch behavior are exactly §17.10.2. Under the frozen scalar
registry, VOLATILE functions are never folded and STABLE functions are not
immutable compile-time constants.

### 20.17.2 Boolean simplification

Safe identities include:

```text
x AND TRUE -> x
x OR FALSE -> x
NOT NOT x  -> x
```

Rules are expressed in SQL three-valued logic rather than imported from two-valued Boolean algebra.

A rewrite MUST NOT duplicate, drop, or reorder a VOLATILE expression.

The baseline optimizer is conservative around expressions whose evaluation may raise a SQL error; it does not use Boolean algebra merely to hide or newly force such evaluation.

### 20.17.3 Predicate pushdown

Predicates may be pushed toward base relations only when the referenced slots
and join semantics permit and the transformation is demand-safe under §20.17.

For INNER/CROSS joins, referencing only one side is necessary but not
sufficient for pushdown. If movement would demand the predicate for rows that
the unrevised plan would not test, the predicate may move only when an exact
proof establishes that the additional evaluations cannot change a value,
NULL/UNKNOWN behavior, error, or other correctness-observable result. A
potentially erroring left-local predicate above an INNER JOIN therefore cannot
move to the left input merely because it references only left slots when some
left rows would never produce a joined row.

LEFT JOIN uses its existing stricter null-preservation rules in addition to
this demand-safety requirement.

The architecture does not apply an inner-join pushdown rule across a LEFT JOIN unless an explicit outer-join transformation proves equivalence.

### 20.17.4 Projection pruning

Projection pruning computes values required by ancestors and removes unused outputs.

Ancestor slot nonuse alone does not authorize removal of an ordinary projected
expression whose evaluation is demanded under §20.7 and may be
correctness-observable. Such an expression may be removed only when an explicit
specialized semantic rule establishes that the value is not demanded, as for
EXISTS under §20.14.5, or an exact §20.17 proof establishes that suppressing
the evaluation cannot change any observable result. A projected expression
proven total and error-free over its complete demand domain may qualify when
all other semantics are unchanged.

It MUST retain every value still required by:

```text
filters
join predicates
group keys
aggregate arguments
sort keys
RETURNING
DML assignments
hidden target RID/system slots
```

### 20.17.5 Expression canonicalization

Executable source-derived scalar expressions preserve Chapter 17's semantic
child order. For a binary expression, its semantic left child remains left and
its semantic right child remains right. Chapter-20 canonicalization MUST NOT
swap commutative operands, reverse an executable comparison such as `a < b` to
`b > a`, sort executable children, or use associative/commutative algebra to
reassociate them. This includes executable arithmetic, comparison, AND, and OR
trees. Chapter-17 AND/OR short-circuiting and CASE branch order remain exact.

Value commutativity is not evaluation commutativity: even when successful
values satisfy `value(a OP b) == value(b OP a)`, the executable trees can
differ in child demand, error occurrence/category, responsible `SourceSpan`,
and short-circuit behavior. Mathematical equality alone therefore cannot
justify executable operand reordering.

Commutativity, associativity, or exact comparison reversal MAY normalize
non-executable equivalence facts, lookup/search keys, proof facts, or comparison
descriptors. Such metadata MUST NOT replace the executable expression, change
its child order or evaluation semantics, redefine an error or `SourceSpan`, or
become a user-visible expression tree. No particular metadata representation is
required.

Chapter-17-authorized constant folding under §17.10.2 remains permitted because
that owner defines its exact value, error, skipped-branch, and timing
equivalence. Existing source-child provenance remains attached to its original
semantic occurrence; synthesized and transformed provenance follows the
general §20.17 contract.

### 20.17.6 Join graph extraction

INNER/CROSS joins may be summarized as a join graph:

```text
relation occurrences (BindingIds) -> vertices
join predicates                    -> edges
local predicates                   -> vertex metadata
```

Outer joins impose semantic ordering constraints and are not freely inserted into an ordinary inner-join graph.

### 20.17.7 Inner-join equality equivalence classes

Within semantics where ordinary equality and join type make transitivity safe, derive equality equivalence classes.

For example:

```text
A.x = B.x
B.x = C.x
```

creates one class containing:

```text
A.x
B.x
C.x
```

Uses include:

```text
transitive join-predicate derivation
constant propagation
join-graph connectivity
interesting-order recognition
```

Equivalence derivation does not cross a LEFT JOIN null-extension boundary unless a separate proven rewrite first removes that outer-join constraint.

### 20.17.8 Constant propagation and contradiction detection

For an inner-join/filter-safe equivalence:

```text
A.x = B.x
AND A.x = 5
```

the optimizer may derive:

```text
B.x = 5
```

which may create a new base-access alternative.

Constraint analysis also detects provable contradictions such as:

```text
x = 1 AND x = 2
x < 5 AND x >= 5
NOT NULL column IS NULL
```

and marks the corresponding logical relation as provably empty before physical search.

Proof of empty output is not by itself permission to suppress another
correctness-observable evaluation demanded by the unrevised logical semantics.
Contradiction propagation or an empty replacement must also preserve that
demand, or carry an exact §20.17 proof that every changed evaluation is
insensitive.

These are semantic proofs only when they use exact typed constants, Chapter-17 type semantics, and trusted enforced catalog constraints. Statistics-derived min/max, NDV, MCV, histogram, HLL, NULL fraction, row count, or estimated join domains are not constraints and cannot participate in this contradiction proof.

Derived predicates retain ordinary SQL NULL semantics.

No predicate is propagated across nullable outer-join semantics without proof.

### 20.17.9 Trusted key metadata

Only constraints actually enforced by the engine may establish logical key properties.

Trusted PRIMARY KEY / UNIQUE + NOT NULL metadata may support:

```text
candidate-key inference
maximum join multiplicity
DISTINCT redundancy reasoning
GROUP BY key properties
```

Low estimated NDV is never treated as proof of uniqueness.

Foreign-key-based rewrites and join elimination are outside the trusted-proof
surface because their required enforced constraints and semantic proofs are not
part of that surface.

### 20.17.10 Semantic-emptiness propagation

The logical semantic-reasoning layer MUST derive `is_provably_empty` according to the exact-proof whitelist in §35.2 and these operator-specific rules:

```text
LogicalValues:
    provably empty exactly when it contains zero rows

LogicalGet:
    provably empty when its complete attached predicate is proven never TRUE
    for any base row; base-table statistics alone never prove the Get empty

LogicalFilter:
    provably empty when its child is already provably empty or the complete
    filter predicate is proven never TRUE for any child row

LogicalProject:
    preserves an empty-child proof

LogicalJoin INNER or CROSS:
    provably empty when either required child is provably empty;
    INNER is also provably empty when its complete ON condition is
    proven never TRUE for any candidate pair

LogicalJoin LEFT:
    provably empty exactly when its preserved left child is provably empty;
    an empty right child or an ON condition proven never TRUE establishes
    no matches, not empty output, because left rows are null-extended

LogicalAggregate with one or more grouping expressions:
    preserves an empty-child proof because an empty input forms no groups

LogicalAggregate with no grouping expressions:
    is not provably empty merely because its child is empty;
    global aggregation emits exactly one row under §29.5

LogicalDistinct / LogicalSort:
    preserve an empty-child proof

LogicalLimit:
    provably empty when its child is provably empty or its actual bound
    SQL LIMIT is exactly zero

LogicalInsert / LogicalUpdate / LogicalDelete:
    a provably empty relational input proves zero input/target rows and
    empty RETURNING output, but any no-op lowering must preserve statement
    completion, affected-row reporting, and transaction semantics

LogicalAnalyze:
    is never eliminated because a prior statistical row count is zero;
    it executes its required Chapter-34 collection protocol
```

For `WHERE`, `HAVING`, and INNER JOIN match selection, only TRUE admits a row or pair. Therefore an exact predicate that is constant FALSE or constant UNKNOWN, or otherwise proven unable to become TRUE, may establish the corresponding filter/inner-join proof. LEFT JOIN uses the same TRUE-only match rule, but FALSE/UNKNOWN there does not remove the preserved left row. CASE arms, projections, and other scalar-expression contexts do not become empty merely because an expression is or may be UNKNOWN. `IN`/`NOT IN` may contribute a proof only after exact SQL three-valued evaluation of the complete predicate in a row-rejecting context.

An empty-result helper or equivalent plan replacement may be introduced only for a node whose semantic proof is established by these rules. The replacement must preserve the node's output schema and all statement/operator semantics. A numerical zero estimate is never such authorization.

Exact emptiness alone does not authorize erasing an otherwise demanded
potentially erroring expression. The replacement is legal only when the
unrevised semantics demand no suppressed correctness-observable evaluation or
an exact §20.17 proof establishes that every demand difference is insensitive.

## 20.18 Logical-plan validation

Validation runs after canonical logical-plan construction and after major
rewrite phases.

At minimum it checks:

```text
every referenced LogicalSlotId exists in the required child output
every distinct logical output occurrence has a whole-statement-unique
LogicalSlotId, including nested/side plans, and no eliminated ID is reused
Get/Values/Project/Aggregate/derived-boundary fresh-slot rules and every
schema-preserving pass-through slot rule agree with §20.2
expression return types are resolved
filter/HAVING/join predicates are BOOLEAN
aggregate references are legal; every logical aggregate entry carries its
unchanged canonical ordinal and a distinct output LogicalSlotId
node output schemas agree with expressions
join output schemas are left-child outputs followed by right-child outputs,
with child slots preserved and LEFT JOIN right-side nullability applied
LogicalLimit preserves its child's schema and slots
hidden DML RID/system slots survive when required
catalog descriptor/schema-version references are internally consistent
every provably-empty annotation or empty-result replacement has approved
semantic provenance under §§20.17.10 and 35.2
every expression subquery is one supported §20.14 mode with exactly one
independent child and no OuterRef/correlated slot
scalar/IN child arity, subquery result TypeId/nullability, derived-table output
names/slot maps, and NOT wrappers agree with §20.14
every accepted rewrite is justifiable as demand-preserving or by an exact
demand-insensitivity proof under §20.17
every executable source-derived or synthesized expression has valid canonical
diagnostic provenance under §20.17, and no non-executable metadata is a
user-facing diagnostic origin
every executable scalar tree preserves Chapter-17 semantic child order under
§20.17.5
```

Malformed logical plans are architecture errors detected before execution, not conditions left for executor crashes.

## 20.19 Logical EXPLAIN

Logical plans have an AST-independent debug representation.

Logical EXPLAIN describes the validated logical representation. An illustrative
logical tree is:

```text
Limit 10
  Sort [orders.amount DESC NULLS LAST]
    Project [users.name, orders.amount]
      Filter [orders.amount > 100]
        InnerJoin [users.id = orders.user_id]
          Get users
          Get orders
```

Debug detail may include `LogicalSlotId`, `TableId`, `BindingId`, types, nullability, lineage, and a provably-empty proof kind distinct from estimated rows.

The logical EXPLAIN representation does not depend on reparsing or pretty-printing the original AST.

Sections 40.2, 40.5, and 40.8 own EXPLAIN presentation and the relationship
between logical, selected physical, and execution-analysis information;
Chapter 20 defines only the logical representation being described.

## 20.20 Logical-plan/rewrite invariants

1. Logical operators encode relational/statement semantics, not physical algorithms.
2. `LogicalSlotId` identifies exactly one statement-wide logical output occurrence, is never reused during that statement lifetime, and is distinct from catalog/binding/ordinal/pointer identities and persistent formats.
3. Logical nodes and bound expressions are immutable once published.
4. Logical relations are bags of row occurrences; equal values do not merge implicitly, and multiplicity does not establish row order.
5. INNER, LEFT, and CROSS joins preserve exact occurrence-pair multiplicity; join schema is left outputs then right outputs, LEFT null-extends exactly one row on zero TRUE matches, and no join establishes row order.
6. LEFT JOIN null extension is represented as logical right-output nullability without changing catalog nullability, type, lineage, or child slot identity.
7. ON and WHERE remain semantically distinct.
8. Aggregate outputs contain only grouping/aggregate-derived values; every source aggregate occurrence retains its upstream ordinal, distinct output slot, and diagnostic provenance through logical rewrites.
9. GROUP BY and DISTINCT use SQL grouping equivalence, including one NULL group.
10. Sort keys always carry resolved ASC/DESC and NULL order.
11. `LogicalLimit` applies validated OFFSET before validated LIMIT, selects occurrences without deduplication, preserves schema/slots and ordered-child order, and introduces no physical order for an unordered child.
12. Hidden DML RID slots are not user-visible and are preserved while required.
13. SELECT planning begins from one deterministic canonical logical shape.
14. Rewrites preserve NULL, FLOAT64, volatility, and outer-join semantics.
15. Attempted correlated references are detected distinctly and rejected; no executable logical plan contains OuterRef/Apply state.
16. Logical validation occurs before execution/physical planning consumes a plan.
17. `LogicalAnalyze` carries a resolved table/schema/index set and does not perform name lookup during execution.
18. EXPLAIN consumes the bound/logical representation rather than AST syntax alone.
19. Semantic emptiness is derived only from §35.2 exact facts and propagates by §20.17.10; estimated zero is not a rewrite proof.
20. Literal/operator/cast/predicate/scalar-function binding uses only the closed §§17.2–17.10 registry.
21. Every accepted subquery is one closed §20.14 uncorrelated form with its required logical child, cardinality/3VL contract, and physical fallback.
22. Subquery rewrites preserve lazy demand, error precedence, snapshot/CommandId, and exact proof provenance defined by §§34.1 and 35.2.
23. Every logical rewrite preserves correctness-observable demanded evaluations and existing error category, source-derived SourceSpan, and frozen precedence unless exact semantic proof establishes the demand difference is insensitive.
24. Executable scalar expressions preserve Chapter-17 child order; commutative, associative, or comparison-reversal normalization is confined to non-executable metadata and never replaces executable order.
25. Source-derived executable provenance survives movement, sharing, duplication, and error-capable replacement under §20.17; spanless synthetic execution is confined to exactly proven total/error-free expressions.
