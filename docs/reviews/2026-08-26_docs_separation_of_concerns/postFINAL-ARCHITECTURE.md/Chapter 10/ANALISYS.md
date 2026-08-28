# Chapter 10 architecture review

## 1–9. Verdict, scope, and counts

1. **Verdict:** `CHAPTER 10 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED`.

2. **Primary scope read:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:7523), from `# 10. MVCC Visibility and Tuple-Version Semantics` through the line before Chapter 11.

3. **Actual heading inventory:**

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 10 | MVCC Visibility and Tuple-Version Semantics | Chapter-wide tuple-version visibility contract | ARCHITECTURE-APPROPRIATE, with semantic findings |
| 10.1 | Heap-version MVCC and rollback model | Physical version creation and logical abort behavior | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 10.1.a | Aborted INSERT | Aborted creator invisibility | ARCHITECTURE-APPROPRIATE |
| 10.1.b | Aborted DELETE | Aborted deleter ineffectiveness | ARCHITECTURE-APPROPRIATE |
| 10.1.c | Aborted UPDATE | Old/new version outcome after abort | ARCHITECTURE-APPROPRIATE |
| 10.2 | Creator visibility | Exact `xmin`/creator decision | ARCHITECTURE-APPROPRIATE, semantic finding |
| 10.3 | Deleter visibility | Exact `xmax`/deleter decision | ARCHITECTURE-APPROPRIATE, semantic finding |
| 10.3.1 | No deleter | `xmax=INVALID_TXN_ID` | ARCHITECTURE-APPROPRIATE |
| 10.3.2 | Deleted/superseded by the current transaction | Self-delete `cmax` rule | ARCHITECTURE-APPROPRIATE, semantic finding |
| 10.3.3 | Deleted/superseded by another transaction | Other-deleter status/snapshot rule | ARCHITECTURE-APPROPRIATE, semantic finding |
| 10.4 | Visibility evaluation order | Creator-then-deleter composition | ARCHITECTURE-APPROPRIATE, inherits status gap |
| 10.5 | MVCC hint cleanup | Aborted-`xmax` normalization and freezing | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 10.5.a | Aborted xmax | Physical normalization after abort | ARCHITECTURE-APPROPRIATE |
| 10.5.b | Frozen creator | Physical creator freezing | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 10.6 | MVCC invariants | Normative summary | ARCHITECTURE-APPROPRIATE, incomplete for findings |

4. **Context-only architecture consulted:**

- Front matter and contract language.
- §§4.3.2, 4.5–4.6, 4.13.3.
- §§5.2, 5.4, 5.7, 5.15–5.17.
- Chapter 7 latch, dirty-page, WAL-before-data, and guard boundaries.
- §§8.1, 8.22–8.23.
- Chapter 9 in full.
- §§11.1–11.7 and 11.10–11.12.
- §§12.10.4–12.10.5 and WAL publication context.
- §§13.12–13.19.
- §§14.6–14.14.
- §§15.1–15.7.
- Catalog, scan, parallel-execution, and DML target-spool owners where needed.
- §39.1 and §41 transaction/verification obligations.

5. **Other live docs consulted:** `docs/VERIFICATION.md`, `docs/PROJECT_STATE.md`, and `docs/DEVELOPMENT.md`, only for role and coverage classification.

6. **BLOCKING:** 0.

7. **MAJOR:** 2.

8. **MINOR:** 0.

9. **EDITORIAL:** 1.

## 10. Section-by-section review

Abbreviations: `OK` = clear/consistent, `F` = finding, `N/A` = owned elsewhere.

| Section | Role | Timeless | Ownership | Depth | Terms | Creator | Deleter | Self | Snapshot | Status | Chain | Index | Retry | Reclaim | Failure | Xref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 10 | Visibility contract | Yes | Correct | Generally strong | OK | F | F | F | OK | F | OK | OK | OK | OK | F | Note | F | FINDING |
| 10.1 | MVCC/rollback | Yes | Correct | Sufficient | OK | OK | OK | OK | N/A | OK | N/A | OK | N/A | OK | OK | F | OK | CLEAN WITH NOTE |
| Aborted INSERT | Abort creator | Yes | Correct | Sufficient | OK | OK | N/A | N/A | OK | OK | N/A | OK | N/A | OK | OK | — | OK | CLEAN |
| Aborted DELETE | Abort deleter | Yes | Correct | Sufficient | OK | N/A | OK | N/A | OK | OK | N/A | N/A | N/A | OK | OK | — | OK | CLEAN |
| Aborted UPDATE | Abort pair | Yes | Correct | Sufficient | OK | OK | OK | OK | OK | OK | N/A | OK | N/A | OK | OK | — | OK | CLEAN |
| 10.2 | Creator rule | Yes | Correct | Strong normal-path rationale | OK | F | N/A | F | OK | F | N/A | N/A | N/A | N/A | F | — | F | FINDING |
| 10.3 | Deleter rule | Yes | Correct | Strong normal paths | OK | N/A | F | F | OK | F | N/A | N/A | N/A | N/A | F | — | F | FINDING |
| 10.3.1 | No deleter | Yes | Correct | Sufficient | OK | N/A | OK | N/A | OK | N/A | N/A | N/A | N/A | N/A | OK | — | OK | CLEAN |
| 10.3.2 | Self deleter | Yes | Correct | Sufficient for valid values | OK | N/A | OK | F | OK | SELF | N/A | N/A | N/A | N/A | F | — | F | FINDING |
| 10.3.3 | Other deleter | Yes | Correct | Strong normal paths | OK | N/A | F | N/A | OK | F | N/A | N/A | N/A | N/A | F | Note | F | FINDING |
| 10.4 | Decision order | Yes | Correct | Sufficient | OK | F inherited | F inherited | OK | OK | F | N/A | N/A | N/A | N/A | F | — | F | FINDING |
| 10.5 | Hint cleanup | Yes | Correct | Sufficient | OK | OK | OK | N/A | N/A | OK | N/A | N/A | N/A | OK | OK | F | OK | CLEAN WITH NOTE |
| Aborted xmax | Normalize abort | Yes | Correct | Sufficient | OK | N/A | OK | N/A | N/A | OK | N/A | N/A | N/A | OK | OK | Note | OK | CLEAN |
| Frozen creator | Freeze creator | Yes | Correct | Sufficient | OK | OK | N/A | N/A | N/A | OK | N/A | N/A | N/A | OK | OK | F | OK | CLEAN WITH NOTE |
| 10.6 | Invariants | Yes | Correct | Concise | OK | F omission | F omission | F omission | OK | F omission | N/A | OK | N/A | OK | F | — | F | FINDING |

## 11–13. Ownership, inputs, and preconditions

11. **Ownership boundary:** Chapter 9 creates snapshots and resolves transaction status; Chapter 10 consumes tuple metadata, snapshot state, and status to decide ordinary SQL visibility. Chapter 11 owns write conflicts and uniqueness. Chapters 5/8/27 own physical candidate production; Chapter 14 owns reclamation.

12. **Visibility inputs:**

| Input | Owner | Chapter-10 use |
|---|---|---|
| `T.xmin` | Chapter 5 | Creator identity |
| `T.xmax` | Chapter 5 | No-delete/self/other deleter |
| `T.cmin` | Chapters 5/9 | Self-creator command comparison |
| `T.cmax` | Chapters 5/9 | Self-deleter command comparison |
| `S.xmax` | Chapter 9 | Too-new horizon |
| `S.active` | Chapter 9 | Nonterminal-at-capture classification |
| `S.owner_txn_id` | Chapter 9 | SELF bypass |
| `S.command_id` | Chapter 9 | Self-command boundary |
| `Status(xmin/xmax)` | Chapter 9 | Other-transaction outcome |
| `FROZEN_TXN_ID` | Chapters 4/9 | Creator-visible special case |
| `S.xmin` | Chapter 9 | Not an ordinary per-tuple visibility input; reclamation horizon input |
| `prev` RID | Chapter 5 | Not used by ordinary visibility |

13. **Tuple-header preconditions:** §4.13.3 validates the physical tuple before ordinary visibility: `xmin` is FROZEN or normal, `xmax` is INVALID or normal, FROZEN is forbidden as deleter, and the tuple codec/owner/page/RID are valid. Command fields are structurally uint32; correctness-significant future self-command relationships remain unresolved by Chapter 10.

## 14–24. Creator visibility

14. Creator evaluation is first and mechanically clear for valid runtime states.

15. **FROZEN creator:** Always creator-visible; no status-page access.

16. **SELF creator:** Uses `T.xmin == S.owner_txn_id`.

17. **`cmin`:** Valid normal rule is exactly `T.cmin < S.command_id`. Equality hides current-command writes. Greater-than-current is not separately rejected—finding M2.

18. **Committed before snapshot:** Visible when `xmin < S.xmax` and absent from `S.active`.

19. **Committed but active at snapshot:** Invisible permanently to that snapshot.

20. **Committed with `xmin >= S.xmax`:** Invisible as too new.

21. **Other IN_PROGRESS creator:** Invisible.

22. **ABORTED creator:** Invisible; physical bytes may remain.

23. **RETIRED creator:** Chapter 10 currently classifies it invisible through `status != COMMITTED`; §§9.13 and 14.14.3 require such a still-status-dependent tuple to be an invariant/corruption condition. Finding M1.

24. **INVALID/RESERVED creator:** Also collapsed to invisible, despite §9.11.1 requiring an invariant/corruption result after completed recovery for a status-dependent reference. Finding M1.

## 25–44. Deleters, updates, and version chains

25. Deleter evaluation occurs only after creator visibility succeeds.

26. **No-delete sentinel:** `xmax=INVALID_TXN_ID` means visible.

27. **SELF deleter:** Uses `xmax == owner_txn_id`.

28. **`cmax`:** `cmax < command_id` hides the tuple; equality retains it for the current statement. Greater-than-current is not rejected—finding M2.

29. **Committed delete before snapshot:** Old version invisible when `xmax < S.xmax` and absent from `S.active`.

30. **Committed delete after snapshot:** Old version remains visible when the deleter is too new or was active at capture.

31. **Other IN_PROGRESS deleter:** Old version visible; readers do not wait.

32. **ABORTED deleter:** Old version visible; physical `xmax` may remain.

33. **RETIRED deleter:** No branch is defined. Valid storage should have normalized an effective old `xmax` before retirement; encountering it while status-dependent requires an error. Finding M1.

34. **INVALID/RESERVED deleter:** No branch is defined. They must not be guessed terminal. Finding M1.

35. **Composition:** Creator failure dominates; otherwise the deleter decides whether the creator-visible physical version survives.

36. **Own operations:** Valid cases compose deterministically:

- Current-command insert: hidden from ordinary rescan.
- Current-command delete/update of old version: old remains visible.
- Later command: prior insert becomes visible; prior delete hides old version.
- Own update swaps old/new visibility at the next command boundary.

37. **Same-command self-visibility:** Clear for valid equality cases.

38. **Later-command self-visibility:** Clear through strict `<`.

39. **Retry identity:** Canonical pre-write RC retry uses the same CommandId and a fresh snapshot; because no persistent write has published, there are no abandoned self-created versions to reinterpret.

40. **Other-transaction UPDATE:** Correctly derives from creator/deleter rules.

41. **Old/new pair:** Exactly one version is visible for valid update states.

42. **Version-chain owner:** Ordinary visibility does not traverse `prev`; Chapter 5 owns the physical link and Chapter 14 owns splicing/reuse.

43. **Termination/corruption:** N/A to ordinary visibility. When a maintenance/verifier owner traverses the chain, §4.13 L2/L3 and Chapter 14 own domain checks, cycle detection, stale RID handling, and bounded progress.

44. **Reclamation dependency:** Visibility assumes that versions needed by legal snapshots remain physically available; Chapter 14 supplies that guarantee.

## 45–57. Scans and isolation

45. **Heap scan:** Each structurally valid NORMAL physical version is evaluated independently; only visible versions are emitted.

46. **Index scan:** Every B+ result is a physical RID candidate requiring heap fetch and Chapter-10 visibility.

47. **Stale index candidate:** Aborted/obsolete but valid candidates are rejected by visibility; dangling, reused, wrong-owner, non-NORMAL, or key-mismatched candidates are corruption under Chapters 8/14/27.

48. **Index-only scope:** Arbitrary index-only scans are outside v1 until covering/all-visible machinery exists.

49. **READ COMMITTED:** Chapter 9 owns fresh snapshots per statement attempt; Chapter 10 applies one snapshot consistently to all candidate versions.

50. **RC statement snapshot:** Commits after capture do not alter the running attempt; the next statement may see them.

51. **RC retry:** Allowed pre-write retry uses fresh snapshot, same logical CommandId, and complete re-evaluation.

52. **REPEATABLE READ:** Uses the first ordinary statement’s transaction snapshot across later statements.

53. **RR own writes:** Stable external membership does not suppress own later writes because owner/CommandId branches bypass ordinary snapshot membership.

54. **Phantoms/snapshot isolation:** Newly committed external rows remain invisible to the fixed RR snapshot; this is snapshot isolation, not serializable isolation. Write skew remains possible.

55. **Supported isolation scope:** READ COMMITTED and REPEATABLE READ; READ COMMITTED is default. Owner: §9.5.

56. **SERIALIZABLE:** Outside v1; SSI/predicate/key-range mechanisms are deferred by Chapter 9.

57. **READ UNCOMMITTED:** Not in the v1 supported set; no dirty-read fallback exists.

## 58–67. Status consumption

| Result | Creator interpretation | Deleter interpretation | Additional snapshot test | Status |
|---|---|---|---|---|
| FROZEN | Visible | Structurally forbidden as `xmax` | No | Clear |
| SELF | `cmin < command` | Invisible only if `cmax < command` | No | Clear for valid values |
| IN_PROGRESS | Invisible | Old version visible | No | Clear |
| COMMITTED | Apply horizon/active tests | Delete effective only in visible past | Yes | Clear |
| ABORTED | Invisible | Delete ineffective | No | Clear |
| RETIRED | Currently invisible | Undefined | Must instead be status-independent/error | FINDING |
| INVALID | Currently invisible | Undefined | Context determines invariant error | FINDING |
| RESERVED | Currently invisible | Undefined | Recognized nonterminal, not terminal | FINDING |
| Lookup I/O/corruption failure | Not stated | Not stated | None | FINDING |

58. The valid-state lookup algorithm correctly consumes Chapter 9’s precedence.

59–63. FROZEN, SELF, IN_PROGRESS, COMMITTED, and ABORTED normal cases are coherent.

64–67. RETIRED, INVALID, RESERVED, and lookup-failure paths are incomplete or contradictory with Chapters 9/14; finding M1.

## 68–73. CommandId and failed statements

68. **Comparison semantics:** `<` means earlier command; `==` means current command. `>` is not classified separately and is finding M2.

69. **Same-command INSERT:** Ordinary rescan does not see it.

70. **Same-command DELETE:** The old version remains visible to that same command.

71. **Same-command UPDATE:** Old visible, new invisible during the command; later command reverses them.

72. **Failed-statement visibility:** If no write published, no physical effect survives. If a write published, §39.1 forces transaction abort; aborted creator/deleter rules neutralize the partial logical effects.

73. **Canonical retry:** Same logical CommandId, fresh RC snapshot, and only before any persistent write.

## 74–90. Durability, recovery, ownership, and failures

74. **Aborted bytes:** Clear distinction between physical presence and logical invisibility.

75. **Committed vs data-flushed:** No data-page force is required; Chapter 10 does not contradict NO-FORCE.

76. **Recovery loser:** Inserted versions invisible; loser `xmax` ineffective.

77. **Recovered terminal state:** WAL/status reconciliation completes before READY; no pre-crash active cache is trusted.

78. **Read-only interaction:** Read-only transactions own ordinary snapshots but cannot create status-dependent tuple metadata because they create no persistent writes.

79. **Writer reads:** Same visibility function plus SELF rules; no separate writer algorithm.

80. **Locks vs visibility:** Visibility decides historical eligibility; Chapter 11 locks coordinate writes. Readers do not acquire tuple locks merely to read.

81. **Write conflicts:** Chapter 11, especially §§11.4–11.7.

82. **Uniqueness:** §11.10 uses a separate current-owner predicate; ordinary snapshot visibility is explicitly not the UNIQUE predicate.

83. **DML target recheck:** Chapters 11, 15, and 31 own post-lock refetch/revalidation.

84. **SQL snapshot vs RID epoch:** Distinct. Snapshot controls logical history; ReadEpochGuard controls physical RID reuse.

85. **Invisible vs reclaimable:** Clearly distinct. Invisibility to one snapshot does not authorize global reclamation.

86. **DEAD terminology:** `DEAD` is absent from Chapter 10. Chapter 5/14’s persisted slot state remains distinct from ordinary snapshot invisibility.

87. **Visibility hints/cache:** Only maintenance normalization is defined. It is physical, WAL-protected, and non-authoritative. No visibility-result cache is fixed.

88. **Scan consistency:** One statement attempt uses one snapshot for all heap/index/operator candidates.

89. **Parallel workers:** Chapter 32 requires the same immutable transaction/snapshot/CommandId view for all read workers.

90. **Failure taxonomy:**

| Condition | Visibility result | Required owner/result |
|---|---|---|
| Structurally invalid `xmin/xmax` | No visibility evaluation | `CORRUPT_HEAP`/format owner |
| Future self `cmin/cmax` | Currently misclassified | Frozen semantic question M2 |
| RETIRED/INVALID/RESERVED required status | Currently false/undefined | Frozen semantic question M1 |
| Status I/O/corruption | No guessed visibility | §39 storage/corruption result; Chapter-10 propagation missing |
| Broken predecessor/cycle | No ordinary traversal | L2/L3 verifier/vacuum error |
| MUST_ABORT/COMMITTING/ABORTING | No ordinary statement | §§9.4, 39.1 |
| RC write conflict | Re-evaluate or abort by write boundary | Chapters 11/15/39 |
| RR write conflict | Transaction-fatal serialization conflict | Chapters 11/15/39 |
| Unsupported tuple/page format | No visibility evaluation | Unsupported-format owner |

## 91–102. Documentation-model assessment

91. **Global model:**

- Analytical rather than chronological: Yes.
- Current-state narration: No.
- DEVELOPMENT sequencing: No.
- VERIFICATION procedure leakage: No.
- PROJECT_STATE leakage: No.
- Devlog/history leakage: No.
- Terminology ambiguity: No material synonym ambiguity; semantic status branches are incomplete.
- Rationale sufficient: Yes for valid self/snapshot/abort/reclamation paths; insufficient for impossible status/command paths.
- Readable without implementation status: Yes.
- Timeless canonical contract: Timeless, but not semantically complete enough to stand unchanged.

92. **Temporality:** No project chronology found.

93. **Temporal-language classification:**

| Phrase/category | Classification | Result |
|---|---|---|
| “current transaction/current statement” | A — runtime ordering | Valid |
| “after creator visibility succeeds” | A — semantic evaluation order | Valid |
| “currently has an in-progress xmax” | A — runtime state | Valid |
| “before snapshot” / “too new” | B — MVCC history | Valid |
| “may remain until vacuum” | A/B — runtime cleanup/history | Valid |
| “Later WAL/recovery chapters” | E — document navigation | Vague cross-reference |
| “later freeze rules” | E — document navigation | Vague cross-reference |
| “once WAL is active” | A — runtime persistence mode | Valid |
| “Version 1/v1” | D — durable v1 scope | Valid |

94. **Current-state leakage:** None.

95. **Document ownership:** All substantive Chapter-10 material is architecture-owned.

96. **Ownership classification:**

| Material | Classification |
|---|---|
| MVCC formulas and command comparisons | ARCHITECTURE |
| Abort logical/physical distinction | ARCHITECTURE |
| RETURNING mechanism suggestion | ARCHITECTURE integration guidance; detailed output procedure owned later |
| Conflict delegation | ARCHITECTURE navigation |
| Hint normalization | ARCHITECTURE |
| Test procedures | None |
| Implementation sequencing/status/history | None |

97. **Analytical depth:** Generally strong.

98. **Analytical-depth table:**

| Mechanism | Assessment |
|---|---|
| Heap-version rollback | ANALYTICALLY SUFFICIENT |
| Creator snapshot rule | ANALYTICALLY SUFFICIENT for valid states |
| Deleter snapshot rule | ANALYTICALLY SUFFICIENT for valid states |
| Self CommandId semantics | SEMANTICALLY CLEAR, but impossible-value handling missing |
| Aborted physical bytes | ANALYTICALLY SUFFICIENT |
| Committed vs snapshot-visible | ANALYTICALLY SUFFICIENT |
| Index/heap separation | Sufficient through canonical cross-chapter owners |
| RETIRED/INVALID/RESERVED | ANALYTICAL DEPTH/SEMANTIC COMPLETENESS FINDING |
| Reclamation distinction | ANALYTICALLY SUFFICIENT |
| Version chain | Correctly delegated; ordinary visibility does not traverse |

99. **Terminology:** Normal terms are precise; no overloaded “dead/live/current version” terminology appears.

100. **Normative language:**

| Text form | Role | Assessment |
|---|---|---|
| “The exact v1 rule is” | Normative algorithm | Strong |
| Declarative visibility outcomes | Normative | Strong |
| RETURNING “SHOULD use produced values” | Advisory mechanism | Acceptable; stronger external semantics owned by §§15.7, 21.15, 31.9 |
| Physical bytes/index entries “may remain” | Explicit permission | Correct |
| Maintenance “may rewrite” | Conditional physical optimization | Correct |
| Invariant list | Normative summary | Incomplete for M1/M2 |

101. **Source-layout coupling:** None.

102. **Implementation freedom:** Appropriate. The chapter fixes semantic branch order and outcomes, not containers, helper classes, source files, cache structures, or code layout.

## 103–120. Required semantic tables

### 103. Ownership boundary

| Input/service | Canonical owner | Consumer | Responsibility |
|---|---|---|---|
| Tuple format/structural validity | Chapters 4–5 | Chapter 10 | Valid physical metadata |
| Snapshot construction/lifetime | Chapter 9 | Chapter 10 | Stable visibility horizon |
| Status precedence | Chapter 9 | Chapter 10 | Transaction outcome lookup |
| Tuple visibility | Chapter 10 | Scans/DML/catalog | Visible/invisible decision |
| Physical RID candidates | Chapters 5/8/27 | Chapter 10 | Heap recheck |
| Write conflicts/uniqueness | Chapter 11 | DML | Coordination/current ownership |
| WAL/recovery | Chapters 12–13 | Status/visibility after READY | Durable terminal reconstruction |
| Reclamation/read epochs | Chapter 14 | Heap/index/vacuum | Physical lifetime |
| Retry/failure | Chapters 15/39 | Command/executor | Retry/abort/error result |

### 104. Visibility inputs

Covered in item 12; notably `snapshot.xmin` and `prev` are not ordinary tuple-visibility inputs.

### 105. Creator/xmin table

| Creator | Command/snapshot relation | Result |
|---|---|---|
| FROZEN | Any | Visible |
| SELF | `cmin < C` | Visible |
| SELF | `cmin == C` | Invisible |
| SELF | `cmin > C` | Currently invisible; finding M2 |
| COMMITTED | `<xmax`, absent active | Visible |
| COMMITTED | in active | Invisible |
| COMMITTED | `>=xmax` | Invisible |
| IN_PROGRESS other | Any | Invisible |
| ABORTED | Any | Invisible |
| RETIRED | Any | Currently invisible; finding M1 |
| INVALID/RESERVED | Any | Currently invisible; finding M1 |
| Status failure | Any | Undefined propagation; finding M1 |

### 106. Deleter/xmax table

| Deleter | Relation | Result |
|---|---|---|
| INVALID sentinel | No delete | Visible |
| SELF | `cmax < C` | Invisible |
| SELF | `cmax == C` | Visible |
| SELF | `cmax > C` | Currently visible; finding M2 |
| COMMITTED other | `<xmax`, absent active | Invisible |
| COMMITTED other | active or `>=xmax` | Visible |
| IN_PROGRESS other | Any | Visible |
| ABORTED other | Any | Visible |
| FROZEN | Structurally forbidden | Corruption before visibility |
| RETIRED/INVALID/RESERVED | Required status | Undefined; finding M1 |
| Status failure | Any | Undefined propagation; finding M1 |

### 107. Complete visibility rule

```text
validate physical tuple/RID
evaluate creator
    creator not visible -> tuple invisible
evaluate deleter
    no effective delete for S -> tuple visible
    effective committed/self-earlier delete -> tuple invisible
```

Impossible status and future-self-command branches are the only unresolved parts.

### 108. Self visibility

| Operation | Stored command | Current C | Old visible? | New visible? |
|---|---:|---:|---:|---:|
| INSERT current command | `cmin=C` | C | N/A | No |
| INSERT later statement | `cmin<C` | C | N/A | Yes |
| DELETE current command | `cmax=C` | C | Yes | N/A |
| DELETE later statement | `cmax<C` | C | No | N/A |
| UPDATE current command | old `cmax=C`; new `cmin=C` | C | Yes | No |
| UPDATE later statement | old/new command `<C` | C | No | Yes |
| RC pre-write retry | Same CommandId, no prior published version | C | Re-evaluate | Re-evaluate |

### 109. Update versions

| Updater state/relation | Old | New | Selected logical version |
|---|---:|---:|---|
| SELF current command | Visible | Invisible | Old |
| SELF later command | Invisible | Visible | New |
| IN_PROGRESS other | Visible | Invisible | Old |
| COMMITTED before snapshot | Invisible | Visible | New |
| COMMITTED after/active at snapshot | Visible | Invisible | Old |
| ABORTED | Visible | Invisible | Old |

### 110. Isolation

| Mode | Capture | Refresh | External later commits | Own later writes | Retry | V1 |
|---|---|---|---|---|---|---|
| READ COMMITTED | Each attempt | Next statement/retry | Visible only to later snapshot | Yes through SELF rules | Fresh snapshot, same CommandId pre-write | Supported/default |
| REPEATABLE READ | First ordinary statement | No membership refresh | Remain invisible | Yes through SELF rules | Fixed transaction snapshot; conflicts abort | Supported/snapshot isolation |
| SERIALIZABLE | N/A | N/A | N/A | N/A | N/A | Outside v1 |
| READ UNCOMMITTED | N/A | N/A | Dirty reads forbidden by supported model | N/A | N/A | Unsupported |

### 111. Status consumption

Covered in items 58–67; M1 prevents a complete error-capable table.

### 112. Version chain

| Situation | Candidate | `prev` action | Stop/error | Reclamation dependency |
|---|---|---|---|---|
| Ordinary heap scan | Current physical slot | None | Per-version visibility only | Snapshot horizon |
| Ordinary index scan | Exact referenced RID | None | Reject/return candidate | Read epoch |
| Vacuum | Selected version | May splice successor link | Defer/error if relationship unproved | Grace + splicing |
| Explicit verifier | Every chain | Traverse with visited set | Cycle/domain error | No reuse before proof |

### 113. Scan/recheck

| Source | Candidate | Visibility | Chain traversal | Snapshot | Output |
|---|---|---|---|---|---|
| Heap scan | NORMAL physical slot | Chapter 10 | No | One attempt snapshot | Visible tuple only |
| B+ equality | Exact candidate RID(s) | Heap recheck | No | One attempt snapshot | Visible matching versions |
| B+ range | Ordered candidate RIDs | Heap recheck | No | One attempt snapshot | Visible survivors in candidate order |
| Catalog scan | Ordinary catalog heap versions | Same MVCC | No | Caller’s catalog snapshot | Visible catalog rows |

### 114. Retry

| Isolation | Retry | Same CommandId | Fresh snapshot | Prior persistent effects | Owner |
|---|---|---:|---:|---|---|
| RC | Internal pre-write | Yes | Yes | Forbidden | §§9.9, 11.5, 15.7 |
| RC | After write | No same-TxnId retry | N/A | Transaction abort | §§15.7, 39.1 |
| RR | Write conflict | No internal statement refresh | No | Abort/serialization result | §§11.6, 39.1 |
| Client whole-request retry | New transaction | No | New transaction snapshot | Prior transaction terminal | §15.7 |

### 115. Failure matrix

| Failure | Visibility | Statement/transaction result | Owner |
|---|---|---|---|
| Invalid tuple TxnId domain | None | Corruption | §§4.13.3, 39.1 |
| Status I/O/corruption | Must not guess | Storage/corruption result; propagation missing in Ch.10 | M1 |
| RESERVED required context | Must not guess | Invariant/corruption expected | M1 |
| RETIRED without proof | Must not guess | Invariant/corruption expected | M1 |
| Future self command | Must not silently classify | Corruption/internal expected | M2 |
| Broken predecessor/cycle | Not traversed ordinarily | Verifier/vacuum error | Chapters 4/14 |
| MUST_ABORT | No ordinary read statement | Automatic abort | §§9.4, 39.1 |
| RC write conflict | Retry before write; otherwise abort | Retry/transaction failure | Chapters 11/15/39 |
| RR conflict | No newer-version following | Serialization abort | Chapters 11/39 |

### 116. Concurrency semantics

| Scenario | Ordering point | Visible result | Forbidden |
|---|---|---|---|
| Snapshot before writer commit | Capture sees writer active | Old visible/new invisible | Retroactive membership change |
| Snapshot after terminal commit | Terminal publication precedes capture | New visible/old deleted | Writer left active |
| Uncommitted INSERT | Creator nonterminal | Invisible | Dirty read |
| Uncommitted DELETE | Deleter nonterminal | Old visible | Physical `xmax` hiding row |
| Aborted UPDATE | Terminal ABORTED | Old visible/new invisible | New visible or old hidden |
| RC S1, commit, S2 | Per-statement capture | S1 old; S2 may see new | Mid-S1 refresh |
| RR, later commit | First-statement capture | Original version remains | Later commit entering visible past |
| Own update | Command boundary | Old during command, new later | Both or neither for valid pair |
| Index candidate/status change | Stable snapshot plus current status | Same historical result | Index hit deciding visibility |

### 117. Cross-chapter consistency

| Chapter | Result |
|---|---|
| 3 lifecycle/durable COMMIT | CONSISTENT |
| 4 domains/validation | CONSISTENT BUT SPECIALIZED; invalid status propagation missing |
| 5 tuple/version identity | CONSISTENT |
| 7 physical durability/lifetime | CONSISTENT |
| 8 candidate RID/recheck | CONSISTENT |
| 9 snapshots/status | FINDING: INVALID/RESERVED/RETIRED consumption |
| 11 conflicts/uniqueness | FINDING: future self-command metadata is error there but not here |
| 12 durability | CONSISTENT |
| 13 recovered terminal outcomes | CONSISTENT |
| 14 retirement/reclamation | FINDING: retired status-dependent tuples must not be silently invisible |
| 15 DML/retry | CONSISTENT BUT SPECIALIZED |
| 39 errors | FINDING: exact propagation absent in Chapter 10 |
| 41 verification | CONSISTENT, but follow-up gaps remain |

### 118. Explicit cross-references

| Source | Evidence | Intended owner | Exists/correct | Precision | Status |
|---|---|---|---|---|---|
| §10.1 | “Later WAL/recovery chapters…” | §12.10.4 and §§13.13–13.16 | Yes | Vague | EDITORIAL FINDING |
| §10.3.3 | “Write-conflict handling is owned by Chapter 11.” | §§11.4–11.7 | Yes/correct | Adequate boundary, broad | CLEAN WITH NOTE |
| §10.5 | “later freeze rules…” | §§14.13.1–14.14.3 | Yes | Vague | EDITORIAL FINDING |
| §10.5 | WAL/page-LSN rules once active | §§12.10–12.12 | Yes | Implicit | EDITORIAL FINDING |
| §10.5 | “defined in Chapter 14” | §§14.13–14.14 | Yes/correct | Broad | CLEAN WITH NOTE |

### 119. Terminology

| Term | Canonical meaning | Assessment |
|---|---|---|
| Visible | Passes creator and deleter rules for snapshot S | Precise |
| Committed | Terminal creator/deleter outcome | Not synonymous with visible |
| Current statement | `snapshot.command_id` equality boundary | Precise |
| Tuple version | Physical heap version with RID | Precise |
| Old/new version | UPDATE predecessor/replacement physical versions | Precise |
| Aborted version | Physical bytes whose creator/deleter status is aborted | Precise |
| Snapshot | Chapter-9 immutable horizon/active membership plus owner/command | Precise |
| Garbage/vacuumable | Physical cleanup candidate, not immediate reuse | Precise |
| DEAD | Not used in Chapter 10 | N/A |
| RETIRED | Not mentioned where required | Semantic finding, not synonym ambiguity |
| Candidate RID | Defined in Chapters 8/27 | Correct boundary |

### 120. Normative table

The exact algorithms and invariant statements are suitably normative for valid states. The defect is missing branches, not weak modal wording.

## 121. Technical consistency matrix

| # | Item | Result |
|---:|---|---|
| 1 | Ownership boundary | CONSISTENT |
| 2 | Visibility inputs | CONSISTENT |
| 3 | Tuple metadata validation | CONSISTENT BUT SPECIALIZED |
| 4 | FROZEN creator | CONSISTENT |
| 5 | SELF creator | CONSISTENT |
| 6 | Creator `cmin` | FINDING for future values |
| 7 | Committed creator before snapshot | CONSISTENT |
| 8 | Creator active at snapshot | CONSISTENT |
| 9 | Creator `>=xmax` | CONSISTENT |
| 10 | In-progress creator | CONSISTENT |
| 11 | Aborted creator | CONSISTENT |
| 12 | Retired creator | FINDING |
| 13 | `xmax` absent | CONSISTENT |
| 14 | SELF deleter | CONSISTENT |
| 15 | Deleter `cmax` | FINDING for future values |
| 16 | Committed deleter before snapshot | CONSISTENT |
| 17 | Committed deleter after snapshot | CONSISTENT |
| 18 | In-progress deleter | CONSISTENT |
| 19 | Aborted deleter | CONSISTENT |
| 20 | Retired deleter | FINDING |
| 21 | Insert+delete composition | CONSISTENT |
| 22 | Own insert | CONSISTENT |
| 23 | Own delete | CONSISTENT |
| 24 | Own update old version | CONSISTENT |
| 25 | Own update new version | CONSISTENT |
| 26 | Other updater in progress | CONSISTENT |
| 27 | Updater committed before | CONSISTENT |
| 28 | Updater committed after | CONSISTENT |
| 29 | Aborted update | CONSISTENT |
| 30 | Version-chain owner | CONSISTENT BUT SPECIALIZED |
| 31 | Version-chain termination | CONSISTENT BUT SPECIALIZED |
| 32 | Version corruption | CONSISTENT BUT SPECIALIZED |
| 33 | Heap scan | CONSISTENT BUT SPECIALIZED |
| 34 | Index recheck | CONSISTENT BUT SPECIALIZED |
| 35 | Stale index candidate | CONSISTENT BUT SPECIALIZED |
| 36 | RC semantics | CONSISTENT |
| 37 | RC retry | CONSISTENT BUT SPECIALIZED |
| 38 | RR semantics | CONSISTENT |
| 39 | RR own writes | CONSISTENT |
| 40 | Phantom semantics | CONSISTENT |
| 41 | Supported isolation set | CONSISTENT BUT SPECIALIZED |
| 42 | Snapshot-isolation terminology | CONSISTENT |
| 43 | SERIALIZABLE scope | CONSISTENT BUT SPECIALIZED |
| 44 | READ UNCOMMITTED scope | CONSISTENT BUT SPECIALIZED |
| 45 | TxnStatus consumption | FINDING |
| 46 | RETIRED | FINDING |
| 47 | INVALID | FINDING |
| 48 | RESERVED | FINDING |
| 49 | Status lookup failure | FINDING |
| 50 | Command comparison operators | FINDING for `>` |
| 51 | Same-command insert | CONSISTENT |
| 52 | Same-command delete | CONSISTENT |
| 53 | Same-command update | CONSISTENT |
| 54 | Failed statement | CONSISTENT BUT SPECIALIZED |
| 55 | Retry identity | CONSISTENT |
| 56 | Aborted bytes physical | CONSISTENT |
| 57 | Committed != flushed | CONSISTENT |
| 58 | Recovery loser | CONSISTENT |
| 59 | Read-only interaction | CONSISTENT |
| 60 | Locking boundary | CONSISTENT |
| 61 | Write-conflict owner | CONSISTENT BUT SPECIALIZED |
| 62 | Unique-conflict owner | CONSISTENT BUT SPECIALIZED |
| 63 | DML target recheck | CONSISTENT BUT SPECIALIZED |
| 64 | Reclamation boundary | CONSISTENT |
| 65 | Snapshot vs read epoch | CONSISTENT |
| 66 | Visibility hints | CONSISTENT |
| 67 | Scan snapshot consistency | CONSISTENT BUT SPECIALIZED |
| 68 | Parallel workers | CONSISTENT BUT SPECIALIZED |
| 69 | Error taxonomy | FINDING |
| 70 | Implementer invention | FINDING |

## 122. Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | CONSISTENT |
| 2 | Transaction-time language preserved | CONSISTENT |
| 3 | No implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No DEVELOPMENT sequencing | CONSISTENT |
| 6 | No VERIFICATION leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT |
| 9 | No source-layout coupling | CONSISTENT |
| 10 | Visible/committed distinction | CONSISTENT |
| 11 | Committed/snapshot-visible distinction | CONSISTENT |
| 12 | Invisible/reclaimable distinction | CONSISTENT |
| 13 | DEAD/MVCC-dead distinction | N/A |
| 14 | SELF/current-command terminology | CONSISTENT |
| 15 | `xmin/xmax` roles | CONSISTENT |
| 16 | Status ownership/reference precision | FINDING |
| 17 | Active-set rationale | CONSISTENT through Chapter 9 |
| 18 | Aborted physical-byte rationale | CONSISTENT |
| 19 | Index/reclamation rationale | CONSISTENT through canonical owners |
| 20 | Readable without implementation-status knowledge | CONSISTENT |

## 123–126. Findings

### 123. BLOCKING findings

None.

### 124. MAJOR findings

#### M1 — Incomplete/improper nonterminal and retired status consumption

- **Section:** §§10.2, 10.3.3, and inherited §10.4.
- **Evidence:** `if status != COMMITTED: creator_visible = false`; deleter logic defines only ABORTED, IN_PROGRESS, and COMMITTED.
- **Severity:** MAJOR.
- **Type:** STATUS CONSUMPTION.
- **Scope:** Cross-section and cross-chapter.
- **Arithmetic:** N/A.
- **Explanation:** Chapter 9 lookup can return RETIRED, INVALID, or RESERVED, and can fail. §9.11.1 says status-dependent INVALID/RESERVED references after recovery are invariant failure/corruption. §14.14.3 says valid MVCC metadata must not remain status-dependent below the retirement cutoff. Chapter 10 instead makes creator-side invalid states ordinary invisibility and leaves deleter-side behavior undefined.
- **Canonical comparison:** §§9.11.1, 9.13, 14.14.3, and §11.10.4’s explicit corruption handling.
- **Consequence:** Implementations may silently hide a tuple, retain it, or report an error. In corrupted/invariant-broken state, a formerly committed tuple could be silently suppressed instead of failing safely.
- **Owner:** Chapter 10 must define visibility consumption; Chapters 9/14 define the preconditions; §39 owns resulting failure class.
- **Future action:** **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**

#### M2 — Future/impossible self-command metadata is silently treated as a valid visibility case

- **Section:** §§10.2 and 10.3.2.
- **Evidence:** `creator_visible = (T.cmin < S.command_id)` and `if T.cmax < S.command_id ... else tuple remains visible`.
- **Severity:** MAJOR.
- **Type:** SELF VISIBILITY.
- **Scope:** Cross-section and cross-chapter.
- **Arithmetic:** Exact trichotomy is `<`, `==`, `>`; Chapter 10 defines only `<` versus “everything else.”
- **Explanation:** Equality has intentional current-command meaning. Greater-than-current is an impossible future-command relationship for ordinary transaction execution and must not silently share equality’s result. Related impossible self combinations such as self-created/self-deleted metadata with inconsistent command order are also not validated.
- **Canonical comparison:** §11.10.4 explicitly classifies future self `cmin/cmax` and impossible self-command states as `CORRUPTION_OR_INTERNAL_ERROR`.
- **Consequence:** Ordinary visibility may silently hide or expose a tuple carrying impossible metadata, while uniqueness reports an error for the same tuple.
- **Owner:** Chapter 10, with Chapter 9’s CommandId lifecycle and §39’s failure taxonomy.
- **Future action:** **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**

### 125. MINOR findings

None.

### 126. EDITORIAL findings

#### E1 — Vague durability/recovery/freeze navigation

- **Section:** §§10.1 and 10.5.
- **Evidence:** “Later WAL/recovery chapters…”, “later freeze rules…”, and broad “defined in Chapter 14.”
- **Severity:** EDITORIAL.
- **Type:** CROSS-REFERENCE.
- **Scope:** Cross-section.
- **Arithmetic:** N/A.
- **Explanation:** The referenced owners exist, but the navigation is broad and chronology-shaped.
- **Canonical comparison:** §12.10.4 and §§13.13–13.16 own recovery behavior; §§14.13.1–14.14.3 own normalization/freezing/status retirement; §§12.10–12.12 own WAL/page-LSN mutation.
- **Consequence:** No semantic defect; readers must search broad chapters.
- **Owner:** ARCHITECTURE navigation.
- **Future action:** **G. CROSS-REFERENCE FIX.**

## 127–129. Semantic questions, verification gaps, and out-of-scope observations

127. **Frozen architecture semantic questions:**

1. What exact visibility/error result is required when ordinary creator/deleter evaluation encounters RETIRED, INVALID, RESERVED, or a failed status lookup?
2. Must `cmin/cmax > snapshot.command_id`, and inconsistent self-created/self-deleted command ordering, return corruption/internal error exactly as §11.10.4 does?

128. **Follow-up verification gaps:**

- Add Chapter-10 status-consumption cases for RETIRED, INVALID, RESERVED, and lookup failure after M1 is resolved.
- Add future self `cmin/cmax` and impossible self-command combinations after M2 is resolved.
- Add an explicit old/new UPDATE pair matrix across SELF, IN_PROGRESS, committed-before, committed-after, and ABORTED states.
- Existing heap/index recheck, normal xmin/xmax cases, RC/RR, recovery losers, and reclamation tests remain valid.

129. **Out-of-scope observations:**

- §11.12 says “The initial LockManager MAY use `hash map<LockKey, LockQueue>`”; implementation-stage/mechanism wording belongs to the Chapter-11 review.
- §15.7 contains “future explicit opt-in retry policy” and “Future statement savepoints/subtransactions…” roadmap wording; Chapter 15 owns its direct review.
- §27.5 uses “initial implementation”/“future implementation” for RID batching; the execution-chapter review owns it.

## 130–156. Direct decisions and next action

130. Creator/xmin ambiguity? **Yes**, only for RETIRED/INVALID/RESERVED/failure and future self command values.

131. Deleter/xmax ambiguity? **Yes**, only for missing status/error branches and future `cmax`.

132. `cmin/cmax` ambiguity? **Yes** for `>` and impossible combinations; `<` and `==` are exact.

133. Same-command self-visibility ambiguity? **No** for valid equality cases.

134. Committed-vs-snapshot-visible contradiction? **No**.

135. Aborted-delete contradiction? **No**.

136. Update old/new ambiguity? **No** for valid states.

137. Version-chain ambiguity? **No**; ordinary visibility does not traverse chains.

138. RC snapshot ambiguity? **No**.

139. RR snapshot ambiguity? **No**.

140. TxnStatus-consumption ambiguity? **Yes**.

141. RETIRED handling ambiguity? **Yes**.

142. Index-recheck gap? **No architecture gap**.

143. Visibility/reclamation conflation? **No**.

144. Correctness-relevant implementer invention required? **Yes**, for M1 and M2.

145. Project-time/current-state wording? **No category-F wording**.

146. DEVELOPMENT-owned material? **No**.

147. VERIFICATION-owned procedure leakage? **No**.

148. PROJECT_STATE-owned material? **No**.

149. Devlog/history material? **No**.

150. Ambiguous terminology? **No material terminology ambiguity**; the defects are missing semantic branches.

151. Analytically underexplained boundary? **Yes**, impossible status/self-command handling.

152. Timeless canonical v1 contract? **Timeless, but not complete enough to remain canonical without frozen review.**

153. **Previous-chapter regression:** Normal valid-state semantics preserve Chapters 3–9. M1 conflicts with Chapter 9/14 error preconditions; M2 conflicts with Chapter 11’s current-owner validation.

154. **Chapter-9 compatibility:** Snapshot horizons, active membership, SELF, CommandId equality, status precedence, and terminal publication are consumed correctly for legal states. INVALID/RESERVED/RETIRED handling is not.

155. **Recommended next action:** `frozen semantic review required`.

156. **Recommended Chapter-11 review scope:** Exact tuple-write lock identity and revalidation; lock/latch separation; READ COMMITTED retry versus post-write abort; REPEATABLE READ first-updater-wins; current-state uniqueness versus snapshot visibility; same-command uniqueness; terminal lock lifetime; lock-table implementation coupling; unified wait-for graph/deadlock behavior; current-owner status/error completeness.

## 157–164. Read-only and Git confirmation

157. **Files modified by audit:** NONE.

158. **Initial Git state:** working tree clean; index clean; HEAD `df5725b5627714b8bf0ae410098e0c7522ccda14`.

159. **Final Git state:** working tree clean; index clean; HEAD unchanged at `df5725b5627714b8bf0ae410098e0c7522ccda14`.

160. **`git diff --check`:** Passed with no output.

161. **Repository-state change:** None.

162. **Audit-created changes:** None.

163. **Implementation work:** None; no builds, tests, benchmarks, staging, commits, or artifacts.

164. **Phase 2:** remains `NOT STARTED / NOT AUTHORIZED`.