# Chapter-16 review verdict

**CHAPTER 16 — CLEAN**

Chapter 16 is technically coherent, semantically complete, time-independent, and correctly scoped as architecture. It introduces no frozen semantic question and needs no architecture edit.

Findings:

- BLOCKING: 0
- MAJOR: 0
- MINOR: 0
- EDITORIAL: 0

The recommended next action is **no architecture edit; perform Chapter-16 verification synchronization** to add byte-exact and deterministic procedural coverage.

## Scope and repository state

Primary scope: [docs/ARCHITECTURE.md — Chapter 16](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12489)

- Start: line 12489, `# 16. Catalog and Schema Metadata`
- Last Chapter-16 line: 13279
- Chapter-17 boundary: line 13280, `# 17. SQL Type and Value System`
- Size: 791 lines
- Structure: 29 heading lines—one chapter heading plus 28 sections/subsections.

Context-only architecture consulted:

- Front matter
- Chapters 3–15 where referenced or required
- §17.4.6 only because Chapter 16 explicitly references VARCHAR binary semantics
- §18.4 identifier canonicalization
- Chapter 21 DDL/catalog integration
- Chapter 34 statistics persistence
- §39.1 failure semantics
- §41.4 verification obligations

Other live documents consulted:

- `AGENTS.md`
- `docs/VERIFICATION.md`, especially Catalog Tests and §41.4 coverage
- Targeted ownership checks in `docs/PROJECT_STATE.md` and `docs/DEVELOPMENT.md`

No source, tests, builds, devlogs, or review artifacts were inspected.

## Actual heading inventory

| Line | Section | Exact heading |
|---:|---|---|
| 12489 | 16 | Catalog and Schema Metadata |
| 12491 | 16.1 | Role and dependency boundary |
| 12542 | 16.2 | V1 namespace model |
| 12573 | 16.3 | Stable catalog identities |
| 12615 | 16.4 | Built-in TypeId registry |
| 12634 | 16.5 | Catalog schema version 1 |
| 12650 | 16.5.1 | Built-in identities and scalar carriers |
| 12681 | 16.5.2 | `sys_tables` — TableId `1` |
| 12698 | 16.5.3 | `sys_columns` — TableId `2` |
| 12738 | 16.5.4 | `sys_indexes` — TableId `3` |
| 12759 | 16.5.5 | `sys_index_columns` — TableId `4` |
| 12773 | 16.5.6 | `sys_constraints` — TableId `5` |
| 12798 | 16.5.7 | `sys_statistics` — TableId `6` |
| 12830 | 16.5.8 | Logical keys, references, and physical catalog indexes |
| 12851 | 16.5.9 | Bootstrap self-description fixed point |
| 12866 | 16.5.10 | Validation and cache reconstruction |
| 12891 | 16.5.11 | Canonical creation traces |
| 12912 | 16.5.12 | Forbidden schema-v1 interpretations |
| 12929 | 16.6 | Immutable descriptors |
| 12982 | 16.7 | Historical schema interpretation |
| 13018 | 16.8 | Column identity versus position |
| 13041 | 16.9 | Catalog bootstrap |
| 13062 | 16.9.1 | CATALOG superblock identity |
| 13076 | 16.9.2 | CATALOG_DATA bootstrap-page format |
| 13142 | 16.9.3 | Bootstrap checksum and validation |
| 13169 | 16.9.4 | Minimal interpretation rule |
| 13206 | 16.9.5 | Creation and lifetime |
| 13227 | 16.10 | Catalog cache and descriptor lifetime |
| 13255 | 16.11 | Catalog invariants |

## Section-by-section review

Legend: `Clear` means fully specified locally; `Delegated` means a precise canonical owner supplies the mechanism.

| Section | Role | Timeless | Owner | Depth | Terms | Format | Identity | Txn/concurrency | Publication/failure | Recovery | X-ref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 16 | Chapter owner | Yes | Architecture | Sufficient | Clear | Clear | Clear | Clear | Clear | Clear | Precise | Consistent | CLEAN |
| 16.1 | Dependency boundary | Yes | Architecture | Sufficient | Clear | N/A | Clear | Delegated | Delegated | N/A | Clear | Consistent | CLEAN |
| 16.2 | Namespace model | Yes | Architecture | Sufficient | Clear | N/A | Clear | Delegated | Delegated | N/A | Clear | Consistent | CLEAN |
| 16.3 | Stable identities | Yes | Architecture | Sufficient | Clear | Clear | Clear | Delegated | Clear | Clear | Precise | Consistent | CLEAN |
| 16.4 | TypeId registry | Yes | Architecture | Sufficient | Clear | Exact | Clear | N/A | N/A | Exact | Clear | Consistent | CLEAN |
| 16.5 | Catalog schema selector | Yes | Architecture | Sufficient | Clear | Exact | Clear | MVCC delegated | Clear | Clear | Precise | Consistent | CLEAN |
| 16.5.1 | Built-ins/carriers | Yes | Architecture | Strong | Clear | Exact | Exact | Clear | Clear | Clear | Precise | Consistent | CLEAN |
| 16.5.2 | `sys_tables` | Yes | Architecture | Strong | Clear | Exact | Exact | MVCC delegated | Clear | File checks clear | Precise | Consistent | CLEAN |
| 16.5.3 | `sys_columns` | Yes | Architecture | Strong | Clear | Exact | Exact | MVCC delegated | Clear | Clear | Precise | Consistent | CLEAN |
| 16.5.4 | `sys_indexes` | Yes | Architecture | Strong | Clear | Exact | Exact | §11 delegated | Clear | Unsupported/corrupt clear | Precise | Consistent | CLEAN |
| 16.5.5 | `sys_index_columns` | Yes | Architecture | Strong | Clear | Exact | Exact | Delegated | Clear | Clear | Clear | Consistent | CLEAN |
| 16.5.6 | `sys_constraints` | Yes | Architecture | Strong | Clear | Exact | Exact | §11 delegated | Clear | Corruption clear | Precise | Consistent | CLEAN |
| 16.5.7 | `sys_statistics` | Yes | Architecture | Strong | Clear | Exact | Exact | MVCC clear | Clear | Rebuildable exception clear | Precise | Consistent | CLEAN |
| 16.5.8 | Keys/references/indexes | Yes | Architecture | Strong | Clear | Clear | Exact | DDL delegated | Clear | Clear | Precise | Consistent | CLEAN |
| 16.5.9 | Fixed point | Yes | Architecture | Strong | Clear | Exact | Exact | Open-owned | READY gate exact | Exact | Precise | Consistent | CLEAN |
| 16.5.10 | Validation/cache rebuild | Yes | Architecture | Strong | Clear | Exact | Exact | Snapshot-aware | Terminal publication exact | Exact | Precise | Consistent | CLEAN |
| 16.5.11 | Canonical traces | Yes | Architecture | Sufficient | Clear | Clear | Clear | DDL delegated | Clear | Clear | Precise | Consistent | CLEAN |
| 16.5.12 | Forbidden interpretations | Yes | Architecture | Strong | Clear | Exact | Exact | Clear | Clear | Clear | Precise | Consistent | CLEAN |
| 16.6 | Immutable descriptors | Yes | Architecture | Sufficient | Clear | Runtime | Exact | Snapshot-owned | Clear | N/A | Clear | Consistent | CLEAN |
| 16.7 | Historical schemas | Yes | Architecture | Strong | Clear | Clear | Exact | Lifetime clear | Clear | Reclamation clear | Clear | Consistent | CLEAN |
| 16.8 | Column ID vs position | Yes | Architecture | Sufficient | Clear | N/A | Exact | N/A | N/A | N/A | Clear | Consistent | CLEAN |
| 16.9 | Bootstrap role | Yes | Architecture | Strong | Clear | Exact | Exact | Open-owned | Clear | Exact | Clear | Consistent | CLEAN |
| 16.9.1 | CATALOG superblock | Yes | Architecture | Sufficient | Clear | Exact | Exact | N/A | Creation delegated | Exact | Clear | Consistent | CLEAN |
| 16.9.2 | Bootstrap page | Yes | Architecture | Strong | Clear | Byte-exact | Exact | N/A | Immutable | Exact | Precise | Consistent | CLEAN |
| 16.9.3 | Checksum/validation | Yes | Architecture | Strong | Clear | Byte-exact | Exact | N/A | Error classes exact | Exact | Precise | Consistent | CLEAN |
| 16.9.4 | Minimal interpretation | Yes | Architecture | Strong | Clear | Exact | Exact | Exclusive OPENING | READY publication exact | Exact | Precise | Consistent | CLEAN |
| 16.9.5 | Creation/lifetime | Yes | Architecture | Strong | Clear | Exact | Exact | Exclusive creation | Parent-fsync authority exact | Reopen exact | Precise | Consistent | CLEAN |
| 16.10 | Cache/lifetime | Yes | Architecture | Strong | Clear | Runtime | Exact | Snapshot-aware | Terminal publication exact | Rebuild derived | Precise | Consistent | CLEAN |
| 16.11 | Consolidated invariants | Yes | Architecture | Strong | Clear | Clear | Clear | Clear | Clear | Clear | Precise | Consistent | CLEAN |

## Canonical owner and ownership-boundary assessment

| Responsibility | Canonical owner |
|---|---|
| Catalog semantic metadata and six v1 relation schemas | Chapter 16 |
| Stable catalog identities and built-in identities | Chapter 16, allocator delegated to §13.2.6 |
| Catalog scalar carriers and validation | Chapter 16 |
| `catalog.dat` and `CATALOG_DATA` bootstrap bytes | Chapter 16 |
| Immutable/historical descriptor meaning | Chapter 16 |
| Snapshot-safe catalog-cache semantics | Chapter 16 |
| File superblock/checksum/version families | Chapter 4 |
| Tuple bytes and tuple `schema_version` | Chapter 5 |
| BufferPool/frame/pin retirement | Chapter 7 |
| Physical B+ layout and MTRs | Chapter 8 |
| Transaction snapshots, CommandId, terminal states | Chapter 9 |
| Catalog-row MVCC visibility | Chapters 9–10 |
| UNIQUE enforcement | Chapter 11 |
| WAL authorization and page persistence | Chapter 12 |
| Durable catalog-object allocator and recovery | Chapter 13 |
| Historical-metadata retention and file retirement gates | Chapter 14 |
| DML storage mutation | Chapter 15 |
| DDL locking, execution, and publication | Chapter 21 |
| Statistics payload grammar and generation selection | Chapter 34 |
| Failure classification | §39.1 |
| Deterministic proof methodology | `docs/VERIFICATION.md` |

The boundaries are precise. Chapter 16 does not duplicate DDL lock protocols, WAL mechanics, physical page formats beyond its own bootstrap page, or verification schedules.

## Persistent and runtime state assessment

Persistent structures owned by Chapter 16:

- Fixed TypeId registry.
- Six exact catalog relation descriptors.
- Stable built-in TableIds `1..6`.
- Scalar carrier rules and catalog-specific invariants.
- `catalog.dat`:
  - page 0 CATALOG FileSuperblock;
  - page 1 immutable CATALOG_DATA v1 page.
- Bootstrap entries and fixed-point self-description contract.
- Catalog object identity/reference relationships.
- Historical schema metadata.

Byte layout is complete:

- Page number, type, version, flags, LSN, header size, magic, generation, reserved bytes, entry count, offsets, widths, order, endianness, zero regions, and checksum are exact.
- Six 32-byte entries occupy bytes `64..255`.
- Bytes `256..8191` are zero.
- Unknown greater recognized versions are unsupported; malformed v1 content is corruption.
- No native structs, implicit enum ordinals, hidden catalog indexes, or implementation-defined payloads are allowed.

Runtime state:

- Immutable `TableDescriptor`, `SchemaDescriptor`, and `IndexDescriptor`.
- Snapshot-qualified cache entries.
- Descriptor references retained by plans/queries.
- Transaction-local uncommitted DDL visibility.
- Derived cache publication/invalidation state.

The cache is explicitly nonauthoritative and reconstructible. Persistent catalog MVCC rows and the validated bootstrap/self-description fixed point remain authoritative.

## Identity, namespace, allocation, and versions

| Domain | Width/source | Zero | Allocation/nonreuse |
|---|---|---|---|
| TableId | uint64, shared catalog-object allocator | Invalid | Durable, crash gaps legal, never reused |
| IndexId | uint64, shared catalog-object allocator | Invalid | Same |
| ConstraintId | uint64, shared catalog-object allocator | Invalid | Same |
| ColumnId | uint32, table-local | Invalid | Starts at 1; historical values never reused |
| TypeId | uint32 fixed registry | Invalid stored type | Fixed values 1–7 |
| FileId | uint32 allocator | Invalid | Durable nonreuse under Chapter 4 |
| SchemaVer | uint32 carrier | Invalid | Initial value 1; historical versions resolvable |
| StatsVersion | `(TxnId, CommandId)` | Normal TxnId required | Immutable generation identity |
| BindingId/plan slot | Runtime semantic IDs | Separate domains | Never conflated with catalog IDs |

Other conclusions:

- One namespace, `main`.
- Table and index names are distinct name classes.
- Names are binder-canonical binary bytes.
- Built-in TableIds consume the first six allocator values; the high-water becomes `7`.
- Exhaustion is exact under §13.2.6: no wrapping; last returned object ID is `UINT64_MAX-1`.
- `catalog_schema_version` selects the catalog compatibility contract.
- `sys_tables.schema_version` and tuple-header `schema_version` select ordinary table schema descriptors.
- These selectors are explicitly noninterchangeable.

## Catalog, bootstrap, DDL, and concurrency

Bootstrap fixed point:

```text
catalog.dat + built-in v1 decoder
    -> recovered catalog rows
    -> reconstructed immutable descriptors
    -> exact equality with the six canonical descriptors
```

A mismatch is committed catalog corruption and prevents READY.

Creation/open:

- The database is privately staged.
- TableIds `1..6` and required FileIds are allocated.
- Six system relation files are initialized.
- Self-description rows are seeded as FROZEN committed tuples.
- Checksummed `catalog.dat` is written and validated.
- Publication completes through the staging-root rename and parent-directory `fsync`.
- Open runs under exclusive OPENING ownership.
- Recovery completes before catalog-row interpretation.
- No normal catalog consumer runs before atomic READY publication.

DDL integration, owned by Chapter 21:

- Schema-changing DDL holds `SchemaLock`.
- Target-sensitive CREATE INDEX/DROP operations use exclusive `TableWriterGate`.
- Waits participate in Chapter 11’s unified wait graph.
- No wait occurs while holding page/B+ latches.
- Names and identities are revalidated against current committed catalog state.
- Required physical files are durably final-name-published before catalog rows name them.
- Catalog mutations use ordinary MVCC.
- Committed cache entries publish only at terminal COMMITTED publication.
- Abort leaves catalog rows invisible and transfers files to orphan retirement.
- DROP uses MVCC deletion and delayed physical retirement; no mutable dropped bit exists.

Rename/recreation and ALTER are delegated appropriately:

- Name reuse is governed by live-name visibility plus nonreused object IDs.
- A recreated object receives new stable IDs.
- Schema-changing DDL monotonically increases `schema_version`.
- Historical descriptors remain available while persisted tuples or live queries require them.
- V1 may reject unsupported tuple-rewrite forms without weakening the history-capable metadata model.

## Catalog semantics

Catalog MVCC:

- READ COMMITTED binding uses its statement snapshot.
- REPEATABLE READ uses its retained transaction snapshot.
- SELF visibility follows normal CommandId rules.
- Cache hits cannot override the caller’s catalog snapshot.
- Initial bootstrap rows are FROZEN.
- Ordinary catalog creators remain status-dependent until normal freezing/removal rules make them independent.

Constraints:

- PRIMARY KEY = one constraint row, one same-table unnamed unique index, ordered key rows, and nonnullable columns.
- UNIQUE constraints use the same immediate current-state protocol as §11.10.
- Standalone unique indexes have a non-NULL index name and no constraint owner.
- NOT NULL is solely `sys_columns.nullable=false`.
- CHECK and FOREIGN KEY have no schema-v1 rows.
- Defaults use exact `DefaultValueBlob` bytes; absence and typed `DEFAULT NULL` are distinct.
- Generated columns are outside this schema.

Collection order:

- Column order comes from explicit physical/logical ordinals.
- Index-key order comes from `key_ordinal`.
- Statistics assembly order comes from `chunk_index`.
- Heap row order is never semantic authority.
- Duplicates and missing ordinals are corruption, not arbitrary tie cases.

## Mutation, publication, failure, and recovery

- Catalog writes are ordinary WAL-backed MVCC heap mutations.
- Multi-page/multi-file logical atomicity comes from transaction status and recovery, not simultaneous page writes.
- Before transaction-owned catalog publication, a recoverable DDL failure may remain ACTIVE only after deterministic orphan ownership is established.
- Once a catalog row or catalog-row deletion marker publishes, later recoverable statement failure requires MUST_ABORT.
- Cache-install failure after durable COMMIT cannot change the transaction outcome; coherent cache invalidation/bypass is required, otherwise the database is noncontinuable.
- Known failures retain their §39.1 category. Uncertain ownership/publication is never guessed.
- Recovery replays physical WAL only; it does not replay SQL DDL.
- Catalog descriptors are rebuilt after system-relation recovery and before READY.
- No Chapter-16-specific checkpoint or WAL-retention exception exists.
- `catalog.dat` is immutable after creation and is not an ordinary WAL-managed catalog row store.
- File unlink waits for snapshot, descriptor, BufferPool/page/file-owner, and durable directory-retirement conditions.
- Catalog row RIDs are ordinary heap identities; semantic references use stable object IDs, not catalog RIDs.

Error taxonomy is complete:

- Duplicate user-visible live names: semantic DDL/name error under Chapter 21.
- Duplicate required committed catalog identities: corruption.
- Missing required catalog rows/files: corruption/prevents READY.
- Unknown required v1 enum/code: corruption.
- Recognized greater required format: unsupported format.
- Malformed statistics metadata: narrow rebuildable fallback where §16.5.7 permits it.
- Allocator exhaustion: explicit nonwrapping failure.
- Persistent page/index/bootstrap corruption: database corruption/noncontinuable, never a guessed descriptor.

## Documentation-model audits

### Temporality

| Category | Chapter-16 examples | Result |
|---|---|---|
| Runtime ordering | before READY, after recovery, after terminal publication | Valid |
| Transaction history | current transaction, earlier DDL command, later commit | Valid |
| Schema history | historical SchemaVer, later tuple versions | Valid |
| Persistent evolution | greater bootstrap/catalog version, explicit migration | Valid |
| Durable v1 scope | v1 defines/defer statements | Valid |
| Project chronology | current implementation, next milestone, Phase 2 | None |

“Future schema evolution” and “later catalog schema/migration” describe versioned persistent-format extensibility, not project scheduling.

### Document ownership

| Material | Present in Chapter 16? | Assessment |
|---|---:|---|
| Architecture contract | Yes | Correct owner |
| DEVELOPMENT sequencing | No | Clean |
| VERIFICATION recipe | No | Clean |
| PROJECT_STATE narration | No | Clean |
| Devlog/history | No | Clean |
| Source-layout prescription | No | Clean |
| Current implementation limitation | No | Clean |

The canonical creation traces are semantic examples, not deterministic test schedules. The validation order is a correctness protocol, not verification leakage.

### Analytical depth

| Mechanism | Assessment |
|---|---|
| Catalog/storage/SQL dependency boundary | Analytically sufficient |
| Stable identities and scalar carriers | Analytically sufficient |
| Six catalog schemas | Analytically sufficient |
| Constraint/index normalization | Analytically sufficient |
| Bootstrap fixed point | Analytically sufficient |
| Validation/cache reconstruction | Analytically sufficient |
| Historical schemas | Analytically sufficient |
| Descriptor immutability | Analytically sufficient |
| Cache authority and publication | Analytically sufficient |
| DDL delegation | Clear and precisely cross-referenced |
| Corruption vs rebuildable statistics | Analytically sufficient |
| Bootstrap creation durability | Analytically sufficient |

### Terminology

| Term | Canonical meaning |
|---|---|
| Catalog | Authoritative transactional semantic metadata |
| Catalog schema version | Compatibility selector for the six system relation descriptors |
| SchemaVer | Per-table tuple-body descriptor identity |
| Bootstrap | Immutable locator and minimal decoder input |
| Self-description fixed point | Equality between built-in decoder facts and recovered catalog rows |
| Descriptor | Immutable semantic metadata snapshot |
| Cache | Derived snapshot-qualified acceleration structure |
| Current descriptor | Descriptor selected for an eligible catalog snapshot |
| Historical descriptor | Exact `(TableId, SchemaVer)` interpretation retained for old tuples |
| Semantic unique key | Catalog invariant independent of physical index presence |
| Stable identity | Typed persistent object identifier, not a name or address |
| Carrier | Physical scalar representation for a named logical domain |
| Committed catalog state | MVCC-visible terminal metadata |
| Rebuildable statistics | Non-authoritative planning metadata with narrow fallback |
| READY | Atomic publication point after recovery and complete catalog validation |

No correctness-relevant ambiguous synonym remains.

### Normative language

| Requirement family | Normative quality |
|---|---|
| Fixed TypeIds/TableIds/ColumnIds | Exact MUST-level contract |
| Catalog schema columns/order/nullability | Exact |
| Scalar carriers and validation | Exact |
| Constraint/index cross-checks | Exact |
| Bootstrap bytes/checksum | Exact |
| Version/corruption classification | Exact |
| Fixed-point validation | Exact |
| Descriptor immutability/lifetime | Exact |
| Cache snapshot qualification | Exact |
| Historical schema retention | Exact |
| Forbidden interpretations | Explicit MUST NOT list |

Source-layout and algorithm coupling are absent. Abstract descriptor names are architectural concepts; the text explicitly leaves C++ ownership and derived layout choices implementation-specific.

## Explicit cross-reference audit

| Source | Target | Purpose | Status |
|---|---|---|---|
| 16.3 | §13.2.6 | Catalog-object allocation | Precise/correct |
| 16.4 | 16.5 | Persistent TypeId use | Precise/correct |
| 16.5 | §4.14 | Compatibility dispatch | Precise/correct |
| 16.5.1 | §4.3.2 | Numeric domains/writer maxima | Precise/correct |
| 16.5.1 | §18.4 | Identifier canonicalization | Precise/correct |
| 16.5.2 | §4.7 | Managed HEAP/FSM identity | Precise/correct |
| 16.5.3 | §21.12.1 | DefaultValueBlob | Precise/correct |
| 16.5.4 | §11.10 | UNIQUE semantics | Precise/correct |
| 16.5.4 | §4.14.6 | Required index format classification | Precise/correct |
| 16.5.6 | §11.10 | UNIQUE/PK enforcement | Precise/correct |
| 16.5.7 | Chapter 34/§34.3.1 | StatsVersion identity | Precise/correct |
| 16.5.7 | §4.3.2.5 | Chunk arithmetic | Precise/correct |
| 16.5.7 | §34.14/§34.14.6 | Payload/fallback validation | Precise/correct |
| 16.5.7 | §17.4.6 | Binary VARCHAR carrier | Precise/correct |
| 16.5.8 | Chapter 34 | Selected statistics references | Precise/correct |
| 16.5.9 | Chapter 3 | READY prevention | Precise/correct |
| 16.5.10 | §16.5.7 | Statistics exception | Precise/correct |
| 16.5.10 | §4.7 | DROP/file retirement | Precise/correct |
| 16.5.10 | §39.1 | Terminal publication/failure | Precise/correct |
| 16.5.11 | §4.7 | CREATE file publication | Precise/correct |
| 16.5.11 | §11.10 | Unique-index enforcement | Precise/correct |
| 16.5.11 | §34.14 | Statistics payload | Precise/correct |
| 16.9.2 | §16.5.1/§13.2.6 | Bootstrap identities | Precise/correct |
| 16.9.3 | Chapter 4/§4.14.2 | CRC/version classification | Precise/correct |
| 16.9.4 | §3.3 | OPENING/READY lifecycle | Precise/correct |
| 16.9.5 | §4.7.8 | Database-root publication | Precise/correct |
| 16.10 | §14.17.1 | Object/statistics publication ordering | Precise/correct |

No vague or broken reference was found.

## Cross-chapter consistency matrix

| Owner | Chapter-16 relationship | Result |
|---|---|---|
| Chapter 3 | OPENING/READY and noncontinuable state | CONSISTENT |
| Chapter 4 | IDs, files, CRC, format classification | CONSISTENT BUT SPECIALIZED |
| Chapter 5 | Tuple format and `schema_version` | CONSISTENT BUT SPECIALIZED |
| Chapter 7 | File/frame retirement | CONSISTENT |
| Chapter 8 | B+ identity/key-schema/MTR | CONSISTENT |
| Chapter 9 | TxnId, CommandId, snapshots | CONSISTENT |
| Chapter 10 | Catalog-row MVCC/SELF | CONSISTENT |
| Chapter 11 | UNIQUE and transaction gates | CONSISTENT |
| Chapter 12 | WAL-backed mutation | CONSISTENT |
| Chapter 13 | Allocator, recovery, READY files | CONSISTENT BUT SPECIALIZED |
| Chapter 14 | historical metadata/reclamation/publication claims | CONSISTENT |
| Chapter 15 | internal catalog DML/no-undo boundary | CONSISTENT |
| Chapter 21 | DDL lock/publication protocol | CONSISTENT BUT SPECIALIZED |
| Chapter 34 | statistics payload/generation/fallback | CONSISTENT BUT SPECIALIZED |
| §39.1 | failure and terminal publication | CONSISTENT |
| §41.4 | catalog verification obligations | CONSISTENT, methodology incomplete |

Previous-chapter regression result: **PASS**.  
Chapter-15 compatibility result: **PASS**.

## 100-item technical consistency matrix

| # | Obligation | Result |
|---:|---|---|
| 1 | Catalog/storage/SQL dependency direction | CONSISTENT |
| 2 | Parser lacks physical-storage knowledge | CONSISTENT |
| 3 | Binder does not choose physical algorithms | CONSISTENT |
| 4 | One `main` namespace | CONSISTENT |
| 5 | Table-name live uniqueness | CONSISTENT |
| 6 | Index-name live uniqueness | CONSISTENT |
| 7 | Table/index separate name classes | CONSISTENT |
| 8 | Column-name per-version uniqueness | CONSISTENT |
| 9 | TableId width/domain | CONSISTENT |
| 10 | IndexId width/domain | CONSISTENT |
| 11 | ConstraintId width/domain | CONSISTENT |
| 12 | ColumnId width/domain | CONSISTENT |
| 13 | TypeId width/domain | CONSISTENT |
| 14 | Shared catalog-object allocator | CONSISTENT |
| 15 | Typed wrappers remain distinct | CONSISTENT |
| 16 | Zero object IDs forbidden | CONSISTENT |
| 17 | Crash gaps legal | CONSISTENT |
| 18 | Persistent ID nonreuse | CONSISTENT |
| 19 | Allocator overflow does not wrap | CONSISTENT |
| 20 | Built-in IDs consume values 1–6 | CONSISTENT |
| 21 | Initial high-water equals 7 | CONSISTENT |
| 22 | ColumnIds begin at 1 | CONSISTENT |
| 23 | Historical ColumnIds not reused | CONSISTENT |
| 24 | Fixed TypeIds 1–7 | CONSISTENT |
| 25 | NULL/UNKNOWN not persisted TypeIds | CONSISTENT |
| 26 | Catalog schema selector distinct from SchemaVer | CONSISTENT |
| 27 | Tuple schema version fixed at 1 for built-ins | CONSISTENT |
| 28 | Schema-v1 field order exact | CONSISTENT |
| 29 | Schema-v1 nullability exact | CONSISTENT |
| 30 | Schema change requires version/migration | CONSISTENT |
| 31 | Unsigned uint64 carrier semantics | CONSISTENT |
| 32 | Zero-extended uint32 carrier semantics | CONSISTENT |
| 33 | Signed carrier ordering not identity authority | CONSISTENT |
| 34 | File/object writer maxima checked | CONSISTENT |
| 35 | Names binder-canonical and binary | CONSISTENT |
| 36 | No extra identifier-length limit | CONSISTENT |
| 37 | System relation direct DML forbidden | CONSISTENT |
| 38 | `sys_tables` schema exact | CONSISTENT |
| 39 | HEAP/FSM identity cross-check | CONSISTENT |
| 40 | No dropped/system flags | CONSISTENT |
| 41 | `sys_columns` schema exact | CONSISTENT |
| 42 | Physical/logical ordinals contiguous | CONSISTENT |
| 43 | ColumnId not inferred from ordinal | CONSISTENT |
| 44 | No-default vs DEFAULT NULL distinct | CONSISTENT |
| 45 | DefaultValueBlob exact validation | CONSISTENT |
| 46 | `sys_indexes` schema exact | CONSISTENT |
| 47 | Index file identity exact | CONSISTENT |
| 48 | Key schema version exact | CONSISTENT |
| 49 | Greater key format unsupported | CONSISTENT |
| 50 | Constraint-owned index unnamed | CONSISTENT |
| 51 | Standalone index named | CONSISTENT |
| 52 | Unique bit and constraint authority separated | CONSISTENT |
| 53 | `sys_index_columns` schema exact | CONSISTENT |
| 54 | Key ordinals contiguous | CONSISTENT |
| 55 | Duplicate key components forbidden | CONSISTENT |
| 56 | 1024-byte key limit preserved | CONSISTENT |
| 57 | `sys_constraints` schema exact | CONSISTENT |
| 58 | UNIQUE kind code exact | CONSISTENT |
| 59 | PRIMARY_KEY kind code exact | CONSISTENT |
| 60 | Unknown constraint code corrupt | CONSISTENT |
| 61 | NOT NULL authority in `sys_columns` | CONSISTENT |
| 62 | One primary key per table | CONSISTENT |
| 63 | Constraint/index same-table cross-check | CONSISTENT |
| 64 | `sys_statistics` schema exact | CONSISTENT |
| 65 | StatsVersion identity exact | CONSISTENT |
| 66 | StatsVersion does not pin status history | CONSISTENT |
| 67 | Chunk range/size arithmetic exact | CONSISTENT |
| 68 | Statistics fragments binary-safe | CONSISTENT |
| 69 | Malformed statistics use narrow fallback | CONSISTENT |
| 70 | No hidden catalog B+ indexes | CONSISTENT |
| 71 | Semantic keys independent of access path | CONSISTENT |
| 72 | Cross-row references complete | CONSISTENT |
| 73 | Catalog meaning independent of heap order | CONSISTENT |
| 74 | Bootstrap self-description fixed point exact | CONSISTENT |
| 75 | Fixed-point mismatch prevents READY | CONSISTENT |
| 76 | Validation ordering complete | CONSISTENT |
| 77 | Duplicate committed metadata is corruption | CONSISTENT |
| 78 | No arbitrary duplicate selection | CONSISTENT |
| 79 | Cache reconstruction deterministic | CONSISTENT |
| 80 | CREATE uses transactional catalog rows | CONSISTENT |
| 81 | DROP uses MVCC deletion | CONSISTENT |
| 82 | No mutable dropped bit | CONSISTENT |
| 83 | Descriptor fields sufficient | CONSISTENT |
| 84 | Descriptors immutable | CONSISTENT |
| 85 | C++ addresses not semantic identity | CONSISTENT |
| 86 | Historical ResolveSchema complete | CONSISTENT |
| 87 | Vacuum retains required schemas | CONSISTENT |
| 88 | Index-only metadata need not change tuple SchemaVer | CONSISTENT |
| 89 | Column identity distinct from position | CONSISTENT |
| 90 | `catalog.dat` exactly two pages | CONSISTENT |
| 91 | CATALOG superblock fields exact | CONSISTENT |
| 92 | Bootstrap page offsets/widths exact | CONSISTENT |
| 93 | Bootstrap entries ordered and unique | CONSISTENT |
| 94 | Reserved bytes canonical zero | CONSISTENT |
| 95 | CRC mandatory from creation | CONSISTENT |
| 96 | Greater recognized format unsupported | CONSISTENT |
| 97 | Recovery precedes self-description validation | CONSISTENT |
| 98 | READY publication is atomic | CONSISTENT |
| 99 | Database creation waits for parent `fsync` | CONSISTENT |
| 100 | Cache never overrides catalog snapshot | CONSISTENT |

Implementer-invention result: **none required for Chapter-16-owned behavior**. Delegated behavior has precise canonical owners.

## 20-item documentation-model matrix

| # | Question | Result |
|---:|---|---|
| 1 | Timeless wording? | CONSISTENT |
| 2 | Runtime temporal language valid? | CONSISTENT |
| 3 | No current implementation status? | CONSISTENT |
| 4 | No Phase-2 narration? | CONSISTENT |
| 5 | No DEVELOPMENT sequencing? | CONSISTENT |
| 6 | No verification procedure leakage? | CONSISTENT |
| 7 | No PROJECT_STATE leakage? | CONSISTENT |
| 8 | No devlog/history? | CONSISTENT |
| 9 | No source-layout coupling? | CONSISTENT |
| 10 | Persistent ownership precise? | CONSISTENT |
| 11 | Runtime/persistent authority separated? | CONSISTENT |
| 12 | Identity terminology precise? | CONSISTENT |
| 13 | Version terminology precise? | CONSISTENT |
| 14 | Bootstrap terminology precise? | CONSISTENT |
| 15 | Publication terminology precise? | CONSISTENT |
| 16 | Corruption/unsupported distinction clear? | CONSISTENT |
| 17 | Analytical rationale sufficient? | CONSISTENT |
| 18 | Cross-references precise? | CONSISTENT |
| 19 | Implementation freedom preserved? | CONSISTENT |
| 20 | Readable without implementation-state knowledge? | CONSISTENT |

## Findings and frozen questions

- Complete BLOCKING findings: none.
- Complete MAJOR findings: none.
- Complete MINOR findings: none.
- Complete EDITORIAL findings: none.
- FROZEN ARCHITECTURE SEMANTIC QUESTIONS: **NONE**.
- Out-of-scope observations: none.

## Follow-up verification gaps

These are methodology gaps, not architecture findings.

| Gap | Architecture owner | Missing/partial procedural family | Reusable coverage |
|---|---|---|---|
| V16-1 | §§16.9.1–16.9.3 | Byte-exact `catalog.dat` superblock/bootstrap fixtures, every reserved region, checksum and version-classification matrix | Chapter-4 byte/checksum framework |
| V16-2 | §§16.4–16.5.7 | Exact six-descriptor fixtures, carrier boundaries, sentinels, nullable/default combinations, forbidden schema-v1 encodings | Tuple/type corruption fixtures |
| V16-3 | §§16.5.8–16.5.10 | Deterministic cross-row uniqueness/reference/ordinal and fixed-point mismatch matrix | Existing catalog reopen tests |
| V16-4 | §§16.9.4–16.9.5 | Bootstrap creation/open persisted-prefix matrix, missing/wrong file identities, READY prohibition | Lifecycle/file-publication crash harness |
| V16-5 | §§16.6–16.10 | Snapshot-qualified cache install/invalidate races, terminal publication failure, descriptor immutability | Existing cache/DDL visibility procedures |
| V16-6 | §§16.7–16.8 | Historical-schema retention through vacuum, DROP, old snapshots, and exact index-key reconstruction | Chapter-14 reclamation procedures |
| V16-7 | §16.5.7 | Statistics chunk/fallback integration with catalog MVCC/freezing and manifest identity | Chapter-34 statistics procedures |
| V16-8 | §16.5.11 | CREATE/DROP catalog-row and file-publication crash/failure composition | §41.3 lifecycle and Chapter-21 DDL coverage |

Existing `docs/VERIFICATION.md` covers bootstrap open, descriptor reconstruction, stable IDs, historical lookup, cache visibility, CREATE abort, and DROP retirement at a high level. It does not yet constitute a complete byte-exact Chapter-16 obligation map.

## Final direct questions

| Question | Answer |
|---|---|
| Any project-time/current-state wording? | NO |
| Any DEVELOPMENT-owned material? | NO |
| Any VERIFICATION-owned recipe? | NO |
| Any PROJECT_STATE-owned material? | NO |
| Any devlog/history material? | NO |
| Any ambiguous terminology? | NO |
| Any insufficient rationale? | NO |
| Any duplicated canonical owner? | NO |
| Any persistent-format ambiguity? | NO |
| Any identity/nonreuse ambiguity? | NO |
| Any allocator/high-water ambiguity? | NO |
| Any schema/version ambiguity? | NO |
| Any bootstrap fixed-point ambiguity? | NO |
| Any transaction/snapshot ambiguity? | NO |
| Any lock/gate ambiguity? | NO |
| Any DDL/DML coordination ambiguity? | NO |
| Any publication-boundary ambiguity? | NO |
| Any failure-after-publication ambiguity? | NO |
| Any crash/recovery ambiguity? | NO |
| Any future-format/corruption ambiguity? | NO |
| Any correctness-relevant implementer invention? | NO |
| Can Chapter 16 stand as timeless canonical v1 architecture? | YES |

## Chapter-17 boundary

The next direct review scope should begin at line 13280 with Chapter 17, **SQL Type and Value System**, and continue only through the line before Chapter 18. Its principal Chapter-16 handoff is the fixed TypeId registry; it must also preserve Chapter-5 storage encoding and Chapter-8 index ordering ownership.

Chapter 17 was not reviewed by this task.

## Git and read-only guarantees

Initial state:

- Working tree: clean
- Index: clean
- HEAD: `c49100b0b6afcfaa5bf9427a1d02a8b3d5506c05`

Final state:

- Working tree: clean
- Index: clean
- HEAD: `c49100b0b6afcfaa5bf9427a1d02a8b3d5506c05`
- `git diff --check`: passed with no output

Repository-state assessment:

- Files modified by audit: **NONE**
- Audit-created changes: **NONE**
- Review artifacts read/modified/staged: **NONE**
- Implementation work: **NONE**
- Builds/tests/benchmarks: **NONE**
- Staging/commit: **NONE**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
