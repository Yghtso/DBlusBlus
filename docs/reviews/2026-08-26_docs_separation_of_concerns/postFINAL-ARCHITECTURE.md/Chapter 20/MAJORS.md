# Chapter-20 major-semantic-review verdict

**A coherent D20-M1–D20-M6 semantic package is recommended and awaits architecture-owner approval.**

No frozen cross-owner conflict was found. The recommendations are compatible with Chapters 16–19, Chapter 29, D20-B1, D20-B2, and downstream physical-planning contracts.

This was read-only. No files changed.

## Repository state

- Initial status/index: clean
- Initial HEAD: `ea2a43c3c388734dd2f0ea67347bef482f3acabd`
- Final status/index: clean
- Final HEAD: `ea2a43c3c388734dd2f0ea67347bef482f3acabd`
- `git diff --check`: passed with no output
- Audit-created changes: **NONE**
- Review artifacts: not read, created, modified, moved, or staged

# F20-M1 — LogicalSlotId

Relevant live sections:

- [§20.2 Logical output schema and LogicalSlotId](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16291)
- [§20.4 LogicalGet](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16356)
- [§20.7 LogicalProject](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16416)
- [§20.9 LogicalAggregate](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16505)
- [§20.14.3 Derived tables](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16725)
- [§20.18 Logical-plan validation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17484)
- Upstream: §§16.3, 16.8, 19.2, 19.3, 19.5 and 19.7

## Exact gap

`LogicalSlotId` currently identifies “one semantic value flowing through a logical plan,” but “query-local” does not define:

- query-block versus top-level-statement uniqueness;
- reuse after an output disappears;
- duplicate projection outputs;
- derived-table remapping;
- mandatory pass-through preservation;
- when equivalent expressions constitute the same occurrence.

This is a genuine logical-identity gap.

## Identity comparison

| Identity | Semantic object | Recommended domain | Persistent? | Duplicate sharing |
|---|---|---|---|---|
| `TableId` | Catalog table | Database/catalog | Yes | Same table shares |
| `ColumnId` | Catalog column within table | Table-local | Yes | Same source column shares |
| `BindingId` | Relation occurrence | Whole top-level statement | No | Never between distinct occurrences |
| `LogicalSlotId` | Logical output occurrence | Whole top-level statement | No | Never between distinct output occurrences |
| Output ordinal | Position in ordered result schema | One output schema | No | Each position distinct |
| Aggregate ordinal | Source aggregate occurrence | Query block | No | Same source occurrence reused; distinct source occurrences do not share |

## Candidate uniqueness models

| Model | Assessment |
|---|---|
| Node-local | Rejected: child references, ordering properties, rewrites, and derived mappings would require additional hidden scoping policy. |
| Query-block-local | Viable but unnecessarily complex for nested side plans and cross-block EXPLAIN/optimizer identity. Every reference would require block-qualified identity. |
| Whole top-level statement | Recommended: directly matches the plan lifetime, prevents nested collisions, and composes with statement-wide `BindingId`. |
| Hierarchical `(block, local-id)` | Acceptable implementation of whole-statement uniqueness, but the representation must remain opaque and semantically globally distinct. |

## Recommended node rules

| Node | Slot rule |
|---|---|
| `LogicalGet` | **FRESH** for each exposed `(BindingId, ColumnId)` output occurrence |
| `LogicalValues` | **FRESH** for each column position |
| `LogicalFilter` | **PASSTHROUGH** unchanged |
| `LogicalProject` | **FRESH** for every declared output position |
| `LogicalJoin` | **PASSTHROUGH**, concatenating child identities under M3 |
| `LogicalAggregate` | **FRESH** for each group-key output and aggregate-occurrence output |
| `LogicalDistinct` | **PASSTHROUGH** |
| `LogicalSort` | **PASSTHROUGH** |
| `LogicalLimit` | **PASSTHROUGH** |
| `LogicalSubqueryScan` | **REMAPPED** to fresh outer-boundary slots |
| Hidden computed/order output | **FRESH** |

`SELECT a,a`, `SELECT a AS x,a AS y`, and `SELECT a+1,a+1` therefore have two distinct projection `LogicalSlotId` values even when expression computation can be physically shared.

A derived table receives fresh exported slots because it establishes a new relation occurrence and namespace boundary. Its mapping records child slot → outer slot/`BindingId`/name.

## Rewrite-stability recommendation

A rewrite preserves a `LogicalSlotId` exactly when the same logical output occurrence, with the same consumer-visible definition and lineage, survives. Equivalent value computation alone does not establish identity.

- Surviving output: preserve ID.
- New output occurrence: fresh ID.
- Duplicated output occurrence: fresh ID for the duplicate.
- Eliminated output: ID disappears and is not reassigned.
- Project elimination/derived-boundary inlining: consumer-visible IDs remain through an explicit semantic mapping.
- Different statements may reuse opaque numeric representations.

### Recommended D20-M1

> `LogicalSlotId` is an opaque runtime identity for exactly one logical output occurrence and is unique across the entire top-level logical statement, including nested query blocks and side plans. It is never reused while that statement’s logical representation survives. Distinct output positions receive distinct identities even when their expressions or values are equivalent. Pass-through operators preserve identities; semantic output boundaries create fresh identities; derived-table boundaries remap child outputs to fresh outer identities. Rewrites preserve an identity only when the same logical output occurrence survives.

No width, numbering, allocator, or pointer representation is prescribed.

# F20-M2 — Core bag semantics

Relevant sections:

- [§§20.4–20.12 core operators](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16356)
- [§20.14.3 derived tables](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16725)
- [§20.14.10 clause composition](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16993)
- [§20.15 canonical SELECT shape](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17113)
- Upstream: §18.13 ordered source multiplicity and MVCC visibility owners

## Exact gap

Chapter 20 does not state a general bag-of-row-occurrences model or complete per-node occurrence mapping. DISTINCT and grouping imply bag input, but no rule prevents implicit deduplication in Get, Values, Filter, Project, Sort, Limit, or derived-table boundaries.

This is a genuine relational-semantic gap.

## Recommended multiplicity model

Logical relations are bags of row occurrences unless a named operator explicitly changes occurrence cardinality or equivalence classes.

- Value-equal rows remain distinct occurrences.
- Multiplicity is separate from row ordering.
- No operator deduplicates merely because rows or expressions compare equal.

| Operator | Recommended occurrence rule |
|---|---|
| Get | One occurrence per transaction-visible logical base row; value-equal rows coexist |
| Values | One occurrence per listed row; duplicate listed rows remain |
| Filter | One occurrence for each input occurrence whose predicate is TRUE; otherwise zero |
| Project | Exactly one output occurrence per input occurrence |
| Sort | Same occurrences and multiplicities, reordered |
| Limit | Selects/removes occurrences but never deduplicates |
| Derived table | Same child bag multiplicity across the namespace boundary |
| Join | M3 formulas |
| Distinct | One occurrence per grouping-equivalence class |
| Aggregate | One occurrence per group; global aggregate one group |
| EXISTS/IN/scalar | Existing specialized §20.14 semantics |

`LogicalGet` sees the row version selected by MVCC as one logical row occurrence. Physical historical tuple versions are not separate logical rows, but two visible rows with equal column values remain two occurrences.

VALUES source order remains relevant to syntax/provenance where frozen, but M2 should not turn it into an SQL ordering property. Semantic result order remains separately owned.

### Recommended D20-M2

> Every Chapter-20 logical relation is a bag of row occurrences. Equal row values do not merge unless an explicit operator—such as `LogicalDistinct` or grouping—defines an equivalence-class collapse. Get and Values construct occurrences; Filter and Limit remove occurrences according to their own rules; Project, Sort, and derived-table boundaries preserve occurrence multiplicity; joins produce occurrences under D20-M3. Bag multiplicity does not itself establish row order.

# F20-M3 — JOIN semantics

Relevant sections:

- [§20.8 Logical joins](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16437)
- [§20.8.3 LEFT JOIN null extension](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16484)
- [§20.17.3 predicate pushdown](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17250)
- Downstream consistency: §§28.3–28.13

## Exact gap

Chapter 20 defines join kinds and LEFT nullability but does not locally state:

- candidate-pair multiplicity;
- TRUE/FALSE/UNKNOWN results;
- duplicate-match behavior;
- exact unmatched-LEFT multiplicity;
- output schema order;
- CROSS Cartesian formula.

## Proposed formulas

Let `L` and `R` be bags of row occurrences.

### INNER JOIN

For every occurrence pair `(l,r) ∈ L×R`:

- evaluate ON under D20-B1;
- TRUE produces exactly one joined occurrence;
- FALSE or UNKNOWN produces none.

Duplicate left/right occurrences and multiple TRUE matches produce their complete multiplicative output.

### LEFT JOIN

For each left occurrence `l`:

- emit one joined occurrence for every right occurrence whose ON result is TRUE;
- FALSE and UNKNOWN are nonmatches;
- if no right occurrence produces TRUE, emit exactly one occurrence consisting of `l` plus a NULL-extended right side.

Thus:

- zero TRUE matches → exactly one extended occurrence;
- one TRUE match → one matched occurrence;
- multiple TRUE matches → one occurrence per match;
- only FALSE/UNKNOWN matches → exactly one extended occurrence.

### CROSS JOIN

Emit exactly one joined occurrence for each pair in `L×R`. If equivalent value rows occur with multiplicities `m` and `n`, their paired result multiplicity is `m*n`.

Empty-side behavior:

- INNER/CROSS with either side empty → empty.
- LEFT with empty left → empty.
- LEFT with empty right → one NULL-extended output per left occurrence.

## Schema and order

Recommended join schema:

1. every left-child output in left schema order;
2. every right-child output in right schema order.

Slot identities pass through unchanged. For LEFT JOIN, right-side logical nullability becomes nullable while catalog nullability and lineage remain available separately.

Logical joins establish no semantic row order and do not promise preservation of either child’s order. Physical pair-generation order remains nonsemantic.

### Recommended D20-M3

> INNER and LEFT JOIN consider the bag Cartesian product of their child occurrences. A pair contributes one output exactly when ON is TRUE; FALSE and UNKNOWN do not match. LEFT JOIN additionally emits exactly one NULL-extended right occurrence for each left occurrence having no TRUE match. CROSS JOIN emits the full Cartesian bag. Join output schema is left child outputs followed by right child outputs; LEFT makes every right output logically nullable. No logical join establishes row order.

# F20-M4 — LogicalLimit

Relevant sections:

- [§20.12 LogicalLimit](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16572)
- [§20.14.10 LIMIT/OFFSET composition](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16993)
- [§20.15 canonical shape](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17113)
- Upstream: §19.14
- Downstream: §27.9 and §35.20

## Exact gap

Chapter 19 fully owns count admissibility, folding, normalization, acquisition, and final validation. Chapter 20’s `LogicalLimit` section does not locally define the relational operation on already validated counts.

Downstream §27.9 already fixes OFFSET before LIMIT, while §35.20 supplies the compatible cardinality formula.

## Recommended local contract

Given child cardinality `n`, validated offset `o`, and optional limit `l`:

```text
after_offset = max(0, n - o)
output_count =
    after_offset                 if no LIMIT
    min(after_offset, l)         otherwise
```

This does not require computing `o+l`, avoiding overflow.

Behavior:

- OFFSET first, LIMIT second.
- OFFSET 0 removes nothing.
- LIMIT 0 returns no occurrences.
- LIMIT greater than remaining cardinality returns all remaining occurrences.
- OFFSET at least child cardinality returns none.
- No new count-domain error is introduced.

Ordered child:

- skip the first `o` ordered occurrences;
- retain at most the next `l`;
- preserve surviving order.

Unordered child:

- no hidden canonical “first” row exists;
- any sub-bag with the required output cardinality is allowed;
- no heap, page, RID, index, hash, or thread order becomes semantic;
- output remains unordered.

Schema and all child `LogicalSlotId` values pass through unchanged.

### Recommended D20-M4

> `LogicalLimit` applies validated OFFSET before validated LIMIT to child row occurrences. It removes at most OFFSET occurrences, then emits at most LIMIT remaining occurrences, without deduplication or `offset+limit` arithmetic. It preserves child schema and slot identities. On semantically ordered input it selects and preserves the corresponding ordered suffix-prefix; on unordered input the selected sub-bag is intentionally unspecified and no physical traversal order is semantic. Chapter 19 exclusively owns count evaluation and errors.

# F20-M5 — Aggregate ordinal handoff

Relevant sections:

- §19.10
- [§20.9 LogicalAggregate](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16505)
- [§20.17 rewrites](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17173)
- §29.3.7

## Exact gap

The behavior is already frozen across owners:

- §19.10 assigns an ordinal to each source aggregate occurrence.
- §29.3.7 defines ascending first source-byte occurrence within the query block and uses the lowest ordinal for aggregate-finalization error selection.
- Both require the ordinal to survive rewrites.

Chapter 20 fails to state how `LogicalAggregate` carries that identity or how physical sharing relates to distinct source occurrences. This is primarily a handoff/documentation gap with logical consequences.

## Recommended occurrence model

- Each distinct bound source aggregate occurrence remains a distinct logical aggregate occurrence.
- Identical source expressions at different source spans have different ordinals.
- Reuse of the same source occurrence—such as `ORDER BY` referencing `SELECT SUM(x) AS s`—does not create another occurrence or ordinal.
- Each logical aggregate occurrence receives its own aggregate output `LogicalSlotId`.
- Rewrites cannot renumber, reassign, merge, duplicate, or move the occurrence between query blocks.
- Removing an occurrence is allowed only when its semantic demand is legally removed under D20-B1; remaining ordinals are not compacted or reused.

Physical computation may be shared for equivalent occurrences if the plan retains:

- every logical occurrence and output mapping;
- each occurrence’s ordinal/provenance;
- behavior equivalent to distinct evaluation;
- lowest-ordinal aggregate error selection.

### Recommended D20-M5

> Every `LogicalAggregate` entry carries the canonical query-block aggregate ordinal assigned under §§19.10 and 29.3.7. Distinct source aggregate occurrences remain distinct logical occurrences and outputs even when descriptor and arguments are equivalent. References to an existing output or shared source occurrence reuse its occurrence and ordinal. Rewrites preserve ordinals without renumbering or cross-block movement. Physical computation may be shared only behind a mapping that preserves every logical occurrence, output identity, diagnostic provenance, and lowest-ordinal error behavior.

# F20-M6 — SourceSpan and diagnostic provenance

Relevant sections:

- §§18.8 and 18.14
- §§19.3, 19.6, 19.7 and 19.20
- [§20.17 rewrite safety](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17173)
- [§20.18 validation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17484)
- §§39.2–39.3

## Exact gap

D20-B1 preserves an existing source-derived error identity when demand changes, and D20-B2 preserves source child order. Still undefined:

- provenance when expressions move, share, or duplicate;
- replacement provenance;
- synthesized executable expression diagnostics;
- whether a provenance set is required;
- which synthetic expressions may lack a source span;
- folding and source-buffer boundaries.

This is a genuine runtime-diagnostic semantic gap.

## Recommended provenance model

### Source-derived occurrence

Every executable logical scalar occurrence originating from a bound expression retains its canonical diagnostic origin:

- `SourceSpan`;
- explicit/implicit cast provenance;
- any more-specific child error origin already represented;
- source occurrence identity needed for frozen precedence.

### Movement and sharing

Moving or structurally sharing an unchanged occurrence preserves exactly the same diagnostic origin. Pointer identity remains irrelevant.

### Duplication

A duplicated executable copy retains the same origin as the source occurrence. D20-B1 separately decides whether duplication is legal; provenance copying does not authorize additional demand.

### Replacement

If a replacement can raise a user-visible semantic error, it must:

- preserve the replaced occurrence’s externally observable error category, diagnostic origin, and precedence; or
- be rejected.

A different equivalent source expression cannot donate an arbitrary span.

### Synthesized executable expressions

A synthetic expression with no canonical source site is legal when exact proof establishes it is total and error-free over its complete demand domain.

An error-capable synthetic executable expression is legal only when it semantically implements exactly one existing source-derived occurrence and carries that occurrence’s diagnostic origin. Otherwise it is illegal.

Examples such as constant TRUE, slot identity, or a safe NULL marker may be spanless when exactly proven total/error-free. Synthesized arithmetic, cast, comparison, or predicate work cannot be assumed safe without the relevant Chapter-17 proof.

### Non-executable metadata

Equivalence classes, lookup keys, proof facts, estimates, and memo/search metadata need no diagnostic `SourceSpan` because they cannot themselves execute or raise a user-visible scalar error.

A provenance set may be retained for lineage/debugging, but a set alone is not a diagnostic-selection rule and is not required by the semantic contract.

### Folding and lifetime

- §17.10.2 remains the complete owner of folded value/error timing and provenance behavior.
- Numeric `SourceSpan` values may outlive source storage.
- Original SQL bytes need not remain resident.
- Any required rendered text follows §18.14 retain-or-materialize.
- No source-buffer retention is added.

### Recommended D20-M6

> Every source-derived executable logical scalar occurrence retains its canonical diagnostic origin through movement, sharing, duplication, and semantics-preserving rewrites. A potentially erroring replacement must preserve the replaced occurrence’s error category, origin, and frozen precedence. A synthesized executable expression without a canonical source origin is permitted only when exact proof establishes it is total and error-free; otherwise it must implement one identifiable source occurrence and inherit that occurrence’s origin, or it is illegal. Non-executable optimizer metadata has no diagnostic origin and cannot raise user-visible errors. Folding remains §17.10.2-owned, and source-buffer lifetime remains §18.14-owned.

# Dependency graph

```text
D20-M2 bag occurrences
    ├──> D20-M3 join pairing/multiplicity
    └──> D20-M4 Limit occurrence selection

D20-M1 LogicalSlotId
    ├──> D20-M3 join output schema
    ├──> D20-M4 schema/slot pass-through
    └──> D20-M5 aggregate output identity

D20-M5 aggregate occurrences
    ├──> D20-M6 diagnostic provenance
    └──> D20-B1 demanded aggregate evaluation

D20-M6 provenance
    ├──> D20-B1 error/demand preservation
    └──> D20-B2 executable child-order preservation
```

Recommended freezing order:

1. M1 and M2;
2. M3 and M4;
3. M5 and M6;
4. consolidate invariants and validator handoffs.

They should be approved as one package because isolated alternatives can produce incompatible slot, multiplicity, or diagnostic contracts.

# Consolidated proposed package

| Decision | Exact rule | Rejected alternatives | Compatibility and consequences |
|---|---|---|---|
| D20-M1 | Statement-wide opaque identity for one logical output occurrence; fresh at semantic output/remap boundaries, pass-through otherwise, no reuse | Node-local, unqualified block-local, expression-equivalence sharing | Compatible with IDs in Ch16/19; enables stable optimizer properties; no persistence impact; allocator/width free |
| D20-M2 | Logical relations are bags of row occurrences; no implicit deduplication | Implicit set semantics, unspecified duplicates, convention-only behavior | Compatible with DISTINCT, grouping, COUNT and DML affected rows; physical representation free |
| D20-M3 | Bag Cartesian candidate pairs; TRUE emits, FALSE/UNKNOWN reject; LEFT emits one extended row when no TRUE match; schema left then right | First-match joins, set joins, algorithm-defined schema/order | Compatible with Ch17 3VL, Ch19 source structure and Ch28; no algorithm prescribed |
| D20-M4 | OFFSET then LIMIT over occurrences; ordered prefix semantics or explicitly arbitrary unordered sub-bag; schema/slots preserved | Hidden physical first-row order, LIMIT-before-OFFSET, `offset+limit` arithmetic | Compatible with §§19.14, 27.9 and 35.20; no new errors |
| D20-M5 | Logical aggregate entries retain source occurrence ordinal; identical source occurrences remain distinct; physical sharing behind occurrence map only | Logical deduplication, renumbering, traversal-order identity | Directly consumes §§19.10/29.3.7; preserves D20-B1 errors; no execution strategy mandate |
| D20-M6 | Preserve canonical diagnostic origin for source-derived executable occurrences; spanless synthesis only when total/error-free; otherwise exact source-origin mapping required | Arbitrary inherited span, no-span errors, mandatory provenance set as sole selection rule | Compatible with Ch18/19/39 and D20-B1/B2; provenance representation and source storage free |

All six decisions:

- affect no persistent format;
- require no numeric ID width;
- require no monotonic numbering;
- require no particular logical node classes;
- require no allocator, map, memo, or provenance container;
- require no physical join, sort, aggregate, or Limit algorithm;
- require no source-buffer retention.

# Final questions 1–86

## M1 — Questions 1–20

| # | Answer |
|---:|---|
| 1 | One logical output occurrence at a semantic plan boundary. |
| 2 | Whole top-level statement, including nested blocks and side plans. |
| 3 | No reuse while that statement/plan survives. |
| 4 | Runtime/query-plan only; never persistent. |
| 5 | Fresh slot per exposed `(BindingId, ColumnId)` occurrence. |
| 6 | Fresh slot per Values column position. |
| 7 | Project creates fresh slots for every declared output. |
| 8 | Filter preserves child slots. |
| 9 | Join passes through child slots in left-then-right schema order. |
| 10 | Aggregate creates fresh group-key and aggregate-output slots. |
| 11 | Distinct preserves slots. |
| 12 | Sort preserves slots. |
| 13 | Limit preserves slots. |
| 14 | Derived boundary remaps to fresh outer slots. |
| 15 | Self-join Get outputs are distinct because their occurrences/BindingIds differ. |
| 16 | `SELECT a,a` has distinct projection slot IDs. |
| 17 | Equivalent projection expressions still receive distinct output IDs. |
| 18 | Preserve only when the same logical output occurrence survives. |
| 19 | Pointer identity is excluded. |
| 20 | No implementation-specific allocator is required. |

## M2 — Questions 21–30

| # | Answer |
|---:|---|
| 21 | Yes, logical relations are bags of occurrences. |
| 22 | Yes, Get preserves distinct visible rows with equal values. |
| 23 | Yes, Values preserves duplicate listed rows. |
| 24 | Filter maps each occurrence to one output on TRUE, otherwise zero. |
| 25 | Project produces exactly one occurrence per input occurrence. |
| 26 | Sort preserves multiplicity. |
| 27 | No; Limit selects occurrences but does not deduplicate. |
| 28 | Yes, derived-table boundaries preserve child multiplicity. |
| 29 | Filter, Join, Distinct, Aggregate and Limit change counts under explicit rules; only Distinct/grouping collapse value-equivalence classes. |
| 30 | Yes, row ordering is separate from multiplicity. |

## M3 — Questions 31–44

| # | Answer |
|---:|---|
| 31 | Every left occurrence paired with every right occurrence. |
| 32 | TRUE emits exactly one joined occurrence for that pair. |
| 33 | FALSE emits none. |
| 34 | UNKNOWN emits none. |
| 35 | Every duplicate occurrence and TRUE pair is retained. |
| 36 | LEFT emits every TRUE matching pair. |
| 37 | Zero TRUE matches emits exactly one NULL-extended right occurrence. |
| 38 | FALSE/UNKNOWN-only candidates count as zero TRUE matches. |
| 39 | Full Cartesian bag, multiplicity `m*n`. |
| 40 | INNER/CROSS empty if either side empty; LEFT preserves nonempty left. |
| 41 | Left schema in order, then right schema in order. |
| 42 | Every LEFT right-side output becomes logically nullable. |
| 43 | No logical join establishes row order. |
| 44 | No physical algorithm semantics are introduced. |

## M4 — Questions 45–59

| # | Answer |
|---:|---|
| 45 | Yes, OFFSET applies first. |
| 46 | Yes, LIMIT applies second. |
| 47 | LIMIT-only returns at most the first/selected `l` occurrences. |
| 48 | OFFSET-only removes up to `o` occurrences. |
| 49 | LIMIT 0 returns empty. |
| 50 | OFFSET 0 removes nothing. |
| 51 | Oversized limit returns all remaining occurrences. |
| 52 | Oversized offset returns empty. |
| 53 | Avoided by not computing `offset+limit`. |
| 54 | Semantic ordered input order is preserved. |
| 55 | Unordered input yields an arbitrary correctly sized sub-bag with no order guarantee. |
| 56 | Physical scan/page/index order remains nonsemantic. |
| 57 | Child schema is preserved. |
| 58 | Child slots are preserved under M1. |
| 59 | No new Chapter-19 count error is introduced. |

## M5 — Questions 60–70

| # | Answer |
|---:|---|
| 60 | §§19.10 and 29.3.7. |
| 61 | Yes, per query block. |
| 62 | Yes, ascending first aggregate source-byte occurrence. |
| 63 | Recommended: carried explicitly by each LogicalAggregate entry. |
| 64 | Two distinct source occurrences remain distinct. |
| 65 | No; ORDER alias references the existing output/occurrence. |
| 66 | No renumbering. |
| 67 | No logical occurrence merge. |
| 68 | Physical computation may share when observationally equivalent. |
| 69 | Sharing retains a mapping for every logical slot, ordinal, and origin; lowest ordinal governs shared finalization failure. |
| 70 | Every source occurrence retains its own diagnostic provenance. |

## M6 — Questions 71–86

| # | Answer |
|---:|---|
| 71 | Yes. |
| 72 | Movement preserves it. |
| 73 | Structural sharing preserves it. |
| 74 | Duplication copies it, subject to D20-B1 legality. |
| 75 | Not if the replacement can error; it must preserve the original origin. |
| 76 | Preserve category, origin, and precedence or reject the replacement. |
| 77 | Yes only if exact proof makes it total/error-free. |
| 78 | Yes; a total/error-free synthetic occurrence may be spanless. |
| 79 | Only when it implements one identifiable source occurrence and inherits that origin; otherwise no. |
| 80 | No mandatory provenance-set representation is needed. |
| 81 | No; non-executable metadata cannot raise a diagnostic. |
| 82 | §17.10.2 remains authoritative. |
| 83 | No original source-buffer retention is required solely for numeric spans. |
| 84 | Fully compatible with D18-L1/§18.14. |
| 85 | Compatible with D20-B1 demand/error preservation. |
| 86 | Compatible with D20-B2 child-order preservation. |

# Final assessments

## Implementer-invention test

After the proposed package, two conforming implementations cannot choose different correctness-observable behavior for:

- duplicate or nested slot identity;
- join output identity;
- duplicate relational rows;
- INNER/LEFT/CROSS multiplicity;
- unmatched LEFT rows;
- Limit selection cardinality/order contract;
- aggregate source-occurrence identity;
- rewritten source diagnostics;
- synthesized error diagnostics.

Result: **PASS — no correctness-relevant policy remains in M1–M6.**

## Implementation freedom

Preserved. The package does not require:

- ID width or numbering;
- allocator or pointer identity;
- node-class layout;
- map/memo representation;
- physical algorithm;
- aggregate sharing strategy;
- source-buffer retention;
- provenance container format.

## Persistence impact

**NONE.**

No file, tuple, catalog, WAL, recovery, page, RID, or transaction-ID format changes.

## Documentation-model assessment

All proposed rules are Architecture-owned “what” semantics. They contain no implementation sequencing, test procedure, current-state narration, benchmark method, or project history.

## Cross-owner conflict

**NONE.**

Notably:

- D20-M3 matches Chapter-17 3VL and Chapter-28 physical contracts.
- D20-M4 matches §§19.14, 27.9 and 35.20.
- D20-M5 directly consumes already-frozen §§19.10 and 29.3.7.
- D20-M6 composes with §§18.8, 18.14, 19.6–19.7, D20-B1/B2 and §39.

## Status

- D20-B1: **remains CLOSED**
- D20-B2: **remains CLOSED**
- F20-B1: **remains CLOSED**
- F20-B2: **remains CLOSED**
- F20-M1: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-M2: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-M3: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-M4: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-M5: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-M6: **SEMANTIC DECISION RECOMMENDED — AWAITING ARCHITECTURE-OWNER APPROVAL**
- F20-N1–N6: **OPEN / UNCHANGED**

Chapter 20:

- **BLOCKERS CLOSED**
- **MAJOR SEMANTIC DECISIONS AWAITING APPROVAL**
- **NOT SEMANTICALLY CLEAN**
- **NOT DOCUMENT-CLEAN**
- **NOT FULLY CLOSED**

Recommended next task after approval: **Chapter-20 major semantic integration — D20-M1 through D20-M6, Chapter 20 only.**

Chapter-20 verification synchronization: **NOT STARTED**
Chapter-21 direct review: **NOT STARTED**

No build, test, benchmark, implementation, staging, commit, or repository mutation occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
