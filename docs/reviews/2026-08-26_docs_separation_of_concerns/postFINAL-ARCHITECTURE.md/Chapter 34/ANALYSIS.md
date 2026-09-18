## Chapter 34 initial architecture review

### 1. Initial review verdict

**CLEAN WITH MINOR FOLLOW-UP**

Chapter 34 is semantically coherent and its publication, concurrency, lifetime, fallback, and planner-consumption contracts are complete. No data-integrity defect, unresolved policy, or contradiction with frozen Chapters 31–33 was found.

Two localized findings remain:

- **N34-1 — MINOR:** invariant 34.17.6 does not explicitly scope its no-mixing rule to each statistics object/table generation.
- **N34-2 — EDITORIAL:** several roadmap-style phrases remain in otherwise timeless Architecture wording.

### 2. Initial HEAD/status

- HEAD: `7e5a3b5795da8548f39b012917f14aae93a8a32c`
- Commit: `7e5a3b5 synced VERIFICATION after chapter 33 ARCHITECTURE fix`
- Worktree: clean
- Index: clean

### 3. Final HEAD/status

Unchanged:

- HEAD: `7e5a3b5795da8548f39b012917f14aae93a8a32c`
- Worktree: clean
- Index: clean

### 4. Audit-created changes

**NONE**

No tracked or untracked file was created, edited, staged, removed, or restored.

### 5. Chapter 34 boundaries and inventory

Chapter 34 occupies [ARCHITECTURE.md:24352](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24352) through line 25521. Chapter 35 begins at line 25525.

Subsections:

- §34.1 Statistics role
- §34.2 ANALYZE
- §34.3 ANALYZE visibility and publication
- §34.3.1 StatsVersion publication and transaction-status lifetime
- §34.4 TableStatistics
- §34.5 ColumnStatistics
- §34.6 IndexStatistics
- §34.7 V1 collection strategy
- §34.8 Small-table exact mode
- §34.9 HyperLogLog NDV estimation
- §34.10 Most-common values
- §34.11 Histogram collection
- §34.12 Equi-depth rationale
- §34.13 Histogram value semantics
- §34.14 Statistics persistence
- §§34.14.1–34.14.5 persisted payload formats and rebuildability
- §34.14.6 deterministic validation and normalization, with ten subparts
- §34.15 Statistics snapshots and cache
- §34.16 Freshness and modification counters
- §34.17 Statistics invariants, containing 23 numbered invariants

### 6. Canonical owner matrix

| Concern | Canonical owner | Chapter 34 relationship |
|---|---|---|
| Transaction, snapshot, CommandId | Chapter 9 | One ANALYZE transaction/command and stable effective snapshot |
| DDL/VACUUM/ANALYZE coordination | §14.17.1 | Object-use and publication claims, revalidation, GC |
| Catalog identity/lifetime | Chapter 16 | Stable IDs, immutable descriptors, MVCC retention |
| Scalar semantics | Chapter 17 | NULL, equality, hashing, ordering, persisted scalars |
| ANALYZE logical boundary | §21.17.1 | Bound target and transaction-local/global visibility |
| Value backing | Chapter 23 | Retained VARCHAR and borrowed-value lifetime |
| Memory/spill | Chapter 24 | Accounting, allocation, spill, cleanup |
| Execution lifecycle | Chapters 25–26 | Cancellation, Finalize, quiescence |
| ANALYZE coordinator | §31.12.1 | One coordinator and no parallel publication |
| Parallel work | Chapter 32 | Does not independently grant parallel ANALYZE |
| Planner snapshot | §33.4 | Stable descriptor per statistics object |
| Estimation/fallback | Chapter 35 | Estimates versus exact semantic proof |
| Cost/search | Chapters 36–38 | Statistics consumption without semantic authority |
| Errors | Chapter 39 | Pre/post statistics-row failure and commit/cache outcomes |
| Diagnostics | Chapter 40 | StatsVersion, provenance and staleness visibility |
| Verification obligations | §41.6 | Statistics, publication, validation and semantic-proof tests |

### 7. Statistics model and field inventory

The required model is complete and appropriately limited.

| Scope | Required fields |
|---|---|
| TABLE | TableId, StatsVersion, analyzed schema version, analyzed live-row count, physical heap pages, logical/stored average widths, dead-version estimate, exact column/index manifest |
| COLUMN | TableId, ColumnId, StatsVersion, LogicalType, null fraction, non-NULL NDV, min/max, MCV values/fractions, histogram, average and maximum observed width |
| INDEX | TableId in payload, IndexId, StatsVersion, physical entry count, logical live-entry count, invisible-entry estimate, leaf-page count, average entries per leaf, leading-key/heap correlation |

The text distinguishes exact-at-collection values from approximate/stale planner metadata. It does not require unsupported multi-column statistics or persisted HLL sketches.

### 8. Payload identity and compatibility

`StatsVersion` is exactly `(TxnId, CommandId)`. A TABLE manifest establishes the complete per-table generation and names all required COLUMN and INDEX members.

Compatibility uses stable TableId, SchemaVer, ColumnId and IndexId identities, plus the object/manifest checks owned by §14.17.1 and Chapter 16. Same names or numerically equal catalog/statistics version counters are not compatibility proof.

The publication unit is one table’s complete manifest generation—not all tables or the whole database.

### 9. Collection-source and snapshot assessment

ANALYZE uses one stable effective SQL snapshot under the transaction’s isolation rules. The full heap scan derives SQL-visible statistics only from tuples visible to that snapshot.

Physical page, dead-version, index-entry and correlation observations may be approximate and need not represent the identical instant as the MVCC heap snapshot. Guards, read epochs and object-use claims remain mandatory.

**Assessment: COMPLETE.**

### 10. Collection visibility assessment

Concurrent commits do not change the in-progress snapshot’s visible row set. READ COMMITTED and REPEATABLE READ behavior is inherited from Chapter 9.

Concurrent DML may make the result immediately stale, but cannot produce semantic incorrectness. Statistics never replace runtime visibility checks.

**Assessment: COMPLETE.**

### 11. Sampling and approximation assessment

The baseline uses:

- a full vectorized heap scan;
- bounded HLL state for larger NDV domains;
- bounded heavy-hitter collection;
- bounded reservoir/histogram state;
- full or bounded index-physical sampling.

Independent valid samples need not be bitwise identical. Persisted validation, canonical ordering and normalized descriptor construction are deterministic for fixed payloads.

**Assessment: COMPLETE.**

### 12. Empty and all-NULL input assessment

§34.14.6.7 supplies canonical representations:

- empty table: zero row count, `null_fraction=0`, zero NDV, no min/max/MCV/histogram;
- nonempty all-NULL column: `null_fraction=1`, zero NDV, no min/max/MCV/histogram;
- repeated equal non-NULL values: `min=max`, NDV one, with one permitted MCV/residual representation.

These remain statistics, not semantic emptiness proofs.

**Assessment: COMPLETE.**

### 13. Single-publication-coordinator assessment

§31.12.1 requires one coordinator per `PhysicalAnalyze` statement and publication. Vectorized/batched collection does not create publication authority for helpers.

Chapter 32 does not independently add parallel ANALYZE. Any future permitted helper remains collection-only unless Architecture is revised.

**Assessment: COMPLETE.**

### 14. Complete-generation publication assessment

Before the first `sys_statistics` row:

- the immutable candidate is complete;
- serialization and complete-generation validation succeed;
- all publication claims are acquired;
- current table/schema/column/index compatibility is revalidated.

A planner sees one complete generation, an older complete generation, or missing-statistics fallback. Mixed TABLE/COLUMN/INDEX members are forbidden.

**Assessment: COMPLETE.**

### 15. StatsVersion identity and advancement

StatsVersion:

- is assigned from the owning transaction and command;
- has no separate allocator;
- is compared unsigned-lexicographically;
- is a deterministic freshness preference, not semantic time;
- becomes opaque, status-independent generation identity after committed publication;
- does not retain transaction-status history.

Failed or canceled generations do not become globally selectable. Concurrent committed generations are selected by the defined comparison, not wall-clock completion.

**Assessment: COMPLETE.**

### 16. Concurrent ANALYZE assessment

Same-table and different-table ANALYZE operations may run concurrently. Same-table operations may both publish complete generations; monotonic applicable StatsVersion selection prevents an older callback from regressing the cache.

A valid incumbent does not permit partial generation publication.

**Assessment: COMPLETE.**

### 17. ANALYZE/DDL concurrency assessment

The `STATS_PUBLISH`/`MANIFEST_CHANGE` gate supplies an exact winner:

- ANALYZE claims first: DDL waits through terminal outcome.
- DDL manifest change first: ANALYZE revalidation fails and publishes no generation.

DROP/recreate under the same name cannot capture stale metadata because identity is ID-based.

**Assessment: COMPLETE.**

### 18. ANALYZE/VACUUM concurrency assessment

ANALYZE and VACUUM may coexist. Snapshot registration, page guards, read epochs and object-use claims preserve visible values and physical identity while VACUUM continues permitted maintenance.

VACUUM does not acquire statistics publication authority.

**Assessment: COMPLETE.**

### 19. ANALYZE/DML concurrency assessment

Foreground DML remains concurrent with collection. ANALYZE’s fixed SQL snapshot determines live-row/value statistics; physical observations may be skewed and approximate.

Collection workers cannot perform DML mutation publication.

**Assessment: COMPLETE.**

### 20. Publication validation assessment

Validation covers:

- chunk framing and identity;
- checksum and payload version;
- manifest completeness;
- type and object identity;
- scalar representation;
- finite numerical domains;
- MCV uniqueness/order;
- histogram order and residual mass;
- NDV and count relationships;
- canonical degenerate forms;
- generation-wide consistency.

Malformed advisory metadata, valid stale metadata and missing metadata are correctly distinguished.

**Assessment: COMPLETE.**

### 21. Prepublication failure assessment

Collection, retained-value, construction, encoding or validation failure before any statistics row publishes is pre-write `FA` for a usable explicit transaction unless an independent fatal owner applies.

The candidate and temporary resources are discarded, and the prior committed descriptor remains authoritative.

**Assessment: COMPLETE.**

### 22. Publication-boundary cancellation assessment

The boundary is precise:

- before the first statistics row: cancellation discards the candidate;
- after any row publication: statement failure is `MA`, requiring transaction abort;
- after the publication-authorizing commit append: commit is uncancellable;
- incomplete rows remain globally unusable through completeness filtering.

No physical undo protocol is invented.

**Assessment: COMPLETE.**

### 23. Postpublication failure assessment

After durable COMMIT:

- failure to install the cache cannot change COMMITTED;
- the system must install, invalidate/bypass, or retain an older/missing safe fallback;
- inability to establish a coherent fallback is database-noncontinuable;
- later delivery/session failure does not reverse committed publication.

**Assessment: COMPLETE.**

### 24. Old-generation retention assessment

Existing planners may continue using an old immutable descriptor after a newer generation publishes. Old generations become GC candidates only under ordinary catalog MVCC rules and cannot be physically deleted while active catalog snapshots still require them.

**Assessment: COMPLETE.**

### 25. Descriptor and value-backing lifetime

Published descriptors are immutable and held through versioned/shared lifetime handles. Retained VARCHAR statistics must obey Chapter 23 ownership or deep-copy rules and persisted scalar encoding.

No page-backed or reusable-chunk pointer may escape into a published descriptor. No particular reference-counting or epoch implementation is mandated.

**Assessment: COMPLETE.**

### 26. Memory, spill and accounting assessment

Ownership is separated:

- collection/candidate state: QueryExecutionContext and QueryMemoryManager;
- optional helper spill: SpillManager;
- transactional catalog rows: transaction/catalog MVCC;
- published descriptor backing: catalog/statistics cache lifetime;
- planner references: immutable descriptor handles.

Reservation does not guarantee allocation. Spill/source failures use Chapter 24/39 outcomes and clean temporary state. No separate statistics memory manager is needed.

**Assessment: COMPLETE.**

### 27. Missing-statistics fallback

Missing statistics are a valid explicit optimizer input. Chapter 35 supplies conservative fallback estimates and provenance. ANALYZE is not a prerequisite for query validity.

**Assessment: COMPLETE.**

### 28. Stale-statistics behavior

Structurally valid stale statistics remain usable performance metadata. Staleness may affect confidence, diagnostics and plan quality but not query correctness or semantic emptiness.

No synchronous refresh is required.

**Assessment: COMPLETE.**

### 29. Invalid/incompatible-statistics behavior

Malformed, unsupported, incomplete or schema-incompatible generations are rejected atomically. The loader selects an older complete compatible generation or missing-statistics fallback.

Malformed outer catalog tuple/page framing retains the stronger catalog-corruption owner.

**Assessment: COMPLETE.**

### 30. Stable planner snapshot

Planner P may retain S1 while S2 publishes. S1 is not mutated, mixed with S2 or prematurely reclaimed. A later planner may retain S2.

**Assessment: COMPLETE, subject to N34-1’s wording clarification for multi-table invocations.**

### 31. Catalog/statistics compatibility

Current-object/schema/index-manifest compatibility is revalidated independently of the old ANALYZE snapshot. StatsVersion numerical equality with a catalog generation is neither required nor sufficient.

Descriptor replacement and old-descriptor retention are coherent with Chapter 16.

**Assessment: COMPLETE.**

### 32. Statistics versus semantic proof

Chapter 34 repeatedly forbids statistics from proving:

- relation or predicate emptiness;
- uniqueness;
- tuple visibility;
- value absence;
- impossible NULLs without a catalog constraint;
- safe omission of demanded errors;
- altered bag multiplicity or DML error precedence.

Only Chapters 20 and 35 may originate exact semantic-emptiness proof.

**Assessment: COMPLETE.**

### 33. Chapter 35 estimator handoff

Chapter 34 supplies row count, NDV, NULL fraction, MCV, histogram, width and provenance-compatible metadata. Chapter 35 owns derived selectivity/cardinality formulas and missing-statistics fallback.

Payload values, derived estimates and exact semantic proofs remain distinct.

**Assessment: COMPLETE.**

### 34. Chapter 36 cost handoff

Physical heap pages, dead-version pressure, index-entry pressure, leaf pages, occupancy and heap correlation support cost estimation without becoming correctness facts.

No fixed selectivity threshold or live BufferPool residency rule is introduced.

**Assessment: COMPLETE.**

### 35. Chapters 37–38 optimizer handoff

The optimizer receives one stable statistics snapshot and uses it for join/access-path/memory estimates. Search, dominance and final plan validation remain owned by Chapters 37–38.

A numerical zero cannot eliminate required alternatives or execution.

**Assessment: COMPLETE.**

### 36. Determinism and approximation qualification

Deterministic requirements cover:

- version comparison;
- publication ordering;
- stable planner retention;
- persisted validation;
- canonical scalar order;
- exact dyadic mass checks;
- normalized descriptor materialization;
- fallback classification.

Approximate collection results need not match bitwise across independently sampled runs. Fixed retained inputs remain subject to Chapter 33/38 planning determinism.

### 37. Chapter 31 ANALYZE/control regression

The chapter preserves:

- one publication coordinator;
- ordinary QueryExecutionContext ownership;
- transactional statistics rows;
- pre/post-row error classification;
- no partial global descriptor;
- DDL/VACUUM/ANALYZE role separation.

It does not import DML W/C/R or RETURNING semantics into ANALYZE.

**Assessment: PASS.**

### 38. Chapter 32 worker/collection regression

Chapter 34 neither authorizes parallel ANALYZE nor permits helper publication. Any permitted vectorized/batched work remains coordinator-owned and subject to normal source, cancellation and resource invariants.

**Assessment: PASS.**

### 39. Chapter 33 stable-planner-input regression

Chapter 33’s planner snapshot, statistics-not-correctness rule and exact-emptiness separation remain intact.

N34-1 concerns only the precision of §34.17.6’s shorthand; §33.4 already supplies the intended per-statistics-object scope.

### 40. Document-role assessment

Chapter 34 is overwhelmingly:

- timeless in its technical contracts;
- implementation-independent;
- precise about persistence and publication;
- free of implementation-status or test-result claims.

N34-2 identifies the remaining roadmap-style exceptions.

### 41. Complexity assessment

- **CORE:** snapshot visibility, complete-generation publication, immutable descriptors, StatsVersion identity, fallback, lifetime.
- **JUSTIFIED ADVANCED:** deterministic persisted validation, exact mass normalization, concurrent publication ordering, old-generation retention.
- **POSSIBLE OVERENGINEERING:** none established. The detailed §34.14.6 rules are justified by cross-platform persisted-format determinism and generation-atomic acceptance.

### 42. Mandatory adversarial thought-experiment matrix

| Case | Owner | Required/permitted outcome | Sufficiency |
|---|---|---|---|
| A Empty table | §§34.7, 34.14.6.7 | Canonical empty-column representation; no semantic proof | Sufficient |
| B All NULL | §§34.5, 34.14.6.7 | NULL fraction one, NDV zero, no values/histogram | Sufficient |
| C Repeated equal values | §§34.10, 34.14.6.3/.7 | Occurrences contribute frequency; canonical one-value representation | Sufficient |
| D Huge VARCHAR | Chapters 17/23/24; §34.14 | Exact retained representation or controlled prepublication resource/representability failure | Sufficient |
| E Concurrent DML | §§9, 21.17.1, 34.3 | Fixed snapshot; resulting statistics may be stale | Sufficient |
| F Concurrent VACUUM | §14.17.1 | Concurrent with guards/epochs; physical observations approximate | Sufficient |
| G Helper fails | §§31.12.1, 39.1 | Candidate fails; helper cannot publish | Sufficient |
| H Helper result invalid | §§34.14.6, 31.12.1 | Whole candidate rejected before publication | Sufficient |
| I Construction failure | Chapters 24/39 | Pre-row FA and cleanup | Sufficient |
| J Validation failure | §§34.14.6, 39.1 | Pre-row FA; no partial generation | Sufficient |
| K Interrupted before visibility | §§34.3, 39.1 | No globally visible generation; partial rows abort/invisible | Sufficient |
| L Cancellation at boundary | §§31.12.1, 39.1 | FA before row, MA after row, commit uncancellable after append | Sufficient |
| M Same-table concurrent ANALYZE | §14.17.1 | Both allowed; greatest applicable StatsVersion selected | Sufficient |
| N Different-table ANALYZE | §14.17.1 | Concurrent, independently published generations | Sufficient |
| O S2 publishes while P retains S1 | §§16.10, 33.4, 34.15 | P finishes with S1; later planner may use S2 | Sufficient |
| P Reclaim S1 while retained | §§14.17.1, 16.10 | Reclamation waits for catalog snapshot/handle lifetime | Sufficient |
| Q DDL during ANALYZE | §14.17.1 | Publication gate chooses winner; stale manifest discarded | Sufficient |
| R DDL while optimizer retains old descriptor | §§16.10, 33.4 | Old valid handle remains usable for its invocation | Sufficient |
| S Mixed collection attempts | §§34.3, 34.14 | Generation rejected; no salvage | Sufficient |
| T Estimated zero, actual rows | §§34.1, 35.2 | Scan remains executable; rows returned | Sufficient |
| U Missing statistics | §35.25 | Explicit fallback/provenance | Sufficient |
| V Invalid statistics | §§34.14.5–.6 | Reject before use; older/missing fallback | Sufficient |
| W Old valid plus newer incompatible | §§14.17.1, 34.3.1 | Use applicable older generation or missing fallback | Sufficient |
| X Valid but inaccurate | §§34.1, 34.14.6 | May affect performance only | Sufficient |
| Y Memory exhausted | Chapters 24/39 | Controlled failure; pre/post-row boundary decides FA/MA | Sufficient |
| Z Spill/source read failure | Chapters 24/31/39 | Cleanup plus owner-correct error classification | Sufficient |
| AA Result delivery fails after publication | §§31, 39.1 | Committed publication is not undone; session/transport owner applies | Sufficient |
| AB Worker attempts publication | §31.12.1 | Rejected; only coordinator publishes | Sufficient |
| AC Numeric catalog/StatsVersion equality | §§14.17.1, 34.3.1 | Invalid compatibility test; use IDs/schema/manifest | Sufficient |
| AD Two planners retain different valid generations | §§33.4, 34.15 | Permitted; each invocation remains stable | Sufficient |
| AE Estimate removes demanded error | §§20, 34.1, 35.2 | Rewrite rejected | Sufficient |
| AF Old valid stats force synchronous ANALYZE | §§34.1, 34.15–.16, 35.25 | Refresh not required; old/fallback input remains legal | Sufficient |

### 43. Global contradiction-search results

- **TRUE CONTRADICTION:** 0
- **VALID DIFFERENT OWNER:** Chapter 14 coordination, Chapter 16 identity/lifetime, Chapter 21 binding, Chapter 31 physical ANALYZE, Chapters 35–38 estimation/search, Chapter 39 errors.
- **VALID DIFFERENT STAGE:** transaction-local versus globally committed descriptors; candidate validation versus loader validation; collection versus cache installation.
- **VALID DIFFERENT VERSION:** S1/S2 coexistence and old planner retention.
- **VALID OPTIONAL CAPABILITY:** small-table exact mode, bounded index sampling, background maintenance scheduling.
- **NON-NORMATIVE/EDITORIAL:** roadmap phrases identified in N34-2.

No frozen Chapter 31–33 contradiction was found.

### 44. Existing Verification reuse inventory

Exact reusable owners include:

- `V21-26 — ANALYZE operation boundary`
- `V23-G — Value-stable borrowing and ownership`
- `V23-I — Large VARCHAR exact representability and resource errors`
- `V23-K — Operator and pipeline handoffs`
- `V24-B — Complete committed-capacity accounting universe`
- `V24-D — Accounted ownership lifecycle`
- `V24-J — Spill framing and pre-access validation`
- `V24-M — Retry, cancellation, teardown, and retained ownership`
- `V26-G — Dependency readiness, Combine, and Finalize`
- `V26-I — Canonical error-owner transport`
- `V26-K — Cancellation, failure, and quiescence`
- `V26-M — Backing release, borrowing, and reset`
- `V31-N — DDL, VACUUM, ANALYZE, and cross-owner role checks`
- V32 source/cancellation/resource families where collection work reuses their contracts
- `V33-G — Catalog, statistics, and semantic-fact coherence`
- `Control-Operator Tests`
- `Vacuum and Reclamation Tests`
- `Descriptor immutability, cache, and catalog MVCC`
- `Statistics Tests`
- `Statistics Algorithm Tests`
- `Statistics Publication and Versioning Tests`
- `One stable statistics snapshot per optimization`
- `Statistics Persistence and Validation Tests`
- `Semantic Emptiness Tests`
- `Optimizer Diagnostics Tests`

The duplicated “Header/identity” row in the existing Statistics Persistence table is a presentation duplicate, not an Architecture defect or missing oracle.

### 45. Missing Chapter 34 Verification inventory

A future V34 integration family should add deterministic procedures for:

- complete per-table generation event/ownership tracing;
- exact same-table concurrent ANALYZE ordering and reverse callback completion;
- collection-helper publication prohibition;
- DDL manifest-change races at both sides of the publication gate;
- VACUUM/ANALYZE snapshot and read-epoch interaction;
- cancellation immediately before and after the first statistics row;
- construction, validation, row-write, commit and C5 cache failures;
- planner P retaining S1 while S2 publishes and S1 GC is attempted;
- mixed-generation and incompatible-schema negative cases;
- retained large-value backing;
- collection memory/spill/source failure cleanup;
- valid-old versus missing/invalid/incompatible fallback;
- exact-estimate versus exact-proof separation;
- all 23 §34.17 invariants.

No Verification edit was performed.

### 46. BLOCKING findings/count

**0**

### 47. MAJOR findings/count

**0**

### 48. MINOR findings/count

**1**

**N34-1 — Unscoped no-mixing invariant**

- Location: [ARCHITECTURE.md:25504](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25504), §34.17 invariant 6.
- Canonical owner: §33.4, especially line 24275; §§34.3.1 and 34.15.
- Current wording: “One optimizer invocation does not mix statistics descriptor versions.”
- Problem: read literally, it could require all tables in a multi-table optimization to share one StatsVersion, although publication and selection are per table/statistics object.
- Scenario: T1 and T2 have independently committed valid generations `(A,0)` and `(B,0)`. A join legitimately retains one complete descriptor for each table.
- Observable consequence: an implementation could unnecessarily discard valid per-table statistics or demand a nonexistent database-global StatsVersion. SQL correctness remains safe, but planner input behavior becomes needlessly divergent.
- Smallest repair: qualify the invariant per statistics object/table generation and explicitly permit different objects to retain different StatsVersions while forbidding mixed members within any one object’s generation.

### 49. EDITORIAL findings/count

**1**

**N34-2 — Roadmap chronology in live Architecture**

Affected passages include:

- [ARCHITECTURE.md:24385](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24385): “may later include”
- line 24395: “deferred until optimizer/executor behavior is well measured”
- line 24397: “even if a later maintenance command”
- lines 24722–24724: “intentionally preferred to a complex initial page sampler” / “future performance optimization”
- line 24767: persisted sketches “are deferred”
- related “Initial precision/target/estimate” wording at lines 24655, 24754, 24781 and 24806

Smallest repair: restate these as baseline capability, outside-baseline optional capability, or explicit unsupported baseline behavior. Preserve the full-scan baseline, bounded algorithms, distinct VACUUM/ANALYZE semantics and optional extension status.

### 50. DESIGN-SCOPE questions/count

**0**

### 51. FROZEN SEMANTIC questions/count

**0**

No new statistics, concurrency, transaction, optimizer, or fallback policy is required.

### 52. Exact next Architecture action

Apply one narrow Chapter-34-only documentation repair:

1. scope §34.17 invariant 6 per statistics object/table generation;
2. convert the identified roadmap phrases to timeless baseline/optionality wording.

No other chapter needs modification.

### 53. Recommended next authorized project task

**CHAPTER 34 — ARCHITECTURE FIX A**, limited to N34-1 and N34-2, followed by a focused read-only Chapter-34 Architecture closure audit.

Chapter-34 Verification synchronization should wait until that closure audit succeeds.

### 54. `git diff --check`

**PASS** — no output.

### 55. Final repository-state confirmation

- HEAD unchanged.
- Worktree clean.
- Index clean.
- No audit-created changes.
- No build, tests, sanitizers or benchmarks run.
- Chapter 35 not started.

```text
CHAPTER 31 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 31 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 32 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 32 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 33 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTER 33 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 34 ARCHITECTURE:
    CLEAN WITH MINOR FOLLOW-UP

CHAPTER 34 VERIFICATION:
    NOT SYNCHRONIZED

CHAPTER 35 REVIEW:
    NOT STARTED

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE
```

END CHAPTER-34 INITIAL READ-ONLY ARCHITECTURE REVIEW.