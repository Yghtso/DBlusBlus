# 8. B+ Tree Indexing

## 8.1 Scope and subsystem role

The primary general-purpose persistent index is a page-backed B+ tree.

The production tree operates through the real BufferPool and MUST be persistent
under the page and WAL ownership rules of Chapters 7 and 12. A separate
in-memory pointer tree is not a conforming index.

The architecture supports:

- equality lookup,
- ordered forward range scans,
- composite keys,
- variable-length VARCHAR keys,
- duplicate SQL user keys,
- SQL UNIQUE indexes enforced transactionally above the physical tree,
- concurrent readers and writers,
- leaf and internal splits,
- redistribution and merge,
- root replacement and contraction,
- tree-local page reuse,
- physical heap tuple-version RIDs,
- WAL/recovery integration.

The B+ tree owns physical ordered index structure.

It does not own SQL parsing, snapshot visibility, or user-transaction uniqueness semantics.

## 8.2 Index file and superblock metadata

Each index owns one page-based file:

```text
index_<index_id>.btree
```

with:

```text
page 0       B+ tree superblock
page 1..N    internal / leaf / free pages
```

The file uses the `BTREE` file kind defined by the persistent-storage foundation.

The B+ tree superblock is a file-kind-specific extension of the common 72-byte FileSuperblock prefix.

The common prefix stores:

```text
file_kind   = BTREE
object_id   = IndexId
header_size = 128
```

The B+ extension occupies bytes `72..127`.

### 8.2.1 BTREE superblock extension v1

| Page offset | Size | Field / required meaning |
|---:|---:|---|
| `72` | 8 | `table_id` |
| `80` | 8 | `root_page_no` |
| `88` | 8 | `first_leaf_page_no` |
| `96` | 8 | `last_leaf_page_no` |
| `104` | 8 | `free_page_head` |
| `112` | 8 | `key_schema_fingerprint` |
| `120` | 4 | `key_schema_version = 1` |
| `124` | 2 | `tree_height` |
| `126` | 2 | `index_flags = 0` |
| `128` | 8064 | reserved bytes, all zero |
|  | **8192** | **total page** |

All multi-byte fields are little-endian.

`index_flags` has no assigned bits. Encoders MUST write zero and decoders MUST reject nonzero `index_flags`.

`key_schema_version` is an independent required format domain under §4.14.6. V1 accepts only `1`; zero is corruption, a greater value makes the committed index unsupported, and a version-1 fingerprint mismatch is corruption rather than forward compatibility.

The B+ tree does not duplicate `IndexId` in the extension; the common FileSuperblock `object_id` is the index identifier.

`root_page_no`, `first_leaf_page_no`, and `last_leaf_page_no` MUST be valid non-sentinel page numbers in an initialized tree.

`free_page_head` uses:

```text
INVALID_PAGE_NO
```

for an empty B+ free list.

An initialized empty tree is represented by one valid empty leaf:

```text
tree_height = 1
root_page_no = one empty leaf page
first_leaf_page_no = root_page_no
last_leaf_page_no  = root_page_no
root.slot_count    = 0
```

A normal initialized empty tree MUST NOT use an invalid root.

The B+ superblock extension above is byte-exact.

Its `key_schema_fingerprint` is defined in §8.3.1 so the persisted metadata can be validated independently of compiler enums or ABI layout.

## 8.3 Index key schema

Each B+ tree has one fixed logical key schema supplied by catalog metadata and fingerprinted in the B+ tree superblock.

The architecture supports:

```text
ascending key components
NULLS FIRST
binary VARCHAR collation
```

Native descending physical index components and locale-aware collations are deferred.

Descending SQL results MAY be produced by executor sorting; native reverse index traversal is outside the supported scope as specified in §8.20.1.

The B+ tree's key-order semantics MUST agree with the database's SQL comparison semantics for the supported indexable types.

### 8.3.1 Key-schema fingerprint v1

The B+ superblock persists:

```text
key_schema_version     = 1
key_schema_fingerprint = FNV-1a-64(canonical_descriptor)
```

The 64-bit FNV-1a parameters are:

```text
offset_basis = 14695981039346656037
prime        = 1099511628211
```

For each descriptor byte:

```text
hash = hash XOR byte
hash = hash * prime mod 2^64
```

The canonical descriptor bytes are:

```text
ASCII "DBLUS-INDEX-KEY-SCHEMA-V1"
0x00
component_count as uint32 little-endian
then, for each key component in schema order:
    1 byte physical type code
    1 byte order code
    1 byte null-order code
    1 byte collation code
```

Fingerprint-only physical type codes are:

```text
1 BOOLEAN
2 INT32
3 INT64
4 FLOAT64
5 DATE
6 TIMESTAMP
7 VARCHAR
```

Order/null/collation codes are:

```text
order:
    0 = ASC

null order:
    0 = NULLS_FIRST

collation:
    0 = NONE / not applicable
    1 = BINARY
```

For VARCHAR, the architecture requires `collation = BINARY`.

For non-VARCHAR components, the architecture requires `collation = NONE`.

These codes define only the canonical fingerprint descriptor; they do not replace the catalog's logical type representation.

The fingerprint is a deterministic mismatch detector, not a cryptographic identity. Opening an index MUST reject a mismatch between the expected key-schema descriptor and the persisted fingerprint.

## 8.4 User keys, physical keys, and RID encoding

The architecture distinguishes:

```text
user key
    SQL-visible indexed-column tuple

physical key
    (user key, RID)
```

RID is a deterministic tiebreaker.

Consequently, every physical B+ tree entry has a unique total position even when multiple SQL rows have equal user keys.

This physical-key choice simplifies:

- duplicate-key representation,
- duplicate ranges spanning leaves,
- split separators,
- lower/upper-bound search,
- exact physical deletion.

### 8.4.1 Persisted RID encoding

An RID stored in a B+ tree entry occupies exactly 16 bytes:

| RID-relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `heap_file_id` |
| `4` | 8 | `heap_page_no` |
| `12` | 2 | `heap_slot_id` |
| `14` | 2 | reserved |
|  | **16** | **total** |

Multi-byte fields use the persistent integer byte order defined by the storage foundation.

Physical RID ordering is numeric lexicographic order:

```text
(heap_file_id, heap_page_no, heap_slot_id)
```

For the defined architecture, bytes `14..15` are strict reserved-zero bytes:

```text
encoder:
    MUST write zero

decoder:
    MUST reject nonzero values
```

A persisted leaf RID MUST also satisfy:

```text
heap_file_id != INVALID_FILE_ID
heap_page_no != INVALID_PAGE_NO
heap_slot_id != INVALID_SLOT_ID
```

A context-free RID decoder can validate these sentinel rules.

When the index handle knows the expected heap `FileId`, tree/index-level validation MUST additionally reject a leaf RID whose `heap_file_id` does not belong to the indexed relation.

This strict decoder rule is part of the persisted architecture contract.

## 8.5 Memcomparable user-key encoding

`IndexKeyCodec` converts logical indexed values into an order-preserving byte sequence.

For supported user keys `A` and `B`:

```text
lexicographic_compare(Encode(A), Encode(B))
```

MUST produce the same ordering as the database's index comparator.

The B+ tree therefore compares encoded bytes in the hot path rather than invoking a polymorphic SQL comparator for each binary-search comparison.

The index encoding is allowed to differ from ordinary tuple serialization because its purpose is order preservation, not compact row storage.

### 8.5.1 Field presence prefix

Every encoded field begins with:

```text
0x00 = NULL
0x01 = non-NULL
```

which establishes ascending `NULLS FIRST`.

A NULL field consists of the NULL marker for that component; non-NULL fields continue with the type-specific encoding below.

### 8.5.2 BOOLEAN

For a non-NULL BOOLEAN, the value byte is:

```text
false -> 0x00
true  -> 0x01
```

### 8.5.3 INT32 and DATE

Interpret the signed value as a 32-bit bit pattern and flip the sign bit:

```text
sortable =
    bits(value) XOR 0x80000000
```

Then encode `sortable` big-endian.

### 8.5.4 INT64 and TIMESTAMP

Interpret the signed value as a 64-bit bit pattern and flip the sign bit:

```text
sortable =
    bits(value) XOR 0x8000000000000000
```

Then encode `sortable` big-endian.

### 8.5.5 FLOAT64

Before encoding FLOAT64, normalize the IEEE-754 binary64 payload bits:

```text
-0.0 and +0.0
    -> 0x0000000000000000

every NaN
    -> 0x7ff8000000000000
```

Let `u` be the normalized 64-bit payload.

Compute:

```text
if (u & 0x8000000000000000) != 0:
    sortable = bitwise_not(u)
else:
    sortable = u XOR 0x8000000000000000
```

where `bitwise_not` and the XOR operate on exactly 64 bits.

Encode `sortable` as an unsigned 64-bit integer in big-endian byte order.

This yields the required total order:

```text
-infinity
...
negative finite values
...
zero
...
positive finite values
...
+infinity
NaN
```

All NaNs compare equal for index-key semantics because they normalize to the same canonical payload.

The SQL FLOAT64 comparison layer MUST use the same equality/order semantics.

### 8.5.6 VARCHAR

The architecture uses binary bytewise collation.

Encode payload bytes as:

```text
ordinary non-zero byte -> itself
0x00                   -> 0x00 0xFF
```

Terminate the VARCHAR field with:

```text
0x00 0x00
```

This makes the field self-delimiting while preserving binary lexicographic byte-string ordering.

### 8.5.7 Composite keys

Composite user keys concatenate encoded component fields in physical key-schema order:

```text
field0 || field1 || ... || fieldN
```

The combined user-key byte string remains lexicographically order preserving.

No per-comparison type dispatch is required inside the B+ tree.

## 8.6 Encoded-key size and physical comparison

The maximum encoded **user-key** size in the architecture is:

```text
1024 bytes
```

An operation whose encoded user key exceeds this bound MUST fail explicitly.

The key MUST NOT be silently truncated.

The bound exists to preserve useful fanout on 8192-byte pages and avoid overflow-key pages in the defined architecture.

Physical-key comparison is:

```text
1. lexicographically compare encoded user-key bytes
2. if equal, numerically compare RID
```

Conceptual search-only sentinels:

```text
MIN_RID
MAX_RID
```

bound the complete duplicate range for user key `K`:

```text
(K, MIN_RID) ... (K, MAX_RID)
```

These search sentinels need not be persistable real RIDs.

Deferred key-space extensions include overflow keys, key compression, separator-prefix truncation, and alternative page sizes.

## 8.7 Node-page organization

Leaf and internal nodes are 8192-byte slotted pages using:

```text
32-byte common page header
64-byte total node header
8-byte slot descriptors
variable-length packed entries
```

The slot directory begins at byte `64` and grows upward.

Packed entry bytes grow downward from the end of the page.

Conceptually:

```text
0
┌──────────────────────────────┐
│ common page header           │
│ B+ tree node header          │
├──────────────────────────────┤
│ slot 0                       │
│ slot 1                       │
│ ...                          │
│                        lower │
├──────────────────────────────┤
│ contiguous free space        │
├──────────────────────────────┤
│ upper                        │
│ packed entry bytes           │
│ ...                          │
└──────────────────────────────┘
8192
```

For B+ leaf/internal pages:

```text
format_version     = 1
header_size        = 64
common flags       = 0
common reserved16  = 0
node flags         = 0
```

Encoders MUST write zero and decoders MUST reject nonzero values in these unassigned/reserved fields.

A blank node initializes:

```text
slot_count = 0
lower      = 64
upper      = PAGE_SIZE
```

For every valid node:

```text
lower = 64 + slot_count * 8
64 <= lower <= upper <= PAGE_SIZE
```

The slot array is kept in sorted physical-key order.

Packed entry payload bytes do not need to be physically ordered.

Insertion into the middle of a node SHOULD normally move slot descriptors rather than rewriting all existing variable-length entry payloads.

Page-local compaction MAY repack entry bytes and rewrite slot offsets while holding the page write latch.

## 8.8 Node slot entry

Every B+ tree slot descriptor occupies exactly 8 bytes:

| Slot-relative offset | Size | Field |
|---:|---:|---|
| `0` | 2 | `entry_offset` |
| `2` | 2 | `entry_length` |
| `4` | 2 | `user_key_length` |
| `6` | 2 | `flags` |
|  | **8** | **total** |

The slot `flags` field has no assigned bits.

```text
slot.flags = 0
```

Encoders MUST write zero and decoders MUST reject nonzero slot flags.

## 8.9 Leaf-node format

A leaf node has a 64-byte total header:

| Page offset | Size | Field |
|---:|---:|---|
| `0..31` | 32 | common page header |
| `32` | 2 | `level = 0` |
| `34` | 2 | `slot_count` |
| `36` | 2 | `lower` |
| `38` | 2 | `upper` |
| `40` | 4 | `flags` |
| `44` | 8 | `prev_leaf_page_no` |
| `52` | 8 | `next_leaf_page_no` |
| `60` | 4 | reserved |
|  | **64** | **total** |

Leaf level is always:

```text
0
```

A leaf packed entry is:

```text
encoded user key     user_key_length bytes
RID                  16 bytes
```

Therefore:

```text
entry_length = user_key_length + 16
```

The B+ tree stores no:

```text
xmin
xmax
snapshot metadata
```

inside leaf entries.

For leaf pages:

```text
flags                    = 0
reserved bytes 60..63    = 0
no previous leaf         = INVALID_PAGE_NO
no next leaf             = INVALID_PAGE_NO
```

Leaf-node flag/reserved validation follows the canonical B+ node rules in §8.7.

## 8.10 Internal-node format and routing semantics

An internal node also has a 64-byte total header:

| Page offset | Size | Field |
|---:|---:|---|
| `0..31` | 32 | common page header |
| `32` | 2 | `level > 0` |
| `34` | 2 | `slot_count` |
| `36` | 2 | `lower` |
| `38` | 2 | `upper` |
| `40` | 4 | `flags` |
| `44` | 8 | `leftmost_child_page_no` |
| `52` | 12 | reserved |
|  | **64** | **total** |

An internal node with `N` separator entries has:

```text
N + 1 children
```

Each packed internal entry contains:

```text
encoded user key       variable bytes
separator RID          16 bytes
right_child_page_no     8 bytes
```

Every separator is therefore a complete physical key:

```text
(user key, RID)
```

The full physical separator is required so routing remains correct when equal SQL keys span multiple subtrees.

For internal pages:

```text
flags                 = 0
reserved bytes 52..63 = 0
```

Internal-node flag/reserved validation follows the canonical B+ node rules in §8.7.

### 8.10.1 Routing lower bounds

An internal node is represented conceptually as:

```text
C0, (K1 -> C1), (K2 -> C2), ... (Kn -> Cn)
```

with:

```text
all keys in C0 < K1

for each i > 0:
    all keys in Ci >= Ki

and, when K(i+1) exists:
    all keys in Ci < K(i+1)
```

`Ki` is a routing lower bound for child `Ci`.

Immediately after a split it normally equals the minimum physical key in `Ci`.

After ordinary deletion it MAY remain stale-low.

For example, if a right subtree minimum moves from `50` to `60`, retaining separator `50` remains correct so long as every key in that subtree is still `>= 50`.

A stale-low separator may cause an unnecessary descent that finds no key; it MUST NOT cause a real key to be missed.

A separator need not be tightened only because deletion increased the right-child minimum.

A separator MUST change when a structural operation moves entries across its routing boundary, including:

- redistribution left-to-right,
- redistribution right-to-left,
- right-child replacement,
- split installation of a new child.

After redistribution, the simplest correct separator is the first physical key of the new right child.

## 8.11 Node search

### 8.11.1 Internal search

For target physical key `T`, internal routing uses binary search equivalent to:

```text
upper_bound(separators, T)
```

so:

```text
T < K1            -> C0
K1 <= T < K2      -> C1
K2 <= T < K3      -> C2
...
Kn <= T           -> Cn
```

Production internal-node lookup MUST NOT linearly scan separators.

### 8.11.2 Leaf search

Leaf lookup uses binary search equivalent to:

```text
lower_bound(physical_keys, target)
```

Exact physical lookup uses:

```text
target =
    (encoded_user_key, RID)
```

SQL-user-key equality scanning begins at:

```text
target =
    (encoded_user_key, MIN_RID)
```

and proceeds forward until the user key changes.

For a range scan, binary search finds the initial slot once; iteration then proceeds sequentially rather than repeating a binary search for every next entry.

The hot comparison path SHOULD primarily be encoded-byte lexicographic comparison plus RID comparison when encoded user keys are equal.

## 8.12 Split trigger and node compaction

Before splitting because an insertion does not fit:

```text
if contiguous free bytes are insufficient
and existing fragmented/reclaimable bytes would be sufficient:
    compact the node
```

A split occurs only if the pending entry still does not fit after any useful compaction.

Variable-length node capacity, split balance, and occupancy are measured in **bytes**, not entry count.

## 8.13 Leaf split and sibling links

A leaf split:

1. includes the pending new entry in the conceptual sorted entry set,
2. chooses a boundary producing approximately balanced byte usage,
3. leaves at least one entry on each resulting leaf,
4. keeps lower physical keys in the original left page,
5. moves higher physical keys to a new right page,
6. copies the first physical key of the new right leaf into the parent as separator.

The separator remains present in the right leaf.

Splitting solely by entry count is not permitted because encoded keys are variable length.

### 8.13.1 Sibling-link publication

If the pre-split chain is:

```text
P <-> L <-> N
```

then after splitting `L`:

```text
P <-> L <-> R <-> N
```

with:

```text
R.prev = L
R.next = old L.next
L.next = R

if N exists:
    N.prev = R
else:
    superblock.last_leaf = R
```

Both forward and backward links are persisted.

The architecture exposes native forward scans; maintaining the backward links does not by itself require native reverse scans.

## 8.14 Internal split

Given:

```text
C0, K1->C1, ... Km->Cm, ... Kn->Cn
```

an internal split chooses promoted physical key `Km` by byte balance.

The left page retains:

```text
C0
K1->C1
...
K(m-1)->C(m-1)
```

The parent receives:

```text
Km
```

The right page receives:

```text
leftmost_child = Cm
K(m+1)->C(m+1)
...
Kn->Cn
```

Unlike leaf splitting, the promoted internal separator is removed from the child level.

## 8.15 Root split and contraction

### 8.15.1 Root split

When the current root splits:

```text
allocate new internal root
old root -> leftmost child
new split page -> right child
install one separator
new_root.level = old_root.level + 1
update root_page_no
update tree_height
```

Root growth applies §4.3.2's uint16 boundary: `tree_height` may be at most
`UINT16_MAX` and every published node level is smaller than that height. If
`old_root.level + 1` or the corresponding new height is not representable, the
insert/MTR fails before provisional mutation or page-allocation publication.

### 8.15.2 Root contraction

If an internal root reaches zero separator entries:

```text
its single child becomes the root
old root is retired/recycled
tree_height decreases by one
```

If the root is a leaf and becomes empty, that leaf remains the valid empty root and tree height remains `1`.

## 8.16 Occupancy and underflow

For B+ node occupancy:

```text
used_bytes =
    slot_directory_bytes
    +
    packed_entry_bytes
```

measured relative to:

```text
PAGE_SIZE - 64
```

A non-root node becomes a practical rebalance candidate below approximately:

```text
25% byte occupancy
```

This is a soft threshold, not a strict half-full B+ tree invariant.

The softer threshold is intended to reduce split/merge oscillation and write amplification with variable-length keys.

A temporarily sparse page is a performance issue rather than structural corruption.

## 8.17 Redistribution and merge

For an underfull page:

1. inspect an adjacent sibling with the same parent,
2. prefer redistribution when it can restore healthy occupancy,
3. otherwise merge when the combined entries fit in one page,
4. propagate parent underflow when a separator is removed.

Whenever redistribution moves keys across a routing boundary, the relevant parent separator MUST be updated.

Ordinary deletion that does not move keys between children does not require separator tightening.

### 8.17.1 Leaf merge

When possible, leaf merge is deterministic:

```text
merge right into left
```

Update:

```text
left.next = right.next

if right.next exists:
    right.next.prev = left
else:
    superblock.last_leaf = left
```

Remove the separator and child pointer for `right` from the parent, then retire `right`.

### 8.17.2 Internal rebalancing

Internal redistribution/merge is defined in terms of:

```text
children + routing separators
```

rather than blindly moving serialized entry bytes.

The parent separator participates in the child boundary and MUST be transformed consistently.

Internal redistribution/merge MAY reconstruct the affected logical child/separator sequences or use another physically equivalent transformation. Every implementation MUST preserve the same canonical child/separator sequence, key ordering, routing bounds, occupancy legality, `BTREE_MTR` scope, and publication invariants.

## 8.18 Tree-local free pages and safe reuse

The B+ tree superblock stores:

```text
free_page_head
```

A retired B+ page becomes `BTREE_FREE`.

The free-page format is:

```text
page_type          = BTREE_FREE
format_version     = 1
header_size        = 40
common flags       = 0
common reserved16  = 0
```

| Page offset | Size | Field / required meaning |
|---:|---:|---|
| `0` | 32 | common page header |
| `32` | 8 | `next_free_page_no` |
| `40` | 8152 | reserved bytes, all zero |
|  | **8192** | **total** |

`next_free_page_no` is a uint64 little-endian PageNo.

The terminal free-list link is:

```text
INVALID_PAGE_NO
```

A v1 decoder MUST reject nonzero common flags/common reserved16 or any nonzero byte in the free-page reserved region.

Allocation policy is:

```text
if tree-local free list is non-empty:
    reuse a retired B+ page
else:
    append a page to the index file
```

A global extent allocator is not required solely for the B+ tree.

The tree-local free-list head and every terminal free-page link use `INVALID_PAGE_NO`.

The complete in-range/exclusion, acyclic, unique-membership, free/live-disjointness, bounded-allocation, and verifier rules are canonical in §4.13.4–§4.13.5.

### 8.18.1 Reuse safety

A retired page may be recycled only after:

- it is detached from every installed parent/root/sibling route that can legally reach it,
- the write latches required for that detachment are held,
- no traversal can retain an unprotected stale page reference.

This relies on latch-coupled traversal and guarded leaf handoff.

Long-lived raw `PageNo` cursors MUST NOT be exposed outside the B+ tree implementation.

## 8.19 Concurrency and latch ordering

Transaction-level logical locks and B+ tree page/metadata latches remain separate mechanisms.

### 8.19.1 Read traversal

Point lookup uses read-latch coupling:

```text
read-latch parent
    ↓
determine child
    ↓
pin + read-latch child
    ↓
release parent
```

The child is pinned and latched before the parent latch is released.

### 8.19.2 Write traversal

Insert/delete uses top-down write latch crabbing:

```text
write-latch parent
    ↓
write-latch child
    ↓
if child is safe for the operation:
    release older ancestors
else:
    retain required ancestors
```

For insertion, a node is safe only if the current operation cannot force a split to propagate above it.

For an internal node, this safety test includes possible separator insertion caused by a child split.

For deletion, a node is safe only if the current deletion cannot require redistribution/merge that propagates upward.

The architecture does not require a tree-wide write lock for ordinary writes.

### 8.19.3 Root metadata latch and optimistic root validation

Each tree has a small root-metadata latch protecting process/runtime access to structurally sensitive metadata such as:

- current `root_page_no`,
- current tree height,
- structurally necessary first/last leaf metadata,
- free-list-head metadata.

Runtime root metadata also maintains a process-local monotonically increasing:

```text
root_generation
```

`root_generation` is not persisted and does not participate in crash recovery.

It increments whenever a runtime structural publication changes:

```text
root_page_no
or
tree_height
```

Normal traversal obtains the root optimistically:

```text
1. acquire root-metadata latch
2. read candidate root_page_no and root_generation
3. pin candidate root while metadata still identifies it
4. release root-metadata latch

5. acquire the required latch on the pinned candidate root

6. while holding the candidate root page latch:
       acquire root-metadata latch
       validate:
           root_page_no == candidate
           root_generation == captured generation
       release root-metadata latch

7. if validation failed:
       release page latch/pin
       restart from the root
```

A thread MUST NOT wait to acquire a B+ page latch while holding the root-metadata latch.

This is the critical anti-deadlock rule for root acquisition.

A structural operation that already holds the relevant B+ page latch(s) MAY acquire the root-metadata latch to publish a new root/height or other protected metadata.

Thus the waiting order for operations that hold page latches is:

```text
B+ page latch(es)
    ->
root-metadata latch
```

while root acquisition uses the metadata latch only to snapshot/pin identity and releases it **before** waiting on the page latch.

Root replacement publication increments `root_generation` before releasing the metadata latch.

The old root page cannot become reusable merely because the superblock/runtime root pointer changed; normal B+ safe-detachment/page-reuse rules still apply.

### 8.19.4 Free-list and endpoint metadata updates

Operations that need `free_page_head`, `first_leaf_page_no`, or `last_leaf_page_no` and also need a B+ page latch use optimistic validate/update rather than holding the metadata latch while waiting on that page.

For example, popping a free page conceptually uses:

```text
1. snapshot free_page_head under metadata latch
2. release metadata latch
3. pin + write-latch the candidate BTREE_FREE page
4. read candidate.next_free_page_no
5. acquire metadata latch while still holding the page latch
6. validate free_page_head still equals candidate
7. if valid:
       publish free_page_head = candidate.next_free_page_no
   else:
       release and restart
```

Equivalent validation/restart applies when first/last leaf metadata must be changed concurrently.

### 8.19.5 Page-latch acquisition order

Vertical acquisition order is:

```text
parent before child
```

When adjacent leaf pages must be latched horizontally, acquisition order is:

```text
left to right in key order
```

If an operation already holding a later/right page discovers it would need to wait for an earlier/left page:

```text
release and restart
```

rather than waiting in the reverse order.

This rule avoids a major class of structural deadlocks.

## 8.20 Forward range scans and cursor lifetime

The architecture natively supports ascending B+ tree range scans.

A forward cursor conceptually retains:

```text
current ReadPageGuard
current slot index
encoded upper bound
upper-bound inclusivity
```

Any tuple/key view derived from the current leaf MUST NOT outlive the current leaf page guard.

At the end of leaf `L`, handoff is latch coupled:

1. while holding `L` read latch, read `L.next`,
2. if no next page exists, finish,
3. pin and read-latch the next leaf,
4. perform the complete §4.13.5 same-owner, type/level, reciprocal-link, key-boundary, and bounded-progress checks,
5. release `L`,
6. continue.

Holding the current leaf until the next leaf is safely pinned/latched prevents a concurrent merge from detaching and reusing the target page during handoff.

### 8.20.1 Reverse scans

`prev_leaf_page_no` is maintained in the defined architecture, but native descending/reverse scans are deferred from the architecture baseline.

Bidirectional latch coupling introduces additional deadlock-order concerns.

The architecture therefore defines no reverse-cursor latch/restart protocol. Adding native reverse traversal requires an explicit architecture revision that preserves the page-lifetime and latch-order invariants.

## 8.21 B+ tree and IndexKeyCodec API boundaries

Physical B+ tree operations provide responsibilities equivalent to:

```text
Insert(encoded_user_key, RID)
Erase(encoded_user_key, RID)

FindPhysical(encoded_user_key, RID)
LowerBound(encoded_user_key, RID-bound)
Scan(lower_bound, upper_bound)
```

A convenience user-key lookup may be implemented as a physical range:

```text
(K, MIN_RID) ... (K, MAX_RID)
```

Exact C++ method names are not part of the architecture contract.

The physical B+ tree does not own:

- SQL AST nodes,
- tuple visibility,
- user-transaction uniqueness policy.

`IndexKeyCodec` owns:

- logical indexed values -> memcomparable user-key bytes,
- type-specific order-preserving encoding,
- NULL ordering,
- composite-key encoding,
- key-size validation,
- FLOAT64 canonicalization,
- binary VARCHAR collation.

The tree receives opaque encoded user-key bytes plus RIDs.

One exact physical `(encoded_user_key,RID)` has at most one stored entry. `Insert` encountering that exact entry is idempotent success only for the same in-process mutation owner that has continuously retained its operation context/logical ownership and recorded that exact insertion as already published. An ordinary executor insertion without that provenance reports an internal invariant/corruption error, not a SQL UNIQUE violation. Recovery applies/skips `BTREE_MTR` page actions by page LSN and does not invoke SQL uniqueness. Duplicate stored copies of one exact physical key are structural corruption.

## 8.22 Duplicate keys, uniqueness, and MVCC

The physical tree always permits duplicate SQL user keys because physical keys differ by RID.

Entries with one user key appear consecutively in RID order.

This remains true for a SQL UNIQUE index because obsolete, aborted, and in-progress tuple versions can coexist physically.

### 8.22.1 Transactional uniqueness boundary

Uniqueness MUST NOT be enforced as:

```text
if BTree.Contains(user_key):
    reject
```

Matching physical entries may reference:

- globally dead versions,
- aborted versions,
- versions created by the current transaction,
- in-progress conflicting transactions.

The architectural shape is:

```text
acquire logical unique-key protection
    ↓
scan all physical entries with the user key
    ↓
fetch referenced heap versions
    ↓
consult transaction/MVCC state
    ↓
reject or permit insertion
```

The exact unique-key domain, lock identity, acquisition/recheck order, duration, current-owner predicate, UPDATE self-exclusion, and physical-retry distinction are canonical in §§11.8–11.10.

For SQL UNIQUE semantics:

```text
if any indexed key component is NULL:
    duplicate user keys are allowed
```

Only fully non-NULL user keys participate in duplicate rejection.

NULL-containing keys are still physically encoded, ordered, and stored normally.

### 8.22.2 Visibility

An index entry contains:

```text
encoded user key -> RID
```

while MVCC metadata remains in the heap tuple version.

Therefore an ordinary index scan is:

```text
B+ candidate
    ↓
RID
    ↓
heap fetch
    ↓
MVCC visibility check
    ↓
return or reject
```

A physical B+ tree hit never proves that a SQL row is visible.

Likewise, a physical hit never proves UNIQUE conflict ownership. Constraint enforcement applies §11.10's heap/status/current-command predicate under logical key serialization, while ordinary scans continue to apply Chapter-10 snapshot visibility.

Because visibility remains heap-owned, arbitrary index-only scans are deferred until additional machinery such as all-visible metadata and covering payloads exists.

## 8.23 UPDATE, DELETE, vacuum, and aborted user DML

With heap-version MVCC:

### UPDATE

A baseline UPDATE creates:

```text
new heap tuple version
    ↓
new RID
    ↓
new physical (key, RID) index entry
```

Even if indexed column values are unchanged, UPDATE creates the new physical `(key, new_RID)` entry.

Old physical entries remain until vacuum.

### DELETE

Logical SQL deletion updates heap MVCC metadata.

It does not synchronously remove every corresponding B+ tree entry.

### Vacuum

Before physically reclaiming a globally dead heap tuple version, vacuum can derive each indexed user key and erase the exact physical entry:

```text
Erase(encoded_user_key, dead_RID)
```

RID participation makes this exact even among duplicate user keys.

### Aborted user DML

The canonical transaction/recovery architecture establishes: ordinary user-DML heap/index modifications are not physically undone on user abort.

Consequently, an aborted INSERT/UPDATE may leave an index entry that references an invisible aborted heap version.

Vacuum removes such physical garbage later.

This refinement does not change the physical ordering or B+ structural invariants.

HOT-like update behavior that avoids some new index entries remains deferred.

## 8.24 BufferPool integration and index-scan cost

Root and internal B+ pages remain ordinary BufferPool pages.

The B+ tree MUST NOT create a second private cache for upper tree levels.

Frequently accessed root/internal pages should become resident naturally through the database replacement policy.

Any specialized residency treatment MUST remain coordinated through BufferPool policy and preserve BufferPool ownership of PageId identity, pin/latch lifetime, validation, dirty state, WAL-before-data, and retirement. It MUST NOT create a second cache or bypass page-lifetime/flush ownership.

Secondary-index order does not imply heap-page locality.

A range scan may therefore generate many random heap-page accesses.

The optimizer MUST model this cost rather than assuming that the presence of an index automatically makes an index scan cheaper.

## 8.25 Structural modification and WAL boundary

A logical user index action is distinct from a B+ tree structural modification.

For example:

```text
user INSERT
    logical effect:
        create physical (key, RID) entry

    possible structural effects:
        split leaf
        split internal node
        replace root
```

A structurally valid split/merge/root shape is not conceptually rolled back merely because the owning user transaction later aborts.

Chapter 12 defines the exact recovery mechanism: every physical B+ mutation is a recovery-safe B+ mini-transaction/system structural action encoded as one logical `BTREE_MTR` record with a temporary no-flush barrier.

That same protocol also owns non-crash failure atomicity: §12.10.3 requires exact pre-MTR rollback for a known append failure and noncontinuable escalation for append uncertainty, failed restoration, or failed post-append publication. User-transaction abort behavior in §12.10.4 is distinct from this pre-publication storage rollback.

Chapter 8 does not duplicate that WAL payload format or barrier protocol; Chapter 12 owns them.

### 8.25.1 Page LSN and WAL-before-data participation

Every persistent B+ tree modification participates in page-LSN/WAL ordering, including:

- leaf insertion/deletion,
- internal insertion/deletion,
- split,
- merge,
- redistribution,
- sibling-link change,
- root change,
- free-list change,
- B+ tree superblock change.

BufferPool remains the data-page enforcement point:

```text
durable WAL >= page.page_lsn
before
dirty page reaches the data file
```

The B+ mini-transaction contract in §12.10.2 imposes a temporary no-flush barrier while one structural/mutation unit is being constructed.

## 8.26 Runtime structural publication

Concurrent runtime operations MUST NOT observe:

- a child pointer to an uninitialized page,
- a sibling pointer to an uninitialized page,
- a root pointer to an uninitialized page,
- a detached child before replacement routing is installed.

New pages are initialized before publication.

Routing pointers are published only while the required structural latches are held.

Runtime atomicity is provided by latch and publication ordering.

Provisional MTR bytes are visible only inside the exclusive §12.10.3 ownership domain. Root identity/height/generation, parent/sibling reachability, and free-list effects become ordinary-reader-visible only at the successful MTR publication point; known pre-append failure restores all of them before latches release.

Crash atomicity follows the `BTREE_MTR`/WAL publication rules in §§12.10.2–12.10.3 and §12.12 and the atomic redo/torn-page reconstruction rules in §§13.13.3–13.14.

## 8.27 Page validation and corruption handling

The complete mandatory B+ validation contract is §4.13.4–§4.13.5. Every ordinary node load validates checked geometry, nonoverlapping entry ownership, exact key/RID/child encoding, strict local physical-key order, pointer domains, and required-zero fields before RESIDENT publication. Sortedness and local overlap are production L1 checks, not debug/verifier options.

Reference targets receive the bounded traversal checks when followed. Invalid bytes, ordering, bounds, identity, level, or chain progress produce controlled `CORRUPT_INDEX`; the tree never continues through a known-malformed page merely because its checksum passed.

## 8.28 Full-tree verifier

The architecture requires the §4.13.5/§4.13.9 explicit full-tree verifier capable of checking at least:

1. every leaf is level `0`,
2. every internal child is exactly one level below its parent,
3. all leaves occur at the same depth,
4. leaf physical keys are sorted,
5. internal separators are sorted,
6. routing-lower-bound invariants hold,
7. global leaf-chain order matches tree order,
8. superblock `first_leaf` and `last_leaf` are correct,
9. every reachable child has the expected page type,
10. no reachable tree page is simultaneously on the free list,
11. the free list is acyclic and duplicate-free and every link is in range,
12. internal child counts are consistent with separator counts,
13. child graph acyclicity and unique live-page reachability,
14. all allocated/published pages are classified and unauthorized orphans are reported.

The verifier is a structural correctness facility, not merely a debugging convenience.

## 8.29 B+ tree invariants

1. An initialized B+ tree always has a valid root, including when empty.
2. Every leaf is level `0`.
3. Every internal child is exactly one level below its parent.
4. All physical entries are totally ordered by `(encoded_user_key, RID)`.
5. Leaf slot directories are sorted by physical key.
6. Internal separator slots are sorted.
7. Internal separators are valid lower routing bounds for their right children.
8. `N` internal separators imply `N+1` children.
9. Duplicate SQL user keys are legal at the physical-tree layer.
10. Duplicate stored copies of one exact physical `(encoded_user_key,RID)` are corruption; only proven same-operation replay is idempotent.
11. A B+ tree hit never decides MVCC visibility or SQL UNIQUE ownership.
12. Forward scans use safe latch-coupled leaf handoff.
13. Published child, sibling, and root pointers reference initialized pages.
14. A detached page is not reused while a legal traversal can still reference it.
15. Structural shape changes are separate from user-transaction visibility/abort semantics.
16. Every persistent B+ tree mutation participates in page-LSN/WAL ordering.
17. Variable-length split/rebalance decisions are byte based.
18. Encoded user keys larger than 1024 bytes are rejected rather than truncated.
19. B+ tree pages are accessed through BufferPool, never directly through DiskManager.
20. Persistent B+ page parsing validates bounds before dereferencing offsets/lengths.
21. Persistent B+ page parsing validates pairwise entry non-overlap and strict local key order before ordinary use.
22. Every free-list PageNo is an in-range ordinary B+ page distinct from root/endpoints; the list is acyclic, duplicate-free, and disjoint from the live tree.
23. Ordinary traversal and allocation use the bounded progress/recheck rules of §4.13.4–§4.13.5; the complete tree can be checked by the explicit verifier.
24. Native range traversal is forward/ascending.
25. Root acquisition never waits for a B+ page latch while holding the root-metadata latch; root identity is validated using the process-local `root_generation`.
26. Structural metadata publication may acquire the root-metadata latch while holding the required page latch(es).
27. Vertical page-latch acquisition is parent-before-child.
28. Horizontal adjacent-leaf latch acquisition is left-to-right or the operation restarts.
29. Tree-local page reuse occurs only after safe structural detachment.
30. Physical UNIQUE-index user-key duplicates remain representable because uniqueness is transactionally enforced above the tree.
31. A failed pre-append MTR restores exact page/root/free-list runtime state; a new/reused page cannot escape as both reachable and free or as an unpublished child/root.
