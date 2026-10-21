# Chapter 36 — Cost Model and Base-Access Verification

This family composes the existing expression-demand, physical-operator, statistics,
estimation, resource, optimizer-search, validation, and diagnostic procedures at Chapter
36's integration boundaries. It does not replace their component suites. A conforming
harness may use immutable cost records, structured optimizer events, controlled descriptor
and configuration snapshots, arbitrary-precision reference arithmetic, or equivalent
inspectable state; no cost-model class, method, object layout, or tracing framework is
required.

## V36-A — Cost event, identity, and evidence oracle

For every fixture maintain an independent ledger keyed by optimization invocation and bound
logical-node identity. Correlate required `LogicalSlotId`s/properties, retained table/index
descriptors, TableId/IndexId/SchemaVer, each selected StatsVersion, Chapter-35 rows and
representation-specific widths, validated CostConfig identity, capability/eligibility,
selected physical inputs and provenance, natural-unit workloads, coefficients, normalized
terms, local-work owner, child inclusion, startup/run/total, saturation, active Chapter-38
objective/comparison, separate exact-proof metadata, selected plan, and final validation.
Production cost, plan shape, and runtime results are observations, never their own oracle.

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-001 | Optimize one typed query that enumerates a legal SeqScan and IndexScan, costs a retained Filter/Project, compares alternatives, and validates the winner. | One invocation ledger identifies every retained descriptor/statistic/configuration/estimate/workload/coefficient/cost/objective/validation event and at least one selected legal path. This is the positive instrumentation control. |
| V36-002 | Suppress in turn configuration identity, selected generation, physical-fallback identity, rows, width representation, coefficient, workload, child/local owner, startup/run split, saturation, objective, comparison, and validation evidence. | Each missing essential observation yields `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from a plausible total, plan, or result. |
| V36-003 | Arm a configuration, arithmetic, source-selection, ownership, comparator, race, or validation boundary; pause before it and release a second run through it. | The harness proves the target and invocation, that the boundary was entered, and that the condition/fault fired before assessing the canonical outcome. An untriggered injector or missed race is failed setup. |
| V36-004 | Give different descriptors, generations, configurations, slots, and physical alternatives equal names and equal numerical cost. | The ledger keeps every identity distinct; equality of names, numbers, or output cannot establish shared ownership or provenance. |
| V36-005 | Run matched descriptor fact, selected statistic, stale statistic, physical fallback, Chapter-35 fallback, estimated zero, structural zero, valid saturation, invalid raw cost, and runtime-actual cases. | Every state is independently distinguishable, and no estimate/cost/provenance field is accepted as structural fact, actual work, resource grant, or semantic proof. |

Every later V36 procedure inherits V36-003 whenever it names an injected failure,
arithmetic boundary, model-selection path, publication race, or validation failure.

## V36-B — CostConfig, domains, dimensions, and stability

Use deliberately distinct finite configuration values so a swapped coefficient or fallback
cannot accidentally produce the same result. Each field case records field presence,
retained configuration identity, exact value, unit, domain, selected consumer, workload or
fallback trigger, normalized term where applicable, and rejection boundary.

| ID | CostConfig field and controlled case | Independent observation and required result |
|---|---|---|
| V36-006 | `seq_page_cost`: vary only sequential heap pages in a SeqScan. | The sequential-page workload is multiplied exactly once by the finite positive configured value. |
| V36-007 | `random_page_cost`: vary B+ descent/random heap-page work independently. | Only random persistent-page work uses this finite positive coefficient. |
| V36-008 | `cpu_tuple_cost`: vary physical tuple-version/visibility work with output fixed. | Examined tuple/version work uses this finite positive coefficient, not output rows. |
| V36-009 | `cpu_operator_cost`: vary pushed/retained expression and materialization operations. | Generic expression/operator work uses this finite positive coefficient exactly once. |
| V36-010 | `hash_cost`: enable a legal Chapter-38 hash alternative with controlled build/probe/update counts. | Hash operations use this finite positive coefficient only under the specialized owner. |
| V36-011 | `comparison_cost`: vary B+ search and Sort comparison counts separately. | Comparison operations use this finite positive coefficient without being charged again by a parent. |
| V36-012 | `temp_page_cost`: force a legal spill estimate with controlled temporary reads/writes. | Derived temporary page I/O uses this finite positive coefficient; spill bytes are not added directly to scalar cost. |
| V36-013 | `effective_cache_pages`: compare zero and positive cache assumptions while poisoning momentary BufferPool-residency access. | Unsigned zero is valid and means no assumed cache; the value is not a coefficient or divisor, and live residency is not consulted. |
| V36-014 | `unknown_physical_heap_pages`: omit every usable physical heap-page input. | The positive unsigned configured page count supplies only the missing heap-work estimate. |
| V36-015 | `unknown_dead_version_count`: omit dead-pressure statistics and test zero plus a positive interior value. | Finite nonnegative configured pressure is used; zero is valid. |
| V36-016 | `unknown_index_minimum_physical_entries`: omit index-pressure statistics with zero and small logical live estimates. | The positive unsigned floor participates in §36.7's checked approximate entry derivation. |
| V36-017 | `unknown_index_entry_pressure_multiplier`: hold the floor fixed and vary logical live entries. | The finite value `>=1` multiplies logical live entries with checked arithmetic and remains an estimate. |
| V36-018 | `unknown_index_entries_per_leaf`: omit occupancy, then provide collected zero occupancy. | The finite positive value is used only as the fallback divisor; valid collected zero is not rejected. |
| V36-019 | `unknown_heap_rows_per_page`: make physical density unavailable. | The finite positive rows/page value feeds only the bounded distinct-heap-page approximation. |
| V36-020 | `unknown_index_heap_correlation`: omit correlation and exercise `-1`, `0`, and `+1`. | Each endpoint/interior value is accepted in `[-1,1]`, affects locality cost only, and is identified as configured fallback. |
| V36-021 | For every mandatory field remove it; inject negative, NaN, infinity, forbidden zero, invalid unsigned carriers, correlation outside `[-1,1]`, and positive-divisor zero. | Missing/invalid configuration is rejected before alternative comparison under the existing OptimizerError/internal-configuration owner; valid zero cache/dead/correlation cases pass, and no undocumented default or `CostModelError` appears. |
| V36-022 | Pause one invocation after validation, replace the external configuration, resume it, then start a later invocation. | The first invocation uses one complete retained identity without mixed coefficients; the later invocation may use the new valid configuration and may produce a different legal cost. |
| V36-023 | Independently compute all seven natural-unit workload products and their checked sum; tag bytes, widths, selectivity, memory, and an already-normalized child cost with tempting numeric values. | Each workload uses its matching coefficient exactly once; non-cost units are not added, and normalized child/local costs are not renormalized. |

## V36-C — Finite cost arithmetic and comparison

Use symbolic or arbitrary-precision reference arithmetic independent of the production
representation. Record operands, operation order, component saturation, scalar finalization,
objective, tie decision, and structural tie key.

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-024 | Compute values below, exactly at, and above `MAX_FINITE_COST`, including zero workload. | Below/at values remain exact in the cost domain; valid excess saturates to the one canonical bound; zero contributes zero. |
| V36-025 | Exercise huge uint64 counters, workload-times-weight, multiplication followed by addition, repeated additions, and an already-saturated Chapter-35 estimate. | No integer wrap or nonfinite intermediate occurs; component counters and scalar terms saturate through their declared checked paths. |
| V36-026 | Contrast valid overflow with negative, NaN, and infinite raw workload/cost injection. | Valid overflow is diagnostic saturation; intrinsically invalid input is rejected before comparison and cannot be legalized by finalization. |
| V36-027 | Exercise zero setup, nonzero setup, ordinary run work, startup-only saturation, run-only saturation, both saturated, and sum-only saturation. | `startup`, `run`, and `total` are finite/nonnegative; `total=saturating_add(startup,run)` and `startup<=total`. |
| V36-028 | Compare legal alternatives with distinct costs, exact ties, relative-tolerance ties, outside-tolerance costs, one saturated cost, and two saturated costs; also give an illegal plan the smallest cost. | Chapter-38 objective/dominance/tie rules receive finite values, ties use the canonical structural key, saturation is not resource failure, and cost never legalizes an invalid alternative. |
| V36-029 | Repeat fixed inputs/configuration while perturbing insertion order, allocator layout, hash iteration, and worker timing; separately change one legal calibration. | Fixed inputs produce the canonical deterministic decision; legal calibration changes may alter costs/plans but never SQL semantics or require cross-configuration equality. |

## V36-D — Physical input selection and heap fallbacks

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-030 | For each §36.5.1 quantity, offer every source that its live input-map row actually permits—applicable retained structural fact, complete compatible or valid-old generation, stable derivation, and configured fallback—with coincident numeric values. | The quantity-specific precedence, retained identity/StatsVersion, derivation, confidence/provenance, and consumer—not numerical coincidence—select the input; no unsupported source or rejected-generation member is used. |
| V36-031 | Plan an unanalyzed table, a table lacking physical pages, and a physically nonempty table whose Chapter-35 row estimate is zero. | Logical rows use Chapter-35 fallback where needed; missing pages use the positive physical fallback; SeqScan remains finite/executable and no physical emptiness proof appears. |
| V36-032 | Remove dead pressure; use zero, positive, and huge finite fallback values; then supply valid collected pressure. | Physical tuple versions are the checked live-plus-dead estimate, collected input precedes fallback, and SeqScan CPU remains finite. |
| V36-033 | Supply valid-old and valid-stale complete generations, rejected newest with valid old, no compatible generation, and malformed outer catalog framing. | Chapter-34 selection precedes costing; old/stale retain actual provenance, advisory rejection may fall back, and outer corruption keeps its stronger owner. |
| V36-034 | Planner P retains T1/S1 and T2/S2, pauses during costing, ANALYZE publishes T1/S3, then P resumes and Q starts; repeat with two T1 aliases. | P uses coherent S1/S2, aliases share one underlying retained descriptor, Q may use S3, and no cross-table equality, member mixing, or mid-invocation refresh occurs. |
| V36-035 | Poison exact BufferPool-residency access and compare descriptor-, statistics-, stale-, and configured-fallback physical inputs. | Costing uses only stable inputs, preserves distinct provenance and CostConfig identity, and diagnostics identify the actual selected source. |

## V36-E — Index pressure, locality, and candidate estimates

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-036 | Cost a valid index with retained Chapter-8 height, then remove/corrupt height or mismatch IndexId/SchemaVer/fingerprint while assigning an attractive cost. | Valid structural height is descriptor-owned; missing/corrupt/incompatible structure makes the path ineligible and cannot be repaired by statistics or cost. |
| V36-037 | With no IndexStatistics, exercise zero/positive logical live entries, floor-dominant and multiplier-dominant cases, and huge multiplication; then repeat with compatible fresh and stale collected physical counts. | The exact §36.7 fallback uses checked `max(floor,logical*multiplier)` only when no selected count exists; every collected or fallback value remains approximate work metadata with its actual provenance. |
| V36-038 | Independently compute `max(0,physical-logical)` and the fallback leaf-page ceiling for physical counts below/equal/above logical counts and very large counts. | Invisible entries remain nonnegative, occupancy divisor is positive, leaf pages are finite, and no subtraction/division fault escapes. |
| V36-039 | Supply collected `average_entries_per_leaf=0`, missing occupancy, positive collected occupancy, and a structurally empty valid index with and without compatible zero-valued IndexStatistics. | Collected zero remains a valid generation but invokes the configured divisor; selected collected zero is not replaced as a semantic fact, while a missing-statistics positive estimate may conservatively overestimate and remains only an approximation. |
| V36-040 | Exercise missing correlation, `-1/0/+1`, invalid correlation, known/missing heap density, scattered candidates, and highly localized candidates. | Valid correlation/locality/density inputs produce bounded page work in correct units; invalid configuration is rejected and no value proves visibility or truth. |
| V36-041 | Use collected and fallback physical-entry counts as candidate caps, including zero logical candidates, large inflation, stale zero candidates, and runtime matches. | The cap affects estimated cost only; runtime cursor traversal, heap visits, MVCC checks, executable path, results, and proof metadata are not bounded by it. |
| V36-042 | Cost point lookup with controlled height, cached upper levels, leaf comparisons, duplicate/stale physical entries, RID fetches, MVCC rejects, and output decode. | Every point component has one owner and finite term; UNIQUE never removes heap visibility or duplicate physical pressure. |
| V36-043 | Cost short/long ranges with fresh, stale, and missing index statistics, varied correlation, occupancy, candidate pressure, residual work, and required ordering. | Logical selectivity stays separate from physical work, fallback paths remain finite, distinct pages are bounded, and only actual advertised ordering is recorded. |

## V36-F — Access paths, predicates, and required columns

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-044 | For index `(a,b,c)`, exercise leading equalities, one range, IS NULL, ordinary `= NULL`, unconstrained suffixes, later-key constraints, non-sargable expressions, endpoint inclusivity, duplicate RID bounds, and mismatched schema. | Classification and transient bounds match §§36.11–36.13; exact represented predicates alone are discharged and every partial predicate remains residual. |
| V36-045 | Give distinct counts to physical index entries, visible heap candidates reaching a residual, and final survivors; include legal short-circuit and demanded error cases. | Pre-visibility predicates use physical-entry counts, residuals use candidates reaching them, survivor count is not substituted, and §38.17/Chapter-20 demand remains authoritative. |
| V36-046 | SeqScan fresh/missing/stale/zero-estimated/high-dead/large-page fixtures with selective pushed predicates and narrow projection. | The ledger contains physical page, tuple-version, visible-predicate, and survivor decode work with correct rows/widths/provenance and finite startup/run/total. |
| V36-047 | Enumerate a table with no index, several usable single indexes, an incompatible index, and missing TABLE/INDEX statistics; observe alternatives before comparison. | Every legal LogicalGet has SeqScan plus every usable single-index alternative, each finitely costed; incompatible paths are excluded and missing advisory metadata removes none. |
| V36-048 | Offer two indexes whose individual and combined predicates differ, plus bitmap/intersection/union temptations. | At most one index is used per relation occurrence; unsupported combinations remain absent and residual predicates remain executable. |
| V36-049 | Hold page I/O fixed while pruning wide unused VARCHAR columns and varying predicate-only versus output-only columns. | Required-slot width, decode, copying, materialization, memory traffic, and output width change at the actual owner without changing page I/O or omitting demanded columns/errors. |

## V36-G — Streaming ownership and composition

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-050 | Compare pushed versus separate Filter and fused versus separate Project over the same source with distinctive child/local terms. | Every work unit has exactly one owner; retained unary work is charged, removed/fused work is not duplicated, and semantic demand is unchanged. |
| V36-051 | `PhysicalValues` with zero, one, duplicate, and demanded-erroring declared occurrences. | No child cost exists; each demanded occurrence is charged once, initialization/first bounded occurrence is startup, remainder run, and errors are not cost-pruned. |
| V36-052 | `PhysicalSeqScan` with distinct setup, first page/tuple, remaining pages, visibility, pushed predicate, and decode quantities. | Source startup/run allocation and each local domain match §36.2.2/§36.6; estimated zero without proof retains setup and execution. |
| V36-053 | `PhysicalIndexScan` with distinct descent, first candidate, remaining leaf/candidate, heap/MVCC, residual, and decode quantities. | The source owns each term once, startup includes bounded descent/first attempt, run contains remaining work, and no index-only shortcut appears. |
| V36-054 | `PhysicalFilter` over children with distinctive startup/run, early/late/no match, and expensive/erroring predicates. | Child decomposition is included once; predicates are charged per presented input occurrence, not survivors; first attempt is not a guarantee of output. |
| V36-055 | `PhysicalProject` with direct and expensive/erroring computed outputs over a distinctive child. | Child cost is included once; retained expressions/materialization use one-to-one processed occurrences and preserve demanded errors. |
| V36-056 | `PhysicalLimit` with known, unknown execution-start, invalid, and erroring count expressions over streaming and blocking children. | Count acquisition/validation is charged exactly once in startup; child consumption follows the exact safe objective and no cost assumption changes runtime count semantics. |
| V36-057 | Supported fused scan/filter/project with separately measurable source, predicate, and projection work. | The fused node owns the union once using unchanged evaluation domains; eliminated unary nodes contribute neither duplicate cost nor missing semantics. |
| V36-058 | Inject parent omission, whole-child-plus-startup duplication, local duplication, and pushed-expression duplication; compare a blocking Sort child and a specialized rescanning inner. | Ordinary unary child/local work appears exactly once; negative plans fail the ledger; blocking and repeated work use their specialized Chapter-38 owners. |

## V36-H — Startup objectives, specialized costs, and plan legality

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-059 | Exercise Values first occurrence, SeqScan first page, IndexScan descent/first candidate, Filter early/late/no match, Project expression, and estimated-zero child. | Fixed setup and bounded first-attempt work enter startup, remainder enters run, `startup<=total`, and startup never asserts that output exists. |
| V36-060 | Cost the same legal alternatives under ALL_ROWS, first-row/partial where defined, and exact representable FIRST_K_ROWS. | Chapter-36 components are stable inputs to Chapter-38's active objective; blocking boundaries and partial fractions are observed without changing semantics. |
| V36-061 | Compare known representable K, unrepresentable K, unknown LIMIT, unknown OFFSET, and approximate/saturated count estimates under V22-J/K and V30-J. | Only an exact representable K affects Top-N eligibility; otherwise a legal ordering provider plus Limit remains, and estimates never become exact K or early-stop authority. |
| V36-062 | Exercise supported nested-loop/materialization, INLJ repeated lookups, hash build/probe, capability-enabled merge, Sort, aggregate, Top-N, spill, and property enforcement. | Each applicable Chapter-38 specialized formula overrides the generic transfer without omitted or duplicated child/local/repeated/blocking work; disabled optional capabilities are excluded. |
| V36-063 | Give lowest cost to alternatives missing a required slot/order, using unavailable capability, incompatible index, illegal join orientation, or ineligible Top-N. | Eligibility/property/slot owners exclude or final validation rejects each candidate; a legal conforming fallback survives. |
| V36-064 | Feed identified Chapter-36 costs into dominance, tie, bounded search, fallback, and final validation while varying objective and interesting order. | Comparison consumes the matching invocation's finite costs; Chapter 36 neither selects plans nor overrides Chapter-33/38 search, fallback, or tie policy. |

## V36-I — Proof, failures, diagnostics, and end-to-end handoff

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-065 | Contrast estimated base/selectivity/candidate zero, stale-domain exclusion, saturated cost, structural zero, and approved semantic-empty proof with runtime rows/errors. | Cost and estimates never originate proof or bound runtime access; only an approved proof source authorizes its permitted rewrite and demanded errors remain observable. |
| V36-066 | Inject invalid CostConfig/raw cost, valid saturation, missing/rejected statistics, malformed catalog framing, invalid index structure, and unsupported capability. | Existing configuration/invariant/fallback/corruption/eligibility owners remain distinct; no `CostModelError` or low-cost rescue is invented. |
| V36-067 | Exhaust planning resources with and without a legal bounded fallback; separately inject runtime allocation/spill failure after a low memory estimate. | Legal fallback wins when it fits; otherwise `OptimizerResourceLimit`; runtime resource owners remain separate and an estimate is not a grant/allocation guarantee. |
| V36-068 | Inspect structured diagnostics for retained descriptors/StatsVersions, physical inputs/fallbacks, rows/width representations, workloads, coefficients, components, startup/run/total, saturation, objective, proof, selected plan, and actual metrics. | Trace, immutable records, and selected plan identify the same invocation while estimates remain distinct from elapsed time, grants, and runtime actuals. |
| V36-069 | Repeat fixed input/configuration, then vary one valid sample, StatsVersion, coefficient, cache assumption, or physical fallback. | Fixed planning inputs follow deterministic planning paths; changed valid inputs may alter costs and plans, while each execution remains within the owning result/error family under Optimizer Differential Correctness Tests. No accuracy threshold is invented. |
| V36-070 | Differentially execute a composed access/filter/project/join-or-Sort/Limit query through fresh, stale, missing, fallback, and saturated cost inputs. | Chosen legal plans match a trusted semantic/error oracle; only estimates, costs, legal shapes, resource use, and diagnostics vary. |

## V36-J — Frozen-owner regressions and integrity

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V36-071 | Run cost-different DML plans through V31-A/B/G/N. | Pre-W candidate/error closure, one mutation publisher, W/C/R, retries, counts, and result ownership are unchanged. |
| V36-072 | Vary worker count, morsels, scheduling, and legal early stop under V32-B/C/H/I. | Exact source occurrences, disjoint/exclusive claims, no replay, errors, and SQL semantics remain worker-independent; cost does not create occurrence identity. |
| V36-073 | Exercise capability, bounded search, active objective, dominance/ties, resource fallback, slots/properties, and validation under V33-A/C/D/E/F/G/H/I/K/N. | Only admitted legal alternatives may win; no global optimum beyond the bounded contract is required. |
| V36-074 | Exercise complete, valid-old, stale, missing, rejected, unequal cross-table, and concurrently replaced statistics under V34-B/C/F/G/J/K. | Costing preserves complete per-object retained generations and advisory-only authority without global StatsVersion equality. |
| V36-075 | Exercise representation-specific widths, logical fallbacks, unknown counts, saturation, provenance, and proof separation under V35-001–005, V35-013–021, V35-028–033, V35-040–041, V35-047–061, and V35-064–069. | Cost consumes the exact identified Chapter-35 outputs and never recomputes semantics, substitutes width representations, creates proof, or derives exact K. |
| V36-076 | Validate all V36 IDs, coverage entries, reusable references, optionality boundaries, and mandatory evidence categories against the live documents. | No duplicate/missing/orphan ID, broken reference, circular evidence claim, mandatory optional capability, or unavailable essential observation may PASS. |

### V36-077 — Calibration-tool primitive completeness and configuration ownership

Run one identified calibration invocation in a controlled target-deployment context. Record
the tool or equivalent calibration identity, invocation/context identity, each primitive
measurement event, workload, measured quantity and declared natural unit, and the attributable
derivation or aggregation path into the proposed relative weights. The eight required
primitive oracles are:

| §36.4 primitive | Controlled workload | Required independent observation and natural unit | Proposal evidence |
|---|---|---|---|
| cached sequential scan throughput | Repeated sequential scan over a controlled page/data volume after establishing the fixture's cached state | The cached sequential path actually executes and reports data volume per time, such as pages/time or bytes/time with page size recorded. | Identify the proposed-weight input or declared derivation/aggregation that consumes this cached-scan measurement. |
| cold-ish sequential page reads | Sequentially read a controlled page range after establishing and observing the fixture's cold-ish condition | The persistent sequential-read path actually executes and reports pages/time, bytes/time, or time/page with every conversion identified. | Identify its contribution or declared derivation into the sequential persistent-I/O relative weight. |
| random page reads | Read a controlled nonsequential page sequence distinct from the sequential fixture | The random persistent-read path actually executes and reports pages/time, bytes/time, or time/page with sequence identity retained. | Identify its contribution or declared derivation into the random persistent-I/O relative weight. |
| integer predicate throughput | Evaluate a controlled integer predicate over a known occurrence count | Executed predicate evaluations and elapsed/throughput quantity are observed as evaluations/time or time/evaluation. | Identify the proposed CPU/expression weight input or declared aggregation that consumes this measurement. |
| VARCHAR comparison throughput | Compare controlled VARCHAR operands with recorded lengths/content classes | Executed comparisons and elapsed/throughput quantity are observed as comparisons/time or time/comparison; any byte normalization is explicit. | Identify the proposed comparison-weight input or declared aggregation that consumes this measurement. |
| hash throughput | Hash controlled values with recorded operation count and payload size | Executed hash operations and elapsed/throughput quantity are observed as operations/time or time/operation; any byte normalization is explicit. | Identify its contribution or declared derivation into the hash-operation relative weight. |
| sort comparison throughput | Sort controlled keys while independently counting comparator invocations | Executed sort comparisons and elapsed/throughput quantity are observed as comparisons/time or time/comparison. | Identify its contribution or declared aggregation into the comparison relative weight without counting the same measurement twice. |
| temporary spill read/write throughput | Write and read controlled temporary-page volumes through the temporary-spill path | Both executed write and read quantities are observed in pages/time, bytes/time, or time/page with page size and direction retained. | Identify its contribution or declared derivation into the temporary-page-I/O relative weight. |

The positive control requires all eight distinguishable observations in that invocation and
an attributable relative-weight proposal. For every measurement, the proposal identifies
the proposed weight or its declared derivation/aggregation path; Architecture does not
require a one-to-one eight-measurement-to-seven-coefficient formula. Any proposed CostConfig
conversion values consumed by planning pass §§36.2.1/36.3 validation, while cache and
physical-fallback assumptions remain separately configured unless the proposal explicitly
and validly supplies them. The controlled measurements, rather than untraceable hard-coded
machine defaults, must account for the proposal. No universal coefficient values,
latency-accuracy threshold, or claim of SQL truth/cardinality follows from calibration.

The same invocation proves that its result is deployment/configuration data: applying a
valid proposal may create a later retained configuration identity, but does not add or alter
database-file, catalog/schema, TABLE/COLUMN/INDEX statistics, ANALYZE-payload, semantic-proof,
or PhysicalPlan-property fields. External storage of deployment configuration remains
permitted and is not treated as database persistent format.

For the negative control, suppress each of the eight measurement paths independently after
proving its controlled workload was selected; a syntactically valid finite proposal still
fails calibration completeness when any required measurement is proven omitted. Also reject
as nonconforming a completed measurement set with no relative-weight proposal, a proposal
whose contributions cannot be attributed to the measured invocation, or a contract that
makes calibration output a database-format requirement. If the invocation, selected path,
measurement event/unit, proposal attribution, or ownership boundary cannot be observed, the
result is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, not PASS. These controls inherit
V36-003; an invalid proposed CostConfig remains distinct and follows the existing
OptimizerError/configuration-validation owner when consumption is attempted.

## V36 atomic architecture-obligation ledger

| Atomic range | Contract under test | Required reusable oracle |
|---|---|---|
| V36-001–V36-005 | Correlated cost event, positive/missing evidence, nonvacuous boundaries, identity/state separation | V33-A/G/I; V34-B/K; V35-001–005; `Optimizer Diagnostics Tests` |
| V36-006–V36-023 | All 15 CostConfig fields, validation, stability, and dimensional conversion | V24-I/L; V33-G; V35-013–021; `Cost Model Tests` |
| V36-024–V36-029 | Finite construction, saturation, startup/run/total, comparison and determinism | V33-C/D/E; V35-040–041; `Memo and Pruning Tests`; `Optimizer Determinism and Resource-Limit Tests` |
| V36-030–V36-035 | Physical-input precedence, heap fallbacks, stable per-object generations and provenance | V34-B/C/J/K; V35-050–055; `Access Path Tests` |
| V36-036–V36-043 | B+ structure, entry/leaf pressure, locality, point/range candidate estimates | V22-D/F; V27-E/F/H; V34-F/G/K; `Access Path Tests` |
| V36-044–V36-049 | Sargability, residuals, enumeration, one-index baseline, required-column work | V22-C/D/F; V27-B/D/E/F/N; `Access Path Tests`; `Physical Property and Enforcement Tests` |
| V36-050–V36-058 | One work owner, all streaming transfers, evaluation domains and child inclusion | V20-5/15/16; V22-G; V27-I/J/K/L; `Scan and Unary Operator Tests` |
| V36-059–V36-064 | Startup/partial objectives, exact K, specialized precedence, eligibility and search handoff | V22-J/K; V28-E/F/K/L/O; V29-N/O; V30-E/F/I/J/K; V33-C/D/H; `Memory/Spill Plan Tests` |
| V36-065–V36-070 | Proof/error/resource boundaries, diagnostics, determinism and differential handoff | V20-19; V22-I/K; V24-A/L/N; V33-E/F; V35-056–061/066–068; `Final Optimizer Validation Tests`; `Optimizer Differential Correctness Tests` |
| V36-071–V36-076 | Frozen Chapters 31–35 and integration/reference integrity | V31-A/B/G/N; V32-B/C/H/I; V33-A/C/D/E/F/G/H/I/K/N; V34-B/C/F/G/J/K; V35-A/C/E/F/G/H/I/J |
| V36-077 | §36.4/§42.5 calibration primitive completeness, relative-weight proposal, and deployment/configuration ownership | V36-002–V36-003; `Cost Model Benchmarks` supplies only supplementary operator-ranking methodology |

## Chapter 36 invariant coverage map

| §36.19 invariant | Exact V36 fixture and independent oracle |
|---|---|
| 1 — abstract costs, not milliseconds | V36-023/V36-068; vary calibration and compare normalized components, not elapsed time |
| 2 — inspectable components | V36-001–V36-002/V36-023; missing component evidence cannot PASS |
| 3 — no instantaneous BufferPool input | V36-013/V36-035 with poisoned residency access |
| 4 — SeqScan pages and versions | V36-031–V36-032/V36-046; independent page/version ledger |
| 5 — index heap MVCC checks | V36-041–V36-043; V27-E/F runtime oracle |
| 6 — logical selectivity versus physical pressure | V36-037–V36-043; distinct controlled counts |
| 7 — correlation/bounded distinct-page fallback | V36-040/V36-043; extreme locality fixtures |
| 8 — per-index predicate classification | V36-044; `Access Path Tests` |
| 9 — leftmost equalities plus one range | V36-044; cursor/bound reference oracle |
| 10 — IS NULL versus `= NULL` | V36-044; V20-5 and `Selectivity Estimation Tests` |
| 11 — transient sentinels | V36-044; V27-E and persisted-format negative check |
| 12 — exact bounds versus residuals | V36-044–V36-045; independent normalized predicate |
| 13 — SeqScan plus every usable single index | V36-047; pre-comparison enumeration ledger |
| 14 — one index per occurrence | V36-048; BindingId-tagged alternatives |
| 15 — cost-derived break-even | V36-046–V36-048/V36-069; vary one cost input |
| 16 — required-column work | V36-049; RequiredSlotSet/width ledger |
| 17 — numerical zero is not absence | V36-031/V36-041/V36-065; runtime-match and proof-negative oracle |
| 18 — stable validated dimensional CostConfig | V36-006–V36-023; field/domain/config-identity ledger |
| 19 — checked saturation, no proof/resource meaning | V36-024–V36-028/V36-065–V36-067 |
| 20 — missing physical fallbacks preserve costability | V36-030–V36-043/V36-047 |
| 21 — one owner and ordinary child exactly once | V36-050–V36-058; omission/duplication negatives |
| 22 — actual evaluation domains, demand/K/properties unchanged | V36-044–V36-045/V36-052–V36-063 |

## Chapter 36 subsection coverage map

| Architecture subsection | V36 integration coverage | Exact reusable coverage |
|---|---|---|
| §36.1 philosophy/abstract dimensions | V36-001/V36-023/V36-068–069 | `Cost Model Tests`; `Optimizer Diagnostics Tests` |
| §36.2 Cost structure | V36-001–005/V36-027 | V22-I; `Cost Model Tests` |
| §36.2.1 finite arithmetic | V36-024–029 | V24-I/L; V35-040–041 |
| §36.2.2 local/child composition | V36-050–064 | V22-G; V27-I/J/K/L; V28-E/F/K/L/O; V29-N/O; V30-E/F/I/J/K |
| §36.3 CostConfig and units | V36-006–023 | V33-G; V35-013–021; `Cost Model Tests` |
| §36.4 calibration | V36-077; V36-029/V36-069 cover only fixed-input determinism and legal calibration variation | §42.5 cross-owner; `Cost Model Benchmarks` supplies only supplementary operator-ranking methodology |
| §36.5 cache model | V36-013/V36-035/V36-040 | `Access Path Tests` |
| §36.5.1 input precedence/fallback | V36-030–043 | V34-B/C/J/K; V35-050–055 |
| §36.6 SeqScan cost | V36-031–032/V36-046 | V27-B/D/N; `Access Path Tests` |
| §36.7 B+ point lookup | V36-036–042 | V27-E/F/H; V34-G/K |
| §36.8 range scan | V36-037–043 | V27-E/F/H; `Access Path Tests` |
| §36.9 correlation | V36-040/V36-043 | V34-F/G/K |
| §36.10 distinct heap pages | V36-039–043 | `Access Path Tests` |
| §36.11 predicate classification | V36-044–045 | V27-E/F; `Access Path Tests` |
| §36.12 B+ sargability | V36-044 | V20-5; V27-E/F |
| §36.13 composite bounds | V36-044 | V27-E; `Access Path Tests` |
| §36.14 residual predicates | V36-044–045/V36-053 | V20-15/16; V27-F |
| §36.15 alternatives | V36-046–048 | V22-D/F; `Access Path Tests` |
| §36.16 one-index baseline | V36-048 | V22-D; `Access Path Tests` |
| §36.17 break-even | V36-046–048/V36-069 | `Access Path Tests`; `Cost Model Tests` |
| §36.18 required-column cost | V36-049/V36-050/V36-057 | V22-C/G; V27-N |
| §36.19 invariants | V36 invariant map above | Listed exact owners |

## Chapter 41 Chapter-36 obligation coverage map

| Owner and obligation | Procedure and controlled evidence | Observable result |
|---|---|---|
| §§36.2.1–36.3, 38.4, 41.6–41.7 configuration/units/arithmetic | V36-006–029 | Valid fields convert once; invalid inputs rejected; saturation/comparison finite |
| §§33.3–33.4, 34.15, 36.5–36.5.1 stable inputs/cache | V36-013/V36-030–035/V36-038 | One stable invocation view; no BufferPool or generation mixing |
| §§34.4/34.6, 36.6–36.10, 41.6 physical access work | V36-031–043 | Heap/index fallback inputs and provenance feed finite costs |
| §§36.11–36.14, 41.6 sargability/bounds/residuals | V36-044–045 | Exact classification/bounds and actual residual domains |
| §§36.15–36.17 base alternatives/break-even | V36-046–048 | SeqScan/all usable single-index paths costed before comparison |
| §§36.18, 22, 37, 41.5 required slots/properties | V36-049/V36-063 | Width/work reflects required slots; low cost cannot legalize invalid plan |
| §§36.2.2, 27, 38 streaming composition | V36-050–059 | One owner, exactly-once child, implementable startup/run |
| §§30.7, 35.20, 38.15–38.16 exact K/objectives | V36-056/V36-060–061 | Full/partial/exact-K boundaries preserved |
| §§38.8–38.20 specialized algorithms/resources | V36-058/V36-062/V36-067 | Specialized repeated/blocking/spill work remains authoritative |
| §§20.17.10, 35.2, 38.24 exact proof/final validation | V36-063/V36-065 | Cost/zero/saturation creates no proof; invalid winner rejected |
| §§34–36 missing/stale/rejected/corrupt ownership | V36-030–035/V36-066 | Correct valid-old/fallback/corruption/structure/config owner |
| §§33.6, 38.3–38.5/38.21 comparison, ties, bounded fallback | V36-028–029/V36-064/V36-067 | Legal finite comparison, deterministic ties, canonical resource fallback |
| §§24, 38.19–38.20, 39 resource separation | V36-012/V36-062/V36-067 | Estimate is not grant; planning/runtime failures stay distinct |
| §§40, 41.5–41.7 diagnostics/differential correctness | V36-001–005/V36-068–070 | Trace agrees with identified cost path; semantics match reference |
| Chapters 31–35 frozen obligations | V36-071–075 | Lower cost waives no mutation, occurrence, search, statistics, estimate, or proof rule |

## V36 adversarial-case matrix

| Cases | New procedure | Reused oracle | Controlled failure/result |
|---|---|---|---|
| A–D missing/negative/nonfinite/zero weight | V36-021 | V24-L; `Cost Model Tests` | Rejected before costing; no silent default |
| E valid zero cache | V36-013 | `Access Path Tests` | Accepted no-cache assumption, not divisor |
| F invalid correlation; G invalid divisor | V36-020–021/V36-040 | V34-G | Reject invalid config before comparison |
| H–J huge conversion/multiplication/startup sum | V36-024–027 | V24-I; V35-040–041 | Checked canonical saturation |
| K both saturated tied | V36-028 | V33-D; `Optimizer Determinism and Resource-Limit Tests` | Structural tie key decides |
| L–M legal calibration/fixed determinism | V36-029/V36-069 | V33-G | Legal differences allowed; fixed inputs deterministic |
| N–R missing heap inputs/zero pages/zero logical rows | V36-031–033/V36-046 | V34-C/K; V35-017–018/056 | Finite SeqScan retained; no proof |
| S–T no IndexStats, known height/missing pressure | V36-036–040 | V27-E/F; V34-K | Structural height plus finite derived/fallback inputs |
| U invalid B+ metadata | V36-036/V36-063 | V22-D/K; V27-Q | Ineligible despite cheap cost |
| V–X zero occupancy/missing correlation/empty index | V36-039–040 | V34-G; `Access Path Tests` | Zero-safe fallback and approximate work only |
| Y fallback estimate used as runtime cap; Z stale-zero runtime match | V36-041/V36-065 | V27-E/F; V35-056 | Runtime traversal/results retained; no proof |
| AA several indexes; AB cheap incompatible index | V36-047–048/V36-063 | V22-D/F; `Access Path Tests` | All usable single paths enumerated; incompatible excluded |
| AC–AD many pages/few rows and dead-heavy scan | V36-032/V36-046 | V27-B/D | Page/version work independent of output |
| AE many entries/few visible rows | V36-041–043 | V27-E/F | Physical candidate and MVCC work retained |
| AF pre-visibility; AG post-visibility residual | V36-045 | V27-E/F | Distinct actual evaluation domains |
| AH pushed/separate Filter; AI fused/separate Project | V36-050/V36-054–057 | V27-I/J | One actual owner, no omission/duplication |
| AJ–AL omitted/doubled child or pushed work | V36-058 | V22-G; `Cost Model Tests` | Independent ledger rejects composition |
| AM selective first row; AN blocking child | V36-054/V36-059–060 | `Memory/Spill Plan Tests` | Approximate startup and blocking boundary preserved |
| AO count once; AP unknown count as exact K | V36-056/V36-061 | V20-11; V22-J/K; V30-J | One runtime acquisition; no approximate K |
| AQ nested-loop repeat; AR Sort blocking | V36-058/V36-062 | V28-E/F; V30-E/F | Specialized formula precedence |
| AS wrong order; AT missing slot | V36-063 | V22-C/D/K; `Physical Property and Enforcement Tests` | Low cost cannot legalize |
| AU saturation as impossibility | V36-026/V36-065 | V20-19; V35-058 | Saturation remains cost metadata |
| AV missing stats reject SQL | V36-031/V36-047/V36-066 | V34-C/K; V35-017 | Fallback and legal SeqScan remain |
| AW cross-table versions equal; AX publication changes P | V36-034 | V33-G; V34-B/J | Per-object retained generations remain stable |
| AY estimate as grant | V36-067 | V24-A/N; V35-049 | Runtime allocation remains independent |
| AZ planning exhaustion with fallback | V36-067 | V33-E/F | Legal bounded fallback or canonical resource error |
| BA cheap plan suppresses SQL error | V36-050/V36-065/V36-070 | V20-15/16; V35-061 | Demanded error remains observable |
| BB different valid samples | V36-069 | V34-F/J; V35-055 | Cost may differ; semantics invariant |
| BC diagnostics disagree with selected cost | V36-002/V36-068 | `Optimizer Diagnostics Tests` | Missing/mismatched evidence cannot PASS |

## V36 reuse inventory and stale-rule audit

Exact reusable procedure IDs used by this section are V20-5, V20-11, V20-15,
V20-16, and V20-19; V22-C, V22-D, V22-F, V22-G, V22-I, V22-J, and
V22-K; V24-A, V24-I, V24-L, and V24-N; V27-B, V27-D, V27-E, V27-F, V27-H,
V27-I, V27-J, V27-K, V27-L, V27-N, and V27-Q; V28-E, V28-F, V28-K, V28-L,
and V28-O; V29-N and V29-O; V30-E, V30-F, V30-I, V30-J, and V30-K; V31-A,
V31-B, V31-G, and V31-N; V32-B, V32-C, V32-H, and V32-I; V33-A, V33-C,
V33-D, V33-E, V33-F, V33-G, V33-H, V33-I, V33-K, and V33-N; V34-B, V34-C,
V34-F, V34-G, V34-J, and V34-K; and V35-001–V35-005, V35-013–V35-021,
V35-028–V35-033, V35-040–V35-041, V35-047–V35-061, and V35-064–V35-069.
The cited V35 family identifiers V35-A, V35-C, V35-E, V35-F, V35-G, V35-H,
V35-I, and V35-J are the live owners of those atomic procedures.

Exact reusable headings used here are `Scan and Unary Operator Tests`, `Selectivity
Estimation Tests`, `Access Path Tests`, `Physical Property and Enforcement Tests`,
`Memory/Spill Plan Tests`, `Memo and Pruning Tests`, `Cost Model Tests`, `Cost Model Benchmarks`, `Optimizer Determinism and Resource-Limit Tests`,
`Final Optimizer Validation Tests`, `Optimizer Differential Correctness Tests`, and
`Optimizer Diagnostics Tests`.

The live Verification procedures must contain no permission for negative/nonfinite or
undocumented default coefficients; no rejection of valid zero cache/dead/correlation
inputs; no unguarded cost overflow, competing ceiling, or saturation-as-resource/proof
interpretation; no momentary BufferPool input or mandatory ANALYZE; no missing-index-stats
ineligibility; no estimated physical count as runtime cursor cap; no numerical-zero physical
emptiness; no omitted heap MVCC; no mandatory index-only, bitmap, expression-index, merge,
or ordered capability; no residual charged only on survivors when evaluated earlier; no
omitted/doubled child or fused work; no ordinary-streaming treatment of blocking Sort; no
exact K from unknown counts; no low-cost waiver of slots/properties/demand/errors; no memory
estimate treated as grant; no weakening of V31/V32; and no globally cheapest requirement
beyond bounded search. Apparent matches are classified by owner, stage, approximation,
optionality, or explicit negative-test context before being treated as stale. Any required
identity, source, workload, coefficient, ownership, arithmetic boundary, objective, proof,
or validation evidence that is unavailable is `NOT VERIFIED / TEST INFRASTRUCTURE
INCOMPLETE`.
