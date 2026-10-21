# Catalog, SQL, and Logical-Plan Verification

## Chapter 18 Lexer, Parser, and Raw-AST Verification

This section is the complete procedural owner for the closed lexical, syntactic, raw-AST,
lifetime, and front-end resource contracts in
[`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 18. Chapters 16, 17, 19, 20, 21, and
29 and §§39.1–39.3 remain authoritative for the semantic handoffs identified below. A
parser success proves membership in the Chapter-18 language only; it is not an object,
type, function, subquery-shape, or execution-success oracle.

### Deterministic front-end harness and oracle discipline

Use a byte-oriented fixture driver with deterministic barriers and event capture. No
correctness result may depend on sleeps, wall-clock deadlines, source API names, parser
algorithm, allocator layout, or production lexer/parser code serving as its own oracle.
The observable event vocabulary is conceptual:

```text
lexer:
    source offset before token; token start/end; token class; token payload;
    lexical-mode and comment entry/exit; LexerError; EOF

parser:
    token demanded; production entered/completed; raw node created;
    ordered child appended; optional clause present/absent; statement boundary;
    semicolon consumed; EOF consumed; ParserError

binder handoff:
    canonical identifier/type-name bytes; literal provenance; raw tree; SourceSpan

lifetime:
    backing retained; payload borrowed/materialized; token/raw AST destroyed;
    surviving consumer observation

resource:
    budget admitted/refused; allocation failed; private state unwound;
    complete result or structured failure published
```

The test driver records these events without requiring them to be exposed by production
interfaces. Deterministic test adapters, fault points, ownership sentinels, or equivalent
debug instrumentation may provide the observations. A fixture changes one relevant
condition at a time; malformed inputs are compared with a nearest valid control.

Independent oracles are:

| Domain | Independent oracle |
|---|---|
| source bytes | explicit 256-entry state-dependent byte partition derived from §§18.2–18.7 |
| whitespace/comments | six-byte table plus a small state machine for line/block/string/quoted modes |
| keywords | exact registry table plus ASCII-only case folding and whole-identifier boundary checks |
| symbols | exact trie/table of registered spellings with exhaustive prefix-overlap comparison |
| identifiers/strings | independent byte decoders implementing only the displayed Chapter-18 grammars |
| numeric tokens | declarative §§17.5.1–17.5.2 recognizers that return spans and exact spelling, never host numeric conversion |
| spans | integer interval arithmetic over the original byte vector and §18.8's selection rules |
| request/statement/expression grammar | declarative EBNF-derived acceptance tables and expected conceptual trees, separate from production parsing code |
| raw AST | canonical structural serialization containing node roles, ordered children, multiplicity, payload, and spans but no source class names |
| lifetime | ownership graph plus generation-tagged backing sentinels; every read must target a live generation |
| resources | injected guard/allocation event determines category independently of source validity |
| unsupported syntax | complement matrix of expressly absent productions, not another SQL implementation |

A reference database may supplement semantic differential tests where dialects overlap,
but it is never the oracle for Chapter-18 lexical boundaries or unsupported syntax.

### Source-byte, whitespace, and comment procedures

For each of the 256 byte values, exercise every lexical state in which that byte can be
decisive: structural, line comment, block comment, string, and quoted identifier. In
structural state classify it as exact whitespace, ASCII identifier start/continuation,
digit, delimiter, comment/operator candidate, registered punctuation/operator, or invalid.
Prove that whole-source UTF-8 validity is unnecessary, high bytes outside admitted payload
states fail deterministically, unquoted NUL fails, and embedded string NUL remains an
ordinary byte under §17.5.3. Repeat the classification oracle under representative locales
and both signed- and unsigned-`char` build modes or an equivalent byte-domain simulation.

The whitespace oracle contains exactly `09 0A 0B 0C 0D 20`; exhaustively assert that the
other 250 byte values are not whitespace. Place each legal byte between identifiers,
keywords, numeric tokens, symbols, and statements. Trivia separates tokens where needed
and never concatenates bytes around it.

The comment state-machine matrix is:

| Source family | Expected lexical behavior |
|---|---|
| `--x`, `--x\n`, `--x\r`, `--x\r\n` | EOF or first CR/LF terminates legally; terminator remains whitespace |
| `a--x\nb`, `--/*`, `--'` | opener wins over `-`; payload is uninterpreted until terminator |
| `/**/`, `/*x*/` | first `*/` closes |
| `/* /* */` | inner opener has no nesting significance |
| `/*`, `/*x` | `LexerError`, opening delimiter through EOF |
| `/* */ */` | comment closes, then unregistered structural suffix is diagnosed normally |
| `a/*b*/c`, `a / * b` | comment/no-comment token boundaries follow exact bytes |
| strings/quoted identifiers containing `--`, `/*`, or `*/` | marker bytes are payload, not comments |

Comment openers are tested before their component `-` and `/` tokens. String and quoted
identifier delimiters are tested before comment/symbol interpretation of enclosed bytes.
No nested-comment expectation is permitted.

### Token registry, identifier, string, and numeric procedures

The exact reserved-keyword oracle is:

```text
ANALYZE AND AS ASC BEGIN BY CASE CAST COMMIT CREATE CROSS DEFAULT DELETE DESC
DISTINCT DROP ELSE END EXISTS EXPLAIN FALSE FROM GROUP HAVING IN INDEX INNER
INSERT INTO IS JOIN KEY LEFT LIMIT NOT NULL OFFSET ON OR ORDER PRIMARY RETURNING
ROLLBACK SELECT SET TABLE THEN TRUE UNIQUE UPDATE VACUUM VALUES WHEN WHERE
```

For every entry, test lowercase, uppercase, and mixed ASCII case; all map to the same
reserved token. Test `Kx`, `_K`, and `K1` for every `K` where the identifier grammar
admits the bytes; these are identifiers, not keyword prefixes. An unquoted keyword is not
an identifier, a quoted keyword is one, no parser state makes a contextual keyword, and a
representative complement of other-dialect words remains an identifier or invalid solely
under the ordinary grammar. Maintain a keyword-to-production coverage table so every
registered word is exercised in at least one admitted role.

| Keyword(s) | Admitted production role exercised |
|---|---|
| CREATE, TABLE | CREATE TABLE |
| CREATE, INDEX, UNIQUE, ON | CREATE INDEX / CREATE UNIQUE INDEX |
| DROP, TABLE, INDEX | DROP TABLE / DROP INDEX |
| PRIMARY, KEY, UNIQUE, NOT, NULL, DEFAULT | CREATE TABLE elements and constraints |
| INSERT, INTO, VALUES | INSERT sources |
| UPDATE, SET | UPDATE assignments |
| DELETE, FROM | DELETE target |
| RETURNING, AS | DML RETURNING item and explicit alias |
| SELECT, DISTINCT | SELECT header |
| FROM, WHERE, GROUP, BY, HAVING | SELECT relational clauses |
| ORDER, BY, ASC, DESC, LIMIT, OFFSET | SELECT tail clauses |
| INNER, LEFT, CROSS, JOIN, ON | exact join tails |
| BEGIN, COMMIT, ROLLBACK | transaction control |
| VACUUM, ANALYZE | maintenance statements |
| EXPLAIN, ANALYZE | EXPLAIN and EXPLAIN ANALYZE |
| AND, OR, NOT | Boolean expression levels |
| IS, NOT, NULL, IN | null and membership predicates |
| CAST, AS | CAST expression |
| CASE, WHEN, THEN, ELSE, END | searched CASE |
| EXISTS | EXISTS subquery primary |
| TRUE, FALSE, NULL | literal primaries |

The exact symbolic oracle is:

```text
( ) , . ; + - * / % = <> < <= > >=
```

Enumerate each token, each proper prefix, every registered overlap, and adjacent symbol
pairs. Longest registered spelling wins, so `<=`, `<>`, and `>=` are not split. `!=`,
`||`, `::`, `?`, `$`, `:`, brackets, and every other unregistered symbol sequence produce
`LexerError` rather than a generic operator token.

Unquoted identifiers are checked against `[A-Za-z_][A-Za-z0-9_]*` with exhaustive ASCII
start/continuation partitions. Boundary rows include `a`, `Z`, `_`, `_0`, `a9`, `A_B`,
`0a`, punctuation, high-byte start/continuation, NUL, and empty input. Canonicalization is
exhaustive over admitted bytes: `A`–`Z` map to `a`–`z`; lowercase, digits, and `_` are
unchanged; no locale or Unicode fold participates.

Quoted identifiers use an independent doubled-double-quote decoder. Verify `"Foo"`,
`"foo"`, `"a""b"`, quoted keywords, every representative high byte, NUL rejection,
decoded-empty rejection for `""`, and EOF before the closer. Payload bytes/case and the
quoted-source distinction survive in the raw AST.

Strings use an independent doubled-single-quote decoder. Cover empty/plain literals,
`'It''s'`, literal backslash bytes, the two bytes backslash-plus-`n`, comment markers,
semicolon, high bytes, embedded NUL, and EOF before the closer. Assert exact decoded bytes
and original source span; no backslash escape or Unicode transformation is allowed.

Numeric tokenization composes with the Chapter-17 type/value procedures but independently
proves lexical boundaries. Integer tokens are `DIGIT+`; floating tokens are exactly
`DIGIT+'.'DIGIT+([eE][+-]?DIGIT+)?` or `DIGIT+[eE][+-]?DIGIT+`. Cover leading zeros,
INT32/INT64 boundaries, exact magnitude `2^63`, larger magnitudes, decimal/exponent forms,
malformed exponents, adjacency to identifiers/symbols, and complete-token consumption.
The sign is always a separate `+` or `-` token. No `strto*`-like conversion is an oracle.

The numeric/dot matrix includes `1.2`, `1.`, `.5`, `1..2`, `a.b`, `a.1`, and `1.a`.
Expected tokens/errors come from the closed number and symbol grammars: `.` never begins a
numeric token, and a host numeric parser cannot extend or accept a token.

Direct-negative composition compares these raw trees:

```text
-2147483648                 UnaryMinus(NumericLiteral("2147483648"))
-(2147483648)               UnaryMinus(ParenthesizedExpression(NumericLiteral(...)))
-9223372036854775808        UnaryMinus(NumericLiteral("9223372036854775808"))
-(9223372036854775808)      UnaryMinus(ParenthesizedExpression(NumericLiteral(...)))
```

The parser test asserts only exact provenance and lossless magnitude; Chapter 17 is the
classification oracle.

### SourceSpan and diagnostic-selection procedures

For every token and raw node assert `0 <= start <= end <= source_length` and that
`[start,end)` indexes the original request bytes. Use legal high bytes inside strings and
quoted identifiers to prove byte rather than code-point addressing. Compute all expected
intervals from fixture byte-vector indices, independently of production spans.

Diagnostic selection follows §18.8 exactly:

| Condition | Canonical selected span |
|---|---|
| invalid source byte | smallest byte range causing lexical rejection |
| unrecognized symbolic candidate | smallest candidate range causing lexical rejection |
| unterminated string/quoted identifier/block comment | opening delimiter through EOF |
| unexpected existing token | that token's span |
| required input missing at EOF | `[source_length,source_length)` |
| bind/type failure | attached span of the syntactic construct whose resolution directly failed |
| equally specific represented candidates | earliest span in source order |

Exercise missing `)`, missing CASE `END`, missing expression after an operator, missing
table name or SELECT item, and dangling comma at EOF. Handoff-only fixtures retain spans
for unknown column, unknown type name, unresolved object, and unsupported cast without
retesting Chapter-19/17 semantics. A synthetic oracle fixture may establish the tie rule
when no natural single-error case presents two equally specific spans.

A numeric `SourceSpan` value may remain after source release. An attempted text extraction
then succeeds only when the source owner was retained or the text was materialized. This
test distinguishes interval-value lifetime from source-byte lifetime and does not require
execution to retain SQL text.

### Request framing and complete statement dispatch

The request-framing table is normative for parser verification:

| Source shape | Result |
|---|---|
| `stmt`, `stmt;` | one complete statement |
| `stmt;stmt`, `stmt;stmt;` | two source-ordered statements |
| surrounding whitespace/comments | same statement sequence and original spans |
| empty, whitespace-only, comment-only | `ParserError` for zero statements |
| `;`, `;;`, `;stmt`, `stmt;;`, `stmt; ;stmt` | `ParserError`; no empty statement |
| valid prefix plus trailing nongrammar token | `ParserError`; no ignored suffix |

Success consumes EOF and preserves exact statement order. Error recovery under §18.10.1 may
collect later independent diagnostics after semicolon synchronization, but it MUST NOT
publish the valid prefix or any recovery artifact as a successful bindable request.

Run one minimal accepted source and one extension rejection for every dispatch root:

```text
CREATE TABLE                 CREATE INDEX / CREATE UNIQUE INDEX
DROP TABLE / DROP INDEX      INSERT      UPDATE      DELETE      SELECT
BEGIN                        COMMIT      ROLLBACK
VACUUM table                 ANALYZE table
EXPLAIN SELECT               EXPLAIN ANALYZE SELECT
```

An unsupported statement start never becomes a generic statement node.

### DDL, DML, and list procedures

CREATE TABLE fixtures cover one/multiple columns; `NOT NULL`; `DEFAULT expr`; fixed
`NOT NULL DEFAULT` order; table PRIMARY KEY/UNIQUE with one/composite columns; and mixed
column/constraint order. Reject empty elements, every trailing comma, explicit column
`NULL`, column-level PRIMARY KEY/UNIQUE, DEFAULT before NOT NULL, named constraints,
CHECK, FOREIGN KEY, generated/identity columns, temporary/`IF NOT EXISTS` forms, and table
options. Preserve element and constraint-column order and duplicates for DDL binding.

CREATE INDEX fixtures cover ordinary/UNIQUE forms, one/multiple base columns, source order,
and duplicate preservation. Reject empty/trailing lists, expressions, ASC/DESC, INCLUDE,
partial predicates, and `IF NOT EXISTS`. DROP accepts exactly one TABLE or INDEX object and
rejects `IF EXISTS`, CASCADE, RESTRICT, and multiple names.

INSERT fixtures cover optional nonempty target columns, one/multiple nonempty VALUES rows,
INSERT SELECT, and optional RETURNING. UPDATE covers a nonempty assignment list, optional
WHERE/RETURNING, and duplicate assignments. DELETE covers exact `DELETE FROM`, optional
WHERE/RETURNING. Reject UPDATE/DELETE aliases, UPDATE FROM, tuple assignment, DELETE USING,
DML ORDER BY/LIMIT, all DML trailing commas, `DEFAULT VALUES`, `VALUES(DEFAULT)`, and
`SET c=DEFAULT`. Column-definition DEFAULT remains accepted.

RETURNING accepts a nonempty source-ordered list of `expression [AS alias]` only on INSERT,
UPDATE, and DELETE. Reject unqualified/qualified star, implicit aliases, trailing comma,
and RETURNING on SELECT, DDL, transaction, or maintenance statements.

For every comma-separated family, execute the list-cardinality matrix below. Explicit
`f()` is a separate zero-argument production, not an empty required list.

### SELECT, aliases, joins, and LIMIT/OFFSET procedures

Generate SELECT cases from the exact clause sequence `SELECT [DISTINCT] projection [FROM]
[WHERE] [GROUP BY] [HAVING] [ORDER BY] [LIMIT] [OFFSET]`. Exercise each optional clause
alone where legal and in complete composition. Reject representative reorderings,
duplicates, and trailing commas. No-FROM fixtures verify syntax—including nested SELECT—
and delegate wildcard/binding/logical meaning to §§19.5, 20.5, 20.14, and 20.15.

Projection accepts `*`, `identifier.*`, expression, and `expression AS alias`; it is
nonempty. Base-table aliases require AS. Derived `(SELECT ...) [AS] alias` requires the
alias while permitting AS to be absent. Reject implicit projection/base aliases,
missing derived aliases, derived-column alias lists, and qualified star beyond one
identifier.

Join fixtures admit exactly INNER/LEFT with one ON expression and CROSS without ON. Reject
bare/comma/RIGHT/FULL/NATURAL/USING/LATERAL joins, CROSS with ON, missing ON, and
parenthesized join trees. For `a INNER JOIN b ON p LEFT JOIN c ON q`, compare against
`LeftJoin(InnerJoin(a,b,p),c,q)` or an equivalent canonical serialization; no flattening or
right association may lose the required tree.

GROUP BY and ORDER BY lists are nonempty, ordered, duplicate-preserving, and reject trailing
commas. ORDER items accept optional ASC/DESC and reject NULLS FIRST/LAST. LIMIT and OFFSET
accept full expressions, including `LIMIT 1+1` and OFFSET without LIMIT. Structurally valid
but nonintegral, negative, nonconstant, or subquery operands reach §§19.14/20.14.9 instead
of parser rejection.

Transaction controls accept only BEGIN, COMMIT, and ROLLBACK. VACUUM and ANALYZE require
one table target and no options/columns. EXPLAIN and EXPLAIN ANALYZE accept SELECT only;
nested DML, DDL, transaction, and maintenance statements are rejected structurally.

### Expression, function, star, and subquery procedures

Use an independent expected-tree table for the exact low-to-high levels OR, AND, prefix
NOT, one nonchainable predicate suffix, additive, multiplicative, unary signs, and primary.
Repetition levels are left-associated; prefix levels nest toward their operand; parentheses
override and remain represented in the raw AST.

Mandatory precedence rows include:

| Source | Canonical grouping |
|---|---|
| `a OR b AND c` | `OR(a,AND(b,c))` |
| `NOT a = b` | `NOT(EQ(a,b))` |
| `a+b*c` | `ADD(a,MUL(b,c))` |
| `a*b+c` | `ADD(MUL(a,b),c)` |
| `-a*b` | `MUL(NEG(a),b)` |
| `-(a+b)` | `NEG(Paren(ADD(a,b)))` |
| `a=b AND c=d` | `AND(EQ(a,b),EQ(c,d))` |
| `a IN (x) OR b` | `OR(IN(a,[x]),b)` |
| `a IS NULL AND b` | `AND(IS_NULL(a),b)` |

Associativity rows include `a-b-c`, `a/b*c`, `a+b-c`, repeated AND/OR, `NOT NOT a`, and
`- - x`. Reject `a<b<c`, `a=b=c`, and `a IS NULL IS NULL`. Assert `NOT a IN (x)` is
`NOT(IN(a,[x]))`, `a NOT IN (x)` is one NOT-IN predicate, and `a IS NOT NULL` is one null
predicate.

IN expression lists are nonempty, no-trailing, ordered, and multiplicity-preserving;
reject `IN ()`, `IN (1,)`, and row constructors. CAST accepts exactly
`CAST(expression AS unquoted-type-name)`; reject `::`, quoted and parameterized types.
Searched CASE requires at least one ordered WHEN/THEN arm and optional ELSE; reject simple
CASE, missing WHEN/THEN/END. Column references admit one or two identifiers, never three.

Generic calls parse independently of registries as `f()`, `f(expr,...)`, or `f(*)`.
Compare `count(*)`, `sum(*)`, and `foo(*)` to the same star-argument syntax role and defer
validity to Chapters 17/29. Exercise an ordinary quoted identifier such as `"f"(*)` and
reject qualified `ns.f()`; no production lookup may special-case a name. Distinguish raw
roles for `SELECT *`, `t.*`, `f(*)`, and `a*b`.

Subquery syntax admits scalar `(SELECT ...)`, `EXISTS (SELECT ...)`, IN SELECT, and derived
SELECT. Prefix NOT over EXISTS and the NOT-IN spelling compose through the ordinary
expression grammar rather than separate subquery statement productions. A correlated but
structurally valid SELECT, a multi-column scalar SELECT, and an IN SELECT with bad output
shape/type all parse and reach Chapters 19/20. Parenthesized INSERT, UPDATE, DELETE, row
constructors, and other absent wrappers fail with `ParserError` because only SELECT
occupies a structural subquery position. Parser fixtures use no catalog, scope,
output-schema, or type information.

### Raw-AST contract and parser/binder boundary

Serialize every raw node into architecture-visible roles rather than implementation class
names. The required category inventory is:

```text
StatementRequest; SelectStatement; CreateTableStatement; CreateIndexStatement;
DropStatement; InsertStatement; UpdateStatement; DeleteStatement;
TransactionStatement; VacuumStatement; AnalyzeStatement; ExplainStatement;
AstIdentifier; AstQualifiedName; AstTypeName; AstLiteral; AstBinaryExpression;
AstUnaryExpression; AstParenthesizedExpression; AstComparison; AstNullPredicate;
AstInList; AstInSubquery; AstFunctionCall; AstStarArgumentCall; AstCast; AstCase;
AstStar; AstSubqueryExpression; AstTableReference; AstJoin; AstReturningClause
```

Audit metadata to prove absence of TableId, ColumnId, IndexId, ConstraintId, BindingId,
resolved TypeId, catalog descriptors, inserted semantic casts, and logical/physical plan
data. Textual/canonical names, optional qualification, explicit aliases, literal
provenance, clause presence, spans, parenthesis provenance, join kind/children/ON, and
star roles remain recoverable.

Use position-tagged duplicate fixtures for statements, binary operands, CASE arms, IN
items, projections, VALUES rows/items, INSERT columns, UPDATE assignments, GROUP/ORDER BY,
function arguments, table elements/constraint columns, index keys, RETURNING items,
qualified components, and join children. Compare the complete ordered sequence; never use
a set or sorted representation as oracle.

The parser/binder boundary matrix below proves that type/object/function/subquery semantic
validity does not affect parse results. In particular, `Potato` is a legal unquoted
type-name syntax payload and `"INT32"` is a parser error. Exercise BOOLEAN, INT32, INT64,
FLOAT64, DATE, TIMESTAMP, and VARCHAR in mixed ASCII case, plus unknown names, to prove that
the parser admits the identifier shape while TypeResolver alone applies the seven-name
registry. `name` and `main.name` parse, `a.b.c` does not; the binder alone validates
namespace `main`. Raw type names contain canonical bytes and span, never TypeId.

### Backing, payload, and object-lifetime procedures

Use generation-tagged backing owners and deterministic destruction events. A read through
a borrowed view after its generation is dead is a test failure; the fixture must not
perform an actual dangling dereference. Required cases are:

| Case | Ownership transition | Required observation |
|---|---|---|
| A | token borrows source; AST materializes; token dies; source later dies | AST payload remains exact |
| B | AST borrows source; token dies; source owner retained | AST remains exact until release |
| C | bound object borrows source; raw AST dies; source retained | bound payload/diagnostic remains exact |
| D | binder materializes; source and raw AST die | bound object remains exact |
| E | SourceSpan value retained; source dies | interval remains usable; text dereference is unavailable unless materialized |

Repeat across decoded string bytes, decoded quoted identifiers, canonical unquoted names,
type/object/column/alias/function names, and numeric provenance including magnitude `2^63`.
Destroy tokens before raw-AST consumers and destroy raw AST before eligible bound consumers
to prove that object lifetime and payload lifetime are distinct. Exercise success, lexical
failure, parse failure, bind failure, cancellation, and resource failure cleanup. Backing
is runtime-only; no fixture expects WAL, catalog, tuple/index, checkpoint, or recovery state.

### Front-end resources, complete-result boundary, and transaction composition

First prove with the grammar oracle that no implementation-convenience maximum is part of
the language for request/token/string/identifier length, nesting/depth, CASE/IN/projection/
VALUES/statement counts, or raw-AST nodes. Existing semantic/domain bounds from other
owners remain separate.

Inject deliberate admission refusal at deterministic nesting, node, parser-work, and
front-end memory guards without causing allocation failure. The result is exactly
`FrontEndResourceLimit`. Inject required allocation failures during token payload creation,
string/identifier materialization, raw-node construction, numeric provenance retention,
and binder materialization; the result is exactly `OutOfMemory`. A guard reached before a
failed allocation dominates as `FrontEndResourceLimit`; an unsatisfied required allocation
is `OutOfMemory`. Neither is LexerError, ParserError, BindError, TypeError, nor a generic
resource error.

Run one grammar-valid input under sufficient resources and under a deterministically tiny
guard. The grammar oracle returns valid in both; only processing outcome differs. Deep
valid nesting must either succeed or return one structured resource outcome before stack
overflow, abort, memory corruption, uncaught allocation failure, or undefined behavior;
the test does not prescribe recursion or iteration.

Inject failure after private tokens/nodes exist. Assert no successful token stream, raw
AST, bindable prefix, or executable prefix is published; all private ownership unwinds.
Under each injection prove there is no token/string/identifier truncation, ignored suffix,
dropped/reordered/deduplicated child, shortened list, weakened numeric provenance,
literal approximation/type change, or prefix-only bind/execute fallback.

Chapter 18 contributes the structured cause before transaction-owned write publication;
§39.1 independently determines the command/transaction outcome. No fixture assigns a
parser-specific rollback/retry rule or persistent mutation. Resource policy and failed
front-end state leave no WAL, catalog, control-file, tuple/page/index, checkpoint,
transaction-status, or recovery representation.

### Platform, technique, and closed-language procedures

Replay lexical/AST fixtures under representative locales, char signedness modes, and
Unicode-library versions or equivalent isolated classification adapters. Tokenization,
canonical payload, spans, and accept/reject results are identical. Replay numeric fixtures
with deliberately disagreeing host integer/FLOAT conversion adapters; token boundaries and
Chapter-17 handoff remain oracle-defined.

Where two parser implementations are available—or through one parser plus an independent
declarative recognizer—compare token sequence, accept/reject result, and canonical raw-tree
serialization. Generated, handwritten, and other deterministic techniques are all
conforming possibilities; generator conflict resolution or recovery defaults cannot alter
the language.

Build the unsupported complement from explicit Chapter-18 exclusions. It includes, where
applicable: ALTER TABLE, MERGE, UPSERT/ON CONFLICT, WITH/CTE, UNION/INTERSECT/EXCEPT,
RIGHT/FULL/NATURAL/USING/LATERAL joins, window/FILTER/OVER syntax, CHECK, FOREIGN KEY,
named constraints, parameters, VARCHAR(n), DECIMAL, INTERVAL, JSON, ARRAY, typed DATE/
TIMESTAMP literals, UPDATE FROM, DELETE USING, RETURNING outside DML, `!=`, `||`, and `::`.
Each fixture has one nearest valid control and one absent production; expected result is
LexerError for absent symbols/bytes or ParserError for absent structural productions.

### Mandatory verification matrices

The following matrices are maintained as executable table data or an equivalent generated
fixture inventory. Each cell is asserted; blank or implementation-selected cells are not
permitted.

| Matrix | Required rows | Required columns |
|---|---|---|
| lexical boundary | six whitespace bytes, keyword/identifier prefixes, quotes, comments, symbol overlaps, numeric/dot, semicolon, EOF, high byte, NUL | source bytes, tokens, payload, span, result/category, oracle |
| identifier | simple/mixed ASCII, underscore, continuation digit, digit-first, keyword, quoted keyword, empty/doubled-quote/high-byte quoted, high-byte unquoted, NUL | lexical result, canonical payload, token class, raw role, error |
| string | empty, plain, doubled quote, backslash, comment marker, semicolon, high byte, NUL, unterminated | tokens, decoded bytes, span, error |
| numeric | zero, INT32/INT64 boundaries, `2^63`, decimal/exponent FLOAT, malformed exponent, `.5`, `1.`, `1..2`, signs | tokens, raw payload, raw AST, Chapter-17 handoff, result owner |
| statement | every supported root | minimal form, options, required lists, alias policy, trailing comma, root role, semantic owner, excluded extensions |
| list cardinality | CREATE elements/PK/UNIQUE/index keys/INSERT columns/VALUES rows/items/UPDATE/SELECT/GROUP/ORDER/RETURNING/function args/IN/CASE arms | minimum, empty legality, trailing comma, order, duplicates, later validation |
| alias | projection, base table, derived table | AS required/optional, alias required, implicit allowed, raw payload, binder owner |
| join | INNER, LEFT, CROSS, bare, comma, RIGHT, FULL, NATURAL, USING | status, ON rule, raw kind, association, result |
| star role | `SELECT *`, `t.*`, `f(*)`, `a*b` | tokens, context, raw role, downstream owner |
| subquery | scalar/EXISTS/IN/derived/correlated/multi-column scalar/bad-IN-shape/INSERT/UPDATE/DELETE/row constructor | parser result, binder owner, semantic owner, error stage |
| SourceSpan | valid token, invalid byte, three unterminated constructs, unexpected token, missing-at-EOF, unknown column/type handoff, tie | selected span, start, end, owner, rule |
| lifetime | source, token, raw AST, bound object, SourceSpan, decoded string, identifiers, numeric provenance | borrow, own, owner, outlive source, materialize, runtime-only |
| resource | invalid source, invalid grammar, configured guard, allocation failure, spill failure, optimizer bound | category, grammar validity, partial result, DB mutation, transaction owner, persistence |
| parser/binder | table/column existence, namespace `main`, type registry, function existence/arity/star, operator/cast legality, alias, duplicate target, constraint/default, LIMIT, correlation/output shape | parse owner, bind/type owner, later owner, expected stage |

The compact executable tables below fix the architecture-visible cells. Implementations may
add representation-specific observation columns, but may not replace these oracles.

#### Lexical, identifier, string, and numeric matrices

| Lexical row | Source bytes | Tokenization/payload | Span/result | Oracle |
|---|---|---|---|---|
| whitespace | each of `09 0A 0B 0C 0D 20` | trivia separator | exact one-byte interval; accepted | six-byte table |
| non-whitespace complement | each other byte in structural state | grammar-specific class or invalid, never whitespace | byte-specific | 256-entry partition |
| keyword boundary | `SELECT`, `selectx`, `_select`, `select1` | keyword then three identifiers | whole spelling | keyword table plus identifier grammar |
| quotes | valid/unterminated single and double quote forms | string or quoted-identifier mode | exact token or LexerError | independent decoders |
| line comment | `--x`, CR, LF, CRLF, EOF variants | trivia through before terminator | next token begins after whitespace | comment state machine |
| block comment | terminated, nested-looking, unterminated | first closer; no nesting | accepted or opener-through-EOF error | comment state machine |
| comment/operator | `a--b`, `a/*b*/c`, `a / * b` | opener priority or three ordinary symbols | exact component spans | symbol/comment priority |
| symbol overlap | `<`, `<=`, `<>`, `>`, `>=` | longest registered token | whole token | exact symbol trie |
| numeric/dot | `1.2`, `1.`, `.5`, `1..2` | declarative numeric/punctuation result | exact consumed prefix/error | Chapter-17 recognizer |
| qualified name | `a.b`, `a.1`, `1.a` | identifier/dot or numeric/error sequence | exact components | identifier plus numeric grammar |
| semicolon | single/final/repeated placements | punctuation then request framing | §18.10.1 result | request EBNF |
| EOF | after complete/incomplete construct | EOF or lexical/parser error | zero-width only for missing parser input | lexical state plus §18.8 |
| high byte | structural, quoted, and string states | invalid, exact identifier payload, exact string payload | byte interval or containing token | state-dependent byte table |
| NUL | structural/comment/quoted/string states | invalid except exact string payload | LexerError or string token | §§18.2, 17.5.3 |

| Identifier row | Lexical result | Canonical payload | Token/raw role | Error |
|---|---|---|---|---|
| `a`, `Z` | unquoted identifier | `a`, `z` | textual identifier | none |
| `_`, `_0`, `A_B9` | unquoted identifier | exact lowercase mapping | textual identifier | none |
| `0a` | integer then identifier or grammar rejection by context | separate payloads | not one identifier | contextual ParserError where invalid |
| reserved word | keyword | keyword token | grammar terminal | none or ParserError by position |
| quoted reserved word | quoted identifier | exact spelling | textual quoted identifier | none |
| `"Foo"`, `"foo"` | quoted identifier | case preserved | distinct textual payloads | none |
| `"a""b"` | quoted identifier | `a"b` | textual quoted identifier | none |
| `""` | rejected identifier | empty | none | LexerError/identifier-boundary error |
| quoted high byte | quoted identifier | exact byte | textual quoted identifier | none |
| unquoted high byte | invalid | none | none | LexerError |
| identifier containing NUL | invalid | none | none | LexerError |

| String row | Tokenization | Decoded bytes | Span | Error |
|---|---|---|---|---|
| `''` | one string token | empty | complete spelling | none |
| `'a'` | one string token | `a` | complete spelling | none |
| `'It''s'` | one string token | `It's` | complete spelling | none |
| backslash bytes | one string token | unchanged backslash bytes | complete spelling | none |
| comment markers | one string token | unchanged marker bytes | complete spelling | none |
| semicolon | one string token | semicolon byte | complete spelling | none |
| high bytes | one string token | exact high bytes | complete spelling | none |
| embedded NUL | one string token | exact NUL byte | complete spelling | none |
| unterminated | no token success | decoded prefix remains private | opener through EOF | LexerError |

| Numeric row | Tokens/raw payload | Raw AST | Chapter-17 handoff | Result owner |
|---|---|---|---|---|
| `0`, leading-zero decimal | integer spelling/magnitude | literal | smallest integer classification | Chapter 17 |
| INT32 boundaries | integer spelling/magnitude | literal or direct unary minus | exact INT32/INT64 boundary | Chapter 17 |
| INT64 boundaries | integer spelling/magnitude | literal or direct unary minus | exact INT64 boundary | Chapter 17 |
| `9223372036854775808` | lossless magnitude `2^63` | literal | direct-negative-only rule | Chapter 17 |
| decimal FLOAT | complete decimal token | literal | correctly rounded conversion | Chapter 17 |
| exponent FLOAT | complete exponent token | literal | correctly rounded conversion | Chapter 17 |
| malformed exponent | no valid complete FLOAT token | no accepted expression | lexical/grammar result | Chapters 17/18 |
| `.5`, `1.`, `1..2` | dot/integer boundaries per grammar | accepted only if resulting grammar admits sequence | no host extension | Chapter 18 |
| `+1`, `-1` | sign plus unsigned numeric token | unary node plus literal | direct-sign classification | Chapters 18 then 17 |
| direct/parenthesized INT64_MIN | same lossless magnitude | distinct parenthesis provenance | legal direct form versus ordinary operand | Chapter 17 |

#### Statement, list, alias, join, star, and subquery matrices

| Statement root | Minimal legal form | Principal optional surface | Required-list rule | Raw root | Semantic owner / excluded extension |
|---|---|---|---|---|---|
| CREATE TABLE | `CREATE TABLE t(a INT32)` | column modifiers, table PK/UNIQUE | elements/constraint columns nonempty | CreateTableStatement | Chs. 16/21; no ALTER/CHECK/FK/named constraint |
| CREATE INDEX | `CREATE INDEX i ON t(a)` | UNIQUE spelling is separate row | keys nonempty | CreateIndexStatement | §21.8; no expression/order/include/predicate |
| CREATE UNIQUE INDEX | `CREATE UNIQUE INDEX i ON t(a)` | none | keys nonempty | CreateIndexStatement | §21.8 |
| DROP TABLE | `DROP TABLE t` | none | no list | DropStatement | §21.9; no IF EXISTS/multiple |
| DROP INDEX | `DROP INDEX i` | none | no list | DropStatement | §21.9; no IF EXISTS/multiple |
| INSERT | `INSERT INTO t VALUES(1)` | columns, rows, SELECT, RETURNING | all displayed lists nonempty | InsertStatement | §§21.11–21.12; no DML DEFAULT |
| UPDATE | `UPDATE t SET a=1` | WHERE, RETURNING | assignments nonempty | UpdateStatement | §21.13; no alias/FROM/DEFAULT/LIMIT |
| DELETE | `DELETE FROM t` | WHERE, RETURNING | RETURNING nonempty when present | DeleteStatement | §21.14; no alias/USING/LIMIT |
| SELECT | `SELECT 1` | exact §18.11 clauses | projection and present lists nonempty | SelectStatement | Chs. 19/20 |
| BEGIN | `BEGIN` | none | none | TransactionStatement | Chapter 9 |
| COMMIT | `COMMIT` | none | none | TransactionStatement | Chapter 9 |
| ROLLBACK | `ROLLBACK` | none | none | TransactionStatement | Chapter 9 |
| VACUUM | `VACUUM t` | none | one target | VacuumStatement | §14.17; no targetless/options |
| ANALYZE | `ANALYZE t` | none | one target | AnalyzeStatement | §21.17.1/Ch. 34; no targetless/options |
| EXPLAIN SELECT | `EXPLAIN SELECT 1` | SELECT surface | SELECT lists as ordinary | ExplainStatement | Chapter 40; no non-SELECT target |
| EXPLAIN ANALYZE SELECT | `EXPLAIN ANALYZE SELECT 1` | SELECT surface | SELECT lists as ordinary | ExplainStatement | Chapter 40; no non-SELECT target |

| List family | Minimum | Empty? | Trailing comma? | Order/duplicates | Later validation |
|---|---:|---|---|---|---|
| CREATE TABLE elements | 1 | no | no | preserve both | DDL binder |
| PRIMARY KEY columns | 1 | no | no | preserve both | DDL binder |
| UNIQUE columns | 1 | no | no | preserve both | DDL binder |
| index keys | 1 | no | no | preserve both | index binder |
| INSERT columns | 1 when wrapper present | no | no | preserve both | DML binder |
| VALUES rows | 1 | no | no | preserve both | DML binder |
| VALUES row items | 1 | no | no | preserve both | DML binder |
| UPDATE assignments | 1 | no | no | preserve both | DML binder |
| SELECT items | 1 | no | no | preserve both | SELECT binder |
| GROUP BY | 1 when clause present | no | no | preserve both | aggregate binder |
| ORDER BY | 1 when clause present | no | no | preserve both | ORDER binder |
| RETURNING | 1 when clause present | no | no | preserve both | DML binder |
| ordinary function arguments | 1 in argument-list alternative | no | no | preserve both | function registry; `f()` separate |
| IN expression list | 1 | no | no | preserve both | TypeResolver |
| CASE WHEN arms | 1 | no | not comma syntax | preserve both | CASE binder |

| Alias context | AS rule | Alias required? | Implicit form | Raw payload | Binder owner |
|---|---|---|---|---|---|
| projection | AS required when alias present | no | rejected | explicit alias and span | §19.5 |
| base table | AS required when alias present | no | rejected | explicit alias and span | Chapter 19 |
| derived table | AS optional | yes | alias required after child | alias and child boundary | §20.14.3 |

| Join spelling | Grammar status | ON rule | Raw kind | Association/result |
|---|---|---|---|---|
| INNER JOIN | accepted | exactly one | inner | left-fold |
| LEFT JOIN | accepted | exactly one | left | left-fold |
| CROSS JOIN | accepted | absent | cross | left-fold |
| bare JOIN | rejected | not applicable | none | ParserError |
| comma join | rejected | not applicable | none | ParserError |
| RIGHT / FULL | rejected | not applicable | none | ParserError |
| NATURAL | rejected | not applicable | none | ParserError |
| USING | rejected | not applicable | none | ParserError |
| parenthesized join tree / LATERAL | rejected | not applicable | none | ParserError |

| Star source | Token context | Raw role | Downstream owner |
|---|---|---|---|
| `SELECT *` | select-item | unqualified select star | binder wildcard expansion |
| `t.*` | select-item after one identifier/dot | qualified select star | binder relation expansion |
| `f(*)` | function-call argument | star-argument call | Chapter 17/29 registry |
| `a*b` | multiplicative expression | binary multiplication | Chapter 17 operator registry |

| Subquery row | Parser result | Binder/result owner | Semantic owner | Error stage |
|---|---|---|---|---|
| scalar SELECT | accept | scalar-shape binding | §20.14.4 | bind/runtime cardinality as applicable |
| EXISTS SELECT | accept | EXISTS binding | §20.14.5 | semantic owner |
| IN SELECT | accept | one-column/type binding | §20.14.6 | semantic owner |
| derived SELECT | accept with required alias | derived boundary | §20.14.3 | semantic owner |
| correlated SELECT | accept | detect outer capture | §§19.18, 20.14.2 | UnsupportedCorrelation |
| multi-column scalar SELECT | accept | validate output arity | §20.14.1 | bind/semantic error |
| invalid IN output shape/type | accept | validate arity/type | §20.14.1 | bind/type error |
| INSERT/UPDATE/DELETE in subquery position | reject | none | §18.12.3 | ParserError |
| row constructor | reject | none | §18.12 | ParserError |

#### SourceSpan, lifetime, and resource matrices

| SourceSpan row | Selected span | Owner/rule |
|---|---|---|
| valid token | complete original token bytes | lexer token |
| invalid byte | smallest rejecting byte range | §18.8 lexical rule |
| unterminated string | opening single quote through EOF | §18.8 lexical rule |
| unterminated quoted identifier | opening double quote through EOF | §18.8 lexical rule |
| unterminated block comment | opening slash-star through EOF | §18.8 lexical rule |
| unexpected token | unexpected token span | §18.8 parser rule |
| missing required token at EOF | `[source_length,source_length)` | §18.8 parser rule |
| unknown column handoff | column-reference span | binder construct |
| unknown type handoff | type-name span | TypeResolver construct |
| equally specific tie | earliest represented span | §18.8 tie rule |

| Lifetime row | May borrow? | May own/materialize? | Required owner/lifetime | May outlive source? |
|---|---|---|---|---|
| original source | not applicable | backing owns | while any original-byte borrower lives | owner defines source lifetime |
| token | yes | yes | through token use and dependent transfer | only independent payload may |
| raw AST | yes | yes | through every raw consumer | yes with retained owner/materialization |
| bound object | yes only with retained owner | yes | through every bound consumer | yes when lifetime-safe |
| SourceSpan value | offsets only | value | independent integer value | yes; dereference may not |
| decoded string | yes | yes | through every consumer | yes when retained/materialized |
| identifier payload | yes | yes | through every consumer | yes when retained/materialized |
| numeric provenance | yes | yes | through Chapter-17 consumer | yes when retained/materialized |

| Resource condition | Canonical category | Grammar valid? | Partial raw result? | DB mutation/persistence | Transaction owner |
|---|---|---|---|---|---|
| invalid source form | LexerError | no | no | none | §39.1 |
| invalid grammar | ParserError | no | no | none | §39.1 |
| configured/safety guard | FrontEndResourceLimit | may be yes | no | none | §39.1 |
| required allocation fails | OutOfMemory | independent | no | none | §39.1 |
| execution spill failure | SpillIOError | independent | not a front-end result | temporary only | §§39.1, 39.3 |
| optimizer configured bound | OptimizerResourceLimit | independent | complete front-end input | none from front end | §§39.1, 39.4 |

#### Raw-AST and parser/binder matrices

| Raw category | Ordered children/payload | Span/provenance | Resolved IDs/TypeId? | Downstream owner |
|---|---|---|---|---|
| StatementRequest | statement sequence | request/statement spans | forbidden | command/binder |
| SelectStatement | clauses and lists | exact clause presence | forbidden | Chs. 19/20 |
| CreateTableStatement | table elements | names/types/default syntax | forbidden | Chapter 21 |
| CreateIndexStatement | key columns | uniqueness spelling/names | forbidden | §21.8 |
| DropStatement | object syntax | object kind/name | forbidden | §21.9 |
| InsertStatement | columns/source/RETURNING | exact source shape | forbidden | §§21.11–21.12 |
| UpdateStatement | assignments/WHERE/RETURNING | exact source shape | forbidden | §21.13 |
| DeleteStatement | WHERE/RETURNING | exact source shape | forbidden | §21.14 |
| TransactionStatement | none | keyword kind/span | forbidden | Chapter 9 |
| VacuumStatement | target name | statement/target span | forbidden | §14.17 |
| AnalyzeStatement | target name | statement/target span | forbidden | §21.17.1/Ch. 34 |
| ExplainStatement | SELECT child | ANALYZE presence | forbidden | Chapter 40 |
| AstIdentifier | canonical bytes | quoted distinction/span | forbidden | binder |
| AstQualifiedName | one/two components | ordered components/span | forbidden | binder |
| AstTypeName | canonical unquoted bytes | span | TypeId forbidden | TypeResolver |
| AstLiteral | decoded/lossless payload | spelling/direct-negative data | TypeId forbidden | Chapter 17 |
| AstBinaryExpression | left/right | operator/span | forbidden | TypeResolver |
| AstUnaryExpression | operand | operator/directness | forbidden | TypeResolver |
| AstParenthesizedExpression | expression | mandatory parenthesis provenance | forbidden | Chapter 17/binder |
| AstComparison | left/right | operator/span | forbidden | TypeResolver |
| AstNullPredicate | operand | NOT presence/span | forbidden | TypeResolver |
| AstInList | left/items | order/multiplicity | forbidden | TypeResolver |
| AstInSubquery | left/SELECT | IN/NOT provenance | forbidden | Chapters 19/20 |
| AstFunctionCall | name/arguments | zero/nonzero argument form | forbidden | Chapter 17/29 |
| AstStarArgumentCall | name | distinct star role | forbidden | Chapter 29 |
| AstCast | expression/type syntax | span | TypeId/cast insertion forbidden | TypeResolver |
| AstCase | WHEN arms/ELSE | order/clause presence | forbidden | Chapter 17 |
| AstStar | qualifier when present | select-star role | forbidden | binder |
| AstSubqueryExpression | SELECT child | scalar/EXISTS role | forbidden | Chapters 19/20 |
| AstTableReference | object/alias or derived child | alias/AS/source form | forbidden | binder |
| AstJoin | left/right/ON | kind and left-fold | forbidden | Chapters 19/20 |
| AstReturningClause | expression/alias items | order/multiplicity | forbidden | Chapter 21 |

| Question | Parse owner | Binder/TypeResolver/later owner | Expected stage |
|---|---|---|---|
| table or index exists | syntax only | catalog/binder | bind |
| column exists or is ambiguous | syntax only | binder | bind |
| two-part namespace equals `main` | component count only | binder/catalog | bind |
| type-name is one of seven | unquoted identifier only | TypeResolver | bind/type |
| function exists and arity is legal | call shape only | Chapter 17/29 | bind/type |
| aggregate star is legal | star-call shape only | Chapter 29 | bind/type |
| operator types compatible | operator syntax only | TypeResolver | bind/type |
| cast pair/value legal | CAST syntax only | TypeResolver | bind/type/runtime |
| alias resolves | explicit syntax only | binder | bind |
| duplicate DML target legal | preserve duplicates | DML binder | bind |
| constraint/default semantically legal | preserve syntax | Chapter 21/17 | bind |
| LIMIT/OFFSET integral/nonnegative/constant/no-subquery | expression syntax | §§19.14, 20.14.9 | bind |
| subquery correlated | SELECT structure only | §§19.18, 20.14.2 | bind |
| subquery output shape/type | SELECT structure only | §20.14/TypeResolver | bind/semantic |

#### Cross-chapter and high-level case matrices

| Owner | Chapter-18 handoff | Existing verification reused | Chapter-18 composition |
|---|---|---|---|
| Chapter 16 | canonical names, no stable IDs | catalog identity/name tests | prove IDs arise only after parse |
| Chapter 17 | literal provenance and raw expression syntax | type-system property section | direct-negative, CASE/IN/CAST/call handoff |
| Chapter 18 | closed tokens/grammar/raw AST/lifetime/resources | this section | direct owner |
| Chapter 19 | names/spans/raw trees | Binder Tests | semantic permutation leaves parsing unchanged |
| Chapter 20 | SELECT/subquery raw shape | Subquery Tests | structural acceptance versus semantic rejection |
| Chapter 21 | DDL/DML/default/RETURNING syntax | DDL/DML binder tests | syntax-to-semantic handoff |
| Chapter 29 | generic call/star syntax | aggregate tests | registry-independent parse |
| §39.1 | front-end failure cause | transaction matrix tests | no front-end mutation/retry rule |
| §39.2 | source/semantic/resource categories | front-end error tests | exact category and span |
| §39.3 | cross-layer OutOfMemory | execution resource tests | same cause before execution |
| Chapter 40 | EXPLAIN SELECT raw target | EXPLAIN tests | syntax-only target restriction |
| §41 | architecture verification obligations | subsystem suites | complete procedural realization |

| High-level case | Deterministic oracle | Expected result |
|---|---|---|
| each of `09 0A 0B 0C 0D 20` | whitespace table | trivia |
| line-comment EOF | comment state machine | accepted trivia |
| unterminated block comment | comment state machine/span arithmetic | LexerError, opener through EOF |
| mixed-case keyword | keyword table/ASCII fold | reserved keyword |
| quoted keyword | quoted decoder | identifier |
| structural high byte / quoted high byte | state byte table | LexerError / preserved payload |
| identifier NUL / empty quoted identifier | identifier grammar | rejection |
| doubled string quote | string decoder | one quote payload byte |
| direct-negative INT64_MIN | raw-tree plus Chapter-17 oracle | direct unary provenance retained |
| `.5`, `1.`, `1..2` | numeric/dot recognizer | exact closed-grammar result |
| `<=` | symbol trie | one longest token |
| comment/operator collision | lexical priority table | comment opener wins |
| `stmt`, `stmt;`, `stmt;stmt`, `stmt;;` | request EBNF | accept first three, reject last |
| CREATE TABLE trailing comma | CREATE EBNF | ParserError |
| unknown unquoted type | type-name grammar plus TypeResolver | parse then type failure |
| `f(*)`, `COUNT(*)`, `SUM(*)` | generic-call grammar | same raw star-call shape |
| implicit projection/base alias | SELECT EBNF | ParserError |
| INNER then LEFT chain | canonical join fold | left-associated raw tree |
| comparison chain | predicate-suffix count | ParserError |
| NOT precedence | expression expected-tree table | NOT wraps predicate |
| empty IN | IN production | ParserError |
| scalar SELECT subquery | subquery production | parse success |
| correlated SELECT subquery | syntax plus binder scope oracle | parse then UnsupportedCorrelation |
| data-modifying subquery | SELECT-only production | ParserError |
| missing input at EOF | span arithmetic | zero-width EOF span |
| duplicated AST list item | canonical structural serialization | both items retained in order |
| source release with retained AST | generation-tagged owner graph | valid only after retain/materialize |
| deliberate front-end guard | injected-cause oracle | FrontEndResourceLimit |
| failed required allocation | injected-cause oracle | OutOfMemory |
| failure after private AST nodes | publication/ownership ledger | no partial success; full unwind |

### Cross-owner consistency

Cross-owner composition asserts, without duplicating semantic suites: Chapter 16 receives
canonical names and creates stable IDs; Chapter 17 receives literal provenance and owns
types/operators/casts/CASE/IN/scalar calls; Chapter 19 receives unbound names/spans and owns
resolution, aliases, functions, LIMIT/OFFSET, and correlation diagnostics; Chapter 20 owns
no-FROM and subquery shape/semantics; Chapter 21 owns DDL/DML/default/RETURNING semantics;
Chapter 29 owns aggregate names/arity/star legality; §39.1 owns transaction consequences;
§§39.2–39.3 own error categories; Chapter 40 owns EXPLAIN semantics; §41 supplies the
architecture-level verification requirement.

### Chapter-18 verification family index

| Family | Scope |
|---|---|
| V18-1 | source-byte and lexical-domain determinism |
| V18-2 | whitespace and comments |
| V18-3 | keyword/symbol registries and longest match |
| V18-4 | identifiers, strings, and numeric-token handoff |
| V18-5 | SourceSpan and diagnostic selection |
| V18-6 | request and batch framing |
| V18-7 | top-level DDL/DML/control/maintenance grammar |
| V18-8 | SELECT, aliases, joins, clauses, and LIMIT/OFFSET |
| V18-9 | expression precedence, predicates, CAST, and CASE |
| V18-10 | generic calls, star roles, and structural subqueries |
| V18-11 | raw-AST shape, provenance, order, and multiplicity |
| V18-12 | parser/binder/TypeResolver/catalog boundaries |
| V18-13 | source, payload, token, AST, and bound-object lifetime |
| V18-14 | front-end resource categories and complete-result failure |
| V18-15 | error-taxonomy and transaction-boundary composition |
| V18-16 | locale/platform/parser-technique determinism |
| V18-17 | unsupported closed-language rejection |
| V18-18 | harness, independent oracles, and mandatory matrices |
| V18-19 | cross-chapter semantic handoffs |

### Atomic architecture-obligation coverage map

Each row below is one independently asserted Chapter-18 obligation. `Procedure` names the
subsection or mandatory matrix above; `Oracle` names an independently computed result.
Each mapping requires both. The inventory is derived from the live Chapter-18 contract;
its count is not a target selected in advance.

| Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Independent oracle |
|---|---|---|---|---|
| V18-1.1 — Front end is Lexer, Parser, raw AST followed by separate Binder | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.2 — SQL source is one length-delimited byte sequence | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.3 — Structural syntax is ASCII-defined | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.4 — Whole-source UTF-8 validity is not required | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.5 — Invalid structural high bytes fail deterministically | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.6 — Unquoted source NUL is rejected outside strings | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.7 — Original source stays immutable while borrowed | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-1.8 — Front-end processing is independent of locale, Unicode syntax classification, and signed char | §§18.1–18.2.1 | V18-1 | Harness; source-byte procedure | 256-byte/state partition |
| V18-2.1 — Whitespace is exactly bytes 09, 0A, 0B, 0C, 0D, and 20 | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.2 — Every other byte is non-whitespace | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.3 — Trivia separates adjacent lexical sequences without concatenating surrounding bytes | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.4 — Line comment opener has priority over minus tokens | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.5 — Line comments end before CR, before LF, or legally at EOF | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.6 — CRLF comment termination leaves both bytes as whitespace | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.7 — Block comments close at the first following closer | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.8 — Block comments are nonnested | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.9 — Unterminated block comments are LexerError with deterministic span | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-2.10 — Comment markers inside strings and quoted identifiers are inert | §§18.2, 18.7 | V18-2 | Whitespace/comment procedure; lexical matrix | six-byte table and comment state machine |
| V18-3.1 — Reserved-keyword registry is enumerated exactly | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.2 — Keyword recognition is ASCII case-insensitive | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.3 — Keyword recognition requires a complete identifier boundary | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.4 — Quoted keyword spellings remain identifiers | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.5 — No contextual or nonreserved keyword class exists | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.6 — Symbolic-token registry is enumerated exactly | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.7 — Longest registered symbolic spelling wins | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.8 — Comment openers precede component symbol recognition | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-3.9 — Unregistered symbolic sequences are LexerError rather than generic operators | §18.3 | V18-3 | Registry procedure; lexical matrix | closed keyword and symbol tables |
| V18-4.1 — Unquoted identifier start bytes are exactly ASCII letters and underscore | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.2 — Unquoted identifier continuation bytes are exactly ASCII letters, digits, and underscore | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.3 — Unquoted identifiers reject non-ASCII and NUL | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.4 — Unquoted canonicalization maps only ASCII uppercase to lowercase | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.5 — Quoted identifiers use double-quote delimiters | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.6 — Doubled double quote decodes to one quote byte | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.7 — Quoted identifier bytes and case are preserved without Unicode normalization | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.8 — Quoted identifiers reject NUL and decoded empty payload | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.9 — Unterminated quoted identifiers are LexerError | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.10 — Strings use single-quote delimiters and doubled-single-quote decoding | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.11 — Backslash has no escape meaning and string high bytes plus embedded NUL are retained | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.12 — Unterminated strings are LexerError with original spelling span | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.13 — Integer tokens are ASCII DIGIT plus and signs remain separate | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.14 — FLOAT tokens use exactly the two Chapter-17 decimal forms | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-4.15 — Numeric dot boundaries and lossless magnitude through 2^63 are preserved without host conversion | §§18.4–18.6; §§17.5.1–17.5.3 | V18-4 | Identifier/string/numeric procedures; matrices | independent byte decoders and declarative numeric recognizers |
| V18-5.1 — Token and AST spans are half-open intervals | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.2 — Span offsets count bytes in the original request | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.3 — Every span satisfies zero less-than-or-equal start less-than-or-equal end less-than-or-equal source length | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.4 — Invalid-byte and unknown-symbol spans identify the rejecting range | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.5 — Unterminated lexical constructs span opening delimiter through EOF | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.6 — Unexpected-token parser errors use that token span | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.7 — Missing required input at EOF uses the zero-width EOF span | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.8 — Binder and type diagnostics retain the directly responsible syntactic span with earliest-source tie rule | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-5.9 — Span values may outlive source bytes while text dereference requires retained or materialized bytes | §18.8 | V18-5 | SourceSpan procedure; SourceSpan matrix | original-vector interval arithmetic |
| V18-6.1 — A request contains at least one statement | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.2 — A statement needs no final semicolon | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.3 — One optional final semicolon is accepted | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.4 — Semicolons are required between adjacent statements | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.5 — Leading, repeated, and interior empty statements are rejected | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.6 — Whitespace-only and comment-only requests are rejected | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.7 — Successful parsing consumes all nontrivia input through EOF | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-6.8 — Request AST preserves statement order and no failure publishes a successful prefix | §18.10.1 | V18-6 | Request-framing procedure | request EBNF table |
| V18-7.1 — Complete dispatch admits exactly the listed statement roots | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.2 — CREATE TABLE requires a nonempty ordered table-element list | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.3 — Column definition is name, unquoted type name, optional NOT NULL, then optional DEFAULT expression | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.4 — CREATE TABLE supports only table-level PRIMARY KEY and UNIQUE constraints | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.5 — CREATE TABLE excludes explicit column NULL, column-level key constraints, named constraints, CHECK, and foreign keys | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.6 — CREATE INDEX and CREATE UNIQUE INDEX require a nonempty base-column key list | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.7 — CREATE INDEX excludes expression keys, ordering, INCLUDE, predicates, and IF NOT EXISTS | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.8 — DROP admits exactly one TABLE or INDEX object | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.9 — DROP excludes IF EXISTS, CASCADE, RESTRICT, and multiple targets | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.10 — INSERT admits optional nonempty target columns and VALUES or SELECT source | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.11 — VALUES contains one or more nonempty rows | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.12 — UPDATE requires a nonempty assignment list and preserves duplicate assignments | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.13 — UPDATE excludes target aliases, FROM, tuple assignment, ORDER BY, and LIMIT | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.14 — DELETE requires DELETE FROM and excludes target aliases, USING, ORDER BY, and LIMIT | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.15 — DML DEFAULT sentinels are absent while column-definition DEFAULT remains | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.16 — RETURNING is a nonempty expression list with optional explicit AS aliases | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.17 — RETURNING is available only on INSERT, UPDATE, and DELETE and excludes stars | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.18 — Transaction syntax is exactly BEGIN, COMMIT, and ROLLBACK | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.19 — VACUUM and ANALYZE each require exactly one table target and no options | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-7.20 — EXPLAIN and EXPLAIN ANALYZE accept SELECT only | §§18.10.2–18.10.4 | V18-7 | DDL/DML/dispatch procedures; statement/list matrices | statement EBNF tables |
| V18-8.1 — SELECT clause order is fixed from DISTINCT through OFFSET | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.2 — Projection list is nonempty, ordered, multiplicity-preserving, and has no trailing comma | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.3 — SELECT DISTINCT is optional | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.4 — FROM is syntactically optional in outer and nested SELECT | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.5 — Projection aliases require explicit AS | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.6 — Base-table aliases require explicit AS | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.7 — Derived-table alias is mandatory and its AS is optional | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.8 — Qualified SELECT star has exactly one identifier qualifier | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.9 — FROM admits only one joined-table chain, not comma joins or table functions | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.10 — INNER JOIN requires exactly one ON expression | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.11 — LEFT JOIN requires exactly one ON expression | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.12 — CROSS JOIN has no ON expression | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.13 — Join tails left-fold in source order | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.14 — Bare, RIGHT, FULL, NATURAL, USING, LATERAL, and parenthesized-tree joins are rejected | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.15 — GROUP BY and ORDER BY required lists are nonempty, ordered, and no-trailing | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.16 — ORDER items allow only optional ASC or DESC | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-8.17 — LIMIT and OFFSET operands are expressions and OFFSET may appear without LIMIT | §§18.11–18.11.1 | V18-8 | SELECT/alias/join procedures; matrices | SELECT EBNF and canonical join tree |
| V18-9.1 — OR is the lowest binary expression level and left-associative | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.2 — AND binds above OR and is left-associative | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.3 — Prefix NOT binds above AND and nests toward its operand | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.4 — Predicate suffix binds above NOT and occurs at most once | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.5 — Comparison operators are exactly equals, not-equals angle, less, less-equal, greater, greater-equal | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.6 — IS NULL and IS NOT NULL are exact null-predicate forms | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.7 — IN and NOT IN are exact predicate forms | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.8 — Comparison and predicate chaining is rejected | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.9 — Additive plus and minus are left-associative | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.10 — Multiplicative star, slash, and percent are left-associative | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.11 — Unary plus and minus nest toward their operand | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.12 — Parentheses override precedence and remain represented | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.13 — IN expression lists are nonempty, no-trailing, ordered, and duplicate-preserving | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.14 — Row-valued IN and row-constructor expressions are absent | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.15 — CAST syntax is exactly CAST expression AS unquoted type name | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.16 — Searched CASE requires one or more ordered WHEN arms and optional ELSE | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.17 — Simple CASE and parameterized or double-colon cast syntax are absent | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-9.18 — Column references admit exactly one or two identifier components | §§18.12, 18.15 | V18-9 | Expression procedure; precedence matrix | expression EBNF expected-tree table |
| V18-10.1 — Ordinary zero-argument function-call syntax is registry-independent | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.2 — Ordinary nonempty expression-argument calls preserve order and multiplicity | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.3 — Star-argument calls are a distinct syntax form | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.4 — Parser does not special-case COUNT, SUM, AVG, MIN, MAX, or any function name | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.5 — Function names are unqualified identifiers and qualified calls are absent | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.6 — SELECT star, qualified star, call star, and multiplication star remain distinct | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.7 — Scalar parenthesized SELECT subquery parses structurally | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.8 — EXISTS SELECT subquery parses structurally | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.9 — IN SELECT subquery parses structurally | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.10 — Derived SELECT requires its alias and parses structurally | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.11 — Data-modifying and other non-SELECT subqueries are ParserError | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.12 — Unsupported row constructors and absent subquery wrappers are ParserError | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.13 — Correlated structurally valid SELECT reaches binding | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-10.14 — Scalar and IN output-shape or type failures reach semantic owners | §§18.12.3–18.12.4 | V18-10 | Function/star/subquery procedures; matrices | call and SELECT-only subquery productions |
| V18-11.1 — Raw AST records syntax rather than resolved semantics | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.2 — Every listed raw-AST semantic category has an observable equivalent | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.3 — Raw AST retains canonical textual identifiers and optional qualification | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.4 — Raw type-name payload retains bytes and span but no TypeId | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.5 — Raw AST excludes stable catalog IDs and BindingId | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.6 — Raw AST excludes catalog descriptors, inserted semantic casts, and plans | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.7 — Statement nodes retain every present clause | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.8 — Parenthesized-expression provenance is mandatory | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.9 — Direct-negative literal provenance distinguishes direct and parenthesized operands | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.10 — Every ordered raw child collection preserves exact source order | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.11 — Every raw child collection preserves exact multiplicity and duplicates | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.12 — Join nodes preserve kind, left, right, and required ON presence | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-11.13 — RETURNING items and explicit aliases remain ordered and syntactically distinct | §§18.13, 18.16 | V18-11 | Raw-AST procedure; raw-AST matrix | canonical structural serialization |
| V18-12.1 — Parser is independent of catalog contents and object existence | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.2 — Parser is independent of TypeResolver and the seven-type registry | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.3 — Unknown unquoted type names parse and fail only during type resolution | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.4 — Quoted identifiers are rejected in type-name position | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.5 — One- and two-part object names parse without validating namespace main | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.6 — Three-part object and column names are structurally rejected | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.7 — Function existence, arity, scalar or aggregate identity, and star legality are semantic checks | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.8 — Operator and cast type legality are semantic checks | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.9 — Alias resolution, duplicate targets, defaults, and constraint validity are semantic checks | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-12.10 — LIMIT/OFFSET legality, correlation, and subquery output shape are semantic checks | §§18.10, 18.12.5; Chapters 16–20, 29 | V18-12 | Boundary procedure; parser/binder matrix | same syntax under permuted semantic fixtures |
| V18-13.1 — One conceptual front-end backing owner covers source and referenced derived payload | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.2 — Original source bytes remain immutable while borrowed | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.3 — Tokens and raw AST may borrow source or derived payload | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.4 — Every backing owner outlives every borrower | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.5 — Required payload is retained or materialized before an earlier owner is released | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.6 — Token object lifetime is distinct from token-payload lifetime | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.7 — Tokens may die before raw AST after safe payload transfer or retained backing | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.8 — Raw AST may die after binding when no surviving object depends on it | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.9 — Bound objects may borrow only while the corresponding owner remains alive | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.10 — Decoded string and quoted-identifier payloads remain valid for every consumer | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.11 — Canonical identifiers and exact numeric provenance remain valid for every consumer | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-13.12 — Backing and SourceSpan text are runtime-only and introduce no persistent format | §§18.2.1, 18.8, 18.14 | V18-13 | Lifetime procedures; lifetime matrix | generation-tagged ownership graph |
| V18-14.1 — No implementation-convenience request, token, payload, nesting, list, statement, or AST maximum becomes grammar | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.2 — Existing semantic and persistent-domain limits remain authoritative | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.3 — Finite operational front-end budgets are permitted independently of grammar | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.4 — Deliberate checked budget refusal is FrontEndResourceLimit | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.5 — Unsatisfied required allocation is OutOfMemory at every front-end or binding stage | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.6 — FrontEndResourceLimit and OutOfMemory remain distinct | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.7 — One valid SQL source may succeed or resource-fail under different configurations without changing validity | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.8 — Uncontrolled stack overflow, abort, corruption, or undefined behavior is nonconforming | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.9 — Resource failure publishes no successful partial token stream, AST, or bindable prefix | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.10 — Resource failure never truncates source payload or ignores suffix input | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.11 — Resource failure never drops, reorders, or deduplicates AST children | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.12 — Resource failure never weakens numeric provenance or changes literal semantics | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.13 — Private lexical, parser, binder, and backing state unwinds safely | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-14.14 — Front-end budgets and failure state are runtime-only and absent from persistence and recovery | §18.17 | V18-14 | Resource procedures; resource matrix | injected guard versus failed-allocation event |
| V18-15.1 — Invalid source byte or token form is LexerError | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.2 — Structurally invalid grammar is ParserError | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.3 — Bind, type, catalog, constraint, unsupported, and cardinality categories remain semantic-owner results | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.4 — FrontEndResourceLimit is not LexerError, ParserError, BindError, or TypeError | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.5 — OutOfMemory is not relabeled by the pipeline stage | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.6 — No generic competing ResourceError or parser-specific resource category is introduced | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.7 — Canonical source processing reports the encountered resource cause without proving an unprocessed suffix | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-15.8 — Section 39.1 alone maps the failure cause to statement and transaction outcome | §§18.8, 18.12.5, 18.17; §§39.1–39.3 | V18-15 | Diagnostic/resource/transaction procedures | error-stage and injected-cause table |
| V18-16.1 — Locale cannot alter whitespace, keyword, identifier, or token classification | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-16.2 — Host char signedness cannot alter high-byte behavior | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-16.3 — Unicode library version cannot alter structural syntax or normalization | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-16.4 — Host integer conversion cannot alter numeric token boundaries or handoff | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-16.5 — Host floating conversion cannot alter numeric token boundaries or handoff | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-16.6 — Parser implementation or generator conflict resolution cannot alter accept/reject or canonical raw tree | §§18.2–18.9, 18.16 | V18-16 | Platform and technique procedures | cross-configuration canonical token/tree comparison |
| V18-17.1 — Unsupported top-level statement families are rejected | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-17.2 — Unsupported DDL constraints, options, and type syntaxes are rejected | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-17.3 — Unsupported join, alias, and SELECT set/window forms are rejected | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-17.4 — Unsupported DML extensions and RETURNING placements are rejected | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-17.5 — Unsupported expression, parameter, function modifier, and symbolic forms are rejected | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-17.6 — No external SQL dialect compatibility alias expands the language | §§18.3, 18.10–18.12.5 | V18-17 | Closed-language complement procedure | expressly absent-production matrix |
| V18-18.1 — Deterministic harness observes lexer, parser, handoff, lifetime, and resource events | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.2 — Core correctness uses deterministic barriers and no sleeps | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.3 — Production lexer and parser do not serve as their own oracle | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.4 — Lexical, identifier, string, numeric, and SourceSpan matrices are complete | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.5 — Statement, list, alias, join, star, and subquery matrices are complete | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.6 — Lifetime, resource, raw-AST, and parser/binder matrices are complete | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.7 — High-level domain/case matrix covers every named Chapter-18 boundary family | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-18.8 — Single-defect fixtures isolate one invalid condition against a valid control | Chapter 18; §41.4 | V18-18 | Harness and mandatory matrices | independent fixture inventory audit |
| V18-19.1 — Chapter 16 canonical-name and stable-ID boundary is preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.2 — Chapter 17 literal, type, operator, cast, CASE, IN, and scalar-call boundary is preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.3 — Chapters 19 and 20 binding, no-FROM, LIMIT, and subquery boundaries are preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.4 — Chapter 21 DDL, DML, default, RETURNING, and recovery boundaries are preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.5 — Chapter 29 aggregate registry and star-legality boundary is preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.6 — Sections 39.1–39.3 error and transaction boundaries are preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |
| V18-19.7 — Chapter 40 EXPLAIN and Section 41 verification ownership are preserved | Chapters 16–21, 29, 39, 40, 41 | V18-19 | Cross-owner consistency procedure; composition matrix | owner-to-handoff ledger |


No debug formatter, fuzzer, production parser, or reference database substitutes for the
independent lexical, grammar, raw-tree, span, lifetime, and resource oracles above.

---

## Chapter 19 Binding and Expression Semantics Verification

This section is the complete procedural owner for the binding and bound-expression
contract in [`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 19. Chapters 16–18 supply the
catalog, type, raw-AST, SourceSpan, and lifetime primitives; Chapters 20, 21, and 29 own
the downstream logical, statement, and aggregate semantics; §§39.1–39.3 own transaction
and error consequences. The procedures here verify their Chapter-19 composition without
creating a second semantic registry.

### Deterministic binder harness and oracle discipline

Use a fixture driver that accepts one exact Chapter-18 raw AST plus its immutable source
backing, one transaction-visible catalog snapshot, and the frozen scalar and aggregate
registries. A fixture controls canonical identifier bytes, SourceSpans, parenthesis and
integer-literal provenance, statement/query-block boundaries, descriptor presentation
order, SchemaVer, and transaction-local catalog visibility.

The harness records architecture-visible conceptual events:

```text
input:
    raw node entered; source child visited; SourceSpan/provenance observed
catalog:
    snapshot selected; object-kind lookup; immutable descriptor selected
scope:
    query block entered/exited; relation occurrence created; qualifier exposed
binding:
    local candidates collected; outer diagnostic lookup; name/type/call resolved
    wildcard expanded; implicit cast inserted; aggregate classified/numbered
    output metadata appended; ORDER item classified; LIMIT expression classified/folded
error:
    prerequisite satisfied/failed; candidate category/span/class; selected error
publication:
    complete immutable bound statement published or private state discarded
resource:
    checked front-end budget admitted/refused; required allocation succeeded/failed
```

These are observations, not required production APIs. Deterministic adapters, event
sentinels, injected descriptors, and fault points may expose them. Correctness never uses
sleeps, wall-clock ordering, production container iteration, pointer values, source class
names, or a production binder/type resolver as its own oracle.

Canonical comparison serializes a successful bound result as ordered semantic data:

```text
statement/query-block structure
relation occurrences and opaque-equivalence BindingId classes
TableId / ColumnId / SchemaVer descriptor identities
bound expression kind, logical type, nullability, semantic children
resolved operator/function/aggregate descriptor identity
explicit/implicit cast provenance and SourceSpan
ordered output metadata and wildcard expansion
aggregate ordinal
ORDER output identity or ordinary expression
folded/residual LIMIT/OFFSET expression
```

BindingId numeric values are alpha-renamed by first relation-occurrence source order before
cross-run comparison. This preserves equality/inequality and rewrite stability without
making a representation or allocation sequence observable.

Independent oracles are:

| Domain | Independent oracle |
|---|---|
| raw input | Chapter-18 canonical raw-tree serializer and exact byte-span/provenance tables |
| BindingId | declarative whole-statement relation-occurrence graph with alpha-renamed identities |
| qualifiers/scopes | query-block-local qualifier maps plus explicit parent diagnostic links |
| columns | complete local candidate-set cardinality model over ordered immutable descriptors |
| catalog | Chapter-16/21 snapshot visibility model keyed by stable object identity and SchemaVer |
| wildcards | source relation order crossed with §16.8 presentation order |
| output names | explicit-alias/direct-column/source-byte-slice decision table |
| clause visibility | closed clause-by-name matrix below, independent of binder traversal |
| types/casts | complete Chapter-17 table-driven TypeResolver oracle already owned by Type-System Property Tests |
| calls | union of §17.9.3 and §29.3 descriptor tables plus star/arity/shape filtering |
| grouping | independent fully-bound structural comparator and recursive group-validity predicate |
| ORDER BY | raw-provenance classifier followed by explicit-alias and input-scope models |
| LIMIT/OFFSET | recursive dependency classifier, Chapter-17 type/fold oracle, and final INT64-domain oracle |
| DML/DDL | closed statement-local namespace and object-kind tables below |
| subqueries | explicit child-scope graph plus §20.14 shape/result handoff tables |
| errors | prerequisite-filtered candidate list sorted by `(start, length, validation_class)` |
| resources/lifetime | deterministic failure schedule and generation-tagged ownership graph |
| environment | canonical serialization replay under permuted containers, locales, addresses, and schedules |

Production binder code MUST NOT construct the expected result used to verify itself.

### V19-1 — Raw-AST and bound-result boundary

Feed exact Chapter-18 raw trees directly and compare their successful output with the
canonical bound serialization. Assert complete resolution of names, types, nullability,
operator/call identities, casts, spans, provenance, relation identities, and ordered output
metadata. The binder neither reinterprets grammar nor chooses physical access/join
algorithms, estimates cardinality, reads heap/RID state, allocates persistent object/File
IDs, writes WAL, mutates catalog/database state, or leaves executor-time SQL name lookup.
Assert the closed expression-kind inventory and the return type, conservative
nullability, and SourceSpan on every node. The expression handoff may feed vectorized
evaluation but owns no operator scheduling or pipeline state. On failure the binder
publishes no successful or executable bound prefix.

### V19-2 — BindingId identity and lifetime

Build statements containing one table, aliased and unaliased self-joins, multiple joins,
scalar/EXISTS/IN children, derived tables, and the same TableId repeated in several child
blocks. Compare against the relation-occurrence graph: every distinct occurrence in the
whole top-level statement has a distinct BindingId, including nested blocks, and no child
scope reset permits reuse. Corresponding self-join columns may share TableId and ColumnId
but not BindingId. Different statements may alpha-rename/reuse opaque values.

Apply an abstract logical rewrite fixture. A surviving semantic occurrence retains its
BindingId, a removed occurrence disappears, and a newly distinct occurrence cannot inherit
an existing identity. Assert no persistence, WAL/catalog representation, width, numeric
ordering, pointer identity, or cross-statement value is required.

### V19-3 — Qualifier namespace, alias hiding, and diagnostic shadowing

The scope oracle constructs exactly one canonical-byte qualifier map per query block.
`main.t` exposes `t`; `t AS x` exposes only `x`; a derived table exposes only its mandatory
alias. Duplicate insertion reports `BindError` at the later conflicting qualifier span,
including an unaliased self-join; distinct explicit aliases make a self-join legal. No
first/last win, automatic alias, or hidden disambiguator is accepted.

For qualified `q.col`, local `q` is authoritative. A missing member is a local BindError and
does not inspect an outer `q`. For unqualified `col`, collect the entire local candidate set
before any outer lookup: zero permits diagnostic outer search, one binds locally, and two
or more is local ambiguity. A local miss plus outer resolution is
`UnsupportedFeature/UnsupportedCorrelation`; absence everywhere is ordinary BindError.

### V19-4 — Column resolution

Enumerate zero/one/many unqualified candidates and qualifier-zero/member-zero/one qualified
cases over descriptors whose storage and insertion orders are permuted. The expected bound
reference contains the exact BindingId, TableId, ColumnId, logical type, nullability, and
source span. Unknown qualifier uses the qualifier span; known qualifier with unknown member
uses the member span; ambiguity uses the complete column-reference span. No leftmost,
catalog-order, hash-order, or alias/base-name fallback is legal.

### V19-5 — Catalog snapshot, descriptor, and SchemaVer binding

Reuse the Catalog Tests snapshot oracle with committed visible descriptors, own completed
earlier DDL, another transaction's uncommitted DDL, old snapshots, cache hits/misses, and
cache replacement. The selected immutable descriptor, TableId/ColumnId, and SchemaVer are
stable for the bound statement. Same raw AST plus same snapshot produces the same object
identity regardless of cache layout; binding does not duplicate MVCC or publication tests.

### V19-6 — Wildcards and output metadata

Cross source relation order with each descriptor's logical presentation order. Scramble
TableId, ColumnId, descriptor storage, and hash insertion order. `*` expands that exact
cross-product; `x.*` expands one qualifier; unknown `x` is BindError; no-FROM `*` is
BindError. Duplicate source names remain distinct ordered bound outputs, and execution
never performs wildcard expansion.

For each output independently derive `(expression, display_name, logical_type, nullable)`.
Display-name priority is explicit AS alias, direct source-column canonical name, then the
exact original source-byte slice selected by the expression SourceSpan. Preserve spelling
and whitespace; do not pretty-print, normalize, or expose inserted-cast text. Duplicate
top-level names and explicit aliases are legal distinct slots. Generated/direct-column
names without AS never enter the output-alias namespace. Exercise §18.14 retention and
materialization when source backing ends.

### V19-7 — SELECT aliases and clause visibility

Use the clause matrix below as the independent oracle. Explicit SELECT AS aliases are
visible only in ORDER BY, not in the same SELECT item list, JOIN ON, WHERE, GROUP BY,
HAVING, LIMIT, or OFFSET. For one complete unqualified ORDER identifier, consider only
explicit AS aliases: zero falls back to input scope, one references the existing output
identity, and multiple produce ambiguous-output-alias BindError. Generated names and
unaliased direct-column display names are excluded. An alias reference never rebinds its
expression or creates another aggregate occurrence.

### V19-8 — FROM and JOIN visibility

Derived tables bind as independent uncorrelated child blocks and see neither earlier nor
later FROM siblings. A sibling reference reaches unsupported-correlation diagnosis rather
than LATERAL semantics. For `left JOIN right ON p`, `p` sees all accumulated-left bindings
plus current right and no future relation. After the tree is complete, WHERE, GROUP BY,
HAVING, SELECT, and ordinary ORDER fallback see its local relations; LIMIT/OFFSET does not.

### V19-9 — Type names, literals, NULL, and TypeResolver handoff

Resolve every admitted Chapter-18 type-name node to the exact frozen TypeId and report an
unknown type as TypeError at its type-name span. Reuse the complete Chapter-17 literal and
TypeResolver oracles for integer, FLOAT64, VARCHAR, BOOLEAN, and unresolved NULL. Exercise
NULL in projection, comparison, explicit cast, CASE, IN, INSERT/UPDATE assignment, and
LIMIT/OFFSET. Context must resolve it to one concrete type or the frozen underconstrained
error; no persistent UNKNOWN TypeId survives binding. Bound return type and conservative
nullability are compared with the Chapter-17/20 owner tables. The nullability table covers
NOT NULL and nullable columns, arithmetic, comparison, IS NULL, COUNT, SUM, scalar
subqueries, EXISTS/NOT EXISTS, and IN/NOT IN subqueries independently.
Compose the NOT NULL case with V20-7's existing LEFT JOIN contextual-nullability
transform; source catalog metadata alone is not the output-nullability oracle.

### V19-10 — Casts, assignment, operators, predicates, CASE, and IN

Reuse the exhaustive Chapter-17 operator, comparison, cast, assignment, CASE, IN, and 3VL
matrices, but inspect the Chapter-19 bound tree. Required implicit widening produces one
synthetic cast with exact target type, implicit provenance, and the coerced operand span.
Explicit `CAST(expr AS T)` retains explicit provenance and the complete CAST span. The two
origins remain distinguishable and runtime-only under §18.14.

INSERT/UPDATE use only §17.8.5 assignment coercion, never general explicit-cast reachability.
JOIN ON, WHERE, HAVING, and CASE WHEN require BOOLEAN with no integer truthiness. CASE and
IN preserve source order, contextual NULL typing, common type, inserted casts, and the
Chapter-17 evaluation handoff. No binder-specific overload or cast matrix is an oracle.

### V19-11 — Generic calls and registry/type error classification

Construct Chapter-18 generic ordinary/star calls without parser name special-casing. Filter
the frozen scalar and aggregate descriptors by canonical name, call shape, arity,
star authorization, and argument types. Exactly one legal descriptor binds. Unknown name,
wrong arity/shape, unauthorized star form (`foo(*)`, `sum(*)` where absent), argument type
mismatch, and unresolved overload ambiguity are TypeError. `count(*)` succeeds only through
the §29.3 descriptor. If an argument such as `unknown_column` fails, its prerequisite
BindError is returned without a fabricated call TypeError. A resolved aggregate in an
illegal clause remains placement BindError. The named scalar registry stays empty.

For each resolved call, compare canonical name, argument signature or polymorphic rule,
return-type rule, volatility, NULL handling, and runtime implementation identity. The
implementation identity is not persistent default metadata. Verify the IMMUTABLE-only
constant-folding gate, that VOLATILE expressions are never folded as stable, and that the
bound node carries dispatch identity without executor-time name lookup.

### V19-12 — Aggregate classification, placement, nesting, and ordinal

Classify calls from §29.3 before applying placement. Direct aggregates are legal only in
SELECT, HAVING, and ORDER BY; reject direct aggregate use in JOIN ON, WHERE, GROUP BY,
LIMIT/OFFSET, DML expressions/RETURNING, and schema defaults as BindError. Reject a same-
block aggregate inside another aggregate; an aggregate in a child query block belongs only
to that child. Aggregate DISTINCT and FILTER forms remain absent from the aggregate
registry even if another layer recognizes related syntax or operators.

Build source layouts where SELECT/HAVING/ORDER traversal differs from byte order. The
independent span sorter assigns aggregate ordinal by first aggregate source-byte occurrence
per query block, retains one ordinal for shared occurrences, and verifies ORDER alias
references do not duplicate an aggregate. Rewrites preserve the ordinal as §29.3.7 requires.

### V19-13 — Aggregate-query and grouped-expression legality

The classification oracle marks a block aggregate when GROUP BY exists or a block-owned
aggregate occurs in SELECT, HAVING, or ORDER BY. Child aggregates do not classify a parent;
aggregate without GROUP BY creates one global group. HAVING is legal only for an aggregate
query; HAVING alone without GROUP BY or any aggregate is BindError.

Use an independent comparator over fully bound expressions. Ignore SourceSpan,
parenthesis-only provenance, and display aliases; include BindingId, TableId/ColumnId,
literal value/type, TypeId, operator/call descriptor, explicit and implicit casts, child
order, and every semantic child. Do not infer commutativity, associativity, constant-
folding equivalence, keys, uniqueness, predicates, or functional dependencies.

The recursive group-validity oracle admits current-block aggregates, complete exact
group-key subtrees, execution-independent constants, and scalar nodes whose row-dependent
children are group-valid. Thus `GROUP BY a` admits `a+1`, while `GROUP BY a+1` does not
admit bare `a`. GROUP BY aliases and ordinals are absent; `GROUP BY 1` is an ordinary
integer expression.

### V19-14 — ORDER BY aliases and ordinals

Classify an ordinal only when the complete raw expression is a bare, unparenthesized,
unsigned integer-literal node. `(1)`, `+1`, `-1`, `1+0`, `CAST(1 AS INT32)`, and `1.0` are
ordinary expressions. Compare the lossless magnitude without host signed conversion:
`1..N` references the output, while zero, `N+1`, and arbitrarily large magnitudes are
BindError ordinal attempts.

For every item apply ordinal classification first, explicit-AS alias lookup second, and
ordinary input binding third. Alias/input collisions and duplicate aliases prove this
priority. Output references retain existing semantic identity. ORDER expressions use the
§17.7.1 orderable-type oracle; BOOLEAN is rejected absent an explicit legal cast.

### V19-15 — LIMIT/OFFSET folding and execution-start validation

The recursive eligibility oracle admits typed literals/NULL, casts, scalar operators,
comparisons/predicates, CASE, and an IMMUTABLE scalar call whose children qualify. It
rejects columns, relation references, output aliases, aggregates, scalar/EXISTS/IN
subqueries, and every row-dependent construct. The empty scalar registry means no named
source call qualifies. LIMIT/OFFSET has no relation or output-alias namespace.

Apply the Chapter-17 type oracle: only INT32/INT64 are accepted, INT32 widens to INT64, raw
NULL receives INT64 NULL context, nonconstant is BindError, and nonintegral is TypeError.
Then apply mandatory §17.10.2 folding and error timing. Required cases are:

| Source | Binding result | Execution-start result |
|---|---|---|
| `1+2` | folded/residual INT64 `3` | acquire `3` once |
| `1/0` | `DIVISION_BY_ZERO` | not reached |
| `INT64_MAX+1` | `NUMERIC_OVERFLOW` | not reached |
| `NULL` | typed/folded INT64 NULL | `ExecutionError` |
| `-1` | folded/residual INT64 `-1` | `ExecutionError` |
| column or subquery | BindError | not reached |
| `1.5` | TypeError | not reached |

Execution consumes the folded/residual representation once before any relational operator
produces or consumes rows; it does not reevaluate completed pre-fold work. Final NULL and
negative validation occurs only at that boundary, with no clamping or implementation-sized
semantic maximum. Use deterministic event barriers, never sleeps.

### V19-16 — DML namespaces

INSERT VALUES expressions have no target-row scope; `VALUES(a)` does not see target `t.a`.
INSERT SELECT is an independent query block, with target mapping after source binding.
UPDATE creates one unaliased target binding visible to SET RHS, WHERE, and RETURNING;
DELETE exposes it to WHERE and RETURNING. The qualifier is the final target table-name
component, and assignment LHS lookup is direct against the target descriptor. Verify
duplicate INSERT targets and UPDATE assignments as BindError with deterministic spans.

Compose with §21.15 row images: INSERT/UPDATE RETURNING bind new/final rows and DELETE
RETURNING binds the old row. Aggregates remain forbidden. Verify namespace/type binding
only; physical writes, affected rows, publication, MVCC, and result buffering stay in the
Chapter-15/21 procedures.

### V19-17 — DDL, maintenance-target, and EXPLAIN binding

CREATE TABLE uses one statement-local declared-column namespace. Valid, missing, repeated,
and multiply declared PRIMARY KEY/UNIQUE members produce the exact
ConstraintDefinitionError outcomes in §§21.6–21.7; defaults reuse §21.12.

CREATE INDEX resolves the catalog-visible table and source-ordered descriptor columns;
duplicate key is BindError and an ineligible type is TypeError. DROP resolves exactly TABLE
or INDEX; missing/wrong-kind is CatalogError. No physical creation, publication,
dependency, retirement, or ID allocation is tested here. VACUUM/ANALYZE target lookup uses
the same canonical snapshot oracle. EXPLAIN's inner SELECT canonical binding/error must
equal ordinary SELECT binding; the wrapper cannot alter resolution.

### V19-18 — Subquery scopes and unsupported correlation

For scalar, EXISTS, IN, and derived forms, create a child query block with statement-unique
BindingIds and local resolution. The bound form records stable query-local identity, exact
subquery kind, typed logical child plan, result type/nullability, and SourceSpan. Derived
tables expose only their mandatory alias and Chapter-20 exported schema. Scalar/IN output
shape, scalar runtime cardinality, EXISTS result, IN comparison result, and derived
exported-name restrictions are asserted through the complete Subquery Tests rather than
redefined.

Qualified and unqualified outer-only references produce UnsupportedCorrelation only after
the local lookup rules run. Local qualifier shadowing, missing local member, and local
ambiguity suppress outer fallback. No OuterRef, sibling LATERAL visibility, correlated
bound expression, or downstream SQL name lookup is produced.

### V19-19 — Error categories and deterministic precedence

Populate the error matrix below with one row per condition. Verify CatalogError, BindError,
TypeError, ConstraintDefinitionError, UnsupportedFeature, downstream CardinalityError,
FrontEndResourceLimit, and OutOfMemory without category drift. Call registry failures are
type/coercion class; resolved aggregate placement is structural-placement class.

Only errors with satisfied semantic prerequisites enter the independent candidate list.
Sort genuine ordinary candidates by earliest SourceSpan start, then shorter span, then the
identical-span class order `name, placement/shape, constraint, type/coercion, cardinality`.
Use cross-clause errors and synthetic equal-start/equal-span fixtures. Permute binder
traversal, descriptor order, and containers; the selected error remains unchanged.
Resource/cancellation behavior is immediate under Chapters 18 and 39 and is not reordered
behind an ordinary semantic diagnostic.

### V19-20 — Immutability, source lifetime, and resource cleanup

Completed bound expressions are immutable. A rewrite creates a new expression or shares
immutable children; no assertion prescribes tree/arena/refcount ownership. Use generation-
tagged source backing to prove borrowed output names and diagnostics remain valid only while
backing survives, or are materialized before release; numeric SourceSpan intervals may
survive independently.

Inject checked budget refusal and required-allocation failure after relation identity,
scope, cast, output, aggregate, and error-candidate creation. Expect
FrontEndResourceLimit/OutOfMemory, complete private unwind, no successful partial bound
object, no persistent state, and no executable prefix. Reuse the Chapter-18 resource and
§39 transaction oracles.

### V19-21 — Environment and representation determinism

Replay each canonical fixture under permuted descriptor/cache storage, hash seeds, alias
insertion, allocation addresses, opaque BindingId values, scheduler event orders, locales,
and byte-character signedness simulation. Alpha-renamed canonical bound results and errors
must match. Identifier comparison uses Chapter-18 canonical bytes only—no locale folding,
Unicode normalization, quoted-name folding, pointer identity, or cache/container order.

### V19-22 — Cross-owner composition

The composition matrix below verifies that Chapter 19 consumes rather than redefines the
canonical Chapter-16 catalog, Chapter-17 type, Chapter-18 syntax/lifetime, Chapter-20
logical/subquery, Chapter-21 statement, Chapter-29 aggregate, and §39 error contracts.

The owner ledger also verifies that DISTINCT is handed off as a logical duplicate-
elimination requirement, only searched CASE is admitted, and IN/NOT IN retains its semantic
node and once-only left-to-right boundary without requiring an OR rewrite. Parameter syntax
has no binder semantics, a no-FROM block has an empty relation namespace, and binding
does not alter §39.1 transaction consequences.

### Mandatory procedural matrices

#### Bound-expression boundary matrix

| Contract | Independent expected result | Forbidden result |
|---|---|---|
| admitted kinds | Constant, ColumnRef, Unary, Binary, Comparison, Boolean, Cast, Function, Aggregate, Case, IsNull, InList, Subquery | parser-only or physical-plan node |
| common metadata | resolved return type, conservative nullability, SourceSpan | unresolved name/type or missing span |
| semantic identity | resolved IDs, descriptor/operator identity, casts/provenance, ordered children | executor-time SQL lookup |
| execution handoff | immutable typed expression compatible with vector evaluation | operator scheduling or pipeline ownership |
| side effects | none | cardinality estimation, heap/RID access, WAL, persistent ID allocation, catalog/database mutation |
| failure | private state discarded | executable partial bound statement |

#### BindingId matrix

| Relation case | TableId | BindingId oracle | ColumnId behavior | Qualifier | Persistence/rewrite |
|---|---|---|---|---|---|
| one base table | descriptor ID | one occurrence identity | descriptor column | final name | runtime; preserve if occurrence survives |
| two base tables | respective IDs | distinct | respective | each exposed name | same rule |
| aliased base | descriptor ID | one | unchanged | alias only | base qualifier hidden |
| self-join aliases | same | distinct | may be same | aliases distinct | both survive independently |
| self-join unaliased | same | second insertion fails | N/A | duplicate final name | BindError |
| derived table | none as base identity | distinct relation occurrence | exported slots/identity | mandatory alias | statement-local |
| scalar/EXISTS/IN child relations | descriptor IDs | distinct from every parent/child occurrence | descriptor columns | child-local | whole-statement domain |
| repeated table in child blocks | same | all distinct | may be same | block-local | no scope-reset reuse |

#### Qualifier and name-resolution matrix

| Case | Local candidates/qualifier | Outer diagnostic lookup | Result | Category/span |
|---|---|---|---|---|
| unaliased `main.t` | qualifier `t` | no | exposed | — |
| `t AS x` | `x` only | no | `t` hidden | — |
| duplicate qualifier | insertion conflict | no | failure | BindError/conflicting qualifier |
| unqualified zero | 0 | yes | outer-only unsupported; absent unknown | UnsupportedFeature or BindError/reference |
| unqualified one | 1 | no | exact bound column | — |
| unqualified many | >1 | no | ambiguity | BindError/reference |
| qualified local/member present | qualifier then member | no | exact bound column | — |
| qualified local/member absent | qualifier present | no | local unknown member | BindError/member |
| qualified outer only | local qualifier absent | yes | unsupported correlation | UnsupportedFeature/reference |
| local shadows outer | local authoritative | no | local result/error | local span |

#### Clause-visibility matrix

| Context | Input relations | Current JOIN right | Future relation | SELECT alias | Outer binding | Aggregate | Ordinal |
|---|---|---|---|---|---|---|---|
| FROM primary | none from siblings; a derived child is child-local | N/A | no | no | diagnostic only | child rules | no |
| JOIN ON | accumulated left | yes | no | no | diagnostic only | forbidden | no |
| WHERE | completed FROM | N/A | N/A | no | diagnostic only | forbidden | no |
| GROUP BY | completed FROM | N/A | N/A | no | diagnostic only | forbidden | no |
| HAVING | completed FROM/group keys | N/A | N/A | no | diagnostic only | allowed | no |
| SELECT item | completed FROM | N/A | N/A | no same-list | diagnostic only | allowed | no |
| ORDER BY | completed FROM fallback | N/A | N/A | explicit AS only | diagnostic only | allowed | bare-int ordinal |
| LIMIT | none | N/A | N/A | no | no | forbidden | no |
| OFFSET | none | N/A | N/A | no | no | forbidden | no |

#### Wildcard and output matrix

| Form | Resolution/order | Display name | Alias namespace | Duplicate result |
|---|---|---|---|---|
| `*` | relation source order × descriptor presentation order | source columns | no unless explicit AS impossible here | preserved |
| `x.*` | one qualifier, descriptor presentation order | source columns | no | preserved |
| self-join `*` | each occurrence in source order | may duplicate | no | distinct BindingIds/slots |
| direct column | exact bound column | source column | no | legal |
| direct column `AS x` | exact bound column | `x` | yes | legal |
| expression | bound expression | exact SourceSpan slice | no | legal |
| expression `AS x` | bound expression | `x` | yes | legal |
| duplicate explicit aliases | ordered expressions | duplicate `x` | two candidates | top-level legal; ORDER `x` ambiguous |

#### Type-resolution matrix

| Form | Raw/context | Oracle | Bound result | Failure owner |
|---|---|---|---|---|
| type name | canonical identifier | TypeId registry | exact TypeId | TypeError |
| literals | raw spelling/kind | Chapter-17 literal tables | typed constant/unresolved NULL | Chapter 17 |
| NULL | contextual candidate | TypeResolver | concrete typed NULL | TypeError if underconstrained |
| unary/arithmetic | operand types | §§17.6, 17.10.1 | operator + casts + type | TypeError |
| comparison/BOOLEAN | operand/context | §§17.7, 17.10.1 | BOOLEAN/nullability | TypeError |
| CASE/IN | ordered children | §§17.8.5, 17.9 | common type + casts | TypeError |
| explicit CAST | source/target | §17.8 | explicit cast/full span | TypeError or scalar category |
| implicit cast | resolver edge | §17.8.5 | implicit cast/operand span | TypeError if no edge |
| function/aggregate | descriptor candidates | §§17.9.3, 29.3 | one descriptor | TypeError |
| assignment | destination type | §17.8.5 assignment graph | destination-typed expression | TypeError |
| LIMIT/OFFSET | integral context | §§17.8.5, 17.10.2 | INT64 folded/residual | BindError/TypeError/scalar error |

#### Nullability matrix

| Bound form | Independent expected nullability/type |
|---|---|
| NOT NULL column reference without contextual null extension | non-nullable source type; LEFT output context follows V20-7 |
| nullable column reference | nullable source type |
| arithmetic `x op y` | nullable iff either operand is nullable |
| comparison `x cmp y` | nullable BOOLEAN iff either operand is nullable |
| `IS NULL(x)` | non-nullable BOOLEAN |
| `COUNT(*)` | non-nullable INT64 |
| `SUM(nullable_input)` | nullable registry result type |
| scalar subquery | nullable even if child output expression is NOT NULL |
| EXISTS / NOT EXISTS | non-nullable BOOLEAN |
| IN / NOT IN subquery | conservatively nullable BOOLEAN |

#### Function-descriptor and volatility matrix

| Condition | Independent expected result |
|---|---|
| resolved descriptor | canonical name, signature/rule, return rule, volatility, NULL rule, runtime implementation identity |
| implementation identity | process/runtime dispatch metadata, no executor name lookup |
| persisted catalog default | typed scalar only; no implementation identity |
| IMMUTABLE + constant arguments | eligible for ordinary folding |
| IMMUTABLE + nonconstant argument | not eligible through volatility alone |
| STABLE | not admitted by the IMMUTABLE folding gate |
| VOLATILE | never folded as stable |
| named scalar registry | empty |

#### Aggregate and grouping matrix

| Case | Aggregate block? | Placement/group result | Category/ordinal |
|---|---|---|---|
| GROUP BY, no aggregate | yes | grouped legality | no aggregate ordinal |
| aggregate SELECT, no GROUP | yes/global group | legal if group-valid | source ordinal |
| aggregate HAVING/ORDER | yes | legal | source ordinal |
| HAVING alone nonaggregate | no | illegal | BindError |
| aggregate WHERE/JOIN/GROUP/LIMIT | classification after resolution | forbidden | BindError |
| aggregate DML/default | owning block/context | forbidden | BindError |
| nested same-block aggregate | yes | forbidden | BindError |
| aggregate in child subquery | child only | parent unaffected | child ordinal domain |

#### Group-key equality matrix

| Pair | Structurally equal? | Reason/group result |
|---|---|---|
| `a` / `a` same bound identity | yes | exact key |
| `a` / `(a)` | yes | parenthesis-only provenance ignored |
| `a+b` / same bound `a+b` | yes | child/order/descriptor identities equal |
| `a+b` / `b+a` | no | no commutativity |
| `a` / `a+0` | no | no algebraic equivalence |
| same ColumnId under different BindingIds | no | relation occurrence differs |
| implicit-cast(`a`) / `a` | no | cast is semantic |
| same value/different TypeId | no | type is semantic |
| same tree/different SourceSpan | yes | span ignored |

#### ORDER BY matrix

| Source | Ordinal? | Alias lookup | Input fallback | Expected |
|---|---|---|---|---|
| `1` | yes | no | no | output 1 or range BindError |
| `0`, `N+1`, huge integer | yes | no | no | BindError |
| `(1)`, `+1`, `-1`, `1+0`, `CAST(1 AS INT32)`, `1.0` | no | only identifier forms qualify | yes | ordinary expression result |
| unique explicit alias `x` | no | one | no | existing output identity |
| duplicate explicit alias `x` | no | many | no | BindError |
| no alias, input `x` | no | zero | yes | input expression |
| alias and input `x` | no | one | no | alias wins |

#### LIMIT/OFFSET matrix

| Source | Constant? / type | Binding/fold | Execution-start result |
|---|---|---|---|
| `1`, INT32/INT64 literal | yes / integral | INT64 folded/residual | accepted |
| `1+2` | yes / INT64 | folds to 3 | accepted 3 |
| `1/0` | yes / integral | DIVISION_BY_ZERO | not reached |
| `INT64_MAX+1` | yes / integral | NUMERIC_OVERFLOW | not reached |
| `NULL` | yes / INT64 NULL | typed/folded NULL | ExecutionError |
| `-1` | yes / INT64 | folded/residual -1 | ExecutionError |
| column/output alias/aggregate/subquery | no | BindError | not reached |
| FLOAT or other nonintegral | eligibility independent | TypeError | not reached |
| explicit legal CAST to INT64 | recursive | folded/residual | value/domain result |
| constant CASE | recursive | §17.10.2 result/error | value/domain result |

#### DML matrix

| Context | Target row visible? | Qualifier/unqualified names | Coercion/aggregate | Row image |
|---|---|---|---|---|
| INSERT VALUES | no | no target names | assignment after expression; aggregate forbidden | new row only in RETURNING |
| INSERT SELECT | no target in child | child SELECT scope | target coercion after child | new row in RETURNING |
| UPDATE SET RHS/WHERE | yes | final table name + unqualified | assignment/BOOLEAN; aggregate forbidden | final new row |
| UPDATE RETURNING | yes | same target | aggregate forbidden | final new row |
| DELETE WHERE | yes | final table name + unqualified | BOOLEAN; aggregate forbidden | old row |
| DELETE RETURNING | yes | same target | aggregate forbidden | old row |

#### DDL matrix

| Form | Namespace/validation | Failure | Downstream owner |
|---|---|---|---|
| CREATE TABLE column/type | declaration namespace/type registry | CatalogError/TypeError | §21.6 |
| PK/UNIQUE member | declaration namespace | ConstraintDefinitionError | §§21.6–21.7 |
| repeated member/second PK | statement constraint | ConstraintDefinitionError | §§21.6–21.7 |
| DEFAULT | closed expression/type | ConstraintDefinitionError/type owner | §21.12 |
| CREATE INDEX key | target descriptor/source order | unknown/catalog, duplicate BindError, type TypeError | §21.8 |
| DROP TABLE/INDEX | requested object kind | CatalogError missing/wrong kind | §21.9 |
| VACUUM/ANALYZE | table snapshot lookup | catalog owner | maintenance owner |
| EXPLAIN | ordinary inner SELECT binding | same inner error | Chapter 40 |

#### Subquery matrix

| Form | Child scope/BindingId | Outer lookup | Bound result | Downstream owner |
|---|---|---|---|---|
| scalar | independent/statement-unique | diagnostic only | typed subquery | §20.14.4 cardinality |
| EXISTS | independent/statement-unique | diagnostic only | non-NULL BOOLEAN handoff | §20.14.5 |
| IN | independent/statement-unique | diagnostic only | comparison-type handoff | §20.14.6 |
| derived | independent/relation BindingId | no sibling visibility | aliased exported schema | §20.14.3 |
| correlated qualified/unqualified | local rules first | outer match only diagnoses | UnsupportedCorrelation | §20.14.2 |
| local alias shadows outer | local authoritative | none | local result/error | §19.18 |
| local ambiguity plus outer match | ambiguity | none | BindError | §19.18 |

#### Error-category matrix

| Condition | Prerequisite | Category | Responsible span | Class |
|---|---|---|---|---|
| missing table | table lookup attempted | CatalogError | table name | name |
| missing index | index lookup attempted | CatalogError | index name | name |
| wrong DROP object kind | requested-kind lookup attempted | CatalogError | object name | name |
| create-name/object collision | snapshot lookup complete | CatalogError | object name | name |
| unknown column | local/outer diagnostic lookup complete | BindError | reference | name |
| ambiguous unqualified column | complete local candidate set | BindError | complete reference | name |
| unknown qualifier | local/outer diagnostic lookup complete | BindError | qualifier | name |
| known qualifier, unknown member | qualifier resolved | BindError | member | name |
| duplicate qualifier | prior qualifier installed | BindError | later qualifier | name |
| ambiguous output alias | output aliases bound | BindError | ORDER identifier | name |
| ordinal zero | output width known | BindError | ORDER integer | placement/shape |
| ordinal above output width | output width known | BindError | ORDER integer | placement/shape |
| illegal aggregate placement | call resolved | BindError | aggregate expression | placement/shape |
| nested same-block aggregate | both calls resolved/owned | BindError | nested aggregate | placement/shape |
| grouped-query legality failure | group keys/expressions bound | BindError | responsible expression | placement/shape |
| HAVING legality failure | aggregate classification complete | BindError | HAVING clause/expression | placement/shape |
| nonconstant LIMIT/OFFSET | expression bound | BindError | count expression | placement/shape |
| duplicate INSERT target | target descriptor known | BindError | later target | placement/shape |
| duplicate UPDATE assignment | target descriptor known | BindError | later assignment target | placement/shape |
| duplicate CREATE INDEX key | target descriptor known | BindError | later key | placement/shape |
| unknown type | type syntax present | TypeError | type name | type/coercion |
| operator overload mismatch | operands bound | TypeError | operator expression | type/coercion |
| cast/coercion mismatch | source/target known | TypeError | cast/coerced expression | type/coercion |
| non-BOOLEAN predicate | expression typed | TypeError | predicate expression | type/coercion |
| incompatible CASE common type | arms bound | TypeError | CASE expression | type/coercion |
| incompatible IN common type | operands bound | TypeError | IN expression | type/coercion |
| unknown generic call | arguments bound as required | TypeError | call | type/coercion |
| wrong call shape or arity | arguments bound as required | TypeError | call | type/coercion |
| unauthorized star call | star raw form bound | TypeError | call | type/coercion |
| call argument type mismatch | arguments bound | TypeError | call | type/coercion |
| unresolved overload ambiguity | candidates typed | TypeError | call | type/coercion |
| nonintegral LIMIT/OFFSET | expression typed | TypeError | count expression | type/coercion |
| index-ineligible key type | key resolved/typed | TypeError | key | type/coercion |
| missing PK/UNIQUE member | declaration namespace built | ConstraintDefinitionError | member | constraint |
| repeated constraint member | declaration namespace built | ConstraintDefinitionError | later member | constraint |
| multiple PRIMARY KEY declarations | declarations parsed/bound | ConstraintDefinitionError | later declaration | constraint |
| invalid default definition | default syntax/type prerequisites | ConstraintDefinitionError owner | default expression | constraint |
| outer-only reference | local miss and outer match | UnsupportedFeature | reference | placement/shape |
| scalar runtime row cardinality | child executes | CardinalityError owner | subquery | cardinality |
| deliberate front-end budget | guard reached | FrontEndResourceLimit | operational | immediate |
| required allocation fails | allocation attempted | OutOfMemory | operational | immediate |

#### SourceSpan and precedence matrix

| Condition | Canonical span | Tie/class behavior |
|---|---|---|
| unknown qualifier | qualifier | name class |
| unknown qualified member | member | name class |
| ambiguous unqualified column | complete reference | name class |
| duplicate qualifier/target/key | later conflicting construct | name or placement class |
| invalid ordinal | complete ordinal expression | placement class |
| aggregate placement | aggregate expression | placement class |
| type/call mismatch | responsible expression/call | type class |
| constraint member | member/declaration | constraint class |
| LIMIT nonconstant | count expression | placement class |
| implicit cast metadata | coerced operand | provenance, not error candidate itself |
| explicit CAST metadata | complete CAST expression | provenance, not error candidate itself |

#### Catalog and determinism matrix

| Perturbation | Candidate set | Presentation order | Selected error/result | Canonical authority |
|---|---|---|---|---|
| descriptor/cache storage order | unchanged | unchanged | unchanged | snapshot + source/presentation order |
| hash seed/container order | unchanged | unchanged | unchanged | complete sets + explicit sorter |
| TableId/ColumnId numeric order | identities only | unchanged | unchanged | descriptors/§16.8 |
| source relation order | defined change | defined change | defined semantic change | raw AST |
| descriptor presentation order | columns unchanged | defined change | defined semantic change | §16.8 |
| alias insertion order | source conflict span only | N/A | source-defined | qualifier namespace |
| allocation/pointer addresses | unchanged | unchanged | unchanged | explicit semantic identity |
| locale/Unicode environment | unchanged | unchanged | unchanged | canonical bytes |
| scheduler interleaving | unchanged | unchanged | unchanged | deterministic semantic contract |

#### Cross-chapter composition matrix

| Owner | Chapter-19 handoff | Existing verification reused | New composition |
|---|---|---|---|
| Chapter 16 | names, IDs, descriptors, SchemaVer, snapshot/presentation order | Catalog Tests | bind selected identities |
| Chapter 17 | types, NULL, coercions, calls, folding/errors | Type-System Property Tests | inspect bound tree/count pipeline |
| Chapter 18 | raw AST, provenance, spans, lifetime/resources | Chapter 18 verification | raw-to-bound handoff |
| Chapter 19 | identity, scopes, resolution, legality, precedence | this section | direct owner |
| Chapter 20 | slots, grouping/DISTINCT/ORDER/LIMIT/subqueries | Subquery/Logical Planner Tests | handoff shape/identity |
| Chapter 21 | DML/DDL/default/RETURNING/publication | Catalog and DML tests | namespace/validation only |
| Chapter 29 | aggregate descriptors/signatures/ordinal | Aggregate Tests | resolution/placement/ordinal |
| §39.1 | transaction consequences | Statement Failure tests | no redefinition |
| §39.2 | front-end categories | Front-End Error tests | exact binder map |
| §39.3 | execution/resource categories | Execution-failure tests | LIMIT final/resource handoff |
| §41.4 | required binder verification families | this section/matrices | owner-to-procedure map |

#### High-level Chapter-19 case matrix

| Case | Independent oracle | Expected result | Family |
|---|---|---|---|
| self-join with aliases | occurrence graph | same TableId, distinct BindingIds | V19-2 |
| unaliased self-join | qualifier map | duplicate-qualifier BindError | V19-3 |
| alias hides base name | qualifier map | alias only | V19-3 |
| two distinct relations expose one duplicate qualifier | qualifier map | later-qualifier BindError | V19-3 |
| local shadows outer | scope graph | local result/error | V19-3 |
| outer-only reference | scope graph | UnsupportedCorrelation | V19-3/18 |
| ambiguous unqualified column | candidate set | BindError | V19-4 |
| self-join wildcard | order cross-product | duplicate names, distinct identities | V19-6 |
| expression display name | byte-slice oracle | exact source bytes | V19-6 |
| duplicate explicit aliases | output/alias models | legal outputs; ORDER ambiguity | V19-6/7 |
| ORDER BY duplicate explicit alias | alias candidate set | ambiguous-output-alias BindError | V19-7 |
| unknown type | TypeId table | TypeError/type span | V19-9 |
| underconstrained standalone NULL | TypeResolver table | TypeError | V19-9 |
| implicit/explicit cast | TypeResolver/span table | exact cast/provenance | V19-10 |
| `foo()`, `foo(*)`, illegal `sum(*)` | call descriptor model | TypeError | V19-11 |
| `count(*)` | aggregate table | bound aggregate | V19-11/12 |
| aggregate in WHERE/nested | placement/block oracle | BindError | V19-12 |
| HAVING alone | aggregate classifier | BindError | V19-13 |
| GROUP `a`, SELECT `a+1` | structural recursion | valid | V19-13 |
| GROUP `a+1`, SELECT `a` | structural recursion | BindError | V19-13 |
| ORDER `1`, `(1)`, `+1`, `0`, huge | provenance/magnitude model | exact ordinal/ordinary outcomes | V19-14 |
| LIMIT `1+2`, `1/0`, overflow, NULL, -1 | fold/domain models | exact bind/execution outcomes | V19-15 |
| LIMIT column/FLOAT | dependency/type models | BindError/TypeError | V19-15 |
| INSERT VALUES target reference | DML namespace | target not visible | V19-16 |
| UPDATE qualification/DELETE RETURNING | DML namespace/row image | exact target/new-old binding | V19-16 |
| repeated index key/wrong DROP kind | DDL tables | BindError/CatalogError | V19-17 |
| correlated derived/scalar child | scope graph | UnsupportedCorrelation | V19-18 |
| multiple independent errors | candidate sorter | canonical one error | V19-19 |
| allocation failure mid-bind | failure schedule/ownership graph | resource error, no partial bound object | V19-20 |
| permuted locale/container/address | canonical serialization | identical result/error | V19-21 |

### Complete Chapter-19 architecture-obligation coverage map

Each row below is one independently checkable Chapter-19 obligation. The referenced deterministic procedure must have an expected-result oracle independent
of production traversal, or precisely reuses a lower-layer oracle with independent expected results. The
primary-family codes are the closed Chapter-19 inventory domains:

| Code | Primary obligation family |
|---|---|
| A | BINDER INPUT CONTRACT |
| B | RAW-AST → BOUND REPRESENTATION |
| C | BINDINGID DOMAIN |
| D | BINDINGID LIFETIME |
| E | TABLEID / COLUMNID DISTINCTION |
| F | QUERY-BLOCK SCOPE |
| G | RELATION QUALIFIER NAMESPACE |
| H | ALIAS HIDING |
| I | LOCAL / OUTER SHADOWING |
| J | CORRELATION DIAGNOSTIC LOOKUP |
| K | UNQUALIFIED COLUMN RESOLUTION |
| L | QUALIFIED COLUMN RESOLUTION |
| M | AMBIGUITY |
| N | CATALOG SNAPSHOT |
| O | SCHEMAVER / DESCRIPTOR IDENTITY |
| P | WILDCARD EXPANSION |
| Q | WILDCARD ORDER |
| R | OUTPUT METADATA |
| S | OUTPUT DISPLAY NAMES |
| T | DUPLICATE OUTPUT NAMES |
| U | SELECT OUTPUT-ALIAS NAMESPACE |
| V | CLAUSE VISIBILITY |
| W | FROM/JOIN VISIBILITY |
| X | TYPE-NAME RESOLUTION |
| Y | LITERAL BINDING |
| Z | NULL CONTEXT |
| AA | TYPERESOLVER HANDOFF |
| AB | IMPLICIT CAST INSERTION |
| AC | CAST PROVENANCE |
| AD | ASSIGNMENT COERCION |
| AE | OPERATOR RESOLUTION |
| AF | BOOLEAN CONTEXTS |
| AG | CASE BINDING |
| AH | IN / NOT IN BINDING |
| AI | FUNCTION REGISTRY |
| AJ | STAR-CALL RESOLUTION |
| AK | AGGREGATE CLASSIFICATION |
| AL | AGGREGATE PLACEMENT |
| AM | NESTED AGGREGATES |
| AN | AGGREGATE ORDINAL |
| AO | GROUPED-QUERY CLASSIFICATION |
| AP | GLOBAL GROUP |
| AQ | HAVING LEGALITY |
| AR | GROUP-KEY EQUALITY |
| AS | GROUP-VALID EXPRESSION |
| AT | NO FUNCTIONAL DEPENDENCIES |
| AU | ORDER BY ALIAS RESOLUTION |
| AV | ORDER BY ORDINAL RECOGNITION |
| AW | ORDER BY ORDINAL RANGE |
| AX | ORDER BY RESOLUTION PRIORITY |
| AY | LIMIT/OFFSET CONSTANT ELIGIBILITY |
| AZ | LIMIT/OFFSET TYPE CONTEXT |
| BA | LIMIT/OFFSET CONSTANT FOLDING |
| BB | LIMIT/OFFSET EXECUTION-START CONSUMPTION |
| BC | LIMIT/OFFSET FINAL DOMAIN VALIDATION |
| BD | INSERT BINDING |
| BE | UPDATE BINDING |
| BF | DELETE BINDING |
| BG | RETURNING BINDING |
| BH | CREATE TABLE BINDING |
| BI | CREATE INDEX BINDING |
| BJ | DROP BINDING |
| BK | VACUUM / ANALYZE TARGET BINDING |
| BL | EXPLAIN BINDING |
| BM | SCALAR SUBQUERY |
| BN | EXISTS |
| BO | IN SUBQUERY |
| BP | DERIVED TABLE |
| BQ | UNSUPPORTED CORRELATION |
| BR | BINDER ERROR CATEGORY MAP |
| BS | ERROR PREREQUISITES |
| BT | SOURCE-SPAN ERROR PRECEDENCE |
| BU | SAME-SPAN PRIORITY |
| BV | RESOURCE FAILURE |
| BW | NO PARTIAL BOUND OBJECT |
| BX | BINDER DETERMINISM |
| BY | CONTAINER / CATALOG ORDER INDEPENDENCE |
| BZ | LOCALE / PLATFORM INDEPENDENCE |
| CA | BOUND-EXPRESSION IMMUTABILITY |
| CB | REPRESENTATION FREEDOM |
| CC | SOURCE-BACKING LIFETIME |
| CD | CROSS-CHAPTER COMPOSITION |
| CE | VERIFICATION METHODOLOGY |
| CF | OTHER |

| Atomic ID | Primary family | Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Independent oracle |
|---|---|---|---|---|---|---|
| V19-1.1 | A | Input is one Chapter-18 unbound raw AST | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.2 | A | Canonical identifier bytes reach binding unchanged | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.3 | A | Raw SourceSpans reach binding unchanged | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.4 | A | Parenthesis provenance reaches binding unchanged | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.5 | A | Integer-literal provenance reaches binding unchanged | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.6 | A | Binder does not reinterpret grammar | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.7 | B | Success is a fully resolved typed semantic representation | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.8 | B | Success includes conservative nullability | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.9 | B | Success includes all downstream semantic identities | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.10 | B | Physical access and join algorithms are excluded | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.11 | B | Heap pages and RIDs are not consulted by ordinary binding | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.12 | B | Binding writes no WAL | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.13 | B | Binding mutates no database state | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.14 | B | Binding does not estimate cardinality | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.15 | B | Executor-time SQL name resolution is forbidden | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.16 | B | Failure publishes no successful executable bound prefix | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.17 | B | Bound nodes use only the bound-expression kind inventory | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.18 | B | Every bound expression carries a resolved return type | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.19 | B | Every bound expression carries conservative nullability | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.20 | B | Every bound expression carries its SourceSpan | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.21 | B | Execution receives resolved types and performs no SQL type inference | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-1.22 | B | Expression nodes own neither operator scheduling nor pipeline state | §§19.1, 19.3, 19.6 | V19-1 | V19-1 boundary procedure | raw-tree/bound serialization |
| V19-2.1 | C | Every base-table occurrence receives one BindingId | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.2 | C | Every derived-table relation occurrence receives one BindingId | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.3 | C | Every other visible relation-like occurrence uses the same identity rule | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.4 | C | BindingId uniqueness spans the whole top-level statement | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.5 | C | Nested query blocks share that uniqueness domain | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.6 | D | Child-scope completion does not permit identity reuse | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.7 | C | Distinct relation occurrences never compare equal | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.8 | D | Different statements may reuse opaque values | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.9 | D | BindingId is runtime-only | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.10 | D | BindingId is absent from catalog state | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.11 | D | BindingId is absent from WAL | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.12 | D | BindingId has no recovery identity | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.13 | E | Self-join occurrences may share TableId | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.14 | E | Self-join occurrences have distinct BindingIds | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.15 | E | Corresponding self-join columns may share ColumnId | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.16 | E | Bound column identity cannot reduce to TableId or ColumnId alone | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.17 | D | A surviving relation occurrence retains BindingId through logical construction/rewrites | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-2.18 | D | A newly distinct relation occurrence cannot inherit an existing BindingId | §§19.2–19.3 | V19-2 | V19-2 occurrence/rewrite fixtures | whole-statement occurrence graph |
| V19-3.1 | F | Each query block has exactly one local qualifier namespace | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.2 | G | Qualifier keys use canonical identifier bytes | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.3 | H | Unaliased main.t exposes final component t | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.4 | H | Aliased t AS x exposes only x | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.5 | H | An alias hides the base-table qualifier | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.6 | H | A derived table exposes only its mandatory alias | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.7 | G | Every exposed local qualifier is unique | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.8 | G | A later duplicate qualifier produces BindError | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.9 | G | The conflicting qualifier span is responsible | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.10 | G | No first or last qualifier wins | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.11 | G | No automatic or numeric disambiguator is invented | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.12 | G | Distinct aliases make a self-join legal | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.13 | H | An unaliased self-join with duplicate qualifier fails | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.14 | I | A local qualified binding is authoritative | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.15 | J | A missing member on a local qualifier does not search outer scope | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.16 | I | Unqualified lookup collects the complete local candidate set first | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.17 | J | Zero local candidates permit diagnostic outer lookup | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.18 | I | One local candidate binds locally | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.19 | J | Multiple local candidates produce local ambiguity without outer lookup | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.20 | J | Local miss plus outer match is UnsupportedCorrelation | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-3.21 | J | Absence in local and outer scopes is ordinary BindError | §§19.2, 19.4, 19.18 | V19-3 | V19-3 qualifier/scope fixtures | local qualifier maps and parent diagnostic links |
| V19-4.1 | K | Unqualified zero matches is unknown-column BindError | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.2 | K | Unqualified one match binds that exact occurrence/column | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.3 | M | Unqualified multiple matches is ambiguous-column BindError | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.4 | M | No leftmost match is accepted | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.5 | L | Qualified lookup resolves qualifier before member | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.6 | L | Unknown qualifier is distinct from unknown member | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.7 | L | Unknown qualifier uses the qualifier span | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.8 | L | Known qualifier with unknown member uses the member span | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.9 | E | Bound columns carry BindingId TableId ColumnId type nullability and span | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-4.10 | M | Descriptor/hash iteration cannot select a column result | §§19.3–19.4 | V19-4 | V19-4 column-resolution matrix | complete candidate cardinality model |
| V19-5.1 | O | Binding consumes transaction-visible immutable descriptors | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.2 | N | Own completed earlier DDL is visible under the frozen snapshot rules | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.3 | N | Another transaction's uncommitted DDL is invisible | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.4 | O | Selected TableId is stable | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.5 | O | Selected ColumnId is stable | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.6 | O | Selected SchemaVer/descriptor identity is stable for the bound statement | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.7 | N | Catalog cache layout cannot override snapshot selection | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.8 | O | Cache hit and cache miss bind to equivalent canonical identity | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.9 | N | Same AST and same snapshot produce the same selected identities | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-5.10 | N | Table/index/DROP lookups distinguish the requested object kind | §§19.1–19.3; §§16.6, 16.10, 21.3 | V19-5 | V19-5 catalog-snapshot composition | Chapter-16/21 snapshot model |
| V19-6.1 | P | SELECT star expands during binding | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.2 | P | Unqualified star requires at least one visible FROM relation | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.3 | P | No-FROM unqualified star is BindError | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.4 | P | Qualified star resolves exactly one local qualifier | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.5 | P | Unknown qualified-star qualifier is BindError | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.6 | Q | Wildcard relation order is source relation-occurrence order | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.7 | Q | Wildcard column order is descriptor presentation order | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.8 | Q | TableId numeric order does not control expansion | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.9 | Q | ColumnId numeric order does not control expansion | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.10 | P | Execution performs no wildcard expansion | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.11 | R | Every output records an ordered bound expression | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.12 | S | Every output records a display name | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.13 | R | Every output records logical type | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.14 | R | Every output records nullability | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.15 | S | Explicit AS alias has first display-name priority | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.16 | S | Direct bound source column has second display-name priority | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.17 | S | Generated names preserve source spelling | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.18 | S | Generated names omit binder-inserted cast text | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.19 | T | Duplicate top-level display names are legal | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.20 | U | Generated/direct unaliased display names do not enter alias lookup | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.21 | CC | Generated display names obey source-backing lifetime | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.22 | T | Duplicate outputs remain distinct ordered slots | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.23 | S | Generated names are the exact original source-byte slice designated by SourceSpan | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.24 | S | Generated names use no normalization pretty printer or reconstructed SQL | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.25 | T | Duplicate top-level display names and explicit aliases remain legal distinct slots | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-6.26 | U | Generated names are presentation metadata with no persistent format or binder lookup identity | §§19.5–19.7 | V19-6 | V19-6 wildcard/output procedures | order cross-product and source-slice model |
| V19-7.1 | U | Only explicit SELECT AS aliases enter the output-alias namespace | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.2 | U | Generated display names are excluded from output-alias lookup | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.3 | U | Direct-column display names without AS are excluded | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.4 | V | SELECT aliases are visible in ORDER BY | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.5 | V | SELECT aliases are not visible in the same SELECT list | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.6 | V | SELECT aliases are not visible in JOIN ON | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.7 | V | SELECT aliases are not visible in WHERE | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.8 | V | SELECT aliases are not visible in GROUP BY | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.9 | V | SELECT aliases are not visible in HAVING | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.10 | V | SELECT aliases are not visible in LIMIT | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.11 | V | SELECT aliases are not visible in OFFSET | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.12 | AU | Zero explicit ORDER alias matches falls back to input binding | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.13 | AU | One explicit ORDER alias match references the existing output identity | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.14 | AU | Multiple explicit ORDER alias matches produce BindError | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.15 | AU | ORDER alias binding does not rebind or reevaluate the output expression | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.16 | V | GROUP BY has no output-alias shortcut | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-7.17 | V | GROUP BY has no ordinal shortcut | §§19.2.2, 19.5, 19.13 | V19-7 | V19-7 clause/alias matrix | closed clause visibility and alias candidate tables |
| V19-8.1 | W | the architecture has no lateral table-reference visibility | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.2 | W | A derived table binds as an independent child block | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.3 | W | A derived child sees neither earlier nor later siblings | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.4 | W | A sibling reference uses unsupported-correlation diagnosis | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.5 | W | JOIN ON sees every accumulated-left relation | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.6 | W | JOIN ON sees the current right relation | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.7 | W | JOIN ON excludes future join relations | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-8.8 | W | Completed FROM scope feeds WHERE GROUP BY HAVING SELECT and ordinary ORDER fallback but not LIMIT/OFFSET | §19.2.1 | V19-8 | V19-8 FROM/JOIN visibility fixtures | left-associated relation-scope graph |
| V19-9.1 | X | Each recognized type name resolves to its exact frozen TypeId | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.2 | X | Unknown type name is TypeError at the type-name span | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.3 | Y | Integer literal classification delegates to Chapter 17 | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.4 | Y | FLOAT64 literal classification delegates to Chapter 17 | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.5 | Y | VARCHAR literal classification delegates to Chapter 17 | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.6 | Y | BOOLEAN literal classification delegates to Chapter 17 | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.7 | Z | Raw NULL begins with the unresolved marker rather than a persistent type | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.8 | Z | Comparison/cast context may resolve NULL | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.9 | Z | CASE/IN context may resolve NULL | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.10 | Z | Assignment context may resolve NULL | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.11 | Z | LIMIT/OFFSET resolves raw NULL to INT64 NULL | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.12 | Z | Standalone underconstrained NULL follows the frozen TypeError rule | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.13 | Z | No unresolved marker reaches an executable bound expression | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.14 | AA | Every bound expression has one resolved logical return type | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.15 | Z | Bound nullability follows the frozen conservative owner rules | §§19.6, 19.8, 19.16–19.17, 19.20.1 | V19-9 | V19-9 type/NULL composition | Chapter-17 TypeResolver tables |
| V19-9.16 | Z | A NOT NULL column reference without contextual null extension is non-nullable | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.17 | Z | A nullable column reference is nullable | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.18 | Z | Arithmetic nullability is the disjunction of operand nullability | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.19 | Z | Comparison nullability is the disjunction of operand nullability | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.20 | Z | IS NULL produces non-nullable BOOLEAN | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.21 | Z | COUNT star produces non-nullable INT64 | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.22 | Z | SUM over nullable input remains nullable | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.23 | Z | A scalar subquery remains nullable even for a non-nullable output expression | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.24 | Z | EXISTS and NOT EXISTS produce non-nullable BOOLEAN | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-9.25 | Z | IN and NOT IN subqueries produce conservatively nullable BOOLEAN | §19.6 | V19-9 | V19-9 nullability table | Chapter-17/20 nullability owner tables |
| V19-10.1 | AE | Operator selection uses only the closed Chapter-17 registries | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.2 | AA | TypeResolver owns coercion/promotion and overload selection | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.3 | AB | Required implicit casts appear explicitly in the bound tree | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.4 | AA | No unsupported cast chain is synthesized | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.5 | AB | An implicit cast is marked implicit | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.6 | AC | An implicit cast uses the coerced operand SourceSpan | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.7 | AA | An explicit CAST is marked explicit | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.8 | AC | An explicit CAST uses the complete CAST-expression SourceSpan | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.9 | AC | Explicit and implicit provenance remain distinguishable | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.10 | AC | Cast provenance is runtime-only and nonpersistent | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.11 | AC | Borrowed provenance obeys the Chapter-18 lifetime rule | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.12 | AD | INSERT/UPDATE assignment uses only the assignment-coercion graph | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.13 | AD | General explicit-cast legality is not borrowed for assignment | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.14 | AF | JOIN ON WHERE HAVING and CASE WHEN require BOOLEAN | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.15 | AF | Integer truthiness is absent | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.16 | AG | CASE preserves source order common typing contextual NULL and exact inserted casts | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-10.17 | AH | IN and NOT IN preserve source order common typing contextual NULL and exact inserted casts | §§19.6, 19.8, 19.16–19.17 | V19-10 | V19-10 cast/operator procedures | Chapter-17 resolver plus span table |
| V19-11.1 | AJ | Parser supplies only generic ordinary/star call shapes | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.2 | AI | Binder does not special-case source function names | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.3 | AI | Scalar and aggregate registries are both considered as defined | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.4 | AJ | Star calls consider only descriptors authorizing star | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.5 | AI | Exactly one legal descriptor is required | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.6 | AI | Unknown function name is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.7 | AI | Wrong arity is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.8 | AI | Unsupported call shape is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.9 | AJ | Unauthorized star form is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.10 | AI | Argument semantic-type mismatch is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.11 | AI | Unresolved overload ambiguity is TypeError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.12 | AI | Argument binding failure remains its prerequisite error | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.13 | AL | A resolved aggregate in an illegal context is placement BindError | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.14 | AJ | COUNT star resolves only through Chapter 29 | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.15 | AI | The named scalar registry stays empty | §§19.9, 19.20.1–19.20.2 | V19-11 | V19-11 call-resolution fixtures | scalar/aggregate descriptor filter |
| V19-11.16 | AI | Bound call metadata contains canonical name signature return rule volatility NULL handling and implementation identity | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.17 | AI | Function implementation identity is process/runtime dispatch identity | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.18 | AI | Catalog defaults do not persist function implementation identity | §19.9; §21.12 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.19 | AI | Volatility uses exactly IMMUTABLE STABLE and VOLATILE | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.20 | AI | Only IMMUTABLE calls with constant arguments qualify for ordinary folding | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.21 | AI | VOLATILE expressions are never folded as stable | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-11.22 | AI | Bound function nodes carry dispatch identity without executor-time name lookup | §19.9 | V19-11 | V19-11 descriptor-metadata table | scalar/aggregate descriptor filter |
| V19-12.1 | AK | The aggregate call inventory is exactly registry-driven | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.2 | AK | A bound aggregate is distinct from a scalar call | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.3 | AK | Aggregate argument/result/nullability use Chapter-29 signatures | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.4 | AL | Direct aggregate is legal in SELECT | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.5 | AL | Direct aggregate is legal in HAVING | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.6 | AL | Direct aggregate is legal in ORDER BY | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.7 | AL | Direct aggregate is forbidden in JOIN ON | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.8 | AL | Direct aggregate is forbidden in WHERE | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.9 | AL | Direct aggregate is forbidden in GROUP BY | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.10 | AL | Direct aggregate is forbidden in LIMIT | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.11 | AL | Direct aggregate is forbidden in OFFSET | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.12 | AL | Direct aggregate is forbidden in DML expressions and RETURNING | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.13 | AL | Direct aggregate is forbidden in defaults | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.14 | AM | Same-query-block nested aggregate is BindError | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.15 | AM | Child-query aggregate is not parent nesting | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.16 | AN | Aggregate ordinal is per query block | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.17 | AN | Ordinal follows first aggregate source-byte occurrence | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.18 | AN | Shared/aliased aggregate references do not create duplicate ordinals | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.19 | AN | Rewrites retain the aggregate ordinal | §§19.9–19.10; §29.3 | V19-12 | V19-12 aggregate placement/ordinal fixtures | aggregate registry and source-span sorter |
| V19-12.20 | AK | Aggregate DISTINCT and FILTER are absent from the aggregate registry | §19.10; §29.3 | V19-12 | V19-12 aggregate registry fixture | aggregate registry and source-span sorter |
| V19-13.1 | AO | GROUP BY presence classifies an aggregate query | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.2 | AO | A block-owned SELECT aggregate classifies the block | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.3 | AQ | A block-owned HAVING aggregate classifies the block | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.4 | AO | A block-owned ORDER aggregate classifies the block | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.5 | AO | A child-block aggregate does not classify the parent | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.6 | AP | Aggregate without GROUP BY creates one global group | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.7 | AQ | HAVING with GROUP BY is eligible | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.8 | AQ | HAVING with an aggregate in SELECT/HAVING/ORDER is eligible | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.9 | AQ | HAVING alone in a nonaggregate query is BindError | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.10 | AR | Group-key equality compares fully bound structure | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.11 | AR | SourceSpan is ignored by group-key equality | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.12 | AR | Parenthesis-only provenance is ignored | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.13 | AO | Output alias/display metadata is ignored | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.14 | AR | BindingId and catalog column identity participate | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.15 | AR | TypeId and literal value participate in structural equality | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.16 | AR | Operator/call descriptor identity participates in structural equality | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.17 | AR | Commutativity is excluded | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.18 | AR | Associativity is excluded | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.19 | AR | Algebraic/constant-folding equivalence is excluded | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.20 | AS | GROUP BY a admits a+1 | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.21 | AS | GROUP BY a+1 does not admit bare a | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.22 | AR | Commutativity associativity and algebraic equivalence are excluded | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.23 | AT | Functional dependencies keys and uniqueness are excluded | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.24 | AT | Recursive group validity admits exact keys aggregates and constants | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-13.25 | AS | Arbitrary selection of a non-grouped row-varying column is rejected | §§19.11–19.12 | V19-13 | V19-13 grouping classifier/comparator | independent bound structural model |
| V19-14.1 | AV | Only a bare unsigned integer literal can be an ordinal | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.2 | AV | The integer node must be the complete ORDER expression | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.3 | AV | Parenthesized integer is ordinary expression | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.4 | AV | Unary plus integer is ordinary expression | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.5 | AV | Unary minus integer is ordinary expression | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.6 | AV | Composed integer expression is ordinary | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.7 | AV | CAST integer is ordinary | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.8 | AV | FLOAT literal is ordinary | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.9 | AW | Ordinals are one-based | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.10 | AW | Ordinal zero is BindError | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.11 | AW | Ordinal greater than output width is BindError | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.12 | AW | Arbitrarily large integer remains an ordinal attempt | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.13 | AW | Host integer overflow cannot alter classification | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.14 | AX | Ordinal classification precedes alias lookup | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.15 | AX | Alias lookup precedes ordinary input fallback | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.16 | AU | Alias/input collision resolves to the explicit alias | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.17 | AE | ORDER accepts exactly the Chapter-17 SQL-orderable types | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-14.18 | AE | ORDER rejects BOOLEAN absent an explicit legal cast | §19.13 | V19-14 | V19-14 ORDER matrix | raw provenance and lossless magnitude model |
| V19-15.1 | AY | LIMIT/OFFSET has no relation-column namespace | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.2 | AY | LIMIT/OFFSET has no SELECT-alias namespace | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.3 | AY | Execution-start constant eligibility is recursive | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.4 | AZ | Literals and contextually typed NULL are eligible leaves | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.5 | AY | Casts scalar operators comparisons and CASE are recursively eligible | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.6 | AY | Only IMMUTABLE calls with eligible children qualify | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.7 | AY | The empty scalar registry admits no named source call | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.8 | AY | Bound column dependency is forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.9 | AY | Relation dependency is forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.10 | AY | Output alias dependency is forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.11 | AY | Aggregate dependency is forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.12 | AY | Scalar EXISTS and IN subqueries are forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.13 | AY | Every other row-dependent construct is forbidden | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.14 | AZ | Only INT32 and INT64 final semantic types are accepted | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.15 | AZ | INT32 is normalized to INT64 | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.16 | AZ | No implicit FLOAT VARCHAR BOOLEAN DATE or TIMESTAMP conversion is synthesized | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.17 | AZ | Raw NULL receives INT64 NULL context | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.18 | AY | Nonconstant expression is BindError | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.19 | AZ | Nonintegral expression is TypeError | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.20 | BA | Mandatory Chapter-17 constant folding occurs during binding | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.21 | BA | Dominating constant scalar errors retain Chapter-17 binding-time category | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.22 | BA | The folded/residual expression is handed to execution | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.23 | BA | Completed pre-fold work is not reevaluated | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.24 | BB | Final count is acquired exactly once before relational row processing | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.25 | BC | Final NULL is ExecutionError | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.26 | AZ | Final negative INT64 is ExecutionError | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.27 | BC | LIMIT/OFFSET never clamps NULL or negative values | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-15.28 | BC | No implementation-sized semantic maximum is introduced | §19.14 | V19-15 | V19-15 LIMIT/OFFSET matrix | dependency/type/fold/domain models |
| V19-16.1 | BD | INSERT target identity/columns do not create VALUES target-row scope | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.2 | BD | INSERT VALUES target-column-looking names use ordinary scalar lookup | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.3 | BD | INSERT SELECT binds as an independent query block | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.4 | BD | INSERT target mapping occurs after source SELECT binding | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.5 | BE | UPDATE creates one target-row relation occurrence | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.6 | BE | UPDATE target is visible in every SET RHS | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.7 | BE | UPDATE target is visible in WHERE | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.8 | BE | UPDATE target is visible in RETURNING | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.9 | BE | UPDATE exposes final target table-name qualifier | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.10 | BE | UPDATE assignment LHS resolves directly against target descriptor | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.11 | BF | DELETE creates one target-row relation occurrence | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.12 | BF | DELETE target is visible in WHERE | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.13 | BF | DELETE target is visible in RETURNING | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.14 | BF | DELETE exposes final target table-name qualifier | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.15 | BD | DML target aliases remain absent | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.16 | BG | INSERT RETURNING uses the new row image | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.17 | BG | UPDATE RETURNING uses the final new row image | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.18 | BG | DELETE RETURNING uses the old deleted row image | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.19 | BD | Duplicate INSERT targets are BindError | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.20 | BE | Duplicate UPDATE targets are BindError | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.21 | BD | Aggregates remain forbidden in DML expressions | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-16.22 | BD | DML binding does not redefine physical row-version publication retry or result buffering | §19.4.3 | V19-16 | V19-16 DML namespace matrix | closed target-row namespace table |
| V19-17.1 | BH | CREATE TABLE columns form one statement-local declaration namespace | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.2 | BH | PK/UNIQUE members resolve only in that namespace | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.3 | BH | Unknown constraint member is ConstraintDefinitionError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.4 | BH | Repeated constraint member is ConstraintDefinitionError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.5 | BH | Second PRIMARY KEY declaration is ConstraintDefinitionError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.6 | BH | Default restrictions delegate to the Chapter-21 owner | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.7 | BI | CREATE INDEX resolves target table in the catalog snapshot | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.8 | BI | CREATE INDEX resolves source-ordered key columns | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.9 | BI | Duplicate index key is BindError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.10 | BH | Index-ineligible key type is TypeError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.11 | BJ | DROP TABLE resolves only table kind | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.12 | BJ | DROP INDEX resolves only index kind | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.13 | BJ | Missing DROP object is CatalogError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.14 | BH | DDL binding performs no physical build/publication/dependency action | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.15 | BK | VACUUM target lookup uses ordinary canonical table binding | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.16 | BK | ANALYZE target lookup uses ordinary canonical table binding | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.17 | BL | EXPLAIN inner SELECT uses ordinary canonical binding | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.18 | BH | DDL binding allocates no persistent object or File ID | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-17.19 | BJ | A DROP name visible only as the wrong object kind is CatalogError | §19.4.4; §§18.10.4, 21.6–21.12 | V19-17 | V19-17 DDL/control matrix | declaration/object-kind tables |
| V19-18.1 | BM | Scalar subquery creates an independent child query block | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.2 | BN | EXISTS creates an independent child query block | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.3 | BO | IN subquery creates an independent child query block | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.4 | BP | Derived table creates an independent child query block | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.5 | F | Child relation BindingIds remain whole-statement unique | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.6 | BP | Derived relation receives its own BindingId and mandatory alias | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.7 | BP | Derived child has no sibling lateral visibility | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.8 | BM | Scalar output shape remains Chapter-20-owned | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.9 | BM | Scalar runtime row cardinality remains downstream-owned | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.10 | BN | EXISTS result semantics remain Chapter-20-owned | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.11 | BO | IN one-column/type/3VL semantics remain Chapter-20/17-owned | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.12 | BP | Derived exported names and restrictions remain Chapter-20-owned | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.13 | BQ | Outer-only qualified references diagnose UnsupportedCorrelation | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.14 | BQ | Outer-only unqualified references diagnose UnsupportedCorrelation | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.15 | BQ | A local missing member does not fall through | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.16 | BQ | Local ambiguity does not fall through | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.17 | BQ | A locally absent name with no enclosing match remains ordinary BindError | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.18 | BQ | No executable OuterRef or correlated bound expression is produced | §19.18; §20.14 | V19-18 | V19-18 subquery matrix | child-scope graph and downstream shape tables |
| V19-18.19 | BM | A bound subquery records stable query-local identity | §19.18; §20.14 | V19-18 | V19-18 subquery metadata fixture | child-scope graph and downstream shape tables |
| V19-18.20 | BM | A bound subquery records its exact semantic kind | §19.18; §20.14 | V19-18 | V19-18 subquery metadata fixture | child-scope graph and downstream shape tables |
| V19-18.21 | BM | A bound subquery records its typed logical child plan | §19.18; §20.14 | V19-18 | V19-18 subquery metadata fixture | child-scope graph and downstream shape tables |
| V19-18.22 | BM | A bound subquery records result type and nullability | §19.18; §20.14 | V19-18 | V19-18 subquery metadata fixture | child-scope graph and downstream shape tables |
| V19-18.23 | BM | A bound subquery records its SourceSpan | §19.18; §20.14 | V19-18 | V19-18 subquery metadata fixture | child-scope graph and downstream shape tables |
| V19-19.1 | BR | Missing table is CatalogError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.2 | BR | Unknown column is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.3 | BR | Unknown qualifier is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.4 | BR | Unknown qualified member is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.5 | BR | Wrong DROP object kind is CatalogError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.6 | BR | Other canonical catalog lookup failure is CatalogError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.7 | BR | Illegal aggregate placement is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.8 | BR | Nested aggregate is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.9 | BR | Duplicate INSERT target is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.10 | BR | Duplicate UPDATE assignment is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.11 | BR | Operator mismatch is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.12 | BR | Unknown generic call is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.13 | BR | Nonintegral LIMIT/OFFSET is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.14 | BR | Invalid CREATE TABLE constraint definition is ConstraintDefinitionError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.15 | BR | Invalid default definition uses its frozen ConstraintDefinitionError owner | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.16 | BR | Unauthorized star call is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.17 | BV | FrontEndResourceLimit retains its resource category | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.18 | BV | OutOfMemory retains its resource category | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.19 | BS | Unknown names do not fabricate dependent type errors | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.20 | BS | Unknown names do not fabricate dependent grouping errors | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.21 | BS | Unknown names do not fabricate dependent call errors | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.22 | BR | Non-BOOLEAN predicate is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.23 | BR | CASE common-type mismatch is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.24 | BR | IN common-type mismatch is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.25 | BV | Resource/cancellation timing remains immediate | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.26 | BR | Hash catalog and traversal order cannot select the semantic error | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.27 | BR | Unknown qualifier/member is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.28 | BR | Duplicate local qualifier is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.29 | BR | Ambiguous ORDER output alias is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.30 | BR | Invalid/out-of-range ordinal is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.31 | BR | Aggregate placement nesting grouping and HAVING legality failures are BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.32 | BR | Nonconstant LIMIT/OFFSET is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.33 | BR | Duplicate DML/index target is BindError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.34 | BR | Unknown type is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.35 | BR | Operator cast coercion predicate CASE and IN mismatch is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.36 | BR | Unresolvable generic call/arity/star/overload/argument mismatch is TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.37 | BR | Nonintegral LIMIT/OFFSET and index-ineligible key are TypeError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.38 | BR | Invalid CREATE TABLE constraint/default definition uses ConstraintDefinitionError | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.39 | BR | Unsupported correlation uses UnsupportedFeature | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.40 | BR | Runtime scalar-subquery cardinality remains CardinalityError owner | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.41 | BV | FrontEndResourceLimit and OutOfMemory retain distinct resource categories | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.42 | BS | Only errors with satisfied semantic prerequisites become candidates | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.43 | BS | Unknown names do not fabricate dependent type/group/call errors | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.44 | BT | Earliest responsible SourceSpan start wins | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.45 | BT | Equal starts choose the shorter represented span | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.46 | BU | Identical spans use name placement constraint type cardinality class order | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.47 | BU | Call registry TypeError is class four | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.48 | BU | Resolved aggregate placement BindError is class two | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-19.49 | BV | Resource/cancellation timing is immediate and hash/catalog/traversal order cannot select the error | §§19.20.1–19.20.2 | V19-19 | V19-19 error/precedence matrices | prerequisite-filtered candidate sorter |
| V19-20.1 | CA | Completed bound expressions are immutable | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.2 | CA | Rewrite creates a new expression or shares immutable children | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.3 | CB | Allocation ownership and storage representation are implementation-defined | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.4 | CB | Pointer/allocation identity is nonsemantic | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.5 | CC | Bound object may borrow source only while backing is retained | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.6 | CC | Required display/diagnostic bytes are materialized before backing release | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.7 | CC | Numeric SourceSpan intervals may outlive source bytes | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.8 | CC | Cast provenance is runtime-only | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.9 | CC | Checked budget refusal is FrontEndResourceLimit | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.10 | BV | Unsatisfied required allocation is OutOfMemory | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.11 | CC | Mid-bind resource failure unwinds private scopes identities casts outputs and candidates | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.12 | BW | No resource failure publishes a partial executable bound statement | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.13 | CC | Binding resource state has no persistent/recovery representation | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-20.14 | CC | Resource/transaction consequences reuse Chapters 18 and 39 | §§19.6–19.7, 19.20; §§18.14, 18.17 | V19-20 | V19-20 lifetime/resource faults | ownership graph and failure schedule |
| V19-21.1 | BX | Same AST snapshot and registries produce the same result or error | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.2 | BY | Descriptor/cache storage order is nonsemantic | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.3 | BY | Hash seed and container iteration are nonsemantic | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.4 | BY | TableId/ColumnId numeric order does not control presentation | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.5 | CB | Allocation addresses and layout are nonsemantic | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.6 | CB | Opaque BindingId numeric values are alpha-renamable | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.7 | BX | Scheduler interleaving cannot alter binding | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.8 | BZ | Locale cannot alter canonical identifier comparison | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.9 | BZ | Unicode normalization is absent | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.10 | BZ | Quoted identifiers are not case-folded by binder | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.11 | BY | Source relation and descriptor presentation order remain the only defined presentation orders | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-21.12 | BY | Error selection is independent of traversal and catalog order | §§19.2, 19.4–19.5, 19.20.2 | V19-21 | V19-21 environment replay | alpha-renamed canonical serialization |
| V19-22.1 | CD | Chapter-16 identifiers descriptors SchemaVer and snapshot rules are consumed unchanged | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.2 | CD | Chapter-17 TypeIds NULL TypeResolver casts calls and folding rules are consumed unchanged | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.3 | CD | Chapter-18 grammar raw AST provenance SourceSpan lifetime and resources are consumed unchanged | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.4 | CD | Chapter-20 LogicalSlotId grouping DISTINCT ORDER LIMIT and subquery handoffs are preserved | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.5 | CD | Chapter-21 DML DDL default RETURNING and publication ownership is preserved | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.6 | CD | Chapter-29 descriptor signature and aggregate ordinal ownership is preserved | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.7 | CD | Sections 39.1–39.3 error transaction and resource ownership is preserved | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.8 | CD | Section 41.4 binder verification obligations are covered | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.13 | CE | Procedures use deterministic barriers and independent oracles | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.15 | CE | All mandatory procedural matrices are present | Architecture front matter; Chapters 16–21, 29, 39, 41 | V19-22 | V19-22 composition and methodology check | owner ledger and mandatory procedural matrices |
| V19-22.17 | CD | DISTINCT is handed off as an explicit logical duplicate-elimination requirement | §§19.15, 20.10 | V19-22 | V19-22 composition ledger | owner ledger and mandatory procedural matrices |
| V19-22.18 | AG | Only searched CASE is admitted by the binder semantics | §19.16; §17.9.1 | V19-22 | V19-22 composition ledger | owner ledger and mandatory procedural matrices |
| V19-22.19 | AH | IN and NOT IN remain their own semantic node rather than a required OR rewrite | §19.17; §17.9.2 | V19-22 | V19-22 composition ledger | owner ledger and mandatory procedural matrices |
| V19-22.20 | AH | IN and NOT IN preserve once-only left evaluation and left-to-right error/short-circuit boundaries | §19.17; §17.7.3 | V19-22 | V19-22 composition ledger | owner ledger and mandatory procedural matrices |
| V19-22.21 | CF | Parameter syntax has no binder semantics | §19.19; Chapter 18 | V19-22 | V19-22 grammar/binder boundary fixture | owner ledger and mandatory procedural matrices |
| V19-22.22 | F | A no-FROM block has an empty local relation namespace and no implicit relation or OuterRef | §19.20.3 | V19-22 | V19-22 no-FROM fixture | owner ledger and mandatory procedural matrices |
| V19-22.23 | CD | Binding does not alter Section 39.1 transaction consequences | §19.20.3; §39.1 | V19-22 | V19-22 composition ledger | owner ledger and mandatory procedural matrices |


No fuzzer, production binder, debug formatter, reference
database, container implementation, or downstream executor substitutes for these
independent binding oracles.

---

## Chapter 20 Logical-Planning and Relational-Semantics Verification

This section is the complete procedural owner for the logical-planning and relational-
semantics contract in [`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 20. Chapters 16–19
supply catalog identity, scalar semantics, source provenance, and fully bound input;
Chapter 29 supplies aggregate value and ordinal semantics. Sections 22.4, 27.9, 30.1,
30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, and 40.8 own the stated
downstream composition points. These procedures verify Chapter 20 without redefining
those owners or prescribing a logical-node representation, optimizer framework, or
physical algorithm.

### Deterministic logical-semantics harness

Use a fixture driver whose input is one fully bound Chapter-19 statement and an immutable
catalog/snapshot model. The fixture controls:

```text
bound input:
    query-block and statement structure
    BindingIds and bound column references
    bound scalar trees, types, nullability, casts, SourceSpans, and diagnostic origins
    aggregate descriptors and canonical aggregate ordinals
    ordered output aliases/metadata and ORDER output identities
    folded/residual LIMIT/OFFSET expressions and validated INT64 counts

catalog/snapshot:
    immutable TableDescriptor/SchemaDescriptor identities and SchemaVer
    TableId, ColumnId, presentation order, catalog nullability
    transaction-visible logical base-row occurrences

logical construction:
    node kind and child relation
    ordered output schema and LogicalSlotId equivalence relation
    fresh, pass-through, and remapped slot events
    row-occurrence bag and semantic-order property
    logical nullability and lineage
    aggregate and subquery occurrence entries

rewrite:
    canonical pre-rewrite tree
    demanded-evaluation relation over logical occurrences
    exact semantic proof facts and their provenance
    executable scalar tree and non-executable metadata
    post-rewrite tree, output mapping, aggregate mapping, and diagnostic mapping

observable result:
    output schema and slot-identity relation
    row bag, or ordered occurrence sequence where semantic order exists
    value / NULL / UNKNOWN result
    logical error category, canonical diagnostic origin, and frozen precedence
    aggregate ordinal and source-occurrence identity
```

Instrumentation records conceptual semantic events; it is not a required production API.
The harness MAY use deterministic adapters, immutable fixture nodes, generation-tagged
occurrence handles, and injected exact-proof facts. It MUST NOT use production planner,
optimizer, validator, expression evaluator, hash table, comparator, or EXPLAIN output as
the expected-result generator for the same behavior being checked.

For representation perturbation, replay each applicable fixture while varying allocator
layout, pointer addresses, opaque ID encodings, map/hash insertion order and seed, memo or
rule visitation order, scheduler interleaving, vector/chunk boundaries, physical scan
order, and conforming physical operator choice. No correctness procedure uses sleeps,
wall-clock races, host container order, or byte-for-byte row sequence where Chapter 20
defines only an unordered bag or tie-equivalence class.

Canonical comparison alpha-renames opaque `BindingId`, `LogicalSlotId`, subquery-
occurrence, and proof-node representations while preserving their equality, inequality,
scope, nonreuse, lineage, and mapping relations. It compares ordered schema positions,
semantic order only where required, and row occurrences by a fixture-only occurrence tag
that is not part of SQL values.

### Independent oracle registry

| Oracle | Independent construction |
|---|---|
| bound-to-logical | declarative translation of Chapter-19 semantic records; no SQL-name lookup |
| schema | ordered list algebra over child schemas and node-local output declarations |
| slot identity | whole-statement occurrence graph with an append-only retired-ID set and alpha-renaming |
| node slot rules | declarative fresh/pass-through/remap table applied to ordered child/output occurrences |
| lineage | tuple `(query block, BindingId, TableId, ColumnId, descriptor/SchemaVer, boundary map)` where applicable |
| bag | mathematical multiset over fixture-only row-occurrence tags plus SQL values |
| visible Get rows | Chapter-16/snapshot visibility fixture projected to one occurrence per visible logical row |
| Values rows | source-list row-occurrence enumeration preserving repeated value tuples |
| no-FROM | singleton multiset containing one zero-column occurrence |
| Filter | per-occurrence Chapter-17 3VL selector |
| Project | ordered expression-vector constructor producing one tagged row per input occurrence |
| join candidate pairs | explicit finite `L × R` occurrence-pair enumeration |
| LEFT extension | per-left TRUE-match count with one extension exactly when the count is zero |
| join schema | left/right ordered-list concatenation over complete child schemas |
| nullability | declarative schema transform that distinguishes catalog and contextual logical nullability |
| grouping | runtime partition by Chapter-17 normalized non-NULL equality plus one NULL class per key position |
| DISTINCT | one componentwise canonical representative occurrence per independently computed grouping-equivalence class |
| NULL equivalence | explicit NULL-class table distinct from predicate `=` |
| FLOAT equivalence | canonical NaN and signed-zero normalization before class partitioning |
| global group | constant one-group cardinality for no-key aggregation, including empty input |
| explicit-group empty | constant zero-group cardinality for empty input with grouping keys |
| aggregate occurrence | source-occurrence/ordinal table from Chapter 19 and §29.3.7, separate from aggregate values |
| aggregate value/error | existing Chapter-29 exact-state and lowest-ordinal oracle |
| aggregate ordinal propagation | immutable query-block source-occurrence table compared before and after rewrite |
| ordering | §30.3 comparator model followed by equivalence classes for unspecified ties |
| ordered Limit | exact sequence slicing after OFFSET and before LIMIT |
| unordered Limit | child-multiset subbag predicate plus exact cardinality formula |
| scalar subquery | final-row cardinality state machine with typed-empty, one-row, and second-row outcomes |
| EXISTS | final-row existence state machine that omits projection-only value demand |
| IN/NOT IN | repeated Chapter-17 equality 3VL fold over the complete final RHS multiset |
| derived table | child-bag preservation plus explicit child/outer slot-and-name boundary map |
| derived order | consumer-visible ordering-property model distinct from physical child emission order |
| demand | declarative relation `(expression occurrence, relational occurrence, context)` over the unrevised canonical tree |
| rewrite safety | pre/post observable-demand relation plus independently checked exact proof facts |
| executable order | source-derived ordered scalar tree serialized without commutative normalization |
| non-executable metadata | tagged metadata domain proven incapable of execution or diagnostics |
| provenance | source-occurrence-to-origin map carrying SourceSpan, cast provenance, and precedence identity |
| synthetic legality | exact total/error-free proof or one-source-origin inheritance predicate |
| exact proof | closed §§20.17.10/35.2 proof calculus, disjoint from every estimate/statistic input |
| validation | structural predicate over immutable fixture plans, independent of the production validator |
| logical/physical boundary | owner ledger plus cross-realization semantic-equivalence comparison |
| determinism | alpha-renamed canonical semantic serialization and allowed-result equivalence predicates |
| ownership | cross-chapter responsibility ledger and logical-semantics procedure index below |

### V20-1 — Bound statement to canonical logical plan

Feed the harness fully bound Chapter-19 statements. Assert that logical construction does
not look up names, reinterpret aliases, reassign `BindingId`, re-resolve types/casts,
reclassify ORDER aliases/ordinals, renumber aggregate occurrences, or repeat Chapter-19
LIMIT/OFFSET admissibility and error decisions. Compare canonical pre-rewrite SELECT shape
with §20.15, including the §20.5 no-FROM source, and compare DML relational children and
hidden-slot requirements with §20.13 and their Chapter-21 handoff. The reference shape is
semantic, not a physical execution sequence.

### V20-2 — Logical schema and semantic identities

Construct expected output schemas as ordered semantic records containing slot identity,
display metadata where applicable, logical type, logical nullability, lineage, and the
system/internal flag. Verify that `LogicalSlotId` identifies one logical output occurrence
and is unequal in meaning to `TableId`, `ColumnId`, `BindingId`, heap `SlotId`, output
ordinal, aggregate ordinal, expression equivalence, pointer identity, and permanent vector
position. Use one fixture in which all relevant identities and ordinals differ.

### V20-3 — Slot lifecycle, node rules, and rewrite stability

Build one top-level statement containing a Get self-join, Values, duplicate projection,
aggregate, nested scalar/EXISTS/IN side plans, derived table, and DML relational child.
The occurrence-graph oracle requires whole-statement uniqueness with no query-block or
side-plan reset. Retire an output through a legal rewrite, then create another output and
assert nonreuse. Across separate statements, permit opaque representation reuse.

Apply the exact node table:

```text
Get              fresh per exposed BindingId + ColumnId occurrence
Values           fresh per output position
Project          fresh per declared output position
Aggregate        fresh per group-key output and aggregate occurrence
derived boundary fresh outer slot with explicit child-to-outer mapping
Filter           pass through
Join             pass through left schema then right schema
Distinct         pass through
Sort             pass through
Limit            pass through
```

For rewrites, preserve an ID only for the same surviving consumer-visible occurrence with
the same definition, lineage, and semantic boundary. New or duplicated occurrences are
fresh; removed IDs disappear and remain retired; boundary removal retains an explicit
consumer mapping. Equivalent values alone are not identity.

### V20-4 — Bag baseline and logical sources

Tag input occurrences independently of SQL values and compare mathematical multisets.
Verify that equal rows remain separate occurrences and that multiplicity does not imply
order. Get emits one occurrence per visible logical base row and excludes non-visible
historical versions. Values emits one occurrence per listed row, preserves duplicates,
and establishes no SQL order. The no-FROM source is exactly one zero-column occurrence.
Only an operator's stated contract may remove, produce, or equivalence-collapse
occurrences.

### V20-5 — Filter and Project

For Filter, evaluate its predicate once in the oracle for each occurrence whose retention
must be determined: TRUE maps to one output occurrence; FALSE and UNKNOWN map to none.
For ordinary Project, evaluate every declared output expression for every produced input
occurrence and emit exactly one projected occurrence, retaining equal projected rows as
duplicates. Project output positions always receive fresh slots. Contrast ordinary
Project demand with the specialized EXISTS projection-irrelevance family V20-13.

### V20-6 — INNER, LEFT, and CROSS occurrence semantics

Enumerate finite `L × R` explicitly. INNER emits one output for each pair whose ON result
is TRUE and none for FALSE/UNKNOWN. LEFT emits every TRUE pair and, for each left
occurrence having zero TRUE pairs, exactly one right-NULL-extended occurrence; FALSE and
UNKNOWN are nonmatches and no extra extension follows any TRUE match. CROSS emits every
pair without an ON predicate. Include duplicate values/occurrences, `m*n` multiplicity,
and the complete empty-side matrix. Predicate errors use the V20-15 demand relation rather
than a physical pair-generation order.

### V20-7 — Join schema, nullability, and row order

Compute join schema as the complete left child schema followed by the complete right child
schema. Preserve all child slots. For LEFT, change only contextual logical nullability of
right outputs; retain type, `BindingId`, slot, lineage, descriptor, and catalog `NOT NULL`.
Compare join results only as bags: no nested-loop, hash, merge, left-major, right-major,
RID, or scan sequence is semantic.

### V20-8 — DISTINCT, grouping, global group, and HAVING

Partition rows with the grouping-equivalence oracle, not SQL predicate `=` and not a
production hash/comparator. NULLs share one class, both zeros share one FLOAT64 class,
all canonical NaNs share one class, VARCHAR compares exact bytes, and composite keys apply
the rule componentwise. Independently apply §17.10.3's canonical representative: typed
NULL stays typed NULL, the zero class emits `+0.0`, the NaN class emits canonical NaN, and
other values retain their semantic representation. DISTINCT emits that componentwise
canonical occurrence per class and preserves schema and slots. Aggregate grouping emits
one row per group with canonical key components; no-key aggregation over empty input forms
one group, explicit grouping over empty input forms none. HAVING retains only TRUE. Keep
Chapter-19 structural group-expression legality separate from runtime value grouping.

### V20-9 — Aggregate occurrence identity and sharing

Compare each `LogicalAggregate` entry with the Chapter-19/§29.3.7 source-occurrence table.
Every distinct source occurrence keeps its canonical query-block ordinal and has a fresh
output slot even when descriptor, arguments, type, and value are identical. ORDER alias
reuse consumes the existing output. Rewrites do not renumber, compact, reuse, merge,
duplicate one occurrence under one ordinal, or move it across a query block; legal removal
leaves surviving ordinals unchanged. A conceptual shared physical computation passes only
when the mapping retains every logical occurrence, slot, ordinal, origin, output, and the
lowest-ordinal Chapter-29 error selection.

### V20-10 — Sort and unordered results

Use bound key order, direction, resolved NULL placement, and the §30.3 SQL/FLOAT
comparator. Assert `ASC -> NULLS FIRST` and `DESC -> NULLS LAST`, preserve the exact input
bag and slots, and compare rows equal on every key as an unordered tie class under §30.1.
Without a semantic ordering operator, compare final results as bags; scan, join, hash,
page, RID, and container order are never an SQL order oracle.

### V20-11 — LogicalLimit

Supply only Chapter-19-admitted count expressions and their execution-start validated
nonnegative INT64 values. Independently apply OFFSET first:

```text
after_offset = max(0, n - o)
without LIMIT: output_count = after_offset
with LIMIT:    output_count = min(after_offset, l)
```

Never compute `o+l`. Ordered input uses exact sequence slicing and preserves survivor
order. Unordered input accepts any child subbag of the exact resulting cardinality and
remains unordered; compare with a subbag predicate, not a chosen physical prefix. Verify
zero/oversized/INT64_MAX cases, no new error, no deduplication, and exact schema/slot/
metadata preservation. Section 27.9 physical behavior and §35.20 estimates must conform
to, not define, this logical oracle.

### V20-12 — Scalar subqueries

Drive the independent child to final rows after all relational clauses. Zero rows yield a
typed NULL, one row yields its scalar value, and successful construction of a second row
raises `CardinalityViolation`/SQL `CardinalityError`. Errors constructing row one or two
precede cardinality; after two successful rows, cardinality precedes later-row work. The
consumer demands at most two final rows, but blocking work needed to produce them remains
demanded. Remove the check only with exact final-cardinality-at-most-one proof; estimates
and `required_rows` do not qualify.

### V20-13 — EXISTS, IN, and NOT IN

EXISTS is non-null FALSE for no final row and TRUE after the first final row. Projection-
only values, projection-only aggregate work, irrelevant DISTINCT value construction,
and removable ORDER-key values are not demanded solely for existence. DISTINCT-dependent
values needed to determine final-row existence after OFFSET are demanded under
Architecture §20.14.5. FROM/WHERE/grouping/HAVING and required OFFSET/LIMIT work retain
their existing demand. Include global aggregate, LIMIT0, OFFSET, and erroring projections.

For the DISTINCT/OFFSET composition, use committed t(k) fixtures with test-owned input
bags `[]`, `[1]`, `[1,1]`, `[1,2]` and `[1,1,2]`. Independently partition the projected
values by the Chapter-17/20 grouping-equivalence oracle; do not obtain the expected class
count from the production DISTINCT operator, retained plan, or estimated NDV. With d
classes, validated offset o, and optional limit l, expected existence is
`d > o` and `(l is absent or l > 0)`. Cross o=0/1/2 and absent/zero/positive LIMIT.
In particular, `[1,1]` with OFFSET1 is FALSE and `[1,2]` with OFFSET1 is TRUE;
without DISTINCT the duplicate-input case is TRUE. Also use NULL duplicates, signed
zeros and canonical-equivalent NaNs so physical encoding cannot create extra classes.

Observe actual demanded projection evaluation, class production, OFFSET consumption,
first-final-row delivery and lazy occurrence initialization through owner events. Require
at least one duplicate-rich fixture where dropping DISTINCT changes the Boolean result,
and one two-class positive result, so an always-empty or DISTINCT-free implementation
cannot pass. Run the independent oracle against fallback and permitted optimized plans,
varying chunk/selection layout and spill conditions without using plan-to-plan agreement
as the expected result.

Use `EXISTS(SELECT DISTINCT 1/k FROM t OFFSET 1)` with two rows k=0,1 and no trusted
proof removing the required class construction. The demanded division error must be
preserved; it cannot be suppressed as projection-only. Contrast OFFSET0/absent OFFSET,
where this value work is irrelevant, and LIMIT0, where projection cannot establish a
final row. These contrasts do not suppress independently demanded relational work or
count validation. Arm a projection fault in the relevant fixture and prove it is reached;
in the irrelevant fixture prove no projection-only evaluation contributes an error.

For a constant-error projected expression over a table with no exact row-production
proof, reuse §17.10.2's existing lazy relational-subquery folding oracle: merely possible
rows or estimated NDV cannot force an eager error. Actual empty input supplies no runtime
projection row; demanded nonempty input follows the retained expression's error owner.
Test skipped outer CASE/AND/OR and demanded occurrences separately. Required aggregate
inputs/results feeding DISTINCT use existing aggregate value/error oracles. Binding still
checks names/types even for irrelevant projection. Scalar error provenance, specialized
subquery precedence, occurrence caching and transaction consequences remain unchanged.

For IN, evaluate the left scalar first on first demand, then consume and validate the
complete final RHS before returning a probe result. Fold Chapter-17 equality results:
TRUE if any TRUE, UNKNOWN if no TRUE and any UNKNOWN, otherwise FALSE. Cover empty RHS,
left NULL, RHS NULL, duplicates, NaN, signed zero, VARCHAR, numeric promotion, and NOT as
exact 3VL negation. RHS duplicates may be collapsed only because truth is unchanged.

### V20-14 — Subquery occurrence state, derived tables, and rewrites

Each bound expression-subquery occurrence has one stable query-local identity, independent
logical child, semantic mode, and at-most-once state per statement attempt initialized on
first demand. Skipped CASE/AND/OR or empty outer input does not initialize it; a retry
discards attempt state and uses the new statement snapshot while retaining CommandId.
Children share the containing snapshot/ReadEpochGuard and consume no nested transaction,
snapshot, CommandId, or lock owner.

Apply V20-13's existing empty-RHS/NULL-left truth case to every NOT IN anti-join
rewrite. A NOT NULL RHS type does not prove that the RHS has a row: rejecting a
NULL left unconditionally must fail this equivalence oracle when the build is
empty. Compare required left/build demand as well as the filter result under
Architecture §20.14.11.

Derived-table verification requires a fresh outer-slot map, derived `BindingId`, exact
child bag preservation, namespace isolation, and no advertised outer order merely from
child execution order. Child ORDER affects LIMIT/OFFSET but does not create an outer SQL
ordering guarantee. Correlated/row-valued/quantified/LATERAL/set/data-modifying forms have
no executable plan. Compare every optimized subquery against the canonical semantic mode;
rewrite support never depends on successful decorrelation or semi/anti/marker conversion.

### V20-15 — Demanded-evaluation oracle

Build the demand relation from the unrevised canonical logical semantics. Its key is an
executable scalar occurrence plus the row, pair, group, ordering occurrence, or subquery
control context for which evaluation is required. Cover Filter, ordinary Project, JOIN ON,
group keys, aggregate arguments, HAVING, ORDER keys, scalar final rows, EXISTS existence
work, IN left/build work, and Chapter-17 CASE/AND/OR skipping. CPU count, vector lanes,
physical visitation, allocation, spill, and other resource use are not demand observables.

### V20-16 — Rewrite demand safety and exact proof

For every rewrite, compare pre/post demand relations and semantic observables. Any added,
removed, moved, or duplicated potentially observable evaluation requires either exact
preservation or an independently validated proof over the complete changed domain that it
is total, error-free, deterministic for the same bound values, and insensitive to the
count/order change. Statistics, samples, selectivity, row-count estimates, costs,
heuristics, and likely ranges never authorize correctness-sensitive change.

Mandatory cases include unsafe and exactly safe predicate pushdown, ordinary projection
pruning versus EXISTS projection irrelevance, Boolean simplification, constant folding,
join-graph/equality metadata, propagation, trusted-key reasoning, contradiction detection,
and exact-empty replacement. A proven empty result alone cannot erase a demanded
potentially erroring expression. All rewrites additionally preserve bags, NULL/UNKNOWN,
outer joins, order, slots, aggregate identity, diagnostic origin, and frozen precedence.

### V20-17 — Executable scalar order

Serialize executable trees in semantic child order and compare before/after. Arithmetic,
comparison, AND, OR, CASE, and every other ordered scalar construct retain Chapter-17
left-to-right and short-circuit order. Reject commutative swapping, associative
reassociation, executable child sorting, and comparison reversal such as `a < b` to
`b > a`. Permit canonicalized equality classes, proof/search keys, and descriptors only
when they are non-executable, cannot raise an error, cannot own a user diagnostic, and do
not replace or mutate the executable tree. Reuse §17.10.2 for authorized folding.

### V20-18 — Runtime diagnostic provenance

The independent origin table records each source-derived executable occurrence's
canonical SourceSpan, explicit/implicit cast provenance, more-specific error-origin data,
and precedence identity. Movement and structural sharing preserve one origin; legal
duplication copies it to every copy but remains V20-15-controlled. An error-capable
replacement preserves category, canonical origin, and precedence or is rejected.

A spanless synthesized executable is accepted only with exact proof that it is total and
error-free over its complete demand domain. An error-capable synthetic is accepted only
when it implements exactly one identifiable source occurrence and inherits that origin
and precedence. Non-executable metadata has no diagnostic span and cannot raise a scalar
error. Representation as a set/list/graph is optional and never substitutes for
deterministic diagnostic selection. Reuse §17.10.2 folding provenance and Chapter-18
retain-or-materialize lifetime procedures; source-buffer retention is not required.

### V20-19 — Logical properties and exact semantic proof

Check candidate keys, constants, lineage, nullability, semantic order, estimated rows, and
`is_provably_empty` as distinct properties. The exact-proof oracle accepts only the closed
§§20.17.10/35.2 sources and applies every operator-specific propagation rule, including
LEFT preservation, global aggregate one-row behavior, grouped aggregate behavior,
Limit-zero, and DML completion semantics. Perturb every statistic and estimate while
holding exact facts fixed; no proof bit or correctness rewrite may change. Logical
properties remain distinct from Chapter-37 physical properties, reserved property
extensions, and cost/search objectives.

### V20-20 — Logical-plan validation

Construct malformed immutable plans directly, one violated invariant at a time, with all
other descriptors valid. Use the independent structural predicate to require every
§20.18 invariant: child count and schema, whole-statement slot uniqueness/nonreuse,
node-specific slot rules and references, resolved types/BOOLEAN predicates, aggregate
ordinal/output identity, join schema/right nullability, Limit pass-through, hidden DML
slots, descriptor consistency, exact-proof provenance, supported subquery shape, rewrite
demand proof, executable diagnostic origin, and child order. Positive fixtures for every
node family prevent a reject-all validator from passing. Validate canonical construction
and each major rewrite output before physical consumption.

### V20-21 — DML, maintenance, and hidden logical values

Verify that internal target RID/system slots are not exposed through wildcard or ordinary
user output and survive while required. Insert accepts Values or SELECT through one
logical contract. Update carries target identity, RID, old values, assignments, and
RETURNING requirements; Delete preserves target RID and required old values. Analyze
carries the resolved table descriptor, SchemaVer, visible columns/indexes, has no ordinary
relational output, and performs no execution-time name lookup. These tests stop at the
Chapter-20 handoff and do not redefine Chapter-15/21 write semantics.

### V20-22 — Logical/physical and EXPLAIN ownership

Replay one logical plan through multiple conforming physical choices. Logical schema,
bag/order class, demand, error, and provenance remain fixed while access path, join/sort/
Limit algorithm, pipeline layout, vector width, worker/input partitioning, and memory strategy
vary.
Supported subqueries always retain a conforming §20.14.7 fallback; §37.17 chooses physical
realization. Logical EXPLAIN describes the validated logical representation and exposes
identity sufficient to distinguish required modes/boundaries without becoming an oracle
for semantics; §§40.2, 40.5, and 40.8 own presentation and physical/analysis information.

### V20-23 — Representation and environment determinism

Canonical semantic serialization MUST remain equivalent under hash seed, map iteration,
pointer/address, allocation order, opaque numeric ID representation, TableId/ColumnId
numeric order, optimizer visitation, scheduler, source-buffer release, physical scan
order, and conforming join/sort/aggregate/Limit realization. Where order is unspecified,
compare bags or tie classes; where slot values are opaque, compare the identity graph.
These perturbations may affect costs, resource use, and allowed unordered choices but not
the required semantic equivalence class or diagnostic origin.

### V20-24 — Error and diagnostic composition

Inject deterministic scalar errors at Filter, Project, JOIN ON, group key, aggregate
argument/finalization, HAVING, ORDER key, scalar subquery rows, IN build, and each rewrite
boundary. Compare the unrevised demand/origin/precedence oracle and §§39.1–39.3 category
owner. A logical validator defect remains an internal architecture error. Verification of
transaction consequences delegates to the existing statement-write-boundary family and
does not let logical planning choose ACTIVE versus MUST_ABORT.

### V20-25 — Cross-chapter composition and mandatory matrices

Run every matrix below and the cross-chapter owner ledger. Reused lower-layer procedures
are applicable only when their owning family supplies the required oracle and the V20 fixture checks
the Chapter-20 handoff. Architecture prose, a production implementation, or a reference
DBMS is not an oracle.

### V20-26 — Oracle independence and representation freedom

Use deterministic controls and the independent oracle registry above. Production logical
planners, optimizers, validators, and EXPLAIN output cannot supply their own expected
results. Compare opaque identities through equality graphs and alpha-renaming, and
unordered results as bags. Exercise permitted representation choices without requiring a
particular allocator, container, memo, execution strategy, or provenance representation.

### V20-27 — Procedure and oracle traceability

Each owner-to-procedure mapping requires a deterministic procedure and an
independent oracle, directly or through its named lower-layer family.

### Mandatory Chapter-20 matrices

The matrix cells are expected semantic results, not production implementation sketches.
Each row is exercised directly and through every applicable rewrite, provenance, and
determinism perturbation family.

#### LogicalSlotId matrix

| Case | Producer | Fresh/pass-through/remap | BindingId | ColumnId lineage | Uniqueness domain | Survives rewrite? | Persistent? |
|---|---|---|---|---|---|---|---:|
| base Get output | Get | fresh | base occurrence | exact source column | whole statement | while same output survives | no |
| self-join left | left Get | fresh | left occurrence | same source allowed | whole statement | same occurrence only | no |
| self-join right | right Get | fresh | right occurrence | same source allowed | whole statement | same occurrence only | no |
| Values column | Values | fresh | n/a | n/a | whole statement | same output position only | no |
| direct-column Project | Project | fresh | lineage retained | source column retained | whole statement | Project occurrence only | no |
| duplicate Project output | Project | fresh per position | lineage may match | lineage may match | whole statement | positions remain distinct | no |
| equivalent Project expression | Project | fresh per position | lineage may match | lineage may match | whole statement | equivalence does not merge | no |
| Filter output | Filter | pass-through | unchanged | unchanged | whole statement | yes | no |
| Join left output | Join | pass-through | unchanged | unchanged | whole statement | yes | no |
| Join right output | Join | pass-through | unchanged | unchanged | whole statement | yes | no |
| Aggregate group key | Aggregate | fresh | retained in lineage where applicable | retained in lineage where applicable | whole statement | same aggregate output only | no |
| Aggregate occurrence | Aggregate | fresh | source lineage where applicable | source lineage where applicable | whole statement | distinct from ordinal | no |
| Distinct | Distinct | pass-through | unchanged | unchanged | whole statement | yes | no |
| Sort | Sort | pass-through | unchanged | unchanged | whole statement | yes | no |
| Limit | Limit | pass-through | unchanged | unchanged | whole statement | yes | no |
| derived child output | child plan | existing child identity | child scope | child lineage | whole statement | while child occurrence survives | no |
| derived outer output | SubqueryScan boundary | remap fresh | derived relation | mapped child lineage | whole statement | explicit boundary map | no |
| nested scalar side-plan output | side plan | node rule | side-plan scope | as produced | whole statement | same occurrence only | no |
| surviving rewrite output | rewrite | preserve | unchanged | unchanged | whole statement | yes, same definition/lineage/boundary | no |
| duplicated rewrite output | rewrite | fresh duplicate | mapped | mapped | whole statement | original and duplicate differ | no |
| eliminated output | rewrite | retire | n/a | n/a | whole statement | no; ID never reused | no |

#### Bag/occurrence matrix

| Operator/case | Input occurrence rule | Output occurrence rule | Duplicate preservation | Equivalence collapse? | Order effect |
|---|---|---|---|---:|---|
| Get | visible logical base rows | one per visible row | equal rows retained | no | none |
| Values | listed rows | one per listed row | repeated rows retained | no | none from source position |
| no-FROM | no relational input | one zero-column row | n/a | no | none |
| Filter TRUE | one input occurrence | one output occurrence | retained independently | no | none established |
| Filter FALSE | one input occurrence | zero | occurrence removed | no | none |
| Filter UNKNOWN | one input occurrence | zero | occurrence removed | no | none |
| Project | one input occurrence | one projected occurrence | equal projected rows retained | no | none established |
| INNER JOIN | every `L × R` pair | one per TRUE pair | full pair multiplicity | no | none |
| LEFT JOIN | each left and every right candidate | TRUE pairs or one extension | full matches/left preservation | no | none |
| CROSS JOIN | every `L × R` pair | one per pair | full Cartesian multiplicity | no | none |
| Aggregate | input bag | one per runtime group | inputs combine by group | yes, grouping | group order unspecified |
| Distinct | input bag | one per equivalence class | one componentwise canonical representative occurrence | yes | none |
| Sort | input bag | identical occurrence multiset | all retained | no | semantic key order |
| Limit | child occurrences | exact selected sequence/subbag | selected duplicates remain duplicates | no | preserves ordered input; unordered stays unordered |
| derived table | child bag | same child multiplicity | all retained | no | child order not exported by itself |

#### Join matrix

| Case | Candidate occurrences | Output multiplicity | NULL extension | Schema | Slot behavior | Logical order |
|---|---|---:|---|---|---|---|
| INNER TRUE | one exact left/right occurrence pair | one | none | left then right | pass-through | none |
| INNER FALSE | one exact pair | zero | none | left then right | pass-through | none |
| INNER UNKNOWN | one exact pair | zero | none | left then right | pass-through | none |
| INNER duplicate matches | every duplicate occurrence pair | one per TRUE pair | none | left then right | pass-through | none |
| LEFT zero match | all right candidates for one left | one | exactly one right-side extension | left then right | pass-through | none |
| LEFT one match | all right candidates for one left | one matched row | none | left then right | pass-through | none |
| LEFT multiple match | all right candidates for one left | number of TRUE pairs | none | left then right | pass-through | none |
| LEFT FALSE-only | all right candidates for one left | one | exactly one | left then right | pass-through | none |
| LEFT UNKNOWN-only | all right candidates for one left | one | exactly one | left then right | pass-through | none |
| CROSS | every `L × R` occurrence pair | one per pair | none | left then right | pass-through | none |
| empty left | no candidate pairs | zero for every join type | none | still left then right | pass-through | none |
| empty right | no candidate pairs | INNER/CROSS zero; LEFT one per left | LEFT only | left then right | pass-through | none |

#### Nullability matrix

| Output | Catalog nullability | Logical nullability | TypeId | Source lineage | Reason |
|---|---|---|---|---|---|
| Get NOT NULL column | NOT NULL | non-nullable | unchanged | exact source | descriptor contract |
| Get nullable column | nullable | nullable | unchanged | exact source | descriptor contract |
| Filter output | unchanged | child value | unchanged | unchanged | schema-preserving selector |
| Project scalar | source metadata unchanged | bound expression result | resolved expression TypeId | expression lineage | new projected occurrence |
| INNER left | unchanged | left child value | unchanged | unchanged | no null extension |
| INNER right | unchanged | right child value | unchanged | unchanged | no null extension |
| LEFT left | unchanged | left child value | unchanged | unchanged | preserved side |
| LEFT right | unchanged, including NOT NULL | nullable | unchanged | unchanged | unmatched-row null extension |
| Aggregate output | descriptor/source unchanged | bound key/aggregate result | resolved output TypeId | aggregate/key source | fresh aggregate output |
| scalar subquery | child metadata unchanged | always nullable | child output TypeId | subquery occurrence | zero rows yield typed NULL |
| derived table | child/catalog metadata unchanged | exported child logical value | unchanged | child-to-outer map | namespace/slot boundary |

#### DISTINCT matrix

| Values | Equivalent? | Output multiplicity | Owner/order |
|---|---:|---:|---|
| identical non-NULL | yes | one/class | Chapter-17 normalized equality; no order |
| NULL / NULL | yes | one/class | §20.9 grouping equivalence |
| NULL / non-NULL | no | separate | §20.9 |
| NaN / NaN | yes | one/class | canonical FLOAT64 equality |
| +0.0 / -0.0 | yes | one/class | canonical FLOAT64 equality |
| byte-identical VARCHAR | yes | one/class | exact bytes |
| byte-distinct VARCHAR | no | separate | exact bytes |
| composite equal/different | componentwise | one/class | all key positions |

#### Grouping matrix

| Case | Same group? | Group count | Output row count | Aggregate invocation | Retained? |
|---|---:|---:|---:|---|---:|
| equal normal key | yes | one class | one/group | one state/group | yes |
| unequal key | no | two classes | two | one state/class | yes |
| NULL / NULL | yes | one NULL class | one | one state | yes |
| NaN / NaN | yes | one NaN class | one | one state | yes |
| +0.0 / -0.0 | yes | one zero class | one | one state | yes |
| multi-column equal | componentwise yes | one tuple class | one | one state | yes |
| multi-column one difference | no | two tuple classes | two | one state/class | yes |
| zero input + GROUP BY | n/a | zero | zero | no runtime group | n/a |
| zero input + global aggregate | n/a | one | one | one global state/finalize | yes |
| HAVING TRUE | n/a | preexisting | one/group | already formed | yes |
| HAVING FALSE | n/a | preexisting | zero for group | already formed | no |
| HAVING UNKNOWN | n/a | preexisting | zero for group | already formed | no |

#### Order matrix

| Case | Comparator | NULL placement | Tie rule | Semantic order? | Legal alternative sequences |
|---|---|---|---|---:|---|
| ASC normal | §30.3 ascending | resolved per key | no stability beyond keys | yes | every key-consistent sequence |
| DESC normal | §30.3 descending | resolved per key | no stability beyond keys | yes | every key-consistent sequence |
| ASC NULL | §30.3 | NULLS FIRST | NULL peers tie when other keys tie | yes | permutations within full-key ties |
| DESC NULL | §30.3 | NULLS LAST | NULL peers tie when other keys tie | yes | permutations within full-key ties |
| NaN | canonical FLOAT order | resolved per key | canonical NaNs tie | yes | NaN peer permutations |
| +0.0/-0.0 | one canonical equality class | resolved per key | zeros tie | yes | either zero-peer order |
| equal single key | §30.3 | resolved | no stable relative order | yes | any full-key tie permutation |
| equal prefix + secondary key | lexicographic key sequence | per key | secondary/following keys decide | yes | only final full-key ties vary |
| no ORDER BY | none | n/a | n/a | no | every bag-equivalent sequence |

#### LIMIT/OFFSET matrix

| Case | `after_offset` | Output count | Ordered selection | Unordered allowed bag | Schema preserved? | Slots preserved? | New error? |
|---|---:|---:|---|---|---:|---:|---:|
| OFFSET 0 | `n` | remaining or limited | unchanged prefix basis | whole child or limited subbag | yes | yes | no |
| LIMIT 0 | `n` after offset | `0` | empty | empty | yes | yes | no |
| LIMIT 1 | `max(0,n-o)` | `min(after_offset,1)` | next one | any one-member subbag if available | yes | yes | no |
| offset only | `max(0,n-o)` | `after_offset` | ordered suffix | any child subbag of count | yes | yes | no |
| limit only | `n` | `min(n,l)` | ordered prefix | any child subbag of count | yes | yes | no |
| offset + limit | `max(0,n-o)` | `min(after_offset,l)` | skip then take | any valid exact-count subbag | yes | yes | no |
| offset >= n | `0` | `0` | empty | empty | yes | yes | no |
| limit > remaining | `max(0,n-o)` | `after_offset` | all remainder | any valid remainder-count subbag | yes | yes | no |
| INT64_MAX counts | formula without `o+l` | formula result | semantic slice | exact-count subbag | yes | yes | no |
| ordered child | formula | formula result | exact sequence slice | n/a | yes | yes | no |
| unordered child | formula | formula result | n/a | any valid child subbag | yes | yes | no |
| empty child | `0` | `0` | empty | empty | yes | yes | no |

#### Subquery matrix

| Case | Demanded? | Output | NULL/3VL | Cardinality/error | Multiplicity | Order behavior | Owner |
|---|---:|---|---|---|---|---|---|
| scalar zero | when outer control reaches occurrence | typed NULL | NULL | zero-row success | one scalar value | child final-row semantics | §20.14.4 |
| scalar one | yes | selected value | selected NULL preserved | one-row success | one scalar value | child final-row semantics | §20.14.4 |
| scalar many | first two final rows | no value | n/a | second successful row -> cardinality | n/a | later rows suppressed | §20.14.4 |
| scalar skipped branch | no | surrounding control result | surrounding rule | no child error | none | n/a | §§17.7.3, 20.14.8 |
| EXISTS empty | until exhaustion | FALSE | non-null Boolean | success | zero child final rows | child order irrelevant | §20.14.5 |
| EXISTS nonempty | through first final row | TRUE | non-null Boolean | later rows suppressed | at least one child row | first demanded row only | §20.14.5 |
| EXISTS projection-only error | irrelevant projection value no | TRUE/FALSE from relation | non-null | no projection-only error | relation cardinality only | projection order irrelevant | §20.14.5 |
| IN empty | left then complete empty build | FALSE | exact 3VL | success | RHS duplicates absent | unordered build | §20.14.6 |
| IN match | left then complete build | TRUE | exact 3VL | build errors precede result | duplicates truth-neutral | unordered build | §20.14.6 |
| IN no match | left then complete build | FALSE absent NULL | exact 3VL | success | duplicates truth-neutral | unordered build | §20.14.6 |
| IN RHS NULL | left then complete build | UNKNOWN absent match | NULL Boolean | success | NULL marker | unordered build | §20.14.6 |
| left NULL/nonempty | left then complete build | UNKNOWN | NULL Boolean | success | RHS multiplicity truth-neutral | unordered build | §20.14.6 |
| NOT IN RHS NULL | ordinary NOT after IN | UNKNOWN absent match | NULL Boolean | success | RHS multiplicity truth-neutral | unordered build | §20.14.6 |
| derived table | ordinary relational demand | child rows | child values | ordinary child errors | exact child multiplicity | no outer order by itself | §20.14.3 |
| derived ORDER | as child requires | child bag | child values | ordinary child errors | unchanged except child operators | not exported by itself | §20.14.10 |
| derived LIMIT | as child requires | selected child rows | child values | Limit errors upstream-owned | exact selected bag | child ORDER controls selection | §§20.12, 20.14.10 |
| unsupported correlation | no executable demand | no plan | n/a | bind-time UnsupportedCorrelation | n/a | n/a | §§19.18, 20.14.2 |

#### Rewrite-safety matrix

| Rewrite family | Executable change? | Demand change? | Exact proof required? | Operand order preserved? | Provenance preserved? | Bag safe? | Order safe? | NULL safe? | Aggregate ordinal safe? |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| constant folding | yes | possible | §17.10.2 equivalence | yes | folded-owner rule | yes | yes | yes | yes |
| Boolean simplification | yes | possible | when observable demand changes | yes | yes | yes | yes | 3VL exact | yes |
| predicate pushdown | yes | often | yes for enlarged/moved observable demand | yes | yes | yes | join/order constraints | yes | yes |
| projection pruning | yes | suppresses candidate work | ordinary error-capable expression yes | yes | yes | yes | yes | yes | yes |
| expression canonicalization | metadata only unless exact legal transform | no executable reordering | exact folding rule where executable | yes | yes | yes | yes | yes | yes |
| join graph extraction | no, metadata | no | no for metadata | executable tree unchanged | metadata no diagnostic | yes | outer boundaries retained | yes | yes |
| equality classes | metadata or derived predicate | possible for derived executable | exact proof for executable effect | yes | inherited/exact | yes | outer boundaries retained | yes | yes |
| constant propagation | may synthesize predicate | possible | yes | yes | exact source/safe synthetic | yes | yes | yes | yes |
| contradiction detection | may replace relation | may suppress | yes plus demand safety | yes | yes | exact bag empty | schema/order class preserved | TRUE-only context exact | yes |
| trusted-key metadata | no by itself | no | enforced fact required for later rewrite | n/a | metadata no diagnostic | n/a | n/a | n/a | yes |
| semantic emptiness | may replace relation | possible | approved proof plus demand safety | yes | yes | exact empty semantics | output order class preserved | context exact | yes |

#### Demand matrix

| Context | Demand domain | May skip? | Specialized owner | Error observable? | Rewrite proof requirement |
|---|---|---|---|---:|---|
| Filter predicate | each retention occurrence | only if row not semantically reached | §§20.6, 20.17 | yes | preserve or exact insensitivity |
| ordinary Project | each expression for each produced input occurrence | no by ancestor nonuse alone | §§20.7, 20.17.4 | yes | preserve or exact total/error-free |
| JOIN ON | semantically required candidate pairs | pairs outside domain | §§20.8, 20.17 | yes | preserve pair domain or exact proof |
| group key | applicable aggregate input rows | only undemanded rows | §20.9 | yes | preserve or exact proof |
| aggregate argument | applicable rows/occurrence | descriptor-defined NULL handling occurs after expression | §§20.9, 29.3 | yes | preserve or exact proof |
| HAVING | produced groups whose retention is determined | absent groups | §20.9 | yes | preserve or exact proof |
| ORDER key | logical rows reaching Sort | rows not reaching Sort | §20.11 | yes | preserve or exact proof |
| scalar subquery | demanded occurrence and work for up to two final rows | skipped outer control path | §20.14.4 | yes | preserve cardinality/error precedence |
| EXISTS projection | row existence, not projection-only values | projection-only work | §20.14.5 | projection-only no | specialized irrelevance or exact proof |
| IN build | complete final RHS after demanded left | only while occurrence dormant | §20.14.6 | yes | preserve left/build precedence |
| CASE branch | source-ordered selected path | unselected branches | §17.7.3 | yes | preserve child/control order |
| AND/OR RHS | Chapter-17 short-circuit path | skipped RHS | §17.7.3 | yes | preserve child/control order |

#### Provenance matrix

| Expression state | SourceSpan | Cast provenance | Error origin | Precedence identity | Allowed? | Governing owner |
|---|---|---|---|---|---:|---|
| unchanged source expression | original | original explicit/implicit | original | original | yes | §20.17 |
| moved source expression | unchanged | unchanged | unchanged | unchanged | demand-safe only | §§20.17, 20.17.3 |
| shared source expression | same canonical span | same | same semantic origin | same | yes | §20.17 |
| duplicated source expression | copied to each | copied | copied | copied | only if V20-15 legal | §20.17 |
| error-capable replacement | replaced occurrence span | preserved | preserved category/origin | preserved | exact only | §20.17 |
| safe spanless synthetic | none | n/a | cannot error | n/a | exact total/error-free only | §20.17 |
| error-capable synthetic mapped to source | inherited one source span | inherited as applicable | inherited | inherited | exact one-source implementation only | §20.17 |
| illegal source-less error-capable synthetic | none | unknown | none | none | no | §20.17 |
| non-executable metadata | none required | n/a | cannot originate diagnostic | n/a | yes as metadata | §§20.17, 20.17.5 |
| Chapter-17 folded expression | §17.10.2 rule | §17.10.2 rule | §17.10.2 rule | §17.10.2 rule | delegated | §17.10.2 |

#### Aggregate-occurrence matrix

| Case | Aggregate ordinal | LogicalSlotId | Logical occurrence count | Rewrite behavior | Provenance | Error selection |
|---|---|---|---:|---|---|---|
| one aggregate | upstream ordinal | fresh | one | retained unless legally removed | source occurrence | its ordinal |
| same descriptor twice at different source occurrences | two ordinals | two fresh slots | two | no merge/renumber | distinct source origins | lowest failing ordinal |
| SELECT aggregate + ORDER alias | existing ordinal | existing output slot | one | reference reuse | same origin | existing ordinal |
| legal demand-safe removal | removed ordinal disappears | slot retires | one fewer | survivors unchanged/no compact | removed with occurrence | survivors keep ordinals |
| physical shared computation | every original ordinal | every original slot | unchanged | exact logical map | each origin retained | lowest original failing ordinal |
| aggregate across nested query block | ordinal local to each block | slots unique statement-wide | distinct per source occurrence | no cross-block move | each block occurrence | per frozen ordinal rule |

#### No-FROM/constant-query matrix

| Query shape | Source cardinality | Final bag cardinality | Slots | Order | Errors |
|---|---:|---:|---|---|---|
| `SELECT 1` | one zero-column row | one | fresh Project output | unordered singleton | none |
| `SELECT NULL` | one zero-column row | one | fresh typed Project output | unordered singleton | bound NULL rules |
| `SELECT 1 WHERE TRUE` | one | one | Filter pass-through then fresh Project | unordered singleton | predicate demanded |
| `SELECT 1 WHERE FALSE` | one | zero | output schema still defined | unordered empty | no projection-row evaluation |
| `SELECT COUNT(*)` | one | one global aggregate row | fresh aggregate output | unordered singleton | Chapter-29 value rule |
| `SELECT COUNT(*) WHERE FALSE` | one source, zero after Filter | one global aggregate row | fresh aggregate output | unordered singleton | count zero |
| `SELECT 1 ORDER BY 1` | one | one | Project then Sort pass-through | semantic key order | ordinary key demand |
| `SELECT 1 LIMIT 0` | one | zero | schema/slot preserved by Limit | unordered empty | no new count error |
| `SELECT 1 OFFSET 1` | one | zero | schema/slot preserved by Limit | unordered empty | no error |
| scalar subquery containing no-FROM SELECT | independent one-row source | zero/one after child clauses | independent child slots | child order rules | scalar cardinality rules |

#### Error/diagnostic matrix

| Condition | Logical demand point | Category owner | SourceSpan/origin | Rewrite effect | Transaction consequence owner |
|---|---|---|---|---|---|
| scalar subquery second row | second successfully produced final row | §§20.14.4, 39.3 | subquery occurrence/cardinality origin | check removable only by exact proof | §39.1 |
| error-capable Filter | each retention decision | Chapters 17, 39 | predicate origin | no unsafe suppression/enlargement | §39.1 |
| error-capable Project | each ordinary output expression/row | Chapters 17, 39 | expression origin | ancestor nonuse insufficient | §39.1 |
| JOIN predicate error | demanded candidate pair | Chapters 17, 39 | ON expression origin | pair-domain movement proof required | §39.1 |
| aggregate argument error | demanded argument row | Chapters 17, 29, 39 | argument origin | no unsafe removal/duplication | §39.1 |
| aggregate finalization | after complete demanded input | §§29.3.7, 39 | lowest failing source ordinal | physical sharing preserves selection | §39.1 |
| HAVING error | produced group retention | Chapters 17, 39 | HAVING origin | group-demand preservation | §39.1 |
| ORDER-key error | row reaching Sort | Chapters 17, 39 | ORDER expression origin | sort removal demand-safe only | §39.1 |
| IN build error | complete demanded final RHS | §§20.14.6, 39 | RHS expression origin | precedes possible match | §39.1 |
| unsafe pushdown/pruning/empty | changed demand domain | §20.17 | original source origin | reject rewrite | §39.1 |
| error-capable synthesized/replacement | replacement demand point | §§20.17, 39 | exact inherited origin or none | exact mapping required or invalid | §39.1 |

#### Exact-proof matrix

| Fact | Exact? | May authorize demand-changing rewrite? | Owner | Reason |
|---|---:|---:|---|---|
| exact literal evaluation | yes | yes when complete/error-safe | §§17.10.2, 35.2 | closed scalar semantics |
| exact type/domain rule | yes | yes when complete/error-safe | Chapters 17/35 | authoritative domain |
| exact nullability | yes | yes within its exact domain | §§20.16, 35.2 | semantic property |
| trusted enforced constraint | yes | yes within whitelist | §§20.17.9, 35.2 | runtime-enforced fact |
| exact semantic emptiness | yes | only with separate demand safety | §§20.17.10, 35.2 | output fact does not erase errors by itself |
| histogram | no | no | Chapter 34 | approximate/stale statistics |
| selectivity | no | no | Chapter 35 | estimate only |
| sample | no | no | Chapter 34 | incomplete observation |
| cardinality estimate | no | no | Chapter 35 | numerical planning metadata |
| cost estimate | no | no | Chapter 38 | physical choice only |
| heuristic/likely range | no | no | optimizer policy | not a semantic fact |

#### Determinism matrix

| Perturbation | May affect logical schema? | Slot equality? | Bag? | Required ordering? | Error/provenance? | Allowed effect |
|---|---:|---:|---:|---:|---:|---|
| hash seed | no | no | no | no | no | internal work/cost only |
| map iteration | no | no | no | no | no | internal work/cost only |
| pointer addresses | no | no | no | no | no | none observable |
| allocation order | no | no | no | no | no | opaque representations may differ |
| LogicalSlotId numeric encoding | no | no after alpha-renaming | no | no | no | numeric labels differ |
| TableId numeric ordering | no | no | no | no | no | identity labels only |
| ColumnId numeric ordering | no | no | no | no | no | lineage labels only |
| physical scan order | no | no | no | no semantic order created | no | unordered sequence/subbag variation |
| join algorithm | no | no | no | no semantic order created | no | physical work/order |
| sort algorithm | no | no | no | no outside allowed ties | no | tie permutations |
| optimizer rule traversal | no | no | no | no | no | equivalent plan choice |
| scheduler interleaving | no | no | no | no | no | timing only |
| source-buffer lifetime | no | no | no | no | no dangling/changed origin | retain/materialize presentation choice |

#### Cross-chapter composition matrix

| Owner | Mechanism | Upstream input | Chapter-20 responsibility | Downstream handoff | Verification owner | Duplication? |
|---|---|---|---|---|---|---:|
| Chapter 16 | IDs/descriptors/SchemaVer/presentation/snapshot | immutable catalog semantics and visible rows | consume stable lineage and source occurrences | logical schema/Get to physical planning | V16 + V20-1/2/4 | no |
| Chapter 17 | types/3VL/order/casts/evaluation/folding | resolved scalar semantics | preserve executable values, demand, order, and errors | expression execution | Type-System/Expression + V20-5/15/17 | no |
| Chapter 18 | source order/SourceSpan/lifetime | canonical source origins | preserve provenance without forced buffer retention | diagnostics | V18 + V20-18 | no |
| Chapter 19 | BindingId/bound outputs/ordinals/ORDER/LIMIT | fully bound statement | construct logical identity/relations without rebinding | Chapter-20 plan | V19 + V20-1/9/11 | no |
| Chapter 20 | logical relations/properties/rewrites | bound statement and exact facts | define all logical semantics | physical planning and diagnostics | V20-1–27 | canonical local statement only |
| Chapter 21 | DML/DDL statement semantics | logical DML relational child | preserve hidden row identity and resolved target handoff | write execution | V20-21 | no Chapter-21 redefinition |
| §22.4 | physical vocabulary | validated logical nodes | prohibit physical choice in logical identity | physical operator selection | V20-22 | no |
| Chapters 24–26 | memory/state/pipeline | logical demand/state contracts | remain implementation-independent | physical memory/expression/pipeline | V20-14/22/23 | no |
| §27.9 | PhysicalLimit | LogicalLimit counts/order class | define occurrence selection | conforming physical Limit | V20-11/22 | no |
| Chapter 29 | aggregate values/errors/ordinal | descriptor/ordinal semantics | preserve logical aggregate occurrences/mapping | aggregate execution | Aggregate families + V20-8/9/24 | no |
| §§30.1/30.3 | ties/comparator | physical comparator contract | carry exact logical key requirement | sort realization | Sort families + V20-10 | no |
| §§34.1/35.2 | estimates/exact proofs | disjoint estimate/proof domains | accept only exact semantic authority | optimizer reasoning | Semantic Emptiness + V20-16/19 | no |
| §35.20 | Limit cardinality | logical count values | own semantic selection/cardinality | estimate derivation | V20-11/19 | no |
| §37.17 | subquery physical planning | logical mode/fallback | require conforming rewrite/fallback semantics | physical subquery plan | V20-12–14/22 | no |
| §§39.1–39.3 | categories/transaction effects | semantic/runtime errors | preserve category/origin/precedence | command/transaction outcome | Error families + V20-18/24 | no |
| §§40.2/40.5/40.8 | EXPLAIN presentation | validated logical representation | define logical information only | UI/presentation and physical analysis | V20-22 | no format duplication |
| §41 | verification architecture | frozen architecture obligations | provide deterministic independent procedures | maintained verification contract | V20-25–27 | no architecture restatement as oracle |

#### Logical-semantics procedure index

| Requirement | Verification |
|---|---|
| no physical algorithm mandate | V20-22 |
| no allocator/ID representation mandate | V20-3/23 |
| logical-plan boundary precise | V20-1 |
| LogicalSlotId precise | V20-2/3 |
| bag semantics precise | V20-4 |
| join semantics precise | V20-6/7 |
| LEFT nullability precise | V20-7 |
| grouping/DISTINCT precise | V20-8 |
| order semantics precise | V20-10 |
| Limit semantics precise | V20-11 |
| subquery semantics precise | V20-12–14 |
| demanded evaluation precise | V20-15 |
| rewrite proof boundary precise | V20-16/19 |
| operand order precise | V20-17 |
| aggregate occurrence precise | V20-9 |
| provenance precise | V20-18 |
| logical/physical ownership precise | V20-22 |
| EXPLAIN ownership precise | V20-22 |
| deterministic methodology | harness/V20-23 |
| independent oracles | oracle registry |
| implementation freedom | V20-22/23/26 |

#### High-level case matrix

| Fixture | Architecture owner | Independent oracle | Expected semantic result | Verification family |
|---|---|---|---|---|
| self join same column | §§20.2, 20.4 | occurrence/lineage graph | distinct BindingIds and slots | V20-3 |
| `SELECT a,a` | §§20.2, 20.7 | schema occurrence graph | two fresh output slots | V20-3 |
| equivalent Project expressions | §§20.2, 20.7 | schema occurrence graph | distinct slots/one row per input | V20-3/5 |
| nested child slot domain | §20.2 | whole-statement graph | no collisions | V20-3 |
| derived slot remapping | §§20.2, 20.14.3 | boundary map | fresh outer slots | V20-3/14 |
| duplicate Values rows | §20.5 | multiset | duplicates retained | V20-4 |
| duplicate Get rows | §20.4 | visibility/multiset | visible occurrences retained | V20-4 |
| no-FROM SELECT | §§20.5, 20.15 | singleton zero-column source | one source row | V20-1/4 |
| Filter TRUE/FALSE/UNKNOWN | §20.6 | 3VL selector | one/zero/zero | V20-5 |
| INNER duplicate matches | §20.8.1 | cross product | every TRUE pair | V20-6 |
| LEFT no match | §20.8.1 | per-left match count | one NULL extension | V20-6 |
| LEFT multiple matches | §20.8.1 | per-left match count | every TRUE pair, no extension | V20-6 |
| CROSS duplicates | §20.8.1 | cross product | `m*n` | V20-6 |
| LEFT NOT NULL right column | §20.8.3 | nullability transform | logically nullable/catalog unchanged | V20-7 |
| DISTINCT NULL | §§20.9–20.10 | partition oracle | one NULL class | V20-8 |
| DISTINCT NaN | §§20.9–20.10 | normalized FLOAT partition | one NaN class | V20-8 |
| DISTINCT signed zero | §§20.9–20.10 | normalized FLOAT partition | one zero class | V20-8 |
| GROUP BY NULL | §20.9 | partition oracle | one NULL group | V20-8 |
| global aggregate empty | §20.9 | grouping cardinality | one group | V20-8 |
| explicit grouping empty | §20.9 | grouping cardinality | zero groups | V20-8 |
| HAVING UNKNOWN | §20.9 | 3VL selector | group removed | V20-8 |
| Sort NULL | §20.11 | order oracle | ASC first/DESC last | V20-10 |
| Sort NaN | §20.11 | §30.3 comparator oracle | canonical FLOAT order | V20-10 |
| equal ORDER ties | §20.11 | tie equivalence | any equal-key permutation | V20-10 |
| unordered final SELECT | §§20.1, 20.14.10 | bag oracle | no physical order | V20-10/23 |
| ordered Limit | §20.12 | sequence slice | exact ordered survivors | V20-11 |
| unordered Limit | §20.12 | subbag predicate | any valid exact-cardinality subbag | V20-11 |
| LIMIT 0 | §20.12 | count formula | empty, schema retained | V20-11 |
| oversized OFFSET | §20.12 | count formula | empty/no error | V20-11 |
| scalar zero rows | §20.14.4 | cardinality state machine | typed NULL | V20-12 |
| scalar more than one | §20.14.4 | cardinality state machine | second-row error | V20-12 |
| EXISTS projection-only error | §20.14.5 | existence-demand oracle | irrelevant projection error suppressed | V20-13 |
| IN RHS NULL | §20.14.6 | repeated equality 3VL | UNKNOWN absent match | V20-13 |
| derived ORDER | §§20.14.3, 20.14.10 | order-boundary oracle | no outer order by itself | V20-14 |
| duplicate aggregate source occurrence | §§20.9, 29.3.7 | occurrence table | two ordinals/slots | V20-9 |
| ORDER alias aggregate reuse | §§19.13, 20.9 | output identity table | no new occurrence | V20-9 |
| pushdown error case | §§20.17, 20.17.3 | demand relation | reject without exact proof | V20-16 |
| projection-pruning error case | §§20.7, 20.17.4 | demand relation | retain ordinary expression | V20-16 |
| contradiction-to-empty error case | §§20.17, 20.17.10 | proof+demand oracle | reject unsafe erasure | V20-16 |
| comparison reversal | §20.17.5 | ordered tree serializer | executable rewrite rejected | V20-17 |
| safe synthetic TRUE | §20.17 | proof/origin table | spanless only if total/error-free | V20-18 |
| source-less error-capable synthetic | §20.17 | proof/origin table | invalid | V20-18 |
| moved erroring source expression | §20.17 | provenance map | same origin plus demand-safe | V20-18 |
| physical aggregate sharing | §20.9 | occurrence/error mapping | identities and lowest ordinal retained | V20-9 |

### Complete Chapter-20 architecture-obligation coverage map

The following ledger is the authoritative atomic inventory for this verification section.
`Procedure` names the deterministic V20 family; `Oracle` names an independent expected-
result construction from the registry or matrices above.

| Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Independent oracle |
|---|---|---|---|---|
| V20-1.1 — Logical planning consumes a fully bound Chapter-19 statement | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.2 — Logical planning performs no SQL-name rebinding | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.3 — BindingId equality and inequality are preserved | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.4 — Bound column TableId and ColumnId identities are preserved | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.5 — Bound scalar TypeId and nullability are preserved | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.6 — Explicit and implicit cast provenance is preserved | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.7 — Aggregate ordinals are consumed rather than reassigned | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.8 — ORDER alias and ordinal output identities are consumed unchanged | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.9 — LIMIT/OFFSET bound-expression and validated-count ownership remains Chapter 19 | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.10 — Logical plans are immutable relational-semantic trees once published | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.11 — Every logical node exposes kind, children, output schema, properties, and debug representation semantically | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.12 — Logical operators describe required relations rather than physical algorithms | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.13 — Helper nodes represent real relation or statement semantics rather than physical algorithms | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.14 — Canonical SELECT shape begins with explicit FROM/JOIN or the no-FROM source | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.15 — Canonical SELECT clause composition follows §20.15 before legal rewrites | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-1.16 — Canonical logical shape is not asserted as physical execution order | §§20.1, 20.3, 20.13, 20.15; Chapter 19 | V20-1 | V20-1 procedure and applicable mandatory-matrix row | bound-to-logical oracle and canonical-shape table |
| V20-2.1 — Every logical output position carries a LogicalSlotId | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.2 — Every output position carries deterministic display metadata where applicable | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.3 — Every output position carries resolved logical type | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.4 — Every output position carries contextual logical nullability | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.5 — Every output position carries lineage where applicable | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.6 — Internal/system visibility metadata remains distinguishable | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.7 — One LogicalSlotId identifies exactly one logical output occurrence | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.8 — LogicalSlotId is distinct from TableId | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.9 — LogicalSlotId is distinct from ColumnId | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.10 — LogicalSlotId is distinct from BindingId | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.11 — LogicalSlotId is distinct from persisted heap SlotId | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.12 — LogicalSlotId is distinct from output ordinal | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.13 — LogicalSlotId is distinct from aggregate ordinal | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.14 — LogicalSlotId is distinct from expression-equivalence identity | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.15 — LogicalSlotId is distinct from pointer/object identity | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-2.16 — LogicalSlotId is not a permanent physical vector position | §20.2 | V20-2 | V20-2 procedure and applicable mandatory-matrix row | ordered schema algebra and semantic-identity fixture |
| V20-3.1 — LogicalSlotId uniqueness spans the whole top-level statement | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.2 — Nested query-block outputs participate in the same uniqueness domain | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.3 — Scalar/EXISTS/IN side-plan outputs participate in the same uniqueness domain | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.4 — Derived-table child outputs participate in the same uniqueness domain | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.5 — DML relational-child outputs participate in the same uniqueness domain | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.6 — No distinct logical output occurrences share a LogicalSlotId | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.7 — An eliminated LogicalSlotId is never reused during the statement lifetime | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.8 — Different top-level statements may reuse opaque representation values | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.9 — LogicalSlotId is runtime-only | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.10 — LogicalSlotId is absent from catalog and WAL state | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.11 — LogicalSlotId is not recovered or encoded in tuple/page formats | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.12 — LogicalSlotId width and numeric allocation order are nonsemantic | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.13 — LogicalGet creates a fresh slot per exposed BindingId plus ColumnId occurrence | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.14 — Self-join Get occurrences have distinct BindingIds and slots | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.15 — LogicalValues creates a fresh slot per output column position | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.16 — LogicalProject creates a fresh slot per declared output position | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.17 — SELECT a,a has distinct Project output slots | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.18 — SELECT a AS x,a AS y has distinct Project output slots | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.19 — SELECT a+1,a+1 has distinct Project output slots | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.20 — LogicalAggregate creates fresh grouping-key output slots | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.21 — LogicalAggregate creates a fresh slot per aggregate occurrence | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.22 — LogicalFilter passes child slots through unchanged | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.23 — LogicalDistinct passes child slots through unchanged | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.24 — LogicalSort passes child slots through unchanged | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.25 — LogicalLimit passes child slots through unchanged | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.26 — LogicalJoin passes child slots through unchanged | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.27 — Derived-table export remaps each child slot to a fresh outer slot | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.28 — Derived remap retains child slot, outer slot, derived BindingId, and exported metadata | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.29 — A surviving identical logical output occurrence preserves its slot through rewrite | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.30 — A newly introduced or duplicated output occurrence receives a fresh slot | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.31 — An eliminated occurrence disappears and remains retired | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-3.32 — Equivalent value computation alone never proves slot identity | §§20.2, 20.4–20.12, 20.14.3, 20.17 | V20-3 | V20-3 procedure and applicable mandatory-matrix row | whole-statement occurrence graph with retired-ID set |
| V20-4.1 — Logical relations use bags of row occurrences | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.2 — Equal SQL-valued rows remain distinct occurrences | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.3 — Multiplicity and semantic row order are independent | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.4 — No unnamed operator implicitly collapses value-equivalent rows | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.5 — LogicalGet emits one occurrence per snapshot-visible logical base row | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.6 — Distinct visible base rows with equal values remain distinct occurrences | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.7 — Non-visible physical historical versions add no logical occurrences | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.8 — LogicalGet performs no duplicate elimination | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.9 — LogicalGet establishes no physical scan order | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.10 — LogicalValues emits one occurrence per listed row | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.11 — LogicalValues preserves duplicate listed rows | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.12 — VALUES source order is not an SQL result order by itself | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.13 — The no-FROM source has exactly one row occurrence | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.14 — The no-FROM source has exactly zero columns | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.15 — LogicalSort preserves the complete input occurrence bag | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.16 — LogicalLimit removes occurrences without duplicate-equivalence collapse | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.17 — A derived-table boundary preserves child occurrence multiplicity | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.18 — Only DISTINCT and grouping collapse value-equivalence classes | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-4.19 — Filter, Join, and Limit change multiplicity only by their explicit contracts | §§20.1, 20.4–20.5, 20.10–20.12, 20.14.3 | V20-4 | V20-4 procedure and applicable mandatory-matrix row | fixture-tagged mathematical multiset |
| V20-5.1 — LogicalFilter output schema equals child schema | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.2 — LogicalFilter preserves child slots | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.3 — Filter TRUE maps one input occurrence to one output occurrence | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.4 — Filter FALSE maps one input occurrence to zero output occurrences | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.5 — Filter UNKNOWN maps one input occurrence to zero output occurrences | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.6 — Filter treats duplicate input occurrences independently | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.7 — Filter performs no duplicate elimination | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.8 — Filter predicate is demanded for each retention decision | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.9 — LogicalProject emits one output occurrence per input occurrence | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.10 — Project preserves equal-valued row multiplicity | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.11 — Project performs no implicit DISTINCT | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.12 — Project output expression order follows declared output positions | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.13 — Every ordinary Project expression is demanded for every produced row | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.14 — Ancestor slot nonuse alone does not suppress ordinary Project demand | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.15 — EXISTS projection-value irrelevance is a specialized exception | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-5.16 — Project demand/error semantics are independent of physical evaluation strategy | §§20.6–20.7, 20.14.5, 20.17 | V20-5 | V20-5 procedure and applicable mandatory-matrix row | per-occurrence 3VL selector and projection constructor |
| V20-6.1 — INNER and LEFT candidate multiplicity is defined over L cross R occurrences | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.2 — Join pair semantics do not require materializing a Cartesian product | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.3 — INNER ON TRUE emits exactly one occurrence per candidate pair | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.4 — INNER ON FALSE emits no occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.5 — INNER ON UNKNOWN emits no occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.6 — INNER preserves every TRUE duplicate occurrence pair | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.7 — INNER is not first-match or existence semantics | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.8 — LEFT emits one joined occurrence for every TRUE right match | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.9 — LEFT treats FALSE as a nonmatch | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.10 — LEFT treats UNKNOWN as a nonmatch | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.11 — LEFT with zero TRUE matches emits exactly one NULL-extended occurrence per left occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.12 — LEFT with one TRUE match emits exactly one matched occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.13 — LEFT with multiple TRUE matches emits all matched occurrences | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.14 — LEFT emits no extra NULL-extended row when any TRUE match exists | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.15 — LEFT with only FALSE/UNKNOWN candidates emits one NULL-extended occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.16 — CROSS emits one occurrence for every L cross R pair | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.17 — CROSS has no ON predicate | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.18 — CROSS duplicate multiplicity is m times n | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.19 — INNER with empty left is empty | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.20 — INNER with empty right is empty | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.21 — CROSS with empty left is empty | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.22 — CROSS with empty right is empty | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.23 — LEFT with empty left is empty | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.24 — LEFT with empty right emits one NULL-extended row per left occurrence | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-6.25 — Join predicate errors are observed only for semantically demanded candidate pairs | §20.8.1 | V20-6 | V20-6 procedure and applicable mandatory-matrix row | finite occurrence-pair cross product and per-left TRUE-match counts |
| V20-7.1 — Join output schema contains all left outputs first | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.2 — Join output schema then contains all right outputs | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.3 — Join schema order ignores TableId numeric order | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.4 — Join schema order ignores ColumnId numeric order | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.5 — Join schema order ignores slot numeric representation | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.6 — Join creates no fresh slots merely for crossing the boundary | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.7 — LEFT right outputs become logically nullable | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.8 — LEFT left-output nullability remains unchanged | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.9 — LEFT contextual nullability does not alter catalog NOT NULL metadata | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.10 — LEFT contextual nullability does not alter TypeId | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.11 — LEFT contextual nullability does not alter BindingId, slot, or lineage | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.12 — No logical join establishes semantic row order | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.13 — No physical pair-generation sequence is a join result-order oracle | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-7.14 — ON and WHERE remain semantically distinct | §§20.8.1–20.8.3 | V20-7 | V20-7 procedure and applicable mandatory-matrix row | join schema concatenation and contextual-nullability transform |
| V20-8.1 — Grouping runtime equality is distinct from Chapter-19 structural expression equality | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.2 — Grouping places NULL values in one equivalence class | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.3 — Grouping equates negative and positive FLOAT64 zero | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.4 — Grouping equates all canonical NaNs | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.5 — Grouping compares VARCHAR by exact binary bytes | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.6 — Grouping applies equality componentwise to multi-column keys | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.7 — Grouping retains unequal normal values in separate classes | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.8 — LogicalAggregate emits one row occurrence per runtime group | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.9 — No-key aggregate forms one global group over empty input | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.10 — Explicit grouping over empty input forms zero groups | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.11 — Global aggregate row count remains Chapter-20-owned while values remain Chapter-29-owned | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.12 — HAVING TRUE retains one produced group occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.13 — HAVING FALSE removes the produced group occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.14 — HAVING UNKNOWN removes the produced group occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.15 — LogicalDistinct is explicit duplicate-elimination semantics | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.16 — LogicalDistinct emits one occurrence per grouping-equivalence class | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.17 — LogicalDistinct preserves child output schema | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.18 — LogicalDistinct preserves child LogicalSlotIds | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.19 — DISTINCT NULL/NULL collapses to one occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.20 — DISTINCT NaN/NaN collapses to one occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.21 — DISTINCT signed zeros collapse to one occurrence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.22 — DISTINCT byte-identical VARCHAR values collapse | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.23 — DISTINCT byte-distinct VARCHAR values remain distinct | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.24 — DISTINCT composite rows use componentwise equivalence | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.25 — DISTINCT result row order is not established | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-8.26 — Grouping and DISTINCT never substitute ordinary NULL comparison semantics | §§20.9–20.10; §§17.7, 17.10.3; §29.5 | V20-8 | V20-8 procedure and applicable mandatory-matrix row | normalized grouping-equivalence partition |
| V20-9.1 — LogicalAggregate outputs only group-key and aggregate-derived values | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.2 — Each bound source aggregate occurrence becomes one logical aggregate occurrence | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.3 — Every logical aggregate entry carries its upstream canonical ordinal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.4 — Aggregate ordinal remains query-block-local source-occurrence identity | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.5 — Chapter 20 does not assign ordinal from logical traversal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.6 — Identical aggregate descriptors may have distinct logical occurrences | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.7 — Identical aggregate arguments may have distinct logical occurrences | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.8 — Distinct aggregate occurrences receive distinct fresh output slots | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.9 — Aggregate ordinal and LogicalSlotId remain distinct identities | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.10 — ORDER BY alias reuses an existing aggregate output occurrence | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.11 — ORDER BY alias creates no new aggregate ordinal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.12 — Rewrite cannot renumber aggregate ordinals | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.13 — Rewrite cannot compact surviving aggregate ordinals | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.14 — Rewrite cannot reuse a removed aggregate ordinal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.15 — Rewrite cannot merge distinct aggregate occurrences | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.16 — Rewrite cannot duplicate one occurrence under the same ordinal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.17 — Rewrite cannot move an occurrence across query blocks | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.18 — Legal aggregate-occurrence removal leaves surviving ordinals unchanged | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.19 — Physical computation sharing may preserve multiple logical occurrences | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.20 — Physical sharing retains every logical output slot | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.21 — Physical sharing retains every aggregate ordinal | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.22 — Physical sharing retains diagnostic provenance and output mapping | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.23 — Physical sharing retains Chapter-29 lowest-ordinal error selection | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.24 — Aggregate occurrence provenance survives logical rewrites | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-9.25 — Aggregate argument/group/HAVING demand composes with V20-15 | §20.9; §§19.10–19.13; §29.3.7 | V20-9 | V20-9 procedure and applicable mandatory-matrix row | source-occurrence/ordinal table plus Chapter-29 error oracle |
| V20-10.1 — LogicalSort carries bound sort-key sequence without rebinding | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.2 — LogicalSort preserves ASC direction | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.3 — LogicalSort preserves DESC direction | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.4 — LogicalSort stores resolved NULL placement | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.5 — ASC defaults to NULLS FIRST | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.6 — DESC defaults to NULLS LAST | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.7 — LogicalSort uses the canonical SQL type comparator | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.8 — FLOAT64 order uses canonical NaN and signed-zero semantics | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.9 — LogicalSort preserves child schema | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.10 — LogicalSort preserves child slots | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.11 — LogicalSort preserves every input row occurrence | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.12 — LogicalSort performs no duplicate elimination | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.13 — LogicalSort establishes semantic key-sequence order | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.14 — Rows equal on all ORDER keys have no stable relative-order guarantee | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.15 — Sort-key expressions are demanded for rows to which Sort applies | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.16 — No ORDER BY leaves a result unordered unless another exact rule establishes order | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.17 — Heap/page/RID/index/hash order is not an SQL result order | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-10.18 — Physical sort algorithm may alter only unspecified tie order | §§20.1, 20.11, 20.14.10; §§30.1, 30.3 | V20-10 | V20-10 procedure and applicable mandatory-matrix row | canonical SQL comparator plus tie-equivalence classes |
| V20-11.1 — Chapter 19 owns LIMIT/OFFSET admissibility | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.2 — Chapter 19 owns execution-start constancy and type normalization | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.3 — Chapter 19 owns mandatory folding and count-expression errors | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.4 — LogicalLimit receives a validated nonnegative INT64 offset | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.5 — LogicalLimit receives an optional validated nonnegative INT64 limit | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.6 — LogicalLimit applies OFFSET before LIMIT | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.7 — after_offset equals max of zero and n minus o | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.8 — No-LIMIT output cardinality equals after_offset | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.9 — Limited output cardinality equals min of after_offset and l | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.10 — LogicalLimit never requires o plus l arithmetic | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.11 — LIMIT-only means offset zero | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.12 — OFFSET-only returns all remaining occurrences | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.13 — OFFSET zero removes nothing | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.14 — LIMIT zero returns an empty bag | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.15 — Limit larger than remaining cardinality returns every remainder occurrence | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.16 — Offset at least child cardinality returns empty | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.17 — Zero and oversized cases introduce no new error | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.18 — Ordered child selection skips the first o ordered occurrences | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.19 — Ordered child selection takes at most the next l occurrences | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.20 — Ordered child survivor order is preserved | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.21 — Unordered child has no canonical first occurrence | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.22 — Unordered Limit accepts any valid exact-cardinality child subbag | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.23 — Unordered Limit output remains unordered | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.24 — No physical traversal order becomes Limit semantics | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.25 — LogicalLimit preserves schema, slots, metadata, type, nullability, and lineage | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.26 — LogicalLimit does not deduplicate | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-11.27 — PhysicalLimit and optimizer cardinality conform to Chapter-20 semantics | §20.12; §19.14; §§27.9, 35.20 | V20-11 | V20-11 procedure and applicable mandatory-matrix row | ordered sequence slice or unordered exact-subbag predicate |
| V20-12.1 — Scalar subquery requires exactly one bound output column | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.2 — Scalar subquery result TypeId equals its bound output TypeId | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.3 — Scalar subquery result is conservatively nullable | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.4 — Zero final rows produce a typed NULL scalar | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.5 — Exactly one final row produces its selected scalar value | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.6 — A SQL NULL selected value remains SQL NULL rather than empty | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.7 — Successful construction of a second final row raises CardinalityViolation | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.8 — Scalar cardinality observes final rows after all child relational clauses | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.9 — Scalar empty result does not remove the outer row | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.10 — Scalar consumer semantically demands at most two final rows | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.11 — Blocking work needed to establish final rows remains demanded | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.12 — Error constructing the first final selected value precedes cardinality | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.13 — Error constructing the second final selected value precedes cardinality | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.14 — After two successful rows cardinality precedes every later-row error | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.15 — Scalar never returns an arbitrary first/last row from a multirow result | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.16 — Exact final cardinality at most one may remove the check | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.17 — LIMIT 1 supplies an exact at-most-one cap | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.18 — Estimate or required_rows cannot remove cardinality checking | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.19 — DISTINCT applies before scalar cardinality | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-12.20 — OFFSET alone does not cap scalar cardinality | §§20.14.1, 20.14.4, 20.14.10–20.14.12 | V20-12 | V20-12 procedure and applicable mandatory-matrix row | scalar final-row cardinality state machine |
| V20-13.1 — EXISTS empty final relation returns non-null FALSE | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.2 — EXISTS nonempty final relation returns non-null TRUE | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.3 — NOT EXISTS is ordinary non-null Boolean NOT | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.4 — EXISTS stops after the first demanded final row | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.5 — EXISTS does not demand later rows after existence is known | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.6 — EXISTS does not demand projection-only scalar values | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.7 — EXISTS does not demand projection-only aggregate arguments/results | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.8 — EXISTS removes DISTINCT value work only when irrelevant to final-row existence | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.9 — EXISTS may remove ORDER-key value work used only for existence | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.10 — EXISTS still demands FROM/WHERE/grouping/HAVING and OFFSET/LIMIT existence work | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.11 — Global aggregate EXISTS performs ordinary child work before its one row unless exact proof permits skipping | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.12 — LIMIT zero makes EXISTS false | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.13 — Positive LIMIT permits first-row stop only after OFFSET | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.14 — IN binds one scalar left value and one scalar RHS output | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.15 — IN uses the Chapter-17 resolved equality/common type | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.16 — IN returns TRUE if any comparison is TRUE | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.17 — IN returns UNKNOWN when no TRUE and at least one comparison is UNKNOWN | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.18 — IN returns FALSE when no TRUE and no UNKNOWN | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.19 — IN over empty RHS is FALSE even for NULL left | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.20 — NOT IN over empty RHS is TRUE even for NULL left | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.21 — IN with nonempty RHS and NULL left is UNKNOWN | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.22 — IN exact non-NULL match is TRUE despite RHS NULLs | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.23 — IN no match plus RHS NULL is UNKNOWN | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.24 — NOT IN is exact 3VL NOT of IN | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.25 — RHS duplicates do not change IN truth | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.26 — IN equality normalization covers NaN, signed zero, VARCHAR, and numeric promotion | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.27 — First demanded IN evaluation evaluates left scalar before dormant build | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.28 — Demanded IN consumes and validates the complete final RHS before any probe result | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.29 — Any build error precedes a possible match result | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-13.30 — Later demanded outer selections reuse the completed occurrence build | §§20.14.5–20.14.6, 20.14.8–20.14.12; §17.10.3 | V20-13 | V20-13 procedure and applicable mandatory-matrix row | existence state machine and repeated-equality 3VL fold |
| V20-14.1 — Every expression-subquery occurrence has stable query-local identity | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.2 — Every expression subquery has one independent logical child | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.3 — Scalar, EXISTS, and IN modes remain semantically distinct | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.4 — NOT wrappers remain ordinary Boolean nodes | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.5 — Every supported mode has a conforming executable fallback | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.6 — Fallback support does not depend on a successful optimizer rewrite | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.7 — Each uncorrelated occurrence initializes at most once per statement attempt | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.8 — Occurrence initialization happens only on first semantic demand | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.9 — An undemanded occurrence never executes | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.10 — A successful occurrence state is reused for later outer rows | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.11 — A failed occurrence fails the statement and is not retried for a different outcome | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.12 — Skipped CASE and AND/OR branches leave subqueries dormant | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.13 — Empty outer input may leave a subquery dormant | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.14 — Statement-attempt retry discards prior subquery state | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.15 — Retry uses a fresh statement snapshot and retains logical CommandId | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.16 — Subquery child uses containing attempt snapshot and ReadEpochGuard | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.17 — Subquery child consumes no new CommandId | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.18 — Subquery child creates no nested transaction or lock owner | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.19 — Derived table receives one relation BindingId and namespace boundary | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.20 — Derived child outputs remap to fresh outer slots | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.21 — Derived boundary preserves child bag multiplicity | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.22 — Derived aliases/hidden columns do not leak outward | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.23 — Derived output order follows child projection positions | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.24 — Derived boundary is not a semantic materialization barrier | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.25 — Derived child ORDER does not by itself advertise outer order | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.26 — Derived ORDER is consumed by child LIMIT/OFFSET when applicable | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.27 — Outer ORDER establishes its own semantic order | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.28 — Correlation is rejected by Chapter 19 and creates no OuterRef/Apply plan | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.29 — Row-valued scalar/IN forms have no executable plan | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.30 — ANY/SOME/ALL quantified forms have no executable plan | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.31 — LATERAL/outer-reference forms have no executable plan | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.32 — CTE/set-operation/data-modifying subqueries have no executable plan | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.33 — Subquery rewrite preserves lazy demand, snapshot/CommandId, 3VL, duplicates, cardinality, and errors | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.34 — Exact-empty scalar/EXISTS/NOT EXISTS rewrites use their specified values | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.35 — Exact-empty IN/NOT IN still preserves required once-only left evaluation | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.36 — Estimate/statistics/required_rows cannot authorize subquery semantic rewrite | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.37 — Vector batch size cannot alter subquery error precedence | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-14.38 — Parser/AST capacity cannot add a supported semantic form | §§20.14.1–20.14.13; §§19.18, 37.17 | V20-14 | V20-14 procedure and applicable mandatory-matrix row | subquery occurrence/state graph and derived-boundary model |
| V20-15.1 — Demand is defined for executable scalar occurrences | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.2 — Demand is indexed by logical row/pair/group/other relational occurrence | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.3 — Demand is derived from unrevised logical relational semantics | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.4 — Demand is independent of optimizer scheduling | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.5 — Demand is independent of physical visitation order | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.6 — Demand is independent of CPU evaluation count and vector lanes | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.7 — Filter predicate demand covers every required retention decision | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.8 — Ordinary Project demand covers every declared expression/produced row | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.9 — JOIN ON demand covers semantically required candidate pairs | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.10 — Group-key demand covers aggregate input rows | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.11 — Aggregate-argument demand covers applicable rows and occurrences | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.12 — HAVING demand covers produced groups whose retention is determined | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.13 — ORDER-key demand covers rows to which Sort applies | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.14 — Scalar-subquery demand follows surrounding Chapter-17 control flow | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.15 — EXISTS projection demand follows relevance, including required DISTINCT classes | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.16 — IN left demand precedes complete build demand | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.17 — CASE unselected branch is undemanded | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.18 — AND/OR skipped right operand is undemanded | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.19 — Demand observables include value and NULL/UNKNOWN behavior | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.20 — Demand observables include success/error category and origin | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.21 — Demand observables include Chapter-17 child order/short circuit | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.22 — Demand observables include frozen error precedence | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-15.23 — Resource-use differences are excluded from logical demand observables | §§20.6–20.12, 20.14, 20.17 | V20-15 | V20-15 procedure and applicable mandatory-matrix row | unrevised canonical-tree demand relation |
| V20-16.1 — Every rewrite preserves all correctness-observable demanded evaluations or supplies exact proof | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.2 — Demand-changing proof covers the complete newly affected domain | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.3 — Proof establishes totality | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.4 — Proof establishes error freedom | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.5 — Proof establishes determinism for the same bound values | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.6 — Proof establishes insensitivity to changed evaluation count/order | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.7 — Absent proof potentially erroring expressions are treated conservatively | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.8 — Existing error category is preserved | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.9 — Existing diagnostic origin is preserved | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.10 — Existing frozen precedence is preserved | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.11 — Statistics cannot authorize a correctness-sensitive rewrite | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.12 — Samples cannot authorize a correctness-sensitive rewrite | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.13 — Selectivity/cardinality estimates cannot authorize a correctness-sensitive rewrite | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.14 — Costs and heuristics cannot authorize a correctness-sensitive rewrite | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.15 — Constant folding delegates exact semantics to §17.10.2 | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.16 — Boolean simplification preserves 3VL and demand | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.17 — Potentially erroring Boolean work is not hidden or newly forced | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.18 — Predicate pushdown checks slot and join semantics | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.19 — Side-local reference is necessary but insufficient for pushdown | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.20 — Pushdown enlarging demand requires exact total/error-free proof | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.21 — LEFT JOIN pushdown retains outer-join null-preservation requirements | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.22 — Projection pruning retains every ancestor-required value and hidden DML slot | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.23 — Ancestor nonuse alone cannot prune error-capable ordinary projection | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.24 — Exactly total/error-free ordinary projection may be pruned when all else is unchanged | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.25 — EXISTS projection irrelevance permits its specialized pruning | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.26 — Join graph/equality metadata does not alter executable semantics | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.27 — Constant propagation preserves SQL NULL and outer-join boundaries | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.28 — Contradiction detection uses exact typed semantics and trusted constraints | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.29 — Exact-empty proof alone cannot suppress demanded potentially erroring work | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-16.30 — Empty replacement preserves output schema and statement/operator semantics | §§20.17, 20.17.1–20.17.4, 20.17.6–20.17.10; §§34.1, 35.2 | V20-16 | V20-16 procedure and applicable mandatory-matrix row | pre/post demand relation plus closed exact-proof calculus |
| V20-17.1 — Executable binary expressions preserve semantic left child | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.2 — Executable binary expressions preserve semantic right child | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.3 — Executable arithmetic operands are not commutatively swapped | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.4 — Executable comparison operands are not commutatively swapped | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.5 — Executable comparison reversal is prohibited | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.6 — Executable AND child order is preserved | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.7 — Executable OR child order is preserved | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.8 — Executable CASE condition/branch order is preserved | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.9 — Executable associative reassociation is prohibited | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.10 — Executable children are not sorted for canonicalization | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.11 — Value commutativity does not imply evaluation commutativity | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.12 — Short-circuit demand remains Chapter-17 exact | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.13 — Non-executable equality facts may be canonicalized | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.14 — Non-executable lookup/search keys may be canonicalized | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.15 — Non-executable proof/comparison descriptors may be canonicalized | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.16 — Non-executable metadata cannot replace the executable expression | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.17 — Non-executable metadata cannot define a user error or SourceSpan | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-17.18 — Chapter-17 authorized constant folding remains permitted | §§20.17, 20.17.5; §§17.7.3, 17.10.2 | V20-17 | V20-17 procedure and applicable mandatory-matrix row | ordered executable-tree serializer and metadata/nonmetadata partition |
| V20-18.1 — Every source-derived executable occurrence retains canonical SourceSpan | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.2 — Every source-derived executable occurrence retains explicit/implicit cast provenance | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.3 — Every source-derived executable occurrence retains specific error-origin metadata | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.4 — Every source-derived executable occurrence retains precedence identity | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.5 — Moving an unchanged occurrence preserves diagnostic origin exactly | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.6 — Movement creates no new diagnostic origin | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.7 — Structural sharing preserves the same semantic origin | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.8 — Provenance equality is independent of pointer sharing | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.9 — Every legal duplicate copies the canonical origin | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.10 — Copied provenance does not itself authorize duplication | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.11 — V20-15 independently governs duplication legality | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.12 — Error-capable replacement preserves observable error category | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.13 — Error-capable replacement preserves canonical diagnostic origin | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.14 — Error-capable replacement preserves frozen precedence | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.15 — Replacement choosing an arbitrary equivalent expression span is illegal | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.16 — Spanless synthetic executable requires exact total/error-free proof | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.17 — Spanless synthetic proof covers the complete demand domain | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.18 — Proven safe constants/NULL markers/direct slot identities may be spanless | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.19 — No comparison/cast/arithmetic family is presumed safe without proof | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.20 — Error-capable synthetic requires one identifiable source occurrence | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.21 — Error-capable synthetic inherits that occurrence origin and precedence | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.22 — Source-less error-capable synthetic execution is illegal | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.23 — Non-executable metadata requires no diagnostic SourceSpan | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.24 — Non-executable metadata cannot raise a user-visible scalar error | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.25 — Provenance container representation is not prescribed | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.26 — A provenance collection is not a diagnostic-selection rule | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.27 — Constant-fold provenance remains §17.10.2-owned | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-18.28 — Numeric SourceSpan may outlive source bytes without permanent buffer retention | §20.17; §§17.10.2, 18.8, 18.14 | V20-18 | V20-18 procedure and applicable mandatory-matrix row | source-occurrence diagnostic-origin map |
| V20-19.1 — Logical properties include output slots and lineage | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.2 — Logical properties distinguish logical nullability | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.3 — Logical properties may carry candidate keys and known constants | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.4 — Logical properties distinguish semantic ordering | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.5 — Estimated cardinality remains a placeholder/estimate | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.6 — Semantic is_provably_empty carries exact proof provenance | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.7 — Estimated zero never implies semantic emptiness | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.8 — Only enforced trusted constraints establish key proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.9 — Low estimated NDV never proves uniqueness | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.10 — LogicalValues is provably empty exactly for zero listed rows | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.11 — LogicalGet emptiness requires complete exact predicate proof rather than statistics | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.12 — LogicalFilter propagates child emptiness or exact no-TRUE predicate proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.13 — LogicalProject preserves child emptiness proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.14 — INNER/CROSS propagate empty required child proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.15 — INNER may use exact no-TRUE ON proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.16 — LEFT is empty exactly when its preserved left child is empty | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.17 — Empty LEFT right or no-TRUE ON proves no matches rather than empty output | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.18 — Grouped Aggregate preserves empty-child proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.19 — Global Aggregate is not empty merely because child is empty | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.20 — Distinct and Sort preserve empty-child proof | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.21 — LogicalLimit exact SQL LIMIT zero proves empty | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.22 — DML empty input proves zero input/targets but preserves completion semantics | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.23 — LogicalAnalyze is not eliminated from a zero statistic | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.24 — Predicate TRUE-only row-rejecting contexts interpret exact FALSE/UNKNOWN correctly | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.25 — Empty replacement requires approved provenance and preserves schema/demand | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-19.26 — Logical properties do not substitute for physical property contracts | §§20.16, 20.17.9–20.17.10; §§34.1, 35.2, 35.20 | V20-19 | V20-19 procedure and applicable mandatory-matrix row | closed exact-proof calculus disjoint from estimate model |
| V20-20.1 — Validator checks every referenced slot exists in required child output | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.2 — Validator checks whole-statement slot uniqueness | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.3 — Validator checks eliminated slot IDs are not reused | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.4 — Validator checks Get/Values/Project/Aggregate fresh-slot rules | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.5 — Validator checks schema-preserving pass-through slot rules | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.6 — Validator checks derived remapping | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.7 — Validator checks resolved expression return types | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.8 — Validator checks Filter/HAVING/Join predicates are BOOLEAN | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.9 — Validator checks aggregate reference legality | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.10 — Validator checks every aggregate entry has unchanged ordinal and distinct slot | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.11 — Validator checks node output schema matches expressions | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.12 — Validator checks join schema left then right and child slots preserved | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.13 — Validator checks LEFT right logical nullability | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.14 — Validator checks LogicalLimit schema/slot preservation | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.15 — Validator checks required hidden DML slots | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.16 — Validator checks descriptor/SchemaVer consistency | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.17 — Validator checks exact-empty annotation/replacement provenance | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.18 — Validator checks supported subquery mode/child/arity/type/nullability/mapping | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.19 — Validator checks every rewrite has demand preservation or exact proof | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.20 — Validator checks executable provenance and non-executable diagnostic exclusion | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-20.21 — Validator checks executable child order | §20.18 | V20-20 | V20-20 procedure and applicable mandatory-matrix row | independent structural plan predicate |
| V20-21.1 — Canonical logical operator inventory is represented without adding physical algorithms | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.2 — Hidden target RID/system slots are marked internal | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.3 — Hidden system slots are excluded from wildcard/user-visible enumeration | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.4 — Projection pruning retains required hidden slots | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.5 — LogicalInsert carries target TableId and canonical target ColumnIds | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.6 — LogicalInsert accepts Values and SELECT input under one logical contract | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.7 — LogicalInsert carries coercion/default expressions and optional RETURNING | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.8 — LogicalUpdate carries target TableId and BindingId | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.9 — LogicalUpdate child supplies exact target RID | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.10 — LogicalUpdate child supplies required old values | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.11 — LogicalUpdate carries assignments keyed by ColumnId and optional RETURNING | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.12 — LogicalDelete child supplies exact target RID and required old values | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.13 — LogicalDelete targets one base table | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.14 — LogicalAnalyze carries resolved TableId and immutable descriptor | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.15 — LogicalAnalyze carries analyzed SchemaVer and visible ColumnId/IndexId set | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.16 — LogicalAnalyze has no ordinary relational output requirement | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.17 — LogicalAnalyze performs no execution-time name lookup | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-21.18 — LogicalAnalyze does not choose statistics algorithms or bypass visibility | §§20.3, 20.13 | V20-21 | V20-21 procedure and applicable mandatory-matrix row | statement-node schema and hidden-value contract table |
| V20-22.1 — LogicalGet does not choose sequential versus index access | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.2 — LogicalJoin does not select a physical join algorithm | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.3 — LogicalSort does not prescribe a sorting algorithm | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.4 — LogicalLimit semantics do not prescribe an implementation algorithm | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.5 — Logical properties remain distinct from Chapter-37 physical properties and search objectives | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.6 — Subquery fallback semantics exist independently of physical rewrite success | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.7 — Physical subquery planning remains §37.17-owned | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.8 — Memory/expression-state/pipeline mechanics remain Chapters 24–26-owned | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.9 — Multiple conforming physical plans preserve one logical bag/order class | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.10 — Physical vector width cannot alter logical values/errors | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.11 — Physical aggregate sharing retains exact logical mapping | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.12 — Logical EXPLAIN describes the validated logical representation | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.13 — Logical EXPLAIN is independent of reparsing/AST pretty-printing | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.14 — Logical EXPLAIN may show semantic IDs/types/nullability/lineage/proofs | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.15 — EXPLAIN presentation remains §§40.2/40.5/40.8-owned | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-22.16 — No exact output string becomes a semantics oracle | §§20.1, 20.4–20.14, 20.16, 20.19; §§22.4, 27.9, 30.1, 30.3, 37.17, 40.2, 40.5, 40.8 | V20-22 | V20-22 procedure and applicable mandatory-matrix row | cross-realization semantic equivalence and owner ledger |
| V20-23.1 — Hash seed cannot change logical schema | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.2 — Container iteration cannot change slot equality or bag semantics | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.3 — Pointer addresses cannot define logical identity | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.4 — Allocation order cannot define logical identity | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.5 — Opaque LogicalSlotId numeric values are alpha-renamable | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.6 — TableId numeric order cannot reorder output schema | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.7 — ColumnId numeric order cannot reorder output schema | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.8 — Physical scan order cannot create semantic order | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.9 — Physical join algorithm cannot change occurrence multiplicity | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.10 — Physical sort algorithm cannot change comparator order | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.11 — Physical aggregate reduction cannot change logical occurrence mapping | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.12 — Physical Limit choice cannot select outside the allowed result class | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.13 — Optimizer rule visitation cannot change semantic result/error | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.14 — Scheduler interleaving cannot change semantic result/provenance | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.15 — Vector/chunk boundaries cannot change subquery or scalar error precedence | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.16 — Source-buffer release cannot invalidate retained numeric SourceSpan | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.17 — Unspecified tie orders compare by tie-equivalence class | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-23.18 — Unordered results compare by row-occurrence bag rather than sequence | §§20.1–20.2, 20.8, 20.11–20.12, 20.17, 20.20 | V20-23 | V20-23 procedure and applicable mandatory-matrix row | alpha-renamed canonical semantic serialization |
| V20-24.1 — Filter scalar errors retain their Chapter-17/39 category and predicate origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.2 — Project scalar errors retain their category and expression origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.3 — Join predicate errors retain pair demand and source origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.4 — Group-key errors retain row demand and source origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.5 — Aggregate argument errors precede that row's aggregate update | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.6 — Aggregate finalization chooses lowest canonical failing ordinal | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.7 — HAVING errors retain group demand and source origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.8 — ORDER-key errors retain sorted-row demand and source origin | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.9 — Scalar-subquery cardinality maps to SQL CardinalityError | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.10 — Scalar row-construction errors obey first/second-row precedence | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.11 — EXISTS suppresses undemanded projection/later-row errors | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.12 — IN build errors precede possible probe results | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.13 — Unsafe pushdown/pruning/empty rewrite is rejected rather than changing error | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.14 — Error-capable replacement retains category/origin/precedence | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.15 — Invalid source-less error-capable synthetic is rejected by plan validation | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.16 — Logical validator failure is an internal architecture error | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-24.17 — Logical planning does not independently choose transaction ACTIVE/MUST_ABORT effect | §§20.6–20.14, 20.17–20.18; §§39.1–39.3 | V20-24 | V20-24 procedure and applicable mandatory-matrix row | demand/origin/precedence trace plus existing error-category table |
| V20-25.1 — Chapter-16 identity/descriptor/SchemaVer/presentation/snapshot semantics are consumed unchanged | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.2 — Chapter-17 type/3VL/comparison/order/evaluation/folding semantics are consumed unchanged | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.3 — Chapter-18 SourceSpan/source-lifetime semantics are consumed unchanged | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.4 — Chapter-19 BindingId/output/ordinal/ORDER/LIMIT semantics are consumed unchanged | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.5 — Chapter-29 aggregate value/error/ordinal semantics are consumed unchanged | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.6 — Physical operator vocabulary does not redefine logical semantics | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.7 — PhysicalLimit conforms to Chapter-20 selection semantics | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.8 — Sort tie/comparator owners conform to Chapter-20 ordering requirement | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.9 — Statistics remain approximate and cannot originate proof | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.10 — §35.2 exact-proof whitelist remains closed | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.11 — §35.20 Limit estimate matches logical formula | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.12 — §37.17 subquery physical planning preserves fallback semantics | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.13 — §§39.1–39.3 preserve error categories and transaction ownership | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.14 — §§40.2/40.5/40.8 retain EXPLAIN presentation ownership | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.15 — §41 verification obligations are covered by deterministic V20 families | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.16 — Chapter-21 DML handoff is observed without redefining Chapter-21 semantics | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-25.17 — All mandatory Chapter-20 matrices are present and status-complete | Architecture Chapters 16–20, 29; §§22.4, 27.9, 30.1, 30.3, 34.1, 35.2, 35.20, 37.17, 39.1–39.3, 40.2, 40.5, 40.8, 41 | V20-25 | V20-25 procedure and applicable mandatory-matrix row | cross-chapter owner ledger and mandatory matrices |
| V20-26.12 — Verification procedures are deterministic | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.13 — Correctness procedures use no sleeps | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.14 — Every semantic family has an independent oracle | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.15 — Production logical planner does not verify itself | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.16 — Production optimizer does not verify itself | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.17 — Production validator does not verify itself | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.18 — Production EXPLAIN does not verify semantics | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.19 — Reference DBMS behavior is not an oracle | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.20 — Opaque identities are compared by equality graph and alpha-renaming | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.21 — Unordered results are not compared as byte-for-byte sequences | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.23 — Logical representation freedom is preserved | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.24 — Optimizer search/memo freedom is preserved | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.25 — Physical execution freedom is preserved | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-26.26 — Source-buffer ownership freedom is preserved | Architecture front matter; Chapter 20; §41 | V20-26 | V20-26 procedure and applicable mandatory-matrix row | independent oracle registry, V20-23 perturbations, and V20-26 methodology |
| V20-1.17 — Bound join expressions map resolved relation occurrences to LogicalSlotIds without rebinding | §§20.2, 20.8.2; Chapter 19 | V20-1 | V20-1 bound-predicate handoff fixture | bound-to-logical oracle and lineage map |
| V20-3.33 — Every other explicitly schema-preserving node passes slots through unchanged | §20.2 | V20-3 | V20-3 node-rule table | whole-statement occurrence graph with retired-ID set |
| V20-3.34 — Boundary elimination preserves every surviving external identity through explicit mapping | §§20.2, 20.17 | V20-3 | V20-3 boundary-rewrite fixture | whole-statement occurrence graph with retired-ID set |
| V20-8.27 — Every grouping/DISTINCT hash or comparison realization agrees with grouping equivalence | §§20.9–20.10 | V20-8 | V20-8 cross-realization matrix | normalized grouping-equivalence partition |
| V20-9.26 — Expressions above Aggregate reference aggregate outputs rather than arbitrary pre-aggregate columns | §20.9 | V20-9 | V20-9 schema/reference fixture | source-occurrence/ordinal table plus schema oracle |
| V20-10.19 — Physical sorting and ordering-property matching consume stored resolved NULL order rather than infer it | §20.11; §§30.1, 30.3 | V20-10 | V20-10 logical/physical handoff fixture | canonical SQL comparator plus key descriptor |
| V20-11.28 — The folded/residual count expression is acquired once at execution start rather than reevaluated from source | §§19.14, 20.12 | V20-11 | V20-11 ownership-boundary fixture | Chapter-19 count-expression record plus Limit oracle |
| V20-11.29 — LogicalLimit introduces no count-expression category, error timing, or reevaluation | §20.12; §19.14 | V20-11 | V20-11 error-boundary fixture | Chapter-19 count-error oracle |
| V20-14.39 — Duplicate derived exported names fail at the derived boundary | §20.14.3; §19.5 | V20-14 | V20-14 derived-name fixture | bound exported-name table |
| V20-14.40 — A generated presentation name must be explicitly aliased before derived export | §20.14.3; §19.5 | V20-14 | V20-14 derived-name fixture | bound exported-name table |
| V20-14.41 — Qualified derived wildcard follows child projection order | §20.14.3 | V20-14 | V20-14 derived-wildcard fixture | ordered exported-schema oracle |
| V20-14.42 — Supported expression subqueries retain the closed legal-context table | §20.14.9 | V20-14 | V20-14 context matrix | closed context-admission table |
| V20-14.43 — LIMIT/OFFSET, defaults, generated/catalog expressions, and unsupported constraints admit no subquery side plan | §20.14.9 | V20-14 | V20-14 negative context matrix | closed context-admission table |
| V20-14.44 — Only SELECT query blocks appear as subqueries | §20.14.9 | V20-14 | V20-14 unsupported-form matrix | closed support table |
| V20-14.45 — DML subquery errors retain §39.1 before/after-write transaction consequences | §§20.14.9, 39.1 | V20-14 | V20-14/V20-24 DML error fixture | statement-write-boundary oracle |
| V20-14.46 — IN occurrence state records match, build-has-null, and build-empty semantics without prescribing storage | §§20.14.6–20.14.7 | V20-14 | V20-13/14 build-state matrix | repeated-equality 3VL fold |
| V20-14.47 — Subquery values, sets, and spill state are query-local and add no persistent format | §20.14.12 | V20-14 | V20-14 lifetime/persistence fixture | persistence owner ledger |
| V20-14.48 — Logical observability distinguishes scalar, EXISTS, IN, lazy state, and surviving derived boundaries | §20.14.12 | V20-14 | V20-14/V20-22 EXPLAIN identity fixture | semantic-mode occurrence graph |
| V20-14.49 — Every forbidden §20.14.13 implementation is rejected by the closed support/demand/cardinality oracle | §20.14.13 | V20-14 | V20-12–14 forbidden-case table | scalar/EXISTS/IN/derived independent oracles |
| V20-16.31 — Rewrites do not duplicate, drop, or reorder any represented VOLATILE occurrence | §20.17.2 | V20-16 | V20-16 volatility-boundary fixture | demand relation and descriptor classification |
| V20-16.32 — STABLE functions are not treated as immutable compile-time constants | §§20.17.1–20.17.2; §17.10.2 | V20-16 | V20-16 folding-boundary fixture | Chapter-17 folding oracle |
| V20-16.33 — Foreign-key rewrites and join elimination remain outside the trusted proof surface | §20.17.9 | V20-16 | V20-16 trusted-proof negative fixture | closed exact-proof calculus |
| V20-16.34 — Only constraints actually enforced by the engine establish logical key properties | §20.17.9 | V20-16 | V20-16 key-proof fixture | trusted-constraint ledger |
| V20-18.29 — No rewrite invents a source-byte position or optimizer-generated user diagnostic site | §20.17 | V20-18 | V20-18 origin-negative fixture | source-occurrence diagnostic-origin map |
| V20-19.27 — Exact-proof provenance is planner/query-lifetime metadata and adds no persistent format | §§20.16, 35.2 | V20-19 | V20-19 lifetime fixture | proof-provenance owner ledger |
| V20-19.28 — Estimates may affect cost and physical choice but never logical equivalence | §§20.16, 34.1, 35.2 | V20-19 | V20-19 estimate-perturbation fixture | exact-proof/estimate partition |
| V20-20.22 — Validation runs after canonical construction and after every major rewrite phase | §20.18 | V20-20 | V20-20 phase-boundary injection | independent structural plan predicate |
| V20-22.17 — Every supported subquery physical realization conforms to the logical fallback's value, demand, state, and error contract | §§20.14.7, 37.17 | V20-22 | V20-22 cross-realization fixture | canonical subquery fallback oracle |
| V20-24.18 — Chapter-20 errors compose with §39.1 transaction consequences without redefining them | §§20.14.9, 20.17, 39.1–39.3 | V20-24 | V20-24 before/after-write composition fixture | existing statement-write-boundary oracle |
| V20-25.18 — Chapter-20 semantics introduce no page, tuple, catalog, WAL, or recovery-format change | Chapter 20; Chapters 3–16 | V20-25 | V20-25 persistence-owner audit | cross-chapter owner ledger |
| V20-27.1 — Every Chapter-20 atomic obligation has one ledger row | Chapter 20; §41.4 | V20-27 | V20-27 procedure and applicable mandatory-matrix row | owner-to-procedure enumeration and oracle-presence check |
| V20-27.2 — Every ledger row names a deterministic procedure or complete reusable family | Chapter 20; §41.4 | V20-27 | V20-27 procedure and applicable mandatory-matrix row | owner-to-procedure enumeration and oracle-presence check |
| V20-27.3 — Every ledger row names an independent oracle | Chapter 20; §41.4 | V20-27 | V20-27 procedure and applicable mandatory-matrix row | owner-to-procedure enumeration and oracle-presence check |


---

## Chapter 21 DDL/DML Semantic-Planning Verification

This section is the complete procedural owner for the operation, publication, and SQL
contract in [`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 21. It consumes the complete
Chapter-15 DML, Chapter-16 catalog, Chapter-17 scalar/default, Chapter-18 parser,
Chapter-19 binder, and Chapter-20 logical-semantics verification families. Chapters 5,
7–14, and 31 and §§39.1–39.3 remain authoritative for tuple bytes, files, indexes,
transactions, visibility, locks, WAL, recovery, reclamation, physical execution, and
transaction consequences. A reference to one of those complete families is a reuse of its
deterministic procedure, not a duplication of its architecture.

Every fixture starts from fully bound input. Production parsing, binding, planning,
execution, catalog caching, and recovery never serve as their own expected-result oracle.
No correctness case uses sleeps; controlled barriers, exact event release, explicit
persisted prefixes, and independent finite models determine every outcome.


### Deterministic Chapter-21 harness

The conceptual harness controls and records:

```text
catalog:
    canonical table/index name classes and same-transaction DROP reservations
    TableId, ColumnId, IndexId, ConstraintId, FileId, SchemaVer
    immutable table/index/constraint descriptors and their ownership links
    transaction-local and globally committed catalog versions

transaction:
    TxnId, isolation level, CommandId, admitted statement, attempt identity
    attempt snapshot, first-published-write flag, locks/gates, runtime state
    C0-C6/A0-A4 boundaries and explicit/autocommit result envelope

MVCC:
    current logical owners, historical/superseded versions, deleted versions
    own earlier-command and current-command versions, other-transaction versions

DML:
    bound target mappings, source occurrence bag/order, default actions/coercions
    finalized target RIDs, duplicate discoveries, stale transitions
    old/candidate/final row images, constraint claims, affected rows, RETURNING bag

DDL:
    bound object identities, SchemaLock/TableWriterGate/MANIFEST_CHANGE ownership
    private/final/catalog-committed/retired file state, current-owner build set
    catalog rows, descriptor/cache publication, namespace reservation

diagnostic:
    attempt membership, finalized occurrence, category, responsible SourceSpan
    SourceSpan specificity, row-semantic phase, ordinary-versus-dynamic class

observable:
    statement/transaction result, public error and origin, catalog visibility
    committed row/index state, affected-row count, RETURNING occurrence bag
    SchemaVer/cache state, file lifecycle, durable/recovered state

perturbation:
    RID/page/source/spool/index visitation, vector and batch shape, hash seed
    worker schedule, allocator/pointer identity, and legal physical build algorithm
```

The harness exposes barriers immediately before and after statement admission, attempt
start, snapshot registration, target finalization, every logical-lock/gate wait, every
ordinary-error candidate becoming established, every transaction-owned publication,
catalog/file namespace transition, terminal publication, cache coherence action, and
recovery READY. Perturbations may change an unordered RETURNING sequence, but never its
bag, committed state, selected ordinary error, or transaction consequence.

### Independent oracle registry

- The **statement oracle** assigns one CommandId to each admitted statement, nests attempts
  beneath it, and derives retry eligibility, attempt reset, lock lifetime, and terminal
  state from §§9.6, 15.7, and 39.1.
- The **publication oracle** marks the first transaction-owned WAL-backed database
  mutation and independently classifies prepublication failure, mandatory abort, and
  noncontinuable outcomes using the existing Chapter-12/15/39 event model.
- The **result-envelope oracle** separates attempt output, statement success, explicit
  transaction response, implicit COMMIT, and client publication.
- The **row-construction oracle** applies resolved INSERT mappings or simultaneous UPDATE
  assignments to immutable input images using test-side scalar/coercion models.
- The **target oracle** forms a set keyed by exact target RID, applies finalized-attempt
  qualification and stale-owner rules, and never consults production spool order.
- The **constraint oracle** checks descriptor NOT NULL predicates, canonical key encoding,
  NULL participation, current-owner uniqueness, and exact UPDATE old-RID self-exclusion.
- The **RETURNING oracle** builds operation-specific row occurrences and compares
  unordered multisets by exact typed value and multiplicity.
- The **ordinary-error oracle** constructs candidates only from finalized attempt
  occurrences and sorts semantic keys `(start, span_length, phase, category, origin)`;
  complete ties compare as one observational class.
- The **current-owner build oracle** derives the CREATE INDEX row set from transaction-
  local version histories at the command boundary rather than a SQL snapshot or heap scan.
- The **index-completeness oracle** compares the required `(encoded_key,RID)` multiset of
  the build oracle with the published index-entry multiset.
- The **catalog oracle** evaluates catalog MVCC, immutable descriptors, stable IDs,
  SchemaVer, constraint/index links, name classes, and DROP reservations independently.
- The **file-lifecycle oracle** is a finite state model over private, final durable
  uncommitted, catalog committed, retiring, and retired-linked resources.
- The **durability oracle** constructs valid WAL/file/directory prefixes and derives
  commit eligibility and reopen state without invoking production recovery as oracle.
- The **recovery oracle** applies the architecture-owned redo/loser/file-classification
  transition to the selected durable prefix and repeats it for second-crash cases.
- The **surface oracle** compares the admitted Chapter-21 statement/subquery/default
  registry with literal closed sets and verifies that excluded forms fail explicitly.
- The **documentation oracle** checks ownership references, timeless procedural prose,
  absence of implementation mandates, and one ledger row per atomic obligation.

### V21-1 — Front-end and bound-statement handoff

Use one valid and one failing request at each Chapter-18/19 boundary. A parse failure must
produce no Chapter-21 operation; a bind failure must produce no durable allocation, file,
gate, or operation. For valid DDL/DML/ANALYZE, compare the consumed IDs, types, coercions,
SourceSpans, descriptors, defaults, and logical child against the immutable bound fixture.
Mutate source names after binding and prove Chapter 21 neither re-resolves nor rebinds them.

### V21-2 — Statement attempts, CommandId, and retry

Reuse the Chapter-15 statement ledger while adding Chapter-21 row construction, target
sets, error candidates, counts, and RETURNING buffers to attempt-local state. Force RC
pre-write retry and prove a fresh attempt/snapshot with the same CommandId and no inherited
provisional semantics. Force the same conflict after publication and RR conflict at either
point; verify no forbidden retry and the exact §39.1 outcome.

### V21-3 — Atomicity, locks, and result envelopes

Inject each ordinary, dynamic, DML, and DDL failure immediately before and after the first
published write. Compare explicit and autocommit outcomes with the publication and result-
envelope oracles. Locks and gates remain transaction-owned through their terminal release
stage. No failed multirow statement yields partial committed state, count, or RETURNING.

### V21-4 — INSERT row construction and defaults

Construct candidate rows from bound target order. Cover explicit reorder, omitted nullable
without default, omitted persisted default, omitted nonnullable without default, selected
assignment coercions, multiple rows, VALUES, and SELECT. Decode DefaultValueBlob through
V21-27, but use its independently decoded typed scalar as the INSERT input; never bind or
reevaluate an expression per row.

### V21-5 — INSERT uniqueness and physical handoff

Combine the candidate-row oracle with complete Chapter-11 uniqueness and Chapter-15 INSERT
procedures. Cover committed, own earlier-command, own current-command prior-row, active
other-owner, aborted, NULL, NaN, signed-zero, and composite keys. Then compare the fresh
tuple version and required index-entry set with the reusable header/WAL/index oracles.

### V21-6 — INSERT self-reference and result

Fix the source relation under the statement snapshot/CommandId and vary physical
materialization. Newly inserted current-command rows cannot recursively extend that source.
Count one per successful logical input occurrence and build one final-new RETURNING
occurrence per input in the final successful attempt; compare as an unordered bag.

### V21-7 — UPDATE targets, deduplication, stale handling, and Halloween safety

Produce duplicate discoveries and predicate/index changes for exact old RIDs. The target
oracle must finalize each RID once before mutation. Exercise immediate grant, wait/grant,
stale owner, changed predicate, RC clean retry, post-write conflict, and RR conflict. A
replacement version or changed access key must never be rediscovered as another target.

### V21-8 — UPDATE simultaneous row construction

Evaluate every RHS against one immutable old row, apply bound coercions, copy unmentioned
columns, and compare the complete candidate row. Mandatory fixtures include `SET a=b,b=a`,
overlapping expressions, NULL, subquery errors, and `SET x=x`. Permute assignment storage
and physical evaluation while preserving Chapter-17/20 within-expression demand order.

### V21-9 — descriptor-wide UPDATE NOT NULL

For each finalized target, place barriers after RHS evaluation, coercion, complete-row
construction, NOT NULL validation, uniqueness admission, and publication. Cover nullable
NULL, non-PK and PK nonnullable NULL, valid copied nonnullable columns, simultaneous swap,
and no-op UPDATE. A violation is `ConstraintViolation`, uses the responsible RHS origin
when applicable, and publishes no target tuple/delete/index effect before validation.

### V21-10 — UPDATE uniqueness, swaps, cycles, and index maintenance

Apply immediate uniqueness after complete-row NOT NULL validation. Cover unchanged and
changed keys, exact old-RID exclusion, another own row, same-command collision, two-row
swap, three-row cycle, NULL, NaN, signed zero, and multiple indexes. Swaps/cycles fail; old
physical entries remain and every successful replacement receives required new entries.

### V21-11 — UPDATE affected rows and RETURNING

Count each distinct finalized target acted upon once, including `SET x=x`; exclude removed
duplicates, stale/nonqualifying targets, and abandoned attempts. Build one final-new row
occurrence per counted target. Permute target and write order and compare count plus
unordered RETURNING bag; no failed prefix escapes.

### V21-12 — DELETE operation semantics

Exercise exact RID targeting, deduplication, post-wait revalidation, RC/RR stale behavior,
one-target-once and index/predicate Halloween fixtures. Compare `xmax/cmax`, absence of a
new version, retained index entries, one affected row per finalized delete, and one
retained-old RETURNING occurrence in an unordered bag.

### V21-13 — deterministic ordinary DML error precedence

Build the candidate set independently from the final attempt. Include expression/subquery,
descriptor NOT NULL, and immediate UNIQUE candidates, plus deduplicated-away, stale,
nonqualifying, and abandoned-attempt noncandidates. Select smallest SourceSpan start, then
shorter span, then expression/subquery before NOT NULL before UNIQUE. Complete semantic
ties must produce the same public diagnostic without a row-identity tie-breaker.

### V21-14 — RETURNING bags and failure suppression

For INSERT, UPDATE, and DELETE generate two or more legal physical sequences. Exact
operation image and multiplicity must match; sequence may differ. Verify ordered source
does not transfer order, duplicate targets do not multiply output, reclamation does not
alter DELETE images, retries discard buffers, failures expose no prefix, and autocommit
withholds the bag through the required COMMIT envelope.

### V21-15 — CREATE TABLE lifecycle

Drive SchemaLock, name revalidation, ID reservation, private heap/FSM/backing-index
creation, durable final-name publication, catalog insertion, transaction-local visibility,
terminal cache publication, and abort/orphan transitions with explicit barriers. Compare
catalog and file state independently at every failure prefix; gaps remain consumed and no
catalog row may name a not-final-durable file.

### V21-16 — CREATE INDEX gates and offline build

Exercise SchemaLock, exclusive target writer gate, writer drain, current object/name
revalidation, private B+ construction, MANIFEST_CHANGE, catalog publication, and terminal
release. Include legal retained exclusive ownership for a transaction-local table and
proactive rejection of same-table shared-to-exclusive upgrade. No page latch may span a
gate wait, and no scan order or build algorithm is an oracle.

### V21-17 — CREATE INDEX current-owner build view

Construct version histories containing committed owners, own earlier INSERTs, own earlier
UPDATE replacements, own earlier deletes, aborted versions, superseded versions, active
other owners, and reclamation history. At the command boundary after writer drain compare
the exact included RID set with the current-owner build oracle. Repeat under an older RR
query snapshot and prove query visibility differs without underfilling the index build.

### V21-18 — CREATE UNIQUE INDEX and publication completeness

Validate canonical uniqueness over exactly the V21-17 build set, including own earlier
rows, NULL-containing keys, NaN, signed zero, composite keys, and duplicates spanning
arbitrary physical partitions. Before catalog visibility compare the required and actual
entry multisets for exact equality; a half-built or underfilled index never publishes.

### V21-19 — DROP TABLE, DROP INDEX, and physical retirement

Model catalog deletion separately from resource retirement. DROP TABLE covers its table,
constraints, and indexes as one operation; ordinary standalone index DROP covers its
index. Verify transaction-local/global MVCC visibility, abort restoration, old descriptor
survival, no-new-fetch and drain barriers, RETIRED_LINKED, durable directory sync,
retryable cleanup failure, crash, and delayed unlink.

### V21-20 — constraint-owned backing-index protection

Resolve one ordinary index and live PRIMARY KEY/UNIQUE backing links. A standalone DROP of
either backing index must return `CatalogError` at the target-name origin before catalog
deletion, descriptor retirement, LIVE-to-RETIRING, or file retirement. State is byte- and
identity-equal afterward. DROP TABLE remains legal as the owning dependency operation.

### V21-21 — dropped-name reservation and recreation

Track catalog visibility and namespace reservation as separate booleans in the catalog
oracle. Same-transaction DROP then same-name CREATE TABLE/INDEX rejects even when the
predecessor is self-invisible. Abort leaves the predecessor and no replacement; committed
DROP releases the name to a later transaction, whose object receives fresh IDs/files and
may coexist with old descriptors and pending predecessor retirement.

### V21-22 — Catalog MVCC, SchemaVer, descriptors, and cache

Run RC and RR binding around uncommitted CREATE, committed CREATE, DROP, rollback, and
same-name recreation. Compare authoritative catalog MVCC with immutable descriptor and
SchemaVer identities. Cache hits may neither expose invisible metadata nor hide visible
transaction-local metadata; postcommit install failure must yield coherent bypass/
invalidation or the frozen noncontinuable result without changing COMMITTED.

### V21-23 — WAL, durability, recovery, and second crash

Reuse Chapter-12/13 prefix construction for DML heap/index, catalog mutation, private-file
initialization, final-name publication, index build, DROP retirement, COMMIT, and ABORT.
Assert no new WAL grammar. Reopen private/final orphans, uncommitted/committed CREATE and
DROP, and pending retirement; then persist the recovered state, crash again, and require
the same committed logical result and reconstructible cleanup ownership.

### V21-24 — Constraint and uniqueness closure

Use table-driven matrices for INSERT/UPDATE NOT NULL, PRIMARY KEY implication, ordinary
and composite UNIQUE, NULL, NaN, signed zero, same-statement INSERT, UPDATE collision,
swap/cycle, concurrent nonterminal owners, aborted creators/deleters, own earlier/current
commands, and exact old-RID exclusion. The independent key/status model determines
conflict, wait/recheck, category, and statement outcome.

### V21-25 — Determinism and implementation freedom

Replay every applicable DML/DDL fixture while perturbing RID/page/source/spool/index order,
batch/vector shape, hash seed, worker schedule, allocation addresses, and legal build
algorithm. Committed/catalog state, affected rows, public errors, transaction state, and
RETURNING bags remain equal. Only sequence presentation of an unordered bag may vary.

### V21-26 — ANALYZE operation boundary

Consume the bound TableId, descriptor, SchemaVer, columns, and visible IndexIds without
rebinding. Verify normal RC/RR value visibility, final object/manifest revalidation,
STATS_PUBLISH/MANIFEST_CHANGE composition, transaction-local descriptor visibility,
terminal global publication, conflict with same-manifest DDL, cancellation/failure before
and after statistics-row publication, and cache fallback through existing statistics tests.

### V21-27 — DefaultValueBlob v1

Construct bytes independently: `DBLUSDEF`, version 1, zero flags/reserved, exact total
length, CRC32C with checksum bytes zeroed, and one PersistedScalarV1 of the target TypeId.
Cover every scalar kind, NULL, 4096-byte boundary, oversize rejection, malformed and
trailing bytes, wrong type, zero/malformed version, greater positive version, and reopen.
Original SQL text and expression trees must not become execution authority.

### V21-28 — SQL surface and exclusions

Compare parser/binder/logical handoffs against the literal Chapter-21 statement and
subquery registries. Every admitted form reaches its canonical owner; every listed
excluded form fails explicitly and cannot be partially reinterpreted through generic AST
or IR capacity. The fixture makes no claim about when excluded features might exist.

### V21-29 — Operation-plan and upper-invariant validation

Inspect semantic plans and operation manifests before physical effects: hidden target RID
survives to DML, required old/new row values are present, bound IDs/types/nullability and
uncorrelated side-plan ownership are preserved, index manifests are complete, and no
physical algorithm or SQL name resolution appears. Inject one broken invariant at a time
and require validation failure before data-changing publication.

### V21-30 — Cross-owner procedure and oracle traceability

Check each cross-owner handoff against its named procedure and independent oracle.
A heading or architectural statement alone does not supply the required methodology.

### Mandatory Chapter-21 matrices

#### Statement-attempt matrix

| Case | CommandId/attempt | Retry | Published write | Result/locks | Transaction oracle |
|---|---|---|---|---|---|
| INSERT success | one ID/final attempt | no | operation dependent | final count/bag; locks retained | ACTIVE or implicit COMMIT |
| INSERT row error | one ID/current attempt | no | before/after matrix | no count/bag | FA before; MA after |
| UPDATE success | one ID/final attempt | no | yes | distinct-target result | ACTIVE or implicit COMMIT |
| RC stale before write | same ID/new attempt | yes | no | old provisional state discarded | ACTIVE |
| RC stale after write | same ID/no new attempt | no | yes | no success; locks through abort | MA/ABORTED |
| RR stale conflict | one ID/no replacement | no | either | no success | frozen serialization outcome |
| UPDATE NOT NULL | one ID | no | target check precedes target write | no target result | §39.1 boundary |
| UPDATE UNIQUE | one ID | no | boundary dependent across rows | no successful prefix | §39.1 boundary |
| DELETE success | one ID/final attempt | no | yes | distinct-target old-image bag | ACTIVE or implicit COMMIT |
| pre-catalog DDL failure | one ID | no | no catalog write | orphans owned; gates released as allowed | FA/ACTIVE or autocommit ABORT |
| post-catalog DDL failure | one ID | no | yes | no DDL success | MA/automatic ABORT |
| autocommit success | one ID | no | yes if persistent | output withheld through C4-C5 | COMMITTED before success |

#### DML row-image matrix

| Operation | Input image | Construction | Constraint point | Physical handoff | RETURNING |
|---|---|---|---|---|---|
| INSERT | bound source occurrence | target map, persisted default/NULL, coercion | NOT NULL then immediate UNIQUE | fresh §15.2 version and indexes | final new row |
| UPDATE | revalidated old target | all RHS from old; coercion; copy unmentioned | descriptor NOT NULL then immediate UNIQUE | §15.3 replacement/old xmax/indexes | complete final new row |
| DELETE | revalidated retained old target | no replacement row | old-key/current-owner admission | §15.4 xmax/cmax; retain indexes | retained old row |

#### DML multiplicity matrix

| Case | Logical actions | Physical effects | Affected rows | RETURNING occurrences |
|---|---:|---|---:|---:|
| INSERT repeated equal nonunique values | one per input occurrence | one version/index set each | input count | input count |
| INSERT same unique key | statement fails | possible aborted prefix | no successful count | none |
| UPDATE duplicate target discovery | one per distinct finalized RID | one replacement protocol | one | one |
| UPDATE stale/nonqualifying target | zero unless final attempt acts | none for skipped target | zero | zero |
| UPDATE `SET x=x` | one | baseline UPDATE protocol | one | one |
| DELETE duplicate target | one per distinct finalized RID | one xmax/cmax | one | one |
| DELETE stale target | zero unless final attempt acts | none for skipped target | zero | zero |

#### DML error-precedence matrix

| Case | Candidate? | Semantic key/result | Physical order | Consequence owner |
|---|---|---|---|---|
| two expression errors | yes if both finalized | smallest start, then span, expression phase | irrelevant | §39.1 after selection |
| expression vs NOT NULL, identical span | yes | expression wins | irrelevant | §39.1 |
| NOT NULL vs UNIQUE, identical span | yes | NOT NULL wins | irrelevant | §39.1 |
| two UNIQUE errors | yes | span then equivalence rules | irrelevant | §39.1 |
| equal start/different length | yes | shorter span wins | irrelevant | §39.1 |
| complete semantic tie | yes | observationally equivalent diagnostic | no row tie-break | §39.1 |
| deduplicated target occurrence | no | absent | irrelevant | none |
| stale/nonfinal occurrence | no | absent | irrelevant | concurrency owner |
| abandoned retry attempt | no | discarded | irrelevant | retry owner |
| deadlock/cancellation/resource/I/O | no ordinary candidate | independent dynamic failure | occurrence-time behavior | Chapters 11/15/39 |

#### RETURNING matrix

| Case | Image | Multiplicity | Order | Publication |
|---|---|---|---|---|
| INSERT ordered source | final new | one/input | none inherited | successful envelope only |
| INSERT unordered source | final new | one/input | unordered | successful envelope only |
| UPDATE | final new | one/distinct acted target | unordered | successful envelope only |
| UPDATE duplicate discovery | final new | one, not duplicate | unordered | successful envelope only |
| DELETE | retained old | one/distinct deleted target | unordered | successful envelope only |
| stale/skipped target | none | zero | N/A | none |
| abandoned retry | discarded | zero exposed | N/A | replacement attempt only |
| failed statement | discarded | zero exposed | N/A | error only |
| autocommit commit failure | buffered then discarded/not-success | zero successful response | N/A | no success |

#### CREATE INDEX build-set matrix

| Version/owner class | Included? | Index entry required? | Reason |
|---|---:|---:|---|
| committed live current owner | yes | yes | belongs to current target state |
| own earlier INSERT | yes | yes | can commit with DDL transaction |
| own earlier UPDATE replacement | yes | yes | current replacement owner |
| own earlier UPDATE predecessor | no | no | superseded historical version |
| own earlier DELETE target | no | no | not live in transaction-local state |
| aborted creator | no | no | cannot belong to commit state |
| superseded committed history | no | no | not current logical owner |
| nonterminal other transaction | no | no | cannot be admitted as committing owner |
| RR-visible historical predecessor | no unless current owner | no | SQL visibility is not build authority |
| current owner omitted by old RR query snapshot | yes | yes | special current-owner build view |
| reclamation-only retained version | no | no | physical history is nonsemantic |

#### DDL object-lifecycle matrix

| Case | Catalog | Namespace/file | Cache/descriptor | WAL/recovery result |
|---|---|---|---|---|
| CREATE TABLE success | transaction-local then committed | private -> final -> catalog committed | publish at terminal commit | committed object recoverable |
| CREATE TABLE pre-catalog failure | absent | private/final orphan; IDs consumed | none | classify/retire orphan |
| CREATE TABLE post-catalog failure | aborted/invisible | final orphan candidate | no global entry | MA/loser recovery |
| CREATE INDEX success | complete index row after build | private -> final | immutable entry at commit | complete index recoverable |
| UNIQUE build failure | no visible index | orphan candidate | none | no half index |
| DROP TABLE | deleted as one dependency scope | delayed retirement | old descriptors survive | committed deletion recoverable |
| ordinary DROP INDEX | deleted | delayed retirement | old descriptors survive | committed deletion recoverable |
| backing-index DROP rejection | unchanged | unchanged | unchanged | CatalogError; no retirement |
| rollback | creator/deleter ineffective | orphan or retained predecessor | no false global publish | loser/ABORT semantics |
| crash after durable commit | committed result | required namespace durable | cache rebuilt/bypassed | same logical result |
| second crash after recovery | same committed result | cleanup ownership persistent | rebuildable | no recovery-only authority |

#### CREATE TABLE failure matrix

| Failure boundary | Catalog result | Physical result | ID result | Transaction/recovery result |
|---|---|---|---|---|
| name revalidation | unchanged | none | none | prepublication failure |
| object/FileId reservation | unchanged | none or no published file | consumed gaps permitted | pre-catalog failure |
| private heap/FSM initialization | unchanged | deterministic private orphan ownership | consumed | pre-catalog failure or lower fatal cause |
| backing-index construction | unchanged | complete survivor set becomes orphan-owned | consumed | no partial object |
| final-name publication/sync | unchanged | private/final prefix classified exactly | consumed | no catalog reference to incomplete namespace |
| before first catalog row | unchanged | final durable uncommitted orphan | consumed | may remain ACTIVE only after ownership transfer |
| after first catalog row | transaction-owned rows later aborted | final files become orphan candidates | consumed | mandatory abort; loser recovery |
| constraint/index catalog publication | all-or-aborted | no visible partial dependency set | consumed | §39.1 boundary result |
| COMMIT prerequisite | no acknowledged commit | required files retained/classified | consumed | canonical COMMIT failure path |
| after durable COMMIT | committed complete object | required files durable | retained | outcome remains COMMITTED |

#### DDL visibility and name matrix

| Observer/state | Object visible? | Name reserved? | CREATE same name? | Identity |
|---|---:|---:|---:|---|
| creator own later command | yes | occupied | no | created identity |
| other transaction before CREATE commit | no | not its usable object | governed by current-owner/name lock | no exposure |
| eligible transaction after CREATE commit | yes | occupied | no | created identity |
| dropper own later command | no | yes by own DROP | no | predecessor retained |
| same transaction after DROP | no | yes | rejected | no replacement |
| DROP rollback | predecessor visible | occupied | no replacement | predecessor identity |
| later transaction after committed DROP | no predecessor | no DROP reservation | yes if otherwise free | fresh IDs/FileIds |
| old RR snapshot after recreation | predecessor by snapshot | new name owner may exist for newer view | snapshot-specific | identities distinct |

#### DROP dependency matrix

| Target | Allowed? | Constraint/catalog state | File retirement | Error |
|---|---:|---|---|---|
| ordinary named standalone index | yes | index deletion under DROP protocol | delayed | none on success |
| PRIMARY KEY backing index | no | unchanged | not begun | CatalogError at target |
| UNIQUE backing index | no | unchanged | not begun | CatalogError at target |
| DROP TABLE owning dependencies | yes | table/constraints/indexes one operation | delayed collectively | none on success |

#### Constraint matrix

| Constraint | Binding owner | Runtime check | Publication boundary | Error/consequence |
|---|---|---|---|---|
| INSERT NOT NULL | Chapter 19 descriptor/action | final candidate value | before row publication | ConstraintViolation; §39.1 |
| UPDATE NOT NULL | Chapter 19 descriptor/RHS | complete replacement, all nonnullable columns | before target tuple/xmax/index publication | ConstraintViolation; §39.1 |
| PRIMARY KEY | Chapters 16/19 | NOT NULL then UNIQUE | before conflicting publication | constraint/unique cause; §39.1 |
| UNIQUE | Chapters 11/16/19 | immediate current-owner check | before conflicting publication | UniqueViolation; §39.1 |
| CHECK | unsupported | none | N/A | explicit unsupported |
| foreign key | unsupported | none | N/A | explicit unsupported |

#### UNIQUE matrix

| Case | Conflict? | Wait/recheck | Result |
|---|---:|---|---|
| non-NULL duplicate current owner | yes | exact-key complete scan | UniqueViolation |
| NULL-containing ordinary UNIQUE | no | no ordinary key claim | allowed unless another constraint |
| composite partial NULL | no | no ordinary conflict | allowed unless another constraint |
| NaN canonical-equal duplicate | key-owner rule | canonical encoding | model result |
| +0/-0 | key-owner rule | canonical encoding | model result |
| same-statement INSERT duplicate | yes | current-command owner participates | statement failure |
| UPDATE another-row collision | yes | all owners except exact old RID | statement failure |
| two-row swap / cycle | yes | immediate per-target semantics | statement failure |
| other uncommitted owner | terminal-dependent | wait then recheck | conflict/free by outcome |
| aborted creator/deleter | status-dependent | heap/status recheck | model result |
| own earlier/current command | current-owner rules | exact command-aware recheck | model result |
| exact old UPDATE RID | self-excluded only when allowed | still scan every other RID | no self-conflict alone |

#### WAL and durability matrix

| Action | Prerequisite | Visibility/publication | Recovery owner | Second crash |
|---|---|---|---|---|
| heap INSERT/UPDATE/delete marker | Chapter-12 authorized WAL | transaction-owned page publication | Chapters 12–15 | logical terminal result retained |
| DML index mutation | heap authority precedes referring entry | B+ MTR publication | Chapters 8/12/13 | replay idempotent |
| catalog tuple mutation | ordinary catalog WAL | crosses first-write boundary | Chapters 12/13/16 | MVCC result retained |
| private file initialization | private-file durability rules | no catalog visibility | §§4.7, 13 | orphan ownership retained |
| final-name publication | rename plus directory sync | still uncommitted without catalog | §§4.7, 13 | namespace state classified |
| CREATE INDEX build | ordinary B+ WAL/MTR plus file rules | private until catalog | Chapters 8/12/13 | complete/orphan classified |
| DROP retirement | catalog commit before delayed unlink | logical DROP independent of cleanup | §§4.7, 7.12.5, 14.17.1 | retirement resumes |
| COMMIT | required files final-durable; commit WAL durable | terminal catalog visibility | Chapters 9/12/13/15 | COMMITTED retained |
| ABORT | canonical terminal protocol | no committed DDL/DML result | Chapters 9/12/13/15 | loser remains ABORTED |

#### Error-category matrix

| Failure | Category/origin | V21-13 candidate? | Statement result | Transaction owner |
|---|---|---:|---|---|
| parse/bind/type/default definition | Chapter 18/19/17 category/span | no | no operation or prewrite failure | §39.1/39.2 |
| INSERT/UPDATE NOT NULL | ConstraintViolation; responsible value origin | yes when finalized row-semantic | no success | §39.1 |
| immediate UNIQUE | UniqueViolation nested cause | yes when finalized row-semantic | no success | §39.1 |
| DROP backing dependency | CatalogError; DROP target name | no multirow candidate | unchanged catalog/file | §39.1 |
| stale RC | TransactionConflict owner | no | retry or failure by boundary | §§15.7, 39.1 |
| stale RR | serialization owner | no | failure | §§11/15/39 |
| deadlock | DeadlockVictim | no | mandatory abort | Chapters 11/39 |
| scalar/subquery DML | scalar/category origin | yes when ordinary/finalized | selected by V21-13 | §39.1 |
| DDL namespace collision/reservation | CatalogError at object name | no | prepublication rejection | §39.1 |
| postcommit cache failure | lower cause plus coherent fallback/NC | no | COMMITTED unchanged | §39.1.5/39.1.8 |
| cancellation/resource/storage | preserved lower cause | no | boundary/dynamic result | §§15/39 |

#### Determinism matrix

| Perturbation | Committed/catalog state | Public ordinary error | RETURNING | Allowed effect |
|---|---|---|---|---|
| RID/page order | equal | equal | equal bag | sequence only if unordered |
| source occurrence order | equal under statement semantics | V21-13, not visit order | equal bag | no hidden order |
| spool/index traversal | equal | equal | equal bag | none beyond sequence |
| vector/batch shape | equal | equal | equal bag | none beyond sequence |
| hash seed | equal | equal | equal bag | none beyond sequence |
| worker schedule | equal | dynamic failures retain owner | equal bag on success | legal timing only |
| allocator/pointer identity | equal | equal | equal bag | none |
| file pathname-generation detail | equal identity contract | equal | N/A | representation only |
| CREATE INDEX scan/build algorithm | complete equal index | equal duplicate result | N/A | implementation freedom |

#### Cross-chapter composition matrix

| Owner | Upstream input | Chapter-21 role | Reused verification | Duplicated? |
|---|---|---|---|---:|
| Chapter 5 | tuple/RID/version bytes | operation image/version handoff | Tuple codec + V15 | no |
| Chapter 7 | guards/files/drain | retirement boundary | Buffer/File retirement tests | no |
| Chapter 8 | key codec/B+ MTR | required index-entry set | B+ Tree Verification | no |
| Chapter 9 | TxnId/CommandId/snapshot | admission and attempt composition | Transaction identity tests | no |
| Chapter 10 | MVCC/current owner | row/catalog/build visibility | MVCC Visibility Tests | no |
| Chapter 11 | locks/gates/uniqueness | operation claims and exclusion | Locking/UNIQUE tests | no |
| Chapters 12–13 | WAL/recovery | DML/DDL durability composition | WAL/Recovery tests | no |
| Chapter 14 | retention/retirement | old entries/descriptors/files | Vacuum/Reclamation tests | no |
| Chapter 15 | physical DML/attempt/envelope | upper operation semantics | DML Execution Tests | no |
| Chapter 16 | catalog/IDs/descriptors/cache | DDL object publication | Catalog Tests | no |
| Chapter 17 | scalar/coercion/default scalar | row construction/default handoff | Type-System tests | no |
| Chapter 18 | parse/framing | successful raw-statement boundary | Chapter-18 tests | no |
| Chapter 19 | binding/SourceSpan | fully bound input | Chapter-19 tests | no |
| Chapter 20 | bags/order/subqueries/provenance | relational child composition | V20 families | no |
| Chapter 21 | operation/publication/SQL scope | direct V21 oracle | V21-1–30 | canonical |
| Chapter 31 | physical targets/results/DDL | downstream conformance | DML/Execution tests | no |
| §39 | categories/transaction consequence | selected error handed off | Statement Failure tests | no |
| §41 | verification architecture | procedure coverage | ledger/matrices | no |

#### High-level case matrix

| Fixture | Architecture owner | Independent oracle | Expected result | Family |
|---|---|---|---|---|
| RC pre-write retry | §§15.7, 21.16.1 | statement/publication | same CommandId, fresh attempt, discarded provisional state | V21-2 |
| post-write UPDATE failure | §§21.13, 39.1 | publication/transaction | no retry; automatic abort | V21-3/9 |
| INSERT duplicate unique | §§21.7, 11.10 | constraint | UniqueViolation; no success | V21-5 |
| INSERT self-reference | §§21.11, 20.14 | source relation | no recursive current-command feed | V21-6 |
| UPDATE duplicate target / stale target | §§21.13, 31.2/31.5 | target | once or skipped/retried per owner | V21-7 |
| `SET a=b,b=a` / `SET nn=NULL` / `SET x=x` | §21.13 | row/constraint | simultaneous; reject NN; no-op acts | V21-8/9 |
| key swap / cycle | §§21.7, 15.3 | constraint | immediate conflict | V21-10 |
| DELETE duplicate / stale target | §§21.14, 31.2/31.5 | target | one action or owner-defined stale result | V21-12 |
| INSERT/UPDATE/DELETE RETURNING reorder | §21.15 | multiset | equal bags; sequence unspecified | V21-14 |
| expression vs NOT NULL vs UNIQUE | §21.16.1 | ordinary-error | span then phase precedence | V21-13 |
| complete error tie / deadlock exclusion | §21.16.1 | error equivalence/dynamic owner | same diagnostic / deadlock owner | V21-13 |
| CREATE TABLE success / abort | §§21.5–21.6 | catalog/file | visible complete object / invisible orphan | V21-15 |
| CREATE TABLE -> INSERT -> CREATE INDEX | §21.8.2 | build/completeness | committed index contains inserted rows | V21-17/18 |
| own UPDATE/DELETE -> CREATE INDEX | §21.8.2 | build | replacement included; deleted row excluded | V21-17 |
| CREATE UNIQUE INDEX own duplicate | §21.8.2 | constraint/build | duplicate detected before publication | V21-18 |
| DROP ordinary / PK / UNIQUE index | §21.9 | dependency | allow / reject / reject | V21-19/20 |
| DROP TABLE dependencies | §21.9 | catalog lifecycle | one semantic operation | V21-19 |
| same-txn DROP -> same-name CREATE | §§21.3, 21.9 | reservation | reject | V21-21 |
| DROP rollback / commit then later recreate | §21.9 | catalog/identity | predecessor / fresh replacement IDs | V21-21 |
| old RR descriptor and pending old file plus recreation | §§21.9–21.10 | descriptor/file | identities coexist; no early unlink | V21-19/21/22 |
| crash before/after DDL commit and second crash | §§21.5–21.10 | durability/recovery | orphan or committed result remains reconstructible | V21-23 |

### Complete Chapter-21 architecture-obligation coverage map

The following ledger maps the Chapter-21 obligations to verification procedures. Each row has one deterministic V21 procedure (or an explicitly reused complete lower-
layer family) and one independent expected-result oracle.

| Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference | Independent oracle |
|---|---|---|---|---|
| V21-1.1 — Chapter 18 remains sole parser/request-recovery owner | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.2 — A parse failure creates no Chapter-21 operation | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.3 — Chapter 19 remains sole SQL binding and type-resolution owner | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.4 — A bind failure allocates no durable ID or physical resource | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.5 — Chapter 21 consumes one fully bound statement | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.6 — Chapter 21 performs no SQL-name rebinding | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.7 — Resolved TableId/ColumnId/IndexId identities are preserved | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.8 — Bound types, nullability, casts, and coercions are preserved | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.9 — Bound SourceSpan and diagnostic origins are preserved | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.10 — Bound logical child and subquery occurrence ownership are preserved | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.11 — DDL binding allocates no persistent object or FileId | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-1.12 — Statement admission and CommandId allocation remain §§9.6/39.1-owned | §§21.1, 21.6.1, 21.8.1, 21.11, 21.13–21.14, 21.17; Chapters 18–20 | V21-1 | V21-1 procedure and applicable mandatory-matrix row | bound-input identity map and surface oracle |
| V21-2.1 — One CommandId is assigned per admitted statement | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.2 — One multirow statement uses one CommandId | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.3 — An internal retry creates a new statement attempt | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.4 — An RC pre-write retry reuses the statement CommandId | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.5 — An RC retry captures a fresh statement snapshot | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.6 — An RC retry discards the old target/source occurrence state | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.7 — An RC retry discards provisional row images | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.8 — An RC retry discards provisional constraint decisions | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.9 — An RC retry discards provisional ordinary-error candidates | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.10 — An RC retry discards affected-row and RETURNING state | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.11 — Retained transaction locks carry no prior semantic decision | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.12 — Retry is allowed only before the first published write | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.13 — No retry occurs after a transaction-owned write publishes | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.14 — RR does not refresh its transaction snapshot for stale-target retry | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.15 — An abandoned attempt exposes no error or result | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.16 — A terminal statement success or failure consumes its CommandId | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.17 — CommandId exhaustion follows the existing checked boundary | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-2.18 — Whole-request/autocommit transparent rerun is not introduced | §§21.15–21.16.1; §§9.6, 9.9–9.10, 15.7, 39.1 | V21-2 | V21-2 procedure and applicable mandatory-matrix row | statement ledger and publication oracle |
| V21-3.1 — Prepublication recoverable statement failure may leave explicit transaction ACTIVE | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.2 — Postpublication statement failure enters mandatory abort | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.3 — A failed multirow DML cannot partially commit | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.4 — A failed DDL cannot expose a partially committed catalog operation | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.5 — Physical DML bytes need not be undone on abort | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.6 — Private/final DDL artifacts use orphan ownership rather than logical success | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.7 — Earlier statements in a transaction abort when a later mandatory-abort failure occurs | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.8 — Transaction-owned tuple/key locks survive statement failure until terminal release | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.9 — SchemaLock and writer/publication gates follow terminal lifetime | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.10 — No gate wait occurs while a page/B+ latch is held | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.11 — Explicit successful statement result may publish while transaction remains ACTIVE | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.12 — Explicit result never claims durability | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.13 — Later explicit rollback does not rewrite an already returned successful statement result | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.14 — Autocommit withholds affected rows and RETURNING through required commit completion | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.15 — Autocommit commit failure exposes no successful statement result | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.16 — Transport failure cannot rewrite a terminal committed outcome | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.17 — Failed or retried statements expose no partial result prefix | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-3.18 — Statement result metadata is neither persisted nor recovered | §§21.2, 21.5–21.10, 21.15–21.16; §§15.1.2, 15.5–15.7, 39.1 | V21-3 | V21-3 procedure and applicable mandatory-matrix row | publication, transaction-state, lock-lifetime, and result-envelope oracles |
| V21-4.1 — INSERT consumes the resolved target table identity | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.2 — INSERT consumes canonical full target-column order | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.3 — Explicit target columns map source positions exactly | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.4 — INSERT VALUES and INSERT SELECT share one target-column contract | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.5 — Duplicate target rejection remains Chapter-19-owned | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.6 — Source expression type/coercion choices remain Chapter-19/17-owned | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.7 — Each omitted column follows its bound persisted-default or typed-NULL action | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.8 — A nullable omitted column without default receives typed NULL | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.9 — A nonnullable omitted column without a value fails | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.10 — A persisted default is already destination-typed | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.11 — INSERT does not reopen or resolve a persisted default expression | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.12 — Persisted default value is applied independently to each constructed row | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.13 — Permitted default syntax is closed and immutable | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.14 — Default expressions contain no column reference, subquery, aggregate, function, or parameter | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.15 — Default folding occurs once during DDL binding | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.16 — Default assignment coercion occurs before persistence | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.17 — Default fold/cast/constraint failure stores no partial expression | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-4.18 — INSERT subquery occurrences retain lazy once-per-attempt ownership rather than per-row reevaluation | §§21.11–21.12; §§17.8.5, 19.4.3, 19.20, 20.14 | V21-4 | V21-4 procedure and applicable mandatory-matrix row | independent row-construction and persisted-default oracles |
| V21-5.1 — INSERT validates NOT NULL on the final candidate row | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.2 — PRIMARY KEY NULL fails before uniqueness admission | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.3 — PRIMARY KEY means NOT NULL for every component plus UNIQUE | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.4 — Every fully non-NULL unique key acquires the canonical claim | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.5 — INSERT uniqueness uses current-owner rather than caller snapshot alone | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.6 — Committed live conflicting owner conflicts | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.7 — Own earlier-command live owner conflicts | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.8 — Own current-command prior input row conflicts | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.9 — Other nonterminal owner is waited/rechecked | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.10 — Aborted creator is not a live owner | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.11 — Aborted deleter leaves its creator owning as specified | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.12 — Any-NULL ordinary UNIQUE key does not conflict | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.13 — NaN uniqueness follows canonical key equality | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.14 — Signed zero uniqueness follows canonical key equality | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.15 — Composite keys use canonical component encoding and NULL rule | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.16 — A same-statement duplicate INSERT fails immediately | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.17 — A successful INSERT creates the exact fresh §15.2 tuple version | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-5.18 — Every applicable index receives the required `(key,RID)` entry | §§21.7, 21.11; §§11.9–11.10, 15.2, 16.5.4–16.5.6 | V21-5 | V21-5 procedure and applicable mandatory-matrix row | constraint/current-owner oracle plus V15 header/index oracle |
| V21-6.1 — INSERT SELECT source uses the containing statement snapshot | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.2 — INSERT self-reference does not recursively consume current-command inserts | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.3 — Source isolation is semantic and does not require a particular materialization | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.4 — Each successful logical input occurrence contributes one affected row | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.5 — Physical tuple/index/WAL multiplicity does not alter affected rows | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.6 — Only the final successful attempt contributes affected rows | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.7 — A failed INSERT has no successful affected-row count | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.8 — INSERT RETURNING uses the final new row image | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.9 — INSERT RETURNING emits one occurrence per successful logical input occurrence | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.10 — INSERT RETURNING is an unordered bag | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.11 — Ordered source does not establish RETURNING order | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.12 — Abandoned-attempt RETURNING rows are discarded | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.13 — A failed INSERT exposes no RETURNING prefix | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-6.14 — Autocommit INSERT output remains buffered through commit | §§21.11, 21.15; §§15.1.2, 15.2, 15.7.3, 20.14, 31.6, 31.9 | V21-6 | V21-6 procedure and applicable mandatory-matrix row | fixed-source, affected-row, RETURNING-bag, and result-envelope oracles |
| V21-7.1 — UPDATE logical child supplies exact target RID | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.2 — UPDATE logical child supplies every required old value | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.3 — Target materialization completes before target mutation | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.4 — Duplicate discovery of one RID finalizes one target | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.5 — Finalized targets are keyed by logical target RID identity | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.6 — A target is acted upon at most once | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.7 — Access-key changes cannot rediscover a replacement as a target | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.8 — Predicate changes cannot rediscover a replacement as a target | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.9 — Target-spool spill preserves the semantic target set | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.10 — Spool finalization failure publishes no target write | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.11 — Every target is re-fetched after TUPLE_WRITE wait/grant | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.12 — Every target is revalidated after unique-lock wait/grant | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.13 — Changed qualification can remove a stale target | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.14 — RC stale conflict before publication may restart the attempt | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.15 — RC stale conflict after publication cannot restart | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.16 — RR stale conflict follows the serialization owner | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.17 — Removed stale/nonqualifying target contributes no count/result | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-7.18 — Replacement attempt rediscovers targets independently | §21.13; §§15.1.2, 15.3, 15.7, 31.1–31.5 | V21-7 | V21-7 procedure and applicable mandatory-matrix row | exact-RID target-set and stale-owner oracles |
| V21-8.1 — UPDATE consumes fully bound SET/WHERE/RETURNING input | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.2 — Each assignment maps one resolved ColumnId to one typed expression | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.3 — Every SET RHS reads the complete old target row | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.4 — All SET assignments are simultaneous | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.5 — `SET a=b,b=a` swaps old values rather than evaluating sequentially | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.6 — Bound assignment coercion is applied after RHS evaluation | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.7 — Unmentioned columns copy their old values | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.8 — The complete candidate row contains every target descriptor column | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.9 — WHERE remains part of target materialization | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.10 — Supported UPDATE subqueries are uncorrelated Chapter-20 occurrences | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.11 — Assignment/RETURNING subquery errors use the ordinary write boundary | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.12 — Within-expression demand/evaluation order remains Chapters 17/20-owned | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.13 — Assignment storage order cannot change the final row | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.14 — `SET x=x` produces a complete value-preserving candidate | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-8.15 — Logical planning requests every old/new value required by physical tuple and RETURNING construction | §21.13; §§17.8.5, 19.4.3, 20.14 | V21-8 | V21-8 procedure and applicable mandatory-matrix row | immutable-old-row expression and candidate-row oracle |
| V21-9.1 — UPDATE revalidates/finalizes the old target before row construction | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.2 — Every RHS is evaluated before descriptor-wide NOT NULL validation | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.3 — Every assignment coercion completes before NOT NULL validation | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.4 — The complete replacement row exists before NOT NULL validation | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.5 — Every descriptor-declared nonnullable column is checked | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.6 — Non-PK nonnullable columns are checked | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.7 — PRIMARY KEY nonnullable columns are checked | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.8 — Unmentioned copied nonnullable columns are checked | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.9 — Nullable columns may remain NULL | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.10 — NOT NULL validation precedes immediate uniqueness | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.11 — Violation publishes no new tuple version for that target | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.12 — Violation publishes no old-version xmax/cmax for that target | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.13 — Violation publishes no new referring index entry for that target | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.14 — Violation uses canonical ConstraintViolation | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.15 — Explicit NULL-producing RHS retains its responsible SourceSpan | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.16 — No synthetic SQL offset is invented for structural corruption | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-9.17 — Simultaneous assignment and no-op acted-target semantics remain unchanged | §21.13; §§15.3, 17.8.5, 39.1, 39.3 | V21-9 | V21-9 procedure and applicable mandatory-matrix row | descriptor-wide NOT NULL and publication-prefix oracle |
| V21-10.1 — UPDATE uniqueness is immediate per finalized target | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.2 — NOT NULL validation precedes UPDATE uniqueness | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.3 — Unchanged unique key checks current owners | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.4 — Changed unique key checks current owners | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.5 — Only the exact old RID may be self-excluded | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.6 — Another row owned by the same transaction is not self-excluded | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.7 — Current-command replacement remains a conflicting owner as specified | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.8 — Same-statement two-target collision fails | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.9 — Two-row unique-key swap fails | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.10 — Three-row unique-key cycle fails | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.11 — No deferred final-state permutation rule is introduced | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.12 — NULL-containing ordinary UNIQUE key follows the no-conflict rule | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.13 — NaN and signed-zero follow canonical key encoding | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.14 — A successful UPDATE creates a fresh replacement RID/version | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.15 — Old physical index entries are retained | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.16 — Every index receives a new replacement entry | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-10.17 — Unchanged indexed key may retain old/new RID candidates under one key | §§21.7, 21.13; §§11.9–11.10, 15.3 | V21-10 | V21-10 procedure and applicable mandatory-matrix row | immediate uniqueness and index-entry multiset oracle |
| V21-11.1 — UPDATE counts each distinct finalized acted target once | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.2 — `SET x=x` counts once per qualifying target | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.3 — Duplicate target discovery cannot double count | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.4 — Stale/nonqualifying target contributes zero | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.5 — Abandoned attempt count is discarded | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.6 — Failed UPDATE publishes no successful count | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.7 — UPDATE RETURNING uses the complete final new row | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.8 — UPDATE RETURNING emits one occurrence per distinct acted target | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.9 — Duplicate target discovery emits no duplicate RETURNING row | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.10 — UPDATE RETURNING is unordered | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.11 — RID/source/spool/mutation order is nonsemantic | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-11.12 — Failed or autocommit-uncommitted UPDATE exposes no RETURNING prefix | §§21.13, 21.15; §§15.1.2, 15.3, 31.9 | V21-11 | V21-11 procedure and applicable mandatory-matrix row | distinct-target affected-row and RETURNING-bag oracle |
| V21-12.1 — DELETE consumes a fully bound one-table target | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.2 — DELETE logical child supplies exact target RID | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.3 — DELETE logical child supplies old values required by semantics/RETURNING | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.4 — DELETE target materialization completes before mutation | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.5 — Duplicate discovery finalizes one RID once | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.6 — DELETE target is revalidated after waits | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.7 — RC stale behavior follows the pre/post-write boundary | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.8 — RR stale behavior follows the serialization owner | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.9 — Stale/nonqualifying target is not deleted | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.10 — Physical rediscovery cannot delete a target twice | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.11 — DELETE publishes current TxnId as xmax | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.12 — DELETE publishes current CommandId as cmax | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.13 — DELETE creates no new tuple version | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.14 — DELETE retains physical index entries | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.15 — Vacuum remains index-garbage owner | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.16 — DELETE affected rows count distinct finalized deletes once | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.17 — DELETE RETURNING uses the retained old row once per deleted target | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-12.18 — DELETE RETURNING is unordered and reclamation-independent | §§21.14–21.15; §§15.4, 31.1–31.5, 31.8–31.9 | V21-12 | V21-12 procedure and applicable mandatory-matrix row | exact-RID delete, retention, affected-row, and RETURNING-bag oracles |
| V21-13.1 — Ordinary candidate must arise from one DML row occurrence | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.2 — Candidate occurrence belongs to the final statement attempt | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.3 — Binding/logical prerequisites must be satisfied | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.4 — Target revalidation prerequisites must be satisfied | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.5 — Demanded scalar/subquery errors may be candidates | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.6 — Descriptor-wide final-row NOT NULL violations may be candidates | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.7 — Immediate UNIQUE violations may be candidates | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.8 — Only already-supported other row constraints may be candidates | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.9 — Abandoned retry-attempt errors are excluded | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.10 — Deduplicated-away target occurrences are excluded | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.11 — Stale nonfinal targets are excluded | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.12 — Skipped/nonqualifying occurrences are excluded | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.13 — Smallest responsible SourceSpan start is primary precedence | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.14 — Equal starts use shorter/more-specific represented SourceSpan | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.15 — Identical span uses scalar/subquery/final-row-expression phase first | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.16 — Identical span uses descriptor-wide NOT NULL phase second | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.17 — Identical span uses immediate UNIQUE phase third | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.18 — Existing within-phase precedence remains authoritative | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.19 — Complete semantic ties are observationally equivalent | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.20 — RID/page/source-row index cannot break a tie | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.21 — Spool/vector/batch/hash/index/operator/scheduler order cannot break a tie | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.22 — Ordered logical input does not imply first-row error precedence | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.23 — Unordered logical input remains unordered | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-13.24 — Scalar child demand/evaluation order is not changed | §21.16.1; Chapters 11, 15, 17, 20; §39.1 | V21-13 | V21-13 procedure and applicable mandatory-matrix row | ordinary-error candidate set and semantic-precedence oracle |
| V21-14.1 — All INSERT RETURNING results are unordered bags | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.2 — All UPDATE RETURNING results are unordered bags | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.3 — All DELETE RETURNING results are unordered bags | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.4 — Bag equality uses exact typed values and occurrence multiplicity | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.5 — Different equal-bag sequences are semantically equivalent | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.6 — INSERT image/multiplicity remain final-new per input | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.7 — UPDATE image/multiplicity remain final-new per distinct acted target | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.8 — DELETE image/multiplicity remain retained-old per distinct deleted target | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.9 — RETURNING order freedom does not alter affected rows | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.10 — RETURNING order freedom does not alter error selection | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.11 — Cancellation remains outside ordinary error arbitration | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.12 — Deadlock/resource/storage/concurrency failures retain independent owners | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.13 — Dynamic future failures need not be predicted | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.14 — Lower-precedence visitation cannot hide an established higher-precedence ordinary error | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.15 — Physical publication order cannot change that error's §39.1 consequence | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.16 — No prevalidation/staging/spool/sort algorithm is mandated | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-14.17 — No partial failed or abandoned RETURNING buffer is exposed | §§21.15–21.16.1; §§15.1.2, 15.7.3, 31.9, 39.1 | V21-14 | V21-14 procedure and applicable mandatory-matrix row | RETURNING multiset, dynamic-failure, and publication-consequence oracles |
| V21-15.1 — CREATE TABLE acquires SchemaLock under §11.13 rules | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.2 — Name availability is revalidated at execution | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.3 — Own earlier-DROP reservations participate in name revalidation | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.4 — TableId is allocated only during execution | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.5 — Heap and FSM FileIds are allocated only during execution | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.6 — Durable catalog-object high-water advances before persistent use | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.7 — Aborted/crashed allocations may leave gaps | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.8 — TableId/IndexId/ConstraintId/FileId reuse is forbidden | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.9 — Fresh table receives exclusive TableWriterGate ownership | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.10 — Private heap/FSM files are initialized before catalog publication | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.11 — ColumnIds are assigned 1 through N for schema version 1 | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.12 — Required PK/UNIQUE backing indexes are built as private objects | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.13 — All required files reach durable final names before catalog rows | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.14 — Catalog rows use the exact Chapter-16 schema | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.15 — Created descriptors remain transaction-local before commit | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.16 — Creating transaction may use its retained exclusive gate for DML | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.17 — Creating transaction may reuse that gate for later CREATE INDEX | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.18 — Autocommit success enters commit after statement completion | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.19 — Explicit CREATE remains transaction-local until user commit | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.20 — SchemaLock/gates remain held through terminal publication | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.21 — Committed CREATE publishes globally coherent descriptor/name state | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.22 — Abort keeps catalog rows invisible | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.23 — Abort transfers physical survivors to orphan retirement | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.24 — Pre-catalog failure may remain ACTIVE only after deterministic orphan transfer | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.25 — Post-catalog failure mandates abort | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-15.26 — Committed table has tuple SchemaVer 1 | §§21.2, 21.4–21.7; §§4.7, 13.2.6, 16.5, 39.1 | V21-15 | V21-15 procedure and applicable mandatory-matrix row | DDL catalog/file lifecycle, allocator, and result-envelope oracles |
| V21-16.1 — Schema-changing CREATE INDEX uses database-wide SchemaLock | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.2 — CREATE INDEX uses target TableWriterGate exclusive | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.3 — Existing target writers drain to terminal outcome | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.4 — New target writers remain blocked through index terminal outcome | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.5 — Reads may continue because private index is catalog-invisible | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.6 — Same-table shared-to-exclusive upgrade is proactively rejected | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.7 — Different-table gate acquisition remains legal through unified graph | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.8 — Retained exclusive gate for own new table is reusable | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.9 — No transaction wait occurs under heap/B+ latch | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.10 — DDL lock order follows SchemaLock then target gate then manifest/publication | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.11 — Bound target TableId is revalidated | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.12 — Bound ordered key ColumnIds are consumed unchanged | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.13 — Index-name availability is revalidated | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.14 — Own earlier-DROP reservation participates in revalidation | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.15 — IndexId and FileId allocate only during execution | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.16 — Private B+ tree starts empty | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.17 — MANIFEST_CHANGE is acquired before catalog manifest publication | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.18 — Complete current table/index manifest is revalidated | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.19 — Final B+ name is durable before catalog rows | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.20 — Catalog index/key rows use Chapter-16 ordering | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.21 — Gate/manifest/SchemaLock ownership survives through terminal outcome | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-16.22 — No physical scan/build order is semantic | §§21.2–21.2.1, 21.8; §§11.13, 14.17.1, 15.5–15.6 | V21-16 | V21-16 procedure and applicable mandatory-matrix row | gate-event graph and DDL lifecycle oracle |
| V21-17.1 — Build view is fixed at CREATE INDEX command boundary | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.2 — Writer drain precedes build-set observation | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.3 — Build view is not ordinary SQL snapshot visibility | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.4 — Build view is not committed-only visibility | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.5 — Build view is not raw physical version enumeration | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.6 — Every committed live current owner is included | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.7 — Own earlier-command INSERT row is included | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.8 — Own earlier-command UPDATE replacement is included | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.9 — Own earlier UPDATE predecessor is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.10 — Own earlier-command deleted row is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.11 — Aborted creator/version is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.12 — Superseded historical version is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.13 — Other nonterminal version unable to join commit state is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.14 — Reclamation-only retained history is excluded | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.15 — Older RR SQL snapshot cannot underfill the build | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.16 — Ordinary RR SELECT visibility remains unchanged | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.17 — CREATE INDEX performs no recursive row mutation inclusion | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-17.18 — Later-command mutations are not invented | §21.8.2; Chapters 9–10; §§11.13, 20.4 | V21-17 | V21-17 procedure and applicable mandatory-matrix row | transaction-local current-owner build-set oracle |
| V21-18.1 — UNIQUE build uses exactly the V21-17 build set | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.2 — Every fully non-NULL duplicate group is detected | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.3 — Any-NULL unique keys do not conflict | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.4 — Composite key comparison is canonical | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.5 — NaN follows canonical key equality | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.6 — Signed zero follows canonical key equality | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.7 — Own earlier-command rows participate in duplicate detection | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.8 — Duplicate detection is complete across physical partitions/order | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.9 — Build-row order cannot change validation result | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.10 — Successful build inserts every required `(key,RID)` | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.11 — Published index entry multiset equals required current-owner multiset | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.12 — No missing current owner is permitted at catalog commit | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.13 — Same-transaction CREATE TABLE/INSERT/CREATE INDEX commits complete | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.14 — Half-built index is never visible to planning | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-18.15 — Failed unique build becomes an orphan candidate without catalog visibility | §21.8.2; §§8.23, 11.10.2, 11.10.9, 16.5.4–16.5.5 | V21-18 | V21-18 procedure and applicable mandatory-matrix row | canonical unique-build and index-completeness oracles |
| V21-19.1 — DROP TABLE and DROP INDEX are transactional at catalog visibility | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.2 — DROP acquires SchemaLock | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.3 — DROP acquires owning table's exclusive writer gate | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.4 — DROP acquires MANIFEST_CHANGE for its scope | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.5 — Current object identity/manifest/LIVE state is revalidated | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.6 — LIVE-to-RETIRING precedes catalog deletion only after admission checks | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.7 — DROP TABLE uses table plus dependent indexes as one manifest scope | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.8 — DROP TABLE retires dependent constraints and indexes as one operation | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.9 — Ordinary DROP INDEX affects its owning table/index scope | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.10 — No new object/statistics claim is admitted after RETIRING | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.11 — Abort restores LIVE through the object-publication protocol | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.12 — Abort makes catalog deletion xmax ineffective | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.13 — Commit makes object invisible to new catalog snapshots | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.14 — Older catalog snapshots may still resolve predecessor | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.15 — Immutable old descriptors remain usable while retained | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.16 — DROP does not unlink files immediately | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.17 — Retirement waits for catalog-global death | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.18 — Retirement waits for descriptor/query-handle release | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.19 — Retirement waits for BufferPool/file-owner drain | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.20 — No-new-fetch/pin and frame drain follow §7.12.5 | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.21 — Unlink uses managed-file §4.7.7 protocol | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.22 — Directory fsync is required for durable retirement | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.23 — Unlink/sync failure leaves semantic DROP committed and cleanup pending | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-19.24 — Crash recovery resumes/defer retirement without resurrecting dropped object | §§21.2.1, 21.9; §§4.7.7, 7.12.5, 14.17.1, 16.10 | V21-19 | V21-19 procedure and applicable mandatory-matrix row | DROP catalog-MVCC and file-retirement state oracle |
| V21-20.1 — Constraint-owned index is defined by a live PK/UNIQUE catalog reference | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.2 — Constraint-owned ownership is semantic even when canonical index name is NULL | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.3 — Standalone DROP INDEX rejects a PRIMARY KEY backing IndexId | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.4 — Standalone DROP INDEX rejects a UNIQUE backing IndexId | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.5 — Rejection occurs before catalog deletion | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.6 — Rejection occurs before descriptor retirement | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.7 — Rejection occurs before LIVE-to-RETIRING | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.8 — Rejection occurs before file-retirement eligibility | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.9 — Rejection does not mutate the owning constraint | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.10 — Rejection does not delete the index row | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.11 — Rejection leaves physical file lifecycle unchanged | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.12 — Rejection reports CatalogError | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.13 — DROP target-name construct supplies diagnostic origin | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-20.14 — DROP TABLE dependency operation remains legal | §21.9; §§16.5.4, 16.5.6, 39.1 | V21-20 | V21-20 procedure and applicable mandatory-matrix row | constraint/index dependency graph and unchanged-state oracle |
| V21-21.1 — Catalog visibility and namespace reservation are distinct | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.2 — Own successful DROP may hide predecessor from later commands | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.3 — Own successful DROP reserves the canonical name | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.4 — Reservation lasts through terminal transaction outcome | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.5 — Reservation follows the existing table/index name classes | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.6 — Same-transaction DROP TABLE then same-name CREATE TABLE rejects | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.7 — Same-transaction DROP INDEX then same-name CREATE INDEX rejects | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.8 — CREATE name revalidation checks current owner conflicts | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.9 — CREATE name revalidation checks own earlier-DROP reservations | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.10 — Abort makes predecessor deletion ineffective | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.11 — Abort creates no replacement identity | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.12 — Committed DROP removes the live transaction reservation | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.13 — Later transaction may reuse name if otherwise available | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.14 — Later recreation receives fresh object IDs | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.15 — Later recreation receives fresh FileIds | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.16 — Old snapshot/descriptor may coexist with replacement | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.17 — Old/new objects remain distinguished by persistent identity | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-21.18 — Pending RETIRED_LINKED predecessor file does not block logical name reuse | §§21.3, 21.6.2, 21.8.2, 21.9; §§16.2–16.3, 16.10 | V21-21 | V21-21 procedure and applicable mandatory-matrix row | catalog visibility/name-reservation/identity oracle |
| V21-22.1 — READ COMMITTED binding uses statement catalog snapshot | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.2 — REPEATABLE READ binding uses transaction catalog snapshot | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.3 — Uncommitted DDL from another transaction is invisible | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.4 — Own completed earlier DDL is visible through self/CommandId rules | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.5 — Long-running RR does not suddenly resolve new committed object | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.6 — Cache cannot override catalog MVCC visibility | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.7 — Cached descriptor requires visibility to binding snapshot | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.8 — Uncommitted CREATE metadata is transaction-local | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.9 — Committed CREATE publishes current cache/name entry | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.10 — Committed DROP removes current-name lookup | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.11 — Old immutable descriptors are never mutated | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.12 — Active plans retain their descriptor identity | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.13 — CREATE TABLE establishes tuple SchemaVer 1 | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.14 — CREATE INDEX updates applicable manifest under frozen SchemaVer rules | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.15 — Failed DDL does not falsely publish a schema/cache state | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.16 — Postcommit cache install may use safe invalidate/bypass | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.17 — Postcommit cache incoherence yields NC without changing COMMITTED | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-22.18 — Same-name replacement cannot rebind predecessor descriptor | §§21.3, 21.6.2, 21.8.2, 21.9–21.10; Chapter 16 | V21-22 | V21-22 procedure and applicable mandatory-matrix row | catalog MVCC, descriptor identity, SchemaVer, and cache oracle |
| V21-23.1 — DML heap mutation uses existing WAL grammar | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.2 — DML index mutation uses existing B+ MTR grammar | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.3 — Catalog tuple mutation is WAL-governed | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.4 — Chapter 21 introduces no new WAL record family | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.5 — Private file creation follows §4.7 namespace/durability rules | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.6 — Final-name publication requires no-replace rename | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.7 — Final-name publication requires parent-directory durability | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.8 — Catalog CREATE cannot commit before every required file is final-durable | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.9 — CREATE INDEX contents are durable/recoverable at committed publication | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.10 — DROP catalog outcome is independent of delayed physical unlink | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.11 — DROP unlink durability requires directory synchronization | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.12 — Private CREATE crash prefix is classified as orphan/incomplete | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.13 — Final-name/no-catalog crash prefix is classified without live object | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.14 — Uncommitted catalog CREATE recovers as loser/invisible | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.15 — Committed CREATE recovers as visible complete object | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.16 — Uncommitted DROP recovers predecessor as live | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.17 — Committed DROP recovers predecessor as dropped | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.18 — Pending file retirement resumes or remains owned | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.19 — Recovery replays physical WAL rather than SQL statements | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.20 — Recovery reconstructs no statement attempts/counts/RETURNING | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.21 — Committed DDL survives a crash before cache installation | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.22 — Recovered committed CREATE survives a second crash | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.23 — Recovered committed DROP survives a second crash | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-23.24 — No transient recovery-only hidden state is correctness authority | §§21.5–21.10; Chapters 4, 7, 12–15 | V21-23 | V21-23 procedure and applicable mandatory-matrix row | durable-prefix, file-lifecycle, recovery, and second-crash oracles |
| V21-24.1 — enforced constraint set is NOT NULL, PRIMARY KEY, and UNIQUE | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.2 — CHECK constraint execution is outside the defined architecture | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.3 — Foreign-key execution is outside the defined architecture | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.4 — INSERT NOT NULL checks final candidate value | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.5 — UPDATE NOT NULL checks complete candidate row | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.6 — PRIMARY KEY implies descriptor NOT NULL for all components | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.7 — PRIMARY KEY fully non-NULL value uses UNIQUE current-owner semantics | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.8 — Ordinary UNIQUE permits any-NULL key | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.9 — Composite partial-NULL key follows any-NULL rule | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.10 — Canonical string/value equality feeds key encoding | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.11 — NaN key semantics match Chapter 8/11 owner | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.12 — Signed-zero key semantics match Chapter 8/11 owner | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.13 — Same-statement INSERT duplicate conflicts | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.14 — Same-statement UPDATE collision conflicts | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.15 — Key swap conflicts | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.16 — Key cycle conflicts | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.17 — Concurrent nonterminal owner is waited/rechecked | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.18 — Aborted creator/deleter statuses are rechecked exactly | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.19 — Own earlier/current-command owners follow §11.10.6 | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-24.20 — Only exact old UPDATE RID receives self-exclusion | §§21.6–21.8, 21.13; §§11.10, 16.5.6, 39.3 | V21-24 | V21-24 procedure and applicable mandatory-matrix row | closed constraint and uniqueness matrix oracle |
| V21-25.1 — RID visitation order cannot change committed state | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.2 — Page placement/order cannot change committed state | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.3 — Source occurrence visitation cannot select ordinary error | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.4 — Target spool order cannot change operation result | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.5 — Index traversal order cannot change uniqueness/build result | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.6 — Vector width and batch boundaries cannot change semantics | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.7 — Hash seed/bucket order cannot change semantics | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.8 — Worker schedule cannot choose ordinary row error | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.9 — Allocator result cannot change semantics | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.10 — Pointer identity cannot change semantics | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.11 — Physical CREATE INDEX scan algorithm cannot change build set | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.12 — Physical CREATE INDEX build algorithm cannot change completeness | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.13 — Physical DML algorithm cannot change affected rows | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.14 — Physical DML algorithm cannot change RETURNING bag | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.15 — Different unordered RETURNING sequences may be equivalent | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.16 — Equal RETURNING bag is required for such equivalence | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.17 — Dynamic failures retain occurrence-time owners | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-25.18 — All successful perturbations yield equal final logical state | §§21.8.2, 21.15–21.16.1, 21.20; Chapters 20, 31 | V21-25 | V21-25 procedure and applicable mandatory-matrix row | metamorphic environment and observable-state oracle |
| V21-26.1 — ANALYZE consumes one fully bound base-table target | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.2 — Bound ANALYZE captures TableId | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.3 — Bound ANALYZE captures immutable TableDescriptor | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.4 — Bound ANALYZE captures current SchemaVer | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.5 — Bound ANALYZE captures analyzed ColumnIds | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.6 — Bound ANALYZE captures currently visible IndexIds | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.7 — Captured identities are scan inputs rather than final publication authority | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.8 — Final current-object/schema/index-manifest revalidation is mandatory | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.9 — ANALYZE acquires no SchemaLock merely for stable row set | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.10 — ANALYZE acquires no exclusive writer gate merely for stable row set | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.11 — ANALYZE values use normal RC statement snapshot | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.12 — ANALYZE values use normal RR transaction snapshot/current command boundary | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.13 — Concurrent DML may make statistics approximate without invalidating correctness | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.14 — Statistics rows use ordinary transaction/catalog MVCC | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.15 — StatsDescriptor is transaction-local before commit | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.16 — Own later statements may use transaction-local descriptor | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.17 — Global cache changes only at terminal COMMITTED | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.18 — Failed/cancelled prepublication ANALYZE publishes no partial descriptor | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.19 — Failure after statistics-row publication mandates abort | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-26.20 — Same-manifest DDL/ANALYZE races follow §14.17.1 claims/revalidation | §21.17.1; §§14.17.1, 19.1, 34.14, 39.1 | V21-26 | V21-26 procedure and applicable mandatory-matrix row | bound ANALYZE identity, snapshot, generation, and publication oracle |
| V21-27.1 — Default blob magic is exactly ASCII DBLUSDEF | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.2 — Default blob format_version is exactly 1 | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.3 — Default blob flags are zero | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.4 — Default blob reserved32 is zero | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.5 — Default blob header is 24 bytes | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.6 — All multibyte fields are little-endian | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.7 — Payload is exactly one PersistedScalarV1 | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.8 — total_length equals 24 plus scalar length | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.9 — No trailing bytes are accepted | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.10 — CRC32C covers total_length with checksum bytes logically zero | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.11 — MAX_DEFAULT_VALUE_BLOB is 4096 bytes | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.12 — Exactly-4096-byte valid blob is accepted | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.13 — Oversize default is rejected as unsupported | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.14 — Decoder validates magic/version/flags/reserved | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.15 — Decoder validates length and checksum | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.16 — Decoder validates PersistedScalarV1 structure | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.17 — Decoded scalar TypeId must equal target column TypeId | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.18 — Version zero/malformed v1 is required-metadata corruption | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.19 — Positive version greater than one is UNSUPPORTED_DEFAULT_FORMAT | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.20 — Malformed greater/recognized dispatch is not guessed | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.21 — Expression-bearing blob is not parsed as scalar | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.22 — Original SQL text is not execution authority | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.23 — No function/operator identity tree is persisted | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-27.24 — Reopen reconstructs the same typed constant default | §§21.12–21.12.1; §§4.14.2, 17.10.2, 17.13 | V21-27 | V21-27 procedure and applicable mandatory-matrix row | literal byte builder, CRC32C, and independent PersistedScalarV1 decoder |
| V21-28.1 — Every listed statement kind has one canonical upstream/downstream owner | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.2 — CREATE TABLE is admitted only through its bound handoff | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.3 — CREATE/UNIQUE INDEX is admitted only through its bound handoff | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.4 — DROP TABLE/INDEX is admitted only through its bound handoff | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.5 — INSERT/UPDATE/DELETE/SELECT use the stated bound/logical handoffs | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.6 — BEGIN/COMMIT/ROLLBACK use transaction owners | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.7 — VACUUM/ANALYZE use maintenance owners | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.8 — EXPLAIN/EXPLAIN ANALYZE use presentation/execution owners | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.9 — Supported subquery surface equals §20.14 exactly | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.10 — Generic parser or IR capacity does not expand semantics | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.11 — Every listed excluded feature fails explicitly | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.12 — Unsupported CHECK/foreign key do not receive partial runtime semantics | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.13 — Expression/partial index syntax is not silently approximated | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.14 — Correlated/data-modifying subqueries are not silently decorrelated/executed | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-28.15 — Excluded types/features do not alter persisted formats | §§21.6.1, 21.8.1, 21.17–21.19; Chapters 18–20 | V21-28 | V21-28 procedure and applicable mandatory-matrix row | literal closed statement/subquery/feature registry |
| V21-29.1 — Parser output names and binder resolved IDs remain distinct | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.2 — Executor performs no SQL name resolution | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.3 — Logical operations encode no physical algorithms | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.4 — BindingId/ColumnId/LogicalSlotId/heap SlotId/vector position remain distinct | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.5 — All operation expressions have resolved type/nullability | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.6 — Hidden DML target RID survives until DML consumption | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.7 — INSERT operation carries complete canonical target mapping | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.8 — UPDATE operation carries all old values needed for RHS and tuple construction | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.9 — UPDATE operation carries complete candidate-row descriptor | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.10 — DELETE operation carries retained old values needed by RETURNING | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.11 — DML subqueries are only supported uncorrelated forms | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.12 — Required index manifest is stable for the operation | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.13 — Catalog cache cannot bypass snapshot visibility | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.14 — Uncommitted DDL is not globally published | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.15 — Unsupported SQL is not partially reinterpreted | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.16 — Validation failure occurs before data-changing effects | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.17 — Plan validation introduces no persistent format | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-29.18 — Operation manifest does not prescribe executor data structures | §§21.11, 21.13–21.14, 21.20; Chapters 19–20, 31 | V21-29 | V21-29 procedure and applicable mandatory-matrix row | operation-manifest validator and invariant mutation table |
| V21-30.1 — Every Chapter-21 heading has verification coverage | Architecture front matter; Chapter 21; §41.4–§41.5 | V21-30 | V21-30 procedure and applicable mandatory-matrix row | cross-owner ledger, mandatory matrices, and oracle-presence check |
| V21-30.2 — Every Chapter-21 atomic obligation has one ledger row | Architecture front matter; Chapter 21; §41.4–§41.5 | V21-30 | V21-30 procedure and applicable mandatory-matrix row | cross-owner ledger, mandatory matrices, and oracle-presence check |
| V21-30.3 — Every ledger row names an architecture owner | Architecture front matter; Chapter 21; §41.4–§41.5 | V21-30 | V21-30 procedure and applicable mandatory-matrix row | cross-owner ledger, mandatory matrices, and oracle-presence check |
| V21-30.4 — Every ledger row names a deterministic procedure or complete reusable family | Architecture front matter; Chapter 21; §41.4–§41.5 | V21-30 | V21-30 procedure and applicable mandatory-matrix row | cross-owner ledger, mandatory matrices, and oracle-presence check |
| V21-30.5 — Every ledger row names an independent oracle | Architecture front matter; Chapter 21; §41.4–§41.5 | V21-30 | V21-30 procedure and applicable mandatory-matrix row | cross-owner ledger, mandatory matrices, and oracle-presence check |


The Chapter-21 procedures compose the canonical semantic-planning owners.
Physical-plan/runtime verification belongs to the Chapter-22 family.

---

## Front-End Error and Source-Span Tests

Drive one representative failure through each architecture-owned category in §§21.16 and
39.2–39.3:

```text
LexerError
ParserError
BindError
TypeError
CatalogError
ConstraintDefinitionError
UnsupportedFeature / UnsupportedCorrelation
CardinalityError
FrontEndResourceLimit
OutOfMemory
```

For SQL-originating failures, assert the canonical retained SourceSpan selected by §18.8
and the Chapter-18 SourceSpan matrix above: offending lexical range, unexpected-token span,
zero-width EOF span, or directly responsible bound syntactic construct with the
earliest-source tie rule. Cover unknown/ambiguous identifiers, invalid casts/operators,
invalid constraint definitions, unsupported semantic subquery properties, and the scalar
subquery occurrence used by a runtime cardinality error. Independently inject a deliberate
front-end guard and an unsatisfied allocation to prove that resource outcomes remain
distinct from source-language and semantic categories. Preserve lower-layer categories
where §§21.16 and 39 require it. Internal logical-validator defects use internal
invariant/validation errors and need no invented SQL span.

---

## Type-System Property Tests

This section is the detailed procedural owner for the SQL type/value contract in
[`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 17. Chapter 17 remains authoritative for
semantic results; Chapters 5, 8, and 16 remain authoritative for ordinary tuple bytes,
index-key bytes, and persistent TypeIds/catalog descriptors. The procedures below compare
those owners without creating a second scalar registry.

### Deterministic type/value harness and fixture discipline

Use table-driven semantic fixtures and deterministic event barriers rather than timing or
sleeps. The harness records architecture-visible events without requiring any source API or
container layout:

```text
binding:
    token handoff -> literal classification -> unresolved marker creation
    -> contextual candidate/common type -> inserted cast -> overload selection
    -> resolved expression or bind error

evaluation:
    child demanded/skipped -> child value/error -> operator/cast begins
    -> checked boundary/result -> typed value, SQL UNKNOWN, or controlled error

folding:
    bound tree -> demanded fold subtree -> folded value/error
    -> runtime evaluation of the same bound tree

persistence:
    semantic value -> ordinary tuple / PersistedScalarV1 / index-key encoding
    -> structural validation -> schema/type resolution -> semantic decode
```

Every fixture is canonical except for the one condition under test. Corruption fixtures
mutate one field, bit, byte, length, TypeId, or padding region at a time after constructing
a valid encoding and recomputing only enclosing checksums that are not the tested defect.
Semantic-input errors use valid storage and malformed SQL values; persisted corruption uses
validly framed SQL-independent bytes with one malformed persisted field. Fixed-seed
property generators partition all scalar domains into boundary and semantic classes, but
fixed seeds are replay aids rather than the oracle.

Binding observations assert concrete logical type, nullability, inserted casts, selected
registry identity, and exact error category. Evaluation observations assert type, NULL
state, semantic value or exact error, and which children were demanded. Persistence
observations assert exact bytes where the owning format is canonical and semantic/order
equivalence where different codecs intentionally use different bytes.

### Independent scalar oracles

Production parsing, comparison, encoding, formatting, and dispatch code must not verify
itself. Use these independent test-side models:

| Oracle | Independent construction |
|---|---|
| TypeId and closed registry | Literal map `1..7` plus a separately authored finite set of types, operators, predicates, casts, and functions |
| Contextual NULL | Constraint solver over the Chapter-17 context table; its state is called an **unresolved type marker**, never SQL UNKNOWN |
| SQL 3VL | Literal TRUE/FALSE/UNKNOWN tables, with UNKNOWN represented as typed BOOLEAN NULL |
| Integer arithmetic | Arbitrary-precision mathematical integers followed by explicit target-domain checks |
| Integer quotient/remainder | Mathematical truncation toward zero plus `a=q*b+r`, remainder bound, and dividend-sign rule |
| FLOAT64 classes/order | Bit-level sign/exponent/significand classifier and Chapter-17 normalization, without host unordered predicates |
| FLOAT64 arithmetic | Exact significand/exponent or arbitrary-precision rational operation followed by one binary64 nearest/ties-even rounding per bound operator |
| VARCHAR | Length-delimited byte arrays and unsigned-byte lexicographic comparison |
| DATE/TIMESTAMP | Independent proleptic-Gregorian civil-day conversion and checked integer microsecond arithmetic |
| Cast/coercion | Literal 7-by-7 cast table plus separate context/assignment tables; direct edges are evaluated directly, not by production cast chaining |
| Literal classification | Checked mathematical token magnitude and separately authored ASCII grammars |
| PersistedScalarV1 | Test-side byte builder/reader using the exact §17.13 offsets, little-endian helpers, checked length arithmetic, and canonical padding |
| Ordinary tuple handoff | Chapter-5 test-side layout/null-bitmap/scalar codec already used by Tuple codec tests |
| Index order | Test-side semantic comparator sorted independently from Chapter-8 encoded-byte sorting |
| Historical schema | Immutable `(TableId,SchemaVer)` descriptor map independent of the current descriptor/cache |
| Folding/runtime | Small interpreted bound-expression tree using the independent scalar and demand models, compared with folded and runtime paths |
| Equality modes | Separate predicate, grouping, hash, UNIQUE, and physical-order relations; no generic host equality |
| Error classification | Literal decision table separating bind/user semantic, runtime resource, required corruption, and advisory-statistics invalidation |

For FLOAT64 text, use an independently implemented shortest-round-trip model or a vetted
externally generated corpus whose expected bytes and bit patterns are embedded as fixtures;
the production parser/formatter is never the expected-value generator. For platform
determinism, replay the same corpus under distinct locale/timezone settings and available
floating environments, and include software-oracle cases that expose extended precision,
reassociation, and contraction even when one host cannot manifest all variants.

### TypeId, closed-registry, and runtime-tag verification

The exact persistent registry fixture is:

| TypeId | SQL type |
|---:|---|
| `1` | BOOLEAN |
| `2` | INT32 |
| `3` | INT64 |
| `4` | FLOAT64 |
| `5` | DATE |
| `6` | TIMESTAMP |
| `7` | VARCHAR |

Round-trip every legal TypeId through catalog descriptors and metadata scalars. Reject
`0`, `8`, and `UINT32_MAX` as non-storable/unknown in recognized-v1 data with the owning
required-metadata corruption or advisory-statistics invalidation outcome. Run the semantic
registry through a test adapter whose runtime tags are deliberately permuted relative to
`1..7`; all binding/evaluation results and persistent bytes must remain unchanged.

Enumerate the closed type set and assert that NULL, SQL UNKNOWN, the unresolved type marker,
DECIMAL, BLOB, UUID, INTERVAL, JSON, arrays, timezone-aware timestamps, and parameterized
`VARCHAR(n)` do not become scalar TypeIds or registry entries. Syntax may fail at its
Chapter-18 owner; any accepted syntax reaching type resolution must fail with the exact
Chapter-17 unsupported/type result. Parser acceptance and runtime-library capability never
create a registry entry.

### Contextual NULL, typed NULL, and SQL UNKNOWN

At the binding event boundary, expose an abstract unresolved type marker and prove that it
is neither a SQL scalar type nor a TypeId, cannot be encoded, and is eliminated before an
ordinary resolved expression or executor input is published. Cover every §17.3 context:

| Context | Required observation |
|---|---|
| `CAST(NULL AS T)` for each concrete type | typed NULL of `T` |
| comparison/arithmetic with one compatible concrete operand | selected registered/common numeric type, NULL result at evaluation |
| uniquely determining operator signature | signature type; `NOT NULL` and `NULL AND NULL` become BOOLEAN |
| INSERT/UPDATE/default destination | destination-typed NULL, followed by ordinary constraint checking |
| searched CASE, IN list, or VALUES column with a concrete compatible value | exact common type |
| `NULL IS NULL` / `NULL IS NOT NULL` | non-NULL BOOLEAN TRUE/FALSE without persisted pseudo-type |
| standalone SELECT/VALUES NULL, `NULL=NULL`, `NULL+NULL`, all-NULL CASE, or any nonunique context | bind-time `TYPE_ERROR` |

For each of the seven concrete types, construct typed NULL directly and through every legal
cast edge. Assert retained target type, NULL validity, no source payload inspection or
parse/range error, `IS NULL=TRUE`, `IS NOT NULL=FALSE`, and UNKNOWN for every supported
ordinary comparison. Persisted typed NULL retains its concrete TypeId; no unresolved marker
or separate UNKNOWN encoding exists.

SQL UNKNOWN fixtures always use nullable BOOLEAN NULL. Reject any fourth truth state, third
non-NULL BOOLEAN payload, UNKNOWN TypeId, or unresolved executor value. Keep this vocabulary
in fixture names and diagnostics so bind-time unresolved typing cannot be mistaken for 3VL.

### BOOLEAN, three-valued logic, predicates, and demand

Evaluate the complete literal tables:

| NOT | Result |
|---|---|
| TRUE | FALSE |
| FALSE | TRUE |
| UNKNOWN | UNKNOWN |

| AND | TRUE | FALSE | UNKNOWN |
|---|---|---|---|
| TRUE | TRUE | FALSE | UNKNOWN |
| FALSE | FALSE | FALSE | FALSE |
| UNKNOWN | UNKNOWN | FALSE | UNKNOWN |

| OR | TRUE | FALSE | UNKNOWN |
|---|---|---|---|
| TRUE | TRUE | TRUE | TRUE |
| FALSE | TRUE | FALSE | UNKNOWN |
| UNKNOWN | TRUE | UNKNOWN | UNKNOWN |

Independently record demand: FALSE skips an AND RHS, TRUE skips an OR RHS, and every
other left value demands the RHS before applying the table. Put deterministic errors and
evaluation counters in each RHS. Ordinary comparisons are strict: after demanded children
evaluate, any NULL operand yields UNKNOWN. `IS NULL` and `IS NOT NULL` still demand their
child and never suppress its error; when the child succeeds they return non-NULL BOOLEAN.
Where relational clauses consume predicates, reuse the binder/executor fixtures to prove
that only TRUE qualifies and FALSE/UNKNOWN do not, without host truthiness.

For expression-list IN, evaluate the left expression exactly once and list items
left-to-right. Stop at the first TRUE; with no TRUE return UNKNOWN iff any demanded
comparison was UNKNOWN, otherwise FALSE. NOT IN applies exact 3VL NOT. Cover incompatible
common types, nonempty-list enforcement, early-TRUE suppression of a later error, demanded
errors, duplicate values, typed NULLs, signed zeros, and canonical-equivalent NaNs.

### Literals, integer arithmetic, and operator registry

Build checked token-magnitude fixtures around `2^31` and `2^63`, including leading zeros,
underscores/radix/suffix rejection, and parentheses:

| Form | Required classification |
|---|---|
| `0`, `2147483647` | INT32 |
| `2147483648`, `9223372036854775807` | INT64 |
| direct `-2147483648` | INT32 minimum literal construction |
| direct `-9223372036854775808` | INT64 minimum literal construction |
| positive or parenthesized `9223372036854775808` | `INVALID_LITERAL` before unary evaluation |
| `-(2147483648)` | valid INT64 unary negation |
| unary minus applied to an already formed INT32/INT64 minimum | `NUMERIC_OVERFLOW` |
| magnitude above `2^63`, or direct unary plus at `2^63` | `INVALID_LITERAL` |

Exhaust unary `+`, unary `-`, and NOT over all seven types and NULL states. Exhaust each
binary arithmetic symbol over all 49 type pairs at binding, then evaluate every legal row
of §17.6.2. Integer cases use neighborhoods around minimum, maximum, `-1`, `0`, and `1`,
generated overflow pairs, multiplication square-root boundaries, both mixed INT32/INT64
directions, and NULLs. Assert result type, checked exact value, and no wrap.

Division/remainder fixtures cover all sign combinations, zero numerator, zero divisor,
and `MIN/-1`. Assert truncation toward zero, `a=q*b+r`, `abs(r)<abs(b)`, dividend-sign
remainder, `DIVISION_BY_ZERO` for integer zero divisors, and `NUMERIC_OVERFLOW` for both
`MIN/-1` quotient and remainder. Arithmetic/comparison operators evaluate children
left-to-right and do not use NULL as general child-demand short-circuit: a demanded child
error survives. Once children succeed, strict NULL suppresses only the non-NULL operation.

### Cast, coercion, and comparison verification

Drive every cell of the cast, assignment, arithmetic, and comparison matrices below through
the central TypeResolver. Assert selected common type/edge and inserted casts before
evaluation. Unsupported cells fail at bind time with `TYPE_ERROR`; a registered edge whose
particular value fails uses its value-specific error. Direct casts use their registered
semantics and are not synthesized through an intermediate type.

Numeric conversion uses the independent rounding/range oracle. Include all INT32/INT64
limits, `2^24` and `2^53` neighborhoods, `INT64_MIN/MAX`, finite FLOAT64 values just inside
and outside each integer target after truncation, subnormals, signed zeros, infinities, and
NaNs. INT32→INT64 is exact; integer→FLOAT64 rounds nearest/ties-even even when inexact;
FLOAT64→integer truncates toward zero before range checking; NaN/infinity is
`INVALID_CAST`; finite target-range failure is `NUMERIC_OVERFLOW`.

Mixed numeric comparison first converts both operands to the registered smallest common
type. Include `2^53` neighborhoods where mathematical integer/real comparison differs from
comparison after INT64→FLOAT64 rounding. BOOLEAN equality works but BOOLEAN SQL ordering
does not. VARCHAR, DATE, and TIMESTAMP compare only with their own type. SQL NULL comparison
remains UNKNOWN even though Chapter 8 supplies a total physical NULL order.

Assignment/default coercion is tested separately from explicit cast legality. Only identity,
INT32→INT64, INT32→FLOAT64, INT64→FLOAT64, and contextual typed NULL are automatic for
their permitted destinations. Parsing, stringification, narrowing, temporal conversion,
and BOOLEAN conversion remain explicit-only; destination constraints run after coercion.

### FLOAT64 semantic, arithmetic, text, and cross-engine verification

Partition bit-pattern fixtures into negative/positive finite normals, subnormals, both
zeros, infinities, quiet/signaling NaNs, payload/sign variants, and randomized raw bits.
Every bit pattern is a legal semantic FLOAT64. The semantic comparator normalizes both
zeros together and all NaNs together, orders NaN after positive infinity, and otherwise
orders numerically. Assert this relation in constant evaluation, vector evaluation,
hash/grouping, UNIQUE, and Chapter-8 key encoding. Ordinary tuple round-trip preserves all
64 input bits; PersistedScalarV1 emits only canonical quiet NaN bits; index keys normalize
zeros and NaNs. These are three intentionally different representation contracts.

For `+`, `-`, `*`, `/`, and unary minus, compare exact output bits or canonical NaN class
with the independent one-round-per-operator oracle. Include halfway cases, overflow,
underflow, subnormal transitions, signed-zero signs, `finite/±0`, `±0/±0`, and
`infinity/infinity`. FLOAT64 zero division produces IEEE infinity/NaN and never integer
`DIVISION_BY_ZERO`; FLOAT64 remainder is a bind-time `TYPE_ERROR`. Use expression pairs that
distinguish per-node binary64 rounding from extended intermediates, reassociation, and FMA.

Literal tests accept only §17.5.2's two unsigned token shapes plus syntactic unary sign;
reject `.5`, `1.`, hexadecimal, underscores, suffixes, and NaN/infinity literal keywords.
Finite literal overflow is `INVALID_LITERAL`; underflow rounds, retaining a negative zero
when applicable. Explicit VARCHAR parsing separately accepts its §17.8.2 full-string forms
and canonical special spellings, with `NUMERIC_OVERFLOW` for syntactically valid finite
out-of-range text.

For canonical FLOAT64→VARCHAR, verify signed-zero spellings, NaN/infinity spellings, and
the shortest round-trippable finite rule, candidate-distance tie break, fixed/scientific
length choice, lowercase exponent, and zero suppression. Parse the output with the
independent parser and require exact finite bits, preserved zero sign, or canonical NaN
class. Replay under alternate locale, timezone, and host rounding settings.

### VARCHAR byte semantics and size-boundary composition

Use empty strings, ASCII, embedded and repeated `0x00`, bytes `0x80..0xff`, equal byte
strings, strict prefixes, and fixed-seed arbitrary byte arrays. Assert byte length, exact
equality, unsigned lexicographic order, no UTF-8/normalization/case-folding, and no C-string
termination. Explicitly distinguish empty non-NULL VARCHAR from typed NULL.

String-literal handoff fixtures assert doubled quotes, ordinary backslash, embedded quoted
NUL, and the Chapter-18 lexical rejection of an unquoted source NUL. VARCHAR casts consume
the complete ASCII byte string, accept no surrounding whitespace or trailing bytes, and
use the exact BOOLEAN, integer, FLOAT64, DATE, and TIMESTAMP grammars. Include empty,
Unicode digit/sign, embedded-NUL, valid boundary, malformed, and range-overflow cases with
their exact error categories. Scalar-to-VARCHAR fixtures assert canonical BOOLEAN,
integer, FLOAT64, DATE, and TIMESTAMP output.

Compose semantic VARCHAR validity with storage limits: a valid value may make an ordinary
tuple cross the Chapter-5 complete encoded-length boundary (`8135` accepted, `8136`
rejected) or an encoded user key cross Chapter 8's 1024-byte boundary. Assert the owning
row/key-size result without globally narrowing the VARCHAR domain, truncating bytes, or
changing the semantic Value.

### DATE, TIMESTAMP, calendar, and timezone verification

The semantic-domain generator covers every int32 DATE and int64 TIMESTAMP carrier class,
including minima/maxima and fixed-seed values. Independently verify day zero and microsecond
zero at `1970-01-01 00:00:00`, exact microsecond units, pre-epoch values, and signed order.
All carrier bit patterns remain valid semantic values even when their civil year is outside
the textual range.

Text fixtures cover years `0001..9999`, Gregorian leap rules, month/day limits, fixed-width
fields, mandatory timestamp seconds, optional 1–6 fractional digits and right-padding,
microsecond boundaries, and exact ASCII/full consumption. Reject year 0000/10000, invalid
calendar/time fields, second 60, `T`, timezone suffixes/offsets, whitespace, trailing data,
and more than six fractional digits. DATE/TIMESTAMP→VARCHAR outside the textual civil range
is `INVALID_CAST`; malformed input uses `INVALID_DATE` or `INVALID_TIMESTAMP` as owned.

DATE→TIMESTAMP multiplies by exact microseconds/day and range-checks int64. TIMESTAMP→DATE
uses floor toward the prior midnight and checked int32 range. Mandatory vectors include
`0`, `1us`, `day_us-1`, `day_us`, `-1us`, `-day_us`, and `-day_us-1`, plus both cast overflow
boundaries. Replaying with distinct process timezones must not change any result. Temporal
arithmetic, implicit DATE/TIMESTAMP cross-comparison, timezone-aware types, and typed
DATE/TIMESTAMP literals are rejected where their syntax reaches binding.

### CASE, IN, branch demand, and fold/runtime equivalence

Searched CASE fixtures cover BOOLEAN conditions, unresolved NULL conditions becoming
BOOLEAN UNKNOWN, same-type and promoted-numeric result arms, contextual NULL arms, all-NULL
failure, missing ELSE typed NULL, incompatible arms, and simple-CASE rejection. Record each
condition/result demand in source order: stop at the first TRUE and never evaluate an
unselected result or unnecessary ELSE.

For every registered scalar operator/cast, compare folded and runtime type, NULL state,
semantic value, FLOAT64 bits/class, or error. Constant errors surface during binding only
when the subtree dominates every scalar-control-flow completion path. Include root error,
FALSE-AND error, TRUE-OR error, selected/unselected CASE errors, IN early match/later error,
EXISTS projection/ORDER BY errors, empty relational subquery, and demanded subquery cases.
No estimate, `required_rows`, or possible row production proves demand.

Use checked-integer expression trees where reassociation changes overflow and FLOAT64 trees
where reassociation/contraction changes rounded bits. Folded and runtime evaluation preserve
the bound tree. Aggregate constant evaluation delegates to the Chapter-29 n-ary state and
does not replace it with repeated scalar addition.

### Equality modes, generic Value, and lifetime

Maintain separate expected relations:

| Mode | NULL rule | Non-NULL rule |
|---|---|---|
| ordinary predicate/join | any NULL is nonmatching/UNKNOWN | Chapter-17 equality after bound common type |
| GROUP BY/DISTINCT | all NULLs one class | same normalized equality |
| hash | mode-specific NULL handling | equal values must hash equal; exact bits are not prescribed |
| UNIQUE | NULL-containing key bypasses ordinary admission equality | normalized non-NULL component equality |
| B+ physical order | total NULLS FIRST and BOOLEAN order | Chapter-8 order; SQL-orderable non-NULL values agree |

Exercise every type, composite values, mixed numeric predicate comparisons, both zeros,
multiple NaNs, and exact VARCHAR bytes. Assert the negative cross-mode cases: `NULL=NULL`
is UNKNOWN while grouping NULLs coalesce; BOOLEAN has a physical order but no SQL ordering
overload; mixed numeric comparison does not create a mixed-TypeId persisted key.

A generic Value fixture asserts one resolved logical type for resolved values, or the
explicitly null-only pre-context state permitted by §17.3, plus the corresponding NULL state,
scalar payload, and owned VARCHAR lifetime in non-hot semantic use. Permute its test
implementation and prove no persistent bytes or runtime tag ordinal are inferred from object
layout. Materialize an escaping VARCHAR Value from tuple/page bytes, release the page guard
and recycle source memory, then verify the Value remains valid. The hot vector representation
remains owned by the execution architecture and is not required to use generic Value.

### Ordinary tuple, index, catalog, default, and historical-schema handoff

For each type and representative boundary/NULL value, execute three distinct paths:

```text
semantic value -> Chapter-5 ordinary tuple bytes -> semantic value
semantic value -> §17.13 PersistedScalarV1 bytes -> semantic value
semantic value -> Chapter-8 key bytes -> semantic equality/order property
```

Do not compare bytes across codecs. Tuple NULL uses the Chapter-5 bitmap and canonical
VARCHAR NULL descriptor. BOOLEAN byte defects and malformed VARCHAR descriptors are
`CORRUPT_HEAP`; FLOAT64 tuple NaN/infinity payloads are legal and preserve bits; every
signed DATE/TIMESTAMP carrier is legal. Index properties reuse `IndexKeyCodec and
physical-order properties`, adding SQL-vs-physical NULL and BOOLEAN-order distinctions.

Catalog fixtures map each ColumnDescriptor TypeId through the fixed Chapter-16 registry.
DefaultValueBlob stores exactly one final destination-typed PersistedScalarV1; typed DEFAULT
NULL differs from no default. Decode an old tuple only after resolving its exact
`(TableId,SchemaVer)` descriptor, including a later schema with changed column arrangement.
No current descriptor, runtime tag, or changed type parameter may reinterpret the old
TypeId; the architecture admits no hidden scalar type parameters.

### PersistedScalarV1 exact-byte and validation procedures

Use an independent byte builder. The codec has **no embedded version field**; the enclosing
supported catalog/default/statistics grammar selects `PersistedScalarV1`. Verify exact total size
`Align8(16+payload_length)` and this header:

| Offset | Width | Field | Canonical rule |
|---:|---:|---|---|
| `0` | 4 | TypeId | one of `1..7`, little-endian |
| `4` | 4 | flags | bit 0 is `IS_NULL`; all other bits zero |
| `8` | 4 | payload_length | exact type/NULL length, little-endian |
| `12` | 4 | reserved32 | zero |

Typed NULL for every TypeId uses flags `1`, payload length `0`, no payload, and total size
16. Non-NULL payloads are BOOLEAN 1 byte, INT32/DATE 4 bytes, INT64/FLOAT64/TIMESTAMP
8 bytes, and VARCHAR exact byte length. All alignment padding is zero. Test minima, maxima,
zero, `-1`, both FLOAT64 zeros/infinities, canonical NaN, empty/embedded-NUL/high-byte
VARCHAR, and checked length boundaries. Persisted NaN is exactly
`0x7ff8000000000000`; a noncanonical NaN is invalid metadata even though it is legal in an
ordinary tuple.

From each valid scalar, inject one defect: TypeId 0/8/large, unknown flag bit, nonzero
reserved byte, NULL with payload, wrong fixed width, BOOLEAN byte outside 0/1, noncanonical
NaN, nonzero padding, overflowed/truncated length, truncated VARCHAR, or extra bytes outside
the enclosing scalar extent. Bounds/structure are validated before type-specific decode,
and successful decode returns owned bytes rather than an alias into unvalidated storage.

Apply the same malformed scalar under two owners. Required catalog/default metadata follows
the required corruption policy; a malformed scalar inside an otherwise safely framed
advisory statistics generation invalidates only that generation and selects the prior valid
or missing-statistics fallback. Malformed outer/core catalog framing never receives the
advisory exception.

### Error taxonomy, resource failure, and DML composition

Use the mandatory error matrix below. In particular, unsupported structure is
`TYPE_ERROR`; malformed/out-of-domain source literal is `INVALID_LITERAL`; a registered
cast with no target value is `INVALID_CAST`; checked range failure is
`NUMERIC_OVERFLOW`; integer zero division/remainder is `DIVISION_BY_ZERO`; textual calendar
errors are `INVALID_DATE`/`INVALID_TIMESTAMP`. None silently becomes NULL. Persisted defects
are corruption or advisory invalidation, never user cast errors.

Inject allocation failure while acquiring owned VARCHAR bytes and while parse/format
scratch state is live. Expect the §39 query resource category, complete cleanup, and no
NULL substitution or semantic-error relabeling. Feed the same row-dependent scalar error
through DML before and after the first published mutation; Chapter 17 supplies the scalar
error while Chapter 15/§39.1 supplies ACTIVE versus MUST_ABORT/abort consequences. No type
test invents transaction policy.

### Unsupported registry and platform-determinism sweeps

Enumerate every absent type, cast cell, arithmetic/comparison cell, unary overload,
predicate, and scalar function. Representative parser-accepted absent forms must reach the
closed registry and fail deterministically; parser-rejected syntax remains a Chapter-18
case. Include BOOLEAN ordering, FLOAT64 remainder, VARCHAR arithmetic/implicit coercion,
DATE/TIMESTAMP arithmetic and implicit cross-comparison, unsupported predicates, and named
scalar functions. Aggregate names remain Chapter-29 entries, not scalar functions.

Replay semantic and byte corpora with changed process locale/timezone, high-bit VARCHAR
bytes, opposite-endian oracle construction, altered host rounding mode where safely
isolated, and builds/configurations capable of exposing excess precision or contraction.
Results must remain identical. The methodology constrains observable results, demand,
errors, bytes, and lifetimes—not a C++ enum, Value layout, parser data structure,
TypeResolver dispatch table, vector representation, or floating-point implementation
strategy.

### Mandatory scalar matrices

These matrices are test inventories. Each cell is exercised at binding and, when legal,
with positive, NULL, boundary, and value-error evaluation fixtures.

#### Type matrix

| Type | TypeId | Non-NULL domain | NULL | SQL equality/order | Cast/operator highlights | Tuple / index / metadatan owner |
|---|---:|---|---|---|---|---|
| BOOLEAN | 1 | FALSE, TRUE | yes | `=`/`<>`; no SQL order | NOT/AND/OR; explicit VARCHAR only | Ch. 5 / Ch. 8 / §17.13 |
| INT32 | 2 | signed 32-bit | yes | signed; mixed numeric promotion | checked arithmetic; implicit INT64/FLOAT64 | Ch. 5 / Ch. 8 / §17.13 |
| INT64 | 3 | signed 64-bit | yes | signed; mixed numeric promotion | checked arithmetic; implicit FLOAT64; explicit INT32 | Ch. 5 / Ch. 8 / §17.13 |
| FLOAT64 | 4 | every binary64 pattern | yes | normalized total equality/order | IEEE scalar arithmetic; explicit integer casts | Ch. 5 / Ch. 8 / §17.13 |
| DATE | 5 | signed int32 day count | yes | same-type signed order | explicit VARCHAR/TIMESTAMP; no arithmetic | Ch. 5 / Ch. 8 / §17.13 |
| TIMESTAMP | 6 | signed int64 microseconds | yes | same-type signed order | explicit VARCHAR/DATE; no arithmetic | Ch. 5 / Ch. 8 / §17.13 |
| VARCHAR | 7 | arbitrary finite bytes within owner resources | yes | exact bytes/unsigned lexicographic | explicit parse casts; no arithmetic | Ch. 5 / Ch. 8 / §17.13 |

#### Cast matrix

`ID` is identity, `IM` registered widening usable only in registered automatic contexts,
`EX` explicit-only, and `NO` forbidden.

| Source \ Target | BOOL | I32 | I64 | F64 | VARCHAR | DATE | TS |
|---|---:|---:|---:|---:|---:|---:|---:|
| BOOL | ID | NO | NO | NO | EX | NO | NO |
| I32 | NO | ID | IM | IM | EX | NO | NO |
| I64 | NO | EX | ID | IM | EX | NO | NO |
| F64 | NO | EX | EX | ID | EX | NO | NO |
| VARCHAR | EX | EX | EX | EX | ID | EX | EX |
| DATE | NO | NO | NO | NO | EX | ID | EX |
| TS | NO | NO | NO | NO | EX | EX | ID |

Every legal edge propagates NULL to target-typed NULL. Numeric edges use exact/range/rounding
rules; VARCHAR source edges use full-string ASCII grammars; scalar→VARCHAR uses canonical
formatting; temporal cross-casts use checked epoch arithmetic. NO is bind-time `TYPE_ERROR`.

#### Assignment/default coercion matrix

| Source | Legal automatic destinations |
|---|---|
| BOOLEAN | BOOLEAN |
| INT32 | INT32, INT64, FLOAT64 |
| INT64 | INT64, FLOAT64 |
| FLOAT64 | FLOAT64 |
| VARCHAR | VARCHAR |
| DATE | DATE |
| TIMESTAMP | TIMESTAMP |
| untyped NULL | any concrete destination, then constraints |

#### Comparison matrix

| Left \ Right | BOOL | I32 | I64 | F64 | VARCHAR | DATE | TS |
|---|---|---|---|---|---|---|---|
| BOOL | EQ | NO | NO | NO | NO | NO | NO |
| I32 | NO | EO:I32 | EO:I64 | EO:F64 | NO | NO | NO |
| I64 | NO | EO:I64 | EO:I64 | EO:F64 | NO | NO | NO |
| F64 | NO | EO:F64 | EO:F64 | EO:F64 | NO | NO | NO |
| VARCHAR | NO | NO | NO | NO | EO:bytes | NO | NO |
| DATE | NO | NO | NO | NO | NO | EO:day | NO |
| TS | NO | NO | NO | NO | NO | NO | EO:microsecond |

`EQ` means `=`/`<>` only; `EO` means equality and ordering after the named common type.
Every legal ordinary comparison with either operand NULL returns UNKNOWN; `NO` is bind-time
`TYPE_ERROR`.

#### Arithmetic matrix

| Operator | Legal inputs | Result | NULL | Error/special rule |
|---|---|---|---|---|
| unary `+` | I32, I64, F64 | same type | strict | identity; signed zero preserved |
| unary `-` | I32, I64 | same type | strict | MIN overflow except direct literal construction |
| unary `-` | F64 | F64 | strict | sign flip; canonical NaN result |
| `+ - * / %` | I32/I32 | I32 | strict | checked; zero divisor and MIN/-1 rules |
| `+ - * / %` | I32/I64 either order, I64/I64 | I64 | strict | checked; zero divisor and MIN/-1 rules |
| `+ - * /` | any numeric pair containing F64 | F64 | strict | one binary64 rounding/operator; IEEE division |
| `%` | any pair containing F64 | unsupported | N/A | bind-time `TYPE_ERROR` |
| arithmetic | BOOLEAN/VARCHAR/DATE/TS involvement | unsupported | N/A | bind-time `TYPE_ERROR` |

#### NULL and three-valued matrix

| Construct | Required result/demand |
|---|---|
| NOT TRUE/FALSE/UNKNOWN | FALSE/TRUE/UNKNOWN |
| AND 3×3 | exact table above; FALSE skips RHS only |
| OR 3×3 | exact table above; TRUE skips RHS only |
| legal ordinary comparison with NULL | UNKNOWN after demanded children |
| `IS NULL` / `IS NOT NULL` | non-NULL BOOLEAN; child still demanded |
| WHERE/HAVING/ON | only TRUE qualifies |
| IN | first TRUE wins; else UNKNOWN if observed; else FALSE |
| NOT IN | 3VL NOT of IN |

#### FLOAT64 matrix

| Class | Legal | Equality/order class | Tuple | Index | PersistedScalarV1 | Integer cast / text |
|---|---|---|---|---|---|---|
| negative/positive finite | yes | numeric | preserve bits | total-order encoding | preserve bits | truncate/range; shortest round-trip |
| negative/positive subnormal | yes | numeric | preserve bits | total-order encoding | preserve bits | usually zero after truncation; shortest round-trip |
| `-0` / `+0` | yes | one equality/order class | preserve sign | one normalized class | preserve sign | integer 0; `-0` / `0` |
| `-Inf` / `+Inf` | yes | numeric endpoints below NaN | preserve bits | ordered | preserve bits | `INVALID_CAST`; canonical spelling |
| quiet/signaling/payload NaNs | yes | one class above +Inf | preserve all bits | canonical class | canonical quiet NaN only | `INVALID_CAST`; `NaN` |

#### VARCHAR matrix

| Fixture | Semantic validity/length | Equality/order | Tuple/index | Result boundary |
|---|---|---|---|---|
| empty non-NULL | valid, 0 bytes | distinct from NULL; prefix minimum | present empty descriptor / terminator | success |
| ASCII/equal bytes | valid, byte length | exact equality | exact payload / escaped key | success |
| embedded `00` | valid, includes byte | unsigned-byte comparison | length-delimited / `00 FF` | no truncation |
| bytes `80..ff` | valid | unsigned order | exact / memcomparable | signed-char independent |
| strict prefix pair | valid | shorter first | distinct encodings | success |
| tuple exceeds inline limit | semantic value valid | unchanged | tuple operation rejects | owning row-size error |
| key encoding >1024 | semantic value valid | unchanged | index operation rejects | oversized-key result |
| `VARCHAR(n)` | not a type | N/A | no encoding | syntax/type rejection |

#### Temporal matrix

| Fixture | Semantic value | Text validity | Cast/result | Storage/order |
|---|---|---|---|---|
| carrier zero | epoch DATE/TIMESTAMP | valid epoch text | exact | signed carrier |
| negative/pre-epoch | valid carrier | valid only if civil year in range | TIMESTAMP→DATE floors | signed carrier |
| int32/int64 extrema | valid semantic carrier | generally outside text range | formatting `INVALID_CAST`; cross-cast may overflow | valid persisted/order |
| years 0001/9999 | valid | accepted exact width | exact round-trip | signed carrier |
| leap day / century cases | calendar-dependent | Gregorian oracle | accepted/rejected exactly | N/A |
| invalid date/time/second 60 | no text value | rejected | `INVALID_DATE`/`INVALID_TIMESTAMP` | N/A |
| fractions absent/1..6/>6 | right-pad accepted forms | >6 rejected | exact microseconds/error | N/A |
| timezone suffix/offset | timezone-aware text outside the defined architecture | rejected | `INVALID_TIMESTAMP` | no conversion |
| DATE→TS / TS→DATE | valid when target range fits | N/A | checked multiply / prior-midnight floor | target signed carrier |

#### Literal matrix

| Literal family | Handoff/classification | Boundary/error |
|---|---|---|
| integer token | checked magnitude; smallest signed type | direct-negative minima special; >`2^63` invalid |
| FLOAT64 token | exact §17.5.2 ASCII forms | finite overflow `INVALID_LITERAL`; rounded underflow |
| BOOLEAN | TRUE/FALSE keywords | numeric/string forms do not become BOOLEAN literals |
| VARCHAR | decoded length-delimited bytes | doubled quote; backslash ordinary; source-NUL boundary owned by lexer |
| DATE/TIMESTAMP | no typed literal syntax | construct by explicit VARCHAR cast |
| NULL | unresolved type marker during binding | contextual typed NULL or `TYPE_ERROR` |

#### Type/storage-handoff matrix

| Type | Semantic owner | TypeId owner | Tuple/null owner | Metadatan owner | Index owner | Historical/validation owner |
|---|---|---|---|---|---|---|
| BOOLEAN | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| INT32 | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| INT64 | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| FLOAT64 | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| DATE | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| TIMESTAMP | Ch. 17 | §16.4 | §§5.10–5.11 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |
| VARCHAR | Ch. 17 | §16.4 | §§5.10, 5.12 | §17.13 | §8.5 | §§5.13, 16.7 / Ch. 5 |

#### PersistedScalarV1 byte matrix

| Region/type | Offset/width or extent | Positive fixture | Single defect | Required / advisory outcome |
|---|---|---|---|---|
| codec selection | enclosing grammar; no embedded field | supported enclosing v1 | unsupported/malformed envelope | owner dispatch; never guess scalar version |
| TypeId | 0/4 | each 1..7 | 0, 8, large | corruption / generation invalid |
| flags | 4/4 | 0 or `IS_NULL` | unknown bit | corruption / generation invalid |
| length | 8/4 | exact payload | wrong/overflow/truncated | corruption / generation invalid |
| reserved32 | 12/4 | zero | one nonzero bit/byte | corruption / generation invalid |
| typed NULL | total 16 | each concrete TypeId, flags 1, len 0 | payload/nonzero length | corruption / generation invalid |
| BOOLEAN | len 1, total 24 | 00/01 plus zero pad | other byte/nonzero pad | corruption / generation invalid |
| INT32/DATE | len 4, total 24 | boundary LE bits | wrong length/nonzero pad | corruption / generation invalid |
| INT64/TS | len 8, total 24 | boundary LE bits | wrong length | corruption / generation invalid |
| FLOAT64 | len 8, total 24 | finite, zeros, infinities, canonical NaN | noncanonical NaN/wrong length | corruption / generation invalid |
| VARCHAR | len N, total `Align8(16+N)` | empty/NUL/high bytes | truncation/extra extent/nonzero pad | corruption / generation invalid |

#### Error matrix

| Condition | Class | NULL? | State/fallback owner | Oracle |
|---|---|---|---|---|
| unsupported type/operator/cast/function; underconstrained NULL | `TYPE_ERROR` | no | binder / §39.1 if in statement | closed registry |
| malformed/out-of-domain source literal | `INVALID_LITERAL` | no | binder/folder | literal grammar |
| registered cast value has no target | `INVALID_CAST` | no | expression / §39.1 | direct cast model |
| checked arithmetic/narrowing/range failure | `NUMERIC_OVERFLOW` | no | expression / §39.1 | widened/range model |
| integer `/` or `%` zero divisor | `DIVISION_BY_ZERO` | no | expression / §39.1 | quotient model |
| invalid DATE/TIMESTAMP text | `INVALID_DATE` / `INVALID_TIMESTAMP` | no | expression / §39.1 | calendar model |
| malformed ordinary tuple scalar | `CORRUPT_HEAP` | no | Ch. 5/§39 | byte/layout oracle |
| malformed required PersistedScalarV1 | required corruption | no | §§4.14.6, 16, 39 | byte oracle |
| malformed advisory statistics scalar | generation invalid/fallback | no | §34.14 | byte + envelope oracle |
| owned-byte/scratch allocation failure | resource/OutOfMemory | no | §39.3 then §39.1 | injected resource boundary |

#### Demand and error-dominance matrix

| Expression | Demanded | Skipped | Result/error | Fold/runtime |
|---|---|---|---|---|
| `FALSE AND error` | left | RHS | FALSE | equal |
| `TRUE AND error` | both | none | RHS error | equal |
| `TRUE OR error` | left | RHS | TRUE | equal |
| `FALSE OR error` | both | none | RHS error | equal |
| `UNKNOWN AND FALSE` | both | none | FALSE | equal |
| `UNKNOWN OR TRUE` | both | none | TRUE | equal |
| selected CASE safe, later error arm | conditions through first TRUE and selected arm | later arms/ELSE | selected value | equal |
| selected CASE error | selected path | later paths | selected error | equal |
| IN early TRUE, later error | left/items through TRUE | later items | TRUE | equal |
| IN no TRUE, observed UNKNOWN | left/all items | none | UNKNOWN unless demanded error | equal |
| NOT IN | same IN demand | same | NOT of IN | equal |
| `NULL + erroring child` | both children | none | child error | equal |
| `NULL / 0` with both child values successful | both children | non-NULL division operation | typed NULL | equal |
| `IS NULL(error)` | child | none | child error | equal |

#### Cross-chapter composition matrix

| Owner | Chapter-17 composition | Verification procedure |
|---|---|---|
| Ch. 4 | checked domains, format/corruption policy | widened arithmetic and required/advisory classification |
| Ch. 5 | tuple NULL/scalar/VARCHAR/schema bytes | ordinary tuple handoff and one-defect codec fixtures |
| Ch. 8 | scalar key bytes and total order | independent semantic sort versus encoded-byte sort |
| Ch. 15 | expression failure before/after publication | same semantic error at both DML boundaries |
| Ch. 16 | TypeIds, descriptors, defaults, history | registry/default/historical-schema fixtures |
| Ch. 17 | scalar semantics and PersistedScalarV1 | this complete section |
| Ch. 18 | token/string/numeric handoff | parser tests plus literal semantic fixtures |
| Ch. 20 | subquery demand/IN | demand matrix plus Subquery Tests |
| Ch. 21 | folded-default envelope and assignment handoff | exact DefaultValueBlob plus nested PersistedScalarV1 fixture |
| Ch. 29 | aggregate registry/n-ary FLOAT semantics | Aggregate Tests; no scalar-function conflation |
| Chs. 34/35 | scalar equality/proof and advisory fallback | statistics scalar and semantic-proof fixtures |
| §39 | scalar/resource errors and transaction consequence | error matrix plus DML boundary composition |
| §41 | durable verification obligations | this section plus referenced lower-layer owners |

#### High-level domain/case matrix

| Case | Deterministic fixture | Independent oracle | Owner |
|---|---|---|---|
| TypeIds 1..7 / unknown | literal and descriptor carriers | fixed map | §§16.4, 17.2 |
| resolvable / underconstrained NULL | every §17.3 context | NULL constraint model | §17.3 |
| SQL UNKNOWN / complete 3VL | literal matrices | truth tables | §17.7 |
| integer minima and MIN/-1 | boundary expressions | widened integer model | §§17.5–17.6 |
| integer→FLOAT precision | `2^53` neighborhoods | exact rounding model | §17.8.1 |
| FLOAT NaN/zeros/order | raw-bit class corpus | normalized total-order model | §§17.4.3, 8.5.5 |
| VARCHAR arbitrary bytes | NUL/high-byte/prefix corpus | unsigned byte model | §17.4.6 |
| pre-epoch timestamp→date | `-1us` and day edges | floor/calendar model | §17.8.4 |
| legal/forbidden/NULL casts | full 7×7 matrix | direct cast table | §17.8 |
| PersistedScalar typed NULL/NaN | exact byte fixtures | independent codec | §17.13 |
| required/advisory malformed scalar | same one-byte defect in two envelopes | ownership classifier | §§17.13, 34.14 |
| skipped fold error | AND/OR/CASE/IN/subquery cases | demand interpreter | §§17.7.3, 17.10.2 |
| reassociation/contraction | overflow and halfway trees | bound-tree evaluator | §17.10.2 |
| tuple/index round-trip/order | all types/boundaries | Ch. 5/8 independent models | §§17.1, 17.10.3 |
| historical decode | old tuple plus changed descriptor | `(TableId,SchemaVer)` map | §§16.7, 17.10.3 |

### Chapter 17 atomic architecture-obligation coverage map

`Type-System Property Tests` means the procedures and matrices in this section. Referenced
lower-layer sections provide reusable byte, lifecycle, execution, or catalog mechanisms;
the listed procedure supplies the Chapter-17 semantic composition. Matrix cells are all
executed, but the map counts the owning architectural rule rather than multiplying one rule
by fixture count. Every row is required.

Domain codes are:

```text
A scope/ownership                 B TypeId mapping             C closed registry
D unresolved marker              E typed NULL                 F SQL UNKNOWN
G BOOLEAN                         H three-valued logic          I INT32
J INT64                           K FLOAT64                     L DATE
M TIMESTAMP                       N VARCHAR                     O general literal classification
P integer literal                 Q FLOAT literal               R VARCHAR/BOOLEAN literal
S temporal text                   T unary/strict evaluation     U binary arithmetic
V comparison                      W NULL predicates             X evaluation order
Y short-circuit/demand            Z explicit casts              AA implicit casts
AB assignment coercion            AC numeric casts              AD VARCHAR parsing
AE VARCHAR formatting             AF temporal casts             AG CASE
AH IN/NOT IN                      AI scalar functions           AJ TypeResolver
AK constant folding               AL equality modes             AM hash/group/UNIQUE/index
AN forbidden implementations     AO generic Value              AP Value lifetime
AQ scalar header                  AR scalar NULL                AS scalar payloads
AT scalar validation              AU tuple handoff              AV index handoff
AW catalog/default handoff        AX historical schema          AY error taxonomy
AZ resource failure               BA platform determinism       BB unsupported scope
BC cross-owner invariants
```

| ID | Domain | Atomic obligation | Architecture owner | Verification procedure |
|---:|---|---|---|---|
| 1 | A | Chapter 17 owns logical scalar meaning | §17.1 | ownership/handoff matrix |
| 2 | A | Chapter 5 remains tuple-byte owner | §17.1 | ordinary tuple handoff |
| 3 | A | Chapter 8 remains index-byte/order owner | §17.1 | index-order differential |
| 4 | A | SQL semantics compose with both physical owners | §17.1 | cross-chapter matrix |
| 5 | B | TypeId 1 means BOOLEAN | §§16.4, 17.2 | fixed TypeId oracle |
| 6 | B | TypeId 2 means INT32 | §§16.4, 17.2 | fixed TypeId oracle |
| 7 | B | TypeId 3 means INT64 | §§16.4, 17.2 | fixed TypeId oracle |
| 8 | B | TypeId 4 means FLOAT64 | §§16.4, 17.2 | fixed TypeId oracle |
| 9 | B | TypeId 5 means DATE | §§16.4, 17.2 | fixed TypeId oracle |
| 10 | B | TypeId 6 means TIMESTAMP | §§16.4, 17.2 | fixed TypeId oracle |
| 11 | B | TypeId 7 means VARCHAR | §§16.4, 17.2 | fixed TypeId oracle |
| 12 | B | TypeId zero is invalid/not stored | §§16.4, 17.2 | zero descriptor/scalar fixtures |
| 13 | B | Unknown persistent TypeIds are not runtime opaque values | §§17.2, 17.13.4 | unknown-TypeId byte fixtures |
| 14 | B | Runtime tag ordinal need not equal TypeId | §§16.4, 17.2 | permuted runtime-tag adapter |
| 15 | C | The scalar registry contains exactly seven types | §§17.2, 17.4 | literal closed-set oracle |
| 16 | C | NULL is not a scalar TypeId | §§17.2–17.3 | registry/descriptor negatives |
| 17 | C | SQL UNKNOWN is not a scalar TypeId | §§17.2–17.3 | registry/descriptor negatives |
| 18 | C | Unresolved type marker is not a scalar TypeId | §§17.2–17.3 | binder/persistence negatives |
| 19 | C | DECIMAL/BLOB/UUID/INTERVAL are unregistered | §17.2 | unsupported-type sweep |
| 20 | C | JSON/arrays/timezone-aware timestamp are unregistered | §17.2 | unsupported-type sweep |
| 21 | C | the architecture has no hidden type parameters or VARCHAR(n) | §§17.2, 17.4.6 | parameter/type negatives |
| 22 | C | Parser capacity cannot open scalar registry | §17.4 | parser-to-resolver negative |
| 23 | D | Untyped NULL may carry unresolved type marker during binding | §17.3 | binding event trace |
| 24 | D | Marker is binder-only | §§17.3, 17.12 | phase-boundary assertion |
| 25 | D | Marker is not a SQL scalar type | §17.3 | semantic registry negative |
| 26 | D | Marker is not persisted | §17.3 | all-codec rejection |
| 27 | D | Marker is not an executor vector element | §17.3 | resolved-plan validation |
| 28 | D | Successful binding eliminates marker | §17.3 | bind-to-execution trace |
| 29 | D | `CAST(NULL AS T)` resolves to T | §17.3 | seven-target context fixtures |
| 30 | D | Compatible comparison/arithmetic context resolves NULL | §17.3 | context matrix |
| 31 | D | Unique operator signature resolves untyped positions | §17.3 | NOT/AND context fixtures |
| 32 | D | Assignment/default context resolves destination type | §17.3 | assignment context fixtures |
| 33 | D | CASE/IN/VALUES concrete context resolves common type | §17.3 | common-type fixtures |
| 34 | D | NULL predicates need no persisted pseudo-type | §17.3 | folded predicate fixtures |
| 35 | D | Every underconstrained listed context is TYPE_ERROR | §17.3 | underconstrained table sweep |
| 36 | D | No fallback VARCHAR/INT32 type is guessed | §17.3 | fallback-negative assertion |
| 37 | E | Every concrete type admits typed NULL value state | §§17.2–17.3 | seven-type NULL matrix |
| 38 | E | Typed NULL retains concrete logical type | §§17.3, 17.13.2 | semantic/byte assertions |
| 39 | E | Legal cast of NULL yields target-typed NULL | §17.8 | every legal cast edge |
| 40 | E | NULL cast performs no parse/range operation | §17.8 | poisoned-payload fixtures |
| 41 | E | Typed NULL is distinct from absent default | §§16.5.3, 17.3 | default handoff fixture |
| 42 | F | SQL UNKNOWN is nullable BOOLEAN NULL | §§17.3, 17.7.2 | truth-state representation fixture |
| 43 | F | UNKNOWN has no independent payload/state | §17.7.2 | fourth-state negative |
| 44 | F | Binder marker terminology remains distinct from UNKNOWN | §§17.3, 17.7.2 | diagnostics/event vocabulary assertion |
| 45 | G | BOOLEAN non-NULL domain is exactly FALSE/TRUE | §§17.2, 17.4.1 | domain and persisted-byte fixtures |
| 46 | G | BOOLEAN supports nullable NULL state | §17.4.1 | typed-NULL fixture |
| 47 | G | BOOLEAN has no integer truthiness | §17.4.1 | binder negatives |
| 48 | G | BOOLEAN has no numeric cast | §§17.4.1, 17.8 | cast-matrix negatives |
| 49 | G | BOOLEAN SQL comparison is equality-only | §§17.4.1, 17.7.1 | comparison matrix |
| 50 | G | Physical BOOLEAN order does not authorize SQL order | §§17.4.1, 17.10.3 | SQL/index contrast fixture |
| 51 | H | NOT TRUE is FALSE | §17.7.2 | literal 3VL oracle |
| 52 | H | NOT FALSE is TRUE | §17.7.2 | literal 3VL oracle |
| 53 | H | NOT UNKNOWN is UNKNOWN | §17.7.2 | literal 3VL oracle |
| 54 | H | Complete AND 3x3 table is exact | §17.7.2 | exhaustive 3VL matrix |
| 55 | H | Complete OR 3x3 table is exact | §17.7.2 | exhaustive 3VL matrix |
| 56 | H | Non-BOOLEAN predicates are rejected | §§17.7.2, 17.12 | binder predicate sweep |
| 57 | H | WHERE consumes only TRUE | §17.7.2 | relational predicate fixture |
| 58 | H | HAVING consumes only TRUE | §17.7.2 | aggregate predicate fixture |
| 59 | H | JOIN ON consumes only TRUE | §17.7.2 | join predicate fixture |
| 60 | H | XOR is unsupported | §17.7.2 | operator negative |
| 61 | P | Unsuffixed integer token is ASCII DIGIT+ | §17.5.1 | independent token grammar |
| 62 | P | Leading zeros do not imply octal | §17.5.1 | leading-zero vectors |
| 63 | P | Radix prefixes/underscores/suffixes are unsupported | §17.5.1 | literal negatives |
| 64 | P | Magnitudes through INT32_MAX bind INT32 | §17.5.1 | boundary classification |
| 65 | P | INT32_MAX+1 through INT64_MAX bind INT64 | §17.5.1 | boundary classification |
| 66 | P | Positive magnitude 2^63 is direct-minus-only | §17.5.1 | syntax-shape fixtures |
| 67 | P | Magnitude above 2^63 is INVALID_LITERAL | §17.5.1 | checked magnitude oracle |
| 68 | P | Direct negative through 2^31 chooses INT32 | §17.5.1 | direct-negative vectors |
| 69 | P | Larger direct negative through 2^63 chooses INT64 | §17.5.1 | direct-negative vectors |
| 70 | P | Direct `-2^63` constructs INT64_MIN | §17.5.1 | special-edge fixture |
| 71 | P | Parentheses break direct-negative special form | §17.5.1 | AST-shape/literal fixture |
| 72 | P | Direct unary plus receives no special range | §17.5.1 | positive-edge negative |
| 73 | I | INT32 domain is exact signed 32-bit | §§17.2, 17.4.2 | widened-domain fixtures |
| 74 | I | INT32 arithmetic is checked mathematical arithmetic | §17.4.2 | widened integer oracle |
| 75 | I | INT32 unary minimum negation overflows | §17.6.1 | minimum fixture |
| 76 | I | INT32 operations never use host wrap | §§17.4.2, 17.10.4 | boundary/property sweep |
| 77 | I | INT32+INT32 result is INT32 | §17.6.2 | arithmetic matrix |
| 78 | I | INT32 integer division truncates toward zero | §17.6.2 | quotient oracle |
| 79 | I | INT32 remainder follows dividend sign | §17.6.2 | remainder oracle |
| 80 | J | INT64 domain is exact signed 64-bit | §§17.2, 17.4.2 | widened-domain fixtures |
| 81 | J | INT64 arithmetic is checked mathematical arithmetic | §17.4.2 | widened integer oracle |
| 82 | J | INT64 unary minimum negation overflows | §17.6.1 | minimum fixture |
| 83 | J | INT64 operations never use host wrap | §§17.4.2, 17.10.4 | boundary/property sweep |
| 84 | J | Mixed INT32/INT64 arithmetic result is INT64 | §17.6.2 | both-order matrix fixtures |
| 85 | J | INT64 division truncates toward zero | §17.6.2 | quotient oracle |
| 86 | J | INT64 remainder follows dividend sign | §17.6.2 | remainder oracle |
| 87 | U | Integer addition checks overflow | §17.6.2 | min/max pair sweep |
| 88 | U | Integer subtraction checks overflow | §17.6.2 | min/max pair sweep |
| 89 | U | Integer multiplication checks overflow | §17.6.2 | widened product sweep |
| 90 | U | Integer zero divisor is DIVISION_BY_ZERO | §17.6.2 | zero-divisor matrix |
| 91 | U | Integer MIN/-1 division is NUMERIC_OVERFLOW | §17.6.2 | minimum fixture |
| 92 | U | Integer MIN%-1 is NUMERIC_OVERFLOW | §17.6.2 | minimum fixture |
| 93 | U | Remainder satisfies `a=q*b+r` | §17.6.2 | arithmetic identity oracle |
| 94 | U | Remainder magnitude is below divisor magnitude | §17.6.2 | arithmetic identity oracle |
| 95 | U | Strict integer NULL returns resolved typed NULL | §§17.6, 17.12 | NULL/type fixtures |
| 96 | K | Every binary64 bit pattern is a legal FLOAT64 value | §§17.2, 17.4.3 | raw-bit class/property corpus |
| 97 | K | FLOAT64 arithmetic uses binary64 | §17.4.3 | software rounding oracle |
| 98 | K | Each scalar operator rounds nearest/ties-even | §17.4.3 | halfway vectors |
| 99 | K | Extended-precision result differences are forbidden | §17.4.3 | excess-precision discriminators |
| 100 | K | Altered process rounding cannot change SQL result | §17.4.3 | environment replay/software oracle |
| 101 | K | Observable FMA/contraction differences are forbidden | §17.4.3 | contraction discriminators |
| 102 | K | Arithmetic NaN results normalize canonically | §17.4.3 | NaN-producing operation corpus |
| 103 | K | FLOAT64 arithmetic preserves signed zero | §17.4.3 | zero-sign vectors |
| 104 | K | FLOAT overflow produces signed infinity | §17.4.3 | maximum finite vectors |
| 105 | K | FLOAT underflow produces rounded subnormal/signed zero | §17.4.3 | underflow boundary vectors |
| 106 | K | FLOAT arithmetic raises no floating trap semantics | §17.4.3 | controlled environment fixture |
| 107 | K | `-0` and `+0` compare equal | §§17.4.3, 17.7.1 | comparator matrix |
| 108 | K | All NaNs compare equal | §§17.4.3, 17.7.1 | payload/sign corpus |
| 109 | K | NaN sorts after +infinity | §§17.4.3, 17.7.1 | total-order corpus |
| 110 | K | Non-NaN FLOAT order is numeric | §§17.4.3, 17.7.1 | semantic comparator oracle |
| 111 | K | Raw IEEE unordered predicates are not SQL comparator | §§17.7.1, 17.10.4 | host-comparator negative |
| 112 | K | FLOAT scalar arithmetic differs from aggregate n-ary semantics | §§17.4.3, 29.3 | scalar/aggregate contrast |
| 113 | K | FLOAT zero division follows IEEE outcomes | §§17.6.2, 39.3.2 | division class matrix |
| 114 | K | FLOAT remainder is unsupported | §17.6.2 | binder negative |
| 115 | K | FLOAT unary plus preserves semantic value and signed zero | §17.6.1 | unary fixtures |
| 116 | K | FLOAT unary minus flips sign and canonicalizes NaN result | §17.6.1 | unary bit fixtures |
| 117 | K | INT/FLOAT mixed arithmetic result is FLOAT64 | §17.6.2 | pair matrix |
| 118 | K | FLOAT folding equals runtime bits/classes | §17.10.2 | fold/runtime differential |
| 119 | L | DATE semantic domain is every signed int32 day count | §§17.2, 17.4.4 | carrier-domain generator |
| 120 | L | DATE epoch day zero is 1970-01-01 | §17.4.4 | civil-day oracle |
| 121 | L | DATE uses proleptic Gregorian calendar | §17.4.4 | independent calendar model |
| 122 | L | DATE signed order follows day count | §§17.2, 17.7.1 | comparison/order fixture |
| 123 | L | DATE text range is narrower than semantic domain | §17.4.4 | carrier/text contrast |
| 124 | L | Out-of-text-range DATE formatting is INVALID_CAST | §§17.4.4, 17.8.3 | boundary formatting fixture |
| 125 | M | TIMESTAMP semantic domain is every signed int64 microsecond count | §§17.2, 17.4.5 | carrier-domain generator |
| 126 | M | TIMESTAMP epoch zero is exact | §17.4.5 | epoch oracle |
| 127 | M | TIMESTAMP unit is microseconds | §17.4.5 | adjacent-unit fixtures |
| 128 | M | TIMESTAMP is timezone-naive | §17.4.5 | timezone replay |
| 129 | M | Leap seconds are outside the defined architecture | §17.4.5 | second-60 rejection |
| 130 | M | TIMESTAMP signed order follows microsecond count | §§17.2, 17.7.1 | comparison/order fixture |
| 131 | M | TIMESTAMP text range is narrower than semantic domain | §17.4.5 | carrier/text contrast |
| 132 | M | Out-of-text-range TIMESTAMP formatting is INVALID_CAST | §§17.4.5, 17.8.3 | boundary formatting fixture |
| 133 | N | VARCHAR semantic values are arbitrary finite byte strings | §§17.2, 17.4.6 | byte-array generator |
| 134 | N | VARCHAR length is byte length | §17.4.6 | NUL/high-byte length fixtures |
| 135 | N | VARCHAR equality is exact bytes | §§17.2, 17.7.1 | byte comparator oracle |
| 136 | N | VARCHAR order is unsigned lexicographic | §§17.2, 17.7.1 | unsigned-byte oracle |
| 137 | N | Equal-prefix shorter VARCHAR sorts first | §17.7.1 | prefix corpus |
| 138 | N | VARCHAR requires no UTF-8 | §17.4.6 | invalid-UTF8 byte corpus |
| 139 | N | VARCHAR requires no normalization/collation locale | §17.4.6 | locale/canonical-byte replay |
| 140 | N | Embedded NUL is ordinary data | §§17.4.6, 17.5.3 | NUL round-trip/order fixture |
| 141 | N | Empty VARCHAR is non-NULL length zero | §§5.12.3, 17.4.6 | NULL/empty contrast |
| 142 | N | Semantic VARCHAR domain is distinct from tuple limit | §§17.2, 17.4.6 | oversized-row composition |
| 143 | N | Semantic VARCHAR domain is distinct from key limit | §§17.2, 8.6 | oversized-key composition |
| 144 | Q | FLOAT literal grammar has exactly two unsigned token forms | §17.5.2 | independent grammar sweep |
| 145 | Q | FLOAT literal leading sign is unary syntax | §17.5.2 | AST/token handoff fixture |
| 146 | Q | `.5`, `1.`, hex, underscore, suffix are invalid | §17.5.2 | negative grammar corpus |
| 147 | Q | NaN/infinity keywords are not FLOAT literals | §17.5.2 | literal negative |
| 148 | Q | FLOAT literal parse is full, ASCII, locale-independent | §17.5.2 | parser oracle/environment replay |
| 149 | Q | Finite FLOAT literal overflow is INVALID_LITERAL | §17.5.2 | decimal boundary fixture |
| 150 | Q | FLOAT literal underflow rounds without error | §17.5.2 | subnormal/zero vectors |
| 151 | R | VARCHAR literal uses doubled quote | §17.5.3 | token handoff fixture |
| 152 | R | Backslash has no escape meaning | §17.5.3 | literal byte assertion |
| 153 | R | Quoted embedded NUL is retained | §17.5.3 | length-delimited fixture |
| 154 | R | Unquoted source NUL is lexical error | §§17.5.3, 18 | parser/lexer composition |
| 155 | R | BOOLEAN literals are TRUE/FALSE keywords only | §17.5.3 | keyword/literal sweep |
| 156 | R | Numeric/string literals never become BOOLEAN implicitly | §17.5.3 | binder negatives |
| 157 | S | DATE has no typed-literal syntax | §17.5.4 | syntax/resolver negative |
| 158 | S | TIMESTAMP has no typed-literal syntax | §17.5.4 | syntax/resolver negative |
| 159 | S | Temporal construction uses explicit VARCHAR cast | §17.5.4 | cast-path fixture |
| 160 | S | DATE grammar is exact YYYY-MM-DD | §17.5.4 | grammar/calendar corpus |
| 161 | S | TIMESTAMP grammar uses space and mandatory seconds | §17.5.4 | grammar corpus |
| 162 | S | Timestamp fraction is absent or 1..6 digits | §17.5.4 | fraction boundary sweep |
| 163 | S | Fraction digits right-pad to microseconds | §17.5.4 | exact scalar fixture |
| 164 | S | More than six fraction digits reject, not round | §17.5.4 | negative boundary |
| 165 | S | Year range is exactly 0001..9999 | §17.5.4 | year boundary fixtures |
| 166 | S | Gregorian leap rule is exact | §17.5.4 | century/leap corpus |
| 167 | S | Time is 00:00:00 through 23:59:59 | §17.5.4 | field boundary corpus |
| 168 | S | Timezone/whitespace/trailing bytes reject | §17.5.4 | full-consumption negatives |
| 169 | T | Scalar children evaluate left-to-right by default | §17.6 | event trace |
| 170 | T | Strict NULL suppresses non-NULL operation after child evaluation | §17.6 | NULL/error precedence matrix |
| 171 | T | Strictness never suppresses an error raised by a demanded child | §17.6 | child-error event trace |
| 172 | T | Strict result nullability follows nullable inputs | §17.6 | bound nullability assertions |
| 173 | T | Unary plus exists only for INT32/INT64/FLOAT64 | §17.6.1 | unary 7-type matrix |
| 174 | T | Unary plus is same-type identity | §17.6.1 | boundary/NULL fixtures |
| 175 | T | Integer unary minus is checked | §17.6.1 | min/non-min sweep |
| 176 | T | Direct-minimum literal exception is not runtime negation | §§17.5.1, 17.6.1 | literal/runtime contrast |
| 177 | T | FLOAT unary minus uses IEEE sign negation | §17.6.1 | bit fixtures |
| 178 | T | NOT has only BOOLEAN overload | §17.6.1 | unary registry sweep |
| 179 | T | No unary overload exists for VARCHAR/DATE/TIMESTAMP | §17.6.1 | binder negatives |
| 180 | U | Binary arithmetic registry is exactly §17.6.2 table | §17.6.2 | exhaustive 7x7x5 binding matrix |
| 181 | U | INT32/INT32 arithmetic retains INT32 | §17.6.2 | result-type assertions |
| 182 | U | Any INT64 integer pair promotes to INT64 | §17.6.2 | both-order result fixtures |
| 183 | U | Any legal FLOAT64 numeric pair promotes to FLOAT64 | §17.6.2 | all mixed pair fixtures |
| 184 | U | Minimum promotions, not arbitrary explicit casts, resolve arithmetic | §17.6.2 | inserted-cast inspection |
| 185 | U | VARCHAR concatenation is unsupported | §17.6.2 | binder negative |
| 186 | U | DATE/TIMESTAMP arithmetic is unsupported | §17.6.2 | binder negative |
| 187 | U | FLOAT64 division by zero is not SQL integer error | §§17.6.2, 39.3.2 | division contrast |
| 188 | V | Every ordinary comparison returns BOOLEAN | §17.7.1 | bound result-type matrix |
| 189 | V | Comparison nullability follows nullable operands | §17.7.1 | nullability matrix |
| 190 | V | BOOLEAN supports equality/inequality only | §17.7.1 | comparison matrix |
| 191 | V | Every numeric pair supports equality and order | §17.7.1 | 3x3 numeric pair matrix |
| 192 | V | Numeric comparison uses smallest common type | §17.7.1 | inserted-cast/common-type oracle |
| 193 | V | Inexact INT64→FLOAT64 promotion governs comparison | §§17.7.1, 17.8.1 | `2^53` boundary fixtures |
| 194 | V | VARCHAR same-type equality/order uses exact bytes | §17.7.1 | byte comparator differential |
| 195 | V | DATE same-type equality/order uses signed days | §17.7.1 | signed carrier fixtures |
| 196 | V | TIMESTAMP same-type equality/order uses signed microseconds | §17.7.1 | signed carrier fixtures |
| 197 | V | Every other comparison pair is unsupported | §17.7.1 | full 7x7 matrix |
| 198 | V | VARCHAR/numeric/temporal implicit comparison is absent | §17.7.1 | cross-family negatives |
| 199 | V | DATE/TIMESTAMP cross-comparison is absent | §17.7.1 | temporal negative |
| 200 | V | FLOAT SQL comparator uses canonical total semantics | §17.7.1 | float class matrix |
| 201 | V | FLOAT SQL and B+ non-NULL order agree | §§17.7.1, 8.5.5 | semantic/encoded sort differential |
| 202 | W | Any NULL operand makes legal ordinary comparison UNKNOWN | §17.7.2 | operator/type/side NULL sweep |
| 203 | W | `x=NULL` is not rewritten to IS NULL | §§17.7.2, 17.12 | bound-tree/result fixture |
| 204 | W | IS NULL accepts every valid scalar type | §17.7.2 | seven-type matrix |
| 205 | W | IS NOT NULL accepts every valid scalar type | §17.7.2 | seven-type matrix |
| 206 | W | NULL predicates return non-NULL BOOLEAN | §17.7.2 | result validity assertions |
| 207 | W | NULL predicates evaluate/error their child | §17.7.2 | erroring-child fixture |
| 208 | W | IS TRUE/FALSE/UNKNOWN are unsupported | §§17.7.2, 17.9.2 | predicate negatives |
| 209 | X | Scalar evaluation follows source child order | §§17.6, 17.7.3 | numbered event trace |
| 210 | Y | FALSE AND skips RHS | §17.7.3 | controlled-error/counter fixture |
| 211 | Y | TRUE OR skips RHS | §17.7.3 | controlled-error/counter fixture |
| 212 | Y | Other AND/OR left states demand RHS | §17.7.3 | TRUE/FALSE/UNKNOWN demand matrix |
| 213 | Y | Searched CASE conditions run in source order | §17.7.3 | branch event trace |
| 214 | Y | CASE stops at first TRUE and demands only selected result | §17.7.3 | selected/unselected errors |
| 215 | Y | CASE ELSE runs only with no TRUE | §17.7.3 | ELSE demand fixture |
| 216 | Y | IN evaluates left exactly once | §17.7.3 | evaluation counter |
| 217 | Y | IN items run left-to-right and stop at first TRUE | §17.7.3 | item event trace |
| 218 | Y | IN without TRUE continues to determine UNKNOWN | §17.7.3 | UNKNOWN/later-item fixture |
| 219 | Y | Optimizer cannot force skipped error/volatile expression | §§17.7.3, 17.10.2 | fold/rewrite demand differential |
| 220 | Z | Explicit cast syntax resolves only closed 7x7 matrix | §17.8 | complete cast matrix |
| 221 | Z | Identity cast is legal implicitly and explicitly | §17.8 | seven identity edges |
| 222 | Z | Registered widening cast is also explicit | §17.8 | I-edge binding fixtures |
| 223 | Z | EX cells require explicit CAST | §17.8 | implicit-negative/explicit-positive pairs |
| 224 | Z | NO cells are bind-time TYPE_ERROR | §§17.8, 17.10.1 | matrix negative sweep |
| 225 | Z | Direct cast never inherits an unregistered chained path | §§17.8, 17.10.4 | direct-versus-chain discriminators |
| 226 | AC | INT32→INT64 is exact | §17.8.1 | full boundary conversion |
| 227 | AC | INT32→FLOAT64 rounds nearest/ties-even | §17.8.1 | precision-boundary oracle |
| 228 | AC | INT64→FLOAT64 rounds nearest/ties-even | §17.8.1 | `2^53`/limits corpus |
| 229 | AC | Inexact integer→FLOAT64 remains legal | §17.8.1 | exactness contrast |
| 230 | AC | INT64→INT32 is explicit and range-checked | §§17.8, 17.8.1 | four boundary values |
| 231 | AC | FLOAT→integer is explicit | §§17.8, 17.8.1 | implicit negative/explicit positive |
| 232 | AC | Finite FLOAT→integer truncates toward zero | §17.8.1 | signed fractional corpus |
| 233 | AC | FLOAT→integer checks range after truncation | §17.8.1 | adjacent target limits |
| 234 | AC | FLOAT NaN/infinity→integer is INVALID_CAST | §17.8.1 | special-class matrix |
| 235 | AC | FLOAT signed zero→integer is zero | §17.8.1 | both-zero fixtures |
| 236 | AC | No numeric cast wraps or modulo-truncates | §§17.8.1, 17.10.4 | out-of-range negative |
| 237 | AD | VARCHAR casts parse complete byte string | §17.8.2 | full-consumption corpus |
| 238 | AD | VARCHAR parse permits no surrounding whitespace | §17.8.2 | whitespace variants |
| 239 | AD | VARCHAR parse recognizes ASCII only | §17.8.2 | Unicode digit/sign negatives |
| 240 | AD | VARCHAR→BOOLEAN accepts case-insensitive TRUE/FALSE only | §17.8.2 | accepted/near-match corpus |
| 241 | AD | VARCHAR→INT32/INT64 grammar is exact | §17.8.2 | sign/zero/range corpus |
| 242 | AD | VARCHAR→FLOAT64 grammar and special spellings are exact | §17.8.2 | grammar/special corpus |
| 243 | AD | VARCHAR→DATE delegates exact temporal grammar | §§17.8.2, 17.5.4 | calendar parse corpus |
| 244 | AD | VARCHAR→TIMESTAMP delegates exact temporal grammar | §§17.8.2, 17.5.4 | timestamp parse corpus |
| 245 | AD | Empty/malformed/trailing/embedded-NUL BOOLEAN/numeric parse is INVALID_CAST | §17.8.2 | negative corpus |
| 246 | AD | Valid integer/finite-FLOAT text range failure is NUMERIC_OVERFLOW | §17.8.2 | range boundary corpus |
| 247 | AD | FLOAT text underflow rounds and preserves negative zero | §17.8.2 | signed underflow vectors |
| 248 | AD | Invalid temporal text uses INVALID_DATE/INVALID_TIMESTAMP | §17.8.2 | error taxonomy fixture |
| 249 | AD | NaN text constructs canonical semantic NaN | §17.8.2 | special parse fixture |
| 250 | AE | BOOLEAN→VARCHAR emits TRUE/FALSE | §17.8.3 | canonical byte fixtures |
| 251 | AE | Integer→VARCHAR is shortest canonical decimal | §17.8.3 | signed boundary corpus |
| 252 | AE | DATE→VARCHAR uses exact date text or INVALID_CAST | §17.8.3 | range/calendar corpus |
| 253 | AE | TIMESTAMP→VARCHAR always emits six fractional digits or error | §17.8.3 | exact format corpus |
| 254 | AE | FLOAT special/zero spellings are canonical | §17.8.3 | NaN/Inf/zero corpus |
| 255 | AE | Finite FLOAT text is shortest bit-round-trippable | §17.8.3 | independent formatter corpus |
| 256 | AE | FLOAT candidate-distance tie uses even final digit | §17.8.3 | tie corpus |
| 257 | AE | Fixed/scientific shortest-form tie chooses fixed | §17.8.3 | representation tie corpus |
| 258 | AE | Scientific syntax has canonical lowercase/no-plus/no-leading-zero form | §17.8.3 | exact byte assertions |
| 259 | AE | Formatting is independent of libc/iostream/locale | §§17.8.3, 17.10.4 | environment differential |
| 260 | AF | DATE→TIMESTAMP sets midnight and checks multiplication | §17.8.4 | widened microsecond oracle |
| 261 | AF | Unrepresentable DATE→TIMESTAMP is NUMERIC_OVERFLOW | §17.8.4 | exact overflow boundary |
| 262 | AF | TIMESTAMP→DATE floors toward prior midnight | §17.8.4 | negative-boundary matrix |
| 263 | AF | TIMESTAMP→DATE checks int32 day range | §17.8.4 | target-boundary fixture |
| 264 | AF | Temporal casts perform no timezone conversion | §17.8.4 | timezone replay |
| 265 | AA | Automatic coercion is contextual, not arbitrary path search | §17.8.5 | context-specific resolver trace |
| 266 | AA | Operator coercion is exact/minimum numeric promotion only | §17.8.5 | operator matrix inspection |
| 267 | AA | CASE/IN/VALUES common type is same or smallest numeric type | §17.8.5 | common-type matrix |
| 268 | AA | Scalar functions add no coercion because registry is empty | §§17.8.5, 17.9.3 | function negatives |
| 269 | AA | Aggregate coercion uses only Chapter-29 signatures | §§17.8.5, 29.3 | aggregate binding matrix |
| 270 | AA | Arbitrary unlike expressions receive no implicit conversion | §17.8.5 | unlike-expression negatives |
| 271 | AB | BOOLEAN assignment is identity only | §17.8.5 | assignment matrix |
| 272 | AB | INT32 assignment admits INT32/INT64/FLOAT64 | §17.8.5 | assignment matrix |
| 273 | AB | INT64 assignment admits INT64/FLOAT64 | §17.8.5 | assignment matrix |
| 274 | AB | FLOAT64/VARCHAR/DATE/TIMESTAMP assignment is same type only | §17.8.5 | assignment matrix |
| 275 | AB | Untyped NULL adopts any concrete destination before constraints | §17.8.5 | nullable/not-null fixtures |
| 276 | AB | Narrowing/parsing/stringifying/temporal conversion is explicit-only | §17.8.5 | assignment negatives |
| 277 | AB | Destination constraints run after coercion | §17.8.5 | coercion/constraint event order |
| 278 | AB | DDL default stores final destination-typed scalar | §17.8.5 | default encode fixture |
| 279 | AB | Runtime default application performs no deferred cast | §17.8.5 | bound-default inspection |
| 280 | AG | Only searched CASE is supported | §17.9.1 | searched/simple syntax-binding contrast |
| 281 | AG | Every CASE WHEN must be BOOLEAN | §17.9.1 | condition-type matrix |
| 282 | AG | Untyped NULL CASE condition becomes BOOLEAN UNKNOWN | §17.9.1 | contextual/demand fixture |
| 283 | AG | CASE result common type is same/smallest numeric | §17.9.1 | arm matrix |
| 284 | AG | NULL CASE arms adopt resolved result type | §17.9.1 | typed-NULL arm fixtures |
| 285 | AG | All-NULL CASE results fail binding | §17.9.1 | underconstrained case |
| 286 | AG | Missing ELSE is typed NULL of resolved result | §17.9.1 | result metadata/value fixture |
| 287 | AG | Simple CASE is unsupported | §17.9.1 | binder negative |
| 288 | AH | Expression-list IN/NOT IN are supported | §17.9.2 | binding/result matrix |
| 289 | AH | IN list must be nonempty | §17.9.2 | empty-list negative |
| 290 | AH | IN uses one comparison type | §§17.9.2, 17.8.5 | common-type inspection |
| 291 | AH | Any TRUE comparison makes IN TRUE | §17.9.2 | ordered item fixtures |
| 292 | AH | No TRUE plus any UNKNOWN makes IN UNKNOWN | §17.9.2 | NULL item/probe matrix |
| 293 | AH | No TRUE/UNKNOWN makes IN FALSE | §17.9.2 | nonmatch fixture |
| 294 | AH | NOT IN is exact 3VL NOT | §17.9.2 | paired IN/NOT IN matrix |
| 295 | AH | Scalar and subquery IN registries remain distinct | §§17.9.2, 20.14 | cross-owner binding fixtures |
| 296 | AH | Unlisted predicates/convenience expressions are unsupported | §17.9.2 | predicate sweep |
| 297 | AI | Named scalar-function registry is empty | §17.9.3 | representative/all-token function negatives |
| 298 | AI | AST function-call capacity does not authorize a function | §17.9.3 | parser/resolver negative |
| 299 | AI | Aggregate calls remain separate Chapter-29 registry | §§17.9.3, 29.3 | scalar/aggregate binding contrast |
| 300 | AI | Defaults may not invoke an unregistered named scalar function | §17.9.3 | default binding negative |
| 301 | AJ | One TypeResolver owns literal classification | §17.10.1 | cross-consumer registry identity |
| 302 | AJ | TypeResolver owns common numeric type | §17.10.1 | all-consumer common-type differential |
| 303 | AJ | TypeResolver owns operator/comparison/cast tables | §17.10.1 | binder/folder/executor signature comparison |
| 304 | AJ | TypeResolver owns assignment and CASE/IN resolution | §17.10.1 | resolved-plan inspection |
| 305 | AJ | No consumer adds overloads | §17.10.1 | unsupported cross-engine sweep |
| 306 | AY | Unsupported registry/type/underconstrained NULL is TYPE_ERROR | §17.10.1 | error matrix |
| 307 | AY | Malformed/out-of-domain source literal is INVALID_LITERAL | §17.10.1 | literal error fixtures |
| 308 | AY | Registered value-invalid cast is INVALID_CAST | §17.10.1 | cast error fixtures |
| 309 | AY | Checked arithmetic/narrowing/range is NUMERIC_OVERFLOW | §17.10.1 | range/operation fixtures |
| 310 | AY | Integer zero divisor is DIVISION_BY_ZERO | §17.10.1 | division/remainder fixtures |
| 311 | AY | Invalid DATE text is INVALID_DATE | §17.10.1 | date parse fixture |
| 312 | AY | Invalid TIMESTAMP text is INVALID_TIMESTAMP | §17.10.1 | timestamp parse fixture |
| 313 | AY | Unsupported static cast pair differs from value-invalid cast | §17.10.1 | TYPE_ERROR/INVALID_CAST contrast |
| 314 | AY | Constant versus row-dependent error timing follows demand | §§17.10.1–17.10.2 | fold/runtime timing matrix |
| 315 | AY | Arithmetic conceptual subcategory need not invent source enum | §17.10.1 | surfaced-category/lower-cause assertion |
| 316 | AY | Transaction consequence remains §39.1 owner | §§17.10.1, 39.1 | DML boundary composition |
| 317 | AK | Every scalar operator/cast is deterministic/IMMUTABLE | §17.10.2 | repeated/environment differential |
| 318 | AK | Folded value matches runtime concrete type | §17.10.2 | fold/runtime matrix |
| 319 | AK | Folded NULL/value matches runtime | §17.10.2 | fold/runtime matrix |
| 320 | AK | Folded integer checks/errors match runtime | §17.10.2 | overflow differential |
| 321 | AK | Folded FLOAT rounding/NaN/zero matches runtime | §17.10.2 | hard-float differential |
| 322 | AK | Folded parse/format/comparison/3VL matches runtime | §17.10.2 | registry-wide differential |
| 323 | AK | Dominating constant error reports at bind/plan time | §17.10.2 | root/dominance fixture |
| 324 | AK | Nondominating constant error remains demand-lazy | §17.10.2 | branch demand matrix |
| 325 | AK | Folding ignores estimated/actual row count for dominance | §17.10.2 | estimate/empty contrast |
| 326 | AK | Persisted defaults are mandatorily folded | §§17.10.2, 17.8.5 | default fixture |
| 327 | AK | Irrelevant EXISTS projection/order value is not demanded; relevant DISTINCT follows relational demand | §§17.10.2, 20.14.5 | subquery demand fixture |
| 328 | AK | Relational-subquery constant error needs proven outer+inner demand | §§17.10.2, 20.14, 20.17.10 | proof/demand matrix |
| 329 | AK | Possible/estimated subquery rows do not prove demand | §17.10.2 | estimate-only negative |
| 330 | AK | FLOAT folding is environment/architecture independent | §17.10.2 | environment replay |
| 331 | AK | Constant aggregate evaluation uses Chapter-29 state | §§17.10.2, 29.3 | scalar-fold/aggregate contrast |
| 332 | AL | Equal non-NULL hashable values hash equal | §17.10.3 | equality-class property |
| 333 | AL | Mixed numeric hash/equality applies bound common type | §17.10.3 | `2^53` mixed fixtures |
| 334 | AL | FLOAT hash/key normalize both zeros | §17.10.3 | zero class fixture |
| 335 | AL | FLOAT hash/key normalize all NaNs | §17.10.3 | NaN payload corpus |
| 336 | AL | VARCHAR hashes exact bytes | §17.10.3 | byte equality property |
| 337 | AL | Hash bits remain process-local implementation freedom | §17.10.3 | property-only assertion |
| 338 | AL | Ordinary predicate/join NULL is nonmatching | §17.10.3 | join/predicate fixtures |
| 339 | AL | Non-NULL NaNs match in joins | §17.10.3 | join equality fixture |
| 340 | AL | GROUP BY/DISTINCT coalesce NULLs | §17.10.3 | grouping fixture |
| 341 | AL | Grouping NULL rule does not leak into ordinary equality | §17.10.3 | negative cross-mode fixture |
| 342 | AM | B+ physical order includes NULL and BOOLEAN | §17.10.3 | key-order fixture |
| 343 | AM | SQL-orderable non-NULL order matches B+ | §17.10.3 | encoded-sort differential |
| 344 | AM | Index bounds preserve NULL/BOOLEAN predicate distinctions | §17.10.3 | bound/residual fixtures |
| 345 | AM | Broader physical bound retains residual recheck | §17.10.3 | access-path result differential |
| 346 | AM | UNIQUE bypasses NULL-containing key | §§11.10, 17.10.3 | unique composition fixture |
| 347 | AM | UNIQUE uses normalized non-NULL component equality | §§11.10, 17.10.3 | FLOAT/VARCHAR key fixtures |
| 348 | AM | Semantic proof uses only closed exact error-safe facts | §§17.10.3, 34.1, 35.2 | proof acceptance/rejection corpus |
| 349 | AM | Statistics/required_rows are not scalar semantic proof | §17.10.3 | estimate-versus-proof negative |
| 350 | AN | Host signed overflow/unchecked negation is rejected | §17.10.4 | widened-oracle differential |
| 351 | AN | Host quotient/remainder corner behavior is rejected | §17.10.4 | sign/MIN boundary corpus |
| 352 | AN | Implicit VARCHAR/numeric/temporal/BOOLEAN conversion is rejected | §17.10.4 | closed-matrix sweep |
| 353 | AN | Locale-sensitive parsing/formatting/collation is rejected | §17.10.4 | environment differential |
| 354 | AN | Host optional/pointer NULL equality is rejected | §17.10.4 | SQL/host contrast |
| 355 | AN | Raw IEEE NaN comparison/hash differences are rejected | §17.10.4 | special-class differential |
| 356 | AN | Library-default FLOAT text is not canonical oracle | §17.10.4 | exact text corpus |
| 357 | AN | Narrowing/wrapping cast shortcuts are rejected | §17.10.4 | target-boundary negatives |
| 358 | AN | Estimates/data/runtime libraries cannot add semantic facts/entries | §17.10.4 | unsupported/proof sweep |
| 359 | AN | Grouping NULL equality cannot replace predicate equality | §17.10.4 | cross-mode negative |
| 360 | AN | Scalar results cannot vary by locale/timezone/FENV/machine | §17.10.4 | platform corpus |
| 361 | AN | Host object/struct serialization is rejected | §17.10.4 | byte-oracle differential |
| 362 | AO | Generic Value is permitted only as semantic representation | §17.11 | semantic-field assertions |
| 363 | AO | Resolved generic Value has one logical type; pre-context null-only state remains nonpersisted | §§17.3, 17.11 | representation-permutation differential |
| 364 | AO | Generic Value owns escaping VARCHAR bytes | §17.11 | source-lifetime poison fixture |
| 365 | AO | Generic Value is not required hot executor cell layout | §17.11 | ownership/nonrequirement assertion |
| 366 | AP | Value escaping page/chunk lifetime remains valid | §17.11 | unpin/recycle lifetime fixture |
| 367 | AP | No persisted bytes derive from Value object layout | §§17.11, 17.13 | two-representation byte differential |
| 368 | AQ | PersistedScalarV1 is shared by listed metadata scalar uses | §17.13 | default/statistics codec comparison |
| 369 | AQ | PersistedScalarV1 differs from ordinary tuple codec | §§17.1, 17.13 | separate-codec assertions |
| 370 | AQ | Codec has no embedded version field | §17.13 | byte-layout/absence fixture |
| 371 | AQ | Enclosing supported grammar selects exact codec | §17.13 | envelope dispatch fixture |
| 372 | AQ | Header is exactly 16 bytes little-endian | §17.13.1 | independent byte fixture |
| 373 | AQ | TypeId is uint32 at offset 0 | §17.13.1 | offset/endian fixture |
| 374 | AQ | Flags are uint32 at offset 4 | §17.13.1 | offset/endian fixture |
| 375 | AQ | Payload length is uint32 at offset 8 | §17.13.1 | offset/endian fixture |
| 376 | AQ | reserved32 is zero at offset 12 | §17.13.1 | single-bit mutation |
| 377 | AQ | Flags bit 0 alone means IS_NULL | §17.13.1 | flag matrix |
| 378 | AQ | Total extent is Align8(16+length) | §17.13.1 | checked geometry oracle |
| 379 | AQ | Alignment padding is zero | §17.13.1 | every pad-byte mutation |
| 380 | AR | Typed NULL has IS_NULL=1 and length zero | §17.13.2 | seven-TypeId exact fixtures |
| 381 | AR | Typed NULL has no payload | §17.13.2 | payload/length defect fixtures |
| 382 | AR | Typed NULL retains concrete TypeId | §17.13.2 | decode assertions |
| 383 | AR | No unresolved/UNKNOWN encoding exists | §§17.3, 17.13.2 | codec negatives |
| 384 | AS | BOOLEAN payload is one byte 0/1 | §17.13.3 | exact/invalid-byte fixtures |
| 385 | AS | INT32 payload is 4-byte little-endian two's complement | §17.13.3 | min/max/0/-1 bytes |
| 386 | AS | INT64 payload is 8-byte little-endian two's complement | §17.13.3 | min/max/0/-1 bytes |
| 387 | AS | DATE payload is signed int32 little-endian | §17.13.3 | carrier boundary bytes |
| 388 | AS | TIMESTAMP payload is signed int64 little-endian | §17.13.3 | carrier boundary bytes |
| 389 | AS | FLOAT64 payload is exact uint64 little-endian | §17.13.3 | finite/zero/infinity bytes |
| 390 | AS | Persisted FLOAT signed zero is preserved | §17.13.3 | both-zero byte fixtures |
| 391 | AS | Persisted metadata NaN is canonical quiet pattern | §17.13.3 | canonical/noncanonical fixtures |
| 392 | AS | VARCHAR payload is exact bytes with no terminator | §17.13.3 | empty/NUL/high-byte fixtures |
| 393 | AT | Decoder rejects unknown/non-storable TypeId | §17.13.4 | 0/8/large fixtures |
| 394 | AT | Decoder rejects unknown flags | §17.13.4 | one-bit sweep |
| 395 | AT | Decoder rejects nonzero reserved32 | §17.13.4 | one-byte sweep |
| 396 | AT | Decoder rejects NULL with nonzero length | §17.13.4 | length/payload defect |
| 397 | AT | Decoder rejects wrong fixed-width length | §17.13.4 | per-type length matrix |
| 398 | AT | Decoder rejects BOOLEAN payload outside 0/1 | §17.13.4 | byte sweep |
| 399 | AT | Decoder rejects nonzero padding | §17.13.4 | every padding position |
| 400 | AT | Decoder rejects noncanonical persisted NaN | §17.13.4 | payload/sign corpus |
| 401 | AT | Decoder checks length arithmetic/truncation before access | §17.13.4 | terminal-length/truncation cases |
| 402 | AT | Successful decoder returns owned typed value | §17.13.4 | source-buffer poison fixture |
| 403 | AU | Every semantic type round-trips through ordinary tuple codec | §§5.9–5.12, 17.1 | tuple type/boundary matrix |
| 404 | AU | Tuple NULL meaning comes from bitmap/schema | §§5.10, 17.3 | bitmap/value differential |
| 405 | AU | Tuple NULL inactive bytes gain no semantic Value meaning | §§5.10.6, 17.3 | inactive-byte contrast |
| 406 | AU | Invalid tuple BOOLEAN is CORRUPT_HEAP | §§5.11.1, 17.10.1 | persisted/user-error contrast |
| 407 | AU | Malformed tuple VARCHAR framing is CORRUPT_HEAP | §§5.12, 17.10.1 | descriptor defect matrix |
| 408 | AU | Ordinary tuple FLOAT preserves all NaN payload bits | §§5.11.3, 17.4.3 | raw-bit round-trip |
| 409 | AU | All DATE/TIMESTAMP carrier bits are legal tuple values | §§5.11.2, 17.4.4–17.4.5 | extrema decode fixtures |
| 410 | AV | Integer/date/timestamp semantic order matches index bytes | §§8.5, 17.10.3 | semantic/encoded sort property |
| 411 | AV | VARCHAR semantic order matches memcomparable bytes | §§8.5.6, 17.4.6 | NUL/high-byte/prefix property |
| 412 | AV | FLOAT semantic classes/order match index bytes | §§8.5.5, 17.4.3 | full class property |
| 413 | AV | Physical NULLS FIRST remains distinct from SQL NULL comparison | §§8.5.1, 17.7.2 | direct contrast |
| 414 | AV | Physical BOOLEAN order remains distinct from SQL overloads | §§8.5.2, 17.4.1 | direct contrast |
| 415 | AV | Oversized encoded key does not invalidate semantic Value | §§8.6, 17.2 | key-limit composition |
| 416 | AW | Catalog ColumnDescriptor TypeId selects Chapter-17 semantics | §§16.4, 17.2 | descriptor/value round-trip |
| 417 | AW | DefaultValueBlob uses final typed PersistedScalarV1 | §§16.5.3, 17.8.5, 17.13, 21.12.1 | default handoff fixture |
| 418 | AW | Required metadata scalar defect is corruption | §§4.14.6, 17.13 | required envelope fixture |
| 419 | AW | Advisory statistics scalar defect invalidates generation only | §§17.13, 34.14 | same-defect advisory fixture |
| 420 | AW | Advisory fallback never weakens malformed core framing | §§16.5.7, 34.14 | core/advisory contrast |
| 421 | AX | Tuple SchemaVer resolves exact historical descriptor | §§5.13, 16.7 | old-tuple/new-schema fixture |
| 422 | AX | Current descriptor never substitutes for old SchemaVer | §16.7 | wrong-current negative |
| 423 | AX | TypeId meaning is stable, nonreused, and never rebound across SchemaVer | §§16.4, 16.7, 17.2 | multi-version descriptor fixture |
| 424 | AX | Historical decode uses historical column arrangement | §§16.7–16.8 | reordered-column fixture |
| 425 | AX | the architecture has no hidden historical type parameter | §§17.2, 17.4.6 | descriptor/type negative |
| 426 | AZ | Owned VARCHAR allocation failure is resource error | §§17.11, 39.3 | injected allocation failure |
| 427 | AZ | Parse/format allocation failure is not INVALID_CAST/NULL | §§17.8, 39.3 | resource/error contrast |
| 428 | AZ | Resource failure cleans query-owned state | §39.3 | lifetime assertions |
| 429 | AY | No semantic error silently returns NULL | §§17.6, 17.10.1 | error-versus-NULL matrix |
| 430 | AY | User semantic invalidity differs from persisted corruption | §§17.10.1, 17.13.4 | paired semantic/byte fixtures |
| 431 | AY | Same scalar error before first DML write follows FA owner | §§15, 39.1 | prepublication DML fixture |
| 432 | AY | Same scalar error after first DML write follows MA owner | §§15, 39.1 | postpublication DML fixture |
| 433 | AY | Chapter 17 never independently chooses transaction state | §§17.10.1, 39.1 | ownership assertion |
| 434 | BA | Scalar semantics are locale-independent | §§17.5–17.10 | locale replay |
| 435 | BA | Scalar semantics are timezone-environment-independent | §§17.4.5, 17.10.2 | timezone replay |
| 436 | BA | VARCHAR order is char-signedness-independent | §§17.4.6, 17.10.4 | high-byte corpus |
| 437 | BA | Semantic values are native-endian-independent | §§17.1, 17.10.4 | opposite-endian oracle |
| 438 | BA | FLOAT results are host-rounding/precision/contraction-independent | §§17.4.3, 17.10.2 | software-oracle discriminators |
| 439 | BA | Persistent bytes never use native object layout | §§5, 8, 17.10.4, 17.13 | byte-exact differential |
| 440 | BB | Unsupported type names do not resolve | §§17.2, 17.4 | type sweep |
| 441 | BB | Unsupported casts do not resolve | §17.8 | 7x7 NO-cell sweep |
| 442 | BB | Unsupported unary/binary operators do not resolve | §17.6 | registry complement sweep |
| 443 | BB | Unsupported predicates do not resolve | §17.9.2 | predicate complement sweep |
| 444 | BB | Unsupported named scalar functions do not resolve | §17.9.3 | function sweep |
| 445 | BB | Unsupported string/temporal arithmetic is not invented | §§17.6.2, 17.10.4 | targeted negatives |
| 446 | BB | Unsupported BOOL/numeric and VARCHAR implicit coercion is not invented | §§17.8, 17.10.4 | coercion negatives |
| 447 | BB | Unsupported DATE/TIMESTAMP implicit comparison is not invented | §§17.7.1, 17.10.4 | comparison negative |
| 448 | BC | Closed registry is used consistently by binder/folder/executor | §§17.4, 17.10.1 | three-engine differential |
| 449 | BC | Closed registry is consistent with tuple/index/catalog handoffs | §§17.1, 17.10.3 | cross-codec matrix |
| 450 | BC | Every Chapter-17 invariant maps to a direct procedure | §17.12 | invariant-to-map audit |


| Required family | Direct ownership in this section |
|---|---|
| V17-1 closed TypeResolver/type registry | fixed registry, permuted runtime tags, and complete complement sweeps |
| V17-2 literals/direct-negative integers | independent token grammars and exact `2^31`/`2^63` boundaries |
| V17-3 contextual NULL/unresolved marker | every resolving/underconstrained context and no-persistence boundary |
| V17-4 casts/coercions | complete cast, assignment, operator, and comparison matrices |
| V17-5 VARCHAR parse/format | arbitrary-byte, full-consumption, canonical-format, and size-owner fixtures |
| V17-6 DATE/TIMESTAMP | full carrier domains, calendar/text boundaries, checked casts, and timezone replay |
| V17-7 PersistedScalarV1 | exact header/payload/padding and one-defect required/advisory fixtures |
| V17-8 FLOAT64 equivalence | bit classes, arithmetic rounding, text, tuple, index, hash, grouping, and UNIQUE |
| V17-9 error taxonomy | semantic/corruption/advisory/resource and DML-boundary matrix |
| V17-10 storage handoff | ordinary tuple, metadata, TypeId, index, default, and historical-schema compositions |
| V17-11 folding/demand | fold/runtime values/errors, scalar/subquery demand, reassociation, and contraction |
| V17-12 unsupported registry | exhaustive absent type/cast/operator/predicate/function sweeps |

---

## Subquery Tests

Execute every row of `ARCHITECTURE.md` §20.14.1 as a table-driven bind/plan/result or
rejection case. The table supplies expected support, arity, bound result, zero/multirow
behavior, canonical logical mode, and physical fallback; this guide does not duplicate the
matrix. Every accepted occurrence is independently uncorrelated and every rejected form
produces its structured error/span without a partially valid bound or logical plan.

### Scopes and derived tables

Create nested blocks with local names, local aliases, ambiguous local references, names
that exist only in an ancestor, and independently nested no-FROM SELECTs. Assert lookup is
local-only; an attempted ancestor capture is `UnsupportedCorrelation`, an unrelated missing
name remains the ordinary name error, and no executable OuterRef/Apply state appears.

For derived tables, test the mandatory relation alias with and without optional `AS`,
missing/duplicate/invalid aliases, explicit and direct-source output names, duplicate or
generated unnamed outputs, `d.*` projection order, and lookup through the derived
descriptor. Child aliases, hidden columns, and unnamed generated outputs do not leak.
Derived-column alias lists are wholly unsupported in the defined architecture, so every `d(a,b)` form is rejected
rather than tested as a supported column-count-mismatch feature.

### Scalar cardinality and error precedence

For each scalar result type, construct final subquery output with zero, exactly one, two,
and more than two rows after WHERE/aggregation/HAVING/DISTINCT/ORDER/OFFSET/LIMIT. Assert:

```text
zero rows -> one typed NULL scalar
one row -> that value, including SQL NULL
second successfully constructed row -> CardinalityError
```

Inject an error while constructing the first row, the second row, and a later row. First/
second-row construction errors precede cardinality failure; after two rows construct
successfully, CardinalityError precedes later work. `LIMIT 1` may prove at-most-one, while
an estimate or `required_rows=1` may not remove the check.

### EXISTS demand

Reuse V20-13's independent DISTINCT/OFFSET class-count and projection-relevance fixtures.
Use children whose irrelevant projection, DISTINCT value, or ORDER BY key would raise a
controlled error if value-evaluated. Assert such work is bound but not demanded; relevant
DISTINCT values and FROM/WHERE/grouping/HAVING/OFFSET/LIMIT work retain their owner demand. A global
aggregate performs its required child work and then exists even over empty input.

Place deterministic counters/failures after the first final relational row. EXISTS stops
at that row; NOT EXISTS applies ordinary Boolean NOT. `LIMIT 0`, OFFSET, grouped
aggregation, and no-row cases exercise the opposite outcomes without relying on timing.

### IN and NOT IN

Drive the complete §20.14.6 truth table with empty and nonempty RHS, NULL/non-NULL probes,
matching and nonmatching values, RHS NULL markers, all-NULL RHS, and duplicate values/
NULLs. Assert exact TRUE/FALSE/UNKNOWN in both filtering and projection contexts. Empty RHS
returns FALSE/TRUE for IN/NOT IN even with a NULL probe; duplicate multiplicity does not
change truth or require one physical deduplication strategy.

Repeat with promoted numerics, VARCHAR bytes, infinities, `+0.0/-0.0`, and canonical-
equivalent NaNs. Membership equality/hash state must agree with the shared §17.10.3 type
property tests. Inject output, memory, and spill errors throughout the complete RHS build;
on first demand a left-expression error precedes a dormant build, while any demanded build
error precedes probing.

### Lazy occurrence, snapshot, and CommandId

Place scalar/EXISTS/IN occurrences in selected and skipped CASE arms, short-circuited AND/
OR right operands, and predicates over empty outer selections. A skipped occurrence never
runs; a demanded occurrence initializes at most once per bound occurrence per statement
attempt and serves all outer rows. On statement retry, old side state is discarded.

Use concurrent commits and current-command visibility fixtures to assert outer and subquery
plans share one effective snapshot, ReadEpochGuard, TxnId, and CommandId. A subquery never
takes a fresh READ COMMITTED snapshot, consumes a new CommandId, creates a nested
transaction, or independently owns logical locks. Cross-reference the §41.3 isolation and
statement-retry tests for the underlying visibility mechanics.

### Relational clauses and DML integration

Cover global and grouped aggregates, DISTINCT, ORDER BY, LIMIT, and OFFSET inside scalar,
EXISTS, IN, and derived-table contexts. Include global aggregate over empty input, zero/
one/multiple grouped results, DISTINCT reducing scalar duplicates, scalar `LIMIT 1`,
EXISTS after OFFSET, and IN over the post-LIMIT/OFFSET result. Without ORDER BY, do not
assert which arbitrary row `LIMIT 1` selects.

Evaluate subquery errors in INSERT/UPDATE/DELETE WHERE, assignments, input expressions,
and RETURNING before and after the first published transaction-owned write. Assert the
subquery error enters the existing §39.1 matrix at that boundary; this section defines no
new rollback/retry policy.

### Rewrites and unsupported forms

For each exactly proven empty subquery child, compare fallback and rewritten scalar,
EXISTS/NOT EXISTS, IN/NOT IN, and derived-table results. Preserve one-time left evaluation,
lazy demand, 3VL, schema, and errors. Repeat with `estimated_rows == 0` but no approved proof
and assert no exact-empty rewrite, cross-referencing the §41.6 Semantic Emptiness Tests.

Directly exercise every unsupported §20.14.1 row, including correlated forms, row-valued
or multi-column scalar/IN forms, ANY/SOME/ALL, LATERAL, CTE/set-operation forms, and
data-modifying subqueries. Assert the architecture-defined `UnsupportedFeature`,
`UnsupportedCorrelation`, type, or syntax category, useful source span, and absence of an
executable logical/physical node. Parser/AST capacity and hypothetical decorrelation never
authorize support.

---

## Catalog Tests

These procedures prove the Chapter 16 catalog contract. They specialize the byte, lifecycle,
MVCC, reclamation, DDL, and statistics procedures owned elsewhere; they do not make the
catalog cache, a filename, a physical catalog RID, or a production decoder an authority.

### Deterministic catalog harness and independent oracles

Use barriers and exact persisted-prefix construction around these semantic events:

```text
CREATE DATABASE:
    candidate root created; catalog-object ID reserved; FileId reserved;
    system relation initialized; bootstrap rows encoded; catalog.dat encoded;
    startup-critical file synchronization complete; root rename complete;
    external-parent fsync complete

OPEN:
    database owner acquired; control selected; recovery begins/completes;
    catalog.dat superblock validated; CATALOG_DATA validated;
    system files recovered; catalog rows decoded; fixed-point comparison completes;
    immutable descriptors constructed; cache publication completes; READY publishes

CATALOG MVCC/CACHE:
    statement snapshot captured; row candidate read; status resolved;
    snapshot-qualified cache lookup; descriptor assembled; terminal state publishes;
    cache install/invalidate/bypass; descriptor reference released

DDL/RETIREMENT:
    SchemaLock acquired; target TableWriterGate acquired where required;
    object/FileId allocated; final-name publication completes; catalog mutation publishes;
    COMMIT/ABORT publishes; orphan/retirement begins; file drain/unlink completes

HISTORICAL/STATISTICS:
    tuple SchemaVer read; ResolveSchema begins/completes; old descriptor retained/released;
    statistics rows selected; chunks assembled; candidate rejected; fallback selected
```

No race uses a sleep as evidence. A barrier releases one named event at a time; a crash
fixture persists only its selected files, pages, control slot, directory entries, and WAL
prefix before reopen. Primary negative fixtures start canonical and introduce exactly one
defect.

The independent reference package implements only test-side arithmetic and semantic models:

| Oracle | Independent input and required comparison |
|---|---|
| `catalog.dat` file oracle | Two independently assembled 8192-byte pages; exact total length 16384; no trailing page or byte |
| CATALOG superblock oracle | Chapter 4 field offsets, little-endian loads/stores, catalog FileKind/object specialization, and zero regions; no production superblock codec |
| CATALOG_DATA oracle | Chapter 16 offsets, six entry encodings, zero suffix, and exact page number/type; no production catalog codec |
| CRC oracle | A separately selected CRC32C implementation or known vectors, with checksum bytes logically zero |
| built-in identity oracle | Literal relation-code/TableId/name registry and independently tracked allocator high-water |
| descriptor oracle | Literal six-relation schema table below, sorted by explicit ordinals rather than production builder order |
| carrier oracle | Widened unsigned arithmetic plus field-specific sentinel/writer-limit tables |
| key/reference oracle | Set/multiset model over typed logical keys and explicit same-table ownership edges |
| fixed-point oracle | Field-by-field equality between the literal built-in descriptors and descriptors reconstructed from MVCC-visible rows |
| snapshot/cache oracle | Chapter 9/10 visibility reference plus an independently tagged descriptor version; cache contents are only candidates |
| historical-schema oracle | Exact map keyed by `(TableId, SchemaVer)` plus independently retained tuple/descriptor dependencies |
| DDL publication oracle | Durable namespace manifest + MVCC catalog rows + transaction outcome; runtime cache state is excluded |
| statistics oracle | Exact chunk identity/order and Chapter 34 payload validator/selection model |
| READY oracle | Conjunction of Chapter 3/13 recovery and every required Chapter 16 validation prerequisite |

Production serializers, decoders, descriptor builders, cache lookup, and cache invalidation
cannot validate themselves. Differential comparison against a second invocation of the same
production component is insufficient.

### `catalog.dat` byte, checksum, and format verification

Create the canonical file independently. It is exactly 16384 bytes: page 0 is one CATALOG
FileSuperblock and page 1 is one immutable CATALOG_DATA page. Reject a truncated file,
missing page, swapped pages, any overlong byte/page, or a page at the wrong offset according
to the owning corruption/missing-required-object result. The generic superblock decoder's
ability to accept a larger input buffer does not authorize an overlong `catalog.dat` file.

The page-0 fixture chooses one valid nonzero FileId and one explicit opaque
`creation_epoch`, then compares every byte below. Its `page_lsn` uses an architecture-legal
initial superblock value; the fixture does not assign new semantics to that generic field.
The page-1 fixture uses `INVALID_LSN` exactly. All integers are encoded little-endian.

#### Mandatory `catalog.dat` byte matrix

| Structure | Offset | Width | Canonical value/rule | Positive fixture | One-defect fixture | Expected class | Architecture / verification owner |
|---|---:|---:|---|---|---|---|---|
| whole file | 0 | 16384 | exactly two pages | exact-length image | truncate or append one byte/page | corrupt/missing required catalog file | §§16.9, 4.14 / this section |
| superblock common | 0 | 2 | `SUPERBLOCK=0` | literal LE code | `CATALOG_DATA` | `CORRUPT_FILE` | §§4.9–4.10, 16.9.1 / generic codec + specialization |
| superblock common | 2 | 2 | `format_version=1` | v1 | zero; greater recognized version | corrupt; unsupported file format | §§4.10, 4.14.2 / format matrix |
| superblock common | 4 | 4 | flags zero | zero | one set bit | corrupt | §§4.10.1, 4.14.3 / reserved fixture |
| superblock common | 8 | 8 | generic valid page LSN | chosen legal value | illegal owner-specific value if applicable | owning result | §§4.8.2, 4.10 / generic page fixture |
| superblock common | 16 | 4 | whole-page CRC32C | independent CRC | flip checksum bit | corrupt | §4.10.3 / CRC oracle |
| superblock common | 20 | 2 | `header_size=72` | 72 | 128/other | corrupt | §§4.10, 16.9.1 / specialization |
| superblock common | 22 | 2 | reserved zero | zero | one set bit | corrupt | §4.10.1 / reserved fixture |
| superblock common | 24 | 8 | `page_no=0` | zero | one | corrupt | §4.10.1 / specialization |
| superblock prefix | 32 | 8 | ASCII `DBLUSBLS` | exact bytes | one wrong byte | corrupt | §4.10 / byte oracle |
| superblock prefix | 40 | 2 | `FileKind::CATALOG=4` | 4 | known wrong/unknown kind | corrupt | §§4.7, 16.9.1 / specialization |
| superblock prefix | 42 | 2 | reserved zero | zero | one set bit | corrupt | §4.10.1 / reserved fixture |
| superblock prefix | 44 | 4 | page size 8192 | 8192 | 4096 | corrupt | §4.10.1 / byte oracle |
| superblock prefix | 48 | 4 | allocated nonzero FileId | chosen legal ID | zero/wrong catalog FileId | corrupt/identity mismatch | §§4.3.2, 16.9.1 / identity oracle |
| superblock prefix | 52 | 4 | reserved zero | zero | one set bit | corrupt | §4.10.1 / reserved fixture |
| superblock prefix | 56 | 8 | `object_id=0` | zero | nonzero | corrupt | §§4.10.2, 16.9.1 / specialization |
| superblock prefix | 64 | 8 | opaque creation epoch | chosen bit pattern | altered pattern with recomputed CRC remains valid | valid opaque value | §4.10.2 / round-trip fixture |
| superblock suffix | 72 | 8120 | all zero | zero | one nonzero byte | corrupt | §4.10.1 / reserved fixture |
| CATALOG_DATA common | 0 | 2 | `CATALOG_DATA=6` | 6 | wrong/unknown PageType | corrupt page | §§4.9, 16.9.2 / byte oracle |
| CATALOG_DATA common | 2 | 2 | `format_version=1` | 1 | zero; greater | corrupt; unsupported page format | §§4.8, 4.14.2 / format matrix |
| CATALOG_DATA common | 4 | 4 | flags zero | zero | one set bit | corrupt | §§4.8, 16.9.2 / reserved fixture |
| CATALOG_DATA common | 8 | 8 | `INVALID_LSN` | exact sentinel | another value | corrupt bootstrap | §16.9.2 / byte oracle |
| CATALOG_DATA common | 16 | 4 | whole-page CRC32C | independent CRC | checksum/payload bit flip | corrupt | §§4.8, 16.9.3 / CRC oracle |
| CATALOG_DATA common | 20 | 2 | `header_size=64` | 64 | other | corrupt | §16.9.2 / byte oracle |
| CATALOG_DATA common | 22 | 2 | reserved zero | zero | one set bit | corrupt | §16.9.2 / reserved fixture |
| CATALOG_DATA common | 24 | 8 | `page_no=1` | 1 | zero/two | corrupt | §16.9.2 / byte oracle |
| CATALOG_DATA body | 32 | 8 | ASCII `DBLUSCAT` | exact bytes | one wrong byte | corrupt | §16.9.2 / byte oracle |
| CATALOG_DATA body | 40 | 2 | `bootstrap_version=1` | 1 | zero; greater | corrupt; unsupported catalog format | §§16.9.2–16.9.3 / format matrix |
| CATALOG_DATA body | 42 | 2 | `entry_count=6` | 6 | 5/7 | corrupt | §16.9.2 / entry oracle |
| CATALOG_DATA body | 44 | 4 | `catalog_schema_version=1` | 1 | zero; greater | corrupt; unsupported catalog schema | §§16.5, 16.9.3 / format matrix |
| CATALOG_DATA body | 48 | 8 | `bootstrap_generation=1` | 1 | zero/two | corrupt | §§16.9.2–16.9.3 / byte oracle |
| CATALOG_DATA body | 56 | 8 | reserved zero | zero | one set bit | corrupt | §16.9.2 / reserved fixture |
| entries | 64 | 192 | six 32-byte entries in code order | exact six | omit/duplicate/reorder | corrupt | §16.9.2 / entry oracle |
| entry `i` | `64+32i` | 2 | relation code `i+1` | 1..6 | wrong/duplicate/out of order | corrupt | §16.9.2 / entry oracle |
| entry `i` | `66+32i` | 2 | flags zero | zero | one set bit | corrupt | §16.9.2 / reserved fixture |
| entry `i` | `68+32i` | 4 | relation SchemaVer 1 | 1 | zero; greater | corrupt; unsupported catalog schema | §§16.9.2–16.9.3 / format matrix |
| entry `i` | `72+32i` | 8 | TableId equals relation code | 1..6 | mismatch | corrupt | §§16.5.1, 16.9.2 / identity oracle |
| entry `i` | `80+32i` | 4 | nonzero unique HEAP FileId | chosen allocated ID | zero/duplicate/wrong owner | corrupt | §§16.5.2, 16.9.2 / file oracle |
| entry `i` | `84+32i` | 4 | nonzero unique FSM FileId | chosen allocated ID | zero/duplicate/role reuse | corrupt | §§16.5.2, 16.9.2 / file oracle |
| entry `i` | `88+32i` | 8 | reserved zero | zero | one set bit | corrupt | §16.9.2 / reserved fixture |
| CATALOG_DATA suffix | 256 | 7936 | all zero | zero | one nonzero byte | corrupt | §§16.9.2–16.9.3 / reserved fixture |

Instantiate the entry rows six times rather than testing only a generic entry:

| Entry index | Absolute byte range | Code/TableId | Canonical relation | Relation SchemaVer |
|---:|---:|---:|---|---:|
| 0 | 64..95 | 1 | `sys_tables` | 1 |
| 1 | 96..127 | 2 | `sys_columns` | 1 |
| 2 | 128..159 | 3 | `sys_indexes` | 1 |
| 3 | 160..191 | 4 | `sys_index_columns` | 1 |
| 4 | 192..223 | 5 | `sys_constraints` | 1 |
| 5 | 224..255 | 6 | `sys_statistics` | 1 |

Give every entry distinguishable HEAP/FSM FileIds. Decode each entry independently, compare
every byte and typed field, then run omission, duplication, adjacent swap, FileId role swap,
and cross-entry FileId reuse as separate one-defect cases.

Compute both page checksums independently with bytes `16..19` logically zero. For each page,
compare a canonical image, a one-bit payload mutation, a checksum-field mutation, and a
reserved-byte mutation. Dispatch family/version before applying v1 checksum/layout rules;
once the supported format is selected, validate its CRC before trusting dependent type-specific fields.

Ordinary CREATE, DROP, schema, index, and ANALYZE activity must produce no write, WAL record,
generation change, or checksum change in `catalog.dat`. Reopen after each operation and
compare both pages byte-for-byte with their post-creation baseline. Ordinary evolution is
present only in the six self-hosted relations.

### Built-in identities, carriers, and six descriptor fixtures

Use a literal TypeId oracle:

```text
0 invalid/not stored; 1 BOOLEAN; 2 INT32; 3 INT64; 4 FLOAT64;
5 DATE; 6 TIMESTAMP; 7 VARCHAR
```

Test every valid value, zero, `8`, the INT32 boundaries, and source-enum permutations. A
required schema-v1 descriptor containing zero, NULL/UNKNOWN, or an unlisted TypeId is catalog
corruption; the statistics-only fallback remains limited to Chapter 34's advisory grammar.

#### Mandatory carrier and sentinel matrix

| Logical domain | Physical carrier | Semantic domain / zero | Writer maximum | Negative fixture and result |
|---|---|---|---:|---|
| TableId | INT64 bit pattern | uint64; zero invalid; built-ins 1..6 | `UINT64_MAX-1` | zero or unreachable greater identity: corrupt committed row |
| IndexId | INT64 bit pattern | uint64; zero invalid; later IDs at least 7 | `UINT64_MAX-1` | zero/wrong typed owner: corruption |
| ConstraintId | INT64 bit pattern | uint64; zero invalid; later IDs at least 7 | `UINT64_MAX-1` | zero/wrong typed owner: corruption |
| normal TxnId | INT64 bit pattern | exact normal §4.3.2 domain | `MAX_ALLOCATABLE_TXN_ID` | INVALID/FROZEN/unreachable value: invalid row/generation |
| FileId | zero-extended INT64 | uint32; zero invalid | `UINT32_MAX-1` | negative SQL value, high bits, zero, or unreachable value: corruption |
| ColumnId | zero-extended INT64 | table-local uint32; zero invalid | `UINT32_MAX` | negative/high bits/zero/duplicate: corruption |
| SchemaVer | zero-extended INT64 | uint32; zero invalid | `UINT32_MAX` | zero/high bits: corruption; unavailable referenced version: required error |
| CommandId | zero-extended INT64 | uint32; zero legal | `UINT32_MAX` | negative/high bits: invalid catalog/statistics row |
| TypeId | INT32 | fixed registry 1..7; zero invalid | 7 | zero/unlisted: corruption in required descriptor |
| ordinal/enum | INT32 | field-specific nonnegative range | field-specific | negative, gap, duplicate, unknown code: corruption |
| chunk count/index | INT32 | count 1..1048576; index 0..count-1 | 1048576 / 1048575 | invalidates statistics generation |
| BOOLEAN | BOOLEAN | canonical false/true only | N/A | noncanonical carrier: malformed tuple/catalog state |
| name/blob | VARCHAR | exact bytes; names nonempty | ordinary tuple/blob limit | locale conversion, empty required name, malformed blob: owning result |

For uint64 identities, include values whose INT64 carrier appears negative and assert that
typed unsigned comparison/range checking—not signed SQL arithmetic—determines identity. A
carrier shared by two domains never authorizes cross-domain substitution.

#### Mandatory six-descriptor matrix

Each fixture sets tuple-header `schema_version=1`, literal TableId/name, no unlisted fields,
and the exact column sequence below. `NN` means nonnullable and `N` nullable.

| Relation | TableId / columns in physical and logical ordinal order | Semantic keys and references | Special fixture assertions | Status |
|---|---|---|---|---|
| `sys_tables` | 1; `table_id:I64 NN`, `namespace:V NN`, `table_name:V NN`, `heap_file_id:I64 NN`, `fsm_file_id:I64 NN`, `schema_version:I64 NN`; ColumnIds 1..6 | unique table ID, `(namespace,name)`, HEAP FileId, FSM FileId; file identities agree with TableId | namespace `main`; SchemaVer 1 for built-ins; no flags/dropped bit | exact fixture |
| `sys_columns` | 2; `table_id:I64 NN`, `schema_version:I64 NN`, `column_id:I64 NN`, `physical_ordinal:I32 NN`, `logical_ordinal:I32 NN`, `column_name:V NN`, `type_id:I32 NN`, `nullable:B NN`, `has_default:B NN`, `default_value:V N`; ColumnIds 1..10 | four `(table,version,...)` keys; visible table reference | complete 0..N-1 ordinal sets; default discriminator/blob agreement | exact fixture |
| `sys_indexes` | 3; `index_id:I64 NN`, `table_id:I64 NN`, `index_name:V N`, `btree_file_id:I64 NN`, `is_unique:B NN`, `key_schema_version:I32 NN`; ColumnIds 1..6 | unique IndexId/FileId; table and managed BTREE reference | key version 1; named standalone versus unnamed constraint-owned | exact fixture |
| `sys_index_columns` | 4; `index_id:I64 NN`, `key_ordinal:I32 NN`, `column_id:I64 NN`; ColumnIds 1..3 | unique `(index,key ordinal)` and `(index,column)`; index and same-table column refs | contiguous 0..K-1, K>=1; no repeated component | exact fixture |
| `sys_constraints` | 5; `constraint_id:I64 NN`, `table_id:I64 NN`, `constraint_kind:I32 NN`, `constraint_name:V N`, `index_id:I64 NN`; ColumnIds 1..5 | unique ConstraintId/backing IndexId; same-table index; nonnull names unique within table | kind 1 UNIQUE or 2 PK; unnamed unique backing index; PK columns nonnullable | exact fixture |
| `sys_statistics` | 6; `table_id:I64 NN`, `scope_kind:I32 NN`, `scope_id:I64 NN`, `stats_txn_id:I64 NN`, `stats_command_id:I64 NN`, `chunk_index:I32 NN`, `chunk_count:I32 NN`, `payload_fragment:V NN`; ColumnIds 1..8 | unique full row identity; TABLE/COLUMN/INDEX target rules | binary nonempty fragment; StatsVersion and chunk contract; narrow fallback only | exact fixture |

For every built-in descriptor, permute physical catalog row order while retaining explicit
ordinals and require identical reconstruction. Then remove, duplicate, or alter one
ColumnId, physical/logical ordinal, name, TypeId, NULLability, default state, or SchemaVer
and require the architecture-owned corruption result. The runtime descriptor builder is
compared with this literal fixture, not used to generate it.

Defaults use the §21.12.1 codec as a delegated byte oracle. Test `(has_default=false,
SQL NULL)`, a valid typed `DEFAULT NULL`, and a valid folded non-NULL value. Reject false plus
a blob, true plus SQL NULL, empty/overlong/bad-CRC/reserved/type-mismatched blobs, and typed
NULL on a nonnullable column. `sys_columns.nullable=false` is the only NOT NULL catalog
authority; no hidden constraint row may appear.

For indexes and constraints, build standalone nonunique/unique indexes and normalized UNIQUE
and PRIMARY KEY metadata. Verify the constraint-owned index is unnamed, same-table, and
unique; its ordered columns come only from `sys_index_columns`; every PK column is
nonnullable; at most one PK exists. Unknown/zero constraint kind is committed catalog
corruption. CHECK and FOREIGN KEY rows are absent in schema v1 and tests do not invent their
metadata.

### Logical keys, references, ordinals, and fixed point

Use a set/multiset reference model over MVCC-visible committed catalog rows. For every exact
logical key, begin with one valid row set, then introduce one duplicate, missing member,
dangling edge, wrong-table edge, duplicate ID/name, or ordinal defect. Never accept first or
last heap encounter as authority.

#### Mandatory logical-reference matrix

| Source | Required target/ownership | Nullable? | Dangling or wrong-owner fixture | Result |
|---|---|---:|---|---|
| `sys_columns.table_id` | visible `sys_tables.table_id` | no | absent table | corrupt required catalog |
| `sys_indexes.table_id` | visible `sys_tables.table_id` | no | absent/wrong table | corrupt |
| `sys_index_columns.index_id` | `sys_indexes.index_id` | no | absent index | corrupt |
| `sys_index_columns.column_id` | column in that index's table/current indexed descriptor | no | another table's same numeric ColumnId | corrupt |
| `sys_constraints.table_id` | visible owning table | no | absent/wrong owner | corrupt |
| `sys_constraints.index_id` | same-table, unnamed, unique backing index | no | named/nonunique/other-table index | corrupt |
| selected statistics COLUMN | manifest-named ColumnId of TableId | no | old/foreign/unlisted target | reject generation/fallback |
| selected statistics INDEX | manifest-named IndexId of TableId | no | old/foreign/unlisted target | reject generation/fallback |
| bootstrap heap/FSM FileIds | matching `sys_tables` row and matching file superblocks | no | missing/duplicate/wrong kind/object/FileId | corrupt; READY forbidden |

#### Mandatory ordinal matrix

| Collection | Valid | Duplicate | Missing middle | Out of range | Shuffled heap rows |
|---|---|---|---|---|---|
| physical column ordinals | exactly 0..N-1 | corrupt | corrupt | corrupt | reconstruct by ordinal |
| logical column ordinals | exactly 0..N-1 | corrupt | corrupt | corrupt | reconstruct by ordinal |
| index key ordinals | exactly 0..K-1, K>=1 | corrupt | corrupt | corrupt | reconstruct by ordinal |
| statistics chunks | exactly 0..count-1 | invalid generation | invalid generation | invalid generation | assemble by index |

Name fixtures apply §18.4 canonicalization before persistence. Unquoted equivalent spellings
compare as the same lowercase binary name; quoted names preserve exact bytes/case. Table and
index names may share bytes because they are distinct classes. Table names, index names, and
per-version column names are unique only in their exact architectural scopes. No locale or
filesystem spelling participates.

Catalog schema v1 has no hidden persistent catalog index. Reconstruct over every heap-order
permutation and after removing any hypothetical cache/access path. Semantic keys and
references remain identical; a required user-index file named by `sys_indexes` remains a
separate required object and must pass its own identity/format checks.

Construct the positive bootstrap fixed point from three independently produced inputs: the
canonical `catalog.dat`, MVCC-visible self-hosted rows, and the literal six-descriptor oracle.
Recovery and row validation precede reconstruction. READY may proceed only when all exact
fields compare equal.

#### Mandatory fixed-point matrix

| Case | Bootstrap valid? | Rows physically/MVCC valid? | Descriptor equality | Classification | READY |
|---|---:|---:|---:|---|---:|
| exact six-relation self-description | yes | yes | yes | valid | yes, after all other gates |
| missing built-in row | yes | yes | no | committed catalog corruption | no |
| duplicate semantic row | yes | yes | no | committed catalog corruption | no |
| wrong TableId or canonical name | yes | yes | no | committed catalog corruption | no |
| wrong ColumnId/ordinal | yes | yes | no | committed catalog corruption | no |
| wrong TypeId/NULLability/default | yes | yes | no | committed catalog corruption | no |
| wrong relation/table SchemaVer | yes | yes | no | corruption or unsupported selector per field | no |
| wrong heap/FSM FileId, kind, basename, or object owner | yes | yes | no | required-object corruption/missing | no |
| wrong constraint/index relation or hidden catalog index | yes | yes | no | committed catalog corruption | no |

Neither the built-in side nor self-hosted rows win a mismatch. No row is synthesized and no
arbitrary duplicate is selected. Validation proceeds in Chapter 16's order: physical tuple,
scalar/field, then cross-row/fixed-point checks. Instrument each layer and assert that a
failure prevents later descriptor/cache publication.

### Database creation, open, recovery, and READY

Compose the §41.3 root-lifecycle harness with Chapter 16. Persist only the named prefix,
reopen through the ordinary owner, and record whether a final root is authoritative,
recovery work is required, fixed-point validation runs, cleanup/adoption owns residue, and
READY may publish.

#### Mandatory creation/open crash matrix

| Persisted boundary | Root authoritative? | Required action | READY |
|---|---:|---|---:|
| before system files initialize | no | private-root cleanup/adoption owner | no |
| system files initialized; no `catalog.dat` | no live publication | private-root handling | no |
| valid `catalog.dat` and files; root private | no | private-root handling; catalog bytes do not publish root | no |
| final rename visible; parent fsync absent | uncertain | §4.7.8 adoption/reconciliation before inspection | no before reconciliation |
| parent fsync complete | yes | ordinary ownership, control selection, recovery, catalog validation | not yet |
| recovery incomplete | yes | finish redo/loser/status/file classification | no |
| recovery complete; fixed point fails | yes but invalid | corruption/unsupported result | no |
| fixed point succeeds; descriptors not published | valid opening state | complete coherent cache/descriptor publication | no |
| atomic READY publication | yes | admit ordinary consumers | yes |

Test missing `catalog.dat`, missing page 0/1, missing system HEAP/FSM file, wrong basename,
wrong FileId, wrong FileKind, wrong object ID, and a stale system page whose required WAL is
durable. The stale-page case must redo before catalog interpretation. No bootstrap/catalog
fact may be read to drive recovery before its referenced pages are recovered, and no binder,
DDL, ANALYZE, cache consumer, or DML operation may run before READY.

The positive database-creation fixture independently verifies allocations 1..6 in relation
order, `next_catalog_object_id=7`, unique system FileIds, exact FROZEN seed headers
`(xmin=1,cmin=0,xmax=0,cmax=0)`, all startup-critical syncs, complete bootstrap validation,
no-replace root rename, and external-parent fsync before success.

### Descriptor immutability, cache, and catalog MVCC

Treat every cache entry as a candidate requiring proof against the caller's catalog snapshot.
Obtain an old descriptor, publish a new descriptor, invalidate/replace name/current-version
entries, and retain the old object across the race. Compare all semantic fields before and
after; a runtime address or cache slot is never identity.

#### Mandatory cache/snapshot matrix

| Case | Catalog snapshot | Legal result/cache behavior | Global publication | Old descriptor free? |
|---|---|---|---|---:|
| RC statement A before DDL commit | statement A | old descriptor or snapshot-aware reconstruction | no new entry for A | no while referenced |
| RC statement B after DDL commit | fresh statement B | new eligible descriptor | committed entry legal | no while referenced |
| RR across DDL commit | retained transaction snapshot | old descriptor; new cache entry cannot override | new entry may coexist | no while referenced |
| creator SELF, later command | owner + later CommandId | transaction-local MVCC descriptor | never global before commit | no |
| other transaction before commit | its snapshot | no uncommitted descriptor | forbidden | no |
| other eligible snapshot after COMMITTED | qualifying snapshot | new descriptor/cache or authoritative reconstruction | allowed after terminal publication | old refs retained |
| DDL abort | any other transaction | aborted rows ignored; no global descriptor | forbidden | old object unchanged |
| COMMITTED before cache install | eligible snapshot | persistent rows remain authority; miss/rebuild is legal | install or coherent bypass | no reversal of commit |
| cache install fails after COMMIT | eligible snapshot | invalidate/bypass/rebuild; noncontinuable if coherence cannot be established | no partial entry | existing refs valid |
| invalidation races old reference acquisition | snapshot-tagged | old or new descriptor only when snapshot-valid | atomic semantic outcome | only after reference drains |

After durable COMMIT, inject allocation, delayed callback, and install failure. COMMIT remains
COMMITTED. The coordinator must expose the new catalog state through a valid immutable cache
entry or authoritative snapshot-aware bypass; inability to establish either exact state uses
the existing database-noncontinuable result. Never publish a partial descriptor or regress a
newer applicable entry with a delayed older install.

Destroy all runtime cache state, crash/reopen, recover, and rebuild. The resulting descriptor
set must equal the independent descriptor/fixed-point oracle. No cache invalidation record,
pointer, generation object, or SQL DDL replay is recovered.

Inject a volatile cache entry whose stable IDs/version or fields disagree with authoritative
catalog rows. It must never be returned as catalog truth. Assert the existing internal
invariant/noncontinuable classification where coherence cannot be recovered, without
reclassifying the persistent rows as corrupt merely because volatile derived state was bad.

Catalog-row visibility uses the Chapter 9/10 reference oracle. Cover RC statements around a
DDL commit, RR retention, SELF earlier/current-command boundaries, aborted creators, aborted
DROP deleters, lookup failures, and dependent `RETIRED`/INVALID/RESERVED status results. A
normal aborted row is invisible garbage rather than catalog corruption; a persisted required
row whose status dependency has already retired is the existing invariant failure.

Freeze a legally eligible committed catalog creator and normalize an eligible aborted
deleter. Descriptor reconstruction and MVCC results remain equivalent while the status
dependency disappears. Catalog row removal/reuse follows the complete Chapter 14
index/predecessor/DEAD/UNUSED/epoch/claim barriers; stable catalog IDs, never the catalog row
RID, carry semantic identity.

### Historical schemas, ColumnIds, and retirement

Build an exact `(TableId, SchemaVer)` reference map independently. Persist a tuple under V1,
publish a legal V2 fixture where the architecture permits schema evolution, and resolve both
descriptors. The old tuple, an old RR snapshot, and an old plan each continue to obtain V1;
new eligible work obtains V2. A missing, never-existent, corrupt, or wrong-table referenced
SchemaVer yields its owning required-metadata error—never current/nearest-version fallback.

#### Mandatory historical-schema matrix

| Case | ResolveSchema result | Persistent reclamation | Runtime descriptor release | File retirement |
|---|---|---|---|---|
| current tuple/current schema | exact current descriptor | no while tuple needs it | after refs | no while object live |
| old tuple after V2 commit | exact old descriptor | forbidden | after refs | forbidden if object/file needed |
| old RR snapshot | snapshot-visible old descriptor | forbidden while snapshot can require metadata | after snapshot/query | forbidden |
| old schema + live plan reference | exact immutable descriptor | persistent rows follow other dependencies | forbidden until release | forbidden if file referenced |
| no tuple but old snapshot remains | old descriptor | forbidden | after snapshot | forbidden |
| all persistent/runtime dependencies clear | no required lookup | permitted through owning vacuum/catalog protocol | permitted | permitted after complete drain |
| missing referenced SchemaVer | failure | no | N/A | no |
| removed ColumnId rebound in later version | invalid construction/corruption | no | N/A | no |
| old tuple used for index cleanup | old descriptor drives Chapter 8 key codec | forbidden until cleanup proof | retained for operation | no early retirement |

Change physical and logical positions while retaining a ColumnId in a model fixture; add or
remove another column without reusing its historical ID. Decoding, default lookup, logical
presentation, and index-key reconstruction must follow explicit fields. An index-only
metadata change leaves tuple SchemaVer unchanged. If a SQL ALTER form is outside the supported scope, the
fixture exercises the metadata/history model directly and does not claim SQL support.

Direct SQL ALTER-based column removal/reorder and object RENAME are N/A because
Chapter 16 and Chapter 21 define no such executable SQL operation. Synthetic persisted-row
and descriptor fixtures still prove the history-capable invariants; same-name DROP/CREATE
proves identity nonreuse without inventing RENAME behavior.

Compose DROP with one outstanding dependency at a time: old SQL snapshot, immutable
descriptor/query handle, OBJECT_USE claim, BufferPool pin/frame/file owner, historical tuple
or index cleanup, and namespace durability. Unlink is forbidden while any required barrier
remains. After all barriers clear, retirement may proceed through §§7.12.5, 4.7.7, 14.17.1,
and 21.9; FileId and semantic object IDs remain consumed. Recreating the same canonical name
must allocate new identities and cannot rebind an old descriptor.

### Statistics catalog integration

Chapter 34 owns payload bytes, numerical validity, and generation selection; this section
owns their catalog-row envelope and composition with Chapter 16 visibility, identity, and
fallback boundaries.

#### Mandatory statistics matrix

| Case | Outer catalog MVCC | Chunk/generation result | Fallback/selection | Core catalog result |
|---|---|---|---|---|
| valid one-chunk generation | visible committed | complete | select if greatest applicable | valid |
| valid multi-chunk generation | visible committed | assemble 0..N-1 | select if complete/applicable | valid |
| duplicate/missing/out-of-range chunk | decodable | incomplete | older valid or missing statistics | core remains valid |
| wrong StatsVersion or mixed generation | decodable | reject complete generation | older/missing | core remains valid |
| malformed known payload | decodable | invalid | older/missing + diagnostic | core remains valid |
| positive greater payload version | decodable stable family/version | unsupported advisory generation | older/missing | READY allowed |
| older complete generation available | visible | valid | select older | valid |
| no complete generation | visible/none | none | missing-statistics behavior | planning allowed |
| outer tuple aborted/invisible | not visible | not a candidate | prior/missing | ordinary MVCC |
| outer creator frozen | visible committed | identity unchanged | normal selection | valid |
| creator status retired after global publication | visible status-independent outer state | payload identity does not query status | normal selection | valid |
| target/manifest TableId, ColumnId, IndexId mismatch | visible/decodable | reject generation | older/missing | no core-descriptor weakening |

Assert exact StatsVersion `(TxnId, CommandId)` equality across outer row identity and payload,
unsigned lexicographic selection, same-TxnId/different-command and different-TxnId cases, and
normal TxnId/CommandId numeric domains. Once ordinary catalog MVCC has admitted a committed
row, the payload `stats_txn_id` is opaque generation identity and must not pin or query
transaction-status history. Freezing changes outer MVCC metadata only.

The narrow fallback starts only after ordinary page/tuple decoding yields decodable
`sys_statistics` rows. Repeat the same defect in `sys_tables`, `sys_columns`, bootstrap,
defaults, or required index metadata and require corruption/unsupported failure instead of
statistics fallback.

### CREATE, DROP, allocation, and failure composition

Use the Chapter 21 lock/publication order and §39.1 statement boundary without restating
their internal lock or WAL algorithms. Observe object/FileId allocation, private/final file
state, catalog-row publication, transaction outcome, cache state, and retirement ownership.

#### Mandatory CREATE/DROP matrix

| Case | Catalog visibility | File authority | Transaction/cache result | Retirement/identity result |
|---|---|---|---|---|
| CREATE before final-name publication | none | private/unpublished | no catalog reference allowed | IDs consumed if allocated; deterministic cleanup |
| final file published; catalog row absent | none | final durable uncommitted | pre-catalog recoverable failure may remain ACTIVE after orphan transfer | never a live object |
| catalog row published; transaction aborts | aborted/invisible | final uncommitted residue | mandatory ABORT; no global cache entry | orphan retirement; IDs consumed |
| COMMIT; cache install succeeds | visible to eligible snapshots | required file already durable | COMMITTED; immutable cache entry | IDs retained forever |
| COMMIT; cache install fails | visible through catalog | required file durable | COMMITTED; coherent bypass/rebuild or noncontinuable | no rollback/reuse |
| crash before durable COMMIT | creator recovered loser/ABORTED | private/final orphan | no SQL replay/cache recovery | classify before READY |
| crash after durable COMMIT | committed after recovery | required final file present | rebuild descriptor/cache | object remains live |
| DROP marker publishes then aborts | old row visible; xmax ineffective | file retained | no committed-drop cache invalidation | object remains LIVE |
| DROP commits with old snapshot | invisible to new, visible to old | file retained | current name removed; old descriptor valid | retirement waits |
| DROP commits with descriptor/pin | logically dropped | file retained | descriptor immutable | retirement waits |
| DROP dependencies clear | dropped | may unlink after durable drain | cache no longer authority | IDs not reused |
| crash after DROP commit before unlink | committed drop after recovery | file may remain | no resurrection | resume/defer retirement |
| same-name recreation | new visible object | new managed files | new descriptors | new object IDs and FileIds |

Directly forbid a catalog row from naming a file whose §4.7.4 final-name publication is not
complete. Inject failure before and after the first transaction-owned catalog mutation:
before it, ACTIVE is legal only after deterministic orphan ownership and attempt-only lock
release; after it, the statement must abort under §39.1. Crash before COMMIT recovers no SQL
DDL action and classifies the creator as a loser; crash after COMMIT rebuilds from persistent
catalog rows even if no cache install occurred.

The allocation specialization starts a canonical database with built-in TableIds 1..6 and
`next_catalog_object_id=7`. TableId, IndexId, and ConstraintId allocations draw from one
durably advanced uint64 high-water while remaining typed domains. Reserve an ID and crash or
abort before catalog publication; reopen must preserve the gap. Model the final legal
`UINT64_MAX-1` allocation and failing successor with widened arithmetic. Repeat FileId at
`UINT32_MAX-1`. Neither control fallback nor catalog reconstruction may regress a durable
high-water, and no dropped/aborted identity is reused.

### Format, error, and READY matrices

#### Mandatory format/error matrix

| Fixture | User error | Corruption | Unsupported | Rebuildable | Transaction/open consequence | READY | Canonical owner |
|---|---:|---:|---:|---:|---|---:|---|
| duplicate CREATE name after current-state recheck | yes | no | no | no | prepublication DDL failure | unchanged | §§21.6, 39.1 |
| missing user-requested object | yes | no | no | no | semantic lookup failure | unchanged | §21.16 |
| duplicate committed stable ID/name/ordinal | no | yes | no | no | required catalog failure | no | §16.5.10 |
| missing required system row/file | no | yes/missing-required | no | no | open/required-object failure | no | §§13.19, 16.9 |
| malformed recognized-v1 bootstrap/catalog tuple | no | yes | no | no | required format failure | no | §§4.14, 16.9.3 |
| recognized greater bootstrap/catalog version | no | no | yes | no | unsupported open | no | §§4.14.2, 16.9.3 |
| TypeId zero/unlisted in required descriptor | no | yes | no | no | descriptor publication fails | no | §§4.14.4, 16.4 |
| constraint kind outside 1/2 in schema v1 | no | yes | no | no | descriptor publication fails | no | §16.5.6 |
| committed index key version <=0 / >1 | no | <=0 | >1 | no | required index unusable | no | §§4.14.6, 16.5.4 |
| malformed/greater statistics payload after safe row decoding | no | no | advisory greater only | yes | older/missing statistics | yes if core valid | §§4.14.5, 34.14.5 |
| catalog page I/O/checksum failure | no | owning I/O/corruption | no | no | required I/O/access failure | no when required | §§3.3, 39.1 |
| object/FileId exhaustion | no | no | no | no | DDL fails before publication | unchanged | §§4.3.2, 13.2 |
| cache install failure after COMMIT | no | no | no | derived cache bypass/rebuild | COMMITTED remains; NC if incoherent | yes if coherent | §§21.10, 39.1 |
| orphan unlink/directory-sync failure | no | no | no | retryable physical residue | semantic outcome retained | owner-dependent | §§4.7.7, 21.5/21.9 |
| shutdown/cancellation | no | no | no | no | existing lifecycle/FA-MA rule | no new admission | §§3.3.6, 39.1 |
| uncertain required namespace/WAL/cache authority | no | no | no | no | database noncontinuable | no | §§3.3.5, 39.1 |

The READY prohibition oracle is the explicit conjunction of valid supported control/WAL
recovery, exact `catalog.dat`, all required system files and owners, decodable required
catalog tuples, complete logical keys/references/ordinals, exact bootstrap fixed point,
resolvable required historical schemas, coherent derived-cache publication, and resolved
orphan/retirement classification where the lifecycle owner requires it. Remove one conjunct
at a time and assert that no ordinary admission event follows. The all-valid fixture must
reach one atomic READY event and admit ordinary work only afterward.

#### Mandatory cross-chapter composition matrix

| Architecture owner | Existing verification reused | Chapter-16 specialization |
|---|---|---|
| Chapter 3 | lifecycle/READY tests | no catalog consumer before validated fixed point |
| Chapter 4 | superblock, CRC, format, root publication, FileId | CATALOG/file/page exact bytes and required-file identity |
| Chapter 5 | tuple codec/SchemaVer | six relation headers and historical interpretation |
| Chapter 7 | pins/frames/file drain | DROP descriptor/file retirement conjunction |
| Chapter 8 | key codec/MTR/index identity | historical descriptor key reconstruction and required index owner |
| Chapter 9 | TxnId/CommandId/snapshots | catalog RC/RR and StatsVersion carriers |
| Chapter 10 | MVCC/SELF/status failures | catalog-row visibility specialization |
| Chapter 11 | UNIQUE and unified gates | normalized metadata plus Chapter 21 lock composition |
| Chapter 12 | WAL publication/WAL-before-data | catalog mutation boundary; no `catalog.dat` DDL WAL |
| Chapter 13 | control allocators/recovery | built-in high-water and recovery-before-decode |
| Chapter 14 | freezing/history/RID/file claims | historical metadata and catalog-row reclamation |
| Chapter 15 | DML statement publication/no undo | internal catalog mutation failure boundary |
| Chapter 16 | catalog contract | all direct fixtures/matrices in this section |
| Chapter 21 | DDL lock/file/catalog/cache protocol | CREATE/DROP persistent-state composition |
| Chapter 34 | statistics payload/selection | `sys_statistics` row/chunk/MVCC integration |
| §39.1 | FA/MA/NC and durable COMMIT | cache/orphan/catalog failure outcomes |
| §41.4 | broad catalog obligations | exact byte, fixed-point, cache, history, DDL procedures |

#### High-level catalog domain/case matrix

| Fixture | Deterministic fault/order | Independent oracle | Owner |
|---|---|---|---|
| canonical `catalog.dat` | exact two-page image | byte + CRC | §§16.9, 4.10 |
| reserved byte / future version | one-bit or discriminator change | format classifier | §§4.14, 16.9.3 |
| each canonical descriptor | literal schema row | descriptor oracle | §§16.4–16.5 |
| invalid carrier/TypeId | one boundary value | widened typed-domain oracle | §§4.3.2, 16.5.1 |
| duplicate ordinal/reference | one row defect | set/ordinal oracle | §§16.5.2–16.5.10 |
| fixed-point mismatch | one descriptor field | independent equality | §16.5.9 |
| missing/wrong system file | omit or alter owner | durable file manifest | §§16.5.2, 16.9 |
| canonical open | release recovery/fixed-point/READY barriers | READY conjunction | §§3.3, 13.19, 16.9.4 |
| RC/RR cache | DDL commit between lookups | MVCC snapshot oracle | §§16.10, 21.3 |
| post-COMMIT cache failure | fail install after terminal publication | persistent catalog authority | §§16.10, 21.10, 39.1 |
| old tuple after schema change | retain V1 tuple while V2 publishes | `(TableId,SchemaVer)` map | §§16.7–16.8 |
| vacuum/old descriptor | clear dependencies one at a time | retention conjunction | §§14.17.1, 16.7 |
| same-name recreation | DROP then CREATE after legal name reuse | stable-ID history | §§16.2–16.3, 21.9 |
| CREATE abort/crash | persist selected file/catalog prefix | DDL publication oracle | §§21.5–21.6, 39.1 |
| DROP old snapshot/crash | commit deletion before unlink | MVCC + retirement oracle | §§16.10, 21.9 |
| malformed statistics | one chunk/payload/manifest defect | Chapter 34 generation model | §§16.5.7, 34.14 |
| catalog-object crash gap | durable high-water, no row | allocator arithmetic | §§13.2.6, 16.5.1 |

### Chapter 16 atomic architecture-obligation coverage map

`Catalog Tests` in this section means the Chapter-16-specialized procedures and matrices in this
section. A referenced lower-layer section supplies the reusable deterministic mechanism;
the listed catalog procedure supplies the Chapter-16 composition. Every row is required.

| ID | Domain | Atomic obligation | Architecture owner | Verification procedure |
|---:|---|---|---|---|
| 1 | A | Catalog is semantic metadata between storage and SQL | §16.1 | descriptor/boundary assertions |
| 2 | A | Parser does not consume physical catalog storage | §16.1 | front-end dependency fixture |
| 3 | A | Binder resolves semantic IDs without choosing physical algorithms | §16.1 | binder/catalog boundary fixture |
| 4 | A | Descriptors expose no heap/B+/BufferPool implementation state | §16.1 | descriptor field oracle |
| 5 | B | namespace is exactly `main` | §16.2 | name namespace fixtures |
| 6 | B | Qualified and unqualified `main` lookup agree | §16.2 | binder name fixture |
| 7 | B | Live table names are unique | §16.2 | logical-key matrix |
| 8 | B | Live named indexes are unique | §16.2 | logical-key matrix |
| 9 | B | Column names are unique per table schema version | §16.2 | logical-key matrix |
| 10 | B | Table and index names are separate classes | §16.2 | same-bytes cross-class fixture |
| 11 | C | TableId is uint64 semantic identity | §16.3 | carrier/identity oracle |
| 12 | C | IndexId is uint64 semantic identity | §16.3 | carrier/identity oracle |
| 13 | C | ConstraintId is uint64 semantic identity | §16.3 | carrier/identity oracle |
| 14 | C | ColumnId is uint32 table-local identity | §16.3 | carrier/ColumnId fixtures |
| 15 | C | TypeId is uint32 fixed-registry identity | §§16.3–16.4 | TypeId fixture |
| 16 | C | Bound references use stable IDs rather than names | §16.3 | recreation/name-rebinding fixture |
| 17 | C | Catalog IDs differ from BindingId/slots/positions | §16.3 | typed-domain negative fixtures |
| 18 | D | TableId/IndexId/ConstraintId share one allocator | §§13.2.6, 16.3 | allocation sequence model |
| 19 | D | Typed wrappers remain distinct in shared allocator | §§13.2.6, 16.3 | cross-domain substitution negatives |
| 20 | D | Shared allocator never returns zero | §§13.2.6, 16.3 | zero boundary fixture |
| 21 | D | Durable high-water advances before returning an ID | §13.2.6 | control allocation prefix |
| 22 | D | Crash gaps are legal and retained | §§13.2.6, 16.3 | object-ID crash-gap fixture |
| 23 | D | Any possibly persisted object ID is never reused | §§13.2.6, 16.3 | abort/drop/reopen fixture |
| 24 | D | Final catalog-object allocation is `UINT64_MAX-1` | §§4.3.2, 13.2.6 | widened terminal arithmetic |
| 25 | D | Catalog-object allocation never wraps | §§4.3.2, 13.2.6 | failing-successor fixture |
| 26 | E | Required FileIds are nonzero | §§4.3.2, 16.5.1 | carrier/sentinel matrix |
| 27 | E | FileId is distinct from catalog-object domains | §§4.3, 16.5.1 | typed-domain negatives |
| 28 | E | Managed filenames agree with catalog IDs and FileSuperblocks | §§4.7, 16.5.2, 16.5.4 | required-file identity fixture |
| 29 | E | FileIds consumed by failed/aborted DDL are not reused | §§4.7, 13.2.5, 21.5 | DDL allocation/reopen fixture |
| 30 | E | Final FileId allocation is `UINT32_MAX-1` without wrap | §§4.3.2, 13.2.5 | widened terminal arithmetic |
| 31 | F | CREATE assigns ColumnIds 1..N | §16.3 | literal descriptor fixture |
| 32 | F | ColumnId zero is invalid | §§4.3.2, 16.3 | sentinel fixture |
| 33 | F | ColumnIds never derive from row/ordinal position | §§16.3, 16.8 | shuffled/changed-position fixture |
| 34 | F | Historical ColumnIds are not reused | §§16.3, 16.7 | historical schema fixture |
| 35 | F | ColumnId exhaustion rejects schema before publication | §4.3.2 | model boundary fixture |
| 36 | G | TypeIds 1..7 map exactly to built-in types | §16.4 | literal TypeId registry |
| 37 | G | TypeId zero is invalid/not stored | §16.4 | zero fixture |
| 38 | G | NULL/UNKNOWN are not persisted column TypeIds | §16.4 | descriptor negative fixture |
| 39 | G | TypeIds do not derive from source enum order | §16.4 | permuted source-registry fixture |
| 40 | H | `catalog_schema_version=1` selects exact six descriptors | §16.5 | selector/descriptor oracle |
| 41 | H | Catalog schema selector differs from table SchemaVer | §16.5 | selector substitution negative |
| 42 | H | Every built-in tuple header has SchemaVer 1 | §16.5 | tuple/descriptor fixture |
| 43 | H | Catalog schema version zero is corruption | §§4.14.2, 16.5 | version matrix |
| 44 | H | Greater catalog schema version is unsupported | §§4.14.2, 16.5 | version matrix |
| 45 | H | A newer catalog is never decoded as | §§4.14.2, 16.5 | dispatch-stop assertion |
| 46 | I | uint64 domains persist unchanged INT64 bit patterns | §16.5.1 | signed/unsigned carrier fixtures |
| 47 | I | uint32 domains persist zero-extended in INT64 | §16.5.1 | high-bit/range fixtures |
| 48 | I | Type/ordinal/enum/count domains use nonnegative INT32 | §16.5.1 | carrier matrix |
| 49 | I | BOOLEAN catalog fields use canonical BOOLEAN | §16.5.1 | tuple scalar fixture |
| 50 | I | Names/blobs use ordinary binary VARCHAR | §§16.5.1, 17.4.6 | binary-byte fixture |
| 51 | I | Shared carrier does not merge typed domains | §16.5.1 | cross-domain substitution fixture |
| 52 | I | Signed INT64 order is not unsigned identity order | §16.5.1 | high-bit identity fixture |
| 53 | I | Writer maxima are enforced after typed decode | §§4.3.2, 16.5.1 | carrier boundary oracle |
| 54 | I | Names are nonempty binder-canonical bytes | §§16.5.1, 18.4 | canonical name fixtures |
| 55 | I | No separate identifier-length rule bypasses tuple size | §16.5.1 | tuple-size boundary mapping |
| 56 | J | `sys_tables` has exact TableId/name/schema | §§16.5.1–16.5.2 | six-descriptor matrix |
| 57 | J | `sys_tables` has exact six columns/IDs/order/types/nullability | §16.5.2 | literal descriptor fixture |
| 58 | J | `table_id` is semantically unique | §16.5.2 | logical-key matrix |
| 59 | J | `(namespace,table_name)` is semantically unique | §16.5.2 | logical-key matrix |
| 60 | J | HEAP and FSM FileIds are each unique and role-distinct | §16.5.2 | file-reference matrix |
| 61 | J | Managed HEAP/FSM identity matches TableId | §16.5.2 | wrong-owner fixture |
| 62 | J | Six built-in rows use bootstrap FileIds and SchemaVer 1 | §16.5.2 | fixed-point oracle |
| 63 | J | DROP uses MVCC/retirement, not flags | §16.5.2 | DROP matrix/field absence |
| 64 | K | `sys_columns` has exact TableId/name/schema | §§16.5.1, 16.5.3 | six-descriptor matrix |
| 65 | K | `sys_columns` has exact ten columns/IDs/order/types/nullability | §16.5.3 | literal descriptor fixture |
| 66 | K | Rows form a complete descriptor per `(TableId,SchemaVer)` | §16.5.3 | completeness fixture |
| 67 | K | Physical ordinals are exactly 0..N-1 | §16.5.3 | ordinal matrix |
| 68 | K | Logical ordinals are exactly 0..N-1 | §16.5.3 | ordinal matrix |
| 69 | K | ColumnId/name/physical/logical keys are unique | §16.5.3 | logical-key matrix |
| 70 | K | Every column row references a visible table | §16.5.3 | reference matrix |
| 71 | K | Current schema rows reconstruct completely | §16.5.3 | descriptor oracle |
| 72 | K | No-default is false plus SQL NULL | §16.5.3 | default matrix |
| 73 | K | Typed DEFAULT NULL is distinct and valid only when nullable | §16.5.3 | DefaultValueBlob fixture |
| 74 | K | Non-NULL default blob is canonical and type-matched | §§16.5.3, 21.12.1 | delegated blob codec + row fixture |
| 75 | K | Invalid discriminator/blob/nullability combinations are corruption | §16.5.3 | single-defect defaults |
| 76 | K | Built-in self-description has no defaults | §16.5.3 | fixed-point fixture |
| 77 | L | `sys_indexes` has exact TableId/name/schema | §§16.5.1, 16.5.4 | six-descriptor matrix |
| 78 | L | `sys_indexes` has exact six columns/IDs/order/types/nullability | §16.5.4 | literal descriptor fixture |
| 79 | L | IndexId and BTREE FileId are each unique | §16.5.4 | logical-key matrix |
| 80 | L | Index row references one existing table | §16.5.4 | reference matrix |
| 81 | L | Managed BTREE file identity/fingerprint agrees | §§8.2–8.3, 16.5.4 | index identity fixture |
| 82 | L | Key schema version 1 is required | §16.5.4 | format matrix |
| 83 | L | Nonpositive key version is corruption; greater is unsupported | §§4.14.6, 16.5.4 | format matrix |
| 84 | L | Named index is standalone and unique in index-name class | §16.5.4 | name/ownership fixture |
| 85 | L | NULL index name means exactly one constraint owner | §16.5.4 | constraint normalization fixture |
| 86 | L | `is_unique` and constraint kind are separate authorities | §16.5.4 | cross-check negative fixture |
| 87 | M | `sys_index_columns` exact descriptor is fixed | §16.5.5 | six-descriptor matrix |
| 88 | M | Key ordinals are exactly 0..K-1 with K>=1 | §16.5.5 | ordinal matrix |
| 89 | M | `(IndexId,key_ordinal)` is unique | §16.5.5 | logical-key matrix |
| 90 | M | `(IndexId,ColumnId)` is unique | §16.5.5 | repeated-component fixture |
| 91 | M | Index reference exists | §16.5.5 | reference matrix |
| 92 | M | Column belongs to indexed table/current descriptor | §16.5.5 | wrong-table fixture |
| 93 | M | Reconstruction sorts by key ordinal, not row order | §16.5.5 | permutation fixture |
| 94 | M | the architecture rejects unsupported key-component forms and oversized keys | §§8.6, 16.5.5 | descriptor creation negatives |
| 95 | N | `sys_constraints` exact descriptor is fixed | §16.5.6 | six-descriptor matrix |
| 96 | N | Kind 1 is UNIQUE and kind 2 PRIMARY_KEY | §16.5.6 | literal kind registry |
| 97 | N | Any other schema-v1 kind is corruption | §16.5.6 | format/error matrix |
| 98 | N | CHECK/FK have no schema-v1 rows | §16.5.6 | forbidden-row fixture |
| 99 | N | NOT NULL authority is `sys_columns.nullable=false` | §16.5.6 | metadata normalization fixture |
| 100 | N | ConstraintId and backing IndexId are unique | §16.5.6 | logical-key matrix |
| 101 | N | Non-NULL constraint names are per-table unique | §16.5.6 | name fixture |
| 102 | N | Constraint references same-table unnamed unique index | §16.5.6 | reference matrix |
| 103 | N | Constraint columns come only from index-column rows | §16.5.6 | duplicate-source negative |
| 104 | N | At most one PK exists per table | §16.5.6 | duplicate-PK fixture |
| 105 | N | Every PK component is nonnullable | §§16.5.6, 21.7 | nullable-PK fixture |
| 106 | N | Standalone unique index has no constraint owner | §16.5.6 | ownership fixture |
| 107 | O | `sys_statistics` exact descriptor is fixed | §16.5.7 | six-descriptor matrix |
| 108 | O | Statistics row identity includes table/scope/StatsVersion/chunk | §16.5.7 | statistics matrix |
| 109 | O | StatsVersion is exact `(TxnId,CommandId)` | §§16.5.7, 34.3 | identity fixture |
| 110 | O | Original outer `xmin/cmin` equals StatsVersion | §16.5.7 | insertion fixture |
| 111 | O | Later freezing does not alter StatsVersion columns | §16.5.7 | freeze/reload fixture |
| 112 | O | Globally admitted StatsVersion does not pin status history | §§14.14.3, 16.5.7, 34.3.1 | status-retirement fixture |
| 113 | O | Chunk count/index domains are exact | §§4.3.2, 16.5.7 | carrier/ordinal matrix |
| 114 | O | Nonfinal/final fragment lengths are exact | §16.5.7 | chunk boundary fixtures |
| 115 | O | Chunks are contiguous and never mixed across versions | §16.5.7 | statistics defect matrix |
| 116 | O | TABLE/COLUMN/INDEX scope identity is exact | §16.5.7 | target/manifest fixtures |
| 117 | O | Unknown scope invalidates only advisory generation | §16.5.7 | narrow-fallback fixture |
| 118 | O | Malformed statistics row framing remains core corruption | §§4.14.5, 16.5.7 | outer-framing negative |
| 119 | O | Valid prior generation or missing-statistics fallback is selected | §§16.5.7, 34.14 | selection fixture |
| 120 | P | All core cross-row references are validated | §16.5.8 | logical-reference matrix |
| 121 | P | Current core rows reference current committed objects | §16.5.8 | current-owner fixture |
| 122 | P | Stale statistics do not become core constraints | §16.5.8 | DDL/manifest fallback fixture |
| 123 | P | Catalog meaning is independent of heap row order | §16.5.8 | row permutation fixture |
| 124 | P | Column/index/statistics collections sort by explicit ordinal | §16.5.8 | ordinal matrix |
| 125 | Q | Schema v1 has no hidden persistent catalog indexes | §16.5.8 | access-path absence fixture |
| 126 | Q | Semantic uniqueness holds without a physical catalog index | §16.5.8 | heap-scan permutation fixture |
| 127 | Q | No hidden built-in IndexId/bootstrap root exists | §§16.5.1, 16.5.8 | allocation/bootstrap manifest fixture |
| 128 | R | Built-in decoder is the only schema-v1 bootstrap decoder | §16.5.9 | fixed-point positive fixture |
| 129 | R | Reconstructed descriptors exactly equal built-ins | §16.5.9 | independent fixed-point oracle |
| 130 | R | Bootstrap entry identity/FileIds cross-check self-hosted rows | §16.5.9 | fixed-point field matrix |
| 131 | R | Self-description includes every exact built-in column | §16.5.9 | missing/field mismatch fixtures |
| 132 | R | No bootstrap-only index/constraint rows exist | §16.5.9 | hidden-row negative |
| 133 | R | Initial statistics relation is empty | §16.5.9 | creation fixture |
| 134 | R | Every fixed-point mismatch is corruption and blocks READY | §16.5.9 | fixed-point matrix |
| 135 | S | Tuple physical validation precedes catalog-field validation | §16.5.10 | validation event order |
| 136 | S | Field validation precedes cross-row reconstruction | §16.5.10 | validation event order |
| 137 | S | Duplicate/missing/unknown core state is corruption | §16.5.10 | single-defect matrices |
| 138 | S | Internal writers validate before cache publication | §16.5.10 | prepublication fault fixture |
| 139 | S | Required corruption after READY is noncontinuable | §§3.3, 16.5.10 | online detection fixture |
| 140 | T | Cache reconstruction reads one committed catalog snapshot | §16.5.10 | snapshot/cache oracle |
| 141 | T | Reconstruction groups by typed identity and explicit ordinal | §16.5.10 | permutation fixture |
| 142 | T | File/built-in cross-checks precede complete cache publication | §16.5.10 | publication barrier |
| 143 | T | No partial replacement cache publishes | §16.5.10 | one-defect publication fixture |
| 144 | T | Cache can be rebuilt deterministically after restart | §§16.5.10, 16.10 | cache destruction/reopen fixture |
| 145 | U | Canonical CREATE TABLE rows match normalized schema | §16.5.11 | creation-trace fixture |
| 146 | U | Canonical UNIQUE/PK rows cross-check all authorities | §16.5.11 | constraint creation fixture |
| 147 | U | Standalone index has no constraint row | §16.5.11 | index creation fixture |
| 148 | U | ANALYZE writes one complete manifest/chunk family | §16.5.11 | statistics creation fixture |
| 149 | U | DROP removes current authorities transactionally | §16.5.11 | DROP matrix |
| 150 | V | No alternative built-in IDs/order/source-struct layout is accepted | §16.5.12 | forbidden-interpretation negatives |
| 151 | V | No default/NULL or column/order inference shortcut is accepted | §16.5.12 | descriptor/default negatives |
| 152 | V | No duplicate PK/index authority or hidden catalog index is accepted | §16.5.12 | normalization negatives |
| 153 | V | Unknown codes/flags are not forward-compatible data | §§4.14, 16.5.12 | format matrix |
| 154 | V | Cache cannot publish before complete validation | §16.5.12 | publication barrier |
| 155 | W | TableDescriptor contains exact stable table/file/schema metadata | §16.6 | descriptor field oracle |
| 156 | W | SchemaDescriptor identity is `(TableId,SchemaVer)` | §16.6 | descriptor equality fixture |
| 157 | W | IndexDescriptor contains exact table/file/key/constraint metadata | §16.6 | descriptor field oracle |
| 158 | W | Published descriptors are immutable | §§16.6, 16.10 | invalidation/reference race |
| 159 | W | Derived/precomputed layout cannot alter descriptor meaning | §16.6 | two-representation differential |
| 160 | W | Runtime address is not descriptor identity | §16.6 | rebuild-at-new-address fixture |
| 161 | X | ResolveSchema returns exact historical descriptor | §16.7 | historical-schema matrix |
| 162 | X | Tuple SchemaVer selects tuple interpretation | §§5.13, 16.7 | old-tuple fixture |
| 163 | X | Vacuum resolves historical schema before old-key decode | §16.7 | Chapter 8/14 composition |
| 164 | X | Required historical metadata cannot be reclaimed early | §16.7 | dependency-at-a-time fixture |
| 165 | X | Schema-changing DDL monotonically increases SchemaVer | §16.7 | checked-next model fixture |
| 166 | X | Index-only metadata change need not alter tuple SchemaVer | §16.7 | index-only change fixture |
| 167 | X | Unsupported rewrite DDL rejection does not weaken history model | §16.7 | metadata-model fixture |
| 168 | X | Missing referenced historical version is never substituted | §16.7 | missing-version negative |
| 169 | Y | ColumnId differs from physical/logical position | §16.8 | changed-position fixture |
| 170 | Y | `SELECT *` follows logical presentation order | §16.8 | binder/descriptor fixture |
| 171 | Y | Old tuples remain interpretable across position evolution | §§16.7–16.8 | historical decode fixture |
| 172 | Z | `catalog.dat` page 0 is CATALOG FileSuperblock | §§16.9, 16.9.1 | byte matrix |
| 173 | Z | CATALOG superblock uses kind 4/object 0/header 72 | §16.9.1 | specialized superblock fixture |
| 174 | Z | Catalog FileId is ordinary nonzero durable identity | §16.9.1 | carrier/file fixture |
| 175 | Z | CATALOG superblock reserved/trailing bytes and CRC are exact | §§4.10, 16.9.1 | byte/CRC matrix |
| 176 | AA | `catalog.dat` is exactly two 8192-byte pages | §16.9 | exact-file fixture |
| 177 | AA | Page 1 is exact CATALOG_DATA common header | §16.9.2 | byte matrix |
| 178 | AA | `DBLUSCAT`, versions, count, generation are exact | §16.9.2 | byte matrix |
| 179 | AA | Six entries occupy bytes 64..255 | §16.9.2 | entry oracle |
| 180 | AA | Entries occur once in relation-code order 1..6 | §16.9.2 | entry permutation negatives |
| 181 | AA | Every entry TableId equals relation code | §16.9.2 | identity fixture |
| 182 | AA | Every entry has unique nonzero HEAP/FSM FileIds | §16.9.2 | entry/file fixture |
| 183 | AA | Entry flags/reserved bytes are zero | §16.9.2 | single-bit negatives |
| 184 | AA | Bytes 256..8191 are zero | §16.9.2 | zero-suffix fixture |
| 185 | AA | All bootstrap integers are little-endian | §16.9.2 | byte-swapped negatives |
| 186 | AB | Bootstrap page checksum is mandatory from creation | §16.9.3 | independent CRC fixture |
| 187 | AB | Every reserved region is validated | §§4.14.3, 16.9.3 | reserved-byte matrix |
| 188 | AB | exact versions are accepted | §16.9.3 | format matrix |
| 189 | AB | Zero/malformed v1 is corruption | §§4.14.2, 16.9.3 | format matrix |
| 190 | AB | Recognized greater bootstrap/catalog/relation version is unsupported | §§4.14.2, 16.9.3 | format matrix |
| 191 | AB | Unsupported format is not parsed through v1 grammar | §§4.14.2, 16.9.3 | dispatch-stop assertion |
| 192 | AC | Built-in decoder contains only exact schema-v1 facts | §16.9.4 | fixed-point oracle |
| 193 | AC | Open validates `catalog.dat` before bootstrap decode | §16.9.4 | open event order |
| 194 | AC | System files recover before catalog row decode | §16.9.4 | stale-page redo fixture |
| 195 | AC | Self-description cross-check precedes cache publication | §16.9.4 | open event order |
| 196 | AC | No ordinary catalog consumer runs before READY | §16.9.4 | admission barriers |
| 197 | AC | Bootstrap mismatch prevents normal open | §16.9.4 | fixed-point matrix |
| 198 | AD | Creation allocates built-ins first and high-water 7 | §16.9.5 | creation/allocator fixture |
| 199 | AD | Creation seeds exact FROZEN committed tuple headers | §16.9.5 | byte-exact tuple fixture |
| 200 | AD | Complete startup-critical files/directories are synchronized | §§4.7.8, 16.9.5 | creation prefix matrix |
| 201 | AD | Root rename is no-replace | §§4.7.8, 16.9.5 | lifecycle harness |
| 202 | AD | Parent-directory fsync precedes create success | §§4.7.8, 16.9.5 | root publication barriers |
| 203 | AD | Incomplete creation is not an open target | §16.9.5 | crash-prefix matrix |
| 204 | AD | `catalog.dat` is immutable after creation | §16.9.5 | post-DDL byte/WAL comparison |
| 205 | AD | Bootstrap changes require explicit new format/migration | §16.9.5 | ordinary-DDL mutation absence |
| 206 | AE | Cache is derived and nonauthoritative | §16.10 | destroy/rebuild fixture |
| 207 | AE | Cache keys use stable IDs/versions and qualified names | §16.10 | key/identity fixture |
| 208 | AE | Cache hit must be proven visible to caller snapshot | §16.10 | cache/snapshot matrix |
| 209 | AE | Older descriptors remain alive while referenced | §16.10 | invalidation/reference race |
| 210 | AE | Ownership mechanism remains implementation-specific | §16.10 | observable-lifetime-only assertions |
| 211 | AF | RC uses statement-qualified catalog visibility | §§16.10, 21.3 | RC cache matrix |
| 212 | AF | RR retains transaction-qualified catalog visibility | §§16.10, 21.3 | RR cache matrix |
| 213 | AF | New cache entries cannot override old snapshots | §16.10 | old-snapshot/new-cache race |
| 214 | AF | Cache miss may reconstruct without changing semantics | §16.10 | cache-off differential |
| 215 | AG | Owner sees completed earlier DDL through SELF/CommandId | §§10, 16.10, 21.3 | SELF catalog fixture |
| 216 | AG | Other transactions never see uncommitted DDL | §§16.10, 21.3 | concurrent visibility fixture |
| 217 | AG | Uncommitted DDL is never globally cached | §16.10 | aborted/uncommitted cache fixture |
| 218 | AH | Current cache entries change only after terminal publication | §16.10 | COMMIT barrier |
| 219 | AH | Committed descriptor publication is complete/immutable | §§16.10, 21.10 | install observer |
| 220 | AH | Aborted DDL publishes no global descriptor | §§16.10, 21.10 | abort fixture |
| 221 | AH | Delayed older install cannot regress current identity | §§16.10, 34.15 | delayed callback race |
| 222 | AI | Cache install failure cannot reverse durable COMMIT | §§16.10, 21.10, 39.1 | post-COMMIT fault |
| 223 | AI | Coherent invalidate/bypass/rebuild preserves catalog authority | §§16.10, 21.10 | cache failure fixture |
| 224 | AI | Inability to establish cache coherence is noncontinuable | §§21.10, 39.1 | failure matrix |
| 225 | AJ | DDL allocates IDs only during execution under exclusivity | §§21.2, 21.4 | binding/execution event trace |
| 226 | AJ | Required files are final-name durable before catalog reference | §§21.5–21.6 | CREATE publication oracle |
| 227 | AJ | Filesystem existence alone is not committed catalog authority | §21.5 | file-only prefix fixture |
| 228 | AJ | Pre-catalog failure transfers deterministic orphan ownership | §§21.5, 39.1 | failure prefix |
| 229 | AJ | Post-catalog failure mandates ABORT | §§21.5, 39.1 | failure prefix |
| 230 | AK | CREATE revalidates name/current state before allocation | §21.6 | DDL race fixture |
| 231 | AK | CREATE publishes exact schema-v1 rows | §§16.5.11, 21.6 | creation trace |
| 232 | AK | CREATE retains DDL ownership to terminal outcome | §§21.2, 21.6 | gate/terminal barriers |
| 233 | AK | CREATE abort leaves invisible rows and orphan files | §§21.5–21.6 | abort/reopen fixture |
| 234 | AK | Durable CREATE survives crash without SQL replay | §§13.20, 21.6 | post-COMMIT crash fixture |
| 235 | AL | DROP publishes MVCC deletion, not a dropped bit | §§16.5.2, 21.9 | DROP matrix |
| 236 | AL | Aborted DROP leaves deletion ineffective/object live | §§10.3, 21.9 | abort fixture |
| 237 | AL | Committed DROP hides object from new snapshots | §21.9 | snapshot fixture |
| 238 | AL | Old snapshots/descriptors retain dropped object | §§16.10, 21.9 | retention fixture |
| 239 | AL | File unlink waits for snapshots/descriptors/page/file owners | §§7.12.5, 21.9 | one-barrier-missing matrix |
| 240 | AL | Unlink/fsync failure does not reverse committed DROP | §§4.7.7, 21.9 | retirement failure fixture |
| 241 | AL | Crash after DROP commit does not resurrect object | §§13.20, 21.9 | post-commit/pre-unlink prefix |
| 242 | AM | Same-name recreation receives new catalog-object identity | §§16.2–16.3, 21.9 | recreation fixture |
| 243 | AM | Recreated physical object receives new FileId and cannot alias old descriptor | §§4.7, 16.10 | recreation/file fixture |
| 244 | AN | New tuple-interpreting schema monotonically advances SchemaVer | §16.7 | checked-next model |
| 245 | AN | SchemaVer never wraps or overwrites historical identity | §§4.3.2, 16.7 | terminal boundary model |
| 246 | AN | unsupported ALTER forms remain unsupported | §§4.3.2, 16.7, 21.19 | no-SQL-support assertion |
| 247 | AO | Catalog rows use ordinary heap MVCC | §§16.5.10, 21.3 | catalog visibility fixtures |
| 248 | AO | RC catalog visibility follows statement snapshots | §§9.9, 21.3 | RC matrix |
| 249 | AO | RR catalog visibility follows retained snapshot | §§9.10, 21.3 | RR matrix |
| 250 | AO | SELF catalog visibility follows cmin/cmax | §§9.6, 10, 21.3 | CommandId fixture |
| 251 | AO | Aborted catalog creator is invisible garbage | §§10.2, 16.5.10 | aborted creator fixture |
| 252 | AO | Aborted catalog deleter is ineffective | §§10.3, 21.9 | aborted DROP fixture |
| 253 | AO | Cache cannot alter any MVCC result | §16.10 | cache-on/off differential |
| 254 | AP | Dependent RETIRED/INVALID/RESERVED status is an invariant failure | §§10, 14.14.3 | status negative fixture |
| 255 | AP | Legal creator freezing preserves descriptor semantics | §§10.5, 14.13, 16.5.7 | freeze/reconstruct fixture |
| 256 | AP | Legal aborted-xmax normalization preserves catalog visibility | §§10.5, 14.13 | normalize/reconstruct fixture |
| 257 | AP | Opaque StatsVersion does not retain status pages | §§14.14.3, 16.5.7 | status cutoff fixture |
| 258 | AQ | Persisted old tuple retains historical schema metadata | §§14.17.1, 16.7 | historical retention matrix |
| 259 | AQ | Live query/descriptor reference retains runtime descriptor | §§16.7, 16.10 | reference barrier |
| 260 | AQ | Vacuum uses old descriptor for exact index cleanup | §§14.15, 16.7 | historical key fixture |
| 261 | AQ | Metadata may reclaim only after all persistent/runtime dependencies clear | §§14.17.1, 16.7 | conjunction oracle |
| 262 | AQ | Catalog row RID reuse follows complete Chapter 14 barriers | §§14.6–14.12, 16.11 | reclamation composition |
| 263 | AR | File retirement blocks new fetch/pin and drains frames | §§7.12.5, 21.9 | BufferPool retirement fixture |
| 264 | AR | Cached descriptor alone is not a lifetime/publication claim | §§14.17.1, 16.10 | object-claim negative |
| 265 | AR | Open descriptor/file owners prevent unlink | §§7.12.5, 21.9 | one-missing-barrier fixture |
| 266 | AR | Durable unlink preserves nonreuse and directory durability | §§4.7.7, 21.9 | retirement prefix fixture |
| 267 | AS | Open ownership precedes control/recovery/catalog work | §§3.3, 16.9.4 | open event order |
| 268 | AS | Recovery reconstructs system pages before decode | §§13.19, 16.9.4 | stale-page WAL fixture |
| 269 | AS | Required files and fixed point are READY prerequisites | §§13.19, 16.9.4 | READY matrix |
| 270 | AS | Unsupported required catalog state prevents READY | §§4.14.6, 16.9.3 | format/READY matrix |
| 271 | AS | Valid open publishes READY atomically after coherent cache | §§3.3.4, 16.9.4 | positive READY barrier |
| 272 | AS | Recovery rebuilds descriptors/cache without SQL or cache replay | §§13.20, 16.10 | crash/reopen fixture |
| 273 | AT | Statistics chunks integrate with outer catalog MVCC | §§16.5.7, 34.3.1 | statistics MVCC matrix |
| 274 | AT | TABLE manifest names complete exact COLUMN/INDEX members | §§16.5.7, 34.14 | manifest fixture |
| 275 | AT | Statistics rows/payload identities cross-check exactly | §§16.5.7, 34.14 | row/payload differential |
| 276 | AT | Invalid generations never mix or partly salvage | §§16.5.7, 34.14.5 | chunk defect matrix |
| 277 | AT | Statistics fallback cannot weaken core catalog validation | §§4.14.5, 16.5.7 | same-defect core/advisory contrast |
| 278 | AU | Catalog-object exhaustion is explicit prepublication failure | §§4.3.2, 13.2.6 | terminal arithmetic fixture |
| 279 | AU | FileId exhaustion is explicit prepublication failure | §§4.3.2, 13.2.5 | terminal arithmetic fixture |
| 280 | AU | ColumnId/SchemaVer exhaustion rejects schema construction | §4.3.2 | model fixture |
| 281 | AU | Resource/I/O failure cannot publish guessed metadata | §§39.1, 16.5.10 | error matrix |
| 282 | AV | Malformed recognized-v1 bootstrap is corruption | §§4.14, 16.9.3 | format matrix |
| 283 | AV | Recognized future bootstrap/catalog is unsupported | §§4.14, 16.9.3 | format matrix |
| 284 | AV | Duplicate user CREATE name differs from committed catalog corruption | §§16.5.10, 21.6 | error contrast fixture |
| 285 | AV | Missing user object differs from missing required system metadata | §§16.5.10, 21.16 | error contrast fixture |
| 286 | AV | Required catalog I/O/checksum failure prevents READY/use | §§3.3, 16.5.10 | I/O fault fixture |
| 287 | AV | Orphan cleanup failure remains retryable when ownership is coherent | §§4.7.6–4.7.7, 21.5 | cleanup fault fixture |
| 288 | AV | Uncertain required publication/coherence is noncontinuable | §§3.3.5, 39.1 | noncontinuable fixture |
| 289 | AV | No arbitrary duplicate/fallback/repair makes authoritative state usable | §§4.14, 16.5.10 | single-defect negative suite |


| Required family | Direct ownership in this section |
|---|---|
| V16-1 byte-exact `catalog.dat` | byte/entry/CRC/version matrices and immutability comparison |
| V16-2 descriptors and carriers | TypeId/carrier/default and six-descriptor fixtures |
| V16-3 logical validation/fixed point | reference/ordinal/fixed-point matrices |
| V16-4 creation/open/READY | persisted-prefix and READY conjunction matrices |
| V16-5 cache/descriptors | snapshot/cache/terminal-failure races |
| V16-6 historical schemas | retention/reclamation/key-reconstruction matrix |
| V16-7 statistics integration | catalog-envelope/chunk/MVCC/fallback matrix |
| V16-8 CREATE/DROP composition | DDL publication/crash/retirement matrix |

All catalog races and crash cases use the common deterministic harness. The procedures make
no assumption about descriptor container type, cache class, allocator data structure,
thread count, row/vector execution, or sparse-file API. They assert only architecture-
visible bytes, typed identities, snapshots, durable namespace/WAL prefixes, terminal states,
and publication/lifetime relationships.

---

## Logical Planner Tests

The complete Chapter-20 logical-planner procedure is V20-1 through V20-14 above. Retain
small SQL-to-canonical-shape integration fixtures here for no-FROM, Get/Values,
Filter/Project, INNER/LEFT/CROSS joins, Aggregate/HAVING, DISTINCT, Sort/Limit, DML hidden
values, derived tables, and scalar/EXISTS/IN children. Their expected plans come from the
V20 bound-to-logical, schema, slot, bag, join, grouping, ordering, Limit, and subquery
oracles. They do not rebind names, use EXPLAIN strings as the oracle, or assert physical
access, join, aggregation, sorting, Limit, or subquery algorithms.

---

## Logical-Plan Validator Tests

V20-20 is the complete Chapter-20 validator procedure. Construct immutable malformed plans
that bypass parser/binder construction and violate exactly one §20.18 invariant while all
other descriptors remain valid. The structural expected result is produced by V20-20's
independent predicate, never by the production validator. Failure is an internal
architecture error before physical planning or execution, not a user SQL syntax error.

| Area | Malformed logical state | Required assertion |
|---|---|---|
| Slots/schema | Missing child slot, sibling/unavailable slot reference, whole-statement duplicate or reused LogicalSlotId, wrong fresh/pass-through/remap rule, output/expression order mismatch, or schema inconsistent with child/output expressions | Reject without assigning a physical interpretation |
| Types | Unresolved expression, non-BOOLEAN Filter/HAVING/Join predicate, incompatible comparison/cast, aggregate result mismatch, or output type/nullability contradiction | Reject using the already-resolved type contract |
| Operator shape | Wrong child count, condition on CROSS, missing required join condition, misplaced/renumbered/merged aggregate occurrence, impossible DML/DDL child, or hidden target RID removed | Reject before rewrite/search |
| Catalog/nullability | Inconsistent TableId/SchemaVer descriptor, mutated/stale identity pairing, or LEFT JOIN right output not null-extended | Reject rather than repair from names/current cache |
| Rewrite/proof | Empty annotation/replacement with missing, statistics-derived, or operator-invalid provenance; demanded-evaluation change without exact proof; or executable child-order change | Reject under §§20.17, 20.17.5, 20.17.10, and 35.2 |
| Diagnostic provenance | Missing/changed source origin, arbitrary replacement span, source-less potentially erroring synthetic executable, or metadata used as a diagnostic origin | Reject under §20.17 |
| Subquery state | Unsupported mode, wrong arity/type/nullability, missing/multiple independent child, OuterRef/correlated slot, invalid derived slot/name map, or NOT wrapper inconsistent with the mode | Reject using §20.14 without executing the support matrix again |

Maintain positive validator fixtures for every canonical logical family above so a
validator that rejects all plans cannot pass.

### Validation around rewrites

At rewrite entry, pass a malformed logical plan and assert validation fails before any
rule-match/application counter advances. After each major rewrite phase, use a test-only
malformed transform or injected bad output to assert immediate validation before the plan
can enter another phase or physical search. Valid user SQL may produce a structured
front-end error, but it must never directly manufacture an accepted malformed logical
plan.

---

## Logical Rewrite Tests

V20-15 through V20-19 are the complete rewrite procedure. Every rule receives an immutable
input plan, an independently derived expected output or rejection, a pre/post demanded-
evaluation relation, exact-proof facts where required, executable-tree serialization,
slot/aggregate/provenance mappings, and bag/order/nullability oracles. Exercise both the
exact matching boundary and the nearest nonmatching boundary, then validate input and
output with V20-20. Differential execution, when available, supplements but never replaces
these independent structural and semantic oracles.

### Volatility, folding, and Boolean 3VL

Constant folding covers deterministic scalar results, NULL, casts, arithmetic errors,
searched CASE, and short-circuited branches using §17.10.2 precedence. Boolean
simplification runs complete TRUE/FALSE/UNKNOWN fixtures. Accept only identities valid
under SQL 3VL, §17.10.2, V20-15 demand preservation, V20-17 executable child order, and
V20-18 provenance; an unproven potentially erroring occurrence is conservative.

### Predicate pushdown and projection pruning

For predicate pushdown, construct INNER/CROSS and LEFT JOIN plans with left-local,
right-local, cross-side, NULL-sensitive, and error-capable predicates. A side-local
reference is necessary but not sufficient: compare pre/post demand domains and reject
movement that adds potentially observable evaluation unless the independent exact-proof
oracle establishes total/error-free demand insensitivity. Preserve ON-versus-WHERE meaning
and LEFT null extension. An estimate is never rewrite authority.

Projection-pruning fixtures place required values in parent projection, filters, join
conditions, grouping/aggregate arguments, sorting, DML assignments/RETURNING, hidden target
RID/system slots, and subquery/derived-slot maps. Ancestor nonuse alone does not permit
removing an ordinary demanded error-capable Project expression. Removal requires the
EXISTS specialized irrelevance rule or exact total/error-free proof with all other
semantics unchanged. Retained occurrences keep their LogicalSlotIds; newly introduced or
duplicated occurrences receive fresh IDs, and retired IDs are not reused.

### Exact-empty and subquery-sensitive rewrites

Exercise every §20.17.10 propagation case with approved proof provenance and with an
otherwise identical numerical-zero estimate lacking proof. Only the former establishes
semantic emptiness, and even it may introduce a schema-equivalent empty/no-op plan only
when V20-15/16 prove that no demanded potentially observable evaluation is suppressed.
Preserve LEFT rows, global-aggregate one-row semantics, DML completion/affected-row
behavior, and subquery lazy demand/left evaluation. Reuse the §41.6 Semantic Emptiness
Tests for estimator fixtures without letting them define the demand oracle.

For supported subquery semi/anti/marker or boundary-removal rewrites, compare against the
canonical independent fallback. Preserve 3VL, duplicate behavior, cardinality checks,
snapshot/CommandId, exact proof provenance, and §20.14.12 error precedence. Decorrelating
an unsupported form is not a way to make it supported.

---

## Catalog / Front-End Benchmarks

Performance is less critical than execution/storage, but benchmark enough to avoid pathological architecture.

Measure:

```text
lexer MB/s
parser statements/sec
binder latency
catalog name lookup latency
catalog ID lookup latency
historical schema lookup latency
catalog cache hit rate
large SELECT-list binding
large VALUES binding
large expression-tree binding
multi-join binding
logical plan construction time
rewrite-phase time
front-end allocation calls and bytes
optimizer planning-arena allocations and bytes
```

Apply the shared Chapter-42 benchmark evidence policy. Identify each workload by stable
SQL/input text or generator and seed, catalog/schema snapshot, expected valid result,
semantic scale, and measured work denominator. Reuse Chapter-18 token/raw-AST,
Chapter-16 descriptor/visibility, Chapter-19 binding, Chapter-20 plan/rewrite, and
Chapter-38 planning-resource correctness oracles; an invalid result or owner resource
failure is not an ordinary successful throughput/latency sample. Time each stage from
its own entry through its completed owner output, rather than labeling end-to-end SQL
latency as several stage times or subtracting overlapping intervals.

- Lexer MB/s uses a deterministic valid byte corpus with an exact input-byte denominator
  and independently expected token sequence/outcome. Time source consumption through
  tokenization only. Parser statements/sec uses a replayable statement corpus with
  declared statement count, bytes, shape mix, and expected raw-AST results; time parser
  consumption through complete raw-AST production. Declare whether tokenization is
  inside that parser interval, and retain its separate observation when reporting both
  stages. Do not mix malformed/error workloads into successful throughput.
- Binder latency uses a replayable valid raw AST and a fixed visible catalog snapshot,
  timed from binder entry to completed typed/bound representation. Record the expected
  bound identities, types, and output shape; control or explicitly vary descriptor-cache
  state. Count successful bindings as work. Standalone catalog lookup latency is not
  inferred from binder duration merely because binding performs catalog access.
- Build a deterministic catalog population with known names, stable TableIds/ColumnIds
  and applicable IndexIds, current SchemaVer, at least one legally retained older
  SchemaVer, and declared snapshot visibility. Measure current-object lookup by visible
  name and lookup by stable semantic ID as separate owner paths and latency series,
  checking each returned immutable descriptor's identity/version. An ID-path sample
  cannot first resolve the name and merely read the returned ID. Measure
  `ResolveSchema(TableId, old SchemaVer)` with `old != current`, a retained historical
  descriptor, and exact returned-version identity; declare whether historical lookup
  is cache-hit or underlying-resolution work. Invisible, dropped, or otherwise invalid
  requests retain their canonical outcome and are not ordinary successful lookup samples.
- Catalog cache hit rate uses owner-observed, same-interval hits divided by applicable
  cache lookup attempts, with misses/bypasses separately identified. For a positive hit
  case, legally precondition the snapshot-visible descriptor in cache and observe an
  actual hit; repeated calls, pointer equality, or low latency are not proof. A controlled
  miss/bypass case observes snapshot-aware catalog fallback returning the same semantic
  descriptor. Reuse Chapter-16 cache/MVCC oracles: uncommitted or snapshot-invisible
  descriptors invalidate the sample, and missing cache-event evidence is not zero hits.
- Bind a valid wide SELECT fixture (the 100-column case below is one Verification-owned
  choice), a declared large VALUES row-by-column/type/NULL matrix, a deep expression
  with known node count/depth/type/cast shape, and a multi-join query with declared
  relation occurrences, distinct BindingIds, aliases, predicates, and expected bound
  relation graph. Record exact fixture scales and successful binder intervals for each;
  VALUES allocation tracking alone does not measure binding, and multi-join binding is
  distinct from optimizer join-order search.
- Logical-plan construction starts with a validated, fully bound fixture and ends with
  its validated canonical pre-rewrite logical plan. Include representative join,
  aggregate, and sort/limit or DML forms, retaining expected structure, slots, and
  output schema. Rewrite timing starts from a validated pre-rewrite logical plan and
  ends after the owned rewrite phase and required post-phase validation (§20.18).
  Include a fixture with an applicable rule/phase whose owner activity and before/after
  logical structure prove nontrivial rewrite work; an unmatched plan is only a baseline.
  Reuse V20 semantics/demand oracles. Neither interval includes physical optimization
  or execution, and rewrite timing does not include parsing or binding.

Measure optimizer planning-arena reservation/allocation or growth events and charged
bytes from the dedicated §38.21 arenan owner on successful legal planning invocations;
record peak live arena bytes separately under that owner's budget semantics. Do not infer
arena bytes from plan-node count, process RSS, or execution QueryMemoryManager memory.
Reuse Chapter-38 budget/fallback/error procedures rather than redefining them here.
Full optimizer search/cost benchmark methodology remains in its own sections.

---

## Parser/AST Memory Benchmark

Track allocations/bytes for:

```text
100-column SELECT
large VALUES insert
deep boolean expression
multi-join query
```

Record total/front-end-attributed bytes, peak live bytes, allocation count, retained backing,
and payload materialization. Compare representations only after proving the §18.14 lifetime
contract and §18.17 resource behavior; the benchmark does not prefer an arena, per-object
ownership, interning, source slices, or another allocation technique.

For each case, record the exact source/generator and seed, valid expected raw-AST/bound
structure, semantic node/list-element count, front-end-attributed allocation calls and
allocated bytes, peak live bytes, and retained/materialized backing. The 100-column SELECT
is a Verification fixture for Architecture's qualitative wide SELECT case, not a SQL or
Architecture width constant. Attribute general-purpose heap allocation separately from
any implementation-specific arena reservation/growth, source backing, unrelated runtime
allocation, and the optimizer's mandatory planning arena. RSS alone is not an allocation
oracle; use scoped allocation events or equivalent owner/test-harness attribution.

For at least one scalable SELECT-list, VALUES, expression-tree, or join fixture, predeclare
multiple materially increasing semantic node/element counts and measure allocation calls
and bytes at each size under comparable stage and ownership boundaries. Compare the
growth in general-purpose allocations with independently known semantic counts to detect
a one-for-one per-node/list-element churn pattern versus coarser or amortized storage.
Changing only SQL whitespace/byte length without changing semantic structure is not this
test. Report the pattern as evidence, not a universal allocations-per-node threshold or
an AST-arena requirement; concrete AST allocation/container/reference representation
remains implementation-owned under §18.14.

---

## Front-End Fuzzing

Fuzz at least:

```text
lexer
parser
tuple/type literal conversion
bound expression constant evaluator
```

Requirements:

- no crashes,
- no out-of-bounds access,
- no undefined behavior,
- structured `LexerError`, `ParserError`, semantic-owner, `FrontEndResourceLimit`, or
  `OutOfMemory` classification according to the deterministic Chapter-18 procedures,
- no successful partial token/raw-AST result and no truncation or semantic fallback,
- no unbounded parser-recovery loop.

SQL parser fuzzing is high-value because arbitrary text reaches it directly.

Use reproducible corpus entries and report the generated seed on failure. Include arbitrary
bytes, deeply nested delimiters, truncated literals/comments, long identifiers/lists,
numeric boundaries, and malformed multi-statement batches. Literal/type-conversion and
bound-constant-evaluator fuzzing must preserve the closed Chapter-17 error categories and
Chapter-18 complete-result/resource behavior.

Parser fuzzing may naturally reach subquery syntax, but deterministic table-driven
§20.14 tests remain authoritative for supported and rejected forms. Fuzzing does not stand
in for the subquery support matrix or logical-validator malformed-plan corpus.
