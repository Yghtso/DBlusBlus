## 1–9. Verdict, scope, and counts

1. **Chapter-5 verdict:** **CHAPTER 5 — TARGETED DOCUMENT FIXES RECOMMENDED**

2. **Primary scope read:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:2755), lines 2755–3854, from `# 5. Heap Storage and Tuple Format` through the line before Chapter 6.

3. **Actual heading inventory:** 49 headings including the Chapter 5 heading.

| Section | Exact heading | Canonical responsibility |
|---|---|---|
| 5 | Heap Storage and Tuple Format | Heap-page and tuple-format contract |
| 5.1 | Scope and storage model | Relation heap/FSM files and page organization |
| 5.2 | Heap scan order | Physical page/slot traversal order |
| 5.3 | HEAP_DATA page format v1 | Common-header specialization |
| 5.3.1 | Heap-specific header layout | Header fields and geometry |
| 5.3.2 | Free-slot-list field | Canonical reusable-slot chain |
| 5.3.3 | Reserved and hint fields | Zero fields and non-authoritative prune hint |
| 5.4 | Slot directory format | Slot bytes and state registry |
| 5.4.1 | NORMAL slots | Live tuple-coordinate form |
| 5.4.2 | UNUSED and REDIRECT_RESERVED slots | Reusable and known-unsupported forms |
| 5.4.3 | DEAD slots and physical reclamation | Retained/reclaimed DEAD forms |
| 5.4.4 | Stable SlotId | Compaction identity guarantee |
| 5.5 | Free-space geometry and page compaction | Page regions, insertion cost, compaction |
| 5.6 | Maximum inline tuple size | Inline raw tuple-length bound |
| 5.7 | Tuple header v1 | Fixed tuple-header bytes |
| 5.7.1 | xmin | Creator identity |
| 5.7.2 | xmax | Invalidator/deleter identity |
| 5.7.3 | cmin and cmax | Command-order metadata |
| 5.7.4 | Previous-version pointer | Same-heap backward version link |
| 5.8 | Tuple flags v1 | Tuple flag registry |
| 5.8.1 | HAS_VARLEN | Schema/layout-derived varlen flag |
| 5.8.2 | Deferred tuple flags | Forbidden unassigned flags |
| 5.9 | Canonical tuple body layout | Header/bitmap/fixed/varlen ordering |
| 5.9.1 | Exact tuple length | No-trailing-byte rule |
| 5.10 | Null bitmap | Bitmap size and per-column ownership |
| 5.10.1 | Persisted bit meaning | NULL/present encoding |
| 5.10.2 | Persisted bit ordering | LSB-first mapping |
| 5.10.3 | Nullability metadata | NOT NULL validation |
| 5.10.4 | Unused bitmap bits | Canonical zero tail bits |
| 5.10.5 | HAS_NULLS | Flag/bitmap equivalence |
| 5.10.6 | Fixed-area bytes beneath NULL | Writer canonicalization/reader minimum |
| 5.11 | Fixed-width physical values | Scalar widths and encodings |
| 5.11.1 | BOOLEAN | Exact Boolean byte domain |
| 5.11.2 | Signed scalar representations | Two’s-complement encoding |
| 5.11.3 | FLOAT64 | Exact binary64 payload preservation |
| 5.12 | VARCHAR representation | Inline descriptor and payload |
| 5.12.1 | NULL VARCHAR | Canonical `(0,0)` descriptor |
| 5.12.2 | Present VARCHAR packing | Consecutive schema-order payloads |
| 5.12.3 | Present empty VARCHAR | Empty-versus-NULL distinction |
| 5.12.4 | Payload semantics | Opaque storage-layer bytes |
| 5.13 | Schema versioning | Historical schema resolution |
| 5.14 | INSERT protocol boundary | Cross-subsystem insertion outline |
| 5.15 | UPDATE protocol boundary | New physical version/new RID |
| 5.16 | DELETE protocol boundary | Logical deletion before reclamation |
| 5.17 | MVCC visibility boundary | Heap mechanics versus visibility |
| 5.18 | HeapPage representation boundary | Non-owning page-format view |
| 5.19 | TupleCodec boundary | Schema-directed tuple serialization |
| 5.20 | Tuple views and execution decoding | Borrowed-view lifetime |
| 5.21 | Heap and tuple invariants | Consolidated Chapter-5 invariants |

4. **Context-only architecture consulted:** §§4.3–4.6, 4.8–4.10, 4.13–4.15; §§6.1–6.4 and 6.10–6.13; §§7.5–7.7 and 7.10–7.11; §§8.4 and 8.23–8.25; §§9.2–9.6; §§12.9–12.12; §§13.13–13.14; §§14.4–14.12; §§15.1–15.4 and 15.7–15.9; §§16.6–16.8; §§39.1–39.3; §§41.1 and 41.3; Appendix A.

5. **Other live documents consulted:** `AGENTS.md`, relevant `PROJECT_STATE.md` owner-validation mismatch, and the relevant storage/reclamation procedures in `VERIFICATION.md`.

6. **BLOCKING:** 0
7. **MAJOR:** 0
8. **MINOR:** 4
9. **EDITORIAL:** 1

## 10. Section-by-section review

Legend: `✓` clear, `—` not owned, `N` clean note, `F` finding. Columns correspond to timelessness, byte layout, identity/RID, slot/tuple, validation, mutation, WAL/recovery, reclamation, normative language, cross-references, rationale, and semantic consistency.

| Section | Role | T | Byte | RID | Slot/tuple | Val | Mut | WAL | Reclaim | Norm | Xref | Why | Sem | Status |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 5 | Chapter boundary | F | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | F | N | ✓ | ✓ | FINDING |
| 5.1 | Storage model | ✓ | — | — | — | ✓ | — | — | — | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.2 | Scan order | ✓ | — | N | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.3 | Page format | ✓ | ✓ | N | ✓ | ✓ | — | N | — | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.3.1 | Heap header | ✓ | ✓ | — | ✓ | ✓ | ✓ | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.3.2 | Free-slot chain | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | N | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.3.3 | Reserved/hint | N | ✓ | — | — | ✓ | ✓ | — | N | ✓ | — | ✓ | ✓ | CLEAN WITH NOTE |
| 5.4 | Slot format | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | — | ✓ | ✓ | CLEAN |
| 5.4.1 | NORMAL | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.4.2 | UNUSED/REDIRECT | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.4.3 | DEAD | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | N | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.4.4 | Stable SlotId | ✓ | — | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | — | ✓ | ✓ | CLEAN |
| 5.5 | Geometry/compaction | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.6 | Inline limit | F | N | — | ✓ | ✓ | ✓ | — | — | F | — | ✓ | ✓ | FINDING |
| 5.7 | Tuple header | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | N | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.7.1 | xmin | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | ✓ | — | N | ✓ | CLEAN |
| 5.7.2 | xmax | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | — | N | ✓ | CLEAN |
| 5.7.3 | cmin/cmax | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | F | ✓ | ✓ | FINDING |
| 5.7.4 | Previous link | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | — | ✓ | ✓ | CLEAN |
| 5.8 | Flags | ✓ | ✓ | — | ✓ | ✓ | ✓ | N | N | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.8.1 | HAS_VARLEN | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.8.2 | Deferred flags | N | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN WITH NOTE |
| 5.9 | Body layout | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.9.1 | Exact length | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10 | Null bitmap | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10.1 | Bit meaning | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10.2 | Bit ordering | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10.3 | Nullability | ✓ | ✓ | — | ✓ | F | — | — | — | F | — | ✓ | ✓ | FINDING |
| 5.10.4 | Tail bits | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10.5 | HAS_NULLS | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.10.6 | NULL fixed bytes | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.11 | Fixed scalars | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.11.1 | BOOLEAN | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.11.2 | Signed scalars | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.11.3 | FLOAT64 | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.12 | VARCHAR | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.12.1 | NULL VARCHAR | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.12.2 | Packing | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.12.3 | Empty VARCHAR | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.12.4 | Payload semantics | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.13 | Schema version | F | ✓ | N | ✓ | ✓ | — | — | ✓ | F | ✓ | ✓ | ✓ | FINDING |
| 5.14 | INSERT boundary | ✓ | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | F | ✓ | ✓ | FINDING |
| 5.15 | UPDATE boundary | ✓ | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | F | ✓ | ✓ | FINDING |
| 5.16 | DELETE boundary | ✓ | — | ✓ | ✓ | ✓ | ✓ | N | ✓ | ✓ | — | ✓ | ✓ | CLEAN |
| 5.17 | Visibility boundary | ✓ | — | ✓ | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.18 | HeapPage boundary | ✓ | — | ✓ | ✓ | ✓ | ✓ | N | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.19 | TupleCodec boundary | ✓ | ✓ | — | ✓ | ✓ | — | — | — | ✓ | — | ✓ | ✓ | CLEAN |
| 5.20 | Borrowed views | ✓ | — | ✓ | ✓ | ✓ | — | — | — | ✓ | ✓ | ✓ | ✓ | CLEAN |
| 5.21 | Invariants | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | CLEAN |

## 11–20. Ownership, page layout, and slots

11. **Ownership boundary:** Chapter 5 owns HEAP_DATA specialization, slots, tuple bytes/codecs, physical identity behavior, and local mutation boundaries. Chapter 4 owns common framing and structural validation; Chapters 6, 7, 12–15 own FSM, residency, WAL, recovery, and global reclamation.

12. **Heap-page layout:** Internally consistent and fully decodable.

13. **Common-header inheritance:** Exact 32-byte Chapter-4 header, with HEAP_DATA specialization requiring type 1, version 1, header size 48, zero flags/reserved16, expected PageNo.

14. **Heap-header arithmetic:** `32 + 16 = 48` bytes.

15. **Slot-entry arithmetic:** `2 + 2 + 2 + 2 = 8` bytes.

16. **Structural slot capacity:**

```text
floor((8192 - 48) / 8)
= floor(8144 / 8)
= 1018 descriptors
```

17. **SlotId:** Legal allocated ordinals `0..1017`; `1018..65534` are representable but structurally impossible; `65535` is invalid.

18. **Slot states:** Exactly `UNUSED=0`, `NORMAL=1`, `DEAD=2`, `REDIRECT_RESERVED=3`; other codes are corrupt.

19. **State machine:** Complete when combined with Chapter 14. No direct `DEAD -> NORMAL`, `NORMAL -> UNUSED`, or direct DEAD reuse exists.

20. **Canonical non-live forms:** UNUSED is `(offset=0,length=0,aux=next/sentinel)`; reclaimed DEAD is `(0,0,aux=0)`. Retained DEAD must retain a complete valid tuple and `aux=0`.

## 21–24. RID and version identity

21. **RID:** Physical tuple-version identity `(heap FileId, PageNo, SlotId)`, never logical row identity.

22. **UPDATE:** Creates a complete new physical tuple version and new RID. User tuple bytes are not replaced in place.

23. **Reuse:** Permitted only after exact index cleanup, persistent DEAD, read-epoch grace, required version-chain splicing, and canonical `DEAD -> UNUSED`.

24. **Index safety:** Compaction preserves SlotId/RID; index entries may remain for NORMAL invisible/aborted versions, but must be absent before DEAD publication.

## 25–36. Geometry and mutation

25. **Tuple region:** Header `[0,48)`, slots `[48,lower)`, immediate free gap `[lower,upper)`, retained tuples/holes within `[upper,8192)`.

26. **Tuple extents:** Checked nonempty range entirely inside `[upper,8192)`.

27. **Tuple overlap:** Every retained NORMAL/DEAD range must be pairwise disjoint.

28. **Directory overlap:** Retained tuple ranges cannot intersect `[0,lower)`.

29. **Free space:** `upper-lower` means contiguous immediately available space, not holes plus reclaimable DEAD payload.

30. **FSM:** Consistent. It categorizes that contiguous gap conservatively assuming a new 8-byte slot; it remains advisory.

31. **Insertion space:**

- reusable slot: tuple bytes only;
- new slot: tuple bytes plus 8;
- fragmented candidate: optional compaction;
- insufficient post-compaction capacity: page-local `NO_SPACE`.

32. **Insert mutation/failure:** Free-list pop/slot installation is one page mutation. Chapter 12 guarantees old or complete WAL-backed new state, never half-publication.

33. **DELETE:** Only sets `xmax/cmax`; physical reclamation and slot reuse are separate.

34. **Compaction:** May move NORMAL/retained-DEAD bytes and discard DEAD payload, but preserves SlotIds and final L1 validity.

35. **Fragmentation:** Holes are valid. Dense packing is compactor output, not a reader-validity requirement.

36. **Page initialization:** Common HEAP_DATA header; slot count 0; free head `0xFFFF`; `lower=48`; `upper=8192`; prune hint and reserved fields zero. With WAL, PAGE_INIT supplies PageId/type/page_lsn and complete checksum-valid image before publication.

## 37–43. Owner validation and validation order

37. **PageNo validation:** Page 0 and `INVALID_PAGE_NO` are invalid for HEAP_DATA; expected/stored PageNo and published range are checked.

38. **FileKind/PageType:** Registered HEAP file plus HEAP_DATA ordinary page is required.

39. **FileId:** Supplied by the safely opened registered file and expected PageId; cross-checked with superblock/catalog ownership.

40. **Object owner:** HEAP superblock `object_id=TableId`, catalog descriptor, deterministic namespace mapping, and registered validator context jointly establish ownership.

41. **Owner chain:** Complete; checks need not reside in one function.

42. **Ordinary versus recovery:** Ordinary publication requires L0/L1 and available L2. Recovery may hold torn bytes privately, but reconstructed pages must pass normal validation before RESIDENT/READY.

43. **Safe validation order:**

1. exact transfer/framing;
2. family/version discrimination;
3. v1 checksum;
4. expected FileKind/PageType and common header;
5. PageId/published-bound/owner checks;
6. heap header and checked directory geometry;
7. slot states/free-list;
8. checked retained extents and pairwise non-overlap;
9. fixed tuple header;
10. historical schema resolution and schema-directed tuple validation.

`page_lsn` is not trusted before checksum acceptance.

## 44–56. Tuple format

44. **Tuple-format ownership:** Chapter 5 owns physical bytes; schema descriptors supply immutable resolved layout metadata.

45. **Tuple header:** Exact and gap-free.

46. **Header arithmetic:** `8+8+4+4+8+2+2+2+2+4+4 = 48`.

47. **Minimum tuple size:**

```text
48
+ ceil(column_count / 8)
+ Σ fixed_width(column)
```

A valid zero-column physical schema gives the absolute minimum: **48 bytes**.

48. **Maximum tuple size:** Field representation is wider, but the v1 writer accepts at most **8135 complete encoded tuple bytes**. The geometric meet-at-boundary value is 8136 and is deliberately rejected.

49. **MVCC representation:** Widths and sentinel rules match Chapters 4 and 9.

50. **Flags/reserved:** Known mask `0x0003`; tuple reserved bytes 44–47 zero; unknown bits rejected.

51. **Null bitmap:** Exact `ceil(N/8)`, LSB-first, one bit per physical column, unused high bits zero.

52. **Variable layout:** Absolute tuple-relative uint32 descriptors; canonical schema-order packing; no gaps/overlap/reordering/trailing bytes.

53. **Schema/Layout boundary:** Clear. TupleCodec consumes resolved physical layout and does not own SQL meaning.

54. **SchemaVer:** uint32, zero invalid through Chapter 4, v1 writer emits 1, historical schema must remain resolvable.

55. **Malformed tuple behavior:** Deterministic under §4.13.3, except for one localized wording ambiguity in §5.10.3.

56. **Slot/tuple cross-validation:** Complete via §4.13.3.

## 57–65. WAL, recovery, reclamation, and errors

57. **WAL/MTR:** All persistent page mutations use the canonical §12.12 provisional/publication contract.

58. **page_lsn/checksum:** page_lsn installs with published WAL mutation; stable copied writeback recomputes checksum after final bytes and enforces WAL-before-data.

59. **PAGE_INIT:** Target PageId, expected PageType, full image, embedded page_lsn, checksum, owner context, and publication bound are all covered.

60. **Recovery target:** FileId, PageNo, FileKind, PageType, and descriptor/object identity are validated across WAL, file registry, and reconstructed L0/L1/L2 checks.

61. **Write validity:** Private provisional bytes are allowed only behind exclusive/no-flush ownership. Final externally usable/writeback-eligible state must validate.

62. **Vacuum/reclamation:** Chapter 5 does not authorize premature reuse; Chapter 14 supplies the full gate.

63. **Slot-directory shrink:** No v1 mutation authorizes it. Compaction preserves `slot_count`; reclamation converts slots rather than removing ordinals.

64. **Empty/reusable page:** A page with only non-live slots is not necessarily the canonical blank image. Whole-page reinitialization that reuses old RIDs remains grace-gated.

65. **Error domains:** `NO_SPACE`, `ROW_TOO_LARGE`, `PAGE_NUMBER_EXHAUSTED`, `RESOURCE_FULL`, `WAL_POSITION_EXHAUSTED`, corruption, and unsupported state remain conceptually distinct. §5.6 should name its oversized-tuple result more precisely.

## 66–74. Document quality and external cross-checks

66. **Temporality:** Two localized project-sequencing findings in §§5.6 and 5.13. Other occurrences of “current,” “later,” and “initial” describe runtime state, version evolution, or cross-reference context.

67. **Terminology:** Generally precise. “Raw tuple payload” and “row-too-large/unsupported” in §5.6 should be normalized to complete encoded tuple length and the canonical outcome.

68. **Normative language:** Semantic strength is consistent; no MUST-level invariant disappears in summaries.

69. **Document ownership:** No detailed test recipe, benchmark procedure, source-layout mandate, or development diary is embedded.

70. **Source-layout coupling:** None. Architectural component names such as `HeapPage` and `TupleCodec` are legitimate boundaries; exact methods/files are expressly non-normative.

71. **Implementation freedom:** Preserved for in-memory representation, helper decomposition, compaction mechanics, containers, and concrete APIs.

72. **Analytical depth:** Sufficient, especially for stable RIDs, delayed reuse, tuple canonicality, advisory FSM behavior, and non-owning views.

73. **PROJECT_STATE owner-validation question:** **ARCHITECTURE CONTRACT CLEAR.** The recorded HEAP validation deficiency is implementation noncompliance, not an architecture gap.

74. **VERIFICATION:** Existing owners are `Storage Verification` → `Slotted-page tests`/`Tuple codec tests`, plus WAL/MTR, PAGE_INIT, recovery, and vacuum sections. No follow-up verification gap was established.

## 75. Heap-page header table

All integer fields are little-endian.

| Field | Offset | Width | Meaning/domain | Writer/mutation owner | Validation |
|---|---:|---:|---|---|---|
| page_type | 0 | 2 | `HEAP_DATA=1` | Initialization | Exact expected type |
| format_version | 2 | 2 | `1` | Initialization | Zero corrupt; greater unsupported |
| flags | 4 | 4 | zero | Initialization | Reject nonzero |
| page_lsn | 8 | 8 | newest WAL-protected mutation | WAL/page mutation | Trust only after checksum |
| checksum_crc32c | 16 | 4 | whole-page CRC32C | Stable flush/full image | Verify before page_lsn |
| header_size | 20 | 2 | 48 | Initialization | Exact |
| reserved16 | 22 | 2 | zero | Initialization | Reject nonzero |
| page_no | 24 | 8 | expected ordinary PageNo | Initialization | `>=1`, non-sentinel, expected/published |
| slot_count | 32 | 2 | `0..1018` | Slot-directory growth | Checked geometry |
| free_slot_head | 34 | 2 | slot `<slot_count` or `0xFFFF` | Reclamation/insertion | Exact complete UNUSED chain |
| lower | 36 | 2 | `48+8*slot_count` | Slot growth | Exact formula |
| upper | 38 | 2 | tuple-region start | Insert/compaction | `lower<=upper<=8192` |
| prune_hint | 40 | 4 | non-authoritative; any bits valid | Initialized zero | Cannot waive checks |
| heap reserved | 44 | 4 | zero | Initialization | Reject nonzero |

## 76. Slot-entry table

| Field | Offset | Width | Meaning | Validation |
|---|---:|---:|---|---|
| tuple_offset | 0 | 2 | Tuple start or zero | State-dependent |
| tuple_length | 2 | 2 | Complete tuple length or zero | State-dependent; max 8135 retained |
| slot_flags/state | 4 | 2 | Codes 0–3 | Unknown corrupt |
| aux | 6 | 2 | Zero or UNUSED next link | State-dependent |
| Total |  | **8** |  | No gaps/overlap |

## 77. Slot-state table

| State | Code | Coordinates | Bytes referenced | RID disposition | Reusable | Validation |
|---|---:|---|---|---|---|---|
| UNUSED | 0 | `(0,0)` | No | Old RID not dereferenceable | Yes | Exactly once in free list |
| NORMAL | 1 | Nonzero valid extent | Complete tuple | Query/vacuum physical identity | No | Tuple valid; `aux=0` |
| DEAD retained | 2 | Nonzero valid extent | Complete tuple | Retired; diagnostic/vacuum only | No | Same tuple checks; `aux=0` |
| DEAD reclaimed | 2 | `(0,0)` | No | Retired identity remains protected | No | `aux=0` |
| REDIRECT_RESERVED | 3 | No v1 interpretation | Must not inspect | Unsupported | No | `UNSUPPORTED_RESERVED_STATE` |
| Other | — | No interpretation | No | Invalid | No | `CORRUPT_HEAP` |

## 78. Slot-transition table

| From | To | Trigger/precondition | WAL/MTR | RID/reuse result |
|---|---|---|---|---|
| No slot | NORMAL | New directory entry and tuple fit | One page mutation | New RID |
| UNUSED | NORMAL | Canonical free-list head popped | One page mutation | Safely reused ordinal, new version |
| NORMAL | NORMAL | `xmax/cmax` or other header mutation | WAL page mutation | Same physical version |
| NORMAL | DEAD | Garbage eligibility and exact index cleanup | WAL page mutation | Retired, not reusable |
| DEAD retained | DEAD reclaimed | Compaction discards payload | WAL page mutation | RID still protected |
| DEAD reclaimed | UNUSED | Grace plus version-link proof | One page redo mutation | Becomes reusable |
| Any valid state | Same/final valid state | Recovery replay | Owning WAL record | No new transition semantics |

## 79. Tuple-header table

All fields are little-endian.

| Field | Offset | Width | Domain/meaning | Validation |
|---|---:|---:|---|---|
| xmin | 0 | 8 | Frozen or normal TxnId | Structural TxnId domain |
| xmax | 8 | 8 | 0 or normal TxnId | Frozen is not a deleter |
| cmin | 16 | 4 | CommandId, zero legal | uint32 domain |
| cmax | 20 | 4 | CommandId, zero legal | uint32 domain |
| prev_page_no | 24 | 8 | Prior PageNo or invalid | Sentinel pair, same heap |
| prev_slot | 32 | 2 | Prior SlotId or invalid | Sentinel pair and target checks |
| tuple_flags | 34 | 2 | Known mask `0x0003` | Reject unknown bits |
| header_bytes | 36 | 2 | 48 | Exact |
| null_bitmap_bytes | 38 | 2 | `ceil(column_count/8)` | Exact schema match |
| schema_version | 40 | 4 | Nonzero SchemaVer | Historical descriptor required |
| reserved | 44 | 4 | zero | Reject nonzero |

## 80. Tuple flags/reserved table

| Material | Value/rule | Writer | Reader |
|---|---|---|---|
| HAS_NULLS | `0x0001`, iff a used NULL bit exists | Exact | Reject mismatch |
| HAS_VARLEN | `0x0002`, iff schema contains VARCHAR | Exact | Reject mismatch |
| Other flag bits | Unassigned | Never emit | Reject |
| Tuple reserved 44–47 | Zero | Write zero | Reject nonzero |
| Unused bitmap high bits | Zero | Write zero | Reject nonzero |
| Fixed bytes under NULL fixed scalar | Writer-zero canonical | Write zero | Nonzero tolerated but semantically unread |
| NULL VARCHAR descriptor | `(0,0)` | Exact | Reject other form |

## 81. Byte-arithmetic table

| Layout/calculation | Stated | Derived | Result |
|---|---:|---:|---|
| Common header | 32 | `2+2+4+8+4+2+2+8=32` | Correct |
| Heap-specific header | 16 | `2+2+2+2+4+4=16` | Correct |
| Heap total header | 48 | `32+16=48` | Correct |
| Slot entry | 8 | `2+2+2+2=8` | Correct |
| Structural descriptors | 1018 | `floor((8192-48)/8)=1018` | Correct |
| Maximum SlotId | 1017 | `1018-1` | Correct |
| `lower` at max slots | 8192 | `48+1018*8=8192` | Correct |
| Tuple header | 48 | field sum 48 | Correct |
| VARCHAR descriptor | 8 | `4+4=8` | Correct |
| Null bitmap | formula | `ceil(N/8)` | Correct |
| Absolute minimum tuple | 48 | zero-column schema | Correct |
| Max live min-size tuples | — | `floor(8144/(48+8))=145` | Structural distinction |
| Geometric one-slot tuple | 8136 | `8192-48-8` | Correct |
| Accepted one-slot tuple | 8135 | deliberate one-byte stricter limit | Correct specialization |
| One-VARCHAR minimum | — | `48+1+8=57` | Correct |
| One-VARCHAR maximum payload | — | `8135-57=8078` | Correct |

No unintended gaps or overlaps exist in fixed layouts.

## 82. Heap-region geometry

| Region | Range | Growth | Meaning |
|---|---|---|---|
| Common header | `[0,32)` | Fixed | Common framing |
| Heap header | `[32,48)` | Fixed | Heap geometry |
| Slot directory | `[48,lower)` | Upward | `slot_count*8` |
| Contiguous free gap | `[lower,upper)` | Shrinks | Immediate capacity |
| Tuple region | `[upper,8192)` | Downward/compacted | Retained tuples plus legal holes |

## 83. Tuple-extent validation

| Check | Required oracle |
|---|---|
| Coordinate addition | Checked before comparison |
| Nonempty retained extent | `tuple_length>0` |
| Lower bound | `tuple_offset>=upper` |
| End bound | checked end `<=8192` |
| Header/directory exclusion | No intersection with `[0,lower)` |
| Pairwise ownership | No NORMAL/DEAD overlap |
| Minimum decoding | Slot length at least schema-derived minimum |
| Header validation | Complete fixed header before body access |
| Body validation | Bitmap/fixed/varlen exact under historical schema |
| Exact length | Final varlen cursor equals slot length |

## 84. RID/reclamation table

| State | RID lookup valid? | Bytes may remain? | Index may still reference? | Reusable? | Prerequisite |
|---|---|---|---|---|---|
| NORMAL visible/invisible | Yes, subject to MVCC | Yes | Yes | No | N/A |
| NORMAL garbage candidate | Vacuum-only candidate | Yes | Yes until exact cleanup | No | Global garbage eligibility |
| DEAD retained | Not query-returnable | Yes | Required exact entries absent | No | Index cleanup completed |
| DEAD reclaimed | Not query-returnable | No owned bytes | Required exact entries absent | No | Still awaiting grace/splicing |
| UNUSED | No old tuple lookup | No | No | Yes | Grace and version-chain proof |
| REDIRECT_RESERVED | No | No interpretation | No valid use | No | Unsupported |

## 85. Free-space/FSM consistency

| Situation | Heap authority | FSM representation/result |
|---|---|---|
| New slot needed | gap must cover tuple+8 | Exact conservative assumption |
| UNUSED reusable slot | gap need cover tuple only | FSM may understate capacity |
| Fragmented holes | Compaction may create gap | FSM may remain stale |
| DEAD payload discarded | Final geometry authoritative | Category repaired later |
| Stale high estimate | Heap rejects candidate safely | Repair/retry |
| Stale low estimate | Page may be overlooked | Repair/rebuild |
| FSM update failure | Heap mutation remains correct | Advisory metadata stale |

## 86. Owner-validation table

| Dimension | Expected source | Persisted where? | Validator | Before ordinary publication? | Failure |
|---|---|---|---|---|---|
| FileKind | Registered file/superblock | Page 0 | File/PageFile + BufferPool | Yes | Corruption/load failure |
| FileId | Registered file, catalog mapping | Superblock; PageId context | Registry/BufferPool | Yes | Wrong-owner/corruption |
| Table owner | TableDescriptor | Superblock `object_id` | Registered heap owner | Yes | Corrupt/missing object |
| PageNo | Requested PageId/published bound | Common header | L0/BufferPool | Yes | Corrupt page |
| PageType | Expected HEAP_DATA | Common header | L0 dispatch | Yes | Corrupt/unsupported |
| Published bound | File length + WAL authority | Runtime reconstructed | Registered file owner | Yes | Out-of-range |
| Schema history | TableId/SchemaVer descriptor | Tuple schema_version/catalog | Heap L1 owner | Yes | No weaker publication |

## 87. Mutation/WAL table

| Operation | Preconditions | Page changes | RID effect | WAL/FSM/recovery |
|---|---|---|---|---|
| Initialize page | New private PageNo/owner | Complete header/blank geometry | No tuple RID yet | PAGE_INIT; validate before publication |
| Insert/new slot | Encoded tuple fits | Tuple, slot, count/lower/upper | New RID | WAL page mutation; FSM afterward |
| Insert/reused slot | Canonical UNUSED head | Tuple, slot, free head/upper | New version at safe reused RID | One mutation; FSM afterward |
| UPDATE new version | Locks/revalidation | New tuple/slot | New RID | Heap redo before index MTR |
| UPDATE/DELETE old header | Revalidated NORMAL | `xmax/cmax` | Same old physical RID | WAL page mutation |
| NORMAL→DEAD | Index entries absent | Slot state | Retires RID | WAL; restart preserves DEAD |
| Compact | Exclusive latch; valid page | Tuple bytes, offsets, upper | Stable | WAL page mutation; FSM repair |
| DEAD→UNUSED | Grace/splicing complete | State, aux, free head | Enables later reuse | One redo unit; FSM update |

## 88. Malformed-state table

| Structure | Malformation | Detection owner | Before | Classification |
|---|---|---|---|---|
| Page framing | Short transfer | L0 | Header use | Corrupt/I/O |
| Common header | Wrong type/version/header | L0 | Heap parsing | Corrupt or unsupported version |
| Checksum | CRC mismatch | L0 | page_lsn trust | Corrupt page |
| Owner | Wrong FileId/Table/PageNo | L0/L2 | RESIDENT publication | Corrupt/wrong owner |
| Heap geometry | Count/product/range invalid | Heap L1 | Slot access | CORRUPT_HEAP |
| Free list | Range/cycle/duplicate/missing UNUSED | Heap L1 | Publication | CORRUPT_HEAP |
| Slot state | Unknown code | Heap L1 | Payload access | CORRUPT_HEAP |
| Slot state | REDIRECT_RESERVED | Heap L1 | Payload interpretation | UNSUPPORTED_RESERVED_STATE |
| NORMAL/DEAD | Noncanonical `aux`/coordinates | Heap L1 | Tuple access | CORRUPT_HEAP |
| Extent | Overflow/out of page/directory intersection | Heap L1 | Dereference | CORRUPT_HEAP |
| Extents | Pairwise overlap | Heap L1 | Publication | CORRUPT_HEAP |
| Tuple header | Short, wrong header size/reserved | Tuple codec/L1 | Body decode | CORRUPT_HEAP |
| MVCC/link | Invalid TxnId/sentinel pair/self-link | Heap L1/L2 | Follow/use | CORRUPT_HEAP |
| Flags | Unknown or HAS_* mismatch | Tuple codec/L1 | Body use | CORRUPT_HEAP |
| Bitmap | Wrong size/tail bits/NOT NULL violation | Tuple codec/L1 | Value use | Canonically CORRUPT_HEAP |
| Scalar | Invalid BOOLEAN | Tuple codec/L1 | Value publication | CORRUPT_HEAP |
| VARCHAR NULL | Descriptor not `(0,0)` | Tuple codec/L1 | Payload use | CORRUPT_HEAP |
| VARCHAR present | Overflow/gap/overlap/reorder/backward offset | Tuple codec/L1 | Payload dereference | CORRUPT_HEAP |
| Tuple length | Trailing or truncated bytes | Tuple codec/L1 | Publication | CORRUPT_HEAP |
| Schema | Required descriptor unavailable | Registered owner | RESIDENT publication | Load/open failure; never weak parse |

## 89. Cross-chapter consistency matrix

| Owner/consumer | Chapter-5 relationship | Result |
|---|---|---|
| Chapter 4 | IDs, RID, common header, L0–L3 validation | Consistent |
| Chapter 6 | Contiguous gap, slot-cost category, compaction | Consistent |
| Chapter 7 | Registered validation, guards, dirty publication | Consistent |
| Chapter 8 | Persisted physical RID and delayed index cleanup | Consistent |
| Chapter 9 | TxnId/CommandId representation | Consistent |
| Chapter 12 | PAGE_INIT, heap-before-index, mutation publication | Consistent |
| Chapter 13 | Recovery-private reconstruction and target checks | Consistent |
| Chapter 14 | DEAD/grace/UNUSED and version-chain safety | Consistent |
| Chapter 15 | Exact INSERT/UPDATE/DELETE integration | Consistent |
| Chapter 16 | Immutable/historical schema descriptors | Consistent |
| Chapter 39 | Error and transaction consequences | Consistent |
| Chapter 41 | Storage/recovery/reclamation obligations | Consistent |
| Appendix A | Format registry values | Consistent; repeats §5.6 “payload” terminology |

## 90. Explicit cross-reference table

| Source | Target/purpose | Exists/owner | Precision/status |
|---|---|---|---|
| §5.1 | Chapter 6 FSM format | Yes | Precise enough |
| §5.3 | Chapter 4 common header | Yes | Precise enough |
| §5.3.2 | Chapter 14 safe slot reuse | Yes | Broad but clear |
| §5.4.2 | §5.3.2 UNUSED link | Yes | Precise |
| §5.4.2 | §4.13.3 REDIRECT | Yes | Precise |
| §5.4.3 | §4.13.3 retained tuple validation | Yes | Precise |
| §5.4.3 | Chapter 14 DEAD→UNUSED | Yes | Broad but clear |
| §5.5 | §5.4.3 DEAD compaction | Yes | Precise |
| §5.7.3 | “MVCC/transaction chapters” | §§9.6–9.16 | Exists; imprecise |
| §5.8 | §4.14 version compatibility | Yes | Precise |
| §5.13 | Chapter 16 ResolveSchema | §16.7 | Exists; named contract makes intent clear |
| §5.14 | “later WAL/BufferPool transaction protocol” | §§7.10, 12.11–12.12, 15.2 | Exists; imprecise |
| §5.15 | “later transaction/durability chapters” | Chapters 9, 12, 15, 39 | Exists; imprecise |
| §5.18 | §7.7.2 guard lifetime | Yes | Precise |
| §5.21 | §4.13.3 canonical L1 | Yes | Precise |

## 91. Terminology table

| Term | Canonical reading | Assessment |
|---|---|---|
| RID | Physical tuple-version identity | Clear |
| Free space | Current contiguous `[lower,upper)` gap | Clear |
| Reclaimable space | Space obtainable through safe compaction | Clear by context |
| DEAD | Retired but non-reusable identity | Clear |
| UNUSED | Grace-authorized reusable slot | Clear |
| “Raw tuple payload” | Intended complete encoded tuple bytes | Ambiguous wording |
| “Unsupported outcome” in §5.6 | Intended oversized-row rejection | Should use canonical category |
| SchemaVer | Historical descriptor identity, not tuple-format discriminator | Clear |
| REDIRECT_RESERVED | Recognized but unsupported state | Clear |
| “Current” | Runtime cursor/gap/transaction | Legitimate |
| “First implementation” | Project-sequencing wording | Finding |

## 92. Normative-language table

| Area | Normative rule | Strength result |
|---|---|---|
| Header/slot bytes | Exact widths, codes, zero fields | Consistent MUST |
| NORMAL/DEAD extents | Complete validation/non-overlap | Consistent MUST |
| UNUSED chain | Complete, acyclic, exact membership | Consistent MUST |
| REDIRECT_RESERVED | Writers forbid; readers reject | Consistent MUST |
| Compaction | Stable SlotIds and DEAD rules | Consistent MUST/MAY |
| Tuple size | 8136 rejected | Strong and consistent |
| Oversized outcome | “row-too-large/unsupported” | Local clarity finding |
| Tuple flags | Unknown bits rejected | Consistent MUST |
| Bitmap | Formula/order/canonical bits | Consistent MUST |
| VARCHAR | Exact canonical packing | Consistent MUST |
| DML outlines | Conceptual; later owners canonical | No strength conflict |
| Views | Guard lifetime required | Consistent MUST |
| Schema v1 scope | Semantics clear, temporality wording weak | Local wording finding |

## 93. Fifty-item high-priority consistency matrix

| # | Item | Result |
|---:|---|---|
| 1 | Chapter-5 ownership boundary | CONSISTENT |
| 2 | Common-header inheritance | CONSISTENT BUT SPECIALIZED |
| 3 | Heap-header size | CONSISTENT |
| 4 | Heap-header offsets | CONSISTENT |
| 5 | Slot-entry size | CONSISTENT |
| 6 | Slot-entry offsets | CONSISTENT |
| 7 | Structural slot count | CONSISTENT |
| 8 | Maximum SlotId | CONSISTENT |
| 9 | SlotId sentinel | CONSISTENT |
| 10 | Slot-state registry | CONSISTENT |
| 11 | UNUSED canonical encoding | CONSISTENT |
| 12 | DEAD canonical encoding | CONSISTENT |
| 13 | Allowed slot transitions | CONSISTENT |
| 14 | RID physical-version semantics | CONSISTENT |
| 15 | Update/new-RID semantics | CONSISTENT |
| 16 | RID reuse predicate | CONSISTENT |
| 17 | Tuple-region boundaries | CONSISTENT |
| 18 | Checked extent arithmetic | CONSISTENT |
| 19 | Tuple/header overlap prevention | CONSISTENT |
| 20 | Tuple/directory overlap prevention | CONSISTENT |
| 21 | Live/retained tuple overlap prevention | CONSISTENT |
| 22 | Free-space definition | CONSISTENT BUT SPECIALIZED |
| 23 | Reusable-slot insertion cost | CONSISTENT |
| 24 | New-slot insertion cost | CONSISTENT |
| 25 | Fragmentation semantics | CONSISTENT |
| 26 | Compaction semantics | CONSISTENT |
| 27 | Compaction RID stability | CONSISTENT |
| 28 | Page initialization | CONSISTENT |
| 29 | PageNo 0 rejection | CONSISTENT |
| 30 | Invalid PageNo rejection | CONSISTENT |
| 31 | FileKind/PageType validation | CONSISTENT |
| 32 | FileId validation | CONSISTENT |
| 33 | Object-owner validation | CONSISTENT |
| 34 | Descriptor-owner chain | CONSISTENT |
| 35 | Ordinary/recovery validation | CONSISTENT BUT SPECIALIZED |
| 36 | Validation order | CONSISTENT |
| 37 | Tuple-header size | CONSISTENT |
| 38 | Minimum tuple size | CONSISTENT |
| 39 | Maximum physical tuple size | CONSISTENT BUT SPECIALIZED |
| 40 | Tuple flags/reserved handling | CONSISTENT |
| 41 | Schema/layout ownership | CONSISTENT |
| 42 | SchemaVer behavior | CONSISTENT |
| 43 | Malformed tuple rejection | CONSISTENT; localized wording finding |
| 44 | page_lsn/checksum | CONSISTENT BUT SPECIALIZED |
| 45 | WAL/MTR ownership | CONSISTENT BUT SPECIALIZED |
| 46 | FSM consistency | CONSISTENT BUT SPECIALIZED |
| 47 | Reclamation/grace | CONSISTENT BUT SPECIALIZED |
| 48 | Error-domain distinction | CONSISTENT; localized wording finding |
| 49 | Temporality/document ownership | FINDING |
| 50 | Implementation freedom | CONSISTENT |

## 94–100. Findings

94. **BLOCKING findings:** None.

95. **MAJOR findings:** None.

96. **MINOR findings:**

### M1 — §5.6 oversized-tuple terminology/outcome

- **Evidence:** “maximum accepted raw tuple payload” and “explicit row-too-large/unsupported outcome.”
- **Severity/type:** MINOR — NORMATIVE CLARITY.
- **Scope:** Cross-section with §§4.3.2.5 and 4.13.8.
- **Arithmetic:** `8192-48-8=8136`; v1 deliberately accepts only 8135 complete encoded tuple bytes.
- **Issue:** “Payload” can be read as excluding the 48-byte tuple header, and “row-too-large/unsupported” leaves two semantic categories where Chapter 4 identifies `ROW_TOO_LARGE`.
- **Consequence:** An implementer could apply the limit to the body rather than the complete tuple or expose inconsistent error classification.
- **Future action:** **A. LOCAL WORDING FIX** — use “complete encoded tuple length/raw tuple byte length” and the canonical oversized-row outcome.

### M2 — §5.6 project-sequencing language

- **Evidence:** “For the initial page geometry” and “remains deferred until the base heap, WAL/recovery, MVCC, and index layers are mature.”
- **Severity/type:** MINOR — TEMPORALITY.
- **Scope:** Local.
- **Issue:** The durable rule is v1 inline-only storage; subsystem “maturity” is project chronology.
- **Consequence:** No semantic ambiguity, but it weakens timeless architecture style.
- **Future action:** **A. LOCAL WORDING FIX** — say “For the v1 page geometry” and “Overflow/large-object storage is deferred from v1.”

### M3 — §5.10.3 malformed NOT NULL classification

- **Evidence:** “Such a tuple is invalid/corrupt or incompatible relative to that schema.”
- **Severity/type:** MINOR — VALIDATION.
- **Scope:** Cross-section with §4.13.8.
- **Issue:** A supported-v1 tuple violating its resolved schema is a local codec failure and canonically `CORRUPT_HEAP`; “or incompatible” is not a stable classification.
- **Consequence:** Diagnostics could incorrectly treat supported malformed bytes as compatibility/unsupported state.
- **Future action:** **A. LOCAL WORDING FIX** — state the canonical corrupt-heap classification.

### M4 — §5.13 implementation-progress wording

- **Evidence:** “The first implementation MAY support only schema version `1`” and “until such behavior is explicitly implemented.”
- **Severity/type:** MINOR — TEMPORALITY.
- **Scope:** Cross-section with §§4.3.2.1 and 16.7.
- **Issue:** Canonical v1 semantics already say SchemaVer 1 is emitted and schema-changing ALTER is absent; wording is framed as implementation progress.
- **Consequence:** No semantic gap, but the live architecture reads like sequencing/status.
- **Future action:** **A. LOCAL WORDING FIX** — state timeless v1 support and the architecture-revision requirement for additional behavior.

97. **EDITORIAL finding:**

### E1 — imprecise later-owner references

- **Sections/evidence:** §5.7.3 “MVCC/transaction chapters”; §5.14 “later WAL/BufferPool transaction protocol”; §5.15 “later transaction/durability chapters.”
- **Severity/type:** EDITORIAL — CROSS-REFERENCE.
- **Scope:** Cross-section.
- **Issue:** Owners exist and are consistent, but precise section references would reduce navigation cost.
- **Consequence:** No correctness ambiguity after reading the later chapters.
- **Future action:** **B. CROSS-REFERENCE FIX** — cite the exact owning sections.

98. **FROZEN ARCHITECTURE SEMANTIC QUESTIONS:** None.

99. **FOLLOW-UP VERIFICATION GAPS:** None identified.

100. **Out-of-scope observations:**

- **KNOWN OUT-OF-SCOPE OBSERVATION — CHAPTER 7:** §7.5 still says “once the buffer layer exists.” It remains unchanged for the Chapter-7 review.
- The PROJECT_STATE HEAP owner-validation defect remains an implementation issue against clear architecture and was not reopened.

## 101–115. Final direct decisions

101. Byte-layout arithmetic mismatch? **No.**
102. Unexpected field overlap? **No.**
103. SlotId/slot-capacity contradiction? **No.**
104. Undefined slot-state encoding? **No.**
105. RID semantic contradiction? **No.**
106. Premature RID reuse path? **No.**
107. Unsafe tuple-extent validation gap? **No.**
108. Live tuple-overlap ambiguity? **No.**
109. Free-space/FSM mismatch? **No.**
110. Owner-validation architecture gap? **No.**
111. Validation-order safety gap? **No.**
112. Tuple-decoding ambiguity? **No semantic ambiguity; one error-class wording finding.**
113. Mutation/WAL atomicity ambiguity? **No.**
114. Recovery/ordinary-state contradiction? **No.**
115. Correctness-relevant implementer invention required? **No.** Remaining freedom concerns algorithms and API mechanics, not policy.

## 116–127. Regression, next action, and repository state

116. **Previous-chapter regression:** No incompatibility found with the closed Chapter 1–4 contracts.

117. **Chapter-4 compatibility:** Fully consistent for identifiers, 8192-byte pages, little-endian codecs, PageNo 0 reservation, common header, SlotId 0–1017, RID meaning, validation, checksum/page_lsn, owner identity, and exhaustion/error distinctions.

118. **Known §7.5 observation:** Present and unchanged.

119. **Recommended next action:** **targeted documentation edit** for the four minor findings and optional cross-reference cleanup. No semantic review or verification synchronization is required.

120. **Recommended Chapter-6 review scope:** Review the actual Chapter-6 ownership of:

- exact FSM superblock/data-page bytes;
- category arithmetic and inverse lower bound;
- heap-page-to-entry mapping;
- initialized-prefix and zero-suffix rules;
- stale/advisory/repair/rebuild behavior;
- compaction and vacuum boundary;
- empty-page/whole-page reuse interaction with RID safety;
- FSM owner validation and WAL/recovery publication.

Treat Chapter-5 `free_bytes`, 8-byte new-slot cost, 8135-byte tuple limit, and RID-reuse rules as consumed frozen inputs.

121. **Files modified by audit:** None.

122. **Initial Git state:**

```text
status:  M docs/VERIFICATION.md
index:   empty
HEAD:    9b12cc6a72dff7d6f69e40320d5365636a74dc4c
```

The verification modification pre-existed this read-only review.

123. **Final Git state:**

```text
status:  M docs/VERIFICATION.md
index:   empty
HEAD:    9b12cc6a72dff7d6f69e40320d5365636a74dc4c
```

124. **`git diff --check`:** Passed with no output.

125. **Repository-state assessment:** Initial and final tracked/index/HEAD state are unchanged. `git diff --name-only` remained only `docs/VERIFICATION.md`.

126. **Audit-created repository changes:** None. No pre-existing material was modified, staged, discarded, or committed.

127. **Phase 2:** No implementation work occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.