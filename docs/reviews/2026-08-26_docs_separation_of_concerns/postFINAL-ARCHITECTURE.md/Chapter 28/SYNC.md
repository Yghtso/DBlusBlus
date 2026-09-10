# Chapter-28 Verification synchronization verdict

**CHAPTER 28 — FULLY REVIEWED AND CLOSED.**

- Architecture: CLEAN
- Verification: FULLY SYNCHRONIZED
- Frozen semantic questions: NONE
- PARTIAL / MISSING / CONTRADICTORY: **0 / 0 / 0**

## Repository state

Initial:

- HEAD: `1aea6fa7959ad2207e5e7cc9634bf2fb5ff678c5`
- Commit: `1aea6fa chapter 28 ARCHITECTURE analysis`
- Working tree: clean
- Index: clean
- Architecture diff: none
- Verification diff: none

Final:

- HEAD unchanged: `1aea6fa7959ad2207e5e7cc9634bf2fb5ff678c5`
- Working tree: `M docs/VERIFICATION.md`
- Index: clean
- Files changed: only docs/VERIFICATION.md (/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:19381)
- Diff: 707 insertions, 1 deletion
- `git diff --check`: PASS

No external repository change occurred during this task. Architecture remained untouched. Historical review artifacts were not read, modified, moved, or staged.

## Verification sections added or modified

Added one coherent `Chapter 28 — Join Execution Verification` family:

- V28-A — operator, join-type, role, and owner inventory
- V28-B — mathematical join bags, multiplicity, and NULL extension
- V28-C — hash/equality compatibility
- V28-D — collision directory and duplicate chains
- V28-E — PhysicalNestedLoopJoin
- V28-F — PhysicalIndexNestedLoopJoin
- V28-G — hash-build ownership and RequiredSlotSet
- V28-H — build Sink, Combine, Finalize, and readiness
- V28-I — probe continuation
- V28-J — LEFT matchedness
- V28-K — Grace spill, recursive repartition, and skew fallback
- V28-L — PhysicalMergeJoin capability execution
- V28-M — algorithm substitutability
- V28-N — output schema, side identity, and RequiredSlotSet
- V28-O — ordering properties
- V28-P — empty-side demand and downstream early stop
- V28-Q — expression errors, provenance, and output failure
- V28-R — memory, exact extents, large values, and cardinality
- V28-S — cancellation, retry, invalid states, and persistence-negative registry
- V28-T — representation, chunk, hash-seed, worker, and algorithm determinism
- V28-U — cross-chapter reuse map
- V28-V — complete atomic obligation ledger
- V28 stale-rule and document-quality audit

The older compact `Hash Join Tests` section was corrected so production NestedLoopJoin is no longer HashJoin’s sole oracle. Both now compare to the independent V28 bag/equality/LEFT model.

## Operator and type inventory

| OperatorSupported execution scope |                                                                 |
| --------------------------------- | --------------------------------------------------------------- |
| `PhysicalNestedLoopJoin`          | INNER, LEFT, CROSS                                              |
| `PhysicalIndexNestedLoopJoin`     | Applicable indexed INNER and LEFT                               |
| `PhysicalHashJoin`                | INNER and LEFT                                                  |
| `PhysicalMergeJoin`               | Capability-valid equality forms, including valid LEFT execution |

RIGHT, FULL, SEMI, ANTI, and null-aware anti are not positive Chapter-28 v1 execution alternatives. Range-style MergeJoin is also outside the v1 execution contract.

The owner map preserves:

- Chapter 17: equality, NULL, FLOAT64, VARCHAR, hash compatibility
- Chapter 20: bags, multiplicity, LEFT null extension, demand
- Chapter 21: D21-S4/D21-S5
- Chapter 22: algorithms, schemas, capability validation
- Chapters 23–24: representations, borrowing, memory, spill
- Chapter 25: expressions, provenance, D25-S1
- Chapter 26: Sink acceptance, continuation, Finalize, early stop
- Chapter 31: external publication
- Chapter 32: parallel dependencies
- Chapter 37: OrderingProperty and RequiredSlotSet
- §39: failure and transaction consequences

## Join semantic methodology

The independent `JB` oracle enumerates tagged logical occurrence pairs:

- INNER emits every complete-predicate TRUE pair exactly once.
- FALSE and UNKNOWN emit nothing.
- CROSS emits the exact Cartesian product.
- LEFT emits every TRUE pair or exactly one right-null-extended row when none qualify.
- Equal physical values, CONSTANT payloads, and repeated DICTIONARY indices remain distinct logical occurrences.
- Symbolic `M×N` multiplicity is calculated mathematically without finite-width multiplication or full materialization.

NULL extension verifies unchanged TypeIds and LogicalSlotIds, false validity for every missing-side column, and inaccessible poisoned payload.

## Hash/equality methodology

The `HE` and `HC` models cover:

- BOOLEAN, INT32, INT64, DATE, TIMESTAMP
- FLOAT64 `-0.0`/`+0.0`
- canonical NaN values
- exact-length VARCHAR with embedded NUL/high bytes
- composite component ordering and boundaries
- ordinary NULL and composite NULL keys
- controlled unequal-key hash collisions

Equal values must be lookup-compatible. Same hash is only candidate generation; every candidate undergoes full exact key equality.

Duplicate equal build keys remain individually represented or use an exact multiplicity-preserving encoding. CONSTANT and DICTIONARY physical sharing cannot collapse occurrences.

The exact hash function, seed, and directory layout remain implementation freedom and are not persistent format.

## NestedLoop and IndexNL

NestedLoop methodology verifies:

- logical right materialized once;
- no implicit source rewind;
- stable and accounted retained values;
- exact Cartesian continuation across capacity;
- no restart, omission, or duplication;
- empty-right INNER/CROSS suppression;
- empty-right LEFT null extension;
- oversized retained-row handling.

IndexNL methodology independently verifies:

- outer key evaluation;
- exact index range lookup;
- heap-reference validation;
- MVCC recheck;
- residual TRUE/FALSE/UNKNOWN/error;
- LEFT no-match behavior;
- multi-chunk candidate continuation.

Index membership alone never establishes a match.

## Hash build and probe

The complete build ownership graph covers keys, payload, residual-only and output-only columns, VARCHAR bytes, RowCollection, directory entries, duplicate chains, cached hashes, handles, and borrowed output.

Build lifecycle methodology proves:

- complete-domain Sink acceptance exactly once;
- no blind replay;
- build exhaustion is not probe readiness;
- applicable Combine includes every build occurrence exactly once;
- successful Finalize publishes complete immutable probe state;
- failed Finalize blocks probe;
- Finalize is not cleanup.

Probe continuation verifies:

- one accepted probe input;
- one unresolved lifecycle per local state;
- zero, one, or arbitrarily many output chunks;
- exact first-unoffered resume position;
- no restart, skip, or reacceptance;
- stable direct backing or independently copied state;
- finite zero-output progress;
- proper new-input readiness;
- final output before terminal;
- failed/canceled continuation never resumes.

## LEFT matchedness

| Candidate resultMatchedOutput |                     |                                 |
| ----------------------------- | ------------------- | ------------------------------- |
| No candidate                  | No                  | One NULL-extended row           |
| Same hash, unequal key        | No                  | NULL extension after exhaustion |
| Exact key, residual FALSE     | No                  | Continue                        |
| Exact key, residual UNKNOWN   | No                  | Continue                        |
| Exact key, residual error     | No successful match | Canonical error                 |
| One TRUE                      | Yes                 | One joined row                  |
| N TRUE                        | Yes                 | N joined rows, no NULL row      |
| NULL probe key                | No                  | One NULL-extended row           |

NULL extension cannot occur before all demanded candidates are ruled out. The supported LEFT hash orientation builds the nonpreserved right side, so no build-unmatched final phase was invented.

## Spill and MergeJoin

Grace methodology verifies:

- compatible build/probe partitioning;
- one exact partition membership per occurrence;
- equal keys never separated incompatibly;
- duplicate preservation;
- in-memory/spilled semantic equivalence;
- recursive repartition progress;
- bounded depth;
- exact, accounted skew fallback;
- SpillIOError classification and cleanup.

Ordinary HashJoin OOM does not silently select NestedLoopJoin. The §28.11 skew fallback remains the only specialized authorized fallback.

MergeJoin methodology covers:

- capability-valid equality forms;
- compatible ordered inputs;
- ordinary NULL nonmatching;
- incremental duplicate groups;
- symbolic `M×N` multiplicity;
- valid LEFT unmatched behavior;
- exact OrderingProperty validation;
- range-style and incompatible alternatives rejected before execution.

## Algorithm substitution

Every eligible algorithm pair is checked against an independent logical oracle:

- NestedLoop / IndexNL
- NestedLoop / Hash
- NestedLoop / Merge
- IndexNL / Hash
- IndexNL / Merge
- Hash / Merge

Where both execute successfully, values, NULLs, tagged bags, LEFT extension, schemas, LogicalSlotIds, required ordering class, D25-S1, D21-S4, and transaction/result semantics must agree.

Resource feasibility, spill behavior, and unordered physical sequence may differ. No generic runtime fallback is inferred.

## Schema, slots, and ordering

Verification establishes:

- logical left/right remains distinct from build/probe;
- INNER build orientation does not change output schema;
- output follows declared logical-left then logical-right physical schema;
- LogicalSlotIds, TypeIds, nullability, and provenance are exact;
- self-join sides remain distinct, including same-RID cases;
- duplicate display names do not trigger runtime resolution;
- key-only, residual-only, and output-only slots remain available;
- RequiredSlotSet cannot erase demanded expression work.

Ordering results:

- HashJoin advertises no ordering.
- Hash seed, bucket, chain, insertion, spill, pointer, and runtime ordinal order are nonsemantic.
- NestedLoop, IndexNL, and MergeJoin advertise only explicitly capability-proven properties.
- Duplicate ties create no hidden SQL order.
- Chapter 37 remains the sole property owner.

## Demand and early stop

The formerly missing empty-build family is now COMPLETE:

- Exact empty INNER build/right makes result empty and removes opposite-side row demand merely for EOS.
- Exact empty CROSS side behaves likewise.
- LEFT with empty right still demands every preserved left occurrence.
- Undemanded opposite-side row/storage errors do not surface.
- `LIMIT 0` above any join creates no relational build/probe demand after mandatory count acquisition.
- Positive Limit reached mid-chain abandons remaining matches and later probes.
- Later undemanded residual errors are suppressed.
- Output handoff and cleanup remain mandatory.

No zero-allocation promise was introduced.

## Errors, resources, and invalid states

D25-S1 methodology reduces demanded candidates across rows, duplicate chains, chunks, continuation, and workers. Build/probe row, bucket, chain position, chunk, worker, orientation, and physical algorithm never become ranking keys.

DML joins retain all D21-S4 candidate metadata without D25 pre-ranking. Physical hash-key/residual decomposition preserves source provenance.

Resource methods cover all dynamic join memory, exact extent arithmetic, large VARCHAR keys, oversized retained rows, symbolic result cardinality, and unbounded match-chain continuation.

Canonical outcomes remain:

- supported exact allocation denial: OOM;
- unavailable exact representation: representability `ExecutionError`;
- spill failures: `SpillIOError`;
- child corruption: lower-layer category;
- malformed query-local hash state: internal invalid runtime state.

Current failed output is nonconsumable. Prior internal handoff and prior externally returned prefix remain distinct.

Cancellation and retry matrices cover all NL, IndexNL, hash build/Finalize/probe, spill/repartition, and MergeJoin phases. Retry uses fresh mutable state while the immutable plan may be reused.

## Determinism

Successful semantics are invariant under:

- DataChunk capacity and boundaries
- empty progress batches
- FLAT/CONSTANT/DICTIONARY representation
- hash seed
- bucket and duplicate-chain layout
- INNER build orientation
- worker completion and Combine order
- pointer/address and runtime ordinals
- eligible algorithm choice

Resource feasibility and unordered physical sequence remain permitted differences.

## Required matrices

The V28 family now contains all required matrices:

- join types
- physical algorithms
- hash equality
- inner multiplicity
- LEFT matching
- continuation
- build ownership
- pipeline lifecycle
- spill/repartition
- MergeJoin
- algorithm substitution
- empty/early demand
- output schema
- RequiredSlotSet
- ordering
- error owners
- resources
- invalid states
- retry/cancellation
- determinism
- cross-chapter reuse

## Previously incomplete families

All eighteen are now COMPLETE:

1. Operator/type inventory
2. Hash equality/NULL/FLOAT/VARCHAR composition
3. Collision and duplicate chains
4. Algorithm substitution
5. Probe continuation
6. Build/probe ownership
7. Finalize/readiness
8. Grace spill/skew fallback
9. LEFT matched/unmatched behavior
10. RequiredSlotSet
11. Ordering properties
12. D25-S1 join-pair reduction
13. D21-S4 transport
14. LIMIT-zero/early stop
15. Empty-build demand
16. Resource/result-cardinality composition
17. Cancellation/retry/invalid states
18. Worker/hash-seed determinism

## Atomic coverage

The complete ledger contains exactly 204 unique rows:

- TOTAL ATOMIC: **204**
- CORRECTNESS-RELEVANT: **204**
- COMPLETE: **204**
- PARTIAL: **0**
- MISSING: **0**
- CONTRADICTORY: **0**
- N/A: **0**

Ledger distribution:

| FamilyRows |         |
| ---------- | ------- |
| A–C        | 12 each |
| D–J        | 10 each |
| K          | 12      |
| L          | 8       |
| M–N        | 10 each |
| O          | 8       |
| P–T        | 10 each |

Unsupported RIGHT/FULL/SEMI/ANTI/null-aware anti variants create no positive atomic rows. Their exclusion is captured by COMPLETE negative capability obligation V28-A12. Range-style MergeJoin exclusion is captured by COMPLETE V28-L07. Therefore no N/A justification is required.

## Stale-methodology audit

The synchronization rejects:

- chronology-bound join roles;
- same hash as equality;
- NULL ordinary equality matches;
- host FLOAT or C-string semantics;
- duplicate overwrite;
- one-chunk probe assumptions;
- continuation through resubmission;
- empty output as EOS;
- premature LEFT matchedness/null extension;
- mandatory opposite-side scans after exact emptiness;
- unconditional hash build below `LIMIT 0`;
- build/probe-defined output schema;
- incidental join ordering;
- finite-width `M×N` overflow as SQL error;
- RowLayout/chunk width as SQL limits;
- first-physical error ranking.

The compact legacy Hash Join section remains a smoke-test index; V28-A–V is the normative methodology.

## Reread answers 1–342

- Questions 1–338: **YES**, for every applicable obligation.
- Question 339 — frozen Chapter-28 semantic question: **NO**
- Question 340 — Chapter-29 review started: **NO**
- Question 341 — Phase 2 started: **NO**
- Question 342 — Phase 2 authorized: **NO**

N28-1 remains CLOSED.

## Final scope and status

- Task-created hunk classes: A–AK
- Class AL: only unavoidable Markdown wrapping
- Only `docs/VERIFICATION.md` was task-modified.
- Architecture, Development, Project State, source, tests, benchmarks, build files, devlogs, and review artifacts were unchanged.
- No build, test, sanitizer, or benchmark was run.
- No implementation occurred.
- Nothing was staged or committed.
- No new semantic question was found.
- Chapter 29 remains NOT STARTED.
- Phase 2 remains NOT STARTED / NOT AUTHORIZED.

Recommended next task: **CHAPTER 29 DIRECT READ-ONLY ARCHITECTURE REVIEW.**