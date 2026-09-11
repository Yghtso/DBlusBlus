# Chapter 30 architecture review

## 1. Verdict

**CLEAN WITH MINOR FIXES**

Chapter 30 has a coherent, implementable correctness contract. No blocking ambiguity, major ownership gap, frozen semantic question, or design-policy decision remains.

The frozen Chapters 17, 20, 22, 24, 26, 31, 37, and 38 resolve the highest-risk issues:

- ORDER BY equality classes are intentionally unstable.
- LIMIT/OFFSET selects occurrences from any sequence satisfying the resolved ordering.
- Consequently, full Sort + Limit, Top-N, and IndexScan + Limit may return different visible rows when the boundary cuts an equal-key class.
- External-sort Source I/O may fail after a client-visible prefix; the query then fails and the completed prefix is not retracted.
- Fan-in or memory conditions that cannot make progress terminate through Chapter 24’s controlled resource rules.

Two minor documentation defects remain:

1. implementation chronology and premature tuning/algorithm language in §§30.2 and 30.4;
2. §30.8(11)’s “including bag … tie … behavior” should state directly that equivalence means membership in the same permitted tie-outcome family, not identical tied occurrences across executions.

## 2. Finding counts

| Class | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 2 |
| EDITORIAL | 0 |
| DESIGN-SCOPE QUESTION | 0 |
| FROZEN SEMANTIC QUESTION | 0 |

## 3. Repository state

### Initial

```text
HEAD:
c4b54f4ce25c6a560d6154ef3e04020c6bb10cda

Commit:
c4b54f4 synced VERIFICATION after chapter 29 ARCHITECTURE fix

Tracked modifications:
NONE

Staged modifications:
NONE

Untracked paths:
NONE
```

### Final

```text
HEAD:
c4b54f4ce25c6a560d6154ef3e04020c6bb10cda

Tracked modifications:
NONE

Staged modifications:
NONE

Untracked paths:
NONE

git diff --check:
PASS — no output
```

Audit-created changes: **NONE**.

## 4. Exact live Chapter-30 boundary

```text
Chapter heading:
# 30. Sorting and Top-N

Start:
line 22568

End:
line 22784

Next chapter:
line 22785
# 31. DML, DDL, VACUUM, and Result Interface
```

### Complete subsection inventory

| Lines | Subsection | Primary ownership | Assessment |
|---:|---|---|---|
| 22570–22595 | 30.1 PhysicalSort | Blocking sort role, output comparator/property | Correct |
| 22596–22630 | 30.2 Sort storage and records | Sort retention and normalized prefix | Correct; chronology/tuning cleanup |
| 22631–22655 | 30.3 Sort comparator | Canonical physical comparator composition | Correct |
| 22656–22663 | 30.4 In-memory sort | Comparison-sort mechanism and payload movement | Correct mechanism; chronology cleanup |
| 22664–22689 | 30.5 External merge sort | Run generation and k-way merge | Correct |
| 22690–22708 | 30.6 Sort-run temporary format | Sort-specific spill-run metadata | Correct |
| 22709–22761 | 30.7 PhysicalTopN | Exact-K applicability and heap/output behavior | Correct |
| 22762–22781 | 30.8 Sorting invariants | Cross-algorithm invariants | Correct; tie summary should be clearer |
| 22783–22784 | Separator/blank boundary | Chapter termination | Correct |

## 5. Context inspected

Architecture:

- front matter and contract language;
- Chapter 8 user/physical index keys, RID tie order, and `IndexKeyCodec`;
- Chapter 17 scalar order, FLOAT64 order, signed zero, NaNs, VARCHAR collation;
- §§19.13–19.14 ORDER BY and LIMIT/OFFSET;
- §§20.11–20.12, 20.17, 20.17.10;
- §§22.1–22.8, especially §22.4.1;
- §§23.1, 23.6–23.13;
- Chapter 24 memory, retained rows, pressure, spill blocks, errors, cleanup;
- §§25.1, 25.1.1, 25.7–25.8;
- §§26.1–26.10;
- §27.9 PhysicalLimit;
- §29.10 comparator handoff only;
- §31.10 result publication;
- §§32.7–32.8;
- §§37.1–37.6;
- §§38.13, 38.15–38.16, 38.18–38.20, 38.24–38.25;
- §§39.1.3, 39.3;
- §41.5.

Other live documents:

- `DEVELOPMENT.md`: Phase E8 sorting sequence.
- `PROJECT_STATE.md`: searched for sorting/Top-N state leakage; none relevant.
- `VERIFICATION.md`: V20-10/11, V22-C/G/J/K/L, V23 retention, V24 spill, V25 errors, V26 lifecycle, Sort Tests, physical-property/enforcement tests, cancellation/resource procedures.

No historical review artifact was read.

## 6. Canonical owner matrix

| Concern | Canonical owner | Chapter-30 role |
|---|---|---|
| ORDER BY syntax and name resolution | §19.13 | Consumes resolved key expressions/slots |
| ORDER BY type admissibility | §§17.7.1, 19.13 | Does not admit new types |
| ASC/DESC and NULL defaults | §20.11 | Consumes stored resolved choices |
| Logical ordered bag | §20.11 | Physically realizes it |
| Equal-key instability | §§20.11, 30.1 | Implements permitted tie freedom |
| LIMIT/OFFSET SQL domain | §19.14 | No reinterpretation |
| Logical slicing/cardinality | §20.12 | Top-N/Limit must conform |
| Mathematical Top-N K | §30.7 | Direct owner |
| Top-N applicability | §§22.4.1, 30.7, 38.15, 38.24 | Algorithm precondition |
| Scalar comparator | Chapter 17 | Composes it per key |
| FLOAT64 total order | §17.4.3 | Uses it unchanged |
| VARCHAR collation | Chapter 17 | Uses binary collation |
| Sort-key evaluation/errors | Chapters 20 and 25 | Determines which keys must be retained |
| Bag preservation | §§20.11–20.12 | Must retain occurrences |
| Required slots | §37.4 | Retains required key/payload slots |
| OrderingProperty representation | §§37.2–37.5 | Produces runtime order |
| Memory accounting | Chapter 24 | Identifies sort-owned memory |
| Spill resources/blocks | §§24.7–24.10 | Adds run-specific payload descriptors |
| Merge progress | §24.6 | Selects legal sort progress actions |
| Pipeline readiness | Chapter 26 and §30.1 | Owns sort-specific blocking state |
| External publication | §31.10 | Supplies ready Source |
| Parallel sort | §32.7 | Chapter 30 supplies comparator/run semantics |
| Costing/selection | Chapter 38 | No local cost-policy ownership |
| Transaction/error effects | Chapter 39 | Propagates owned failures |
| Retry freshness | §§26.3.1, 39.1.4 | Sort state is attempt-local |

No duplicated semantic owner was found.

## 7. Physical operator inventory

| Operator/path | Shape | Memory/spill | Output property |
|---|---|---|---|
| `PhysicalSort` in-memory | Blocking Sink → Finalize → Source | Accounted retained rows/keys | Exact resolved key order |
| `PhysicalSort` external | Blocking run generation → merge-ready Source | SpillManager runs; multi-pass | Same exact comparator order |
| Parallel sort | Worker-local runs → final merge | Local/global accounted runs | Final merge order only |
| `PhysicalTopN` | Blocking for positive output demand | At most exact K retained records; no mandatory Top-N spill | Exact final key order |
| Exact existing-order provider + `PhysicalLimit` | Streaming where provider permits | Provider-owned | Provider order preserved |
| Full Sort + `PhysicalLimit` | Blocking sort then streaming limit | Sort-owned | Sort order preserved |

## 8. Logical handoffs

### LogicalSort

The handoff is complete:

- exact child bag and duplicate occurrences survive;
- child schema and `LogicalSlotId`s survive;
- only semantic ordering changes;
- hidden sort expressions may be represented by hidden slots;
- direction and NULL placement are already resolved;
- physical execution does not re-infer defaults;
- equal-key order is unspecified.

Bag preservation need not be repeated more extensively in Chapter 30 because §20.11 is canonical and Chapter 30 explicitly consumes that logical ordering.

### LogicalLimit

The handoff is complete:

- OFFSET is applied first;
- LIMIT is applied second;
- the logical result formula does not need `offset + limit`;
- Top-N’s `K=N+OFFSET` is an independent physical requirement;
- an unrepresentable K makes only that Top-N implementation ineligible;
- another exact provider plus `PhysicalLimit` remains available.

## 9. Comparator assessment

The comparator is complete and shared by all relevant algorithms:

```text
lexicographic key sequence
+ per-key direction
+ resolved NULL placement
+ Chapter-17 scalar order
+ binary VARCHAR collation
+ Chapter-17 FLOAT64 total order
```

It is deterministic and transitive at the semantic level. Comparator equality means equality on every resolved ORDER BY key—not payload equality and not GROUPING equivalence generally.

Forbidden substitutes include:

- host NaN comparison;
- locale-dependent VARCHAR ordering;
- pointer order;
- raw struct bytes;
- normalized-prefix equality as full-key equality;
- heap or RID order unless explicitly represented as an ORDER BY key.

### FLOAT64 order

For ascending scalar order:

```text
-Infinity
< finite negative values
< zero class (-0.0 and +0.0 compare equal)
< finite positive values
< +Infinity
< NaN class
```

All NaN encodings compare equal for ORDER BY. Descending reverses the non-NULL scalar order. NULL placement remains independently resolved per key.

Sorting preserves original projected payload bits. Comparator equality does not authorize rewriting `-0.0` to `+0.0` or canonicalizing a projected NaN payload.

### NULL ordering

```text
ASC default  -> NULLS FIRST
DESC default -> NULLS LAST
```

Explicit resolved choices are consumed as stored. Composite-key NULL treatment is component-local; later keys are consulted only after equality on all preceding components.

### Multi-key behavior

Mixed directions, NULL placements, and types compose lexicographically. OrderingProperty matching uses the same key sequence, slot identity, direction, NULL order, and collation.

## 10. Equal-key stability and Top-N ties

Full Sort may arbitrarily permute occurrences equal on every ORDER BY key. Physical stability is not a SQL guarantee.

The frozen logical interpretation is:

> LIMIT/OFFSET slices one legal occurrence sequence satisfying the resolved ORDER BY preorder.

Therefore, if a LIMIT/OFFSET boundary cuts through a comparator-equivalence class:

- any required number of occurrences from that boundary class may survive;
- distinct non-key payloads can make that choice SQL-visible;
- Sort, Top-N, and an index provider need not select the same tied occurrences;
- each selected sequence must remain key-monotonic and have the exact required cardinality;
- occurrences must not be fabricated or deduplicated.

Example:

```text
(A,key=5), (B,key=5), (C,key=5)
ORDER BY key LIMIT 1
```

Legal results include `A`, `B`, or `C`.

### Explicit answers

Can full Sort + Limit and Top-N legally return different visible tied rows?
**YES.**

Can IndexScan + Limit and Sort + Limit legally return different visible tied rows?
**YES.** The index’s physical RID tie order is not part of the SQL OrderingProperty.

Is the choice defined?
**YES.** It is constrained nondeterminism within the full comparator-equivalence class.

Does §30.8(11) express this ideally?
**No.** Read with §§20.11–20.12 it is correct, but “including bag … tie … behavior” should explicitly mean the same permitted outcome family, not identical tied occurrence selection. This is MINOR-2.

## 11. Normalized-prefix contract

The normalized prefix is a runtime acceleration aid:

- it must be order-preserving for every distinction it claims to decide;
- differing prefix bytes may decide order only when soundness is proven;
- equal prefixes require the complete comparator unless another exact equivalence proof exists;
- it is not a hash;
- an unsupported prefix optimization can shorten to zero bytes and fall back to full comparison;
- inability to optimize a key is not an SQL error.

### Prefix soundness by type

| Type/aspect | Required soundness |
|---|---|
| NULL marker | Must encode resolved FIRST/LAST placement |
| BOOLEAN | Not a v1 SQL ORDER BY type; any internal use requires its physical order |
| INT32 | Signed numeric order must map monotonically |
| INT64 | Signed numeric order must map monotonically |
| DATE | Chronological scalar order |
| TIMESTAMP | Chronological scalar order |
| FLOAT64 | Must encode Chapter-17 zero/NaN equivalence and total order |
| VARCHAR | Binary byte order with exact prefix-string behavior |
| ASC | Prefix direction follows ascending order |
| DESC | Transformation must reverse the admitted order soundly |
| Composite | Component boundaries and preceding-key equality must be preserved |
| Unsupported/insufficient encoding | Shorten or use zero prefix; full comparator decides |

### VARCHAR

Plain truncation cannot wrongly decide `"a"` versus `"aa"`. A sound design may encode an exact terminator/length distinction or leave the prefixes equal and invoke the full comparator. Embedded NUL and high-byte values are ordinary bytes under binary collation.

### FLOAT64

A normalized prefix must:

- place NaNs according to Chapter 17;
- map all ORDER-BY-equivalent NaNs to equal ordering prefixes where needed;
- treat `-0.0` and `+0.0` as comparator-equal;
- order infinities and finite values correctly;
- avoid host NaN predicates as the semantic oracle.

Payload bits remain unmodified.

### Index-codec relationship

Sort normalization is not declared to reuse the persistent `IndexKeyCodec`. Reuse is legal only after proving compatibility for:

- direction;
- NULL placement;
- collation;
- FLOAT64 order;
- truncation;
- component boundaries.

A runtime sort prefix does not become part of the persistent index format.

### Prefix size

The mechanism is **JUSTIFIED ADVANCED**: it exposes a real modern sorting optimization with useful cache/comparison behavior.

The fixed “initial normalized prefix target is 8–16 bytes” is not a correctness rule and is better treated as configurable tuning or Development guidance. This is included in MINOR-1; it does not require a design-policy decision.

## 12. Sort-key evaluation and demanded errors

Chapter 20 determines which logical rows demand each sort expression. Chapter 25 owns evaluation and deterministic error selection.

For a demanded blocking sort:

- all demanded sort keys must be evaluated before sorted Source readiness;
- normalized prefixes cannot replace required semantic evaluation;
- Top-N heap fullness cannot suppress a later demanded key;
- speculative undemanded evaluations cannot create public errors;
- worker, chunk, run, and spill discovery order do not choose the error;
- hidden keys retain source provenance and slot identity.

Alias/ordinal references consume existing output slots and do not reevaluate the source expression.

No sort-specific error ordinal is needed.

## 13. Payload and lifetime

The sort must retain:

- every slot required for comparison;
- every slot required for output/downstream evaluation;
- occurrence cardinality even with zero visible payload columns.

Chapter 37 owns RequiredSlotSet derivation. Chapter 30 owns retaining what that set requires.

All retained VARCHAR key and payload bytes must survive source chunk reuse, source page unpin, spill, merge, and output. Borrowed `StringRef`s cannot be retained beyond their owner.

The compact record model remains implementation-flexible:

```text
normalized prefix + handle
```

is conceptual. Inline keys, indirect keys, row collections, segmented oversized rows, or equivalent exact records remain legal.

The “expensive payload is not repeatedly moved” rule is performance guidance consistent with the project’s goals, not an observable SQL contract.

## 14. Sort lifecycles

### PhysicalSort

```text
Sink:
    consume demanded occurrences
    evaluate sort keys
    retain required key/payload state
    produce in-memory or spilled runs

Finalize:
    finish the final run
    establish valid merge/output state
    publish successful dependency readiness

Source:
    emit comparator-ordered chunks
```

No row is available before successful required build/finalization.

### In-memory sort

- zero input produces zero rows;
- one row retains its exact payload;
- duplicate occurrences remain duplicate;
- any high-quality comparator-based algorithm is legal;
- equal-key instability is permitted.

### External run generation

```text
reserve/retain rows
→ sort complete run
→ write sequential run
→ release/reset run memory
```

If one exact row does not fit the ordinary representation, Chapter 24 requires another exact supported representation or controlled representability/OOM failure. Truncation is forbidden.

### External merge

- every run uses the same comparator;
- every logical occurrence appears exactly once;
- bounded fan-in is selected from memory and run count;
- multiple passes are allowed;
- each successful pressure/pass action must make well-founded progress;
- inability to support fan-in of at least two when multiple runs remain ends in controlled OOM or the applicable SpillIOError—it cannot loop at fan-in one;
- tied run heads may be selected in any legal equal-key order;
- non-tied order cannot change.

## 15. Memory accounting inventory

Chapter 24 covers all potentially unbounded sort state:

- sort records;
- normalized prefixes;
- retained complete keys;
- output payload rows;
- VARCHAR backing;
- RowCollection blocks;
- oversized-row representations;
- run directories and metadata;
- serialization buffers;
- read buffers;
- write buffers;
- merge heap/tournament state;
- reloaded blocks;
- output buffers;
- worker-local runs;
- Top-N heap records and varlen backing.

No unowned large allocation was found.

## 16. Temporary run format

Sort runs are query/attempt-temporary SpillManager objects. They are neither WAL-logged nor crash-recovered and have no long-lived compatibility promise.

Conceptual metadata is appropriate:

- run identity/version;
- row-layout/schema mismatch detector;
- sort-key descriptor mismatch detector;
- record count;
- checksummed blocks;
- sorted payload.

Fingerprints are mismatch detectors, not collision-free semantic identity. Query/run ownership and owner-specific structural validation remain authoritative. A fingerprint collision alone cannot establish compatibility.

Checksum success does not replace validation of:

- framing;
- versions;
- extents;
- counts;
- offsets;
- record boundaries;
- file ranges;
- owner/run association;
- sort descriptor applicability.

### Error classification

| Condition | Result |
|---|---|
| Short read/write, ENOSPC | `SpillIOError` |
| Bad framing/version/checksum | `SpillIOError` |
| Malformed count/length/offset/range | `SpillIOError` |
| Run owner/descriptor structural mismatch in decoded spill | `SpillIOError` |
| Unsupported spill addressability | `SpillIOError` |
| Self-generated in-memory construction invariant violation | Internal invalid state |
| Allocation denial for supported exact form | `OutOfMemory` |
| No supported exact runtime representation | Controlled representability `ExecutionError` |

The checksummed/versioned run mechanism is **JUSTIFIED ADVANCED**. It largely reuses Chapter 24 and teaches robust external execution without imposing durable-format compatibility.

## 17. Post-readiness Source failure

External merge need not pre-read or prevalidate every future run block before Source readiness.

A later run read/checksum failure may therefore occur after earlier sorted chunks have been returned. The result is:

```text
query completion = failure
error = SpillIOError
already returned prefix = not retracted
prefix ≠ successful complete query
```

This is clearly owned by §§26.3.2, 31.10, and 39.3. Chapter 29’s stronger no-prefix-before-all-group-validation rule does not apply to Sort.

No semantic question exists here.

## 18. Cancellation, cleanup, retry, and parallel sort

Cancellation may terminate key evaluation, sorting, run writing, merge passes, final merge Source, Top-N build, or retained-output sort. Tasks must quiesce and query-owned state must clean up.

A retry receives fresh:

- in-memory rows;
- prefixes;
- run identity/namespace/files;
- merge cursors;
- heap state;
- readiness state;
- Source cursor.

No spill data survives into another attempt.

Chapter 32 permits worker-local run generation followed by an order-preserving merge. Arbitrary worker interleaving cannot become final ordered output.

## 19. PhysicalTopN assessment

### Blocking role

For positive demanded output, Top-N is blocking: any later demanded row may improve the retained set. The live phrase “After input completion: sort … emit” establishes this, together with Chapter 26. A direct `Sink → Finalize → Source` sentence would be clearer but is not required to resolve behavior.

Heap fullness never authorizes early input stop.

### Exact K

```text
K = exact mathematical N + OFFSET
```

K may exceed INT64. It is not a public SQL value.

| Condition | Required result |
|---|---|
| Exact K supported | Top-N may be eligible |
| Exact K unsupported | Top-N ineligible |
| Wider exact internal domain available | May retain eligibility |
| Saturated/clamped K | Forbidden without independent exact proof |
| Top-N ineligible | Exact provider + PhysicalLimit remains |
| Bad estimate/cost | May choose slow plan, never alter semantics |
| Runtime allocation denial | Controlled OOM |
| Valid SQL with unrepresentable Top-N K | No public overflow |

### `LIMIT 0 OFFSET M`

This is coherent:

- logical result is empty because LIMIT is zero;
- mathematical Top-N K remains `M`;
- §20.17.10 can prove the LogicalLimit empty;
- demand-safe nonexecution may avoid the child entirely;
- `K=0` in §30.7 is only the narrower `N=0, OFFSET=0` fast case.

No contradiction exists, though §30.7 could mention the broader LIMIT-zero owner when undergoing other cleanup.

### Heap and output

For K > 0:

- at most K occurrence records are retained;
- the root is the worst retained candidate;
- an incoming better row replaces it;
- a worse row may be discarded;
- an equal-key row may be retained or discarded consistently with tie freedom;
- equal candidates are not deduplicated;
- retained records are sorted before OFFSET/LIMIT;
- heap-array order never escapes.

PhysicalTopN is **CORE**: it is a compact, recognizable, high-value physical optimization. Exact-K applicability is justified correctness machinery rather than overengineering.

## 20. Algorithm substitutability

### Full Sort + Limit versus Top-N

| Observable | Must agree? |
|---|---:|
| Output cardinality | Yes |
| Non-tied membership | Yes |
| Key-monotonic ordering | Yes |
| NULL ordering | Yes |
| Scalar comparator | Yes |
| Duplicate multiplicity before boundary | Yes |
| Schema/slots | Yes |
| Demanded semantic errors | Yes |
| Transaction result class | Yes |
| Resource success/failure occurrence | No; resource paths may differ |
| Boundary-tie member identity | No |
| Relative order among complete-key ties | No |

### Index provider versus Top-N/Sort

An index provider may carry physical RID order among equal user keys, but that tie order is not part of the SQL OrderingProperty. It may therefore select different tied occurrences under LIMIT.

It must still agree on:

- comparator key monotonicity;
- NULL/collation direction;
- exact cardinality;
- non-tied selection;
- demand/error semantics;
- output schema and slots.

### Demanded-error equivalence

An order provider may stop after the required prefix only when the frozen logical demand contract makes the remainder unnecessary. It cannot suppress an expression that remains demanded merely because its physical access path reached K rows.

## 21. OrderingProperty and computed keys

`OrderingProperty` describes key monotonicity, not stability.

Exact satisfaction requires matching:

- `LogicalSlotId`;
- direction;
- NULL order;
- collation;
- key-vector prefix.

For computed ORDER BY expressions, a hidden slot carries the resolved value. Property matching compares slot identity and descriptors, not expression text or pointer identity.

A provided `(a,b,c)` ordering may satisfy `(a,b)` only when all matching descriptors agree. Spill/remerge must preserve every advertised non-tied ordering relation.

Hidden sort keys cannot be pruned while still required.

## 22. Boundary cases

| Case | Result |
|---|---|
| Empty Sort input | Zero rows; vacuously ordered |
| Empty Top-N input | Zero rows |
| One row | Same occurrence/payload |
| One run | May avoid spill/merge |
| Duplicate equal rows | Every occurrence retained by full Sort |
| Top-N boundary duplicates | Exact number of occurrences retained; no dedup |
| Zero visible payload columns | Cardinality survives through hidden keys/row occurrences |
| Huge row | Exact alternate representation or controlled error; never truncation |
| Comparator-equal `±0` keys | Original projected bits preserved |
| Comparator-equal NaNs | Original projected payload bits preserved |
| Spill/remerge | May change tie order, never non-tied order |

## 23. Temporal and document-role audit

Actual Chapter-30 chronology:

| Line | Wording | Classification |
|---:|---|---|
| 22609 | “The initial normalized prefix target…” | Development/tuning chronology |
| 22658 | “The initial implementation uses…” | Implementation sequencing |
| 22662 | “Custom radix sorting is a future optimization…” | Future implementation chronology |
| 22666 | “current in-memory run” | Legitimate runtime state |
| 22676 | “After input finalization” | Legitimate runtime lifecycle |

Chronology count: **3**.

`PROJECT_STATE.md` contains no Chapter-30 implementation-status leakage relevant to this review. `DEVELOPMENT.md` correctly owns Phase E8 sequencing.

## 24. Complexity classifications

| Mechanism | Classification | Assessment |
|---|---|---|
| Comparator-based in-memory sort | CORE | Fundamental execution mechanism |
| Compact records/handles | CORE | Makes comparison sorting practical |
| Normalized prefixes | JUSTIFIED ADVANCED | Strong educational/performance value |
| Fixed 8–16-byte target | POSSIBLE OVERENGINEERING / document-role issue | Tuning detail, not semantic architecture |
| External multi-pass merge | CORE | Necessary under bounded query memory |
| Buffered spill I/O | CORE | Essential external-sort mechanism |
| Checksummed/versioned runs | JUSTIFIED ADVANCED | Robust temporary-I/O handling |
| Descriptor fingerprints | JUSTIFIED ADVANCED as mismatch detectors | Must not become semantic identity |
| Top-N heap | CORE | High-value standard relational optimization |
| Exact-K domain handling | JUSTIFIED ADVANCED | Prevents real cardinality bugs |
| Named introsort-style algorithm | Implementation guidance | Optional example, not semantic requirement |
| Payload indirection | JUSTIFIED | Avoids expensive repeated movement |

No formal D30-* decision is necessary.

## 25. Existing Verification coverage

### Reusable and substantially complete

- V17 scalar/FLOAT/VARCHAR comparator oracles;
- V19 ORDER BY and LIMIT binding;
- V20-10 Sort bag/order/tie classes;
- V20-11 logical LIMIT/OFFSET;
- V22-C/G operator substitutability;
- V22-J exact-K domain and saturation negatives;
- V22-K physical-plan validation;
- V23 borrowing/StringRef lifetime;
- V24 accounting, spill framing, extent validation, progress, cleanup;
- V25 demanded-error selection;
- V26 blocking/readiness, early stop, cancellation;
- Chapter-37 property/enforcement tests;
- optimizer cost, enforcement, and exact-K fallback tests.

### Partial or missing Chapter-30-specific coverage

- full normalized-prefix soundness matrix by type;
- VARCHAR prefix-string counterexamples;
- FLOAT zero/NaN prefix equivalence;
- zero-length prefix fallback;
- exact tied-boundary outcome-family oracle for Sort/Top-N/index providers;
- explicit Top-N blocking barriers;
- Top-N duplicate-occurrence retention;
- full run-generation/merge occurrence ledger;
- fan-in-below-two controlled termination;
- multi-pass progress metric;
- run comparator mismatch injection;
- fingerprint-is-not-identity test;
- checksum-success-with-malformed-structure tests specific to sort runs;
- huge-single-row sort retention;
- post-readiness spill-read failure after returned prefix;
- sort retry poisoning/freshness;
- complete sort-owned memory ledger;
- parallel worker-run merge equivalence;
- Top-N OOM and lack of unowned runtime replanning;
- zero-visible-payload/hidden-sort-slot fixtures.

Verification synchronization is not blocked by a semantic question.

## 26. Previous-chapter regression

| Frozen owner | Contradiction? | Result |
|---|---:|---|
| Chapter 17 scalar ordering | No | Comparator delegates correctly |
| Chapter 19 LIMIT/OFFSET | No | Validated INT64 inputs retained |
| Chapter 20 Sort/Limit semantics | No | Bags, instability, slicing preserved |
| Chapter 22 exact-K applicability | No | Same exact domain/fallback |
| Chapter 23 occurrences/lifetimes | No | Retention deep-copies varlen |
| Chapter 24 memory/spill | No | Accounting, progress, errors delegated |
| Chapter 25 errors | No | No physical discovery precedence |
| Chapter 26 blocking/readiness | No | Sort/Top-N compose correctly |
| Chapter 29 ordered comparator use | No | Same comparator/order property |
| Chapter 31 publication boundary | No | Prefix-after-ready failure coherent |
| Chapter 37 ordering properties | No | No duplicate property system |
| Chapter 38 costing/validation | No | Correct separation of applicability/cost |

Previous-chapter regression result: **NONE**.

## 27. Technical-consistency matrix

Legend:

- **YES** — precise and consistent.
- **OWNER** — precise through named canonical owner.
- **MINOR-1/2** — mapped finding.
- **N/A** — outside the relevant v1 capability.

### A. Logical contract and comparator

| # | Check | Result |
|---:|---|---|
| 1 | Sort preserves every child occurrence | OWNER—§20.11 |
| 2 | Sort creates no occurrence | OWNER—§20.11 |
| 3 | Sort deduplicates nothing | OWNER—§20.11 |
| 4 | Child schema survives | OWNER—§20.11 |
| 5 | Child slots survive | OWNER—§20.11 |
| 6 | Hidden key slots are supported | OWNER—§37.2 |
| 7 | Direction is resolved upstream | OWNER—§20.11 |
| 8 | NULL order is resolved upstream | OWNER—§20.11 |
| 9 | Physical execution does not infer defaults | YES |
| 10 | ASC default is NULLS FIRST | OWNER—§20.11 |
| 11 | DESC default is NULLS LAST | OWNER—§20.11 |
| 12 | Key comparison is lexicographic | YES |
| 13 | Later key follows preceding equality | YES |
| 14 | Mixed directions are supported | YES |
| 15 | Mixed NULL placement is supported | YES |
| 16 | Scalar order comes from Chapter 17 | OWNER |
| 17 | VARCHAR collation is binary | OWNER—Ch17 |
| 18 | Locale collation is excluded | YES |
| 19 | Raw pointers cannot order values | YES |
| 20 | Raw struct bytes cannot substitute generally | YES |

### B. FLOAT64, NULL, and ties

| # | Check | Result |
|---:|---|---|
| 21 | `-Inf` precedes finite values ascending | OWNER—§17.4.3 |
| 22 | `+Inf` follows finite values | OWNER—§17.4.3 |
| 23 | NaNs follow `+Inf` ascending | OWNER—§17.4.3 |
| 24 | All ORDER-BY NaNs compare equal | OWNER—§17.4.3 |
| 25 | Signed zeros compare equal | OWNER—§17.4.3 |
| 26 | Host NaN comparison is insufficient | YES |
| 27 | DESC reverses scalar ordering | YES |
| 28 | NULL placement is independent of scalar order | YES |
| 29 | Equal-key input order need not survive | YES |
| 30 | Full Sort may swap tied rows | YES |
| 31 | Non-equal rows cannot swap across order | YES |
| 32 | Stability is not an OrderingProperty | YES |
| 33 | Stable implementation remains legal | YES |
| 34 | Stability cannot be required downstream | YES |
| 35 | Projected `-0` bits survive sorting | YES |
| 36 | Projected NaN payload bits survive sorting | YES |
| 37 | Sorting alone performs no canonicalization | YES |
| 38 | Tie order may change during run merge | YES |
| 39 | Tie order may change with workers | YES |
| 40 | Non-tied order may not change with workers | YES |

### C. LIMIT and tie-boundary semantics

| # | Check | Result |
|---:|---|---|
| 41 | OFFSET is applied before LIMIT | OWNER—§20.12 |
| 42 | Logical cardinality avoids `o+l` | OWNER—§20.12 |
| 43 | Ordered Limit slices a legal sequence | OWNER—§20.12 |
| 44 | Boundary ties may select different members | YES |
| 45 | Sort+Limit and Top-N may differ on tied payload | YES |
| 46 | Index+Limit and Sort+Limit may differ on ties | YES |
| 47 | Exact output count remains invariant | YES |
| 48 | Non-tied membership remains invariant | YES |
| 49 | Tie selection cannot fabricate rows | YES |
| 50 | Tie selection cannot deduplicate multiplicity | YES |
| 51 | `±0` boundary rows may select either occurrence | YES |
| 52 | NaN boundary rows may select any tied occurrence | YES |
| 53 | Payload makes tie selection observable | YES |
| 54 | Physical RID tie order is not SQL order | OWNER—Ch8/37 |
| 55 | §30.8 outcome-family meaning is derivable | YES |
| 56 | §30.8 wording is locally explicit | MINOR-2 |
| 57 | Hidden deterministic tie-breaker exists | NO—intentionally absent |
| 58 | Exact tied bag across executions is required | NO |
| 59 | Tie rule is algorithm-independent | YES |
| 60 | Tie instability extends only to full-key ties | YES |

### D. Normalized prefixes

| # | Check | Result |
|---:|---|---|
| 61 | Prefix is an ordering aid | YES |
| 62 | Prefix is not a hash | YES |
| 63 | Prefix difference may decide only soundly | YES |
| 64 | Prefix equality invokes full comparator | YES |
| 65 | Zero-length fallback is permitted | YES |
| 66 | Unsupported optimization is not SQL error | YES |
| 67 | ASC transform must preserve direction | YES |
| 68 | DESC transform must reverse soundly | YES |
| 69 | NULL marker must follow resolved placement | YES |
| 70 | INT32 prefix may be monotonic | YES |
| 71 | INT64 prefix may be monotonic | YES |
| 72 | DATE prefix may be monotonic | YES |
| 73 | TIMESTAMP prefix may be monotonic | YES |
| 74 | FLOAT prefix must normalize comparison classes | YES |
| 75 | VARCHAR prefix must preserve binary order | YES |
| 76 | Embedded NUL remains ordinary byte | YES |
| 77 | `"a"`/`"aa"` cannot be misordered by truncation | YES |
| 78 | Long common prefix falls back safely | YES |
| 79 | Composite boundaries must remain sound | YES |
| 80 | Prefix collision implies semantic equality | NO |

### E. Prefix ownership and in-memory sort

| # | Check | Result |
|---:|---|---|
| 81 | Sort prefix is independent of persistent codec | YES |
| 82 | Codec reuse requires semantic proof | YES |
| 83 | Runtime prefix creates no persistent ABI | YES |
| 84 | 8–16 bytes is correctness-required | NO |
| 85 | Prefix mechanism is proportionate | YES |
| 86 | Fixed target belongs as tuning guidance | MINOR-1 |
| 87 | In-memory sort must be comparison-correct | YES |
| 88 | Introsort specifically is mandatory | NO |
| 89 | Standard-library implementation is permitted | YES |
| 90 | Radix sort is semantically forbidden | NO |
| 91 | Radix sort requires comparator equivalence | YES |
| 92 | “initial implementation” is timeless | MINOR-1 |
| 93 | “future optimization” is timeless | MINOR-1 |
| 94 | Compact handles are permitted | YES |
| 95 | Inline exact records are permitted | YES |
| 96 | Payload movement guidance affects SQL semantics | NO |
| 97 | Expensive repeated payload movement is discouraged | YES |
| 98 | Sort algorithm choice may alter tie order | YES |
| 99 | Algorithm choice may alter bag | NO |
| 100 | Algorithm choice may alter non-tied order | NO |

### F. Expression, slots, payload, and lifetime

| # | Check | Result |
|---:|---|---|
| 101 | Every demanded row’s key is evaluated | OWNER—Ch20/25 |
| 102 | Top-N threshold may skip demanded key evaluation | NO |
| 103 | Prefix creation may suppress a demanded error | NO |
| 104 | Hidden key expression retains provenance | OWNER—Ch20/25 |
| 105 | Alias reuse avoids reevaluation | OWNER—§19.13 |
| 106 | Ordinal reuse avoids reevaluation | OWNER—§19.13 |
| 107 | Error winner ignores chunk order | OWNER—§25.1.1 |
| 108 | Error winner ignores worker order | OWNER—§25.1.1 |
| 109 | Sort emits before all demanded keys succeed | NO |
| 110 | Required key slots may be pruned | NO |
| 111 | Required output slots may be pruned | NO |
| 112 | Unrelated payload may be pruned | YES |
| 113 | Zero visible payload preserves occurrences | YES |
| 114 | Retained VARCHAR may borrow source chunk | NO |
| 115 | Retained VARCHAR survives unpin | YES |
| 116 | Retained VARCHAR survives run spill | YES |
| 117 | Prefix lifetime follows sort state | YES |
| 118 | Payload lifetime follows retained owner | YES |
| 119 | Huge VARCHAR may be truncated | NO |
| 120 | Huge exact row may use alternate representation | OWNER—§24.2 |

### G. PhysicalSort and external merge

| # | Check | Result |
|---:|---|---|
| 121 | PhysicalSort is explicitly blocking | YES |
| 122 | Sink consumes demanded input | YES |
| 123 | Finalize establishes run/merge readiness | OWNER—Ch26 |
| 124 | Source waits for readiness | OWNER—Ch26 |
| 125 | Empty input emits zero rows | YES |
| 126 | One row remains unchanged | YES |
| 127 | One run need not spill | YES |
| 128 | Run generation sorts before write | YES |
| 129 | Run release follows successful retention/write | YES |
| 130 | Final merge uses same comparator | YES |
| 131 | Merge loses no occurrence | OWNER—§20.11 |
| 132 | Merge duplicates no occurrence | OWNER—§20.11 |
| 133 | Merge may be multi-pass | YES |
| 134 | Merge buffers are accounted | OWNER—Ch24 |
| 135 | Run metadata is accounted | OWNER—Ch24 |
| 136 | Fan-in is memory-bounded | YES |
| 137 | Fan-in one may loop indefinitely | NO |
| 138 | No feasible progress terminates controlled | OWNER—§24.6 |
| 139 | Successful pass must advance pressure state | OWNER—§24.6 |
| 140 | Fixed retry count is required | NO |

### H. Runs, validation, errors, publication

| # | Check | Result |
|---:|---|---|
| 141 | Runs are query-temporary | YES |
| 142 | Runs are WAL-logged | NO |
| 143 | Runs are crash-recovered | NO |
| 144 | Run format is long-lived ABI | NO |
| 145 | Run identity is validated | YES |
| 146 | Format version is validated | YES |
| 147 | Record count is checked before use | OWNER—§24.8 |
| 148 | Lengths/extents are checked | OWNER—§24.8 |
| 149 | Offsets/ranges are checked | OWNER—§24.8 |
| 150 | CRC alone establishes validity | NO |
| 151 | Fingerprint alone establishes semantic identity | NO |
| 152 | Fingerprint collision may authorize mismatch | NO |
| 153 | Malformed decoded run is SpillIOError | OWNER—§39.3 |
| 154 | In-memory invariant defect is SpillIOError | NO |
| 155 | Short read is SpillIOError | YES |
| 156 | ENOSPC is SpillIOError | YES |
| 157 | External Source may perform later reads | YES |
| 158 | Later read may fail after returned prefix | YES |
| 159 | Returned prefix is retracted | NO |
| 160 | Prefix means complete-query success | NO |

### I. Top-N and exact K

| # | Check | Result |
|---:|---|---|
| 161 | K is mathematical `N+OFFSET` | YES |
| 162 | K may exceed INT64 | YES |
| 163 | K is a public SQL value | NO |
| 164 | Unrepresentable K invalidates SQL | NO |
| 165 | Unsupported K makes Top-N ineligible | YES |
| 166 | Wider exact K domain is permitted | YES |
| 167 | Saturated retained K is generally legal | NO |
| 168 | Clamped retained K is generally legal | NO |
| 169 | Exact proof may justify a reduced bound | YES |
| 170 | V1 global cardinality bound supplies that proof | NO |
| 171 | Positive-output Top-N is blocking | YES |
| 172 | Heap reaching K authorizes input stop | NO |
| 173 | Heap retains at most K occurrences | YES |
| 174 | Heap may deduplicate equal rows | NO |
| 175 | Heap root is worst retained record | YES |
| 176 | Better candidate may replace root | YES |
| 177 | Equal candidate follows tie freedom | YES |
| 178 | Heap order may be emitted directly | NO |
| 179 | Retained K must be sorted before OFFSET | YES |
| 180 | Large K may favor full sort by cost | OWNER—Ch38 |

### J. Top-N boundaries, properties, resources

| # | Check | Result |
|---:|---|---|
| 181 | LIMIT zero returns empty | OWNER—§20.12 |
| 182 | LIMIT zero may prove child unnecessary | OWNER—§20.17.10 |
| 183 | `LIMIT 0 OFFSET M` has mathematical K=M | YES |
| 184 | That K forces child execution | NO |
| 185 | Input fewer than OFFSET yields empty | YES |
| 186 | Input exactly OFFSET yields empty | YES |
| 187 | K above input retains all input | YES |
| 188 | OFFSET subtraction underflow is allowed | NO |
| 189 | Top-N OOM is controlled | OWNER—Ch24/39 |
| 190 | Top-N must itself spill | NO |
| 191 | Runtime OOM authorizes arbitrary replan | NO |
| 192 | Planner misestimate changes SQL result | NO |
| 193 | PhysicalSort advertises exact key order | OWNER—§37.5 |
| 194 | PhysicalTopN advertises final key order | OWNER—§37.5 |
| 195 | OrderingProperty implies stability | NO |
| 196 | Longer exact ordering satisfies prefix | OWNER—§37.3 |
| 197 | Direction mismatch satisfies property | NO |
| 198 | NULL-order mismatch satisfies property | NO |
| 199 | Collation mismatch satisfies property | NO |
| 200 | Slot-identity mismatch satisfies property | NO |

### K. Cancellation, retry, parallelism, document role

| # | Check | Result |
|---:|---|---|
| 201 | Cancellation during Sink is controlled | OWNER—Ch26/39 |
| 202 | Cancellation during run write cleans resources | OWNER—Ch24/26 |
| 203 | Cancellation during merge quiesces tasks | OWNER—Ch26/32 |
| 204 | Cancellation permits successful readiness afterward | NO |
| 205 | Retry reuses failed run files | NO |
| 206 | Retry reuses failed heap state | NO |
| 207 | Retry reuses Source cursor | NO |
| 208 | Immutable comparator descriptor may be reused safely | YES |
| 209 | Parallel workers may generate local runs | OWNER—§32.7 |
| 210 | Workers may directly interleave final ordered rows | NO |
| 211 | Final parallel merge preserves comparator order | YES |
| 212 | Worker count may change tie order | YES |
| 213 | Worker count may change non-tied order | NO |
| 214 | Chapter 30 duplicates OrderingProperty representation | NO |
| 215 | Chapter 30 owns optimizer costing | NO |
| 216 | Chapter 38 owns plan selection | YES |
| 217 | Current implementation state appears in Chapter 30 | MINOR-1 |
| 218 | Verification procedures leak into Chapter 30 | NO |
| 219 | Persistent-format commitments leak from sort runs | NO |
| 220 | Chapter 30 requires specialist unnecessary machinery | NO |

Technical checks completed: **220**.

## 28. Findings

### BLOCKING

**NONE.**

### MAJOR

**NONE.**

### MINOR-1 — Implementation chronology and tuning detail

Sections and text:

- §30.2, line 22609: “The initial normalized prefix target is 8–16 bytes.”
- §30.4, line 22658: “The initial implementation uses…”
- §30.4, line 22662: “Custom radix sorting is a future optimization…”

Why this is a defect:

- “initial” and “future” narrate implementation sequence;
- the named algorithm and exact prefix target are not correctness requirements;
- Architecture should state timeless applicability and semantic constraints.

Smallest repair:

- retain normalized-prefix soundness;
- describe prefix width as configurable/tunable, or omit the target;
- permit a comparator-equivalent comparison/radix strategy without implementation chronology;
- leave actual sequencing in Development and availability in Project State.

### MINOR-2 — Top-N tie equivalence summary is locally imprecise

Section:

- §30.8(11), lines 22776–22778.

Live wording:

> “An eligible Top-N is semantically equivalent to an exact ordering provider followed by PhysicalLimit, including bag, semantic order, comparator, tie, demanded-error, and transaction behavior.”

Why this merits clarification:

Read with §§20.11–20.12, the rule is resolvable: equivalence means the same allowed result family, with arbitrary occurrence choice where LIMIT cuts a complete-key tie class. Standing alone, “including bag” can be misread as requiring identical tied occurrence bags between separate valid physical executions.

Smallest repair:

State locally that:

```text
equivalence is membership in the same Chapter-20 permitted ordered-result
family; providers need not select the same visible occurrence when the
LIMIT/OFFSET boundary cuts an equal-key class.
```

No hidden tiebreaker or new policy is needed.

### EDITORIAL

**NONE.**

### DESIGN-SCOPE QUESTIONS

**NONE.**

The complexity classifications above can be settled by direct cleanup; no owner decision is required.

### Frozen N30-* semantic questions

**NONE.**

## 29. Recommended fixing sequence

### STEP A — low-risk chronology/document-role cleanup

- Replace the three temporal phrases in §§30.2 and 30.4.
- Preserve normalized prefixes as a justified mechanism.
- Remove or make configurable the 8–16-byte implementation target.
- Express comparison/radix choices as timeless comparator-equivalent options.

### STEP B — local execution-contract clarification

- Clarify §30.8(11)’s permitted tie-outcome family.
- Optionally make the already-derivable Top-N blocking lifecycle explicit:
  `Sink → successful Finalize → Source`.

### STEP C — ownership/resource/spill fixes

No required repair.

### STEP D — frozen semantic decisions

None.

### STEP E — design-scope decisions

None.

## 30. Direct final-review answers

- Does full Sort preserve the exact child bag? **Yes.**
- Can full Sort change relative order inside an equal-key class? **Yes.**
- Does physical stability have a SQL-visible guarantee? **No.**
- Can Top-N choose arbitrary members of a boundary tie class? **Yes, with exact required multiplicity.**
- Is that choice currently defined? **Yes, through §§20.11–20.12; §30.8 should restate it more clearly.**
- Does PhysicalTopN have complete semantic equivalence with Sort+Limit? **Yes, as an allowed-result-family equivalence.**
- Can an ordered index provider choose different boundary ties? **Yes.**
- Is the tie rule algorithm-independent? **Yes.**
- Can prefix bytes substitute unsafely for full semantics? **No.**
- Can a truncated VARCHAR prefix misorder `"a"` and `"aa"`? **No; it must encode the distinction soundly or fall back.**
- Can sort normalization reuse persistent index encoding without proof? **No.**
- Is 8–16-byte sizing Architecture-appropriate? **Not as “initial” fixed guidance; it is tuning.**
- Can sort retain borrowed VARCHAR bytes? **No.**
- Can a huge row be truncated under pressure? **No.**
- Can external merge fail to make progress indefinitely? **No.**
- Can merge fan-in remain one with multiple runs? **No; use another exact action or fail controllably.**
- Are fingerprints semantic identity? **No, mismatch detection only.**
- Can checksum success replace structural validation? **No.**
- Can spill-read failure occur after prefix publication? **Yes.**
- Is that behavior clearly owned? **Yes, by Chapters 26, 31, and 39.**
- Is Top-N blocking for K>0? **Yes.**
- Can heap fullness authorize early stop? **No.**
- Can unrepresentable K become public overflow? **No.**
- Can K be clamped? **No, absent an exact proof.**
- Is `LIMIT 0 OFFSET M` coherent? **Yes.**
- Can Top-N emit heap order? **No.**
- Can Top-N skip demanded key errors? **No.**
- Does Top-N retain required duplicate occurrences? **Yes.**
- Does spill change non-tied ordering? **No.**
- Can spill change tied order legally? **Yes.**
- Does OrderingProperty imply stability? **No.**
- Can RequiredSlotSet prune a required hidden key? **No.**
- Is external merge sort proportionate for v1? **Yes—CORE.**
- Is normalized-prefix acceleration proportionate? **Yes—JUSTIFIED ADVANCED.**
- Are checksummed/versioned runs proportionate? **Yes—JUSTIFIED ADVANCED.**
- Does Chapter 30 contain implementation chronology? **Yes, three phrases.**
- Does Chapter 30 contain correctness-relevant invention points? **No.**
- Can Chapter 30 stand unchanged as ideal canonical v1 Architecture? **Semantically yes; document-quality closure needs the two minor repairs.**

## 31. Exact next action

Proceed with a small documentation-only:

```text
CHAPTER-30 ARCHITECTURE FIX A —
TIMELESS SORT GUIDANCE AND TIE-OUTCOME CLARIFICATION
```

No separate frozen semantic/design decision package is required. Verification synchronization should wait until those minor Architecture repairs are complete.

```text
CHAPTER 31 REVIEW:
    NOT STARTED

VERIFICATION MODIFICATION:
    NONE

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE
```