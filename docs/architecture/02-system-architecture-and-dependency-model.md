# 2. System Architecture and Dependency Model

## 2.1 Layered system model

The high-level system coordination and representation flow is:

```text
SQL / Parser
    ↓
Binder + Catalog
    ↓
Logical Plan
    ↓
Optimizer
    ↓
Physical Plan
    ↓
Execution Engine
    ↓
Tables / Indexes / Transactions
    ↓
Buffer Pool
    ↓
WAL + Page / File Management
    ↓
Operating System / Storage Device
```

In this overview, each downward arrow denotes a high-level handoff or service-consumption
relationship from the preceding stage to the stage below; it is not necessarily one runtime
call stack or a complete static module-dependency graph.

Lower infrastructure MUST NOT depend on unresolved higher-layer syntax, semantic
coordination, or policy objects, and receiving metadata does not transfer ownership of its
semantics. Resolved immutable descriptors, schemas, layouts, types, and identifiers MAY
cross a boundary only where the owning architecture contract explicitly permits them.

Examples:

- B+ tree code MUST NOT require SQL table syntax or parser objects.
- Buffer management MUST NOT require tuple-schema interpretation.
- WAL MUST NOT depend on SQL statement kinds such as `SELECT`.
- Parser code MUST NOT depend on physical page layout.
- Physical page parsers MUST NOT own transaction-visibility policy.
- SQL name resolution MUST NOT occur inside execution hot loops.

## 2.2 Separation of logical and physical concerns

The architecture maintains explicit boundaries between:

- SQL syntax and bound semantic objects,
- logical relational operators and physical algorithms,
- transaction-level logical locks and short-lived in-memory latches,
- persistent page identity and in-memory buffer-frame identity,
- tuple-format interpretation and page-local byte management,
- optimizer estimates and actual execution metrics.

These distinctions are cross-cutting requirements. Later chapters define their concrete representations and protocols.

## 2.3 Correctness and performance model

Correctness precedes micro-optimization, but performance-sensitive structure MUST remain visible from the start.

Architectural and performance analysis SHOULD explicitly consider:

- CPU cache behavior,
- branch prediction,
- allocation frequency,
- memory layout,
- pointer chasing,
- synchronization and contention,
- sequential versus random I/O,
- buffer-pool hit rate,
- WAL and durable-flush frequency,
- query cardinality,
- intermediate-result size,
- query memory pressure and spill behavior.

Measured hot paths MAY justify more complex algorithms or representations. Complexity SHOULD NOT be introduced solely from intuition when a simpler architecture satisfies the same correctness requirements and preserves a viable optimization path.

## 2.4 Shared-state and concurrency direction

The engine is single-process and multi-threaded. “Single-process” is an actively
enforced per-database invariant, not a deployment assumption; §3.3.2 defines
the exclusive database-owner lock and same-process owner rule.

Centralized global locks SHOULD NOT be used in hot paths merely to simplify synchronization.

The architecture must permit progressively partitioned shared state, including:

- sharded buffer-pool lookup structures,
- per-page latches,
- partitioned logical lock management,
- thread-local or query-local arenas,
- coordinated WAL append state.

A subsystem MAY use simple synchronization where contention can be reduced without replacing its ownership model.

## 2.5 Storage subsystem ownership and dependency direction

The storage stack is ordered from foundational providers to higher-level consumers:

```text
common definitions
    ↓
raw disk / file I/O
    ↓
buffer management
    ↓
heap pages / B+ tree physical pages
    ↓
relation / index abstractions
```

In this diagram, each downward arrow means that the lower layer consumes services or
definitions provided by the layer above.

Higher-level coordination builds on these layers rather than bypassing them.

The principal storage responsibilities are:

```text
DiskManager
    raw fixed-position file I/O and raw file lifecycle

BufferPool
    resident-page caching, pins, latches, replacement, dirty state, flush

HeapPage
    slotted-page mechanics over one caller-owned resident page buffer

HeapFile
    relation-wide heap operations across pages

FreeSpaceMap
    advisory candidate-page discovery for heap insertion

TupleCodec
    schema-directed tuple serialization and typed access

Visibility subsystem
    snapshot visibility of physical tuple versions

Table
    logical relation coordination across heap, indexes, and schema
```

These names describe architectural roles; exact C++ type names are not required by the contract.

Critical dependency rules:

- `HeapPage` MUST NOT perform raw file I/O or call `DiskManager` directly.
- Normal resident-page access flows through the BufferPool.
- `HeapPage` interprets one already-resident page and does not own a second page-sized allocation.
- `TupleCodec` is independent of page management.
- `TupleCodec` MAY depend on schema/type definitions but MUST remain independent of SQL parser AST representation.
- Transaction visibility MAY inspect tuple headers but MUST NOT be embedded in the physical heap-page parser.
- Buffer management MUST remain independent of heap tuple and B+ tree key semantics.

Concrete source-code directory and filename organization is not an architectural requirement. Persistent database namespace layout and lifecycle coordination are the separate contracts in §4.7.1–§4.7.10.
