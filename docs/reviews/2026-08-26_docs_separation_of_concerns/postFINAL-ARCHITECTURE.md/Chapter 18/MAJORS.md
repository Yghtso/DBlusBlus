# D18-M1–D18-M9 integration verdict

**SUCCESS — all nine accepted decisions are integrated and closed.**

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) was modified, exclusively within Chapter 18. Full statement and expression grammar remain intentionally open.

## Git state

| State | Working tree | Index | HEAD |
|---|---|---|---|
| Initial | Clean | Clean | `558e4f480c48fd5c02ea9962db7ec56c5ebcb6fd` |
| Final | `M docs/ARCHITECTURE.md` | Clean | unchanged |

There was no pre-existing repository state to preserve. No review artifact was read, modified, created, or staged.

## Sections modified

- [§18.2 Token model](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14210)
- [§18.3 Token classes](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14257)
- [§18.4 Identifier rules](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14321)
- [§18.6 Numeric literals](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14373)
- [§18.7 Comments](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14409)
- [§18.10 Initial statement set](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14460)
- New [§18.10.1 SQL request and statement-batch framing](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14499)
- [§18.13 AST contract](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14656)
- [§18.15 Expression precedence](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14746)
- [§18.16 Front-end invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14773)

Chapter 19 begins at line 14797 and is unchanged.

## Integrated lexical rules

### D18-M1 — source domain

SQL source is now explicitly one length-delimited byte sequence. Structural syntax is ASCII-defined; source-wide UTF-8 validity is neither required nor used for tokenization.

Locale-sensitive classification, Unicode syntax classification, and signed-char-dependent behavior are forbidden.

Payload rules are explicit:

- Strings may contain arbitrary bytes, including NUL.
- Quoted identifiers may contain arbitrary non-NUL bytes.
- Comments may contain arbitrary non-NUL bytes.
- NUL everywhere outside strings is rejected.
- Other nonadmitted bytes produce positioned lexical errors.

### D18-M2 — whitespace and comments

Whitespace is exactly:

```text
09 0A 0B 0C 0D 20
```

Line comments:

- begin with `--`;
- end before the first CR or LF;
- may terminate at EOF;
- leave CR/LF to the whitespace grammar;
- make CRLF deterministic without normalization.

Block comments:

- begin with `/*`;
- close at the first following `*/`;
- are non-nested;
- treat inner `/*` as payload;
- produce lexical error if EOF precedes the closer.

Comment delimiters inside strings and quoted identifiers are inert.

## D18-M3 — identifiers

Unquoted identifiers now use exactly:

```text
[A-Za-z_][A-Za-z0-9_]*
```

Canonicalization maps ASCII `A`–`Z` to `a`–`z`. No locale or Unicode folding participates.

Quoted identifiers:

- use `"..."`;
- decode `""` inside the identifier to one quote byte;
- preserve exact bytes and case;
- permit high-byte values;
- forbid NUL;
- reject unterminated input;
- reject an empty decoded identifier.

The raw AST preserves canonical bytes, relevant quoted-source distinction, and source span. Catalog lookup and object-ID assignment remain binder-owned.

## D18-M4 — closed keyword model

All keywords are reserved when unquoted. There are no contextual or nonreserved keywords. Recognition is ASCII case-insensitive, and quoting permits a reserved spelling to be used as an identifier.

The inserted registry is:

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

No external dialect keyword list or compatibility alias was imported.

## D18-M5 — closed symbolic-token model

The inserted registry is:

| Class | Exact tokens |
|---|---|
| Punctuation | `(` `)` `,` `.` `;` |
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `=` `<>` `<` `<=` `>` `>=` |

Additional rules:

- `!=` remains unsupported.
- Arbitrary generic operator tokens are forbidden.
- The longest registered symbolic spelling wins.
- `--` and `/*` are recognized before component operator tokens.
- String/identifier delimiters enter their lexical mode before payload interpretation.
- Unknown symbol-like bytes produce lexical error.

## D18-M6 — numeric boundaries

Leading `+` and `-` remain separate symbolic tokens.

A digit begins exactly one unsigned Chapter-17 integer or FLOAT token. A leading `.` is punctuation because Chapter 17 does not permit dot-leading numerics. `.5` and `1.` remain excluded as FLOAT tokens.

The token/raw AST must retain a lossless spelling, checked magnitude, exact decimal object, or equivalent representation. Magnitude `9223372036854775808` must survive through binding without premature signed-integer or host-double conversion.

Chapter 17 remains the sole semantic owner of type selection, overflow, direct-negative classification, and FLOAT conversion.

## D18-M7 — direct-negative provenance

The raw AST must preserve a parenthesized-expression boundary, conceptually `AstParenthesizedExpression` or an equivalent property.

The required distinction is now explicit:

```text
-9223372036854775808
    UnaryMinus
        NumericLiteral(...)

-(9223372036854775808)
    UnaryMinus
        ParenthesizedExpression
            NumericLiteral(...)
```

Parentheses break the direct-negative special form. The weak “where practical” wording was removed from the invariant summary.

No parser-side semantic integer classification was introduced.

## D18-M8 — source order and multiplicity

Every ordered raw-AST collection must preserve exact source order and multiplicity. Parser-side sorting, canonical reordering, and deduplication are forbidden.

Named collections include:

- binary left/right operands;
- unary provenance;
- CASE arms;
- IN-list items;
- SELECT projection items;
- VALUES rows and row items;
- INSERT columns;
- UPDATE assignments;
- GROUP BY items;
- ORDER BY items;
- function and aggregate arguments;
- table/column declarations;
- index key columns;
- qualified-name components;
- statement-batch entries.

Later semantic layers may still reject duplicates. No C++ container or allocation representation was mandated.

## D18-M9 — request framing

A request contains one or more statements in source order.

- Semicolon is required between adjacent statements.
- One final semicolon is optional.
- Empty statements are forbidden.
- Leading/repeated/extra semicolons are `ParserError`.
- Whitespace/comment-only input is an empty-request `ParserError`.
- Successful parsing consumes all nontrivia input.
- No trailing suffix is ignored.
- The request AST preserves statement order.

The `ANALYZE table_name;` snippet was minimally synchronized to separate statement grammar from request-level semicolon framing.

Parser recovery remains §21.17-owned. Batch transaction/execution behavior remains with later owners.

## Analytical rationale

The integrated text records why:

- ASCII structure prevents locale/Unicode token divergence.
- Explicit ASCII folding produces stable Chapter-16 catalog-name bytes.
- Closed keywords and symbols make byte-to-token mapping deterministic.
- Parenthesis provenance is necessary for Chapter-17 literal semantics.
- Order and multiplicity affect Chapter-17 demand/error behavior and later duplicate validation.
- Explicit framing prevents ignored suffixes and implementation-dependent empty statements.

No review history or decision identifiers were written into Architecture.

## Regression assessment

| Area | Result |
|---|---|
| Input/whitespace/comments | Exact and internally consistent |
| String NUL semantics | Unchanged |
| Identifier/catalog handoff | Canonical bytes clarified; binder ownership unchanged |
| Keywords | Closed registry added without new productions |
| Symbolic tokens | Closed registry; no alias added |
| Numeric literals | Chapter-17 grammar unchanged |
| Direct-negative semantics | Chapter 17 unchanged; syntax provenance supplied |
| CASE/IN/AND/OR order | Semantics unchanged; AST preservation guaranteed |
| Statement execution | Unchanged |
| TypeIds | Unchanged; no parser TypeId added |
| Catalog identities | Unchanged |
| Chapter 19 binder boundary | Unchanged |

## Local reread answers 1–95

| Questions | Result |
|---|---|
| 1–9, D18-M1 | **YES individually** |
| 10–19, D18-M2 | **YES individually** |
| 20–33, D18-M3 | **YES individually** |
| 34–40, D18-M4 | **YES individually** |
| 41–51, D18-M5/M6 | **YES individually** |
| 52–58, D18-M7 | **YES individually** |
| 59–74, D18-M8 | **YES individually** |
| 75–85, D18-M9 | **YES individually** |
| 86–94, protected findings remain open/untouched | **YES individually** |
| 95, new frozen semantic question arose? | **NO** |

## Documentation-model assessment

| Question | Result |
|---|---|
| Project chronology introduced? | No |
| Current implementation narration introduced? | No |
| DEVELOPMENT sequencing introduced? | No |
| VERIFICATION recipes introduced? | No |
| PROJECT_STATE facts introduced? | No |
| History/devlog introduced? | No |
| Analytical architecture preserved? | Yes |
| Task-created prose timeless? | Yes |
| Representation freedom preserved? | Yes |
| Existing F18-N2 coupling left pending? | Yes |
| Binder boundary preserved? | Yes |
| Chapter-17 ownership preserved? | Yes |

## Decision and finding status

| Item | Status |
|---|---|
| D18-M1 | INTEGRATED / CLOSED |
| D18-M2 | INTEGRATED / CLOSED |
| D18-M3 | INTEGRATED / CLOSED |
| D18-M4 | INTEGRATED / CLOSED |
| D18-M5 | INTEGRATED / CLOSED |
| D18-M6 | INTEGRATED / CLOSED |
| D18-M7 | INTEGRATED / CLOSED |
| D18-M8 | INTEGRATED / CLOSED |
| D18-M9 | INTEGRATED / CLOSED |
| F18-B1 | CLOSED |
| F18-B2 | CLOSED |
| F18-B3 | CLOSED |
| F18-B4 | CLOSED |
| F18-B7 | CLOSED |
| F18-B8 | CLOSED |
| F18-B5 | **OPEN overall; request/batch framing subproblem CLOSED** |
| F18-B6 | **OPEN** |
| F18-M1 | PENDING / UNCHANGED |
| F18-M2 | PENDING / UNCHANGED |
| F18-M3 | PENDING / UNCHANGED |
| F18-N1 | PENDING / UNCHANGED |
| F18-N2 | PENDING / UNCHANGED |
| F18-N3 | PENDING / UNCHANGED |
| F18-N4 | PENDING / UNCHANGED |

No new frozen semantic question arose.

**Chapter 18 remains FROZEN, NOT CLEAN, and NOT CLOSED.**

The next task remains the read-only closed-grammar extraction for F18-B5/F18-B6. It was not started. Chapter 19 direct review was not started.

## Diff ownership

Task-created hunks cover classifications A–U:

- A–D: source bytes, whitespace, line/block comments.
- E–G: unquoted/quoted identifiers and ASCII canonicalization.
- H–J: keyword/symbol registries and longest match.
- K–L: numeric/dot boundary and lossless handoff.
- M–N: parenthesis provenance and direct-negative invariant.
- O–P: order and multiplicity.
- Q–R: framing and EOF/trailing tokens.
- S–U: invariant synchronization, exact references, and rationale.
- V: no separate nonsemantic wrapping hunk was needed.

Final diff: 247 insertions, 23 deletions, all in Chapter 18.

## Final checks

- Files changed: `docs/ARCHITECTURE.md` only.
- Index: clean.
- HEAD: unchanged.
- `git diff --check`: passed with no output.
- No external repository change was observed.
- No review artifact was modified or staged.
- No source, tests, builds, benchmarks, or scaffolding were created.
- No build, test, benchmark, stage, commit, or devlog occurred.
- Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
