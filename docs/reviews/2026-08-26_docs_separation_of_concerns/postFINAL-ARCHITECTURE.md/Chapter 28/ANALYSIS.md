# Chapter 28 review verdict

**CHAPTER 28 — TARGETED DOCUMENT FIXES RECOMMENDED**

Architecture semantics are consistent. One document-only temporality/development-sequencing finding exists.

- BLOCKING: **0**
- MAJOR: **0**
- MINOR: **1**
- EDITORIAL: **0**
- Frozen Chapter-28 semantic questions: **NONE**

## Repository state

| Check | Initial | Final |
|---|---|---|
| HEAD | `8a67b790ad38b942623310a2da61e0c4ffd4efe6` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | not applicable initially | PASS |
| Audit-created changes | — | NONE |

Only read-only commands were used. Historical review artifacts were unread, unmodified, unmoved, and unstaged.

## Live chapter structure

- Exact title: `# 28. Join Execution`
- Start: line **21486**
- End: line **21868**
- Next heading: `# 29. Aggregation and DISTINCT`
- Chapter-29 boundary: line **21869**

### Complete subsection inventory

| Section | Exact heading | Responsibility | Semantic owner | Runtime owner | Consumer | Classification |
|---|---|---|---|---|---|---|
| 28.1 | Join algorithm roles | Physical join vocabulary and intended algorithm roles | Ch20/22/37 | Ch28 | optimizer/pipeline | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 28.2 | Central query-local hash semantics | Runtime hash/equality compatibility | Ch17 | Ch28 runtime utility | joins/aggregation | ARCHITECTURE-APPROPRIATE |
| 28.2.1 | Equality modes | Ordinary join versus grouping NULL treatment | Ch17/20/29 | hash utility | joins/aggregates | ARCHITECTURE-APPROPRIATE |
| 28.3 | Nested-loop join | Right materialization and left probe execution | Ch20/22 | PhysicalNestedLoopJoin | downstream pipeline | ARCHITECTURE-APPROPRIATE |
| 28.4 | Index nested-loop join | Indexed candidate generation, MVCC, residual qualification | Ch20/22/27/36 | PhysicalIndexNestedLoopJoin | downstream pipeline | ARCHITECTURE-APPROPRIATE |
| 28.5 | Hash-join shape and preserved-side orientation | Inputs, join types, and LEFT orientation | Ch20/22/37 | PhysicalHashJoin | hash build/probe | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 28.6 | Hash-join build storage and directory | RowCollection payload and collision-safe duplicate chains | Ch17/23/24 | hash build state | probe phase | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 28.7 | Hash-build pipeline and finalization | Build sink, NULL-key exclusion, immutable publication | Ch24/26/32 | HashJoinBuild/global state | probe dependency | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 28.8 | Probe continuation and residual predicates | One-input/multiple-output continuation | Ch20/25/26 | local probe state | downstream operator | ARCHITECTURE-APPROPRIATE |
| 28.9 | LEFT hash join | Match and NULL-extension behavior | Ch20 | PhysicalHashJoin | downstream pipeline | ARCHITECTURE-APPROPRIATE |
| 28.10 | Grace hash-join spill | Partitioned build/probe spill | Ch24 | PhysicalHashJoin/SpillManager | partition probe | ARCHITECTURE-APPROPRIATE |
| 28.11 | Recursive repartition and skew fallback | Bounded repartition and exact fallback | Ch24 | hash join runtime | downstream pipeline | ARCHITECTURE-APPROPRIATE |
| 28.12 | Merge join | Capability-conditional ordered join execution | Ch20/22/37/38 | PhysicalMergeJoin | downstream pipeline | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 28.13 | Join invariants | Cross-section invariant summary | referenced owners | Ch28 operators | validator/Verification | ARCHITECTURE-APPROPRIATE |

No non-Architecture material beyond the chronology/development-sequencing wording was found.

## Context consulted

Architecture context:

- §§17.7, 17.10
- §§19.1–19.7
- §§20.1–20.8, 20.14, 20.17–20.18
- §§21.16, 21.18–21.20
- §§22.1–22.8
- §§23.1–23.14
- §§24.1–24.11
- §§25.1–25.8
- §§26.1–26.10
- Chapter 27 scan/source handoff
- §29.1 immediate blocking/finalization boundary only
- Chapter 30 spill/order boundary as needed
- §§31.9–31.10
- §§32.1–32.5, 32.8
- §§36.11–36.16
- §§37.2–37.5, 37.7–37.18
- §§38.8–38.12, 38.18–38.24
- §§39.1–39.4

Verification context:

- V17 equality/hash procedures
- V20 join/schema/demand procedures
- V22 physical operator and capability matrices
- V23 representation/borrowing procedures
- V24 memory/spill procedures
- V25 expression/error procedures
- V26 continuation/finalization procedures
- V27 scan/index handoff
- existing `Hash Join Tests`
- optimizer join-estimation/order procedures

Chapter 29 was not reviewed.

# Operator and algorithm inventory

| Operator | Types | Logical left/right and physical roles | Runtime phases | Continuation | Ordering |
|---|---|---|---|---|---|
| `PhysicalNestedLoopJoin` | INNER, LEFT, CROSS | Logical right materialized; logical left probes | blocking right build, streaming left probe | probe row/right-row cursor | No property unless separately guaranteed |
| `PhysicalIndexNestedLoopJoin` | INNER and LEFT applicability; CROSS inapplicable | Outer is logical left; indexed inner is logical right | streaming outer plus batched inner lookups | outer row/index candidate/output cursor | Only an explicitly guaranteed Ch37 property |
| `PhysicalHashJoin` | INNER, LEFT | INNER may build either side; LEFT fixes right build/left preserved probe | build sink, Finalize, streaming probe; spill phases | probe row/duplicate chain/residual/matched state | None |
| `PhysicalMergeJoin` | Capability-conditional equality execution; LEFT semantics explicitly constrained | Logical semantics remain independent of physical cursor roles | ordered binary streaming with duplicate-group state | duplicate-group cursor | Only capability-proven Ch37 property |

No physical RIGHT, FULL, SEMI, or ANTI join is part of Chapter 28’s v1 operator contract. Semi/anti alternatives mentioned in optimizer context remain capability/proof-gated and cannot replace mandatory subquery fallbacks.

## Pipeline roles

| Phase | Role | Contract |
|---|---|---|
| Nested-loop right materialization | Blocking sink/state | Stable, accounted RowCollection |
| Nested-loop left processing | Streaming | One accepted left input may yield multiple outputs |
| Hash build | Sink | Each submitted domain accepted once |
| Hash Finalize | Semantic finalization | Publishes immutable probe-ready state |
| Hash probe | Streaming | One probe input may produce multiple chunks |
| Grace partition write/read | Blocking/spill | Exact one-partition membership and replay |
| Merge join | Streaming/ordered algorithm | Capability-gated, incremental duplicate groups |
| LEFT unmatched probe row | Streaming continuation | Emitted after all candidates fail |
| Unmatched build final source | N/A | RIGHT/FULL are unsupported; LEFT builds the nonpreserved side |

# Canonical owner matrix

| Contract | Owner | Chapter-28 responsibility | Status |
|---|---|---|---|
| Logical join bags/types | Ch20 | Execute selected algorithm | EARLIER OWNER |
| SQL equality, NULL, FLOAT, VARCHAR | Ch17 | Hash and compare compatibly | EARLIER OWNER |
| Bound slots/provenance | Ch19/20 | Use resolved mappings | EARLIER OWNER |
| Physical algorithm eligibility | Ch22/36–38 | Execute only validated alternative | EARLIER/LATER OWNER |
| Build/probe orientation | Ch28/37 | Enforce selected legal orientation | CHAPTER 28 OWNS |
| Output schema/LogicalSlotIds | Ch20/22 | Materialize declared schema | EARLIER OWNER |
| Hash collision handling | Ch28 | Full key equality before match | CHAPTER 28 OWNS |
| Duplicate-chain preservation | Ch28 | Retain all build occurrences | CHAPTER 28 OWNS |
| Residual scalar semantics | Ch17/20/25 | Evaluate only demanded candidates | EARLIER OWNER |
| Continuation lifecycle | Ch26 | Maintain join-specific cursor | SHARED HANDOFF |
| Build ownership/accounting | Ch23/24 | Own retained rows and strings | SHARED HANDOFF |
| Spill ownership/errors | Ch24 | Preserve join partition semantics | SHARED HANDOFF |
| Build readiness/Finalize | Ch26/32 | Publish immutable state after success | SHARED HANDOFF |
| OrderingProperty | Ch37 | Advertise only guaranteed order | LATER OWNER |
| D25-S1/D21-S4 | Ch25/21 | Preserve candidates and provenance | EARLIER OWNER |
| External publication | Ch31 | Produce internal output only | LATER OWNER |
| Transaction consequences | §39/Ch21 | Propagate errors; no local decision | LATER OWNER |

No duplicated or ambiguous normative owner was found.

# Join semantics

- Supported logical join types: **INNER, LEFT, CROSS**.
- INNER emits one occurrence per TRUE logical pair. Duplicate multiplicity is exact mathematical `L × R`; no deduplication is allowed.
- CROSS emits the exact Cartesian product.
- LEFT emits every TRUE pair, or exactly one right-side NULL-extended occurrence when no TRUE pair exists.
- FALSE and UNKNOWN are nonmatches.
- RIGHT, FULL, SEMI, ANTI, and null-aware anti join: outside Chapter-28 v1 execution scope.
- NULL extension retains declared TypeIds and LogicalSlotIds while setting validity false.
- A hash collision, key-equality candidate with residual FALSE, or residual UNKNOWN does not establish a match.
- Unmatched LEFT output occurs exactly once and only after all applicable candidates have failed.

## Hash/equality compatibility

| Type/state | Equality/hash rule | Recheck | Status |
|---|---|---|---|
| BOOLEAN | Exact FALSE/TRUE equality | Full equality | CONSISTENT |
| INT32/INT64 | Bound common-type equality | Full equality | CONSISTENT |
| FLOAT64 zero | `-0.0 = +0.0`; hashes compatible | Full equality | CONSISTENT |
| FLOAT64 NaN | Canonical NaNs compare equal and hash compatibly | Full equality | CONSISTENT |
| DATE | Signed day value | Full equality | CONSISTENT |
| TIMESTAMP | Signed microsecond value | Full equality | CONSISTENT |
| VARCHAR | Exact length and bytes, including NUL | Full equality | CONSISTENT |
| Composite | Ordered components and NULL markers | All components | CONSISTENT |
| Any ordinary NULL component | Nonmatchable key | No NULL-payload read | CONSISTENT |

Different keys sharing a 64-bit hash continue the open-addressing probe sequence and remain distinct directory entries. Hash equality is candidate generation, never SQL equality by itself.

Duplicate equal build keys share a physical chain but retain every logical build occurrence. Runtime build-row handles are occurrence handles, not SQL identity, RID identity, persistence identity, or an error-ranking key.

# Build/probe and schema

- Logical left/right and physical build/probe are explicitly distinct.
- INNER may choose either build orientation only when output-slot semantics remain preserved.
- LEFT fixes logical right as build and logical left as preserved probe.
- Build/probe swapping never changes declared output-column sequence, LogicalSlotIds, nullability, or left/right semantic provenance.
- Join output follows the declared physical schema implementing logical left columns followed by logical right columns.
- Duplicate display names and equal values remain distinct through LogicalSlotIds.
- Self-joins use distinct BindingIds/LogicalSlotIds even when both sides reference the same table or RID.
- Predicate slots map through resolved physical schemas; runtime name lookup is forbidden.
- RequiredSlotSet may prune only values irrelevant to keys, residuals, output, unmatched handling, and demanded evaluation.

Build state owns or validly retains all keys, payload, residual-only columns, output columns, duplicate links, and VARCHAR bytes. Probe input remains stable until continuation no longer depends on it or required state has been copied. Output borrowing remains valid through downstream consumption.

# Continuation and lifecycle

One accepted probe input may produce zero, one, or arbitrarily many output chunks. Continuation resumes at the first unprocessed build candidate for the current probe row.

It must not:

- reaccept the probe input;
- restart the duplicate chain;
- skip remaining matches;
- admit new input while the prior lifecycle is unresolved;
- release probe backing while still referenced;
- overwrite output still borrowed downstream.

Empty output is nonterminal when finite candidate/cursor progress occurred. Final nonempty output is offered and handed off before terminal completion.

Cancellation or terminal error invalidates current output and continuation. Retry uses fresh build tables, cursors, duplicate-chain state, match state, spill state, Finalize state, and error candidates.

# Empty input and early demand

| Case | Result | Probe/build demand |
|---|---|---|
| Nested-loop/hash INNER, build/right empty | Empty | Once emptiness is established, opposite-side row production is not semantically required |
| CROSS, materialized right empty | Empty | Left row production may stop once exact right emptiness is established |
| LEFT, right/build empty | One NULL-extended row per left occurrence | Preserved left/probe remains demanded |
| Index-NL LEFT, no qualifying inner candidate | One NULL-extended row per outer occurrence | Outer demanded; each demanded lookup proves no match |
| `LIMIT 0` above join | Empty after count acquisition | No relational build/probe work is demanded merely to obtain EOS |
| Positive LIMIT reached mid-chain | Required prefix only | Remaining chain and later probe inputs become undemanded |
| Early stop during LEFT unmatched work | No further output required | Cleanup remains mandatory |

Probe-side scalar/source errors after an exact INNER/CROSS empty-side result becomes established are not demanded merely to confirm EOS. This follows Chapter 20’s pair-demand and exact-emptiness rules plus Chapter 26’s early-stop contract.

For hash join, downstream `LIMIT 0` can prevent build admission entirely unless separate statement work is independently required. Hash build is not unconditionally demanded.

# Nested-loop and index-NL results

Nested-loop materializes the right input once in stable, accounted storage. It does not assume implicit source rewindability. Each logical left occurrence scans the materialized right occurrence domain; continuation spans right rows and output chunks without requiring the cross product to fit in memory.

Index nested-loop treats index hits only as candidates. Each candidate undergoes heap-reference validation, MVCC visibility, and residual evaluation. LEFT unmatched output is produced only after no inner candidate survives.

Both must preserve the same successful logical result and demanded-error semantics as other eligible algorithms.

# LEFT match tracking

LEFT hash join preserves probe-side matched state per logical probe occurrence:

| Candidate outcome | Counts as match? | Output |
|---|---:|---|
| No bucket/candidate | No | One NULL-extended row |
| Same hash, unequal key | No | Continue; unmatched if no later match |
| Exact key, residual FALSE | No | Continue |
| Exact key, residual UNKNOWN | No | Continue |
| Exact key, residual error | No successful match | Canonical error |
| One residual TRUE | Yes | One joined occurrence |
| N residual TRUE | Yes | N joined occurrences |
| NULL probe key | No | One NULL-extended row |

There is no build-side unmatched-finalization phase in the supported LEFT orientation because the build/right side is not preserved. The parallel unmatched-build barrier is therefore N/A for live Chapter 28.

# Ordering

- Logical joins create no row ordering.
- `PhysicalHashJoin` advertises no ordering.
- Hash seed, bucket layout, duplicate-chain layout, build insertion order, spill partition order, and worker schedule are nonsemantic.
- Nested-loop, index-NL, and merge join may advertise only an ordering explicitly guaranteed by their capability-enabled runtime contract and validated under Chapter 37.
- Physical duplicate-key order creates no hidden SQL tie order.
- LEFT hash spill order may change without violating semantics because no ordering is advertised.
- Build/probe swaps cannot silently change a required property.
- Parallel execution must preserve the required ordering class or the plan must include an ordering provider.

# Memory, resource, and spill

All hash directories, RowCollections, keys, payload, duplicate chains, continuation metadata, spill buffers/directories, and worker-local dynamic state are Chapter-24-accounted.

- Exact extents are checked before allocation or addressing.
- Large VARCHAR keys use complete bytes and may not be clipped.
- Ordinary RowLayout limits do not become SQL row limits.
- Joined rows exceeding an ordinary layout use an exact alternative or controlled representability/resource failure.
- `L × R` need not be formed in finite-width arithmetic.
- Output streams incrementally across chunks; no fixed result-cardinality ceiling is created.
- Supported representation allocation denial is OOM.
- No supported exact representation is a controlled representability `ExecutionError`.
- Spill failures are `SpillIOError`, not persistent corruption.
- Grace build and probe use identical partition functions.
- Every ordinary key-bearing row enters exactly one compatible partition.
- Repartition progress is bounded; skew fallback remains exact and accounted.
- Runtime fallback does not authorize changed semantics or ordering claims.

The controlled skew fallback in §28.11 is architecture-authorized. Generic OOM-triggered switching between otherwise selected join algorithms is not authorized.

# Error, cancellation, and retry

Hash-key and residual expression errors retain Chapter-25 expression semantics and provenance. Ordinary SELECT uses D25-S1; DML source plans retain D21-S4 candidate metadata.

Probe row, build row, hash bucket, chain position, chunk, worker, or discovery order cannot rank errors. Hash pruning is legal only because the Chapter-17 compatible hash proves different-hash keys cannot be equal; residual evaluation remains demanded for every exact key candidate unless an upstream proof removes it.

Current failed output is nonconsumable. Earlier internal handoffs remain internal; previously returned client prefixes remain Chapter-31-owned and do not imply whole-query success.

Build failure or cancellation prevents successful Finalize and probe readiness. Probe cancellation stops continuation. No join operator decides transaction commit, abort, or retry eligibility.

# Algorithm equivalence

| Observable | Nested loop / index-NL / hash / merge when all eligible and successful |
|---|---|
| Values and NULLs | Equal |
| Row bag and duplicate multiplicity | Equal |
| LEFT null extension | Equal |
| Output schema and LogicalSlotIds | Equal |
| Required order | Same allowed property class |
| Demanded semantic errors | Same canonical owner/result |
| D25-S1 / D21-S4 | Preserved |
| Transaction/result semantics | Preserved |
| Chunk boundaries | May differ |
| Memory/resource feasibility | May differ |
| Spill behavior | May differ |
| Physical visitation/order without property | May differ |

No generic dynamic fallback exists beyond §28.11’s explicit per-partition skew fallback.

# Required matrices

## Join-type matrix

| Type | Preserved side | Match/no-match | Schema | Algorithms |
|---|---|---|---|---|
| INNER | none | Every TRUE pair; no unmatched output | left then right | NL, INLJ, hash, capability merge |
| LEFT | logical left | TRUE pairs or one right-NULL row | left then nullable right | NL, INLJ, hash, capability merge |
| CROSS | none | Cartesian product | left then right | NL |
| RIGHT/FULL | N/A | Unsupported | N/A | N/A |
| SEMI/ANTI | N/A | Not Chapter-28 v1 operators | N/A | N/A |

## Inner multiplicity matrix

| Inputs | Qualifying pairs | Output | Dedup |
|---|---:|---:|---|
| `0 × N` | 0 | 0 | forbidden |
| `1 × 1` | 1 | 1 | forbidden |
| `1 × N` all match | N | N | forbidden |
| `M × 1` all match | M | M | forbidden |
| `M × N` all match | `M×N` mathematically | `M×N` streamed | forbidden |
| Repeated CONSTANT/DICTIONARY occurrences | per logical occurrence | full multiplicity | forbidden |
| Residual subset | number of TRUE pairs | same | forbidden |

## Algorithm matrix

| Algorithm | Blocking | Streaming | Residual | Spill | Property |
|---|---|---|---|---|---|
| Nested loop | Right materialization | Left probe | Yes | Chapter-24 storage as applicable | Explicit guarantee only |
| Index NL | No complete build | Outer probe/lookups | Yes | Not join-spill-defined | Explicit guarantee only |
| Hash | Build + Finalize | Probe | Yes | Grace/repartition | None |
| Merge | Ordered-input setup as capability requires | Incremental duplicate groups | Exact predicate applicability | Not specified locally | Capability-proven only |

## Continuation matrix

| Case | Resume/lifecycle |
|---|---|
| Zero matches | Resolve input after exact no-match handling |
| One match | Emit once, then advance probe |
| Capacity+1 matches | Resume first unoffered match |
| Residual rejects candidates | Advance chain without false match |
| Empty progress | Cursor must advance |
| Early stop mid-chain | Remaining work abandoned safely |
| Error/cancel mid-chain | Current output invalid; no resume |
| Final output | Output handoff before terminal |

## Build ownership matrix

| State | Owner/lifetime |
|---|---|
| Fixed-width keys | Build RowCollection/global state |
| VARCHAR keys/payload | Deep-copied or equivalently stable build owner |
| Residual-only columns | Retained while residuals can run |
| Output-only columns | Retained through output handoff |
| Duplicate occurrences | Individually represented or exact multiplicity encoding |
| Directory/chains | Accounted join state |
| Local/global build | Stable through Combine/Finalize/probe |
| Output borrowing build row | Build survives downstream borrow |
| Cleanup | After probe/borrow dependencies end |

## Output-schema matrix

| Case | Result |
|---|---|
| Build left versus build right INNER | Same declared left-then-right schema |
| LEFT hash | Logical left then nullable logical right |
| Self-join | Distinct BindingIds/LogicalSlotIds |
| Duplicate names | Allowed; no runtime name resolution |
| Equal-valued columns | Remain distinct output slots |
| Null extension | TypeId/slot preserved; validity false |

## Ordering matrix

| Case | SQL guarantee |
|---|---|
| Hash seed/bucket/chain order | None |
| Hash spill partition order | None |
| Nested-loop traversal | None unless advertised |
| Index-NL traversal | Exact property only if guaranteed |
| Merge join | Capability-proven property only |
| Duplicate ties | No extra hidden order |
| Parallel probe | Bag-equivalent unless ordered plan provides merge |
| Build/probe swap | Must preserve validated property |

## Error-owner matrix

| Origin | Owner | First physical wins? | Current output |
|---|---|---:|---|
| Key/residual expression | D25-S1 or D21-S4 | No | invalid on terminal failure |
| Build/probe OOM | Ch24/39 | Not scalar ranking | invalid |
| Representability | Ch24/39 | Not scalar ranking | invalid |
| Spill I/O | Ch24/39 | Not scalar ranking | invalid |
| Cancellation | Ch26/39 | Not scalar ranking | invalid |
| Child storage corruption | lower storage owner/§39 | No join reinterpretation | invalid |
| Join-state corruption | internal invalid state | N/A | invalid |

## Invalid-state matrix

All of the following are internal/protocol failures rejected before unsafe use or output:

- probe before successful Finalize;
- duplicate build/probe acceptance;
- omitted accepted input;
- invalid continuation cursor;
- wrong key TypeId;
- non-BOOLEAN residual;
- stale build reference;
- directory/chain mismatch;
- output-schema mismatch;
- new input before lifecycle resolution;
- premature unmatched output;
- output after terminal;
- failed-state retry reuse;
- failed current output consumption.

No persistent effect is permitted from these runtime-only states.

## Resource matrix

| Resource | Required classification |
|---|---|
| Directory/entries/arena/duplicates | Exact and accounted |
| Oversized key/row | Exact alternative or representability error |
| Supported allocation denial | OOM |
| Spill framing/I/O | SpillIOError |
| Result cardinality | No SQL limit from counter width |
| Skew recursion | Bounded progress and exact fallback |
| Cleanup | Success/error/cancel/retry teardown |

## Pipeline lifecycle matrix

| Event | Output/readiness |
|---|---|
| Build Sink acceptance | No query success |
| Build exhaustion | Not alone probe readiness |
| Local build completion | Awaits required global work |
| Combine/final directory construction | Includes each build occurrence once |
| Finalize | Publishes immutable ready state |
| Probe acceptance | At most one unresolved input per local state |
| Probe continuation | Multiple outputs without reacceptance |
| Probe completion | Terminal only after pending output |
| Error/cancellation | No later success |
| Cleanup | Resource release, not semantic Finalize |
| Retry | Fresh mutable state |

## Determinism matrix

Chunk capacity, chunk boundaries, empty batches, hash seed, bucket layout, duplicate-chain order, vector representation, worker schedule, combine order, pointer addresses, runtime ordinals, build side, and algorithm choice cannot change values, NULLs, bag multiplicity, null extension, required order, demanded errors, D25/D21 selection, result cardinality, or transaction effects when executions succeed. Resource feasibility and physical output sequence without a property may differ.

## Cross-chapter handoff matrix

| Handoff | Contract | Status |
|---|---|---|
| Ch17→28 | Equality, NULL, hash normalization | CONSISTENT |
| Ch19→28 | Bound sides and provenance | CONSISTENT |
| Ch20→28 | Join bags, schema, demand | CONSISTENT |
| Ch21→28 | DML error/publication envelope | CONSISTENT |
| Ch22→28 | Physical algorithms/capability | CONSISTENT |
| Ch23→28 | Logical occurrences and borrowing | CONSISTENT |
| Ch24→28 | RowCollection, memory, spill | CONSISTENT |
| Ch25→28 | Expressions and D25-S1 | CONSISTENT |
| Ch26→28 | Sink/Finalize/continuation | CONSISTENT |
| Ch27→28 | Child scan/chunk handoff | CONSISTENT |
| Ch28→29 | Join bag enters aggregate | CONSISTENT |
| Ch28→31 | Internal output versus publication | CONSISTENT |
| Ch28→32 | Parallel build/probe readiness | CONSISTENT |
| Ch28→36–38 | Applicability, costing, validation | CONSISTENT |
| Ch28→37 | Ordering/RequiredSlotSet | CONSISTENT |
| Ch28→39 | Failures and transaction consequences | CONSISTENT |

# Documentation-quality audits

## Temporal-language inventory

| Location | Phrase | Classification |
|---|---|---|
| §28.1 | “Their initial roles are” | Project chronology/development sequencing |
| §28.1 | “MergeJoin … added after the simpler join paths are stable” | Project chronology |
| §28.1 | “optimizer chooses the algorithm later” | Ambiguous chronology rather than logical-stage wording |
| §28.3 | “once hash join is available” | Implementation sequencing |
| §28.5 | “Initial join types are” | Project chronology |
| §28.6 | “initial target maximum directory load factor” | Chronology around a durable tuning target |
| §28.7 | “initial INNER/LEFT shape” | Project chronology |
| §28.8 | “current probe row/current build-row handle” | Runtime state; legitimate |
| §28.12 | “later execution algorithm once … paths are stable” | Project chronology |
| §28.12 | “initial implementation may focus” | Development sequencing |
| §28.12 | “may be added” | Future capability narration |

Project chronology occurrences: **10 chronology-bearing phrases** across one coherent finding. The two §28.8 “current” occurrences are legitimate runtime language and are not findings.

Other audits:

- Current implementation narration: none beyond the chronology family.
- Development-owned sequencing: present; included in N28-1.
- Verification procedure leakage: none.
- Project-State leakage: none.
- History/devlog leakage: none.
- Unnecessary ABI/source-layout coupling: none requiring a finding. Physical class names and the open-addressing/load-factor policy describe selected architecture, not ABI layout or persistent format.
- Architecture remains analytical apart from N28-1.

## Terminology dictionary

| Term | Exact meaning/owner | Status |
|---|---|---|
| Logical left/right | Semantic child orientation, Ch20 | precise |
| Build/probe | Physical hash/NL execution roles, Ch28/37 | precise |
| Candidate pair | Pair selected for exact key/residual checking | precise |
| Qualifying match | Complete ON predicate TRUE | precise |
| Hash match | Same 64-bit hash only; not a qualifying match | context makes distinction precise |
| Duplicate chain | All equal-key build occurrences | precise |
| Preserved side | Logical side requiring unmatched output | precise |
| NULL extension | Missing-side validity false, same schema identity | precise |
| Continuation | Execution-local pending work for accepted input | precise |
| Finalize | Semantic build readiness/publication | precise |
| Cleanup | Resource teardown, not Finalize | precise through Ch26 |
| Materialization | Retaining right/build rows in query-owned storage | precise |
| OrderingProperty | Explicit Ch37 row-order guarantee | precise |
| Row occurrence | Bag member independent of equal value/pointer | precise |

Logical left/right is not conflated with build/probe. Candidate and complete match are adequately distinguished. Physical sequence is not conflated with SQL order.

## Normative language and analytical depth

Correctness-critical MUST-level behavior is present for collision recheck, NULL nonmatching, duplicate preservation, LEFT orientation, probe readiness, ownership, partition compatibility, and bounded spill progress. MAY-level freedom remains for hash mixing, physical grouping, continuation representation, fallback mechanism, vector representation, and worker organization.

Analytical rationale is sufficient around:

- hash versus equality;
- duplicate chains;
- preserved-side orientation;
- build ownership;
- immutable probe readiness;
- residual matchedness;
- continuation;
- spill partition compatibility;
- unordered hash output.

The chronology wording should be rewritten as timeless capability and role statements without changing these semantics.

## Explicit cross-reference audit

| Source | Target | Purpose | Quality |
|---|---|---|---|
| §28.2.1 | §17.10.3 | Equality/hash normalization | GOOD |
| §28.10 | Chapter 24 | SpillManager contract | GOOD |
| §28.13(1) | implicit Ch27/10 | Index candidate MVCC handoff | VAGUE BUT HARMLESS |
| §28.13(2–4) | §28.2/17.10.3 | Hash/equality invariants | GOOD |
| §28.13(9) | §28.7/Ch26 | Finalize readiness | GOOD |
| §28.13(11–13) | §28.10–28.11/Ch24 | Spill/repartition | GOOD |

No stale, circular, wrong-owner, or duplicated normative cross-reference was found. Chapter 28 contains no explicit Chapter-29 reference.

# Finding

## N28-1 — chronology and development sequencing in join roles/capabilities

- Severity: **MINOR**
- Primary type: **TEMPORALITY**
- Classification: **DOCUMENT-ONLY**
- Sections: §§28.1, 28.3, 28.5–28.7, 28.12
- Evidence: “initial roles,” “added after … stable,” “chooses … later,” “once hash join is available,” “Initial join types,” “initial target,” “initial … shape,” “later execution algorithm,” “initial implementation,” and “may be added.”
- Owner/handoff affected: Architecture versus Development/Project-State document role.
- Architectural comparison: the same sections already define durable v1 capability gating through Chapters 22, 37, and 38; chronology is unnecessary.
- Consequence: no SQL-semantic divergence, but the chapter cannot stand fully timelessly and may be misread as implementation sequence/current availability.
- Smallest future action: replace those phrases with present-tense durable role, tuning-target, capability-gating, and excluded-scope wording. Preserve all operator semantics and constants.

No other findings exist.

# High-priority hotspot results

| Hotspot | Result |
|---|---|
| Build/probe versus logical sides | Explicitly separated |
| Hash collision equality | Full equality mandatory |
| Duplicate build occurrences | Preserved through chains |
| Ordinary NULL keys | Nonmatchable |
| FLOAT hash compatibility | Exact, including zero/NaN |
| Multi-chunk probe continuation | Required |
| Probe backing | Stable or independently preserved |
| Build strings | Deep-copied/stably owned |
| LEFT matched status | Residual TRUE only |
| Unmatched output | Exactly once after candidate exhaustion |
| Build/probe swap | Cannot alter schema/LEFT semantics |
| Algorithm error equivalence | Ch20/22/25-owned and determinate |
| Empty build demand | Ch20/26 settle it |
| LIMIT-zero demand | Ch20/26/27 settle it |
| NL rewind | Right is explicitly materialized |
| Build Finalize | Required before probe |
| Unmatched-build parallel barrier | N/A; RIGHT/FULL unsupported |
| Ordering property | Ch37-owned |
| Large multiplicity | Incremental; no finite product prerequisite |
| Oversized runtime rows | Ch24 exact alternatives/errors |

# Verification cross-check

Overall Chapter-28 Verification status: **PARTIAL**.

The existing `Hash Join Tests` names broad cases but does not provide a complete atomic ledger, independent oracle specification, or complete deterministic composition methodology.

| Family | Current classification | Main gap |
|---|---|---|
| Operator/type inventory | PARTIAL | No Chapter-28 ledger |
| Logical join bags/schema | COMPLETE via V20 | Reusable |
| Hash equality/NULL/FLOAT/VARCHAR | PARTIAL | Component oracles exist; join composition incomplete |
| Collision/duplicate chains | PARTIAL | Cases named, exact methodology sparse |
| NL/INLJ/hash/merge substitution | PARTIAL | Nested-loop comparison is not sufficient as sole oracle |
| Probe continuation | PARTIAL | Generic V26 complete; match-chain trace incomplete |
| Build/probe ownership | PARTIAL | Ch23/24 reusable; join owner graph incomplete |
| Build Finalize/readiness | PARTIAL | Generic V26/parallel checks reusable |
| Grace spill/skew fallback | PARTIAL | Cases named; exact partition/replay ledger absent |
| LEFT matched/unmatched behavior | PARTIAL | Named, but complete collision/residual matrix absent |
| Output schema/LogicalSlotIds | COMPLETE via V20/V22 | Reusable |
| RequiredSlotSet | PARTIAL | Join key/residual/payload composition incomplete |
| Ordering properties | PARTIAL | Ch37 owner covered; algorithm matrix incomplete |
| D25-S1/D21-S4 | PARTIAL | Owner methods exist; join-pair reduction incomplete |
| LIMIT-zero/early stop | PARTIAL | Generic demand methods exist; join integration absent |
| Empty-build demand | MISSING | Direct deterministic cases absent |
| Resource/count representability | PARTIAL | Ch24 reusable; join cardinality composition absent |
| Cancellation/retry/invalid states | PARTIAL | Generic methods exist; join-specific state matrix absent |
| Worker/hash-seed determinism | PARTIAL | Parallel methods exist; full join oracle absent |

No Verification family is blocked by a semantic question.

# Technical consistency question matrix

The following matrix contains **360 actual Chapter-28 checks**, ten independently classified checks per row. “10C” means ten CONSISTENT checks; “9C/1F” identifies the one document-only finding check.

| IDs | Ten checks covered | Status |
|---|---|---|
| Q001–Q010 | live operators; supported types; capability gates; logical sides; build side; probe side; algorithm owner; plan validation; no semantic reinterpretation; no absent variants | 10C |
| Q011–Q020 | INNER pairs; TRUE; FALSE; UNKNOWN; duplicate left; duplicate right; `L×R`; no dedup; CROSS product; empty CROSS side | 10C |
| Q021–Q030 | LEFT preserved side; all TRUE matches; no-match extension; no extra extension; NULL TypeIds; validity; slot identity; collision nonmatch; FALSE residual; UNKNOWN residual | 10C |
| Q031–Q040 | ordinary NULL key; NULL/NULL; NULL/non-NULL; composite NULL; poison payload; build NULL exclusion; LEFT probe NULL; grouping mode separation; no null-aware anti; no NULL sentinel leakage | 10C |
| Q041–Q050 | BOOLEAN hash; integer hash; DATE; TIMESTAMP; VARCHAR bytes; embedded NUL; FLOAT zero; FLOAT NaN; composite boundaries; equal-implies-compatible-hash | 10C |
| Q051–Q060 | collision candidate; exact equality; unequal collision; duplicate entry chain; representative comparison; chain append; no map overwrite; build occurrence identity; repeated values; runtime handle nonsemantic | 10C |
| Q061–Q070 | output schema; left-before-right; build swap independence; LogicalSlotIds; duplicate names; self-join; same RID both sides; physical ordinal; predicate slot mapping; no name lookup | 10C |
| Q071–Q080 | NL right materialization; stable ownership; no source rewind; left probe; multi-chunk product; right-empty INNER; right-empty LEFT; CROSS; predicate 3VL; NL ordering nonsemantic | 10C |
| Q081–Q090 | INLJ key evaluation; index lookup; heap fetch; MVCC; residual; LEFT unmatched; candidate duplication; continuation; indexed-inner ownership; applicability owner | 10C |
| Q091–Q100 | hash orientation INNER; hash orientation LEFT; no outer swap; key expressions; residual; join type; build input; probe input; preserved side; output independence | 10C |
| Q101–Q110 | RowCollection; RowLayout; key retention; payload retention; cached hash; chain metadata; deep VARCHAR copy; stable handles; no RID identity; query-local lifetime | 10C |
| Q111–Q120 | open addressing; hash mismatch; hash match; head equality; different-key continuation; load threshold tuning; resize preservation; exact extents; accounting; no persistent hash format | 10C |
| Q121–Q130 | build Sink acceptance; key evaluation once per occurrence; NULL exclusion; deep copy; hash storage; duplicate construction; Finalize; immutable publication; dependency gate; build failure | 10C |
| Q131–Q140 | probe acceptance once; current row; current chain handle; matched flag; residual state; capacity crossing; exact resume; backing lifetime; no early new input; final output before terminal | 10C |
| Q141–Q150 | no bucket; collision-only bucket; equality residual FALSE; equality residual UNKNOWN; equality residual TRUE; many TRUE matches; error; LEFT NULL extension; no premature extension; no duplicate extension | 10C |
| Q151–Q160 | Grace build partition; probe partition; identical function; one partition membership; build spill; probe spill; pair processing; NULL probe shortcut; LEFT unmatched per partition; unordered partition output | 10C |
| Q161–Q170 | recursive extra bits; bounded depth; finite progress; skew statistics nonsemantic; exact fallback; accounted fallback; no hard-limit breach; no lost row; no duplicate row; spill cleanup | 10C |
| Q171–Q180 | merge capability gating; equality mode; NULL nonmatch; duplicate groups; incremental output; no complete cross product; LEFT unmatched; exact predicate applicability; property owner; no universal inequality merge | 10C |
| Q181–Q190 | active cardinality; no capacity iteration; CONSTANT multiplicity; DICTIONARY repetition; SelectionVector order; inactive poison; build dictionary insertion; probe dictionary handling; representation freedom; zero-column cardinality if admitted | 10C |
| Q191–Q200 | hash key demand; build expression provenance; probe expression provenance; residual demand; hash pruning proof; no conjunct reorder; collision equality versus residual; D25-S1; D21-S4; no first physical error | 10C |
| Q201–Q210 | many error candidates; chunk reduction; worker reduction; continuation reduction; error after prior match; semi distinction N/A avoided; current output invalid; prior internal handoff; prior cursor prefix; no local transaction outcome | 10C |
| Q211–Q220 | empty INNER build result; empty INNER probe demand; empty CROSS side; empty LEFT build; LIMIT zero; mid-chain Limit; later residual suppression; later probe suppression; unmatched suppression; cleanup | 10C |
| Q221–Q230 | hash join no order; bucket order; seed; insertion order; chain order; spill partition order; NL incidental order; INLJ property gate; merge property gate; duplicate ties | 10C |
| Q231–Q240 | RequiredSlotSet output; key slots; residual-only slots; build output slots; probe output slots; unmatched payload; predicate-only hiding; no demanded-error pruning; payload compression proof; cardinality without payload | 10C |
| Q241–Q250 | directory accounting; row accounting; varlen accounting; chain accounting; metadata accounting; exact multiplication; large key; oversized row; exact alternative; representability versus OOM | 10C |
| Q251–Q260 | result cardinality streaming; no `L×R` prerequisite; chain length unbounded; no chunk-sized semantic cap; spill I/O; malformed spill; temp namespace; no WAL; crash garbage; no runtime-state recovery | 10C |
| Q261–Q270 | parallel local build; every build occurrence once; build barrier; partition Finalize; immutable probe state; probe partition coverage; local continuation; worker order; no early global finish; LEFT preserved semantics | 10C |
| Q271–Q280 | fresh plan/runtime split; fresh table; fresh cursor; fresh flags; fresh spill; fresh candidate errors; cancellation build; cancellation probe; cleanup/quiescence; immutable plan reuse | 10C |
| Q281–Q290 | probe-before-ready; duplicate build acceptance; duplicate probe; missing build; missing probe; invalid cursor; wrong key type; non-BOOLEAN residual; stale pointer; schema mismatch | 10C |
| Q291–Q300 | directory corruption internal; match-state mismatch; output after terminal; failed output consumed; failed state reused; Finalize twice outside protocol; premature unmatched N/A; no unsafe dereference; no persistent effect; controlled failure | 10C |
| Q301–Q310 | algorithm success bag; values; NULLs; LEFT extension; schema; required order; D25; D21; transaction result; resource differences | 10C |
| Q311–Q320 | no generic fallback; skew fallback exact; estimates nonsemantic; zero estimate not proof; capability registry; unsupported algorithm absent; hash seed freedom; table layout freedom; worker freedom; vector freedom | 10C |
| Q321–Q330 | Chapter-29 handoff; aggregate multiplicity; scalar-subquery downstream cardinality; ORDER BY provider; DML source semantics; ResultSink boundary; transaction boundary; persistence-negative registry; runtime cleanup; canonical errors | 10C |
| Q331–Q340 | chronology audit; current-state audit; Development leakage; Verification leakage; Project-State leakage; history leakage; owner precision; terminology; rationale; timelessness | 9C/1F: Q331 |
| Q341–Q350 | explicit references; §17 target; Ch24 target; Chapter-29 reference absence; no circularity; no wrong owner; no stale target; no duplicate owner; capability navigation; boundary precision | 10C |
| Q351–Q360 | implementer can identify types; algorithms; match; schema; continuation; ownership; demand; errors; properties; completion without invention | 10C |

Totals:

- CONSISTENT: **359**
- CONSISTENT BUT SPECIALIZED: included within the applicable capability/LEFT/spill rows above
- FINDING: **1** (`Q331`, N28-1)
- N/A padding: **0**

Absent RIGHT/FULL/SEMI/ANTI behavior was not used to pad the 360-question set.

# Implementer-invention assessment

No correctness-relevant policy must be invented. Existing owners answer:

- candidate and match domains;
- empty-input and early-demand behavior;
- NULL/FLOAT/VARCHAR equality;
- collision rechecks;
- multiplicity;
- schema and slots;
- continuation;
- build/probe ownership;
- Finalize readiness;
- ordering properties;
- memory and spill outcomes;
- errors, cancellation, and retry.

The only required future change is timeless wording.

# Previous-chapter regression

| Frozen chapter | Result |
|---|---|
| Chapter 17 | Equality, NULL, NaN, signed zero, and byte strings unchanged |
| Chapter 19 | Binding/provenance unchanged |
| Chapter 20 | Join bags, LEFT semantics, schema, demand, ordering unchanged |
| Chapter 21 | D21-S4/D21-S5/retry unchanged |
| Chapter 22 | Eligibility, immutable plans, schemas, properties unchanged |
| Chapter 23 | Logical occurrences and borrowing unchanged |
| Chapter 24 | Accounting, representations, spill, progress unchanged |
| Chapter 25 | Expression semantics, D25-S1, failed output unchanged |
| Chapter 26 | Sink, Finalize, continuation, early stop, retry unchanged |
| Chapter 27 | Scan/index candidate handoff unchanged |

No upstream chapter was reopened.

# Frozen Chapter-28 semantic questions

**NONE.**

In particular, there is no unresolved ambiguity concerning:

- logical left/right versus build/probe;
- output schema;
- hash collisions;
- NULL or FLOAT hash keys;
- duplicate multiplicity;
- continuation;
- build ownership;
- LEFT matchedness;
- finalization;
- empty-build demand;
- LIMIT-zero demand;
- nested-loop rewind;
- RequiredSlotSet;
- ordering;
- resource extents;
- retry/cancellation.

# Chapter-29 boundary

Chapter 29 begins at line **21869**:

`# 29. Aggregation and DISTINCT`

Immediate handoff: Chapter 28 produces exact join occurrence bags and internal chunks; Chapter 29 consumes those occurrences for blocking/global or grouped aggregation under its own state, Combine, Finalize, equality, and numerical contracts.

Chapter 28 contains no explicit Chapter-29 reference.

Recommended eventual Chapter-29 review scope: aggregate state APIs, exact update/combine/finalization semantics, grouping equality, numerical determinism, global-empty behavior, spill, DISTINCT, ordering properties, and blocking/pipeline readiness. That review was not started.

# Final status and next action

- Project chronology: **YES — N28-1**
- Current implementation narration: **only as part of N28-1 chronology**
- Development-owned material: **YES — sequencing phrases in N28-1**
- Verification procedure leakage: **NO**
- Project-State leakage: **NO**
- History/devlog leakage: **NO**
- Correctness-relevant implementer invention: **NO**
- Can Chapter 28 stand unchanged as fully timeless canonical v1 Architecture? **NO, because of N28-1; semantics themselves can.**

Recommended next action:

**TARGETED CHAPTER-28 DOCUMENT-ONLY CLEANUP — N28-1 ONLY.**

Do not synchronize Chapter-28 Verification until that cleanup is complete.

Chapter 29 review was not started. No Verification synchronization, implementation, build, test, sanitizer, or benchmark work occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.