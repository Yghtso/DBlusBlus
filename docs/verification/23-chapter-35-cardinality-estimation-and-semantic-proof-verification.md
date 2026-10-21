# Chapter 35 — Cardinality Estimation and Semantic-Proof Verification

This family composes the existing logical-semantics, statistics, physical-planning, cost,
resource, and diagnostic procedures at Chapter 35's integration boundaries. It does not
replace their component suites. A conforming harness may use immutable estimator records,
trace events, controlled descriptor snapshots, independent reference arithmetic, or
equivalent inspectable state; no estimator class, method, object layout, or tracing library
is required.

## V35-A — Estimation event, identity, and ownership oracle

For every fixture maintain an independent ledger keyed by optimization invocation and bound
logical-node identity. Correlate resolved `LogicalSlotId`/type inputs, caller-visible catalog
descriptors, each retained table's complete StatsVersion, estimator and fallback
configuration identities, consumed statistical fields, selected specialized/generic model,
complete truth triple, cardinality, representation-specific width, numeric finalization or
saturation, confidence/provenance chain, independent semantic-proof record, downstream cost
input, and final plan validation/handoff. Production estimates, plan shape, and execution
results are observations, never their own identity oracle.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-001 | Optimize one typed query whose valid statistics drive an equality Filter, Project, join, and final legal access path. | The ledger observes identified retained inputs, the actual estimator path and finite outputs, confidence/provenance, cost consumption, and validation of the same selected plan. This is the positive instrumentation control. |
| V35-002 | Disconnect in turn the selected generation, fallback-config identity, estimate, provenance, proof-source identity, cost handoff, and validation event. | Any missing essential evidence yields `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from output coincidence. |
| V35-003 | Arm an arithmetic/model-selection fault or boundary, hold execution before it, then release a second run through it. | The first run fails setup; the second proves the path and fault/boundary were reached before its canonical outcome is assessed. Untriggered injection cannot PASS. |
| V35-004 | Give equal names, equal payload values, and equal numerical estimates to different table/generation/slot identities. | The ledger distinguishes objects, generations, aliases, slots, estimate provenance, and logical row occurrences; equality of names or outputs proves none of them identical. |
| V35-005 | Run matched numerical-zero, structural-exact, exact-proof, missing-statistics, stale-statistics, unknown-count, saturated, and actual-runtime-count cases. | Each state is independently distinguishable; no estimate/provenance field is accepted as proof or runtime actual, and proof metadata identifies an approved §35.2 source. |

Every later V35 procedure that names a model, arithmetic, fault, race, or validation
boundary inherits V35-003: the harness must prove that boundary was entered and the intended
condition occurred before assessing the expected outcome.

## V35-B — Equijoin zero domains, 3VL, MCV, and uniqueness

Use independently computed Chapter-17 equality results and exact rational/dyadic fixture
fractions. Record child rows, null/non-NULL fractions, NDVs, selected branch, MCV/residual
populations, output estimate, truth triple, proof state, and executable alternatives.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-006 | Join two nonempty all-NULL key columns with valid `null_fraction=1`, `NDV=0`, and no MCV/histogram. | §35.7's zero-domain branch is observed; TRUE and join rows are zero, UNKNOWN is one, FALSE is zero, no division occurs, and no empty proof is created. |
| V35-007 | Repeat with only the left, then only the right, non-NULL domain zero. | Each one-sided case takes the zero branch without division and preserves `UNKNOWN = 1-(1-nA)*(1-nB)` and residual FALSE. |
| V35-008 | Exercise neither/partial NULL populations and NDV-one domains. | The independent triple is finite and normalized; TRUE is bounded by joint non-NULL mass, FALSE is its remainder, and INNER output uses TRUE alone. |
| V35-009 | Analyze a zero-domain generation, commit matching non-NULL rows later, then plan and execute while retaining the stale descriptor. | The estimate may be zero with `STALE_STATISTICS`, but an executable join remains and returns all visible matching pairs. |
| V35-010 | Make common heavy hitters consume all non-NULL mass, then independently make residual mass or residual NDV zero. | Common MCV pair mass is added once; zero residual contributes zero without division or MCV/histogram double count. |
| V35-011 | Use differing heavy hitters, duplicate-heavy common keys, and partial NULL populations. | SQL equality identifies common MCVs, duplicate pair multiplicity is retained, residual population stays separate, and the final triple/cardinality is finite. |
| V35-012 | Contrast a trusted enforced unique non-NULL key with nullable/inapplicable uniqueness and NDV that merely equals row count. | Only the trusted eligible case applies §35.9's pair bound; NDV alone proves neither uniqueness nor emptiness, duplicates remain semantic occurrences, and foreign-key refinement is not required. |

## V35-C — Fallback configuration, validation, and precedence

Supply test-owned configuration values; do not infer product defaults. For each named field
record trigger, validated configured input, legal domain/unit, competing available model,
chosen value, confidence/provenance, and downstream consumer.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-013 | Enumerate base rows; logical/value, stored-tuple, and operator-temporary byte widths; equality/range selectivity; NULL fraction; NDV; generic 3VL; and unknown OFFSET/LIMIT effects. | Every live §35.25 field is present when needed, validated in its canonical domain, and traceable to the estimate that consumes it; an unused field need not be consulted. |
| V35-014 | Inject negative, NaN, and infinite base rows and widths, plus a width tagged for the wrong representation. | The existing configuration/invariant owner prevents every raw invalid value from reaching cost or plan metadata; any owner-permitted finalization is recorded and cannot disguise wrong units or nonfinite input. No new public error enum is assumed. |
| V35-015 | Inject selectivities outside `[0,1]`, nonfinite components, incomplete triples, and a triple with a large normalization inconsistency. | Observe whether the exact owner rejects or performs its permitted clamp/replacement; only a complete finite normalized triple may reach Filter/join costing, and small-drift normalization cannot hide a large inconsistency. |
| V35-016 | Supply unknown OFFSET/LIMIT effects outside their §35.20 bounds and a valid effect at each endpoint. | The exact owner rejects or bounds/finalizes only as canonically permitted; no out-of-bound value reaches costing, while endpoints/interior remain deterministic configured inputs rather than evaluated counts. |
| V35-017 | Plan an unanalyzed table, then no qualifying generation, then malformed newest plus valid older generation. | Chapter-34 selection runs first; valid-old wins when present, otherwise the finite nonnegative base-row fallback is used with LOW/MISSING provenance and no synchronous ANALYZE. |
| V35-018 | Retain a valid stale zero-row generation and separately reject all compatible generations. | The former remains input tagged `STALE_STATISTICS`; the latter uses `MISSING_STATISTICS`; neither proves current emptiness. |
| V35-019 | Remove each width category while keeping fixed-type/layout derivation available, then remove that derivation. | Known/derivable width wins; otherwise only the matching finite byte fallback is used. Logical/value, stored, and temporary widths are never substituted for one another or treated as allocation. |
| V35-020 | Offer an exact structural input, specialized estimator, valid statistics, known layout/count, and trusted constraint alongside corresponding fallbacks. | Canonical exact/specialized/valid/known/trusted inputs win deterministically; cost preference cannot select a less authoritative estimation model. |
| V35-021 | Combine fresh evidence with one missing field, stale evidence, generic assumptions, independence, and damping. | The estimate retains all material provenance and the least-confident material assumption; missing, stale, and non-statistical configured fallback remain distinguishable from semantic-proof provenance. |

## V35-D — Generic predicate and selectivity integration

Compose `Selectivity Estimation Tests` with the V35-A identity ledger. The independent
oracle evaluates exact Chapter-17 NULL/operator behavior first and supplies configured
probability only for unresolved statistical mass.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-022 | Estimate an unsupported specialized predicate over non-NULL, nullable, and trusted-NOT-NULL inputs. | The chosen generic path returns a complete finite normalized triple, preserves known UNKNOWN mass, uses TRUE only for Filter, and carries LOW configured-model provenance. |
| V35-023 | Use a NULL constant and other known operator-NULL behavior, with and without column statistics. | Exact SQL 3VL takes precedence over the generic assumption; UNKNOWN is never folded into FALSE and exact no-TRUE proof arises only where §35.2 permits it. |
| V35-024 | Place a potentially erroring demanded expression under a generic estimate whose TRUE fraction is zero. | The estimate remains advisory; V20-15/16 demand and error behavior remains executable and final validation rejects an estimate-derived elimination. |
| V35-025 | Exercise equality NULL, MCV hit, residual equality, outside min/max, and range/histogram endpoints including duplicate-heavy boundaries. | Model selection, complete triples, MCV/residual separation, boundary inclusion, confidence, and provenance match §§35.6/35.10 without statistics-derived proof. |
| V35-026 | Exercise IS NULL/IS NOT NULL, IN/NOT IN with duplicate/NULL list members, and NOT over controlled triples. | Exact Chapter-17 transformations and complete triples hold; filtering retains TRUE only and statistical extrema do not originate proof. |
| V35-027 | Exercise independent AND/OR, repeated same-column constraints, exact contradictory typed bounds, and correlated columns without extended statistics. | Same-column intersection precedes independence; exact contradiction has approved proof, independence/correlation uncertainty is exposed, and no optional multicolumn model is required. |

## V35-E — Known and unknown LIMIT/OFFSET and exact K

Use Chapter-19-admitted folded/residual count expressions. Observe planning-time value
availability, configured effect, application order, estimate/proof, one execution-start
acquisition, runtime validation/error, exact mathematical K, physical eligibility, and
final ordering provider.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-028 | Cover known LIMIT zero/positive, OFFSET zero/positive, both counts, numerical-zero child, and independently proven-empty child. | OFFSET precedes LIMIT with exact max/min transfer; only actual LIMIT zero or propagated child proof supplies proof, and known semantics match V20-11. |
| V35-029 | Use unknown LIMIT with absent/known OFFSET and unknown OFFSET with absent/known LIMIT. | The corresponding bounded configured step is used without early evaluation; output is finite/LOW, positive non-proven input is not exhausted by the assumption, and provenance identifies the expression and fallback. |
| V35-030 | Make both counts unknown and vary both configured effects at legal interior/endpoints. | OFFSET then LIMIT is deterministic, each output stays within its current child bound, and neither assumption is reported as an evaluated count or `MISSING_STATISTICS`. |
| V35-031 | At execution start make an unknown count become zero, NULL, negative, or raise its canonical scalar error. | Planning metadata changes no runtime acquisition, validation, demand, source provenance, or result/error owner. |
| V35-032 | Give a non-proven child numerical estimate zero while the unknown count assumption is exercised. | Numerical zero may propagate but creates neither `SQL_LIMIT_ZERO` proof nor permission to suppress count evaluation. |
| V35-033 | Compare known representable K, known K outside one implementation domain, unknown LIMIT, unknown OFFSET, and both unknown under V22-J/K and V30-J. | Only exact representable K admits `PhysicalTopN`; all other cases retain a conforming ordering provider plus `PhysicalLimit`, and approximate estimates never authorize early stop. |

## V35-F — Structural transfers and saturating arithmetic

Tag every logical occurrence independently of value. Use symbolic/arbitrary-precision
reference arithmetic to detect overflow without materializing enormous bags; production
representation is not the oracle.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-034 | Exercise zero, one, duplicate, and potentially erroring `LogicalValues` declarations. | Estimate equals exact declared occurrences; duplicates remain distinct, zero carries only the approved structural proof, and positive row expressions remain demanded. |
| V35-035 | Exercise no-FROM constant and erroring Projects. | The source is exactly one zero-column occurrence, table-statistics fallback is absent, and every independently demanded expression retains its result/error. |
| V35-036 | Sort positive, numerical-zero/non-proven, and proven-empty children. | Child cardinality and estimate/proof provenance are preserved exactly; Sort changes order only and invents no proof. |
| V35-037 | CROSS ordinary, estimated-zero, proven-empty, and overflowing child estimates. | Pair estimate is the finite saturating product; occurrence multiplicity follows V20-6 and only §20.17.10 propagates exact emptiness. |
| V35-038 | Generic INNER with controlled TRUE/FALSE/UNKNOWN triples, including TRUE zero and substantial UNKNOWN. | Saturating child product times TRUE alone supplies matching-pair estimate; numerical zero is not proof and FALSE/UNKNOWN do not match. |
| V35-039 | Present a generic predicate fallback and then an eligible equijoin/MCV/unique model for the same join shape. | Generic transfer is used only without a specialized model; applicable §§35.7–35.9 take precedence and no competing join estimator runs. |
| V35-040 | Evaluate products below, at, and above the finite bound; zero times huge; huge product times tiny TRUE; and repeated composed products. | Operands/path and saturation are observable, no intermediate NaN/infinity/undefined overflow escapes, and the finite result remains estimate metadata. |
| V35-041 | Repeat §35.7 all-NULL and §35.8 zero-residual cases through the common numeric finalizer. | No zero denominator or nonfinite output occurs; saturation/clamping cannot legalize invalid Chapter-34 input or create proof. |

## V35-G — Operator cardinality, grouping, LEFT preservation, and width

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-042 | Compose Filter and Project over positive, numerical-zero, and exactly empty children with wide unused VARCHAR slots. | Filter multiplies by TRUE only; Project preserves rows and recomputes width from output slots; demand/proof follows V20-5/15/19. |
| V35-043 | Exercise DISTINCT and one/multiple-column GROUP BY with NULL classes, duplicates, missing NDV, correlated keys, and pre-cap overestimates. | NULL adds one group class, damping/cap is observed with its provenance, and approximate NDV proves neither uniqueness nor exact groups. |
| V35-044 | Compare global and grouped aggregation over a proven-empty and a merely estimated-zero child. | Global aggregate estimates/emits one row; grouped aggregate propagates exact empty only from proof; V29-B remains the semantic oracle. |
| V35-045 | Exercise LEFT joins with no/one/many TRUE matches, NULL keys, and unmatched preserved rows. | Estimated output never falls below nonempty left estimate, matched pairs differ from matched-left count, and exact left preservation remains unchanged. |
| V35-046 | Run INNER equality, generic INNER, CROSS, and LEFT fixtures with duplicates, NULLs, skew, stale, and missing inputs. | Correct specialized/generic model, full pair multiplicity, finite estimates, 3VL, confidence/provenance, and no illegal proof are observed together. |
| V35-047 | Remove logical/value, stored, and temporary width evidence independently for fixed, VARCHAR, join, aggregate, sort, and pruned Project outputs. | Each stage uses the applicable known/derived or matching fallback byte unit; projection/join RequiredSlotSet pruning changes width without changing logical rows. |
| V35-048 | Feed correct rows/widths through Chapters 36–38, then tag a wrong representation or wrong output-slot set with a plausible number. | Cost and selected-plan metadata consume the exact identified units/slots; inconsistent handoff is rejected by integration/final validation rather than silently costed. |
| V35-049 | Treat a width or peak-memory estimate as if it were a reservation, then inject runtime allocation/spill failure. | The false ownership inference is rejected; actual Chapter-24 accounting and Chapter-39 runtime failure remain independent of planning estimates. |

## V35-H — Stable statistics inputs and fallback classification

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-050 | Planner P retains T1/S1 and T2/S2 with unequal StatsVersions; pause, publish T1/S3, then resume and start Q. | P uses coherent S1/S2 throughout, Q may use S3, and estimator provenance names actual retained per-object inputs without global version equality. |
| V35-051 | Repeat V35-050 with two aliases of T1 and inspect every member field consumed. | Both aliases share one underlying retained generation while logical relation occurrences remain distinct; no member mixing or mid-invocation switch occurs. |
| V35-052 | Supply no generation, incomplete/malformed newest plus valid old, no compatible old, and valid stale input. | V34-C/J selection happens first; estimator observes exact valid-old, missing, or stale state and applies only the corresponding model/provenance. |
| V35-053 | Supply malformed outer catalog framing and separately a well-framed rejected advisory payload. | The former retains catalog-corruption ownership; only the latter may proceed through valid-old/missing advisory fallback. |
| V35-054 | Remove a specialized predicate model while keeping its type/query legal. | Generic complete-3VL fallback keeps downstream inputs finite and SQL legal; unsupported specialization is not invalid SQL. |
| V35-055 | Fix query, descriptors, payloads, estimator/fallback configuration, and numerical environment, then repeat; separately vary sample, valid StatsVersion, fallback, budget, or model availability. | Fixed inputs follow canonical deterministic paths; permitted input changes may alter valid estimates/plans but never SQL semantics or require cross-sample bit equality. |

## V35-I — Estimate/proof boundary, errors, and resources

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-056 | Contrast zero source estimate, zero equality TRUE, all-NULL join, stale range exclusion, generic INNER zero, and CROSS estimated-zero with visible rows/matches. | `is_provably_empty` remains false, executable alternatives survive, and differential execution observes required rows/errors. |
| V35-057 | Contrast zero-row Values, exact typed contradiction, actual LIMIT zero, and valid §20.17.10 propagated child proof. Also use a one-row INT32 fixture `x=0` with `WHERE (1/x = 1) AND NULL`: derive no-TRUE output and demanded division independently from Chapter-17 source-order/3VL rules, then offer an empty replacement that skips the division. Use `x=1` as the successful empty-output control. | Only the closed §35.2 sources create proof. Proof kind, demand conditions, replacement, and final validation are independently observed; no-TRUE output alone cannot authorize the error-suppressing replacement. The negative candidate is rejected, the demanded division error retains its owner/provenance, and the control emits no row. Missing proof, demand, or validator evidence is infrastructure-incomplete rather than PASS from an empty bag. |
| V35-058 | Forge proof provenance from statistics, confidence, saturation, fallback, or cost while giving the candidate the lowest cost. | Logical/optimizer validation rejects the candidate before execution; output coincidence cannot make fabricated proof valid. |
| V35-059 | Exercise missing advisory statistics, rejected advisory statistics, malformed outer framing, invalid fallback config, and a nonfinite estimator intermediate. | Each follows its existing fallback/configuration/corruption/invariant owner; no generic `EstimationError` or invalid SQL classification is invented. |
| V35-060 | Exhaust deterministic planning resources while a legal bounded fallback fits, then while it cannot fit. | Chapter 38 returns the legal fallback in the first case and Chapter 39 `OptimizerResourceLimit` in the second; estimate quality does not waive the bound. |
| V35-061 | Inject runtime allocation/spill failure and a demanded SQL/count-expression error into plans with uncertain or zero estimates. | Runtime resource and ordinary error owners remain distinct and observable; estimator uncertainty suppresses neither. |

## V35-J — Frozen-owner regressions, diagnostics, and end-to-end handoff

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V35-062 | Run estimate-dependent DML plans through V31-A/B/G/N around pre-W closure, W/C/R, retry, and result publication. | Estimates alter only legal plan choice; candidate/error closure, one mutation publisher, and transaction/result ownership are unchanged. |
| V35-063 | Vary worker count, morsels, scheduling, and legal early stop for estimate-different plans under V32-B/C/H/I. | Required occurrences, exclusive ownership, no replay, and owner-defined demand/error/result semantics remain intact; composed aggregate outputs use V22/V29’s result-family oracle. Estimated rows never become occurrence identity. |
| V35-064 | Vary estimates across legal alternatives, bounded search, properties, slots, objectives, and planning limits under V33-E–H. | Only capability-enabled legal plans may win; canonical fallback/ties/properties/final validation remain mandatory and no global optimum is promised. |
| V35-065 | Reuse V34-B/C/J/K with valid, stale, missing, rejected, unequal cross-table, and newly published generations. | Complete per-object stable inputs and advisory-only authority remain intact; Chapter 35 neither selects torn generations nor changes publication/GC. |
| V35-066 | Inspect Chapter-40 structured diagnostics against the V35 ledger and separately collected runtime counters. | Rows, widths/representation, StatsVersions, confidence/provenance, fallback path, proof, chosen plan, and actuals identify the same objects while estimates remain distinct from actuals. |
| V35-067 | Compose base rows, predicate, Project, join, aggregate/Sort, and LIMIT into one query and inspect every estimator-to-cost boundary. | Stage rows, slots, widths, provenance, cost inputs, selected metadata, and final validation form one coherent chain with no wrong-unit or mixed-generation field. |
| V35-068 | Differentially execute the same SQL with fresh, stale, missing, rejected, saturated, and configured-generic estimates. | Optimizer Differential Correctness Tests compare SQL bags, NULLs, demanded errors, visibility, and transaction behavior against the same owning semantic result family; exact cross-plan equality is required where the owner guarantees it. |
| V35-069 | Attempt mandatory foreign-key refinement, extended statistics, hard-coded fallback values, early count evaluation, approximate Top-N K, or estimate-based execution elimination. | Each is rejected as outside the baseline or contrary to its owner; optional capability absence is not failure and required baseline paths remain available. |

## V35 atomic architecture-obligation ledger

| Atomic range | Contract under test | Required reusable oracle |
|---|---|---|
| V35-001–V35-005 | Nonvacuous correlated estimation/identity/proof/cost ledger | V33-G; V34-B/K; `Optimizer Diagnostics Tests` |
| V35-006–V35-012 | Zero-domain equijoin, complete 3VL, MCV residuals, uniqueness | `Join Estimation Tests`; V34-F/G |
| V35-013–V35-021 | Fallback inventory, invalid configuration, selection, units, provenance | V34-C/K; `Cost Model Tests` |
| V35-022–V35-027 | Generic and specialized predicate selection with complete 3VL | `Selectivity Estimation Tests`; V20-5/15/16/19 |
| V35-028–V35-033 | Known/unknown counts and exact Top-N K | V20-11; V22-J/K; V30-J |
| V35-034–V35-041 | Five structural transfers and finite saturation | V20-4/6/10/15/19; `Cost Model Tests` |
| V35-042–V35-049 | Unary/group/join/width/cardinality-to-cost integration | V20-5/6/8; V29-B; V22-K |
| V35-050–V35-055 | Stable generations, valid-old/missing/stale/corruption, determinism | V33-G; V34-B/C/J/K |
| V35-056–V35-061 | Estimate/proof negatives and exact error/resource owners | `Semantic Emptiness Tests`; V22-I/K; Chapter-39 procedures |
| V35-062–V35-069 | Frozen owners, diagnostics, complete handoff, optionality | V31-A/B/G/N; V32-B/C/H/I; V33-G; V34-K |

## Chapter 35 invariant coverage map

| §35.27 invariant | Exact procedure, controlled evidence, and reusable oracle |
|---|---|
| 1 — finite nonnegative estimates | V35-014–V35-016, V35-040–V35-041; symbolic bounds and `Cost Model Tests` |
| 2 — explicit exact-proof separation | V35-005, V35-056–V35-058; V20-19 and `Semantic Emptiness Tests` |
| 3 — Filter retains TRUE only | V35-022–V35-027, V35-042; V20-5 |
| 4 — complete finite truth triples | V35-006–V35-008, V35-015, V35-022–V35-027 |
| 5 — comparison NULL mass is UNKNOWN | V35-007–V35-008, V35-023/V35-025 |
| 6 — IS NULL/IS NOT NULL never UNKNOWN | V35-026 with exact Chapter-17 oracle |
| 7 — IN/NOT IN NULL semantics | V35-026 with deduplicated lists and NOT transform |
| 8 — explicit 3VL AND/OR | V35-027 with independent truth-triple arithmetic |
| 9 — NDV excludes NULL; grouping adds NULL class | V35-006–V35-012, V35-043; V34-G and V20-8 |
| 10 — MCV/residual mass not doubled | V35-010–V35-011, V35-025/V35-041 |
| 11 — same-column constraints before independence | V35-027 |
| 12 — correlation uncertainty exposed | V35-027, V35-043 |
| 13 — damped/capped multi-column NDV | V35-043 |
| 14 — LEFT output lower bound | V35-045–V35-046; V20-6 |
| 15 — width accompanies rows | V35-019, V35-042/V35-047–V35-049 |
| 16 — semantic layer originates proof | V35-056–V35-058; V20-16/19 |
| 17 — exact-at-ANALYZE is not current proof | V35-009, V35-018, V35-056 |
| 18 — extrema/zero counts are cost evidence | V35-025, V35-052/V35-056 |
| 19 — compositions cannot create proof | V35-027, V35-038, V35-056 |
| 20 — exact §20.17.10 propagation | V35-028, V35-034–V35-038, V35-044/V35-057 |
| 21 — zero-domain equijoin totality | V35-006–V35-011, V35-041 |
| 22 — centralized finite fallbacks | V35-013–V35-021, V35-029–V35-032 |
| 23 — five structural/join transfers | V35-034–V35-039 |

## Chapter 35 subsection coverage map

| Architecture subsection | V35 integration coverage | Exact reusable coverage |
|---|---|---|
| §35.1 finite CardinalityEstimate | V35-005, V35-040–V35-041 | `Cost Model Tests` |
| §35.2 estimate/proof boundary | V35-005, V35-056–V35-058 | V20-16/19; `Semantic Emptiness Tests` |
| §35.3 row width | V35-019, V35-042, V35-047–V35-049 | V22-I/K; `Cost Model Tests` |
| §35.4 complete truth estimates/finalization | V35-006–V35-008, V35-015, V35-022–V35-027 | `Selectivity Estimation Tests` |
| §35.5 selectivity bounds/Filter transfer | V35-015, V35-022–V35-027, V35-042 | V20-5 |
| §35.6 equality model dispatch | V35-023–V35-026 | `Selectivity Estimation Tests` |
| §35.6.1 NULL constant | V35-023/V35-025 | V20-5/19 |
| §35.6.2 MCV hit | V35-025 | V34-F/G; selectivity tests |
| §35.6.3 outside statistical range | V35-025/V35-056 | `Semantic Emptiness Tests` |
| §35.6.4 residual equality | V35-025/V35-041 | `Selectivity Estimation Tests` |
| §35.6.5 non-NULL equality triple | V35-008/V35-025 | complete 3VL reference oracle |
| §§35.7–35.9 equijoin/MCV/unique | V35-006–V35-012, V35-039/V35-041 | `Join Estimation Tests` |
| §35.10 ranges | V35-025/V35-027 | `Selectivity Estimation Tests` |
| §35.11 NULL predicates | V35-022/V35-026 | V20-5/19 |
| §35.12 IN-list predicates | V35-026 | `Selectivity Estimation Tests` |
| §35.13 NOT | V35-026 | Chapter-17 3VL oracle |
| §35.14 AND | V35-024/V35-027 | V20-15/16 |
| §35.15 OR | V35-024/V35-027 | V20-15/16 |
| §35.16 same-column constraints | V35-027 | V20-16/19 |
| §35.17 correlation limitation | V35-027, V35-043, V35-069 | `Join Estimation Tests` |
| §35.17.1 structural/join transfers | V35-034–V35-041 | V20-4/6/10/19 |
| §35.18 Project cardinality/width | V35-042/V35-047 | V20-5/15 |
| §35.19 Filter cardinality | V35-022–V35-027/V35-042 | V20-5 |
| §35.20 LIMIT/OFFSET | V35-028–V35-033 | V20-11; V22-J; V30-J |
| §35.21 DISTINCT cardinality | V35-043 | V20-8 |
| §35.22 GROUP BY cardinality | V35-043–V35-044 | V20-8; V29-B |
| §35.23 multi-column damping | V35-021/V35-043 | `Optimizer Diagnostics Tests` |
| §35.24 LEFT JOIN | V35-045–V35-046 | V20-6; `Join Estimation Tests` |
| §35.25 fallbacks | V35-013–V35-024, V35-029–V35-032, V35-052–V35-054 | V34-C/K |
| §35.26 confidence/provenance | V35-001–V35-005, V35-021, V35-050–V35-055/V35-066 | `Optimizer Diagnostics Tests` |
| §35.27 invariants | V35 invariant map above | Listed exact owners |

## Chapter 41 Chapter-35 obligation coverage map

| Owner and obligation | Procedure and controlled input/fault | Observable result |
|---|---|---|
| §§35.1, 35.25, 41.6 base rows/fallback validation | V35-013–V35-018; valid and invalid configured/missing inputs | Finite rows or owner-correct rejection; valid-old precedes fallback |
| §§35.3, 36, 41.6 width/cost units | V35-019, V35-047–V35-049 | Representation-correct bytes reach cost; no reservation inference |
| §§33.4, 34.15, 35.26 compatibility/provenance | V35-050–V35-055 | Stable complete per-object inputs and exact missing/stale/corrupt distinction |
| §§35.6–35.9, 41.6 equality/NDV/joins | V35-006–V35-012, V35-025, V35-046 | Total equality estimates, full multiplicity, eligible refinements only |
| §§35.4, 35.11–35.15, 41.6 complete 3VL | V35-006–V35-008, V35-022–V35-027 | Finite normalized triples; TRUE-only filtering/matching |
| §§35.8, 34.14.6, 41.6 MCV/residual | V35-010–V35-011, V35-025/V35-041 | One MCV contribution, separated zero-safe residual |
| §§35.10, 35.16, 41.6 ranges/constraints | V35-025/V35-027 | Endpoint semantics and exact constraint intersection precede heuristics |
| §§35.17, 35.23 correlation/damping | V35-027/V35-043 | Uncertainty and damping provenance exposed; no optional model mandated |
| §§35.7–35.9, 35.17.1, 35.24 joins | V35-006–V35-012, V35-037–V35-039, V35-045–V35-046 | Finite pair estimates, specialized precedence, LEFT preservation |
| §§35.21–35.23, 29 aggregate/group cardinality | V35-043–V35-044 | NULL groups, cap/damping, global/grouped empty distinction |
| §§19.14, 35.20 counts | V35-028–V35-032 | Known exact and unknown bounded estimates retain runtime demand |
| §§22.4.1, 30.7, 38.15–38.16 exact K | V35-033 | Only exact representable K admits Top-N; conforming fallback remains |
| §§35.1, 35.4, 41.6 finite saturation | V35-014–V35-016, V35-040–V35-041 | No nonfinite/undefined result reaches cost or plan metadata |
| §§35.25–35.26 fallback precedence/provenance | V35-013–V35-021 | Canonical model wins and full material provenance/confidence propagates |
| §§35–38 estimator-to-cost/plan handoff | V35-048, V35-066–V35-068 | Identified rows/units/proofs reach one legal validated plan |
| §§20.17.10, 35.2, 41.6 semantic proof | V35-005, V35-024, V35-056–V35-058 | Estimate-zero stays non-proof; only approved exact source eliminates |
| §§35.26, 40 diagnostics | V35-021/V35-066 | Trace fields agree with retained inputs/path and remain distinct from actuals |
| Chapters 31–34 frozen obligations | V35-062–V35-065 | Estimate changes waive no mutation, occurrence, search, or generation rule |

## V35 reuse inventory and stale-rule audit

Exact reusable procedures are V20-4, V20-5, V20-6, V20-8, V20-10, V20-11,
V20-15, V20-16, and V20-19; V22-I, V22-J, and V22-K; V29-B; V30-J; V31-A,
V31-B, V31-G, and V31-N; V32-B, V32-C, V32-H, and V32-I; V33-E, V33-F,
V33-G, and V33-H; and V34-B, V34-C, V34-F, V34-G, V34-J, and V34-K. Reused
exact headings are `Selectivity Estimation
Tests`, `Semantic Emptiness Tests`, `Join Estimation Tests`, `Cost Model Tests`,
`Final Optimizer Validation Tests`, `Optimizer Differential Correctness Tests`, and
`Optimizer Diagnostics Tests`. V35 adds cross-stage identities, configured-input and
model-selection evidence, arithmetic boundaries, and downstream handoff where those
component suites alone are insufficient.

The live Verification procedures must contain no valid-zero-NDV division; no requirement
that valid all-NULL domains have positive NDV; no conversion of join UNKNOWN to FALSE; no
estimate-derived emptiness/uniqueness/error suppression; no SQL rejection or synchronous
ANALYZE for missing statistics; no undefined base-row or required-width input; no width-unit
conflation; no two-valued generic predicate fallback; no invented hard-coded fallback
default; no unknown count treated as evaluated zero or evaluated during planning; no exact
Top-N K derived from approximation; no loss of zero-row Values proof or duplicate Values
occurrences; no zero-row no-FROM source; no Sort cardinality change; no unsaturated
CROSS/INNER overflow or saturated estimate treated as actual; no generic model overriding
an eligible specialized equijoin; no planning/runtime-resource conflation; no global
StatsVersion equality; and no estimate-based weakening of V31/V32 semantics or demand for a
globally cheapest plan beyond canonical bounded search. Apparent matches are classified by
owner, stage, approximation, optional capability, or explicit negative-test context before
being treated as stale. Any required identity, configured input, arithmetic/model boundary,
proof, cost handoff, or validation evidence that is unavailable is `NOT VERIFIED / TEST
INFRASTRUCTURE INCOMPLETE`.
