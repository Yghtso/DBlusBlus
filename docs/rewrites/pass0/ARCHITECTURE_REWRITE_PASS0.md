# Rewrite Pass 0 — Architecture Inventory and Target Structure

## 1. Source snapshot

- Source file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- Physical text lines: 21,290
- Numbered architecture sections: `0..725` inclusive = **726 sections**
- Numbered `# N.` sections: **725**, with legacy section `0` represented as an H2 heading
- Markdown headings at levels H1–H3: **1,629**
- Missing numbered sections: **none**
- Duplicate numbered section IDs: **none**

The source snapshot above is the fixed reference for the rewrite ledger. If the legacy architecture changes before Pass 16, the inventory must be regenerated or the delta must be explicitly incorporated.

## 2. Pass 0 conclusion

A one-shot rewrite is not appropriate. The document is an evolutionary architecture record containing several layers at once: current normative contracts, earlier high-level versions of those contracts, rationale, project sequencing, historical architecture-status snapshots, verification plans, benchmark plans, module-layout recommendations, deferred experiments, and agent-specific instructions.

The rewrite should therefore be **semantics-preserving first**. Organizational cleanup and deduplication happen before deliberate architecture refinement.

## 3. Migration classes

| Class | Meaning in the rewrite |
|---|---|
| Architecture contract | Rewrite into the owning canonical chapter without changing semantics. |
| Architecture overview | Merge into later concrete contract; preserve unique constraints and rationale, remove duplicate normative statements. |
| Rationale | Keep adjacent to the decision it explains, visually subordinate to normative requirements. |
| Invariant | Preserve exactly; colocate with owner and maintain a global invariant appendix/index. |
| Deferred/future | Preserve as explicit non-v1 scope, consolidated by subsystem. |
| Verification/performance verification | Keep architecture-level obligations; move detailed procedures to proposed verification documentation. |
| Project roadmap/milestone | Move out of the final architecture contract; preserve in proposed development/roadmap documentation. |
| Project state/status | Move current status to `PROJECT_STATE.md`; retain only unique architecture diagrams/constraints. |
| AI/workflow | Move to `AGENTS.md`; no AI-specific wording in the final architecture. |

### Inventory totals

The complete per-section disposition is in `ARCHITECTURE_REWRITE_COVERAGE.md` / `.csv`. Important aggregate counts from the initial classification are:

- 546 — `ARCHITECTURE_CONTRACT`
-  38 — `ARCHITECTURE_OVERVIEW + ARCHITECTURAL_RATIONALE`
-  36 — `VERIFICATION_REQUIREMENT`
-  26 — `ARCHITECTURE_CONTRACT + RATIONALE`
-  25 — `PROJECT_ROADMAP / MILESTONE`
-  16 — `PERFORMANCE_VERIFICATION`
-   6 — `ARCHITECTURAL_INVARIANTS`
-   6 — `SUBSYSTEM_CONTRACT_INTRODUCTION`
-   6 — `DEFERRED_SCOPE`
-   5 — `IMPLEMENTATION_GUIDANCE / SOURCE_LAYOUT`
-   5 — `FUTURE_EXPERIMENTS / RATIONALE`
-   4 — `PROJECT_STATE + ARCHITECTURE_SUMMARY`
-   3 — `ARCHITECTURE_DECISION_SUMMARY`
-   2 — `PROJECT_ROADMAP / HISTORICAL_DESIGN_SEQUENCE`
-   1 — `AI_WORKFLOW`
-   1 — `PROJECT_PHILOSOPHY + ARCHITECTURAL_RATIONALE`

## 4. Canonicalization rules

1. The legacy `ARCHITECTURE.md` remains authoritative until final cutover.
2. Every legacy numbered section `0..725` must have a coverage-ledger row and a final disposition.
3. No source section is considered migrated merely because similar prose exists in the new file; its ledger row is marked complete only after semantic comparison.
4. The first rewrite preserves architecture semantics. New architectural decisions are recorded in the issue register and require explicit acceptance.
5. Later, more concrete contracts override/refine earlier generic overview language when the source explicitly says so; the earlier rationale is retained only if still accurate.
6. Persisted numeric codes, field widths, byte offsets, sentinels, formulas, state transitions, compatibility rules, and required error behavior receive exact-value verification during their owning pass.
7. Normative duplication is eliminated: each requirement receives one canonical definition, with cross-references elsewhere.
8. Rationale is preserved but separated from normative requirements.
9. Project progress, implementation sequencing, milestone targets, source-tree suggestions, and AI workflow do not remain embedded in the final architecture contract.
10. Detailed test/benchmark recipes are not silently discarded; they require a preserved destination before final cutover.
11. Deferred functionality is not treated as implemented/current v1 behavior.
12. Cross-subsystem rules are written at the highest common owning layer and referenced by dependent chapters.
13. The final contract uses ordinary technical documentation language. Agent-specific instructions are excluded.
14. Pass 16 must verify all coverage rows, all exact persisted-format values, terminology, and cross-references before replacement of the legacy contract.

## 5. Proposed final table of contents

### Part I — Foundations

1. **Scope and Design Goals**
2. **System Architecture and Dependency Model**
3. **Platform and Runtime Baseline**

### Part II — Storage and Persistence

4. **Persistent Storage Foundations**
5. **Heap Storage and Tuple Format**
6. **Free-Space Management and Physical Reclamation**
7. **I/O and Buffer Management**

### Part III — Indexing

8. **B+ Tree Indexing**

### Part IV — Transactions and Durability

9. **Transaction Lifecycle and Snapshots**
10. **MVCC Visibility and Tuple-Version Semantics**
11. **Logical Locking and Write Conflicts**
12. **Write-Ahead Logging and Commit Durability**
13. **Checkpointing and Crash Recovery**
14. **Vacuum and Storage Reclamation**
15. **Transactional Write Protocols**

### Part V — Catalog and SQL Semantics

16. **Catalog and Schema Metadata**
17. **SQL Type and Value System**
18. **Lexer, Parser, and AST**
19. **Binding and Expression Semantics**
20. **Logical Plans, Properties, and Rewrites**
21. **DDL/DML Semantic Planning and SQL v1 Scope**

### Part VI — Physical Execution

22. **Physical Plan and Runtime Operator Model**
23. **Vectorized Data and String Representation**
24. **Query Memory, Row Storage, and Spill**
25. **Vectorized Expression Execution**
26. **Pipeline Execution Model**
27. **Scans and Unary Physical Operators**
28. **Join Execution**
29. **Aggregation and DISTINCT**
30. **Sorting and Top-N**
31. **DML, DDL, VACUUM, and Result Interface**
32. **Parallel Execution and Scheduling**

### Part VII — Cost-Based Optimization

33. **Optimizer Architecture**
34. **Statistics**
35. **Cardinality Estimation**
36. **Cost Model and Base Access Paths**
37. **Physical Properties and Join Enumeration**
38. **Memo/Search and Memory-Aware Optimization**

### Part VIII — Cross-Cutting Requirements

39. **Error and Corruption Model**
40. **Observability and EXPLAIN**
41. **Verification Requirements**
42. **Performance Requirements**

### Appendices

- Appendix A. **Persistent Format Registry**
- Appendix B. **Global Invariants**
- Appendix C. **Deferred Features and Future Experiments**
- Appendix D. **Open Architecture Decisions**

### Structural intent

- Exact persisted layouts live in their owning subsystem chapter; Appendix A is a registry/index, not a competing second definition.
- Subsystem invariants live beside the subsystem and are also indexed in Appendix B.
- Deferred functionality is gathered in Appendix C with links back to the owning subsystem.
- Appendix D contains only genuinely unresolved decisions discovered during rewrite/refinement.
- Testing and performance remain technical requirements, but milestone-specific recipes should be separated from architecture if `VERIFICATION.md` is adopted.

## 6. Proposed supporting human documentation

Two additional files are recommended, but **not created as authoritative project documents in Pass 0**:

### `DEVELOPMENT.md`

Candidate destination for implementation order, milestone targets, and optional source/module-layout guidance currently mixed into the architecture.

### `VERIFICATION.md`

Candidate destination for detailed unit/property/crash/concurrency/fuzz/benchmark procedures. The architecture would still retain the correctness and performance properties that must be verified.

These destinations can be accepted or changed before final cutover. Until then, the legacy source remains the preservation copy.

## 7. Known consistency findings discovered during inventory

The detailed register is `ARCHITECTURE_REWRITE_ISSUES.md`.

Most importantly, current architecture §113 now requires RID reserved bytes `14..15` to be zero and requires the v1 decoder to reject nonzero values, while the Phase 1 audit recorded the decoder as permissive. This is not a rewrite ambiguity: it is an architecture/implementation checkpoint mismatch that must remain visible while code changes are paused.

The source also contains deliberate refinements such as old §181 replacing the generic old §24 `analysis/redo/undo` wording with the v1 no-physical-user-DML-undo recovery model. Such refinement chains are precisely why duplicate generic and concrete contracts must be collapsed carefully.

## 8. Additional safeguards added to the rewrite process

Pass 0 adds four controls beyond the original plan:

1. **Source hash pinning** — every coverage artifact identifies the exact legacy snapshot it describes.
2. **Subheading index** — all H1–H3 headings are indexed, not only the 726 numbered section boundaries.
3. **Migration status per old section** — every coverage row begins `PENDING` and can be marked migrated/reviewed only after comparison.
4. **Issue register** — contradictions, refinements, and true architecture questions are recorded outside the new contract until deliberately resolved.

## 9. Agreed 17-pass workflow

- **Pass 0:** Inventory and target structure — this pass.
- **Pass 1:** Legacy §§0–52: scope, global architecture, early overview, and separation of non-architecture material.
- **Pass 2:** Legacy §§53–63: persistent storage foundations.
- **Pass 3:** Legacy §§64–81: heap and tuple storage.
- **Pass 4:** Legacy §§82–85: FSM and page-local physical maintenance.
- **Pass 5:** Legacy §§86–108: I/O, BufferPool contract, ownership/lifetime boundaries, and removal of storage roadmap material.
- **Pass 6:** Legacy §§109–179: B+ tree architecture.
- **Pass 7:** Legacy §§180–214: transactions, snapshots, MVCC visibility, logical locking.
- **Pass 8:** Legacy §§215–255: WAL, commit, checkpoint, recovery.
- **Pass 9:** Legacy §§256–300: vacuum, RID reclamation, transactional write protocols, and transaction project-planning material.
- **Pass 10:** Legacy §§301–358: catalog, types, lexer/parser, binder, expressions.
- **Pass 11:** Legacy §§359–433: logical planning, rewrites, DDL/DML semantics, SQL-v1 scope, and upper-layer planning/status material.
- **Pass 12:** Legacy §§434–478: physical-plan/runtime foundations, vectors, memory, pipelines, scans/unary operators.
- **Pass 13:** Legacy §§479–567: joins, aggregation, sorting, DML execution, parallel runtime, quality/performance material, and execution roadmap/status.
- **Pass 14:** Legacy §§568–627: statistics, cardinality estimation, cost model, base access planning.
- **Pass 15:** Legacy §§628–725: physical properties, join enumeration, memo/search, diagnostics, optimizer quality/performance material, roadmap/status.
- **Pass 16:** Whole-document semantic reconciliation and cutover audit.

## 10. Exit criteria for every rewrite pass

1. Read the complete source range for that pass, including all subheadings.
2. Rewrite only into the canonical target chapters; do not preserve evolutionary duplicate structure.
3. Preserve every normative value and state transition unless an explicit issue is raised.
4. Preserve rationale that remains accurate, but separate it from requirements.
5. Update coverage rows for the processed source sections only after comparison.
6. Add newly discovered ambiguity/contradiction to the issue register instead of guessing.
7. Do not modify production code during the architecture rewrite.
8. Do not declare a pass complete while any source section in its range has an unresolved migration disposition.

## 11. Pass 0 artifacts

- `ARCHITECTURE_NEW.md` — empty contract skeleton/target TOC only; no migrated architecture content yet.
- `ARCHITECTURE_REWRITE_COVERAGE.md` — human-readable section-by-section migration ledger.
- `ARCHITECTURE_REWRITE_COVERAGE.csv` — machine-friendly equivalent of the ledger.
- `ARCHITECTURE_REWRITE_SUBSECTION_INDEX.csv` — all H1–H3 source headings with line numbers and parent old section.
- `ARCHITECTURE_REWRITE_ISSUES.md` — rewrite consistency/open-issue register.

Pass 0 performs no architecture-content rewrite and makes no semantic architecture decision.
