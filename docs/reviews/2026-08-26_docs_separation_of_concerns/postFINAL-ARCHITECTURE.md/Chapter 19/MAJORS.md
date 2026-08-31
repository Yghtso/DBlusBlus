# Chapter 19 conflict-resolution verdict

**D19-C1 AND D19-C2 INTEGRATED — BOTH CONFLICTS CLOSED**

Chapter 19 is now:

- **SEMANTICALLY CLEAN**
- **NOT YET DOCUMENT-CLEAN**
- **NOT FULLY CLOSED**

Only [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15788) was modified.

## Repository baseline

- Initial status: clean
- Initial index: clean
- Initial HEAD: `36665b12b55343e83262bc256f2907624a68240e`
- The preceding D19-S1–S18 integration was already committed.
- All previously integrated Chapter-19 semantics were preserved.
- No review artifact was read, modified, created, or staged.

## Sections modified

- [§19.9 Function registry and volatility](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:15788)
- [§19.14 LIMIT and OFFSET](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16004)
- [§19.20.1 Binder error categories](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16151)
- [§19.20.2 Deterministic ordinary error precedence](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16177)
- [§19.20.3 Invariants](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:16210)

## C19-1 — Function-call classification

D19-C1 preserves Chapter 17’s registry/type-resolution ownership.

A syntactically valid generic call produces `TypeError` when no legal descriptor remains for its:

- canonical name;
- call shape;
- arity;
- star/nonstar form;
- argument types;
- overload resolution.

Canonical results:

| Input/condition | Result |
|---|---|
| `foo()` with no descriptor | `TypeError` |
| `foo(*)` with no star descriptor | `TypeError` |
| `sum(*)` without a star-capable SUM descriptor | `TypeError` |
| Wrong arity | `TypeError` |
| Argument-type mismatch | `TypeError` |
| Unresolved overload ambiguity | `TypeError` |
| `count(*)` | Resolves only through Chapter 29’s star-capable descriptor |
| Resolved aggregate in WHERE/GROUP BY/etc. | Placement `BindError` |
| Same-block nested aggregate | `BindError` |
| Unresolved call argument column | Argument’s prerequisite `BindError`; no fabricated call error |

Parser behavior remains registry-independent; no valid generic call becomes `ParserError`.

Statuses:

- D19-C1: **CLOSED**
- C19-1: **CLOSED**
- D19-S15: **CLOSED**
- D19-S16: **CLOSED**
- F19-M5: **CLOSED**

## C19-2 — LIMIT/OFFSET folding and consumption

The canonical pipeline is now:

1. Bind the raw expression.
2. Enforce execution-start-constant eligibility.
3. Enforce INT32/INT64 typing and normalize INT32 to INT64.
4. Apply all mandatory §17.10.2 folding and constant-error timing.
5. Retain the folded/residual bound expression.
6. At execution start, acquire its final count once before relational row processing.
7. Validate the final count domain.

“Folded/residual” means the representation remaining after mandatory Chapter-17 folding. It may be a literal or another legal execution-start-constant expression. The original pre-fold source tree is not reevaluated.

| Input | Binding result | Execution-start result |
|---|---|---|
| `LIMIT 1+2` | Folds to semantic INT64 `3` | Consumes `3` |
| `LIMIT 1/0` | `DIVISION_BY_ZERO` during binding | Not reached |
| `LIMIT 9223372036854775807+1` | `NUMERIC_OVERFLOW` during binding | Not reached |
| `LIMIT NULL` | Typed/folded as `INT64 NULL` | `ExecutionError` |
| `LIMIT -1` | May fold to INT64 `-1` | `ExecutionError` |
| `LIMIT column` | `BindError` | Not reached |
| `LIMIT (SELECT ...)` | `BindError` | Not reached |
| `LIMIT 1.5` | `TypeError` | Not reached |

Final-domain rules remain:

- NULL → `ExecutionError`
- negative INT64 → `ExecutionError`
- nonnegative INT64 → accepted
- no clamping or NULL-to-zero interpretation

Statuses:

- D19-C2: **CLOSED**
- C19-2: **CLOSED**
- D19-S10: **CLOSED**
- F19-B6: **CLOSED**

## Final Chapter-19 error map

| Category | Chapter-19 conditions |
|---|---|
| `CatalogError` | Missing table/index, wrong DROP object kind, create-name collision, catalog lookup failures |
| `BindError` | Unknown/ambiguous columns and qualifiers, duplicate qualifier, ambiguous output alias, invalid ordinal, aggregate placement/nesting, grouping/HAVING legality, nonconstant LIMIT/OFFSET, duplicate DML/index targets |
| `TypeError` | Unknown type, operator/cast/coercion mismatch, non-BOOLEAN predicate, CASE/IN mismatch, unresolved generic scalar/aggregate call, name/shape/arity/star mismatch, argument mismatch, overload ambiguity, nonintegral LIMIT/OFFSET, index-ineligible type |
| `ConstraintDefinitionError` | Invalid CREATE TABLE constraints and frozen default-definition failures |
| `UnsupportedFeature` | Unsupported correlation and other explicitly frozen unsupported semantics |
| `CardinalityError` | Downstream scalar-subquery and other cardinality owners |
| Resource categories | `OutOfMemory` and `FrontEndResourceLimit` unchanged |

Unknown columns remain `BindError`; they did not move into type resolution.

## D19-S17 regression

The deterministic precedence rule is unchanged:

1. only prerequisite-valid errors are candidates;
2. earliest responsible SourceSpan start wins;
3. shorter span wins an equal-start tie;
4. identical spans use the existing class order.

Call registry/type failures use class 4, type/coercion resolution. A successfully resolved aggregate’s illegal placement uses class 2, semantic placement/shape.

Resource and cancellation behavior remains immediate. Hash, catalog, descriptor, pointer, allocation, and scheduling order cannot select the error.

## Owner preservation

- [§17.9.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13888) remains unchanged and owns unsupported-call `TYPE_ERROR`.
- [§17.10.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13929) remains unchanged and owns mandatory scalar folding/error timing.
- Chapter 18 generic call syntax remains unchanged.
- Chapter 29 remains the aggregate descriptor, signature, and ordinal owner.
- §39 categories and transaction consequences remain unchanged.
- Chapters 16–18 and Chapter 20+ received no task edits.
- Persistent formats and recovery are unchanged.

## Reread answers 1–86

- Questions 1–15: **YES**
- Questions 16–36: **YES**
- Questions 37–54: **YES**
- Questions 55–62: **YES**
- Questions 63–79: **YES**
- Question 80, “Any new frozen semantic conflict?”: **NO**
- Questions 81–86: **YES**

No new frozen semantic conflict or question arose.

## Final status

| Item | Status |
|---|---|
| D19-C1 | CLOSED |
| D19-C2 | CLOSED |
| D19-S1–S18 | ALL CLOSED |
| F19-B1–B7 | ALL CLOSED |
| F19-M1–M5 | ALL CLOSED |
| F19-N4 | CLOSED |
| F19-N1 | PENDING / UNCHANGED |
| F19-N2 | PENDING / UNCHANGED |
| F19-N3 | PENDING / UNCHANGED |
| Frozen Chapter-19 semantic questions | NONE |
| Chapter-19 verification | NOT STARTED |
| Chapter-20 direct review | NOT STARTED |

Next task: **Chapter-19 document-only cleanup F19-N1–N3**.

## Hunk classification

- A–B: call error category and name/arity/star outcomes
- C: error-table synchronization
- D: same-span class clarification
- E: §17.10.2 folding precedence
- F: folded/residual representation
- G: execution-start consumption and final validation
- H: invariant synchronization
- I: exact §§17.9.3/17.10.1 ownership
- J: exact §17.10.2 ownership
- K: local wrapping only

## Final Git state

- Status: `M docs/ARCHITECTURE.md`
- Index: clean
- HEAD: `36665b12b55343e83262bc256f2907624a68240e`
- Diff size: 52 insertions, 12 deletions
- `git diff --check`: passed
- Files changed: only `docs/ARCHITECTURE.md`
- External repository changes observed: none
- No staging, commit, build, test, benchmark, implementation, devlog, or review artifact
- Phase 2 remains **NOT STARTED / NOT AUTHORIZED**
