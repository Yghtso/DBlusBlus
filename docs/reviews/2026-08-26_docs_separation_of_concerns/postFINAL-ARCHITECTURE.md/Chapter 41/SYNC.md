**CHAPTER 34/41 — HLL QUALITY-CONTRACT ARCHITECTURE FIX A1: CLOSED A1.** Chapter 34 now defines a falsifiable v1 population-quality contract in [§34.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24767). Independent Architecture closure is still pending; V41-G1 remains blocked until that audit.

The repository began and ended at `b97f676fdf2ef6492c84bb517db2b84b705e007f` (`chapter 41 ARCHITECTURE analysis`). The initial worktree and index were clean. The final worktree has one unstaged change: `docs/ARCHITECTURE.md`, **24 insertions, 0 deletions**. The index remains clean. The tracked Chapter-41 review artifact was untouched.

The new contract retains fixed v1 `p = 14`, `m = 16,384`. For each `N` from `4m` through `64m`—**65,536 through 1,048,576** in v1—the population consists of uniformly drawn sets of `N` distinct INT64 SQL values. Each value is presented twice, in ascending signed-value order, through ordinary canonical hashing and HLL collection. Thus the input has `2N` rows but exact NDV `N`; the writer’s row-count cap is `2N`, so it does not clip ordinary positive error at `N`.

`N_hat` is the **post-construction-bounds NDV candidate**, interpreted as its finite binary64 value—not an internal raw estimate. With `e = (N_hat − N)/N` and `E` over that input-set ensemble, §34.9 now requires:

```text
sqrt(E[e²]) ≤ 2/√m
|E[e]|    ≤ 1/√m
```

The symbolic bounds are normative; no separate decimal constants were added. They are population ceilings, not per-sketch bands. No estimator formula, bias-correction method, production RNG, configurable v1 precision, or hash-seed field was mandated. I found no Architecture-owned HLL merge capability, so the optional merge clause was not added.

Empty and smaller inputs retain §§34.8 and 34.14.6.5 treatment; relative error is not defined for `N = 0`. Structural validity and writer bounds remain separate from distributional quality. A valid individual tail estimate is not corruption, an ANALYZE/SQL error, a transaction abort, or a database-health failure solely because of its error. No persisted format or reader-validation rule changed. NDV remains approximate planning evidence, never semantic proof under Chapter 35; Chapter-40 q-error and §41.6 remain unchanged. The `m = 2^p` relationship does not make v1 `p` configurable.

The semantic checks resolve as intended: zero-only and severely positively or negatively biased collectors fail the population limits; lower-RMS collectors and improved bias correction remain legal; one unlucky trial alone does not fail them. A structurally valid but statistically poor collector is quality-nonconforming, while a compliant estimate that later becomes stale remains approximate metadata. Different estimator algorithms may conform. The targeted HLL and cross-owner search found no new contradiction. The Architecture supplies the ensemble, domain, metric, and ceilings; finite trial counts, seeds, and acceptance methodology remain for a later Verification repair.

`git diff --check` passed. No changes were made outside the authorized Architecture edit; Chapter 41, Verification, Development, Project State, source, tests, benchmarks, and review artifacts were not modified. No procedures were executed.

The exact next authorized task is **CHAPTER 34/41 — HLL QUALITY-CONTRACT INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT**.

```text
CHAPTER 34 HLL QUALITY CONTRACT:
    REPAIRED

CHAPTER 34 ARCHITECTURE:
    NARROWLY REOPENED — A1 COMPLETE, INDEPENDENT AUDIT PENDING

CHAPTER 41 ARCHITECTURE:
    CLOSED / UNMODIFIED

V41-G1 — HLL DISTRIBUTIONAL QUALITY:
    BLOCKED PENDING ARCHITECTURE RE-CLOSURE

V41-G2 — B+ CONCURRENCY VERIFIER SHOULD:
    OPEN / UNMODIFIED

CHAPTER 41 VERIFICATION:
    NOT YET SYNCHRONIZED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    UNMODIFIED

REVIEW ARTIFACTS:
    PRESERVED / UNMODIFIED

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 34/41 — HLL QUALITY-CONTRACT INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT
```

END CHAPTER-34/41 HLL QUALITY-CONTRACT ARCHITECTURE FIX A1.

## Independent verdict: NEEDS ARCHITECTURE FIX

The live §34.9 repair is mathematically clear for its stated population, but that population is too narrow to close the HLL quality reopening. It constrains **ascending, exactly-two-copy INT64 inputs** while ordinary ANALYZE may encounter other row orders and duplicate multiplicities. No Chapter-34 rule transfers the quality bound to those ordinary presentations.

A collector could use the specified HLL path and meet both population bounds on ascending pairs, yet produce poor estimates when the same values arrive in another order. That is a material quality loophole, not a formatting concern. **V41-G1 remains blocked.** No file was edited during this audit.

### Repository and live contract

Initial and final HEAD: `b97f676fdf2ef6492c84bb517db2b84b705e007f` — `chapter 41 ARCHITECTURE analysis`. The index was clean throughout. The live A1 diff is **24 insertions, 0 deletions**, confined to [§34.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24767).

The worktree also contained a pre-existing untracked artifact not listed in the expected baseline: `docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 41/SYNC.md`. It remained untouched, as did the tracked Chapter-41 `ANALYSIS.md`.

The inserted text does define, without a further mathematical choice, a uniform distribution over `N`-element subsets of the `2^64` INT64 values, for each inclusive `N ∈ [4m, 64m]`. With v1 `p=14`, `m=16,384`, the endpoints are **65,536** and **1,048,576**. Each set has one specified ascending two-copy presentation. `N_hat` is unambiguously the finite binary64, **writer-bounded ANALYZE candidate**, not raw HLL output or a later persisted-data estimate. `e=(N_hat−N)/N` is signed; the bounds are population `sqrt(E[e²]) ≤ 2/√m` and `|E[e]| ≤ 1/√m`, or 1.5625% RMS and 0.78125% absolute bias in v1. The two bounds are compatible: `E[e²]=Var(e)+E[e]²`; neither requires zero bias or limits each sketch.

### Finding

| ID | Severity | Location and consequence | Smallest repair |
|---|---|---|---|
| HLL-AUD-1 | **MAJOR — blocks closure** | [§34.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24770) fixes ascending order and exactly two occurrences per value. The statistical guarantee therefore need not hold for other ordinary scan orders or duplicate profiles; Chapter 34 supplies no separate quality-transfer invariant. | Keep the INT64 set ensemble and numeric bounds, but make the quality property apply across ordinary presentation orders and duplicate multiplicities that retain exact NDV `N` and enough rows to avoid the `N` writer cap. Define that quantification precisely without mandating a particular HLL update algorithm. |

Finding counts: **MAJOR 1; BLOCKING 0; MINOR 0; EDITORIAL 0; DESIGN-SCOPE QUESTION 0.** “Major” here is closure-blocking under the requested standard.

The INT64 type and `[4m,64m]` interval are explicit *chosen limits* of the v1 statistical guarantee. A collector good for INT64 but poor for VARCHAR, or good inside the interval but poor beyond it, is outside this particular quality claim, though still subject to other hash, statistics, and SQL-correctness rules. Those limits are not hidden ambiguities. The **fixed presentation shape** is different: it allows quality to depend on an incidental scan pattern within the very population being used to claim HLL-path quality.

### Other audit results

- **Writer bounds and numeric domain:** `2N` input rows leave ordinary positive error around `N` uncensored by the row-count cap. The §34.10 MCV limit is 64, so the observed-MCV/one-value lower bound cannot materially clip normal errors at `N ≥ 65,536`. From [§34.14.6.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25274), `N_hat ≤ 2N` and `N_hat ≥ 1` here; hence `−1 < e ≤ 1`. Binary64 represents these row-count endpoints exactly. The finite population and bounded error make both expectations existent.
- **Finite Verification design:** Predeclared independently sampled sets can yield sample means of `e²` and `e`. Bounds on `e` permit conservative, predeclared confidence margins for both without assuming a particular error-distribution shape. Seeds can make the selected trials replayable; a deterministic PRNG is a sampling mechanism, not the Architecture probability measure. Finite evidence cannot prove every population case, and the margin/trial count belong in Verification. This aspect is testable, though the presentation-scope defect must be fixed first.
- **Hash path and collisions:** The rule measures actual canonical INT64 hashing through HLL and construction bounds; it assumes neither ideal hash uniformity nor collision-free 64-bit hashes. A poor deterministic hash fails if it makes this ensemble’s quality poor. Duplicate values target NDV `N`, not row count `2N`.
- **Exact mode:** [§34.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24726) permits a configurable exact-mode threshold, so `2N ≥ 131,072` does **not** universally guarantee automatic HLL selection. The phrase “to the HLL collection path” identifies the component whose quality is required; a later Verification procedure must prove that path was actually exercised. Exact-mode output alone cannot satisfy the HLL test. This is implementable without a production toggle.
- **Structural, persistence, and runtime boundaries:** The new rule does not change register validity, payload bytes, `StatsVersion`, reader acceptance, or error taxonomy. One statistically poor but structurally valid tail candidate is not corrupt and does not cause SQL, transaction, or database-health failure. Population-level bad quality instead makes the collector nonconforming.
- **Downstream consistency:** [Chapter 35](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25544) still treats NDV as estimate, never semantic proof. Chapters 36 and 38 may use it for costs and plan selection without promising one plan or changing query correctness. [Chapter 40](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30023) q-error compares comparable plan estimates and execution actuals; it is not §34.9’s signed population error. [§41.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30473) can refer to the Chapter-34-owned quality rule without repeating its constants.
- **Implementation freedom and document role:** No exact estimator, correction table, hash algorithm, seed, production RNG, or new persisted field is required. The uniform set and `N` interval define a statistical policy; the duplicate construction defines the writer-bounded endpoint. Fixing one ascending presentation, however, makes the policy unnecessarily fixture-shaped. The `2/√m` and `1/√m` ceilings are honestly stated as project requirements, not attributed to a universal HLL theorem. I found no Architecture-owned mandatory HLL merge or parallel ANALYZE capability, so the omitted merge clause is appropriate. V1 precision remains fixed; `m=2^p` alone authorizes no future precision change.

### Negative-case assessment

| Case | Live-contract result |
|---|---|
| A zero-only; B severe positive bias; C severe negative bias | Nonconforming on the defined ensemble. The MCV floor does not rescue zero-only output. |
| D lower-RMS collector; E improved bias correction | Permitted if all other owner rules hold. |
| F one unlucky high-error sample; I individual tail estimate | Not alone a violation, corruption, or runtime error. |
| G structurally valid but population-quality-bad collector | Valid representation; nonconforming implementation quality. |
| H compliant estimate later stale | Still approximate metadata, not proof. |
| J different estimator formula | Permitted. |
| K good only on ascending input | **Technically able to satisfy the written quality population; closure-blocking loophole.** |
| L good for INT64, poor for VARCHAR | Outside the expressly scoped statistical guarantee; other type/semantic rules still apply. |
| M exact-mode bypass | Does not exercise the named HLL path and cannot serve as its Verification evidence. |
| N poor deterministic hash plus good textbook HLL | Nonconforming when the combined path breaches the ensemble bounds. |

**Next authorized task:** `CHAPTER 34/41 — HLL QUALITY-CONTRACT ARCHITECTURE FIX A2: PRESENTATION-ORDER AND DUPLICATE-PROFILE SCOPE`. Repair that owner contract, then perform a separate independent read-only closure audit before retrying V41 Verification Fix 1. V41-G2 remains open and unmodified.

Final HEAD and worktree/index state matched the initial state, including the pre-existing untracked `SYNC.md`. `git diff --check` passed. Audit-created changes: **none**.

```text
CHAPTER 34 HLL QUALITY CONTRACT:
    NEEDS FIX

CHAPTER 34 ARCHITECTURE:
    NARROW HLL REOPENING REMAINS OPEN

CHAPTER 41 ARCHITECTURE:
    CLOSED / UNMODIFIED

V41-G1 — HLL DISTRIBUTIONAL QUALITY:
    STILL BLOCKED

V41-G2 — B+ CONCURRENCY VERIFIER SHOULD:
    OPEN / UNMODIFIED

CHAPTER 41 VERIFICATION:
    NOT YET SYNCHRONIZED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    UNMODIFIED

REVIEW ARTIFACTS:
    PRESERVED / UNMODIFIED

AUDIT-CREATED CHANGES:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 34/41 — HLL QUALITY-CONTRACT ARCHITECTURE FIX A2:
    PRESENTATION-ORDER AND DUPLICATE-PROFILE SCOPE
```

END CHAPTER-34/41 HLL QUALITY-CONTRACT INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT.

**CHAPTER 34/41 — HLL QUALITY-CONTRACT ARCHITECTURE FIX A2: CLOSED A2.** The revised [§34.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24767) no longer limits the quality guarantee to ascending, exactly-two-copy input. The combined A1+A2 contract still awaits an independent read-only closure audit; V41-G1 remains blocked until then.

Initial and final HEAD were `b97f676fdf2ef6492c84bb517db2b84b705e007f` (`chapter 41 ARCHITECTURE analysis`). The initial index was clean, with A1’s unstaged `docs/ARCHITECTURE.md` delta of **24 insertions, 0 deletions**. A2 changed **21 lines in and 14 lines out** of that paragraph, a net seven-line increase. The total current Architecture diff is **31 insertions, 0 deletions**, all in §34.9. The final index is clean; no file other than `docs/ARCHITECTURE.md` was changed by this task.

For each `N` in the unchanged `4m ≤ N ≤ 64m` domain, `S` is now uniform over all `N`-element subsets of signed INT64 SQL values. An admissible `P` is a deterministic mapping defined for every such `S`: it produces a finite, representable ANALYZE input containing only values from `S`, **each at least twice**, with otherwise unrestricted row order and duplicate multiplicity. The exact distinct set remains `S`; its row count `R` satisfies `R ≥ 2N`.

For **every fixed admissible `P`**, the ordinary canonical-hash/HLL path produces the post-construction-bounds finite binary64 candidate `N_hat_P(S)`, and `e_P(S) = (N_hat_P(S) − N)/N`. Expectation is **only over uniform `S`, while `P` stays fixed**. Both unchanged population ceilings apply to every `P`:

```text
sqrt(E_S[e_P(S)²]) ≤ 2/√m
|E_S[e_P(S)]|     ≤ 1/√m
```

Thus neither a favorable order nor one duplicate profile can stand in for the whole presentation scope. This does **not** require identical sketch states, raw outputs, or bounded candidates across presentations, and adds no random production presentation policy. The real writer cap remains `R`, which is at least `2N`; the quality endpoint is not replaced with a raw internal estimate. The explicit HLL path remains necessary even if a configured small-table exact mode would otherwise apply.

The statistical scope remains INT64 and the original `N` interval; v1 still fixes `p=14`, `m=16,384`. Structural validation, persisted format, runtime/error semantics, and the distinction between approximate estimate and semantic proof are unchanged. Chapters 35, 40, and 41 retain their separate roles. A future Verification procedure can use reproducible sampled sets and several deterministic presentation policies; Architecture does not prescribe seeds, trial counts, or an acceptance-test implementation. The targeted HLL/order/duplicate and downstream-owner searches found no new contradiction.

| Negative case | Result under A1+A2 |
|---|---|
| A–C: good only ascending, only exactly-two copies, or poor on skewed duplicates | **Nonconforming** if the corresponding admissible policy breaches either population ceiling. |
| D: good across admissible policies | Potentially conforming, subject to other owner rules. |
| E–F: poor for VARCHAR or outside `[4m,64m]` | Outside this deliberately scoped statistical-quality guarantee. |
| G: one high-error tail estimate | Not alone nonconforming. |
| H: population failure for any admissible policy | **Nonconforming.** |
| I: exact-mode output substituted for HLL evidence | Does not establish HLL-path quality. |
| J: gross overestimate capped at `R` | Evaluate the actual bounded candidate; it may fail quality. |
| K: different deterministic estimator | Permitted if it satisfies all owner rules and bounds. |
| L: structurally valid but statistically bad collector | Candidate representation can be valid while implementation quality is nonconforming. |

The tracked Chapter-41 `ANALYSIS.md` and pre-existing untracked Chapter-41 `SYNC.md` were preserved untouched. Chapter 41, Verification, Development, Project State, source, tests, benchmarks, and build files were not edited. `git diff --check` passed. No build, test, sanitizer, benchmark, stage, or commit was performed.

**Exact next authorized task:** CHAPTER 34/41 — HLL QUALITY-CONTRACT INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT — A1+A2.

```text
CHAPTER 34 HLL QUALITY CONTRACT:
    A1+A2 REPAIRED

CHAPTER 34 ARCHITECTURE:
    NARROW HLL REOPENING — A2 COMPLETE, INDEPENDENT AUDIT PENDING

CHAPTER 41 ARCHITECTURE:
    CLOSED / UNMODIFIED

V41-G1 — HLL DISTRIBUTIONAL QUALITY:
    BLOCKED PENDING ARCHITECTURE RE-CLOSURE

V41-G2 — B+ CONCURRENCY VERIFIER SHOULD:
    OPEN / UNMODIFIED

CHAPTER 41 VERIFICATION:
    NOT YET SYNCHRONIZED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    UNMODIFIED

REVIEW ARTIFACTS:
    PRESERVED / UNMODIFIED

AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 34/41 — HLL QUALITY-CONTRACT
    INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT — A1+A2
```

END CHAPTER-34/41 HLL QUALITY-CONTRACT ARCHITECTURE FIX A2.

## Independent verdict: CLEAN — HLL QUALITY CONTRACT CLOSED

The combined A1+A2+A3 contract is mathematically coherent, implementation-independent, compatible with downstream owners, and sufficient to unblock Chapter-41 Verification Fix 1.

The narrow Chapter-34 reopening is independently closed. No files were edited.

### Repository state

Initial and final HEAD:

`b97f676fdf2ef6492c84bb517db2b84b705e007f` — `chapter 41 ARCHITECTURE analysis`

The index was clean. The only tracked worktree modification was `docs/ARCHITECTURE.md`, with the complete A1+A2+A3 delta of **48 insertions, 0 deletions**, confined to §34.9.

Review artifacts were preserved:

- tracked `Chapter 41/ANALYSIS.md`
- pre-existing untracked `Chapter 41/SYNC.md`

`git diff --check` passed. Audit-created changes: **none**.

### Contract audit

Layer A, base structure, remains clear:

- v1 fixes `p = 14`, `m = 16,384`.
- HLL hashes canonical non-NULL SQL values.
- NULL is excluded.
- HLL registers remain process-local.
- v1 persists only the NDV estimate.

Layer B, pre-bound semantics, is now complete:

- The pre-bound estimate is the finite binary64 result from completed HLL collection before §34.14.6.5 construction bounds.
- For the same distinct canonical non-NULL SQL-value set, the pre-bound estimate must be identical across:
  - singleton, mixed, and repeated multiplicities;
  - ascending, descending, clustered, and interleaved order;
  - chunk boundaries.
- Internal register-state identity is not required.
- Post-bound candidate equality is not required.

This correctly permits singleton and repeated presentations to have different final candidates because their row-count caps may differ.

Layer C, post-bound quality, remains intact:

- `S` is uniformly distributed over all `N`-element signed-INT64 subsets.
- `4m ≤ N ≤ 64m`.
- `P` is a complete deterministic presentation policy, defined for every `S`.
- Every value occurs at least twice for the uncensored quality population.
- Row order and additional multiplicity are unrestricted.
- `N_hat_P(S)` is the finite binary64 candidate after §34.14.6.5 bounds.
- For every fixed `P`, expectation is only over `S`.
- Required bounds remain:

```text
sqrt(E_S[e_P(S)^2]) ≤ 2 / sqrt(m)
|E_S[e_P(S)]|     ≤ 1 / sqrt(m)
```

These are population bounds, not per-sketch limits.

### Key findings

- Pre-bound finiteness and canonical `+0` are compatible with §34.14.6.5 numerical validation.
- The distinct-set invariant uses canonical SQL values, not raw bytes or hash outputs.
- Hash collisions remain permitted and contribute naturally to estimator error.
- Different distinct sets may produce the same estimate; A3 does not imply injectivity.
- The qualitative invariant applies to every logical type using HLL.
- The quantitative RMS/bias guarantee remains intentionally INT64-only.
- No mandatory HLL merge or parallel ANALYZE capability exists; omission is correct.
- Exact-mode collection remains separate and is not HLL-quality evidence.
- Legitimate resource failures, cancellation, storage errors, and configured limits remain governed by existing owners. Presentation shape alone is not a new failure cause.
- No persisted field, `StatsVersion`, reader rule, runtime error, transaction consequence, or semantic-proof rule changed.
- Chapter 35 still treats NDV as approximate evidence, never proof.
- Chapters 36 and 38 retain estimate-driven costing/search without imposing a fixed plan.
- Chapter 40 q-error remains an execution diagnostic with different scope.
- §41.6’s “HLL error distribution” now has a complete Chapter-34 owner contract; no Chapter-41 edit is needed.

The pre-bound stage and post-bound stage are sufficiently distinguished. §34.14.6.5 identifies the canonical construction bounds applied to the raw/pre-bound estimate, leaving no material hidden row-count or order-dependent stage between the named estimator and candidate.

### Negative matrix

| Case | Result |
|---|---|
| Same set, different pre-bound singleton/pair result | Nonconforming |
| Same set, different pre-bound result by order | Nonconforming |
| Same set, different pre-bound result by multiplicity | Nonconforming |
| Same set, different pre-bound result by chunking | Nonconforming |
| Same pre-bound result, different post-bound candidates | Permitted and expected |
| One high-error candidate | Not independently nonconforming |
| RMS or bias population failure | Nonconforming |
| Genuine resource failure | Existing resource semantics apply |
| Fabricated presentation-specific failure | Not permitted |
| Exact-mode result substituted for HLL evidence | Insufficient evidence |
| Different estimator algorithm | Permitted |
| Hash collision | Permitted input phenomenon |
| Different sets sharing an estimate | Permitted |
| Structurally valid but quality-bad implementation | Representation valid; implementation nonconforming |
| Later stale estimate | Approximate metadata; no proof |
| Hidden post-bound multiplicity correction | Not permitted by the canonical construction pipeline |
| Hidden post-bound order correction | Not permitted by the canonical construction pipeline |

No new findings were identified.

### Prior finding disposition

- `HLL-AUD-1`: **CLOSED**
- `HLL-A12-AUD-1`: **CLOSED**
- `HLL-A12-AUD-2`: **CLOSED**

The singleton loophole is closed by A3’s pre-bound set-function invariant. A1+A2 remains necessary because it measures post-bound statistical quality on uncensored repeated presentations.

A future Verification procedure can now combine:

- exact singleton/pair/mixed/order/chunk invariance checks;
- seeded INT64 population samples;
- representative repeated presentation policies;
- post-bound `N_hat`, `e`, and `e²`;
- predeclared finite-sample acceptance margins;
- explicit HLL-path activation and resource-failure evidence.

## Final status

```text
CHAPTER 34 HLL QUALITY CONTRACT:
    CLEAN — CLOSED

HLL-AUD-1:
    CLOSED

HLL-A12-AUD-1:
    CLOSED

HLL-A12-AUD-2:
    CLOSED

CHAPTER 34 ARCHITECTURE:
    NARROW HLL REOPENING CLOSED

CHAPTER 41 ARCHITECTURE:
    CLOSED / UNMODIFIED

V41-G1 — HLL DISTRIBUTIONAL QUALITY:
    UNBLOCKED — VERIFICATION REPAIR PENDING

V41-G2 — B+ CONCURRENCY VERIFIER SHOULD:
    OPEN / UNMODIFIED

CHAPTER 41 VERIFICATION:
    NOT YET SYNCHRONIZED

IMPLEMENTATION:
    NOT AUTHORIZED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

INDEX:
    UNMODIFIED

REVIEW ARTIFACTS:
    PRESERVED / UNMODIFIED

AUDIT-CREATED CHANGES:
    NONE

NEXT AUTHORIZED TASK:
    CHAPTER 41 — VERIFICATION SYNC FIX 1:
    DETERMINISTIC HLL ERROR-DISTRIBUTION
    ACCEPTANCE ORACLE
    (RETRY AFTER ARCHITECTURE RE-CLOSURE)
```

END CHAPTER-34/41 HLL QUALITY-CONTRACT INDEPENDENT READ-ONLY ARCHITECTURE CLOSURE AUDIT — A1+A2+A3.

**CHAPTER 41 — VERIFICATION SYNC FIX 1, retry: CLOSED V41-G1.** [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25650) now gives §41.6’s HLL error-distribution obligation an exact pre-bound set-invariance oracle and a predeclared post-bound population-quality acceptance rule. The narrow V34-043 wording was also aligned with HLL chunk invariance. No new procedure ID or dedicated V41 suite was created.

The exact oracle compares canonical binary64 pre-bound estimates for the same independently established distinct set across singleton, paired, mixed and skewed duplicates; order changes; and chunk boundaries. It covers applicable HLL scalar families and FLOAT64 canonical edges. It does **not** require equal register state or equal post-bound singleton/pair candidates. Genuine owner failures retain their causes; presentation shape alone is not a new failure cause.

The quality campaign fixes `p=14`, `m=16,384`; `N={65,536, 262,144, 1,048,576}`; five deterministic paired/order/skew policies; and 128 trials per `(N,P)` cell. Floyd sampling with a replayable test-only SHAKE256 stream constructs the INT64 sets. Each trial must prove the HLL path ran and use the actual bounded candidate, independent exact `N`, `e=(N_hat−N)/N`, and `e²`. From `1≤N_hat≤R`, the procedure derives each policy’s error support. It checks `E[e²]≤4/m` and `|E[e]|≤1/√m` using fixed one-sided Bernstein margins, `α=0.01` divided across 45 checks. Missing trials, invalid candidates, and resource failures cannot become quality PASS samples. Seeds, inputs, aggregates, margins, and failure causes are replayable. These finite checks are evidence for—not a redefinition or exhaustive proof of—§34.9’s population and universal-policy contract.

The requested statistical self-review A–O and pre-bound transfer review A–L have no procedural gaps; negative controls A–Q have deterministic dispositions. No per-sketch error band, new Architecture threshold, production RNG, or persisted field was introduced. V41-G2 was not changed.

**Repository state:** Initial and final HEAD are `b97f676fdf2ef6492c84bb517db2b84b705e007f` (`chapter 41 ARCHITECTURE analysis`). Contrary to the expected clean index, the live index already contained the staged Architecture `+48/−0` repair and staged Chapter-41 `SYNC.md`; tracked `ANALYSIS.md` was untouched. Both staged items remain unchanged. This task’s only delta is unstaged [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25650), `+134/−7`. Chapter 41 Architecture, Development, Project State, source, tests, benchmarks, and review artifacts were not edited. `git diff --check` passed. No build, test, sanitizer, or benchmark was run.

CHAPTER 34 HLL QUALITY CONTRACT: CLEAN — CLOSED / UNMODIFIED
CHAPTER 34 ARCHITECTURE: NARROW HLL REOPENING CLOSED
CHAPTER 41 ARCHITECTURE: CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 1: CLOSED V41-G1
V41-G1 — HLL DISTRIBUTIONAL QUALITY: CLOSED
V41-G2 — B+ CONCURRENCY FULL-TREE VERIFIER SHOULD: OPEN / UNMODIFIED
ATOMIC O41 OBLIGATIONS: 140
REMAINING VERIFICATION GAPS: 1
DEDICATED V41 SUITE: NOT CREATED
CHAPTER 41 VERIFICATION: SYNCHRONIZED EXCEPT V41-G2
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: UNMODIFIED (PRE-EXISTING STAGED ITEMS PRESERVED)
REVIEW ARTIFACTS: PRESERVED / UNMODIFIED
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
NEXT AUTHORIZED TASK: CHAPTER 41 — VERIFICATION SYNC FIX 2: B+ CONCURRENCY POST-QUIESCENCE FULL-TREE VERIFIER COMPOSITION (CLOSE V41-G2)

END CHAPTER-41 VERIFICATION SYNCHRONIZATION FIX 1 — RETRY.

**CHAPTER 41 — VERIFICATION SYNC FIX 2: CLOSED V41-G2 at the repair level.** The [Concurrent Tests procedure](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2870) now composes the existing §8.28 full-tree/L3 verifier with B+ concurrency cases at a stable post-quiescence checkpoint. Chapter-41 Verification is synchronized at the repair level; independent closure has **not** been performed.

The added procedure requires proof that the intended concurrent schedule and any named structural event occurred, followed by the existing direct content, latch, cursor, restart, and progress oracles. Workers must then reach their owned completion or restart boundary, finish applicable deadlock resolution, release required guards and pins, and cease tree mutation before a **fresh** L3 traversal runs and passes. Skipped L3, a reused earlier result, or an L3 failure makes the case non-PASS. The composition covers controlled reader/writer, disjoint and hot-range writer, duplicate, split/merge, scan/write, tiny-BufferPool, and applicable deadlock/restart cases; stress uses safe quiescent checkpoints and final quiescence. It does not add an online verifier, a production pause API, or a per-operation cadence. No coverage-map change or new procedure ID was needed.

Self-review: nonvacuity A–G is covered by event traces, owned completion, direct assertions, fresh post-quiescence invocation, and checked L3 result. Safety A–G is **not required** by the new procedure: L3 does not race mutators, replace direct oracles, or require implementation or a debug SQL API. §41.2’s frequent-use **SHOULD** remains a verification-methodology obligation, not a new production MUST. The B+ consistency search found no competing full-tree verifier or contradictory cadence rule.

At task entry, HEAD was `b97f676fdf2ef6492c84bb517db2b84b705e007f` (`chapter 41 ARCHITECTURE analysis`). Architecture `+48/−0` and Chapter-41 `SYNC.md` were staged; the HLL Verification repair `+134/−7` was unstaged. During the read-only inspection, another actor staged that HLL repair and the then-unstaged `SYNC.md` changes. **I did not alter the index.** The task-created delta is only `+25/−0` unstaged lines in `docs/VERIFICATION.md`; the total current Verification change against HEAD is `+159/−7`. The HLL subsection’s content hash remained unchanged. Staged Architecture, review artifacts, and tracked `ANALYSIS.md` were not edited by this task. Final HEAD is unchanged; `git diff --check` passed. No build, tests, sanitizers, or benchmarks were run.

CHAPTER 34 HLL QUALITY CONTRACT: CLEAN — CLOSED / UNMODIFIED
CHAPTER 34 ARCHITECTURE: NARROW HLL REOPENING CLOSED
CHAPTER 41 ARCHITECTURE: CLOSED / UNMODIFIED
V41-G1 — HLL DISTRIBUTIONAL QUALITY: CLOSED / UNMODIFIED
VERIFICATION SYNC FIX 2: CLOSED V41-G2
V41-G2 — B+ CONCURRENCY FULL-TREE VERIFIER SHOULD: CLOSED
ATOMIC O41 OBLIGATIONS: 140
REMAINING VERIFICATION GAPS: 0 AT REPAIR LEVEL
DEDICATED V41 SUITE: NOT CREATED
CHAPTER 41 VERIFICATION: SYNCHRONIZED AT REPAIR LEVEL
INDEPENDENT VERIFICATION CLOSURE: NOT PERFORMED
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: CHANGED EXTERNALLY DURING TASK; NOT MODIFIED BY THIS TASK
REVIEW ARTIFACTS: PRESERVED / UNMODIFIED BY THIS TASK
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
NEXT AUTHORIZED TASK: CHAPTER 41 — INDEPENDENT READ-ONLY VERIFICATION CLOSURE AUDIT

END CHAPTER-41 VERIFICATION SYNCHRONIZATION FIX 2.