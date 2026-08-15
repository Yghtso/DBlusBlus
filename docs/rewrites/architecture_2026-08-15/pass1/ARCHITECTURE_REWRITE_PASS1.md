# Rewrite Pass 1 — Scope, Global Architecture, and Legacy Overview

## Source and scope

Source snapshot:

- file: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `0..52`
- processed source line range: `1..1423` plus the legacy section-0 heading at line 9
- production code changed: **none**
- legacy architecture changed: **no**

This pass rewrites the early architecture overview into the target document structure established by Pass 0.

## What changed in `ARCHITECTURE_NEW.md`

Pass 1 adds:

- technical document purpose and normative contract language,
- system scope, design goals, and non-goals,
- dependency/layering model,
- Linux/C++20 runtime baseline,
- concise baseline contracts for storage, heap/tuple layout, FSM, BufferPool, indexing, transactions, MVCC, locking, WAL, recovery, vacuum, catalog, SQL front end, logical planning, execution, and optimization,
- cross-cutting error/corruption, observability, verification, and performance requirements,
- global architecture invariants,
- consolidated deferred/future scope.

The early overview sections are intentionally concise where later legacy sections contain the concrete contract. Their purpose is to preserve unique high-level commitments and rationale without recreating the evolutionary duplication of the old file.

## Non-architecture material removed from the new contract

### Legacy §0

AI/Codex workflow instructions are excluded from `ARCHITECTURE_NEW.md`.

The technical requirements embedded in that section were either already represented elsewhere in the architecture or migrated to their canonical technical sections.

### Legacy §49

The suggested implementation order is not system architecture.

It remains preserved in the legacy file. A future `DEVELOPMENT.md` remains the proposed human-facing destination.

### Legacy §§45–46

Architecture-level verification and benchmark obligations remain in Chapters 41–42.

Detailed test recipes and benchmark procedures are not duplicated into the architecture. They remain preserved by the legacy file pending a possible `VERIFICATION.md`.

### Legacy §52

The architecture-relevant design intent is retained in Chapter 1.

General implementation/learning workflow language is excluded from the technical contract.

## Important semantic treatment

### Recovery overview

Legacy §24 says `analysis / redo / undo`.

The already-recorded rewrite issue R-002 notes that legacy §181 later explicitly refines v1 recovery to avoid physical undo of ordinary user-DML heap/index changes.

Pass 1 therefore preserves only the non-conflicting ARIES-inspired recovery baseline and does not make generic physical user-DML undo independently normative in the new document.

The complete v1 recovery semantics remain for Pass 8.

### Logical vs physical architecture

The early architecture's distinction between logical relational operators and physical implementations is preserved as a system-wide boundary and as the foundation of the execution/optimizer chapters.

### Concurrency distinction

Transaction-level logical locks remain distinct from page/tree latches throughout the migrated baseline.

### Performance requirements

The old “correctness before micro-optimization” and measurement philosophy is expressed as technical system-quality requirements rather than as instructions to an AI/coding agent.

## Coverage ledger result

All legacy sections `0..52` now have non-`PENDING` migration statuses.

Legacy sections `53..725` remain `PENDING`.

No Pass 1 section was silently dropped.

Special dispositions:

- §0 — moved out of architecture,
- §24 — migrated with explicit later-refinement handling,
- §§31, 38, 40, 45, 46, 52 — split between architecture content and non-architecture/process material,
- §49 — excluded from final architecture as project roadmap,
- §50 — migrated to the global invariant appendix,
- §51 — merged into the foundational decisions chapter.

## Issue register changes

Pass 1 retains R-001 through R-007 and adds:

- **R-008** — supporting-document preservation required before final cutover for roadmap/verification material;
- **R-009** — legacy join/aggregation/sort overview mixes architecture and implementation sequencing.

Neither issue changes database architecture in this pass.

## Validation performed

The pass verified:

1. the legacy source hash still equals the Pass 0 pinned hash;
2. every legacy numbered section `0..52` has a completed migration disposition;
3. no legacy section `53..725` was marked migrated;
4. no production source file was modified;
5. the legacy architecture file was not modified;
6. `ARCHITECTURE_NEW.md` contains no Codex/AI workflow section;
7. the twelve legacy global invariants are represented in Appendix B;
8. C++20 and heap-version MVCC remain explicit foundational decisions;
9. the early generic recovery wording is not allowed to contradict the later concrete refinement.

## Pass 1 exit status

**COMPLETE.**

Pass 2 should process only legacy §§53–63 and turn Chapter 4 from a high-level storage baseline into the canonical persistent-storage-foundation contract.
