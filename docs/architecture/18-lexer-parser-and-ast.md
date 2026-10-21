# 18. Lexer, Parser, and AST

## 18.1 Front-end boundary

The SQL front end is an in-project subsystem.

It consists of:

```text
Lexer
Parser
AST
```

followed by the separate Binder in Chapter 19. Parser output represents syntax. Object lookup, type resolution, and database identity belong to binding. The front end targets an explicit SQL subset rather than pretending to implement the full SQL standard.

## 18.2 Token model

SQL source is one length-delimited byte sequence. Structural SQL syntax is
defined over ASCII bytes; the complete source is not required to be valid
UTF-8. Lexical decisions MUST NOT depend on locale-sensitive character
classification, Unicode-library syntax classification, or host signed-char
behavior.

The complete whitespace set is:

```text
0x09  horizontal tab
0x0A  LF
0x0B  vertical tab
0x0C  form feed
0x0D  CR
0x20  space
```

No other byte is whitespace. Whitespace is discarded from the raw syntactic
representation except that source spans continue to refer to its original
location. Whitespace and comments separate adjacent identifier or numeric
sequences where separation is required; removing that trivia MUST NOT
concatenate the surrounding bytes into one token.

Outside a string literal, quoted identifier, or comment, every byte not
admitted by the closed lexical grammar is a source-positioned lexical error.
In particular, the §17.5.3 unquoted-source-NUL rule remains authoritative: NUL
is rejected everywhere outside a string literal, including comments and quoted
identifiers. A source-wide UTF-8 validation or normalization pass is neither
required nor permitted to redefine tokenization. This byte-defined grammar
keeps structural syntax independent of locale and Unicode-library behavior.

Lexical analysis produces tokens containing at least:

```text
TokenKind
source byte span
optional literal payload
source offset / diagnostic location
```

Token spans use half-open source-byte ranges `[start, end)`. Line/column information may be cached or derived from byte offsets.

### 18.2.1 Front-end source and payload backing

Each front-end processing attempt for an admitted SQL request has one
conceptual **front-end backing owner**. It keeps alive the immutable original
SQL source bytes and every derived payload store on which a surviving token,
raw-AST object, bound object, or diagnostic depends. Derived payload includes,
where applicable:

```text
decoded string-literal bytes
decoded quoted-identifier bytes
canonicalized unquoted-identifier bytes
lossless numeric-literal provenance
other referenced lexical payload
```

This owner is a semantic lifetime concept, not a required C++ class or
allocation structure. The original request bytes MUST remain immutable while
any surviving object refers to them. An implementation MAY retain the request
buffer, copy it, retain another immutable owner, or use stable offsets into an
equivalent immutable copy.

Tokens and later front-end objects MAY borrow views of original or derived
payload storage. Every backing owner MUST outlive every borrower. Before an
object survives release of the owner from which it borrows, the implementation
MUST either retain that owner or materialize/transfer every required payload
into storage that covers the object's remaining lifetime. It MUST NOT mutate,
recycle, or release referenced bytes and leave a dangling view.

Front-end backing is process-local runtime state. It is not WAL-logged,
checkpointed, cataloged, stored in user data or indexes, or recovered after a
crash. Source spans and raw SQL text are not persistent database format.

## 18.3 Token classes

The token classes are:

```text
identifier
quoted identifier
integer literal
floating literal
string literal
reserved keyword
symbolic operator / punctuation
end-of-input
```

The keyword registry is closed. Every keyword is reserved when unquoted;
the architecture has no contextual or nonreserved keyword class. A reserved spelling may be
used as an identifier only through quoted-identifier syntax. Recognition uses
only the ASCII mapping `A`..`Z` to `a`..`z`, independently of locale.

The complete reserved-keyword registry is:

```text
ANALYZE     AND         AS          ASC         BEGIN
BY          CASE        CAST        COMMIT      CREATE
CROSS       DEFAULT     DELETE      DESC        DISTINCT
DROP        ELSE        END         EXISTS      EXPLAIN
FALSE       FROM        GROUP       HAVING      IN
INDEX       INNER       INSERT      INTO        IS
JOIN        KEY         LEFT        LIMIT       NOT
NULL        OFFSET      ON          OR          ORDER
PRIMARY     RETURNING   ROLLBACK    SELECT      SET
TABLE       THEN        TRUE        UNIQUE      UPDATE
VACUUM      VALUES      WHEN        WHERE
```

This registry reserves the word terminals already required by the language
surface. Reservation does not authorize a word in a grammar position where the
closed statement or expression grammar does not admit it, and the registry does
not import compatibility words from another SQL dialect.

The complete symbolic-token registry is:

| Class | Exact spellings |
|---|---|
| punctuation | `(`, `)`, `,`, `.`, `;` |
| arithmetic | `+`, `-`, `*`, `/`, `%` |
| comparison | `=`, `<>`, `<`, `<=`, `>`, `>=` |

`!=` and every unlisted symbolic operator or punctuation spelling are absent
from the defined architecture. The lexer MUST NOT manufacture a generic operator token from an
arbitrary symbol sequence.

When registered symbolic tokens share a prefix, tokenization chooses the
longest registered spelling. Comment openers `--` and `/*` are recognized
before their component `-` or `/` tokens. A string or quoted-identifier
delimiter enters its corresponding lexical construct before bytes within that
construct can be interpreted as comments or symbolic tokens. A symbol-like
byte not admitted by another lexical construct or by the table above is a
source-positioned lexical error. These rules make byte-to-token mapping unique
without prescribing a lexer algorithm.

## 18.4 Identifier rules

An unquoted identifier has exactly this ASCII grammar `[A-Za-z_][A-Za-z0-9_]*`.

No non-ASCII byte or NUL is admitted in an unquoted identifier. Canonicalization
maps each ASCII `A`..`Z` byte to the corresponding `a`..`z` byte and leaves all
other admitted bytes unchanged. Locale-sensitive or Unicode case folding MUST
NOT participate.

Thus `Users`, `USERS`, and `users` all resolve textually as `users`.

Quoted identifiers use double-quote delimiters. Within a quoted identifier,
`""` decodes to one literal `"` byte; any lone `"` closes the identifier.
The decoded payload may contain arbitrary non-NUL bytes, including bytes with
the high bit set, and preserves exact byte values and case without Unicode
normalization. NUL is forbidden. End-of-input before a closing delimiter is a
source-positioned lexical error.

The decoded canonical identifier payload MUST be nonempty, so `""` is rejected
at the Chapter-18 identifier boundary rather than becoming an empty catalog
name. The raw AST preserves the canonical identifier bytes, the quoted-source
distinction where syntax or diagnostics requires it, and the original source
span.

AST identifiers remain textual until binding.

Binding performs namespace/catalog lookup and assigns object identities. The
lexer, parser, and raw AST do not decide object existence or assign TableId,
ColumnId, or IndexId.

## 18.5 String literals

Single quotes delimit SQL string literals.

A literal single quote is represented by doubled quote:

```sql
'It''s fine'
```

The token payload contains decoded logical bytes while the original source span remains available for diagnostics. Vendor-specific backslash escape modes are not part of the baseline. An unterminated string is a source-positioned lexical error.

The exact byte/NUL/no-backslash semantics are the closed §17.5.3 registry.

## 18.6 Numeric literals

The lexer recognizes exactly the integer and FLOAT64 token grammars in
§§17.5.1–17.5.2. Representative legal forms are:

```text
123
1.25
1e10
1.2e-3
```

Leading `+`/`-` is tokenized as a unary operator rather than embedded in the
numeric token.

When the current source byte is an ASCII digit, the lexer consumes exactly one
unsigned integer or FLOAT64 token under §§17.5.1–17.5.2. It does not extend the
token according to a host numeric parser. A current `.` byte is the registered
punctuation token because §17.5.2 does not permit a numeric token to begin with
`.`. The same grammar excludes `.5` and excludes `1.` as a FLOAT64 token; this
chapter does not broaden those forms.

Thus `-2147483648` is parsed as unary minus applied to a positive numeric
literal representation and has the exact INT32 result described by §17.5.1.

The token/raw AST retains a lossless spelling, checked mathematical magnitude,
exact decimal token object, or equivalent representation sufficient for
Chapter 17's canonical classification. In particular, the unsigned decimal
magnitude `9223372036854775808` remains representable through binding so that
§17.5.1 can distinguish its legal direct-negative use. No specific source
container or numeric representation is required.

Literal overflow/type selection and FLOAT64 conversion are semantic/type-
resolution concerns rather than unchecked host-language integer/FLOAT
conversion in the lexer or parser.

## 18.7 Comments

The lexer recognizes line comments and non-nested block comments:

```sql
-- line comment
/* block comment */
```

Nested block comments are outside the lexical grammar.

A line comment begins with `--` outside a string literal or quoted identifier.
It consumes bytes after the opener up to but not including the first CR or LF,
or through end-of-input when neither occurs. End-of-input is therefore a valid
line-comment terminator. The terminating CR/LF remains ordinary whitespace; in
CRLF input the comment ends before CR and both CR and LF are then whitespace.
No newline normalization is required.

A block comment begins with `/*` outside a string literal or quoted identifier
and ends at the first following `*/`. It is non-nested: an inner `/*` has no
nesting significance. End-of-input before the closer is a source-positioned
lexical error.

Comment payload may otherwise contain arbitrary non-NUL bytes. Comment delimiters
inside string literals or quoted identifiers are ordinary payload bytes and do
not begin or end comments. A complete comment acts as lexical trivia and as a
token separator where required.

## 18.8 Source locations

Every token and AST source span is the half-open interval: `SourceSpan = [start_byte_offset, end_byte_offset)` over the original SQL request byte sequence, with:

```text
0 <= start_byte_offset <= end_byte_offset <= source_byte_length
```

Offsets count bytes, not Unicode code points, characters, graphemes, or
line/column identities. Line/column information may be derived separately but
does not redefine a SourceSpan.

A source-originating diagnostic carries the `SourceSpan` of the token or
syntactic construct whose violation directly determines that diagnostic, using
the most specific such span already represented by the responsible front-end
stage. If two represented spans are equally specific, the earliest span in
source order is selected.

Lexical diagnostics use these spans:

- an invalid source byte or unrecognized symbolic-token candidate uses the
  smallest offending byte or candidate range that causes lexical rejection;
- an unterminated string literal, quoted identifier, or block comment uses the
  range from its opening delimiter byte through end-of-input.

A parser diagnostic for an unexpected existing token uses that token's span.
When required input is missing at end-of-input, it uses the zero-width span
`[source_byte_length, source_byte_length)`. A binding or type diagnostic uses
the span already attached to the raw or bound syntactic construct whose
resolution directly produced the error. The selection rule does not prescribe
diagnostic message text, recovery behavior, or presentation formatting.

The numeric interval value MAY outlive the original source storage. Extracting
or rendering the original text identified by that interval requires either the
original immutable source backing or a materialized copy of the needed text.
A diagnostic that outlives the source owner and needs rendered source text MUST
materialize that text before source release; retaining offsets alone does not
require execution to retain the original SQL request. Source positions and any
required backing remain available through parsing and semantic diagnostics for
as long as their consumers require them.

## 18.9 Parser semantic contract

Parsing MUST implement the closed statement and expression grammars in this
chapter. Precedence, associativity, grouping, and syntax-preserving boundaries
MUST produce the canonical raw-AST structure defined by §§18.10–18.15.

This chapter is authoritative for the accepted language and raw-AST semantic
contract. Parser-library or generator defaults, conflict resolution, traversal
strategy, lookahead strategy, and recovery heuristics MUST NOT redefine them.
Parser construction technique is otherwise implementation-defined.

## 18.10 Closed statement grammar

The grammar in this chapter uses these EBNF conventions:

```text
"TOKEN"        exact reserved keyword or symbolic token
<name>         nonterminal
[ item ]       optional item
{ item }       zero or more repetitions
a | b          alternatives
```

Every syntactically required comma-separated list is nonempty and no
comma-separated list accepts a trailing comma. An explicitly optional wrapper
does not make the list inside it empty-capable. A zero-argument function call
is the separate production in §18.12.4 rather than an exception to this list
rule. This closed policy prevents list boundaries from depending on a generic
parser-helper implementation.

The name grammar is:

```ebnf
<identifier> ::= <unquoted-identifier> | <quoted-identifier>

<object-name> ::=
      <identifier>
    | <identifier> "." <identifier>

<table-name>  ::= <object-name>
<index-name>  ::= <object-name>
<column-name> ::= <identifier>
<alias>       ::= <identifier>

<type-name> ::= <unquoted-identifier>
```

Tables and indexes therefore admit either an unqualified name or exactly
`namespace.object`. Binding requires the canonical first component of a
two-part object name to equal `main`; it performs the namespace/object lookup
and reports the existing binding error otherwise. Three-part and arbitrary
repeated-dot object names are not syntax. Parsing this shape requires no
catalog access.

A type-name position accepts any unquoted identifier syntactically. Binding and
the Chapter-17 TypeResolver accept exactly the canonical names `BOOLEAN`,
`INT32`, `INT64`, `FLOAT64`, `DATE`, `TIMESTAMP`, and `VARCHAR` after §18.4
canonicalization. Any other unquoted identifier parses and then fails through
the existing type-resolution error; a quoted identifier is not type-name
syntax. The raw AST retains the type-name bytes and source span, never a
resolved TypeId. There are no type aliases or parameterized `VARCHAR(n)` syntax.

The complete top-level dispatch is:

```ebnf
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
```

No generic statement node admits an unlisted statement start.

### 18.10.1 SQL request and statement-batch framing

A parsed SQL request contains one or more statements in source order. The `;`
token is required between adjacent statements and optional after the final
statement:

```text
statement
statement ;
statement ; statement
statement ; statement ;
```

Empty statements are forbidden. A leading semicolon, repeated semicolons, or
an extra semicolon after the optional final terminator is a `ParserError` under
§39.2. A request containing only whitespace and/or comments likewise contains
zero statements and is rejected as an empty SQL request.

Successful request parsing MUST consume all non-whitespace/non-comment input
through end-of-input. After the final statement, only end-of-input or one
optional final semicolon followed by whitespace/comments and end-of-input is
valid. No trailing token suffix is ignored. A token that is neither a valid
continuation of the current statement nor, after the required separator, the
start of another admitted statement produces `ParserError`.

The request/batch AST preserves exact statement source order. Chapter 18 owns
request framing and parser recovery. Recovery MAY synchronize at a semicolon
to continue diagnosing later independent statement syntax, but a request with
any `ParserError` fails as a whole and MUST NOT publish a successfully bindable
or executable prefix, or any recovery artifact, as a successful raw AST.
Recovery does not change the accepted grammar, statement source order, or
multiplicity. Transaction/execution behavior for independently completed
statements remains with its respective downstream owners. Explicit framing
prevents empty entries or ignored suffixes from becoming implementation-
dependent batch behavior.

The equivalent request production is:

```ebnf
<sql-request> ::=
    <statement> { ";" <statement> } [ ";" ] <end-of-input>
```

### 18.10.2 DDL statements

The complete CREATE TABLE grammar is:

```ebnf
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
```

Column modifiers have exactly the displayed order. A column is nullable when
neither this syntax nor a table PRIMARY KEY causes the Chapter-16/21 semantic
owner to make it non-nullable. Explicit column `NULL`, column-level PRIMARY KEY
or UNIQUE, DEFAULT before NOT NULL, named constraints, CHECK, FOREIGN KEY,
generated/identity columns, temporary tables, `IF NOT EXISTS`, and table
options are absent. Table-element and constraint-column order and multiplicity
are preserved; DDL binding validates duplicate names, duplicate/referenced
constraint columns, type support, and default legality.

`ALTER TABLE` is outside the supported grammar.

The standalone index and DROP grammars are:

```ebnf
<create-index-statement> ::=
    "CREATE" [ "UNIQUE" ] "INDEX" <index-name>
    "ON" <table-name>
    "(" <column-name> { "," <column-name> } ")"

<drop-statement> ::=
      "DROP" "TABLE" <table-name>
    | "DROP" "INDEX" <index-name>
```

Index keys are source-ordered base-column names. The raw list retains
duplicates for binder rejection. Expression keys, per-key ASC/DESC, INCLUDE,
partial predicates, `IF NOT EXISTS`, `IF EXISTS`, CASCADE, RESTRICT, and
multiple DROP targets are absent.

### 18.10.3 DML statements and RETURNING

The complete INSERT grammar is:

```ebnf
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
```

When present, the target-column list is nonempty; VALUES contains at least one
nonempty row. Row/target order and multiplicity are exact. Binding owns target
column uniqueness, row/target arity, assignment coercion, omitted-column
defaults, NULLability, and INSERT SELECT compatibility.

The complete UPDATE and DELETE grammars are:

```ebnf
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
```

The SET list is nonempty and retains duplicate assignments for binder
rejection. UPDATE/DELETE target aliases, implicit or explicit, are not syntax.
UPDATE FROM, tuple assignment, DELETE USING, and DML ORDER BY/LIMIT are absent.

Explicit DML DEFAULT sentinels do not exist: `DEFAULT VALUES`, a DEFAULT item
inside VALUES, and `SET column = DEFAULT` are syntax errors. INSERT obtains a
stored default only for an omitted target column under §21.11. Column-definition
`DEFAULT <expression>` remains the distinct DDL form governed by §21.12.

RETURNING is available only on INSERT, UPDATE, and DELETE:

```ebnf
<returning-clause> ::=
    "RETURNING" <returning-item> { "," <returning-item> }

<returning-item> ::=
    <expression> [ "AS" <alias> ]
```

Its list is nonempty, source ordered, and multiplicity preserving. Aliases
require AS. `RETURNING *`, qualified-star RETURNING, implicit aliases, and
RETURNING on SELECT, DDL, transaction, or maintenance statements are absent.
Chapter 15 and §§21.13–21.15 own DML row-image and result semantics.

### 18.10.4 Transaction, maintenance, and EXPLAIN statements

Transaction-control syntax is exactly the three keyword-only alternatives in
`<statement>`: `BEGIN`, `COMMIT`, and `ROLLBACK`. The architecture has no START TRANSACTION,
BEGIN/COMMIT TRANSACTION, isolation clause, READ ONLY, SAVEPOINT, RELEASE, or
ROLLBACK TO syntax; Chapter 9 owns transaction semantics.

The maintenance and observability grammar is:

```ebnf
<vacuum-statement>  ::= "VACUUM" <table-name>
<analyze-statement> ::= "ANALYZE" <table-name>

<explain-statement> ::=
      "EXPLAIN" <select-statement>
    | "EXPLAIN" "ANALYZE" <select-statement>
```

VACUUM and ANALYZE each require exactly one table target and accept no option or
column list. Targetless `ANALYZE` is outside the supported grammar. EXPLAIN and
EXPLAIN ANALYZE accept only SELECT; DML, DDL,
transaction, and maintenance statements are not explainable syntax. The
semantic owners are §14.17, §21.17.1/Chapter 34, and Chapter 40 respectively.

## 18.11 SELECT grammar surface

The complete SELECT grammar is:

```ebnf
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
```

Projection, GROUP BY, and ORDER BY lists are nonempty. Projection and base-table
aliases require explicit AS. A derived-table alias is mandatory while its AS
is optional, as required by §20.14.3. Qualified star has exactly one identifier
component; `main.table.*` and arbitrary repeated qualification are not syntax.
Wildcard expansion, output aliases/ordinals, and ORDER BY semantic resolution
remain Chapter-19 responsibilities.

INNER and LEFT JOIN require exactly one ON expression; CROSS JOIN has none.
Each repeated join tail left-folds over the preceding joined table, preserving
source order. Thus `a INNER JOIN b ON p LEFT JOIN c ON q` has the raw structure
`LeftJoin(InnerJoin(a,b,p),c,q)`. Bare JOIN, comma joins, RIGHT/FULL/NATURAL/
USING/LATERAL joins, and parenthesized join-tree grouping are absent. Parentheses
in FROM are used only by the displayed derived-SELECT production.

Clause order is exactly projection, FROM, WHERE, GROUP BY, HAVING, ORDER BY,
LIMIT, OFFSET; no clause repeats. ORDER BY has no NULLS FIRST/LAST syntax.
OFFSET may appear without LIMIT. LIMIT/OFFSET operands are syntactic
expressions, matching §§19.14 and 20.14.9: binding requires an integral,
nonnegative value constant at execution start, and subqueries are prohibited.
The parser does not enforce those semantic properties or narrow the operand to
one integer token.

`FROM` is optional in every SELECT query specification, including a
supported nested SELECT. Omitting it does not remove or add any other clause in
this grammar.

### 18.11.1 SELECT without FROM

`FROM` is optional in every SELECT query specification, including a
supported nested SELECT. Section 19.5 owns wildcard and output-name binding for
an empty relation namespace; §§20.5 and 20.15 own the logical source and
relational-operator semantics when FROM is omitted; §20.14 owns composition
with supported subqueries. Chapter 18 adds no no-FROM-specific binding,
planning, execution, aggregate, or scalar semantics; this subsection is the
syntax and ownership index for omitted FROM.

## 18.12 Closed expression grammar

The complete expression grammar is:

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
```

OR, AND, additive, and multiplicative repetitions construct left-associated
trees. Prefix NOT and unary signs nest from the prefix toward their operand.
The optional predicate suffix occurs at most once, so comparisons and
predicates do not chain: `a < b < c`, `a = b = c`, and
`a IS NULL IS NULL` are syntax errors. Consequently `NOT a = b` means
`NOT (a = b)`, `NOT a IN (x)` means `NOT (a IN (x))`, `a NOT IN (x)` is one
NOT-IN predicate spelling, and `a IS NOT NULL` is one null-predicate spelling.
These results follow the grammar rather than parser conflict resolution.

An expression-list IN right-hand side is nonempty, has no trailing comma, and
retains source order and multiplicity for §17.7.3. Its subquery alternative is
exactly one SELECT statement. Row-valued IN and `(a,b)` row-constructor syntax
do not exist.

### 18.12.1 Primary expressions and names

The primary-expression grammar is:

```ebnf
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

<literal> ::=
      <integer-literal-token>
    | <floating-literal-token>
    | <string-literal-token>
    | "TRUE"
    | "FALSE"
    | "NULL"
```

A two-part column reference is syntactically `relation-or-alias.column`;
binding chooses the relation binding and column. Three-part
`namespace.table.column` references are absent even though table/index object
names separately permit two parts. Column lookup is never performed by the
parser.

The literal token classes and their lossless/decoded payloads are §§17.3 and
17.5–18.6. NULL remains syntactically untyped. Parentheses remain a mandatory
raw-AST boundary under §§18.6 and 18.13, including for Chapter-17 direct-negative
classification.

### 18.12.2 CAST and searched CASE

The exact forms are:

```ebnf
<cast-expression> ::=
    "CAST" "(" <expression> "AS" <type-name> ")"

<case-expression> ::=
    "CASE"
    <when-clause> { <when-clause> }
    [ "ELSE" <expression> ]
    "END"

<when-clause> ::=
    "WHEN" <expression> "THEN" <expression>
```

CASE therefore has at least one WHEN arm, preserves arm source order, and has
an optional ELSE with the exact §17.9.1 missing-ELSE semantics. Simple CASE,
`::`, CONVERT, and parameterized type syntax are absent. The parser records
type-name syntax; TypeResolver owns cast/type legality.

### 18.12.3 Subquery syntax and rejection boundary

The expression and FROM grammars admit exactly these structural SELECT-subquery
positions:

```ebnf
<exists-expression> ::=
    "EXISTS" "(" <select-statement> ")"

<scalar-subquery-expression> ::=
    "(" <select-statement> ")"
```

Together with the productions above, the complete structural classes are
scalar `(SELECT ...)`, `[NOT] EXISTS (SELECT ...)`, scalar expression
`[NOT] IN (SELECT ...)`, and derived table
`(SELECT ...) [AS] required_alias`. SELECT is reserved, so a parenthesized
SELECT is distinct from a parenthesized ordinary expression without catalog or
type information. The optional AS on a derived table never makes its alias
optional, and no derived-column alias list exists.

Only SELECT can occupy a subquery position. A parenthesized INSERT, UPDATE,
DELETE, DDL, or other statement is therefore a `ParserError`, as is an
unsupported row constructor or wrapper/operator for which this grammar has no
production. The parser does not create a generic arbitrary-statement subquery.

A structurally valid SELECT subquery is parsed without resolving names,
detecting correlation, inspecting output arity/types, or consulting a schema.
Binding and §20.14 then reject unsupported semantic properties with the
existing `UnsupportedCorrelation`, `UnsupportedFeature`, type, or cardinality
owner as applicable. In particular:

- a correlated SELECT reaches binding, which detects the attempted outer
  reference;
- a scalar SELECT with the wrong output arity reaches binding/subquery-shape
  validation;
- an IN SELECT with an incompatible output arity or type reaches binding and
  TypeResolver;
- an independently well-formed nested SELECT is not rejected merely because
  the parser cannot establish its semantic support.

This split makes structurally nonexistent forms parser errors while preserving
Chapter-19/20 ownership of every unsupported property that requires semantic
knowledge. It uses the existing §21.16/§39.2 categories and creates no new
error enum.

### 18.12.4 Generic function and star-argument calls

Function-call syntax is registry-independent:

```ebnf
<function-call> ::=
      <identifier> "(" ")"
    | <identifier> "(" <expression> { "," <expression> } ")"
    | <identifier> "(" "*" ")"
```

Function names are unqualified identifiers; because `<identifier>` includes
both §18.4 forms, quoted call names are structurally legal without receiving
special semantic treatment. Normal calls preserve an ordered zero-or-more
argument sequence. Star-argument calls retain a distinct syntactic star role.

The parser MUST NOT recognize `count`, `sum`, `avg`, `min`, `max`, or any other
name through a scalar/aggregate registry. Thus `count(*)`, `sum(*)`, and
`foo(*)` all parse as the same star-call shape, while `f()`, `f(a)`, and
`f(a,b)` parse as ordinary calls. Chapter 17 and Chapter 29 decide function
existence, scalar versus aggregate identity, arity/types, and whether star is
legal. Aggregate DISTINCT, FILTER, OVER, and qualified function names are not
syntax.

### 18.12.5 Parse and bind boundary

Parse success means only that the source belongs to this closed syntactic
language. It does not establish that an object/column/type/function exists;
that an alias resolves; that a call's arity or star form is registered; that an
operator or cast accepts its operand types; that a duplicate target is legal;
that a default expression is permitted; that a SELECT subquery has supported
semantic shape; or that a LIMIT/OFFSET expression satisfies §§19.14 and
20.14.9. Those checks belong to binding and the cited semantic registries.

Conversely, unsupported statement starts, missing/reordered/repeated clauses,
empty required lists, trailing commas, implicit aliases, excluded JOIN forms,
comparison chaining, malformed CAST/CASE/call syntax, DML DEFAULT sentinels,
RETURNING wildcards, DML target aliases, data-modifying subqueries, and
unsupported row constructors fail structurally with the existing ParserError.
The parser remains catalog-independent and TypeResolver-independent.

## 18.13 AST contract

The AST records syntax, not resolved semantics.

Raw-AST semantic categories include:

```text
StatementRequest
SelectStatement
CreateTableStatement
CreateIndexStatement
DropStatement
InsertStatement
UpdateStatement
DeleteStatement
TransactionStatement
VacuumStatement
AnalyzeStatement
ExplainStatement
AstIdentifier
AstQualifiedName
AstTypeName
AstLiteral
AstBinaryExpression
AstUnaryExpression
AstParenthesizedExpression
AstComparison
AstNullPredicate
AstInList
AstInSubquery
AstFunctionCall
AstStarArgumentCall
AstCast
AstCase
AstStar
AstSubqueryExpression
AstTableReference
AstJoin
AstReturningClause
```

These names describe required syntactic distinctions rather than mandatory C++
class names. Statement nodes retain every present clause and its ordered raw
children. `AnalyzeStatement` and `VacuumStatement`, for example, contain an
unresolved target table name and source span, not a TableId.

Textual object/column/function names and type-name syntax remain names in the
AST. An optional second object-name component, explicit AS aliases, literal
syntax provenance, and each syntactic star role remain recoverable.

Raw AST MUST NOT contain TableId, ColumnId, IndexId, ConstraintId, BindingId,
resolved TypeId, catalog descriptors, parser-inserted semantic casts, or a
logical/physical plan. Binding creates those resolved identities and casts.

The four star contexts remain distinct:

```text
SELECT *
identifier.*
binary multiplication
function-call star argument
```

No star is encoded as a fake identifier, fake NULL/expression argument, or a
different star role. Function-call argument/star distinction, type-name syntax,
RETURNING expressions/aliases, and JOIN kind/left/right/ON presence likewise
remain explicit.

Parentheses form a mandatory syntax-preserving raw-AST boundary, represented by
`AstParenthesizedExpression` conceptually or by an exactly equivalent raw-AST
property. This preserves the distinction required by §17.5.1:

```text
-9223372036854775808
    UnaryMinus
        NumericLiteral("9223372036854775808")

-(9223372036854775808)
    UnaryMinus
        ParenthesizedExpression
            NumericLiteral("9223372036854775808")
```

Only the first numeric literal is the direct syntactic operand of unary minus.
Parentheses break the direct-negative special form; `-x` remains ordinary
unary minus. Chapter 18 preserves this syntax provenance while Chapter 17 alone
classifies and evaluates the literal.

Every raw-AST child collection whose source syntax is ordered MUST preserve
exact source order and exact multiplicity. Parser-side sorting, canonical
reordering, and deduplication are forbidden. This includes, where the syntax is
present:

```text
binary-expression left/right operands
unary operand provenance
CASE WHEN arms
IN expression-list items
SELECT projection items
VALUES rows and each row's items
INSERT column lists
UPDATE assignments
GROUP BY items
ORDER BY items
function and aggregate arguments
table/column declaration lists
index key-column lists
qualified-name components
statement request/batch entries
RETURNING items
JOIN left/right source structure
```

Both occurrences of a duplicated syntactic item remain present in their
written order even when binding or another downstream semantic owner rejects
the duplicate. Chapter 17 makes source order observable for short-circuit
AND/OR, searched CASE, and IN/NOT IN demand/error behavior; downstream owners
also require the actual written sequence when validating duplicate names or
assignments.
The required abstraction is an ordered sequence with exact multiplicity, not
any particular C++ container or allocation layout.

## 18.14 Front-end object ownership and lifetime

AST nodes have statement/query-batch-scoped lifetime ownership. The concrete
allocation, container, and reference representation is not part of the
SQL-semantic contract.

Token lifetime and token-payload lifetime are distinct. Token objects MAY be
destroyed after parsing once no surviving raw-AST object depends on token-owned
storage, or after every required payload has been transferred/materialized into
another valid owner. The token sequence itself need not remain resident for the
raw-AST lifetime.

A raw AST MUST remain valid through every stage that is permitted to reference
it. It MAY be released after successful binding when the bound representation
no longer refers to it and no surviving diagnostic or result depends on
raw-AST-owned payload. Bound expressions, statements, trees, and plans MAY
borrow source/raw-AST-backed information only while the corresponding backing
owner remains alive. Otherwise binding MUST materialize the required names,
literal bytes, source excerpts, source-derived labels, or other payload before
releasing the earlier owner. Neither borrowing nor copying is mandatory;
lifetime correctness is.

The following information MUST remain valid for as long as any surviving
consumer references it:

```text
decoded string-literal logical bytes
decoded quoted-identifier canonical bytes
canonicalized unquoted-identifier bytes
lossless numeric-literal information required by Chapter 17
type-name identifier bytes
object, column, alias, and function identifier bytes
every other lexical payload represented by reference
```

The ownership boundary is:

| Object/state | May borrow source or derived payload? | May own/materialize payload? | Required lifetime |
|---|---|---|---|
| original source | not applicable | owned by front-end backing | while any borrower needs original bytes |
| token | yes | yes | through token use and transfer of every dependent payload |
| raw AST | yes | yes | through every raw-AST consumer |
| bound object | only while the backing owner is retained | yes | through every bound-object consumer |
| `SourceSpan` value | stores offsets, not a byte view | value may be retained independently | interval may outlive source; source dereference may not |

Front-end backing is released only after the last dependent object has been
destroyed or has transferred/materialized every required payload elsewhere.
This rule applies on parse or bind success, parse or bind failure,
cancellation, and resource failure. Cleanup MUST NOT leave a surviving dangling
object. Allocation and storage representation remain implementation-defined
subject to this lifetime contract.

## 18.15 Expression precedence

Precedence from low to high is:

```text
OR
AND
NOT
comparison / IS NULL / IN
+ -
* / %
unary + -
primary
```

Parentheses override precedence.

The syntax-preserving parenthesized-expression boundary in §18.13 may survive
in the raw AST even when the enclosed expression tree already captures the
same precedence grouping.

Binary arithmetic and Boolean infix operators use the left associativity fixed
by §18.12's repetition productions. Prefix NOT and unary signs nest toward
their operand.

Comparison/IS NULL/IN predicate suffixes are non-chainable in the defined architecture.

Thus `a < b < c` is rejected rather than assigned accidental host-language
semantics; `NOT a = b`, `a NOT IN (b)`, and `a IS NOT NULL` have the exact
structures stated in §18.12.

## 18.16 Front-end invariants

1. Lexing and parsing conform to this chapter's closed lexical and syntactic grammars; implementation technique does not define the accepted language.
2. SQL source is length-delimited bytes; ASCII structural classification and the exact whitespace set are locale/Unicode-library independent.
3. Token and AST source spans are half-open original-request byte intervals, and source-originating diagnostics select them under §18.8.
4. The reserved-keyword and symbolic-token registries are closed; keywords use ASCII-insensitive recognition and symbolic prefixes use longest registered match.
5. Unquoted identifiers use the exact ASCII grammar and fold only `A`..`Z`; quoted identifiers preserve decoded non-NUL bytes/case and use doubled double quotes.
6. String quote escaping uses doubled single quotes in the defined architecture.
7. Leading numeric signs remain symbolic tokens, numeric syntax is the closed Chapter-17 grammar, and numeric handoff is lossless through magnitude `2^63`.
8. Parenthesized-expression provenance is mandatory, so direct unary-minus literal syntax remains distinguishable from a parenthesized operand.
9. Line-comment CR/LF/EOF termination and first-closer non-nested block comments use the exact §18.7 rules.
10. Parser output contains textual syntax names, not resolved catalog IDs.
11. Raw-AST ordered children/lists preserve exact source order and multiplicity; parser-side sorting and deduplication are forbidden.
12. AST nodes retain source spans through semantic binding.
13. Expression ASTs use the precedence and associativity defined by §§18.12 and 18.15, independently of parser construction technique.
14. One request contains one or more source-ordered statements, requires semicolons between them, permits one optional final semicolon, rejects empty statements, and consumes all nontrivia input.
15. `ANALYZE table_name` is a first-class statement AST, not parsed as VACUUM/EXPLAIN syntax.
16. Unsupported syntax fails explicitly rather than being half-interpreted.
17. Constructing a generic AST operator/function node does not imply semantic support; binding accepts only the closed Chapter-17 registry.
18. FROM is optional in every SELECT query specification; §§19.5, 20.5, 20.14, and 20.15 own the resulting binding and relational semantics.
19. The §18.10 top-level dispatch is complete; no generic statement production admits an unlisted statement.
20. Table/index object names have one or two identifier components; binding alone validates the optional namespace as `main`, and three-part names are not syntax.
21. A type-name is an unquoted identifier retained as syntax; parser output never contains a resolved TypeId, and the binder/TypeResolver owns the seven-name registry.
22. Every required comma-separated list is nonempty, no list accepts a trailing comma, and every raw list preserves source order and multiplicity.
23. Projection/base-table/RETURNING aliases require AS; derived-table aliases are mandatory with optional AS; UPDATE/DELETE targets have no aliases.
24. JOIN syntax is exactly left-associated INNER/LEFT with ON and CROSS without ON; bare, comma, parenthesized-tree, and every other join form are absent.
25. DML has no DEFAULT sentinel; RETURNING is a nonempty expression/explicit-alias list on INSERT, UPDATE, and DELETE only.
26. LIMIT/OFFSET operands parse as expressions while §§19.14 and 20.14.9 own integral, nonnegative, execution-start-constant, and no-subquery validation.
27. Generic zero/multiple/star-argument calls parse without function-name lookup; Chapter 17/29 owns every scalar/aggregate identity, arity, type, and star decision.
28. Only SELECT occupies a structural subquery position; parser errors own nonexistent structures, while binding owns correlation, output shape/type, and other semantic rejection.
29. Raw AST contains no stable catalog ID, BindingId, resolved TypeId, inserted semantic cast, catalog descriptor, or plan.
30. SELECT star, qualified SELECT star, multiplication, and function-call star argument remain distinct syntax roles.
31. Every accepted request has one canonical raw syntactic structure determined without catalog contents or TypeResolver results.
32. Original source and derived lexical payload backing outlive every borrower, or required payload is materialized before the earlier owner is released.
33. `SourceSpan` is a half-open byte interval over the original request; an interval value may outlive source storage, but dereferencing it may not.
34. Token, raw-AST, and bound-object lifetimes are independent provided every surviving payload reference remains valid.
35. A front-end resource failure returns `FrontEndResourceLimit` or `OutOfMemory` rather than changing grammar or exposing a successful partial syntax object.
36. Operational resource policy never permits truncation, AST reordering/deduplication, weakened literal provenance, or uncontrolled process failure.

## 18.17 Front-end resource and failure boundary

Chapter 18 defines no fixed SQL-language maximum merely for implementation
convenience on request, token, string-literal, or identifier byte length;
parenthesis or expression depth; CASE-arm, IN-list, projection, VALUES-row,
VALUES-item, or statement count; or raw-AST node count/depth. A correctness or
domain limit defined by another architecture owner remains authoritative,
including persisted-field widths, object-ID domains, tuple/page or index-key
limits, and scalar value domains. This rule prevents a parser implementation
limit from masquerading as grammar; it does not erase canonical semantic
limits.

A conforming implementation MAY enforce finite operational budgets for memory,
call-stack safety, parser work, AST construction, nesting, node count, or other
request-processing resources. Such budgets are runtime resource policy, not
SQL grammar, and their numerical values and decomposition are
implementation/configuration choices. The same syntactically valid request may
therefore complete under one resource configuration and fail under another
without changing whether it belongs to the language.

### 18.17.1 Structured front-end resource outcomes

The relevant failure classification is:

| Condition | Canonical category |
|---|---|
| invalid source byte or token form | `LexerError` |
| structurally invalid grammar | `ParserError` |
| unknown or incompatible name/type/semantic property | existing bind/type owner |
| deliberate configured or safety front-end budget reached | `FrontEndResourceLimit` |
| required memory allocation cannot be satisfied | `OutOfMemory` |
| execution spill I/O failure | existing `SpillIOError` under §39.3 |
| optimizer configured budget reached | existing `OptimizerResourceLimit` under §39.4 |

The two resource categories available to front-end work are exactly
`FrontEndResourceLimit` and `OutOfMemory`.

`FrontEndResourceLimit` means that the front end deliberately refused further
work because a configured or safety budget was reached before uncontrolled
resource failure. A nesting-safety guard, bounded parser-work or AST-node
budget, or checked front-end memory budget may produce this result. The
architecture does not prescribe which guards exist or their numerical values.
The result means **resource guard reached**, not invalid SQL syntax.

`OutOfMemory` is the cross-layer operational cause for inability to satisfy a
required memory allocation. An allocation failure during lexing, parsing,
payload decoding, raw-AST construction, or binding remains `OutOfMemory`; it
MUST NOT be relabeled `LexerError`, `ParserError`, `BindError`, or `TypeError`
because of the pipeline stage in which it occurred. `FrontEndResourceLimit`
and `OutOfMemory` therefore distinguish deliberate refusal at a checked budget
from failure to satisfy a required allocation.

Resource failure MUST NOT be reported as proof that a request is
syntactically invalid. The front end returns the failure encountered in its
canonical source-processing order; if a resource failure prevents continuation,
it need not prove whether unprocessed suffix bytes would have produced another
error. This operational rule does not change grammar closedness or any
otherwise-defined demanded-error order.

### 18.17.2 Safe failure and complete-result boundary

Uncontrolled stack overflow, memory corruption, uncaught allocation failure,
undefined behavior, or process abort caused solely by parser depth is not a
conforming result for valid but resource-intensive syntax. The implementation
MUST use a resource-safe technique or detect/refuse work before uncontrolled
failure and return the corresponding structured resource cause. No particular
recursion, iteration, or stack representation is required.

On `FrontEndResourceLimit` or `OutOfMemory`, the front-end operation fails as a
whole. It MUST expose either a complete canonical raw syntax object or a
structured failure, never a partially constructed token stream/raw AST as a
successful bindable or executable request. Private lexical, parser, binder,
and front-end backing state MUST unwind without dangling payload references, an
active partial AST, a bindable prefix, or leaked ownership that changes later
semantics.

Resource exhaustion never authorizes semantic fallback. It MUST NOT truncate a
token, identifier, or string; ignore trailing source or statements; drop,
deduplicate, or reorder AST children; truncate CASE/IN/VALUES lists; weaken
lossless numeric provenance; approximate or change literal types; bind or
execute only a prefix; or reinterpret a resource-heavy construct. If sufficient
resources cannot preserve exact bytes, source order, multiplicity, provenance,
and AST completeness, the operation returns a resource failure.

### 18.17.3 Transaction, batch, and persistence ownership

Lexing, parsing, raw-AST construction, and binding publish no
transaction-owned persistent database mutation, so their resource cleanup
requires no Chapter-18 physical undo protocol. `FrontEndResourceLimit` and
`OutOfMemory` identify the operational cause; §39.1 alone determines the
statement/transaction outcome under its existing publication boundary and any
independent stronger condition. Chapter 18 creates no transaction state or
retry rule.

For a multi-statement request, front-end resource failure MUST NOT fabricate a
successful parse/bind result for the completed prefix. The existing batch and
transaction owners determine the status of any earlier statement whose
execution had independently completed; §18.10.1 framing and §21.17 behavior
are unchanged.

Front-end budgets, backing storage, and failed parser state are runtime-only.
They do not alter catalogs, WAL, checkpoints, `database.control`, tuple/page
formats, transaction status, or recovery, and recovery does not reconstruct a
failed front-end operation.
