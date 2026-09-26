**CHAPTER 42 — ARCHITECTURE FIX A2: BENCHMARK-DIMENSION VS FIXTURE-GRID OWNERSHIP — CLOSED P42-2.** Chapter 42 now owns the regimes to measure without freezing four exact benchmark fixture grids. The v1 1024-row default and the exhaustive-to-heuristic transition remain Architecture-owned.

The live baseline differed from the expected one: initial and final HEAD were `f2498903a82a9d668cec6745b9678918827b353b` (`applied FIX-A1 42 in ARCHITECTURE`). A1 was already committed. The worktree and index were clean at entry; the previously reported `FIX-A1.md` deletion was absent. No external change occurred during this task.

The only task change is unstaged [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30732), confined to §§42.2, 42.3, 42.4 and 42.6: **A2 +4/−26**. Relative to the pre-A1 parent, the cumulative A1+A2 Architecture change is **+6/−28**. A1’s opening and constraint-12 wording remain unchanged.

| Location | Final Architecture wording and retained requirement |
|---|---|
| [§42.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30732) | “Group-commit measurement SHOULD include a single-committer baseline and representative multi-committer levels spanning batching and contention.” Existing throughput, latency, sync and WAL metrics remain. The `1,2,4,8,16,32+` fixture grid is no longer Architecture-owned; SHOULD strength remains. |
| [§42.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30748) | Representative parser/AST cases now include “a **wide SELECT**, large VALUES insert, deep Boolean expression, and multi-join query.” No exact 100-column fixture or SQL width limit is implied. |
| [§42.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30766) | “The vector-size study includes representative chunk sizes **below, at, and above the v1 default** on representative workloads.” The exact alternate-size grid is removed; the existing **1024** default remains, as owned by §23.1. Measurements may motivate a later explicit Architecture revision; they do not change the default automatically. |
| [§42.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30864) | Benchmarks use a configured `exhaustive_join_limit` that permits both regimes, with counts near either side and larger heuristic cases. The exact `2…30` grid is removed. The existing requirement to **demonstrate** the exhaustive-bushy to bounded-heuristic transition and record its counters remains. Choosing a configuration that permits both regimes also handles the valid `exhaustive_join_limit=0` case without inventing a new setting. |

These descriptions are sufficient for Verification to select finite, reproducible fixtures later. Existing exact grids in `docs/VERIFICATION.md` were not edited. §12.14 group-commit durability, §18.14 AST lifetime, §23.1 vector capacity, and §§37.11–37.13/38.21 search ownership remain consistent; no configuration mechanism or hard performance target was added.

| Case | A2 classification |
|---|---|
| A. Single/multi/contention group-commit points using `1,4,12,24` | Potentially conforming; old powers-of-two grid not required. |
| B. Group commit measured only with one committer | Insufficient concurrency-regime evidence. |
| C. Vector sizes below/at/above 1024, without the old alternates | Potentially conforming. |
| D. Vector study omits the 1024 comparison point | Insufficient. |
| E. Wide SELECT uses 64 or 200 columns | Potentially valid fixture; Architecture no longer owns 100. |
| F. Join suite adapts to the configured threshold and exercises both regimes | Conforming in principle. |
| G. Old join grid misses the configured transition | Insufficient despite using historical values. |
| H. Execution default changes from 1024 without Architecture revision | Not authorized by A2. |
| I. Parallel worker-count example | Unchanged; P42-4/A4 still owns its review. |

The stale-grid search found the targeted exact grids absent from Chapter 42 and the legitimate 1024 default intact. Arena wording, hot-path constraints 1–2, parallel-scaling wording, and JIT wording are unchanged. Status: **P42-1 and P42-2 closed; P42-3 through P42-6 open/unmodified; P42-7 deferred**.

`git diff --check` passed. The index remained clean and its cached-diff digest remained `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`. No review artifact or file outside the authorized Architecture edit was changed by this task.

CHAPTER 41 ARCHITECTURE: CLEAN — CLOSED
CHAPTER 41 VERIFICATION: CLEAN — CLOSED
CHAPTER 42 ARCHITECTURE: REPAIR IN PROGRESS
P42-1 — TIMELESS PERFORMANCE-EVIDENCE COVERAGE: CLOSED
P42-2 — BENCHMARK FIXTURE-GRID OWNERSHIP: CLOSED
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
REVIEW ARTIFACTS: CLEAN; TASK MODIFIED: NO
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
NEXT AUTHORIZED TASK: **CHAPTER 42 — ARCHITECTURE FIX A3: FRONT-END ALLOCATION AND HOT-PATH CONSTRAINT SCOPE**

END CHAPTER-42 ARCHITECTURE FIX A2 — BENCHMARK-DIMENSION VS FIXTURE-GRID OWNERSHIP.