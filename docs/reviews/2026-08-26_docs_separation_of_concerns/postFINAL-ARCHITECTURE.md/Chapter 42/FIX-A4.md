CHAPTER 42 — ARCHITECTURE FIX A4: PARALLEL-SCALING AND DEFERRED ADVANCED-CAPABILITY SCOPE

**CLOSED P42-4 AND P42-5 at repair level.** I changed only the parallel-scaling paragraph and constraint 13 in [§42.4 of docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30804). A1–A3 and constraints 1–12 remain intact. Independent Chapter-42 Architecture closure has **not** been performed.

The repaired scaling rule reads: “For a physical path that supports multi-worker execution, parallel scaling is measured against a single-worker baseline at representative supported multi-worker levels to identify scheduler, synchronization, cache, and memory-bandwidth limits.” Ordinary §42.4 measurement still applies to a single-worker path. A supported multi-worker path requires scaling evidence; a parallel-ready design alone does not trigger that measurement. The `1, 2, 4, 8, ...` fixture grid is gone. No parallel capability, worker configuration, or scaling-efficiency target was added; §32.13 remains the runtime owner.

Constraint 13 now reads: “within the supported Architecture capability scope, profiles—not intuition alone—justify optional explicit SIMD, permitted radix strategies, or explicit prefetch complexity; deferred capabilities such as JIT remain outside v1 absent an explicit Architecture revision.” This preserves the portable C++ baseline, optional SIMD, §32.12’s prefetch safety rule, and the owning limits on radix strategies. Profile evidence can motivate a revision; it cannot itself promote JIT or another deferred feature. No benefit threshold or implementation authorization was added.

The requested cases resolve as follows:

| Cases | Disposition |
|---|---|
| A | No supported multi-worker path: no Chapter-42 scaling evidence required. |
| B | Missing ordinary single-worker performance coverage: insufficient. |
| C, G | Supported multi-worker path measured only at one worker, or declared inapplicable: insufficient/invalid. |
| D | Representative supported counts `1,3,6,12`: potentially conforming; the old grid is not required. |
| E | Unsupported worker count: not required. |
| F | Fast but incorrect parallel result: nonconforming under correctness owners. |
| H | Portable C++ baseline: permitted. |
| I, L | Evidence-backed, in-scope SIMD or permitted radix strategy: potentially conforming, subject to owner rules. |
| J | SIMD complexity added from intuition alone: violates constraint 13. |
| K | Evidence-backed prefetch with unsafe lifetime: nonconforming under the lifetime owner. |
| M, P | JIT or another deferred capability enabled without revision: nonconforming. |
| N | Profile evidence motivating a JIT revision proposal: permitted; not implementation authorization. |
| O | JIT after an explicit future Architecture revision: outside the current v1 baseline; coherent only under that revised contract. |

The parallel and advanced-optimization searches found no conflicting Chapter-42 worker grid or profile-based JIT permission. A full post-edit Chapter 42 read found no new material issue, grammar damage, or regression to A1–A3.

Initial HEAD was `b719a33a756bd98eb9e85625de591c16e5dacf12` (`applied FIX-A3 42 in ARCHITECTURE`); A3 was already committed, the worktree and index were clean, and `FIX-A3.md` was absent at entry. Final HEAD is unchanged. The A4-only and cumulative current Architecture diff are **+2/−8**, unstaged. `git diff --check` passed. The index remains clean; its cached-diff digest stayed `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

An untracked `FIX-A4.md` review artifact **appeared externally during the task**. I did not create, edit, stage, or remove it. No other external state change was observed.

    CHAPTER 41 ARCHITECTURE:
        CLEAN — CLOSED

    CHAPTER 41 VERIFICATION:
        CLEAN — CLOSED

    CHAPTER 42 ARCHITECTURE:
        REPAIRS COMPLETE AT FIX LEVEL —
        INDEPENDENT CLOSURE PENDING

    P42-1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE:
        CLOSED

    P42-2 — BENCHMARK FIXTURE-GRID OWNERSHIP:
        CLOSED

    P42-3 — FRONT-END ALLOCATION REPRESENTATION:
        CLOSED

    P42-4 — PARALLEL-SCALING CAPABILITY CONDITION:
        CLOSED

    P42-5 — DEFERRED JIT / ADVANCED-CAPABILITY SCOPE:
        CLOSED

    P42-6 — HOT-PATH CONSTRAINT SCOPE:
        CLOSED

    P42-7 — VERIFICATION SYNCHRONIZATION:
        DEFERRED UNTIL ARCHITECTURE INDEPENDENTLY CLOSES

    KNOWN CHAPTER-42 ARCHITECTURE FINDINGS:
        0 OPEN AT REPAIR LEVEL

    CHAPTER 42 VERIFICATION:
        NOT SYNCHRONIZED

    INDEPENDENT CHAPTER-42 ARCHITECTURE CLOSURE:
        NOT PERFORMED

    IMPLEMENTATION:
        NOT AUTHORIZED

    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN

    INDEX:
        CLEAN; NO STAGED FILES
        TASK MODIFIED: NO

    REVIEW ARTIFACTS:
        UNTRACKED FIX-A4.md APPEARED EXTERNALLY
        TASK MODIFIED: NO

    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

    NEXT AUTHORIZED TASK:
        CHAPTER 42 — INDEPENDENT READ-ONLY
        ARCHITECTURE CLOSURE AUDIT

END CHAPTER-42 ARCHITECTURE FIX A4 —
PARALLEL-SCALING AND DEFERRED
ADVANCED-CAPABILITY SCOPE.