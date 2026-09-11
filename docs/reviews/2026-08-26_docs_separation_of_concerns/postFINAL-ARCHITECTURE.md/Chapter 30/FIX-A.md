## Chapter 30 Fix A report

1. Initial repository state

```text
HEAD: 377cabf82b96a1517d9028abedcce44a2a9b63b5
Commit: 377cabf chapter 30 ARCHITECTURE analysis
Working tree: clean
Index: clean
```

2. Final repository state

```text
HEAD: 377cabf82b96a1517d9028abedcce44a2a9b63b5
Working tree: docs/ARCHITECTURE.md modified
Index: clean
```

3. Files modified

- [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

4. Architecture sections modified

- §30.2 Sort storage and records
- §30.4 In-memory sort
- §30.8 Sorting invariants, item 11

5. MINOR-1 before/after

Before:

- fixed “initial” 8–16-byte prefix target;
- “initial implementation” prescribed an introsort-style algorithm;
- radix sorting was described as a “future optimization.”

After:

- prefix width is an implementation/configuration tuning choice;
- comparison sort and other proven-equivalent algorithms are permitted;
- radix, normalized-key, and hybrid approaches are timeless conditional capabilities.

6. Final normalized-prefix-width rule

Prefix width may vary by implementation or key shape. Shorter and zero-length prefixes are legal when useful sound normalization is unavailable. No width is a correctness requirement.

7. Final in-memory-sort wording

In-memory sorting may use a high-quality comparison sort or another algorithm proven equivalent to the complete semantic comparator for its supported key domain.

8. Final radix/normalized-key wording

Radix, normalized-key, and hybrid strategies are permitted only where their ordering is proven equivalent to the complete semantic comparator. Otherwise, execution uses the comparator/fallback path.

9. Chronology audit

The identified `initial` and `future optimization` phrases are gone. No project chronology remains from MINOR-1.

The remaining “current in-memory run” wording describes runtime state, not project chronology.

10. MINOR-2 before/after

Before, §30.8(11) required equivalence “including bag … tie … behavior,” which could be misread as requiring identical tied occurrences across executions.

After, it explicitly defines equivalence as membership in the same Chapter-20 permitted ordered-result family.

11. Final tie-outcome rule

When LIMIT/OFFSET cuts through a complete ORDER BY comparator-equivalence class:

- valid providers may choose and order different actual child occurrences within that class;
- exact cardinality and every strict comparator relation remain preserved;
- only actual child occurrences may be returned;
- deduplication and fabrication are forbidden;
- no hidden RID, payload, or stable-input tiebreaker becomes SQL semantics.

12. Tie example

No example was added. The normative rule is sufficiently direct without expanding Chapter 30 into a test catalog.

13. Optional blocking-role sentence

Not added. Existing §30.7 “After input completion” wording and Chapter 26 already establish the positive-output Top-N blocking lifecycle unambiguously.

14. Explicit answers

| Question | Answer |
|---|---|
| Can Sort+Limit and Top-N return different boundary-tied rows? | Yes |
| Can IndexScan+Limit and Sort+Limit return different boundary-tied rows? | Yes |
| Does OrderingProperty imply stability? | No |
| Is hidden RID tie order SQL-visible? | No |
| Must strict non-tied ordering agree? | Yes |

15. No hidden deterministic tiebreaker was introduced.

16. Exact-K semantics are unchanged.

17. Comparator semantics are unchanged.

18. External-sort, spill, and post-readiness result-prefix semantics are unchanged.

19. Chapters 1–29 were not modified.

20. New Architecture semantic question discovered: **NONE**.

21. `git diff --check`: **PASS**.

22. Diff summary

```text
docs/ARCHITECTURE.md | 29 +++++++++++++++++++----------
1 file changed, 19 insertions(+), 10 deletions(-)
```

23. Final Git status

```text
 M docs/ARCHITECTURE.md
```

No staged changes.

24. Confirmations

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
```

CHAPTER 30 ARCHITECTURE READY FOR READ-ONLY CLOSURE AUDIT

# Chapter 30 final architecture closure audit

## 1. Final closure verdict

**CLEAN — ARCHITECTURE CLOSED**

Chapter 30 is now a complete, timeless, internally consistent v1 Architecture contract. The two original minor findings are closed, no new finding or semantic question exists, and all remaining physical variability is intentional and bounded.

## 2. Initial HEAD/status

The repository had advanced since the Fix-A report:

```text
HEAD:
0a42ccae461a857d1341405cf7326dc1d806a02e

Commit:
0a42cca applied FIX-A 30 in ARCHITECTURE

Working tree:
clean

Index:
clean

Untracked paths:
none
```

Fix A was already committed before this audit.

## 3. Final HEAD/status

```text
HEAD:
0a42ccae461a857d1341405cf7326dc1d806a02e

Commit:
0a42cca applied FIX-A 30 in ARCHITECTURE

Working tree:
clean

Index:
clean

Untracked paths:
none
```

## 4. Audit-created changes

**NONE.**

`git diff -- docs/ARCHITECTURE.md` remained empty.

## 5. Exact Chapter-30 boundary and subsection inventory

```text
Heading:
# 30. Sorting and Top-N

Start:
line 22568

End:
line 22793

Chapter-31 boundary:
line 22794
# 31. DML, DDL, VACUUM, and Result Interface
```

| Lines | Subsection |
|---:|---|
| 22570–22595 | §30.1 PhysicalSort |
| 22596–22628 | §30.2 Sort storage and records |
| 22629–22653 | §30.3 Sort comparator |
| 22654–22665 | §30.4 In-memory sort |
| 22666–22691 | §30.5 External merge sort |
| 22692–22710 | §30.6 Sort-run temporary format |
| 22711–22763 | §30.7 PhysicalTopN |
| 22764–22791 | §30.8 Sorting invariants |
| 22792–22793 | Chapter separator and trailing blank line |

## 6. Context sections inspected

- Architecture front matter and normative-language rules
- §17.4.3 FLOAT64
- §17.4.6 VARCHAR
- §§19.13–19.14
- §§20.11–20.12, 20.17, 20.17.10
- §§22.3–22.4.1
- §§23.1, 23.9–23.13
- §§24.1–24.10
- §§25.1–25.1.1
- §§26.1–26.10
- §27.9
- §29.10 comparator/order handoff
- Chapter 30 in full
- §31.10
- §32.7
- §§37.2–37.5
- §§38.13, 38.15–38.16, 38.24
- §§39.1.3, 39.3
- §41.5
- Relevant existing Verification families and generic Sort Tests

## 7. Original finding closure table

| Finding | Status | Evidence |
|---|---|---|
| MINOR-1 — chronology/tuning leakage | **CLOSED** | §§30.2 and 30.4 now use timeless tuning and capability language; 8–16-byte target, “initial implementation,” and “future optimization” are absent |
| MINOR-2 — Top-N tie equivalence | **CLOSED** | §30.8(11) now explicitly defines one Chapter-20 permitted result family and permits different actual occurrences only within a complete-key tie class |

## 8–13. Finding counts

| Class | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 0 |
| EDITORIAL | 0 |
| DESIGN-SCOPE QUESTION | 0 |
| FROZEN SEMANTIC QUESTION | 0 |

## 14. MINOR-1 closure assessment

MINOR-1 is fully closed.

The normalized-prefix width is no longer a fixed or phased target. The in-memory algorithm is no longer tied to an initial introsort implementation, and radix sorting is no longer narrated as future work.

The new language distinguishes:

- mandatory comparator equivalence;
- optional algorithm choices;
- implementation/configuration tuning;
- legal fallback to the complete comparator.

## 15. Complete remaining temporal-language search

| Live wording | Location | Classification |
|---|---:|---|
| `NULLS FIRST/LAST` | §§30.1, 30.3 | SQL ordering terminology, not chronology |
| “implementation/configuration tuning choice” | §30.2, lines 22609–22611 | Timeless implementation freedom |
| “current in-memory run” | §30.5, line 22668 | Legitimate runtime state |
| “selected implementation” | §30.7, line 22730 | Timeless capability/applicability condition |
| “An implementation MAY…” | §30.7, line 22735 | Timeless implementation freedom |

Absent from Chapter 30:

```text
initial
future
currently
later implementation
planned
eventually
first implementation
roadmap
phase
```

No stale project chronology remains.

## 16. Final normalized-prefix-width contract

The live rule is precise:

- width is an implementation/configuration tuning choice;
- it may vary by key shape or implementation;
- shorter prefixes are legal;
- zero-length prefixes are legal;
- no numeric width is a correctness requirement;
- unavailable or unhelpful normalization falls back to complete comparison;
- every prefix-based ordering decision must be comparator-sound.

The former 8–16-byte target is absent.

## 17. Final in-memory-sort algorithm freedom

§30.4 permits:

- a high-quality comparison sort; or
- another algorithm proven equivalent to the complete semantic comparator over its supported key domain.

It does not require introsort, `std::sort`, one C++ library primitive, or one initial implementation strategy.

## 18. Final radix/normalized-key capability wording

Radix, normalized-key, and hybrid strategies are timeless optional mechanisms. They are legal only when their ordering is proven equivalent to the complete semantic comparator. Otherwise, execution uses the full comparator/fallback path.

No implementation availability or future-phase promise is made.

## 19. MINOR-2 closure assessment

MINOR-2 is fully closed.

§30.8(11) now states that Top-N and exact ordering-provider-plus-Limit executions belong to the same Chapter-20 permitted ordered-result family.

When LIMIT/OFFSET cuts a complete-key tie class, implementations may choose and order different actual child occurrences within that class. They must still:

- preserve exact result cardinality;
- return only child occurrences;
- preserve every strict comparator relation;
- avoid deduplication and fabrication;
- preserve demanded errors;
- preserve schema and `LogicalSlotId`s;
- preserve transaction/result semantics.

For:

```text
(A, key=5)
(B, key=5)
(C, key=5)

ORDER BY key LIMIT 1
```

`A`, `B`, or `C` is legal.

## 20. Sort+Limit versus Top-N tied rows

**YES.** They may return different visible boundary-tied rows when those rows compare equal on every resolved ORDER BY key.

## 21. IndexScan+Limit versus Sort+Limit tied rows

**YES.** An index’s internal RID tie order does not become SQL order, so the providers may select different tied occurrences.

## 22. OrderingProperty stability

**NO.** `OrderingProperty` guarantees key monotonicity with exact key descriptors. It does not guarantee stability inside a complete-key tie class.

## 23. Hidden-tiebreaker audit

No hidden semantic tiebreaker exists.

Specifically, none of these becomes SQL-visible tie order:

- RID;
- row address;
- payload;
- source position;
- insertion order;
- worker order;
- run order;
- heap insertion order;
- stable input order.

Physical implementations may use deterministic internal choices, but those choices are not part of the advertised OrderingProperty or SQL semantics.

## 24. Bag and multiplicity preservation

Full `PhysicalSort` preserves every child logical occurrence exactly once.

It cannot:

- deduplicate equal rows;
- collapse equal sort keys;
- fabricate rows;
- lose duplicate occurrences;
- canonicalize unrelated payloads.

Equal-key instability affects only relative order.

Top-N removes occurrences solely through the frozen LIMIT/OFFSET selection contract and retains duplicate occurrences as occurrences rather than unique values.

## 25. Comparator consistency

The same complete semantic comparator governs:

- in-memory sort;
- external-run generation;
- k-way merge;
- Top-N membership;
- Top-N retained-output sorting;
- realization of the advertised ordering property.

It composes:

```text
resolved key sequence
per-key ASC/DESC
resolved NULLS FIRST/LAST
Chapter-17 scalar order
binary VARCHAR collation
Chapter-17 FLOAT64 total order
```

No optimized path introduces a different equality class.

## 26. FLOAT64 comparator and tie classes

The live contract remains:

- `-0.0` and `+0.0` compare equal for ORDER BY;
- all ORDER-BY-equivalent NaNs compare equal;
- NaN sorts after positive infinity in ascending scalar order;
- projected payload bits are not canonicalized merely because key comparison reports equality.

Consequently, comparator-equal rows may remain visibly byte-distinct, and boundary tie selection may expose either occurrence. This is intentional.

## 27. Normalized-prefix soundness

Every order decision made from prefixes must imply the result of the complete comparator.

The contract remains sound for:

- resolved NULL placement;
- ASC and DESC;
- signed integers;
- DATE/TIMESTAMP;
- FLOAT64 zero and NaN classes;
- binary VARCHAR;
- composite keys.

Prefix equality or insufficient prefix information invokes the complete comparator unless another exact semantic proof exists.

## 28. VARCHAR prefix counterexamples

For `"a"` versus `"aa"`:

- a sound prefix encoding may include sufficient terminator/length information; or
- the prefixes remain equal and full comparison resolves the order.

Plain truncation cannot incorrectly decide the order.

Embedded NUL, high bytes, and long common prefixes remain governed by exact binary byte comparison. Zero-length fallback is legal.

## 29. FLOAT prefix assessment

A sound FLOAT64 prefix must preserve Chapter-17 ordering and comparator equality:

- signed zeros cannot become differently ordered;
- equivalent NaNs cannot acquire incompatible ordering;
- infinities and finite values retain their defined positions;
- host NaN comparison is not an acceptable semantic substitute.

Original projected FLOAT64 bits remain payload, not prefix semantics.

## 30. Persistent-index-codec separation

Chapter 30 does not equate runtime sort prefixes with persistent `IndexKeyCodec` bytes.

Any implementation reuse requires an independent proof covering:

- direction;
- NULL ordering;
- collation;
- FLOAT64 semantics;
- truncation;
- composite boundaries.

Sort normalization creates no persistent compatibility, WAL, or index-format ABI.

## 31. Sort-key evaluation and demanded errors

Chapters 20 and 25 remain authoritative.

A demanded sort-key expression cannot be skipped merely because:

- a prefix already distinguishes another key;
- Top-N already holds K rows;
- a candidate currently appears worse;
- LIMIT is small.

Speculative undemanded work cannot manufacture a public error. Source, chunk, worker, run, and discovery order do not create sort-specific diagnostic precedence.

## 32. PhysicalSort lifecycle

The owner chain defines:

```text
Sink:
    consume demanded input
    evaluate keys
    retain rows
    create runs as required

Finalize:
    complete final run/sort
    establish merge/output readiness
    publish successful dependency readiness

Source:
    emit comparator-ordered chunks
```

No pre-readiness row may be exposed.

Readiness does not require pre-reading every future external-run byte.

## 33. External merge progress

Multiple runs are merged with a memory-bounded fan-in.

Every successful pressure cycle must:

- obtain the needed resource;
- make relevant well-founded progress; or
- terminate with a controlled existing failure.

A fan-in-one rewrite that leaves the same run count indefinitely is not progress. If a merge of at least two runs cannot be supported and no other exact progress action exists, Chapter 24 requires controlled failure rather than an infinite loop.

## 34. Temporary-run validation

Sort runs remain:

- query/attempt-temporary;
- nonpersistent;
- not WAL-logged;
- not crash-recovered;
- outside long-lived compatibility guarantees.

Fingerprints detect mismatches; they are not collision-free semantic identity. Checksums are integrity signals, not replacements for framing, extent, count, offset, range, and owner-structure validation.

## 35. Post-readiness Source failure

The owner chain still permits:

```text
successful readiness
→ one or more returned result chunks
→ later spill-run read/checksum failure
→ SpillIOError and failed query completion
```

Already returned chunks are not retracted, but they do not make the query successfully complete.

Fix A did not import Chapter 29’s stronger aggregate publication barrier into Sort.

## 36. PhysicalTopN blocking assessment

For positive demanded output, `PhysicalTopN` is unambiguously blocking:

- any later demanded row may improve the retained set;
- output sorting/emission occurs after input completion;
- Chapter 26 prevents Source use before successful readiness.

Reaching heap size K does not authorize early input termination. Only the Chapter-20/26 semantic-demand rules can make remaining input unnecessary.

## 37. Exact-K assessment

Unchanged and coherent:

```text
K = exact mathematical LIMIT + OFFSET
```

K may exceed INT64.

If a selected implementation cannot represent and honor exact K:

- that Top-N alternative is ineligible;
- the SQL remains valid;
- another exact ordering provider plus `PhysicalLimit` remains available;
- no public count overflow is introduced.

Saturation or clamping remains forbidden without an independent exact proof.

## 38. LIMIT-zero/OFFSET assessment

`LIMIT 0 OFFSET M` remains coherent:

```text
logical result = empty
mathematical Top-N K = M
```

The exact LIMIT-zero semantic proof may avoid child execution independently of K. The Top-N algorithm need not be entered merely because mathematical K is positive.

No contradiction exists among Chapters 20, 22, 27, 30, and 38.

## 39. Top-N heap and multiplicity

The heap:

- retains at most exact K occurrence records;
- treats equal-key rows as distinct occurrences;
- may retain or discard equal boundary candidates under the permitted tie rule;
- cannot deduplicate tied values;
- cannot fabricate rows;
- cannot stop input merely upon becoming full.

## 40. Top-N output ordering

Heap layout is never SQL order.

After input completion, Top-N:

1. sorts retained records with the complete comparator;
2. skips OFFSET;
3. emits up to LIMIT rows.

Every strict comparator relation and resolved NULL ordering remains intact.

## 41. OrderingProperty assessment

Chapter 37 continues to own property representation and satisfaction.

Matching requires identical:

- `LogicalSlotId`;
- direction;
- NULL placement;
- collation.

The required key vector must be an exact prefix of the provided key vector. Stability and physical tie order are absent from the property.

## 42. RequiredSlotSet and hidden keys

RequiredSlotSet remains Chapter-37-owned.

Sort retains:

- hidden computed ORDER BY slots;
- downstream-required payload;
- row-occurrence cardinality even when visible payload is empty.

No required key may be pruned. Chapter 30 creates no competing pruning system.

## 43. Memory/resource ownership

Chapter 24 covers all potentially growing sort state:

- prefixes and complete keys;
- row/payload storage;
- VARCHAR backing;
- run directories and metadata;
- read/write buffers;
- merge heap/state;
- worker-local runs;
- Top-N heap;
- output buffers.

A huge row requires an exact supported representation or the existing controlled representability/OOM result. Truncation is forbidden.

## 44. Cancellation and retry

Cancellation and failed-attempt cleanup cover:

- retained rows;
- prefixes;
- runs;
- merge cursors;
- Top-N heap;
- readiness;
- Source cursor.

An authorized retry receives fresh query/attempt-local state and spill ownership. No failed-attempt run or cursor may leak into it.

## 45. Parallel-sort boundary

Chapter 32 owns parallel mechanics:

```text
worker-local run generation
→ merge tree / k-way merge
→ ordered Source
```

Workers may affect tie order only. They may not alter:

- the row bag;
- strict comparator order;
- demanded-error semantics;
- final OrderingProperty.

Chapter 30 does not duplicate the parallel scheduler contract.

## 46. Document-role assessment

Chapter 30 now contains:

- timeless semantic requirements;
- timeless capability conditions;
- implementation freedom;
- performance rationale tied to real database mechanisms.

It contains no:

- implementation-status narration;
- roadmap;
- future-work promise;
- project sequencing;
- benchmark procedure;
- Verification procedure;
- persistent-format claim for temporary sort state.

Document-role result: **CLEAN**.

## 47. Complexity/project-scope recheck

| Mechanism | Classification | Result |
|---|---|---|
| Comparison sort | CORE | Proportionate |
| Compact sort records | CORE | Proportionate |
| Normalized prefixes | JUSTIFIED ADVANCED | Mechanism retained without fixed width |
| External merge sort | CORE | Necessary under bounded memory |
| Checksummed/versioned temporary runs | JUSTIFIED ADVANCED | Reuses Chapter-24 robust spill model |
| Top-N | CORE | Standard high-value operator |
| Exact-K representability | JUSTIFIED ADVANCED | Prevents cardinality/overflow errors |

No new design-scope question was introduced.

## 48. Previous-chapter regression

| Owner | Result |
|---|---|
| Chapter 17 scalar/FLOAT/VARCHAR ordering | Unchanged and consistent |
| Chapter 19 ORDER BY/LIMIT binding | Unchanged |
| Chapter 20 bag, instability, slicing, demand | Unchanged and now explicitly delegated |
| Chapter 22 physical applicability | Unchanged |
| Chapter 23 occurrence/lifetime semantics | Unchanged |
| Chapter 24 memory/spill/progress | Unchanged |
| Chapter 25 error ownership | Unchanged |
| Chapter 26 blocking/readiness | Unchanged |
| Chapter 27 PhysicalLimit | Unchanged |
| Chapter 29 comparator handoff | Unchanged |
| Chapter 31 result-prefix boundary | Unchanged |
| Chapter 32 parallel sort | Unchanged |
| Chapter 37 properties/required slots | Unchanged |
| Chapter 38 exact-K/cost/validation | Unchanged |
| Chapter 39 failures | Unchanged |

Regression result: **NONE**.

## 49. Verification reusable-coverage inventory

Existing reusable coverage includes:

- Chapter-17 scalar, FLOAT64, and VARCHAR comparator oracles;
- V19 ORDER BY and LIMIT/OFFSET binding;
- V20-10 Sort bag/order/tie classes;
- V20-11 LogicalLimit;
- V22-C bags and properties;
- V22-G algorithm substitutability;
- V22-J exact-K domain and saturation cases;
- V22-K physical-plan validation;
- V23 retained VARCHAR/borrow lifetime;
- V24 accounting, spill validation, progress, errors, and cleanup;
- V25 demanded-error selection;
- V26 blocking/readiness, returned-prefix, cancellation, and retry;
- physical property and enforcement tests;
- generic Sort Tests for direction, NULLs, multiple keys, equal keys, VARCHAR/FLOAT edges, empty/one-row, external runs, and multi-pass merge.

## 50. Verification missing/partial Chapter-30 inventory

A dedicated Chapter-30 synchronization should add or strengthen:

- normalized-prefix soundness by type;
- zero-length prefix fallback;
- VARCHAR `"a"`/`"aa"`, embedded-NUL, and common-prefix cases;
- FLOAT64 zero/NaN prefix equivalence;
- tie-outcome-family comparison across Sort, Top-N, and index providers;
- full-Sort occurrence ledger;
- Top-N blocking barriers;
- Top-N duplicate-occurrence retention;
- run-generation and merge occurrence ledger;
- fan-in-below-two controlled termination;
- multi-pass progress oracle;
- run comparator mismatch injection;
- fingerprint-not-semantic-identity cases;
- checksum-success plus malformed-structure cases;
- huge-row retention/error boundary;
- post-readiness spill-read failure after a returned prefix;
- sort-attempt retry poisoning/freshness;
- complete sort-owned memory ledger;
- parallel local-run/final-merge coverage;
- Top-N OOM and no unowned runtime replanning;
- hidden-key and zero-visible-payload fixtures.

These are Verification synchronization needs, not Architecture gaps.

## 51. Complete BLOCKING findings

**NONE.**

## 52. Complete MAJOR findings

**NONE.**

## 53. Complete MINOR findings

**NONE.**

## 54. Complete EDITORIAL findings

**NONE.**

## 55. Complete DESIGN-SCOPE questions

**NONE.**

## 56. N30-* semantic questions

**NONE.**

## 57. Remaining Architecture action

**NONE.**

Chapter 30 requires no further Architecture edit before Verification synchronization.

## 58. Next task recommendation

```text
CHAPTER-30 VERIFICATION SYNCHRONIZATION
```

`git diff --check`: **PASS — no output**.

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