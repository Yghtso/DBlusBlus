# CHAPTER 40 — FIX A1

**Result: CLOSED N40-1 at the repair level.** Chapter 40 Architecture is not independently closed; N40-2 and N40-3 remain open.

The initial HEAD was `954d2ec932ff5eff8f75806a82712135ff0f19d9` (`chapter 40 ARCHITECTURE analysis`), rather than the expected baseline. The worktree and index were clean. Final HEAD is unchanged; the only worktree change is `docs/ARCHITECTURE.md`, and the index remains clean. The diff is **80 insertions, 0 deletions**, confined to [§40.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29628) and [§40.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29727).

The repair gives each reported metric an owner, unit, and scope/lifetime. It distinguishes event totals, sampled gauges, stage durations, and approximate estimates. Diagnostic overflow must be marked by saturation or unavailability; it cannot silently wrap, cause a SQL arithmetic error, change a transaction outcome, or make the database noncontinuable. Metrics report canonical subsystem facts but never become authority for them.

| A1 fixture | Result under the repaired contract |
|---|---|
| A. Resident hit | Logical read +1, hit +1, miss +0, physical read +0. **Resolved.** |
| B. Successful simple miss | Logical read +1, hit +0, miss +1, physical read +1. **Resolved.** |
| C. Two claims share one load | Logical reads +2, misses +2, physical reads +1. **Resolved.** |
| D. Load fails | No completed logical read/hit/miss. A completed full-page transfer counts as a physical read even if validation then fails; a partial/failed transfer does not. **Resolved.** |
| E. Five committers share one durability operation | One flush attempt and one successful group-commit batch of size five; newly durable WAL bytes count once. Each transaction outcome is counted at its canonical boundary. **Resolved.** |
| F. COMMITTED, C6 acknowledgement lost | Committed +1, aborted +0; connection/uncertainty is separate. **Resolved.** |
| G. MUST_ABORT then ABORTED | Intermediate states are sampled gauges; aborted +1 only at authoritative A2 publication. **Resolved.** |
| H. Two active snapshots | Sampled active-snapshot gauge is 2, not a lifetime total. **Resolved.** |
| I. Counter representation exhausted | Marked saturation or overflow/unavailability; no wrap or semantic failure. **Resolved.** |

A logical page read now means a successful normal BufferPool fetch claim converted to a public pin. Hits and misses partition those completed claims; internal retries do not double-count them. Physical reads and page writes count completed full-page transfers, not logical requests, mutations, or durable writeback. WAL appended bytes follow the valid published append prefix; synced bytes follow newly established durability, without multiplication by waiters. Transaction begin, commit, and abort totals follow canonical admission and semantic terminal boundaries, not response delivery.

The global owner search found **no frozen-chapter contradiction**: BufferPool, WAL, transaction, and approximate-statistics owners retain their existing semantics. Existing Verification material was inspected for future testability but not changed. No Chapter 40 Verification procedure was added.

N40-2’s ordinary-EXPLAIN nonexecution rule and N40-3’s profile/q-error applicability rules were **not repaired**; §§40.2 and 40.6–40.8 are unchanged. The next recommended task is **Chapter 40 — Fix A2: Ordinary EXPLAIN Nonexecution Boundary**.

`git diff --check` passed. No files outside the authorized edit changed; `docs/VERIFICATION.md`, Development, Project State, frozen chapters, and historical artifacts are unmodified. No procedures were executed, and no build, test, sanitizer, benchmark, staging, or commit occurred.

    CHAPTER 40 FIX A1:
        CLOSED N40-1
    N40-1 — METRIC IDENTITY:
        CLOSED
    N40-2 — ORDINARY EXPLAIN EXECUTION:
        OPEN / UNMODIFIED
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

END CHAPTER-40 ARCHITECTURE FIX A1.