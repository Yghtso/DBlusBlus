# D25-S1 semantic-integration verdict

D25-S1 is integrated and CLOSED. Q25-1 and B25-1 are CLOSED. Chapter 25 is semantically clean, but the seven nonsemantic document/integration findings remain open.

No cross-owner semantic conflict was found.

## Repository state

Initial:

- HEAD: `22701a5511270c50a6d7e050dc726138a6b1d50c`
- Index: clean
- `docs/ARCHITECTURE.md`: clean
- Pre-existing untracked artifact: `docs/reviews/.../Chapter 25/BLOCKING.md`

Final:

- HEAD unchanged
- Index clean
- Modified: `docs/ARCHITECTURE.md`
- Pre-existing review artifact remains untracked
- Diff: 109 insertions, 0 deletions
- `git diff --check`: passed

The pre-existing review artifact was not read, modified, moved, or staged. No other external repository changes were observed.

## Sections modified

- [§20.17 owner handoff](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17480)
- [§25.1.1 Ordinary non-DML runtime expression-error selection](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20258)
- [§25.8 Expression invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20443)
- [§31.10 Query result interface](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22321)

## Integrated candidate semantics

An ordinary non-DML candidate exists for each semantically demanded scalar evaluation occurrence whose canonical Chapter-17/20 evaluation reaches an ordinary runtime scalar semantic error.

Excluded from candidate establishment:

- undemanded CASE branches;
- short-circuited AND/OR operands;
- skipped IN-list work;
- per-row expressions with no demanded row occurrence;
- errors from speculative undemanded evaluation.

Candidate existence is semantic, not determined by physical discovery.

Within one scalar occurrence, Chapters 17 and 20 remain authoritative for child order, short-circuiting, demand, and the responsible failing subexpression. D25-S1 compares only the resulting occurrence-level failures.

Each candidate conceptually retains:

- canonical source-derived expression occurrence and diagnostic provenance;
- responsible `SourceSpan`;
- public error category;
- closed Chapter-17 conceptual cause;
- any other already-frozen diagnostic metadata.

A demanded row occurrence establishes candidate membership and multiplicity. It is not a ranking key, SQL ordering key, or new row identifier.

## Final ranking

The public error is the minimum under this lexicographic semantic preorder:

1. smallest responsible `SourceSpan.start_byte_offset`;
2. for equal starts, shorter represented `SourceSpan`;
3. for identical spans, canonical semantic source-expression occurrence order preserved through parsing, binding, and logical rewriting;
4. for the same semantic expression occurrence:

```text
INVALID_CAST
NUMERIC_OVERFLOW
DIVISION_BY_ZERO
INVALID_DATE
INVALID_TIMESTAMP
```

This cause order is an Architecture semantic order. It does not depend on enum values, declaration order, registry iteration, container order, or function-table addresses.

SourceSpan alone is insufficient because one expression occurrence can produce distinct public causes over different rows. For example, one integer division occurrence can produce either `DIVISION_BY_ZERO` or `NUMERIC_OVERFLOW`.

## Candidate equivalence

Candidates are observationally equivalent only when all frozen fields agree:

- semantic diagnostic origin;
- `SourceSpan`;
- public category;
- conceptual cause;
- any other frozen diagnostic metadata.

Any representative of the minimum equivalent class is conforming. Exact message prose and generic offending-value rendering are not ranking keys because they are not generally Architecture-frozen.

All Architecture-visible candidate fields are deterministic.

## No row or physical ordering

D25-S1 is a total preorder over observable candidate classes, not rows.

It introduces no:

- SQL row order;
- hidden execution order;
- row ordinal or row identifier;
- RID/page/slot ranking;
- scan-order precedence.

Even ordered relational input does not automatically create row-based error precedence.

The following never select or break a tie:

- vector lane or capacity;
- chunk boundary;
- FLAT, CONSTANT, or DICTIONARY representation;
- selection storage or dictionary child index;
- scan task/order;
- worker, thread, or SIMD completion order;
- pointer or allocation address;
- hash or filesystem order;
- physical expression or Project visitation order.

## Project, Filter, and DICTIONARY behavior

Distinct Project outputs are ranked by canonical source-derived semantic occurrence, not physical output visitation.

Multiple demanded Filter-row failures use the same provenance/cause preorder. Input row ordinal, scan order, and selection index have no role.

Repeated DICTIONARY occurrences remain repeated demanded occurrences. They are not semantically deduplicated. If all frozen candidate fields agree, their candidates occupy one observational equivalence class.

## Execution freedom

Implementations may:

- vectorize or use SIMD;
- evaluate demanded work in parallel;
- speculate over demanded occurrences;
- compute local candidate minima;
- merge minima in any discovery order;
- stop once the minimum class is provably unbeatable.

They need not:

- execute one row at a time;
- materialize every candidate;
- use one worker;
- evaluate physically in ranking order.

A speculative error from undemanded work is discarded. First physical discovery alone is not proof that a candidate is the semantic minimum.

Absent a different legitimate resource or cancellation event, vector width, chunk boundaries, representation, selection layout, SIMD completion, worker scheduling, and addresses cannot change the selected ordinary error class.

## Specialized-owner separation

- D21-S4 remains authoritative for DML.
- `RETURNING` remains DML and is excluded from D25-S1.
- Aggregate-finalization ranking remains §29.3-owned.
- Scalar-subquery cardinality and specialized subquery precedence remain §20.14-owned.
- `OutOfMemory`, `SpillIOError`, representability/resource `ExecutionError`, and other resource failures remain Chapter-24/§39-owned.
- `QueryCancelled` remains separately owned.
- Corruption and malformed internal expression state remain internal/specialized failures.

No universal precedence between ordinary semantic errors and resource, cancellation, or internal failures was introduced.

## Result-publication boundary

Section 31.10 now states the approved minimum boundary:

- a chunk already returned by a completed cursor operation is not retroactively retracted by a later ordinary expression error;
- the returned prefix does not make the failed query a successful complete query;
- whole-query buffering is not required;
- cursor protocol and returned-value lifetime are unchanged.

Chapter 31 remains the canonical result-publication owner. Section 31.10 required this concise clarification.

Section 20.17 also required one minimal owner-direction sentence: Chapter 20 owns demand/order/provenance preservation, while §25.1.1 owns non-DML occurrence-level candidate selection.

## Regression assessment

- Chapter 17: unchanged; scalar errors, casts, arithmetic, NULL, folding, and child order remain canonical.
- Chapter 18: unchanged; `SourceSpan` coordinates and ownership remain canonical.
- Chapter 19: unchanged; binding, output occurrence order, and cast provenance remain upstream.
- Chapter 20: semantics unchanged; only a minimal ownership reference was added.
- Chapter 21: unchanged; D21-S4 and D21-S5 remain authoritative.
- Chapter 22: unchanged; no `LogicalSlotId` or physical-position rule changed.
- Chapter 23: unchanged; vector/lane/chunk and repeated-DICTIONARY semantics remain intact.
- Chapter 24: unchanged; resource feasibility and failure remain separate.
- Persistence: unchanged.
- Transactions: unchanged; §39.1 remains authoritative.
- Public error taxonomy: unchanged.

## Reread answers 1–115

### Candidate domain

1. Yes.
2. Yes; undemanded CASE failures are excluded.
3. Yes; short-circuited AND RHS failures are excluded.
4. Yes; short-circuited OR RHS failures are excluded.
5. Yes; skipped IN work is excluded.
6. Yes; zero-row per-row evaluation is excluded.
7. Yes; resource failures are excluded.
8. Yes; cancellation is excluded.
9. Yes; internal invalid states are excluded.
10. Yes; DML is excluded.
11. Yes; aggregate specialization is excluded.
12. Yes; scalar-subquery specialization is excluded.

### Within one occurrence

13. Yes; Chapter 17 remains scalar-semantic owner.
14. Yes; D20-B1 is unchanged.
15. Yes; D20-B2 is unchanged.
16. Yes; child order is unchanged.
17. Yes; CASE demand is unchanged.
18. Yes; AND/OR demand is unchanged.
19. Yes; IN demand/order is unchanged.
20. Yes; D25-S1 compares already-established occurrence-level failures only.

### Provenance and ranking

21. Yes; responsible `SourceSpan` is retained.
22. Yes; semantic source-expression provenance is retained.
23. Yes; ranking first uses span start.
24. Yes; equal starts use shorter span.
25. Yes; exact-span ties use semantic source-expression occurrence order.
26. Yes; same-occurrence ties use the closed cause order.
27. Yes; the exact order is `INVALID_CAST`, `NUMERIC_OVERFLOW`, `DIVISION_BY_ZERO`, `INVALID_DATE`, `INVALID_TIMESTAMP`.
28. Yes; enum numeric values are excluded.
29. Yes; registry iteration is excluded.
30. Yes; physical expression visitation is excluded.
31. Yes; SourceSpan alone intentionally does not resolve every case.
32. Yes; same-expression/different-cause behavior is explained.

### Equivalence

33. Yes; semantic origin must agree.
34. Yes; `SourceSpan` must agree.
35. Yes; public category must agree.
36. Yes; conceptual cause must agree.
37. Yes; all other frozen metadata must agree.
38. Yes; exact message prose is excluded.
39. Yes; generic offending-value rendering is excluded.
40. Yes; any minimum-class representative is conforming.
41. Yes; every Architecture-visible field remains deterministic.

### Row and physical nonidentity

42. Yes; no SQL row order was introduced.
43. Yes; no hidden execution-row order was introduced.
44. Yes; no row identifier was introduced.
45. Yes; RID ranking is forbidden.
46. Yes; page/slot ranking is forbidden.
47. Yes; scan-order ranking is forbidden.
48. Yes; lane ranking is forbidden.
49. Yes; chunk ranking is forbidden.
50. Yes; dictionary child-index ranking is forbidden.
51. Yes; worker/thread ranking is forbidden.
52. Yes; SIMD completion ranking is forbidden.
53. Yes; pointer/address ranking is forbidden.
54. Yes; hash order is forbidden.
55. Yes; physical Project visitation is forbidden.

### Execution freedom

56. Yes; vectorization is preserved.
57. Yes; SIMD is preserved.
58. Yes; parallelism is preserved.
59. Yes; demanded speculation is preserved.
60. Yes; deterministic local/global reduction is permitted.
61. Yes; full candidate materialization is unnecessary.
62. Yes; early stop requires proof that the minimum is unbeatable.
63. Yes; scalar row-at-a-time execution is not required.
64. Yes; physical discovery order may differ from ranking order.
65. Yes; the semantic minimum remains invariant.

### Result publication

66. Yes; the existing cursor/result owner is preserved.
67. Yes; an already-returned chunk is not retracted.
68. Yes; the prefix does not imply successful completion.
69. Yes; no whole-query buffering is required.
70. Yes; no cursor redesign was introduced.
71. Yes; D25-S1 selects only the eventual ordinary error.
72. Yes; Chapter 31 remains canonical.

### Specialized owners

73. Yes; D21-S4 remains DML owner.
74. Yes; `RETURNING` remains DML.
75. Yes; §29.3 remains aggregate-specialized owner.
76. Yes; §20.14 remains subquery-specialized owner.
77. Yes; Chapter 24/§39 remain resource owners.
78. Yes; `QueryCancelled` is unchanged.
79. Yes; internal invalid state is excluded.
80. Yes; no public error category was added.

### Cross-chapter reread

81. Yes; Chapter 17 was task-unchanged.
82. Yes; Chapter 18 was task-unchanged.
83. Yes; Chapter 19 was task-unchanged.
84. Yes; Chapter 21 was task-unchanged.
85. Yes; Chapter 22 was task-unchanged.
86. Yes; Chapter 23 was task-unchanged.
87. Yes; Chapter 24 was task-unchanged.
88. Yes; Chapter 26 was task-unchanged.
89. Yes; Chapter 27 was task-unchanged.
90. Yes; Chapter 29 was task-unchanged.
91. Yes; §39 was task-unchanged.
92. Yes; §20 received only minimal owner synchronization.
93. Yes; §31 received only the approved publication clarification.
94. Yes; persistence is unchanged.
95. Yes; transaction semantics are unchanged.
96. Yes; `LogicalSlotId` is unchanged.
97. Yes; row identity is unchanged.
98. Yes; SQL ordering is unchanged.

### Finding and closure status

99. Yes; Q25-1 is CLOSED.
100. Yes; B25-1 is CLOSED.
101. Yes; D25-S1 is CLOSED.
102. Yes; frozen Chapter-25 semantic questions are NONE.
103. Yes; M25-1 remains open.
104. Yes; M25-2 remains open.
105. Yes; M25-3 remains open.
106. Yes; M25-4 remains open.
107. Yes; N25-1 remains open.
108. Yes; N25-2 remains open.
109. Yes; N25-3 remains open.
110. Yes; Chapter 25 is semantically clean.
111. No; Chapter 25 is not document/integration clean.
112. No; Verification is not synchronized by this task.
113. No; Chapter 25 is not fully closed.
114. No; Chapter 26 review was not started.
115. No; Phase 2 is not authorized.

## Final status

```text
D25-S1:
    CLOSED

Q25-1:
    CLOSED

B25-1:
    CLOSED

Frozen Chapter-25 semantic questions:
    NONE

M25-1 through M25-4:
    OPEN / NONSEMANTIC INTEGRATION

N25-1 through N25-3:
    OPEN / DOCUMENT-ONLY

Chapter 25:
    SEMANTICALLY CLEAN
    NOT YET DOCUMENT/INTEGRATION CLEAN
    NOT FULLY CLOSED

Chapter-25 Verification:
    NOT SYNCHRONIZED BY THIS TASK

Chapter 26:
    NOT STARTED
```

Next task: `TARGETED CHAPTER-25 DOCUMENT / INTEGRATION CLEANUP`.

## Diff classification

All expected hunk classes A–T are represented:

- A–C: candidate domain, demand, and upstream within-occurrence delegation
- D–G: provenance and four-level ranking
- H–J: equivalence, total preorder, and physical-key prohibition
- K–L: vector/parallel reduction and early-stop freedom
- M–O: DML, specialized owner, resource/cancellation/internal exclusions
- P/R: result-publication boundary and §31.10 synchronization
- Q: minimal §20.17 owner reference
- S: analytical rationale
- T: Markdown wrapping only

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) was task-modified. No build, test, sanitizer, benchmark, implementation, staging, commit, devlog, or review artifact was created. Phase 2 remains NOT STARTED / NOT AUTHORIZED.
