## Chapter-20 blocking-integration verdict

**D20-B1 and D20-B2 are integrated and CLOSED. F20-B1 and F20-B2 are CLOSED.**

Chapter 20 still has six open MAJOR and six open MINOR findings, so it is not semantically clean, document-clean, or fully closed.

### Repository state

- Initial status/index: clean
- Initial HEAD: `9e08dd633b16443d84b37abe3462d818357258d6`
- Pre-existing changes: none
- Review artifacts: not read, modified, created, or staged
- No external repository changes observed during the task

## Sections modified

Only Chapter 20 in [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md):

- [§20.6 LogicalFilter](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16397)
- [§20.7 LogicalProject](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16416)
- [§20.8.1 Join node](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16439)
- [§20.9 LogicalAggregate](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16505)
- [§20.11 LogicalSort](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16545)
- [§20.17 Logical rewrites](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17173)
- [§20.17.3 Predicate pushdown](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17250)
- [§20.17.4 Projection pruning](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17269)
- [§20.17.5 Expression canonicalization](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17295)
- [§20.17.8 Contradiction detection](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17366)
- [§20.17.10 Semantic emptiness](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17422)
- [§20.18 Logical-plan validation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17484)
- [§20.20 Invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17532)

## D20-B1 final semantics

- A demanded evaluation is one required by unrevised logical semantics for a particular row, candidate pair, group, or other relational occurrence.
- Demand is independent of optimizer scheduling, physical visitation, vector lanes, CPU counts, and page/RID order.
- `LogicalFilter` demands its predicate for every child row whose retention it must determine.
- Ordinary `LogicalProject` demands every declared expression for every produced Project row.
- Join predicates are demanded only for candidate pairs that logical join semantics require to test.
- Group keys, aggregate arguments, HAVING predicates, and sort keys are demanded only over their applicable logical rows or groups.
- Specialized §20.14 scalar-subquery, EXISTS, IN, lazy-demand, and short-circuit rules remain authoritative.

A rewrite is legal only if:

1. it preserves correctness-observable demanded evaluations, including value context, NULL behavior, Chapter-17 order, short-circuiting, error category, existing responsible `SourceSpan`, and frozen precedence; or
2. exact semantic proof establishes that changing demand cannot alter any observable result.

A sufficient exact proof establishes total, error-free, deterministic behavior over the complete newly affected domain. Statistics, samples, estimates, costs, heuristics, and likely ranges are insufficient. Unproven expressions are conservatively treated as potentially erroring.

Consequences:

- Predicate pushdown requires demand safety in addition to slot and join conditions.
- Projection nonuse alone does not authorize pruning a demanded potentially erroring expression.
- Exact emptiness alone does not authorize suppressing demanded erroring work.
- Existing source-derived error category, `SourceSpan`, and frozen precedence must survive.
- Resource usage, allocation count, spill volume, and incidental physical work are not logical demand effects.
- No new physical row-, page-, RID-, hash-, or thread-order error precedence was introduced.

## D20-B2 final semantics

Executable scalar trees preserve Chapter-17 semantic child order:

- commutative arithmetic operands cannot be swapped;
- executable comparisons cannot be reversed for canonical ordering;
- AND/OR operands cannot be reordered;
- associative reassociation and child sorting are prohibited;
- CASE branch order and Boolean short-circuiting remain unchanged.

Value commutativity is explicitly distinguished from evaluation commutativity because errors, short-circuiting, and diagnostic spans can differ.

Commutative normalization remains permitted for non-executable:

- equivalence facts;
- lookup/search keys;
- proof facts;
- normalized comparison descriptors.

Such metadata cannot replace the executable tree, alter child order, redefine an error or `SourceSpan`, or become a user-visible expression.

Chapter-17-authorized §17.10.2 folding remains permitted. General synthesized-expression provenance remains open under F20-M6.

## Rewrite-by-rewrite audit

| Rewrite | Classification | Result |
|---|---|---|
| §20.17.1 constant folding | C — specialized frozen rule | §17.10.2 remains authoritative |
| §20.17.2 Boolean simplification | C | Preserves 3VL, source order, skipped errors, and short-circuiting |
| §20.17.3 predicate pushdown | B | Demand-changing movement requires exact proof |
| §20.17.4 projection pruning | B/C | Ordinary demand requires proof; explicit EXISTS exception remains |
| §20.17.5 expression canonicalization | A/C | Executable order preserved; only non-executable metadata may normalize |
| §20.17.6 join graph extraction | A | Non-executable metadata; executable predicates unchanged |
| §20.17.7 equality classes | A | Proof metadata; executable order unchanged |
| §20.17.8 propagation/contradiction | B | Demand-changing replacement requires exact safety |
| §20.17.9 trusted-key metadata | A | Metadata only; subsequent rewrites remain governed by §20.17 |
| §20.17.10 semantic emptiness | B/C | Exact proof plus demand safety required |

Category D violations after integration: **0**.

## Required case results

Predicate pushdown:

- Error-capable left predicate over INNER JOIN with unmatched left rows: pushdown is not generally legal.
- If exact proof establishes total/error-free behavior over all newly tested rows: pushdown may be legal if all other conditions hold.
- LEFT JOIN: existing null-extension restrictions and demand safety both apply.

Projection pruning:

- Unused ordinary error-capable projection: ancestor nonuse alone is insufficient.
- Unused total/error-free projection: may be removed under exact proof if other semantics are unchanged.
- EXISTS projection: existing specialized irrelevance rule permits skipping/pruning.

Operand order:

- Erroring arithmetic, comparison, AND, and OR operands retain source child order.
- `dangerous_a < dangerous_b` cannot become executable `dangerous_b > dangerous_a`.
- Symmetric proof metadata may record equivalence without replacing executable order.

Contradiction/empty result:

- Exact empty output does not alone permit erasing demanded potentially erroring work.
- Demand preservation or exact insensitivity proof is additionally required.

## Acceptance reread

- Questions 1–34: **YES**
- Question 35, “Can metadata replace executable expression order?”: **NO**
- Question 36, “Can metadata redefine SourceSpan?”: **NO**
- Questions 37–74: **YES**

This confirms:

- §§17.6, 17.7.3, and 17.10.2 remain unchanged and authoritative.
- Chapters 17–19 and Chapter 21+ are unchanged.
- §20.14 subquery semantics, DISTINCT/grouping, NULL ordering, and unordered LIMIT semantics are unchanged.
- No persistent-format, recovery, binder, or physical-order semantics changed.
- No new frozen semantic conflict was found.

## Finding status

- D20-B1: **CLOSED**
- D20-B2: **CLOSED**
- F20-B1: **CLOSED**
- F20-B2: **CLOSED**

Unchanged and open:

- F20-M1: **OPEN**
- F20-M2: **OPEN**
- F20-M3: **OPEN**
- F20-M4: **OPEN**
- F20-M5: **OPEN**
- F20-M6: **OPEN**
- F20-N1–N6: **OPEN**

No `LogicalSlotId` lifecycle, complete bag contract, join multiplicity/schema order, complete `LogicalLimit` contract, aggregate ordinal handoff, or general synthesized-expression provenance was added. No document cleanup or verification synchronization was performed.

## Diff classification

- A: demanded-evaluation definition
- B: Filter demand
- C: Project demand
- D: join/group/order demand
- E: general rewrite-safety invariant
- F: exact total/error-free proof
- G: predicate-pushdown safety
- H: projection-pruning safety
- I: contradiction/exact-empty safety
- J: error/category/SourceSpan minimum
- K: validator and invariant synchronization
- L: executable operand-order prohibition
- M: commutativity/evaluation distinction
- N: comparison-reversal prohibition
- O: Boolean operand-order preservation
- P: non-executable metadata allowance
- Q: constant-folding preservation
- R: analytical rationale
- S: local wrapping

## Final state

- Files changed: only `docs/ARCHITECTURE.md`
- Diff: 136 insertions, 11 deletions
- Final status: `M docs/ARCHITECTURE.md`
- Index: clean
- Final HEAD: `9e08dd633b16443d84b37abe3462d818357258d6`
- `git diff --check`: passed
- No staging or commit
- No build, tests, or benchmarks
- No implementation work
- No review artifact or devlog
- No external repository change observed

Chapter 20 status:

- **BLOCKING FINDINGS CLOSED**
- **MAJOR SEMANTIC REVIEW STILL REQUIRED**
- **NOT SEMANTICALLY CLEAN**
- **NOT DOCUMENT-CLEAN**
- **NOT FULLY CLOSED**

Next task: Chapter-20 major semantic review for F20-M1–M6.

Chapter-20 verification synchronization: **NOT STARTED**
Chapter-21 direct review: **NOT STARTED**
Phase 2: **NOT STARTED / NOT AUTHORIZED**
