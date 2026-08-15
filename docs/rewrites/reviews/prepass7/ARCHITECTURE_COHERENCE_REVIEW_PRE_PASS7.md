# Architecture Coherence Review — Pre-Pass-7

## Review identity

Reviewed document:

```text
ARCHITECTURE_NEW.md
SHA-256: 7d26766b49044d19168d5ca6eb84b14997bbdcaff6ba7fb03ea23d5e27a50d10
```

Review point:

```text
Passes completed: 0..6
legacy coverage migrated: §§0..179
Pass 7: NOT performed
```

This review is architecture-to-architecture only.

It checks whether the rewritten architecture produced so far is internally correct and coherent as one technical contract. It is separate from the earlier implementation/devlog consistency review.

## Scope

The review covered:

- Chapters 1–8,
- Pass-1 baseline interfaces in Chapters 9–15 where they interact with migrated storage/index rules,
- Chapters 39–42 where migrated storage/index verification/performance requirements were added,
- Appendices A–D,
- the rewrite issue register,
- the legacy source only when necessary to distinguish a rewrite defect from an inherited architecture gap.

The review did **not**:

- modify `ARCHITECTURE_NEW.md`,
- modify legacy `ARCHITECTURE.md`,
- modify production code,
- perform Pass 7,
- silently choose missing persistent-format or concurrency rules.

## Method

The review used five checks.

### A. Cross-chapter semantic consistency

Repeated concepts were compared across their owning and summary chapters:

```text
PageId / RID
FileSuperblock
page headers
heap tuple/version semantics
FSM
BufferPool ownership
WAL-before-data
B+ physical keys
MVCC visibility boundary
vacuum/reclamation
```

### B. Persistent-layout arithmetic

The documented layouts were mechanically checked:

```text
common page header              32 bytes
FileSuperblock logical header   72 bytes
heap page total header          48 bytes
heap slot                        8 bytes
tuple header                    48 bytes
VARCHAR descriptor               8 bytes
FSM_DATA specific header        16 bytes
FSM_DATA complete page        8192 bytes
persisted index RID             16 bytes
B+ slot                          8 bytes
B+ leaf total header            64 bytes
B+ internal total header        64 bytes
```

All documented size arithmetic is internally correct.

### C. Formula/order sanity

The FSM category mapping and inverse representative boundaries were recomputed and match the document.

The documented INT32 sign-bit-flip + big-endian index transform was checked for monotonic ordering.

The documented binary VARCHAR escape/terminator encoding was exhaustively sanity-checked over small byte strings and preserves bytewise lexicographic order.

FLOAT64 index bytes remain intentionally unresolved under R-015 and were not guessed.

### D. Cross-reference integrity

All current-document section references resolve.

The only unresolved-looking `§...` references are intentional legacy-section references in rewrite-status/source-history wording.

No duplicate current numbered heading was found.

### E. Required-vs-deferred consistency

The review checked that:

- immediate DEAD-slot reuse is not accidentally authorized,
- B+ tree reuse is object-specific rather than a general extent allocator,
- index hits do not bypass MVCC,
- page latches and transaction locks remain distinct,
- NO-FORCE and STEAL remain compatible with BufferPool/WAL boundaries,
- the later no-physical-user-DML-undo refinement is consistently reflected in the current B+ and recovery baselines.

No contradiction was found in these areas.

# Overall verdict

The rewrite is **structurally strong but not yet fully architecture-coherent**.

The core algorithms and persisted layouts already migrated are mostly consistent.

The review found:

```text
1 direct persistent-format contradiction
3 high-priority unresolved technical-contract gaps
2 cross-chapter synchronization issues
existing known checksum/FLOAT64/B+ format gaps remain
```

The most important new finding is the FileSuperblock/B+ superblock contradiction.

# Findings

## C-001 — BLOCKER: generic FileSuperblock zero-reserved suffix conflicts with B+ superblock metadata

Chapter 4 defines a single v1 FileSuperblock layout with:

```text
offset 72..8191 = reserved, all zero
```

and requires a v1 decoder to reject any nonzero byte there.

Chapter 8 simultaneously requires page 0 of every B+ index file to be a B+ tree superblock storing:

```text
IndexId
TableId
root_page_no
first_leaf_page_no
last_leaf_page_no
tree_height
index_flags
key_schema_version
key_schema_fingerprint
free_page_head
```

Only `IndexId` can plausibly be satisfied by the existing base `object_id` field.

The other required B+ metadata has no legal persisted location under the current base-v1 rule.

This is stronger than “the B+ offsets are not specified.”

As the document is currently written:

```text
Chapter 4:
    bytes 72..8191 MUST be zero

Chapter 8:
    B+ page-0 MUST store additional persistent metadata

=> both cannot be satisfied simultaneously
```

The architecture must explicitly define a BTREE-superblock specialization/versioning mechanism.

Possible design families exist, but this review deliberately does not select one.

**Issue-register action:** R-014 expanded to record the direct conflict.

**Severity:** BLOCKING before final persistent-format cutover; preferably resolve before B+ implementation.

---

## C-002 — HIGH: root-metadata latch has no ordering rule relative to page latches

Chapter 8 defines:

```text
root-metadata latch
page latches
parent-before-child order
left-to-right leaf order
```

but no ordering/restart protocol between the metadata latch and page latches.

A potential cycle exists:

```text
T1:
    hold page/root-leaf write latch
    need root-metadata latch for root replacement

T2:
    hold root-metadata latch
    wait while obtaining/latching current root

T1 waits for T2
T2 waits for T1
```

The single-leaf-root split case makes the risk concrete.

The architecture needs a rule for:

- root acquisition,
- root replacement,
- height update,
- first/last leaf update,
- free-list-head update,
- whether waiting is permitted or restart is required.

**Issue-register action:** R-020 added.

**Severity:** HIGH concurrency correctness issue before B+ implementation.

---

## C-003 — HIGH: tuple fixed-area layout is not byte-exact

Chapter 5 correctly defines:

```text
header
bitmap
fixed area
varlen payload
```

and defines every scalar width plus the VARCHAR descriptor.

However, `MinimumTupleSize()` and `VarlenPayloadOffset()` are used without defining the exact physical-layout derivation.

The architecture currently does not explicitly state one of the following equivalent kinds of contract:

```text
fixed_area_offset = 48 + null_bitmap_bytes

physical columns packed in schema order
with exact widths
with no padding/gaps

VarlenPayloadOffset =
    fixed_area_offset + sum(fixed widths/descriptors)

MinimumTupleSize =
    VarlenPayloadOffset
```

nor does it state that these offsets are separately persisted in catalog metadata.

For a persistent tuple format, this is not just an API detail: two independent implementations need to derive the same byte offsets.

**Issue-register action:** R-018 added.

**Severity:** HIGH persistent-format precision issue.

---

## C-004 — HIGH/MEDIUM: ordinary-page common `flags` semantics are incomplete

Chapter 4 says the page-specific format completes the common-header `flags` contract.

HEAP_DATA and FSM_DATA define:

```text
format_version
header_size
reserved-zero rules
```

but do not define:

```text
valid flag bits
unknown-bit handling
zero requirement
preserve/ignore behavior
format-version interaction
```

FSM initialization explicitly permits a nonzero caller-supplied value without defining its v1 meaning.

This is inconsistent with the document's own claim that persistent-format field semantics are completed by page type.

**Issue-register action:** R-017 added.

**Severity:** HIGH/MEDIUM persistent compatibility/corruption-validation gap.

---

## C-005 — HIGH/MEDIUM: R-014 needed broader B+ byte-format completion

The existing R-014 was correct, but the cross-coherence review found additional B+ format details that belong to the same issue.

The unresolved B+ contract also includes:

- conflict with the base FileSuperblock trailing-zero rule,
- common-header flag semantics,
- end-of-chain sentinel for `prev_leaf_page_no`,
- end-of-chain sentinel for `next_leaf_page_no`,
- empty `free_page_head` sentinel,
- terminal `next_free_page_no` sentinel,
- blank leaf/internal `lower` / `upper` initialization,
- exact `lower = 64 + slot_count * 8` geometry rule,
- exact leaf entry-length validation (`user_key_length + 16`),
- exact internal entry-length validation (`user_key_length + 24`),
- persisted RID validity/sentinel rules and indexed-relation ownership checks where applicable.

The current node layouts themselves are arithmetically correct; this finding is about completing the parser/compatibility contract around them.

**Issue-register action:** R-014 expanded.

**Severity:** HIGH/MEDIUM before declaring B+ persistent v1 format complete.

---

## C-006 — MEDIUM: Chapter 4 wording is narrower than canonical Chapter 8 physical-key semantics

Chapter 4 says:

```text
For a non-unique index, physical ordering includes RID
```

Chapter 8 correctly establishes:

```text
every physical B+ key = (user key, RID)
```

including SQL UNIQUE indexes.

The later rule is necessary because UNIQUE indexes may physically contain multiple versions/aborted entries with the same user key.

This is not a design decision anymore; Chapter 4 should simply be synchronized to the canonical Chapter 8 rule.

**Issue-register action:** R-019 added.

**Severity:** MEDIUM editorial/semantic synchronization.

---

## C-007 — MEDIUM: whole heap-page reuse must be explicitly gated by RID-reuse safety

Chapter 4 lists completely empty heap pages as future page-reuse candidates.

Chapter 6 says an empty heap page remains reusable database space, while separately saying physical slot reuse requires later safe RID reuse.

Reinitializing the same heap PageNo can implicitly reuse many `(PageNo,SlotId)` identities, so page reuse cannot bypass the physical RID grace-period protocol merely because the whole page is empty.

The later legacy §§260–264 already resolve the architectural principle:

```text
index cleanup
NORMAL -> DEAD
read-epoch grace period
DEAD -> UNUSED/reusable
```

Therefore this is a synchronization requirement for Pass 9, not a new architecture choice.

**Issue-register action:** R-021 added.

**Severity:** MEDIUM now; correctness-critical when reclamation is canonicalized.

# Existing issues confirmed

## R-010 — ordinary-page checksum coverage

Still real.

The superblock checksum is exact, but a universal byte-exact ordinary-page checksum coverage rule remains unspecified.

No new contradiction was found beyond that known gap.

## R-015 — FLOAT64 index bytes

Still real.

The semantic ordering contract is coherent, but exact canonical NaN bits and exact sortable transform remain unspecified.

## R-001 — RID reserved bytes

Not an architecture-to-architecture contradiction.

The rewritten architecture itself is coherent:

```text
write zero
reject nonzero
```

R-001 remains an implementation synchronization issue only.

## R-016 — no physical user-DML undo refinement

The current rewritten chapters are coherent on this point.

Heap-version visibility, B+ garbage retention, STEAL/NO-FORCE, and the recovery baseline no longer require ordinary physical user-DML undo.

# Areas that passed without new coherence findings

## Identifiers and page identity

No conflict among:

```text
FileId
PageNo
SlotId
TxnId
CommandId
Lsn
TableId
IndexId
SchemaVer
PageId
RID
```

or their currently defined sentinels.

## Heap page and tuple state

The following rules agree across Chapters 4–6 and the invariants:

- 8192-byte pages,
- 48-byte HEAP_DATA header,
- 8-byte slot entries,
- stable SlotId,
- NORMAL/DEAD behavior,
- post-compaction DEAD `(0,0)` coordinates,
- no immediate DEAD reuse,
- 8135-byte maximum raw tuple,
- 48-byte tuple header,
- previous-version same-heap semantics,
- exact tuple length,
- null-bitmap rules,
- fixed scalar representations,
- VARCHAR descriptor/payload rules.

## FSM

The category formula, inverse lower bound, representative values, page geometry, deterministic mapping, initialized-prefix semantics, and stale/advisory behavior are mutually consistent.

## Disk/Buffer ownership

No ownership contradiction was found among:

```text
DiskManager
page-file layer
BufferPool
HeapPage
TupleCodec
B+ tree
```

The central WAL-before-data enforcement point is consistently BufferPool for BufferPool-managed pages.

## B+ ordering/search/split semantics

No logical inconsistency was found in:

- memcomparable INT/BOOLEAN/VARCHAR rules,
- composite concatenation,
- physical `(user_key,RID)` ordering,
- `upper_bound` internal routing,
- `lower_bound` leaf search,
- stale-low separator semantics,
- byte-balanced splits,
- redistribution/merge semantics,
- forward leaf handoff,
- left-to-right horizontal latch ordering,
- duplicate/UNIQUE/MVCC separation.

# Rewrite-process material still present

The current in-progress file still contains statements such as:

```text
Legacy §§...
not invented in this pass
Rewrite status: ...
```

These are not AI instructions, but they are rewrite-process metadata rather than final technical architecture.

They are acceptable while the rewrite is in progress.

Before final cutover they should be removed or converted into pure technical wording after the associated gaps are resolved.

This is an editorial finalization item, not a correctness defect.

# Gate decision before Pass 7

**Pass 7 was not performed in this task.**

The document is coherent enough that Pass 7 could technically be migrated while the issue register carries the unresolved storage/index questions.

However, because C-001 is a real direct contradiction and C-002 is a real concurrency-design hole, the precision-first workflow should preferably insert one small architecture-resolution task before Pass 7.

Recommended next task:

> **Pre-Pass-7 Coherence Resolution — resolve only the architecture-internal findings from `ARCHITECTURE_COHERENCE_REVIEW_PRE_PASS7.md`. First address the FileSuperblock/B+ superblock conflict, ordinary-page common flags, tuple fixed-area byte derivation, B+ persistent sentinel/geometry details, root-metadata/page-latch ordering, the Chapter 4 physical-key wording, and the heap-page-reuse/RID-reuse cross-reference. Use the legacy architecture and existing rewrite issue register as constraints; do not perform Pass 7, do not modify production code, and do not guess. Where a genuine design choice is required, present the alternatives and stop for my decision rather than silently selecting one.**

After that resolution task, Pass 7 can proceed on a cleaner contract.
