Pass-D minor-fix verdict: **PASS with a follow-up verification gap identified.** All three authorized architecture-document findings are resolved.

### Git state

1. **Verdict:** D-m1 resolved; D-m2 document ownership resolved; D-m3 resolved.
2. **Initial Git state:** working tree clean; index empty; HEAD `249b379ae3ce52a6160a4d0c6abe8b985e0b8af1`.

### D-m1 — timeless heap geometry

3. **Original:** “heap geometry currently limits an allocated slot to `1017`.”
4. **Geometry inputs:** page size `8192`; total heap header `48` bytes (`32` common + `16` heap-specific, including its reserved field); slot entry `8` bytes; no additional reserved tail bytes. The minimum v1 tuple is `48` bytes but is not part of the structural slot-directory limit because canonical `UNUSED` and reclaimed `DEAD` slots have zero tuple coordinates.
5. **Arithmetic:** `(8192 − 48) / 8 = 8144 / 8 = 1018` slots. Zero-based SlotIds are `0..1017`.
6. **Corrected:** “the v1 heap-page geometry limits an allocated slot to `1017`.” See [ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:858).
7. **Semantic geometry:** unchanged; `1017`, page size, headers, slot size, and persisted format remain unchanged.
8. **Status:** **D-m1 RESOLVED**.

### D-m2 — architecture/verification ownership

9. **Original §4.3.2.6 classification:**

   - Numeric-exhaustion versus `RESOURCE_FULL` statement and definitions: **A — Architecture requirement**.
   - Exhaustion error domains and diagnostic classification: **A**.
   - Statement/transaction lifecycle consequences: **A**.
   - Operations permitted after domain exhaustion and open/recovery behavior: **A**.
   - Crash, high-water, gap, and recovery rules: **A**.
   - “Implementations and boundary verification MUST forbid…”: **E — Mixed**.
   - Each of the nine prohibition items: **A**.
   - Synthetic below/at/above-boundary injection sentence: **C — Verification procedure**.
   - Required crash/restart, consumed-gap, maximum-construction, and pre-publication cases: **C**.
   - “verification obligation, not a prescribed allocator API”: **B — Architecture rationale**.
   - No pinned compatibility vector (**D**) occurs in this subsection.

10. **Requirements retained:** deterministic exhaustion, no wrap/reuse/truncation, persistent high-water/gap state, checked construction, terminal headroom, and rejection before publication.
11. **Rationale retained:** numeric exhaustion versus resource failure, lifecycle scope, continued legal operations, crash persistence, and recovery behavior.
12. **Procedure removed/reduced:** synthetic allocator/counter setup immediately below, at, and above every boundary and its prescribed case list.
13. **Existing VERIFICATION ownership:** partial coverage exists in [Storage Verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:90), [Non-Crash WAL/MTR Failure Injection](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:388), [PAGE_INIT failure](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:466), [CommandId tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1032), and [catalog identity tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1788).
14. **Verification gap:** those sections do not provide one comprehensive all-domain exhaustion-boundary/fault-injection procedure matching the removed detail.
15. **Corrected architecture wording:** explicit `MUST` requirements now cover deterministic boundaries, restart-persistent high-water/gap state, checked maximum-size construction, and rejection before publication, followed by: “Detailed verification and fault-injection procedures for these requirements belong in `VERIFICATION.md`.” See [ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1144).
16. **Normative strength:** unchanged; the mixed heading became `Implementations MUST forbid`, and the retained outcomes use explicit `MUST`.
17. **Status:** **D-m2 DOCUMENT OWNERSHIP RESOLVED — FOLLOW-UP VERIFICATION GAP IDENTIFIED**. No verification obligation was deleted.

### D-m3 — system-relation file cardinality

18. **Original:** “all six system-relation heap/FSM files.”
19. **Canonical relations:** six—`sys_tables`, `sys_columns`, `sys_indexes`, `sys_index_columns`, `sys_constraints`, and `sys_statistics`.
20. **Canonical files:**

| System relation | Heap | FSM | Contribution |
|---|---:|---:|---:|
| `sys_tables` | 1 | 1 | 2 |
| `sys_columns` | 1 | 1 | 2 |
| `sys_indexes` | 1 | 1 | 2 |
| `sys_index_columns` | 1 | 1 | 2 |
| `sys_constraints` | 1 | 1 | 2 |
| `sys_statistics` | 1 | 1 | 2 |
| **Total** | **6** | **6** | **12 files** |

21. **Ambiguity:** the original could mean six files total or six relations each owning a heap/FSM pair.
22. **Corrected:** “the heap and FSM files for all six system relations.” See [ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:1588).
23. **Bootstrap protocol:** file set, initialization order, synchronization, validation, ownership, rename, and publication steps are unchanged.
24. **Status:** **D-m3 RESOLVED**.

### Document and regression checks

25. **Temporality:** the status-oriented “currently” is gone. No forbidden progress wording occurs in the changed scopes; unchanged uses of “later” describe legitimate runtime ordering.
26. **Document roles:** §4.3.2 is timeless geometry; §4.3.2.6 states architecture invariants and delegates detailed methodology; §4.7.8 is cardinality-unambiguous.
27. **Cross-references:** all local references in the edited scopes resolve.
28. **Normative strength:** preserved; no `MUST`/`MUST NOT` outcome was weakened.
29. **D-M1:** ownership before rename, claim/lock continuity, rename-plus-parent-fsync publication, next-owner adoption, and absence of new persistent state remain unchanged. Pass A–C material is outside every diff hunk and therefore unaffected.
30. **D-M2:** offline ownership, supported-v1 identity, retired-root grammar, U1 authority, rename-plus-parent-fsync retirement, retryable reclamation, and future-format refusal remain unchanged. §4.7.9 is byte-for-byte identical to HEAD.
31. **§41.3:** byte-for-byte unchanged.
32. **`docs/VERIFICATION.md`:** byte-for-byte unchanged.
33. **§7.5:** byte-for-byte unchanged; “once the buffer layer exists” remains.
34. **Frozen architecture semantic question:** none.
35. **Follow-up verification gap:** yes—the comprehensive exhaustion-boundary procedure is absent from `VERIFICATION.md`.

### Diff and final repository state

36. **Files changed:** only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).
37. **Hunk classification:** A—D-m1 geometry wording; B—D-m2 ownership cleanup; C—D-m3 cardinality wording; D—no wrapping-only hunk.
38. **Task-modified files:** only `docs/ARCHITECTURE.md`.
39. **Final Git state:** ` M docs/ARCHITECTURE.md`; index empty; HEAD unchanged at `249b379ae3ce52a6160a4d0c6abe8b985e0b8af1`.
40. **`git diff --check`:** passed, exit code 0.
41. **External changes:** none observed; repository began clean and ended with only the authorized task diff.
42. **Pre-existing material:** nothing was modified, reverted, staged, or committed.
43. **Implementation work:** none; no builds, tests, benchmarks, formatting pass, source changes, or generated files.
44. **Phase 2:** remains **NOT STARTED / NOT AUTHORIZED**.