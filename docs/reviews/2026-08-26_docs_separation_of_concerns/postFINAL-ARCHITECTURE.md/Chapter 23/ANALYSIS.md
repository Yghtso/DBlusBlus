# Chapter-23 verdict

**CHAPTER 23 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED.**

Findings:

```text
BLOCKING    3
MAJOR       2
MINOR       1
EDITORIAL   0
```

Four frozen Chapter-23 semantic questions require owner decisions before cleanup or Verification synchronization.

## Repository state

Initial and final state were identical:

```text
HEAD    f0c486f7b420a1f1bc6b4739800f7ada64c330fd
status  clean
index   clean
```

`git diff --check` passed. Audit-created changes: **NONE**.

Historical review artifacts were not read, modified, moved, or staged.

## Chapter boundaries and structure

- Exact title: `# 23. Vectorized Data and String Representation`
- Start: [docs/ARCHITECTURE.md:19116](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19116)
- End: line 19490; final substantive line 19487, separator at 19489
- Chapter 24 begins at [line 19491](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19491)
- Chapter-24 heading: `# 24. Query Memory, Row Storage, and Spill`

### Complete heading inventory

| Section | Exact heading | Responsibility | Upstream | Downstream | Role |
|---|---|---|---|---|---|
| 23 | Vectorized Data and String Representation | Runtime batch/value representation | Ch17/20/22 | Ch24–31 | Architecture |
| 23.1 | Standard vector size and DataChunk | Chunk shape, capacity/cardinality, EOS distinction | Ch22 | Ch25–27 | Architecture; findings |
| 23.2 | Why 1024 | Tuning rationale and persistence distinction | Ch22 | Ch24–26 | Architecture |
| 23.3 | Vector kinds | V1 representation vocabulary | Ch17/22 | Ch25–30 | Architecture with document-role issue |
| — | FLAT | Contiguous physical values | Ch17 | Ch25/27 | Architecture |
| — | CONSTANT | Repeated scalar representation | Ch17/20 | Ch25 | Architecture; cross-section aliasing question |
| — | DICTIONARY | Child plus selection indirection | Ch20/22 | Ch25–30 | Architecture; findings |
| 23.4 | Flat fixed-width vectors | Runtime fixed-width storage | Ch17 | Ch25 | Architecture with document-role issue |
| 23.5 | Validity mask | Runtime NULL representation | Ch17 | Ch25–31 | Architecture |
| 23.6 | SelectionVector | Logical-to-physical lane mapping | Ch20/22 | Ch25–28 | Architecture; blocking finding |
| 23.7 | Dictionary composition | Bounded indirection normalization | 23.6 | Ch25 | Architecture; inherited bounds finding |
| 23.8 | UnifiedVectorFormat | Representation-neutral kernel view | 23.3–23.7 | Ch25 | Architecture |
| 23.9 | VARCHAR `StringRef` | Runtime length/prefix/reference structure | Ch17 | Ch24–31 | Architecture; blocking finding |
| 23.9.1 | Prefix definition | Exact first-four-byte cache | Ch17 | Ch25/29/30 | Architecture |
| 23.10 | String ownership | Valid owners and retention boundary | Ch5/17/22 | Ch24–31 | Architecture; blocking finding |
| 23.11 | DataChunk StringHeap | Page-to-chunk VARCHAR ownership | Ch5/22 | Ch27 | Architecture |
| 23.12 | Borrowed vectors | Synchronous borrowing and retention | Ch22 | Ch25/26 | Architecture; blocking finding |
| 23.13 | Chunk/vector reuse | Reset and reusable capacity | Ch22/24 | Ch25–27 | Architecture |
| 23.14 | Vector/string invariants | Consolidated representation invariants | all above | all consumers | Architecture |

## Context consulted

Architecture context:

- Front matter
- §§5.10–5.12, 5.20–5.21
- §§17.1–17.4, 17.7, 17.10–17.13
- §§19.1–19.2
- §§20.1–20.2, 20.10–20.12, 20.20
- §§22.1–22.3, 22.5–22.6, 22.8
- Chapter 24 for memory/row/spill ownership boundary only
- Chapter 25 for normalized vector consumption
- Chapter 26 for chunk/EOS/borrowed lifetime handoff
- §39.3
- §41.5

Verification context:

- V20 physical-width independence
- V22-G, V22-L, V22-M
- Vector Correctness Tests
- Expression Execution Tests
- String Lifetime Tests
- Pipeline Finalization and Resource Tests
- Scan and Unary Operator Tests

Chapter 24 was not reviewed as a chapter.

# Canonical owner map

| Concept | Classification | Canonical responsibility |
|---|---|---|
| SQL scalar domains/equality/order | EARLIER OWNER | Chapter 17 |
| Bag occurrences/order | EARLIER OWNER | Chapter 20 |
| Physical output schema/LogicalSlotIds | EARLIER OWNER | Chapter 22 |
| DataChunk | CHAPTER 23 OWNS | Runtime column batch |
| Capacity/cardinality | CHAPTER 23 OWNS | Runtime batch bounds |
| Empty chunk/EOS distinction | CHAPTER 23 OWNS with Ch26 handoff | Empty batch versus source status |
| FLAT/CONSTANT/DICTIONARY | CHAPTER 23 OWNS | V1 vector representations |
| SelectionVector | CHAPTER 23 OWNS | Logical-to-physical indirection |
| Validity mask | CHAPTER 23 OWNS | Runtime NULL state |
| UnifiedVectorFormat | CHAPTER 23 OWNS | Normalized kernel view |
| StringRef | CHAPTER 23 OWNS | Runtime byte reference |
| StringRef prefix | CHAPTER 23 OWNS | Exact cache semantics |
| Borrowed/owned lifetime | CHAPTER 23 OWNS | Representation lifetime contract |
| Heap/page tuple bytes | EARLIER OWNER | Chapter 5 |
| Query memory/accounting | LATER OWNER | Chapter 24 |
| Spill serialization | LATER OWNER | Chapter 24 and operator chapters |
| Expression kernels/error selection | LATER OWNER | Chapter 25 |
| Pipeline chunk lifetime/EOS | LATER OWNER | Chapter 26 |
| Scan decoding | LATER OWNER | Chapter 27 |
| Retained join/aggregate/sort data | LATER OWNER | Chapters 28–30 |
| DML/result persistence | LATER OWNER | Chapter 31 |
| Pointer persistence prohibition | SHARED HANDOFF | Chapters 22–24 |
| Plan/output slot map | AMBIGUOUS HANDOFF | Chapter 22 owns schema, but §23.1 does not state how `columns[]` realizes it |

# Core semantic models

## DataChunk

The live model is:

```text
DataChunk
    capacity
    cardinality
    Vector columns[]
    chunk-local StringHeap
```

- Standard capacity: 1024.
- Maximum stated capacity: 65535.
- All normal columns share one logical cardinality.
- `0 <= cardinality <= capacity`.
- Partial final chunks are legal.
- Cardinality zero is an ordinary empty batch.
- EOS is a separate source status.
- Capacity storage may be reused after borrowers finish.

The chapter does not explicitly connect `Vector columns[j]` to physical output-schema position `j` or another exact `LogicalSlotId` map. That is M23-2.

## Capacity/cardinality boundary matrix

| Capacity/cardinality case | Live result |
|---|---|
| capacity 0, cardinality 0 | Not prohibited; semantic/liveness policy undefined |
| cardinality 0 | Valid empty chunk |
| cardinality 1 | Valid if capacity ≥1 |
| cardinality capacity−1 | Valid |
| cardinality capacity | Valid |
| cardinality capacity+1 | Invalid |
| capacity 1024 | Standard |
| capacity 65535 | Valid maximum |
| capacity 65536 | Invalid under current contract |
| capacity larger than SQL result | Split into chunks; no SQL row-count error |
| cardinality growth | Active positions must represent values, but transition/initialization protocol is implicit |
| inactive storage | Semantically inaccessible |

A positive minimum capacity or restricted zero-capacity use requires a frozen decision.

## Logical and physical positions

The intended mapping is:

```text
logical chunk position i
    -> effective selection[i]
    -> base physical position
    -> value/validity payload
```

A lane is not a RID, `LogicalSlotId`, `BindingId`, row-occurrence identity, or semantic tie-breaker.

Selection order defines logical order within that vector view. Thus, assuming indices are legal:

```text
[2,0,2] -> base[2], base[0], base[2]
```

Repeated indices represent repeated logical occurrences.

The unresolved point is the legal selected-position domain: allocated capacity versus initialized active positions.

## Row and chunk order

- Physical lane order alone does not establish SQL order.
- For an ordered provider, logical row sequence within each chunk and chunk production sequence jointly realize the order.
- Dictionary/selection order must preserve that logical sequence.
- Equal sort-key ties remain governed by Chapter 30.
- Parallel merge/scheduling remains downstream-owned.

# Vector representations

| Representation | Payload | Logical indexing | Validity | Aliasing/lifetime | Status |
|---|---|---|---|---|---|
| FLAT | contiguous typed slots | effective position selects slot | per physical slot | may store or reference storage | Defined |
| CONSTANT | one scalar | every logical row maps to scalar position zero | one repeated validity state | mutation semantics unresolved | Finding |
| DICTIONARY | child vector + selection | logical `i` maps through selection | selected child validity | borrows child/selection storage | Finding |
| UnifiedVectorFormat | data pointer + effective selection/validity views | normalized | normalized view | adapter only | Defined |
| SEQUENCE | absent from v1 | N/A | N/A | N/A | Deferred wording is temporal |
| RLE | absent from v1 | N/A | N/A | N/A | Deferred wording is temporal |

Representation substitutions must preserve values, NULLs, cardinality, order, multiplicity, VARCHAR bytes, demanded errors, and lifetime safety.

## Representation substitutability

| Pair | Applicable domain | Semantic result |
|---|---|---|
| FLAT ↔ CONSTANT | all logical positions carry one equivalent value/NULL | Identical |
| FLAT ↔ DICTIONARY | selection identifies all logical values | Identical |
| CONSTANT ↔ DICTIONARY | dictionary selects one equivalent scalar repeatedly | Identical |
| nested dictionary ↔ normalized dictionary | composed indices legal | Identical |
| borrowed ↔ owned | owner lifetime satisfied or bytes copied | Identical |
| representation conversion resource failure | allocation unavailable | Existing resource error; never different SQL value |

## Selection/dictionary matrix

| Case | Required result | Status |
|---|---|---|
| empty selection | zero logical rows | Consistent |
| one selected element | one occurrence | Consistent |
| repeated index | repeated occurrences | Consistent by direct mapping |
| unsorted indices | output follows listed order | Consistent by direct mapping |
| first active position | selected value | Consistent |
| last active position | selected value | Blocked by “physical bounds” definition |
| allocated but inactive position | ambiguous | **FINDING** |
| index ≥ capacity | invalid | Consistent |
| nested selection | compose outer then inner | Consistent |
| composition duplicates | preserve duplicates | Consistent |
| composition reordering | preserve outer logical order | Consistent |
| chain depth > effective one in hot kernel | normalize first | Consistent |
| cycle/missing base | malformed internal state | Internal invalid state |

# Validity and types

## Validity matrix

| Representation/state | Effective NULL rule |
|---|---|
| FLAT | selected physical slot’s validity bit |
| CONSTANT | one scalar validity repeated |
| DICTIONARY | selected child validity after composition |
| `all_valid=true` | all active values non-NULL; bitmap need not be read |
| NULL payload bytes | semantically ignored |
| bits outside active domain | must not affect results |
| NULL VARCHAR | reference payload must not be dereferenced |
| non-NULL empty VARCHAR | valid state with length zero |
| FLOAT NaN | non-NULL unless validity says NULL |
| LEFT JOIN extension | every right output validity is NULL |

Validity polarity is exact:

```text
1 = valid/non-NULL
0 = NULL
```

Words are `uint64`, but runtime bit ordering within words is not frozen. That is appropriate because it is process-local.

## Runtime type/storage matrix

| Type | Runtime FLAT storage | Canonical semantics owner |
|---|---|---|
| BOOLEAN | `uint8`, byte per value | Chapter 17 |
| INT32 | `int32` | Chapter 17 |
| INT64 | `int64` | Chapter 17 |
| FLOAT64 | binary64/double | Chapter 17 |
| DATE | `int32` day count | Chapter 17 |
| TIMESTAMP | `int64` microseconds | Chapter 17 |
| VARCHAR | StringRef | Chapter 17 value semantics; Chapter 23 lifetime |

Runtime storage is process-local and distinct from little-endian persisted codecs. BOOLEAN runtime values still represent only TRUE/FALSE. Runtime validity—not a NaN sentinel—represents NULL.

FLOAT copies may preserve arbitrary input NaN payload bits, while consuming equality/order follows Chapter 17: all NaNs are equivalent and signed zeros compare equal. Arithmetic result canonicalization remains Chapter 17/25-owned.

# VARCHAR and StringRef

The live structure is:

```text
StringRef {
    uint32 length
    uint32 prefix
    const char* data
}
```

- `length` is exact byte length.
- No NUL terminator is required.
- Embedded NUL is ordinary data.
- Empty non-NULL and NULL remain distinct through validity.
- Prefix is unsigned, big-endian first-four bytes with zero fill.
- Equal prefix never proves full equality/order.
- Exact bytes and length determine VARCHAR value.

The unresolved resource-domain issue is that `uint32 length` cannot represent every mathematical finite byte string allowed by Chapter 17’s resource-qualified domain, but the chapter does not define the resulting limit/error or permit another exact runtime representation.

## VARCHAR lifetime matrix

| Case | Owner | Valid through | Copy required to retain? | Valid after owner reset? |
|---|---|---|---:|---:|
| owned empty string | chunk/query/operator storage | owner lifetime | only if crossing owner lifetime | no, unless copied |
| owned embedded-NUL string | same | owner lifetime | same | same |
| borrowed input-chunk string | input chunk/StringHeap | synchronous consumption | yes beyond batch | no |
| heap/page string | pinned page only; scans copy to chunk | guard/pin lifetime | scan performs copy | no after unpin |
| retained across chunk reset | new retaining owner required | retaining owner lifetime | yes | yes after copy |
| constant string vector | stable plan/query/chunk owner | owner lifetime | if retained longer | owner-dependent |
| dictionary string | underlying owner | base/byte owner lifetime | yes beyond it | no |
| repeated dictionary index | same bytes, multiple occurrences | owner lifetime | if retained longer | no |
| NULL string | no dereference required | validity state | no byte copy | value remains NULL only while vector state valid |
| zero-length non-NULL | no byte dereference required | vector/owner lifetime | owner-dependent | owner-dependent |

## Owner/lifetime graph

| Owner | Borrower | Borrow begins | Borrow ends | Invalidation | Required action |
|---|---|---|---|---|---|
| DataChunk StringHeap | StringRef/vector | value production | synchronous downstream completion | chunk reset/reuse | copy before retention |
| input Vector/DataChunk | reference/dictionary output | streaming operator return | downstream consumption | owner reset/reuse—or unresolved mutation | retain owner or materialize |
| pinned heap page | temporary tuple view | page decode | guard release | unpin | scan copies VARCHAR to chunk |
| QueryArena/stable constants | constant vectors | execution setup | query/owner end | arena release | copy if longer lifetime |
| RowCollection/block | retained vector/row refs | append/materialize | collection/block release | release/spill replacement | use owned row representation |
| blocking operator storage | output views | finalization/output | state release | operator cleanup | result sink retains if needed |
| sort/run storage | sorted output view | finalized run consumption | run release | cleanup | materialize at longer boundary |
| result spool/client owner | client-visible values | result materialization | client/result release | release | must own exact bytes |

No ownership cycle is required. Selection storage ownership is not named separately and should be covered by the borrowed vector’s owner; that should be made explicit alongside Q23-2.

# Reset, reuse, aliasing, and copying

Reset specifies:

```text
cardinality = 0
clear/reinitialize vector logical state
reset chunk StringHeap
preserve allocated capacity
```

This is sufficient to make old positions inaccessible after reset, provided:

- every new active position is initialized before consumption;
- stale validity/selection metadata cannot influence new cardinality;
- StringHeap reset waits for all borrowers;
- cardinality never expands over uninitialized slots.

Those requirements are mostly implicit rather than independently enumerated.

Copy semantics by context:

- “deep-copy” means exact value/NULL/cardinality/order/multiplicity and owned VARCHAR bytes.
- Dictionary normalization is a representation-only selection copy and need not copy payload.
- Borrowed/reference vector creation is shallow and lifetime-bound.
- Move/transfer semantics are not architectural; N/A.
- Flattening/materialization must preserve semantic contents and establish sufficient ownership.

Aliasing is permitted for borrowed/reference/dictionary vectors, but the chapter does not say whether a borrow is an immutable snapshot or a live view if base storage mutates while still alive. This is Q23-2.

# Memory, persistence, and resource boundaries

## Memory-owner taxonomy

| Memory | Owner | Accounting/lifetime owner |
|---|---|---|
| vector fixed payload | chunk/operator state or borrowed source | Ch23 lifetime; Ch24 accounting |
| validity storage | vector/chunk or borrowed base | Ch23 |
| selection storage | dictionary/operator/chunk state | Ch23, but explicit owner wording is thin |
| chunk StringHeap | DataChunk | Ch23; Ch24 accounting |
| stable constants | plan/query owner | Ch22/24 |
| retained rows/varlen | RowCollection/blocking state | Ch24/operator chapter |
| QueryArena | query | Ch24 |
| spill buffers/files | SpillManager/operator | Ch24 |
| BufferPool frame bytes | page guard/frame | Ch5/7 |
| persistent tuple/catalog/WAL bytes | persistent codec owner | Ch5/16/17; never raw vectors |

Runtime pointer/ABI state is process-local. Spill must serialize explicit values/offsets, not raw `StringRef`, vector, selection, or DataChunk memory.

The standard vector size is a tuning constant, not persistent format. Runtime endianness and exact validity bit ordering need not be persistent.

## Resource/maxima classification

| Bound | Classification |
|---|---|
| standard 1024 | runtime tuning default |
| capacity ≤65535 | runtime representation bound |
| uint16 selection index | runtime representation choice |
| uint32 StringRef length | runtime representation bound with unresolved failure/alternative policy |
| VARCHAR SQL domain | Chapter-17 finite byte strings under owner resource limits |
| query memory | Chapter 24 |
| chunk payload allocation | resource operation, not SQL row-count semantics |
| total result rows | not bounded by chunk capacity; split across chunks |
| spill bytes | temporary Chapter-24/operator format |

# Operator and pipeline handoffs

| Consumer | Chapter-23 handoff |
|---|---|
| Expressions | UnifiedVectorFormat; active logical selection and validity |
| Filter | output selection preserves order/multiplicity; FALSE/UNKNOWN removed |
| Project | borrowed column or owned computed result |
| SeqScan | fixed values copied; VARCHAR copied to chunk StringHeap |
| IndexScan | same lifetime rules after heap fetch |
| Join | output schema left then right; dictionary duplicates preserve occurrence count |
| LEFT JOIN | right-side validity set NULL regardless of stale payload |
| Aggregate | vectors supply typed values; state retention owns/copies |
| DISTINCT | semantic equality, not pointer/bit identity |
| Sort | retained VARCHAR becomes sort/run-owned; order spans chunks |
| Top-N | ordered chunking does not alter exact-K semantics |
| Limit | cardinality/selection trims without exposing stale positions |
| Scalar/IN subquery | retained materialization follows ordinary ownership |
| DML | tuple codec encodes values; runtime pointers never persist |
| ResultSink | materializes or safely retains client-visible memory |
| Pipeline | empty batch separate from FINISHED; borrowed data consumed synchronously |
| Parallel runtime | worker-local ownership and scheduling delegated to Chapters 26/32 |

Vector capacity, chunk boundaries, lane positions, and representation must not select different errors. Chapter 25/operator owners select demanded errors.

# Invalid-state and validation matrix

| Malformed state | Required classification |
|---|---|
| capacity >65535 | internal invalid representation |
| cardinality >capacity | internal invalid representation |
| zero capacity | unresolved legal/precondition status |
| selection ≥allocated capacity | internal invalid representation |
| selection inside capacity but outside active initialized domain | unresolved |
| validity storage too short | internal invalid representation |
| all-valid claim inconsistent with intended NULL state | internal invalid representation |
| type/schema mismatch | physical-plan/runtime invariant failure |
| StringRef length cannot be represented | unresolved resource/domain result |
| non-NULL positive-length StringRef without valid bytes | internal lifetime violation |
| expired byte owner | use-after-lifetime, nonconforming |
| missing dictionary base | internal invalid state |
| unbounded/cyclic dictionary chain | invalid; normalization must bound effective indirection |
| stale selection after reset | internal invalid state |
| active slot not freshly initialized | internal invalid state |
| raw pointer serialized | forbidden persistence violation |

No expensive universal runtime validation policy is frozen. Construction/operator boundaries must nevertheless prevent malformed state from causing out-of-bounds access or undefined behavior. Internal malformed representation is not a new SQL semantic error; genuine allocation failure remains Chapter-24/39 resource failure.

# Determinism matrix

| Perturbation | Values/NULL/bag/order/error/transaction/persistence may change? |
|---|---|
| chunk capacity | No |
| chunk boundary | No |
| physical vector position | No |
| selection layout | No, when it represents the same logical sequence |
| repeated dictionary index | No; it preserves repeated occurrences |
| pointer address | No |
| allocation order | No |
| arena address | No |
| string storage location | No |
| FLAT/CONSTANT/DICTIONARY | No |
| runtime NaN payload bits | No semantic comparison/grouping effect |
| worker ownership | No |
| validity garbage outside active positions | No |
| reused inactive storage | No |
| exact prefix cache | No; full comparison resolves ties |

# Cross-chapter handoff matrix

| Boundary | Contract | Duplication/ambiguity | Status |
|---|---|---|---|
| Ch17→23 | typed scalar and NULL/FLOAT/VARCHAR semantics | no duplicate semantic owner | Consistent |
| Ch20→23 | bags, occurrence multiplicity, semantic order | representation preserves them | Consistent |
| Ch22→23 | output schema, runtime ownership, vector independence | column/schema mapping not explicit | **Finding** |
| Ch23→24 | lifetime representation to accounting/retention/spill | no property duplication | Consistent |
| Ch23→25 | normalized values/selections/validity | exact | Consistent |
| Ch23→26 | DataChunk flow, EOS status, borrowing | value-stability mutation rule missing | **Finding** |
| Ch23→27 | scan copies page VARCHAR into chunk | exact | Consistent |
| Ch23→28 | retained join data and duplicate selections | selection active-domain question | **Finding** |
| Ch23→29 | grouping/DISTINCT equality | Chapter 17 remains owner | Consistent |
| Ch23→30 | comparator/order and retained strings | exact | Consistent |
| Ch23→31 | DML/result ownership and persistence | raw pointers forbidden | Consistent |

# Documentation audits

## Temporality

Project-time phrases:

| Line | Phrase | Class |
|---:|---|---|
| 19176 | “The initial vector representations are” | E — project chronology |
| 19202 | “Later representations such as” | E — roadmap |
| 19209 | “deferred until measurement justifies them” | E — future optimization sequencing |
| 19234 | “not bit-packed in the initial executor” | E — implementation-generation narration |
| 19418 | “current input-consumption lifetime” | A — legitimate runtime lifetime |

Project chronology phrase count: **4** across three passages.

Timeless equivalents could state that FLAT/CONSTANT/DICTIONARY are the v1 set, SEQUENCE/RLE are outside the v1 baseline, and BOOLEAN is byte-per-value in v1.

## Document ownership

- DEVELOPMENT leakage: none beyond the chronology wording above.
- VERIFICATION procedure: none.
- PROJECT_STATE/current-implementation narration: one “initial executor” passage.
- History/devlog leakage: none.
- Benchmark recipe: none. “benchmark-configurable” describes a tuning parameter, not a procedure.
- Current implementation status: one chronology phrase, no capability inventory.

Chapter 23 cannot yet stand years later without timeline knowledge.

## Implementation coupling

| Item | Classification |
|---|---|
| exact 1024 standard | correctness-compatible tuning architecture |
| uint16 selection | deliberate representation constraint |
| uint64 validity words | process-local implementation constraint |
| naturally aligned buffers | useful execution representation |
| byte-per-BOOLEAN | explicit v1 representation |
| concrete StringRef fields | explicit architecture; length consequence unresolved |
| “naturally 16 bytes on a 64-bit process” | implementation/ABI coupling, analytically useful but not semantic |
| exact validity bit order omitted | appropriate implementation freedom |
| runtime endianness omitted | appropriate implementation freedom |
| allocator/container/SIMD width | not frozen |

## Terminology

| Term | Canonical meaning | Status |
|---|---|---|
| DataChunk | common-cardinality vector batch | Clear |
| capacity | allocated physical-position bound | Clear except zero legality |
| cardinality | active logical occurrence count | Clear |
| logical position | active row position before indirection | Clear |
| physical position | base vector slot selected by mapping | Bounds ambiguous |
| FLAT | contiguous typed physical slots | Clear |
| CONSTANT | one scalar repeated | Clear; mutation missing |
| DICTIONARY | child plus selection | Clear; bounds/mutation missing |
| SelectionVector | logical-to-physical index map | Domain ambiguous |
| validity | non-NULL bit state | Clear |
| all_valid | fast-path metadata | Clear |
| UnifiedVectorFormat | normalized data/selection/validity view | Clear |
| StringRef | length/prefix/pointer runtime reference | Clear except large length |
| borrowed | owner-backed synchronous view | Lifetime clear, value stability unclear |
| owned | bytes retained by named query/chunk/operator owner | Clear |
| reset | clear logical state and StringHeap, preserve capacity | Clear |
| reuse | refill storage after borrowers finish | Clear |
| flatten/normalize | bounded effective selection | Clear |
| deep copy | retaining owner obtains exact varlen bytes | Clear |

## Normative language and analytical depth

Normative rules are appropriately strong for capacity maximum, cardinality, EOS, selected-index bounds, inactive validity, StringRef lifetime, and reset safety. Performance choices remain mostly descriptive.

Analytical strengths:

- 1024 rationale;
- runtime/persistent separation;
- exact prefix behavior;
- page-to-chunk string copy rationale;
- borrowing and retention rationale;
- dictionary normalization.

Depth gaps:

- selected active-domain definition;
- borrowed value stability under mutation;
- large StringRef length outcome;
- zero-capacity liveness;
- physical schema-to-column mapping.

Implementation freedom is generally preserved for allocation, validity layout, endianness, flattening mechanics, storage owners, and parallel scheduling. The fixed `uint32` StringRef length currently closes freedom without defining its semantic/resource consequence.

# Explicit cross-references

| Source | Target | Purpose | Exists/owner | Status |
|---|---|---|---|---|
| §23.9.1 | Chapter 17 | binary VARCHAR collation | yes/canonical | GOOD |
| §23.14 invariant 10 | §23.9.1 | StringRef prefix definition | yes/local owner | GOOD |

Implicit owner references—“query memory contract” and “pipeline executor”—correctly point conceptually to Chapters 24 and 26, but precise navigation would be useful. They are vague but harmless, not separate findings.

# Technical consistency matrix — 220 questions

Legend: `C` consistent, `CS` consistent but specialized, `F` finding, `N/A` absent/outside Chapter 23.

```text
001 C  Is DataChunk the execution batch?
002 C  Does it contain capacity?
003 C  Does it contain cardinality?
004 C  Does it contain vector columns?
005 C  Does it own chunk-local StringHeap storage?
006 C  Do normal columns share cardinality?
007 C  Is cardinality bounded above by capacity?
008 F  Is zero capacity explicitly legal or forbidden?
009 C  Is cardinality zero legal?
010 C  Is a partial final chunk legal?
011 C  Is empty chunk distinct from EOS?
012 C  Does EOS use explicit runtime status?
013 C  Is 1024 the standard capacity?
014 C  Is 1024 nonpersistent?
015 C  Is the literal centralized?
016 C  Is capacity limited to 65535?
017 C  Can total results exceed one chunk?
018 C  Must large results split across chunks?
019 C  Are inactive positions semantically inaccessible?
020 CS Is exact allocation policy left to Chapter 24?

021 F  Is columns[] explicitly mapped to physical output schema?
022 F  Is LogicalSlotId lookup from a DataChunk fully specified?
023 C  Is physical column position nonpersistent?
024 C  Is vector position distinct from LogicalSlotId?
025 C  Is vector position distinct from BindingId?
026 C  Is vector position distinct from RID?
027 C  Is vector position distinct from row-occurrence identity?
028 C  Does logical position precede selection indirection?
029 C  Does selection determine the effective physical position?
030 C  Does repeated selection preserve repeated occurrences?
031 C  Does unsorted selection define its listed order?
032 C  Is row multiplicity independent of payload sharing?
033 C  Does chunk lane order alone fail to establish SQL order?
034 C  Can ordered providers preserve logical lane sequence?
035 C  Does chunk production sequence contribute to provided order?
036 C  Can unordered chunks be compared as bags?
037 C  Is PageNo order nonsemantic?
038 C  Is RID order nonsemantic?
039 C  Is pointer order nonsemantic?
040 C  Is worker order nonsemantic?

041 C  Is FLAT a v1 representation?
042 C  Is CONSTANT a v1 representation?
043 C  Is DICTIONARY a v1 representation?
044 C  Does FLAT use contiguous typed positions?
045 C  May FLAT reference rather than own storage?
046 C  Does CONSTANT repeat one scalar?
047 C  May CONSTANT repeat NULL?
048 C  Does CONSTANT cardinality remain DataChunk-owned?
049 C  Does DICTIONARY reference a child vector?
050 C  Does DICTIONARY carry one selection?
051 C  Does DICTIONARY avoid an independent value array?
052 C  Does DICTIONARY logical i map by selection?
053 C  Are SEQUENCE and RLE outside the v1 set?
054 F  Is that exclusion stated timelessly?
055 C  Is representation dispatch per vector/batch?
056 C  Is per-row kind dispatch avoided?
057 C  Is UnifiedVectorFormat a view?
058 C  Is it nonpersistent?
059 C  Does CONSTANT normalize to position zero?
060 C  Does DICTIONARY normalize selections?
061 C  May nested dictionaries exist before normalization?
062 C  Must hot-kernel effective indirection be bounded?
063 C  Does normalization preserve logical order?
064 C  Does normalization preserve validity?
065 C  Does normalization preserve underlying value identity?
066 C  May normalization avoid copying payload?
067 C  Are flat/constant/dictionary values intended equivalent?
068 C  Is representation choice nonsemantic?
069 F  Is a borrowed/base vector value-stable against mutation?
070 F  Is constant partial-write behavior defined?

071 C  Is validity separate from payload?
072 C  Does one bit represent one physical position?
073 C  Does bit one mean non-NULL?
074 C  Does bit zero mean NULL?
075 C  Are words uint64?
076 C  Is all_valid a fast path?
077 C  May kernels ignore words when all_valid?
078 C  Do inactive validity bits lack semantic effect?
079 C  Does CONSTANT repeat one validity state?
080 C  Does DICTIONARY use selected child validity?
081 C  Is NULL independent of integer payload?
082 C  Is NULL independent of FLOAT NaN?
083 C  Is NULL independent of StringRef pointer?
084 C  Is NULL VARCHAR allowed without bytes?
085 C  Is non-NULL empty VARCHAR distinct?
086 C  Can stale validity outside cardinality remain inaccessible?
087 C  Must left-null extension use NULL validity?
088 CS Is runtime validity bit order implementation-defined?
089 CS Is persisted tuple NULL polarity correctly distinct?
090 C  Can validity conversion preserve Chapter-17 NULL?

091 C  Does runtime BOOLEAN use one byte?
092 C  Does runtime INT32 use int32?
093 C  Does runtime INT64 use int64?
094 C  Does runtime FLOAT use binary64?
095 C  Does runtime DATE use int32?
096 C  Does runtime TIMESTAMP use int64?
097 C  Are these process-local execution representations?
098 C  Are they independent of persisted endianness?
099 C  Does Chapter 17 remain the type owner?
100 C  Is unresolved NULL excluded from vectors?
101 C  Is runtime NaN non-NULL unless validity says otherwise?
102 C  Are signed zeros still distinct bits where copied?
103 C  Do semantic comparisons equate signed zeros?
104 C  Do semantic comparisons equate all NaNs?
105 C  Is NaN payload irrelevant to grouping/order?
106 C  Does runtime BOOLEAN avoid bit packing?
107 F  Is “initial executor” wording timeless?
108 C  Are alignment details nonpersistent?
109 C  Is host ABI serialization forbidden upstream?
110 F  Is VARCHAR length above UINT32_MAX handled?
111 C  Is StringRef length exact bytes?
112 C  Is StringRef prefix explicitly defined?
113 C  Is StringRef data a runtime pointer?
114 C  Is no terminator required?
115 C  Is zero-length data non-dereferenced?

116 C  Are arbitrary VARCHAR bytes preserved?
117 C  Is embedded NUL preserved?
118 C  Is unsigned byte interpretation explicit?
119 C  Is prefix big-endian?
120 C  Are missing prefix bytes zero-filled?
121 C  Can differing prefix reject equality/order?
122 C  Must equal prefixes use full comparison?
123 C  Does prefix agree with binary collation?
124 C  Is StringRef valid only while owner lives?
125 C  Are chunk heaps valid owners?
126 C  Are query/stable constant stores valid owners?
127 C  Are RowCollections valid owners?
128 C  Is blocking storage a valid owner?
129 C  Is sort/run storage a valid owner?
130 C  Must StringRef not outlive an unpinned page?
131 C  Must it not outlive reset chunks?
132 C  Must it not outlive expression scratch?
133 C  Must it not outlive released run/block storage?
134 C  Must retention beyond input lifetime copy bytes?
135 C  Does scan copy page VARCHAR into chunk heap?
136 C  Can page guards release after copying?
137 C  Must chunk heap wait for borrowers before reset?
138 C  May streaming operators borrow synchronously?
139 C  Must blocking operators own retained data?
140 C  Must result/client boundaries retain safely?
141 C  Does Chapter 26 own pipeline enforcement?
142 F  Must borrowed storage remain immutable while borrowed?
143 F  Is dictionary selection storage lifetime explicit?
144 C  Is post-reset borrowed StringRef invalid?
145 C  Is copied StringRef valid under the new owner?

146 C  Are chunks intended reusable?
147 C  Must borrowers finish before output reset?
148 C  Does reset set cardinality to zero?
149 C  Does reset clear vector logical state?
150 C  Does reset reset StringHeap?
151 C  Does reset retain capacity allocations?
152 C  Are large reused buffers memory-accounted?
153 C  Are old positions inaccessible after reset?
154 C  Must new active positions be initialized?
155 CS Is that initialization requirement currently implicit?
156 C  May stale bytes remain outside cardinality?
157 C  Must stale bits not affect new cardinality?
158 C  Must stale selection not affect new cardinality?
159 C  Must old StringRefs not reappear after refill?
160 C  Can a smaller refill safely reuse a larger buffer?
161 F  Are input/output mutation and aliasing rules explicit?
162 F  Is dictionary-base mutation prohibited or defined?
163 F  Is constant-vector mutation flatten-on-write or forbidden?
164 C  Is copy-on-write implementation-specific?
165 N/A Are C++ move-source semantics architectural?

166 C  Are runtime vectors nonpersistent?
167 C  Is UnifiedVectorFormat nonpersistent?
168 C  Are raw StringRef pointers nonpersistent?
169 C  Are raw selection arrays nonpersistent?
170 C  Are runtime validity words nonpersistent?
171 C  Are tuple encodings Chapter-5-owned?
172 C  Are catalog scalar encodings §17.13-owned?
173 C  Are spill bytes Chapter-24/operator-owned?
174 C  Must spill reconstruct value ownership?
175 C  Must DML encode values rather than pointers?
176 C  Must result sinks retain client-visible strings?
177 C  Does QueryMemoryManager own large memory accounting?
178 C  Does QueryArena avoid unbounded data ownership?
179 F  Is uint32 StringRef exhaustion classified?
180 C  Is chunk capacity a runtime, not SQL, maximum?

181 C  Does Filter preserve selected occurrence order?
182 C  Does Filter allow zero-result empty chunks?
183 C  Does Project borrow simple columns when safe?
184 C  Do computed VARCHAR outputs own sufficient bytes?
185 C  Does SeqScan copy page VARCHAR?
186 C  Does IndexScan obey the same heap lifetime rule?
187 C  Do join duplicate selections preserve bags?
188 C  Does LEFT JOIN set right validity NULL?
189 C  Do aggregates retain owned state?
190 C  Does DISTINCT use semantic equality?
191 C  Does Sort own retained varlen bytes?
192 C  Does Sort preserve cross-chunk order?
193 C  Does Top-N chunking preserve order/Limit result?
194 C  Can PhysicalLimit trim cardinality/selection?
195 C  Can Limit emit a partial final chunk?
196 C  Can global aggregate emit one row?
197 C  Can grouped empty aggregate emit zero rows?
198 C  Do subquery materializations follow ordinary lifetime?
199 C  Does pipeline status—not empty chunk—end a source?
200 C  Must asynchronous queues materialize borrowed data?

201 C  Can vector width change result values?
202 C  Can vector width change NULL state?
203 C  Can vector width change bag multiplicity?
204 C  Can vector width change required order?
205 C  Can vector width select a different semantic error?
206 C  Can chunk boundaries change transaction state?
207 C  Can pointer addresses change comparison?
208 C  Can allocation order change result?
209 C  Can arena address change result?
210 C  Can storage location change VARCHAR equality?
211 C  Can representation kind change errors?
212 C  Can NaN payload bits change semantic grouping?
213 C  Can worker ownership change semantics?
214 F  Is “initial vector representations” timeless?
215 F  Is “later/deferred until measurement” timeless?
216 C  Is Chapter 23 free of test procedures?
217 C  Is it free of benchmark-result history?
218 C  Is it free of project-state capability tables?
219 C  Does normative language distinguish correctness/performance?
220 F  Can an implementer complete all lifetime/bounds policy without invention?
```

Status summary:

```text
CONSISTENT                 191
CONSISTENT BUT SPECIALIZED   5
FINDING                     23 question instances
N/A                          1
```

The 23 FINDING entries collapse into the six primary findings below rather than 23 independent defects.

# Implementer-invention assessment

The 60 requested implementer questions are answerable except:

- 8: exact `LogicalSlotId`/vector-column association;
- 12–15 only after defining selected active-domain bounds;
- 25–29 only after resolving large StringRef and mutation-stability details;
- 33–35: aliasing/base/constant mutation;
- 55–56: malformed selection/zero-capacity and exact validation outcome.

All other questions are answered by Chapter 23 plus the frozen owner handoffs. Correctness-relevant invention remains, so the chapter is not ready for document cleanup alone.

# Findings

## BLOCKING

### B23-1 — Selected-position active domain is undefined

- Section: [§§23.5–23.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19238)
- Evidence: index must be within “physical bounds,” but that term is not defined as allocated capacity or initialized active positions.
- Type: `SELECTION`
- Example: base capacity 1024, cardinality 1, dictionary index 7.
- Interpretations:
  - valid because 7 is within allocated capacity;
  - invalid because 7 is outside the active initialized domain.
- Consequence: stale value/NULL leakage or divergent rejection/value behavior.
- Owner/handoff: Chapter 23; consumed by Chapters 25–30.
- Smallest action: define the exact selectable domain for FLAT, CONSTANT, and composed DICTIONARY vectors, including validation and active-position rules.

### B23-2 — Borrowed views have lifetime but no value-stability contract

- Section: [§§23.10–23.12](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19395)
- Evidence: owner must remain alive and unreset, but mutation/overwrite while alive is not addressed.
- Type: `BORROWED LIFETIME`
- Example: dictionary/reference output exists while base vector storage remains allocated but is overwritten before consumption.
- Interpretations:
  - borrowed output is a stable snapshot;
  - borrowed output is a live view of mutable storage.
- Consequence: different SQL values, NULLs, order, or dangling selection semantics.
- Owner/handoff: Chapter 23→25/26.
- Smallest action: require value/validity/selection stability for the borrow interval, or define another exact snapshot/materialization rule.

### B23-3 — StringRef’s uint32 length has no complete semantic/resource boundary

- Section: [§23.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19346)
- Evidence: StringRef length is fixed to uint32 while Chapter 17 admits arbitrary finite bytes representable under owner resource limits.
- Type: `RESOURCE SEMANTICS`
- Valid input: a valid VARCHAR value whose mathematical byte length exceeds `UINT32_MAX`.
- Interpretations:
  - uint32 is a frozen execution representability limit with controlled resource failure;
  - another exact runtime representation may be selected;
  - query is incorrectly treated as invalid/truncated.
- Consequence: success versus public resource error, or truncation if implemented incorrectly.
- Owner/handoff: Chapter 17→23→24/39.
- Smallest action: define exact representability/fallback and error classification; forbid truncation and SQL-domain reinterpretation.

## MAJOR

### M23-1 — Zero-capacity legality and progress are undefined

- Section: [§23.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19118)
- Evidence: only `capacity <= 65535` and `0 <= cardinality <= capacity` are stated.
- Type: `CAPACITY`
- Competing outcomes: capacity zero accepted as a valid empty container versus rejected as an invalid execution batch.
- Consequence: a source can be unable to make progress while returning non-EOS empty batches.
- Smallest action: define a positive minimum for executable chunks or an exact restricted zero-capacity use.

### M23-2 — Physical output schema to vector-column mapping is not explicit

- Section: [§23.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19118)
- Evidence: DataChunk has `Vector columns[]`, while Chapter 22 owns output schema/LogicalSlotIds, but no positional or explicit-map handoff is stated.
- Type: `SLOT MAPPING`
- Consequence: implementers must invent how duplicate projections, self joins, derived remaps, and reordered Projects locate values.
- Frozen constraint: physical position is not semantic identity.
- Smallest action: state a representation-neutral conformance rule tying each chunk column to the physical output schema’s exact `LogicalSlotId`.

## MINOR

### N23-1 — Project-time/current-generation wording

- Sections: §23.3 and §23.4
- Evidence: “initial vector representations,” “Later representations,” “deferred until measurement,” “initial executor.”
- Type: `TEMPORALITY`
- Consequence: Chapter cannot stand as a timeless v1 snapshot.
- Smallest action: replace with durable v1 inclusion/exclusion and byte-per-BOOLEAN statements.

## EDITORIAL

None.

# Frozen Chapter-23 semantic questions

## Q23-1 — Selection target domain

- Exact section: §23.6, composed through §23.7.
- Valid state: capacity exceeds cardinality and selection names an allocated inactive slot.
- Competing interpretations: capacity-bounded versus active-value-bounded.
- Consequence: stale value/NULL versus rejection.
- Frozen constraints: Chapter-20 bags/order; Chapter-22 lane independence.
- Decision required: exact per-representation selectable domain and validation rule.

## Q23-2 — Borrowed view stability

- Exact section: §§23.10–23.12.
- Valid state: base owner alive but storage mutates before borrower completes.
- Competing interpretations: immutable snapshot versus live mutable view.
- Consequence: divergent values/NULL/order and possible lifetime misuse.
- Frozen constraints: Chapter-17 values; Chapter-20 occurrences; Chapter-22 runtime isolation.
- Decision required: stability/immutability or exact materialization rule for the borrow interval.

## Q23-3 — VARCHAR larger than StringRef length domain

- Exact section: §23.9.
- Valid input: Chapter-17 VARCHAR with length greater than `UINT32_MAX`.
- Competing interpretations: controlled execution resource failure versus alternate exact representation.
- Consequence: success/error difference; truncation must never be legal.
- Frozen constraints: Chapter-17 byte domain and Chapter-24/39 resource ownership.
- Decision required: exact representability policy and error category.

## Q23-4 — Zero-capacity chunk

- Exact section: §23.1.
- Valid state under current inequalities: capacity/cardinality both zero.
- Competing interpretations: legal container/batch versus invalid execution chunk.
- Consequence: pipeline progress/nontermination risk.
- Frozen constraints: empty chunk is not EOS; Chapter 26 statuses are separate.
- Decision required: positive minimum or restricted zero-capacity semantics.

# Verification gaps

| Architecture owner | Status | Missing/partial methodology | Reusable oracle | Blocked? |
|---|---|---|---|---|
| §23.1 DataChunk bounds | PARTIAL | capacity 0/1/max, custom capacities, cardinality overflow | declarative chunk model | Q23-4 |
| §§23.3–23.8 representations | PARTIAL | duplicate/unsorted selection and exact active domain | scalar vector oracle | Q23-1 |
| §23.5 validity | PARTIAL | malformed mask/all_valid, stale validity after reuse | independent validity model | no |
| §23.9 StringRef | MISSING | exact prefix, empty/embedded NUL, large-length boundary | byte-sequence oracle | Q23-3 |
| §§23.10–23.12 lifetime | PARTIAL | owner matrix, selection lifetime, mutation stability | allocation poisoning/owner graph | Q23-2 |
| §23.13 reuse | MISSING | large→small refill, stale selection/validity/StringRefs | generation/poisoned-storage oracle | no |
| runtime/persistence boundary | PARTIAL | negative raw-pointer serialization registry | persistent-format registry | no |
| aliasing/mutability | MISSING | constant/dictionary/input-output mutation cases | immutable semantic snapshot | Q23-2 |
| width/chunk determinism | PARTIAL | full capacity/boundary/error perturbation | V20/V22 semantic oracle | no |
| malformed runtime states | MISSING | direct invalid vector corpus and classification | declarative vector validator | Q23-1/Q23-4 |

Existing Verification is valuable but Chapter 23 is not synchronized.

# Final assessments

- Project chronology? **Yes — four phrase occurrences.**
- Current implementation narration? **Yes — “initial executor.”**
- DEVELOPMENT-owned sequencing? **Yes, limited to SEQUENCE/RLE “later/deferred until measurement.”**
- VERIFICATION procedure? **No.**
- PROJECT_STATE leakage? **No separate capability report, but “initial executor” is current-generation narration.**
- History/devlog leakage? **No.**
- DataChunk ambiguity? **Yes.**
- Capacity/cardinality ambiguity? **Yes, zero capacity.**
- Empty/EOS ambiguity? **No.**
- Logical/physical-position ambiguity? **Yes, selection active domain.**
- LogicalSlotId mapping ambiguity? **Yes.**
- Vector representation inventory ambiguity? **No.**
- Representation-equivalence ambiguity? **No for read-only values; mutation stability unresolved.**
- Selection ambiguity? **Yes.**
- Dictionary ambiguity? **Yes, inherited bounds and mutation stability.**
- Validity ambiguity? **No for valid immutable input.**
- NULL ambiguity? **No.**
- FLOAT ambiguity? **No.**
- VARCHAR ambiguity? **Yes, large length.**
- StringRef lifetime ambiguity? **Yes, value stability rather than owner liveness.**
- Borrowed/owned ambiguity? **Yes.**
- Reset/reuse ambiguity? **No material semantic gap once borrowers finish; initialization is implicit.**
- Aliasing/mutability ambiguity? **Yes.**
- Deep-copy ambiguity? **No for retention boundaries.**
- Runtime/persistent boundary ambiguity? **No.**
- Memory-owner ambiguity? **No major ambiguity; selection ownership wording is thin.**
- Chunk-boundary determinism ambiguity? **No.**
- Vector-width determinism ambiguity? **No.**
- Lane-error ambiguity? **No; downstream owner.**
- Correctness-relevant implementer invention? **Yes.**
- Can Chapter 23 stand years later as canonical v1 Architecture? **No.**

# Next action

**Frozen Chapter-23 semantic review / decision package** covering Q23-1 through Q23-4.

Do not perform document cleanup or Verification synchronization first.

After decisions and semantic integration:

1. targeted document cleanup for M23-2/N23-1 and any decision-related wording;
2. Chapter-23 Verification synchronization;
3. only then declare Chapter 23 fully reviewed and closed;
4. then conduct Chapter-24 direct read-only review.

Chapter-24 review: **NOT STARTED**.
Verification synchronization: **NOT PERFORMED**.
Implementation/build/test/benchmark: **NONE**.
Phase 2: **NOT STARTED / NOT AUTHORIZED**.

# Q23-1–Q23-4 — DECISION PACKAGE COMPLETE

The four questions can be resolved independently as D23-S1 through D23-S4. No additional frozen semantic question was discovered. These are recommendations awaiting architecture-owner approval; nothing has been frozen or edited.

## Repository state

| Check | Initial | Final |
|---|---|---|
| HEAD | `02d2fbcedc8624df1a1e5ad8f1c4db46bf607eb2` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | — | passed |
| Task-created changes | none | none |

Historical review artifacts were not read, modified, moved, or staged.

## Sections analyzed

- [Architecture front matter](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1)
- §§17.1–17.4, 17.11–17.13: scalar domains, VARCHAR, owned `Value`, persistence.
- §§20.1–20.2: bag occurrences and logical identity.
- §§22.1–22.8: physical schema, runtime ownership, lane/representation independence.
- [Chapter 23 in full](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19116), especially §§23.1, 23.3, 23.5–23.14.
- §§24.4–24.10: query memory, allocation failure, resource ownership.
- §§25.1–25.8: active selection, normalization, result ownership.
- §§26.4, 26.6, 26.10: source status, borrowed-data lifetime, pipeline invariants.
- §§27.1–27.2: scan output and page-backed VARCHAR copying.
- §§39.2–39.4: front-end, execution, resource, and internal errors.
- Verification’s Vector Correctness, String Lifetime, pipeline cleanup, and Chapter-22 vector-independence procedures.

Frozen constraints include exact Chapter-17 values, Chapter-20 bags, Chapter-22 physical-schema and lane independence, explicit runtime/persistence separation, and §39’s distinction between `ExecutionError` and actual `OutOfMemory`.

## Combined alternatives

| Question | Alternative | Compatibility and safety | Complexity/freedom | Public-error effect | Recommend? |
|---|---|---|---|---|---|
| Q23-1 | Capacity-bounded selection | Unsafe without another initialization domain; may expose stale slots | Simple but incorrect under reuse | Internal defects could become values | No |
| Q23-1 | Active-logical-domain selection | Preserves initialized values, CONSTANT semantics, composition | Representation-independent | Invalid state remains internal | **Yes** |
| Q23-1 | Separate initialized domain | Can be safe, but creates another state dimension | More validation and bookkeeping | Internal only | No |
| Q23-2 | Live mutable view | Allows observable values to change after publication | Simple but semantically unsafe | Could alter results/errors | No |
| Q23-2 | Value-stable borrow | Safe with zero-copy, pinning, COW, transfer, or copying | Maximum useful mechanism freedom | Only real resource failures remain | **Yes** |
| Q23-2 | Mandatory deep copy | Safe | Unnecessarily prohibits zero-copy | More possible OOM | No |
| Q23-3 | Mandatory `UINT32_MAX` bound | Deterministic but unnecessarily closes exact alternatives | Preserves compact ABI only | Controlled representability error | Acceptable, not preferred |
| Q23-3 | Mandatory alternate representation | Exact but requires an undefined large-string design | Large v1 expansion | Avoids representability error | No |
| Q23-3 | Exact alternate-or-resource failure | Preserves type domain and exact implementations | Best implementation freedom | Controlled `ExecutionError` when unavailable | **Yes** |
| Q23-3 | Truncation/wraparound | Violates Chapter 17 | Incorrect | Silent semantic corruption | Never |
| Q23-4 | Positive executable capacity | Simple, structurally progress-capable | No API overconstraint | Invalid executable state is internal | **Yes** |
| Q23-4 | Zero only outside executable state | Compatible complement to positive execution rule | Preserves default/moved-from freedom | Internal if emitted | **Yes, as part of D23-S4** |
| Q23-4 | Executable zero with special progress rules | Adds no useful execution capability | Unnecessary complexity | Risks endless non-EOS batches | No |

# Q23-1 — Selection target domain

## Analysis

For a FLAT vector, capacity says how many slots are allocated, not how many contain active logical values. A selection bounded only by capacity can expose stale payload or validity after reset or partial fill.

For CONSTANT, logical and physical domains differ deliberately. A CONSTANT child with cardinality 10 has logical positions `0..9`, all resolving to physical payload slot 0. Therefore “selected index less than physical payload length” is wrong.

For DICTIONARY, each index addresses an active logical position of its immediate child. Resolution then follows the child representation. Nested dictionaries validate every intermediate logical index; flattening cannot legalize an invalid intermediate reference.

Repeated and unsorted selections remain legal. They create distinct occurrences in the listed order and preserve Chapter-20 bag semantics.

Every active logical position must resolve to fully initialized payload and validity state. Allocated inactive capacity is semantically inaccessible.

## Selection-domain matrix

| Case | Logical-domain validity | Effective payload | Allowed? | Classification | Stale-data risk |
|---|---|---|---:|---|---|
| FLAT active `i < C` | valid | physical slot `i` | yes | conforming | none |
| FLAT inactive `C <= i < P` | invalid | must not resolve | no | malformed internal representation | high if capacity-bounded |
| CONSTANT active `i < C` | valid | scalar slot 0 | yes | conforming | none |
| CONSTANT with `C=0` | no logical positions | none | no selection allowed | malformed if selected | none if checked |
| DICTIONARY active child position | valid | representation-specific child resolution | yes | conforming | none |
| DICTIONARY inactive child position | invalid | must not resolve | no | malformed internal representation | high |
| Nested dictionary, all bounds valid | valid at every level | composed base position | yes | conforming | none |
| Nested dictionary, invalid intermediate | invalid | must not normalize/dereference | no | malformed internal representation | high |
| Repeated selection `[2,2]` | valid if `2<C` | same payload twice | yes | two logical occurrences | none |
| Unsorted selection `[2,0,1]` | valid if each `<C` | corresponding payloads | yes | listed order preserved | none |

An out-of-domain selection is an internal runtime-representation defect. It must be prevented or rejected before payload or validity access. It is not a public SQL error and must never fall back to stale data.

## Recommended D23-S1

**D23-S1 — Active logical selection domain**

### Proposed normative wording

```text
A SelectionVector indexes active logical positions of its immediate child
vector or child view. It does not index arbitrary allocated-capacity slots.

For every selected index:

    0 <= index < child logical cardinality

MUST hold for the immediate child view.

Resolution from a valid child logical position to payload storage is
representation-specific:

    FLAT:
        logical position i resolves to the fully initialized physical slot i;

    CONSTANT:
        every valid logical position resolves to scalar payload position 0;

    DICTIONARY:
        the logical position resolves through that dictionary's SelectionVector
        and immediate child.

Nested dictionary composition MUST validate every intermediate logical index.
Normalization or flattening MUST NOT make an out-of-domain intermediate
selection valid merely because an allocated slot exists.

Repeated and unsorted selected indices are legal and preserve their listed
logical occurrence order and multiplicity.

Every active logical position MUST resolve to fully initialized value and
validity state. Allocated but inactive capacity is semantically inaccessible
and MUST NOT be selected.

An out-of-domain selection is an invalid internal runtime representation. It
MUST be prevented or rejected before payload or validity access and MUST NOT be
reported as a public SQL error.
```

# Q23-2 — Borrowed-view value stability

## Analysis

Owner liveness and value stability are separate. Keeping a chunk allocated does not make a borrow safe if the producer overwrites its payload, validity, selection, dictionary base, `StringRef`, or referenced bytes.

The minimum stable surface is everything required to resolve the borrowed logical values:

- active logical cardinality;
- representation/type metadata;
- payload reachable by the view;
- validity state;
- selection entries;
- dictionary-child/base relationship;
- backing addresses/ranges required by the view;
- `StringRef` length, prefix, and data reference;
- referenced VARCHAR bytes.

Capacity itself need not be semantically frozen if changing it cannot invalidate any required address, range, mapping, or value. Reallocation that invalidates a borrowed address is prohibited unless the borrower is safely rebound through an equivalent stable mechanism.

## Borrow-stability matrix

| Mutation/action during live borrow | Permitted? | Reason |
|---|---:|---|
| Reachable FLAT payload mutation | no | changes borrowed SQL value |
| Reachable validity mutation | no | changes NULL state |
| Selection entry mutation | no | changes resolved occurrence/value/order |
| Reachable dictionary-base mutation | no | changes borrowed logical view |
| `StringRef` pointer/length/prefix mutation | no | changes or invalidates represented bytes |
| Referenced string-byte mutation | no | changes VARCHAR value |
| Reset/reuse of reachable owner | no | invalidates view |
| Mutation of unreachable inactive storage | yes | cannot affect borrower |
| Copy/materialization before mutation | yes | borrower obtains independent stable owner |
| Ownership transfer | yes | transferred owner preserves lifetime/stability |
| Copy-on-write | yes | live borrower retains original view |
| Delayed mutation | yes | mutation begins after borrow ends |
| Storage relocation | only if observationally identical and references remain valid | address/lifetime contract must survive |

A CONSTANT shared scalar cannot be independently changed for logical row 3 while remaining CONSTANT. A write that makes occurrences diverge must first establish a representation capable of representing the distinct values.

A live DICTIONARY view likewise requires stable selection storage and stable reachable base values. Input/output aliasing remains allowed when all live aliases observe the same stable values.

Synchronous zero-copy pass-through remains valid. Asynchronous retention requires ownership retention or materialization, matching Chapter 26. Page-backed strings remain copied into the chunk heap under §27.2; this decision does not authorize raw page borrowing.

## Recommended D23-S2

**D23-S2 — Value-stable borrowing**

### Proposed normative wording

```text
Every borrowed, reference, dictionary, or selection-backed runtime view denotes
a value-stable logical view for its declared borrow interval.

All backing state required to resolve that view MUST remain alive and
observationally unchanged for the interval. This includes, where applicable:

    active logical cardinality;
    type and representation metadata;
    reachable scalar payload;
    validity state;
    selection storage and indices;
    dictionary child/base relationships;
    backing addresses and ranges;
    StringRef length, prefix, and data reference;
    referenced VARCHAR bytes.

An owner MUST NOT reset, reuse, overwrite, reallocate incompatibly, or otherwise
mutate reachable backing state while a live borrower depends on it.

Inactive storage that is unreachable from the borrowed logical view MAY change.

A conforming implementation MAY preserve the required stability by retaining
immutable backing storage, delaying mutation, transferring ownership,
copy-on-write, materializing, deep-copying, pinning under an already defined
owner contract, or another exact mechanism. No particular mechanism is
mandated.

Any write that would make one logical occurrence diverge from a shared
CONSTANT or DICTIONARY representation MUST first establish storage or a
representation capable of preserving every live logical view.

Retention beyond the declared borrow interval requires a new valid owner or
materialization under the existing retention rules. Failure to allocate memory
required for such materialization remains an ordinary resource failure.
```

This uses “value-stable logical view,” avoiding confusion with MVCC transaction snapshots.

# Q23-3 — VARCHAR beyond `uint32` StringRef length

## Analysis

Chapter 17 defines VARCHAR as arbitrary finite bytes subject to the owning row or execution resource limits. `StringRef.length` is an exact `uint32`, so the compact form cannot represent lengths above `UINT32_MAX`.

Ownership or deep copying cannot fix the width problem. The heap tuple limit is not a universal runtime VARCHAR limit; intermediate expressions and query-owned values are a separate domain.

The error taxonomy supports:

- `ExecutionError`: controlled execution failure umbrella;
- `OutOfMemory`: actual inability to obtain required memory;
- `FrontEndResourceLimit`: front-end only and therefore wrong here.

There is no existing named `ValueTooLarge` category. Q23-3 itself can resolve this by freezing a controlled `ExecutionError` with a runtime value-representability/resource cause. This does not require a separate fifth semantic question. `OutOfMemory` remains reserved for actual allocation failure.

## Alternatives

**A — Canonical `UINT32_MAX` runtime bound.** Consistent with Chapter 17’s resource qualification, simple, and deterministic, but permanently prohibits exact larger runtime representations.

**B — Mandatory alternate exact representation.** Preserves all finite values but introduces an unspecified large-string representation and broad downstream changes. This is too large for the v1 contract without defining the representation.

**C — Exact alternate representation when available; otherwise controlled representability failure.** Preserves the SQL domain, permits compact implementations, and allows exact larger representations without mandating one. Resource-capability differences may affect whether an enormous value can be materialized, which Chapter 17 already permits through owner resource limits.

Within one implementation/capability configuration, representation selection must not create arbitrary plan-dependent behavior: if an exact supported representation is available, a compact-only realization is inapplicable for that value.

## Large-VARCHAR matrix

| Exact length | Compact `StringRef` | Alternate exact representation | Required result |
|---:|---:|---|---|
| `0` | exact | unnecessary | valid non-NULL empty string or NULL according to validity |
| `1` | exact | optional | exact byte |
| `UINT32_MAX-1` | exact | optional | exact bytes |
| `UINT32_MAX` | exact | optional | exact bytes |
| `UINT32_MAX+1` | impossible | use if supported | exact value or controlled representability failure |
| Much larger finite length | impossible | use if supported | exact value or controlled representability failure |
| Alternate available | compact inapplicable above max | exact | operation may succeed |
| Alternate unavailable | impossible above max | none | controlled `ExecutionError` representability/resource cause |
| Allocation fails | irrelevant | cannot materialize | `OutOfMemory` |
| Truncation/wrap attempt | forbidden | forbidden | nonconforming implementation |

The success/resource-failure boundary may vary with advertised exact runtime capability and configured resource limits, but never merely because a planner selected an incapable compact representation while an exact supported form was available.

## Recommended D23-S3

**D23-S3 — Exact StringRef representability**

### Proposed normative wording

```text
The Chapter-23 StringRef form is an exact compact runtime representation for a
VARCHAR value whose exact byte length is at most UINT32_MAX.

A VARCHAR value MUST NOT be truncated, wrapped, split semantically, treated as
NUL-terminated, or otherwise reinterpreted merely to fit StringRef.

An implementation MAY provide another exact runtime representation for a
VARCHAR value whose byte length exceeds UINT32_MAX, provided it preserves all
Chapter-17 byte-string semantics and all Chapter-23 ownership and lifetime
requirements.

When an exact alternate representation is supported and available, a
StringRef-only physical realization is inapplicable to an over-domain value;
representation selection MUST NOT cause failure merely by choosing an
incapable compact form.

If no exact runtime representation is available, an attempt to materialize
such a value fails as a controlled ExecutionError with a runtime
value-representability/resource-limit cause. It is not a parser, type, cast,
corruption, or invalid-VARCHAR error.

OutOfMemory remains reserved for failure to obtain memory required by an
otherwise representable exact form.

This runtime representability/resource boundary does not narrow the Chapter-17
SQL VARCHAR value domain and does not change any persistent tuple, catalog,
statistics, spill, or WAL format.
```

# Q23-4 — Zero-capacity DataChunk

## Analysis

An executable zero-capacity chunk can never carry a row and, because empty chunk is not EOS, can participate in an endless no-progress stream. There is no execution benefit sufficient to justify that state.

Architecture need not prohibit a default, moved-from, or bookkeeping object with zero capacity. It only needs to exclude it from executable batch state.

Positive-capacity empty chunks remain legal. To close the progress aspect fully, an empty `HAVE_MORE` result must advance finite input/operator state; endlessly returning an empty batch cannot substitute for `FINISHED`.

## Capacity matrix

| Capacity | Executable? | Allowed cardinality | Empty legal? | EOS relation | Progress implication |
|---:|---:|---|---:|---|---|
| `0` | no | none in executable state | only as non-executable bookkeeping | none | cannot be emitted/consumed |
| `1` | yes | `0..1` | yes | empty is not EOS | can carry progress |
| `1024` | yes, standard | `0..1024` | yes | empty is not EOS | ordinary standard batch |
| `65535` | yes, maximum | `0..65535` | yes | empty is not EOS | final legal capacity |
| `65536` | no | invalid | no | none | cannot be addressed by v1 selection |

## Recommended D23-S4

**D23-S4 — Positive executable chunk capacity**

### Proposed normative wording

```text
Every DataChunk participating in operator execution MUST satisfy:

    1 <= capacity <= 65535
    0 <= cardinality <= capacity

cardinality = 0 remains an ordinary empty batch and MUST NOT represent
end-of-stream. End-of-stream remains the separate source status.

A zero-capacity container MAY exist as implementation-specific uninitialized,
default, moved-from, or bookkeeping state, but it is outside the executable
DataChunk state and MUST NOT be emitted or consumed as a normal execution
batch.

An operator that returns HAVE_MORE with an empty executable chunk MUST
nevertheless advance finite input or operator state. It MUST NOT emit an
unbounded sequence of empty batches as a substitute for FINISHED.

STANDARD_VECTOR_SIZE remains 1024. Implementations may use other positive
capacities within the v1 maximum where permitted; constructor and allocation
mechanics are not prescribed.
```

# Cross-decision results

## Interaction matrix

| Interaction | Result |
|---|---|
| D23-S1 + D23-S2 | Selected mapping, child domain, payload, and validity are both bounded and value-stable. |
| D23-S1 + D23-S4 | A positive-capacity empty chunk has zero selectable positions; capacity positivity does not legalize inactive slots. |
| D23-S2 + reset/reuse | Reachable backing state cannot reset or mutate until borrowers finish or obtain independent ownership. |
| D23-S2 + asynchronous queues | Retain owner or materialize; recycled borrowed chunks cannot be queued. |
| D23-S2 + page strings | Existing page-to-chunk copy remains mandatory. |
| D23-S2 + D23-S3 | Ownership and length representability remain separate; copying cannot widen `uint32`. |
| D23-S3 + persistence | No persistent-format changes. |
| D23-S3 + result sink | Exact representation or the same controlled resource/representability failure; no truncation. |
| D23-S4 + Chapter 26 | Explicit EOS remains unchanged; empty `HAVE_MORE` must make state progress. |

The dependency graph is four independent decisions:

```text
D23-S1 ─┐
         ├─ compose for stable dictionary resolution
D23-S2 ─┘

D23-S3    independent runtime value-representability/resource rule

D23-S4    independent executable-batch/progress rule
```

They should remain four D23-S decisions because their invariants, error consequences, and integration surfaces are distinct.

## Error/resource taxonomy

| Condition | Classification | Public SQL semantic error? | Required boundary |
|---|---|---:|---|
| Selection outside active child domain | internal invalid runtime representation | no | prevent/reject before dereference |
| Expired borrow | internal lifetime invariant violation/nonconforming state | no | prevent before access |
| Mutation violating live borrow | internal lifetime/aliasing invariant violation | no | prevent or preserve equivalent view |
| Oversized VARCHAR with no exact form | controlled `ExecutionError`, runtime representability/resource cause | no semantic/type error | fail without truncation |
| Required allocation fails | `OutOfMemory` | operational resource error | existing §§24/39 handling |
| Executable zero-capacity chunk | internal invalid runtime state | no | reject before execution consumption |
| Endless empty `HAVE_MORE` without state progress | invalid pipeline implementation | no | pipeline progress invariant |

Malformed selection, borrow, and chunk states must not be consumed far enough to cause out-of-bounds access, stale values, arbitrary writes, or persistent corruption. Construction invariants or boundary validation may enforce this; universal per-row validation is not mandated.

If a controlled Q23-S3 failure occurs during a statement that already published database writes, existing §39.1 transaction consequences apply. No new rollback policy is introduced.

## Impact assessment

- Persistence: none. Page, tuple, catalog, statistics, spill, WAL, control, and RID formats remain unchanged.
- Transactions: none. TxnId, CommandId, MVCC snapshots, attempts, D21 rules, publication, and rollback behavior remain unchanged.
- Logical identity: none. M23-2 remains the document-only need to state the existing Chapter-22 slot-to-column handoff.
- Implementation freedom: preserved for selection integer implementation, normalization, pinning, copying, COW, ownership transfer, alternate exact string forms, chunk constructors, and positive custom capacities.
- Timelessness: all proposed wording states final v1 contracts without roadmap or implementation-status language.

# Future integration and verification

## Semantic integration surfaces

| Decision | Smallest likely integration surface |
|---|---|
| D23-S1 | §§23.3, 23.6–23.8, 23.14 |
| D23-S2 | §§23.3, 23.10, 23.12–23.14; §26.6 synchronization only where needed |
| D23-S3 | §§23.9, 23.14; §39.3 for the explicit `ExecutionError` representability cause |
| D23-S4 | §§23.1, 23.14; §§26.4/26.10 for empty-batch progress |

Chapter 24 needs no semantic change unless integration chooses to add a non-normative pointer to its existing allocation accounting. It remains unreviewed.

## Decision-dependent verification

| Decision | Future methodology |
|---|---|
| D23-S1 | FLAT/CONSTANT/DICTIONARY active bounds; inactive-slot poisoning; repeated/unsorted indices; nested invalid intermediate; malformed-selection corpus |
| D23-S2 | payload/validity/selection/base/StringRef-byte poisoning; mutation-before-consume; reset/reuse; safe transfer/copy/COW; asynchronous retention |
| D23-S3 | lengths `0`, `UINT32_MAX-1`, `UINT32_MAX`, symbolic `UINT32_MAX+1`; exact alternate capability; no truncation/wrap; representability vs OOM classification |
| D23-S4 | capacities `0`, `1`, `1024`, `65535`, `65536`; empty positive-capacity non-EOS; state-progress and endless-empty rejection |

No Verification file was edited.

# Reread answers 1–86

1. Yes—capacity is distinct from the active logical domain.
2. Yes—inactive allocated slots are semantically inaccessible.
3. Yes—a SelectionVector indexes immediate-child logical positions.
4. Yes—the immediate child’s active cardinality is the bound.
5. Yes—active FLAT position `i` maps to initialized slot `i`.
6. Yes—every active CONSTANT position resolves to payload 0.
7. Yes—a CONSTANT logical index greater than zero is valid when below cardinality.
8. Yes—DICTIONARY selection addresses child logical positions.
9. Yes—every nested intermediate index is validated.
10. Yes—repeated indices are legal.
11. Yes—unsorted indices are legal.
12. Yes—they preserve listed occurrence order.
13. Yes—an index at least child cardinality is invalid.
14. Yes—allocated but inactive selection is invalid.
15. Yes—invalid selection is internal, not a SQL error.
16. No—an invalid selection cannot be dereferenced.
17. Yes—D23-S1 prevents inactive stale-value leakage.
18. Yes—owner lifetime and value stability are distinct.
19. Yes—reachable payload must remain stable.
20. Yes—reachable validity must remain stable.
21. Yes—selection state must remain stable.
22. Yes—`StringRef` metadata must remain stable.
23. Yes—referenced bytes must remain stable.
24. Yes—required representation metadata must remain stable.
25. No—reachable backing state cannot be overwritten unless equivalent stable semantics are preserved.
26. Yes—unreachable inactive storage may mutate.
27. Yes—the implementation may deep-copy.
28. Yes—it may transfer ownership.
29. Yes—it may use copy-on-write.
30. Yes—it may delay mutation.
31. No single mechanism is mandated.
32. Yes—stable borrowing resolves dictionary-base mutation.
33. Yes—diverging CONSTANT writes require a capable representation first.
34. Yes—input/output aliasing is safe only under stable-view rules.
35. Yes—selection storage is included in backing state.
36. Yes—reset/reuse waits for borrowers or independent ownership.
37. Yes—asynchronous retention requires retention/materialization.
38. Yes—zero-copy synchronous pass-through remains allowed.
39. Yes—transaction-snapshot terminology is avoided.
40. Yes—Chapter 17 retains arbitrary finite bytes under owner resource limits.
41. Yes—compact `StringRef` has an exact `uint32` length field.
42. Yes—a semantic VARCHAR may exceed `UINT32_MAX` subject to resources.
43. No—truncation is never allowed.
44. No—wraparound is never allowed.
45. No—the heap tuple maximum is not a universal runtime VARCHAR maximum.
46. No—ownership/deep copy does not solve length representability.
47. Yes—an exact alternate runtime representation is architecturally possible.
48. No—every implementation need not provide one under the recommendation.
49. A controlled `ExecutionError` with runtime representability/resource cause occurs when no exact form exists.
50. Yes—it is a representability/resource failure.
51. Yes—the SQL VARCHAR domain remains unchanged.
52. Yes—persistent formats remain unchanged.
53. Yes—the exact §39 umbrella is `ExecutionError`; actual allocation failure remains `OutOfMemory`.
54. No, except explicitly permitted differences in exact runtime capability or real resource availability.
55. Yes—the proposed rule forbids truncation and wraparound.
56. Yes—it permits alternate exact representations.
57. Yes—zero cardinality remains legal.
58. Yes—zero cardinality remains distinct from EOS.
59. Yes—executable capacity must be positive.
60. Yes—capacity 1 is legal.
61. Yes—capacity 1024 is legal.
62. Yes—capacity 65535 is legal.
63. No—capacity 65536 is invalid.
64. Yes—a zero-capacity non-executable bookkeeping state may exist.
65. No—it cannot be emitted as a normal execution batch.
66. Yes—structural zero-capacity stalling is eliminated; empty `HAVE_MORE` must advance state.
67. Yes—constructor and moved-from mechanics remain implementation-specific.
68. No persistence change.
69. No transaction change.
70. No LogicalSlotId semantic change.
71. Yes—M23-2 remains document-only.
72. Yes—N23-1 remains document-only.
73. No Verification edit.
74. No new semantic question.
75. Yes—the decisions can be frozen independently.
76. Yes—all four recommendations are implementation-independent.
77. Yes—they preserve Chapter-17 values.
78. Yes—they preserve Chapter-20 bags.
79. Yes—they preserve Chapter-22 lane/representation independence.
80. Yes—they prevent stale or uninitialized observation.
81. Yes—they resolve dangling/live-mutation ambiguity.
82. Yes—they prevent silent VARCHAR truncation.
83. Yes—they define a positive executable capacity and empty-batch progress domain.
84. Yes—Chapter 23 is ready for semantic integration after owner approval.
85. Yes—Chapter 24 remains unreviewed.
86. Yes—Phase 2 remains unauthorized.

# Approval package and status

Recommended owner-approval package:

- **D23-S1:** Active logical selection domain.
- **D23-S2:** Value-stable borrowing.
- **D23-S3:** Exact compact StringRef with optional exact alternate representation, otherwise controlled `ExecutionError` representability/resource failure.
- **D23-S4:** Positive executable chunk capacity and state-progress requirement for empty non-EOS batches.

No new frozen Chapter-23 semantic question was discovered.

- M23-2: **OPEN / DOCUMENT-ONLY**
- N23-1: **OPEN / DOCUMENT-ONLY**
- Chapter 23: **SEMANTIC DECISIONS PENDING ARCHITECTURE-OWNER APPROVAL; NOT CLEAN; NOT FULLY CLOSED**
- Recommended next action after approval: **CHAPTER-23 SEMANTIC INTEGRATION**
- Chapter 24 review: **NOT STARTED**
- Verification synchronization: not performed.
- Implementation/build/tests/benchmarks: none.
- Phase 2: **NOT STARTED / NOT AUTHORIZED**.
