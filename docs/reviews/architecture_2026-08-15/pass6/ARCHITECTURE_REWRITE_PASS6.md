# Rewrite Pass 6 — B+ Tree Indexing

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `109..179`
- processed source lines: `4202..6167`
- legacy architecture modified: **no**
- production code modified: **no**
- transaction architecture §180+ migrated: **no**

Pass 6 replaces the Pass-1 B+ tree overview with the canonical indexing contract.

## Canonicalized areas

Chapter 8 now defines:

- page-backed BufferPool-owned B+ storage,
- one B+ file per index and valid empty-tree representation,
- fixed key-schema ordering constraints,
- physical `(user_key,RID)` ordering,
- exact 16-byte persisted index RID encoding,
- memcomparable field/composite encoding,
- 1024-byte encoded-user-key maximum,
- slotted 64-byte-header node organization,
- exact 8-byte slot descriptors,
- leaf/internal headers and entry composition,
- routing-lower-bound separators,
- binary internal/leaf search,
- compact-before-split behavior,
- byte-balanced leaf/internal splits,
- persistent sibling links,
- root split/contraction,
- ~25% soft byte-occupancy rebalance threshold,
- redistribution/merge policy,
- tree-local free-page reuse and safe detachment,
- read latch coupling and write latch crabbing,
- root metadata synchronization,
- parent-before-child and left-to-right latch ordering,
- guard-backed forward range scans,
- duplicate physical keys and transactional uniqueness boundary,
- heap-owned MVCC visibility,
- UPDATE/DELETE/vacuum interaction,
- BufferPool integration and index-scan locality cost,
- structural publication and WAL/page-LSN boundaries,
- page validation,
- full-tree verification,
- consolidated B+ invariants.

## Strict RID rule

The v1 persisted RID rule is now canonical in §8.4.1:

```text
16 bytes total
reserved bytes 14..15 = 0
encoder writes zero
decoder rejects nonzero
```

R-001 remains open only as an implementation/PROJECT_STATE mismatch.

## Key-format preservation

The exact source-defined key rules are retained:

```text
NULL marker      0x00
non-NULL marker  0x01

INT32/DATE:
    sign-bit flip
    big-endian

INT64/TIMESTAMP:
    sign-bit flip
    big-endian

VARCHAR:
    0x00 payload byte -> 0x00 0xFF
    terminator         -> 0x00 0x00
```

Composite components concatenate in schema order.

The B+ hot comparison path is encoded-byte lexicographic comparison plus numeric RID tie-breaking.

## Node and structural semantics

Leaf/internal nodes use 64-byte total headers and 8-byte slot descriptors.

Leaf entries store:

```text
encoded user key
16-byte RID
```

Internal entries store:

```text
encoded user key
16-byte separator RID
8-byte right child PageNo
```

Internal separators are routing lower bounds and may remain stale-low after simple deletion. They must be updated when keys move across routing boundaries.

All split/underflow decisions are byte based rather than entry-count based.

The soft non-root rebalance threshold remains approximately 25% byte occupancy.

## Concurrency

The canonical initial write/read strategy remains:

```text
read:
    parent -> pin/latch child -> release parent

write:
    top-down write latch crabbing
    release ancestors once child is safe
```

Vertical order is parent-before-child.

Adjacent leaves are acquired left-to-right; operations restart instead of blocking in reverse order.

Forward leaf handoff keeps the current leaf protected until the next leaf is pinned/read-latched and validated.

## Unique indexes and MVCC

The physical tree always permits duplicate SQL user keys.

UNIQUE is transactionally enforced above the tree through logical key protection plus physical duplicate scan and heap/MVCC state inspection.

Any NULL component skips duplicate rejection in v1.

An index hit never establishes visibility.

Index-only scans remain deferred.

UPDATE creates a new `(key,new_RID)` entry even when indexed values are unchanged; DELETE leaves physical entries until vacuum.

## Later no-undo refinement

Pass 6 performed a targeted later-source consistency check without migrating transaction architecture.

Legacy §161's suggestion that a logical inserted B+ entry may be undone is superseded by §181 and §§225–228:

```text
aborted user DML:
    heap version becomes/remains logically invisible/live according to status
    physical index garbage may remain
    vacuum removes it later

B+ structural shape:
    remains valid
```

This is recorded as R-016.

## Persistent-format gaps recorded instead of guessed

### R-014 — B+ metadata/page-format completion

The legacy source does not fully specify specialized superblock offsets/widths, node format-version/header-size rules, node/slot flag semantics, reserved-byte policies, BTREE_FREE layout, key-schema fingerprint algorithm, or index_flags semantics.

Chapter 8 preserves the existing exact layout but does not fabricate the missing pieces.

### R-015 — FLOAT64 memcomparable encoding

The source locks zero/NaN canonical semantics and total ordering, but does not give the canonical NaN bit pattern or exact sortable-bit transform.

Because this affects persistent index bytes, Pass 6 leaves it explicitly unresolved.

## Verification/performance separation

Legacy §§167–169 implementation milestones were moved out of architecture.

Legacy §§170–173 were reduced to architecture-level B+ verification obligations in §41.2.

Legacy §174 benchmark dimensions were retained in §42.1 without milestone/tuning workflow language.

Legacy §§175–176 deferred/future B+ features were consolidated into Appendix C.

§177 invariants were preserved in §8.29.

§178 decision-summary duplication was removed after checking each item against the canonical chapter.

§179 next-stage planning was excluded from the technical contract.

## Coverage result

```text
legacy §§0..179     migrated / explicitly disposed
legacy §§180..725   pending
```

All 71 Pass-6 sections are complete.

No §180+ transaction section was marked migrated.

## Validation

Mechanical validation confirms:

- pinned legacy SHA-256 unchanged,
- §109 begins at line 4202,
- §180 begins at line 6168,
- every source section 109..179 exists,
- all coverage rows through §179 are non-PENDING,
- all rows from §180 onward remain PENDING,
- Chapter 8 occurs exactly once,
- strict RID reserved-zero semantics are present,
- exact key markers/sign transforms/VARCHAR escaping are present,
- exact 64-byte node headers and 8-byte slots are present,
- binary search / split / merge / latching / range-scan / WAL boundaries are present,
- B+ milestones and next-stage roadmap are absent from Chapter 8,
- legacy `ARCHITECTURE.md` was not modified,
- production code was not modified.

## Pass 6 exit status

**COMPLETE.**

Pass 7 should process only legacy §§180–214 and canonicalize transaction lifecycle, transaction IDs, snapshots, MVCC visibility, logical locks, write conflicts, and unique-key locking without migrating the WAL/recovery body from §215 onward.
