# Chapter 29 — Aggregation and DISTINCT Verification

This family verifies `ARCHITECTURE.md` Chapter 29 and its Chapter-29-specific cross-owner
handoffs. Verify each obligation through a deterministic procedure and independent
oracle, directly or through the exact V29-T reuse row.

Use these independent test-side oracles:

| Oracle | Independent construction |
|---|---|
| `AR` | Literal closed aggregate overload/result/nullability/empty-state registry |
| `GO` | Tagged-row grouping-equivalence partition using Chapter-17 scalar semantics |
| `CR` | Componentwise canonical grouping representative over typed values and bits |
| `DO` | Logical-occurrence ledger independent of payload/storage identity |
| `IO` | Arbitrary-range mathematical integer/count and exact-rational integer-AVG model |
| `FT` | Bit-exact binary64 legal-reduction-tree enumerator and trace validator |
| `FS` | Explicit-input NaN/infinity classifier plus final zero/NaN normalizer |
| `MM` | Chapter-17 total-order MIN/MAX candidate selector and canonicalizer |
| `GH` | Controlled candidate hash directory plus independent full grouping-equality recheck |
| `PL` | Sink/Combine/Finalize/readiness/Source state automaton |
| `SP` | Partition, replay, accounting, and well-founded pressure-progress model |
| `RO` | Ordered active-group/finalized-row-spool/readiness/cleanup model |
| `ER` | Aggregate ordinal, provenance, demanded-input, and terminal-error owner table |
| `AF` | Attempt-generation ledger with poisoned failed state and fresh retry state |
| `OP` | Chapter-30 comparator and Chapter-37 required/provided ordering model |
| `SS` | Declared schema, LogicalSlotId, RequiredSlotSet, and payload-demand model |
| `IX` | One-defect invalid-state model with forbidden access/output/effect counters |

Production aggregate algorithms, hashes, equality routines, finalizers, spill codecs,
comparators, and serial results are observations, never their own oracle. Use exact typed
fixtures, occurrence tags, explicit barriers, synthetic semantic states, deterministic
fault injection, and seeded schedules. Use no sleeps, wall-clock races, allocator/address
order, filesystem enumeration, or uncontrolled worker timing.

## V29-A — Closed registry and aggregate-state lifecycle

Compare parser/binder/descriptor/executor admission with literal `AR`. Cover `COUNT(*)`,
`COUNT(expr)`, SUM/AVG over each listed numeric TypeId, and MIN/MAX over every listed
orderable TypeId. Verify result TypeId, nullability, NULL/empty behavior, semantic state,
Merge law, and Finalize rule. Reject BOOLEAN MIN/MAX, unlisted SUM/AVG inputs, implicit
string/Boolean/temporal conversions, aggregate DISTINCT/FILTER, unsupported arity, and
unsupported functions through the existing V18/V19 rejection procedures; runtime dispatch
must not re-admit them.

`AR` is this literal test-owned registry:

| Form | Admitted input | Result | Empty/all-NULL | Semantic oracle |
|---|---|---|---|---|
| `COUNT(*)` | no argument | INT64 NOT NULL | `0` | exact row count |
| `COUNT(expr)` | any concrete scalar | INT64 NOT NULL | `0` | exact non-NULL count |
| `SUM` | INT32 | nullable INT64 | NULL | exact integer |
| `SUM` | INT64 | nullable INT64 | NULL | exact integer |
| `SUM` | FLOAT64 | nullable FLOAT64 | NULL | FT + FS |
| `AVG` | INT32 | nullable FLOAT64 | NULL | exact rational IO |
| `AVG` | INT64 | nullable FLOAT64 | NULL | exact rational IO |
| `AVG` | FLOAT64 | nullable FLOAT64 | NULL | FT subtotal + exact count + FS |
| `MIN` / `MAX` | INT32, INT64, FLOAT64, VARCHAR, DATE, TIMESTAMP | nullable input type | NULL | MM |

The operator-role oracle is likewise explicit:

| Path | Pipeline role | Readiness/output | Memory/order |
|---|---|---|---|
| grouped `PhysicalHashAggregate` | blocking Sink/Combine/Finalize/Source | all groups validated before Source | group hash state; no order |
| global aggregate fast path | blocking single-state aggregate | one successful row after Finalize | no group table; no added order |
| `PhysicalSortAggregate` | ordered blocking aggregate | retained rows become Source after complete readiness | one active mutable group plus accounted ordered spool |
| hash `PhysicalDistinct` | blocking Sink/Finalize/Source | Sink emits nothing | duplicate-class state; no order |
| ordered/streaming DISTINCT | capability-conditional streaming path | class-boundary output under semantic demand | compatible input/property required |

For every admitted descriptor, drive `Initialize -> Update* -> Combine* -> Finalize`, then
conditional destruction. A test adapter reports state extent/alignment, initialization,
owned-backing generation, and release events without making a C++ struct or callback ABI the
oracle. Exercise inline and owned-handle realizations, empty state, partial initialization,
success, Update/Combine/Finalize failure, cancellation, and cleanup. State is query/attempt-
local, fully accounted, never persisted, and never reused by a retry; immutable descriptor
metadata may be reused.

## V29-B — Logical occurrences, global/grouped cardinality, and empty input

Tag every active logical row independently of its value. Apply each aggregate to FLAT,
CONSTANT, DICTIONARY, nested-DICTIONARY, identity/nonidentity selections, all-valid/all-NULL/
mixed validity, empty, one-row, irregular, and full chunks. A CONSTANT value with cardinality
100 contributes 100 occurrences; DICTIONARY `[2,2,2]` contributes three. Inactive capacity
and poisoned payload beneath NULL validity are unreadable. `COUNT(*)` counts zero-width rows
without requiring a payload column.

`GO/AR` require exactly one global row for empty or nonempty no-key aggregation; on no
non-NULL argument COUNT is zero and SUM/AVG/MIN/MAX are typed NULL. Explicit grouping over
empty input yields zero groups. Keep grouped-empty and global-empty as separate direct
fixtures and vary all chunk/representation shapes without changing these cardinalities.

## V29-C — Group hash table, grouping equality, collisions, and key ownership

Build `GO` partitions for typed NULL, BOOLEAN/integer/date/timestamp, exact-byte VARCHAR
(embedded NUL/high bytes/equal prefix-different suffix), FLOAT signed zeros, multiple NaN
encodings, composites, and repeated occurrences. `GH` forces unequal values to one test hash
and equal values through compatible lookup. Same hash is only a candidate; every candidate
receives complete grouping equality, unequal collisions remain separate groups, and equal
keys locate one semantic group state.

Reset/unpin/reuse and poison source chunks after accepted input. Retained VARCHAR group keys
and state-owned VARCHAR values must remain exact. Account directory, keys, cached hashes,
aligned states, varlen backing, and group metadata. Group-table order, insertion order, hash
seed, bucket growth, and pointers establish no SQL order.

## V29-D — Canonical GROUP BY and DISTINCT representatives

`CR` maps typed NULL to the same typed NULL, either FLOAT zero to `+0.0`, every grouping-
equivalent NaN to bits `0x7ff8000000000000`, and every other scalar to its existing semantic
representation. It applies componentwise and is implemented independently of production
grouping/canonicalization code.

For GROUP BY `{−0.0,+0.0}` require one group and exposed `+0.0` bits. For several NaN payload/
sign encodings require one group and canonical NaN bits. Repeat with composites, reverse
input, controlled collisions, varied seeds, workers, Combine order, spill, and hash/ordered
aggregation. Scalar-bit inspection and an independently evaluated downstream cast prove
that first encounter cannot leak. Ordinary nongrouped `-0.0`, persisted bits, scalar casts,
ordinary equality, total order, and MIN/MAX rules remain unchanged.

For DISTINCT, partition complete rows with `GO`, emit one occurrence per class, and compare
every component with `CR`. Cover NULL, zeros, NaNs, exact VARCHAR, composite rows, and
zero-width reachable schemas. Preserve the child schema and LogicalSlotIds. Hash insertion,
workers, spill replay, and ordered first member cannot select a different row representation.

## V29-E — Baseline hash DISTINCT lifecycle and storage

Drive `PL` through hash `PhysicalDistinct` Sink, Finalize, and Source. Place barriers after
the first/new class, after several classes, before final demanded input, after input
exhaustion, during Finalize, and before readiness publication. Sink emits no row and no
dependent sees readiness. Successful Finalize publishes readiness exactly once; Source then
emits exactly one canonical row per `GO` class in no asserted order.

Put a demanded expression error after an early class and require the error: observing a
class is not an early-stop proof. Use Chapters 20/26 for valid nonexecution. Exercise many
unique/duplicate/composite/large-VARCHAR rows and forced spill where supported; preserve
classes and canonical rows without first-spilled/replayed selection.

## V29-F — COUNT and exact integer SUM/AVG

`IO` models unbounded mathematical state. COUNT(*) counts every demanded row; COUNT(expr)
evaluates each demanded argument and counts only non-NULL. Synthetic states at `INT64_MAX`
and `INT64_MAX+1` prove success versus Finalize `NUMERIC_OVERFLOW`. Update/Combine never
raise COUNT-width overflow, and an optional absorbing marker cannot suppress later demanded
arguments or child errors.

For SUM(INT32/INT64), ignore NULL and retain an exact mathematical subtotal. Directly test
`INT64_MAX + 1 - 1`, `INT64_MIN - 1 + 1`, partitions whose local subtotals exceed INT64,
final in-range/out-of-range values, no wrap/saturation, and Finalize-only range checks.
SUM(INT32) returns nullable INT64 and admits values beyond INT32 when INT64-valid.

For integer AVG, divide exact sum by exact non-NULL count and independently round the exact
rational once to binary64 ties-to-even. Include `(1,2) -> 1.5` and
`(INT64_MAX,INT64_MIN) -> -0.5`; do not range-check a SUM result or average partial averages.
Exact COUNT/integer SUM/AVG outcomes are invariant to vectors, workers, Merge trees, spill,
and aggregate algorithm.

## V29-G — MIN/MAX total order, canonical candidates, and VARCHAR lifetime

For every admitted MIN/MAX TypeId, use `MM`, ignore NULL, and return typed NULL for no
non-NULL value. Check Chapter-17 order and direct FLOAT cases:
`MIN(+0,-0)=+0`, `MAX(+0,-0)=+0`, `MIN(NaN,1)=1`, and `MAX(NaN,1)=canonical NaN`.
Permute input, vectors, workers, Combine trees, spill, and algorithms; output remains exact.
For VARCHAR, include embedded NUL/high bytes and poison/recycle all source backing before
Combine/Finalize to prove exact owned candidate bytes and cleanup.

## V29-H — Independent F2 legal-tree oracles

`FT-small` exhaustively enumerates, for deliberately small finite tagged bags, every allowed
leaf permutation where relational order is unconstrained and every full binary
parenthesization. Each edge uses an isolated bit-exact IEEE-754 binary64 nearest/ties-even
model. The complete discrete root-bit set—not an epsilon—is the oracle; a production result
must be a member after allowed final canonicalization.

`FT-trace` handles larger/forced physical shapes. A deterministic debug adapter observes or
forces leaf occurrence IDs, partial IDs, edge input/output bits, empty identities, and final
root. The independent validator proves a finite tree/forest, each demanded finite leaf once,
no extra leaf, one required binary64 operation per nonempty edge, and root correspondence.
Production arithmetic is observed, not reused as the expected operation. Debug
instrumentation need not become a public runtime API.

## V29-I — F2 execution shapes, specials, and final representation

For one finite bag exercise serial left/right-deep, balanced/irregular, one-row/standard/
irregular chunks, one/two/several deterministic worker partitions and Combine trees, hash/
ordered aggregate, no spill, and several forced spill/replay shapes. Require `FT-small`
membership or an `FT-trace` proof; never require cross-shape bit identity. All non-FLOAT
observables remain invariant.

`FS` classifies demanded explicit inputs independently: any NaN -> canonical NaN; both
infinity signs -> canonical NaN; only +Infinity -> +Infinity; only -Infinity -> -Infinity;
otherwise use the finite tree. Put specials first/middle/last, in separate workers and spill
partitions. Finite-edge-created infinity/NaN never sets explicit flags retroactively;
subsequent edges use binary64 behavior. Edge overflow is a value, not `NUMERIC_OVERFLOW`.
Finalize maps every zero root to `+0.0` and every NaN root to canonical bits without
legitimizing an illegal tree.

## V29-J — FLOAT64 AVG and exact internal count domain

For finite AVG independently obtain the `FT` subtotal, `IO` exact non-NULL count, Chapter-
29 internal count-to-binary64 conversion, one binary64 nearest/ties-even division, then `FS`
normalization. State and Combine contain subtotal, exact count, and explicit flags; worker-
local averages are never averaged.

Inject synthetic exact counts `0` (conversion helper only), `1`, `2`, `INT64_MAX`,
`INT64_MAX+1`, values around binary64 integer-rounding boundaries, very large finite-
conversion values, both sides of the finite-to-`+Infinity` boundary, and larger values.
Compare with an independent arbitrary-precision integer-to-binary64 ties-even converter.
AVG count remains valid when public COUNT would overflow. When conversion is `+Infinity`,
verify the defined binary64 division and final zero/NaN normalization without materializing
the impossible row count.

## V29-K — Combine and parallel aggregation

Build explicit partial-state trees. COUNT combines exact counts; integer SUM exact sums;
integer AVG exact sum/count; MIN/MAX canonical candidates. FLOAT empty+empty stays empty,
empty+finite is identity without an edge, finite+finite adds one `FT` edge, exact counts add,
and flags union independently of order. Cover balanced, left/right-deep, and seeded irregular
trees, alias/incompatible-state rejection where observable, and variable-backing cleanup.

With one/two/several workers and enumerated completion/Combine orders, require exact
aggregates to be identical and FLOAT results to pass `FT`. One local completion cannot
publish global readiness; all required Combine/Finalize work and quiescence precede the
dependent transition.

## V29-L — Aggregate errors, ordinals, demand, and provenance

Create several groups and descriptors with multiple COUNT/integer-SUM final failures. `ER`
derives the lowest semantic aggregate ordinal from the Chapter-19 source-occurrence table,
not descriptor layout, group order, worker, pointer, spill, or Finalize iteration. Preserve
that ordinal and SourceSpan through rewrites/sharing. FLOAT edge infinity is never a numeric-
overflow candidate.

Place a demanded child/argument error after a COUNT overflow marker, explicit FLOAT NaN or
infinity, and a MIN/MAX candidate that cannot improve. The Chapter-20/25 demand relation,
not accumulator convenience, controls evaluation. Preserve input/child error ownership and
do not evaluate undemanded work merely to seek an error. Resource/cancellation categories
and §39 transaction consequences remain separate; no global cross-class precedence is
invented.

## V29-M — All-group validation and ordered retained output

`RO` tracks ordered input, one active mutable group, closed-group Finalize, complete-row
materialization, retained-row append, global readiness, and Source. Early groups may occupy
internal retained storage but are not dependent/client output. A late numeric failure must
discard them and expose no aggregate prefix. Hash aggregate likewise exposes no Source
before successful complete Finalize.

For `PhysicalSortAggregate`, use Chapter-30/37-compatible contiguous groups. At every closed-
group barrier before readiness, attempt dependent/client access and require none. Successful
readiness requires all demanded input, required Combine, every group validation, all retained-
output preparation, and one dependency publication. Source then emits retained rows once in
the proven order without rewinding/reexecuting the child.

## V29-N — Retained-output, hash-aggregate spill, and pressure progress

The `RO/SP` ledgers cover group directory/keys, aligned and variable states, exact integer/
count backing, F2 state, VARCHAR candidates, worker-local/global tables, DISTINCT state,
partition metadata/buffers, and ordered retained fixed/varlen rows. Check exact extents,
continuous QueryMemoryManager ownership, transfer, release, cancellation, and failed-attempt
cleanup without dictating allocator/container layout.

For aggregate spill test raw qualifying-value replay and safe partial-state serialization.
Raw replay preserves every tagged occurrence. Partial state preserves exact integers/counts,
canonical candidates, FLOAT subtotal bits, flags, and descriptor state; decimal FLOAT
serialization and narrowing are rejected. Spill/tree shape may alter only an admitted FLOAT
root. Force many groups, skew, one large exact state, repeated pressure, and tiny budgets;
`SP` requires useful well-founded progress or the existing OOM/SpillIO/cancellation result,
never same-state recursion or approximation.

## V29-O — Ordered DISTINCT, algorithm substitution, and properties

Capability-valid ordered DISTINCT receives input whose independent Chapter-30 comparator
makes every `GO` class contiguous. It emits one `CR` row per adjacent class and may stream
only under Chapters 20/26 demand. Reject/invalidate an unproven/incompatible order or require
planner Sort enforcement; never generalize this path to baseline hash DISTINCT.

Compare hash/sort aggregate and hash/ordered DISTINCT only as secondary substitutability
checks against the independent oracles. Aggregate algorithms agree on groups,
representatives, NULL/empty behavior, exact aggregates, types, slots, flags, canonical
FLOAT representation, and owner results; FLOAT roots alone may differ through independently
legal trees. DISTINCT algorithms agree on classes and canonical rows. Hash aggregate/hash
DISTINCT advertise no order; ordered paths advertise only `OP`-proven properties, including
after retained-output spill. Global one-row cardinality creates no extra property.

## V29-P — Schema, RequiredSlotSet, HAVING, and semantic demand

`SS` compares group-key and aggregate outputs with the declared physical schema and original
LogicalSlotIds; runtime groups/state positions create no IDs. DISTINCT preserves child
schema/slots. RequiredSlotSet retains grouping keys, arguments, output, HAVING/ORDER BY
dependencies, and diagnostic provenance. Prune irrelevant payload and test zero/nearly-zero
width rows without changing row/group/class cardinality or demanded errors.

HAVING remains a downstream filter: after readiness it consumes declared slots and retains
TRUE only; FALSE/UNKNOWN and demanded errors use existing scalar procedures. Test LIMIT 0/1
over hash aggregate, hash DISTINCT, ordered aggregate, and ordered DISTINCT using the exact
V20/V26 demand oracle. Do not infer that blocking always executes or LIMIT always suppresses
input; hash DISTINCT cannot emit a newly observed class during Sink, while ordered DISTINCT
streams only when its capability and demand contract permit.

## V29-Q — Failures, cancellation, retry, invalid states, and Destroy

Inject OOM, representability, SpillIO, malformed temporary state, and cancellation at group/
state growth, varlen retention, spill creation/read/write, Combine, Finalize, retained-output
materialization, hash DISTINCT build, ordered active group/spool, ready Source, and ordered
DISTINCT. Before readiness no aggregate prefix exists; after readiness, Chapter-31 cursor
prefix rules apply to later source/transport failure. No failure narrows exact state, drops
groups, loses flags, returns a non-FT FLOAT, or approximates.

Poison failed-attempt group tables, exact/F2 states, counts, flags, candidates, DISTINCT set,
spill namespace, active group, retained rows, readiness, and source cursor. An independently
authorized retry uses fresh state; immutable plan/descriptor metadata alone may survive.
Enumerate cancellation schedules with explicit barriers and quiescence, not sleeps.

Use `IX` for Update-before-Initialize, misalignment, incompatible Combine, finalized/
destroyed-state misuse, forbidden repeated Finalize, Source-before-readiness, stale pointers,
wrong key/argument/output TypeId, corrupt count/flags, invalid selection/cursor/schema/slot,
post-terminal output, and failed-state reuse. Reject before unsafe access/output as an
internal invalid state, never UB or a new public SQL error. For owned backing, verify cleanup
after success, every failure stage, and cancellation with one semantic release; no specific
C++ destructor shape is required.

## V29-R — Constant evaluation, randomized properties, and hash perturbation

Where aggregate evaluation over an exactly known constant relation is admitted, exact
aggregates use `IO/MM`, while FLOAT uses one `FT`-legal tree and need not match every runtime
tree. Production folding is not its own oracle and ordinary scalar folding remains V17-owned.

Generate seeded small fixtures over TypeId, NULLs, groups/classes, FLOAT edges, manageable
integer boundaries, vector forms, and partitions. Use `GO/CR/IO/MM/FT-small`; larger cases
use `FT-trace`. Record failing seeds. Force controlled same-hash unequal keys and several
deterministic production seeds/layouts: classes and canonical/exact results remain fixed;
FLOAT may differ only when partitioning also yields another admitted tree. Structural checks
may observe grouped hash state, global no-table path, one active ordered mutable group, a
separately accounted spool, and capability-valid streaming DISTINCT, but impose no benchmark
threshold.

External databases may be used only for aligned relational bag/cardinality and NULL
semantics. PostgreSQL, DuckDB, SQLite, host accumulation, and any production DBlusBlus path
are not FLOAT64 F2 conformance oracles because their reduction contracts may differ.

## V29-S — Complete §29.3.8 boundary-vector map

Every row below is executed directly with exact bit/value/error comparison. `FT-explicit`
means evaluate the written parenthesization; `FT-all` proves the displayed result for every
tree possible for that listed bag. Synthetic states avoid impossible physical row counts.

| Architecture vector | Class | Independent oracle / expected property | Procedure |
|---|---|---|---|
| `SUM(F64: ((1e16+-1e16)+1))` | tree-dependent | FT-explicit root `1.0` | V29-H/I |
| `SUM(F64: 1e16+(-1e16+1))` | tree-dependent | FT-explicit canonical `+0.0` | V29-H/I |
| `SUM(+0.0)` | invariant | FT-all; canonical `+0.0` | V29-I |
| `SUM(-0.0)` | invariant | FT-all; canonical `+0.0` | V29-I |
| `SUM(+0.0,-0.0)` | invariant | FT-all; canonical `+0.0` | V29-I |
| `SUM(+Infinity, finite...)` | invariant | FS; `+Infinity` absent conflicting explicit special | V29-I |
| `SUM(-Infinity, finite...)` | invariant | FS; `-Infinity` absent conflicting explicit special | V29-I |
| `SUM(+Infinity,-Infinity)` | invariant | FS; canonical NaN | V29-I |
| `SUM(any input NaN, finite...)` | invariant | FS; canonical NaN | V29-I |
| `SUM(max,max)` | invariant two-leaf | FT-explicit `+Infinity`, no overflow error | V29-H/I |
| `SUM(((max+max)+-max))` | tree-dependent | FT-explicit `+Infinity` | V29-H/I |
| `SUM(max+(max+-max))` | tree-dependent | FT-explicit `max_finite` | V29-H/I |
| SUM smallest positive subnormal | invariant | exact input bits | V29-I |
| `SUM(1,2^-53)` | invariant two-leaf | FT-explicit `1.0` ties-even | V29-H/I |
| `SUM(INT64_MAX)` | invariant | IO; `INT64_MAX` | V29-F |
| `SUM(INT64_MAX,1)` | invariant | IO; Finalize `NUMERIC_OVERFLOW` | V29-F |
| `SUM(INT64_MAX,1,-1)` | invariant | IO; `INT64_MAX` | V29-F |
| `SUM(INT64_MIN)` | invariant | IO; `INT64_MIN` | V29-F |
| `SUM(INT64_MIN,-1)` | invariant | IO; Finalize `NUMERIC_OVERFLOW` | V29-F |
| `SUM(INT64_MIN,-1,1)` | invariant | IO; `INT64_MIN` | V29-F |
| `SUM(INT32_MAX,1)` | invariant | IO INT64 `2147483648` | V29-F |
| `COUNT = INT64_MAX` | synthetic boundary | IO; successful `INT64_MAX` | V29-F |
| `COUNT = INT64_MAX+1` | synthetic boundary | IO; Finalize `NUMERIC_OVERFLOW` | V29-F |
| `AVG(empty)` / `AVG(all NULL)` | invariant | AR typed NULL for each case | V29-B |
| `AVG(INT:1,2)` | invariant | IO exact rational `1.5` | V29-F |
| `AVG(INT64_MAX,INT64_MIN)` | invariant | IO exact rational `-0.5` | V29-F |
| AVG first cancellation tree | tree-dependent | FT-explicit subtotal 1; divide by 3, bits `0x3fd5555555555555` | V29-H/J |
| AVG second cancellation tree | tree-dependent | FT-explicit subtotal 0; canonical `+0.0` | V29-H/J |
| `AVG(+Infinity)` | invariant | FS; `+Infinity` | V29-I/J |
| `AVG(+Infinity,-Infinity)` | invariant | FS; canonical NaN | V29-I/J |
| `AVG(any input NaN,1.0)` | invariant | FS; canonical NaN | V29-I/J |
| `AVG(-0.0)` / `AVG(+0.0,-0.0)` | invariant | FT/FS; canonical `+0.0` for each case | V29-I/J |
| AVG smallest subnormal plus zero | invariant two-leaf | FT then halfway division -> even `+0.0` | V29-H/J |

## V29-T — Cross-chapter reuse map

Every reuse names an existing deterministic procedure and its independent oracle; V29 adds
the aggregate-specific composition and observations described above.

| Handoff | Canonical contract | V29 family | Reused Verification methodology / oracle |
|---|---|---|---|
| Ch17→29 | scalar/FLOAT equality, order, NaN, hash, casts, canonical representative | C/D/G/H/J | V17 FLOAT/comparison/hash/cast families; independent scalar oracle |
| Ch18→29 | call syntax and unsupported forms | A | lexer/parser aggregate-call matrices |
| Ch19→29 | binding, overload, placement, occurrence ordinal/provenance | A/L/P | V19-12/13 and V20-9 occurrence table |
| Ch20→29 | groups, DISTINCT, bag/cardinality, demand, slots | B/D/E/P | V20-4/8/9/12–18; GO/DO/SS |
| Ch21→29 | authorized retry and transaction consequence | L/Q | V21-2/3/13/14; ER/AF |
| Ch22→29 | operator vocabulary, schema, capability, bounded-result family | A/E/O/P | V22-A/B/C/K; plan/schema/capability oracle |
| Ch23→29 | active occurrences, validity, representations, borrowing | B/C/G | V23-A/C/D/G/H/I/K/M; DO/lifetime graph |
| Ch24→29 | accounting, RowCollection, spill, progress/resources | A/C/M/N/Q | V24-B/D/H/J/K/L/M/N; SP/accounting ledger |
| Ch25→29 | argument evaluation, demanded errors, FLOAT handoff | B/H/L/P | V25-B/E/I/J/O/P; demand/error oracle |
| Ch26→29 | Sink/Combine/Finalize/Source, readiness, cancel/retry | A/E/K/M/Q | V26-C/G/H/I/K/L/Q; PL/AF |
| Ch28→29 | GROUPING hash mode and collision recheck | C | V28-C/D hash-candidate method specialized by GO/GH |
| Ch29→30 | comparator/order making classes contiguous | M/O | Sort Tests plus V20-10; OP |
| Ch29→31 | internal readiness versus external cursor prefix | M/Q | V25-O; V21-14; result-envelope oracle |
| Ch29→32 | local/global state, Combine, readiness/quiescence | K/Q | Parallel Execution Tests; V26-K; PL |
| Ch29→37 | OrderingProperty and RequiredSlotSet | O/P | V22-C/G; V20-10; OP/SS |
| Ch29→38 | capability, cost/resource applicability, final validation | O/R | physical property/plan validation procedures |
| Ch29→39 | numeric/resource/cancel categories and transaction result | F/I/L/N/Q | V24-L/M; V21-3; ER |
| Ch29→41 | architecture-level verification obligations | H–S | independent-oracle and perturbation requirements above |

## V29-U — Atomic architecture-obligation ledger

One row is one independently falsifiable obligation. Repeated §29.9 summaries and examples
map to their owning rows rather than inflating the inventory. Each row requires a named
procedure and independent oracle. Rationale/navigation are not atoms. Unsupported syntax is represented
by falsifiable rejection rows rather than N/A.

| ID | Architecture location | Atomic obligation | Procedure | Oracle / exact reuse |
|---|---|---|---|---|
| V29-A01 | 29.1, 29.5 | No grouping keys select the global single-state path. | V29-A/B | AR |
| V29-A02 | 29.1 | Grouped hash aggregation is a pipeline breaker through finalization. | V29-A/M | PL |
| V29-A03 | 29.2 | Descriptor state size is respected without fixing representation. | V29-A | state extent adapter |
| V29-A04 | 29.2 | Descriptor state alignment is respected. | V29-A/Q | alignment/IX |
| V29-A05 | 29.2 | Initialize establishes the exact empty semantic state. | V29-A | AR/state trace |
| V29-A06 | 29.2 | Update implements the descriptor's semantic state transition. | V29-A/B | AR/DO |
| V29-A07 | 29.2 | Combine implements the descriptor's Merge law. | V29-A/K | AR/IO/FT/MM |
| V29-A08 | 29.2 | Finalize implements the descriptor's result/error rule. | V29-A/F–J | AR/IO/FT/FS/MM |
| V29-A09 | 29.2, 29.3.1 | Owned variable backing is query-accounted and conditionally destroyed. | V29-A/N/Q | accounting/lifetime ledger |
| V29-A10 | 29.2 | Dispatch is aggregate/vector-batch scoped rather than SQL rebinding per row. | V29-A | V25 dispatch trace |
| V29-B01 | 29.3 | the architecture admits exactly COUNT, SUM, AVG, MIN, and MAX forms in AR. | V29-A | literal AR |
| V29-B02 | 29.3.2 | Every admitted overload has its exact declared input/result TypeIds. | V29-A | literal AR |
| V29-B03 | 29.3.2 | Every admitted overload has its exact declared nullability. | V29-A/B | literal AR |
| V29-B04 | 29.3 | Aggregate DISTINCT forms are rejected in the defined architecture. | V29-A | V18/V19 rejection oracle |
| V29-B05 | 29.3 | Aggregate FILTER forms are rejected in the defined architecture. | V29-A | V18/V19 rejection oracle |
| V29-B06 | 29.3.2 | BOOLEAN MIN/MAX are rejected. | V29-A | literal AR |
| V29-B07 | 29.3.2 | Unlisted SUM/AVG TypeIds and implicit conversions are rejected. | V29-A | literal AR |
| V29-B08 | 29.3.2 | Executor dispatch cannot admit a binder-rejected overload. | V29-A/Q | AR/IX |
| V29-C01 | 29.3.1 | ExactSignedInteger is logically unbounded for admitted subtotals. | V29-F/N | IO/accounting |
| V29-C02 | 29.3.1 | ExactNonnegativeCount is a mathematical nonnegative integer without INT64 state limit. | V29-F/J | IO |
| V29-C03 | 29.3.1 | Binary64Partial is EMPTY or one admitted-tree subtotal. | V29-H/K | FT |
| V29-C04 | 29.3.1 | FloatAggregateState retains non-NULL state and three explicit-special flags. | V29-I/J | FS/state trace |
| V29-C05 | 29.3.1 | OrderedCandidate is empty or one canonical owned non-NULL value. | V29-G | MM/lifetime |
| V29-C06 | 29.3.1 | Exact state never wraps or narrows to host/result width. | V29-F/N | IO |
| V29-C07 | 29.3.1 | Alternate FLOAT machinery is conforming only if it yields an admitted tree result. | V29-H | FT |
| V29-C08 | 29.3.1 | Resource failure cannot authorize omitted or approximate state. | V29-N/Q | SP/error ledger |
| V29-D01 | 29.3.4, 29.9 | Every active demanded logical occurrence contributes according to aggregate admission. | V29-B | DO |
| V29-D02 | 29.3.4 | CONSTANT cardinality N contributes N occurrences. | V29-B | DO |
| V29-D03 | 29.3.4 | Repeated DICTIONARY positions contribute repeatedly. | V29-B | DO |
| V29-D04 | 23, 29.9 | Only active selection lanes contribute. | V29-B | V23 active-domain oracle |
| V29-D05 | 29.3.2 | NULL validity prevents argument admission and poisoned-payload reads. | V29-B | validity/access ledger |
| V29-D06 | 29.5 | Global aggregation emits one successful row on empty input. | V29-B | AR/global-group oracle |
| V29-D07 | 20.9, 29.5 | Explicit grouped aggregation emits zero rows on empty input. | V29-B | GO |
| V29-D08 | 29.3.2, 29.5 | COUNT empty is zero; other aggregates with no non-NULL are typed NULL. | V29-B | AR |
| V29-E01 | 29.4 | Group equality uses GROUPING mode, not ordinary nullable SQL equality. | V29-C | GO |
| V29-E02 | 29.4 | Typed NULLs share one grouping class. | V29-C | GO |
| V29-E03 | 29.4 | FLOAT signed zeros share one grouping class. | V29-C | GO |
| V29-E04 | 29.4 | Grouping-equivalent NaNs share one grouping class. | V29-C | GO |
| V29-E05 | 29.4 | VARCHAR grouping uses exact byte length and bytes. | V29-C | GO/V17 byte oracle |
| V29-E06 | 29.4 | Composite grouping equality is componentwise. | V29-C | GO |
| V29-E07 | 29.4 | Equal grouping values are hash-lookup compatible. | V29-C | GH |
| V29-E08 | 29.4 | Hash collisions require full grouping-equality recheck. | V29-C/R | GH |
| V29-F01 | 29.4 | One semantic aggregate state exists per grouping-equivalence class. | V29-C | GO/state map |
| V29-F02 | 29.4 | Retained group keys are deep/stably owned beyond input reuse. | V29-C | lifetime poison |
| V29-F03 | 29.4 | Retained VARCHAR group keys preserve embedded NUL/arbitrary bytes. | V29-C | byte oracle/lifetime |
| V29-F04 | 29.4 | Group directory, keys, states, and metadata are query-accounted. | V29-C/N | accounting ledger |
| V29-F05 | 29.4 | Internal group-key representation remains implementation-defined. | V29-C/R | semantic equivalence variants |
| V29-F06 | 29.4, 20.9 | Emitted group keys use CanonicalGroupingRepresentative componentwise. | V29-D | CR |
| V29-F07 | 29.4 | Zero grouping class emits positive-zero bits. | V29-D | CR/bit oracle |
| V29-F08 | 29.4 | NaN grouping class emits canonical quiet-NaN bits. | V29-D | CR/bit oracle |
| V29-F09 | 29.4 | Hash/worker/Combine/spill/algorithm order cannot choose group-key bits. | V29-D/R | CR perturbations |
| V29-F10 | 17.10.3, 29.4 | Group-output canonicalization does not alter ordinary scalar/persisted semantics. | V29-D | V17 regression oracle |
| V29-G01 | 29.3.2–29.3.3 | COUNT(*) counts every demanded row including zero-width rows. | V29-B/F | DO/IO |
| V29-G02 | 29.3.2–29.3.3 | COUNT(expr) evaluates demanded arguments and ignores NULL values. | V29-F/L | IO/demand |
| V29-G03 | 29.3.3 | COUNT state never wraps. | V29-F | IO |
| V29-G04 | 29.3.3 | COUNT at INT64_MAX finalizes successfully. | V29-F/S | IO synthetic state |
| V29-G05 | 29.3.3 | COUNT above INT64_MAX raises NUMERIC_OVERFLOW at Finalize. | V29-F/L/S | IO/ER |
| V29-G06 | 29.3.3 | COUNT overflow marker is not an Update/Combine error. | V29-F/K | IO/state trace |
| V29-G07 | 29.3.3 | COUNT overflow knowledge cannot suppress later demanded work. | V29-F/L | demand/ER |
| V29-G08 | 29.3.2–29.3.3 | Integer SUM ignores NULL and uses exact mathematical state. | V29-F | IO |
| V29-G09 | 29.3.3 | An out-of-range integer partial subtotal is not itself an SQL error. | V29-F/K | IO |
| V29-G10 | 29.3.3 | Later cancellation can return an in-range integer SUM. | V29-F/S | IO |
| V29-G11 | 29.3.3 | Integer SUM applies INT64 range only at Finalize. | V29-F/L | IO/ER |
| V29-G12 | 29.3.3 | Out-of-range final integer SUM raises NUMERIC_OVERFLOW without wrap/saturation. | V29-F/L | IO/ER |
| V29-G13 | 29.3.2–29.3.3 | SUM(INT32) returns nullable INT64 and admits INT32-exceeding valid sums. | V29-F/S | AR/IO |
| V29-G14 | 29.3.3 | Integer SUM result/error is execution-shape invariant. | V29-F/K/O | IO |
| V29-H01 | 29.3.2–29.3.3 | Integer AVG retains exact sum and exact non-NULL count. | V29-F | IO |
| V29-H02 | 29.3.3 | Integer AVG does not first range-check an integer SUM result. | V29-F | IO |
| V29-H03 | 29.3.3 | Integer AVG converts exact rational sum/count once to binary64 ties-even. | V29-F | IO rational rounding |
| V29-H04 | 29.3.3 | Integer AVG never averages partial averages. | V29-F/K | IO |
| V29-H05 | 29.3.3 | Integer AVG internal count is not limited by COUNT result width. | V29-F/J | IO synthetic count |
| V29-H06 | 29.3.3 | Integer AVG cannot create infinity from valid finite integer inputs. | V29-F | IO/range proof |
| V29-H07 | 29.3.7 | Integer AVG is invariant across serial/parallel/spill/algorithm shapes. | V29-F/K/O | IO |
| V29-I01 | 29.3.4 | Every demanded non-NULL finite FLOAT occurrence is exactly one tree leaf. | V29-H/I | FT/DO |
| V29-I02 | 29.3.4 | A legal FLOAT reduction tree is finite. | V29-H | FT graph validator |
| V29-I03 | 29.3.4 | Every nonempty internal edge is binary64 addition. | V29-H | bit-exact FT |
| V29-I04 | 29.3.4 | Every edge uses nearest/ties-even rounding. | V29-H | bit-exact FT |
| V29-I05 | 29.3.4 | Every edge subtotal has binary64 semantics with no hidden excess precision. | V29-H/Q | FT/IX |
| V29-I06 | 29.3.4 | Empty partial is identity without a synthetic edge. | V29-H/K | FT trace |
| V29-I07 | 29.3.4 | Physical execution may select any admitted tree shape. | V29-H/I | FT result set |
| V29-I08 | 29.3.4 | Vector/chunk shape may alter the admitted FLOAT root. | V29-I | FT |
| V29-I09 | 29.3.4, 32.6 | Worker partition/schedule/Combine may alter the admitted FLOAT root. | V29-I/K | FT/PL |
| V29-I10 | 29.3.4, 29.6 | Spill partition/replay may alter the admitted FLOAT root. | V29-I/N | FT/SP |
| V29-I11 | 29.3.4, 29.10 | Hash versus ordered aggregate may alter the admitted FLOAT root. | V29-I/O | FT |
| V29-I12 | 29.3.4 | Every FLOAT result must belong to an admitted-tree discrete result family. | V29-H/I | FT-small/FT-trace |
| V29-J01 | 29.3.5 | Any demanded explicit input NaN selects canonical NaN. | V29-I | FS |
| V29-J02 | 29.3.5 | Explicit inputs containing both infinity signs select canonical NaN. | V29-I | FS |
| V29-J03 | 29.3.5 | Only explicit positive infinity selects positive infinity. | V29-I | FS |
| V29-J04 | 29.3.5 | Only explicit negative infinity selects negative infinity. | V29-I | FS |
| V29-J05 | 29.3.5 | Explicit-special classification is order/worker/spill independent. | V29-I/K/N | FS |
| V29-J06 | 29.3.4–29.3.5 | Finite-edge-generated nonfinite values do not set explicit-input flags. | V29-I | FT/FS |
| V29-J07 | 29.3.4 | Generated nonfinite subtotals participate in later ordinary binary64 edges. | V29-I | FT |
| V29-J08 | 29.3.4–29.3.5 | Exposed zero SUM/AVG is canonical positive zero. | V29-I/J | FS/bit oracle |
| V29-J09 | 29.3.4–29.3.5 | Exposed NaN SUM/AVG is the canonical quiet NaN. | V29-I/J | FS/bit oracle |
| V29-J10 | 29.3.4, 39.3 | FLOAT edge overflow is an IEEE value, not NUMERIC_OVERFLOW. | V29-I/L | FT/ER |
| V29-K01 | 29.3.2, 29.3.4 | FLOAT SUM finalizes to selected legal root absent explicit-special winner. | V29-H/I | FT/FS |
| V29-K02 | 29.3.2, 29.3.4 | FLOAT AVG state retains F2 subtotal, exact count, and flags. | V29-J | FT/IO/FS |
| V29-K03 | 29.3.4 | FLOAT AVG count increments for every non-NULL input including specials/zeros. | V29-J | DO/IO |
| V29-K04 | 29.3.4 | FLOAT AVG internal count has no INT64 result-width limit. | V29-J | IO |
| V29-K05 | 29.3.4 | Exact AVG count converts to binary64 nearest/ties-even. | V29-J | arbitrary-precision conversion oracle |
| V29-K06 | 29.3.4 | Count beyond finite binary64 conversion range converts to positive infinity. | V29-J | arbitrary-precision boundary oracle |
| V29-K07 | 29.3.4 | Finite FLOAT AVG performs one binary64 subtotal/converted-count division. | V29-J | bit-exact division oracle |
| V29-K08 | 29.3.4 | FLOAT AVG does not average worker-local averages. | V29-J/K | FT/IO |
| V29-K09 | 29.3.4 | FLOAT AVG low bits may vary only through its SUM tree. | V29-I/J | FT |
| V29-L01 | 29.3.6 | MIN/MAX support exactly the listed non-BOOLEAN TypeIds. | V29-A/G | AR/MM |
| V29-L02 | 29.3.6 | MIN/MAX ignore NULL and return NULL with no non-NULL candidate. | V29-G | MM |
| V29-L03 | 29.3.6 | MIN/MAX use Chapter-17 total order. | V29-G | MM |
| V29-L04 | 29.3.6 | FLOAT zero candidates canonicalize to positive zero. | V29-G | MM/bit oracle |
| V29-L05 | 29.3.6 | FLOAT NaN candidates canonicalize before retention. | V29-G | MM/bit oracle |
| V29-L06 | 29.3.6 | Equal MIN/MAX classes retain the same canonical representative. | V29-G/K | MM |
| V29-L07 | 29.3.6 | VARCHAR MIN/MAX retains exact owned bytes beyond source lifetime. | V29-G/Q | MM/lifetime poison |
| V29-L08 | 29.3.7 | MIN/MAX result is execution-shape invariant. | V29-G/K/O | MM |
| V29-M01 | 29.3.7 | Exact aggregate Merge uses initialized empty state as identity. | V29-K | IO/MM |
| V29-M02 | 29.3.7 | FLOAT empty partial is identity and two nonempty partials add one edge. | V29-K | FT trace |
| V29-M03 | 29.3.7 | FLOAT Combine adds exact AVG counts without narrowing. | V29-J/K | IO |
| V29-M04 | 29.3.7 | FLOAT Combine unions explicit-special flags commutatively. | V29-I/K | FS |
| V29-M05 | 29.3.7 | Different FLOAT Combine trees may yield different admitted roots. | V29-H/K | FT |
| V29-M06 | 29.3.7 | Combine cannot alter occurrence membership, count, flags, type, or canonical output. | V29-K | DO/IO/FS/AR |
| V29-M07 | 32.6 | Worker-local completion is not global aggregate readiness. | V29-K/M | PL |
| V29-M08 | 32.6 | Exact aggregates remain invariant to worker and Merge order. | V29-F/G/K | IO/MM |
| V29-M09 | 32.6 | Parallel FLOAT results correspond to an admitted tree. | V29-H/I/K | FT |
| V29-M10 | 29.3.7 | Aggregate constant evaluation selects a conforming semantic result. | V29-R | IO/MM/FT |
| V29-N01 | 29.3.7 | Demanded argument/child errors precede supplying that aggregate value. | V29-L | V25 demand/error oracle |
| V29-N02 | 29.3.3, 29.3.7 | COUNT overflow knowledge cannot terminate demanded child execution. | V29-F/L | demand/ER |
| V29-N03 | 29.3.7 | Integer SUM cannot fail from an intermediate subtotal. | V29-F/L | IO/ER |
| V29-N04 | 29.3.7 | Explicit FLOAT special state cannot suppress later demanded expressions. | V29-I/L | demand/ER |
| V29-N05 | 29.3.7 | Lowest semantic aggregate ordinal wins competing numeric finalization errors. | V29-L | ordinal ER |
| V29-N06 | 29.3.7 | Ordinal derives from source occurrence and survives rewrites/sharing. | V29-L/P | V19/V20 occurrence oracle |
| V29-N07 | 29.3.7 | Group/hash/worker/spill/Finalize order cannot select numeric diagnostic. | V29-L | ER perturbation |
| V29-N08 | 29.3.7, 39 | Resource and cancellation failures retain their existing categories. | V29-N/Q | V24/ER |
| V29-N09 | 29.3.7, 39 | Aggregate execution creates no cross-class error precedence. | V29-L/Q | ER |
| V29-N10 | 29.3.7, 39 | Transaction consequence remains §39-owned. | V29-L/Q | V21/ER |
| V29-O01 | 26.1, 29.3.7 | Every demanded group is numerically validated before aggregate Source readiness. | V29-M | PL/RO |
| V29-O02 | 29.3.7 | A failing group fails the whole aggregate invocation. | V29-L/M | ER/RO |
| V29-O03 | 29.3.7 | No successful aggregate prefix is exposed before all-group validation. | V29-M | RO/result envelope |
| V29-O04 | 29.10 | SortAggregate mutable aggregate state may be approximately one active group. | V29-M/R | RO structural trace |
| V29-O05 | 29.10 | One-group mutable-state bound is not a total-query-memory bound. | V29-M/N | RO/accounting |
| V29-O06 | 29.10 | A closed ordered group is finalized/validated before row retention. | V29-M | RO |
| V29-O07 | 29.10 | Complete finalized rows enter stable order-preserving temporary storage. | V29-M/N | RO/SS |
| V29-O08 | 29.10 | Retained rows are not dependent/client output before readiness. | V29-M | RO/PL |
| V29-O09 | 29.10 | Later pre-readiness failure discards prior retained rows. | V29-M/Q | RO/AF |
| V29-O10 | 29.10 | SortAggregate Source starts only after complete successful readiness. | V29-M | RO/PL |
| V29-P01 | 29.6 | Hash aggregate may spill raw qualifying values/rows exactly. | V29-N | SP/DO |
| V29-P02 | 29.6 | Safe partial-state spill preserves exact integer/count state. | V29-N | SP/IO |
| V29-P03 | 29.6 | FLOAT partial-state spill preserves subtotal bits, count, flags, and descriptor state. | V29-N | SP/FT/IO/FS |
| V29-P04 | 29.6 | MIN/MAX spill preserves canonical candidate and owned VARCHAR bytes. | V29-G/N | SP/MM |
| V29-P05 | 29.6 | FLOAT subtotal spill through decimal text is forbidden. | V29-N/Q | SP/IX |
| V29-P06 | 29.6 | Spill/replay cannot omit or duplicate contributions. | V29-N | SP/DO |
| V29-P07 | 29.6 | Recursive repartition is bounded and makes relevant progress or fails. | V29-N | SP progress model |
| V29-P08 | 29.6, 24.6 | Skew/large state never permits approximation fallback. | V29-N/Q | SP/error ledger |
| V29-P09 | 29.10 | Ordered retained rows are continuously accounted and spill-capable. | V29-M/N | RO/accounting/SP |
| V29-P10 | 29.10 | Retained spill/replay preserves the proven output order. | V29-M/N/O | RO/OP |
| V29-Q01 | 29.7 | Hash DISTINCT uses grouping-equivalence classes. | V29-D/E | GO |
| V29-Q02 | 29.7 | Hash DISTINCT Sink consumes all demanded input and emits no row. | V29-E | PL/DO |
| V29-Q03 | 29.7 | Hash DISTINCT Finalize publishes readiness only after complete successful build. | V29-E | PL |
| V29-Q04 | 29.7 | Hash DISTINCT Source emits exactly one row per class after readiness. | V29-D/E | GO/PL |
| V29-Q05 | 29.7 | Hash DISTINCT emits the componentwise canonical class representative. | V29-D/E | CR |
| V29-Q06 | 29.7 | Early class discovery cannot suppress later demanded input/error. | V29-E/L | demand/ER |
| V29-Q07 | 29.7–29.8 | Hash DISTINCT advertises no ordering. | V29-E/O | OP |
| V29-Q08 | 29.7 | Hash DISTINCT spill preserves classes and canonical representatives. | V29-E/N | GO/CR/SP |
| V29-R01 | 29.10 | Ordered aggregate/Distinct eligibility requires capability and final-plan validation. | V29-O | capability oracle |
| V29-R02 | 29.10 | SortAggregate input ordering makes grouping classes contiguous. | V29-M/O | OP/GO |
| V29-R03 | 29.10 | Ordered DISTINCT input ordering makes duplicate classes contiguous. | V29-O | OP/GO |
| V29-R04 | 29.10 | Ordered DISTINCT emits one canonical row per adjacent class. | V29-D/O | GO/CR |
| V29-R05 | 29.10 | Ordered DISTINCT streaming remains governed by semantic demand. | V29-O/P | V20/V26 demand |
| V29-R06 | 29.8–29.10 | Ordered operators advertise only proven OrderingProperty. | V29-O | OP |
| V29-R07 | 29.8–29.10 | Hash/ordered aggregates agree except admitted FLOAT root variability. | V29-H/O | GO/CR/IO/MM/FT/SS |
| V29-R08 | 29.7, 29.10 | Hash/ordered DISTINCT agree on classes and canonical rows. | V29-D/O | GO/CR/SS |
| V29-S01 | 20, 29.4–29.10 | Aggregate output uses declared schema and LogicalSlotIds. | V29-P | SS |
| V29-S02 | 20, 29.7 | DISTINCT preserves child schema and LogicalSlotIds. | V29-D/P | SS |
| V29-S03 | 37, 29.9 | RequiredSlotSet retains grouping keys and aggregate arguments. | V29-P | SS |
| V29-S04 | 37, 29.9 | RequiredSlotSet retains output/HAVING/ORDER BY/provenance dependencies. | V29-P | SS |
| V29-S05 | 37, 29.9 | Payload pruning preserves zero-width occurrence/group/class cardinality. | V29-B/P | DO/SS |
| V29-S06 | 20, 29 | HAVING remains a downstream TRUE-only filter over ready aggregate slots. | V29-P | V20 scalar/filter oracle |
| V29-S07 | 20, 26, 29 | LIMIT/nonexecution is decided by canonical semantic demand. | V29-P | V20/V26 demand |
| V29-S08 | 29.8 | Hash aggregate output order is unspecified. | V29-C/O | OP |
| V29-S09 | 29.8 | Global one-row output does not invent an OrderingProperty. | V29-O | OP |
| V29-T01 | 29.3.1, 29.4, 29.6, 29.10 | All dynamically growing aggregate/DISTINCT/spool memory has an accounted owner. | V29-N | accounting ledger |
| V29-T02 | 24, 29 | Exact extent/representability and OOM remain distinct. | V29-N/Q | V24 classifier |
| V29-T03 | 29.3.7, 29.10 | Spill write/read/capacity failures use SpillIOError. | V29-N/Q | V24 error oracle |
| V29-T04 | 29.10, 31 | Post-readiness source failure preserves prior returned cursor prefix but not query success. | V29-M/Q | result envelope |
| V29-T05 | 26, 29, 32 | Cancellation prevents readiness, quiesces workers, and cleans query state. | V29-Q | V26 cancellation model |
| V29-T06 | 21, 26, 29 | Retry uses fresh group/aggregate/DISTINCT/spill/spool/cursor state. | V29-Q | AF |
| V29-T07 | 29.2, 39 | Invalid aggregate protocol/state is rejected before unsafe use. | V29-Q | IX |
| V29-T08 | 29.2 | Owned aggregate backing is released after success/failure/cancellation. | V29-A/Q | lifetime ledger |
| V29-T09 | 29.10 | Ordered retained state has no WAL, persistence, recovery, or retry identity. | V29-M/Q | AF/persistence-negative ledger |
| V29-T10 | 29.10 | SortAggregate output does not rewind or reexecute original input. | V29-M | RO/input-generation trace |
| V29-U01 | 29.3.8 | Every normative boundary vector maps to a direct independent oracle. | V29-S | IO/FT/FS/AR |
| V29-U02 | 29.3.9 | Omitted/duplicated/deduplicated aggregate occurrences are rejected. | V29-B/H/Q | DO/FT/IX |
| V29-U03 | 29.3.9 | Wrong FLOAT rounding/hidden incompatible precision is rejected. | V29-H/Q | FT/IX |
| V29-U04 | 29.3.9 | Partial-average averaging and exact-count corruption are rejected. | V29-J/K/Q | IO/FT/IX |
| V29-U05 | 29.3.9 | Noncanonical FLOAT zero/NaN outputs are rejected. | V29-D/G/I/Q | CR/MM/FS |
| V29-U06 | 29.3.9 | FLOAT variability cannot leak to exact aggregates, groups, or DISTINCT representatives. | V29-D/F/G/O | CR/IO/MM/FT |
| V29-U07 | 22.4.1, 24.6, 39.3 | Cross-chapter bounded-result exceptions are limited to owner-admitted F2 trees. | V29-H/N/O | FT/cross-owner table |
| V29-U08 | 29, 41 | Procedures use independent oracles, deterministic controls, and no production self-oracle. | V29-R/T | oracle audit |
| V29-U10 | 29 | Verification preserves implementation freedom for layouts, containers, trees, workers, and spill shape. | V29-A/H/N/R/V | alternative-realization audit |
| V29-U11 | 29 | No stale exact-dyadic/execution-shape-identical FLOAT aggregate rule remains. | V29-V | stale-rule search |
| V29-U12 | 29 | External differential databases are not FLOAT F2 conformance oracles. | V29-H/R/V | oracle-source audit |

## V29-V — Negative-case and oracle checks

Search all Chapter-29-facing methodology and reject, with direct fixtures above: exact-dyadic
or exact-n-ary FLOAT SUM/AVG; bit-identical FLOAT roots across vectors/workers/Combine/spill/
algorithms; epsilon acceptance; exact-rational FLOAT AVG; delaying every FLOAT rounding to
Finalize; decimal/dyadic partial spill as a requirement; first group/DISTINCT representative;
streaming hash DISTINCT Sink; same-hash group identity; ordinary NULL equality for grouping;
separate signed-zero or NaN-payload classes; physical-order representative/diagnostic choice;
hash order as SQL order; early SortAggregate publication; one-group total memory; unaccounted
retained output; reordered advertised output; wrapping/early-stopping COUNT; intermediate
integer-SUM overflow; averaged partial averages; FLOAT infinity as NUMERIC_OVERFLOW;
resource-failure approximation; and one-worker readiness. Each is falsifiable through
V29-B–U and is nonconforming.

The valid exactness wording is limited to COUNT/integer SUM/AVG, MIN/MAX canonical values,
exact occurrence/count/state preservation, exact binary64 bits at each selected F2 edge, and
independent Chapter-34 statistics methodology. The compact Aggregate Tests and Parallel
Execution Tests are smoke/delegation indexes; V29-A–V is the complete Chapter-29 owner.

Exercise permitted state/container layouts, hash mixes, chunk capacities, worker counts,
schedules, exact tree selections, spill partitions, and equivalent retained-row storage.
Use deterministic seeds and barriers rather than sleeps.
