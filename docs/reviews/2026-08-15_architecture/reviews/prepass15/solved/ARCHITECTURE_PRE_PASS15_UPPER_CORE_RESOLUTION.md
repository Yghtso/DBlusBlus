# Pre-Pass-15 Upper-Core Resolution

## Scope

This resolution closes exactly:

```text
R-036  catalog bootstrap / CATALOG_DATA
R-040  persistent default encoding
R-044  persistent sys_statistics encoding
R-046  ANALYZE end-to-end integration
R-047  physical index-garbage pressure in statistics/costing
R-048  exact primitive PredicateTruthEstimate triples
```

It does **not** perform Pass 15 or migrate legacy §628+.

Inputs:

```text
ARCHITECTURE_NEW_PASS14.md
SHA-256: 392972b682a4963bcd7b811757cbe81c24855589fc765f552e725900535352f6

ARCHITECTURE(4).md
SHA-256: 2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86
```

Resolved architecture:

```text
ARCHITECTURE_NEW_PRE_PASS15_RESOLVED.md
SHA-256: 77edb72e4872c9a3a6665e8a969b0f7c1a420b439c06b2c8dbc45832459f326d
```

No production code was modified.

## R-036 — resolved bootstrap

V1 uses one well-known immutable:

```text
catalog.dat
```

with exactly:

```text
page 0  CATALOG FileSuperblock, object_id = 0
page 1  CATALOG_DATA bootstrap page
```

The page-1 format is byte exact:

```text
64-byte header
6 * 32-byte system-relation entries
7936 zero reserved bytes
= 8192 bytes
```

The six fixed relation codes locate:

```text
sys_tables
sys_columns
sys_indexes
sys_index_columns
sys_constraints
sys_statistics
```

The bootstrap page is checksummed from creation, immutable after successful database creation, and cross-validated against the self-hosted catalog.

It is only a locator/interpreter seed; ordinary metadata remains relational.

## R-040 — resolved by simplifying v1 default persistence

V1 defaults remain syntactically capable of closed IMMUTABLE scalar expressions, but no expression tree is persisted.

DDL:

```text
bind
-> reject column/subquery/aggregate/STABLE/VOLATILE dependencies
-> evaluate/fold completely
-> coerce to target column type
-> persist one typed scalar
```

The byte-exact `DefaultValueBlob` uses:

```text
24-byte checksummed header
+
one shared PersistedScalarV1
```

and is capped at 4096 bytes.

This removes the need for stable persistent runtime function/operator implementation IDs in v1.

A future expression-tree format requires a new blob version.

## Shared PersistedScalarV1

Defaults and statistics share one scalar format:

```text
16-byte scalar header
type_id
flags
payload_length
reserved
type-specific payload
zero Align8 padding
```

BOOLEAN/INT32/INT64/FLOAT64/DATE/TIMESTAMP/VARCHAR bytes are exact.

Persisted FLOAT64 NaNs are canonical; signed zero is preserved because it may affect arithmetic.

## R-044 — resolved statistics persistence

`StatsVersion` is:

```text
(TxnId, CommandId)
```

No new durable counter is introduced.

`sys_statistics` rows carry:

```text
table/scope identity
StatsVersion
chunk_index/chunk_count
payload_fragment
```

with at most 4096 payload bytes per row.

Every reassembled payload uses a common 40-byte checksummed header.

Exact scope formats are:

```text
TABLE:
    104-byte fixed prefix + ColumnId/IndexId manifest arrays

COLUMN:
    104-byte fixed prefix
    + optional min/max PersistedScalarV1
    + MCV scalar/frequency entries
    + histogram scalar/mass entries

INDEX:
    exactly 112 bytes
```

TABLE scope is the completeness manifest.

A loader never mixes different StatsVersions to manufacture a descriptor.

Invalid/incomplete statistics fall back to an older complete visible version or missing-statistics behavior because statistics are rebuildable performance metadata.

## R-046 — ANALYZE integrated end to end

The v1 stack now contains:

```text
ANALYZE table_name
AnalyzeStatement
bound resolved target
LogicalAnalyze
PhysicalAnalyze
```

PhysicalAnalyze uses the normal execution context and one stable effective MVCC snapshot.

It builds a complete descriptor, writes transaction-owned `sys_statistics` rows, and may expose the descriptor only transaction-locally after successful statement completion.

Global publication occurs only after terminal COMMITTED.

ABORT/cancellation never publishes globally and leaves the prior committed descriptor authoritative.

ANALYZE does not take schema-changing DDL exclusivity or an exclusive TableWriterGate simply to stabilize rows.

## R-047 — physical B+ pressure separated from logical rows

`IndexStatistics` now distinguishes:

```text
physical_entry_count
logical_live_entry_count
invisible_entry_count_estimate
leaf_page_count
average_entries_per_leaf
leading_key_heap_correlation
```

Logical output cardinality remains based on SQL-visible statistics.

Physical IndexScan cost separately estimates leaf entries, candidate RIDs, MVCC rejects, and heap work.

A bounded approximate candidate-inflation ratio is available when no key-specific garbage model exists.

Thus vacuum/index-garbage state may make a plan slower without changing SQL cardinality.

## R-048 — exact SQL 3VL estimator primitives

Primitive predicate estimators now return complete:

```text
(TRUE, FALSE, UNKNOWN)
```

triples.

For non-NULL equality/ranges:

```text
unknown = column null_fraction
false   = 1 - true - unknown
```

IS NULL / IS NOT NULL never produce UNKNOWN.

IN distinguishes the presence of NULL list elements.

The independence fallbacks are exact under SQL 3VL:

```text
AND:
    t = tA*tB
    f = fA + fB - fA*fB
    u = 1 - t - f

OR:
    t = tA + tB - tA*tB
    f = fA*fB
    u = 1 - t - f
```

The post-audit exhaustively checked these formulas against the pure T/F/U truth tables.

## Coverage and boundary

Coverage is unchanged:

```text
legacy §§0..627   complete / explicitly disposed
legacy §§628..725 pending
```

The Pass-15 owner chapters 37–38 remain byte-for-byte unchanged from the Pass-14 input.

## Exit

```text
R-036 RESOLVED
R-040 RESOLVED
R-044 RESOLVED
R-046 RESOLVED
R-047 RESOLVED
R-048 RESOLVED
```

**PRE-PASS-15 UPPER-CORE RESOLUTION COMPLETE.**
