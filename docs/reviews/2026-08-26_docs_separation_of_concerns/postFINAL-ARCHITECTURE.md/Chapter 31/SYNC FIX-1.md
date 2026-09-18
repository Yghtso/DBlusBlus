## 1. Repository state

Initial:

```text
HEAD:
    2757374e707c4d64cb17f5076d60f7fe3b445a78

COMMIT:
    2757374 synced VERIFICATION after chapter 31 ARCHITECTURE fix

WORKTREE:
    clean

INDEX:
    clean
```

Final:

```text
WORKTREE:
    M docs/VERIFICATION.md

INDEX:
    clean
```

Only `docs/VERIFICATION.md` was modified by this repair.

## 2. Sections changed

- V31-I — Autocommit pre-C0 result preparation
- V31-K — Non-failing C-to-R transition
- Chapter 31 subsection coverage map

V31-A through V31-N and V31-001 through V31-041 were preserved.

## 3. MAJOR-1 resolution

V31-I now contains a concrete preparation fault matrix covering:

- semantic finalization;
- affected-count representation;
- RETURNING construction and append;
- spill writes;
- spool finalization;
- schema/descriptor validation;
- framing and representability;
- result-envelope construction;
- result-owner allocation;
- cursor construction;
- spill-handle acquisition;
- resource registration;
- publication destination preparation;
- physical allocation after reservation;
- retained-value backing;
- accounting preparation;
- cleanup-owner preparation.

Each row specifies:

- semantic boundary;
- injected failure;
- reusable owner or required Verification hook;
- expected event trace;
- transaction/error ownership;
- cleanup and accounting result.

The procedure now rejects vacuous passes when the intended operation is never reached.

## 4. Pre-C0 event oracle

Every required-preparation failure must show:

```text
ordinary closure completed
targeted preparation boundary entered
injected failure observed
C0 NOT entered
no COMMIT-authorizing append
R NOT occurred
no successful result/count publication
no RETURNING prefix delivered
safe cleanup and accounting closure
```

The actual W state remains authoritative:

- `W = false`: canonical pre-W consequence;
- `W = true`: canonical post-W execution consequence;
- stronger Chapter-39 owners remain authoritative where applicable.

Successful controls must show preparation success before C0, followed by C0–C5 and R.

## 5. Reusable procedures used

Confirmed live reusable owners include:

- `V21-1` — bound-result handoff;
- `V21-2` — statement attempts and retry;
- `V21-14` — RETURNING bags;
- `V23-G/H` — backing and borrowing lifetime;
- `V24-B/C/D` — accounting, allocation, and ownership;
- `V24-F` — retained rows;
- `V24-G/I` — exact representability and arithmetic;
- `V24-J/K` — spill framing and reclamation;
- `V24-M` — retry, teardown, and retained ownership;
- `V26-H/I/J/M` — finalization, error transport, publication, and cleanup;
- `V30-G` — spill/source failure comparison;
- existing transaction, isolation, locking, VACUUM, catalog, and statistics sections.

## 6. New Verification hooks

Where no existing reusable injector directly covers the operation, V31 now requires a semantic hook for:

- count representation;
- envelope construction;
- result-owner acquisition;
- cursor construction;
- spill-handle operations;
- resource registration;
- publication-destination preparation;
- cleanup-owner preparation;
- C-to-R suboperation tracing.

These hooks do not prescribe C++ classes, allocator APIs, or a tracing implementation.

## 7. Preparation versus delivery

The Verification now explicitly distinguishes:

```text
required owner allocation before C0
versus delivery-chunk allocation after R

required spool write/finalization before C0
versus future spill read after R

envelope construction before C0
versus transport encoding after R
```

The expected transaction and statement outcomes are owned by their respective stages.

## 8. MAJOR-2 resolution

V31-K now requires a structured event trace containing:

```text
phase boundary
operation category
required-to-establish-R classification
attempted/not attempted
ordering sequence
completion or injected failure
owner and cleanup consequence
```

The trace must expose suboperations hidden behind ownership transfer.

## 9. C-to-R zero-operation oracle

Between successful C4–C5 and successful R, the required-operation count must be zero for:

- physical allocation;
- memory-budget grant;
- validation;
- result construction;
- required spill I/O;
- handle acquisition or duplication;
- resource registration;
- fallible cursor construction;
- required external callbacks;
- publication-destination preparation.

An attempted forbidden operation fails conformance even if it succeeds.

Permitted operations include existing COMMIT internals through C5, non-failing R activation, instrumentation, and post-R delivery work.

## 10. Positive and negative controls

V31-K now requires:

- a positive pre-C0 control showing an allowed fallible preparation operation;
- a positive post-R delivery control showing permitted delivery allocation or spill read;
- failure as `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE` if required trace events are unavailable.

Hidden required allocation cannot be relabeled as delivery.

## 11. Crash and invariant separation

The repaired wording preserves three distinct cases:

1. Required work attempted between C and R: implementation nonconformance.
2. Actual invariant/noncontinuable defect: COMMITTED remains COMMITTED; no fabricated R, ABORT, or replay.
3. Crash/session loss after durable COMMIT: recovery/session owner applies; committed effects remain; R may never occur.

## 12. No-RETURNING and zero-row coverage

The V31-I/K requirements continue to cover:

- INSERT, UPDATE, DELETE;
- with and without RETURNING;
- zero-row DML.

No-RETURNING statements still prepare command-result metadata and cross R. Zero-row statements still prepare count `0`, satisfy applicable COMMIT prerequisites, and cross R even when W is absent.

## 13. MINOR-1 resolution

The coverage map now uses exact live references.

Examples:

```text
V24-M — Retry, cancellation, teardown, and retained ownership
V21-2 — Statement attempts, CommandId, and retry
V26-J — Internal output and external publication
V26-M — Backing release, borrowing, and reset
V30-G — Temporary format, error categories, and Source failure
Vacuum and Reclamation Tests
Control-Operator Tests
Catalog Tests
Statistics Publication and Versioning Tests
```

The vague references `V9/V11/V15`, `V14 Vacuum`, `V30 Source`, and `V26 handoff` were removed from the Chapter 31 map.

All referenced targets were confirmed to exist and supply the claimed component coverage.

## 14. Counts and integrity

```text
V31 families:
    14

V31 atomic obligations:
    41

Duplicate V31 family IDs:
    none

Duplicate V31 atomic IDs:
    none

Missing V31-001..V31-041:
    none

Broken corrected references:
    none
```

## 15. Chapter-41 coverage

The repaired procedures now provide explicit coverage for:

- preparation before C0;
- reservation versus physical allocation;
- C1 readiness;
- C5 result lifetime;
- zero required C-to-R operations;
- atomic R;
- post-R delivery classification;
- COMMIT uncertainty;
- result backing and accounting;
- no abort or replay after COMMITTED.

## 16. Regression status

Preserved:

```text
N31-1:
    ordinary candidate closure before W

N31-2:
    atomic R and post-R delivery ownership

Architecture:
    not modified

V31-A through V31-N:
    preserved

V31-001 through V31-041:
    preserved
```

No new Architecture semantics were introduced.

## 17. Historical synchronization discrepancy

Read-only Git metadata confirms the prior synchronization commit contains:

```text
docs/VERIFICATION.md
docs/reviews/2026-08-26_docs_separation_of_concerns/
postFINAL-ARCHITECTURE.md/Chapter 31/SYNC.md
```

This remains a historical commit-scope discrepancy. No historical artifact was opened, modified, reverted, or deleted by this repair.

## 18. Validation

```text
git diff --check:
    PASS

Repair diff:
    docs/VERIFICATION.md only

Final status:
    M docs/VERIFICATION.md

Index:
    clean

Architecture:
    unmodified
```

Confirmed:

```text
ARCHITECTURE NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
HISTORICAL ARTIFACTS NOT MODIFIED BY THIS REPAIR
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
CHAPTER 32 REVIEW NOT STARTED
```

CHAPTER 31 VERIFICATION FIX V1 COMPLETE —
READY FOR FINAL READ-ONLY VERIFICATION CLOSURE AUDIT