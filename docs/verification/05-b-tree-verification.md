# B+ Tree Verification

This section is the detailed procedural owner for the B+ tree contract in
[`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 8. It specializes, without duplicating, the
generic page-format, BufferPool, PAGE_INIT, WAL/MTR, recovery, uniqueness, and reclamation
procedures in this guide. Principal architecture owners are §§4.3.2, 4.6, 4.10–4.14,
7.5–7.12, 8.1–8.29, 11.8–11.10, 12.10.2–12.10.3, 12.12, 12.17,
13.13.3–13.14, 14.5–14.15, 15.2–15.4, 16.5.4–16.6, 17.4 and 17.7,
39.1, and 41.2.

Every synthetic fixture MUST be canonical except for the single tested dimension. Valid
fixtures have an exact FileSuperblock, FileId, IndexId, TableId, PageNo, PageType, level,
published bound, schema fingerprint/version, key and RID encodings, required-zero bytes, and
checksum. A routing or race case must not accidentally become a checksum or owner-validation
case.

## Deterministic harness, observability, and reference models

Concurrency and failure cases use explicit hooks, barriers, controlled threads, injected
outcomes, and separate-process termination where crash semantics require it. Sleeps,
wall-clock ordering, and repeated racing until a condition appears are not valid primary
oracles. Random stress remains complementary state-space exploration.

The harness can pause and inspect abstract semantic events without requiring source-level
function names:

```text
root metadata snapshot and root_generation capture
root/parent/child/sibling latch attempt and acquisition
safe/unsafe child decision and ancestor release
split/merge decision, page reservation, and private initialization
private split/rebalance distribution and separator preparation
BTREE_MTR reservation, authorizing append, and runtime publication
root/endpoint/free-list metadata publication
page detachment, BTREE_FREE publication, guard drain, and reuse eligibility
cursor next-leaf handoff and guard release
fault/crash boundary and recovery redo begin/complete
L1/L2 validation, ordinary residency publication, and L3 verifier begin/complete
```

Observable state includes complete page bytes, PageId and owner, page type/level, slot and
entry geometry, page LSN, root PageNo/height/generation, endpoint and free-list metadata,
parent/child/sibling reachability, guard/pin/latch ownership, no-flush and dirty generations,
WAL valid/durable ends, file published bound, ordinary residency, and verifier visited sets.

Use two independent oracles:

1. A byte oracle writes and reads expected little-/big-endian fields directly from specified
   offsets; production encode/decode cannot serve as both operation and oracle.
2. A logical ordered model compares semantic values and canonical numeric RIDs independently
   of `IndexKeyCodec` and B+ search code. It owns expected insert, exact erase, point, equality,
   range, and total-order results.

## Persisted byte and geometry verification

Construct the complete specialized BTREE superblock independently and compare every byte.
Exercise the canonical page and one-defect-at-a-time variants below; the ordinary open/load
path must classify the defect before typed use.

| Superblock case | Deterministic fixture/oracle | Expected result |
|---|---|---|
| Common prefix and extension | Check common bytes `0..71`; little-endian `table_id` at 72, `root_page_no` at 80, `first_leaf_page_no` at 88, `last_leaf_page_no` at 96, `free_page_head` at 104, fingerprint at 112, schema version at 120, height at 124, and flags at 126 | Exact 8192-byte match; `header_size=128` |
| Identity and kind | Mutate FileKind, common `object_id`/IndexId, or registered TableId independently | Registered-owner/open rejection with the architecture-owned corruption result |
| Root/endpoints/free head | Use invalid, out-of-bound, root/free overlap, and inconsistent empty-tree values one at a time | Local or L3 rejection at the owning layer; no ordinary tree use |
| Schema | Mutate fingerprint, zero version, and recognizable greater version separately | V1 mismatch/zero is corruption; future version is unsupported format |
| Height | Test zero, one-leaf height 1, maximum legal height, and root-level mismatch | Only architecture-legal state accepted; no wrap |
| Flags and reserved | Set `index_flags`, each common required-zero field, then table-drive every byte in `128..8191` independently | Recognized malformed v1 is rejected |

For BTREE_LEAF and BTREE_INTERNAL, independently check the 32-byte common header, 32-byte
node header, 8-byte slot descriptors, and packed-entry regions. Include blank nodes
(`slot_count=0`, `lower=64`, `upper=8192`), exact boundary values, and the structural
slot-only bound 1016 as an arithmetic/rejection boundary rather than an impossible populated
valid node. Exercise `lower==upper` in both a legal full image and an otherwise impossible
image. Checked arithmetic establishes `lower = 64 + slot_count * 8` and
`64 <= lower <= upper <= 8192`.

| Structure | Exact byte/geometry cases | One-defect cases and oracle |
|---|---|---|
| Leaf header | level 0; count/lower/upper; flags 0; prev/next PageNos; bytes 60..63 zero | Nonzero level/flags/reserved, invalid endpoint, or inconsistent geometry is rejected |
| Internal header | level >0; count/lower/upper; flags 0; leftmost child; bytes 52..63 zero | Level 0, invalid child, nonzero flags/reserved, or inconsistent geometry is rejected |
| Slot | Four little-endian uint16 fields at offsets 0, 2, 4, 6 | Reject nonzero flags, zero or >1024 key length, wrap, crossing page/directory, pairwise overlap, or forbidden aliased payload |
| Leaf entry | `entry_length = user_key_length + 16`; exact canonical RID | Reject inconsistent length, malformed key, RID sentinel/reserved/owner defect, overlap, or unsorted/duplicate physical key |
| Internal entry | `entry_length = user_key_length + 24`; separator RID then right-child uint64 | Reject inconsistent length, malformed separator, invalid child, overlap, or unsorted/duplicate separator |
| BTREE_FREE | Common header, `next_free_page_no` at 32, bytes `40..8191` zero | Reject wrong type/owner/PageNo, out-of-range link, locally detectable self-link, or any nonzero reserved body |

The required-zero matrix is explicit rather than represented by one generic case:

| Structure | Required-zero region | Fault coverage |
|---|---|---|
| Common FileSuperblock/page header | Every §4.8/§4.10 unassigned flag/reserved field | Set each field independently |
| BTREE superblock | `index_flags` and bytes `128..8191` | One flag case plus a table-driven walk covering every reserved byte |
| Leaf | node flags, bytes `60..63`, every slot flag, RID bytes `14..15` | One-field/byte-at-a-time mutation |
| Internal | node flags, bytes `52..63`, every slot flag, separator-RID bytes `14..15` | One-field/byte-at-a-time mutation |
| BTREE_FREE | common flags/reserved16 and bytes `40..8191` | One-field case plus complete reserved-body walk |

These cases catch ABI serialization, unchecked offset arithmetic, and parsers that trust a
checksum without proving safe ownership of each byte range.

## IndexKeyCodec and physical-order properties

For each supported key type, generate semantic values independently, encode only through the
system under test, sort one copy with a test-side semantic comparator, sort encoded bytes
lexicographically, and require identical equivalence classes and order. Decode/round-trip is
an additional assertion, not the ordering oracle. Chapter 17 owns scalar semantics; §8.5
owns index encoding.

| Key family | Deterministic/property fixtures | Independent oracle |
|---|---|---|
| NULL/presence | NULL, minimum non-NULL, zero-like, positive and negative values; NULL in every composite position | NULLS FIRST and exact `00`/`01` presence partition |
| BOOLEAN | NULL, false, true, including composite positions | Semantic `NULL < false < true` |
| INT32/DATE | signed minima, adjacent minima/maxima, -1, 0, 1, and DATE domain endpoints | Signed semantic order equals sign-flipped big-endian order |
| INT64/TIMESTAMP | analogous 64-bit and valid timestamp endpoints | Signed semantic order equals sign-flipped big-endian order |
| FLOAT64 | infinities, finite signs, subnormal/boundary values, both zeros, and multiple NaN payload/sign forms | Chapter 17 canonical total order; zeros and all NaNs collapse to their required classes without host NaN comparison |
| VARCHAR | empty, ASCII, embedded/repeated zero, prefixes, high bytes, and longest fitting component | Binary collation; `00 FF` escaping and `00 00` terminator preserve prefix order unambiguously |
| Composite | differences in first/middle/final components, NULL positions, and VARCHAR prefixes | Component-wise semantic lexicographic order equals concatenated encoded order |

For equal encoded user keys, vary canonical RIDs across FileId, PageNo, and SlotId boundaries.
The model compares the numeric tuple `(FileId,PageNo,SlotId)` and requires the same physical
order; RID reserved bytes stay zero. Distinct RIDs remain legal even for one SQL key, while
an exact duplicate physical key is rejected except for the narrowly proven same-operation
replay path.

Exercise encoded user-key lengths 1, 1023, 1024, and 1025. Legal sizes proceed when node
geometry permits. Size 1025 fails with the exact operation-level oversized-key result before
any structural publication, never truncates, and is not classified as persisted corruption.
Separately, representable persisted length fields that violate formulas or page bounds
produce `CORRUPT_INDEX`.

## Routing, search, and duplicate ranges

Build internal pages directly as `C0,(K1->C1),...,(Kn->Cn)` and use a test-side routing
oracle implementing Chapter 8's lower-bound relation. Probe below K1, exact equality at every
separator, every between-separator interval, and above Kn. Exact equality MUST choose the
right child; repeat with equal user bytes and lower/equal/between/higher separator RIDs to
catch user-key-only internal comparisons.

Construct a legal stale-low separator by deleting the former minimum from a right child
without a structural boundary movement. Existing keys still route to that child, keys in the
stale gap may descend and fail locally, and L3 accepts the tree. Then make the separator
greater than a reachable right-child key; page-local checks may pass, but L3 must reject the
misrouting-high tree. Redistribution and child replacement cases verify exact boundary
refresh where Chapter 8 requires it.

Leaf lower-bound fixtures probe before first, exact first, between entries, equal SQL keys at
different RIDs, exact last, and beyond last. Equality starts at `(K,MIN_RID)`, traverses
next-leaf links, returns every matching RID once in physical order, and stops at the first
different user key. A duplicate run large enough to span several leaves is mandatory; it
catches implementations that treat a leaf boundary as an equality boundary.

## Split, redistribution, merge, and root publication

Create fragmented and compact leaf images with mixed key lengths at the insert-fit boundary:

1. fits without compaction;
2. fits only after compaction;
3. still does not fit and must split;
4. one very large legal key;
5. mixed small/large keys for which count balance differs from byte balance.

After a leaf split, compare the union against the pre-state plus insertion: every physical
entry appears exactly once, both pages fit and are nonempty, global order holds, the parent
separator is the first complete physical key in the right leaf, and sibling/end metadata is
reciprocal and correct. Force a split mostly or entirely inside one duplicate SQL-key run and
require RID order and cross-leaf equality completeness.

For an internal split, construct variable-length separator entries that force propagation.
The oracle reconstructs the original logical child/separator sequence and verifies the
selected promoted key is absent from both child pages, the right leftmost child is the
promoted key's child, all other children appear exactly once, levels are unchanged, parent
routing is correct, and every resulting page fits. Cascading and repeated splits apply the
same oracle at every level.

Exact 50/50 partition, slot midpoint, byte midpoint, physical memmove strategy, and internal
reconstruction algorithm are not test requirements. Every architecture-legal partition is
accepted if it preserves fit, nonempty results, canonical order, complete ownership,
routing, sibling/endpoints, level, and root invariants.

Run duplicate-heavy redistribution in both directions and right-into-left merge. Verify the
complete physical sequence before and after, exact parent lower-bound update when keys cross
the boundary, unchanged equality results, parent child removal, link/endpoint repair, and
detachment before BTREE_FREE publication. Sparse legal pages are accepted; approximately
25% occupancy is a soft policy, not a corruption threshold. If internal rebalance is chosen,
validate only the canonical resulting child/separator sequence and publication invariants.

For root split, contraction, and the one-empty-leaf case, pause before and after structural
publication and inspect root PageNo, tree height, root level, endpoints, page reachability,
`root_generation`, and the authorizing BTREE_MTR. Readers see the complete old tree before
publication and the complete new tree afterward. A root pointer to a private/incomplete page
is forbidden.

Race a reader that captured `(root_page_no,root_generation)` with split and contraction. The
reader latches the candidate, detects any changed identity/generation, releases, and restarts;
it never treats a former or reused root as current. Contraction decrements height exactly
once, retires the old root through the normal gate, and preserves one empty leaf at height 1.

## Latching, write crabbing, and cursor lifetime

All ordering tests use acquisition-attempt and acquisition-complete hooks. The test records a
wait-for graph and fails immediately on an architecture-forbidden edge; a watchdog is only a
secondary hang detector.

| Scenario | Required order | Deterministic race and oracle |
|---|---|---|
| Root acquisition | metadata snapshot/pin; release metadata; wait for root page; reacquire metadata to validate | Worker A holds root page and later needs metadata; worker B must not retain metadata while waiting and must restart on generation change |
| Root publication | required structural page latches before metadata latch | Reverse-order attempt cannot become a blocking edge; publication is complete before release |
| Vertical traversal | parent before child, with coupling where required | Competing writer makes early parent release or child-before-parent visible; only canonical order survives |
| Adjacent pages | left before right | A right-held operation discovering a needed left page releases/restarts rather than waiting right-to-left |
| Free-list/endpoint | snapshot metadata, release, latch page, reacquire metadata, revalidate | A competing head/endpoint change forces restart; stale candidate is never published |

For insert, construct a child that cannot propagate a split and one that can. The safe case
releases no-longer-needed ancestors at the canonical point; the unsafe case retains the
required path and can propagate without reverse-order reacquisition. Repeat for delete using
whether the chosen operation can require redistribution/merge propagation; do not harden the
soft occupancy policy. If optimistic release allows intervening mutation, force it at a
barrier and require revalidation/restart before structural use.

No transaction-level lock wait may retain a B+ page latch. Logical uniqueness wait cases are
owned by the transaction tests below, with a B+ observer asserting all page guards/latches
are released before the wait begins.

A forward cursor fixture records the current read guard, slot, upper bound, and next-leaf
handoff. At the leaf boundary it reads next while guarding the current leaf, acquires and
validates the next guard, then releases the old guard. Borrowed key/entry views are poisoned
or token-checked after release and cannot survive it.

Race handoff separately with split, merge, detach, and reuse. The physical traversal remains
ordered, misses no qualifying physical key solely because of handoff, and either reaches the
validated surviving chain or performs an architecture-permitted restart. A guarded current
or target page cannot expose replacement state; an unguarded numeric PageNo has no long-lived
identity guarantee. Transaction snapshot filtering is not inferred here: Chapter 10 owns
visibility of returned candidate RIDs.

## BTREE_MTR failure, crash, append, and recovery

Layer tree-specific checks on the generic Non-Crash WAL/MTR Failure Injection, PAGE_INIT and
MTR Rollback, and Crash Injection Framework below. For each structural operation, capture a
complete valid old tree and complete expected new-tree invariant set, then pause at page
reservation, private initialization, private mutation, MTR reservation, authorizing append,
runtime publication, allowed flush, and crash. Reopen through ordinary recovery and run the
full-tree verifier.

The recovered survivor is exactly the old tree or the complete new tree. It is never a
mixture. Exercise:

- leaf split: redistribution, reciprocal links, parent separator, and endpoint;
- internal/cascading split: child distribution, promotion, parent propagation;
- root split and contraction: root PageNo, height, root contents, and persistent metadata;
- redistribution/leaf and internal merge: movement, separator removal/update, links,
  endpoint, detachment, and free transition;
- free-list pop/push/reuse: head selection/revalidation, private reinitialization, and live
  publication;
- appended BTREE page: PAGE_INIT, file bound, private frame, MTR reference, and reachability.

Before an authorizing append, inject reservation, allocation/BufferPool, encoding, validation,
and known WAL append failures. Exact restoration compares page bytes, dirty/no-flush and
generation metadata, root/height/generation, endpoints, free head, frame/PageId state, and
new/reused page disposition. An unpublished appended tail follows §4.11.1.1 exactly.

After an authorizing append, inject uncertain append outcome, WAL write/sync failure, and
runtime publication failure separately. No case rolls back and continues. The operation
retains protected/nonordinary ownership for completion or enters the §12.12
`DATABASE_NONCONTINUABLE` lifecycle; no subsequent statement observes ambiguous structure.

For appended pages, ordinary traversal cannot reach or fetch the private page before the
same publication boundary establishes canonical page bytes, PAGE_INIT/BTREE_MTR authority,
file published bound, BufferPool residency, and structural reachability. After publication,
all dimensions agree. Recovery/private I/O cannot expose an unvalidated intermediate page to
ordinary callers.

Reopen matrices include clean tree, leaf split, internal/root split, merge/contraction,
endpoint change, free-page reuse, and interrupted MTR. Assert recovered root/height/endpoints,
exact-once contents, separators, levels, sibling/free graphs, page LSN coherence, and full
verifier acceptance. Run recovery again on the recovered files: no entry, separator,
free-list transition, or root metadata change repeats, and process-local `root_generation`
is initialized from recovered runtime state rather than treated as persistent history.

## L1, registered-owner, and unsupported-format validation

Ordinary load specializes the Chapters 4 and 7 order:

```text
exact full read
family/version dispatch
checksum
common PageId identity
registered file/index owner and published bound
PageType
B+ L1 structure
ordinary RESIDENT and typed use
```

`page_lsn` is not trusted before checksum. L1 validates one page without fetching referenced
pages; registered-owner validation supplies FileId/IndexId/TableId/schema/heap context; L3
owns global topology. A failed stage publishes no ordinary mapping/guard and later stages do
not run.

| Page family | One-defect-at-a-time L1 matrix | Expected result |
|---|---|---|
| Leaf | Common framing/checksum/PageId/type; flags/reserved; level; count/lower/upper; directory/payload and pairwise overlap; key/length/RID; owner heap FileId; strict physical order/duplicate; sibling domain | Recognized malformed v1 yields `CORRUPT_INDEX` before ordinary use |
| Internal | Common fields; level 0; missing/invalid leftmost child; count/child relation; separator codec/order/duplicate; right-child domain/self-reference where locally forbidden; overlap/length/reserved/bound | `CORRUPT_INDEX`; global subtree range remains L3 |
| BTREE_FREE | Type, owner/PageNo, link domain/local self-link, flags/reserved body | `CORRUPT_INDEX`; cycles and live overlap remain L3 |
| Superblock/owner | FileKind/FileId/IndexId/TableId/root/endpoints/free head/fingerprint/schema version/height/flags/reserved | Owning corruption or unsupported result; no tree publication |

Cross valid index A with registered index/descriptor B for FileId, IndexId, TableId, schema
fingerprint/version, expected heap FileId, and valid pages from the wrong file. Wrong ownership
is never accepted merely because bytes parse. Construct recognizable future file, page, and
key-schema versions separately: dispatch returns the exact unsupported-format result and does
not decode them as malformed v1. Zero/invalid v1 values remain corruption.

## L3 full-tree topology verification

Start from one deterministic valid multi-level tree containing duplicate runs across leaves,
a legal stale-low separator, valid endpoints, and a nonempty disjoint free list. L3 MUST
accept it. Mutate one global dimension at a time while keeping every page locally valid:

| L3 defect | Deterministic fixture | Full-verifier oracle |
|---|---|---|
| Child cycle | Back-edge to an ancestor | Bounded visited-set rejection; no loop |
| Duplicate parentage | One live child named by two parents | Reject nonunique live ownership |
| Subtree range | Locally sorted child contains key outside parent lower/upper bounds | Reject misrouting topology; legal stale-low remains accepted |
| Leaf global order | Locally sorted adjacent leaves reversed across boundary, including RID tie-break | Reject global physical-order violation |
| Leaf chain | Prev/next disagreement, cycle, disconnected reachable leaf | Reject with bounded traversal |
| Level/depth | Wrong child level, unequal leaf depth, root level/height mismatch | Reject |
| Endpoint/root | First/last not actual endpoints, invalid empty-tree tuple, wrong root type/level | Reject |
| Orphan | Published owned live node unreachable from root and absent from free list | Report unauthorized orphan under §§4.13.5/8.28 |
| Free/live overlap | One page reachable from both root and free head | Reject |
| Free-list topology | Cycle, duplicate membership/predecessor inconsistency, out-of-range link | Bounded rejection |
| Classification partition | Every published ordinary page assigned exactly once to live or free graph | Reject unclassified/duplicate ownership |

This distinction proves why locally safe parsing is not sufficient evidence of a correct
tree while avoiding an O(tree-size) validation requirement on every ordinary fetch.

## Detach, free-page reuse, and stale-reference safety

For page R, pause canonical merge/retirement after each ordering point: remove parent/root
routing, remove sibling/endpoint reachability, prevent new traversal, drain existing guards,
publish BTREE_FREE, then make R eligible for free-list allocation. A reachable page cannot be
rewritten as free, and a free page cannot remain live.

Hold an old PageGuard across logical detachment. New traversal may cease reaching R, but
physical reuse cannot expose a replacement image through that guard. After all lifetime gates
drain, reuse privately rewrites the complete page into one canonical leaf/internal image,
removes old free metadata/reserved-body residue, preserves the architecture-owned PageId, and
publishes free-list removal plus live reachability in one structural MTR.

Capture only R's numeric PageNo without a guard, detach/reuse it, and verify public access
must reacquire through current FileId/PageId, owner, validation, and guard rules. The raw
number has no generation guarantee and cannot implement a long-lived cursor. Inject an old
BufferPool asynchronous completion against the reused frame and apply Chapter 7's
identity/generation oracle: replacement state is unchanged.

## Cross-owner semantics, exhaustion, and failure classification

The physical tree permits `(K,RID1)` and `(K,RID2)`, can enumerate the entire user-key range,
and always returns physical candidates for heap/MVCC recheck. Construct invisible, aborted,
and dead-but-not-reclaimed target versions: the index remains structurally valid, lookup
returns the candidate, and Chapters 10–11 decide visibility/uniqueness. The UNIQUE tests below
own logical key locks and current-owner truth tables. Chapter 14 owns stale-entry removal and
RID grace; removing an index entry alone never authorizes heap RID reuse.

All ordinary index pages use Chapter 7 BufferPool PageId, guard, validation, writeback, and
retirement methodology. Add B+ observers proving traversal/split code retains no page pointer
after guard release and no specialized residency path creates a second cache/lifetime owner.

Specialize terminal-boundary tests with a maximum legal tree height and a growth attempt.
The maximum tree remains valid; growth fails before provisional mutation/page publication,
does not wrap height/level, and leaves the exact old tree. Page-local no-space first selects
compaction/split and is not disk exhaustion. Inject `NO_REPLACEABLE_FRAME`, disk
`RESOURCE_FULL`, PageNo exhaustion, height exhaustion, oversized operation key, malformed
persisted length, and WAL failures independently so each reaches only its owner.

| Condition | Verification oracle |
|---|---|
| Malformed recognized-v1 index bytes/topology | `CORRUPT_INDEX`; no ordinary malformed use |
| Recognizable future file/page/key-schema version | Exact unsupported-format result |
| Attempted encoded key >1024 | Operation-level oversized-key failure before publication |
| Current node does not fit | Compact if useful, then structural split; not `RESOURCE_FULL` |
| No BufferPool victim | `NO_REPLACEABLE_FRAME`; known pre-authorization tree unchanged |
| Disk capacity | `RESOURCE_FULL`; not buffer or numeric exhaustion |
| PageNo/height terminal growth | Owning deterministic exhaustion result; no wrap/partial root |
| WAL reservation/known append failure | Exact old tree and local continuation only where §12.12 permits |
| Authorizing append uncertainty | `DATABASE_NONCONTINUABLE`; no rollback-and-continue |
| Transactional uniqueness conflict | Chapter 11 logical/transaction result, not physical duplicate error |
| Structurally valid stale index entry | Not corruption by itself; heap/MVCC recheck and Chapter 14 cleanup |

## Chapter 8 procedural matrices

The following compact matrices index the detailed procedures above. Each mapping requires a
deterministic fixture, controlled boundary, independent oracle, and expected outcome; architecture prose or random stress alone is insufficient.

### Structural crash-outcome matrix

| Structural operation | Pre-authorization fault | Authorizing state / publication | Post-authorization uncertainty | Recovered oracle |
|---|---|---|---|---|
| Leaf split | Exact old leaf/tree restored | One BTREE_MTR covers pages, links, parent, endpoint | Complete/retry or noncontinuable | Exact old or complete new tree |
| Internal/cascading split | Old child/parent sequence restored | One MTR covers distribution/promotion/propagation | No local rollback | No orphan/duplicate child |
| Root split | Old root/height retained | New root and metadata co-publish | No ambiguous root admitted | Coherent root/height/tree |
| Redistribution | Old siblings/separator restored | Movement and boundary publish together | No mixed boundary | Old or canonical new sequence |
| Leaf/internal merge | Old reachability restored | Movement, parent removal, links, detach publish together | Free transition cannot escape alone | No required detached child/live free page |
| Root contraction | Old root/height retained | Child promotion, metadata, retirement co-publish | No ambiguous root admitted | Coherent root/height |
| Endpoint update | Old endpoint retained | Endpoint included with structural change | No mixed chain/endpoint | Endpoint equals actual chain end |
| Free-list pop | Old head/page remains free | Head removal and private conversion authorize together | Page protected/nonordinary | Page wholly free or wholly live |
| Free-list push/reuse | Old live/free state restored | Detach/free or free/live transition is one MTR | No both/neither state | Disjoint complete graph |
| Appended index page | Tail/frame/bound restored | PAGE_INIT, bound, residency, route authorize together | Protected completion/noncontinuable | Unpublished old or complete new page |

### Latch-order matrix

| Scenario | First action | Second action | Forbidden reverse/wait | Restart oracle |
|---|---|---|---|---|
| Vertical traversal | Parent latch | Child pin/latch | Child then parent | Reacquire from root/path |
| Adjacent pages | Left latch | Right latch | Wait right-to-left | Release and restart |
| Root acquisition | Snapshot/pin under metadata, then release | Root page latch, then metadata validation | Waiting on page while retaining metadata | Generation/identity mismatch restarts |
| Root publication | Structural page latches | Metadata latch | Metadata held while acquiring pages | Publication retries under canonical order |
| Free-list/endpoint | Metadata snapshot, then release | Page latch, then metadata revalidation | Metadata retained during page wait | Changed head/endpoint restarts |

### Cursor-concurrency matrix

| Case | Barrier | Protected resource | Legal survivor | Forbidden survivor |
|---|---|---|---|---|
| Ordinary handoff | After reading next, before next guard | Current guard and target identity | Valid next guard before current release | Unguarded borrowed view |
| Handoff vs split | Split before/after next acquisition | Current/next guarded chain | Ordered continuation or permitted restart | Miss due solely to stale link |
| Handoff vs merge | Detach before/after target guard | Existing guards and reciprocal chain | Surviving chain or restart | Use-after-detach/reuse |
| Handoff vs detach | After numeric PageNo capture | Guard/lifetime gate | Fresh validated acquisition or restart | Raw PageNo treated as durable handle |
| Handoff vs reuse | Reuse eligibility attempt | Pin/guard and BufferPool generation | Reuse waits or old access ends | Replacement bytes through old guard |
| Guard release | Before/after release | Borrowed entry/key view | View invalid after release | View survives reassignment |

### L1 corruption matrix

| Page family | Defect family | Local/owner oracle | Classification | Before ordinary use? |
|---|---|---|---|---:|
| Superblock | Framing, kind/identity, roots/endpoints/free head, schema, height, flags/reserved | Exact byte and registered-owner check | Corruption or exact unsupported format | yes |
| Leaf | Header/level/geometry/overlap/key/RID/order/duplicate/sibling | Nonfetching L1 plus owner context | `CORRUPT_INDEX` | yes |
| Internal | Header/level/geometry/overlap/separator/child/order | Nonfetching L1 plus bound context | `CORRUPT_INDEX` | yes |
| BTREE_FREE | Header/type/owner/link/reserved body | Nonfetching free-page check | `CORRUPT_INDEX` | yes |
| Cross-owner | FileId/IndexId/TableId/schema/heap mismatch | Registered descriptor validation | Owning corruption | yes |
| Future format | Recognizable file/page/key-schema version | Dispatch before v1 parse | Unsupported-format result | yes |

### L3 topology matrix

| Defect | Deterministic fixture | Full-verifier oracle | Bounded? |
|---|---|---|---:|
| Child cycle | Ancestor back-edge | Reject repeated PageNo | yes |
| Duplicate parentage | Child named twice | Reject nonunique live ownership | yes |
| Subtree range | Locally valid out-of-range child | Reject routing violation | yes |
| Unequal leaf depth | One path shorter/longer | Reject level/depth mismatch | yes |
| Global leaf order | Locally sorted boundary inversion | Reject physical-key inversion | yes |
| Sibling chain | Reciprocity, cycle, or disconnect defect | Reject chain mismatch/repeat | yes |
| Endpoint mismatch | First/last disagrees with chain | Reject metadata mismatch | yes |
| Orphan live page | Published live page in neither graph | Report unauthorized orphan | yes |
| Free/live overlap | Same page in both graphs | Reject duplicate classification | yes |
| Free-list cycle/duplicate | Repeated free PageNo | Reject repeat | yes |
| Root/height mismatch | Root type/level or empty tuple wrong | Reject | yes |

### Recovery/reopen matrix

| Persistent starting state | WAL/recovery action | Root/topology oracle | Full verifier |
|---|---|---|---|
| Clean tree | Ordinary reopen | Same root/height/content | accepts |
| Durable leaf split | Redo/reopen | Exact-once keys, links, endpoints | accepts |
| Durable internal/root split | Atomic BTREE_MTR redo | Coherent root/height/levels | accepts |
| Durable merge/contraction | Atomic redo | Removed pages unreachable, survivor canonical | accepts |
| Durable free-page reuse | Atomic redo | Page appears exactly once, live or free | accepts |
| Durable endpoint change | Atomic redo | Metadata equals recovered chain | accepts |
| Interrupted structural MTR | WAL-prefix validation and redo | Exact old or complete new state | accepts survivor |
| Already recovered tree | Repeat recovery/reopen | No duplicate action or persistent-generation drift | accepts unchanged state |

### High-level domain/case matrix

| Family | Deterministic fixture | Barrier/fault | Independent oracle | Architecture owner |
|---|---|---|---|---|
| Superblock bytes | Exact 8192-byte vectors | One field/byte mutation | Direct offset decoder | §§8.2–8.3 |
| Leaf/internal/free bytes | Canonical page vectors | Geometry/reserved mutation | Checked range/byte oracle | §§8.7–8.10, 8.18 |
| Key codec/order | Boundary/property values | Type/component boundary | Semantic comparator | §§8.3–8.6; Ch. 17 |
| Physical order | Equal key, varied RIDs | RID boundaries | Numeric RID model | §§8.4, 8.6 |
| Routing/stale-low | Multi-separator tree | Equality/deletion boundary | Lower-bound relation | §§8.10–8.11 |
| Leaf/internal split | Variable-byte full nodes | Fit/compaction/promotion | Invariant-set split oracle | §§8.12–8.15 |
| Duplicate mutation | Multi-leaf duplicate run | Split/redistribute/merge | Ordered multiset | §§8.4, 8.13, 8.17 |
| Root publication | Root split/contraction | Generation/publication hooks | Old-or-new tree | §§8.15, 8.19.3, 8.26 |
| Latch/crabbing | Controlled competing workers | Acquisition/safe hooks | Wait-edge/order oracle | §8.19 |
| Cursor handoff | Multi-leaf scan | Split/merge/reuse barriers | Ordered physical range | §8.20 |
| BTREE_MTR crash | Every structural family | Reservation/append/publish/crash | Old-or-new tree | §§8.25–8.26; Chs. 12–13 |
| Append failure | New/reused page | Pre/post-authorizing append | Exact rollback/noncontinuable | §§4.11, 12.12 |
| L1/owner validation | Canonical page, one defect | Pre-residency stages | Independent parser/context | §§4.13, 8.27 |
| L3 topology | Valid complex tree, one defect | Verifier traversal | Graph/order model | §8.28 |
| Detach/reuse | Guarded retired page | Reachability/drain/reuse hooks | Lifetime and graph oracle | §§8.18–8.20; Ch. 7 |
| Recovery/reopen | Durable/interrupted MTR states | Process crash/restart | Logical model + L3 | §§13.13.3–13.14 |
| Exhaustion/failure | Isolated terminal/resource states | Owning fault point | Exact error and survivor | §§4.3.2, 8.6, 8.15, 39.1 |

---

## Randomized Tests

Compare the B+ tree to an in-memory sorted oracle of physical keys.

Random operation stream:

```text
insert
erase
point lookup
range lookup
close/reopen
```

Periodically:

```text
run full verifier
compare complete sorted contents against oracle
```

Random seeds must be reproducible and printed on failure.

Single-threaded randomized structural verification is a prerequisite for concurrency
stress testing. Randomized tests explore long operation sequences and broad state space;
they do not replace any deterministic byte, boundary, latch, cursor, fault, or crash case
above.

---

## Concurrent Tests

First run deterministic barrier cases for:

```text
search versus split
split versus split on one path
root metadata versus root-page latch acquisition
safe and unsafe insert/delete crabbing
left/right sibling acquisition and restart
cursor handoff versus split, merge, detach, and reuse
free-list/endpoint snapshot revalidation
guard drain versus page reuse
```

Each case asserts legal and forbidden outcomes directly; no sleep or repeated race is an
ordering mechanism. Then use reproducible stress as complementary coverage:

- many readers + one writer,
- writers on disjoint ranges,
- writers on one hot range,
- duplicate-heavy inserts,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes.

Use deliberately tiny buffer pools in some tests.

Add watchdogs/timeouts as secondary deadlock detectors, while acquisition traces and barriers
remain the primary latch-order oracle.

Compose the existing §8.28 `L3 full-tree topology verification` procedure with §41.2's
frequent-verifier SHOULD at safe post-quiescence checkpoints. For each barrier-controlled
case above, first prove from the scheduled event trace that the intended overlap and any
named split, merge, root change, detach, or reuse actually occurred. Preserve the case's
direct latch-order, restart, cursor/result, exact physical-key/RID, and progress oracles;
L3 cannot substitute for them. Then release the barriers, let every participating worker
complete or reach its owned terminal/restart result, finish required transaction or
deadlock-victim resolution, and confirm through the test scheduler that no worker remains
in a tree mutation step or holds a B+ write latch. Release worker page guards/pins required
by the case before verification, including in tiny-BufferPool cases. Only after that
quiescent barrier, invoke a **new** L3 traversal of the final stable tree and require PASS.
An absent intended schedule/event, skipped L3 run, stale earlier L3 result, or any L3
invariant failure makes that case non-PASS; retain the first owning failure evidence.

Apply the same post-quiescence composition to the existing reader/writer, disjoint/hot-range
writer, duplicate-heavy, simultaneous-split, split/merge-churn, forward-scan/write, and
tiny-BufferPool families. Range-scan visibility/results and BufferPool eviction/pin/latch
checks remain their separate direct oracles. For any B+-participating deadlock/restart case,
resolve the cycle, victim outcome, surviving operation, required retry/abort boundary, and
latch/lock cleanup before the stable L3 run; never traverse an unresolved cycle. Reproducible
stress remains supplementary: use the same L3 procedure at safe quiescent checkpoints when
the stress harness provides them and at final quiescence, not while mutators race. These
are verification checkpoints, not an L3 call on each ordinary operation, an online global
verifier, a production pause API, or a new numerical cadence requirement.

---

## Chapter 8 architecture-obligation coverage map

The atomic inventory assigns each obligation one primary domain and one primary procedural
owner. Cross-owner references are dependencies, not duplicate ownership.

| Domain | Primary domain | Atomic obligations |
|---|---|---:|
| A | FILESUPERBLOCK | 8 |
| B | PAGE LAYOUT | 6 |
| C | SLOT / ENTRY GEOMETRY | 7 |
| D | KEY CODEC | 11 |
| E | PHYSICAL ORDER | 4 |
| F | ROUTING | 7 |
| G | DUPLICATE HANDLING | 5 |
| H | SEARCH / RANGE SCAN | 5 |
| I | LEAF SPLIT | 7 |
| J | INTERNAL SPLIT | 6 |
| K | ROOT PUBLICATION | 7 |
| L | DELETE / REDISTRIBUTION / MERGE | 7 |
| M | LATCHING / CRABBING | 11 |
| N | CURSOR LIFETIME | 6 |
| O | WAL / BTREE_MTR | 8 |
| P | APPEND / PAGE PUBLICATION | 5 |
| Q | VALIDATION L1 | 9 |
| R | VALIDATION L3 | 11 |
| S | OWNER VALIDATION | 6 |
| T | RECLAMATION / FREE LIST | 7 |
| U | RECOVERY / REOPEN | 7 |
| V | EXHAUSTION / FAILURE | 7 |
| W | UNIQUENESS / MVCC CROSS-OWNER | 6 |
| X | OTHER | 4 |
|  | **Total** | **167** |

| # / domain | Atomic obligation | Architecture owner | Verification owner | Procedure type |
|---:|---|---|---|---|
| 1 (A1) | Common 72-byte FileSuperblock prefix is verified byte-for-byte | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 2 (A2) | BTREE extension fields are verified at exact offsets and widths | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 3 (A3) | All BTREE superblock multibyte fields are verified little-endian | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 4 (A4) | BTREE superblock header_size is exactly 128 | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 5 (A5) | Initialized root/first/last PageNos form a legal metadata tuple | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 6 (A6) | free_page_head sentinel and in-range domains are verified | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 7 (A7) | key_schema_version and fingerprint outcomes are distinguished | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 8 (A8) | index_flags and the complete trailing reserved region are zero | §§8.2–8.3.1 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 9 (B1) | Leaf common and node headers occupy the exact 64-byte prefix | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 10 (B2) | Internal common and node headers occupy the exact 64-byte prefix | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 11 (B3) | BTREE_FREE header_size and body boundaries are exact | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 12 (B4) | Leaf level and sibling fields use their canonical byte positions | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 13 (B5) | Internal level and leftmost-child fields use their canonical byte positions | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 14 (B6) | Every page-family required-zero region is tested independently | §§8.7, 8.9–8.10, 8.18 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 15 (C1) | Slot descriptors are exactly 8 bytes with four uint16 fields | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 16 (C2) | lower equals 64 plus eight times slot_count under checked arithmetic | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 17 (C3) | The 64 <= lower <= upper <= 8192 bounds are enforced | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 18 (C4) | Leaf entry_length equals user_key_length plus 16 | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 19 (C5) | Internal entry_length equals user_key_length plus 24 | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 20 (C6) | Entry payloads cannot overlap the slot directory or page end | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 21 (C7) | Distinct live entry payloads cannot overlap or alias | §§8.7–8.10 | Persisted byte and geometry verification | Deterministic fixture/oracle |
| 22 (D1) | NULL presence encoding establishes NULLS FIRST | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 23 (D2) | BOOLEAN encoded order matches NULL, false, true semantic order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 24 (D3) | INT32 sign transform and big-endian bytes preserve order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 25 (D4) | DATE encoding preserves its canonical semantic domain order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 26 (D5) | INT64 sign transform and big-endian bytes preserve order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 27 (D6) | TIMESTAMP encoding preserves its canonical semantic domain order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 28 (D7) | FLOAT64 infinities and finite values preserve canonical order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 29 (D8) | FLOAT64 negative and positive zero canonicalize identically | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 30 (D9) | All FLOAT64 NaN payloads canonicalize to one ordered class | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 31 (D10) | VARCHAR zero escaping and termination are unambiguous | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 32 (D11) | Composite concatenation preserves component-wise order | §§8.3–8.6; §§17.4, 17.7 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 33 (E1) | Physical comparison uses encoded user-key bytes before RID | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 34 (E2) | Equal user keys use numeric FileId/PageNo/SlotId RID order | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 35 (E3) | RID reserved bytes are canonical zero | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 36 (E4) | Exact duplicate physical keys are rejected outside proven replay | §§8.4, 8.6 | IndexKeyCodec and physical-order properties | Deterministic fixture/oracle |
| 37 (F1) | Internal targets below the first separator route to C0 | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 38 (F2) | Separator equality routes to the separator's right child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 39 (F3) | Between-separator targets route to the bounded child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 40 (F4) | Targets above the last separator route to the last child | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 41 (F5) | Routing compares the complete physical separator including RID | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 42 (F6) | Legal stale-low separators preserve reachability and are accepted | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 43 (F7) | Misrouting-high separators are rejected by full-tree validation | §§8.10–8.11 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 44 (G1) | Distinct RIDs for one SQL key remain physically representable | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 45 (G2) | Duplicate SQL-key runs remain in RID order | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 46 (G3) | Duplicate runs may cross leaf boundaries without omission | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 47 (G4) | Duplicate-heavy split retains complete physical order | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 48 (G5) | Duplicate redistribution and merge preserve exact-once contents | §§8.4, 8.11, 8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 49 (H1) | Leaf exact search is lower_bound-equivalent on physical keys | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 50 (H2) | Equality scan begins at the conceptual MIN_RID bound | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 51 (H3) | Equality scan terminates exactly when the user key changes | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 52 (H4) | Forward range results are ordered and bounded correctly | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 53 (H5) | Every physical candidate remains subject to heap visibility recheck | §§8.11, 8.20–8.22 | Routing, search, and duplicate ranges | Deterministic fixture/oracle |
| 54 (I1) | Insert fitting contiguous space avoids split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 55 (I2) | Fragmented sufficient space compacts before split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 56 (I3) | Post-compaction no-fit selects split | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 57 (I4) | Leaf split is byte-based for variable-size entries | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 58 (I5) | Both leaf survivors fit, are nonempty, and preserve exact contents | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 59 (I6) | Right-leaf first physical key becomes the parent separator | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 60 (I7) | Leaf sibling and endpoint metadata are updated coherently | §§8.12–8.13 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 61 (J1) | Internal split selection is verified by legal byte geometry | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 62 (J2) | The promoted separator is removed from the child level | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 63 (J3) | The promoted separator's child becomes the right leftmost child | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 64 (J4) | All children remain uniquely represented after split | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 65 (J5) | Resulting levels and routing bounds remain valid | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 66 (J6) | Cascading split propagation preserves the same invariants | §8.14 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 67 (K1) | Root split constructs a complete new internal root privately | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 68 (K2) | Root PageNo and tree height publish with the structural MTR | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 69 (K3) | Readers observe only complete old or complete new root state | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 70 (K4) | root_generation changes with runtime root identity/height | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 71 (K5) | A stale root snapshot detects mismatch and restarts | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 72 (K6) | Root contraction promotes the sole child and decrements height once | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 73 (K7) | The empty tree remains one empty leaf at height one | §§8.2.1, 8.15, 8.19.3, 8.26 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 74 (L1) | Ordinary deletion may retain a legal stale-low separator | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 75 (L2) | Sparse nodes below the soft threshold are not corruption | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 76 (L3) | Redistribution in either direction updates the crossing boundary | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 77 (L4) | Leaf merge moves right into left and repairs links/endpoints | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 78 (L5) | Parent separator and right-child removal are coherent | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 79 (L6) | Internal rebalance preserves canonical child/separator sequence | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 80 (L7) | Merged pages detach before entering BTREE_FREE state | §§8.10.1, 8.16–8.18, 8.23 | Split, redistribution, merge, and root publication | Deterministic fixture/oracle |
| 81 (M1) | Read traversal acquires parent before child | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 82 (M2) | Latch coupling retains parent until the child is pinned and latched | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 83 (M3) | Root metadata is released before waiting on a root page latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 84 (M4) | Root identity/generation is revalidated under the page latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 85 (M5) | Structural page latches precede root metadata publication latch | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 86 (M6) | Adjacent-page acquisition is left to right | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 87 (M7) | A required reverse-order sibling acquisition releases and restarts | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 88 (M8) | Insert-safe children permit release of unnecessary ancestors | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 89 (M9) | Insert-unsafe children retain ancestors needed for propagation | §8.19 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 90 (N1) | Forward handoff retains the current guard until next is validated | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 91 (N2) | Borrowed key/entry views expire with their leaf guard | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 92 (N3) | Cursor handoff versus split preserves physical scan progress | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 93 (N4) | Cursor handoff versus merge reaches a valid chain or restarts | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 94 (N5) | Cursor handoff versus detach/reuse cannot dereference replacement state | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 95 (N6) | Long-lived unguarded raw PageNo cursors are rejected | §§8.18.1, 8.20 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 96 (O1) | Every persistent B+ mutation participates in one owning BTREE_MTR | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 97 (O2) | One structural mutation covers every affected page and metadata item | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 98 (O3) | Affected pages receive the canonical common MTR LSN semantics | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 99 (O4) | No-flush ownership prevents provisional bytes from escaping | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 100 (O5) | Known pre-append failure restores the exact old structural state | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 101 (O6) | Authorizing append uncertainty never rolls back and continues | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 102 (O7) | Runtime publication exposes the complete MTR as one boundary | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 103 (O8) | WAL-before-data remains mandatory for every B+ page write | §§8.25–8.26; §§12.10.2–12.10.3, 12.12 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 104 (P1) | A newly appended BTREE page remains private before publication | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 105 (P2) | PAGE_INIT and structural MTR authority cover the new page | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 106 (P3) | The owning file bound and structural reachability agree | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 107 (P4) | Pre-authorizing append failure restores/truncates the unpublished tail | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 108 (P5) | Post-authorizing publication failure follows completion or noncontinuable rules | §§4.11, 7.7, 8.26 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 109 (Q1) | Leaf header, geometry, and required-zero fields receive L1 validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 110 (Q2) | Leaf entries receive checked length and nonoverlap validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 111 (Q3) | Leaf keys and RIDs receive canonical codec validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 112 (Q4) | Leaf strict physical order and exact-duplicate rejection are L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 113 (Q5) | Internal header, geometry, and required-zero fields receive L1 validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 114 (Q6) | Internal separators and children receive local codec/domain validation | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 115 (Q7) | Internal strict separator order is L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 116 (Q8) | BTREE_FREE exact header, link domain, and zero body receive L1 | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 117 (Q9) | Recognizable unsupported file/page/key-schema versions bypass v1 parsing | §§4.13.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 118 (R1) | A valid multi-level duplicate/stale-low/free-list fixture is accepted | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 119 (R2) | Child-reference cycles are rejected with bounded progress | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 120 (R3) | Duplicate live parentage is rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 121 (R4) | Subtree key-range violations are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 122 (R5) | All leaves at unequal depths are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 123 (R6) | Global leaf physical-order violations are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 124 (R7) | Sibling reciprocity, cycles, and disconnection are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 125 (R8) | First/last endpoint mismatches are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 126 (R9) | Unauthorized orphan live pages are reported | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 127 (R10) | Free/live overlap is rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 128 (R11) | Free-list cycles and duplicate membership are rejected | §§4.13.5, 4.13.9, 8.28 | L3 full-tree topology verification | Deterministic fixture/oracle |
| 129 (S1) | Registered FileKind and FileId match the loaded file/page | §§4.10.2, 4.13.1, 8.2–8.4, 8.27; §§16.5.4–16.6 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 130 (S2) | Common object_id matches the expected IndexId | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 131 (S3) | Persisted TableId matches the index descriptor | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 132 (S4) | Schema fingerprint/version match the resolved key descriptor | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 133 (S5) | Leaf RID heap FileId matches the indexed relation | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 134 (S6) | Valid BTREE bytes from another index file are rejected | §§4.10.2, 4.13.1, 8.2–8.4, 8.27 | L1, registered-owner, and unsupported-format validation | Deterministic fixture/oracle |
| 135 (T1) | A page is removed from parent/root reachability before free | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 136 (T2) | Sibling and endpoint reachability are removed before free | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 137 (T3) | New traversals cannot reach a detached page | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 138 (T4) | Existing guards/pins drain before replacement state is exposed | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 139 (T5) | BTREE_FREE publication and free-list membership are coherent | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 140 (T6) | Free-page reuse completely removes old free metadata | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 141 (T7) | An old asynchronous completion cannot alter a reused page | §§8.18–8.20; §§14.5–14.15 | Detach, free-page reuse, and stale-reference safety | Deterministic fixture/oracle |
| 142 (U1) | Leaf split reopens with exact-once contents and coherent links | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 143 (U2) | Internal/root split reopens with coherent levels/root metadata | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 144 (U3) | Merge/root contraction reopens with detached pages unreachable | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 145 (U4) | Free-page reuse reopens with exclusive free-or-live ownership | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 146 (U5) | Endpoint changes reopen consistent with the leaf chain | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 147 (U6) | Interrupted structural MTR recovers as exact old or complete new tree | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 148 (U7) | Repeated recovery is idempotent and does not persist generation drift | §§8.25–8.28; §§13.13.3–13.14 | BTREE_MTR failure, crash, append, and recovery | Deterministic fixture/oracle |
| 149 (V1) | Maximum legal height remains a valid tree | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 150 (V2) | Growth beyond legal height fails before partial root publication | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 151 (V3) | Attempted oversized key differs from malformed persisted key length | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 152 (V4) | Page-local no-space selects compaction/split rather than disk exhaustion | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 153 (V5) | NO_REPLACEABLE_FRAME differs from RESOURCE_FULL and numeric exhaustion | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 154 (V6) | Known WAL failure preserves exact pre-authorized tree state | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 155 (V7) | Uncertain authorizing append enters the noncontinuable lifecycle | §§4.3.2, 8.6, 8.15.1, 39.1 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 156 (W1) | Physical distinct-RID duplicates do not decide SQL uniqueness | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 157 (W2) | Physical equality enumeration supplies all uniqueness candidates | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 158 (W3) | Logical unique-key locking and current-owner decisions remain Chapter 11 owned | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 159 (W4) | Index presence does not decide MVCC visibility | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 160 (W5) | Structurally valid stale entries are not corruption by themselves | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 161 (W6) | Index entry removal alone does not authorize RID reuse | §§8.22–8.23; §§10–11; §14.15 | Cross-owner semantics, exhaustion, and failure classification | Deterministic fixture/oracle |
| 162 (X1) | Ordinary B+ pages use BufferPool-managed PageId/guard lifetime | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle |
| 163 (X2) | No specialized index path creates a second cache owner | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle |
| 164 (X3) | Production operations are compared to an independent ordered model | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle |
| 165 (X4) | Randomized and stress coverage remains complementary to deterministic cases | §§8.1, 8.21, 8.24; §41.2 | Deterministic harness, observability, and reference models | Deterministic fixture/oracle |
| 166 (M10) | Delete-safe and delete-unsafe children release or retain ancestors according to propagation risk | §8.19.2 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |
| 167 (M11) | Optimistic ancestor release is followed by required structural revalidation or restart | §8.19.2 | Latching, write crabbing, and cursor lifetime | Deterministic fixture/oracle |


---

## B+ Tree Benchmarks

Measure:

```text
random point lookup ops/sec
sorted insertion ops/sec
random insertion ops/sec
exact deletion ops/sec
short range scan rows/sec
long range scan rows/sec
duplicate-heavy equality lookup
split frequency
tree height
average leaf byte occupancy
average internal byte occupancy
buffer-pool hit rate
latch wait time when available
```

Benchmark key shapes:

```text
INT64
short VARCHAR
long VARCHAR
composite keys
```

Apply the shared Chapter-42 evidence policy and reuse the independent logical ordered
model and §8.28/L3 structural verifier for correctness. For each key class, predeclare
the key cardinality, generator/seed, encoded-width range, insertion and lookup order,
duplicate distribution, and valid RID construction. A replayable base fixture uses
nonnegative INT64 ranks `0..N-1`; short binary-collated VARCHAR keys encode each rank
as 16 fixed-width ASCII hexadecimal bytes; long VARCHAR keys use the same prefix plus
240 deterministic ASCII padding bytes; a composite fixture pairs the INT64 rank with
the short VARCHAR. Choose and record `N` per intended regime, respecting the owner's
encoded-key limit. These concrete lengths are Verification fixtures, not Architecture
constants. Generate physical `(user key, RID)` entries under the canonical comparator.

- Random point lookups replay a recorded seeded sequence, declaring existing-key and
  missing-key counts separately. Sorted insertion uses canonical increasing key order;
  random insertion uses a recorded fixed-seed permutation of the comparable key set.
  Record successful insert counts. Exact deletion starts from a reproducible tree and
  deletes a declared existing-key/RID set in recorded order, with expected post-delete
  contents checked against the logical model.
- Short and long range scans use predeclared bounds chosen from the sorted reference
  set, with known expected cardinalities and the same key shape. The long range must
  cover materially more rows than the short range; neither may be classified by its
  name alone. A separate duplicate-heavy fixture uses a declared set of logical keys
  with 32 distinct RIDs each; verify the expected equality result count and physical
  `(user key, RID)` uniqueness. Record the key/RID distribution rather than assuming
  duplicates from the requested operation.
- For split frequency, use a dedicated insertion fixture that actually publishes at
  least one split according to the B+ owner event/counter. Report split count against
  successful insertion count (for example splits per insert), with both counts scoped
  to the same interval. Zero observed splits leaves this split-frequency regime
  unverified; it is not a characterization of splitting behavior.
- At safe quiescence, obtain the actual reachable tree pages, page kinds, and height
  from canonical tree state, with L3 validation where applicable; never infer them from
  key count or theoretical fanout alone. Compute leaf and internal average byte
  occupancy separately from valid slot-directory plus packed-entry bytes divided by
  the owner-defined usable node bytes (§8.16), over the corresponding validated reachable
  pages. Structural inspection and L3 time stay outside operation-throughput windows
  unless their cost is explicitly the benchmark subject. A wrapper's reported height,
  split count, or occupancy is not an independent oracle for the underlying owner state.
- Report BufferPool hit rate from same-interval owner hit/(hit + miss) fetch claims,
  identifying the benchmark's page-access scope; keep logical operations distinct from
  completed physical reads/writes. If canonical page-latch wait instrumentation is
  supported, report its interval, unit, and aggregation. Genuine lack of optional
  instrumentation is N/A; expected-but-missing evidence is TEST INFRASTRUCTURE
  INCOMPLETE, never zero or a latency-derived estimate.

Benchmark both:

### Hot tree

Mostly resident in buffer pool.

Choose a tree-page working set reached by the measured operations that fits the actual
configured BufferPool capacity. Precondition and confirm ordinary residency, then use
same-interval canonical hit/miss and physical-read observations to show that the intended
hot path ran. Key count or a prior traversal alone does not establish residency.

Focus:

- CPU,
- comparisons,
- latches,
- cache behavior.

### Larger-than-buffer tree

Working set exceeds buffer pool.

Record the actual tree-page working set reached by the operation sequence and actual
configured BufferPool frame capacity; require working-set pages to exceed capacity.
Confirm measured misses, replacements, and completed physical reads from their owners.
Keep key shape and operation mix comparable with the hot-tree case where possible,
recording deliberate setup differences. OS/file-cache warmth is a separate declared
condition; no direct-I/O device or perfectly cold cache is required.

Focus:

- fanout,
- page access patterns,
- replacement,
- random I/O sensitivity.
