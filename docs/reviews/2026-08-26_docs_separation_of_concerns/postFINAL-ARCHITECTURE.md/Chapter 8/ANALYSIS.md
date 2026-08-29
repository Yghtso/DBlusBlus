# Chapter 8 architecture review

## 1. Verdict

**CHAPTER 8 — TARGETED DOCUMENT FIXES RECOMMENDED**

Chapter 8 is technically coherent. Its persistent formats, routing rules, duplicate ordering, structural mutation protocol, concurrency model, WAL/recovery integration, and reclamation rules do not require frozen semantic review.

The chapter contains five localized project-time/development-roadmap issues and one optional cross-reference improvement.

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 5 |
| EDITORIAL | 1 |

No frozen architecture semantic question arose.

Primary scope: [docs/ARCHITECTURE.md §8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:5210), lines 5210–6713, ending immediately before Chapter 9.

## 2. Repository state and scope

Initial state:

| Item | Result |
|---|---|
| Working tree | Clean |
| Index | Clean |
| HEAD | `447fc43f40c9901c62e9b2629ba4e7c9a1206de0` |

Context consulted:

- Architecture front matter.
- Chapter 4: identifiers, RID addressing, FileKind/PageType registries, superblock, page publication, validation, compatibility, exhaustion.
- Chapter 5: physical RID and heap-version context.
- Chapter 7: BufferPool, guards, WAL-before-data, allocation, retirement.
- Chapters 10–11: MVCC and unique-key coordination.
- Chapter 12: `BTREE_MTR`, no-flush, publication, WAL-before-data.
- Chapter 13: atomic MTR redo and torn-page reconstruction.
- Chapter 14: index cleanup and RID reclamation.
- Chapters 15–16 and §21.8: DML, descriptors, catalog identity, offline index build.
- §39.1, §41.2, Appendix B.

Other live documents consulted:

- [DEVELOPMENT.md](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md:592)
- [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1660)
- [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:282)

`PROJECT_STATE.md` correctly owns the fact that B+ tree implementation is absent. Chapter 8 does not depend on or repeat that state.

---

## 3. Actual Chapter 8 heading inventory

| Section | Exact heading | Canonical responsibility | Document role |
|---|---|---|---|
| 8 | B+ Tree Indexing | Entire physical B+ tree contract | Architecture-appropriate |
| 8.1 | Scope and subsystem role | Feature and ownership boundary | Architecture-appropriate |
| 8.2 | Index file and superblock metadata | Index-file organization | Architecture-appropriate |
| 8.2.1 | BTREE superblock extension v1 | Exact specialized bytes | Architecture with role issue |
| 8.3 | Index key schema | Supported ordering/schema scope | Architecture with role issue |
| 8.3.1 | Key-schema fingerprint v1 | Stable descriptor fingerprint | Architecture-appropriate |
| 8.4 | User keys, physical keys, and RID encoding | `(user key,RID)` identity | Architecture-appropriate |
| 8.4.1 | Persisted RID encoding | Exact 16-byte RID format | Architecture-appropriate |
| 8.5 | Memcomparable user-key encoding | Order-preserving codec | Architecture-appropriate |
| 8.5.1 | Field presence prefix | NULL representation/order | Architecture-appropriate |
| 8.5.2 | BOOLEAN | Boolean key encoding | Architecture-appropriate |
| 8.5.3 | INT32 and DATE | Signed-32 sortable encoding | Architecture-appropriate |
| 8.5.4 | INT64 and TIMESTAMP | Signed-64 sortable encoding | Architecture-appropriate |
| 8.5.5 | FLOAT64 | Canonical total-order encoding | Architecture-appropriate |
| 8.5.6 | VARCHAR | Binary collation/escaping | Architecture-appropriate |
| 8.5.7 | Composite keys | Concatenation and lexicographic order | Architecture-appropriate |
| 8.6 | Encoded-key size and physical comparison | Size bound and total key order | Architecture-appropriate |
| 8.7 | Node-page organization | Shared slotted-node geometry | Architecture-appropriate |
| 8.8 | Node slot entry | Exact slot bytes | Architecture-appropriate |
| 8.9 | Leaf-node format | Leaf bytes and entry format | Architecture-appropriate |
| 8.10 | Internal-node format and routing semantics | Internal bytes and child relation | Architecture-appropriate |
| 8.10.1 | Routing lower bounds | Separator semantics | Architecture-appropriate |
| 8.11 | Node search | Search ownership | Architecture-appropriate |
| 8.11.1 | Internal search | `upper_bound` routing | Architecture-appropriate |
| 8.11.2 | Leaf search | `lower_bound` and range start | Architecture-appropriate |
| 8.12 | Split trigger and node compaction | Fit and compaction decision | Architecture-appropriate |
| 8.13 | Leaf split and sibling links | Leaf split outcome | Architecture-appropriate |
| 8.13.1 | Sibling-link publication | Bidirectional chain update | Architecture-appropriate |
| 8.14 | Internal split | Promotion/distribution | Architecture-appropriate |
| 8.15 | Root split and contraction | Root structural changes | Architecture-appropriate |
| 8.15.1 | Root split | New root/height publication | Architecture-appropriate |
| 8.15.2 | Root contraction | Child promotion/empty root | Architecture-appropriate |
| 8.16 | Occupancy and underflow | Soft byte-occupancy policy | Architecture-appropriate |
| 8.17 | Redistribution and merge | Rebalancing semantics | Architecture-appropriate |
| 8.17.1 | Leaf merge | Deterministic right-into-left merge | Architecture-appropriate |
| 8.17.2 | Internal rebalancing | Logical reconstruction | Non-architecture leakage |
| 8.18 | Tree-local free pages and safe reuse | Free-page format/allocation | Architecture-appropriate |
| 8.18.1 | Reuse safety | Detachment and lifetime gate | Architecture-appropriate |
| 8.19 | Concurrency and latch ordering | Physical concurrency boundary | Architecture-appropriate |
| 8.19.1 | Read traversal | Read latch coupling | Architecture-appropriate |
| 8.19.2 | Write traversal | Write crabbing/safety | Architecture-appropriate |
| 8.19.3 | Root metadata latch and optimistic root validation | Root publication/deadlock rule | Architecture-appropriate |
| 8.19.4 | Free-list and endpoint metadata updates | Optimistic metadata updates | Architecture-appropriate |
| 8.19.5 | Page-latch acquisition order | Vertical/horizontal order | Architecture-appropriate |
| 8.20 | Forward range scans and cursor lifetime | Guarded leaf traversal | Architecture-appropriate |
| 8.20.1 | Reverse scans | Deferred reverse-scan scope | Architecture with role issue |
| 8.21 | B+ tree and IndexKeyCodec API boundaries | Logical API responsibility | Architecture-appropriate |
| 8.22 | Duplicate keys, uniqueness, and MVCC | Physical/logical distinction | Architecture-appropriate |
| 8.22.1 | Transactional uniqueness boundary | Delegation to logical locks | Architecture-appropriate |
| 8.22.2 | Visibility | Heap recheck | Architecture-appropriate |
| 8.23 | UPDATE, DELETE, vacuum, and aborted user DML | Stale-entry lifecycle | Architecture-appropriate |
| 8.24 | BufferPool integration and index-scan cost | Buffer/cache and cost boundary | Architecture with role issue |
| 8.25 | Structural modification and WAL boundary | System MTR ownership | Architecture-appropriate |
| 8.25.1 | Page LSN and WAL-before-data participation | Flush ordering | Architecture-appropriate |
| 8.26 | Runtime structural publication | Reader-visible publication | Architecture with editorial issue |
| 8.27 | Page validation and corruption handling | Mandatory validation | Architecture-appropriate |
| 8.28 | Full-tree verifier | Required structural verification facility | Architecture-appropriate |
| 8.29 | B+ tree invariants | Consolidated normative invariants | Architecture-appropriate |

---

## 4. Section-by-section review

Legend: `OK` = clear/consistent; `—` = not applicable; `N` = clean note; `F` = finding. Columns correspond exactly to timelessness, ownership, analytical depth, terminology, layout, key/order, search/routing, split/root, delete/merge, concurrency, WAL/recovery, validation, reclamation, cross-reference, and semantic consistency.

| Section | Time | Owner | Analysis | Terms | Bytes | Key | Route | Split | Delete | Conc. | WAL | Valid. | Reclaim | X-ref | Semantic | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 8 | F | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | N | OK | FINDING |
| 8.1 | OK | OK | OK | OK | — | — | — | — | — | — | — | — | — | OK | OK | CLEAN |
| 8.2 | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | — | OK | OK | CLEAN |
| 8.2.1 | F | OK | OK | OK | OK | — | — | — | — | — | OK | OK | — | OK | OK | FINDING |
| 8.3 | F | OK | OK | OK | — | OK | — | — | — | — | — | OK | — | OK | OK | FINDING |
| 8.3.1 | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | OK | CLEAN |
| 8.4–8.4.1 | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | CLEAN |
| 8.5–8.5.7 | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | OK | CLEAN |
| 8.6 | OK | OK | OK | OK | OK | OK | OK | OK | — | — | — | OK | — | OK | OK | CLEAN |
| 8.7–8.9 | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | OK | CLEAN |
| 8.10–8.10.1 | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | — | OK | — | OK | OK | CLEAN |
| 8.11 | OK | OK | OK | OK | — | OK | OK | — | — | — | — | — | — | OK | OK | CLEAN |
| 8.11.1 | OK | OK | N | OK | — | OK | OK | — | — | — | — | — | — | OK | OK | CLEAN WITH NOTE |
| 8.11.2 | OK | OK | OK | OK | — | OK | OK | — | — | — | — | — | — | OK | OK | CLEAN |
| 8.12–8.14 | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | CLEAN |
| 8.15–8.15.2 | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.16 | OK | OK | OK | N | — | — | — | — | OK | OK | — | OK | — | OK | OK | CLEAN WITH NOTE |
| 8.17–8.17.1 | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.17.2 | F | F | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | FINDING |
| 8.18–8.18.1 | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.19–8.19.5 | OK | OK | OK | OK | — | — | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.20 | OK | OK | OK | OK | — | OK | OK | — | OK | OK | — | OK | OK | OK | OK | CLEAN |
| 8.20.1 | F | OK | OK | OK | — | — | — | — | — | OK | — | — | — | OK | OK | FINDING |
| 8.21–8.23 | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.24 | F | F | OK | OK | — | — | — | — | — | OK | — | — | — | OK | OK | FINDING |
| 8.25–8.25.1 | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 8.26 | OK | OK | OK | OK | — | — | OK | OK | OK | OK | OK | OK | OK | F | OK | FINDING |
| 8.27 | OK | OK | OK | OK | OK | OK | OK | — | — | OK | OK | OK | — | OK | OK | CLEAN |
| 8.28 | OK | OK | N | OK | — | OK | OK | OK | OK | — | — | OK | OK | OK | OK | CLEAN WITH NOTE |
| 8.29 | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |

Notes:

- §8.16 intentionally makes the ~25% occupancy threshold soft. Correctness does not depend on an exact percentage; write crabbing can conservatively retain ancestors or use any compliant rebalance policy.
- §8.11.1 deliberately pins binary-search-equivalent routing as a v1 performance constraint. It is not persistent-format truth, but it is explicit rather than accidental.
- §8.28 defines an architecture-required structural verifier and its invariants, not detailed test execution procedure.

---

## 5. Persistent-format and identity assessment

### B+ FileSuperblock

| Offset | Width | Encoding | Meaning/domain | Writer | Validator |
|---:|---:|---|---|---|---|
| 0–71 | 72 | LE fields | Common FileSuperblock prefix | File owner | §4.10 |
| 72 | 8 | LE | Nonzero expected `table_id` | B+ owner/MTR | Registered owner |
| 80 | 8 | LE | Valid root PageNo | Root MTR | Superblock + bounded root load |
| 88 | 8 | LE | Valid first leaf PageNo | Structural MTR | Endpoint validation |
| 96 | 8 | LE | Valid last leaf PageNo | Structural MTR | Endpoint validation |
| 104 | 8 | LE | Free-list head or `INVALID_PAGE_NO` | Free-list MTR | Free-list validator |
| 112 | 8 | LE | FNV-1a-64 schema fingerprint | Index creation | Descriptor cross-check |
| 120 | 4 | LE | `key_schema_version = 1` | Index creation | Format dispatch |
| 124 | 2 | LE | `tree_height >= 1` | Root MTR | Root-level cross-check |
| 126 | 2 | LE | `index_flags = 0` | Writer | Strict-zero validator |
| 128–8191 | 8064 | Zero | Reserved | Writer | Strict-zero validator |

Arithmetic:

```text
common prefix            72
six uint64 fields        48
schema version            4
tree height               2
index flags               2
                         ---
header                   128
reserved                8064
                         ---
page                    8192
```

Assessment: exact and consistent with Chapter 4.

### Root, height, and empty tree

- `tree_height` is the number of page levels.
- Leaf level is `0`.
- Root level is `tree_height - 1`.
- Empty tree: one valid empty leaf, height `1`, root/first/last all equal.
- An internal root with zero separators must contract before publication.
- Root PageNo, height, endpoint metadata, and runtime `root_generation` publish through the same protected structural MTR boundary.
- Root growth past `UINT16_MAX` fails before provisional mutation/publication.

No root/height ambiguity was found.

### Page types and states

| Type/state | Ordinary reachable? | Level/entries | Reuse meaning | Transitions |
|---|---|---|---|---|
| `SUPERBLOCK` | Metadata-managed | Root/endpoints/free head | Never node data | MTR mutation |
| `BTREE_LEAF` | Yes | Level 0; `(key,RID)` | Live tree page | New/reused publication; detach → FREE |
| `BTREE_INTERNAL` | Yes | Level >0; lower-bound separators | Live tree page | New/reused publication; detach → FREE |
| `BTREE_FREE` | No | Next-free link only | Safely detached page | FREE → private node → published |
| Private new/reused frame | No | Final type under MTR ownership | Not yet reachable | Publish atomically or restore |

There is no persisted parent pointer, page generation, or separate free-state flag.

---

## 6. Node and entry layouts

### Shared node geometry

```text
common header = 32 bytes
node header   = 32 bytes
total header  = 64 bytes
slot width    = 8 bytes
lower         = 64 + slot_count * 8
64 <= lower <= upper <= 8192
```

### Slot descriptor

| Relative offset | Width | Field |
|---:|---:|---|
| 0 | 2 | `entry_offset` |
| 2 | 2 | `entry_length` |
| 4 | 2 | `user_key_length` |
| 6 | 2 | `flags = 0` |

### Leaf page

| Offset | Width | Field |
|---:|---:|---|
| 0–31 | 32 | Common header |
| 32 | 2 | `level = 0` |
| 34 | 2 | `slot_count` |
| 36 | 2 | `lower` |
| 38 | 2 | `upper` |
| 40 | 4 | `flags = 0` |
| 44 | 8 | Previous leaf or invalid |
| 52 | 8 | Next leaf or invalid |
| 60 | 4 | Reserved zero |
| 64… | variable | Slot directory/free region/packed entries |

Leaf entry:

| Region | Width |
|---|---:|
| Encoded user key | `k`, where `1 <= k <= 1024` |
| RID | 16 |
| `entry_length` | `k + 16` |
| Slot plus entry storage | `k + 24` |

### Internal page

| Offset | Width | Field |
|---:|---:|---|
| 0–31 | 32 | Common header |
| 32 | 2 | `level > 0` |
| 34 | 2 | `slot_count` |
| 36 | 2 | `lower` |
| 38 | 2 | `upper` |
| 40 | 4 | `flags = 0` |
| 44 | 8 | Leftmost child PageNo |
| 52 | 12 | Reserved zero |
| 64… | variable | Slot directory/free region/packed entries |

Internal entry:

| Region | Width |
|---|---:|
| Encoded user key | `k` |
| Separator RID | 16 |
| Right child PageNo | 8 |
| `entry_length` | `k + 24` |
| Slot plus entry storage | `k + 32` |

### Free page

| Offset | Width | Field |
|---:|---:|---|
| 0–31 | 32 | Common header |
| 32 | 8 | Next free PageNo or invalid |
| 40–8191 | 8152 | Reserved zero |

### Capacity arithmetic

| Case | Formula | Maximum |
|---|---|---:|
| Structural slot-only bound | `floor((8192-64)/8)` | 1016 |
| Leaf, minimum key `k=1` | `floor(8128/(1+24))` | 325 |
| Internal, minimum key `k=1` | `floor(8128/(1+32))` | 246 |
| Leaf, maximum key `k=1024` | `floor(8128/1048)` | 7 |
| Internal, maximum key `k=1024` | `floor(8128/1056)` | 7 |

The documented `slot_count <= 1016` is a safe structural upper bound; actual capacity is constrained further by entry bytes.

No byte-layout or arithmetic mismatch was found.

---

## 7. Key, RID, order, and routing assessment

### Key-order semantics

| Type | Canonical encoding/order |
|---|---|
| NULL | Presence byte `0x00`; sorts first |
| Non-NULL | Presence byte `0x01`, then payload |
| BOOLEAN | `0x00` false, `0x01` true |
| INT32/DATE | Flip sign bit, encode big-endian |
| INT64/TIMESTAMP | Flip sign bit, encode big-endian |
| FLOAT64 | Normalize signed zero/NaN, sign-transform, big-endian |
| VARCHAR | Binary collation; zero escaped `00 FF`; terminator `00 00` |
| Composite | Concatenated self-delimiting component encodings |
| Physical key | Lexicographic user bytes, then numeric RID |

NULL, FLOAT64, VARCHAR, and composite behavior agree with Chapter 17. Native DESC and locale-aware collation are outside the v1 baseline.

### RID

```text
heap_file_id  uint32
heap_page_no  uint64
heap_slot_id  uint16
reserved      uint16 = 0
```

Total: 16 bytes. Ordering is numeric lexicographic `(FileId,PageNo,SlotId)`. The registered index owner additionally checks the expected heap FileId.

### Separator and routing table

Internal representation:

```text
C0, (K1 -> C1), (K2 -> C2), ... (Kn -> Cn)
```

Each `Ki` is an inclusive lower bound for `Ci`.

| Search relation | Selected child |
|---|---|
| `T < K1` | `C0` |
| `T == K1` | `C1` |
| `K1 < T < K2` | `C1` |
| `T == K2` | `C2` |
| `T >= Kn` | `Cn` |

This is mechanically `upper_bound(separators,T)`. Equality routing is unambiguous.

Stale-low separators after ordinary deletion remain safe: they may route an absent key to a subtree but cannot route an existing key away from its subtree. Structural redistribution or child replacement must update the affected boundary.

### Duplicate ordering

| User-key relation | RID relation | Physical order | Search/split implication |
|---|---|---|---|
| Different | Irrelevant | Encoded user-key order | Normal range routing |
| Equal | Lower RID | Earlier | Deterministic duplicate order |
| Equal | Equal RID | Duplicate physical key, forbidden | Corruption except proven same-operation replay |
| Equal | Higher RID | Later | Duplicate runs may span leaves |

Equality lookup begins at `(K,MIN_RID)` and follows forward leaf links until the user key changes. Splits may divide equal SQL keys because RID preserves a complete physical ordering.

No key-encoding, duplicate-order, separator, or routing ambiguity was found.

---

## 8. Mutation, split, delete, and reclamation

### Insert/split table

| Operation | Pages touched | Key/routing effect | WAL owner | Failure survivor |
|---|---|---|---|---|
| Leaf insert | Leaf | Insert `(key,RID)` in order | One `BTREE_MTR` | Exact pre-MTR leaf |
| Leaf split | Old/new leaf, parent, adjacent sibling/end metadata as needed | Parent gets first key of right leaf | One MTR | Exact old tree |
| Parent insert | Parent | Installs lower bound and right child | Same MTR | Exact prior routing |
| Internal split | Old/new internal, parent | Promote `Km`; remove it from child level | Same MTR | Exact old tree |
| Root split | Old root, split page, new root, superblock | Height +1; new root published | Same MTR | Old root/tree |
| Root contraction | Root, child, superblock, retired page | Child becomes root; height −1 | Same MTR | Previous valid root |

Variable-length splits are byte-balanced, not entry-count balanced. Exact balance is implementation freedom; correctness requires fitting pages, valid ordering, and legal nonempty leaf results.

### Delete/merge table

| Operation | Trigger/policy | Separator effect | Retirement |
|---|---|---|---|
| Exact erase | Exact `(key,RID)` | May leave stale-low separator | None unless rebalance |
| Redistribution | Underfull-policy decision | Boundary separator must change | None |
| Leaf merge | Combined entries fit | Remove right child separator | Right leaf detached then freed |
| Internal merge | Reconstruct child/separator sequence | Parent boundary removed/transformed | Removed page detached then freed |
| Root contraction | Internal root has zero separators | Child becomes root | Old root retired |
| Last-entry delete | Empty root leaf remains | No invalid-root state | No root retirement |

The 25% threshold is explicitly soft; sparse pages are not corruption. Implementations may choose when to rebalance, but any chosen structural operation must obey latch/MTR/publication rules.

### Reclamation

| Retired object | Reachability gate | Buffer protection | Persistent transition | Reuse |
|---|---|---|---|---|
| B+ node page | Removed from parent/root/sibling routes | No guard/pin or legal stale reference | `BTREE_FREE` via MTR | Yes, through free-list MTR |
| Entire index file | Catalog/object retirement gate | BufferPool file drain | Durable unlink protocol | FileId never reused |
| Referenced heap RID | Every index entry absent; chain/grace rules | ReadEpoch protection | DEAD→UNUSED protocol | Only after Chapter 14 grace |

B+ page reuse does not require the heap RID epoch mechanism because tree traversals retain page guards and do not expose long-lived raw PageNos.

---

## 9. Latching, scans, and concurrency

### Latch/guard table

| Operation | Guard mode | Ancestors retained? | Acquisition/release | Restart condition |
|---|---|---|---|---|
| Point search | Read | Parent until child acquired | Parent → child, then release parent | Root generation mismatch |
| Insert/delete | Write | Retained while child unsafe | Parent → child | Unsafe path/revalidation failure |
| Root acquisition | Metadata snapshot then page latch | N/A | Release metadata before waiting for page latch | Root/generation changed |
| Root publication | Write-latched pages then metadata latch | Required structural pages | Page latch(es) → metadata latch | MTR failure |
| Leaf range handoff | Read | Current leaf until next guarded | Left/current → right/next | Corrupt link/progress failure |
| Adjacent-page structural work | Write | As needed | Left-to-right key order | Reverse-order need |
| Free-list pop | Free-page write guard plus metadata latch | N/A | Snapshot, page latch, validate metadata | Head changed |

Transaction locks remain outside this latch domain. No transaction-level wait may retain a B+ page latch.

### Concurrency matrix

| Race | Coordination | Legal outcomes | Forbidden outcome |
|---|---|---|---|
| Search/search | Shared page guards | Both read same valid tree | Invalid/stale page access |
| Search/insert | Latch coupling | Search sees complete pre/post publication state | Half-inserted entry |
| Insert/insert same leaf | Exclusive leaf latch/MTR | Serialized ordered entries | Duplicate exact physical key |
| Search/split | Parent/child latches and publication | Old or complete new route | Missing reachable key |
| Split/split same path | Write crabbing | Serialized/restarted structural MTRs | Conflicting parent separators |
| Duplicate concurrent insert | Physical RID order; logical lock for UNIQUE | Both nonunique entries or one logical conflict | Page latch used as UNIQUE lock |
| Delete/search | Write/read latch exclusion | Entry seen before deletion or absent after | Dereference after page reuse |
| Delete/merge/insert | Ancestor/sibling order | Serialized/restarted mutation | Detached live child |
| Root split/search | Root metadata generation validation | Old root before publication or new root after | Pinning reused former root |
| Retirement/search | Structural detachment plus guards | Existing guarded traversal drains | Reuse while traversable |
| Range scan/split | Guarded current/next handoff | Complete ordered traversal under allowed visibility | Miss/duplicate from unsafe handoff |
| Recovery/private access | Recovery-private ownership | Validated complete tree before READY | Untrusted private bytes published |

Parallel-ready compatibility is preserved. Chapter 8 rejects a tree-wide write lock for ordinary writes and permits disjoint-page activity.

---

## 10. WAL, recovery, validation, and failures

### WAL/recovery table

| Mutation | Affected state | WAL/MTR scope | `page_lsn` | Crash/recovery |
|---|---|---|---|---|
| Leaf insert/erase | Leaf | One `BTREE_MTR` | Common MTR LSN | Redo/skip by page LSN |
| Split | Children, parent, siblings/endpoints | One MTR | Same LSN on all affected pages | Complete MTR replay only |
| Redistribution/merge | Siblings, parent, endpoints/free page | One MTR | Same MTR LSN | Pre-state or complete post-state |
| Root change | Root pages, superblock/runtime metadata | One MTR | Same MTR LSN | Atomic root reconstruction |
| Free-list pop/push | Free page, superblock, replacement page | One MTR | Same MTR LSN | No page simultaneously free/live |
| New appended B+ page | Full image plus publication metadata | Publishing MTR | MTR LSN | Missing/torn tail reconciled |
| Later writeback | Stable copied image | BufferPool | Existing page LSN | WAL durable before data |

A valid append followed by publication failure is noncontinuable; pre-append known failure restores exact pages, frame metadata, root metadata, free-list state, and appended tail.

### Validation table

| Structure | Local invariant | Tree-level invariant | Owner/result |
|---|---|---|---|
| Superblock | Kind, IDs, version, fingerprint, endpoints, height, zeros | Root/endpoints agree | Owner validator; corruption/unsupported |
| Leaf | Geometry, canonical keys/RIDs, strict order, sibling domains | Global leaf order/chain | `CORRUPT_INDEX` |
| Internal | Geometry, `N+1` children, strict separators | Subtree ranges, unique parentage, depth | `CORRUPT_INDEX` |
| Free page | Exact header/link/zeros | Acyclic, unique, disjoint | `CORRUPT_INDEX` |
| Root/height | Root level = height−1 | All leaves same depth | `CORRUPT_INDEX` |
| Key schema | Version/fingerprint/canonical encoding | Descriptor agreement | Unsupported or corruption |
| RID | Sentinel/reserved/heap owner | Protected heap dereference | Corruption/stale protocol |
| Child reference | In range, distinct, non-self | Correct owner/type/level/range | `CORRUPT_INDEX` |

Checksum/common framing precedes `page_lsn` trust. Future recognized formats produce unsupported-format outcomes rather than corruption.

### Owner validation

| Dimension | Expected source | Persisted location | Before use? |
|---|---|---|---|
| FileKind | Registered file/catalog | FileSuperblock | Yes |
| FileId | Managed file registration/name | External owner + page identity | Yes |
| IndexId | IndexDescriptor | Superblock `object_id` | Yes |
| TableId | IndexDescriptor | Extension `table_id` | Yes |
| PageNo | Requested PageId | Common page header | Yes |
| PageType | Parent/operation | Common page header | Yes |
| Root/height | Superblock/runtime metadata | Extension/node level | Yes |
| Key schema/version | IndexDescriptor | Superblock fingerprint/version | Yes |
| Heap identity | TableDescriptor | Leaf/separator RID FileId | Yes |

### Failure matrix

| Failure | Tree survivor | Caller/lifecycle result |
|---|---|---|
| Leaf corruption | No malformed residency/traversal | `CORRUPT_INDEX`; online persistent corruption follows §39.1 |
| Unsupported page/file version | No v1 interpretation | Applicable unsupported-format result |
| Key >1024 encoded bytes | Tree unchanged | Explicit pre-publication operation failure |
| Split allocation/resource failure | Exact old tree | Resource/PageNo/BufferPool result |
| `NO_REPLACEABLE_FRAME` | Tree unchanged | Ordinary bounded resource result |
| WAL reservation/known append failure | Exact pre-MTR state | Structured local failure |
| WAL append uncertainty | Protected nonordinary state | `STORAGE_NONCONTINUABLE` |
| Parent insert/root update failure before append | Exact old routing/root | Local failure |
| Publication failure after valid append | No rollback-and-continue | Noncontinuable |
| Dirty flush failure | Valid dirty resident tree | Retry/failure; no false clean |
| Retirement/free transition failure | Page remains nonreusable | Operation failure/noncontinuable as classified |
| Height exhaustion | Existing maximum-height tree | Deterministic root-growth failure |

Buffer exhaustion remains distinct from disk `RESOURCE_FULL`, PageNo exhaustion, WAL exhaustion, page-local capacity, corruption, and oversized-key failure.

---

## 11. Cross-chapter consistency

| Owner | Chapter 8 consumption | Result |
|---|---|---|
| Chapter 1 | Parallel-ready concurrent design | Consistent |
| Chapter 2 | Resolved immutable descriptors; no SQL ownership | Consistent |
| Chapter 3 | Lifecycle/open/shutdown | Consistent but specialized |
| Chapter 4 | File/page bytes, RID, validation, publication, exhaustion | Consistent |
| Chapter 5 | Physical tuple-version RID | Consistent |
| Chapter 6 | FSM remains unrelated advisory state | Consistent |
| Chapter 7 | BufferPool/PageGuard/WAL-before-data | Consistent |
| Chapters 9–10 | Snapshot visibility remains heap-owned | Consistent |
| Chapter 11 | Transactional uniqueness and logical locks | Consistent but specialized |
| Chapter 12 | Atomic `BTREE_MTR` and no-flush publication | Consistent |
| Chapter 13 | Atomic MTR redo/private reconstruction | Consistent |
| Chapter 14 | Exact stale-entry cleanup and RID grace | Consistent |
| Chapter 15 | Heap-before-index DML integration | Consistent |
| Chapter 16 | Immutable index/table descriptors | Consistent |
| §21.8 | Offline CREATE INDEX publication | Consistent |
| §39.1 | Statement/storage failure effects | Consistent |
| §41.2 | Architecture-level verification obligations | Consistent |
| Appendix B | Chapter 8 invariant ownership | Consistent |

No previous-chapter regression was found. Chapter 7 compatibility is complete.

---

## 12. Explicit cross-reference audit

| Source | Target | Purpose | Exists/owner/precision | Status |
|---|---|---|---|---|
| 8.2.1 | §4.14.6 | Key-schema version handling | Yes/correct/precise | Clean |
| 8.2.1 | §8.3.1 | Fingerprint definition | Yes/correct/precise | Clean |
| 8.9, 8.10 | §8.7 | Shared node rules | Yes/correct/precise | Clean |
| 8.15.1 | §4.3.2 | Height exhaustion | Yes/correct/precise | Clean |
| 8.18 | §§4.13.4–4.13.5 | Free-list/global validation | Yes/correct/precise | Clean |
| 8.20 | §4.13.5 | Leaf-handoff validation | Yes/correct/precise | Clean |
| 8.22.1 | §§11.8–11.10 | Unique locks/recheck | Yes/correct/precise | Clean |
| 8.22.2 | §11.10 | Constraint predicate | Yes/correct/precise | Clean |
| 8.22.2 | Chapter 10 | Snapshot visibility | Yes/correct; chapter-level | Clean |
| 8.25 | Chapter 12 | MTR ownership | Yes/correct | Clean |
| 8.25 | §§12.10.3–12.10.4 | Failure/abort distinction | Yes/correct/precise | Clean |
| 8.25.1 | §12.10.2 | No-flush barrier | Yes/correct/precise | Clean |
| 8.26 | §12.10.3 | Runtime publication | Yes/correct/precise | Clean |
| 8.26 | “later WAL/recovery protocol” | Crash atomicity | Owner exists but target vague | Editorial finding |
| 8.27 | §§4.13.4–4.13.5 | Mandatory validation | Yes/correct/precise | Clean |
| 8.28 | §§4.13.5, 4.13.9 | Full verifier | Yes/correct/precise | Clean |
| 8.29 | §§4.13.4–4.13.5 | Bounded traversal/allocation | Yes/correct/precise | Clean |

---

## 13. Terminology and normative language

### Canonical terminology

| Term | Exact meaning |
|---|---|
| User key | SQL-visible indexed-column tuple |
| Physical key | `(encoded_user_key,RID)` |
| Separator | Complete physical lower bound for its right child |
| Entry | Leaf key/RID or internal separator/right-child record |
| Root | Page named by superblock/runtime root metadata |
| Level | Leaf `0`; parent = child+1 |
| Height | Number of levels; root level = height−1 |
| Sibling | Persisted adjacent leaf in bidirectional chain |
| Safe node | Current operation cannot propagate split/rebalance above it |
| Underfull | Soft rebalance candidate, approximately below 25% byte occupancy |
| Stale-low separator | Retained lower bound below current right-child minimum |
| Free page | Safely detached `BTREE_FREE` page |
| Stale index entry | Structurally valid entry referencing invisible/dead/aborted version |
| Unique index | Physically duplicate-capable tree with transactional uniqueness above it |

No correctness-relevant ambiguous synonym was found. “Underfull” is deliberately policy-oriented rather than a corruption threshold.

### Normative-language assessment

| Area | Strength |
|---|---|
| BufferPool access | `MUST NOT` bypass with private tree/cache |
| Key/schema agreement | `MUST` |
| Reserved bytes/flags | `MUST` write/reject |
| Key-size rejection | `MUST`, `MUST NOT` truncate |
| Separator maintenance | `MUST` on boundary movement |
| Search algorithm | Production internal lookup `MUST NOT` linear scan |
| Reuse safety | `MUST NOT` expose long-lived raw PageNo cursors |
| Root latch ordering | `MUST NOT` wait for page latch under metadata latch |
| Cursor lifetime | View `MUST NOT` outlive guard |
| Uniqueness | `MUST NOT` use physical presence alone |
| BufferPool cache ownership | `MUST NOT` create second private cache |
| Structural publication | Concurrent operations `MUST NOT` observe incomplete routes |
| WAL ordering | Explicitly mandatory through Chapters 7/12 |

Normative strength is sufficient and was not weakened by project-stage prose.

---

## 14. Temporal-language classification

Classification: A runtime ordering; B transaction/MVCC history; C format evolution; D durable v1 scope; E navigation; F project chronology/status.

| Section | Phrase | Class | Safe? | Action/finding |
|---|---|---:|---|---|
| 8.1 | “persistence is added later” | D | Yes | Counterfactual prohibition on a nonconforming architecture |
| 8.2.1 | “is now byte-exact” | F | No | Timeless rewrite; MINOR |
| 8.3 | “may initially be produced” | F | No | Timeless v1 capability wording; MINOR |
| 8.6 | “Deferred key-space extensions” | D | Yes | Explicit v1 scope |
| 8.10.1 | “After ordinary deletion…” | A | Yes | Runtime tree state |
| 8.15 | “current root” | A | Yes | Runtime identity |
| 8.17.2 | “later implementation may optimize” | F | No | Remove development sequencing; MINOR |
| 8.19.2 | “Initial insert/delete uses…” | A | Yes | Initial traversal of an operation |
| 8.19.3 | “current root…” | A | Yes | Runtime metadata |
| 8.19.5 | “later/right…earlier/left” | A | Yes | Key-order relationship |
| 8.20 | “current leaf/guard” | A | Yes | Runtime cursor state |
| 8.20.1 | “initial format” | C/D | Yes | Persisted v1 baseline |
| 8.20.1 | “reverse scans are deferred” | D | Yes | Explicit v1 scope |
| 8.20.1 | “A later reverse-cursor design may…” | F | No | Remove roadmap wording; MINOR |
| 8.22 | “current transaction/current-owner” | B | Yes | Transaction semantics |
| 8.22.2, 8.23 | “continue/later” visibility/vacuum | A/B | Yes | Runtime maintenance sequence |
| 8.24 | “If future profiling…” | F | No | Timeless invariant; MINOR |
| 8.24 | “Potential later…improvements” | F | No | State as deferred scope or remove; same finding |
| 8.25 | “transaction later aborts” | B | Yes | Transaction history |
| 8.26 | “later WAL/recovery protocol” | E | Yes, vague | Precise cross-reference; EDITORIAL |

### Document-ownership table

| Section | Content | Current owner | Correct owner | Finding? |
|---|---|---|---|---|
| 8.2.1 | “now byte-exact” history marker | Architecture | Timeless Architecture | Yes |
| 8.3 | “initially” use executor sorting | Architecture | Timeless Architecture; sequencing belongs Development | Yes |
| 8.17.2 | Reconstruction first, optimize later | Architecture | DEVELOPMENT for sequencing; Architecture only semantic equivalence | Yes |
| 8.20.1 | Later reverse-cursor design | Architecture | Timeless deferred-scope architecture | Yes |
| 8.24 | Future profiling/later improvements | Architecture | Timeless invariant/deferred scope; roadmap otherwise Development | Yes |
| 8.26 | Vague later protocol navigation | Architecture | Precise Architecture cross-reference | Editorial |
| 8.28 | Required verifier capabilities | Architecture | Architecture for facility/invariants; procedure remains Verification | No |

No current-state, PROJECT_STATE, devlog, test-result, source-layout, or detailed verification-procedure leakage was found.

---

## 15. Analytical-depth assessment

| Mechanism | Rule clear? | Rationale present? | Assessment |
|---|---|---|---|
| Physical `(key,RID)` order | Yes | Duplicate addressability/split/search/delete explained | Analytically sufficient |
| Memcomparable encoding | Yes | Removes polymorphic hot-path comparison | Analytically sufficient |
| Separator lower bounds | Yes | Stale-low safety explained | Analytically sufficient |
| Duplicate scans | Yes | RID total order and sentinel bounds explained | Analytically sufficient |
| Heap recheck | Yes | Physical hit does not prove visibility/conflict | Analytically sufficient |
| Variable-byte split | Yes | Entry-count split rejected due variable keys | Analytically sufficient |
| Root publication | Yes | MTR and root generation prevent partial roots | Analytically sufficient |
| Soft underflow | Yes | Avoids oscillation/write amplification | Analytically sufficient |
| Latch ordering | Yes | Root and horizontal deadlock rationale present | Analytically sufficient |
| Cursor lifetime | Yes | Prevents merge/reuse race | Analytically sufficient |
| WAL/MTR | Yes | Structural state independent of user abort | Analytically sufficient |
| Free-page reuse | Yes | Detachment/guard lifetime rationale present | Analytically sufficient |
| Uniqueness | Yes | Physical presence cannot decide ownership | Analytically sufficient |
| BufferPool ownership | Yes | Prevents second cache/lifetime owner | Analytically sufficient |

No analytical-depth finding was identified.

---

## 16. High-priority technical matrix

| # | Item | Result | Basis |
|---:|---|---|---|
| 1 | Chapter-8 ownership boundary | CONSISTENT | Physical tree only |
| 2 | BTREE FileKind | CONSISTENT | Code 2 |
| 3 | Specialized superblock size | CONSISTENT | 128 bytes |
| 4 | Root PageNo semantics | CONSISTENT | Always valid after initialization |
| 5 | Height semantics | CONSISTENT | Number of levels |
| 6 | Empty-tree representation | CONSISTENT | One empty root leaf |
| 7 | PageType registry | CONSISTENT | Internal/leaf/free |
| 8 | Internal header size | CONSISTENT | 64 |
| 9 | Leaf header size | CONSISTENT | 64 |
| 10 | Free-page format | CONSISTENT | 40-byte header |
| 11 | Key encoding owner | CONSISTENT | `IndexKeyCodec` |
| 12 | Key comparison | CONSISTENT | Encoded bytes then RID |
| 13 | NULL ordering | CONSISTENT | NULLS FIRST |
| 14 | Composite ordering | CONSISTENT | Lexicographic components |
| 15 | Key size limit | CONSISTENT | 1024 bytes |
| 16 | Leaf entry format | CONSISTENT | Key + RID |
| 17 | RID format | CONSISTENT | Exact 16 bytes |
| 18 | Internal entry | CONSISTENT | Key + RID + right child |
| 19 | Separator semantics | CONSISTENT | Right-child lower bound |
| 20 | Equality routing | CONSISTENT | Equality routes right |
| 21 | Duplicate total order | CONSISTENT | RID tiebreak |
| 22 | Nonunique equality scan | CONSISTENT | `(K,MIN_RID)` forward |
| 23 | Duplicates across leaves | CONSISTENT | Forward continuation |
| 24 | Unique semantics owner | CONSISTENT BUT SPECIALIZED | Chapter 11 |
| 25 | Unique NULL semantics | CONSISTENT BUT SPECIALIZED | Any NULL permits duplicates |
| 26 | Index MVCC model | CONSISTENT | No MVCC in entry |
| 27 | Heap recheck | CONSISTENT | Mandatory |
| 28 | Stale-entry legality | CONSISTENT | Vacuum-owned cleanup |
| 29 | Leaf insert | CONSISTENT | Ordered MTR mutation |
| 30 | Leaf capacity | CONSISTENT | Byte geometry |
| 31 | Leaf split trigger | CONSISTENT | Fit after compaction |
| 32 | Split distribution | CONSISTENT | Byte-balanced, valid pages |
| 33 | Variable-length split | CONSISTENT | Byte based |
| 34 | Duplicate-run split | CONSISTENT | Physical key order permits |
| 35 | Parent insertion | CONSISTENT | Right lower bound |
| 36 | Root split | CONSISTENT | New internal root |
| 37 | Root publication | CONSISTENT | Atomic MTR/runtime publication |
| 38 | Internal split | CONSISTENT | Promote/remove `Km` |
| 39 | Level invariant | CONSISTENT | Child+1 |
| 40 | Child validation | CONSISTENT | Local + traversal + verifier |
| 41 | Parent pointer | N/A | Not persisted |
| 42 | Sibling links | CONSISTENT | Bidirectional leaves |
| 43 | Range scan | CONSISTENT | Forward guard-coupled |
| 44 | Search concurrency | CONSISTENT | Read coupling |
| 45 | Latching model | CONSISTENT | Read/write guards + crabbing |
| 46 | Safe-page definition | CONSISTENT BUT SPECIALIZED | Relative to chosen structural action |
| 47 | Insert latching | CONSISTENT | Unsafe ancestors retained |
| 48 | Delete latching | CONSISTENT | Rebalance propagation protected |
| 49 | Delete semantics | CONSISTENT | Exact physical erase |
| 50 | Underflow rule | CONSISTENT BUT SPECIALIZED | Soft policy, not validity |
| 51 | Redistribution | CONSISTENT | Parent boundary updated |
| 52 | Merge | CONSISTENT | Right into left for leaves |
| 53 | Root contraction | CONSISTENT | Single child promoted |
| 54 | Empty after delete | CONSISTENT | Empty leaf root |
| 55 | Index page reuse | CONSISTENT | Detach then free |
| 56 | PageNo reuse | CONSISTENT BUT SPECIALIZED | Tree free list only |
| 57 | Multi-page MTR atomicity | CONSISTENT | One logical record |
| 58 | WAL-before-data | CONSISTENT | No exception |
| 59 | Page LSN | CONSISTENT | Common MTR LSN |
| 60 | Split recovery | CONSISTENT | Atomic MTR redo |
| 61 | Merge recovery | CONSISTENT | Atomic MTR redo |
| 62 | Root recovery | CONSISTENT | Superblock affected page |
| 63 | Local validation | CONSISTENT | Mandatory L1 |
| 64 | Tree-level validation | CONSISTENT | Explicit L3 verifier |
| 65 | Owner validation | CONSISTENT | File/index/table/heap chain |
| 66 | Descriptor/fingerprint | CONSISTENT | Versioned exact cross-check |
| 67 | Uniqueness concurrency | CONSISTENT BUT SPECIALIZED | Chapter 11 locks |
| 68 | Height exhaustion | CONSISTENT | Checked pre-MTR failure |
| 69 | Error taxonomy | CONSISTENT BUT SPECIALIZED | Chapter 4/7/39 owners |
| 70 | Correctness policy invention | CONSISTENT | None required |

## 17. Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | No current implementation status | CONSISTENT |
| 3 | No Phase-2 narration | CONSISTENT |
| 4 | No implementation sequencing | FINDING |
| 5 | No verification-procedure leakage | CONSISTENT |
| 6 | No devlog/history | CONSISTENT |
| 7 | No PROJECT_STATE facts | CONSISTENT |
| 8 | No source-layout coupling | CONSISTENT |
| 9 | No temporary algorithm promoted accidentally | CONSISTENT |
| 10 | Separator terminology precise | CONSISTENT |
| 11 | Duplicate terminology precise | CONSISTENT |
| 12 | Root/height terminology precise | CONSISTENT |
| 13 | Split terminology precise | CONSISTENT |
| 14 | Merge/reuse terminology precise | CONSISTENT |
| 15 | Ownership references precise | FINDING, editorial only |
| 16 | RID/heap-recheck rationale | CONSISTENT |
| 17 | Split/root-publication rationale | CONSISTENT |
| 18 | Concurrency rationale | CONSISTENT |
| 19 | Recovery/reclamation rationale | CONSISTENT |
| 20 | Readable without implementation status | CONSISTENT |

---

## 18. Findings

### BLOCKING findings

None.

### MAJOR findings

None.

### MINOR findings

#### M1 — §8.2.1 temporal review residue

- Evidence: “The v1 B+ superblock extension above is **now byte-exact**.”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Local.
- Explanation: “Now” makes a persistent-format contract read as the result of a recent event.
- Canonical comparison: The extension is simply the byte-exact v1 format.
- Consequence: Documentation chronology; no technical ambiguity.
- Owner: Timeless `ARCHITECTURE.md`.
- Future action: **B. TIMELESSNESS REWRITE**.

#### M2 — §8.3 implementation-stage fallback wording

- Evidence: “A descending SQL result may **initially** be produced by executor sorting rather than native reverse index traversal.”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Local, related to §8.20.1.
- Explanation: “Initially” implies a staged implementation path. The durable rule is that native reverse traversal is deferred and executor sorting may satisfy descending output.
- Consequence: Architecture reads partly as development sequencing.
- Owner: Timeless `ARCHITECTURE.md`; implementation ordering belongs in `DEVELOPMENT.md`.
- Future action: **B. TIMELESSNESS REWRITE**.

#### M3 — §8.17.2 implementation sequencing

- Evidence: “A clear reconstruction … is preferable … **until** equivalent correctness has been established; **later implementation may optimize**…”
- Severity/type: MINOR — DOCUMENT OWNERSHIP.
- Scope: Local.
- Explanation: Logical reconstruction versus optimized copying is legitimate implementation freedom, but “first establish, later optimize” is contributor sequencing.
- Canonical comparison: Architecture should require semantically equivalent child/separator reconstruction without prescribing when optimization occurs.
- Consequence: DEVELOPMENT material leaks into the architecture contract.
- Correct owner: `DEVELOPMENT.md` for sequencing; Architecture for semantic equivalence.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX**.

#### M4 — §8.20.1 reverse-cursor roadmap wording

- Evidence: “A **later** reverse-cursor design may use restart or nonblocking-latch techniques.”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Local.
- Explanation: Native reverse scans are already durably deferred. Speculating about a later design is roadmap narration.
- Canonical comparison: State only the v1 exclusion and any constraint a future architecture revision must preserve.
- Consequence: No semantic defect; weakens timeline independence.
- Owner: Timeless Architecture scope statement.
- Future action: **B. TIMELESSNESS REWRITE**.

#### M5 — §8.24 future optimization roadmap

- Evidence:
  - “If **future profiling** justifies special treatment…”
  - “Potential **later** execution/storage improvements include RID batching…”
- Severity/type: MINOR — TEMPORALITY.
- Scope: Local.
- Explanation: The BufferPool-ownership invariant is architectural; profiling chronology and a list of possible later work are roadmap language.
- Canonical comparison: Any special caching treatment must remain BufferPool-owned; enhancements outside v1 should be called deferred or omitted.
- Consequence: Architecture mixes canonical constraints with possible development evolution.
- Correct owner: Architecture for the invariant/deferred scope; DEVELOPMENT for implementation roadmap.
- Future action: **B. TIMELESSNESS REWRITE**.

### EDITORIAL findings

#### E1 — §8.26 vague WAL/recovery reference

- Evidence: “Crash atomicity is provided by the **later WAL/recovery protocol**.”
- Severity/type: EDITORIAL — CROSS-REFERENCE.
- Scope: Cross-section.
- Explanation: Precise owners exist.
- Canonical comparison: `BTREE_MTR` construction/publication is in §§12.10.2–12.10.3 and 12.12; atomic redo/torn-page handling is in §§13.13.3–13.14.
- Consequence: Navigation only; the surrounding protocol is already coherent.
- Owner: Architecture navigation.
- Future action: **G. CROSS-REFERENCE FIX**.

---

## 19. Documentation-model result

| Question | Answer |
|---|---|
| Analytical rather than chronological? | **YES**, except five localized phrases |
| Current-state narration? | **NO** |
| DEVELOPMENT sequencing leakage? | **YES**, §8.17.2 |
| VERIFICATION procedure leakage? | **NO** |
| PROJECT_STATE leakage? | **NO** |
| Devlog/history leakage? | **NO** |
| Implementation absence treated as optionality? | **NO** |
| Terminology precise enough? | **YES** |
| Rationale sufficient? | **YES** |
| Readable without index implementation status? | **YES** |
| Fully timeless canonical v1 contract as written? | **NO**, targeted wording cleanup is needed |

Source-layout coupling: none.

Implementation freedom is properly retained for container representation, physical byte movement, exact split balance, rebalance timing, and serialized scheduling. It is intentionally constrained for persistent bytes, physical ordering, `upper_bound`/`lower_bound` search behavior, CLOCK/BufferPool ownership inherited from Chapter 7, latch ordering, and MTR publication.

---

## 20. Follow-up verification gap

**FOLLOW-UP VERIFICATION GAP**

[VERIFICATION.md’s B+ section](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1660) lists the major structural cases, randomized oracle, duplicate stress, and concurrency stress. Generic WAL/MTR rollback, uniqueness, vacuum, and numeric-exhaustion procedures provide useful shared coverage.

However, several Chapter-8-specific obligations are named without complete deterministic fixtures, barriers, and oracles:

1. Exact BTREE superblock/node/slot/free-page byte and reserved-zero matrices.
2. Property comparison of every `IndexKeyCodec` type against Chapter 17 semantic order.
3. Exact separator equality routing and stale-low separator cases.
4. Deterministic variable-byte leaf/internal split boundary and promoted-key cases.
5. Duplicate runs split across multiple leaves during redistribution and merge.
6. Root split/contraction publication with `root_generation` validation/restart.
7. Root-metadata-latch versus page-latch anti-deadlock barriers.
8. Parent-before-child and left-to-right latch-order restart cases.
9. Safe/unsafe write-crabbing ancestor-release cases.
10. Forward cursor handoff raced deterministically with split, merge, and page reuse.
11. Tree-specific MTR crash-prefix matrices for split, merge, root replacement, endpoint update, and free-list reuse.
12. Exact pre-append restoration and post-append noncontinuable outcomes for those structural operations.
13. Local L1 corruption matrices for overlapping entries, noncanonical keys, wrong owner, levels, children, and sibling endpoints.
14. Full L3 verifier fixtures for cycles, duplicate parentage, global order, orphan pages, and free/live overlap.
15. Deterministic free-page detach/reuse tests proving stale PageNos and guards cannot observe replacement state.
16. Reopen/recovery tests proving atomic reconstruction of complete structural MTRs and root metadata.

This is not an architecture defect. It belongs in `VERIFICATION.md`.

---

## 21. Direct review answers

| Question | Answer |
|---|---|
| Any byte-layout mismatch? | No |
| Any root/height ambiguity? | No |
| Any separator/routing ambiguity? | No |
| Any duplicate-order ambiguity? | No |
| Any key-encoding ambiguity? | No |
| Any RID/heap-recheck contradiction? | No |
| Any split-rule correctness ambiguity? | No; exact byte balance is permitted implementation freedom |
| Any root-publication durability ambiguity? | No |
| Any merge/reclamation ambiguity? | No |
| Any concurrency/latching ambiguity? | No frozen ambiguity |
| Any multi-page WAL/MTR ambiguity? | No |
| Any validation/owner gap? | No |
| Any uniqueness semantic ambiguity? | No |
| Any correctness-relevant implementer invention required? | No |
| Any project-time/current-state wording? | Yes: five localized findings |
| Any DEVELOPMENT-owned material? | Yes: §8.17.2 |
| Any VERIFICATION-owned procedure in Chapter 8? | No |
| Any PROJECT_STATE-owned material? | No |
| Any devlog/history material? | No |
| Any ambiguous terminology? | No correctness-relevant ambiguity |
| Any analytically underexplained boundary? | No |
| Can Chapter 8 stand as timeless canonical v1 contract? | After targeted wording cleanup, yes |

## 22. Recommended next actions

1. **Targeted documentation edit** for M1–M5 and optional E1.
2. Synchronize the separate Chapter-8 verification methodology gap.
3. Do not begin Chapter 9 review until those follow-ups are closed unless explicitly authorized.

Recommended Chapter 9 review boundary: actual Chapter 9, “Transaction Lifecycle and Snapshots,” §§9.1–9.16. Focus on transaction identity/reservation, lifecycle states, snapshots, command IDs, status storage, terminal publication, and read-only behavior. Chapter 8 should enter that review only as a physical candidate source; transactional uniqueness remains Chapter 11-owned.

## 23. Final read-only guarantee

| Item | Final result |
|---|---|
| Working tree | Clean |
| Index | Clean |
| HEAD | `447fc43f40c9901c62e9b2629ba4e7c9a1206de0` |
| `git diff --check` | Passed, no output |
| Files modified by audit | None |
| Repository-state change | None |
| Audit artifact/devlog | None |
| Build/tests/benchmarks | Not run |
| Implementation work | None |
| Phase 2 | **NOT STARTED / NOT AUTHORIZED** |

No audit-created change occurred, and no pre-existing material was modified, reverted, or staged.