## Verdict

**CHAPTER 17 SQL TYPE / VALUE VERIFICATION SYNCHRONIZATION — COMPLETE.**

All required V17-1–V17-12 methodology families are closed. The final 450-obligation coverage map has no partial, missing, or contradictory entries.

**CHAPTER 17 — FULLY REVIEWED AND CLOSED.**

## Repository state

Initial state:

- Working tree: clean
- Index: clean
- HEAD: `272677ea581b92ad23f06a5e14fa89d7a30fbd46`
- Pre-existing Architecture state: committed and preserved
- Review artifacts: not read, modified, moved, or staged

Only [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md) was task-modified.

## Verification organization

The existing [Type-System Property Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6805) section was expanded as Chapter 17’s single detailed verification owner. This avoids creating a competing methodology location while retaining precise delegation to existing tuple, index, catalog, subquery, execution, aggregate, statistics, and DML procedures.

Added verification sections:

- [Deterministic harness and fixture discipline](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6813)
- [Independent scalar oracles](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6852)
- [TypeId and closed registry](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6885)
- [Contextual NULL and SQL UNKNOWN](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6912)
- [BOOLEAN and 3VL](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6938)
- [Literals and integer arithmetic](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6974)
- [Casts, coercion, and comparison](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7003)
- [FLOAT64 verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7029)
- [VARCHAR verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7060)
- [DATE/TIMESTAMP verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7081)
- [CASE, IN, demand, and folding](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7103)
- [Equality modes and Value lifetime](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7123)
- [Storage/catalog/history handoffs](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7148)
- [PersistedScalarV1 byte verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7171)
- [Error/resource/DML composition](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7204)
- [Unsupported registry and platform determinism](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7220)
- [Mandatory matrices](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7237)
- [Atomic obligation map](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:7476)

## Atomic obligation inventory

Actual inventory: **450 atomic obligations**.

Domain totals:

```text
A=4   B=10  C=8   D=14  E=5   F=3   G=6   H=10
I=7   J=7   K=23  L=6   M=8   N=11  O=0   P=12
Q=7   R=6   S=12  T=11  U=17  V=14  W=7   X=1
Y=10  Z=6   AA=6  AB=9  AC=11 AD=13 AE=10 AF=5
AG=8  AH=9  AI=4  AJ=5  AK=15 AL=10 AM=8  AN=12
AO=4  AP=2  AQ=12 AR=4  AS=9  AT=10 AU=7  AV=6
AW=5  AX=5  AY=16 AZ=3  BA=6  BB=8  BC=3
```

Domain O has zero direct rows because every literal obligation has a more specific primary classification under P–S or TypeResolver under AJ.

Coverage totals:

```text
COMPLETE:       450
PARTIAL:          0
MISSING:          0
CONTRADICTORY:    0
```

## Independent oracles

The synchronized methodology defines independent oracles for:

- Fixed TypeIds and the closed registry
- Contextual NULL resolution
- SQL three-valued logic
- Widened mathematical integer arithmetic
- Quotient/remainder semantics
- FLOAT64 bit classification and total order
- Per-operator binary64 rounding
- VARCHAR byte equality/order
- Proleptic-Gregorian DATE and TIMESTAMP conversion
- Cast/coercion matrices
- Literal classification
- PersistedScalarV1 bytes
- Ordinary tuple scalar handoff
- Index order preservation
- Historical SchemaVer resolution
- Folding/runtime equivalence
- Equality-mode consistency
- Error classification

Production parsing, formatting, encoding, comparison, and dispatch cannot generate their own expected values.

## Semantic methodology results

### Type, NULL, and 3VL

- TypeIds 1–7 are byte-exact and stable.
- TypeId 0, 8, and a large unknown value are covered.
- Runtime tag values are deliberately permuted from TypeIds.
- The scalar registry is closed.
- The unresolved type marker is binder-only, nonpersistent, and eliminated before execution.
- Every contextual NULL case and every underconstrained `TYPE_ERROR` case is covered.
- Typed NULL exists for every concrete type and every legal cast edge.
- SQL UNKNOWN remains nullable BOOLEAN NULL.
- No UNKNOWN TypeId, fourth truth state, or unresolved executor value is assumed.
- Complete NOT/AND/OR matrices and demand behavior are covered.
- Ordinary NULL comparison remains distinct from Chapter-8 physical NULL order.
- WHERE/HAVING/JOIN ON and IN/NOT IN consumption are covered.

### Integers

Coverage includes:

- Exact INT32/INT64 domains
- Direct-negative minimum literal construction
- Parenthesized and unary-plus boundaries
- Checked unary minus
- Checked addition, subtraction, and multiplication
- Mixed INT32/INT64 result types
- Division toward zero
- Dividend-sign remainder
- Zero-divisor errors
- `MIN/-1` division and remainder overflow
- Strict NULL behavior
- No host signed wrap or UB

### FLOAT64

Coverage includes:

- Every binary64 semantic class
- Normals, subnormals, infinities, signed zeros, and NaN payload variants
- `+0/-0` equality and key normalization
- Canonical-equivalent NaNs and ordering above positive infinity
- One nearest/ties-even rounding per bound scalar operator
- Extended-precision, reassociation, and FMA discriminators
- IEEE FLOAT division rather than integer zero-divisor semantics
- FLOAT remainder rejection
- Integer conversion and range/error behavior
- Literal grammar and explicit VARCHAR parsing
- Canonical shortest-round-trip formatting
- Ordinary tuple bit preservation
- Index normalization
- UNIQUE/hash/grouping consistency
- PersistedScalarV1 canonical metadata NaN

### VARCHAR

Coverage includes:

- Arbitrary finite bytes
- Empty versus NULL
- Embedded NUL
- High bytes and signed-char independence
- Exact byte equality
- Unsigned lexicographic and prefix ordering
- No UTF-8, normalization, locale collation, or C-string behavior
- Full-consumption parsing for every legal target
- Canonical scalar-to-VARCHAR formatting
- `VARCHAR(n)` rejection
- Complete tuple boundaries `8135/8136`
- Index-key 1024-byte boundary without narrowing the semantic domain

### DATE and TIMESTAMP

Coverage includes:

- Complete signed carrier domains
- Exact epochs and units
- Negative/pre-epoch values
- Gregorian leap rules
- Text years 0001–9999
- Timestamp fractions absent or 1–6 digits
- More-than-six-digit rejection
- Invalid calendar/time values
- Timezone suffix rejection and timezone independence
- Checked DATE→TIMESTAMP multiplication
- Prior-midnight flooring for negative TIMESTAMP→DATE
- Semantic-domain versus textual-domain distinction
- Temporal arithmetic and implicit cross-comparison rejection

### Casts, comparison, and operators

The final methodology contains:

- Complete 7×7 cast matrix
- Separate assignment/default coercion matrix
- Complete 7×7 comparison matrix
- Complete arithmetic matrix
- Direct-cast/no-chaining checks
- Identity, widening, explicit-only, and forbidden edges
- NULL propagation for every legal cast
- Mixed numeric comparison around `2^53`
- BOOLEAN equality-only verification
- Exhaustive unary/binary registry complements
- No implicit VARCHAR, BOOLEAN, or temporal conversions

### CASE, IN, folding, and demand

Covered:

- CASE type unification and contextual NULL
- All-NULL CASE failure
- Missing ELSE typed NULL
- First-TRUE selection and lazy result evaluation
- IN common type and left-once evaluation
- First-TRUE termination
- UNKNOWN accumulation
- Exact NOT IN behavior
- Fold/runtime value and error equivalence
- Dominating versus nondominating constant errors
- EXISTS/subquery demand composition
- No estimate-based demand inference
- Checked-overflow reassociation prohibition
- FLOAT contraction/FMA prohibition
- Chapter-29 aggregate-state separation

### Value and lifetime

- Generic Value’s semantic contract is tested without mandating layout.
- The explicitly null-only pre-context state remains permitted and nonpersistent.
- Escaping VARCHAR bytes remain owned and valid after page/chunk recycling.
- No persistent bytes derive from runtime Value layout.
- Generic Value is not required as the hot executor representation.

## Storage and persistence handoffs

Three separate paths are verified for every type:

```text
semantic value -> ordinary tuple -> semantic value
semantic value -> PersistedScalarV1 -> semantic value
semantic value -> index key -> semantic order/equality property
```

The procedures preserve these distinctions:

- Ordinary tuple FLOAT64 preserves every payload bit.
- Index keys normalize FLOAT zeros and NaNs.
- PersistedScalarV1 canonicalizes metadata NaNs.
- Tuple NULL uses Chapter 5’s bitmap and schema.
- PersistedScalarV1 typed NULL retains a concrete TypeId.
- Historical tuples use the exact `(TableId,SchemaVer)` descriptor.
- TypeId meanings are stable, nonreused, and never rebound.
- DefaultValueBlob contains one final destination-typed scalar and distinguishes typed DEFAULT NULL from no default.

## PersistedScalarV1

Verified exact contract:

- No embedded scalar version field
- Enclosing grammar selects v1
- 16-byte little-endian header
- TypeId at offset 0
- Flags at offset 4
- Payload length at offset 8
- Zero `reserved32` at offset 12
- `IS_NULL` as the only legal flag
- `Align8(16 + payload_length)` extent
- Zero padding
- Exact BOOL/INT32/INT64/FLOAT64/DATE/TIMESTAMP/VARCHAR payloads
- Canonical quiet NaN `0x7ff8000000000000`
- Wrong lengths, flags, reserved bytes, padding, TypeIds, BOOLEAN bytes, NaNs, and truncation rejected
- Required metadata corruption separated from advisory-statistics generation invalidation

## Error and environment methodology

The error matrix distinguishes:

- `TYPE_ERROR`
- `INVALID_LITERAL`
- `INVALID_CAST`
- `NUMERIC_OVERFLOW`
- `DIVISION_BY_ZERO`
- `INVALID_DATE`
- `INVALID_TIMESTAMP`
- `CORRUPT_HEAP`
- Required metadata corruption
- Advisory statistics fallback
- Resource/OutOfMemory failure

No error is silently converted to NULL. Chapter 17 determines scalar meaning; Chapter 15 and §39.1 retain transaction-state ownership before and after DML publication.

Locale, timezone, native endianness, char signedness, floating environment, excess precision, and contraction cannot change observable results.

## Mandatory matrices

All required matrices are present:

1. Type
2. Cast
3. Assignment/default coercion
4. Comparison
5. Arithmetic
6. NULL/3VL
7. FLOAT64
8. VARCHAR
9. Temporal
10. Literal
11. Type/storage handoff
12. PersistedScalarV1 bytes
13. Error
14. Demand/error dominance
15. Cross-chapter composition
16. High-level domain/case

Single-defect fixture policy and deterministic no-sleep event barriers are explicit. Implementation freedom is preserved.

## Final reread: all 232 questions

Every question was individually checked; ranges are grouped here for readability.

| Questions | Result |
|---|---|
| 1–6 Type registry | YES |
| 7–16 unresolved type / NULL | YES |
| 17–28 BOOLEAN / 3VL | YES |
| 29–41 integers | YES |
| 42–65 FLOAT64 | YES |
| 66–85 VARCHAR | YES |
| 86–101 temporals | YES |
| 102–115 cast/coercion | YES |
| 116–124 comparison | YES |
| 125–135 operators/expressions | YES |
| 136–145 folding/determinism | YES |
| 146–160 Value/storage | YES |
| 161–180 PersistedScalarV1 | YES |
| 181–187 historical/catalog | YES |
| 188–195 equality modes | YES |
| 196–207 errors | YES |
| 208–215 unsupported scope | YES |
| 216–230 documentation model | YES |
| 231 new architecture rule invented? | **NO** |
| 232 Chapter-17 verification complete? | **YES** |

No item required N/A.

## Documentation-model assessment

| Question | Result |
|---|---|
| Current implementation narration introduced? | NO |
| Phase-2 narration introduced? | NO |
| DEVELOPMENT sequencing introduced? | NO |
| Review/devlog history introduced? | NO |
| Architecture unnecessarily duplicated? | NO |
| ARCHITECTURE.md modified? | NO |
| Task-created sections procedural/analytical? | YES |
| Independent of current implementation? | YES |
| Time-independent? | YES |
| Runtime/expression ordering preserved? | YES |
| Runtime Value implementation freedom preserved? | YES |
| TypeResolver implementation freedom preserved? | YES |
| Parser/binder implementation freedom preserved? | YES |
| FLOAT implementation freedom preserved subject to semantics? | YES |
| Separation of concerns preserved? | YES |

F17-E1 terminology is consumed correctly: “unresolved type marker” is used only for binding; SQL UNKNOWN remains the 3VL result.

## Cross-reference and regression results

Validated composition with:

- Chapters 4, 5, 8, 15, 16, 17, 18, 20, 21, and 29
- Chapters 34–35
- §39
- §41

Existing parser, binder, subquery, tuple, index, catalog, logical-rewrite, execution, aggregate, DML, and statistics verification remains intact. The prior broad Chapter-17 property requirement is preserved and made deterministic and complete.

Frozen Chapter-17 semantic questions: **NONE**.

## Verification-family status

| Family | Status |
|---|---|
| V17-1 closed TypeResolver/type registry | CLOSED |
| V17-2 literals/direct-negative integers | CLOSED |
| V17-3 contextual NULL/unresolved marker | CLOSED |
| V17-4 casts/coercions | CLOSED |
| V17-5 VARCHAR parse/format | CLOSED |
| V17-6 DATE/TIMESTAMP | CLOSED |
| V17-7 PersistedScalarV1 | CLOSED |
| V17-8 FLOAT64 equivalence | CLOSED |
| V17-9 error taxonomy | CLOSED |
| V17-10 storage handoff | CLOSED |
| V17-11 folding/demand | CLOSED |
| V17-12 unsupported registry | CLOSED |

## Diff classification

Task-created logical hunks:

| Class | Content |
|---|---|
| A | Deterministic type/value harness |
| B | TypeId/closed registry |
| C | Unresolved type/typed NULL |
| D | 3VL |
| E | Integer literals/arithmetic |
| F | FLOAT64 |
| G | VARCHAR |
| H | DATE/TIMESTAMP |
| I | Cast/coercion |
| J | Comparison |
| K | Operator registry |
| L | CASE/IN/demand |
| M | Folding/runtime equivalence |
| N | Equality modes |
| O | Generic Value/lifetime |
| P | Tuple/index/catalog handoff |
| Q | PersistedScalarV1 |
| R | Historical schema/defaults |
| S | Error/resource/DML composition |
| T | Unsupported registry |
| U | Mandatory matrices |
| V | 450-row obligation map |
| W | Independent oracles |
| X | Cross-references/navigation |
| Y | Local wrapping |

## Final status

- Final working tree: `M docs/VERIFICATION.md`
- Final index: clean
- Final HEAD: `272677ea581b92ad23f06a5e14fa89d7a30fbd46`
- `git diff --check`: passed
- External repository changes observed: none
- Architecture task-change: none
- Review artifacts modified/staged: none
- Chapter 18: unchanged; direct review **NOT STARTED**
- §31.7: unchanged
- Appendix C: unchanged
- Builds/tests/benchmarks: not run
- Implementation work: none
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
