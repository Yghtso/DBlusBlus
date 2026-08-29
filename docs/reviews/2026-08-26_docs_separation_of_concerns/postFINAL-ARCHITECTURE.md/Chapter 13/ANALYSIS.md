# Chapter 13 — CLEAN WITH OPTIONAL EDITORIAL NOTES

The Chapter 13 architecture is technically coherent, semantically complete, deterministic for every architecture-permitted crash prefix, time-independent, and consistent with Chapters 3–12.

No frozen semantic decision is required.

- BLOCKING: 0
- MAJOR: 0
- MINOR: 0
- EDITORIAL: 2
- FOLLOW-UP VERIFICATION GAPS: 1
- Files modified: none
- Phase 2: NOT STARTED / NOT AUTHORIZED

Primary scope: [docs/ARCHITECTURE.md — Chapter 13](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9890), through the line preceding Chapter 14.

## 1–9. Scope, structure, and counts

Context consulted:

- Chapter 3 lifecycle, ownership, READY, and failure gates
- Chapter 4 checksums, formats, namespaces, allocation, and exhaustion
- Chapters 5–12 in the requested chain
- Chapter 14 transaction-status retention and maintenance boundary
- Chapter 15 terminal/DML failure ownership
- Chapter 16 bootstrap/catalog validation
- §39 failure semantics
- §41 verification obligations
- [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2962)
- [docs/PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:1)
- [docs/DEVELOPMENT.md](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md:1)
- [AGENTS.md](/home/yghtso/Projects/DBlusBlus/AGENTS.md:1)

No source, tests, devlogs, or review artifacts were read.

### Actual Chapter 13 heading inventory

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 13 | Checkpointing and Crash Recovery | Recovery/checkpoint architecture | ARCHITECTURE-APPROPRIATE |
| 13.1 | Scope and recovery model | Analysis/redo/loser-resolution model | ARCHITECTURE-APPROPRIATE |
| 13.2 | Database control file | Global persistent recovery metadata | ARCHITECTURE-APPROPRIATE |
| 13.2.1 | Control-slot v1 byte layout | Exact control-slot codec | ARCHITECTURE-APPROPRIATE |
| 13.2.2 | Initial valid state | Canonical initial slots/high-water values | ARCHITECTURE-APPROPRIATE |
| 13.2.3 | Validation and slot selection | Slot validity, fallback, version handling | ARCHITECTURE-APPROPRIATE |
| 13.2.4 | Alternating update protocol | Torn-safe durable publication | ARCHITECTURE-APPROPRIATE |
| 13.2.5 | FileId allocation | Durable FileId high-water | ARCHITECTURE-APPROPRIATE |
| 13.2.6 | Catalog-object ID allocation | Shared durable catalog-ID high-water | ARCHITECTURE-APPROPRIATE |
| 13.3 | Control-file update frequency | Control-file write scope | ARCHITECTURE-APPROPRIATE |
| 13.4 | Fuzzy checkpoint goal | NO-FORCE checkpoint semantics | ARCHITECTURE-APPROPRIATE |
| 13.5 | Checkpoint identity and capture protocol | Checkpoint construction/installation | ARCHITECTURE-APPROPRIATE |
| 13.6 | CHECKPOINT_BEGIN payload | Exact BEGIN codec | ARCHITECTURE-APPROPRIATE |
| 13.7 | CHECKPOINT_DATA payload and dirty-page/writer entries | Exact DATA codec and capture contents | ARCHITECTURE-APPROPRIATE |
| 13.8 | CHECKPOINT_END payload and completeness validation | Exact END codec and completeness | ARCHITECTURE-APPROPRIATE |
| 13.9 | Checkpoint redo bound | Minimum reconstructible redo LSN | ARCHITECTURE-APPROPRIATE |
| 13.10 | WAL recycling | Installed recovery floor and segment deletion | ARCHITECTURE-APPROPRIATE |
| 13.11 | Recovery startup and WAL-tail validation | Inventory, valid prefix, tail classification | ARCHITECTURE-APPROPRIATE |
| 13.12 | Recovery phase 1: analysis | DPT, writer, terminal, and high-water reconstruction | ARCHITECTURE-APPROPRIATE |
| 13.13 | Recovery phase 2: redo | Redo entry and ordering | ARCHITECTURE-APPROPRIATE |
| 13.13.1 | Page redo | Page-family redo and owner deferral | ARCHITECTURE-APPROPRIATE |
| 13.13.2 | Terminal status redo | Terminal-WAL/status reconciliation | ARCHITECTURE-APPROPRIATE |
| 13.13.3 | BTREE_MTR atomicity | Atomic structural replay | ARCHITECTURE-APPROPRIATE |
| 13.14 | Torn/corrupt data pages during redo | Full-image reconstruction authority | ARCHITECTURE-APPROPRIATE |
| 13.15 | Recovery phase 3: loser resolution | Canonical ABORTED loser publication | ARCHITECTURE-APPROPRIATE |
| 13.16 | No ordinary user-DML CLRs | NO-UNDO/NO-CLR boundary | ARCHITECTURE-APPROPRIATE |
| 13.17 | Recovery of transaction-status pages | Terminal-WAL precedence | ARCHITECTURE-APPROPRIATE |
| 13.18 | Approximate/rebuildable metadata | Noncritical recovery exclusions | ARCHITECTURE-APPROPRIATE |
| 13.19 | Recovery completion gate | Exact pre-READY recovery obligations | ARCHITECTURE-APPROPRIATE |
| 13.20 | Crash-recovery correctness principle | Durable outcome preservation | ARCHITECTURE-APPROPRIATE |
| 13.21 | Recovery invariants | Canonical consolidated invariants | ARCHITECTURE-APPROPRIATE |

## 10. Section-by-section review

Abbreviations: `S` = sufficient/clear, `N/A` = not owned by that subsection, `Note` = optional editorial note.

| Section | Role | Time | Owner | Depth | Terms | Ctrl/CP | Tail | Redo | page_lsn | MTR | Status/loser | High-water | Retention | READY | Failure | Xref | Semantics | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 13.1 | Model | S | Correct | S | Note | N/A | N/A | S | S | S | S | N/A | N/A | N/A | S | S | S | CLEAN WITH NOTE |
| 13.2 | Control owner | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.2.1 | Slot codec | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.2.2 | Initial state | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.2.3 | Selection | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | S | S | S | S | S | CLEAN |
| 13.2.4 | Publication | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.2.5 | FileId | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.2.6 | Catalog IDs | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.3 | Update scope | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.4 | Fuzzy goal | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | N/A | S | N/A | S | S | S | CLEAN |
| 13.5 | Capture/install | S | Correct | S | S | S | N/A | N/A | N/A | N/A | S | S | S | S | S | S | S | CLEAN |
| 13.6 | BEGIN | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | S | N/A | N/A | S | S | S | CLEAN |
| 13.7 | DATA/DPT | S | Correct | S | S | S | N/A | N/A | S | N/A | S | S | S | S | S | S | S | CLEAN |
| 13.8 | END/complete | S | Correct | S | S | S | N/A | N/A | N/A | N/A | N/A | N/A | S | S | S | S | S | CLEAN |
| 13.9 | Redo bound | S | Correct | S | S | S | N/A | S | S | N/A | S | N/A | S | N/A | S | S | S | CLEAN |
| 13.10 | Recycling | S | Correct | S | S | S | S | N/A | N/A | N/A | S | N/A | S | N/A | S | S | S | CLEAN |
| 13.11 | Startup/tail | S | Correct | S | S | S | S | N/A | N/A | N/A | N/A | N/A | S | S | S | S | S | CLEAN |
| 13.12 | Analysis | S | Correct | S | S | S | S | S | S | S | S | S | S | N/A | S | S | S | CLEAN |
| 13.13 | Redo | S | Correct | S | S | N/A | N/A | S | S | S | S | N/A | N/A | N/A | S | S | S | CLEAN |
| 13.13.1 | Page redo | S | Correct | S | S | N/A | N/A | S | S | S | N/A | N/A | N/A | S | S | Note | S | CLEAN WITH NOTE |
| 13.13.2 | Status redo | S | Correct | S | S | S | N/A | S | S | N/A | S | N/A | S | S | S | S | S | CLEAN |
| 13.13.3 | MTR redo | S | Correct | S | S | N/A | N/A | S | S | S | S | N/A | N/A | S | S | S | S | CLEAN |
| 13.14 | Reconstruction | S | Correct | S | S | N/A | N/A | S | S | S | S | N/A | S | S | S | S | S | CLEAN |
| 13.15 | Losers | S | Correct | S | S | S | N/A | S | S | N/A | S | N/A | S | S | S | S | S | CLEAN |
| 13.16 | No CLR | S | Correct | S | S | N/A | N/A | S | N/A | N/A | S | N/A | N/A | N/A | S | S | S | CLEAN |
| 13.17 | Status pages | S | Correct | S | S | N/A | N/A | S | S | N/A | S | N/A | S | S | S | S | S | CLEAN |
| 13.18 | Rebuildable | S | Correct | S | S | N/A | N/A | S | N/A | N/A | N/A | N/A | N/A | S | S | S | S | CLEAN |
| 13.19 | READY gate | S | Correct | S | S | S | S | S | S | S | S | S | S | S | S | S | S | CLEAN |
| 13.20 | Correctness | S | Correct | S | S | N/A | S | S | S | S | S | N/A | N/A | S | S | S | S | CLEAN |
| 13.21 | Invariants | S | Correct | S | S | S | S | S | S | S | S | S | S | S | S | S | S | CLEAN |

## 11–13. Ownership, recovery entry, and every-open recovery

11. Chapter 13 owns recovery semantics without absorbing implementation progress or deterministic test recipes.

12. Recovery entry is exact:

- exclusive owner acquisition and root-adoption barrier precede inspection;
- control/bootstrap/WAL prerequisites are opened in `OPENING`;
- only recovery-scoped services exist in `RECOVERING`;
- ordinary transaction and background admission remains closed;
- `RECOVERING -> READY` is one publication point.

13. Every successful open performs WAL-tail validation, analysis, redo, loser resolution, status repair, and a recovery checkpoint. V1 has no clean-shutdown bit; a successful close can reduce work but cannot bypass recovery.

## 14–26. Control file and checkpoint assessment

### Control-file semantic/format table

| Offset | Width | Field | Encoding/invariant | Validation |
|---:|---:|---|---|---|
| 0 | 8 | magic | ASCII `DBLUSCTL` | exact |
| 8 | 2 | format version | LE uint16, `1` | `0` invalid; `>1` unsupported |
| 10 | 2 | header size | LE uint16, `88` | exact |
| 12 | 4 | flags | LE uint32, zero | nonzero corruption |
| 16 | 8 | generation | LE uint64, nonzero | monotonic; no wrap |
| 24 | 8 | checkpoint BEGIN LSN | LE uint64 or zero | checkpoint triplet consistency |
| 32 | 8 | checkpoint END LSN | LE uint64 or zero | checkpoint triplet consistency |
| 40 | 8 | redo LSN | LE uint64 or zero | matches complete checkpoint |
| 48 | 8 | reserved TxnId end | LE uint64, exclusive | at least 2; valid block authority |
| 56 | 8 | status reclaim cutoff | LE uint64 | aligned, bounded by reserved end |
| 64 | 4 | next FileId | LE uint32 | nonzero |
| 68 | 4 | reserved | zero | nonzero corruption |
| 72 | 8 | next catalog-object ID | LE uint64 | nonzero |
| 80 | 4 | CRC32C | LE uint32 | all 4096 bytes, field logically zero |
| 84 | 4 | reserved | zero | nonzero corruption |
| 88 | 4008 | reserved suffix | all zero | any nonzero byte corrupt |
| — | 4096 | slot size | two slots in 8192-byte file | exact |

14. Ownership: Chapter 13 owns `database.control`; Chapter 4 owns general format/exhaustion policy.

15. Format: exact 8192-byte file with two independently encoded 4096-byte slots.

16. Torn update: write the nonselected slot, increment generation, compute CRC, exact `pwrite`, `fdatasync`, then publish in memory. The old slot remains fallback.

17. Selection: highest usable supported generation. A checkpoint-bearing slot is usable only if its referenced checkpoint and recovery range validate. Equal-generation differing slots are corruption.

18. Future/corrupt control:

- exact magic plus version `>1`: `UNSUPPORTED_DATABASE_FORMAT`, no older fallback;
- version zero/wrong CRC/invalid v1 fields: invalid candidate;
- no valid supported slot: open cannot proceed;
- one independently valid slot is sufficient;
- wrong bootstrap/root identity is handled by Chapters 3, 4, and 16 rather than an invented control UUID.

### Checkpoint record table

| Record | Identity/fields | Purpose | Completeness role | Durability | Recovery meaning |
|---|---|---|---|---|---|
| CHECKPOINT_BEGIN | own LSN `B`; previous BEGIN; next TxnId; reserved end; next FileId | names checkpoint and captures allocator state | required | part of sequence flushed through END | analysis starts at `B` |
| CHECKPOINT_DATA | `B`, chunk index, DPT count, writer count, exact arrays | dirty-page and active-writer capture | zero or more; indexes exactly `0..N-1` | covered by END flush | initializes DPT/writer table |
| CHECKPOINT_END | `B`, DATA count, totals, redo LSN | closes and validates sequence | required | must be durable before control publication | proves complete sequence and redo bound |

19. Discovery: control slot identifies the installed BEGIN/END; recovery validates the corresponding WAL sequence.

20. Record family: exactly BEGIN, zero or more DATA, and END.

21. Completeness: exact BEGIN identity, END cross-link, contiguous chunk indexes, exact totals, valid codecs/CRC, and valid redo bound.

22. Partial crash semantics:

| State | Usable? | Selected control | Retention advance? | Crash result |
|---|---:|---|---:|---|
| No checkpoint | yes, full retained recovery | zero checkpoint fields | no | scan oldest required WAL |
| BEGIN only | no | prior installed slot | no | inert WAL record |
| Partial DATA | no | prior installed slot | no | partial sequence ignored |
| END appended but not durable | no | prior installed slot | no | uninstalled candidate |
| END durable, control not published | no as installed checkpoint | prior slot | no | complete inert sequence |
| Control write torn/unsynced | only if recovered slot independently validates | highest usable slot | only for selected slot | deterministic slot fallback |
| Control slot durable | yes | new generation | yes, after all dependencies | new checkpoint canonical |
| Runtime FPI publication pending | durable checkpoint remains canonical | new slot | yes | extra later images are conservative |

23. Checkpoint durability requires WAL through END before control publication.

24. Publication requires the alternating control-slot protocol and `fdatasync`.

25. Checkpoint state never competes with terminal WAL. Durable/surviving terminal evidence and persisted status remain transaction authority.

26. Recovery scan starts at checkpoint BEGIN for analysis and at `checkpoint_redo_lsn` for redo; without a checkpoint it starts from the oldest required retained WAL.

## 27–45. WAL inventory and valid-tail assessment

### WAL inventory table

| Inventory object | Rule | Invalid case | Result |
|---|---|---|---|
| Segment basename | 16 lowercase hexadecimal digits plus `.wal` | alternate spelling or unsafe object | not a v1 segment; never used as WAL |
| Segment size | exactly 67,108,864 bytes | short/malformed required segment | corruption/recovery failure |
| Segment 0 | bytes 0–7 zero; records start at 8 | nonzero reserved prefix | corruption |
| Later segment | usable stream offset starts at 0 | invented segment header | invalid implementation |
| Retained range | exact contiguous indexes from installed floor to tail | missing interior segment | corruption |
| Segment below floor | may be absent or re-unlinked | used as newer authority | forbidden |
| Exact next all-zero segment | permitted empty artifact | adopted without validation/resync | forbidden |
| Unexplained later segment | cannot bridge a gap or extend valid stream | treated as authoritative continuation | recovery failure/corrupt inventory |
| Unknown filesystem name | recorded/ignored | guessed as segment/deleted | forbidden |

### WAL-tail classification table

| Fixture | Physically complete? | CRC | Known type? | Grammar | Authoritative prefix? | Classification | READY? |
|---|---:|---|---:|---|---|---|---:|
| Clean unused zero tail | no record | N/A | N/A | N/A | preceding records only | clean tail | yes |
| Incomplete final header | no | unavailable | N/A | incomplete | preceding records only | torn/invalid final tail | yes after discard |
| Incomplete final payload/span | no | unavailable | yes candidate | incomplete | preceding records only | torn final tail | yes after discard |
| Complete-sized final CRC-invalid candidate | yes | invalid | candidate known | outer invalid | preceding records only | invalid/torn first unrequired tail | yes after discard |
| Malformed known-v1 final record | yes | valid | yes | invalid | cannot be accepted | corruption | no |
| Unknown complete final record | yes | valid | no | outer valid | semantic scan stops | `UNSUPPORTED_WAL_FORMAT` | no |
| Interior CRC failure before required history | yes | invalid | candidate | invalid | cannot establish prefix | corruption | no |
| Missing interior segment | N/A | N/A | N/A | N/A | gap | corruption/recovery failure | no |
| Exact all-zero next segment | zero artifact | N/A | N/A | empty | prior segment only | permitted empty tail | yes |
| Forbidden zero-count known record | yes | valid | yes | invalid v1 payload | cannot be accepted | corruption | no |

27. Segment inventory is exact, retained, and contiguous.

28. Allocated physical segment length, physical bytes, logical valid end, and durable record-start LSN are distinct.

29. Valid-prefix grammar checks start alignment, segment containment, header/length, zero padding, encoded LSN, flags/reserved, CRC, record registry, and payload grammar.

30. Incomplete final header: torn/invalid tail, excluded.

31. Incomplete final payload/span: torn final record, excluded without overread.

32. Complete final CRC-invalid candidate: excluded only as the first unrequired final suffix under §13.11.

33. Interior CRC failure: corruption, never tail truncation.

34. Malformed recognized-v1 record: corruption.

35. All forbidden zero-count PAGE_DELTA/BTREE_MTR/PATCH_SET records are complete malformed v1 WAL and therefore corruption.

36. Unknown complete type: `UNSUPPORTED_WAL_FORMAT`.

37. Future owning format: owning unsupported-format result, not v1 corruption.

38. Zero unused suffix does not become a record.

39. One exact next-contiguous all-zero segment is a permitted empty artifact.

40. A missing required interior segment is corruption.

41. Older below-floor segments may be absent/re-unlinked; an unexplained later segment cannot bridge a gap.

42. Recovery reconstructs the no-hole valid end and initial append position; empty WAL yields append end 8.

43. The fixed segment length is preserved; invalid suffix bytes are logically discarded/zeroed. No ordinary record is reused.

44. Surviving valid WAL is authoritative durability evidence. The recovery checkpoint subsequently makes the recovery-produced extension durable; ordinary work never observes an uninitialized durability state.

45. Ordinary append remains blocked until inventory, valid end, recovery, reconciliation, and READY complete.

## 46–58. Page validation and redo

### page_lsn decision table

| Page state | Comparison | Result |
|---|---|---|
| Invalid checksum/framing | untrusted | do not read `page_lsn`; reconstruct from retained full image or fail |
| Valid trusted page | `< record.lsn` | apply record and install record LSN |
| Valid trusted page | `== record.lsn` | already reflected; skip |
| Valid trusted page | `> record.lsn` | later state already reflected; skip older action |
| Missing authorized new page | N/A | reconstruct through PAGE_INIT/full-image publication |
| Missing required existing page without base | N/A | corruption/recovery failure |

### Redo table

| Family | Target validation | `<` | `==` | `>` | Checksum/rewrite | Atomicity |
|---|---|---|---|---|---|---|
| PAGE_DELTA | PageId, owner, type, published range; trusted base | apply ordered patches | skip | skip | install LSN, canonical checksum on write | one page action |
| PAGE_INIT | authorized new PageId/publication history | install full image and bound | skip | skip | image already canonical; validate result | new-page publication owner |
| PAGE_IMAGE | existing target/full-image authority | install image | skip | skip | validate full image | one page action |
| BTREE_MTR | all entries/owners decoded first | apply affected older/untrusted components | skip reflected component | skip older component | validate complete result set | one system action |
| TXN_STATUS image | system PAGE_INIT/PAGE_IMAGE | establish physical base | skip | skip | status page validation | image is not terminal authority |
| TXN_COMMIT/ABORT | TxnId mapping and reclaim cutoff | install exact terminal bit | skip if same-page serialization proves reflected | skip | set status `page_lsn=T` | semantic terminal authority |
| WAL_PAD/checkpoint | no page target | no page redo | N/A | N/A | N/A | framing/analysis only |

46. Pages undergo framing, owner, type, reserved-byte, and checksum validation before redo decisions.

47. A checksum-invalid page cannot contribute a trusted `page_lsn`.

48. `<`: redo applies.

49. `==`: redo skips idempotently.

50. `>`: the older action is skipped.

51. Interrupted recovery remains idempotent because every reconstructed page carries the authorizing LSN and every open repeats recovery.

52. PAGE_DELTA uses the trusted page/patch grammar; forbidden zero count never reaches redo.

53. PAGE_INIT reconstructs missing/short authorized new pages and their publication bound.

54. PAGE_IMAGE reconstructs an existing page; it does not substitute for PAGE_INIT’s new-page publication semantics.

55. BTREE_MTR is decoded and recovered as one system action.

56. Mixed MTR pages converge to complete new state: pages at/above MTR LSN are already reflected; older/untrusted pages receive their component.

57. A valid newer page LSN prevents old MTR overwrite without weakening atomicity, because same-page WAL order proves the later page state includes earlier history.

58. FSM redo remains valid, but stale/rebuildable FSM state may be reconstructed from authoritative heap state under Chapter 6; it is not silently treated as unlogged correctness state.

## 59–70. Transaction status, terminals, and losers

### TXN_STATUS recovery table

| Persisted prefix/state | Physical base | Terminal authority | Recovered result |
|---|---|---|---|
| F absent, T absent | prior trusted page or none | none | prior outcome or loser rules |
| F present, T absent | F | none | F creates no terminal status |
| T appended but absent from valid prefix | F/prior | none | loser or prior terminal result |
| Complete COMMIT T survives | F/trusted page | COMMIT T | COMMITTED |
| Complete ABORT T survives | F/trusted page | ABORT T | ABORTED |
| T survives, status page stale/unflushed | F/trusted page | T | repair exact terminal status |
| Status page torn with retained F | F | later T records | reconstruct then replay |
| Status page below reclaim cutoff | no page required | retired proof | do not recreate |

59. The F/T protocol is consistent: `rec_lsn=F`, current `page_lsn=T`.

60. F-only is physically useful but terminally inert.

61. Complete terminal WAL is authoritative over stale/missing status bytes.

62. Surviving COMMIT reconstructs COMMITTED even without client acknowledgement or C4.

63. Surviving ABORT reconstructs ABORTED.

64. A nonterminal persistent writer at analysis end is a recovery loser and becomes ABORTED.

65. No user-DML physical undo is required.

66. No user-DML CLR stream is required.

67. An aborted physical `xmax` may remain; Chapter 10 makes it ineffective and Chapter 14 may normalize it later.

68. Pre-crash terminal caches are runtime-only. Chapter 3 rebuilds coherent runtime terminal/catalog services before READY.

69. No pre-crash active transaction enters READY as active.

70. Read-only transactions leave no terminal WAL/status; their runtime existence disappears, while durable block reservation still prevents TxnId reuse.

### Terminal reconciliation matrix

| WAL evidence | Status bytes | Active/loser state | Recovered outcome | Repair? | Corruption? |
|---|---|---|---|---:|---:|
| COMMIT | INVALID/RESERVED | terminal | COMMITTED | yes | no |
| COMMIT | ABORTED | terminal | COMMITTED | yes | no; terminal WAL wins |
| COMMIT | COMMITTED | terminal | COMMITTED | idempotent | no |
| ABORT | INVALID/RESERVED | terminal | ABORTED | yes | no |
| ABORT | COMMITTED | terminal | ABORTED | yes | no; terminal WAL wins |
| No retained terminal | checkpoint/analysis writer remains nonterminal | loser | ABORTED | yes | no |
| No retained terminal | old COMMITTED/ABORTED, no active writer | prior persisted terminal history | preserve | no | no |
| No retained terminal | INVALID/RESERVED and required status-dependent reference | not active | unresolved | no | yes after recovery |
| Any | below retired cutoff | retired | RETIRED/status-independent | no page access | no if cutoff proof holds |

## 71–80. Allocators, catalog, and file ownership

### Allocator/high-water table

| Domain | Durable source | Crash gap? | Recovered next authority | Reuse? | Exhaustion owner |
|---|---|---:|---|---:|---|
| TxnId blocks | control `reserved_txn_id_end`; checkpoint BEGIN/observed IDs specialize analysis | yes | at least durable reserved end; unused suffix may be skipped | forbidden | §§4.3.2.1, 9.3 |
| FileId | control `next_file_id` | yes | selected durable next value | forbidden | §§4.3.2.1, 13.2.5 |
| Table/Index/Constraint IDs | control `next_catalog_object_id` | yes | selected durable next value | forbidden | §§4.3.2.1, 13.2.6 |
| PageNo/published bound | file length plus PAGE_INIT/MTR publication history | private tail may vanish | reconciled published contiguous bound | only owner-defined reuse | §§4.11, 12.12 |
| Control generation | selected valid slot | torn newer slot ignored | selected generation+1 | never wraps/reuses | §§4.3.2, 13.2.3 |
| Checkpoint identity | BEGIN record-start LSN | inert candidates possible | installed BEGIN LSN | no independent counter | WAL LSN exhaustion |

71. TxnId high-water is reconstructed from selected control/checkpoint information and observed WAL.

72. The durable control reservation end dominates any lower observed issued-ID prefix.

73. Every possibly reserved normal TxnId remains unavailable for reuse.

74. CommandId is transaction-local runtime state and is not recovered globally.

75. File/page bounds and allocator state are reconstructed through control, file length, PAGE_INIT, MTR, and append-tail reconciliation.

76. Authorized PAGE_INIT reconstructs the page and published bound together; no public uninitialized page appears.

77. B+ root, endpoints, height/generation, and free-list effects are recovered through one MTR, not independent metadata writes.

78. Catalog/system relations use ordinary WAL/page redo under bootstrap-assisted ownership.

79. Every required FileId, basename, FileKind, PageId, PageType, and object owner validates before publication.

80. Bootstrap and self-hosted catalog descriptors are fixed-point validated before READY.

## 81–101. Checkpoint state, retention, interrupted recovery, and READY

81. Checkpoint DPT entries contain exact WAL PageId and `rec_lsn`; writer entries contain TxnId and `last_wal_lsn`.

82. `rec_lsn` is the first complete full-image recovery base for the current dirty interval.

83. The checkpoint does not contain a generic ARIES transaction table; it contains the exact active-writer subset needed by this recovery model.

84. DATA is chunked by zero-based indexes and exact count/totals; missing, duplicate, or cross-linked chunks invalidate installation.

85. Checkpoint identity is its BEGIN LSN. There is no separate checkpoint-generation field.

86. Independent checkpoint-generation exhaustion is N/A; control generation and WAL LSN exhaustion are the owning domains.

87. Failure before durable control publication leaves the prior installed checkpoint authoritative.

88. Once the new control slot is durably selected, the new checkpoint is canonical; later cleanup cannot revert selection.

89. Uncertain/torn control publication is resolved by independent slot CRC/generation selection. Runtime continuation without known publication authority is forbidden.

### WAL-retention table

| Dependency | Earliest required LSN | Owner | Clears when | Consequence |
|---|---|---|---|---|
| Installed checkpoint sequence | BEGIN and all sequence segments through END | §§13.5–13.10 | later checkpoint safely installed | retain sequence |
| Dirty-page reconstruction | checkpoint minimum DPT `rec_lsn` | §§12.16, 13.7–13.10 | page durably clean/new checkpoint proof | retain from redo floor |
| Dirty TXN_STATUS page | preparatory image F=`rec_lsn` | §§12.10.5, 13.10, 14.14 | page durably clean and checkpoint permits | T alone cannot replace F |
| Later page/terminal changes | from each retained base forward | §§13.13–13.14 | durable clean/checkpoint proof | retain complete replay chain |
| Active writer chain | checkpoint `last_wal_lsn` plus post-BEGIN scan; no physical undo chain | §§13.7, 13.12, 13.15 | loser/terminal closure and recovery checkpoint | no independent ARIES undo floor |
| Retired status history | no page dependency below cutoff | §14.14 | durable cutoff proof | pages may be punched |
| Segment namespace removal | entire segment below every floor | §§13.10, 14 | unlink plus directory fsync | whole-segment deletion only |

90. Checkpoint installation alone does not permit deleting required reconstruction WAL.

91. A TXN_STATUS dirty page retains F even though its current `page_lsn` is T.

92. Ordinary dirty pages retain their initial full-image `rec_lsn`.

93. The lower bound is the installed checkpoint/redo minimum plus the whole installed sequence and every retained image dependency.

94. Whole-segment deletion belongs to Chapter 13’s floor and Chapter 14’s dependency release; namespace deletion requires directory synchronization.

95. A crash during recovery causes another complete open/recovery. Recovery writes already durably represented remain ordinary redo input.

96. Redone pages need not all be forced before READY; the required recovery checkpoint captures remaining dirty dependencies.

97. If redone pages remain dirty, retained WAL/full-image state and WAL-before-data make a second crash recoverable.

### READY-gate table

| Obligation | Owner | Before READY? | Failure |
|---|---|---:|---|
| Exclusive owner/root adoption established | Chapter 3/§4.7 | yes | open failure/noncontinuable by certainty |
| Valid control/checkpoint selected | §§13.2–13.8 | yes | no READY |
| Exact WAL inventory and valid tail | §13.11 | yes | no READY |
| Append position/durable-prefix state known | Chapters 12–13 | yes | no READY |
| Analysis complete | §13.12 | yes | no READY |
| All required/deferred redo complete | §§13.13–13.14 | yes | no READY |
| Losers terminally ABORTED and WAL durable | §13.15 | yes | no READY |
| TXN_STATUS safe for lookup | §§13.13.2, 13.17 | yes | no READY |
| MTR result sets structurally valid | §§13.13.3–13.14 | yes | no READY |
| TxnId/FileId/catalog high-water restored | §§13.2, 13.12 | yes | no READY |
| Bootstrap/catalog fixed point and file owners valid | Chapter 16/§3.3 | yes | no READY |
| Required append tails reconciled | §§4.11.3, 13.11 | yes | no READY |
| Recovery checkpoint installed | §§13.5–13.10, 13.15 | yes | no READY |
| Pending/orphan/final files classified | §§4.7.6, 13.19 | yes | no READY unless only durable cleanup remains |
| Runtime caches/registries/services coherent | §3.3.3 | yes | no READY |
| Normal background services held behind gate | §3.3.4 | yes | no READY |
| Atomic admission publication | §3.3.4 | publication itself | all ordinary work remains blocked |

98. The exact READY inventory is complete above.

99. READY is one atomic publication relative to transaction/background admission.

100. Known quiesceable recovery failure returns a failed open/CLOSED; uncertain publication or incomplete cleanup becomes NONCONTINUABLE with ownership retained.

101. Clean shutdown still executes recovery; the final checkpoint merely reduces work.

## 102–132. Runtime reset, target safety, failures, and concurrency

102. Pre-crash logical holders, waiters, queues, and graph edges are runtime state and are not replayed.

103. Historical redo does not rerun SQL UNIQUE admission.

104. Recovery replays physical WAL, not SQL text, binding, planning, or execution.

105. At READY, durable committers are COMMITTED, losers ABORTED, old snapshots gone, and only new runtime transactions can become active.

106. RESERVED is decoded as known nonterminal state; recovery must resolve a required reference or reject it.

107. INVALID is valid absence/nonterminal encoding, not a guessed terminal result.

108. Terminal WAL overrides stale opposing status bytes; an unresolved status-dependent reference after recovery is corruption.

109. The complete status conflict matrix appears above.

110. Every page-associated WAL record validates FileId, PageNo, FileKind/PageType, object ownership, published range, and format.

111. A short/missing page is reconstructible only where PAGE_INIT/full-image authority exists; otherwise it is corruption/I/O failure.

112. Authorized PAGE_INIT can justify extending/publishing a page beyond the stale bound. PAGE_DELTA cannot invent such authority.

113. Recovery file extension follows exact existing file identity and PAGE_INIT/MTR publication; it cannot create arbitrary missing files.

114. Root/file inventory follows Chapter 3/§4.7 ownership and the recovered catalog.

115. Recognizable future page/file formats are unsupported, not rewritten.

116. Malformed required v1 page/file state is corruption unless retained WAL explicitly reconstructs it.

117. Repairable state is stale/torn physical state with a retained full image and complete later WAL; unrecoverable state lacks that authority.

118. Full-page base selection is the DPT/dirty-interval `rec_lsn` or another architecturally applicable retained full image—not an arbitrary image.

119. Reconstruction installs the selected full image and then later deltas/MTRs in WAL order.

120. Redo scans in increasing WAL order, with each complete MTR treated as one action.

121. Parallel recovery is implementation freedom only if it preserves WAL dependencies, MTR atomicity, and publication ordering.

122. Valid-input memory exhaustion fails recovery/open; malformed persistent counts are corruption before allocation.

123. Required recovery disk/write/sync failure prevents READY. Known retryable state may permit later open; uncertainty may be noncontinuable.

124. WAL read failure in a required range is recovery failure, never tail.

125. Inability to establish control generation, checkpoint completeness, valid WAL end, reconstruction authority, or terminal outcome forbids READY.

126. Chapter 3 gives the exact CLOSED versus NONCONTINUABLE distinction based on whether state and cleanup are fully known.

127. A READY-database checkpoint failure leaves the old installed checkpoint authoritative when the failed candidate was not durably published. Fatal uncertainty follows §39.

128. Fuzzy checkpoint capture is synchronized at DPT and active-writer publication boundaries without requiring stop-the-world execution.

129. Checkpoint/page-flush races produce either captured dirty state or proven durable clean state; no dependency is forgotten.

130. Checkpoint/status mutation preserves the earlier F=`rec_lsn`, not the later T=`page_lsn`.

131. WAL appended after checkpoint BEGIN remains visible to analysis from BEGIN and retained after the installed redo floor.

132. Segment retention acts only after checkpoint/control publication and complete dependency proof.

## 133–144. Global documentation-model assessment

### Global assessment

| Question | Result |
|---|---|
| Analytical rather than chronological? | YES |
| Current-state narration? | NO |
| DEVELOPMENT sequencing leakage? | NO |
| VERIFICATION procedure leakage? | NO |
| PROJECT_STATE leakage? | NO |
| Devlog/history leakage? | NO |
| Correctness-relevant terminology ambiguity? | NO |
| Rationale sufficient? | YES |
| Readable without knowing implementation status? | YES |
| Timeless canonical v1 contract? | YES |
| Source-layout coupling? | NO |
| Implementation freedom preserved? | YES |

### Temporal-language classification

| Language family found | Class | Assessment |
|---|---|---|
| freshly initialized, during open/recovery, after synchronization | A — runtime ordering | valid |
| previous checkpoint, later WAL record, precrash unlink, subsequent restart | B — crash/WAL history | valid |
| future format/version/layout/subsystem | C — persistent-format evolution | valid |
| v1/no replication/PITR/no CLRs | D — durable v1 scope | valid |
| Chapter/section references | E — navigation | valid |
| initial implementation, later implementation, Phase 2, currently unsupported | F — project chronology | none |

### Document-ownership classification

| Material | Actual owner | Chapter 13 status |
|---|---|---|
| Recovery/checkpoint/control semantics | ARCHITECTURE | correctly present |
| Implementation sequencing/classes/source paths | DEVELOPMENT | absent |
| Crash fixture construction/fault hooks | VERIFICATION | absent except architectural protocol outcomes |
| Current recovery availability | PROJECT_STATE | absent |
| Historical audit/milestone evidence | devlog/reviews | absent |
| Contributor workflow | AGENTS | absent |

### Analytical-depth table

| Boundary | Assessment |
|---|---|
| Physical versus valid WAL end | ANALYTICALLY SUFFICIENT |
| Tail versus corruption versus unsupported | ANALYTICALLY SUFFICIENT |
| Control/checkpoint publication | ANALYTICALLY SUFFICIENT |
| Redo idempotence/page_lsn | ANALYTICALLY SUFFICIENT |
| MTR atomic recovery | ANALYTICALLY SUFFICIENT |
| TXN_STATUS F/T | ANALYTICALLY SUFFICIENT |
| Durable COMMIT precedence | ANALYTICALLY SUFFICIENT |
| Loser/no-undo model | ANALYTICALLY SUFFICIENT |
| Allocator high-water | ANALYTICALLY SUFFICIENT |
| WAL retention | ANALYTICALLY SUFFICIENT |
| READY publication | ANALYTICALLY SUFFICIENT |

### Terminology table

| Term | Canonical Chapter 13 meaning | Ambiguity |
|---|---|---|
| recovery | exclusive-owner reconstruction before READY | none |
| analysis | rebuild DPT/writers/terminals/high-water | none |
| redo | physical replay from redo bound using page LSN | none |
| valid WAL prefix | contiguous complete accepted records with no holes | none |
| valid end/tail | first position after last accepted record | none |
| torn/invalid tail | first unrequired incomplete/CRC-invalid suffix | none |
| corruption | required supported-v1 state violates grammar/inventory | none |
| unsupported | recognizable semantics beyond v1 | none |
| checkpoint | BEGIN/DATA/END plus durable control installation | none |
| rec_lsn | retained full-image base of current dirty interval | none |
| page_lsn | newest WAL action physically reflected in trusted page | none |
| recovery loser | persistent writer lacking surviving terminal evidence | none |
| reconciliation | repair persistent/status/ownership state from authority | none |
| high-water | durable exclusive allocator/next-value authority | none |
| READY | atomic publication after all recovery prerequisites | none |

### Normative-language table

| Section/evidence | Force | Assessment |
|---|---|---|
| 13.2.3 “MUST NOT wrap” | required | correct exhaustion rule |
| 13.7 “MUST NOT miss a clean-to-dirty transition” | required | correct checkpoint race rule |
| 13.13.1 “MUST NOT fail merely because … unresolved FileId” | required | correctly paired with later owner proof |
| 13.13.3 “MUST NOT acquire UNIQUE_KEY locks” | required | correct physical/SQL separation |
| 13.14 “MUST NOT guess … or trust a torn page’s LSN” | required | correct corruption rule |
| Declarative exact formats/protocols without keyword | normative by contract language | sufficiently definite |

## 145. Ownership-boundary table

| Responsibility | Canonical owner | Chapter 13 consumption |
|---|---|---|
| Open/READY/failure lifecycle | Chapter 3 | exact entry/gate |
| Checksums/formats/namespace/exhaustion | Chapter 4 | validation and classification |
| User-DML physical residue | Chapters 5/10 | no undo |
| FSM rebuildability | Chapter 6 | noncritical recovery |
| Dirty/writeback/WAL-before-data | Chapter 7 | DPT and redo durability |
| B+ structural semantics | Chapter 8 | MTR replay |
| Terminal lifecycle/TxnId/status | Chapter 9 | reconstruction/losers |
| Visibility | Chapter 10 | consumes recovered statuses |
| Runtime locks/UNIQUE | Chapter 11 | not replayed |
| WAL grammar/durable prefix | Chapter 12 | scanner/redo inputs |
| Checkpoint/recovery | Chapter 13 | primary owner |
| Status retirement/dependency release | Chapter 14 | retention boundary |
| SQL/DML failure outcomes | Chapter 15/§39 | consequence owner |
| Catalog bootstrap/fixed point | Chapter 16 | pre-READY validation |
| Procedures | VERIFICATION | correctly external |

## 146–160. Consolidated required matrices

The control, checkpoint-record, checkpoint-state, WAL-tail, WAL-inventory, redo, page-LSN, status, terminal, loser, allocator, READY, and retention tables appear above.

### Loser table

| Crash state | Recovered result | Physical undo? | Status action | Client interpretation | Locks replayed? |
|---|---|---:|---|---|---:|
| ACTIVE writer | ABORTED | no | recovery abort/status repair | failed/unknown work aborted | no |
| MUST_ABORT | ABORTED | no | recovery abort/status repair | transaction failed | no |
| COMMITTING, no surviving COMMIT | ABORTED | no | loser resolution | commit not established | no |
| COMMITTING, surviving COMMIT | COMMITTED | no | repair COMMITTED | success or uncertain, never aborted | no |
| ABORTING, ABORT survives | ABORTED | no | repair ABORTED | aborted | no |
| ABORTING, ABORT absent | ABORTED loser | no | recovery abort | aborted | no |
| Read-only active | no surviving runtime transaction | no | no terminal record required | no durable outcome fact | no |

### Failure matrix

| Failure | Result | Database state | Continue? | Retry/repair | Owner |
|---|---|---|---:|---|---|
| No valid supported control slot | open failure | CLOSED if quiesceable | no | repair/reopen | Chapters 3/13 |
| Future control format | unsupported | CLOSED | no | newer reader required | §4.14/13.2 |
| Incomplete checkpoint | old checkpoint/fallback or failure if pointed-to | recovery state | only through valid fallback | yes if old range valid | §§13.5–13.8 |
| WAL read I/O in required range | recovery failure | CLOSED/noncontinuable by certainty | no | later open possible if known | §§3.3,39 |
| Final torn tail | discard suffix | recoverable | yes after recovery | canonical reconciliation | §13.11 |
| Interior CRC/missing segment | corruption | open failure | no | external repair | §13.11 |
| Malformed recognized v1 | corruption | open failure | no | external repair | §§4.14,12,13 |
| Unknown complete WAL type | unsupported WAL | open failure | no | newer reader | §§4.14,13.11 |
| Repairable page checksum failure | reconstruct | RECOVERING | after successful validation | retained image + later WAL | §13.14 |
| Unrecoverable page corruption | recovery failure/corruption | no READY | no | external repair | §13.14 |
| Terminal/status mismatch | terminal WAL or loser analysis wins | RECOVERING | after repair | rewrite status | §§13.13.2,13.15 |
| Allocator reconstruction failure | recovery failure | no READY | no | restore authority | §§13.2,13.12 |
| Checkpoint publication known failure | old slot remains authority | READY may continue if running checkpoint | yes where state known | retry | §§13.2.4,13.5 |
| Checkpoint publication uncertainty | noncontinuable/reopen selection | NONCONTINUABLE | no ordinary work | restart/recovery | Chapters 3/39 |
| Valid-input resource exhaustion | open/recovery failure | no READY | no | resource/retry | §39 |
| READY prerequisite failure | open failure | CLOSED or NONCONTINUABLE | no | certainty-dependent | §3.3 |

### Concurrency/ordering matrix

| Race | Required ordering | Legal result | Forbidden result |
|---|---|---|---|
| Checkpoint vs new WAL | BEGIN precedes forward analysis range | new records recovered after snapshot | omitted post-capture WAL |
| Checkpoint vs dirty flush | DPT transition synchronization | captured dirty or durably clean | missing dependency |
| Checkpoint vs status mutation | preserve F=`rec_lsn` | F retained, T replayed | replacing F with T |
| Checkpoint publication vs crash | WAL END durable before control slot | old or complete new checkpoint | partial checkpoint selected |
| Redo vs interrupted recovery | page-LSN idempotence | repeat/skip | double logical effect |
| Redo vs newer page | valid `page_lsn>` skips old action | retain later state | overwrite with old record |
| Recovery vs transaction admission | READY publication | admission after full recovery | partial-state observation |
| Recovery vs lock manager | new runtime coordination only after recovery | empty fresh state | pre-crash lock replay |
| Retention vs checkpoint | installed control plus dependency proof first | whole old segment deletion | deleting required image/checkpoint WAL |

## 161. Cross-chapter consistency matrix

| Owner | Result | Chapter 13 relation |
|---|---|---|
| Chapter 3 | CONSISTENT BUT SPECIALIZED | recovery entry/READY/failure |
| Chapter 4 | CONSISTENT BUT SPECIALIZED | formats, checksum, namespace, exhaustion |
| Chapter 5 | CONSISTENT | physical loser bytes may remain |
| Chapter 6 | CONSISTENT BUT SPECIALIZED | FSM advisory/rebuildable |
| Chapter 7 | CONSISTENT BUT SPECIALIZED | DPT, WAL-before-data, dirty pages |
| Chapter 8 | CONSISTENT BUT SPECIALIZED | MTR atomicity |
| Chapter 9 | CONSISTENT BUT SPECIALIZED | terminal authority, loser, high-water |
| Chapter 10 | CONSISTENT | recovered visibility |
| Chapter 11 | CONSISTENT | no SQL lock/UNIQUE replay |
| Chapter 12 | CONSISTENT BUT SPECIALIZED | exact WAL grammar and durable prefix |
| Chapter 14 | CONSISTENT BUT SPECIALIZED | status cutoff and retention release |
| Chapter 15 | CONSISTENT | terminal/DML failure boundary |
| §39 | CONSISTENT BUT SPECIALIZED | open/noncontinuable/client uncertainty |
| §41 | CONSISTENT | verification obligations |

## 162. Explicit Chapter 13 cross-reference audit

| Source | Target | Purpose | Exists/owner | Precision | Status |
|---|---|---|---|---|---|
| 13.2.3 | §4.14.6 | control version dispatch | yes/correct | exact | clean |
| 13.2.5 | §4.3.2.1 | FileId exhaustion | yes/correct | exact | clean |
| 13.5 | §4.3.2 | checked checkpoint fields | yes/correct | exact enough | clean |
| 13.5 | §§3.3.3, 3.3.6 | recovery/close checkpoints | yes/correct | exact | clean |
| 13.7 | §12.16 | `rec_lsn` semantics | yes/correct | exact | clean |
| 13.7 | §7.10.5 | DPT capture race | yes/correct | exact | clean |
| 13.11 | §§3.3.2–3.3.3 | ownership/startup order | yes/correct | exact | clean |
| 13.11 | §4.7 | namespace safety | yes/correct | exact owner | clean |
| 13.11 | §13.19 | completion gate | yes/correct | exact | clean |
| 13.11 | §4.11.3 | append-tail reconciliation | yes/correct | exact | clean |
| 13.11 | §4.3.2.4 | WAL end arithmetic | yes/correct | exact | clean |
| 13.11 | §§12.2, 12.2.1 | segment grammar/creation | yes/correct | exact | clean |
| 13.11 | §§12.12, 12.12.4 | valid end/uncertainty | yes/correct | exact | clean |
| 13.11 | §§4.14, 12.7 | unknown-record classification | yes/correct | correct | clean |
| 13.12 | §12.10.5 | status full-image DPT | yes/correct | exact | clean |
| 13.13.1 | Chapter 12 | page codecs/page-LSN rules | yes/correct | broad | optional note |
| 13.13.1 | §4.7.6 | orphan target proof | yes/correct | exact | clean |
| 13.13.2 | §§12.10.5, 9.12 | status base/mapping | yes/correct | exact | clean |
| 13.13.3 | §§4.11.3, 11.10 | tails/no UNIQUE replay | yes/correct | exact | clean |
| 13.14 | §§12.10.5, 7.6.4, 4.13 | reconstruction/validation | yes/correct | exact | clean |
| 13.15 | §§12.10.5, 3.3.3 | loser status/ownership gate | yes/correct | exact | clean |
| 13.17 | §§12.10.5, 15.5 | status protocol/acknowledgement | yes/correct | exact | clean |
| 13.19 | §§3.3.3–3.3.4 | READY publication | yes/correct | exact | clean |
| 13.20 | §§4.7, 12.2.1 | namespace durability | yes/correct | exact owners | clean |
| 13.20 | §39.1.5 | client outcome uncertainty | yes/correct | exact | clean |
| 13.21 | §11.10 | UNIQUE authorization | yes/correct | exact | clean |

## 163–164. Terminology and normative tables

Provided under sections 133–144 above.

## 165. 100-item technical consistency matrix

| # | Item | Result | Basis |
|---:|---|---|---|
| 1 | Recovery lifecycle owner | CONSISTENT | Ch.3/§13.11–13.19 |
| 2 | Recovery every open | CONSISTENT | §3.3.3 |
| 3 | Control owner | CONSISTENT | §13.2 |
| 4 | Control layout | CONSISTENT | §13.2.1 |
| 5 | Torn-write protocol | CONSISTENT | §13.2.4 |
| 6 | Slot selection | CONSISTENT | §13.2.3 |
| 7 | Future control | CONSISTENT | §§4.14.6,13.2.3 |
| 8 | Checkpoint discovery | CONSISTENT | control pointer + WAL validation |
| 9 | Record family | CONSISTENT | §§13.6–13.8 |
| 10 | Completeness | CONSISTENT | §13.8 |
| 11 | Durability | CONSISTENT | §13.5 |
| 12 | Control publication | CONSISTENT | §§13.2.4,13.5 |
| 13 | Partial checkpoint | CONSISTENT | §§13.5–13.8 |
| 14 | Start LSN | CONSISTENT | §§13.9,13.12–13.13 |
| 15 | Segment inventory | CONSISTENT | §13.11 |
| 16 | Physical vs valid end | CONSISTENT | §§12.2–12.3,13.11 |
| 17 | Valid-prefix grammar | CONSISTENT | §13.11 |
| 18 | Incomplete header | CONSISTENT | §13.11 |
| 19 | Incomplete payload | CONSISTENT | §13.11 |
| 20 | Final CRC-invalid | CONSISTENT BUT SPECIALIZED | first unrequired final tail |
| 21 | Interior CRC | CONSISTENT | corruption |
| 22 | Malformed known v1 | CONSISTENT | §§4.14,12,13 |
| 23 | Unknown complete | CONSISTENT | unsupported WAL |
| 24 | Future WAL format | CONSISTENT | owning unsupported result |
| 25 | Zero tail | CONSISTENT | §§12.3,13.11 |
| 26 | All-zero next segment | CONSISTENT | §§12.2.1,13.11 |
| 27 | Missing interior segment | CONSISTENT | corruption |
| 28 | Extra segments | CONSISTENT BUT SPECIALIZED | floor/empty-next rules |
| 29 | Valid-end reconstruction | CONSISTENT | §13.11 |
| 30 | Tail cleanup | CONSISTENT | logical discard/zero |
| 31 | Reopen durability state | CONSISTENT BUT SPECIALIZED | surviving prefix + recovery checkpoint |
| 32 | Append before READY | CONSISTENT | §§3.3,13.19 |
| 33 | Page validation | CONSISTENT | §§4.13,13.14 |
| 34 | Checksum before LSN | CONSISTENT | §13.14 |
| 35 | page_lsn `<` | CONSISTENT | apply |
| 36 | page_lsn `==` | CONSISTENT | skip |
| 37 | page_lsn `>` | CONSISTENT | skip older |
| 38 | Idempotence | CONSISTENT | page LSN/full image |
| 39 | PAGE_DELTA | CONSISTENT | §§12.8,13.13 |
| 40 | PAGE_INIT | CONSISTENT | §§12.9,13.13–13.14 |
| 41 | PAGE_IMAGE | CONSISTENT | §§12.9,13.14 |
| 42 | BTREE_MTR | CONSISTENT | §13.13.3 |
| 43 | Mixed MTR states | CONSISTENT | per-page reflected/older rule |
| 44 | Newer page in old MTR | CONSISTENT | skip newer trusted page |
| 45 | FSM recovery | CONSISTENT BUT SPECIALIZED | Ch.6/§13.18 |
| 46 | Status preparatory image | CONSISTENT | §12.10.5 |
| 47 | Terminal authority | CONSISTENT | §§13.13.2,13.17 |
| 48 | F/T LSN split | CONSISTENT | §§12.10.5,13.10 |
| 49 | COMMIT reconstruction | CONSISTENT | §§13.17,13.20 |
| 50 | ABORT reconstruction | CONSISTENT | §§13.15,13.17 |
| 51 | Recovery loser | CONSISTENT | §13.15 |
| 52 | No DML undo | CONSISTENT | §§13.1,13.15 |
| 53 | No DML CLR | CONSISTENT | §13.16 |
| 54 | Aborted xmax | CONSISTENT | Ch.10/§14.13 |
| 55 | Terminal cache reset | CONSISTENT | §3.3.3 |
| 56 | Active registry reset | CONSISTENT | Ch.3/9 |
| 57 | Read-only crash | CONSISTENT | §9.15 |
| 58 | TxnId high-water | CONSISTENT | §§9.3,13.2,13.12 |
| 59 | TxnId nonreuse | CONSISTENT | §§4.3.2.1,9.3 |
| 60 | CommandId recovery | N/A | transaction-local runtime state |
| 61 | Page high-water | CONSISTENT | §§4.11,13.11 |
| 62 | PAGE_INIT/bound | CONSISTENT | §§4.11,12.12,13 |
| 63 | B+ metadata | CONSISTENT | MTR |
| 64 | Catalog/system redo | CONSISTENT | ordinary page WAL |
| 65 | File owner validation | CONSISTENT | §§3.3,4.7,13.19 |
| 66 | Catalog validation | CONSISTENT | §§3.3,16.5 |
| 67 | Checkpoint DPT | CONSISTENT | §13.7 |
| 68 | rec_lsn | CONSISTENT | §§12.16,13.7 |
| 69 | Checkpoint transaction table | CONSISTENT BUT SPECIALIZED | active-writer table |
| 70 | Chunking | CONSISTENT | §§13.7–13.8 |
| 71 | Checkpoint generation | N/A | identity is BEGIN LSN |
| 72 | Checkpoint-generation exhaustion | N/A | WAL/control domains own exhaustion |
| 73 | Prepublication failure | CONSISTENT | old checkpoint authority |
| 74 | Postpublication failure | CONSISTENT | new durable slot authority |
| 75 | Checkpoint uncertainty | CONSISTENT | slots/reopen/noncontinuable |
| 76 | Checkpoint vs commit | CONSISTENT | terminal WAL/status authority |
| 77 | Checkpoint vs retention | CONSISTENT | §§13.9–13.10 |
| 78 | Status F retention | CONSISTENT | §§12.10.5,13.10 |
| 79 | Ordinary dirty retention | CONSISTENT | DPT rec_lsn |
| 80 | Retention lower bound | CONSISTENT | §13.10 |
| 81 | Segment deletion owner | CONSISTENT | §13.10/Ch.14 |
| 82 | Interrupted recovery | CONSISTENT | every-open/idempotence |
| 83 | Recovery writeback | CONSISTENT BUT SPECIALIZED | fuzzy recovery checkpoint |
| 84 | Page durability before READY | CONSISTENT | dirty+DPT+retained WAL |
| 85 | READY prerequisites | CONSISTENT | §§3.3.4,13.19 |
| 86 | READY publication | CONSISTENT | §3.3.4 |
| 87 | READY failure | CONSISTENT | §§3.3.4,39 |
| 88 | Clean shutdown open | CONSISTENT | §3.3.3 |
| 89 | No lock replay | CONSISTENT | runtime reset/Ch.11 |
| 90 | No UNIQUE replay | CONSISTENT | §13.13.3 |
| 91 | No SQL reexecution | CONSISTENT | physical redo model |
| 92 | Recovered visibility | CONSISTENT | Ch.10/§13.20 |
| 93 | RESERVED recovery | CONSISTENT | §§9.11.1,13.15 |
| 94 | INVALID recovery | CONSISTENT | §§9.13,13.15 |
| 95 | Stale terminal reconciliation | CONSISTENT | terminal WAL/loser authority |
| 96 | Target-owner validation | CONSISTENT | §§4.7.6,13.13.1 |
| 97 | Repairable/unrecoverable page | CONSISTENT | §13.14 |
| 98 | Redo order | CONSISTENT | forward WAL scans |
| 99 | Resource exhaustion | CONSISTENT | §§4.3.2,39 |
| 100 | Implementer invention | CONSISTENT | none correctness-relevant required |

## 166. Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | CONSISTENT |
| 2 | Valid runtime/crash time preserved | CONSISTENT |
| 3 | No current implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No DEVELOPMENT sequencing | CONSISTENT |
| 6 | No VERIFICATION recipe leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT |
| 9 | No source-layout coupling | CONSISTENT |
| 10 | Physical/valid end terminology | CONSISTENT |
| 11 | Torn/corrupt/unsupported distinction | CONSISTENT |
| 12 | Redo/replay terminology | CONSISTENT |
| 13 | Checkpoint authority | CONSISTENT |
| 14 | rec_lsn/page_lsn terminology | CONSISTENT, optional spelling note |
| 15 | Terminal WAL/status authority | CONSISTENT |
| 16 | Loser rationale | CONSISTENT |
| 17 | Checkpoint rationale | CONSISTENT |
| 18 | READY rationale | CONSISTENT |
| 19 | Retention rationale | CONSISTENT |
| 20 | Independent of implementation status | CONSISTENT |

## 167–170. Findings

### BLOCKING findings

None.

### MAJOR findings

None.

### MINOR findings

None.

### EDITORIAL findings

#### E13-1 — `recLSNs` spelling differs from canonical field spelling

- Section: §13.1
- Evidence: “dirty-page recLSNs”
- Severity: EDITORIAL
- Type: TERMINOLOGY
- Scope: local
- Arithmetic: N/A
- Explanation: later normative text consistently uses persisted/runtime field spelling `rec_lsn`.
- Canonical comparison: §§12.16, 13.7, 13.9–13.10.
- Consequence: no semantic ambiguity; only minor glossary/search inconsistency.
- Correct owner: ARCHITECTURE.
- Future action: H. TERMINOLOGY NORMALIZATION.
- Suggested result: use “dirty-page `rec_lsn` values.”

#### E13-2 — Broad Chapter 12 page-redo cross-reference

- Section: §13.13.1
- Evidence: “apply the page-LSN/full-image rules from Chapter 12.”
- Severity: EDITORIAL
- Type: CROSS-REFERENCE
- Scope: local
- Arithmetic: N/A
- Explanation: the named record families make the meaning recoverable, but exact references would improve navigation.
- Canonical comparison: §§12.8–12.10.3 and §12.17, with §4.8.2 for page LSN.
- Consequence: none for correctness; only navigation precision.
- Correct owner: ARCHITECTURE.
- Future action: G. CROSS-REFERENCE FIX.

## 171. Frozen architecture semantic questions

None.

Every requested correctness choice is determinable from Chapter 13 plus its canonical owners. No durable commit can reverse, no MTR can partially publish, no required WAL can be silently skipped, no reserved TxnId can be reused, and READY cannot occur with unresolved recovery authority.

## 172. Follow-up verification gaps

One procedural gap remains:

### V13-1 — Independent byte-exact `database.control` slot fixture

Architecture is complete, but [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:237) does not yet give the 4096-byte control slot the same explicit independent byte-oracle treatment now given to WAL records.

Existing verification already covers:

- torn control writes;
- one/two/no valid slot selection;
- generation fallback;
- high-water crash behavior;
- unsupported future control format;
- checkpoint installation ordering.

The missing explicit fixture should independently construct and validate:

- every §13.2.1 offset and little-endian field;
- exact 4096-byte size and 8192-byte two-slot placement;
- bytes 80–83 logically zero during CRC32C;
- the entire 4008-byte zero suffix;
- initial slot-0/zero slot-1 state;
- equal-generation same/different-content cases;
- one-defect mutations for each field, CRC, flag, reserved range, high-water relation, checkpoint triplet, and future version.

Classification: FOLLOW-UP VERIFICATION GAP, not an architecture finding.

## 173. Out-of-scope observations

No Chapter 13 finding was assigned to later-owner material.

The Chapter 14 review should focus on the actual boundary now exposed by Chapter 13:

- §14.14 status-retention proof and page-aligned cutoff;
- status F=`rec_lsn` dependency release;
- sparse page reclamation;
- §14.17 StatusHistoryGuard/reclaimer concurrency;
- checkpoint versus status-cutoff publication;
- vacuum normalization/freezing proof;
- whole-segment retention/deletion interaction;
- persistent DEAD/read-epoch restart semantics.

Known deferred wording in §14.17, §§15.7.2–15.7.3, §31.7, and Appendix C was not reviewed as Chapter 13 content and was not modified.

## 174–198. Direct ambiguity and documentation answers

| # | Question | Answer |
|---:|---|---|
| 174 | Control-file selection ambiguity? | NO |
| 175 | Checkpoint-completeness ambiguity? | NO |
| 176 | Valid-WAL-end ambiguity? | NO |
| 177 | Final-tail classification ambiguity? | NO |
| 178 | Malformed/unsupported/torn conflation? | NO |
| 179 | Redo/page_lsn ambiguity? | NO |
| 180 | Checksum-before-page_lsn violation? | NO |
| 181 | BTREE_MTR partial replay path? | NO |
| 182 | Durable-COMMIT reversal path? | NO |
| 183 | Status page competing with terminal WAL? | NO |
| 184 | Loser-resolution ambiguity? | NO |
| 185 | Accidental user-DML physical undo? | NO |
| 186 | TxnId reservation reuse path? | NO |
| 187 | Checkpoint/retention dependency gap? | NO |
| 188 | READY prerequisite ambiguity? | NO |
| 189 | Pre-crash lock replay? | NO |
| 190 | Correctness-relevant invention required? | NO |
| 191 | Project-time/current-state wording? | NO |
| 192 | DEVELOPMENT-owned material? | NO |
| 193 | VERIFICATION procedure leakage? | NO |
| 194 | PROJECT_STATE-owned material? | NO |
| 195 | Devlog/history material? | NO |
| 196 | Correctness-relevant ambiguous terminology? | NO |
| 197 | Analytically underexplained correctness boundary? | NO |
| 198 | Timeless canonical v1 contract? | YES |

## 199–202. Regression, compatibility, and next action

199. Previous-chapter regression: PASS. Chapter 13 consumes the closed Chapters 3–12 without reopening or contradicting them.

200. Chapter 12 compatibility: PASS. Exact WAL grammar, durable-prefix meaning, append authorization, F/T protocol, nonzero mutation counts, MTR atomicity, PAGE_INIT, and WAL-before-data are preserved.

201. Recommended next action: verification synchronization limited to the byte-exact `database.control` fixture/mutation matrix. The two editorial notes are optional and do not block proceeding.

202. Recommended Chapter 14 review scope: status-history dependency proof, cutoff publication, status-page physical reclamation, WAL-retention release, vacuum normalization/freezing, StatusHistoryGuard concurrency, persistent DEAD restart handling, and checkpoint/retention races.

## 203–210. Repository and phase guarantees

### Git state

| Check | Initial | Final |
|---|---|---|
| `git status --short` | empty/clean | empty/clean |
| `git diff --cached --name-only` | empty | empty |
| `git rev-parse HEAD` | `641c81df6d41a5f6b6e395ab47dbc1ff63e70680` | same |
| `git diff --check` | N/A at initial capture | PASS, no output |

203. Files modified by audit: NONE.

204. Initial status/index/HEAD: clean index and worktree at `641c81df6d41a5f6b6e395ab47dbc1ff63e70680`.

205. Final status/index/HEAD: unchanged and clean at the same HEAD.

206. `git diff --check`: PASS.

207. Repository-state-change assessment: no audit-created or external change appeared during the review.

208. No audit-created repository change occurred.

209. No implementation, build, test, benchmark, source audit, staging, commit, devlog, or review-artifact work occurred.

210. Phase 2 remains NOT STARTED / NOT AUTHORIZED.