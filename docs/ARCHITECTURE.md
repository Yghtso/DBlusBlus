# DBlusBlus Architecture

**Authority:** Technical architecture contract

## Purpose

This document defines the technical architecture of DBlusBlus: a from-scratch, single-node relational database management system implemented in C++20.

It specifies architectural responsibilities, subsystem boundaries, persistent-format requirements, concurrency and lifetime rules, correctness invariants, and performance-relevant design constraints.

`ARCHITECTURE.md` is the authority for intended system behavior. `PROJECT_STATE.md` records current implementation capabilities, limitations, architecture/implementation mismatches, and active implementation boundaries; implementation sequencing and module-layout guidance belong in `DEVELOPMENT.md`; detailed test and benchmark procedures belong in `VERIFICATION.md`; historical implementation records belong in `devlog/`.

## Contract language

Requirements use ordinary normative language:

- **MUST / MUST NOT** — required by the architecture contract.
- **SHOULD / SHOULD NOT** — the architectural default; deviation requires a concrete technical reason and must remain compatible with all MUST-level requirements.
- **MAY** — explicitly permitted implementation freedom.
- **Deferred** — outside the required architectural scope. Making it required requires an explicit architecture revision.

Where a cross-layer summary restates a rule, the detailed format or protocol in the owning subsystem chapter is canonical. Summaries and registries are indexes and integration constraints; they do not create independent conflicting values.

Changes to an accepted architectural requirement require an explicit architecture revision. Implementation behavior does not silently redefine this document.

## Chapters

Each chapter is its own file under `architecture/`. Section numbers
(`§4.13.3`) are stable: code comments and the other documents cite them, and the
rendered book links every `§` reference to its section.


### Part I — Foundations

- [1. Scope and Design Goals](architecture/01-scope-and-design-goals.md)
- [2. System Architecture and Dependency Model](architecture/02-system-architecture-and-dependency-model.md)
- [3. Platform and Runtime Baseline](architecture/03-platform-and-runtime-baseline.md)

### Part II — Storage and Persistence

- [4. Persistent Storage Foundations](architecture/04-persistent-storage-foundations.md)
- [5. Heap Storage and Tuple Format](architecture/05-heap-storage-and-tuple-format.md)
- [6. Free-Space Management and Physical Reclamation](architecture/06-free-space-management-and-physical-reclamation.md)
- [7. I/O and Buffer Management](architecture/07-i-o-and-buffer-management.md)

### Part III — Indexing

- [8. B+ Tree Indexing](architecture/08-b-tree-indexing.md)

### Part IV — Transactions and Durability

- [9. Transaction Lifecycle and Snapshots](architecture/09-transaction-lifecycle-and-snapshots.md)
- [10. MVCC Visibility and Tuple-Version Semantics](architecture/10-mvcc-visibility-and-tuple-version-semantics.md)
- [11. Logical Locking and Write Conflicts](architecture/11-logical-locking-and-write-conflicts.md)
- [12. Write-Ahead Logging and Commit Durability](architecture/12-write-ahead-logging-and-commit-durability.md)
- [13. Checkpointing and Crash Recovery](architecture/13-checkpointing-and-crash-recovery.md)
- [14. Vacuum and Storage Reclamation](architecture/14-vacuum-and-storage-reclamation.md)
- [15. Transactional Write Protocols](architecture/15-transactional-write-protocols.md)

### Part V — Catalog and SQL Semantics

- [16. Catalog and Schema Metadata](architecture/16-catalog-and-schema-metadata.md)
- [17. SQL Type and Value System](architecture/17-sql-type-and-value-system.md)
- [18. Lexer, Parser, and AST](architecture/18-lexer-parser-and-ast.md)
- [19. Binding and Expression Semantics](architecture/19-binding-and-expression-semantics.md)
- [20. Logical Plans, Properties, and Rewrites](architecture/20-logical-plans-properties-and-rewrites.md)
- [21. DDL/DML Semantic Planning and SQL Scope](architecture/21-ddl-dml-semantic-planning-and-sql-scope.md)

### Part VI — Physical Execution

- [22. Physical Plan and Runtime Operator Model](architecture/22-physical-plan-and-runtime-operator-model.md)
- [23. Vectorized Data and String Representation](architecture/23-vectorized-data-and-string-representation.md)
- [24. Query Memory, Row Storage, and Spill](architecture/24-query-memory-row-storage-and-spill.md)
- [25. Vectorized Expression Execution](architecture/25-vectorized-expression-execution.md)
- [26. Pipeline Execution Model](architecture/26-pipeline-execution-model.md)
- [27. Scans and Unary Physical Operators](architecture/27-scans-and-unary-physical-operators.md)
- [28. Join Execution](architecture/28-join-execution.md)
- [29. Aggregation and DISTINCT](architecture/29-aggregation-and-distinct.md)
- [30. Sorting and Top-N](architecture/30-sorting-and-top-n.md)
- [31. DML, DDL, VACUUM, and Result Interface](architecture/31-dml-ddl-vacuum-and-result-interface.md)
- [32. Parallel Execution and Scheduling](architecture/32-parallel-execution-and-scheduling.md)

### Part VII — Cost-Based Optimization

- [33. Optimizer Architecture](architecture/33-optimizer-architecture.md)
- [34. Statistics](architecture/34-statistics.md)
- [35. Cardinality Estimation](architecture/35-cardinality-estimation.md)
- [36. Cost Model and Base Access Paths](architecture/36-cost-model-and-base-access-paths.md)
- [37. Physical Properties and Join Enumeration](architecture/37-physical-properties-and-join-enumeration.md)
- [38. Memo, Costed Physical Search, and Memory-Aware Optimization](architecture/38-memo-costed-physical-search-and-memory-aware-optimization.md)

### Part VIII — Cross-Cutting Requirements

- [39. Error and Corruption Model](architecture/39-error-and-corruption-model.md)
- [40. Observability and EXPLAIN](architecture/40-observability-and-explain.md)
- [41. Verification Requirements](architecture/41-verification-requirements.md)
- [42. Performance Requirements](architecture/42-performance-requirements.md)

### Appendices

- [Appendix A. Persistent Format Registry](architecture/appendix-a-persistent-format-registry.md)
- [Appendix B. Global Invariants](architecture/appendix-b-global-invariants.md)
- [Appendix C. Capability Boundaries](architecture/appendix-c-capability-boundaries.md)
- [Appendix D. Architecture Authority and Revision](architecture/appendix-d-architecture-authority-and-revision.md)
