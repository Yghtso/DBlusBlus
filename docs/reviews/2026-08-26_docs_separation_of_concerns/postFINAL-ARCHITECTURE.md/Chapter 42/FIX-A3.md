CHAPTER 42 — ARCHITECTURE FIX A3: FRONT-END ALLOCATION AND HOT-PATH CONSTRAINT SCOPE

**CLOSED P42-3 AND P42-6.** The only task-created change is an unstaged edit to [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30742). Chapter 42 remains under repair; this is not independent Architecture closure.

The live entry HEAD was `55583df5d255f9192797a8a1f203381d18c5f468` (`applied FIX-A2 42 in ARCHITECTURE`). Contrary to the reported baseline, A2 was already committed: there was no pre-existing Architecture diff. The index was clean, and `FIX-A3.md` was already untracked. None of those facts changed during this task. The A3-only and cumulative *current* Architecture diff are both **+5/−4**; no other file was changed by this task.

In [§42.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30761), the measurements now say “front-end allocation calls and bytes” and “optimizer planning-arena allocations and bytes.” The churn rule now says: “Front-end allocation should limit avoidable per-node general-purpose heap-allocation churn without prescribing the AST representation; benchmark evidence—not guesswork—drives further optimization.” This preserves §18.14’s AST representation freedom and §38.21’s mandatory, measurable optimizer planning arena. It changes no parser, binder, lifetime, or logical-plan semantics.

In [§42.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30814), constraint 1 now prohibits an *inherent one-for-one general-purpose heap allocation* per processed row or cell in steady-state hot execution. Setup, amortized growth, owner-required retained data, and exceptional paths are not thereby forbidden; a standalone allocation for each ordinary hot-path cell remains forbidden. Constraint 2 now prohibits per-row virtual or type-switch **re-dispatch of a choice already resolved at batch/kernel selection**. It permits coarse-boundary dispatch and data-dependent row branches, and mandates neither JIT nor a particular C++ selection mechanism. Both remain strong requirements. No allocation-rate target or benchmark fixture was added.

Owner checks found alignment with §25.3’s resolved-type kernels and §32.11’s batch-level dispatch policy. Constraint 3 and constraints 4–13, including A1’s instrumentation rule and the still-open JIT wording, were unchanged. A1’s opening and every A2 fixture-grid repair were unchanged. The parallel-scaling paragraph was unchanged. Later Verification can measure allocation calls/bytes and planning-arena use, and inspect hot-path allocation and dispatch granularity, without another Architecture representation choice or a procedure being specified here.

The requested negative cases resolve as follows:

| Cases | Disposition |
|---|---|
| A, C | Non-arena AST with valid lifetime/performance, and representation-neutral allocation measurement: permitted/valid. |
| B, E | Missing mandatory optimizer planning arena: nonconforming. An AST-arena mandate does not remain. |
| D | Measured pathological per-node churn: performance concern; an arena is not prescribed as the sole remedy. |
| F–H, K | Setup allocation, amortized growth, valid retained-VARCHAR storage, and diagnostic error-path allocation: permitted outside the one-for-one steady-hot-path prohibition. |
| I–J | Separate ordinary heap allocation for every hot integer cell or batch-representable output row: nonconforming. |
| L, O–R | Batch-selected kernel, chunk-boundary virtual call, NULL/predicate branches, and portable C++ without JIT: permitted. |
| M–N | Per-row type switch or virtual call re-selecting an already-known operation: nonconforming. |

The Architecture search found no competing Chapter-42 AST-arena or old hot-path wording. `git diff --check` passed. Final HEAD is unchanged; the index remains clean with cached-diff digest `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. Final worktree state is the unstaged Architecture edit plus the same pre-existing untracked review artifact. No external change was observed during the task.

    CHAPTER 41 ARCHITECTURE:
        CLEAN — CLOSED

    CHAPTER 41 VERIFICATION:
        CLEAN — CLOSED

    CHAPTER 42 ARCHITECTURE:
        REPAIR IN PROGRESS

    P42-1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE:
        CLOSED

    P42-2 — BENCHMARK FIXTURE-GRID OWNERSHIP:
        CLOSED

    P42-3 — FRONT-END ALLOCATION REPRESENTATION:
        CLOSED

    P42-4 — PARALLEL-SCALING CAPABILITY CONDITION:
        OPEN / UNMODIFIED

    P42-5 — DEFERRED JIT / ADVANCED-CAPABILITY SCOPE:
        OPEN / UNMODIFIED

    P42-6 — HOT-PATH CONSTRAINT SCOPE:
        CLOSED

    P42-7 — VERIFICATION SYNCHRONIZATION:
        DEFERRED UNTIL ARCHITECTURE CLOSES

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
        PRE-EXISTING UNTRACKED FIX-A3.md
        TASK MODIFIED: NO

    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

    NEXT AUTHORIZED TASK:
        CHAPTER 42 — ARCHITECTURE FIX A4:
        PARALLEL-SCALING AND DEFERRED
        ADVANCED-CAPABILITY SCOPE

END CHAPTER-42 ARCHITECTURE FIX A3 —
FRONT-END ALLOCATION AND HOT-PATH
CONSTRAINT SCOPE.