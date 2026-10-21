# 22. Physical Plan and Runtime Operator Model

## 22.1 Execution architecture

The production execution engine is:

```text
vectorized
chunk-at-a-time
pipeline-oriented
memory-budgeted
spill-capable
parallel-ready
```

It consumes resolved physical plans and interacts with:

```text
transactional heap access
B+ tree access
immutable catalog descriptors
QueryMemoryManager
SpillManager
```

The execution layer MUST NOT perform:

```text
SQL parsing
SQL name resolution
catalog-name lookup in hot loops
cost-based search
```

Its performance purpose is to amortize branches, dispatch, synchronization, allocation, and tuple decoding across batches instead of paying those costs once per row.

## 22.2 Immutable physical plan versus execution state

The architecture distinguishes:

```text
PhysicalOperator
    immutable plan description

GlobalOperatorState
    mutable state shared by workers for one execution

LocalOperatorState
    mutable state owned by one worker/task
```

A physical plan MAY be cached or reused.

Runtime state is created per execution.

Transaction, snapshot, read-epoch registration, cancellation state, memory usage, cursors, and other execution-specific mutable state MUST NOT live directly inside an immutable physical-plan node.

## 22.3 Physical operator contract

Every physical operator describes at least:

```text
operator kind
children
output schema / LogicalSlotIds
required child inputs
physical properties
estimated row/cost placeholders
EXPLAIN metadata
```

Physical operators encode a selected execution algorithm.

For example:

```text
LogicalGet
    -> PhysicalSeqScan
    -> PhysicalIndexScan
```

but execution methods/state are not SQL AST methods and do not perform binding again.

The detailed optimizer rules that choose one physical operator are owned by the optimizer chapters.

## 22.4 Physical operator family

The execution architecture includes:

```text
PhysicalSeqScan
PhysicalIndexScan
PhysicalValues

PhysicalFilter
PhysicalProject

PhysicalNestedLoopJoin
PhysicalHashJoin
PhysicalIndexNestedLoopJoin
PhysicalMergeJoin

PhysicalHashAggregate
PhysicalSortAggregate
PhysicalDistinct

PhysicalSort
PhysicalTopN
PhysicalLimit

PhysicalScalarSubquery
PhysicalExistsSubquery
PhysicalInSubqueryBuild

PhysicalInsert
PhysicalUpdate
PhysicalDelete

PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
PhysicalVacuum
PhysicalAnalyze

PhysicalExplain
PhysicalResultSink
```

An operator is added when it represents a distinct physical execution algorithm or statement execution role.

The three subquery entries are the mandatory lazy side-plan roles in §20.14.7,
not parameterized/correlated operators. Detailed join/aggregate/sort/DML
execution algorithms are specified in Chapters 28–31.

### 22.4.1 Physical implementation availability

The physical operator family is the architecture's algorithm vocabulary;
runtime capability determines which algorithms are eligible for physical
planning.

The physical planner has an explicit capability/implementation registry.

It enumerates an alternative only when the corresponding runtime implementation is available and validated.

Eligibility is specific to the logical-plan instance. A physical algorithm is
eligible only when every algorithm-specific exact requirement is representable
and satisfiable by the selected implementation. For `PhysicalTopN`, this
includes the exact mathematical first-K requirement defined by §30.7. If that
implementation cannot represent and honor the exact K, `PhysicalTopN` is
ineligible for the plan instance; the SQL statement does not acquire a new
LIMIT/OFFSET validity condition or user-visible arithmetic error.

Physical capability affects plan shape, resource use, and performance, not the
validity or frozen semantics of a logical query for which another conforming
realization exists. Search MUST retain a conforming alternative when an
optional physical algorithm is ineligible. Cost may choose only among legal
alternatives and cannot change the owning semantic result contract, ordering,
mandatory errors, or transaction state. A result may vary only where that
contract explicitly defines a bounded family of legal results, such as
§29.3.4's FLOAT64 aggregate reduction trees; algorithm choice creates no
additional variability.

In particular:

```text
PhysicalHashJoin
PhysicalNestedLoopJoin
PhysicalIndexNestedLoopJoin
PhysicalHashAggregate
PhysicalSort
PhysicalTopN
PhysicalScalarSubquery
PhysicalExistsSubquery
PhysicalInSubqueryBuild
```

are baseline physical implementations. The subquery roles may reuse any
capability-enabled child algorithms, but a final plan cannot depend on a
semi/anti rewrite in place of their fallback.

Capability-conditional algorithms in their execution chapters include:

```text
PhysicalMergeJoin
PhysicalSortAggregate
ordered/streaming DISTINCT
```

They are costed/enumerated only when their runtime capability is enabled.

An unavailable algorithm can never appear in the final PhysicalPlan merely because the cost model has a formula for it.

## 22.5 Query execution context

Each execution owns one:

```text
QueryExecutionContext
```

containing or referencing at least:

```text
Transaction
effective statement/transaction Snapshot
ReadEpochGuard
QueryMemoryManager
SpillManager
query cancellation state
pipeline-local early-stop state
lazy statement-attempt subquery-state registry keyed by bound occurrence
profiling state
query-scoped arena
```

The `ReadEpochGuard` is held for the period in which execution may retain or dereference index-derived RIDs, satisfying Chapter 14's physical-RID reuse contract.

For READ COMMITTED, the effective snapshot is the statement snapshot.

For REPEATABLE READ, it is the transaction snapshot with the current command boundary.

The execution context does not weaken transaction-owned snapshot/lock lifetimes defined by Chapters 9–15.

## 22.6 Runtime ownership

The immutable plan owns semantic/physical configuration such as:

```text
resolved IDs
resolved types
physical expressions
bounds
operator properties
```

Runtime state owns mutable work such as:

```text
source cursors
reusable DataChunks
expression scratch
row collections
memory reservations
spill runs
counters
continuation state
subquery NOT_STARTED/RUNNING/READY/FAILED state and owned scalar/set payload
```

No runtime pointer may be persisted or placed into a catalog/storage format.

## 22.7 Physical properties

Physical nodes carry explicit property metadata; consumers MUST NOT infer
required or provided properties from operator names or incidental output.
Chapter 37 is the canonical owner of the physical-property taxonomy,
definitions, satisfaction rules, and operator preservation/provision rules.

The tracked property set is exactly:

```text
OrderingProperty
RequiredSlotSet
```

`OrderingProperty` represents required and provided row ordering.
`RequiredSlotSet` represents the exact `LogicalSlotId` values a planning
subproblem must provide. Sections 37.2–37.5 define their structures,
satisfaction, and baseline operator rules; Chapter 22 does not define a second
property system.

Other plan information remains distinct from physical properties:

```text
candidate/unique-key facts:
    exact logical/proof metadata under their canonical owners

partitioning / rewindability / materialization:
    reserved extension points, not tracked physical properties

blocking/streaming nature and spill support:
    algorithm capability and runtime-execution traits

estimated cardinality and estimated memory:
    estimates used by costing, never exact properties or proofs

RequiredRowsObjective:
    cost/search metadata under §38.16, not a physical property

PhysicalTopN exact K support:
    algorithm applicability/capability under §§22.4.1 and 30.7
```

An operator MUST advertise only ordering guaranteed by its execution contract;
incidental output from a particular execution strategy does not establish a
provided property.

## 22.8 Foundation invariants

1. Production execution is vectorized/chunk-at-a-time.
2. Physical plans are immutable after publication.
3. Per-execution mutable state is separate from the plan.
4. Global and local worker state are distinct.
5. Execution consumes resolved IDs/types/slots and never performs SQL name resolution.
6. Transaction/snapshot/read-epoch state is execution context, not plan state.
7. A physical operator represents an execution algorithm or resolved statement-execution role, not unresolved SQL syntax.
8. Maintenance operators such as PhysicalVacuum/PhysicalAnalyze consume resolved descriptors and preserve their owning maintenance/transaction protocols.
9. Physical properties are explicit plan metadata, not string/name inference.
10. Runtime state is query-lifetime/process-local and never persistent format.
