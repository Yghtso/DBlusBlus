# Q22-1 — DECISION PACKAGE COMPLETE

The architecture-consistent resolution is: **an unrepresentable Top-N bound makes that physical alternative ineligible; it does not make the SQL query invalid.** A wider exact representation may preserve Top-N eligibility. `RequiredRowsObjective` overflow likewise cannot fail the query and should fall back to no finite row goal unless represented exactly.

No additional frozen semantic question was discovered.

## 1. Repository state

| Check | Initial | Final |
|---|---|---|
| HEAD | `2ff96c19fd1394a3bfd2cf8d2534c1c879d61594` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | N/A initially | passed, no output |
| Task-created changes | none | none |

Historical review artifacts were not read, modified, moved, or staged.

## 2. Architecture sections analyzed

Primary sections:

- [§19.14 LIMIT/OFFSET binding](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15860)
- [§20.12 LogicalLimit](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16758)
- [§20.17 demanded evaluation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17313)
- [§22.4 physical operator vocabulary](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18894)
- [§22.4.1 capability registry](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18943)
- [§27.9 PhysicalLimit](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:20351)
- [§30.7 PhysicalTopN](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21443)
- [§30.8 sort/Top-N invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21479)
- [§37.16 algorithm substitutability](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25261)
- [§38.15 order optimization](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25740)
- [§38.16 RequiredRowsObjective](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25776)
- [§38.24 final plan validation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26033)
- [§39.3 runtime errors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26462)
- [§39.4 planning/internal errors](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26549)

Verification was consulted only around existing Limit, Top-N, required-row-objective, equivalence, and final-plan validation procedures.

## 3. Frozen and conflicting rules

### D20-M4 — frozen LogicalLimit rule

For input cardinality `n`, offset `o`, and optional limit `l`:

```text
after_offset = max(0, n - o)

without LIMIT:
    output = after_offset

with LIMIT:
    output = min(after_offset, l)
```

OFFSET is applied first, LIMIT second. The rule explicitly states that it never requires computing `o + l`, and therefore creates no arithmetic-overflow path.

The normalized count domain is nonnegative INT64, with no additional implementation-sized SQL row-count maximum.

### Current §30.7 rule

PhysicalTopN currently:

```text
K = LIMIT + OFFSET
```

with checked arithmetic, and declares overflow to be a planning/execution error rather than wraparound.

### Current §38.16 rule

`RequiredRowsObjective` currently derives:

```text
required_rows = LIMIT + OFFSET
```

with checked arithmetic. The same section says this value is only a cost/search objective, never SQL semantics, a semantic proof, or an executor row cap.

### Exact contradiction

For:

```sql
LIMIT 9223372036854775807 OFFSET 1
```

both counts are valid under Chapters 19 and 20.

- `PhysicalSort → PhysicalLimit` can execute the frozen subtraction/minimum semantics without overflow.
- Current PhysicalTopN text can instead report overflow while deriving `K`.

Thus optimizer plan choice can determine success versus public error. That violates D20-M4, physical-algorithm substitutability, and §38.16’s objective-only rule. It is correctly classified as BLOCKING.

## 4. Valid input domain

Let `I = INT64_MAX`.

All individually valid `N, M ∈ [0, I]` remain valid when mathematical `N + M > I`.

No reviewed frozen owner establishes:

- relation cardinality ≤ `I`;
- query output cardinality ≤ `I`;
- sort input cardinality ≤ `I`;
- a physical row-count maximum as SQL semantics.

The maximum mathematical sum is:

```text
2 × INT64_MAX = 2^64 - 2
```

An unsigned 64-bit representation could represent that particular numeric range, but Architecture need not mandate `uint64_t` or any other concrete type.

## 5. Large-bound matrix

`Exact K` applies only to the Top-N realization of ordered `LIMIT/OFFSET`.

| Case | Exact K | Signed INT64? | Error policy | Exact-capability policy | Row objective | Baseline semantics |
|---|---:|---|---|---|---|---|
| `LIMIT 0 OFFSET 0` | `0` | yes | succeeds | Top-N eligible if otherwise legal | exact `0` | empty |
| `LIMIT 0 OFFSET I` | `I` | yes | succeeds | eligible if implementation supports K | exact `I` | empty |
| `LIMIT 1 OFFSET I` | `I+1` | no | erroneous under current §30.7 | wider exact Top-N or fallback | wider exact or no finite goal | skip I, emit at most 1 |
| `LIMIT I OFFSET 0` | `I` | yes | succeeds | eligible if supported | exact `I` | emit at most I |
| `LIMIT I OFFSET 1` | `I+1` | no | erroneous under current §30.7 | wider exact Top-N or fallback | wider exact or no finite goal | skip 1, emit at most I |
| `LIMIT I OFFSET I` | `2I` | no | erroneous under current §30.7 | wider exact Top-N or fallback | wider exact or no finite goal | skip I, emit at most I |
| `LIMIT I-1 OFFSET 1` | `I` | yes | succeeds | eligible if supported | exact `I` | skip 1, emit at most I−1 |
| `LIMIT I-1 OFFSET 2` | `I+1` | no | erroneous under current §30.7 | wider exact Top-N or fallback | wider exact or no finite goal | skip 2, emit at most I−1 |
| `LIMIT 1 OFFSET I-1` | `I` | yes | succeeds | eligible if supported | exact `I` | skip I−1, emit at most 1 |
| no LIMIT, `OFFSET I` | N/A | N/A | no Top-N sum | Top-N finite-K form N/A | no finite goal | offset-only PhysicalLimit |

No case becomes a public arithmetic error under the recommended policy.

## 6. Child-cardinality matrix

For child cardinality `c`, the frozen result count is:

```text
min(max(0, c - M), N)
```

| Child cardinality | Frozen result | Saturated retained-K safety |
|---|---:|---|
| `0` | `0` | happens to be safe |
| `1` | formula above | case-dependent |
| `< M` | `0` | happens to be safe |
| `= M` | `0` | happens to be safe |
| `M + 1` | `1` | can fail when `M + 1 > I` |
| `I` | `min(I-M,N)` | generally matches an I-bound |
| `> I` | potentially more than `I-M` | saturation can under-read |
| exact `K` or greater | `N` | requires retaining the exact K prefix |

Counterexample to saturated Top-N:

```text
N = I
M = 1
exact K = I + 1
child cardinality = I + 1
```

Correct result cardinality is `I`. A Top-N operator retaining only saturated `I` rows and then skipping one emits only `I−1`.

A stronger counterexample is:

```text
N = I
M = I
exact K = 2I
child cardinality = 2I
```

Saturating retained K to `I` and then skipping `I` emits zero rows instead of `I`.

Therefore simple saturation of the algorithm’s retention bound is not universally correct.

## 7. Interpretation analysis

### Interpretation A — no new public error

Compatible with all frozen owners.

- `N + M` is not a SQL expression.
- Exact-K representability is a physical-algorithm feasibility condition.
- Top-N remains eligible if an implementation can represent and honor exact mathematical K.
- Otherwise Top-N is ineligible.
- The optimizer selects a conforming fallback.

This preserves plan-choice independence and implementation freedom.

### Interpretation B — physical overflow is public error

Rejected.

It conflicts with:

- D20-M4’s explicit no-sum/no-overflow semantics;
- Chapter 22’s physical-algorithm substitutability;
- §37.16’s separation of physical choice from row semantics;
- §38.16’s objective-only contract;
- §39’s separation of public runtime failures from internal invalid-plan conditions.

It lets cost, search order, or capability availability alter query validity.

### Saturated Top-N bound

Rejected as a general solution.

Saturation can retain fewer than exact K rows and therefore under-read after OFFSET. It is valid only if an independent exact bound proves that the child can never contain enough rows for the difference to matter. No such global bound is frozen.

### Wider exact Top-N domain

Accepted as an optional implementation strategy.

Architecture should require exactness, not a particular integer type. An implementation may use:

- a wider integer;
- a domain-tagged bound;
- another exact representation;
- capability-based ineligibility and fallback.

## 8. RequiredRowsObjective alternatives

### Checked overflow error

Rejected. A search/cost objective cannot invalidate valid SQL.

### No finite objective / `ALL_ROWS`

Accepted and recommended as the canonical fallback when exact K is not representable in the objective domain.

This safely reverts costing to ordinary/full-input behavior and cannot be mistaken for an executor cap.

### Wider exact objective

Accepted. An implementation may carry exact mathematical K in a wider internal domain.

### Saturated objective

Conditionally legal only as explicitly approximate costing metadata, provided it:

- is never semantic proof;
- never authorizes early termination;
- never becomes an executor row cap;
- never removes the baseline plan;
- never makes a physical alternative legal or illegal based on the clamped value;
- never causes query failure.

Because saturation can be misread as an exact finite objective, `ALL_ROWS`/no finite goal is the cleaner canonical fallback.

## 9. Alternatives table

| Alternative | D20-M4 compatible? | Plan-choice independent? | Hidden cardinality bound? | Public error? | Proof risk | Recommendation |
|---|---|---|---|---|---|---|
| A. Top-N overflow is query error | no | no | no | yes | high | reject |
| B. Top-N ineligible; fallback | yes | yes | no | no | none | **canonical** |
| C. Wider exact Top-N K | yes | yes | no | no | none | permit |
| D. Saturated Top-N K | not generally | no | yes, unless independently proved | possibly no, but wrong rows | critical | reject generally |
| E. Objective overflow error | no | no | no | yes | high | reject |
| F. No finite row objective | yes | yes | no | no | none | **canonical fallback** |
| G. Wider exact objective | yes | yes | no | no | none | permit |
| H. Saturated objective | conditionally | only under strict nonsemantic use | no | no | moderate | optional, not canonical |

## 10. Top-N versus Sort+Limit

For every case where Top-N is eligible, it must match `PhysicalSort → PhysicalLimit` in:

- output bag;
- required semantic ordering;
- comparator and tie behavior;
- demanded scalar errors;
- transaction state;
- child-consumption requirements.

When exact K is not supported, Top-N is simply absent from the legal alternative set.

A baseline exists:

- PhysicalSort is a baseline physical operator.
- PhysicalLimit is the Chapter-27 conforming realization.
- §38.15 requires Sort enforcement when no exact ordering provider exists.

An already ordered access path followed by PhysicalLimit is another possible fallback. Full Sort is not mandatory when exact required order is otherwise provided.

## 11. Capability and validation model

PhysicalTopN is legal only when the selected implementation can:

1. honor the exact required comparator and ordering semantics;
2. represent and execute exact mathematical K;
3. satisfy all other capability and resource-model prerequisites.

Existing Chapter-22/37 capability machinery can express this as an applicability condition. No new implementation structure is required.

Final-plan validation should reject a PhysicalTopN node whose K lies outside its advertised exact domain. Such rejection means:

- the optimizer constructed an invalid physical plan;
- execution must not begin;
- no side effects occur;
- the condition is an internal optimizer/plan-validation failure.

It must not be surfaced as a user-facing LIMIT/OFFSET overflow.

## 12. Error and resource consequences

No public error category should represent “Top-N internal K not representable.”

This does not suppress legitimate failures such as:

- `OutOfMemory`;
- spill I/O failure;
- cancellation;
- controlled optimizer resource exhaustion;
- storage/runtime failures under their existing owners.

Those failures arise from actual resource or runtime conditions, not from treating a valid SQL count pair as invalid.

Removing the algorithmic overflow error also prevents it from incorrectly preempting unrelated scalar/runtime errors governed by demanded evaluation.

## 13. Exact proof, demand, and order

- `RequiredRowsObjective` remains cost/search metadata, not exact proof.
- Saturated or absent row goals cannot prove emptiness, cardinality, or safe demand suppression.
- D20-B1 demanded evaluation remains unchanged.
- Top-N must consume whatever child input is required to establish the correct ordered result.
- Q22-1 introduces no new early-termination rule.
- ORDER BY comparator, ASC/DESC, NULL placement, FLOAT behavior, VARCHAR ordering, and equal-key tie semantics remain unchanged.

## 14. Transaction and persistence effects

No changes to:

- TxnId or CommandId;
- statement attempts or retry;
- snapshots;
- transaction state;
- lock lifetime;
- D21-S1–S6;
- page, tuple, catalog, or WAL formats;
- recovery, RID, or persistent IDs.

Persistence impact: **none**.

## 15. Recommended canonical decision

Recommendation: **one combined D22-S1 decision with two linked clauses**, rather than two independently selectable decisions. One principle governs both: physical optimization metadata and representability cannot introduce new SQL invalidity.

### Proposed normative wording

> **D22-S1 — Exact first-K feasibility and row-goal overflow**
>
> `LIMIT` and `OFFSET` remain individually validated nonnegative INT64 counts under §§19.14 and 20.12. Their mathematical sum is not a SQL expression or a public-error boundary.
>
> A physical alternative whose algorithm requires the first mathematical `K = OFFSET + LIMIT` ordered occurrences is eligible only when the selected implementation can represent and honor that exact K together with the required ordering semantics. Failure of this feasibility condition makes that physical alternative ineligible and MUST NOT raise a user-visible LIMIT/OFFSET overflow error. Physical search MUST retain and select another conforming realization, such as an exact ordering provider or PhysicalSort followed by PhysicalLimit.
>
> An implementation MAY use any wider exact internal representation to retain Top-N eligibility. No concrete integer representation is architectural. A saturated or clamped retained-K bound is conforming only when an independent exact architecture proof establishes that it cannot under-read; absent such proof it MUST NOT be used as the Top-N retention bound.
>
> `RequiredRowsObjective` is cost/search metadata only. When the exact mathematical finite objective is representable, the optimizer MAY carry it exactly. Otherwise it MUST use no finite row goal/`ALL_ROWS`, unless it has another exact representation. Objective overflow MUST NOT fail the query, remove every legal plan, become semantic proof, establish an executor row cap, or authorize semantic early termination.
>
> A saturated objective MAY be used only as explicitly approximate costing metadata that cannot affect plan legality or correctness. No saturated objective may be treated as the exact first-K requirement of a physical operator.

This uniquely satisfies all stated recommendation criteria.

## 16. Later integration surface

After architecture-owner approval, semantic integration is required in:

- §22.4.1 — make exact-bound feasibility an operator applicability condition.
- §30.7 — remove the public checked-overflow error and define exact-K eligibility/fallback.
- §30.8 — replace “checked `N + OFFSET`” retention wording with exact mathematical-K conformance.
- §38.15 — consider Top-N only when exact-K capability is satisfied.
- §38.16 — define wider-exact or no-finite-objective fallback.
- §38.24 — validate selected Top-N bound capability as an internal plan invariant.

§20.12 requires no change.

§39 requires no semantic change unless integration finds a stale navigation statement; no such public error is needed.

## 17. Future Verification synchronization

Existing methodology must later be extended—not edited in this task—for:

- LogicalLimit large-count boundaries.
- Top-N versus Sort+Limit equivalence for `K > INT64_MAX`.
- Top-N capability ineligibility and baseline fallback.
- Optional wider exact-K implementation.
- rejection of saturated retained-K without exact proof.
- `RequiredRowsObjective` exact/wider/unbounded cases.
- confirmation that objective overflow cannot fail SQL.
- saturated objective isolation from semantic proof and plan legality.
- final-plan validator rejection of an out-of-domain Top-N node as internal invalid-plan state.
- preservation of unrelated error precedence and resource failures.

Relevant existing families already cover Limit conformance, Top-N equivalence, `FIRST_K_ROWS`, estimate-versus-proof, memory/spill planning, capability validation, and final optimizer validation.

## 18. Existing document-only findings

- M22-1: **OPEN / DOCUMENT-ONLY**
- N22-1: **OPEN / DOCUMENT-ONLY**
- N22-2: **OPEN / DOCUMENT-ONLY**

They were not analyzed as new semantic questions or modified.

## 19. Reread questions 1–45

| # | Answer |
|---:|---|
| 1 | Yes, N and M are individually valid INT64 values. |
| 2 | Yes, mathematical N+M can exceed INT64_MAX. |
| 3 | Yes, D20-M4 remains valid. |
| 4 | No, D20-M4 does not require N+M. |
| 5 | No, the derived overflow is not a logical SQL error. |
| 6 | Yes, Sort+Limit remains valid. |
| 7 | Yes, current Top-N text introduces a new error. |
| 8 | Yes, current plan choice can alter success/error. |
| 9 | Yes, that is blocking. |
| 10 | Yes, Top-N can be made ineligible. |
| 11 | Yes, Sort+Limit or an ordered provider plus Limit is available. |
| 12 | Yes, a wider exact representation can preserve eligibility. |
| 13 | No concrete integer type should be required. |
| 14 | No, saturated Top-N is not always correct. |
| 15 | Yes, its safety would depend on an exact cardinality bound. |
| 16 | No such global bound is frozen. |
| 17 | No, RequiredRowsObjective cannot fail the query. |
| 18 | No, it is not semantic proof. |
| 19 | Yes, an unrepresentable objective can become no finite row goal. |
| 20 | Yes, an exact wider objective may be used. |
| 21 | Yes, saturation is permissible only as nonsemantic approximate costing metadata. |
| 22 | No, saturated retained-K cannot be used generally. |
| 23 | Yes, Top-N capability must include exact-bound feasibility. |
| 24 | No, capability failure does not mean query failure. |
| 25 | Yes, final validation rejects an illegally constructed Top-N plan. |
| 26 | No, that must not be a user LIMIT overflow. |
| 27 | Yes, genuine resource failures remain possible. |
| 28 | Yes, they are distinct from bound representability. |
| 29 | Yes, D20-B1 remains unchanged. |
| 30 | Yes, ordered-result semantics remain unchanged. |
| 31 | Yes, tie semantics remain unchanged. |
| 32 | Yes, Chapter 21 remains unchanged. |
| 33 | Yes, transaction state remains unchanged. |
| 34 | Yes, persistence remains unchanged. |
| 35 | Yes, M22-1 remains document-only. |
| 36 | Yes, N22-1/N22-2 remain document-only. |
| 37 | No new semantic question was found. |
| 38 | Yes, the recommendation is plan-choice independent. |
| 39 | Yes, implementation freedom is preserved. |
| 40 | Yes, integration requires no §20.12 change. |
| 41 | Yes, §30.7 must change after approval. |
| 42 | Yes, §38.16 must change after approval. |
| 43 | Yes, Verification must later be synchronized. |
| 44 | Yes, Chapter 23 remains unreviewed. |
| 45 | Yes, Phase 2 remains unauthorized. |

## 20. Final campaign status

- New frozen semantic questions: **NONE**
- Chapter 22: **SEMANTIC DECISION PENDING ARCHITECTURE-OWNER APPROVAL**
- Chapter 22: **NOT CLEAN / NOT FULLY CLOSED**
- Recommended next action after approval: **CHAPTER-22 Q22-1 SEMANTIC INTEGRATION**
- Chapter 23 review: **NOT STARTED**
- Verification edit: **none**
- Implementation: **none**
- Build/test/benchmark: **none**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**

# D22-S1 semantic integration — COMPLETE

The approved exact-first-K policy is integrated. Q22-1 and B22-1 are closed with no new semantic conflict.

## Repository state

Initial:

- HEAD: `2ff96c19fd1394a3bfd2cf8d2534c1c879d61594`
- Architecture diff: clean
- Index: clean
- Pre-existing untracked artifact: `docs/reviews/.../Chapter 22/BLOCKING.md`

Final:

- HEAD: unchanged
- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- Pre-existing artifact: unchanged and untracked
- Task diff: 99 insertions, 8 deletions
- `git diff --check`: passed

Historical review artifacts were not read, modified, moved, or staged.

## Sections modified

Only these authorized sections in [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md) changed:

- [§22.4.1 Physical implementation availability](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18943)
- [§30.7 PhysicalTopN](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21458)
- [§30.8 Sorting invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:21511)
- [§38.15 Ordering enforcement and final ORDER BY](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25780)
- [§38.16 RequiredRowsObjective and startup cost](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25823)
- [§38.24 Final physical-plan validation](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26114)

[§20.12 LogicalLimit](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16758) is byte-for-byte unchanged.

## Integrated semantic model

### §22.4.1

Physical eligibility is now plan-instance-specific. An algorithm must satisfy every exact algorithmic requirement.

For Top-N:

- exact mathematical K representability is a capability condition;
- inability to honor K makes Top-N ineligible;
- it does not invalidate SQL or create a public arithmetic error;
- search must retain a conforming alternative;
- cost chooses only among legal alternatives.

Physical choice may affect plan shape, resources, and performance—not results, ordering, mandatory errors, or transaction state.

### §30.7

Top-N now derives:

```text
K = mathematical N + OFFSET
```

K is explicitly:

- a physical-algorithm requirement;
- not an SQL INT64 value;
- not a user-visible arithmetic operation.

Top-N is legal only when the selected implementation can represent and honor exact K. Wider or domain-specific exact representations are permitted without prescribing an integer type.

Generic retained-K saturation is forbidden unless an independent exact Architecture proof proves it cannot under-read. No global relation-cardinality bound was introduced.

Genuine `OutOfMemory`, `SpillIOError`, cancellation, and other existing resource/runtime failures remain possible.

### §30.8

An eligible Top-N must be equivalent to an exact ordering provider followed by PhysicalLimit in:

- bag and semantic order;
- comparator and ties;
- demanded errors;
- transaction behavior.

If Top-N is ineligible, search retains another exact ordering provider followed by PhysicalLimit. This includes PhysicalSort → PhysicalLimit when no existing provider satisfies the order.

### §38.15

Top-N enters search only when:

- semantically equivalent;
- capability-enabled;
- exact mathematical K is supported.

Legality precedes cost. Cost, search order, and optional capability state cannot determine whether valid LIMIT/OFFSET SQL succeeds.

### §38.16

The final taxonomy distinguishes:

| Concept | Role |
|---|---|
| LogicalLimit count | SQL semantics |
| Mathematical first-K | Exact derived algorithmic requirement |
| RequiredRowsObjective | Cost/search metadata |
| PhysicalTopN K | Exact operator requirement |
| Estimated cardinality | Estimate only |

When exact K is representable, `FIRST_K_ROWS(K)` may carry it exactly. A wider exact objective is also permitted.

When not representable, the canonical fallback is:

```text
ALL_ROWS
no finite row goal
equivalent unbounded objective
```

No query error occurs.

A saturated objective is permitted only as explicitly approximate costing metadata. It cannot affect legality, become semantic proof, act as an executor cap, supply exact Top-N K, or authorize semantic early termination.

### §38.24

Final validation now proves that every selected PhysicalTopN supports the node’s exact mathematical K.

An out-of-domain selected node is:

- an invalid physical plan;
- rejected before execution;
- guaranteed to produce no execution, DML, storage, catalog, or WAL side effect;
- an internal optimizer/validation failure;
- not a public LIMIT/OFFSET overflow.

## Large-bound matrix

Let `I = INT64_MAX`.

| Case | Mathematical K | Final result |
|---|---:|---|
| `LIMIT 0 OFFSET 0` | `0` | valid |
| `LIMIT 0 OFFSET I` | `I` | valid |
| `LIMIT 1 OFFSET I` | `I+1` | exact wider Top-N or fallback; no error |
| `LIMIT I OFFSET 0` | `I` | valid |
| `LIMIT I OFFSET 1` | `I+1` | exact wider Top-N or fallback |
| `LIMIT I OFFSET I` | `2I` | exact wider Top-N or fallback; no saturation |
| `LIMIT I−1 OFFSET 1` | `I` | ordinary eligibility if otherwise supported |
| `LIMIT I−1 OFFSET 2` | `I+1` | exact-capability rule |
| `LIMIT 1 OFFSET I−1` | `I` | valid |
| no LIMIT, `OFFSET I` | N/A | offset-only PhysicalLimit; no finite Top-N K |

All remain valid SQL.

The saturation counterexamples remain decisive:

- `N=I, M=1, child=I+1`: saturated retained K emits `I−1` instead of `I`.
- `N=I, M=I, child=2I`: saturated retained K emits `0` instead of `I`.

No hidden cardinality maximum was introduced.

## Regression results

- D20-B1 demanded evaluation: unchanged.
- D20-M4: unchanged; OFFSET first, LIMIT second, no semantic sum.
- Ordering, comparator, NULL/FLOAT/VARCHAR, and tie behavior: unchanged.
- D21-S1–S6 and transaction behavior: unchanged.
- Persistence formats, WAL, recovery, IDs, RID, and SchemaVer: unchanged.
- §39 public error taxonomy: unchanged.
- Concrete numeric representation: unspecified.
- No new frozen cross-owner conflict found.

## Reread answers 1–60

| # | Answer | # | Answer |
|---:|---|---:|---|
| 1 | Yes | 31 | No |
| 2 | Yes | 32 | No |
| 3 | Yes | 33 | No |
| 4 | Yes | 34 | No |
| 5 | Yes | 35 | Yes |
| 6 | Yes | 36 | Yes |
| 7 | Yes | 37 | Yes |
| 8 | Yes, unchanged | 38 | Yes |
| 9 | Yes | 39 | Yes |
| 10 | Yes | 40 | Yes |
| 11 | Yes | 41 | Yes |
| 12 | Yes | 42 | Yes |
| 13 | Yes | 43 | Yes |
| 14 | Yes | 44 | Yes |
| 15 | Yes | 45 | Yes |
| 16 | Yes | 46 | Yes |
| 17 | Yes | 47 | Yes |
| 18 | Yes | 48 | Yes |
| 19 | Yes | 49 | Yes |
| 20 | Yes | 50 | Yes |
| 21 | Yes | 51 | Yes |
| 22 | Yes | 52 | Yes |
| 23 | Yes | 53 | No |
| 24 | Yes | 54 | Yes |
| 25 | No | 55 | Yes |
| 26 | Yes | 56 | Yes |
| 27 | Yes | 57 | No |
| 28 | Yes | 58 | No |
| 29 | Yes | 59 | No |
| 30 | No | 60 | No |

Key “No” answers match the requested negative expectations: capability cannot change SQL validity; objective overflow does not fail SQL; objectives are not proof/caps/early-stop authority; no new conflict exists; Chapter 22 is not document-clean or fully closed; Verification was not synchronized; Chapter 23 was not started.

## Finding and campaign status

- D22-S1: **CLOSED**
- Q22-1: **CLOSED**
- B22-1: **CLOSED**
- Frozen Chapter-22 semantic questions: **NONE**
- M22-1: **OPEN / DOCUMENT-ONLY**
- N22-1: **OPEN / DOCUMENT-ONLY**
- N22-2: **OPEN / DOCUMENT-ONLY**

Chapter 22 Architecture is:

```text
SEMANTICALLY CLEAN
NOT YET DOCUMENT-CLEAN
NOT FULLY CLOSED
```

Next task: **TARGETED CHAPTER-22 DOCUMENT-ONLY CLEANUP**.

## Scope and hunk ownership

Task-created hunks cover classes A–S:

- capability and ineligibility rules;
- exact mathematical K;
- no public overflow;
- wider exact representation;
- saturation prohibition;
- Top-N equivalence and fallback;
- search eligibility and legality-before-cost;
- RequiredRowsObjective taxonomy and exact/unbounded behavior;
- approximate-objective restrictions;
- final-plan validation and internal-error classification;
- cross-owner references and analytical rationale;
- local Markdown wrapping.

Only `docs/ARCHITECTURE.md` was task-modified. Verification, Development, Project State, source, tests, benchmarks, and build files were untouched.

No build, test, benchmark, implementation, staging, commit, devlog, or review artifact was created. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.