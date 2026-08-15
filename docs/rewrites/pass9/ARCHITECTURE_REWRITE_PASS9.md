# Rewrite Pass 9 — Vacuum, Physical Reclamation, and End-to-End Transaction Protocols

## Source and scope

- source: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `256..300`
- processed source lines: `8383..9653`
- legacy architecture modified: **no**
- production code modified: **no**
- catalog/SQL §301+ migrated: **no**

## Canonical result

Chapter 14 now owns the complete vacuum/reclamation chain:

```text
registered-snapshot global horizon
garbage eligibility
exact index cleanup
NORMAL -> DEAD
read-epoch grace
DEAD -> UNUSED
version-chain splicing
aborted-xmax cleanup
freezing/status retirement eligibility
FSM/vacuum maintenance
```

Chapter 15 now owns the integrated INSERT/UPDATE/DELETE/COMMIT/ABORT and READ COMMITTED retry boundary.

The cross-cutting error, observability, verification, and benchmark material from §§281–291 was consolidated into Chapters 39–42 instead of duplicated as a transaction checklist.

Roadmap/module-layout/milestone/status material in §§295–300 was classified rather than copied into the architecture. Only lasting subsystem responsibility and cross-layer contract rules were retained.

## Key reclamation rule

Physical RID reuse is now canonically:

```text
NORMAL
  -> remove exact index entries
  -> DEAD
  -> read-epoch grace
  -> UNUSED/reusable
```

A surviving tuple version cannot retain `prev` pointing to reusable storage.

This completes R-021.

## New explicit gaps

### R-028
The source defines read-epoch safety, but not exact epoch arithmetic/linearization.

### R-029
The source permits old transaction-status pages to become reclaimable, but the current absolute TxnId-to-status-page mapping does not define physical prefix truncation/remapping.

No mechanism was guessed.

## Coverage

```text
legacy §§0..300     complete
legacy §§301..725   pending
```

All 45 Pass-9 sections have explicit dispositions.

## Validation

- legacy SHA unchanged,
- §256 starts at line 8383,
- §301 starts at line 9654,
- all coverage rows through §300 are non-PENDING,
- all rows from §301 remain PENDING,
- Chapters 14 and 15 each occur exactly once,
- no §301+ catalog/SQL material was imported,
- legacy architecture and production code were untouched.

## Exit status

**COMPLETE WITH R-028 AND R-029 EXPLICITLY OPEN.**
