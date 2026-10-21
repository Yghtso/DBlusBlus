# 24. Query Memory, Row Storage, and Spill

Chapter 17 owns SQL scalar domains, and Chapter 20 owns relational row-count and
bag-multiplicity semantics, required ordering, and `LIMIT`/`OFFSET` semantics.
This chapter instead owns execution-resource policy: memory budgets, accounting
and hard limits, operator and `RowCollection` block targets, spill thresholds,
and temporary/runtime representation applicability govern whether and how an
execution can obtain finite resources. They do not redefine those upstream SQL
semantics or any persistent tuple/database format.

Resource policy and capability MAY determine whether execution succeeds or
reports a controlled resource failure under §39.3. For any two legal executions
that both succeed, differences in memory budget, spill threshold, block size,
spill path, exact retained-row representation, allocation layout, or block/run/
reload layout MUST preserve the owning operator's semantic result contract,
including scalar values, NULL state, row-occurrence multiplicity, required SQL
order, demanded semantic errors, transaction effects, and the persistent database
result. Owner-admitted result variation, including §29.3.4's tree-selected FLOAT64
SUM/AVG result, remains permitted; downstream operators apply their ordinary
semantics to that result. Resource policy introduces no additional variability.
`RowCollection` insertion/block order and
spill/run/reload order are physical and establish no SQL order unless the
canonical ordering contract of the owning operator requires that order.

## 24.1 Execution RowLayout

Blocking operators use a query-temporary row layout distinct from:

```text
persistent heap tuple layout
columnar DataChunk layout
```

A `RowLayout` is derived from resolved logical types.

It contains conceptually:

```text
null bitmap
fixed-width values
varlen offset/length descriptors
variable payload in row/block-owned storage
operator-owned optional metadata
```

Examples of operator-owned metadata include hash, duplicate-chain link, and partition/run identifier. A temporary row never carries persistent heap MVCC headers merely for query execution. Variable-length descriptors use offsets/lengths into row/block-owned storage rather than assuming a stable raw pointer inside a movable/serializable row image.

Every Chapter-24 size, count, offset, capacity, extent, reservation total, and
count-derived byte calculation denotes an exact mathematical nonnegative value.
Before any such value controls allocation, accounting, range construction,
pointer arithmetic, serialization, seek, read, write, or dereference, it MUST be
exactly representable in every runtime or temporary-storage domain that consumes
it. The rule applies at least to `count * width`, fixed bytes plus variable bytes,
`offset + length`, row and block extents, `live_total + requested_charge`, charge
subtraction, block/run/partition count growth, spill payload and record lengths,
file offset plus I/O length, allocation extents, and capacity rounding.

Signed overflow, unsigned wraparound, modulo reduction, narrowing truncation,
pointer-range overflow, and file-offset truncation are forbidden.
Implementations may satisfy this rule with checked arithmetic, wider exact
representations, prevalidated bounds, or an equivalent safe mechanism; no
concrete integer type is required.

## 24.2 RowCollection

`RowCollection` stores temporary rows in append-oriented blocks. Ordinary `RowCollection` blocks use a target size of: `256 KiB`.

This target is configuration/tuning state, not persistent format.

The target block size, ordinary `RowLayout`, and ordinary offset/length
descriptor domains are runtime representation choices, not correctness maxima,
SQL row or `VARCHAR` limits, SQL cardinality limits, or persistent heap-tuple
limits. An ordinary representation is applicable only when it can exactly
represent the retained row's size, values, offsets, and extents.

A row or value that an ordinary form cannot represent MUST NOT be truncated,
wrapped, clipped, rejected as invalid SQL, or semantically split. The retaining
owner MAY instead use a dedicated oversized allocation, an alternate exact
layout, physically segmented storage, an operator-specific exact retained form,
or another exact representation. Physical segmentation MUST preserve one
logical row occurrence and every exact scalar value. If another supported exact
form exists, execution MUST NOT fail merely because a known-incapable ordinary
form was selected. If no supported exact retained representation exists,
retention fails with `ExecutionError` carrying a runtime row/value
representability or resource-limit cause. If an exact supported form exists but
its representable allocation cannot be obtained, the failure is `OutOfMemory`.

The persistent heap tuple maximum is not a runtime/intermediate retained-row
maximum. Likewise, an exact runtime representation of a `VARCHAR` under §23.9
does not by itself establish that a narrower retained-row descriptor can store
it; the retaining owner must provide an exact retained representation.

Properties are:

```text
append-oriented
stable in-memory row handles while the collection/block lives
bulk deallocation
minimal per-row allocation
associated varlen blocks where required
```

A row handle is query-local. It is never a persistent RID. If data is spilled and later reconstructed, the spill representation/handle is separate from an in-memory row handle.

## 24.3 QueryArena

Each query owns an arena for small query-lifetime objects such as:

```text
expression state
operator/local-state objects
temporary key descriptors
small metadata arrays
pipeline graph nodes
```

Allocation is bump-oriented with bulk release at query end.

The arena is **not** the storage mechanism for unbounded hash tables, row
collections, retained tuples, groups, runs, partitions, large vector buffers,
large result spools, or other execution data whose aggregate capacity can grow
without an independent architecture bound.

Arena backing capacity is obtained/accounted as non-spillable query memory so
the arena cannot bypass the query memory budget merely because its individual
allocations are small. Objects contained in an already-accounted arena page need
not receive duplicate individual charges.

## 24.4 QueryMemoryManager

`QueryMemoryManager` owns execution-memory accounting across:

```text
one query
the database/global execution budget
operator reservations
spill pressure
hard-limit enforcement
```

Configuration exposes conceptually:

```text
global execution-memory limit
per-query soft budget
per-query hard maximum
```

Exact deployment defaults are not architecture constants.

The manager tracks at least:

```text
currently reserved/held bytes
peak query bytes
operator ownership
spillable versus non-spillable memory
```

Query-memory accounting measures logical committed capacity, not exact operating-
system RSS. Every process-memory region that becomes query-owned and whose
aggregate capacity can grow with query input, query shape, retained results,
operator state, rows, blocks, groups, runs, partitions, tasks, workers, or other
execution-dependent growth MUST be covered by a live accounted owner. This
includes, where query-owned, arena backing capacity; owned chunk, vector,
`StringHeap`, and selection capacity; `RowCollection` fixed and variable payload
and block metadata; dynamic block/run/partition directories; hash, aggregate,
`DISTINCT`, sort, DML, and result-spool state; spill read/write buffers; reloaded
spill blocks; and worker-local dynamic execution storage.

Accounting MAY occur at arena-page, allocation, buffer-capacity, block,
collection, reservation, operator-owner, or another exact owner-region
granularity. Each region has one conceptual accounted owner in each applicable
ledger. A parent region may own the charge for contained objects; duplicate
per-object charging of the same capacity is not required. Temporary conservative
overlap during an ownership handoff is permitted under §24.5, but every owned
capacity MUST remain covered. Individually small allocations are not exempt when
their aggregate execution-dependent growth is unbounded, and unbounded metadata
MUST have an accounted owner.

The charge for an owner region MUST be no smaller than the implementation-
controlled memory capacity committed and made available to that query-owned
region. Conservative over-accounting is permitted; systematic under-accounting
is forbidden. Hidden allocator metadata, virtual-memory implementation details,
page-table costs, and physical RSS need not be measured exactly.

An exemption is permitted only when another explicit architecture owner governs
and bounds the memory, or when aggregate capacity has an independent fixed
architecture bound unrelated to unbounded query execution growth. Persistent
BufferPool frame capacity is excluded because BufferPool is its separate bounded
owner; being a small allocation is never by itself an exemption.

For a growth grant subject to query and global hard gates, checking and updating
both live totals is one logically atomic accounting transition. Concurrent
requests MUST observe coherent totals and MUST NOT collectively oversubscribe a
hard gate through stale independent checks. The architecture does not prescribe
the synchronization mechanism.

## 24.5 MemoryReservation

Operators obtain tracked accounting ownership for query-owned dynamic capacity.
`MemoryReservation` is the conceptual grant/ownership mechanism; it does not
require one source-level API or object layout.

A reservation records at least:

```text
operator owner
bytes currently held
spillability
```

Examples include:

```text
hash build
aggregate state
sort
DISTINCT
DML target spool
result spool
large RowCollection
```

The lifecycle distinguishes four concepts:

```text
request:
    asks for capacity; creates neither a charge nor query-owned memory
accounting grant:
    creates budget authority and a live charge; does not prove allocation success
physical allocation:
    obtains provisional storage
live query-owned memory:
    storage exposed or retained as query execution state
```

Memory MUST NOT become live query-owned capacity before a sufficient charge
exists under every applicable ledger. Reserve-before-allocation is valid but is
not required. An allocator call may precede final accounting only while its
provisional storage remains private, is already covered by an existing charge or
an explicit bounded construction allowance, is neither exposed nor retained as
query state, and is immediately discarded if sufficient accounting cannot be
established. This flexibility MUST NOT create unbounded transient query-owned
memory or a hard-limit bypass.

If a logical growth request of `G` causes implementation-controlled committed
capacity `A > G`, the live charge MUST cover `A` before the additional capacity
becomes query-owned. The implementation may enlarge the grant, expose only the
covered capacity, discard or roll back provisional storage, or use an exact
equivalent mechanism. For in-place growth, the positive committed-capacity delta
is charged before it becomes live.

For an exact supported and representable form, an ordinary catchable physical
allocation denial or allocation exception becomes controlled `OutOfMemory`. If
allocation fails after accounting approval, the unused grant from that attempt
is released, existing owned memory and accounting remain coherent, and no
partially initialized allocation becomes live query state. An exact extent that
cannot be represented in the supported runtime size or allocator/address domain
instead follows the checked representability rule in §§24.1 and 24.8 and is not
`OutOfMemory`. Non-catchable operating-system or process termination is outside
the guarantee that execution returns a controlled database error.

Every live charge has one accounting owner and covers the full ownership
interval. Ownership transfer preserves continuous lifetime and accounting
coverage through a logically atomic handoff or conservative overlap; an
unaccounted gap is forbidden. If ownership leaves query execution, the receiving
canonical owner establishes its own lifetime and resource ownership.

Release occurs exactly once when ownership ends and updates each applicable
total exactly once. Double release, accounting underflow, or release of unowned
capacity is an internal invariant violation. No idempotent release API, C++
ownership idiom, or concrete reservation state representation is required.

## 24.6 Soft and hard pressure

The per-query soft budget is a pressure signal. Crossing it may request spill/reduction even when the hard limit is not yet reached. The hard query/global limit is an allocation gate.

When a requested growth grant cannot be made, an owner whose execution contract
provides an exact spill/reclaim action MUST attempt an applicable action. Such an
action may release eligible memory, spill state, reduce an exact
representation's live-memory requirement, advance a finite spill/repartition
state, or use an already-authorized operator-local exact fallback.

A retry is permitted only after state relevant to satisfying the denied request
has changed. Relevant progress includes reducing live accounted capacity,
reducing the required charge, freeing an eligible owner, replacing state with an
exact lower-memory form, or advancing a well-founded finite exact pressure state
that can make the request satisfiable. Repeating the same denied request,
writing spill bytes while freeing no useful capacity, or cycling through
equivalent state is not progress.

Each pressure cycle MUST do one of:

```text
obtain the grant
terminate with a controlled error or cancellation
advance a well-founded finite exact pressure state relevant to the denied demand
```

Execution MUST NOT enter an unbounded spill/reclaim/repartition/retry cycle with
no relevant progress. No fixed global retry count is required. If no exact
progress-producing action remains that can make the request satisfiable under
the applicable hard limits, execution terminates with controlled
`OutOfMemory`. Actual spill read/write or temporary-storage creation/capacity
failure, including `ENOSPC`, remains `SpillIOError`; cancellation remains
`QueryCancelled`.

Pressure handling preserves exact retained/input values, NULL state, bag
multiplicity, required ordering, demanded semantic errors, transaction behavior,
and the owning operator's result contract. It may select a different result
only where that contract explicitly admits bounded physical-reduction
variability, as in §29.3.4; this is not permission for approximation,
truncation, deduplication, row dropping, or required-order violation. This
protocol does not introduce general runtime
replanning, optimizer re-entry, or arbitrary physical-plan replacement; an
operator uses only exact actions already authorized by its execution contract.

Spilling/reclaim callbacks execute outside the QueryMemoryManager's internal accounting lock so an operator can safely release/re-request memory without recursive lock coupling.

The query MUST NOT rely on arbitrary `std::bad_alloc` as its normal memory-
pressure protocol. An ordinary catchable allocator denial for an exact supported
representable extent is normalized to `OutOfMemory`; an implementation MUST NOT
intentionally allow it to escape merely as uncontrolled process/runtime failure.

## 24.7 SpillManager

Every query owns one `SpillManager` for temporary execution files under a
dedicated managed temp location. Every live spill resource has fresh, exclusive,
temporary ownership sufficient to prevent collision with another live query,
statement attempt, process/database instance sharing the location, or stale
crash leftover. Filename syntax, identifier generation, directory layout, and
temporary metadata representation remain implementation choices.

It owns:

```text
query/attempt temp-resource naming and lifecycle
temporary block/run allocation
large sequential write/read helpers
cleanup on normal completion/error/cancel/abandoned-attempt teardown
```

Spill data is:

```text
temporary
not WAL logged
not part of crash recovery
not a persistent database format
```

Creating a spill resource MUST establish fresh ownership. A new owner MUST NOT
adopt, trust, or blindly overwrite a pre-existing object merely because a path,
query-local counter, run identifier, partition identifier, or process/query
identifier collides. On collision it establishes another fresh identity or
fails safely with `SpillIOError`. Blind adoption or overwrite despite this
ownership invariant is an internal defect.

Normal query/attempt teardown deletes every spill resource still owned by that
query or attempt on success, ordinary error, resource error, `SpillIOError`,
cancellation, and abandoned-attempt/retry teardown. A restarted attempt receives
fresh ownership and MUST NOT inherit an abandoned attempt's spill files as live
state.

A process crash aborts the query and removes its in-memory ownership, but spill
files may remain. Those leftovers are garbage temporary resources only: they are
never WAL-logged query state, crash-recovered state, new-query state, or
persistent database state, and no spill fsync or recovery requirement follows.

During continued healthy managed-temporary-storage operation, the subsystem
MUST eventually reclaim spill resources proven stale so known crash leftovers
cannot accumulate without bound. Reclamation may occur at temp-manager
initialization, through startup or periodic maintenance, during namespace-local
allocation/cleanup, or by another safe schedule; synchronous complete deletion
on every database open is not required.

Reclamation may delete only resources proven both to belong to the DBlusBlus
managed spill namespace and to have no live owner. Live-owned spill and unrelated
user or temporary files MUST remain untouched. The naming mechanism and
reclamation schedule remain implementation-defined subject to these safety and
eventuality rules; no durable spill identity, UUID, WAL, fsync, or persistent
recovery format is required.

## 24.8 Spill block contract

A spill block is self-describing and contains at least:

```text
magic
temporary format version
operator/run/partition identity
payload length
row count
CRC32C
payload
```

Integer serialization is explicit rather than native C++ object dumping. The CRC32C validates the encoded block metadata/payload according to the block format used by that operator.

Spill payload lengths, record/header-plus-payload extents, row/block/run counts,
offsets, and file offset plus I/O length obey the exact-before-use rule in §24.1.
If a record, run, file extent, encoded length, file offset, or I/O extent cannot
be represented by the supported temporary encoding or spill file/I/O domain,
the operation fails with `SpillIOError` carrying a spill
range/addressability/resource cause. If another exact supported spill form is
available to the owner, the known-incapable form is inapplicable.

Before decoded spill metadata controls allocation, pointer arithmetic, memory
dereference, seek, read, write, or range construction, the decoder validates all
applicable dependencies, including complete framing, recognized magic/version,
representable payload and record lengths, representable counts, exact fixed and
count-derived extents, offsets/lengths within their owning payload,
representable file/I/O ranges, CRC, and owner-specific structure. Equivalent
safe validation orderings are permitted, but no unvalidated decoded size, count,
or offset may drive allocation or memory/file access.

Self-generated temporary data remains subject to these checks because partial
I/O, process crash, stale leftovers, disk faults, external modification, or an
implementation defect can produce malformed bytes. The guarantee is memory-safe
controlled handling, not a durable recovery or hostile-input security contract.
A checksum, framing, count, offset, range, extent, or owner-structure mismatch is
a `SpillIOError`, not persistent database corruption. A self-generated in-memory
state that violates an already-established construction invariant remains an
internal defect.

Because spill data is query-temporary and never crash-recovered, the architecture does not assign it a long-lived database persistent-format compatibility promise. Specialized join, aggregate, and sort spill payloads are defined by their owning operator contracts in Chapters 28–30.

## 24.9 Spill I/O

Spill I/O favors:

```text
large sequential writes
large sequential reads
```

The operator-level I/O target is: `~1 MiB I/O blocks`.

This target is configurable. Operators buffer enough serialized rows/blocks to avoid issuing one tiny write per row. Spill buffers themselves remain query-memory accounted.

## 24.10 Error and cleanup boundary

The memory/spill subsystem reports controlled categories including:

```text
ExecutionError
OutOfMemory
SpillIOError
QueryCancelled
```

The category boundary is:

```text
hard-gate denial with no exact progress-producing action:
    OutOfMemory
catchable allocation denial for an exact supported representable form:
    OutOfMemory
unsupported in-memory runtime size/address extent:
    ExecutionError with representability/resource-limit cause
no exact retained-row/value representation:
    ExecutionError with representability/resource-limit cause
spill read/write, temporary capacity/ENOSPC, creation, range, or addressability failure:
    SpillIOError
malformed spill framing/CRC/count/offset/range/owner structure:
    SpillIOError
cancellation:
    QueryCancelled
double release, accounting underflow, or blind stale-spill adoption/overwrite:
    internal invariant violation
non-catchable operating-system/process termination:
    outside the controlled-return guarantee
```

Query cancellation or execution failure unwinds MemoryReservation, RowCollection, arena backing pages, spill files, and operator state through ordinary lifetime/RAII ownership.

Memory/spill cleanup does not commit or abort the SQL transaction by itself;
§39.1 applies each reported failure relative to the current statement's first
published database write.

## 24.11 Memory/spill invariants

1. Persistent heap tuple layout and temporary `RowLayout` are distinct; neither
   ordinary row/block size nor descriptor width is a SQL or persistent-format
   limit.
2. Query row handles are never persistent RIDs.
3. Retained `VARCHAR` payload is exact and owned by the retaining row/operator
   storage; incapable ordinary forms do not truncate or narrow it.
4. Every potentially unbounded query-owned capacity has a live accounted owner;
   many small allocations and metadata cannot bypass accounting.
5. Accounting uses implementation-controlled committed capacity, permits
   conservative overcharge, and does not require exact RSS or duplicate charging
   of capacity already covered by a parent region.
6. QueryArena is for bounded-purpose metadata/state, not unbounded operator data,
   and its backing capacity is accounted.
7. An accounting exemption is independently bounded or belongs to another
   explicit bounded owner such as BufferPool.
8. Applicable query/global hard-gate checks and updates are logically atomic.
9. Request, accounting grant, physical allocation, and live ownership are
   distinct; live query-owned capacity is continuously covered by sufficient
   accounting.
10. Allocator rounding and growth are fully charged before additional capacity
    becomes live; failed post-grant allocation releases its unused charge.
11. Ownership transfer preserves continuous accounting, and ownership release
    updates each ledger exactly once.
12. Retrying a denied growth request requires relevant well-founded progress;
    an equivalent no-progress pressure cycle cannot continue indefinitely.
13. No exact progress path and catchable failure to allocate a supported exact
    representable form yield `OutOfMemory`; representability failures remain
    distinct.
14. Pressure handling preserves exact SQL values, NULLs, bag multiplicity,
    required order, demanded errors, and transaction behavior without runtime
    optimizer re-entry.
15. Every Chapter-24 size, count, offset, capacity, extent, and accounting total
    is exact and representable before use; overflow, wrap, and narrowing are
    forbidden.
16. Spill metadata is structurally and arithmetically validated before it
    controls allocation, pointer/range access, or I/O.
17. Spill files are temporary, never WAL logged, never crash recovered, and
    never adopted as new query or persistent database state.
18. Each live spill resource has fresh exclusive ownership; collisions establish
    another identity or fail safely.
19. Normal teardown deletes owned spill, while proven stale managed crash
    leftovers are eventually reclaimed without deleting live-owned or unrelated
    files.
20. Spill serialization never dumps compiler-native pointer/object memory.
21. Spill I/O is block/sequential oriented rather than row-at-a-time.
22. Cancellation/error releases memory and temp resources through ownership
    semantics.
23. Memory/spill failure changes transaction state only through §39.1's current-
    statement publication boundary or an independent fatal classification.
