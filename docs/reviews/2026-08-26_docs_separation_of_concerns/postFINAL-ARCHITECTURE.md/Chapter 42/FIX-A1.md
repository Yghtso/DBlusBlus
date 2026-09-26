**CHAPTER 42 — ARCHITECTURE FIX A1: TIMELESS PERFORMANCE-EVIDENCE COVERAGE — CLOSED P42-1.** The two temporal formulations are now final-state Architecture requirements. A2–A4 remain open; Chapter 42 is not independently closed.

The live baseline differed from the expected state. Initial and final HEAD were `11243749309057c9cc45eb8bf2c74a9afb0d97a6` (`chapter 42 ARCHITECTURE analysis`). The index was clean, with no staged files; its cached-diff digest remained `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. At entry, Chapter-42 `FIX-A1.md` had a pre-existing unstaged deletion of 1,010 lines. It remains untouched. No external state change was observed during this task.

The only task edit is an unstaged **+2/−2** change in [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30658):

- Opening: **“V1 performance evaluation MUST cover at least:”** replaces “The benchmark program must eventually cover at least:”. Coverage can now be checked against the listed families without an unspecified future date or a claim that benchmarks currently exist.
- [§42.4 constraint 12](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30839): **“aggressive optimization remains measurable with applicable Chapter-40 instrumentation/profiling,”** replaces the implementation-order instruction. This states a measurable final property while leaving metric semantics and profile applicability with Chapter 40.

Both requirements can be assessed independently of project chronology. The edit adds no always-on telemetry, public profiling API, benchmark procedure, hard numeric target, current-implementation claim, or implementation authorization. The existing coverage list, micro/end-to-end distinction, residency guidance, and evidence-over-noise SHOULD remain unchanged. Verification retains benchmark methodology; Development retains benchmark build commands; Project State retains implementation reality.

| Negative case | A1 disposition |
|---|---|
| A. Final evaluation omits group commit | Nonconforming with Chapter-42 coverage. |
| B. Subject subsystem is currently unimplemented | Project-State fact, not itself an Architecture contradiction. |
| C. Implementation exists but benchmark procedure is unsynchronized | Documentation/Verification gap, not a runtime error. |
| D. Aggressive optimization cannot be meaningfully measured/profiled through applicable facilities | Violates repaired constraint 12. |
| E. Ordinary production query runs with profiling disabled | Permitted where Chapter 40 permits it. |
| F. Measured performance is slow without an owned numeric threshold | Optimization evidence, not automatic Architecture failure. |
| G. Exact fixture grid differs | Not addressed by A1; P42-2 remains open. |
| H. JIT remains deferred | Unchanged; P42-5 remains open. |

The group-commit, vector-size and join-count grids, 100-column SELECT, arena wording, hot-path constraints 1–2, parallel-scaling wording, and constraint 13’s JIT wording are byte-for-byte unchanged. Finding status: **P42-1 closed; P42-2 through P42-6 open/unmodified; P42-7 deferred until Architecture closes.** `git diff --check` passed. No file outside the authorized Architecture edit was changed by this task.

CHAPTER 41 ARCHITECTURE: CLEAN — CLOSED
CHAPTER 41 VERIFICATION: CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE: REPAIR IN PROGRESS
P42-1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE: CLOSED
P42-2 — BENCHMARK FIXTURE-GRID OWNERSHIP: OPEN / UNMODIFIED
P42-3 — FRONT-END ALLOCATION REPRESENTATION: OPEN / UNMODIFIED
P42-4 — PARALLEL-SCALING CAPABILITY CONDITION: OPEN / UNMODIFIED
P42-5 — DEFERRED JIT / ADVANCED-CAPABILITY SCOPE: OPEN / UNMODIFIED
P42-6 — HOT-PATH CONSTRAINT SCOPE: OPEN / UNMODIFIED
P42-7 — VERIFICATION SYNCHRONIZATION: DEFERRED UNTIL ARCHITECTURE CLOSES
CHAPTER 42 VERIFICATION: NOT SYNCHRONIZED
INDEPENDENT CHAPTER-42 ARCHITECTURE CLOSURE: NOT PERFORMED
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: CLEAN; TASK MODIFIED: NO
REVIEW ARTIFACTS: CHAPTER-42 `FIX-A1.md` HAS A PRE-EXISTING UNSTAGED DELETION; TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
NEXT AUTHORIZED TASK: **CHAPTER 42 — ARCHITECTURE FIX A2: BENCHMARK-DIMENSION VS FIXTURE-GRID OWNERSHIP**

END CHAPTER-42 ARCHITECTURE FIX A1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE.