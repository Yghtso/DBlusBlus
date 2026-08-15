# Rewrite Pass 11 — Logical Planning, DDL/DML Semantics, Rewrites, and Front-End Contracts

## Source and scope

- source: `ARCHITECTURE(4).md`
- source SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed sections: `359..433`
- source lines: `11299..13317`
- next untouched section: `434. Vectorized Physical Execution Engine Contract`
- legacy architecture modified: **no**
- production code modified: **no**
- execution §434+ migrated: **no**

Working architecture:

```text
input  SHA-256: b8ef973ae7abec372434b07d0718c06eb3bc29ec866d2eb1813af3779c67e3ec
output SHA-256: d8b21d116d9545118f309fe19c7363dde48a9403162c65abfebb3419b7990513
```

## Canonical result

Chapter 20 now owns immutable logical nodes, query-local `LogicalSlotId`, logical Get/Values/Filter/Project/Join/Aggregate/Distinct/Sort/Limit/DML nodes, hidden RID slots, subquery semantics, logical properties, canonical SELECT construction, rewrites, validation, and logical EXPLAIN.

Chapter 21 now owns conservative DDL concurrency, snapshot-aware catalog visibility/cache publication, durable catalog-object identity allocation, CREATE TABLE/PRIMARY KEY/offline CREATE INDEX/DROP retirement, DML binding, RETURNING, SQL-v1 scope, and explicit deferrals.

## Precision completions

- Logical plan `SlotId` is named `LogicalSlotId` to avoid collision with persisted heap SlotId.
- V1 ORDER BY defaults are `ASC -> NULLS FIRST` and `DESC -> NULLS LAST`.
- GROUP BY/DISTINCT group NULLs together and use FLOAT64/VARCHAR equivalence compatible with the type/index contracts.
- Offline CREATE INDEX blocks/drains target-table writers through a conservative writer gate while readers continue; the private index is published only after COMMITTED.
- DROP physical unlink waits for global catalog invisibility plus descriptor/storage-owner quiescence.
- Catalog cache is explicitly not a visibility authority.

## R-037 resolved

`database.control` now persists `next_catalog_object_id` and its v1 header grows from 80 to 88 bytes. TableId/IndexId/ConstraintId allocation is durable-before-use, gap-tolerant, nonzero, and non-reused. ColumnId remains table-local/non-reused.

## Open gaps

- `R-036`: byte-exact catalog bootstrap / CATALOG_DATA representation.
- `R-040`: byte-exact persistent default-expression representation.

No encoding was guessed for either.

## Cross-cutting consolidation

Source testing/fuzzing/benchmark/error material is consolidated into §§39.2, 40.5, 41.4, and 42.3. Concrete source-tree, implementation-order, milestone, and historical-status sections §§428–433 are classified rather than copied as architecture.

## Coverage

```text
legacy §§0..433   complete / explicitly disposed
legacy §§434..725 pending
```

## Validation

- legacy SHA unchanged,
- §359 starts at 11299 and §434 at 13318,
- Chapters 20/21 occur exactly once,
- control-slot header and CRC offsets are internally consistent,
- DDL writer-gate ordering is synchronized into Chapter 15,
- every row through §433 is non-PENDING,
- every row §434+ remains PENDING,
- no production code or legacy architecture changed.

## Exit status

**PASS 11 COMPLETE.**

## Post-write coherence audit

**PASS.**

The independent audit verified:

- no stale 80-byte control-header/old CRC-offset wording,
- DDL statements do not implicitly auto-commit inside explicit transactions,
- Chapter 15 and Chapter 21 agree on writer-gate acquisition/release and lock ordering,
- the catalog cache remains snapshot-aware rather than a visibility authority,
- logical `LogicalSlotId` does not collide with persisted heap `SlotId`,
- grouping/DISTINCT and sort NULL semantics are explicit,
- expression vectorizability/nullability obligations from legacy §§406–407 are preserved,
- all current dotted section cross-references resolve,
- R-037 is resolved and R-036/R-040 remain explicit,
- legacy §434+ remains outside Pass 11.

Final architecture SHA-256:

```text
d8b21d116d9545118f309fe19c7363dde48a9403162c65abfebb3419b7990513
```
