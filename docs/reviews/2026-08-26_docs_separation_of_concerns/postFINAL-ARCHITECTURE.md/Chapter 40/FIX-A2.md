# CHAPTER 40 — FIX A2

**Result: CLOSED N40-2 at the repair level.** [§40.2 EXPLAIN](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29670) now expressly requires ordinary `EXPLAIN SELECT` to plan without executing the inner SELECT. `EXPLAIN ANALYZE SELECT` uses the same planning and final-validation rules, then executes under ordinary SELECT ownership.

Initial and final HEAD: `7ba88ae0354c90427dc0a4ac74bef97ebaed1031` (`applied FIX-A1 40 in ARCHITECTURE`). A1 had been **committed** before this task; the initial worktree and index were clean, so no pre-existing A1 worktree diff needed preservation. A2 changed only `docs/ARCHITECTURE.md`: **18 insertions, 0 deletions**, all in §40.2. That is also the total current worktree diff. Final status is ` M docs/ARCHITECTURE.md`; the index is empty.

The new rule permits canonical parsing, binding, logical planning and validation, and—when physical information is requested—optimization and final physical-plan validation. Their existing errors, including planning-owned resource failures, remain possible. These stages are **not** physical SELECT execution. Ordinary EXPLAIN cannot start operators or pipelines, incur execution-only memory or spill work, return the SELECT’s rows, or fabricate actual rows, time, memory, spill, or operator counters. A runtime-only error is not induced by executing the inner SELECT for ordinary EXPLAIN; an independently required earlier-stage error remains reportable. EXPLAIN ANALYZE executes the selected plan with ordinary transaction, snapshot, lock, cancellation, resource/error, and cleanup rules. Presentation does not choose a different plan or create an EXPLAIN-specific transaction model.

| Conceptual fixture | Contract result |
|---|---|
| A. Ordinary EXPLAIN of a simple SELECT | Plans as needed; no physical execution or SELECT rows. **Resolved.** |
| B. EXPLAIN ANALYZE of that SELECT | Executes the selected plan and observes performed work. **Resolved.** |
| C–D. Parser or binder error | Existing front-end owner reports it; no execution. **Resolved.** |
| E–F. Optimizer or final-validation failure | Existing planning/internal owner reports it; invalid plan never executes. **Resolved.** |
| G. Runtime-only expression error, such as division by a zero value read from a table row | Ordinary EXPLAIN does not induce it by execution; EXPLAIN ANALYZE follows Chapter 39 if execution demands it. **Resolved.** |
| H. Execution-time spill failure | No execution `SpillIOError` for ordinary EXPLAIN; possible under EXPLAIN ANALYZE. **Resolved.** |
| I. Execution-time OOM | Ordinary EXPLAIN may still have planning OOM, but no OOM from unstarted operators. **Resolved.** |
| J. Cancellation | Planning may be cancelled in ordinary EXPLAIN; EXPLAIN ANALYZE may be cancelled during execution. Partial-profile presentation remains undecided. **Resolved for A2.** |
| K. Estimates versus actuals | Ordinary EXPLAIN has planning diagnostics only; EXPLAIN ANALYZE actuals come from executed operators. **Resolved for A2.** |

The global Architecture search found no frozen-owner contradiction. Chapter 18 owns SELECT-only syntax; Chapters 20 and 33–38 own logical/physical planning and validation; execution and Chapter 39 retain runtime-error ownership. Existing Verification procedures—including V19-17, V20-22, V33 planning/handoff procedures, and the EXPLAIN ANALYZE and Profiling Tests—offer reusable boundaries for future V40 verification, but `docs/VERIFICATION.md` was not modified.

N40-1’s committed metric contract remains intact. N40-3’s failed/partial-profile and q-error applicability questions remain open; §§40.6–40.8 were not edited. Development, Project State, frozen chapters, source, tests, build files, and historical artifacts were not modified. `git diff --check` passed. No build, test, sanitizer, or benchmark was run; nothing was staged or committed.

**Next authorized task:** Chapter 40 — Fix A3: Profile Completion, Partial Execution, Early Stop and Q-Error Applicability. Verification synchronization is premature.

    CHAPTER 40 FIX A1:
        CLOSED N40-1 / PRESERVED
    CHAPTER 40 FIX A2:
        CLOSED N40-2
    N40-1 — METRIC IDENTITY:
        CLOSED / UNMODIFIED
    N40-2 — ORDINARY EXPLAIN EXECUTION:
        CLOSED
    N40-3 — PROFILE/Q-ERROR APPLICABILITY:
        OPEN / UNMODIFIED
    CHAPTER 40 ARCHITECTURE:
        NOT YET CLOSED
    CHAPTER 40 VERIFICATION:
        NOT SYNCHRONIZED / UNMODIFIED
    IMPLEMENTATION:
        NOT AUTHORIZED BY THIS TASK
    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN
    INDEX:
        UNMODIFIED
    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

END CHAPTER-40 ARCHITECTURE FIX A2.