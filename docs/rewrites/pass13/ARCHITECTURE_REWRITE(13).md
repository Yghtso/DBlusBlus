# Rewrite Pass 13 — Execution Algorithms and Runtime System

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `479..567`
- processed source lines: `14628..17112`
- next untouched section: `568. Cost-Based Optimizer Contract`
- legacy architecture modified: **no**
- production code modified: **no**
- optimizer §568+ migrated: **no**

Working architecture:

```text
input  SHA-256: 57d039e21ed307cfa74b8fc8d78433215d67cf8081e88947efd6ae54a9f1e138
output SHA-256: 762a3e7e7d7d9c4f49d1059beb7f450ed0609499238d4a5e3082db0e5bf5c787
```

## Canonical execution chapters

Pass 13 replaces the remaining execution placeholders with:

```text
Chapter 28  Join Execution
Chapter 29  Aggregation and DISTINCT
Chapter 30  Sorting and Top-N
Chapter 31  DML, DDL, VACUUM, and Result Interface
Chapter 32  Parallel Execution and Scheduling
```

and completes execution-specific contracts in Chapters 22, 26, 39, 40, 41, and 42.

## Join execution

Canonicalized:

```text
NestedLoop          small materialized inner baseline
IndexNestedLoop     small outer + selective indexed inner
HashJoin            main equality join
MergeJoin           ordered-input later algorithm
```

Hash join uses RowCollection build storage, a power-of-two open-addressed hash/key directory, duplicate chains, full equality validation after hash match, ~0.70 target maximum directory load, build/probe pipeline dependency, continuation state, residual predicates, LEFT unmatched output, Grace partition spill, bounded recursive repartition, and controlled skew fallback.

Pass-13 precision completion fixes LEFT hash orientation to logical-right build / logical-left preserved probe.

Hash values remain query-local and use explicit ordinary-join versus grouping equality modes.

## Aggregation and DISTINCT

The aggregate state API is exact enough for vectorized update, local partial state, Combine, Finalize, and spill.

Initial result signatures are explicit for COUNT/SUM/MIN/MAX/AVG.

GROUP BY/DISTINCT use grouping equality where NULLs group together while ordinary join equality still treats NULL as non-matchable.

Global aggregation uses one state block without a hash table.

Hash-aggregate spill may partition raw group-key/argument rows rather than turning compiler-native aggregate-state bytes into an accidental spill compatibility format.

## Sort and Top-N

PhysicalSort uses compact sort records, an order-preserving normalized prefix, the exact semantic comparator, in-memory comparison sort, sequential sorted spill runs, and k-way/multi-pass external merge.

Temporary sort runs are checksummed/query-local and never WAL logged or crash recovered.

Top-N uses checked `K=N+OFFSET`, a bounded heap of at most K records, then sorts retained rows before OFFSET/LIMIT emission.

## DML / Halloween / results

UPDATE/DELETE finalize a target spool before mutation.

A target physical RID appears at most once per statement attempt, either by proven child uniqueness or explicit spool de-duplication.

DML revalidation is synchronized with resolved R-033:

```text
retry-requiring RC conflict before first persistent statement write:
    discard spool/RETURNING and rebuild with fresh snapshot

after first persistent statement write:
    no same-TxnId whole-statement restart
    transaction abort/conflict
```

INSERT/UPDATE/DELETE retain the lower Chapter-15 write/WAL/lock protocols.

V1 DML mutation is single-worker.

RETURNING remains buffered/spillable through successful statement completion before external emission.

Client-visible result chunks own/retain memory and the baseline synchronous cursor lifetime is explicit.

Physical DDL and VACUUM remain management/control operators rather than artificial vector hot paths.

## Physical properties / pipeline breakers

Physical properties now explicitly include ordering, keys, partitioning, rewindability, blocking/streaming nature, estimates, memory, and spill capability.

Pipeline breakers declare memory state, spillability, Finalize transition, post-finalize source state, and dependent pipelines.

Canonical dependencies cover hash build/probe, sort, aggregate, and DML target spool.

## Parallel runtime

The canonical parallel architecture is:

```text
fixed worker pool
morsel tasks
global ready-task queue
dependency counters
worker-local operator state
```

with heap-scan morsels initially around 64–256 pages.

Parallel hash build uses local RowCollections/partitioned finalize; probe sees immutable finalized state.

Parallel aggregate uses local group tables plus Combine.

Parallel sort generates local runs then merges.

Ordered properties are preserved only through an order-preserving final source/merge, never arbitrary worker interleaving.

DML mutation, DDL, and VACUUM remain single-coordinator in the v1 baseline.

NUMA, lock-free work stealing, pervasive hand-SIMD, and explicit prefetch remain evidence-driven/deferred.

## Profiling / errors / verification / performance

Execution profiling now owns operator, pipeline, memory, spill, and actual-row counters plus EXPLAIN ANALYZE.

Physical plan validation occurs before data-changing execution.

Execution errors distinguish OOM/spill/cancel/cardinality/cast/arithmetic/constraint/transaction-conflict families.

Integer arithmetic is checked and never relies on signed-overflow UB.

Integer division/remainder by zero raises ArithmeticError; FLOAT64 division follows explicit IEEE-754 binary64 behavior.

Execution verification and performance requirements from §§546–558 are consolidated into Chapters 41–42 rather than retained as milestone checklists.

## Classification of §§560–567

- concrete execution module tree: excluded from architecture per R-006,
- implementation order: excluded per R-003,
- execution milestones: excluded as project planning while their unique correctness/performance obligations remain in Chapters 28–32/41–42,
- historical execution-layer status snapshot: excluded per R-004; Part VII is the canonical optimizer transition.

## Issues

No new unresolved execution architecture gap was introduced.

Pass 13 adds resolved issue notes:

```text
R-042 execution semantic completions
R-043 client result-chunk lifetime completion
```

R-033 is explicitly synchronized through the physical DML layer.

Existing open upper-layer format gaps remain:

```text
R-036 catalog bootstrap / CATALOG_DATA format
R-040 persistent default-expression encoding
```

## Coverage

```text
legacy §§0..567     complete / explicitly disposed
legacy §§568..725   pending
```

All 89 Pass-13 sections have explicit dispositions.

## Validation

Mechanical validation confirms:

- pinned legacy SHA unchanged,
- §479 begins at line 14628,
- §568 begins at line 17113,
- Chapters 28–32 occur exactly once,
- LEFT hash preserved-side orientation is explicit,
- grouping/join hash modes are distinct,
- aggregate result signatures are explicit,
- Top-N final-order rule is explicit,
- DML retry text matches resolved R-033,
- result-chunk lifetime is explicit,
- physical plan validation precedes data-changing execution,
- all coverage rows through §567 are non-PENDING,
- every coverage row from §568 remains PENDING,
- no production code or legacy architecture was modified.

## Post-write coherence audit

A separate cross-chapter sweep synchronized:

- aggregate return types/nullability back into binder semantics (§19.10),
- the stronger successful-statement RETURNING publication boundary into Chapters 15 and 21,
- IEEE FLOAT64 arithmetic/division back into the type layer,
- immutable transaction/snapshot/CommandId consumption by parallel read workers.

No new open issue was created by this sweep.

## Exit status

**PASS 13 COMPLETE.**
