# Chapter 34 — Statistics Collection, Publication, and Planner Visibility Verification

This family composes the existing statistics algorithm, persistence, transaction, catalog,
memory, pipeline, optimizer, and diagnostic procedures at Chapter 34's integration
boundaries. It does not replace those component suites. A conforming harness may use trace
events, deterministic barriers, an independent catalog-row ledger, or equivalent
inspectable state; it need not expose a particular class, lock, cache, reference-counting
scheme, or collection-worker implementation.

## V34-A — ANALYZE generation, event, and ownership oracle

For each fixture maintain an independent correlated ledger keyed by ANALYZE invocation,
owning `(TxnId, CommandId)`, bound `TableId`/`SchemaVer`, effective SQL snapshot, collection
attempt, candidate TABLE manifest, required `ColumnId`/`IndexId` members, exact candidate
`StatsVersion`, and transaction outcome. Record collection/validation completion,
`STATS_PUBLISH` admission and manifest revalidation, the first statistics-row publication,
C4 terminal visibility, C5 cache install/invalidate/bypass, planner selection/retention,
reclamation eligibility, and cleanup. Member membership comes from exact row/payload and
manifest identities, never names, equal values, estimates, or equal-looking version
counters.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-001 | Run one valid explicit/manual ANALYZE from bound target through collection, candidate validation, transactional rows, COMMIT, cache action, and a later planner lookup. | The ledger observes the real collection and validation events, one publication coordinator, first-row boundary, C4/C5 outcomes, one complete selected generation, and planner retention with the same identities. This is the positive instrumentation control. |
| V34-002 | Suppress or disconnect each essential observation in turn, including validation, first row, C4, C5/coherent fallback, planner retention, and cleanup. | The result is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from an empty or stage-skipping trace. |
| V34-003 | Arm a boundary-specific fault but hold execution before that boundary, then release it in a second run. | The first run is a failed setup, not a conformance PASS; the second must prove the intended boundary and fault were reached before checking outcome. |
| V34-004 | Correlate all TABLE/COLUMN/INDEX rows and payloads with the bound statement and transaction ledger. | Exactly one statement attempt owns one StatsVersion and candidate; no helper, cache callback, or planner is misidentified as publication owner. |
| V34-005 | Independently reconstruct globally selectable generations from ordinary catalog MVCC rows after commit/restart. | The reconstruction distinguishes incomplete candidate rows, incomplete committed row sets, rejected generations, committed complete generations, retained older generations, and explicit missing-statistics state. |

## V34-B — Per-object generation identity and stable planner scope

Use the V34-A ledger with `Descriptor immutability, cache, and catalog MVCC`, `Statistics
Publication and Versioning Tests`, and V33-G. SQL aliases remain logical occurrences; the
statistics object is keyed by the underlying stable table identity.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-006 | Give T1 complete compatible S1 and T2 complete compatible S2 with `S1 != S2`, then optimize a join. | Retained descriptor evidence shows T1/S1 and T2/S2 throughout the invocation; version inequality alone is accepted and neither object switches. |
| V34-007 | Self-join T as aliases a and b, pause after T's descriptor retention, publish a newer generation, and resume. | Both aliases use one retained generation for the underlying `TableId`, while logical relation/row occurrences remain distinct; no mid-invocation switch occurs. |
| V34-008 | Run two independent planners around a same-table publication. | Each planner may retain a different valid generation, but each invocation is internally stable and complete. |
| V34-009 | Retain S1, publish S2, and inspect every required member read during costing. | Every read is from S1 and its immutable backing; no S1/S2 member mixture or in-place mutation is observable. |
| V34-010 | Construct same names and equal statistical values for distinct/recreated object identities. | Names and equal payload values do not establish generation or catalog compatibility; stable IDs and manifest fingerprints decide. |
| V34-011 | Compare catalog `SchemaVer` and StatsVersion values that happen to be numerically equal or unequal. | Numeric equality has no authority; compatibility uses object/schema/member identity and StatsVersion retains only its canonical generation role. |

## V34-C — Manifest coherence, validation, and fallback selection

Construct catalog rows directly and invoke the exact loader/materializer path from
`Statistics Persistence and Validation Tests`. Every negative proves whole-generation
rejection and records the next older qualifying generation or explicit missing fallback.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-012 | TABLE manifest S2 with a required COLUMN payload from S1. | S2 is incomplete/invalid as a unit; no cross-version salvage or planner-visible mixed descriptor. |
| V34-013 | TABLE manifest S2 with a required INDEX payload from S3. | Same whole-generation rejection and fallback behavior as V34-012. |
| V34-014 | Remove, duplicate, or malform one required COLUMN or INDEX member. | Exact manifest completeness/uniqueness validation rejects the generation; valid siblings are not retained selectively. |
| V34-015 | Keep StatsVersion equal but substitute wrong `TableId`, `SchemaVer`, `ColumnId`, or `IndexId`. | Identity/schema validation rejects the generation despite matching version and names. |
| V34-016 | Keep names and version equal but change index ownership or key-schema/fingerprint identity. | The incompatible index member and therefore the whole generation are rejected. |
| V34-017 | Present no generation, or only incomplete/malformed/unsupported/incompatible generations. | Rejection reasons remain distinct and canonical missing-statistics fallback is selected when none qualifies; no synchronous ANALYZE is required. |
| V34-018 | Present older complete compatible S1 and newer incomplete, malformed, unsupported, or incompatible variants. | Selection uses the greatest visible committed complete applicable generation, hence S1 when it remains qualifying; it never assembles a hybrid. |
| V34-019 | Present newer complete compatible S2 and older complete compatible S1. | Unsigned lexicographic StatsVersion selection chooses S2 independently of callback order. |
| V34-020 | Corrupt outer catalog tuple/page framing instead of advisory payload content. | The stronger catalog/page-corruption owner is preserved; it is not downgraded to harmless missing statistics. |

## V34-D — Complete publication, StatsVersion, and concurrent ANALYZE

Use barriers before construction completion, validation, `STATS_PUBLISH`, manifest
revalidation, first row, intermediate rows, C4, and C5. Inspect catalog MVCC independently
of cache state.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-021 | Pause a multi-member candidate at each pre-C4 barrier and query from another transaction. | No partial/uncommitted generation is globally selectable; the previous committed generation remains authoritative. |
| V34-022 | Commit all required TABLE/COLUMN/INDEX rows and inspect immediately before and after C4. | Eligibility changes only at terminal COMMITTED and only for the complete validated per-table generation. |
| V34-023 | Delay/fail C5 cache installation after durable committed publication. | COMMITTED and persistent rows remain; cache becomes coherently installed, invalidated/bypassed, older, or missing as §39.1.8 permits, never torn. |
| V34-024 | Verify assignment and comparison with multiple commands in one transaction and different TxnIds. | `StatsVersion` is exactly `(TxnId, CommandId)`, has no extra allocator, and compares by unsigned lexicographic pair; it is not wall-clock, schema-version, or visibility proof. |
| V34-025 | Run two same-table ANALYZE statements with controlled claim, row-publication, terminal, and reverse callback orders. | Compatible claims follow §14.17.1; each selectable generation is complete, greatest applicable version wins, and delayed older callback cannot regress cache state. |
| V34-026 | Fail or cancel one concurrent same-table attempt before or after its first row while the other commits. | Failed attempt never supplies a usable partial generation; the successful attempt remains independently selected according to version/applicability. |
| V34-027 | Run ANALYZE concurrently on T1 and T2. | Publications are independent per-table units, may use different StatsVersions, and encounter no database-global generation barrier. |
| V34-028 | Retire creator transaction status, freeze outer catalog tuple metadata, restart, and reload a complete old generation. | Payload StatsVersion remains directly comparable and usable without creator-status lookup, status-page retention, or terminal-cache reconstruction; outer tuple MVCC remains independent. |
| V34-029 | Supply invalid normal-TxnId/CommandId domains, row/payload mismatch, or cross-version chunks. | Structural validation rejects them; missing retired creator status alone does not. |

## V34-E — DDL, VACUUM, DML, and publication ownership

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-030 | ANALYZE acquires all `STATS_PUBLISH` claims first; then overlapping DROP/manifest DDL requests `MANIFEST_CHANGE`. | DDL waits through terminal publication in the unified graph; claims are retained after first row and released only at the canonical terminal cleanup. |
| V34-031 | DDL wins `MANIFEST_CHANGE` before ANALYZE final revalidation, including drop/recreate under the same name, column replacement, and index replacement. | Current stable identities/manifest fail revalidation before first stats row; the entire candidate is discarded under the canonical prepublication outcome. |
| V34-032 | Run a permitted VACUUM during ANALYZE collection with page/read-epoch and object-use barriers. | SQL-visible statistics use the fixed snapshot; physical observations may be approximate; backing/RID lifetime is safe and VACUUM gains no publication authority. |
| V34-033 | Fix the ANALYZE snapshot, then commit INSERT/UPDATE/DELETE under READ COMMITTED and REPEATABLE READ fixtures. | Collection follows Chapter 9's effective snapshot; later DML may make estimates stale but cannot alter visibility semantics or become an ANALYZE publication. |
| V34-034 | When helpers are supported, let one finish/fail and attempt direct publication; when absent, run the coordinator-only positive path. | Helper publication is rejected and exactly one coordinator owns rows/claims; absence of optional parallel ANALYZE is not a failure. |
| V34-035 | Let an optional §14.16 maintenance trigger request ANALYZE and separately request VACUUM. | Trigger/scheduler has no publication authority; ANALYZE and VACUUM retain distinct canonical owners and results. |

## V34-F — Collection strategy and bounded statistical algorithms

Compose `Statistics Tests` and `Statistics Algorithm Tests` with the V34-A snapshot and
candidate identity. Fixed seeds or injectable sampler state make structural assertions
repeatable; independent approximate samples need not be bitwise equal.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-036 | Instrument the required visible-row source over multiple chunks/pages. | Every snapshot-visible heap row is accepted once by the vectorized full scan; mandatory replacement page/block sampling is rejected. |
| V34-037 | Exercise the configured small-table threshold on both sides and under insufficient maintenance memory. | Exact NDV/frequencies occur only when the permitted bounded exact mode fits; changing mode changes quality, not validity or SQL truth. |
| V34-038 | Feed NULLs, canonical-equal values, duplicates, and known cardinalities to HLL. | NULL is excluded; `p=14`, 16,384 registers, register domain, writer bounds, and persisted NDV-only contract hold; malformed state fails before persistence. |
| V34-039 | Feed skew, cutoff ties, uniform values, and duplicate-heavy tails to the MCV collector. | At most 64 canonical unique MCVs, deterministic accepted order/mass, no unbounded exact map, and no NULL identity. |
| V34-040 | Replay fixed bounded reservoirs with skew, duplicates, equal boundaries, and MCV overlap. | Target remains 100,000 subject to memory, bin count at most 100, boundaries/masses validate, and MCV residual mass is not double counted. |
| V34-041 | Exercise VARCHAR bytes, FLOAT64 specials, BOOLEAN, integer, DATE, and TIMESTAMP values. | Hash/equality/order use Chapter 17 and §34.14.6 semantics; locale, pointer identity, raw NaN comparison, and host iteration are not oracles. |
| V34-042 | Collect required physical index pressure through full walk or bounded sampling while concurrent DML changes physical state. | Logical live count follows the snapshot; physical/garbage/page/correlation values remain approximate advisory metadata and valid within their domains. |
| V34-043 | Repeat accepted bounded collection with varied chunk/merge/worker order where a capability exists. | Every candidate passes identical deterministic validation/normalization; the HLL pre-bound estimate is exactly chunk-invariant for the same distinct set under §34.9, while other approximation may differ and optional merge/parallel capability is not mandated. |

## V34-G — Degenerate descriptors and persisted validation

Reuse `Statistics Persistence and Validation Tests` and exact dyadic/rational fixture
construction. Confirm live constants from Architecture: 40-byte common header, 104-byte
TABLE fixed prefix, 104-byte COLUMN fixed prefix, 112-byte INDEX payload, 4096-byte maximum
fragment, and aggregate tolerance `2^-40`.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-044 | Build empty-table and nonempty all-NULL columns. | Exact §34.14.6.7 flags, `+0` fields, NDV excluding NULL, empty MCV/histogram, and width forms are required; no fabricated min/max. |
| V34-045 | Build one-value/repeated-equal and residual-mass-zero columns. | Min=max and NDV constraints hold; representation is one legal canonical form, with no duplicate identity or MCV/histogram double count. |
| V34-046 | Corrupt header, length, checked arithmetic, fragment geometry/order/identity, CRC32C, scope, flags, or reserved fields one at a time. | Validation occurs before unsafe dependent access and rejects the complete generation with the canonical invalid/unsupported distinction. |
| V34-047 | Corrupt PersistedScalarV1 type/value canonicality, MCV uniqueness/order, histogram order/overlap, widths, NDV/counts, correlation, or manifest relationships. | Exact §34.14.6 registry rejects rather than clamps/salvages; valid-old/missing fallback remains available. |
| V34-048 | Execute every §34.14.6.10 endpoint, adjacent value, exact-sum, and both-sides-of-`2^-40` vector. | Acceptance and normalized process-local descriptor are platform-independent; only the two specified aggregate relationships use tolerance. |
| V34-049 | Serialize/decode every accepted candidate and compare normalized semantic descriptors. | Round trip preserves exact identities and canonical `+0`/mass normalization; reader acceptance does not rewrite payload bytes. |
| V34-050 | Present persisted HLL-register or incremental-sketch fields not in the defined architecture. | No such field is required or inferred; persists the defined NDV estimate and exact existing payload only. |
| V34-051 | Compare stale-but-internally-valid physical counts with current storage. | Staleness is not numerical corruption; logical/physical distinctions remain intact and zero estimates do not suppress runtime access. |

## V34-H — Retained values, memory, spill, and cleanup

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-052 | Collect a large VARCHAR, release/poison scan chunks and page guards, then construct, serialize, publish, cache, retain, and GC the descriptor. | Exact bytes remain available through authorized ownership; no published descriptor borrows destroyed storage or truncates a representable value. Reuse V23-G/I/K. |
| V34-053 | Track collection state, bounded sketches/reservoirs, temporary payload/chunks, spill, published backing, planner handles, and GC in the Chapter-24 ledgers. | Every growing resource has continuous accounting and one cleanup owner; grant is not allocation success, transfers have no gap, and release occurs exactly once. |
| V34-054 | Deny supported allocation, exact representability, source read, spill write/read, and reload at their declared boundaries. | Preserve V24/§39 cause categories (`OutOfMemory`, representability `ExecutionError`, `SpillIOError`, or stronger lower-layer failure); no partial selectable generation or leak. |
| V34-055 | Cancel/fail while worker, page guard, read epoch, borrower, or spill I/O is active. | V26-K/M quiescence precedes unsafe teardown; retained state survives until users release it and no helper publishes during cleanup. |

## V34-I — Failure, cancellation, commit, and cache boundaries

Every row first proves the named boundary was reached and records candidate identity, row
count published, transaction state, claims, cache state, and cleanup. The terms FA/MA are
used only with their Chapter-39 meanings; ANALYZE does not acquire DML W/C/R semantics.

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-056 | Inject collection, source-read, allocation/spill, retention, construction, serialization, validation, claim-acquisition, or manifest-revalidation failure before the first stats row. | No row is published; candidate is unusable/discarded, prior generation remains, and the exact recoverable FA/stronger owner and cleanup apply. |
| V34-057 | Cancel during collection, before validation, or after claims but before first row. | Same pre-row visibility guarantee; explicit transaction may remain ACTIVE only where Chapter 39 permits, while autocommit follows its owner. |
| V34-058 | Fail while publishing a later member after the independently observed first stats row. | Transaction enters the required MA/abort path; incomplete rows stay globally unusable, claims persist to terminal cleanup, and old committed generation remains valid. |
| V34-059 | Cancel immediately after first row or before the publication-authorizing commit append. | Post-row abort consequences apply until commit admission; cancellation cannot expose a partial generation. |
| V34-060 | Cancel or lose the session after the authorizing commit append. | COMMIT is uncancellable; runtime completes C2–C5 or enters the canonical noncontinuable gate, never physical undo or ABORTED relabeling. |
| V34-061 | Crash before surviving durable commit, after durable commit before C4/C5, and after status reclamation. | Recovery selects only visible complete committed rows and never needs payload creator-status lookup; loser/incomplete rows are excluded. |
| V34-062 | Fail cache installation after COMMITTED while safe invalidate/bypass/older/missing fallback succeeds. | COMMITTED survives and cleanup completes coherently; cache installation is not commit authority. |
| V34-063 | Make C5 cache/ownership coherence impossible. | Preserve COMMITTED plus Chapter 39's noncontinuable classification and unsafe ownership; do not pretend a safe statistics fallback was established. |
| V34-064 | Fail result/completion delivery or lose the session after successful C4–C5. | Committed publication is not reversed; delivery/session classification remains separate from ANALYZE transaction outcome. |

## V34-J — Cache ordering, planner retention, and old-generation reclamation

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-065 | Planner P retains complete compatible S1; publish S2 while P is paused; start planner Q afterward. | P reads only S1 and Q may select S2; descriptor/member identity and backing, not similar estimates, prove retention. |
| V34-066 | Make S1 otherwise a statistics-GC candidate while P retains it, then attempt reclamation. | S1 and all scalar backing remain live until P/catalog snapshot releases them; reclamation then occurs once without permanent leak or double free. |
| V34-067 | Deliver delayed same-table install callbacks in descending and ascending version order. | Cache install is conditional on applicability and greater version; same version is idempotent and older callback cannot regress state. |
| V34-068 | Commit DROP before a delayed ANALYZE cache callback, then recreate the same name with new IDs. | Callback rechecks applicability, cannot reinstall retired statistics, and name reuse cannot capture the old descriptor. |
| V34-069 | Force a cache miss with valid persisted old rows and separately with no qualifying generation. | Reload uses catalog MVCC plus complete validation, never payload-status lookup; result is exact qualifying generation or explicit missing fallback. |

## V34-K — Statistics authority, estimator/cost handoff, and diagnostics

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-070 | Contrast estimated row count zero, TRUE fraction zero, stale absent MCV/range, and exact-looking empty ANALYZE count with visible matching rows or a demanded error. | Estimates/provenance remain non-proof; execution/visibility/error work is retained. Reuse `Semantic Emptiness Tests` and V33-G. |
| V34-071 | Supply an independent approved Chapter-20/35 exact emptiness proof for the same logical shape. | Only exact proof provenance authorizes elimination; mutating provenance to statistics makes validation fail. |
| V34-072 | Feed one complete descriptor and missing/invalid/incompatible variants through Chapters 35–38. | Field identities, version, freshness, row/null/NDV/MCV/histogram/width/index pressure reach estimator/cost/search together or canonical fallback does; no mixed field reaches planning and final plan remains legal. |
| V34-073 | Compare Chapter-40 diagnostics with retained descriptor, estimator input, selected plan, and runtime counters. | Exposed StatsVersion/provenance/staleness/estimates match the retained object, distinguish estimates from actuals/proofs, and never imply cross-table version equality or synchronous refresh. |

## V34-L — Frozen-owner regressions and optional capability boundaries

| ID | Controlled procedure | Observable oracle and required result |
|---|---|---|
| V34-074 | Run PhysicalAnalyze through V21-26 and V31-N with pure control lowering. | Bound target, one coordinator, snapshot, error/result ownership, and C4/C5 publication remain intact; no DML W/C/R or RETURNING rule is imported. |
| V34-075 | If parallel collection helpers exist, compose V32-A–V32-N source/claim/cancellation oracles; otherwise use coordinator-only collection. | Authorized helper work has exact ownership and scheduling independence but never independent publication; Chapter 32 alone does not require parallel ANALYZE. |
| V34-076 | Optimize with multiple objects, stale/zero statistics, and concurrent publication using V33-G. | One stable complete generation per underlying object, different-table versions allowed, statistics advisory, and final PhysicalPlan legal/validated. |
| V34-077 | Attempt targetless all-table ANALYZE, mandatory automatic ANALYZE, mandatory page-sampling replacement, mandatory persisted HLL sketch, and bitwise-identical independent samples. | These are not baseline requirements; explicit single-table ANALYZE/full heap scan/defined version-1 payload remains the positive path, and optional §14.16 scheduling gains no authority. |
| V34-078 | Differentially execute plans produced from valid, stale, missing, and rejected statistics over the same SQL snapshots and perturb collection/cache callback order. | Use Optimizer Differential Correctness Tests and V22’s owning-result-family oracle for SQL bags, NULL/type/error/visibility semantics; legal physical variation cannot add outcomes outside that family. Any missing required V34 observation yields infrastructure-incomplete, not semantic PASS. |

## V34 atomic architecture-obligation ledger

| Atomic range | Contract under test | Required hook or exact reusable oracle |
|---|---|---|
| V34-001–V34-005 | Correlated, nonvacuous ANALYZE/publication/planner event ledger | V34-A; `Statistics Publication and Versioning Tests`; V21-26; V31-N |
| V34-006–V34-011 | Per-object identity, cross-table independence, aliases, stable retention | V34-B; V33-G; `Descriptor immutability, cache, and catalog MVCC` |
| V34-012–V34-020 | Manifest/member rejection and exact valid-old/missing fallback | V34-C; `Statistics Persistence and Validation Tests` |
| V34-021–V34-029 | Complete publication, StatsVersion/status lifetime, concurrent ANALYZE | V34-D; `Statistics Publication and Versioning Tests` |
| V34-030–V34-035 | DDL/VACUUM/DML interleavings and coordinator ownership | V34-E; `Vacuum and Reclamation Tests`; V31-N; V32-N |
| V34-036–V34-043 | Full scan, exact mode, bounded HLL/MCV/reservoir/histogram/index collection | V34-F; `Statistics Tests`; `Statistics Algorithm Tests` |
| V34-044–V34-051 | Degenerate forms, byte-exact persistence, deterministic normalization | V34-G; `Statistics Persistence and Validation Tests` |
| V34-052–V34-055 | Large-value lifetime, accounting, spill, cleanup | V23-G/I/K; V24-B/D/J/M; V26-K/M |
| V34-056–V34-064 | Pre/post-row, commit, cancellation, cache, crash, delivery failures | V34-I; V26-I/K; V31-N; Chapter-39 transaction procedures |
| V34-065–V34-069 | Cache order, S1/S2 planner retention, GC, DROP invalidation | V34-J; catalog MVCC/reclamation and stable-snapshot procedures |
| V34-070–V34-073 | Non-proof statistics, downstream handoff, diagnostics | `Semantic Emptiness Tests`; V33-G; `Optimizer Diagnostics Tests` |
| V34-074–V34-078 | Frozen control/parallel/optimizer boundaries, optionality, differential semantics | V31-N; V32-A–V32-N; V33-G; optimizer differential procedures |

## Chapter 34 subsection coverage map

| Architecture obligation | V34 integration coverage | Exact reusable component coverage |
|---|---|---|
| §34.1 role | V34-070–V34-073, V34-078 | `Semantic Emptiness Tests`; V33-G |
| §34.2 ANALYZE interface and optional scheduling | V34-001, V34-035, V34-074, V34-077 | V21-26; `Control-Operator Tests`; V31-N |
| §34.3 visibility/publication | V34-001–V34-005, V34-021–V34-027, V34-056–V34-064 | `Statistics Publication and Versioning Tests`; Chapter-39 procedures |
| §34.3.1 StatsVersion/status lifetime | V34-024–V34-029, V34-061, V34-069 | `Statistics Publication and Versioning Tests`; catalog MVCC/status-reclamation procedures |
| §34.4 TableStatistics | V34-036, V34-044, V34-051, V34-072 | `Statistics Tests`; persistence validation |
| §34.5 ColumnStatistics | V34-038–V34-041, V34-044–V34-049, V34-052, V34-072 | `Statistics Algorithm Tests`; V23-G/I |
| §34.6 IndexStatistics | V34-042, V34-051, V34-072 | persistence validation; `Access Path Tests` |
| §34.7 collection strategy | V34-030–V34-043 | ANALYZE snapshot tests; V32 capability-conditional source tests |
| §34.8 small-table exact mode | V34-037 | `Statistics Tests` |
| §34.9 HLL | V34-038, V34-050 | `Statistics Algorithm Tests` |
| §34.10 MCV | V34-039, V34-045, V34-047–V34-049 | `Statistics Algorithm Tests` |
| §34.11 histogram collection | V34-040, V34-045, V34-047–V34-049 | `Statistics Algorithm Tests` |
| §34.12 equi-depth rationale | V34-040 | Structural bounded/equi-depth consequence only; no new quality policy |
| §34.13 histogram value semantics | V34-041, V34-047–V34-049 | Chapter-17 scalar oracles; `Statistics Algorithm Tests` |
| §34.14 persistence | V34-005, V34-012–V34-020, V34-044–V34-051 | `Statistics Persistence and Validation Tests` |
| §34.14.1 header | V34-046, V34-048–V34-049 | Exact framing/CRC fixtures |
| §34.14.2 TABLE manifest | V34-012–V34-020, V34-044, V34-047 | Manifest completeness fixtures |
| §34.14.3 COLUMN payload | V34-014–V34-016, V34-044–V34-050 | Scalar/mass/identity fixtures |
| §34.14.4 INDEX payload | V34-013–V34-016, V34-042, V34-047, V34-051 | Index consistency fixtures |
| §34.14.5 validation/rebuildability | V34-017–V34-020, V34-046–V34-051, V34-069 | Invalid/unsupported/corruption classification |
| §34.14.6 validation/normalization | V34-036–V34-051 | Complete boundary-vector component suite |
| §34.15 snapshots/cache | V34-006–V34-011, V34-021–V34-029, V34-062–V34-069 | Stable snapshot and descriptor-lifetime procedures |
| §34.16 freshness/counters | V34-033, V34-051, V34-073 | `Optimizer Diagnostics Tests` |
| §34.17 invariants | V34 invariant map below | Listed exact owners |

## Chapter 34 invariant coverage map

| §34.17 invariant | Exact V34 procedure and controlled oracle |
|---|---|
| 1 — planning metadata, not semantic truth | V34-070–V34-073 and V34-078 compare estimates/proofs/execution |
| 2 — one stable ANALYZE SQL snapshot | V34-032–V34-033 and V34-036 independently enumerate visible rows |
| 3 — exact StatsVersion and transaction owner | V34-004, V34-024, V34-029 inspect row/payload owner identities |
| 4 — TABLE completeness manifest | V34-012–V34-016 and V34-021 reject missing/mixed members |
| 5 — global publication only after COMMITTED | V34-021–V34-023 and V34-061 observe C4 |
| 6 — per-object stable complete generation | V34-006–V34-019 and V34-065 test cross-table, alias, mixing, retention, fallback |
| 7 — NDV excludes NULL | V34-038 and V34-044–V34-045 use exact reference counts |
| 8 — no MCV/histogram double count | V34-039–V34-040 and V34-045/V34-047 inspect residual mass |
| 9 — SQL-compatible histogram/hash semantics | V34-038, V34-041, V34-047–V34-049 |
| 10 — bounded HLL/MCV/reservoir | V34-038–V34-040 and V34-053 account exact bounds |
| 11 — no unbounded large exact map | V34-037/V34-039 under constrained maintenance memory |
| 12 — logical versus physical index pressure | V34-042 and V34-051 compare same logical/different physical fixtures |
| 13 — bad/missing/stale stats not correctness | V34-017–V34-020, V34-051, V34-070–V34-072, V34-078 |
| 14 — exact chunked /shared scalar codec | V34-046–V34-050 |
| 15 — numerical zero/domain absence not emptiness | V34-070 with visible-row/error fixtures |
| 16 — accepted/rejected stats both lack proof authority | V34-017–V34-020 and V34-070–V34-071 |
| 17 — partial-row failure requires abort | V34-058–V34-059 prove first-row boundary and MA |
| 18 — post-commit cache failure preserves COMMITTED | V34-023, V34-062–V34-064 |
| 19 — MVCC before publication; opaque identity after | V34-021–V34-024, V34-028, V34-061 |
| 20 — complete selection/no creator-status lookup | V34-005, V34-018–V34-020, V34-028, V34-069 |
| 21 — status reclamation does not invalidate/retain for StatsVersion | V34-028 and V34-061 |
| 22 — outer catalog MVCC independent of payload TxnId | V34-020, V34-028, V34-061 |
| 23 — exact deterministic acceptance/whole-generation failure | V34-014–V34-020 and V34-046–V34-050 |

## Chapter 41 Chapter-34 obligation coverage map

| Owner and obligation | Procedure and controlled input/fault | Expected observable outcome |
|---|---|---|
| §§34.7–34.13, 41.6 collection/representation/bounded algorithms | V34-036–V34-045; exact, NULL, skew, bounded and fixed-sampler fixtures | Snapshot-complete full scan; valid bounded normalized descriptor without mandatory identical approximation |
| §§34.14–34.14.6, 41.6 persistence/validation | V34-012–V34-020, V34-044–V34-051; one-defect and boundary vectors | Exact framing/scalar/numeric/manifest acceptance; generation-atomic fallback |
| §§14.17.1, 34.3, 41.6 atomic publication | V34-021–V34-023, V34-030–V34-031; pre/C4/C5 and DDL barriers | No partial global generation; claims and compatibility enforce one complete per-table publication |
| §§34.3.1, 34.15 version ordering/cache | V34-024–V34-029, V34-067–V34-069; reverse callbacks/status reclamation | Unsigned pair order, no cache regression, no payload-status dependency |
| §§14.17.1, 34.15 same-table/different-table ANALYZE | V34-025–V34-027 | Concurrent complete versions resolve canonically; unrelated tables remain independent |
| §§9, 14.17.1, 21.17.1 DDL/VACUUM/DML concurrency | V34-030–V34-035 | Exact gate winner, stable SQL snapshot, safe physical lifetime, distinct maintenance authority |
| §§16, 34.15 descriptor retention/GC | V34-065–V34-069; S1/S2 and reclamation barriers | Retained descriptors stay live/immutable; reclamation waits and later completes |
| §§31.12.1, 34.3, 39 failure/cancellation | V34-056–V34-064; pre-row/post-row/append/C4/C5 faults | Correct FA/MA/COMMIT owner, no partial selection, no reversal after commit |
| §§23–26, 31.12.1 memory/spill/value lifetime | V34-052–V34-055; poison, accounting and fault boundaries | Exact retained values, continuous ownership, owner-correct failure and quiescent cleanup |
| §§33.4, 34.15 planner snapshot and per-object scope | V34-006–V34-011, V34-065–V34-066 | Different-table versions allowed; same-object aliases stable; no mixed generation |
| §§20, 34.1, 35.2, 41.6 statistics versus proof | V34-070–V34-072; zero/stale/missing versus exact proof | Estimates never authorize semantic elimination; approved proof remains separate |
| §§35–38 estimator/cost/search handoff | V34-072 and V34-078; valid/missing/invalid/incompatible inputs | Coherent inputs or canonical fallback influence cost only; final plan remains legal |
| §§34.16, 40 diagnostics | V34-073; compare descriptor, trace, plan and actual counters | Identity/provenance/staleness/estimate fields agree and remain non-actual/non-proof |
| §§31–33 frozen control/parallel/optimizer roles | V34-074–V34-078 | One coordinator, conditional helpers, stable planner input, unchanged SQL semantics |

## V34 reuse inventory and stale-rule audit

Exact reusable procedure owners are V21-26; V23-G, V23-I, and V23-K; V24-B,
V24-D, V24-J, and V24-M; V26-G, V26-I, V26-K, and V26-M; V31-N;
V32-A–V32-N as capability-applicable; and V33-G. Exact reusable headings are
`Control-Operator Tests`, `Vacuum and Reclamation Tests`, `Descriptor immutability,
cache, and catalog MVCC`, `Statistics Tests`, `Statistics Algorithm Tests`,
`Statistics Publication and Versioning Tests`, `One stable statistics snapshot per
optimization`, `Statistics Persistence and Validation Tests`, `Semantic Emptiness Tests`,
and `Optimizer Diagnostics Tests`. V34 supplies the cross-owner identity, barriers, and
outcomes where those component procedures alone do not.

The live Verification procedures must contain no requirement for one StatsVersion across
unrelated tables; no alias-specific generation for one underlying table; no mixed or
partially salvaged TABLE/COLUMN/INDEX generation; no name/version-number substitute for
identity compatibility; no synchronous ANALYZE prerequisite; no estimate-derived semantic
proof or demanded-error suppression; no helper publication or database-global ANALYZE
serialization; no callback-order cache precedence/regression; no reclamation of a retained
descriptor; no conflation of pre-row FA, post-row MA, and post-commit failure; no reversal
of COMMITTED due to cache/delivery failure; no payload creator-status retention; no
exemption of outer catalog tuples from MVCC; no mandatory page-sampling replacement,
persisted HLL sketch, parallel ANALYZE, or bitwise-identical independent approximation; and
no budget/allocation conflation. Apparent matches are classified by object, version, stage,
owner, optional capability, or explicit negative-test context before being treated as
stale. Any required event, fault, identity, or cleanup evidence that cannot be observed is
`NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`.
