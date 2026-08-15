# Architecture Rewrite Issue Register

Source architecture snapshot: `ARCHITECTURE(4).md`  
SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`

This register records inconsistencies, ambiguities, or semantic decisions discovered during the rewrite. It is not itself an architecture authority.

## R-001 — RID reserved-byte contract vs Phase 1 implementation checkpoint

**Type:** architecture / implementation consistency.

Current architecture §113 states for persisted RID bytes `14..15`:

- encoder MUST write zero;
- v1 decoder MUST reject nonzero reserved bytes.

The Phase 1 `0020` audit recorded that the implementation encoder writes zero but the decoder still accepts nonzero reserved bytes.

**Rewrite action:** preserve the current strict architecture contract in Chapter 8 unless the project owner explicitly changes it. Do not weaken it during restructuring.

**Implementation action:** deferred while code changes are intentionally paused. Before normal implementation resumes, reconcile the decoder and `PROJECT_STATE.md` with the accepted architecture.

## R-002 — Early generic recovery overview is superseded/refined by the concrete recovery contract

Old §24 states an ARIES-inspired `analysis / redo / undo` direction.

Old §181 explicitly refines this for v1 heap-version MVCC to:

```text
analysis
redo
loser-transaction resolution
```

with no physical undo of ordinary aborted user-DML heap/index modifications.

**Rewrite action:** the later concrete contract owns the normative v1 behavior. Preserve the earlier ARIES rationale only where it remains accurate; do not keep two apparently competing recovery algorithms.

**Pass 8 canonicalized resolution:** Chapters 10 and 13 now make the v1 phases explicit as analysis, redo, and loser-status resolution with no ordinary physical user-DML undo/CLR phase.

**Status:** RESOLVED refinement-chain issue.

## R-003 — Implementation roadmap content is mixed into the architecture contract

Old §§49, 103, 167–169, 296–299, 429–432, 561–566, and 719–724 describe implementation order or milestone targets.

**Rewrite action:** remove project sequencing from the final architecture contract. Preserve any architectural dependency constraints in their owning chapters. Proposed destination for the detailed implementation plan is a human-facing `DEVELOPMENT.md`.

## R-004 — Historical architecture-status snapshots are mixed into the contract

Old §§300, 433, 567, and 725 describe what had been specified at successive points in the design process and what should happen next.

**Rewrite action:** retain only unique end-to-end architecture diagrams or constraints. Current progress/status belongs in `PROJECT_STATE.md`; historical progression remains recoverable from the legacy contract/devlogs.

## R-005 — Detailed verification recipes and benchmark plans are mixed with architecture

The source contains subsystem-specific test checklists, fuzz plans, crash-test plans, and benchmark recipes.

**Rewrite action:** retain architecture-level verification/performance requirements in the new contract. Proposed destination for detailed procedures is a human-facing `VERIFICATION.md`.

## R-006 — Source/module layout guidance is presented as locked architecture

Old §§101, 295, 428, 560, and 718 provide recommended source trees while also stating that exact filenames may evolve.

**Rewrite action:** preserve subsystem dependency/ownership boundaries in architecture; move concrete source-tree guidance to `DEVELOPMENT.md` if retained.

## R-007 — AI/workflow wording is embedded in architecture text

The current front matter names Codex/AI as an audience, old §0 is explicitly agent workflow, and several invariant sections say “Codex must preserve”.

**Rewrite action:** replace these with ordinary technical contract wording. Agent workflow belongs in `AGENTS.md`. Architectural invariants themselves remain fully preserved.

## R-008 — Supporting-document preservation before final cutover

**Type:** rewrite/document-ownership issue, not a database architecture decision.

Pass 1 removes legacy §49 implementation sequencing from the new architecture contract and keeps only architecture-level portions of legacy §§45–46.

The old file remains the preservation source, but before final cutover the project should decide whether the detailed implementation roadmap and verification/benchmark procedures are retained in dedicated human documentation such as:

```text
DEVELOPMENT.md
VERIFICATION.md
```

**Rewrite action:** do not reinsert project sequencing or detailed test recipes into `ARCHITECTURE_NEW.md` merely to avoid losing them. Preserve them in the legacy source until an explicit documentation destination is accepted.

## R-009 — Early join/aggregation/sort sections mix architecture with implementation sequencing

**Type:** editorial classification / semantic-preservation note.

Legacy §§38–40 use phrases such as “implementation order”, “first”, and “later” while also defining actual algorithm choices and required future capabilities.

**Rewrite action:** Pass 1 preserves the algorithm family, primary/baseline role, and staged capability constraints in Chapters 28–30, while removing development-agent wording. Later execution passes must compare the concrete contracts against these migrated baselines before canonicalizing them.

## R-010 — Exact checksum coverage for ordinary random-access pages is not globally specified

**Type:** persistent-format contract gap.

Legacy §59 fully specifies the v1 superblock checksum:

```text
CRC32C over exactly bytes 0..8191
with bytes 16..19 logically zero
```

Legacy §63 specifies for persistent page checksums:

- CRC32C,
- the checksum field is treated as zero,
- checksums may be optional before WAL/recovery integration,
- checksums should be enabled by default once recovery exists,
- checksums detect corruption/torn writes but do not repair torn pages.

However, §63 does not explicitly state one exact checksum byte range for every non-superblock random-access page type.

Legacy §82.6 additionally states that, before WAL/recovery integration, ordinary FSM_DATA mutation does not generate/update a “whole-page CRC32C checksum”, and that later global checksum/WAL policies apply. This is evidence that FSM page checksums are intended to be whole-page checksums, but it still does not explicitly establish one universal exact byte-range rule for every ordinary page type.

**Rewrite action:** Chapters 4 and 6 preserve the exact algorithm, staging, and FSM “whole-page” wording that are actually specified and do not invent a universal coverage range. The owning later page/recovery passes should preserve any additional page-type-specific rules they contain.

**Resolution required before final cutover:** determine whether the v1 global contract is intended to be “CRC32C over the complete 8192-byte page with bytes 16..19 zeroed” for every checksummed random-access page, or whether checksum coverage is page-type-specific.

**Resolution:** All checksummed 8192-byte random-access pages use CRC32C over exactly bytes `0..8191` with bytes `16..19` logically zero. Superblock checking is mandatory from creation; ordinary page checking retains the existing staged enablement policy.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-011 — Heap INSERT wording references slot reuse before reuse eligibility is defined

**Type:** resolved editorial/refinement-chain note.

Legacy §78 describes an INSERT step as:

```text
allocate/reuse slot
```

while legacy §§66–67 explicitly state that:

- nonempty `free_slot_head` linkage remains deferred until slot reuse is implemented,
- a compacted `DEAD` slot remains `DEAD`,
- physical reclamation does not by itself make the slot `UNUSED` or reusable.

Later architecture defines separate physical RID-reuse safety.

**Rewrite action:** Chapter 5 preserves the INSERT slot-allocation step but states that reuse is legal only when a separately defined safe-reuse protocol makes a slot eligible. No immediate `DEAD`-slot reuse rule is introduced.

**Status:** no architecture decision required for the rewrite; this is a clarification of the existing boundary.

## R-012 — Early DiskManager `SyncWal()` sketch is refined by later WAL ownership

**Type:** resolved architecture refinement-chain note.

Legacy §86 shows a conceptual `DiskManager` interface containing:

```text
SyncWal()
```

while also stating that the exact API may differ.

Later locked WAL architecture (§231) assigns a dedicated WAL writer/flusher responsibility for:

- writing WAL bytes to segment files,
- calling `fdatasync` when durability is requested,
- advancing `durable_lsn`,
- waking commit waiters.

**Rewrite action:** Chapter 7 preserves DiskManager as the raw file-I/O layer but does not make it the owner of transaction-level WAL durability coordination. The later WAL subsystem owns WAL scheduling/durable-LSN semantics. A raw I/O primitive may still be used underneath that subsystem.

**Status:** no architecture decision required; the later concrete WAL contract refines the earlier conceptual method sketch.


## R-013 — Legacy Storage Milestone 1 test list contains premature “reusable slots”

**Type:** stale verification-roadmap item.

Legacy §104 lists “reusable slots” under the old Storage Milestone 1 slotted-page tests.

However, legacy §§66–67 and the later vacuum/RID-reuse architecture state that:

- nonempty heap free-slot linkage is deferred,
- compacted `DEAD` slots remain `DEAD`,
- physical reclamation does not itself authorize immediate slot/RID reuse,
- later safe-reuse gating controls `DEAD -> UNUSED`.

**Rewrite action:** the detailed milestone checklist is not part of `ARCHITECTURE_NEW.md`. Chapter 41 requires slot-reuse verification only once the safe RID-reuse protocol makes reuse an implemented feature.

**Status:** no architecture decision required; the milestone recipe is historical/stale relative to the later locked storage/reclamation contract.

## R-014 — B+ tree persistent metadata/page formats are not fully byte-specified

**Type:** persistent-format contract gap.

Legacy §§110, 119–124, 138, and 165 lock substantial B+ persistent structure, including:

- one page-zero B+ superblock with a required metadata set,
- 64-byte leaf/internal total headers,
- exact leaf/internal header field offsets,
- 8-byte node slot descriptors,
- leaf/internal entry composition,
- `BTREE_FREE` pages with `next_free_page_no`,
- validation of page type, format version, header size, and level/type consistency.

However, the legacy/current rewritten contract does not fully define:

- byte offsets and widths for the B+ superblock-specific metadata fields,
- whether/how the base FileSuperblock's `object_id` relates to the separately listed `IndexId`,
- how B+ superblock-specific metadata can coexist with the base v1 FileSuperblock rule that bytes `72..8191` are reserved and must all be zero,
- whether a specialized BTREE superblock retains base `header_size = 72`, uses a different header size/version, or uses another explicitly versioned specialization mechanism,
- exact B+ node `format_version`,
- explicit common-header `header_size` requirement for B+ nodes,
- common-header `flags` semantics for B+ node/free pages,
- persisted bit meanings/valid masks for node-header `flags`,
- persisted bit meanings/valid masks for node-slot `flags`,
- zero/compatibility semantics for leaf/internal reserved bytes,
- exact blank-node geometry/initialization rules such as the persisted `lower`/`upper` state,
- whether node validation must require `lower = 64 + slot_count * 8`,
- exact format-specific entry-length validation (`user_key_length + 16` for leaves and `user_key_length + 24` for internal entries),
- terminal/sentinel conventions for `prev_leaf_page_no`, `next_leaf_page_no`, `free_page_head`, and `next_free_page_no`,
- exact `BTREE_FREE` payload offsets/widths/version/header rules for `next_free_page_no`,
- validity rules for persisted leaf RIDs, including sentinel rejection and relation/file ownership checks where applicable,
- the algorithm/semantic contract for `key_schema_fingerprint`,
- bit semantics for `index_flags`.

**Rewrite action:** Chapter 8 preserves every byte offset and field that is actually specified, and explicitly leaves missing values/semantics unresolved rather than inventing them. The pre-Pass-7 coherence-resolution edit additionally canonicalized the directly implied node geometry and entry-length checks:

```text
64 <= lower <= upper <= PAGE_SIZE
lower = 64 + slot_count * 8
leaf entry_length = user_key_length + 16
internal entry_length = user_key_length + 24
```

The remaining R-014 items still require explicit design decisions.

**Resolution required before final cutover:** define a complete v1 B+ persistent-format contract for the specialized superblock, leaf/internal common-header requirements, flags/reserved fields, free pages, and schema fingerprint.

**Resolution:** BTREE uses a 128-byte file-kind-specific superblock header with the common FileSuperblock prefix at `0..71` and the exact extension at `72..127`; node/common versions, zero-only flags/reserved fields, node geometry, entry-length checks, sentinels, RID validity, free-page layout, key-schema fingerprint, and zero-only `index_flags` are now byte-exact in Chapter 8.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-015 — FLOAT64 memcomparable index encoding is not byte-exact

**Type:** persistent key-format contract gap.

Legacy §115 locks FLOAT64 index semantics:

- canonicalize `-0.0` and `+0.0` to one value,
- canonicalize all NaNs to one representation,
- total order places NaN after `+infinity`,
- apply a “standard sortable IEEE-754 bit transform”,
- encode the transformed value big-endian,
- SQL FLOAT64 comparison must use matching equality/order semantics.

The source does not specify:

- the exact canonical NaN 64-bit pattern,
- the exact sortable-bit transform formula for sign/negative values.

Because encoded index keys are persistent bytes, these details cannot safely be inferred from an implementation convention.

**Rewrite action:** Chapter 8 preserves the locked semantic ordering and the source's transform requirement but does not invent the missing bit-exact formula.

**Resolution required before final cutover:** lock the exact canonical FLOAT64 bit normalization and sortable transform.

**Resolution:** FLOAT64 index encoding now canonicalizes both zeros to `0x0000000000000000`, all NaNs to `0x7ff8000000000000`, transforms negative normalized bits with 64-bit bitwise NOT and nonnegative bits with XOR `0x8000000000000000`, then writes the sortable uint64 big-endian.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-016 — Early B+ user-abort wording is superseded by no-physical-user-DML-undo

**Type:** resolved architecture refinement-chain note.

Legacy §161 says that if a user transaction aborts:

```text
logical inserted entry may be undone
```

while structural split shape need not be undone.

Later locked transaction/recovery architecture explicitly refines v1 rollback:

- §181: ordinary aborted heap/index user-DML modifications are not physically undone;
- aborted INSERT/UPDATE index entries may remain and reference invisible aborted heap versions;
- vacuum removes those entries later;
- §§225–228: physical B+ mutations use recovery-safe mini-transactions and B+ changes may survive user abort.

**Rewrite action:** Chapter 8 preserves the important separation between user-transaction semantics and structural tree shape, but does not retain physical logical-entry undo as normative v1 behavior.

**Status:** no architecture decision required; the later concrete transaction/recovery contract supersedes the earlier wording.

## R-017 — Ordinary-page common `flags` semantics are not completed

**Type:** persistent-format contract gap discovered by cross-chapter coherence review.

Chapter 4 states that the meaning of common-header `format_version`, `header_size`, `flags`, and reserved-field restrictions is completed by the specific page format.

The rewritten HEAP_DATA and FSM_DATA contracts define their format versions, header sizes, and reserved-zero rules, but they do not define the validity/compatibility contract for the common-header `flags` field.

FSM initialization currently even permits:

```text
flags = 0 unless explicitly supplied
```

without defining which nonzero bits are valid, invalid, preserved, or version-gated.

This leaves persistent compatibility and corruption validation ambiguous for ordinary HEAP_DATA/FSM_DATA pages.

**Rewrite action:** do not infer a policy from implementation. Before final cutover, explicitly choose one coherent v1 rule for common-header flags on each ordinary page type (or one generic rule inherited by page types), including encode/decode behavior for unknown bits.

**Status:** unresolved architecture-format question.

**Resolution:** For ordinary v1 page formats with no assigned common-header flag bits, `flags=0`; encoders write zero and decoders reject nonzero. HEAP_DATA, FSM_DATA, and current B+ page formats inherit/use this rule.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-018 — Tuple fixed-area layout is not byte-exact enough for a persistent-format contract

**Type:** persistent tuple-format contract gap discovered by cross-chapter coherence review.

Chapter 5 defines the tuple order as:

```text
48-byte header
null bitmap
fixed layout area
variable payload
```

and defines scalar widths plus the 8-byte VARCHAR descriptor. It also uses conceptual values such as:

```text
MinimumTupleSize()
VarlenPayloadOffset()
```

However, the rewritten contract does not explicitly define the byte-exact derivation of the fixed area, including:

- fixed-area start offset,
- physical-column packing order,
- whether every fixed-width value / VARCHAR descriptor is packed consecutively with no gaps,
- exact per-column fixed-offset derivation,
- exact `VarlenPayloadOffset()` derivation,
- exact `MinimumTupleSize()` derivation.

Without one of these rules—or an explicit alternative stating that the catalog persists physical offsets—the same schema could be encoded incompatibly by two conforming implementations.

**Rewrite action:** preserve the current tuple body/width semantics, but do not invent the missing derivation. Before final cutover, lock the physical-layout derivation or explicitly assign persisted physical offsets to another authoritative format.

**Status:** unresolved persistent-format question.

**Resolution:** Tuple v1 uses compact schema-order fixed packing with no padding: fixed area starts at `48 + null_bitmap_bytes`; fixed offsets are prefix sums of physical widths; VARCHAR contributes its 8-byte descriptor; `VarlenPayloadOffset()` and `MinimumTupleSize()` are the final fixed cursor.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-019 — Chapter 4 narrows RID physical ordering to non-unique indexes, while Chapter 8 makes it universal

**Type:** resolved cross-chapter wording/refinement issue.

Chapter 4 currently says:

```text
For a non-unique index, physical ordering includes RID:
    (index key, RID)
```

Chapter 8's canonical B+ contract later establishes that **every** physical B+ tree key is:

```text
(user key, RID)
```

including SQL UNIQUE indexes, because physical duplicates can coexist and uniqueness is enforced transactionally above the tree.

The later detailed rule is the coherent v1 architecture.

**Rewrite action:** Chapter 4 has been synchronized to state that every B+ tree index, including SQL UNIQUE indexes, uses physical `(user_key,RID)` ordering.

**Status:** RESOLVED by the pre-Pass-7 coherence-resolution edit. No architecture decision was required.

## R-020 — Root-metadata latch ordering relative to page latches is unspecified

**Type:** B+ tree concurrency-contract gap discovered by cross-chapter coherence review.

Chapter 8 defines:

- a root-metadata latch protecting root replacement, height, first/last leaf metadata, and free-list-head updates,
- normal traversal holding that latch long enough to safely obtain/pin the root,
- page-latch ordering as parent-before-child vertically and left-to-right horizontally.

It does **not** define the acquisition/restart rule between the root-metadata latch and B+ page latches.

This can create an architectural deadlock possibility if one path:

```text
root-metadata latch -> waits for root/page latch
```

while a structural path already holding that page latch later needs:

```text
page latch -> root-metadata latch
```

The single-leaf-root split case is a concrete example of the potential cycle.

**Rewrite action:** before B+ implementation, define one deadlock-free root-metadata/page-latch protocol, including structural updates to root/height/first/last/free-list metadata. Do not guess the ordering during the rewrite.

**Status:** unresolved concurrency-design question.

**Resolution:** The architecture uses optimistic root identity validation with a process-local monotonic `root_generation`. Root metadata is released before waiting for the root page latch; the latched candidate is then validated against root identity/generation. Structural metadata publication may acquire metadata while holding required page latches; free-list/leaf-endpoint updates use analogous snapshot/latch/validate/restart protocols.

**Status:** RESOLVED by explicit owner-delegated pre-Pass-7 architecture decision.
## R-021 — Whole heap-page recycling must obey physical RID reuse safety

**Type:** later-refinement synchronization issue.

Chapter 4 names completely empty heap pages as candidates for object-specific page recycling, and Chapter 6 says an empty heap page remains reusable database space.

Reinitializing/recycling the same heap `PageNo` can implicitly reuse many `(PageNo, SlotId)` physical RIDs, even if the page-local operation is described as page reuse rather than slot reuse.

The later locked RID-reclamation architecture (§§260–264 in the legacy source) already establishes that a physical RID cannot be reused until:

```text
index cleanup
DEAD retirement
read-epoch grace period
```

have made reuse safe.

**Rewrite action:** Chapters 4 and 6 now explicitly state that whole heap-page recycling/reinitialization cannot bypass the later physical RID-reuse safety gate. Pass 9 must still migrate the complete reclamation/read-epoch protocol and become the canonical owner of that gate.

**Status:** PARTIALLY RESOLVED / SYNCHRONIZED. No new architecture decision is required if Pass 9 preserves the later locked RID-reuse protocol.

**Pass 9 canonical resolution:** Chapter 14 now owns the complete two-phase physical RID-reclamation protocol. Whole heap-page recycling is explicitly subject to the same read-epoch reuse gate when it can recreate old `(PageNo,SlotId)` identities.

**Status:** RESOLVED by Pass 9.

## R-022 — Transaction-status persistent format is incomplete

**Type:** persistent-format contract gap discovered during Pass 7.

Legacy §§191–193 define:

```text
txn_status.dat
page 0 = superblock
page 1..N = TXN_STATUS pages
32-byte common page header
8160-byte payload
4 two-bit states per byte
32,640 TxnIds/page
persistent states:
    INVALID
    COMMITTED
    ABORTED
    RESERVED
```

and require a deterministic pure:

```text
TxnId -> page + byte offset + two-bit position
```

mapping.

The legacy architecture does **not** define:

- a persisted `FileKind` numeric code for `txn_status.dat`,
- a persisted `PageType` numeric code for transaction-status pages,
- exact two-bit numeric codes for `INVALID/COMMITTED/ABORTED/RESERVED`,
- the operational meaning/lifecycle of the persisted `RESERVED` state,
- the exact mapping arithmetic,
- whether reserved TxnIds `0` and `1` consume ordinary status positions,
- complete v1 common-header/reserved validation rules for status pages.

Later legacy sections use transaction-status pages but do not fill these byte-level gaps.

**Rewrite action:** Chapter 9 preserves the exact capacity and semantic status set but does not invent numeric codes or mapping arithmetic.

**Resolution required before transaction-status implementation / final cutover:** define the complete persistent `txn_status.dat` v1 format and mapping.

**Final resolution:** `txn_status.dat` now uses `FileKind::TXN_STATUS=5`, `PageType::TXN_STATUS=7`, `header_size=32`, zero-only unassigned common flags/reserved16, and 2-bit LSB-first codes `00 INVALID`, `01 COMMITTED`, `10 ABORTED`, `11 RESERVED`. Normal TxnId `2` maps to status entry zero; TxnIds `0` and `1` are special and consume no entries. The exact page/byte/bit mapping formula and page validation rules are canonical in §9.11–§9.12. `RESERVED` is a recognized nonterminal code but ordinary v1 BEGIN does not emit it; correctness relies on the active registry plus terminal COMMITTED/ABORTED publication.

**Status:** RESOLVED by owner-delegated learning-focused architecture decision.
## R-023 — `reserved_txn_id_end` boundary convention is unspecified

**Type:** durable transaction-identity contract gap discovered during Pass 7.

Legacy §183 requires durable TxnId reservation in blocks of:

```text
1,048,576
```

and persists:

```text
reserved_txn_id_end
```

before handing out IDs from a newly reserved range.

The later database-control-file contract preserves the same field but does not state whether it means:

```text
inclusive last reserved TxnId
```

or:

```text
exclusive first unreserved TxnId
```

The distinction affects exact allocator arithmetic, initial control metadata, reservation rollover, and exhaustion checks.

**Rewrite action:** Chapter 9 preserves the durable-before-use and never-reuse guarantees without choosing an inclusivity convention.

**Resolution required before TxnId allocator implementation / final cutover:** lock one exact interval convention and initial value.

**Final resolution:** `reserved_txn_id_end` is the exclusive first-unreserved TxnId. Durable reservation space is the half-open interval `[FIRST_NORMAL_TXN_ID, reserved_txn_id_end)`. A fresh database starts with `reserved_txn_id_end=FIRST_NORMAL_TXN_ID`; exhaustion reserves the next checked block of 1,048,576 IDs durably before any ID from `[old_end,new_end)` can be handed out.

**Status:** RESOLVED by owner-delegated learning-focused architecture decision.
## R-024 — Snapshot owner membership and exact `xmin` derivation are underspecified

**Type:** snapshot/vacuum-horizon semantic gap discovered during Pass 7.

Legacy §187 defines:

```text
Snapshot {
    xmin
    xmax
    sorted active
    owner_txn_id
    command_id
}
```

with:

```text
active = transactions active at capture with txn_id < xmax
xmin   = minimum transaction ID relevant to the active set
if none: xmin = xmax
```

The source does not state whether:

```text
owner_txn_id
```

is itself included in the persisted/runtime `active` vector used by the snapshot, or whether self is always excluded because visibility handles self separately.

That choice can change stored `xmin` and therefore the precision of the global vacuum horizon, even though tuple self-visibility remains correct either way.

Later vacuum material defines the global horizon as the minimum registered `snapshot.xmin` but does not resolve this owner-membership convention.

**Rewrite action:** Chapter 9 preserves exact self-visibility and snapshot high-water semantics, but does not choose the owner-membership/xmin convention.

**Resolution required before SnapshotManager/vacuum implementation / final cutover:** define the exact `active` membership rule and deterministic `xmin` calculation.

**Final resolution:** `snapshot.active` contains only other nonterminal normal transactions below `xmax`; `owner_txn_id` is excluded because self visibility is handled by owner/cmin/cmax rules. The exact horizon is `xmin=min(active)` when nonempty, otherwise `xmin=xmax`. This avoids pinning the vacuum horizon merely because an old READ COMMITTED transaction exists.

**Status:** RESOLVED by owner-delegated learning-focused architecture decision.

## R-025 — WAL record and payload byte encoding is not complete

**Type:** persistent WAL-format contract gap discovered during Pass 8.

Legacy §§216–226 lock the 8-byte alignment, segment-containment rule, 48-byte header, semantic CRC coverage, record families, and semantic redo/MTR payload contents, but do not define numeric `record_type` codes, `flags`/reserved behavior, exact `total_length`/padding semantics, byte-exact `WAL_PAD`, WAL PageId encoding, patch/count widths, MTR discriminators, checkpoint payload bytes, or an exact oversized-record rule.

**Rewrite action:** Chapter 12 preserves the locked header and semantic recovery contract without inventing the missing binary grammar.

**Resolution required before WAL implementation/final cutover:** define the complete v1 WAL binary grammar, record-type registry, length/padding convention, and payload codecs.

## R-026 — `database.control` slot format is semantic but not byte-exact

**Type:** persistent control-file format gap discovered during Pass 8.

Legacy §237 locks an 8192-byte file with two alternating 4096-byte slots and semantic fields for magic/version/generation/checkpoint pointers/reserved TxnIds/next FileId/flags/CRC, but does not define exact offsets/widths, magic, flag bits, CRC range, initial slot state, generation overflow policy, or zero/reserved bytes.

**Rewrite action:** Chapter 13 preserves alternating-slot durability semantics without fabricating the missing persistent bytes.

**Resolution required before control-file implementation/final cutover:** define the complete 4096-byte slot codec and validation/update rules.

## R-027 — Checkpoint WAL sequence identity is underspecified

**Type:** recovery-format/protocol gap discovered during Pass 8.

Legacy §§240–243 require `CHECKPOINT_BEGIN`, one or more `CHECKPOINT_DATA`, and `CHECKPOINT_END`, where incomplete checkpoints are ignored and control metadata is installed only after END is durable. The source does not define the persistent identifier/linkage proving which DATA/END records belong to which BEGIN across multiple or interrupted checkpoint attempts.

**Rewrite action:** Chapter 13 preserves checkpoint completeness/install semantics but does not guess the sequence identifier.

**Resolution required before checkpoint/recovery implementation/final cutover:** define byte-exact BEGIN/DATA/END linkage and chunk ordering/completeness validation.

**Pre-Pass-10 coherence review additions:**

The current rewritten Chapters 13.2 and 13.10 also use two names:

```text
database.control.latest_checkpoint_lsn
latest_checkpoint_begin_lsn
```

without explicitly stating that they are the same persisted value or defining another field.

More importantly, recovery analysis currently says to start from the installed checkpoint metadata, while terminal transaction records from **before** `CHECKPOINT_BEGIN` can still be required when a dirty transaction-status page has:

```text
rec_lsn < checkpoint_begin_lsn
```

and its terminal status was not yet forced to the status file.

The checkpoint/recovery contract therefore also needs to define:

- what exact LSN `latest_checkpoint_lsn` names,
- the analysis scan lower bound relative to `checkpoint_redo_lsn`,
- how pre-BEGIN terminal records needed for dirty status pages are replayed/reconstructed.

A natural candidate is to make `latest_checkpoint_lsn` explicitly the BEGIN LSN and allow analysis/recovery scanning from the minimum required recovery LSN, but this review does not silently select the final protocol.

## R-028 — Read-epoch grace arithmetic is not exact

**Type:** physical reclamation concurrency gap discovered during Pass 9.

Legacy §§261–263 lock:

```text
global monotonically increasing epoch
active reader epochs
retirement epoch at NORMAL -> DEAD
DEAD -> UNUSED only after all readers that could have observed the old RID exit
```

but do not define the exact epoch advance/registration/retirement arithmetic or synchronization that makes the grace test linearizable.

**Rewrite action:** Chapter 14 preserves the exact physical-identity safety property and restart semantics without inventing arithmetic.

**Resolution required before ReadEpochManager / physical RID reuse implementation:** define one exact registration/retirement protocol and grace predicate.

## R-029 — Transaction-status prefix reclamation conflicts with the current absolute status-page mapping

**Type:** cross-chapter persistent-storage coherence issue discovered during Pass 9.

The resolved §9.12 mapping is absolute:

```text
status_page_no =
    1 + (txn_id - FIRST_NORMAL_TXN_ID) / 32640
```

Legacy §270 allows sufficiently old status pages to be retired/truncated after freezing proves they are unnecessary.

Ordinary prefix truncation would remove/renumber pages required by the absolute mapping.

The architecture defines no base-TxnId offset, segmented status files, sparse-hole contract, logical-retired-page representation, or remapping metadata.

**Rewrite action:** Chapter 14 defines semantic retirement eligibility but forbids guessing physical prefix compaction/remapping.

**Resolution required before transaction-status physical reclamation/final cutover:** choose a persistent reclamation scheme compatible with deterministic TxnId lookup.

## R-030 — Ordinary-page checksum lifecycle is incomplete for the torn-page recovery contract

**Type:** cross-chapter persistence/recovery coherence gap discovered in the pre-Pass-10 review.

Chapter 4 locks whole-page CRC32C coverage, but currently says that once recovery exists, ordinary-page checksums only **SHOULD** be enabled by default.

Chapter 13 relies on an invalid page checksum to decide:

```text
do not trust page_lsn
restore from WAL full image
```

The architecture also does not define the BufferPool writeback step that finalizes/recomputes `checksum_crc32c` after `page_lsn` and persistent page bytes have reached their write image.

Without mandatory verification and a checksum-finalization rule, the promised torn-page recovery path is not complete.

**Required resolution:**

Define at least:

- whether WAL/recovery-enabled v1 ordinary random-access pages MUST carry/verify CRC32C,
- when a resident page checksum may be stale,
- how the flush image is stabilized,
- when bytes `16..19` are recomputed,
- how checksum finalization composes with page latches, `page_lsn`, dirty-state races, and full-page WAL images.

**Timing:** SOLVE NOW. No later catalog/execution/optimizer pass owns this storage/recovery rule.

## R-031 — New-page allocation and initialization lack a crash-safe publication protocol

**Type:** storage/WAL publication gap discovered in the pre-Pass-10 review.

The storage layer allocates pages by extending a file. WAL later defines `PAGE_INIT` and B+ MTR full images.

The architecture does not define what happens if a process crashes after physical file extension but before a valid initializing WAL record has made that new PageNo part of recoverable database state.

This can leave zero/uninitialized appended pages that fail normal page-format validation.

The problem is especially important under concurrent appenders: without a publication rule, a failed initialization could otherwise leave an interior uninitialized page before a later valid page.

The same relation-wide boundary also needs to state how new heap pages cause the advisory FSM initialized prefix/page set to grow.

**Required resolution:**

Define one crash-safe page-allocation publication protocol covering:

- reservation of a new PageNo,
- file extension,
- PAGE_INIT or BTREE_MTR creation,
- when the new page becomes reachable/scan-visible,
- serialization needed to prevent unrecoverable allocation holes,
- recovery handling of trailing unpublished extensions,
- heap/FSM growth ordering.

**Timing:** SOLVE NOW. This is owned by the already migrated storage/WAL core.

## R-032 — Vacuum reclamation state machine is incomplete at `DEAD -> UNUSED` and crash-retry boundaries

**Type:** heap/vacuum data-structure coherence gap discovered in the pre-Pass-10 review.

Chapter 14 currently orders:

```text
DEAD -> UNUSED
then optionally compact reclaimable tuple bytes
```

but Chapters 5/6 authorize compaction to physically discard tuple payload only while the slot is persistently `DEAD`.

After the state is changed to `UNUSED`, the current compaction contract no longer authorizes discarding that old payload.

The architecture also does not yet define:

- canonical `tuple_offset`, `tuple_length`, and `aux` values when a reclaimed slot becomes `UNUSED`,
- whether v1 actually uses the persisted `free_slot_head` after safe reuse becomes available or simply scans for `UNUSED`,
- crash-retry semantics when some exact B+ garbage entries were removed before the crash but the heap slot remained `NORMAL`.

Chapter 14 explicitly allows that last crash state, so cleanup retry must be idempotent; an already-absent exact `(key,RID)` entry cannot make vacuum permanently fail.

**Required resolution:**

Lock the complete v1 reclamation state transition, including payload compaction ordering, canonical UNUSED fields, free-slot discovery policy, and idempotent re-entry after partial index cleanup.

**Timing:** SOLVE NOW. No upper-layer pass owns the heap slot-state machine.

## R-033 — READ COMMITTED whole-statement retry is unsafe after partial persistent writes

**Type:** transactional correctness gap discovered in the pre-Pass-10 review.

The architecture correctly uses no physical ordinary-user-DML undo.

It also allows READ COMMITTED to restart an affected statement with a fresh snapshot.

However, a multi-row statement may have already produced persistent heap versions/index entries before encountering a later row conflict.

`CommandId` hides same-command writes during the current attempt, but if the retry later succeeds and the command advances, writes from the abandoned attempt would become self-visible/committed as well.

Buffering `RETURNING` protects external output only; it does not roll back internal persistent state.

Without savepoints, subtransaction outcome state, or physical statement undo, unrestricted whole-statement retry after the first persistent write is not correct.

**Required resolution:**

Choose a precise v1 retry boundary. Viable families include:

- permit internal retry only while the current statement attempt has produced no persistent writes; after that, force transaction abort/retry at a higher boundary,
- or introduce a real statement-attempt rollback/subtransaction mechanism.

The existing architecture intentionally defers savepoints/subtransactions and ordinary physical undo, so the first option is the smallest compatible v1 rule.

**Timing:** SOLVE NOW. Passes 10–13 may define SQL/executor mechanics, but they must consume a correct transaction retry contract rather than invent one.

## R-034 — Terminal transaction publication has an active-registry / lock-release linearization race

**Type:** transaction/locking/snapshot coherence gap discovered in the pre-Pass-10 review.

Current status lookup checks:

```text
active registry -> IN_PROGRESS
before
terminal cached/persistent status
```

while COMMIT/ABORT currently do:

```text
publish terminal outcome
release logical locks
then remove active-registry state
```

In that window another writer can acquire the just-released logical key/tuple lock but still classify the old transaction as `IN_PROGRESS`.

There may then be no logical lock owner left to wait on.

Snapshot capture also needs a single linearization point deciding whether a transaction is active or terminal.

**Required resolution:**

Define one atomic/synchronized terminal-publication boundary.

For example, terminal runtime outcome may dominate active-registry lookup, and snapshot-active membership can be removed/marked terminal before transaction locks become acquirable by another writer.

Whatever rule is chosen must guarantee that a new snapshot/writer observes either:

```text
nonterminal + lock ownership
```

or:

```text
terminal outcome
```

rather than a hybrid state.

**Timing:** SOLVE NOW. This is entirely inside Chapters 9/11/15.

## R-035 — Fuzzy checkpoint WAL retention does not yet guarantee retention of the full image needed for a later torn write

**Type:** WAL/checkpoint/torn-page correctness gap discovered in the pre-Pass-10 review.

The architecture currently requires one full-page image on the first modification after a completed checkpoint epoch, then permits later deltas.

The checkpoint is fuzzy and does not force all dirty pages.

Consider a page whose earlier checkpoint-epoch full image is at LSN `I`, which is later successfully flushed, then becomes dirty again at delta LSN `D > I` without another full image. A later fuzzy checkpoint may capture:

```text
rec_lsn = D
```

and retain WAL only from the checkpoint/redo bound at or after `D`.

If the data page subsequently tears during writeback, the valid on-disk base can be destroyed while the older full image `I` has already been recycled.

The current DPT stores only:

```text
PageId
rec_lsn
```

so it has no separate retained-image LSN that would prevent this.

**Required resolution:**

Strengthen the FPI/retention invariant.

A simple v1-compatible design is to require a full after-image on the first WAL-protected modification of every **clean -> dirty interval**, so `rec_lsn` itself always identifies a retained recovery image for a dirty page. The existing post-checkpoint FPI rule can remain as an additional bound.

Other designs are possible, but the retained recovery image for every potentially torn dirty page must be explicit.

**Timing:** SOLVE NOW. Waiting for catalog/execution passes cannot repair this recovery invariant.
