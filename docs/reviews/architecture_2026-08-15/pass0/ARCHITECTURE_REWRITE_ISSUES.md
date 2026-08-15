# Architecture Rewrite Issue Register

Source architecture snapshot: `ARCHITECTURE(4).md`  
SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`

This register records inconsistencies, ambiguities, or semantic decisions discovered during the rewrite. It is not itself an architecture authority.

## R-001 — RID reserved-byte contract vs Phase 1 implementation checkpointARCHITECTURE

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
