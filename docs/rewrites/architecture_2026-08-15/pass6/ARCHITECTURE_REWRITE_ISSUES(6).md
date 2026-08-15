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

However, the legacy contract does not fully define:

- byte offsets and widths for the B+ superblock-specific metadata fields,
- whether/how the base FileSuperblock's `object_id` relates to the separately listed `IndexId`,
- exact B+ node `format_version`,
- explicit common-header `header_size` requirement for B+ nodes,
- persisted bit meanings/valid masks for node-header `flags`,
- persisted bit meanings/valid masks for node-slot `flags`,
- zero/compatibility semantics for leaf/internal reserved bytes,
- exact `BTREE_FREE` payload offsets/widths/version/header rules for `next_free_page_no`,
- the algorithm/semantic contract for `key_schema_fingerprint`,
- bit semantics for `index_flags`.

**Rewrite action:** Chapter 8 preserves every byte offset and field that is actually specified, and explicitly leaves these missing values/semantics unresolved rather than inventing them.

**Resolution required before final cutover:** define a complete v1 B+ persistent-format contract for the specialized superblock, leaf/internal common-header requirements, flags/reserved fields, free pages, and schema fingerprint.


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

