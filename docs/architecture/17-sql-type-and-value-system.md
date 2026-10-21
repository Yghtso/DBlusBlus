# 17. SQL Type and Value System

## 17.1 Scope

The SQL type subsystem defines logical scalar types, nullability/value-state semantics, type promotion, casts, comparison compatibility, BOOLEAN three-valued logic, planning-time scalar values, and centralized operator/function type resolution.

Storage byte layout remains owned by Chapter 5.

Index-key byte ordering remains owned by Chapter 8.

The SQL type layer MUST use semantics compatible with both.

## 17.2 Logical scalar types

The complete stored scalar set is exactly the seven TypeIds in §16.4:

| TypeId | Type | Canonical non-NULL logical domain | SQL equality/order | Hashable | Direct literal |
|---:|---|---|---|---|---|
| `1` | `BOOLEAN` | `FALSE` or `TRUE` | `=`/`<>`; SQL ordering operators are unsupported | yes | keywords `TRUE`, `FALSE` |
| `2` | `INT32` | signed integers `[-2^31, 2^31-1]` | equality and signed numerical order | yes | unsuffixed integer token when classified below |
| `3` | `INT64` | signed integers `[-2^63, 2^63-1]` | equality and signed numerical order | yes | unsuffixed integer token when classified below |
| `4` | `FLOAT64` | every IEEE-754 binary64 value, including signed zero, infinities, and NaNs | canonical equality/total order in §17.4.3 | yes after canonical normalization | decimal/exponent floating token; no NaN/infinity literal keyword |
| `5` | `DATE` | every signed int32 civil-day count from `1970-01-01` in the proleptic Gregorian calendar | equality and signed day-count order | yes | none; construct by explicit cast |
| `6` | `TIMESTAMP` | every signed int64 microsecond count from `1970-01-01 00:00:00` | equality and signed microsecond order | yes | none; construct by explicit cast |
| `7` | `VARCHAR` | arbitrary finite byte string representable by the owning row/execution resource limits | exact-byte equality and unsigned lexicographic byte order | yes | single-quoted byte string |

No scalar TypeId exists for NULL, SQL three-valued-logic UNKNOWN, the
binder's unresolved type marker, DECIMAL, BLOB, UUID, INTERVAL, JSON, arrays,
or timezone-aware timestamps. Anything outside this table is not a supported scalar
type.

Every listed type may carry SQL NULL as a separate value/validity state. The
ordinary tuple codec owns stored column bytes, §17.13 owns persisted metadata
scalars, and Chapter 8 owns key bytes; none changes the logical registry.

A compact value-like representation is used conceptually:

```text
LogicalType {
    TypeKind kind;
}
```

Type descriptors do not require an inheritance hierarchy or one heap allocation per type.

Extending this representation with type parameters requires an architecture
revision; type parameters are not open registry entries.

## 17.3 Storable types versus semantic pseudo-types

Stored table columns use only:

```text
BOOLEAN
INT32
INT64
FLOAT64
DATE
TIMESTAMP
VARCHAR
```

`NULL` is not a legal stored schema type. The binder's unresolved type marker
is not a SQL scalar type, TypeId, persisted schema type, or executor vector
element type.

SQL NULL is primarily a value state attached to a concrete logical type.

An untyped NULL token may temporarily carry an unresolved type marker during
binding. Successful binding resolves the marker to a concrete logical type
before ordinary execution; if no legal context supplies a unique concrete type,
binding produces `TYPE_ERROR`.

The distinct terminology reflects distinct semantic layers: an unresolved type
marker is a bind-time state, whereas SQL three-valued-logic UNKNOWN is the
nullable BOOLEAN NULL result defined in §17.7.2.

`NULL` may be used by generic non-hot semantic values to denote an explicitly null-only value before contextual coercion, but it does not become a persisted column type or executor vector element type.

Where context supplies a concrete target type, an unresolved NULL is coerced to:

```text
target LogicalType
+
is_null = true
```

The SQL token `NULL` initially binds as an untyped NULL candidate. Contextual
typing is exact:

| Context | Result |
|---|---|
| `CAST(NULL AS T)` | typed NULL of concrete stored type `T` |
| comparison/arithmetic with one concrete compatible operand | typed to the selected registered overload/common numeric type |
| operator whose closed registry supplies exactly one signature for the untyped positions | typed from that signature; for example `NOT NULL` and `NULL AND NULL` use BOOLEAN |
| INSERT/UPDATE/default target | typed to the destination type, then ordinary NULLability enforcement applies |
| searched CASE, IN list, or one VALUES column with at least one concrete compatible value | typed to the exact common type |
| `NULL IS NULL` / `NULL IS NOT NULL` | folded directly to non-NULL BOOLEAN TRUE/FALSE without persisting a pseudo-type |
| standalone `SELECT NULL`, standalone `VALUES(NULL)`, `NULL = NULL`, `NULL + NULL`, all-NULL CASE results, or another context with no unique concrete type | bind-time `TYPE_ERROR` |

No fallback VARCHAR/INT32 type is guessed for an underconstrained NULL.

## 17.4 Closed scalar semantic registry

This section and §§17.5–17.10 are the one finite scalar registry. A parser
may construct syntax outside it, but binding MUST reject every type, literal,
operator overload, comparison, cast, predicate, or scalar function not listed
here. Empty table cells mean unsupported at bind time.

### 17.4.1 BOOLEAN

BOOLEAN has SQL TRUE/FALSE values plus nullable NULL state.

No integer truthiness is implied.

BOOLEAN has no implicit or explicit numeric conversion. Its only ordered
physical key representation (`FALSE < TRUE`) exists so indexes can store and
scan BOOLEAN; SQL `<`, `<=`, `>`, and `>=` overloads for BOOLEAN do not exist.

### 17.4.2 INT32 and INT64

Signed integer semantics correspond to the persisted two's-complement widths from Chapter 5.

SQL arithmetic is checked mathematical arithmetic in those domains. Host signed
overflow, overflowing negation, narrowing truncation, and host `INT_MIN / -1`
behavior are never semantic implementations.

### 17.4.3 FLOAT64

FLOAT64 uses IEEE-754 binary64.

SQL comparison/equality semantics MUST agree with the total-order/index semantics from Chapter 8:

```text
-infinity
...
finite values
...
+infinity
NaN
```

For comparison/equality purposes in the defined architecture:

- `-0.0` and `+0.0` compare equal,
- all NaNs are canonical-equivalent,
- NaN sorts after `+infinity`.

Arithmetic uses IEEE-754 binary64 behavior; the execution-level checked/integer-versus-FLOAT64 division boundary is canonical in §39.3.2.

Every scalar arithmetic operation is evaluated in binary64 round-to-nearest,
ties-to-even mode and rounded to binary64 at that operator boundary. Extended-
precision intermediates, altered process rounding modes, or contraction/FMA that
changes the result of the bound expression tree are forbidden. Arithmetic NaN
results are normalized to the canonical quiet NaN semantic value; signed zero
is preserved. Overflow produces signed infinity and underflow produces the
correctly rounded subnormal or signed zero. No floating exception trap is part
of SQL semantics.

These scalar operator boundaries do not define aggregate reduction order.
`SUM(FLOAT64)` and `AVG(FLOAT64)` are the separate n-ary operations in §29.3:
every demanded finite occurrence contributes exactly once to an admitted
binary64 reduction tree. Each tree edge uses this section's binary64 addition,
so worker, chunk, Combine, and spill shape may change finite low result bits
within §29.3's bounded aggregate contract.

Execution, constant folding, hashing/grouping support, and B+ ordering MUST NOT silently use incompatible equality/order rules.

### 17.4.4 DATE

DATE is a signed 32-bit count of civil days from:

```text
1970-01-01
```

using the proleptic Gregorian calendar.

The persisted scalar is the signed day count.

Textual DATE conversion accepts only civil years `0001..9999`; the wider
signed day-count domain remains comparable/storable, but converting an out-of-
text-range DATE to VARCHAR is `INVALID_CAST`.

### 17.4.5 TIMESTAMP

TIMESTAMP is a signed 64-bit count of microseconds from:

```text
1970-01-01 00:00:00
```

TIMESTAMP is timezone-naive.

Timezone-aware types/conversion rules are deferred.

Leap-second modeling is not part of the scalar contract.

Textual TIMESTAMP conversion likewise accepts civil years `0001..9999`.
The wider signed microsecond domain remains comparable/storable, but conversion
to the canonical text form fails outside that civil range.

### 17.4.6 VARCHAR

VARCHAR values are byte strings.

The architecture uses:

```text
binary bytewise comparison/collation
```

matching Chapter 8 index ordering.

The type layer does not require UTF-8 validation, locale collation, character-count semantics, or `VARCHAR(n)` length semantics.

String length in the baseline representation is a byte length.

## 17.5 Literal and textual scalar grammar

### 17.5.1 Integer literals

An unsuffixed integer token is ASCII decimal `DIGIT+`; underscores, radix
prefixes, and type suffixes are unsupported. Leading zeros are allowed and do
not imply octal. The lexer retains a checked mathematical magnitude through
`2^63` for the binder:

| Positive token magnitude | Bound type/outcome |
|---|---|
| `0..2^31-1` | `INT32` |
| `2^31..2^63-1` | `INT64` |
| exactly `2^63` | legal only as the direct operand of syntactic unary `-`, producing the constant `INT64_MIN` |
| greater than `2^63` | `INVALID_LITERAL` |

For a syntactically direct unary `-` plus integer token, the binder classifies
the resulting negative mathematical constant into the smallest signed type:
magnitudes through `2^31` produce INT32 (including `-2147483648`), and larger
magnitudes through `2^63` produce INT64 (including
`-9223372036854775808`). This is literal construction, not evaluation of
overflowing unary negation. Parentheses break the direct form:
`-(2147483648)` is a valid INT64 negation, while positive or parenthesized
`9223372036854775808` is invalid before unary evaluation. Direct unary `+` has
no special range. No host parser overflow is consulted.

### 17.5.2 FLOAT64 literals

A floating token is exactly one of these ASCII forms, with `DIGIT=[0-9]`:

```text
DIGIT+ '.' DIGIT+ ([eE] [+-]? DIGIT+)?
DIGIT+ [eE] [+-]? DIGIT+
```

Thus `1.0`, `1e10`, and `1.2e-3` are legal; `.5`, `1.`, hexadecimal floats,
underscores, suffixes, and NaN/infinity keywords are not. Leading sign is unary
`+`/`-`. Decimal conversion is locale-independent, consumes the complete token,
and returns the correctly rounded binary64 round-to-nearest-ties-to-even value.
Finite decimal overflow is `INVALID_LITERAL`; underflow is the correctly rounded
subnormal or signed zero and is not an error.

### 17.5.3 VARCHAR and BOOLEAN literals

Single quotes delimit VARCHAR. A quote byte is doubled (`'It''s'`); backslash has
no escape meaning. SQL input is length-delimited: an embedded `0x00` inside the
quotes is retained as an ordinary byte, while an unquoted source NUL is a lexical
error. No Unicode decoding, validation, normalization, or locale conversion is
performed.

The only BOOLEAN literals are case-insensitive SQL keywords `TRUE` and `FALSE`.
Numeric and string literals never acquire BOOLEAN type implicitly. `NULL` follows
§17.3 and is not a BOOLEAN literal.

### 17.5.4 DATE and TIMESTAMP text construction

The architecture has no `DATE '...'` or `TIMESTAMP '...'` typed-literal syntax. Construction
uses explicit VARCHAR casts with full-string, ASCII, locale-independent parsing:

```text
DATE:      YYYY-MM-DD
TIMESTAMP: YYYY-MM-DD HH:MM:SS[.f{1,6}]
```

Every fixed field is zero-padded; year is exactly `0001..9999`. TIMESTAMP uses
one ASCII space, not `T`; seconds are mandatory. Missing fractional digits mean
zero and 1–6 digits are right-padded to microseconds. More than six digits are
rejected rather than rounded. Month/day validity uses the proleptic Gregorian
calendar and the divisible-by-4, except divisible-by-100 unless divisible-by-400,
leap-year rule. Time is `00:00:00` through `23:59:59`; second `60`, timezone
suffixes/offsets, whitespace, and trailing bytes are invalid.

## 17.6 Operator overload registry

Unless a rule below says otherwise, scalar operators evaluate child expressions
left-to-right and are **strict**: after child evaluation, any NULL operand yields
a typed NULL result without invoking the non-NULL value operation. Strictness
does not suppress an error raised while evaluating a child. The result is
nullable exactly when a strict input may be NULL.

### 17.6.1 Unary operators

| Operator | Input | Result | NULL/error behavior |
|---|---|---|---|
| unary `+` | INT32 | INT32 | strict identity |
| unary `+` | INT64 | INT64 | strict identity |
| unary `+` | FLOAT64 | FLOAT64 | strict identity preserving signed zero |
| unary `-` | INT32 | INT32 | strict; `INT32_MIN` raises `NUMERIC_OVERFLOW`, except §17.5.1's direct literal construction |
| unary `-` | INT64 | INT64 | strict; `INT64_MIN` raises `NUMERIC_OVERFLOW`, except §17.5.1's direct literal construction |
| unary `-` | FLOAT64 | FLOAT64 | strict IEEE sign negation; NaN normalizes canonically |
| `NOT` | BOOLEAN | BOOLEAN | exact §17.7 3VL |

No unary overload exists for VARCHAR, DATE, or TIMESTAMP.

### 17.6.2 Binary arithmetic

The complete arithmetic table is:

| Left | Right | `+` | `-` | `*` | `/` | `%` |
|---|---|---|---|---|---|---|
| INT32 | INT32 | INT32 | INT32 | INT32 | INT32 | INT32 |
| INT32 | INT64 | INT64 | INT64 | INT64 | INT64 | INT64 |
| INT64 | INT32 | INT64 | INT64 | INT64 | INT64 | INT64 |
| INT64 | INT64 | INT64 | INT64 | INT64 | INT64 | INT64 |
| INT32 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | unsupported |
| FLOAT64 | INT32 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | unsupported |
| INT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | unsupported |
| FLOAT64 | INT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | unsupported |
| FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | FLOAT64 | unsupported |

Mixed rows are resolved by inserting the minimum numeric promotions necessary
for the displayed common result type. This is registered operator resolution,
not permission to coerce strings, booleans, or temporal values. VARCHAR
concatenation and DATE/TIMESTAMP arithmetic are unsupported in the defined architecture.

INT32/INT64 `+`, `-`, and `*` use checked arithmetic and raise
`NUMERIC_OVERFLOW` rather than wrap. Integer `/` truncates the mathematical
quotient toward zero. Division by zero raises `DIVISION_BY_ZERO`; `MIN / -1`
raises `NUMERIC_OVERFLOW`. Integer `%` is defined by:

```text
a = trunc_toward_zero(a / b) * b + (a % b)
abs(a % b) < abs(b)
nonzero remainder has the sign of a
```

Zero divisor raises `DIVISION_BY_ZERO`; `MIN % -1` raises `NUMERIC_OVERFLOW`
because the corresponding quotient is unrepresentable. Normative examples are:

| Expression | Result |
|---|---:|
| `5 / 2` | `2` |
| `-5 / 2` | `-2` |
| `5 / -2` | `-2` |
| `-5 / -2` | `2` |
| `5 % 2` | `1` |
| `-5 % 2` | `-1` |
| `5 % -2` | `1` |
| `-5 % -2` | `-1` |

FLOAT64 `+`, `-`, `*`, and `/` use §17.4.3 arithmetic. In particular:

```text
finite nonzero / ±0.0 -> signed infinity
±0.0 / ±0.0          -> canonical NaN
infinity / infinity   -> canonical NaN
```

FLOAT64 division by zero is not a SQL error. FLOAT64 remainder is unsupported.

## 17.7 Comparisons, predicates, and three-valued logic

### 17.7.1 Comparison overloads

Every ordinary comparison is strict and returns BOOLEAN, nullable exactly when
either bound operand may be NULL. Its complete non-NULL overload registry is:

| Operand pair | `=` `<>` | `<` `<=` `>` `>=` | Coercion/semantics |
|---|---|---|---|
| BOOLEAN, BOOLEAN | yes | unsupported | exact FALSE/TRUE equality |
| any INT32/INT64/FLOAT64 pair | yes | yes | promote both to the smallest common numeric type, then compare |
| VARCHAR, VARCHAR | yes | yes | unsigned lexicographic exact-byte comparison; prefix sorts before longer equal-prefix value |
| DATE, DATE | yes | yes | signed civil-day count |
| TIMESTAMP, TIMESTAMP | yes | yes | signed microsecond count |
| every other pair | unsupported | unsupported | bind-time TYPE_ERROR unless user writes an explicit cast |

There is no implicit VARCHAR/numeric/temporal comparison, DATE/TIMESTAMP cross-
comparison, or BOOLEAN ordering. For example, `1 = '1'` is a bind-time error.

For FLOAT64 SQL comparison, the §17.4.3 total semantics are authoritative rather
than IEEE unordered predicates:

```text
-0.0 = +0.0                     -> TRUE
every NaN = every NaN            -> TRUE
NaN <> NaN                       -> FALSE
finite/infinite values order numerically
+infinity < NaN                  -> TRUE
NaN > every non-NaN              -> TRUE
```

This is intentionally the same normalized non-NULL equality/order used by B+
keys, UNIQUE, grouping, DISTINCT, sorting, and hash modes. IEEE arithmetic and
SQL comparison are separate semantic layers.

### 17.7.2 NULL predicates and 3VL

For every ordinary `=`, `<>`, `<`, `<=`, `>`, or `>=`, either operand NULL
produces UNKNOWN (nullable BOOLEAN NULL). `x = NULL` is never rewritten to
`IS NULL`. `expr IS NULL` and `expr IS NOT NULL` accept any valid scalar
expression and always return non-NULL BOOLEAN. `IS TRUE`, `IS FALSE`, and
`IS UNKNOWN` are unsupported in the defined architecture.

IS NULL/IS NOT NULL still evaluates their child expression and does not suppress
an error raised by that child.

WHERE, HAVING, and JOIN ON require BOOLEAN; integer/string truthiness is rejected.
The only logical binary overloads are
`AND(BOOLEAN,BOOLEAN)->BOOLEAN` and `OR(BOOLEAN,BOOLEAN)->BOOLEAN`; XOR is
unsupported. Result nullability follows the tables. UNKNOWN is nullable BOOLEAN
NULL. The full tables are:

| `A AND B` | TRUE | FALSE | UNKNOWN |
|---|---|---|---|
| TRUE | TRUE | FALSE | UNKNOWN |
| FALSE | FALSE | FALSE | FALSE |
| UNKNOWN | UNKNOWN | FALSE | UNKNOWN |

| `A OR B` | TRUE | FALSE | UNKNOWN |
|---|---|---|---|
| TRUE | TRUE | TRUE | TRUE |
| FALSE | TRUE | FALSE | UNKNOWN |
| UNKNOWN | TRUE | UNKNOWN | UNKNOWN |

| A | `NOT A` |
|---|---|
| TRUE | FALSE |
| FALSE | TRUE |
| UNKNOWN | UNKNOWN |

### 17.7.3 Evaluation order and short-circuit

The architecture guarantees per-row left-to-right short-circuit:

```text
FALSE AND rhs -> rhs is not evaluated
TRUE  OR rhs  -> rhs is not evaluated
otherwise AND/OR evaluates rhs and applies the tables above
```

Searched CASE evaluates WHEN predicates in source order, stops at the first
TRUE, and evaluates only that result expression; it evaluates ELSE only when no
WHEN is TRUE. IN-list evaluates the left expression once and list expressions
left-to-right, stopping at the first TRUE comparison; without TRUE it must
continue to determine whether UNKNOWN occurred. Optimizer rewrites may not
reorder/force a skipped error or volatile expression. Therefore
`FALSE AND (1 / 0 = 0)` is FALSE without error, and an unselected CASE arm does
not raise its errors.

## 17.8 Cast and coercion registry

Explicit cast syntax is exactly `CAST(expr AS type)`. The matrix is closed:

```text
=  identity cast (implicit/no-op and explicit)
I  registered widening cast; explicit CAST also legal
E  explicit CAST only
-- unsupported
```

| Source \ Target | BOOLEAN | INT32 | INT64 | FLOAT64 | VARCHAR | DATE | TIMESTAMP |
|---|---:|---:|---:|---:|---:|---:|---:|
| BOOLEAN | `=` | -- | -- | -- | E | -- | -- |
| INT32 | -- | `=` | I | I | E | -- | -- |
| INT64 | -- | E | `=` | I | E | -- | -- |
| FLOAT64 | -- | E | E | `=` | E | -- | -- |
| VARCHAR | E | E | E | E | `=` | E | E |
| DATE | -- | -- | -- | -- | E | `=` | E |
| TIMESTAMP | -- | -- | -- | -- | E | E | `=` |

NULL input to any legal cast returns typed NULL without parsing/range error.

### 17.8.1 Numeric casts

- `INT32 -> INT64` is exact.
- `INT32/INT64 -> FLOAT64` is IEEE round-to-nearest-ties-to-even and remains
  legal when the integer is not exactly representable.
- `INT64 -> INT32` checks the target range; failure is `NUMERIC_OVERFLOW`.
- `FLOAT64 -> INT32/INT64` rejects NaN and infinities, truncates a finite value
  toward zero, then checks the target range. Signed zero becomes integer zero.
  Failure is `INVALID_CAST` for NaN/infinity and `NUMERIC_OVERFLOW` for range.

No cast uses modulo truncation or a host unchecked narrowing conversion.

### 17.8.2 VARCHAR parsing

Every VARCHAR-to-scalar cast consumes the entire byte string, permits no leading
or trailing whitespace, and recognizes ASCII only:

| Target | Exact accepted text |
|---|---|
| BOOLEAN | case-insensitive ASCII `TRUE` or `FALSE` only |
| INT32/INT64 | `[+-]?[0-9]+`; leading zeros allowed; range checked |
| FLOAT64 | `[+-]?[0-9]+`, `[+-]?[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?`, or `[+-]?[0-9]+[eE][+-]?[0-9]+`; additionally exact `NaN`, `Infinity`, `+Infinity`, `-Infinity` |
| DATE | exact §17.5.4 DATE grammar |
| TIMESTAMP | exact §17.5.4 TIMESTAMP grammar |

Empty strings, Unicode digits/signs, embedded NUL, trailing junk, and malformed
numeric grammar fail with `INVALID_CAST`. Syntactically valid integer/finite-
FLOAT text outside the target numeric range raises `NUMERIC_OVERFLOW`; FLOAT
underflow rounds to subnormal/signed zero, and a leading `-` is preserved when
the rounded result is zero. Invalid temporal grammar/calendar
values raise `INVALID_DATE`/`INVALID_TIMESTAMP`. Special NaN text produces the
canonical NaN semantic value.

### 17.8.3 Canonical VARCHAR formatting

Explicit scalar-to-VARCHAR casts emit:

| Source | Canonical output |
|---|---|
| BOOLEAN | `TRUE` or `FALSE` |
| INT32/INT64 | shortest base-10 integer, no leading `+`/zeros; zero is `0` |
| DATE | `YYYY-MM-DD`; out-of-`0001..9999` civil range is `INVALID_CAST` |
| TIMESTAMP | `YYYY-MM-DD HH:MM:SS.ffffff`, always six fractional digits; out-of-`0001..9999` civil range is `INVALID_CAST` |
| FLOAT64 NaN/infinities | `NaN`, `Infinity`, or `-Infinity` |
| FLOAT64 signed zero | `0` or `-0` preserving sign |
| other finite FLOAT64 | canonical shortest round-trippable decimal defined below |

For finite nonzero FLOAT64, choose the minimum number of significant decimal
digits whose exact §17.8.2 parse returns the same binary64 bits; among candidates
choose the closest exact decimal to the binary value, ties by an even final
significand digit. Render fixed or scientific notation whichever uses fewer
bytes (fixed on a length tie), with no insignificant leading/trailing zeros.
Fixed form uses one `0` before the decimal point when magnitude is below one.
Scientific form has one digit before an optional decimal point, lowercase `e`,
no `+` on a positive exponent, and no exponent leading zeros. This defines
observable output independently of libc/iostream formatting.

### 17.8.4 DATE/TIMESTAMP casts

`DATE -> TIMESTAMP` sets time to `00:00:00.000000` and checked-converts the day
count to microseconds; an unrepresentable int64 timestamp is
`NUMERIC_OVERFLOW`. `TIMESTAMP -> DATE` selects the containing proleptic-
Gregorian civil date (floor toward the prior midnight for pre-epoch values) and
returns its checked int32 day count. No timezone conversion occurs.

### 17.8.5 Implicit and assignment coercion

Implicit coercion is contextual, not a general request to find any explicit cast:

| Bound source | Automatic assignment/default targets |
|---|---|
| BOOLEAN | BOOLEAN only |
| INT32 | INT32, INT64, FLOAT64 |
| INT64 | INT64, FLOAT64 |
| FLOAT64 | FLOAT64 only |
| VARCHAR | VARCHAR only |
| DATE | DATE only |
| TIMESTAMP | TIMESTAMP only |
| untyped NULL | any concrete destination type; later NULLability/constraint check may reject |

| Context | Allowed automatic conversion |
|---|---|
| arithmetic/comparison operator | exact types plus minimum INT32→INT64→FLOAT64 promotions shown in the overload tables |
| searched CASE / IN-list / VALUES-column common type | same type, or smallest common numeric promoted type; untyped NULL follows the selected concrete type |
| INSERT/UPDATE assignment and folded default | identity, INT32→INT64, INT32→FLOAT64, INT64→FLOAT64, or contextual typed NULL |
| scalar function arguments | none because the scalar-function registry is empty |
| aggregate arguments | only the exact Chapter-29 aggregate signatures; no invented cast |
| arbitrary unlike expression | no implicit conversion |

Assignment narrowing, VARCHAR parsing/stringification, temporal conversion, and
BOOLEAN conversion therefore require explicit CAST. Destination NOT NULL and
other constraints are checked after coercion. DDL folds a default, applies this
same assignment coercion, and stores exactly one final destination-typed
PersistedScalarV1; runtime default application performs no cast.

## 17.9 CASE, IN, and scalar functions

### 17.9.1 Searched CASE

Only searched CASE is supported:

```sql
CASE WHEN boolean_expr THEN result ... [ELSE result] END
```

Each WHEN must be BOOLEAN (an untyped NULL condition becomes typed BOOLEAN
UNKNOWN). Result common type is exact same type or the smallest common numeric
type; untyped NULL branches adopt that concrete type. No other cross-type common
type exists. If every result is untyped NULL, binding fails. Missing ELSE is a
typed NULL of the resolved result type. Simple `CASE value WHEN ...` is
unsupported. Evaluation follows §17.7.3.

### 17.9.2 Scalar predicates

The architecture supports expression-list `IN` and `NOT IN`, `IS NULL`, and `IS NOT NULL`.
An IN list must be nonempty and obtain one comparison type using §17.8.5; the
left value is compared under ordinary `=` semantics:

```text
any TRUE comparison                         -> TRUE
no TRUE and left/list comparison is UNKNOWN -> UNKNOWN
otherwise                                   -> FALSE
NOT IN                                      -> 3VL NOT of IN
```

Subquery IN/EXISTS/scalar forms use the separate closed §20.14 registry; they do
not add scalar comparison or coercion rules. `BETWEEN`, `LIKE`, `COALESCE`,
`NULLIF`, `IS TRUE/FALSE/UNKNOWN`, and every other scalar predicate/convenience
expression are unsupported in the defined architecture.

### 17.9.3 Closed scalar-function registry

The **scalar-function registry is empty**. There is no ABS, LOWER, LENGTH,
NOW, CURRENT_TIMESTAMP, RANDOM, or other named scalar function, deterministic or
nondeterministic. A parsed `AstFunctionCall` that is not one of the separate
aggregate calls in §29.3 fails binding with unsupported-function TYPE_ERROR.
BoundFunction descriptors and IMMUTABLE/STABLE/VOLATILE classes are reserved
IR machinery, not open semantic entries. Defaults may use immutable
registered operators/casts, but no named scalar function.

| Name | Arity/signature | Result | NULL/error behavior | Volatility/folding |
|---|---|---|---|---|
| *(none)* | no scalar-function signature exists | — | bind-time TYPE_ERROR for every scalar-function call | — |

## 17.10 Type resolution, folding, and cross-engine equality

### 17.10.1 Central TypeResolver and error taxonomy

One `TypeResolver` owns literal classification, common numeric type, every table
in §§17.6–17.9, assignment coercion, and CASE/IN resolution. Parser, binder,
optimizer, defaults, expression VM, and executor may not add overloads.

| Conceptual error | Required use |
|---|---|
| `TYPE_ERROR` / unsupported overload | no registry entry, underconstrained NULL, unsupported function/predicate/type |
| `INVALID_LITERAL` | malformed/out-of-domain source literal |
| `INVALID_CAST` | supported cast pair but source value has no target value, including NaN/infinity to integer |
| `NUMERIC_OVERFLOW` | checked integer arithmetic/narrowing, finite parse/range conversion, temporal-unit conversion overflow |
| `DIVISION_BY_ZERO` | integer `/` or `%` zero divisor only |
| `INVALID_DATE` | malformed/out-of-calendar-range DATE text |
| `INVALID_TIMESTAMP` | malformed/out-of-calendar/time-range TIMESTAMP text |

Absence of an overload, or a statically unsupported source/target cast pair, is
a bind-time TYPE_ERROR. Literal and fully constant failures are detected during
binding/folding when §17.10.2 permits; row-dependent parse, overflow, and
division failures are runtime expression errors. `NUMERIC_OVERFLOW` and
`DIVISION_BY_ZERO` are conceptual SQL-expression subcategories of the existing
runtime `ArithmeticError`; this registry does not require new source enums.
§39.1 alone determines whether such a runtime error leaves the statement
transaction ACTIVE or requires abort after published effects.

### 17.10.2 Constant folding and errors

Every scalar operator/cast is deterministic and IMMUTABLE. For a constant
expression whose evaluated path is required, folding result **or error** must be
identical to runtime evaluation: same concrete type, NULL, integer checks,
binary64 rounding after every operator, canonical NaN, signed zero, parsing,
formatting, comparison, and 3VL. Folding cannot use host arithmetic/parsers with
different semantics.

Binder/planner folds every fully constant scalar subtree to its value when
doing so preserves §17.7.3. For a constant **error**, it reports the error at
bind/plan time exactly when that subtree dominates every scalar-control-flow
completion path of its containing expression after earlier constant conditions;
the binder does not consult estimated or actual input row count for this timing
rule. Otherwise it retains the subtree and the error occurs only if runtime
control flow reaches it. An AND/OR RHS, CASE condition/result, or later IN item
that an earlier value may skip therefore cannot raise eagerly merely because it
is constant. Root `1/0` raises during binding/folding, while
`FALSE AND (1/0=0)` and `CASE WHEN FALSE THEN 1/0 ELSE 1 END` do not. Folding is
mandatory for persisted defaults and for these ordinary fully constant
subtrees; an unfolded nonconstant expression must behave identically when
reached.

An expression used only for an irrelevant EXISTS projection/ORDER BY value under
§20.14.5 is not a demanded scalar-control-flow path. It is name/type checked but
its constant error is not raised merely to establish row existence. Projection
values needed for DISTINCT cardinality before a relevant OFFSET are not covered
by that exemption; their folding follows the relational-subquery demand rule below.

More generally, a constant-error expression inside a relational subquery is
raised eagerly only when exact §20.14/§20.17.10 reasoning proves both that the
outer control flow demands that subquery occurrence and that the inner
relational execution demands that expression for at least one row. Estimated
or possible row production is insufficient. Otherwise the expression may fold
to an error-producing bound node, but the error occurs only if lazy runtime
execution reaches it. This preserves, for example, a skipped subquery branch
and an empty inner relation.

FLOAT64 folding uses binary64 round-to-nearest-ties-to-even at each bound
operator, forbids observable extended precision/contraction, preserves signed
zero, and normalizes NaN results exactly as runtime. Process floating environment,
compiler fast-math, locale, timezone, machine architecture, and libc formatting
defaults cannot change SQL results.

If an optimizer evaluates an aggregate over an exactly known constant relation,
it uses §29.3's aggregate state and finalization rather than treating the input
as an arbitrary left fold of scalar operators.

### 17.10.3 Equality modes, keys, and semantic proof

For every hashable non-NULL type, equality implies equal hash. Mixed numeric
hash/equality first applies the bound common-type cast. FLOAT64 hash/key
normalization maps both zeros together and every NaN together; VARCHAR hashes
exact bytes. The hash algorithm itself remains process-local.

Ordinary `=` and join predicates use §17.7 3VL: any NULL key is nonmatching, and
non-NULL NaNs match because canonical FLOAT64 equality says TRUE. Hash join must
reproduce that result. GROUP BY/DISTINCT use the same non-NULL equality but place
all NULLs into one group/duplicate class; that grouping NULL rule never leaks
into ordinary `=`.

The canonical scalar representative of a GROUPING-equivalence class is defined
by `CanonicalGroupingRepresentative(value)`:

```text
typed NULL                           -> the same typed NULL state
FLOAT64 -0.0 or +0.0                -> +0.0
any grouping-equivalent FLOAT64 NaN -> the canonical quiet NaN semantic value
every other non-NULL scalar       -> its existing semantic value representation
```

The mapping is componentwise for composite grouping values. VARCHAR class
members already have identical lengths and bytes, and BOOLEAN, integer, DATE,
and TIMESTAMP classes require no additional normalization. This mapping owns
only the relational output representative of a GROUPING-equivalence class; it
does not rewrite ordinary scalar values, scalar arithmetic, casts, persisted
FLOAT64 payloads, or persistent key encodings.

B+ uses a deterministic total order including NULL placement and BOOLEAN
physical order. SQL FLOAT64 and every SQL-orderable non-NULL type use the same
value order, but ordinary comparison with NULL remains UNKNOWN and BOOLEAN SQL
ordering is unsupported. Index-bound construction must preserve those predicate
rules and retain a residual recheck whenever a physical bound alone is broader.
§11.10 UNIQUE bypasses any NULL-containing key and uses exactly this normalized
non-NULL equality for every component.

Sections 34.1 and 35.2 may derive a semantic proof from a scalar fact only when all literals,
overloads, casts, and predicates are entries in this closed registry and exact
evaluation is error-safe. `1=2` is exact FALSE and `NULL=1` exact UNKNOWN;
unsupported/implementation-defined operations and statistics/required_rows are
never scalar semantic proof.

This registry changes no TypeId, PersistedScalarV1 byte, heap scalar payload, or
B+ key encoding. Those codecs are the representation owners.

### 17.10.4 Forbidden scalar implementations

The architecture explicitly forbids:

1. typing every integer literal as INT64 instead of applying §17.5.1;
2. implementing SQL integer arithmetic with C++ signed overflow or unchecked negation;
3. inheriting host `MIN / -1` or `MIN % -1` behavior;
4. inheriting implementation-dependent negative quotient/remainder rules;
5. silently converting VARCHAR to numeric/temporal/BOOLEAN for comparison or assignment;
6. locale-sensitive numeric, BOOLEAN, DATE, or TIMESTAMP parsing;
7. treating SQL NULL comparison like C++ pointer/optional equality;
8. using raw IEEE unordered NaN predicates instead of the explicit SQL FLOAT64 comparator;
9. hashing `-0.0` and `+0.0`, or canonical-equivalent NaNs, differently in an equality mode that equates them;
10. folding a value/error/branch differently from runtime execution;
11. treating `std::to_string`, iostreams, libc `%g`, or another library default as canonical FLOAT64 text;
12. accepting leading/trailing whitespace or trailing junk in explicit string casts;
13. truncating/wrapping a narrowing integer or FLOAT64-to-integer cast;
14. using required_rows, estimates, statistics, or observed data as scalar semantic facts;
15. inventing an implicit cast to make an absent overload bind;
16. applying GROUP BY's NULL-equals-NULL rule to ordinary `=` or join equality;
17. varying scalar results with process locale, timezone, floating environment, or machine architecture;
18. binding a scalar function merely because a standard/runtime library provides it;
19. serializing host scalar/struct representation instead of the existing explicit persisted codecs.

## 17.11 Generic scalar Value

A generic owned `Value` representation is acceptable in non-hot semantic paths such as:

```text
parser literals
constant folding
catalog defaults
tests
diagnostic/debug printing
```

Conceptually it contains:

```text
LogicalType
is_null
small scalar payload
owned VARCHAR bytes
```

It is not the per-cell representation for hot vectorized execution.

The executor's vector representation is defined in Chapter 23.

## 17.12 Type/value invariants

1. NULL is primarily a value state, not a storable column type.
2. The unresolved type marker is binder-only and never appears in persisted schemas or ordinary resolved execution.
3. Numeric implicit conversion occurs only in §17.8.5's registered contexts, widens, and never silently narrows.
4. Non-BOOLEAN predicates are rejected.
5. SQL Boolean semantics are three-valued.
6. Ordinary comparison with NULL returns NULL.
7. `x = NULL` is not rewritten as `x IS NULL`.
8. General expression comparison does not silently stringify unlike values.
9. VARCHAR equality/order is binary bytewise and agrees with B+ ordering.
10. FLOAT64 SQL comparison semantics agree with B+ key semantics.
11. DATE/TIMESTAMP logical units agree with their persisted signed scalar representations.
12. TypeResolver is the single semantic owner of coercion/type compatibility.
13. Generic Value is not the hot executor cell representation.
14. The registry in §§17.2–17.10 is closed; parser acceptance or runtime-library capability adds no SQL semantic entry.
15. Every integer operation/cast is checked before narrowing or publication and never relies on host signed overflow.
16. FLOAT64 arithmetic and comparison deliberately use their separately specified IEEE-arithmetic and canonical-total-comparison modes.
17. Scalar parsing/formatting is ASCII, locale-independent, timezone-independent, and full-consumption.
18. The named scalar-function registry is empty; aggregate functions remain separately registered in Chapter 29.
19. Constant folding reproduces runtime value, error, and short-circuit semantics exactly.
20. Assignment/default coercion stores or produces the final destination type and never leaves a deferred runtime default cast.

## 17.13 Persisted scalar value encoding

Catalog metadata that must persist a typed scalar value uses one shared byte-exact codec.

This codec is used by:

```text
v1 persisted column defaults
statistics min/max values
statistics MCV values
statistics histogram boundaries
```

It is independent of the executor's in-memory `Value`, Vector, or StringRef representation.

PersistedScalarV1 has no embedded version field. Its enclosing supported catalog/default/statistics grammar selects this exact codec. Unknown TypeIds, flags, lengths, reserved bytes, or padding are malformed v1 scalar data, not a future scalar version and not preserve-and-round-trip material. Under §4.14.6, required default/catalog ownership treats that as corruption; advisory statistics invalidate only the affected statistics generation.

### 17.13.1 Scalar header

Each encoded scalar begins with a 16-byte little-endian header:

| Offset | Size | Field / meaning |
|---:|---:|---|
| `0` | 4 | persisted TypeId |
| `4` | 4 | flags |
| `8` | 4 | payload_length |
| `12` | 4 | reserved32 = `0` |

The architecture defines:

```text
flags bit 0 = IS_NULL
all other bits = 0
```

The complete scalar encoding occupies:

```text
Align8(16 + payload_length)
```

bytes.

Any bytes after the payload up to that 8-byte boundary are zero padding.

### 17.13.2 NULL

For a typed NULL:

```text
IS_NULL       = 1
payload_length = 0
```

No payload bytes follow.

The TypeId still records the concrete SQL type of the NULL value.

### 17.13.3 Non-NULL payloads

For non-NULL values:

| Logical type | payload_length | payload |
|---|---:|---|
| BOOLEAN | `1` | byte `0` or `1` |
| INT32 | `4` | signed two's-complement int32 little-endian |
| INT64 | `8` | signed two's-complement int64 little-endian |
| FLOAT64 | `8` | IEEE-754 binary64 bits, uint64 little-endian |
| DATE | `4` | signed day-count int32 little-endian |
| TIMESTAMP | `8` | signed microsecond-count int64 little-endian |
| VARCHAR | byte length | exact string bytes, no terminator |

FLOAT64 persistence preserves the sign of zero because it can affect later arithmetic.

Every persisted NaN uses the canonical quiet-NaN bit pattern:

```text
0x7ff8000000000000
```

Other NaN payloads are not emitted by the v1 encoder.

### 17.13.4 Validation

The decoder rejects:

```text
unknown/non-storable TypeId
unknown flag bits
nonzero reserved32
NULL with nonzero payload_length
wrong fixed-width payload_length
BOOLEAN payload other than 0 or 1
nonzero alignment padding
noncanonical persisted NaN
length arithmetic overflow / truncated input
```

The decoder returns one owned typed semantic value; it never exposes a pointer into unvalidated persistent bytes as a catalog Value.

### 17.13.5 Scalar-codec invariants

1. One canonical scalar codec is shared by defaults and statistics.
2. The codec uses stable catalog TypeIds, never process enum ordinals.
3. VARCHAR payload length is byte length.
4. Persisted FLOAT64 NaNs are canonical while signed zero is preserved.
5. Padding/reserved bytes are zero and validated.
6. Scalar decoding is bounds checked before type-specific interpretation.
