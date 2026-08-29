# Chapter 17 review verdict

**CHAPTER 17 — CLEAN WITH OPTIONAL EDITORIAL NOTES.**

Chapter 17 is semantically complete, internally coherent, cross-chapter consistent, timeless, and independently implementable. It leaves no frozen architecture semantic question.

Finding counts:

```text
BLOCKING:  0
MAJOR:     0
MINOR:     0
EDITORIAL: 1
```

The only note concerns terminology: `UNKNOWN` denotes both a binder-only unresolved-type marker and the SQL three-valued-logic result represented by nullable BOOLEAN NULL. The chapter defines both meanings correctly, so this is not a semantic ambiguity.

## Scope and repository state

Primary scope:

- [Chapter 17 start](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13280)
- Last Chapter 17 line: 14,177
- [Chapter 18 boundary](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14178)

Actual Chapter 17 structure contains **46 headings**, including the chapter heading.

Context consulted:

- Architecture front matter
- Chapter 4 numeric, format, and corruption rules
- Chapter 5 tuple, NULL bitmap, scalar, VARCHAR, and tuple-view rules
- Chapter 8 index-key encodings and total order
- Chapter 15 DML/error-publication boundaries
- Chapter 16 TypeIds, descriptors, and historical schemas
- Referenced §§20.14, 20.17.10, 29.3, 34.1, 35.2, 39.1, 39.3.2, and 41.4
- Chapter 18 scope/boundary only
- Relevant verification procedures
- DEVELOPMENT and PROJECT_STATE only for documentation-role comparison

Initial Git state:

```text
working tree: clean
index:        clean
HEAD:         50ba11e018a90450ff5c79a08448f68647f2c310
```

## Section-by-section review

Legend:

- `T`: timeless
- `A`: architecture-owned
- `S`: analytically sufficient
- `D`: directly defined
- `R`: precisely delegated/referenced
- `—`: not locally applicable
- `P`: precise cross-reference
- `N`: editorial terminology note

| Section | Exact heading | Architectural role/domain | Time | Owner | Depth | Term | Type | NULL | Cast | Compare | Arith | Storage | Error | Xref | Consistency/status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 17 | SQL Type and Value System | Complete scalar semantic contract | T | A | S | C | D | D | D | D | D | R/D | D | P | CLEAN |
| 17.1 | Scope | Ownership boundary | T | A | S | C | D | D | D | D | D | R | R | P | CLEAN |
| 17.2 | Logical scalar types | Closed TypeId/type inventory | T | A | S | C | D | D | R | D | R | R | R | P | CLEAN |
| 17.3 | Storable types versus semantic pseudo-types | Typed NULL and unresolved typing | T | A | S | N | D | D | D | — | — | R | D | P | CLEAN WITH NOTE |
| 17.4 | Closed v1 scalar semantic registry | Closed semantic registry | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.4.1 | BOOLEAN | BOOLEAN domain | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.4.2 | INT32 and INT64 | Integer domains/checking | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.4.3 | FLOAT64 | IEEE arithmetic and canonical comparison | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.4.4 | DATE | Civil-day domain | T | A | S | C | D | D | D | D | — | R | D | P | CLEAN |
| 17.4.5 | TIMESTAMP | Naive microsecond domain | T | A | S | C | D | D | D | D | — | R | D | P | CLEAN |
| 17.4.6 | VARCHAR | Binary byte-string domain | T | A | S | C | D | D | R | D | — | R | D | P | CLEAN |
| 17.5 | Literal and textual scalar grammar | Semantic literal grammar | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.5.1 | Integer literals | Exact integer classification | T | A | S | C | D | — | — | — | D | — | D | P | CLEAN |
| 17.5.2 | FLOAT64 literals | Locale-independent float literals | T | A | S | C | D | — | — | — | D | — | D | P | CLEAN |
| 17.5.3 | VARCHAR and BOOLEAN literals | Exact bytes and Boolean keywords | T | A | S | C | D | D | — | — | — | — | D | P | CLEAN |
| 17.5.4 | DATE and TIMESTAMP text construction | Exact temporal grammar | T | A | S | C | D | — | D | — | — | R | D | P | CLEAN |
| 17.6 | Operator overload registry | Closed operator resolution | T | A | S | C | D | D | — | — | D | — | D | P | CLEAN |
| 17.6.1 | Unary operators | Unary type/result/error rules | T | A | S | C | D | D | — | — | D | — | D | P | CLEAN |
| 17.6.2 | Binary arithmetic | Complete arithmetic matrix | T | A | S | C | D | D | — | — | D | — | D | P | CLEAN |
| 17.7 | Comparisons, predicates, and three-valued logic | SQL comparison/3VL | T | A | S | C | D | D | R | D | — | R | D | P | CLEAN |
| 17.7.1 | Comparison overloads | Exact comparison compatibility | T | A | S | C | D | D | D | D | — | R | D | P | CLEAN |
| 17.7.2 | NULL predicates and 3VL | NULL comparisons/truth tables | T | A | S | C | D | D | — | D | — | R | D | P | CLEAN |
| 17.7.3 | Evaluation order and short-circuit | Observable error/evaluation order | T | A | S | C | — | D | — | D | D | — | D | P | CLEAN |
| 17.8 | Cast and coercion registry | Closed cast matrix | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.8.1 | Numeric casts | Widening/narrowing rules | T | A | S | C | D | D | D | — | D | — | D | P | CLEAN |
| 17.8.2 | VARCHAR parsing | Full-consumption parsing | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.8.3 | Canonical VARCHAR formatting | Observable CAST output | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.8.4 | DATE/TIMESTAMP casts | Checked temporal conversions | T | A | S | C | D | D | D | — | — | R | D | P | CLEAN |
| 17.8.5 | Implicit and assignment coercion | Contextual coercion registry | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.9 | CASE, IN, and scalar functions | Composite scalar expressions | T | A | S | C | D | D | D | D | — | — | D | P | CLEAN |
| 17.9.1 | Searched CASE | Type unification/lazy evaluation | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.9.2 | Scalar predicates | IN/NOT IN and predicate scope | T | A | S | C | D | D | D | D | — | — | D | P | CLEAN |
| 17.9.3 | Closed scalar-function registry | Empty v1 function registry | T | A | S | C | D | D | D | — | — | — | D | P | CLEAN |
| 17.10 | Type resolution, folding, and cross-engine equality | Cross-engine semantic authority | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.10.1 | Central TypeResolver and error taxonomy | Single resolution/error owner | T | A | S | C | D | D | D | D | D | — | D | P | CLEAN |
| 17.10.2 | Constant folding and errors | Folding/runtime equivalence | T | A | S | C | D | D | D | D | D | — | D | P | CLEAN |
| 17.10.3 | Equality modes, keys, and semantic proof | Predicate/group/hash/index modes | T | A | S | C | D | D | D | D | — | R | D | P | CLEAN |
| 17.10.4 | Forbidden scalar implementations | Determinism/implementation boundaries | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.11 | Generic scalar Value | Non-hot owned runtime value | T | A | S | C | D | D | — | — | — | R | — | Adequate | CLEAN |
| 17.12 | Type/value invariants | Consolidated semantic invariants | T | A | S | C | D | D | D | D | D | R | D | P | CLEAN |
| 17.13 | Persisted scalar value encoding | Metadata-scalar codec | T | A | S | C | D | D | — | — | — | D | D | P | CLEAN |
| 17.13.1 | Scalar header | Byte-exact framing | T | A | S | C | D | D | — | — | — | D | D | P | CLEAN |
| 17.13.2 | NULL | Typed-NULL persisted encoding | T | A | S | C | D | D | — | — | — | D | D | P | CLEAN |
| 17.13.3 | Non-NULL payloads | Type-specific payloads | T | A | S | C | D | — | — | — | — | D | D | P | CLEAN |
| 17.13.4 | Validation | Malformed-scalar rejection | T | A | S | C | D | D | — | — | — | D | D | P | CLEAN |
| 17.13.5 | Scalar-codec invariants | Canonical codec summary | T | A | S | C | D | D | — | — | — | D | D | P | CLEAN |

## Canonical ownership map

| Concept | Canonical owner | Chapter 17 relationship |
|---|---|---|
| Persistent TypeId numbers | Chapter 16 §16.4 | Consumes exact 1–7 registry |
| SQL semantic type meaning | Chapter 17 | Owns |
| Typed NULL/UNKNOWN semantics | Chapter 17 | Owns |
| Literal classification | Chapter 17; Chapter 18 tokenizes | Owns semantic classification |
| Cast/coercion compatibility | Chapter 17 TypeResolver | Owns |
| Arithmetic/comparison semantics | Chapter 17 | Owns |
| Generic non-hot Value | Chapter 17 | Conceptual owned representation |
| Hot vector representation | Chapters 23/25 | Referenced only |
| Ordinary tuple scalar bytes | Chapter 5 | Delegated |
| Persisted metadata scalar bytes | Chapter 17 §17.13 | Owns distinct codec |
| Catalog column TypeIds | Chapter 16 | Consumes |
| Index key bytes/total order | Chapter 8 | Must agree semantically |
| Historical schema resolution | Chapter 16 §16.7 | Consumes exact old TypeId |
| Statement transaction consequence | §39.1/Chapter 15 | Chapter 17 identifies semantic error only |
| Aggregate value semantics | Chapter 29 | Separate closed registry |
| Scalar subquery semantics | §20.14 | Uses Chapter 17 types/comparisons |
| Presentation outside CAST | Client/output layers | Not owned here |

Ownership is precise. Chapter 17 does not duplicate ordinary tuple or B+ encodings.

## Exact v1 type matrix

| Type | TypeId | NULL Value? | Domain | Runtime representation constrained? | Tuple owner | Index owner | Equality/order | Implicit casts | Explicit casts | Operators |
|---|---:|---|---|---|---|---|---|---|---|---|
| BOOLEAN | 1 | Yes | FALSE/TRUE | Semantic state only | Ch. 5 | Ch. 8 | Equality; no SQL order | Identity | VARCHAR | NOT, AND, OR |
| INT32 | 2 | Yes | `[-2^31,2^31-1]` | Mathematical signed domain | Ch. 5 | Ch. 8 | Numeric equality/order | INT64, FLOAT64 | VARCHAR | `+ - * / %`, unary `+/-` |
| INT64 | 3 | Yes | `[-2^63,2^63-1]` | Mathematical signed domain | Ch. 5 | Ch. 8 | Numeric equality/order | FLOAT64 | INT32, VARCHAR | `+ - * / %`, unary `+/-` |
| FLOAT64 | 4 | Yes | All binary64 values | IEEE semantics fixed; container free | Ch. 5 | Ch. 8 | Canonical total comparison | Identity | INT32, INT64, VARCHAR | `+ - * /`, unary `+/-` |
| DATE | 5 | Yes | Signed int32 civil-day count | Semantic day count | Ch. 5 | Ch. 8 | Signed day equality/order | Identity | VARCHAR, TIMESTAMP | No arithmetic |
| TIMESTAMP | 6 | Yes | Signed int64 microsecond count | Semantic microsecond count | Ch. 5 | Ch. 8 | Signed timestamp equality/order | Identity | VARCHAR, DATE | No arithmetic |
| VARCHAR | 7 | Yes | Arbitrary finite byte string | Required lifetime only | Ch. 5 | Ch. 8 | Exact-byte equality/unsigned lexicographic order | Identity | BOOLEAN, numeric, DATE, TIMESTAMP | No string operators |

The runtime tag need not numerically equal persisted TypeId. Stable TypeIds never derive from process enum order.

Canonical SQL spellings are the seven listed names. No alias or parameterized type is registered; under the closed-registry rule, unlisted spellings and `VARCHAR(n)` do not gain semantics.

## NULL, UNKNOWN, and Value model

- NULL is a value state attached to a concrete type.
- NULL is not a persisted TypeId.
- An untyped NULL token initially carries the binder-only unresolved marker.
- Context converts it into a typed NULL.
- Underconstrained NULL expressions produce bind-time `TYPE_ERROR`.
- SQL truth UNKNOWN is nullable BOOLEAN NULL.
- UNKNOWN never reaches persisted schemas or executor vector element types as a separate type.
- `CAST(NULL AS T)` produces typed NULL without parsing or range checking.
- `IS NULL`/`IS NOT NULL` always return non-NULL BOOLEAN.
- Generic `Value` owns VARCHAR bytes and is permitted only for non-hot semantic paths.
- Borrowed tuple/page values cannot outlive the owning page guard; escaping execution values must own or otherwise retain valid backing storage.

## Cast matrix

Legend: `=` identity, `I` implicit widening, `E` explicit-only, `—` forbidden.

| Source \ Target | BOOL | INT32 | INT64 | FLOAT64 | VARCHAR | DATE | TIMESTAMP |
|---|---:|---:|---:|---:|---:|---:|---:|
| BOOLEAN | = | — | — | — | E | — | — |
| INT32 | — | = | I | I | E | — | — |
| INT64 | — | E | = | I | E | — | — |
| FLOAT64 | — | E | E | = | E | — | — |
| VARCHAR | E | E | E | E | = | E | E |
| DATE | — | — | — | — | E | = | E |
| TIMESTAMP | — | — | — | — | E | E | = |

Key rules:

- INT32→INT64 is exact.
- Integer→FLOAT64 uses nearest/ties-even and may be inexact.
- INT64→INT32 is range checked.
- FLOAT64→integer rejects NaN/infinity, truncates finite values toward zero, then checks range.
- VARCHAR parsing is ASCII, locale-independent, full-consumption, and has exact grammars.
- DATE↔TIMESTAMP uses checked day/microsecond conversion and prior-midnight flooring for negative timestamps.
- Assignment narrowing, temporal conversion, parsing, and BOOLEAN conversion require explicit CAST.
- NULL input to any legal cast yields typed NULL.
- Direct casts are defined independently; implementations may not synthesize them by chaining other casts.

## Comparison matrix

Legend: `EQ` equality only, `EO` equality and ordering, `NUM` numeric common-type EO, `—` unsupported.

| Left \ Right | BOOL | INT32 | INT64 | FLOAT64 | VARCHAR | DATE | TIMESTAMP |
|---|---:|---:|---:|---:|---:|---:|---:|
| BOOLEAN | EQ | — | — | — | — | — | — |
| INT32 | — | EO | NUM | NUM | — | — | — |
| INT64 | — | NUM | EO | NUM | — | — | — |
| FLOAT64 | — | NUM | NUM | EO | — | — | — |
| VARCHAR | — | — | — | — | EO | — | — |
| DATE | — | — | — | — | — | EO | — |
| TIMESTAMP | — | — | — | — | — | — | EO |

Every ordinary comparison with NULL returns UNKNOWN. That SQL rule is separate from Chapter 8’s physical `NULLS FIRST` total order.

Non-NULL equality/order agrees with index encoding:

- signed numeric/date/timestamp order;
- exact unsigned-byte VARCHAR order;
- FLOAT64 canonical zero/NaN classes;
- BOOLEAN physical order exists, but SQL ordering is unsupported.

## Arithmetic matrix

| Operator | Legal operands | Result | NULL | Error/edge semantics |
|---|---|---|---|---|
| unary `+` | INT32/INT64/FLOAT64 | Same type | Strict | Identity; FLOAT preserves signed zero |
| unary `-` | INT32/INT64 | Same type | Strict | MIN overflows except direct literal construction |
| unary `-` | FLOAT64 | FLOAT64 | Strict | IEEE sign change; NaN canonicalized |
| `+ - *` | same/mixed integers | Smallest common integer | Strict | Checked, no wrap |
| `/` | same/mixed integers | Smallest common integer | Strict | Toward zero; zero divisor error; MIN/−1 overflow |
| `%` | same/mixed integers | Smallest common integer | Strict | Remainder sign follows dividend; zero and MIN/−1 errors |
| `+ - * /` | any pair containing FLOAT64 | FLOAT64 | Strict | Per-operator binary64 nearest/ties-even |
| `%` | any FLOAT64 pair | Unsupported | — | Bind-time TYPE_ERROR |
| temporal arithmetic | DATE/TIMESTAMP | Unsupported | — | Bind-time TYPE_ERROR |
| VARCHAR arithmetic/concatenation | VARCHAR | Unsupported | — | Bind-time TYPE_ERROR |

Reassociation is not permitted where it changes the bound operator-tree result or error. Constant folding must reproduce runtime semantics exactly.

## NULL and three-valued logic matrix

| Operation | Required result |
|---|---|
| `NOT TRUE` / `NOT FALSE` / `NOT UNKNOWN` | FALSE / TRUE / UNKNOWN |
| `TRUE AND TRUE/FALSE/UNKNOWN` | TRUE / FALSE / UNKNOWN |
| `FALSE AND any` | FALSE; RHS skipped |
| `UNKNOWN AND TRUE/FALSE/UNKNOWN` | UNKNOWN / FALSE / UNKNOWN |
| `TRUE OR any` | TRUE; RHS skipped |
| `FALSE OR TRUE/FALSE/UNKNOWN` | TRUE / FALSE / UNKNOWN |
| `UNKNOWN OR TRUE/FALSE/UNKNOWN` | TRUE / UNKNOWN / UNKNOWN |
| ordinary comparison with either operand NULL | UNKNOWN |
| `x IS NULL` | non-NULL TRUE iff x is NULL |
| `x IS NOT NULL` | non-NULL inverse |
| WHERE/HAVING/ON | Only TRUE qualifies/matches |
| `IN` | First TRUE wins; otherwise UNKNOWN if any comparison UNKNOWN; else FALSE |
| `NOT IN` | Exact 3VL NOT of IN |

Evaluation is left-to-right. Skipped AND/OR, CASE, and expression-list IN branches do not raise their errors.

## FLOAT64 matrix

| Class | Legal? | Equality/order | Integer cast | Tuple bytes | Index bytes | Canonicalization |
|---|---|---|---|---|---|---|
| `+0.0` | Yes | Equal to `-0.0` | 0 | Bits preserved | Normalized zero | Semantic zero class |
| `-0.0` | Yes | Equal to `+0.0` | 0 | Sign preserved | Normalized zero | Sign preserved for arithmetic/text |
| finite ± | Yes | Numeric | Truncate then range check | Exact bits | Order transform | No value change |
| subnormal | Yes | Numeric | Usually zero/range-checked | Exact bits | Order transform | Correct rounding required |
| max finite | Yes | Numeric | Range checked | Exact bits | Order transform | Arithmetic may overflow to infinity |
| `+∞`/`−∞` | Yes | Ordered around finite values | `INVALID_CAST` | Exact bits | Canonical total order | Preserved |
| NaN | Yes | Every NaN equal; above +∞ | `INVALID_CAST` | Arbitrary tuple payload legal | All normalize together | Arithmetic/metadata/index output canonicalizes as specified |

Scalar operations round to binary64 after every operator. Extended precision, changed rounding mode, result-changing FMA/contraction, fast-math, host unordered comparison, and floating traps are forbidden.

## VARCHAR matrix

| Case | Valid? | Length | Equality/order | Tuple storage | Index storage |
|---|---|---|---|---|---|
| Empty | Yes, distinct from NULL | 0 bytes | Equal only to empty | Zero-length present descriptor | Terminator encoding |
| ASCII | Yes | Bytes | Exact unsigned bytes | Exact bytes | Memcomparable bytes |
| High-bit bytes | Yes | Bytes | Unsigned comparison | Exact bytes | Exact escaped encoding |
| Embedded NUL | Yes | Includes NUL | Ordinary byte | Length-delimited | `00 FF` escaped |
| Prefix pair | Yes | Bytes | Shorter equal-prefix first | Exact bytes | Terminator preserves order |
| Equal bytes | Yes | Bytes | Equal | Identical payload | Equal user-key component |
| Different bytes | Yes | Bytes | Lexicographic | Exact payload | Matching physical order |
| `VARCHAR(n)` | Not a v1 type | N/A | N/A | N/A | N/A |
| Complete tuple >8135 bytes | Rejected by Chapter 5 | Bytes remain semantic | Unchanged | `ROW_TOO_LARGE` | N/A |
| Encoded user key >1024 bytes | Value remains valid | Bytes remain semantic | Unchanged | May be storable | Index operation fails explicitly |

There is no UTF-8 validation, normalization, character count, locale collation, terminator, or case folding.

## Temporal matrix

| Case | DATE | TIMESTAMP |
|---|---|---|
| Semantic storage domain | Every signed int32 day count | Every signed int64 microsecond count |
| Epoch | `1970-01-01` day 0 | `1970-01-01 00:00:00.000000` microsecond 0 |
| Pre-epoch | Negative day counts valid | Negative microseconds valid |
| Text range | Civil years 0001–9999 | Civil years 0001–9999 |
| Leap day | Proleptic Gregorian validation | Same calendar |
| Invalid calendar/time | `INVALID_DATE` | `INVALID_TIMESTAMP` |
| Text precision | Exact date | 0–6 fractional digits, right-padded |
| DATE→TIMESTAMP | Checked prior midnight | Result |
| TIMESTAMP→DATE | Result | Floor to containing prior midnight for negative values |
| Timezone suffix/input | Unsupported | Unsupported |
| Timezone model | N/A | Timezone-naive |
| Arithmetic | Unsupported | Unsupported |
| Physical owner | Chapter 5 signed int32 | Chapter 5 signed int64 |
| Index owner | Chapter 8 signed order | Chapter 8 signed order |

All fixed-width bit patterns are valid semantic DATE/TIMESTAMP values. “Invalid temporal value” applies to textual construction, not an otherwise valid signed persisted scalar.

## Storage handoff matrix

| Type | Semantic owner | Tuple owner | TypeId owner | Index owner | NULL owner | Validation | Historical owner |
|---|---|---|---|---|---|---|---|
| BOOLEAN | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Ch. 17 + Ch. 5 bitmap | Ch. 5/17.13 | Ch. 16.7 |
| INT32 | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Same | Ch. 16.7 |
| INT64 | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Same | Ch. 16.7 |
| FLOAT64 | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Tuple bits Ch. 5; metadata NaN §17.13 | Ch. 16.7 |
| DATE | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Same | Ch. 16.7 |
| TIMESTAMP | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Same | Ch. 16.7 |
| VARCHAR | Ch. 17 | Ch. 5 | Ch. 16 | Ch. 8 | Same | Ch. 5 lengths; §17.13 metadata | Ch. 16.7 |

Round trips preserve semantic values. Ordinary tuple NaN payloads may differ while remaining one canonical semantic NaN class; PersistedScalarV1 requires its exact canonical NaN bits.

## Error matrix

| Condition | Class | NULL substituted? | Statement-state owner | Canonical owner |
|---|---|---|---|---|
| Unsupported type/operator/coercion | `TYPE_ERROR` | No | §39.1 | §§17.4, 17.10.1 |
| Out-of-domain literal | `INVALID_LITERAL` | No | §39.1 | §§17.5, 17.10.1 |
| Supported cast with invalid value | `INVALID_CAST` | No | §39.1 | §17.8 |
| Checked integer overflow | `NUMERIC_OVERFLOW` | No | §39.1 | §§17.6, 39.3.1 |
| Integer divide/remainder by zero | `DIVISION_BY_ZERO` | No | §39.1 | §§17.6.2, 39.3.1 |
| Narrowing overflow | `NUMERIC_OVERFLOW` | No | §39.1 | §17.8.1 |
| Invalid BOOLEAN text | `INVALID_CAST` | No | §39.1 | §17.8.2 |
| Invalid DATE text | `INVALID_DATE` | No | §39.1 | §§17.5.4, 17.8.2 |
| Invalid TIMESTAMP text | `INVALID_TIMESTAMP` | No | §39.1 | Same |
| NaN/infinity→integer | `INVALID_CAST` | No | §39.1 | §17.8.1 |
| Unknown required TypeId | Corruption | No | Storage/open owner | §§4.14.6, 16.4, 17.13 |
| Malformed tuple scalar | Heap corruption | No | Storage owner | Chapter 5 |
| Malformed required PersistedScalarV1 | Catalog/default corruption | No | Catalog/open owner | §17.13 |
| Malformed advisory statistics scalar | Reject generation/fallback | No | Statistics owner | §§17.13, 34.14 |
| Value/string allocation failure | Resource error | No | §39.1 | Query-memory owner |

No ordinary semantic error silently becomes NULL.

## Type resolution, optimizer, and determinism

- TypeResolver is the single registry owner.
- Binder inserts all permitted implicit casts.
- Executor receives fully resolved operator/cast identities and performs no new coercion.
- Constant folding reproduces runtime values, NULLs, errors, rounding, and branch demand.
- Integer checked-overflow semantics prevent unsafe algebraic reassociation.
- AND/OR, CASE, and expression-list IN preserve exact source-order demand.
- Hash equality, grouping equality, join equality, UNIQUE equality, and index ordering use explicitly distinguished modes.
- Equal non-NULL values always hash equally.
- Grouping places NULLs together; ordinary `=` still returns UNKNOWN for NULL.
- Process locale, timezone, floating environment, machine architecture, native endian, `char` signedness, libc parsing/formatting, and C++ overflow cannot alter SQL results.
- Native C++/library names appear only in explicit forbidden-implementation rationale.
- No runtime `Value` object is serialized by native layout.

## Temporality and documentation ownership

| Occurrence | Classification | Result |
|---|---|---|
| “Future type parameters…later architecture/version” | Persistent evolution/durable v1 scope | Valid |
| NULL “initially binds” | Runtime binding order | Valid |
| “later NULLability…check” | Runtime semantic order | Valid |
| named function `NOW`/`CURRENT_TIMESTAMP` | SQL identifiers, not chronology | Valid |
| “later IN item” | Expression evaluation order | Valid |
| “future-capable IR machinery” | Durable closed-registry boundary | Valid |
| executor representation “defined later” | Document navigation | Valid |
| signed zero affecting later arithmetic | Runtime value history | Valid |

Results:

- Project chronology: **none**
- Current implementation narration: **none**
- DEVELOPMENT sequencing: **none**
- VERIFICATION recipes: **none**
- PROJECT_STATE leakage: **none**
- Devlog/history: **none**
- Presentation semantics leakage: **none**
- Source-layout mandates: **none**

The mention of tests as one permissible non-hot `Value` consumer is not a verification recipe and creates no architecture dependency.

## Analytical-depth and terminology assessment

| Mechanism | Assessment |
|---|---|
| Closed TypeId/type registry | Analytically sufficient |
| Typed NULL/untyped NULL | Analytically sufficient |
| SQL 3VL versus index NULL order | Analytically sufficient |
| Checked integer arithmetic | Analytically sufficient |
| FLOAT64 canonical comparison and IEEE arithmetic | Analytically sufficient |
| Binary VARCHAR | Analytically sufficient |
| DATE/TIMESTAMP | Analytically sufficient |
| Cast/coercion registry | Analytically sufficient |
| Short-circuit/error observability | Analytically sufficient |
| Folding/runtime equivalence | Analytically sufficient |
| Equality/hash/group/index modes | Analytically sufficient |
| Persistent scalar codec | Analytically sufficient |
| `UNKNOWN` terminology | Exact but optionally clearer |

Canonical terminology:

| Term | Meaning |
|---|---|
| SQL type/logical type | One of seven closed semantic types |
| TypeId | Stable persisted numeric identity |
| TypeKind/runtime tag | Process representation; not persisted identity |
| NULL | Absent value state for a concrete type |
| untyped NULL | NULL token awaiting contextual typing |
| binder UNKNOWN | Unresolved type marker |
| SQL UNKNOWN | Typed nullable BOOLEAN NULL truth result |
| typed NULL | Concrete TypeId plus null validity |
| cast | Registered explicit or identity conversion |
| coercion | Context-authorized automatic conversion |
| comparison | SQL 3VL operation |
| physical total order | B+ key ordering including NULL |
| overflow | Checked range failure, never wrap |
| VARCHAR | Arbitrary binary byte string |
| DATE | Signed civil-day count |
| TIMESTAMP | Signed timezone-naive microsecond count |
| PersistedScalarV1 | Metadata/default/statistics scalar codec |

Normative wording is sufficient: correctness rules use MUST/MUST NOT or exact closed tables; MAY is used only for implementation freedom.

## Explicit cross-reference audit

| Source | Target | Purpose | Exists/owner/precision |
|---|---|---|---|
| 17.1 | Chapters 5 and 8 | Tuple/index representation ownership | Valid, precise owner |
| 17.2 | §16.4 | TypeId registry | Valid |
| 17.2 | §17.4.3 | FLOAT comparison | Valid |
| 17.2 | §17.13 | Metadata scalar codec | Valid |
| 17.2 | Chapter 8 | Index bytes | Valid |
| 17.4.2 | Chapter 5 | Integer persisted widths | Valid |
| 17.4.3 | Chapter 8 | FLOAT total order | Valid |
| 17.4.3 | §39.3.2 | FLOAT division boundary | Valid |
| 17.4.3 | §29.3 | Aggregate FLOAT semantics | Valid |
| 17.4.6 | Chapter 8 | VARCHAR index ordering | Valid |
| 17.5.3 | §17.3 | NULL literal typing | Valid |
| 17.6.1 | §17.5.1 | Direct negative literal edge | Valid |
| 17.6.1 | §17.7 | 3VL NOT | Valid |
| 17.6.2 | §17.4.3 | FLOAT arithmetic | Valid |
| 17.7.1 | §17.4.3 | FLOAT comparison | Valid |
| 17.8.2 | §17.5.4 | Temporal grammar | Valid |
| 17.8.3 | §17.8.2 | Round-trip float parser | Valid |
| 17.9.1 | §17.7.3 | CASE evaluation order | Valid |
| 17.9.2 | §17.8.5 | IN common type | Valid |
| 17.9.2 | §20.14 | Subquery predicates | Valid |
| 17.9.3 | §29.3 | Aggregate registry | Valid |
| 17.10.1 | §§17.6–17.9 | Resolver registry | Valid |
| 17.10.1 | §17.10.2 | Folding timing | Valid |
| 17.10.1 | §39.1 | Transaction consequence | Valid |
| 17.10.2 | §17.7.3 | Short-circuit | Valid |
| 17.10.2 | §§20.14.5, 20.14, 20.17.10 | Relational/subquery demand | Valid |
| 17.10.2 | §29.3 | Aggregate constant evaluation | Valid |
| 17.10.3 | §17.7 | Predicate equality | Valid |
| 17.10.3 | §11.10 | UNIQUE equality | Valid |
| 17.10.3 | §§34.1, 35.2 | Semantic proof boundary | Valid |
| 17.10.4 | §17.5.1 | Integer literal classification | Valid |
| 17.12 | §§17.2–17.10 and Chapter 29 | Registry/invariant summary | Valid |
| 17.13 | §4.14.6 | Corruption/advisory fallback | Valid |

No broken, wrong-owner, or correctness-significantly vague cross-reference was found.

## Documentation-model matrix

| # | Requirement | Result |
|---:|---|---|
| 1 | Timeless wording | CONSISTENT |
| 2 | No implementation-state narration | CONSISTENT |
| 3 | No Phase-2 narration | CONSISTENT |
| 4 | No DEVELOPMENT sequencing | CONSISTENT |
| 5 | No VERIFICATION recipes | CONSISTENT |
| 6 | No PROJECT_STATE leakage | CONSISTENT |
| 7 | No history/devlog leakage | CONSISTENT |
| 8 | No source-representation coupling | CONSISTENT |
| 9 | TypeId ownership precise | CONSISTENT |
| 10 | Value semantics ownership precise | CONSISTENT |
| 11 | Tuple delegation precise | CONSISTENT |
| 12 | Index-order delegation precise | CONSISTENT |
| 13 | NULL terminology precise | CONSISTENT |
| 14 | SQL comparison/physical order separated | CONSISTENT |
| 15 | Cast terminology precise | CONSISTENT |
| 16 | Overflow terminology precise | CONSISTENT |
| 17 | Error/corruption separated | CONSISTENT |
| 18 | Rationale sufficient | CONSISTENT |
| 19 | Implementation freedom preserved | CONSISTENT |
| 20 | Independently readable years later | CONSISTENT |

## 120-item technical consistency matrix

`C` = CONSISTENT, `S` = CONSISTENT BUT SPECIALIZED, `N/A` = absent/delegated by design.

| Items | Items | Items | Items |
|---|---|---|---|
| 1 TypeId mapping—C | 2 closed set—C | 3 NULL model—C | 4 UNKNOWN model—C |
| 5 typed NULL—C | 6 BOOLEAN—C | 7 3VL—C | 8 SQL comparison—C |
| 9 index-order distinction—C | 10 integer domains—C | 11 widening—C | 12 narrowing—C |
| 13 arithmetic—C | 14 overflow—C | 15 division—C | 16 FLOAT64—C |
| 17 NaN—C | 18 infinity—C | 19 negative zero—C | 20 VARCHAR bytes—C |
| 21 VARCHAR length—C | 22 VARCHAR order—C | 23 embedded NUL—C | 24 collation—C |
| 25 DATE—C | 26 TIMESTAMP—C | 27 timezone—C | 28 casts—C |
| 29 implicit coercion—C | 30 explicit coercion—C | 31 NULL casts—C | 32 mixed comparison—C |
| 33 result types—C | 34 string casts—C | 35 Boolean casts—C | 36 literal typing—C |
| 37 assignment coercion—C | 38 tuple handoff—C | 39 index handoff—C | 40 catalog handoff—C |
| 41 historical schema—C | 42 defaults—C | 43 validation order—C | 44 persisted invalid—C |
| 45 user invalid—C | 46 corruption—C | 47 error taxonomy—C | 48 resource errors—S |
| 49 platform determinism—C | 50 host overflow—C | 51 locale—C | 52 timezone environment—C |
| 53 floating environment—C | 54 native representation—C | 55 tag vs TypeId—C | 56 value lifetime—C |
| 57 borrowed strings—C | 58 page aliasing—C | 59 round-trip—C | 60 order preservation—C |
| 61 equality/order consistency—C | 62 UNIQUE consistency—C | 63 NULL UNIQUE—C | 64 PK NULL—C |
| 65 grouping—C | 66 hashing—C | 67 SQL sorting—S | 68 constant folding—C |
| 69 short-circuit—C | 70 optimizer equivalence—C | 71 error precedence—C | 72 SchemaVer typing—C |
| 73 TypeId nonreuse—C | 74 type extensibility—C | 75 type parameters—N/A |
| 76 unsupported types—C | 77 DECIMAL scope—C | 78 INTERVAL scope—C |
| 79 BLOB scope—C | 80 array/JSON scope—C | 81 system-catalog types—C |
| 82 default blobs—C | 83 index-key limit—C | 84 tuple-size limit—C |
| 85 declared VARCHAR limit—N/A | 86 DATE range—C | 87 TIMESTAMP range—C |
| 88 arithmetic NULL propagation—C | 89 comparison NULL propagation—C |
| 90 IS NULL—C | 91 Boolean operators—C | 92 NULL casts—C |
| 93 integer-min edge—C | 94 MIN/−1—C | 95 signed zero—C |
| 96 float/integer conversion—C | 97 VARCHAR/numeric coercion—C |
| 98 DATE/string coercion—C | 99 BOOL/numeric coercion—C |
| 100 unknown TypeId—C | 101 malformed BOOL—C | 102 malformed DATE text—C |
| 103 malformed TIMESTAMP text—C | 104 malformed VARCHAR—C |
| 105 malformed FLOAT metadata—S | 106 source coupling—C |
| 107 algorithm freedom—C | 108 temporality—C | 109 current-state leakage—C |
| 110 verification leakage—C | 111 development leakage—C | 112 rationale—C |
| 113 terminology—C with editorial note | 114 normative language—C |
| 115 cross-references—C | 116 persistent/runtime separation—C |
| 117 semantic/presentation separation—C | 118 statement-failure ownership—C |
| 119 implementer invention—C | 120 timeless readability—C |

## Cross-chapter consistency

| Owner | Result |
|---|---|
| Chapter 4 numeric domains/version/corruption | CONSISTENT |
| Chapter 5 tuple NULL/scalar/VARCHAR bytes | CONSISTENT BUT SPECIALIZED |
| Chapter 8 index total order | CONSISTENT |
| Chapter 9 transaction lifecycle | CONSISTENT |
| Chapter 15 DML validation/publication failures | CONSISTENT |
| Chapter 16 TypeIds/descriptors/history | CONSISTENT |
| Chapter 18 tokenization boundary | CONSISTENT |
| Chapter 20 subqueries/folding/rewrite demand | CONSISTENT |
| Chapter 29 aggregates | CONSISTENT BUT SEPARATE N-ARY SEMANTICS |
| §§34.1/35.2 semantic proof boundary | CONSISTENT |
| §39 error/transaction consequences | CONSISTENT |
| §41 verification obligations | CONSISTENT, but methodology needs expansion |

Previous chapters remain frozen and unregressed. Chapter 16’s exact TypeIds 1–7, NULL/UNKNOWN exclusion, descriptor semantics, and historical schema ownership are consumed without drift.

## Finding

### F17-E1 — overloaded `UNKNOWN` terminology

- Section: §§17.3 and 17.7.2
- Evidence:
  - “`UNKNOWN` is a binder-only unresolved type…”
  - “UNKNOWN is nullable BOOLEAN NULL.”
- Severity: EDITORIAL
- Primary type: TERMINOLOGY
- Scope: cross-section
- Arithmetic: N/A
- Explanation: the chapter uses one word for two explicitly distinguished concepts: an unresolved binder type marker and the SQL 3VL truth result.
- Canonical comparison: the former disappears during binding; the latter is a runtime typed BOOLEAN NULL.
- Consequence: no semantic uncertainty, but casual implementations or diagnostics may use imprecise names.
- Owner: Chapter 17
- Future action: **G. TERMINOLOGY NORMALIZATION**
- Suggested direction: call the binder state “unresolved type” or `UNKNOWN_TYPE`, while retaining SQL `UNKNOWN` for 3VL.
- Required change: optional only.

No BLOCKING, MAJOR, or MINOR findings exist.

## Frozen architecture semantic questions

**NONE.**

All requested behavior is determinable without policy invention, including type names under the closed registry, NULL typing, float edge cases, casts, comparison modes, overflow, string bytes, temporal domains, error timing, and storage ownership.

## Follow-up verification gaps

Chapter 17 is clearer and broader than its current direct methodology. The following are verification gaps, not architecture defects:

| Family | Current coverage | Missing direct methodology | Reusable coverage |
|---|---|---|---|
| Closed TypeResolver/type registry | PARTIAL | Exhaustive accepted/rejected registry and result-nullability matrix | Binder property tests |
| Literal classification | PARTIAL | INT32/INT64/direct-negative boundaries and FLOAT grammar/rounding vectors | Parser tests |
| Contextual NULL typing | PARTIAL | Complete context matrix and every underconstrained error | Binder/3VL tests |
| Cast/coercion registry | PARTIAL | Full 7×7 explicit/implicit/assignment matrix | Existing cast execution tests |
| VARCHAR parse/format | MISSING/PARTIAL | Full-consumption grammar and canonical FLOAT text round-trip | String/parser tests |
| DATE/TIMESTAMP | MISSING | Signed-domain/text/leap/pre-epoch/cast boundary matrix | Tuple/index scalar tests |
| PersistedScalarV1 | MISSING | Byte-exact header/payload/padding/NULL/NaN/corruption fixtures | Catalog/default/statistics harness |
| FLOAT cross-engine equivalence | PARTIAL | Canonical text and metadata scalar composition | Index and expression FLOAT tests |
| Error taxonomy | PARTIAL | User error versus required corruption versus advisory fallback | §39/error harness |
| Storage handoff | PARTIAL | One end-to-end tuple/metadata/index semantic round-trip matrix | Chapters 5, 8, and 16 procedures |
| Folding/error dominance | SUBSTANTIAL | Complete constant-error demand matrix outside and inside subqueries | Logical rewrite/subquery tests |
| Unsupported registry entries | PARTIAL | Exhaustive rejected type/operator/predicate/function cases | Parser/binder negative tests |

Recommended follow-up: **Chapter 17 type/value verification synchronization** before beginning the Chapter 18 review.

## Final direct answers

| Question | Answer |
|---|---|
| Project-time/current-state wording? | NO |
| DEVELOPMENT-owned material? | NO |
| VERIFICATION recipe leakage? | NO |
| PROJECT_STATE material? | NO |
| Devlog/history? | NO |
| TypeId ambiguity? | NO |
| NULL/UNKNOWN semantic ambiguity? | NO; one editorial naming overlap |
| SQL comparison/index-order ambiguity? | NO |
| Cast/coercion ambiguity? | NO |
| Arithmetic/overflow ambiguity? | NO |
| FLOAT ambiguity? | NO |
| VARCHAR ambiguity? | NO |
| DATE/TIMESTAMP ambiguity? | NO |
| Value-lifetime ambiguity? | NO |
| User-error/corruption ambiguity? | NO |
| Platform-determinism ambiguity? | NO |
| Short-circuit/error-observability ambiguity? | NO |
| Correctness-relevant implementer invention required? | NO |
| Can Chapter 17 stand as timeless canonical v1 architecture? | YES |

Recommended next action: **no required architecture edit; perform verification synchronization**. The optional `UNKNOWN` terminology cleanup can be included in a later document-only polish or omitted without semantic risk.

Recommended Chapter 18 review boundary, when separately authorized: start at `# 18. Lexer, Parser, and AST`, review every subsection through the line before Chapter 19, with particular attention to token grammar, literal handoff to Chapter 17, source spans, parser recovery, supported statement syntax, precedence, and AST ownership. That review was not started here.

## Read-only guarantee

Final state:

```text
working tree: clean
index:        clean
HEAD:         50ba11e018a90450ff5c79a08448f68647f2c310
git diff --check: PASS
```

Repository-state change: **NONE**.

Files modified by this audit: **NONE**.

No review artifact was read, modified, created, or staged. No implementation, build, test, benchmark, formatting, staging, commit, or devlog work occurred.

**Phase 2 remains NOT STARTED / NOT AUTHORIZED.**
