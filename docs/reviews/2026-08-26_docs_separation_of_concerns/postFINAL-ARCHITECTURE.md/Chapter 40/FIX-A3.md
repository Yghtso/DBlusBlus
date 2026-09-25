# CHAPTER 40 — FIX A3

**Result: CLOSED N40-3 at the repair level.** This is not an independent Chapter-40 Architecture closure audit.

A2 had been committed before this task, contrary to the expected uncommitted baseline. Initial and final HEAD are `6a460f1503f943cc2eec1af98c4b31ace522c7b3` (`applied FIX-A2 40 in ARCHITECTURE`). The initial worktree and index were clean; there was **no pre-existing A2 worktree diff**. A3 changed only `docs/ARCHITECTURE.md`: **55 insertions, 3 deletions**, which is the entire current worktree diff. Final status is ` M docs/ARCHITECTURE.md`; the index remains clean.

The edit is confined to [§40.6 and §§40.6.1–40.6.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29827) and [§40.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:29999). A1’s metric rules and A2’s ordinary-EXPLAIN nonexecution rule are unchanged.

The repaired diagnostic model distinguishes, per physical node and measured output boundary:

- **Not started:** the estimate remains visible, but no actual row count is observed. It is not a completed zero.
- **Started, incomplete:** counters report only work actually observed. Early stop, failure, or cancellation may leave cardinality demand-censored.
- **Cardinality-comparable:** the observed count completely covers the same output population and boundary as that node’s estimate. Opening, clean teardown, or local `FINISHED` alone does not prove this.

These are profile-applicability classifications, **not execution or transaction states**. A parent can be comparable while a child is not. Fully consumed blocking input remains valid input-work evidence, but does not by itself prove that the operator’s estimated *output* boundary was completely observed.

Numeric q-error now requires a completed, comparable actual at the estimate’s boundary. The existing formula remains unchanged: positive `E,A` use `max(E/A,A/E)`; completed `0/0` gives `1`; exactly one zero gives an infinite marker. Not-started or incomplete/demand-censored actuals receive explicit *unavailable/not comparable*, never invented zero, infinity, or extrapolation. Estimates and semantic-empty proof provenance remain separately visible. Runtime actuals still do not automatically update persistent statistics.

If a failed or cancelled partial profile is exposed, it identifies unsuccessful execution, retains only safely observed work, and leaves the original error or cancellation primary. Worker-local measurements are combined without double counting on success; during failure cleanup, safely available measurements are retained before local state is discarded when a partial profile is exposed. Missing measurements are marked incomplete, not fabricated. A diagnostic merge failure cannot alter canonical query or transaction outcomes, indefinitely impede cleanup, or keep tasks runnable. No crash-surviving profile or particular error-plus-profile wire format is required.

| Fixture | Result |
|---|---|
| A–B: completed positive counts | `100/100 → 1`; `100/1 → 100`. **Resolved.** |
| C–E: completed zero cases | `0/0 → 1`; `0/10` and `10/0 → infinity`. **Resolved.** |
| F–G: node never starts | Estimate retained; no completed actual zero; q-error unavailable. **Resolved.** |
| H: `Limit(1)` over a stopped scan | Scan’s one observed row remains work evidence, but its q-error is unavailable; Limit is assessed independently. **Resolved.** |
| I: EXISTS first-row stop | Result-producing node may complete; censored child retains work counters without comparable full cardinality. **Resolved.** |
| J: exhausted filter emits zero | Genuine comparable zero; existing zero q-error rules apply. **Resolved.** |
| K: hash-build/spill failure | Original `SpillIOError` remains primary; exposed work is partial, with no fabricated completion. **Resolved.** |
| L: cancelled scan | Processed-row work remains observable; full-cardinality q-error is unavailable. **Resolved.** |
| M: blocking child complete, later output stopped | Input and output boundaries are assessed separately, node by node. **Resolved.** |
| N: worker failure | Available local counts may be retained after quiescence; missing work is marked incomplete; original error governs. **Resolved.** |
| O: diagnostic merge/field failure | Affected diagnostic unavailable; semantic outcome unchanged. **Resolved.** |
| P: successful query, censored descendant | Success remains success; descendant q-error remains unavailable. **Resolved.** |
| Q: estimate zero, node never starts | No actual `A=0`; `0/0 → 1` is inapplicable. **Resolved.** |
| R: observed prefix equals estimate | Equality does not prove completion; q-error remains unavailable. **Resolved.** |

The Architecture-wide search found **no frozen-owner contradiction** with demand-driven early stop, operator completion, worker quiescence, or Chapter-39 error ownership. The existing *EXPLAIN ANALYZE and Profiling Tests* still say every physical node reports actual rows and q-error without this applicability qualification. That is a **future Verification synchronization obligation**, not authority to weaken the Architecture or edit Verification now.

No new execution state machine, SQL error, transaction state, persisted profile, profile WAL, adaptive statistics feedback, runtime reoptimization, per-row global atomic requirement, or fixed user-facing serialization was introduced. `git diff --check` passed. No frozen chapter, Verification, Development, Project State, source, test, build, devlog, or historical artifact was modified; no build, test, sanitizer, benchmark, staging, or commit occurred.

**Next task:** an independent read-only Chapter-40 Architecture closure audit covering the whole chapter after A1–A3. Do not synchronize Chapter-40 Verification unless that audit closes Architecture.

    CHAPTER 40 FIX A1:
        CLOSED N40-1 / COMMITTED / PRESERVED
    CHAPTER 40 FIX A2:
        CLOSED N40-2 / PRESERVED
    CHAPTER 40 FIX A3:
        CLOSED N40-3
    N40-1 — METRIC IDENTITY:
        CLOSED / UNMODIFIED
    N40-2 — ORDINARY EXPLAIN EXECUTION:
        CLOSED / UNMODIFIED
    N40-3 — PROFILE/Q-ERROR APPLICABILITY:
        CLOSED
    CHAPTER 40 ARCHITECTURE:
        NOT YET INDEPENDENTLY CLOSED
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

END CHAPTER-40 ARCHITECTURE FIX A3.