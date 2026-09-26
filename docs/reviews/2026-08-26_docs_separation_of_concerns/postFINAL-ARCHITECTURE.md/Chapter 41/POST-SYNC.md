**CHAPTER 41 — FOCUSED HLL FINITE-SAMPLE ACCEPTANCE-ORACLE VERIFICATION FIX: V41-AUD-1 CLOSED AT REPAIR LEVEL.** The new rule controls false rejection for Architecture-conforming populations and has declared power against the specified gross violations. Independent Chapter-41 Verification closure remains pending.

The live repository differed from the expected baseline. Initial and final HEAD were `bab7ba110e0baa27a37c5e683d77a90091679d01` (`synced VERIFICATION after chapter 41 ARCHITECTURE fix`), with a clean index and no staged files. At entry, only Chapter-41 `SYNC.md` had a pre-existing unstaged edit; that edit disappeared externally during this task. I did not alter the index or review artifacts. The task-created change is solely [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25722), unstaged, **+91/−39**. The committed Architecture and Verification repairs were the live baseline, not pre-existing staged changes.

The repaired acceptance oracle keeps `m=16,384`, Architecture limits `RMS≤0.015625`, `E[e²]≤0.000244140625`, and `|E[e]|≤0.0078125`. It declares Verification-only gross alternatives of `RMS≥0.20` (`E[e²]≥0.04`) and signed bias `≥+0.10` or `≤−0.10`. Across 3 cardinalities × 5 policies × 3 checks, family-wise `α=0.01` gives `δ=1/4500` per check; each gross-alternative check has `β≤0.01`.

| Policies | Error support | `e²` upper bound | Trials per `(N,P)` | Acceptance |
|---|---|---:|---:|---|
| P1–P3 | `[1/N−1, 1]` | `1` | 1,600 | `x̄≤0.0033`, `−T1≤ē≤T1` |
| P4–P5 | `[1/N−1, 1+30/N]` | `(1+30/N)²` | 1,600 | same |

Here `T1 = 1/128 + √(2(4/16384)ln(4500)/1600) + 2(2+29/65536)ln(4500)/(3·1600) ≈ 0.01642612485866581`. Bounded-mean Chernoff/KL bounds calibrate M2 and gross-bias power; one-sided Bernstein, using the Architecture second-moment bound, calibrates good-side bias. The worst M2 good-side exponent is `8.858754` (`>ln 4500`); the gross-M2 exponent is `46.611273`. The worst direct positive and negative gross-bias exponents are `5.616447` and `5.616833`, giving false-acceptance bounds below `0.00364`. The M2 check supplies additional gross-bias power, but does not replace either signed-bias check. I cross-checked the calibration across all six support classes using two algebraically equivalent KL calculations; they agreed to floating arithmetic precision.

**PASS** requires every scheduled trial and pre-bound invariance check to complete, all structural/path/nonvacuity checks to pass, and all three calibrated inequalities to hold in every cell. Equality at a threshold passes; crossing it fails. Synthetic constant `+0.10`, constant `−0.10`, alternating `±0.20`, constant `+0.25`, and alternating `±0.25` all yield non-PASS; zero errors are on the quality-statistic acceptance side. An isolated tail sample is judged by the aggregates, not failed automatically. Missing trials, resource failures, nonfinite candidates, exact-mode substitution, unreplayable seeds, and post-hoc thresholds remain non-PASS under their respective causes.

The test’s indifference zone does **not** redefine Architecture conformance. PASS is reproducible finite evidence under the stated ideal independent uniform-set model—not proof of the exact population inequalities, all admissible policies, individual-estimate correctness, or a performance target. The fixed SHAKE256 run remains replayable sampling evidence, not literal independent randomness.

The N grid, P1–P5 policies, Floyd/SHAKE design, pre-bound invariance oracle, V34-043, and V41-G2 B+ procedure are unchanged. No Architecture contradiction or semantic drift was found. `git diff --check -- docs/VERIFICATION.md` and final global `git diff --check` pass; the initial global check had flagged the pre-existing `SYNC.md` edit that later disappeared externally. No build, project test, sanitizer, or benchmark was run.

CHAPTER 34 HLL QUALITY CONTRACT: CLEAN — CLOSED / UNMODIFIED
CHAPTER 34 ARCHITECTURE: NARROW HLL REOPENING CLOSED
CHAPTER 41 ARCHITECTURE: CLEAN — CLOSED / UNMODIFIED
V41-G1 — HLL DISTRIBUTIONAL QUALITY: REPAIRED AT FIX LEVEL
V41-AUD-1 — FINITE-SAMPLE ACCEPTANCE POWER: CLOSED AT REPAIR LEVEL
V41-G2 — B+ CONCURRENCY FULL-TREE VERIFIER SHOULD: CLEAN — CLOSED / UNMODIFIED
ATOMIC O41 OBLIGATIONS: 140
REMAINING VERIFICATION GAPS: 0 AT REPAIR LEVEL
CHAPTER 41 VERIFICATION: SYNCHRONIZED AT REPAIR LEVEL — INDEPENDENT RE-AUDIT PENDING
DEDICATED V41 SUITE: NOT NEEDED
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: CLEAN / UNMODIFIED BY THIS TASK
REVIEW ARTIFACTS: UNMODIFIED BY THIS TASK; PRE-EXISTING `SYNC.md` EDIT DISAPPEARED EXTERNALLY
AUDIT-CREATED CHANGES OUTSIDE AUTHORIZED EDIT: NONE
NEXT AUTHORIZED TASK: **CHAPTER 41 — INDEPENDENT READ-ONLY VERIFICATION CLOSURE AUDIT (RE-RUN AFTER HLL ACCEPTANCE-ORACLE FIX)**

END CHAPTER-41 FOCUSED HLL FINITE-SAMPLE ACCEPTANCE-ORACLE VERIFICATION FIX.

**CLEAN — VERIFICATION CLOSED.** I independently checked the live Chapter-41 obligations, the repaired HLL procedure and its statistical calibration, and the B+ concurrency composition. The prior power defect is closed. This is documentation-level closure only: no procedure was executed and no implementation is authorized.

### HLL acceptance-oracle re-audit

[§34.9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24745) still fixes `p=14`, `m=16,384`, RMS `≤0.015625`, `E[e²]≤0.000244140625`, and `|E[e]|≤0.0078125`. It separately owns exact pre-bound same-set invariance and post-bound INT64 population quality. [§34.14.6.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25298) supplies the writer bounds. Verification has not replaced these limits with its gross-error targets.

The complete [HLL procedure](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25675) retains independent distinct-set truth, exact pre-bound numeric comparison without register-state identity, singleton/pair/mixed/skew/order/chunk cases, applicable integer/BOOLEAN/DATE/TIMESTAMP/VARCHAR/FLOAT64 families, and canonical FLOAT64 edges. It permits different singleton/pair *post-bound* candidates. HLL activation, exact `N`, the post-bound `N_hat` endpoint, missing-trial handling, resource-cause preservation, and replay records remain explicit.

The grid is `N={65,536; 262,144; 1,048,576}`—`{4m,16m,64m}`. P1–P3 have `R=2N`; P4–P5 have `R=2N+30`. Thus `1≤N_hat≤R` yields six support classes: one paired and one skewed class at each `N`.

| Class | `e` support | `B`, upper bound on `e²` | Good M2 exponent | Gross M2 exponent |
|---|---|---:|---:|---:|
| 65,536 paired | `[1/N−1,1]` | 1 | 8.866873 | 46.655000 |
| 65,536 skewed | `[1/N−1,1+30/N]` | 1.000915737 | **8.858754** | **46.611273** |
| 262,144 paired | `[1/N−1,1]` | 1 | 8.866873 | 46.655000 |
| 262,144 skewed | `[1/N−1,1+30/N]` | 1.000228895 | 8.864842 | 46.644062 |
| 1,048,576 paired | `[1/N−1,1]` | 1 | 8.866873 | 46.655000 |
| 1,048,576 skewed | `[1/N−1,1+30/N]` | 1.000057221 | 8.866365 | 46.652265 |

The live rule schedules `1,600` trials per `(N,P)` cell: 15 cells, **24,000 policy trials**. It allocates `α=0.01` over 45 one-sided decisions, so `δ=1/4500` and `ln(4500)=8.411832675758…`; `β=0.01` is predeclared per gross alternative. Its exact M2 comparator is `x̄≤0.0033` (the decimal `33/10000`). Normalizing bounded `X=e²` by its support permits the stated upper- and lower-tail Bernoulli-KL bounds. Every good-side exponent exceeds `ln(4500)`; every gross-M2 exponent greatly exceeds `ln(100)`.

The exact bias threshold is the written formula, not its rounded display:
`T1 = 1/128 + √(2Q ln(4500)/1600) + 2(2+29/65536)ln(4500)/(3·1600) = 0.0164261248586658088…`.
The support width `2+29/65536` dominates both signs of `e` in all six classes. Under the **complete Architecture-good condition**, `Var(e)≤E[e²]≤Q`; the one-sided Bernstein bound at this conservative threshold has exponent about `9.912714`, exceeding `ln(4500)`. Under gross bias, the procedure instead uses support-only KL bounds—**not** the good-side RMS assumption. Recomputed worst exponents are `5.616447` for positive bias and `5.616833` for negative bias, with false-acceptance bounds `0.003638` and `0.003636`, respectively. The M2 check adds power against `|E[e]|≥0.10`, but the direct bias checks already meet `β`.

I recomputed all six classes with high-precision decimal arithmetic and cross-checked critical KL values using an algebraically different entropy expression. The formulas and inequality directions agree. Constant `+0.10`, constant `−0.10`, alternating `±0.20`, and the stronger `0.25` controls deterministically fail the observed-sample thresholds; zero errors satisfy their statistical side. Equality at a threshold passes and a value beyond it fails, subject to all other gates.

The [PASS wording](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:25777) correctly calls this finite evidence with an indifference zone. A population beyond an Architecture limit but below a gross target may pass or fail; it is **not** declared conforming. The stated probabilities apply to the ideal independent uniform-`S` model. Fixed test-only SHAKE256 supplies replay, not literal random independence; Floyd selection, unbiased bounded draws, signed mapping, and exact cardinality remain sound. No stale 128-trial HLL rule or old acceptance ceiling remains operative.

**V41-G1: CLEAN — CLOSED. V41-AUD-1: CLEAN — CLOSED.**

### Other Chapter-41 coverage

Live Chapter 41 is [“Verification Requirements,” lines 30096–30653](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:30096), with §§41.1–41.7 and Chapter 42 beginning at line 30654. It is unchanged; Chapter-34’s more detailed HLL contract adds no separate Chapter-41 atomic obligation. The historical **140** remains the valid inventory. I followed critical mappings to procedure bodies, rather than treating coverage tables as their own oracle:

- §41.1: byte/slot/free-list, heap/reopen, FSM, BufferPool and fault procedures remain covered.
- §41.2: deterministic, corruption, duplicate, randomized/reopen, and concurrency procedures remain covered. The committed [Concurrent Tests](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2870) preserve event and direct-content oracles, prove worker/restart/guard quiescence, and require a **fresh** existing §8.28 [L3 traversal](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2658) afterward. Skipped or stale L3 is non-PASS; stress is supplementary and §41.2’s SHOULD is not converted into an online production MUST.
- §§41.3–41.5: controlled crash/non-crash faults, committed-transaction recovery model, MVCC/lock models, catalog/front-end/logical validators, and physical/vector/DML procedures retain independent, nonvacuous oracles.
- §§41.6–41.7: statistics/estimator/proof separation and optimizer differential/reference-plan procedures remain covered. [V34-043](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:23432) still scopes exact chunk invariance to HLL without imposing it on other approximate collectors.

The focused unstaged diff changes only HLL finite-sample content; it does not touch B+. No circular critical mapping, conflicting cross-reference, Chapter-42 time/throughput gate, implementation-status claim, or new Architecture contradiction was found. All 140 obligations map to actual existing-suite procedures; direct body evidence and cross-suite reuse overlap rather than form a disjoint ledger. The two explicit Chapter-41 SHOULD methodologies—crash-point coverage and frequent full-tree verification—are covered. Partial: **0**; gap: **0**; optional obligation lacking a procedure: **0**. No dedicated V41 suite is needed.

### Repository and disposition

Initial and final HEAD: `bab7ba110e0baa27a37c5e683d77a90091679d01` (`synced VERIFICATION after chapter 41 ARCHITECTURE fix`). The index was clean with no staged files throughout. The worktree was and remains `docs/VERIFICATION.md` unstaged (**+91/−39**) plus pre-existing untracked Chapter-41 `POST-SYNC.md`. No external state change was observed during this audit; neither item was modified by it. `git diff --check` passes. New findings: **none**.

CHAPTER 34 HLL QUALITY CONTRACT: CLEAN — CLOSED
CHAPTER 34 ARCHITECTURE: NARROW HLL REOPENING CLOSED
CHAPTER 41 ARCHITECTURE: CLEAN — CLOSED
V41-G1 — HLL DISTRIBUTIONAL QUALITY: CLEAN — CLOSED
V41-AUD-1 — FINITE-SAMPLE ACCEPTANCE POWER: CLEAN — CLOSED
V41-G2 — B+ CONCURRENCY FULL-TREE VERIFIER SHOULD: CLEAN — CLOSED
ATOMIC O41 OBLIGATIONS: 140
REMAINING VERIFICATION GAPS: 0
CHAPTER 41 VERIFICATION: CLEAN — CLOSED
DEDICATED V41 SUITE: NOT NEEDED
IMPLEMENTATION: NOT AUTHORIZED
BUILD/TEST/SANITIZER/BENCHMARK: NOT RUN
INDEX: CLEAN; AUDIT MODIFIED: NO
REVIEW ARTIFACTS: `POST-SYNC.md` UNTRACKED; AUDIT MODIFIED: NO
AUDIT-CREATED CHANGES: NONE
NEXT AUTHORIZED TASK: **CHAPTER 42 — INITIAL READ-ONLY ARCHITECTURE ANALYSIS**

END CHAPTER-41 INDEPENDENT READ-ONLY VERIFICATION CLOSURE AUDIT — RE-RUN AFTER HLL ACCEPTANCE-ORACLE FIX.