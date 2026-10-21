# Appendix A. Persistent Format Registry

This appendix indexes canonical persistent-format definitions. It does not replace the owning chapters.

| Persistent item | Canonical definition |
| --- | --- |
| `file_kind` code | §4.7 |
| Common page header | §4.8 |
| `page_type` code | §4.9 |
| File superblock | §4.10 |
| File superblock common prefix | §4.10 |
| File superblock CRC32C | §4.10.3 |
| HEAP_DATA total page header | §5.3 |
| HEAP_DATA heap-specific header | §5.3.1 |
| Heap slot entry | §5.4 |
| Heap slot-state code | §5.4 |
| Maximum raw inline tuple payload | §5.6 |
| Tuple header | §5.7 |
| Tuple flags | §5.8 |
| Null bitmap | §5.10 |
| VARCHAR descriptor | §5.12 |
| FSM category | §6.3 |
| FSM_DATA total page header | §6.5 |
| FSM_DATA specific header | §6.5 |
| FSM_DATA category region | §6.5 |
| FSM_DATA `first_heap_page_no` | §6.5–§6.6 |
| FSM_DATA `entry_count` | §6.5–§6.7 |
| Persisted index RID | §8.4.1 |
| Maximum encoded B+ user key | §8.6 |
| B+ leaf/internal total node header | §8.7–§8.10 |
| B+ node slot descriptor | §8.8 |
| B+ leaf entry | §8.9 |
| B+ internal entry | §8.10 |
| BTREE superblock total header | §8.2.1 |
| BTREE superblock extension | §8.2.1 |
| BTREE_FREE header | §8.18 |
| FLOAT64 index key value payload | §8.5.5 |
| Index key-schema fingerprint | §8.3.1 |
| `TXN_STATUS` FileKind code | §4.7 / §9.11 |
| `TXN_STATUS` PageType code | §4.9 / §9.12 |
| Transaction-status page | §9.12 |
| Transaction-status entry | §9.11–§9.12 |
| Transaction-status page capacity | §9.12 |
| Built-in catalog TypeId codes | §16.4 |
| `CATALOG` FileKind code | §4.7 / §16.9 |
| `CATALOG_DATA` PageType code | §4.9 / §16.9 |
| `catalog.dat` bootstrap file | §16.9 |
| CATALOG_DATA bootstrap page | §16.9.2 |
| CATALOG_DATA system-relation entry | §16.9.2 |
| PersistedScalarV1 header | §17.13 |
| PersistedScalarV1 total size | §17.13 |
| DefaultValueBlob v1 header | §21.12.1 |
| DefaultValueBlob v1 maximum | §21.12.1 |
| Built-in catalog relation TableIds | §16.5.1 |
| Catalog schema version 1 descriptors | §16.5 |
| Statistics catalog fragment maximum | §16.5.7 / §34.14 |
| StatisticsPayloadV1 common header | §34.14.1 |
| Statistics TABLE payload fixed prefix | §34.14.2 |
| Statistics COLUMN payload fixed prefix | §34.14.3 |
| Statistics INDEX payload | §34.14.4 |
| WAL segment | §12.2 |
| WAL record alignment | §12.3 |
| WAL ordinary record header | §12.4 |
| WAL PageId | §12.5 |
| WAL record_type code | §12.7 |
| PAGE_DELTA prefix | §12.8 |
| PAGE_DELTA patch header | §12.8 |
| PAGE_INIT/PAGE_IMAGE payload prefix | §12.9 |
| PAGE_INIT/PAGE_IMAGE page image | §12.9 |
| BTREE_MTR payload prefix | §12.10.2 |
| BTREE_MTR affected-page prefix | §12.10.2 |
| Database control file | §13.2 |
| Database control-slot v1 header | §13.2.1 |
| Catalog-object ID allocator | §13.2.6 / §21.4 |
| CHECKPOINT_BEGIN payload | §13.6 |
| CHECKPOINT_DATA prefix | §13.7 |
| Checkpoint DPT entry | §13.7 |
| Checkpoint writer entry | §13.7 |
| CHECKPOINT_END payload | §13.8 |

Use each owning definition for widths, field values, layout formulas, and validation rules.
