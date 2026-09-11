# Chapter 29 architecture review

## 1. Verdict

**BLOCKED ON SEMANTIC QUESTIONS**

Chapter 29 is numerically rigorous and broadly consistent with Chapters 17–28, but it leaves two correctness-observable representative-selection rules undefined:

- **N29-1:** canonical output representative for a `GROUP BY` equivalence class containing `-0.0` and `+0.0`, or multiple NaN encodings.
- **N29-2:** canonical surviving row/value for a `DISTINCT` equivalence class containing those representations.

Two additional major execution-contract clarifications and one minor chronology cleanup are recommended. The exact FLOAT64 SUM/AVG policy is internally coherent but is recorded as one design-scope question because it is unusually demanding for v1.

Counts:

| Category | Count |
|---|---:|
| BLOCKING | 2 |
| MAJOR | 2 |
| MINOR | 1 |
| EDITORIAL | 0 |
| DESIGN-SCOPE QUESTION | 1 |
| FROZEN SEMANTIC QUESTION | 2 |

## 2. Repository state

### Initial state

- `git status --short`: clean
- staged paths: none
- HEAD: `49d89edb5d35a84d20d6d5880a118571134b0984`
- subject: `synced VERIFICATION after chapter 28 ARCHITECTURE fix`

### Final state

- `git status --short`: clean
- staged paths: none
- HEAD: `49d89edb5d35a84d20d6d5880a118571134b0984`
- `git diff --check`: passed
- Architecture diff: none
- Verification diff: none
- Audit-created changes: **NONE**

Historical review artifacts were not read, modified, moved, or staged.

## 3. Exact live scope

- Title: `# 29. Aggregation and DISTINCT`
- Start: [ARCHITECTURE.md:21873](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21873)
- End: line **22385**
- Next heading: `# 30. Sorting and Top-N`
- Chapter-30 boundary: [ARCHITECTURE.md:22386](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22386)

Chapter 30 was inspected only through the immediate sorting/comparator/property handoff. It was not reviewed.

## 4. Complete subsection review matrix

| Section | Exact heading | Responsibility and owners | Runtime/downstream consumer | Classification |
|---|---|---|---|---|
| 29.1 | Hash aggregation role | Ch29 owns hash-aggregate role; Ch20 owns groups; Ch22 owns physical-plan vocabulary | pipeline builder, aggregate runtime | Architecture-appropriate |
| 29.2 | Aggregate state API | Ch29 owns conceptual state operations and batch dispatch | aggregate descriptors, workers, spill | Architecture-appropriate |
| 29.3 | V1 aggregate semantics registry | Ch29 owns aggregate value/state/finalization semantics | binder, executors, constant evaluator | Architecture-appropriate |
| 29.3.1 | Semantic state domains | Exact integer/count/dyadic conceptual domains; Ch24 owns resources | aggregate states, spill | Architecture-appropriate |
| 29.3.2 | Closed overload table | Ch29 value registry; Ch18/19 own syntax/binding | binder and executor dispatch | Architecture-appropriate |
| 29.3.3 | COUNT and integer SUM | Exact count/sum/AVG state and Finalize errors | Update/Combine/Finalize | Architecture-appropriate |
| 29.3.4 | FLOAT64 exact finite accumulation and rounding | Exact n-ary SUM/AVG semantics; Ch17 owns scalar arithmetic | all aggregate algorithms | Architecture-appropriate; design-scope question |
| 29.3.5 | FLOAT64 special values | Canonical NaN/infinity/zero aggregate results | Finalize | Architecture-appropriate |
| 29.3.6 | MIN/MAX canonical representation | Canonical retained representatives | Update/Combine/Finalize | Architecture-appropriate |
| 29.3.7 | Merge, execution shape, spill, and errors | Merge invariance, numeric error selection, publication barrier | parallel/spill/final output | Architecture-appropriate |
| 29.3.8 | Normative boundary vectors | Compact semantic examples | verification and implementers | Architecture-appropriate |
| 29.3.9 | Forbidden aggregate implementations | Prohibits mechanisms that violate frozen semantics | all implementations | Architecture-appropriate |
| 29.4 | Group hash table | Group ownership, hash mode, memory | grouped hash aggregation | **Architecture with semantic issue N29-1** |
| 29.5 | Global aggregate fast path | One no-key group without hash table | global aggregate | Architecture-appropriate |
| 29.6 | Hash-aggregate spill | Raw-value spill and exact replay | SpillManager/hash aggregate | Architecture with document-role issue |
| 29.7 | DISTINCT | Hash/group duplicate elimination | PhysicalDistinct | **Architecture with semantic issue N29-2 and owner issue M29-1** |
| 29.8 | Aggregation output properties | Unordered hash output/global cardinality | planner, consumers | Architecture-appropriate |
| 29.9 | Aggregation invariants | Normative summary | all consumers | Architecture-appropriate |
| 29.10 | Ordered aggregation and streaming DISTINCT | Capability-conditional ordered paths | planner, PhysicalSortAggregate/Distinct | **Architecture with role/resource issue M29-2 and chronology N29-5** |

## 5. Sections consulted

Architecture context:

- Front matter and contract language
- §§17.4.3, 17.7, 17.8.3, 17.10.3–17.10.4, 17.13
- §§18.11–18.12
- §§19.10–19.15
- §§20.2–20.3, 20.9–20.10, 20.12, 20.14.4–20.14.5, 20.14.9–20.14.11, 20.15–20.17
- Chapter 21 error/retry ownership
- §§22.1–22.7
- §§23.1, 23.5–23.14
- §§24.1–24.10
- Chapter 25 argument execution/error ownership
- §§26.1–26.10
- Chapter 27 handoff context
- §28.2.1 grouping hash mode
- §§30.1–30.3 and 30.8 only
- §31.10
- §§32.1–32.8
- §§37.1–37.5
- §§38.14, 38.16–38.20, 38.24
- §§39.1 and 39.3

Other live documents:

- DEVELOPMENT: Phase E7, Execution Milestones 2–5, optimizer ordered-aggregate/streaming opportunities
- PROJECT_STATE: document ownership and implemented storage-only state; no aggregate implementation claim
- VERIFICATION: V17/V19/V20/V22/V23/V24/V25/V26/V28 reusable coverage, Pipeline Finalization and Resource Tests, Parallel Execution Tests, Aggregate Tests, Sort Tests, physical-property and optimizer tests

## 6. Canonical owner matrix

| Contract | Canonical owner | Chapter-29 role |
|---|---|---|
| Aggregate syntax, `COUNT(*)` parse shape | Ch18 | references admitted forms |
| Placement/nesting | Ch19 | consumes bound descriptors |
| Overload/type/nullability resolution | Ch19 using §29.3 registry | owns registry values, not binding traversal |
| Scalar argument semantics | Ch17/25 | consumes successful typed values |
| Aggregate value and Finalize semantics | Ch29 | primary owner |
| Group bag/cardinality | Ch20 | executes it |
| Grouping equality | Ch17/20 | applies GROUPING mode |
| Hash compatibility | Ch17 and §28.2.1 | applies hash candidate/recheck |
| Aggregate occurrence and ordinal | Ch19/20 | uses ordinal for Finalize error selection |
| LogicalSlotIds | Ch20/22 | emits declared schema |
| PhysicalHashAggregate applicability | Ch22/38 | executes selected algorithm |
| PhysicalSortAggregate capability | Ch22/38 | defines execution constraints |
| Ordered-input requirement | Ch37/38, comparator in Ch30 | consumes proven property |
| DataChunk/vector occurrences | Ch23 | Update consumes active logical domain |
| Memory and exact extents | Ch24 | owns/account charges state |
| Spill infrastructure/errors | Ch24 | owns aggregate spill payload semantics |
| Combine scheduling | Ch26/32 | descriptors provide semantic Combine |
| Finalize readiness | Ch26/32 | defines aggregate readiness details |
| External publication | Ch31 | adds aggregate no-prefix prerequisite |
| Error categories | §39 | selects aggregate numeric ordinal locally |
| Transaction consequences | §39.1/Ch21 | no local decision |

No frozen upstream contradiction was found.

## 7. Physical operator and pipeline inventory

| Path | Logical role | Physical execution | State/lifecycle | Spill/order/publication |
|---|---|---|---|---|
| `PhysicalHashAggregate` grouped | `LogicalAggregate` | group-key/argument evaluation into group hash table | Sink; local/global states permitted; Combine; numeric Finalize; finalized Source | spill-capable; no ordering; all-group numeric validation before aggregate row exposure |
| Global aggregate specialized path | no-key `LogicalAggregate` | one aggregate state block, no group table | blocking Sink + Finalize + one-row Source | query-accounted; exactly one row on successful empty or nonempty input |
| `PhysicalSortAggregate` | same logical aggregate | contiguous ordered groups | capability-conditional; input order required; state per current group | only explicitly proven property; publication-buffer boundary incomplete |
| Hash `PhysicalDistinct` | `LogicalDistinct` | grouping-equivalent row classes using group-hash structure | baseline physical operator; blocking build is stated principally in §38.16 rather than locally | no ordering; spill/accounting required by Ch24 |
| Ordered/streaming DISTINCT | `LogicalDistinct` | emit one occurrence per adjacent class | streaming, capability-conditional | exact compatible ordering required; may stop only under Ch20/26 demand |
| Hash-aggregate spill/replay | grouped aggregate | group-hash partition and raw-value or exact-state replay | partition/repartition, Combine, Finalize | temporary only; exact equivalence; OOM/SpillIO under Ch24 |

All consume declared physical schemas and emit the Ch20/22-declared output schema. No execution state may invent `LogicalSlotId`s.

## 8. Aggregate registry

| Aggregate | Accepted input | Result | NULL/empty | State/finalization |
|---|---|---|---|---|
| `COUNT(*)` | no argument | `INT64 NOT NULL` | every row; empty `0` | exact nonnegative count/overflow marker; overflow only at Finalize |
| `COUNT(expr)` | any concrete v1 scalar | `INT64 NOT NULL` | ignore NULL; empty/all-NULL `0` | same count semantics |
| `SUM` | INT32 | nullable INT64 | ignore NULL; none → NULL | exact integer; Finalize range check |
| `SUM` | INT64 | nullable INT64 | same | exact integer; Finalize range check |
| `SUM` | FLOAT64 | nullable FLOAT64 | same | exact finite dyadic plus special flags; one rounding |
| `AVG` | INT32 | nullable FLOAT64 | ignore NULL; none → NULL | exact integer sum/count; exact rational then one rounding |
| `AVG` | INT64 | nullable FLOAT64 | same | same |
| `AVG` | FLOAT64 | nullable FLOAT64 | same | exact dyadic/count plus special flags |
| `MIN` | INT32, INT64, FLOAT64, VARCHAR, DATE, TIMESTAMP | nullable input type | ignore NULL; none → NULL | canonical least candidate |
| `MAX` | same | nullable input type | same | canonical greatest candidate |

Explicitly absent:

- BOOLEAN MIN/MAX
- SUM/AVG over BOOLEAN, VARCHAR, DATE, or TIMESTAMP
- implicit string/numeric/temporal conversions
- aggregate `DISTINCT`
- aggregate `FILTER`
- user-defined, approximate, window, percentile, or statistical aggregates

## 9. State API and vector semantics

`StateSize`, `StateAlignment`, `Initialize`, `Update`, `Combine`, `Finalize`, and conditional `Destroy` form a conceptual API, not a C++ ABI.

Precise:

- state size/alignment are descriptor-owned;
- fixed state may hold an owned variable-backing handle;
- variable backing is query-accounted;
- cleanup covers error and cancellation;
- Update/Combine/Finalize preserve semantic domains;
- state is query-local and not persisted or reused after failure;
- immutable descriptor/plan metadata may survive retry.

Implementation freedom appropriately remains for:

- inline versus indirect state;
- multiword/exponent-bin/superaccumulator representation;
- source-preserving versus source-consuming Combine internals, provided lifecycle ownership remains safe;
- Finalize’s internal mutation strategy;
- dispatch and container mechanics.

Repeated Finalize and double-destroy need not be public capabilities; invoking outside the valid lifecycle is an internal protocol fault.

Vector Update is correctly occurrence-based:

- active SelectionVector entries define occurrences;
- CONSTANT cardinality `100` contributes 100 times;
- repeated DICTIONARY indices remain repeated inputs;
- inactive capacity contributes nothing;
- NULL validity controls aggregate admission;
- vector shape cannot alter result/error.

## 10. Grouping and DISTINCT representatives

### Grouping equality

Precise and consistent:

- NULL groups with NULL.
- `-0.0` groups with `+0.0`.
- canonical-equivalent NaNs form one class.
- VARCHAR uses exact bytes.
- composite equality is componentwise.
- equal values must hash compatibly.
- collision is only a candidate; full grouping equality is required.
- retained VARCHAR keys are deep-copied.

### Group-key representative

| Class | Result |
|---|---|
| NULL | typed NULL; no payload selection issue |
| ordinary integer/date/timestamp/BOOLEAN | equivalent representations do not create a live ambiguity |
| VARCHAR | equality requires identical length and bytes, so every member has the same visible value |
| `-0.0` / `+0.0` | **SEMANTIC QUESTION N29-1** |
| multiple NaN encodings | **SEMANTIC QUESTION N29-1** |

Chapter 17 makes the zero distinction observable: FLOAT64-to-VARCHAR preserves zero sign. Chapter 29 deep-copies a group key but does not say whether it canonicalizes grouping-class representatives. Thus first insertion, worker Combine order, hash order, or spill replay could select the output bits.

### DISTINCT representative

| Class | Result |
|---|---|
| NULL | typed NULL |
| VARCHAR | exact equivalent bytes, no alternative visible representative |
| ordinary fixed types | no identified multi-representation ambiguity |
| `-0.0` / `+0.0` | **SEMANTIC QUESTION N29-2** |
| multiple NaN encodings | **SEMANTIC QUESTION N29-2** |

`LogicalDistinct` preserves child slots and emits one occurrence per class, but neither Chapter 20 nor Chapter 29 specifies whether the retained scalar is canonical or an arbitrary class member.

## 11. Aggregate-value results

- Global aggregate: exactly one result row after successful execution, including empty input.
- Explicit grouping over empty input: zero groups and zero rows.
- `COUNT(*)`: counts every demanded child occurrence; empty is INT64 `0`.
- `COUNT(expr)`: evaluates demanded arguments and counts non-NULL values.
- Count beyond `INT64_MAX`: absorbing semantic marker permitted; no early error; `NUMERIC_OVERFLOW` at Finalize.
- Integer SUM: exact mathematical subtotal; `INT64_MAX + 1 - 1` succeeds as `INT64_MAX`.
- Integer AVG: exact rational; `AVG(INT64_MAX, INT64_MIN) = -0.5`.
- FLOAT64 SUM/AVG: exact finite-dyadic accumulation and one correctly rounded Finalize.
- Exact aggregate zero: `+0.0`.
- NaN present, or both infinities: canonical quiet NaN.
- One infinity sign only: that infinity.
- MIN/MAX: Chapter-17 total order and canonical representatives; NULL ignored.
- VARCHAR MIN/MAX: exact-byte order and owned retained bytes.
- Hash, ordered, serial, parallel, and spilled execution must use the same Merge law.

The §29.3.8 boundary-vector corpus is internally consistent with Chapter 17 and the overload table. No adjacent missing Architecture example creates an additional ambiguity.

## 12. Error and publication assessment

- Aggregate argument and child errors occur before a row contributes.
- COUNT overflow cannot suppress later demanded input work.
- Integer SUM cannot fail solely from a cancelable intermediate subtotal.
- Final aggregate range failures are `NUMERIC_OVERFLOW`.
- If multiple aggregate descriptors fail, lowest semantic aggregate ordinal wins.
- Group identity, bucket order, worker order, and Finalize iteration are excluded from ranking.
- Same ordinal failing in multiple groups yields the same category/provenance, so no extra group ranking is required.
- Resource failure or cancellation may prevent reaching numeric Finalize; no global cross-class precedence is invented.
- No approximation is permitted after OOM/spill failure.
- Every group’s numeric state must validate before the first aggregate row is exposed.
- Internal buffering may precede external exposure, but the failed invocation’s current output is invalid.
- Chapter 31 owns returned cursor-prefix behavior; Chapter 29’s stronger aggregate barrier prevents an aggregate overflow discovered during Finalize from following an already returned aggregate row.

## 13. Ownership, memory, and spill

Group hash state owns:

- deep-copied group keys;
- aligned state blocks;
- variable aggregate backing;
- optional cached hash;
- collision/equality metadata.

VARCHAR group keys and VARCHAR MIN/MAX candidates must have value-stable owned bytes beyond input-chunk reuse.

All dynamic state is covered by Chapter 24, including directories, group entries, exact integer/dyadic backing, candidate bytes, spill metadata/buffers, worker-local tables, continuation state, and DISTINCT storage.

Spill:

- remains query-temporary and non-WAL;
- may serialize safe exact state or replay raw qualifying values;
- cannot serialize rounded FLOAT subtotals or narrowed integer subtotals;
- must reproduce unlimited-memory groups, values, and errors;
- inherits checked extents, bounded pressure progress, `OutOfMemory`, `SpillIOError`, cleanup, and fresh-retry ownership from Chapter 24.

“Bounded in the same spirit as Grace hash join” is weak prose, but Chapter 24 resolves finite progress and terminal OOM when no exact progress action remains.

## 14. Ordering, schema, and required slots

- Hash aggregate: no `OrderingProperty`.
- Hash DISTINCT: no `OrderingProperty`.
- Group hash iteration, insertion, pointer, partition, spill, and worker order are nonsemantic.
- `PhysicalSortAggregate`: only a capability-proven property may be advertised.
- Ordered DISTINCT: requires ordering compatible with all DISTINCT keys and grouping equivalence.
- Chapter 30’s full comparator uses the same FLOAT64 total order, NULL placement, and binary VARCHAR semantics, so grouping-equivalent values are adjacent under an appropriate complete ordering.
- Required order enforcement remains Chapters 37–38/30-owned.
- Aggregate group outputs and aggregate occurrences use declared Ch20/22 slots.
- DISTINCT preserves child schema and `LogicalSlotId`s.
- RequiredSlotSet must retain group keys, aggregate arguments, HAVING/ORDER BY dependencies, output values, and diagnostic mappings.
- Payload pruning cannot destroy logical row/group cardinality. Zero-width cases retain occurrence counts when the physical representation supports them.

## 15. Pipeline findings

### Hash aggregate

Clear blocking sequence:

`Sink → local/global Combine → numeric Finalize/all-group validation → ready group Source`

One worker cannot publish readiness; all required local/global work must finish.

### Hash DISTINCT

Chapter 38 calls DISTINCT build a blocking semantic boundary, but Chapter 29 only says `PhysicalDistinct` “may reuse” group-hash infrastructure. That is an inverted owner boundary: the execution chapter should state whether baseline hash DISTINCT is a blocking Sink/Finalize/Source operator.

### Ordered aggregate

Ordered input allows one active group, but all groups must validate before any dependent aggregate row is exposed. Therefore completed groups must be retained or spooled somewhere until global validation succeeds. The claim that memory is approximately current-group state does not state:

- who owns retained finalized rows;
- whether operator-local memory excludes an output spool;
- whether the spool spills;
- when it becomes a source;
- how its resource failure composes with Finalize.

No early external row is legal. The missing point is the buffering/resource handoff, not the aggregate value semantics.

### Ordered DISTINCT

It may emit one adjacent-class row incrementally because the all-group numeric barrier does not apply. Early stop is governed by Ch20/26 consumer demand. It must not infer class boundaries from an incompatible comparator.

## 16. Algorithm substitutability

| Alternatives | Required equivalent observables | Permitted differences | Status |
|---|---|---|---|
| Hash vs Sort aggregate | groups, aggregate values/NULLs, numeric error ordinal, schema/slots | memory, CPU, spill, unordered group sequence | precise except group representative N29-1 |
| Serial vs parallel | values/errors/groups | state bytes, worker timing, Merge tree | precise |
| In-memory vs spill | groups/values/errors | files, partitions, replay order | precise |
| Hash vs ordered DISTINCT | classes, schema/slots, representative | memory, execution order, provided property | blocked by N29-2 |
| Vector/chunk variations | values/errors/groups | physical batches | precise |

## 17. Project complexity and instructiveness

| Mechanism | Classification | Assessment |
|---|---|---|
| Vectorized Update | Core modern DB mechanism | High educational/performance value |
| Group hash table | Core modern DB mechanism | Essential |
| Global aggregate fast path | Core modern DB mechanism | Simple and valuable |
| Worker-local state + Combine | Core modern DB mechanism | Avoids shared hot-state contention |
| Query memory accounting | Core modern DB mechanism | Essential for bounded execution |
| Aggregate spill | Advanced but justified | Natural continuation of memory ownership/spill |
| Ordered aggregation | Useful instructive extension | Valuable property-aware alternative |
| Streaming DISTINCT | Useful instructive extension | Simple once ordering properties exist |
| Exact arbitrary-range integer state | Advanced but justified | Freezes cancellation- and Merge-invariant semantics while leaving representation free |
| Exact finite-dyadic FLOAT SUM/AVG | **Possible excess for v1; D29-1** | Strong reproducibility, substantial arithmetic/state/spill complexity |
| Deterministic lowest-ordinal aggregate errors | Advanced but justified | Strong deterministic diagnostics; modest relative complexity once ordinals exist |
| All-group numeric validation before publication | Advanced but justified | Prevents failed query from exposing an aggregate prefix, but requires explicit buffering |

The exact FLOAT64 contract is substantially stronger than ordinary engine practice. DuckDB documents floating SUM/AVG as order-affected, including its Kahan variants; PostgreSQL exposes floating SUM/AVG as floating results and supports partial aggregation rather than promising DBlusBlus-style exact n-ary reduction. [DuckDB aggregate documentation](https://duckdb.org/docs/lts/sql/functions/aggregates), [PostgreSQL aggregate documentation](https://www.postgresql.org/docs/current/functions-aggregate.html).

## 18. Temporal and document-role audit

Meaningful temporal terms in Chapter 29:

| Line | Phrase | Classification |
|---:|---|---|
| 21889 | “until its group state is finalized” | runtime lifecycle |
| 22016 | “later demanded input…” | semantic demand |
| 22020 | “later values may cancel” | mathematical input sequence |
| 22038/22059 | “first” rounding/materialization operation | execution semantics |
| 22161 | “before the first aggregate result row” | publication lifecycle |
| 22164–22166 | first source-byte occurrence | semantic ordinal |
| 22299 | “A later aggregate descriptor may…” | project/development chronology |
| 22301 | “current v1 aggregate” | durable closed-registry scope, though “v1 registry aggregate” would be clearer |
| 22329 | “A later physical planner may choose…” | project/development chronology |
| 22360 | “supports later ordered implementations” | project/development chronology |
| 22364 | “current group” | runtime state |
| 22382 | “Until the runtime capability exists…” | current-state/project chronology |

Project chronology count: **4**.

No Verification procedure, test result, benchmark history, devlog history, or Project-State implementation inventory leaks into Chapter 29. The four phrases are Development/Project-State-style availability narration.

## 19. Forbidden-implementation audit

All 16 prohibitions are correctness-essential or direct semantic clarifications:

- items 1–3, 6–12 protect reduction-shape invariance;
- items 4–5 prohibit insufficient host representations, not harmless implementation choices;
- items 13–14 protect publication/error determinism;
- item 15 protects constant/runtime equivalence;
- item 16 protects exactness under resource failure.

The list does not mandate one accumulator container, integer library, spill record, worker layout, or dispatch ABI. No Verification-style leakage was found.

## 20. Explicit cross-reference table

| Chapter-29 reference | Handoff | Quality |
|---|---|---|
| Chapter 17 | argument semantics, total order, canonical scalar behavior | good |
| §39.1 | transaction effects | good |
| §29.3 internal links | state, special values, canonical candidates | good |
| Chapter 24 | memory/spill/error ownership | good |
| §28.2.1 | centralized GROUPING hash mode | good, though Ch17/20 remain semantic owners |
| Chapter 20 / §20.9 | grouping/DISTINCT equivalence | good |
| Chapter 22 capability registry | conditional algorithms | concept correct; wording chronological |
| Chapter 30 implicit comparator handoff | ordered contiguity | should be explicit |
| Chapter 37/38 implicit property/planning handoff | eligibility/order enforcement | should be explicit |
| Chapter 26/31 publication barrier | aggregate readiness/external exposure | semantically consistent but ordered-buffer ownership incomplete |
| Chapter 32 | parallel local/global Combine | consistent |

## 21. Actual technical-consistency matrix — 220 checks

Legend: **Y** precise/consistent; **OD** owner-delegated; **N/A** outside v1; **SQ** semantic question; **DSQ** design-scope question; **NO** finding.

| Questions | Results |
|---|---|
| 1–10 Registry | 1 exact function list Y; 2 COUNT-star Y; 3 COUNT-expr Y; 4 SUM overload closure Y; 5 AVG closure Y; 6 MIN/MAX closure Y; 7 BOOLEAN MIN/MAX N/A; 8 aggregate DISTINCT N/A; 9 FILTER N/A; 10 implicit conversions forbidden Y |
| 11–20 Binding | 11 syntax OD-Ch18; 12 placement OD-Ch19; 13 nesting OD-Ch19; 14 overload resolution OD-Ch19/29; 15 result type Y; 16 nullability Y; 17 source ordinal Y; 18 rewrite retention Y; 19 alias reuse Y; 20 physical sharing mapping Y |
| 21–30 State API | 21 StateSize Y; 22 alignment Y; 23 Initialize Y; 24 Update Y; 25 Combine Y; 26 Finalize Y; 27 Destroy Y; 28 variable handle Y; 29 state persistence forbidden Y; 30 retry freshness OD-Ch26 |
| 31–40 Vector input | 31 active cardinality OD-Ch23; 32 selection OD-Ch23; 33 CONSTANT repeats Y; 34 DICTIONARY repeats Y; 35 NULL validity Y; 36 inactive capacity excluded Y; 37 empty batch not EOS OD-Ch23; 38 zero-column occurrences preserved OD-Ch20/23; 39 batch dispatch Y; 40 per-row virtual callback forbidden Y |
| 41–50 Grouping equality | 41 NULL class Y; 42 signed-zero class Y; 43 NaN class Y; 44 VARCHAR bytes Y; 45 integer equality Y; 46 temporal equality Y; 47 composite componentwise OD-Ch17/28; 48 equal→hash-compatible Y; 49 collision recheck Y; 50 ordinary NULL equality distinct Y |
| 51–60 Group representation | 51 NULL representative Y; 52 VARCHAR representative Y; 53 integer representative Y; 54 temporal representative Y; 55 signed-zero representative SQ-N29-1; 56 NaN representative SQ-N29-1; 57 hash-order independence SQ; 58 worker-order independence SQ; 59 spill-order independence SQ; 60 downstream cast observability SQ |
| 61–70 DISTINCT | 61 one row/class Y; 62 child schema preserved OD-Ch20; 63 child slots preserved OD-Ch20; 64 NULL class Y; 65 VARCHAR class Y; 66 signed-zero representative SQ-N29-2; 67 NaN representative SQ-N29-2; 68 zero-width class cardinality OD-Ch20/23; 69 hash ordering absent Y; 70 ordered equivalence blocked by SQ |
| 71–80 Empty/global | 71 no-key one group Y; 72 empty global one row Y; 73 grouped empty zero OD-Ch20; 74 COUNT empty zero Y; 75 SUM empty NULL Y; 76 AVG empty NULL Y; 77 MIN empty NULL Y; 78 MAX empty NULL Y; 79 no global hash table Y; 80 “at most one” reconciled by successful-path rule Y |
| 81–90 COUNT | 81 every row COUNT-star Y; 82 no argument demand Y; 83 expr every demanded row Y; 84 NULL ignored Y; 85 exact count Y; 86 no wrap Y; 87 overflow marker Y; 88 no early error Y; 89 Finalize overflow Y; 90 later demanded errors retained Y |
| 91–100 Integer SUM/AVG | 91 exact subtotal Y; 92 local overflow not final Y; 93 cancellation vector Y; 94 final INT64 range Y; 95 SUM(INT32)→INT64 Y; 96 AVG exact sum Y; 97 AVG exact count Y; 98 no SUM pre-round/range Y; 99 no partial averages Y; 100 huge AVG count exact Y |
| 101–110 FLOAT finite | 101 exact decoded inputs Y; 102 dyadic state Y; 103 no Update rounding Y; 104 no Combine rounding Y; 105 SUM one final rounding Y; 106 AVG exact rational Y; 107 ties-even Y; 108 subnormal Y; 109 overflow infinity Y; 110 vector/worker invariance Y |
| 111–120 FLOAT special | 111 NaN dominance Y; 112 mixed infinity→NaN Y; 113 positive infinity Y; 114 negative infinity Y; 115 canonical NaN Y; 116 payload independence Y; 117 exact zero→+0 Y; 118 only -0→+0 Y; 119 AVG special count Y; 120 physical order irrelevant Y |
| 121–130 MIN/MAX | 121 NULL ignored Y; 122 no value→NULL Y; 123 Chapter-17 order Y; 124 signed-zero canonical Y; 125 NaN canonical Y; 126 NaN order Y; 127 VARCHAR bytes Y; 128 VARCHAR ownership OD-Ch23/29; 129 equal-class encounter independence Y; 130 no arithmetic overflow Y |
| 131–140 Merge | 131 empty identity Y; 132 exact integer addition Y; 133 exact dyadic addition Y; 134 exact count addition Y; 135 commutative flags Y; 136 canonical candidate merge Y; 137 tree-shape invariance Y; 138 source-state consumption implementation freedom Y; 139 hash/sort equivalence Y; 140 spill equivalence Y |
| 141–150 Errors | 141 argument errors OD-Ch17/25; 142 child errors OD-Ch26/39; 143 COUNT deferred Y; 144 SUM deferred Y; 145 numeric category Y; 146 lowest ordinal Y; 147 group not ranking key Y; 148 worker not ranking key Y; 149 resource class OD-Ch24/39; 150 transaction effect OD-§39.1 |
| 151–160 Publication | 151 all input before numeric validation Y; 152 every group validated Y; 153 no failing aggregate prefix Y; 154 internal/external distinction OD-Ch26/31; 155 current failed output invalid OD-Ch25/26; 156 prior external cursor prefix OD-Ch31; 157 worker-local completion insufficient Y; 158 dependency publication OD-Ch26/32; 159 failed Finalize blocks output Y; 160 cleanup distinct from Finalize OD-Ch26 |
| 161–170 Hash state | 161 one state/group Y; 162 group key deep copy Y; 163 aligned state Y; 164 cached hash optional Y; 165 collision not identity Y; 166 varlen stable Y; 167 state accounted Y; 168 directory accounted OD-Ch24; 169 arbitrary container allowed Y; 170 no persistent runtime identity Y |
| 171–180 Spill | 171 temporary only Y; 172 raw values allowed Y; 173 exact state allowed conditionally Y; 174 rounded subtotal forbidden Y; 175 integer narrowing forbidden Y; 176 partition equality compatible Y; 177 recursive bounded OD-Ch24; 178 skew terminal OOM OD-Ch24; 179 SpillIO OD-Ch24/39; 180 cleanup/retry OD-Ch24/26 |
| 181–190 PhysicalDistinct | 181 operator exists Y; 182 grouping-hash reuse optional Y; 183 zero aggregate states Y; 184 blocking hash shape NO-M29-1; 185 Sink role NO-M29-1; 186 Finalize readiness NO-M29-1; 187 streaming hash alternative unauthorized/unclear NO; 188 ordered path conditional Y; 189 early stop OD-Ch20/26; 190 no hash SQL order Y |
| 191–200 Ordered paths | 191 SortAggregate conditional Y; 192 ordered grouping keys required Y; 193 Sort enforcement OD-Ch30/37/38; 194 comparator compatibility Y; 195 current-group processing Y; 196 all-group publication barrier Y; 197 retained-output owner NO-M29-2; 198 low-memory scope NO-M29-2; 199 ordered DISTINCT contiguity Y; 200 capability availability chronology NO-N29-5 |
| 201–210 Properties/schema | 201 hash aggregate no order Y; 202 grouped hash order unspecified Y; 203 global one row Y; 204 SortAggregate property OD-Ch37; 205 ordered DISTINCT property OD-Ch37; 206 declared schema OD-Ch20/22; 207 aggregate slots OD-Ch20; 208 DISTINCT slots OD-Ch20; 209 RequiredSlotSet OD-Ch37; 210 runtime IDs forbidden Y |
| 211–220 Lifecycle/scope | 211 local/global state OD-Ch22/32; 212 Combine barrier OD-Ch26/32; 213 cancellation OD-Ch26/32; 214 fresh retry OD-Ch26/39; 215 invalid state OD-Ch26/39; 216 exact integer mechanism freedom Y; 217 exact FLOAT mechanism freedom Y; 218 exact FLOAT v1 proportionality DSQ-D29-1; 219 chronology-free chapter NO-N29-5; 220 canonical standalone architecture NO pending N29-1/N29-2 |

## 22. Findings

### BLOCKING

#### N29-1 — GROUP BY representative is undefined

- Section: §29.4, composing with §§17.4.3, 17.8.3, 20.9
- Evidence: grouping equates signed zeros and NaNs, while group rows merely own a deep-copied key.
- Alternative A: canonicalize every grouping key component, e.g. zero to `+0.0` and NaN to canonical NaN.
- Alternative B: retain an arbitrary/first class member.
- Observable difference: `CAST(group_key AS VARCHAR)` can distinguish zero sign; raw client FLOAT64 output may distinguish bits.
- Consequence: result can depend on input, hash, worker, Combine, or spill order.
- Smallest future action: define the output representative for every grouping-equivalence class.

#### N29-2 — DISTINCT representative is undefined

- Section: §§29.7 and 29.10, composing with §20.10
- Evidence: one row is emitted per equivalence class, but no member-selection or component-canonicalization rule exists.
- Alternatives: canonical row-value representation versus arbitrary encountered member.
- Observable difference: signed-zero text/result bits and potentially NaN bits.
- Consequence: hash and ordered DISTINCT can return distinguishable values.
- Smallest future action: define canonical component representation or explicitly define an allowed SQL-observational equivalence boundary.

### MAJOR

#### M29-1 — Baseline hash DISTINCT pipeline role is owned indirectly

- Section: §29.7
- Evidence: “may reuse” group-hash infrastructure; blocking “DISTINCT build” appears in §38.16.
- Affected handoff: Ch29 → Ch26/38.
- Consequence: implementations could disagree about full-input blocking versus streaming insertion/emission, changing early-stop demand and error visibility.
- Smallest future action: state the baseline hash `PhysicalDistinct` Sink/Finalize/Source and early-stop contract directly in Chapter 29.

#### M29-2 — Ordered aggregate buffering/resource boundary is incomplete

- Section: §§29.3.7 and 29.10
- Evidence: all groups must validate before exposure, while ordered aggregation claims approximately current-group memory.
- Affected handoff: Ch29 → Ch24/26/31.
- Consequence: hidden all-group output retention/spooling ownership and resource failures must be invented.
- Smallest future action: define whether the memory claim is aggregation-state-only and identify the owner/spill/publication transition for retained finalized rows.

### MINOR

#### N29-5 — Capability chronology in live Architecture

- Sections: §§29.6, 29.7, 29.10
- Phrases:
  - “A later aggregate descriptor may…”
  - “A later physical planner may choose…”
  - “supports later ordered implementations…”
  - “Until the runtime capability exists…”
- Timeless concept: capability-conditional alternatives are eligible only when their descriptor/runtime/property contracts are enabled.
- State owner: actual implementation availability belongs in PROJECT_STATE; sequencing belongs in DEVELOPMENT.
- Smallest future action: present-tense capability wording only.

No editorial-only findings.

## 23. Design-scope question

### D29-1 — Exact correctly rounded FLOAT64 SUM/AVG in v1

- Current rule: exact finite-dyadic accumulation, exact count/rational division, one correctly rounded Finalize, order/worker/spill independence.
- Correctness benefit: bit-stable results across all legal physical reductions.
- Ordinary SQL requirement: SQL engines generally define result types and broad arithmetic behavior but commonly do not promise bitwise order-independent floating aggregation.
- Typical practice: ordinary floating accumulation, compensated accumulation, or implementation-dependent partial reduction; DuckDB explicitly documents floating SUM/AVG as order-affected.
- Cost: nontrivial accumulator, conversion, Combine, serialization/replay, accounting, and boundary verification.
- Educational value: high for reproducible numerics and parallel reduction, but beyond the core mechanics needed to teach hash/ordered aggregation.
- Classification: **POSSIBLE EXCESS FOR V1**.
- Recommended policy choice: explicitly affirm this as a flagship deterministic-numerics goal, or adopt a simpler, precisely frozen floating aggregate contract. Do not weaken it implicitly during implementation.
- Verification dependency: complete FLOAT execution-shape verification depends directly on the decision.

Exact integer state is classified **ADVANCED BUT JUSTIFIED**, not a separate open question: it prevents false overflow from partition shape and retains broad implementation freedom.

## 24. Verification cross-check

| Family | Existing status |
|---|---|
| Aggregate overload registry | Complete existing coverage |
| Empty global/grouped behavior | Partial |
| Aggregate occurrence/ordinal identity | Complete existing coverage |
| Numeric error ordinal | Complete existing coverage |
| COUNT overflow | Complete existing coverage |
| Integer SUM cancellation | Complete existing coverage |
| Integer AVG rational behavior | Complete existing coverage |
| FLOAT exact accumulation/special values | Complete existing coverage |
| MIN/MAX canonicalization | Complete existing coverage |
| Group equality/hash compatibility | Complete existing coverage |
| Group representative | Blocked by N29-1 |
| DISTINCT representative | Blocked by N29-2 |
| State API lifecycle and malformed states | Partial |
| Varlen group/candidate ownership | Partial |
| Complete aggregate memory ledger | Partial |
| Spill exactness | Partial |
| Recursive repartition/skew progress | Missing dedicated aggregate method |
| Combine numerical determinism | Complete |
| Combine ownership/failure lifecycle | Partial |
| Hash versus ordered aggregate | Partial |
| Hash versus ordered DISTINCT | Partial and blocked by N29-2 |
| Ordering-property matrix | Partial |
| RequiredSlotSet/zero-width groups | Partial |
| Pipeline readiness/all-group validation | Complete existing generic coverage |
| No result prefix before Finalize failure | Complete existing coverage |
| Cancellation | Partial generic coverage |
| Retry freshness | Missing aggregate-specific matrix |
| Invalid aggregate runtime states | Missing |
| Document chronology | Missing dedicated audit |

Reusable Verification material:

- V17 equality/hash/FLOAT/VARCHAR oracles
- V19 aggregate registry, placement, ordinal, provenance
- V20 grouping/DISTINCT bag and occurrence oracles
- V22 physical operator/capability/schema matrix
- V23 logical-occurrence and borrowing tests
- V24 memory/spill/resource methodology
- V25 argument expression and error-selection methodology
- V26 dependency, Finalize, publication, cancellation, and retry models
- V28 centralized hash/equality/collision methodology
- Aggregate Tests exact state/finalization and FLOAT execution-shape invariance
- Sort comparator/property tests
- Parallel Execution Tests

Verification is **not synchronized for Chapter 29** and was not modified.

## 25. Previous-chapter regression

| Chapter | Result |
|---|---|
| 17 | No contradiction; scalar signed-zero preservation exposes N29-1/N29-2 |
| 18 | No contradiction |
| 19 | No contradiction |
| 20 | No contradiction; representative choice remains undefined |
| 21 | No contradiction |
| 22 | No contradiction |
| 23 | No contradiction |
| 24 | No contradiction |
| 25 | No contradiction |
| 26 | No contradiction; exposes M29-2 buffering handoff |
| 28 | No contradiction |

## 26. Direct answers to final review questions

- Different group bags? **No**, except output representative values within one equivalence class remain undefined.
- Different visible DISTINCT representatives? **Yes — N29-2.**
- Can ±0 GROUP BY depend on first encounter? **Yes — N29-1.**
- Can NaN GROUP BY depend on payload encounter order? **Undefined — N29-1.**
- Can DISTINCT ±0 depend on hash/worker order? **Yes — N29-2.**
- Can COUNT overflow suppress later demanded error? **No.**
- Can integer SUM fail on a cancelable local subtotal? **No.**
- Can FLOAT SUM depend on vector size? **No.**
- Can FLOAT AVG depend on worker count? **No.**
- Can MIN/MAX retain noncanonical NaN/zero? **No.**
- Can hash and ordered aggregate return different aggregate values? **No.**
- Can spill change aggregate value/error? **No.**
- Can collision merge distinct groups? **No.**
- Can retained VARCHAR bytes dangle? **No.**
- Can one worker publish readiness? **No.**
- Can SortAggregate externally expose an early valid group before later overflow? **No.**
- Can ordered aggregation claim only current-group total memory while withholding all rows? **Not without clarifying retained-output ownership — M29-2.**
- Can hash DISTINCT emit rows before complete input? **The live ownership is insufficiently direct — M29-1; §38 currently treats it as blocking.**
- Can ordered DISTINCT stop early? **Yes, only when Ch20/26 consumer demand proves remaining input unnecessary.**
- Can aggregate runtime invent a new `LogicalSlotId`? **No.**
- Can hash iteration become SQL ordering? **No.**
- Can capability be expressed without chronology? **Yes.**
- Does Chapter 29 describe implementation history? **Four phrases do — N29-5.**
- Is exact FLOAT accumulation clearly justified for v1? **It requires explicit policy affirmation — D29-1.**
- Does Chapter 29 contain correctness-relevant invention points? **Yes: N29-1, N29-2, M29-1, and M29-2.**
- Can Chapter 29 stand unchanged as canonical v1 Architecture? **No.**

## 27. Recommended fixing sequence

1. **Step A — low-risk/document role:** remove the four chronology phrases in §§29.6, 29.7, and 29.10.
2. **Step B — local clarification:** state baseline hash DISTINCT’s blocking Sink/Finalize/Source role and its Ch20/26 early-stop boundary.
3. **Step C — owner/cross-reference repair:** add explicit Chapter 30/37/38 ordering and capability references.
4. **Step D — semantic decisions:** resolve N29-1 and N29-2 canonical representative rules.
5. **Step E — high-impact pipeline/design:** define ordered-aggregate retained-output ownership, memory/spill boundary, then explicitly affirm or revise D29-1’s exact FLOAT64 policy.

Recommended next task: **FROZEN CHAPTER-29 SEMANTIC REVIEW / DECISION PACKAGE**, beginning with N29-1 and N29-2. Verification synchronization must wait.

Chapter 30 review: **NOT STARTED**.
Verification modification: **NONE**.
Implementation: **NOT STARTED**.
Build/test/sanitizer/benchmark: **NOT RUN**.

# Chapter 29 frozen semantic/design decision review

## 1. Repository state

Initial and final repository state:

- HEAD: `49d89edb5d35a84d20d6d5880a118571134b0984`
- Commit: `synced VERIFICATION after chapter 28 ARCHITECTURE fix`
- Index: clean
- Tracked working tree: clean
- Pre-existing untracked path:

  `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 29/`

- `git diff --check`: passed
- Audit-created changes: **NONE**

The untracked historical-review directory was not read, modified, moved, or staged.

## 2. Decision inventory

| ID | Decision status | Recommended outcome |
|---|---|---|
| N29-1 | User policy decision required | Canonical FLOAT64 grouping-key representative |
| N29-2 | User policy decision required | Componentwise canonical DISTINCT representative |
| D29-1 | User policy decision required | F2: bounded ordinary binary64 partial aggregation |
| M29-2 | No user decision required | Spill-capable finalized-output spool |
| M29-1 | Direct fix; no user decision | Baseline hash DISTINCT is blocking Sink → Finalize → Source |
| N29-5 | Direct fix; no user decision | Timeless capability-conditional wording |

# 3. N29-1 — GROUP BY representative

## Option matrix

| Dimension | Option A — canonical representative | Option B — arbitrary class member |
|---|---|---|
| SQL-visible result | `±0 → +0`; all equivalent NaNs → canonical NaN | Any actual member may survive |
| Determinism | Independent of encounter order | May vary by input/hash/worker order |
| Hash vs sort aggregate | Exact representative equivalence | May return different bits |
| Parallelism | Combine order cannot affect key bits | Worker winner can affect output |
| Spill | Replay order irrelevant | Replay order can select representative |
| Downstream CAST | Stable: zero formats as `0` | Could produce `0` or `-0` |
| Implementation | Normalize FLOAT64 key when retained/materialized | Retain first/any member |
| Runtime cost | Negligible bit normalization | Marginally simpler |
| Modern-engine relevance | Strong deterministic result discipline | Arbitrary member behavior is common but often undocumented |
| Educational value | Clearly separates equivalence, hashing, and representation | Teaches physical representative selection but leaks execution order |
| Owner | Ch17 defines scalar canonicalization; Ch20 applies it to grouping output | Ch20 would need to explicitly authorize arbitrary representation |
| Future edit | §§17.10.3, 20.9, 29.4/29.9 | §§20.9 and 29.4 would need explicit nondeterminism |

## Recommendation

**Choose Option A: canonical representative.**

The cost is extremely small because hash/key normalization already identifies signed zeros and NaNs. It provides exact hash/sort/spill/parallel substitutability and prevents an incidental first insertion from becoming SQL semantics.

Recommended rule:

```text
Every FLOAT64 grouping-key output component is materialized using the
canonical representative of its grouping-equivalence class:

    -0.0 or +0.0 -> +0.0
    any grouping-equivalent NaN -> Chapter-17 canonical quiet NaN.

Other scalar types retain their existing exact semantic representation.
```

NULL remains a typed NULL. VARCHAR-equivalent values already have identical length and bytes.

# 4. N29-2 — DISTINCT representative

## Option matrix

| Dimension | Option A — componentwise canonical row | Option B — arbitrary member row |
|---|---|---|
| SQL-visible result | Every emitted component uses its grouping canonical representative | Any member row may survive |
| Determinism | Stable across all execution shapes | Hash, worker, spill, or input order may affect bits |
| Hash vs ordered DISTINCT | Exact value equivalence | Algorithms may emit distinguishable values |
| Parallelism | Worker winner irrelevant | Worker winner may determine result |
| Spill | Replay order irrelevant | Replay order may determine result |
| Downstream CAST | `±0` always becomes `0` | May become `0` or `-0` |
| Schema/slots | Child schema and slots preserved | Same |
| Implementation | Normalize retained/output components | Keep first/any stored row |
| Runtime cost | Negligible for FLOAT64; no extra row search | Slightly simpler |
| Modern-engine relevance | Strong reproducibility | Arbitrary representative is common where equality hides representation differences |
| Educational value | Demonstrates that equality class and output representation are separate contracts | Exposes physical-order nondeterminism without relational benefit |
| Owner | Ch17 canonical map; Ch20 DISTINCT result rule; Ch29 execution delegation | Ch20 must explicitly allow arbitrary-member output |

## Recommendation

**Choose Option A: componentwise canonical row.**

Recommended rule:

```text
DISTINCT first determines duplicate classes using grouping equivalence.
Its one emitted row per class is materialized componentwise using the
canonical grouping representative.

This changes neither the child schema nor any LogicalSlotId.
```

N29-1 and N29-2 should use one shared scalar canonicalization rule. Separate canonicalization policies would create needless opportunities for hash aggregate, sort aggregate, hash DISTINCT, and ordered DISTINCT to disagree.

# 5. Shared owner recommendation

Use a layered, nonduplicated ownership model:

| Owner | Minimal responsibility |
|---|---|
| Chapter 17, §17.10.3 | Define the canonical representative function for grouping-equivalent scalar classes |
| Chapter 20, §§20.9–20.10 | Require GROUP BY and DISTINCT output values to use that function |
| Chapter 29, §§29.4, 29.7, 29.9–29.10 | Require every physical algorithm to materialize the Chapter-20 representative |
| Verification | Compare output components against an independent canonicalization oracle |

Chapter 17 should define approximately:

```text
CanonicalGroupingRepresentative(value):

    typed NULL -> same typed NULL
    FLOAT64 zero class -> +0.0
    FLOAT64 NaN class -> canonical quiet NaN
    every other non-NULL scalar -> its existing exact semantic representation
```

This is runtime/result canonicalization. It must not redefine heap storage, ordinary scalar arithmetic, or the rule that persisted FLOAT64 can preserve signed zero.

# 6. D29-1 — FLOAT64 SUM/AVG policy

## F1/F2/F3 comparison

| Dimension | F1 — current exact contract | F2 — ordinary binary64 partials | F3 — compensated partials |
|---|---|---|---|
| Finite SUM state | Exact dyadic sum | Binary64 partial sum | Sum plus compensation |
| AVG state | Exact dyadic sum + exact count | Binary64 partial + exact count | Compensated partial + exact count |
| Determinism | Bit-identical across order/tree/workers/spill | Result may vary over a precisely admitted reduction tree | Still generally tree/order-dependent |
| Serial cost | High relative to hardware addition | Lowest | Moderate |
| Parallel Combine | Exact but relatively expensive | One binary64 addition per partial | Nontrivial; many compensated schemes lack a simple equivalent Merge |
| State size | Large or variable | One binary64 plus flags/count | Usually two or more binary64 values plus flags/count |
| Spill | Exact accumulator serialization or raw replay | Store exact binary64 partial bits or replay | Preserve compensation state exactly |
| Implementation difficulty | High; specialist numerical work | Low; core aggregate machinery | Medium |
| Verification | Exact mathematical oracle and correctly rounded conversion | Legal-reduction-tree oracle | Algorithm-specific numerical oracle |
| Portability | Requires carefully implemented exact state/conversion | Straightforward with frozen IEEE environment | Requires precise algorithm and compiler controls |
| Modern-engine relevance | Uncommon as default floating SUM | Common default behavior | Common as an optional accuracy-enhanced aggregate |
| Educational value | High numerical-computing value, less central to relational execution | Focuses on Update/Combine/spill/parallel mechanics | Teaches numerical stability but complicates Merge |
| Performance relevance | Potentially material overhead in a hot aggregate | Best baseline performance | Better accuracy at moderate cost |
| Replacement risk | None if exact reproducibility remains a goal | API permits a later separate exact/compensated aggregate | Likely replacement if exact reproducibility is later required |
| Architecture edits | None if retained | Rewrite FLOAT state, invariance, vectors, prohibitions, spill | Specify one exact compensated algorithm and Merge |
| Verification changes | Keep exact-bit invariance | Replace exact-result invariance with admitted-tree conformance | Add algorithm-specific state and error-bound tests |

Modern practice supports treating F1 as exceptional rather than foundational:

- DuckDB documents ordinary floating `sum` and `avg` as order-affected; even its Kahan-based `fsum`/`favg` remain order-affected. [DuckDB aggregate documentation](https://duckdb.org/docs/lts/sql/functions/aggregates)
- SQLite describes floating `sum` as an approximation and provides exact summation only through a separate extension, with additional CPU and memory cost. [SQLite aggregate documentation](https://www.sqlite.org/lang_aggfunc.html)
- PostgreSQL exposes floating SUM/AVG with partial aggregation support but does not promise an exact, execution-shape-independent n-ary result. [PostgreSQL aggregate documentation](https://www.postgresql.org/docs/current/functions-aggregate.html)

## Recommended F2 contract

**Choose F2: ordinary IEEE-754 binary64 partial aggregation, with a bounded legal-reduction model.**

This should not be documented merely as “usual floating-point variation.” A precise contract is:

```text
For one FLOAT64 SUM/AVG group:

1. Every demanded non-NULL input occurrence participates exactly once.
2. Finite inputs are binary64 leaves.
3. Update and Combine may form any finite binary reduction tree over those leaves.
4. Every internal addition is IEEE-754 binary64 round-to-nearest,
   ties-to-even, with one binary64 result at that node.
5. Vector boundaries, worker partitioning, Merge tree, and spill replay may
   choose different legal trees.
6. No extended-precision hidden state, altered rounding mode, unsafe FMA
   contraction, input omission, duplication, or non-binary64 accumulator is
   permitted unless separately capability-defined.
7. A finite result is the root value of the selected legal tree.
8. A NaN root is normalized to the Chapter-17 canonical NaN.
9. A zero final result is normalized to +0.0.
10. AVG divides the selected finite binary64 subtotal’s exact dyadic value by
    the exact non-NULL count and rounds that rational once to binary64,
    nearest/ties-even.
```

Special input policy should remain deterministic:

```text
any input NaN                         -> canonical NaN
both input infinity signs present     -> canonical NaN
only +Infinity present                -> +Infinity
only -Infinity present                -> -Infinity
otherwise                             -> finite-input legal reduction
```

That preserves the clear current behavior for explicit NaN/infinity inputs while allowing finite partial sums to vary by admitted reduction shape. If a finite-only legal reduction overflows or creates a nonfinite intermediate, ordinary IEEE node semantics apply; a final NaN is canonicalized.

This choice teaches the central database mechanisms—vector Update, partial state, Combine, spill, and parallel reduction—without requiring a specialist exact summation subsystem.

F3 is not recommended. It adds real state and Merge complexity but still cannot provide F1’s invariant result. It is better introduced later as a separately named accuracy-enhanced aggregate than made the baseline SUM/AVG contract.

# 7. D29-1 downstream edit impact

If F2 is selected:

| Surface | Required future change |
|---|---|
| §29.3.1 | Replace `ExactFiniteDyadic`/exact finite FLOAT state with binary64 partial-reduction state; retain exact AVG count and special flags |
| §29.3.3 | Integer COUNT/SUM/AVG remain unchanged |
| §29.3.4 | Replace exact accumulation and one exact-sum rounding with the legal binary64 reduction-tree contract |
| §29.3.5 | Retain deterministic explicit-input NaN/infinity rules and canonical final NaN/+0; clarify finite-reduction nonfinite intermediates |
| §29.3.7 | Remove bitwise FLOAT invariance across workers/chunks/Merge/spill; require every result to belong to the admitted reduction model |
| §29.3.8 | Keep fixed empty/special/single-value cases; convert order-sensitive vectors into tree-specific examples or permitted-result sets |
| §29.3.9 | Remove prohibitions against binary64 partial sums and shape-dependent low bits; retain prohibitions on omitted/duplicated values, hidden extended precision, invalid rounding modes, and state corruption |
| Spill | Permit exact serialization of binary64 partial bits or raw-value replay; replay/partitioning may select another legal reduction tree |
| Parallel Combine | Combine partials with one specified binary64 addition per tree edge; schedule/tree may affect finite result |
| Constant evaluation | Must use the same special rules and one legal reduction tree; it need not reproduce every physical runtime tree’s bits |
| Verification | Replace exact mathematical-output equality with independent legal-tree conformance; force known trees and confirm each leaf occurs exactly once |
| §17.4.3 | Replace the statement that aggregate SUM/AVG accumulate finite inputs exactly; continue separating scalar and aggregate contracts |
| §39.3.2 | Replace its exact-n-ary summary with the selected Chapter-29 reduction contract |

Unchanged regardless of D29-1:

- Chapter-17 FLOAT64 equality and total order
- hash normalization
- grouping equality
- N29-1/N29-2 canonical representative decisions
- MIN/MAX canonical `+0.0`/NaN behavior
- NULL and empty-input behavior
- accepted overloads
- exact integer COUNT/SUM/AVG semantics
- memory/error/cancellation owners

# 8. M29-2 decision

**NO USER DECISION REQUIRED.**

The proposed repair composes directly with Chapters 24, 26, and 31.

## Exact recommended repair

```text
PhysicalSortAggregate retains only the active ordered group's mutable
aggregate state.

When a group closes, the operator numerically finalizes and validates that
group. A successful finalized output row is copied into query-owned,
memory-accounted, spill-capable temporary row storage in the required group
order. It is not offered to a dependent pipeline or externally exposed.

The operator continues over every demanded group, retaining the lowest
semantic aggregate-ordinal failure candidate. If any group fails numerical
finalization, execution discards the retained output and fails without an
aggregate-row prefix.

After input exhaustion, successful validation of every group, and successful
Finalize, the retained output storage becomes the operator's Source. Its
in-memory and spilled iteration preserves every OrderingProperty advertised
by PhysicalSortAggregate.

The current-group memory statement applies only to mutable aggregate-state
memory. It does not exclude the finalized-output spool, its metadata, or its
spill buffers.

Failure to retain, spill, or replay output uses Chapter-24/39
representability, OutOfMemory, SpillIOError, or cancellation rules.
```

This uses existing mechanisms:

- `RowLayout`
- `RowCollection`
- query accounting
- `SpillManager`
- blocking readiness
- successful Finalize publication
- external result ownership

No new policy, persistent format, or transaction rule is needed.

# 9. M29-1 and N29-5

## M29-1

**DIRECT FIX — NO USER DECISION.**

Recommended rule:

```text
Baseline hash PhysicalDistinct is a blocking operator:

    Sink
    -> complete demanded input/build
    -> successful Finalize
    -> Source.

It emits no row while constructing the hash duplicate-class state.
Valid nonexecution or early-stop follows Chapters 20 and 26; observing one
duplicate class is not independently sufficient.
```

This is already implied by §38.16’s blocking boundary and only needs to be owned directly by Chapter 29.

## N29-5

**DIRECT FIX — NO USER DECISION.**

Replace chronology with timeless capability language:

- descriptor-specific serialized partial-state spill is eligible only when safe temporary serialization and Combine are defined;
- ordered DISTINCT and `PhysicalSortAggregate` are capability-conditional alternatives;
- the planner enumerates them only when the runtime capability and required physical properties are satisfied.

# 10. Final decision table

| ID | Recommended choice | User must decide? | Reason | Future edit owner | Verification impact |
|---|---|---:|---|---|---|
| N29-1 | Canonical grouping representative | Yes | Prevent SQL-visible first-encounter dependence | Ch17 definition; Ch20 semantics; Ch29 execution | Add ±0/NaN hash/sort/worker/spill oracle |
| N29-2 | Componentwise canonical DISTINCT row | Yes | Preserve hash/ordered substitutability | Ch17/20; Ch29 delegation | Add duplicate-class representative matrix |
| D29-1 | F2 bounded ordinary binary64 reduction | Yes | Best core-DB learning/performance balance | Ch17, Ch29, §39 summaries | Replace exact-bit shape invariance with legal-tree oracle |
| M29-2 | Current-group mutable state plus finalized-output spool | No | Existing Ch24/26/31 mechanisms settle it | Ch29 with owner references | Add spool/readiness/order/resource tests |
| M29-1 | Blocking hash DISTINCT Sink → Finalize → Source | No | Already implied by frozen planning/demand contracts | Ch29 | Add blocking/readiness/early-stop tests |
| N29-5 | Timeless capability wording | No | Pure document-role repair | Ch29 | Chronology audit only |

# 11. Copy-paste decision block

```text
CHAPTER-29 DECISIONS

N29-1:
Use a canonical FLOAT64 grouping-key representative:
-0.0 and +0.0 materialize as +0.0, and every grouping-equivalent
NaN materializes as the Chapter-17 canonical quiet NaN. Other types
retain their existing exact semantic representation.

N29-2:
DISTINCT emits one componentwise canonical row per grouping-equivalence
class, using the same canonical scalar representative rule as GROUP BY.
It preserves the child schema and LogicalSlotIds.

D29-1:
Adopt F2, ordinary IEEE-754 binary64 partial SUM/AVG under a precisely
bounded legal binary-reduction-tree contract. Vector, worker, Combine,
and spill shape may alter finite low bits, but every demanded non-NULL
occurrence participates exactly once; every reduction node uses binary64
round-to-nearest, ties-to-even; explicit NaN/infinity input policy remains
deterministic; final NaN and zero are canonicalized; AVG uses the selected
binary64 subtotal and exact count.

M29-2:
NO USER DECISION REQUIRED. PhysicalSortAggregate keeps approximately one
active group's mutable aggregate state and retains each successfully
finalized group row in query-owned, memory-accounted, spill-capable,
order-preserving temporary row storage. No retained row is externally
exposed until all demanded input and every group's numerical validation
succeed. After successful Finalize the retained output becomes the Source;
resource failures remain Chapter-24/39 outcomes.
```

No Architecture edit was made.
No Verification edit was made.
No implementation occurred.
No build, test, sanitizer, or benchmark was run.