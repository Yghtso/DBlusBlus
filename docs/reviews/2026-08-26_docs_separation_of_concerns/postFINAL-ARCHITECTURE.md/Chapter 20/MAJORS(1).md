## Verdict

Chapter 20 major semantic integration is complete.

- D20-M1–D20-M6: **CLOSED**
- F20-M1–F20-M6: **CLOSED**
- D20-B1/D20-B2 and F20-B1/F20-B2: remain **CLOSED**
- Frozen Chapter-20 semantic questions: **NONE**
- Chapter 20: **SEMANTICALLY CLEAN**
- Chapter 20: **NOT YET DOCUMENT-CLEAN / NOT FULLY CLOSED**
- F20-N1–N6: **OPEN / UNCHANGED**

## Repository state

Initial:

- HEAD: `e87ba864319eb68d53f9b7ef9caf9296c1c8eb9d`
- Working tree: clean
- Index: clean
- Pre-existing architecture diff: none

Final:

- HEAD unchanged: `e87ba864319eb68d53f9b7ef9caf9296c1c8eb9d`
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- `git diff --check`: passed
- Diff: 334 insertions, 43 deletions
- No external or unrelated repository changes observed

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16261) was modified. All task hunks fall between the Chapter-20 and Chapter-21 boundaries.

No review artifact was read, modified, created, or staged.

## Sections modified

- §§20.1–20.2
- §§20.4–20.12
- §20.14.3
- §20.17 preamble
- §20.17.5
- §20.18
- §20.20

Chapters 1–19 and Chapter 21 onward remain unchanged.

## D20-M1 — LogicalSlotId

[§20.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16303) now defines:

- One opaque runtime identity per logical output occurrence.
- Whole-top-level-statement uniqueness, including nested blocks, side plans, derived children, and DML relational children.
- No reuse during the statement lifetime.
- No persistence, WAL, recovery, pointer, width, or allocation-order semantics.
- Fresh slots for Get columns, Values columns, every Project position, aggregate outputs, and derived-table exports.
- Pass-through slots for Filter, Join, Distinct, Sort, and Limit.
- Distinct slots for `SELECT a,a`, aliased duplicates, and equivalent expressions.
- Self-join distinction through separate BindingIds and LogicalSlotIds.
- Rewrite preservation only for the same surviving consumer-visible occurrence; new or duplicated occurrences receive fresh IDs.

Derived tables create fresh outer slots with explicit child-to-outer mappings.

## D20-M2 — Bag semantics

[§20.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16263) now establishes relations as bags of row occurrences:

- Equal-valued rows do not merge implicitly.
- Multiplicity and ordering are independent.
- Get emits one occurrence per visible logical row.
- Values preserves every listed row, including duplicates.
- Filter maps TRUE to one occurrence and FALSE/UNKNOWN to zero.
- Project produces one occurrence per input occurrence without deduplication.
- Sort preserves the complete bag.
- Limit selects occurrences without duplicate collapse.
- Derived boundaries preserve child multiplicity.
- DISTINCT and grouping are the explicit equivalence-class collapsing operations.

No physical scan or VALUES source order became semantic.

## D20-M3 — Join semantics

[§20.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16527) now defines:

- INNER: every `L × R` occurrence pair with ON TRUE emits exactly one row; FALSE/UNKNOWN emit none.
- LEFT: every TRUE pair is emitted; zero TRUE matches for a left occurrence emits exactly one NULL-extended row.
- CROSS: exactly one output per pair, including `m*n` duplicate multiplicity.
- Exact empty-side behavior for all three join types.
- Output schema is left child outputs followed by right child outputs.
- Child LogicalSlotIds pass through unchanged.
- LEFT right-side outputs become logically nullable without changing catalog nullability, TypeId, BindingId, slot identity, or lineage.
- No logical join establishes result-row order or mandates a physical join algorithm.

## D20-M4 — LogicalLimit

[§20.12](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16749) now preserves Chapter-19 ownership of folded/residual count expressions and execution-start validation.

The local relational contract is:

```text
after_offset = max(0, n - o)

without LIMIT: output cardinality = after_offset
with LIMIT:    output cardinality = min(after_offset, l)
```

It additionally defines:

- OFFSET before LIMIT.
- No `offset + limit` arithmetic requirement.
- Exact zero and oversized-count behavior.
- Ordered-child prefix skipping and surviving-order preservation.
- Any cardinality-valid sub-bag for unordered children, with no physical order becoming semantic.
- Complete schema, slot, display metadata, type, nullability, and lineage preservation.
- No duplicate elimination or new count error.

## D20-M5 — Aggregate ordinal handoff

[§20.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16640) now requires:

- Every logical aggregate entry carries its Chapter-19/§29.3.7 canonical ordinal.
- Distinct source aggregate occurrences remain distinct even when descriptors and arguments match.
- Every aggregate occurrence receives a fresh LogicalSlotId.
- Aggregate ordinal and output slot identity remain separate.
- ORDER alias references reuse existing aggregate output identity.
- Rewrites cannot renumber, compact, reuse, merge, duplicate under one ordinal, or move aggregates between query blocks.
- Removing an occurrence removes only its ordinal; remaining ordinals stay unchanged.
- Physical sharing is permitted only behind mappings preserving every logical occurrence, slot, ordinal, provenance, output, and lowest-ordinal error behavior.

## D20-M6 — Diagnostic provenance

[§20.17](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17391) now defines:

- Source-derived executable expressions retain SourceSpan, cast provenance, error-origin metadata, and precedence-relevant occurrence identity.
- Movement preserves origin exactly.
- Structural sharing preserves semantic origin independently of pointer sharing.
- Duplication copies origin but remains subject to D20-B1.
- Error-capable replacement must preserve category, canonical origin, and frozen precedence.
- Spanless synthesized executable expressions require exact total/error-free proof over their complete demand domain.
- Error-capable synthetic expressions must implement exactly one identifiable source occurrence and inherit its origin.
- Arbitrary synthetic or borrowed diagnostic spans are forbidden.
- Non-executable proof/search metadata needs no SourceSpan and cannot originate a user error.
- Provenance container representation remains implementation-defined.
- §17.10.2 retains constant-folding provenance ownership.
- Chapter-18 retain-or-materialize lifetime rules remain unchanged; source-buffer retention is not required.
- D20-B1 demand safety and D20-B2 child-order preservation remain independent rewrite prerequisites.

## Cross-dependency synchronization

The integrated package now explicitly composes:

- M1 ↔ M3: join schema and slot pass-through.
- M1 ↔ M4: Limit schema/slot preservation.
- M1 ↔ M5: fresh aggregate output slots.
- M2 ↔ M3: occurrence-pair join multiplicity.
- M2 ↔ M4: occurrence selection without deduplication.
- M5 ↔ M6: ordinal-specific aggregate provenance.
- M5/M6 ↔ B1: demand-safe aggregate and provenance rewrites.
- M6 ↔ B2: source responsibility follows preserved executable child order.

Logical output schemas retain slot identity, display metadata, logical type, nullability, and lineage without defining persistent or C++ representations.

[§20.18](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17762) validates whole-statement slot identity, node-specific slot rules, join schema/nullability, aggregate ordinals, Limit pass-through, provenance, D20-B1, and D20-B2.

[§20.20](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17821) consolidates the package as normative invariants.

## Reread answers 1–175

All questions have the required polarity:

- **YES:** 1–94, 100–111, 113–116, 118, 122–135, 140–175.
- **NO, as required:** 95–99, 112, 117, 119–121, 136–139.

The NO answers confirm:

- no ordinal renumbering, compacting, reuse, merging, or cross-block movement;
- no arbitrary replacement span;
- no arbitrary error-capable spanless synthetic expression;
- no SourceSpan requirement or user-error capability for non-executable metadata;
- no mandatory provenance container;
- no persistence change;
- no implementation algorithm mandate;
- no changes to Chapters 17–19 or Chapter 29.

No frozen cross-owner conflict was found.

## Finding status

| Decision/finding | Status |
|---|---|
| D20-M1 / F20-M1 | CLOSED |
| D20-M2 / F20-M2 | CLOSED |
| D20-M3 / F20-M3 | CLOSED |
| D20-M4 / F20-M4 | CLOSED |
| D20-M5 / F20-M5 | CLOSED |
| D20-M6 / F20-M6 | CLOSED |
| D20-B1 / F20-B1 | CLOSED |
| D20-B2 / F20-B2 | CLOSED |
| F20-N1–N6 | OPEN / UNCHANGED |

DISTINCT, grouping, subquery, NULL-ordering, unordered-LIMIT, persistence, recovery, and upstream binding semantics remain unchanged.

## Diff classification

All requested hunk classes A–AS are represented:

- A–I: LogicalSlotId definition, lifecycle, node allocation/pass-through, derived remapping, rewrite stability.
- J–P: bag baseline and operator multiplicity.
- Q–W: INNER/LEFT/CROSS formulas, empty behavior, schema, nullability, and no-order rule.
- X–AC: Limit ownership, composition, formulas, ordered/unordered behavior, and pass-through.
- AD–AH: aggregate ordinal handoff, occurrence identity, slots, rewrite stability, and physical sharing.
- AI–AN: source provenance, transformations, synthetic expressions, metadata, folding, and lifetime.
- AO: validator synchronization.
- AP: invariant synchronization.
- AQ: exact upstream/downstream ownership references.
- AR: analytical rationale.
- AS: local wrapping limited to edited semantic sentences.

## Final status and boundaries

- Next task: Chapter-20 document-only cleanup for F20-N1–N6.
- Chapter-20 verification synchronization: **NOT STARTED**.
- Chapter-21 direct review: **NOT STARTED**.
- Phase 2: **NOT STARTED / NOT AUTHORIZED**.

No build, test, benchmark, implementation work, staging, commit, devlog, or review artifact occurred.
