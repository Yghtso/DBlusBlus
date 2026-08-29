## Chapter 14 verification synchronization

1. **Verdict:** **CHAPTER 14 — FULLY REVIEWED AND CLOSED.** The final methodology is integrated in [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:5861).

2. Initial Git state:

   - Working tree: clean
   - Index: clean
   - HEAD: `2e136f867538000152a7e88ce2c5b82ed86b26a7`

3. `docs/ARCHITECTURE.md` had no pre-existing diff and remained byte-for-byte task-unchanged.

4. No review artifact appeared in Git status; none was read, modified, or staged.

## Organization and inventory

5. The former three-subsection reclamation section was expanded with:

   - deterministic harness and independent oracles;
   - protection-domain matrix;
   - write-status registration/lifetime races;
   - StatusHistoryGuard and negative status lookup;
   - freezing, normalization, and status independence;
   - cutoff durability, control publication, and second-crash retention;
   - read epochs and `TUPLE_WRITE` claim lifetime;
   - complete RID-reuse barriers;
   - status-page sparse reclamation;
   - FSM, lifecycle, scheduling, and failures;
   - high-level case matrix;
   - atomic architecture-obligation map.

6. The organization remains mechanism-oriented under “Vacuum and Reclamation Tests.” Existing Chapter 5–13 verification owners are referenced rather than duplicated.

7. Complete atomic inventory:

| Domain | Rows | Count |
|---|---:|---:|
| A. Status-dependency model | 1–4 | 4 |
| B. Write-status registration | 5–9 | 5 |
| C. Write-status lifetime/release | 10–14 | 5 |
| D. SQL snapshot horizon | 15–18 | 4 |
| E. StatusHistoryGuard | 19–22 | 4 |
| F. Status lookup/RETIRED | 23–27 | 5 |
| G. Creator freezing | 28–33 | 6 |
| H. Aborted-xmax normalization | 34–39 | 6 |
| I. Deadness/reclaimability | 40–44 | 5 |
| J. Status-independence proof | 45–50 | 6 |
| K. Cutoff domain/alignment | 51–55 | 5 |
| L. Cutoff coordination | 56–61 | 6 |
| M. Prerequisite durability | 62–68 | 7 |
| N. Control publication | 69–74 | 6 |
| O. WAL retention/second crash | 75–81 | 7 |
| P. TXN_STATUS reclamation | 82–87 | 6 |
| Q. Sparse deallocation | 88–92 | 5 |
| R. Read-epoch registration | 93–96 | 4 |
| S. Read-epoch grace | 97–101 | 5 |
| T. TUPLE_WRITE claims | 102–106 | 5 |
| U. Epoch-to-lock handoff | 107–110 | 4 |
| V. Claim cancellation/release | 111–116 | 6 |
| W. Index cleanup | 117–121 | 5 |
| X. `prev_RID` cleanup | 122–125 | 4 |
| Y. DEAD-to-UNUSED/reuse | 126–134 | 9 |
| Z. Crash/restart | 135–140 | 6 |
| AA. BufferPool/frame coordination | 141–144 | 4 |
| AB. FSM/free space | 145–148 | 4 |
| AC. Shutdown/lifetime | 149–153 | 5 |
| AD. Failure/exhaustion | 154–160 | 7 |
| AE. Separation/other | 161–164 | 4 |

8. Actual atomic obligation count: **164**.

## Harness and independent oracles

9. The deterministic harness defines semantic barriers for transaction registration, guards, status lookup, freeze/normalization authorization, WAL durability, cutoff publication, epoch registration, lock claims, reuse, crash, recovery, and READY.

10. Visibility is independently evaluated from §§10.2–10.4 snapshot/status/header rules.

11. Status mapping independently computes `ordinal=T-2`, PageNo, entry, byte, and bit position using checked widened arithmetic.

12. Cutoff alignment independently enumerates whole 32,640-entry status pages and chooses the greatest legal exclusive boundary.

13. Epoch grace independently evaluates the exact predicate: no active `E <= retire_epoch`.

14. RID reuse uses an explicit conjunction of every barrier, never a production “reusable” helper as its own oracle.

15. WAL authority derives from the exact persisted complete-record prefix, `durable_lsn`, DPT `rec_lsn`, and retained records.

16. Control selection independently decodes and CRC-validates both slots and selects the greatest valid usable generation.

## Write-status and status-history methodology

17. Write-capable registration is paused before/after dependency visibility and before transaction admission.

18. Idle RC coverage removes the statement snapshot while retaining the transaction’s own status dependency.

19. Explicitly read-only transactions omit the dependency, may retain snapshots independently, and reject persistent writes.

20. Transaction-first race clamps the greatest legal aligned cutoff below the registered TxnId.

21. Reclaimer-first race requires the later TxnId to be `>=C` and registered before admission.

22. Assignment/registration race forces both legal orders and rejects a `T<C` unpinned admission state.

23. C3/C4/C5 and MUST_ABORT/A2/A3 probes verify dependency release only during C5/A3.

24. Existing tuple/catalog dependencies remain cutoff blockers after runtime dependency release.

25. StatusHistoryGuard coverage includes guard-first, cutoff-first, multiple guards, and exact `G` boundaries.

26. Required `RETIRED` creator and deleter outcomes independently produce the reclamation-invariant failure path.

27. Required `INVALID` and `RESERVED` outcomes are rejected without guessed status.

28. Status I/O, checksum, corruption, and unsupported-format failures authorize no maintenance rewrite.

## Freezing and normalization

29. Positive creator freezing uses an eligible committed creator.

30. Pre/post visibility equivalence is checked with the independent visibility oracle.

31. Just-eligible, equality-boundary, and too-new creator cases establish the exact horizon relation.

32. ABORTED, IN_PROGRESS, required-RETIRED, INVALID, RESERVED, and lookup-failure creators cannot freeze.

33. Positive aborted-xmax normalization verifies the canonical field rewrite.

34. Independent visibility evaluation proves normalization equivalence.

35. COMMITTED, IN_PROGRESS, unsafe SELF, required-RETIRED, INVALID, RESERVED, and lookup-failure xmax cases remain unchanged.

## Cutoff durability and crash recovery

36. Volatile freeze with undurable WAL cannot publish the cutoff.

37. Volatile normalization with undurable WAL cannot publish the cutoff.

38. A durably stored status-independent page is accepted as sufficient proof.

39. Durable retained reconstructive WAL is accepted while the old data-page image remains on disk.

40. Multiple WAL-dependent mutations derive the maximum required authorizing record-start LSN independently.

41. Control-file fdatasync and WAL fdatasync are separate fault domains.

42. Failed/torn control publication leaves the older valid cutoff authoritative while durable maintenance progress remains valid.

43. New cutoff plus dirty old disk page is accepted only with retained reconstructive WAL and redo before READY.

44. New cutoff plus missing required WAL is explicitly forbidden; recycling must refuse to cross `rec_lsn`.

45. Second-crash coverage keeps the recovered dirty page’s reconstruction WAL until its dependency clears.

46. DPT `rec_lsn` survives cutoff publication and recovery while needed.

47. Dirty TXN_STATUS `F/T` coverage preserves `F=rec_lsn` until the page is durable or safely retired.

48. Checkpoint-before-cutoff, checkpoint-after-cutoff, both concurrent linearizations, and second-crash composition are covered. Neither checkpoint nor page force is mandatory.

## Read epochs and TUPLE_WRITE claims

49. Epoch registration versus reuse is forced in both legal orders.

50. Grace fixtures use active epochs `R-1`, `R`, and `R+1`.

51. A long-lived reader may force wait/defer but never reuse.

52. Last-reader exit permits only revalidation and continuation; it does not bypass other barriers.

53. Epoch exhaustion at `UINT64_MAX` verifies failure/no-wrap and no stale-reader alias.

54. Page latch, BufferPool pin, and read epoch receive separate fixtures.

55. Epoch-to-request handoff pauses while the discovery epoch remains active.

56. Immediate grant must exist before epoch release.

57. Queued-waiter registration must become a live claim before epoch release.

58. Multiple waiters all count, regardless of queue position.

59. Commit owner remains a claim through C4 and releases at C5.

60. Abort owner remains a claim through A2 and releases at A3.

61. Queued deadlock victims retain claims until canonical no-late-grant removal; granted victims retain through A3.

62. Cancellation/grant races force both legal winners and forbid a disappeared claim followed by grant.

63. Request registration versus reclaimer zero-claim/reuse publication is gap-free.

64. Registration resource failure leaves the discovery epoch held.

65. Post-wait revalidation remains mandatory despite stable RID identity.

## RID reuse methodology

66. The positive oracle requires global deadness, index absence, persistent DEAD, predecessor absence, epoch grace, zero lock claims, frame/page eligibility, canonical UNUSED, and free-list publication.

67. NORMAL, DEAD, and UNUSED receive direct persistent-state and transition fixtures.

68. One remaining index is isolated as the only missing proof.

69. One remaining `prev_RID` is isolated as the only missing proof.

70. One old queued/granted lock claim is isolated as the only missing proof.

71. Fully legal same-slot reuse is covered only after every barrier clears.

72. Crash with a waiter verifies no lock replay and persistent DEAD retention.

73. Persistent DEAD restart requires fresh retirement/grace before reuse.

74. Legal UNUSED restart preserves canonical free-list semantics.

75. Post-reuse crash cannot resurrect any pre-reuse runtime identity.

## Status versus physical reclamation

76. Freezing removes creator-status dependency but does not mark DEAD or satisfy physical barriers.

77. Aborted-xmax normalization removes deleter dependency and does not authorize DEAD.

78. Status-history retirement and physical RID reuse are verified as independent axes.

79. Long RR snapshots block logical deadness through the snapshot horizon.

80. Long epochs may allow status retirement but block physical reuse.

81. Surviving committed normal xmax remains status-dependent.

82. Surviving old normal xmin remains status-dependent.

83. Crash-recoverable FROZEN state permits positive creator-status retirement.

84. Crash-recoverable normalized xmax permits positive deleter-status retirement.

85. Fully removed versions contribute no remaining tuple-status dependency.

## Status-page reclamation

86. `T<C` and `T==C` establish exclusive cutoff behavior.

87. Whole-page arithmetic independently verifies alignment and maximum-domain behavior.

88. A page wholly below C is eligible only after every control/guard/frame/DPT condition.

89. A boundary page containing any TxnId at or above C is ineligible.

90. Interior sparse reclamation preserves absolute PageNos and logical file length.

91. Sparse methodology uses abstract supported and unavailable/failing capabilities, not Linux APIs.

92. Missing backing below C returns `RETIRED` without page access.

93. Missing required backing at/above C follows the status I/O/corruption owner.

94. Unavailable sparse deallocation retains safe extra storage.

95. Failure after durable C leaves C authoritative and storage allocated.

96. Reclamation before durable cutoff publication is directly forbidden.

97. Torn control selects the old valid cutoff without premature page reclamation.

98. Durable cutoff with status page still present treats stale bytes as harmless.

99. Durable cutoff with page reclaimed accepts absence below C.

100. Crash during sparse deallocation preserves logical addressing and relies on already-authoritative C.

101. BufferPool pins/I/O/frame retirement use the Chapter-7 drain methodology.

102. StatusHistoryGuard prevents invalidating a guarded status lookup.

## Checkpoint, FSM, and failures

103. Checkpoint/status-page-retirement races force both DPT-capture orders.

104. Checkpoint/freeze races preserve either the old dependency or new dirty interval.

105. Checkpoint/xmax-normalization races preserve reconstructive WAL.

106. Cutoff/checkpoint/WAL-recycling/second-crash composition is explicit.

107. FSM remains advisory; stale-high requires heap recheck and stale-low may miss space.

108. FSM update failure leaves heap reclamation authoritative and FSM rebuildable.

109. Index cleanup failure blocks DEAD/reuse according to the authorized prefix.

110. `prev_RID` splice failure blocks reuse.

111. Corrupt tuple/page fixtures authorize no freeze, normalization, DEAD, or reuse mutation.

112. Pre-authorization WAL failure leaves the old canonical state.

113. Post-authorization failure retains authoritative progress under §12.12.

114. WAL uncertainty follows the existing noncontinuable/no-guessing owner.

115. Control failure leaves old authority and retains valid prerequisite progress.

116. Vacuum workspace and claim-registration resource exhaustion cannot create unsafe partial reclamation.

117. READY/DRAINING/CLOSING/RECOVERING/NONCONTINUABLE admission and quiescence are mapped to lifecycle verification.

## Mandatory matrices

118. Write-status race/lifetime matrix: present.

119. Status-independence matrix: present.

120. Cutoff prerequisite durability matrix: present.

121. Read-epoch/reuse matrix: present.

122. `TUPLE_WRITE`/reuse matrix: present.

123. Complete one-missing-barrier RID-reuse matrix: present.

124. Cutoff/control/status-page crash matrix: present.

125. Protection-domain matrix: present.

126. Failure/result matrix: present.

127. Checkpoint/retention matrix: present.

128. High-level reclamation domain/case matrix: present.

129. Negative fixtures isolate one missing proof so failures remain diagnostic.

130. Core races require deterministic barriers; sleeps and probabilistic repetition are rejected as correctness oracles.

131. Lock table, epoch registry, coordinator, worker, checkpoint cadence, and scheduling implementation remain unconstrained.

132. Sparse verification is platform-neutral and requires no syscall or filesystem-specific API.

## Coverage and status

133. The complete atomic coverage table is in [Chapter 14 atomic architecture-obligation coverage map](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6346), with architecture owner, verification procedure, and status for every row.

134. COMPLETE: **164**.

135. PARTIAL: **0**.

136. MISSING: **0**.

137. CONTRADICTORY: **0**.

138. Final 150-question re-read:

| Questions | Answer |
|---|---|
| 1–12, write-status dependency | YES |
| 13–20, guard/RETIRED/lookup | YES |
| 21–30, freezing/normalization | YES |
| 31–50, cutoff durability | YES |
| 51–58, read epochs | YES |
| 59–73, TUPLE_WRITE retention | YES |
| 74–88, RID-reuse barriers/crash | YES |
| 89–98, status versus RID reclamation | YES |
| 99–115, status-page reclamation | YES |
| 116–124, checkpoint/WAL/crash | YES |
| 125–138, FSM/failure/lifecycle | YES |
| 139–148, documentation methodology | YES |
| 149, new semantic rule invented? | **NO** |
| 150, Chapter-14 verification complete? | **YES** |

No requested item was genuinely N/A.

139. Documentation-model assessment:

| Assessment | Result |
|---|---|
| Current-state leakage | None |
| DEVELOPMENT sequencing | None |
| History/review chronology | None |
| Architecture duplication | Avoided through precise references |
| Time independence | Preserved |
| Procedural/analytical quality | Complete |
| Implementation independence | Preserved |
| Platform independence | Preserved |
| Runtime/MVCC temporal language | Preserved |
| Separation of concerns | Preserved |

140. Cross-reference validation covers Chapters 5, 7, 8, 9, 10, 11, 12, 13, 14; DML handoff in §§15.3–15.6; failure ownership in §39.1; and the governing §41 verification obligations.

141. Existing transaction-status, visibility, WAL, control/checkpoint, BufferPool, B+, locking/deadlock, and FSM methodologies were not deleted or weakened. The previous reclamation material was retained as a procedural superset.

142. Frozen architecture semantic question status: **NONE**.

143. All six required verification families: **CLOSED**.

144. Chapter 14: **FULLY REVIEWED AND CLOSED**.

145. Chapter-15 direct review: **NOT STARTED**.

146. Protected architecture wording remained unchanged, including §§15.7.2–15.7.3, §31.7, and Appendix C.

## Repository state

147. Files changed:

   - [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:5861)

148. Task-created hunk classifications:

| Class | Content |
|---|---|
| A | Write-status registration/races |
| B | Transaction dependency lifetime |
| C | Guard/RETIRED |
| D | Creator freezing |
| E | Xmax normalization |
| F | Status-independence matrices |
| G | Cutoff domain/alignment |
| H | WAL durability/control publication |
| I | Second-crash retention |
| J | Read-epoch registration/grace |
| K | TUPLE_WRITE retention |
| L | Epoch/lock handoff and cancellation |
| M | RID-reuse barriers |
| N | Index and `prev_RID` cleanup |
| O | DEAD/UNUSED/restart |
| P | Sparse status-page reclamation |
| Q | Checkpoint/control/cutoff crashes |
| R | BufferPool/frame/guard mapping |
| S | FSM/error/lifecycle |
| T | Mandatory matrices |
| U | Atomic coverage map |
| V | Precise references/navigation |
| W | Local wrapping only |

149. Only `docs/VERIFICATION.md` was task-modified.

150. Final Git state:

   - Working tree: `M docs/VERIFICATION.md`
   - Index: clean
   - HEAD unchanged: `2e136f867538000152a7e88ce2c5b82ed86b26a7`

151. `git diff --check`: **PASS**, no output.

152. No external repository change was detected during the task.

153. `docs/ARCHITECTURE.md` remained unmodified with no diff.

154. No review artifact was modified or staged.

155. No source, test, build, benchmark, implementation, staging, commit, devlog, or generated artifact work occurred.

156. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.
