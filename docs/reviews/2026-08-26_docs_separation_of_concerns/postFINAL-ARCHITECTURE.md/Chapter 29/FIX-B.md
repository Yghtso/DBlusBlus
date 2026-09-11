## Chapter 29 Fix B — complete

1. Initial HEAD: `606c3048bc5e6731f24de3aecd2f2b69a256e495` (`applied FIX-A 29 in ARCHITECTURE`).
2. Initial state: clean working tree and index. The expected unstaged Fix-A change had already been incorporated externally into HEAD.
3. Fix-A preservation: confirmed. Its blocking hash DISTINCT lifecycle and Chapter 30/37/38 capability wording remain intact.
4. Files modified: only `docs/ARCHITECTURE.md`.
5. Sections modified:

- §17.10.3 — canonical scalar representative owner
- §20.9 — logical GROUP BY output
- §20.10 — logical DISTINCT output
- §29.4 — grouped-aggregate physical delegation
- §29.7 — hash DISTINCT physical delegation
- §29.9 — aggregation invariant
- §29.10 — ordered aggregation/DISTINCT algorithm equivalence

6. Canonical semantic owner: §17.10.3 owns `CanonicalGroupingRepresentative(value)`. Chapter 20 owns its logical GROUP BY/DISTINCT application. Chapter 29 owns physical compliance.
7. Canonical scalar rule:

```text
typed NULL                           -> same typed NULL state
FLOAT64 -0.0 or +0.0                -> +0.0
grouping-equivalent FLOAT64 NaN     -> canonical quiet NaN
every other non-NULL v1 scalar      -> existing semantic representation

```

Composite values apply this componentwise. Ordinary scalar values, casts, arithmetic, persistent FLOAT64 payloads, and persistent key encodings remain unaffected.

8. GROUP BY: every emitted grouping-key component now uses the canonical representative. Group equivalence, multiplicity, schema, slots, and ordering are unchanged.
9. DISTINCT: every duplicate class now emits one componentwise canonical row while preserving child schema and `LogicalSlotId` values.
10. Grouped aggregation: internal group-key representation remains implementation-defined, but relational output must be canonical. Hash seed, insertion order, workers, Combine order, spill, and algorithm selection cannot change representative bits.
11. Hash DISTINCT: its post-Finalize Source emits the canonical §20.10 representative.
12. Ordered DISTINCT: emits the same representative as hash DISTINCT; the first adjacent physical class member does not determine output bits.
13. Normative examples added:

- GROUP BY `{-0.0,+0.0}` → one group keyed by `+0.0`
- GROUP BY equivalent NaNs → one group keyed by canonical NaN
- DISTINCT `{-0.0,+0.0}` → one row containing `+0.0`
- DISTINCT equivalent NaNs → one row containing canonical NaN

### Regression matrix

| QuestionResult                                                 |     |
| -------------------------------------------------------------- | --- |
| Ordinary non-grouped `-0.0` remains representable              | YES |
| Persisted FLOAT64 behavior unchanged                           | YES |
| Ordinary FLOAT64-to-VARCHAR unchanged                          | YES |
| GROUP BY-key CAST can reveal first zero encountered            | NO  |
| GROUP BY NaN output can depend on first payload                | NO  |
| Hash/sort aggregation can expose different representative bits | NO  |
| Hash/ordered DISTINCT can expose different representative bits | NO  |
| Grouping equality changed                                      | NO  |
| Ordinary SQL equality changed                                  | NO  |
| MIN/MAX behavior changed                                       | NO  |

15. N29-1: **CLOSED**
16. N29-2: **CLOSED**
17. D29-1: unchanged. The exact FLOAT64 SUM/AVG contract remains live for Fix C.
18. M29-2: unchanged. The current-group memory/retained-output boundary remains for Fix C.
19. Newly discovered semantic questions: none.
20. `git diff --check`: passed.
21. Current Fix-B diff relative to HEAD:

```text
docs/ARCHITECTURE.md | 58 ++++++++++++++++++++++++++++++++++++++++++++++------
1 file changed, 52 insertions(+), 6 deletions(-)

```

22. Final state:

```text
 M docs/ARCHITECTURE.md

```

Index remains clean; HEAD remains `606c3048bc5e6731f24de3aecd2f2b69a256e495`.

23. Confirmed:

- VERIFICATION NOT MODIFIED
- IMPLEMENTATION NOT STARTED
- NO BUILD/TEST/SANITIZER/BENCHMARK
- NO STAGING OR COMMIT
- Historical review artifacts untouched