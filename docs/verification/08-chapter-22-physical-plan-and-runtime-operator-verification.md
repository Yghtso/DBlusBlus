# Chapter 22 — Physical Plan and Runtime Operator Verification

This family verifies the Chapter-22 physical-plan/runtime foundation. It uses abstract
plan descriptions and public architectural identities rather than source classes or C++
layout. Detailed operator algorithms remain covered by their downstream execution,
property, and optimizer families; the reuse links below compose these verification procedures.

The independent test-side oracles used throughout are:

```text
PS  declarative physical-plan schema and child-shape model
OG  immutable-plan/runtime ownership and lifetime graph
SM  LogicalSlotId-to-physical-position mapping table
BM  row-occurrence multiset and required-order comparator
PO  Chapter-37 property/satisfaction model
CR  physical capability/applicability registry model
EC  execution-context identity and state-lifetime tracker
VM  independent MVCC/access-path visibility and residual model
MK  mathematical-integer LIMIT/OFFSET/first-K model
PV  independent physical-plan legality predicate
DM  DML/DDL final-state, diagnostic-precedence, and publication model
PF  persistent-format field registry
```

Production property derivation, costing, validation, and execution are never accepted as
their own oracle. Randomized variants use recorded deterministic seeds. Scheduling tests
use barriers; no procedure uses sleeps.

Across physical shapes, compare outcomes against the owning semantic result family.
For FLOAT64 SUM/AVG, reuse V29-H–K's independent legal-tree oracle rather than a
single serial result. Apply downstream scalar/relational semantics to each admitted
aggregate result; exact cross-run equality is required only where the owner
guarantees it. This qualification applies to V22-D/G/K/L and composed V23/V24
perturbation tests; it does not weaken exact retained-value or multiplicity checks.

Activate the composition with a V29-H tagged finite bag whose independently
enumerated root set contains at least two distinct results. Force two legal trees
through worker/chunk/spill choices, record their occurrence/edge traces, and require
each result to match its independent tree evaluation. Feed each into the same
downstream projection/filter fixture and compare against the independently applied
Chapter-17/20 semantics. A result outside the admitted set, a lost/duplicated leaf,
or changed exact COUNT fails; a differing legal FLOAT root does not. Repeat with a
tree-invariant aggregate as an exact-equality control. V29 owns the arithmetic
oracle, V22 the plan composition, and V24 the resource/retention observations.

## V22-A — Resolved handoff and immutable plan

Construct resolved logical/statement fixtures, then compare the emitted physical plan
against `PS`. The physical plan must consume the existing `LogicalSlotId`, `BindingId`,
type/nullability, aggregate ordinal, SourceSpan/cast provenance, descriptor identity,
subquery occurrence/mode, and hidden DML target identity where applicable. Poison parser,
identifier-normalization, binder, type-inference, and catalog-name-lookup entry points;
physical construction and execution must not call them or mutate the logical fixture.

Publish one physical plan, record a canonical plan digest and `OG`, and execute it twice
with deliberately different contexts. Before the second execution, poison every released
cursor, chunk, reservation, spill handle, continuation, counter, subquery state, and
worker-local cell from the first. The plan digest and plan-owned graph must remain
unchanged, and the second result must equal the independent semantic oracle. This proves
that mutable global state is execution-scoped, local state is worker/task-scoped, and no
transaction, snapshot, epoch registration, cancellation, memory use, cursor, or other
mutable execution state leaks through an immutable node. If a plan is reused, repeat its
descriptor/identity/schema validation; this procedure does not invent a cache policy.

## V22-B — Output schema and LogicalSlotId mapping

`SM` assigns semantic identities before physical planning and maps each identity to the
selected plan's output position. It never derives identity from a pointer, value,
`ColumnId`, `BindingId`, aggregate ordinal, or physical ordinal. Exercise:

| Fixture | Required mapping result |
|---|---|
| `SELECT a,a` | two distinct output `LogicalSlotId`s even when values match |
| same expression projected twice | two declared output occurrences; sharing is behind `SM` |
| self join | left and right identities remain distinct |
| Project reorder | identities follow declared outputs, not prior ordinals |
| join | output is exact left-then-right slot concatenation |
| grouped aggregate | group and aggregate outputs use declared fresh identities |
| repeated aggregate expression | distinct outputs retain aggregate occurrence/ordinal mapping |
| derived table | the outer boundary uses its declared remap |
| hidden DML target | RID/system identity remains internal and available until consumed |
| Filter/Sort/Limit | schema and slot identities pass through exactly |

For each fixture, permute physical positions and duplicate equal values. Every consumer
must still locate its input through `LogicalSlotId`. Provenance/lineage is compared to the
bound/logical origin table independently of physical sharing.

## V22-C — Bags, semantic order, and canonical properties

`BM` tags each input occurrence independently of value equality. Compare unordered output
as a multiset of occurrences and ordered output with the independent Chapter-17/20
comparator. Perturb heap page/slot order, duplicate-key RID order, hash seed/bucket order,
vector lanes, batch boundaries, worker schedule, and spill partitions. These may alter
physical visitation and legal plan shape, but not multiplicity, required SQL order,
diagnostic/provenance, transaction result, or persistent state. Deduplication is accepted
only from the logical Aggregate, DISTINCT, or Limit selection contract.

Chapter 37 remains the sole property oracle. `PO` verifies exact
`OrderingProperty`/`RequiredSlotSet` structure, prefix satisfaction, enforcement, and
LogicalSlotId identity. An operator-name rule or one accidentally sorted execution is a
negative fixture: a parent requiring order must receive an exact provider or enforcer.

| Property/property-like item | Exact? | Physical property? | Other category | Canonical owner and V22 treatment |
|---|---:|---:|---|---|
| `OrderingProperty` | yes | yes | — | Ch37 `PO`; exact keys/direction/NULL/collation/prefix |
| `RequiredSlotSet` | yes | yes | — | Ch37 `PO`; exact `LogicalSlotId` set |
| candidate/unique-key facts | yes when proved | no | logical/proof metadata | Ch20/35; never inferred from estimates |
| partitioning | not tracked | no | reserved extension | Ch37; cannot satisfy a requirement |
| rewindability | not tracked | no | reserved extension | Ch37; downstream algorithm precondition only where owned |
| materialization | not tracked | no | reserved extension/runtime mechanism | Ch24/26/37 |
| blocking/streaming | contract-specific | no | execution trait | Ch26/operator owner |
| spill capability | capability | no | runtime capability | Ch24/operator owner |
| estimated cardinality | no | no | estimate | Ch34/38; cost only |
| estimated memory | no | no | estimate | Ch24/38; cost only |
| `RequiredRowsObjective` | no semantic proof | no | search/cost metadata | §38.16; V22-J |
| `PhysicalTopN` exact K | yes | no | applicability requirement | §§22.4.1/30.7; `CR`/`MK` |

## V22-D — Capability and applicability

Drive physical enumeration with a controllable `CR`. For each conditional alternative,
test available-and-legal, available-but-inapplicable, unavailable, and falsely advertised
capability. Enumeration and `PV` must reject the latter three as appropriate. Give an
illegal alternative the cheapest possible cost; legality still wins. Disabling an
optional implementation must retain the architecturally required baseline. Capability,
cost, and search order may change plan shape/resource use and owner-admitted result
variation only. They cannot redefine SQL validity, result/order, mandatory-error,
or transaction contracts.

## V22-E — Execution context and runtime ownership

Create one `EC` per statement attempt with distinct transaction, effective snapshot,
`ReadEpochGuard`, query-memory manager, spill manager, cancellation state, pipeline-local
early-stop state, bound-occurrence subquery registry, profiling state, and query arena.
Instrument every physical node's runtime access: it must obtain these objects from the
active context rather than a stale plan copy. READ COMMITTED retry reuses the admitted
`CommandId` but receives a fresh attempt, snapshot, context, and runtime state; REPEATABLE
READ uses its transaction snapshot/current command boundary. Reuse V21-2 rather than
duplicating the transaction retry matrix.

Hold a barrier while an index-derived RID remains retained and prove the read epoch is
registered; release it only after the last possible dereference. Reuse the Chapter-14 and
Index Scan safety procedures. `PF` rejects physical-plan pointers, global/local state,
chunk/arena addresses, spill handles, and cancellation tokens from every catalog/storage/
WAL format. Crash/reopen therefore never depends on process-local plan/runtime bytes.

## V22-F — Scan, access-path, and MVCC composition

Reuse **Scan and Unary Operator Tests**, Chapter-14 RID-reuse tests, Chapter-17 comparator
tests, Chapter-36 access-path tests, and Chapter-37 property tests with this direct map:

| Path | Candidate source | Required checks | Order claim | Independent oracle |
|---|---|---|---|---|
| SeqScan | physical heap versions | exact snapshot visibility; one current logical owner occurrence | none unless separately established | `VM` + `BM` |
| IndexScan | index entries/RIDs | heap fetch, MVCC/current-owner recheck, stale rejection, residual predicate, read epoch | only exact advertised Ch37 property | `VM` + comparator + `PO` |
| index-only candidate | index bytes | reject without exact index-side visibility capability | N/A | `CR` + `PV` |

Include NULL, NaN, signed zero, arbitrary-byte VARCHAR, duplicate keys, deleted/superseded
versions, and non-exact range bounds. Every predicate not exactly proved by the access path
remains residual; index entry presence proves neither visibility nor predicate truth. RID
tie order never becomes SQL order.

## V22-G — Operator composition and substitutability

The following direct Chapter-22 checks reuse detailed downstream procedures:

- Filter: TRUE retains one occurrence; FALSE/UNKNOWN reject; schema, multiplicity,
  compatible order, expression demand, and provenance are preserved.
- Project: one output occurrence per input, declared fresh slots, duplicate outputs,
  demand/provenance, and exact order-key identity are checked by `SM`/`BM`.
- Join: all legal implementations for one logical INNER/LEFT/CROSS fixture are compared by
  `BM`/`SM`, including duplicates, UNKNOWN, LEFT null extension, errors, build/probe choice,
  input order, spill path, and hash seed.
- Aggregate/DISTINCT: hash and ordered/sort implementations, where legal, use an independent
  grouping-equivalence model for NULL, NaN, signed zero, arbitrary-byte VARCHAR, and
  composite keys; aggregate ordinal/finalization-error order is invariant. FLOAT64
  SUM/AVG results use the V29-H–K legal-tree oracle, not cross-shape bit identity.
- Sort/Top-N/Limit: full Sort plus Limit, an exact ordered provider plus Limit, and eligible
  Top-N use one comparator/tie/demand oracle and must belong to the same Chapter-20
  permitted ordered-result family. They preserve cardinality, strict comparator relations,
  demand, schema, slots, and result semantics; they need not select or order identical
  occurrences where LIMIT/OFFSET intersects a complete-key tie class.
- Scalar/EXISTS/IN subquery roles: reuse the §20.14 and pipeline procedures for lazy demand,
  cardinality error, three-valued NULL behavior, occurrence identity, and fallback.
- Materialization/rescan: retained values, multiplicity, slots, required order, VARCHAR
  ownership, snapshot, occurrence identity, and demanded errors remain invariant.
- DataChunk/vector details remain Chapter-23/25-owned. Vary width, selection layout, lane,
  and batch boundary to verify that representation does not become semantics.

## V22-H — DML, DDL, and statement roles

Reuse V21-4 through V21-26 and the DML/Control execution procedures. `DM` maps the physical
plan to finalized target RID, deduplication, Halloween protection, stale-target
revalidation, row images, retry boundary, and V21-9, V21-13/14, V21-17, and V21-20/21.

For one finalized multi-row DML attempt, independently permute scan/spool/RID/lane/batch/
hash/worker order. The V21-13 oracle selects the same `(SourceSpan.start, specificity,
semantic phase)` winner. V21-14 compares RETURNING as an unordered bag. CREATE INDEX build
input is compared to the transaction-local current logical-owner set after its gate, never
an RR query snapshot, committed-only view, or raw physical tuple enumeration. Validation
failure must precede any DML, catalog, storage, or WAL side effect.

## V22-I — Exact proof, estimates, cost, and plan shape

Deliberately make cardinality, selectivity, cost, and memory statistics inconsistent with
fixture data. Estimates and `RequiredRowsObjective` must not prove emptiness, uniqueness,
contradiction, scalar cardinality at most one, safe expression pruning, order, or exact row
count. Change estimates, cost ties, rule iteration, and legal capability sets; compare all
selected plans to `BM`/`SM`/the error and transaction oracle. Only legal plan preference,
shape, EXPLAIN physical detail, and resource use may differ. Reuse the **Statistics
Tests**, **Physical Property and Enforcement Tests**, **Optimizer Determinism and
Resource-Limit Tests**, **Physical-Plan Validator Tests**, **Final Optimizer Validation
Tests**, and **Optimizer Differential Correctness Tests**.

## V22-J — Exact first-K feasibility and row-goal metadata

`MK` uses an arbitrary-precision mathematical-integer domain independent of production
arithmetic. The logical Limit oracle always computes `after_offset = max(0, n - o)` and
then, when LIMIT exists,
`min(after_offset, l)`; it never computes `o + l`. Let `I = INT64_MAX`:

| LIMIT | OFFSET | Mathematical K for Top-N | Signed-INT64 K? | SQL valid? | Top-N treatment | Objective treatment | Public overflow? |
|---:|---:|---:|---:|---:|---|---|---:|
| 0 | 0 | 0 | yes | yes | eligible if other capabilities hold | exact zero | no |
| 0 | I | I | yes | yes | eligible if other capabilities hold | exact I | no |
| 1 | I | I+1 | no | yes | exact wider support or ineligible | exact wider or ALL_ROWS | no |
| I | 0 | I | yes | yes | eligible if other capabilities hold | exact I | no |
| I | 1 | I+1 | no | yes | exact wider support or ineligible | exact wider or ALL_ROWS | no |
| I | I | 2I | no | yes | exact wider support or ineligible | exact wider or ALL_ROWS | no |
| I-1 | 1 | I | yes | yes | eligible if other capabilities hold | exact I | no |
| I-1 | 2 | I+1 | no | yes | exact wider support or ineligible | exact wider or ALL_ROWS | no |
| 1 | I-1 | I | yes | yes | eligible if other capabilities hold | exact I | no |
| absent | I | N/A | N/A | yes | no finite Top-N first-K requirement | ALL_ROWS | no |

Use a mock Top-N domain ending at `I`: `LIMIT I OFFSET 1` makes Top-N ineligible and retains
an exact ordering provider plus PhysicalLimit, including PhysicalSort plus PhysicalLimit
when needed. Use a second mock domain reaching at least `2I`: exact wider Top-N is legal.
No concrete integer representation is required.

The saturation oracle is symbolic and does not materialize huge inputs. With `N=I`,
`M=1`, and child cardinality `I+1`, retaining only `I` then skipping one emits `I-1`
instead of `I`. With `N=I`, `M=I`, and child cardinality `2I`, retaining only `I` then
skipping `I` emits zero instead of `I`. Generic retained-K saturation therefore fails
unless a separate exact proof makes it harmless; the fixture assumes no hidden global
cardinality bound.

| V22-D case | SQL valid? | Top-N legal? | Fallback | Objective | Semantic proof/cap? | Validator |
|---|---:|---|---|---|---|---|
| representable exact K | yes | if all capabilities hold | retained | exact finite allowed | no | accept legal node |
| unrepresentable K in selected domain | yes | no | required | ALL_ROWS or exact wider | no | reject such node |
| wider exact K | yes | allowed | retained | exact wider allowed | no | accept if advertised |
| saturated retained K | yes | no without independent exact proof | required | unrelated | never | reject |
| exact row objective | yes | unaffected | retained | exact finite | cost/search only | unaffected |
| unrepresentable objective | yes | unaffected | retained | ALL_ROWS/no finite goal | no | unaffected |
| wider exact objective | yes | unaffected | retained | exact wider | cost/search only | unaffected |
| approximate saturated objective | yes | unaffected | retained | tagged approximate | no legality/proof/cap/stop | unaffected |
| illegally selected Top-N | yes | no | optimizer should have retained one | irrelevant | no | reject pre-execution |

Perturb exact/unbounded/approximate objective metadata while holding logical input fixed.
It may affect costing/shape only: it is never an executor cap, semantic proof,
early-termination authority, plan-legality authority, or query error. A demanded scalar
error paired with an unrepresentable K is still selected by ordinary demanded-evaluation
rules; physical bound infeasibility cannot preempt it. OutOfMemory, spill I/O,
cancellation, controlled optimizer resource exhaustion, and storage/runtime failures use
their existing procedures and remain distinct from exact-K representability.

## V22-K — Final physical-plan validation

Construct each malformed plan directly and derive the expected answer from `PS`, `SM`,
`PO`, `CR`, `MK`, and `PV`, not from the production validator. Cover child count, output
schema, required slots, expression types, hidden RID/system slots, stable descriptors,
property satisfaction, capability legality, and runtime-context requirements. In
particular, construct `PhysicalTopN` with mathematical K outside the selected
implementation's advertised exact domain. The validator must reject it as an internal
invalid physical plan before execution, with zero child opens, result rows, DML/catalog/
storage changes, WAL records, or other side effects; it is not a public LIMIT/OFFSET
overflow. Positive fixtures for every operator role prevent a reject-all validator from
passing.

Use this error-classification oracle for planning/execution boundary cases:

| Condition | Architectural class/owner | Required V22 result |
|---|---|---|
| upstream binding/type/logical semantic error | Chapters 19–21 and §39 | preserve category, provenance, and transaction consequence; do not reclassify physically |
| memory, spill I/O, cancellation, or storage/runtime failure | Chapters 24–26/31 and §39 | existing resource/runtime category remains possible |
| controlled optimizer work/memory exhaustion | §38.21 and §39 | existing optimizer-resource result; distinct from algorithm applicability |
| malformed or capability-incoherent selected physical plan | §§38.24/41.5 | internal validation failure before execution and side effects |
| exact K outside an optional Top-N implementation domain | §§22.4.1/30.7/38.16 | Top-N ineligible during search; no public LIMIT/OFFSET error |
| valid SQL executed by alternate legal plans | Chapters 20–22, §29.3 and §39 | each public outcome conforms to its owner; compare exact outcomes only where invariant, retaining admitted aggregate/tie variation |

## V22-L — Operator vocabulary, alternate plans, and determinism

The operator-role matrix is exhaustive for §22.4; names denote architectural roles, not
source classes:

| Operator/role | Logical owner | Detail owner | Availability | Child shape | Slots/order | Runtime/capability | Oracle/status |
|---|---|---|---|---|---|---|---|
| SeqScan | Ch20 Get | Ch27 | baseline | leaf | mapped source/no order | context + heap | `VM/SM` |
| IndexScan | Ch20 Get | Ch27/36/37 | conditional access path | leaf | mapped/exact advertised order only | index+heap+epoch | `VM/PO` |
| Values | Ch20 Values | Ch27 | baseline role | leaf | declared fresh | context | `PS/BM` |
| Filter | Ch20 Filter | Ch25/27 | baseline role | unary | pass-through/preserves exact compatible order | expression | `SM/BM` |
| Project | Ch20 Project | Ch25/27 | baseline role | unary | fresh declared slots/conditional order | expression | `SM` |
| NestedLoopJoin | Ch20 Join | Ch28 | baseline | binary | left then right/no incidental order | join kind | `BM/SM` |
| HashJoin | Ch20 Join | Ch28 | baseline | binary | left then right/no order | hash capability | `BM/SM` |
| IndexNestedLoopJoin | Ch20 Join | Ch28 | baseline | outer + parameterized lookup | left then right/exact claim only | index capability | `VM/BM` |
| MergeJoin | Ch20 Join | Ch28/37 | capability-conditional | binary | left then right/required input order | merge capability | `PO/BM` |
| HashAggregate | Ch20 Aggregate | Ch29 | baseline | unary | declared outputs/no order | hash capability | grouping oracle |
| SortAggregate | Ch20 Aggregate | Ch29/37 | capability-conditional | unary | declared outputs/contract order only | ordered capability | grouping/`PO` |
| Distinct | Ch20 Distinct | Ch29 | hash baseline; ordered conditional | unary | pass-through/contract order only | selected implementation | equivalence oracle |
| Sort | Ch20 Sort | Ch30/37 | baseline | unary | pass-through/establishes exact order | memory/spill | comparator/`PO` |
| TopN | Ch20 Sort+Limit | Ch30/38 | baseline role, instance-conditional | unary | pass-through/exact required order | exact K + sort capability | `MK/PO` |
| Limit | Ch20 Limit | Ch27 | baseline | unary | pass-through/preserves child order class | counts/context | Limit oracle |
| ScalarSubquery | §20.14 | Ch26/27/37 | baseline fallback | lazy side plan | typed scalar occurrence | bound occurrence registry | subquery oracle |
| ExistsSubquery | §20.14 | Ch26/27/37 | baseline fallback | lazy side plan | BOOLEAN occurrence | specialized demand | subquery oracle |
| InSubqueryBuild | §20.14 | Ch26/27/37 | baseline fallback | lazy side plan | BOOLEAN occurrence | set/null state | subquery oracle |
| Insert | Ch21 | Ch31 | statement role | source child | result/RETURNING + hidden target | attempt context | `DM` |
| Update | Ch21 | Ch31 | statement role | target child | result/RETURNING + hidden RID | attempt context | `DM` |
| Delete | Ch21 | Ch31 | statement role | target child | result/RETURNING + hidden RID | attempt context | `DM` |
| CreateTable | Ch21 | Ch31 | statement role | resolved statement | result contract | descriptor/attempt | `DM` |
| CreateIndex | Ch21 | Ch31 | statement role | current-owner build source | result contract | gates/descriptors | `DM` |
| Drop | Ch21 | Ch31 | statement role | resolved statement | result contract | descriptor/attempt | `DM` |
| Vacuum | Ch15/21 | Ch31 | maintenance role | resolved descriptor | result contract | transaction protocol | `DM` |
| Analyze | Ch20/21 | Ch31 | maintenance role | resolved descriptor | result contract | visibility protocol | `DM` |
| Explain | Ch20/40 | Ch31/40 | statement role | resolved plan | presentation-owned output | mode/context | §40 oracle |
| ResultSink | Ch21 | Ch26/31 | terminal role | unary/statement | consumes declared output | execution context | `BM/DM` |

| Slot-bearing role | Input IDs | Output IDs | Rule | Physical position/lineage |
|---|---|---|---|---|
| source | none | declared source IDs | mapped/fresh at logical boundary | position nonsemantic; descriptor lineage |
| Filter | child IDs | same IDs | pass-through | may reorder storage only, not identity |
| Project | referenced IDs | declared fresh IDs | fresh, duplicates retained | explicit map/provenance |
| join | left/right IDs | left then right IDs | concatenate distinct identities | explicit side lineage |
| aggregate | input/group IDs | declared group/aggregate IDs | fresh outputs | aggregate ordinal retained |
| DISTINCT | child IDs | same IDs | pass-through | multiplicity changes only |
| Sort | child IDs | same IDs | pass-through | order property uses IDs |
| Limit | child IDs | same IDs | pass-through | position nonsemantic |
| derived table | inner IDs | outer declared IDs | explicit remap | boundary lineage retained |
| DML hidden RID | target/internal IDs | internal until consumed | pass-through/internal | never user identity |
| result sink | child IDs | result metadata | consumes declared order | display ordinal not semantic identity |

The `no` entries below forbid variation beyond the canonical semantic contract;
they retain the owner-admitted aggregate and complete-key-tie families described
above. Evaluate downstream results against the selected legal semantic outcome.

| Perturbation | Plan shape may change? | Extra bag/order/error/state/RETURNING variation? | Required comparison |
|---|---:|---:|---|
| hash seed | yes | no | bag/order/error/DM oracle |
| pointer address | no semantic constraint | no | canonical IDs/`SM` |
| allocation order | yes | no | plan digest + semantic oracle |
| RID order | yes | no | bag; required order separately |
| PageId order | yes | no | bag; required order separately |
| vector lane | no semantic constraint | no | result/error/DM |
| batch size | no semantic constraint | no | result/error/DM |
| worker schedule | yes | no | barriers + result/error/DM |
| spill partition | yes | no | result/order/error |
| cost tie | yes if permitted | no | selected legal plans vs oracle |
| capability toggle | yes | no | fallback/equivalence |
| Top-N exact-domain limit | yes | no | `MK/CR/PV` |

## V22-M — Cross-chapter reuse and obligation map

| Boundary | Contract verified here | Reused methodology |
|---|---|---|
| Ch17→22 | typed values/comparator/executable errors | scalar/FLOAT/VARCHAR/expression tests |
| Ch19→22 | BindingId, aggregate ordinal, provenance, normalized counts | V19/V20 handoff tests |
| Ch20→22 | immutable logical plans, slots, bags, demand, Limit/subqueries | V20-1 through V20-25 |
| Ch21→22 | attempts and DML/DDL operation contracts | V21-1 through V21-30 |
| Ch22→23 | canonical chunk abstraction, representation nonsemantic | Vector Correctness Tests |
| Ch22→24 | query memory, owned temporary rows, spill | pipeline/resource and memory/spill tests |
| Ch22→25 | executable vector expressions | Expression Execution Tests |
| Ch22→26 | pipelines, early stop, finalization | Pipeline Finalization and Resource Tests |
| Ch22→27 | scans, unary operators, Limit, values | Scan and Unary Operator Tests |
| Ch22→28 | join algorithms/substitutability | join tests and differential tests |
| Ch22→29 | aggregate/DISTINCT equivalence | aggregation/distinct tests |
| Ch22→30 | Sort/Top-N and exact K | Sort Tests + V22-J |
| Ch22→31 | DML/DDL/control physical execution | DML/Control tests + V21 |
| Ch22→37 | canonical property taxonomy/satisfaction | Physical Property and Enforcement Tests + `PO` |
| Ch22→38 | capability, search/cost, final validation | optimizer/final-plan tests + V22-D/J/K |
| Ch22→39 | public/resource/internal error classes | error classification and injection tests |
| Ch22→40 | EXPLAIN physical metadata/presentation | EXPLAIN tests |

The ledger abbreviates the procedures above as `A` through `M`; oracle abbreviations are
defined at the start of this family. Every row is independently falsifiable at the stated
granularity.

| ID | Architecture | Atomic obligation | Verification | Independent oracle | Reuse |
|---|---|---|---|---|---|
| A01 | §22.1 | production execution is vectorized | E/G | EC + result oracle | vector tests |
| A02 | §22.1 | production execution is chunk-at-a-time | E/G | EC + batch perturbation | vector tests |
| A03 | §22.1 | execution is pipeline-oriented | G | pipeline event model | pipeline tests |
| A04 | §22.1 | execution is memory-budgeted | E/G | reservation ledger | memory tests |
| A05 | §22.1 | execution supports spill contracts | E/G | spill event/ownership model | spill tests |
| A06 | §22.1 | execution is parallel-ready without semantic schedule dependence | E/L | EC + semantic oracle | parallel tests |
| A07 | §22.1 | execution consumes resolved physical plans | A | PS | V20 |
| A08 | §22.1 | heap access composes with transaction semantics | F | VM | scan/V21 |
| A09 | §22.1 | B+ access composes with index semantics | F | VM | index tests |
| A10 | §22.1 | catalog access uses immutable resolved descriptors | A/H | PS + descriptor table | V16/V21 |
| A11 | §22.1 | query memory is context-owned | E | OG/EC | memory tests |
| A12 | §22.1 | spill manager is context-owned | E | OG/EC | spill tests |
| A13 | §22.1 | execution does not parse SQL | A | poisoned entry point | front-end tests |
| A14 | §22.1 | execution does not resolve SQL names | A | poisoned entry point | V19/V20 |
| A15 | §22.1 | hot loops do not perform catalog-name lookup | A | descriptor/name-lookup trace | V16/V21 |
| A16 | §22.1 | execution does not perform cost-based search | A/I | phase event trace | optimizer tests |
| B01 | §22.2 | PhysicalOperator is immutable plan description | A | digest + OG | — |
| B02 | §22.2 | GlobalOperatorState is mutable per execution | A/E | OG/EC | pipeline tests |
| B03 | §22.2 | GlobalOperatorState is worker-shared only within that execution | A/E | OG/EC | parallel tests |
| B04 | §22.2 | LocalOperatorState belongs to one worker/task | A/E | OG/EC | parallel tests |
| B05 | §22.2 | cached/reused plan remains immutable and valid | A | digest + PS | final validation |
| B06 | §22.2 | runtime state is newly created per execution | A/E | poison/reset tracker | pipeline tests |
| B07 | §22.2 | transaction is not stored in plan nodes | A/E | OG | V21 |
| B08 | §22.2 | snapshot is not stored in plan nodes | A/E | OG/EC | V21 |
| B09 | §22.2 | read-epoch registration is not stored in plan nodes | A/E | OG/EC | RID tests |
| B10 | §22.2 | cancellation state is not stored in plan nodes | A/E | OG/EC | resource tests |
| B11 | §22.2 | memory usage is not stored in plan nodes | A/E | OG/EC | memory tests |
| B12 | §22.2 | cursors/other mutable execution state are not stored in plan nodes | A/E | OG/EC | pipeline tests |
| C01 | §22.3 | every node identifies an operator kind | A/K | PS | validator tests |
| C02 | §22.3 | every node has valid child relationships | A/K | PS | validator tests |
| C03 | §22.3 | every node declares output schema | B/K | PS/SM | validator tests |
| C04 | §22.3 | output schema carries LogicalSlotIds | B/K | SM | V20/validator |
| C05 | §22.3 | every node declares required child inputs | A/K | PS/SM | validator tests |
| C06 | §22.3 | property metadata follows Chapter 37 | C/K | PO | property tests |
| C07 | §22.3 | estimates are metadata, not proof | I | estimate perturbation | optimizer tests |
| C08 | §22.3 | EXPLAIN metadata follows §40 | L/M | §40 model | EXPLAIN tests |
| C09 | §22.3 | node identifies a selected execution algorithm/role | A/L | PS/CR | validator tests |
| C10 | §22.3 | execution state/methods are not SQL AST behavior | A | phase/ownership trace | V18/V20 |
| C11 | §22.3 | physical execution does not bind again | A | poisoned binder | V19/V20 |
| D01 | §22.4 | PhysicalSeqScan role is represented | L | operator matrix/PS | scan tests |
| D02 | §22.4 | PhysicalIndexScan role is represented | L | operator matrix/PS | scan tests |
| D03 | §22.4 | PhysicalValues role is represented | L | operator matrix/PS | unary tests |
| D04 | §22.4 | PhysicalFilter role is represented | L | operator matrix/PS | unary tests |
| D05 | §22.4 | PhysicalProject role is represented | L | operator matrix/PS | unary tests |
| D06 | §22.4 | PhysicalNestedLoopJoin role is represented | L | operator matrix/PS | join tests |
| D07 | §22.4 | PhysicalHashJoin role is represented | L | operator matrix/PS | join tests |
| D08 | §22.4 | PhysicalIndexNestedLoopJoin role is represented | L | operator matrix/PS | join tests |
| D09 | §22.4 | PhysicalMergeJoin role is represented | L | operator matrix/PS | join tests |
| D10 | §22.4 | PhysicalHashAggregate role is represented | L | operator matrix/PS | aggregate tests |
| D11 | §22.4 | PhysicalSortAggregate role is represented | L | operator matrix/PS | aggregate tests |
| D12 | §22.4 | PhysicalDistinct role is represented | L | operator matrix/PS | distinct tests |
| D13 | §22.4 | PhysicalSort role is represented | L | operator matrix/PS | sort tests |
| D14 | §22.4 | PhysicalTopN role is represented | J/L | operator matrix/MK | sort tests |
| D15 | §22.4 | PhysicalLimit role is represented | L | operator matrix/Limit oracle | unary tests |
| D16 | §22.4 | PhysicalScalarSubquery role is represented | G/L | operator matrix/subquery oracle | V20/pipeline |
| D17 | §22.4 | PhysicalExistsSubquery role is represented | G/L | operator matrix/subquery oracle | V20/pipeline |
| D18 | §22.4 | PhysicalInSubqueryBuild role is represented | G/L | operator matrix/subquery oracle | V20/pipeline |
| D19 | §22.4 | PhysicalInsert role is represented | H/L | operator matrix/DM | V21/DML |
| D20 | §22.4 | PhysicalUpdate role is represented | H/L | operator matrix/DM | V21/DML |
| D21 | §22.4 | PhysicalDelete role is represented | H/L | operator matrix/DM | V21/DML |
| D22 | §22.4 | PhysicalCreateTable role is represented | H/L | operator matrix/DM | V21/control |
| D23 | §22.4 | PhysicalCreateIndex role is represented | H/L | operator matrix/DM | V21/control |
| D24 | §22.4 | PhysicalDrop role is represented | H/L | operator matrix/DM | V21/control |
| D25 | §22.4 | PhysicalVacuum role is represented | H/L | operator matrix/DM | maintenance tests |
| D26 | §22.4 | PhysicalAnalyze role is represented | H/L | operator matrix/DM | V21/control |
| D27 | §22.4 | PhysicalExplain role is represented | L | operator matrix/§40 model | EXPLAIN tests |
| D28 | §22.4 | PhysicalResultSink role is represented | L | operator matrix/BM | pipeline tests |
| D29 | §22.4 | each vocabulary entry is a distinct algorithm or statement role | L | role matrix + positive fixtures | downstream families |
| D30 | §22.4 | three subquery roles provide mandatory lazy fallbacks | G/L | subquery mode/fallback model | V20/pipeline |
| D31 | §22.4 | subquery fallback roles are not correlated/parameterized operators | G/L | bound-mode model | V20 |
| E01 | §22.4.1 | capability registry is explicit | D | CR | optimizer tests |
| E02 | §22.4.1 | only available validated implementations are enumerated | D | CR/PV | optimizer tests |
| E03 | §22.4.1 | eligibility is plan-instance-specific | D | CR fixture matrix | optimizer tests |
| E04 | §22.4.1 | every algorithm-specific exact requirement must be representable | D/J | CR/MK | optimizer tests |
| E05 | §22.4.1 | every algorithm-specific exact requirement must be satisfiable | D | CR/PV | optimizer tests |
| E06 | §22.4.1 | Top-N requires exact mathematical K support | J | MK/CR | sort/optimizer |
| E07 | §22.4.1 | unsupported exact K makes Top-N ineligible | J | MK/CR | optimizer tests |
| E08 | §22.4.1 | exact-K infeasibility adds no SQL count restriction | J | Limit oracle/MK | V20-11 |
| E09 | §22.4.1 | exact-K infeasibility adds no public arithmetic error | J/K | error oracle | §39 tests |
| E10 | §22.4.1 | capability may change plan shape | D/L | alternate-plan set | optimizer tests |
| E11 | §22.4.1 | capability may change resource use/performance | D/L | resource/cost model | optimizer tests |
| E12 | §22.4.1 | capability cannot change logical validity | D/J | semantic oracle | differential tests |
| E13 | §22.4.1 | capability cannot change result/order | D/L | BM/comparator | differential tests |
| E14 | §22.4.1 | capability cannot change mandatory errors | D/L | error oracle | differential tests |
| E15 | §22.4.1 | capability cannot change transaction state | D/H/L | DM | V21 |
| E16 | §22.4.1 | search retains a conforming alternative | D/J | CR + baseline map | optimizer tests |
| E17 | §22.4.1 | cost chooses only among legal alternatives | D/I | PV + adversarial cost | optimizer tests |
| E18 | §22.4.1 | listed baseline implementations remain represented | L | operator/capability matrix | downstream families |
| E19 | §22.4.1 | subquery baseline may use capable child algorithms | G/L | subquery plan model | V20/optimizer |
| E20 | §22.4.1 | subquery fallback cannot depend on semi/anti rewrite | G/L | fallback capability toggle | V20/optimizer |
| E21 | §22.4.1 | MergeJoin/SortAggregate/ordered DISTINCT are capability-conditional | D/L | CR/operator matrix | downstream families |
| E22 | §22.4.1 | conditional algorithms are costed only when enabled | D/I | CR + search trace | optimizer tests |
| E23 | §22.4.1 | cost formula alone cannot place unavailable algorithm in final plan | D/K | CR/PV | validator tests |
| F01 | §22.5 | each execution owns one QueryExecutionContext | E | EC | pipeline tests |
| F02 | §22.5 | context references the transaction | E | EC/OG | V21 |
| F03 | §22.5 | context carries effective snapshot | E | EC | MVCC tests |
| F04 | §22.5 | context owns ReadEpochGuard | E/F | EC + epoch trace | RID tests |
| F05 | §22.5 | context owns QueryMemoryManager | E | EC/OG | memory tests |
| F06 | §22.5 | context owns SpillManager | E | EC/OG | spill tests |
| F07 | §22.5 | context owns cancellation state | E | EC/OG | resource tests |
| F08 | §22.5 | context owns pipeline-local early-stop state | E/G | EC/OG | pipeline tests |
| F09 | §22.5 | context owns lazy subquery state keyed by bound occurrence | E/G | EC + occurrence table | V20/pipeline |
| F10 | §22.5 | context owns profiling state | E | EC/OG | EXPLAIN/analyze tests |
| F11 | §22.5 | context owns query-scoped arena | E | EC/OG | memory tests |
| F12 | §22.5 | epoch covers retained/dereferenced index RIDs | E/F | epoch barrier oracle | RID tests |
| F13 | §22.5 | RC uses statement snapshot | E | snapshot oracle | V21 |
| F14 | §22.5 | RR uses transaction snapshot/current command boundary | E | snapshot/CommandId oracle | V21 |
| F15 | §22.5 | context does not weaken transaction-owned snapshot/lock lifetimes | E/H | lock/snapshot trace | Ch9–15/V21 |
| G01 | §22.6 | plan owns resolved IDs | A | PS/OG | V20 |
| G02 | §22.6 | plan owns resolved types | A | PS/OG | V19/V20 |
| G03 | §22.6 | plan owns physical expressions | A/G | PS/OG + provenance | expression tests |
| G04 | §22.6 | plan owns immutable bounds | A/J | PS/OG + MK | V20-11 |
| G05 | §22.6 | plan owns operator property metadata | C | PO/OG | property tests |
| G06 | §22.6 | runtime owns source cursors | A/E | OG/EC | scan tests |
| G07 | §22.6 | runtime owns reusable DataChunks | A/E | OG/EC | vector tests |
| G08 | §22.6 | runtime owns expression scratch | A/E | OG/EC | expression tests |
| G09 | §22.6 | runtime owns row collections | A/E | OG/EC | aggregate/sort tests |
| G10 | §22.6 | runtime owns memory reservations | A/E | OG/EC | memory tests |
| G11 | §22.6 | runtime owns spill runs | A/E | OG/EC | spill tests |
| G12 | §22.6 | runtime owns counters | A/E | OG/EC | pipeline tests |
| G13 | §22.6 | runtime owns continuation state | A/E | OG/EC | pipeline tests |
| G14 | §22.6 | runtime owns subquery state and payload | A/E/G | OG/EC | subquery tests |
| G15 | §22.6 | runtime pointers never enter persistent/catalog/storage formats | E | PF | format tests |
| H01 | §22.7 | property metadata is explicit | C | PO/PS | property tests |
| H02 | §22.7 | consumers do not infer required properties from operator names | C | negative PO fixture | property tests |
| H03 | §22.7 | consumers do not infer provided properties from incidental output | C | order perturbation | property tests |
| H04 | §22.7 | Chapter 37 is canonical property owner | C/M | owner/property matrix | property tests |
| H05 | §22.7 | tracked set contains OrderingProperty | C | PO taxonomy | property tests |
| H06 | §22.7 | tracked set contains RequiredSlotSet | C | PO taxonomy | property tests |
| H07 | §22.7 | no third tracked physical property is admitted | C | PO negative taxonomy | property tests |
| H08 | §22.7 | OrderingProperty represents exact required/provided row order | C | PO | property tests |
| H09 | §22.7 | RequiredSlotSet represents exact needed LogicalSlotIds | B/C | SM/PO | property tests |
| H10 | §22.7 | candidate/unique facts remain exact logical/proof metadata | C/I | proof-origin tags | stats tests |
| H11 | §22.7 | partitioning is a reserved extension, not tracked property | C | PO taxonomy | property tests |
| H12 | §22.7 | rewindability is a reserved extension, not tracked property | C | PO taxonomy | property tests |
| H13 | §22.7 | materialization is a reserved extension, not tracked property | C | PO taxonomy | property tests |
| H14 | §22.7 | blocking/streaming are execution traits, not properties | C/G | event model + PO | pipeline/property |
| H15 | §22.7 | spill support is capability/runtime trait, not property | C/D | CR + PO | spill/property |
| H16 | §22.7 | estimated cardinality/memory are estimates, not proof/property | C/I | estimate-origin tags | stats tests |
| H17 | §22.7 | RequiredRowsObjective is cost/search metadata, not property | C/I/J | objective tags + PO | optimizer tests |
| H18 | §22.7 | Top-N exact K is applicability/capability, not property | C/D/J | CR/MK/PO | optimizer tests |
| H19 | §22.7 | advertised order must be execution-contract-guaranteed | C/F/G | comparator/PO | operator tests |
| H20 | §22.7 | incidental strategy output cannot establish provided order | C/L | order perturbation/PO | property tests |
| I01 | §22.8 | vectorized/chunk execution invariant is directly checked | G | batch perturbation | vector tests |
| I02 | §22.8 | plan immutability after publication is directly checked | A | digest/OG | — |
| I03 | §22.8 | per-execution state separation is directly checked | A/E | poison/reset/OG | pipeline tests |
| I04 | §22.8 | global/local worker-state distinction is directly checked | A/E | OG + barriers | parallel tests |
| I05 | §22.8 | resolved IDs/types/slots and no name resolution are directly checked | A/B | PS/SM + poison | V19/V20 |
| I06 | §22.8 | transaction/snapshot/epoch context ownership is directly checked | E | EC/OG | V21/RID tests |
| I07 | §22.8 | operators represent resolved algorithm/statement roles | A/L | PS/operator matrix | downstream families |
| I08 | §22.8 | Vacuum/Analyze consume descriptors and preserve owner protocols | H/L | descriptor table/DM | V21/maintenance |
| I09 | §22.8 | explicit property metadata/no string inference is directly checked | C | PO negative fixtures | property tests |
| I10 | §22.8 | runtime state is query/process-local and nonpersistent | E | OG/PF | format tests |

Non-falsifiable Chapter-22 material is accounted for separately:

| ID | Architecture | Material | Classification and justification | Status |
|---|---|---|---|---|
| N01 | §22.1 | batching amortizes per-row overhead | analytical performance rationale; no correctness predicate or fixed performance threshold | N/A |
| N02 | §22.3 | optimizer chapters own detailed selection rules | canonical navigation; concrete capability/search rules are covered by D/I/K and their owner families | N/A |
| N03 | §22.4 | Chapters 28–31 own detailed algorithms | canonical navigation; each role is mapped to its complete downstream procedure above | N/A |
