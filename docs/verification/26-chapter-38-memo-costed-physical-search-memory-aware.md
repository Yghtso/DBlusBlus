# Chapter 38 — Memo, Costed Physical Search, Memory-Aware Optimization, and Resource-Ownership Verification

This section composes the existing semantic, physical-operator, memory, statistics,
estimation, cost, join-search, validation, and error procedures at Chapter 38's integration
boundaries. It does not duplicate their component suites. A conforming harness may use
immutable planning records, structured events, controlled configuration/fault hooks, and
independent mathematical models; no private class, API, container, trace encoding, or C++
layout is required.

## V38-A — Evidence, identity, and nonvacuity

Maintain one ledger keyed by optimizer invocation and bound logical-subproblem identity.
Correlate BindingIds, LogicalSlotIds, predicate occurrences, required slots, raw/normalized
ordering, row objective, retained descriptors/StatsVersions/configurations/capabilities,
PlanAlternative and structural-key identity, estimates, widths, costs, exact minimum and
anchored window, dominance, algorithms/orientations, pipeline phases/targets, spill work,
initial search mode, planning-arena guard/checkpoint/rollback, backing-allocation outcome,
terminal cause, selected plan, and both validation boundaries. A selected plan or SQL result
is an observation, never its own enumeration, pruning, costing, or resource oracle.

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-001 | Optimize one typed join query with ordered/unordered base paths, two applicable join algorithms, blocking state, final ordering, and a finite LIMIT. | One invocation ledger follows binding through alternatives, estimates, costing, memo/dominance, memory assignment, anchored selection, and successful final validation with consistent retained identities. |
| V38-002 | Reconstruct the V38-001 search requirements, legal alternatives, structural keys, finite arithmetic, target allocation, and resource/error decisions in independent set/tuple/arbitrary-precision models. | Every production event agrees with the independent models; equal names, costs, fingerprints, or output rows establish no identity. |
| V38-003 | Suppress each essential ledger class independently, including candidates, anchor/window, full key, phase targets, guard, allocation outcome, terminal cause, and validator reachability. | Each dependent verdict is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from a plausible winner or result. |
| V38-004 | Arm every later malformed input, omission, collision, guard, allocation denial, rollback, mixed-cause sequence, or validator rejection and record target invocation, reached boundary, trigger, and outcome. | An untriggered hook, unvisited candidate, clear guard claimed as triggered, allocator never called, or validator never offered the candidate is failed setup. |

All later V38 procedures inherit V38-003 and V38-004 where evidence or injection is named.

## V38-B — Memo, PlanAlternative, and dominance

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-005 | Hold RelationSet fixed while varying logical predicate identity, BindingId occurrence, and legal outer-join boundary. | Distinct logical subproblems do not collide; TableId or display equality never merges occurrences. |
| V38-006 | Vary required-slot class, normalized order including NULL/collation fields, ALL_ROWS, FIRST_K_ROWS(1), and FIRST_K_ROWS(100). | Every preference-changing requirement is memo identity; raw vectors that normalize identically share only the canonical class. |
| V38-007 | Exercise uniquely derived slots/no row goal and non-equivalent slot/objective cases. | The reduced RelationSet-plus-order key is used only where Architecture proves the omitted requirements equivalent. |
| V38-008 | Inspect retained alternatives for logical/prototype identity, rows/width/provenance, Cost, order, slots, memory/spill, proof, feasibility/capability, and full key; inject one missing field and execution-time mutable state. | Complete immutable planning state is retained; incomplete or mutable prototypes are rejected and a cheap invalid alternative cannot be selected. |
| V38-009 | Compare semantically equivalent alternatives varying objective cost, ordering, slots, feasibility, spill, memory, and proof provenance. | Dominance applies only after logical/search compatibility and preserves every requirement/feasibility advantage. |
| V38-010 | Offer a cheaper unordered path and a useful ordered path, then high-total/low-startup and low-total/high-startup pairs. | Useful ordering and the objective-relevant low-startup alternative survive until the applicable root comparison. |
| V38-011 | Give an invalid/ineligible plan the lowest cost and an unproven feasibility advantage to another candidate. | Semantic, property, capability, proof, and feasibility owners precede cost; uncertainty causes conservative retention, not unsafe dominance. |
| V38-012 | Insert a near-tied candidate, provisionally compare it, then discover a lower exact minimum whose final anchored window requires the earlier structural-key winner. | Incremental insertion retains the required candidate or a proven sufficient summary and matches batch selection over the completed eligible set. |

## V38-C — Validated anchored selection and structural identity

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-013 | Exercise `cost_tie_relative_epsilon` at default `1e-9`, zero, ordinary positive, and the greatest selected representable value below one. | A mandatory finite binary64 in `[0,1)` is validated before comparison and retained unchanged; it is not a Chapter-36 conversion coefficient. |
| V38-014 | Supply missing, negative, exactly one, greater than one, NaN, infinities, and unrepresentable epsilon; change external configuration mid-invocation. | Invalid inputs yield `OptimizerError` without clamp/default substitution; the active invocation retains its validated value. |
| V38-015 | Independently compute `m`, `max(1,m)`, checked tolerance, and saturating tie limit for zero, ordinary, huge, and saturated costs. | No NaN/infinity/overflow is evaluated; valid excess saturates, invalid raw cost is rejected, and valid saturation is ordinary metadata. |
| V38-016 | Compare exact equals, inside-window costs, outside-window costs, epsilon zero, and different ALL_ROWS/FIRST_K objectives. | One completed-set anchor defines membership; the structural key orders the window and exact objective cost then key order the remainder. |
| V38-017 | Use `a=100,b=109,c=119,epsilon=0.1` under every insertion and container order. | Anchor 100 and limit 110 place only a/b in the window; one structural winner results and any pairwise-nearness tournament fails. |
| V38-018 | Compare ordinary versus saturated and two saturated costs near `MAX_FINITE_COST`. | Ordinary exact order and anchored membership are preserved; equal saturated values resolve by full key and never imply resource failure or proof. |
| V38-019 | Build self-join leaves using one TableId/index but distinct BindingIds and output slots. | Full keys differ by occurrence/slot identity while equivalent reconstructions compare equal. |
| V38-020 | Vary join kind/algorithm/orientation, predicate occurrence/referenced set/attachment, output mapping, bounds/direction, ordering, exact LIMIT/OFFSET/Top-N K, objective-dependent mode, and child tree. | Every plan-relevant difference changes an authoritative tagged field; K=10 and K=11, INNER and LEFT, and one/two predicates remain distinct. |
| V38-021 | Encode absent/present-empty, empty/one-empty sequence, scalar/sequence variants, and `A|BC` versus `AB|C`. | Type tags, presence tags, fixed field order, and framed sequences prevent ambiguous equality. |
| V38-022 | Rebuild equal sets/children under reversed container insertion and allocation layouts; swap ordered children. | Canonical member order is stable, ordered child orientation remains distinct, and pointer/address values never enter comparison. |
| V38-023 | Force equal FNV-1a-64 fingerprints for unequal full keys. | Alternatives and memo states remain distinct; full-key comparison decides ties and all capability/slot/validation checks still run. |
| V38-024 | Repeat the complete candidate set under hash seeds, insertion order, allocator addresses, and worker scheduling. | Anchor, window, dominance, full keys, targets, and selected legal plan remain deterministic for fixed retained inputs. |

## V38-D — DP and operator-cost ownership

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-025 | Initialize each BindingId with SeqScan and every usable implemented index path under present/missing statistics and estimated zero. | One shared logical base cardinality feeds all paths; executable scans and useful orders survive and absent ANALYZE does not omit a legal path. |
| V38-026 | Enumerate chain, disconnected, hyperedge-only, overlapping-hyperedge, constrained LEFT, and four-way bushy regions against an independent admitted-space model. | Every Chapter-37-admitted subset/partition and legal algorithm/orientation is observed with complete predicate availability/ownership and no fabricated state. |
| V38-027 | Give omitted/duplicated/early predicates and a left-deep-only four-way implementation plausible final cardinalities. | Enumeration evidence detects each defect; one shared logical join estimate feeds all physical algorithms and final output cannot mask missing bushy work. |
| V38-028 | Cost HashJoin children, setup, build/probe hashing, matches, residuals, output, memory/spill, both legal INNER orientations, and canonical LEFT orientation. | Each work unit has one owner, the build remains blocking under first-K, and estimated-zero build rows neither remove execution nor proof emptiness. |
| V38-029 | Validate retained hash load factor `(0,1]`, power-of-two fanout `>=2`, and recursive-pass count `>=0`, including conditional capability absence. | Missing/invalid applicable values yield `OptimizerError`; valid values are stable runtime-owned inputs, not cost coefficients, and approximately 0.70 is only the initial tuning baseline. |
| V38-030 | Compute hash memory for zero/large rows, duplicate-heavy and varlen payloads, load-factor boundaries, and saturated arithmetic. | Pruned retained payload plus directory/chain metadata is counted once with checked finite arithmetic; zero estimate is not proof. |
| V38-031 | Exercise fit/equality/spill, zero target, capped fanout, one/multiple passes, zero/max pass count, unknown/skew/no-progress, and saturation; independently calculate the smallest power-of-two fanout at least `max(2,ceil(P/T))`, or the configured maximum when T is zero. | The selected fanout matches the checked rule and cap, initial and recursive work is charged once, termination occurs on fit/bound/no-progress, unresolved partitions charge Chapter-28 fallback work, and no prediction grants memory. |
| V38-032 | Cost NLJ with tiny/large inner, expensive child, one valid materialization, predicates, output, and first/full objectives. | Inner construction/materialization is neither free nor repeatedly charged after one retained materialization; child and local work remain distinct. |
| V38-033 | Cost INLJ with zero/unknown/large NDV, duplicate/unique keys, correlation, heap MVCC, residuals, and first-K. | Locality reduction is bounded and applied once; required lookup/heap/MVCC work never becomes free. |
| V38-034 | Enable/disable MergeJoin and vary predicate support, slot availability, exact four-field input ordering, enforcement Sort, duplicate groups, and output guarantee. | Only capable eligible forms enter; all child/enforcement/merge work is charged and only runtime-guaranteed ordering is advertised. |
| V38-035 | Cost Sort at N=0/1, narrow/wide/varlen records, in-memory, multiple runs, and merge passes. | Comparison, retained payload, blocking startup, temporary writes/reads, and merge CPU use checked finite ownership once. |
| V38-036 | Exercise Top-N K=N+O at zero, small, huge exact, implementation-unrepresentable, unknown, estimated-zero, and disabled capability. | Only a capable exact-K Top-N enters; a legal order provider/Sort plus Limit remains and estimates never manufacture K. |
| V38-037 | Compare HashAggregate, optional SortAggregate, hash/ordered DISTINCT, and ordering enforcement over empty/duplicate/wide/spilling inputs. | Input, state size/alignment, grouping/finalization, retained output, memory/spill, and Sort enforcement are complete; optional forms stay conditional and global aggregate output survives estimated zero. |

## V38-E — Objectives, demand, and retained payload

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-038 | Build startup/run/total values below/at/above finite bounds with zero, single-component, and sum-only saturation. | Components remain finite/nonnegative, `total=saturating_add(startup,run)`, and `startup<=total` without dropped or doubled child work. |
| V38-039 | Exercise ALL_ROWS and FIRST_K_ROWS at 0,1,100, large exact, objective-unrepresentable, Top-N-unrepresentable, estimated-zero, and LIMIT/OFFSET. | Exact objectives remain distinct memo inputs; fallback to ALL_ROWS is legal, approximate K is metadata only, and objective zero is not semantic proof. |
| V38-040 | Independently evaluate partial fractions below/at/above `[0,1]` for Scan/Filter/Project/Limit and blocking HashJoin build, Sort, aggregate, DISTINCT, and Top-N. | Canonical clamping/validation applies, necessary blocking work remains complete, and no general join row-goal propagation or executor stop is invented. |
| V38-041 | Apply first-K costing to scalar, EXISTS, and IN/NOT IN subplans with demanded errors. | Scalar still checks a second final row, EXISTS stops only after a lawful first final row, and IN/NOT IN completes its required RHS build and 3VL state. |
| V38-042 | Supply a source-ordered conjunction of total IMMUTABLE comparisons whose cost ranking favors the opposite order; independently record the bound child identities. Vary zero/unknown rejection probability and ties, then add erroring and demand-sensitive cases and VOLATILE expressions only where admitted by the scalar owner. Inspect metadata ranking, executable child order, and demanded occurrence evidence; offer a cost-reordered executable tree as the negative case. | Optional metadata ranking remains finite and never divides by zero. The executable tree retains the independently recorded §20.17.5 child order even for total IMMUTABLE operands; Chapter-17 demand and Chapter-20 provenance remain intact. The reordered tree is rejected, and missing child/demand evidence is infrastructure-incomplete rather than PASS from coincident SQL values. |
| V38-043 | Prune wide unused VARCHAR while retaining join/residual keys, computed ORDER slots, varlen ownership, Sort payload, hash payload, and aggregate state alignment. | Width and memory change only after lawful slot closure; required data and owned bytes remain through their consumer/lifetime. |
| V38-044 | Attempt to prune DML RID, assignment/RETURNING inputs, demanded erroring projection, and a compact handle whose source expires. | Candidate is rejected or repaired before costing/selection; no cost saving weakens DML, demand, or lifetime ownership. |
| V38-045 | Correlate every specialized node's child/local/repeated/blocking/enforcement work with one PlanAlternative identity. | No candidate borrows another candidate's work, target, spill, or proof; cost feedback changes preference only and cannot authorize semantics. |

## V38-F — Execution-memory phases and targets

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-046 | Validate `query_execution_memory_budget_bytes` at zero/positive/maximum and missing/negative/unrepresentable values; change it mid-invocation. | One retained Chapter-24 soft execution budget feeds costing; invalid configuration yields `OptimizerError` and it never aliases the planning arena or a runtime grant. |
| V38-047 | Build one/concurrent/sequential/nested blockers, Sort-plus-aggregate, Top-N-plus-upstream, repeated nodes, shared state, and a blocker spanning phases. | Chapter-26 dependencies define stable occurrence-key phases; shared state counts once and sequential states are not summed as concurrent. |
| V38-048 | Independently allocate `(need,budget)` of `80/100`, `80,80/100`, `10,80/100`, `10,80/50`, `10,80,80/100`, and sequential `80;80/100`. | Targets are respectively 80; 50/50; 10/80 with 10 unused; 10/40; 10/45/45; and separate 80 targets with peak 80. |
| V38-049 | Exercise zero need/budget, saturated need, indivisible remainder, capped shares, repeated redistribution, and reversed insertion order; also feed independently represented raw needs below one byte, exact integers, values just below/above integer boundaries, large finite/saturating values, and invalid raw values, recording raw estimate and converted need before allocator entry (for example `B=1`, `raw_need=[0.5]` and `B=3`, `raw_need=[0.5,1.5]`). | Each valid raw need is rounded upward to whole bytes exactly once with checked saturation (`0.5→1`, `1.5→2`, `2→2`, `0→0`), so the examples assign targets 1 and 1/2; integer quotient/remainder and occurrence-key order produce one bounded allocation with only integral operands, no target exceeds the converted need or budget, §38.20 fit/spill uses the same converted need, invalid raw values keep their existing failure ownership, and the loop terminates. |
| V38-050 | Give one blocker different phase allocations and repeated equal structural keys. | Occurrence path disambiguates nodes; minimum phase target drives conservative operator costing and phase peaks use coherent effective assignments without duplicate work. |
| V38-051 | Cost two alternatives with different liveness graphs under one query budget. | Each alternative receives its own phases, targets, spill, and objective cost; no target or spill estimate crosses PlanAlternative identity. |
| V38-052 | Make actual usage exceed estimates, deny a QueryMemoryManager grant, and inject runtime spill I/O failure. | Planner targets grant nothing; runtime recovery/errors remain V24/39-owned and predicted spill alone is neither OOM nor planning exhaustion. |

## V38-G — Planning resources and causal error ownership

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-053 | Validate `optimizer_planning_arena_budget_bytes` from deployment default, zero, ordinary/huge values, and missing/negative/unrepresentable effective values; mutate external config mid-plan. | One mandatory effective unsigned value is retained before search; malformed input is `OptimizerError` and no universal byte default is invented. |
| V38-054 | Observe guard arithmetic below, equal, and one byte above budget; zero budget; release/rollback; repeated growth; and arithmetic overflow. | The guard fires immediately before a growth only for projected charged bytes `>` budget; equality is allowed and overflow is internal/`OptimizerError`, not resource exhaustion. |
| V38-055 | Vary wall time, subproblem/partition/alternative/memo counters, plan cost, and container order while the byte guard remains clear. | These remain diagnostics and cannot trigger fallback or alter fixed-input selection. |
| V38-056 | Trigger the guard before useful DP state, after complete entries, mid-partition, and mid-alternative. | A region checkpoint exists; every region-specific partial/complete exhaustive entry is discarded, charged bytes roll back, and unrelated completed regions survive. |
| V38-057 | Restart bounded search after V38-056. | The same invocation, region, descriptors/statistics/configuration/objective/properties/capabilities/predicates are used; no execution starts and the selected result is finally validated. |
| V38-058 | Exercise `(N,limit)=(9,10),(10,10),(11,10),(1,0)`, near/equal guard-clear, equal guard-hit, and two opposite-side regions. | Below/equal starts exhaustive, above/zero-limit starts heuristic, only a real guard causes restart, and regions select independently. |
| V38-059 | Let bounded fallback fit, fail initialization by configured budget, exceed budget later, and start above threshold under an insufficient valid zero budget. | Success returns a validated plan; configured-bound inability yields `OptimizerResourceLimit`, while malformed config and high/saturated cost remain distinct. |
| V38-060 | With budget 1 GiB and charged/requested/projected 1/1/2 MiB, keep the guard clear and catchably deny the supported representable backing allocation. | The allocator invocation and denial are observed and unresolved terminal ownership is Chapter-39 `OutOfMemory`, never unconditional `OptimizerResourceLimit`. |
| V38-061 | From V38-060, let an already permitted safe mitigation first succeed and then, separately, encounter the same below-budget denial. | Successful recovery produces a validated plan; persistent physical denial remains `OutOfMemory` and the attempt does not reclassify its cause. |
| V38-062 | Execute guard→physical denial, physical denial→distinct guard, recovered denial→later guard, successful guard fallback→physical denial, and denial→successful recovery. | Ordered event evidence identifies the actual terminal preventing cause: respectively OOM, resource limit, resource limit, OOM, and success; no first/last-error precedence is invented. |

## V38-H — Statistics, diagnostics, validation, and semantics

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-063 | Retain unequal per-object StatsVersions, self-join shared descriptor, valid old/incompatible new/missing/stale inputs, concurrent ANALYZE, changed literals, and stale estimated zero with actual rows. | One compatible snapshot and provenance feed the invocation; later publication affects only later planners and no estimate or runtime actual rewrites proof/persistent statistics. |
| V38-064 | Inspect diagnostics for retained inputs, estimates/confidence/proof, alternatives, DP, costs, anchor/window/key/fingerprint, dominance, phases/targets/spill, guard/rollback/fallback/allocation cause, selection, and validation. | Trace identities agree with the ledger, collisions have no authority, wall time is diagnostic, and missing events cannot fabricate evidence. |
| V38-065 | Actually offer candidates with false order, missing slot/RID, illegal LEFT orientation, unavailable capability, unowned predicate, invalid Top-N K, forged proof, estimated-zero empty replacement, and NaN memory. | Each reaches and fails its canonical optimizer/execution validation boundary before execution or side effects; a legal LIMIT fallback remains. |
| V38-066 | Differentially execute composed nullable/duplicate joins, ORDER BY, hidden slots, LIMIT/OFFSET, scalar/EXISTS/IN, and applicable DML through several legal plans. | Bags, order, visibility, demand/errors, subquery cardinality/state, candidate closure, and effects match an independent semantic oracle while internal evidence remains separately required. |
| V38-067 | Compose malformed CostConfig/epsilon/hash/search/planning config, invalid/saturated cost, accounting overflow, guard, bounded exhaustion, physical denial/recovery, runtime memory/spill, capability, plan, and demanded SQL error. | Each event preserves `OptimizerError`, `OptimizerResourceLimit`, `OutOfMemory`, `SpillIOError`, ineligibility, internal, SQL-error, or successful-recovery ownership without new categories. |
| V38-068 | Vary one valid calibration, statistic, capability, objective, or budget while holding semantics fixed, then repeat all inputs exactly. | Legal plan choice may change with valid inputs; fixed inputs remain deterministic and every selected plan preserves SQL meaning. |

## V38-I — Frozen-owner integration

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-069 | Run cost/memory/search-different DML alternatives through frozen Chapter-31 procedures. | RID/hidden slots, assignments/RETURNING, candidate closure, W/C/R, canonical errors, retry, counts, and publication are unchanged. |
| V38-070 | Vary workers, morsels, scheduling, replay pressure, first-K, and blocking plans through Chapter-32 procedures. | Binding/search identity never replaces source/claim identity; coverage, no replay, cancellation, errors, and legal early stop remain worker-independent. |
| V38-071 | Exercise retained invocation inputs, capabilities, fallback, per-object statistics publication, and deterministic selection through Chapters 33–34. | One coherent invocation and compatible retained generations govern every decision; bounded search and final validation remain authoritative. |
| V38-072 | Exercise zero/unknown/saturated estimates, widths, proof separation, finite costs, seven coefficients, startup/run/total, and work ownership through Chapters 35–36. | Logical estimates/proof and physical cost/resource metadata remain separate; no estimate or cost changes SQL legality or resource ownership. |
| V38-073 | Exercise ordering normalization, slots, RelationSet, Cartesian prerequisites/components, predicate ownership, threshold, heuristic passes, algorithms/orientations, and subquery demand through Chapter 37. | Chapter-38 comparison/costing consumes but never weakens the closed search space, properties, cardinality independence, or subquery semantics. |
| V38-074 | Apply §41.7's optimizer obligations and use §42.6 counters only as measurement context. | Verification remains deterministic and falsifiable; no benchmark-specific decision, accuracy threshold, global optimum, or executed benchmark is required. |

## V38-J — Coverage and static integrity

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V38-075 | Independently evaluate every A–CL adversarial row plus the distinct physical-denial and mixed-cause rows below. | Each row reaches its controlled boundary and matches its operative owner; a matrix label or coincident result alone cannot PASS. |
| V38-076 | Parse live §38.25 and §§38.1–38.25 and correlate them with the invariant and subsection ledgers below. | All 19 invariants and 25 subsections have at least one substantive atomic fixture and independent expected observation; no summary-only coverage is accepted. |
| V38-077 | Extract every external procedure ID and named heading actually referenced by V38 and compare with the declared inventories. | Both set differences are empty, duplicates are absent, every target resolves exactly once, and capability-conditional reuse stays conditional. |
| V38-078 | Parse the live V38 section for family/ID continuity, unique definitions, coverage/reuse orphans, matrices, evidence controls, and document role. | Exactly one V38-A–J and one V38-001–078 definition exist; no missing/duplicate/orphan ID, broken reference, unsupported coverage assertion, vacuous injection, mandatory optional feature, or invented semantic rule may PASS. |

## V38 reuse keys

The matrices below use these exact reusable-oracle keys:

- `SEM` = V19-2, V19-14, V19-18; V20-6, V20-10, V20-13, V20-14,
  V20-15, V20-16, V20-20, V20-21.
- `PLAN` = V22-B, V22-D, V22-I, V22-J, V22-K, V22-L; V33-001,
  V33-002, V33-003, V33-004, and V33-010 through V33-029.
- `MEM` = V23-G, V23-I, V23-L; V24-D, V24-H, V24-L; V26-G, V26-I,
  V26-N, V26-O.
- `OPS` = V27-G, V27-N, V27-O; V28-B, V28-E, V28-F, V28-G, V28-K,
  V28-L, V28-M, V28-N, V28-O, V28-P, V28-Q, V28-R, V28-T; V29-B,
  V29-N, V29-O, V29-P; V30-C, V30-D, V30-F, V30-I, V30-J, V30-K.
- `EST` = V34-B, V34-C, V34-D, V34-J, V34-K; V35-B, V35-E, V35-G,
  V35-H, V35-I, V35-J; V36-001 through V36-005, V36-024 through
  V36-028, V36-059 through V36-070, and V36-073.
- `SEARCH` = V37-003 through V37-018, V37-023, V37-025 through V37-060,
  and V37-063 through V37-069.
- `FROZEN` = V31-A, V31-B, V31-E, V31-G, V31-H, V31-N; V32-A,
  V32-B, V32-C, V32-H, V32-I, V32-J.

Named reusable suites are `Memory/Spill Plan Tests`, `Memo and Pruning Tests`,
`Cost Model Tests`, `Optimizer Determinism and Resource-Limit Tests`, `Join-Order Tests`,
`Final Optimizer Validation Tests`, and `Optimizer Diagnostics Tests`.

For V38 reuse, `Optimizer Determinism and Resource-Limit Tests` instantiates its
“planning-arena/work budget” fixture only as §38.21's retained charged-byte guard; wall time
and work counters remain diagnostic. V33-022, V36-067, and V37-029 supply
`OptimizerResourceLimit` only when the configured planning bound prevents bounded
completion. V24-D/H/L supply the distinct catchable physical-denial oracle, while V33-029
is runtime-allocation/spill reuse and does not by itself test the planning-arena denial.

## V38 adversarial regression ledger

Each row inherits V38-003/004. The case text is the controlled fixture; the result column is
the independent expected observation, not an assertion derived from the selected plan.

| Case | Canonical owner | V38 procedure | Reuse | Controlled fixture and independently observable result |
|---|---|---|---|---|
| A | §38.1 | V38-005 | SEM/PLAN | Same RelationSet, different predicate identity remains distinct. |
| B | §38.1 | V38-006 | PLAN | Same logical state, different required slots remains distinct. |
| C | §§37.2,38.1 | V38-006 | SEARCH | Equivalent normalized order vectors share one canonical class. |
| D | §38.1 | V38-006 | SEARCH | Same RelationSet, different ordering remains distinct. |
| E | §§38.1,38.16 | V38-006/V38-039 | PLAN | ALL_ROWS and FIRST_K do not collide. |
| F | §§38.1,38.16 | V38-006/V38-039 | PLAN | Different exact K objectives do not collide. |
| G | §§19,38.1/5 | V38-005/V38-019 | SEM | Self-join aliases retain distinct BindingIds and keys. |
| H | §38.2 | V38-008 | PLAN | Missing capability state makes PlanAlternative incomplete/rejected. |
| I | §38.2 | V38-008 | PLAN | Execution-mutable state cannot enter a memo prototype. |
| J | §38.3 | V38-010 | PLAN/SEARCH | Cheaper unordered path cannot prune useful ordered path. |
| K | §§38.3,38.16 | V38-010 | PLAN/EST | Low-startup alternative survives under first-K. |
| L | §38.3 | V38-011 | PLAN | Infeasible low-cost plan cannot dominate a valid plan. |
| M | §§36.2.1,38.4 | V38-015/V38-067 | EST | Negative/NaN cost is rejected before comparison. |
| N | §§36.2.1,38.4 | V38-018 | EST | Valid saturation remains comparable metadata. |
| O | §38.4 | V38-013/V38-014 | PLAN | Invalid/unrepresentable epsilon is `OptimizerError`. |
| P | §38.4 | V38-015 | EST | Tolerance arithmetic checks/saturates without overflow. |
| Q | §38.4 | V38-017 | PLAN | Nontransitive pairwise chain cannot drive selection. |
| R | §38.5 | V38-019–V38-022 | PLAN | Physically different plans have unequal full keys. |
| S | §38.5 | V38-023 | PLAN | Fingerprint collision has no selection authority. |
| T | §§38.4–38.5 | V38-022/V38-024 | PLAN | Pointer changes do not alter key or winner. |
| U | §§38.4–38.5 | V38-024 | PLAN | Hash iteration cannot alter the final plan. |
| V | §38.6 | V38-025 | EST/SEARCH | Estimated-zero base retains SeqScan. |
| W | §38.6 | V38-025 | EST | Missing ANALYZE does not omit a legal index path. |
| X | §§38.6–38.7 | V38-025/V38-027 | EST | Access algorithms share logical cardinality. |
| Y | §38.7/Ch37 | V38-026/V38-027 | SEARCH | Three-relation predicate cannot activate at two bindings. |
| Z | §38.7/Ch37 | V38-027 | SEARCH | Predicate is neither charged nor attached twice. |
| AA | §38.7 | V38-026/V38-027 | SEARCH | Required four-way bushy transition is observable. |
| AB | §§37.15,38.7–38.8 | V38-026/V38-028 | OPS/SEARCH | Cheapest invalid LEFT orientation is excluded. |
| AC | §§38.8,38.16 | V38-028/V38-040 | OPS/EST | Hash build remains blocking. |
| AD | §38.9 | V38-030 | OPS/MEM | Duplicate-heavy retained payload is counted. |
| AE | §38.9 | V38-029 | OPS | Zero load factor is invalid before division. |
| AF | §38.9 | V38-031 | OPS/MEM | Nonfitting partition charges bounded fallback work. |
| AG | §38.10 | V38-032 | OPS/EST | Materialized NLJ inner is never free. |
| AH | §38.10 | V38-032 | OPS/EST | One retained materialization is not repeatedly charged. |
| AI | §38.11 | V38-033 | OPS/EST | Zero/unknown outer NDV uses bounded fallback costing. |
| AJ | §38.11 | V38-033 | OPS/EST | Locality discount cannot make all lookups free. |
| AK | §38.11 | V38-033 | OPS | Required heap MVCC work survives. |
| AL | §38.12 | V38-034 | OPS/PLAN | Unsupported MergeJoin is ineligible. |
| AM | §38.12 | V38-034 | OPS/SEARCH | Mismatched child order requires enforcement or exclusion. |
| AN | §38.13 | V38-035 | OPS/EST | Sort N=0/1 has finite valid cost. |
| AO | §§38.13,38.18 | V38-035/V38-043 | MEM/OPS | Wide varlen sort payload is retained/costed. |
| AP | §§38.13,38.20 | V38-035 | OPS/MEM | External Sort charges every required write/read pass. |
| AQ | §§38.13,38.15 | V38-036 | OPS/PLAN | Unrepresentable exact Top-N K is ineligible. |
| AR | §38.15 | V38-036 | OPS/PLAN | Valid LIMIT retains Sort/provider plus Limit fallback. |
| AS | §§38.15–38.16 | V38-036/V38-039 | SEM/OPS | Unknown LIMIT never becomes exact K. |
| AT | §38.16 | V38-039 | SEM/EST | FIRST_K_ROWS(0) is not semantic-empty proof. |
| AU | §38.14 | V38-037 | OPS/EST | Estimated groups zero does not remove global aggregate row. |
| AV | §38.14 | V38-037 | OPS/MEM | Aggregate state size/alignment is exact. |
| AW | §38.14 | V38-037 | OPS/PLAN | Unsupported ordered aggregate is not required. |
| AX | §38.14 | V38-037 | OPS/PLAN | Ordered DISTINCT requires capability. |
| AY | §38.15 | V38-037 | SEARCH/OPS | Satisfied order receives no unnecessary Sort. |
| AZ | §38.15 | V38-037 | PLAN/EST | Unordered plan pays enforcement before comparison. |
| BA | §§36.2.1,38.16 | V38-038 | EST | Startup plus run saturates rather than overflows. |
| BB | §38.16 | V38-040 | EST | Partial fraction cannot escape `[0,1]`. |
| BC | §38.16 | V38-040 | OPS/EST | Hash build/Sort blocking work cannot scale away by LIMIT. |
| BD | §38.16 | V38-040 | SEM/PLAN | General join first-K propagation needs a safe owner. |
| BE | §38.17 | V38-042 | SEM | VOLATILE predicate is not cost-reordered. |
| BF | §38.17 | V38-042 | SEM | Error-sensitive predicate is not statistics-reordered. |
| BG | §38.17 | V38-042 | EST | Zero rejection probability never divides by zero. |
| BH | §38.18/Ch31 | V38-044 | FROZEN | Projection pruning cannot lose DML RID. |
| BI | §38.18 | V38-043/V38-044 | MEM | Sort row handle cannot outlive backing storage. |
| BJ | §38.19 | V38-047–V38-049 | MEM | Concurrent blockers cannot each receive full budget. |
| BK | §38.19 | V38-047/V38-048 | MEM | Sequential blockers are not summed as simultaneous peak. |
| BL | §38.19 | V38-049 | MEM | Unused shares redistribute canonically. |
| BM | §§38.19–38.20 | V38-048/V38-052 | MEM | Need equal target predicts no spill solely from equality. |
| BN | §38.20 | V38-031/V38-035/V38-043 | MEM/OPS | Spill/materialization includes reads and deep copy once. |
| BO | §§38.20,24 | V38-052 | MEM | Estimated zero cannot suppress runtime spill handling. |
| BP | §§37.11,38.21 | V38-058 | SEARCH | Threshold equality remains exhaustive when guard clear. |
| BQ | §38.21 | V38-056/V38-057 | PLAN/SEARCH | Actual guard hit causes clean bounded restart. |
| BR | §§38.21,39.4 | V38-059 | PLAN | Bounded configured exhaustion is controlled resource limit. |
| BS | §38.22/Ch34 | V38-063 | EST | Concurrent ANALYZE cannot change retained snapshot. |
| BT | §38.22 | V38-063 | EST | Runtime actuals do not rewrite persistent statistics. |
| BU | §38.23 | V38-064 | EST | Trace keeps exact proof separate from confidence estimate. |
| BV | §§38.5,38.23 | V38-023/V38-064 | PLAN | Fingerprint never becomes correctness identity. |
| BW | §38.24 | V38-065 | PLAN/SEARCH | False ordering candidate reaches validator and is rejected. |
| BX | §38.24 | V38-065 | PLAN/SEARCH | Missing-slot candidate reaches validator and is rejected. |
| BY | §§38.15,38.24 | V38-065 | OPS/PLAN | Invalid Top-N cannot start execution. |
| BZ | §§38.6–38.7,38.24 | V38-025/V38-027/V38-065 | EST | Estimated-zero subtree cannot become unproven empty result. |
| CA | §38.24/Ch31 | V38-065/V38-069 | FROZEN | Mutation side effects cannot precede failed validation. |
| CB | §§33,36,38.4 | V38-068 | EST/PLAN | Different valid calibration may choose another legal plan. |
| CC | §§38.4–38.5 | V38-024 | PLAN | Fixed inputs cannot vary from unordered-container iteration. |
| CD | §§38.12,38.14–38.15 | V38-034/V38-037 | OPS/PLAN | Optional algorithm absence leaves a baseline legal plan. |
| CE | §§36.2.1,38.21 | V38-067 | EST/PLAN | High estimated cost is not `OptimizerResourceLimit`. |
| CF | §§38.19,38.21 | V38-046/V38-053 | MEM/PLAN | Planning arena and execution-memory target remain distinct. |
| CG | §§24,38.19/21 | V38-052/V38-067 | MEM | Runtime memory failure is not planning configuration error. |
| CH | §§35,38.7 | V38-027 | EST/SEARCH | Join cardinality cannot vary solely by physical algorithm. |
| CI | §§20.14,38.16 | V38-041 | SEM | Scalar first-row costing cannot skip second-row check. |
| CJ | §§20.14,38.16 | V38-041 | SEM | IN complete build cannot be skipped by first-K. |
| CK | §§35.2,38.3/24 | V38-009/V38-011/V38-065 | EST/PLAN | Memo pruning cannot lose or forge exact proof. |
| CL | §38.23 | V38-064 | EST | Diagnostics retain estimate/proof/configuration provenance. |

Additional physical-denial and mixed-cause cases:

| Case | Canonical owner | V38 procedure | Reuse | Controlled fixture and independently observable result |
|---|---|---|---|---|
| FB1 | §§38.21,39.3 | V38-060 | MEM/PLAN | Guard-clear supported allocation denial is unresolved `OutOfMemory`. |
| FB2 | §38.21 | V38-061 | PLAN | Safe physical-denial mitigation succeeds and validates. |
| FB3 | §§38.21,39.3 | V38-061 | MEM/PLAN | Persistent below-budget denial remains `OutOfMemory`. |
| FB4 | §§38.21,39.3–39.4 | V38-062 | MEM/PLAN | Mixed causes follow the actual terminal preventing cause. |
| FB5 | §§38.21,39.4 | V38-062 | PLAN | Distinct later configured guard remains `OptimizerResourceLimit`. |

## V38 error and resource outcome ledger

| Controlled cause | Required outcome | Owning procedure/oracle |
|---|---|---|
| malformed CostConfig or raw nonfinite cost | `OptimizerError`/internal numeric owner before comparison | V38-015/V38-067; EST |
| malformed epsilon | `OptimizerError` before alternative comparison | V38-013–V38-014; PLAN |
| malformed applicable hash/spill tuning | `OptimizerError` before affected costing | V38-029; OPS |
| malformed search or planning-budget configuration | `OptimizerError` before search state | V38-053/V38-067; PLAN/SEARCH |
| valid saturated cost | ordinary diagnostic cost metadata | V38-018/V38-067; EST |
| unrepresentable arena extent/accounting overflow | internal/`OptimizerError`, not valid exhaustion | V38-054/V38-067; EST/PLAN |
| configured guard during exhaustive DP | clean rollback and bounded restart | V38-056–V38-057; PLAN |
| configured bound prevents bounded completion | `OptimizerResourceLimit` | V38-059; PLAN |
| guard-clear supported backing allocation denied | `OutOfMemory` if unresolved | V38-060; MEM/PLAN |
| safe backing-denial mitigation succeeds | validated planning success | V38-061; PLAN |
| mitigation repeats below-budget physical denial | `OutOfMemory` | V38-061; MEM/PLAN |
| mixed physical/configured events | actual terminal preventing cause under §§39.3–39.4 | V38-062; MEM/PLAN |
| execution target predicts pressure/spill | cost metadata only | V38-046–V38-052; MEM |
| QueryMemoryManager denial with no exact progress | runtime `OutOfMemory` | V38-052/V38-067; MEM |
| temporary spill I/O/create/capacity failure | `SpillIOError` | V38-052/V38-067; MEM |
| unsupported physical capability | ineligible alternative, not invalid SQL/resource failure | V38-011/V38-034/V38-037; PLAN/OPS |
| malformed selected physical plan | internal validation failure before execution | V38-065; PLAN |
| demanded SQL expression/subquery error | existing SQL/runtime owner; cost cannot suppress it | V38-041–V38-042/V38-066–V38-067; SEM |
| high estimated plan cost | ordinary cost comparison | V38-067; EST |
| estimated zero or predicted spill | neither semantic proof nor an actual resource error | V38-025/V38-052/V38-063; EST/MEM |

## V38 invariant coverage ledger

| §38.25 invariant (actual obligation) | Owner | New procedure | Reuse | Positive/negative fixture and independent result |
|---|---|---|---|---|
| 1 — complete logical/search identity | §38.1 | V38-005–V38-007 | SEM/PLAN | Predicate/slot/order/boundary variants split; normalized equivalents unite. |
| 2 — finite row objective in identity | §§38.1,38.16 | V38-006/V38-039 | PLAN | ALL_ROWS/K=1/K=100 do not collide. |
| 3 — useful order/low startup survive | §38.3 | V38-009–V38-010 | PLAN/SEARCH | Cheaper incompatible alternatives cannot dominate. |
| 4 — finite epsilon and anchored total order | §38.4 | V38-013–V38-018 | EST/PLAN | Invalid epsilon, nontransitive chain, and saturation have exact outcomes. |
| 5 — container/pointer/scheduling independence | §§38.4–38.5 | V38-024 | PLAN | Perturbations preserve winner and assignments. |
| 6 — full key authoritative, fingerprint diagnostic | §38.5 | V38-019–V38-023 | PLAN | Self-join/framing/collision fixtures retain distinction. |
| 7 — shared base cardinality | §38.6 | V38-025 | EST | Seq/index alternatives consume one estimate. |
| 8 — algorithm-independent join cardinality | §38.7 | V38-026–V38-027 | EST/SEARCH | Hash/NLJ/INLJ/Merge consume one logical estimate. |
| 9 — capability gating | §§38.2,38.7/12/14/15 | V38-008/V38-034/V38-037 | OPS/PLAN | Fabricated cheap unsupported form is excluded. |
| 10 — widths/config/targets/bounded spill | §§38.9,38.18–38.20 | V38-029–V38-031/V38-043/V38-046–V38-052 | MEM/OPS | Invalid tuning, skew, payload, target, and runtime-boundary fixtures. |
| 11 — enforcement costs complete | §§38.13–38.15 | V38-035–V38-037 | OPS/EST | Sort/Top-N/aggregate/DISTINCT compare with all enforcement. |
| 12 — full versus first-K objective | §38.16 | V38-038–V38-041 | SEM/EST | Exact objectives differ without semantic early stop. |
| 13 — planning guard/fallback/resource owner | §38.21 | V38-053–V38-062 | PLAN/MEM/SEARCH | Guard, rollback, exhaustion, OOM, recovery, and mixed causes are distinct. |
| 14 — stable snapshot | §38.22 | V38-063 | EST | Concurrent publication cannot mix retained inputs. |
| 15 — missing/stale remain advisory | §38.22 | V38-063 | EST | Stale estimated zero retains execution and provenance. |
| 16 — explainable decisions | §38.23 | V38-064 | EST | Missing/colliding trace evidence cannot decide or PASS. |
| 17 — plan choice preserves SQL | §§38.17,38.24 | V38-042/V38-066/V38-068 | SEM/PLAN | Unsafe reorder and legal-plan differential fixtures preserve meaning. |
| 18 — validation before execution | §38.24 | V38-065 | PLAN | Every malformed winner is offered and rejected before effects. |
| 19 — estimated zero is not proof | §§38.6–38.7,38.24 | V38-025/V38-027/V38-065 | EST | Executable zero-estimate paths survive and forged proof fails. |

## V38 subsection coverage ledger

| Architecture subsection | New V38 procedure | Reuse | Controlled fixture / independent expected observation |
|---|---|---|---|
| §38.1 search requirements/memo | V38-005–V38-007 | SEM/PLAN | Complete preference-changing identity; canonical equivalence only. |
| §38.2 PlanAlternative | V38-008 | PLAN | Complete immutable alternative; missing/mutable state rejected. |
| §38.3 dominance | V38-009–V38-012 | PLAN | Compatibility, useful retention, and late-anchor safety. |
| §38.4 ties/choice | V38-013–V38-018 | EST/PLAN | Validated epsilon, checked anchor, total deterministic selection. |
| §38.5 structural key/fingerprint | V38-019–V38-024 | PLAN | Complete framed key; collision has no authority. |
| §38.6 DP initialization | V38-025 | EST/SEARCH | Complete base paths, shared estimate, estimated-zero execution. |
| §38.7 DP transition | V38-026–V38-027 | SEARCH | All admitted partitions, predicates, algorithms, properties, dominance. |
| §38.8 hash cost | V38-028 | OPS/EST | Child/build/probe/output/residual/memory work once. |
| §38.9 hash memory/spill | V38-029–V38-031 | MEM/OPS | Domains, formula, fanout, bounded recursion, fallback. |
| §38.10 NLJ cost | V38-032 | OPS/EST | Materialized inner charged once with product work. |
| §38.11 INLJ cost/locality | V38-033 | OPS/EST | NDV/correlation discount bounded; MVCC retained. |
| §38.12 MergeJoin | V38-034 | OPS/PLAN | Capability/order/enforcement/duplicate/output-property checks. |
| §38.13 Sort/Top-N | V38-035–V38-036 | OPS/MEM | In-memory/external costs and exact-K fallback. |
| §38.14 aggregate/DISTINCT | V38-037 | OPS/MEM | Hash/conditional ordered alternatives and retained output. |
| §38.15 ordering enforcement | V38-034/V38-036/V38-037 | OPS/SEARCH | Natural order competes with complete enforcement cost. |
| §38.16 row objective/startup | V38-038–V38-041 | SEM/EST | Exact objective, saturation, partial/blocking/demand boundaries. |
| §38.17 predicate CPU order | V38-042 | SEM/EST | Cost ranking is metadata only; executable source-child order and demand remain intact. |
| §38.18 payload pruning | V38-043–V38-044 | MEM/FROZEN | Slots/payload/lifetime preserved before cost savings. |
| §38.19 target assignment | V38-046–V38-051 | MEM | Exact phases, integer sharing, redistribution, multiphase identity. |
| §38.20 spill/materialization | V38-031/V38-035/V38-043/V38-052 | MEM/OPS | Strict `need>target`, bytes/passes/copies once, runtime authority. |
| §38.21 planning resources | V38-053–V38-062 | PLAN/MEM/SEARCH | Configuration, byte guard, rollback, threshold, OOM/resource cause. |
| §38.22 statistics | V38-063 | EST | Stable compatible per-object snapshot and advisory provenance. |
| §38.23 diagnostics | V38-064 | EST | Explain all decisions without changing them. |
| §38.24 final validation | V38-065 | PLAN | Actual malformed candidates rejected before execution/effects. |
| §38.25 invariants | V38-076 plus invariant ledger | all keys | Every operative invariant has a real positive/negative fixture. |
| §41.7 verification handoff | V38-074–V38-078 | PLAN/SEARCH | Deterministic properties/memo/cost/resource/validation evidence. |
| §42.6 measurement context | V38-074 | `Cost Model Tests`; `Optimizer Diagnostics Tests` | Counters support later measurement; no accuracy/global-optimum threshold. |

## V38 reuse inventory and stale-rule audit

SET A is the expansion of every external ID named by `SEM`, `PLAN`, `MEM`, `OPS`, `EST`,
`SEARCH`, and `FROZEN` above. SET B, the declared inventory, is exactly:

V19-2, V19-14, V19-18; V20-6, V20-10, V20-13, V20-14, V20-15, V20-16,
V20-20, V20-21; V22-B, V22-D, V22-I, V22-J, V22-K, V22-L; V23-G, V23-I,
V23-L; V24-D, V24-H, V24-L; V26-G, V26-I, V26-N, V26-O; V27-G, V27-N,
V27-O; V28-B, V28-E, V28-F, V28-G, V28-K, V28-L, V28-M, V28-N, V28-O,
V28-P, V28-Q, V28-R, V28-T; V29-B, V29-N, V29-O, V29-P; V30-C, V30-D,
V30-F, V30-I, V30-J, V30-K; V31-A, V31-B, V31-E, V31-G, V31-H, V31-N;
V32-A, V32-B, V32-C, V32-H, V32-I, V32-J; V33-001, V33-002, V33-003,
V33-004, V33-010, V33-011, V33-012, V33-013, V33-014, V33-015, V33-016,
V33-017, V33-018, V33-019, V33-020, V33-021, V33-022, V33-023, V33-024,
V33-025, V33-026, V33-027, V33-028, V33-029; V34-B, V34-C, V34-D, V34-J,
V34-K; V35-B, V35-E, V35-G, V35-H, V35-I, V35-J; V36-001, V36-002,
V36-003, V36-004, V36-005, V36-024, V36-025, V36-026, V36-027, V36-028,
V36-059, V36-060, V36-061, V36-062, V36-063, V36-064, V36-065, V36-066,
V36-067, V36-068, V36-069, V36-070, V36-073; V37-003, V37-004, V37-005,
V37-006, V37-007, V37-008, V37-009, V37-010, V37-011, V37-012, V37-013,
V37-014, V37-015, V37-016, V37-017, V37-018, V37-023, V37-025, V37-026,
V37-027, V37-028, V37-029, V37-030, V37-031, V37-032, V37-033, V37-034,
V37-035, V37-036, V37-037, V37-038, V37-039, V37-040, V37-041, V37-042,
V37-043, V37-044, V37-045, V37-046, V37-047, V37-048, V37-049, V37-050,
V37-051, V37-052, V37-053, V37-054, V37-055, V37-056, V37-057, V37-058,
V37-059, V37-060, V37-063, V37-064, V37-065, V37-066, V37-067, V37-068,
and V37-069.

Thus SET A minus SET B and SET B minus SET A must both be empty after exact range
expansion, and SET B contains no duplicate identity. The exact named-heading inventory is
`Memory/Spill Plan Tests`, `Memo and Pruning Tests`, `Cost Model Tests`, `Optimizer
Determinism and Resource-Limit Tests`, `Join-Order Tests`, `Final Optimizer Validation
Tests`, and `Optimizer Diagnostics Tests`.

The live Verification text must contain no pairwise approximate-tie comparator; incomplete
structural identity; fingerprint authority; algorithm-specific logical cardinality;
estimated-zero proof; omitted blocking/enforcement/spill work; mandatory optional operator;
unsafe first-K or predicate reordering; pruned required payload; arbitrary memory split;
planning/execution-budget conflation; threshold-proximity fallback; partial-memo survival;
wall-clock resource trigger; unconditional conversion of backing denial to
`OptimizerResourceLimit`; or validator bypass. Any missing essential observation is `NOT
VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, and every injected event must prove its boundary
and trigger before its expected outcome is assessed.
