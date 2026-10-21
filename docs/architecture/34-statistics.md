# 34. Statistics

## 34.1 Statistics role

Statistics are approximate, rebuildable performance metadata.

They may be:

```text
sampled
stale
approximate
missing
```

without making a query semantically incorrect. A bad estimate may produce a bad plan; it MUST NOT produce a wrong result.

Query correctness is invariant under the age or numerical contents of any structurally valid statistics descriptor. Values that were exact for the ANALYZE snapshot, including a zero live-row count, exact NULL count, or exact min/max, become ordinary potentially stale statistics when used for planning. They are not execution-time constraints and MUST NOT establish semantic emptiness.

Only the semantic reasoning layer defined by §§20.17.10 and 35.2 may prove a plan subtree empty. Statistics may produce a numerical estimate of zero, but no table, column, index, sampled, cached, or derived statistic may set `is_provably_empty` or authorize an empty-result rewrite.

Statistics therefore remain outside WAL-critical data-correctness paths except for the ordinary transactional persistence of the catalog rows that store them.

## 34.2 ANALYZE

The SQL maintenance interface includes:

```sql
ANALYZE table_name;
```

A targetless `ANALYZE` over all tables is outside the baseline.

The baseline requires an explicit/manual ANALYZE path. Automatic ANALYZE
scheduling is not required; optional maintenance-trigger scheduling may request
ANALYZE under §14.16 without assuming statistics-publication ownership.

VACUUM and ANALYZE remain distinct operations even when one maintenance
scheduler or combined invocation requests both.

## 34.3 ANALYZE visibility and publication

One ANALYZE invocation uses one stable effective SQL snapshot under the transaction's normal isolation rules.

Snapshot stability does not establish current object liveness for publication;
§14.17.1 owns the short final publication gate against DROP/schema/index changes.

Rows visible to that snapshot contribute to SQL-visible live-row/column statistics. Physical heap-page, dead-version, and B+ entry/leaf information may additionally be inspected as approximate maintenance metadata without changing SQL visibility semantics.

Every ANALYZE statement receives one exact:

```text
StatsVersion {
    TxnId     txn_id;
    CommandId command_id;
}
```

equal to the owning transaction and current command that perform the ANALYZE. The pair is unique for one statement attempt under one TxnId and requires no separate persistent version allocator. Version comparison is lexicographic by: `(txn_id, command_id)` for choosing among multiple visible committed statistics versions for the same table.

This ordering is a deterministic freshness preference, not SQL semantic truth; overlapping ANALYZE transactions may still produce approximate/stale statistics. ANALYZE constructs one complete immutable candidate `StatsDescriptor` before writing/publishing its statistics version.

All `sys_statistics` rows for one StatsVersion are written by the same transaction. The TABLE-scope payload is the version manifest: it names the exact analyzed ColumnIds and IndexIds that must have complete matching payloads for the version to be accepted.

Publication has two levels:

```text
inside owning transaction after statement success:
    transaction-local StatsDescriptor may be used by own later statements

outside owning transaction:
    previous committed StatsDescriptor remains authoritative
    until owning transaction reaches terminal COMMITTED
```

At terminal COMMITTED publication:

```text
generation becomes eligible for global committed selection
then C5 installs one complete immutable descriptor/cache entry
or establishes the required safe cache fallback
```

without mutating descriptors held by existing planners. ABORT, cancellation, an incomplete chunk set, a missing manifest member, or payload validation failure does not globally publish that version. After restart, the catalog/statistics loader selects the highest visible committed TABLE-manifest StatsVersion whose complete listed COLUMN/INDEX payload set validates.

It may fall back to an older complete visible version or to missing-statistics behavior rather than combining fragments from different versions.

### 34.3.1 StatsVersion publication and transaction-status lifetime

`StatsVersion = (TxnId, CommandId)` has two deliberately separate lifetime
roles:

```text
before global committed publication:
    identify transaction-owned ANALYZE rows/candidate state while ordinary
    transaction status and catalog MVCC determine whether those writes publish

after global committed publication:
    immutable opaque generation identity and deterministic freshness key among
    catalog rows already admitted as committed
```

During live execution, the owning transaction and current command assign the
pair. Other transactions cannot use its rows before ordinary terminal COMMITTED
publication. The owning transaction may use its complete transaction-local
descriptor after successful statement completion under the existing CommandId
boundary; ABORT or crash without a surviving committed outcome prevents global
selection.

The runtime transition to globally published identity occurs at §15.5 C4's
terminal COMMITTED publication after durable C3, with C5 installing or safely
invalidating/bypassing the derived cache. After a crash, recovery's committed
transaction classification plus ordinary catalog MVCC reconstruction establishes
the equivalent boundary before statistics loading. Once a `sys_statistics` row
has been admitted as visible committed catalog input, a loader MUST NOT re-prove
its creator outcome by looking up the payload `stats_txn_id`.

The authoritative complete-generation selection is:

```text
1. obtain rows through ordinary catalog MVCC for the caller; this step excludes
   aborted/uncommitted outer tuple versions and may consult/freeze their tuple xmin
2. retain only rows applicable to the current catalog object/schema rules
3. decode and group by exact table/scope/StatsVersion identity
4. validate identifier domains, row/payload StatsVersion equality, unique and
   contiguous chunks, TABLE manifest, required COLUMN/INDEX members, object
   identities, and the §34.14 payload grammar
5. among complete visible committed generations for one table, select the
   greatest StatsVersion under unsigned lexicographic (txn_id, command_id) order
6. otherwise use an older complete generation or missing-statistics fallback
```

Step 1's **outer catalog tuple MVCC metadata** and step 3's **payload identity**
are not interchangeable. Vacuum may later rewrite the outer tuple `xmin` to
`FROZEN_TXN_ID`; it never rewrites payload `stats_txn_id`. The latter is not
passed to the transaction-status store, does not retain a status page/cache entry,
and remains directly comparable after its creator's status has been reclaimed.

TxnIds are globally monotonically allocated and never reused under §9.2, so the
unsigned TxnId component supplies the primary deterministic order while valid
identifiers remain; this section does not alter the separate exhaustion boundary.
CommandIds are monotonic and never reused within one transaction under §9.6, so
the command component distinguishes and orders multiple ANALYZE statements by
that transaction, including repeated analyses of one table. The ordering is only
a freshness preference among otherwise usable generations. It does not prove
object liveness, statistics accuracy, or semantic query facts.

Status reclamation therefore ignores `MIN(sys_statistics.stats_txn_id)`. A very
old complete generation remains structurally usable after its creator status is
`RETIRED`; a missing/reclaimed status entry is neither ABORTED nor malformed
StatsVersion evidence. Object/catalog applicability still decides whether a
generation belongs to a currently usable table, and old-generation deletion is
ordinary catalog MVCC/maintenance work under the maintenance coordination
protocol in §14.17.1.

Process-local statistics cache keys compare the stored numeric pair directly.
They retain no transaction-status object pointer, status-page pin, reclamation
guard, or terminal-outcome-cache entry solely for StatsVersion, and loading old
statistics never repopulates ancient terminal outcomes. Cache failure after
durable commit remains governed by the installed/invalidated/fallback rule in §39.1.8 and cannot
invalidate the committed catalog generation.

Restart and crash outcomes are exact:

| Point | Statistics outcome |
|---|---|
| crash before a surviving durable ANALYZE commit | transaction rows are loser/aborted catalog versions and are not selected |
| durable COMMIT survives, crash precedes C4/C5 cache publication | recovery establishes COMMITTED; visible complete rows load normally without payload-status lookup |
| status reclamation occurs much later | StatsVersion remains unchanged and usable; status pages may be sparsely reclaimed |
| restart after that reclamation | catalog MVCC/frozen outer rows establish visibility; loader accepts the old complete generation without creator-status history |

StatsVersion structural validation still rejects/falls back from an invalid
normal-TxnId domain (`INVALID_TXN_ID`, `FROZEN_TXN_ID`, or outside the allocated
identity contract), an invalid CommandId carrier/range, row/payload mismatch,
cross-version chunks, or duplicate/malformed chunk identity under §§16.5.7 and
34.14. Missing transaction status is not one of those conditions.

The architecture forbids:

1. retaining status pages because statistics payloads contain their TxnIds;
2. invalidating statistics because `stats_txn_id` is below the reclaim cutoff;
3. interpreting missing/retired creator status as ABORTED;
4. querying transaction status on statistics-cache load after catalog visibility;
5. rewriting/deleting StatsVersion solely to advance status reclamation;
6. treating payload `stats_txn_id` as the outer catalog tuple `xmin`;
7. using StatsVersion to establish object liveness;
8. selecting aborted/uncommitted ANALYZE rows because their pair compares newer;
9. making valid statistics lifetime depend on status-history retention; and
10. extending this opaque-identity exception to ordinary status-dependent heap or
    catalog MVCC TxnIds.

## 34.4 TableStatistics

For each table, collect at least:

```text
TableId
StatsVersion
analyzed_schema_version
analyzed_live_row_count
physical_heap_pages
average_logical_row_width
average_stored_tuple_width
dead_version_estimate
```

Useful derived values include:

```text
live_rows_per_heap_page
physical_tuple_version_estimate
```

The optimizer costs SQL-visible rows primarily from `analyzed_live_row_count`, while scan I/O uses physical heap pages and scan CPU may include dead-version pressure. `analyzed_live_row_count == 0` is a cardinality estimate for the ANALYZE snapshot, not proof that a later scan is empty.

## 34.5 ColumnStatistics

For each analyzed base column, collect:

```text
TableId
ColumnId
StatsVersion
LogicalType
null_fraction
NDV                       // non-NULL distinct values
min_value
max_value
MCV values
MCV frequency fractions
histogram
average_width             // especially VARCHAR
maximum_observed_width
```

`NDV` excludes NULL. All scalar values in statistics use the same logical type semantics as binder/executor comparisons.

`null_fraction`, NDV, min/max, MCVs, histogram boundaries/masses, widths, and values absent from any collected structure are costing evidence only. In particular, `null_fraction == 0` does not prove `IS NULL` impossible, `null_fraction == 1` does not prove `IS NOT NULL` impossible, and absence from an MCV list or histogram does not prove value absence. An enforced `NOT NULL` catalog constraint is a separate semantic fact.

## 34.6 IndexStatistics

The cost model also owns approximate statistics required by index access costing.

For an analyzed index, retain when available:

```text
IndexId
StatsVersion

physical_entry_count
logical_live_entry_count
invisible_entry_count_estimate

leaf_page_count
average_entries_per_leaf
leading_key_heap_correlation
```

The counts have distinct meanings:

```text
physical_entry_count:
    approximate number of physical B+ leaf entries observed

logical_live_entry_count:
    number of entries expected for SQL-visible live rows in the ANALYZE snapshot

invisible_entry_count_estimate:
    approximate physical entries not represented by that live-row baseline
    (aborted/dead/obsolete tuple-version index garbage and concurrent skew)
```

For the non-partial index model, one visible live tuple contributes one logical entry to each ordinary index, including rows whose key contains NULL.

A baseline fallback estimate is:

```text
invisible_entry_count_estimate
    = max(0, physical_entry_count - logical_live_entry_count)
```

with the result treated as approximate because physical index collection may overlap concurrent DML.

The derived candidate-inflation factor is:

```text
max(
    1,
    physical_entry_count
    /
    max(1, logical_live_entry_count)
)
```

and may be used as a fallback when the optimizer lacks a more key-specific garbage distribution model. `leading_key_heap_correlation` is in: `[-1.0, +1.0]` and summarizes correlation between leading-key order and heap PageNo order.

These statistics are approximate planning metadata, not B+ structural correctness metadata or key-presence facts. A zero `physical_entry_count`, `logical_live_entry_count`, `invisible_entry_count_estimate`, or `leaf_page_count` estimate cannot suppress runtime index/heap access or prove that no key/RID candidate exists.

B+ metadata such as the actual current tree height remains owned by Chapter 8 and may be read from an immutable descriptor/metadata snapshot for costing.

## 34.7 Collection strategy

ANALYZE performs a vectorized full heap scan in the defined architecture.

During the visible heap scan it derives:

```text
exact live-row count for the ANALYZE snapshot
exact NULL count
exact min/max over visible non-NULL values
approximate NDV
approximate MCVs
bounded distribution samples
width statistics
physical/dead-version maintenance estimates
```

“Exact” in this collection list means exact for that one ANALYZE snapshot. It does not promote the persisted result to an authoritative constraint for a later statement, including a later command in the same transaction after intervening writes.

For each visible index, ANALYZE additionally obtains the physical index-maintenance statistics required by §34.6.

The baseline may use a full leaf-chain walk or bounded sampling for:

```text
physical_entry_count
leaf_page_count / occupancy
sampled key-rank <-> heap-PageNo correlation
```

These index-physical observations are performance metadata and need not be from the identical instant as the MVCC heap snapshot. The descriptor records their approximate nature rather than treating them as visibility facts.

The baseline deliberately uses the full heap scan rather than a page/block
sampler. Page/block sampling as a replacement for that scan is outside the
baseline collection contract.

The ANALYZE writer applies the canonical numerical construction and validation
rules in §34.14.6 before encoding any scope payload. Approximation affects the
quality of an accepted value, not the byte-domain, ordering, mass, or
generation-consistency rules that the writer must satisfy.

## 34.8 Small-table exact mode

For tables with at most approximately: `50,000 live rows`.

ANALYZE may collect exact:

```text
NDV
value frequencies
```

when its bounded maintenance-memory budget permits. The threshold is configurable and is not a persistent-format value.

## 34.9 HyperLogLog NDV estimation

For larger inputs use a HyperLogLog-style sketch.

Baseline precision:

```text
p = 14
register_count = 16,384
```

Hash the same canonical non-NULL SQL value representation used by query hash semantics for that logical type. NULL is excluded.

The v1 payload persists the resulting NDV estimate, not the HLL sketch.
Persisted incremental statistics sketches are outside the v1 payload contract.

The process-local register domain, canonical output bounds, and persisted-NDV
reader checks are §34.14.6.5. Because no HLL sketch bytes are persisted,
payload validation never invents a register grammar.

For a fixed HLL configuration, the pre-bound HLL estimate is the finite
binary64 numeric result delivered by completed process-local HLL collection
before the generic §34.14.6.5 observed-value, MCV, and exact non-NULL row-count
construction bounds; zero has its canonical `+0` value. For any two
successfully completed finite HLL collections with the same set of distinct
canonical non-NULL SQL values, this pre-bound estimate MUST be identical,
regardless of row order, duplicate multiplicity (including singleton values),
or chunk boundaries. This distinct-set rule applies to every logical type
using HLL; it requires neither identical internal sketch state nor identical
post-bound NDV candidates, whose construction bounds may differ.

HLL distributional quality is an end-to-end conformance property of canonical
SQL-value hashing, HLL collection, and the bounded NDV candidate produced by
the ordinary §34.14.6.5 ANALYZE construction path. Let `m = 2^p`; the architecture fixes
`p = 14` and `m = 16,384`. For each exact distinct cardinality `N` with
`4m <= N <= 64m`, let `S` be uniformly distributed over all `N`-element
subsets of the complete signed INT64 SQL-value domain. An admissible
presentation policy `P` is any deterministic mapping that, for every such
`S`, produces a finite input sequence `P(S)` consisting only of values in
`S`, with every value occurring at least twice, in any order and with any
further duplicate multiplicities. Its exact analyzed-row count `R` must be
representable by the ordinary ANALYZE row-count owner. For every admissible
`P`, feed `P(S)` through the ordinary HLL collection path and let `N_hat_P(S)`
be its resulting §34.14.6.5 post-construction-bounds NDV candidate,
interpreted as the mathematical value of its finite binary64 representation.
Define `e_P(S) = (N_hat_P(S) - N) / N`. Holding `P` fixed and taking expectation
`E_S` only over the uniform distinct-set ensemble, a conforming collector
MUST satisfy both `sqrt(E_S[e_P(S)^2]) <= 2 / sqrt(m)` and
`abs(E_S[e_P(S)]) <= 1 / sqrt(m)` for every admissible `P` and each such `N`.
These are population ceilings, not per-sketch limits or target errors; exact
state or candidate equality across presentations is not required. Every
presentation has `R >= 2N`, so the non-NULL row-count writer cap is above the
true distinct count and does not clip ordinary positive NDV error at `N`.
The ensemble imposes no production RNG or configurable hash seed and does not
prescribe an estimator, bias correction, or a separate statistical guarantee
for every SQL type.
Empty/smaller inputs retain §34.8 and §34.14.6.5 behavior; relative error is
not defined at `N = 0`. An individual structurally valid tail estimate is not
corruption or a runtime SQL, transaction, or database-health failure merely
because of its statistical error. Even a collector satisfying these quality
bounds produces approximate planning metadata, never semantic proof.

The HLL invariance and quality requirements concern successfully produced
estimates/candidates under the ordinary resource and failure rules; they do
not require ANALYZE success despite an independently permitted resource,
cancellation, or lower-layer failure. Row order or duplicate multiplicity
alone is not a new failure cause.

## 34.10 Most-common values

For large inputs use a bounded heavy-hitter method such as `SpaceSaving`.

Baseline target: `64 MCV entries per analyzed column`.

Persist conceptually:

```text
value
estimated frequency fraction
```

in descending frequency order.

The MCV frequency fraction is relative to all visible rows. Its exact persisted
domain, aggregate-mass tolerance, and residual normalization are owned by
§34.14.6; no implementation-local floating tolerance applies.

An unbounded exact value->count map is forbidden for large relations.

## 34.11 Histogram collection

Maintain a bounded reservoir sample of non-NULL values. Baseline target: `100,000 sampled values per analyzed column` subject to maintenance memory pressure.

After scanning:

1. sort sample values using SQL type ordering,
2. remove or discount MCV-represented mass so MCV and histogram estimation do not double count it,
3. construct approximately:
   ```text
   100 equi-depth bins
   ```
4. retain enough bin boundary/mass information for range estimation.

Histogram mass conceptually represents the residual non-NULL, non-MCV distribution.

The canonical maximum bin count, boundary order/equality, MCV disjointness,
aggregate coverage check, and deterministic normalized masses are
§§34.14.6.4 and 34.14.6.6.

## 34.12 Equi-depth rationale

Equi-depth bins target approximately equal row mass per bin.

This handles skew more usefully than equal-width numeric buckets for distributions such as:

```text
salary
dates
hot ID ranges
binary VARCHAR prefixes
```

without requiring an enormous bucket count.

## 34.13 Histogram value semantics

Histogram sorting/comparison uses §34.14.6.3's statistics scalar order, which is
the canonical non-NULL execution/index-key value order without adding a SQL
comparison overload (notably, BOOLEAN remains SQL-unordered).

In particular:

```text
VARCHAR -> binary byte ordering
FLOAT64 -> canonical total order/equality conventions
DATE/TIMESTAMP -> Chapter 17 scalar ordering
```

Statistics MUST NOT introduce a second locale/FLOAT ordering model.

## 34.14 Statistics persistence

Statistics live semantically in `sys_statistics` using the exact relation descriptor and chunked row contract from §16.5.7. Each TABLE/COLUMN/INDEX scope is first encoded as one complete `StatisticsPayloadV1` byte sequence and then split into catalog-row fragments of at most: `STATISTICS_CHUNK_BYTES = 4096`.

Reassembly concatenates fragments in strictly increasing `chunk_index`.

A scope is invalid unless:

```text
chunk_count >= 1
every index 0..chunk_count-1 appears exactly once
all rows have identical table/scope/StatsVersion identity
every nonfinal fragment has exactly 4096 bytes
final fragment has 1..4096 bytes
reassembled length matches payload total_length
payload checksum/structure validates
```

### 34.14.1 Common statistics-payload header

Every scope payload begins with this 40-byte prefix:

| Offset | Size | Field / meaning |
|---:|---:|---|
| `0` | 8 | magic = ASCII `DBLUSSTA` |
| `8` | 2 | payload_version = `1` |
| `10` | 2 | scope_kind (`1` TABLE, `2` COLUMN, `3` INDEX) |
| `12` | 4 | total_length |
| `16` | 4 | checksum_crc32c |
| `20` | 4 | flags = `0` |
| `24` | 8 | stats_txn_id |
| `32` | 4 | stats_command_id |
| `36` | 4 | reserved32 = `0` |

All multi-byte integers are little-endian. `total_length` is the exact number of bytes in the reassembled scope payload. CRC32C is computed over exactly `total_length` bytes with bytes `16..19` logically zero. The `(stats_txn_id, stats_command_id)` pair is the payload's `StatsVersion` and must equal the catalog-row identity.

### 34.14.2 TABLE payload / version manifest

For `scope_kind = TABLE`, bytes `40..103` are:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 4 | analyzed SchemaVer |
| `52` | 4 | column_count |
| `56` | 4 | index_count |
| `60` | 4 | reserved32 = `0` |
| `64` | 8 | analyzed_live_row_count |
| `72` | 8 | physical_heap_pages |
| `80` | 8 | dead_version_estimate |
| `88` | 8 | average_logical_row_width as binary64 |
| `96` | 8 | average_stored_tuple_width as binary64 |

Both width fields MUST decode to finite binary64 values numerically greater than or equal to zero. Zero is valid, including for an empty relation. A NaN, positive or negative infinity, or value numerically less than zero makes the statistics generation numerically invalid under §34.14.6.

Starting at byte `104`, the manifest contains:

```text
column_count entries:
    ColumnId  uint32 little-endian
    reserved uint32 = 0

then index_count entries:
    IndexId uint64 little-endian
```

ColumnIds and IndexIds are each strictly increasing within their respective arrays and contain no duplicates.

The exact TABLE payload length is:

```text
104
+ column_count * 8
+ index_count * 8
```

with checked uint32 `total_length` arithmetic. The TABLE payload is the completeness manifest for the StatsVersion. Its `scope_id` in `sys_statistics` is exactly `0`.

### 34.14.3 COLUMN payload

For `scope_kind = COLUMN`, bytes `40..103` are:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 4 | ColumnId |
| `52` | 4 | persisted TypeId |
| `56` | 8 | null_fraction as binary64 |
| `64` | 8 | NDV estimate as binary64 |
| `72` | 8 | average_width as binary64 |
| `80` | 8 | maximum_observed_width |
| `88` | 4 | mcv_count |
| `92` | 4 | histogram_bin_count |
| `96` | 4 | value_flags |
| `100` | 4 | reserved32 = `0` |

`value_flags` are:

```text
bit 0 = HAS_MIN
bit 1 = HAS_MAX
all other bits = 0
```

Variable data begins at byte `104` in this exact order:

```text
if HAS_MIN:
    one §17.13 PersistedScalarV1

if HAS_MAX:
    one §17.13 PersistedScalarV1

mcv_count times:
    one non-NULL PersistedScalarV1 value
    one binary64 frequency_fraction

histogram_bin_count times:
    one non-NULL PersistedScalarV1 upper_boundary
    one binary64 residual_mass
```

Every scalar TypeId must equal the column TypeId. MCV entries are in descending frequency order with deterministic scalar-order tie breaking.

Histogram boundaries are nondecreasing in the §34.14.6 statistics scalar
order. The complete finite/range, MCV uniqueness/order, histogram, mass,
degenerate-column, and normalization rules are §34.14.6; this byte grammar does
not define a second looser acceptance policy.

### 34.14.4 INDEX payload

For `scope_kind = INDEX`, the v1 payload length is exactly `112` bytes:

| Offset | Size | Field |
|---:|---:|---|
| `40` | 8 | TableId |
| `48` | 8 | IndexId |
| `56` | 8 | physical_entry_count |
| `64` | 8 | logical_live_entry_count |
| `72` | 8 | invisible_entry_count_estimate |
| `80` | 8 | leaf_page_count |
| `88` | 8 | average_entries_per_leaf as binary64 |
| `96` | 8 | leading_key_heap_correlation as binary64 |
| `104` | 8 | reserved64 = `0` |

`average_entries_per_leaf` and `leading_key_heap_correlation` obey the exact
§34.14.6 domains and signed-zero normalization.

The three entry-count fields keep physical B+ work separate from SQL-visible live-row cardinality.

### 34.14.5 Payload validation and rebuildability

A statistics loader validates:

```text
catalog row identity
chunk completeness/order
magic/version/scope/flags/reserved fields
length arithmetic
CRC32C
StatsVersion equality
TableId/ColumnId/IndexId identity
scalar codecs
counts/fractions/order invariants
the complete §34.14.6 numerical and generation-consistency contract
TABLE manifest completeness
```

A payload version unknown to the current engine, malformed statistics payload, or incomplete statistics version invalidates that statistics version. After ordinary catalog rows/chunks reassemble safely, exact `DBLUSSTA` plus a readable positive `payload_version>1` is enough to classify unsupported advisory metadata without applying v1 checksum/layout parsing; version zero or malformed version-1 bytes are an invalid generation. Neither is parsed best-effort.

Because statistics are rebuildable performance metadata, such a failure may produce: `missing statistics + diagnostic` rather than making otherwise valid user data unreadable.

Neither a structurally valid descriptor nor a descriptor rejected as invalid can produce semantic proof. Rejected statistics are ignored in favor of an older valid descriptor or missing-statistics fallback; the exact §34.14.6 tolerance does not alter this rule.

This is the narrow §4.14.5 IGNORABLE_EXTENSION/fallback whitelist. It begins only after ordinary §§4.13–4.14 heap/tuple decoding yields decodable `sys_statistics` rows. Malformed catalog-page/tuple physical framing remains corruption; unknown core catalog descriptors/defaults do not inherit statistics tolerance. Section 34.14.6 owns numerical validation inside otherwise known payload version 1.

The loader MUST NOT mix rows from different StatsVersions to repair an incomplete descriptor.

Under §34.3.1, a reclaimed/missing terminal-status entry for
`stats_txn_id` is not a payload-validation input and MUST NOT invalidate an
otherwise complete generation. Structural validation checks the pair's numeric
domains and consistency directly; it does not call transaction-status lookup.

Arbitrary C++ object graphs, enum ordinals, process pointers, or native object dumps are never serialized.

### 34.14.6 Statistics validation and normalization

This section is the sole owner of numerical acceptance for known
`StatisticsPayloadV1` payloads. The validation layers are distinct:

```text
structural validity:
    chunk/header/length/checksum/scalar grammar and manifest completeness

numerical validity:
    deterministic finite domains, ordering, uniqueness, probability mass,
    cross-field relationships, and normalized descriptor state in this section

statistical accuracy:
    resemblance to current data or to the original sampled population
```

Structural and numerical validity are mandatory and deterministic. Accuracy may
degrade through approximation or staleness without invalidating a generation.
Even a fully valid descriptor remains costing-only under §§34.1 and 35.2.

#### 34.14.6.1 Binary64 domain, exact sum, and tolerance

The one aggregate probability tolerance is the exact dyadic value:

```text
STATS_PROBABILITY_SUM_EPSILON
    = 2^-40
    = 1 / 1,099,511,627,776
    = binary64 bits 0x3d70000000000000
    = 0.0000000000009094947017729282379150390625
```

For a correctly rounded exact fraction in `[0,1]`, the absolute conversion error
is at most `2^-53`; even conservatively charging all 165 fraction fields gives
`165 * 2^-53 < 2^-45 < 2^-40`. The constant therefore exceeds ordinary final
fraction-conversion accumulation while remaining far below a meaningful
probability. It is not
`numeric_limits<double>::epsilon()`, a relative tolerance, or a configurable
value.

Every aggregate probability sum and tolerance comparison in this section uses
**exact dyadic summation**. Decode each finite binary64 operand as its exact
mathematical value `significand * 2^exponent`, sum those dyadic rationals without
rounding, and compare the exact result with exact `0`, `1`, and the epsilon
above. Stored order remains the diagnostic order, but acceptance cannot depend
on floating evaluation order, an unordered container, extended precision, or a
process rounding mode. A process-local binary64 derived from an exact dyadic or
rational result is rounded once using §17.4.3 round-to-nearest, ties-to-even.

For any field whose domain includes numerical zero, persisted `-0.0` is accepted
as the same numerical value and normalized to `+0.0` in the materialized
statistics descriptor. A canonical ANALYZE writer emits `+0.0`. This rule does
not permit a negative nonzero value. NaN and either infinity are invalid in
every statistics binary64 field in the defined architecture.

Tolerance applies only to the two aggregate mass relationships explicitly
defined in §34.14.6.4. It never expands an individual field domain, correlation
bound, width bound, NDV bound, or ordering test. Values below `0` or above `1`
are rejected, not clamped, even when their distance from the endpoint is less
than epsilon.

#### 34.14.6.2 Persisted-field numerical registry

All unsigned integer carriers accept their full already-defined field domain;
identifier/count geometry and checked decoding remain §§4.3.2.5, 16.5.7, and
34.14. The numerical fields have this closed registry:

| Scope / field | Persisted type | Accepted numerical domain | NaN / infinity | `-0.0` | Exact cross-field rule | Aggregate tolerance | Failure |
|---|---|---|---|---|---|---|---|
| TABLE `analyzed_live_row_count` | uint64 | `0..2^64-1` | n/a | n/a | Exact generation row-count basis | none | invalid generation |
| TABLE `physical_heap_pages` | uint64 | `0..2^64-1` | n/a | n/a | No equality to current storage is required | none | invalid generation only for malformed carrier |
| TABLE `dead_version_estimate` | uint64 | `0..2^64-1` | n/a | n/a | Approximate; no exact relation to live rows/pages is required | none | invalid generation only for malformed carrier |
| TABLE `average_logical_row_width`, `average_stored_tuple_width` | binary64 | finite `>= 0` | forbidden | accept, normalize `+0` | Both are numerical zero when live-row count is zero | none | invalid generation |
| COLUMN `null_fraction` | binary64 | finite `[0,1]` | forbidden | accept, normalize `+0` | Participates in §34.14.6.4 | aggregate only | invalid generation |
| COLUMN `NDV` | binary64 | finite `>= 0` | forbidden | accept, normalize `+0` | §34.14.6.5; excludes NULL | none | invalid generation |
| COLUMN `average_width` | binary64 | finite `>= 0` | forbidden | accept, normalize `+0` | With non-NULL observations, no greater than exact uint64 `maximum_observed_width`; otherwise zero | none | invalid generation |
| COLUMN `maximum_observed_width` | uint64 | `0..2^64-1` | n/a | n/a | Zero when there is no non-NULL observation | none | invalid generation only for malformed carrier |
| COLUMN MCV `frequency_fraction` | binary64 | finite `[0,1]` | forbidden | accept, normalize `+0` | §34.14.6.3–.4 | aggregate only | invalid generation |
| COLUMN histogram `residual_mass` | binary64 | finite `[0,1]` | forbidden | accept, normalize `+0` | §34.14.6.4 and .6 | aggregate only | invalid generation |
| INDEX `physical_entry_count` | uint64 | `0..2^64-1` | n/a | n/a | Approximate physical observation; §34.14.6.8 | none | invalid generation only for malformed carrier |
| INDEX `logical_live_entry_count` | uint64 | `0..2^64-1` | n/a | n/a | Equals TABLE live-row basis; §34.14.6.8 | none | invalid generation |
| INDEX `invisible_entry_count_estimate` | uint64 | `0..2^64-1` | n/a | n/a | Approximate; need not equal physical minus logical | none | invalid generation only for malformed carrier |
| INDEX `leaf_page_count` | uint64 | `0..2^64-1` | n/a | n/a | Approximate physical observation; no current-tree equality | none | invalid generation only for malformed carrier |
| INDEX `average_entries_per_leaf` | binary64 | finite `>= 0` | forbidden | accept, normalize `+0` | No exact count ratio is required because collection may sample | none | invalid generation |
| INDEX `leading_key_heap_correlation` | binary64 | finite `[-1,1]` | forbidden | accept, normalize `+0` | Undefined/fewer-than-two-sample writer output is canonical `+0`; reader need not infer sample size | none | invalid generation |

Comparing a binary64 field with a uint64 bound means comparing its exact dyadic
value with the exact integer, not first rounding that integer to binary64.
Estimator-output clamping in Chapter 35 is downstream arithmetic and cannot make
an input outside this table valid.

#### 34.14.6.3 MCV identity and canonical order

For one COLUMN payload: `0 <= mcv_count <= 64`.

Each MCV scalar is non-NULL, uses the payload TypeId, and is a canonical
`PersistedScalarV1`. MCV values are pairwise unique under §17.10.3 non-NULL
canonical equality. Thus `-0.0` and `+0.0` are one FLOAT64 identity, and all
canonical-equivalent NaNs are one identity; exact VARCHAR bytes and the ordinary
BOOLEAN/integer/DATE/TIMESTAMP equality apply. A duplicate invalidates the
complete StatsVersion. Readers do not merge duplicate frequencies.

Materialized min/max, MCV, and histogram scalar identities use the same
§17.10.3 key normalization: FLOAT64 signed zeros become one `+0.0` identity and
NaN is the canonical NaN identity. This process-local normalization does not
rewrite the persisted scalar payload.

Statistics scalar order is the canonical non-NULL Chapter-8/Chapter-17 storage
order without adding SQL operator overloads: `FALSE < TRUE`, signed numeric/date/
timestamp order, unsigned-byte lexicographic VARCHAR order, and the §17.4.3
FLOAT64 total order with equal signed zeros and one NaN class. MCV entries are
ordered by:

```text
frequency_fraction numerically nonincreasing;
for equal binary64 numerical frequencies, statistics scalar value strictly increasing
```

The strict tie break plus pairwise uniqueness makes MCV order canonical. Zero
frequency remains in the already-specified `[0,1]` domain, although canonical
writers omit zero-frequency candidates when constructing the bounded top-64
list.

#### 34.14.6.4 Probability mass and normalized residual

When TABLE `analyzed_live_row_count=0`, §34.14.6.7's empty-column form is
mandatory, `R_exact=0`, and the ordinary mass formula below is not applied. This
prevents an empty table's canonical `null_fraction=0` from being mistaken for a
unit non-NULL population.

For a nonempty analyzed table, let exact dyadic sums be:

```text
M = sum of MCV frequency_fraction values in persisted order
P = exact(null_fraction) + M
D = 1 - P
```

The MCV/NULL base mass is accepted only when `P <= 1 + epsilon`. The canonical
residual non-NULL, non-MCV mass is:

```text
if abs(D) <= epsilon: R_exact = 0
else if D > epsilon: R_exact = D
else:                 invalid generation

residual_nonnull_nonmcv_mass = round_binary64_ties_even(R_exact)
```

This is the only clamping-like normalization: a base-mass excess or deficit no
larger than epsilon becomes exact residual `+0.0`. The persisted NULL/MCV values
are not rewritten. If `R_exact=0`, `histogram_bin_count` MUST be zero.

Let `H` be the exact dyadic sum of histogram `residual_mass` values. When
`histogram_bin_count=0`, no histogram coverage relationship is asserted and
estimators use the ordinary residual/NDV fallback. When it is nonzero, all of
these are required:

```text
R_exact > 0
H > 0
abs(H - R_exact) <= epsilon
```

The materialized descriptor exposes the normalized values:

```text
normalized null/MCV/bin inputs: persisted values with numerical -0 changed to +0
residual_nonnull_nonmcv_mass:   the binary64 value defined above
normalized mass of bin i:       round_binary64_ties_even(
                                    R_exact * exact(bin_i.residual_mass) / H)
```

The per-bin expression is evaluated as an exact rational and rounded once; it
does not perform an overflowing/intermediately rounded host division. Range
estimation consumes bins in persisted order, caps accumulated histogram contribution at
`residual_nonnull_nonmcv_mass`, and applies the existing estimator finalization;
it cannot reselect a local epsilon. This deterministic scale reconciles accepted
aggregate drift without changing payload bytes. Aggregate mismatch beyond
epsilon invalidates the generation.

#### 34.14.6.5 NDV, HLL, and observed-value constraints

`NDV` is an approximate binary64 estimate of non-NULL distinct values, not a
semantic integer fact. Reader validation requires:

```text
NDV <= exact TABLE analyzed_live_row_count
NDV >= exact mcv_count
HAS_MIN/HAS_MAX present -> NDV >= 1
HAS_MIN/HAS_MAX absent  -> NDV = 0, mcv_count = 0, histogram_bin_count = 0
nonempty table and R_exact > 0 -> NDV >= exact(mcv_count + 1)
```

An approximate HLL result is not rejected merely for differing from current
data. During canonical construction, ANALYZE bounds its raw estimate below by
the number of directly observed distinct MCV values (and by one when any
non-NULL value was observed) and above by the exact non-NULL count from its full
snapshot scan. If an exact uint64 upper bound is not exactly representable, the
encoded cap is the greatest binary64 value not greater than that integer. This
writer cap makes an above-row-count persisted NDV invalid rather than reader-
dependent approximation tolerance.

The v1 payload persists only the resulting NDV, not HLL registers. The
process-local HLL collection state is fixed at `p=14`, exactly 16,384
registers, each initialized to zero and in `0..51` for a 64-bit hash (50 suffix
bits plus the all-zero suffix rank). Wrong precision/register count, an
out-of-domain register, or a nonfinite/negative estimate is an ANALYZE candidate
failure before persistence; no HLL byte grammar or new payload field is implied.
Reservoir/HLL sample cardinality is not persisted in StatisticsPayloadV1 and therefore
has no reader-side field relationship to validate.

#### 34.14.6.6 Min/max and histogram invariants

`HAS_MIN` and `HAS_MAX` are either both set or both clear. If set, both scalars
are non-NULL canonical values of the COLUMN TypeId and:

```text
min_value <= max_value under statistics scalar order
every MCV value lies in [min_value, max_value]
every histogram boundary lies in [min_value, max_value]
```

If both flags are clear, MCV and histogram arrays MUST be empty. Any MCV or
histogram entry requires both flags set. Min/max may remain present when both
arrays are empty. These are exact relationships among statistics produced from
one analyzed snapshot; they do not compare against current table contents.

For one COLUMN payload: `0 <= histogram_bin_count <= 100`.

The histogram representation is a one-dimensional equi-depth residual histogram. NULLs and MCV value
identities are excluded from its population. Each entry stores an upper
boundary plus that bin's probability mass relative to all visible analyzed
rows; no implicit equal-mass value replaces the persisted `residual_mass`.

Each boundary is non-NULL, has the COLUMN TypeId, and boundaries are
nondecreasing under statistics scalar order. Equal adjacent boundaries remain
legal: they represent adjacent equi-depth residual bins ending at the same
semantic value and are consumed in stored order; a reader neither rejects nor
silently merges them. Histogram population excludes MCV identities, so no
boundary may equal any MCV value under canonical equality. This prevents MCV and
residual histogram accounting from representing the same value class.

For range estimation, an upper boundary is inclusive. A run of equal boundaries
is one comparison boundary group: `< b` excludes all mass in the run ending at
`b`, while `<= b` includes all of it; interpolation is only across distinct
adjacent boundary values. This gives duplicate quantile endpoints one exact
meaning without changing their persisted order or masses.

FLOAT64 boundaries use the full §17.4.3 total order, not raw IEEE unordered
predicates. Signed zeros compare equal, all NaNs form one greatest class, and
infinities retain their defined positions. Canonical persisted NaN requirements
remain §17.13. VARCHAR boundaries use unsigned-byte lexicographic order with no
locale or normalization.

#### 34.14.6.7 Degenerate canonical representations

Given the TABLE payload and each required COLUMN payload, canonical generation
states are:

| Analyzed state | Required COLUMN representation |
|---|---|
| `analyzed_live_row_count=0` | `null_fraction=+0`, `NDV=+0`, both value flags clear, empty MCV/histogram, `average_width=+0`, `maximum_observed_width=0` |
| live rows exist and all values are NULL | `null_fraction=1`, `NDV=+0`, both value flags clear, empty MCV/histogram, `average_width=+0`, `maximum_observed_width=0` |
| at least one non-NULL value | both min/max flags set, `NDV>=1`; min may equal max; average/maximum width obey §34.14.6.2 |
| one distinct non-NULL value / all non-NULL values equal | `min_value=max_value`, `NDV=1`; that value may be represented once as an MCV, by legal equal-boundary residual bins, or by the no-histogram residual fallback |
| NULL plus MCV base mass is tolerance-equivalent to one | normalized residual is `+0` and histogram is empty |

Because a binary64 fraction can round to an endpoint for extremely large uint64
row counts, `null_fraction=1` alone is not the non-NULL-observation discriminator;
the min/max flags and NDV constraints are. A canonical writer computes the
fraction from exact counts with one ties-to-even conversion and emits the table
above whenever the exact non-NULL count is zero.

TABLE average widths are numerical `+0` for zero live rows. INDEX observations
remain allowed for an empty live table because stale physical entries/pages may
exist; its `logical_live_entry_count` is nevertheless zero under the next
subsection.

#### 34.14.6.8 Counts and generation-wide consistency

The TABLE manifest, every required scope identity, TypeId, and object/schema
fingerprint or descriptor fact already present in the architecture must agree exactly under
§§16.5.7 and 34.14. In addition, every non-partial INDEX payload in the
generation has: `logical_live_entry_count = TABLE analyzed_live_row_count` because both describe one logical index entry per visible live heap tuple in the
same ANALYZE snapshot. No exact equality is required among
`physical_entry_count`, `invisible_entry_count_estimate`, `leaf_page_count`, or
`average_entries_per_leaf`: §34.7 permits physical index observation to overlap
concurrent DML and sampling. Likewise no current-storage comparison validates
TABLE page/dead estimates. Staleness is never numerical corruption.

#### 34.14.6.9 Generation acceptance, cache state, and writer rule

Validation is generation-atomic. If any manifest-required TABLE/COLUMN/INDEX
scope fails structural or numerical validation, the entire StatsVersion is
`INVALID_STATS_GENERATION`; a loader selects the next older complete valid
generation or missing-statistics fallback. It MUST NOT mix scopes from different
StatsVersions, merge duplicate MCVs, drop a bad bin/member, clamp an individually
illegal field, or salvage only the valid columns. A well-framed unsupported
payload version remains `UNSUPPORTED_STATS_VERSION` under §34.14.5;
missing statistics is distinct from both. Malformed outer catalog tuple framing
remains catalog/page corruption rather than advisory-payload fallback.

The cache contains only descriptors that passed the complete generation check.
It stores the normalized scalar identities, `+0` numerical zeros, exact-derived
residual value, and normalized histogram masses defined above. Cache hits do not
bypass that materialization validation, and §14.17.1 prevents a delayed
invalid/older install from replacing a newer applicable generation.

ANALYZE MUST emit only values satisfying this section and validate its complete
candidate through the same rules before the first `sys_statistics` publication.
Reader epsilon is solely a final binary64 aggregate-rounding allowance; writers
MUST NOT intentionally emit out-of-range probabilities or aggregate discrepancy
larger than the unavoidable construction drift. A canonical serialize/decode
round trip of an ANALYZE candidate must accept and produce the same normalized
semantic descriptor. No normalized value is written back merely because a
reader accepted it.

#### 34.14.6.10 Forbidden interpretations and boundary vectors

The architecture forbids:

1. choosing machine epsilon, a relative epsilon, or a local configurable tolerance;
2. accepting NaN/infinity because ordinary comparisons happened to return false;
3. clamping an individual negative probability or a value above one;
4. summing through unordered iteration or environment-dependent floating arithmetic;
5. merging duplicate MCV identities or treating signed zeros/NaN payloads as distinct MCVs;
6. using raw IEEE unordered comparison for FLOAT64 histogram order;
7. rejecting legal equal adjacent histogram boundaries or accepting decreasing ones;
8. double-counting a value in MCV and histogram boundary identity;
9. rejecting a stale-but-internally-valid descriptor because current storage differs;
10. mixing StatsVersions or retaining only selected valid members of a failed generation;
11. using estimator output clamping to legalize malformed persisted input;
12. allowing accepted statistics, including exact-looking zero, to prove semantic emptiness;
13. treating the persisted NDV as exact or requiring unbounded HLL accuracy;
14. rejecting the canonical empty/all-NULL/one-value representations;
15. preserving `-0.0` as a distinct process-local probability/correlation identity; or
16. letting compiler, libc, locale, host precision, or container iteration change acceptance.

Verification includes exact binary64 fixtures for `+0`, `-0`, `1`, the adjacent
representable values below zero and above one, sums exactly one, both sides of
the `2^-40` boundary, and sums just beyond it. It also covers duplicate MCVs,
`+0/-0` duplicate identities, two canonical-NaN duplicate identities,
noncanonical persisted NaN rejection, MCV tie order,
decreasing/equal histogram boundaries, MCV/histogram overlap, empty/all-NULL/
one-value columns, MCV-exhausted residual, nonfinite/negative widths, NDV bounds,
and correlation endpoints plus their adjacent out-of-range values. Synthetic
near-boundary payloads must yield the same accept/reject and normalized
descriptor on every supported platform.

## 34.15 Statistics snapshots and cache

Planning obtains one immutable `StatsDescriptor` for the required table/index statistics.

Materialization first applies the complete §34.14.6 generation validation and
normalization. Cache publication never exposes raw, partially validated, or
implementation-locally normalized payload values.

For ordinary cross-transaction planning, the descriptor must come from a complete visible committed StatsVersion. A transaction that has successfully completed ANALYZE may use its own complete transaction-local descriptor for later commands after the normal CommandId boundary even before COMMIT.

A concurrent committed ANALYZE publishes a new immutable descriptor atomically.

Concurrent install, delayed callback, and DROP invalidation are ordered by
§14.17.1; the cache may not regress to an older applicable StatsVersion or
reinstall metadata for a retired identity.

Existing planners may finish with the old descriptor. The catalog/statistics cache is an acceleration mechanism and preserves the caller's catalog visibility rules.

StatsVersion cache keys obey §34.3.1: they compare the stored unsigned numeric
pair directly and retain no transaction-status page/cache ownership solely for
that identity. A cache miss reloads through catalog MVCC and complete-generation
validation; it does not query or repopulate creator terminal status from the
payload pair.

Global cache publication is a commit-side effect; an aborted transaction never leaves its descriptor globally published.

After durable commit, inability to install the new descriptor in the statistics cache does not alter COMMITTED. Retaining the prior descriptor or selecting missing-statistics fallback is a coherent recovery path because statistics are approximate; the cache must never expose an uncommitted/partial candidate.

Transaction-local use, old cache entries held by existing planners, concurrent committed DML, and committed publication of a newer descriptor all preserve §34.1: descriptor freshness changes estimates only and never supplies semantic proof.

## 34.16 Freshness and modification counters

Track approximate table modifications since the last ANALYZE:

```text
rows_inserted
rows_updated
rows_deleted
```

A useful diagnostic staleness measure is:

```text
changed_rows
/
max(1, analyzed_live_row_count)
```

The optimizer may surface staleness in traces/EXPLAIN diagnostics. The architecture does not reject or automatically refresh a plan solely because statistics are stale. Exact crash-persistent modification-counter accounting is not a correctness requirement.

When a committed ANALYZE descriptor becomes the global current version for a table, runtime staleness counters may reset relative to that version. A transaction-local or later-aborted ANALYZE does not reset globally visible modification counters.

## 34.17 Statistics invariants

1. Statistics are planning metadata, not semantic truth.
2. One ANALYZE uses one stable SQL visibility snapshot for live-row/value statistics.
3. A StatsVersion is exactly `(TxnId, CommandId)` and all of its catalog rows are owned by one transaction.
4. The TABLE payload is the completeness manifest for one statistics version.
5. Global statistics publication occurs only after the owning transaction reaches terminal COMMITTED.
6. For each table statistics object used by one optimizer invocation, the invocation retains one complete compatible TABLE-manifest generation under §§33.4, 34.3.1, and 34.15; all required TABLE/COLUMN/INDEX members have that generation's StatsVersion, and later publication cannot mutate or mix it. Different tables may retain different valid StatsVersions; when no complete compatible generation is available, valid-old or missing-statistics fallback applies.
7. NDV excludes NULL.
8. MCV and histogram mass are not double counted.
9. Histogram/hash semantics agree with SQL value semantics.
10. HLL/MCV/reservoir structures are bounded in memory.
11. Large relations do not require unbounded exact frequency maps.
12. Index statistics distinguish logical live entries from physical entry/garbage pressure.
13. Unsupported/stale/incomplete statistics may degrade plans but never query correctness.
14. Statistics payloads use the byte-exact chunked v1 format and shared persisted-scalar codec.
15. Numerical zero or apparent domain absence from table, column, index, MCV, histogram, HLL-derived, sampled, cached, or composed statistics never establishes semantic emptiness.
16. Statistics rejected as corrupt and statistics accepted as structurally valid are both incapable of creating a semantic proof; they differ only in whether their estimates are used or fallback is selected.
17. Partial transaction-owned ANALYZE rows require transaction abort on statement failure even though descriptor completeness rules prevent their global use.
18. Post-commit statistics-cache installation failure retains COMMITTED and uses an older/missing descriptor fallback rather than redefining transaction outcome.
19. Before global publication, ordinary transaction/catalog MVCC decides whether ANALYZE rows commit; after publication, StatsVersion is opaque status-independent identity.
20. Complete-generation selection uses visible catalog rows, manifest/chunk/payload validation, and unsigned lexicographic StatsVersion ordering without payload creator-status lookup.
21. Status reclamation does not invalidate, rewrite, delete, or retain status pages for old complete statistics generations.
22. Outer `sys_statistics` tuple MVCC metadata remains subject to ordinary freezing/status rules independently of its payload `stats_txn_id`.
23. StatisticsPayloadV1 payloads are accepted only by the exact §34.14.6 numerical registry; one failed required member invalidates the whole generation, and every accepted descriptor has one deterministic normalized process-local form.
