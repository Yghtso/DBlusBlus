# 1. Initial HEAD/status

```text
HEAD:   823b3df038d60c60214b2feabad0b2ee7ae6bb9f
Commit: 823b3df chapter 31 ARCHITECTURE analysis
Working tree: clean
Index: clean
```

# 2. Final HEAD/status

```text
HEAD:   823b3df038d60c60214b2feabad0b2ee7ae6bb9f
Commit: 823b3df chapter 31 ARCHITECTURE analysis
Working tree:
 M docs/ARCHITECTURE.md
Index: clean
```

# 3. Files modified

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md).

# 4. Exact Chapter-31 sections modified

- [§31.7 UPDATE execution](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22939)
- [§31.12 Physical VACUUM](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:23054)
- [§31.12.1 Physical ANALYZE](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:23070)

No other chapter was modified.

# 5. MINOR-1 closure status

```text
MINOR-1:
    CLOSED
```

All three identified implementation-status or future-roadmap phrases were replaced with timeless v1 capability and ownership language.

# 6. §31.7 old wording summary

The old rule said the v1 mutation/write phase was single-worker “unless a later architecture” defined parallel write coordination.

That mixed a valid v1 capability restriction with speculative future sequencing.

# 7. §31.7 final timeless DML-write capability wording

> In v1, the mutation/write-publication phase of `PhysicalInsert`, `PhysicalUpdate`, and `PhysicalDelete` is single-worker. This restriction does not independently require their input scans, target materialization, or expression evaluation to be single-worker.

This fixes the restriction specifically on transactional mutation/write publication and makes no future parallel-DML promise.

# 8. DML read/evaluation capability regression

Not narrowed.

The final wording expressly does not independently require these activities to be single-worker:

- input scans;
- target materialization;
- expression evaluation.

Existing vectorized, batched, or otherwise authorized physical execution remains available. No parallel mutation/write-publication capability was introduced.

# 9. §31.12 old VACUUM future-result wording

The old text said PhysicalVacuum “may later expose progress/debug result chunks.”

That was speculative future SQL-result-surface language.

# 10. §31.12 final result-surface wording

> PhysicalVacuum produces no relational result-row bag; successful execution is reported through ordinary command completion. Internal tracing, metrics, profiling, or debug instrumentation do not define SQL result rows.

# 11. VACUUM result-surface confirmation

No VACUUM relational, progress, or debug result surface was introduced.

Internal metrics, tracing, profiling, and debug instrumentation remain non-SQL observability mechanisms.

# 12. VACUUM concurrency regression result

Chapter 14’s ownership model is preserved explicitly:

- same-table mutating VACUUM passes are serialized;
- different-table VACUUM passes are not serialized by that ownership;
- VACUUM may coexist with ordinary DML;
- VACUUM may coexist with ANALYZE under the owning maintenance rules;
- one PhysicalVacuum pass is not implicitly parallelized internally by the generic query scheduler.

No global VACUUM serialization was introduced.

# 13. §31.12.1 old ANALYZE wording

The old text stated:

> The baseline implementation is single-coordinator.

This described an implementation baseline rather than the durable ownership invariant.

# 14. §31.12.1 final coordinator wording

> In v1, one coordinator owns each `PhysicalAnalyze` statement and its transactional statistics publication. This control-operator ownership does not prohibit vectorized or batched collection work permitted by Chapter 34 and introduces no parallel ANALYZE capability.

# 15. Parallel ANALYZE confirmation

No parallel ANALYZE capability was invented.

The edit fixes one publication coordinator per statement while preserving Chapter-34-authorized vectorized or batched collection work.

# 16. Control-operator/vectorization regression

DDL, VACUUM, and ANALYZE remain control/management operators.

This classification does not prohibit:

- vectorized scans;
- batched collection;
- efficient storage helpers;
- other lower-level work already permitted by canonical owners.

It prevents generic query scheduling from creating independent transactional publication coordinators.

# 17. Remaining temporal-language search table

| Live occurrence | Classification |
|---|---|
| `Read phase` | Runtime lifecycle |
| `Write phase begins only after successful spool Finalize` | Runtime lifecycle |
| `mutation/write-publication phase` | Timeless v1 capability |
| DELETE `mutation/write phase is single-worker` | Timeless v1 capability |
| `later transaction COMMIT` | Runtime transaction sequence |
| `later explicit ROLLBACK` | Runtime transaction sequence |
| `later cursor operation` | Runtime cursor sequence |
| ANALYZE descriptor available to `later statements` | Runtime transaction sequence |
| `v1 execution baseline` in invariant 8 | Timeless architecture baseline |
| attempt that `later restarts/fails` | Runtime attempt lifecycle |
| `later transport failure` | Runtime result-delivery sequence |

No hits remain for:

- `future`;
- `initial`;
- `baseline implementation`;
- `current implementation`;
- `currently`;
- `planned`;
- `eventually`;
- `first implementation`;
- `roadmap`;
- `unless a later architecture`.

No stale project chronology from MINOR-1 remains.

# 18. Document-role audit result

**PASS.**

The changed text now contains:

- timeless v1 capability restrictions;
- canonical ownership;
- runtime lifecycle;
- cross-chapter delegation.

It contains no roadmap, implementation-status narration, speculative future result surface, or optimization sequence.

# 19. VACUUM transaction-model regression

Unchanged.

VACUUM remains Chapter-14 system maintenance rather than ordinary transactional user DML. Completed maintenance publication units are not reinterpreted as rollbackable statement work.

# 20. ANALYZE transaction-model regression

Unchanged.

ANALYZE remains transactional system DML:

- statistics rows are transaction-owned;
- incomplete or uncommitted generations are not globally usable;
- post-publication failure retains the existing Chapter-39 consequence;
- global descriptor/cache publication still waits for terminal commit and applicability checks.

# 21. DDL coordinator regression

Unchanged.

The existing rule that DDL execution is single-coordinator in v1 was not edited. No parallel DDL capability was introduced.

# 22. N31-1 confirmation

```text
N31-1 APPROVED AS ALTERNATIVE A
N31-1 NOT IMPLEMENTED IN THIS FIX
```

No ordinary-candidate closure, DML staging, statement-wide unique locking, INSERT SELECT materialization, `RETURNING` precomputation, or Chapter-39 change was made.

# 23. N31-2 confirmation

```text
N31-2 APPROVED AS ALTERNATIVE A
N31-2 NOT IMPLEMENTED IN THIS FIX
```

No boundary `R`, result-ownership transfer, post-success spool-failure rule, or affected-count publication change was made.

# 24. New Architecture semantic question discovered

```text
NONE
```

# 25. `git diff --check` result

```text
PASS
```

No whitespace errors were reported.

# 26. Diff summary

```text
docs/ARCHITECTURE.md | 20 ++++++++++++++++----
1 file changed, 16 insertions(+), 4 deletions(-)
```

The diff contains exactly three Chapter-31 documentation hunks.

# 27. Final git status

```text
 M docs/ARCHITECTURE.md
```

Index remains clean.

# 28. Final confirmations

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
```

CHAPTER 31 FIX A COMPLETE —
READY FOR N31-1 ARCHITECTURE REPAIR