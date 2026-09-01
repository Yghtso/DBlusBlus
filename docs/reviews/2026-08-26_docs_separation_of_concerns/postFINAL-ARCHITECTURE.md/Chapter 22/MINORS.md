# Chapter-22 document-only cleanup — COMPLETE

M22-1, N22-1, and N22-2 are closed. Chapter 22 Architecture is now clean, with no physical-plan semantic changes.

## Repository state

Initial:

- HEAD: `f86cbe57ea3f57b83823e45b481a4b6ddecd09f4`
- Working tree: clean
- Index: clean
- D22-S1 integration: already committed
- Review artifacts in status: none

Final:

- HEAD: unchanged
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- Task diff: 42 insertions, 40 deletions
- `git diff --check`: passed

Historical review artifacts were not read, modified, moved, or staged.

## Sections modified

Only Chapter 22 was edited:

- [§22.1 Execution architecture](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18809)
- [§22.2 Immutable physical plan versus execution state](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18843)
- [§22.4 V1 physical operator family](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18892)
- [§22.4.1 Physical implementation availability](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18941)
- [§22.7 Physical properties](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19055)
- [§22.8 Foundation invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19101)

Chapters 1–21, Chapter 23+, and §§30, 37, and 38 were task-unchanged.

## M22-1 — property ownership

### Original problem

§22.7 presented all of these as “physical properties”:

- ordering;
- candidate/unique keys;
- partitioning;
- rewindability;
- blocking/streaming nature;
- estimated cardinality and memory;
- spill capability.

That conflicted with Chapter 37’s canonical taxonomy, which tracks exactly:

```text
OrderingProperty
RequiredSlotSet
```

Chapter 37 also explicitly says `RequiredRowsObjective` is not a physical property.

### Final ownership model

§22.7 now:

- declares Chapter 37 the canonical property owner;
- delegates definitions and satisfaction/preservation rules to §§37.2–37.5;
- prevents consumers from inferring properties from operator names or incidental output;
- avoids defining a competing property system.

Final inventory:

| Term | Classification | Canonical owner |
|---|---|---|
| `OrderingProperty` | Exact required/provided physical property | §§37.2–37.5 |
| `RequiredSlotSet` | Exact physical-planning requirement | §37.4 |
| Candidate/unique-key facts | Exact logical/proof metadata, not physical property | Logical/proof owners |
| Partitioning | Reserved extension point, not tracked v1 property | §37.1 |
| Rewindability | Reserved extension point, not tracked v1 property | §37.1 |
| Materialization | Reserved extension point, not tracked v1 property | §37.1 |
| Blocking/streaming | Algorithm/runtime trait | Execution owners |
| Spill support | Capability/runtime trait | Execution/memory owners |
| Estimated cardinality/memory | Cost estimates, never exact proof | Optimizer chapters |
| `RequiredRowsObjective` | Cost/search metadata | §38.16 |
| PhysicalTopN exact K | Applicability/capability condition | §§22.4.1, 30.7 |

Exact properties, estimates, objectives, proof facts, and capabilities are now explicitly distinct.

D22-S1 remains intact: exact-K support is capability/applicability, not a physical property.

## N22-1 — timeless wording

### Pre-cleanup inventory

| Original phrase | Classification | Final treatment |
|---|---|---|
| “may later be cached or reused” | E — project-time wording | “MAY be cached or reused” |
| “Initial physical operator family” | E — implementation-era heading | “V1 physical operator family” |
| “currently eligible” | E — current-state wording | “eligible” |
| “later/conditional” algorithms | E — roadmap wording | “Capability-conditional” |
| “only after … capability is enabled” | Runtime capability condition expressed chronologically | “only when … capability is enabled” |
| “requiring later components” | D — downstream structural reference | Replaced by “consumers” during §22.7 rewrite |
| “current single-threaded implementation” | E — current implementation narration | Replaced by implementation-independent incidental-output rule |
| “from day one” | E — chronology | Removed |
| “current command boundary” | B — transaction-runtime semantics | Preserved |

### Post-cleanup temporal audit

The complete search found one surviving meaningful target term:

| Section | Phrase | Class | Justification |
|---|---|---|---|
| §22.5 | “current command boundary” | B | Frozen REPEATABLE READ/CommandId runtime meaning |

Project chronology/current-state count: **0**.

No roadmap, deferred-development, project Phase, implementation milestone, or sequencing language remains in Chapter 22.

## N22-2 — verification ownership

The exact leaked sentence was:

> “A row-at-a-time reference executor MAY exist for differential/correctness testing…”

It was removed from §22.1.

No operator was removed. The architectural requirements remain:

- production execution is vectorized/chunk-at-a-time;
- physical algorithms are capability-gated;
- all conforming alternatives preserve logical results, ordering, mandatory errors, and transaction behavior.

Differential execution, reference plans, and oracle construction remain Verification-owned under §41 and `docs/VERIFICATION.md`. No Verification text was moved or edited in this task.

Post-cleanup Chapter-22 searches found no:

- reference executor;
- differential-testing recipe;
- test fixture;
- oracle construction;
- benchmark procedure;
- verification methodology.

## Document-role audit

| Chapter-22 section | Final owner |
|---|---|
| §22.1 Execution architecture | ARCHITECTURE |
| §22.2 Plan versus execution state | ARCHITECTURE |
| §22.3 Operator contract | ARCHITECTURE |
| §22.4 Operator vocabulary | ARCHITECTURE |
| §22.4.1 Capability availability | ARCHITECTURE |
| §22.5 Query execution context | ARCHITECTURE |
| §22.6 Runtime ownership | ARCHITECTURE |
| §22.7 Physical properties | ARCHITECTURE |
| §22.8 Foundation invariants | ARCHITECTURE |

Leakage results:

- DEVELOPMENT sequencing: none
- VERIFICATION procedure: none
- PROJECT_STATE narration: none
- devlog/history narration: none
- current implementation narration: none

## Implementation-coupling audit

Surviving implementation-oriented terms are architecturally justified:

| Term | Classification |
|---|---|
| Vectorized/chunk-at-a-time | Correctness/performance architecture |
| Query-scoped arena | Useful abstract lifetime handoff |
| `DataChunk` | Canonical downstream execution abstraction |
| Runtime pointer nonpersistence | Correctness-significant ownership invariant |

No additional unnecessary coupling finding was discovered.

Terminology, normative strength, analytical rationale, and implementation freedom remain sound. In particular, `MUST NOT` property inference and `MUST` ordering-advertisement rules remain normative.

## Changed-reference audit

| Source | Target | Purpose | Owner/precision | Circular? | Status |
|---|---|---|---|---|---|
| §22.7 | Chapter 37 | Canonical property ownership | Correct canonical owner | No | GOOD |
| §22.7 | §§37.2–37.5 | Structures, satisfaction, baseline operator rules | Exact | No | GOOD |
| §22.7 | §38.16 | `RequiredRowsObjective` ownership | Exact | No | GOOD |
| §22.7 | §§22.4.1, 30.7 | Top-N exact-K capability | Exact | No | GOOD |

## Semantic regression

- D22-S1: unchanged.
- Exact mathematical K: unchanged.
- Top-N ineligibility instead of SQL failure: unchanged.
- Wider exact representations: unchanged.
- Saturated retained-K prohibition: unchanged.
- `ALL_ROWS`/no-finite-objective fallback: unchanged.
- Final-plan exact-K validation: unchanged.
- No hidden cardinality bound or concrete integer type introduced.
- D20-B1–B2 and D20-M1–M6: unchanged.
- D21-S1–S6: unchanged.
- Physical operators, scans, joins, aggregation, Sort/Limit, and subqueries: unchanged.
- Cost/search and plan validation: unchanged.
- Transaction, persistence, and WAL semantics: unchanged.

No new frozen semantic question was found.

## Reread answers 1–62

| # | Answer | # | Answer |
|---:|---|---:|---|
| 1 | Yes | 32 | Yes |
| 2 | Yes | 33 | Yes |
| 3 | Yes | 34 | Yes |
| 4 | Yes | 35 | Yes |
| 5 | Yes | 36 | Yes |
| 6 | Yes | 37 | Yes |
| 7 | Yes | 38 | Yes |
| 8 | Yes | 39 | Yes |
| 9 | Yes | 40 | Yes |
| 10 | No | 41 | Yes |
| 11 | No | 42 | Yes |
| 12 | No | 43 | Yes |
| 13 | No | 44 | Yes |
| 14 | No | 45 | Yes |
| 15 | Yes | 46 | Yes |
| 16 | Yes | 47 | No |
| 17 | Yes | 48 | No |
| 18 | No | 49 | No |
| 19 | No | 50 | No |
| 20 | No | 51 | No |
| 21 | No | 52 | No |
| 22 | Yes | 53 | Yes |
| 23 | Yes | 54 | Yes |
| 24 | Yes | 55 | Yes |
| 25 | Yes | 56 | Yes |
| 26 | Yes | 57 | Yes |
| 27 | Yes | 58 | Yes |
| 28 | Yes | 59 | Yes |
| 29 | Yes | 60 | No |
| 30 | Yes | 61 | No |
| 31 | Yes | 62 | No |

The negative answers correspond to the requested absences: no chronology, current-state narration, roadmap, Verification recipe, leakage, new semantic question, Verification synchronization, full closure, or Chapter-23 review.

## Final status

- M22-1: **CLOSED**
- N22-1: **CLOSED**
- N22-2: **CLOSED**
- D22-S1: **CLOSED**
- Q22-1: **CLOSED**
- B22-1: **CLOSED**
- Frozen Chapter-22 semantic questions: **NONE**

Chapter-22 Architecture:

```text
CLEAN
```

Chapter-22 Verification:

```text
SYNCHRONIZATION PENDING
```

Chapter 22 fully closed: **NO**

Next task: **CHAPTER-22 VERIFICATION SYNCHRONIZATION**

Chapter-23 direct review: **NOT STARTED**

## Scope confirmation

Task-created hunks cover classes A–J:

- Chapter-37 delegation;
- stale taxonomy removal;
- exact/estimated/capability terminology;
- timeless wording;
- current-state/roadmap removal;
- reference-executor testing removal;
- physical substitutability preservation;
- owner/cross-reference updates;
- analytical rationale preservation;
- local Markdown wrapping.

Only `docs/ARCHITECTURE.md` was modified. No build, test, benchmark, implementation, staging, commit, devlog, or review artifact was created. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.