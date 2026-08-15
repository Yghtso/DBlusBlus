# Rewrite Pass 14 — Statistics, Cardinality Estimation, Cost Model, and Base Access Planning

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `568..627`
- processed source lines: `17113..18780`
- next untouched section: `628. Physical Property Model`
- legacy architecture modified: **no**
- production code modified: **no**
- physical-property/join-search §628+ migrated: **no**

Working architecture:

```text
input  SHA-256: 762a3e7e7d7d9c4f49d1059beb7f450ed0609499238d4a5e3082db0e5bf5c787
output SHA-256: 392972b682a4963bcd7b811757cbe81c24855589fc765f552e725900535352f6
```

## Canonical chapters

Pass 14 replaces optimizer placeholders with:

```text
Chapter 33  Optimizer Architecture
Chapter 34  Statistics
Chapter 35  Cardinality Estimation
Chapter 36  Cost Model and Base Access Paths
```

## Optimizer architecture

The selected architecture remains:

```text
System-R-style bottom-up DP
+ small property-aware memo
+ rule-based logical normalization
+ cost-based physical selection
```

Planning uses stable logical/catalog/statistics descriptors and never inspects exact mutable page contents or momentary BufferPool residency.

The finalized PhysicalPlan carries estimated rows, row width, cost components, memory/spill estimates, and physical properties for later execution/EXPLAIN.

## Statistics

V1 ANALYZE is an explicit vectorized full heap scan.

One ANALYZE uses one stable statement/maintenance snapshot and atomically publishes one complete immutable StatsDescriptor version.

Table statistics include live rows, heap pages, average widths, dead-version estimate and version/generation metadata.

Column statistics include null fraction, non-NULL NDV, min/max, MCVs, equi-depth histogram, and width statistics.

IndexStatistics now explicitly owns the planning metadata already required by legacy range costing:

```text
entry count
leaf pages
average entries/leaf
leading-key/heap-page correlation
```

Initial approximate structures remain:

```text
HLL p=14              16,384 registers
MCV target            64 entries/column
reservoir target      100,000 values/column
histogram target      ~100 equi-depth bins
small exact threshold ~50,000 live rows
```

MCV and histogram residual mass are kept conceptually separate so they are not double counted.

## Statistics format gap

`R-044` is new and OPEN.

The source requires versioned `sys_statistics` persistence but does not define the byte-exact payload or stable scalar encoding for MCV/histogram values.

Pass 14 preserves the semantic format and refuses to invent compiler-object serialization.

## Cardinality estimation

The estimator now canonically owns:

```text
finite double row estimates
explicit is_provably_empty
average row width
PredicateTruthEstimate(TRUE/FALSE/UNKNOWN)
MCV/histogram equality and ranges
NULL / IN / NOT / AND / OR
same-column constraint intersection
unique-key and MCV-aware joins
projection/filter/limit
DISTINCT/GROUP BY NDV
multi-column NDV damping
LEFT JOIN preserved-side lower bound
```

The exact fallback multi-column damping remains:

```text
NDV1
* NDV2^0.75
* NDV3^0.50
* every remaining NDVi^0.25
```

capped by input rows.

Grouping/DISTINCT estimates explicitly add one NULL class for a nullable base grouping column, matching execution semantics.

## Cost model

Cost remains calibrated abstract resource units rather than fake milliseconds.

Inspectable components include sequential/random persistent I/O, temp I/O, CPU rows/expressions, hash/comparison work, memory, and spill estimates.

Central deployment configuration owns relative weights.

`effective_cache_pages` models likely caching without inspecting exact BufferPool state.

SeqScan costing now explicitly relates dead-version pressure to physical tuple-version CPU work.

B+ point/range costs include tree/leaf work, heap candidate pages, MVCC, required decode, and residual predicates.

## Index correlation and heap fetches

Pass 14 gives index/heap correlation an explicit statistics owner.

High absolute correlation lowers expected random heap behavior; low correlation approaches random access.

When correlation is missing, the model uses a bounded occupancy/distinct-page estimate rather than charging one cold random page for every matching row.

No arbitrary exact occupancy formula is promoted to architecture.

## Sargability and composite bounds

For `(a,b,c)`:

```text
zero or more leading equality-like constraints
then at most one range component
```

remains the B+ leftmost-prefix rule.

Pass-14 completions:

```text
IS NULL      -> may be exact nullable-key equality search
= NULL       -> UNKNOWN, never converted to IS NULL

MIN/MAX key/RID bound sentinels
    -> transient cursor-search objects only
    -> never persisted as actual RID/index entries
```

Composite bounds use the exact Chapter-8 IndexKeyCodec and preserve inclusion, NULL, type and collation semantics.

## Base path enumeration

Every LogicalGet enumerates:

```text
PhysicalSeqScan
every usable single PhysicalIndexScan
```

Each alternative carries rows, width, Cost, ordering, required heap columns and residuals.

V1 chooses one index per base relation occurrence; bitmap intersection/union/index merge remain deferred.

The SeqScan/IndexScan break-even is cost-derived, never a fixed selectivity threshold.

## Cross-cutting additions

Pass 14 adds:

```text
§40.7 statistics/estimation/base-path diagnostics
§41.6 statistics/estimator/access-path verification
§42.5 ANALYZE/cost-calibration measurements
```

These retain the legacy architecture's verification/performance obligations without copying roadmap material into the core contract.

## Issues

New:

```text
R-044 OPEN      persistent sys_statistics byte grammar
R-045 RESOLVED  Pass-14 semantic ownership/completions
```

Previously open upper-layer gaps remain:

```text
R-036 catalog bootstrap / CATALOG_DATA physical representation
R-040 persistent default-expression encoding
```

## Coverage

```text
legacy §§0..627     complete / explicitly disposed
legacy §§628..725   pending
```

All 60 Pass-14 sections have explicit dispositions.

## Validation

Mechanical validation confirms:

- pinned legacy SHA unchanged,
- §568 begins at line 17113,
- §628 begins at line 18781,
- Chapters 33–36 occur exactly once,
- HLL/MCV/reservoir/histogram constants are preserved,
- SQL 3VL predicate estimator is explicit,
- sargability/composite-bound semantics are synchronized with Chapters 8/17/20,
- all coverage rows through §627 are non-PENDING,
- every coverage row from §628 remains PENDING,
- no production code or legacy architecture was modified.

## Exit status

**PASS 14 COMPLETE.**
