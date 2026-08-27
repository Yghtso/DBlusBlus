# Chapter 6 architecture review

## 1–9. Verdict, scope, and finding counts

1. **Verdict:** **CHAPTER 6 — TARGETED DOCUMENT FIXES RECOMMENDED**

   The persistent layout, category mathematics, mapping, advisory semantics, ownership validation, WAL/recovery integration, and reclamation relationships are coherent. No frozen semantic decision is required. The chapter does, however, contain four localized documentation findings and one editorial navigation issue.

2. **Primary scope read:** [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md), lines 3855–4349: Chapter 6 in full, ending immediately before Chapter 7.

3. **Actual Chapter-6 heading inventory:**

| Section | Exact heading | Canonical responsibility | Document role |
|---|---|---|---|
| 6 | Free-Space Management and Physical Reclamation | Chapter-level FSM and heap physical-reclamation boundary | Architecture-appropriate |
| 6.1 | Scope | FSM authority, advisory status, and chapter ownership boundary | Architecture-appropriate, with imprecise navigation |
| 6.2 | FSM file organization | FSM file/page organization and category domain | Architecture-appropriate |
| 6.3 | V1 free-space category semantics | Exact forward category arithmetic | Architecture-appropriate, with terminology/temporality findings |
| 6.3.1 | Representative boundaries | Normative category examples | Architecture-appropriate |
| 6.4 | Category lower-bound interpretation | Exact inverse-category meaning | Architecture-appropriate |
| 6.5 | FSM_DATA page format v1 | Page-type/version/header contract | Architecture-appropriate |
| 6.5.1 | Byte layout | Exact persisted FSM_DATA bytes | Architecture-appropriate |
| 6.6 | Deterministic heap-page-to-FSM-entry mapping | PageNo-to-FSM addressing and reverse mapping | Architecture-appropriate |
| 6.7 | entry_count and initialized-prefix semantics | Initialized prefix and required-zero suffix | Architecture with current-state/ambiguity issue |
| 6.8 | Blank FSM_DATA page initialization | Canonical page initialization | Architecture with implementation-stage leakage |
| 6.9 | Runtime FSM acceleration | Nonpersistent derived candidate-index freedom | Architecture-appropriate |
| 6.10 | Advisory, stale, repairable, and rebuildable semantics | Heap authority, stale-state safety, repair/rebuild | Architecture-appropriate |
| 6.11 | Heap-page compaction boundary | Compaction’s FSM/RID boundary | Architecture-appropriate |
| 6.12 | Vacuum physical-reclamation boundary | Cross-subsystem physical reclamation boundary | Architecture-appropriate, with imprecise navigation |
| 6.13 | FSM and reclamation invariants | Canonical summary invariants | Architecture-appropriate, with terminology finding |

4. **Context-only architecture consulted:** front matter; §§4.3.2, 4.7–4.14; relevant Chapter 5 heap geometry, free-space, compaction, INSERT, and RID rules; §§7.5–7.12; §§12.1, 12.7.3, 12.9, 12.11–12.12, 12.17–12.18; §§13.11–13.21; §§14.5–14.12, 14.16–14.18; §§15.1–15.4, 15.8–15.9; §§16.5.2, 16.5.9–16.6; §39.1; §41.1; Appendices A–C.

5. **Other live documents consulted:** `AGENTS.md`, `docs/PROJECT_STATE.md`, `docs/VERIFICATION.md`, and the role/sequence portions of `docs/DEVELOPMENT.md`.

6. **BLOCKING:** 0
7. **MAJOR:** 0
8. **MINOR:** 4
9. **EDITORIAL:** 1

## 10. Section-by-section review

“Clear” means the applicable contract is explicit or precisely delegated.

| Section | Role | Timelessness | Ownership | Analytical depth | Ambiguity | Layout | Mapping/math | Owner validation | Advisory/stale | Mutation/WAL | Recovery/rebuild | Reclamation | Normative clarity | Cross-ref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6 | Chapter boundary | Clear | Correct | Sufficient | None | Clear | Clear | Clear | Clear | Delegated | Delegated | Clear | Clear | Good | Consistent | CLEAN |
| 6.1 | Scope | Clear | Correct | Sufficient | None | N/A | N/A | Delegated | Clear | Delegated | Delegated | Clear | Clear | Imprecise | Consistent | FINDING |
| 6.2 | File/category organization | Clear | Correct | Sufficient | “payload” ambiguity | Clear | N/A | Via Ch. 4/16 | Clear | N/A | N/A | N/A | Clear | Good | Consistent | FINDING |
| 6.3 | Forward category | Project-stage phrase | Correct | Sufficient | “payload” ambiguity | N/A | Clear | N/A | Clear | N/A | N/A | Slot reuse clear globally | Clear | Good | Consistent | FINDING |
| 6.3.1 | Boundaries | Clear | Correct | Sufficient | None | N/A | Exact | N/A | N/A | N/A | N/A | N/A | Clear | Good | Consistent | CLEAN |
| 6.4 | Inverse bound | Clear | Correct | Sufficient | “payload” ambiguity | N/A | Exact | N/A | Clear | N/A | N/A | N/A | Clear | Good | Consistent | FINDING |
| 6.5 | FSM page contract | Clear | Correct | Sufficient | None | Clear | N/A | Via §4.13.6 | N/A | Common rules | Common rules | N/A | Clear | Good | Consistent | CLEAN |
| 6.5.1 | FSM bytes | Clear | Correct | Sufficient | None | Exact | N/A | Clear | N/A | Common rules | Common rules | N/A | Clear | Good | Consistent | CLEAN |
| 6.6 | Mapping | Clear | Correct | Sufficient | None | N/A | Exact | Clear | N/A | Publication delegated | Recovery delegated | N/A | Strong | Good | Consistent | CLEAN |
| 6.7 | Prefix/suffix | Current-state phrase | Correct rule, awkward framing | Sufficient | “currently existing” can imply completeness | Clear | Clear | §4.13.6 closes bound | Short prefix safe | Generic publication | Rebuildable | N/A | Mostly clear | Good | Consistent globally | FINDING |
| 6.8 | Initialization | Not timeless | Other-doc leakage | Sufficient otherwise | Stage-conditioned behavior | Clear | Clear | Clear | N/A | Globally settled | Globally settled | N/A | Locally conditional | Good | Globally consistent | FINDING |
| 6.9 | Runtime accelerator | Clear | Correct | Sufficient | None | Nonpersistent | Free | N/A | Clearly derived | N/A | Rebuildable | N/A | Clear | Good | Consistent | CLEAN |
| 6.10 | Advisory/stale/rebuild | Clear | Correct | Strong | “as appropriate” leaves safe policy freedom | N/A | N/A | Structural/stale split clear | Strong | Separate hint path | Clear | N/A | Strong | Good | Consistent | CLEAN |
| 6.11 | Compaction | Clear | Correct specialization | Sufficient | None | Uses Ch. 5 | N/A | Via page validation | FSM consequence clear | Delegated | Delegated | Strong | Strong | Precise §4.13.3 | Consistent | CLEAN |
| 6.12 | Vacuum boundary | Clear | Correct specialization | Sufficient | None | N/A | N/A | N/A | Update position clear | Delegated | Delegated | Strong | Clear | Imprecise | Consistent | FINDING |
| 6.13 | Invariants | Clear | Correct | Sufficient | “tuple payload” ambiguity | Summary exact | Summary exact | §4.13.6 | Strong | Delegated | Rebuildable | Strong | Clear | Precise | Consistent | FINDING |

## 11–18. Ownership and persistent format

11. **Ownership boundary:** Chapter 6 owns FSM bytes, category arithmetic, mapping, initialized-prefix semantics, runtime accelerator constraints, advisory/stale behavior, and FSM-facing compaction/reclamation boundaries. It does not own heap truth, tuple bytes, RID grace, BufferPool residency, WAL publication, crash recovery, DML orchestration, catalog identity, development sequencing, or detailed test procedure.

12. **FSM FileSuperblock:** It uses the generic 72-byte Chapter-4 FileSuperblock without specialization conflicts.

13. **HEAP/FSM pairing:** Each table has distinct HEAP and FSM FileIds, a common nonzero TableId in `object_id`, deterministic managed filenames, and one `sys_tables` descriptor binding both. Creation and retirement encompass both files.

14. **FSM page layout:** Exact and internally consistent.

15. **Common-header inheritance:** The first 32 bytes remain the Chapter-4 common page header; Chapter 6 does not redefine them.

16. **Header arithmetic:** `32 common + 16 FSM-specific = 48`.

17. **Entry width:** Exactly one byte. All values `0..255` are valid within the initialized prefix; no reserved category codes exist.

18. **Entry count:** `8192 - 48 = 8144` entries.

### 95. FSM FileSuperblock specialization

| Field | FSM value | Meaning | Validation source |
|---|---|---|---|
| `page_type` | `SUPERBLOCK` | Page zero framing | §§4.9–4.10 |
| `format_version` | `1` | V1 codec | §4.10 |
| `header_size` | `72` | Generic FSM superblock | §4.10 |
| `magic` | `DBLUSBLS` | File family | §4.10 |
| `file_kind` | `FSM` / code 3 | Free-space metadata file | §§4.7, 4.10 |
| `page_size` | `8192` | Physical page size | §4.10 |
| `file_id` | Expected nonzero FSM FileId | Distinct file identity | §§4.3, 4.13.6, 16.5.2 |
| `object_id` | Expected TableId | Relation ownership | §§4.10.2, 16.5.2 |
| `creation_epoch` | Opaque uint64 | Nonsemantic persisted value | §4.10.2 |
| flags/reserved/trailing | Zero | V1 canonical form | §§4.10, 4.14 |

### 96–97. FSM page header and byte arithmetic

| Field | Offset | Width | Encoding/value | Writer/validator rule |
|---|---:|---:|---|---|
| `page_type` | 0 | 2 | LE, `FSM_DATA` | Common writer; reject mismatch |
| `format_version` | 2 | 2 | LE, `1` | Reject unsupported/corrupt version |
| common `flags` | 4 | 4 | LE, zero | Reject nonzero |
| `page_lsn` | 8 | 8 | LE Lsn | Chapter-12 mutation owner |
| `checksum_crc32c` | 16 | 4 | LE CRC32C | Validate before trusting `page_lsn` |
| `header_size` | 20 | 2 | LE, `48` | Reject mismatch |
| common `reserved16` | 22 | 2 | Zero | Reject nonzero |
| `page_no` | 24 | 8 | LE, actual FSM PageNo | Reject zero, invalid, mismatch, unpublished |
| `first_heap_page_no` | 32 | 8 | LE deterministic mapping | Must equal `1+(P-1)*8144` |
| `entry_count` | 40 | 2 | LE, `0..8144` | Reject larger |
| FSM `reserved16` | 42 | 2 | Zero | Reject nonzero |
| FSM `reserved32` | 44 | 4 | Zero | Reject nonzero |
| categories | 48 | 8144 | One byte each | Prefix valid; suffix zero |

| Layout | Stated | Derived | Offsets/gaps | Maximum | Result |
|---|---:|---:|---|---:|---|
| Common header | 32 | `0..31 = 32` | No gap/overlap | N/A | Consistent |
| FSM-specific header | 16 | `8+2+2+4 = 16` | `32..47` | N/A | Consistent |
| Total header | 48 | `32+16 = 48` | No gap/overlap | N/A | Consistent |
| Category region | 8144 | `8192-48 = 8144` | `48..8191` | 8144 entries | Consistent |
| Whole page | 8192 | `48+8144 = 8192` | Exact | N/A | Consistent |

## 19–28. Category mathematics

19. A category is a quantized conservative estimate of complete encoded tuple insertion capacity after reserving a potential eight-byte new slot. It is not exact free space or insertion authority.

20. **Forward formula:**

```text
b = min(free_bytes, 8144)
u = min(max(b - 8, 0), 8135)
category = floor(u * 255 / 8135)
```

21. **Inverse lower bound:**

```text
minimum_usable(c) = ceil(c * 8135 / 255)
                  = (c * 8135 + 254) / 255
```

22. **Monotonicity:** `min`, saturated subtraction, positive multiplication, and floor division are monotonic, so `f1 <= f2` implies `category(f1) <= category(f2)`.

23. **Saturation:** Inputs above 8144 clamp to the physical maximum; usable bytes clamp at 8135. This prevents category 255 from implying that an 8136-byte tuple is valid.

24. **Zero:** Category zero covers free gaps `0..39`. Within the initialized prefix it is a valid least-capacity estimate, not “uninitialized.”

25. **Eight-byte reservation:** Exactly matches Chapter 5’s new-slot descriptor cost and never under-reserves.

26. **Reusable slot:** The FSM can understate capacity because reuse can avoid the eight-byte charge. That is safe inefficiency, not a correctness failure.

27. **Compaction:** Categories use the current contiguous `[lower,upper)` gap. Compaction may increase that gap; the post-compaction estimate may be refreshed or later rebuilt.

28. **8135 cross-check:** A fresh page with gap 8144 gives usable 8135 and category 255. A page with gap 8143 also gives 8135. An 8136-byte tuple remains rejected by Chapter 5.

### 98–100. Category tables

| Input | Computation | Result |
|---|---|---:|
| Legal heap gap | `0..8144` | All map to `0..255` |
| Slot reservation | `max(free-8,0)` | No unsigned underflow |
| Tuple cap | `min(...,8135)` | 8136 never represented |
| Category | `floor(usable*255/8135)` | Monotonic |
| Inverse | `ceil(c*8135/255)` | Inclusive lower bound |

| Category | Inverse lower bound |
|---:|---:|
| 0 | 0 |
| 1 | 32 |
| 2 | 64 |
| 127 | 4052 |
| 254 | 8104 |
| 255 | 8135 |

| Free-byte range/example | Usable bytes | Category | Boundary meaning |
|---|---:|---:|---|
| `0..8` | 0 | 0 | Slot reserve consumes the gap |
| 9 | 1 | 0 | Below first quantum |
| 39 | 31 | 0 | Last category-0 byte |
| 40 | 32 | 1 | First category-1 byte |
| `40..71` | `32..63` | 1 | First full bucket |
| 72 | 64 | 2 | Next transition |
| 4075 | 4067 | 127 | Published representative |
| 8142 | 8134 | 254 | Cannot guarantee 8135 |
| 8143 | 8135 | 255 | Exact tuple+slot boundary |
| 8144 | capped 8135 | 255 | Empty page |
| 8136-byte tuple | N/A | N/A | Rejected by heap contract |

For `1 <= c <= 254`, the exact fresh-page free range is:

```text
8 + ceil(c*8135/255)
through
7 + ceil((c+1)*8135/255)
```

Category 255 covers free gaps 8143 and 8144.

## 29–39. Mapping, maximum coverage, and initialization

29. **Mapping:** For ordinary heap PageNo `H`, ordinal `H-1`, FSM PageNo `1+(H-1)/8144`, entry `(H-1)%8144`.

30. **First page:** Heap page 1 maps to FSM page 1, entry 0.

31. **Coverage:** FSM page `P` covers heap pages:

```text
1 + (P-1)*8144 through P*8144
```

subject to the initialized prefix and paired heap published bound.

32. **Maximum mapping:** The maximum physical heap PageNo is `1,125,899,906,842,622`. It maps safely to FSM page `138,249,006,243`, entry `7773`.

33. **Maximum paired coverage:** The required FSM file has `138,249,006,244` pages including page zero and length `1,132,535,859,150,848` bytes—well below the signed positional-I/O file limit. Therefore the HEAP physical PageNo cap, not FSM coverage, is the paired relation limit.

34. **Initialized prefix:** It is a page-local count in `0..8144`, not a highest index. It may be shorter than the paired heap extent and must never extend past the paired heap’s published bound.

35. **Zero suffix:** Every byte from `entry_count` through entry 8143 is canonical zero. Zero inside the prefix remains a valid category.

36. **Initial FSM state:** A newly created FSM minimally contains its page-zero superblock. Chapter 6 does not require eager FSM_DATA allocation while the paired heap has no ordinary published pages.

37. **Initial empty heap category:** `lower=48`, `upper=8192`, `free=8144`, usable `min(8144-8,8135)=8135`, category 255.

38. **Heap extension:** A HEAP_DATA page publishes first through PAGE_INIT; the corresponding FSM entry follows as advisory metadata. FSM lag does not invalidate the heap.

39. **FSM-page extension:** A required new FSM_DATA page follows the same append/PAGE_INIT publication protocol. A private or unpublished page cannot be searched.

### 101–103. Mapping, coverage, and prefix tables

| Heap PageNo | Ordinal | FSM PageNo | Entry | FSM coverage |
|---:|---:|---:|---:|---|
| 1 | 0 | 1 | 0 | `1..8144` |
| 8144 | 8143 | 1 | 8143 | `1..8144` |
| 8145 | 8144 | 2 | 0 | `8145..16288` |
| 16288 | 16287 | 2 | 8143 | `8145..16288` |
| 16289 | 16288 | 3 | 0 | `16289..24432` |
| `1,125,899,906,842,622` | `…621` | `138,249,006,243` | 7773 | Final page uses prefix 7774 |

| FSM PageNo | First represented heap page | Last mathematical page | Valid initialized range |
|---:|---:|---:|---|
| 1 | 1 | 8144 | Prefix bounded by heap publication |
| 2 | 8145 | 16288 | Same |
| `P` | `1+(P-1)*8144` | `P*8144` | First `entry_count` entries |
| Final required page | `1,125,899,906,834,849` | `1,125,899,906,842,992` | Only 7774 entries may represent legal heap pages |

| Location | Byte value zero means | Nonzero permitted? | Validation |
|---|---|---|---|
| Initialized prefix | Valid category 0 | Yes, any `0..255` | Category accuracy advisory |
| Uninitialized suffix | Canonical unused storage | No | Nonzero is structural corruption |
| Entry outside prefix | No valid entry exists | N/A | Access/update rejected |
| Prefix beyond heap published bound | Invalid ownership/range | N/A | Reject before use |

## 40–55. Mutation, advisory behavior, and WAL

40. **INSERT update:** The heap mutation is authoritative and may publish before the FSM hint. The hint can be recomputed afterward.

41. **DELETE:** Logical DELETE does not reclaim tuple bytes, so it must not advertise new free space.

42. **UPDATE:** The destination/new-version page loses capacity; the old-version page does not regain it until reclamation.

43. **Vacuum:** Reclamation, compaction, or `DEAD -> UNUSED` can improve placement opportunities; FSM maintenance may be batched.

44. **Compaction:** A refresh uses final validated heap geometry. Failure to refresh leaves a stale hint, not an invalid heap page.

45. **Stale high:** Candidate fetch occurs, heap validation rejects insufficient space, and the entry may be repaired downward. No unsafe insertion follows.

46. **Stale low:** A usable page may be skipped, causing packing inefficiency or unnecessary extension, but no incorrect tuple placement.

47. **Advisory boundary:** FSM cannot return tuples, prove emptiness, decide visibility, authorize reuse, or establish insertion success.

48. **Mandatory heap recheck:** Every FSM-selected page must be fetched, owner-validated, guarded/latched, and checked using actual heap geometry before mutation.

49. **Candidate selection:** The request size and categories may guide any suitable search. Stale candidates can be repaired/retried; exact bucket/container/search ordering is free.

50. **Search determinism:** No lowest-PageNo or other deterministic tie-break is required.

51. **Runtime candidate index:** Derived, process-local, nonpersistent, and rebuildable. Chapter 6 deliberately does not mandate the current project’s bucket implementation.

52. **Persisted versus runtime FSM:** Persisted bytes own category/mapping/prefix structure. Runtime indexes merely accelerate searches.

53. **FSM update failure:** A completed heap mutation remains valid. The old FSM category remains structurally valid and stale unless the FSM mutation itself entered a noncontinuable WAL/publication state.

54. **Same MTR:** Heap and FSM hints are not required to be one atomic MTR. Each persistent FSM page mutation follows its own WAL-backed page-publication unit.

55. **WAL-before-data:** Every WAL-protected FSM page obeys `durable_lsn >= page_lsn` before stable page write.

### 105–106. Mutation and stale-state tables

| Operation | Heap truth changed? | FSM bytes | Same MTR? | May be stale? | WAL owner | Failure effect |
|---|---:|---|---|---:|---|---|
| New heap page | Yes | Entry/page may follow | No requirement | Yes | §§4.11, 12.12 | Heap survives; rebuild hint |
| INSERT | Yes | Usually reduced category | Separate allowed | Yes | Heap and FSM page units | No rollback solely for missed hint |
| Logical DELETE | Header only | No free-space increase | N/A | Existing estimate remains valid/stale | Heap mutation owner | No premature free space |
| UPDATE | New page loses capacity | Destination update | Separate allowed | Yes | DML/WAL owners | Old page unchanged until reclaim |
| Compaction | Contiguous gap increases | Refresh may follow | Separate allowed | Yes | Heap then FSM owner | Old category may understate |
| `NORMAL -> DEAD` | Slot state changes | Usually no immediate capacity gain | Separate | Yes | Vacuum/WAL | DEAD not reusable |
| `DEAD -> UNUSED` | Reuse eligibility changes | Refresh/rebuild | Separate | Yes | Vacuum/FSM owners | Grace remains authoritative |
| Local repair | No | One entry changes | FSM page unit | Immediately stale again possible | FSM page owner | Old legal estimate may remain |
| Rebuild | No | Pages/prefixes reconstructed | Independent valid units | Concurrently stale possible | FSM/PAGE_INIT owner | Restartable derived work |

| State | Safety consequence | Required action |
|---|---|---|
| Fresh exact category | Useful candidate hint | Still recheck heap |
| Stale high | Extra failed candidate | Recheck prevents overflow; optional repair |
| Stale low | Page overlooked | Efficiency only; later repair/rebuild |
| Short prefix | Some heap pages absent from FSM search | Never index outside prefix |
| Runtime index stale | Same as persisted stale | Rebuild from valid persisted FSM/heap |
| Category mismatch with heap | Not structural corruption | Treat as stale estimate |

## 56–65. Recovery, corruption, repair, and rebuild

56. **Recovery:** Complete logged FSM mutations redo normally; approximate FSM accuracy is not on the critical recovery path.

57. **READY with stale FSM:** Permitted when FSM structure and ownership are valid. Heap recheck preserves correctness.

58. **Structural corruption:** Bad checksum/header/mapping/prefix/suffix/owner is not “staleness”; ordinary use is rejected.

59. **Missing FSM:** A committed table requires its catalog-owned FSM file at the exact final name. Missing required physical ownership prevents READY unless an explicitly owned repair/reconstruction procedure first restores that required object.

60. **Corrupt FSM and rebuildability:** Structurally invalid FSM state cannot be used ordinarily, but authoritative rebuild from validated heap geometry is explicitly permitted. This does not make wrong-owner bytes acceptable.

61. **Derived-state classification:** **Required but rebuildable derived persistent state.** The file and its identity are required; category accuracy is not authoritative.

62. **Rebuild:** Scan validated heap headers, apply the same forward category formula, build valid prefixes/suffixes, and publish each persistent page through ordinary ownership/WAL rules. No background scheduler is mandated.

63. **Rebuild crash:** Only complete independently valid FSM page mutations survive. Partial runtime work disappears; stale/short but structurally valid survivors remain safe and rebuild may restart.

64. **Repair:** Local repair updates stale entries; whole-file/page rebuild restores broader state. Neither changes heap truth.

65. **Repair races:** Ordinary FSM page synchronization prevents torn persisted bytes. A lost freshness race may leave a legal stale category, which remains safe because heap recheck is mandatory.

### 107–109. Repair/rebuild, crash, and malformed-state tables

| Mechanism | Trigger | Authority | Publication | Concurrency result |
|---|---|---|---|---|
| Local repair | Observed stale entry | Guarded validated heap geometry | Ordinary FSM page WAL unit | May immediately become stale again |
| Prefix repair | Heap published range exceeds prefix | Paired heap published bound | Checked prefix/page mutation | Short old prefix remains safe |
| Rebuild | Startup or maintenance | Validated heap headers | Valid FSM pages/PAGE_INIT | May run incrementally; result may age |
| Runtime-index rebuild | Startup/cache invalidation | Persisted valid FSM or heap | Process-local only | No crash consequence |

| Crash boundary | Heap truth | FSM state | Recovery/open | Candidate safety |
|---|---|---|---|---|
| Heap mutation durable, FSM absent | New heap state | Old hint | Accept stale; repair later | Heap recheck |
| FSM WAL appended, page not flushed | Unchanged | Redoable if WAL survived | Redo or retain old hint | Safe |
| FSM PAGE_INIT incomplete | Unchanged | Unpublished page/tail | Generic tail reconciliation | Not searchable |
| Prefix advancement interrupted | Unchanged | Old or complete new page state | No torn prefix publication | Bounds enforced |
| Rebuild partially complete | Authoritative heap intact | Valid subset/short prefixes | Resume/restart | Missing entries not searched |
| Repair interrupted | Heap intact | Old or new valid category | Retry optional | Safe |
| Heap page reinitialized before hint repair | New page authority | Possibly old estimate | Recheck new page | RID grace required first |
| Page retired/unpublished | Published bound excludes it | Old bytes may remain | Ignore/rebuild outside bound | Cannot become candidate |

| Malformation | Detection owner | Before | Classification |
|---|---|---|---|
| Wrong `PageType` | L0/common validator | FSM parsing/use | `CORRUPT_PAGE` |
| Unsupported version | Version dispatcher | Layout parsing | Unsupported-format result |
| Bad checksum | L0 | Trusting `page_lsn` | Corruption/recovery-private repair |
| Wrong `header_size` | L1 | FSM-specific use | Corruption |
| Nonzero flags/reserved | L1 | Ordinary publication | Corruption |
| Wrong PageNo | L0/L1 owner | Publication | Corruption |
| Wrong FileId/TableId/pair | Registered owner/L2 | Ordinary use | Wrong-owner corruption |
| `entry_count > 8144` | L1 | Entry access | Corruption |
| Prefix beyond paired heap bound | L2 FSM owner | Candidate publication | Corruption |
| Nonzero suffix | L1 | Ordinary publication | Corruption |
| Invalid category byte | N/A | N/A | All uint8 values valid |
| Category differs from heap | Repair/verifier | Never a basis for insertion | Stale, not corruption |
| Runtime candidate beyond prefix/bound | Runtime owner | Fetch/use | Reject derived state |

## 66–79. Page reuse, validation, and transaction interaction

66. **Empty page:** A newly initialized page has the full 8144-byte gap. A page with no live tuples but retained slot history may have less contiguous space and a different category.

67. **Whole-page reuse:** Requires Chapter-14 RID grace before old `(PageNo,SlotId)` identities can reappear. Reinitialization resets heap geometry and the FSM estimate must eventually describe the new page.

68. **Unpublished tail:** No FSM entry may expose it. Candidate selection is bounded by the authoritative heap published count.

69. **Page retirement:** V1 does not require ordinary heap shrinking. Any entry beyond an authoritative published bound is ignored/rejected and may be zeroed or rebuilt.

70. **Growth races:** HEAP publication precedes FSM hint creation; FSM pages themselves publish through PAGE_INIT. Search cannot join private page intents.

71. **Owner validation:** Complete across FileKind, FileId, TableId, paired descriptor, PageNo, PageType, FSM and heap published bounds.

72. **Wrong pair:** An FSM for Table A attached to Heap B fails descriptor/superblock TableId, FileId pairing, and published-range validation before use.

73. **Validation order:** framing/size → family/version dispatch → checksum → common PageId/type/FileKind → registered FileId/TableId/pair → FSM header/ranges → prefix → suffix → entry use → ordinary publication. `page_lsn` is not trusted before checksum.

74. **Prefix validation:** Checked arithmetic proves `entry_count <= 8144`, deterministic range does not overflow, and initialized entries do not exceed the paired heap published bound.

75. **Checksum/page_lsn:** Fully consistent with Chapters 4, 7, and 12.

76. **Version/flags/reserved:** Version 1 only; all common/FSM flags and reserved fields are zero. Future version is unsupported, while malformed v1 bytes are corruption.

77. **Transaction/abort:** FSM describes physical geometry, not logical visibility. It is not rolled back merely because the inserting transaction aborts; the invisible physical version still consumes space.

78. **UPDATE/DELETE:** New UPDATE versions reduce destination capacity; old UPDATE/DELETE versions remain allocated until vacuum.

79. **RID safety:** No FSM category or “empty page” observation authorizes SlotId/RID or whole-page reuse.

### 104. Owner-validation table

| Identity | Expected source | Persisted where | Validator | Before use? | Failure |
|---|---|---|---|---:|---|
| Database/root | Active DatabaseInstance owner | Namespace/control context | Registry/lifecycle owner | Yes | Open/ownership failure |
| FileKind | Catalog/registered descriptor | FSM superblock | PageFile/registered owner | Yes | Corruption |
| FSM FileId | `sys_tables.fsm_file_id` | FSM superblock | Registered file owner | Yes | Corruption/missing object |
| TableId | TableDescriptor | FSM `object_id` | Superblock/catalog cross-check | Yes | Corruption |
| Paired HEAP identity | Same TableDescriptor | HEAP/FSM superblocks and catalog | Relation-wide owner | Yes | Wrong-owner corruption |
| FSM PageNo | Requested PageId | Common header | L0/L1 | Yes | Corruption |
| PageType | `FSM_DATA` | Common header | L0/L1 | Yes | Corruption |
| FSM published bound | Registered FSM file | Reconstructed runtime bound | File/BufferPool owner | Yes | Reject unpublished |
| Paired heap bound | Registered HEAP owner | Reconstructed runtime bound | FSM L2 owner | Yes | Corruption/reject |

### 110. Page-reuse interaction

| Heap state | FSM consequence | RID consequence |
|---|---|---|
| Newly initialized | Category 255 when synchronized | New identity only after publication |
| No live tuples, slots retained | Category reflects actual contiguous gap | Existing RIDs not automatically reusable |
| DEAD payload compacted | Gap may increase | Slot remains nonreusable |
| DEAD → UNUSED | Reusable-slot benefit may be understated | Requires grace and chain proof |
| Whole page empty | May remain reusable space | Reinitialization still grace-gated |
| Unpublished tail | No valid candidate entry | PageNo may be retried only under append rollback |
| Retired/out-of-bound page | Ignore/remove metadata | Cannot be reached by FSM |

## 80–94. Documentation model and supporting-doc assessment

80. **Global documentation-model assessment:**

- a. Analytical rather than chronological? **Mostly, but not completely.**
- b. Project/current-state narration? **Yes, localized in §§6.3, 6.7, and especially 6.8.**
- c. Development sequencing leakage? **Yes.**
- d. Detailed verification methodology leakage? **No.**
- e. History/results belonging in devlog? **No.**
- f. Present implementation facts belonging in PROJECT_STATE? **Implied in §§6.7–6.8.**
- g. Architecture conditioned on implementation availability? **Yes, §6.8’s “Before/Once WAL integration.”**
- h. Independently readable without timeline? **Not entirely until those phrases are rewritten.**
- i. Wording implying an implementation plan? **Yes, §6.8 most clearly.**
- j. Deferred capability expressed as durable v1 scope? **Generally yes; the identified phrases are the exception.**

81–82. **Temporality assessment:**

| Passage | Class | Assessment |
|---|---|---|
| “after the candidate page has been fetched” | A runtime ordering | Valid |
| “current contiguous free-space gap” | A runtime state | Valid |
| “a later insertion path…slot reuse may become possible” | F project sequencing | Finding |
| “currently existing heap pages” | A runtime state, but ambiguous against short-prefix rule | Finding component |
| “A later relation-wide FSM owner MAY grow…” | F implementation availability | Finding |
| “Before WAL integration…” | F implementation sequence | Finding |
| “Before WAL/recovery integration…” | F implementation sequence | Finding |
| “Once WAL/recovery is active…” | F implementation sequence | Finding |
| “later FSM repair/rebuild work” | A runtime consequence | Valid |
| “At startup or during maintenance” | A lifecycle ordering | Valid |
| “currently succeed” | A runtime state | Valid |
| “later WAL/recovery and vacuum protocols” | E navigation | Valid but imprecise |
| “later safe RID-reuse protocol” | E navigation | Valid but imprecise |

83. **Current-state leakage:** Localized; it mirrors PROJECT_STATE’s absence of a relation-wide FSM owner and WAL/BufferPool integration rather than stating only the timeless v1 contract.

84–85. **Document ownership:**

| Passage/topic | Actual role | Correct owner |
|---|---|---|
| Persistent category/layout/mapping | Architecture | Chapter 6 |
| Advisory/recheck/rebuild invariants | Architecture | Chapter 6 |
| “later relation-wide owner” availability | Project state/sequencing | PROJECT_STATE/DEVELOPMENT; retain only timeless rule in Architecture |
| “Before WAL integration” defaults | Implementation stage | PROJECT_STATE/DEVELOPMENT; Architecture should state final publication semantics |
| Exact fault/crash test cases | Not present | VERIFICATION |
| Historical implementation results | Not present | devlog |
| Runtime bucket representation freedom | Architecture implementation boundary | Chapter 6 |

86. **Terminology/ambiguity:** Category, entry, stale, repair, rebuild, advisory, and authoritative are otherwise consistent. “Tuple payload” is the principal ambiguity because Chapter 5’s 8135 limit applies to the complete encoded tuple.

87–88. **Analytical depth:**

| Mechanism | Assessment |
|---|---|
| Advisory metadata | Analytically sufficient |
| Heap recheck | Analytically sufficient |
| Conservative eight-byte reservation | Analytically sufficient |
| Stale-high safety | Analytically sufficient |
| Stale-low consequence | Analytically sufficient |
| Runtime index derivation | Analytically sufficient |
| Rebuild source of truth | Analytically sufficient |
| RID/page reuse | Sufficient through precise Chapter-14 ownership |
| Why short prefixes are safe | Semantically clear globally; local wording could be sharper |

89. **Normative language:** Normative strengths are consistent with detailed owners. No summary weakens mandatory heap recheck, reserved-zero validation, checked mapping, prefix/suffix validity, or reclamation gates.

90. **Cross-reference quality:** Exact references resolve. The vague later-owner references should be made precise.

91. **Source coupling:** None. Conceptual `bucket[256]` is explicitly nonbinding.

92. **Implementation freedom:** Appropriate. Candidate-index container, tie-breaking, search order, repair scheduling, and rebuild scheduling remain free without weakening persistent semantics.

93. **PROJECT_STATE cross-check:** Page-local FSM encoding/mapping exists; the relation-wide owner, automatic repair/update, BufferPool integration, and paired-bound validation do not. The architecture contract is clear; these are implementation/project-state limitations, not architecture defects.

94. **VERIFICATION assessment:** One follow-up gap exists; see item 121.

### 111. Cross-chapter/document consistency matrix

| Owner | Relationship | Assessment |
|---|---|---|
| Chapter 4 | File/page identity, mapping checks, publication bounds, append/PAGE_INIT | CONSISTENT |
| Chapter 5 | `[lower,upper)`, eight-byte slot, compaction, 8135 limit, RID stability | CONSISTENT, terminology needs normalization |
| Chapter 7 | Guards, latches, validation before residency, writeback | CONSISTENT |
| Chapter 12 | WAL mutation publication and WAL-before-data | CONSISTENT; §6.8 wording is temporal |
| Chapter 13 | Approximate metadata may remain stale/rebuild | CONSISTENT |
| Chapter 14 | Vacuum, grace, slot/page reuse, batching | CONSISTENT |
| Chapter 15 | Advisory candidate selection and DML physical effects | CONSISTENT |
| Chapter 16 | HEAP/FSM descriptor and object ownership | CONSISTENT |
| Chapter 39 | Corruption/WAL/noncontinuable outcomes | CONSISTENT |
| Chapter 41 | Stale-FSM repair obligation | CONSISTENT but procedurally incomplete in VERIFICATION |
| DEVELOPMENT | Owns implementation sequencing | OWNERSHIP LEAKAGE from §6.8 |
| VERIFICATION | Owns exact procedures | CORRECT boundary in Chapter 6 |
| PROJECT_STATE | Owns present missing relation-wide owner/WAL integration | IMPLIED LEAKAGE in §§6.7–6.8 |
| devlog | Owns history/results | CORRECT; no leakage |

### 112. Explicit and vague cross-reference audit

| Source | Target | Purpose | Exists/owner | Precision | Status |
|---|---|---|---|---|---|
| §6.7 | §6.7 | Prefix bound | Yes/self | Precise | Clean |
| §6.11 | §4.13.3 | Heap extent/nonoverlap validation | Yes/canonical | Precise | Clean |
| §6.13 | §4.13.6 | FSM owner validation | Yes/canonical | Precise | Clean |
| §6.1 | “later chapters” | I/O, BufferPool, WAL/recovery | §§7.3–7.12, 12.12, Chapter 13 | Vague | Editorial |
| §6.12 | “later WAL/recovery and vacuum protocols” | Crash-safe reclamation order | §§12.12, 13.13, 14.5–14.12, 14.16 | Vague | Editorial |
| §6.12 | “later safe RID-reuse protocol” | Slot/page reuse | §§14.5–14.12 | Vague | Editorial |

### 113. Terminology table

| Term | Canonical meaning | Assessment |
|---|---|---|
| `free_bytes` | Current contiguous `upper-lower` gap | Clear |
| complete encoded tuple length | Whole persisted tuple including header | Canonical Chapter-5 term |
| tuple payload | Ambiguous in Chapter 6 | Finding |
| category | Quantized conservative insertion estimate | Clear |
| initialized prefix | First `entry_count` entries | Clear |
| zero suffix | Required-zero storage outside prefix | Clear |
| stale | Structurally valid estimate differing from heap truth | Clear |
| repair | Local estimate correction | Clear |
| rebuild | Derived reconstruction from heap headers | Clear |
| candidate | Suggested PageNo requiring heap recheck | Clear |
| authoritative | Heap geometry for insertion; FSM structure for mapping/format only | Clear |
| empty page | Must be interpreted physically, not merely by visibility | Clear globally |

### 114. Normative-language table

| Requirement | Strength | Assessment |
|---|---|---|
| FSM must not overstate guaranteed capacity due to slot reuse | MUST NOT | Correct |
| Runtime may use inverse/category accelerators | MAY | Appropriate freedom |
| Selected page must be fetched/rechecked | MUST | Correctness-critical and consistent |
| Reserved fields rejected | MUST | Correct |
| Mapping checked; deterministic first page | MUST/MUST NOT | Correct |
| Suffix zero; out-of-prefix access rejected | MUST | Correct |
| Prefix may grow | MAY | Semantically safe; wording temporal |
| Runtime accelerator/data structure | MAY | Appropriate |
| Stale FSM must not bypass heap check | MUST NOT | Correct |
| Recovery may tolerate stale categories | MAY | Consistent with Chapter 13 |
| Repair/rebuild capability | MUST | Correct |
| Compaction may move bytes/discard DEAD only | MAY | Correct |
| Compaction preserves IDs and does not infer global death | MUST/MUST NOT | Correct |

## 115. Sixty-item consistency/documentation matrix

| # | Item | Result | Note |
|---:|---|---|---|
| 1 | Ownership boundary | CONSISTENT | Clear delegation |
| 2 | FSM FileKind | CONSISTENT | Code 3 |
| 3 | FSM FileId | CONSISTENT | Distinct catalog identity |
| 4 | `object_id`/TableId | CONSISTENT | Paired relation |
| 5 | HEAP/FSM pairing | CONSISTENT | Descriptor and superblocks |
| 6 | FSM PageType | CONSISTENT | `FSM_DATA` |
| 7 | Common header | CONSISTENT | 32 bytes |
| 8 | Header size | CONSISTENT | 48 |
| 9 | Header offsets | CONSISTENT | No gaps/overlap |
| 10 | Entry width | CONSISTENT | 1 byte |
| 11 | Entries/page | CONSISTENT | 8144 |
| 12 | Category domain | CONSISTENT | All 0..255 |
| 13 | Zero category | CONSISTENT | Least estimate, not uninitialized in prefix |
| 14 | Forward formula | CONSISTENT | Exact |
| 15 | Monotonicity | CONSISTENT | Proven |
| 16 | Inverse lower bound | CONSISTENT | Exact |
| 17 | Saturation | CONSISTENT | Safe |
| 18 | Eight-byte slot reserve | CONSISTENT | Chapter 5 match |
| 19 | Reusable-slot conservatism | CONSISTENT | Safe underestimation |
| 20 | Compaction | CONSISTENT BUT SPECIALIZED | Uses current gap |
| 21 | 8135 tuple | CONSISTENT | Terminology finding only |
| 22 | PageNo mapping | CONSISTENT | Exact |
| 23 | First-page mapping | CONSISTENT | P1/e0 |
| 24 | Coverage boundary | CONSISTENT | 8144/page |
| 25 | Reversibility | CONSISTENT | Deterministic first/range |
| 26 | Max arithmetic | CONSISTENT | Checked and within limits |
| 27 | Paired max coverage | CONSISTENT | FSM comfortably sufficient |
| 28 | Prefix meaning | CONSISTENT globally | Local ambiguity finding |
| 29 | Prefix range | CONSISTENT | 0..8144 |
| 30 | Prefix monotonicity | N/A | Shrink/short stale coverage is safe; no monotonic correctness requirement |
| 31 | Zero suffix | CONSISTENT | Required |
| 32 | New heap page | CONSISTENT | HEAP first, hint later |
| 33 | New FSM page | CONSISTENT | PAGE_INIT |
| 34 | Empty-page category | CONSISTENT | 255 |
| 35 | INSERT update | CONSISTENT | Advisory/separate |
| 36 | DELETE free space | CONSISTENT | No premature gain |
| 37 | Vacuum update | CONSISTENT | May batch |
| 38 | Stale high | CONSISTENT | Mandatory recheck |
| 39 | Stale low | CONSISTENT | Efficiency only |
| 40 | Heap recheck | CONSISTENT | First-class invariant |
| 41 | Advisory boundary | CONSISTENT | Strong |
| 42 | Update failure | CONSISTENT | Heap remains valid |
| 43 | WAL/MTR ownership | CONSISTENT globally | §6.8 wording finding |
| 44 | Recovery | CONSISTENT | Stale allowed |
| 45 | Rebuild | CONSISTENT | Heap source |
| 46 | Repair | CONSISTENT | Local stale correction |
| 47 | Owner validation | CONSISTENT | Complete chain |
| 48 | Page reuse/RID | CONSISTENT | Chapter-14 gate |
| 49 | Implementation freedom | CONSISTENT | Appropriate |
| 50 | Implementer invention | CONSISTENT | No correctness policy missing |
| 51 | Timeless wording | FINDING | §§6.3, 6.7, 6.8 |
| 52 | No state narration | FINDING | Relation owner/WAL stage |
| 53 | No sequencing leakage | FINDING | §6.8 |
| 54 | No verification leakage | CONSISTENT | None |
| 55 | No history leakage | CONSISTENT | None |
| 56 | No current choice promoted | CONSISTENT | Runtime index is optional |
| 57 | Terminology | FINDING | Whole tuple called payload |
| 58 | Precise references | FINDING | Editorial vague owners |
| 59 | Analytical rationale | CONSISTENT | Sufficient |
| 60 | Timeline-independent canonical text | FINDING | §6.8 prevents full pass |

## 116–119. Complete findings

### 116. BLOCKING findings

None.

### 117. MAJOR findings

None.

### 118. MINOR findings

**M1 — Whole-tuple capacity terminology**

- Section: §§6.2–6.4 and 6.13.
- Evidence: “tuple-payload insertion capacity,” “usable tuple-payload bytes,” and “caps usable tuple payload at 8135 bytes.”
- Severity/type: MINOR — TERMINOLOGY.
- Scope: Cross-section and cross-Chapter-5.
- Arithmetic: 8135 is the maximum complete encoded tuple length; it includes the tuple header.
- Explanation: “Payload” can be read as excluding the tuple header.
- Canonical comparison: Chapter 5 uses “complete encoded tuple length.”
- Consequence: Candidate-size callers could interpret the input domain inconsistently, though mandatory heap recheck prevents memory corruption.
- Correct owner: ARCHITECTURE rewritten timelessly.
- Future action: **H. TERMINOLOGY NORMALIZATION.**

**M2 — Slot-reuse project sequencing**

- Section: §6.3.
- Evidence: “even if a later insertion path can sometimes reuse a slot…” and “slot reuse may become possible.”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Cross-section with Chapters 5 and 14.
- Arithmetic: The eight-byte reservation remains correct.
- Explanation: Safe slot reuse is already part of the canonical architecture; it is not merely a later implementation possibility.
- Canonical comparison: Chapter 5 defines reusable `UNUSED` slots; Chapter 14 defines the grace prerequisite.
- Consequence: Makes an existing architectural behavior look like roadmap work.
- Correct owner: ARCHITECTURE rewritten timelessly.
- Future action: **B. TIMELESSNESS REWRITE.**

**M3 — Initialized-prefix/current-owner wording**

- Section: §6.7.
- Evidence: “represents currently existing heap pages” and “A later relation-wide FSM owner MAY grow…”
- Severity/type: MINOR — CURRENT-STATE LEAKAGE.
- Scope: Cross-section with §§4.11.2 and 4.13.6.
- Arithmetic: `0 <= entry_count <= 8144`; a short prefix is explicitly legal.
- Explanation: “Currently existing” can imply complete coverage, while a short prefix is permitted; “later relation-wide owner” reflects implementation availability.
- Canonical comparison: §4.13.6 precisely says the initialized range may be short but cannot exceed the paired heap published range.
- Consequence: An implementer may confuse format validity with full relation-wide synchronization.
- Correct owner: Timeless rule in ARCHITECTURE; current owner absence remains in PROJECT_STATE.
- Future action: **C. CURRENT-STATE REMOVAL / PROJECT_STATE OWNERSHIP.**

**M4 — WAL-integration stage language**

- Section: §6.8.
- Evidence: “Before WAL integration…”, “Before WAL/recovery integration…”, and “Once WAL/recovery is active…”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Cross-section with §§4.8, 4.12, 7.10–7.11, 12.9, 12.12, and 12.17.
- Arithmetic: No layout issue.
- Explanation: V1 mandates WAL. The chapter conditions persistent-page behavior on implementation progress.
- Canonical comparison: Final page publication, `page_lsn`, checksum, dirty-state, and WAL-before-data behavior are already canonical.
- Consequence: Years-later readers could mistake provisional development behavior for an alternative v1 mode.
- Correct owner: ARCHITECTURE rewritten into final v1 publication semantics; implementation stage belongs in PROJECT_STATE/DEVELOPMENT.
- Future action: **B. TIMELESSNESS REWRITE.**

### 119. EDITORIAL findings

**E1 — Imprecise later-owner navigation**

- Sections: §§6.1 and 6.12.
- Evidence: “later chapters,” “later WAL/recovery and vacuum protocols,” and “later safe RID-reuse protocol.”
- Severity/type: EDITORIAL — CROSS-REFERENCE.
- Scope: Cross-section.
- Explanation: The owners exist but are not named precisely.
- Canonical comparison: Relevant owners are §§7.3–7.12, 12.12, Chapter 13, and §§14.5–14.12/14.16.
- Consequence: Navigation and ownership tracing are weaker than necessary; semantics remain clear globally.
- Correct owner: ARCHITECTURE.
- Future action: **G. CROSS-REFERENCE FIX.**

## 120–122. Semantic questions, verification, and out-of-scope observations

120. **FROZEN ARCHITECTURE SEMANTIC QUESTIONS:** None.

121. **FOLLOW-UP VERIFICATION GAP:** One composite Chapter-6 verification gap.

`VERIFICATION.md` names stale-FSM candidate repair and generic `entry_count` boundaries, but it does not provide a coherent FSM-specific procedure covering:

- all forward/inverse category boundaries and monotonicity;
- 8135/8136 interaction and eight-byte reservation;
- PageNo mapping, exact 8144-page boundaries, and maximum paired coverage;
- prefix/zero-suffix and paired-heap-bound validation;
- wrong HEAP/FSM pairing;
- stale-high versus stale-low behavior;
- FSM PAGE_INIT, interrupted prefix advancement, repair, and partial rebuild crash cases;
- required-file versus rebuildable-derived-state behavior.

This is a verification-document gap, not an architecture defect.

122. **Out-of-scope observations:**

- **KNOWN OUT-OF-SCOPE OBSERVATION — CHAPTER 7:** §7.5 still says “once the buffer layer exists.” It remains reserved for direct Chapter-7 review.
- No other later-chapter issue directly contradicts Chapter 6.

## 123–143. Direct audit answers

123. Byte-layout mismatch? **No.**
124. Category-formula contradiction? **No.**
125. Inverse-bound contradiction? **No.**
126. Heap/FSM mapping ambiguity? **No.**
127. Initialized-prefix ambiguity? **Localized wording ambiguity only; global contract is clear.**
128. Zero-suffix ambiguity? **No.**
129. Unsafe stale-high path? **No.**
130. Missing mandatory heap recheck? **No.**
131. FSM owner-validation gap? **No.**
132. WAL/MTR semantic ambiguity? **No; §6.8 has only temporal wording leakage.**
133. Crash/rebuild ambiguity? **No correctness-relevant ambiguity.**
134. Whole-page-reuse/RID contradiction? **No.**
135. Correctness-relevant implementer invention required? **No.**
136. Project-time/current-state wording? **Yes.**
137. DEVELOPMENT-owned material? **Yes, localized sequencing language.**
138. VERIFICATION-owned procedure in Chapter 6? **No.**
139. PROJECT_STATE-owned material? **Implied relation-owner/WAL availability language.**
140. Devlog/history material? **No.**
141. Ambiguous exact terminology? **Yes: “tuple payload” for whole encoded length.**
142. Analytically underexplained correctness boundary? **No material gap.**
143. Timeless canonical v1 description? **Not fully until M2–M4 are corrected.**

## 144–148. Regression and next action

144. **Previous-chapter regression:** Chapters 1–4 remain compatible. No Chapter-6 prose changes their v1, dependency, lifecycle, identity, exhaustion, publication, or validation contracts.

145. **Chapter-5 compatibility:** Technical behavior is fully compatible:

- 48-byte heap header;
- eight-byte slot descriptor;
- `[lower,upper)` contiguous gap;
- 8135 complete encoded tuple maximum;
- physical RID identity;
- delayed reuse;
- compaction preserving SlotId/RID.

Only Chapter-6 terminology should be normalized to Chapter 5’s whole-tuple wording.

146. **Known §7.5 observation:** Still present and unchanged; out of scope.

147. **Recommended next action:** **Targeted documentation edit**, followed by **verification synchronization** for the separate FSM methodology gap.

148. **Recommended Chapter-7 review scope:** Review the actual Chapter-7 boundary around explicit I/O, DiskManager/PageFile transfer semantics, BufferPool ownership and frame state, validation-before-residency, guards/pins/latches, dirty-generation publication, copied writeback, WAL-before-data, CLOCK eviction, new-page publication, retirement/drain, error outcomes, and shutdown. Prioritize §7.5’s implementation-stage wording. Treat Chapter 6 only as a consumer supplying `FSM_DATA` owner validation and advisory-page mutations.

## 149–155. Read-only and Git guarantees

149. **Files modified by audit:** None.

150. **Initial Git state:**

```text
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 3b1ab0c033bb2e3b38e8f1e373b50b0c8ae8f80b
```

151. **Final Git state:**

```text
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 3b1ab0c033bb2e3b38e8f1e373b50b0c8ae8f80b
```

152. **`git diff --check`:** Passed with no output.

153. **Repository-state-change assessment:** Initial and final status/index/HEAD are identical.

154. **Audit-created changes:** None. No files, reports, staging, commits, builds, tests, benchmarks, or formatting operations were created or performed.

155. **Phase 2:** Remains **NOT STARTED / NOT AUTHORIZED**.