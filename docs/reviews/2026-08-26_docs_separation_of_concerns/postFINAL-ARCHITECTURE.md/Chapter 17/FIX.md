## Verdict

**CHAPTER-17 TARGETED TERMINOLOGY CLEANUP — COMPLETE.**

F17-E1 is resolved. Binder-only typing now uses “unresolved type marker”; `UNKNOWN` remains the SQL three-valued-logic result. No type/value semantics changed.

## Git state and scope

- Initial working tree: clean
- Initial index: clean
- Initial HEAD: `5b4563699c21e7332bc566acd409d206e5f51afc`
- Pre-existing state: preserved
- Review artifacts: not read, modified, moved, or staged
- Only modified file: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md)

## Terminology changes

Original binder-oriented wording included:

> `UNKNOWN` is a binder-only unresolved type used for context-dependent expressions such as an untyped NULL literal and future parameters.

It now states in [§17.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13342):

> An untyped NULL token may temporarily carry an unresolved type marker during binding. Successful binding resolves the marker to a concrete logical type before ordinary execution; if no legal context supplies a unique concrete type, binding produces `TYPE_ERROR`.

The semantic distinction is now explicit:

> The distinct terminology reflects distinct semantic layers: an unresolved type marker is a bind-time state, whereas SQL three-valued-logic UNKNOWN is the nullable BOOLEAN NULL result defined in §17.7.2.

Additional synchronization:

- [§17.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13306) distinguishes SQL `UNKNOWN` and the binder’s unresolved marker from persisted TypeIds.
- [§17.12 invariant 2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:14054) now names the “unresolved type marker” and excludes it from persisted schemas and resolved execution.

Resulting terminology:

| Term | Canonical meaning |
|---|---|
| unresolved type marker | Binder-only type-resolution state; not a SQL type, TypeId, persisted value, or executor type |
| SQL `UNKNOWN` | Three-valued-logic result represented by nullable BOOLEAN NULL |
| typed NULL | NULL value associated with a concrete logical type |
| untyped NULL | NULL token awaiting contextual type resolution |
| TypeId | Stable persisted identity from Chapter 16’s fixed registry |

## UNKNOWN occurrence classification

All four formerly binder-oriented bare-`UNKNOWN` occurrences were normalized:

- §17.2 closed TypeId exclusion
- §17.3 stored-schema-type statement
- §17.3 binder-state definition
- §17.12 invariant 2

Current uppercase `UNKNOWN` occurrences classify as follows:

| Classification | Locations | Result |
|---|---|---|
| SQL three-valued `UNKNOWN` | §§17.2, 17.3 rationale, 17.7.2–17.7.3, 17.9.1–17.9.2, 17.10.3 | Preserved; 24 occurrences |
| Binder unresolved type | None using bare `UNKNOWN` | Replaced with “unresolved type marker” |
| Ordinary English “unknown” | §17.13 unknown TypeIds/flags | Preserved as corruption terminology |
| Other | None | N/A |

The TRUE/FALSE/UNKNOWN tables in [§17.7.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:13681) are unchanged.

## Semantic regression assessment

| Area | Result |
|---|---|
| Typed NULL and contextual NULL resolution | Unchanged |
| Underconstrained NULL | Still produces `TYPE_ERROR` |
| SQL `UNKNOWN` representation | Still nullable BOOLEAN NULL |
| Three-valued logic | Unchanged |
| NULL comparisons | Unchanged |
| `IS NULL` / `IS NOT NULL` | Unchanged |
| WHERE/HAVING/JOIN ON | Unchanged |
| IN/NOT IN | Unchanged |
| TypeResolver | Unchanged |
| Cast/coercion registry | Unchanged |
| CASE and literal rules | Unchanged |
| TypeIds 1–7 | Unchanged |
| Chapter 5 tuple/NULL encoding | Unchanged |
| Chapter 8 physical NULL ordering | Unchanged |
| Chapter 15 error publication | Unchanged |
| Chapter 16 registry/history semantics | Unchanged |
| PersistedScalarV1 | Unchanged |
| Runtime executor types | No new type introduced |
| Persistent types/formats | No new TypeId or encoding introduced |

## Local reread questions

| # | Answer | # | Answer |
|---:|---|---:|---|
| 1 | YES | 18 | YES |
| 2 | YES | 19 | YES |
| 3 | YES | 20 | YES |
| 4 | YES | 21 | YES |
| 5 | YES | 22 | YES |
| 6 | YES | 23 | YES |
| 7 | YES | 24 | YES |
| 8 | YES | 25 | YES |
| 9 | YES | 26 | YES |
| 10 | YES | 27 | YES |
| 11 | YES | 28 | YES |
| 12 | YES | 29 | YES |
| 13 | YES | 30 | YES |
| 14 | YES | 31 | YES |
| 15 | YES | 32 | YES |
| 16 | YES | 33 | YES |
| 17 | YES | 34 | **NO — no new semantic question arose** |

## Documentation-model assessment

| Question | Result |
|---|---|
| A. Project chronology introduced? | NO |
| B. Current implementation narration introduced? | NO |
| C. DEVELOPMENT sequencing introduced? | NO |
| D. VERIFICATION procedure introduced? | NO |
| E. PROJECT_STATE facts introduced? | NO |
| F. Devlog/history introduced? | NO |
| G. Binder/runtime distinction clearer? | YES |
| H. SQL `UNKNOWN` remains canonical? | YES |
| I. Architecture remains analytical? | YES |
| J. Architecture remains timeless? | YES |
| K. Implementation freedom preserved? | YES |
| L. Persistent/runtime ownership unchanged? | YES |

Terminology is no longer overloaded. The added rationale is concise and analytical. No C++ enum, sentinel, AST representation, container, or runtime representation was mandated.

## Status

- F17-E1: **RESOLVED**
- Frozen Chapter-17 semantic questions: **NONE**
- Chapter-17 architecture: **CLEAN**
- Chapter-17 verification: **SYNCHRONIZATION PENDING**
- Chapter 17 fully closed: **NO**, pending verification synchronization
- Chapter-18 direct review: **NOT STARTED**

The later verification synchronization remains responsible for the final TypeResolver registry, contextual NULL typing, casts/coercions, scalar parsing, temporal boundaries, PersistedScalarV1, FLOAT equivalence, error taxonomy, storage handoffs, and folding/error-demand equivalence. `docs/VERIFICATION.md` was not changed.

## Diff audit

Task-created hunk classifications:

- A — §17.3 binder unresolved-type terminology
- B — §17.2 binder-marker/TypeId distinction
- C — §17.12 invariant synchronization
- D — concise bind-time versus SQL-3VL rationale
- E — local wrapping only

No Chapter 16, Chapter 18, cast, comparison, arithmetic, truth-table, or PersistedScalarV1 hunk exists.

Final state:

- Working tree: `M docs/ARCHITECTURE.md`
- Index: clean
- HEAD: `5b4563699c21e7332bc566acd409d206e5f51afc`
- `git diff --check`: passed
- External repository changes observed: none
- Review artifacts modified/staged: none
- Builds/tests/benchmarks: not run
- Implementation work: none
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
