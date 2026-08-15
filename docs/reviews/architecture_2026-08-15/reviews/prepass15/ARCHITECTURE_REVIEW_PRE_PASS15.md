# Pre-Pass-15 Architecture / Data-Structure / Gap-Timing Review

## Review boundary

Current working architecture:

```text
ARCHITECTURE_NEW_PASS14.md
SHA-256: 392972b682a4963bcd7b811757cbe81c24855589fc765f552e725900535352f6
```

Pinned legacy source:

```text
ARCHITECTURE(4).md
SHA-256: 2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86
```

Migration boundary:

```text
legacy §§0..627   complete / explicitly disposed
legacy §§628..725 pending
```

This task is a **review only**.

It does not:

```text
perform Pass 15
modify ARCHITECTURE_NEW_PASS14.md
modify legacy ARCHITECTURE(4).md
modify production code
change coverage statuses
```

The issue-register copy is updated only to record review findings/timing.

# 1. Executive verdict

## Architectural coherence

**STRONG, WITH A SMALL PRE-PASS-15 RESOLUTION GATE.**

Passes 10–14 compose well as one upper stack:

```text
catalog/schema/type semantics
    ↓
binder / expression IR
    ↓
logical plan / DDL-DML semantics
    ↓
physical execution / pipelines
    ↓
joins / aggregates / sort / DML runtime
    ↓
statistics / cardinality / base access costing
```

No subsystem redesign is required.

The major semantic axes are coherent:

```text
stable catalog IDs
historical schema decoding
MVCC-aware catalog visibility
LogicalSlotId distinct from persisted heap SlotId
SQL NULL/3VL semantics
FLOAT64 and binary-VARCHAR equality/order compatibility
GROUP BY/DISTINCT grouping equality
vector/string ownership
query-memory/spill ownership
index hit -> heap MVCC
offline DDL publication
DML Halloween protection
immutable plan/runtime-state separation
stable statistics snapshot
cost-driven base access selection
```

However, Pass 14 exposed one missing end-to-end maintenance command path and one important physical-cost distinction, while an estimator truth-state detail should be made exact before join search consumes it.

Existing upper-layer format gaps R-036/R-040/R-044 also have no later owner.

**Recommendation: do not start Pass 15 yet. Run one focused Pre-Pass-15 Upper-Core Resolution first.**

# 2. Mechanical/data-structure audit

The following checks passed:


```text
PASS  TypeId registry BOOLEAN..VARCHAR = 1..7
```

```text
PASS  Index fingerprint type-code registry BOOLEAN..VARCHAR = 1..7
```

```text
PASS  STANDARD_VECTOR_SIZE = 1024
```

```text
PASS  uint16 SelectionVector capacity bound <= 65535
```

```text
PASS  1024-row validity = 16 uint64 words / 128 bytes
```

```text
PASS  StringRef length/prefix/pointer layout present
```

```text
PASS  database.control canonical 88-byte slot header
```

```text
PASS  HLL p=14 / 16,384 registers
```

```text
PASS  MCV target 64
```

```text
PASS  reservoir target 100,000
```

```text
PASS  histogram target ~100 equi-depth bins
```

```text
PASS  all internal §x.y references resolve
```

Additional mechanical results:

```text
duplicate numbered H1 chapters: none
unresolved internal §x.y refs:   0
coverage §§0..627 pending:       0
coverage §§628..725 nonpending:  0
```

## 2.1 Identifier/type coherence

The built-in catalog TypeId registry and the B+ key-schema fingerprint type-code registry both currently use:

```text
1 BOOLEAN
2 INT32
3 INT64
4 FLOAT64
5 DATE
6 TIMESTAMP
7 VARCHAR
```

The architecture correctly treats the B+ codes as fingerprint-specific rather than silently deriving persistent bytes from a C++ enum.

DATE/TIMESTAMP widths and semantics remain compatible with tuple/index/statistics use.

## 2.2 Logical versus physical identities

The separation remains coherent:

```text
TableId
ColumnId
BindingId
LogicalSlotId
heap SlotId
RID
query RowCollection handle
```

No rewritten upper layer requires one identity to double as another.

## 2.3 Vector/string structures

The following remain mutually consistent:

```text
STANDARD_VECTOR_SIZE = 1024
uint16 SelectionVector
capacity <= 65535
1024 validity bits = 16 uint64 words = 128 bytes

StringRef:
    uint32 length
    uint32 prefix
    pointer
    naturally 16 bytes on 64-bit
```

Borrowed-vector and StringHeap lifetimes agree with scan/filter/project/join/result retention rules.

## 2.4 Control/catalog structures

The Pass-11 catalog-object allocator remains coherent with the canonical control-slot layout:

```text
control slot header = 88 bytes
reserved suffix      = 4008 bytes
total slot           = 4096 bytes
```

The stale earlier 80-byte registry entry is absent.

# 3. Cross-pass semantic coherence that PASSES

## 3.1 Historical schemas

Tuple scan/vacuum use:

```text
ResolveSchema(TableId, tuple.schema_version)
```

and catalog descriptors retain historical interpretation long enough for persisted versions.

Passes 10, 11, 12, and vacuum therefore share one schema-history owner.

## 3.2 SQL value semantics across binder/index/execution/statistics

The same semantic model is used for:

```text
binary VARCHAR ordering/equality
FLOAT64 zero/NaN normalization/equality/order
DATE/TIMESTAMP scalar order
NULL value state
GROUP BY / DISTINCT NULL grouping class
```

Statistics explicitly reuse executor/index ordering and query-hash semantics rather than defining a second value model.

## 3.3 Index visibility

The execution contract says:

```text
B+ entry
    -> candidate RID
    -> heap fetch
    -> MVCC visibility
```

and the cost model retains heap/MVCC work even for unique indexes.

This remains consistent with the physical `(user_key,RID)` index design.

## 3.4 DML/runtime boundaries

The Pass-13 target spool, one-target-once rule, no-retry-after-persistent-write rule, and RETURNING publication boundary remain consistent with Chapters 11/15/21.

Optimizer statistics/costing do not alter those semantics.

## 3.5 Immutable planning/runtime ownership

The stack maintains:

```text
immutable catalog descriptors
immutable logical plan
immutable physical plan
per-execution global/local operator state
stable optimizer statistics snapshot
```

No Pass-14 costing decision introduces mutable execution state into plan objects.

# 4. Gaps that SHOULD BE SOLVED NOW

## 4.1 R-036 — bootstrap / CATALOG_DATA format

**Solve before Pass 15.**

No optimizer section from §628 onward owns catalog bootstrap bytes or self-hosted catalog startup.

Waiting adds no information.

Pass 16 should reconcile formats, not invent this one for the first time.

## 4.2 R-040 — persistent default-expression format

**Solve before Pass 15.**

No later optimizer section defines:

```text
persistent expression-node grammar
stable persisted function/operator identity
literal Value encoding
reopen validation
```

This is already fully owned by the catalog/type/binder layer.

## 4.3 R-044 — persistent statistics payload

**Solve before Pass 15.**

Pass 14 now owns the complete statistics semantic model.

Pass 15 will consume statistics; it will not define their persistent serialization.

The resolution should preferably choose one atomic persistent StatsDescriptor model so table/column/index statistics from one ANALYZE generation cannot be mixed.

## 4.4 R-046 — ANALYZE end-to-end integration

**Solve before Pass 15.**

Current architecture says:

```sql
ANALYZE table_name;
```

exists, but the earlier SQL/runtime stack has no Analyze AST/logical/physical/control operator.

The resolution should add:

```text
parser/AST statement
bound target
LogicalAnalyze
PhysicalAnalyze
Chapter-31 maintenance execution
QueryMemoryManager integration
transaction-owned sys_statistics publication
global stats-cache publication only after COMMITTED
```

An ANALYZE that later rolls back must not globally publish its descriptor.

## 4.5 R-047 — physical index garbage pressure

**Solve before Pass 15.**

The executor correctly treats every physical B+ entry as a candidate and may reject it by MVCC.

Vacuum deliberately leaves index garbage until cleanup.

Base access cost currently has no exact meaning for `IndexStatistics.entry_count`, so it cannot clearly distinguish:

```text
visible selected rows
from
physical entries/RIDs examined
```

This matters before join DP begins comparing whole plans.

## 4.6 R-048 — exact primitive 3VL estimator triples

**Solve before Pass 15.**

The estimator has the correct three-valued representation but should lock primitive TRUE/FALSE/UNKNOWN mass before Pass-15 plan search compounds estimates through NOT/AND/OR and joins.

# 5. Items that SHOULD WAIT FOR PASS 15

The following are intentionally **not** defects in Passes 10–14 because legacy §628+ is their owner.

## 5.1 Physical property model

Wait for §§628–631:

```text
OrderingProperty
required output slots
ordering prefix satisfaction
interesting orders
memo alternative keys
```

Chapter 36 may advertise an index ordering today; Pass 15 should define exactly how that property satisfies requirements.

## 5.2 RelationSet / join graph / bushy DP

Wait for §§632–638.

The current optimizer front half stops correctly before the exact:

```text
BindingId bitset
join graph
bushy subset enumeration
exhaustive threshold
large-join heuristic
cartesian legality
```

contracts.

## 5.3 Join/operator costs and memory-aware planning

Wait for §§639–657 and §§689–698.

This includes:

```text
HashJoin build-side costing
hash memory/spill estimates
NestedLoop/INLJ/MergeJoin cost
Sort/TopN cost
HashAggregate/SortAggregate/DISTINCT cost
predicate CPU ordering
required-rows / LIMIT startup objective
pipeline-aware peak memory
```

### SortAggregate synchronization item

`PhysicalSortAggregate` is currently listed in the physical-operator family while Chapter 29 canonically implements HashAggregate and describes ordered alternatives only indirectly.

Legacy §650 explicitly says SortAggregate costing applies **when implemented**.

Pass 15 should make availability gating explicit rather than pretending every named physical operator is executable from day one.

This should be handled inside Pass 15, not invented in this review.

## 5.4 Missing-statistics fallback/confidence/provenance

Wait for §§666–668.

Pass 14 correctly allows missing/stale statistics.

Pass 15 owns the exact fallback guesses, confidence tags, and estimation provenance used in optimizer trace.

## 5.5 Cardinality floor versus LIMIT/startup objective

The source permits a near-one-row working floor for non-provably-empty estimates.

Pass 15 owns required-rows/startup/partial-consumption costing.

That is the right place to ensure the working floor does not destroy known operator upper bounds or LIMIT behavior.

# 6. Items that SHOULD WAIT FOR PASS 16 / IMPLEMENTATION

## 6.1 Rewrite-history wording

The working architecture still contains temporary rewrite metadata such as:

```text
"owned by Pass 13"
"Pass 12 does not..."
"legacy §513"
rewrite-status headings
rewrite-progress appendix
```

Some of this is already stale because those passes are complete.

It does not change database semantics, but final human architecture should remove it.

**Timing:** final Pass-16 document cleanup, unless convenient during the pre-Pass-15 resolution.

## 6.2 R-003..R-008 documentation classification

Development sequencing, source-tree guidance, detailed verification recipes, AI/workflow wording, and supporting-document preservation remain Pass-16/document-cutover concerns.

## 6.3 R-001 implementation mismatch

The RID decoder mismatch remains an implementation/state issue, not an architecture ambiguity.

Code remains frozen during the rewrite.

Fix after architecture cutover before ordinary implementation resumes.

# 7. Review of Pass-15 dependency boundary

The legacy source begins §628 with the physical-property model and then owns:

```text
ordering satisfaction
interesting orders
memo alternative key
RelationSet
join graph
bushy DP
large-join heuristic
join algorithm costs
memory/spill planning
required rows / LIMIT
memo dominance
stats fallbacks/confidence/provenance
optimizer trace/fingerprint
bounded planning time/memory
determinism
optimizer verification/performance
```

That confirms the boundary is correct.

It also confirms that the already-open:

```text
R-036
R-040
R-044
```

and new:

```text
R-046
R-047
R-048
```

will **not** become better specified by simply moving into Pass 15.

# 8. Recommended next task

Do **not** enter Pass 15 yet.

Run one focused:

```text
Pre-Pass-15 Upper-Core Resolution
```

with exactly this scope:

1. resolve R-036 bootstrap/CATALOG_DATA,
2. resolve R-040 persistent default-expression encoding,
3. resolve R-044 exact sys_statistics/StatsDescriptor persistence,
4. resolve R-046 ANALYZE end-to-end SQL/logical/physical/transaction publication,
5. resolve R-047 physical-versus-live index statistics/costing,
6. resolve R-048 primitive PredicateTruthEstimate 3VL formulas,
7. remove only stale Pass-10..14 meta wording encountered in the touched sections,
8. run a short post-resolution coherence/data-structure audit,
9. stop before §628.

Then Pass 15 can be a clean final optimizer-structure pass.

# 9. Review status

```text
ARCHITECTURE_NEW_PASS14 modified: NO
legacy ARCHITECTURE(4).md modified: NO
coverage modified: NO
production code modified: NO
Pass 15 performed: NO
issue-register copy updated: YES
```

**PRE-PASS-15 REVIEW: COMPLETE.**
