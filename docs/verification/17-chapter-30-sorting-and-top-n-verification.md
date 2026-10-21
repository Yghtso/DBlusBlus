# Chapter 30 — Sorting and Top-N Verification

This family verifies `ARCHITECTURE.md` Chapter 30 and its immediate cross-owner contracts.
Verify each obligation through a deterministic procedure and independent oracle,
directly or through the exact V30-N reuse row.

Use these independent test-side oracles:

| Oracle | Independent construction |
|---|---|
| `CO` | Complete lexicographic comparator from resolved keys, direction, NULL placement, and Chapter-17 scalar order |
| `BO` | Tagged logical-occurrence multiset independent of payload or storage identity |
| `TO` | Complete-comparator equivalence classes and all Chapter-20-permitted LIMIT/OFFSET outcomes |
| `PO` | Prefix implication checker: every prefix-decided strict relation must equal `CO` |
| `SO` | Declared schema, LogicalSlotId, hidden-key, and RequiredSlotSet model |
| `PL` | Sink/Finalize/readiness/Source state automaton |
| `MO` | Reservation, spill-transition, release, and cleanup ledger |
| `RO` | Run membership, sorted-run, merge, and well-founded progress model |
| `TF` | Temporary framing, extent, descriptor-compatibility, checksum, and ownership model |
| `KO` | Arbitrary-precision `LIMIT + OFFSET` and implementation-applicability model |
| `HO` | Tagged-occurrence top-K heap and final retained-sort model |
| `ER` | Semantic demand, expression provenance, error category, and result-envelope model |
| `AF` | Attempt-generation ledger with poisoned failed state and fresh retry state |
| `OP` | Exact Chapter-37 ordering-vector and prefix-satisfaction model, with no stability field |

Production comparators, sort providers, prefix encoders, heap implementations, spill codecs,
and index order are observations, never their own oracle. Use exact typed values, occurrence
tags, deterministic barriers and fault points, synthetic descriptors, forced memory budgets,
and recorded random seeds. Use no sleeps, pointer order, uncontrolled scheduler timing, or
production-provider differential equality as a sole oracle.

## V30-A — Complete comparator

Construct literal one-key and multi-key descriptors over INT32, INT64, DATE, TIMESTAMP,
VARCHAR, and FLOAT64. For each, independently compose ASC/DESC and resolved NULLS
FIRST/LAST, then compare every pair and selected triples against `CO`. Verify sign
antisymmetry, transitivity, deterministic equivalence classes, and lexicographic evaluation:
a later key participates only when every earlier key compares equal. Mixed directions,
mixed NULL policies, repeated values, and composite prefixes are mandatory.

The FLOAT matrix includes negative infinity, finite negatives, both signed zeros, finite
positives, positive infinity, and several NaN encodings. Verify the exact Chapter-17 NaN
position, signed-zero equality, NaN-class equality, and DESC reversal. The VARCHAR matrix
uses empty strings, embedded NUL, high bytes, prefix strings, unequal lengths, equal bytes,
and long common prefixes under exact binary collation. Reject host NaN operators, locale or
C-string comparison, raw struct bytes, pointer order, and any comparator disagreement among
in-memory sort, run generation, merge, Top-N membership, retained Top-N sorting, and the
provided ordering property.

## V30-B — Bag preservation and tie-outcome family

Tag every child row with an oracle-only occurrence ID. Full PhysicalSort must emit exactly
the same occurrence multiset: no loss, fabrication, collapse, or deduplication, including
identical rows, repeated vector payload references, and equal keys with different payloads.
Only relative order inside a complete-`CO` equivalence class may vary.

`TO` partitions rows by complete-comparator equality and enumerates legal ordered sequences
and LIMIT/OFFSET slices. For `(A,5)`, `(B,5)`, `(C,5)` under `ORDER BY key LIMIT 1`, `{A}`,
`{B}`, and `{C}` are legal. Compare PhysicalSort + PhysicalLimit, eligible PhysicalTopN,
external and parallel sort, and an exact index/order provider + PhysicalLimit by membership
in this family. Require exact cardinality, actual child occurrences only, all strict
comparator relations, resolved NULL order, demanded-error behavior, schema, slots, and
transaction/result semantics. Do not require identical boundary-tied members or tie order.

Perturb RID, input position, page, pointer, payload, run, worker, heap-insertion, and stable
sort order. None may become a required SQL-visible tiebreaker or OrderingProperty component.
When signed-zero or NaN-variant keys compare equal, retain original projected payload bits;
sorting does not canonicalize payload representation.

## V30-C — Normalized-prefix soundness

Instrument every prefix comparison. For each strict prefix result, `PO` independently proves
the same strict `CO` result. Prefix equality or insufficient information must invoke the
complete comparator; it cannot establish full-key equality. Force prefix collisions and
verify they only increase complete compares.
Never treat prefix bytes as hashes or semantic row identity.

Cover NULL markers, ASC/DESC, INT32, INT64, DATE, TIMESTAMP, FLOAT64, VARCHAR, and composite
keys. FLOAT prefixes preserve signed-zero and equivalent-NaN comparator classes and order
infinities correctly. VARCHAR fixtures include `"a"`/`"aa"`, `"a"`/`"a\0"`,
`"a\0"`/`"a\0b"`, termination at the prefix boundary, long common prefixes, embedded NUL,
and high-byte differences. Either a sound strict decision or an inconclusive result followed
by `CO` is legal; a truncation-based inversion is not.

Force zero prefix bytes for every admitted key family. Results, properties, errors, and
occurrences remain exact; only performance may change. Vary prefix widths to prove no fixed
width, including 8–16 bytes, is semantic. If persistent index-key encoding is reused, inject
direction, NULL, FLOAT, VARCHAR-truncation, and composite-boundary differences and require an
independent equivalence proof. Runtime prefixes acquire no persistent, WAL, or index-format
identity.

## V30-D — Sort-key demand, slots, payload, and lifetime

Use the Chapter-20/25 demand oracle to prove every demanded sort-key expression is evaluated.
Place a later error after an earlier key already decides comparison, after a Top-N heap is
full, behind a currently worse candidate, and beneath a small LIMIT. Physical convenience
cannot suppress the error; speculative undemanded evaluation cannot publish one. Expected
provenance and selection come from the existing expression-error owner, not sort timing.

Exercise ORDER BY an unprojected expression, alias, ordinal, and repeated expression. `SO`
proves hidden computed slots retain their LogicalSlotIds through Sort/Top-N and RequiredSlotSet
pruning while remaining absent from final user output. Retain all comparison and emitted
payload, but prune unrelated columns. Include zero-visible-column rows where the execution
model permits them and prove occurrence cardinality remains.

Poison or recycle source chunks, pages, and string heaps after Sink. Sort keys and output
VARCHAR payload must retain exact bytes through in-memory sorting, spill, merge, and Source.
A huge row is either represented exactly or receives the existing controlled
representability/OOM result; truncation, partial payload, or a dropped key is forbidden.
Verify retained rows are query-owned temporary state and complete key values remain
reachable whenever prefix comparison cannot decide.

## V30-E — PhysicalSort and in-memory lifecycle

Drive `PL` through barriers after the first and later Sink rows, after a run spill, after the
last child row, before and during Finalize, immediately before readiness, and after readiness.
Sink evaluates and retains demanded occurrences and creates runs as needed; Finalize
completes the final sort/run and merge readiness; Source is unavailable before successful
readiness and emits comparator-ordered chunks afterward. Empty input emits zero rows and one
row preserves its exact occurrence and payload.

Run the same no-spill fixtures through multiple admitted in-memory strategies or adapters.
Require exact bag, strict order, schema, slots, and demanded errors; permit tie permutations.
Do not require introsort, a standard-library primitive, stability, direct payload movement,
or one record/container layout. Compact records, inline keys, handles, and equivalent owned
representations are legal when comparator, lifetime, accounting, and output hold.
Instrument record/payload movement and prove comparison sorting does not repeatedly move
expensive payload; accept any equivalent indirection or compact-record mechanism.

## V30-F — Run generation and external merge

Under forced budgets, assign each input occurrence to exactly one generated run. Each run is
sorted by `CO`, written completely, and releases/resets its in-memory reservation only after
the owning write transition. Final k-way merge must emit the exact occurrence ledger in
strict comparator order with a legal tie permutation. Cover zero, one, two, three, and many
runs; fan-in two and greater; irregular run sizes; and multiple passes.

At each successful pass, `RO` proves a strictly improving bounded measure such as fewer
runs. With multiple runs and insufficient resources for an exact effective fan-in of at
least two, require another exact progress action or an existing terminal resource failure.
Reject fan-in-one rewriting, same-state recursion, unbounded retry, row dropping, and
approximate order. Inject a run-generation/merge comparator-descriptor mismatch and require
controlled rejection before wrong output; a finite fingerprint alone cannot establish
compatibility. Where Chapter 32 admits worker-local runs, tag every worker occurrence once
and apply the same final merge oracle. Instrument run I/O to prove readers and writers use
bounded accounted buffering without changing framing or occurrence membership.

## V30-G — Temporary format, error categories, and Source failure

Create valid and one-defect sort runs covering run identity/version, RowLayout/schema,
complete sort descriptor, record count, block framing, lengths, offsets, file ranges,
checksums, and records. Use checked arithmetic for every extent. Force incompatible
descriptors to share a finite fingerprint and require exact structural/owner validation to
reject them. Create checksum-valid blocks with malformed counts, offsets, lengths, framing,
owner association, or descriptor relation; checksum success never substitutes for parsing.

Classify short read/write, ENOSPC, checksum failure, malformed external block, spill
addressability, allocation denial, no exact runtime representation, descriptor mismatch,
and in-memory invariant defects through the Chapter-24/39 oracle. Do not misclassify an
internal invalid state merely because spill is present. Runs are query-temporary and
attempt-local: no WAL, crash recovery, persistent ABI, or cross-attempt identity.

After successful external-sort readiness, return one result chunk and fail the next required
run read. Verify query completion fails, the already-returned prefix remains returned, rows
are not retracted, and Chapter 31/39 owns the error envelope. This does not weaken the ban on
pre-readiness Source output and does not impose Chapter-29 aggregate prepublication rules.

## V30-H — Memory, cancellation, and retry

`MO` tracks retained rows, complete keys, prefixes, VARCHAR backing, run metadata,
serialization/read/write buffers, merge heap/state, output buffers, worker-local runs, and
Top-N records. For each, observe reserve, growth, spill transition, release, cancellation
cleanup, and failure cleanup. Check exact extents and distinguish OOM, representability, and
SpillIO outcomes. Memory pressure permits only exact progress or controlled failure, never
truncation or approximate sorting.

Cancel deterministically during Sink, key evaluation, in-memory/run sort, run write, merge
pass, final-merge Source, Top-N build, and Top-N retained sort. No terminally canceled attempt
publishes successful readiness; running work quiesces and query-owned state cleans without a
wall-clock deadline. Poison retained rows, prefixes, run namespace/files/cursors, merge and
Top-N heaps, readiness, and Source cursor, then start an independently authorized retry.
`AF` requires fresh execution state; only immutable plan/comparator descriptors may be
reused under their existing owner.

## V30-I — PhysicalTopN lifecycle and heap semantics

For positive demanded output, place barriers before heap size `K`, immediately after it,
after later better and tied candidates, after final input, before retained sorting, before
readiness, and after readiness. No output precedes readiness, and reaching `K` never by
itself stops demanded input. Evaluate every demanded candidate key.

With occurrence IDs, `HO` proves at most exact `K` records are retained, the root denotes a
worst retained candidate, a better row may replace it, a worse row may be discarded, and an
equal row is handled within `TO`. Equal values and physically identical rows are never
deduplicated. Cover `K` below, equal to, and above duplicate multiplicity. After input,
compare the retained output sort with `CO`, then independently apply OFFSET and LIMIT; heap
array order may never escape. Empty input yields zero rows and byte-distinct tied payloads
remain eligible occurrences.

## V30-J — Exact K, zero LIMIT, fallback, and OOM

`KO` uses arbitrary-precision arithmetic for `K = LIMIT + OFFSET` at `0+0`, `0+M`, `1+0`,
`INT64_MAX+0`, `INT64_MAX+1`, and `INT64_MAX+INT64_MAX`. LIMIT/OFFSET validity remains with
the binder/logical owner. A Top-N implementation must represent exact `K` or be ineligible;
ineligibility never invalidates SQL, raises public numeric overflow, saturates, or clamps.
An independently proven equivalent lower bound is accepted only through the Architecture
owner. Verify fallback to an exact ordering provider plus PhysicalLimit.

For LIMIT zero with OFFSET zero, one, and `INT64_MAX`, require empty logical output. A
semantic-demand proof may avoid child execution even though mathematical `K` equals OFFSET;
do not require Top-N construction. Force Top-N reservation failure for otherwise legal `K`
and require the existing OOM/resource result. Do not silently approximate or switch runtime
algorithms unless an independently owned adaptive-replanning contract authorizes it.

## V30-K — Provider substitutability, properties, and parallel execution

Compare legal outcome families for Sort + Limit, Top-N, and exact index/order provider +
Limit. They agree on cardinality, strict order, NULL/comparator semantics, demand/errors,
schema, LogicalSlotIds, and transaction/result behavior. They may choose/order different
boundary-tied occurrences and may differ in physical resource success. No RID/index order is
promoted to SQL order.

Inject a later sort-key or child error beyond the first physically available `K` rows for
each provider. A provider may stop before it only when the Chapter-20/25/26 semantic-demand
oracle proves the remainder unnecessary; merely obtaining `K` rows cannot change demanded
error behavior.

`OP` verifies PhysicalSort and PhysicalTopN advertise the exact resolved key vector and that
index providers advertise only proven descriptors. Matching includes LogicalSlotId,
direction, NULL placement, and collation; a longer vector satisfies an exact matching
prefix. Stability is absent. `SO` verifies RequiredSlotSet retains hidden keys, output,
downstream, and provenance values.

Where parallel sort is admitted, assign each occurrence to one worker-local run and verify
the final merge's exact bag and strict order. Partitioning and scheduling may vary tie order,
but worker interleaving cannot directly become output order or alter error/cancellation
ownership. Do not require parallel sort as a capability.

## V30-L — Boundaries, randomized properties, and collision negatives

Exercise empty, one-row, one-run, all-equal, duplicate, signed-zero, NaN-payload, large
VARCHAR, zero-visible-payload, and exact-K boundaries. Generate deterministic seeded cases
over key counts, types, directions, NULL policies/patterns, FLOAT edges, arbitrary VARCHAR
bytes, duplicate classes, payloads, LIMIT/OFFSET, chunking, run sizes, fan-in, and worker
partitions. `CO`, `BO`, and `TO` derive expected strict order, occurrence bag, and legal
LIMIT family; record the seed on failure.

Force collisions in normalized prefixes, auxiliary hashes, and descriptor fingerprints.
Collisions may increase comparison or trigger exact validation only; they never establish
order/identity, merge rows, or lose occurrences. Audit all V30 procedures to ensure they do
not require stability, a fixed prefix width, introsort, a future-only radix phase, Top-N
spill, or one production data structure.

## V30-M — Cross-chapter reuse map

| Handoff | Canonical contract | Reused Verification/oracle | V30 composition |
|---|---|---|---|
| Ch17 -> 30 | Scalar, FLOAT64, and binary VARCHAR total order | V17 ordering families | V30-A/C `CO`/`PO` |
| Ch19 -> 30 | ORDER BY binding, hidden keys, LIMIT/OFFSET validation | V19 binder/ordinal procedures | V30-D/J `SO`/`KO` |
| Ch20 -> 30 | Sort bag, unstable ties, LIMIT slicing, semantic demand | V20-10/V20-11 | V30-B/D/J/K `BO`/`TO`/`ER` |
| Ch22 -> 30 | Physical operators, substitutability, exact-K applicability | V22-C/G/J/K | V30-E/I/J/K |
| Ch23 -> 30 | Logical occurrences and retained-value ownership | V23 occurrence/lifetime families | V30-B/D/F `BO` |
| Ch24 -> 30 | Memory, checked spill, progress, errors, cleanup | V24 spill/resource families | V30-F/G/H `MO`/`RO`/`TF` |
| Ch25 -> 30 | Demanded expression errors and provenance | V25 error families | V30-D/K `ER` |
| Ch26 -> 30 | Blocking readiness, Source, cancellation, retry | V26 lifecycle families | V30-E/G/H/I `PL`/`AF` |
| Ch27 -> 30 | PhysicalLimit slicing | V27 limit procedures | V30-B/I/J/K `TO`/`KO` |
| Ch29 -> 30 | Ordered aggregate/DISTINCT consume this comparator | V29-M/O/T | V30-A/K `CO`/`OP` |
| Ch30 -> 31 | Ready Source and already-returned failure prefix | result publication tests | V30-G `ER` |
| Ch30 -> 32 | Worker-local runs and final merge when admitted | Parallel Execution Tests | V30-F/H/K |
| Ch30 -> 37 | OrderingProperty and RequiredSlotSet | Physical Property tests | V30-D/K `SO`/`OP` |
| Ch30 -> 38 | Exact-K capability, costing, enforcement, validation | V22-J and optimizer/enforcement tests | V30-J/K `KO`/`OP` |
| Ch30 -> 39 | SpillIO, OOM, representability, cancellation | resource/error tests | V30-G/H/J `ER`/`MO` |
| Ch30 -> 41 | Independent deterministic verification obligations | verification-contract audit | V30-A–O oracle/ledger audit |

## V30-N — Atomic Architecture-obligation ledger

The following rows are the atomic Chapter-30 coverage inventory. Each row is one
falsifiable obligation; repeated rationale, examples, navigation, and summary invariants map
to their owner row instead of increasing the count.

| ID | Architecture location | Atomic obligation | Procedure | Oracle/reuse |
|---|---|---|---|---|
| V30-A01 | 30.3 | Comparator uses the resolved key sequence. | V30-A | CO/V19 |
| V30-A02 | 30.3 | ASC preserves the scalar order. | V30-A | CO/V17 |
| V30-A03 | 30.3 | DESC reverses the scalar order. | V30-A | CO/V17 |
| V30-A04 | 30.3 | Resolved NULLS FIRST is component-local. | V30-A | CO/V19 |
| V30-A05 | 30.3 | Resolved NULLS LAST is component-local. | V30-A | CO/V19 |
| V30-A06 | 30.3 | Later keys participate only after prior-key equality. | V30-A | CO |
| V30-A07 | 17, 30.3 | Integer scalar ordering is exact. | V30-A | CO/V17 |
| V30-A08 | 17, 30.3 | DATE/TIMESTAMP scalar ordering is exact. | V30-A | CO/V17 |
| V30-A09 | 17, 30.3 | VARCHAR uses exact binary collation. | V30-A | CO/V17 |
| V30-A10 | 17, 30.3 | FLOAT64 uses the Chapter-17 total order. | V30-A | CO/V17 |
| V30-A11 | 17, 30.3 | FLOAT64 signed zeros compare equal. | V30-A | CO/V17 |
| V30-A12 | 17, 30.3 | Equivalent NaNs compare equal at the defined order position. | V30-A | CO/V17 |
| V30-A13 | 30.3–30.8 | Every sort/merge/Top-N/property path uses equivalent comparator semantics. | V30-A/F/I/K | CO/OP |
| V30-A14 | 30.3 | Host NaN, locale, pointer, C-string, and raw-byte substitutes are rejected. | V30-A | CO |
| V30-B01 | 20.11, 30.1 | PhysicalSort emits every child occurrence exactly once. | V30-B | BO/V20 |
| V30-B02 | 30.1 | PhysicalSort fabricates no occurrence. | V30-B | BO |
| V30-B03 | 30.1 | Duplicate occurrences remain duplicate occurrences. | V30-B | BO |
| V30-B04 | 20.11, 30.3 | Complete-key ties may reorder. | V30-B | TO/V20 |
| V30-B05 | 30.3 | Non-tied strict order cannot reorder. | V30-B | CO |
| V30-B06 | 20.11–20.12, 30.8 | Providers belong to one permitted ordered-result family. | V30-B/K | TO/V20 |
| V30-B07 | 30.8 | A boundary tie may select different actual occurrences. | V30-B | TO |
| V30-B08 | 20.12, 30.8 | Every provider preserves exact LIMIT/OFFSET cardinality. | V30-B | TO/V20 |
| V30-B09 | 30.8 | A tied outcome contains only child occurrences. | V30-B | BO/TO |
| V30-B10 | 30.8 | Tie freedom never authorizes deduplication or fabrication. | V30-B | BO/TO |
| V30-B11 | 30.3, 30.8 | RID is not an SQL-visible tiebreaker. | V30-B | TO/OP |
| V30-B12 | 30.3, 30.8 | Input/payload/run/worker/heap order is not a required tiebreaker. | V30-B | TO/OP |
| V30-B13 | 30.2–30.8 | Comparator equality does not canonicalize projected payload bits. | V30-B/L | BO/CO |
| V30-B14 | 30.8 | Providers may differ only within the permitted tie freedom and physical resource outcomes. | V30-B/K | TO/ER |
| V30-C01 | 30.2 | A normalized prefix is an acceleration aid, not semantic identity. | V30-C | PO |
| V30-C02 | 30.2 | Every strict prefix decision implies the same strict complete comparison. | V30-C | PO/CO |
| V30-C03 | 30.2 | Inconclusive or equal prefixes fall back to the complete comparator. | V30-C | PO/CO |
| V30-C04 | 30.2 | Prefix handling preserves ASC order. | V30-C | PO |
| V30-C05 | 30.2 | Prefix handling preserves DESC order. | V30-C | PO |
| V30-C06 | 30.2 | Prefix NULL markers preserve resolved placement. | V30-C | PO/CO |
| V30-C07 | 30.2 | Integer prefixes are order-sound. | V30-C | PO/CO |
| V30-C08 | 30.2 | DATE/TIMESTAMP prefixes are order-sound. | V30-C | PO/CO |
| V30-C09 | 30.2 | FLOAT prefixes preserve signed-zero equivalence. | V30-C | PO/CO |
| V30-C10 | 30.2 | FLOAT prefixes preserve NaN classes and infinity order. | V30-C | PO/CO |
| V30-C11 | 30.2 | VARCHAR prefixes preserve binary order. | V30-C | PO/CO |
| V30-C12 | 30.2 | Prefix strings such as `a`/`aa` cannot be inverted by truncation. | V30-C | PO/CO |
| V30-C13 | 30.2 | Embedded-NUL and boundary-termination strings fall back safely. | V30-C | PO/CO |
| V30-C14 | 30.2 | Long-common-prefix and high-byte strings fall back safely. | V30-C | PO/CO |
| V30-C15 | 30.2 | Composite-prefix decisions respect key boundaries. | V30-C | PO/CO |
| V30-C16 | 30.2 | A zero-length normalized prefix is legal and exact. | V30-C | PO/CO |
| V30-C17 | 30.2 | Prefix width is tuning, with no fixed semantic width. | V30-C | width perturbation |
| V30-C18 | 30.2 | Index-codec reuse requires proof and creates no persistent sort-prefix ABI. | V30-C | PO/persistent-codec tests |
| V30-D01 | 20, 25, 30.1 | Every semantically demanded sort key is evaluated. | V30-D | ER/V20/V25 |
| V30-D02 | 30.7 | Heap fullness or current rank cannot suppress demanded key errors. | V30-D/I | ER |
| V30-D03 | 20, 25 | Speculative undemanded evaluation cannot publish an error. | V30-D | ER/V25 |
| V30-D04 | 25, 30 | Sort timing does not replace expression error provenance/selection. | V30-D | ER/V25 |
| V30-D05 | 19, 30 | Alias/ordinal/repeated ORDER BY expressions use bound semantic identity. | V30-D | SO/V19 |
| V30-D06 | 19, 37 | Hidden ORDER BY keys retain their LogicalSlotIds. | V30-D | SO |
| V30-D07 | 37, 30 | Required hidden keys survive payload pruning. | V30-D | SO/V37 |
| V30-D08 | 30.1–30.2 | Every required emitted payload value is retained. | V30-D | SO |
| V30-D09 | 23, 37 | Zero-visible-column rows retain occurrence cardinality where admitted. | V30-D | BO/SO |
| V30-D10 | 23, 24, 30.2 | Retained VARCHAR data has stable owned lifetime. | V30-D | lifetime oracle/V23 |
| V30-D11 | 30.2, 30.5 | Source reuse/unpin cannot alter retained bytes. | V30-D | lifetime oracle |
| V30-D12 | 24, 30 | A huge row is exact or fails controllably; it is never truncated. | V30-D/F | MO/V24 |
| V30-D13 | 30.2 | Complete key values remain reachable for prefix fallback. | V30-C/D | CO/PO |
| V30-D14 | 30.2 | Sort-owned retained rows are query-temporary state. | V30-D/H | MO/AF |
| V30-E01 | 26, 30.1 | PhysicalSort is blocking. | V30-E | PL/V26 |
| V30-E02 | 30.1 | Sink consumes exactly the demanded input occurrences. | V30-E | BO/PL |
| V30-E03 | 30.1–30.2 | Sink evaluates keys and creates owned sort records. | V30-D/E | PL/SO |
| V30-E04 | 30.5 | Sink may form/spill exact runs under reservation. | V30-E/F | PL/MO |
| V30-E05 | 30.1, 30.5 | Finalize completes the final sort/run and establishes readiness. | V30-E | PL |
| V30-E06 | 26, 30.1 | Source emits nothing before successful readiness. | V30-E | PL/V26 |
| V30-E07 | 30.1 | Ready Source emits comparator-ordered chunks. | V30-E | CO/PL |
| V30-E08 | 30.1 | Empty input emits zero rows. | V30-E/L | BO |
| V30-E09 | 30.1 | One input row preserves its occurrence and payload. | V30-E/L | BO |
| V30-E10 | 30.4 | In-memory algorithm choice preserves the comparator contract. | V30-E | CO |
| V30-E11 | 20.11, 30.4 | In-memory sorting need not be stable. | V30-E | TO |
| V30-E12 | 30.2, 30.4 | Sort-record and payload-movement representation remains implementation-free. | V30-E | alternative-realization audit |
| V30-E13 | 30.4 | Comparison sorting does not repeatedly move expensive payload. | V30-E | movement instrumentation |
| V30-F01 | 24, 30.5 | A run becomes full through accounted reservation, not truncation. | V30-F | MO |
| V30-F02 | 30.5 | Each generated run is internally sorted by the complete comparator. | V30-F | RO/CO |
| V30-F03 | 30.5 | A run is written completely before its ownership transition completes. | V30-F | RO/TF |
| V30-F04 | 24, 30.5 | In-memory run state releases/resets under the memory owner. | V30-F | MO |
| V30-F05 | 30.5 | Every input occurrence belongs to exactly one generated run. | V30-F | BO/RO |
| V30-F06 | 30.5 | Spill loses or duplicates no occurrence. | V30-F | BO/RO |
| V30-F07 | 30.3, 30.5 | Run and merge comparator semantics are identical. | V30-F | CO/RO |
| V30-F08 | 30.5 | Final merge is an exact k-way merge. | V30-F | RO |
| V30-F09 | 24, 30.5 | Merge buffers and open-run state are bounded/accounted. | V30-F/H | MO |
| V30-F10 | 30.5 | Fan-in is selected from run count and available resources. | V30-F | RO/MO |
| V30-F11 | 24.6, 30.5 | Multiple runs require effective progress or controlled failure. | V30-F | RO/V24 |
| V30-F12 | 30.5 | Multi-pass merge preserves exact occurrence membership. | V30-F | BO/RO |
| V30-F13 | 24.6, 30.5 | Every successful pass strictly improves a bounded progress measure. | V30-F | RO/V24 |
| V30-F14 | 24.6, 30.5 | Fan-in-one same-state rewriting cannot loop. | V30-F | RO |
| V30-F15 | 30.5 | Final external output preserves the exact input bag. | V30-F | BO/RO |
| V30-F16 | 30.5 | External output preserves every strict comparator relation. | V30-F | CO/RO |
| V30-F17 | 20.11, 30.5 | External merging may choose a legal tie permutation. | V30-F | TO/RO |
| V30-F18 | 30.5, 32 | Admitted worker-local runs merge without occurrence loss. | V30-F/K | BO/RO |
| V30-F19 | 24, 30.5 | Run readers and writers use bounded accounted buffering. | V30-F/H | MO/RO |
| V30-G01 | 30.6 | Sort runs are query-temporary and attempt-local. | V30-G | TF/AF |
| V30-G02 | 30.6 | Run identity/version is validated. | V30-G | TF |
| V30-G03 | 30.6 | RowLayout/schema compatibility is validated. | V30-G | TF/SO |
| V30-G04 | 30.6 | Complete sort-descriptor compatibility is validated. | V30-G | TF/CO |
| V30-G05 | 30.6 | Record counts are checked against framing and extents. | V30-G | TF |
| V30-G06 | 24, 30.6 | Block lengths, offsets, and file ranges use checked extents. | V30-G | TF/V24 |
| V30-G07 | 30.6 | Checksums detect corruption but do not validate structure alone. | V30-G | TF |
| V30-G08 | 30.6 | A finite fingerprint is only a mismatch detector. | V30-G/L | TF |
| V30-G09 | 30.6 | Fingerprint collision cannot establish descriptor identity. | V30-G/L | TF |
| V30-G10 | 30.6 | Sort runs are not WAL records. | V30-G | persistence-negative oracle |
| V30-G11 | 30.6 | Sort runs are not crash-recovered or a persistent ABI. | V30-G | persistence-negative oracle |
| V30-G12 | 24, 30.6, 39 | Malformed/checksum-failing external runs use the owned spill failure. | V30-G | ER/TF |
| V30-G13 | 39 | In-memory invariant defects retain the internal-failure category. | V30-G | ER |
| V30-G14 | 31, 30.5 | A ready external Source may fail on a later spill read. | V30-G | ER/PL |
| V30-G15 | 31, 39 | A returned prefix is not retracted after that Source failure. | V30-G | result envelope |
| V30-G16 | 31, 39 | Such a prefix does not make the query successfully complete. | V30-G | result envelope |
| V30-H01 | 24, 30 | Retained records are memory-accounted. | V30-H | MO |
| V30-H02 | 24, 30 | Complete keys and prefixes are memory-accounted. | V30-H | MO |
| V30-H03 | 24, 30 | Payload and VARCHAR backing are memory-accounted. | V30-H | MO |
| V30-H04 | 24, 30.5–30.6 | Run metadata and I/O buffers are memory-accounted. | V30-H | MO |
| V30-H05 | 24, 30.5 | Merge heap/state and output buffers are memory-accounted. | V30-H | MO |
| V30-H06 | 24, 30, 32 | Worker-local runs and Top-N state are memory-accounted. | V30-H | MO |
| V30-H07 | 24 | Every allocation/extent is exactly representable or fails. | V30-H | MO/V24 |
| V30-H08 | 24, 39 | OOM, representability, and SpillIO remain distinct outcomes. | V30-G/H | ER/MO |
| V30-H09 | 24.6, 30.5 | Memory pressure permits exact progress or controlled failure only. | V30-F/H | MO/RO |
| V30-H10 | 30 | Resource failure never permits approximate sorting. | V30-H | CO/BO/MO |
| V30-H11 | 26, 30 | Cancellation prevents later successful readiness. | V30-H | PL/V26 |
| V30-H12 | 24, 26, 30 | Cancellation quiesces work and cleans query-owned state. | V30-H | MO/AF |
| V30-H13 | 21, 26, 30 | An authorized retry initializes fresh sort/Top-N execution state. | V30-H | AF |
| V30-H14 | 24, 30.6 | Failed-attempt run files/cursors never participate in retry. | V30-H | AF/TF |
| V30-I01 | 22, 30.7 | Eligible PhysicalTopN realizes ORDER BY with LIMIT/OFFSET. | V30-I/J | KO/TO |
| V30-I02 | 26, 30.7 | Positive demanded Top-N output is blocking. | V30-I | PL |
| V30-I03 | 30.7 | Positive demanded Top-N output emits nothing before complete input and readiness. | V30-I | PL |
| V30-I04 | 30.7 | Reaching heap size K does not authorize input termination. | V30-I | ER/PL |
| V30-I05 | 30.7 | The heap retains at most exact K occurrence records. | V30-I | HO/KO |
| V30-I06 | 30.7 | The heap root represents a worst retained candidate. | V30-I | HO/CO |
| V30-I07 | 30.7 | A better candidate may replace the root. | V30-I | HO/CO |
| V30-I08 | 30.7 | A worse candidate may be discarded. | V30-I | HO/CO |
| V30-I09 | 30.7–30.8 | Equal-candidate handling remains within the tie family. | V30-I | HO/TO |
| V30-I10 | 30.7 | Top-N never deduplicates equal values. | V30-I | BO/HO |
| V30-I11 | 30.7 | Top-N never fabricates retained occurrences. | V30-I | BO/HO |
| V30-I12 | 20, 25, 30.7 | Every demanded candidate sort key is evaluated. | V30-D/I | ER |
| V30-I13 | 30.7 | Retained records are sorted before emission. | V30-I | HO/CO |
| V30-I14 | 30.7 | Retained sorting uses the complete comparator. | V30-I | CO/HO |
| V30-I15 | 20.12, 30.7 | OFFSET is applied to the ordered retained sequence. | V30-I | TO/HO |
| V30-I16 | 20.12, 30.7 | At most LIMIT rows are emitted after OFFSET. | V30-I | TO/HO |
| V30-I17 | 30.7 | Heap-array order never becomes SQL output. | V30-I | HO/CO |
| V30-I18 | 30.7 | Duplicate occurrences are retained according to exact K, not set semantics. | V30-I | BO/HO |
| V30-I19 | 30.7 | Empty Top-N input emits zero rows. | V30-I/L | BO |
| V30-I20 | 30.7–30.8 | Top-N preserves declared schema, slots, and result envelope. | V30-I/K | SO/ER |
| V30-J01 | 19, 20, 30.7 | LIMIT/OFFSET validity remains owned upstream. | V30-J | KO/V19/V20 |
| V30-J02 | 22, 30.7 | K is the exact mathematical LIMIT plus OFFSET. | V30-J | KO/V22-J |
| V30-J03 | 30.7 | Exact K may exceed INT64. | V30-J | KO |
| V30-J04 | 22, 30.7 | Top-N applicability requires exact representability of K. | V30-J | KO/V22-J |
| V30-J05 | 22, 30.7 | Unrepresentable K makes Top-N ineligible. | V30-J | KO |
| V30-J06 | 22, 30.7 | Top-N ineligibility does not invalidate SQL or raise count overflow. | V30-J | KO/ER |
| V30-J07 | 30.7, 38 | Ineligible Top-N falls back to an exact order provider plus Limit. | V30-J/K | KO/OP |
| V30-J08 | 30.7 | K is never saturated. | V30-J | KO |
| V30-J09 | 30.7 | K is never clamped without an owner-proved equivalent bound. | V30-J | KO |
| V30-J10 | 20, 26, 30.7 | LIMIT 0 OFFSET 0 may avoid child execution through semantic demand. | V30-J | KO/ER |
| V30-J11 | 20, 30.7 | LIMIT 0 OFFSET M is empty although mathematical K equals M. | V30-J | KO/TO |
| V30-J12 | 38 | Required-row estimates cannot alter exact K semantics. | V30-J/K | KO |
| V30-J13 | 24, 30.7 | Top-N reservation failure has the existing controlled OOM outcome. | V30-J | MO/ER |
| V30-J14 | 30.7, 38 | Runtime Top-N failure cannot trigger unowned approximate/adaptive output. | V30-J | KO/ER |
| V30-K01 | 30.8 | Sort+Limit and Top-N belong to the same permitted result family. | V30-K | TO |
| V30-K02 | 30.8 | Exact index/provider + Limit belongs to that same family. | V30-K | TO/OP |
| V30-K03 | 30.8 | Provider substitution preserves exact result cardinality. | V30-K | TO |
| V30-K04 | 30.8 | Provider substitution preserves strict comparator order. | V30-K | CO/TO |
| V30-K05 | 30.8 | Provider substitution preserves NULL and complete-comparator semantics. | V30-K | CO |
| V30-K06 | 20, 25, 30.8 | Provider substitution preserves demanded errors. | V30-K | ER |
| V30-K07 | 22, 30.8 | Provider substitution preserves schema and LogicalSlotIds. | V30-K | SO |
| V30-K08 | 30.8, 31, 39 | Provider substitution preserves transaction/result semantics. | V30-K | ER |
| V30-K09 | 30.8 | Providers may differ in boundary-tied membership/order. | V30-B/K | TO |
| V30-K10 | 22.4.1, 30.8 | Physical providers may differ in resource success/failure. | V30-K | ER/V22 |
| V30-K11 | 30.8, 37 | Sort/Top-N provide the exact resolved ordering vector. | V30-K | OP |
| V30-K12 | 37 | OrderingProperty contains no stability guarantee. | V30-K | OP |
| V30-K13 | 37 | Ordering-prefix satisfaction requires exact descriptor matches. | V30-K | OP |
| V30-K14 | 37 | RequiredSlotSet retains hidden, output, downstream, and provenance slots. | V30-D/K | SO |
| V30-K15 | 30, 32 | Parallel sorting preserves exact bag and strict order. | V30-F/K | BO/CO |
| V30-K16 | 20.11, 30, 32 | Parallel partition/schedule changes may alter tie order only. | V30-K | TO/ER |
| V30-L01 | 30.1, 30.7 | Empty/one-row/one-run fast paths preserve ordinary semantics. | V30-L | BO/CO |
| V30-L02 | 17, 30 | Byte-distinct signed-zero/NaN payloads survive comparator ties. | V30-B/L | BO/CO |
| V30-L03 | 30 | Seeded randomized fixtures use independent comparator/bag/tie oracles. | V30-L | CO/BO/TO |
| V30-L04 | 30.2, 30.6 | Prefix/hash/fingerprint collisions cannot establish semantics. | V30-C/G/L | PO/TF |
| V30-L06 | 30.2 | Verification does not require an 8–16-byte prefix. | V30-C/O | width audit |
| V30-L07 | 30.4 | Verification does not require introsort or a named library algorithm. | V30-E/O | implementation-freedom audit |
| V30-L08 | 30.4 | Radix/normalized/hybrid sort is optional when comparator-equivalent. | V30-C/E/O | CO/PO |
| V30-L09 | 30 | Verification preserves container, fan-in, buffer, heap, and run-format freedom. | V30-E–O | alternative-realization audit |
| V30-L10 | 30, 41 | Every Chapter-30 obligation uses a deterministic independent oracle or exact reuse. | V30-A–O | oracle/reuse audit |


## V30-O — Negative-case and oracle checks

Search all Chapter-30-facing Verification and reject, through the named procedures above:
stable full Sort; stability in OrderingProperty; hidden RID/input/payload/run/worker tie
order; identical boundary-tied occurrences across Sort, Top-N, or index providers; prefix
equality as full-key equality; prefixes as hashes; a required 8–16-byte width; required
introsort; radix as future-only or forbidden; runtime prefixes as persistent index encoding;
locale/C-string VARCHAR; host NaN comparison; canonicalized projected signed zero or NaN;
heap-full early termination; heap-order output; saturated/clamped K; SQL invalidity from
Top-N inapplicability; collision-free fingerprints; checksum-only structural validation;
fan-in-one indefinite progress; huge-row truncation; retraction of a returned prefix after
post-readiness failure; WAL/persistent spill runs; failed-attempt run reuse; and
resource-failure approximation. Each stale assumption is directly falsifiable by V30-A–N.

Exercise permitted prefix widths, comparison/radix/hybrid strategies, sort-record and
container representations, fan-ins, buffers, block/checksum encodings, heap containers,
run layouts, worker counts, and tie permutations. The compact Sort Tests are a smoke-test
index; V30-A–O supplies the detailed Chapter-30 procedures.
