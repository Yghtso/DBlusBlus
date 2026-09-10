# Chapter-28 document-only cleanup verdict

**N28-1 CLOSED. Chapter-28 Architecture is CLEAN.**

No semantics changed and no new semantic question was discovered.

## Repository state

Initial:

- HEAD: `8a67b790ad38b942623310a2da61e0c4ffd4efe6`
- Working tree: one untracked historical Chapter-28 review directory
- Index: clean
- Architecture diff: none

During the task, an external process advanced HEAD to:

- `1aea6fa7959ad2207e5e7cc9634bf2fb5ff678c5`
- Commit subject: `chapter 28 ARCHITECTURE analysis`

That external commit incorporated the exact Architecture wording changes made during this task and added the pre-existing historical review artifact. I did not stage or commit anything and did not read the artifact.

Final:

- HEAD: `1aea6fa7959ad2207e5e7cc9634bf2fb5ff678c5`
- Working tree: clean
- Index: clean
- `git diff --check`: PASS

## Sections modified

Only these Chapter-28 sections were substantively changed:

- §28.1 Join algorithm roles
- §28.3 Nested-loop join
- §28.5 Hash-join shape and preserved-side orientation
- §28.6 Hash-join build storage and directory
- §28.7 Hash-build pipeline and finalization
- §28.12 Merge join

Chapter 28 now ends at line 21872; Chapter 29 begins at line 21873.

## N28-1 replacements

| SectionOriginalTimeless replacement |                                                                                                             |                                                                                                 |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| §28.1                               | `Their initial roles are:`                                                                                  | `Their v1 roles are:`                                                                           |
| §28.1                               | `ordered-input algorithm added after the simpler join paths are stable`                                     | `capability-conditional ordered-input algorithm when predicates and physical properties permit` |
| §28.1                               | `The physical optimizer chooses the algorithm later.`                                                       | `Physical planning selects one capability-valid join algorithm before execution.`               |
| §28.3                               | `Nested-loop join is not the universal large equi-join fallback once hash join is available.`               | `Nested-loop join is a costed physical alternative, not a universal large equi-join fallback.`  |
| §28.5                               | `Initial join types are:`                                                                                   | `` `PhysicalHashJoin` supports: ``                                                              |
| §28.6                               | `The initial target maximum directory load factor is approximately:`                                        | `The ordinary maximum directory load-factor tuning target is approximately:`                    |
| §28.7                               | `For the initial INNER/LEFT shape...`                                                                       | `For the supported INNER/LEFT hash-join shape...`                                               |
| §28.12                              | `Merge join is an architecture-supported later execution algorithm once nested-loop/hash paths are stable.` | Capability-conditional PhysicalMergeJoin wording tied to Chapters 22, 37, and 38                |
| §28.12                              | `The initial implementation may focus on equality merge joins.`                                             | ``The v1 `PhysicalMergeJoin` contract admits equality merge joins.``                            |
| §28.12                              | `Range-style merge opportunities may be added...`                                                           | Range-style merge joins are explicitly outside the v1 Chapter-28 execution contract             |

The directory load-factor target remains exactly **approximately 0.70**. It remains runtime tuning/resource policy—not a SQL, persistence, or cardinality limit.

MergeJoin remains capability-conditional. Equality merge joins remain admitted; range-style merge joins are now expressed as outside v1 scope rather than future work.

## Semantic regression results

All protected semantics are unchanged:

- Supported logical joins: INNER, LEFT, CROSS
- Physical algorithms: NestedLoopJoin, IndexNestedLoopJoin, HashJoin, MergeJoin
- RIGHT, FULL, SEMI, ANTI, and null-aware anti were not added
- INNER and CROSS multiplicity unchanged
- LEFT matching and NULL extension unchanged
- Ordinary NULL hash keys remain nonmatchable
- Composite NULL-key behavior unchanged
- FLOAT64 signed-zero and NaN hash/equality compatibility unchanged
- VARCHAR remains exact-byte, embedded-NUL-safe
- Hash collisions still require full equality
- Duplicate build occurrences remain preserved
- INNER build-side flexibility unchanged
- LEFT remains logical-right build/logical-left preserved probe
- Output schema and LogicalSlotIds unchanged
- Nested-loop right-side materialization unchanged
- Index-NL heap/MVCC/residual recheck unchanged
- Probe continuation and ownership unchanged
- Build RowCollection/VARCHAR ownership unchanged
- Successful Finalize remains required before probe
- Grace spill, repartition, and skew fallback unchanged
- OrderingProperty remains Chapter-37-owned
- RequiredSlotSet unchanged
- Resource and representability semantics unchanged
- D25-S1 and D21-S4 unchanged
- Empty-build and `LIMIT 0` demand unchanged
- Cancellation, retry, publication, transaction, and persistence boundaries unchanged

## Final documentation audits

Temporal-language inventory remaining in Chapter 28:

| PhraseClassification       |                            |
| -------------------------- | -------------------------- |
| `current probe row`        | Runtime continuation state |
| `current build-row handle` | Runtime continuation state |
| `residual later fails`     | Runtime predicate sequence |

These are legitimate runtime/lifetime uses.

Final counts:

- Project chronology: 0
- Current implementation narration: 0
- Development sequencing: 0
- Verification procedure leakage: 0
- Project-State leakage: 0
- History/devlog leakage: 0

Terminology remains precise among logical left/right, build/probe, hash candidate, exact key equality, residual-qualified match, preserved side, NULL extension, semantic Finalize, cleanup, physical sequence, and OrderingProperty.

The new cross-references to Chapters 22, 37, and 38 point to the canonical physical-capability, property, and validation owners. No circular or decorative reference was introduced.

Implementation freedom remains for hash mixing, internal directory mechanics, duplicate-chain representation, continuation state, RowCollection internals, output vectors, workers, spill buffering, and merge-group cursors.

## Reread answers 1–112

- Questions 1–101: **YES**
- Question 102, new semantic question: **NO**
- Question 103, semantic behavior changed: **NO**
- Questions 104–107: **YES**
- Question 108, Verification fully synchronized: **NO**
- Question 109, Chapter 28 fully closed: **NO**
- Question 110, Chapter 29 review started: **NO**
- Question 111, Phase 2 started: **NO**
- Question 112, Phase 2 authorized: **NO**

## Final status

- N28-1: **CLOSED**
- Frozen Chapter-28 semantic questions: **NONE**
- Chapter-28 Architecture: **CLEAN**
- Chapter-28 Verification: **PARTIAL / SYNCHRONIZATION PENDING**
- Chapter 28 fully closed: **NO**
- Chapter 29: **NOT STARTED**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**

Task hunk classes A–I were applied; J supplied the precise Chapters 22/37/38 owner references, and K covered necessary Markdown wrapping.

No build, tests, sanitizers, or benchmarks were run. No implementation, Verification synchronization, devlog, or review artifact was created by this task.

Next task: **CHAPTER-28 VERIFICATION SYNCHRONIZATION.**