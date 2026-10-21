# Chapter 24 — Query Memory, Row Storage, and Spill Verification

This family verifies the complete Chapter-24 execution-resource contract without using
production counters, allocators, row decoders, spill decoders, or filename generators as
their own expected results. All schedules, allocation outcomes, and filesystem outcomes are
explicit inputs. Concurrency uses barriers and enumerated interleavings; liveness uses
finite state and well-founded metrics; no procedure depends on sleeps, allocator addresses,
OS pressure timing, directory order, or filename luck.

The independent test-side oracles are:

```text
RS  Chapter-17/20 scalar, NULL, occurrence, order, LIMIT/OFFSET, and demanded-error model
ML  declarative region/owner/ledger/committed-capacity accounting model
BI  arbitrary-precision nonnegative size/count/offset arithmetic model
OL  request/grant/allocation/live-ownership/transfer/release state machine
PG  finite pressure graph with a well-founded progress metric
RR  exact retained-row model: schema values, NULL states, VARCHAR bytes, one occurrence
SF  independent temporary spill framing, range, CRC, and structural model
NS  managed-temp namespace and exclusive-owner model over a controlled fake filesystem
CL  query/attempt resource and cleanup ledger
EC  Chapter-24/§39 cause-to-category classifier
PF  runtime/temporary/persistent representation registry
DS  deterministic scheduler for accounting grants, scavenging, and cancellation
LG  V23-G generation-tagged borrow/value-stability graph
```

`ML` records each implementation-controlled committed region, its live owner, covered
capacity, query/global ledgers, exemption proof, and any permitted transfer overlap. It does
not sample RSS. `BI` computes the mathematical result before testing each consuming domain.
`SF` parses independently from production code and records whether allocation, pointer
arithmetic, dereference, or I/O was attempted. `NS` never derives expected ownership from a
generated path alone.

## V24-A — Resource policy and SQL-semantics boundary

Construct one declarative query result under Chapter-17/20 semantics, then realize it with
paired legal execution configurations varying memory budget, query/global hard limits,
ordinary block target, operator target, spill threshold, exact retained representation,
allocation layout, and spill/no-spill path. For every pair that completes without a resource
failure, compare scalar values, NULL states, occurrence count and bag multiplicity,
required order, demanded semantic errors, §39.1 transaction effect, and persistent database
bytes against the owning semantic contract. Use exact comparison where invariant;
compose V22's result-family procedure and V29-H–K for owner-admitted FLOAT64
aggregate variation and its downstream effects. `RS` supplies the expectation;
neither row/block order nor production output order is
the oracle for an unordered result.

Resource capability is a separate input. A lower budget, unavailable exact form, injected
allocation denial, or injected temporary-I/O fault may legitimately change feasibility and
produce the canonical `OutOfMemory`, representability/resource `ExecutionError`, or
`SpillIOError`. Do not require all configurations to succeed or fail together. When a path
succeeds, resource policy cannot redefine a scalar domain, logical row count, duplicate
multiplicity, required ordering, `LIMIT`/`OFFSET`, or persistent format. RowCollection
insertion/block order and spill/run/reload order are asserted nonsemantic unless the owning
operator's established ordering contract requires an order.

## V24-B — Complete committed-capacity accounting universe

Populate `ML` from the final owner map, not from allocations observed in production. For
each live region, assert one conceptual accounted owner in every applicable ledger and a
charge no smaller than implementation-controlled committed capacity made available to the
query. Parent page/block/arena coverage is legal and contained objects need no duplicate
charge. Conservative overcharge is legal; systematic undercharge is not. Allocator headers,
page tables, virtual-memory details, and exact RSS are outside the measurement oracle.

| Region/category | Query-owned when present? | Unbounded aggregate possible? | Required coverage | Permitted owner granularity | Exemption/owner | Oracle/status |
|---|---:|---:|---|---|---|---|
| QueryArena backing | yes | yes through page growth | query/global charge; nonspillable | arena page/capacity | none | `ML` |
| owned DataChunk/vector capacity | yes | yes across retained/live owned buffers | owning region charge | chunk/vector/buffer owner | none when query-owned | `ML` + V23 |
| owned StringHeap capacity | yes | yes | owning chunk/operator charge | heap/page/buffer | none | `ML/LG` |
| owned SelectionVector capacity | yes | yes in aggregate | owning buffer charge | vector/chunk parent allowed | none | `ML` |
| RowCollection fixed payload | yes | yes | collection/block charge | block/collection | none | `ML/RR` |
| RowCollection variable payload | yes | yes | collection/varlen owner charge | block/varlen region | none | `ML/RR` |
| RowCollection block metadata | yes | yes by block count | accounted parent/metadatan owner | block/directory | none | `ML` |
| block/run/partition directories | yes | yes | accounted metadatan owner | directory/page/operator | none | `ML` |
| hash/aggregate/DISTINCT/sort retained state | yes | yes | operator/query ledgers | region/operator | owner-defined spillability | `ML` |
| DML/RETURNING execution spool | yes while execution-owned | yes | spool/operator/query charge | block/collection/operator | later owner only after valid transfer | `ML/CL` |
| spill read/write buffers | yes | yes by capacity/workers | query/global charge | buffer/operator | none merely because temporary | `ML` |
| reloaded spill blocks | yes | yes | charge before live reload ownership | block/operator | none | `ML/OL` |
| query-owned worker-local dynamic state | yes | yes across tasks/workers | query/global charge | worker page/cache/operator | none merely because local | `ML/DS` |
| BufferPool frame capacity | no, under Chapter-7 owner | separately bounded | excluded from query ledger | BufferPool | separate bounded owner | owner map |
Generate `N` individually small allocations through direct regions, metadata arrays, and
QueryArena pages such that the exact aggregate crosses a hard gate. The accepted prefix is
the maximal one whose covered committed capacity fits; the rest cannot become live. Repeat
with a parent page that covers header, offsets, and payload: one parent charge is sufficient.
The case `committed=1024, charge=2048` conforms, while `committed=1024, charge=1000` does
not. A request of 1000 that commits and exposes 1024 must be covered by at least 1024.
At every accounting transition, compare the reported peak query charge with the maximum
live query total in the complete `ML` event history; release can lower the live total but
never the already-observed peak.

An exemption fixture is accepted only with a separately bounded architecture owner or an
independent fixed architecture bound unrelated to execution growth. Labels such as small,
metadata, temporary, or worker-local never suffice. Attempting to route row-, group-, run-,
partition-, or result-dependent unbounded storage through QueryArena is a nonconforming
owner classification even if every individual object is small.

## V24-C — Query/global hard gates and exact accounting arithmetic

Use `BI` for `live + requested <= hard_limit` and `DS` for grant interleavings. Cover zero,
`limit + 0`, `(limit-1)+1`, `limit+1`, maximum consuming-domain values, and mathematical
sums beyond that domain. No wrapped sum may grant capacity.

`BI` distinguishes a representable exact sum above the hard limit, which is a denied grant
and follows V24-H, from an exact total outside the supported accounting-size domain, which
is a representability/resource `ExecutionError` before any ledger update. If denial has no
exact progress path, V24-H/L supplies the terminal `OutOfMemory` classification.

For the concurrency fixture, set query and global remaining capacity to `X`, pause two
requests of `X` at a barrier before the logical commit point, and enumerate both serial
orders plus the contested interleaving. Exactly one request may commit. Repeat with distinct
queries sharing only the global gate and with one query constrained only by its query gate.
The `ML` transition checks and updates every applicable total atomically; production lock or
atomic implementation is not prescribed.

## V24-D — Accounted ownership lifecycle

Drive `OL` through these conceptual states without requiring matching source enums:

| Event | Charge after event | Live memory after event | Required next/result | Independent assertion |
|---|---:|---:|---|---|
| request | no new charge | none | grant or denial | request is not ownership |
| denied request | unchanged | none new | discard/private provisional state | `ML/OL` unchanged |
| grant | live | none implied | allocation or release | grant is budget authority only |
| exact allocation success | live | live | ownership interval | charge covers committed capacity |
| rounded allocation `A > G` | at least `A` | `A` only after coverage | enlarge, expose covered part, or discard | never charge `G`/own `A` |
| grant enlargement denied | old charge only | old state only | rollback/discard provisional growth | no partial growth |
| in-place growth by `D` | old plus covered `D` | enlarged only after charge | own enlarged region | positive delta precovered |
| catchable exact allocation failure | unused grant released | no new live state | `OutOfMemory` | previous ownership coherent |
| size/address domain unsupported | no narrowed charge/allocation | none new | representability/resource `ExecutionError` | not OOM |
| transfer | continuous; overlap allowed | continuously owned | receiving owner established | no unaccounted gap |
| release | decremented once | no longer owned | terminal ownership state | every ledger updated once |
| second release/underflow | no wrapped mutation | invalid internal state | internal invariant failure | no public resource relabeling |
| error/cancellation teardown | all ending ownership released once | none except valid transfer | canonical error retained | `CL` reaches zero |

Explicitly exercise private allocate-first storage. It conforms only while bounded or
already charged, inaccessible as query state, and immediately discarded if accounting
fails. Catchable allocator denial for an exact supported representable request is always
`OutOfMemory`; non-catchable OS/process termination is outside the returned-error model.
This procedure rejects vague “where possible” normalization.

## V24-E — QueryArena accounting and no-bypass

Allocate many small query-lifetime objects until arena backing adds pages. Compare the
backing-page capacity transitions with `ML`; do not sum object payloads again when the page
charge already covers them. Assert the arena is nonspillable and released at query teardown.
Then classify attempted unbounded retained rows, groups, runs, partitions, vector buffers,
or result spools in the arena as invalid ownership plans before using an allocator. This is
an architecture-owner test, not a required QueryArena API or class-layout test.

## V24-F — RowCollection retained-row and borrow ownership

`RR` represents one row as ordered schema values, independent NULL states, exact VARCHAR
bytes, and one bag occurrence. Compare append/iterate/spill/reload results to `RR`; block
number, offset, handle value, insertion order, and pointer address are never identity or SQL
order. Verify row handles are query-local, stable only while their collection/block lives,
never RIDs, and distinct from reconstructed spill handles.

Append a row containing a borrowed input-chunk StringRef, then reset and poison the input.
The retained row must still equal `RR`: the RowCollection or another stable owner owns exact
bytes and validity. Reuse V23-G for the borrow interval and Chapters 28–31 operator tests for
retained hash/aggregate/sort/DML/result state. A NULL payload remains ignored; non-NULL empty
VARCHAR remains distinct from NULL.

## V24-G — Exact oversized retained-row applicability

Use symbolic row extents around the ordinary 256 KiB target (`target-1`, `target`, and
`target+1`) and beyond an ordinary descriptor domain. The target is a tunable resource
target, never a SQL, VARCHAR, cardinality, correctness, or heap-tuple maximum.

| Retained-row case | SQL/runtime value valid? | Ordinary form | Exact alternate | Expected result | Occurrence/value oracle |
|---|---:|---|---|---|---|
| below target | yes | applicable if exact | optional | exact retention | `RR` |
| at target | yes | applicable if exact | optional | exact retention | `RR` |
| above target, ordinary exact | yes | applicable | optional | exact retention | `RR` |
| ordinary descriptor overflow | yes | inapplicable | capability-dependent | alternate or controlled failure | `BI/RR` |
| dedicated/other exact form available | yes | may be inapplicable | yes | must not fail due to ordinary-form choice | `RR` |
| exact physical segmentation | yes | capability-dependent | yes | one row; every scalar exact | `RR` occurrence count |
| no supported exact form | yes | inapplicable | no | representability/resource `ExecutionError` | `EC` |
| exact form, allocation denied | yes | any | yes | `OutOfMemory` | injected allocator + `EC` |
| larger than heap tuple capacity | yes for runtime retention | independent | capability-dependent | never rejected solely by heap limit | `PF/RR` |
| VARCHAR length greater than UINT32_MAX with exact scalar form | yes | capability-dependent | capability-dependent | retain exactly, or `ExecutionError`; allocation denial is OOM | V23-I + `RR/EC` |

Every negative fixture asserts no truncation, clipping, wrap, modulo reduction, or semantic
splitting. Physical segmentation is accepted only when it preserves one row occurrence and
one exact scalar value per schema position. No concrete oversized representation is
required.

## V24-H — Finite resource-pressure progress

`PG` nodes contain live charges, denied request, eligible exact actions, operator-local
finite pressure state, and cancellation/I/O outcomes. An edge is progress only if it lowers
live relevant capacity, lowers required charge, frees an eligible owner, selects an exact
lower-memory form, or advances a well-founded finite state capable of satisfying the same
demand.

| Pressure case | Progress? | Retry permitted? | Terminal/result | Semantic oracle |
|---|---:|---:|---|---|
| sufficient reclaim | yes | yes; grant succeeds | continue | `ML/PG` |
| partial reclaim with decreasing finite metric | yes | yes | later grant or terminal error | `PG` |
| spill writes bytes but frees zero useful capacity | no by itself | no equivalent retry | `OutOfMemory` if no other action | `PG` |
| no eligible spill/reclaim owner | no | no | `OutOfMemory` | `EC` |
| finite repartition step | yes only with well-founded advance | yes | finite continuation | symbolic metric |
| exact lower-memory representation | yes | yes | continue if grant fits | `RR/PG` |
| unchanged denied state | no | no | reject loop; terminal OOM when no action | cycle detector |
| spill read/write/create failure | n/a | no | `SpillIOError` | injected filesystem + `EC` |
| temporary capacity/ENOSPC | n/a | no | `SpillIOError` | fake filesystem quota |
| cancellation | n/a | no | `QueryCancelled` | barrier-controlled cancellation |

No arbitrary retry count is the oracle; termination follows grant, controlled terminal
outcome, or strict progress in a well-founded finite state. Pressure actions are limited to
the selected operator contract and never require optimizer re-entry. Compare in-memory,
one-spill, multiple-finite-spill, and exact lower-memory paths against `RS` for values,
NULLs, bag, required order, demanded errors, and transaction behavior. Approximation,
truncation, deduplication, row dropping, and required-order loss are rejected.

Instrument the accounting critical section and callback boundary: a spill/reclaim callback
may release and re-request without recursive accounting-lock ownership. The oracle is the
absence of lock-cycle/reentrant ownership in the explicit event graph, not a required mutex.

## V24-I — Universal exact size/count/offset arithmetic

For each operation below, `BI` computes the exact mathematical value, then a separate domain
predicate decides whether use is legal. Poison the production operation after the first
invalid boundary and require zero allocation/range/pointer/I/O attempts.

| Expression | Boundary fixtures | Consuming domain | Invalid result |
|---|---|---|---|
| `count * width` | 0, 1, max exact, one above, symbolic huge | allocation/range | representability/resource failure before use |
| fixed + varlen bytes | exact, native-overflowing symbolic sum | row/block extent | `ExecutionError` before allocation/pointer use |
| `offset + length` | each operand valid but sum invalid | memory range | safe rejection; no wrap |
| `live_total + charge` | limit boundaries and beyond native width | query/global ledger | no wrapped grant |
| release subtraction | exact owned charge; release greater than owned | ledger | internal invariant; no underflow |
| capacity rounding | exact boundary; round-up overflow | allocation/accounting | controlled representability/resource failure |
| block/run/partition count | max exact and next | metadata identity/domain | controlled failure; no alias/reuse/loss |
| spill payload length | exact and over-domain | temporary encoding | `SpillIOError` |
| header + payload record length | exact and over-domain | temporary record | `SpillIOError` |
| file offset + I/O length | operands valid, sum invalid | spill file/I/O | `SpillIOError` before I/O |
| allocation extent | in-domain denied; out-of-domain | allocator/address domain | OOM versus `ExecutionError` distinction |

Signed overflow, unsigned wrap, modulo reduction, narrowing, pointer-range overflow, and
file-offset truncation are separately faulted. Checked operations, wider integers, or
prevalidated bounds may all conform; tests do not require a concrete integer type/library.

## V24-J — Spill framing and pre-access validation

Build records with `SF`, independently encode exact valid fixtures, and generate one-defect
variants. Event counters distinguish header reads needed to validate framing from any
allocation, derived pointer arithmetic, payload dereference, seek, or payload I/O driven by
decoded metadata.

| Record case | Allocation from decoded extent? | Pointer/range use? | Payload dereference/I/O? | Expected result | Persistent corruption? |
|---|---:|---:|---:|---|---:|
| valid complete record | after validation | after validation | yes | exact decoded payload | no |
| truncated framing/header | no | no | no payload use | `SpillIOError` | no |
| bad magic/version | no | no | no | `SpillIOError` | no |
| payload-length over domain | no | no | no | `SpillIOError` | no |
| record-length/count-derived overflow | no | no | no | `SpillIOError` | no |
| offset outside payload | no dependent allocation | no invalid pointer | no invalid dereference | `SpillIOError` | no |
| offset and length valid, sum invalid | no | no | no | `SpillIOError` | no |
| file range/addressability overflow | no dependent range | no | no I/O | `SpillIOError` | no |
| CRC mismatch | only safely bounded storage | no invalid range | no accepted payload | `SpillIOError` | no |
| owner-specific structural mismatch | no unsafe dependent action | no | no accepted payload | `SpillIOError` | no |

Repeat malformed cases as partial write, crash truncation, stale managed leftover, disk-fault
mutation, and deterministic external byte mutation. Self-generated origin is never a trust
oracle. An impossible in-memory state that violates an already-established construction
invariant is classified internal rather than converted to public `SpillIOError`.
Temporary integer serialization is explicit and never native object/pointer dumping. The
test does not impose a long-lived spill ABI; Chapters 28–30 own specialized payloads.

## V24-K — Spill namespace, collision, crash, and reclamation

`NS` models `(managed namespace, process/database-instance owner, query, attempt,
resource)` separately from the pathname. A controlled fake filesystem provides exclusive
create, collision, delete, crash, quota, and enumeration events in an explicit order.

| Lifetime event | Live owner after event | Immediate deletion? | Adoptable by new query? | Eventual action/error | WAL/recovery |
|---|---|---:|---:|---|---|
| fresh live resource | creating attempt | no | no | ordinary use | none |
| live-name collision | first owner unchanged | no overwrite | no | fresh identity or `SpillIOError` | none |
| stale-name collision | none for stale object | no blind overwrite | no | fresh identity or safe failure | none |
| query success | none | yes for owned spill | no | deleted | none |
| semantic/ExecutionError/OOM/SpillIO error | none after unwind | yes | no | deleted | none |
| cancellation | none after unwind | yes | no | deleted | none |
| abandoned retry attempt | none for old attempt | yes | no | next attempt gets fresh identity | none |
| process crash | in-memory owner disappears | may remain | no | garbage managed leftover | none |
| proven stale managed leftover | none | schedule-dependent | no | eventually reclaimed during healthy operation | none |
| unrelated file | outside namespace | no | no | untouched | n/a |
| live-owned managed file seen by scavenger | live owner | no | no | protected until ownership ends | none |

Enumerate scavenger interleavings with live ownership publication/removal barriers. A file
is deletable only when both managed provenance and absence of a live owner are proven. A
finite maintenance schedule moves every proven stale resource to reclaimed during
continued healthy operation; no wall-clock wait or synchronous every-open scan is required.
Run the same oracle with deterministic, random-looking, PID-like, and UUID-like names to
prove that filename syntax, directory layout, identifier scheme, and scavenging schedule
are not conformance requirements.

## V24-L — Resource and internal-error taxonomy

`EC` maps cause, not the physical point of detection, to the final category:

| Condition | Category | Public/internal boundary | Transaction owner | Cleanup oracle |
|---|---|---|---|---|
| hard-gate denial, no exact progress path | `OutOfMemory` | controlled public resource | §39.1 | `CL` |
| catchable denial of supported exact representable allocation | `OutOfMemory` | controlled public resource | §39.1 | unused grant released |
| unsupported in-memory size/address extent | `ExecutionError` representability/resource | controlled public execution | §39.1 | no narrowed operation |
| no exact retained-row/value form | `ExecutionError` representability/resource | controlled public execution | §39.1 | no partial row |
| spill read/write/create failure | `SpillIOError` | controlled public spill | §39.1 | `CL/NS` |
| temporary capacity exhaustion/ENOSPC | `SpillIOError` | controlled public spill | §39.1 | `CL/NS` |
| spill encoding/file-offset/I/O addressability failure | `SpillIOError` | controlled public spill | §39.1 | no I/O |
| malformed spill framing/CRC/count/offset/range | `SpillIOError` | controlled public spill | §39.1 | pre-access rejection |
| cancellation | `QueryCancelled` | controlled public cancellation | §39.1/Ch26 | `CL` |
| double release/accounting underflow | internal invariant | internal | §39 | totals not wrapped |
| blind stale adoption/overwrite | internal invariant | internal | §39 | existing object preserved |
| fresh spill creation cannot be established | `SpillIOError` | controlled public spill | §39.1 | no adopted object |
| non-catchable OS/process termination | outside controlled-return guarantee | external termination | recovery owners only | crash-leftover model |

No test states that semantic errors always beat resource errors or vice versa. Reuse the
Chapter-20 demanded-evaluation and §39.1 publication-boundary procedures. Under identical
explicit budgets, capabilities, allocator faults, filesystem faults, and schedules, `EC`
must reproduce the category; real OS OOM timing is not required deterministic.

## V24-M — Retry, cancellation, teardown, and retained ownership

Extend the existing cancellation resource matrix and Chapter-21 attempt oracle with `ML`,
`OL`, `CL`, and `NS`. Inject success, ordinary semantic error, `ExecutionError`,
`OutOfMemory`, `SpillIOError`, `QueryCancelled`, abandoned pre-write retry, and final query
teardown after each ownership transition. At attempt end, every ending query/attempt charge
is released exactly once, attempt RowCollections and arena pages are gone, spill ownership
is empty, and the next attempt has fresh namespace ownership. Explicitly transferred memory
may survive only under its receiving canonical owner.

Cancellation barriers stop consumers before borrowed owners are torn down. Reuse V23-G for
StringRef/selection/value stability and the Chapter-26 task-graph cleanup procedure. No
teardown order is mandated beyond ending every borrow before releasing its backing
owner; no leaked resource reaches a
later attempt, and executor cleanup does not choose transaction outcome or release
transaction-owned locks.

Spill/reload exact VARCHAR, NULL, NaN variants, `+0.0`, `-0.0`, and three equal row
occurrences. Require fresh runtime ownership, exact bytes/length, NULL independent of stale
payload, Chapter-17 FLOAT semantics, and unchanged multiplicity. Serialized old pointers,
query-arena addresses, reservation handles, row/vector pointers, temporary ownership IDs,
and live counters are forbidden by `PF` from pages, WAL, catalogs, or recovery state.
Crash/reopen must ignore stale spill for WAL replay, transaction status, catalog recovery,
and query continuation.

## V24-N — Resource-path and representation determinism

Here `invariant` means preservation of the owning semantic contract under V24-A's
comparison rules. It does not require identical FLOAT64 roots across legal
aggregate trees. Retained values/NULLs and occurrence membership remain exact;
downstream query/transaction results are checked against the admitted semantic
outcome, with no additional variability introduced by resource policy.

| Perturbation | Successful values/NULL/bag/order/errors | Resource failure may differ? | Transaction/persistent result on matching success | Oracle/status |
|---|---|---:|---|---|
| memory budget | invariant | yes | invariant | `RS/EC` |
| RowCollection block target | invariant | only feasibility | invariant | `RR/RS` |
| spill threshold | invariant | yes if spill path faults | invariant | `RS/EC` |
| spill/no-spill path | invariant | yes for actual I/O fault | invariant | `RS/SF` |
| ordinary/exact oversized retained form | invariant | capability/allocation may differ | invariant | `RR` |
| allocation address | invariant | no semantic effect | invariant | address permutation |
| block/run boundary | invariant | only resource behavior | invariant | `RR/RS` |
| reload order | invariant unless owning order contract | only resource behavior | invariant | order oracle |
| temp filename/namespace encoding | invariant | collision/create fault may differ | invariant | `NS/RS` |
| allocation order | invariant under same semantic demand | controlled resource point may differ | canonical §39.1 result | `DS/EC` |
For multi-row DML, compose V21-7 target-spool semantics with V21-13's V21-13 candidate
and winner oracle while varying memory, spill, and block layout. Compare the same winner
only when execution reaches that semantic decision under the same relevant controlled
resource conditions; do not invent universal semantic-error-versus-OOM precedence.

## V24-O — Mandatory matrices and negative-case checks

The V24-B, D, H, G, I, J, K, L, and N tables are respectively the required
memory-accounting, lifecycle, pressure-progress, retained-row, arithmetic,
spill-validation, spill-lifetime, resource-error, and determinism matrices. The following
invalid-state matrix supplies the common side-effect boundary:

| Invalid state | Classification | Reject before | Persistent/result side effect | Oracle |
|---|---|---|---|---|
| unaccounted live query memory | internal ownership violation | exposure as live state | none | `ML/OL` |
| query/global hard-gate oversubscription | internal accounting violation | grant commit | none | `ML/DS` |
| accounting addition overflow | representability/resource or denial | total update | none | `BI` |
| release underflow/double release | internal invariant | wrapped subtraction | none | `BI/OL` |
| leaked post-grant charge | internal cleanup violation | attempt completion | none | `CL` |
| repeated no-progress pressure cycle | liveness/invariant violation; terminal OOM when exhausted | equivalent retry | none | `PG` |
| ordinary row form outside domain | inapplicable form | truncation/allocation | none | `BI/RR` |
| narrowed oversized row extent | representability violation | row publication | none | `BI/RR` |
| malformed spill length/offset/CRC/framing | `SpillIOError` | dependent allocation/access/I/O | none | `SF` |
| spill addressability overflow | `SpillIOError` | seek/read/write | none | `BI/SF` |
| stale spill adoption/overwrite | internal invariant | ownership acquisition/mutation | none | `NS` |
| live-owned spill scavenged | internal lifetime violation | delete | none | `NS/DS` |
| unrelated file scavenged | namespace violation | delete | none | `NS` |
| retry inherits old spill | attempt-ownership violation | new attempt publication | none | `NS/CL` |

Malformed or invalid states must not cause out-of-bounds access, pointer overflow, stale
value publication, arbitrary overwrite, persistent corruption, or duplicate result
publication. Construction invariants or safe boundary validation may enforce the contract;
no one validation API is required.

The cross-chapter reuse map is:

| Handoff | Contract used by V24 | Reusable verification | Independent oracle |
|---|---|---|---|
| Ch5→24 | durable tuple format is distinct from temporary rows/spill | tuple-format negative registry | `PF` |
| Ch12→24 | temporary spill has no WAL/recovery role | crash/recovery tests | `PF/NS` |
| Ch17→24 | scalar, NULL, FLOAT, VARCHAR domains | type/value tests | `RS/RR` |
| Ch20→24 | row count, bag, order, LIMIT/OFFSET, demand | V20-4/10/11/15 | `RS` |
| Ch21→24 | attempt, retry, DML publication | V21-2/3/7 | attempt/publication oracle |
| Ch22→24 | QueryExecutionContext and runtime ownership | V22-E ownership/context tests | `ML/CL` |
| Ch23→24 | DataChunk/StringHeap, V23-G borrowing, V23-I representability | V23-G/I/J/M | `LG/RR/PF` |
| Ch24→25 | expression/query temporary memory is accounted and owner-safe | expression scratch/resource tests | `ML/LG` |
| Ch24→26 | cancellation, progress, and finalization cleanup | pipeline cancellation/progress tests | `PG/CL/DS` |
| Ch24→28 | retained join state and spill specialize generic ownership | join spill tests | `RR/ML/SF` |
| Ch24→29 | aggregate/DISTINCT retained state and spill | aggregate spill tests | `RR/ML/SF` |
| Ch24→30 | sort/Top-N runs and required order | sort spill/order tests | `RS/SF` |
| Ch24→31 | DML/RETURNING spools and result ownership | V21/DML spool tests | `CL/RR` |
| Ch24→39 | resource categories and transaction consequences | §39 fault matrix | `EC` |

Generic execution procedures elsewhere in this document count as V24 evidence only through
these exact oracles. “Tiny budget forces spill” alone does not prove progress; generic
cancellation cleanup alone does not prove crash namespace safety; ordinary allocation fault
tests alone do not prove committed-capacity coverage; and production decoding alone does not
prove exact extent validation. No methodology may assume only large allocations count,
equate grant with allocation, retry an unchanged denial, treat 256 KiB or heap tuple size as
a retained-row maximum, trust self-generated spill without validation, recover stale spill,
delete an entire unrelated temp directory, or normalize allocation failure merely “where
possible.”

### Complete Chapter-24 atomic architecture-obligation coverage map

| ID | Architecture | Atomic obligation | Verification / independent oracle / reuse |
|---|---|---|---|
| A01 | Ch24 framing | Chapter 17 remains the SQL scalar-domain owner | V24-A; `RS`; type tests |
| A02 | Ch24 framing | Chapter 20 remains the row-count and bag-multiplicity owner | V24-A; `RS`; V20-4 |
| A03 | Ch24 framing | Chapter 20 remains the required-order owner | V24-A/N; `RS`; V20-10 |
| A04 | Ch24 framing | Chapter 20 remains the LIMIT/OFFSET owner | V24-A; `RS`; V20-11 |
| A05 | Ch24 framing | Chapter-24 policy governs finite-resource feasibility | V24-A; capability/budget matrix |
| A06 | Ch24 framing | resource policy does not redefine scalar domains | V24-A; paired `RS` outcomes |
| A07 | Ch24 framing | resource policy does not redefine row count or bag multiplicity | V24-A/N; occurrence oracle |
| A08 | Ch24 framing | resource policy does not redefine required order or LIMIT/OFFSET | V24-A/N; ordering/limit oracle |
| A09 | Ch24 framing | resource policy does not redefine persistent tuple/database formats | V24-A/M; `PF` |
| A10 | Ch24 framing | resource capability may alter success versus legitimate resource failure | V24-A/L; `EC` |
| A11 | Ch24 framing | successful resource paths preserve scalar values | V24-A/N; `RS` |
| A12 | Ch24 framing | successful resource paths preserve NULL state | V24-A/N; `RS` |
| A13 | Ch24 framing | successful resource paths preserve logical occurrences/multiplicity | V24-A/N; `RS` |
| A14 | Ch24 framing | successful resource paths preserve required SQL order | V24-A/N; `RS` |
| A15 | Ch24 framing | successful resource paths preserve demanded semantic errors | V24-A/L/N; V20-15 |
| A16 | Ch24 framing | successful resource paths preserve transaction effects | V24-A/L; §39.1 oracle |
| A17 | Ch24 framing | successful resource paths preserve persistent database result | V24-A/N; durable-byte snapshot |
| B01 | §24.1 | temporary RowLayout is distinct from persistent heap tuple layout | V24-F/G/M; `PF/RR` |
| B02 | §24.1 | temporary RowLayout is distinct from columnar DataChunk layout | V24-F; `RR` + V23-A |
| B03 | §24.1 | RowLayout derives from resolved logical types | V24-F; schema/type oracle |
| B04 | §24.1 | retained rows represent NULL independently | V24-F/M; `RR` |
| B05 | §24.1 | retained rows represent fixed-width values exactly | V24-F; `RR` |
| B06 | §24.1 | variable values use exact offset/length descriptors where that form applies | V24-F/I; `BI/RR` |
| B07 | §24.1 | variable payload has row/block-owned storage | V24-F/M; owner graph |
| B08 | §24.1 | optional operator metadata remains operator-owned | V24-B/F; `ML` |
| B09 | §24.1 | temporary rows do not acquire persistent MVCC headers | V24-F/M; `PF` |
| B10 | §24.1 | movable/serializable rows do not depend on embedded stable raw pointers | V24-F/J/M; pointer poison |
| B11 | §24.2 | RowCollection stores temporary rows in append-oriented blocks | V24-F; append/iterate `RR` |
| B12 | §24.2 | ordinary block target is 256 KiB configuration/tuning state | V24-G/N; target-boundary fixtures |
| B13 | §24.2 | ordinary block target is not a correctness maximum | V24-G; symbolic target+1 |
| B14 | §24.2 | ordinary forms do not define SQL row or VARCHAR limits | V24-A/G; `RS/RR` |
| B15 | §24.2 | ordinary forms do not define SQL cardinality or heap-format limits | V24-A/G; `RS/PF` |
| B16 | §24.2 | an ordinary retained form is applicable only when every extent is exact | V24-G/I; `BI` |
| B17 | §24.2 | an oversized retained row/value is never truncated | V24-G; exact witness |
| B18 | §24.2 | oversized retained extents are never wrapped or clipped | V24-G/I; `BI` |
| B19 | §24.2 | ordinary-form inapplicability is neither invalid SQL nor semantic splitting | V24-G; `RS/RR/EC` |
| B20 | §24.2 | any supported exact alternate retained representation is permitted | V24-G; capability matrix |
| B21 | §24.2 | physical segmentation preserves one occurrence and every scalar | V24-G; segmented `RR` |
| B22 | §24.2 | an incapable ordinary form cannot cause failure when an exact supported form exists | V24-G; adversarial chooser |
| B23 | §24.2 | absence of every exact retained form yields representability/resource ExecutionError | V24-G/L; `EC` |
| B24 | §24.2 | allocation denial for an exact supported retained form yields OutOfMemory | V24-G/L; injected allocator |
| B25 | §24.2 | persistent heap tuple maximum is not a runtime retained-row maximum | V24-G/M; `PF` |
| B26 | §24.2 | exact V23-I scalar representation does not imply retained-form applicability | V24-G; V23-I + `RR` |
| B27 | §24.2 | append-oriented storage preserves each retained occurrence | V24-F; `RR` bag oracle |
| B28 | §24.2 | in-memory row handles remain stable only for the collection/block lifetime | V24-F/M; generation graph |
| B29 | §24.2 | row handles are query-local and not persistent RIDs | V24-F/M; identity registry |
| B30 | §24.2 | associated variable payload remains owned for row lifetime | V24-F/M; `LG/RR` |
| B31 | §24.2 | reconstructed spill handles are distinct from in-memory handles | V24-F/K; handle-domain oracle |
| B32 | §24.2 | RowCollection/block lifetime permits one coherent bulk deallocation | V24-F/M; `CL` |
| C01 | §24.3 | each query owns its QueryArena | V24-E; `ML/CL` |
| C02 | §24.3 | QueryArena backing is bulk-released at query end | V24-E/M; `CL` |
| C03 | §24.3 | QueryArena cannot own unbounded execution-dependent data | V24-E; owner classification |
| C04 | §24.3 | QueryArena backing capacity is query-memory accounted | V24-B/E; `ML` |
| C05 | §24.3 | QueryArena backing is nonspillable query memory | V24-B/E; spillability ledger |
| C06 | §24.3 | many small arena allocations cannot bypass the budget | V24-B/E; page-growth fixture |
| C07 | §24.3 | objects within an accounted arena page need no duplicate charge | V24-B/E; parent-region oracle |
| C08 | §24.4 | QueryMemoryManager owns per-query accounting | V24-B/C; owner map |
| C09 | §24.4 | QueryMemoryManager composes the global execution ledger | V24-B/C; `ML` |
| C10 | §24.4 | operator reservations have explicit accounting ownership | V24-B/D; `ML/OL` |
| C11 | §24.4 | spill pressure and hard-limit enforcement use the same coherent owner model | V24-C/H; `ML/PG` |
| C12 | §24.4 | global limit, per-query soft budget, and per-query hard maximum remain distinct | V24-A/C/H; policy matrix |
| C13 | §24.4 | live held capacity, owner, and spillability are tracked coherently | V24-B/D; `ML` |
| C14 | §24.4 | accounting measures logical committed capacity rather than exact RSS | V24-B; symbolic capacity |
| C15 | §24.4 | every potentially unbounded query-owned region has live coverage | V24-B; complete region inventory |
| C16 | §24.4 | QueryArena backing participates in the complete universe | V24-B/E; `ML` |
| C17 | §24.4 | query-owned DataChunk/vector capacity participates | V24-B; `ML` + V23 |
| C18 | §24.4 | query-owned StringHeap/SelectionVector capacity participates | V24-B; `ML/LG` |
| C19 | §24.4 | RowCollection fixed payload participates | V24-B/F; `ML/RR` |
| C20 | §24.4 | RowCollection variable payload participates | V24-B/F; `ML/RR` |
| C21 | §24.4 | RowCollection block metadata participates | V24-B; metadata-growth fixture |
| C22 | §24.4 | dynamic block/run/partition directories participate | V24-B; metadata-growth fixture |
| C23 | §24.4 | hash/aggregate/DISTINCT/sort retained state participates | V24-B; operator-owner matrix |
| C24 | §24.4 | execution-owned DML/result spools participate | V24-B/M; `ML/CL` |
| C25 | §§24.4,24.9 | spill read/write buffers and reloaded blocks participate | V24-B; `ML/OL` |
| C26 | §24.4 | query-owned worker-local dynamic storage participates | V24-B/C; `ML/DS` |
| C27 | §24.4 | accounting granularity may be page/allocation/buffer/block/collection/operator | V24-B; multi-granularity fixtures |
| C28 | §24.4 | each region has one conceptual owner in each applicable ledger | V24-B/D; `ML` |
| C29 | §24.4 | a parent region may cover children without per-object double charge | V24-B; coverage map |
| C30 | §§24.4–24.5 | transfer overlap is allowed only while coverage remains continuous | V24-B/D; interval oracle |
| C31 | §24.4 | individually small allocations are not exempt when aggregate growth is unbounded | V24-B; many-small fixture |
| C32 | §24.4 | unbounded metadata has an accounted owner | V24-B; row/block/run growth |
| C33 | §24.4 | charge is no smaller than committed query-available capacity | V24-B; 1000→1024 fixture |
| C34 | §24.4 | conservative over-accounting is permitted | V24-B; 1024→2048 fixture |
| C35 | §24.4 | systematic under-accounting is forbidden | V24-B; 1024→1000 fixture |
| C36 | §24.4 | allocator metadata/page tables/RSS need not be measured exactly | V24-B; excluded-cost oracle |
| C37 | §24.4 | exemptions require a separate bounded owner or independent fixed bound | V24-B; exemption proof table |
| C38 | §24.4 | BufferPool frame capacity remains under its separate owner | V24-B/M; owner map |
| C39 | §24.4 | query/global gate check and update is one logical atomic transition | V24-C; `ML/DS` |
| C40 | §24.4 | concurrent stale checks cannot collectively oversubscribe a hard gate | V24-C; barrier interleavings |
| C41 | §24.4 | recorded peak query bytes equal the maximum coherent live total observed | V24-B; `ML` event maximum |
| D01 | §24.5 | a request creates no charge | V24-D; `OL` |
| D02 | §24.5 | a request creates no live query-owned memory | V24-D; `OL` |
| D03 | §24.5 | an accounting grant creates budget authority/live charge | V24-D; `OL/ML` |
| D04 | §24.5 | an accounting grant does not prove physical allocation | V24-D; grant-only state |
| D05 | §24.5 | provisional physical allocation is distinct from grant | V24-D; `OL` |
| D06 | §24.5 | exposure/retention as live query state is a separate ownership transition | V24-D; publication event |
| D07 | §24.5 | live query-owned capacity never precedes sufficient ledger coverage | V24-D; interval invariant |
| D08 | §24.5 | reserve-before-allocation is valid but not mandatory | V24-D; dual ordering fixtures |
| D09 | §24.5 | allocate-first provisional storage remains private | V24-D; accessibility probe |
| D10 | §24.5 | allocate-first storage is already charged or independently bounded | V24-D; `ML` |
| D11 | §24.5 | allocate-first storage is not exposed or retained as query state | V24-D; publication counter |
| D12 | §24.5 | failed allocate-first accounting immediately discards provisional storage | V24-D/M; `CL` |
| D13 | §24.5 | allocation ordering cannot create an unbounded transient/budget bypass | V24-D; bounded-state proof |
| D14 | §24.5 | allocator/container rounding to A requires charge for A before exposure | V24-B/D; `ML` |
| D15 | §24.5 | enlarge/expose-covered/discard mechanisms may all conform | V24-D; mechanism matrix |
| D16 | §24.5 | positive in-place committed-capacity delta is charged before live growth | V24-D; delta fixture |
| D17 | §§24.5,24.10 | catchable supported exact allocation denial is OutOfMemory | V24-D/L; `EC` |
| D18 | §24.5 | failed post-grant allocation releases the unused grant | V24-D/M; `OL/CL` |
| D19 | §24.5 | failed growth leaves prior memory/accounting coherent | V24-D; pre/post snapshot |
| D20 | §24.5 | failed allocation publishes no partial live state | V24-D; poison/publication counter |
| D21 | §§24.5,24.10 | unsupported runtime size/address domain is ExecutionError, not OOM | V24-D/I/L; `BI/EC` |
| D22 | §§24.5,24.10 | non-catchable process termination is outside controlled return | V24-D/K/L; crash model |
| D23 | §24.5 | every live charge has one owner for the full interval | V24-D; `OL` |
| D24 | §24.5 | ownership transfer preserves continuous accounting | V24-D; interval graph |
| D25 | §24.5 | logically atomic handoff or conservative overlap may implement transfer | V24-D; two valid traces |
| D26 | §24.5 | transfer never creates an unaccounted gap | V24-D; coverage assertion |
| D27 | §24.5 | ownership leaving query execution is established under the receiving owner | V24-D/M; owner registry |
| D28 | §24.5 | release occurs exactly once when ownership ends | V24-D/M; `OL/CL` |
| D29 | §24.5 | release updates each applicable ledger exactly once | V24-C/D; `ML` |
| D30 | §§24.5,24.10 | double release is an internal invariant violation | V24-D/L; `BI/EC` |
| D31 | §§24.5,24.10 | underflow or release of unowned capacity is internal | V24-D/I/L; `BI/EC` |
| E01 | §24.6 | the per-query soft budget is a pressure signal | V24-H; policy-state fixture |
| E02 | §24.6 | soft-budget crossing may request reduction before the hard gate | V24-H; threshold trace |
| E03 | §24.6 | the query/global hard limit is the growth gate | V24-C/H; `ML/PG` |
| E04 | §24.6 | a denied owner uses an applicable exact pressure action when its contract provides one | V24-H; action-capability graph |
| E05 | §24.6 | release/spill/lower-memory/finite fallback actions remain exact | V24-H/N; `PG/RS` |
| E06 | §24.6 | retry occurs only after relevant state changes | V24-H; edge predicate |
| E07 | §24.6 | reducing live relevant capacity qualifies as progress | V24-H; `PG` |
| E08 | §24.6 | reducing required charge qualifies as progress | V24-H; `PG` |
| E09 | §24.6 | freeing an eligible owner qualifies as progress | V24-H; `PG/ML` |
| E10 | §24.6 | replacing state with an exact lower-memory form qualifies | V24-H; `PG/RR` |
| E11 | §24.6 | well-founded finite spill/repartition advance may qualify | V24-H; symbolic metric |
| E12 | §24.6 | repeating an unchanged denial is not progress | V24-H; cycle detector |
| E13 | §24.6 | writing spill bytes while freeing no useful capacity is not progress | V24-H; zero-release fixture |
| E14 | §24.6 | cycling equivalent pressure states is forbidden | V24-H; graph SCC oracle |
| E15 | §24.6 | each pressure cycle grants, terminates, or strictly advances | V24-H; transition exhaustiveness |
| E16 | §24.6 | no fixed global retry count is required | V24-H; metric-length variants |
| E17 | §§24.6,24.10 | no remaining exact progress path yields OutOfMemory | V24-H/L; `PG/EC` |
| E18 | §§24.6,24.10 | actual spill I/O/create/capacity/ENOSPC failure yields SpillIOError | V24-H/K/L; fake filesystem |
| E19 | §§24.6,24.10 | cancellation during pressure yields QueryCancelled | V24-H/L/M; barrier |
| E20 | §24.6 | pressure preserves values and NULL state | V24-H/N; `RS` |
| E21 | §24.6 | pressure preserves bag, required order, demanded errors, and transaction behavior | V24-H/N; `RS`/§39.1 |
| E22 | §24.6 | pressure cannot approximate, truncate, deduplicate, drop, or violate order | V24-H; negative semantic matrix |
| E23 | §24.6 | pressure handling does not introduce optimizer re-entry/replanning | V24-H; event graph |
| E24 | §24.6 | reclaim callback executes without recursive accounting-lock coupling | V24-H; lock event graph |
| E25 | §24.6 | ordinary catchable allocator denial cannot escape as arbitrary bad_alloc/process failure | V24-D/L; injected exception + `EC` |
| F01 | §24.1 | every Chapter-24 size/count/offset/capacity/extent is mathematical first | V24-I; `BI` |
| F02 | §24.1 | exact value is representable in every consuming domain before use | V24-I/J; domain predicates |
| F03 | §24.1 | count-times-width is checked exactly | V24-I; `BI` |
| F04 | §24.1 | fixed-plus-variable bytes is checked exactly | V24-I; `BI` |
| F05 | §24.1 | offset-plus-length is checked exactly | V24-I/J; `BI` |
| F06 | §24.1 | row and block extents are checked exactly | V24-G/I; `BI/RR` |
| F07 | §§24.1,24.4 | live-total-plus-charge is checked exactly | V24-C/I; `BI/ML` |
| F08 | §§24.1,24.5 | charge subtraction is checked exactly | V24-D/I; `BI` |
| F09 | §24.1 | block/run/partition count growth is checked exactly | V24-I; `BI` |
| F10 | §§24.1,24.8 | spill payload length is checked exactly | V24-I/J; `BI/SF` |
| F11 | §§24.1,24.8 | spill record/header-plus-payload extent is checked exactly | V24-I/J; `BI/SF` |
| F12 | §§24.1,24.8 | file-offset-plus-I/O-length is checked exactly | V24-I/J; `BI/SF` |
| F13 | §24.1 | allocation extent is checked before allocator use | V24-I; attempt counter |
| F14 | §§24.1,24.5 | capacity rounding arithmetic is checked | V24-D/I; `BI` |
| F15 | §24.1 | signed overflow is forbidden | V24-I; boundary faults |
| F16 | §24.1 | unsigned wraparound is forbidden | V24-I; boundary faults |
| F17 | §24.1 | modulo reduction is forbidden | V24-I; symbolic witness |
| F18 | §24.1 | narrowing truncation is forbidden | V24-I; domain witness |
| F19 | §24.1 | pointer-range overflow is forbidden | V24-I/J; pointer-attempt counter |
| F20 | §24.1 | file-offset truncation is forbidden | V24-I/J; I/O-attempt counter |
| F21 | §24.1 | conformance does not require one integer type or checked library | V24-I; multiple safe mechanism fixtures |
| F22 | §24.8 | spill framing validates magic and temporary version | V24-J; independent `SF` |
| F23 | §24.8 | spill framing validates owner/run/partition identity, payload length, and row count | V24-J; `SF` |
| F24 | §24.8 | spill framing validates CRC and payload ownership/extent | V24-J; `SF` |
| F25 | §24.8 | temporary integers are explicitly serialized rather than native dumped | V24-J/M; byte codec oracle |
| F26 | §24.8 | CRC covers the metadata/payload defined by the owning temporary format | V24-J; independent CRC |
| F27 | §§24.8,24.10 | unsupported spill encoding/file/I/O addressability yields SpillIOError | V24-I/J/L; `BI/EC` |
| F28 | §24.8 | an incapable spill form is inapplicable when an exact supported form exists | V24-J; capability chooser |
| F29 | §24.8 | decoded metadata is validated before dependent allocation | V24-J; allocation counter |
| F30 | §24.8 | decoded metadata is validated before pointer arithmetic/range formation | V24-J; pointer counter |
| F31 | §24.8 | decoded metadata is validated before memory dereference | V24-J; poison/read counter |
| F32 | §24.8 | decoded metadata is validated before seek/read/write | V24-J; fake-I/O counter |
| F33 | §24.8 | complete framing and recognized magic/version are prerequisites | V24-J; truncated/bad-header fixtures |
| F34 | §24.8 | lengths, counts, and count-derived extents are representable/exact first | V24-J; `BI/SF` |
| F35 | §24.8 | payload ranges, file ranges, CRC, and owner structure are validated | V24-J; one-defect matrix |
| F36 | §24.8 | no unvalidated size/count/offset drives memory or file access | V24-J; all attempt counters zero |
| F37 | §24.8 | self-generated spill remains structurally validated | V24-J; crash/partial/stale/fault fixtures |
| F38 | §24.8 | malformed temporary bytes are handled memory-safely | V24-J/O; poison/side-effect oracle |
| F39 | §§24.8,24.10 | malformed spill is SpillIOError, not persistent corruption | V24-J/L; `EC/PF` |
| F40 | §24.8 | impossible self-generated in-memory state remains an internal defect | V24-J/L; construction-state oracle |
| F41 | §24.8 | temporary spill has no long-lived persistent-format ABI promise | V24-J/M; `PF` |
| F42 | §24.8 | specialized join/aggregate/sort payload semantics remain with Chapters 28–30 | V24-J/O; cross-owner map |
| G01 | §24.7 | every query has one generic SpillManager owner | V24-K/M; `NS/CL` |
| G02 | §24.7 | spill resources live under a managed temporary location | V24-K; `NS` |
| G03 | §24.7 | every live spill resource has fresh exclusive ownership | V24-K; exclusive-create oracle |
| G04 | §24.7 | namespace ownership distinguishes live queries | V24-K; owner tuple |
| G05 | §24.7 | namespace ownership distinguishes statement attempts | V24-K; retry fixture |
| G06 | §24.7 | namespace ownership isolates processes/database instances sharing temp storage | V24-K; cross-owner collision |
| G07 | §24.7 | namespace ownership isolates stale crash leftovers | V24-K; stale collision |
| G08 | §24.7 | filename/identifier/directory/temp-metadata mechanism remains free | V24-K; naming permutations |
| G09 | §24.7 | SpillManager owns query/attempt temp-resource lifecycle | V24-K/M; `NS/CL` |
| G10 | §24.7 | SpillManager owns temporary block/run allocation and I/O helpers | V24-K; owner event graph |
| G11 | §24.7 | SpillManager owns cleanup on normal/error/cancel/abandoned attempt | V24-K/M; cleanup matrix |
| G12 | §24.7 | spill data is temporary | V24-K/M; `PF` |
| G13 | §24.7 | spill data is not WAL logged | V24-K/M; WAL-byte snapshot |
| G14 | §24.7 | spill data is not part of crash recovery | V24-K/M; reopen oracle |
| G15 | §24.7 | spill data is not a persistent database format | V24-K/M; `PF` |
| G16 | §24.7 | spill creation establishes fresh ownership | V24-K; exclusive-create transition |
| G17 | §24.7 | a new owner cannot adopt a colliding pre-existing spill object | V24-K; stale/live collision |
| G18 | §24.7 | a new owner cannot trust colliding pre-existing contents | V24-K/J; poison contents |
| G19 | §24.7 | a new owner cannot blindly overwrite a colliding object | V24-K; write counter |
| G20 | §§24.7,24.10 | collision establishes another identity or safely fails SpillIOError | V24-K/L; `NS/EC` |
| G21 | §§24.7,24.10 | blind stale adoption/overwrite is an internal invariant defect | V24-K/L; `EC` |
| G22 | §24.7 | successful query/attempt teardown deletes owned spill | V24-K/M; `CL/NS` |
| G23 | §24.7 | ordinary/resource/SpillIO error teardown deletes owned spill | V24-K/M; cleanup matrix |
| G24 | §24.7 | cancellation teardown deletes owned spill | V24-K/M; barrier + `NS` |
| G25 | §24.7 | abandoned retry-attempt teardown deletes old spill | V24-K/M; attempt oracle |
| G26 | §24.7 | restarted attempt gets fresh ownership and never inherits old spill | V24-K/M; owner tuple |
| G27 | §24.7 | process crash removes in-memory query ownership | V24-K; crash transition |
| G28 | §24.7 | crash may leave physical spill files | V24-K; fake process death |
| G29 | §24.7 | crash leftovers are garbage only, never query/database state | V24-K/M; `NS/PF` |
| G30 | §24.7 | spill requires no fsync or recovery protocol | V24-K/M; negative registry |
| G31 | §24.7 | proven stale managed resources are eventually reclaimed in healthy operation | V24-K; finite scheduler |
| G32 | §24.7 | known stale spill cannot accumulate without bound in healthy operation | V24-K; liveness metric |
| G33 | §24.7 | initialization/startup/periodic/allocation-local schedules may conform | V24-K; schedule variants |
| G34 | §24.7 | complete synchronous deletion at every database open is not required | V24-K; delayed safe schedule |
| G35 | §24.7 | reclamation deletes only proven managed resources | V24-K; provenance oracle |
| G36 | §24.7 | reclamation deletes only resources with no live owner | V24-K; live/scavenger interleavings |
| G37 | §24.7 | live-owned and unrelated files remain untouched | V24-K; protection fixtures |
| G38 | §24.7 | no durable spill identity/UUID/WAL/fsync/recovery format is mandated | V24-K/M; mechanism permutations |
| H01 | §24.10 | error/cancellation unwind releases MemoryReservation ownership | V24-D/M; `OL/CL` |
| H02 | §24.10 | error/cancellation unwind releases RowCollections | V24-F/M; `CL` |
| H03 | §24.10 | error/cancellation unwind releases arena backing pages | V24-E/M; `ML/CL` |
| H04 | §24.10 | error/cancellation unwind releases owned spill files | V24-K/M; `NS/CL` |
| H05 | §24.10 | error/cancellation unwind releases operator state | V24-M; task/resource graph |
| H06 | §24.10 | cleanup does not itself commit or abort the transaction | V24-L/M; §39.1 oracle |
| H07 | §24.10 | §39.1 publication boundary alone owns transaction consequence | V24-L/M; publication matrix |
| H08 | §§24.1,24.8,24.11 | spill serialization never dumps native pointer/object memory | V24-J/M; byte registry |
| H09 | §§24.7–24.11 | runtime rows, handles, counters, and temp IDs never become persistent state | V24-M; `PF` |
| H10 | §§24.2,24.11 | retained borrowed values obtain stable exact ownership | V24-F/M; V23-G `LG` |
| H11 | §§24.7–24.8 | reloaded VARCHAR has exact bytes/length and a fresh valid owner | V24-J/M; `RR/LG` |
| H12 | §§24.1–24.8 | spill/reload preserves NULL independently of payload | V24-F/J/M; `RR` |
| H13 | §§24.6,24.8 | spill/reload preserves Chapter-17 NaN and signed-zero semantics | V24-M/N; `RS` |
| H14 | §§24.6,24.8 | spill/reload preserves repeated equal row occurrences | V24-M/N; bag oracle |
| H15 | §§24.2,24.6,24.11 | block/run/reload placement does not create SQL identity/order | V24-A/F/N; `RS/RR` |
| H16 | §§24.6,24.10 | DML memory/spill variation preserves V21-13 under matching resource conditions | V24-N; V21-7 target-spool and V21-13 winner oracles |

Non-falsifiable or non-correctness Chapter-24 material is accounted for separately:

| ID | Architecture | Material | N/A justification | Status |
|---|---|---|---|---|
| X01 | §24.1 | hash/link/run metadata examples | illustrative examples; B08/C23 test the owned-metadata rule | N/A |
| X02 | §24.2 | minimal per-row allocation preference | performance preference; Chapter 42 owns measurement | N/A |
| X03 | §24.3 | examples of small arena objects | illustrative owner vocabulary; C01–C07 test the contract | N/A |
| X04 | §24.4 | deployment defaults are not architecture constants | implementation/configuration freedom, not one falsifiable value | N/A |
| X05 | §24.5 | hash/aggregate/sort/DML/result reservation examples | illustrative navigation; C23–C24 and operator families test ownership | N/A |
| X06 | §24.8 | Chapters 28–30 specialize operator payloads | navigation; F42 and the cross-chapter map verify the handoff | N/A |
| X07 | §24.9 | sequential-I/O preference, ~1 MiB target, and avoiding tiny writes | performance/tuning methodology belongs to Chapter 42 benchmarks; C25 still verifies accounting | N/A |
| X08 | §24.11 | numbered invariant list restates detailed §§24.1–24.10 | consolidated index; A01–H16 cover each falsifiable rule | N/A |
| X09 | §24.3 | bump-oriented arena allocation strategy | implementation/performance mechanism; C01–C07 own lifetime and accounting correctness | N/A |

The closed-set and stale-rule audit for this family requires all of the following:

- no procedure treats only individually large allocations as accounted, exempts unbounded
  metadata/QueryArena state, or uses RSS/production counters as the oracle;
- no procedure conflates request, grant, allocation, and live ownership or leaves a failed
  allocation charge live;
- no pressure procedure accepts zero-useful spill, unchanged retry, arbitrary fixed retry,
  or runtime optimizer re-entry as progress;
- no retained-row procedure treats 256 KiB, a descriptor width, or heap tuple capacity as a
  SQL limit;
- no arithmetic or spill procedure uses unchecked native arithmetic or a production decoder
  as its expected result;
- no temporary-state procedure trusts self-generated bytes, adopts stale spill, requires
  spill recovery, scavenges unrelated/live files, or requires synchronous every-open
  deletion;
- no error procedure uses vague allocation normalization, invents semantic-error precedence,
  or changes §39.1 transaction ownership;
- no sleep, allocator address, scheduler accident, or filesystem enumeration order
  substitutes for controlled verification evidence.
