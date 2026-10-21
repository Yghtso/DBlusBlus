# Chapter 25 — Vectorized Expression Execution Verification

This family verifies `ARCHITECTURE.md` §§25.1–25.8; the scalar, relational, DML,
vector, resource, and result-interface owners remain authoritative. V25-R maps each
obligation to its deterministic procedure and independent oracle. A fixture may use an adapter or symbolic model; no executor class, mask API,
candidate struct, allocator, SIMD instruction set, or synchronization primitive is required.

## V25-A — Expression forms and independent scalar foundation

Build a test-side interpreted, typed expression tree from the literal Chapter-17 registry,
using **Independent scalar oracles** under **Type-System Property Tests**. Call this `SX`:
arbitrary-precision integer operations and range checks; exact binary64 rounding at each
operator; length-delimited unsigned bytes; independent ASCII and Gregorian parsers;
literal three-valued truth tables. Never obtain expected results, types, causes, or
rounding from production kernels, dispatch registries, formatters, or numeric enum values.
Use the already-resolved fixture types; poison runtime binder/type-inference entry points.

`DX` independently traverses that tree in Chapter-17/§20.17.5 child order and records
semantic demand, values, and the responsible first failure within each scalar occurrence.
`PX` is V20-18's separately authored source-origin table, including half-open byte spans,
explicit/implicit cast provenance, and canonical semantic source-occurrence order.
`RX` maps demanded input occurrences to result occurrences; its test labels distinguish
duplicates but are not runtime row IDs or error-ranking keys. `EX` reduces ordinary
non-DML candidates using V25-I/J. `IX` classifies malformed states independently in V25-N;
`OX` is V25-O's publication state machine. Reuse V23's `LV/VV/BS/LG/LM/PF` and V24's
exact extent/accounting/error models through V25-Q.

The expression-form matrix is exhaustive for this handoff. Each row uses its upstream
result-type/NULL owner, `DX` for parent domain D and child order, and `SX` as value oracle;
ordinary failures use V25-I/J except where the specialized owner is stated. Apply these oracles to every applicable row. Vector method denotes permitted observation, not required class shape.

| Form | Scalar and result-type/NULL owner | Parent/child demand and order | Vector execution observation | Error owner / oracle |
|---|---|---|---|---|
| literal / BoundConstant | §§17.2–17.5, 19.6 | D; no children | repeated typed value, possibly CONSTANT | folding §17.10.2; SX |
| input reference | §§19.3, 19.6, 20.2 | D; no children | resolved column/normalized view | IX; slot model |
| unary `+`, `-` | §17.6.1 | child on D | typed fixed-width path | EX/SX |
| binary `+`, `-`, `*`, `/`, `%` | §17.6.2; only registered overloads | left then right on surviving scalar evaluations | typed normalized inputs | EX/SX |
| ordinary comparisons | §17.7.1 | left then right, strict after children | BOOLEAN or Filter selection | EX/SX |
| NOT | §17.7.2 | child on D | nullable BOOLEAN | EX/SX |
| AND | §§17.7.2–17.7.3 | left then restricted right | V25-C subset | EX/DX/SX |
| OR | §§17.7.2–17.7.3 | left then restricted right | V25-C subset | EX/DX/SX |
| CAST, explicit or inserted | §§17.8, 19.6 | child on D | resolved cast; no new coercion | EX/PX/SX |
| searched CASE | §§17.7.3, 17.9.1 | source-ordered conditions; selected result only | composed subsets | EX/DX/SX |
| IS NULL / IS NOT NULL | §17.7.2 | child required, including child errors | non-null BOOLEAN | EX/DX/SX |
| IN-list / NOT IN-list | §§17.7.3, 17.9.2 | left once; entries left-to-right until TRUE | demand-restricted comparisons | EX/DX/SX |
| scalar subquery | §20.14.4 | first demand initializes occurrence once per attempt | cached scalar/CONSTANT for D | §20.14.12; V20-12/14 |
| EXISTS / NOT EXISTS | §20.14.5 | existence work only; lazy occurrence | cached non-null BOOLEAN | §20.14.12; V20-13/14 |
| IN-subquery / NOT IN-subquery | §20.14.6 | left before lazy complete build; then probe | vectorized probes for D | §20.14.12; V20-13/14 |
| named scalar functions | §17.9.3 empty registry | no valid invocation | no executable function path | negative state V25-N |

Aggregate nodes are not ordinary scalar reduction kernels: feed successfully evaluated
typed argument occurrences into §29.3's independent aggregate oracle: exact mathematical
integer/count state or V29-H's admitted binary64 reduction tree, explicit special-value
flags, and semantic descriptor
ordinals. Compare argument values, NULLs, multiplicity, and demanded child failures before
finalization; competing final range failures use the lowest aggregate ordinal, not EX.
Never use a scalar left fold as the aggregate oracle. No unsupported functions, volatility
effects, FLOAT remainder, string concatenation, or convenience predicates are fixture semantics.

Use an abstract dispatch/allocation trace, or equivalent inspection, to check batch-level
typed execution: normalization and operator/type/kind selection outside the per-row
representation branch, no generic Value construction per hot cell, and a useful all-valid
arithmetic path without per-row validity work. Compare both all-valid and nullable paths
with SX. Do not impose instruction counts, one kernel table, or throughput thresholds.
Digest immutable bound/logical meaning before and after invocation/reuse; query state may
change, but the digest and source origins must not. V22-A supplies the ownership check.

## V25-B — Generic demanded-set handoff

For active logical domain A, author D as a subset of occurrence positions in A, independently
of payload indices. DX derives each child's domain from the parent's D and the scalar
control-flow result. Compare semantic invocation traces, not raw CPU evaluation counts.
For every descendant check membership, no ancestor-excluded occurrence restored, and no
required occurrence dropped. Ordinary strict operators evaluate children in semantic order;
a NULL child suppresses the non-NULL payload operation, not evaluation of another required
child or its error. Do not apply an invented function-wide strictness policy.

Encode the same D as an index list, SelectionVector-like view, bitmap, and partitioned
domains in the fixture adapter. Production need not expose each encoding. Decode to DX
before comparison. Poison inactive capacity, invalid dictionary positions, and stale slots;
require no access or semantic effect. An out-of-A demand is IX-invalid before access.
For D empty, expect zero per-row evaluations, candidates, and result occurrences even if
storage capacity is positive. Use row-dependent bound fixtures so mandatory constant
folding cannot be confused with runtime zero-row suppression.

Allow a speculative trace to compute outside D, including a would-be earlier-ranked
failure. Its value must never appear and its ordinary error must not enter EX. This permits
nonsemantic speculation without weakening the independently derived demand relation.

## V25-C — Conditional demand matrix and nested composition

For every row below, parent demand is D unless empty; DX/SX supplies the condition/value
oracle. Physical speculative work is permitted only with V25-B nonvisibility. A semantic
error may surface only from a demanded child reached before the scalar occurrence fails.
No physical mask encoding is part of the expected answer.

| Form/child | Child demanded subset | Value/control rule | Semantic error may surface? | Oracle / status |
|---|---|---|---|---|
| arithmetic ordinary children | D, left-to-right until occurrence failure | strict after child evaluation | yes, even sibling of NULL | DX/SX |
| comparison children | same | NULL produces UNKNOWN after children | yes | DX/SX |
| CAST child | D | NULL skips parse/payload conversion, not child | yes | DX/SX |
| AND RHS | D where L is TRUE or NULL | FALSE skips RHS | only subset | DX/SX |
| OR RHS | D where L is FALSE or NULL | TRUE skips RHS | only subset | DX/SX |
| CASE condition | D not selected by earlier TRUE | FALSE/NULL continue | only remaining subset | DX/SX |
| selected THEN | remaining rows whose WHEN is TRUE | exactly selected result | yes | DX/SX |
| unselected THEN | empty for excluded rows | no value contribution | no | DX/SX |
| selected ELSE | D with no TRUE WHEN | ELSE or typed NULL if absent | only actual ELSE | DX/SX |
| unselected ELSE | empty for already-selected rows | no contribution | no | DX/SX |
| IN left | D, once per scalar occurrence | preserve value for comparisons | yes | DX/SX |
| IN entry | D with no earlier TRUE comparison | NULL comparison records UNKNOWN; continue | yes until TRUE | DX/SX |
| zero-demand child | empty | no per-row evaluation | no | DX/RX |
Enumerate all nine AND and OR pairs and three NOT inputs. Add FALSE AND error, TRUE AND
error, NULL AND error; TRUE OR error, FALSE OR error, NULL OR error. NULL requires RHS in
both operators, so NULL AND FALSE is FALSE and NULL OR TRUE is TRUE; other combinations
follow the literal 3VL tables. Use row-dependent error operands, not unsupported volatile
functions. Compare demanded RHS domains independently of final truth values.

In one mixed CASE domain, exercise first WHEN TRUE, first FALSE then second TRUE, WHEN
NULL, selected ELSE, absent ELSE, selected-branch error, unselected THEN error, unselected
ELSE error, and an error in a condition after an earlier TRUE. For
`CASE WHEN p THEN (a AND b) ELSE c END`, derive T/E from p, pass only T to AND, and derive
its RHS subset inside T. Repeat with nested OR/CASE/IN; ancestor exclusions never reappear.
For IN/NOT IN use a match before an error, an error before a match, NULL with no match,
NULL then match, duplicate values, and NULL left with an erroring list entry. Verify
left-once, list order, no stop merely on UNKNOWN, first-TRUE stop, and exact NOT negation.

## V25-D — Resolved slot/schema/DataChunk mapping

Extend V22-B's independent SM map: `LogicalSlotId -> S[j] -> DataChunk.columns[j]`, including
the expected TypeId. Poison runtime name/BindingId lookup and supply misleading equal
values/aliased storage. Distinct slots must remain distinct. Begin with slots A,B,C, then
place columns C,A,B and evaluate references to A/B. Change physical positions again and
require the same semantic result. Do not infer identity from the original ordinal.

| Case | Semantic identity / LogicalSlotId | Physical schema entry and column | Chapter-25 role | Result identity owner | Oracle / status |
|---|---|---|---|---|---|
| source reference | resolved source occurrence | S[j] / columns[j] | consume exact mapping | containing schema | SM |
| Project input | declared child slot | resolved j | compute selected expression | Project output schema | SM/SX |
| Filter input | preserved child slot | resolved j | compute predicate, not remove rows | Filter preserves schema | SM/SX |
| reordered physical schema | A/B unchanged | C,A,B gives A=1, B=2 | no stale ordinal | containing schema | SM |
| self-join equal values | distinct left/right slots | distinct entries even if backing shared | no value/pointer coalescing | join then Project | SM |
| derived-table remap | declared fresh outer slot | final remapped entry | no name-based fallback | derived boundary | SM/V20-14 |
| repeated semantic output | two declared output slots | corresponding output entries | sharing cannot merge identity | §20.7 / §22.3 | SM/PX |
| temporary expression vector | no independent slot required | evaluation-local storage | compute values, not allocate identity | containing output schema if exported | SM/RX |
## V25-E — Result occurrences and local output domain

RX pairs each demanded occurrence with exactly one successful scalar result in the same
evaluation-domain order. Compare occurrence lists, not sets of values or payload counts.
Filter owns removing FALSE/UNKNOWN rows; Project owns column placement. For the optional
direct comparison-to-selection path, independently apply Filter's TRUE-only selection to
SX's BOOLEAN results and compare the selected occurrence sequence. This specialized
operator path does not authorize generic scalar row removal.

| Case | Expected occurrences / active result domain | Exact value | Ownership | Publishable? | Identity owner / oracle / status |
|---|---|---|---|---|---|
| FLAT N | N corresponding to D | SX | output or valid borrow | on success | schema; RX |
| CONSTANT N | N despite one payload | repeated SX | scalar backing stable | on success | schema; RX/LV |
| DICTIONARY `[2,2,5,2]` | four, in listed correspondence | selected SX | full selected view stable | on success | schema; RX/LV |
| partial demand | exactly cardinality of D | SX on D only | owner for D | on success | operator domain; DX/RX |
| empty demand | zero; no stale active result | empty | no required payload | empty success allowed | operator; RX/OX |
| NULL | one per demanded occurrence | validity NULL; payload ignored | validity stable | on success | schema; VV/SX |
| borrowed result | one per D | exact stable view | LG | within valid interval | schema; RX/LG |
| computed VARCHAR | one per D | exact bytes/length | live result-byte owner | on success | schema; BS/LG |
| failed evaluation | no successful invocation result | partial storage inaccessible | cleanup owner | no | OX |
## V25-F — Representation substitutability

Compose V23-C/D's recursive LV normalizer with SX/DX/RX/EX. Encode identical logical
occurrence sequences using every applicable FLAT/CONSTANT/DICTIONARY combination,
including repeated and nested dictionaries, identity/nonidentity selections, and mixed
input kinds. CONSTANT is a valid alternative only for a repeated logical scalar sequence.
Compare value, NULL, child demand, candidate class, and result multiplicity; accept any
legal result representation. Normalize only valid intermediate dictionary indices.
With child cardinality at least six, `[2,2,5,2]` demands four occurrences even when three
refer to the same child.
Sharing or CSE may change physical work but must not deduplicate semantic occurrences.

## V25-G — NULL and validity

Use V23-E's VV oracle with all-valid, all-NULL, mixed validity, and inactive poison bits.
Poison NULL payloads with stale bytes, unusable pointer tokens, and zero denominators.
Require no payload read/dereference or payload-derived error when scalar semantics skip
the non-NULL operation. Separately evaluate a NULL left child and a failing required right
child: the right error is not suppressed. Compare fast/all-valid and nullable paths,
CONSTANT repeated validity, and dictionary-selected validity. Successful NULL output has
initialized validity; no exact payload is required for a NULL occurrence. IS NULL/IS NOT
NULL still evaluates its child and returns non-null BOOLEAN only on successful evaluation.

## V25-H — Scalar arithmetic, comparison, cast, and folding equivalence

Apply **Checked integer arithmetic and division**, **BOOLEAN, casts, and FLOAT64**, and
**Hash/comparison consistency** under **Expression Execution Tests** below, using SX rather
than vector kernels as oracle. Enumerate every registered overload, minimum/maximum and
their neighbors, -1/0/1, checked add/subtract/multiply/negate, signed quotient/remainder,
zero divisors, MIN/-1, and MIN%-1. Check both exact value and conceptual cause.

Reuse **FLOAT64 semantic, arithmetic, text, and cross-engine verification** for exact
rounding at each node, no observable FMA/reassociation/extended intermediates, NaN,
infinities, subnormals, and signed zeros. SQL comparison uses canonical total classes,
not host unordered comparison. FLOAT zero division is not integer DIVISION_BY_ZERO.

Reuse **Cast, coercion, and comparison verification** for every supported cast, inserted
promotion, NULL, malformed syntax, numeric range, DATE/TIMESTAMP calendars and temporal
unit conversions. Reuse its independent formatting corpus for scalar-to-VARCHAR. Compare
exact bytes including embedded NUL, high-bit bytes, strict prefixes, and equal four-byte
prefixes with different suffixes through BS/V23-F. A cached prefix is not an equality
proof; empty non-NULL strings are distinct from NULL and need no data dereference.

For folding/runtime equivalence use the independently interpreted tree and the existing
**Independent scalar oracles** folding/runtime row. Compare type, value/NULL, error cause,
per-node FLOAT result, and PX provenance. Mandatory dominating constant errors belong to
binding; runtime cases use column-dependent values or branches not proven demanded during
folding. Execution does not invent coercions or re-resolve overloads.

## V25-I — Ordinary candidate domain and owner exclusions

EX accepts only occurrence-level ordinary non-DML failures from DX. PX supplies responsible
subexpression provenance, not the containing Project's entire span by default. One scalar
occurrence with two potentially failing children contributes only the failure reached by
canonical child order; physically encountered errors in unreachable children are excluded.
Compose Filter and Project with V20-5/15: projection errors on rows never produced by Filter
are not hypothetical candidates. Do not equate one worker's reached prefix with the complete
semantic candidate domain. V25-J's early-stop and V25-M's resource boundaries govern stopping.

In this matrix P means PX's canonical origin/span, C means the independently derived cause,
and E means equality of every frozen diagnostic field. Physical order is irrelevant to
ordinary ranking in every row; an excluded owner's demand/termination rule still applies.

| Case | Candidate? | Provenance / cause / equivalence | D25 rank | Winner owner | Oracle / status |
|---|---|---|---|---|---|
| one ordinary failure | yes | P/C/E | sole class | §25.1.1 | DX/EX |
| distinct span starts | yes | P differs | smallest start | §25.1.1 | PX/EX |
| same start, unequal length | yes | P differs | shorter span | §25.1.1 | PX/EX |
| exact span, distinct semantic occurrences | yes | origin differs | semantic occurrence order | §25.1.1 | PX/EX |
| same-origin invalid cast / overflow | yes | C differs; not E | INVALID_CAST | §25.1.1 | SX/EX |
| same-origin overflow / zero division | yes | C differs; not E | NUMERIC_OVERFLOW | §25.1.1 | SX/EX |
| equivalent failures | yes, including repeats | all frozen fields E | minimum class, any representative | §25.1.1 | EX |
| multiple Project outputs | yes where demanded | each responsible P/C | full semantic key | §25.1.1 | DX/PX/EX |
| multiple Filter rows | yes where demanded | predicate failures P/C | full key, never row | §25.1.1 | DX/EX |
| repeated dictionary occurrence | repeated demanded candidates | may all be E | same minimum class | §25.1.1 | LV/DX/EX |
| undemanded branch or IN entry | no | physical error is irrelevant | none | §§17/20 demand | DX |
| zero-row per-row expression | no | no scalar occurrence | none | §20.17 | DX |
| OOM / SpillIOError / resource ExecutionError | no | preserve resource cause | none | Ch24 / §39 | V24-L |
| cancellation | no | QueryCancelled | none | §39 | V24-L/OX |
| corruption / internal defect | no | owning fatal/invariant class | none | §39 / IX | IX |
| DML including RETURNING | no D25 candidate | DML origin/cause/phase | none | §21.16.1 | V25-K |
| aggregate finalization | no | aggregate ordinal/cause | none | §29.3.7 | aggregate oracle |
| scalar-cardinality / specialized subquery failure | no | occurrence and specialized phase | none | §20.14.12 | V20-12/13 |
## V25-J — Source/cause preorder, equivalence, and reduction

Build EX's key independently as `(start, end-start, semantic source-occurrence order,
conceptual cause rank)` from PX and the literal order below. Byte offsets are not character
positions; use multibyte source prefixes, represented nested spans, and legal implicit-cast
provenance ties. For the comparator's identical-span distinct-occurrence case use typed
semantic-provenance fixtures, not invented SQL text or physical node IDs. Separately prove
that legal movement/sharing/duplication preserves PX through V20-18 and V22-A/B.

| Cause | Chapter-17 origin | Public category under §§17.10.1/39.3 | D25 conceptual rank | Same-expression variability | Independent oracle / status |
|---|---|---|---|---|---|
| INVALID_CAST | supported cast with no target value | CastError | first | VARCHAR/FLOAT to integer can also overflow | ASCII/range SX; literal EX |
| NUMERIC_OVERFLOW | arithmetic, narrowing, numeric parse, temporal conversion range | ArithmeticError | second | division also zero; cast also invalid | mathematical range SX; EX |
| DIVISION_BY_ZERO | integer division/remainder zero divisor | ArithmeticError | third | same division can overflow | mathematical divisor SX; EX |
| INVALID_DATE | invalid DATE text/calendar | CastError | fourth | DATE parsing variants share this cause | Gregorian SX; EX |
| INVALID_TIMESTAMP | invalid TIMESTAMP text/calendar/time | CastError | fifth | TIMESTAMP parsing variants share this cause | Gregorian/microsecond SX; EX |
Compare every conceptual label pair in the key model, independently of production enum,
declaration, registry, or container order. Cross-cause key fixtures are comparator-domain
checks, not a claim that one bound cast can return both DATE and TIMESTAMP. Runtime cases
must be type-realizable. In particular, put `(1,0)` and `(INT32_MIN,-1)` through the same
bound integer division: NUMERIC_OVERFLOW wins. Put `"x"` and `"2147483648"` through the
same VARCHAR-to-INT32 cast: INVALID_CAST wins. Reverse input order and split the rows
across chunks/workers; the responsible origin/span is unchanged in each experiment.

Equivalence compares semantic origin, represented span, public category, conceptual cause,
and every other already-frozen diagnostic field in PX (including applicable explicit/
implicit cast provenance). Vary message prose and offending-value rendering while keeping
those fields equal; accept either representative. Change cause or category and require
non-equivalence. Do not add row numbers, new diagnostic IDs, SQLSTATE codes, or message
ordering. Compare the minimum observable class, not candidate object identity.

Create two demanded Project output expressions using row-dependent division and cast
failures; reverse physical output evaluation without changing source provenance. Construct
Filter predicates with the same division/cast cases nested under BOOLEAN comparison, across
unordered rows. Mix equivalent repeats and distinct causes. In all cases source start,
length, occurrence, then cause decide; even an ordered input grants no row precedence.

For deterministic reduction partition a finite candidate set into local groups, compute
each independent minimum, and enumerate merge permutations and parenthesizations. Require
the same EX minimum, including equivalent representatives. Use explicit candidate-release
events for SIMD lanes and worker completion, not actual SIMD or scheduler timing.

For early stop, give the independent demand/provenance model a finite pending domain.
Case A supplies a candidate with no possible smaller class in that domain: stopping may
conform. Case B first supplies a larger-span candidate while a demanded smaller-span
failure remains, or DIVISION_BY_ZERO while same-origin overflow remains possible: reporting
the first candidate is rejected. Check the eventual result or a valid lower-bound proof;
do not require materializing every candidate, physical rank order, one worker, or a scalar
reference interpreter in production. No proof demands an upstream-skipped occurrence.

## V25-K — DML candidate transport and attempt ownership

Extend V21-13's independent final-attempt candidate oracle with a Chapter-25 transport
adapter. Compare semantic records before/after physicalization, sharing, duplication, and
arrival permutations: responsible span, semantic origin, public category/owning cause, and
scalar/subquery/final-row expression-phase membership. The adapter may summarize only if
it preserves every candidate needed by V21-13. It cannot use EX's non-DML cause/occurrence
preorder to discard required DML information. Constraint-phase ranking remains V21-13.

| DML path | Provenance and span retained? | Expression phase? | V25-J used? | V21-13 / physical order | Oracle / status |
|---|---|---|---|---|---|
| INSERT expression | yes | yes | no | owner / irrelevant | V21-13 + PX |
| UPDATE expression | yes | yes | no | owner / irrelevant | V21-13 + PX |
| DELETE predicate | yes | yes | no | owner / irrelevant | V21-13 + DX |
| RETURNING expression | yes | yes | no | owner; V21-14 bag unchanged | V21-13/14 |
| candidate-row expression | yes | yes | no | owner after prerequisites | V21-13 |
| multiple row candidates | all needed records | yes for expressions | no | semantic minimum, not mutation order | V21-13 |
| retry attempt | only finalized-attempt eligible records | preserved, not a new phase | no | abandoned state excluded | V21-2/13 |
High-priority separation fixture: author two row-dependent expression sites e0/e1, with
e0's span earlier. In an admitted pre-write retry trace, provisional evaluation observes
e0 in an abandoned attempt; the fresh finalized attempt instead establishes e1. Blind
D25 reduction over transported observations chooses e0; V21's eligibility oracle excludes
it and chooses e1. Also include two eligible expression candidates in that final attempt,
with the better one delivered last, plus V21-13's same-span expression/NOT NULL/UNIQUE
phase fixtures. Repeat before the first published write using V21's publication oracle.
Do not manufacture a different DML cause tie-break: on an all-eligible expression-only set
with distinct spans, D21 and D25 can legitimately agree. The discriminators are candidate
eligibility, preservation, phase, and owner routing, not a fabricated opposite winner.

For RETURNING verify the DML failure envelope and bag, not SELECT streaming-prefix rules.
Permute row/lane/preparation/arrival order and discard deduplicated-away, nonqualifying,
stale, and abandoned-attempt candidates as V21 requires. Reuse V21-2 and V24-M to end an
attempt and start another with fresh masks, candidates, vectors, borrowed backing, and
output state under the admitted retry's snapshot/CommandId rules. This changes no retry
admission or §39.1 transaction consequence.

## V25-L — Value-stable borrowing and VARCHAR ownership

Instantiate V23-G/H's generation-tagged LG graph at each expression result publication.
Snapshot every reachable component and compare subsequent reads to LV/VV/BS, not just
pointer liveness. Attempt one mutation/reset before consumer completion, then the same
operation after completion or exact preservation. In-place output writes must preserve
inputs still needed by another child or consumer. Accept prevention, delayed mutation,
stable retention, transfer, copy, materialization, COW, or another owner-valid mechanism.

| Case | Borrow legal? / stable state | Owner / interval | Materialization required? | Exact result oracle / status |
|---|---|---|---|---|
| FLAT pass-through | yes; payload, validity, active domain/type | input owner through consumer | only if stability otherwise lost | LG/LV |
| CONSTANT pass-through | yes; scalar payload/validity plus N-domain | constant owner through consumer | same | LG/RX |
| DICTIONARY pass-through | yes; selection, child relation, all reachable state | complete backing graph through consumer | same | LG/LV |
| StringRef borrow | yes; length, prefix, data range, exact bytes | live byte owner through consumer | same | LG/BS |
| computed VARCHAR | only if exact reference result; otherwise owned bytes | output StringHeap or other exact result owner | no mandatory owner type | LG/BS |
| synchronous consumer | yes; all reachable state stable | producer until §26.6 consumption ends | no; zero-copy fixture | LG event trace |
| retaining consumer | only with stable ownership beyond producer interval | receiving owner | transfer/preservation also accepted | LG split/transfer |
| reset/reuse | not while unpreserved live dependency exists | ends borrow or establishes independence | no mandatory deep copy | LG generation trace |
| in-place mutation | only if every required live view preserved | aliases/children/consumers | representation change may suffice | LG two-view oracle |
Computed-byte fixtures poison stack/scratch storage at producer return; bytes must remain
exact in a live result owner. Include empty, embedded-NUL, equal-prefix and high-bit strings.
The synchronous fixture requires no copy; a retaining fixture must survive source reuse
through valid ownership, not a naked copied pointer. Inspect declared cursor lifetimes
only as V25-O's immediate result handoff; do not extend the lifetime of an old cursor chunk.

## V25-M — Exact large values, accounting, and resource failures

Compose V23-I's LM capability model with V24-G/I's retained-form and arbitrary-precision
extent models. Use symbolic lengths 0, 1, UINT32_MAX-1, UINT32_MAX, UINT32_MAX+1, and a
larger exact finite length, with an abstract byte generator/witness; allocate no multi-GB
fixture. Check exact value/length, never truncation, wrap, modulo, clipping, NUL termination,
or semantic splitting. An available capable alternate must not fail merely because an
incapable compact form was chosen. Independently check retaining-consumer applicability:
an exact scalar form does not make a narrow retained descriptor capable.

For actual scalar-to-VARCHAR paths, independently compute output length, offset+length,
allocation extent, and capacity rounding before every consuming operation. Exercise sums
whose operands fit but result does not with a symbolic allocation adapter; no unsupported
concatenation function is implied. Require safe classification before narrowing, allocation,
accounting, range construction, or pointer use. Reuse V24-I's exact mathematical oracle.

| Resource case | Accounted owner | Exact-before-use? | Canonical outcome on failure | D25 candidate? | Oracle / status |
|---|---|---|---|---|---|
| temporary/output vector | query expression/operator region | yes | supported allocation denial: OOM | no | V24-B/D/I |
| demand-mask/selection storage | query region, including small aggregate growth | yes | OOM or representability ExecutionError by cause | no | V24-B/I/L |
| computed VARCHAR bytes | result region with full lifetime | yes | cause-specific resource error | no | LG/V24-B/I/L |
| large exact VARCHAR | compact or supported alternate owner | yes, plus retained-domain check | exact success subject to resources | no resource candidate | LM/V24-G |
| unsupported exact runtime form | no falsely published owner | before use | representability/resource ExecutionError | no | LM/V24-L |
| supported exact allocation denied | grant/owner cleanup ledger | yes | OutOfMemory | no | V24-D/L |
| size/address unrepresentability | affected exact region | before any consuming operation | representability/resource ExecutionError | no | V24-I/L |
| cancellation | ending execution owners | outstanding resources accounted | QueryCancelled | no | V24-M/OX |
Inventory temporary vectors, output capacities, masks, computed bytes, and variable scratch
in V24-B/D's committed-capacity ledger. Grow many individually small allocations; require
continuous coverage without untracked scratch exemptions or mandatory duplicate charges
for capacity already owned by a parent. Inject deterministic allocation denial, capability
failure, and cancellation at named ownership events, not real OS OOM timing. Check cleanup
through V24-M and current-result nonpublication through OX. Do not rank resource versus
ordinary errors globally: a resource event may prevent semantic establishment. Compare
ordinary minima only within their applicable domain, and successful values when both
paths succeed. Operational failure never permits approximate or silently shortened values.

## V25-N — Invalid expression/runtime states

Construct one-defect malformed fixtures and positive controls. IX uses the bound schema,
closed registry, child arity, LV/VV domain, and LG lifetime graph, not the production
validator's answer. Reuse V22-K final-plan validation; add runtime expression-boundary
checks. Observe construction prevention or rejection before unsafe access, without
requiring full-tree validation each batch or a particular assertion/error API.

All invalid rows below prohibit successful output, user TypeError/CastError/ArithmeticError,
and EX candidate creation; §39.1 owns internal invariant consequences. Instrument forbidden
payload/validity/child-map reads, pointer/range calculations, stale publication, mutation,
and persistent writers. Rejection means zero such unsafe effects, not assertion-and-continue.

| State | Valid? | Static/plan owner | Runtime owner | Public scalar error? / internal? | Safe rejection point | Publishable? | Oracle / status |
|---|---|---|---|---|---|---|---|
| wrong child count | no | Ch19/22, §38.24 | §25.7.1 | no / yes | before child access | no | IX/V22-K |
| wrong input TypeId | no | resolved schema/plan | §25.7.1 | no / yes | before typed payload use | no | IX/SM |
| wrong output TypeId | no | bound result/schema | §25.7.1 | no / yes | before typed write/publication | no | IX/SX |
| missing validated kernel | no | resolved plan/registry | §25.7.1 | no / yes | before dispatch | no | IX |
| invalid slot mapping | no | §22.3 / §38.24 | §25.2 | no / yes | before column access; no fallback | no | IX/SM |
| invalid selection/demand | no | construction where applicable | §§23.6/25.1 | no / yes | before payload/validity/child map | no | IX/LV |
| expired or unstable borrow | no | ownership construction | §§23.10/25.7 | no / yes | before backing dereference | no | IX/LG |
| invalid validity/representation | no | construction where applicable | §§23.14/25.7.1 | no / yes | before interpreting storage | no | IX/VV/LV |
| uninitialized demanded output | no | output contract | §25.7.1 | no / yes | before successful publication | no | IX/OX |
| purported executable named function | no entry | §17.9.3 / binding | §25.7.1 | no runtime scalar error / yes | before nonexistent dispatch | no | IX/registry |
| malformed state of a valid named function | no applicable object | empty registry | not instantiated | not an executable test case | not applicable | no | N/A: empty registry |

Distinguish a valid kernel suffering allocation, representability, or cancellation failure:
V24-L supplies the controlled resource/cancellation category, not IX-invalid classification.

## V25-O — Successful/failed result publication

OX records invocation start, per-demanded-occurrence validity/payload/owner initialization,
success publication, failure, and independently completed cursor returns. Fault at each
initialization boundary. Seed output storage with a different prior generation; after a
failure no stale or partially initialized view may be consumed as successful active output.
On success require validity for all D, exact payload for non-NULL D, and live variable-byte
owners; inactive/undemanded storage remains inaccessible. Candidate selection and result
validity are independently checked: a correct EX minimum never legitimizes a partial vector.

| Event | Invocation initialized? | Invocation publishable? | Prior returned chunk retracted? | Query complete? | Error owner / oracle / status |
|---|---|---|---|---|---|
| successful evaluation | all demanded requirements | yes | no | not implied by one vector/chunk | SX/RX/OX |
| semantic error mid-vector | possibly partial | no | no | failed, not successful completion | EX or specialized owner / OX |
| OOM mid-vector | possibly partial | no | no | failed | §24.10/39 / OX |
| representability failure | possibly partial | no | no | failed | §24.10/39 / OX |
| cancellation mid-vector | possibly partial | no | no | failed | §39 / OX |
| internal invalid runtime state | invalid | no | no retraction protocol invented | no normal continuation | §39.1 / IX/OX |
| earlier completed cursor return A | A valid for its declared interval | A was already returned; says nothing about new invocation | no | prefix does not prove completion | §31.10 / OX |
Drive `Next()` to return chunk A successfully, record client consumption or copy within
§31.10's lifetime, then make another operation fail in expression evaluation. The recorded
delivery is not retracted and is not successful whole-query completion; the failing
invocation publishes no successful result. Do not dereference A beyond the next Next()
unless the client copied/materialized it. No whole-query buffering or cursor redesign is
required. DML RETURNING uses V21-14's separate withheld result envelope.

## V25-P — Determinism, reuse, and persistence-negative registry

Replay fixed semantic fixtures with explicit permutations/barriers and symbolic allocation
events; use no sleeps, allocator luck, real OOM, hash iteration, or OS worker timing.
In the matrix, `=` means invariant scalar value, NULL, demanded occurrence relation,
multiplicity, and semantic output-slot identity after mapping back to the same logical
domain. D25 and D21 invariance applies in their respective eligible candidate domains;
resource feasibility can differ legitimately and never permits changed successful values.

| Perturbation | Value / NULL | Demand | Multiplicity | D25 error | D21 winner | Resource feasibility | Output identity |
|---|---|---|---|---|---|---|---|
| capacity 1,2,7,1024,2048 | = | = | = | = | = | may differ | = |
| chunk partitions, including empty batches | = | = | = | = | = | may differ | = |
| equivalent FLAT/CONSTANT/DICTIONARY | = | = | = | = | = | may differ | = |
| dictionary child remapping | = | = | = | = | = | may differ | = |
| equivalent mask encodings | = | = | = | = | = | may differ | = |
| repeated selection encoding | = | = | repeats preserved | = | = | may differ | = |
| SIMD candidate-release order | = | = | = | = | = | controlled event dependent | = |
| worker completion/merge order | = | = | = | = | = | controlled event dependent | = |
| physical Project visitation | = | = | = | = | = | may differ | = |
| physical row/scan/task order | per occurrence | same semantic bag | = | =; no row rank | = | may differ | = |
| input/result/candidate pointers | = | = | = | = | = | no semantic role | = |
| allocation addresses | = | = | = | = | = | no semantic role | = |
| temporary layout / budget | = if successful | = | = | applicable domain | applicable domain | may differ | = |

The negative winner-key registry additionally varies page/RID/physical slot labels, thread
labels, hash iteration, filesystem ordering labels, function-table addresses, and selection
storage. None supplies a diagnostic rank. Permute physical dictionary backing while
preserving the decoded logical sequence; do not demand equality for genuinely changed
inputs or impose SQL order on an unordered bag. Ordinary minima use EX, DML uses V21-13,
and resource reproducibility uses V24-L under explicitly identical fault conditions.

Reuse the same conceptual execution facility across error then success, large then small
domain, dictionary then FLAT, and different query contexts. Compare DX/EX/VV/LV/OX against
fresh independent invocations: no stale mask, candidate minimum, temporary selection,
validity, output, or subquery state may leak. Preserve LG before chunk reset and discard
attempt-local state at V21/V24 retry boundaries. Exercise subquery states with V20-12–14:
empty/skipped demand never initializes; first nonempty demand initializes once; all demanded
outer rows reuse the cached CONSTANT/value-set result; distinct occurrences are not merged;
failed side plans are not rerun, and a retry receives fresh state. No extra snapshot,
CommandId, or transaction is created by the expression handoff.

Extend V23-J's PF format-field provenance registry to vector/result addresses, demand masks,
selection storage, physical schema ordinals, temporary vectors, candidate records, borrow
handles, kernel pointers, and scratch. They must not become page/WAL/catalog/recovered
query identity. Owner-defined scalar encoding remains legal; absence of matching byte
patterns alone is not proof. Reuse §39.1's statement-outcome oracle for terminal expression
and resource failures before/after persistent publication; execution cleanup cannot choose
new transaction outcomes, independently release transaction locks, or introduce persistent
effects. This is an owner-handoff check, not a new persistence or transaction protocol.

## V25-Q — Cross-chapter reuse and oracle boundaries

Every reference below reuses the named procedure's independent oracle, not production
behavior or an unqualified claim that another chapter covers the case. V25 adds the
expression invocation/transport/publication observations specified above. The §26.6
reference is limited to the immediate synchronous consumer lifetime.

| Handoff | Contract / canonical Architecture owner | V25 family | Reusable Verification procedure | Independent oracle |
|---|---|---|---|---|
| Ch17→25 | scalar meanings/types/causes/folding, §§17.4–17.10 | A/C/G/H/J | Independent scalar oracles; Cast, coercion, and comparison verification; FLOAT64 semantic, arithmetic, text, and cross-engine verification | SX / literal registry |
| Ch18→25 | half-open byte spans, §18.8 | I/J/K | V20-18 Runtime diagnostic provenance; Front-End Error and Source-Span Tests | PX/source-byte intervals |
| Ch19→25 | resolved expressions, casts, output occurrences, §§19.5–19.7 | A/D/H/J/K | V22-A/B; V20-18 | SM/PX/immutable digest |
| Ch20→25 | demand, child order, provenance, subquery modes, §§20.6–20.7/20.14/20.17 | B/C/I/J/P | V20-5, V20-12–18 | DX/PX/side-plan state model |
| Ch21→25 | DML eligibility/rank/attempts/RETURNING, §§21.15–21.16.1 | K/O/P | V21-2/3/13/14 | final-attempt candidate and publication models |
| Ch22→25 | physical schema/validation/runtime ownership, §§22.2–22.3/22.6/22.8 | A/D/N/P | V22-A/B/K | SM/plan digest/IX |
| Ch23→25 | active domain, normalized vectors, borrowing, exact bytes, §§23.1–23.14 | B/D/E/F/G/L/M/N | V23-A/C/D/E/F/G/H/I/M | LV/VV/BS/LG/LM |
| Ch24→25 | accounting/extents/exact forms/resources, §§24.1–24.5/24.10 | M/O/P | V24-B/D/G/I/L/M/N | exact extent/capability/accounting/error ledgers |
| Ch25→26 | synchronous stable result consumption, §26.6 | L/O | V23-G/K owner/consumer procedures | LG and explicit completion events |
| Ch25→27 | Filter removes rows; Project places columns, §§27.7–27.8 | D/E/L | V20-5; V22-B/G; V23-K | SX/RX/SM/LG |
| Ch25→29 | typed argument handoff; specialized finalization, §29.3.7 | A/I | V29-A/G/H/I/L/M; Aggregate Tests smoke index | AR/IO/FT/FS/MM/ER |
| Ch25→31 | returned-chunk boundary, §31.10 | O | V23-G/K; V25-O publication sequence; V21-14 for DML distinction | OX/LG |
| Ch25→39 | category/consequence/internal boundary, §§39.1/39.3 | I/K/M/N/O/P | V24-L/M; V21-3; statement-error matrix procedures | literal error/publication classifier |

## V25-R — Atomic architecture-obligation ledger

A row is one independently falsifiable obligation. Repeated §25.8 summaries map to the
same row rather than inflating the inventory. Each mapping requires the named procedure's
deterministic fixture and independent oracle. Repair references that no longer supply
the required methodology. Rationale and navigation are not additional correctness
obligations. The two explicit N/A rows delimit unavailable named-function methodology.

| ID | Architecture section | Atomic obligation | Verification section | Independent oracle | Reusable family |
|---|---|---|---|---|---|
| V25-A01 | 25.1; 25.3; 25.8 | Execution consumes resolved scalar types without runtime rebinding. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A02 | 25.1; 25.3; 25.8 | Expression/type dispatch is vector-batch scoped rather than repeated SQL-type selection per cell. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A03 | 25.1; 25.3; 25.8 | Representation normalization occurs outside the per-row kind branch. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A04 | 25.1; 25.3; 25.8 | Hot evaluation does not construct a generic Value per active cell. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A05 | 25.1; 25.3; 25.8 | All-valid arithmetic has a useful specialized validity-free path with scalar-equivalent results. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A06 | 25.1; 25.3; 25.8 | Expression evaluation operates over vector logical occurrences rather than changing scalar meaning. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A07 | 25.1; 25.3; 25.8 | Mutable expression state is execution-owned, not immutable plan meaning. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A08 | 25.1; 25.3; 25.8 | Reuse cannot mutate the bound/logical expression or its diagnostic origin. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A09 | 25.1; 25.3; 25.8 | Only closed expression forms and overloads are executable. | V25-A | SX/DX + dispatch trace | Independent scalar oracles; V22-A; V23-C |
| V25-A10 | 25.1; 25.3; 25.8 | Aggregate arguments supply successfully evaluated typed values, not scalar aggregate reductions. | V25-A | SX/DX and exact aggregate state/ordinal oracle | Exact aggregate state and finalization |
| V25-B01 | 25.1; 25.8(15) | Caller demand is a semantic subset of the active input logical domain. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B02 | 25.1; 25.8(15) | Each child demand is derived from Chapter-17/20 semantics, not physical work. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B03 | 25.1; 25.8(15) | Nested child demand cannot restore ancestor-excluded occurrences. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B04 | 25.1; 25.8(15) | Vector-layout convenience cannot omit semantically required occurrences. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B05 | 25.1; 25.8(15) | Equivalent physical masks preserve the semantic demand relation. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B06 | 25.1; 25.8(15) | Demand cannot expose inactive capacity, stale slots, or invalid dictionary positions. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B07 | 25.1; 25.8(15) | Empty demand creates no per-row scalar evaluation. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B08 | 25.1; 25.8(15) | Undemanded speculative values cannot become observable results. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B09 | 25.1; 25.8(15) | Undemanded speculative ordinary errors cannot become candidates. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-B10 | 25.1; 25.8(15) | NULL strictness does not suppress a required child's evaluation or error. | V25-B | DX/LV | V20-15/17; V23-D |
| V25-C01 | 25.1; 25.5–25.6 | Executable children retain Chapter-17 order, including failure reachability. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C02 | 25.1; 25.5–25.6 | AND evaluates its left child first. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C03 | 25.1; 25.5–25.6 | AND excludes FALSE-left rows from RHS semantic evaluation. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C04 | 25.1; 25.5–25.6 | AND demands RHS for TRUE/NULL-left rows and applies exact 3VL. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C05 | 25.1; 25.5–25.6 | OR evaluates its left child first. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C06 | 25.1; 25.5–25.6 | OR excludes TRUE-left rows from RHS semantic evaluation. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C07 | 25.1; 25.5–25.6 | OR demands RHS for FALSE/NULL-left rows and applies exact 3VL. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C08 | 25.1; 25.5–25.6 | CASE conditions retain source order and stop after the first TRUE. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C09 | 25.1; 25.5–25.6 | CASE evaluates only the selected result/ELSE and produces typed NULL for absent ELSE. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C10 | 25.1; 25.5–25.6 | IN evaluates its left operand once and entries left-to-right. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C11 | 25.1; 25.5–25.6 | IN stops at first TRUE, not UNKNOWN, and NOT IN uses exact 3VL negation. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-C12 | 25.1; 25.5–25.6 | Nested CASE/AND/OR/IN masks preserve every ancestor demand restriction. | V25-C | DX/SX | V20-15/17; scalar 3VL oracle |
| V25-D01 | 25.2 | Input identity is the upstream LogicalSlotId. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D02 | 25.2 | Resolved physical schema entry S[j] addresses DataChunk.columns[j] with matching type. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D03 | 25.2 | Physical ordinal is a schema-local locator, not semantic identity. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D04 | 25.2 | Project reordering uses the corresponding resolved mapping rather than stale ordinals. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D05 | 25.2 | Derived boundaries consume their final declared remap. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D06 | 25.2 | Equal/repeated/self-join values do not collapse distinct semantic slots. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D07 | 25.2 | Names, BindingId, pointer, or vector position cannot rediscover runtime input identity. | V25-D | SM/PX | V22-B; V20-14 |
| V25-D08 | 25.2 | Temporary/result vectors create no new LogicalSlotId and need no duplicated slot metadata. | V25-D | SM/PX | V22-B; V20-14 |
| V25-E01 | 25.2; 25.4; 25.8(6,17) | Successful ordinary evaluation yields exactly one result for each demanded occurrence. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E02 | 25.2; 25.4; 25.8(6,17) | Result correspondence preserves the evaluation-domain logical order. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E03 | 25.2; 25.4; 25.8(6,17) | Scalar evaluation does not deduplicate equal occurrences. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E04 | 25.2; 25.4; 25.8(6,17) | Scalar evaluation does not perform Filter row removal. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E05 | 25.2; 25.4; 25.8(6,17) | CONSTANT payload count does not collapse logical result multiplicity. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E06 | 25.2; 25.4; 25.8(6,17) | Repeated DICTIONARY selection produces repeated result occurrences. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E07 | 25.2; 25.4; 25.8(6,17) | Partial/empty demand yields exactly its result domain without stale active output. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E08 | 25.2; 25.4; 25.8(6,17) | Project's output schema owns placement and identity, not expression buffers. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-E09 | 25.2; 25.4; 25.8(6,17) | Direct comparison-to-Filter selection is allowed and equals independently selected TRUE positions. | V25-E | RX/SX/SM | V20-5; V22-B; V23-C |
| V25-F01 | 25.2; 25.8(3) | Normalized selection resolves each effective input index correctly. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-F02 | 25.2; 25.8(3) | Normalized validity resolves each logical input's NULL state correctly. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-F03 | 25.2; 25.8(3) | FLAT/CONSTANT/DICTIONARY substitution preserves scalar values and NULLs. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-F04 | 25.2; 25.8(3) | Representation substitution preserves child demand and ordinary candidate classes. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-F05 | 25.2; 25.8(3) | Valid nested dictionary normalization preserves occurrence mapping and multiplicity. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-F06 | 25.2; 25.8(3) | Any legal result representation preserving the logical correspondence is conforming. | V25-F | LV/DX/RX/EX | V23-C/D |
| V25-G01 | 25.2–25.4; 25.7.1 | NULL payloads are not interpreted when the scalar non-NULL operation is suppressed. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-G02 | 25.2–25.4; 25.7.1 | Inactive validity/payload storage cannot influence scalar output. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-G03 | 25.2–25.4; 25.7.1 | CONSTANT and DICTIONARY validity follow logical occurrence resolution. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-G04 | 25.2–25.4; 25.7.1 | Ordinary comparison with NULL produces SQL UNKNOWN. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-G05 | 25.2–25.4; 25.7.1 | IS NULL/IS NOT NULL evaluates its child and returns non-null BOOLEAN on success. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-G06 | 25.2–25.4; 25.7.1 | Fast and nullable paths preserve the same Chapter-17 validity semantics. | V25-G | VV/SX + poison reads | V23-E; scalar NULL oracle |
| V25-H01 | 25.1; 25.3–25.4 | Arithmetic kernels execute the registered scalar operator result types and values. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H02 | 25.1; 25.3–25.4 | Checked integer add/subtract/multiply errors do not wrap. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H03 | 25.1; 25.3–25.4 | Unary integer negation handles minimum overflow under scalar semantics. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H04 | 25.1; 25.3–25.4 | Integer division/remainder have exact signed quotient/remainder and zero-divisor causes. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H05 | 25.1; 25.3–25.4 | MIN/-1 and MIN%-1 report numeric overflow. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H06 | 25.1; 25.3–25.4 | FLOAT arithmetic rounds at each bound node without observable reassociation or contraction. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H07 | 25.1; 25.3–25.4 | FLOAT NaN/infinity/signed-zero behavior remains canonical, including non-error zero division. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H08 | 25.1; 25.3–25.4 | FLOAT comparison follows Chapter-17 total semantic classes. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H09 | 25.1; 25.3–25.4 | VARCHAR comparison uses exact unsigned bytes, not prefix-only or C-string semantics. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H10 | 25.1; 25.3–25.4 | Resolved casts preserve the registered value-specific causes and public mappings. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H11 | 25.1; 25.3–25.4 | DATE/TIMESTAMP parse, range, formatting, and conversion obey upstream scalar semantics. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-H12 | 25.1; 25.3–25.4 | Folded and reached runtime evaluation agree without changing binding-time error timing. | V25-H | SX/BS | Type-System Property Tests; Expression Execution Tests; V23-F |
| V25-I01 | 25.1.1; 25.8(12,14) | Only semantically demanded ordinary non-DML runtime failures establish candidates. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I02 | 25.1.1; 25.8(12,14) | Canonical child order determines the occurrence-level failure before candidate comparison. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I03 | 25.1.1; 25.8(12,14) | Zero-row per-row expressions establish no runtime candidate. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I04 | 25.1.1; 25.8(12,14) | Filter/Project candidate membership follows relational demand, not hypothetical unreached values. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I05 | 25.1.1; 25.8(12,14) | DML/RETURNING candidates are excluded from ordinary non-DML reduction. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I06 | 25.1.1; 25.8(12,14) | Aggregate-finalization failures retain their specialized ranking owner. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I07 | 25.1.1; 25.8(12,14) | Scalar-cardinality and specialized subquery failures retain their precedence owner. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I08 | 25.1.1; 25.8(12,14) | OOM, SpillIOError, resource ExecutionError, and cancellation are not ordinary candidates. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I09 | 25.1.1; 25.8(12,14) | Corruption and internal-invalid-state failures are not ordinary candidates. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-I10 | 25.1.1; 25.8(12,14) | No universal ordinary-semantic versus resource/cancellation precedence is inferred. | V25-I | DX/PX/EX | V20-12/13/15/17/18; V24-L |
| V25-J01 | 25.1.1; 25.8(13) | Candidates retain canonical responsible source-derived origin. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J02 | 25.1.1; 25.8(13) | Candidates retain responsible represented SourceSpan rather than a physical diagnostic site. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J03 | 25.1.1; 25.8(13) | Candidates retain public category, conceptual cause, and other frozen diagnostic metadata. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J04 | 25.1.1; 25.8(13) | Logical demanded row occurrences establish membership/multiplicity but not ranking identity. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J05 | 25.1.1; 25.8(13) | Smallest responsible span start is the primary rank. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J06 | 25.1.1; 25.8(13) | Equal starts use shorter represented span. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J07 | 25.1.1; 25.8(13) | Identical spans use preserved semantic source-expression occurrence order. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J08 | 25.1.1; 25.8(13) | Same occurrence uses INVALID_CAST, NUMERIC_OVERFLOW, DIVISION_BY_ZERO, INVALID_DATE, INVALID_TIMESTAMP order. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J09 | 25.1.1; 25.8(13) | Cause rank does not depend on enum values, declaration order, registry iteration, or table addresses. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J10 | 25.1.1; 25.8(13) | Same-origin division overflow beats zero division across unordered rows. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J11 | 25.1.1; 25.8(13) | Same-origin invalid numeric cast beats numeric overflow across unordered rows. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J12 | 25.1.1; 25.8(13) | Equivalent candidates agree on every frozen public diagnostic field. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J13 | 25.1.1; 25.8(13) | Any representative of the minimum equivalent class is conforming. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J14 | 25.1.1; 25.8(13) | Exact message prose and generic offending-value rendering do not break ties. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J15 | 25.1.1; 25.8(13) | Project output failures use semantic provenance, not physical visitation. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J16 | 25.1.1; 25.8(13) | Multiple Filter-row failures use provenance/cause, not row order. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J17 | 25.1.1; 25.8(13) | Repeated dictionary candidates retain membership but may share a minimum equivalence class. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J18 | 25.1.1; 25.8(13) | Ordered or unordered input does not establish D25 row precedence. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J19 | 25.1.1; 25.8(13) | Local-minimum/global-minimum reduction is independent of discovery and merge order. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J20 | 25.1.1; 25.8(13) | Execution may stop only when no smaller demanded candidate class can still be established. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-J21 | 25.1.1; 25.8(13) | Full candidate materialization, scalar row-at-a-time, and physical rank-order execution are not required. | V25-J | PX/EX/SX | V20-18; independent scalar cause registry |
| V25-K01 | 25.1.2; 25.8(16) | DML transport retains responsible span and semantic origin. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K02 | 25.1.2; 25.8(16) | DML transport retains public category/owning cause. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K03 | 25.1.2; 25.8(16) | DML expression failures retain scalar/subquery/final-row expression-phase membership. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K04 | 25.1.2; 25.8(16) | Chapter 21 owns eligibility including attempt and target-revalidation prerequisites. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K05 | 25.1.2; 25.8(16) | Every eligible candidate needed by V21-13 survives transport or equivalent aggregation. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K06 | 25.1.2; 25.8(16) | D25 reduction or physical arrival order cannot discard needed DML candidates. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K07 | 25.1.2; 25.8(16) | NOT NULL/UNIQUE/other constraint phases are ranked by Chapter 21, not expression kernels. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K08 | 25.1.2; 25.8(16) | Physical sharing/duplication preserves upstream provenance, not node/kernel/ordinal/lane identity. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K09 | 25.1.2; 25.8(16) | RETURNING remains DML with its unordered bag and failure envelope. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-K10 | 25.1.2; 25.8(16) | Abandoned/retried attempt candidates and results do not leak into the finalized attempt. | V25-K | PX + final-attempt candidate oracle | V21-2/13/14; V20-18; V24-M |
| V25-L01 | 25.7; 25.8(10,19) | Fixed-width computed results have reusable execution/output-owned storage. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L02 | 25.7; 25.8(10,19) | Computed VARCHAR bytes have an exact live owner covering the required result interval. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L03 | 25.7; 25.8(10,19) | Borrowed results require value stability, not owner liveness alone. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L04 | 25.7; 25.8(10,19) | Reachable payload and validity remain stable through the borrow. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L05 | 25.7; 25.8(10,19) | Reachable selection and dictionary relationships remain stable through the borrow. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L06 | 25.7; 25.8(10,19) | Reachable active-domain/type/representation metadata remain stable through the borrow. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L07 | 25.7; 25.8(10,19) | StringRef metadata, address ranges, and bytes remain stable through the borrow. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L08 | 25.7; 25.8(10,19) | Reset/reuse/incompatible mutation waits for borrow end or exact preservation. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L09 | 25.7; 25.8(10,19) | In-place output aliasing cannot change inputs still required by children/consumers. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L10 | 25.7; 25.8(10,19) | Synchronous consumption permits zero-copy borrowing. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L11 | 25.7; 25.8(10,19) | Retention beyond the producer interval obtains stable ownership or materialization. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-L12 | 25.7; 25.8(10,19) | No one copy/transfer/COW mechanism is required. | V25-L | LG/LV/VV/BS | V23-G/H; V22-A |
| V25-M01 | 25.7.2; 25.8(20) | Compact StringRef and heap-tuple limits are not SQL/expression VARCHAR maxima. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M02 | 25.7.2; 25.8(20) | Available supported alternate exact runtime forms remain applicable beyond compact limits. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M03 | 25.7.2; 25.8(20) | Values are not truncated, wrapped, clipped, or semantically split to fit a runtime form. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M04 | 25.7.2; 25.8(20) | Retaining consumers independently satisfy exact retained-row applicability. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M05 | 25.7.2; 25.8(20) | Expression scratch/temp vectors/demand storage/output bytes have live accounted owners. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M06 | 25.7.2; 25.8(20) | Individually small unbounded aggregate growth cannot bypass accounting. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M07 | 25.7.2; 25.8(20) | Parent-region coverage does not require duplicate per-object charges. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M08 | 25.7.2; 25.8(20) | Variable-result length, offset+length, allocation extent, and capacity growth are exact before use. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M09 | 25.7.2; 25.8(20) | Unsupported exact runtime size/address/value representation yields controlled resource ExecutionError. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M10 | 25.7.2; 25.8(20) | Catchable allocation denial for a supported exact form yields OutOfMemory. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M11 | 25.7.2; 25.8(20) | Cancellation retains QueryCancelled classification and cleanup ownership. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-M12 | 25.7.2; 25.8(20) | Resource feasibility cannot redefine successful scalar values or introduce error precedence. | V25-M | LM/BS + exact extent/accounting oracle | V23-I; V24-B/D/G/I/L/M |
| V25-N01 | 25.7.1; 25.8(18) | Wrong child count after validation is internal invalid state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N02 | 25.7.1; 25.8(18) | Wrong input TypeId after validation is internal invalid state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N03 | 25.7.1; 25.8(18) | Wrong output TypeId after validation is internal invalid state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N04 | 25.7.1; 25.8(18) | Missing/unresolved kernel for a validated expression is internal invalid state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N05 | 25.7.1; 25.8(18) | Invalid input-slot mapping is internal invalid state without fallback lookup. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N06 | 25.7.1; 25.8(18) | Out-of-domain selection/demand is invalid before payload/validity/child-map access. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N07 | 25.7.1; 25.8(18) | Expired or unstable borrow is invalid before dereference. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N08 | 25.7.1; 25.8(18) | Invalid validity/representation state is internally invalid. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N09 | 25.7.1; 25.8(18) | Uninitialized demanded output cannot be treated as valid successful state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N10 | 25.7.1; 25.8(18) | The empty named-function registry authorizes no purported executable named-function state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N11 | 25.7.1; 25.8(18) | Invalid states are not user TypeError/CastError/ArithmeticError or D25 candidates. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N12 | 25.7.1; 25.8(18) | Construction, final-plan validation, or local guards may enforce preconditions without per-batch full validation. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N13 | 25.7.1; 25.8(18) | Malformed state is stopped before unsafe ranges, OOB access, dangling data, stale publication, or arbitrary mutation. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N14 | 25.7.1; 25.8(18) | Malformed expression state cannot cause persistent database effects. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N15 | 25.7.1; 25.8(18) | Internal failure follows §39.1 invariant consequences, not assertion-and-continue. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-N16 | 25.7.1; 25.8(18) | Legitimate resource/cancellation failure of a valid expression is not malformed state. | V25-N | IX/SM/LV/VV/LG + access trace | V22-K; V23-D/G/M; §39 outcome oracle |
| V25-O01 | 25.7.1; 25.8(18); §31.10 handoff | Success initializes every demanded result occurrence's validity. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O02 | 25.7.1; 25.8(18); §31.10 handoff | Success initializes exact payload for every non-NULL demanded occurrence. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O03 | 25.7.1; 25.8(18); §31.10 handoff | Success establishes required variable-length ownership. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O04 | 25.7.1; 25.8(18); §31.10 handoff | Undemanded/inactive output capacity remains semantically inaccessible. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O05 | 25.7.1; 25.8(18); §31.10 handoff | Failed evaluation does not publish its result vector/view as successful. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O06 | 25.7.1; 25.8(18); §31.10 handoff | Incomplete demanded positions or stale prior output cannot escape as consumable active success. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O07 | 25.7.1; 25.8(18); §31.10 handoff | Correct error reduction does not authorize partial-result publication. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O08 | 25.7.1; 25.8(18); §31.10 handoff | Already-returned cursor chunks are not retroactively retracted by a subsequent ordinary expression error. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O09 | 25.7.1; 25.8(18); §31.10 handoff | A returned prefix is not successful complete-query execution. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-O10 | 25.7.1; 25.8(18); §31.10 handoff | The cursor's declared lifetime is preserved without whole-query buffering or protocol redesign. | V25-O | OX/SX/RX/LG | V23-E/G; V24-L; V21-14 |
| V25-P01 | 25.1; 25.1.1; 25.7; 25.8 | Legal vector capacity/chunk changes preserve successful semantics and applicable ordinary minima. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P02 | 25.1; 25.1.1; 25.7; 25.8 | Representation/selection storage/dictionary mapping cannot rank public ordinary errors. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P03 | 25.1; 25.1.1; 25.7; 25.8 | Worker/thread/SIMD discovery order cannot rank public ordinary errors. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P04 | 25.1; 25.1.1; 25.7; 25.8 | Scan/task/page/RID/physical slot/hash/filesystem order cannot rank ordinary errors. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P05 | 25.1; 25.1.1; 25.7; 25.8 | Pointer/allocation/function-table address cannot rank ordinary errors or identify slots. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P06 | 25.1; 25.1.1; 25.7; 25.8 | Successful values/demand/multiplicity/output identity survive physical layout perturbations. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P07 | 25.1; 25.1.1; 25.7; 25.8 | Legitimate resource feasibility differences qualify semantic comparisons rather than imposing universal precedence. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P08 | 25.1; 25.1.1; 25.7; 25.8 | Reuse clears stale demand, candidate, validity, selection, and result state. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P09 | 25.1; 25.1.1; 25.7; 25.8 | Expression subqueries initialize only on first nonempty semantic demand. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P10 | 25.1; 25.1.1; 25.7; 25.8 | Subquery state is keyed by bound occurrence and initialized at most once per attempt, not per vector row. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P11 | 25.1; 25.1.1; 25.7; 25.8 | Cached scalar/CONSTANT or IN probe results serve exactly the demanded outer rows. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P12 | 25.1; 25.1.1; 25.7; 25.8 | Skipped/empty subquery demand does not initialize a side plan. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P13 | 25.1; 25.1.1; 25.7; 25.8 | Subquery cardinality, EXISTS projection irrelevance, and IN build/probe precedence survive vector execution. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P14 | 25.1; 25.1.1; 25.7; 25.8 | Failed side plans are not rerun to seek another result and retry gets fresh side state. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P15 | 25.1; 25.1.1; 25.7; 25.8 | Expression side plans share their attempt snapshot/CommandId without a new transaction. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P16 | 25.1; 25.1.1; 25.7; 25.8 | Runtime expression pointers/masks/ordinals/candidates/borrows/scratch never become persistent semantic state. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-P17 | 25.1; 25.1.1; 25.7; 25.8 | Error cleanup preserves §39.1 transaction consequences and transaction-owned locks. | V25-P | DX/EX/RX/PX/OX/LG/PF | V20-12/13/14; V21-2/13; V22-A; V23-J; V24-M/N |
| V25-NA1 | 25.3; 17.9.3 | Actual named-function argument/strictness/volatility execution. | V25-A/C | Closed empty scalar-function registry | Type-System Property Tests registry sweep |
| V25-NA2 | 25.7.1; 17.9.3 | Malformed function-local state of an otherwise valid named scalar function. | V25-N | Closed empty scalar-function registry | V22-K |

Audit Chapter-25-relevant procedures against the ledger for stale eager-demand rules,
first-physical-error or arbitrary-candidate acceptance, SourceSpan-only ranking, D25-for-DML
reduction, ordinal/pointer slot identity, payload-count cardinality, dictionary deduplication,
liveness-only borrowing, mandatory deep copy, compact/tuple-derived SQL VARCHAR limits,
user-scalar relabeling of malformed states, and partial successful output after failure.
A negative example is not a stale rule when its required outcome rejects that behavior.
Use the matrices above to separate ordinary error invariance from legitimate resource
feasibility differences.

## Expression Execution Tests

Run every supported physical expression kernel over all applicable FLAT, CONSTANT, and
DICTIONARY input combinations, with identity/non-identity selections and all-valid,
all-NULL, and mixed validity. Compare against a scalar/reference oracle that implements the
closed Chapter-17 registry. Compare successful values/NULLs and ordinary error classes under
V25-I/J/K's applicable candidate domain, independently of representation or chunk boundary;
V25-M/P separately qualifies legitimate resource-feasibility differences.

### Checked integer arithmetic and division

Exercise every integer overload in `ARCHITECTURE.md` §§17.6.1–17.6.2 and 39.3.1. Use
systematic neighborhoods around each type's minimum, maximum, `-1`, `0`, and `1`, plus
generated near-boundary operand pairs. Cover:

```text
addition overflow
subtraction overflow
multiplication overflow
unary-minus overflow
division and remainder by zero
MIN_INT / -1
MIN_INT % -1 where defined by the registry
positive/negative quotient and remainder combinations
```

Assert the architecture-defined exact result or `ArithmeticError`; do not evaluate the
oracle through C++ signed-overflow behavior. Put a failing row at the beginning, middle, and
end of a vector and behind active/inactive selections to prove that only semantically
demanded rows raise an error.

### BOOLEAN, casts, and FLOAT64

Use all three TRUE/FALSE/NULL inputs for `NOT` and the complete three-by-three input matrix
for `AND` and `OR`. Place supported row-dependent error-producing expressions on the right
and prove that §17.7.3's per-row short-circuit selection evaluates only demanded rows across
every vector representation.

For every supported cast in the closed registry, test successful boundary values, NULL,
invalid conversion, and range overflow. Run each through direct expression evaluation and
through selected/dictionary vectors. Assert the declared result type and controlled
category for the Chapter-17 conceptual cause: `NUMERIC_OVERFLOW` maps to `ArithmeticError`,
not a blanket `CastError`. Use V25-J's cause/category oracle; execution must not select a
new coercion absent from the bound expression.

Exercise FLOAT64 scalar arithmetic, comparison, and division under §§17.4.3, 17.6.2, and
39.3.2 with:

```text
finite boundary and subnormal values
+Infinity and -Infinity
multiple NaN payload inputs and canonical NaN results where required
-0.0 and +0.0
finite nonzero / ±0.0
±0.0 / ±0.0
Infinity / Infinity
```

Check exact result bits where the architecture fixes canonical representation or signed
zero, and semantic comparison classes otherwise. Compiler fast-math behavior is not an
alternative oracle.

### Hash/comparison consistency

For ordinary join equality and GROUP BY/DISTINCT equality modes, generate values classified
as equal and assert equal hash values using `ARCHITECTURE.md` §28.2.1. Include NULL handling
for each mode, FLOAT64 signed zero and canonical-equivalent NaNs, exact VARCHAR bytes,
numeric/temporal types, and composite keys. Cross-check comparison, grouping, hash join,
aggregate grouping, and index-key semantics where each mode applies; a collision remains
legal only when full semantic comparison distinguishes unequal values.

---

## String Lifetime Tests

Use V23-G/H's generation-tagged owner/borrower graph and mutation matrix. Create tests that
deliberately:

1. scan VARCHAR data,
2. release/unpin source pages,
3. recycle source chunks,
4. continue downstream blocking processing,
5. verify strings remain valid.

In addition to owner lifetime, mutate reachable payload, validity, selection, dictionary
base, StringRef metadata, and referenced bytes before borrow completion. A conforming
implementation prevents/delays the mutation or establishes independent stable ownership;
the borrower must continue to observe the original logical value. Mutating proven
unreachable inactive storage remains legal. Use deterministic poison generations rather
than relying on allocator reuse accidents.
