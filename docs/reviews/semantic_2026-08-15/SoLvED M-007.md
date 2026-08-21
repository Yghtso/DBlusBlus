• Resolved M-007 in docs/ARCHITECTURE.md:13983.

  1. V1 supports only uncorrelated scalar subqueries, EXISTS/NOT EXISTS, single-column scalar IN/NOT IN, and derived tables. Correlated, row-
     valued, multi-column scalar/IN, ANY/SOME/ALL, LATERAL, CTE/set-operation, and data-modifying subqueries are unsupported.

  2. Correlation uses Model A: no executable OuterRef, Apply, parameterized rescan, or decorrelation requirement exists in v1.
  3. Every subquery creates a local query block. Column lookup never falls through to enclosing blocks. Attempted outer capture reports
     UnsupportedCorrelation; inner names do not leak outward.

  4. A scalar subquery requires exactly one output column. Its expression TypeId is that column’s type and is conservatively always nullable.
  5. Zero scalar rows produce one typed NULL value; they do not remove the outer row.
  6. One scalar row returns its value, including NULL. A successfully produced second row raises CardinalityViolation; no first/last/arbitrary row
     is selected.

  7. EXISTS is non-null BOOLEAN, stops at its first final row, and ignores SELECT projection values. EXISTS(SELECT 1/0 FROM t) does not evaluate
     1/0; names and types are still validated.

  8. NOT EXISTS is ordinary Boolean NOT of EXISTS and never produces UNKNOWN.
  9. IN compares one scalar left value against one scalar subquery column using M-006 equality/promotion. It returns TRUE on a match, UNKNOWN when
     no match exists but a comparison is UNKNOWN, and otherwise FALSE.

  10. NOT IN is exactly 3VL NOT of IN. A NULL-producing subquery candidate prevents naive anti-join semantics.
  11. For an empty subquery, IN returns FALSE and NOT IN returns TRUE, including for a NULL left value. The left expression is still evaluated once
     unless proven error-free and removable.

  12. Scalar output participates in ordinary M-006 operators, CASE, assignment coercion, and aggregate signatures. IN uses the ordinary equality
     registry; no subquery-specific casts exist.

  13. Derived syntax is FROM (SELECT ...) [AS] alias; alias is mandatory and AS optional. Output names must be explicit aliases or direct-column
     names and must be unique. Derived-column alias lists are unsupported. Materialization is not mandatory.

  14. Parent links may exist only for diagnostics/future design. No executable outer-reference representation is emitted.
  15. Canonical logical forms are scalar/EXISTS/IN bound subquery nodes with independent logical children, NOT wrappers for negative forms, and
     LogicalSubqueryScan for derived namespace/slot boundaries.

  16. Scalar physical fallback is a lazy side plan that consumes at most two final rows and owns/copies its one cached result.
  17. EXISTS physical fallback is lazy, executes only existence-demanded work, stops after one row, and caches one BOOLEAN.
  18. IN/NOT IN fallback lazily builds the complete final subquery result, deduplicates non-NULL values, records empty/has-NULL markers, and then
     supports vectorized probes.

  19. Each bound expression-subquery occurrence executes at most once per statement attempt, when first semantically demanded. Its result is cached
     for later outer rows. Retry discards the old attempt’s state.

  20. Every subquery and derived child uses the containing statement attempt’s effective READ COMMITTED or REPEATABLE READ snapshot and
     ReadEpochGuard.

  21. Subqueries consume no CommandId and create no nested transaction. Existing current-command visibility rules remain authoritative.
  22. A subquery in a skipped CASE arm, skipped AND/OR operand, or never-evaluated predicate does not execute. Constant folding cannot expose
     errors from such dormant side plans.

  23. IN may become a semi-join only in row-rejecting contexts while preserving equality, lazy demand, and build errors. Expression-valued IN
     requires match/null/empty markers or the canonical build.

  24. NOT EXISTS may become an equivalent anti-join/existence guard. NOT IN may use ordinary anti-semi join only with exact subquery-NOT-NULL proof
     and left-NOT-NULL handling; statistics cannot prove this.

  25. Subqueries may contain ordinary aggregates. Global aggregate over empty input still emits one row: scalar COUNT returns 0, and EXISTS over
     global COUNT is TRUE after successful child evaluation. Grouped scalar results still follow zero/one/many cardinality.

  26. LIMIT/OFFSET applies normally. LIMIT 0 proves emptiness; scalar LIMIT 1 proves at most one final row; EXISTS honors OFFSET then stops; IN
     uses only post-limit rows. required_rows is never a semantic LIMIT. LIMIT without ORDER BY creates no hidden row order.

  27. Supported uncorrelated forms are legal in projections, predicates, CASE/AND/OR, GROUP BY, eligible ORDER BY expressions, aggregate arguments,
     UPDATE/DELETE predicates, UPDATE assignments, INSERT expressions, and RETURNING. M-005 governs errors relative to published DML writes.

  28. Defaults, generated/catalog expressions, CREATE INDEX keys, constraint definitions, LIMIT/OFFSET expressions, and all DDL closed-expression
     contexts reject subqueries.

  29. Scalar subqueries are always conservatively nullable; EXISTS/NOT EXISTS are non-null BOOLEAN; IN/NOT IN are conservatively nullable BOOLEAN.
  30. Scalar first/second-row expression errors precede cardinality failure; cardinality then precedes later-row errors. EXISTS suppresses
     irrelevant projection/later-row errors. IN evaluates the left first, then completes its entire build; any build error precedes a possible
     match.

  31. IN state is query-memory accounted and spill-capable using existing hash/DISTINCT or external sort machinery. Retained VARCHAR values own
     their bytes. Spill/OOM errors follow existing query and M-005 handling.

  32. Exact emptiness rewrites are scalar→typed NULL, EXISTS→FALSE, NOT EXISTS→TRUE, IN→evaluate-left-then-FALSE, NOT IN→evaluate-left-then-TRUE,
     and derived table→empty relation. Estimated zero and statistics never authorize them.

  33. EXPLAIN must distinguish scalar, EXISTS, IN-build, lazy side-plan state, derived boundaries, and any proven semi/anti/marker rewrite.
  34. Forbidden implementations now include arbitrary scalar-row selection, empty-scalar outer-row removal, per-outer-row uncorrelated execution,
     eager skipped execution, nullable NOT IN anti-join, UNKNOWN loss, fresh snapshots/CommandIds, outer capture, decorrelation dependence,
     estimate-derived proofs, EXISTS projection evaluation, default/data-modifying subqueries, batch-dependent error precedence, and semantics-
     changing rewrites.

  35. The canonical section is docs/ARCHITECTURE.md:13983. Cross-references were updated in M-006 folding, parser syntax, binder scopes/IR, logical
     validation, DML/default scope, physical operators/context, vector expression state, pipeline scheduling, optimizer output/costing, error
     handling, and verification obligations.

  36. No persisted format, WAL/page/catalog layout, TypeId, or schema version changed.
  37. No separate major execution subsystem was introduced. Three lazy side-plan roles were added within the existing vectorized pipeline/memory/
     spill architecture; no Apply/parameterized executor is required.

  38. No correctness-relevant M-007 question remains.
  39. No prerequisite dependency or format contradiction was discovered.
  40. No new architecture question was introduced. M-013 remains untouched.
  41. Production code and tests were untouched. git diff --check passed; no tests were run because this was architecture-only.
  42. Implementation Phase 2 was not entered.