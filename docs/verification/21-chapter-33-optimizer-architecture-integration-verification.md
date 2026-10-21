# Chapter 33 — Optimizer Architecture Integration Verification

This family verifies the Chapter-33 integration boundary. Detailed rewrite, statistics,
cost, enumeration, memo, operator, and execution semantics remain with their existing
owners; V33 composes those oracles from one bound logical-plan input through publication of
one validated immutable PhysicalPlan. It does not require a particular optimizer framework,
internal pass structure, or tracing API.

Every required observation may be supplied by deterministic hooks, barriers, or equivalent
inspectable planning state. If an event or identity on which a verdict depends cannot be
observed, the result is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, not `PASS`.

## V33-A — Planning event and ownership oracle

Use one correlated trace for the V33 family. Each record identifies the optimization
invocation, bound typed logical-plan identity, event ordinal, retained catalog-descriptor
generation, retained statistics generation, planning/search resource configuration,
execution-memory budget, cost configuration, active objective, applicable search mode,
alternative identity and
status where relevant, cardinality/width and runtime memory/spill estimates,
required/provided properties and slots, error owner, and final plan identity. Required event
distinctions are normalization/analysis completion, descriptor and
statistics retention, estimation, alternative admission/rejection/retention, dominance or
tie decision, resource guard/fallback, property enforcement, final-validation attempt and
outcome, immutable-plan publication, and pipeline-builder handoff. Equivalent events are
acceptable when the same ordering and ownership facts can be reconstructed.

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-001 | Optimize a bound typed plan with distinguishable logical, catalog, statistics, planning-resource, execution-budget, cost-configuration, property, and slot identities. | One invocation trace correlates every applicable planning input and boundary through one final plan; no stage is inferred only from the final SQL rows. |
| V33-002 | Use a query that admits at least two legal alternatives and requires property enforcement. | Admission, retained-set comparison under the active objective, enforcement, validation, and handoff events are all observable in causal order. |
| V33-003 | Run one valid positive fixture through final validation and pipeline handoff. | A real validation-success event and a real handoff of the same immutable plan identity are observed; this is the nonvacuous positive control. |
| V33-004 | Suppress each required event class or identity in the verification adapter in turn. | Every dependent procedure reports `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`; an empty, truncated, or mismatched trace cannot pass. |

## V33-B — Bound input and complete planning-stage handoff

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-005 | Supply a valid V20 bound typed logical plan and poison parser, name-binding, and independent type-resolution entry points after handoff. | Optimization succeeds without reparsing or rebinding; any poisoned call fails the procedure. Reuse V20-15–V20-20 and V22-A. |
| V33-006 | Poison execution/storage entry points during optimization, then execute only after handoff. | The optimizer performs no SQL execution or side effect; execution begins only from the validated plan under the executor owner. |
| V33-007 | Trace normalization/analysis, coherent inputs, estimation, legal access/join alternatives, property enforcement, bounded cost selection, and validation. | Each applicable stage supplies the identity consumed by its successor; no unavailable or unvalidated intermediate is silently published. |
| V33-008 | Inject, one at a time, a missing required slot, false ordering, unavailable operator, invalid emptiness proof, and missing required side-plan metadata immediately before final validation. | V22-K and `Final Optimizer Validation Tests` reject each plan; execution and pipeline handoff remain unobserved and the canonical validation owner is reported. |
| V33-009 | Permit all required stages and validate a legal winner. | Exactly one immutable PhysicalPlan is published and handed off; no second winner or mutable execution state is attached. |

## V33-C — Exhaustive and bounded search selection

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-010 | Construct a small join region within the live Chapter-37 exhaustive bound and independently enumerate its legal alternatives. | The exhaustive path is observed and the winner follows Chapter 38's active objective, eligibility, dominance, tie, and validation rules over the applicable retained legal alternatives. |
| V33-011 | Construct a region above that bound, and separately apply deterministic planning-resource pressure that selects the canonical bounded path. | The bounded mode, admitted/explored/retained identities, deterministic heuristic decisions, and legal validated winner match `Join-Order Tests` and `Optimizer Determinism and Resource-Limit Tests`. |
| V33-012 | In the V33-011 fixture, independently construct a legal lower-reference-cost plan outside the permitted explored set. | Its unexamined existence does not fail conformance or become a mandatory candidate; the selected plan must still win the canonical comparison among the alternatives that the applicable search path admits and retains. |
| V33-013 | Perturb enumeration order, allocator layout, and hash iteration while holding the search configuration fixed. | Search-mode decisions and the canonical selected plan remain deterministic under Chapter 38; worker or container order is not a tie rule. |
| V33-014 | Make every explored low-cost candidate invalid while leaving one legal candidate available to the applicable path. | Cost does not excuse illegality; the legal candidate is retained and validated, or the existing canonical no-plan/resource owner applies if none survives. |

## V33-D — Dominance, active objective, ties, and eligibility

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-015 | Insert alternatives that differ in the active full-result/startup objective, required slots, and ordering. | `Memo and Pruning Tests` and `Cost Model Tests` show comparison under the active objective without merging distinct memo/property states. |
| V33-016 | Insert one canonically dominated and one superficially cheaper but non-dominated alternative. | Only Chapter-38 dominance removes the former; the latter remains available until active-objective comparison and validation. |
| V33-017 | Create exact canonical cost ties and perturb insertion order and compact fingerprint collisions. | `Optimizer Determinism and Resource-Limit Tests` applies the canonical structural tie rule; no new tolerance, pointer, hash, or worker-order tie rule appears. |
| V33-018 | Give an unavailable, disabled, ineligible, or inexactly representable operator the lowest cost. | V22-D/K and `Physical-Plan Validator Tests` exclude it before publication; a cost formula is not capability evidence. |

## V33-E — Planning bounds, fallback, and error classification

Every fault hook records that the intended guard was reached. A fixture that did not reach
its configured boundary fails setup rather than passing by absence of an error.

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-019 | Give exhaustive search sufficient deterministic work and planning-arena capacity. | No fallback event occurs; the exhaustive path completes and the selected legal plan passes final validation. |
| V33-020 | Reach the canonical exhaustive-search guard after a legal incumbent exists. | The required Chapter-38 bounded/fallback path is still entered; the incumbent alone cannot bypass a mandatory fallback or validation. |
| V33-021 | Configure the canonical bounded heuristic/fallback so that it fits. | It returns a legal plan selected by its owner-defined mechanism and that plan passes §38.24 validation before handoff. |
| V33-022 | Make the bounded fallback unable to fit its planning resource contract. | The invocation reports `OptimizerResourceLimit` exactly under §§38.21/39.4, publishes no plan, performs no execution, and releases planning ownership. |
| V33-023 | Leave no validated executable plan because of a non-resource capability/property defect. | No plan is fabricated and the canonical validation/no-plan owner is observed; the case is not automatically relabeled `OptimizerResourceLimit`. |
| V33-024 | Exhaust a planning bound and also inject an invalid cheap incumbent. | Fallback cannot publish the incumbent or skip final validation; the outcome is the canonical validated fallback or owner-defined failure, never arbitrary success. |

## V33-F — Planning resources versus execution resources

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-025 | Hold the Chapter-24 execution-memory budget fixed and reduce the optimizer planning/search bound. | Only planning search, fallback, or `OptimizerResourceLimit` behavior changes; the execution budget identity is unchanged. |
| V33-026 | Hold the planning bound and its accounting rules fixed and reduce the execution-memory budget. Record the retained budget identities, resulting alternatives/costs, and actual charged planning allocations through the same guard. | The planning budget and accounting rules remain unchanged. Execution-memory costs may change search/retention choices and therefore actual planning usage; each observed allocation still follows the same §38.21 accounting and guard. Neither execution capacity nor a runtime reservation supplies planning capacity, and costs, selected alternatives, and runtime behavior retain their canonical owners. |
| V33-027 | Hold execution budget and runtime estimates fixed while forcing planning-arena/search exhaustion. | V33-E owns the planning result; runtime estimates and execution capacity cannot mask the reached planning bound. |
| V33-028 | Validate a plan whose runtime peak exceeds its estimate, then exercise the exact V24 reservation/allocation/spill procedures. | The estimate is diagnostic/planning metadata, not an allocation guarantee; actual runtime accounting and failure remain V24/Chapter 39 outcomes. |
| V33-029 | Grant runtime budget and inject physical allocation failure, then separately inject spill I/O failure. | Allocation and spill failures retain V24-D/J/M and Chapter-39 runtime ownership and are not reported as optimizer planning-resource failures. |

## V33-G — Catalog, statistics, and semantic-fact coherence

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-030 | Pause one optimization after retaining its caller-visible catalog view and one complete statistics generation; commit DDL and publish a new StatsVersion. | The invocation retains a compatible live view with no mixed payload generation or dangling descriptor; a later invocation may observe the newer generations. Reuse `Descriptor immutability, cache, and catalog MVCC`, `Statistics Publication and Versioning Tests`, and `One stable statistics snapshot per optimization`. |
| V33-031 | Make selected access paths depend on retained schema/index identities, then invalidate or replace the externally current descriptors while planning is paused. | Retained immutable descriptors remain valid for the invocation or the canonical compatibility owner rejects the plan; numeric catalog and statistics versions need not match. |
| V33-032 | Set estimated rows and predicate TRUE fraction to zero while the snapshot contains a visible row and a demanded potentially erroring expression. | Execution is not removed, the expression remains demanded, and no statistical estimate becomes an emptiness or error-suppression proof. Reuse V20-15/V20-16/V20-19 and `Semantic Emptiness Tests`. |
| V33-033 | Supply missing, stale, and invalid statistics generations. | The Chapter-34/35 fallback remains finite and explicit, a legal executable alternative is retained, and semantics and visibility are unchanged. |
| V33-034 | Contrast V33-032 with a canonical exact semantic-emptiness proof carrying required demand/provenance evidence. | Only the exact proof permits the corresponding empty replacement; final validation rejects a statistics-derived imitation. |

## V33-H — Capability, physical properties, and required slots

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-035 | Make an eligible IndexScan available but costlier than SeqScan, then make it cheaper without changing semantics. | `Access Path Tests` and `Cost Model Tests` select among legal available paths by canonical cost; index existence alone is not selection authority and live BufferPool residency is not an instantaneous rule. |
| V33-036 | Disable optional MergeJoin and SortAggregate and make an IndexScan instance ineligible. | Unavailable/ineligible alternatives are absent from the final plan; optional capability absence is not SQL invalidity when a conforming alternative exists. |
| V33-037 | Request Top-N with an exact first-K value outside the selected implementation's representable domain. | V22-J/K rejects that alternative and retains Sort/other canonical enforcement where legal; bound truncation is forbidden. |
| V33-038 | Require final ordering while offering unordered parallel SeqScan output and incidental physical traversal order. | `Physical Property and Enforcement Tests` requires an actual OrderingProperty provider/enforcer; worker or page order proves nothing. |
| V33-039 | Offer one ordered plan missing a required LogicalSlotId and one slot-complete plan requiring enforcement. | RequiredSlotSet and ordering remain independent mandatory properties; cost and resource pressure cannot publish the incomplete plan. |

## V33-I — Immutable PhysicalPlan metadata and state exclusion

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-040 | For representative scan, join, aggregate, sort/Top-N, and side-plan winners, inspect the canonical plan/EXPLAIN route. | As applicable it exposes operator and children, logical role, access path, join order/algorithm, side-plan relation and semantic mode, estimated rows/width, exact-proof metadata, cost components/total, estimated peak memory/spill, required/provided properties, and required output slots. |
| V33-041 | Correlate V33-040 fields with admission, estimation, comparison, validation, EXPLAIN, and handoff events. | Identities and values describe the same validated selected plan; inspectable metadata need not occupy a mandated node layout. |
| V33-042 | Poison or inspect transaction state, snapshot guard, read epoch, cursor, runtime reservation, spill handle/run, cancellation state, and worker-local mutable state during plan construction. | The immutable plan owns none of them; V22-E and V24-D/M keep those objects per execution. |
| V33-043 | Reuse one immutable plan in two executions with distinct snapshots, reservations, cancellation, and workers; retire one execution. | Mutable states and cleanup are independent, and retiring one execution neither leaks into the other nor invalidates immutable plan metadata. |

## V33-J — Executor handoff, replanning boundary, and lazy side plans

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-044 | Instrument final validation, publication, pipeline construction, and execution preparation; poison optimizer execution and ordinary executor-time cost-based replanning entry points. | One validated immutable plan crosses the boundary, per-execution state is created separately, and either forbidden call fails conformance even if final rows would match. Legitimate canonical execution preparation remains permitted. |
| V33-045 | Build scalar, EXISTS, and IN side plans with demanded and undemanded occurrences, including an erroring body, scalar cardinality, and NULL cases. | V20-12–V20-14 and V22-G preserve distinct semantic modes and lazy demand; undemanded errors are not raised and demanded outcomes use canonical cardinality/NULL/error owners. |
| V33-046 | Assign attractive cost metadata to an undemanded erroring side plan and perturb outer-row occurrence order. | Cost cannot authorize eager execution, merge occurrence-local state, or suppress/duplicate demanded execution. |
| V33-047 | Remove or corrupt required side-plan relationship/mode metadata before final validation. | V22-K and `Final Optimizer Validation Tests` reject the plan and no pipeline handoff occurs. |

## V33-K — Logical rewrite, demand, and error safety

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-048 | Offer rewrites that remove, duplicate, reorder, or move a demanded erroring expression across an unsafe demand boundary. | V20-15–V20-18 rejects every rewrite lacking the canonical equivalence proof; cost cannot select it. |
| V33-049 | Offer rewrites that alter NULL/type coercion, bag occurrence multiplicity, LEFT JOIN preservation, aggregate empty input, or LIMIT/OFFSET demand. | V20-19/V20-20 and operator component tests reject semantic change even when successful result values coincide on a weak fixture. |
| V33-050 | Optimize DML with multiple ordinary candidate errors and vary access/join alternatives and physical visitation. | V31-B/G preserves complete ordinary candidate closure and canonical reduction before `W`; optimizer cost or worker order creates no second precedence rule. |

## V33-L — Control lowering and join/property composition

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-051 | Lower DDL, VACUUM, and ANALYZE roots that have no relational alternatives. | Dedicated lowering may avoid meaningless join search while `Control-Operator Tests` and V31-N preserve completion metadata and the DDL/VACUUM/ANALYZE coordinators. |
| V33-052 | Attach a genuine relational child or ANALYZE collection subplan and inject an unavailable operator, missing slot, or false property in that child. | Dedicated control lowering cannot bypass semantic/physical validation; the malformed child is rejected before coordinator publication. |
| V33-053 | Compose legal INNER/CROSS regions, LEFT JOIN boundaries, required Cartesian products, algorithm orientations, interesting orders, slots, memo alternatives, and final enforcement. | `Join-Order Tests`, `Physical Property and Enforcement Tests`, and `Memo and Pruning Tests` agree on one legal bounded costed selection; no exhaustive global optimum is required above the canonical bound. |

## V33-M — Frozen execution and diagnostic regressions

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-054 | Run the V31 DML/control fixtures through V33 planning and attempt post-`W` discovery/retry or multiple mutation/control publishers. | V31-A/B/G/N rejects the attempt and preserves W/C/R, count/result authority, and one DML/DDL/VACUUM/ANALYZE publisher. |
| V33-055 | Run the V32 scan/join/aggregate/sort fixtures through plans selected under different search pressure and worker counts. | V32-A–V32-N preserves complete/disjoint source work, exclusive claims, exactly-once acceptance, safe replay/early-stop/pass rules, and worker-count semantic independence; legal FLOAT64 tree variation remains V29-owned. |
| V33-056 | Compare Chapter-40 diagnostics with the selected plan and trace, including bounded search with an unexamined cheaper reference plan. | V20-22 and `Optimizer Diagnostics Tests` report canonical selected-plan structure, estimates, costs, properties, and statistics metadata without presenting estimates as actuals or unexamined alternatives as explored. |

## V33-N — Optionality, end-to-end equivalence, and integrity

| ID | Setup and controlled action | Observable oracle and expected outcome |
|---|---|---|
| V33-057 | Run equivalent planning through two conforming internal organizations or perturb pass grouping while retaining the same canonical inputs. | Different legal plan shapes are allowed; every output preserves semantics, capability, properties, active-objective/search-path rules, validation, and immutable handoff. No Cascades/Volcano class layout is required. |
| V33-058 | Toggle supported optional optimizer techniques and keep unsupported optional algorithms disabled. | Optionality cannot change SQL truth, demand, visibility, required slots/properties, or error ownership; absence alone is not failure and no new tracked property is introduced. |
| V33-059 | Differentially execute selected plans against a trusted logical/reference result across bags, NULLs, errors, joins, aggregates, sorting, subqueries, and DML pre-`W` behavior. | `Optimizer Differential Correctness Tests` and `Optimizer Fuzzing` find no semantic or demanded-error divergence; differing legal plan shape/performance is accepted. |
| V33-060 | Validate all V33 traces, plan identities, references, and coverage rows against their positive controls. | Missing evidence, duplicate IDs, orphaned events, unknown reused procedures, or a plan handed off without validation yields non-PASS; no procedure claims executed test results. |

## V33 atomic architecture-obligation ledger

| Atomic range | Contract under test | Required hook or exact reusable oracle |
|---|---|---|
| V33-001–V33-004 | Correlated, nonvacuous planning trace | V33-A trace and positive/suppression controls |
| V33-005–V33-009 | Bound input, no execution, complete stages, validated one-plan publication | V20-15–V20-20; V22-A/K; `Final Optimizer Validation Tests` |
| V33-010–V33-014 | Exhaustive/bounded selection and unexamined-plan boundary | `Join-Order Tests`; `Optimizer Determinism and Resource-Limit Tests` |
| V33-015–V33-018 | Dominance, objective, ties, capability | `Memo and Pruning Tests`; `Cost Model Tests`; V22-D/K/L |
| V33-019–V33-024 | Search guards, fallback, final validation, resource errors | `Optimizer Determinism and Resource-Limit Tests`; `Final Optimizer Validation Tests` |
| V33-025–V33-029 | Planning, execution budget, estimates, actual allocation/spill | V24-D/J/M; `Memory/Spill Plan Tests` |
| V33-030–V33-034 | Catalog/statistics coherence and exact emptiness | `Descriptor immutability, cache, and catalog MVCC`; `Statistics Publication and Versioning Tests`; `Semantic Emptiness Tests` |
| V33-035–V33-039 | Capability, access paths, OrderingProperty, RequiredSlotSet | V22-D/J/K; `Access Path Tests`; `Physical Property and Enforcement Tests` |
| V33-040–V33-043 | Plan metadata, immutability, execution-state exclusion | V22-A/B/E/I; `Physical-Plan Validator Tests`; `Optimizer Diagnostics Tests` |
| V33-044–V33-047 | Handoff, no replanning, lazy side plans | V20-12–V20-14; V22-G/K; `Pipeline Finalization and Resource Tests` |
| V33-048–V33-050 | Rewrite/demand/error safety | V20-15–V20-20; V31-B/G |
| V33-051–V33-053 | Control lowering and join/property composition | `Control-Operator Tests`; V22-H; V31-N; `Join-Order Tests`; `Physical Property and Enforcement Tests`; `Memo and Pruning Tests` |
| V33-054–V33-056 | Frozen V31/V32 and diagnostics | V31-A/B/G/N; V32-A–V32-N; `Optimizer Diagnostics Tests` |
| V33-057–V33-060 | Implementation freedom, optionality, differential integrity | `Optimizer Differential Correctness Tests`; `Optimizer Fuzzing` |

## Chapter 33 subsection and invariant coverage map

| Architecture obligation | V33 integration coverage | Exact reusable coverage |
|---|---|---|
| §33.1 Role | V33-005–V33-014, V33-044, V33-051–V33-052 | V20-15–V20-20; V22-A/D/H/K; `Control-Operator Tests`; `Optimizer Determinism and Resource-Limit Tests` |
| §33.2 Layering | V33-001–V33-009, V33-044, V33-053 | V22-A/K; `Join-Order Tests`; `Physical Property and Enforcement Tests`; `Final Optimizer Validation Tests` |
| §33.3 Planning inputs | V33-001–V33-002, V33-015, V33-025–V33-029, V33-038–V33-039 | V22-B/C; V24-D/J/M; `Memory/Spill Plan Tests`; `Cost Model Tests`; `Optimizer Determinism and Resource-Limit Tests` |
| §33.4 Stable catalog/statistics view | V33-030–V33-034 | `Descriptor immutability, cache, and catalog MVCC`; `Statistics Publication and Versioning Tests`; `One stable statistics snapshot per optimization`; `Semantic Emptiness Tests` |
| §33.5 PhysicalPlan output | V33-008–V33-009, V33-040–V33-047, V33-056 | V20-22; V22-A/B/E/G/I/K; `Physical-Plan Validator Tests`; `Optimizer Diagnostics Tests` |
| §33.6 Cost-based choice | V33-010–V33-024, V33-035–V33-039 | `Access Path Tests`; `Memo and Pruning Tests`; `Cost Model Tests`; `Optimizer Determinism and Resource-Limit Tests` |
| §33.7 invariant 1 — resolved input; no SQL reparsing | V33-005–V33-006 | V20-15–V20-20; V22-A |
| §33.7 invariant 2 — stable immutable descriptors | V33-030–V33-031 | `Descriptor immutability, cache, and catalog MVCC`; `Statistics Publication and Versioning Tests`; `One stable statistics snapshot per optimization` |
| §33.7 invariant 3 — statistics never determine correctness | V33-032–V33-034, V33-048 | V20-15/V20-16/V20-19; `Semantic Emptiness Tests` |
| §33.7 invariant 4 — no mutable page/live-residency planning input | V33-006, V33-035 | V22-I; `Access Path Tests`; `Cost Model Tests` |
| §33.7 invariant 5 — one finalized immutable PhysicalPlan | V33-003, V33-008–V33-009, V33-040–V33-044 | V22-A/E/K; `Final Optimizer Validation Tests` |
| §33.7 invariant 6 — active-objective bounded legal selection | V33-010–V33-024, V33-035–V33-039 | `Join-Order Tests`; `Memo and Pruning Tests`; `Cost Model Tests`; `Optimizer Determinism and Resource-Limit Tests`; `Final Optimizer Validation Tests` |
| §33.7 invariant 7 — normalization and physical selection remain distinct | V33-001–V33-002, V33-007, V33-048–V33-049 | V20-15–V20-20; V22-A/K |
| §33.7 invariant 8 — no mandatory full framework or adaptive replanning | V33-044, V33-057–V33-058 | V22-L; `Optimizer Determinism and Resource-Limit Tests` |
| §33.7 invariant 9 — only exact proof eliminates execution | V33-008, V33-032–V33-034 | V20-15/V20-16/V20-19; V22-I/K; `Semantic Emptiness Tests` |

## Chapter 41 Chapter-33 obligation coverage map

| Owner and obligation | Procedure and controlled input/fault | Expected observable outcome |
|---|---|---|
| §§20, 41 semantic plan equivalence/demand | V33-048–V33-050, V33-059; unsafe rewrite and differential fixtures | Same bags, NULL/type behavior, demand, and canonical errors; no cost-based semantic waiver. |
| §§20, 35, 41 exact emptiness versus estimates | V33-032–V33-034; zero/stale/missing statistics and exact proof contrast | Estimates never prove emptiness; only owner-valid proof removes execution. |
| §§16, 34, 41 coherent retained planning view | V33-030–V33-031; DDL/ANALYZE barriers | One compatible retained descriptor/statistics view, no mixed generation or dangling identity. |
| §33.3 complete planning inputs | V33-001–V33-002, V33-015, V33-025–V33-026, V33-038–V33-039; correlate every input identity | Bound logical plan, coherent descriptors/statistics, planner resources, execution budget, cost configuration, and final required slots/properties reach their owning stages unchanged. |
| §§33.3, 38.21 planning resources | V33-025, V33-027; independently reduce the planner bound | Dedicated planning accounting and the canonical bounded fallback/resource outcome remain distinct from runtime memory. |
| §§24, 33.3 execution-memory budget and allocation | V33-026, V33-028–V33-029; vary budget and inject allocation/spill faults | Runtime estimates, budget grant, physical allocation, and spill outcomes remain distinct and owner-correct. |
| §§37–38 bounded search/objective/ties | V33-010–V33-024; threshold, unexamined plan, tie, guard, fallback | Correct applicable search path and retained-set comparison without global-optimum promise. |
| §§22, 27–30, 38 capability and eligibility | V33-014, V33-018, V33-035–V33-037 | Unavailable/ineligible/inexact candidates cannot reach final plan. |
| §§22, 37–38 properties and slots | V33-038–V33-039; false order and missing slot | Actual OrderingProperty and complete RequiredSlotSet are mandatory before handoff. |
| §§20, 22, 26, 37 lazy subqueries | V33-045–V33-047; demanded/undemanded erroring side plans | Exact side-plan mode and lazy demand; malformed metadata rejected. |
| §§22, 33, 38, 40 immutable metadata/validation | V33-008–V33-009, V33-040–V33-044, V33-056 | Complete consistent inspectable metadata, no execution state, validation before handoff. |
| §38.24 final validation | V33-008, V33-014, V33-018, V33-021–V33-024, V33-047; inject cheapest malformed winners before handoff | Every winner and fallback is validated; invalid plans produce no handoff or side effect. |
| §§24, 38–39 error classification | V33-019–V33-029; planning bound, allocation, spill faults | OptimizerResourceLimit, validation, allocation, and spill owners remain distinct. |
| §§31–32, 41 DML/control/parallel regression | V33-050–V33-055; unauthorized publication/replay/scheduling cases | W/C/R, coordinators, morsel exactness, and worker-count independence remain intact. |
| §40 diagnostics | V33-041, V33-056; compare trace, plan, and EXPLAIN | Diagnostics describe the validated plan and estimates without fabricating exploration or actual values. |

## V33 negative-case consistency checks

The live Verification procedures must contain no global-cheapest-plan requirement above the
canonical search bounds; no exhaustive-enumeration requirement after a canonical guard; no
arbitrary plan choice where retained alternatives require comparison; no incumbent bypass
of mandatory fallback or final validation; no blanket `OptimizerResourceLimit` for every
no-plan case; no conflation of planning and execution budgets; no estimate treated as an
allocation or semantic-empty proof; no unavailable operator or missing property/slot in a
published plan; no instantaneous BufferPool residency as semantic selection authority; no
mandatory optimizer framework or ordinary runtime cost-based replanning; no eager execution
of undemanded side plans; no optimizer-time SQL execution; no dedicated control lowering
that bypasses a real child; and no weakening of V31 publication or V32 morsel ownership.
Apparent matches must be classified in their stage, owner, optional-capability, or explicit
negative-test context before being treated as stale.
