# 16. Catalog and Schema Metadata

## 16.1 Role and dependency boundary

The catalog is the authoritative semantic metadata layer between persisted transactional storage and the SQL semantic layer.

The upper-layer dependency direction is:

```text
SQL text
  ↓
Lexer
  ↓
Parser
  ↓
AST
  ↓
Binder
  ├── Catalog
  └── Type System
  ↓
Bound Statements / Bound Expressions
  ↓
Logical Planner
```

The parser does not know physical storage.

The binder does not choose physical algorithms.

Catalog descriptors describe semantic objects and their persistent identities; they do not expose heap-page, B+ node, or BufferPool implementation details.

The catalog owns knowledge about at least:

- tables,
- columns,
- logical types,
- indexes,
- uniqueness,
- primary keys,
- nullability,
- namespaces,
- stable object IDs,
- schema versions,
- statistics metadata handles.

It does not own:

- heap-page bytes,
- B+ node layout,
- transaction snapshot implementation,
- SQL tokenization/parsing.

## 16.2 Namespace model

The architecture has one logical database and one SQL namespace:

```text
main
```

Unqualified table names resolve in `main`.

Both:

```sql
SELECT * FROM users;
SELECT * FROM main.users;
```

refer to the same namespace lookup.

Multiple SQL schemas/namespaces are deferred.

Within `main`:

```text
table names are unique among live tables
index names are unique among live indexes
column names are unique within one table schema version
```

Table and index names occupy separate semantic name classes.

## 16.3 Stable catalog identities

The stable identifier widths are:

```text
TableId  = uint64
ColumnId = uint32
IndexId  = uint64
ConstraintId = uint64
TypeId   = uint32
```

A table's `ColumnId` is stable for the lifetime of that column identity.

Internal references after binding use stable IDs rather than object-name strings.

These identities are distinct from:

```text
BindingId
logical output slot
physical column position
display position
```

The architecture allocates `TableId`, `IndexId`, and `ConstraintId` from the durable database-wide catalog-object allocator in §13.2.6.

The allocator never returns zero, tolerates crash gaps, and never reuses an ID that may have appeared in persistent state.

Column IDs are table-local:

```text
CREATE TABLE:
    assign ColumnId values 1..N in declaration order
```

`ColumnId{0}` is not assigned by the defined architecture.

A future schema evolution that adds a column allocates a value greater than every historical ColumnId for that table; dropped/removed ColumnIds are never reused.

Built-in TypeIds remain the fixed registry in §16.4 rather than using the catalog-object allocator.

## 16.4 Built-in TypeId registry

Because `sys_columns.type_id` is persistent semantic metadata, the architecture assigns stable built-in TypeId values:

| TypeId | Logical type |
|---:|---|
| `0` | invalid / not a stored type |
| `1` | BOOLEAN |
| `2` | INT32 |
| `3` | INT64 |
| `4` | FLOAT64 |
| `5` | DATE |
| `6` | TIMESTAMP |
| `7` | VARCHAR |

`NULL` and `UNKNOWN` are not legal persisted column TypeIds.

These TypeId values are a catalog-format contract and MUST NOT be renumbered because of source-language enum order.

## 16.5 Catalog schema version 1

This section is the canonical persisted and semantic descriptor set selected by:

```text
catalog_schema_version = 1
```

The six relations use the ordinary tuple format and ordinary `SchemaDescriptor` layout rules. There is no catalog-only tuple codec. Every tuple version in these six relations has tuple-header `schema_version=1`; physical column order is exactly the ordinal order in the tables below, and `ColumnId` is the listed stable identity, not an inferred ordinal. Any change to a listed column, order, TypeId, NULLability rule, or catalog-specific code requires a new catalog schema version or an explicit migration.

The bootstrap `catalog_schema_version` selects this built-in compatibility contract. It is distinct from `sys_tables.schema_version`, which selects the ordinary tuple-body `SchemaDescriptor` for one table. Both happen to be `1` for all six built-ins in v1; they are not interchangeable fields.

Compatibility dispatch follows §4.14: `catalog_schema_version=1` uses only these descriptors; zero is corrupt, and a recognizable greater version is `UNSUPPORTED_CATALOG_SCHEMA`. V1 never decodes a newer catalog through the version-1 descriptors. Ordinary per-table `schema_version` remains a descriptor identity, not this catalog-format selector.

All semantic fields in a catalog tuple version are immutable after that version is inserted. Catalog changes create/delete ordinary MVCC row versions; they do not update these fields in place. The tuple header's `xmin/cmin/xmax/cmax` remains owned by the ordinary heap/MVCC contract and is not duplicated as descriptor columns.

### 16.5.1 Built-in identities and scalar carriers

The six bootstrap relation codes are also their exact built-in TableIds:

| Relation code / TableId | Canonical relation name |
|---:|---|
| `1` | `sys_tables` |
| `2` | `sys_columns` |
| `3` | `sys_indexes` |
| `4` | `sys_index_columns` |
| `5` | `sys_constraints` |
| `6` | `sys_statistics` |

Database creation performs the first six catalog-object allocations, in that order, before any other catalog-object allocation. It therefore assigns exactly `TableId{1}` through `TableId{6}` and durably advances `next_catalog_object_id` to `7`. Values `1..6` are permanently reserved as these TableIds in the shared TableId/IndexId/ConstraintId allocator domain. No built-in IndexId or ConstraintId is implied. Every subsequently allocated user or internal TableId, IndexId, or ConstraintId is at least `7`; crash gaps remain legal.

The ordinary stored scalar registry has signed SQL integer types but several catalog identities are unsigned. Catalog schema v1 uses these exact carrier rules:

| Logical catalog domain | Physical TypeId | Canonical carrier and validation |
|---|---:|---|
| `TableId`, `IndexId`, `ConstraintId`, normal `TxnId`, or polymorphic uint64 identity | `INT64` (`3`) | Persist the logical uint64 bit pattern unchanged in the INT64 payload; decoding reinterprets those 64 bits as uint64. A negative SQL INT64 carrier is not a negative identifier. The catalog validator then enforces the field's narrower logical domain. |
| `FileId`, `ColumnId`, `SchemaVer`, `CommandId` | `INT64` (`3`) | Persist the zero-extended uint32 value. The SQL INT64 value must be in `0..UINT32_MAX`, then the field-specific sentinel/range rule applies. |
| TypeId, physical/logical/key ordinal, catalog enum code, chunk index/count | `INT32` (`2`) | Persist a nonnegative INT32 and enforce the exact field-specific range below. |
| Boolean property | `BOOLEAN` (`1`) | Use the ordinary canonical BOOLEAN scalar encoding only. |
| Name or opaque bounded byte sequence | `VARCHAR` (`7`) | Use ordinary binary VARCHAR. Names obey the canonical-name rule below; opaque blobs may contain any byte, including zero. |

These are scalar carriers, not conversions between identifier domains. A TableId carrier cannot be accepted as an IndexId merely because both use INT64. Equality, allocation-order comparison, StatsVersion ordering, filename rendering, and range validation occur after decoding to the named unsigned logical domain; signed INT64 ordering/arithmetic over the carrier is not identifier authority. `TableId{0}`, `IndexId{0}`, `ConstraintId{0}`, `FileId{0}`, `ColumnId{0}`, and TypeId `0` are illegal wherever the corresponding object identity is required. Allocator-produced FileId/TableId/IndexId/ConstraintId values and normal TxnIds additionally cannot exceed their exact v1 writer maxima in §4.3.2; a committed catalog row claiming an unreachable greater identity is corrupt rather than evidence of wrap/reuse. A statistics TABLE `scope_id` is the only catalog identity field below that permits zero. `stats_txn_id` must be a normal TxnId in §4.3.2's exact allocation domain, never `INVALID_TXN_ID` or `FROZEN_TXN_ID`; `stats_command_id` may be `0` and must be a valid CommandId of that transaction. No catalog column persists an LSN.

Names are stored as the exact canonical identifier bytes produced by binding: unquoted identifiers have already been normalized to lowercase and quoted identifiers preserve exact bytes/case under §18.4. Name comparison is binary. Names must be nonempty and the resulting catalog tuple must satisfy the ordinary tuple-size contract; catalog schema v1 does not introduce a separate SQL identifier-length limit. The built-in relation and column names below are exact unquoted canonical spellings.

Every column below is system-managed. Ordinary user `SELECT` may read system relations after normal catalog visibility/READY checks. Direct user `INSERT`, `UPDATE`, `DELETE`, or DDL targeting these six relations is forbidden. DDL and ANALYZE use the internal transactional catalog writers.

### 16.5.2 `sys_tables` — TableId `1`

`sys_tables` has exactly six columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `table_id` | `INT64` | no | Required TableId using the uint64 carrier; nonzero allocated identity. |
| `1` | `2` | `namespace` | `VARCHAR` | no | Exactly `main` in the defined architecture. |
| `2` | `3` | `table_name` | `VARCHAR` | no | Nonempty binder-canonical table name. |
| `3` | `4` | `heap_file_id` | `INT64` | no | Required nonzero FileId encoded as zero-extended uint32. |
| `4` | `5` | `fsm_file_id` | `INT64` | no | Required nonzero FileId encoded as zero-extended uint32. |
| `5` | `6` | `schema_version` | `INT64` | no | Current required SchemaVer, encoded as zero-extended uint32; `0` is invalid and CREATE TABLE begins at `1`. |

Semantic uniqueness is required for `table_id`, `(namespace, table_name)`, `heap_file_id`, and `fsm_file_id`. A FileId cannot occupy both heap and FSM roles or belong to two tables. The two files must be the managed `table_<table_id>.heap` and `table_<table_id>.fsm` entries required by §4.7, with matching FileSuperblock FileId, kind, and `object_id = table_id`.

The six built-in rows use their exact TableIds/names above, `namespace = main`, `schema_version = 1`, and the heap/FSM FileIds from the corresponding bootstrap entries. Built-ins use ordinary separate HEAP and FSM files; their tuples do not live in `catalog.dat`. No `flags`, `is_system`, or `is_dropped` column exists. Built-in identity follows the reserved TableId/name set; DROP is represented by ordinary MVCC/catalog visibility and delayed §4.7 retirement, not a mutable dropped bit.

### 16.5.3 `sys_columns` — TableId `2`

`sys_columns` has exactly ten columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `table_id` | `INT64` | no | Owning nonzero TableId using the uint64 carrier. |
| `1` | `2` | `schema_version` | `INT64` | no | Nonzero SchemaVer whose complete descriptor contains this column. |
| `2` | `3` | `column_id` | `INT64` | no | Table-local nonzero ColumnId encoded as zero-extended uint32. |
| `3` | `4` | `physical_ordinal` | `INT32` | no | Zero-based tuple-body column position. |
| `4` | `5` | `logical_ordinal` | `INT32` | no | Zero-based logical/`SELECT *` presentation position. |
| `5` | `6` | `column_name` | `VARCHAR` | no | Nonempty binder-canonical column name. |
| `6` | `7` | `type_id` | `INT32` | no | One recognized stored TypeId `1..7`; `NULL`/`UNKNOWN`/`0` are invalid. |
| `7` | `8` | `nullable` | `BOOLEAN` | no | Authoritative ordinary/PK-induced NULLability for this schema version. |
| `8` | `9` | `has_default` | `BOOLEAN` | no | Distinguishes absence of a DEFAULT clause from an explicit typed NULL default. |
| `9` | `10` | `default_value` | `VARCHAR` | yes | SQL NULL when `has_default=false`; otherwise the exact non-NULL byte string containing one §21.12.1 `DefaultValueBlob`. |

For each `(table_id, schema_version)`, rows form one complete `SchemaDescriptor`. `physical_ordinal` and `logical_ordinal` must each be exactly the contiguous set `0..N-1`; neither is derived from `ColumnId` or heap scan order. The semantic unique keys are:

```text
(table_id, schema_version, column_id)
(table_id, schema_version, physical_ordinal)
(table_id, schema_version, logical_ordinal)
(table_id, schema_version, column_name)
```

Every row references a visible `sys_tables.table_id`; rows for its current `schema_version` must reconstruct a complete current descriptor. CREATE assigns ColumnIds `1..N` in declaration order. ALTER TABLE/DROP COLUMN is outside the supported scope, so a newly created schema has `column_id = physical_ordinal + 1` and `physical_ordinal = logical_ordinal`, but consumers still use the separate fields and MUST NOT infer that equality as a general identity rule.

Default representation is exactly:

| `has_default` | `default_value` catalog SQL value | Meaning |
|---|---|---|
| `false` | SQL NULL | No DEFAULT clause. |
| `true` | non-NULL VARCHAR containing a valid `DefaultValueBlob` whose `PersistedScalarV1` is typed NULL | Explicit `DEFAULT NULL`. |
| `true` | non-NULL VARCHAR containing a valid `DefaultValueBlob` whose scalar is non-NULL | Explicit folded non-NULL default. |

`false` plus a non-NULL blob, or `true` plus SQL NULL, is catalog corruption. A present blob is `1..4096` bytes, passes all §21.12.1 magic/version/flags/length/CRC/reserved validation, contains exactly one canonical `PersistedScalarV1`, and its scalar TypeId equals this row's `type_id`. The scalar's NULL state is legal only when `nullable=true`; `DEFAULT NULL` for a NOT NULL column is rejected by DDL and is invalid committed catalog state. No SQL text, expression tree, bytecode, `schema_version_added/removed`, dropped-column marker, or flags are persisted in catalog schema v1.

The fixed self-description contains one `sys_columns` row for every column listed in all six tables in this section. Those built-in descriptor rows use `schema_version=1`, the listed ColumnId/ordinals/name/TypeId/NULLability, `has_default=false`, and SQL NULL `default_value`.

### 16.5.4 `sys_indexes` — TableId `3`

`sys_indexes` has exactly six columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `index_id` | `INT64` | no | Required nonzero allocated IndexId using the uint64 carrier. |
| `1` | `2` | `table_id` | `INT64` | no | Nonzero TableId of the indexed relation. |
| `2` | `3` | `index_name` | `VARCHAR` | yes | Binder-canonical name for a standalone CREATE [UNIQUE] INDEX; SQL NULL for a constraint-owned backing index. |
| `3` | `4` | `btree_file_id` | `INT64` | no | Required nonzero FileId encoded as zero-extended uint32. |
| `4` | `5` | `is_unique` | `BOOLEAN` | no | Whether normal execution enforces the §11.10 UNIQUE owner protocol for this index. |
| `5` | `6` | `key_schema_version` | `INT32` | no | Exact B+ key encoding schema version; v1 requires `1`. |

`index_id` and `btree_file_id` are each semantically unique. Every row references one `sys_tables` row. The managed file is exactly `index_<index_id>.btree`, with matching FileId, `FileKind::BTREE`, FileSuperblock `object_id = index_id`, key-schema version/fingerprint, and the ordered columns reconstructed from `sys_index_columns`.

Every committed v1 index row requires `key_schema_version=1`. A value less than or equal to zero is corrupt catalog metadata; a positive greater value is an unsupported required index format and prevents descriptor publication/READY under §4.14.6. V1 has no catalog state that permits silently ignoring or automatically rebuilding such an index.

A non-NULL `index_name` must be nonempty, binder-canonical, and unique among visible named indexes in `main`; table and index names remain separate classes. A NULL name means the index is owned by exactly one UNIQUE or PRIMARY KEY row in `sys_constraints`. A non-NULL name means a standalone CREATE INDEX/CREATE UNIQUE INDEX object and no constraint row may own it. The architecture does not generate a second persisted name for an implicit constraint index.

`is_unique` is the direct semantic authority for whether the physical index uses §11.10 enforcement. `sys_constraints` is the sole authority for whether that index also represents a table UNIQUE or PRIMARY KEY constraint. A constraint-owned backing index must have `is_unique=true`; neither the constraint row nor index bit alone is accepted when their required cross-check fails. A standalone `is_unique=true` row represents CREATE UNIQUE INDEX without a table-constraint row. There is no `primary` or flags column; primary-key semantics are never inferred from an index bit.

### 16.5.5 `sys_index_columns` — TableId `4`

`sys_index_columns` has exactly three columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `index_id` | `INT64` | no | Owning nonzero IndexId using the uint64 carrier. |
| `1` | `2` | `key_ordinal` | `INT32` | no | Zero-based position in the user-key component sequence. |
| `2` | `3` | `column_id` | `INT64` | no | Nonzero table-local ColumnId encoded as zero-extended uint32. |

For each index, `key_ordinal` is exactly the contiguous set `0..K-1`, where `K>=1`; `(index_id, key_ordinal)` and `(index_id, column_id)` are semantic unique keys. Every `index_id` references one `sys_indexes` row. Every `column_id` references a column in that index row's `table_id` and current indexed `SchemaDescriptor`. The explicit sort key for reconstruction is `(index_id, key_ordinal)`; physical heap row order is irrelevant.

Index keys contain columns only. Duplicate components, expression components, included/non-key columns, descending/null-order modifiers, and partial predicates are not encoded by this relation. The existing maximum encoded user key of 1024 bytes is authoritative; descriptor creation rejects any declared key whose canonical maximum/runtime encoding violates it. No separate persisted key-count field exists because the complete contiguous row set determines `K`.

### 16.5.6 `sys_constraints` — TableId `5`

`sys_constraints` has exactly five columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `constraint_id` | `INT64` | no | Required nonzero allocated ConstraintId using the uint64 carrier. |
| `1` | `2` | `table_id` | `INT64` | no | Nonzero owning TableId. |
| `2` | `3` | `constraint_kind` | `INT32` | no | Exact code from the table below. |
| `3` | `4` | `constraint_name` | `VARCHAR` | yes | Explicit binder-canonical name, or SQL NULL for an unnamed constraint. |
| `4` | `5` | `index_id` | `INT64` | no | Nonzero backing IndexId using the uint64 carrier. |

The only constraint-kind codes represented by rows are:

| Code | Meaning |
|---:|---|
| `1` | `UNIQUE` table constraint |
| `2` | `PRIMARY_KEY` table constraint |

Any other code in a catalog claiming schema version 1 is `CORRUPT_CATALOG` and prevents descriptor publication/READY; only a greater owning `catalog_schema_version` is unsupported rather than corrupt. CHECK and FOREIGN KEY are outside the supported scope. NOT NULL has no `sys_constraints` row: `sys_columns.nullable=false` is its sole catalog authority, including NOT NULL induced by PRIMARY KEY. Consequently every valid `sys_constraints` row has a backing `index_id`; there is no constraint payload and no payload grammar in schema version 1.

`constraint_id` and `index_id` are each semantically unique in this relation. A non-NULL constraint name must be nonempty and is unique within its owning table; unnamed constraints remain SQL NULL and no hidden generated name is persisted. Each row references an existing table and an existing `sys_indexes` row for that same table whose `index_name` is SQL NULL and `is_unique=true`. The ordered constrained columns are exactly that index's `sys_index_columns` rows; no duplicate column array exists.

For `UNIQUE`, those columns use the complete §11.10 key equality, NULL, current-owner, wait/recheck, and lock-lifetime semantics. For `PRIMARY_KEY`, exactly one such row may exist per table, every backing key column's current `sys_columns.nullable` is `false`, and enforcement is PRIMARY KEY = NOT NULL on every component plus the same §11.10 UNIQUE predicate. A standalone unique index has `sys_indexes.is_unique=true`, a non-NULL name, and no `sys_constraints` row.

### 16.5.7 `sys_statistics` — TableId `6`

`sys_statistics` has exactly eight columns:

| Ordinal | ColumnId | Name | TypeId | Nullable? | Meaning and canonical constraints |
|---:|---:|---|---|---|---|
| `0` | `1` | `table_id` | `INT64` | no | Analyzed nonzero TableId using the uint64 carrier. |
| `1` | `2` | `scope_kind` | `INT32` | no | `1=TABLE`, `2=COLUMN`, `3=INDEX`; no other code. |
| `2` | `3` | `scope_id` | `INT64` | no | Polymorphic uint64 carrier: `0` for TABLE, zero-extended nonzero ColumnId for COLUMN, nonzero IndexId for INDEX. |
| `3` | `4` | `stats_txn_id` | `INT64` | no | Normal creator TxnId using the uint64 carrier; first component of `StatsVersion`. |
| `4` | `5` | `stats_command_id` | `INT64` | no | Valid CommandId encoded as zero-extended uint32; second component of `StatsVersion`. |
| `5` | `6` | `chunk_index` | `INT32` | no | Zero-based fragment ordinal. |
| `6` | `7` | `chunk_count` | `INT32` | no | Identical total fragment count for the scope payload. |
| `7` | `8` | `payload_fragment` | `VARCHAR` | no | Arbitrary nonempty binary fragment, including zero bytes. |

The exact semantic row identity is:

```text
(table_id, scope_kind, scope_id,
 stats_txn_id, stats_command_id, chunk_index)
```

and it is unique among visible rows. `(stats_txn_id, stats_command_id)` is exactly Chapter 34's `StatsVersion`; all rows with that version are created by that transaction/command and their payload headers repeat the same identity. At original insertion, each row's ordinary MVCC `xmin/cmin` equals its `stats_txn_id/stats_command_id`. Any later architecture-authorized tuple freezing changes only ordinary MVCC ownership metadata, never the immutable StatsVersion columns. The outer tuple `xmin/cmin` follows ordinary catalog MVCC/freezing and may require creator-status lookup until made independent; after committed publication, the payload `stats_txn_id/stats_command_id` follows §34.3.1's opaque generation-identity rule and never pins transaction-status history.

For one `(table_id, scope_kind, scope_id, StatsVersion)`, `chunk_count` is in `1..1,048,576`, every row has the same value, and `chunk_index` is exactly the contiguous set `0..chunk_count-1`. Every nonfinal fragment has exactly 4096 bytes and the final fragment has `1..4096` bytes. Construction uses §4.3.2.5's checked uint32 total/chunk arithmetic; excess invalidates/fails only that ANALYZE generation. Reconstruction sorts by `chunk_index`, concatenates, and then applies every §34.14 total-length, checksum, header, identity, manifest, and payload validation rule. A duplicate/missing/noncontiguous chunk or mismatched count makes that scope/version incomplete; rows from versions are never mixed.

TABLE scope requires `scope_id=0`. COLUMN scope requires a nonzero ColumnId belonging to `table_id`. INDEX scope requires an IndexId belonging to `table_id`. Chapter 34 owns the existing TABLE/COLUMN/INDEX binary payload grammar and complete-generation selection; this schema does not alter it. Binary VARCHAR is valid for fragments because §17.4.6 defines VARCHAR as arbitrary byte strings without UTF-8/text validation.

An unknown `scope_kind` is never interpreted as a future scope. It makes the affected statistics row/version unusable under the statistics fallback rule below; unlike an unknown required constraint kind, it cannot redefine a core catalog descriptor.

Statistics are rebuildable planning metadata. A decodable catalog tuple whose statistics identity/chunk/payload metadata is malformed invalidates that statistics scope/version and selects an older complete version or missing-statistics fallback with a diagnostic under §34.14; it does not by itself make otherwise valid user data or the six core descriptors unreadable. This narrow tolerance does not permit a malformed tuple physical encoding to bypass the ordinary tuple decoder, does not repair cross-version chunks, and applies the exact §34.14.6 numerical acceptance policy rather than an implementation-local tolerance.

### 16.5.8 Logical keys, references, and physical catalog indexes

The semantic uniqueness rules above are catalog invariants whether or not a physical access path exists. Catalog schema v1 defines no built-in persistent B+ indexes over the six system relations, allocates no hidden built-in IndexIds, and adds no unlisted bootstrap roots. Catalog lookup/validation may scan these relations. Bootstrapped catalog indexes require a catalog schema revision and migration; an implementation of schema version 1 cannot assume them.

The required cross-row references are:

```text
sys_columns.table_id                  -> sys_tables.table_id
sys_indexes.table_id                  -> sys_tables.table_id
sys_index_columns.index_id            -> sys_indexes.index_id
sys_index_columns.column_id            -> a sys_columns column of that index's table
sys_constraints.table_id              -> sys_tables.table_id
sys_constraints.index_id              -> same-table unique sys_indexes row
selected sys_statistics COLUMN scope  -> manifest-named column of table_id
selected sys_statistics INDEX scope   -> manifest-named index of table_id
```

These are catalog structural invariants, not user-declared SQL FOREIGN KEY constraints. Current descriptor/index/constraint rows must reference current committed catalog objects. Statistics may become stale after later DDL: an old/invisible/inapplicable row may retain its historical scope identity, while a selected complete descriptor must match its TABLE manifest and analyzed schema/object identities under Chapter 34. Stale statistics are ignored/fallback metadata, not current catalog constraints.

Catalog semantic meaning never depends on heap row order. Columns sort by `physical_ordinal` or `logical_ordinal` for their respective purpose, index components sort by `key_ordinal`, and statistics fragments sort by `chunk_index`.

### 16.5.9 Bootstrap self-description fixed point

The built-in schema-v1 descriptors in this section are persisted-compatibility facts and the only descriptors permitted to decode the six relations selected by bootstrap `catalog_schema_version=1`. They do not replace self-hosting. After recovery, the committed/frozen self-hosted rows must describe the same six relations exactly:

```text
bootstrap locator + built-in schema-v1 decoder
    -> recovered catalog rows
    -> reconstructed ordinary descriptors
    -> exact equality with this section's six descriptors
```

For each bootstrap entry, open cross-checks the exact relation code/TableId mapping, `relation_schema_version=1`, heap/FSM FileIds against the matching `sys_tables` row, and each opened file's managed basename, FileId, FileKind, and `object_id`. The `sys_tables` and `sys_columns` self-description must reconstruct every listed column with the exact count, ColumnId, ordinal, name, TypeId, NULLability, and no-default state. `sys_indexes`, `sys_index_columns`, and `sys_constraints` contain no bootstrap-only rows or hidden catalog indexes. Initial `sys_statistics` is empty.

A database cannot redefine a built-in relation by changing its self-hosted rows while retaining catalog schema version 1. Any fixed-point or bootstrap cross-check mismatch is committed catalog corruption and prevents Chapter 3 READY.

### 16.5.10 Validation and cache reconstruction

Open/catalog publication validates in this order:

1. ordinary tuple physical validity under the exact built-in descriptor,
2. scalar/catalog-field validity, including carrier ranges, sentinels, names, enum codes, BOOLEAN canonicality, NULL/default combinations, and opaque-blob structure where applicable,
3. cross-row uniqueness, references, contiguous ordinals/chunks, index/constraint agreement, one PRIMARY KEY per table, complete descriptor reconstruction, and bootstrap fixed-point equality.

Except for the explicit rebuildable-statistics tolerance in §16.5.7, duplicate active identities/names, duplicate or missing ordinals, unknown TypeIds/codes, wrong-table references, missing required associated indexes/files, two primary keys, impossible default/nullability combinations, or bootstrap/self-description mismatches are catalog corruption. Required committed catalog corruption prevents READY; implementations do not select an arbitrary duplicate row. Internal writers perform the same validation before descriptor/cache publication. Discovering invalid required committed catalog state after READY is `CORRUPT_DATABASE`/database-noncontinuable under Chapter 3; no partial replacement cache is published.

Catalog cache reconstruction is deterministic:

```text
read current committed catalog rows under the open/catalog snapshot
validate tuple and scalar fields
group by typed object identity
validate logical keys and cross-row references
sort columns/index columns explicitly by ordinal
construct immutable SchemaDescriptor/IndexDescriptor/constraint metadata
cross-check all built-ins and physical file identities
publish the complete cache only after all required checks succeed
```

Catalog rows use ordinary heap-version MVCC. CREATE inserts rows transactionally. DROP makes rows invisible through ordinary catalog MVCC and sends owned files through §4.7 retirement; old row versions may remain physically. No catalog `is_dropped` bit exists. Cache publication follows §39.1 terminal publication and Chapter 3 READY rules.

### 16.5.11 Canonical creation traces

For a conceptual:

```sql
CREATE TABLE t (
    a INT32 NOT NULL,
    b VARCHAR DEFAULT 'x'
);
```

the internal DDL path allocates one TableId and two FileIds, durably publishes the managed HEAP/FSM files under §4.7, then inserts one `sys_tables` row `(id, main, t, heap, fsm, 1)` and two `sys_columns` rows. Column `a` is `(column_id=1, physical_ordinal=0, logical_ordinal=0, type_id=2, nullable=false, has_default=false, default_value=NULL)`. Column `b` is `(column_id=2, physical_ordinal=1, logical_ordinal=1, type_id=7, nullable=true, has_default=true, default_value=<valid VARCHAR-typed DefaultValueBlob for x>)`. No hidden field is required.

For `UNIQUE(a,b)`, DDL allocates an IndexId, ConstraintId, and B+ FileId; inserts one unnamed `sys_indexes` row with `is_unique=true`; inserts key rows `(index_id,0,column_id(a))` and `(index_id,1,column_id(b))`; and inserts one `sys_constraints` row `(constraint_id,table_id,1,NULL,index_id)`. For `PRIMARY KEY(a)`, the same shape uses kind `2`, the backing key has ordinal `0`, and the `sys_columns` row for `a` has `nullable=false`. Those four normalized facts—constraint kind, unique index, ordered index columns, and column nullability—are the complete authority and must cross-check.

For a standalone `CREATE INDEX i ON t(a,b)`, DDL inserts one `sys_indexes` row with `index_name=i`, `is_unique=false`, and its B+ FileId, plus two ordered `sys_index_columns` rows; there is no `sys_constraints` row. `CREATE UNIQUE INDEX` differs only by `is_unique=true` and uses §11.10 enforcement.

One ANALYZE StatsVersion inserts one TABLE scope chunk set with `scope_id=0`, one COLUMN chunk set for each manifest ColumnId with `scope_id=ColumnId`, and one INDEX chunk set for each manifest IndexId with `scope_id=IndexId`. Every set repeats the same `(table_id,stats_txn_id,stats_command_id)`, uses zero-based contiguous chunk indexes, and carries the already-defined §34.14 payload kind/header. No payload bytes are inferred from catalog row order.

DROP TABLE/INDEX deletes the applicable catalog row versions transactionally under ordinary MVCC. DROP TABLE removes its current table, column, index-column, index, and constraint authority as one DDL transaction; statistics become inapplicable/rebuildable metadata. §4.7 retires physical files only after the existing visibility/read-epoch/file-retirement gates. No ID or catalog row is rewritten into a dropped state.

### 16.5.12 Forbidden schema-v1 interpretations

An implementation of catalog schema version 1 MUST NOT:

1. choose different built-in TableIds or ColumnIds;
2. reorder columns or infer TypeIds/NULLability from source structs;
3. serialize native structs, enum ordinals, or an implementation-defined constraint payload;
4. conflate no default with a typed `DEFAULT NULL`;
5. derive column order from ColumnId or heap scan order;
6. derive PRIMARY KEY from an index flag or create a NOT NULL constraint row;
7. accept a constraint/index column array that disagrees with `sys_index_columns`;
8. assume a hidden built-in catalog index;
9. treat a physical catalog duplicate as a tie to resolve arbitrarily;
10. accept unknown catalog enum codes/flag-like data as forward-compatible;
11. store filesystem path strings instead of the specified FileIds;
12. publish catalog cache state before complete fixed-point/reference validation.

## 16.6 Immutable descriptors

Catalog lookup produces immutable semantic descriptors such as:

```text
TableDescriptor
SchemaDescriptor
IndexDescriptor
```

A `TableDescriptor` identifies at least:

```text
TableId
namespace/name
heap FileId
FSM FileId
current SchemaVer
current schema descriptor
index/constraint metadata references
```

A `SchemaDescriptor` identifies one exact:

```text
(TableId, SchemaVer)
```

and contains enough immutable information to interpret tuple format v1 for that schema version, including:

```text
stable ColumnIds
physical-column order
logical display positions
logical types / TypeIds
nullability
default metadata where applicable
```

An `IndexDescriptor` contains at least:

```text
IndexId
TableId
optional standalone index name
B+ FileId
unique semantics and any separately cross-checked constraint owner/kind
ordered key ColumnIds
key-schema version
```

Descriptor representation may use compact derived/precomputed layouts, but descriptor meaning is independent of C++ container addresses.

## 16.7 Historical schema interpretation

The catalog MUST support the semantic lookup:

```text
ResolveSchema(TableId, SchemaVer)
    -> immutable SchemaDescriptor
```

for every schema version that may still appear in persisted tuple versions or remain referenced by a live query/catalog descriptor.

Tuple decoding uses the tuple's persisted:

```text
schema_version
```

to choose that historical physical schema.

Vacuum likewise resolves the historical schema before decoding an old tuple version to reconstruct indexed user keys.

Historical schema metadata MUST NOT be discarded while a persisted tuple version can still require it for:

- tuple decoding,
- visibility/maintenance inspection,
- exact index garbage cleanup,
- version-chain maintenance.

The table `schema_version` is the version of the logical/physical column schema used to interpret tuple bodies.

A DDL commit that changes that tuple-interpreting schema monotonically increases `schema_version`.

Index-only metadata changes such as CREATE INDEX / DROP INDEX do not force otherwise-identical tuples to acquire a new tuple schema version.

DDL may reject changes that require unsupported tuple rewriting, but the metadata model itself remains history-capable.

## 16.8 Column identity versus position

`ColumnId` is not a display or storage ordinal.

The architecture distinguishes:

```text
ColumnId
physical schema position
logical display position
logical-plan output slot
```

Never assume:

```text
ColumnId == zero-based SELECT * position
```

This identity separation permits schema evolution while old tuple versions remain interpretable; it does not authorize schema-changing DDL outside the supported scope.

`SELECT *` uses the logical presentation order of the bound schema, not ColumnId numeric order.

## 16.9 Catalog bootstrap

The architecture uses one database-wide bootstrap file at the well-known database-root path:

```text
catalog.dat
```

Its only purpose is to locate and minimally interpret the six self-hosted catalog relations.

It is **not** the ordinary catalog row store.

The file is exactly:

```text
page 0    FileSuperblock with FileKind::CATALOG
page 1    one immutable CATALOG_DATA bootstrap page
```

The architecture does not append ordinary catalog rows to `catalog.dat`.

### 16.9.1 CATALOG superblock identity

The `catalog.dat` FileSuperblock uses:

```text
file_kind   = CATALOG              // persisted code 4
object_id   = 0
header_size = 72
```

`file_id` is one ordinary nonzero FileId allocated through the durable FileId allocator.

Because `catalog.dat` is found by its well-known singleton path, startup can read that FileId from the validated superblock before opening the system-relation files named by the bootstrap page.

### 16.9.2 CATALOG_DATA bootstrap-page format

The bootstrap page is exactly PageNo `1`.

Its common header is:

```text
page_type      = CATALOG_DATA      // persisted code 6
format_version = 1
flags          = 0
page_lsn       = INVALID_LSN
header_size    = 64
reserved16     = 0
page_no        = 1
```

The page-specific bytes are:

| Offset | Size | Field / meaning |
|---:|---:|---|
| `32` | 8 | magic = ASCII `DBLUSCAT` |
| `40` | 2 | bootstrap_version = `1` |
| `42` | 2 | entry_count = `6` |
| `44` | 4 | catalog_schema_version = `1` |
| `48` | 8 | bootstrap_generation = `1` |
| `56` | 8 | reserved64 = `0` |
| `64` | 192 | six 32-byte system-relation entries |
| `256` | 7936 | reserved zero bytes |

Each 32-byte system-relation entry is:

| Entry offset | Size | Field / meaning |
|---:|---:|---|
| `0` | 2 | system_relation_code |
| `2` | 2 | flags = `0` |
| `4` | 4 | relation_schema_version |
| `8` | 8 | TableId |
| `16` | 4 | heap FileId |
| `20` | 4 | FSM FileId |
| `24` | 8 | reserved64 = `0` |

All multi-byte integers are little-endian.

Entries occur exactly once and in ascending `system_relation_code` order:

```text
1 = sys_tables
2 = sys_columns
3 = sys_indexes
4 = sys_index_columns
5 = sys_constraints
6 = sys_statistics
```

For the bootstrap page:

```text
relation_schema_version = 1
```

for every entry.

Each entry's TableId is exactly equal to its `system_relation_code`, so the six TableIds are exactly `1..6` with the names fixed by §16.5.1. They are the first six allocations from §13.2.6 and are permanently reserved from later user/internal allocation.

Every heap/FSM FileId is nonzero, has the expected file kind/object identity when opened, and no heap/FSM FileId may be reused by another bootstrap entry.

### 16.9.3 Bootstrap checksum and validation

The bootstrap page carries the ordinary whole-page CRC32C from Chapter 4.

For this startup-critical page, checksum creation and verification are mandatory,
including during database bootstrap.

A v1 decoder rejects at least:

```text
wrong PageType/PageNo
wrong format_version/header_size
nonzero common flags/reserved fields
wrong magic
wrong bootstrap_version
entry_count != 6
catalog_schema_version != 1
bootstrap_generation != 1
unknown/out-of-order/duplicate system_relation_code
TableId not equal to system_relation_code
zero/duplicate object or file identities where prohibited
nonzero entry flags/reserved fields
nonzero bytes 256..8191
checksum mismatch
```

The exact family/version classification is §4.14.2: zero version selectors are corruption, while an exact CATALOG bootstrap family advertising a greater bootstrap/catalog/relation format version is unsupported and is not parsed through these entries/descriptors.

### 16.9.4 Minimal interpretation rule

The engine contains the one exact built-in descriptor set in §16.5 for:

```text
catalog_schema_version = 1
```

It contains exactly the physical/schema information listed there to decode the six system relations named by the bootstrap entries.

That bootstrap descriptor set is not an independently mutable metadata authority.

Startup proceeds conceptually as:

```text
under §3.3 exclusive OPENING ownership, open + validate catalog.dat
    ↓
decode the six bootstrap entries
    ↓
open each referenced system heap/FSM in recovery-private mode using
normal FileId/object checks
    ↓
recover their physical pages and transaction-status dependencies
    ↓
decode recovered sys_tables/sys_columns/... with bootstrap schema version 1
    ↓
construct ordinary immutable catalog descriptors
    ↓
cross-check the six bootstrap identities against visible self-describing catalog rows
    ↓
publish the ordinary metadata cache only at §3.3 READY
```

A bootstrap identity/schema mismatch against the self-hosted catalog is corruption and prevents normal open.
No normal binder, DDL, ANALYZE, or catalog-cache consumer may run between
bootstrap decoding and READY publication.

### 16.9.5 Creation and lifetime

Database creation follows the canonical staging-root and parent-directory publication protocol in §4.7.8. Within that private staging root it performs the first six catalog-object allocations in §16.5.1's fixed order, verifies that they returned TableIds `1..6` and left `next_catalog_object_id=7`, allocates the bootstrap FileIds, initializes the six system relation files, and seeds the exact §16.5 self-describing catalog rows as bootstrap-frozen committed metadata:

```text
xmin = FROZEN_TXN_ID
cmin = 0
xmax = INVALID_TXN_ID
cmax = 0
```

It then writes the complete checksummed `catalog.dat`, creates/synchronizes every startup-critical regular file and managed subdirectory, validates the bootstrap, renames the staging root without replacement, and synchronizes the external parent directory. Database creation MUST NOT report success before §4.7.8's final parent-directory `fsync` succeeds.

A failed incomplete creation is not a valid database open target.

`catalog.dat` is immutable after successful database creation.

Changing the bootstrap relation set or bootstrap schema requires a new explicitly versioned bootstrap format/migration; ordinary DDL never edits page 1.

This keeps bootstrap metadata minimal and prevents a second indefinitely divergent catalog.

## 16.10 Catalog cache and descriptor lifetime

Frequently used immutable descriptors may be cached in memory.

The cache is an acceleration structure, **not** a catalog-visibility authority.

Maintenance-specific current-object claims, ANALYZE statistics installation,
and DROP invalidation use the canonical §14.17.1 publication ordering; a cached
descriptor alone is never an object-lifetime or publication claim.

Cache keys use stable object IDs/schema versions and names where name lookup is required.

A cache hit may be used only when its descriptor is proven visible to the caller's catalog snapshot; otherwise lookup falls back to snapshot-aware catalog relations.

Published descriptors are immutable snapshots.

A schema-changing operation invalidates/replaces current-name/current-version cache entries only after its transaction reaches the terminal publication boundary.

Older immutable descriptors may remain alive while older snapshots/queries still reference them.

Bound/planned queries may retain shared immutable descriptors or generation/versioned handles for their required lifetime.

The exact C++ ownership mechanism is implementation-specific.

The semantic lifetime guarantee is not.

Uncommitted DDL may be visible to its own transaction through normal MVCC/self-visibility after the command boundary, but it is not installed into the globally published committed-descriptor cache.

## 16.11 Catalog invariants

1. The catalog maps human-readable names to stable semantic identities.
2. Parser output does not contain resolved catalog IDs.
3. Bound references use stable IDs rather than unresolved object-name strings.
4. TableId, ColumnId, IndexId, BindingId, schema position, and plan output slots are distinct identities.
5. Built-in persisted TypeIds are stable numeric codes.
6. Catalog system-relation semantic fields are stable even if their physical row layout evolves.
7. Catalog descriptors are immutable snapshots once published.
8. Active queries are never made to observe in-place descriptor mutation.
9. Every persisted tuple schema version that may still exist has a resolvable historical SchemaDescriptor.
10. Historical schema interpretation is retained long enough for vacuum to reconstruct exact old index keys.
11. Catalog cache lookup never bypasses the caller's MVCC/catalog snapshot.
12. Uncommitted DDL is never published as globally committed cache metadata.
13. TableId/IndexId/ConstraintId values are durably allocated before persistent use and never reused.
14. ColumnId values are table-local and never reused within a table's historical schema.
15. `catalog.dat` contains only the immutable bootstrap locator; ordinary catalog rows live in the self-hosted catalog relations.
16. The six bootstrap identities are cross-validated against the self-hosted catalog during open.
17. Bootstrap metadata remains minimal and cannot become a permanently divergent second catalog.
18. Database creation is not durable until the complete staging root has been renamed to its final name and the external parent directory has been synchronized.
19. `catalog_schema_version=1` selects exactly the six relation descriptors, built-in identities, codes, keys, and validation rules in §16.5.
20. The architecture has no hidden persistent catalog indexes, implementation-defined catalog columns, or opaque constraint payloads.
