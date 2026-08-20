• M-002 is resolved. Catalog schema version 1 now identifies one complete, deterministic catalog contract.

  1. Canonical ownership: §16.5 is now the sole catalog_schema_version=1 authority, using ordinary tuple/SchemaDescriptor machinery.
  2. Built-in TableIds:
      - 1 sys_tables
      - 2 sys_columns
      - 3 sys_indexes
      - 4 sys_index_columns
      - 5 sys_constraints
      - 6 sys_statistics

  3. ID reservation: bootstrap performs the first six catalog-object allocations in that order; IDs 1..6 remain permanently reserved and next_catalog_object_id becomes 7.
  4. sys_tables: six columns, ColumnIds 1..6, in physical order: table_id INT64, namespace VARCHAR, table_name VARCHAR, heap_file_id INT64, fsm_file_id INT64, schema_version INT64; all NOT NULL.
  5. sys_columns: ten columns, ColumnIds 1..10: table_id, schema_version, column_id, physical_ordinal, logical_ordinal, column_name, type_id, nullable, has_default, and nullable default_value.
     Physical and logical ordinals are explicit and zero-based.

  6. Defaults: has_default=false/default_value=NULL means no default. has_default=true requires a non-NULL DefaultValueBlob, whose scalar distinguishes typed NULL from non-NULL. Invalid
     combinations are corruption.

  7. sys_indexes: six columns: index_id, table_id, nullable index_name, btree_file_id, is_unique, and key_schema_version. Constraint-owned indexes have NULL names; standalone indexes have
     canonical names.

  8. sys_index_columns: three columns: index_id, zero-based key_ordinal, and column_id. Ordinals must be contiguous; duplicate ordinals or columns are forbidden.
  9. sys_constraints: five columns: constraint_id, table_id, constraint_kind, nullable constraint_name, and required index_id.
  10. Constraint codes are exactly 1=UNIQUE and 2=PRIMARY_KEY. There is no opaque payload or implementation-defined grammar.
  11. UNIQUE representation: sys_constraints establishes a table constraint; its same-table backing sys_indexes row must be unnamed and is_unique=true; columns come exclusively from
     sys_index_columns. Standalone UNIQUE INDEX has no constraint row.

  12. PRIMARY KEY uses kind 2, exactly one per table, a unique backing index, and nullable=false on every key column.
  13. NOT NULL is represented solely by sys_columns.nullable=false. No redundant NOT NULL constraint row exists.
  14. sys_statistics: eight columns: table_id, scope_kind, scope_id, stats_txn_id, stats_command_id, chunk_index, chunk_count, and binary payload_fragment; all NOT NULL.
  15. Statistics row identity is (table_id, scope_kind, scope_id, stats_txn_id, stats_command_id, chunk_index). Chunks are zero-based, contiguous, ordered explicitly, and limited to 4096 bytes.
  16. Identifier carriers are exact: uint64 identities use unchanged bit patterns in INT64 and unsigned-domain interpretation; uint32 identifiers use zero-extended INT64; enums/ordinals use INT32.
     No identifier domain is silently conflated.

  17. Names store binder-canonical bytes: unquoted names lowercase, quoted names exact, binary comparison, nonempty, and subject only to ordinary tuple-size constraints.
  18. Built-in catalog relations use the bootstrap-referenced separate HEAP/FSM files. User tables use table_<TableId>.heap/.fsm; indexes use index_<IndexId>.btree; FileIds and superblock
     identities must cross-check. catalog.dat remains locator-only.

  19. Logical uniqueness is defined for all relations, including table/name/file IDs, column identity/ordinals/names, index identity/name/file, ordered index membership, constraint identity/
     backing index/name, and statistics chunk identity.

  20. Cross-row invariants now cover table, column, index, constraint, and applicable statistics references. They are catalog structural checks, not SQL foreign keys.
  21. Bootstrap fixed point: recovered self-hosted rows must reconstruct descriptors exactly equal to the canonical six schema-v1 descriptors. A database cannot redefine built-ins through catalog
     rows.

  22. Validation has three layers: tuple physical validity, scalar/catalog-field validity, and cross-row/fixed-point validity. Required catalog corruption prevents READY; malformed rebuildable
     statistics select fallback rather than redefining core metadata.

  23. CREATE TABLE tracing now shows the exact sys_tables and sys_columns rows, including NOT NULL and folded default representation.
  24. UNIQUE/PRIMARY KEY tracing identifies the exact sys_constraints, sys_indexes, sys_index_columns, and sys_columns rows and their cross-checks.
  25. CREATE INDEX tracing defines named standalone index rows, key order, FileId/IndexId linkage, and uniqueness state.
  26. ANALYZE tracing defines TABLE, COLUMN, and INDEX scope chunk sets sharing one StatsVersion.
  27. DROP uses ordinary catalog MVCC deletion plus A-003 delayed file retirement. No mutable dropped flags or ID reuse are introduced.
  28. Cache reconstruction now has a deterministic validate/group/order/build/cross-check/publish sequence independent of heap scan order.
  29. Changed sections: lifecycle open cross-reference, §16.3, canonical §16.5, §16.6, §16.9.2–§16.9.5, §16.11, DDL/PK/index cross-references, §34.14, and the persistent-format registry in docs/
     ARCHITECTURE.md:10095.

  30. No existing canonical persisted format changed. This locks previously unspecified schema-v1 semantics; no page, WAL, bootstrap, scalar, or payload layout changed.
  31. catalog_schema_version remains 1.
  32. No M-002 semantic question remains open.
  33. No prerequisite dependency was discovered. Existing VARCHAR binary semantics support default and statistics blobs; no BLOB TypeId was needed.
  34. No new architecture question was introduced. M-006, M-008, M-009, M-011, M-012, and M-013 remain untouched.
  35. Production code and tests were untouched. Only docs/ARCHITECTURE.md changed. git diff --check passed, and descriptor tables were checked at exactly 6/10/6/3/5/8 columns.
  36. Implementation Phase 2 has NOT been entered.