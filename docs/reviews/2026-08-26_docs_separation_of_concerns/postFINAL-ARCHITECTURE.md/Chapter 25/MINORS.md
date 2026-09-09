Chapter-25 document/integration cleanup is complete. M25-1–M25-4 and N25-1–N25-3 are CLOSED. Chapter-25 Architecture is CLEAN; Verification synchronization remains pending.

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) changed: 147 insertions and 3 deletions. D25-S1 and all Architecture text outside Chapter 25 are byte-identical to HEAD.

The initial working tree and index were clean at `90d71da09231058399480b115e6c13cfff5d3207`. The preceding D25-S1 integration was already committed, so there was no pre-existing Architecture diff to preserve. Historical review artifacts were unread, unmodified, unmoved, and unstaged throughout this task.

The exact modified sections are:

| Section | Change |
|---|---|
| [§25.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20230) | Generic demand handoff and conceptual API qualification |
| [§25.1.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20370) | New DML expression-error handoff |
| [§25.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20389) | Input-schema mapping and result occurrences |
| [§25.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20427) | Correct arithmetic ownership direction |
| [§25.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20501) | Precise borrowing and retention references |
| [§25.7.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20524) | New valid-state and result-publication handoff |
| [§25.7.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20555) | New representation and resource handoff |
| [§25.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20581) | Six corresponding invariant summaries |

M25-1 is closed by making the generic demand contract explicit. Each invocation receives or semantically determines its demanded logical occurrences. Parents derive child subsets using Chapter-17 control flow and Chapter-20 child order. Nested demand cannot restore an occurrence excluded by an ancestor or omit one for vector-layout convenience. Empty demand creates no per-row semantic evaluation.

The physical demand representation remains implementation-defined. Selections, bitmaps, index lists, partitioned evaluation, or another exact representation may be used. Every occurrence must remain within Chapter 23’s active logical domain. Speculative undemanded work contributes neither visible values nor ordinary error candidates. Constant folding remains §17.10.2-owned.

M25-2 is closed by explicitly preserving the provenance and eligible candidates needed by D21-S4: responsible SourceSpan, semantic origin, public category/cause, and expression-phase membership. Chapter 21 retains eligibility and ranking, including attempt and revalidation prerequisites. Chapter 25 cannot discard a needed DML candidate through D25-S1 reduction or physical visitation order.

RETURNING remains DML, with its unordered-bag semantics referenced precisely at §21.15. Physical sharing and duplication retain upstream provenance; pointers, kernel entries, ordinals, and lanes cannot replace it. No transport structure or new DML precedence was prescribed.

M25-3 is closed by stating the complete input mapping:

```text
LogicalSlotId (§20.2)
    → containing operator’s physical schema (§22.3)
    → schema entry S[j]
    → DataChunk.columns[j] (§23.1)
```

Chapter 25 consumes this resolved mapping. Physical column ordinal is a runtime locator. Reordering and derived-table remapping follow the corresponding schema; self-join inputs and repeated outputs retain their distinct semantic slots.

Successful ordinary scalar evaluation produces one logical result occurrence per demanded input occurrence, preserving correspondence, order, and multiplicity. CONSTANT payload sharing and repeated DICTIONARY indices do not collapse occurrences. Filter owns row removal, Project owns output placement, and temporary expression vectors create no new LogicalSlotId.

M25-4 is closed by explicitly classifying malformed expression state and defining local result publication. Wrong child counts, missing kernels for validated executable expressions, mismatched TypeIds, invalid mappings, malformed selections/validity, expired borrows, and uninitialized demanded output are internal plan/runtime violations. They do not become user scalar errors or D25-S1 candidates.

Chapter 22’s physical-plan contract and §38.24’s actual final-plan validation handoff remain authoritative. Construction invariants and suitable runtime guards may enforce preconditions without full tree validation on every vector call.

Successful evaluation initializes demanded validity, exact non-NULL payload, and required varlen ownership. Failed evaluation cannot publish its current result vector/view as successful or expose incomplete demanded positions. Malformed state must be prevented or rejected before unsafe access, stale publication, dangling dereference, arbitrary mutation, or persistent effects. Legitimate allocation, representability, and cancellation failures retain their existing categories.

N25-1 is closed by replacing the reversed wording that §39 arithmetic behavior “MUST remain consistent with these kernels.” The final text states that kernels execute Chapter-17 scalar semantics; §39 owns runtime enforcement. Cast, comparison, and closed function-registry ownership follows the same upstream direction. No arithmetic rules were duplicated or changed.

N25-2 is closed by replacing the vague pipeline-lifetime reference with §§23.10–23.13 and §26.6. Borrowed payload, validity, selections, representation metadata, StringRef metadata, and bytes must remain value-stable throughout consumption. Reset, reuse, incompatible mutation, and in-place writes must preserve every live view. Synchronous zero-copy remains permitted; retaining consumers obtain stable ownership or materialize under the existing owner rules. No mandatory deep-copy mechanism was introduced.

N25-3 is closed by precise references to §23.9 for large runtime VARCHAR, §24.2 for retained representation, §24.1 for exact extents, §§24.4–24.5 for accounting, and §24.10/§39.3 for errors. Compact StringRef and heap-tuple capacities do not become SQL VARCHAR maxima. Exact alternate forms remain permitted; truncation, wrap, and clipping remain forbidden.

Variable-length output arithmetic covers lengths, offsets plus lengths, allocation extents, and capacity growth. Growing expression scratch, selections, vectors, and output bytes remain accounted through their owner regions. Supported exact allocation denial remains OutOfMemory; unsupported exact representation remains controlled representability/resource ExecutionError. No cross-class error precedence was added.

The demand-handoff audit uses `D` for the parent’s demanded logical occurrences. Every applicable row is closed.

| Form | Parent domain | Child subset owner | Can be empty? | Undemanded error visible? | Physical representation |
|---|---|---|---:|---:|---|
| Ordinary arithmetic | D | §17.6 child order and strictness | yes | no | implementation-defined |
| CAST | D | §17.8 and child evaluation | yes | no | implementation-defined |
| Named scalar function arguments | N/A | Empty §17.9.3 registry | N/A | N/A | No v1 kernel invented |
| AND | D | §17.7.3, within D | yes | no | implementation-defined |
| OR | D | §17.7.3, within D | yes | no | implementation-defined |
| CASE | D | Ordered conditions and selected result under §17.7.3 | yes | no | implementation-defined |
| IN-list | D | §17.7.3 left/list demand and order | yes | no | implementation-defined |

The invalid-state audit distinguishes static legality from runtime integrity. “Static” below means binding/plan construction can establish the condition; runtime corruption or lifetime violations still need the relevant invariant protection. All applicable wording is in §25.7.1.

| State | Binding protection | Physical-plan protection | Ch23 invariant | Ch25 precondition | Public scalar error? | Classification |
|---|---|---|---|---|---:|---|
| Wrong child count | bound shape | static/construction | — | yes | no | internal |
| Wrong input TypeId | resolved types | static schema/type agreement | §23.1 | yes | no | internal |
| Wrong output TypeId | result type | static schema/type agreement | §23.1 | yes | no | internal |
| Missing/unresolved kernel | supported expression | executable capability/construction | — | yes | no | internal |
| Invalid slot mapping | logical identity | physical schema mapping | §23.1 | yes | no | internal |
| Invalid selection | — | runtime-dependent | §23.6 | yes | no | internal |
| Expired/unstable borrow | — | runtime-dependent | §23.10 | yes | no | internal |
| Invalid validity | — | runtime-dependent | §§23.5–23.6 | yes | no | internal |
| Uninitialized demanded output | — | runtime-dependent | §23.14 | yes | no | internal |
| Malformed named function state | no valid v1 function | no valid executable kernel | N/A | unsupported state cannot execute | no runtime scalar error | N/A as valid v1 state; purported executable state is invalid |

The slot-handoff audit is closed for all supported cases.

| Case | Semantic identity | Physical locator / DataChunk access | Result identity owner | Chapter-25 responsibility |
|---|---|---|---|---|
| Source column reference | resolved LogicalSlotId | S[j] → columns[j] | upstream schema | consume mapping |
| Project input | child LogicalSlotId | Project input schema → columns[j] | Project output schema | compute corresponding values |
| Filter input | child LogicalSlotId | Filter input schema → columns[j] | preserved child schema | evaluate predicate |
| Repeated output/reference | distinct declared output slots where applicable | each corresponding schema entry | LogicalProject | preserve distinctions |
| Self-join input | distinct upstream slot identities | containing input schema | owning plan | avoid name/value/pointer identity |
| Derived-table remap | mapped outer slot | remapped physical schema | derived boundary | consume remap |
| Temporary expression vector | no new semantic slot | evaluation-local vector/view | no independent slot required | preserve occurrence correspondence |

The result-occurrence audit is closed. Counts describe successful ordinary scalar evaluation; specialized owners retain their exceptions.

| Case | Logical result count | Result domain | Ownership | Publishable? | Identity owner |
|---|---:|---|---|---|---|
| FLAT with N demanded occurrences | N | corresponding occurrences | output or stable borrow | on success | owning schema |
| CONSTANT with N demanded occurrences | N | logical occurrences, not payload count | plan/output/stable borrow | on success | owning schema |
| Repeated DICTIONARY selection | selection occurrence count | repetitions preserved | stable child/view or owned result | on success | owning schema |
| Partial demand D | count of occurrences in D | corresponding subset | exact result owner | on success | owning schema |
| Empty demand | 0 | empty | no per-row evaluation required | valid empty result | owning schema |
| NULL result | one per demanded occurrence | validity-defined NULL | valid representation | on success | owning schema |
| Borrowed result | one per demanded occurrence | stable mapped view | live stable owner | during valid borrow | owning schema |
| Computed VARCHAR | one per demanded occurrence | exact values and lengths | exact result owner | on success | owning schema |
| Failed evaluation | no successful result | incomplete positions inaccessible | normal cleanup owners | no | no identity change |

The borrow/lifetime audit is closed.

| Case | May borrow? | Required stable owner | Borrow end | Materialization requirement | Canonical owner |
|---|---:|---|---|---|---|
| Pass-through FLAT | yes | reachable payload/validity | consumer completion | only if stability otherwise unavailable | §§23.10, 23.12 |
| Pass-through CONSTANT | yes | scalar payload and metadata | consumer completion | same | §§23.10, 23.12 |
| Pass-through DICTIONARY | yes | selection, child relationship, values | consumer completion | same | §§23.10, 23.12 |
| Borrowed StringRef | yes | metadata and exact bytes | consumer completion | same | §23.10 |
| Computed StringRef | where an exact stable reference is valid | result bytes or stable backing | required result lifetime | computed bytes otherwise obtain ownership | §§23.11, 27.8 |
| Synchronous consumer | yes | producer retained unchanged | synchronous consumption completes | zero-copy permitted | §26.6 |
| Retaining consumer | only with sufficient ownership | retaining owner | retaining owner’s interval | stable ownership or materialization required | §§23.10–23.12 |
| Input reset/reuse | only after live views are preserved | original or replacement stable owner | borrow ends or preservation established | mechanism remains free | §23.13 |

The resource-handoff audit is closed. Accounting applies to owned capacity through the appropriate region; a separately bounded exemption remains governed by Chapter 24.

| Case | Owner | Accounted? | Canonical failure | D25 ordinary candidate? | Reference |
|---|---|---|---|---:|---|
| Fixed temporary vector | execution/output state | yes where query-owned growth | OOM or representability error | no | §§22.6, 24.4–24.5 |
| Selection/mask storage | execution region | yes where query-owned growth | OOM or representability error | no | §§24.1, 24.4–24.5 |
| Computed string bytes | output/stable result owner | yes | canonical resource failure | no | §§23.9, 24.4–24.5 |
| Large exact VARCHAR | supported exact runtime owner | yes | no error solely for exceeding compact form | no | §23.9 |
| Unsupported exact representation | runtime representation boundary | no unaccounted exposure permitted | representability/resource ExecutionError | no | §§23.9, 24.10, 39.3 |
| Supported exact allocation denied | allocating owner | unused charge handled by lifecycle | OutOfMemory | no | §§24.5, 24.10, 39.3 |
| Size/address unrepresentability | consuming runtime domain | exact-before-use | representability/resource ExecutionError | no | §§24.1, 24.10 |
| Cancellation | query execution context | cleanup preserves accounting | QueryCancelled | no | §24.10, §39.3 |

The entire final Chapter 25 passed the documentation-language audit. The requested temporal-keyword scan found only “expression-evaluation phase” in §25.1.2, classified B: DML runtime/semantic phase.

Other surviving sequence and lifetime language is legitimate:

| Location | Language | Class | Meaning |
|---|---|---|---|
| §25.1 | per-execution, per-invocation, empty demand | A | evaluation lifetime/domain |
| §25.1 | first demand, uninitialized occurrence, run once | A | lazy side-plan lifecycle |
| §25.1.1 | first physical discovery, can still be established | A | runtime candidate discovery |
| §25.1.2 | expression-evaluation phase | B | D21-S4 runtime phase |
| §§25.5–25.6 | evaluate A first; after/then combine | A | frozen scalar execution order |
| §25.7 | full interval, until borrow ends, reset/reuse | A | borrow lifetime |
| §25.7.1 | before unsafe access; earlier returned chunks | A | safety and publication sequence |
| §§25.1.1, 25.3, 25.7 | closed v1 scope | D | durable semantic scope |

Project chronology count is **0**. Current implementation narration, Development sequencing, Verification procedures, Project State narration, and history/devlog leakage are all **0**. No chronology rewrite was needed.

Terminology consistently distinguishes semantic expression occurrence, logical row occurrence, physical lane, LogicalSlotId, schema ordinal, DataChunk column, demanded subset, error candidate, result occurrence, and borrowed view. No ambiguous “slot index” identity was introduced.

MUST/MUST NOT language states the inherited correctness boundaries; MAY language preserves choices of demand representation, candidate transport, validation, vector representation, borrowing, and allocation. No concrete Evaluate signature, candidate struct, mask object, map container, allocator, or dispatch table was mandated.

The changed-reference audit is complete below. Every listed target exists. References delegate to the relevant owner; none introduces circular normative ownership.

| Source | Added or corrected targets | Purpose | Result |
|---|---|---|---|
| §25.1 | §§20.6–20.7, 20.17, 20.17.5; Chapter 17 | Parent/child demand and order | precise, owner-correct |
| §25.1 | §17.10.2 | Folding exclusion | precise |
| §25.1 | §§23.1, 23.6, 23.8 | Active domain and normalized selection | precise |
| §25.1 | §25.1.1 | Undemanded speculation creates no candidate | precise local reference |
| §25.1.2 | Chapters 18–20; §20.17 | Preserved diagnostic origin through physicalization | canonical provenance handoff |
| §25.1.2 | §21.16.1 | DML eligibility, phase, and winner | precise |
| §25.1.2 | §25.1.1 | Exclude non-DML reduction | precise |
| §25.1.2 | §21.15 | RETURNING bag semantics | exact live owner |
| §25.2 | §§20.2, 22.3, 23.1 | Slot → schema → DataChunk mapping | precise |
| §25.2 | §§20.6, 27.7 | Filter row-removal ownership | precise |
| §25.2 | §§20.7, 22.3, 27.8 | Project result placement and identity | precise |
| §25.3 | §§17.4.3, 17.6.1–17.6.2 | Arithmetic semantic authority | corrected owner direction |
| §25.3 | §§39.3.1–39.3.2 | Runtime arithmetic enforcement | precise |
| §25.3 | §§17.7–17.8, 19.6 | Resolved comparison/cast execution | precise |
| §25.3 | §17.9.3 | Closed scalar-function scope | precise |
| §25.7 | §§23.10–23.13 | Stable borrowing, preservation, reset/reuse | precise |
| §25.7 | §26.6 | Synchronous consumption interval | exact lifetime owner |
| §25.7 | §§23.11, 27.8 | Computed VARCHAR result ownership | precise |
| §25.7.1 | §§22.2–22.3, 22.8, 38.24 | Physical-plan and final validation handoff | exact live targets |
| §25.7.1 | §§23.6, 23.10, 23.14 | Invalid selection/borrow/runtime state | precise |
| §25.7.1 | §17.9.3 | No valid named scalar-function state | precise |
| §25.7.1 | §39.1 | Internal-invariant consequences | precise |
| §25.7.1 | §31.10 | Earlier cursor output remains unaffected | precise |
| §25.7.1 | §24.10, §39.3 | Legitimate operational failures | precise |
| §25.7.2 | §23.9, §24.2 | Runtime scalar versus retained applicability | precise |
| §25.7.2 | §22.6, §§24.4–24.5 | Runtime ownership and accounting | precise |
| §25.7.2 | §24.1 | Exact extent arithmetic | precise |
| §25.7.2 | §§23.9, 24.10, 39.3; Chapter 17 | Resource classification without scalar redefinition | precise |
| §25.8 | Chapter 17/20; §21.16.1; §§23.10–23.13, 26.6; §23.9; §§24.1–24.5, 24.10; §39.3 | Invariant summaries of the above handoffs | consistent, no competing owner |

The regression comparisons confirmed:

| Protected contract | Result |
|---|---|
| D25-S1 | Entire §25.1.1 byte-identical |
| D20-B1 | Demand membership unchanged |
| D20-B2 | Executable child order unchanged |
| D21-S4 | DML ranking unchanged; no non-DML cause ordering imported |
| D21-S5 | RETURNING remains an unordered bag |
| Chapter 22 | Slot identity and physical addressing unchanged |
| Chapter 23 | Active domain, representations, multiplicity, borrowing, and large values unchanged |
| Chapter 24 | Accounting, exact extents, applicability, and resource categories unchanged |
| Chapter 31 | Entire publication owner unchanged |
| Transactions/persistence | No change |
| Chapters 1–24 and 26 onward | Byte-identical to HEAD |

For the requested reread, every number in each range has the stated answer. Question 50 is qualified by the actual empty v1 scalar-function registry.

| Questions | Answer | Basis |
|---|---|---|
| 1–12 | YES each | Generic demand, nested subsets, representation freedom, active domain, speculation, and frozen order |
| 13–22 | YES each | DML provenance, expression phase, D21-S4 ownership, RETURNING separation, implementation freedom |
| 23–40 | YES each | Resolved slot mapping, physical nonidentity, result correspondence, CONSTANT/DICTIONARY multiplicity |
| 41–49 | YES each | Invalid shape/type/kernel/mapping/selection/borrow/validity/output states are internal |
| 50 | N/A for valid v1 function state | No named scalar functions exist; a purported unsupported executable kernel is invalid, not a new scalar feature |
| 51–60 | YES each | Internal/public distinction, validation freedom, complete success, failed-invocation nonpublication, safety |
| 61–68 | YES each | Chapter-17 arithmetic authority and coherent cast/comparison delegation |
| 69–80 | YES each | Precise stable-borrow contract, reset protection, zero-copy and retention freedom, owned computed bytes |
| 81–92 | YES each | Exact VARCHAR applicability, no hidden maxima, exact extents, accounting, canonical failures |
| 93–107 | YES each | D25-S1 and Chapter-31 publication boundary preserved |
| 108–113 | NO each | No chronology, implementation narration, Development, Verification, Project State, or history leakage |
| 114–117 | YES each | Analytical, timeless, implementation-independent where permitted, precise references |
| 118 | NO | No new frozen semantic question |
| 119–125 | YES each | All seven M25/N25 findings closed |
| 126–130 | YES each | D25-S1/Q25-1/B25-1 remain closed; no frozen questions; Architecture clean |
| 131 | NO | Verification synchronization pending |
| 132 | NO | Chapter 25 not fully closed |
| 133 | NO | Chapter 26 review not started |
| 134 | NO | Phase 2 not started |
| 135 | NO | Phase 2 not authorized |

Task-created hunk classifications cover A–S and U–W: demand and nesting, DML provenance/phase, slot mapping and multiplicity, invalid state/publication, arithmetic ownership, borrowing, large-value/resource handoffs, references, rationale, and wrapping. Class T required no chronology edit because the audit found none. No D25-S1 semantic hunk was changed.

Final Git state:

```text
 M docs/ARCHITECTURE.md
```

The index is clean; HEAD remains `90d71da09231058399480b115e6c13cfff5d3207`. `git diff --check` passed. No external repository change was observed during the task.

M25-1, M25-2, M25-3, M25-4, N25-1, N25-2, and N25-3 are CLOSED. D25-S1, Q25-1, and B25-1 remain CLOSED. Frozen Chapter-25 semantic questions: NONE.

Chapter-25 Architecture is CLEAN. Chapter-25 Verification is SYNCHRONIZATION PENDING, so Chapter 25 is not fully closed. The next task is **CHAPTER-25 VERIFICATION SYNCHRONIZATION**.

No source, tests, build files, Verification, Development, Project State, or review artifacts were modified. No implementation, build, test, sanitizer, benchmark, staging, commit, devlog, or generated review artifact occurred. Chapter 26 review and Phase 2 remain NOT STARTED; Phase 2 remains NOT AUTHORIZED.