# 36. Cost Model and Base Access Paths

## 36.1 Cost philosophy

The architecture uses calibrated abstract cost units rather than pretending to predict exact milliseconds.

The model separates at least:

```text
sequential persistent I/O
random persistent I/O
CPU tuple/vector work
expression/operator work
hash work
comparison work
memory pressure
temporary spill I/O
```

A scalar comparison cost is a weighted sum using centrally configured/calibrated weights.

Every workload component retains its natural unit until the matching §36.3
coefficient converts it to normalized cost units. Row, page, operation, and byte
quantities are not added directly to the scalar cost.

The component vector remains available for EXPLAIN/diagnostics.

## 36.2 Cost structure

Represent cost conceptually as:

```text
Cost {
    startup_cost
    run_cost
    total_cost

    seq_page_reads
    random_page_reads
    temp_page_reads
    temp_page_writes

    cpu_rows
    cpu_expressions
    hash_ops
    compare_ops

    estimated_peak_memory
    estimated_spill_bytes
}
```

`startup_cost` is scalar work required before the first output can become available. `run_cost` is scalar remaining work to exhaust the operator. `total_cost = startup_cost + run_cost` for a complete-consumption objective unless a later LIMIT/property rule deliberately evaluates partial-consumption cost.

Component counters are additive where semantically appropriate; peak memory is combined according to operator lifetime/dependency rather than blindly summed.

### 36.2.1 Finite cost arithmetic

The scalar `Cost` domain is finite and nonnegative. The selected scalar
representation has one positive `MAX_FINITE_COST`, fixed for the optimizer build
and configuration and used consistently for every alternative in one planning
invocation. It is distinct from Chapter 35's cardinality bound.

All cost additions, multiplications, conversions from exact integer counters,
and `startup_cost + run_cost` use checked arithmetic. A mathematically valid
result above `MAX_FINITE_COST` saturates to `MAX_FINITE_COST`; no overflowing
integer or nonfinite floating intermediate is evaluated first. Subtraction is
clamped only where an owning formula explicitly defines a nonnegative residual.
Zero workload contributes zero cost.

The inspectable workload counters are likewise finite and nonnegative. Checked
counter composition saturates at the counter representation's finite maximum
before scalar conversion and marks the component as saturated; it never wraps.

An invalid raw configuration value or an internally generated negative, NaN, or
infinite workload/cost is not saturation. It is rejected before cost comparison
using Chapter 39's existing `OptimizerError`/internal-invariant ownership. A
valid-input saturation is ordinary approximate planning metadata, is exposed in
optimizer diagnostics, and is neither `OptimizerResourceLimit` nor semantic
proof.

`startup_cost` and `run_cost` are finalized independently into this domain, and: `total_cost = saturating_add(startup_cost, run_cost)`.

Consequently all three values are finite and nonnegative and
`startup_cost <= total_cost`. Chapter 38 remains the owner of the active scalar
objective, partial-run fraction, dominance, and tie comparison.

### 36.2.2 Local work and child composition

Every modeled unit of work has exactly one physical owner. A normal physical
node includes each child objective cost exactly once and adds only work performed
by that node. If an expression is fused or pushed into a scan, that scan owns its
evaluation cost and no removed Filter/Project charges it again. Operator-specific
Chapter-38 formulas override this generic transfer wherever they define rescans,
materialization, blocking build work, property enforcement, spill passes, or
other repeated work.

For an ordinary streaming unary node, compose:

```text
startup = child startup
        + fixed local setup
        + bounded local work for the first input/output attempt

run     = child run under the active propagated objective
        + remaining local work

total   = saturating_add(startup, run)
```

The first-attempt term is at most the node's full local-work estimate. It is the
minimum modeled input/cursor attempt after which output could become available,
not an assertion that the attempt produces a row. Selectivity-based expected
partial consumption remains governed by §38.16. An estimated-zero input without
proof may make estimated per-row work zero, but does not remove fixed access
setup, the executable child, or demanded expressions.

The mandatory streaming/source transfers are:

| Physical form | Child and local-work owner | Startup/run convention |
|---|---|---|
| `PhysicalValues` | No child. The node owns each demanded row-expression evaluation for each declared occurrence. | Initialization and the bounded first declared occurrence are startup; remaining declared occurrences are run. |
| `PhysicalSeqScan` | No child. The scan owns heap-page access, tuple-version/visibility inspection, pushed predicates, and scan-owned decode/materialization. | Cursor/source setup and the bounded first page/tuple attempt are startup; remaining scan work is run. |
| `PhysicalIndexScan` | No child. The scan owns descent, leaf search/traversal, candidate RID work, heap fetch/visibility, scan residuals, and scan-owned decode/materialization. | Cursor setup and root-to-first-leaf/first-candidate attempt are startup; remaining range/candidate work is run. |
| `PhysicalFilter` | Includes its child objective once and owns only predicates still placed at the Filter. Predicates are charged per input occurrence presented to it, not per surviving output row. | Child startup, Filter setup, and one bounded predicate attempt are startup; child run and remaining predicate attempts are run. |
| `PhysicalProject` | Includes its child objective once and owns only projection expressions/materialization still placed at the Project. Work is charged per input/output occurrence processed by the one-to-one Project. | Child startup, Project setup, and one bounded projection attempt are startup; child run and remaining projection work are run. |
| `PhysicalLimit` | Includes its child under the exact objective safely propagated by §§20.12 and 38.16. It owns execution-start count-expression acquisition/validation exactly once and local skip/emit bookkeeping. | Count acquisition/validation and setup are startup; child/objective consumption and per-row bookkeeping are run. |
| Fused scan/filter/project | The fused node owns the union of the work it actually performs, once, using the same row domains as the unfused owners. | Use the source convention; fusion changes ownership and overhead, not demand, errors, or proof. |

This table does not define a generic streaming-binary formula. Join and other
binary algorithms use their actual Chapter-38 formulas and orientation/property
requirements.

## 36.3 Cost units

One mandatory `CostConfig` is validated before physical alternative costing and
is then immutable for the optimization invocation. Its closed inventory is:

| Field | Meaning and unit | Valid domain / zero policy | Consumers |
|---|---|---|---|
| `seq_page_cost` | normalized cost per sequential persistent page read | finite `> 0`; zero invalid | SeqScan and sequential/local index heap work |
| `random_page_cost` | normalized cost per random persistent page read | finite `> 0`; zero invalid | B+ descent/leaf and random heap fetches |
| `cpu_tuple_cost` | normalized cost per examined tuple/version | finite `> 0`; zero invalid | visibility/header and tuple/vector work |
| `cpu_operator_cost` | normalized cost per generic expression/operator operation | finite `> 0`; zero invalid | predicates, projection, decode/materialization, setup/bookkeeping |
| `hash_cost` | normalized cost per hash/build/probe/update operation | finite `> 0`; zero invalid | Chapter-38 hash operators |
| `comparison_cost` | normalized cost per scalar/key comparison operation | finite `> 0`; zero invalid | B+ search, Sort/Top-N, ordered algorithms |
| `temp_page_cost` | normalized cost per temporary page read or write | finite `> 0`; zero invalid | Chapter-38 spill/materialization I/O |
| `effective_cache_pages` | assumed cache capacity in database pages; not a cost coefficient | unsigned integer `>= 0`; zero valid | stable cache-likelihood model |
| `unknown_physical_heap_pages` | fallback physical relation size in heap pages | unsigned integer `>= 1`; zero invalid | missing heap-size costing |
| `unknown_dead_version_count` | fallback expected dead/obsolete tuple versions | finite `>= 0`; zero valid | missing dead-pressure costing |
| `unknown_index_minimum_physical_entries` | fallback physical-entry floor when no compatible index-pressure statistics exist | unsigned integer `>= 1`; zero invalid | missing index-pressure costing |
| `unknown_index_entry_pressure_multiplier` | fallback physical entries per estimated logical live entry | finite `>= 1`; zero invalid | missing index-pressure costing |
| `unknown_index_entries_per_leaf` | fallback expected physical entries per leaf page | finite `> 0`; zero invalid | missing leaf/occupancy costing |
| `unknown_heap_rows_per_page` | fallback expected physical tuple versions per heap page | finite `> 0`; zero invalid | distinct-heap-page occupancy fallback |
| `unknown_index_heap_correlation` | fallback leading-key/heap-rank correlation | finite `[-1,1]`; zero valid | missing index/heap locality costing |

Every mandatory field must be present. Missing or invalid configuration is a
controlled optimizer-configuration failure under Chapter 39's existing
`OptimizerError` ownership before any affected alternative reaches comparison;
there is no language default, silent replacement, or new public error category.

The first seven fields are conversion coefficients. A workload count is
multiplied exactly once by its matching cost-per-unit coefficient. Memory bytes,
row widths, selectivities, and estimated spill bytes remain quantities in their
own units until an owning algorithm derives operation or page counts; they are
never added directly to scalar cost. A Chapter-38 formula that already names a
normalized local cost consumes it directly rather than applying the coefficient
again.

The baseline scalar conversion is therefore the checked saturating sum of:

```text
seq_page_reads                  * seq_page_cost
random_page_reads               * random_page_cost
(temp_page_reads + writes)      * temp_page_cost
cpu_rows                        * cpu_tuple_cost
cpu_expressions                 * cpu_operator_cost
hash_ops                        * hash_cost
compare_ops                     * comparison_cost
```

Operator-specific formulas decide the workload counters; this conversion does
not re-estimate cardinality or add memory bytes/selectivity directly.

Chapter-38 search tolerance, planning/execution memory budgets, hash load factor,
and spill partition/fanout settings remain their owners' configuration. They are
not additional `CostConfig` conversion fields. A capability-specific extension
may define additional validated parameters only while that capability is enabled.

The architecture does not permanently lock textbook numeric constants. Defaults are shipped as configuration and may be calibrated against the engine/hardware.

## 36.4 Calibration

A calibration tool measures at least:

```text
cached sequential scan throughput
cold-ish sequential page reads
random page reads
integer predicate throughput
VARCHAR comparison throughput
hash throughput
sort comparison throughput
temporary spill read/write throughput
```

It proposes relative cost weights. Calibration results are deployment/configuration data, not persistent database format.

## 36.5 Cache model

The architecture uses a configured `effective_cache_pages` together with relation/index size to estimate likely caching.

The optimizer MUST NOT read exact momentary BufferPool contents during ordinary planning. Upper B+ levels may be modeled as more likely cached than heap leaf/data pages. The cache model should be stable enough that plans do not oscillate merely because a particular page happened to be resident during optimization.

Cache fractions and page reductions use checked finite arithmetic and explicit
zero-size handling. A known zero-page object contributes no page reads; an
unknown size uses §36.5.1's positive fallback. `effective_cache_pages == 0`
means no assumed cache capacity and never becomes a division denominator.

### 36.5.1 Physical-cost input precedence and fallback

Physical work inputs are selected independently for each retained table/index
identity in this order:

1. use an authoritative physical fact from the invocation's retained immutable
   descriptor/metadata snapshot when that fact owns the required quantity;
2. otherwise use the applicable complete compatible Chapter-34 generation,
   including a canonically selected older valid generation;
3. otherwise derive the quantity from already selected stable descriptor,
   Chapter-35 estimate, and other available inputs where §§36.6–36.10 define a
   derivation;
4. otherwise use the matching validated `CostConfig` physical-work assumption.

An authoritative current B+ height and an advisory entry-pressure estimate are
different quantities and may both be used. No step mixes members from statistics
generations, refreshes a descriptor during planning, consults momentary
BufferPool residency, or requires synchronous ANALYZE.

Descriptor-derived facts identify their retained object/descriptor. Valid
statistics retain their actual `StatsVersion` and `STALE_STATISTICS` provenance
where applicable. A configured physical fallback caused by absent, rejected, or
incompatible advisory statistics carries LOW confidence,
`MISSING_STATISTICS`, and a diagnostic naming the configured assumption. This
does not require another persisted/public provenance enum and remains distinct
from a Chapter-35 logical-estimate fallback.

The complete physical-input map is:

| Quantity | Preferred selected source | Missing-input derivation/fallback |
|---|---|---|
| logical live/base rows | compatible TABLE generation through Chapter 35 | Chapter-35 base-row fallback |
| physical heap pages | retained authoritative physical metadata snapshot when the planner input owns one, otherwise compatible TABLE statistics | `unknown_physical_heap_pages` |
| dead versions | compatible TABLE statistics | `unknown_dead_version_count` |
| physical tuple versions | selected live rows plus selected dead versions | checked finite sum |
| B+ tree height | retained immutable Chapter-8 index metadata | no statistical substitute; an index lacking required valid structural metadata is not a usable index |
| logical live index entries | compatible INDEX statistics | selected base-row estimate for the non-partial index |
| physical index entries | compatible INDEX statistics | §36.7 pressure derivation |
| invisible index entries | compatible INDEX statistics | nonnegative physical-minus-logical derivation |
| leaf pages / entries per leaf | compatible INDEX statistics | §36.7 derivation using `unknown_index_entries_per_leaf` |
| index/heap correlation | compatible INDEX statistics | `unknown_index_heap_correlation` |
| physical heap rows per page | derived from selected physical tuple versions/pages when both define a positive density | `unknown_heap_rows_per_page` |

## 36.6 Sequential scan cost

For a table:

```text
physical_tuple_versions
≈ analyzed_live_row_count + dead_version_estimate
```

as a planning approximation.

The live-row term is the selected Chapter-35 base-row estimate. When no usable
physical heap-page fact exists, use `unknown_physical_heap_pages`; when no usable
dead-version estimate exists, use `unknown_dead_version_count`. The tuple-version
sum uses §36.2.1 finite arithmetic. A stable authoritative physical page count of
zero may describe a physically empty heap; a configured fallback is never zero
and neither form turns a logical row estimate of zero into semantic proof.

SeqScan components include:

```text
I/O:
    physical_heap_pages sequential reads

CPU:
    physical_tuple_versions * visibility/header inspection
    + live_rows * pushed-predicate input decode/evaluation work
    + output_rows * output-only decode/materialization work
```

Dead-version pressure therefore raises scan CPU even when logical cardinality is unchanged. Required-column width/decode cost is applied separately from physical page I/O.

## 36.7 B+ point-lookup cost

Approximate one point lookup as:

```text
tree descent
+
leaf binary/local search
+
candidate RID heap fetches
+
MVCC visibility
+
required tuple decode
```

Tree descent considers:

```text
tree_height
upper-level cache likelihood
key comparison cost
```

Not every upper level is automatically charged as a cold random read. Even a unique index lookup still visits the heap because index entries do not prove MVCC visibility and there is no index-only visibility path. A SQL UNIQUE index may still contain multiple physical entries for one user key because aborted/obsolete tuple versions are cleaned asynchronously by vacuum.

Point-lookup costing therefore may inflate expected candidate-RID/MVCC work using the §34.6 physical-versus-live entry pressure when no key-specific garbage statistic exists.

The current B+ `tree_height` comes from the retained immutable Chapter-8
descriptor/metadata snapshot, not advisory statistics. When compatible
`IndexStatistics` are unavailable, derive:

```text
logical_live_entry_count = selected base-row estimate

physical_entry_count
    ≈ max(
          unknown_index_minimum_physical_entries,
          logical_live_entry_count
            * unknown_index_entry_pressure_multiplier
      )

invisible_entry_count_estimate
    = max(0, physical_entry_count - logical_live_entry_count)

leaf_page_count
    ≈ max(
          1,
          ceil(
              physical_entry_count
              / unknown_index_entries_per_leaf
          )
      )
```

using checked finite arithmetic and conservative nonnegative rounding/clamping.
These are physical-work estimates, not exact upper bounds or key-presence facts.
Even when their numerical candidate estimate is zero, descent/setup and the
legal runtime index/heap path remain present.

## 36.8 Index range-scan cost

A range-scan estimate includes:

```text
root-to-first-leaf descent
physical leaf pages traversed
physical index entries examined
candidate RID count
estimated invisible/garbage candidates
estimated distinct heap pages fetched
MVCC rejects
required heap decode
residual predicate work
```

Logical result selectivity is estimated from live-row/column statistics. Physical B+ work is then inflated separately when `IndexStatistics` show accumulated non-live entries.

When those statistics are unavailable, use the §36.7 derived physical/live
entry estimates before applying the same candidate-inflation model. Missing
occupancy uses `unknown_index_entries_per_leaf`; missing correlation uses
`unknown_index_heap_correlation`. No missing input is interpreted as zero
entries, perfect locality, or permission to omit heap visibility work.

A baseline derived factor is:

```text
candidate_inflation
    = max(
          1,
          physical_entry_count
          /
          max(1, logical_live_entry_count)
      )
```

and a first physical-candidate estimate may use:

```text
physical_candidates
    ≈ min(
          physical_entry_count,
          logical_candidate_rows * candidate_inflation
      )
```

subject to range/point-specific clamps and stronger measurements. This is intentionally approximate: global index garbage may not be distributed uniformly across key space. The optimizer trace should expose when this fallback inflation model materially affects cost.

Estimated leaf pages use:

```text
physical candidate/index-entry estimate
average_entries_per_leaf / leaf occupancy stats
```

where available.

A collected `average_entries_per_leaf == 0` is a valid degenerate statistical
value but is not a usable divisor. In that case use
`unknown_index_entries_per_leaf`; the statistics generation remains valid and
the fallback use is exposed diagnostically.

Heap fetch locality uses §36.9 correlation or §36.10 fallback.

## 36.9 Index/heap correlation

When available, `IndexStatistics.leading_key_heap_correlation` is an approximate coefficient: `-1.0 .. +1.0` collected from sampled: `(key rank, heap PageNo rank)` pairs for an indexed leading key.

High absolute correlation implies range RID fetches tend toward sequential heap-page order. Low absolute correlation implies near-random access. The sign describes direction; locality costing primarily uses absolute correlation unless the chosen scan direction makes direction itself relevant.

The baseline model interpolates conservatively between sequential and random heap-page cost as correlation strengthens rather than applying a binary clustered/unclustered label.

When no compatible correlation statistic is available, use the validated
`unknown_index_heap_correlation` assumption and expose that fallback in the
optimizer trace. It is not a claim of measured locality.

## 36.10 Fallback distinct heap-page estimate

Without useful correlation, assume secondary-index heap candidates are approximately scattered across table pages. Expected distinct heap pages MUST be capped by `physical_heap_pages` and MUST NOT charge one cold random read per matching row when many rows necessarily share pages.

Use a standard occupancy/distinct-page approximation based on:

```text
candidate rows
physical heap pages
rows-per-page density
```

with stable numerical clamping for empty/small relations.

The page cap is the selected physical heap-page input from §36.5.1. When an
observed density cannot be derived, use `unknown_heap_rows_per_page`; when index
leaf occupancy is unavailable, use `unknown_index_entries_per_leaf`. Zero
candidate estimates may produce zero estimated heap fetches, but do not suppress
the runtime cursor, visibility checks for candidates actually found, or the
alternative itself.

The exact numerical occupancy approximation is a cost-model implementation detail/calibration choice, not a correctness contract.

## 36.11 Access-predicate classification

Before base access enumeration, classify each applicable predicate as:

```text
index-search condition
scan-pushable condition
residual condition
```

Classification is based on the normalized semantic predicate and the particular index key schema. A predicate is not index-searchable merely because it mentions an indexed column.

## 36.12 B+ sargability

For a B+ index on: `(a, b, c)` a tight search prefix consists of:

```text
zero or more leading equality-like constraints
followed by at most one range-constrained component
```

Equality-like constraints include exact non-NULL equality and `IS NULL` when the key component is nullable, because the index has an exact NULL key representation.

Ordinary:

```sql
a = NULL
```

is never converted to an index NULL equality because its SQL result is UNKNOWN.

Example:

```text
a = 5
AND b = 7
AND c >= 10
AND c < 20
```

produces one tight composite range.

But:

```text
a > 5
AND b = 7
```

uses the range on `a`; `b = 7` is residual because the leftmost-prefix search cannot skip across the range component. Expressions/functions on a base column are not searchable through a plain column index unless an optional expression-index capability is defined and enabled.

## 36.13 Composite bounds

Composite index bounds are encoded with the exact Chapter-8 `IndexKeyCodec`.

Conceptually, unspecified trailing user-key components use transient:

```text
LOW_KEY_COMPONENT
HIGH_KEY_COMPONENT
```

search sentinels ordered before/after any valid encoded value for that bound position.

Because physical B+ entries are ordered by: `(user_key, RID)` search construction may append transient:

```text
MIN_RID_BOUND
MAX_RID_BOUND
```

to express inclusive/exclusive duplicate-key endpoints.

These bound sentinels are optimizer/B+ cursor search objects only:

```text
they are never persisted as RID/index entries
and are never accepted by persisted RID decoding
```

Bound construction preserves:

```text
inclusive/exclusive endpoint semantics
NULL ordering
ASC key encoding
type/collation semantics
```

## 36.14 Residual predicates

An index-search predicate may coexist with residual predicates.

For:

```text
index(a,b)
WHERE a = 5
  AND b > 10
  AND expensive_function(c)
```

search bounds represent: `a = 5, b > 10` while the function predicate remains residual.

Because B+ user-key encoding is exact, a predicate fully proven by exact index bounds need not be re-evaluated merely as a safety check. Any predicate not fully represented by the selected bounds remains residual.

A scan-pushable predicate is costed once for each estimated SQL-visible tuple on
which the scan evaluates it. An index residual is costed once for each estimated
candidate that survives the prerequisite index/heap access and MVCC visibility
steps and reaches that predicate, not merely for final survivors. Predicates
evaluated earlier on encoded index entries use that earlier physical-entry domain
instead. Columns decoded to evaluate a predicate use that predicate's evaluation
domain; output-only decode/materialization uses surviving output rows.
Conjunction short-circuit estimates may reduce later-component
evaluation counts only under §38.17's legal expression-order rules; they never
change Chapter-20 demand or ordinary-error behavior.

## 36.15 Base access alternatives

For every `LogicalGet`, enumerate at least:

```text
one PhysicalSeqScan
every semantically usable PhysicalIndexScan
```

unless the logical subtree already carries an approved §35.2 semantic-empty proof and is replaced by a schema-equivalent empty implementation. A statistical zero estimate never suppresses the SeqScan, an otherwise usable IndexScan, runtime index cursor work, or required heap MVCC checks.

Each alternative records:

```text
estimated output rows
estimated average row width
Cost
provided ordering property
required heap ColumnIds/LogicalSlotIds
index-search bounds when applicable
residual predicate
```

No access path is selected solely because an index exists.

Absence of advisory physical statistics never makes a semantically usable path
uncostable or ineligible. Section 36.5.1 supplies finite physical inputs for the
SeqScan and every usable single-index alternative. Descriptor/schema
incompatibility remains an eligibility failure under Chapters 22 and 34 and
cannot be repaired by fallback or low cost.

## 36.16 One-index baseline

The architecture chooses at most one B+ index access path for one base relation occurrence.

Deferred:

```text
bitmap index intersection
bitmap index union
index merge
```

Predicates unsupported by the chosen index remain scan/residual filters.

## 36.17 Index/SeqScan break-even

The cost model must naturally permit either of:

```text
few candidate rows + selective/local index access
    -> IndexScan cheaper

many scattered heap candidates
    -> SeqScan cheaper
```

The break-even point depends on relation size, index/heap correlation, candidate density, cache assumptions, required columns, and calibrated CPU/I/O weights. It is not a hard-coded selectivity percentage.

## 36.18 Required-column cost

Projection pruning changes access cost.

A SeqScan decoding two narrow fixed-width columns has similar page I/O to a scan of twenty wide columns, but substantially less:

```text
tuple decode CPU
vector materialization work
memory traffic
output width
```

IndexScan still fetches heap tuples for visibility in the defined architecture, but required output/predicate columns determine how much tuple decoding/materialization work remains after the fetch.

Decode, expression, copying, and materialization work is charged by the physical
node that performs it, over the estimated occurrences on which it executes. A
scan owns scan-fused work; a separate Project owns its remaining expressions.
Projection pruning changes width and work but does not permit required predicate,
visibility, output, or error-producing expressions to be omitted. The same work
is never charged by both the scan and a removed/fused unary node.

## 36.19 Base-access invariants

1. Cost is a calibrated abstract resource model, not promised wall-clock milliseconds.
2. Cost components remain inspectable rather than collapsing into one unexplained magic constant.
3. Planning does not inspect exact current BufferPool residency.
4. SeqScan costs physical pages/versions as well as logical live rows.
5. B+ point/range access still performs heap MVCC visibility checks.
6. Logical visible-row selectivity and physical B+ entry/candidate pressure are costed as distinct quantities.
7. Range heap locality is costed using correlation when available and a bounded distinct-page fallback otherwise.
8. Access predicates are classified per index schema before path enumeration.
9. Composite B+ search obeys the leftmost equality-prefix + one-range rule.
10. `IS NULL` may form an exact nullable-key search; `= NULL` never does.
11. Search-bound RID/key sentinels are transient and never persisted as actual database identities.
12. Exact index-proven predicates need not be redundantly rechecked; partial predicates remain residual.
13. Every LogicalGet has a SeqScan alternative and every usable single-index alternative.
14. The architecture uses at most one index per base relation occurrence.
15. Index-versus-sequential break-even emerges from costs rather than a fixed selectivity threshold.
16. Required-column width/decode work affects cost even when underlying heap-page I/O is similar.
17. Row-count, key-domain, and index-entry statistics may estimate zero output rows or candidates where the numerical model permits, but normal access-path costs still apply and the estimate cannot remove all semantically valid runtime access paths or prove candidate absence.
18. One complete validated `CostConfig` is stable for an optimization invocation; every scalar conversion has one declared unit and invalid configuration never reaches cost comparison.
19. Checked cost arithmetic saturates valid overflow at `MAX_FINITE_COST`; saturation remains diagnostic estimate metadata and creates no semantic proof or resource failure.
20. Missing advisory physical heap/index statistics use the ordered stable derivation/configured fallback contract, so every legal baseline SeqScan and usable single-index alternative remains finitely costable.
21. Every modeled work unit has one physical owner; ordinary streaming children are included exactly once, while specialized Chapter-38 repeated/blocking formulas remain authoritative.
22. Predicate, projection, decode, and materialization work is charged over the estimated occurrences where it is actually evaluated, without changing expression demand, errors, exact first-K eligibility, or physical properties.
