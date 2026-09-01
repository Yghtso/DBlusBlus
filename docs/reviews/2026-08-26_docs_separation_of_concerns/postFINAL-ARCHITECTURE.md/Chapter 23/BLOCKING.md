# D23-S1–D23-S4 semantic integration — PASS

All four approved decisions are integrated without reopening policy. No frozen cross-owner conflict or new semantic question was found.

## Repository state

| Check | Initial | Final |
|---|---|---|
| HEAD | `0345d8edbf5980b534746d11e3ccfe08458f4790` | unchanged |
| Working tree | clean | `M docs/ARCHITECTURE.md` |
| Index | clean | clean |
| Task-created diff | none | 193 insertions, 27 deletions |
| `git diff --check` | — | passed |

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) was modified. No external/user changes were present or disturbed. Historical review artifacts were unread, unmodified, and unstaged.

## Sections modified

- [§23.1 DataChunk capacity](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19118)
- [§23.3 Vector kinds](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19183)
- [§23.6 SelectionVector](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19283)
- [§23.7 Dictionary composition](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19332)
- [§23.8 UnifiedVectorFormat](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19369)
- [§23.9 StringRef](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19398)
- [§23.10 String ownership](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19473)
- [§23.12 Borrowed vectors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19550)
- [§23.13 Chunk/vector reuse](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19585)
- [§23.14 Vector/string invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19606)
- [§26.4 Runtime interfaces](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20174)
- [§26.6 Borrowed-data lifetime](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20243)
- [§26.10 Pipeline invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20308)
- [§39.3 Execution errors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26714)

## D23-S1 — Active logical selection domain

The final rule is:

- A `SelectionVector` addresses active logical positions of its immediate child.
- The exact bound is `0 <= index < child logical cardinality`, never allocated capacity.
- FLAT logical position `i` resolves to initialized payload and validity position `i`.
- Every valid CONSTANT logical position resolves to physical scalar position zero.
- DICTIONARY resolution proceeds through its own selection and immediate child.
- Every nested intermediate index is validated before following or flattening it.
- Repeated and unsorted indices are legal and preserve listed occurrence order and multiplicity.
- Inactive allocated positions remain inaccessible even when stale bytes remain after reuse.
- Every active logical position resolves to initialized payload, validity, and representation state.

An out-of-domain selection is an invalid internal runtime representation. It must be prevented or rejected before payload, validity, `StringRef`, or child-mapping access. It cannot expose stale data and is not a public SQL error.

## D23-S2 — Value-stable borrowing

Owner liveness and value stability are now explicitly distinct. Every borrowed, reference, dictionary, or selection-backed view is a value-stable logical view for its borrow interval.

The protected backing state includes, where applicable:

- active logical cardinality;
- type and representation metadata;
- reachable payload and validity;
- selection storage and entries;
- dictionary child/base relationship;
- required addresses and ranges;
- `StringRef` length, prefix, and data reference;
- referenced VARCHAR bytes.

Reachable state cannot be reset, reused, overwritten, incompatibly reallocated, or observably mutated while borrowed. Unreachable inactive storage may change.

CONSTANT divergence must first establish a representation capable of preserving every live view. The same rule prevents selection or base mutation from changing a live DICTIONARY. Input/output aliasing is legal only while all live aliases remain observationally stable.

Zero-copy remains permitted through immutable backing, delayed mutation, ownership transfer, copy-on-write, valid owner retention, pinning under an existing owner contract, materialization, deep copy, or another exact mechanism. No mechanism is mandated.

Retention beyond the borrow interval requires a new valid owner or materialization/copy. Reset and reuse wait until borrowers finish or obtain independent valid ownership. The page-to-chunk VARCHAR copy rule remains unchanged.

## D23-S3 — Exact StringRef representability

The compact `StringRef` form exactly represents lengths through `UINT32_MAX`.

For longer values:

- the compact form is inapplicable;
- truncation, wraparound, modulo narrowing, semantic splitting, NUL clipping, and reinterpretation are forbidden;
- another exact runtime representation may be supplied;
- no particular structure, width, rope, LOB, or pointer scheme is prescribed;
- when an exact supported alternative exists, execution cannot fail merely because an incapable compact realization was chosen.

If no supported exact representation exists, materialization raises controlled `ExecutionError` with a runtime value-representability/resource-limit cause.

`OutOfMemory` remains distinct: it applies when an exact representation is supported but its required allocation cannot be obtained.

Chapter 17’s arbitrary finite byte-string domain remains unchanged. The heap tuple limit was not generalized into a runtime VARCHAR maximum. Tuple, catalog, statistics, spill, and WAL formats remain unchanged.

## D23-S4 — Positive executable capacity

An executable `DataChunk` now requires:

```text
1 <= capacity <= 65535
0 <= cardinality <= capacity
```

`STANDARD_VECTOR_SIZE` remains 1024, and positive custom capacities through 65535 remain permitted where applicable.

A zero-capacity container may exist for uninitialized, default, moved-from, or bookkeeping purposes, but it is outside executable batch state. Emitting or consuming one is an internal invalid runtime state, not a SQL error.

Cardinality zero remains a valid empty batch and does not mean EOS or `FINISHED`.

A source returning empty `HAVE_MORE` must advance finite source or operator state. An unbounded empty, no-progress sequence cannot substitute for `FINISHED` and is an invalid pipeline implementation/liveness violation.

## Chapter 26 and §39 synchronization

Chapter 26 now states that:

- empty `HAVE_MORE` output advances finite state;
- borrowed pipeline data remains value-stable, not merely allocated;
- a retaining sink may retain valid ownership or copy/materialize;
- empty no-progress sequences and owner recycling cannot violate the Chapter-23 contracts.

Section 39.3 now assigns missing exact runtime VARCHAR representation to controlled `ExecutionError` with a representability/resource-limit cause, while preserving `OutOfMemory` for actual allocation failure. Existing §39.1 transaction consequences were not changed.

## Invalid-state taxonomy

| Condition | Classification |
|---|---|
| Selection outside immediate-child active domain | Internal invalid runtime representation |
| Expired or value-unstable borrow | Internal lifetime/aliasing invariant violation |
| Zero-capacity executable chunk | Internal invalid runtime state |
| Endless empty `HAVE_MORE` without progress | Invalid pipeline implementation/liveness violation |
| No exact runtime representation for valid VARCHAR | Controlled `ExecutionError`, representability/resource cause |
| Allocation failure for supported exact form | `OutOfMemory` |

Malformed selection, borrow, or executable-chunk states must be rejected before they can cause out-of-bounds access, stale values, dangling access, arbitrary mutation, or persistent corruption. Construction invariants or boundary validation may enforce this; no universal validator API was prescribed.

## Cross-decision composition

- D23-S1 + D23-S2: a DICTIONARY requires both valid active-domain indices and stable mapping/base/value state.
- D23-S1 + D23-S4: positive capacity does not imply selectable rows; capacity 1024 with cardinality zero has no valid selection target.
- D23-S2 + reset: reset waits for borrowers or independent stable ownership.
- D23-S2 + D23-S3: ownership or deep copying does not solve an over-domain length field; another exact representation is still required.

## Regression assessment

- Chapter 17: unchanged; all scalar, NULL, FLOAT, VARCHAR, embedded-NUL, NaN, and signed-zero semantics preserved.
- Chapter 20: unchanged; repeated indices remain repeated bag occurrences with no deduplication.
- Chapter 22: unchanged; vector width, lane, chunk boundary, representation, properties, and D22-S1 remain nonsemantic as frozen.
- Chapter 24: task-unchanged; query memory, spill, retention, and OOM ownership preserved.
- Chapter 25: task-unchanged; expression and error semantics preserved.
- Chapter 26: only borrow-lifetime and empty-batch progress synchronization changed.
- Transactions: no TxnId, CommandId, snapshot, MVCC, retry, publication, or D21 change.
- Persistence: no page, tuple, catalog, WAL, control, RID, SchemaVer, or TypeId change.
- Implementation freedom: preserved across selection storage, normalization, borrow mechanisms, alternate string representations, constructors, and allocation.
- Timelessness: no new project chronology was introduced. Existing N23-1 wording remains untouched.

## Semantic reread answers 1–92

1. Yes—selection targets immediate-child active logical positions.
2. Yes—capacity is not the selection domain.
3. Yes—FLAT mapping is exact.
4. Yes—a CONSTANT logical index above zero is valid when below cardinality.
5. Yes—CONSTANT resolves to payload position zero.
6. Yes—DICTIONARY targets child logical positions.
7. Yes—all nested intermediate bounds are validated.
8. Yes—repeated indices are legal.
9. Yes—unsorted indices are legal.
10. Yes—listed occurrence order is preserved.
11. Yes—inactive capacity is inaccessible.
12. Yes—active positions must be initialized.
13. Yes—invalid selection is rejected or prevented before dereference.
14. Yes—it is internal invalid state.
15. Yes—no public SQL error was introduced.
16. Yes—there is no stale-value fallback.
17. Yes—owner lifetime is distinct from stability.
18. Yes—the borrow is value-stable.
19. Yes—payload remains stable.
20. Yes—validity remains stable.
21. Yes—selection state remains stable.
22. Yes—the dictionary relationship remains stable.
23. Yes—`StringRef` metadata remains stable.
24. Yes—referenced bytes remain stable.
25. Yes—reachable owners cannot reset or reuse storage.
26. Yes—reachable state cannot mutate observably.
27. Yes—unreachable inactive storage may mutate.
28. Yes—zero-copy remains allowed.
29. Yes—copying remains allowed.
30. Yes—copy-on-write remains allowed.
31. Yes—ownership transfer remains allowed.
32. Yes—delayed mutation remains allowed.
33. Yes—no mechanism is mandated.
34. Yes—the CONSTANT divergence rule is defined.
35. Yes—DICTIONARY base and selection mutation are covered.
36. Yes—input/output aliasing requires stability.
37. Yes—retention requires a new owner or materialization.
38. Yes—the page-to-chunk string-copy rule is unchanged.
39. Yes—compact `StringRef` is exact through `UINT32_MAX`.
40. Yes—the compact form is inapplicable above it.
41. Yes—truncation is forbidden.
42. Yes—wraparound is forbidden.
43. Yes—NUL-termination reinterpretation is forbidden.
44. Yes—an alternate exact form is permitted.
45. Yes—it is not mandated.
46. Yes—an incapable compact realization is inapplicable when an exact alternative exists.
47. Yes—absence of an exact form produces controlled `ExecutionError`.
48. Yes—the cause is runtime value representability/resource limit.
49. Yes—`OutOfMemory` remains distinct.
50. Yes—the SQL VARCHAR domain is unchanged.
51. Yes—the heap tuple maximum is not a universal runtime maximum.
52. Yes—persistent formats are unchanged.
53. Yes—no concrete alternate representation is prescribed.
54. Yes—an incapable compact choice cannot cause failure when exact supported capability exists.
55. Yes—executable capacity has minimum 1.
56. Yes—the maximum remains 65535.
57. Yes—cardinality zero remains legal.
58. Yes—cardinality remains bounded by capacity.
59. Yes—1024 remains standard.
60. Yes—positive custom capacities remain permitted where applicable.
61. Yes—zero-capacity execution is forbidden.
62. Yes—zero-capacity bookkeeping state is permitted.
63. Yes—empty chunk remains distinct from EOS and `FINISHED`.
64. Yes—empty `HAVE_MORE` must advance finite state.
65. Yes—endless empty no-progress output is forbidden.
66. Yes—constructor and API mechanics remain unspecified.
67. Yes—a positive empty chunk has zero valid selection targets.
68. Yes—selection validity and borrow stability compose.
69. Yes—reset waits for borrowers or independent ownership.
70. Yes—deep copy alone does not solve an over-domain length.
71. Yes—D23-S3 leaves Chapter-17 semantics unchanged.
72. Yes—D23-S4 leaves EOS semantics unchanged.
73. Yes—no semantic identity was introduced.
74. Yes—transactions are unchanged.
75. Yes—persistence is unchanged.
76. Yes—D22 is unchanged.
77. Yes—M23-2 remains open.
78. Yes—N23-1 remains open.
79. No new frozen semantic question was found.
80. B23-1 is closed.
81. B23-2 is closed.
82. B23-3 is closed.
83. M23-1 is closed.
84. Q23-1 is closed.
85. Q23-2 is closed.
86. Q23-3 is closed.
87. Q23-4 is closed.
88. Yes—Chapter 23 is semantically clean.
89. No—Chapter 23 is not document-clean.
90. No—Chapter 23 is not fully closed.
91. No—Verification is not synchronized by this task.
92. No—Chapter 24 has not been reviewed.

## Closure status

| Item | Status |
|---|---|
| D23-S1 | CLOSED |
| D23-S2 | CLOSED |
| D23-S3 | CLOSED |
| D23-S4 | CLOSED |
| Q23-1–Q23-4 | CLOSED |
| B23-1–B23-3 | CLOSED |
| M23-1 | CLOSED |
| Frozen Chapter-23 semantic questions | NONE |
| M23-2 | OPEN / DOCUMENT-ONLY |
| N23-1 | OPEN / DOCUMENT-ONLY |
| Chapter-23 Architecture | SEMANTICALLY CLEAN |
| Chapter-23 document status | NOT YET DOCUMENT-CLEAN |
| Chapter 23 fully closed | NO |
| Chapter-23 Verification | SYNCHRONIZATION PENDING |
| Chapter 24 review | NOT STARTED |

## Diff ownership

Task-created hunks cover classes A–V:

- A–B: executable capacity and empty/EOS.
- C–F: representation-specific active-domain resolution and normalization.
- G–I: compact StringRef, exact alternatives, and representability failure.
- J–L: value-stable ownership, borrowing, and reset/reuse.
- M–P: consolidated D23-S1–S4 invariants.
- Q–R: Chapter-26 progress and lifetime synchronization.
- S: §39.3 error ownership.
- T–U: exact owner navigation and analytical rationale.
- V: necessary Markdown wrapping.

No M23-2 slot-handoff cleanup or N23-1 chronology cleanup occurred.

Next task: **TARGETED CHAPTER-23 DOCUMENT-ONLY CLEANUP**.

No build, tests, benchmarks, staging, commit, implementation, devlog, or review artifact occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
