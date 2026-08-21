• SA-003 is resolved in docs/ARCHITECTURE.md:13414.

  1. Finding confirmed: The grammar required FROM, while the logical architecture supported SELECT
     1; SA-003 was real.

  2. Canonical grammar: FROM table_reference [joins...] is now optional. No other clauses or
     syntax were added.

  3. No-FROM input: Exactly one conceptual zero-column row, represented by LogicalValues.
  4. Name scope: The local relation namespace is empty. Unbound columns and qualifiers produce
     normal bind errors.

  5. Constants: SELECT 1 returns one INT32 1 row; SELECT TRUE returns one TRUE row. Standalone
     SELECT NULL remains an underconstrained-NULL bind error.

  6. WHERE: TRUE preserves the row; FALSE and UNKNOWN discard it.
  7. Global aggregation: Aggregates consume the one conceptual row after WHERE filtering. Existing
     empty-input aggregate behavior remains unchanged.

  8. COUNT: SELECT COUNT(*) returns INT64 1.
  9. Subqueries: Supported uncorrelated subqueries may omit FROM and receive their own independent
     one-row input.

  10. EXISTS: EXISTS(SELECT 1) is TRUE. Existing projection suppression means EXISTS(SELECT 1/0)
     does not evaluate the irrelevant projection.

  11. Derived tables: (SELECT 1 AS x) AS q remains valid under existing alias and naming rules.
  12. SELECT star: SELECT * without FROM is a bind error; zero-column wildcard expansion is not
     supported. Qualified stars fail with unknown qualifier.

  13. Other clauses: LIMIT, OFFSET, ORDER BY, DISTINCT, GROUP BY, and HAVING retain their existing
     semantics over the one-row source.

  14. Existing authorities: M-006 scalar semantics, M-007 subquery semantics, and SA-002 aggregate
     semantics were only cross-referenced and not changed.

  15. Forbidden implementations: The architecture now explicitly forbids mandatory-FROM rejection,
     implicit DUAL tables, zero-row no-FROM input, accidental outer capture, statistics-dependent
     input existence, and syntax broadening.

  16. Sections changed: §18.11, new §18.11.1, §18.16, §19.5, §19.20, §20.5, §20.14.2, and §20.15.
  17. SQL surface: No capability was added beyond the architecture’s existing SELECT-without-FROM
     commitment.

  18. Persisted formats: None changed.
  19. Remaining SA-003 questions: None.
  20. Prerequisite dependencies: None discovered.
  21. New architecture questions: None introduced.
  22. Production code/tests: Untouched. Only docs/ARCHITECTURE.md changed; git diff --check
     passed.

  23. Phase boundary: Implementation Phase 2 was not entered.