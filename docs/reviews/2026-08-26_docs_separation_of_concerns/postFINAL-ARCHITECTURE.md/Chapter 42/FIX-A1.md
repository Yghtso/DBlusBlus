CHAPTER 42 — ARCHITECTURE FIX A1  
TIMELESS PERFORMANCE-EVIDENCE COVERAGE  
CLOSE P42-1 ONLY

Perform the first narrowly scoped Chapter-42 Architecture repair.

This is an ARCHITECTURE-ONLY documentation edit.

The initial Chapter-42 analysis concluded:

    CHAPTER 42 ARCHITECTURE:
        NEEDS ARCHITECTURE FIX

with ordered repairs:

    A1:
        timeless performance-evidence coverage
        (P42-1)

    A2:
        benchmark-fixture/grid ownership
        (P42-2)

    A3:
        representation/hot-path scope
        (P42-3 / P42-6)

    A4:
        parallel/deferred-capability wording
        (P42-4 / P42-5)

This task performs ONLY A1.

Do NOT opportunistically fix A2–A4.

Do NOT synchronize Verification.

Do NOT implement.

============================================================
1. FINDING TO CLOSE
============================================================

The initial analysis identified:

    P42-1 — MAJOR

Primary location:

    Chapter 42 opening

Live wording reportedly includes:

    "The benchmark program must eventually cover at least:"

Problem:

    "eventually" makes an otherwise mandatory final
    Architecture obligation temporally open-ended and
    indefinitely postponable;

    "benchmark program" can also sound like development
    sequencing rather than a timeless conformance/evidence
    requirement.

The same temporal tendency appears in §42.4 strong constraint
12:

    "instrumentation exists before aggressive optimization"

Problem:

    "before" describes implementation chronology rather than
    the final architectural property.

A1 must make both requirements timeless and falsifiable.

============================================================
2. AUTHORIZED EDIT
============================================================

EDIT ONLY:

    docs/ARCHITECTURE.md

Target ONLY:

    Chapter 42 opening performance-evidence coverage wording

and:

    §42.4 strong implementation constraint 12

Do NOT edit:

    docs/VERIFICATION.md
    docs/DEVELOPMENT.md
    docs/PROJECT_STATE.md

Do NOT modify:

    source;
    tests;
    benchmarks;
    CMake;
    devlogs;
    review artifacts.

Do NOT:

    implement;
    build;
    run tests;
    run sanitizers;
    run benchmarks;
    stage;
    unstage;
    commit;
    amend;
    reset;
    restore;
    clean;
    stash.

============================================================
3. EXPECTED LIVE REPOSITORY STATE
============================================================

The initial Chapter-42 audit reported:

    HEAD:
        bab7ba110e0baa27a37c5e683d77a90091679d01

    commit:
        synced VERIFICATION after chapter 41 ARCHITECTURE fix

    staged:
        docs/VERIFICATION.md
        Chapter-41 POST-SYNC.md

    staged aggregate:
        +193 / -39

    staged-diff digest:
        eac5f95f473c42a117ae20a17372290394b4463a01aeb03ad6c53638d03e2202

    unstaged tracked:
        none

Use ACTUAL live state.

At entry run:

    git rev-parse HEAD
    git log -1 --oneline
    git status --short
    git diff --cached --name-only
    git diff --cached --stat
    git diff --cached | sha256sum
    git diff --stat
    git diff --check

Preserve all pre-existing staged content exactly.

A1 should create only:

    an UNSTAGED docs/ARCHITECTURE.md edit.

Do NOT stage it.

============================================================
4. EXTERNAL INDEX CHANGES
============================================================

Previous tasks observed another actor changing repository
state.

Therefore:

    treat the existing index as externally owned.

Do NOT normalize it.

If staged content changes externally during this task:

    record the change;
    do not undo it;
    distinguish it from A1.

A1 itself must not modify the index.

============================================================
5. READ LIVE CONTEXT BEFORE EDITING
============================================================

Read in full:

    Chapter-42 opening;
    §42.4;
    Chapter-40 observability/profiling owner;
    Architecture front-matter document-role language.

Also inspect:

    VERIFICATION.md purpose/authority;
    DEVELOPMENT.md benchmark-build role;

only to confirm ownership boundaries.

Do NOT edit those documents.

============================================================
6. PRESERVE THE VALID CHAPTER-42 DESIGN
============================================================

The initial analysis found NO need to add:

    TPS thresholds;
    latency thresholds;
    recovery-time thresholds;
    throughput minimums;
    regression percentages.

Do NOT invent any.

Preserve:

    "Performance claims require measurement."

Preserve the existing benchmark-coverage dimensions.

Preserve the microbenchmark/end-to-end distinction.

Preserve storage residency/resource-pressure distinctions.

Preserve:

    performance-sensitive changes require evidence rather than
    one noisy measurement or intuition alone.

A1 changes temporal ownership wording only.

============================================================
7. OPENING REPAIR — SEMANTIC TARGET
============================================================

Replace the open-ended sequencing formulation:

    "benchmark program must eventually cover at least"

with a timeless Architecture requirement.

The final meaning should be equivalent to:

    the v1 performance-evaluation/benchmark evidence set
    MUST cover at least the listed mechanism families.

Preferred semantic shape:

    "V1 performance evaluation MUST cover at least:"

or:

    "The v1 performance-evaluation suite MUST cover at least:"

Choose the wording that best fits live prose.

The key properties are:

A.
    mandatory final-state coverage;

B.
    no "eventually";

C.
    no milestone/project-timeline condition;

D.
    no claim that benchmarks currently exist;

E.
    no requirement that they currently pass;

F.
    no implementation authorization.

============================================================
8. “BENCHMARK PROGRAM” TERMINOLOGY
============================================================

Independently decide whether:

    "benchmark program"

should remain.

If it means:

    benchmark suite/evidence coverage

and is clearly timeless:

    it MAY remain.

If it sounds like:

    project roadmap;
    implementation phase;
    future work program;

replace it with a stable concept such as:

    performance evaluation;
    benchmark coverage;
    performance-evidence suite.

Do not introduce a new subsystem named:

    BenchmarkProgram.

============================================================
9. FINAL-STATE REQUIREMENT
============================================================

The repaired opening must be falsifiable in the final
Architecture state.

A conforming final performance-evaluation specification should
be able to answer:

    Does it cover each mandatory listed mechanism family?

The answer must not depend on:

    how far development has progressed;
    whether implementation is currently authorized;
    an unspecified future date.

============================================================
10. IMPLEMENTATION STATUS REMAINS PROJECT_STATE-OWNED
============================================================

Do NOT add language such as:

    once subsystem X is implemented;
    before release;
    during milestone Y;
    later;
    eventually.

Architecture defines the final contract.

PROJECT_STATE owns whether the implementation currently
supports the required benchmark subject.

============================================================
11. VERIFICATION OWNS BENCHMARK PROCEDURE
============================================================

The repaired opening must NOT specify:

    command line;
    benchmark framework;
    trial count;
    warmup;
    exact fixture;
    dataset generator;
    environment capture;
    repetition count;
    confidence interval.

Those remain future Verification synchronization concerns.

A1 defines:

    WHAT performance areas require evaluation,

not:

    HOW to run each benchmark.

============================================================
12. §42.4 CONSTRAINT 12 — CURRENT ISSUE
============================================================

The live strong constraint reportedly says:

    "instrumentation exists before aggressive optimization"

This has a legitimate underlying intent:

    aggressive performance work must remain measurable and
    evidence-driven using the Chapter-40 observability/profile
    mechanisms.

But:

    "exists before"

describes development chronology.

Repair the sentence into a timeless architectural constraint.

============================================================
13. §42.4 CONSTRAINT 12 — REQUIRED MEANING
============================================================

The resulting rule should mean, in substance:

    aggressive/performance-sensitive optimization must remain
    measurable or profilable through the applicable
    Chapter-40 instrumentation/profile facilities.

A good semantic shape is:

    "aggressive optimization remains measurable with
     applicable Chapter-40 instrumentation/profiling"

or equivalent.

This states a final property.

Do not prescribe implementation order.

============================================================
14. DO NOT CREATE ALWAYS-ON TELEMETRY
============================================================

The new wording must NOT imply:

    every metric always enabled;

    per-row tracing;

    persistent profiling;

    remote telemetry;

    production profiling overhead regardless of request.

Chapter 40's existing applicability and overhead boundaries
remain canonical.

Chapter 42 consumes those facilities only as appropriate for
performance evidence.

============================================================
15. DO NOT REDEFINE CHAPTER 40
============================================================

Do NOT define:

    metric identity;
    counter semantics;
    profile applicability;
    EXPLAIN ANALYZE behavior;
    q-error;
    trace lifetime

inside Chapter 42.

Chapter 40 remains the owner.

Use a cross-owner reference/term only as needed.

============================================================
16. DO NOT OVERSTRENGTHEN THE RULE
============================================================

The original strong list establishes an important architectural
constraint.

But A1 must not accidentally require:

    every optimization to have a dedicated new counter;

    every micro-optimization to expose public profiling;

    a stable debug API;

    proof of speedup inside production runtime.

The required property is:

    applicable measurement/profile evidence can evaluate the
    performance-sensitive path.

============================================================
17. CONSTRAINT 13 REMAINS UNCHANGED
============================================================

Do NOT edit strong constraint 13 in A1:

    profiles—not intuition alone—justify
    SIMD/radix/JIT/prefetch complexity.

Its deferred-JIT issue belongs to:

    P42-5 / A4.

Do not fix it early.

============================================================
18. EXACT FIXTURE GRIDS REMAIN UNCHANGED
============================================================

Do NOT edit:

    group-commit:
        1,2,4,8,16,32+

    vector study:
        256,512,1024,2048,4096

    join search:
        2,4,6,8,10,12,16,20,30

    100-column SELECT

Those belong to:

    P42-2 / A2.

A1 must leave them byte-for-byte unchanged.

============================================================
19. ARENA WORDING REMAINS UNCHANGED
============================================================

Do NOT edit:

    AST/plan arena;
    Arena-based ownership...

Those belong to:

    P42-3 / A3.

============================================================
20. HOT-PATH CONSTRAINTS 1–2 REMAIN UNCHANGED
============================================================

Do NOT repair:

    literal per-row/cell allocation wording;

    "batch specialization is available"

in A1.

Those belong to:

    P42-6 / A3.

============================================================
21. PARALLEL-SCALING WORDING REMAINS UNCHANGED
============================================================

Do NOT repair unconditional parallel-scaling wording.

That belongs to:

    P42-4 / A4.

============================================================
22. JIT WORDING REMAINS UNCHANGED
============================================================

Do NOT repair JIT/deferred-feature qualification.

That belongs to:

    P42-5 / A4.

============================================================
23. PERFORMANCE CLAIMS VS CONFORMANCE
============================================================

A1 must preserve the initial analysis distinction:

    missing required final benchmark coverage
        ->
    Architecture-evidence/conformance problem

whereas:

    measuring poor performance with no owned numeric threshold
        ->
    evidence for optimization work,
    not automatically runtime/database nonconformance.

Do not add numeric acceptance criteria.

============================================================
24. CORRECTNESS REMAINS PREREQUISITE
============================================================

Do not weaken correctness.

A measured result is meaningful only for behavior satisfying
the owning correctness semantics.

No Chapter-42 wording may permit:

    faster but incorrect query result;
    faster but non-durable commit;
    faster but invalid recovery.

No extra prose is required if live architecture already makes
this clear.

============================================================
25. MEASUREMENT EVIDENCE REMAINS TIMELESS
============================================================

Search the repaired Chapter 42 for:

    eventually
    before
    later
    future
    once implemented
    when implementation reaches
    milestone
    phase

Evaluate only A1-related temporal performance-evidence
language.

Do NOT globally rewrite legitimate conditional capability
language.

A1 closes P42-1 only.

============================================================
26. CHAPTER-40 CROSS-CHECK
============================================================

Confirm repaired constraint 12 is compatible with Chapter 40.

Specifically:

    applicable profiling may be optional/requested;

    metric identity remains Chapter-40-owned;

    profiling need not run for every query;

    low-overhead/noninterference rules remain intact.

No contradiction.

============================================================
27. VERIFICATION ROLE CROSS-CHECK
============================================================

Confirm future VERIFICATION.md can derive concrete procedures
from the repaired opening without another Architecture design
choice.

It should be able to interpret:

    mandatory benchmark area coverage

and independently choose:

    fixtures;
    repetition;
    environment;
    exact benchmark commands.

That is the intended role split.

============================================================
28. DEVELOPMENT ROLE CROSS-CHECK
============================================================

Confirm DEVELOPMENT.md remains sole owner of:

    benchmark build preset;
    build command;
    executable location;
    development invocation guidance.

Do not reference:

    clang-bench;
    Google Benchmark;
    CMake

in the Chapter-42 repair.

============================================================
29. PROJECT_STATE CROSS-CHECK
============================================================

The final Architecture wording must not imply:

    these benchmarks currently exist;
    their subjects are implemented;
    they have been run.

PROJECT_STATE remains implementation reality.

============================================================
30. DOCUMENTATION-TEMPORALITY CHECK
============================================================

After editing, answer:

A.
    Is the opening a final-state requirement rather than a
    roadmap item?

B.
    Is constraint 12 a final measurable property rather than
    an implementation-order instruction?

C.
    Can both be true/false independent of project chronology?

All must be:

    YES.

============================================================
31. NEGATIVE SEMANTIC MATRIX
============================================================

Classify after A1:

A.
    final benchmark/evaluation coverage omits group commit:
        NONCONFORMING WITH CHAPTER-42 COVERAGE REQUIREMENT.

B.
    subsystem is currently unimplemented:
        PROJECT_STATE FACT;
        NOT BY ITSELF AN ARCHITECTURE CONTRADICTION.

C.
    implementation exists but benchmark methodology is not yet
    synchronized:
        DOCUMENTATION/VERIFICATION GAP;
        NOT A NEW RUNTIME ERROR.

D.
    optimization cannot be meaningfully profiled/measured with
    applicable observability:
        VIOLATES REPAIRED PERFORMANCE-EVIDENCE CONSTRAINT
        if within the rule's scope.

E.
    profiling is disabled during ordinary production query:
        PERMITTED where Chapter 40 permits it.

F.
    performance result is slow but no Architecture threshold
    exists:
        MEASUREMENT RESULT / OPTIMIZATION EVIDENCE;
        NOT AUTOMATIC ARCHITECTURE FAILURE.

G.
    exact fixture grid differs:
        NOT ADDRESSED BY A1;
        P42-2 REMAINS OPEN.

H.
    JIT remains deferred:
        UNCHANGED;
        P42-5 REMAINS OPEN.

No A1-required row may remain ambiguous.

============================================================
32. SCOPE DISCIPLINE
============================================================

Expected A1 edit should be extremely small.

Preferred:

    one opening-line repair;

    one §42.4 constraint-12 repair.

Do not reflow unrelated paragraphs.

Do not perform A2/A3/A4 wording cleanup.

============================================================
33. DIFF REVIEW
============================================================

After editing inspect:

    git diff -- docs/ARCHITECTURE.md
    git diff --cached -- docs/ARCHITECTURE.md
    git diff --cached --name-only
    git diff --cached --stat
    git diff --cached | sha256sum
    git diff --stat
    git diff --check
    git status --short

Confirm:

    A1 changed only docs/ARCHITECTURE.md;

    A1 is unstaged;

    pre-existing staged docs/VERIFICATION.md unchanged;

    pre-existing staged POST-SYNC.md unchanged;

    cached diff digest unchanged unless an external actor
    changed the index;

    no other task-created changes.

============================================================
34. A1 CLOSURE STANDARD
============================================================

Report:

    CLOSED P42-1

only if both temporal defects are resolved:

1.
    mandatory performance-evaluation coverage is expressed as
    a timeless final Architecture requirement;

2.
    aggressive optimization's instrumentation dependency is
    expressed as timeless measurability/profileability rather
    than "instrumentation first" project chronology.

A1 does NOT require:

    P42-2;
    P42-3;
    P42-4;
    P42-5;
    P42-6

to be closed.

============================================================
35. REQUIRED FINAL REPORT
============================================================

Return:

1. Fix identifier:

       CHAPTER 42 — ARCHITECTURE FIX A1:
       TIMELESS PERFORMANCE-EVIDENCE COVERAGE

2. Result:

       CLOSED P42-1

   or:

       P42-1 REMAINS OPEN

3. Initial HEAD/commit.

4. Initial worktree state.

5. Initial index/staged-file state.

6. Initial cached-diff digest.

7. Any external repository-state change.

8. Final HEAD/worktree/index.

9. Exact file changed.

10. Exact Chapter-42 locations changed.

11. A1 insertion/deletion count.

12. Exact repaired opening wording.

13. Why it is timeless.

14. Exact repaired §42.4 constraint-12 wording.

15. Why it is timeless.

16. Chapter-40 compatibility.

17. Confirmation no always-on telemetry was introduced.

18. Confirmation no current implementation claim was added.

19. Confirmation no benchmark procedure migrated into
    Architecture.

20. Confirmation no hard numeric target was added.

21. Negative matrix A–H.

22. Confirmation exact fixture grids unchanged.

23. Confirmation arena wording unchanged.

24. Confirmation constraints 1–2 unchanged.

25. Confirmation parallel wording unchanged.

26. Confirmation JIT wording unchanged.

27. Remaining P42 finding statuses:
       P42-1
       P42-2
       P42-3
       P42-4
       P42-5
       P42-6
       P42-7

28. `git diff --check` result.

29. Final cached-diff digest/index preservation result.

30. Audit-created changes outside authorized edit.

31. Exact next authorized task.

If P42-1 closes, recommend EXACTLY:

    CHAPTER 42 — ARCHITECTURE FIX A2:
    BENCHMARK-DIMENSION VS FIXTURE-GRID OWNERSHIP

Do NOT recommend an independent closure audit yet.

A2–A4 remain prerequisite.

============================================================
36. REQUIRED TERMINAL STATUS
============================================================

End with:

    CHAPTER 41 ARCHITECTURE:
        CLEAN — CLOSED

    CHAPTER 41 VERIFICATION:
        CLEAN — CLOSED

    CHAPTER 42 ARCHITECTURE:
        REPAIR IN PROGRESS

    P42-1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE:
        [CLOSED / OPEN]

    P42-2 — BENCHMARK FIXTURE-GRID OWNERSHIP:
        OPEN / UNMODIFIED

    P42-3 — FRONT-END ALLOCATION REPRESENTATION:
        OPEN / UNMODIFIED

    P42-4 — PARALLEL-SCALING CAPABILITY CONDITION:
        OPEN / UNMODIFIED

    P42-5 — DEFERRED JIT / ADVANCED-CAPABILITY SCOPE:
        OPEN / UNMODIFIED

    P42-6 — HOT-PATH CONSTRAINT SCOPE:
        OPEN / UNMODIFIED

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
        [EXACT FINAL STATE]
        TASK MODIFIED: NO

    REVIEW ARTIFACTS:
        [EXACT FINAL STATE]
        TASK MODIFIED: NO

    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

    NEXT AUTHORIZED TASK:
        [EXACT TASK]

Do not commit.

END CHAPTER-42 ARCHITECTURE FIX A1 —
TIMELESS PERFORMANCE-EVIDENCE COVERAGE.