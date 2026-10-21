# Chapter 37 — Physical Properties, Join Enumeration, and Subquery-Planning Verification

This section composes the existing binding, logical-semantics, physical-operator,
statistics, cost, optimizer, resource, validation, and diagnostic procedures at Chapter
37's integration boundaries. It does not duplicate their component suites. A conforming
harness may use immutable planning records, structured optimizer events, controlled
configuration snapshots, independent set/graph/property models, or equivalent inspectable
state; no private class, C++ API, container, trace framework, or file format is required.

## V37-A — Search event, evidence, and nonvacuity

Maintain an independent ledger keyed by optimizer invocation and bound logical-subproblem
identity. Correlate the bound query, BindingIds, RelationSets, predicate occurrences and
their referenced sets, region boundaries, retained descriptors/StatsVersions, required
slots, raw and normalized ordering, propagated row objective, retained search
configuration, capabilities, initial mode, subsets/partitions, Cartesian decisions,
predicate placement, algorithms/orientations, Chapter-35 cardinality identity,
Chapter-36/38 cost identity, interesting alternatives, dominance/ties, resource-guard
events, fallback, selected plan, and final validation. Subquery records additionally carry
bound occurrence, statement attempt, subplan, initialization, demand, and final-row
consumption identities. A final plan or query result is an observation, never its own
enumeration/property oracle.

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-001 | Optimize one typed query with three BindingIds, two predicates, competing ordered/unordered access paths, two join algorithms, and a final ordering requirement. | One coherent invocation ledger follows binding, slot/property derivation, graph/region construction, enumeration, cardinality/cost handoff, objective comparison, selection, and successful validation. |
| V37-002 | Execute the V37-001 fixture through an independently constructed reference model of sets, predicates, properties, capabilities, and legal plans. | Every observed identity and decision agrees with the independent model; coincident names, numbers, costs, or output rows do not establish identity. |
| V37-003 | Suppress in turn BindingId, required-slot, normalized-ordering, predicate-owner, enumerated-alternative, initial-mode, objective/cost, resource-guard, and final-validation evidence. | Each missing essential category yields `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from a plausible plan or result. |
| V37-004 | Arm every later injected fault, race, omission, boundary, or validator case; record target invocation, selected fixture, reached boundary, and actual trigger before judging outcome. | An untriggered injection, missed publication/configuration barrier, unvisited alternative, or bypassed validator is failed setup, not a passing negative test. |

Every later V37 procedure inherits V37-003 and V37-004 whenever essential evidence is
missing or a fault, race, resource guard, boundary, or rejection path is named.

## V37-B — Ordering identity, normalization, providers, and interesting orders

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-005 | Construct OrderKeys varying one at a time by LogicalSlotId, ASC/DESC, NULL order, and collation; include same display names, self-join aliases, binary VARCHAR keys, hidden computed slots, and the empty vector. | Property identity uses all four fields and query-local slot identity; empty means no ordering, and no unsupported collation or name/statistics equivalence is inferred. |
| V37-006 | Check available `(a,b,c)` against required `(a,b)`, `(a,b)` against `(b)`, `(a)` against `(a,b)`, an empty requirement, and each four-field mismatch. | Satisfaction is exact normalized prefix matching; every negative fixture reaches the satisfaction/validation boundary and cannot pass from accidental runtime row order. |
| V37-007 | Feed `(x,x)`, `(x,x,y)`, same-slot/different-direction, different-NULL-order, different-collation, same-name/different-slot, and distinct computed occurrences through every property consumer. | A left-to-right pass removes only later exact four-field duplicates before identity, memo lookup, satisfaction, interesting-order retention, and enforcement; first occurrence remains and nonidentical keys remain distinct. |
| V37-008 | Use a duplicate erroring ORDER BY expression, hidden ordering slots, and two raw vectors that normalize identically. | Normalization changes metadata only: one canonical property class results while demanded evaluations, hidden slots, bound occurrences, and ordinary errors remain owned and observable. |
| V37-009 | Exercise SeqScan, forward IndexScan, Filter, simple/reference Project, Limit, Sort, eligible Top-N, HashJoin, HashAggregate, and hash DISTINCT. | Each advertises exactly §37.5's runtime-guaranteed ordering; projection drops a key when it does not survive unchanged, and forward index order never fabricates DESC. |
| V37-010 | Exercise supported NLJ/INLJ and capability-enabled MergeJoin, ordered aggregate, or ordered DISTINCT; repeat with each optional capability absent. | Additional ordering appears only with an implemented enabled capability and matching runtime guarantee; absent optional algorithms create no required alternative. |
| V37-011 | Give lowest cost to candidates falsely claiming ordering after Project or hash operators and to an unsupported ordered algorithm. | Eligibility or final validation rejects each actual offered candidate; a cheap false property cannot win. |
| V37-012 | Compare a locally cheaper unordered path with a costlier path satisfying final ORDER BY, index order, or an enabled ordered consumer. | The useful ordered alternative survives local dominance and may win globally by avoiding enforcement; the active Chapter-38 objective remains authoritative. |
| V37-013 | Generate useful exact prefixes plus arbitrary irrelevant orderings and equivalent raw vectors. | Useful normalized classes are retained once, arbitrary unconsumed classes are not generated, and property-state growth remains bounded without conflating direction/NULL/collation variants. |
| V37-014 | Differentially sort controlled NULL, byte-distinct VARCHAR, duplicate, and hidden-expression keys through an advertised provider and explicit Sort. | Provider output and four-field property metadata agree with Chapter-17/30 ordering; property evidence never substitutes for actual comparison/error semantics. |

## V37-C — Required slots, memo identity, BindingId, and RelationSet

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-015 | Derive required slots for SELECT output, predicates, join/residual keys, grouping/aggregates, ORDER BY, DML assignments/RETURNING, target RID/system state, and supported subquery inputs. | The independently computed closure equals the planner's per-subproblem set; every essential slot reaches its consumer and no cost can waive it. |
| V37-016 | Offer cheaper plans that omit an ORDER BY-only column, join key, residual input, hidden computed slot, DML RID, or demanded erroring projection. | Each actual candidate is repaired by a legal enforcer or rejected before selection; hidden identity, demand, and V31 ownership survive pruning. |
| V37-017 | Create states sharing RelationSet but differing in logical predicate identity, required-slot class, normalized ordering class, or ALL_ROWS versus exact FIRST_K_ROWS. | Chapter-38 memo/search identity keeps every preference-changing state distinct; Chapter 37's reduced key is used only when slots/objective are deterministically equivalent. |
| V37-018 | Use exact K, unknown K, approximate/saturated row estimates, and equal normalized ordering vectors. | Exact K participates only where canonically propagated; estimates never manufacture K, while genuinely equivalent normalized property states do not duplicate memo classes. |
| V37-019 | Bind one table twice with aliases and equal display/ColumnId values, then perturb numeric BindingId assignment while preserving canonical occurrence order. | Each occurrence has a distinct BindingId/RelationSet member and slot identity; TableId/name equality never collapses self-joins. |
| V37-020 | Compare union, intersection, difference, subset, cardinality, empty/singleton sets, duplicate insertion, and deterministic iteration with an independent mathematical-set oracle. | Every operation is exact and deterministic over BindingIds; duplicate insertion is idempotent and no occurrence is lost. |
| V37-021 | Build synthetic legal regions with 63, 64, 65, and more-than-native-word BindingIds under ample and constrained planning arenas. | Exact identities never truncate, wrap, merge, or create a 64-relation SQL limit; a wider/growable representation succeeds or genuine bounded planning exhaustion follows the canonical resource owner. |
| V37-022 | Enumerate partitions under varied container insertion/allocation orders and inject a fixed-width overflow defect. | One stable symmetry rule visits each admitted unordered partition once; representation truncation is independently detected as OptimizerError/invariant failure rather than heuristic mode or SQL invalidity. |

## V37-D — Search configuration, threshold, and resource handoff

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-023 | For `exhaustive_join_limit` exercise default 10, zero, positive, missing, negative, and unrepresentable values. | Nonnegative exact integers are retained, zero is valid, and missing/invalid values are rejected as existing optimizer configuration failure before search-state construction without wrapping/clamping. |
| V37-024 | Repeat V37-023 for `large_join_max_local_passes` with default 4 and zero. | Nonnegative exact integers are retained, zero means greedy with no local passes, and invalid values cannot enter search or overflow the pass counter. |
| V37-025 | Pause after retaining both values, change external configuration, resume planner P, then start planner Q. | P uses one old validated identity without mixed values; Q may use the new valid identity and legal differences are not nondeterminism. |
| V37-026 | Before enumeration use `(N,limit)=(9,10),(10,10),(11,10),(1,0)` and equality under another valid limit. | Per region, below/equal selects exhaustive and above selects heuristic from the outset; threshold zero selects heuristic for every nonempty region and exceedance is not an error. |
| V37-027 | Place N just below/equal to the limit with ample resources and poison any proximity-based fallback branch. | Exhaustive DP proceeds; proximity/equality alone emits no guard event and cannot interrupt it. |
| V37-028 | For below/equal regions deterministically trigger the canonical planning-resource guard before uncontrolled failure, then repeat with the guard clear. | Triggered runs switch to bounded fallback with an identified resource event; clear runs remain exhaustive. The test does not require waiting for actual allocation failure. |
| V37-029 | Contrast invalid configuration, internal RelationSet defect, guard-triggered fallback, bounded fallback exhaustion, high estimated plan cost, and runtime allocation/spill failure. | Owners remain distinct: OptimizerError/invariant, legal fallback, `OptimizerResourceLimit`, ordinary cost metadata, and runtime resource error respectively; no new error enum appears. |
| V37-030 | Put two maximal reorderable regions on opposite threshold sides in one statement and vary only one region's resource pressure. | Each region selects and retains its own canonical mode; one fallback does not rewrite the other's mode or SQL semantics. |

## V37-E — Join graph, predicate ownership, and Cartesian admission

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-031 | Construct equality chain, star, disconnected graph, self-join, local predicates, one three-way hyperedge, overlapping hyperedges, and a LEFT boundary. | One vertex exists per BindingId; every bound predicate occurrence retains its exact referenced set and region/boundary identity; genuine hyperedges are not fabricated pairwise edges. |
| V37-032 | Add one safe Chapter-20 derived equality, one unsafe NULL/error-sensitive derivation, and the original demanded predicates. | Only proven metadata affects connectivity; unsafe derivation is absent, and no derived fact discharges, duplicates, or reorders the original expression. |
| V37-033 | For predicate R=`{A,B,C}`, observe nodes S=`{A,B}` and S=`{A,B,C}`, plus partial overlap and a binding outside the region. | Activation requires `R subset-of S`; unavailable predicates contribute no selectivity, physical expression, discharge, or proof. |
| V37-034 | For disjoint L/U, vary whether R is available, intersects both children, lies wholly in one child, or is movement-restricted. | Crossing requires availability, intersection with both children, and Chapter-20 legality; predicates wholly below are not reattached. |
| V37-035 | Trace a three-way predicate through several candidate trees, duplicate graph references to one occurrence, and two distinct same-text occurrences. | Each occurrence has one lowest legal owning node per tree, no premature/ancestor duplicate or omission, while distinct occurrences remain distinct. |
| V37-036 | Use erroring and UNKNOWN-producing predicates, estimated-zero inputs, safe derived equality, and a constrained LEFT predicate. | Demand, 3VL, error, and boundary owners remain authoritative; placement is not proof and runtime evaluation occurs for every demanded candidate occurrence. |
| V37-037 | Exhaustively enumerate chain `A--B--C`, observing subsets `{A,B}`, `{B,C}`, `{A,C}` and complete partitions. | Connected subsets/partitions are admitted, `{A,C}` is excluded as unnecessary Cartesian work, and both lawful connected complete trees remain planable. |
| V37-038 | Use a sole predicate over `{A,B,C}` with no pairwise predicate. | A lawful predicate-free proper-subset prerequisite is admitted, no premature evaluation occurs, and the predicate activates at the complete binary node without requiring a three-input operator. |
| V37-039 | Use P1=`{A,B,C}` and P2=`{C,D,E}` without pairwise predicates. | Exact hypergraph connectivity and prerequisite assembly leave at least one complete binary plan; each predicate activates once at its first lawful node without pairwise decomposition. |
| V37-040 | Use connected `{A,B}` and `{C,D}` components, three singleton components, pair-plus-singleton, explicit CROSS, and a LEFT boundary. | Complete-component Cartesian assembly remains legal and retains all relations/predicates; unnecessary products and boundary-crossing products are excluded. |

## V37-F — Exhaustive and bounded join search

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-041 | In exhaustive mode enumerate singleton initialization, every admitted subset, all nonempty disjoint partitions whose union is target, applicable algorithms, and useful property classes. | The event ledger equals an independent admitted-search-space generator; no required state/partition is omitted and excluded Cartesian states are not demanded. |
| V37-042 | Make `(A join B) join (C join D)` legal and cost-attractive relative to every left-deep tree. | The bushy alternative is actually enumerated, costed, retained, and eligible; a left-deep-only implementation fails even if its final result is correct. |
| V37-043 | Perturb BindingId values and container order while counting symmetric partitions. | The deterministic symmetry rule considers each admitted unordered split once, with neither duplicate costing nor lost orientation/algorithm alternatives. |
| V37-044 | Above threshold, construct greedy choices with distinct incremental objectives, exact ties, connected extensions, and a necessary Cartesian extension. | Greedy uses the active Chapter-38 objective, stable structural tie-break, and §37.12 legality; cost selects among admitted moves but creates none. |
| V37-045 | Exercise pass budgets zero, one, and several; improving and non-improving complete passes; exact budget exhaustion; and an untriggered optional move. | Greedy always runs, zero runs no passes, each pass visits the enabled legal neighborhood once in stable order, and stopping occurs at first non-improvement or exact budget. |
| V37-046 | Offer illegal LEFT-boundary moves, unsupported moves, and lawful local Cartesian prerequisite/component moves. | Only enabled semantics-preserving moves enter; no beam or particular neighborhood is mandatory and heuristic legality matches exhaustive Cartesian policy. |
| V37-047 | Repeat fixed-input exhaustive and heuristic searches under changed hash/container order, allocator addresses, worker timing, and optional beam absence. | Subsets, admitted moves, ties, and selected legal plan remain canonical; missing a cheaper unvisited heuristic tree is permitted and optional beam absence is not failure. |

## V37-G — Algorithms, orientations, cardinality, and cost handoff

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-048 | For admitted pairs enumerate supported HashJoin, NLJ, eligible INLJ, and capability-enabled MergeJoin while varying join trees. | Join order and every mandatory applicable algorithm are optimized jointly; the search cannot freeze a tree before required algorithm choices. |
| V37-049 | Give an unavailable MergeJoin or incompatible INLJ the lowest fabricated cost, then enable each with valid predicates/properties/descriptors. | Capability/eligibility, not cost, excludes unavailable alternatives; enabled legal alternatives become observable without making optional algorithms mandatory. |
| V37-050 | For INNER equijoin vary left/right rows, payload width, spill pressure, and downstream properties. | Both semantically equivalent runtime-supported build/probe orientations are enumerated and independently costed. |
| V37-051 | For LEFT hash join offer the canonical right-build/left-preserved-probe orientation and a cheaper swapped orientation, with and without an approved logical rewrite. | Baseline rejects the swap; only the separately valid rewritten logical join may expose a different supported orientation. |
| V37-052 | Feed the same logical relation/predicate/statistics identity to Hash, NLJ, INLJ, and enabled MergeJoin. | Every algorithm consumes one Chapter-35 logical cardinality while cost, startup/run, memory/spill, and ordering may differ. |
| V37-053 | Set logical join estimate to zero without proof while runtime rows and demanded errors exist. | Every semantically required join remains executable; zero affects cost only and cannot prune all alternatives or create emptiness. |
| V37-054 | Correlate each alternative's required/provided properties, rows/width, finite Chapter-36/38 components, memory/spill, objective, dominance, and validation. | Every handoff retains invocation/logical identity, charges work once, and lets Chapter 38—not Chapter 37—own comparison, ties, enforcement, and final validation. |

## V37-H — Expression-subquery physical planning

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-055 | Plan two identical-text uncorrelated subqueries with distinct bound occurrences, one occurrence referenced repeatedly, and shared underlying statistics. | Each occurrence has its own lazy physical fallback/state identity; one occurrence initializes at most once per statement attempt and shared descriptors do not merge state. |
| V37-056 | Exercise demanded/undemanded branches, repeated outer rows, cancellation, failure, and statement retry. | Initialization happens only on first semantic demand, successful state is reused within the attempt, failure owns the statement outcome, and retry receives fresh state/snapshot. |
| V37-057 | Scalar child produces zero, one, two, and more final rows; inject a demanded second-row error, estimated zero, and `required_rows=1`. | Zero yields typed NULL, one yields its value, the second successfully constructed row raises the canonical cardinality violation, and at-most-two consumption/errors cannot be cost-pruned. |
| V37-058 | EXISTS child is empty, one-row, many-row, estimated-empty-but-nonempty, setup-erroring, and projection-erroring-only after the first row. | Lawful first-final-row demand/early stop is preserved, demanded relational work/errors remain, undemanded projection-only errors are not invented, and estimates prove nothing. |
| V37-059 | IN/NOT IN child is empty, matching, nonmatching, duplicate, NULL-containing, spill-sized, estimated zero, or erroring; vary NULL probe. | Complete required build, markers, 3VL, duplicates, spill work, and errors match V20-13/14; it is never cost-rewritten as EXISTS or ordinary INNER JOIN. |
| V37-060 | Offer semi/anti/marker rewrites with exact proof plus capability, missing proof, missing capability, changed duplicate/NULL/error demand, and the mandatory fallback. | Rewrite enters only with both proof and capability and exact equivalence; otherwise the independently executable lazy fallback remains. |
| V37-061 | Put an outer reference in scalar, EXISTS, IN, and apparently decorrelatable forms. | The actual binder/semantic owner rejects correlation before physical costing; no once-only cost or presumed decorrelation appears. |
| V37-062 | Hold all retained inputs fixed while repeating a literal, then vary only a legal literal or compatible StatsVersion. | Fixed inputs search deterministically; legal literal/statistics differences may alter estimates/costs/plans without SQL change or a prepared-statement requirement. |

## V37-I — Errors, validation, differential correctness, and frozen regressions

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-063 | Inject invalid search config, RelationSet truncation, malformed ordering, missing slot, false ordering, incompatible index, unsupported capability, illegal outer reorder, correlation, no legal plan, planning exhaustion, runtime allocation/spill failure, scalar cardinality violation, and demanded SQL error. | Each triggered boundary reports its existing configuration/invariant/eligibility/resource/runtime/SQL owner; no low cost, estimate, or new error enum changes classification. |
| V37-064 | Offer the final validator cheap candidates with false ordering, missing slots, wrong orientation, unavailable capability, unowned predicate, and invalid objective metadata. | Each candidate is proven offered and rejected; only a legal fully validated plan may be handed to execution. |
| V37-065 | Differentially execute composed nullable/duplicate joins with ORDER BY, hidden slots, optional LIMIT, supported scalar/EXISTS/IN, and applicable DML through multiple legal plans. | Bags, NULL/UNKNOWN, order, visibility, demand/errors, scalar cardinality, subquery state, and DML effects match an independent semantic plan while separate enumeration/property evidence remains required. |
| V37-066 | Run property- and cost-different DML plans through V31-A/B/E/G/H/N. | Target RID/hidden slots, candidate closure, canonical errors, W/C/R, retry, publication, counts, and RETURNING remain unchanged. |
| V37-067 | Vary workers, morsels, scheduling, join algorithms, and legal early stop under V32-A/B/C/H/I/J. | BindingId/search identity never replaces source/claim identity; coverage, no replay, completion, errors, and SQL multiplicity remain worker-independent. |
| V37-068 | Exercise stable inputs, capabilities, objectives, ties, fallback, descriptors, independent StatsVersions, concurrent publication, and diagnostics under V33-A/C/D/E/F/G/H/I/K/N and V34-B/C/D/J/K. | Search consumes one coherent invocation view, preserves per-object retained generations, and admits only legal alternatives under canonical bounded selection. |
| V37-069 | Exercise zero/unknown estimates, widths, proof separation, finite costs, access-path costability, startup/run/total, work ownership, and saturation under V35-A/B/E/G/H/I/J and V36-A/H/I/J. | Logical cardinality/proof and physical cost/work remain separate; estimates/costs never remove required plans, demand, properties, or runtime resource owners. |

## V37-J — Static integrity and complete evidence

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V37-070 | Parse the live V37 section and Architecture; enumerate family/atomic definitions, ledgers, matrices, external IDs, named headings, optionality, and essential evidence categories. | Exactly one V37-A–J heading and one contiguous V37-001–070 definition exist; no duplicate/missing/orphan ID, broken reference, inventory mismatch, unsupported coverage assertion, mandatory optional capability, circular-only claim, or unavailable essential observation may PASS. |

## V37 atomic-family ledger

| Atomic range | Canonical integration obligation | Primary reusable evidence |
|---|---|---|
| V37-001–004 | Correlated search event, positive control, missing evidence, nonvacuity | V33-A/N; V36-A/J; `Optimizer Diagnostics Tests` |
| V37-005–014 | Ordering identity, prefix, normalization, providers, interesting retention | V20-10/15; V22-C/D/K; V27-G/N; V28-O; V30-C/D/K |
| V37-015–022 | Slot closure, memo identity, BindingId, RelationSet capacity/symmetry | V19-2/14; V20-20/21; V22-B/J/K; V33-H |
| V37-023–030 | Configuration domains/stability, threshold, guard, resource ownership | V33-C/E/F/G; V36-I/J; `Optimizer Determinism and Resource-Limit Tests` |
| V37-031–040 | Graph identity, activation/placement, Cartesian classes | V20-6/15/16; V33-K; `Join-Order Tests` |
| V37-041–047 | Exhaustive bushy and deterministic bounded search | V33-C/D/E/N; `Memo and Pruning Tests` |
| V37-048–054 | Joint algorithms/orientations/cardinality/cost handoff | V22-D/I/K; V28-B/G/M/N/O/T; V35-B/G/I; V36-H/I |
| V37-055–062 | Subquery occurrence, demand, 3VL, rewrites, correlation, literals | V19-18; V20-12/13/14/15/16; `Subquery Tests` |
| V37-063–069 | Errors, validation, differential and frozen regressions | V31-A/B/E/G/H/N; V32-A/B/C/H/I/J; V33-A/C/D/E/F/G/H/I/K/N; V34-B/C/D/J/K; V35-A/B/E/G/H/I/J; V36-A/H/I/J |
| V37-070 | IDs, references, coverage, optionality, document role | V22-L; V33-N; V36-J |

## V37 invariant coverage

| §37.18 invariant | Architecture owner | New/reused procedure and controlled oracle |
|---|---|---|
| 1 — ordering plus required slots only | §37.1 | V37-005/V37-015; V22-B/C/H; optional-category negative fixture |
| 2 — exact four-part prefix | §§37.2–37.3 | V37-005–007; V30-C; mismatch matrix |
| 3 — runtime-guaranteed ordering only | §37.5 | V37-009–011; V27-G/V28-O/V30-K; false-provider injection |
| 4 — duplicate normalization and useful interesting orders | §§37.2, 37.6 | V37-007–008/V37-012–014; V33-D/H |
| 5 — BindingId relation identity | §37.7 | V37-019–022; V19-2 |
| 6 — exhaustive bushy small-region search | §37.10 | V37-041–043; V33-C; bushy four-way fixture |
| 7 — validated per-region threshold/default 10 | §§37.11, 38.21 | V37-023/V37-025–030; V33-C/E/F/G |
| 8 — deterministic greedy and pass budget/default 4 | §37.13 | V37-024/V37-044–047; V33-D/N |
| 9 — necessary Cartesian alternatives only | §§37.10, 37.12–37.13 | V37-037–040/V37-046; V20-6/16 |
| 10 — joint INNER order/algorithm/orientation | §§37.14–37.15 | V37-048–050; V28-M/T |
| 11 — LEFT boundary and orientation | §§37.9, 37.15 | V37-036/V37-046/V37-051; V20-6/16/V28-B |
| 12 — algorithm-independent cardinality | §37.16 | V37-052–053; V35-B/G/I |
| 13 — capability-enabled algorithms only | §37.14 | V37-010–011/V37-049; V22-D/K |
| 14 — correlation rejection and lazy fallback | §37.17 | V37-055–061; V19-18/V20-12/13/14 |
| 15 — complete availability and one predicate owner | §§37.8, 37.10 | V37-033–036; V20-15/16; activation/duplication faults |

## V37 subsection and cross-owner coverage

| Normative owner | New V37 procedure | Exact reuse / independent expected result |
|---|---|---|
| §37.1 scope | V37-005/V37-015/V37-017 | V22-B/C/H; only ordering/slots tracked and row objective remains separate |
| §§37.2–37.3 OrderingProperty/satisfaction | V37-005–008/V37-014 | V19-14; V20-10/15; V30-C/D |
| §37.4 RequiredSlotSet | V37-015–018 | V20-20/21; V22-B/K; V27-N |
| §37.5 provided ordering | V37-009–011 | V27-G; V28-O; V29-O; V30-K; `Scan and Unary Operator Tests`; `Access Path Tests` |
| §37.6 interesting orders | V37-012–014 | V33-D/H; `Physical Property and Enforcement Tests` |
| §37.7 RelationSet | V37-019–022 | V19-2; V33-E; exact mathematical-set oracle |
| §37.8 join graph/predicates | V37-031–036 | V20-6/15/16; V33-K |
| §37.9 reorderable/LEFT regions | V37-032/V37-036/V37-046/V37-051 | V20-6/16; V28-B |
| §37.10 bushy DP | V37-037/V37-041–043 | V33-C/D; independent admitted-space generator |
| §37.11 threshold | V37-023/V37-025–030 | V33-C/E/F/G; exact boundary ledger |
| §37.12 Cartesian products | V37-037–040 | V20-6/16; graph-component oracle |
| §37.13 bounded heuristic | V37-024/V37-028–030/V37-044–047 | V33-C/D/E/N |
| §§37.14–37.15 algorithms/orientations | V37-048–051 | V22-D/K; V28-M/T; `Memory/Spill Plan Tests` |
| §37.16 cardinality independence | V37-052–054 | V35-B/G/I; V36-H/I; `Cost Model Tests` |
| §37.17 subqueries | V37-055–062 | V19-18; V20-12–16; `Subquery Tests` |
| §37.18 invariants | V37-005–069 plus invariant matrix | Exact controlled fixtures above |
| §38.1/§38.3–§38.7 identity, dominance, ties, DP | V37-017–018/V37-041–054 | V33-C/D/H/N; `Memo and Pruning Tests` |
| §38.21 threshold/resource handoff | V37-026–030 | V33-E/F; V36-I; guard-clear/trigger contrast |
| §38.24 final validation | V37-011/V37-016/V37-049/V37-063–065 | V22-K; `Final Optimizer Validation Tests`; `Optimizer Differential Correctness Tests` |
| §41.7 join-search/property obligations | V37-005–065 | Required controlled scenarios and differential oracle are represented |
| §42.6 measurement context | V37-012/V37-026/V37-041–047 | Search counters may support benchmarks; no accuracy/global-optimum rule is added |

## V37 adversarial-case matrix

| Case | New procedure | Canonical owner and injected fixture | Independent result / reuse |
|---|---|---|---|
| A same name/different slot | V37-005 | §§37.2–37.3 | Distinct keys; V19-2/V30-C |
| B same table/two bindings | V37-019 | §37.7 | Distinct members; V19-2 |
| C computed ORDER hidden slot | V37-005/V37-016 | §§37.2, 37.4 | Hidden slot retained; V19-14/V27-N |
| D prefix success/failure | V37-006 | §37.3 | Exact prefix outcomes; V30-C |
| E direction/NULL/collation mismatch | V37-005–006 | §§37.2–37.3 | No satisfaction |
| F forward index claims DESC | V37-009/V37-011 | §37.5 | Reject; V27-G/V22-K |
| G Project drops key | V37-009/V37-011 | §37.5 | Ordering removed; V27-N |
| H HashJoin claims order | V37-009/V37-011 | §37.5 | Reject; V28-O/V22-K |
| I cheap unordered/useful ordered | V37-012 | §§37.6, 38.3 | Both retained; V33-D |
| J duplicate normalization | V37-007 | §37.2 | Canonical class |
| K duplicate erroring demand | V37-008 | §§37.2, 20 | Error demand retained; V20-15 |
| L essential join slot pruned | V37-015–016 | §37.4 | Reject/repair; V22-B/K |
| M DML RID pruned | V37-016/V37-066 | §§37.4, 31 | Reject; V31-B/E |
| N objective omitted from memo | V37-017–018 | §38.1 | States distinct; V22-J |
| O 65 BindingIds/native word | V37-021–022 | §37.7 | Exact or resource result |
| P missing/negative config | V37-023–024 | §§37.11, 37.13 | OptimizerError; V33-E |
| Q threshold zero | V37-026 | §37.11 | Heuristic |
| R equality/guard clear | V37-026–027 | §§37.11, 38.21 | Exhaustive |
| S proximity fallback | V37-027 | §38.21 | Poison branch not reached |
| T actual guard fallback | V37-028 | §38.21 | Bounded fallback; V33-E/F |
| U bounded exhaustion | V37-029 | §§38.21, 39.4 | OptimizerResourceLimit |
| V two region modes | V37-030 | §§37.11, 38.21 | Independent modes |
| W self-join collapse | V37-019 | §37.7 | Distinct identities |
| X hyperedge too early | V37-033/V37-038 | §37.8 | Unavailable before full set |
| Y predicate twice/omitted | V37-035 | §37.8 | One lowest owner |
| Z unsafe equality derivation | V37-032/V37-036 | §§20.17.7, 37.8 | Not admitted/discharged |
| AA chain unnecessary Cartesian | V37-037 | §37.12 | `{A,C}` excluded |
| AB prerequisite forbidden | V37-038 | §37.12 | Pair prerequisite admitted |
| AC overlapping hyperedge loss | V37-039 | §§37.8, 37.12 | Both activate once |
| AD disconnected assembly absent | V37-040 | §37.12 | Complete components combine |
| AE bushy alternative omitted | V37-041–042 | §37.10 | Four-way bushy observed |
| AF inconsistent symmetry | V37-043 | §37.10 | Each split once |
| AG unordered-container greedy tie | V37-044/V37-047 | §37.13 | Structural tie |
| AH pass budget exceeded | V37-045 | §37.13 | Exact stopping |
| AI zero passes omit greedy | V37-024/V37-045 | §37.13 | Greedy still built |
| AJ illegal outer move | V37-046 | §§37.9, 37.13 | Excluded; V20-16 |
| AK unsupported MergeJoin | V37-049 | §37.14 | Capability exclusion |
| AL tree frozen before algorithms | V37-048 | §37.14 | Joint alternatives |
| AM legal INNER orientation omitted | V37-050 | §37.15 | Both supported orientations |
| AN illegal LEFT orientation | V37-051 | §37.15 | Rejected absent rewrite |
| AO algorithm changes cardinality | V37-052 | §37.16 | Shared estimate |
| AP estimated zero removes join | V37-053 | §§37.16, 35 | Join retained |
| AQ same-text subquery state shared | V37-055 | §37.17 | Occurrences distinct |
| AR retry state reused | V37-056 | §37.17 | Fresh attempt state |
| AS scalar second row skipped | V37-057 | §37.17 | Cardinality check/error |
| AT EXISTS first demand lost | V37-058 | §37.17 | Lawful first-row stop |
| AU IN build/NULL wrong | V37-059 | §37.17 | Complete build and 3VL |
| AV rewrite lacks proof/capability | V37-060 | §37.17 | Fallback retained |
| AW correlation costed | V37-061 | §§19, 37.17 | Rejected before costing |
| AX cheap missing slot/property | V37-016/V37-064 | §§37.4, 38.24 | Reject; V22-K |
| AY false order bypasses validator | V37-011/V37-064 | §§37.5, 38.24 | Validator reached/rejects |
| AZ fixed-input nondeterminism | V37-025/V37-047/V37-062 | §§33, 37.13 | Canonical fixed result |

## V37 reuse inventory and stale-rule audit

Exact external procedure IDs used by this section are V19-2, V19-14, V19-18;
V20-6, V20-10, V20-12, V20-13, V20-14, V20-15, V20-16, V20-20, V20-21;
V22-B, V22-C, V22-D, V22-H, V22-I, V22-J, V22-K, V22-L; V27-G, V27-N;
V28-B, V28-G, V28-M, V28-N, V28-O, V28-T; V29-O; V30-C, V30-D, V30-K;
V31-A, V31-B, V31-E, V31-G, V31-H, V31-N; V32-A, V32-B, V32-C, V32-H,
V32-I, V32-J; V33-A, V33-C, V33-D, V33-E, V33-F, V33-G, V33-H, V33-I,
V33-K, V33-N; V34-B, V34-C, V34-D, V34-J, V34-K; V35-A, V35-B, V35-E,
V35-G, V35-H, V35-I, V35-J; and V36-A, V36-H, V36-I, V36-J.

Exact reusable headings used here are `Subquery Tests`, `Scan and Unary Operator Tests`,
`Access Path Tests`, `Join-Order Tests`, `Physical Property and Enforcement Tests`,
`Memory/Spill Plan Tests`, `Memo and Pruning Tests`, `Cost Model Tests`, `Optimizer
Determinism and Resource-Limit Tests`, `Final Optimizer Validation Tests`, `Optimizer
Differential Correctness Tests`, and `Optimizer Diagnostics Tests`.

The live Verification procedures must contain no display-name ordering identity; no raw
unnormalized property class; no fabricated order from SeqScan/hash/forward-DESC paths; no
lost required/hidden/DML slot; no estimated K in memo identity; no BindingId truncation or
TableId alias collapse; no negative/wrapped search configuration; no threshold-proximity
fallback; no cost/resource/runtime conflation; no pairwise decomposition of hyperedges; no
early, omitted, or duplicated predicate owner; no unnecessary chain Cartesian state; no
loss of prerequisite/component Cartesian assembly; no left-deep-only exhaustive search; no
nondeterministic symmetry/greedy tie; no pass-budget overflow; no mandatory beam/global
optimum; no capability inferred from cost; no illegal LEFT orientation; no
algorithm-specific logical cardinality; no estimated-zero proof; no merged subquery
occurrences or retry state; no skipped scalar second-row check; no incomplete IN build; no
proofless rewrite/correlation; and no final-validation bypass. Apparent matches are
classified by canonical owner, stage, admitted bounded approximation, optional capability,
or explicit negative-test context before being treated as stale. Any required identity,
enumeration, property, predicate, mode, guard, objective, or validation evidence that is
unavailable is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`.
