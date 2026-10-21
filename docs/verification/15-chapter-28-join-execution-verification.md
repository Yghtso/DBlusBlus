# Chapter 28 — Join Execution Verification

This family verifies `ARCHITECTURE.md` Chapter 28. It derives expected results from logical
bags and canonical component owners, never from one production join algorithm. Verify each obligation through a deterministic procedure and independent oracle,
directly or by the exact V28-U reuse entry.

Use these independent test-side oracles:

| Oracle | Independent construction |
|---|---|
| `JB` | Tagged logical-left/right occurrence bags and mathematical INNER/CROSS/LEFT evaluator |
| `LE` | Per-left qualifying-pair and exact right-null-extension model |
| `HE` | Chapter-17 typed equality/hash-compatibility model, including NULL, FLOAT64, and byte VARCHAR |
| `HC` | Controllable same-hash candidate directory with independent full-key equality |
| `DO` | Tagged duplicate-occurrence/multiplicity ledger independent of payload identity |
| `NC` | Materialized-right nested-loop Cartesian cursor model |
| `IC` | V27 index/RID/MVCC/residual candidate model specialized per outer occurrence |
| `BG` | Build-value owner graph and Chapter-24 accounting ledger |
| `RD` | Build acceptance/Combine/Finalize/probe-readiness dependency automaton |
| `PC` | Accepted-probe lifecycle and match-chain continuation automaton |
| `GP` | Grace partition/replay and bounded recursive-progress model |
| `MS` | Capability-valid equality merge of independently ordered tagged groups |
| `AS` | Logical-result and owner-defined algorithm-substitution equivalence relation |
| `SS` | Ordered physical-schema, LogicalSlotId, nullability, and RequiredSlotSet model |
| `OP` | Chapter-37 OrderingProperty satisfaction model |
| `DM` | Chapter-20/26 semantic-demand and early-stop relation |
| `ER` | V25-J/V21-13 owner-selected error/provenance model |
| `RM` | Chapter-24 exact extent, accounting, representation, and resource classifier |
| `AF` | Fresh-attempt generation and runtime-only persistence-negative ledger |
| `IX` | One-defect invalid-state model with forbidden-access/output/effect counters |

Production hash functions, joins, tables, cursors, output order, and error discovery are
observations, never oracles. Fixtures use tagged logical occurrences, exact typed values,
controlled collisions, symbolic counts, explicit lifecycle traces, and enumerated worker
orders. Use no sleeps, wall-clock races, allocator placement, pointer order, filesystem
enumeration, bucket iteration, or scheduling luck.

## V28-A — Operator, join-type, role, and owner inventory

Validate every selected operator against immutable plan metadata and this matrix. Poison
runtime name lookup and plan mutation. RIGHT, FULL, SEMI, ANTI, and null-aware anti are not
positive Chapter-28 execution alternatives and must not be admitted by capability or
final-plan validation.

| Operator | Supported logical types | Logical/physical sides | Pipeline shape | Continuation/spill | Property/scheman owner |
|---|---|---|---|---|---|
| `PhysicalNestedLoopJoin` | INNER, LEFT, CROSS | materialize logical right; probe logical left | blocking right then streaming left | Cartesian cursor; accounted materialization | Ch20/22/37 |
| `PhysicalIndexNestedLoopJoin` | applicable INNER/LEFT | outer logical left; indexed logical right | streaming outer plus indexed lookup | outer/index/RID cursor; no join spill | Ch20/22/27/36/37 |
| `PhysicalHashJoin` | INNER, LEFT | INNER either legal build; LEFT right build/left preserved probe | build Sink, Finalize, probe streaming | duplicate-chain cursor; Grace spill | Ch20/26/28/37 |
| `PhysicalMergeJoin` | capability-valid equality forms, including valid LEFT semantics | logical sides remain scheman owners | ordered incremental group processing | duplicate-group cursor | Ch22/28/37/38 |

Chapter 17 owns values/equality; Chapter 20 owns bags, join truth, null extension, and
demand; Chapter 21 owns DML selection/publication; Chapters 22/36–38 own applicability,
schema, slots, and properties; Chapters 23–26 own values, resources, expressions, and
lifecycle; Chapters 31–32 own external publication and parallel dependencies; §39 owns
terminal categories and transaction consequences. Chapter 28 supplies algorithm execution,
orientation, candidate/recheck, continuation, ownership, and spill specialization.

## V28-B — Mathematical join bags, multiplicity, and NULL extension

Give every logical occurrence a unique test tag even when values and physical payloads are
equal. `JB` enumerates `L × R` mathematically and evaluates the complete ON predicate with
Chapter-17 3VL. INNER emits each TRUE pair once; FALSE/UNKNOWN emit none. CROSS emits every
pair without a predicate. LEFT uses `LE`: emit every TRUE pair, or exactly one occurrence
containing the left tag and typed-null right fields when no pair is TRUE.

| Case | Candidate/qualifying pairs | Expected output | Dedup? |
|---|---|---|---:|
| `0 × N` INNER/CROSS | zero | zero | no |
| `1 × 1` TRUE | one/one | one joined occurrence | no |
| `1 × N`, `M × 1`, `M × N` all TRUE | mathematical product | exact product streamed | no |
| FALSE / UNKNOWN | pair exists/nonqualifying | none for that pair | no |
| repeated CONSTANT/DICTIONARY payload | per logical occurrence | full tagged multiplicity | no |
| LEFT no TRUE pair | all candidates rejected/absent | exactly one right-null-extended occurrence | no |
| LEFT one/N TRUE pairs | one/N | one/N joined rows and no null row | no |

Use symbolic `M*N` beyond fixed-width domains without allocating those rows. Compare
incremental emitted tags and a mathematical count, never a finite-width product. For null
extension, verify unchanged TypeIds/LogicalSlotIds, right validity false, and poison payload
inaccessibility; stale output bytes must not become values.

## V28-C — Hash/equality compatibility

Generate values independently of production hashing. For each equality class choose several
test hashes satisfying equal-implies-compatible lookup; separately force unequal collisions.
The production hash algorithm and seed may vary, but observed lookup must obey `HE`.

| Key case | Equality/hash expectation | Candidate/equality action |
|---|---|---|
| BOOLEAN, INT32, INT64, DATE, TIMESTAMP | exact typed equality; equal lookup-compatible | exact full recheck |
| FLOAT64 `-0.0` / `+0.0` | equal and hash-compatible | match after exact equality |
| canonical NaN variants | Chapter-17 equal and hash-compatible | match after canonical equality |
| VARCHAR embedded NUL/high bytes | exact length and bytes | no C-string/prefix equality |
| equal prefix, different suffix/length | unequal; collision permitted | full equality rejects |
| composite non-NULL | ordered typed components, unambiguous boundaries | every component rechecked |
| NULL/NULL or NULL/non-NULL | ordinary key nonmatchable | do not read poisoned NULL payload |
| composite with any NULL | nonmatchable | no ordinary match |

Hash values, component encodings, and mix functions remain query-local. Relocate values and
change seed/mix while retaining `HE`; no persistent hash bytes or exact function are tested.

## V28-D — Collision directory and duplicate chains

`HC` creates unequal keys with one controlled hash and equal keys with independently tagged
build occurrences. Record directory probes, representative comparisons, chain insertions,
and outputs. Same hash enters a candidate path but cannot establish equality or LEFT
matchedness. Every same-hash candidate receives full key equality; different keys continue
the directory probe sequence. Equal keys share a chain without losing occurrences.

| Build/probe case | Directory result | Logical result |
|---|---|---|
| unequal keys, different hash | no compatible candidate | no match |
| unequal keys, same hash | representative/full-key rejection | no match |
| equal key, one build row | one chain member | one result if residual TRUE |
| equal key, N build occurrences | N recoverable members | N results per matching probe |
| CONSTANT payload cardinality N | physical sharing, N tags | N members/results |
| repeated DICTIONARY index | repeated tags | repeated members/results |

Resize and vary open-addressing shape/load below and across the ordinary target. Directory
changes preserve every entry and chain; the approximately `0.70` target is tuning, not a SQL
limit, persistent format, or multiplicity bound.

## V28-E — PhysicalNestedLoopJoin

Materialize tagged logical-right occurrences once into test-owned `RowCollection` state,
then poison/reuse all producer chunks. `NC` walks each demanded logical-left occurrence
against every materialized right occurrence. This proves the algorithm does not implicitly
rewind a source and that retained fixed/varlen values, duplicates, and oversized exact forms
remain stable and accounted.

Use right sizes 0, 1, capacity, and capacity+1 and vary left/right chunking. When output fills
mid-product, continuation resumes at the first unoffered `(left-tag,right-tag)` pair. No
restart, skip, or full-product materialization is permitted.

| Exact right-side state | INNER/CROSS | LEFT | Remaining left demand |
|---|---|---|---|
| empty | empty | one null-extended row per left | unnecessary for INNER/CROSS after exact emptiness; required for LEFT |
| one/N rows | TRUE pairs/product | matches or null extension | per JB/LE |
| retained VARCHAR/oversized row | exact bytes/value | exact bytes/value | storage remains live |

## V28-F — PhysicalIndexNestedLoopJoin

For each tagged outer/logical-left occurrence, `IC` independently evaluates lookup keys,
enumerates the exact index range, validates every heap reference, applies the attempt's MVCC
view, and evaluates the residual. Index membership is only candidate generation. Include no
entry, invisible/dead candidates, wrong references, residual FALSE/UNKNOWN/error, one TRUE,
duplicate TRUE entries across leaves, and capacity+1 qualifying inner occurrences.

LEFT emits one typed-null inner row only after the complete demanded candidate range has no
TRUE survivor. Continuation retains the outer occurrence and first unvisited index/RID
candidate, accepts outer input once, and offers final output before lifecycle resolution or
terminal. Reuse V27-E/F/G for range, RID, stale-entry, and heap visibility mechanics.

## V28-G — Hash-build ownership and RequiredSlotSet

`BG` labels every backing region, borrower, accounted owner, and release edge. Reset and
poison incoming build chunks after successful Sink acceptance; all later key comparisons,
residuals, and output must still resolve exact values.

| Retained item | Required owner/lifetime | Pruning/accounting check |
|---|---|---|
| fixed key/cached hash | build state through all probes | key-required; accounted |
| VARCHAR key/payload | deep/stable build ownership, exact bytes | no dangling StringRef; accounted |
| residual-only build column | through all candidate evaluations | cannot prune |
| output-only build column | through output copy/borrow completion | cannot prune |
| duplicate occurrence/chain link | individually recoverable or exact multiplicity form | no set collapse |
| directory and RowCollection | global finalized build lifetime | exact extents/accounted |
| output borrowing build value | until downstream dependency ends | no early reset/destruction |

Exercise RequiredSlotSet combinations: key-only, residual-only, output-only, key+output,
residual+output, and unused. It may reduce payload only when row semantics and demanded
errors remain unchanged. Ordinary RowLayout incapability uses V24-G's exact alternate,
representability, and OOM classifications rather than creating SQL invalidity.

## V28-H — Build Sink, Combine, Finalize, and readiness

Drive `RD` through build acceptance, input exhaustion, worker-local completion, applicable
Combine/directory construction, Finalize, and probe-ready publication. A successful Sink
accepts the complete submission exactly once; failed acceptance is not success and is not
blindly replayed. Build exhaustion or one worker's completion is not probe readiness.

| State/event | Probe allowed? | Required observation |
|---|---:|---|
| partial accepted build | no | private mutable state |
| all source input exhausted | no, if Combine/Finalize remains | dependency not published |
| one local state complete | no | other states still required |
| all required Combine work | not until Finalize | every build tag exactly once |
| successful Finalize | yes | immutable complete state published once |
| Finalize failure/cancellation | no | no ready transition; cleanup |

After publication, concurrent probes observe complete stable directory/rows. Exact latch or
publication mechanics are not an oracle. Cleanup is tested separately and never substituted
for semantic Finalize.

## V28-I — Probe continuation

`PC` uses oracle states READY, PROBE_ROW, MATCH_CHAIN, OUTPUT_PENDING, RESOLVED, TERMINAL,
FAILED solely for trace comparison. Track the accepted submission, current probe tag, first
unvisited build tag, residual disposition, LEFT matched flag, offered output, backing-release
acknowledgment, and new-input readiness.

| Trace | Expected continuation/lifecycle |
|---|---|
| one probe, zero/one match | exact zero/one output then resolution |
| one probe, capacity+K matches | input accepted once; resume first unoffered chain member |
| many residual rejects | cursor advances with valid zero-output progress |
| output fills exactly at chain end | no restart or skipped next probe |
| direct probe backing retained | backing stable through last dependency |
| required probe state copied | caller backing may release before outputs resolve |
| early stop mid-chain | remainder undemanded; no new input; cleanup |
| error/cancellation mid-chain | current output invalid; FAILED never resumes |
| final nonempty output | handoff before terminal/no-output transition |

Poison consumed entries and place a sentinel at the expected resume point. A new input is
rejected until the preceding lifecycle, continuation, and output handoff permit admission.

## V28-J — LEFT matchedness

For one preserved probe tag, enumerate the following candidate traces independently of hash
layout. Set matched only on complete predicate TRUE. Null extension is delayed until all
demanded candidates are ruled out.

| Candidate trace | Matched? | Joined/null output | Error |
|---|---:|---|---|
| no bucket/candidate | no | 0 / 1 | none |
| same-hash unequal key | no | 0 / 1 after exhaustion | none |
| exact key, residual FALSE/UNKNOWN | no | 0 / 1 after exhaustion | none |
| exact key, residual error | no successful matchedness | 0 / 0 current lifecycle | ER owner |
| FALSE, UNKNOWN, then TRUE | yes only at TRUE | 1 / 0 | none |
| N residual TRUE | yes | N / 0 across chunks | none |
| NULL probe key | no | 0 / 1 | none |
| empty build | no | 0 / 1 per left occurrence | none |

Because supported LEFT hash orientation builds the nonpreserved logical right side, no
build-unmatched final source is invented. Null output uses declared right TypeIds/slots with
validity false and inaccessible payload.

## V28-K — Grace spill, recursive repartition, and skew fallback

`GP` assigns each tagged non-NULL key occurrence to exactly one build/probe partition using
the same pass function. Equal keys must remain partition-compatible; unequal keys may share
a partition and still require full equality. Compare in-memory and spilled runs to `JB/LE`,
not to each other alone.

| Spill case | Oracle requirement | Error/progress |
|---|---|---|
| one/many partitions | every input tag once; corresponding pairs meet | finite processing |
| duplicate keys/rows | all occurrences replayed | no loss/dedup |
| INNER NULL key | nonmatchable/no output | no poisoned payload read |
| LEFT NULL probe key | one unmatched output | may bypass partition |
| recursive repartition | additional bits change state | bounded depth |
| max-depth pathological skew | exact §28.11 fallback | accounted/no hard-limit bypass |
| write/read/temp capacity failure | no successful partial join | SpillIOError |
| malformed framing/CRC/range | no unsafe decode | SpillIOError |
| cancellation | no later success | cleanup/quiescence |

An ordinary hash-build OOM outside the explicit skew fallback propagates as OOM; it does not
silently select NestedLoopJoin. Spill files are fresh attempt-owned temporary resources and
never persistent/WAL/recovery state.

## V28-L — PhysicalMergeJoin capability execution

Use `MS` only after final-plan capability validation establishes an equality merge form,
compatible input order, predicate support, and any provided property. Negative plans use an
incompatible order, predicate, disabled capability, or range-style merge form and must be
rejected before execution.

For legal inputs, compare typed keys with `HE`, do not match ordinary NULLs, and process
duplicate groups of symbolic sizes M and N incrementally with exact `M*N` INNER multiplicity.
LEFT-capable execution emits each unmatched logical-left occurrence once. Vary group/chunk
boundaries and offer final output before terminal. No complete duplicate-group cross product
may be retained merely to fill one chunk. `OP`, not the operator name, decides advertised
ordering. The methods describe capability, never implementation chronology.

## V28-M — Algorithm substitutability

For every pair of capability-valid algorithms—NL/INLJ, NL/hash, NL/merge, INLJ/hash,
INLJ/merge, and hash/merge where applicable—derive expected output once from `JB/LE/HE/SS`.
Both-successful runs must match that independent oracle.

| Observable | Required equivalence |
|---|---|
| scalar values and NULL | exact |
| tagged bag/multiplicity and LEFT extension | exact |
| output schema/LogicalSlotIds | exact |
| required ordering | same OP-allowed class |
| demanded semantic candidates/error | same D20/D25/D21 owner result |
| transaction/result envelope | exact |
| physical sequence without property | may differ |
| memory/spill/resource feasibility | may differ |

Include an equality-plus-erroring-residual fixture. Add both source orders:
`erroring_residual AND left.key = right.key` and
`left.key = right.key AND erroring_residual`, with a NULL key on the side that
would otherwise be pruned by the hash key. The independent Chapter-17 demand
oracle decides whether the residual error is required before the equality
result can suppress it. A hash plan may omit a NULL-key pair only when that
oracle proves the omitted evaluation cannot change a demanded value, error, or
source-ordered precedence; otherwise the capability-valid plan must preserve
the pair demand through a non-pruning or otherwise exact implementation.
Hash pruning may omit other incompatible-hash pairs only because `HE` proves
they cannot satisfy equality and the same demand proof permits the omission.
Among demanded exact-key pairs, algorithm, bucket, chain, chunk, and worker
order cannot change V25-J/V21-13. No generic runtime algorithm fallback is
inferred.

## V28-N — Output schema, side identity, and RequiredSlotSet

`SS` constructs the declared physical output schema from logical left entries followed by
logical right entries, retaining TypeId, LogicalSlotId, nullability, and provenance. Compare
INNER with both legal hash orientations, LEFT, CROSS, self-join aliases, duplicate names,
equal-valued columns, and null extension.

| Case | Required result |
|---|---|
| INNER build-left/build-right | identical logical output schema |
| LEFT | left fields then nullable right fields |
| CROSS | left then right, full product |
| self-join/same RID | distinct side BindingIds/LogicalSlotIds |
| duplicate names/equal values | distinct declared output occurrences |
| null extension | same right IDs/TypeIds, validity false |

Build/probe ordinal, pointer, RID, key component, and display name are not output identity.
RequiredSlotSet retains key-, residual-, output-, and existence/cardinality-required state;
it cannot erase demanded errors. If a validated zero-column intermediate is supported by
the general schema/chunk contract, positive cardinality remains exact; otherwise that plan
shape is rejected before execution rather than improvised by the join.

## V28-O — Ordering properties

Use `OP` to validate plan claims independently of observed row order.

| Algorithm/perturbation | Permitted property result |
|---|---|
| HashJoin | none |
| hash seed/bucket/chain/insertion/spill order | may alter physical sequence, never add order |
| NestedLoop/IndexNL | only explicitly capability-guaranteed property |
| MergeJoin | only exact capability-proven property |
| duplicate-key ties | no hidden RID/pointer/build-ordinal tie rule |
| INNER build orientation | selected plan must retain required property |
| parallel worker order | unordered interleave or explicit order-preserving plan |

No test treats deterministic traversal as SQL order or assumes that “merge” alone proves a
specific output property.

## V28-P — Empty-side demand and downstream early stop

`DM` derives relational demand from Chapter 20 and V26-N. Child fixtures expose a row error,
storage error, and extra row only if fetched, making unnecessary execution observable.

| Parent/join state | Remaining demanded work | Output |
|---|---|---|
| INNER exact right/build empty | no opposite-side row fetch merely for EOS | empty |
| CROSS exact right empty | no opposite-side row fetch merely for EOS | empty |
| LEFT exact right/build empty | every preserved left row | one null-extended row each |
| LIMIT 0 above NL/INLJ/hash/merge | no relational join child admission after mandatory count acquisition | empty |
| positive Limit reached mid-chain | no remaining chain/later probe work | exact prefix |
| LEFT parent stop | no future matches/unmatched rows | exact prefix |

Binding, count acquisition, and independently required statement work remain demanded;
zero child rows does not promise zero runtime objects or allocations. Build is not
unconditionally executed beneath LIMIT zero. Early stop resolves accepted work without
QueryCancelled, suppresses later undemanded residual/child errors, and still performs cleanup.

## V28-Q — Expression errors, provenance, and output failure

Create demanded key/residual error candidates across build/probe rows, duplicate chains,
chunks, continuation, and workers. `ER` selects the ordinary V25-J minimum or transports
all required DML metadata to V21-13. Permute every physical order while retaining the same
demanded semantic candidate set.

| Origin/context | Selector/transport | Forbidden rank key |
|---|---|---|
| build/probe key expression, SELECT | V25-J | side row, hash, chunk, worker |
| residual candidate, SELECT | V25-J | bucket/chain/probe order |
| DML source join | V21-13; no V25-J pre-ranking | physical pair/algorithm |
| decomposition into hash key/residual | original source provenance retained | generated hash node |
| resource/cancel/spill/storage failure | canonical non-scalar owner | invented global precedence |

When a later demanded candidate fails after earlier output, the current failed invocation's
output is nonconsumable. Prior internal handoff remains completed but is not publication;
an already returned cursor prefix remains Chapter-31-owned and is not whole-query success.
No failed continuation resumes.

## V28-R — Memory, exact extents, large values, and cardinality

`RM` accounts directory/buckets, RowCollection fixed/varlen blocks, keys, payload, duplicate
links, continuation, output, partition directories/buffers, fallback state, and worker-local
dynamic storage. Instrument one conceptual owner per live region and verify complete release.

Use symbolic boundaries for bucket count, entry bytes, load-factor growth, capacity rounding,
key/value lengths, row extents, duplicate-chain length, partitions, and `M*N`. Every extent is
exact before allocation/addressing; wrap, narrowing, and pointer overflow are forbidden.

| Demand | Expected classification |
|---|---|
| ordinary exact representation fits | execute/account |
| ordinary RowLayout/key form incapable, exact alternate exists | use exact alternate |
| no supported exact runtime representation | representability `ExecutionError` |
| supported exact allocation denied/hard gate with no progress | OOM |
| spill encoding/I/O/addressability failure | SpillIOError |
| symbolic result/chain above fixed counter domain | stream with exact/checked state or avoid total |

No ordinary row/key/chunk/counter width is a SQL limit, and no finite `M*N` computation is a
semantic prerequisite. Large VARCHAR hashing/equality uses the complete exact value.

## V28-S — Cancellation, retry, invalid states, and persistence-negative registry

Inject cancellation at NL materialization/probe, INLJ lookup, hash build, pre/post-Finalize,
probe continuation, spill write/read, repartition/fallback, and merge duplicate groups.
Current output is invalid, no later same-runtime success occurs, and query-owned state
quiesces/cleans without independently releasing transaction-owned locks.

An authorized retry creates fresh materialized-right state, index cursor, hash directory,
rows/chains/handles, Finalize/readiness state, probe cursor/matched flag, partitions/depth,
merge cursor, and D25/D21 candidate state. The immutable plan may be reused.

| Invalid state | Safe classification/check |
|---|---|
| probe before successful Finalize | internal; reject before directory use |
| duplicate/missing build or probe acceptance | internal lifecycle violation; no duplicate/loss |
| invalid chain/index/merge cursor | internal; reject before dereference |
| wrong key TypeId/non-BOOLEAN residual/schema mismatch | static validation or internal runtime fault |
| stale build reference/directory-chain mismatch | internal, not persistent corruption |
| new input before readiness/output after terminal | internal protocol fault |
| failed output consumed/failed state reused | internal; no output/effect |
| child persistent corruption | preserve lower-layer category |

Hash tables, cached hashes, chains, build handles, cursors, flags, materialized right rows,
spill runtime metadata, repartition depth, fallback/merge/worker state, and readiness are
query/attempt-local. They never become page/catalog/WAL/recovery/TxnId/CommandId identity and
are not crash-recovered.

## V28-T — Representation, chunk, hash-seed, worker, and algorithm determinism

Run equivalent successful fixtures under capacities 1, a small non-power-of-two, 1024, and
65535 where symbolic/feasible; vary build/probe chunk boundaries, legal empty progress,
FLAT/CONSTANT/DICTIONARY selections, hash seed, bucket/chain layout, INNER orientation,
worker completion, Combine order, pointers, runtime row ordinals, and eligible algorithms.

| Perturbation | Invariant | Permitted difference |
|---|---|---|
| chunking/vector representation | values, NULL, tagged bag, LEFT extension, errors | allocation/work |
| seed/bucket/chain order | same semantic result; HashJoin no property | physical sequence |
| build orientation | logical schema/bag/error/property class | resource feasibility |
| worker/Combine order | complete build once, readiness barrier, D25/D21 result | physical timing/order without property |
| pointer/runtime ordinal | every semantic observable | addresses/IDs |
| algorithm choice | AS both-successful equivalence | memory/spill/work/unordered sequence |

Partitioned parallel probes cover each accepted probe occurrence exactly once. One worker's
completion cannot establish global readiness or completion. No schedule is tested with sleeps.

## V28-U — Cross-chapter reuse map

| Handoff | Canonical contract | Reused Verification / independent oracle |
|---|---|---|
| Ch17→28 | equality, NULL, FLOAT64, VARCHAR, hash compatibility | V17 equality/hash; HE |
| Ch19→28 | side identity, BindingId, provenance | binding tests; SS/ER |
| Ch20→28 | INNER/CROSS/LEFT bags, schema, demand | V20-7/16; JB/LE/DM |
| Ch21→28 | V21-13/14 and retry envelope | V21-13/14; ER/AF |
| Ch22→28 | operators, capability, schema, immutable plan | V22-B/D/K; SS/IX |
| Ch23→28 | active occurrences, vectors, borrowing | V23-A–G; DO/BG |
| Ch24→28 | RowCollection, exact extents, accounting, spill | V24; RM/GP |
| Ch25→28 | expressions, V25-J, failed output | V25-J/K/O; ER |
| Ch26→28 | Sink, continuation, Finalize, early stop | V26-C/E/G/N; RD/PC/DM |
| Ch27→28 | scans, index candidates, child completion | V27-D–G/P; IC |
| Ch28→31 | internal output versus cursor publication | result-interface tests; ER/AF |
| Ch28→32 | parallel build/probe and dependency barrier | parallel tests; RD/AF |
| Ch28→36 | INLJ/access applicability | access-path tests; IC |
| Ch28→37 | OrderingProperty/RequiredSlotSet | join-order tests; OP/SS |
| Ch28→38 | capability/final-plan validation | optimizer validation; MS/IX |
| Ch28→39 | resource/cancel/internal/transaction outcomes | error tests; RM/ER/AF |

## V28-V — Atomic architecture-obligation ledger

| ID | Architecture | Atomic obligation | Verification | Oracle | Reuse |
|---|---|---|---|---|---|
| V28-A01 | 28.1 | The four physical join algorithms are represented exactly. | V28-A | SS | V22-D |
| V28-A02 | 28.1; 28.3–28.12 | Only capability-valid algorithm/type combinations execute. | V28-A/L | IX/MS | V22-K |
| V28-A03 | 28.3 | NestedLoop supports INNER, LEFT, and CROSS. | V28-A/E | JB | V20-7 |
| V28-A04 | 28.4 | IndexNL executes applicable indexed INNER/LEFT semantics. | V28-A/F | IC | V27-E/F |
| V28-A05 | 28.5 | HashJoin supports INNER and LEFT. | V28-A | JB | V20-7 |
| V28-A06 | 28.12 | MergeJoin is capability-conditional equality execution. | V28-A/L | MS | V22-D/K |
| V28-A07 | 28.1 | Algorithm selection cannot reinterpret logical semantics. | V28-A/M | AS | V22-D |
| V28-A08 | 28.5 | INNER may use either legal build orientation. | V28-A/N | SS | Ch37 tests |
| V28-A09 | 28.5 | LEFT fixes logical right build and logical left preserved probe. | V28-A/J | LE | Ch37 tests |
| V28-A10 | 28.1–28.12 | Runtime state is execution-local and plan configuration immutable. | V28-A/S | AF | V22-B |
| V28-A11 | 28.1–28.12 | Pipeline roles and phases remain distinct. | V28-A/H/I | RD/PC | V26-B/G |
| V28-A12 | 28.1–28.12 | Unsupported RIGHT/FULL/SEMI/ANTI forms are not admitted. | V28-A/L | IX | V22-K |
| V28-B01 | 28.1; Ch20 | INNER emits every complete-predicate TRUE pair once. | V28-B | JB | V20-7 |
| V28-B02 | 28.1; Ch20 | INNER FALSE pairs emit none. | V28-B | JB | V20-7 |
| V28-B03 | 28.1; Ch20 | INNER UNKNOWN pairs emit none. | V28-B | JB | V20-7 |
| V28-B04 | 28.3 | CROSS emits the exact Cartesian occurrence product. | V28-B/E | JB | V20-7 |
| V28-B05 | 28.3 | Empty side makes INNER/CROSS result empty. | V28-B/P | JB/DM | V20-16 |
| V28-B06 | 28.3–28.9 | Duplicate left occurrences remain distinct. | V28-B | DO | V23-C |
| V28-B07 | 28.3–28.9 | Duplicate right/build occurrences remain distinct. | V28-B/D | DO | V23-C |
| V28-B08 | 28.3–28.12 | All-TRUE M-by-N multiplicity is mathematical M*N. | V28-B | JB | V20-7 |
| V28-B09 | 28.3–28.12 | Product cardinality needs no finite-width prerequisite. | V28-B/R | JB/RM | V24 exactness |
| V28-B10 | 28.3–28.12 | CONSTANT payload sharing does not collapse occurrences. | V28-B/T | DO | V23-B |
| V28-B11 | 28.3–28.12 | DICTIONARY repetition does not collapse occurrences. | V28-B/T | DO | V23-C/D |
| V28-B12 | 28.3–28.12 | Active cardinality, not capacity, defines candidate occurrences. | V28-B/T | DO | V23-A |
| V28-C01 | 28.2 | Equal non-NULL values are hash lookup-compatible. | V28-C | HE | V17 hash |
| V28-C02 | 28.2 | Hash values remain query/process-local and nonpersistent. | V28-C/S | AF | V22-B |
| V28-C03 | 28.2 | Exact hash mix/seed is implementation freedom. | V28-C/T | HE | V17 hash |
| V28-C04 | 28.2.1 | Ordinary NULL key components are nonmatchable. | V28-C | HE | V17 3VL |
| V28-C05 | 28.2.1 | NULL validity prevents poisoned payload reads. | V28-C | HE/IX | V23-E |
| V28-C06 | 28.2.1 | Composite key with any NULL is nonmatchable. | V28-C | HE | V17 hash |
| V28-C07 | 28.2.1 | FLOAT64 signed zeros compare/hash compatibly. | V28-C | HE | V17 hash |
| V28-C08 | 28.2.1 | Canonical NaNs compare/hash under the frozen equality. | V28-C | HE | V17 hash |
| V28-C09 | 28.2.1 | VARCHAR hashing/equality uses exact bytes and length. | V28-C | HE | V17 value tests |
| V28-C10 | 28.2.1 | Embedded NUL/high bytes have no C-string semantics. | V28-C | HE | V17 value tests |
| V28-C11 | 28.2.1 | Composite component order/boundaries are exact. | V28-C | HE | V17 hash |
| V28-C12 | 28.2.1 | Grouping NULL mode never leaks into ordinary join equality. | V28-C | HE | V17 hash |
| V28-D01 | 28.6 | Hash mismatch continues directory probing as required. | V28-D | HC | — |
| V28-D02 | 28.6 | Same hash is only a candidate, never equality. | V28-D | HC | V17 hash |
| V28-D03 | 28.6; 28.13(4) | Every same-hash candidate receives full key equality. | V28-D | HC/HE | V17 hash |
| V28-D04 | 28.6 | Same-hash unequal keys remain distinct directory entries. | V28-D | HC | — |
| V28-D05 | 28.6 | Equal keys share a duplicate chain without lost occurrences. | V28-D | HC/DO | V23-C |
| V28-D06 | 28.6 | New duplicate occurrence appends/preserves exact multiplicity. | V28-D | DO | V20-7 |
| V28-D07 | 28.6 | Resize preserves every directory entry and chain. | V28-D | HC | V24 exactness |
| V28-D08 | 28.6 | Directory target is tuning, not SQL/persistence. | V28-D/R | RM | V24 |
| V28-D09 | 28.6 | Build-row handle is runtime occurrence identity only. | V28-D/S | AF | V22-B |
| V28-D10 | 28.6 | Hash/chain physical order creates no SQL ordering. | V28-D/O | OP | Ch37 tests |
| V28-E01 | 28.3 | NestedLoop materializes logical right once. | V28-E | NC | V24 row tests |
| V28-E02 | 28.3 | Materialization removes any implicit source-rewind assumption. | V28-E | NC | V26-C |
| V28-E03 | 28.3 | Materialized values remain stable through all probes. | V28-E/G | BG | V23-G |
| V28-E04 | 28.3 | Materialized VARCHAR bytes are exact and owned. | V28-E/G | BG | V24 row tests |
| V28-E05 | 28.3 | Every demanded left occurrence scans the right occurrence domain. | V28-E | NC/JB | V20-7 |
| V28-E06 | 28.3 | NestedLoop continuation crosses output capacity exactly. | V28-E/I | NC/PC | V26-C |
| V28-E07 | 28.3 | Empty right yields empty INNER/CROSS. | V28-E/P | JB/DM | V20-16 |
| V28-E08 | 28.3 | Empty right yields one null-extended row per LEFT occurrence. | V28-E/P | LE/DM | V20-7 |
| V28-E09 | 28.3 | Empty right may suppress undemanded INNER/CROSS left-row fetches. | V28-E/P | DM | V20-16/V26-N |
| V28-E10 | 28.3 | NestedLoop remains a costed durable alternative. | V28-A/M | AS | optimizer tests |
| V28-F01 | 28.4 | IndexNL evaluates each demanded outer key under expression owners. | V28-F/Q | IC/ER | V25-B |
| V28-F02 | 28.4 | Index range/point lookup uses planner-valid bounds. | V28-F | IC | V27-E |
| V28-F03 | 28.4 | Every index hit remains a heap candidate. | V28-F | IC | V27-F |
| V28-F04 | 28.4; 28.13(1) | Inner heap references receive safe validation. | V28-F | IC | V27-F |
| V28-F05 | 28.4 | Inner candidates receive MVCC recheck. | V28-F | IC | V27-D/F |
| V28-F06 | 28.4 | Residual TRUE alone qualifies an otherwise valid candidate. | V28-F | IC | V25-B |
| V28-F07 | 28.4 | Residual FALSE/UNKNOWN does not qualify. | V28-F | IC | V17 3VL |
| V28-F08 | 28.4 | LEFT null extension follows complete range exhaustion. | V28-F/J | LE/IC | V20-7 |
| V28-F09 | 28.4 | One outer occurrence may span multiple output chunks. | V28-F/I | IC/PC | V26-C |
| V28-F10 | 28.4 | Outer/index continuation loses or duplicates no candidate. | V28-F/I | IC/PC | V27-G |
| V28-G01 | 28.6–28.7 | Build RowCollection owns retained rows through probe. | V28-G | BG | V24 row tests |
| V28-G02 | 28.6–28.7 | Build variable values are deep/stably owned. | V28-G | BG | V23-G |
| V28-G03 | 28.6 | Output-required build payload cannot be pruned. | V28-G/N | SS | Ch37 tests |
| V28-G04 | 28.6 | Residual-required build payload cannot be pruned. | V28-G/N | SS | Ch37 tests |
| V28-G05 | 28.6 | Key-required build values cannot be pruned. | V28-G/N | SS | Ch37 tests |
| V28-G06 | 28.6 | Output borrowing build values extends build lifetime. | V28-G | BG | V23-G/V26-M |
| V28-G07 | 28.6 | All growing build storage is memory-accounted. | V28-G/R | RM | V24 memory |
| V28-G08 | 28.6 | Ordinary RowLayout incapability is not SQL invalidity. | V28-G/R | RM | V24-G |
| V28-G09 | 28.6 | Exact compressed duplicate storage must reconstruct multiplicity. | V28-G/D | DO | V20-7 |
| V28-G10 | 28.6 | Build storage release waits for all direct/downstream dependencies. | V28-G | BG | V26-M |
| V28-H01 | 28.7 | Successful build Sink accepts each submission completely once. | V28-H | RD | V26-E |
| V28-H02 | 28.7 | Failed Sink acceptance is not success or blind replay. | V28-H | RD/AF | V26-E/L |
| V28-H03 | 28.7 | Build key expressions run for each demanded build occurrence. | V28-H/Q | ER | V25-B |
| V28-H04 | 28.7 | Nonmatchable NULL build keys may be excluded only in nonpreserved shapes. | V28-H/J | HE/LE | V20-7 |
| V28-H05 | 28.7 | Build input exhaustion alone is not probe readiness. | V28-H | RD | V26-G |
| V28-H06 | 28.7 | Combine includes every local build occurrence once where used. | V28-H/T | RD/DO | parallel tests |
| V28-H07 | 28.7 | Successful Finalize precedes probe-ready publication. | V28-H | RD | V26-G |
| V28-H08 | 28.7 | Finalized probe state is complete and observationally immutable. | V28-H | RD/BG | parallel tests |
| V28-H09 | 28.7 | Finalize failure prevents successful probe/query readiness. | V28-H/S | RD/IX | V26-G/K |
| V28-H10 | 28.7 | Semantic Finalize is distinct from cleanup. | V28-H/S | RD/AF | V26-G |
| V28-I01 | 28.8 | One probe submission is accepted at most once. | V28-I | PC | V26-C |
| V28-I02 | 28.8 | At most one accepted probe lifecycle is unresolved per local state. | V28-I | PC | V26-C |
| V28-I03 | 28.8; 28.13(8) | One probe occurrence may yield arbitrarily many chunks. | V28-I | PC | V26-C |
| V28-I04 | 28.8 | Continuation resumes at the first unoffered build candidate. | V28-I | PC | V26-C |
| V28-I05 | 28.8 | Continuation neither restarts nor skips a duplicate chain. | V28-I | PC/DO | V26-C |
| V28-I06 | 28.8 | Directly referenced probe backing remains stable. | V28-I/G | BG/PC | V23-G/V26-M |
| V28-I07 | 28.8 | Independently copied probe state permits earlier caller release. | V28-I | PC/BG | V26-C |
| V28-I08 | 28.8 | Empty output advances finite candidate state. | V28-I | PC | V26-C |
| V28-I09 | 28.8 | New probe input waits for readiness and output handoff. | V28-I | PC | V26-C |
| V28-I10 | 28.8 | Final output precedes terminal/no-output completion. | V28-I | PC | V26-A |
| V28-J01 | 28.8–28.9 | LEFT matchedness is per logical preserved probe occurrence. | V28-J | LE/PC | V20-7 |
| V28-J02 | 28.9 | No candidate yields exactly one null extension. | V28-J | LE | V20-7 |
| V28-J03 | 28.9 | Same-hash unequal key does not mark matched. | V28-J | HC/LE | V17 hash |
| V28-J04 | 28.8–28.9 | Residual FALSE does not mark matched. | V28-J | LE | V17 3VL |
| V28-J05 | 28.8–28.9 | Residual UNKNOWN does not mark matched. | V28-J | LE | V17 3VL |
| V28-J06 | 28.8–28.9 | Residual error is not converted into unmatched output. | V28-J/Q | LE/ER | V25-J |
| V28-J07 | 28.9 | Complete predicate TRUE marks matched and emits once. | V28-J | LE | V20-7 |
| V28-J08 | 28.9 | N TRUE candidates emit N rows and no null extension. | V28-J | LE/DO | V20-7 |
| V28-J09 | 28.9 | NULL probe key emits one right-null-extended row. | V28-J | HE/LE | V17 3VL |
| V28-J10 | 28.9 | Null extension waits until all demanded candidates are ruled out. | V28-J | LE/PC | V20-7 |
| V28-K01 | 28.10 | Build and probe use one compatible partition function per pass. | V28-K | GP/HE | V24 spill |
| V28-K02 | 28.10 | Every ordinary key-bearing input occurrence enters exactly one partition. | V28-K | GP/DO | V24 spill |
| V28-K03 | 28.10 | Equal keys cannot be separated into incompatible partition pairs. | V28-K | GP/HE | V17 hash |
| V28-K04 | 28.10 | Spill replay preserves duplicate occurrence multiplicity. | V28-K | GP/DO | V24 spill |
| V28-K05 | 28.10 | LEFT NULL probe keys retain exact unmatched semantics. | V28-K/J | GP/LE | V20-7 |
| V28-K06 | 28.10 | Spilled and in-memory success match independent join oracle. | V28-K/M | JB/LE | V24 spill |
| V28-K07 | 28.10 | Spill failures retain SpillIOError and no partial success. | V28-K/Q | GP/RM | V24 spill |
| V28-K08 | 28.10 | Hash partition processing creates no ordering property. | V28-K/O | OP | Ch37 tests |
| V28-K09 | 28.11 | Recursive repartition advances with additional hash bits. | V28-K | GP | V24 progress |
| V28-K10 | 28.11; 28.13(13) | Repartition depth is bounded. | V28-K | GP | V24 progress |
| V28-K11 | 28.11 | Pathological max-depth partition uses exact accounted fallback. | V28-K/R | GP/RM | V24 memory |
| V28-K12 | 28.11 | Ordinary OOM does not authorize generic algorithm fallback. | V28-K/M | IX/RM | §39 tests |
| V28-L01 | 28.12 | MergeJoin requires capability-valid equality form. | V28-L | MS/IX | V22-K |
| V28-L02 | 28.12 | Compatible ordered inputs/properties are validated. | V28-L/O | MS/OP | Ch37 tests |
| V28-L03 | 28.12 | Ordinary NULL merge keys never match. | V28-L | MS/HE | V17 3VL |
| V28-L04 | 28.12 | Duplicate groups preserve exact mathematical multiplicity. | V28-L | MS/JB | V20-7 |
| V28-L05 | 28.12; 28.13(14) | Duplicate products stream incrementally. | V28-L/R | MS/RM | V26-C |
| V28-L06 | 28.12 | Valid LEFT merge semantics emit unmatched left once. | V28-L | MS/LE | V20-7 |
| V28-L07 | 28.12 | Range-style merge forms are rejected as outside the defined architecture. | V28-L | IX | V22-K |
| V28-L08 | 28.12 | Merge name alone creates no ordering claim. | V28-L/O | OP | Ch37 tests |
| V28-M01 | 28.1 | Every eligible algorithm preserves logical values and NULL. | V28-M | AS/JB | V20-7 |
| V28-M02 | 28.1 | Every eligible algorithm preserves tagged bag multiplicity. | V28-M | AS/DO | V20-7 |
| V28-M03 | 28.1 | Every eligible LEFT algorithm preserves null extension. | V28-M | AS/LE | V20-7 |
| V28-M04 | 28.1 | Algorithm choice preserves schema and LogicalSlotIds. | V28-M/N | AS/SS | V22-B |
| V28-M05 | 28.1 | Algorithm choice preserves required ordering class. | V28-M/O | AS/OP | Ch37 tests |
| V28-M06 | 28.1 | Algorithm choice preserves demanded V25-J result. | V28-M/Q | AS/ER | V25-J |
| V28-M07 | 28.1 | Algorithm choice preserves V21-13 candidate result. | V28-M/Q | AS/ER | V21-13 |
| V28-M08 | 28.1 | Algorithm choice preserves transaction/result semantics. | V28-M/Q | AS/AF | V21-14 |
| V28-M09 | 28.1 | Resource feasibility may differ among physical algorithms. | V28-M/R | AS/RM | V24 memory |
| V28-M10 | 28.1 | Unordered physical output sequence may differ. | V28-M/O | AS/OP | Ch37 tests |
| V28-N01 | 28.1; 28.5 | Logical left/right remain distinct from build/probe. | V28-N | SS | V20-7 |
| V28-N02 | 28.5 | INNER build orientation cannot change output column sequence. | V28-N | SS | V22-B |
| V28-N03 | 28.5 | LEFT output uses logical left then logical right slots. | V28-N | SS | V20-7 |
| V28-N04 | 28.3–28.12 | Every output TypeId/LogicalSlotId matches physical schema. | V28-N | SS | V22-B |
| V28-N05 | 28.3–28.12 | Duplicate names/equal values retain distinct identities. | V28-N | SS | binding tests |
| V28-N06 | 28.3–28.12 | Self-join sides remain distinct even for same RID/value. | V28-N | SS | Ch37 relation tests |
| V28-N07 | 28.9 | Null extension changes validity/nullability, not slot identity. | V28-N/J | SS/LE | V20-7 |
| V28-N08 | 28.3–28.12 | Runtime names/build ordinals/pointers never resolve slots. | V28-N/S | SS/IX | V22-A |
| V28-N09 | 28.6 | RequiredSlotSet retains every key/residual/output need. | V28-N/G | SS | Ch37 tests |
| V28-N10 | 28.6 | Slot pruning cannot erase demanded expression/error work. | V28-N/P/Q | SS/DM/ER | V20-16 |
| V28-O01 | 28.10; 28.13(12) | PhysicalHashJoin advertises no ordering. | V28-O | OP | Ch37 tests |
| V28-O02 | 28.6 | Hash seed/bucket/chain order is nonsemantic. | V28-O/T | OP | Ch37 tests |
| V28-O03 | 28.3 | NestedLoop advertises only an explicit runtime guarantee. | V28-O | OP | Ch37 tests |
| V28-O04 | 28.4 | IndexNL advertises only an explicit runtime guarantee. | V28-O | OP | Ch37 tests |
| V28-O05 | 28.12 | MergeJoin advertises only a capability-proven property. | V28-O/L | OP/MS | Ch37 tests |
| V28-O06 | 28.6–28.12 | Duplicate ties create no hidden RID/pointer order. | V28-O | OP | Ch37 tests |
| V28-O07 | 28.5 | Build orientation must retain the selected required property. | V28-O/N | OP/SS | Ch37 tests |
| V28-O08 | 28.7–28.10 | Worker/partition order cannot violate required order. | V28-O/T | OP | parallel tests |
| V28-P01 | 28.3; Ch20 | Exact empty right/build makes INNER output empty. | V28-P | DM/JB | V20-16 |
| V28-P02 | 28.3; Ch20 | Exact empty right makes CROSS output empty. | V28-P | DM/JB | V20-16 |
| V28-P03 | 28.3; 28.9 | Empty right/build leaves preserved LEFT rows demanded. | V28-P | DM/LE | V20-7 |
| V28-P04 | 28.3; Ch20/26 | INNER/CROSS need not fetch opposite rows after exact emptiness. | V28-P | DM | V20-16/V26-N |
| V28-P05 | 28.3; Ch20/26 | Undemanded opposite-side row errors do not surface. | V28-P/Q | DM/ER | V20-16/V26-N |
| V28-P06 | 28.3–28.12; Ch27 | LIMIT zero creates no relational join child demand. | V28-P | DM | V27-K |
| V28-P07 | 28.3–28.12; Ch27 | LIMIT zero retains count/binding/independent statement work. | V28-P | DM | V27-K |
| V28-P08 | 28.8; Ch26 | Positive Limit may stop a probe match chain at exact prefix. | V28-P/I | DM/PC | V26-N |
| V28-P09 | 28.8; Ch26 | Later probe/residual work is undemanded after valid stop. | V28-P/Q | DM/ER | V26-N |
| V28-P10 | 28.3–28.12; Ch26 | Early stop still requires safe output handoff and cleanup. | V28-P/S | DM/AF | V26-N |
| V28-Q01 | 28.4–28.8 | Key expression errors retain canonical provenance. | V28-Q | ER | V25-J/K |
| V28-Q02 | 28.4; 28.8 | Residual errors retain canonical provenance. | V28-Q | ER | V25-J/K |
| V28-Q03 | 28.4–28.12 | V25-J selects ordinary demanded join-pair candidates. | V28-Q | ER | V25-J |
| V28-Q04 | 28.4–28.12 | V21-13 receives un-pre-ranked DML candidates. | V28-Q | ER | V21-13/V25-K |
| V28-Q05 | 28.4–28.12 | Build/probe/bucket/chain/chunk/worker order never ranks errors. | V28-Q/T | ER | V25-J/K |
| V28-Q06 | 28.8 | Hash-key/residual decomposition preserves source origin. | V28-Q | ER | V20-16 |
| V28-Q07 | 28.8 | Hash pruning removes only semantically impossible unequal-key pairs. | V28-Q/M | HE/ER | V20-16 |
| V28-Q08 | 28.3–28.12 | Resource/cancel/spill/storage classes retain their owners. | V28-Q/R/S | ER/RM | V24/§39 |
| V28-Q09 | 28.3–28.12 | Failed current output is nonconsumable. | V28-Q | ER | V25-O |
| V28-Q10 | 28.3–28.12 | Prior internal handoff/cursor prefix remain distinct from success. | V28-Q | ER/AF | V26-J/Ch31 |
| V28-R01 | 28.6–28.11 | Every growing join-owned memory region is accounted. | V28-R | RM | V24 memory |
| V28-R02 | 28.6 | Directory capacity/load arithmetic is exact before use. | V28-R | RM | V24 exactness |
| V28-R03 | 28.6–28.8 | Row/key/payload/continuation extents never wrap/narrow. | V28-R | RM | V24 exactness |
| V28-R04 | 28.2; 28.6 | Large VARCHAR key uses its complete exact value. | V28-R/C | RM/HE | V23 string |
| V28-R05 | 28.6 | Oversized retained build row uses exact alternate or canonical failure. | V28-R/G | RM | V24-G |
| V28-R06 | 28.3–28.12 | Joined output row has no ordinary-layout SQL limit. | V28-R | RM | V24-G |
| V28-R07 | 28.3–28.12 | Result cardinality has no fixed-counter SQL limit. | V28-R | RM/JB | V24 exactness |
| V28-R08 | 28.6; 28.8 | Duplicate-chain length is not chunk-width limited. | V28-R/I | RM/PC | V26-C |
| V28-R09 | 28.6–28.11 | Exact supported allocation denial reports OOM. | V28-R | RM | V24 errors |
| V28-R10 | 28.6–28.11 | Unsupported exact representation reports representability ExecutionError. | V28-R | RM | V24 errors |
| V28-S01 | 28.3–28.12 | Cancellation invalidates current output and prevents later success. | V28-S | AF/ER | V26-K |
| V28-S02 | 28.3–28.12 | Cancellation cleans/quiesces every join phase. | V28-S | AF/BG | V24 cleanup |
| V28-S03 | 28.3–28.12 | Retry creates fresh algorithm/build/probe/spill state. | V28-S | AF | V26-L |
| V28-S04 | 28.3–28.12 | Immutable plan may be reused without mutable state leakage. | V28-S | AF | V22-B |
| V28-S05 | 28.7 | Probe before readiness is safely rejected. | V28-S/H | IX/RD | V26-P |
| V28-S06 | 28.3–28.8 | Duplicate/missing acceptance is an internal protocol failure. | V28-S | IX/PC | V26-P |
| V28-S07 | 28.4–28.12 | Invalid cursor/type/residual/schema is rejected before unsafe use. | V28-S | IX | V22-K/V25-N |
| V28-S08 | 28.6–28.8 | Stale build references/directory mismatch are internal runtime faults. | V28-S | IX | V23-M |
| V28-S09 | 28.3–28.12 | Output after terminal/failed output consumption is rejected. | V28-S | IX/ER | V26-P |
| V28-S10 | 28.2–28.12 | Runtime join state has no persistent/recovery identity. | V28-S | AF | V22-B |
| V28-T01 | 28.3–28.12 | Legal chunk capacities preserve semantic results. | V28-T | AS | V23-L |
| V28-T02 | 28.3–28.12 | Build/probe chunk boundaries preserve semantic results. | V28-T | AS/JB | V26-Q |
| V28-T03 | 28.3–28.12 | Empty progress insertion preserves result and advances state. | V28-T/I | PC | V26-D/Q |
| V28-T04 | 28.3–28.12 | FLAT/CONSTANT/DICTIONARY forms preserve occurrences. | V28-T | DO | V23-C/D |
| V28-T05 | 28.2; 28.6 | Hash seed/directory shape preserves semantic result. | V28-T/C/D | HE/HC | V17 hash |
| V28-T06 | 28.6 | Duplicate-chain order preserves bag/error owner. | V28-T/Q | DO/ER | V25-J/K |
| V28-T07 | 28.5 | Legal INNER orientation preserves schema/bag/property/error. | V28-T/M/N | AS/SS | Ch37 tests |
| V28-T08 | 28.7; Ch32 | Worker/Combine order preserves complete build and readiness. | V28-T/H | RD/DO | parallel tests |
| V28-T09 | 28.3–28.12 | Pointer/runtime ordinals have no semantic role. | V28-T/S | AF | V25-P |
| V28-T10 | 28.1 | Eligible algorithm choice preserves both-successful semantics. | V28-T/M | AS | V22-D |


### V28 negative-case and oracle checks

Search Chapter-28-relevant Verification and reject these stale rules: chronology-bound join
roles or MergeJoin sequencing; same hash means match; ordinary NULL equals NULL; host FLOAT
equality; C-string VARCHAR; duplicate overwrite; one probe input produces one chunk;
continuation resubmits input; empty output is EOS; bucket/key hit marks LEFT matched before
residual TRUE; premature or extra null extension; INNER/CROSS always scan the other side
after exact emptiness; hash build always runs beneath LIMIT zero; output schema follows
build/probe order; HashJoin/probe/chain order supplies SQL ordering; MergeJoin name alone
supplies order; `M*N` overflow is SQL error; ordinary RowLayout/chunk width is a join limit;
and first build/probe/bucket/chain/worker error wins. The negative fixtures above make every
form nonconforming. The compact `Hash Join Tests` case list remains a smoke-test index;
V28-A–V supplies the normative deterministic methodology and independent oracles.

Exercise permitted hash mixes, directory/container mechanics, chain/cursor
representations, ownership mechanisms, exact retained-row alternatives, batching, spill
buffering, and worker organization. Use deterministic controls and independent expected
results rather than sleeps or production self-oracles.
