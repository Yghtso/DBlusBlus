## Verdict

**TARGETED CHAPTER-27 DOCUMENT-ONLY CLEANUP — COMPLETE**

- N27-1: **CLOSED**
- N27-2: **CLOSED**
- Frozen Chapter-27 semantic questions: **NONE**
- Chapter-27 Architecture: **CLEAN**
- Chapter-27 Verification: **SYNCHRONIZATION PENDING**
- Chapter 27 fully closed: **NO**

## Repository state

Initial state:

- HEAD: `af7cfa75cfc32a702870359fcc1561655439665c`
- Working tree: clean
- Index: clean
- Pre-existing Architecture diff: none

The HEAD differed from the preceding review’s reported commit, but the repository was clean. That external change was preserved without alteration.

Final state:

- HEAD: `af7cfa75cfc32a702870359fcc1561655439665c`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- `git diff --check`: passed
- Diff: 9 insertions, 5 deletions
- Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21335) was task-modified

Historical review artifacts remained unread, unmodified, unmoved, and unstaged.

## Sections modified

- [§27.5 RID batching](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21335)
- [§27.8 PhysicalProject](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21401)
- [§27.12 Scan/unary invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21468)

Chapters 1–26 and Chapter 28 onward were unchanged.

## N27-1 — RID batching chronology

Exact replacements:

| Original | Timeless replacement |
|---|---|
| `Initial target:` | `The ordinary RID-batch tuning target is:` |
| `The initial implementation preserves B+ cursor order while fetching those candidates.` | `A RID-batching strategy preserves B+ cursor order while fetching candidates when the PhysicalIndexScan advertises or must satisfy the original index ordering.` |
| `A future implementation may group heap fetches by PageId only if the physical plan no longer promises/needs the original index ordering.` | `Alternative batching strategies MAY group heap fetches by PageId only when the physical plan neither promises nor needs the original index ordering.` |
| `Initial index RID batching preserves index order.` | `Index RID batching preserves every ordering advertised by the PhysicalIndexScan.` |

Preserved results:

- Batch target remains exactly **up to one DataChunk capacity**.
- The target is explicitly a physical tuning target, not a SQL limit or persistent format.
- RID batches remain query-local temporary state.
- Candidate coverage, multiplicity, and continuation are unchanged.
- Any ordering advertised by `PhysicalIndexScan` must remain preserved.
- Alternative PageId grouping remains permitted only without a promised or required original index ordering.
- Batching does not independently create an `OrderingProperty`.
- Physical RID tie order does not become SQL ordering.
- Chapter 37 remains the physical-property owner.
- No persistent RID-batch representation was introduced.
- Final Chapter-27 project-chronology count: **0**.

## N27-2 — PhysicalProject terminology

Original:

> Its output order is the projection's LogicalSlotId order.

Replacement:

> Its output columns follow the declared ordered physical output schema. Project preserves the input row-occurrence sequence; any advertised `OrderingProperty` remains governed by §37.5.

Final interpretation:

- Output-column sequence comes from the declared ordered physical output schema.
- `LogicalSlotId` remains semantic identity, not a numeric sorting key.
- Physical column position remains local schema/DataChunk addressing.
- Project preserves input row-occurrence sequence.
- Project does not independently establish SQL row ordering.
- Chapter 37 remains the `OrderingProperty` owner.
- Equal-valued or equivalent output expressions may retain distinct `LogicalSlotId`s.
- Project multiplicity, RequiredSlotSet behavior, expression demand, and provenance are unchanged.
- No new property rule was created.

## Final audits

### Temporal-language inventory

| Remaining occurrence | Classification |
|---|---|
| Historical tuple `schema_version` | Schema-history semantics |
| Reverse scan remains `deferred` by Chapter 8 | Durable v1 capability scope |
| Forward `baseline` IndexScan | Durable v1 capability scope |
| “Once the limit is satisfied” | Runtime state transition |
| “later client/result-interface contract” | Forward navigation to Chapter 31 |

No remaining occurrence is project chronology.

### Document ownership

- Current-state narration: **0**
- Development sequencing: **0**
- Verification methodology: **0**
- Project-State leakage: **0**
- History/devlog narration: **0**
- New or stale cross-references: **0**
- Added cross-reference: §37.5 exists and is the canonical Project ordering-property owner.

The text remains analytical and implementation-independent. Freedom remains for RID batch representation and tuning, safe heap-fetch strategy, Project vector representation, and materialization strategy.

## Semantic regression assessment

All protected semantics remain unchanged:

| Area | Result |
|---|---|
| SeqScan | Unchanged |
| IndexScan candidate/recheck semantics | Unchanged |
| MVCC and snapshot ownership | Unchanged |
| Page and complete tuple validation | Unchanged |
| Stale/dead RID handling | Unchanged |
| RID reuse protection | Unchanged |
| Nullable index keys and duplicate keys | Unchanged |
| Residual predicates | Unchanged |
| Filter semantics | Unchanged |
| Project values, multiplicity, demand, provenance, and ownership | Unchanged |
| Limit/OFFSET and qualifying-row counting | Unchanged |
| LIMIT=0 with positive OFFSET | Unchanged |
| ResultSink acceptance/publication boundary | Unchanged |
| D25-S1 | Unchanged |
| D21-S4 | Unchanged |
| Chapter-26 protocol | Unchanged |
| Chapter-31 publication | Unchanged |
| Chapter-37 properties | Unchanged |
| Transaction and persistence semantics | Unchanged |

No new frozen Chapter-27 semantic question was discovered.

## Reread questions 1–72

- Questions 1–60: **YES**
- Question 61, new frozen semantic question: **NO**
- Question 62, semantic behavior changed: **NO**
- Questions 63–67: **YES**
- Question 68, Verification synchronized: **NO**
- Question 69, Chapter 27 fully closed: **NO**
- Question 70, Chapter 28 review started: **NO**
- Question 71, Phase 2 started: **NO**
- Question 72, Phase 2 authorized: **NO**

## Scope and next action

Task-created hunk classes:

- A–D: RID-batching chronology and invariant cleanup
- E–F: Project column/row-order distinction
- G: precise §37.5 owner reference
- H: Markdown wrapping

No build, tests, sanitizers, benchmarks, implementation, staging, commit, devlog, or review artifact occurred.

Next task: **CHAPTER-27 VERIFICATION SYNCHRONIZATION**.

Chapter 28 remains **NOT STARTED**. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.