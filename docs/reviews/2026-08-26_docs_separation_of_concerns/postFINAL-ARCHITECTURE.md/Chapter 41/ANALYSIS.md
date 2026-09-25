# Chapter 41 — Initial read-only Architecture analysis

**Verdict: CLEAN — ARCHITECTURE CLOSED.** The live chapter states finite, testable verification obligations without taking semantic ownership away from Chapters 1–40. I found no Architecture contradiction, material ambiguity, impossible oracle, or requirement for implementation to exist now. This is an Architecture review—not a Chapter-41 Verification synchronization or a claim that tests passed.

The initial and final HEAD was `0d69dbd9091aa34790ff31ca4e684c0279d8c929` (`synced VERIFICATION after chapter 40 ARCHITECTURE fix`). The index remained clean. At both checks the worktree contained an **untracked Chapter-41 review directory**, a difference from the expected clean baseline; I left it untouched. `git diff --check` passed, and this audit created no changes.

## Live chapter and document role

[Chapter 41, “Verification Requirements”](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30048) runs from line 30048 through line 30605. [Chapter 42, “Performance Requirements”](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30606) follows. The complete Chapter-41 inventory is §§41.1 Storage; 41.2 B+ tree; 41.3 Transaction, recovery, and reclamation; 41.4 Catalog, front-end, and logical plan; 41.5 Physical execution; 41.6 Statistics, estimator, and base access; and 41.7 Join search, properties, memo, and optimizer.

Chapter 41 is necessary as the cross-subsystem *verification-obligation* contract. Its long matrices mostly name distinct high-risk boundaries; they do not prescribe test counts, frameworks, file layouts, benchmark thresholds, or current pass results. Chapters 1–40 remain the owners of behavior. [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md) owns fixtures and procedures; its existing §41 references are evidence that these obligations can be realized, not authority over the Architecture. No missing Verification procedure was treated as an Architecture defect.

The normative-strength check found no promotion of an optional feature into required feature existence or weakening of an owner MUST. The explicit MUST obligations include simulated recovery failure, small-resource testability, storage and B+ coverage, §12.12 non-crash faults, lifecycle/root/removal and transaction boundaries, cross-layer type properties, and DML candidate closure. The crash-point list and frequent full-tree-verifier use remain **SHOULD**. Synthetic near-boundary aggregate states remain **MAY**. The remaining operative lists describe coverage, not new subsystem outcomes.

## Obligation-family assessment

| Chapter 41 area | Independent Architecture assessment |
|---|---|
| Opening strategy | Unit, seeded property/randomized, crash, overlapping-semantics differential SQL, and concurrency tests are appropriate. Randomized inputs do not make the expected result random: seeds and independent models make failures reproducible. A reference DBMS is a supplementary overlapping-semantics comparison, not authority over DBlusBlus-specific rules. Simulated crash/fault boundaries are necessary under Chapters 12–13; clean reopen alone is insufficient. Tiny BufferPools and resource limits are testability requirements, not a production test-control API. |
| [§41.1 Storage](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30072) | Heap geometry and retained ranges, DEAD/UNUSED validation, checked persisted arithmetic, stable SlotIds and bytes after compaction, tuple and raw-file round trips, injected I/O, BufferPool eviction/writeback/pins/CLOCK/latching, reopened scans, and stale-FSM repair each have their respective Chapters 4–7 owners. Slot reuse correctly follows Chapter 14’s grace protocol; immediate DEAD reuse is explicitly rejected. |
| [§41.2 B+ tree](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30098) | Deterministic split/merge/root, type/key-edge, and 1024-byte-boundary cases match Chapter 8 and the key owners. Corruption tests preserve L1 page-local/L2 followed-reference versus L3 global-verifier timing; they do not demand a full graph traversal on every access. Duplicate-heavy physical-key tests distinguish SQL user-key duplicates, exact stored-key corruption, authorized replay, and Chapter-11 UNIQUE enforcement. A sorted `(encoded_user_key,RID)` model remains independent across insert, erase, lookup, and reopen. Concurrent cases require controlled schedules, not a specific latch or fairness policy. Frequent full-tree verification remains SHOULD-level. |
| [§41.3 Failure and transactions](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30155) | Crash and separate §12.12 non-crash faults preserve known/uncertain append, exact rollback, and NONCONTINUABLE boundaries. Exact pre-operation bytes and frame/DPT/FPI metadata are justified by the owner’s rollback contract; PAGE_INIT, B+ MTR, and checkpoint observations preserve publication atomicity. The extensive lifecycle, root-adoption, and whole-root-removal matrices enumerate Chapter-3/4 ownership and durability hazards without inventing deletion policy. Statement-write, result-publication `R`, C0–C6/A0–A4, recovery, MVCC, isolation, lock graph, UNIQUE, and vacuum cases retain their Chapters 9–15/31/39 owners. |
| [§41.4 Front-end](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30299) | Parser synchronization belongs to Chapter 18’s request grammar; binder/type/subquery cases follow Chapters 17–20, including unsupported-form rejection. Cross-layer type-property comparison means the *same owned semantics* where a type participates, not premature implementation. Catalog cases follow Chapter 16. Canonical logical shapes are limited by Chapter 20’s defined initial form; rewrite examples test each implemented rule and its safety, not one universal equivalent representation. Lexer/parser/evaluator fuzzing requires controlled failure, not an invented time limit. |
| [§41.5 Execution](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30348) | Pre-execution validation, synthetic/manual/SQL and differential execution, vector representations, string lifetime, joins, sorting, DML, spill, cancellation, and cleanup follow Chapters 20–32. Reference nested-loop execution checks semantics; it does not constrain production plan choice. Deliberate unpin/recycle tests lifetime, not a required string container. The DML candidate-domain and result-interface obligations preserve first-write and `R` ownership. Aggregate wording correctly separates identical **exact** results/errors from FLOAT64 SUM/AVG results produced by different admitted §29.3.4 trees. |
| [§41.6 Statistics](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30449) | Exact small-table facts, probabilistic collectors, persisted validation, estimator reference distributions, semantic-proof counterexamples, costed access alternatives, and cursor-versus-predicate bounds have Chapters 34–36 and related semantic owners. HLL error-distribution testing can use reproducible seeds and distribution-level evidence; it need not fail on one unlucky sketch. Estimator-quality reference counts are distinct from Chapter 40’s execution-profile q-error, which requires a completed comparable node. Zero estimate, proof, and runtime actual remain separate. No fixed selectivity threshold or universal chosen plan is imposed. |
| [§41.7 Optimizer](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30523) | Controlled search/property/resource scenarios test legality and configured choice behavior, not a globally mandatory cost-dependent winner. Synthetic join estimates have known/reference distributions. A simple reference plan provides a useful differential oracle, complemented by exact type/operator tests against shared-bug false agreement. Seeded fuzzing, bounded planning, and regression properties are implementable without asserting arbitrary floating costs or treating the compact fingerprint as identity. |

The recovery model’s **logical committed contents** oracle correctly permits physical aborted residue. RC/RR tests do not upgrade snapshot isolation to serializability: write skew remains possible. Lock tests preserve transaction-lock versus physical-latch separation, complete wait-graph edges, terminal-only release, and wakeup revalidation. Root publication/removal tests preserve parent-fsync and exclusive-owner boundaries; they do not authorize deletion of external siblings.

## High-risk decisions

| Question | Decision |
|---|---|
| A. Aggregate identity versus FLOAT64 reduction order | No conflict. The explicit FLOAT64 exception requires each result to match an admitted tree, not another execution’s bits. |
| B. Canonical logical shapes | Not an overconstraint when read against Chapter 20’s canonical initial logical form; helper/physical representation freedom remains. |
| C. Expected plan per rewrite rule | Finite per implemented rule and controlled input; differential semantic safety remains separate from exact diagnostic shape. |
| D. Nested-loop differential oracle | Sufficient for optimizer composition when paired with independent expression, type, and operator-owner tests; not a universal sole oracle. |
| E. HLL distributions | Deterministic-enough via reproducible seeds and distribution-level assessment; no one-sketch exactness requirement. |
| F. Full-tree verifier cadence | Appropriately SHOULD; “frequently” is methodology, not a correctness threshold. |
| G. Lifecycle/root/removal matrices | Detailed but owner-referenced, with no incompatible observable outcome found. |
| H. Estimate/proof/actual | Properly separate. |
| I. Q-error | Estimator reference-count quality and Chapter-40 comparable execution actuals are different scopes; no zero/applicability conflict. |
| J. Chapter-42 boundary | No quantitative performance requirement leaked into Chapter 41. Resource stress here tests correctness; benchmarks remain Chapter 42/VERIFICATION methodology. |

Independent oracle types are identifiable throughout: exact bytes and metadata, sorted physical-key models, durable-commit recovery models, MVCC truth tables and wait graphs, bound/reference evaluators, semantic comparators, analytically known distributions, and canonical proof/retained-owner records. Crash points and faults can be activated and evidenced with minimal hooks; randomized failures can retain seeds; neither race luck nor real OOM/ENOSPC is required. The chapter does not mandate production-scale fault services, global deterministic scheduling, exhaustive interleaving model checking, a stable debug SQL API, or a universal reference DBMS.

Targeted comparison with the frozen storage, B+, transaction/WAL/recovery, SQL, execution, optimizer, error, and Chapter-40 owners found **no incompatible observable requirement**. [DEVELOPMENT.md](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md) sequences the prerequisite subsystems without conflicting with future verification obligations. [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:258) still describes BufferPool and upper layers as unimplemented; Chapter 41 makes no implementation-completion claim, and the BufferPool authorization gate remains intact.

**Findings:** none. Blocking 0; major 0; minor 0; editorial 0; design-scope 0; Verification-sync-only 0. No Architecture repair plan is needed.

The next authorized task is **CHAPTER 41 — INITIAL READ-ONLY VERIFICATION SYNCHRONIZATION / COVERAGE-GAP ANALYSIS**: map each §41 obligation to existing subsystem procedures and independent oracles, adding no duplicate V41 suite by assumption. That analysis was not performed here.

```text
CHAPTER 40 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 40 VERIFICATION:
    CLEAN — CLOSED
CHAPTER 41 ARCHITECTURE:
    CLEAN — CLOSED
CHAPTER 41 FINDINGS:
    BLOCKING: 0
    MAJOR: 0
    MINOR: 0
    EDITORIAL: 0
    DESIGN-SCOPE: 0
    VERIFICATION-SYNC-ONLY: 0
CHAPTER 41 VERIFICATION:
    NOT SYNCHRONIZED BY THIS TASK
NEXT AUTHORIZED TASK:
    CHAPTER 41 — INITIAL READ-ONLY VERIFICATION
    SYNCHRONIZATION / COVERAGE-GAP ANALYSIS
IMPLEMENTATION:
    NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN
INDEX:
    UNMODIFIED
AUDIT-CREATED CHANGES:
    NONE
```

END CHAPTER-41 INITIAL READ-ONLY ARCHITECTURE ANALYSIS.