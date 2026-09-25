**CHAPTER 40 — VERIFICATION SYNC FIX 1: CLOSED V40-G6.** The two stale assertions in [EXPLAIN ANALYZE and Profiling Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25479) now follow the closed Chapter-40 Architecture. This is a focused repair, not Chapter-40 Verification closure.

The first assertion previously required estimated rows, actual rows, and q-error for *every* physical node. [The repaired assertion](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25492) retains the estimate for every selected node, checks per-node applicability against direct operator start/completion evidence, and requires actual rows only when observed. Q-error—including the completed zero/infinity cases—applies only to comparable cardinality at the same output boundary. Not-started nodes cannot acquire a fabricated actual zero; incomplete or demand-censored nodes cannot acquire fabricated numeric q-error.

The second assertion previously required a failed or cancelled profile unconditionally. [The repaired failure procedure](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25510) makes profile checks conditional on exposure through a diagnostic, debug, or test interface. When exposed, it must identify unsuccessful/incomplete execution and contain only safely observed work. The original error or cancellation remains primary. Cleanup still requires no live profiling registration or runnable execution work. No error-plus-profile wire format was prescribed.

Self-review: completed 100/100 and genuinely completed 0/0 retain q-error 1; a never-started node has no actual zero or q-error; a LIMIT-censored child may retain observed work without comparable q-error; failure with an exposed partial profile is checked; failure without one remains permissible while error and cleanup checks still apply. The local and global wording searches found no further stale Chapter-40 assertion; estimator-quality tests using known reference counts remain valid.

Repository check: initial and final HEAD are `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). The initial worktree and index were clean. The final worktree contains only `M docs/VERIFICATION.md`; the index remains clean. The edit is **13 insertions, 7 deletions**. `git diff --check` passed. Architecture, Development, Project State, source, tests, build files, and benchmarks were unchanged; no V40 procedure ID was added.

The next recommended task is **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 2: A1 METRIC ACCOUNTING INTEGRATION (V40-G1 + V40-G2)**. It was not started here.

    CHAPTER 40 ARCHITECTURE:
        CLEAN — CLOSED / UNMODIFIED
    CHAPTER 40 VERIFICATION SYNC FIX 1:
        CLOSED V40-G6
    V40-G1 — FETCH / PHYSICAL-I/O METRICS:
        OPEN / UNMODIFIED
    V40-G2 — WAL / TX / GAUGE / OVERFLOW:
        OPEN / UNMODIFIED
    V40-G3 — EXPLAIN EXECUTION BOUNDARY:
        OPEN / UNMODIFIED
    V40-G4 — PROFILE / Q-ERROR APPLICABILITY:
        OPEN / UNMODIFIED
    V40-G5 — OPTIMIZER / NONINTERFERENCE:
        OPEN / UNMODIFIED
    V40-G6 — STALE ASSERTIONS:
        CLOSED
    V40-G7 — BENCHMARK FOLLOW-UP:
        DEFERRED / UNMODIFIED
    V40 PROCEDURES:
        NONE ADDED BY THIS TASK
    IMPLEMENTATION:
        NOT AUTHORIZED
    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN
    INDEX:
        UNMODIFIED
    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 1.

**CHAPTER 40 — VERIFICATION SYNC FIX 2A: CLOSED V40-G1 at the procedure-repair level.** The new [V40-A — BufferPool and page-I/O metric accounting](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26423) adds V40-001–V40-010. Each procedure compares a Chapter-40 metric delta with independent BufferPool state/claim/publication evidence and DiskManager transfer evidence; the metric is never its own oracle. The procedures were written, not executed.

| Procedures | Owner evidence and metric assertion |
|---|---|
| V40-001–003 | Public pins, load publication, registered claims, and full-page transfers distinguish resident hit `L+1 H+1 M+0 P+0`, simple miss `L+1 H+0 M+1 P+1`, and two coalesced claims `L+2 H+0 M+2 P+1`. |
| V40-004–005 | A completed transfer followed by validation failure gives `P+1` but no completed logical fetch; a partial/failed transfer gives `P+0`. Pre-admission rejection also gives no logical fetch. |
| V40-006–008 | Dirtying or copying gives `W+0`; completed full-page `pwrite` gives `W+1` before `fdatasync`. Short/failed write gives `W+0`; later sync failure does not erase a completed transfer or imply stable writeback. |
| V40-009–010 | An eviction-race retry counts one eventual completed caller claim, classified by its final pin path. New-page construction invents no physical read. |

These cover the requested A–J faulty implementations: erroneous hit/miss or coalesced accounting, suppressed completed reads, counted partial reads, dirtying counted as writing, write counts delayed until sync, retry double-counting, and fabricated new-page reads. The section uses before/after snapshots in an identified owner/unit/scope/lifetime, not assumed process-start zero. Missing owner evidence, mandatory metric evidence, fault activation, or publication evidence is explicitly non-PASS. Its page-family metadata check does **not** claim coverage for G2’s other metric families.

The reused owners are the existing unnumbered [Disk tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1273) and [Buffer management verification](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:1283), including their deterministic load/waiter, I/O fault, validation, eviction, new-page, and copied-writeback fixtures. The global consistency search found no conflicting Chapter-40 diagnostic definition or new stale assertion; other page-write references concern storage correctness or durability at different boundaries. No Architecture contradiction was found.

Repository state: initial and final HEAD are `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Fix 1 was uncommitted and remains intact. Fix 2A added **40 lines, 0 deletions**; the total `docs/VERIFICATION.md` worktree diff, including Fix 1, is **53 insertions, 7 deletions**. The index is clean and `git diff --check` passed. A pre-existing untracked `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 40/SYNC.md` was preserved untouched. No file outside `docs/VERIFICATION.md` was changed by this task.

Next recommended task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 2B: WAL / TRANSACTION / GAUGE / OVERFLOW-UNAVAILABLE METRIC INTEGRATION (V40-G2).**

    CHAPTER 40 ARCHITECTURE:
        CLEAN — CLOSED / UNMODIFIED
    CHAPTER 40 VERIFICATION SYNC FIX 1:
        CLOSED V40-G6 / PRESERVED
    CHAPTER 40 VERIFICATION SYNC FIX 2A:
        CLOSED V40-G1
    V40-G1 — FETCH / PHYSICAL-I/O METRICS:
        CLOSED
    V40-G2 — WAL / TX / GAUGE / OVERFLOW:
        OPEN / UNMODIFIED
    V40-G3 — EXPLAIN EXECUTION BOUNDARY:
        OPEN / UNMODIFIED
    V40-G4 — PROFILE / Q-ERROR APPLICABILITY:
        OPEN / UNMODIFIED
    V40-G5 — OPTIMIZER / NONINTERFERENCE:
        OPEN / UNMODIFIED
    V40-G6 — STALE ASSERTIONS:
        CLOSED / PRESERVED
    V40-G7 — BENCHMARK FOLLOW-UP:
        DEFERRED / UNMODIFIED
    V40 PROCEDURES:
        V40-001–V40-010 ADDED
    IMPLEMENTATION:
        NOT AUTHORIZED
    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN
    INDEX:
        UNMODIFIED
    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 2A.

**CHAPTER 40 — VERIFICATION SYNC FIX 2B1: CLOSED V40-G2 EVENT-BOUNDARY SUBSCOPE at the procedure-repair level.** V40-G2 remains open for Fix 2B2; Chapter-40 Verification is not closed.

The new [V40-B — WAL and semantic transaction event-metric accounting](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26464) contains contiguous **V40-011–V40-022**. It compares reported deltas over an identified scope with independent WAL-prefix, flusher, transaction-registration, and terminal-outcome evidence. Missing owner evidence or an untriggered fault is non-PASS.

| Procedures | Independent owner oracle and required comparison |
|---|---|
| V40-011–012 | Valid published append bytes—including padding—are separate from newly durable valid-prefix bytes. Reservations do not count; append before durability leaves synced bytes unchanged. A reached WAL-flusher attempt counts once, not once per syscall or waiter. |
| V40-013–014 | Five identified waiters share one proven flush: one attempt, one successful batch of size five, and newly durable bytes counted once. An injected failed attempt gives no false batch or durability; any successful retry is separately counted. |
| V40-015–017 | Successful §9 admission counts one begin; rejected pre-admission work counts none. Persistent COMMIT counts once at durable C3, and read-only COMMIT once at authoritative C4—without inventing read-only commit WAL. C6 success adds no second commit. |
| V40-018–019 | C6 loss and post-durable failure leave semantic COMMITTED counted and ABORTED uncounted. Uncertainty, connection, completion-failure, and noncontinuable events retain separate owner evidence. |
| V40-020–022 | Explicit and automatic ABORT count once at A2, not at MUST_ABORT, ABORTING, A3 cleanup, A4 delivery, or returned error. An identity-correlated comparison keeps terminal outcome distinct from cleanup and client knowledge. |

The procedures reuse the live **WAL Persistent Codec, Append, and Recovery Verification**, **Non-Crash WAL/MTR Failure Injection**, **Transaction identity, snapshot, and status verification**, **COMMIT Fault-Injection Tests**, **ABORT Fault-Injection Tests**, and the cited V39 procedures—particularly V39-008, V39-026, V39-029–031, V39-033, V39-035, V39-038, and V39-040–044. They add metric comparisons, not replacement protocol tests. The specified negative implementations A–L are falsifiable through these exact event/delta boundaries. The global consistency search found no new stale diagnostic assertion or Architecture contradiction.

Initial and final HEAD: `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Fix 1 and Fix 2A were uncommitted and remain intact; the index was and remains clean. Fix 2B1 added **46 lines, 0 deletions** in `docs/VERIFICATION.md` only. The total worktree diff, including prior repairs, is **99 insertions, 7 deletions**. The pre-existing untracked `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 40/SYNC.md` was left untouched. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

Next recommended task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 2B2: SAMPLED GAUGES / LIFECYCLE / RECOVERY / VACUUM / OVERFLOW-UNAVAILABLE METRIC INTEGRATION.**

    CHAPTER 40 ARCHITECTURE:
        CLEAN — CLOSED / UNMODIFIED
    VERIFICATION SYNC FIX 1:
        CLOSED V40-G6 / PRESERVED
    VERIFICATION SYNC FIX 2A:
        CLOSED V40-G1 / PRESERVED
    VERIFICATION SYNC FIX 2B1:
        EVENT-BOUNDARY SUBSCOPE CLOSED
    V40-G1 — FETCH / PHYSICAL-I/O METRICS:
        CLOSED / PRESERVED
    V40-G2 — WAL / TX / GAUGE / OVERFLOW:
        OPEN — EVENT SUBSCOPE CLOSED,
        GAUGE/DIAGNOSTIC SUBSCOPE NOT YET REPAIRED
    V40-G3 — EXPLAIN EXECUTION BOUNDARY:
        OPEN / UNMODIFIED
    V40-G4 — PROFILE / Q-ERROR APPLICABILITY:
        OPEN / UNMODIFIED
    V40-G5 — OPTIMIZER / NONINTERFERENCE:
        OPEN / UNMODIFIED
    V40-G6 — STALE ASSERTIONS:
        CLOSED / PRESERVED
    V40-G7 — BENCHMARK FOLLOW-UP:
        DEFERRED / UNMODIFIED
    V40 PROCEDURES:
        V40-001–V40-022
    IMPLEMENTATION:
        NOT AUTHORIZED
    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN
    INDEX:
        UNMODIFIED
    PRE-EXISTING UNTRACKED REVIEW ARTIFACT:
        PRESERVED / UNMODIFIED
    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 2B1.

**CHAPTER 40 — VERIFICATION SYNC FIX 2B2: CLOSED V40-G2 at the procedure-repair level.** The event-boundary procedures from Fix 2B1 remain intact. The new [V40-C — Sampled state, maintenance, recovery, and diagnostic degradation](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26508) adds contiguous **V40-023–V40-038** for the remaining G2 obligations. Chapter-40 Verification as a whole is not closed, and none of these procedures was executed.

| New procedures | Independent owner oracle and diagnostic check |
|---|---|
| V40-023–026 | Transaction state, SnapshotManager, DPT, and ReadEpochManager membership at controlled barriers are compared with sampled gauges. This distinguishes current state from lifetime totals. The absent oldest-snapshot `xmin` is kept distinct from the vacuum horizon’s no-snapshot `next_txn_id` fallback. |
| V40-027–029 | Lifecycle transitions, operation failures, checkpoint completion, and stage durations are checked against lifecycle/control/checkpoint owner events. Durations are checked for attribution and validity, never exact elapsed nanoseconds. |
| V40-030–032 | Known WAL/recovery fixtures independently establish records scanned, applied page redo, restored full-page images, crash losers, and the checkpoint redo bound. Scan work is not inferred from redo work; loser count is not inferred from live abort totals. |
| V40-033–035 | Direct vacuum actions establish examined/reclaimed/removed/frozen work. Snapshot and maintenance owners establish exact horizons versus approximate pressure. Existing statement, lock, isolation, and V39 fixtures supply the remaining §40.3 failure, retry, wait, deadlock, and serialization event oracles. |
| V40-036–038 | A controlled representation limit rejects silent wrap and diagnostic-caused semantic failure; optional absence and partial collection loss are explicit, while missing mandatory core metrics are non-PASS. Independently sampled gauges need not form a global atomic snapshot. |

The self-review cases A–P and negative implementations A–O each have a corresponding falsifiable row: intermediate-state gauges (V40-023), snapshots (024), DPT (025), RID queue (026), lifecycle (027–028), checkpoint (029), recovery (030–032), vacuum/approximation (033–034), saturation and semantic noninterference (036), optional versus mandatory availability (037), and independent gauge sampling (038). No remaining mandatory **G2** gauge/diagnostic obligation was found without an owner oracle. The global consistency search found no new stale Chapter-40 assertion or Architecture contradiction.

Initial and final HEAD: `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Fix 1, Fix 2A, and Fix 2B1 were uncommitted and remain preserved. Fix 2B2 added **42 lines, 0 deletions** in `docs/VERIFICATION.md` only. The total current Verification worktree diff is **141 insertions, 7 deletions**. The index is clean; `git diff --check` passed. The pre-existing untracked `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 40/SYNC.md` was left untouched. No Architecture, Development, Project State, source, test, build, or benchmark file was changed.

Next recommended task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 3: ORDINARY EXPLAIN NONEXECUTION / EXPLAIN ANALYZE EXECUTION HANDOFF (V40-G3).**

    CHAPTER 40 ARCHITECTURE:
        CLEAN — CLOSED / UNMODIFIED
    VERIFICATION SYNC FIX 1:
        CLOSED V40-G6 / PRESERVED
    VERIFICATION SYNC FIX 2A:
        CLOSED V40-G1 / PRESERVED
    VERIFICATION SYNC FIX 2B1:
        EVENT-BOUNDARY SUBSCOPE CLOSED / PRESERVED
    VERIFICATION SYNC FIX 2B2:
        CLOSED REMAINING G2 SUBSCOPE
    V40-G1 — FETCH / PHYSICAL-I/O METRICS:
        CLOSED / PRESERVED
    V40-G2 — WAL / TX / GAUGE / OVERFLOW:
        CLOSED
    V40-G3 — EXPLAIN EXECUTION BOUNDARY:
        OPEN / UNMODIFIED
    V40-G4 — PROFILE / Q-ERROR APPLICABILITY:
        OPEN / UNMODIFIED
    V40-G5 — OPTIMIZER / NONINTERFERENCE:
        OPEN / UNMODIFIED
    V40-G6 — STALE ASSERTIONS:
        CLOSED / PRESERVED
    V40-G7 — BENCHMARK FOLLOW-UP:
        DEFERRED / UNMODIFIED
    V40 PROCEDURES:
        V40-001–V40-038
    IMPLEMENTATION:
        NOT AUTHORIZED
    BUILD/TEST/SANITIZER/BENCHMARK:
        NOT RUN
    INDEX:
        UNMODIFIED
    PRE-EXISTING UNTRACKED REVIEW ARTIFACT:
        PRESERVED / UNMODIFIED
    AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
        NONE

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 2B2.

**CHAPTER 40 — VERIFICATION SYNC FIX 3: CLOSED V40-G3.**

Added [V40-D — EXPLAIN execution boundary and stage-owned failures](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26550), with contiguous procedures **V40-039–V40-050**. The procedures compare EXPLAIN behavior with independent operator, task, scan, memory, spill, planning, and validation evidence—not with EXPLAIN text as its own oracle. This is a documentation repair; the procedures were not executed.

| Check | Verification coverage added |
|---|---|
| Ordinary physical EXPLAIN | V40-039 proves planning and estimates without inner-SELECT operator starts, executing tasks, execution memory, or spill. Discarding executed rows cannot pass. |
| Matching ANALYZE and plan handoff | V40-040 proves physical execution and compares the full retained selected-plan identity under fixed planning inputs. It checks ordinary SELECT ownership. |
| Logical EXPLAIN | V40-041 compares displayed logical information with the bound logical owner and proves nonexecution. |
| Front-end and planning failures | V40-042–044 cover parser, binder/type, optimizer/resource, planning OOM, and final-validation failures before execution. |
| Execution-only failures | V40-045–048 pair ordinary EXPLAIN with ANALYZE for data-dependent arithmetic, spill, execution OOM, and scan-time storage faults. |
| Cancellation | V40-049 separates a planning barrier from cancellation after proved ANALYZE execution start. |
| Runtime actuals | V40-050 keeps estimates available while rejecting fabricated actual rows, time, memory, spill, and counters for ordinary EXPLAIN. |

The A–O self-review cases are covered by V40-039–050; none is an unexplained G3 gap. The negative implementations A–L are rejected by the corresponding nonexecution, handoff, fault-stage, validation, cancellation, and actual-field checks. G4 profile applicability and G5 optimizer-diagnostic composition remain outside this family.

The procedures reuse live owner coverage including V18-7, V19-17, V20-22, V22-K, V24-D/L/M, V26-K, V27-C/P, V30-G/H, V33-006/008/040/041, V38-052/059–062/065/067, V39-063–070, and the existing EXPLAIN ANALYZE profiling tests. The consistency search found no new stale Chapter-40 assertion or Architecture contradiction.

Repository review: initial and final HEAD are `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Prior Verification fixes were uncommitted and were preserved. Fix 3 changed only `docs/VERIFICATION.md`, adding **44 lines and deleting 0**; the total current Verification worktree diff is **185 insertions, 7 deletions**. The index remained clean. The pre-existing untracked `SYNC.md` review artifact was untouched. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

Next authorized task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 4: PROFILE APPLICABILITY / NOT-STARTED / EARLY-STOP / FAILURE / Q-ERROR INTEGRATION (CLOSE V40-G4).**

```text
CHAPTER 40 ARCHITECTURE: CLEAN — CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1: CLOSED V40-G6 / PRESERVED
VERIFICATION SYNC FIX 2A: CLOSED V40-G1 / PRESERVED
VERIFICATION SYNC FIX 2B1: CLOSED EVENT SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 2B2: CLOSED V40-G2 / PRESERVED
VERIFICATION SYNC FIX 3: CLOSED V40-G3
V40-G1: CLOSED / PRESERVED
V40-G2: CLOSED / PRESERVED
V40-G3: CLOSED
V40-G4: OPEN / UNMODIFIED
V40-G5: OPEN / UNMODIFIED
V40-G6: CLOSED / PRESERVED
V40-G7: DEFERRED / UNMODIFIED
V40 PROCEDURES: V40-001–V40-050
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: UNMODIFIED
PRE-EXISTING UNTRACKED REVIEW ARTIFACT: PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
```

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 3.

**CHAPTER 40 — VERIFICATION SYNC FIX 4A: CLOSED V40-G4 NORMAL/APPLICABILITY SUBSCOPE.** V40-G4 remains open for Fix 4B’s failure, cancellation, worker-partial, and diagnostic-merge coverage.

Added [V40-E — Profile applicability, lawful early stop, and q-error](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26594), with contiguous procedures **V40-051–V40-059**. They compare each selected node’s retained estimate and output boundary with direct operator start, work, demand, and completion evidence. The profile is never its own oracle; applicability is diagnostic, not a new runtime state.

| Procedure | Normal-execution coverage |
|---|---|
| V40-051–052 | Completed `100/100 → 1`, `100/1 → 100`, `0/0 → 1`, and both comparable exactly-one-zero cases → explicit infinity. |
| V40-053–054 | Selected unstarted nodes with zero or positive estimates; paired unstarted `E=0` against genuinely completed `E=0,A=0` using the same profile reader. |
| V40-055–056 | LIMIT- and EXISTS-censored children retain truthful work but have unavailable q-error; parent/result boundaries are assessed independently. |
| V40-057 | An observed ten-row prefix equal to `E=10` remains incomplete when the parent stops before exhaustion; it cannot yield q-error `1`. |
| V40-058 | Sort’s fully consumed child is comparable at its boundary, while a parent-censored Sort output is not. |
| V40-059 | An identity-correlated check keeps estimate, approved semantic-empty proof, observed work, applicability, and q-error distinct. |

The independent selected-plan oracle is the validated retained physical plan. The start/completion oracle is the direct operator and demand trace, composed from live V20-11/13/14, V26-F/G/N, V27-D/K, V30-E/F, V32-038, V33-040/041/045, V35-005/056/057, and the existing EXPLAIN ANALYZE profiling procedure. Fixed rows and demand barriers replace timing or scheduler luck. Missing owner or profile evidence is non-PASS. The new procedures cover self-review cases **A–P** and reject negative implementations **A–N**; the optional prefix-exceeds-estimate variant is not required for closure.

The global wording search found no new stale Chapter-40 assertion or Architecture contradiction. Existing estimator-quality q-error tests with known reference counts remain valid in their different scope. V40-001–050 and the repaired generic profiling assertions were preserved.

Repository state: initial and final HEAD `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Prior Verification fixes remained uncommitted. Fix 4A changed only `docs/VERIFICATION.md`: **38 insertions, 0 deletions**. The total current Verification worktree diff is **223 insertions, 7 deletions**. The index is clean; the pre-existing untracked `SYNC.md` review artifact is untouched. `git diff --check` passed. No tests, build, sanitizer, or benchmark were run.

Next authorized task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 4B: FAILURE / CANCELLATION / WORKER-LOCAL PARTIAL PROFILE / DIAGNOSTIC-MERGE INTEGRATION (COMPLETE V40-G4).**

```text
CHAPTER 40 ARCHITECTURE:
    CLEAN — CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1:
    CLOSED V40-G6 / PRESERVED
VERIFICATION SYNC FIX 2A:
    CLOSED V40-G1 / PRESERVED
VERIFICATION SYNC FIX 2B1:
    CLOSED EVENT SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 2B2:
    CLOSED V40-G2 / PRESERVED
VERIFICATION SYNC FIX 3:
    CLOSED V40-G3 / PRESERVED
VERIFICATION SYNC FIX 4A:
    NORMAL/APPLICABILITY SUBSCOPE CLOSED
V40-G1: CLOSED / PRESERVED
V40-G2: CLOSED / PRESERVED
V40-G3: CLOSED / PRESERVED
V40-G4:
    OPEN — NORMAL/APPLICABILITY SUBSCOPE CLOSED,
    FAILURE/CANCELLATION/WORKER SUBSCOPE NOT YET REPAIRED
V40-G5: OPEN / UNMODIFIED
V40-G6: CLOSED / PRESERVED
V40-G7: DEFERRED / UNMODIFIED
V40 PROCEDURES: V40-001–V40-059
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: UNMODIFIED
PRE-EXISTING UNTRACKED REVIEW ARTIFACT: PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
```

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 4A.

**CHAPTER 40 — VERIFICATION SYNC FIX 4B: CLOSED V40-G4.** Fix 4A’s normal-applicability coverage remains intact; the remaining failure, cancellation, and worker-partial sub-scope is now specified in [V40-F — Failed/cancelled execution and partial-profile integrity](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26632).

V40-F adds contiguous **V40-060–V40-070**. These are Verification procedures, not executed test results.

| Procedures | Independent oracle and required comparison |
|---|---|
| V40-060–063 | Operator, error, memory, and spill owner traces establish known work before arithmetic failure, SpillIOError, execution OOM, or QueryCancelled. An exposed profile retains safe observed work, never fabricates completion, and gives incomplete nodes no numeric q-error. The canonical error stays primary. |
| V40-064–065 | A partial profile is checked when exposed, but an error response without one remains valid if error and cleanup are correct. A child completed before its parent fails retains its independently comparable actual and q-error. |
| V40-066–068 | Worker accepted-work ledgers establish exact successful merge, quiescence before final partial observation, retention of safely available local work, no double count, and explicit incompleteness when one worker field cannot be recovered. |
| V40-069–070 | A test-only diagnostic-field merge denial leaves successful execution successful and does not replace a primary SpillIOError. Final checks compare actual rows, chunks, memory, spill, timing, and tasks with owner evidence and require completed cleanup and no live profile registration. |

The procedures compose live V24-D/H/L/M, V26-F/G/H/I/K, V27-P, V28-K/Q/S, V29-Q, V30-G/H, V32-009–012/029–039/052–054, V39-055–058/065–068, and the repaired EXPLAIN ANALYZE profiling procedure. Fault activation, worker departure, and owner completion are required evidence; missing evidence is non-PASS. No public error-plus-profile format or persisted profile history was introduced.

Self-review cases **A–P** and negative implementations **A–P** map to V40-060–070: abnormal prefixes and error precedence (060–065), successful and failed worker merges (066–068), diagnostic merge loss (069), and observed-work/cleanup checks (070). The global consistency search found no new stale Chapter-40 assertion or Architecture contradiction. G5 and benchmark follow-up G7 were not changed.

Initial and final HEAD: `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). Prior Verification fixes were uncommitted and preserved. Fix 4B changed only `docs/VERIFICATION.md`: **36 insertions, 0 deletions**. The total current Verification worktree diff is **259 insertions, 7 deletions**. The index is clean; the pre-existing untracked `SYNC.md` review artifact is untouched. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

Next authorized task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 5: OPTIMIZER / RESOURCE / TRACE / FINGERPRINT / NONINTERFERENCE COMPOSITION AND COVERAGE LEDGER (CLOSE V40-G5).**

```text
CHAPTER 40 ARCHITECTURE:
    CLEAN — CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1:
    CLOSED V40-G6 / PRESERVED
VERIFICATION SYNC FIX 2A:
    CLOSED V40-G1 / PRESERVED
VERIFICATION SYNC FIX 2B1:
    CLOSED EVENT SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 2B2:
    CLOSED V40-G2 / PRESERVED
VERIFICATION SYNC FIX 3:
    CLOSED V40-G3 / PRESERVED
VERIFICATION SYNC FIX 4A:
    NORMAL/APPLICABILITY SUBSCOPE CLOSED / PRESERVED
VERIFICATION SYNC FIX 4B:
    CLOSED REMAINING G4 SUBSCOPE
V40-G1: CLOSED / PRESERVED
V40-G2: CLOSED / PRESERVED
V40-G3: CLOSED / PRESERVED
V40-G4: CLOSED
V40-G5: OPEN / UNMODIFIED
V40-G6: CLOSED / PRESERVED
V40-G7: DEFERRED / UNMODIFIED
V40 PROCEDURES: V40-001–V40-070
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: UNMODIFIED
PRE-EXISTING UNTRACKED REVIEW ARTIFACT: PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
```

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 4B.

CHAPTER 40 — VERIFICATION SYNC FIX 5A: **CLOSED V40-G5 OPTIMIZER/DIAGNOSTIC COMPOSITION SUBSCOPE.** This is a documentation-level repair: the new procedures specify deterministic comparisons; no tests were run. V40-G5 remains open for Fix 5B’s chapter-wide noninterference and coverage-ledger work.

Initial and final HEAD were `1d913978a94111a7de65e809ebc16be242fe39d3` (`applied FIX-A3 40 in ARCHITECTURE`). The index was clean throughout. At the start, prior Verification fixes were uncommitted: `docs/VERIFICATION.md` had a 259-insertion, 7-deletion diff. The pre-existing untracked Chapter-40 `SYNC.md` review artifact was preserved untouched.

Only [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26668) changed in this task. The new heading is “V40-G — Optimizer, statistics, trace, and fingerprint diagnostics,” with contiguous procedures **V40-071–V40-083**. Fix 5A added **40 lines and deleted 0**; the total current Verification worktree diff is **299 insertions, 7 deletions**. V40-001–070 and the Fix-1 profiling assertions remain intact.

| New procedures | Independent owner comparison and required diagnostic result |
|---|---|
| V40-071 | The finalized retained physical tree supplies operator identity, rows, width, startup/total cost, relevant memory/spill estimates, and properties; physical EXPLAIN must report that same invocation’s applicable fields. |
| V40-072–074 | Retained statistics generations, schema version, staleness, base/column estimates, and selected index-access records supply the oracle for displayed versions, pressure, NDV/MCV/histogram availability, bounds, residuals, and ordering. Approximate or absent inputs retain that meaning. |
| V40-075–076 | The estimator path supplies missing/stale-statistics and independence/NDV/correlation fallback provenance. Approved proof metadata—not estimated or runtime zero—supplies semantic-empty proof kind. |
| V40-077–078 | Direct search events supply explored/pruned-alternative and trace facts. A never-explored alternative cannot appear as explored; enabling trace must leave the canonical selected plan unchanged. |
| V40-079–080 | The full structural key and its canonical serialization supply the oracle for any *optional* displayed fingerprint. A forced compact-hash collision cannot establish full-key or semantic identity. |
| V40-081–082 | Planner records supply estimated memory/spill; QueryMemoryManager and SpillManager supply actuals. Runtime actuals cannot silently overwrite persistent statistics or a later planner’s retained input. |
| V40-083 | If earliest-material-divergence attribution is exposed, comparable node observations and the configured policy supply its oracle. This remains SHOULD-level guidance, without an invented universal threshold. |

These compose existing owner procedures rather than repeat their proofs: V33-040/041/056; V34-024–029 and 065–073; V35-005, 017–021, 027, 043, 050, 056/057/066; V36-030–049 and 068; V37-052/054; V38-019–024, 052, 063–065; V24-D; and the existing Optimizer Diagnostics and EXPLAIN ANALYZE/Profiling suites. Each new procedure compares a retained owner record with a reported diagnostic from the same identified invocation; missing mandatory owner or diagnostic evidence is non-PASS.

Self-review A–S is covered or composed by V40-071–082, respectively: physical identity and planning fields (071), versions/staleness (072), column/base statistics (073), access paths (074), fallback provenance (075), zero/proof separation (076), trace truth and local plan noninterference (077–078), fingerprint and collision safety (079–080), resource separation (081), and no persistent runtime feedback (082). Item T is SHOULD-level/optional under V40-083. The negative implementations A–O are rejected by those same comparisons, including second-invocation EXPLAIN values, fabricated trace events, stale-as-fresh labeling, estimate/actual/proof substitutions, hash-as-identity, and automatic statistics feedback.

The global consistency search found **no new stale Chapter-40 assertion**. Existing estimator-quality and optimizer-owner tests have valid different scopes. No Architecture contradiction was found; Architecture, Development, Project State, source, tests, builds, benchmarks, devlogs, and historical artifacts were not changed. `git diff --check` passed. Final worktree status is modified `docs/VERIFICATION.md` plus the same pre-existing untracked review artifact; the index remains clean.

Next recommended task: **CHAPTER 40 — VERIFICATION SYNCHRONIZATION FIX 5B: GLOBAL DIAGNOSTIC NONINTERFERENCE / FINAL O40 COVERAGE AND REUSE LEDGER / VERIFICATION CLOSURE READINESS (COMPLETE V40-G5).**

```text
CHAPTER 40 ARCHITECTURE:
    CLEAN — CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1:
    CLOSED V40-G6 / PRESERVED
VERIFICATION SYNC FIX 2A:
    CLOSED V40-G1 / PRESERVED
VERIFICATION SYNC FIX 2B1:
    CLOSED EVENT SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 2B2:
    CLOSED V40-G2 / PRESERVED
VERIFICATION SYNC FIX 3:
    CLOSED V40-G3 / PRESERVED
VERIFICATION SYNC FIX 4A:
    CLOSED NORMAL/APPLICABILITY SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 4B:
    CLOSED V40-G4 / PRESERVED
VERIFICATION SYNC FIX 5A:
    OPTIMIZER/DIAGNOSTIC SUBSCOPE CLOSED
V40-G1 / G2 / G3 / G4 / G6:
    CLOSED / PRESERVED
V40-G5:
    OPEN — COMPOSITION CLOSED; GLOBAL NONINTERFERENCE / FINAL LEDGER NOT YET REPAIRED
V40-G7:
    DEFERRED / UNMODIFIED
V40 PROCEDURES:
    V40-001–V40-083
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    UNMODIFIED
PRE-EXISTING UNTRACKED REVIEW ARTIFACT:
    PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
```

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 5A.

## CHAPTER 40 — VERIFICATION SYNC FIX 5B

**Result: V40-G5 REMAINS OPEN.** The live Chapter-40 requirements now have a coverage/reuse map and focused noninterference procedures, but I could not complete the required one-to-one cross-check against the original **O40-001–O40-069** analysis ledger: its item text was not present in the supplied context or the repository material I found. I did not invent mappings for those labels. This is a closure-readiness limitation, not a newly found Architecture contradiction or an identified missing live-contract test.

Initial and final HEAD: `1d913978a94111a7de65e809ebc16be242fe39d3` — `applied FIX-A3 40 in ARCHITECTURE`. Prior Verification repairs remained uncommitted. The index was clean at both checks. The pre-existing untracked Chapter-40 `SYNC.md` review artifact was preserved untouched.

Only [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26707) was edited. The added family, **V40-H — Diagnostic noninterference and coverage integrity**, contains contiguous **V40-084–V40-091**. Fix 5B’s net delta is **49 insertions, 0 deletions** relative to its initial worktree state; the total current Verification diff is **348 insertions, 7 deletions**. A small wording update in the previously uncommitted V40-G introduction points to the new V40-H map. V40-001–083 were not renumbered or semantically refactored.

| New procedure | Coverage and independent oracle |
|---|---|
| V40-084 | B+ split metric against Chapter-8 published split events, including a failed/prepublication control. |
| V40-085 | Rows-scanned/filtered and hash-build/probe metric deltas against direct scan, Filter, and HashJoin occurrence ledgers. |
| V40-086 | Direct SELECT versus EXPLAIN ANALYZE: underlying result, visibility, demanded work, error category, and transaction/snapshot/lock ownership against execution owners—not client envelope equality. |
| V40-087 | Metric reads and diagnostic degradation against page, WAL, transaction, recovery, vacuum, query, optimizer, and statistics owner states; reuses saturation, merge-failure, trace, collision, and feedback fixtures. |
| V40-088 | §40.4 SHOULD-level debug introspection against transaction, lock, WAL, lifecycle, checkpoint, and vacuum owner barriers. A gauge alone is not treated as per-identity introspection. |
| V40-089 | Operator-specific and pipeline profile fields against direct V24/V27–V30/V32 work, resource, and task-graph records. |
| V40-090 | Ledger-integrity procedure: missing mandatory mapping or evidence, nonexistent reference, circular oracle, or incorrect SHOULD/MAY/benchmark classification is non-PASS. |
| V40-091 | §40.5 useful/optional logical and front-end detail, checked against the bound tree and catalog/front-end owners **when exposed**. |

The [coverage/reuse map](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:26731) traces live §§40.1–40.8 to V40 comparisons and distinct owner evidence. It explicitly includes B+ splits; scan/filter/hash totals; WAL and transaction boundaries; gauges, lifecycle, recovery and vacuum; ordinary EXPLAIN versus ANALYZE; per-node and pipeline profiling; optimizer diagnostics; debug introspection; diagnostic nonauthority; and optional/SHOULD distinctions. G7 remains a **nonblocking benchmark follow-up**, with no threshold or benchmark result invented.

For global negative implementations A–N, the specified checks reject owner-state changes on metric read (V40-084/087), altered SELECT or snapshot/lock semantics and error class (086), diagnostic saturation or merge failure changing outcomes (036–037/069–070/087), trace changing plan choice (078), fingerprint collision becoming identity (080), runtime actuals rewriting statistics (082), and missing/circular or misclassified coverage (090). These are **specified checks, not executed results**.

The global consistency review found no new stale Chapter-40 assertion and no Architecture contradiction. The two Fix-1 profiling assertions remain repaired. The new V40 IDs are contiguous through **V40-091**; the cited new-family owner headings and sampled numeric references resolve. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

**Original O40 cross-check:** I cannot truthfully assign individual statuses to O40-001 through O40-069 without the original item descriptions. Those labels were not persisted in VERIFICATION.md, as requested. The live-subsection map is present, but it is not a verified one-to-one reconstruction of that historical analysis inventory. Accordingly I am not claiming Chapter-40 Verification is synchronized at the repair level or independently closed.

The smallest next task is: **recover or provide the original O40-001–O40-069 analysis ledger, perform its read-only one-to-one comparison with the live §40 coverage map, and make only any resulting focused Verification correction.** The independent closure audit is gated on that check.

```text
CHAPTER 40 ARCHITECTURE:
    CLEAN — CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1:
    CLOSED V40-G6 / PRESERVED
VERIFICATION SYNC FIX 2A:
    CLOSED V40-G1 / PRESERVED
VERIFICATION SYNC FIX 2B1:
    CLOSED EVENT SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 2B2:
    CLOSED V40-G2 / PRESERVED
VERIFICATION SYNC FIX 3:
    CLOSED V40-G3 / PRESERVED
VERIFICATION SYNC FIX 4A:
    CLOSED NORMAL/APPLICABILITY SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 4B:
    CLOSED V40-G4 / PRESERVED
VERIFICATION SYNC FIX 5A:
    CLOSED OPTIMIZER/DIAGNOSTIC SUBSCOPE / PRESERVED
VERIFICATION SYNC FIX 5B:
    V40-G5 REMAINS OPEN — ORIGINAL O40 CROSS-CHECK UNAVAILABLE
V40-G1 / G2 / G3 / G4 / G6:
    CLOSED / PRESERVED
V40-G5:
    OPEN
V40-G7:
    DEFERRED BENCHMARK FOLLOW-UP / NONBLOCKING
V40 PROCEDURES:
    V40-001–V40-091
CHAPTER 40 VERIFICATION:
    NOT YET SYNCHRONIZED
INDEPENDENT VERIFICATION CLOSURE:
    NOT PERFORMED BY THIS TASK
NEXT AUTHORIZED TASK:
    RECOVER ORIGINAL O40-001–O40-069 LEDGER AND COMPLETE
    READ-ONLY ONE-TO-ONE COVERAGE CROSS-CHECK
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    UNMODIFIED
PRE-EXISTING UNTRACKED REVIEW ARTIFACT:
    PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE
```

END CHAPTER-40 VERIFICATION SYNCHRONIZATION FIX 5B.