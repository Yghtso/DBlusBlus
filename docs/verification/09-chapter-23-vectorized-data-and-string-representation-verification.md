# Chapter 23 — Vectorized Data and String Representation Verification

This family verifies the complete runtime representation and lifetime contract in Chapter
23. It compares execution objects with declarative logical models; production Vector,
SelectionVector, validity, StringRef, and normalization code are never their own oracles.
Randomized variants record deterministic seeds, ownership tests use explicit generations
and poison patterns, and scheduling tests use barriers or finite state machines rather than
sleeps.

The independent test-side oracles are:

```text
DC  executable-DataChunk shape, cardinality, and owner-state model
SC  ordered physical-output-schema / LogicalSlotId mapping from V22-B
LV  recursive logical-vector resolver over FLAT, CONSTANT, and DICTIONARY
VV  independent validity bit/state model
SV  scalar Chapter-17 value, NULL, bag, and required-order oracle
BS  exact arbitrary-byte VARCHAR and §23.9.1 prefix oracle
LG  generation-tagged owner/borrower/value-stability graph
LM  arbitrary-precision VARCHAR-length and runtime-capability model
PM  finite source-status and operator-progress state machine
PF  runtime/persistent representation registry
EC  §39 error-category and pre-access side-effect oracle
```

`LV` contains logical cardinality separately from allocated capacity. Its lookup first
checks the immediate child's logical domain, then resolves representation-specific
storage. `LG` records every reachable backing component, borrow start/end, generation,
allowed mutation, and invalidation event. `LM` models lengths symbolically and does not
allocate multi-gigabyte strings. `PF` enumerates owner-defined persistent codecs rather
than searching produced bytes for coincidental patterns alone.

## V23-A — DataChunk shape and physical-schema handoff

Build `DC` with `executable`, `capacity`, `cardinality`, ordered schema `S`, columns, and
StringHeap owner state. For every exchanged executable chunk require:

```text
1 <= capacity <= 65535
0 <= cardinality <= capacity
columns.size == S.width
type(columns[j]) == type(S[j])
```

`SC`, not a Vector field or value comparison, supplies the semantic identity of
`columns[j]` as `S[j].LogicalSlotId`. Exercise `SELECT a,a`, the same expression twice, a
self join with equal values, reordered Project, a derived-table remap, repeated aggregate
expressions, and borrowed, CONSTANT, and DICTIONARY columns. Permute ordinals while
updating `S`: consumers must follow `SC`. Equal values do not merge identities, and row
position `i`, column ordinal `j`, pointer identity, TypeId equality, or an ordinal from a
different schema is never accepted as semantic identity. The conformance oracle does not
require each Vector to store a LogicalSlotId.

| Fixture | Physical schema entry | LogicalSlotId source | DataChunk ordinal/type | Representation | Identity result |
|---|---|---|---|---|---|
| source | resolved source `S[j]` | `S[j]` | exact `j` / matching | FLAT/borrowed | preserved |
| Project | declared output `S[j]` | declared output slot | reordered `j` / matching | any kind | follows output schema |
| duplicate Project output | two declared entries | two distinct slots | two ordinals / matching | equal FLAT/CONSTANT values | distinct |
| self join | left-then-right entries | side-specific slots | separate ordinals / matching | equal payloads allowed | distinct |
| aggregate | group/aggregate entry | declared output slot | exact `j` / matching | any kind | preserved |
| repeated aggregate | two output entries | occurrence-specific slots | two ordinals / matching | shared computation allowed | distinct |
| derived table | boundary `S[j]` | declared fresh/remapped slot | exact `j` / matching | any kind | follows remap |
| Filter | pass-through `S[j]` | unchanged | unchanged / matching | DICTIONARY allowed | preserved |
| Sort | pass-through `S[j]` | unchanged | unchanged / matching | any kind | preserved |
| Limit | pass-through `S[j]` | unchanged | unchanged / matching | sliced/DICTIONARY | preserved |
| DICTIONARY column | producer `S[j]` | unchanged | exact `j` / matching | DICTIONARY | row map only |
| borrowed vector | producer `S[j]` | unchanged | exact `j` / matching | reference view | preserved; V23-G owns lifetime |

## V23-B — Executable capacity, cardinality, empty batches, and progress

The boundary oracle constructs positive and negative `DC` states without using an operator
allocator as the legality test:

| Capacity | Cardinality cases | Executable/valid | Empty/EOS | Selectable rows | Progress requirement | Classification |
|---:|---|---|---|---|---|---|
| 0 | 0 | no; bookkeeping only if provided | not an execution batch/status | none | cannot be emitted | internal invalid if used |
| 1 | 0, 1, 2 | yes for 0..1 | zero is empty, not EOS | exactly cardinality | empty HAVE_MORE advances state | 2 invalid |
| 1024 | 0, 1, 1023, 1024, 1025 | yes for 0..1024 | zero is empty, not EOS | exactly cardinality | same | 1025 invalid |
| 65535 | 0, 1, 65534, 65535, 65536 | yes for 0..65535 | zero is empty, not EOS | exactly cardinality | same | 65536 invalid |
| 65536 | 0 and nonzero | no | no executable state | none | cannot be emitted | internal invalid state |

Verify that all normal columns use the common chunk cardinality, partial final chunks are
legal, `STANDARD_VECTOR_SIZE` is 1024, and other positive capacities through 65535 remain
conforming where an operator accepts them. A zero-capacity default/moved-from/bookkeeping
object, when present, is tested only as non-executable state; no constructor or allocator
shape is required. Capacity and chunk row count never become a public SQL limit or error.

Drive `PM` with explicit `(source_state,status,cardinality,next_state)` transitions:

```text
S0 -> (HAVE_MORE, 0) -> S1 -> (FINISHED, no batch)
```

is valid because finite state advanced. A self-loop

```text
S0 -> (HAVE_MORE, 0) -> S0
```

is rejected as a no-progress implementation/liveness violation. Also cover nonempty
`HAVE_MORE` and `FINISHED`; status, not cardinality, is the EOS oracle. A positive-capacity
empty child has no legal SelectionVector target.

## V23-C — Vector representations and substitutability

Represent one logical sequence in every applicable form. `LV` resolves FLAT logical
position `i` to initialized payload/validity position `i`; CONSTANT positions
`0..cardinality-1` all resolve to physical scalar position zero; DICTIONARY positions
resolve through the selection to the immediate child's logical domain. Cover cardinality
0, 1, and N, non-NULL and NULL constants, empty/all-NULL/mixed inputs, and arbitrary-byte
VARCHAR values.

For a CONSTANT of cardinality N, `SV` counts N occurrences even though one payload exists.
For `[x,x,x,x]`, request a write making logical occurrence 2 equal `y`: the resulting
storage/representation may vary, but it must resolve to `[x,x,y,x]` while every live alias
retains its required value. Remaining CONSTANT while claiming only one occurrence changed
is rejected.

| Pair | Value/NULL | Cardinality | Order/multiplicity | VARCHAR bytes | Error/lifetime |
|---|---|---|---|---|---|
| FLAT ↔ CONSTANT | equal logical sequence | equal | equal | exact | same semantic error; owner valid |
| FLAT ↔ DICTIONARY | equal logical sequence | equal | listed selection order | exact | same |
| CONSTANT ↔ DICTIONARY | equal repeated sequence | equal | duplicates retained | exact | same |
| nested ↔ normalized DICTIONARY | equal recursive resolution | equal | no reorder/dedup | exact | same |
| owned ↔ borrowed | equal while borrow is valid | equal | equal | exact | V23-G stability |

SEQUENCE and RLE are negative vocabulary fixtures: they are not accepted as Vector
kinds. UnifiedVectorFormat is checked as a non-owning adapter with typed/base data,
effective selection, and effective validity. It maps CONSTANT rows to position zero,
composes only valid dictionaries, preserves active logical cardinality, and yields the same
`LV` sequence without becoming a persistent format. Instrumentation verifies
representation dispatch at vector/batch granularity rather than a per-row kind branch; no
particular adapter class or flattening algorithm is required.

## V23-D — SelectionVector and dictionary composition

For each active selection entry, calculate legality from
`0 <= index < immediate_child.logical_cardinality`; allocated capacity is deliberately
different in negative fixtures. Poison inactive payload, validity, StringRef, and child-map
storage and require rejection before any poison read.

| Case | Child cardinality / capacity | Selection | Valid? | Effective payload | Occurrence behavior | Oracle |
|---|---|---|---:|---|---|---|
| FLAT active | 3 / 8 | `[2]` | yes | initialized FLAT slot 2 | one `child[2]` | `LV/VV` |
| FLAT inactive | 1 / 1024 | `[7]` | no | none; pre-access rejection | internal invalid state | poison/read counter |
| CONSTANT active zero | 4 / 4 | `[0]` | yes | scalar slot 0 | one occurrence | `LV` |
| CONSTANT active greater than zero | 4 / 4 | `[3]` | yes | scalar slot 0 | one occurrence | `LV` |
| CONSTANT cardinality zero | 0 / positive | `[0]` | no | none | internal invalid state | bounds predicate |
| DICTIONARY valid child | 3 / 8 | `[2]` | yes | recursively resolved | selected child occurrence | recursive `LV` |
| DICTIONARY inactive child | 1 / 8 | `[7]` | no | none | internal invalid state | poison/read counter |
| nested valid | each intermediate in range | outer/inner fixture | yes | composed base position | exact recursive sequence | recursive `LV` |
| nested invalid intermediate | inner index inactive but capacity-resident | outer/inner fixture | no | none | reject before base lookup | intermediate bounds model |
| repeated | 3 / 8 | `[2,2,0]` | yes | `2,2,0` | duplicate occurrence retained | `SV` multiset |
| unsorted | 3 / 8 | `[2,0,1]` | yes | `2,0,1` | listed order retained | `SV` sequence |

Generate valid and one-defect nested dictionaries. The independent composition oracle
performs each intermediate bounds check before following it. Compare recursive and
normalized results for values, validity, cardinality, order, multiplicity, and VARCHAR
bytes. Normalization may remove indirection but must not legalize a capacity-resident
inactive index, sort, deduplicate, or copy payload solely to remove indirection. Chain-depth
instrumentation verifies bounded effective indirection before a hot kernel without fixing
the representation of the composed selection.

An out-of-domain final or intermediate index is an internal invalid representation, not a
public SQL error. The fixture asserts zero payload/validity/StringRef/child-map reads and no
result, DML, WAL, or persistent side effect. Either construction invariants or explicit
boundary validation may establish the precondition.

## V23-E — Validity and fixed-width runtime storage

`VV` stores one independent valid/NULL state per physical position with `1 = non-NULL` and
`0 = NULL`. Verify uint64-word grouping, the 16-word/128-byte standard-vector calculation,
`all_valid=true`, mixed words, all NULL, and poison bits outside active cardinality. The
fast path and ordinary mask must resolve identically. A NULL payload is poison and is never
interpreted; NULL VARCHAR likewise never dereferences its StringRef payload. CONSTANT
validity repeats one state N times, while DICTIONARY validity follows the selected child
through repeated, unsorted, and nested selections.

| Type | Required runtime element | Boundary/semantic cases | Persistent inference forbidden |
|---|---|---|---|
| BOOLEAN | uint8, one byte per value, not bit-packed | FALSE, TRUE, NULL | tuple byte order/layout |
| INT32 | int32 | min, max, NULL | native bytes as codec |
| INT64 | int64 | min, max, NULL | native bytes as codec |
| FLOAT64 | binary64/double | infinities, multiple NaNs, ±0, NULL | runtime NaN bits as persisted form |
| DATE | int32 | signed-domain boundaries, NULL | host endianness |
| TIMESTAMP | int64 | signed-domain boundaries, NULL | host endianness |

The Chapter-17 scalar oracle, not raw bits, determines SQL equality/order/grouping. NaN is
never a NULL sentinel; ±0 and canonical-equivalent NaNs retain Chapter-17 semantics even
when runtime copies preserve distinct source bits. Runtime alignment and contiguous FLAT
storage are inspected process-locally; persistent codecs remain separate.

## V23-F — StringRef byte and prefix semantics

`BS` owns `(is_null, exact byte sequence)` independently of StringRef. Cover NULL, non-NULL
empty, one through four bytes, longer strings, `b"a\0b"`, and bytes `00`, `7f`, `80`, and
`ff`. No fixture uses `strlen` or a terminator. Pointer/address perturbation cannot change
equality, unsigned lexicographic order, grouping, hashing, or copying.

The prefix oracle computes:

```text
(uint32(b0) << 24) | (uint32(b1) << 16) | (uint32(b2) << 8) | uint32(b3)
```

with absent bytes zero-filled. Verify lengths 0, 1, 2, 3, 4, and greater than 4,
unsigned interpretation, and the examples in §23.9.1. Different prefixes may short-reject;
equal prefixes must still use length/full-byte comparison. Non-NULL length zero remains
distinct from NULL and never requires dereferencing `data`.

Inspect the compact runtime form for exact `uint32 length`, `uint32 prefix`, and non-owning
data reference, including its natural 16-byte size on a 64-bit process. This ABI observation
is process-local and is never reused as a persistence oracle.

| String case | Compact representable? | Exact value/prefix | Ownership | Expected error | Persistent? | Oracle |
|---|---:|---|---|---|---:|---|
| NULL | payload ignored | NULL; no prefix dereference | validity owner | none | only owner codec | `VV/SV` |
| non-NULL empty | yes | length 0 / prefix 0 | valid owner; data not dereferenced | none | no runtime pointer | `BS` |
| embedded NUL `a\0b` | yes | length 3 / `0x61006200` | valid byte owner | none | exact bytes via codec only | `BS` |
| one byte `ff` | yes | length 1 / `0xff000000` | valid byte owner | none | no runtime pointer | `BS` |
| four bytes | yes | exact four-byte prefix | valid byte owner | none | no runtime pointer | `BS` |
| greater than four bytes | yes within limit | first four only; full bytes decide | valid byte owner | none | no runtime pointer | `BS` |
| `UINT32_MAX` | yes | exact symbolic length/prefix | supported compact owner | none subject to allocation | no runtime ABI | `LM/BS` |
| `UINT32_MAX+1` | no | one exact symbolic byte string | alternate owner if available | capability-dependent below | no runtime ABI | `LM/BS` |
| exact alternate supported | compact no | exact full value | alternate's valid owner | none from compact choice | owner-defined temporary only | capability oracle |
| exact alternate unavailable | no | no partial value | none | ExecutionError representability/resource | no | `LM/EC` |
| supported form allocation fails | form-dependent | no partial value | allocation attempted | OutOfMemory | no | injected allocation + `EC` |

## V23-G — Value-stable borrowing and ownership

Create an `LG` node for payload, validity, selection storage/entries, dictionary
child/base relation, active cardinality, type/representation metadata, required address
ranges, StringRef metadata, and referenced bytes. Publish a borrowed view, attempt exactly
one mutation before borrow completion, and compare the borrower to its value-at-publication
`LV`/`BS` result.

| Mutation/mechanism | Allowed during live borrow? | Required borrower result | Oracle/classification |
|---|---:|---|---|
| reachable FLAT payload overwrite | no observable change | original value | `LG` + poison; internal violation otherwise |
| reachable validity change | no observable change | original NULL state | `LG` + `VV` |
| selection entry/storage mutation | no observable change | original values/order | `LG` + `LV` |
| dictionary child/base mutation | no observable change | original recursive view | `LG` + `LV` |
| StringRef length/prefix/data mutation | no observable change | original exact bytes | `LG` + `BS` |
| referenced-byte mutation | no observable change | original exact bytes | poison replacement + `BS` |
| incompatible reallocation | no dangling/remap | original logical view | generation/address-range graph |
| owner reset/reuse | only after independence/end | original view until end | generation graph |
| inactive unreachable mutation | yes | no effect | reachability proof + `LV` |
| immutable/delayed mutation | yes | original view | event order |
| materialization/deep copy | yes | independent equal value | owner-generation split |
| ownership transfer | yes | transferred owner remains valid | ownership graph |
| copy-on-write | yes | borrower sees old; writer sees new | two-owner graph |

Owner allocation alone is insufficient: all reachable resolving state must remain alive
and observationally unchanged. Capacity may change only when every required address, range,
mapping, and logical value remains stable. A synchronous zero-copy fixture completes the
consumer before reset and requires no copy. An asynchronous queue fixture must retain a
valid owner or materialize before producer reuse; deterministic barriers expose premature
recycling. Retention beyond the declared interval, blocking storage, and ResultSink/client
output likewise require a new valid owner or exact materialization. Allocation failure for
required materialization follows the existing resource procedure.

For shared CONSTANT/DICTIONARY state, a writer first establishes storage capable of the
divergent values while preserving every live view; the test accepts COW, transfer,
materialization, immutable retention, or delayed mutation and rejects a live mutable view.
No one mechanism is required. An expired or value-unstable borrow is an internal
lifetime/aliasing violation rejected before dereference, not a public SQL error.

## V23-H — StringHeap, reset, and stale-state exclusion

Scan a page-backed VARCHAR into a chunk StringHeap, release the page guard/pin, poison the
page, and require exact downstream bytes. This composes with the Chapter-27 scan procedure
and does not authorize raw page-pointer retention. Chunk-owned bytes remain valid until
reset/reuse and until every borrower has completed or obtained independent ownership.

Run these deterministic reuse sequences with distinct poison generations:

1. large cardinality -> consume -> reset -> smaller cardinality;
2. small cardinality -> reset -> larger cardinality;
3. DICTIONARY -> reset -> new selection;
4. VARCHAR StringHeap -> reset/reuse -> new strings.

Reset must set cardinality to zero, clear/reinitialize logical vector state, reset the
StringHeap, and may preserve allocated capacity. Old payload, validity, selection entries,
and StringRefs outside the new active domain are inaccessible; every newly active position
has freshly defined payload, validity, and representation state. Large retained buffers
remain covered by the Chapter-24 accounting procedure. Attempted reset while a live
borrower depends on reachable state must be delayed or preceded by independent ownership.

## V23-I — Large VARCHAR exact representability and resource errors

Use `LM` with symbolic lengths `0`, `1`, `UINT32_MAX-1`, `UINT32_MAX`,
`UINT32_MAX+1`, and a much larger finite integer. The fixture carries an abstract exact
byte generator/hash witness so equality and no-truncation can be checked without allocating
the represented length.

| Length/capability | Compact exact? | Required behavior | Error oracle |
|---|---:|---|---|
| 0, 1, `UINT32_MAX-1` | yes | exact single byte-string value of stated length | none subject to resources |
| `UINT32_MAX` | yes | exact maximum compact length | none subject to resources |
| `UINT32_MAX+1` or larger | no | compact form inapplicable | never narrow/modulo/split |
| over-domain, exact alternate supported | no | choose supported exact form and preserve semantics/lifetime | no compact-choice failure |
| over-domain, no exact form supported | no | controlled `ExecutionError` | representability/resource-limit cause |
| exact supported form, allocation denied | form-dependent | fail allocation | `OutOfMemory` |
| any truncation/wrap/NUL clipping | no | reject implementation result | nonconforming representation |

The capability fixture allows but does not require an alternate large-string form and
does not prescribe width, chunking, rope, LOB, or API shape. If one exists, deliberately
offer the incapable compact form and the capable exact form; representation choice must
not manufacture a public failure. If none exists, verify the exact §39.3 ExecutionError
cause and exclude ParserError, TypeError, CastError, corruption, invalid-VARCHAR, and
OutOfMemory classifications. Separately inject allocation failure into a supported exact
form to prove the OutOfMemory distinction.

Use a symbolic runtime/intermediate value larger than the heap inline tuple limit but no
larger than `UINT32_MAX`; it remains a legal runtime VARCHAR subject to resources. A later
attempt to persist it uses the owning tuple/DML rule. This proves neither the tuple maximum
nor compact StringRef narrows Chapter-17's arbitrary-finite-byte domain.

## V23-J — Runtime/persistent boundary

Maintain a negative persistence registry for DataChunk, Vector objects, SelectionVector,
validity words as native ABI, UnifiedVectorFormat, StringRef pointers, borrowed handles,
row handles, and arena addresses. Instrument heap tuple, PersistedScalarV1, catalog,
statistics, WAL, pages, and durable metadata writers: none may serialize these runtime
objects or pointer bytes. DML encodes scalar content through owner-defined persistent
codecs. Spill remains temporary but uses its own explicit offsets/lengths/value encoding;
reload reconstructs ownership and never relies on an in-memory pointer or host endianness.

The registry uses format-field provenance and encode/decode round trips, not accidental
absence of a numeric byte pattern. Runtime fixed-width vectors need no architectural byte
order; persistent codecs independently prove their required byte order.

## V23-K — Operator and pipeline handoffs

Reuse the detailed execution families with V23 perturbations:

- Filter compares TRUE/FALSE/UNKNOWN through the Chapter-17 oracle, preserves selected
  occurrence order/multiplicity, may return a positive-capacity empty batch, and does not
  turn that batch into FINISHED.
- Project maps columns through its physical output schema; pass-through borrowing obeys
  V23-G and computed VARCHAR values own sufficient storage.
- Join compares legal implementations with duplicate selections, LEFT null extension,
  poison NULL payloads, and retained-value ownership.
- Aggregate/DISTINCT use their grouping oracle for NULL, NaN, ±0, and exact VARCHAR bytes;
  pointer identity, vector kind, and NaN payload bits are not group identity.
- Sort/Top-N own retained strings, preserve the required cross-chunk comparator order, and
  acquire no chunk-boundary tie-breaker.
- Limit applies the Chapter-20 occurrence oracle while trimming/skipping chunks; partial or
  zero-cardinality output exposes no inactive position and does not imply EOS.
- Subquery/materialization, ResultSink, and asynchronous boundaries retain an owner or
  materialize before input-chunk lifetime ends.
- DML tuple writers encode values, never runtime pointers or selection mappings.

V26-A/D supply explicit `HAVE_MORE`/`FINISHED` and progress events; V26-G/H/K and
Pipeline Finalization and Resource Tests supply dependency, cancellation, and cleanup
events. V23-G supplies value stability throughout
synchronous handoff; queueing borrowed storage after producer recycle is invalid.

## V23-L — Width, chunk, lane, representation, and error determinism

This matrix isolates vector representation of the same logical values. If a
composed aggregate chooses a different legal FLOAT64 reduction tree, use V22's
owner-family comparison and V29-H–K instead of imposing cross-tree bit identity.

Evaluate one logical fixture under capacities 1, small positive values, 1024, another legal
positive capacity, and symbolic 65535; partition it as single rows, irregular chunks,
standard chunks, and one legal large chunk where feasible. Cross this with FLAT, CONSTANT,
DICTIONARY, nested/normalized selection, physical-lane permutation, repeated indices,
different pointer/StringHeap/arena addresses, allocation order, reset reuse, and legal
worker ownership.

Compare exact values, NULLs, bag, required order, demanded public error, transaction state,
and persistent bytes. Only resource use, performance, legal plan mechanics, and unordered
presentation sequence may vary. Reuse V21-13 with capacity/chunk/selection/representation
perturbations for the same V21-13 winning ordinary error; reuse V21-14 so V21-14 RETURNING
remains the same unordered bag. Chapter 25 remains the owner of demanded lane-error
precedence.

| Perturbation | Value | NULL | Bag | Required order | User error | Txn state | Persistent bytes |
|---|---|---|---|---|---|---|---|
| capacity / chunk boundary | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |
| physical lane / selection layout | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |
| repeated selection | selected duplicates only | selected state | multiplicity follows list | listed view order | unchanged | unchanged | unchanged |
| column ordinal under equivalent schema remap | unchanged through `SC` | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |
| representation kind / normalization | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |
| pointer, arena, StringHeap address / allocation order | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |
| NaN payload bits | Chapter-17 equivalent | never NULL | unchanged | canonical semantics | unchanged | unchanged | owner codec only |
| worker ownership / reset storage reuse | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged | unchanged |

## V23-M — Invalid states and owner reuse

The malformed-state harness changes one condition in an otherwise valid fixture and uses
poison/read counters to prove rejection before unsafe access. It accepts construction
invariants or boundary validation and requires no universal validation API.

| Invalid condition | Architecture state? | Public SQL error? | Required class | Pre-access/side-effect result |
|---|---:|---:|---|---|
| executable capacity 0 or >65535 | invalid | no | internal runtime invariant | no operator consumption/output |
| cardinality > capacity | invalid | no | internal runtime invariant | no active-value access |
| selection >= child cardinality | invalid | no | internal selection invariant | no payload/validity/StringRef/map read |
| selection capacity-resident but inactive | invalid | no | internal selection invariant | poison remains unread |
| invalid nested intermediate | invalid | no | internal selection invariant | no normalization legalization |
| missing dictionary child/base | invalid | no | internal representation invariant | no dereference |
| insufficient validity state / active position uninitialized | invalid | no | internal representation invariant | no publication |
| stale selection/StringRef after reset | invalid access | no | internal lifetime invariant | no stale value/dangling access |
| owner reset or backing mutation under live borrow | invalid absent stable mechanism | no | internal alias/lifetime invariant | borrower unchanged or operation prevented |
| compact use for over-domain VARCHAR | invalid representation choice | no new SQL type error | internal/inapplicable form | no truncation or wrap |
| no exact over-domain representation | valid resource outcome | yes, controlled | `ExecutionError` representability/resource | no partial value |
| supported exact form cannot allocate | valid resource outcome | yes, controlled | `OutOfMemory` | ordinary cleanup |
| endless empty `HAVE_MORE` self-loop | invalid implementation | no ordinary SQL error | pipeline liveness violation | finite-state oracle rejects |

No malformed runtime state may cause out-of-bounds access, stale-value publication,
dangling access, arbitrary mutation, or persistent corruption. Internal failures do not
become lane-dependent public SQL errors. Resource outcomes continue through §39.1's
existing statement-publication consequence; V23 defines no transaction policy.

### V23 memory-owner matrix

| Storage | Owner | Borrower/lifetime | Reset/invalidation | Retention rule | Persistent? | Oracle |
|---|---|---|---|---|---:|---|
| fixed vector payload / validity | chunk or operator state | synchronous view through declared borrow | owner reset after borrowers | retain owner or materialize | no | `LG/LV/VV` |
| selection storage | dictionary/view owner | dictionary and normalized view | owner mutation/reset | stable owner or copy selection state | no | `LG/LV` |
| chunk StringHeap | DataChunk | StringRefs and borrowed vectors | chunk reset after borrowers | copy/transfer/retain | no | `LG/BS` |
| borrowed input storage | upstream owner | synchronous downstream | producer reuse | complete, retain, or materialize | no | `LG/PM` |
| query constant storage | query/plan runtime owner | declared query lifetime | query teardown | retain valid owner | no | `LG` |
| retained operator/RowCollection storage | blocking operator/query | dependent source/sink | operator/query release | owner-defined spill/materialization | temporary only | `LG/PF` |
| QueryArena | query | bounded query state | query teardown | not unbounded row/string owner | no | Chapter-24 ledger |
| page bytes | BufferPool guard/pin | tuple view only while guarded | unpin/reuse | copy VARCHAR to chunk before release | persistent content via codec only | scan + `LG` |

### V23 cross-chapter reuse map

| Boundary | Contract | V23 obligation | Reused family | Independent oracle |
|---|---|---|---|---|
| Ch5→23 | persisted tuple/view versus aligned runtime values | page strings copied; runtime ABI not tuple bytes | tuple codec + scan tests | `PF/BS/LG` |
| Ch17→23 | typed scalar, NULL, FLOAT64, arbitrary-byte VARCHAR | every representation preserves scalar semantics | type/expression tests | `SV/BS/VV` |
| Ch20→23 | bags, occurrences, semantic order | selections preserve listed multiplicity/order without creating identity | V20 + V22-C | `SV` occurrence model |
| Ch22→23 | physical schema, LogicalSlotId, runtime nonidentity | columns realize ordered `S`; width/lane nonsemantic | V22-B/C/G | `SC/SV` |
| Ch23→24 | query memory, retained rows, resource accounting | owners/accounting survive retention/reuse | pipeline/resource procedures | `LG` + reservation ledger |
| Ch23→25 | UVF and active selection consumption | kernels receive exact logical sequence/validity | Expression Execution Tests | `LV/VV/SV` |
| Ch23→26 | batch status, borrow interval, progress | empty != EOS; stable synchronous handoff | V26-A/D/M; Pipeline Finalization and Resource Tests | `PM/LG` |
| Ch23→27 | scan decoding and unary chunks | page-copy, filter/project/limit handoff | Scan and Unary Operator Tests | `BS/LG/SC` |
| Ch23→28 | join retained values/null extension/duplicates | representation does not alter join bag | Hash Join Tests | `SV/LV/VV` |
| Ch23→29 | grouping/DISTINCT retained state | NULL/FLOAT/VARCHAR equivalence invariant | Aggregate Tests | grouping oracle + `BS` |
| Ch23→30 | retained sort values and cross-chunk order | ownership and comparator order invariant | Sort Tests | comparator + `LG` |
| Ch23→31 | DML/result materialization | codecs/results never retain runtime pointers | DML/RETURNING tests | `PF/LG/SV` |
| Ch23→39 | internal invalid state versus resource failures | exact pre-access/error classification | execution-failure tests | `EC` |

### V23 atomic architecture-obligation coverage ledger

Each correctness-relevant row below is independently falsifiable. `A` through `M` name the
procedures above; reuse never substitutes production output for an oracle. Repeated
summary invariants in §23.14 map to their detailed owning rows rather than creating a
second test with a second expected value.

<!-- V23_LEDGER -->

| ID | Architecture | Atomic obligation | Verification | Independent oracle | Reuse |
|---|---|---|---|---|---|
| A01 | §23.1 | DataChunk exposes capacity, cardinality, columns, and chunk-local StringHeap state | A/B/H | `DC/LG` | pipeline tests |
| A02 | §23.1 | an exchanged chunk realizes its operator's ordered physical output schema | A | `SC` | V22-B |
| A03 | §23.1 | `columns.size == S.width` | A | `DC/SC` | validator tests |
| A04 | §23.1 | `columns[j]` realizes schema entry `S[j]` | A | `SC` | V22-B |
| A05 | §23.1 | each runtime column type agrees with `S[j]` | A | schema/type table | type tests |
| A06 | §23.1 | column semantic identity is `S[j].LogicalSlotId` | A | `SC` | V22-B |
| A07 | §23.1 | column ordinal is local physical addressing, not semantic identity | A/L | `SC` + ordinal permutation | V22-B/C |
| A08 | §23.1 | equal-valued columns with distinct slots remain distinct outputs | A | `SC` | V22-B |
| A09 | §23.1 | Project reorder and derived remap follow the declared ordered schema | A/K | `SC` | project/V20 |
| A10 | §23.1 | `(row i,column j)` locates one runtime scalar occurrence of `S[j]` | A | `DC/SC/LV` | — |
| A11 | §23.1 | neither row nor column position is semantic identity | A/L | occurrence tags + `SC` | V20/V22-C |
| A12 | §23.1 | vector kind, selection, and borrowing do not change column identity | A/C/D/G | `SC` | V22-B |
| A13 | §23.1 | conformance does not require duplicate LogicalSlotId metadata in every Vector | A | schema-association implementations | V22-B |
| A14 | §23.1 | `STANDARD_VECTOR_SIZE` is 1024 | B | `DC` constant fixture | vector tests |
| A15 | §23.1 | operator behavior does not depend on scattered literal 1024 | B/L | alternate legal capacities + instrumentation | width tests |
| A16 | §23.1 | executable capacity is at least 1 | B/M | `DC` boundary predicate | — |
| A17 | §23.1 | executable capacity is at most 65535 | B/M | `DC` boundary predicate | — |
| A18 | §23.1 | every normal column shares the chunk cardinality | A/B | `DC` | vector tests |
| A19 | §23.1 | cardinality is nonnegative | B | `DC` mathematical domain | — |
| A20 | §23.1 | cardinality does not exceed capacity | B/M | `DC` boundary predicate | — |
| A21 | §23.1 | a final stream chunk may be partially filled | B/K | partial-chunk fixture | scan/pipeline |
| A22 | §23.1 | cardinality zero is an ordinary empty batch | B | `DC/PM` | pipeline tests |
| A23 | §23.1 | an empty batch is neither EOS nor FINISHED | B/K | `PM` | pipeline tests |
| A24 | §23.1 | source completion uses a separate status channel | B/K | `PM` | pipeline tests |
| A25 | §23.1 | a zero-capacity bookkeeping container is permitted but not required | B | conditional object-state fixture | — |
| A26 | §23.1 | zero-capacity state cannot be emitted or consumed as an executable batch | B/M | `DC/PM` | pipeline tests |
| A27 | §23.1 | zero-capacity executable use is internal invalid state, not public SQL error | B/M | `EC` | error tests |
| A28 | §23.1 | positive custom capacities through 65535 remain permitted where accepted | B/L | legal-capacity parameterization | width tests |
| A29 | §23.1 | object construction and allocation mechanics are not conformance requirements | B | two abstract construction models | — |
| B02 | §23.2 | vector size is execution tuning state, never persistent format | J/L | `PF` | format tests |
| C01 | §23.3 | the representation set is exactly FLAT, CONSTANT, DICTIONARY | C | closed-kind table | validator tests |
| C02 | §23.3 | FLAT stores/references contiguous typed positions | C/E | `LV` + storage trace | vector tests |
| C03 | §23.3 | active FLAT position `i` resolves to initialized payload and validity at `i` | C/D/E | `LV/VV` + poison | — |
| C04 | §23.3 | CONSTANT represents one scalar repeated for all logical rows | C | `LV/SV` | vector tests |
| C05 | §23.3 | the repeated CONSTANT scalar may be NULL | C/E | `LV/VV` | vector tests |
| C06 | §23.3 | every active CONSTANT logical position resolves to physical position zero | C/D | `LV` | — |
| C07 | §23.3 | DICTIONARY consists of an immediate child plus SelectionVector | C/D | recursive vector model | vector tests |
| C08 | §23.3 | DICTIONARY owns no independent value array | C/G | ownership/storage graph | borrow tests |
| C09 | §23.3 | DICTIONARY resolves through an active logical position of its immediate child | C/D | `LV` | — |
| C10 | §23.3 | SEQUENCE and RLE are outside the representation vocabulary | C | closed-kind negative fixtures | validator tests |
| D01 | §23.4 | fixed-width FLAT storage is naturally aligned, contiguous, and validity-associated | E | alignment/range + `VV` | vector tests |
| D02 | §23.4 | runtime BOOLEAN payload is uint8 | E | runtime type table | type tests |
| D03 | §23.4 | runtime INT32 payload is int32 | E | runtime type table | type tests |
| D04 | §23.4 | runtime INT64 payload is int64 | E | runtime type table | type tests |
| D05 | §23.4 | runtime FLOAT64 payload is binary64/double | E | runtime type table + scalar oracle | FLOAT tests |
| D06 | §23.4 | runtime DATE payload is int32 | E | runtime type table | type tests |
| D07 | §23.4 | runtime TIMESTAMP payload is int64 | E | runtime type table | type tests |
| D08 | §23.4 | runtime BOOLEAN is byte-per-value and not bit-packed | E | element-address/width fixture | type tests |
| D09 | §23.4 | runtime fixed-width representation does not define persistence | E/J | `PF` | tuple/scalar codec tests |
| E01 | §23.5 | validity has one bit per physical value position | E | `VV` | vector tests |
| E02 | §23.5 | validity 1 means non-NULL and 0 means NULL | E | `VV/SV` | NULL tests |
| E03 | §23.5 | validity words are uint64 | E | validity storage inspection | vector tests |
| E04 | §23.5 | 1024 positions use 16 words / 128 bytes | E | independent arithmetic | — |
| E05 | §23.5 | `all_valid=true` is equivalent to every active bit valid without reading words | E | dual-path `VV` | expression tests |
| E06 | §23.5 | inactive validity bits cannot affect results | E/H | poison + `VV` | vector tests |
| E07 | §23.5 | CONSTANT repeats one validity state | C/E | `LV/VV` | vector tests |
| E08 | §23.5 | DICTIONARY validity is selected child validity after composition | D/E | recursive `LV/VV` | vector tests |
| F01 | §23.6 | standard SelectionVector entries are uint16 and indexed by active selection position | D | storage/domain fixture | vector tests |
| F02 | §23.6 | selection maps logical row `i` to its immediate child's logical position | D | `LV` | — |
| F03 | §23.6 | allocated child capacity is not the selection domain | D/M | cardinality/capacity divergence fixture | — |
| F04 | §23.6 | each index is less than immediate-child logical cardinality | D/M | mathematical bounds oracle | — |
| F05 | §23.6 | valid FLAT child index resolves to same initialized slot | D | `LV/VV` | — |
| F06 | §23.6 | valid CONSTANT child index resolves to scalar slot zero | D | `LV` | — |
| F07 | §23.6 | valid DICTIONARY child index recursively follows that view | D | `LV` | — |
| F08 | §23.6 | repeated indices are legal | D | explicit selection fixture | V20 bag tests |
| F09 | §23.6 | repeated indices preserve occurrence multiplicity | D/C/L | occurrence multiset | V20/V22-C |
| F10 | §23.6 | unsorted indices preserve listed logical occurrence order | D | sequence comparator | V20/V22-C |
| F11 | §23.6 | selection performs no implicit deduplication or sorting | D | adversarial duplicate/order fixture | V20 |
| F12 | §23.6 | every active logical position resolves to initialized payload | D/H/M | poison read counter | — |
| F13 | §23.6 | every active logical position resolves to initialized validity | D/E/H/M | poison read counter + `VV` | — |
| F14 | §23.6 | every active logical position has valid representation state | C/D/H/M | recursive model | — |
| F15 | §23.6 | allocated inactive positions are semantically inaccessible | D/H | poison + `LV` | — |
| F16 | §23.6 | an out-of-domain index is an internal invalid representation | D/M | `EC` | error tests |
| F17 | §23.6 | invalid index is rejected before payload, validity, StringRef, or child-map access | D/M | poison/read counters | — |
| F18 | §23.6 | invalid selection never exposes stale data | D/H/M | generation poison | — |
| F19 | §23.6 | invalid selection is not a public SQL error | D/M | `EC` | §39 tests |
| F20 | §23.6 | construction invariants or explicit validation may enforce legality | D/M | two enforcement harnesses | — |
| F21 | §23.6 | identity selection need not materialize an index array | C/D | metadata-view conformance fixture | vector tests |
| G01 | §23.7 | hot kernels never receive unbounded dictionary lookup chains | C/D | chain-depth instrumentation | kernel tests |
| G02 | §23.7 | composition follows outer logical position through inner selection to base | D | recursive `LV` | — |
| G03 | §23.7 | every intermediate index is validated against its child cardinality | D/M | one-defect nested fixtures | — |
| G04 | §23.7 | normalization cannot legalize an inactive capacity-resident index | D/M | poisoned capacity fixture | — |
| G05 | §23.7 | chain depth exceeding one effective indirection is normalized before a hot kernel | C/D | dispatch/depth trace | kernel tests |
| G06 | §23.7 | normalization preserves logical row order | C/D | `LV` sequence | V20 |
| G07 | §23.7 | normalization preserves effective validity | C/D/E | `LV/VV` | NULL tests |
| G08 | §23.7 | normalization preserves underlying value identity | C/D | exact scalar/byte oracle | type tests |
| G09 | §23.7 | normalization need not copy payload solely to remove indirection | C/D | borrowed-base conformance fixture | borrow tests |
| H01 | §23.8 | UnifiedVectorFormat exposes typed/base data plus effective selection and validity views | C | conceptual-view adapter fixture | expression tests |
| H02 | §23.8 | representation dispatch occurs once per vector/batch | C | dispatch instrumentation | kernel tests |
| H03 | §23.8 | kernels do not branch per row on representation kind | C | per-row branch counter | kernel tests |
| H04 | §23.8 | CONSTANT effective selection maps every logical row to zero | C/D | `LV` | expression tests |
| H05 | §23.8 | valid nested DICTIONARY selections compose before the hot loop | C/D | `LV` + dispatch trace | expression tests |
| H06 | §23.8 | UnifiedVectorFormat preserves active domain and excludes inactive payload | C/D | poison + `LV` | expression tests |
| H07 | §23.8 | UnifiedVectorFormat is runtime adapter state, not persistent materialization | J | `PF` | format tests |
| I01 | §23.9 | compact StringRef contains uint32 length, uint32 prefix, and data reference | F | compact-field fixture | string tests |
| I02 | §23.9 | compact StringRef is naturally 16 bytes on a 64-bit process | F | process-layout observation | — |
| I03 | §23.9 | StringRef length is exact byte length | F/I | `BS/LM` | string tests |
| I04 | §23.9 | compact StringRef exactly represents lengths through UINT32_MAX | I | arbitrary-precision `LM` | — |
| I05 | §23.9 | length greater than UINT32_MAX is outside compact applicability | I/M | `LM` capability predicate | — |
| I06 | §23.9 | a value is never truncated to fit compact StringRef | I/M | symbolic content/length witness | — |
| I07 | §23.9 | a length is never wrapped or reduced modulo uint32 | I/M | arbitrary-precision `LM` | — |
| I08 | §23.9 | one VARCHAR is not semantically split merely to fit compact form | I | `BS/LM` one-value model | — |
| I09 | §23.9 | NUL clipping or C-string reinterpretation is forbidden | F/I | embedded-NUL `BS` fixture | string tests |
| I10 | §23.9 | the logical value remains one exact Chapter-17 byte string | F/I | `BS/SV` | type tests |
| I11 | §23.9 | an exact alternate over-domain runtime representation is permitted | I | capability-on fixture | — |
| I12 | §23.9 | any alternate preserves Chapter-17 byte semantics | I/L | `BS/SV` | type tests |
| I13 | §23.9 | any alternate preserves Chapter-23 ownership/lifetime | G/I | `LG` | borrow tests |
| I14 | §23.9 | any alternate preserves downstream execution semantics | I/K/L | operator semantic oracles | operator tests |
| I15 | §23.9 | conformance does not mandate an alternate structure or integer width | I | capability-off and capability-on fixtures | — |
| I16 | §23.9 | compact-only realization is inapplicable when a supported exact alternate exists | I | capability/choice oracle | — |
| I17 | §23.9 | execution cannot fail merely because the incapable compact form was chosen | I/M | adversarial representation chooser | §39 tests |
| I18 | §23.9 | absence of every exact form yields controlled ExecutionError representability/resource cause | I/M | `LM/EC` | §39 tests |
| I19 | §23.9 | allocation failure for an otherwise supported exact form remains OutOfMemory | I/M | injected allocation failure + `EC` | resource tests |
| I20 | §23.9 | runtime representability failure is not parser/type/cast/corruption/invalid-VARCHAR | I/M | excluded-category table | §39 tests |
| I21 | §23.9 | heap inline tuple maximum is not a universal runtime VARCHAR maximum | I/J | symbolic intermediate + tuple-owner fixture | tuple tests |
| I22 | §23.9 | StringRef creates no tuple/catalog/statistics/spill/WAL format rule | J | `PF` | format tests |
| I23 | §23.9 | StringRef requires no trailing NUL | F | exact-length `BS` fixture | string tests |
| I24 | §23.9 | length-zero data need not be dereferenced | E/F | invalid pointer + zero-length oracle | NULL/string tests |
| J01 | §23.9.1 | prefix is the first up to four bytes in unsigned big-endian order | F | independent shift/or prefix | string tests |
| J02 | §23.9.1 | missing prefix bytes are zero-filled | F | length 0..3 table | string tests |
| J03 | §23.9.1 | prefix bytes are interpreted unsigned | F | 00/7f/80/ff fixtures | string tests |
| J04 | §23.9.1 | differing prefix may be used only as a valid fast rejection | F | prefix/full comparison oracle | expression tests |
| J05 | §23.9.1 | equal prefix never proves equality or order | F | equal-prefix unequal-tail fixtures | expression tests |
| J06 | §23.9.1 | length/full bytes resolve equal-prefix cases | F | `BS` comparator | type tests |
| J07 | §23.9.1 | prefix-assisted comparison agrees with Chapter-17 binary collation | F/K | `BS/SV` | type/index tests |
| K01 | §23.10 | borrowed StringRef/vector backing owner remains alive | G | `LG` generation graph | lifetime tests |
| K02 | §23.10 | owner liveness alone is insufficient | G | live-but-mutated fixture | — |
| K03 | §23.10 | every borrowed/reference/dictionary/selection view is value-stable for its interval | G | publication-value `LV/BS` | — |
| K04 | §23.10 | active logical cardinality needed by a borrow remains stable | G | cardinality mutation fixture | — |
| K05 | §23.10 | required type and representation metadata remain stable | G | metadata mutation fixture | — |
| K06 | §23.10 | reachable payload and validity remain stable | G | payload/validity poison fixtures | — |
| K07 | §23.10 | selection storage and entries remain stable | G | selection mutation fixture | — |
| K08 | §23.10 | dictionary child/base relationship remains stable | G | base relation mutation fixture | — |
| K09 | §23.10 | required backing addresses and ranges remain valid | G | generation/address graph | — |
| K10 | §23.10 | StringRef length, prefix, and data reference remain stable | G | metadata mutation fixture | string tests |
| K11 | §23.10 | referenced VARCHAR bytes remain stable | G | byte poison fixture | string tests |
| K12 | §23.10 | owner cannot reset or reuse reachable backing during a live borrow | G/H | reset barrier + `LG` | pipeline tests |
| K13 | §23.10 | owner cannot overwrite or observably mutate reachable backing during a live borrow | G | mutation matrix | — |
| K14 | §23.10 | incompatible reallocation cannot invalidate a live view | G | generation/address fixture | — |
| K15 | §23.10 | inactive storage unreachable from the view may change | G | reachability proof + poison | — |
| K16 | §23.10 | capacity may change only when required ranges/mappings/values remain stable | G | stable-versus-invalid growth fixtures | — |
| K17 | §23.10 | every declared owner category supplies a lifetime covering its borrowers | G/M | owner matrix | memory/operator tests |
| K18 | §23.10 | StringRef cannot outlive an unpinned heap page | G/H | page poison after unpin | scan tests |
| K19 | §23.10 | StringRef cannot outlive a reset/reused source chunk | G/H | chunk generation poison | pipeline tests |
| K20 | §23.10 | StringRef cannot outlive temporary expression scratch | G | scratch generation fixture | expression tests |
| K21 | §23.10 | StringRef cannot outlive released spill/in-memory block storage | G/J | released-block generation fixture | spill tests |
| K22 | §23.10 | retained VARCHAR obtains a valid owner or deep-copies bytes | G/K | retention graph | operator tests |
| K23 | §23.10 | retention beyond any borrow interval obtains new ownership/materialization | G | interval-end fixture | — |
| K24 | §23.10 | required materialization allocation failure remains the existing resource failure | G/I/M | injected allocation + `EC` | resource tests |
| K25 | §23.10 | expired or unstable borrow is an internal lifetime/aliasing violation | G/M | `LG/EC` | error tests |
| K26 | §23.10 | expired/unstable backing is rejected before dereference | G/M | poison read counters | — |
| L01 | §23.11 | every reusable DataChunk owns chunk-local variable-length storage | A/H | `DC/LG` | vector tests |
| L02 | §23.11 | scan decoding copies page/tuple VARCHAR bytes into chunk StringHeap | H/K | page-to-chunk ownership graph | scan tests |
| L03 | §23.11 | chunk strings do not extend heap-page pin lifetime | H | guard event trace + poison | buffer/scan tests |
| L04 | §23.11 | StringHeap reset waits until no downstream borrow references it | G/H | `LG` + reset barrier | pipeline tests |
| M01 | §23.12 | synchronous reference/dictionary borrowing is permitted through downstream consumption | G/K | zero-copy event graph | pipeline tests |
| M02 | §23.12 | synchronous borrow keeps all reachable resolving state value-stable | G | complete backing-state mutation matrix | — |
| M03 | §23.12 | input/output aliasing is legal only while every live alias remains stable | G | multi-alias `LG` | project tests |
| M04 | §23.12 | immutable, delayed, transfer, COW, retained, pinned, copied, or other exact mechanisms may satisfy stability | G | mechanism-conformance matrix | — |
| M05 | §23.12 | borrowing does not mandate copying or one ownership mechanism | G | zero-copy plus copy fixtures | — |
| M06 | §23.12 | divergent write to shared CONSTANT/DICTIONARY first establishes capable storage | C/G | logical-write oracle | vector tests |
| M07 | §23.12 | live dictionary selection/base/reachable values cannot mutate observably | G | selection/base mutation fixtures | — |
| M08 | §23.12 | blocking operators deep-copy retained borrowed data | G/K | retained-owner graph | join/aggregate/sort |
| M09 | §23.12 | result/client boundary materializes or safely retains memory | G/K | teardown-before-read fixture | ResultSink tests |
| M10 | §23.12 | pipeline executor enforces the borrow interval | G/K | pipeline event graph | pipeline tests |
| N01 | §23.13 | operators reuse chunk/vector buffers without changing semantics | H/L | generation poison + semantic oracle | width tests |
| N02 | §23.13 | reset occurs only after borrow completion or independent ownership | G/H | `LG` reset precondition | pipeline tests |
| N03 | §23.13 | reset/reuse cannot mutate reachable backing of a live borrower | G/H | reset mutation fixture | — |
| N04 | §23.13 | reset sets cardinality to zero | H | `DC` state transition | vector tests |
| N05 | §23.13 | reset clears/reinitializes vector logical state | H | stale payload/validity/selection poison | vector tests |
| N06 | §23.13 | reset resets chunk StringHeap | H | StringHeap generation fixture | string tests |
| N07 | §23.13 | reset may preserve reusable allocated capacity | H | capacity-before/after fixture | memory tests |
| N08 | §23.13 | large reusable buffers remain query-memory accounted | H/M | reservation ledger | memory tests |
| O01 | §23.14 | malformed state is rejected before out-of-bounds access | M | bounds poison/read counters | error tests |
| O02 | §23.14 | malformed state is rejected before stale-value exposure | M | generation poison | error tests |
| O03 | §23.14 | malformed state is rejected before dangling access | M | `LG` generation graph | error tests |
| O04 | §23.14 | malformed state cannot cause arbitrary mutation | M | side-effect snapshot | error tests |
| O05 | §23.14 | malformed state cannot cause persistent corruption | J/M | `PF` + durable-byte snapshot | format/error tests |
| O06 | §23.14 | malformed selection/borrow/chunk failures are internal, not normal SQL errors | M | `EC` | §39 tests |
| O07 | §23.14 | construction invariants or boundary validation may enforce valid state without one universal API | M | dual enforcement harness | validator tests |

Non-falsifiable Chapter-23 material is accounted for separately:

| ID | Architecture | Material | Classification and justification | Status |
|---|---|---|---|---|
| X01 | §23.2 | representative byte-size calculations and the cache/amortization rationale for 1024 | analytical performance sizing/rationale; Chapter 42 owns measurements | N/A |
| X02 | §23.3 | the representation vocabulary is intentionally limited | rationale for the falsifiable closed set in C01/C10, not another behavior | N/A |
| X03 | §23.6 | Filter, join, slicing, and dictionary common-use examples | illustrative navigation; each applicable behavior has a direct V23/operator procedure | N/A |
| X04 | §23.12 | Filter and simple Project are typical borrowing examples | illustrative navigation; M01–M10 own the actual contract | N/A |
| X05 | §23.14 | numbered invariants restate detailed §§23.1–23.13 rules | consolidated index; detailed ledger rows are the canonical tests | N/A |


The closed-set and stale-language audit for this family requires all of the following:

- no selection oracle uses capacity or vague physical bounds in place of immediate-child
  logical cardinality;
- no borrowing procedure accepts owner liveness without value stability;
- no executable zero-capacity chunk, empty-as-EOS rule, or no-progress empty stream is
  accepted;
- no VARCHAR procedure truncates, wraps, treats an over-domain value as invalid SQL, or
  conflates unsupported exact representation with OutOfMemory;
- no runtime Vector/StringRef ABI is treated as a persistent codec;
- no project chronology, implementation status, test result, sleep, pointer accident, or
  production decoder is used as verification evidence.
