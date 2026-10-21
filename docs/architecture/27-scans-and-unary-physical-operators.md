# 27. Scans and Unary Physical Operators

## 27.1 Physical sequential scan

`PhysicalSeqScan` contains resolved scan configuration such as:

```text
TableDescriptor / TableId
required ColumnIds / output LogicalSlotIds
eligible planner-assigned pushed predicates
```

Runtime scan state obtains its effective snapshot/read epoch through the QueryExecutionContext. The scan walks heap pages in ascending physical PageNo order and slots in physical slot order. This physical traversal does **not** establish a SQL ordering property.

For each heap page:

```text
1. BufferPool fetch/read-latch
2. iterate valid tuple slots
3. inspect tuple header
4. apply exact MVCC visibility
5. evaluate eligible cheap pushed predicates
6. decode only columns still required
7. append surviving values to output DataChunk
8. release guard when the page is no longer needed
```

The scan does not decode every table column merely because the table contains it. Predicate dependency columns may be decoded before output-only columns so rejected rows avoid unnecessary decoding. When a tuple's persisted `schema_version` requires historical interpretation, scan decoding uses Chapter 16's: `ResolveSchema(TableId, tuple.schema_version)` contract.

## 27.2 Scan page and string lifetime

A scan MUST NOT return execution vectors containing pointers into a heap page after that page guard may be released.

Therefore:

```text
fixed-width tuple values
    -> copied to output vectors

VARCHAR tuple bytes
    -> copied to output DataChunk StringHeap
    -> StringRef points into chunk-owned memory
```

The heap page may then be unpinned as the scan advances. This prevents long pipelines from pinning pages solely for downstream string lifetime.

## 27.3 Scan predicate pushdown boundary

The physical planner may assign predicates to a scan only after their semantic pushdown safety has been established. The scan executes those assigned predicates. It does **not** discover additional SQL rewrite opportunities at runtime.

A scan-local predicate may reduce tuple decoding, output cardinality, and memory traffic. General predicate semantics remain representable by PhysicalFilter. The scan requests/decode dependencies needed by pushed predicates even when those columns are not final outputs.

## 27.4 Physical index scan

`PhysicalIndexScan` contains at least:

```text
IndexId / immutable IndexDescriptor
encoded lower bound
encoded upper bound
lower/upper inclusivity
required heap columns
output LogicalSlotIds
ordering property
```

Execution is:

```text
B+ range cursor
    ↓
candidate RID batch
    ↓
heap fetch
    ↓
MVCC visibility
    ↓
decode required columns
    ↓
output DataChunk
```

An index entry is only a candidate. It is never proof of SQL row visibility. The QueryExecutionContext's read-epoch guard protects candidate physical RIDs from reuse while execution may retain them.

## 27.5 RID batching

Index scan collects a small batch of candidate RIDs rather than performing one candidate/one returned row iterator step. The ordinary RID-batch tuning target is: `up to one DataChunk capacity`.

A RID-batching strategy preserves B+ cursor order while fetching candidates when the
PhysicalIndexScan advertises or must satisfy the original index ordering.

Alternative batching strategies MAY group heap fetches by PageId only when the physical
plan neither promises nor needs the original index ordering.

The batch is query-local temporary state and is never a persistent RID list.

## 27.6 Index ordering property

A forward B+ range cursor produces ascending physical user-key order.

Therefore a compatible PhysicalIndexScan may advertise an ascending ordering property when:

```text
index key prefix matches
binary collation/type semantics match
NULL ordering matches
requested direction is supported
```

For forward scan:

```text
ASC
NULLS FIRST
```

is the natural index property. MVCC filtering may remove rows but does not reorder the surviving candidate sequence. Native reverse scan remains deferred by Chapter 8, so DESC ordering is not claimed by the baseline PhysicalIndexScan merely by walking forward.

Physical planning—not executor guesswork—decides whether this property satisfies ORDER BY.

## 27.7 PhysicalFilter

PhysicalFilter evaluates its bound physical predicate over one input chunk/active selection.

Preferred output when ownership permits is:

```text
reduced cardinality
+
DICTIONARY/reference vectors over the input
```

rather than copying every surviving value. Filter preserves logical row order. If no row survives, `output cardinality = 0`, which is an empty batch, not end-of-stream. If downstream lifetime exceeds the input owner's lifetime, materialization occurs at the appropriate retaining boundary.

## 27.8 PhysicalProject

PhysicalProject evaluates projection expressions vectorized. For a direct input slot: `reference/borrow input vector` when lifetime permits.

For a computed fixed-width expression: `write/reuse flat output vector`.

For a computed VARCHAR:

```text
result bytes live in output-owned StringHeap/other valid result owner
```

Projection is streaming and is not a pipeline breaker.

Its output columns follow the declared ordered physical output schema. Project preserves
the input row-occurrence sequence; any advertised `OrderingProperty` remains governed by
§37.5.

## 27.9 PhysicalLimit

Runtime state tracks:

```text
rows_skipped
rows_emitted
```

OFFSET is applied before LIMIT. The operator may use selection/slicing instead of cell copying. Once the limit is satisfied, it requests `pipeline early stop` through §26.8 when no further upstream rows are required. It does not turn ordinary LIMIT completion into whole-query cancellation.

With no finite LIMIT, only OFFSET accounting applies.

## 27.10 PhysicalValues

PhysicalValues is a source over already bound/typed literal rows. It produces DataChunks up to standard capacity. Constant scalar/string payload owned by the immutable plan may be referenced only when its lifetime safely exceeds every execution consumer; otherwise values are copied into the output chunk.

PhysicalValues performs no SQL parsing/type resolution.

## 27.11 PhysicalResultSink foundation

A result sink consumes final DataChunks synchronously or materializes/retains result memory according to the client/result-interface contract in Chapter 31. It MUST NOT expose a borrowed producer chunk beyond the producer/owner lifetime.

The detailed cursor/client result interface and DML RETURNING spool are defined in Chapter 31.

## 27.12 Scan/unary invariants

1. Sequential physical page order is not a SQL ordering guarantee.
2. Scan visibility is always heap MVCC visibility.
3. Scans decode only required/predicate-dependent columns.
4. Historical tuple schema interpretation uses the catalog schema-version contract.
5. Scan output never points into an unpinned heap page.
6. Index entries are candidates and never bypass heap MVCC.
7. Index candidate RID lifetime is covered by the execution read epoch.
8. Index RID batching preserves every ordering advertised by the PhysicalIndexScan.
9. Forward baseline index ordering is ASC/NULLS FIRST when compatible.
10. Filter prefers selection/dictionary output when safe and preserves row order.
11. Project is streaming and computed varlen results own their bytes.
12. Limit uses pipeline early-stop rather than query cancellation.
13. Values/result execution does not redo parsing/binding/type resolution.
14. Client/result boundaries do not leak borrowed chunk lifetime.
