# Architecture Rewrite Final Issue Disposition

All architecture questions/format gaps discovered during the rewrite have a final disposition.

| Issue | Final disposition | Summary |
|---|---|---|
| R-001 | OPEN IMPLEMENTATION MISMATCH — not an architecture question | RID reserved-byte contract vs Phase 1 implementation checkpoint |
| R-002 | RESOLVED / CLASSIFIED | Early generic recovery overview is superseded/refined by the concrete recovery contract |
| R-003 | RESOLVED / CLASSIFIED by Pass 16 | Implementation roadmap content is mixed into the architecture contract |
| R-004 | RESOLVED / CLASSIFIED by Pass 16 | Historical architecture-status snapshots are mixed into the contract |
| R-005 | RESOLVED / CLASSIFIED by Pass 16 | Detailed verification recipes and benchmark plans are mixed with architecture |
| R-006 | RESOLVED / CLASSIFIED by Pass 16 | Source/module layout guidance is presented as locked architecture |
| R-007 | RESOLVED / CLASSIFIED by Pass 16 | AI/workflow wording is embedded in architecture text |
| R-008 | RESOLVED / CLASSIFIED by Pass 16 | Supporting-document preservation before final cutover |
| R-009 | RESOLVED / CLASSIFIED by Pass 16 | Early join/aggregation/sort sections mix architecture with implementation sequencing |
| R-010 | RESOLVED / CLASSIFIED | Exact checksum coverage for ordinary random-access pages is not globally specified |
| R-011 | RESOLVED / CLASSIFIED | Heap INSERT wording references slot reuse before reuse eligibility is defined |
| R-012 | RESOLVED / CLASSIFIED | Early DiskManager `SyncWal()` sketch is refined by later WAL ownership |
| R-013 | RESOLVED / CLASSIFIED | Legacy Storage Milestone 1 test list contains premature “reusable slots” |
| R-014 | RESOLVED / CLASSIFIED | B+ tree persistent metadata/page formats are not fully byte-specified |
| R-015 | RESOLVED / CLASSIFIED | FLOAT64 memcomparable index encoding is not byte-exact |
| R-016 | RESOLVED / CLASSIFIED | Early B+ user-abort wording is superseded by no-physical-user-DML-undo |
| R-017 | RESOLVED / CLASSIFIED | Ordinary-page common `flags` semantics are not completed |
| R-018 | RESOLVED / CLASSIFIED | Tuple fixed-area layout is not byte-exact enough for a persistent-format contract |
| R-019 | RESOLVED / CLASSIFIED | Chapter 4 narrows RID physical ordering to non-unique indexes, while Chapter 8 makes it universal |
| R-020 | RESOLVED / CLASSIFIED | Root-metadata latch ordering relative to page latches is unspecified |
| R-021 | RESOLVED / CLASSIFIED | Whole heap-page recycling must obey physical RID reuse safety |
| R-022 | RESOLVED / CLASSIFIED | Transaction-status persistent format is incomplete |
| R-023 | RESOLVED / CLASSIFIED | `reserved_txn_id_end` boundary convention is unspecified |
| R-024 | RESOLVED / CLASSIFIED | Snapshot owner membership and exact `xmin` derivation are underspecified |
| R-025 | RESOLVED / CLASSIFIED | WAL record and payload byte encoding is not complete |
| R-026 | RESOLVED / CLASSIFIED | `database.control` slot format is semantic but not byte-exact |
| R-027 | RESOLVED / CLASSIFIED | Checkpoint WAL sequence identity is underspecified |
| R-028 | RESOLVED / CLASSIFIED | Read-epoch grace arithmetic is not exact |
| R-029 | RESOLVED / CLASSIFIED | Transaction-status prefix reclamation conflicts with the current absolute status-page mapping |
| R-030 | RESOLVED / CLASSIFIED | Ordinary-page checksum lifecycle is incomplete for the torn-page recovery contract |
| R-031 | RESOLVED / CLASSIFIED | New-page allocation and initialization lack a crash-safe publication protocol |
| R-032 | RESOLVED / CLASSIFIED | Vacuum reclamation state machine is incomplete at `DEAD -> UNUSED` and crash-retry boundaries |
| R-033 | RESOLVED / CLASSIFIED | READ COMMITTED whole-statement retry is unsafe after partial persistent writes |
| R-034 | RESOLVED / CLASSIFIED | Terminal transaction publication has an active-registry / lock-release linearization race |
| R-035 | RESOLVED / CLASSIFIED | Fuzzy checkpoint WAL retention does not yet guarantee retention of the full image needed for a later torn write |
| R-036 | RESOLVED / CLASSIFIED | Catalog bootstrap / `CATALOG_DATA` persistent representation is unspecified |
| R-037 | RESOLVED / CLASSIFIED | Transactional allocation of catalog object IDs is unspecified |
| R-038 | RESOLVED / CLASSIFIED | Built-in catalog TypeId numeric codes were absent from the legacy source |
| R-039 | RESOLVED / CLASSIFIED | DATE/TIMESTAMP epoch and unit semantics were absent from the legacy source |
| R-040 | RESOLVED / CLASSIFIED | Persistent default-expression encoding is not byte-exact |
| R-041 | RESOLVED / CLASSIFIED | Pass-12 execution-foundation precision completions |
| R-042 | RESOLVED / CLASSIFIED | Pass-13 execution semantic completions |
| R-043 | RESOLVED / CLASSIFIED | Client-visible result-chunk lifetime completion |
| R-044 | RESOLVED / CLASSIFIED | `sys_statistics` persistent payload is semantic but not byte-exact |
| R-045 | RESOLVED / CLASSIFIED | Pass-14 optimizer/statistics semantic completions |
| R-046 | RESOLVED / CLASSIFIED | ANALYZE is introduced by statistics architecture but is not integrated through the SQL/logical/physical execution stack |
| R-047 | RESOLVED / CLASSIFIED | Index access costing does not distinguish physical B+ entry pressure from logically live matches |
| R-048 | RESOLVED / CLASSIFIED | PredicateTruthEstimate is three-valued in shape but simple-predicate TRUE/FALSE/UNKNOWN mass is not fully specified |
| R-049 | RESOLVED / CLASSIFIED | Pass-15 optimizer search/property precision completions |

## Unresolved v1 architecture questions

**None.**

Deferred items in `ARCHITECTURE.md` Appendix C are intentionally outside the v1 baseline; they are not unresolved v1 architecture questions.

## Remaining non-architecture consistency item

`R-001` remains a code/state mismatch: the architecture requires RID reserved bytes 14..15 to be zero on write and rejected when nonzero on v1 decode, while the current Phase-1 decoder checkpoint is still permissive. `PROJECT_STATE.md` records this mismatch. No production code is changed by the architecture cutover.

## Pass-16 documentation classifications

- `R-003`: implementation roadmap moved to `DEVELOPMENT.md`.
- `R-004`: historical architecture-status snapshots excluded from the technical contract; current state remains in `PROJECT_STATE.md`, history in `devlog/`.
- `R-005`: detailed test/benchmark procedures preserved in `VERIFICATION.md`; architecture-level obligations remain in Chapters 41–42.
- `R-006`: concrete module-layout guidance moved to `DEVELOPMENT.md`.
- `R-007`: AI/agent workflow is absent from `ARCHITECTURE.md`; it remains in `AGENTS.md`.
- `R-008`: supporting-document preservation is complete through `DEVELOPMENT.md` and `VERIFICATION.md`.
- `R-009`: join/aggregation/sort sequencing language has been separated from the canonical execution architecture.
