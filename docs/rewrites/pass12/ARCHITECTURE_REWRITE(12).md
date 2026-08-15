# Rewrite Pass 12 — Execution Foundations and Pipeline Model

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `434..478`
- processed source lines: `13318..14627`
- next untouched section: `479. Nested-Loop Join`
- legacy architecture modified: **no**
- production code modified: **no**
- join/aggregate/sort/DML execution §479+ migrated: **no**

Working architecture:

```text
input  SHA-256: d8b21d116d9545118f309fe19c7363dde48a9403162c65abfebb3419b7990513
output SHA-256: 57d039e21ed307cfa74b8fc8d78433215d67cf8081e88947efd6ae54a9f1e138
```

Project-state sanity check confirms the execution engine is not implemented yet; Pass 12 therefore changes architecture only and does not rewrite implementation history.

## Canonical chapters

Pass 12 replaces the physical-execution placeholders with:

```text
Chapter 22  Physical Plan and Runtime Operator Model
Chapter 23  Vectorized Data and String Representation
Chapter 24  Query Memory, Row Storage, and Spill
Chapter 25  Vectorized Expression Execution
Chapter 26  Pipeline Execution Model
Chapter 27  Scans and Unary Physical Operators
```

## Physical/runtime boundary

The architecture now has an exact separation:

```text
PhysicalOperator       immutable
GlobalOperatorState    per-execution shared
LocalOperatorState     per-worker/task
QueryExecutionContext  transaction/snapshot/read-epoch/memory/spill/cancel
```

No transaction-specific mutable state belongs in a reusable physical plan.

## DataChunk/vector model

Canonical standard vector size:

```text
1024 rows
```

with v1 chunk capacity constrained to `<= 65535` by uint16 SelectionVector indexing.

Vector kinds:

```text
FLAT
CONSTANT
DICTIONARY
```

Validity:

```text
1 = non-NULL
0 = NULL
uint64 words
```

Dictionary chains are composed into one effective selection before hot kernels.

UnifiedVectorFormat normalizes FLAT/CONSTANT/DICTIONARY per batch.

## String execution model

`StringRef` is:

```text
uint32 length
uint32 prefix
const char* data
```

Pass 12 completes `prefix` as the first up-to-four unsigned bytes packed big-endian and zero-filled.

Heap VARCHAR scan output copies bytes into the DataChunk StringHeap, so returned vectors never depend on heap-page pin lifetime.

Streaming borrowed vectors are allowed only while synchronous pipeline lifetime proves the owner remains alive.

Blocking/retaining boundaries deep-copy.

## Row/memory/spill model

Blocking operators use a query-temporary `RowLayout`, not persistent heap tuple headers.

`RowCollection` uses append-oriented ~256 KiB blocks.

Varlen row descriptors use offset/length into row/block-owned payload storage.

`QueryArena` owns small lifetime-bound objects but obtains accounted backing memory.

`QueryMemoryManager` owns per-query/global accounting, soft pressure, hard limits, operator reservations, and spillability.

Reservation failure is:

```text
spillable     -> spill/release/retry
non-spillable -> controlled OutOfMemory
```

Spill files are temporary, checksummed/self-describing, large-sequential-I/O oriented, never WAL logged, and never crash recovered.

They therefore are not given a long-lived database persistent-format compatibility contract in Pass 12.

## Vectorized expressions

Expression evaluation is batch-oriented:

```text
Evaluate(expression_state, DataChunk, active selection)
    -> Vector
```

Fixed-width kernels normalize input once, use all-valid fast paths, and avoid generic per-cell Value construction.

Comparisons preserve SQL NULL semantics.

AND/OR use row-subset short-circuiting so volatile/error-producing second operands are evaluated only where SQL semantics require them.

## Pipelines

Canonical runtime graph:

```text
Source
  -> streaming operators
  -> Sink
```

with explicit dependency DAGs around blocking operators.

Empty chunks are not EOF.

Single-worker execution is permitted initially, but immutable/global/local state separation is mandatory.

Query cancellation is query-wide and RAII-clean.

Pass 12 deliberately distinguishes QueryCancelled from safe pipeline early stop used by LIMIT.

## Scan integration

Sequential scan:

```text
BufferPool page
-> tuple header
-> MVCC visibility
-> optional planner-approved pushed predicates
-> historical schema-aware required-column decode
-> DataChunk
```

Index scan:

```text
B+ cursor
-> candidate RID batch
-> heap fetch
-> MVCC
-> decode
-> DataChunk
```

The QueryExecutionContext's read epoch protects retained candidate RIDs from physical reuse.

Initial RID batching preserves B+ order.

Forward PhysicalIndexScan may advertise `ASC / NULLS FIRST` when key-prefix/collation semantics match.

DESC is not claimed until reverse scan exists.

## Unary operators

PhysicalFilter prefers SelectionVector/dictionary output without cell copying.

PhysicalProject borrows simple input columns when safe and owns computed output storage.

PhysicalLimit uses rows_skipped/rows_emitted and requests safe pipeline early stop once its limit is satisfied.

PhysicalValues and the result-sink lifetime boundary are also made explicit enough for the pipeline foundation, while the detailed client/result interface remains Pass 13.

## Later-refinement check

Targeted later execution sections were inspected only for compatibility.

They confirm the Pass-12 foundations:

- hash build rows use RowCollection and deep-copy input varlen data,
- join/sort/DML spilling uses SpillManager,
- DML target materialization is a pipeline breaker,
- result/client boundaries cannot expose borrowed DataChunks,
- later parallel execution relies on global/local operator state,
- later execution invariants assume the same chunk/string/memory/pipeline contracts.

No §479+ algorithm body was migrated.

## Issues

No new unresolved persistent-format issue was introduced.

`R-041` records and resolves the process-local precision completions made in this pass.

Existing open upper-layer issues remain:

```text
R-036 catalog bootstrap / CATALOG_DATA format
R-040 persistent default-expression encoding
```

## Coverage

```text
legacy §§0..478     complete / explicitly disposed
legacy §§479..725   pending
```

All 45 Pass-12 sections have explicit dispositions.

## Validation

Mechanical validation confirms:

- pinned legacy SHA unchanged,
- §434 begins at line 13318,
- §479 begins at line 14628,
- Chapters 22–27 occur exactly once,
- standard vector size/capacity/selection rules are present,
- StringRef prefix semantics are exact,
- read-epoch integration is present,
- query-cancel and pipeline-early-stop are distinct,
- all coverage rows through §478 are non-PENDING,
- every coverage row from §479 remains PENDING,
- no production code or legacy architecture was modified.

## Exit status

**PASS 12 COMPLETE.**
