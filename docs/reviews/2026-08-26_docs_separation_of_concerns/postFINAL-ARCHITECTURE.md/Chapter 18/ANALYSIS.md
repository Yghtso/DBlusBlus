# Chapter 18 verdict

**CHAPTER 18 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 18 has a sound high-level lexer/parser/binder separation, exact source-byte spans, clear string and numeric delegation, a useful precedence skeleton, and coherent no-`FROM`/subquery integration. It is not yet a complete canonical grammar contract. Eight issues require frozen semantic decisions because different conforming implementations could tokenize the same bytes differently, accept different SQL, or produce materially different ASTs.

Finding totals:

| Severity | Count |
|---|---:|
| BLOCKING | 8 |
| MAJOR | 3 |
| MINOR | 4 |
| EDITORIAL | 0 |

## Scope and repository state

- Primary scope: [docs/ARCHITECTURE.md:14188](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14188)
- Start: `# 18. Lexer, Parser, and AST`
- Last substantive Chapter-18 line: invariant 14 at line 14569.
- Boundary separator: lines 14571–14572.
- Next heading: [Chapter 19, `# 19. Binding and Expression Semantics`](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14573)
- No Chapter-19 direct review was performed.

Context-only architecture consulted:

- Chapter 15 statement/write-failure ownership.
- Chapter 16 namespace, canonical catalog names, and TypeIds.
- Chapter 17 literal grammar, unresolved NULL, operators, predicates, CASE/IN, TypeResolver.
- Chapter 19 binder boundary and expression ownership.
- §§20.5, 20.14, 20.15 for no-`FROM` and subqueries.
- Chapter 21 SQL scope, semantic planning, and parser recovery.
- §29.3 aggregate syntax.
- §§39.2–39.3 error categories.
- §41.4 front-end verification obligations.

Other live documents consulted:

- `AGENTS.md`
- `docs/DEVELOPMENT.md`, only front-end sequencing/layout guidance.
- `docs/VERIFICATION.md`, SQL grammar, binder, source-span, fuzzing, and Chapter-17 literal coverage.
- `docs/PROJECT_STATE.md`, only to classify current-state ownership.

No review artifact was read.

## Actual Chapter-18 heading inventory

| Section | Exact heading | Canonical responsibility | Domain | Documentation role |
|---|---|---|---|---|
| 18 | Lexer, Parser, and AST | SQL syntax front-end | Whole front end | Architecture-appropriate |
| 18.1 | Front-end boundary | Parser/binder separation | Layering | Architecture-appropriate |
| 18.2 | Token model | Token payload and spans | Lexer/diagnostics | Architecture with role issue |
| 18.3 | Token classes | Token categories and keyword recognition | Lexer | Architecture with semantic gap |
| 18.4 | Identifier rules | Identifier normalization | Lexing/name handoff | Architecture with semantic gap |
| 18.5 | String literals | String delimiters and decoding | Literal lexing | Architecture-appropriate |
| 18.6 | Numeric literals | Token grammar and semantic handoff | Numeric lexing | Architecture with semantic gap |
| 18.7 | Comments | Supported comment forms | Lexer | Architecture with semantic gap |
| 18.8 | Source locations | AST span contract | Diagnostics | Architecture-appropriate |
| 18.9 | Parser architecture | Parsing technique | Implementation technique | Non-architecture material leakage |
| 18.10 | Initial statement set | Top-level statement inventory | Statement grammar | Architecture with role/semantic issues |
| 18.11 | SELECT grammar surface | SELECT clauses and joins | Query grammar | Architecture with semantic gap |
| 18.11.1 | SELECT without FROM | No-`FROM` relational semantics | Binding/logical planning | Architecture with document-role issue |
| 18.12 | Subquery syntax | Closed subquery syntax classes | Expression/relation grammar | Architecture with error-boundary issue |
| 18.13 | AST contract | Raw AST semantic boundary | AST | Architecture with semantic gap |
| 18.14 | AST ownership | AST lifetime/allocation | Runtime ownership | Architecture with implementation coupling |
| 18.15 | Expression precedence | Operator hierarchy | Expression grammar | Architecture with semantic gap |
| 18.16 | Front-end invariants | Cross-section summary | Integration | Architecture with inherited issues |

## Section-by-section review

Legend: `C` consistent, `F` finding, `P` partial, `N/A` not owned.

| Section | Role | Time | Owner | Depth | Terms | Lex | Grammar | Prec./assoc. | AST | Binder boundary | Ch.17 handoff | Errors | X-ref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 18 | Front-end umbrella | C | C | P | C | F | F | F | F | C | P | P | C | P | FINDING |
| 18.1 | Layer boundary | C | C | C | C | N/A | C | N/A | C | C | C | C | C | C | CONSISTENT |
| 18.2 | Token representation | C | F | P | C | F | N/A | N/A | P | C | P | P | C | P | FINDING |
| 18.3 | Token classes | F | C | P | P | F | F | N/A | N/A | C | P | P | N/A | F | FINDING |
| 18.4 | Identifiers | C | C | F | P | F | F | N/A | P | C | N/A | F | P | F | FINDING |
| 18.5 | Strings | C | C | C | C | C | C | N/A | P | C | C | C | C | C | CONSISTENT |
| 18.6 | Numerics | C | C | P | C | C | P | P | F | C | F | C | C | F | FINDING |
| 18.7 | Comments | C | C | P | C | F | P | N/A | N/A | C | N/A | P | N/A | P | FINDING |
| 18.8 | Locations | C | C | P | C | C | N/A | N/A | C | C | N/A | F | C | P | FINDING |
| 18.9 | Parsing method | C | F | C | C | N/A | N/A | C | P | C | N/A | P | N/A | C | FINDING |
| 18.10 | Statements | F | C | F | P | N/A | F | N/A | F | C | N/A | F | P | F | FINDING |
| 18.11 | SELECT surface | F | C | P | C | N/A | F | P | F | C | P | P | C | P | FINDING |
| 18.11.1 | No-FROM semantics | C | F | C | C | N/A | P | N/A | P | C | C | C | C | C | FINDING |
| 18.12 | Subqueries | C | C | P | C | N/A | P | F | P | P | C | F | C | P | FINDING |
| 18.13 | AST contract | C | C | F | C | N/A | P | P | F | C | P | P | C | F | FINDING |
| 18.14 | AST lifetime | C | F | C | C | N/A | N/A | N/A | P | C | N/A | N/A | N/A | C | FINDING |
| 18.15 | Precedence | F | C | P | C | N/A | F | P | P | C | P | N/A | P | P | FINDING |
| 18.16 | Invariants | C | C | P | P | P | P | P | P | C | P | P | C | P | FINDING |

## Canonical owner map

| Concept | Canonical owner | Assessment |
|---|---|---|
| Input bytes, whitespace, comments | Chapter 18 | Owned but incomplete |
| Token kinds and token boundaries | Chapter 18 | Owned but incomplete |
| Keywords/reservation | Chapter 18 | Owned but incomplete |
| Identifier syntax/canonicalization | Chapter 18 | Owned but incomplete |
| String token decoding | Chapter 18 consuming §17.5.3 | Precise for strings |
| Numeric token grammar/sign separation | Chapter 18 consuming §§17.5.1–17.5.2 | Mostly precise |
| Literal type/value classification | Chapter 17 | Correctly delegated |
| NULL unresolved-type state | Chapter 17/binder | Correctly delegated |
| Statement and expression grammar | Chapter 18 | Owned but substantially incomplete |
| Precedence/associativity | Chapter 18 | Partially defined |
| Raw AST syntax and ordering | Chapter 18 | Owned but incomplete |
| Name/catalog resolution | Chapter 19 | Correctly delegated |
| Cast/operator/function resolution | Chapters 17 and 19 | Correctly delegated |
| Statement semantics | Chapters 15, 20, 21 | Referenced/integrated |
| Aggregate semantics | Chapter 29 | Referenced only indirectly |
| Subquery semantics | §20.14 | Precisely delegated |
| Front-end error categories | §§21.16, 39.2 | Later owner |
| Verification methodology | `VERIFICATION.md` | Correctly external |

The lexer/parser/AST boundary is directionally correct: syntax remains textual and catalog-free. The unresolved defects concern exactly what syntax exists and what information the AST must preserve.

## Lexical contract assessment

### Input, whitespace, and comments

- Source spans are byte-based and half-open.
- §17.5.3 establishes length-delimited SQL input, embedded NUL inside strings, and lexical rejection of unquoted NUL.
- Chapter 18 does not define the general source encoding or byte domain.
- No whitespace grammar exists.
- Tabs, CR, LF, CRLF, form feed, high bytes outside strings, and whitespace/comment separation are unspecified.
- `--` and non-nested `/* ... */` exist.
- Unterminated block comments are lexical errors.
- The precise line-comment terminator set and EOF behavior are not stated.
- Comment delimiters inside strings are harmless under the closed string grammar, but the general comment/token boundary remains incomplete.

### Keywords and identifiers

Settled:

- Keywords are case-insensitive.
- Unquoted identifiers normalize to lowercase.
- Quoted identifiers preserve exact bytes/case.
- AST identifiers remain textual.
- Chapter 16 owns binary catalog comparison and nonempty catalog names.

Undefined:

- unquoted identifier start/continuation characters;
- whether folding is ASCII-only;
- locale independence of folding;
- quoted-identifier delimiter and escape syntax;
- non-ASCII/high-byte identifier handling;
- quoted NUL handling;
- empty quoted identifier parse behavior;
- reserved versus contextual keyword inventory;
- which keywords may be aliases or identifiers.

This is correctness-significant because keyword addition/reservation and identifier folding alter parse structure and catalog lookup.

### Strings

The string contract is coherent:

- single-quote delimiter;
- doubled single quote;
- backslash ordinary;
- decoded logical byte payload;
- original half-open span retained;
- embedded NUL allowed inside the literal;
- arbitrary bytes/no Unicode normalization;
- unterminated literal produces positioned lexical error.

Payload lifetime after token-to-AST transfer is not defined.

### Numeric, Boolean, NULL, and temporal literals

- Integer and FLOAT token grammars are exactly delegated to §§17.5.1–17.5.2.
- Signs are separate unary tokens.
- Type selection/overflow is not performed through unchecked host conversion.
- `TRUE`/`FALSE` are Chapter-17 Boolean literals.
- `NULL` remains an untyped syntax candidate; no parser TypeId is authorized.
- DATE/TIMESTAMP have no direct typed-literal syntax.
- Exact VARCHAR casts construct temporal values.

The direct-negative handoff is incomplete: Chapter 17 makes parentheses break the special form, but Chapter 18 does not require the AST to preserve that distinction.

## Tokens, punctuation, and framing

No closed operator or punctuation inventory is present. Consequently longest-match behavior for `<=`, `<>`, `>=`, comment delimiters, dots, and unsupported punctuation is not canonical.

Statement framing is also undefined:

- Semicolons appear in examples.
- §21.17 acknowledges multi-statement batches and semicolon synchronization.
- Chapter 18 does not state whether a single statement requires or merely permits a semicolon.
- Empty statements, repeated semicolons, trailing semicolons, batch grammar, and trailing-token rejection are unspecified.

## Supported statement inventory

The live list is:

1. `CREATE TABLE`
2. `CREATE INDEX`
3. `CREATE UNIQUE INDEX`
4. `DROP TABLE`
5. `DROP INDEX`
6. `INSERT`
7. `UPDATE`
8. `DELETE`
9. `SELECT`
10. `BEGIN`
11. `COMMIT`
12. `ROLLBACK`
13. `VACUUM`
14. `ANALYZE`
15. `EXPLAIN`
16. `EXPLAIN ANALYZE`

Only `ANALYZE table_name;`, a high-level SELECT skeleton, and the subquery classes receive Chapter-18 grammar text. The other statement entries have no canonical production.

Explicitly excluded in Chapter 18 include `ALTER TABLE`, RIGHT/FULL joins, LATERAL, recursive CTEs, window functions, set operations, correlation, row/multi-column subqueries, ANY/SOME/ALL, data-modifying subqueries, and derived-column alias lists. Chapter 21 lists additional semantic exclusions.

## Expression grammar and precedence

The defined precedence, low to high, is:

| Rank | Forms | Associativity | Example shape | Status |
|---:|---|---|---|---|
| 1 | `OR` | left | `(a OR b) OR c` | Clear |
| 2 | `AND` | left | `(a AND b) AND c` | Clear |
| 3 | prefix `NOT` | Not stated | `NOT (a = b)` by rank | Partial |
| 4 | comparison / `IS NULL` / `IN` | comparisons non-chainable | `a < b` | Partial grammar |
| 5 | binary `+ -` | left | `(a-b)-c` | Clear |
| 6 | `* / %` | left | `(a/b)*c` | Clear |
| 7 | unary `+ -` | Not stated | `-(a*b)` does not follow; unary binds tighter | Partial |
| 8 | primary | N/A | literal/name/call/parentheses | Primary set undefined |

Settled:

- Parentheses override precedence.
- Arithmetic and Boolean infix forms are left-associative.
- Comparisons are non-chainable.
- `a < b < c` is rejected.
- `NOT` binds below comparison and above `AND`.

Undefined:

- exact grammar for `IS NOT NULL`;
- expression-list `IN` and empty-list rejection;
- interaction of prefix `NOT`, `NOT IN`, and `NOT EXISTS`;
- CAST production/type syntax;
- searched CASE production at the parser boundary;
- function/aggregate call syntax;
- star contexts;
- primary-expression inventory;
- qualified-name arity;
- parenthesized expression versus subquery;
- unary-operator associativity.

## Statement grammar matrix

| Statement | Chapter-18 syntax | Optional clauses | AST root stated? | Unsupported extensions stated? | Status |
|---|---|---|---|---|---|
| CREATE TABLE | Name only | Undefined | Representative root | ALTER/CHECK/FK only elsewhere | Finding |
| CREATE INDEX | Name only | UNIQUE variant listed | Not listed | Expression/partial indexes later excluded | Finding |
| DROP TABLE | Name only | Undefined | Not listed | Undefined | Finding |
| DROP INDEX | Name only | Undefined | Not listed | Undefined | Finding |
| INSERT | Name only | Undefined | Yes | Some semantics in Ch.21 | Finding |
| UPDATE | Name only | Undefined | Yes | Some semantics in Ch.21 | Finding |
| DELETE | Name only | Undefined | Yes | Some semantics in Ch.21 | Finding |
| SELECT | Clause skeleton | DISTINCT/FROM/etc. | Yes | Several exclusions | Partial |
| BEGIN | Name only | Undefined | Not listed | Isolation/options undefined | Finding |
| COMMIT | Name only | Undefined | Not listed | Undefined | Finding |
| ROLLBACK | Name only | Undefined | Not listed | Undefined | Finding |
| VACUUM | Name only | Undefined | Not listed | Undefined | Finding |
| ANALYZE | `ANALYZE table_name;` | None stated | Yes | Targetless excluded | Clear locally |
| EXPLAIN | Name only | Undefined | Not listed | Undefined | Finding |
| EXPLAIN ANALYZE | Name only | Undefined | Not listed | Undefined | Finding |
| Subquery SELECT | Five syntax classes | Derived `AS` optional; alias required | Generic subquery node | Closed exclusions | Partial |

## AST assessment

Actual representative node inventory:

| Node | Ordered children/payload defined? | Span | Unresolved names | Resolved IDs allowed? | Status |
|---|---|---|---|---|---|
| SelectStatement | No | Required | Yes | No catalog IDs | Partial |
| CreateTableStatement | No | Required | Yes | No catalog IDs | Partial |
| InsertStatement | No | Required | Yes | No catalog IDs | Partial |
| UpdateStatement | No | Required | Yes | No catalog IDs | Partial |
| DeleteStatement | No | Required | Yes | No catalog IDs | Partial |
| AnalyzeStatement | Target name stated | Required | Yes | TableId forbidden | Clear |
| AstIdentifier | Text/quoting incompletely specified | Required | Yes | No | Partial |
| AstQualifiedName | Components not specified | Required | Yes | No | Partial |
| AstLiteral | Payload category incomplete | Required | N/A | TypeId unspecified | Partial |
| AstBinaryExpression | Child order not normative | Required | N/A | Resolved operator forbidden by boundary | Finding |
| AstUnaryExpression | Direct-parenthesis distinction absent | Required | N/A | Resolved operator forbidden | Finding |
| AstFunctionCall | Name/argument/star grammar absent | Required | Yes | Semantic identity forbidden pre-bind | Partial |
| AstCast | Target type payload unspecified | Required | N/A | TypeId ownership unclear | Finding |
| AstStar | Context/qualifier unspecified | Required | N/A | No expansion | Partial |
| AstSubqueryExpression | Kind stated indirectly | Required | Names unresolved | Bound identity later | Partial |

Raw AST versus bound tree is otherwise well separated:

- Raw AST owns textual syntax.
- Binder owns TableId, ColumnId, BindingId, TypeId resolution, overload selection, and cast insertion.
- Parser/catalog independence is clear.
- Parser/TypeResolver independence is clear.
- Parse success is correctly distinct from semantic validity.

Missing AST invariants:

- complete node taxonomy;
- exact ordered-child/list model;
- preservation of duplicates;
- preservation of direct-negative syntactic adjacency versus parentheses;
- type-syntax payload;
- token/source-buffer lifetime;
- partial AST observability on failure.

## Source order and duplicates

Chapter 17 requires source order for AND/OR, searched CASE, and IN lists. Chapter 18 does not normatively require ordered AST children or ordered list storage.

Likewise, it does not forbid parser-side deduplication of:

- SELECT expressions;
- IN items;
- CASE arms;
- VALUES rows/items;
- UPDATE assignments;
- INSERT columns;
- GROUP/ORDER items;
- index columns.

Later binders can reject or interpret duplicates only if the parser preserves them.

## Error and resource assessment

| Boundary | Owner/result | Assessment |
|---|---|---|
| Invalid source byte/token | Chapter 18 / `LexerError` | Exact byte classes missing |
| Unterminated string/block comment | `LexerError` | Clear |
| Unexpected grammar token | `ParserError` | Category clear; grammar incomplete |
| Out-of-domain numeric literal | Chapter 17 `INVALID_LITERAL` | Clear |
| Unsupported operator/cast/type | Binder/Chapter 17 `TYPE_ERROR` | Clear |
| Unknown catalog object | Binder/catalog | Clear |
| Unsupported subquery syntax | Syntax or binding | Ambiguous |
| Partial AST after error | Not defined | Finding |
| Batch recovery | §21.17 permits fail-fast/synchronization | Implementation freedom acceptable, observability incomplete |
| Allocation/stack exhaustion | No front-end classification | Finding |
| Transaction consequence | §39.1 | Correctly delegated |

## Platform determinism

- Numeric token grammar and semantic conversion are ASCII/locale-independent through Chapter 17.
- String bytes are binary-safe.
- Source spans are byte-based.
- Identifier folding, keyword folding, whitespace classification, and high-byte treatment remain potentially locale/platform-dependent.
- Nothing authorizes `strtol`/`strtod` semantic ownership in the lexer.
- Native integer/FLOAT conversion must not preempt Chapter 17.
- Recursion-depth and token-length exhaustion have no architecture-level error boundary.
- No persistent AST format exists or is implied.

## Literal handoff matrix

| Syntax | Token/AST handoff | Parse-time TypeId? | Semantic owner | Status |
|---|---|---:|---|---|
| `NULL` | Untyped literal candidate | No | §17.3 / Binder | Consistent |
| `TRUE`, `FALSE` | Keyword/literal | Not required | §17.5.3 | Consistent |
| Integer | Unsigned decimal token; sign separate | No final TypeId | §17.5.1 | Partial: grouping distinction |
| FLOAT | Exact unsigned token spelling/representation | No final TypeId | §17.5.2 | Consistent |
| VARCHAR | Decoded bytes plus source span | No final TypeId required | §§17.2, 17.5.3 | Consistent |
| DATE/TIMESTAMP | No typed-literal syntax | No | VARCHAR cast via Ch.17 | Consistent |
| CAST type syntax | Unspecified | Unspecified | TypeResolver/Binder | Finding |

## Ambiguity matrix

| Token sequence | Competing interpretations | Canonical result available? | Status |
|---|---|---:|---|
| `-9223372036854775808` | Direct-negative literal versus runtime unary minus | Result defined; AST preservation absent | Finding |
| `-(9223372036854775808)` | Direct form versus parenthesized invalid positive literal | Chapter 17 distinguishes; AST rule absent | Finding |
| `NOT a = b` | `(NOT a)=b` versus `NOT(a=b)` | `NOT(a=b)` from precedence | Consistent |
| `a NOT IN (...)` | Composite predicate versus `(NOT a) IN` | Intended composite; grammar incomplete | Finding |
| `NOT a IN (...)` | Prefix NOT over IN versus malformed composite | Precedence suggests `NOT(a IN...)`; production absent | Finding |
| `a IS NOT NULL` | Composite postfix predicate | Semantics exist; syntax production absent | Finding |
| `*` | multiply, projection star, qualified star, aggregate star | Context contract absent | Finding |
| `a.b` | qualified name | Intended; component grammar absent | Finding |
| `1.2` / `.` | float versus punctuation | Float grammar known; dot token rules absent | Finding |
| `(SELECT ...)` | scalar subquery versus parenthesized form | Subquery class stated | Consistent |
| `expr alias` | implicit alias versus clause start | Alias grammar absent | Finding |
| `stmt; stmt` | batch versus trailing tokens | §21.17 implies batch, grammar absent | Finding |

## Identifier matrix

| Case | Lexically valid? | Canonical bytes | Error/owner |
|---|---:|---|---|
| Simple ASCII | Implied, not formally defined | Lowercase | Grammar finding |
| Mixed case | If valid | Lowercase | Clear normalization |
| Reserved word unquoted | Undefined | Undefined | Keyword finding |
| Reserved word quoted | Intended identifier | Exact bytes | Quote grammar missing |
| Empty quoted identifier | Undefined | Catalog ultimately forbids empty | Boundary missing |
| Embedded identifier quote | Undefined | Undefined | Quote escape missing |
| Non-ASCII/high byte | Undefined | Undefined | Determinism finding |
| Embedded NUL | Unquoted NUL rejected; quoted case undefined | Undefined | Finding |
| Long identifier | No separate catalog semantic max | Exact bytes if admitted | Resource boundary missing |

## String matrix

| Case | Lex valid? | Semantic bytes | Status |
|---|---:|---|---|
| Empty `''` | Yes | Empty non-NULL bytes | Consistent |
| ASCII | Yes | Exact bytes | Consistent |
| Doubled quote | Yes | One quote byte | Consistent |
| Embedded NUL | Yes inside literal | Retained | Consistent |
| High bytes | Yes under arbitrary-byte domain | Retained | Consistent |
| Comment markers | Yes | Ordinary bytes | Consistent |
| Semicolon | Yes | Ordinary byte | Consistent |
| Newline | Permitted by arbitrary-byte semantic domain, lexer framing not explicit | Retained | Partial |
| Unterminated | No | `LexerError` | Consistent |

## Numeric token matrix

| Input | Token/AST | Chapter-17 outcome | Status |
|---|---|---|---|
| `0` | Integer token | INT32 | Complete |
| `001` | Integer token | INT32 1 | Complete |
| `2147483647` | Integer token | INT32_MAX | Complete |
| `2147483648` | Integer token | INT64 | Complete |
| `9223372036854775807` | Integer token | INT64_MAX | Complete |
| `9223372036854775808` | Integer representation | Only legal direct under `-` | AST gap |
| `-2147483648` | `-` plus token | INT32_MIN | Result complete |
| `-9223372036854775808` | `-` plus token | INT64_MIN | AST gap |
| `1.25` | FLOAT token | FLOAT64 | Complete |
| `1e10` | FLOAT token | FLOAT64 | Complete |
| `1e` | Invalid token sequence | Exact lexer/parser split unstated | Partial |
| `.5`, `1.` | Not FLOAT grammar | Dot-token behavior unstated | Partial |

## NULL/Boolean syntax matrix

| Source | Token/AST meaning | Parse-time resolved TypeId? | Status |
|---|---|---:|---|
| `NULL` | Untyped NULL syntax | No | Consistent |
| `TRUE` | Boolean literal keyword | Not required pre-bind | Consistent |
| `FALSE` | Boolean literal keyword | Not required pre-bind | Consistent |
| Mixed-case `null/true/false` | Case-insensitive keyword | No/Boolean after binding | Consistent, folding algorithm incomplete |
| Quoted `"NULL"` | Intended identifier | No | Quote grammar missing |
| Identifier named `null` | Requires quoting if reserved | Reservation table absent | Finding |

## Comma-list matrix

| Family | Empty? | Trailing comma? | Duplicates/order | Status |
|---|---:|---:|---|---|
| SELECT list | Undefined | Undefined | Not normative | Finding |
| GROUP BY | Undefined | Undefined | Not normative | Finding |
| ORDER BY | Undefined | Undefined | Not normative | Finding |
| IN expression list | Nonempty by §17.9.2 | Undefined | Source order required by Ch.17 | Partial |
| CASE WHEN list | At least one implied | N/A | Source order required | Partial |
| Function arguments | Undefined | Undefined | Not normative | Finding |
| VALUES rows/items | Undefined | Undefined | Not normative | Finding |
| INSERT columns | Undefined | Undefined | Must survive to binder | Finding |
| UPDATE SET list | Undefined | Undefined | Binder rejects duplicate targets | Finding |
| CREATE/index column lists | Undefined | Undefined | Declaration/key order semantically relevant | Finding |

## Parse-error matrix

| Defect | Expected family | Partial AST observable? | Status |
|---|---|---:|---|
| Invalid non-token byte | LexerError | Undefined | Input-domain blocker |
| Unterminated string | LexerError | Not stated | Error clear |
| Unterminated quoted identifier | Undefined | Undefined | Identifier blocker |
| Unterminated block comment | LexerError | Not stated | Error clear |
| Unexpected token | ParserError | Undefined | Grammar-dependent |
| Missing `)` | ParserError | Undefined | Methodology exists |
| Missing `END` | ParserError if CASE recognized | Undefined | CASE grammar missing |
| Trailing operator | ParserError | Undefined | Operator inventory missing |
| Trailing comma | Undefined | Undefined | List grammar missing |
| Duplicate clause | Intended parser failure | Undefined | Exact production missing |
| Wrong clause order | Intended parser failure | Undefined | SELECT sketch only |
| Trailing tokens | Undefined | Undefined | Framing blocker |
| Unsupported statement | ParserError or UnsupportedFeature | Undefined | Boundary incomplete |

## Cross-chapter composition

| Owner | Chapter-18 relationship | Duplication/precision | Status |
|---|---|---|---|
| Ch.15 | Statement effects/failures | No syntax delegation needed | Consistent |
| Ch.16 | Canonical names/TypeIds | Names textual pre-bind | Consistent |
| Ch.17 | Literal/operator/type semantics | Good delegation except direct-negative preservation | Finding |
| Ch.18 | Lexing, grammar, raw AST | Primary owner | Incomplete |
| Ch.19 | Binding/name/type resolution | Clear boundary | Consistent |
| Ch.20 | No-FROM/subqueries/logical shape | §18.11.1 duplicates extensive semantics | Minor role issue |
| Ch.21 | DDL/DML semantics and recovery | Recovery text partly fills framing context | Owner split |
| Ch.29 | Aggregate registry | Parser call syntax not defined | Finding |
| §39 | Error/transaction effects | Correct later owner | Consistent |
| §41 | Verification obligations | Cannot fill undefined semantics | Blocked |
| `VERIFICATION.md` | Procedures | Useful but partly blocked | Partial |

## Documentation-model matrix

| Item | Result |
|---|---|
| 1. Timeless wording | FINDING |
| 2. No current implementation narration | CONSISTENT |
| 3. No Phase-2 narration | CONSISTENT |
| 4. No DEVELOPMENT sequencing | FINDING |
| 5. No VERIFICATION recipes | CONSISTENT |
| 6. No PROJECT_STATE leakage | CONSISTENT |
| 7. No history/devlog | CONSISTENT |
| 8. No parser-technique mandate | FINDING |
| 9. No source-layout coupling | CONSISTENT |
| 10. Lexer ownership precise | FINDING |
| 11. Parser ownership precise | FINDING |
| 12. AST ownership precise | FINDING |
| 13. Binder boundary precise | CONSISTENT |
| 14. Chapter-17 literal handoff precise | FINDING |
| 15. Identifier canonicalization precise | FINDING |
| 16. Precedence precise | FINDING |
| 17. Error ownership precise | FINDING |
| 18. Rationale sufficient | CONSISTENT BUT SPECIALIZED |
| 19. Implementation freedom preserved | FINDING |
| 20. Readable without project history | FINDING |

## Temporality classification

| Wording | Classification | Result |
|---|---|---|
| “source byte span”, “next token”, “after parsing” | Runtime/source ordering | Valid |
| “Version 1” / “v1” scope | Durable architecture scope | Valid |
| “Initial token classes” | Project chronology | Finding |
| “initial parser/binder surface” | Project chronology | Finding |
| “reserved for the future all-table form” | Roadmap chronology | Finding |
| “deferred initially” / “adding it later” | Roadmap chronology | Finding |
| “Initial FROM supports” | Project chronology | Finding |
| “Deferred from the initial surface” | Mixed durable scope/chronology | Rewrite recommended |
| “Initial precedence” | Project chronology | Finding |

No `currently`, Phase-2, test-count, milestone, review-history, or implementation-status narration appears in Chapter 18.

## Analytical depth and terminology

| Mechanism | Assessment |
|---|---|
| Parser/binder separation | Analytically sufficient |
| Source-byte spans | Semantically clear; exact error selection thin |
| String decoding | Analytically sufficient |
| Numeric semantic delegation | Sufficient except direct-negative AST preservation |
| Identifier canonicalization | Analytical-depth finding |
| Keyword/reservation model | Analytical-depth finding |
| Statement grammar | Semantic-completeness finding |
| Expression precedence | Clear skeleton, incomplete production model |
| AST role | Clear boundary, incomplete structural contract |
| AST arena | Rationale supplied, but implementation-coupled |
| No-FROM semantics | Analytically strong but misplaced/duplicative |
| Subqueries | Strong semantic delegation; error stage ambiguous |

Canonical terms are generally used consistently:

| Term | Canonical meaning | Assessment |
|---|---|---|
| Token | Lexer output with kind/span/payload | Clear |
| Source span | Half-open byte interval | Clear |
| Identifier | Textual unresolved name | Clear |
| Quoted identifier | Case-preserving identifier | Lexical form incomplete |
| Literal | Syntax handed to Chapter 17 | Clear concept |
| AST | Raw syntax tree | Clear concept, incomplete taxonomy |
| Bind/resolve | Name/type semantic resolution | Clear |
| TypeId | Resolved/persistent type identity | Should remain absent pre-bind |
| ParserError/LexerError | Front-end categories | Later owner; branch detail incomplete |

Normative language is too weak where exactness matters:

- “include” and “representative” leave token/AST sets open;
- “where practical” weakens direct-negative preservation;
- “according to where recognition occurs” explicitly permits divergent error ownership;
- “conventional left associativity” is acceptable for listed binary operators;
- unsupported syntax “fails explicitly” is strong but lacks a closed syntax inventory.

## Explicit cross-references

| Source | Target | Purpose | Exists/owner | Precision |
|---|---|---|---|---|
| 18.1 | Chapter 19 | Binder boundary | Yes/correct | Precise |
| 18.5 | §17.5.3 | String bytes/NUL/escapes | Yes/correct | Precise |
| 18.6 | §§17.5.1–17.5.2 | Numeric grammar | Yes/correct | Precise |
| 18.6 | §17.5.1 | Direct-negative result | Yes/correct | Incomplete AST composition |
| 18.11.1 | §20.5 | Zero-column one-row Values source | Yes/correct | Precise |
| 18.11.1 | §20.15 | SELECT logical order | Yes/correct | Precise |
| 18.11.1 | Chapter 17 | Scalar semantics | Yes/correct | Broad but adequate |
| 18.11.1 | §20.14.5 | EXISTS demand | Yes/correct | Precise |
| 18.11.1 | §20.14 | Subquery behavior | Yes/correct | Precise |
| 18.12 | §20.14 | Closed semantic matrix | Yes/correct | Precise |
| 18.16 | Chapter 17 registry | Binder acceptance | Yes/correct | Broad but recoverable |
| 18.16 | §18.11.1 | No-FROM semantics | Yes/self-reference | Precise |

## Technical consistency matrix

Legend: `C` consistent, `CS` consistent but specialized/delegated, `F` finding, `N/A` not part of the live v1 surface.

| # | Correctness question | Result |
|---:|---|---|
| 1 | Exact source-byte domain defined? | F |
| 2 | Query encoding defined? | F |
| 3 | Invalid non-token bytes classified? | F |
| 4 | Unquoted NUL rejected? | C |
| 5 | Exact whitespace set defined? | F |
| 6 | CR/LF/CRLF behavior defined? | F |
| 7 | Tab/form-feed behavior defined? | F |
| 8 | Comments separate adjacent tokens deterministically? | F |
| 9 | Line-comment terminator defined? | F |
| 10 | Line-comment EOF behavior defined? | F |
| 11 | Block comments supported? | C |
| 12 | Unterminated block comment rejected? | C |
| 13 | Block comments explicitly non-nested? | C |
| 14 | Comment markers inside strings remain bytes? | C |
| 15 | End-of-input token exists? | C |
| 16 | Unquoted identifier grammar defined? | F |
| 17 | Identifier lowercase mapping exact? | F |
| 18 | Identifier folding locale-independent? | F |
| 19 | Quoted-identifier delimiter defined? | F |
| 20 | Quoted-identifier escape defined? | F |
| 21 | Empty quoted identifier boundary defined? | F |
| 22 | Non-ASCII identifier behavior defined? | F |
| 23 | Quoted identifier NUL behavior defined? | F |
| 24 | Unquoted spelling normalizes before binding? | C |
| 25 | AST names remain textual? | C |
| 26 | Catalog lookup excluded from parser? | C |
| 27 | Catalog names are nonempty under Ch.16? | CS |
| 28 | No extra catalog identifier-length limit? | CS |
| 29 | Keywords case-insensitive? | C |
| 30 | Keyword fold algorithm exact? | F |
| 31 | Keyword inventory closed? | F |
| 32 | Reserved/contextual classes defined? | F |
| 33 | Quoted keyword behavior fully defined? | F |
| 34 | Grammar-version keyword evolution defined? | N/A |
| 35 | Identifier/keyword conflict deterministic? | F |
| 36 | Single-quoted strings defined? | C |
| 37 | Doubled-quote decoding defined? | C |
| 38 | Decoded string payload retained? | C |
| 39 | Backslash has no escape meaning? | C |
| 40 | Embedded string NUL retained? | C |
| 41 | Source NUL outside strings rejected? | C |
| 42 | String Unicode normalization excluded? | C |
| 43 | High-byte string payload legal? | C |
| 44 | Empty string legal? | C |
| 45 | String newlines compose with lexer input? | F |
| 46 | Unterminated string rejected? | C |
| 47 | Original string span retained? | C |
| 48 | Semicolon inside string nonterminating? | C |
| 49 | Comment marker inside string ordinary? | C |
| 50 | Decoded payload lifetime defined? | F |
| 51 | Integer token grammar exact? | C |
| 52 | FLOAT token grammar exact? | C |
| 53 | Numeric sign is separate token? | C |
| 54 | Direct-negative minimum result defined? | C |
| 55 | Parentheses-breaking distinction preserved in AST? | F |
| 56 | Magnitude `2^63` retained safely? | CS |
| 57 | Raw/mathematical numeric payload requirement exact? | F |
| 58 | Host integer overflow excluded? | C |
| 59 | Host FLOAT conversion excluded as semantic owner? | C |
| 60 | `.5` and `1.` excluded as FLOAT tokens? | C |
| 61 | Exponent grammar exact? | C |
| 62 | DATE/TIMESTAMP typed literals absent? | C |
| 63 | TRUE/FALSE literal ownership correct? | C |
| 64 | NULL remains unresolved syntax? | C |
| 65 | Parser literal TypeId boundary explicit? | F |
| 66 | Closed operator token set defined? | F |
| 67 | Closed punctuation set defined? | F |
| 68 | Longest-match rule defined? | F |
| 69 | `<`, `<=`, `<>`, `>=` token overlaps defined? | F |
| 70 | `--` versus repeated minus deterministic? | F |
| 71 | Dot/decimal/qualification collision defined? | F |
| 72 | Semicolon token role defined? | F |
| 73 | Explicit end-of-input handling present? | C |
| 74 | Trailing tokens rejected? | F |
| 75 | Single statement versus batch defined? | F |
| 76 | Empty statements defined? | F |
| 77 | Trailing semicolon policy defined? | F |
| 78 | Batch recovery recognized later? | CS |
| 79 | Unknown punctuation must fail explicitly? | CS |
| 80 | Whitespace/comments discarded or represented? | F |
| 81 | Literal primary exists? | CS |
| 82 | Qualified-name production exact? | F |
| 83 | Parentheses override precedence? | C |
| 84 | Unary `+/-` precedence defined? | C |
| 85 | `* / %` precedence defined? | C |
| 86 | Binary `+ -` precedence defined? | C |
| 87 | Comparison level defined? | C |
| 88 | NOT relative precedence defined? | C |
| 89 | AND precedence defined? | C |
| 90 | OR precedence defined? | C |
| 91 | Arithmetic infix left-associative? | C |
| 92 | Boolean infix left-associative? | C |
| 93 | Prefix unary associativity defined? | F |
| 94 | Comparison chaining rejected? | C |
| 95 | `IS NULL` production exact? | F |
| 96 | `IS NOT NULL` production exact? | F |
| 97 | Expression-list IN production exact? | F |
| 98 | `NOT IN` parse exact? | F |
| 99 | EXISTS precedence exact? | F |
| 100 | NOT EXISTS precedence exact? | F |
| 101 | CAST syntax exact? | F |
| 102 | Searched CASE parser production exact? | F |
| 103 | Function-call syntax exact? | F |
| 104 | COUNT(*) parser representation exact? | F |
| 105 | SELECT/table star syntax exact? | F |
| 106 | Multiplication/star contexts disambiguated? | F |
| 107 | Parenthesized expression/subquery disambiguated? | F |
| 108 | Explicit/implicit alias grammar exact? | F |
| 109 | Alias-versus-clause ambiguity resolved? | F |
| 110 | Chapter-17 source order preserved by AST? | F |
| 111 | Top-level statement registry explicitly closed? | F |
| 112 | CREATE TABLE production complete? | F |
| 113 | CREATE INDEX production complete? | F |
| 114 | DROP productions complete? | F |
| 115 | INSERT production complete? | F |
| 116 | UPDATE production complete? | F |
| 117 | DELETE production complete? | F |
| 118 | SELECT production complete? | F |
| 119 | Transaction-control productions complete? | F |
| 120 | VACUUM production complete? | F |
| 121 | `ANALYZE table_name;` stated? | C |
| 122 | EXPLAIN productions complete? | F |
| 123 | FROM optionality exact? | C |
| 124 | Supported join kinds identified? | C |
| 125 | SELECT clause order stated? | C |
| 126 | Comma-list grammar exact? | F |
| 127 | Trailing-comma policy exact? | F |
| 128 | Empty-list policy exact? | F |
| 129 | No-FROM one-row semantics exact? | CS |
| 130 | Supported subquery classes listed? | C |
| 131 | Derived-table alias mandatory? | C |
| 132 | Subquery exclusions stated? | C |
| 133 | Duplicate clauses rejected by grammar? | F |
| 134 | VALUES row-arity owner explicit? | F |
| 135 | Duplicate UPDATE assignment preservation explicit? | F |
| 136 | AST represents syntax, not semantics? | C |
| 137 | Text names retained pre-bind? | C |
| 138 | TableId/ColumnId absent pre-bind? | C |
| 139 | TypeId prebinding rule explicit? | F |
| 140 | AST taxonomy complete rather than representative? | F |
| 141 | AST child/list ordering normative? | F |
| 142 | AST duplicate preservation normative? | F |
| 143 | Token spans half-open bytes? | C |
| 144 | Every AST node has a span? | C |
| 145 | Exact error-span selection deterministic? | F |
| 146 | Borrowed token/AST payload lifetime defined? | F |
| 147 | Partial AST observability defined? | F |
| 148 | Recovery freedom separated from accepted grammar? | CS |
| 149 | Resource exhaustion classified? | F |
| 150 | Parser independent of catalog? | C |
| 151 | Parser independent of TypeResolver? | C |
| 152 | Parse success distinguished from binding validity? | C |
| 153 | Unsupported syntax must fail explicitly? | C |
| 154 | Lexer/parser/bind error categories exist? | C |
| 155 | Unsupported-subquery error stage deterministic? | F |
| 156 | Locale-independent tokenization complete? | F |
| 157 | Signed-char independence complete? | F |
| 158 | Unicode-library independence complete? | F |
| 159 | Native integer conversion cannot own semantics? | C |
| 160 | Native FLOAT conversion cannot own semantics? | C |
| 161 | Nesting/resource-limit outcome defined? | F |
| 162 | AST is nonpersistent? | C |
| 163 | Arena mandated unnecessarily? | F |
| 164 | Recursive-descent/Pratt mandated unnecessarily? | F |
| 165 | External tools cannot redefine language? | C |
| 166 | Spans refer to original source bytes? | C |
| 167 | Comment/whitespace AST preservation requirement clear? | F |
| 168 | Query-batch arena consistent with batch grammar? | F |
| 169 | Binder ownership in Chapter 19 precise? | C |
| 170 | Chapter-17 registry remains authoritative? | C |

Result: 170 actual checks; 86 consistent/specialized, 83 findings, 1 N/A. Most failed checks collapse into the 15 findings below rather than representing 83 independent architecture decisions.

## Complete findings

### BLOCKING findings

#### F18-B1 — lexical input and whitespace domain absent

- Section: §§18.2–18.3, 18.7.
- Evidence: “source byte span”; “Initial token classes include…” with no input/whitespace contract.
- Type: `INPUT DOMAIN`.
- Scope: Cross-section.
- Explanation: The chapter does not define accepted source bytes, whitespace, line endings, or high-byte handling outside literals.
- Canonical comparison: §17.5.3 defines only string/NUL behavior.
- Consequence: Locale/platform-dependent tokenization and different accepted SQL.
- Owner: Chapter 18.
- Future action: **S. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED**.

#### F18-B2 — identifier lexical grammar absent

- Section: §18.4.
- Evidence: “Unquoted identifiers are normalized to lowercase”; “Quoted identifiers preserve exact spelling/case.”
- Type: `IDENTIFIER SEMANTICS`.
- Scope: Cross-section with Chapter 16.
- Explanation: Delimiters, escapes, start/continuation bytes, non-ASCII, NUL, and empty quoted identifiers are undefined.
- Canonical comparison: Chapter 16 consumes exact canonical bytes and requires nonempty catalog names.
- Consequence: Same SQL text may produce different canonical names or fail on one implementation.
- Owner: Chapter 18.
- Future action: **S**.

#### F18-B3 — keyword/reservation registry absent

- Section: §18.3.
- Evidence: “Keywords are case-insensitive. They may be recognized directly or through identifier-to-keyword lookup.”
- Type: `KEYWORD SEMANTICS`.
- Scope: Cross-section.
- Explanation: No closed keyword table or reserved/contextual classification exists.
- Consequence: A word can parse as a keyword, identifier, or alias depending on implementation.
- Owner: Chapter 18.
- Future action: **S**.

#### F18-B4 — operator/punctuation tokenization not closed

- Section: §§18.3, 18.7, 18.15.
- Evidence: generic token classes `operator` and `punctuation`.
- Type: `TOKENIZATION`.
- Scope: Cross-section.
- Explanation: Exact symbols and longest-match behavior are absent.
- Consequence: `<>`, `<=`, dots, stars, signs, and comment boundaries can tokenize differently.
- Owner: Chapter 18.
- Future action: **S**.

#### F18-B5 — statement grammar and input framing incomplete

- Section: §§18.10–18.11.
- Evidence: “The initial parser/binder surface includes…” followed primarily by statement names.
- Type: `STATEMENT GRAMMAR`.
- Scope: Cross-section with §21.17.
- Explanation: Most productions, semicolon policy, one-versus-many statement mode, empty statements, and trailing-token behavior are absent.
- Consequence: Identical input can be accepted, rejected, partially parsed, or treated as a batch.
- Owner: Chapter 18.
- Future action: **S**.

#### F18-B6 — expression grammar is not complete enough to derive one AST

- Section: §§18.12–18.15.
- Evidence: precedence row “comparison / IS NULL / IN” and “Representative AST kinds include”.
- Type: `EXPRESSION GRAMMAR`.
- Scope: Cross-section with Chapter 17.
- Explanation: CAST, CASE, expression-list IN, IS NOT NULL, calls, star, aliases, and primary forms lack productions.
- Consequence: `NOT IN`, `IS NOT NULL`, calls, and star forms can produce different ASTs.
- Owner: Chapter 18.
- Future action: **S**.

#### F18-B7 — direct-negative parenthesis distinction is not preserved contractually

- Section: §§18.6, 18.13, 18.16.
- Evidence: “`-2147483648` is parsed as unary minus applied to a positive numeric literal representation”; invariant says syntactically distinct only “where practical.”
- Type: `TYPE HANDOFF`.
- Scope: Cross-section with §17.5.1.
- Explanation: Chapter 17 requires parentheses to break the direct-negative form, but no grouping node, directness flag, or equivalent AST invariant is required.
- Consequence: `-9223372036854775808` and `-(9223372036854775808)` may become indistinguishable.
- Owner: Chapter 18’s syntax handoff; semantics remain Chapter 17.
- Future action: **S**.

#### F18-B8 — AST child order and duplicate preservation are undefined

- Section: §18.13.
- Evidence: “Representative AST kinds include…” with no child/list invariants.
- Type: `SOURCE ORDER`.
- Scope: Cross-section with §§17.7.3, 17.9.
- Explanation: Source order is observable for AND/OR, CASE, and IN; duplicate lists also reach later semantic validation.
- Consequence: Reordering or deduplication can suppress/introduce errors and alter semantics.
- Owner: Chapter 18.
- Future action: **S**.

### MAJOR findings

#### F18-M1 — unsupported subquery failure stage is implementation-dependent

- Section: §18.12.
- Evidence: “Those forms are rejected as syntax or during binding according to where recognition occurs.”
- Type: `ERROR SEMANTICS`.
- Scope: Cross-section.
- Explanation: The architecture explicitly leaves Lexer/Parser versus UnsupportedFeature/BindError classification variable.
- Consequence: Deterministic client error category and source-span tests cannot be selected.
- Owner: Chapter 18/Chapter 19 boundary.
- Future action: **O. PARSER/BINDER BOUNDARY CLARIFICATION**.

#### F18-M2 — token/AST payload lifetime is incomplete

- Section: §§18.2, 18.5, 18.8, 18.14.
- Evidence: decoded literal payloads and retained spans are required, but only AST node arena lifetime is described.
- Type: `AST OWNERSHIP`.
- Scope: Cross-section.
- Explanation: The lifetime of query source, borrowed slices, decoded strings, and identifier bytes is not defined.
- Consequence: A conforming-looking AST can contain dangling payload views.
- Owner: Chapter 18.
- Future action: **N. AST-OWNERSHIP CLARIFICATION**.

#### F18-M3 — front-end resource exhaustion has no error boundary

- Section: Chapter 18; absence across token length, nesting depth, and AST size.
- Evidence: no semantic limits or resource-failure rule.
- Type: `RESOURCE SEMANTICS`.
- Scope: Cross-section with §39.
- Explanation: Huge valid tokens or deeply nested syntax may exhaust memory/stack.
- Consequence: Implementations may misclassify resource exhaustion as syntax error or crash.
- Owner: Chapter 18 plus §39 resource classification.
- Future action: **P. ERROR/RESOURCE CLARIFICATION**.

### MINOR findings

#### F18-N1 — project-time language

- Sections: §§18.3, 18.10, 18.11, 18.15.
- Evidence: “Initial…”, “reserved for the future…”, “deferred initially”, “adding it later”.
- Type: `TEMPORALITY`.
- Scope: Cross-section.
- Consequence: The v1 contract reads as a roadmap rather than a canonical language definition.
- Owner: Chapter 18 wording.
- Future action: **B. TIMELESSNESS REWRITE**.

#### F18-N2 — parser/allocation implementation technique is overconstrained

- Sections: §§18.2, 18.9, 18.14, 18.16.
- Evidence: “handwritten recursive descent”, “Pratt parsing”, “arena ownership”, repeated-rescan prohibition.
- Type: `IMPLEMENTATION COUPLING`.
- Scope: Cross-section.
- Consequence: Conforming implementations are constrained beyond observable syntax/lifetime requirements.
- Correct owner: Parsing technique/sequencing belongs in DEVELOPMENT; Chapter 18 should retain only deterministic grammar, spans, and lifetime properties.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX**.

#### F18-N3 — no-FROM relational semantics are duplicated in the syntax chapter

- Section: §18.11.1.
- Evidence: detailed `LogicalValues`, aggregation, statistics, binder, and execution rules.
- Type: `DOCUMENT OWNERSHIP`.
- Scope: Cross-section with Chapters 17, 19, 20, 29.
- Consequence: Multiple owners must remain synchronized.
- Correct owner: Chapter 18 owns optional-FROM syntax; §§20.5/20.15 and semantic chapters own behavior.
- Future action: **R. REMOVE DUPLICATION**.

#### F18-N4 — diagnostic-span selection is subjective

- Section: §18.8.
- Evidence: “smallest useful available source span.”
- Type: `SOURCE SPAN`.
- Scope: Cross-section with §§21.16, 39.2, 41.4.
- Consequence: Exact negative fixtures cannot agree on which construct/token owns an error.
- Owner: Chapter 18/error-surface owner.
- Future action: **P. ERROR/RESOURCE CLARIFICATION**.

No editorial-only findings.

## Frozen architecture semantic questions

1. **Input domain:** Which bytes/encoding and whitespace classes constitute SQL source, and how are CR/LF/CRLF/high bytes handled?
2. **Identifiers:** What exact unquoted and quoted identifier grammars, delimiters, escapes, NUL rules, and case-fold algorithm apply?
3. **Keywords:** What is the closed reserved/contextual keyword registry?
4. **Token boundaries:** What is the exact operator/punctuation inventory and longest-match rule?
5. **Statement framing:** Is semicolon required or optional, what is the batch grammar, and how are empty/trailing statements treated?
6. **Expression grammar:** What exact productions and AST shapes govern CAST, CASE, IN/NOT IN, IS predicates, calls, star, aliases, and primaries?
7. **Direct negative:** What syntax-preserving AST property distinguishes direct unary minus from parenthesized positive magnitude?
8. **AST order:** Which child/list collections must preserve exact source order and duplicates?

Frozen constraints:

- Chapter 17 continues to own literal classification and direct-negative semantics.
- Chapter 16 continues to own canonical catalog-name bytes and TypeIds.
- Chapter 19 continues to own lookup/type resolution.
- §20.14 continues to own subquery semantics.
- No decision may introduce parser-side catalog lookup or type resolution.

## Verification cross-check and follow-up gaps

| Family | Existing coverage | Classification |
|---|---|---|
| Positive statement/AST shapes | Generic procedure exists | BLOCKED by incomplete grammar |
| Negative syntax/error spans | Procedure exists | BLOCKED by grammar/error-stage questions |
| Precedence/associativity | Good listed vectors | PARTIAL |
| Identifier canonicalization | Useful vectors | BLOCKED by missing identifier/keyword grammar |
| Strings | Strong deterministic coverage | COMPLETE |
| Comments | Good vectors | PARTIAL; line-ending/input domain missing |
| Numeric token handoff | Strong Chapter-17 coverage | PARTIAL; direct-parenthesis AST blocker |
| NULL/Boolean handoff | Strong Chapter-17 coverage | COMPLETE |
| Statement batches/recovery | §21.17 procedure exists | PARTIAL; framing grammar missing |
| AST ordering/duplicates | Generic list-order assertion | PARTIAL; architectural obligation absent |
| Parser/binder separation | Binder inspection exists | COMPLETE |
| Unsupported syntax | Negative framework exists | BLOCKED by parse-vs-bind decision |
| Resource/fuzz robustness | Fuzzing exists | PARTIAL; resource error oracle missing |
| Source spans | Half-open assertions exist | PARTIAL; blame-span rule subjective |

Follow-up verification should reuse:

- Chapter-17 literal and direct-negative fixtures;
- §20.14 subquery support matrix;
- Chapter-19 binder/name tests;
- §21.17 batch recovery harness;
- existing deterministic no-crash front-end fuzzing.

Verification synchronization should occur only after the eight semantic questions are resolved.

## Previous-chapter regression

- Chapter 15: no statement-write or transaction semantics are contradicted.
- Chapter 16: textual names remain binder-owned; TypeIds are not renumbered.
- Chapter 17: literal/operator semantics remain authoritative, but direct-negative syntax preservation is incomplete.
- SQL NULL remains untyped at the syntax boundary.
- SQL UNKNOWN is not confused with parser or unresolved-type state.
- Parser acceptance does not expand the Chapter-17 scalar registry.
- Chapter 17 remains frozen; no prior architecture defect was reopened.

## Implementer-invention assessment

Correctly determined without invention:

- parser/binder layering;
- byte-offset span model;
- string decoding;
- numeric token grammar;
- sign separation;
- broad statement inventory;
- SELECT clause order;
- no-FROM semantics;
- supported subquery classes;
- broad precedence order;
- comparison non-chaining;
- textual-name AST boundary.

Still requires correctness-relevant invention:

- input and whitespace;
- identifier/keyword grammar;
- token longest match;
- statement framing and most productions;
- several expression productions;
- AST ordering and complete taxonomy;
- direct-negative parenthesis preservation;
- payload lifetime;
- error-stage and resource classification.

Therefore Chapter 18 cannot yet stand as independently implementable canonical v1 syntax architecture.

## Direct answers

| Question | Answer |
|---|---|
| Project-time/current-state wording? | Yes: project-time wording; no implementation-state narration |
| DEVELOPMENT-owned material? | Yes: handwritten/RD/Pratt/arena technique |
| VERIFICATION recipe? | No |
| PROJECT_STATE material? | No |
| History/devlog material? | No |
| Lexical ambiguity? | Yes |
| Identifier ambiguity? | Yes |
| Literal ambiguity? | Numeric grammar is clear; direct-negative AST handoff is ambiguous |
| Direct-negative ambiguity? | Yes |
| Precedence/associativity ambiguity? | Yes, for composite predicates, unary forms, and primaries |
| Statement-grammar ambiguity? | Yes |
| Parser/binder ownership ambiguity? | Mostly clear; unsupported-subquery error stage is ambiguous |
| AST/source-order ambiguity? | Yes |
| Error-boundary ambiguity? | Yes |
| Resource-vs-syntax ambiguity? | Yes |
| Platform-determinism ambiguity? | Yes |
| Implementation-technique overconstraint? | Yes |
| Correctness-relevant implementer invention? | Yes |
| Timeless canonical v1 chapter? | Not yet |

## Recommended next action

**Frozen semantic architecture review required.**

Resolve the eight questions with the smallest possible Chapter-18 architecture revision, then perform targeted document-model cleanup and Chapter-18 verification synchronization.

Recommended future Chapter-19 review scope, not started here:

- binder scopes and BindingId;
- textual-name resolution;
- wildcard/output-name behavior;
- bound-expression IR;
- operator/function/aggregate registries;
- GROUP/HAVING/ORDER/LIMIT semantics;
- CASE/IN/subquery binding;
- parameter/unresolved-type terminology;
- binder invariants and Chapter-18 raw-AST handoff.

## Final repository checks

| Check | Initial | Final |
|---|---|---|
| HEAD | `b7cfadf554e929bc59d591f123c556cf91423fde` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | N/A at start | passed, no output |

- Files modified by audit: **NONE**
- Repository-state change caused by audit: **NONE**
- No review artifact was read, modified, created, or staged.
- No documentation, source, tests, benchmarks, or build files were modified.
- No build, test, benchmark, formatting, staging, or commit occurred.
- No implementation work or scaffolding occurred.
- Chapter 19 direct review: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**.
