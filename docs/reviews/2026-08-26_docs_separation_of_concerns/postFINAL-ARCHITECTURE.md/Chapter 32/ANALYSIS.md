# Chapter-32 Initial Read-Only Architecture Review

## 1. Initial review verdict

**NEEDS ARCHITECTURE FIX**

Finding:

- **N32-1 — MAJOR:** Chapter 32 lacks an explicit complete, non-overlapping, exactly-once morsel partition/claim contract.
- **N32-2 — EDITORIAL:** Several roadmap/progress phrases remain in the live Architecture.

No new semantic decision is required.

## 2–4. Repository state

Initial and final state:

```text
HEAD:
    962f88d32b72f6caa5d240ef375a5ca2b8650dd7

Commit:
    962f88d applied SYNC FIX-1 29 in ARCHITECTURE

Working tree:
    clean

Index:
    clean

AUDIT-CREATED CHANGES:
    NONE
```

`git diff --cached --name-only` was empty.

The current committed history contains prior changes to `docs/VERIFICATION.md` and a historical review artifact. Neither was modified during this audit.

## 5. Chapter-32 boundaries and inventory

```text
Chapter 32:
    docs/ARCHITECTURE.md:23767–24036

Chapter 33 begins:
    docs/ARCHITECTURE.md:24040
```

Subsections:

- §32.1 Worker model
- §32.2 Morsels
- §32.3 Worker-local state
- §32.4 Parallel sequential scan
- §32.5 Parallel hash join
- §32.6 Parallel hash aggregate
- §32.7 Parallel sort
- §32.8 Task scheduler and dependencies
- §32.9 Fairness
- §32.10 NUMA policy
- §32.11 SIMD and hot-loop policy
- §32.12 Prefetch
- §32.13 Parallel-runtime invariants

## 6. Canonical owner matrix

| Concern | Canonical owner | Result |
|---|---|---|
| Transaction identity/snapshot/CommandId | Chapters 9, 20, 26 | Consistent |
| RID/read-epoch protection | Chapters 11 and 14 | Consistent |
| DML mutation publication | Chapters 15 and 31 | Preserved |
| Operator lifecycle/dependencies | Chapter 26 | Consistent |
| Hash join semantics | Chapter 28 | Consistent |
| Aggregate state/Combine/Finalize | Chapter 29 | Consistent |
| Sort readiness/merge/error | Chapter 30 | Consistent |
| Runtime failures/cancellation | Chapter 39 | Consistent |
| Verification obligations | Chapter 41 | Existing reuse available |

## 7. Worker model

§32.1 correctly requires a fixed/configured worker pool and prohibits arbitrary per-query thread creation.

§26.5 and §26.9 correctly separate:

- immutable plan state;
- per-execution global state;
- worker/task-local mutable state.

Shutdown and task quiescence are owned by §3.3.6, §26.3.1, and §39.3. No worker-lifetime contradiction was found.

## 8. Morsel coverage and ownership

**Finding N32-1 — MAJOR**

Affected sections:

- §32.2, lines 23781–23806
- §32.4, lines 23830–23839
- §32.13 invariant 2, line 24025

Chapter 32 names heap page ranges, VALUES row ranges, and spill partition/run ranges, but does not state that, for every parallel-ready source:

- the morsels form a complete partition of the demanded domain;
- morsels are pairwise non-overlapping;
- every logical occurrence belongs to exactly one morsel;
- claim/ownership transfer is linearized;
- empty morsels are handled correctly;
- lost or duplicated claims are invalid;
- cancellation and terminal failure interact correctly with unclaimed work.

§26.4.2 prohibits duplicate, dropped, replayed, or reaccepted required occurrences, but Chapter 32 does not connect that semantic rule to scheduler-level morsel creation and claiming.

Concrete consequence: two implementations could both satisfy the current Chapter-32 wording while one omits a page range or claims one range twice, producing missing or duplicate rows.

Smallest repair surface:

- strengthen §32.2 with a source-independent complete/disjoint ownership rule;
- strengthen §32.4 with the heap-page-range coverage and claim rule;
- strengthen invariant 2 in §32.13;
- retain source-specific details in Chapters 27–30.

No concrete queue or atomic primitive is required.

## 9. Worker-local/shared state

The ownership model is adequate. Local cursors, chunks, scratch, aggregate tables, sort runs, continuation state, and counters are correctly distinguished from immutable plan state and genuinely shared coordination state.

## 10. Parallel sequential scan

The scan contract correctly preserves:

- shared transaction/snapshot semantics;
- immutable task view;
- read-epoch protection;
- BufferPool access;
- arbitrary unordered output;
- explicit order-preserving operators when ordering is required.

The only gap is the N32-1 morsel coverage/claim contract.

## 11. Snapshot, CommandId, and read-epoch lifetime

Chapters 9, 11, 14, and 26 provide sufficient ownership:

- one statement CommandId;
- fresh READ COMMITTED snapshot per authorized retry;
- read epoch through worker use;
- no transaction terminal transition by arbitrary read workers;
- no RID reuse while protected.

No contradiction found.

## 12–14. Hash join

Hash build correctly requires:

- local/query-owned retained storage;
- finalized immutable build state;
- probe blocked until required build partitions finalize;
- safe VARCHAR backing;
- no hash-directory mutation during probe.

Chapter 28.9 preserves exact LEFT JOIN multiplicity, NULL-key handling, residual predicates, and unmatched-row behavior.

No Chapter-32 conflict found.

## 15. Parallel aggregate

The contract is consistent with Chapter 29:

- exact integer/count states;
- mandatory Combine where parallelized;
- no narrowed subtotals;
- exact AVG count;
- canonical MIN/MAX representation;
- admitted FLOAT64 binary64 reduction trees.

The phrase “value/error-equivalent to serial execution” in §32.13 is read together with §29.3.4 and §29.3.7: FLOAT64 low bits and legal tree roots may differ, while occurrence membership, counts, flags, types, NULLability, and canonical representations remain constrained. No contradiction found.

## 16. Parallel sort

Parallel local runs, spill, merge readiness, comparator semantics, and ordered final output correctly delegate to Chapter 30.

Later merge/spill-read failures remain SELECT/execution failures and do not import Chapter-31 result-publication `R`.

## 17–18. Scheduler, dependencies, and Finalize

§32.8 requires successful predecessor Finalize before readiness.

Chapters 26 and 39 additionally establish that:

- failed or canceled predecessors cannot authorize consumers;
- no partially failed graph remains runnable;
- active users quiesce before shared state destruction;
- cleanup does not fabricate success.

No successor-after-failed-Finalize path is permitted.

The concrete queue, counter, wakeup, and work-stealing implementation remains free.

## 19. Error ownership and determinism

The architecture correctly distinguishes:

- ordinary semantic errors governed by Chapters 21 and 31;
- dynamic failures governed by their actual runtime owner;
- resource failures;
- cancellation;
- corruption and invariant failures.

DML ordinary candidate selection remains independent of worker order. No universal source-span ranking is incorrectly imposed on dynamic or SELECT failures.

## 20. Cancellation and fairness

Cancellation is query-wide and checked at chunk/reasonable block boundaries. New unnecessary work is not scheduled, running work drains at permitted points, and transaction-owned locks remain transaction-owned.

No starvation-free numerical fairness guarantee is claimed. Morsel boundaries provide control points.

## 21–22. Memory, spill, value, and output lifetime

Chapters 23–24 and 26 provide sufficient rules for:

- accounted worker-local/shared allocations;
- spill buffers/files;
- retained VARCHAR and dictionary backing;
- safe local-to-global ownership transfer;
- cleanup after cancellation or worker failure;
- quiescence before destruction;
- output-chunk lifetime.

No new memory subsystem is required.

## 23–25. NUMA, SIMD, and prefetch

These are correctly optional performance policies. They must not alter values, NULL behavior, ordering, errors, visibility, or lifetime.

The architectural concern is only their roadmap wording, classified below as editorial.

## 26–29. Control-operator regression

- **DML:** scans/evaluation may parallelize; mutation/write publication remains single-worker under §31.7.
- **DDL:** coordinator ownership remains under Chapter 31.
- **VACUUM:** maintenance ownership and concurrency remain under Chapters 14 and 31.
- **ANALYZE:** §31.12.1 and Chapter 34 retain one publication coordinator; collection helpers do not create parallel publication.

No Chapter-31 decision was reopened.

## 30. Document-role assessment

The chapter is primarily timeless and architectural, but these phrases are roadmap/progress language:

- “first production executor” — line 23777
- “preferred first parallel design” — line 23887
- “first parallel scheduler” — line 23936
- “future optimization” — line 23951
- “future multi-query scheduler” — lines 23959–23965
- “deferred until ... measured” — line 23971
- “profiling-driven later work” — line 23989
- “initial execution” and “added only when profiles show benefit” — lines 24018–24020

**N32-2 — EDITORIAL**

Replace these with timeless capability/optionality wording during the Chapter-32 architecture repair. No semantic policy change is required.

## 31. Complexity assessment

| Area | Classification |
|---|---|
| Fixed worker pool, morsels, local state, dependency DAG | CORE |
| Hash partition/build barriers and immutable probe state | JUSTIFIED ADVANCED |
| Exact aggregate state and legal FLOAT64 trees | JUSTIFIED ADVANCED |
| External sort merge/spill | JUSTIFIED ADVANCED |
| NUMA/SIMD/prefetch hooks | POSSIBLE OVERENGINEERING, but optional and non-semantic |
| Future work-stealing/multi-query controls | POSSIBLE OVERENGINEERING, but not required by baseline |

## 32. Required thought experiments

| Case | Owner | Required outcome | Assessment |
|---|---|---|---|
| A. Worker count changes | §§26.9, 29.3, 32.6 | Same semantic result; legal FLOAT64 tree variation only | Adequate |
| B. Morsel duplicated/omitted | §§20, 26.4.2, 32.2 | No duplication or omission | **Gap: N32-1** |
| C. Cancellation while epoch held | §§14.6, 26.7 | Stop safely, release epoch after worker use | Adequate |
| D. Teardown with active worker | §§3.3.6, 26.3.1, 39.3 | Quiesce/join before destruction | Adequate |
| E. Probe before build Finalize | §§28.7, 32.5 | Probe cannot start | Adequate |
| F. Freed worker-local VARCHAR | §§23, 24, 28.6 | Deep-copy or retain valid backing | Adequate |
| G. LEFT JOIN unmatched rows | §28.9 | Exact preserved-side multiplicity | Adequate |
| H. Combine order changes | §§29.3.4, 32.6 | Admitted exact state/tree rules | Adequate |
| I. Local aggregate overflow/error | §29.3 | Finalize-owned error; no narrowed subtotal | Adequate |
| J. Later sort merge read failure | §§30.1, 30.6, 39.3 | SELECT execution failure, no false success | Adequate |
| K. Finalize fails with successors queued | §§26.3.1, 32.8, 39.3 | Successors remain non-runnable; quiesce/clean | Adequate |
| L. Concurrent different failures | §§39.1.7, 39.3 | Owner-specific primary cause and stronger-failure handling | Adequate |
| M. One worker fails while others allocate | §§24.4–24.10, 26.7, 39.3 | Stop/quiesce and clean all query resources | Adequate |
| N. Long task ignores cancellation | §§26.7, 32.9 | Check at permitted chunk/block boundaries; no numeric fairness promise | Adequate |
| O. Scan advertised as ordered | §§27.1, 32.4 | No ordering unless an order-preserving operator exists | Adequate |
| P. Parallel worker publishes DML mutation | §§15, 31.7, 32.13 | Mutation remains single-worker/write-phase | Adequate |

## 33. Global contradiction search

No contradiction was found for:

- worker ownership;
- snapshot/CommandId/read-epoch use;
- hash-build readiness;
- immutable finalized hash state;
- aggregate Combine and exact state;
- sort finalization;
- cancellation;
- memory accounting;
- output lifetime;
- DML/DDL/VACUUM/ANALYZE coordinator ownership.

The only material correctness omission is N32-1.

The Chapter-32 roadmap language is separately classified as N32-2.

## 34. Existing Verification reuse inventory

Relevant existing procedures/sections include:

- `Parallel Execution Tests` — `docs/VERIFICATION.md:21302`
- `Pipeline Finalization and Resource Tests` — `docs/VERIFICATION.md:21239`
- V26 pipeline lifecycle, dependency, cancellation, and cleanup procedures
- V27 scan procedures, including `V27-R04` for worker partitions without gaps/overlap
- `Hash Join Tests` and V28 join procedures
- `Aggregate Tests` and V29 aggregate procedures
- `Sort Tests` and V30-A–O
- Chapter-24 resource/spill/accounting procedures
- Chapter-39 execution/error procedures
- Chapter-41 physical-execution obligations

## 35. Missing Chapter-32 Verification inventory

No Verification synchronization is being performed.

The future Chapter-32 Verification family should add or make explicit:

- adversarial morsel duplication/omission tests;
- complete/disjoint source-domain coverage for heap ranges, VALUES ranges, and spill ranges;
- claim ownership and cancellation interaction;
- worker-count equivalence using the source-coverage oracle;
- task-claim behavior when a worker stops or fails.

Existing operator-specific tests already cover most join, aggregate, sort, lifetime, dependency, and cancellation semantics.

## 36–41. Finding counts

```text
BLOCKING:
    0

MAJOR:
    1
    N32-1 — incomplete morsel coverage/claim contract

MINOR:
    0

EDITORIAL:
    1
    N32-2 — roadmap/progress wording

DESIGN-SCOPE QUESTIONS:
    0

FROZEN SEMANTIC QUESTIONS:
    0
```

## 42. Exact next Architecture action

Perform a narrowly scoped Chapter-32 Architecture repair covering:

1. complete and disjoint morsel partitioning;
2. exactly-once logical occurrence ownership where required;
3. non-overlapping claim semantics;
4. empty, omitted, duplicated, canceled, and failed morsel behavior;
5. source-specific preservation of snapshot/RID/read-epoch rules;
6. replacement of roadmap wording with timeless optionality language.

Do not change Chapters 31, 30, 29, or 26 except for minimal cross-reference synchronization if required.

## 43. Recommended next authorized project task

**CHAPTER 32 ARCHITECTURE FIX N32-1 — Morsel Coverage and Claim Ownership**

After that repair, perform a focused read-only Chapter-32 closure audit before synchronizing Verification.

## 44. Validation

```text
git diff --check:
    PASS

Build:
    NOT RUN

Tests:
    NOT RUN

Sanitizers:
    NOT RUN

Benchmarks:
    NOT RUN
```

## 45. Final repository confirmation

```text
HEAD:
    962f88d32b72f6caa5d240ef375a5ca2b8650dd7

Working tree:
    clean

Index:
    clean

AUDIT-CREATED CHANGES:
    NONE
```

CHAPTER 31 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 31 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 32 ARCHITECTURE:
    NEEDS ARCHITECTURE FIX

CHAPTER 32 VERIFICATION:
    NOT SYNCHRONIZED

CHAPTER 33 REVIEW:
    NOT STARTED

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE