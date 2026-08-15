# DBlusBlus Architecture

**Status:** Rewrite in progress.  
**Authority during rewrite:** the existing `ARCHITECTURE.md` remains the active architecture contract until the rewrite is fully reconciled and explicitly adopted.  
**Scope of this file at Pass 0:** target structure only; no architecture contract content has been migrated yet.

## Target document structure

### Part I — Foundations

- **1. Scope and Design Goals**
- **2. System Architecture and Dependency Model**
- **3. Platform and Runtime Baseline**

### Part II — Storage and Persistence

- **4. Persistent Storage Foundations**
- **5. Heap Storage and Tuple Format**
- **6. Free-Space Management and Physical Reclamation**
- **7. I/O and Buffer Management**

### Part III — Indexing

- **8. B+ Tree Indexing**

### Part IV — Transactions and Durability

- **9. Transaction Lifecycle and Snapshots**
- **10. MVCC Visibility and Tuple-Version Semantics**
- **11. Logical Locking and Write Conflicts**
- **12. Write-Ahead Logging and Commit Durability**
- **13. Checkpointing and Crash Recovery**
- **14. Vacuum and Storage Reclamation**
- **15. Transactional Write Protocols**

### Part V — Catalog and SQL Semantics

- **16. Catalog and Schema Metadata**
- **17. SQL Type and Value System**
- **18. Lexer, Parser, and AST**
- **19. Binding and Expression Semantics**
- **20. Logical Plans, Properties, and Rewrites**
- **21. DDL/DML Semantic Planning and SQL v1 Scope**

### Part VI — Physical Execution

- **22. Physical Plan and Runtime Operator Model**
- **23. Vectorized Data and String Representation**
- **24. Query Memory, Row Storage, and Spill**
- **25. Vectorized Expression Execution**
- **26. Pipeline Execution Model**
- **27. Scans and Unary Physical Operators**
- **28. Join Execution**
- **29. Aggregation and DISTINCT**
- **30. Sorting and Top-N**
- **31. DML, DDL, VACUUM, and Result Interface**
- **32. Parallel Execution and Scheduling**

### Part VII — Cost-Based Optimization

- **33. Optimizer Architecture**
- **34. Statistics**
- **35. Cardinality Estimation**
- **36. Cost Model and Base Access Paths**
- **37. Physical Properties and Join Enumeration**
- **38. Memo/Search and Memory-Aware Optimization**

### Part VIII — Cross-Cutting Requirements

- **39. Error and Corruption Model**
- **40. Observability and EXPLAIN**
- **41. Verification Requirements**
- **42. Performance Requirements**

### Appendices

- **Appendix A. Persistent Format Registry**
- **Appendix B. Global Invariants**
- **Appendix C. Deferred Features and Future Experiments**
- **Appendix D. Open Architecture Decisions**

The structure is provisional until Rewrite Pass 16 completes semantic reconciliation. Changes to organization are allowed during the rewrite when they improve canonical ownership of existing architecture, but semantic changes require explicit review rather than being hidden inside restructuring.
