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

**Rewrite action:** preserve the current strict architecture contract unless the project owner explicitly changes it. Do not weaken it during restructuring.

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

