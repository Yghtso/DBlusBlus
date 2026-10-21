# 23. Vectorized Data and String Representation

## 23.1 Standard vector size and DataChunk

The basic execution batch is:

```text
DataChunk {
    capacity
    cardinality
    Vector columns[]
    chunk-local StringHeap
}
```

Each DataChunk exchanged by a physical operator realizes that operator's
ordered physical output schema from §22.3. For a physical output schema `S`:

```text
columns.size == S.width
columns[j]   = runtime vector for physical output schema entry S[j]
```

The runtime type of `columns[j]` MUST agree with `S[j]`. The semantic output
identity remains the `LogicalSlotId` carried by `S[j]`; column position `j` is
only the addressing position within that DataChunk. Equal-valued columns remain
distinct outputs when their schema entries have distinct `LogicalSlotId`s, and
projection reordering or derived-boundary remapping follows the corresponding
ordered output schema rather than payload equality or a prior ordinal.

For active logical row position `i`, `(i, j)` locates one runtime scalar
occurrence of schema entry `S[j]`. Neither row position `i` nor column position
`j` is a semantic identity. FLAT, CONSTANT, DICTIONARY, selection, and borrowed
representations change row-position resolution or storage/lifetime only; they
do not change the column's schema entry or `LogicalSlotId`. This positional
association does not require each Vector to store duplicate `LogicalSlotId`
metadata; §22.3 remains the canonical owner of the physical output-schema
identity mapping.

The standard vector capacity is: `STANDARD_VECTOR_SIZE = 1024`.

The constant is centralized and benchmark-configurable; operator code MUST NOT scatter literal `1024`. A DataChunk participating in operator execution MUST satisfy: `1 <= capacity <= 65535` so a uint16 SelectionVector can address every physical position.

Every normal column in one chunk has the same logical cardinality, with: `0 <= cardinality <= capacity`.

A final stream chunk may be partially filled.

`cardinality = 0` is an empty batch, **not** an end-of-stream marker and not
`FINISHED`.

Source completion is represented explicitly by runtime status.

An implementation MAY have a zero-capacity container as an uninitialized,
default, moved-from, or bookkeeping state. Such a container is outside the
executable DataChunk domain and MUST NOT be emitted or consumed as a normal
execution batch. Attempting to use one as an executable batch is an invalid
internal runtime state, not a public SQL error. Positive custom capacities
through 65535 remain permitted where the surrounding operator contract allows
them; object-construction and allocation mechanics are not architectural.

## 23.2 Why 1024

The standard size is large enough to amortize dispatch/selection/buffer/iterator overhead while keeping common vectors cache-friendly.

Representative sizes are:

```text
1024 INT64 values           8 KiB
1024 INT32 values           4 KiB
1024 uint16 selection       2 KiB
1024 validity bits          128 B
```

The chosen size is an execution tuning constant, not a persistent-format value.

## 23.3 Vector kinds

The vector representations are:

```text
FLAT
CONSTANT
DICTIONARY
```

### FLAT

Stores/references contiguous typed values for physical positions.

An active logical position `i` resolves to fully initialized payload and
validity position `i`.

### CONSTANT

Represents one scalar value repeated for all logical rows. The scalar may itself be NULL.

Its active logical positions remain `0..cardinality-1`, and each resolves to
the one scalar payload and repeated validity state at physical position zero.

### DICTIONARY

References another Vector plus one SelectionVector. It does not own an independent value array.

The logical row `i` maps through the dictionary selection to an active logical
position of its immediate child; that child's representation then resolves the
payload and validity position.

Representations such as:

```text
SEQUENCE
RLE
```

are outside the architecture baseline. The representation vocabulary is
intentionally limited to the three forms above.

## 23.4 Flat fixed-width vectors

For fixed-width types, a FLAT vector uses:

```text
naturally aligned contiguous data buffer
+
validity state
```

Physical execution elements are:

```text
BOOLEAN    uint8
INT32      int32
INT64      int64
FLOAT64    binary64/double
DATE       int32
TIMESTAMP  int64
```

Runtime BOOLEAN vectors deliberately use one byte per execution value and
are not bit-packed.

This choice is independent of persistent storage representation.

## 23.5 Validity mask

Validity uses one bit per physical value position:

```text
1 = valid / non-NULL
0 = NULL
```

Words are `uint64`.

For a standard 1024-row vector:

```text
16 words
= 128 bytes
```

A vector carries `all_valid` as a fast path. When `all_valid = true`, kernels do not need to read validity words. Bits outside the vector's active physical capacity/cardinality are semantically irrelevant and MUST NOT influence results.

For a CONSTANT vector, one scalar validity state is logically repeated. For a DICTIONARY vector, effective validity is the referenced child value's validity after selection composition.

## 23.6 SelectionVector

A standard SelectionVector stores: `uint16 indices[capacity]`.

A selection maps active logical position `i` to logical position `indices[i]`
of its immediate child vector or child view. It does not make allocated
capacity part of the child's active logical domain. Every selected index MUST
satisfy: `0 <= indices[i] < child logical cardinality`.

Resolution after that check is representation-specific: FLAT position `j`
uses initialized physical payload/validity position `j`; every valid CONSTANT
position uses scalar position zero; and a DICTIONARY position resolves through
that dictionary's selection and immediate child.

Repeated indices are legal repeated logical occurrences. Unsorted indices are
also legal; the listed selection order is the logical occurrence order. Neither
case performs implicit deduplication or sorting.

Every active logical position MUST resolve to fully initialized payload,
validity, and representation state. Allocated but inactive positions are
semantically inaccessible and MUST NOT be selected, even if stale physical
bytes remain after reuse or a partial fill.

An index outside the immediate child's active logical domain is an invalid
internal runtime representation. It MUST be prevented or rejected before any
payload, validity, StringRef, or child-mapping access. It never exposes a stale
value and is not a public SQL error. Construction invariants or explicit
boundary validation may enforce this rule; no particular validation API is
required.

Common uses include:

```text
filter selections
dictionary vectors
join match selections
chunk slicing
```

Identity selection is represented as metadata/view state rather than materializing `0,1,2,3,...` for every batch.

## 23.7 Dictionary composition

Repeated filtering/projection may otherwise produce:

```text
dictionary -> dictionary -> dictionary -> ...
```

The architecture does not permit unbounded lookup chains in hot kernels.

When composing:

```text
outer_sel[i] -> inner logical row
inner_sel[...] -> base physical row
```

the effective selection is flattened to the resulting base physical index.

Each outer selection addresses an active logical position of its immediate
child. Composition MUST validate every intermediate logical index against that
child view's cardinality before following or flattening it. Normalization MUST
NOT legalize an invalid intermediate index merely because an allocated payload
position exists.

Normalization occurs before a representation reaches a hot vector kernel when chain depth would otherwise exceed one effective indirection.

Dictionary normalization preserves:

```text
logical row order
effective validity
underlying value identity
```

without copying value payloads solely to remove indirection.

## 23.8 UnifiedVectorFormat

Vector kernels consume a normalized view conceptually:

```text
UnifiedVectorFormat {
    typed/base data pointer
    effective SelectionView
    effective ValidityView
}
```

Representation dispatch happens once per input vector/batch. A kernel therefore does not branch per row on: `FLAT vs CONSTANT vs DICTIONARY`.

For CONSTANT input, the effective selection maps every logical row to scalar position zero.

For DICTIONARY input, valid nested logical selections are composed into one
effective selection before the hot loop. UnifiedVectorFormat therefore
preserves the active logical domain and cannot expose allocated but inactive
payload positions.

UnifiedVectorFormat is an adapter/view; it does not become a persistent materialization format.

## 23.9 VARCHAR `StringRef`

The in-memory VARCHAR scalar reference is:

```text
StringRef {
    uint32 length
    uint32 prefix
    const char* data
}
```

and is naturally 16 bytes on a 64-bit process. `length` is the exact byte length.

The compact StringRef form exactly represents byte lengths through
`UINT32_MAX`. A value whose exact byte length exceeds `UINT32_MAX` is outside
this compact form's applicability; the length is never truncated, wrapped,
reduced modulo the field width, semantically split, clipped at a NUL byte, or
otherwise reinterpreted to fit StringRef. The logical value remains one exact
Chapter-17 byte string.

An implementation MAY provide another exact runtime representation for a
larger VARCHAR, provided it preserves Chapter-17 byte-string semantics, the
ownership/lifetime rules in this chapter, and downstream execution semantics.
No concrete alternate representation or integer width is required. When such
an exact representation is supported and available, a StringRef-only
realization is inapplicable to the over-domain value; execution MUST NOT fail
merely because an incapable compact realization was chosen.

If no supported exact runtime representation is available, attempting to
materialize the value raises controlled `ExecutionError` with a runtime
value-representability/resource-limit cause under §39.3. Actual failure to
allocate memory for an otherwise supported exact representation remains
`OutOfMemory` under Chapters 24 and 39.

This is a runtime representability/resource boundary, not a VARCHAR parser,
type, cast, corruption, or persistent-format maximum. In particular, the heap
tuple size limit is not a universal runtime VARCHAR limit, and StringRef does
not alter tuple, catalog, statistics, spill, or WAL formats.

No trailing NUL is required. For `length = 0`, `data` need not be dereferenced.

### 23.9.1 Prefix definition

`prefix` caches the first up to four string bytes as one unsigned big-endian 32-bit value:

```text
b0 b1 b2 b3
->
(b0 << 24) | (b1 << 16) | (b2 << 8) | b3
```

Missing bytes are zero-filled.

Examples:

```text
""      -> 0x00000000
"a"     -> 0x61000000
"ab"    -> 0x61620000
"abcd"  -> 0x61626364
```

Bytes are interpreted unsigned. A differing prefix may be used as a fast binary-order/equality rejection. An equal prefix is never sufficient proof of full-string equality/order; length/full byte comparison resolves the remaining case.

This agrees with Chapter 17's binary VARCHAR collation.

## 23.10 String ownership

A borrowed StringRef or vector view is valid only while its backing owner
remains alive, but owner liveness alone is insufficient. Every borrowed,
reference, dictionary, or selection-backed view denotes a **value-stable
logical view** for its declared borrow interval. All backing state required to
resolve that view MUST remain alive and observationally unchanged throughout
the interval.

That backing state includes, where applicable:

```text
active logical cardinality
type and representation metadata
reachable scalar payload and validity
selection storage and entries
dictionary child/base relationship
required backing addresses and ranges
StringRef length, prefix, and data reference
referenced VARCHAR bytes
```

The owner MUST NOT reset, reuse, overwrite, incompatibly reallocate, or mutate
reachable backing state in a way that changes the borrowed logical view.
Inactive storage unreachable from that view MAY change. Capacity itself need
not remain fixed when changing it preserves every required address, range,
mapping, and logical value.

Valid owners include:

```text
DataChunk StringHeap
query arena / stable query-owned constant storage
RowCollection/row blocks
blocking-operator build storage
sort/run storage
```

A StringRef MUST NOT outlive:

```text
an unpinned heap page
a reset/reused source chunk
temporary expression scratch
a spilled/in-memory block whose storage was released
```

Any operator retaining a VARCHAR beyond the current input-consumption lifetime
obtains a new valid owner or deep-copies the bytes into its own
query/operator-owned storage.

Retention beyond any declared borrow interval requires a new valid owner or
materialization/copy under these rules. Failure to allocate memory required by
that exact materialization remains the existing resource failure.

Access to an expired or observably unstable borrowed view is an internal
lifetime/aliasing invariant violation, not a public SQL error, and MUST be
prevented before dereferencing its backing state.

## 23.11 DataChunk StringHeap

Every reusable DataChunk owns chunk-local variable-length storage.

Heap scan decoding follows:

```text
copy VARCHAR bytes from tuple/page
    ↓
chunk StringHeap
    ↓
StringRef in output VARCHAR vector
```

This prevents downstream execution from extending heap-page pin lifetime merely to keep a string pointer alive. The StringHeap may be reset only when no downstream borrowed vector/chunk still references it.

## 23.12 Borrowed vectors

Streaming operators may return reference/dictionary vectors borrowing input storage when the pipeline guarantees synchronous downstream consumption before the owner is reset.

For the full synchronous borrow interval, payload, validity, selection state,
dictionary relationships, StringRef metadata, and referenced bytes reachable
through the borrowed view remain value-stable. Input/output aliasing is legal
only while every live alias continues to observe its required stable logical
values.

An implementation MAY satisfy this contract through immutable backing
storage, delayed mutation, ownership transfer, copy-on-write, retained valid
ownership, pinning under an already-defined owner contract, materialization,
deep copy, or another exact mechanism. Borrowing does not itself require
copying or prescribe an ownership mechanism.

Before a write makes one logical occurrence diverge from a shared CONSTANT or
DICTIONARY representation, the implementation MUST establish storage or a
representation capable of preserving every live logical view. A live
dictionary's selection, child/base relationship, and reachable values cannot
otherwise be mutated observably.

Typical cases:

```text
PhysicalFilter
simple column PhysicalProject
```

A blocking operator deep-copies retained data. A result/client boundary also materializes or otherwise safely retains its memory before asynchronous/client-visible lifetime can exceed the producer chunk. The pipeline executor owns this lifetime guarantee.

## 23.13 Chunk/vector reuse

Operators reuse DataChunk/vector buffers instead of allocating/freeing them for each batch.

A reusable output chunk may be reset only after its previous contents are no
longer borrowed or after every borrower has obtained independent valid
ownership through an exact mechanism. Reset/reuse MUST NOT mutate any reachable
backing state while a live borrower still depends on its value-stable logical
view.

Reset conceptually:

```text
cardinality = 0
clear/reinitialize vector logical state
reset chunk StringHeap
preserve reusable allocated capacity
```

Large reusable buffers remain memory-accounted through the query memory contract where applicable.

## 23.14 Vector/string invariants

1. Every DataChunk column has one common logical cardinality.
2. An executable DataChunk has capacity `1..65535` and cardinality `0..capacity`.
3. Empty chunk is not end-of-stream; a zero-capacity container is not executable batch state.
4. Standard selection indices are uint16 and chunk capacity never exceeds 65535.
5. A selection addresses active logical positions of its immediate child, never inactive allocated capacity.
6. Every selected and active logical position resolves to initialized payload, validity, and representation state.
7. Repeated/unsorted selections preserve listed occurrence order and multiplicity.
8. Invalid intermediate or final selections are rejected before dereference as internal invalid runtime representation, not public SQL errors.
9. Validity bit `1` means non-NULL and `0` means NULL.
10. Vector representation does not change SQL NULL semantics.
11. Dictionary chains are normalized to bounded effective indirection only after every intermediate logical index is valid.
12. UnifiedVectorFormat performs representation dispatch per batch, not per row.
13. BOOLEAN execution values are byte-per-value in the defined architecture.
14. Compact StringRef exactly represents byte lengths through `UINT32_MAX`, requires no terminator, and never truncates or wraps a larger value.
15. An exact alternate runtime VARCHAR representation is permitted; absence of any exact form is controlled `ExecutionError`, while allocation failure for a supported form is `OutOfMemory`.
16. StringRef prefix has the exact big-endian first-four-byte meaning in §23.9.1.
17. A borrowed view never outlives its owner and remains value-stable for its full borrow interval.
18. Reachable payload, validity, selection, representation, and referenced VARCHAR state cannot be observably mutated while borrowed.
19. Writes that diverge shared CONSTANT/DICTIONARY occurrences first establish a representation preserving every live logical view.
20. A chunk never exposes VARCHAR pointers into an unpinned heap page.
21. Blocking/retaining boundaries retain valid ownership or own/deep-copy varlen data.
22. Chunk reuse never resets or mutates storage that remains borrowed without first establishing independent stable ownership.

Malformed selection, borrow, or executable-chunk state MUST be prevented or
rejected before it can cause out-of-bounds access, stale-value exposure,
dangling access, arbitrary mutation, or persistent corruption. These are
internal invariant failures rather than normal SQL errors. Construction
invariants or boundary validation may enforce the valid-state contract; a
universal validation mechanism is not prescribed.
